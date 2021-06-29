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
 * $Id: smcTable.cpp 90259 2021-03-19 01:22:22Z emlee $
 **********************************************************************/

#include <idl.h>
#include <ide.h>
#include <idu.h>
#include <ideErrorMgr.h>
#include <smErrorCode.h>
#include <smDef.h>
#include <smm.h>
#include <smr.h>
#include <sdr.h>
#include <sdb.h>
#include <smp.h>
#include <svp.h>
#include <sdd.h>
#include <sdp.h>
#include <sdc.h>
#include <smcDef.h>
#include <smcTable.h>
#include <smiTableBackup.h>
#include <smiVolTableBackup.h>
#include <smcReq.h>
#include <sct.h>
#include <smx.h>

#define MAX_CHECK_CNT_FOR_SESSION (10)

extern smiGlobalCallBackList gSmiGlobalCallBackList;

iduMemPool smcTable::mDropIdxPagePool;
SChar      smcTable::mBackupDir[SM_MAX_FILE_NAME];
UInt       smcTable::mTotalIndex;

scSpaceID smcTable::getSpaceID(const void *aTableHeader)
{
    return ((smcTableHeader*)aTableHeader)->mSpaceID;
}

/***********************************************************************
 * Description : smcTable ���� �ʱ�ȭ
 ***********************************************************************/
IDE_RC smcTable::initialize()
{
    mTotalIndex = 0;

    idlOS::snprintf(mBackupDir,
                    SM_MAX_FILE_NAME,
                    "%s%c",
                    smuProperty::getDBDir(0), IDL_FILE_SEPARATOR);

    IDE_TEST(smcCatalogTable::initialize() != IDE_SUCCESS);

    // Drop Index Pool �ʱ�ȭ
    IDE_TEST(mDropIdxPagePool.initialize(IDU_MEM_SM_SMC,
                                         (SChar*)"DROP_INDEX_MEMORY_POOL",
                                         1,
                                         SM_PAGE_SIZE,
                                         SMC_DROP_INDEX_LIST_POOL_SIZE,
                                         IDU_AUTOFREE_CHUNK_LIMIT,			/* ChunkLimit */
                                         ID_TRUE,							/* UseMutex */
                                         IDU_MEM_POOL_DEFAULT_ALIGN_SIZE,	/* AlignByte */
                                         ID_FALSE,							/* ForcePooling */
                                         ID_TRUE,							/* GarbageCollection */
                                         ID_TRUE,                           /* HWCacheLine */
                                         IDU_MEMPOOL_TYPE_LEGACY            /* mempool type*/) 
             != IDE_SUCCESS);			

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : smcTable ���� ����
 ***********************************************************************/
IDE_RC smcTable::destroy()
{
    IDE_TEST( smcCatalogTable::destroy() != IDE_SUCCESS );
    IDE_TEST( mDropIdxPagePool.destroy() != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*
    Table�� Lock Item�� �ʱ�ȭ�ϰ� Runtime Item���� NULL�� �����Ѵ�.

    Server Startup�� DISCARD/OFFLINE Tablespace�� ����
    Table�鿡 ���� ����ȴ�.

    [IN] aHeader - �ʱ�ȭ�� Table�� Header

    [ Lock Item �ʱ�ȭ ]
      DISCARD�� Tablespace�� ���� �ϳ��� Table��
      �� �� �̻��� Transaction�� ���ÿ� DROP�� ������ ���
      ���ü� ��� ���� Lock�� �ʿ��ϴ�.
 */
IDE_RC smcTable::initLockAndSetRuntimeNull( smcTableHeader* aHeader )
{
    // lock item, mSync �ʱ�ȭ
    IDE_TEST( initLockItem( aHeader ) != IDE_SUCCESS );

    // Runtime Item���� Pointer���� NULL�� �ʱ�ȭ
    IDE_TEST( setRuntimeNull( aHeader ) != IDE_SUCCESS );

    // drop�� index header�� list �ʱ�ȭ
    aHeader->mDropIndexLst = NULL;
    aHeader->mDropIndex    = 0;

    // for qcmTableInfo
    // PROJ-1597
    aHeader->mRuntimeInfo  = NULL;

    /* BUG-42928 No Partition Lock */
    aHeader->mReplacementLock = NULL;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*
    Table Header�� Runtime Item���� NULL�� �ʱ�ȭ�Ѵ�.

    Server Startup�� DISCARD/OFFLINE Tablespace�� ����
    Table�鿡 ���� ����ȴ�.

    [IN] aHeader - Table�� Header
*/
IDE_RC smcTable::setRuntimeNull( smcTableHeader* aHeader )
{
    UInt  sTableType;

    sTableType = SMI_GET_TABLE_TYPE( aHeader );

    aHeader->mDropIndexLst = NULL;
    aHeader->mDropIndex    = 0;
    aHeader->mRuntimeInfo  = NULL;

    /* BUG-42928 No Partition Lock */
    aHeader->mReplacementLock = NULL;

    if (sTableType == SMI_TABLE_MEMORY || sTableType == SMI_TABLE_META)
    {
        // Memory Table�� �ʱ�ȭ
        IDE_TEST( smpAllocPageList::setRuntimeNull(
                      SM_MAX_PAGELIST_COUNT,
                      aHeader->mFixedAllocList.mMRDB )
                  != IDE_SUCCESS );

        IDE_TEST( smpAllocPageList::setRuntimeNull(
                      SM_MAX_PAGELIST_COUNT,
                      aHeader->mVarAllocList.mMRDB )
                  != IDE_SUCCESS );

        IDE_TEST( smpFixedPageList::setRuntimeNull(
                      & aHeader->mFixed.mMRDB )
                  != IDE_SUCCESS );

        IDE_TEST( smpVarPageList::setRuntimeNull(
                      SM_VAR_PAGE_LIST_COUNT,
                      aHeader->mVar.mMRDB )
                  != IDE_SUCCESS );
    }
    else if ( sTableType == SMI_TABLE_DISK )
    {
        IDE_TEST( sdpPageList::setRuntimeNull( & aHeader->mFixed.mDRDB )
                                               != IDE_SUCCESS);
    }
    else if (sTableType == SMI_TABLE_VOLATILE)
    {
        // Volatile Table�� �ʱ�ȭ
        IDE_TEST( svpAllocPageList::setRuntimeNull(
                      SM_MAX_PAGELIST_COUNT,
                      aHeader->mFixedAllocList.mVRDB )
                  != IDE_SUCCESS );

        IDE_TEST( svpAllocPageList::setRuntimeNull(
                      SM_MAX_PAGELIST_COUNT,
                      aHeader->mVarAllocList.mVRDB )
                  != IDE_SUCCESS );

        IDE_TEST( svpFixedPageList::setRuntimeNull(
                      & aHeader->mFixed.mVRDB )
                  != IDE_SUCCESS );

        IDE_TEST( svpVarPageList::setRuntimeNull(
                      SM_VAR_PAGE_LIST_COUNT,
                      aHeader->mVar.mVRDB )
                  != IDE_SUCCESS );
    }
    else
    {
        IDE_ASSERT(0);
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/***********************************************************************
 * Description : table header�� lock item �� runtime item �ʱ�ȭ
 * table header�� lock item�� �Ҵ� �� �ʱ�ȭ�� ó���ϸ�,
 * page entry�� runtime item���� �ʱ�ȭ�Ѵ�.
 * - 2nd. code design
 *   + smlLockItem Ÿ���� lockitem�� �޸� �Ҵ� (layer callback)
 *   + lockitem�� �ʱ�ȭ(layer callback)
 *   + table header�� sync �ʱ�ȭ
 *   + drop�� index header�� list �ʱ�ȭ
 *   + QP�� table meta ����(qcmTableInfo)�� ���� ������ �ʱ�ȭ
 *   + ���̺� Ÿ�Կ� ���� page list entry �ʱ�ȭ
 *
 * aStatistics - [IN]
 * aHeader     - [IN] Table�� Header
 ***********************************************************************/
IDE_RC smcTable::initLockAndRuntimeItem( smcTableHeader * aHeader )
{
    // lock item, mSync �ʱ�ȭ
    IDE_TEST( initLockItem( aHeader ) != IDE_SUCCESS );
    // runtime item �ʱ�ȭ
    IDE_TEST( initRuntimeItem( aHeader ) != IDE_SUCCESS );

    // drop�� index header�� list �ʱ�ȭ
    aHeader->mDropIndexLst = NULL;
    aHeader->mDropIndex    = 0;

    // for qcmTableInfo
    aHeader->mRuntimeInfo  = NULL;

    /* BUG-42928 No Partition Lock */
    aHeader->mReplacementLock = NULL;

    // BUG-43761 init row template
    aHeader->mRowTemplate = NULL;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : table header�� lock item�� mSync�ʱ�ȭ
 * table header�� lock item�� �Ҵ� �� �ʱ�ȭ�� ó��
 * - 2nd. code design
 *   + smlLockItem Ÿ���� lockitem�� �޸� �Ҵ� (layer callback)
 *   + lockitem�� �ʱ�ȭ(layer callback)
 *   + table header�� sync �ʱ�ȭ
 *
 * aStatistics - [IN]
 * aHeader     - [IN] Table�� Header
 ***********************************************************************/
IDE_RC smcTable::initLockItem( smcTableHeader * aHeader )
{
    UInt  sTableType;

    sTableType = SMI_GET_TABLE_TYPE( aHeader );

    IDE_ASSERT( (sTableType == SMI_TABLE_MEMORY)   ||
                (sTableType == SMI_TABLE_META)     ||
                (sTableType == SMI_TABLE_VOLATILE) ||
                (sTableType == SMI_TABLE_DISK)     ||
                (sTableType == SMI_TABLE_REMOTE) );


    // table�� lockitem �ʱ�ȭ
    IDE_TEST( smLayerCallback::allocLockItem( &aHeader->mLock ) != IDE_SUCCESS );
    IDE_TEST( smLayerCallback::initLockItem( aHeader->mSpaceID,         // TBS ID
                                             (ULong)aHeader->mSelfOID,  // TABLE OID
                                             SMI_LOCK_ITEM_TABLE,
                                             aHeader->mLock )
              != IDE_SUCCESS );

    aHeader->mIndexLatch = NULL;
    IDE_TEST( allocAndInitIndexLatch( aHeader ) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/***********************************************************************
 * Description : table header�� runtime item �ʱ�ȭ
 * table header�� page entry�� runtime item���� �ʱ�ȭ�Ѵ�.
 * - 2nd. code design
 *   + drop�� index header�� list �ʱ�ȭ
 *   + QP�� table meta ����(qcmTableInfo)�� ���� ������ �ʱ�ȭ
 *   + ���̺� Ÿ�Կ� ���� page list entry �ʱ�ȭ
 *     if (memory table)
 *        - smpFixedPageListEntry�� runtime item �ʱ�ȭ
 *        - smpVarPageListEntry�� runtime item �ʱ�ȭ
 *     else if (disk table )
 *        - sdpPageList�� runtime item �ʱ�ȭ
 *     endif
 *
 * aStatistics - [IN]
 * aHeader     - [IN] Table�� Header
 ***********************************************************************/
IDE_RC smcTable::initRuntimeItem( smcTableHeader * aHeader )
{
    UInt  sTableType;

    sTableType = SMI_GET_TABLE_TYPE( aHeader );

    IDE_ASSERT((sTableType == SMI_TABLE_MEMORY)   ||
               (sTableType == SMI_TABLE_META)     ||
               (sTableType == SMI_TABLE_DISK)     ||
               (sTableType == SMI_TABLE_VOLATILE) ||
               (sTableType == SMI_TABLE_REMOTE) );

    // runtime item �ʱ�ȭ
    switch (sTableType)
    {
        case SMI_TABLE_MEMORY :
        case SMI_TABLE_META :
        case SMI_TABLE_REMOTE:
            // FixedAllocPageList�� Mutex �ʱ�ȭ
            IDE_TEST( smpAllocPageList::initEntryAtRuntime(
                          aHeader->mFixedAllocList.mMRDB,
                          aHeader->mSelfOID,
                          SMP_PAGETYPE_FIX )
                      != IDE_SUCCESS );

            /* fixed smpPageListEntry runtime ���� �ʱ�ȭ �� mutex �ʱ�ȭ */
            IDE_TEST( smpFixedPageList::initEntryAtRuntime(aHeader->mSelfOID,
                                                           &(aHeader->mFixed.mMRDB),
                                                           aHeader->mFixedAllocList.mMRDB)
                  != IDE_SUCCESS);

            // VarAllocPageList�� Mutex �ʱ�ȭ
            IDE_TEST( smpAllocPageList::initEntryAtRuntime(
                          aHeader->mVarAllocList.mMRDB,
                          aHeader->mSelfOID,
                          SMP_PAGETYPE_VAR )
                      != IDE_SUCCESS );

            /* variable smpPageListEntry runtime ���� �ʱ�ȭ �� mutex �ʱ�ȭ */
            IDE_TEST( smpVarPageList::initEntryAtRuntime(aHeader->mSelfOID,
                                                         aHeader->mVar.mMRDB,
                                                         aHeader->mVarAllocList.mMRDB)
                      != IDE_SUCCESS);

            break;
        case SMI_TABLE_VOLATILE :
            // FixedAllocPageList�� Mutex �ʱ�ȭ
            IDE_TEST( svpAllocPageList::initEntryAtRuntime(
                          aHeader->mFixedAllocList.mVRDB,
                          aHeader->mSelfOID,
                          SMP_PAGETYPE_FIX )
                      != IDE_SUCCESS );

            /* fixed svpPageListEntry runtime ���� �ʱ�ȭ �� mutex �ʱ�ȭ */
            IDE_TEST( svpFixedPageList::initEntryAtRuntime(aHeader->mSelfOID,
                                                           &(aHeader->mFixed.mVRDB),
                                                           aHeader->mFixedAllocList.mVRDB)
                  != IDE_SUCCESS);

            // VarAllocPageList�� Mutex �ʱ�ȭ
            IDE_TEST( svpAllocPageList::initEntryAtRuntime(
                          aHeader->mVarAllocList.mVRDB,
                          aHeader->mSelfOID,
                          SMP_PAGETYPE_VAR )
                      != IDE_SUCCESS );

            /* variable smpPageListEntry runtime ���� �ʱ�ȭ �� mutex �ʱ�ȭ */
            IDE_TEST( svpVarPageList::initEntryAtRuntime(aHeader->mSelfOID,
                                                         aHeader->mVar.mVRDB,
                                                         aHeader->mVarAllocList.mVRDB)
                      != IDE_SUCCESS);

            break;

        case SMI_TABLE_DISK :
            /* sdpPageListEntry runtime ���� �ʱ�ȭ �� mutex �ʱ�ȭ */
            IDE_TEST( sdpPageList::initEntryAtRuntime( aHeader->mSpaceID,
                                                       aHeader->mSelfOID,
                                                       SM_NULL_INDEX_ID,
                                                       &(aHeader->mFixed.mDRDB),
                                                       (sTableType == SMI_TABLE_DISK ? ID_TRUE : ID_FALSE) )
                      != IDE_SUCCESS );

            break;
        case SMI_TABLE_TEMP_LEGACY :
        default :
            /* Table Type�� �߸��� ���� ���Դ�. */
            IDE_ASSERT(0);
            break;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/***********************************************************************
 * Description : table header�� lock item �� runtime item �ʱ�ȭ
 * table header�� lock item�� ���� �� page entry�� runtime item�� �����Ѵ�.
 * - 2nd. code design
 *   + lockitem�� ����(layer callback)
 *   + smlLockItem Ÿ���� lockitem�� �޸� ���� (layer callback)
 *   + ���̺� Ÿ�Կ� ���� page list entry ����
 *
 * aHeader - [IN] ���̺� ���
 ***********************************************************************/
IDE_RC smcTable::finLockAndRuntimeItem( smcTableHeader * aHeader )
{
    /* BUG-18133 : TC/Server/qp2/StandardBMT/DiskTPC-H/1.sql���� ���� ������ ������
     *
     * Temp Table�ϰ�� ���⼭ IDE_DASSERT�� �װԲ� �Ǿ� �־����� Temp Table�� ��쿡��
     * �� �Լ��� ȣ��ǵ��� �Ͽ��� �Ѵ�. �ֳ��ϸ� Runtime������ TableHeader�� �Ҵ��
     * Index Latch�� Free�ؾ� �Ǳ� �����̴�. */
    IDE_TEST( finLockItem( aHeader ) != IDE_SUCCESS );

    IDE_TEST( finRuntimeItem( aHeader ) != IDE_SUCCESS );
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : table header�� lock item ����
 * table header�� lock item�� ����.
 * - 2nd. code design
 *   + lockitem�� ����(layer callback)
 *   + smlLockItem Ÿ���� lockitem�� �޸� ���� (layer callback)
 *
 * aHeader - [IN] ���̺� ���
 ***********************************************************************/
IDE_RC smcTable::finLockItem( smcTableHeader * aHeader )
{
    // OFFLINE/DISCARD Tablespace�� ���� mLock�� �ʱ�ȭ�Ǿ� �ִ�.
    IDE_ASSERT( aHeader->mLock != NULL );

    IDE_TEST( smLayerCallback::destroyLockItem( aHeader->mLock )
              != IDE_SUCCESS );
    IDE_TEST( smLayerCallback::freeLockItem( aHeader->mLock )
              != IDE_SUCCESS );

    aHeader->mLock = NULL ;

    /* BUG-42928 No Partition Lock */
    aHeader->mReplacementLock = NULL;

    // Drop�� Table�� ��� mIndexLatch�� �̹� �����Ǿ
    // NULL�� �� �ִ�.
    if ( aHeader->mIndexLatch != NULL )
    {
        IDE_TEST( finiAndFreeIndexLatch( aHeader ) != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/***********************************************************************
 * Description : table header�� runtime item ����
 * table header�� page entry�� runtime item�� �����Ѵ�.
 * - 2nd. code design
 *   + ���̺� Ÿ�Կ� ���� page list entry ����
 *     if (memory table)
 *        - smpFixedPageListEntry�� runtime item ����
 *        - smpVarPageListEntry�� runtime item ����
 *     else if (disk table)
 *        - sdpPageList�� runtime item ����
 *     endif
 *
 * aHeader - [IN] Table�� Header
 ***********************************************************************/
IDE_RC smcTable::finRuntimeItem( smcTableHeader * aHeader )
{
    switch( SMI_GET_TABLE_TYPE( aHeader ) )
    {
        case SMI_TABLE_MEMORY :
        case SMI_TABLE_META :
        case SMI_TABLE_REMOTE:            
            // To Fix BUG-16752 Memory Table Drop�� Runtime Entry�� �������� ����

            // Drop�� Table�� ���ؼ��� Runtime Item�� ��� �����Ǿ� �ִ�.
            // Runtime Item�� �����ϴ�( Drop���� ���� ) Table�� ���ؼ���
            // ó��
            if ( aHeader->mFixed.mMRDB.mRuntimeEntry != NULL )
            {
                /* fixed smpPageListEntry runtime ������ mutex ���� */
                IDE_TEST(smpFixedPageList::finEntryAtRuntime(
                             &(aHeader->mFixed.mMRDB))
                         != IDE_SUCCESS);
            }

            if ( aHeader->mVar.mMRDB[0].mRuntimeEntry != NULL )
            {
                /* variable smpPageListEntry runtime ������ mutex ���� */
                IDE_TEST(smpVarPageList::finEntryAtRuntime(aHeader->mVar.mMRDB)
                         != IDE_SUCCESS);
            }
            break;

        case SMI_TABLE_VOLATILE :
            // To Fix BUG-16752 Memory Table Drop�� Runtime Entry�� �������� ����
            if ( aHeader->mFixed.mVRDB.mRuntimeEntry != NULL )
            {
                /* fixed svpPageListEntry runtime ������ mutex ���� */
                IDE_TEST(svpFixedPageList::finEntryAtRuntime(
                             &(aHeader->mFixed.mVRDB))
                         != IDE_SUCCESS);

            }

            if ( aHeader->mVar.mVRDB[0].mRuntimeEntry != NULL )
            {
                /* variable svpPageListEntry runtime ������ mutex ���� */
                IDE_TEST(svpVarPageList::finEntryAtRuntime(aHeader->mVar.mVRDB)
                         != IDE_SUCCESS);
            }
            break;

        case SMI_TABLE_DISK :
            /* sdpPageListEntry runtime ���� �ʱ�ȭ �� mutex �ʱ�ȭ */
            IDE_TEST( sdpPageList::finEntryAtRuntime(&(aHeader->mFixed.mDRDB))
                      != IDE_SUCCESS);

            break;
        case SMI_TABLE_TEMP_LEGACY :
        default :
            IDE_ASSERT(0);
            break;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : TableHeader�� ������ Type�� Stat������ ������Ų��.
 *
 * aHeader       - [IN] Table�� Header
 * aTblStatType  - [IN] Table Stat Type
 ***********************************************************************/
void smcTable::incStatAtABort( smcTableHeader * aHeader,
                               smcTblStatType   aTblStatType )
{
    smpRuntimeEntry * sTableRuntimeEntry;

    IDE_ASSERT( aHeader != NULL );

    if( SMI_TABLE_TYPE_IS_DISK( aHeader ) == ID_FALSE )
    {
        sTableRuntimeEntry =
            aHeader->mFixed.mMRDB.mRuntimeEntry ;

        IDE_ASSERT( sTableRuntimeEntry != NULL );

        IDE_ASSERT(sTableRuntimeEntry->mMutex.lock(NULL /* idvSQL* */)
                   == IDE_SUCCESS );

        // BUG-17371  [MMDB] Aging�� �и���� System�� ������
        //           �� Aging�� �и��� ������ �� ��ȭ��.
        switch( aTblStatType )
        {
            case SMC_INC_UNIQUE_VIOLATION_CNT:
                sTableRuntimeEntry->mUniqueViolationCount++;
                break;

            case SMC_INC_RETRY_CNT_OF_LOCKROW:
                sTableRuntimeEntry->mLockRowRetryCount++;
                break;

            case SMC_INC_RETRY_CNT_OF_UPDATE:
                sTableRuntimeEntry->mUpdateRetryCount++;
                break;

            case SMC_INC_RETRY_CNT_OF_DELETE:
                sTableRuntimeEntry->mDeleteRetryCount++;
                break;

            default:
                break;
        }

        IDE_ASSERT(sTableRuntimeEntry->mMutex.unlock()
                   == IDE_SUCCESS );
    }
    return;
}

/***********************************************************************
 * Description : memory table ������ page list entry�� �ʱ�ȭ
 * - 2nd. code design
 *   + fixed page list entry �ʱ�ȭ
 *   + variable page list entry �ʱ�ȭ
 *   + table header�� variable page list entry ����(11��) ����
 ***********************************************************************/
void smcTable::initMRDBTablePageList( vULong            aMax,
                                      smcTableHeader*   aHeader )
{
    IDE_DASSERT( aHeader != NULL );

    // memory table�� fixed pagelist entry �ʱ�ȭ
    smpFixedPageList::initializePageListEntry(
                      &(aHeader->mFixed.mMRDB),
                      aHeader->mSelfOID,
                      idlOS::align((UInt)(aMax), ID_SIZEOF(SDouble)));

    // memory table�� variable pagelist entry �ʱ�ȭ
    smpVarPageList::initializePageListEntry(aHeader->mVar.mMRDB,
                                            aHeader->mSelfOID);

    return;
}

/***********************************************************************
 * Description : volatile table ������ page list entry�� �ʱ�ȭ
 * - 2nd. code design
 *   + fixed page list entry �ʱ�ȭ
 *   + variable page list entry �ʱ�ȭ
 *   + table header�� variable page list entry ����(11��) ����
 ***********************************************************************/
void smcTable::initVRDBTablePageList( vULong            aMax,
                                      smcTableHeader*   aHeader )
{
    IDE_DASSERT( aHeader != NULL );

    // volatile table�� fixed pagelist entry �ʱ�ȭ
    svpFixedPageList::initializePageListEntry(
                      &(aHeader->mFixed.mVRDB),
                      aHeader->mSelfOID,
                      idlOS::align((UInt)(aMax), ID_SIZEOF(SDouble)));

    // volatile table�� variable pagelist entry �ʱ�ȭ
    svpVarPageList::initializePageListEntry(aHeader->mVar.mVRDB,
                                            aHeader->mSelfOID);

    return;
}

/***********************************************************************
 * Description : disk table ������ page list entry�� �ʱ�ȭ
 * - 2nd. code design
 *   + sdpPageListEntry �ʱ�ȭ
 *   + table�� data page�� ���� segment ���� ��
 *     segment header ������ �ʱ�ȭ
 ***********************************************************************/
void smcTable::initDRDBTablePageList( vULong             aMax,
                                      smcTableHeader*    aHeader,
                                      smiSegAttr         aSegmentAttr,
                                      smiSegStorageAttr  aSegmentStoAttr )
{
    IDE_DASSERT( aHeader != NULL );

    // disk table�� pagelist entry �ʱ�ȭ
    sdpPageList::initializePageListEntry(
                 &(aHeader->mFixed.mDRDB),
                 idlOS::align((UInt)(aMax), ID_SIZEOF(SDouble)),
                 aSegmentAttr,
                 aSegmentStoAttr );

    return;
}

/***********************************************************************
 * Description : table header �ʱ�ȭ(memory-disk ����)
 * ���̺� ������ catalog table���� �Ҵ���� slot�� table header�� �ʱ�ȭ�Ѵ�.
 *
 * - 2nd. code design
 *   + ���ο� table header�� index ���� �ʱ�ȭ
 *   + ���̺� Ÿ�Կ� ���� table header�� page list entry �ʱ�ȭ
 *     if (memory table)
 *       - smpFixedPageListEntry�� runtime item ����
 *       - smpVarPageListEntry�� runtime item ����
 *     else if (disk table)
 *       - sdpPageList�� runtime item ����(logging)
 *     endif
 *   + table header�� �ʱ�ȭ
 *   + table header �ʱ�ȭ�� ���� �α�ó��
 *     : SMR_SMC_TABLEHEADER_INIT (REDO����)
 ***********************************************************************/
IDE_RC smcTable::initTableHeader( void*                 aTrans,
                                  scSpaceID             aSpaceID,
                                  vULong                aMax,
                                  UInt                  aColumnSize,
                                  UInt                  aColumnCount,
                                  smOID                 aFixOID,
                                  UInt                  aFlag,
                                  smcSequenceInfo*      aSequence,
                                  smcTableType          aType,
                                  smiObjectType         aObjectType,
                                  ULong                 aMaxRow,
                                  smiSegAttr            aSegmentAttr,
                                  smiSegStorageAttr     aSegmentStoAttr,
                                  UInt                  aParallelDegree,
                                  smcTableHeader*       aHeader )
{
    UInt sTableType;
    UInt i;

    idlOS::memset((SChar*)aHeader, 0, sizeof(smcTableHeader));

    aHeader->mLock                = NULL;
    aHeader->mSpaceID             = aSpaceID;
    aHeader->mType                = aType;
    aHeader->mObjType             = aObjectType;
    aHeader->mSelfOID             = aFixOID;

    aHeader->mColumnSize          = aColumnSize;
    aHeader->mColumnCount         = aColumnCount;
    aHeader->mLobColumnCount      = 0;

    aHeader->mColumns.length      = 0;
    aHeader->mColumns.fstPieceOID = SM_NULL_OID;
    aHeader->mColumns.flag        = SM_VCDESC_MODE_OUT;

    aHeader->mInfo.length         = 0;
    aHeader->mInfo.fstPieceOID    = SM_NULL_OID;
    aHeader->mInfo.flag           = SM_VCDESC_MODE_OUT;

    aHeader->mDropIndexLst        = NULL;
    aHeader->mDropIndex           = 0;
    aHeader->mFlag                = aFlag;
    // PROJ-1665
    aHeader->mParallelDegree      = aParallelDegree;
    aHeader->mIsConsistent        = ID_TRUE;
    aHeader->mMaxRow              = aMaxRow;
    /* BUG-48588 */ 
    SM_SET_SCN_INFINITE( &(aHeader->mTableCreateSCN) );

    aHeader->mRowTemplate         = NULL;

    /* BUG-42928 No Partition Lock */
    aHeader->mReplacementLock     = NULL;


    if( aMax < ID_SIZEOF(smOID) )
    {
        aMax = ID_SIZEOF(smOID);
    }

    sTableType = aFlag & SMI_TABLE_TYPE_MASK;
    if ((sTableType == SMI_TABLE_MEMORY) ||
        (sTableType == SMI_TABLE_META)   ||
        (sTableType == SMI_TABLE_REMOTE)) 
    {
        /* ------------------------------------------------
         * memory table�� variable page list entry�� fixed
         * page list entry�� ��� �ʱ�ȭ
         * ----------------------------------------------*/
        initMRDBTablePageList(aMax, aHeader);
    }
    else if ( sTableType == SMI_TABLE_DISK )
    {
        /* ------------------------------------------------
         * disk table�� page list entry�� �ʱ�ȭ�ϸ�,
         * table ����Ÿ ������ ���� segment ���� (mtx logging ���)
         * ----------------------------------------------*/
        initDRDBTablePageList(aMax,
                              aHeader,
                              aSegmentAttr,
                              aSegmentStoAttr );
    }
    /* PROJ-1594 Volatile TBS */
    else if (sTableType == SMI_TABLE_VOLATILE)
    {
        initVRDBTablePageList(aMax, aHeader);
    }
    else
    {
        IDE_ASSERT(0);
    }

    if( aSequence != NULL )
    {
        idlOS::memcpy( &(aHeader->mSequence),
                       aSequence,
                       ID_SIZEOF(smcSequenceInfo) );
    }
    else
    {
        idlOS::memset( &(aHeader->mSequence),
                       0xFF,
                       ID_SIZEOF(smcSequenceInfo) );
    }

    for(i = 0; i < SMC_MAX_INDEX_OID_CNT; i ++)
    {
        aHeader->mIndexes[i].length = 0;
        aHeader->mIndexes[i].fstPieceOID = SM_NULL_OID;
        aHeader->mIndexes[i].flag = SM_VCDESC_MODE_OUT;
    }//for i;

    /* TASK-4990 changing the method of collecting index statistics */
    idlOS::memset( &aHeader->mStat, 0, ID_SIZEOF( smiTableStat ) );

    /* ------------------------------------------------
     * Update Type - SMR_SMC_TABLEHEADER_INIT
     * redo func. :  redo_SMC_TABLEHEADER_INIT()
     * ----------------------------------------------*/
    IDE_TEST(smrUpdate::initTableHeader( NULL, /* idvSQL* */
                                         aTrans,
                                         SM_MAKE_PID(aFixOID),
                                         SM_MAKE_OFFSET(aFixOID)+SMP_SLOT_HEADER_SIZE,
                                         aHeader,
                                         ID_SIZEOF(smcTableHeader) )
             != IDE_SUCCESS);


    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description :  memory/disk Ÿ���� normal table ���� (����)
 * =====================================================================
 * - 2nd. code design
 *   + catalog table�� ���Ͽ� IX_LOCK ��û
 *   + ���ο� table�� header�� �����ϱ� ���� catalog table����
 *     fixed slot�� �Ҵ�
 *   + record�� fixed ������ ���̸� ���
 *   + table Ÿ�Կ� ���� table header �ʱ�ȭ
 *   + table Ÿ�Կ� ���� �ش� pagelist entry�� mutex ��
 *     runtime ������ �ʱ�ȭ
 *   + table ���� ���꿡 ���� NTA �α�
 *     :SMR_OP_CREATE_TABLE
 *   + DDL�� �����߻��� rollback�������� �Ҵ�� fixed slot��
 *     Ʈ����� commit�� commit scn�� ������ �� �ֵ���
 *     SM_OID_DROP_TABLE_BY_ABORTǥ��
 *   + table�� column�� ������ Variable������ ����
 *     : alloc variable slot
 *     : column ���� �α� - SMR_LT_UPDATE�� SMR_PHYSICAL
 *     : Ʈ����� rollback�ÿ� �Ҵ�� new variable slot��
 *       ager�� remove �� �� �ֵ��� SM_OID_NEW_VARIABLE_SLOT�� ǥ��
 *     : table header�� �÷� ���� ���� �� �α�
 *       - SMR_SMC_TABLEHEADER_UPDATE_COLUMNS
 *   + ���ο� table�� info ������ ���Ͽ� ���� ����
 *     : alloc variable slot
 *     : �Ҵ���� variable slot�� val�� ���� �� �α�
 *        - SMR_LT_UPDATE�� SMR_SMC_PERS_UPDATE_VAR_ROW
 *     : Ʈ����� rollback�ÿ� �Ҵ�� new variable slot��
 *       ager�� remove �� �� �ֵ��� SM_OID_NEW_VARIABLE_SLOT�� ǥ��
 *   + table Ÿ�Կ� ���� NullRow�� data page�� insert �� �α�
 *     : s?cRecord�� ���Ͽ� nullrow�� ���� insert ó��
 *     : insert�� nullrow�� ���Ͽ� NULL_ROW_SCN ���� �� �α�
 *       - SMR_LT_UPDATE�� SMR_SMC_PERS_UPDATE_FIXED_ROW
 *     : table header�� nullrow�� ���� ���� �� �α�
 *       - SMR_LT_UPDATE�� SMR_SMC_TABLEHEADER_SET_NULLROW
 *   + ���ο� table�� ���Ͽ� X_LOCK ��û
 * =====================================================================
 *
 * - RECOVERY
 *
 *  # table ������ (* disk-table����)
 *
 *  (0) (1)    (2)          (3)         (4)      (5)
 *   |---|------|------------|-----------|--------|-----------> time
 *       |      |            |           |        |
 *     alloc    *update     *alloc       init     create
 *     f-slot   seg rID,    segment      tbl-hdr  tbl
 *     from     meta pageID [NTA->(1)]   (R)      [NTA->(0)]
 *     cat-tbl  in tbl-hdr
 *    [NTA->(0)]     (R/U)
 *
 *      (6)       (7)     (8)         (9)      (10)        (11)
 *   ----|---------|-------|-----------|---------|----------|---> time
 *     alloc      init     update      alloc     update     update
 *     v-slot     column   column      v-slot    var row    info
 *     from       info     info        from      (R/U)      on tbl-hdr
 *     cat-tbl    (R/U)    on tbl-hdr  cat-tbl              (R/U)
 *     [NTA->(5)]          (R/U)       [NTA->(8)]
 *
 *      (12)      (13)    (14)
 *   ----|---------|-------|----------------> time
 *      insert    update   set
 *      ..        insert   nullrow
 *                row hdr  on tbl-hdr
 *                (R/U)    (R)
 *
 *  # Disk-Table Recovery Issue
 *
 *  - (2)���� �α��Ͽ��ٸ�
 *    + (2) -> undo
 *      : tbl-hdr�� segment rID, table meta PID�� ���� image�� �����Ѵ�.
 *    + (1) -> logical undo
 *      : tbl-hdr slot�� freeslot�Ѵ�.
 *
 *  - (3)���� mtx commit�Ͽ��ٸ�
 *    + (3)-> logical undo
 *      : segment�� free�Ѵ�. ((3)':SDR_OP_NULL) [NTA->(1)]
 *        ���� free���� ���Ͽ��ٸ�, �ٽ� logical undo�� �����ϸ� �ȴ�.
 *        free segment�� mtx commit �Ͽ��ٸ�, [NTA->(1)]���� ���� undo����
 *        ���� skip�ϰ� �Ѿ��.
 *    + (1) -> logical undo
 *      : tbl-hdr slot�� freeslot�Ѵ�.
 *
 *  - (5)���� �α� �Ͽ��ٸ�
 *    + (5) -> logical undo
 *      : setDropTable�� ó���Ѵ�. (U/R) (a)
 *      : doNTADropTable�� �����Ͽ� dropTablePending�� ȣ���Ѵ�. (b)(c)
 *      =>
 *
 *      (5)(a)    (b)                 (c)
 *       |--|------|-------------------|-----------> time
 *                 |                   |
 *                 update             free
 *                 seg rID,           segment
 *                 meta pageID        [NTA->(a)]
 *                 in tbl-hdr         (NTA OP NULL)
 *                 (R/U)
 *      ��. (b)������ �α��ߴٸ� (b)�α׿� ���Ͽ� undo�� ����Ǹ� ������
 *          ���� ó���ȴ�.
 *          => (5)->(a)->(b)->(b)'->(a)'->(a)->(b)->(c)->(5)'->...
 *             NTA            CLR   CLR                  DUMMY_CLR
 *
 *      ��. (c)���� mtx commit�� �Ǿ��ٸ�
 *          => (5)->(a)->(b)->(c)->(d)->..->(5)'->...
 *             NTA                          DUMMY_CLR
 *
 *    + (1) -> logical undo
 *      : tbl-hdr slot�� freeslot�Ѵ�.
 ***********************************************************************/
IDE_RC smcTable::createTable( idvSQL              * aStatistics,
                              void*                 aTrans,
                              scSpaceID             aSpaceID,
                              const smiColumnList*  aColumns,
                              UInt                  aColumnSize,
                              const void*           aInfo,
                              UInt                  aInfoSize,
                              const smiValue*       aNullRow,
                              UInt                  aFlag,
                              ULong                 aMaxRow,
                              smiSegAttr            aSegmentAttr,
                              smiSegStorageAttr     aSegmentStoAttr,
                              UInt                  aParallelDegree,
                              const void**          aHeader )
{
    smpSlotHeader   * sSlotHeader;
    smcTableHeader  * sHeader;
    smOID             sFixOID = SM_NULL_OID;
    smOID             sLobColOID;
    smSCN             sInfiniteSCN;
    smiColumn       * sColumn;
    smcTableHeader    sHeaderArg;
    scPageID          sHeaderPageID = 0;
    UInt              sMax;
    UInt              sCount;
    UInt              sTableType;
    UInt              sState = 0;
    UInt              i;
    smLSN             sLsnNTA;
    svrLSN            sLsn4Vol;
    ULong             sNullRowID;
    smTID             sTID;

    IDU_FIT_POINT( "1.BUG-42154@smcTable::createTable" );

    if(aMaxRow == 0)
    {
        aMaxRow = ID_ULONG_MAX;
    }

    sTID = smxTrans::getTransID( aTrans );

    // BUG-37607 Transaction ID should be recorded in the slot containing table header.
    SM_SET_SCN_INFINITE_AND_TID( &sInfiniteSCN, sTID );

    sLsnNTA = smLayerCallback::getLstUndoNxtLSN( aTrans );

    /* ------------------------------------------------
     * [1] ���ο� Table�� ���� Table Header������
     * Catalog Table���� �Ҵ�޴´�.
     * ----------------------------------------------*/
    if(( aFlag & SMI_TABLE_PRIVATE_VOLATILE_MASK ) == SMI_TABLE_PRIVATE_VOLATILE_FALSE )
    {
        IDE_TEST( smpFixedPageList::allocSlot(aTrans,
                                              SMI_ID_TABLESPACE_SYSTEM_MEMORY_DIC,
                                              NULL,
                                              SMC_CAT_TABLE->mSelfOID,
                                              &(SMC_CAT_TABLE->mFixed.mMRDB),
                                              (SChar**)&sSlotHeader,
                                              sInfiniteSCN,
                                              SMC_CAT_TABLE->mMaxRow,
                                              SMP_ALLOC_FIXEDSLOT_ADD_INSERTCNT |
                                              SMP_ALLOC_FIXEDSLOT_SET_SLOTHEADER,
                                              SMR_OP_SMC_TABLEHEADER_ALLOC)
                  != IDE_SUCCESS );
    }
    else
    {
        /* PROJ-1407 Temporary Table
         * User session temporary table�� ��� restart�� free�Ǿ�� �ϹǷ�
         * temp catalog���� slot�� �Ҵ�޴´�.*/
        IDE_TEST( smpFixedPageList::allocSlotForTempTableHdr(
                                          SMI_ID_TABLESPACE_SYSTEM_MEMORY_DIC,
                                          SMC_CAT_TEMPTABLE->mSelfOID,
                                          &(SMC_CAT_TEMPTABLE->mFixed.mMRDB),
                                          (SChar**)&sSlotHeader,
                                          sInfiniteSCN ) 
                  != IDE_SUCCESS );
    }


    IDU_FIT_POINT( "1.BUG-18279@smcTable::createTable" );

    IDU_FIT_POINT( "1.PROJ-1552@smcTable::createTable" );


    /* ------------------------------------------------
     * [2] ���ο� Table�� ���� Table Header������
     * Catalog Table���� �Ҵ�޾����Ƿ�,
     * New Version List�� �߰��Ѵ�.
     * �� �Լ��� [2] ����.
     * ----------------------------------------------*/
    sHeaderPageID = SMP_SLOT_GET_PID((SChar *)sSlotHeader);
    sFixOID       = SM_MAKE_OID( sHeaderPageID,
                                 SMP_SLOT_GET_OFFSET( sSlotHeader ) );
    sState        = 2;
    sHeader       = (smcTableHeader *)( sSlotHeader + 1 );

    IDU_FIT_POINT_RAISE( "1.PROJ-1407@smcTable::createTable", err_ART );

    /* ------------------------------------------------
     * [3] �� Table�� Fixed ������ Row ���̸�
     * ����Ѵ�.
     * �� Column ������ <Offset + Size>���� �ִ밪��
     * Fixed ������ Row ���̷� ����.
     *
     * bugbug : �� �ڵ�� �ߺ����� ���Ǳ⶧����,
     * ������ �Լ��� �и����� ȣ���ϵ��� ó���ؾ���
     * ----------------------------------------------*/
    sTableType = aFlag & SMI_TABLE_TYPE_MASK;

    // Create Table�� ���� ����ó��
    IDE_TEST( checkError4CreateTable( aSpaceID,
                                      aFlag )
              != IDE_SUCCESS );

    /* Column List�� Valid���� �˻��ϰ� Column�� ������ Fixed row�� ũ��
       �� ���Ѵ�.*/
    IDE_TEST( validateAndGetColCntAndFixedRowSize( sTableType,
                                                   aColumns,
                                                   &sCount,
                                                   &sMax )
              != IDE_SUCCESS );

    /* ------------------------------------------------
     * ���� table header�� ��κ��� ����� �����ϰ�
     * �ʱ�ȭ�Ǵ���, page list entry�� ���ؼ���
     * ���̺�Ÿ�Կ� ���� �ʱ�ȭ�� �ٸ���.
     * ----------------------------------------------*/
    /* stack ������ table header �� �ʱ�ȭ */
    IDE_TEST( initTableHeader( aTrans,
                               aSpaceID,
                               sMax,
                               aColumnSize,
                               sCount,
                               sFixOID,
                               aFlag,
                               NULL,
                               SMC_TABLE_NORMAL,
                               SMI_OBJECT_NONE,
                               aMaxRow,
                               aSegmentAttr,
                               aSegmentStoAttr,
                               aParallelDegree,
                               &sHeaderArg )
              != IDE_SUCCESS );

    /* ���� table header ���� ���� */
    idlOS::memcpy(sHeader, &sHeaderArg, ID_SIZEOF(smcTableHeader));

    IDU_FIT_POINT( "1.BUG-31673@smcTable::createTable" );

    // table Ÿ�Կ� ���� �ش� pagelist entry�� mutex �� runtime ������ �ʱ�ȭ
    IDE_TEST( initLockAndRuntimeItem( sHeader) != IDE_SUCCESS );

    if ( sTableType == SMI_TABLE_DISK )
    {
        /* ------------------------------------------------
         * disk table�� page list entry�� �ʱ�ȭ�ϸ�,
         * table ����Ÿ ������ ���� segment ���� (mtx logging ���)
         * ----------------------------------------------*/
         // disk table�� table segment �Ҵ� �� table segment header page �ʱ�ȭ
         IDE_TEST( sdpSegment::allocTableSeg4Entry(
                       aStatistics,
                       aTrans,
                       aSpaceID,
                       sHeader->mSelfOID,
                       (sdpPageListEntry*)getDiskPageListEntry(sHeader),
                       SDR_MTX_LOGGING )
                   != IDE_SUCCESS );
    }

    /* ------------------------------------------------
     * [4] �� Table�� Fixed ������ Row ���̰� Persistent Page Size�� �ʰ��Ѵٸ�
     * ���� ó�� �Ѵ�.
     * ----------------------------------------------*/
    sTableType = SMI_GET_TABLE_TYPE( sHeader );

    IDE_DASSERT( sTableType == SMI_TABLE_MEMORY ||
                 sTableType == SMI_TABLE_META   ||
                 sTableType == SMI_TABLE_DISK   ||
                 sTableType == SMI_TABLE_VOLATILE );

    // !!] OP_NTA : table ���� ���꿡 ���� �α�
    IDE_TEST( smrLogMgr::writeNTALogRec(NULL, /* idvSQL* */
                                        aTrans,
                                        &sLsnNTA,
                                        SMI_ID_TABLESPACE_SYSTEM_MEMORY_DIC,
                                        SMR_OP_CREATE_TABLE,
                                        sFixOID)
              != IDE_SUCCESS );

    /* ------------------------------------------------
     * !! CHECK RECOVERY POINT
     * case) SMR_OP_CREATE_TABLE�� �α��ϰ� crash �߻�
     * undoAll�������� NTA logical undo�� �����Ͽ� table�� �����Ѵ�.
     * disk table�� �߰������� table segment���� free�Ѵ�.
     * ----------------------------------------------*/
    IDU_FIT_POINT( "1.PROJ-1552@smcTable::createTable" );
    sState = 1;

    /* ------------------------------------------------
     * DDL�� �����߻��� rollback�������� �Ҵ�� fixed slot��
     * ���Ͽ� commit scn�� �����Ҽ� �ֵ��� ǥ��
     * ----------------------------------------------*/
    IDE_TEST( smLayerCallback::addOID( aTrans,
                                       sFixOID,
                                       sFixOID,
                                       SMI_ID_TABLESPACE_SYSTEM_MEMORY_DIC,
                                       SM_OID_DROP_TABLE_BY_ABORT )
              != IDE_SUCCESS );

    /* Table Space�� ���ؼ� DDL�� �߻��ߴٴ°��� ǥ��*/
    IDE_TEST( smLayerCallback::addOID( aTrans,
                                       sFixOID,
                                       sFixOID,
                                       aSpaceID,
                                       SM_OID_DDL_TABLE )
              != IDE_SUCCESS );

    /* tableHeader.mCreateSCN ������ �ʿ��ϴٴ°��� ǥ�� */
    IDE_TEST( smLayerCallback::addOID( aTrans,
                                       sFixOID,
                                       sFixOID,
                                       SMI_ID_TABLESPACE_SYSTEM_MEMORY_DIC,
                                       SM_OID_CREATE_TABLE )
              != IDE_SUCCESS );

    /* [5] �� Table�� Column�� ������ Variable������ �����Ѵ�. */
    IDE_TEST( setColumnAtTableHeader( aTrans,
                                      sHeader,
                                      aColumns,
                                      sCount,
                                      aColumnSize )
              != IDE_SUCCESS );

    // BUG-28356 [QP]�ǽð� add column ���� ���ǿ���
    //           lob column�� ���� ���� ���� �����ؾ� ��
    // Table���� ��� Column�� ��ȸ�ϸ鼭 LOB Column�̸� LOB Segment�� �����Ѵ�.
    if( ( sTableType == SMI_TABLE_DISK ) &&
        ( sHeader->mLobColumnCount != 0 ) )
    {
        for( i = 0 ; i < sHeader->mColumnCount ; i++ )
        {
            sColumn = getColumnAndOID( sHeader,
                                       i, // Column Idx
                                       &sLobColOID );

            if( ( sColumn->flag & SMI_COLUMN_TYPE_MASK) == SMI_COLUMN_TYPE_LOB )
            {
                SC_MAKE_NULL_GRID( sColumn->colSeg );
                IDE_TEST(sdpSegment::allocLobSeg4Entry(
                             aStatistics,
                             aTrans,
                             sColumn,
                             sLobColOID,
                             sHeader->mSelfOID,
                             SDR_MTX_LOGGING )
                         != IDE_SUCCESS);
            }
        }
    }

    /* ------------------------------------------------
     * [6] ���ο� Table�� Info ������ ���Ͽ� ���� �����Ѵ�.
     * ----------------------------------------------*/
    IDE_TEST(setInfoAtTableHeader(aTrans, sHeader, aInfo, aInfoSize)
             != IDE_SUCCESS);

    /* ------------------------------------------------
     * [7] ���ο� Table�� NullRow�� �����Ѵ�.
     *
     * FOR A4 : nullrow ó��
     *
     * nullrow�� table�� insert�ϴ� ������� ó���Ѵ�.
     *
     * + memory table nullrow (smcRecord::makeNullRow)
     *  _______________________________________________________
     * |slot header|record header| fixed|__var__|fixed|__var__|
     * |___________|_____________|______|2_|OID_|_____|2_|OID_|
     *                                     | ___________/
     *                                  ___|/______
     *                                  |0|var    |
     *                                  |_|selfOID|
     *                                    catalog table��
     *                                    nullOID
     *
     *  nullrow�� varcolumn�� �Ź� �Ҵ����� �ʱ����ؼ���
     *  �Ϲ� ���̺��� nullrow�� column�� varcolumn�� �Ҵ���� ������
     *  ���� catalog table�� nullOID�� ����� varcolumn��
     *  assign�ϵ��� ó��
     *
     * ----------------------------------------------*/

    /* BUG-32544 [sm-mem-collection] The MMDB alter table operation can
     * generate invalid null row.
     * CreateTable�ÿ� ������ NullRow�� Undo�� �ʿ䰡 �����ϴ�.
     * ������ Rollback�Ǹ� Table�� Drop�Ǹ鼭 ��ȯ�� ���̱� �����Դϴ�.*/
    sLsnNTA = smLayerCallback::getLstUndoNxtLSN( aTrans );
    if (sTableType == SMI_TABLE_DISK)
    {
        // PROJ-1705
        // Disk Table�� Disk Temp Table��
        // Null Row�� insert���� �ʵ��� �����Ѵ�.

        IDE_TEST( initRowTemplate( NULL, sHeader, NULL ) != IDE_SUCCESS );
    }
    /* PROJ-1594 Volatile TBS */
    else
    {
        if( sTableType == SMI_TABLE_VOLATILE )
        {
            sLsn4Vol = svrLogMgr::getLastLSN(
                ((smxTrans*)aTrans)->getVolatileLogEnv());

            IDE_TEST( smLayerCallback::makeVolNullRow( aTrans,
                                                       sHeader,
                                                       sInfiniteSCN,
                                                       aNullRow,
                                                       SM_FLAG_MAKE_NULLROW,
                                                       (smOID*)&sNullRowID )
                      != IDE_SUCCESS );

            /* vol tbs�� ��� �Ʒ��� ���� UndoLog�� �����ִ� ����
             * NTA Log�̴�. */
            IDE_TEST( svrLogMgr::removeLogHereafter( 
                    ((smxTrans*)aTrans)->getVolatileLogEnv(),
                    sLsn4Vol )
                != IDE_SUCCESS );
        }
        else
        {
            IDE_TEST(smcRecord::makeNullRow(aTrans,
                                            sHeader,
                                            sInfiniteSCN,
                                            aNullRow,
                                            SM_FLAG_MAKE_NULLROW,
                                            (smOID*)&sNullRowID)
                     != IDE_SUCCESS);

            /* smcRecord::makeNullRow�ȿ��� Null Row OID Setting�����ؼ�
               Logging�մϴ�.*/
        }

        IDE_TEST( setNullRow( aTrans,
                              sHeader,
                              sTableType,
                              &sNullRowID )
                  != IDE_SUCCESS );
    }

    /* BUG-32544 [sm-mem-collection] The MMDB alter table operation can
     * generate invalid null row.
     * NTA Log�� ���� NullRow�� ���� Undo�� �����ϴ�*/
    IDE_TEST(smrLogMgr::writeNullNTALogRec( NULL, /* idvSQL* */
                                            aTrans,
                                            & sLsnNTA)
             != IDE_SUCCESS);

    sState = 0;
    // insert dirty page into dirty page list
    IDE_TEST( smmDirtyPageMgr::insDirtyPage( SMI_ID_TABLESPACE_SYSTEM_MEMORY_DIC,
                                             SM_MAKE_PID(sHeader->mSelfOID))
              != IDE_SUCCESS );

    // [8] ���� ������ Table�� ���Ͽ� X Lock ��û
    IDE_TEST( smLayerCallback::lockTableModeX( aTrans, SMC_TABLE_LOCK( sHeader ) )
              != IDE_SUCCESS );

    *aHeader = (const void*)sHeader;


    return IDE_SUCCESS;
#ifdef ALTIBASE_FIT_CHECK
    IDE_EXCEPTION( err_ART );
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_ART));
    }
#endif
    IDE_EXCEPTION_END;

    IDE_PUSH();
    switch(sState)
    {
        case 2:

            if(( aFlag & SMI_TABLE_PRIVATE_VOLATILE_MASK )
               == SMI_TABLE_PRIVATE_VOLATILE_FALSE )
            {
                IDE_ASSERT( smLayerCallback::addOID( aTrans,
                                                     SMC_CAT_TABLE->mSelfOID,
                                                     sFixOID,
                                                     SMI_ID_TABLESPACE_SYSTEM_MEMORY_DIC,
                                                     SM_OID_NEW_INSERT_FIXED_SLOT )
                            == IDE_SUCCESS );
            }
            else
            {
                IDE_ASSERT( smLayerCallback::addOID( aTrans,
                                                     SMC_CAT_TEMPTABLE->mSelfOID,
                                                     sFixOID,
                                                     SMI_ID_TABLESPACE_SYSTEM_MEMORY_DIC,
                                                     SM_OID_NEW_INSERT_FIXED_SLOT )
                            == IDE_SUCCESS );
            }

        case 1:
            IDE_ASSERT( smmDirtyPageMgr::insDirtyPage(
                            SMI_ID_TABLESPACE_SYSTEM_MEMORY_DIC,sHeaderPageID)
                        == IDE_SUCCESS );

        default:
            break;
    }
    IDE_POP();

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : Table Header�� ���ο� Info ������ Setting�Ѵ�. ����
 *               ���� Info ������ �ִٸ� �����ϰ� ���ο� Info �� ��ü�Ѵ�.
 *               ���� ���ο� INfo ���� �����Ȱ��� ���ٸ� Table�� Info
 *               �� NULL�� Setting�ȴ�.
 *
 * aTrans     - [IN] : Transaction Pointer
 * aTable     - [IN] : Table Header
 * aInfo      - [IN] : Info Pointer : ������ Info�� ���ٸ� NULL
 * aInfoSize  - [IN] : Info Size    : ������ Info�� ���ٸ� 0
 ***********************************************************************/
IDE_RC smcTable::setInfoAtTableHeader( void*            aTrans,
                                       smcTableHeader*  aTable,
                                       const void*      aInfo,
                                       UInt             aInfoSize)
{
    smiValue         sVal;
    smOID            sNextOid = SM_NULL_OID;
    scPageID         sHeaderPageID = SM_NULL_PID;
    UInt             sState = 0;
    smOID            sFstVCPieceOID = SM_NULL_OID;
    smOID            sVCPieceOID = SM_NULL_OID;
    smVCPieceHeader* sVCPieceHeader;
    UInt             sInfoPartialLen;
    UInt             sOffset = 0;
    UInt             sMaxVarLen;

    IDE_DASSERT( aTrans != NULL );
    IDE_DASSERT( aTable != NULL );

    /* [1] ���� info ������ �ִ� ��� �� old version�� versioning list�� �߰� */
    if(aTable->mInfo.fstPieceOID != SM_NULL_OID)
    {
        sVCPieceOID = aTable->mInfo.fstPieceOID;

        /* VC�� �����ϴ� VC Piece�� Flag�� Delete Bit�߰�.*/
        if ( ( aTable->mInfo.flag & SM_VCDESC_MODE_MASK ) == SM_VCDESC_MODE_OUT )
        {
            IDE_TEST( smcRecord::deleteVC( aTrans,
                                           SMC_CAT_TABLE,
                                           sVCPieceOID )
                    != IDE_SUCCESS );
        }
        else
        {
            /* Nothing to do. inmode �� fixed record���� �Բ� ó���ȴ�. */
        }
    }

    if( aInfo != NULL )
    {
        sInfoPartialLen = aInfoSize;

        /* =================================================================
         * [2] info ������ variable page�� ����
         *     info ������ SM_PAGE_SIZE�� �Ѵ� ��� ���� �������� ���� ����
         * ================================================================ */
        sMaxVarLen = SMP_VC_PIECE_MAX_SIZE;

        while ( sInfoPartialLen > 0 )
        {
            /* [1-1] info ������ ���� variable column ���� ���� */
            if ( sInfoPartialLen == aInfoSize )
            {
                sVal.length      = sInfoPartialLen  % sMaxVarLen;
                sOffset          = (sInfoPartialLen / sMaxVarLen) * sMaxVarLen;
                sInfoPartialLen -= sVal.length;
            }
            else
            {
                sVal.length      = sMaxVarLen;
                sOffset         -= sMaxVarLen;
                sInfoPartialLen -= sMaxVarLen;
            }
            sVal.value  = (void *)((char *)aInfo + sOffset);

            /* [1-2] info ������ �����ϱ� ���� variable slot �Ҵ� */
            IDE_TEST( smpVarPageList::allocSlot(
                          aTrans,
                          SMI_ID_TABLESPACE_SYSTEM_MEMORY_DIC,
                          SMC_CAT_TABLE->mSelfOID,
                          SMC_CAT_TABLE->mVar.mMRDB,
                          sVal.length,
                          sNextOid,
                          &sVCPieceOID,
                          (SChar**)& sVCPieceHeader) != IDE_SUCCESS );

            /* [1-3] �Ҵ���� ���Կ� info ���� ���� �� �α� *
             *       dirty page ����� �Լ� ���ο��� ����� */
            IDE_TEST(smrUpdate::updateVarRow(
                         NULL, /* idvSQL* */
                         aTrans,
                         SMI_ID_TABLESPACE_SYSTEM_MEMORY_DIC,
                         sVCPieceOID,
                         (SChar*)(sVal.value),
                         sVal.length) != IDE_SUCCESS);


            IDE_TEST( smpVarPageList::setValue(
                          SMI_ID_TABLESPACE_SYSTEM_MEMORY_DIC,
                          sVCPieceOID,
                          sVal.value,
                          sVal.length) != IDE_SUCCESS );

            /* [1-4] ���� �Ҵ���� ������ versioning list�� �߰� */
            IDE_TEST( smLayerCallback::addOID( aTrans,
                                               SMC_CAT_TABLE->mSelfOID,
                                               sVCPieceOID,
                                               SMI_ID_TABLESPACE_SYSTEM_MEMORY_DIC,
                                               SM_OID_NEW_VARIABLE_SLOT )
                      != IDE_SUCCESS );

            /* [1-5] info ������ multiple page�� ���ļ� ����Ǵ� ���
             *       �� �������鳢���� ����Ʈ�� ����                  */
            sNextOid = sVCPieceOID;
        }

        sFstVCPieceOID = sVCPieceOID;
    }
    else
    {
        aInfoSize = 0;
        sFstVCPieceOID = SM_NULL_OID;
    }

    sHeaderPageID = SM_MAKE_PID(aTable->mSelfOID);
    sState = 1;

    /* [3] info ���� �� �α� */
    IDE_TEST(smrUpdate::updateInfoAtTableHead(
                 NULL, /* idvSQL* */
                 aTrans,
                 SM_MAKE_PID(aTable->mSelfOID),
                 SM_MAKE_OFFSET(aTable->mSelfOID) + SMP_SLOT_HEADER_SIZE,
                 &(aTable->mInfo),
                 sFstVCPieceOID,
                 aInfoSize,
                 SM_VCDESC_MODE_OUT)
             != IDE_SUCCESS);


    aTable->mInfo.length      = aInfoSize;
    aTable->mInfo.fstPieceOID = sFstVCPieceOID;
    aTable->mInfo.flag        = SM_VCDESC_MODE_OUT;

    /* [4] �ش� ���̺��� ���� �������� ���� �������� ��� */
    IDE_TEST(smmDirtyPageMgr::insDirtyPage(SMI_ID_TABLESPACE_SYSTEM_MEMORY_DIC,
                                           sHeaderPageID)
             != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if (sState == 1)
    {
        IDE_PUSH();
        IDE_ASSERT(smmDirtyPageMgr::insDirtyPage(
                       SMI_ID_TABLESPACE_SYSTEM_MEMORY_DIC, sHeaderPageID)
                   == IDE_SUCCESS);
        IDE_POP();
    }

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : Table Header�� ���ο� Column List������ Setting�Ѵ�. ����
 *               ���� Column ������ �ִٸ� �����ϰ� ���ο� Column List�� ��ü�Ѵ�.
 *               ���� ���ο� Column List���� �����Ȱ��� ���ٸ� Table�� Column
 *               List�� NULL�� Setting�ȴ�.
 *
 * aTrans       - [IN] Transaction Pointer
 * aTableHeader - [IN] Table Header Pointer
 * aColumnsList - [IN] Column List   : ������ Column�� ���ٸ� NULL��
 * aColumnCnt   - [IN] Column Count  : ������ Column�� ���ٸ� 0
 * aColumnSize  - [IN] Column Size   : ������ Column�� ���ٸ� 0
 ***********************************************************************/
IDE_RC smcTable::setColumnAtTableHeader( void*                aTrans,
                                         smcTableHeader*      aTableHeader,
                                         const smiColumnList* aColumnsList,
                                         UInt                 aColumnCnt,
                                         UInt                 aColumnSize )
{
    smOID             sVCPieceOID;
    smOID             sTableOID = aTableHeader->mSelfOID;
    UInt              sLength;
    smiColumnList   * sColumnList;
    const smiColumn * sOrgColumn;
    const smiColumn * sNewColumn;
    UInt              i;
    UInt              sLobColCnt;
    UInt              sUnitedVarColCnt = 0;

    IDE_DASSERT( aTrans         != NULL );
    IDE_DASSERT( aTableHeader   != NULL );

    IDE_ASSERT( aColumnsList != NULL && aColumnSize != 0 );

    /* ������ Column List �� �����Ѵ�. */
    if(aTableHeader->mColumns.fstPieceOID != SM_NULL_OID)
    {
        sColumnList = (smiColumnList *)aColumnsList;

        for( i = 0;
             (( i < aTableHeader->mColumnCount ) && ( sColumnList != NULL ));
             i++, sColumnList = sColumnList->next )
        {
            sOrgColumn = smcTable::getColumn( aTableHeader, i );
            sNewColumn = sColumnList->column;

            if( (sOrgColumn->flag != sNewColumn->flag) &&
                (sOrgColumn->offset != sNewColumn->offset) &&
                (sOrgColumn->size != sNewColumn->size) &&
                ((sOrgColumn->id & ~SMI_COLUMN_ID_MASK) !=
                 (sNewColumn->id & ~SMI_COLUMN_ID_MASK)) )
            {
                break;
            }
            else
            {
                // BUG-44814 DDL ����� ���̺��� ������Ǵ� ��� ������ ��� ������ DDL ���������� �����ؾ� �մϴ�.
                // ���ο� smiColumn �� set �Ҷ� ������ ��������� �����Ѵ�.
                idlOS::memcpy( (void*)&sNewColumn->mStat, &sOrgColumn->mStat, ID_SIZEOF(smiColumnStat) );
            }
        }//for

        IDE_TEST_RAISE( ((i != aTableHeader->mColumnCount) && (sColumnList != NULL)),
                        not_match_error );

        if ( ( aTableHeader->mColumns.flag & SM_VCDESC_MODE_MASK ) == SM_VCDESC_MODE_OUT )
        {
            IDE_TEST( smcRecord::deleteVC( aTrans,
                                           SMC_CAT_TABLE,
                                           aTableHeader->mColumns.fstPieceOID )
                    != IDE_SUCCESS );
        }
        else
        {
            /* Nothing to do. inmode �� fixed record���� �Բ� ó���ȴ�. */
        }
    }

    IDE_TEST( storeTableColumns( aTrans,
                                 aColumnSize,
                                 aColumnsList,
                                 SMI_GET_TABLE_TYPE( aTableHeader ),
                                 &sVCPieceOID )
              != IDE_SUCCESS );

    // BUG-28356 [QP]�ǽð� add column ���� ���ǿ���
    //           lob column�� ���� ���� ���� �����ؾ� ��
    // Table�� Lob Column�� ���� ����Ѵ�.
    sLobColCnt          = 0;
    sUnitedVarColCnt    = 0;
    for( sColumnList = (smiColumnList *)aColumnsList;
         sColumnList != NULL ;
         sColumnList = sColumnList->next )
    {
        sNewColumn = sColumnList->column;
        if ( (sNewColumn->flag & SMI_COLUMN_TYPE_MASK) == SMI_COLUMN_TYPE_LOB )
        {
            sLobColCnt++;
        }
        else if ( (sNewColumn->flag & SMI_COLUMN_TYPE_MASK) == SMI_COLUMN_TYPE_VARIABLE )
        {
            sUnitedVarColCnt++;
        }
        else
        {
            /* Nothing to do */
        }
    }

    sLength = aColumnCnt * aColumnSize;
    /* ------------------------------------------------
     * table header�� �÷� ���� ���� �� �α�
     * - SMR_LT_UPDATE�� SMR_SMC_TABLEHEADER_UPDATE_COLUMNS
     * ----------------------------------------------*/
    IDE_TEST(smrUpdate::updateColumnsAtTableHead(NULL, /* idvSQL* */
                                                 aTrans,
                                                 SM_MAKE_PID(sTableOID),
                                                 SM_MAKE_OFFSET(sTableOID)
                                                 + SMP_SLOT_HEADER_SIZE,
                                                 &(aTableHeader->mColumns),
                                                 sVCPieceOID,
                                                 sLength,
                                                 SM_VCDESC_MODE_OUT,
                                                 aTableHeader->mLobColumnCount,
                                                 sLobColCnt,
                                                 aTableHeader->mColumnCount,
                                                 aColumnCnt)
             != IDE_SUCCESS);
    aTableHeader->mLobColumnCount = sLobColCnt;
    aTableHeader->mUnitedVarColumnCount = sUnitedVarColCnt;

    // PROJ-1911
    // ALTER TABLE ... ADD COLUMN ... ���� ��,
    // default value�� 'NULL'�� Į���� column count�� ������ �� ����
    aTableHeader->mColumnCount = aColumnCnt;


    aTableHeader->mColumns.length = sLength;
    aTableHeader->mColumns.fstPieceOID = sVCPieceOID;
    aTableHeader->mColumns.flag = SM_VCDESC_MODE_OUT;

    /* [4] �ش� ���̺��� ���� �������� ���� �������� ��� */
    IDE_TEST(smmDirtyPageMgr::insDirtyPage(
                 SMI_ID_TABLESPACE_SYSTEM_MEMORY_DIC, SM_MAKE_PID(sTableOID))
             != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION(not_match_error);
    {
        IDE_SET(ideSetErrorCode (smERR_ABORT_Column_Mismatch));
    }
    IDE_EXCEPTION_END;

    IDE_PUSH();
    IDE_ASSERT(smmDirtyPageMgr::insDirtyPage(
                   SMI_ID_TABLESPACE_SYSTEM_MEMORY_DIC, SM_MAKE_PID(sTableOID))
               == IDE_SUCCESS);
    IDE_POP();

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : Column List�� ���� Column�� ������ Fixed Row�� ũ�⸦ ���Ѵ�.
 *
 * aTableType    - [IN] Table Type
 * aColList      - [IN] Column List
 * aColCnt       - [OUT] Column ����
 * aFixedRowSize - [OUT] Fixed Row�� ũ��
 ***********************************************************************/
IDE_RC smcTable::validateAndGetColCntAndFixedRowSize(const UInt           aTableType,
                                                     const smiColumnList *aColumnList,
                                                     UInt                *aColumnCnt,
                                                     UInt                *aFixedRowSize)
{
    const smiColumn * sColumn;
    UInt  sCount;
    UInt  sColumnEndOffset;
    UInt  sFixedRowSize;
    smiColumnList *sColumnList;

    IDE_DASSERT(aColumnList != NULL);
    IDE_DASSERT(aColumnCnt  != NULL);
    IDE_DASSERT(aFixedRowSize != NULL);

    sColumnList = (smiColumnList *)aColumnList;
    sFixedRowSize = 0;
    sCount = 0;

    while( sColumnList != NULL)
    {
        sColumn  = sColumnList->column;

        if( (sColumn->flag & SMI_COLUMN_COMPRESSION_MASK)
            == SMI_COLUMN_COMPRESSION_FALSE )
        {
            switch(sColumn->flag & SMI_COLUMN_TYPE_MASK)
            {
                case SMI_COLUMN_TYPE_LOB:
 
                    if (aTableType == SMI_TABLE_DISK)
                    {
                        sColumnEndOffset = sColumn->offset + smLayerCallback::getDiskLobDescSize();
                    }
                    else
                    {
                        sColumnEndOffset = sColumn->offset +
                            IDL_MAX( ID_SIZEOF(smVCDescInMode) + sColumn->vcInOutBaseSize,
                                     ID_SIZEOF(smcLobDesc) );
                    }
 
                    break;
 
                case SMI_COLUMN_TYPE_VARIABLE:
 
                    if (aTableType == SMI_TABLE_DISK)
                    {
                        IDE_ASSERT(0);
                    }
                    else
                    {
                        sColumnEndOffset = ID_SIZEOF(smpSlotHeader);
                    }

                    break;

                case SMI_COLUMN_TYPE_VARIABLE_LARGE:

                    if ( aTableType == SMI_TABLE_DISK )
                    {
                        IDE_ASSERT(0);
                    }
                    else
                    {
                        // PROJ-1557 varchar 32k
                        // column end offset = max( vc header_inmode+vcInOutBaseSize, vc header )
                            sColumnEndOffset = sColumn->offset +
                                               IDL_MAX( ID_SIZEOF(smVCDescInMode) + sColumn->vcInOutBaseSize,
                                                        ID_SIZEOF(smVCDesc) );
                            IDE_TEST_RAISE( sColumn->size > SMP_MAX_VAR_COLUMN_SIZE,
                                            err_var_column_size );
                    }
 
                    break;
 
                case SMI_COLUMN_TYPE_FIXED:
 
                    sColumnEndOffset = sColumn->offset + sColumn->size;
 
                    break;
 
                default:
                    IDE_ASSERT(0);
                    break;
            }
        }
        else
        {
            // PROJ-2264
            sColumnEndOffset = sColumn->offset + ID_SIZEOF(smOID);
        }

        if( sColumnEndOffset > sFixedRowSize )
        {
            sFixedRowSize = sColumnEndOffset;
        }

        sCount++;
        sColumnList = sColumnList->next;
    }

    sFixedRowSize = idlOS::align8((UInt)(sFixedRowSize));

    if (aTableType == SMI_TABLE_DISK)
    {
        //PROJ-1705
    }
    else
    {
        IDE_TEST_RAISE( sFixedRowSize > SMP_PERS_PAGE_BODY_SIZE,
                        err_fixed_row_size );

        IDE_TEST_RAISE( sFixedRowSize > SMP_MAX_FIXED_ROW_SIZE,
                        err_fixed_row_size);
    }

    *aColumnCnt = sCount;
    *aFixedRowSize = sFixedRowSize;

    return IDE_SUCCESS;

    IDE_EXCEPTION(err_var_column_size);
    {
        IDE_SET(ideSetErrorCode (smERR_ABORT_smcVarColumnSizeError));
    }
    IDE_EXCEPTION(err_fixed_row_size);
    {
        IDE_SET(ideSetErrorCode (smERR_ABORT_smcFixedPageSizeError));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}



/***********************************************************************
 * Description :  PROJ-1362 QP - Large Record & Internal LOB ����.
 *                table�� �÷������� �ε��� ���� ���� Ǯ��.
 *  Table�� Column ������ Variable slot(��)�� �Ҵ��Ͽ� �����Ѵ�.
 *  - code design
 * ���� �÷� piece���� :=0;
 * ���� variable slot OID := SM_NULL_OID;
 * SMP_MAX_VAR_SIZEũ�� ��ŭ �÷� piece���� �Ҵ�.
 * ���� column :=  aColumns;
 * while(true)
 * do
 *   if((���� Column Piece+ mtcColumn) > �÷� piece ���� ���� �Ǵ� ���� �÷� == NULL)
 *   then
 *      ���� column piece���̿� �ش��ϴ� variable slot �Ҵ�;
 *      variable slot body�� ���� physical logging;
 *      ���� �÷� piece���� ��ŭ  variable slot���� memory copy;
 *      �Ҵ��� variable slot�� ���̸� �����÷� piece���̷� ����;
 *      //������ �Ҵ��� variable slot�� next(mOID)�����۾�.
 *      if (���� variable slot OID != SM_NULL_OID)
 *      then
 *        ���� variable slot�� next(mOID)�� �Ҵ�� variable slot���� ���濡
 *                ���� physical logging;
 *        ���� variable slot�� next(mOID)�� �Ҵ�� variable slot���� �޸� ����;
 *         Dirty Page List�� ���� variable slot������ �߰�;
 *      else
 *          table header�� mColumns OID�� ����ϱ� ���Ͽ�,
 *             ù��° variable slot�� ����Ѵ�;
 *      fi
 *      Ʈ����� rollback�ÿ� �Ҵ�� new variable slot��
 *      memory ager�� remove�Ҽ� �ֵ��� SM_OID_NEW_VARIABLE_SLOT�� ǥ��;
 *      Dirty Page List�� ���� �Ҵ��� variable slot�� ���� ���������;
 *      if(���� �÷� == NULL)
 *      then
 *        break;
 *      fi
 *
 *      ���� variable slot OID ����;
 *      �÷� piece buffer �ʱ�ȭ;
 *      ���� column piece ���� :=0;
 *   fi
 *   mtcColumnũ���� aColumnSize��ŭ �÷� piece buffer�� ���縦 �Ѵ�;
 *   ���� �÷� piece ���� ���� ����;
 *  ���� �÷����� �̵�;
 *done  //end while
 ***********************************************************************/
IDE_RC smcTable::storeTableColumns( void*                  aTrans,
                                    UInt                   aColumnSize,
                                    const smiColumnList*   aColumns,
                                    UInt                   aTableType,
                                    smOID*                 aHeadVarOID)
{
    ULong             sColumnsPiece[SMP_VC_PIECE_MAX_SIZE / ID_SIZEOF(ULong)];
    smiColumnList*    sColumns;
    const smiColumn*  sColumn;
    UInt              i;
    smVCPieceHeader*  sVCPieceHeader        = NULL;
    smOID             sVCPieceOID           = SM_NULL_OID;
    smVCPieceHeader*  sPreVCPieceHeader     = NULL;
    smOID             sPreVCPieceOID        = SM_NULL_OID;
    smVCPieceHeader   sAftVCPieceHeader;
    SChar*            sVarData;
    UInt              sCurColumnsPieceLen = 0;
    UInt              sPieceType;

    IDE_DASSERT( aTrans         != NULL );
    IDE_DASSERT( aColumns       != NULL );
    IDE_DASSERT( aHeadVarOID    != NULL );

    sColumns = (smiColumnList*) aColumns;
    *aHeadVarOID = SM_NULL_OID;

    //�÷� piece buffer �ʱ�ȭ .
    idlOS::memset((SChar*)sColumnsPiece,0x00,SMP_VC_PIECE_MAX_SIZE);
    for(i=0;  ;i++ )
    {
        // ���̺� �÷������� SMI_COLUMN_ID_MAXIMUM�� �ʰ��ϸ� ����.
        IDE_TEST_RAISE( i > SMI_COLUMN_ID_MAXIMUM,
                        maximum_column_count_error );
        // (���� Column Piece+ mtcColumn) > �÷� piece ���۱��� �Ǵ� ���� �÷� == NULL �̸�
        // variable slot�Ҵ� �� �κ� �÷� ���� ����.
        if( (( sCurColumnsPieceLen + aColumnSize ) > SMP_VC_PIECE_MAX_SIZE )
            || ( sColumns == NULL ) )
        {
            if( aTableType == SMI_TABLE_DISK )
            {
                sPieceType = SM_VCPIECE_TYPE_DISK_COLUMN;
            }
            else
            {
                sPieceType = SM_VCPIECE_TYPE_MEMORY_COLUMN;
            }

            //���� column piece���̿� �ش��ϴ� variable slot �Ҵ�.
            IDE_TEST( smpVarPageList::allocSlot(
                          aTrans,
                          SMI_ID_TABLESPACE_SYSTEM_MEMORY_DIC,
                          SMC_CAT_TABLE->mSelfOID,
                          SMC_CAT_TABLE->mVar.mMRDB,
                          sCurColumnsPieceLen,
                          SM_NULL_OID,
                          &sVCPieceOID,
                          (SChar**)&sVCPieceHeader,
                          sPieceType ) != IDE_SUCCESS);

            //variable slot body�� ���� physical logging.
            IDE_TEST(smrUpdate::updateVarRow(
                         NULL, /* idvSQL* */
                         aTrans,
                         SMI_ID_TABLESPACE_SYSTEM_MEMORY_DIC,
                         sVCPieceOID,
                         (SChar*)sColumnsPiece,
                         sCurColumnsPieceLen) != IDE_SUCCESS);


            //���� �÷� piece���� ��ŭ  variable slot���� memory copy.
            sVarData = (SChar*)(sVCPieceHeader+1);

            idlOS::memcpy(sVarData,(SChar*)sColumnsPiece, sCurColumnsPieceLen);

            //�Ҵ��� variable slot�� ���̸� �����÷� piece���̷� ����.
            sVCPieceHeader->length = sCurColumnsPieceLen;

            //������ �Ҵ��� variable slot�� next(mOID)�����۾�.
            if(sPreVCPieceOID != SM_NULL_OID)
            {
                IDE_ASSERT( smmManager::getOIDPtr( 
                                SMI_ID_TABLESPACE_SYSTEM_MEMORY_DIC, 
                                sPreVCPieceOID,
                                (void**)&sPreVCPieceHeader )
                            == IDE_SUCCESS );

                sAftVCPieceHeader = *(sPreVCPieceHeader);

                sAftVCPieceHeader.nxtPieceOID = sVCPieceOID;

                //���� variable slot�� next(mOID)�� �Ҵ�� variable slot���� ���濡
                //���� physical logging.
                IDE_TEST( smrUpdate::updateVarRowHead(NULL, /* idvSQL* */
                                                      aTrans,
                                                      SMI_ID_TABLESPACE_SYSTEM_MEMORY_DIC,
                                                      sPreVCPieceOID,
                                                      sPreVCPieceHeader,
                                                      &sAftVCPieceHeader)
                          != IDE_SUCCESS);


                //���� variable slot�� next(mOID)�� �Ҵ�� variable slot���� �޸� ����.
                sPreVCPieceHeader->nxtPieceOID = sVCPieceOID;
                //Dirty Page List�� ���� variable slot������ �߰�.
                IDE_TEST( smmDirtyPageMgr::insDirtyPage(
                              SMI_ID_TABLESPACE_SYSTEM_MEMORY_DIC,
                              SM_MAKE_PID(sPreVCPieceOID))
                          != IDE_SUCCESS);
            }//if sPreVCPieceOID  != SM_NULL_OID
            else
            {
                //table header�� mColumns OID�� ����ϱ� ���Ͽ�,
                //ù��° variable slot�� ����Ѵ�.
                *aHeadVarOID = sVCPieceOID;
            }//else
            //Ʈ����� rollback�ÿ� �Ҵ�� new variable slot��
            //memory ager�� remove�Ҽ� �ֵ��� SM_OID_NEW_VARIABLE_SLOT�� ǥ��.
            IDE_TEST( smLayerCallback::addOID(
                          aTrans,
                          SMC_CAT_TABLE->mSelfOID,
                          sVCPieceOID,
                          SMI_ID_TABLESPACE_SYSTEM_MEMORY_DIC,
                          SM_OID_NEW_VARIABLE_SLOT)
                      != IDE_SUCCESS);

            //Dirty Page List�� ���� �Ҵ��� variable slot�� ���� ���������.
            IDE_TEST( smmDirtyPageMgr::insDirtyPage( SMI_ID_TABLESPACE_SYSTEM_MEMORY_DIC,
                                                     SM_MAKE_PID(sVCPieceOID) )
                      != IDE_SUCCESS );
            // �÷� list�� ������ ����.
            if(sColumns == NULL)
            {
                break;
            }//if sColumns == NULL

            //���� variable slot OID ����.
            sPreVCPieceOID = sVCPieceOID;
            //�÷� piece buffer �ʱ�ȭ .
            idlOS::memset((SChar*)sColumnsPiece, 0x00, SMP_VC_PIECE_MAX_SIZE);
            sCurColumnsPieceLen = 0;
        }//if

        sColumn = sColumns->column;

        //mtcColumnũ���� aColumnSize��ŭ column piece buffer�� ���縦 �Ѵ�.
        idlOS::memcpy((SChar*)sColumnsPiece+sCurColumnsPieceLen,sColumn,aColumnSize);

        if( (sColumn->id & SMI_COLUMN_ID_MASK) == 0)
        {
            ((smiColumn *)( (SChar*)sColumnsPiece+sCurColumnsPieceLen))->id = (sColumn->id)|i;
        }//if
        else
        {
            IDE_TEST_RAISE( ((sColumn->id & SMI_COLUMN_ID_MASK) != i),
                            id_value_error );

        }//else
        //���� �÷� piece ���� ���� ����.
        sCurColumnsPieceLen += aColumnSize;
        //���� �÷����� �̵�.
        sColumns = sColumns->next;
    }//for

    return IDE_SUCCESS;

    IDE_EXCEPTION( id_value_error );
    {
        IDE_SET(ideSetErrorCode (smERR_ABORT_Invalid_ID_Value));
    }

    IDE_EXCEPTION( maximum_column_count_error );
    {
        IDE_SET(ideSetErrorCode (smERR_ABORT_Maximum_Column_count));
    }

    IDE_EXCEPTION_END;


    return IDE_FAILURE;
}

/***********************************************************************
 * Description :  PROJ-1362 QP - Large Record & Internal LOB ����.
 *                table�� �÷������� �ε��� ���� ���� Ǯ��.
 *  ���̺��� �÷������� ��� variable slot���� �����ϰ�,
 *  ���̺� ����� mColumns.mOID�� null oid�� �����Ѵ�.
 *  - code design
 *  ������ variable slot���� ó�� variable ���� �������� slot�� �����ϱ�
 *  ���Ͽ� OID stack�� ����;
 *  while( stack�� empty�� �ƴҶ� ����)
 *  do
 *    transaction��  last undo next LSN�� ���;
 *    if(stack�� ���� item�� ���� item�� ���� �ϴ°� ?)
 *    then
 *       ���� variable slot�� ���� slot next(mOID)�� null�� �����ϴ�
 *       physical logging;
 *       ���� variable slot�� ���� slot next(mOID)�� null�� ����;
 *       Dirty Page List�� ���� variable slot������ �߰�;
 *    else
 *         table header��  mColumns OID��  SM_NULL_OID���� �����ϴ�
 *         physical logging;
 *         table header��  mColumns OID��  SM_NULL_OID���� ����;
 *    fi
 *    catalog table�� ���� variable slot�� �����Ͽ� ��ȯ��;
 *  done
 *  stack memory  ����;
 ***********************************************************************/
IDE_RC smcTable::freeColumns(idvSQL          *aStatistics,
                             void*           aTrans,
                             smcTableHeader* aTableHeader)
{
    SInt                i;
    smLSN               sLsnNTA2;
    smcOIDStack         sOIDStack;
    smOID               sCurOID;
    smVCPieceHeader   * sPreVCPieceHeader;
    smVCPieceHeader     sAftVCPieceHeader;
    smcFreeLobSegFunc   sLobSegFunc;
    SChar             * sCurPtr;
    UInt                sState =0;

    IDE_DASSERT( aTrans != NULL );
    IDE_DASSERT( aTableHeader != NULL );
    sLobSegFunc = ( SMI_TABLE_TYPE_IS_DISK( aTableHeader ) == ID_TRUE )
                  ? smcTable::freeLobSegFunc : smcTable::freeLobSegNullFunc;

    /* smcTable_freeColumns_malloc_Arr.tc */
    IDU_FIT_POINT("smcTable::freeColumns::malloc::Arr");
    IDE_TEST( iduMemMgr::malloc(
                  IDU_MEM_SM_SMC,
                  ID_SIZEOF(smOID) * getMaxColumnVarSlotCnt(aTableHeader->mColumnSize),
                  (void**) &(sOIDStack.mArr))
              != IDE_SUCCESS);
    sState =1;

    buildOIDStack(aTableHeader, &sOIDStack);

    for( i = (SInt)(sOIDStack.mLength -1); i  >=0 ; i--)
    {
        sCurOID =   sOIDStack.mArr[i];
        IDE_TEST( sLobSegFunc(aStatistics,
                              aTrans,
                              SMI_ID_TABLESPACE_SYSTEM_MEMORY_DIC,
                              sCurOID,
                              aTableHeader->mColumnSize,
                              SDR_MTX_LOGGING) != IDE_SUCCESS);

        sLsnNTA2 = smLayerCallback::getLstUndoNxtLSN( aTrans );
        if( (i -1)  >= 0)
        {
            /* ���� variable slot�� ���� slot next(mOID)�� null��
               �����ϴ� physical logging. */
            IDE_ASSERT( smmManager::getOIDPtr( 
                            SMI_ID_TABLESPACE_SYSTEM_MEMORY_DIC, 
                            sOIDStack.mArr[i-1],
                            (void**)&sPreVCPieceHeader )
                        == IDE_SUCCESS );

            sAftVCPieceHeader = *(sPreVCPieceHeader);

            sAftVCPieceHeader.nxtPieceOID = SM_NULL_OID;

            IDE_TEST( smrUpdate::updateVarRowHead(NULL, /* idvSQL* */
                                                  aTrans,
                                                  SMI_ID_TABLESPACE_SYSTEM_MEMORY_DIC,
                                                  sOIDStack.mArr[i-1],
                                                  sPreVCPieceHeader,
                                                  &sAftVCPieceHeader)
                      != IDE_SUCCESS);

            //���� variable slot�� ���� slot next(mOID)�� null�� ����.
            sPreVCPieceHeader->nxtPieceOID = SM_NULL_OID;

            //Dirty Page List�� ���� variable slot������ �߰�.
            IDE_TEST( smmDirtyPageMgr::insDirtyPage(
                          SMI_ID_TABLESPACE_SYSTEM_MEMORY_DIC,
                          SM_MAKE_PID(sOIDStack.mArr[i-1]))
                      != IDE_SUCCESS);
        }
        else
        {
            // table header�� mColumns �� ���� variable slot.
            /* ------------------------------------------------
             *  table header��  column ������ SM_NULL_OID�� ����
             * - SMR_LT_UPDATE�� SMR_SMC_TABLEHEADER_UPDATE_COLUMNS �α�
             * ----------------------------------------------*/
            IDE_TEST( smrUpdate::updateColumnsAtTableHead(NULL, /* idvSQL* */
                                                          aTrans,
                                                          SM_MAKE_PID(aTableHeader->mSelfOID),
                                                          SM_MAKE_OFFSET(aTableHeader->mSelfOID)
                                                          +SMP_SLOT_HEADER_SIZE,
                                                          &(aTableHeader->mColumns),
                                                          SM_NULL_OID,
                                                          0,
                                                          SM_VCDESC_MODE_OUT,
                                                          aTableHeader->mLobColumnCount,
                                                          0, /* aAfterLobColumnCount */
                                                          aTableHeader->mColumnCount,
                                                          aTableHeader->mColumnCount)
                      != IDE_SUCCESS );


            aTableHeader->mColumns.fstPieceOID  = SM_NULL_OID;
            aTableHeader->mColumns.length = 0;
            aTableHeader->mColumns.flag = SM_VCDESC_MODE_OUT;
        }//else

        IDE_ASSERT( smmManager::getOIDPtr( 
                        SMI_ID_TABLESPACE_SYSTEM_MEMORY_DIC, 
                        sCurOID,
                        (void**)&sCurPtr )
                    == IDE_SUCCESS );

        /* ------------------------------------------------
         * catalog table�� variable slot�� �����Ͽ� ��ȯ��.
         * ----------------------------------------------*/
        IDE_TEST( smpVarPageList::freeSlot(aTrans,
                                           SMI_ID_TABLESPACE_SYSTEM_MEMORY_DIC,
                                           SMC_CAT_TABLE->mVar.mMRDB,
                                           sCurOID,
                                           sCurPtr,
                                           &sLsnNTA2,
                                           SMP_TABLE_NORMAL)
              != IDE_SUCCESS );



    }//for i

    IDE_TEST( iduMemMgr::free(sOIDStack.mArr) != IDE_SUCCESS);


    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    {
        if( sState == 1)
        {
            IDE_ASSERT( iduMemMgr::free(sOIDStack.mArr) == IDE_SUCCESS);

        }//if
    }
    return IDE_FAILURE;

}

/***********************************************************************
 * Description :  PROJ-1362 QP - Large Record & Internal LOB ����.
 *                table�� �÷������� �ε��� ���� ���� Ǯ��.
 *  ������ variable slot���� ó�� variable ���� �������� slot�� �����ϱ�
 *  ���Ͽ� OID stack�� �����Ѵ�.
 ***********************************************************************/
void smcTable::buildOIDStack(smcTableHeader* aTableHeader,
                             smcOIDStack*    aOIDStack)
{

    smOID            sVCPieceOID;
    smVCPieceHeader *sVCPieceHdr;
    UInt             i;

    for( i=0, sVCPieceOID =  aTableHeader->mColumns.fstPieceOID;
         sVCPieceOID != SM_NULL_OID ; i++ )
    {
        ID_SERIAL_BEGIN(aOIDStack->mArr[i] = sVCPieceOID);

        // BUG-27735 : Peterson Algorithm
        ID_SERIAL_EXEC( IDE_ASSERT( smmManager::getOIDPtr( 
                            SMI_ID_TABLESPACE_SYSTEM_MEMORY_DIC, 
                            sVCPieceOID,
                            (void**)&sVCPieceHdr )
                        == IDE_SUCCESS ), 
            1 );

        ID_SERIAL_END(sVCPieceOID =  sVCPieceHdr->nxtPieceOID);
    }//for

    aOIDStack->mLength = i;
}

IDE_RC smcTable::freeLobSegNullFunc(idvSQL*           /*aStatistics*/,
                                    void*             /*aTrans*/,
                                    scSpaceID         /*aSpaceID*/,
                                    smOID             /*aColSlotOID*/,
                                    UInt              /*aColumnSize */,
                                    sdrMtxLogMode     /*aLoggingMode*/)
{
    return IDE_SUCCESS;

}


IDE_RC smcTable::freeLobSegFunc(idvSQL*           aStatistics,
                                void*             aTrans,
                                scSpaceID         aColSlotSpaceID,
                                smOID             aColSlotOID,
                                UInt              aColumnSize,
                                sdrMtxLogMode     aLoggingMode)
{
    smOID            sColOID;
    smVCPieceHeader* sVCPieceHeader;
    smiColumn*       sLobCol;
    UChar*           sLobColumn;
    UInt             i;
    idBool           sInvalidTBS=ID_FALSE;

    IDE_DASSERT( aColSlotSpaceID == SMI_ID_TABLESPACE_SYSTEM_MEMORY_DIC );

    IDE_ASSERT( smmManager::getOIDPtr( 
                    aColSlotSpaceID,
                    aColSlotOID,
                    (void**)&sVCPieceHeader )
                == IDE_SUCCESS );

    sLobColumn = (UChar*)(sVCPieceHeader+1);
    sColOID = aColSlotOID+ ID_SIZEOF(smVCPieceHeader);

    for( i = 0 ;  i <  sVCPieceHeader->length; i+=  aColumnSize)
    {
        sLobCol = (smiColumn*)sLobColumn;

        //�ٸ� TBS�� lob column�� ������� �� tbs�� valid���� �˻��ؾ߸���.
        sInvalidTBS = sctTableSpaceMgr::hasState( sLobCol->colSpace,
                                                  SCT_SS_INVALID_DISK_TBS );
        if( sInvalidTBS == ID_TRUE )
        {
            continue;
        }

        // XXX ���� colSeg�� NULL�� �Ǵ°�? refine �� colSeg�� NULL�̾�����
        // undo�� �� ���� �����ȴ�.
        if( ( SMI_IS_LOB_COLUMN( sLobCol->flag ) == ID_TRUE )  &&
            ( SC_GRID_IS_NOT_NULL( sLobCol->colSeg ) ) )
        {
            IDE_TEST( sdpSegment::freeLobSeg(aStatistics,
                                             aTrans,
                                             sColOID,
                                             sLobCol,
                                             aLoggingMode)
                      != IDE_SUCCESS);
        }

        sColOID    += aColumnSize;
        sLobColumn += aColumnSize;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}



/***********************************************************************
 * Description : table�� Lockȹ��
 ***********************************************************************/
IDE_RC smcTable::lockTable( void                * aTrans,
                            smcTableHeader      * aHeader,
                            sctTBSLockValidOpt    aTBSLvOpt,
                            UInt                  aFlag )
{
    idBool           sLocked;
    UInt             sLockFlag;
    iduMutex       * sMutex;

    /* ------------------------------------------------
     * [1] Drop�ϰ��� �ϴ� Table�� ���Ͽ� X lock ��û
     * ----------------------------------------------*/
    sLockFlag = aFlag & SMC_LOCK_MASK;

    if(sLockFlag == SMC_LOCK_TABLE)
    {

        // PRJ-1548 User Memory Tablespace
        // ���̺��� ���̺����̽��鿡 ���Ͽ� INTENTION ����� ȹ���Ѵ�.
        IDE_TEST( sctTableSpaceMgr::lockAndValidateTBS(
                      (void*)aTrans,                  /* smxTrans * */
                      getTableSpaceID((void*)aHeader),/* smcTableHeader* */
                      aTBSLvOpt,          /* ���̺����̽� Validation �ɼ� */
                      ID_TRUE,            /* intent lock  ���� */
                      ID_TRUE,            /* exclusive lock ���� */
                      sctTableSpaceMgr::getDDLLockTimeOut((smxTrans*)aTrans))
                  != IDE_SUCCESS );

        IDE_TEST( smLayerCallback::lockTableModeXAndCheckLocked( aTrans,
                                                                 SMC_TABLE_LOCK( aHeader ),
                                                                 &sLocked )
                  != IDE_SUCCESS );

        IDE_TEST_RAISE( sLocked == ID_FALSE, already_locked_error );
    }
    else
    {
        if(sLockFlag == SMC_LOCK_MUTEX)
        {
            sMutex = smLayerCallback::getMutexOfLockItem( SMC_TABLE_LOCK( aHeader ) ) ;
            IDE_TEST_RAISE( sMutex->lock( NULL ) != IDE_SUCCESS,
                            mutex_lock_error );

            smLayerCallback::addMutexToTrans( aTrans,
                                              (void*)sMutex );
        }
        else
        {
            IDE_ASSERT(0);
        }
    }
    return IDE_SUCCESS;

    IDE_EXCEPTION( mutex_lock_error );
    {
        IDE_SET(ideSetErrorCode (smERR_FATAL_ThrMutexLock));
    }
    IDE_EXCEPTION(already_locked_error);
    {
        IDE_SET(ideSetErrorCode (smERR_ABORT_Already_Locked));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : table�� ��ȿ�� �˻�

   PRJ-1548 User Memory TableSpace ���䵵��
   Validate Table ���� ������ ���� Lock�� ȹ���Ѵ�.
   [1] Table�� TableSpace�� ���� IX
   [2] Table�� ���� X
   [3] Table�� Index, Lob Column TableSpace�� ���� IX
   Table�� ������ Table�� TableSpace�̸� �׿� ���� IX��
   ȹ���Ѵ�.

 ***********************************************************************/
IDE_RC smcTable::validateTable( void                * aTrans,
                                smcTableHeader      * aHeader,
                                sctTBSLockValidOpt    aTBSLvOpt,
                                UInt                  aFlag )
{
    /* ------------------------------------------------
     * [1] TableSpace�� ���Ͽ� IX lock ��û
     * [2] Table�� ���Ͽ� X lock ��û
     * ----------------------------------------------*/
    IDE_TEST( lockTable( aTrans,
                         aHeader,
                         aTBSLvOpt,
                         aFlag ) != IDE_SUCCESS );

    /* Table Space�� ���ؼ� DDL�� �߻��ߴٴ°��� ǥ��*/
    IDE_TEST( smLayerCallback::addOID( aTrans,
                                       aHeader->mSelfOID,
                                       aHeader->mSelfOID,
                                       aHeader->mSpaceID,
                                       SM_OID_DDL_TABLE )
              != IDE_SUCCESS );

    /* ------------------------------------------------
     * [3] validation
     * ���ŵ� table���� Ȯ���Ѵ�.
     * ----------------------------------------------*/
    IDE_TEST_RAISE( SMP_SLOT_IS_DROP( ((smpSlotHeader *)aHeader-1) ),
                    table_not_found_error );

    if ( aFlag == SMC_LOCK_TABLE )
    {
        // fix BUG-17212
        // ���̺��� Index, Lob �÷� ���� ���̺����̽��鿡 ���Ͽ�
        // INTENTION ����� ȹ���Ѵ�.
        /* ------------------------------------------------
         * [1] TableSpace�� ���Ͽ� IX lock ��û
         * ----------------------------------------------*/
        IDE_TEST( sctTableSpaceMgr::lockAndValidateRelTBSs(
                  (void*)aTrans,    /* smxTrans* */
                  (void*)aHeader,   /* smcTableHeader */
                  aTBSLvOpt,        /* TBS Lock Validation Option */
                  ID_TRUE,          /* intent lock  ���� */
                  ID_TRUE,          /* exclusive lock ���� */
                  sctTableSpaceMgr::getDDLLockTimeOut((smxTrans*)aTrans)) /* wait lock timeout */
                != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION(table_not_found_error);
    {
        IDE_SET(ideSetErrorCode (smERR_ABORT_Table_Not_Found));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : table ���� (����)
 * ���̺� Ÿ�Կ� ���� pending operation�� ����Ͽ��� �Ѵ�.
 * memory table�� ��쿡�� ager�� ���� aging ����� smcTable::dropTablePending��
 * ȣ�� �Ǹ�, disk table�� ��쿡�� TSS�� pending operation�� ���� table OID
 * �����Ͽ� disk G.C�� ���� ó���ϵ��� �Ѵ�.
 *
 * - 2nd. code design
 *   + table validation ó��
 *   + table header�� drop flag�� ���� �� �α�
 *     : SMR_LT_UPDATE�� SMR_SMC_PERS_SET_FIX_ROW_DROP_FLAG
 *     : drop flag�� SMP_SLOT_DROP_TRUE ����
 *     : dirtypage ���
 *   + table Ÿ�Կ� ���� drop pending ���� ���(temporary table�� ����)
 *     if (memory table)
 *       : ager�� ���� ó��
 *       - SM_OID_DROP_TABLE ǥ���Ͽ� Ʈ����� commit�ÿ�
 *         slot header�� delete bit�� �����ϰ�, ager�� ����
 *         ���� drop �ϰ� �Ѵ�.
 *       - ���� table�� unpin ���¶��, ager�� ���ؼ� table
 *          backup file�� �����ϵ��� �Ѵ�.
 *     else if (disk table)
 *       : disk G.C�� ���� ó��
 *       - mtx ���� (SMR_LT_DRDB, SMR_OP_NULL)
 *       - Ʈ��������κ��� tss RID�� ��´�.
 *       - �ش� tss RID�� ���Ե� page�� X_LATCH ���� fix ��Ų��.
 *       - tss�� pending flag(SDR_4BYTES)�� ���� �� �α�
 *       - ������ table OID�� ���� �� �α� (SDR_8BYTES)
 *       - mtx commit
 *     endif
 *
 * - RECOVERY
 *
 *  # table ���� (* disk-table ����)
 *
 *  (0) (1)            (2)            (3)      (4)
 *   |---|--------------|--------------|--------|--------> time
 *       |              |              |        |
 *     *alloc/init      set           set      commit
 *     tss              drop-flag     pending
 *     (R)              on tbl-slot   flag
 *                      (R/U)         to tss
 *                                    (R)
 *
 *  # Disk-Table Recovery Issue
 *
 *  - (1)���� �α��Ͽ��ٸ�
 *    + smxTrans::abort �����Ѵ�.
 *      : �̶� �Ҵ�Ǿ��� tss�� ���� free�� �����Ѵ�.
 *
 *  - (2)���� �α��Ͽ��ٸ�
 *    + (2)�� ���� undo�� ó���Ѵ�.
 *    + smxTrans::abort �����Ѵ�.
 *
 *  - (3)���� �α��Ͽ��ٸ�
 *    + (2)�� ���� undo�� ó���Ѵ�.
 *    + smxTrans::abort �����Ѵ�.
 *
 *  - commit�Ǿ��ٸ� tx commit ��������
 *    tbl-hdr�� slot�� SCN�� delete bit�� �����Ѵ�.
 *    �� ��, disk GC�� ���ؼ� drop table pending ������
 *    �����Ѵ�. dropTablePending�� ������ ����. (b)(c)
 *    : �� �ڼ��� ������ dropTablePending�� ����
 *
 *      (a)    (b)                 (c)
 *       |------|-------------------|-----------> time
 *              |                   |
 *              update             free
 *              seg rID,           segment
 *              meta pageID        [NTA->(a)]
 *              in tbl-hdr         (NTA OP NULL)
 *              (R/U)
 *
 *      ��. (b)������ �α��ߴٸ� (b)�α׿� ���Ͽ� undo�� ����Ǹ� ������
 *          ���� ó���ȴ�.
 *          => (a)->(b)->(b)'->(a)'->(a)->(b)->(c)->...
 *             NTA            CLR   CLR
 *      ��. (c)���� mtx commit�� �Ǿ��ٸ�
 *          => (5)->(a)->(b)->(c)->(d)->..->(5)'->...
 *             NTA                          DUMMY_CLR
 *
 ***********************************************************************/
IDE_RC smcTable::dropTable( void               * aTrans,
                            const void         * aHeader,
                            sctTBSLockValidOpt   aTBSLvOpt,
                            UInt                 aFlag )
{
#ifdef DEBUG
    UInt              sTableType;
#endif
    UInt              sState  = 0;
    scPageID          sPageID = 0;
    smcTableHeader*   sHeader;

    sHeader = (smcTableHeader*)aHeader;

#ifdef DEBUG
    sTableType = SMI_GET_TABLE_TYPE( sHeader );
#endif
    IDE_DASSERT( sTableType == SMI_TABLE_MEMORY ||
                 sTableType == SMI_TABLE_META   ||
                 sTableType == SMI_TABLE_DISK   ||
                 sTableType == SMI_TABLE_VOLATILE ||
                 sTableType == SMI_TABLE_REMOTE );

    // [1] ��ȿ�� table���� Ȯ���ϰ� drop�� �� �ִ� �غ� �Ѵ�.
    IDE_TEST( validateTable(aTrans,
                            sHeader,
                            aTBSLvOpt,
                            aFlag) != IDE_SUCCESS );

    /* ------------------------------------------------
     * [2] table Header���� m_drop_flag�� �����Ѵ�.
     * - Update Type: SMR_SMP_PERS_PRE_DELETE_ROW
     * - table header�� ����� fixed slot�� delete flag ����
     * ----------------------------------------------*/
    sPageID = SM_MAKE_PID(sHeader->mSelfOID);
    sState  = 1;

    IDE_TEST( smrUpdate::setDropFlagAtFixedRow(
                                    NULL, /* idvSQL* */
                                    aTrans,
                                    SMI_ID_TABLESPACE_SYSTEM_MEMORY_DIC,
                                    sHeader->mSelfOID,
                                    ID_TRUE)
              != IDE_SUCCESS);

    //IDU_FIT_POINT("smcTable::dropTable::wait1");
    IDU_FIT_POINT( "1.PROJ-1552@smcTable::dropTable" );

    SMP_SLOT_SET_DROP( (smpSlotHeader *)sHeader-1 );

    /* ------------------------------------------------
     * [3] Drop�ϰ��� �ϴ� Table�� Pending Operation List�� �߰��Ѵ�.
     * memory table�� ager�� ����ϰ�, disk table�� tss�� ����Ѵ�.
     * ----------------------------------------------*/
    IDE_TEST( smLayerCallback::addOID( aTrans,
                                       sHeader->mSelfOID,
                                       ID_UINT_MAX,
                                       SMI_ID_TABLESPACE_SYSTEM_MEMORY_DIC,
                                       SM_OID_DROP_TABLE )
              != IDE_SUCCESS );

    sState = 0;
    IDE_TEST( smmDirtyPageMgr::insDirtyPage( SMI_ID_TABLESPACE_SYSTEM_MEMORY_DIC, 
                                             sPageID ) 
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if ( sState != 0 )
    {
            IDE_PUSH();
            (void)smmDirtyPageMgr::insDirtyPage( SMI_ID_TABLESPACE_SYSTEM_MEMORY_DIC, 
                                                 sPageID );
            IDE_POP();
    }

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : 
 * 1.alter table �������� exception�߻����� ���� undo �ϴ� ��ƾ�̴�
 *   disk table������ ����� �ʿ����.
 *   ������� �߿� �߻��� abort�� ���� Logical undo�� �����Ѵ�.
 * 2.������������� Table�� Plan �籸���� ���ؼ� ����Ѵ�.
 ***********************************************************************/
void  smcTable::changeTableSCNForRebuild( smOID aTableOID )
{
    smSCN            sLstSystemSCN;
    smpSlotHeader  * sSlotHeader;

    IDE_ASSERT( smmManager::getOIDPtr( SMI_ID_TABLESPACE_SYSTEM_MEMORY_DIC, 
                                       aTableOID,
                                       (void**)&sSlotHeader ) 
                == IDE_SUCCESS );

    sLstSystemSCN = smmDatabase::mLstSystemSCN;
    SM_SET_SCN( &(sSlotHeader->mCreateSCN), &sLstSystemSCN );

    return;
}

/***********************************************************************
 * Description : for PR-4360 ALTER TABLE
 *
 * - table�� �Ҵ�� fixed/var pagelist�� ��� free�Ͽ�
 *   system�� �޸� ��ȯ�Ѵ�.
 * - memory table�� alter table ���ó������ ���Ǵ� �Լ��ν�
 *   disk table�� ���Ͽ� �������� �ʾƵ� �Ǵ� �Լ�
 *
 * BUG-30457  memory volatile ���̺� ���� alter��
 * ���� �������� rollback�� ������ ��Ȳ�̸� ������
 * �����մϴ�.
 *
 * �� ������ AlterTable �� ���� Table Backup �� ȣ��ȴ�.
 * ���� �� ������ Undo�ÿ���, �� Table Restore�� �����
 * Page���� DB�� ������ �� �ֵ���, �� Table(aDstHeader)��
 * Logging�Ѵ�. ==> BUG-34438������ smiTable::backupTableForAlterTable()��
 * �̵���
 ***********************************************************************/

IDE_RC smcTable::dropTablePageListPending( void           * aTrans,
                                           smcTableHeader * aHeader,
                                           idBool           aUndo )
{
    smLSN               sLsnNTA;
    UInt                sState = 0;

    IDE_ASSERT( SMI_TABLE_TYPE_IS_DISK( aHeader ) == ID_FALSE );

    // for NTA
    sLsnNTA = smLayerCallback::getLstUndoNxtLSN( aTrans );
    sState = 1;

    /* PROJ-1594 Volatile TBS */
    /* volatile table���� memory table���� �Ǵ��Ͽ� ������ �б��Ѵ�. */
    if( SMI_TABLE_TYPE_IS_VOLATILE( aHeader ) == ID_TRUE )
    {
        IDE_TEST( svpFixedPageList::freePageListToDB( aTrans,
                                                      aHeader->mSpaceID,
                                                      aHeader->mSelfOID,
                                                      &(aHeader->mFixed.mVRDB) )
                  != IDE_SUCCESS );

        (aHeader->mFixed.mVRDB.mRuntimeEntry)->mInsRecCnt = 0;
        (aHeader->mFixed.mVRDB.mRuntimeEntry)->mDelRecCnt = 0;

        IDE_TEST( svpVarPageList::freePageListToDB( aTrans,
                                                    aHeader->mSpaceID,
                                                    aHeader->mSelfOID,
                                                    aHeader->mVar.mVRDB )
                  != IDE_SUCCESS );
    }
    else
    {
        IDE_TEST( smpFixedPageList::freePageListToDB( aTrans,
                                                      aHeader->mSpaceID,
                                                      aHeader->mSelfOID,
                                                      &(aHeader->mFixed.mMRDB) )
                  != IDE_SUCCESS );

        (aHeader->mFixed.mMRDB.mRuntimeEntry)->mInsRecCnt = 0;
        (aHeader->mFixed.mMRDB.mRuntimeEntry)->mDelRecCnt = 0;

        IDE_TEST( smpVarPageList::freePageListToDB( aTrans,
                                                    aHeader->mSpaceID,
                                                    aHeader->mSelfOID,
                                                    aHeader->mVar.mMRDB )
                  != IDE_SUCCESS );
    }
	
    IDU_FIT_POINT( "1.BUG-34438@smiTable::dropTablePageListPending" );

    IDE_TEST( smrLogMgr::writeNTALogRec(NULL, /* idvSQL* */
                                        aTrans,
                                        &sLsnNTA,
                                        SMR_OP_NULL)
              != IDE_SUCCESS );


    sState = 0;
    /* BUG-30871 When excuting ALTER TABLE in MRDB, the Private Page Lists of
       new and old table are registered twice. */
    /* AllocPageList �� FreePageList�� �����, Transactino��
     * PrivatePageList�� ����� �մϴ�. */
    IDE_TEST( smLayerCallback::dropMemAndVolPrivatePageList( aTrans, aHeader )
              != IDE_SUCCESS );

    if ( aUndo == ID_FALSE )
    {
        IDE_TEST( smLayerCallback::dropIndexes( aHeader ) != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if(sState != 0)
    {
        if(ideIsAbort() == IDE_SUCCESS)
        {
            IDE_PUSH();
            (void)smrRecoveryMgr::undoTrans(NULL, /* idvSQL* */
                                            aTrans,
                                            &sLsnNTA);
            IDE_POP();
        }
    }

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : max rows�� �����ϰų�, column�� default value�� ����
 ***********************************************************************/
IDE_RC smcTable::modifyTableInfo( void                 * aTrans,
                                  smcTableHeader       * aHeader,
                                  const smiColumnList  * aColumns,
                                  UInt                   aColumnSize,
                                  const void           * aInfo,
                                  UInt                   aInfoSize,
                                  UInt                   aFlag,
                                  ULong                  aMaxRow,
                                  UInt                   aParallelDegree,
                                  sctTBSLockValidOpt     aTBSLvOpt,
                                  idBool                 aIsInitRowTemplate )
{
    ULong             sRecordCount = 0;
    UInt              sState = 0;
    scPageID          sHeaderPageID = 0;
    UInt              sTableFlag;
    UInt              sColumnCount = 0;
    UInt              sFixedRowSize;
    UInt              sBfrColCnt;
    UInt              i;
    smiColumn       * sColumn;
    smOID             sLobColOID;
    smnIndexHeader  * sIndexHeader;
    smnIndexModule  * sIndexModule;

    if(aMaxRow != ID_ULONG_MAX && aMaxRow != 0)
    {
        IDE_TEST(getRecordCount(aHeader, &sRecordCount)
                 != IDE_SUCCESS);

        IDE_TEST_RAISE(sRecordCount > aMaxRow, err_invalid_maxrow);
    }

    // DDL�� �̹Ƿ� Table Validation�� �����Ѵ�.
    IDE_TEST( validateTable( aTrans,
                             aHeader,
                             aTBSLvOpt,
                             SMC_LOCK_TABLE )
              != IDE_SUCCESS );

    if( aColumns != NULL )
    {
        /* Column List�� Valid���� �˻��ϰ� Column�� ������ Fixed row�� ũ��
           �� ���Ѵ�.*/
        IDE_TEST( validateAndGetColCntAndFixedRowSize(
                      SMI_GET_TABLE_TYPE( aHeader ),
                      aColumns,
                      &sColumnCount,
                      &sFixedRowSize)
                  != IDE_SUCCESS );

        sBfrColCnt = aHeader->mColumnCount;

        /* �� Table�� Column�� ������ Variable������ �����Ѵ�. */
        IDE_TEST( setColumnAtTableHeader( aTrans,
                                          aHeader,
                                          aColumns,
                                          sColumnCount,
                                          aColumnSize )
                  != IDE_SUCCESS );

        if ( SMI_TABLE_TYPE_IS_DISK( aHeader ) == ID_TRUE )
        {
            /* BUG-28356 [QP]�ǽð� add column ���� ���ǿ���
             *           lob column�� ���� ���� ���� �����ؾ� ��
             * �ǽð� Add Disk LOB Column �� Lob Segment�� ������ �־�� �մϴ�.
             * ���� Column�� ���Ŀ� ���� �߰��� Column���� ��ȸ�ϸ鼭
             * LOB Column�� �����ϸ� LOB Segment�� �����մϴ�. */
            if ( aHeader->mLobColumnCount != 0  )
            {
                for ( i = sBfrColCnt ; i < aHeader->mColumnCount ; i++ )
                {
                    sColumn = getColumnAndOID( aHeader,
                                               i, // Column Idx
                                               &sLobColOID );
 
                    if ( (sColumn->flag & SMI_COLUMN_TYPE_MASK) == SMI_COLUMN_TYPE_LOB )
                    {
                        SC_MAKE_NULL_GRID( sColumn->colSeg );
                        IDE_TEST( sdpSegment::allocLobSeg4Entry(
                                      smxTrans::getStatistics( aTrans ),
                                      aTrans,
                                      sColumn,
                                      sLobColOID,
                                      aHeader->mSelfOID,
                                      SDR_MTX_LOGGING ) != IDE_SUCCESS );
                    }
                }
            }

            /* BUG-31949 [qp-ddl-dcl-execute] failure of index visibility check
             * after abort real-time DDL in partitioned table. */
            for( i = 0 ; i < smcTable::getIndexCount( aHeader ) ; i ++ )
            {
                sIndexHeader =
                    (smnIndexHeader*) smcTable::getTableIndex( aHeader, i );

                sIndexModule = (smnIndexModule*)sIndexHeader->mModule;
                IDE_TEST( sIndexModule->mRebuildIndexColumn(
                            sIndexHeader,
                            aHeader,
                            sIndexHeader->mHeader )
                    != IDE_SUCCESS );
            }

            if ( aIsInitRowTemplate == ID_TRUE )
            {
                /* PROJ-2399 Row Template
                 * ������Ʈ�� column������ �������� RowTemplate�� �籸�� �Ѵ�. */
                IDE_TEST( destroyRowTemplate( aHeader )!= IDE_SUCCESS );
                IDE_TEST( initRowTemplate( NULL,  /* aStatistics */
                                           aHeader,
                                           NULL ) /* aActionArg */
                          != IDE_SUCCESS );
            }
        }
    }
    else
    {
        aColumnSize = aHeader->mColumnSize;
    }



    if( aInfo != NULL )
    {
        /* ���ο� Table�� Info ������ ���Ͽ� ���� �����Ѵ� */
        IDE_TEST(setInfoAtTableHeader(aTrans, aHeader, aInfo, aInfoSize)
                 != IDE_SUCCESS);
    }

    sHeaderPageID = SM_MAKE_PID(aHeader->mSelfOID);
    sState = 1;

    if( aFlag == SMI_TABLE_FLAG_UNCHANGE )
    {
        sTableFlag = aHeader->mFlag;
    }
    else
    {
        sTableFlag = aFlag;
    }

    if(aMaxRow == 0)
    {
        aMaxRow = aHeader->mMaxRow;
    }

    if ( aParallelDegree == 0 )
    {
        // Parallel Degree ������ �ȵ�
        // ������ Parallel Degree�� ������ ��
        aParallelDegree = aHeader->mParallelDegree;
    }

    IDE_TEST( smrUpdate::updateAllAtTableHead(NULL, /* idvSQL* */
                                              aTrans,
                                              aHeader,
                                              aColumnSize,
                                              &(aHeader->mColumns),
                                              &(aHeader->mInfo),
                                              sTableFlag,
                                              aMaxRow,
                                              aParallelDegree)
              != IDE_SUCCESS );

    IDU_FIT_POINT( "1.PROJ-1552@smcTable::modifyTableInfo" );

    aHeader->mFlag           = sTableFlag;
    aHeader->mMaxRow         = aMaxRow;
    aHeader->mParallelDegree = aParallelDegree;

    IDE_TEST( smmDirtyPageMgr::insDirtyPage(
                  SMI_ID_TABLESPACE_SYSTEM_MEMORY_DIC, sHeaderPageID)
              != IDE_SUCCESS );

    IDE_TEST( smLayerCallback::addOID( aTrans,
                                       SMC_CAT_TABLE->mSelfOID,
                                       aHeader->mSelfOID,
                                       SMI_ID_TABLESPACE_SYSTEM_MEMORY_DIC,
                                       SM_OID_CHANGED_FIXED_SLOT )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION(err_invalid_maxrow);
    {
        IDE_SET(ideSetErrorCode (smERR_ABORT_Invalid_MaxRows));
    }
    IDE_EXCEPTION_END;

    IDE_PUSH();
    if( sState != 0 )
    {
        IDE_ASSERT( smmDirtyPageMgr::insDirtyPage(
                        SMI_ID_TABLESPACE_SYSTEM_MEMORY_DIC,
                        sHeaderPageID)
                    == IDE_SUCCESS );
    }
    IDE_POP();

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : index ����(����)
 *
 * index ���������� disk/memory table�� ������� ����
 * index �����ڿ� ���� ó���Ǳ� ������ collection layer
 * ������ ����� ���� ����.
 * - 2nd. code design
 *   + table validation ó��
 *   + index Ÿ�Կ� ���� indexheader �ʱ�ȭ
 *     : �����ε����������� initIndexHeader���� segment�� �Ҵ�
 *     : index segment�� �����Ǵ� tablespace id ��� ����
 *   + table header�� index ������ �����ϱ� ���� variable
 *     slot �Ҵ�
 *     : table�� ���� Index�� 1�� �̻� �����ϴ� ���
 *       ���� ���� �Ҵ���� variable slot�� �����ϰ�
 *       ���� �߰��ϴ� index ������ �߰�
 *     : �߰��Ϸ��� index�� primary key���, slot��
 *       �� ó���� �����ϰ�, ���� index�� �� �ڷ� ����
 *       �׷��� �ʴٸ� ���� index�� �� ó���� ���ο� index��
 *       �� �ڷ� �����Ѵ�.
 *   + dirty page ���
 *   + ���� index ������ ������ old variable slot�� �����Ѵٸ�,
 *     SMP_DELETE_SLOT�� ������ ��, Ʈ����� commit �Ŀ�
 *     ager�� ������ �� �ֵ��� SM_OID_OLD_VARIABLE_SLOT ǥ��
 *   + ���ο� index ������ ������ new variable slot�� ���Ͽ�
 *     Ʈ����� rollback �Ŀ� ager�� ������ �� �ֵ���
 *     SM_OID_OLD_VARIABLE_SLOT ǥ��
 *   + ���ο� index ������ ������ variable slot��
 *     table header�� ���� �� �α�
 *     : table header�� ���Ͽ� acquire sync
 *     : ���ο� index ����
 *     : table header�� ���Ͽ� release sync
 *   + index ������ ���� NTA �α� - SMR_OP_CREATE_INDEX
 *   + table header ���濡 ���� SM_OID_CHANGED_FIXED_SLOT ǥ��
 *
 * - RECOVERY
 *
 *  # index ������ (* disk-index����)
 *
 *  (0) (1)        (2)         {(3)       (4)}       (5)      (6)      (7)
 *   |---|----------|-----------|---------|-----------|--------|--------|--> time
 *       |          |           |         |           |        |        |
 *     alloc        update      update    *alloc      set     update    NTA
 *     v-slot       new index,  seg rID    segment    delete  index     OP_CREATE
 *     from         header +    on        [NTA->(2)]  flag    oID       INDEX
 *     cat-tbl      ���� index  tbl-hdr               old     on        [NTA->(0)]
 *    [NTA->(0)]    header      (R/U)                 index   tbl-hdr
 *                  on v-slot                         v-slot  (R/U)
 *                  oID                               (R/U)
 *                  (R/U)
 *
 *  # Disk-index Recovery Issue
 *
 *  - (1)���� �α��Ͽ��ٸ�
 *    + (1) -> logical undo
 *      : v-slot�� delete flag�� �����Ѵ�.
 *    + smxTrans::abort �����Ѵ�.
 *
 *  - (2)���� �α��Ͽ��ٸ�
 *    + (2) -> undo
 *      : ���� index OID�� �����Ѵ�.
 *    + (1) -> logical undo
 *    + smxTrans::abort
 *
 *  - (4)���� mtx commit �Ͽ��ٸ� (!!!!!!!!!!)
 *    + (4) -> logical undo
 *      : segment�� free�Ѵ�. ((3)':SDR_OP_NULL) [NTA->(2)]
 *        ���� free���� ���Ͽ��ٸ�, �ٽ� logical undo�� �����ϸ� �ȴ�.
 *        free segment�� mtx commit �Ͽ��ٸ�, [NTA->(2)]���� ���� undo����
 *        ���� skip�ϰ� �Ѿ��.
 *    + (2) -> undo
 *    + ...
 *    + smxTrans::abort�� �����Ѵ�.
 *
 *  - (6)���� �α��Ͽ��ٸ�
 *    + (6) -> undo
 *    + (5) -> undo
 *    + (4) -> logical undo
 *    + ...
 *    + smxTrans::abort�� �����Ѵ�.
 *
 *  - (7)���� �α��Ͽ��ٸ�
 *    + (7) -> logical undo
 *      : dropIndexByAbortOID�� �����ϸ� ������ ����.
 *
 *      (7)       {(a)         (b)}        (c)         (d)      (e)       (7)'
 *       |----------|-----------|-----------|-----------|--------|--------|--> time
 *                  |           |           |           |        |        |
 *                 update      free         set         update  set      DUMMY
 *                 seg rID     segment      delete flag index   delete   CLR
 *                 on          [NTA->(7)]   old index   oID     flag
 *                 tbl-hdr                  v-slot      on      new index
 *                 (R/U)                    (R/U)       tbl-hdr v-slot
 *                                                      (R/U)   [used]
 *                                                              (R/U)
 *    + smxTrans::abort�� �����Ѵ�.
 *
 ***********************************************************************/
IDE_RC smcTable::createIndex( idvSQL               *aStatistics,
                              void                * aTrans,
                              scSpaceID             aSpaceID,
                              smcTableHeader      * aHeader,
                              SChar               * aName,
                              UInt                  aID,
                              UInt                  aType,
                              const smiColumnList * aColumns,
                              UInt                  aFlag,
                              UInt                  aBuildFlag,
                              smiSegAttr          * aSegAttr,
                              smiSegStorageAttr   * aSegStorageAttr,
                              ULong                 aDirectKeyMaxSize,
                              void               ** aIndex )
{

    SChar           * sOldIndexRowPtr = NULL;
    SChar           * sNewIndexRowPtr = NULL;
    SChar           * sDest;
    smOID             sNewIndexOID;
    smOID             sOldIndexOID;
    smiValue          sVal;
    void            * sIndexHeader;
    UInt              sState  = 0;
    scPageID          sPageID = 0;
    smLSN             sLsnNTA;
    smOID             sIndexOID;
    scOffset          sOffset;
    UInt              sIndexHeaderSize;
    UInt              sMaxIndexCount;
    UInt              sTableLatchState = 0;
    UInt              sAllocState = 0;
    UInt              sTableType;
    UInt              sIdx;
    UInt              sPieceType;
    SChar           * sNewIndexPtr;

    /* ------------------------------------------------
     * [1] table validation
     * ----------------------------------------------*/
    IDE_TEST( validateTable( aTrans,
                             aHeader,
                             SCT_VAL_DDL_DML ) // ���̺����̽� Validation �ɼ�
              != IDE_SUCCESS );

    sMaxIndexCount = smLayerCallback::getMaxIndexCnt();

    /* ----------------------------
     * [2] validation for maximum index count
     * ---------------------------*/
    //max index count test
    IDE_TEST_RAISE( (getIndexCount(aHeader) +1) > sMaxIndexCount,
                    maximum_index_count_error);

    // for NTA
    sLsnNTA = smLayerCallback::getLstUndoNxtLSN( aTrans );

    // ���� index �����ڸ� ���� indexheader�� ũ�⸦ ����
    sIndexHeaderSize = smLayerCallback::getSizeOfIndexHeader();

    /* ------------------------------------------------
     * [3] ���� ������Ű�� Index�� ������ ����
     * indexheader �ʱ�ȭ
     * disk index�� memory index�� ��� ������
     * index �����ڸ� ����ϹǷ� �Ʒ��� ���� ó���Ͽ���
     * Ÿ�Կ� ���� indexheader�� �ʱ�ȭ�� �� �ִ�.
     * ----------------------------------------------*/
    /* smcTable_createIndex_malloc_IndexHeader.tc */
    IDU_FIT_POINT("smcTable::createIndex::malloc::IndexHeader");
    IDE_TEST( iduMemMgr::malloc(IDU_MEM_SM_SMC,
                                sIndexHeaderSize,
                                (void**)&sIndexHeader,
                                IDU_MEM_FORCE)
              != IDE_SUCCESS );
    sAllocState = 1;

    /* ------------------------------------------------
     * FOR A4 : disk index�� �����ε����������� initIndexHeader����
     * segment�� �Ҵ�޴´�.
     * index segment�� table�� �ٸ� tablespace�� ������ �� �����Ƿ�
     * tablespace id�� ����Ҽ� �ִ�.
     * ----------------------------------------------*/
    smLayerCallback::initIndexHeader( sIndexHeader,
                                      aHeader->mSelfOID,
                                      aName,
                                      aID,
                                      aType,
                                      aFlag,
                                      aColumns,
                                      aSegAttr,
                                      aSegStorageAttr,
                                      aDirectKeyMaxSize );
    // PROJ-1362 QP-Large Record & Internal LOB����.
    // index ���� ���� Ǯ�� part.
    // ���ο� �ε��� ������ ������ ��� variable slot�� ã�´�.
    IDE_TEST( findIndexVarSlot2Insert(aHeader,
                                      aFlag,
                                      sIndexHeaderSize,
                                      &sIdx)
              != IDE_SUCCESS);

    /* ------------------------------------------------
     * [4] Table�� ���� Index�� 0�� �����ϴ� ���
     * ----------------------------------------------*/
    sVal.length = aHeader->mIndexes[sIdx].length + sIndexHeaderSize;

    /* ------------------------------------------------
     * [5] Table�� ���� Index�� 1�� �̻� �����ϴ� ���
     * ���� ���� ���ο� Version�� copy�ϰ� ���� �߰��ϴ� Index ������ append!
     * ----------------------------------------------*/
    if(aHeader->mIndexes[sIdx].fstPieceOID != SM_NULL_OID)
    {
        IDE_ASSERT( smmManager::getOIDPtr( SMI_ID_TABLESPACE_SYSTEM_MEMORY_DIC, 
                                           aHeader->mIndexes[sIdx].fstPieceOID,
                                           (void**)&sOldIndexRowPtr )
                    == IDE_SUCCESS );
        sVal.value = sOldIndexRowPtr + ID_SIZEOF(smVCPieceHeader);
    }
    else
    {
        sVal.value = NULL;
    }

    sTableType = SMI_GET_TABLE_TYPE( aHeader );

    if( sTableType == SMI_TABLE_DISK )
    {
        sPieceType = SM_VCPIECE_TYPE_DISK_INDEX;
    }
    else
    {
        sPieceType = SM_VCPIECE_TYPE_MEMORY_INDEX;
    }

    IDE_TEST( smpVarPageList::allocSlot( aTrans,
                                         SMI_ID_TABLESPACE_SYSTEM_MEMORY_DIC,
                                         SMC_CAT_TABLE->mSelfOID,
                                         SMC_CAT_TABLE->mVar.mMRDB,
                                         sVal.length,
                                         SM_NULL_OID,
                                         &sNewIndexOID,
                                         (SChar**)&sNewIndexRowPtr,
                                         sPieceType )
              != IDE_SUCCESS );

    sDest = (SChar *)sNewIndexRowPtr + ID_SIZEOF(smVCPieceHeader);
    sNewIndexPtr = (SChar *)sNewIndexRowPtr + ID_SIZEOF(smVCPieceHeader);

    sPageID = SM_MAKE_PID(sNewIndexOID);
    sState  = 1;

    if( (aFlag & SMI_INDEX_TYPE_MASK) == SMI_INDEX_TYPE_PRIMARY_KEY )
    {

        IDE_ASSERT( sIdx == 0);

        // primary key�� table���� �Ѱ��� �����ϸ�,
        // qp�� primary key�� variable slot�� �Ǿտ� �ִٰ� �����ϰ� ����.

        /* ------------------------------------------------
         * ���ο� index�� primary��� variable slot��
         * �� ó���κп� index header�� �����Ѵ�.
         * - SMR_LT_UPDATE�� SMR_PHYSICAL �α�
         * ----------------------------------------------*/
        sOffset   = SM_MAKE_OFFSET(sNewIndexOID) + ID_SIZEOF(smVCPieceHeader);
        sIndexOID = SM_MAKE_OID(sPageID, sOffset);
        ((smnIndexHeader*)sIndexHeader)->mSelfOID = sIndexOID;

        idlOS::memcpy( sDest, sIndexHeader, sIndexHeaderSize );
        *aIndex = sDest;

        sDest += sIndexHeaderSize;

        if( aHeader->mIndexes[sIdx].length != 0 )
        {
            idlOS::memcpy( sDest,
                           sVal.value,
                           aHeader->mIndexes[sIdx].length );
        }
    }
    else
    {
        /* ------------------------------------------------
         * ���ο� index�� primary�� �ƴ϶�� variable slot��
         * ���� index������ ���� �����ϰ� �� �ںκп�  ���ο�
         * index�� header�� �����Ѵ�.
         * - SMR_LT_UPDATE�� SMR_PHYSICAL �α�
         * ----------------------------------------------*/
        sOffset   = SM_MAKE_OFFSET(sNewIndexOID) +
                    ID_SIZEOF(smVCPieceHeader);
        sIndexOID = SM_MAKE_OID(sPageID,
                                sOffset + aHeader->mIndexes[sIdx].length);
        ((smnIndexHeader*)sIndexHeader)->mSelfOID = sIndexOID;

        if(aHeader->mIndexes[sIdx].length != 0)
        {
            idlOS::memcpy( sDest,
                           sVal.value,
                           aHeader->mIndexes[sIdx].length );
        }

        sDest += aHeader->mIndexes[sIdx].length;
        idlOS::memcpy(sDest, sIndexHeader, sIndexHeaderSize );

        *aIndex = sDest;
    }

    // BUG-25313 : adjustIndexSelfOID() ��ġ ����.
    adjustIndexSelfOID( (UChar*)sNewIndexPtr,
                        sNewIndexOID + ID_SIZEOF(smVCPieceHeader),
                        sVal.length );

    // BUG-25313 : smrUpdate::physicalUpdate() ��ġ ����.
    IDE_TEST(smrUpdate::physicalUpdate(NULL, /* idvSQL* */
                                       aTrans,
                                       SMI_ID_TABLESPACE_SYSTEM_MEMORY_DIC,
                                       sPageID,
                                       sOffset,
                                       NULL,
                                       0,
                                       sNewIndexPtr,
                                       sVal.length,
                                       NULL,
                                       0)
             != IDE_SUCCESS);


    // ���ο� variable slot�� ���Ե� page�� dirty page�� ���
    IDE_TEST( smmDirtyPageMgr::insDirtyPage(
                  SMI_ID_TABLESPACE_SYSTEM_MEMORY_DIC,
                  sPageID) != IDE_SUCCESS );

    IDE_DASSERT( sTableType == SMI_TABLE_MEMORY ||
                 sTableType == SMI_TABLE_META   ||
                 sTableType == SMI_TABLE_DISK   ||
                 sTableType == SMI_TABLE_VOLATILE );

    /* BUG-33803 Disk index�� disable ���·� create �� ��� index segment��
     * �������� �ʴ´�. �� �� index�� enable ���·� ������ �� index segment��
     * �Բ� �����Ѵ�. */
    if( (sTableType == SMI_TABLE_DISK) &&
        ((aFlag & SMI_INDEX_USE_MASK) == SMI_INDEX_USE_ENABLE) )
    {
        // create index segment
        IDE_TEST( sdpSegment::allocIndexSeg4Entry(
                      aStatistics,
                      aTrans,
                      aSpaceID,
                      aHeader->mSelfOID,
                      sIndexOID,
                      aID,
                      aType,
                      aBuildFlag,  // BUG-17848
                      SDR_MTX_LOGGING,
                      aSegAttr,
                      aSegStorageAttr ) != IDE_SUCCESS );
    }

    sPageID = SM_MAKE_PID(aHeader->mSelfOID);

    if( aHeader->mIndexes[sIdx].fstPieceOID != SM_NULL_OID )
    {
        /* ------------------------------------------------
         * ���� index ������ ������ old variable slot�� �����Ѵٸ�,
         * SMP_DELETE_SLOT�� ������ ��, Ʈ����� commit �Ŀ�
         * ager�� ������ �� �ֵ��� SM_OID_OLD_VARIABLE_SLOT ǥ��
         * ----------------------------------------------*/
        IDE_TEST(smcRecord::setFreeFlagAtVCPieceHdr(aTrans,
                                                    aHeader->mSelfOID,
                                                    SMI_ID_TABLESPACE_SYSTEM_MEMORY_DIC,
                                                    aHeader->mIndexes[sIdx].fstPieceOID,
                                                    sOldIndexRowPtr,
                                                    SM_VCPIECE_FREE_OK)
                 != IDE_SUCCESS);

        IDU_FIT_POINT( "4.PROJ-1552@smcTable::createIndex" );

        IDE_TEST( smLayerCallback::addOID( aTrans,
                                           SMC_CAT_TABLE->mSelfOID,
                                           aHeader->mIndexes[sIdx].fstPieceOID,
                                           SMI_ID_TABLESPACE_SYSTEM_MEMORY_DIC,
                                           SM_OID_OLD_VARIABLE_SLOT )
                  != IDE_SUCCESS );
    }

    /* ------------------------------------------------
     * ���ο� index ������ ������ new variable slot�� ���Ͽ�
     * Ʈ����� rollback �Ŀ� ager�� ������ �� �ֵ���
     * SM_OID_NEW_VARIABLE_SLOT ǥ��
     * ----------------------------------------------*/
    IDE_TEST( smLayerCallback::addOID( aTrans,
                                       SMC_CAT_TABLE->mSelfOID,
                                       sNewIndexOID,
                                       SMI_ID_TABLESPACE_SYSTEM_MEMORY_DIC,
                                       SM_OID_NEW_VARIABLE_SLOT )
              != IDE_SUCCESS );

    /* ------------------------------------------------
     * [6] ���ο� index ������ ������ variable slot��
     * table header�� ���� �� �α�
     * - SMR_LT_UPDATE�� SMR_SMC_TABLEHEADER_UPDATE_INDEX
     * ----------------------------------------------*/
    IDE_TEST( smrUpdate::updateIndexAtTableHead(NULL, /* idvSQL* */
                                                aTrans,
                                                aHeader->mSelfOID,
                                                &(aHeader->mIndexes[sIdx]),
                                                sIdx,
                                                sNewIndexOID,
                                                sVal.length,
                                                SM_VCDESC_MODE_OUT)
              != IDE_SUCCESS );

    IDU_FIT_POINT( "5.PROJ-1552@smcTable::createIndex" );

    sOldIndexOID = aHeader->mIndexes[sIdx].fstPieceOID;

    IDE_TEST( smcTable::latchExclusive( aHeader ) != IDE_SUCCESS );
    sTableLatchState = 1;

    aHeader->mIndexes[sIdx].length = sVal.length;
    aHeader->mIndexes[sIdx].fstPieceOID = sNewIndexOID;
    aHeader->mIndexes[sIdx].flag = SM_VCDESC_MODE_OUT;

    // smnIndex �ʱ�ȭ : index ����(module�� createȣ��)
    if ( ( smLayerCallback::getFlagOfIndexHeader( *aIndex ) & SMI_INDEX_USE_MASK )
         == SMI_INDEX_USE_ENABLE )
    {
        IDE_TEST( smLayerCallback::initIndex( aStatistics,
                                              aHeader,
                                              *aIndex,
                                              aSegAttr,
                                              aSegStorageAttr,
                                              NULL,
                                              0 )
                  != IDE_SUCCESS );
    }

    sTableLatchState = 0;
    IDE_TEST( smcTable::unlatch( aHeader ) != IDE_SUCCESS );

    IDE_TEST( smmDirtyPageMgr::insDirtyPage(
                  SMI_ID_TABLESPACE_SYSTEM_MEMORY_DIC, sPageID)
              != IDE_SUCCESS );

    IDE_TEST( smrLogMgr::writeNTALogRec(NULL, /* idvSQL* */
                                        aTrans,
                                        &sLsnNTA,
                                        aHeader->mSpaceID,
                                        SMR_OP_CREATE_INDEX,
                                        sOldIndexOID,
                                        sIndexOID)
              != IDE_SUCCESS );

    IDU_FIT_POINT( "6.PROJ-1552@smcTable::createIndex" );

    IDE_TEST( smLayerCallback::addOID( aTrans,
                                       SMC_CAT_TABLE->mSelfOID,
                                       aHeader->mSelfOID,
                                       SMI_ID_TABLESPACE_SYSTEM_MEMORY_DIC,
                                       SM_OID_CHANGED_FIXED_SLOT )
              != IDE_SUCCESS );

    sAllocState = 0;
    IDE_TEST( iduMemMgr::free(sIndexHeader) != IDE_SUCCESS );

    IDU_FIT_POINT( "1.PROJ-1548@smcTable::createIndex" );

    IDU_FIT_POINT( "1.BUG-30612@smcTable::createIndex" );

    return IDE_SUCCESS;

    IDE_EXCEPTION(maximum_index_count_error);
    {
        IDE_SET(ideSetErrorCode (smERR_ABORT_Maximum_Index_Count,
                                 sMaxIndexCount));
    }
    IDE_EXCEPTION_END;

    if(sState != 0)
    {
        IDE_PUSH();
        (void)smmDirtyPageMgr::insDirtyPage(
            SMI_ID_TABLESPACE_SYSTEM_MEMORY_DIC, sPageID);
        IDE_POP();
    }

    if(sTableLatchState != 0)
    {
        IDE_PUSH();
        IDE_ASSERT( smcTable::unlatch( aHeader ) == IDE_SUCCESS );
        IDE_POP();
    }

    if(sAllocState != 0)
    {
        IDE_ASSERT( iduMemMgr::free(sIndexHeader) == IDE_SUCCESS );
    }

    return IDE_FAILURE;

}

/*
 * PROJ-1917 MVCC Renewal
 * Out-Place Update�� ���� Index Self OID ������
 */
void smcTable::adjustIndexSelfOID( UChar * aIndexHeaders,
                                   smOID   aStartIndexOID,
                                   UInt    aTotalIndexHeaderSize )
{
    UInt             i;
    smnIndexHeader * sIndexHeader;

    for( i = 0;
         i < aTotalIndexHeaderSize;
         i += ID_SIZEOF( smnIndexHeader ) )
    {
        sIndexHeader = (smnIndexHeader*)(aIndexHeaders + i);
        sIndexHeader->mSelfOID = aStartIndexOID + i;
    }
}

/***********************************************************************
 * Description :  PROJ-1362 QP - Large Record & Internal LOB ����.
 *                table�� �÷������� �ε��� ���� ���� Ǯ��.
 * ���ο� �ε��� ������ ������ ��� variable slot�� ã�´�.
 *  - code design
 *  if (���� �߰��� �ε�����  primary key )
 *  then
 *      //replication������ primay key�� �Ǿտ� �����ؾ� �Ѵ�.
 *      i = 0;
 *  else
 *      for( i = 0 ; i < SMC_MAX_INDEX_OID_CNT; i++)
 *      do
 *          if ( table����� mIndexes[i] �� null OID �̰ų�
 *           table ����� mIndexes[i]�� ���� variable slot����  + �ϳ��� �ε��� ��� ũ��
 *             <= SMP_VC_PIECE_MAX_SIZE)
 *          then
 *            break;
 *          fi
 *      done
 *  fi
 * if( i == SMC_MAX_INDEX_OID_CNT)
 * then
 *    retrun �ִ� �ε��� ���� error;
 * fi
 * IDE_EXCEPTION_CONT(success);
 *
 * return i;
 ***********************************************************************/
IDE_RC smcTable::findIndexVarSlot2Insert(smcTableHeader      *aHeader,
                                         UInt                 aFlag,
                                         const UInt          aIndexHeaderSize,
                                         UInt                *aTargetVarSlotIdx)
{
    UInt i =0;


    if( (aFlag & SMI_INDEX_TYPE_MASK) == SMI_INDEX_TYPE_PRIMARY_KEY )
    {
        *aTargetVarSlotIdx =0;
        IDE_ASSERT(aHeader->mIndexes[0].length + aIndexHeaderSize <=
                   SMP_VC_PIECE_MAX_SIZE);
    }
    else
    {
        for( ; i < SMC_MAX_INDEX_OID_CNT ; i++)
        {
            if( (aHeader->mIndexes[i].fstPieceOID == SM_NULL_OID) ||
                (((aHeader->mIndexes[i].length + aIndexHeaderSize)
                  <= SMP_VC_PIECE_MAX_SIZE) && (i != 0)) ||
                // primary key�� ���Ͽ� 0��° variable slot��
                (((aHeader->mIndexes[i].length + 2 * aIndexHeaderSize)
                  <= SMP_VC_PIECE_MAX_SIZE) && ( i == 0)) )

            {
                *aTargetVarSlotIdx = i;
                break;
            }//if
        }//for i
    }//else

    IDE_TEST_RAISE( i == SMC_MAX_INDEX_OID_CNT, maximum_index_count_error );

    return IDE_SUCCESS;

    IDE_EXCEPTION(maximum_index_count_error);
    {
        IDE_SET(ideSetErrorCode (smERR_ABORT_Maximum_Index_Count));
    }

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/***********************************************************************
 * Description : �ε��� ����(����)
 *
 * disk/memory Ÿ���� index�� ���� dropindex�� �����Ѵ�.
 * - �� �Լ������� transaction commit�ñ��� �ε��� ���Ÿ�
 *   pending �������� ó���ϵ��� �Ѵ�.
 * - ���� drop index �۾��� IX lock�� ȹ���ϱ⶧����, ���ÿ�
 *   ���� transaction�� ���� ����� �� �ִ�.
 *
 * - RECOVERY
 *
 *  # index ���� (* disk-index ����)
 *
 ***********************************************************************/
IDE_RC smcTable::dropIndex( void               * aTrans,
                            smcTableHeader     * aHeader,
                            void               * aIndex,
                            sctTBSLockValidOpt   aTBSLvOpt )
{
    SChar          * sDest;
    SChar          * sBase;
    SChar          * sNewIndexRowPtr;
    SChar          * sOldIndexRowPtr;
    UInt             sTableType;
    UInt             sSlotIdx;
    SInt             sOffset = 0;
    smOID            sNewIndexOID = 0;
    smiValue         sVal;
    scPageID         sPageID = 0;
    UInt             sState = 0;
    void           * sIndexListPtr;
    UInt             sTableLatchState = 0;
    UInt             sIndexHeaderSize;
    UInt             sPieceType;

    /* Added By Newdaily since dropIndex Memory must be free at rollback */
    IDE_TEST( smrLogMgr::writeNTALogRec( NULL, /* idvSQL* */
                                         aTrans,
                                         smLayerCallback::getLstUndoNxtLSNPtr( aTrans ),
                                         aHeader->mSpaceID,
                                         SMR_OP_DROP_INDEX,
                                         aHeader->mSelfOID ) 
              != IDE_SUCCESS );


    sIndexHeaderSize = smLayerCallback::getSizeOfIndexHeader();

    /* ----------------------------
     * [1] table validation
     * ---------------------------*/
    IDE_TEST( validateTable( aTrans,
                             aHeader,
                             aTBSLvOpt )
              != IDE_SUCCESS );

    /* ----------------------------
     * [2] drop�ϰ��� �ϴ� Index�� ã��,
     * Table�� Index ������ �����Ѵ�.
     * ---------------------------*/
    IDE_ASSERT( findIndexVarSlot2AlterAndDrop(aHeader, //BUG-23218
                                              aIndex,
                                              sIndexHeaderSize,
                                              &sSlotIdx,
                                              &sOffset)
                 == IDE_SUCCESS );

    //sValue�� �ϳ��� �ε����� �����ǰ� ���� �ε������ ũ��.

    sVal.length = aHeader->mIndexes[sSlotIdx].length - sIndexHeaderSize;
    sVal.value  = NULL;

    IDE_ASSERT( smmManager::getOIDPtr( SMI_ID_TABLESPACE_SYSTEM_MEMORY_DIC, 
                                       aHeader->mIndexes[sSlotIdx].fstPieceOID,
                                       (void**)&sOldIndexRowPtr )
                == IDE_SUCCESS );

    sBase      = sOldIndexRowPtr + ID_SIZEOF(smVCPieceHeader);
    sTableType = SMI_GET_TABLE_TYPE( aHeader );

    if( sTableType == SMI_TABLE_DISK )
    {
        sPieceType = SM_VCPIECE_TYPE_DISK_INDEX;
    }
    else
    {
        sPieceType = SM_VCPIECE_TYPE_MEMORY_INDEX;
    }

    /* BUG-30612 index header self oid�� �߸� ��ϵǴ� ��찡 �ֽ��ϴ�.
     * ���״� ���� �Ǿ����� ������ �̹� �߸� ��ϵǾ� �ִ� ��츦 ����Ͽ�
     * drop�ϴ� slot�� Index Header�� SelfOID�� �����ϵ��� �մϴ�.
     * index header�� SelfOID�� ����� index���� ������ ���˴ϴ�. */
    adjustIndexSelfOID( (UChar*)sBase,
                        aHeader->mIndexes[sSlotIdx].fstPieceOID
                            + ID_SIZEOF(smVCPieceHeader),
                        aHeader->mIndexes[sSlotIdx].length );

    /* ------------------------------------------------
     * drop�� index�� ���Ͽ� pending �������� ��Ͻ�Ŵ
     * - memory index�� ��쿡�� �ش� transaction�� commit�����߿�
     *   smcTable::dropIndexPending�� ȣ���Ͽ� �ٷ� ó���ϸ�,
     *   disk index�� ��쿡�� �ش� tss�� pending operation����
     *   ����Ͽ� disk G.C�� ���� �����ϵ��� ó���Ѵ�.
     * ----------------------------------------------*/
    // �ε����� �Ѱ��� �ִ� ���̺��� drop index�� �Ȱ��.
    if( sVal.length == 0 )
    {
        sOffset = 0;
        IDE_TEST( smLayerCallback::addOID( aTrans,
                                           aHeader->mSelfOID,
                                           aHeader->mIndexes[sSlotIdx].fstPieceOID
                                               + ID_SIZEOF(smVCPieceHeader),
                                           SMI_ID_TABLESPACE_SYSTEM_MEMORY_DIC,
                                           SM_OID_DROP_INDEX )
                  != IDE_SUCCESS );
    }
    else
    {
        if( sVal.length > 0 )
        {
            // �ε����� 2�� �̻��� ���, �ε�������� �ϳ�����
            // ���ο� �ε��� ���� variable slot�� �Ҵ�޴´�.
            IDE_TEST( smpVarPageList::allocSlot(aTrans,
                                            SMI_ID_TABLESPACE_SYSTEM_MEMORY_DIC,
                                            SMC_CAT_TABLE->mSelfOID,
                                            SMC_CAT_TABLE->mVar.mMRDB,
                                            sVal.length,
                                            SM_NULL_OID,
                                            &sNewIndexOID,
                                            &sNewIndexRowPtr,
                                            sPieceType)
                      != IDE_SUCCESS );

            sDest = sNewIndexRowPtr + ID_SIZEOF(smVCPieceHeader);

            IDE_DASSERT( sTableType == SMI_TABLE_MEMORY ||
                         sTableType == SMI_TABLE_META   ||
                         sTableType == SMI_TABLE_DISK   ||
                         sTableType == SMI_TABLE_VOLATILE );

            // index ���ſ� ���� pending operation ����
            IDE_TEST( smLayerCallback::addOID( aTrans,
                                               aHeader->mSelfOID,
                                               aHeader->mIndexes[sSlotIdx].fstPieceOID
                                                   + ID_SIZEOF(smVCPieceHeader) + sOffset,
                                               SMI_ID_TABLESPACE_SYSTEM_MEMORY_DIC,
                                               SM_OID_DROP_INDEX )
                      != IDE_SUCCESS );

            sPageID = SM_MAKE_PID(sNewIndexOID);
            sState  = 1;

            //������ �ε��� ����� ����, �������� ���� copy�۾�.
            idlOS::memcpy( sDest, sBase, sOffset);

            idlOS::memcpy( sDest + sOffset,
                           sBase + sOffset + sIndexHeaderSize,
                           sVal.length - sOffset);

            adjustIndexSelfOID( (UChar*)sDest,
                                sNewIndexOID + ID_SIZEOF(smVCPieceHeader),
                                sVal.length );

            IDE_TEST( smmDirtyPageMgr::insDirtyPage(
                                      SMI_ID_TABLESPACE_SYSTEM_MEMORY_DIC,
                                      sPageID)
                      != IDE_SUCCESS );

            // BUG-25313 : smrUpdate::physicalUpdate() ��ġ ����.
            IDE_TEST( smrUpdate::physicalUpdate(
                                    NULL, /* idvSQL* */
                                    aTrans,
                                    SMI_ID_TABLESPACE_SYSTEM_MEMORY_DIC,
                                    sPageID,
                                    SM_MAKE_OFFSET(sNewIndexOID)
                                        + ID_SIZEOF(smVCPieceHeader),
                                    NULL,
                                    0,
                                    sDest,
                                    sVal.length,
                                    NULL,
                                    0)
                      != IDE_SUCCESS);

        }
    }

    /* ------------------------------------------------
     * [3] �� ��쿡 �°� version list�� �߰�
     * ----------------------------------------------*/
    IDE_TEST(smcRecord::setIndexDropFlag(
                                aTrans,
                                aHeader->mSelfOID,
                                aHeader->mIndexes[sSlotIdx].fstPieceOID
                                    + ID_SIZEOF(smVCPieceHeader) + sOffset,
                                sBase + sOffset,
                                (UShort)SMN_INDEX_DROP_TRUE )
             != IDE_SUCCESS);

    IDE_TEST(smcRecord::setFreeFlagAtVCPieceHdr(
                                        aTrans,
                                        aHeader->mSelfOID,
                                        SMI_ID_TABLESPACE_SYSTEM_MEMORY_DIC,
                                        aHeader->mIndexes[sSlotIdx].fstPieceOID,
                                        sOldIndexRowPtr,
                                        SM_VCPIECE_FREE_OK)
             != IDE_SUCCESS);

    // old index information version
    IDE_TEST( smLayerCallback::addOID( aTrans,
                                       SMC_CAT_TABLE->mSelfOID,
                                       aHeader->mIndexes[sSlotIdx].fstPieceOID,
                                       SMI_ID_TABLESPACE_SYSTEM_MEMORY_DIC,
                                       SM_OID_OLD_VARIABLE_SLOT )
              != IDE_SUCCESS );

    if( sVal.length == 0 )
    {
        sNewIndexOID = SM_NULL_OID;
    }
    else
    {
        // new index information version
        IDE_TEST( smLayerCallback::addOID( aTrans,
                                           SMC_CAT_TABLE->mSelfOID,
                                           sNewIndexOID,
                                           SMI_ID_TABLESPACE_SYSTEM_MEMORY_DIC,
                                           SM_OID_NEW_VARIABLE_SLOT )
                  != IDE_SUCCESS );
    }

    sPageID = SM_MAKE_PID(aHeader->mSelfOID);

    /* ------------------------------------------------
     * SMR_LT_UPDATE�� SMR_SMC_TABLEHEADER_UPDATE_INDEX �α�
     * ----------------------------------------------*/
    IDE_TEST( smrUpdate::updateIndexAtTableHead( NULL, /* idvSQL* */
                                                 aTrans,
                                                 aHeader->mSelfOID,
                                                 &(aHeader->mIndexes[sSlotIdx]),
                                                 sSlotIdx,
                                                 sNewIndexOID,
                                                 sVal.length,
                                                 SM_VCDESC_MODE_OUT )
              != IDE_SUCCESS );

    IDU_FIT_POINT( "3.PROJ-1552@smcTable::dropIndex" );

    IDE_TEST( smcTable::latchExclusive( aHeader ) != IDE_SUCCESS );
    sTableLatchState = 1;

    if(aHeader->mDropIndexLst == NULL)
    {
        /* smcTable_dropIndex_alloc_IndexListPtr.tc */
        IDU_FIT_POINT("smcTable::dropIndex::alloc::IndexListPtr");
        IDE_TEST(mDropIdxPagePool.alloc(&sIndexListPtr) != IDE_SUCCESS);
        aHeader->mDropIndex = 0;

        idlOS::memcpy(sIndexListPtr, sBase + sOffset, sIndexHeaderSize);
        aHeader->mDropIndexLst = sIndexListPtr;
    }
    else
    {
        idlOS::memcpy( (SChar *)(aHeader->mDropIndexLst) +
                       ( aHeader->mDropIndex * sIndexHeaderSize ),
                       sBase + sOffset,
                       sIndexHeaderSize);
    }

    aHeader->mIndexes[sSlotIdx].fstPieceOID    = sNewIndexOID;
    aHeader->mIndexes[sSlotIdx].length         = sVal.length;
    aHeader->mIndexes[sSlotIdx].flag           = SM_VCDESC_MODE_OUT;

    (aHeader->mDropIndex)++;

    sTableLatchState = 0;
    IDE_TEST( smcTable::unlatch( aHeader ) != IDE_SUCCESS );

    IDE_TEST( smmDirtyPageMgr::insDirtyPage(
                                      SMI_ID_TABLESPACE_SYSTEM_MEMORY_DIC,
                                      sPageID) 
              != IDE_SUCCESS );

    IDE_TEST( smLayerCallback::addOID( aTrans,
                                       SMC_CAT_TABLE->mSelfOID,
                                       aHeader->mSelfOID,
                                       SMI_ID_TABLESPACE_SYSTEM_MEMORY_DIC,
                                       SM_OID_CHANGED_FIXED_SLOT )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if(sState != 0)
    {
        IDE_PUSH();
        (void)smmDirtyPageMgr::insDirtyPage(
            SMI_ID_TABLESPACE_SYSTEM_MEMORY_DIC, sPageID);
        IDE_POP();
    }

    if(sTableLatchState != 0)
    {
        IDE_PUSH();
        IDE_ASSERT( smcTable::unlatch( aHeader ) == IDE_SUCCESS );
        IDE_POP();
    }

    return IDE_FAILURE;

}

/***********************************************************************
 * Description :  PROJ-1362 QP - Large Record & Internal LOB ����.
 *                table�� �÷������� �ε��� ���� ���� Ǯ��.
 *  drop �ε���  ��� variable slot�� ã��, �ش� variable slot����
 *  �ε��� ��� offset�� ���Ѵ�.
 *  - code design
 * for( i = 0 ; i < SMC_MAX_INDEX_OID_CNT; i++)
 * do
 *  if( table����� mIndexes[i] == SM_NULL_OID)
 *  then
 *    continue;
 *  else
 *    if( table����� mIndexes[i]�� variable slot�ȿ�
 *       drop�� index����� �ִ°�?)
 *    then
 *        �ش� variable slot���� drop�� index�� offset�� ���Ѵ�;
 *         break;
 *    fi
 *  fi
 *  fi
 * done
 * if( i == SMC_MAX_INDEX_OID_CNT)
 * then
 *     �ε����� ã���� ���ٴ� ����;
 * fi
 * BUG-23218 : DROP INDEX �ܿ� ALTER INDEX ���������� ����� �� �ֵ��� ������
 **************************************************************************/
IDE_RC smcTable::findIndexVarSlot2AlterAndDrop(smcTableHeader      *aHeader,
                                               void*                aIndex, // BUBUB CHAR pointer
                                               const UInt           aIndexHeaderSize,
                                               UInt*                aTargetIndexVarSlotIdx,
                                               SInt*                aOffset)
{
    UInt i;
    UInt j;
    SChar   * sSrc;


    for(i=0  ; i < SMC_MAX_INDEX_OID_CNT ; i++)
    {
        if( aHeader->mIndexes[i].fstPieceOID == SM_NULL_OID)
        {
            continue;
        }//if
        IDE_ASSERT( smmManager::getOIDPtr( 
                        SMI_ID_TABLESPACE_SYSTEM_MEMORY_DIC, 
                        aHeader->mIndexes[i].fstPieceOID + ID_SIZEOF(smVCPieceHeader),
                        (void**)&sSrc )
                    == IDE_SUCCESS );
        for( j = 0;
             j < aHeader->mIndexes[i].length;
             j += aIndexHeaderSize,sSrc += aIndexHeaderSize )
        {
            if( sSrc == aIndex ) /* ã���ִ� smnIndexHeader �� ��� */
            {
                *aTargetIndexVarSlotIdx = i;
                *aOffset =  j;
                IDE_CONT( success );
            }//if
        }// inner for
    }//outer for

    IDE_TEST_RAISE( i == SMC_MAX_INDEX_OID_CNT, not_found_error );

    IDE_EXCEPTION_CONT( success );

    return IDE_SUCCESS;

    IDE_EXCEPTION(not_found_error);
    {
        IDE_SET(ideSetErrorCode (smERR_ABORT_Index_Not_Found ));
    }

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : persistent index ���� ����
 * �� �Լ��� table�� Ư�� index�� ���Ͽ� persistent index ���θ�
 * �����ϴ� �Լ��ν� memory index�� ���ؼ� �����Ǵ�
 * ����̹Ƿ�, disk index������ ����� �ʿ� ����.
 ***********************************************************************/
IDE_RC smcTable::alterIndexInfo(void           * aTrans,
                                smcTableHeader * aHeader,
                                void           * aIndex,
                                UInt             aFlag)
{
    smnIndexHeader    *sIndexHeader;
    UInt               sIndexHeaderSize;
    UInt               sIndexSlot;
    scPageID           sPageID = SC_NULL_PID;
    SInt               sOffset;

    sIndexHeader = ((smnIndexHeader*)aIndex);
    sIndexHeaderSize = smLayerCallback::getSizeOfIndexHeader();

    /* ----------------------------
     * [1] table validation
     * ---------------------------*/
    IDE_TEST( validateTable( aTrans,
                             aHeader,
                             SCT_VAL_DDL_DML )
              != IDE_SUCCESS );


    /* fix BUG-23218 : �ε��� Ž�� ���� ���ȭ*/
    /* ----------------------------
     * [2] �����ϰ��� �ϴ� Index�� ã�´�.
     * ---------------------------*/
    IDE_ASSERT( findIndexVarSlot2AlterAndDrop(aHeader,
                                              sIndexHeader,
                                              sIndexHeaderSize,
                                              &sIndexSlot,
                                              &sOffset)
                 == IDE_SUCCESS );

    /* ------------------------------------------------
     * [3] ã�� �ε����� sIndexSlot�� ��������,
     *     �ε����� ��ġ�� ����Ѵ�.
     * ------------------------------------------------*/
    sPageID = SM_MAKE_PID( aHeader->mIndexes[sIndexSlot].fstPieceOID );



    /* ---------------------------------------------
     * [4] �α� �� �����Ѵ�.
     * ---------------------------------------------*/

    IDE_TEST( smrUpdate::setIndexHeaderFlag( NULL, /* idvSQL* */
                                             aTrans,
                                             aHeader->mIndexes[sIndexSlot].fstPieceOID,
                                             ID_SIZEOF(smVCPieceHeader) + sOffset , // BUG-23218 : �α� ��ġ ���� ����
                                             smLayerCallback::getFlagOfIndexHeader( aIndex ),
                                             aFlag)
              != IDE_SUCCESS);


    smLayerCallback::setFlagOfIndexHeader( aIndex, aFlag );

    (void)smmDirtyPageMgr::insDirtyPage(
                                    SMI_ID_TABLESPACE_SYSTEM_MEMORY_DIC, 
                                    sPageID);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : persistent index�� Inconsistent���� ����
 *
 ***********************************************************************/
IDE_RC smcTable::setIndexInconsistency(smOID            aTableOID,
                                       smOID            aIndexID )
{
    smcTableHeader   * sTableHeader;
    smnIndexHeader   * sIndexHeader;

    UInt               sIndexHeaderSize;
    UInt               sIndexSlot;
    scPageID           sPageID = SC_NULL_PID;
    SInt               sOffset;

    IDE_TEST( smcTable::getTableHeaderFromOID( aTableOID,
                                               (void**)&sTableHeader )
              != IDE_SUCCESS );

    sIndexHeader = (smnIndexHeader*)smcTable::getTableIndexByID(
                                (void*)sTableHeader,
                                aIndexID );
    
    sIndexHeaderSize = smLayerCallback::getSizeOfIndexHeader();

    /* fix BUG-23218 : �ε��� Ž�� ���� ���ȭ
     *  �����ϰ��� �ϴ� Index�� Offset���� ã�´�. */
    IDE_ASSERT( findIndexVarSlot2AlterAndDrop(sTableHeader,
                                              sIndexHeader,
                                              sIndexHeaderSize,
                                              &sIndexSlot,
                                              &sOffset)
                 == IDE_SUCCESS );

    /* ------------------------------------------------
     *  ã�� �ε����� sIndexSlot�� ��������,
     *  �ε����� ��ġ�� ����Ѵ�.
     * ------------------------------------------------*/
    sPageID = SM_MAKE_PID( sTableHeader->mIndexes[sIndexSlot].fstPieceOID );

    // Inconsistent -> Consistent���� ����.
    smLayerCallback::setIsConsistentOfIndexHeader( sIndexHeader, ID_FALSE );

    (void)smmDirtyPageMgr::insDirtyPage(
                                    SMI_ID_TABLESPACE_SYSTEM_MEMORY_DIC, 
                                    sPageID);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/***********************************************************************
 * Description : ��� table�� ��� persistent index�� Inconsistent���� ����
 *
 * Related Issues:
 *      PROJ-2184 RP Sync ���� ���
 *
 *  aTableHeader   - [IN] ��� table�� smcTableHeader
 ***********************************************************************/
IDE_RC smcTable::setAllIndexInconsistency( smcTableHeader   * aTableHeader )
                                         
{
    smnIndexHeader    * sIndexHeader;
    UInt                sIndexIdx = 0;
    UInt                sIndexCnt;
                      
    UInt                sIndexHeaderSize;
    UInt                sIndexSlot;
    smVCPieceHeader   * sVCPieceHdr;
    UInt                sOffset;

    sIndexCnt = smcTable::getIndexCount( aTableHeader );

    /* Index�� �ϳ��� ������ �ٷ� success return */
    IDE_TEST_CONT( sIndexCnt == 0, no_indexes );

    sIndexHeaderSize = smLayerCallback::getSizeOfIndexHeader();

    for( sIndexSlot = 0; sIndexSlot < SMC_MAX_INDEX_OID_CNT; sIndexSlot++ )
    {
        if(aTableHeader->mIndexes[sIndexSlot].fstPieceOID == SM_NULL_OID)
        {
            continue;
        }

        IDE_ASSERT( smmManager::getOIDPtr( 
                        SMI_ID_TABLESPACE_SYSTEM_MEMORY_DIC, 
                        aTableHeader->mIndexes[sIndexSlot].fstPieceOID,
                        (void**)&sVCPieceHdr )
                    == IDE_SUCCESS );

        sOffset = 0;
        while( (sOffset < aTableHeader->mIndexes[sIndexSlot].length) &&
               (sIndexIdx < sIndexCnt) )
        {
            sIndexHeader = (smnIndexHeader*)((UChar*)sVCPieceHdr +
                                             ID_SIZEOF(smVCPieceHeader) +
                                             sOffset);

            if ( smLayerCallback::getIsConsistentOfIndexHeader( sIndexHeader )
                 == ID_TRUE )
            {
                // Inconsistent -> Consistent���� ����.
                smLayerCallback::setIsConsistentOfIndexHeader( sIndexHeader,
                                                               ID_FALSE );
            }
            else
            {
                /* �̹� inconsistent �����̴�. */
            }

            sOffset += sIndexHeaderSize;
            sIndexIdx++;
        }

        (void)smmDirtyPageMgr::insDirtyPage(
                SMI_ID_TABLESPACE_SYSTEM_MEMORY_DIC,
                SM_MAKE_PID(aTableHeader->mIndexes[sIndexSlot].fstPieceOID) );

        if( sIndexIdx == sIndexCnt )
        {
            /* ��� index�� ���ؼ� consistent flag�� ���������Ƿ� loop Ż�� */
            break;
        }
        else
        {
            IDE_DASSERT( sIndexIdx <= sIndexCnt );
        }
    }

    IDE_EXCEPTION_CONT( no_indexes );

    return IDE_SUCCESS;
}


/***********************************************************************
 * Description : undo�ÿ� create index�� ���� nta operation ����
 ***********************************************************************/
IDE_RC smcTable::dropIndexByAbortHeader( idvSQL*            aStatistics,
                                         void             * aTrans,
                                         smcTableHeader   * aHeader,
                                         const  UInt        aIdx,
                                         void             * aIndexHeader,
                                         smOID              aOIDIndex )
{
    scPageID         sPageID = 0;
    UInt             sState = 0;
    UInt             sDstLength;
    UInt             sSrcLength;
    SChar           *sIndexRowPtr;
    smOID            sSrcIndexOID;

    /* drop�ϰ��� �ϴ� Index�� ã�� Table�� Index ������ �����Ѵ�.*/
    IDE_TEST_RAISE( aHeader->mIndexes[aIdx].fstPieceOID == SM_NULL_OID,
                    not_found_error );

    sSrcLength = aHeader->mIndexes[aIdx].length;
    sDstLength = sSrcLength - smLayerCallback::getSizeOfIndexHeader();

    /* �� ��쿡 �°� version list�� �߰� */
    sSrcIndexOID = aHeader->mIndexes[aIdx].fstPieceOID;
    IDE_ASSERT( smmManager::getOIDPtr( SMI_ID_TABLESPACE_SYSTEM_MEMORY_DIC, 
                                       sSrcIndexOID,
                                       (void**)&sIndexRowPtr )
                == IDE_SUCCESS );

    /* header�� assign �Ǿ��� new index OID �� delete flag ���� */
    IDE_TEST(smcRecord::setFreeFlagAtVCPieceHdr(aTrans,
                                                aHeader->mSelfOID,
                                                SMI_ID_TABLESPACE_SYSTEM_MEMORY_DIC,
                                                sSrcIndexOID,
                                                sIndexRowPtr,
                                                SM_VCPIECE_FREE_OK)
             != IDE_SUCCESS);

    sPageID = SM_MAKE_PID(aHeader->mSelfOID);
    sState  = 1;

    if( aOIDIndex != SM_NULL_OID )
    {
        /* BUG-17955: Add/Drop Column ������ smiStatement End��, Unable to invoke
         * mutex_lock()�� ���� ������ ���� */
        IDE_TEST( removeIdxHdrAndCopyIndexHdrArr(
                                          aTrans,
                                          aIndexHeader,
                                          sIndexRowPtr,
                                          aHeader->mIndexes[aIdx].length,
                                          aOIDIndex )
                  != IDE_SUCCESS );

        /* ���� index oID�� delete flag�� used�� �����Ѵ�. */
        IDE_ASSERT( smmManager::getOIDPtr(
                                    SMI_ID_TABLESPACE_SYSTEM_MEMORY_DIC, 
                                    aOIDIndex,
                                    (void**)&sIndexRowPtr )
                    == IDE_SUCCESS );
        IDE_TEST(smcRecord::setFreeFlagAtVCPieceHdr(aTrans,
                                                    aHeader->mSelfOID,
                                                    SMI_ID_TABLESPACE_SYSTEM_MEMORY_DIC,
                                                    aOIDIndex,
                                                    sIndexRowPtr,
                                                    SM_VCPIECE_FREE_NO)
                 != IDE_SUCCESS);
    }
    else
    {
        /* aOIDIndex �� NULL�� ��� length�� 0�̾�� �� */
        IDE_ASSERT( sDstLength == 0 );
    }

    /* ���� index oID(aOIDIndex)�� table header�� assign�Ѵ�. */
    IDE_TEST( smrUpdate::updateIndexAtTableHead(NULL, /* idvSQL* */
                                                aTrans,
                                                aHeader->mSelfOID,
                                                &(aHeader->mIndexes[aIdx]),
                                                aIdx,
                                                aOIDIndex,
                                                sDstLength,
                                                SM_VCDESC_MODE_OUT)
              != IDE_SUCCESS );


    /* ���� index oID�� �����Ѵ�. */
    aHeader->mIndexes[aIdx].fstPieceOID = aOIDIndex;
    aHeader->mIndexes[aIdx].length = sDstLength;
    aHeader->mIndexes[aIdx].flag   = SM_VCDESC_MODE_OUT;

    // PR-14912
    if ( ( smrRecoveryMgr::isRestart() == ID_TRUE ) &&
         ( aOIDIndex != SM_NULL_OID ))
    {
        // at restart, previous index header slot not create,
        // but, do not redo CLR
        if ( smrRecoveryMgr::isRefineDRDBIdx() == ID_TRUE )
        {
            /* BUG-16555: Restart�� Undo�ϴٰ� smcTable::dropIndexByAbortHeader
             * ���� ���� ���: Disk Table�� ���ؼ��� �� �Լ��� ȣ��Ǿ�� ��. */
            if( SMI_TABLE_TYPE_IS_DISK( aHeader ) == ID_TRUE )
            {
                IDE_TEST( rebuildRuntimeIndexHeaders(
                                                 aStatistics,
                                                 aHeader,
                                                 0 ) /* aMaxSmoNo */
                          != IDE_SUCCESS );
            }
        }
        else
        {
            // nothing to do..
        }
    }
    else
    {
        // at runtime, previous index header slot not free
        // nothing to do...
    }

    sState = 0;
    IDE_TEST( smmDirtyPageMgr::insDirtyPage(
                                  SMI_ID_TABLESPACE_SYSTEM_MEMORY_DIC,
                                  sPageID) 
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION(not_found_error);
    {
        IDE_SET(ideSetErrorCode (smERR_ABORT_Index_Not_Found));
    }
    IDE_EXCEPTION_END;

    if(sState != 0)
    {
        IDE_PUSH();
        (void)smmDirtyPageMgr::insDirtyPage(
                                    SMI_ID_TABLESPACE_SYSTEM_MEMORY_DIC, 
                                    sPageID);
        IDE_POP();
    }

    return IDE_FAILURE;

}

/***********************************************************************
 * Description : drop index�� ���� pending ���� ó�� (����)
 ***********************************************************************/
IDE_RC smcTable::dropIndexPending( void*  aIndexHeader )
{

    smcTableHeader *sPtr;

    // [1] ���� Index ���ڵ� ����
    IDE_ASSERT( smcTable::getTableHeaderFromOID( smLayerCallback::getTableOIDOfIndexHeader( aIndexHeader ),
                                                 (void**)&sPtr )
                == IDE_SUCCESS );

    if ( smLayerCallback::isNullModuleOfIndexHeader( aIndexHeader ) != ID_TRUE )
    {
        IDE_TEST( smLayerCallback::dropIndex( sPtr,
                                              aIndexHeader )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

/***********************************************************************
 * Description : drop table�� pending ���� ���� (����)
 * pending �������� ��ϵ� droptable�� ���� ���� ���� ������ �����Ѵ�.
 * memory table�� ��� ager�� ó���ϸ�, disk table�� ��� disk G.C��
 * ���� ȣ��Ǿ� ó���ȴ�.
 *
 *  [!!!!!!!]
 * �� memory ager/disk GC/refine catalog table ����� ȣ��Ǵµ�, ��,
 * refine catalog table�ÿ��� disk table�� ���Ͽ� ȣ����� �ʴ´�.
 * ȣ��Ǵ� ���� ������ ���� .
 *
 * - create memory table�� ������ tx�� commit/abort�� ��� (by ager or G.C)
 * - drop disk/memory table�� ������ tx�� commit�� ���  tss�� ������
 *   (by ager or G.C)
 * - refine catalog table ����� memory table�� ���Ͽ� ȣ���
 *
 * : �̹ۿ� create disk table�ϴٰ� abort�ϴ� ��� �ش� undo�� ���Ͽ�
 *   ���� ó����.
 * : �̹ۿ� drop disk table�ϴٰ� commit�ϴ� ��� �ش� tss�� G.C�� ����
 *   ó����
 *
 * - 2nd code design
 *   + table page list�� �ý��ۿ� ��ȯ��
 *     if (memory table)
 *        - fixed page list entry ����
 *        - variable page list entry ����
 *     else if (disk table)
 *        - data page segment ����
 *     endif
 *   + table header�� index ������ ������ variable
 *     slot�� free��Ŵ
 *   + table header�� column ������ ������ variable
 *     slot�� free��Ŵ
 *   + table header�� info ������ ������ variable
 *     slot�� free��Ŵ
 *   + table ���ſ� ���� nta �α� (SMR_OP_NULL)
 *   + table slot page�� dirty page ���
 *
 * - RECOVERY
 *
 *  # drop table pending (* disk-table����)
 *
 *  (0){(1)         (2)}          {(3)          (4)}
 *   |---|-----------|--------------|------------|--------> time
 *       |           |              |            |
 *      *update      *free          update       free
 *      seg rID,     table         index oID    v-slot
 *      meta PID     segment       on tbl-hdr   to cat-tbl
 *      on tbl-hdr   [NTA->(0)]    (R/U)        [NTA->(2)]
 *      (R/U)
 *
 *     {(5)         (6)}          {(7)          (8)}        (9)
 *    ---|-----------|--------------|------------|-----------|----> time
 *       |           |              |            |           |
 *      update       free          update       free        [NTA->(0)]
 *      column oID   v-slot        info oID     v-slot
 *      on tbl-hdr   to cat-tbl    on tbl-hdr   to cat-tbl
 *      (R/U)       [NTA->(4)]    (R/U)        [NTA->(6)]
 *
 *  # Disk-Table Recovery Issue
 *
 *  : �Ҵ�� TSS�� pending table oID�� ��ϵǾ� �ִ� ����  pending ������
 *    �Ϸ���� �ʾҰų� ������� ���� ����̴�. �׷��Ƿ� �� �Լ���
 *    �ݺ��Ͽ� �����Ͽ��� �Ѵ�.
 *
 *  - (1)���� �α��Ͽ��ٸ�
 *    + (1) -> undo
 *      : tbl-hdr�� segment rID, table meta PID�� ���� image�� �����Ѵ�.
 *    + smxTrans::abort�� �����Ѵ�.
 *
 *  - (2)���� mtx commit�� �Ǿ��ٸ�
 *    + (2) -> logical undo
 *      : doNTADropTable�� �����Ͽ� free segment�� �Ѵ�.
 *    + smxTrans::abort�Ѵ�.
 *    + ...
 *    + smxTrans::abort�Ѵ�.
 *
 *  - (7)���� �α�Ǿ��ٸ�
 *    + (7) -> undo
 *      : table header�� info oID�� ���� image�� �����Ѵ�.
 *    + (6)�� ���� ó���� �Ѵ�.
 *    + ...
 *    + smxTrans::abort�Ѵ�.
 *
 *  - (8)���� �α�Ǿ��ٸ�
 *    + (8) -> logical redo
 *      : var slot�� �ٽ� free �����Ѵ�
 *    + (6)�� ���� ó���� �Ѵ�.
 *    + ...
 *    + smxTrans::abort�Ѵ�.
 *
 *  - (9)���� �α�Ǿ��ٸ�
 *    + (9) -> (0)������ skip undo
 *    + smxTrans::abort�Ѵ�.
 *
 ***********************************************************************/
IDE_RC smcTable::dropTablePending( idvSQL          *aStatistics,
                                   void            * aTrans,
                                   smcTableHeader  * aHeader,
                                   idBool            aFlag )
{

    smOID               sRowOID;
    scPageID            sPageID;
    smLSN               sLsnNTA1;
    smLSN               sLsnNTA2;
    smpSlotHeader     * sSlotHeader;
    UInt                sState = 0;
    UInt                sTableType;
    idBool              sSkipTbsAcc;
    SChar             * sRowPtr;

    // Begin NTA[-1-]
    sLsnNTA1 = smLayerCallback::getLstUndoNxtLSN( aTrans );

    /* ------------------------------------------------
     * [1] table page list ����
     * - memory table�� ���, page list entry�� �Ҵ��
     *   �������� free�Ͽ� �ý��ۿ� ��ȯ�Ѵ�.
     * - disk table�� ���, segment�� ���������ν�
     *   ���̺� �Ҵ�� ���������� ��� �ý��ۿ� ��ȯ�Ѵ�.
     * ----------------------------------------------*/
    sSlotHeader = (smpSlotHeader*)aHeader - 1;

    sPageID = SMP_SLOT_GET_PID((SChar *)sSlotHeader);

    sState = 1;

    sTableType = SMI_GET_TABLE_TYPE( aHeader );
    IDE_DASSERT( sTableType == SMI_TABLE_MEMORY   ||
                 sTableType == SMI_TABLE_META     ||
                 sTableType == SMI_TABLE_VOLATILE ||
                 sTableType == SMI_TABLE_DISK     ||
                 sTableType == SMI_TABLE_REMOTE );

    // Tablespace���� ���� ���ɼ� ���� ( �����ϸ� ID_TRUE )
    //   - DROP/DISCARD�� Tablespace�� ��� ������ �Ұ�����.
    //   - DROP Table����� Tablespace�� IX���� ���� �����̴�.
    sSkipTbsAcc =  sctTableSpaceMgr::hasState( aHeader->mSpaceID,
                                               SCT_SS_SKIP_DROP_TABLE_CONTENT );

    if (sSkipTbsAcc == ID_TRUE )
    {
        // Tablespace�� �������� �ʴ´�.
        // �� disk table�� ��� tablespace�� ���¿� �������
        // index�� �����Ѵ�.
        if ((sTableType == SMI_TABLE_DISK) && (aFlag == ID_TRUE))
        {
            IDE_TEST( smLayerCallback::dropIndexes( aHeader )
                      != IDE_SUCCESS );
        }
    }
    else
    {
        if (sTableType == SMI_TABLE_DISK)
        {
            if ( sdpPageList::getTableSegDescPID(&(aHeader->mFixed.mDRDB))
                 != SD_NULL_PID )
            {
                IDE_TEST( sdpSegment::freeTableSeg4Entry(
                                              aStatistics,
                                              aHeader->mSpaceID,
                                              aTrans,
                                              aHeader->mSelfOID,
                                              &(aHeader->mFixed.mDRDB),
                                              SDR_MTX_LOGGING)
                          != IDE_SUCCESS );

                /* BUG-18101
                   ���� drop �� table�� ���� runtime ������ �����ϴ� ����
                   smcCatalogTable::finAllocedTableSlots()���� ����.
                   ������ dropPending�� ȣ��Ǵ� �� �Լ�����
                   table header ������ ��� �ִ� slot�� free�Ǿ� ������
                   finAllocedTableSlots()���� slot�� Ž���� �� �˻��� �ȵǾ�
                   drop �� disk table�� ���ؼ��� runtime ������ �������� ���ϰ� �ִ�
                   ������ �߰ߵǾ���.
                   ����� ó���ϱ� ���ؼ��� ö���� ���迡 ������ �ҽ� �����丵��
                   �ʿ��ϱ� ������ �ϴ� �޸� ���̺�� ����������
                   ���⼭ ��Ÿ�� ������ �����ϵ��� �Ѵ�. */
                IDE_TEST(finRuntimeItem(aHeader) != IDE_SUCCESS);

                IDE_TEST( destroyRowTemplate( aHeader )!= IDE_SUCCESS );
            }
        }
        else if ((sTableType == SMI_TABLE_MEMORY) ||
                 (sTableType == SMI_TABLE_META)   ||
                 (sTableType == SMI_TABLE_REMOTE))
        {
            IDE_TEST( smpFixedPageList::freePageListToDB(
                                                  aTrans,
                                                  aHeader->mSpaceID,
                                                  aHeader->mSelfOID,
                                                  &(aHeader->mFixed.mMRDB))
                          != IDE_SUCCESS );

            IDE_TEST( smpVarPageList::freePageListToDB(aTrans,
                                                       aHeader->mSpaceID,
                                                       aHeader->mSelfOID,
                                                       aHeader->mVar.mMRDB)
                      != IDE_SUCCESS );

            IDE_TEST(finRuntimeItem(aHeader) != IDE_SUCCESS);
        }
        else if (sTableType == SMI_TABLE_VOLATILE)
        {
            IDE_TEST( svpFixedPageList::freePageListToDB(
                                                  aTrans,
                                                  aHeader->mSpaceID,
                                                  aHeader->mSelfOID,
                                                  &(aHeader->mFixed.mVRDB))
                      != IDE_SUCCESS );

            IDE_TEST( svpVarPageList::freePageListToDB(aTrans,
                                                       aHeader->mSpaceID,
                                                       aHeader->mSelfOID,
                                                       aHeader->mVar.mVRDB)
                      != IDE_SUCCESS );

            IDE_TEST(finRuntimeItem(aHeader) != IDE_SUCCESS);
        }
        else
        {
            IDE_ASSERT(0);
        }

        /* ------------------------------------------------
         * table�� index���� ��� drop ��Ŵ
         * - �Ϲ������� a_flag �� ID_TRUE ���� ������,
         *   refineCatalogTable���� skip �ϱ� ����
         *   ID_FALSE ���� ������.
         * ----------------------------------------------*/
        if( aFlag == ID_TRUE )
        {
            IDE_TEST( smLayerCallback::dropIndexes( aHeader )
                      != IDE_SUCCESS );
        }
    }

    /* ------------------------------------------------
     * [2] table header�� index ������ ������ variable
     *  slot�� free��Ŵ
     * ----------------------------------------------*/
    if( smcTable::getIndexCount(aHeader) != 0)
    {
    // Begin NTA[-2-]
        IDE_TEST(freeIndexes(aStatistics,
                             aTrans,
                             aHeader,
                             sTableType)
                 != IDE_SUCCESS);
    // END NTA[ -2-]
    }

    // Table Header�� Index Latch�� �ı��ϰ� ����
    IDE_TEST( finiAndFreeIndexLatch( aHeader )
              != IDE_SUCCESS );

    IDU_FIT_POINT( "1.BUG-21545@smcTable::dropTablePending" );

    /* ------------------------------------------------
     * [3] table header�� column ������ ������ variable
     *  slot�� free��Ŵ
     * ----------------------------------------------*/
    if(aHeader->mColumns.fstPieceOID != SM_NULL_OID)
    {
        // Begin NTA[-3-]
        IDE_TEST( smcTable::freeColumns(aStatistics,
                                        aTrans,
                                        aHeader)
                  != IDE_SUCCESS);

        // END NTA[-3-]
    }

    /* ------------------------------------------------
     * [5] table header�� info ������ ������ variable
     *  slot�� free��Ŵ
     * ----------------------------------------------*/
    if(aHeader->mInfo.fstPieceOID != SM_NULL_OID)
    {
        // Begin NTA[-4-]
        sLsnNTA2 = smLayerCallback::getLstUndoNxtLSN( aTrans );

        /* ------------------------------------------------
         * info ������ ������ variable slot�� ���� �� table header��
         * info ������ SM_NULL_OID�� ����
         * - SMR_LT_UPDATE�� SMR_SMC_TABLEHEADER_UPDATE_INFO �α�
         * ----------------------------------------------*/
        IDE_TEST( smrUpdate::updateInfoAtTableHead(
                      NULL, /* idvSQL* */
                      aTrans,
                      SM_MAKE_PID(aHeader->mSelfOID),
                      SM_MAKE_OFFSET(aHeader->mSelfOID) + SMP_SLOT_HEADER_SIZE,
                      &(aHeader->mInfo),
                      SM_NULL_OID,
                      0,
                      SM_VCDESC_MODE_OUT) != IDE_SUCCESS );


        sRowOID = aHeader->mInfo.fstPieceOID;

        aHeader->mInfo.fstPieceOID = SM_NULL_OID;
        aHeader->mInfo.length = 0;
        aHeader->mInfo.flag = SM_VCDESC_MODE_OUT;

        /* ------------------------------------------------
         * catalog table�� variable slot�� �����Ͽ� ��ȯ��.
         * ----------------------------------------------*/
        IDE_ASSERT( smmManager::getOIDPtr( 
                        SMI_ID_TABLESPACE_SYSTEM_MEMORY_DIC, 
                        sRowOID,
                        (void**)&sRowPtr )
                    == IDE_SUCCESS );

        IDE_TEST( smpVarPageList::freeSlot(aTrans,
                                           SMI_ID_TABLESPACE_SYSTEM_MEMORY_DIC,
                                           SMC_CAT_TABLE->mVar.mMRDB,
                                           sRowOID,
                                           sRowPtr,
                                           &sLsnNTA2,
                                           SMP_TABLE_NORMAL)
                  != IDE_SUCCESS );
        // END   NTA[-4-]

    }

    // To Fix BUG-16752 Memory Table Drop�� Runtime Entry�� �������� ����
    //
    // Refine�� �̹� Drop�� Table�� ���� Lock & Runtime Item�� �Ҵ�����
    // �ʵ��� �ϱ� ����.
    //  => smpSlotHeader.mUsedFlag �� ID_FALSE�� �ٲ۴�.
    //     -> refine�� nextOIDall���ʿ��� skip

    //  (����) ���� �����Ǵ� Table�� ���� ������ �ʵ���
    //          catalog table�� free slot list���� ���� �ʴ´�.
    IDE_TEST( smpFixedPageList::setFreeSlot(
                  aTrans,
                  SMI_ID_TABLESPACE_SYSTEM_MEMORY_DIC,
                  sPageID,
                  (SChar*) sSlotHeader,
                  SMP_TABLE_NORMAL )
              != IDE_SUCCESS );

    /* ------------------------------------------------
     * [6] table ���ſ� ���� NTA �α�
     * END NTA[-1-]
     * ----------------------------------------------*/
    IDE_TEST( smrLogMgr::writeNTALogRec(NULL, /* idvSQL* */
                                        aTrans,
                                        &sLsnNTA1,
                                        SMR_OP_NULL)
             != IDE_SUCCESS );


    IDE_TEST( smmDirtyPageMgr::insDirtyPage( 
                                    SMI_ID_TABLESPACE_SYSTEM_MEMORY_DIC, 
                                    sPageID ) 
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if(sState != 0)
    {
        IDE_PUSH();
        (void)smmDirtyPageMgr::insDirtyPage(
                                    SMI_ID_TABLESPACE_SYSTEM_MEMORY_DIC, 
                                    sPageID);
        IDE_POP();
    }

    return IDE_FAILURE;
}

/***********************************************************************
 * Description :  PROJ-1362 QP - Large Record & Internal LOB ����.
 *                table�� �÷������� �ε��� ���� ���� Ǯ��.
 *  table drop pending�ÿ� �Ҹ��� �Լ���, table�� �ε��� ������ ����
 *  variable slot�� �����Ѵ�.
 *  - code design
 *  for( i =0 ; i  < i < SMC_MAX_INDEX_OID_CNT; i++)
 *  do
 *    if( table����� mIndex[i]  == SM_NULL_OID)
 *    then
 *         continue;
 *    fi
 *    Ʈ������� last undo LSN�� ��´�;
 *    table ����� mIndexes[i]������ SM_NULL_OID�� �����ϴ�
 *    physical logging;
 *    table ����� mIndexes[i]�� ���;
 *    table ����� mIndexes[i]�� SM_NULL_OID���� ����;
 *    table ����� mIndexes[i]�� ����Ű�� �־��� variable slot����
 *    (smpVarPageList::freeSlot);
 * done
 ***********************************************************************/
IDE_RC smcTable::freeIndexes(idvSQL          * aStatistics,
                             void            * aTrans,
                             smcTableHeader  * aHeader,
                             UInt              aTableType)
{
    smLSN      sLsnNTA2;
    SChar    * sIndexRowPtr;
    SChar    * sSrc;
    scGRID   * sIndexSegGRID;
    smOID      sVarOID;
    SChar    * sRowPtr;
    UInt       sIndexHeaderSize;
    UInt       i;
    UInt       j;

    sIndexHeaderSize = smLayerCallback::getSizeOfIndexHeader();

    for(i = 0 ; i < SMC_MAX_INDEX_OID_CNT; i++)
    {
        if( aHeader->mIndexes[i].fstPieceOID == SM_NULL_OID)
        {
            continue;
        }//if

        // Begin NTA[-2-]
        sLsnNTA2 = smLayerCallback::getLstUndoNxtLSN( aTrans );

        // disk index�� free segment
        if (aTableType == SMI_TABLE_DISK)
        {
            IDE_ASSERT( smmManager::getOIDPtr( 
                                    SMI_ID_TABLESPACE_SYSTEM_MEMORY_DIC, 
                                    aHeader->mIndexes[i].fstPieceOID,
                                    (void**)&sIndexRowPtr )
                        == IDE_SUCCESS );

            sSrc  = sIndexRowPtr + ID_SIZEOF(smVCPieceHeader);

            for( j = 0; j < aHeader->mIndexes[i].length; j += sIndexHeaderSize,
                     sSrc += sIndexHeaderSize )
            {
                sIndexSegGRID = smLayerCallback::getIndexSegGRIDPtr( sSrc );

                if(SC_GRID_IS_NOT_NULL(*sIndexSegGRID))
                {
                    IDE_TEST( sdpSegment::freeIndexSeg4Entry(
                                          aStatistics,
                                          SC_MAKE_SPACE( *sIndexSegGRID ),
                                          aTrans,
                                          aHeader->mIndexes[i].fstPieceOID
                                          + j
                                          + ID_SIZEOF(smVCPieceHeader),
                                          SDR_MTX_LOGGING)
                              != IDE_SUCCESS );
                }//if
            }//for j
        }//if
        /* ------------------------------------------------
         * index ������ ������ variable slot�� ���� �� table header��
         * index ������ SM_NULL_OID�� ����
         * - SMR_LT_UPDATE�� SMR_SMC_TABLEHEADER_UPDATE_INDEX �α�
         * ----------------------------------------------*/
        IDE_TEST( smrUpdate::updateIndexAtTableHead( NULL, /* idvSQL* */
                                                     aTrans,
                                                     aHeader->mSelfOID,
                                                     &(aHeader->mIndexes[i]),
                                                     i,
                                                     SM_NULL_OID,
                                                     0,
                                                     SM_VCDESC_MODE_OUT )
                  != IDE_SUCCESS );


        sVarOID = aHeader->mIndexes[i].fstPieceOID;

        aHeader->mIndexes[i].fstPieceOID = SM_NULL_OID;
        aHeader->mIndexes[i].length      = 0;
        aHeader->mIndexes[i].flag        = SM_VCDESC_MODE_OUT;

        /* ------------------------------------------------
         * catalog table�� variable slot�� �����Ͽ� ��ȯ��.
         * ----------------------------------------------*/
        IDE_ASSERT( smmManager::getOIDPtr( 
                                    SMI_ID_TABLESPACE_SYSTEM_MEMORY_DIC, 
                                    sVarOID,
                                    (void**)&sRowPtr )
                    == IDE_SUCCESS );

        IDE_TEST( smpVarPageList::freeSlot(aTrans,
                                           SMI_ID_TABLESPACE_SYSTEM_MEMORY_DIC,
                                           SMC_CAT_TABLE->mVar.mMRDB,
                                           sVarOID,
                                           sRowPtr,
                                           &sLsnNTA2,
                                           SMP_TABLE_NORMAL)
                  != IDE_SUCCESS );


        // END   NTA[-2-]
    }//i
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/***********************************************************************
 * Description :
 * disk index�� ���� �����۾��� disk G.C�� ����
 * ó���ǹǷ�, �� �Լ������� skip �ϵ��� ó���Ѵ�.
 ***********************************************************************/
IDE_RC smcTable::dropIndexList( smcTableHeader * aHeader )
{

    UInt       i;
    UInt       sState = 0;
    UInt       sIndexHeaderSize;
    SChar    * sIndexHeader;
    smxTrans * sTrans;
    scGRID   * sIndexSegGRID;

    if(aHeader->mDropIndexLst != NULL)
    {
        sIndexHeaderSize = smLayerCallback::getSizeOfIndexHeader();

        IDE_TEST( smcTable::latchExclusive( aHeader ) != IDE_SUCCESS );
        sState = 1;

        for(i = 0, sIndexHeader = (SChar *)aHeader->mDropIndexLst;
            i < aHeader->mDropIndex;
            i++, sIndexHeader += sIndexHeaderSize)
        {
            IDE_TEST(smxTransMgr::alloc(&sTrans) != IDE_SUCCESS);
            sState = 2;

            IDE_ASSERT( sTrans->begin( NULL,
                                       ( SMI_TRANSACTION_REPL_NONE |
                                         SMI_COMMIT_WRITE_NOWAIT ),
                                       SMX_NOT_REPL_TX_ID )
                        == IDE_SUCCESS);
            sState = 3;

            if( SMI_TABLE_TYPE_IS_DISK( aHeader ) == ID_TRUE )
            {
                sIndexSegGRID  =  smLayerCallback::getIndexSegGRIDPtr( sIndexHeader );
                if (SC_GRID_IS_NOT_NULL(*sIndexSegGRID))
                {
                    IDE_TEST( sdpSegment::freeIndexSeg4Entry(
                                  NULL,
                                  SC_MAKE_SPACE(*sIndexSegGRID),
                                  sTrans,
                                  ((smnIndexHeader*)sIndexHeader)->mSelfOID,
                                  SDR_MTX_LOGGING)
                              != IDE_SUCCESS );
                }
            }

            IDE_TEST( smcTable::dropIndexPending( sIndexHeader )
                      != IDE_SUCCESS );

            sState = 2;
            IDE_TEST(sTrans->commit() != IDE_SUCCESS);

            sState = 1;
            IDE_TEST(smxTransMgr::freeTrans(sTrans) != IDE_SUCCESS);
        }

        IDE_TEST(mDropIdxPagePool.memfree(aHeader->mDropIndexLst)
                 != IDE_SUCCESS);

        aHeader->mDropIndexLst = NULL;
        aHeader->mDropIndex = 0;

        sState = 0;
        IDE_TEST( smcTable::unlatch( aHeader ) != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    switch ( sState )
    {
        case 3:
            IDE_ASSERT( sTrans->abort( ID_FALSE, /* aIsLegacyTrans */
                                       NULL      /* aLegacyTrans */ ) == IDE_SUCCESS );
        case 2:
            IDE_ASSERT( smxTransMgr::freeTrans(sTrans) == IDE_SUCCESS );
        case 1:
            IDE_ASSERT( smcTable::unlatch( aHeader ) == IDE_SUCCESS );
            break;
        default:
            break;
    }

    return IDE_FAILURE;

}

/***********************************************************************
 * Description : Table Header���� aIndex ��° Column�� Column Info��
 *               �� Column Info�� ��ġ�� ��Ÿ���� OID�� �����´�.
 ***********************************************************************/
smiColumn* smcTable::getColumnAndOID(const void* aTableHeader,
                                     const UInt  aIndex,
                                     smOID*      aOID)
{
    UInt                   sCurSize;
    const smcTableHeader * sTableHeader;
    smVCPieceHeader      * sVCPieceHeader;
    UInt                   sOffset;
    smiColumn            * sColumn = NULL;

    IDE_DASSERT( aTableHeader != NULL );

    sTableHeader = (const smcTableHeader*) aTableHeader;

    //���̺��� �÷������� ���� ����  variable slot.
    IDE_ASSERT( smmManager::getOIDPtr( 
                                SMI_ID_TABLESPACE_SYSTEM_MEMORY_DIC, 
                                sTableHeader->mColumns.fstPieceOID,
                                (void**)&sVCPieceHeader )
                == IDE_SUCCESS );
    sOffset = sTableHeader->mColumnSize * aIndex;
    *aOID = sTableHeader->mColumns.fstPieceOID;

    /* BUG-22367: ���� �÷� ������ ���� ���̺� ������ ������ ����
     *
     * sCurSize < sOffset => sCurSize <= sOffset , = ������ ������
     * */
    for( sCurSize = sVCPieceHeader->length; sCurSize <= sOffset; )
    {
        sOffset -= sCurSize;

        // next variable slot���� �̵�.
        if( sVCPieceHeader->nxtPieceOID == SM_NULL_OID )
        {
            ideLog::log( IDE_SM_0,
                         "Index:    %u\n"
                         "CurSize:  %u\n"
                         "Offset:   %u\n",
                         aIndex,
                         sCurSize,
                         sOffset );

            ideLog::log( IDE_SM_0, "[ Table header ]" );
            ideLog::logMem( IDE_SM_0,
                            (UChar*)aTableHeader,
                            ID_SIZEOF(smcTableHeader) );

            sColumn = NULL;

            IDE_CONT( SKIP );
        }
        *aOID = sVCPieceHeader->nxtPieceOID;
        IDE_ASSERT( smmManager::getOIDPtr( 
                                    SMI_ID_TABLESPACE_SYSTEM_MEMORY_DIC, 
                                    sVCPieceHeader->nxtPieceOID,
                                    (void**)&sVCPieceHeader )
                    == IDE_SUCCESS );

        sCurSize = sVCPieceHeader->length;
    }//for

    /* Column Info�� ��Ȯ�� ��ġ�� OID�� ��ȯ�Ѵ�. */
    *aOID = *aOID + ID_SIZEOF(smVCPieceHeader) + sOffset;

    sColumn = (smiColumn*)
        ((UChar*) sVCPieceHeader + ID_SIZEOF(smVCPieceHeader)+ sOffset);

    IDE_EXCEPTION_CONT( SKIP );

    /*
     * BUG-26933 [5.3.3 release] CodeSonar::Null Pointer Dereference (2)
     */
    IDE_DASSERT( sColumn != NULL );

    if( aIndex !=
        ( sColumn->id & SMI_COLUMN_ID_MASK ) )
    {
        ideLog::log( IDE_DUMP_0,
                     "ColumnID( %u ) != Index( %u )\n",
                     sColumn->id & SMI_COLUMN_ID_MASK,
                     aIndex );

        ideLog::logMem( IDE_DUMP_0,
                        (UChar*)sColumn,
                        ID_SIZEOF( smiColumn ) );

        ideLog::logMem( IDE_DUMP_0,
                        (UChar*)sTableHeader,
                        ID_SIZEOF( smcTableHeader ) );

        IDE_ASSERT( 0 );
    }

    return sColumn;
}

UInt smcTable::getColumnIndex( UInt aColumnID )
{
    return ( aColumnID & SMI_COLUMN_ID_MASK );
}

UInt smcTable::getColumnSize(void *aTableHeader)
{
    IDE_DASSERT(aTableHeader!=NULL);
    return ((smcTableHeader *)aTableHeader)->mColumnSize;
}


UInt smcTable::getColumnCount(const void *aTableHeader)
{
    IDE_DASSERT(aTableHeader!=NULL);
    return ((smcTableHeader *)aTableHeader)->mColumnCount;
}


/***********************************************************************
 *
 * Description :
 *  lob column�� ������ ��ȯ�Ѵ�.
 *
 *  aTableHeader - [IN] ���̺� ���
 *
 **********************************************************************/
UInt smcTable::getLobColumnCount(const void *aTableHeader)
{
    IDE_DASSERT(aTableHeader!=NULL);

#ifdef DEBUG
    const smiColumn *sColumn;

    UInt       sColumnCnt = getColumnCount( aTableHeader );
    UInt       i;
    UInt       sLobColumnCnt = 0;

    for( i = 0; i < sColumnCnt; i++ )
    {
        sColumn = smcTable::getColumn( aTableHeader, i );

        if( ( sColumn->flag & SMI_COLUMN_TYPE_MASK ) ==
           SMI_COLUMN_TYPE_LOB )
        {
            sLobColumnCnt++;
        }
    }

    IDE_DASSERT( ((smcTableHeader *)aTableHeader)->mLobColumnCount
                 == sLobColumnCnt );
#endif

    return ((smcTableHeader *)aTableHeader)->mLobColumnCount;
}

/***********************************************************************
 *
 * Description :
 *  lob column�� ��ȯ�Ѵ�.
 *
 *  aTableHeader - [IN] ���̺� ���
 *  aSpaceID     - [IN] Lob Segment Space ID
 *  aSegPID      - [IN] Lob segment PID
 *
 **********************************************************************/
const smiColumn* smcTable::getLobColumn( const void *aTableHeader,
                                         scSpaceID   aSpaceID,
                                         scPageID    aSegPID )
{
    const smiColumn *sColumn = NULL;
    const smiColumn *sLobColumn = NULL;
    UInt             sColumnCnt;
    scGRID           sSegGRID;
    UInt             i;

    IDE_DASSERT(aTableHeader!=NULL);

    SC_MAKE_GRID( sSegGRID,
                  aSpaceID,
                  aSegPID,
                  0 );

    sColumnCnt = getColumnCount( aTableHeader );

    for( i = 0; i < sColumnCnt; i++ )
    {
        sColumn = smcTable::getColumn( aTableHeader, i );

        if( ( sColumn->flag & SMI_COLUMN_TYPE_MASK ) ==  SMI_COLUMN_TYPE_LOB )
        {
            if( SC_GRID_IS_EQUAL(sSegGRID, sColumn->colSeg) )
            {
                sLobColumn = sColumn;
                break;
            }
        }
    }

    return sLobColumn;
}

/* ��ũ Page List Entry�� ��ȯ�Ѵ�. */
void* smcTable::getDiskPageListEntry(void *aTableHeader)
{
    IDE_ASSERT(aTableHeader!=NULL);

    return (void*)&(((smcTableHeader *)aTableHeader)->mFixed.mDRDB);
}

/* ��ũ Page List Entry�� ��ȯ�Ѵ�. */
void* smcTable::getDiskPageListEntry( smOID  aTableOID )
{
    smcTableHeader *sTableHeader;

    IDE_ASSERT( aTableOID != SM_NULL_OID );

    IDE_ASSERT( smcTable::getTableHeaderFromOID( aTableOID,
                                                 (void**)&sTableHeader )
                == IDE_SUCCESS );

    return getDiskPageListEntry( sTableHeader );
}


ULong smcTable::getMaxRows(void *aTableHeader)
{

    IDE_DASSERT(aTableHeader!=NULL);
    return ((smcTableHeader *)aTableHeader)->mMaxRow;

}

smOID smcTable::getTableOID(const void *aTableHeader)
{

    IDE_DASSERT(aTableHeader!=NULL);
    return ((smcTableHeader *)aTableHeader)->mSelfOID;

}

/***********************************************************************
 * Description :
 * db refine�������� table backup file�鿡 ���� ���Ÿ� ó���ϴ� �Լ���
 * disk table������ ������ �ʿ����.
 ***********************************************************************/
IDE_RC smcTable::deleteAllTableBackup()
{
    DIR                  *sDIR       = NULL;
    struct  dirent       *sDirEnt    = NULL;
    struct  dirent       *sResDirEnt = NULL;
    SInt                  sOffset;
    SChar                 sFullFileName[SM_MAX_FILE_NAME];
    SChar                 sFileName[SM_MAX_FILE_NAME];
    SInt                  sRC;
    idBool                sCheck1;
    idBool                sCheck2;

    /* BUG-16161: Add Column�� ������ �ٽ� �ٽ� Add Column�� �����ϸ�
     * Session�� Hang���·� �����ϴ�.: Restart Recovery�� Backup������
     * �̿��� Transaction�� �αװ� Disk�� Sync�������� ���¿��� ���⼭
     * Backup������ ����ٸ�, Failure�߻��� �ش� Abort�� Transaction��
     * �ٽ� Abort�Ҽ��� ����. ������ ���⼭ log Flush�� �����Ͽ� Abort��
     * ������ Disk�� �ݿ��Ǵ� ���� �����Ͽ��� �Ѵ�.
     * */
    IDE_TEST( smrLogMgr::syncToLstLSN( SMR_LOG_SYNC_BY_REF )
              != IDE_SUCCESS );

    /* smcTable_deleteAllTableBackup_malloc_DirEnt.tc */
    /* smcTable_deleteAllTableBackup.tc */
    IDU_FIT_POINT_RAISE( "smcTable::deleteAllTableBackup::malloc::DirEnt",
                          insufficient_memory );
    IDE_TEST_RAISE( iduMemMgr::malloc( IDU_MEM_SM_SMC,
                                       ID_SIZEOF(struct dirent) + SM_MAX_FILE_NAME,
                                       (void**)&(sDirEnt),
                                       IDU_MEM_FORCE ) != IDE_SUCCESS,
                    insufficient_memory );

    /* smcTable_deleteAllTableBackup.tc */
    IDU_FIT_POINT_RAISE( "smcTable::deleteAllTableBackup::opendir", err_open_dir );
    sDIR = idf::opendir(getBackupDir());
    IDE_TEST_RAISE(sDIR == NULL, err_open_dir);

    errno = 0;
    /* smcTable_deleteAllTableBackup.tc */
    IDU_FIT_POINT_RAISE( "smcTable::deleteAllTableBackup::readdir_r", err_read_dir );
    sRC = idf::readdir_r(sDIR, sDirEnt, &sResDirEnt) ;
    IDE_TEST_RAISE( (sRC != 0) && (errno != 0), err_read_dir );

    while(sResDirEnt != NULL)
    {
        idlOS::strcpy(sFileName, (const char*)sResDirEnt->d_name);
        if (idlOS::strcmp(sFileName, ".") == 0 || idlOS::strcmp(sFileName, "..") == 0)
        {
            /*
             * BUG-31529  [sm] errno must be initialized before calling readdir_r.
             */
	    errno = 0;
            sRC = idf::readdir_r(sDIR, sDirEnt, &sResDirEnt);
            IDE_TEST_RAISE( (sRC != 0) && (errno != 0), err_read_dir );
            errno = 0;

            continue;
        }

        sOffset = (SInt)idlOS::strlen(sFileName) - 4;

        if(sOffset > 4)
        {
            if(idlOS::strncmp(sFileName + sOffset, SM_TABLE_BACKUP_EXT, 4) == 0)
            {
                idlOS::snprintf(sFullFileName, SM_MAX_FILE_NAME, "%s%c%s",
                                getBackupDir(), IDL_FILE_SEPARATOR,sFileName);

                // BUG-20048
                // ������ ����� �а�, ��Ƽ���̽� ��������� �´����� �˻��Ѵ�.
                IDE_TEST( IDE_SUCCESS !=
                          smiTableBackup::isBackupFile(sFullFileName, &sCheck1) );
                IDE_TEST( IDE_SUCCESS !=
                          smiVolTableBackup::isBackupFile(sFullFileName, &sCheck2) );

                // ��Ƽ���̽� ��������� ������ �����Ѵ�.
                if( (ID_TRUE == sCheck1) || (ID_TRUE == sCheck2) )
                {
                    while(1)
                    {
                        IDE_TEST_RAISE(idf::unlink(sFullFileName) != 0,
                                       err_file_unlink);
                        //==========================================================
                        // To Fix BUG-13924
                        //==========================================================
                        ideLog::log(SM_TRC_LOG_LEVEL_MRECORD,
                                    SM_TRC_MRECORD_FILE_UNLINK,
                                    sFullFileName);
                        break;
                    }//while
                }
            }
        }

        errno = 0;
        sRC = idf::readdir_r(sDIR, sDirEnt, &sResDirEnt);
        IDE_TEST_RAISE( (sRC != 0) && (errno != 0), err_read_dir );
        errno = 0;
    }//while

    idf::closedir( sDIR );
    sDIR = NULL;

    IDE_TEST( iduMemMgr::free( sDirEnt ) != IDE_SUCCESS );
    sDirEnt = NULL;

    return IDE_SUCCESS;

    IDE_EXCEPTION(err_file_unlink);
    {
        IDE_SET(ideSetErrorCode(smERR_FATAL_FileDelete, sFullFileName));
    }
    IDE_EXCEPTION(err_open_dir);
    {
        IDE_SET( ideSetErrorCode( smERR_FATAL_CannotOpenDir, getBackupDir() ) );
    }
    IDE_EXCEPTION(err_read_dir);
    {
        IDE_SET( ideSetErrorCode( smERR_FATAL_CannotReadDir, getBackupDir() ) );
    }
    IDE_EXCEPTION( insufficient_memory );
    {
        IDE_SET( ideSetErrorCode( idERR_ABORT_InsufficientMemory ) );
    }
    IDE_EXCEPTION_END;

    if ( sDIR != NULL )
    {
        idf::closedir( sDIR );
        sDIR = NULL;
    }
    else
    {
        /* Nothing to do */
    }

    if ( sDirEnt != NULL )
    {
        (void)iduMemMgr::free( sDirEnt );
        sDirEnt = NULL;
    }
    else
    {
        /* Nothing to do */
    }

    return IDE_FAILURE;
}

/***********************************************************************
 * Description :  memory table�� onlinebackup�� table backup file�� ����
 * backup�� ������ ó���ϴ� �Լ��� disk table�� ���Ͽ� ������ �ʿ����
 **********************************************************************/
IDE_RC smcTable::copyAllTableBackup(SChar      * aSrcDir, 
                                    SChar      * aTargetDir)
{

    iduFile            sFile;
    struct  dirent    *sDirEnt    = NULL;
    struct  dirent    *sResDirEnt = NULL;
    DIR               *sDIR       = NULL;
    SInt               sOffset;
    SChar              sStrFullFileName[SM_MAX_FILE_NAME];
    SChar              sFileName[SM_MAX_FILE_NAME];
    SInt               sRC;

    /* smcTable_copyAllTableBackup_malloc_DirEnt.tc */
    IDU_FIT_POINT("smcTable::copyAllTableBackup::malloc::DirEnt");
    IDE_TEST(iduMemMgr::malloc(IDU_MEM_SM_SMC,
                               ID_SIZEOF(struct dirent) + SM_MAX_FILE_NAME,
                               (void**)&(sDirEnt),
                               IDU_MEM_FORCE)
             != IDE_SUCCESS);

    sDIR = idf::opendir(aSrcDir);
    IDE_TEST_RAISE(sDIR == NULL, err_open_dir);
    IDE_TEST(sFile.initialize( IDU_MEM_SM_SMC,
                               1, /* Max Open FD Count */
                               IDU_FIO_STAT_OFF,
                               IDV_WAIT_INDEX_NULL ) != IDE_SUCCESS);

    errno = 0;
    sRC = idf::readdir_r(sDIR, sDirEnt, &sResDirEnt) ;
    IDE_TEST_RAISE( (sRC != 0) && (errno != 0), err_read_dir );
    errno = 0;

    while(sResDirEnt != NULL)
    {
        idlOS::strcpy(sFileName, (const char*)sResDirEnt->d_name);
        if (idlOS::strcmp(sFileName, ".") == 0 ||
            idlOS::strcmp(sFileName, "..") == 0)
        {
            errno = 0;
            sRC = idf::readdir_r(sDIR, sDirEnt, &sResDirEnt);
            IDE_TEST_RAISE( (sRC != 0) && (errno != 0), err_read_dir );
            errno = 0;

            continue;
        }

        sOffset = (SInt)idlOS::strlen(sFileName) - 4;
        if(sOffset > 4)
        {
            if(idlOS::strncmp(sFileName + sOffset,
                              SM_TABLE_BACKUP_EXT, 4) == 0)
            {
                idlOS::snprintf(sStrFullFileName, SM_MAX_FILE_NAME, "%s%c%s",
                                aSrcDir, IDL_FILE_SEPARATOR,
                                sFileName);
                IDE_TEST(sFile.setFileName(sStrFullFileName)!= IDE_SUCCESS);
                IDE_TEST(sFile.open()!= IDE_SUCCESS);
                idlOS::snprintf(sStrFullFileName, SM_MAX_FILE_NAME, "%s%c%s",
                               aTargetDir, IDL_FILE_SEPARATOR,
                               sFileName);

                IDE_TEST(sFile.copy(NULL, /* idvSQL* */
                                    sStrFullFileName,
                                    ID_FALSE)!= IDE_SUCCESS);

                IDE_TEST(sFile.close()!= IDE_SUCCESS);
            }
        }
        errno = 0;
        sRC = idf::readdir_r(sDIR, sDirEnt, &sResDirEnt);
        IDE_TEST_RAISE( (sRC != 0) && (errno != 0), err_read_dir );
        errno = 0;
    }
    IDE_TEST(sFile.destroy() != IDE_SUCCESS);
    idf::closedir(sDIR);
    sDIR = NULL;

    if(sDirEnt != NULL)
    {
        IDE_TEST(iduMemMgr::free(sDirEnt) != IDE_SUCCESS);
        sDirEnt = NULL;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION(err_open_dir);
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_NotDir));
    }
    IDE_EXCEPTION(err_read_dir);
    {
        IDE_SET( ideSetErrorCode( smERR_FATAL_CannotReadDir, aSrcDir ) );
    }
    IDE_EXCEPTION_END;

    if(sDirEnt != NULL)
    {
        (void)iduMemMgr::free(sDirEnt);
        sDirEnt = NULL;
    }

    // fix BUG-25556 : [codeSonar] closedir �߰�.
    if(sDIR != NULL)
    {
        idf::closedir(sDIR);
    }

    return IDE_FAILURE;

}

/***********************************************************************
 * Description : FOR A5
 * fix - BUG-25611
 * memory table�� ���� LogicalUndo��, ���� Refine���� ���� �����̱� ������
 * �ӽ÷� ���̺��� Refine���ش�.
 *
 * �ݵ�� finalizeTable4LogicalUndo�� ������ ȣ��Ǿ�� �Ѵ�.
 ***********************************************************************/
IDE_RC smcTable::prepare4LogicalUndo( smcTableHeader * aTable )
{
    IDE_TEST( initLockAndRuntimeItem( aTable ) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : FOR A5
 * fix - BUG-25611
 * memory table�� ���� LogicalUndo��, �ӽ÷� Refine�� ���̺��� ��������
 * �����Ѵ�.
 *
 * ���� ���
 * 1) FreePage(Fixed & Variable) - �ʱ�ȭ��
 * 2) AllocatePage���� Scanlist - �ʱ�ȭ��
 * 3) Private Page - Table���� ��ȯ��
 * 4) RuntimeEntry - �ʱ�ȭ��
 *
 * �ݵ�� prepare4LogicalUndo�� ������ ȣ��Ǿ�� �Ѵ�.
 ***********************************************************************/
IDE_RC smcTable::finalizeTable4LogicalUndo(smcTableHeader * aTable,
                                           void           * aTrans )
{
    smxTrans       *sTrans;

    sTrans = (smxTrans*)aTrans;

    // 1) FreePage(Fixed & Variable) - �ʱ�ȭ��
    initAllFreePageHeader(aTable);
    // 2) AllocatePage���� Scanlist - �ʱ�ȭ��
    IDE_TEST( smpFixedPageList::resetScanList( aTable->mSpaceID,
                                             &(aTable->mFixed.mMRDB))
                     != IDE_SUCCESS);
    // 3) Private Page - Table���� ��ȯ��
    IDE_TEST( sTrans->finAndInitPrivatePageList() != IDE_SUCCESS );
    // 4) RuntimeEntry - �ʱ�ȭ��
    IDE_TEST (finLockAndRuntimeItem(aTable) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}




/***********************************************************************
 * Table�� �Ҵ�� ��� FreePageHeader�� �ʱ�ȭ
 *
 * aTableHeader - [IN] Table�� Header
 ***********************************************************************/
void smcTable::initAllFreePageHeader( smcTableHeader* aTableHeader )
{
    UInt i;

    switch( SMI_GET_TABLE_TYPE( aTableHeader ) )
    {
        case SMI_TABLE_MEMORY :
        case SMI_TABLE_META :
            smpFreePageList::initAllFreePageHeader(aTableHeader->mSpaceID,
                                                   &(aTableHeader->mFixed.mMRDB));

            for( i = 0;
                 i < SM_VAR_PAGE_LIST_COUNT ;
                 i++ )
            {
                smpFreePageList::initAllFreePageHeader(aTableHeader->mSpaceID,
                                                       &(aTableHeader->mVar.mMRDB[i]));
            }
            break;
        case SMI_TABLE_VOLATILE :
            svpFreePageList::initAllFreePageHeader(aTableHeader->mSpaceID,
                                                   &(aTableHeader->mFixed.mVRDB));

            for( i = 0;
                 i < SM_VAR_PAGE_LIST_COUNT ;
                 i++ )
            {
                svpFreePageList::initAllFreePageHeader(aTableHeader->mSpaceID,
                                                       &(aTableHeader->mVar.mVRDB[i]));
            }
            break;
        default :
            IDE_ASSERT(0);
            break;
    }

    return;
}

/***********************************************************************
 * Description : table�� record ������ ��ȯ�Ѵ�.
 * table Ÿ�Կ� ���� page list entry�� record ����
 * ����ϴ� �Լ��� ���� ȣ��
 * - 2nd code design
 *   + table Ÿ�Կ� ���� �����ϴ� record�� ������ ���Ѵ�.
 *     if (memory table)
 *        - fixed page list entry�� record ���� ����
 *     else if (disk table or temporary table)
 *        - disk page list entry�� record ���� ����
 *     endif
 ***********************************************************************/
IDE_RC smcTable::getRecordCount( smcTableHeader* aHeader,
                                 ULong         * aRecordCount )
{
    UInt sTableType;

    IDE_DASSERT( aHeader != NULL );
    IDE_DASSERT( aRecordCount != NULL );

    sTableType = SMI_GET_TABLE_TYPE( aHeader );

    switch (sTableType)
    {
        case SMI_TABLE_MEMORY :
        case SMI_TABLE_META :
        case SMI_TABLE_REMOTE:
            /* memory table�� fixed page list entry�� record ���� ��� */
           *aRecordCount = smpFixedPageList::getRecordCount( &(aHeader->mFixed.mMRDB) );
            break;
        case SMI_TABLE_DISK :
            /* disk table�� page list entry�� record ���� ��� */
            IDE_TEST( sdpPageList::getRecordCount(
                          NULL, /* idvSQL* */
                          &(aHeader->mFixed.mDRDB),
                          aRecordCount,
                          (sTableType == SMI_TABLE_DISK ? ID_TRUE : ID_FALSE))
                      != IDE_SUCCESS);
            break;
        case SMI_TABLE_VOLATILE :
            /* volatile table�� fixed page list entry�� record ���� ��� */
            *aRecordCount = svpFixedPageList::getRecordCount( &(aHeader->mFixed.mVRDB) );
            break;
        case SMI_TABLE_TEMP_LEGACY :
        default :
            /* IDE_ASSERT(0); */
            break;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : table�� record ������ �����Ѵ�.
 *
 * aHeader        - [IN] Table Header
 * aRecordCount   - [IN] Record Count
 ***********************************************************************/
IDE_RC smcTable::setRecordCount(smcTableHeader * aHeader,
                                ULong            aRecordCount)
{
    IDE_DASSERT( aHeader != NULL );

    switch( SMI_GET_TABLE_TYPE( aHeader ) )
    {
        case SMI_TABLE_MEMORY :
        case SMI_TABLE_META :
            smpFixedPageList::setRecordCount(&(aHeader->mFixed.mMRDB),
                                             aRecordCount);
            break;
        case SMI_TABLE_VOLATILE :
            svpFixedPageList::setRecordCount(&(aHeader->mFixed.mVRDB),
                                             aRecordCount);
            break;

        case SMI_TABLE_DISK :
            /* �� �Լ��� Memory Table�� Record ������
             * ���濡�� ���δ�. */
            IDE_ASSERT(0 );

        case SMI_TABLE_TEMP_LEGACY :
        default :
            IDE_ASSERT(0);
            break;
    }

    return IDE_SUCCESS;
}

const void * smcTable::getTableIndexByID( void       * aHeader,
                                          const UInt   aId )
{
    smcTableHeader  *sHeader;
    smnIndexHeader  *sIndexHeader = NULL;
    smVCPieceHeader *sVCPieceHdr  = NULL;
    UInt             i = 0;
    UInt             sCurSize = 0;

    sHeader = (smcTableHeader*) aHeader;
    for(i = 0 ; i < SMC_MAX_INDEX_OID_CNT; i++)
    {
        if(sHeader->mIndexes[i].fstPieceOID == SM_NULL_OID)
        {
            continue;
        }

        IDE_TEST( sHeader->mIndexes[i].length <= 0 );

        IDE_ASSERT( smmManager::getOIDPtr(
                                SMI_ID_TABLESPACE_SYSTEM_MEMORY_DIC,
                                sHeader->mIndexes[i].fstPieceOID,
                                (void**)&sVCPieceHdr )
                    == IDE_SUCCESS );

        sCurSize = 0;
        while( sCurSize < sHeader->mIndexes[i].length )
        {
            sIndexHeader = (smnIndexHeader*)((UChar*)sVCPieceHdr +
                                             ID_SIZEOF(smVCPieceHeader) +
                                             sCurSize);
            if( sIndexHeader->mId == aId )
            {
                break;
            }
            sCurSize += smLayerCallback::getSizeOfIndexHeader();
        }

        /* BUG-34018 ���� �� ���̺��� Index Header ������ 47���� �Ѿ��,
         * ������ Index Id�� �������� Index Header�� ã�� �� ����. */
        if( sIndexHeader->mId == aId )
        {
            break;
        }
    }//for i;

    IDE_TEST( sIndexHeader      == NULL );
    IDE_TEST( sIndexHeader->mId != aId );

    return sIndexHeader;

    IDE_EXCEPTION_END;

    ideLog::log( IDE_SERVER_0,
                 "Index not found ("
                 "Table OID : %u, "
                 "Index ID : %u, "
                 "Index array ID : %u )\n",
                 sHeader->mSelfOID,
                 aId,
                 i );

    ideLog::log( IDE_SERVER_0,
                 "Index array\n" );

    for(i = 0 ; i < SMC_MAX_INDEX_OID_CNT; i++)
    {
        ideLog::log( IDE_SERVER_0,
                     "[%u] "
                     "fstPieceOID : %u "
                     "Length : %u\n",
                     i,
                     sHeader->mIndexes[i].fstPieceOID,
                     sHeader->mIndexes[i].length );

        if(sHeader->mIndexes[i].fstPieceOID == SM_NULL_OID)
        {
            continue;
        }

        IDE_ASSERT( smmManager::getOIDPtr(
                        SMI_ID_TABLESPACE_SYSTEM_MEMORY_DIC,
                        sHeader->mIndexes[i].fstPieceOID,
                        (void**)&sVCPieceHdr )
                    == IDE_SUCCESS );

        sCurSize = 0;
        while( sCurSize < sHeader->mIndexes[i].length )
        {
            sIndexHeader = (smnIndexHeader*)((UChar*)sVCPieceHdr +
                                             ID_SIZEOF(smVCPieceHeader) +
                                             sCurSize);
            ideLog::log( IDE_SERVER_0,
                         "Index ID : %u\n",
                         sIndexHeader->mId );

            sCurSize += smLayerCallback::getSizeOfIndexHeader();
        }
    }

    IDE_ASSERT( 0 );

    return NULL;
}

/***********************************************************************
 * Description : �ش� ������ ���� �ɶ����� ���
 ***********************************************************************/
IDE_RC smcTable::waitForFileDelete( idvSQL *aStatistics, SChar* aStrFileName )
{
    UInt        sCheckSessionCnt = 0;
    idvTime     sInitTime;
    idvTime     sCurTime;
     
    IDV_TIME_GET( &sInitTime );
    while(1)
    {
        IDU_FIT_POINT( "1.BUG-38254@smcTable::waitForFileDelete" );

        if(idf::access(aStrFileName, F_OK) != 0 )
        {
            break;
        }

        idlOS::sleep(3);

        sCheckSessionCnt++;
        if( sCheckSessionCnt >= MAX_CHECK_CNT_FOR_SESSION )
        {
            IDE_TEST( iduCheckSessionEvent( aStatistics )
                      != IDE_SUCCESS );
            sCheckSessionCnt = 0;

            IDV_TIME_GET( &sCurTime );

            IDE_TEST( smuProperty::getTableBackupTimeOut() <
                      (IDV_TIME_DIFF_MICRO(&sInitTime, &sCurTime)/1000000 ));
        }
    }
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : table�� ��� index�� enable �Ǵ� disable ���·� ����
 ***********************************************************************/
IDE_RC smcTable::setUseFlagOfAllIndex(void            * aTrans,
                                      smcTableHeader  * aHeader,
                                      UInt              aFlag)
{
    UInt             i;
    UInt             j;
    UInt             sState = 0;
    UInt             sFlag  = 0;
    SChar          * sCurIndexHeader;
    scPageID         sPageID = SC_NULL_PID;
    UInt             sIndexHeaderSize;

    sIndexHeaderSize = smLayerCallback::getSizeOfIndexHeader();

    for(i = 0 ; i < SMC_MAX_INDEX_OID_CNT ; i++)
    {
        if( aHeader->mIndexes[i].fstPieceOID == SM_NULL_OID)
        {
            continue;
        }

        sPageID = SM_MAKE_PID( aHeader->mIndexes[i].fstPieceOID );

        IDE_ASSERT( smmManager::getOIDPtr( 
                        SMI_ID_TABLESPACE_SYSTEM_MEMORY_DIC, 
                        aHeader->mIndexes[i].fstPieceOID + ID_SIZEOF(smVCPieceHeader),
                        (void**)&sCurIndexHeader )
                    == IDE_SUCCESS );

        for( j = 0;
             j < aHeader->mIndexes[i].length;
             j += sIndexHeaderSize,
                 sCurIndexHeader += sIndexHeaderSize )
        {
            sFlag = smLayerCallback::getFlagOfIndexHeader( sCurIndexHeader ) & ~SMI_INDEX_USE_MASK;

            sFlag |= aFlag;

            IDE_TEST( smrUpdate::setIndexHeaderFlag( NULL, /* idvSQL* */
                                                     aTrans,
                                                     aHeader->mIndexes[i].fstPieceOID,
                                                     j + ID_SIZEOF(smVCPieceHeader),
                                                     smLayerCallback::getFlagOfIndexHeader( sCurIndexHeader ),
                                                     sFlag )
                      != IDE_SUCCESS );


            smLayerCallback::setFlagOfIndexHeader( sCurIndexHeader, sFlag );
            sState = 1;
        }

        sState = 0;
        IDE_TEST( smmDirtyPageMgr::insDirtyPage(
                              SMI_ID_TABLESPACE_SYSTEM_MEMORY_DIC,
                              sPageID) 
                  != IDE_SUCCESS );
    }

    if(aFlag == SMI_INDEX_USE_DISABLE)
    {
       sFlag = aHeader->mFlag;
       sFlag &= ~(SMI_TABLE_DISABLE_ALL_INDEX_MASK);
       sFlag |= SMI_TABLE_DISABLE_ALL_INDEX;
    }
    else if(aFlag == SMI_INDEX_USE_ENABLE)
    {
       sFlag = aHeader->mFlag;
       sFlag &= ~(SMI_TABLE_DISABLE_ALL_INDEX_MASK);
       sFlag |= SMI_TABLE_ENABLE_ALL_INDEX;
    }

    IDE_TEST( smrUpdate::updateFlagAtTableHead(NULL, /* idvSQL* */
                                               aTrans,
                                               aHeader,
                                               sFlag)
              != IDE_SUCCESS );


    aHeader->mFlag = sFlag;

    IDE_TEST( smmDirtyPageMgr::insDirtyPage(
                                  SMI_ID_TABLESPACE_SYSTEM_MEMORY_DIC,
                                  SM_MAKE_PID(aHeader->mSelfOID))
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    // fix BUG-14252
    if (sState != 0)
    {
       (void)smmDirtyPageMgr::insDirtyPage(
                                   SMI_ID_TABLESPACE_SYSTEM_MEMORY_DIC, 
                                   sPageID);
    }

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : table Ÿ�Կ� ���� table�� �Ҵ��� page ������ ���Ѵ�.
 * - 2nd code design
 *   + table Ÿ�Կ� ���� table ���� �Ҵ�� page�� �Ѱ��� ��ȯ
 *     if (memory table)
 *        - fixed page list entry�� �Ҵ�� page ������ ����
 *        - variable page list entry���� �Ҵ�� page ���� ����
 *     else if (disk table or temporary table)
 *        - disk page list entry�� �Ҵ�� page ���� ����
 *     endif
 ***********************************************************************/
IDE_RC smcTable::getTablePageCount( void * aHeader, ULong* aPageCnt )
{
    ULong           sNeedPage = 0;
    smcTableHeader* sHeader = (smcTableHeader*)aHeader;

    switch( SMI_GET_TABLE_TYPE( sHeader ) )
    {
        case SMI_TABLE_MEMORY :
        case SMI_TABLE_META :
            /* memory table�� �Ҵ�� page ���� ���ϱ� */
            sNeedPage = smpManager::getAllocPageCount(&(sHeader->mFixed.mMRDB))
                + smpManager::getAllocPageCount(sHeader->mVar.mMRDB);
            break;
        case SMI_TABLE_DISK :
            /* disk table�� �Ҵ�� page ���� ���ϱ� */
            /* BUGBUG ������ ������ Return�ϴ� �Լ� ����. */
            IDE_TEST( sdpPageList::getAllocPageCnt(
                          NULL, /* idvSQL* */
                          sHeader->mSpaceID,
                          &( sHeader->mFixed.mDRDB.mSegDesc ),
                          &sNeedPage )
                      != IDE_SUCCESS );
            break;
        case SMI_TABLE_VOLATILE :
            /* memory table�� �Ҵ�� page ���� ���ϱ� */
            sNeedPage = svpManager::getAllocPageCount(&(sHeader->mFixed.mVRDB))
                + svpManager::getAllocPageCount(sHeader->mVar.mVRDB);
            break;
        case SMI_TABLE_TEMP_LEGACY :
        default :
            IDE_ASSERT(0);
            break;
    }

    *aPageCnt = sNeedPage;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : �� index ���� ����
 ***********************************************************************/
void smcTable::addToTotalIndexCount( UInt aCount )
{
    mTotalIndex += aCount;

    return;
}

/***********************************************************************
 * Description : table header ���� ��ȯ (logging�� �ʿ��� ����)
 ***********************************************************************/
void smcTable::getTableHeaderInfo(void         * aHeader,
                                  scPageID     * aPageID,
                                  scOffset     * aOffset,
                                  smVCDesc     * aColumnsVCDesc,
                                  smVCDesc     * aInfoVCDesc,
                                  UInt         * aHeaderColumnSize,
                                  UInt         * aHeaderFlag,
                                  ULong        * aHeaderMaxRow,
                                  UInt         * aHeaderParallelDegree)
{
    smcTableHeader * sTableHeader;

    sTableHeader = (smcTableHeader*)aHeader;

    *aPageID               = SM_MAKE_PID(sTableHeader->mSelfOID);
    *aOffset               = SM_MAKE_OFFSET(sTableHeader->mSelfOID)
                             + SMP_SLOT_HEADER_SIZE;
    *aColumnsVCDesc        = sTableHeader->mColumns;
    *aInfoVCDesc           = sTableHeader->mInfo;
    *aHeaderColumnSize     = sTableHeader->mColumnSize;
    *aHeaderFlag           = sTableHeader->mFlag;
    *aHeaderMaxRow         = sTableHeader->mMaxRow;
    *aHeaderParallelDegree = sTableHeader->mParallelDegree;

    return;
}

/***********************************************************************
 * Description : table header�� flag ��ȯ (logging�� �ʿ��� ����)
 ***********************************************************************/
void smcTable::getTableHeaderFlag(void         * aHeader,
                                  scPageID     * aPageID,
                                  scOffset     * aOffset,
                                  UInt         * aHeaderFlag)
{

    smcTableHeader * sTableHeader;

    sTableHeader = (smcTableHeader*)aHeader;

    *aPageID     = SM_MAKE_PID(sTableHeader->mSelfOID);
    *aOffset     = SM_MAKE_OFFSET(sTableHeader->mSelfOID)
                   + SMP_SLOT_HEADER_SIZE;
    *aHeaderFlag = sTableHeader->mFlag;

    return;

}

/***********************************************************************
 * Description : table header�� flag ��ȯ
 ***********************************************************************/
UInt smcTable::getTableHeaderFlagOnly(void         * aHeader)
{
    smcTableHeader * sTableHeader;

    sTableHeader = (smcTableHeader*)aHeader;

    return sTableHeader->mFlag;

}

/***********************************************************************
 * Description : table header�� flag ��ȯ (logging�� �ʿ��� ����)
 ***********************************************************************/
void smcTable::getTableIsConsistent( void         * aHeader,
                                     scPageID     * aPageID,
                                     scOffset     * aOffset,
                                     idBool       * aFlag )
{
    smcTableHeader * sTableHeader;

    sTableHeader = (smcTableHeader*)aHeader;

    *aPageID     = SM_MAKE_PID(sTableHeader->mSelfOID);
    *aOffset     = SM_MAKE_OFFSET(sTableHeader->mSelfOID)
                   + SMP_SLOT_HEADER_SIZE;
    *aFlag       = sTableHeader->mIsConsistent;

    return;

}


/***********************************************************************
 * Description : create index ����ó�������� ���ܻ�Ȳ���� ����
 * DDL rollback�� �����Ѵ�.
 *
 * - ��, temporary disk index�� ���� ���ܵȴ�.
 *   �ֳ��ϸ� QP�ܿ��� create index ������ abort�߻��ϸ�
 *   ��������� drop temporary table�� �����ϱ� �����̴�.
 ***********************************************************************/
IDE_RC  smcTable::dropIndexByAbortOID( idvSQL *aStatistics,
                                       void*   aTrans,
                                       smOID   aOldIndexOID,  // next
                                       smOID   aNewIndexOID ) // previous
{

    smcTableHeader *  sHeader = NULL;
    void           *  sIndexHeader;
    UInt              sIndexHeaderSize;
    UInt              i;
    SInt              sOffset;
    UInt              sState = 0;
    scGRID*           sIndexSegGRID;

    IDE_ASSERT( smmManager::getOIDPtr( SMI_ID_TABLESPACE_SYSTEM_MEMORY_DIC, 
                                       aOldIndexOID,
                                       (void**)&sIndexHeader )
                == IDE_SUCCESS );

    IDE_ASSERT( smcTable::getTableHeaderFromOID( smLayerCallback::getTableOIDOfIndexHeader( sIndexHeader ),
                                                 (void**)&sHeader )
                == IDE_SUCCESS );

    sIndexHeaderSize = smLayerCallback::getSizeOfIndexHeader();

    if(smrRecoveryMgr::isRestart() == ID_FALSE)
    {
        IDE_TEST( smcTable::latchExclusive( sHeader ) != IDE_SUCCESS );
        sState = 1;
    }

    // index header�� seg rID ��ġ ���
    if( SMI_TABLE_TYPE_IS_DISK( sHeader ) == ID_TRUE )
    {
        sIndexSegGRID  =  smLayerCallback::getIndexSegGRIDPtr( sIndexHeader );
        if (SC_GRID_IS_NOT_NULL(*sIndexSegGRID))
        {
            IDE_TEST( sdpSegment::freeIndexSeg4Entry(
                                             aStatistics,
                                             SC_MAKE_SPACE(*sIndexSegGRID),
                                             aTrans,
                                             aOldIndexOID,
                                             SDR_MTX_LOGGING)
                      != IDE_SUCCESS );
        }
    }

    // BUG-23218 : �ε��� Ž�� �Լ� �̸� ����
    IDE_ASSERT( findIndexVarSlot2AlterAndDrop(sHeader,
                                              sIndexHeader,
                                              sIndexHeaderSize,
                                              &i,
                                              &sOffset)
                 == IDE_SUCCESS );

    IDE_TEST( smcTable::dropIndexByAbortHeader(aStatistics,
                                               aTrans,
                                               sHeader,
                                               i,
                                               sIndexHeader,
                                               aNewIndexOID)
              != IDE_SUCCESS );

    /* Restart�ÿ��� Index�� Temporal�κ��� �����Ǿ� ���� �ʴ�.
     * ������ Disk Table�� ��� Redo�Ŀ� Index�� Temporal �κ���
     * �����Ѵ�. */

    if( ( smrRecoveryMgr::isRestart()       == ID_FALSE ) ||
        ( SMI_TABLE_TYPE_IS_DISK( sHeader ) == ID_TRUE  ) )
    {
        IDE_TEST( smcTable::dropIndexPending( sIndexHeader )
                  != IDE_SUCCESS );
    }

    if(sHeader->mDropIndexLst != NULL)
    {
        mDropIdxPagePool.memfree(sHeader->mDropIndexLst);

        sHeader->mDropIndexLst = NULL;
        sHeader->mDropIndex    = 0;
    }

    if(smrRecoveryMgr::isRestart() == ID_FALSE)
    {
        sState = 0;
        IDE_TEST( smcTable::unlatch( sHeader ) != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if(sState != 0)
    {
        IDE_PUSH();

        IDE_ASSERT( smcTable::unlatch( sHeader ) == IDE_SUCCESS );

        IDE_POP();
    }

    return IDE_FAILURE;

}

/***********************************************************************
 * Description : table header�� drop index list�� �ʱ�ȭ�Ѵ�.
 ***********************************************************************/
IDE_RC  smcTable::clearIndexList( smOID aTableOID )
{

    UInt             sState = 0;
    smcTableHeader*  sHeader = NULL;

    IDE_ASSERT( smcTable::getTableHeaderFromOID( aTableOID,
                                                 (void**)&sHeader )
                == IDE_SUCCESS );

    if(sHeader->mDropIndexLst != NULL)
    {
        if(smrRecoveryMgr::isRestart() == ID_FALSE)
        {
            IDE_TEST( smcTable::latchExclusive( sHeader ) != IDE_SUCCESS );
            sState = 1;
        }


        IDE_TEST(mDropIdxPagePool.memfree(sHeader->mDropIndexLst)
                 != IDE_SUCCESS);
        sHeader->mDropIndexLst = NULL;

        if(smrRecoveryMgr::isRestart() == ID_FALSE)
        {
            sState = 0;
            IDE_TEST( smcTable::unlatch( sHeader ) != IDE_SUCCESS );
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if(sState != 0)
    {
        IDE_PUSH();

        IDE_ASSERT( smcTable::unlatch( sHeader ) == IDE_SUCCESS );

        IDE_POP();
    }

    return IDE_FAILURE;

}

/***********************************************************************
 * Description : create table�� ���� operation NTA�� logical undo�� �����
 * disk table �ΰ�쿡 ager�� table�� ���Ÿ� ��Ź���� �ʰ� �ٷ� �ش� tx��
 * �����ϵ��� �Ѵ�.
 ***********************************************************************/
IDE_RC smcTable::doNTADropDiskTable( idvSQL *aStatistics,
                                     void*   aTrans,
                                     SChar*  aRow )
{

    smcTableHeader*   sHeader;

    IDE_DASSERT( aTrans != NULL );
    IDE_DASSERT( aRow != NULL );

    sHeader = (smcTableHeader *)((smpSlotHeader*)aRow + 1);

    if( SMI_TABLE_TYPE_IS_DISK( sHeader ) == ID_TRUE )
    {
        dropTablePending(aStatistics,
                         aTrans, sHeader, ID_FALSE);
    }

    return IDE_SUCCESS;

}

/* SegmentGRID�� ������ Lob �÷��� ��ȯ�Ѵ�. */
const smiColumn* smcTable::findLOBColumn( void  * aHeader,
                                          scGRID  aSegGRID )
{
    UInt             i;
    const smiColumn *sColumn;
    const smiColumn *sLobColumn;
    UInt             sColumnCnt = getColumnCount( aHeader );

    sLobColumn = NULL;

    for( i = 0; i < sColumnCnt; i++ )
    {
        sColumn = smcTable::getColumn( aHeader, i );

        if( (SMI_IS_LOB_COLUMN( sColumn->flag) == ID_TRUE) &&
            (SC_GRID_IS_EQUAL( sColumn->colSeg, aSegGRID )))
        {
            sLobColumn = sColumn;
            break;
        }
    }

    return sLobColumn;
}

/* PROJ-1671 LOB Segment�� ���� Segment Handle�� �����ϰ�, �ʱ�ȭ�Ѵ�.*/
IDE_RC smcTable::createLOBSegmentDesc( smcTableHeader * aHeader )
{
    UInt        i;
    smiColumn * sColumn;
    UInt        sColumnCnt;
    idBool      sInvalidTBS = ID_FALSE;

    IDE_ASSERT( aHeader != NULL );
    sColumnCnt = getColumnCount( aHeader );

    /*
     * BUG-22677 DRDB����create table�� ������ recovery�� �ȵǴ� ��찡 ����.
     */
    if( SMC_IS_VALID_COLUMNINFO(aHeader->mColumns.fstPieceOID) )
    {
        for( i = 0; i < sColumnCnt; i++ )
        {
            sColumn = (smiColumn*)smcTable::getColumn( aHeader, i );

            /*
             * BUG-26848 [SD] Lob column�� �ִ� tbs�� discard�Ǿ�����
             *           refine�ܰ迡�� �״� ��찡 ����.
             */
            sInvalidTBS = sctTableSpaceMgr::hasState( sColumn->colSpace,
                                                      SCT_SS_INVALID_DISK_TBS );
            if( sInvalidTBS == ID_TRUE )
            {
                continue;
            }

            if( ( SMI_IS_LOB_COLUMN(sColumn->flag) == ID_TRUE ) &&
                ( SC_GRID_IS_NOT_NULL( sColumn->colSeg )) )
            {
                IDE_TEST( sdpSegment::allocLOBSegDesc(
                              sColumn,
                              aHeader->mSelfOID )
                          != IDE_SUCCESS );
                
                IDE_TEST( sdpSegment::initLobSegDesc( sColumn )
                          != IDE_SUCCESS );
            }
        }
    }

    return IDE_SUCCESS;
    IDE_EXCEPTION_END;
    return IDE_FAILURE;
}

/* PROJ-1671 LOB Segment�� ���� Segment Handle �����Ѵ�. */
IDE_RC smcTable::destroyLOBSegmentDesc( smcTableHeader * aHeader )
{
    UInt        i;
    smiColumn * sColumn;
    UInt        sColumnCnt;
    idBool      sInvalidTBS = ID_FALSE;

    IDE_ASSERT( aHeader != NULL );
    sColumnCnt = getColumnCount( aHeader );

    for( i = 0; i < sColumnCnt; i++ )
    {
        sColumn = (smiColumn*)smcTable::getColumn( aHeader, i );

        sInvalidTBS = sctTableSpaceMgr::hasState( sColumn->colSpace,
                                                  SCT_SS_INVALID_DISK_TBS );
        if( sInvalidTBS == ID_TRUE )
        {
            continue;
        }

        if( ( SMI_IS_LOB_COLUMN(sColumn->flag) == ID_TRUE ) &&
            ( SC_GRID_IS_NOT_NULL( sColumn->colSeg )) )
        {
            IDE_TEST( sdpSegment::freeLOBSegDesc( sColumn )
                      != IDE_SUCCESS );
        }
    }

    return IDE_SUCCESS;
    IDE_EXCEPTION_END;
    return IDE_FAILURE;
}

/***********************************************************************
 *
 * Description : ��ũ �ε����� Runtime Header �����Ѵ�.
 *
 *   aStatistics - [IN] �������
 *   aHeader     - [IN] ��ũ ���̺� ���
 *   aMaxSmoNo   - [IN] ��ũ �ε��� ��Ÿ�� ��� ����� ������ SmoNo
 *
 **********************************************************************/
IDE_RC smcTable::rebuildRuntimeIndexHeaders( idvSQL         * aStatistics,
                                             smcTableHeader * aHeader,
                                             ULong            aMaxSmoNo )
{
    UInt                 i;
    UInt                 sIndexCnt;
    idBool               sIsExist;
    void               * sIndexHeader;
    scGRID             * sIndexGRID;
    smiSegAttr         * sSegAttrPtr;
    smiSegStorageAttr  * sSegStoAttrPtr;
    smrRTOI              sRTOI;

    IDE_ASSERT( SMI_TABLE_TYPE_IS_DISK( aHeader ) == ID_TRUE );

    sIndexCnt = smcTable::getIndexCount(aHeader);

    for ( i = 0; i < sIndexCnt; i++ )
    {
        sIndexHeader = (void*)smcTable::getTableIndex(aHeader,i);
        sIndexGRID   = smLayerCallback::getIndexSegGRIDPtr( sIndexHeader );

        if ( SC_GRID_IS_NULL(*sIndexGRID) )
        {
             continue;
        }

        // fix BUG-17157 [PROJ-1548] Disk Tablespace Online/Offline �����
        // �ùٸ��� Index Runtime Header �������� ����
        // Refine DRDB ������ Online �������� ȣ��� �� �ִµ�,
        // INVALID_DISK_TBS( DROPPED/DISCARDED ) �ΰ�쿡��
        // Skip �ϵ��� �Ѵ�.
        sIsExist = sddDiskMgr::isValidPageID( aStatistics, /* idvSQL* */
                                              SC_MAKE_SPACE(*sIndexGRID),
                                              SC_MAKE_PID(*sIndexGRID));

        if ( sIsExist == ID_FALSE )
        {
            /* ��ȿ���� �ʴ� ���׸�Ʈ �������� ���� �ε����� ��Ÿ�� �����
             * NULL �ʱ�ȭ�Ѵ� */
            smLayerCallback::setInitIndexPtr( sIndexHeader );
            continue;
        }

        if ( ( smLayerCallback::getFlagOfIndexHeader( sIndexHeader )
               & SMI_INDEX_USE_MASK ) == SMI_INDEX_USE_ENABLE )
        {
            sSegStoAttrPtr = smLayerCallback::getIndexSegStoAttrPtr( sIndexHeader );
            sSegAttrPtr    = smLayerCallback::getIndexSegAttrPtr( sIndexHeader );

            /* PROJ-2162 RestartRiskReduction
             * Property�� ���� ������ ������ */
            smrRecoveryMgr::initRTOI( &sRTOI );
            sRTOI.mCause    = SMR_RTOI_CAUSE_PROPERTY;
            sRTOI.mType     = SMR_RTOI_TYPE_INDEX;
            sRTOI.mState    = SMR_RTOI_STATE_CHECKED;
            sRTOI.mTableOID = smLayerCallback::getTableOIDOfIndexHeader( sIndexHeader );
            sRTOI.mIndexID  = smLayerCallback::getIndexIDOfIndexHeader( sIndexHeader );
            if( smrRecoveryMgr::isIgnoreObjectByProperty( &sRTOI ) == ID_TRUE )
            {
                IDE_TEST( smrRecoveryMgr::startupFailure( &sRTOI,
                                                          ID_FALSE )/*isRedo*/
                          != IDE_SUCCESS);
            }
            else
            {
                if ( smLayerCallback::initIndex( aStatistics,
                                                 aHeader,
                                                 sIndexHeader,
                                                 sSegAttrPtr,
                                                 sSegStoAttrPtr,
                                                 NULL,
                                                 aMaxSmoNo )
                     != IDE_SUCCESS )
                {
                    /* ��޺�����尡 �ƴϸ� ���� �����Ŵ. */
                    IDE_TEST( smuProperty::getEmergencyStartupPolicy()
                                == SMR_RECOVERY_NORMAL );
                    IDE_TEST( smrRecoveryMgr::refineFailureWithIndex(
                                                        sRTOI.mTableOID,
                                                        sRTOI.mIndexID )
                            != IDE_SUCCESS);
                }
                else
                {
                    smLayerCallback::setIndexCreated( sIndexHeader, ID_TRUE );
                }
            }
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 *
 * Description : ��ũ ���̺��� ��� RUNTIME ������ �����Ѵ�.
 *
 *   RUNTIME �������� ������ ���� ������ �ִ�.
 *   - ���̺��� Lock Node�� Mutex
 *   - ���̺��� �ε��� RUNTIME ���
 *   - LOB �÷��� ���׸�Ʈ �ڵ�
 *
 *   aActionArg �� ���� ���������� �ʱ�ȭ�Ǳ⵵ �Ѵ�.
 *
 *   aStatistics - [IN] �������
 *   aHeader     - [IN] ��ũ ���̺� ���
 *   aActionArg  - [IN] Action�Լ��� ���� Argument
 *
 **********************************************************************/
IDE_RC smcTable::initRuntimeInfos( idvSQL         * aStatistics,
                                   smcTableHeader * aHeader,
                                   void           * aActionArg )
{
    idBool sInitLockAndRuntimeItem;

    IDE_ASSERT( aActionArg != NULL );

    sInitLockAndRuntimeItem = *(idBool*)aActionArg;

    if ( SMI_TABLE_TYPE_IS_DISK( aHeader ) == ID_TRUE )
    {
        /* Disk table�� ��� undoAll���� tableheader��
         * �����ϱ� ������ �ʱ�ȭ�� ���⼭ ���־�� �Ѵ�.
         * MemoryDB�� refine�ÿ��� memory table�� ���ؼ��� ó���ϵ��� �Ѵ�. */

        if ( sInitLockAndRuntimeItem == ID_TRUE )
        {
            IDE_TEST( initLockAndRuntimeItem( aHeader )
                      != IDE_SUCCESS );
        }

        IDE_TEST( rebuildRuntimeIndexHeaders( aStatistics,
                                              aHeader,
                                              0 )
                  != IDE_SUCCESS );

        /* PROJ-1671 LOB Segment�� ���� Segment Handle�� �����ϰ�, �ʱ�ȭ�Ѵ�.*/
        IDE_TEST( createLOBSegmentDesc( aHeader ) != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC smcTable::initRowTemplate( idvSQL         * /*aStatistics*/,
                                  smcTableHeader * aHeader,
                                  void           * /*aActionArg*/ )
{
    const smiColumn      * sColumn;
    UInt                   sColumnCount;
    UShort                 sColumnSeq;
    smcRowTemplate       * sRowTemplate; 
    smcColTemplate       * sColumnTemplate;
    SShort                 sColStartOffset = 0;
    UInt                   sColumnLength;
    UShort               * sVariableColumnOffsetArr;
    UShort                 sVariableColumnCount = 0;
    UChar                * sAllocMemPtr;
    UInt                   sAllocState = 0;
    idBool                 sIsEnableRowTemplate;

    IDE_DASSERT( aHeader != NULL );

    aHeader->mRowTemplate = NULL;

    sIsEnableRowTemplate = smuProperty::getEnableRowTemplate();

    IDE_TEST_CONT( SMI_TABLE_TYPE_IS_DISK( aHeader ) != ID_TRUE,
                    skip_init_row_template );

    IDE_TEST_CONT( sctTableSpaceMgr::hasState( aHeader->mSpaceID,             
                                                SCT_SS_INVALID_DISK_TBS ) 
                    == ID_TRUE, skip_init_row_template );

    IDE_TEST_CONT( aHeader->mColumnCount == 0, skip_init_row_template );

    sColumnCount = aHeader->mColumnCount;

    /* smcTable_initRowTemplate_calloc_AllocMemPtr.tc */
    IDU_FIT_POINT("smcTable::initRowTemplate::calloc::AllocMemPtr");
    IDE_TEST( iduMemMgr::calloc( IDU_MEM_SM_SMC,
                                 1,
                                 ID_SIZEOF(smcRowTemplate) 
                                 + (ID_SIZEOF(smcColTemplate) * sColumnCount)
                                 + ((ID_SIZEOF(UShort) * sColumnCount)),
                                 (void**)&sAllocMemPtr )
                != IDE_SUCCESS );
    sAllocState = 1;

    sRowTemplate = (smcRowTemplate*)sAllocMemPtr;

    sRowTemplate->mColTemplate = 
                    (smcColTemplate*)(sAllocMemPtr + ID_SIZEOF(smcRowTemplate));

    sVariableColumnOffsetArr = 
                    (UShort*)(sAllocMemPtr + ID_SIZEOF(smcRowTemplate)
                                     + (ID_SIZEOF(smcColTemplate) * sColumnCount));

    sRowTemplate->mTotalColCount = sColumnCount;

    for ( sColumnSeq = 0; sColumnSeq < sColumnCount; sColumnSeq++ )
    {
        sColumnTemplate = &sRowTemplate->mColTemplate[sColumnSeq];
        sColumn         = smcTable::getColumn( aHeader, sColumnSeq );

        /* BUG-39679 Row Template ������ ����Ұ����� �ƴ��� �Ǵ� �Ѵ�.
           ������� ������� sColumnLength�� ������ ID_UINT_MAX�������Ѵ�. */
        if( sIsEnableRowTemplate == ID_TRUE )
        {
            IDE_ASSERT( gSmiGlobalCallBackList.getColumnStoreLen( sColumn,
                                                                  &sColumnLength )
                        == IDE_SUCCESS );
        }
        else
        {
            sColumnLength = ID_UINT_MAX;
        }

        /* sColumnLength�� ID_UINT_MAX�� ���� variable column �̰ų� 
         * not null column�� �ƴ� ����̴�. */
        if ( sColumnLength == ID_UINT_MAX )
        {
            sColumnTemplate->mColStartOffset  = sColStartOffset;
            sColumnTemplate->mStoredSize      = SMC_UNDEFINED_COLUMN_LEN;
            sColumnTemplate->mColLenStoreSize = 0;
            sColumnTemplate->mVariableColIdx  = sVariableColumnCount;
            /* sColStartOffset�� non variable column�϶��� ���� ��Ų��.
             * (sColumnLength == ID_UINT_MAX�� �ƴҶ� )
             * ��, �� column�� mColStartOffset�� �ش� column�� �տ� �����ϴ�
             * non variable column�� ������ ���̶�� �� �� �ִ�.
             * variable column�� ���̴� fetch�� �����ϴ� �Լ���
             * sdc::doFetch�Լ����� ���̸� ���� ���ȴ�.*/
            sColStartOffset                  += 0;

            /* variable column�տ� �����ϴ� non variable column���� ���̸�
             * �ٷ� �˱� ���� ���� variable column�� sColStartOffset��
             * �����Ѵ�. (mVariableColOffset ����) */
            sVariableColumnOffsetArr[sVariableColumnCount] = sColStartOffset;
            sVariableColumnCount++;
        }
        else
        {
            IDE_ASSERT( sColumnLength < ID_UINT_MAX );

            sColumnTemplate->mColStartOffset  = sColStartOffset;
            sColumnTemplate->mStoredSize      = sColumnLength;
            sColumnTemplate->mColLenStoreSize = SDC_GET_COLLEN_STORE_SIZE(sColumnLength);
            sColumnTemplate->mVariableColIdx  = sVariableColumnCount;
            sColStartOffset                  += (sColumnLength 
                                                 + sColumnTemplate->mColLenStoreSize);
        }
    }

    sRowTemplate->mVariableColOffset = sVariableColumnOffsetArr;
    sRowTemplate->mVariableColCount  = sVariableColumnCount;
    aHeader->mRowTemplate            = sRowTemplate;

    IDE_EXCEPTION_CONT(skip_init_row_template);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if( sAllocState == 1 )
    {
        IDE_ASSERT( iduMemMgr::free( sAllocMemPtr ) == IDE_SUCCESS );
    }
    
    return IDE_FAILURE;
}

IDE_RC smcTable::destroyRowTemplate( smcTableHeader * aHeader )
{
    if ( aHeader->mRowTemplate != NULL )
    {
        IDE_TEST( iduMemMgr::free( aHeader->mRowTemplate ) != IDE_SUCCESS );

        aHeader->mRowTemplate = NULL;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

void smcTable::dumpRowTemplate( smcRowTemplate * aRowTemplate )
{
    smcColTemplate * sColTemplate;
    UShort           sVariableColOffset;
    UInt             sColumnCount;
    UInt             sColumnSeq;

    ideLogEntry      sLog( IDE_ERR_0 );

    IDE_DASSERT( aRowTemplate != NULL );

    sColumnCount = aRowTemplate->mTotalColCount;

    sLog.append( "==================================\n"
                 "Dump Column Template\n");

    for ( sColumnSeq = 0; sColumnSeq < sColumnCount; sColumnSeq++ )
    {
        sColTemplate = &aRowTemplate->mColTemplate[sColumnSeq];

        sLog.appendFormat( "==================================\n"
                           "Column Seq      : %"ID_UINT32_FMT"\n"
                           "StoredSize      : %"ID_UINT32_FMT"\n"
                           "ColLenStoreSize : %"ID_UINT32_FMT"\n"
                           "VariableColIdx  : %"ID_UINT32_FMT"\n"
                           "ColStartOffset  : %"ID_UINT32_FMT"\n",
                           sColumnSeq,
                           sColTemplate->mStoredSize,
                           sColTemplate->mColLenStoreSize,
                           sColTemplate->mVariableColIdx,
                           sColTemplate->mColStartOffset );
    }

    sLog.append( "==================================\n" );

    sLog.append( "==================================\n"
                 "Dump Variable Column Offset\n");

    for ( sColumnSeq = 0; sColumnSeq < sColumnCount; sColumnSeq++ )
    {
        sVariableColOffset = aRowTemplate->mVariableColOffset[sColumnSeq];

        sLog.appendFormat( "==================================\n"
                           "Seq            : %"ID_UINT32_FMT"\n"
                           "ColStartOffset : %"ID_UINT32_FMT"\n",
                           sColumnSeq,
                           sVariableColOffset );
    }

    sLog.append( "==================================\n" );

    sLog.appendFormat( "==================================\n"
                       "TotalColCount     : %"ID_UINT32_FMT"\n"
                       "VariableColCount  : %"ID_UINT32_FMT"\n"
                       "==================================\n", 
                       aRowTemplate->mTotalColCount,
                       aRowTemplate->mVariableColCount );

    sLog.write();
}

/***********************************************************************
 *
 * Description : ��ũ ���̺��� ��� ��ũ �ε����� Integrity üũ�� �����Ѵ�.
 *
 *   aStatistics - [IN] �������
 *   aHeader     - [IN] ��ũ ���̺� ���
 *   aActionArg  - [IN] Action�Լ��� ���� Argument
 *
 **********************************************************************/
IDE_RC smcTable::verifyIndexIntegrity( idvSQL         * aStatistics,
                                       smcTableHeader * aHeader,
                                       void           * aActionArg )
{
    UInt               i;
    UInt               sIndexCnt;
    void*              sIndexHeader;
    scGRID*            sIndexGRID;
    SChar              sStrBuffer[128];

    IDE_ASSERT( aActionArg == NULL );

    IDE_TEST_CONT( SMI_TABLE_TYPE_IS_DISK( aHeader ) == ID_FALSE,
                    skip_verify );

    sIndexCnt = smcTable::getIndexCount(aHeader);

    for( i = 0; i < sIndexCnt; i++ )
    {
        sIndexHeader = (void*)smcTable::getTableIndex(aHeader,i);
        sIndexGRID   = smLayerCallback::getIndexSegGRIDPtr( sIndexHeader );

        if (SC_GRID_IS_NULL(*sIndexGRID) )
        {
            continue;
        }
        else
        {
            /* ��ũ �ε��� ��� �ʱ�ȭ�ÿ� �̹� �ε��� ���׸�Ʈ �������� ����
             * ��ũ�󿡼��� Verify�� �Ϸ�Ǿ��� ������ �ϰ� �����Ѵ�. */
        }

        if ( smLayerCallback::isIndexToVerifyIntegrity( (const smnIndexHeader*)sIndexHeader )
             == ID_FALSE )
        {
            continue;
        }

        if ( ( smLayerCallback::getFlagOfIndexHeader( sIndexHeader )
               & SMI_INDEX_USE_MASK ) == SMI_INDEX_USE_ENABLE )
        {
            idlOS::snprintf(sStrBuffer,
                            128,
                            "       [TABLE OID: %"ID_vULONG_FMT", "
                            "INDEX ID: %"ID_UINT32_FMT" "
                            "NAME : %s] ",
                            ((smnIndexHeader*)sIndexHeader)->mTableOID,
                            ((smnIndexHeader*)sIndexHeader)->mId,
                            ((smnIndexHeader*)sIndexHeader)->mName );

            IDE_CALLBACK_SEND_SYM( sStrBuffer );

            if ( smLayerCallback::verifyIndexIntegrity( aStatistics,
                                                        ((smnIndexHeader*)sIndexHeader)->mHeader )
                 != IDE_SUCCESS )
            {
                IDE_CALLBACK_SEND_SYM( " [FAIL]\n" );
            }
            else
            {
                IDE_CALLBACK_SEND_SYM( " [PASS]\n" );
            }
        }
    }

    IDE_EXCEPTION_CONT( skip_verify );

    return IDE_SUCCESS;
}


IDE_RC smcTable::setNullRow(void*             aTrans,
                            smcTableHeader*   aTable,
                            UInt              aTableType,
                            void*             aData)
{
    smpSlotHeader*   sNullSlotHeaderPtr;
    smSCN            sNullRowSCN;
    smOID            sNullRowOID = SM_NULL_OID;

    smcTableHeader*  sTableHeader = (smcTableHeader*)aTable;

    IDE_DASSERT( aTableType != SMI_TABLE_DISK );

    sNullRowOID = *(( smOID* )aData);

    if( aTableType == SMI_TABLE_MEMORY )
    {
        IDE_ASSERT( smmManager::getOIDPtr( 
                        sTableHeader->mSpaceID, 
                        sNullRowOID,
                        (void**)&sNullSlotHeaderPtr )
                    == IDE_SUCCESS );

        SM_SET_SCN_NULL_ROW( &sNullRowSCN );

        IDE_ASSERT( SM_SCN_IS_EQ(&sNullRowSCN,
                                 &(sNullSlotHeaderPtr->mCreateSCN)));
    }

    IDE_TEST( smrUpdate::setNullRow(NULL, /* idvSQL* */
                                    aTrans,
                                    SMI_ID_TABLESPACE_SYSTEM_MEMORY_DIC,
                                    SM_MAKE_PID(sTableHeader->mSelfOID),
                                    SM_MAKE_OFFSET(sTableHeader->mSelfOID)
                                    + SMP_SLOT_HEADER_SIZE,
                                    sNullRowOID)
              != IDE_SUCCESS);


    sTableHeader->mNullOID = sNullRowOID;

    IDE_TEST( smmDirtyPageMgr::insDirtyPage(SMI_ID_TABLESPACE_SYSTEM_MEMORY_DIC,
                                            SM_MAKE_PID(sTableHeader->mSelfOID))
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : Table�� Lob Column����Ʈ�� �����Ѵ�.
 *
 * aArrLobColumn - [OUT] Lob Column Pointer Array�� �Ѱ��ش�.
 ***********************************************************************/
IDE_RC smcTable::makeLobColumnList(
    void                     * aTableHeader,
    smiColumn             ***  aArrLobColumn,
    UInt                     * aLobColumnCnt)
{
    UInt              sLobColumnCnt;
    smiColumn       **sLobColumn;
    smiColumn       **sFence;
    smiColumn        *sColumn;
    UInt              i;
    UInt              sColumnType;

    sLobColumnCnt = smcTable::getLobColumnCount(aTableHeader);

    *aLobColumnCnt = sLobColumnCnt;
    *aArrLobColumn = NULL;

    if( sLobColumnCnt != 0 )
    {
        /* smcTable_makeLobColumnList_malloc_ArrLobColumn.tc */
        IDU_FIT_POINT("smcTable::makeLobColumnList::malloc::ArrLobColumn");
        IDE_TEST(iduMemMgr::malloc(
                     IDU_MEM_SM_SMC,
                     (ULong)sLobColumnCnt *
                     ID_SIZEOF(smiColumn*),
                     (void**)aArrLobColumn,
                     IDU_MEM_FORCE)
                 != IDE_SUCCESS);

        sLobColumn =  *aArrLobColumn;
        sFence     =  *aArrLobColumn + sLobColumnCnt;

        for( i = 0; sLobColumn < sFence; i++)
        {
            sColumn = (smiColumn*)smcTable::getColumn( aTableHeader, i );
            sColumnType =
                sColumn->flag & SMI_COLUMN_TYPE_MASK;

            if( sColumnType == SMI_COLUMN_TYPE_LOB )
            {
                *sLobColumn = sColumn;
                sLobColumn++;
            }
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if( *aArrLobColumn != NULL )
    {
        iduMemMgr::free(*aArrLobColumn);
    }

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : Column List�� �Ҵ�� �޸𸮸� �ݳ��Ѵ�.
 *
 * aColumnList - [IN] Column List
 ***********************************************************************/
IDE_RC smcTable::destLobColumnList(void *aColumnList)
{
    if( aColumnList != NULL )
    {
        IDE_TEST( iduMemMgr::free(aColumnList)
                  != IDE_SUCCESS );
    }
    return IDE_SUCCESS;
    IDE_EXCEPTION_END;
    return IDE_FAILURE;

}

/***********************************************************************
 * Description : QP�κ��� null row�� ���´�.
 *
 * Implementation :
 *    ���� smiColumnList�� �����Ѵ�. aColListBuffer�� smiColumnList
 *    array�̴�. �� ������ next�� column�� �����Ѵ�.
 *    ������ column list�� next�� NULL�� �����ؾ� �Ѵ�.
 *    ������ smiColumnList�� null row�� ������� ���۸� ���ڷ�
 *    callback �Լ��� ȣ���Ѵ�.
 *
 *    - aTableHeader: ��� ���̺��� ���̺� �ش�
 *    - aColListBuffer: ��� ���̺��� �÷� ����Ʈ�� ������ ���� ����
 *    - aNullRow: smiValue���� ����� ����
 *    - aValueBuffer: fixed null value���� ����� ����
 *                    aNullRow[i].value�� ����Ű�� �����̵ȴ�.
 ***********************************************************************/
IDE_RC smcTable::makeNullRowByCallback(smcTableHeader *aTableHeader,
                                       smiColumnList  *aColListBuffer,
                                       smiValue       *aNullRow,
                                       SChar          *aValueBuffer)
{
    smiColumnList *sCurList;
    UInt           i;

    /* �÷��� ������ �ּ��� 1���̻��̾�� �Ѵ�. */
    IDE_ASSERT(aTableHeader->mColumnCount > 0);

    /* STEP-1 : smiColumnList�� �����Ѵ�. */
    sCurList = aColListBuffer;

    for (i = 0; i < aTableHeader->mColumnCount; i++)
    {
        sCurList->next = sCurList + 1;
        sCurList->column = smcTable::getColumn( aTableHeader, i );
        sCurList++;
    }
    /* �÷��� ������ �ּ��� 1�� �̻��̹Ƿ� for loop�� �ѹ��� �ȵ��� ���� ����. */
    /* ������ smiColumnList�� next�� NULL�� �����ؾ� �Ѵ�. */
    sCurList--;
    sCurList->next = NULL;

    /* STEP-2 : QP�κ��� null row�� ���´�. */
    IDE_TEST(gSmiGlobalCallBackList.makeNullRow(NULL,
                                                aColListBuffer,
                                                aNullRow,
                                                aValueBuffer)
             != IDE_SUCCESS);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**********************************************************************
 * BUG-30378 ������������ Drop�Ǿ����� refine���� �ʴ�
 * ���̺��� �����մϴ�.
 * ���������� ����� �� �ִ� Dump�Լ��� ����
 *
 * BUG-31206    improve usability of DUMPCI and DUMPDDF
 * Util������ �� �� �ִ� ���ۿ� �Լ�
 *
 * Boot log�� Internal Server Error�޼����� ��´�.
 *
 * [IN]     aTableHeader     - ������ �߻��� Table�� Header
 * [IN]     aDumpBinary      - aTableHeader�� ���� ���� a�� ������ ����
 * [IN]     aDisplayTable    - ���� ǥ �������� ������� ����
 * [OUT]    aOutBuf          - ��� ����
 * [OUT]    aOutSize         - ��� ������ ũ��
 *
 **********************************************************************/

void  smcTable::dumpTableHeaderByBuffer( smcTableHeader * aTable,
                                         idBool           aDumpBinary,
                                         idBool           aDisplayTable,
                                         SChar          * aOutBuf,
                                         UInt             aOutSize )
{
    /* smcDef.h:71 - smcTableType */
    SChar            sType[][ 9] ={ "NORMAL",
                                    "TEMP",
                                    "CATALOG",
                                    "SEQ",
                                    "OBJECT" };
    /* smiDef.h:743- smiObjectType */
    SChar            sObjectType[][ 9] ={ "NONE",
                                          "PROC",
                                          "PROC_GRP",
                                          "VIEW",
                                          "DBLINK" };
    SChar            sTableType[][ 9] ={ "META",
                                         "TEMP",
                                         "MEMORY",
                                         "DISK",
                                         "FIXED",
                                         "VOLATILE",
                                         "REMOTE" };
    UInt             sStrLen;
    UInt             sTableTypeID;
    UInt             i;

    IDE_ASSERT( aTable != NULL );

    sTableTypeID = SMI_TABLE_TYPE_TO_ID( SMI_GET_TABLE_TYPE( aTable ) );

    if( aDisplayTable == ID_FALSE )
    {
        idlVA::appendFormat( aOutBuf,
                             aOutSize,
                             "Dump Table Header\n"
                             "Type            : %s(%"ID_UINT32_FMT")\n"
                             "Flag            : %"ID_UINT32_FMT"\n"
                             "TableType       : %s(%"ID_UINT32_FMT")\n"
                             "ObjType         : %s(%"ID_UINT32_FMT")\n"
                             "SelfOID         : %"ID_vULONG_FMT"\n"
                             "NullOID         : %"ID_vULONG_FMT"\n"
                             "ColumnSize      : %"ID_UINT32_FMT"\n"
                             "ColumnCount     : %"ID_UINT32_FMT"\n"
                             "SpaceID         : %"ID_UINT32_FMT"\n"
                             "MaxRow          : %"ID_UINT64_FMT"\n"
                             "TableCreateSCN  : %"ID_UINT64_FMT"\n",
                             sType[aTable->mType],
                             aTable->mType,
                             aTable->mFlag,
                             sTableType[ sTableTypeID ],
                             sTableTypeID,
                             sObjectType[aTable->mObjType],
                             aTable->mObjType,
                             aTable->mSelfOID,
                             aTable->mNullOID,
                             aTable->mColumnSize,
                             aTable->mColumnCount,
                             aTable->mSpaceID,
                             aTable->mMaxRow,
                             SM_SCN_TO_LONG( aTable->mTableCreateSCN ) );

        switch( SMI_GET_TABLE_TYPE( aTable ) )
        {
        case SMI_TABLE_DISK:
            idlVA::appendFormat( aOutBuf,
                                 aOutSize,
                                 "SlotSize        : %"ID_UINT32_FMT"\n",
                                 aTable->mFixed.mDRDB.mSlotSize );
            break;
        case SMI_TABLE_META:
        case SMI_TABLE_MEMORY:
            idlVA::appendFormat(
                            aOutBuf,
                            aOutSize,
                            "FixPage         : SlotSize %8"ID_UINT32_FMT"  "
                            "HeadPageID %8"ID_UINT32_FMT"\n",
                            aTable->mFixed.mMRDB.mSlotSize,
                            smpAllocPageList::getFirstAllocPageID(
                                &aTable->mFixedAllocList.mMRDB[0]) );
            for( i = 0 ; i < SM_VAR_PAGE_LIST_COUNT ; i ++ )
            {
                idlVA::appendFormat(
                                aOutBuf,
                                aOutSize,
                                "VarPage [%2"ID_UINT32_FMT"]    : "
                                "SlotSize %8"ID_UINT32_FMT"  "
                                "HeadPageID %8"ID_UINT32_FMT"\n",
                                i,
                                aTable->mVar.mMRDB[i].mSlotSize,
                                smpAllocPageList::getFirstAllocPageID(
                                    &aTable->mVarAllocList.mMRDB[i]) );
            }
            break;
        case SMI_TABLE_VOLATILE:
            idlVA::appendFormat(
                            aOutBuf,
                            aOutSize,
                            "FixPage         : SlotSize %8"ID_UINT32_FMT"  "
                            "HeadPageID %8"ID_UINT32_FMT"\n",
                            aTable->mFixed.mVRDB.mSlotSize,
                            svpAllocPageList::getFirstAllocPageID(
                                &aTable->mFixedAllocList.mVRDB[0]) );
            for( i = 0 ; i < SM_VAR_PAGE_LIST_COUNT ; i ++ )
            {
                idlVA::appendFormat(
                                aOutBuf,
                                aOutSize,
                                "VarPage [%2"ID_UINT32_FMT"]    : "
                                "SlotSize %8"ID_UINT32_FMT"  "
                                "HeadPageID %8"ID_UINT32_FMT"\n",
                                i,
                                aTable->mVar.mVRDB[i].mSlotSize,
                                svpAllocPageList::getFirstAllocPageID(
                                    &aTable->mVarAllocList.mVRDB[i]) );
            }
            break;
        default:
            break;
        }
    }
    else
    {
        idlVA::appendFormat( aOutBuf,
                             aOutSize,
                             "%9s(%2"ID_UINT32_FMT") "
                             "%8"ID_UINT32_FMT
                             "%9s(%2"ID_UINT32_FMT") "
                             "%9s(%2"ID_UINT32_FMT") "
                             "%-12"ID_vULONG_FMT
                             "%-12"ID_vULONG_FMT
                             "%-8"ID_UINT32_FMT
                             "%-8"ID_UINT32_FMT
                             "%-4"ID_UINT32_FMT
                             "%-8"ID_UINT64_FMT,
                             sType[aTable->mType],
                             aTable->mType,
                             aTable->mFlag,
                             sTableType[ sTableTypeID ],
                             sTableTypeID,
                             sObjectType[aTable->mObjType],
                             aTable->mObjType,
                             aTable->mSelfOID,
                             aTable->mNullOID,
                             aTable->mColumnSize,
                             aTable->mColumnCount,
                             aTable->mSpaceID,
                             aTable->mMaxRow );
    }

    sStrLen = idlOS::strnlen( aOutBuf, aOutSize );

    if( aDumpBinary == ID_TRUE )
    {
        ideLog::ideMemToHexStr( (UChar*) aTable,
                                ID_SIZEOF( smcTableHeader ),
                                IDE_DUMP_FORMAT_NORMAL,
                                aOutBuf + sStrLen,
                                aOutSize - sStrLen );
    }
}


void  smcTable::dumpTableHeader( smcTableHeader * aTable )
{
    SChar          * sTempBuffer;

    IDE_ASSERT( aTable != NULL );

    if( iduMemMgr::calloc( IDU_MEM_SM_SMC,
                           1,
                           ID_SIZEOF( SChar ) * IDE_DUMP_DEST_LIMIT,
                           (void**)&sTempBuffer )
        == IDE_SUCCESS )
    {
        sTempBuffer[0] = '\0';
        dumpTableHeaderByBuffer( aTable,
                                 ID_TRUE,  // dump binary
                                 ID_FALSE, // display table
                                 sTempBuffer,
                                 IDE_DUMP_DEST_LIMIT );

        ideLog::log( IDE_SERVER_0,
                     "%s\n",
                     sTempBuffer );

        (void) iduMemMgr::free( sTempBuffer );
    }
}


/*
    Table Header�� Index Latch�� �Ҵ��ϰ� �ʱ�ȭ

    [IN] aTableHeader - IndexLatch�� �ʱ�ȭ�� Table�� Header
 */
IDE_RC smcTable::allocAndInitIndexLatch(smcTableHeader * aTableHeader )
{
    SChar sBuffer[IDU_MUTEX_NAME_LEN];

    IDE_ASSERT( aTableHeader != NULL );

    UInt sState = 0;

    if ( aTableHeader->mIndexLatch != NULL )
    {
        // ASSERT�� �Ŵ� ���� ������,
        // ���ۿ��� ������ �����Ƿ� ASSERT��� ��Ʈ�α׿� �ﵵ�� ó��
        ideLog::log( IDE_SERVER_0,
                     "InternalError [%s:%u]\n"
                     "IndexLatch Of TableHeader is initialized twice.\n",
                     (SChar *)idlVA::basename(__FILE__),
                     __LINE__ );
        ideLog::logCallStack( IDE_SERVER_0 );

        dumpTableHeader( aTableHeader );

        // ����� ����϶��� ������ ���δ�.
        IDE_DASSERT(0);
    }

    /* smcTable_allocAndInitIndexLatch_malloc_IndexLatch.tc */
    IDU_FIT_POINT("smcTable::allocAndInitIndexLatch::malloc::IndexLatch");
    IDE_TEST( iduMemMgr::malloc( IDU_MEM_SM_SMC,
                                 ID_SIZEOF(iduLatch),
                                 (void**) &(aTableHeader->mIndexLatch) )
              != IDE_SUCCESS );
    sState =1;

    idlOS::snprintf( sBuffer,
                     ID_SIZEOF(sBuffer),
                     "TABLE_HEADER_LATCH_%"ID_vULONG_FMT,
                     aTableHeader->mSelfOID );

    IDE_TEST( aTableHeader->mIndexLatch->initialize( sBuffer )
              != IDE_SUCCESS );
    sState =2;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    IDE_PUSH();
    {
        switch( sState )
        {
            case 2:
                IDE_ASSERT( aTableHeader->mIndexLatch->destroy( )
                            == IDE_SUCCESS );
            case 1:
                IDE_ASSERT( iduMemMgr::free( aTableHeader->mIndexLatch )
                            == IDE_SUCCESS );
                break;

            default:
                break;
        }

        aTableHeader->mIndexLatch = NULL;
    }
    IDE_POP();

    return IDE_FAILURE;
}


/*
    Table Header�� Index Latch�� �ı��ϰ� ����

    [IN] aTableHeader - IndexLatch�� �ı��� Table�� Header
 */
IDE_RC smcTable::finiAndFreeIndexLatch(smcTableHeader * aTableHeader )
{
    IDE_ASSERT( aTableHeader != NULL );

    if ( aTableHeader->mIndexLatch == NULL )
    {
        /*
         * nothing to do
         *
         * 1. Create Table�� smrRecoveryMgr::undoTrans�������� ȣ��ɼ� �ְ�,
         * 2. �ٽ� �ѹ� smxOIDList::processDropTblPending���� ȣ��ɼ� �ִ�.
         *
         */
    }
    else
    {
        IDE_TEST( aTableHeader->mIndexLatch->destroy( )
                  != IDE_SUCCESS );

        IDE_TEST( iduMemMgr::free( aTableHeader->mIndexLatch )
                  != IDE_SUCCESS );

        aTableHeader->mIndexLatch = NULL;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*
    Table Header�� Index Latch�� Exclusive Latch�� ��´�.

    [IN] aTableHeader - Exclusive Latch�� ���� Table�� Header
*/
IDE_RC smcTable::latchExclusive(smcTableHeader * aTableHeader )
{
    IDE_ASSERT( aTableHeader != NULL );

    if ( aTableHeader->mIndexLatch == NULL )
    {
        // ASSERT�� �Ŵ� ���� ������,
        // ���ۿ��� ������ �����Ƿ� ASSERT��� ��Ʈ�α׿� �ﵵ�� ó��
        //
        // (Index DDL�� ���� ����ÿ��� ������ �ȴ�.)
        ideLog::log( IDE_SERVER_0,
                     "InternalError [%s:%u]\n"
                     "IndexLatch is not initialized yet.(trial to execute latchExclusive).\n",
                     (SChar *)idlVA::basename(__FILE__),
                     __LINE__ );

        ideLog::logCallStack( IDE_SERVER_0 );

        dumpTableHeader( aTableHeader );

        // DEBUG Mode�϶��� ASSERT�� ���δ�.
        IDE_DASSERT(0);
    }
    else
    {
        IDE_TEST( aTableHeader->mIndexLatch->lockWrite( NULL, /* idvSQL* */
                                       NULL /* idvWeArgs* */ )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/*
    Table Header�� Index Latch�� Shared Latch�� ��´�.

    [IN] aTableHeader - Shared Latch�� ���� Table�� Header
 */
IDE_RC smcTable::latchShared(smcTableHeader * aTableHeader )
{
    IDE_ASSERT( aTableHeader != NULL );

    if ( aTableHeader->mIndexLatch == NULL )
    {
        // ASSERT�� �Ŵ� ���� ������,
        // ���ۿ��� ������ �����Ƿ� ASSERT��� ��Ʈ�α׿� �ﵵ�� ó��
        //
        // (Index DDL�� ���� ����ÿ��� ������ �ȴ�.)
        ideLog::log( IDE_SERVER_0,
                     "InternalError [%s:%u]\n"
                     "IndexLatch is not initialized yet.(trial to execute latchShared).\n",
                     (SChar *)idlVA::basename(__FILE__),
                     __LINE__ );

        ideLog::logCallStack( IDE_SERVER_0 );

        dumpTableHeader( aTableHeader );

        // DEBUG Mode�϶��� ASSERT�� ���δ�.
        IDE_DASSERT(0);
    }
    else
    {
        IDE_TEST( aTableHeader->mIndexLatch->lockRead( NULL, /* idvSQL* */
                                      NULL /* idvWeArgs* */ )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*
    Table Header�� Index Latch�� Ǯ���ش�.

    [IN] aTableHeader - Index Latch�� Ǯ���� Table�� Header
 */
IDE_RC smcTable::unlatch(smcTableHeader * aTableHeader )
{
    IDE_ASSERT( aTableHeader != NULL );

    if ( aTableHeader->mIndexLatch == NULL )
    {
        // ASSERT�� �Ŵ� ���� ������,
        // ���ۿ��� ������ �����Ƿ� ASSERT��� ��Ʈ�α׿� �ﵵ�� ó��
        //
        // (Index DDL�� ���� ����ÿ��� ������ �ȴ�.)
        ideLog::log( IDE_SERVER_0,
                     "InternalError [%s:%u]\n"
                     "IndexLatch is not initialized yet.(trial to execute unlatch).\n",
                     (SChar *)idlVA::basename(__FILE__),
                     __LINE__ );

        ideLog::logCallStack( IDE_SERVER_0 );

        dumpTableHeader( aTableHeader );

        // DEBUG Mode�϶��� ASSERT�� ���δ�.
        IDE_DASSERT(0);
    }
    else
    {
        IDE_TEST( aTableHeader->mIndexLatch->unlock(  )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : DropIndexByAbort�Լ��� ���ؼ� ȣ��Ǵµ� ���ο� Index Header
 *               Array�� ������ Index Header Array�� Create �� ��ҵ� Index��
 *               ������ Index Header���� �����ؼ� �Ű��ִ� �Լ��̴�.
 *
 * aTrans             - [IN] Transaction Pointer
 * aDropIndexHeader   - [IN] Drop�� Index Header
 * aIndexHeaderArr    - [IN] ������ Index Header Array
 * aIndexHeaderArrLen - [IN] Index Header Array�� ����
 * aOIDIndexHeaderArr - [IN] ���ο� Index Header Array�� ������ �� Var Row��
 *                           OID
 *
 * Related Issue :
 *   - BUG-17955: Add/Drop Column ������ smiStatement End��,
 *                Unable to invoke mutex_lock()�� ���� ������ ����
 *   - BUG-30612: index header self oid�� �߸� ��ϵ� ���� �ֽ��ϴ�.
 ***********************************************************************/
IDE_RC smcTable::removeIdxHdrAndCopyIndexHdrArr(
                                            void*     aTrans,
                                            void*     aDropIndexHeader,
                                            void*     aIndexHeaderArr,
                                            UInt      aIndexHeaderArrLen,
                                            smOID     aOIDIndexHeaderArr )
{
    scGRID            sGRID;
    SChar            *sDestIndexRowPtr;
    SChar            *sFstIndexHeader;
    smrUptLogImgInfo  sAftImgInfo;
    static UInt       sIndexHeaderSize = smLayerCallback::getSizeOfIndexHeader();
    UInt              sIdxCntInIdxArr = aIndexHeaderArrLen / sIndexHeaderSize;
    UInt              sCopySize ;

    IDE_ASSERT( sIdxCntInIdxArr - 1 != 0 );

    /* BUG-17955: Add/Drop Column ������ smiStatement End��, Unable to invoke
     * mutex_lock()�� ���� ������ ���� */
    /* After Image�� ���� ������ Index Header List�� ������ �ش�. */
    sFstIndexHeader  = (SChar*)aIndexHeaderArr + ID_SIZEOF( smVCPieceHeader );
    IDE_ASSERT( smmManager::getOIDPtr( 
                                    SMI_ID_TABLESPACE_SYSTEM_MEMORY_DIC, 
                                    aOIDIndexHeaderArr,
                                   (void**) &sDestIndexRowPtr )
                == IDE_SUCCESS );
    sDestIndexRowPtr += ID_SIZEOF( smVCPieceHeader );

    sCopySize = aIndexHeaderArrLen - sIndexHeaderSize;

    /* create �Ǵ� Index�� ó��, Ȥ�� �������� �߰��ȴ�.*/
    if( sFstIndexHeader == aDropIndexHeader )
    {
         // ù��° IndexHeader�� �����ϴ� ���
        idlOS::memcpy( sDestIndexRowPtr,
                       sFstIndexHeader + sIndexHeaderSize,
                       sCopySize );
    }
    else
    {
        // ������ IndexHeader�� �����ϴ� ���
        IDE_ASSERT( (UInt)((SChar*)aDropIndexHeader - sFstIndexHeader) == sCopySize );

        idlOS::memcpy( sDestIndexRowPtr,
                       sFstIndexHeader,
                       sCopySize );
    }

    /* BUG-30612 index header self oid�� �߸� ��ϵ� �� �ֽ��ϴ�.
     * �ٸ� slot���� ������ �� ���̹Ƿ� self oid�� �߸��Ǿ� �ִ�.
     * self oid�� �����Ѵ�. */
    adjustIndexSelfOID( (UChar*)sDestIndexRowPtr,
                        aOIDIndexHeaderArr + ID_SIZEOF(smVCPieceHeader),
                        sCopySize );

    sAftImgInfo.mLogImg = sDestIndexRowPtr;
    sAftImgInfo.mSize   = sCopySize;

    SC_MAKE_GRID(
        sGRID,
        SMI_ID_TABLESPACE_SYSTEM_MEMORY_DIC,
        SM_MAKE_PID( aOIDIndexHeaderArr ),
        SM_MAKE_OFFSET( aOIDIndexHeaderArr ) + ID_SIZEOF( smVCPieceHeader ));

    /* redo only log�̹Ƿ� wal�� ��Ű�� �ʾƵ� �ȴ�. */
    IDE_TEST( smrUpdate::writeUpdateLog( NULL, /* idvSQL* */
                                         aTrans,
                                         SMR_PHYSICAL,
                                         sGRID,
                                         0, /* smrUpdateLog::mData */
                                         0, /* Before Image Count */
                                         NULL, /* Before Image Info Ptr */
                                         1, /* After Image Count */
                                         &sAftImgInfo )
              != IDE_SUCCESS );

    IDE_TEST(smmDirtyPageMgr::insDirtyPage(
                                     SMI_ID_TABLESPACE_SYSTEM_MEMORY_DIC,
                                     SM_MAKE_PID( aOIDIndexHeaderArr ) )
             != IDE_SUCCESS);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/***********************************************************************
 * Description : Table�� Flag�� �����Ѵ�.
 *
 * aTrans       [IN] Flag�� ������ Ʈ�����
 * aTableHeader [IN] Flag�� ������ Table�� Header
 * aFlagMask    [IN] ������ Table Flag�� Mask
 * aFlagValue   [IN] ������ Table Flag�� Value
 **********************************************************************/
IDE_RC smcTable::alterTableFlag( void           * aTrans,
                                 smcTableHeader * aTableHeader,
                                 UInt             aFlagMask,
                                 UInt             aFlagValue )
{
    IDE_DASSERT( aTrans != NULL);
    IDE_DASSERT( aTableHeader != NULL );
    IDE_DASSERT( aFlagMask != 0);
    // Mask���� Bit�� Value�� ����ؼ��� �ȵȴ�.
    IDE_DASSERT( (~aFlagMask & aFlagValue) == 0 );

    UInt sNewFlag;

    // Table Flag���� ���� ���� üũ �ǽ�
    IDE_TEST( checkError4AlterTableFlag( aTableHeader,
                                         aFlagMask,
                                         aFlagValue )
              != IDE_SUCCESS );

    sNewFlag = aTableHeader->mFlag;

    // Mask Bit�� ��� Clear
    sNewFlag = sNewFlag & ~aFlagMask;
    // Value Bit�� Set
    sNewFlag = sNewFlag | aFlagValue;

    IDE_TEST(
        smrUpdate::updateFlagAtTableHead(
            NULL,
            aTrans,
            aTableHeader,
            sNewFlag)
        != IDE_SUCCESS );

    aTableHeader->mFlag = sNewFlag;

    IDE_TEST( smmDirtyPageMgr::insDirtyPage(
                  SMI_ID_TABLESPACE_SYSTEM_MEMORY_DIC,
                  SM_MAKE_PID(aTableHeader->mSelfOID))
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : Table�� Inconsistent�ϴٰ� �����Ѵ�.
 *
 * aTableHeader [IN/OUT] Flag�� ������ Table�� Header
 **********************************************************************/
IDE_RC smcTable::setTableHeaderInconsistency( smOID            aTableOID )
{
    smcTableHeader   * sTableHeader;

    IDE_TEST( smcTable::getTableHeaderFromOID( aTableOID,
                                               (void**)&sTableHeader )
              != IDE_SUCCESS );

    IDE_ERROR_MSG( aTableOID == sTableHeader->mSelfOID,
                   "aTableOID : %"ID_UINT64_FMT
                   "sTableOID : %"ID_UINT64_FMT,
                   aTableOID,
                   sTableHeader->mSelfOID );

    /* Inconsistent������ table ���� dump */
    dumpTableHeader( sTableHeader );

    /* ���� �̹����󿡵� �����ϴ� ����, RestartRecovery�ÿ���
     * �����ؾ��Ѵ�. */
    IDE_TEST( smrUpdate::setInconsistencyAtTableHead(
            sTableHeader,
            ID_FALSE ) // aForMediaRecovery
        != IDE_SUCCESS );

    /* Inconsistent -> Consistent�� ���� */
    sTableHeader->mIsConsistent = ID_FALSE;

    IDE_TEST( smmDirtyPageMgr::insDirtyPage(
                  SMI_ID_TABLESPACE_SYSTEM_MEMORY_DIC,
                  SM_MAKE_PID(sTableHeader->mSelfOID))
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}


IDE_RC smcTable::setTableHeaderDropFlag( scSpaceID    aSpaceID,
                                         scPageID     aPID,
                                         scOffset     aOffset,
                                         idBool       aIsDrop )
{
    smpSlotHeader *sSlotHeader;

    IDE_ASSERT( smmManager::getOIDPtr( aSpaceID,
                                       SM_MAKE_OID( aPID, aOffset ),
                                       (void**)&sSlotHeader )
                == IDE_SUCCESS );

    if( aIsDrop == ID_TRUE )
    {
        SMP_SLOT_SET_DROP( sSlotHeader );
    }
    else
    {
        SMP_SLOT_UNSET_DROP( sSlotHeader );
    }

    IDE_TEST(smmDirtyPageMgr::insDirtyPage(aSpaceID, aPID) != IDE_SUCCESS);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*
    Create Table�� ���� ����ó��
    [IN] aSpaceID   - ���̺����̽��� ID
    [IN] aTableFlag - ������ ���̺��� Flag
*/
IDE_RC smcTable::checkError4CreateTable( scSpaceID aSpaceID,
                                         UInt      aTableFlag )
{
    // 1. Volatile Table�� ���� �α� �����ϵ��� ������ ��� ����
    if ( sctTableSpaceMgr::isVolatileTableSpace( aSpaceID ) == ID_TRUE )
    {
        if ( (aTableFlag & SMI_TABLE_LOG_COMPRESS_MASK) ==
             SMI_TABLE_LOG_COMPRESS_TRUE )
        {
            IDE_RAISE( err_volatile_log_compress );
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( err_volatile_log_compress );
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_UNABLE_TO_COMPRESS_VOLATILE_TBS_LOG));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/*
    Table�� Flag���濡 ���� ����ó��

    [IN] aTableHeader - ������ ���̺��� ���
    [IN] aFlagMask    - ������ Table Flag�� Mask
    [IN] aFlagValue   - ������ Table Flag�� Value
 */
IDE_RC smcTable::checkError4AlterTableFlag( smcTableHeader * aTableHeader,
                                            UInt             aFlagMask,
                                            UInt             aFlagValue )
{
    IDE_DASSERT( aTableHeader != NULL );

    // 1. Volatile Table�� ���� �α� �����ϵ��� ������ ����
    if ( sctTableSpaceMgr::isVolatileTableSpace(
                               aTableHeader->mSpaceID ) == ID_TRUE )
    {
        if ( (aFlagMask & SMI_TABLE_LOG_COMPRESS_MASK) != 0 )
        {
            if ( (aFlagValue & SMI_TABLE_LOG_COMPRESS_MASK )
                == SMI_TABLE_LOG_COMPRESS_TRUE )
            {

                IDE_RAISE( err_volatile_log_compress );
            }
        }
    }


    // 2. �̹� Table Header�� �ش� Flag�� �����Ǿ� �ִ� ��� ����
    IDE_TEST_RAISE( (aTableHeader->mFlag & aFlagMask) == aFlagValue,
                    err_table_flag_already_set );

    return IDE_SUCCESS;

    IDE_EXCEPTION( err_table_flag_already_set );
    {
        IDE_SET(ideSetErrorCode( smERR_ABORT_TABLE_ATTR_FLAG_ALREADY_SET));
    }
    IDE_EXCEPTION( err_volatile_log_compress );
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_UNABLE_TO_COMPRESS_VOLATILE_TBS_LOG));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*******************************************************************************
 * Description : Replication�� ���ؼ� Table Meta�� ����Ѵ�.
 *
 * [IN] aTrans      - Log Buffer�� ������ Transaction
 * [IN] aTableMeta  - ����� Table Meta�� ���
 * [IN] aLogBody    - ����� Log Body
 * [IN] aLogBodyLen - ����� Log Body�� ����
 ******************************************************************************/
IDE_RC smcTable::writeTableMetaLog(void         * aTrans,
                                   smrTableMeta * aTableMeta,
                                   const void   * aLogBody,
                                   UInt           aLogBodyLen)
{
    smrTableMetaLog sLogHeader;
    smTID           sTransID;
    smrLogType      sLogType = SMR_LT_TABLE_META;
    UInt            sOffset;

    IDE_ASSERT(aTrans != NULL);
    IDE_ASSERT(aTableMeta != NULL);

    smLayerCallback::initLogBuffer( aTrans );
    sOffset = 0;

    /* Log header�� �����Ѵ�. */
    idlOS::memset(&sLogHeader, 0, ID_SIZEOF(smrTableMetaLog));

    sTransID = smLayerCallback::getTransID( aTrans );

    smrLogHeadI::setType(&sLogHeader.mHead, sLogType);

    smrLogHeadI::setSize(&sLogHeader.mHead,
                         SMR_LOGREC_SIZE(smrTableMetaLog) +
                         aLogBodyLen +
                         ID_SIZEOF(smrLogTail));

    smrLogHeadI::setTransID(&sLogHeader.mHead, sTransID);

    smrLogHeadI::setPrevLSN( &sLogHeader.mHead,
                             smLayerCallback::getLstUndoNxtLSN( aTrans ) );

    smrLogHeadI::setFlag(&sLogHeader.mHead, SMR_LOG_TYPE_NORMAL);

    if( (smrLogHeadI::getFlag(&sLogHeader.mHead) & SMR_LOG_SAVEPOINT_MASK)
         == SMR_LOG_SAVEPOINT_OK)
    {
        smrLogHeadI::setReplStmtDepth( &sLogHeader.mHead,
                                       smLayerCallback::getLstReplStmtDepth( aTrans ) );
    }
    else
    {
        smrLogHeadI::setReplStmtDepth( &sLogHeader.mHead,
                                       SMI_STATEMENT_DEPTH_NULL );
    }

    sLogHeader.mTableMeta = *aTableMeta;

    /* Write log header */
    IDE_TEST( smLayerCallback::writeLogToBuffer( aTrans,
                                                 (const void *)&sLogHeader,
                                                 sOffset,
                                                 SMR_LOGREC_SIZE(smrTableMetaLog) )
              != IDE_SUCCESS );
    sOffset += SMR_LOGREC_SIZE(smrTableMetaLog);

    /* Write log body */
    if (aLogBody != NULL)
    {
        IDE_ASSERT(aLogBodyLen != 0);
        IDE_TEST( smLayerCallback::writeLogToBuffer( aTrans,
                                                     aLogBody,
                                                     sOffset,
                                                     aLogBodyLen )
                  != IDE_SUCCESS );
        sOffset += aLogBodyLen;
    }

    /* Write log tail */
    IDE_TEST( smLayerCallback::writeLogToBuffer( aTrans,
                                                 (const void *)&sLogType,
                                                 sOffset,
                                                 ID_SIZEOF(smrLogType) )
              != IDE_SUCCESS );

    IDE_TEST( smLayerCallback::writeTransLog( aTrans, sLogHeader.mTableMeta.mOldTableOID ) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : Disk Table�� Insert Limit�� �����Ѵ�.
 *
 * aTrans       [IN] Flag�� ������ Ʈ�����
 * aTableHeader [IN] Flag�� ������ Table�� Header
 * aSegAttr     [IN] ������ Table Insert Limit
 **********************************************************************/
IDE_RC smcTable::alterTableSegAttr( void                  * aTrans,
                                    smcTableHeader        * aHeader,
                                    smiSegAttr              aSegAttr,
                                    sctTBSLockValidOpt      aTBSLvOpt )
{
    IDE_DASSERT( aTrans != NULL);
    IDE_DASSERT( aHeader != NULL );

    // Disk Table�� �ƴϸ� ���ǹ��ϹǷ� �����Ѵ�.
    IDE_TEST_RAISE( SMI_TABLE_TYPE_IS_DISK( aHeader ) == ID_FALSE,
                    not_support_error );

    // DDL�� �̹Ƿ� Table Validation�� �����Ѵ�.
    IDE_TEST( validateTable( aTrans,
                             aHeader,
                             aTBSLvOpt,
                             SMC_LOCK_TABLE )
              != IDE_SUCCESS );

    IDE_TEST( smrUpdate::updateInsLimitAtTableHead(
                          NULL,
                          aTrans,
                          aHeader,
                          aHeader->mFixed.mDRDB.mSegDesc.mSegHandle.mSegAttr,
                          aSegAttr ) != IDE_SUCCESS );

    sdpSegDescMgr::setSegAttr( &aHeader->mFixed.mDRDB.mSegDesc,
                               &aSegAttr );

    IDE_TEST( smmDirtyPageMgr::insDirtyPage(
                  SMI_ID_TABLESPACE_SYSTEM_MEMORY_DIC,
                  SM_MAKE_PID(aHeader->mSelfOID))
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION( not_support_error );
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_Not_Support_function));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : Disk Table�� Storage Attribute�� �����Ѵ�.
 *
 * aTrans       [IN] Flag�� ������ Ʈ�����
 * aTableHeader [IN] Flag�� ������ Table�� Header
 * aSegStoAttr  [IN] ������ Table Storage Attribute
 **********************************************************************/
IDE_RC smcTable::alterTableSegStoAttr( void               * aTrans,
                                       smcTableHeader     * aHeader,
                                       smiSegStorageAttr    aSegStoAttr,
                                       sctTBSLockValidOpt   aTBSLvOpt )
{
    IDE_DASSERT( aTrans != NULL);
    IDE_DASSERT( aHeader != NULL );

    // Disk Table�� �ƴϸ� ���ǹ��ϹǷ� �����Ѵ�.
    IDE_TEST_RAISE( SMI_TABLE_TYPE_IS_DISK( aHeader ) == ID_FALSE,
                    not_support_error );

    // DDL�� �̹Ƿ� Table Validation�� �����Ѵ�.
    IDE_TEST( validateTable( aTrans,
                             aHeader,
                             aTBSLvOpt,
                             SMC_LOCK_TABLE )
              != IDE_SUCCESS );

    IDE_TEST( smrUpdate::updateSegStoAttrAtTableHead(
                  NULL,
                  aTrans,
                  aHeader,
                  aHeader->mFixed.mDRDB.mSegDesc.mSegHandle.mSegStoAttr,
                  aSegStoAttr )
              != IDE_SUCCESS );

    sdpSegDescMgr::setSegStoAttr( &aHeader->mFixed.mDRDB.mSegDesc,
                                  &aSegStoAttr );

    IDE_TEST( smmDirtyPageMgr::insDirtyPage(
                  SMI_ID_TABLESPACE_SYSTEM_MEMORY_DIC,
                  SM_MAKE_PID(aHeader->mSelfOID))
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION( not_support_error );
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_Not_Support_function));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : Disk Index�� INIT/MAX TRANS�� �����Ѵ�.
 *
 * aTrans       [IN] Flag�� ������ Ʈ�����
 * aTableHeader [IN] Flag�� ������ Table�� Header
 * aSegAttr     [IN] ������ INDEX INIT/MAX TRANS
 **********************************************************************/
IDE_RC smcTable::alterIndexSegAttr( void                  * aTrans,
                                    smcTableHeader        * aHeader,
                                    void                  * aIndex,
                                    smiSegAttr              aSegAttr,
                                    sctTBSLockValidOpt      aTBSLvOpt )
{
    UInt         sIndexSlot;
    SInt         sOffset;
    UInt         sIndexHeaderSize;
    smiSegAttr * sBfrSegAttr;

    IDE_DASSERT( aTrans != NULL);
    IDE_DASSERT( aHeader != NULL );

    // Disk Table�� �ƴϸ� ���ǹ��ϹǷ� �����Ѵ�.
    IDE_TEST_RAISE( SMI_TABLE_TYPE_IS_DISK( aHeader ) == ID_FALSE,
                    not_support_error );

    sIndexHeaderSize = smLayerCallback::getSizeOfIndexHeader();

    // DDL�� �̹Ƿ� Table Validation�� �����Ѵ�.
    IDE_TEST( validateTable( aTrans,
                             aHeader,
                             aTBSLvOpt,
                             SMC_LOCK_TABLE )
              != IDE_SUCCESS );

    // BUG-23218 : �ε��� Ž�� ���ȭ
    IDE_ASSERT( findIndexVarSlot2AlterAndDrop(aHeader,
                                              aIndex,
                                              sIndexHeaderSize,
                                              &sIndexSlot,
                                              &sOffset)
                 == IDE_SUCCESS );

    sBfrSegAttr = smLayerCallback::getIndexSegAttrPtr( aIndex );

    IDE_TEST(smrUpdate::setIdxHdrSegAttr(
                 NULL, /* idvSQL* */
                 aTrans,
                 aHeader->mIndexes[sIndexSlot].fstPieceOID,
                 ID_SIZEOF(smVCPieceHeader) + sOffset , //BUG-23218 : �α� ��ġ ���� ����
                 *sBfrSegAttr,
                 aSegAttr ) != IDE_SUCCESS);

    smLayerCallback::setIndexSegAttrPtr( aIndex, aSegAttr );

    IDE_TEST( smmDirtyPageMgr::insDirtyPage(
                  SMI_ID_TABLESPACE_SYSTEM_MEMORY_DIC,
                  SM_MAKE_PID(aHeader->mSelfOID))
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION( not_support_error );
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_Not_Support_function));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : Disk Index�� Storage Attribute�� �����Ѵ�.
 *
 * aTrans       [IN] Flag�� ������ Ʈ�����
 * aHeader      [IN] Flag�� ������ Table�� Header
 * aIndex       [IN] Flag�� ������ Index�� Header
 * aSegStoAttr  [IN] ������ Storage Attribute
 **********************************************************************/
IDE_RC smcTable::alterIndexSegStoAttr( void                * aTrans,
                                       smcTableHeader      * aHeader,
                                       void                * aIndex,
                                       smiSegStorageAttr     aSegStoAttr,
                                       sctTBSLockValidOpt    aTBSLvOpt )
{
    smnIndexHeader    *sIndexHeader;
    UInt               sIndexHeaderSize;
    UInt               sIndexSlot;
    scPageID           sPageID = SC_NULL_PID;
    SInt               sOffset;
    smiSegStorageAttr* sBfrSegStoAttr;
    smiSegStorageAttr  sAftSegStoAttr;

    IDE_DASSERT( aTrans != NULL);
    IDE_DASSERT( aHeader != NULL );

    // Disk Table�� �ƴϸ� ���ǹ��ϹǷ� �����Ѵ�.
    IDE_TEST_RAISE( SMI_TABLE_TYPE_IS_DISK( aHeader ) == ID_FALSE,
                    not_support_error );

    sIndexHeader = ((smnIndexHeader*)aIndex);
    sIndexHeaderSize = smLayerCallback::getSizeOfIndexHeader();

    /* ----------------------------
     * [1] table validation
     * ---------------------------*/
    IDE_TEST( validateTable( aTrans,
                             aHeader,
                             aTBSLvOpt )
              != IDE_SUCCESS );

    // BUG-23218 : �ε��� Ž��  ���ȭ
    /* ----------------------------
     * [2] �����ϰ��� �ϴ� Index�� ã�´�.
     * ---------------------------*/
    IDE_ASSERT( findIndexVarSlot2AlterAndDrop(aHeader,
                                              sIndexHeader,
                                              sIndexHeaderSize,
                                              &sIndexSlot,
                                              &sOffset)
                 == IDE_SUCCESS );

    /* ------------------------------------------------
     * [3] ã�� �ε����� sIndexSlot�� ��������,
     *     �ε����� ��ġ�� ����Ѵ�.
     * ------------------------------------------------*/
    sPageID = SM_MAKE_PID( aHeader->mIndexes[sIndexSlot].fstPieceOID );

    /* ---------------------------------------------
     * [4] ������ Segment Storage Attribute�� ������
     * before/after �̹����� �����.
     * ---------------------------------------------*/

    sBfrSegStoAttr = smLayerCallback::getIndexSegStoAttrPtr( aIndex );
    sAftSegStoAttr = *sBfrSegStoAttr;

    if ( aSegStoAttr.mInitExtCnt != 0 )
    {
        sAftSegStoAttr.mInitExtCnt = aSegStoAttr.mInitExtCnt;
    }
    if ( aSegStoAttr.mNextExtCnt != 0 )
    {
        sAftSegStoAttr.mNextExtCnt = aSegStoAttr.mNextExtCnt;
    }
    if ( aSegStoAttr.mMinExtCnt != 0 )
    {
        sAftSegStoAttr.mMinExtCnt = aSegStoAttr.mMinExtCnt;
    }
    if ( aSegStoAttr.mMaxExtCnt != 0 )
    {
        sAftSegStoAttr.mMaxExtCnt = aSegStoAttr.mMaxExtCnt;
    }

    /* ---------------------------------------------
     * [5] �α� �� �����Ѵ�.
     * ---------------------------------------------*/
    IDE_TEST(smrUpdate::setIdxHdrSegStoAttr(
                 NULL, /* idvSQL* */
                 aTrans,
                 aHeader->mIndexes[sIndexSlot].fstPieceOID,
                 ID_SIZEOF(smVCPieceHeader) + sOffset, // BUG-23218 : �α� ��ġ ���� ����
                 *sBfrSegStoAttr,
                 sAftSegStoAttr ) != IDE_SUCCESS);


    smLayerCallback::setIndexSegStoAttrPtr( aIndex, sAftSegStoAttr );

    (void)smmDirtyPageMgr::insDirtyPage(
        SMI_ID_TABLESPACE_SYSTEM_MEMORY_DIC, sPageID);

    return IDE_SUCCESS;

    IDE_EXCEPTION( not_support_error );
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_Not_Support_function));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC smcTable::alterTableAllocExts( idvSQL              * aStatistics,
                                      void                * aTrans,
                                      smcTableHeader      * aHeader,
                                      ULong                 aExtendSize,
                                      sctTBSLockValidOpt    aTBSLvOpt )
{
    IDE_DASSERT( aTrans != NULL);
    IDE_DASSERT( aHeader != NULL );

    // Disk Table�� �ƴϸ� ���ǹ��ϹǷ� �����Ѵ�.
    IDE_TEST_RAISE( SMI_TABLE_TYPE_IS_DISK( aHeader ) == ID_FALSE,
                    not_support_error );

    // DDL�� �̹Ƿ� Table Validation�� �����Ѵ�.
    IDE_TEST( validateTable( aTrans,
                             aHeader,
                             aTBSLvOpt,
                             SMC_LOCK_TABLE ) != IDE_SUCCESS );

    IDE_TEST( sdpSegment::allocExts( aStatistics,
                                     aHeader->mSpaceID,
                                     aTrans,
                                     &aHeader->mFixed.mDRDB.mSegDesc,
                                     aExtendSize ) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION( not_support_error );
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_Not_Support_function));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC smcTable::alterIndexAllocExts( idvSQL              * aStatistics,
                                      void                * aTrans,
                                      smcTableHeader      * aHeader,
                                      void                * aIndex,
                                      ULong                 aExtendSize,
                                      sctTBSLockValidOpt    aTBSLvOpt )
{
    const scGRID   *sIndexGRID;

    IDE_DASSERT( aTrans != NULL);
    IDE_DASSERT( aHeader != NULL );

    // Disk Table�� �ƴϸ� ���ǹ��ϹǷ� �����Ѵ�.
    IDE_TEST_RAISE( SMI_TABLE_TYPE_IS_DISK( aHeader ) == ID_FALSE,
                    not_support_error );

    // DDL�� �̹Ƿ� Table Validation�� �����Ѵ�.
    IDE_TEST( validateTable( aTrans,
                             aHeader,
                             aTBSLvOpt,
                             SMC_LOCK_TABLE ) != IDE_SUCCESS );

    sIndexGRID   = smLayerCallback::getIndexSegGRIDPtr( aIndex );

    IDE_TEST( sdpSegment::allocExts( aStatistics,
                                     SC_MAKE_SPACE(*sIndexGRID),
                                     aTrans,
                                     (sdpSegmentDesc*)smLayerCallback::getSegDescByIdxPtr( aIndex ),
                                     aExtendSize )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION( not_support_error );
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_Not_Support_function));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


//fix BUG-22395
IDE_RC smcTable::alterIndexName( idvSQL              * aStatistics,
                                 void                * aTrans,
                                 smcTableHeader      * aHeader,
                                 void                * aIndex,
                                 SChar               * aName )
{
    smnIndexHeader  *sIndexHeader;
    UInt             sTableType;
    UInt             sIndexHeaderSize;
    UInt             sIndexSlot;
    SInt             sIndexOffset;
    scPageID         sPageID = SC_NULL_PID;
    scOffset         sNameOffset = SC_NULL_OFFSET;

    IDE_DASSERT( aTrans != NULL);
    IDE_DASSERT( aHeader != NULL );

    sTableType = SMI_GET_TABLE_TYPE( aHeader );
    IDE_TEST_RAISE( !(sTableType == SMI_TABLE_MEMORY   ||
                      sTableType == SMI_TABLE_DISK     ||
                      sTableType == SMI_TABLE_VOLATILE ),
                    not_support_error );

    sIndexHeader = ((smnIndexHeader*)aIndex);
    sIndexHeaderSize = smLayerCallback::getSizeOfIndexHeader();

    /* ----------------------------
     * [1] table validation
     * ---------------------------*/
    IDE_TEST( validateTable( aTrans,
                             aHeader,
                             SCT_VAL_DDL_DML )
              != IDE_SUCCESS );

    /* ----------------------------
     * [2] �����ϰ��� �ϴ� Index�� ã�´�.
     * ---------------------------*/
    IDE_ASSERT( findIndexVarSlot2AlterAndDrop(aHeader,
                                              sIndexHeader,
                                              sIndexHeaderSize,
                                              &sIndexSlot,
                                              &sIndexOffset)
                 == IDE_SUCCESS );

    /* ------------------------------------------------
     * [3] ã�� �ε����� sIndexSlot�� offset�� ��������,
     * ------------------------------------------------*/
    sPageID     = SM_MAKE_PID( aHeader->mIndexes[sIndexSlot].fstPieceOID );
    sNameOffset = SM_MAKE_OFFSET( aHeader->mIndexes[sIndexSlot].fstPieceOID )
                  + ID_SIZEOF(smVCPieceHeader) 
                  + sIndexOffset + smLayerCallback::getIndexNameOffset() ;

    /* ----------------------------
     * [4] �α� �� �̸��� �����Ѵ�.
     * ----------------------------*/
    IDE_TEST( smrUpdate::physicalUpdate( aStatistics,
                                         aTrans,
                                         SMI_ID_TABLESPACE_SYSTEM_MEMORY_DIC,
                                         sPageID,
                                         sNameOffset,
                                         smLayerCallback::getNameOfIndexHeader( sIndexHeader ), //before image
                                         SMN_MAX_INDEX_NAME_SIZE,
                                         aName, //after image
                                         SMN_MAX_INDEX_NAME_SIZE,
                                         NULL,
                                         0)
              != IDE_SUCCESS );

    smLayerCallback::setNameOfIndexHeader( sIndexHeader, aName );

    (void)smmDirtyPageMgr::insDirtyPage( SMI_ID_TABLESPACE_SYSTEM_MEMORY_DIC, 
                                         sPageID);

    return IDE_SUCCESS;

    IDE_EXCEPTION( not_support_error );
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_Not_Support_function));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : MEMORY TABLE�� FREE PAGE���� DB�� �ݳ��Ѵ�.
 *
 * aTable     [IN] COMPACT�� TABLE�� ���
 * aTrans     [IN] Transaction Pointer
 * aPages	  [IN] Numer of Pages for compaction (0, UINT_MAX : all)
 **********************************************************************/
IDE_RC smcTable::compact( void           * aTrans,
                          smcTableHeader * aTable,
                          UInt             aPages )
{
    UInt               i;
    smpPageListEntry * sPageListEntry;
    smpPageListEntry * sVolPageListEntry;

    IDE_DASSERT(aTrans != NULL);
    IDE_DASSERT(aTable != NULL);

    // PRJ-1548 User Memory TableSpace ���䵵��
    // Validate Table ���� ������ ���� Lock�� ȹ���Ѵ�.
    // [1] Table�� TableSpace�� ���� IX
    // [2] Table�� ���� X
    // [3] Table�� Index, Lob Column TableSpace�� ���� IX
    // Table�� ������ Table�� TableSpace�̸� �׿� ���� IX�� ȹ���Ѵ�.
    // ��, �޸� Table�� [3] ���׿� ���ؼ� �������� �ʴ´�.
    // BUGBUG-���ο��� ValidateTable �� �������� ����.

    /* PROJ-1594 Volatile TBS */
    /* compact�� ���̺��� memory ���̺� �Ǵ� volatile ���̺��̴�. */
    if ( (SMI_TABLE_TYPE_IS_MEMORY( aTable ) == ID_TRUE) ||
         (SMI_TABLE_TYPE_IS_META( aTable ) == ID_TRUE) )
    {
        // Pool�� ��ϵ� FreePage(EmptyPage)�� �ʹ� ������ DB�� �ݳ��Ѵ�.
        // for Fixed
        sPageListEntry = &(aTable->mFixed.mMRDB);

        if ( sPageListEntry->mRuntimeEntry->mFreePagePool.mPageCount
            > SMP_FREEPAGELIST_MINPAGECOUNT )
        {
            IDE_TEST( smpFreePageList::freePagesFromFreePagePoolToDB(
                                                              aTrans,
                                                              aTable->mSpaceID,
                                                              sPageListEntry,
                                                              aPages )
                      != IDE_SUCCESS );
        }


        // for Var
        for( i = 0; i < SM_VAR_PAGE_LIST_COUNT ; i++ )
        {
            sPageListEntry = aTable->mVar.mMRDB + i;

            if( sPageListEntry->mRuntimeEntry->mFreePagePool.mPageCount
                > SMP_FREEPAGELIST_MINPAGECOUNT )
            {
                IDE_TEST( smpFreePageList::freePagesFromFreePagePoolToDB(
                                                          aTrans,
                                                          aTable->mSpaceID,
                                                          sPageListEntry,
                                                          aPages )
                          != IDE_SUCCESS );
            }
        }
    }
    else
    {
        IDE_ASSERT( SMI_TABLE_TYPE_IS_VOLATILE( aTable ) == ID_TRUE );

        // Pool�� ��ϵ� FreePage(EmptyPage)�� �ʹ� ������ DB�� �ݳ��Ѵ�.
        // for Fixed
        sVolPageListEntry = &(aTable->mFixed.mVRDB);

        if( sVolPageListEntry->mRuntimeEntry->mFreePagePool.mPageCount
            > SMP_FREEPAGELIST_MINPAGECOUNT )
        {
            IDE_TEST( svpFreePageList::freePagesFromFreePagePoolToDB(
                                                      aTrans,
                                                      aTable->mSpaceID,
                                                      sVolPageListEntry,
                                                      aPages )
                      != IDE_SUCCESS );
        }

        // for Var
        for( i = 0; i < SM_VAR_PAGE_LIST_COUNT ; i++ )
        {
            sVolPageListEntry = aTable->mVar.mVRDB + i;

            if( sVolPageListEntry->mRuntimeEntry->mFreePagePool.mPageCount
                > SMP_FREEPAGELIST_MINPAGECOUNT )
            {
                IDE_TEST( svpFreePageList::freePagesFromFreePagePoolToDB(
                                                      aTrans,
                                                      aTable->mSpaceID,
                                                      sVolPageListEntry,
                                                      aPages )
                          != IDE_SUCCESS );
            }
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

idBool smcTable::isSegPIDOfTbl( void* aTable, scSpaceID aSpaceID, scPageID aSegPID )
{
    smcTableHeader *sTblHdr = (smcTableHeader*)((smpSlotHeader*)aTable + 1);

    if( ( aSegPID  == sdpPageList::getTableSegDescPID( &sTblHdr->mFixed.mDRDB ) ) &&
        ( aSpaceID == sTblHdr->mSpaceID ) )
    {
        return ID_TRUE;
    }
    else
    {
        return ID_FALSE;
    }
}

scPageID smcTable::getSegPID( void* aTable )
{
    smcTableHeader *sTblHdr = (smcTableHeader*)((smpSlotHeader*)aTable + 1);

    return sdpPageList::getTableSegDescPID( &sTblHdr->mFixed.mDRDB );
}

idBool smcTable::isIdxSegPIDOfTbl( void* aTable, scSpaceID aSpaceID, scPageID aSegPID )
{
    UInt            sIdxCnt;
    UInt            i;
    const void     *sIndex;
    const scGRID   *sIndexGRID;

    sIdxCnt = getIndexCount( aTable );

    for( i = 0; i < sIdxCnt ; i++ )
    {
        sIndex = getTableIndex( aTable, i );
        sIndexGRID = smLayerCallback::getIndexSegGRIDPtr( (void*)sIndex );

        if( ( SC_MAKE_SPACE(*sIndexGRID) == aSpaceID ) &&
            ( SC_MAKE_PID(*sIndexGRID)  == aSegPID ) )
        {
            break;
        }
    }

    if( i == sIdxCnt )
    {
        return ID_FALSE;
    }
    else
    {
        return ID_TRUE;
    }
}

idBool smcTable::isLOBSegPIDOfTbl( void* aTable, scSpaceID aSpaceID, scPageID aSegPID )
{
    const smiColumn *sColumn;
    smcTableHeader  *sTblHdr = (smcTableHeader*)((smpSlotHeader*)aTable + 1);

    UInt       sColumnCnt = getColumnCount( sTblHdr );
    UInt       i;

    for( i = 0; i < sColumnCnt; i++ )
    {
        sColumn = smcTable::getColumn( sTblHdr, i );

        if( ( sColumn->flag & SMI_COLUMN_TYPE_MASK ) ==
            SMI_COLUMN_TYPE_LOB )
        {
            if( ( sColumn->colSeg.mSpaceID == aSpaceID ) &&
                ( sColumn->colSeg.mPageID  == aSegPID ) )
            {
                break;
            }
        }
    }

    if( i == sColumnCnt )
    {
        return ID_FALSE;
    }
    else
    {
        return ID_TRUE;
    }
}

/***********************************************************************
 * Description : Replicate Log�� ����ؾ� �ϴ� Table���� �Ǵ��Ѵ�.
 *
 * Implementation :
 *
 *  aTableHeader - [IN] Replicatiohn ������ Ȯ���� Table �� Header
 *  aTrans       - [IN] Transactioin
 **********************************************************************/

idBool smcTable::needReplicate(const smcTableHeader* aTableHeader,
                               void*  aTrans)
{
    idBool sReplicate;
    UInt   sLogTypeFlag;

    IDE_DASSERT( aTableHeader != NULL );
    IDE_DASSERT( aTrans != NULL );

    sReplicate    = smcTable::isReplicationTable(aTableHeader);
    sLogTypeFlag  = smLayerCallback::getLogTypeFlagOfTrans( aTrans );

    /* �ش� Table�� Replication�� �ɷ� �ְ� ���� Transaction�� Normal
     * Transaction�̶�� Replication Sender�� ���� �ִ� ���·� �α׸� ���ܾ���
     * PROJ-1608 recovery from replication
     * Replication�� �̿��� ������ ���ؼ� Normal(SMR_LOG_TYPE_NORMAL)�� �ƴ�
     * recovery�� �����ϴ� receiver�� ������ Ʈ�����(SMR_LOG_TYPE_REPL_RECOVERY)��
     * Recovery Sender�� ���� �� �ֵ��� ����ؾ� �ϹǷ� Replicate�� �� �ִ� ���·�
     * ��ϵǾ�� �Ѵ�.
     */
    if(sReplicate == ID_TRUE)
    {
        if((sLogTypeFlag == SMR_LOG_TYPE_NORMAL) ||
           (sLogTypeFlag == SMR_LOG_TYPE_REPL_RECOVERY))
        {
            sReplicate = ID_TRUE;
        }
        else
        {
            sReplicate = ID_FALSE;
        }
    }
    else
    {
        sReplicate = ID_FALSE;
    }

    /* PROJ-1723 [MDW/INTEGRATOR] Altibase Plugin ����

       Supplemental Logging �ϴ� ��� Replication Transaction����
       ������� ������ Replication �α׸� ����Ѵ�
    */
    if ( smcTable::isSupplementalTable(aTableHeader) == ID_TRUE )
    {
        sReplicate = ID_TRUE;
    }

    return sReplicate;
}

/***********************************************************************
 * Description : index oid�κ��� index header�� ��´�.
 * index�� OID�� index header�� �ٷ� ����Ų��.
 ***********************************************************************/
IDE_RC smcTable::getIndexHeaderFromOID( smOID     aOID, 
                                        void   ** aHeader )
{
    IDE_TEST( smmManager::getOIDPtr( SMI_ID_TABLESPACE_SYSTEM_MEMORY_DIC, 
                                     aOID,
                                     (void**)aHeader ) 
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description :
 * create Table���� commitSCN ��smcTableHeader.mCreateSCN �� �����Ѵ�. 
 * drop Table ���� smcTableHeader.mCreateSCN �ʱ�ȭ (SM_SCN_INFINITE)
 * ager �� ���� ���� (BUG-48588)
 ***********************************************************************/
IDE_RC smcTable::setTableCreateSCN( smcTableHeader * aHeader, smSCN  aSCN )
{
    aHeader->mTableCreateSCN = aSCN; 

    return smmDirtyPageMgr::insDirtyPage( SMI_ID_TABLESPACE_SYSTEM_MEMORY_DIC, 
                                          SM_MAKE_PID(aHeader->mSelfOID) );
}
 
