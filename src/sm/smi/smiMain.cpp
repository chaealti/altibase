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
 * $Id: smiMain.cpp 90259 2021-03-19 01:22:22Z emlee $
 **********************************************************************/

#include <idl.h>
#include <ideErrorMgr.h>
#include <iduFixedTable.h>
#include <iduMemPoolMgr.h>
#include <smErrorCode.h>
#include <smm.h>
#include <svm.h>
#include <sdd.h>
#include <sdb.h>
#include <smr.h>
#include <sdp.h>
#include <smc.h>
#include <sdc.h>
#include <smn.h>
#include <sml.h>
#include <sm2x.h>
#include <sma.h>
#include <smi.h>
#include <smu.h>
#include <sct.h>
#include <scp.h>
#include <sds.h>
#include <smx.h>
#ifdef ALTIBASE_ENABLE_SMARTSSD
#include <sdm_mgnt_public.h>
#endif

/* SM���� ����� �ݹ��Լ��� */
smiGlobalCallBackList gSmiGlobalCallBackList;

/* The NULL GRID */
scGRID gScNullGRID = { SC_NULL_SPACEID, SC_NULL_OFFSET, SC_NULL_PID };

static IDE_RC smiCreateMemoryTableSpaces( SChar         * aDBName,
                                          UInt            aCreatePageCount,
                                          SChar         * aDBCharSet,
                                          SChar         * aNationalCharSet);

static IDE_RC smiCreateDiskTableSpaces();

static ULong gDBFileMaxPageCntOfDRDB;

/*
 * TASK-6198 Samsung Smart SSD GC control
 */
#ifdef ALTIBASE_ENABLE_SMARTSSD
sdm_handle_t * gLogSDMHandle;
#endif

/********************************************************************
 * Description : MEM_MAX_DB_SIZE�� EXPAND_CHUNK_PAGE_COUNT���� ū��
 *               �˻��Ѵ�.
 ********************************************************************/
IDE_RC smiCheckMemMaxDBSize()
{
    IDE_TEST( smuProperty::getExpandChunkPageCount() * SM_PAGE_SIZE >
              smuProperty::getMaxDBSize() )

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    IDE_SET( ideSetErrorCode( smERR_FATAL_INVALID_MEM_MAX_DB_SIZE,
                              smuProperty::getMaxDBSize(),
                              smuProperty::getExpandChunkPageCount() ) );

    return IDE_FAILURE;
}

/* ����ڰ� ������ �����ͺ��̽� ũ�⸦ ����
 * ������ ������ �����ͺ��̽� ũ�⸦ ����Ѵ�.
 *
 * �ϳ��� �����ͺ��̽� ������ �������� Expand Chunk�� �����Ǳ� ������,
 * ����ڰ� ������ �����ͺ��̽� ũ��� ��Ȯ�� ��ġ���� �ʴ�
 * ũ��� �����ͺ��̽��� ������ �� �ֱ� ������ �� �Լ��� �ʿ��ϴ�.
 *
 * aUserDbCreatePageCount [IN] ����ڰ� ������ �ʱ� ������ ���̽��� Page��
 * aDbCreatePageCount     [OUT] �ý����� ����� �ʱ� ������ ���̽��� Page��
 */
IDE_RC smiCalculateDBSize( UInt   aUserDbCreatePageCount,
                           UInt * aDbCreatePageCount )
{
    UInt sChunkPageCount = smuProperty::getExpandChunkPageCount() ;

#ifdef DEBUG    
    ULong  sCalculateDbPageCount;
#endif

    IDE_ASSERT( sChunkPageCount > 0 );

#ifdef DEBUG    
    sCalculateDbPageCount = smmManager::calculateDbPageCount( 
                               aUserDbCreatePageCount * (ULong)SM_PAGE_SIZE,
                               sChunkPageCount);
    IDE_DASSERT( (ULong)SC_MAX_PAGE_COUNT >= sCalculateDbPageCount );  
#endif

    // BUG-15288
    // create�� page count�� chunk page count�� align���� �ʰ�
    // smmManager�� ���ؼ� ���Ѵ�.
    *aDbCreatePageCount = smmManager::calculateDbPageCount(
                              aUserDbCreatePageCount * (ULong)SM_PAGE_SIZE,
                              sChunkPageCount );

    return IDE_SUCCESS ;
}


/* �ϳ��� �����ͺ��̽� ������ ���ϴ� Page�� ���� �����Ѵ�
 *
 * aDBFilePageCount [IN] �ϳ��� �����ͺ��̽� ������ ���ϴ� Page�� ��
 */
IDE_RC smiGetDBFilePageCount( scSpaceID aSpaceID, UInt * aDBFilePageCount)
{
    smmTBSNode * sTBSNode;
    UInt         sDBFilePageCount;
    UInt         sChunkPageCount ;

    IDE_DASSERT( aDBFilePageCount != NULL );

    IDE_TEST( sctTableSpaceMgr::findSpaceNodeBySpaceID(aSpaceID,
                                                       (void**)&sTBSNode)
              != IDE_SUCCESS );
    IDE_ASSERT( sTBSNode != NULL);
    IDE_DASSERT( sctTableSpaceMgr::isMemTableSpace(aSpaceID) == ID_TRUE );

    IDE_TEST( smmManager::readMemBaseInfo( sTBSNode,
                                           & sDBFilePageCount,
                                           & sChunkPageCount )
              != IDE_SUCCESS );

    * aDBFilePageCount = sDBFilePageCount ;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}



/* �����ͺ��̽��� ������ �� �ִ� �ִ� Page���� ���
 *
 */
UInt smiGetMaxDBPageCount()
{
    UInt   sChunkPageCount;
    ULong  sMaxDbSize;

#ifdef DEBUG    
    ULong  sCalculateDbPageCount;
#endif

    sMaxDbSize      = smuProperty::getMaxDBSize();
    sChunkPageCount = smuProperty::getExpandChunkPageCount();

    IDE_ASSERT( sChunkPageCount > 0 );
#ifdef DEBUG    
    sCalculateDbPageCount = smmManager::calculateDbPageCount( sMaxDbSize,
                                                              sChunkPageCount);
    IDE_DASSERT( (ULong)SC_MAX_PAGE_COUNT >= sCalculateDbPageCount );  
#endif

    return smmManager::calculateDbPageCount( sMaxDbSize,
                                             sChunkPageCount);
}

/* �����ͺ��̽��� ������ �� �ִ� �ּ� Page ���� ���
 * �ּ��� expand_chunk_page_count���� Ŀ�� �Ѵ�.
 */
UInt smiGetMinDBPageCount()
{
    return smuProperty::getExpandChunkPageCount();
}

/*
 * �����ͺ��̽��� �����Ѵ�.
 * createdb ���� �θ���.
 *
 * aDBName          [IN] �����ͺ��̽� �̸�
 * aCreatePageCount [IN] ������ �����ͺ��̽��� ���� Page�� ��
 *                       Membase�� ��ϵǴ� Meta Page(0�� Page)�� ����
 *                       ���Ե��� �ʴ´�.
 * aDBCharSet       [IN] �����ͺ��̽� ĳ���� ��
 * aNationalCharSet [IN] ���ų� ĳ���� ��
 * aArchiveLog      [IN] ��ī�̺� �α� ���
 */
IDE_RC smiCreateDB( SChar         * aDBName,
                    UInt            aCreatePageCount,
                    SChar         * aDBCharSet,
                    SChar         * aNationalCharSet,
                    smiArchiveMode  aArchiveLog )
{
    /* -------------------------
     * [2] create Memory Mgr & init
     * ------------------------*/
    IDE_CALLBACK_SEND_SYM("\tCreating MMDB FILES     ");

    IDE_TEST( smiCreateMemoryTableSpaces( aDBName,
                                          aCreatePageCount,
                                          aDBCharSet,
                                          aNationalCharSet )
              != IDE_SUCCESS );

    IDE_CALLBACK_SEND_MSG("[SUCCESS]\n");

    /* -------------------------
     * [3] create Catalog Table & Index
     * ------------------------*/
    IDE_CALLBACK_SEND_SYM("\tCreating Catalog Tables ");

    IDE_TEST( smcCatalogTable::createCatalogTable()
              != IDE_SUCCESS );

    IDE_CALLBACK_SEND_MSG("[SUCCESS]\n");

    /* FOR A4 : DRDB�� ���� DB Create �۾� ���� */

    IDE_CALLBACK_SEND_SYM("\tCreating DRDB FILES     ");

    IDE_TEST( smiCreateDiskTableSpaces() != IDE_SUCCESS );

    IDE_CALLBACK_SEND_MSG("[SUCCESS]\n");

    /* create database dbname ..[archivelog|noarchivelog] */
    IDE_TEST( smiBackup::alterArchiveMode( aArchiveLog,
                                           ID_FALSE )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*
   Createdb�ÿ� Memory Tablespace���� �����Ѵ�.

   aDBName          [IN] �����ͺ��̽� �̸�
   aCreatePageCount [IN] ������ �����ͺ��̽��� ���� Page�� ��
                         Membase�� ��ϵǴ� Meta Page(0�� Page)�� ����
                         ���Ե��� �ʴ´�.
 */
static IDE_RC smiCreateMemoryTableSpaces(
                       SChar         * aDBName,
                       UInt        aCreatePageCount,
                       SChar         * aDBCharSet,
                       SChar         * aNationalCharSet)
{
    UInt               sState = 0;
    smxTrans *         sTrans = NULL;
    SChar              sOutputMsg[256];
    // �ý����� ���� �� �ִ� �ִ� Page ����
    UInt               sHighLimitPageCnt;
    UInt               sTotalPageCount;
    UInt               sSysDicPageCount;
    UInt               sSysDataPageCount;

    // SYSTEM DICTIONARY TABLESPACE�� �ʱ� ũ��
    // -> Tablespace�� �ּ� ũ���� EXPAND_CHUNK_PAGE_COUNT�� ����
    sSysDicPageCount  = smuProperty::getExpandChunkPageCount();

    // SYSTEM DATA TABLESPACE�� �ʱ� ũ��
    // -> ����ڰ� create database������ ������ �ʱ�ũ����,
    //    aCreatePageCount�� ����
    sSysDataPageCount = aCreatePageCount;

    // ����üũ �ǽ�
    {
        // MEM_MAX_DB_SIZE�� �ɸ��� �ʴ��� �˻�
        sHighLimitPageCnt = smiGetMaxDBPageCount();

        sTotalPageCount = sSysDicPageCount + sSysDataPageCount;

        // ����ڰ� �ʱ� ũ��� ������ Page�� 0���̸� ����
        IDE_TEST_RAISE( aCreatePageCount <= 0, page_range_error);

        // ����ڰ� ������ Page�� + SYSTEM DICTIONARY Tablespaceũ�Ⱑ
        // MEM_MAX_DB_SIZE �� �Ѿ�� ����
        IDE_TEST_RAISE( sTotalPageCount > sHighLimitPageCnt,
                        page_range_error );

        // BUG-29607 Create DB���� Memory Tablespace�� �����ϱ� ��
        //           ���� �̸��� File�� �̹� �����ϴ��� Ȯ���Ѵ�.
        //        Create Tablespace������ �˻� ������
        //        ��ȯ�ϴ� ������ ������ �ٸ���.
        IDE_TEST_RAISE( smmDatabaseFile::chkExistDBFileByProp(
                            SMI_TABLESPACE_NAME_SYSTEM_MEMORY_DIC ) != IDE_SUCCESS,
            error_already_exist_datafile );

        IDE_TEST_RAISE( smmDatabaseFile::chkExistDBFileByProp(
                            SMI_TABLESPACE_NAME_SYSTEM_MEMORY_DATA ) != IDE_SUCCESS,
            error_already_exist_datafile );
    }


    IDE_TEST( smxTransMgr::alloc( &sTrans ) != IDE_SUCCESS );
    sState = 1;


    IDE_ASSERT( sTrans->begin( NULL,
                               ( SMI_TRANSACTION_REPL_DEFAULT |
                                 SMI_COMMIT_WRITE_NOWAIT ),
                               SMX_NOT_REPL_TX_ID )
                == IDE_SUCCESS );
    sState = 2;

    IDE_TEST( smmTBSCreate::createTBS(sTrans,
                                       aDBName,
                                       (SChar*)SMI_TABLESPACE_NAME_SYSTEM_MEMORY_DIC,
                                       SMI_TABLESPACE_ATTRFLAG_SYSTEM_MEMORY_DIC,
                                       SMI_MEMORY_SYSTEM_DICTIONARY,
                                       NULL,   /* Use Default Checkpoint Path*/
                                       0,      /* Use Default Split Size */
                                       // Init Size
                                       sSysDicPageCount * SM_PAGE_SIZE,
                                       ID_TRUE, /* Auto Extend On */
                                       ID_ULONG_MAX,/* Use Default Next Size */
                                       ID_ULONG_MAX,/* Use Default Max Size */
                                       ID_TRUE, /* Online */
                                       aDBCharSet, /* PROJ-1579 NCHAR */
                                       aNationalCharSet, /* PROJ-1579 NCHAR */
                                       NULL    /* No Need to get TBSID */)
              != IDE_SUCCESS );

    IDE_TEST( sTrans->commit() != IDE_SUCCESS );
    sState = 1;


    IDE_ASSERT( sTrans->begin( NULL,
                               ( SMI_TRANSACTION_REPL_DEFAULT |
                                 SMI_COMMIT_WRITE_NOWAIT ),
                               SMX_NOT_REPL_TX_ID )
                == IDE_SUCCESS );
    sState = 2;
    IDE_TEST( smmTBSCreate::createTBS(
                            sTrans,
                            aDBName,
                            (SChar*)SMI_TABLESPACE_NAME_SYSTEM_MEMORY_DATA,
                            SMI_TABLESPACE_ATTRFLAG_SYSTEM_MEMORY_DATA,
                            SMI_MEMORY_SYSTEM_DATA,
                            NULL,    /* Use Default Checkpoint Path*/
                            0,       /* Use Default Split Size */
                            // Init Size
                            sSysDataPageCount * SM_PAGE_SIZE,
                            ID_TRUE, /* Auto Extend On */
                            ID_ULONG_MAX, /* Use Default Next Size */
                            ID_ULONG_MAX, /* Use Default Max Size */
                            ID_TRUE, /* Online */
                            aDBCharSet, /* PROJ-1579 NCHAR */
                            aNationalCharSet, /* PROJ-1579 NCHAR */
                            NULL     /* No Need to get TBSID */  )
              != IDE_SUCCESS );

    IDE_TEST( sTrans->commit() != IDE_SUCCESS );
    sState = 1;

    IDE_TEST( smxTransMgr::freeTrans( sTrans ) != IDE_SUCCESS );
    sState = 0;

    return IDE_SUCCESS;

    IDE_EXCEPTION(page_range_error);
    {
        idlOS::snprintf(sOutputMsg, ID_SIZEOF(sOutputMsg), "ERROR: \n"
                        "Specify Valid Page Range (1 ~ %"ID_UINT64_FMT") \n",
                        (ULong) sHighLimitPageCnt);
        ideLog::log(IDE_SERVER_0,"%s\n",sOutputMsg);

        IDE_SET(ideSetErrorCode(smERR_ABORT_PAGE_RANGE_ERROR, sHighLimitPageCnt));
    }
    IDE_EXCEPTION( error_already_exist_datafile );
    {
        IDE_SET( ideSetErrorCode( smERR_ABORT_AlreadyExistDBFiles ));
    }
    IDE_EXCEPTION_END;

    IDE_PUSH();

    switch ( sState )
    {
        case 2:
            sTrans->abort( ID_FALSE, /* aIsLegacyTrans */
                           NULL      /* aLegacyTrans */ );
        case 1:
            smxTransMgr::freeTrans( sTrans );
            break;

        default :
            break;
    }

    IDE_POP();

    return IDE_FAILURE;
}


static IDE_RC smiCreateDiskTableSpaces( )
{
    smiTableSpaceAttr   sTbsAttr;
    smiDataFileAttr     sDFAttr;
    smiDataFileAttr     sSysDFAttr;
    smiDataFileAttr *   sDFAttrPtr;
    SChar               sDir[SMI_MAX_DATAFILE_NAME_LEN];
    SChar *             sDbDir;
    smxTrans *          sTx = NULL;
    ULong               sExtentSize;
    ULong               sInitSize;
    ULong               sMaxSize;
    ULong               sNextSize;
    UInt                sEntryCnt;
    UInt                sAdjustEntryCnt;

    sDbDir = (SChar *)smuProperty::getDefaultDiskDBDir();
    idlOS::strcpy(sDir, sDbDir);

    /* 1. system tablespace
       2. undo  tablespace
       3. temp  tablespace
       ������ �����Ѵ�.

       // system tablespace�� ����.
      sdpTableSpace::createSystemTBS()�� ȣ��
       //  undo tablespace ����.
      sdpTableSpace::createUndoTBS();
       //  temp tablespace ����.
       //->temp  ���̺� �����̽� �̸�,
       sdpTableSpace::createTempTBS(.....)
     */

    // ===== create system Table Space =====
    // BUG-27911
    // smuProperty �� �Լ��� ���� ȣ������ �ʰ�
    // smiTableSpace �� �������̽� �Լ��� ȣ���ϴ� ������� �����մϴ�.
    sExtentSize = smiTableSpace::getSysDataTBSExtentSize();
    sInitSize   = smiTableSpace::getSysDataFileInitSize();
    sMaxSize    = smiTableSpace::getSysDataFileMaxSize();
    sNextSize   = smiTableSpace::getSysDataFileNextSize();

    if( sInitSize > sMaxSize )
    {
        sInitSize = sMaxSize;
    }

    idlOS::memset(&sTbsAttr, 0x00, ID_SIZEOF(smiTableSpaceAttr));
    idlOS::memset(&sSysDFAttr, 0x00, ID_SIZEOF(smiDataFileAttr));

    // PRJ-1548 User Memory Tablespace
    // DISK SYSTEM TBS�� TBS Node Attribute ����

    sTbsAttr.mAttrType = SMI_TBS_ATTR;
    sTbsAttr.mAttrFlag = SMI_TABLESPACE_ATTRFLAG_SYSTEM_DISK_DATA;
    sTbsAttr.mID = 0;
    idlOS::snprintf(sTbsAttr.mName, SMI_MAX_TABLESPACE_NAME_LEN,
                    "%s", SMI_TABLESPACE_NAME_SYSTEM_DISK_DATA);
    sTbsAttr.mNameLength   = idlOS::strlen(sTbsAttr.mName);
    sTbsAttr.mType         = SMI_DISK_SYSTEM_DATA;
    sTbsAttr.mTBSStateOnLA = SMI_TBS_ONLINE;

    idlOS::snprintf(sSysDFAttr.mName, SMI_MAX_DATAFILE_NAME_LEN,
                    "%s%csystem001.dbf",
                    sDir,
                    IDL_FILE_SEPARATOR);

    // BUG-29607 Create DB���� Disk Tablespace�� �����ϱ� ��
    //           ���� �̸��� File�� �̹� �����ϴ��� Ȯ���Ѵ�.
    //        Create Tablespace������ �˻� ������
    //        ��ȯ�ϴ� ������ ������ �ٸ���.
    IDE_TEST_RAISE( idf::access( sSysDFAttr.mName, F_OK) == 0,
                    error_already_exist_datafile );

    // PRJ-1548 User Memory Tablespace
    // DISK SYSTEM TBS�� DBF Node Attribute ����
    sSysDFAttr.mAttrType     = SMI_DBF_ATTR;
    sSysDFAttr.mNameLength   = idlOS::strlen(sSysDFAttr.mName);
    sSysDFAttr.mIsAutoExtend = ID_TRUE;
    sSysDFAttr.mState        = SMI_FILE_ONLINE;

    // BUG-27911
    // alignByPageSize() �� size �� ���� �߸��� ����� �ϰ� �־����ϴ�.
    // smiTableSpace �� �������̽� �Լ��� ���� ���°���
    // valide �ϱ� ������ �� ���� �ٷ� ����մϴ�.
    sSysDFAttr.mMaxSize      = sMaxSize  / SD_PAGE_SIZE;
    sSysDFAttr.mNextSize     = sNextSize / SD_PAGE_SIZE;
    sSysDFAttr.mCurrSize     = sInitSize / SD_PAGE_SIZE;
    sSysDFAttr.mInitSize     = sInitSize / SD_PAGE_SIZE;
    // BUG-29607 ���� ���ϸ��� ������ �������� ���� ���� ��ȯ�ϵ��� ����
    sSysDFAttr.mCreateMode   = SMI_DATAFILE_CREATE;

    IDE_TEST( smxTransMgr::alloc( &sTx ) != IDE_SUCCESS );

    IDE_ASSERT( sTx->begin( NULL,
                            ( SMI_TRANSACTION_REPL_NONE |
                              SMI_COMMIT_WRITE_NOWAIT ),
                            SMX_NOT_REPL_TX_ID )
                == IDE_SUCCESS );

    /* PROJ-1671 Bitmap-base Tablespace And Segment Space Management
     * Create Database ���������� �⺻ �ý��� ������Ƽ���� �ǵ��Ѵ� */
    sTbsAttr.mDiskAttr.mSegMgmtType  =
             (smiSegMgmtType)smuProperty::getDefaultSegMgmtType();
    sTbsAttr.mDiskAttr.mExtMgmtType  =  SMI_EXTENT_MGMT_BITMAP_TYPE;
    sTbsAttr.mDiskAttr.mExtPageCount = (UInt)(sExtentSize / SD_PAGE_SIZE);

    sDFAttrPtr = &sSysDFAttr;

    IDE_TEST( sdpTableSpace::createTBS( NULL,// BUGBUG
                                        &sTbsAttr,
                                        &sDFAttrPtr,
                                        1,
                                        sTx)
              != IDE_SUCCESS );

    IDE_TEST( sTx->commit() != IDE_SUCCESS );

    // ===== create undo Table Space =====
    // BUG-27911
    // smuProperty �� �Լ��� ���� ȣ������ �ʰ�
    // smiTableSpace �� �������̽� �Լ��� ȣ���ϴ� ������� �����մϴ�.
    sExtentSize = smiTableSpace::getSysUndoTBSExtentSize();
    sInitSize   = smiTableSpace::getSysUndoFileInitSize();
    sMaxSize    = smiTableSpace::getSysUndoFileMaxSize();
    sNextSize   = smiTableSpace::getSysUndoFileNextSize();

    if( sInitSize > sMaxSize )
    {
        sInitSize = sMaxSize;
    }

    idlOS::memset(&sTbsAttr, 0x00, ID_SIZEOF(smiTableSpaceAttr));
    idlOS::memset(&sDFAttr, 0x00, ID_SIZEOF(smiDataFileAttr));

    sTbsAttr.mID = 0;

    // PRJ-1548 User Memory Tablespace
    // UNDO TBS�� TBS Node Attribute ����
    sTbsAttr.mAttrType = SMI_TBS_ATTR;
    sTbsAttr.mAttrFlag = SMI_TABLESPACE_ATTRFLAG_SYSTEM_DISK_UNDO;

    idlOS::snprintf(sTbsAttr.mName,
                    SMI_MAX_TABLESPACE_NAME_LEN,
                    SMI_TABLESPACE_NAME_SYSTEM_DISK_UNDO);

    sTbsAttr.mNameLength =
        idlOS::strlen(sTbsAttr.mName);
    sTbsAttr.mType  = SMI_DISK_SYSTEM_UNDO;
    sTbsAttr.mTBSStateOnLA = SMI_TBS_ONLINE;

    // PRJ-1548 User Memory Tablespace
    // UNDO TBS�� DBF Node Attribute ����

    sDFAttr.mAttrType  = SMI_DBF_ATTR;

    idlOS::snprintf(sDFAttr.mName, SMI_MAX_DATAFILE_NAME_LEN,
                   "%s%cundo001.dbf",
                   sDir,
                   IDL_FILE_SEPARATOR);

    // BUG-29607 Create DB���� Disk Tablespace�� �����ϱ� ��
    //           ���� �̸��� File�� �̹� �����ϴ��� Ȯ���Ѵ�.
    //        Create Tablespace������ �˻� ������
    //        ��ȯ�ϴ� ������ ������ �ٸ���.
    IDE_TEST_RAISE( idf::access( sDFAttr.mName, F_OK) == 0,
                    error_already_exist_datafile );

    sDFAttr.mNameLength   = idlOS::strlen(sDFAttr.mName);
    sDFAttr.mIsAutoExtend = ID_TRUE;
    sDFAttr.mState        = SMI_FILE_ONLINE;

    // BUG-27911
    // alignByPageSize() �� size �� ���� �߸��� ����� �ϰ� �־����ϴ�.
    // smiTableSpace �� �������̽� �Լ��� ���� ���°���
    // valide �ϱ� ������ �� ���� �ٷ� ����մϴ�.
    sSysDFAttr.mMaxSize   = sMaxSize  / SD_PAGE_SIZE;
    sDFAttr.mMaxSize      = sMaxSize  / SD_PAGE_SIZE;
    sDFAttr.mNextSize     = sNextSize / SD_PAGE_SIZE;
    sDFAttr.mCurrSize     = sInitSize / SD_PAGE_SIZE;
    sDFAttr.mInitSize     = sInitSize / SD_PAGE_SIZE;
    // BUG-29607 ���� ���ϸ��� ������ �������� ���� ���� ��ȯ�ϵ��� ����
    sDFAttr.mCreateMode   = SMI_DATAFILE_CREATE;

    sDFAttrPtr = &sDFAttr;

    IDE_ASSERT( sTx->begin( NULL,
                            ( SMI_TRANSACTION_REPL_NONE |
                              SMI_COMMIT_WRITE_NOWAIT ),
                            SMX_NOT_REPL_TX_ID )
                == IDE_SUCCESS );

    sTbsAttr.mDiskAttr.mSegMgmtType  = SMI_SEGMENT_MGMT_CIRCULARLIST_TYPE;
    sTbsAttr.mDiskAttr.mExtMgmtType  = SMI_EXTENT_MGMT_BITMAP_TYPE;
    sTbsAttr.mDiskAttr.mExtPageCount = (UInt)(sExtentSize / SD_PAGE_SIZE);
    IDE_TEST( sdpTableSpace::createTBS( NULL,// BUGBUG
                                        &sTbsAttr,
                                        &sDFAttrPtr,
                                        1,
                                        sTx)
              != IDE_SUCCESS );

    /* To Fix BUG-24090 createdb�� undo001.dbf ũ�Ⱑ �̻��մϴ�.
     * Ʈ����� Commit Pending���� Add DataFile ������ ����Ǿ�
     * SpaceCache�� FreenessOfGGs�� setBit�ϰԵǾ� �ִµ�
     * Segment ������ �Բ� �ϳ��� Ʈ��������� ó���Ǿ� Segment
     * �����ÿ��� ��ȿ���� ���� Freeness�� ���� ���� Ȯ���� �߻��Ͽ�
     * ũ�Ⱑ �����Ͽ���. */
    IDE_TEST( sTx->commit() != IDE_SUCCESS );

    IDE_ASSERT( sTx->begin( NULL,
                            ( SMI_TRANSACTION_REPL_NONE |
                              SMI_COMMIT_WRITE_NOWAIT ),
                            SMX_NOT_REPL_TX_ID )
                == IDE_SUCCESS );

    /***********************************************************************
     * PROJ-1704 DISK MVCC ������
     * Ʈ����� ���׸�Ʈ�� TSS Segment�� Undo Segment�� ��� ���´�
     * ����̴�. Create Database �������� Undo Tablespace�� ������ �Ŀ�
     * ����� ������Ƽ TRANSACTION SEGMENT�� ��õ� ������ �����Ͽ�
     * TSS Segment �� Undo Segment�� �����ϰ�, �̸� �����ϴ� Transaction
     * Segment Manager�� �ʱ�ȭ �Ѵ�.
     ***********************************************************************/
    sEntryCnt = smuProperty::getTXSEGEntryCnt();

    IDE_TEST( sdcTXSegMgr::adjustEntryCount( sEntryCnt,
                                             &sAdjustEntryCnt )
              != IDE_SUCCESS );

    IDE_TEST( sdcTXSegMgr::createSegs( NULL /* idvSQL */, sTx )
              != IDE_SUCCESS );

    IDE_TEST( sdcTXSegMgr::initialize( ID_TRUE /* aIsAttachSegment*/ )
              != IDE_SUCCESS );

    IDE_TEST( sTx->commit() != IDE_SUCCESS );

    // ===== create temp Table Space =====
    // BUG-27911
    // smuProperty �� �Լ��� ���� ȣ������ �ʰ�
    // smiTableSpace �� �������̽� �Լ��� ȣ���ϴ� ������� �����մϴ�.    
    sExtentSize = smiTableSpace::getSysTempTBSExtentSize();
    sInitSize   = smiTableSpace::getSysTempFileInitSize();
    sMaxSize    = smiTableSpace::getSysTempFileMaxSize();
    sNextSize   = smiTableSpace::getSysTempFileNextSize();

    if( sInitSize > sMaxSize )
    {
        sInitSize = sMaxSize;
    }

    idlOS::memset(&sTbsAttr, 0x00, ID_SIZEOF(smiTableSpaceAttr));
    idlOS::memset(&sDFAttr, 0x00, ID_SIZEOF(smiDataFileAttr));

    sTbsAttr.mID = 0;

    // PRJ-1548 User Memory Tablespace
    // DISK TEMP TBS�� TBS Node Attribute ����
    sTbsAttr.mAttrType = SMI_TBS_ATTR;
    sTbsAttr.mAttrFlag = SMI_TABLESPACE_ATTRFLAG_SYSTEM_DISK_TEMP;

    idlOS::snprintf(sTbsAttr.mName,
                    SMI_MAX_TABLESPACE_NAME_LEN,
                    SMI_TABLESPACE_NAME_SYSTEM_DISK_TEMP);

    sTbsAttr.mNameLength = idlOS::strlen(sTbsAttr.mName);

    sTbsAttr.mType  = SMI_DISK_SYSTEM_TEMP;
    sTbsAttr.mTBSStateOnLA = SMI_TBS_ONLINE;

    // PRJ-1548 User Memory Tablespace
    // DISK TEMP TBS�� DBF Node Attribute ����
    sDFAttr.mAttrType  = SMI_DBF_ATTR;

    idlOS::snprintf(sDFAttr.mName,
                    SMI_MAX_DATAFILE_NAME_LEN,
                    "%s%ctemp001.dbf",
                    sDir,
                    IDL_FILE_SEPARATOR);

    // BUG-29607 Create DB���� Disk Tablespace�� �����ϱ� ��
    //           ���� �̸��� File�� �̹� �����ϴ��� Ȯ���Ѵ�.
    //        Create Tablespace������ �˻� ������
    //        ��ȯ�ϴ� ������ ������ �ٸ���.
    IDE_TEST_RAISE( idf::access( sDFAttr.mName, F_OK) == 0,
                    error_already_exist_datafile );

    sDFAttr.mNameLength   = idlOS::strlen(sDFAttr.mName);
    sDFAttr.mIsAutoExtend = ID_TRUE;
    sDFAttr.mState        = SMI_FILE_ONLINE;

    // BUG-27911
    // alignByPageSize() �� size �� ���� �߸��� ����� �ϰ� �־����ϴ�.
    // smiTableSpace �� �������̽� �Լ��� ���� ���°���
    // valide �ϱ� ������ �� ���� �ٷ� ����մϴ�.
    sDFAttr.mMaxSize      = sMaxSize  / SD_PAGE_SIZE;
    sDFAttr.mNextSize     = sNextSize / SD_PAGE_SIZE;
    sDFAttr.mCurrSize     = sInitSize / SD_PAGE_SIZE;
    sDFAttr.mInitSize     = sInitSize / SD_PAGE_SIZE;
    // BUG-29607 ���� ���ϸ��� ������ �������� ���� ���� ��ȯ�ϵ��� ����
    sDFAttr.mCreateMode   = SMI_DATAFILE_CREATE;

    sDFAttrPtr = &sDFAttr;

    IDE_ASSERT( sTx->begin( NULL,
                            ( SMI_TRANSACTION_REPL_NONE |
                              SMI_COMMIT_WRITE_NOWAIT ),
                            SMX_NOT_REPL_TX_ID )
                == IDE_SUCCESS );

    sTbsAttr.mDiskAttr.mSegMgmtType  = SMI_SEGMENT_MGMT_FREELIST_TYPE;
    sTbsAttr.mDiskAttr.mExtMgmtType  = SMI_EXTENT_MGMT_BITMAP_TYPE;
    sTbsAttr.mDiskAttr.mExtPageCount = (UInt)(sExtentSize / SD_PAGE_SIZE);

    IDE_TEST( sdpTableSpace::createTBS( NULL,// BUGBUG
                                        &sTbsAttr,
                                        &sDFAttrPtr,
                                        1,
                                        sTx )
              != IDE_SUCCESS );

    IDE_TEST( sTx->commit() != IDE_SUCCESS );

    IDE_TEST( smxTransMgr::freeTrans( sTx ) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION( error_already_exist_datafile );
    {
        IDE_SET( ideSetErrorCode( smERR_ABORT_AlreadyExistDBFiles ));
    }
    IDE_EXCEPTION_END;
    if( sTx != NULL )
    {
        sTx->commit();
        smxTransMgr::freeTrans( sTx );
    }

    return IDE_FAILURE;
}

// ======================= for supporting Multi Phase Startup =================
// BUGBUG : ���� ����
static smiStartupPhase gStartupPhase = SMI_STARTUP_INIT;

/*
 * ���� Guide:
 * QP/MM���� ���� ������ aPhase�� �Ѿ�� �� ����.
 * �װͿ� ���� ���� �ڵ带 �÷��� ��(Not Killed!!)
 *
 * Phase�� ������ �� ������ Phase�� ���� ��õ� Phase���� �ܰ踦 ���
 * �ʱ�ȭ�� �����ؾ� ��.
 */

/*
 * ������ Phase�� ���� INIT ������ �ܰ������� Shutdown�� �����ؾ� ��.
 */


/**************************************************/
/* TSM���� CreateDB �ÿ� ���Ǵ� �ʱ�ȭ Callback */
/**************************************************/
IDE_RC smiCreateDBCoreInit(smiGlobalCallBackList *   /*aCallBack*/)
{
    struct rlimit sLimit;

    /* -------------------------
     * [0] Get Properties
     * ------------------------*/
    /* BUG-22201: Disk DataFile�� �ִ�ũ�Ⱑ 32G�� �Ѿ�� ���ϻ����� �����ϰ�
     * �ֽ��ϴ�.
     *
     * �ִ�ũ�Ⱑ 32G�� ���� �ʵ��� ������. ( SD_MAX_FPID_COUNT: 1<<22, 2^21 )
     * */
    IDE_TEST(idlOS::getrlimit( RLIMIT_FSIZE, &sLimit) != 0 );
    gDBFileMaxPageCntOfDRDB = sLimit.rlim_cur / SD_PAGE_SIZE;

    gDBFileMaxPageCntOfDRDB = gDBFileMaxPageCntOfDRDB > SD_MAX_FPID_COUNT ? SD_MAX_FPID_COUNT : gDBFileMaxPageCntOfDRDB;

    ideLog::log(IDE_SERVER_0,"\n");

    /* 1.�޸� ���̺����̽� ������ */
    ideLog::log(IDE_SERVER_0," [SM-PREPARE] TableSpace Manager");
    IDE_TEST( sctTableSpaceMgr::initialize( ) != IDE_SUCCESS );
    
    /* 2.Dirty Page ������ */
    ideLog::log(IDE_SERVER_0," [SM-PREPARE] Dirty Page Manager");
    IDE_TEST( smmDirtyPageMgr::initializeStatic() != IDE_SUCCESS );

    /* 3.�޸� ������ */
    ideLog::log(IDE_SERVER_0," [SM-PREPARE] Memory Manager");
    IDE_TEST( smmManager::initializeStatic( ) != IDE_SUCCESS );

    /* 4.���̺����̽� ���� */
    ideLog::log(IDE_SERVER_0," [SM-PREPARE] Memory Tablespace");
    IDE_TEST( smmTBSStartupShutdown::initializeStatic( ) != IDE_SUCCESS );

    /* 5.��ũ ������ */
    ideLog::log(IDE_SERVER_0," [SM-PREPARE] Disk Manager");
    IDE_TEST( sddDiskMgr::initialize( (UInt)gDBFileMaxPageCntOfDRDB )
              != IDE_SUCCESS );

    /* 6.���� ������ */
    ideLog::log(IDE_SERVER_0," [SM-PREPARE] Buffer Manager");
    IDE_TEST( sdbBufferMgr::initialize() != IDE_SUCCESS );

    /* 0�� ���� ���� : ���� �����ں��� ���� ������ �Ǿ� �־�� ��. */
    IDE_TEST( smrRecoveryMgr::create() != IDE_SUCCESS );

    /* 7.���� ������ */
    ideLog::log(IDE_SERVER_0," [SM-PREPARE] Recovery Manager");
    IDE_TEST( smrRecoveryMgr::initialize() != IDE_SUCCESS );

    /* 8.�α� ������ */
    ideLog::log(IDE_SERVER_0," [SM-PREPARE] Log File");
    IDE_TEST( smrLogMgr::initialize() != IDE_SUCCESS );

    /* 9.��ũ ���̺����̽� ������ */
    ideLog::log(IDE_SERVER_0," [SM-PREPARE] Tablespace Manager");
    IDE_TEST( sdpTableSpace::initialize() != IDE_SUCCESS );

    /* 10.��� ������ */
    IDE_TEST( smrBackupMgr::initialize() != IDE_SUCCESS );

    /* prepare Thread ���� */ 
    IDE_TEST( smrLogMgr::startupLogPrepareThread() != IDE_SUCCESS );

    ideLog::log(IDE_SERVER_0," [SM-PREPARE] Buffer Flusher");
    IDE_TEST( sdbFlushMgr::initialize(smuProperty::getBufferFlusherCnt())
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/************************************************/
/* MM���� CreateDB�ÿ� ���Ǵ� �ʱ�ȭ Callback */
/************************************************/
IDE_RC smiCreateDBMetaInit(smiGlobalCallBackList*    /*aCallBack*/)
{
    /* -------------------------
     * [1] CheckPoint & GC���� ������ �ʱ�ȭ
     * ------------------------*/

    ideLog::log(IDE_SERVER_0," [SM-PREPARE] CheckPoint Manager");
    IDE_TEST( gSmrChkptThread.initialize() != IDE_SUCCESS );

    /* PROJ-2733
       Ager���� �ʱ�ȭ���� ���� smxMinSCNBuild::mAgingViewSCN�� �������� �ʵ���
       Ager �������� smxMinSCNBuild�� �ʱ�ȭ�Ѵ�. */
    ideLog::log(IDE_SERVER_0," [SM-PREPARE] Initialize Minimum SCN Builder ");
    IDE_TEST( smxTransMgr::initializeMinSCNBuilder() != IDE_SUCCESS );

    ideLog::log(IDE_SERVER_0," [SM-PREPARE] Memory Garbage Collector");
    IDE_TEST( smaLogicalAger::initializeStatic() != IDE_SUCCESS );

    ideLog::log(IDE_SERVER_0," [SM-PREPARE] Delete Manager");
    IDE_TEST( smaDeleteThread::initializeStatic() != IDE_SUCCESS );

    /* -------------------------
     * [2] Index Rebuilding
     * ------------------------*/

    ideLog::log(IDE_SERVER_0," [SM-PREPARE] Index Rebuilding");
    IDE_TEST( smnManager::rebuildIndexes() != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/***************************************************/
/* TSM���� CreateDB�ÿ� ���Ǵ� Shutdown Callback */
/***************************************************/
IDE_RC smiCreateDBCoreShutdown(smiGlobalCallBackList*    /*aCallBack*/)
{
    ideLog::log(IDE_SERVER_0," [SM-SHUTDOWN] Flush All DirtyPages And Checkpoint Database");
    IDE_TEST( smrRecoveryMgr::finalize() != IDE_SUCCESS );

    /* �α� ������ �۾� ����*/
    IDE_TEST( smrLogMgr::shutdown() != IDE_SUCCESS );

    ideLog::log(IDE_SERVER_0," [SM-SHUTDOWN] Flush Manager");
    IDE_TEST( sdbFlushMgr::destroy() != IDE_SUCCESS );

    /* 10.��� ������ */
    ideLog::log(IDE_SERVER_0," [SM-SHUTDOWN] Backup Manager");
    IDE_TEST( smrBackupMgr::destroy() != IDE_SUCCESS );

    /* 9.��ũ ���̺����̽� ������ */
    ideLog::log(IDE_SERVER_0," [SM-SHUTDOWN] Tablespace Manager");
    IDE_TEST( sdpTableSpace::destroy() != IDE_SUCCESS );

    /* 8.�α� ������ */
    ideLog::log(IDE_SERVER_0," [SM-SHUTDOWN] Log Manager");
    IDE_TEST( smrLogMgr::destroy() != IDE_SUCCESS );

    /* 7.���� ������ */
    ideLog::log(IDE_SERVER_0," [SM-SHUTDOWN] Recovery Manager");
    IDE_TEST( smrRecoveryMgr::destroy() != IDE_SUCCESS );

    /* 6.���� ������ */
    ideLog::log(IDE_SERVER_0," [SM-SHUTDOWN] Buffer Manager");
    IDE_TEST( sdbBufferMgr::destroy() != IDE_SUCCESS );

    /* 5.��ũ ������ */
    ideLog::log(IDE_SERVER_0," [SM-SHUTDOWN] Disk Manager");
    IDE_TEST( sddDiskMgr::destroy() != IDE_SUCCESS );

    /* 4.���̺����̽�  */
    ideLog::log(IDE_SERVER_0," [SM-SHUTDOWN] Memory Tablespace");
    IDE_TEST( smmTBSStartupShutdown::destroyStatic() != IDE_SUCCESS );

    /* 3.�޸� ������ */
    ideLog::log(IDE_SERVER_0," [SM-SHUTDOWN] Memory Manager");
    IDE_TEST( smmManager::destroyStatic() != IDE_SUCCESS );

    /* 2.Dirty Page ������ */
    ideLog::log(IDE_SERVER_0," [SM-SHUTDOWN] Dirty Page Manager");
    IDE_TEST( smmDirtyPageMgr::destroyStatic() != IDE_SUCCESS );

    /* 1.�޸� ���̺����̽� ������ */
    ideLog::log(IDE_SERVER_0," [SM-SHUTDOWN] TableSpace Manager");
    IDE_TEST( sctTableSpaceMgr::destroy() != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/***************************************************/
/* MM���� CreateDB�ÿ� ���Ǵ� Shutdown Callback  */
/***************************************************/
IDE_RC smiCreateDBMetaShutdown(smiGlobalCallBackList* /*aCallBack*/)
{
    // Start up�ÿ� �ݴ� ������ ����

    ideLog::log(IDE_SERVER_0," [SM-SHUTDOWN] Index Storage");
    IDE_TEST( smnManager::destroyIndexes() != IDE_SUCCESS );

    ideLog::log(IDE_SERVER_0," [SM-SHUTDOWN] Memory Garbage Collector Shutdown");
    IDE_TEST( smaLogicalAger::shutdownAll() != IDE_SUCCESS );

    //BUG-35886 server startup will fail in a test case
    //smaDeleteThread�� smaLogicalAger::destroyStatic() �������� shutdown
    //�Ǿ��Ѵ�.
    ideLog::log(IDE_SERVER_0," [SM-SHUTDOWN] Delete Manager Shutdown");
    IDE_TEST( smaDeleteThread::shutdownAll() != IDE_SUCCESS );

    ideLog::log(IDE_SERVER_0," [SM-SHUTDOWN] Memory Garbage Collector Destroy");
    IDE_TEST( smaLogicalAger::destroyStatic() != IDE_SUCCESS );

    ideLog::log(IDE_SERVER_0," [SM-SHUTDOWN] Delete Manager Destroy");
    IDE_TEST( smaDeleteThread::destroyStatic() != IDE_SUCCESS );

    /* PROJ-2733
       smxMinSCNBuild�� run���� �ʾ����Ƿ� destroy�� �����Ѵ�. */
    ideLog::log(IDE_SERVER_0," [SM-SHUTDOWN] Minimum SCN Builder");
    IDE_TEST( smxTransMgr::destroyMinSCNBuilder() != IDE_SUCCESS );

    ideLog::log(IDE_SERVER_0," [SM-SHUTDOWN] CheckPoint Manager");
    IDE_TEST( gSmrChkptThread.destroy() != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/****************************************************/
/* Utility ���α׷����� ���Ǵ� �ʱ�ȭ Callback  */
/* Utility ���α׷��� Process�ܰ踦 ��ġ�� ����.  */
/****************************************************/
IDE_RC smiSmUtilInit(smiGlobalCallBackList*    aCallBack)
{
    struct rlimit sLimit;
    UInt          sTransCnt = 0;

    /* BUG-22201: Disk DataFile�� �ִ�ũ�Ⱑ 32G�� �Ѿ�� ���ϻ����� �����ϰ�
     * �ֽ��ϴ�.
     *
     * �ִ�ũ�Ⱑ 32G�� ���� �ʵ��� ������. ( SD_MAX_FPID_COUNT: 2^22 )
     * */
    IDE_TEST(idlOS::getrlimit( RLIMIT_FSIZE, &sLimit) != 0 );
    gDBFileMaxPageCntOfDRDB = sLimit.rlim_cur / SD_PAGE_SIZE;

    gDBFileMaxPageCntOfDRDB = gDBFileMaxPageCntOfDRDB > SD_MAX_FPID_COUNT ? SD_MAX_FPID_COUNT : gDBFileMaxPageCntOfDRDB;

    /* -------------------------
     * [0] Init Idu module
     * ------------------------*/
    IDE_TEST( iduMemMgr::initializeStatic( IDU_CLIENT_TYPE ) != IDE_SUCCESS );
    IDE_TEST( iduMutexMgr::initializeStatic( IDU_CLIENT_TYPE ) != IDE_SUCCESS );
    iduLatch::initializeStatic( IDU_CLIENT_TYPE );
    IDE_TEST( iduCond::initializeStatic() != IDE_SUCCESS );

    /* -------------------------
     * [1] Property Loading
     * ------------------------*/
    IDE_TEST( idp::initialize(NULL, NULL) != IDE_SUCCESS );
    IDE_TEST( iduProperty::load() != IDE_SUCCESS );
    IDE_TEST( smuProperty::load() != IDE_SUCCESS );
    smuProperty::init4Util();

    /* -------------------------
     * [2] Message Loading
     * ------------------------*/
    IDE_TEST_RAISE( smuUtility::loadErrorMsb(idp::getHomeDir(),
                                             (SChar*)"KO16KSC5601")
                    != IDE_SUCCESS, load_error_msb_error );

    /* -------------------------
     * [3] Init CallBack
     * ------------------------*/
    if( aCallBack != NULL )
    {
        gSmiGlobalCallBackList = *aCallBack;
    }

    /* ---------------------------
     * [4] �⺻ SM Manager �ʱ�ȭ
     * --------------------------*/
    IDE_TEST( idvManager::initializeStatic() != IDE_SUCCESS );
    IDE_TEST( idvManager::startupService() != IDE_SUCCESS );

    IDE_TEST( smxTransMgr::calibrateTransCount(&sTransCnt)
              != IDE_SUCCESS );

    if( aCallBack != NULL )
    {
        IDE_TEST( smlLockMgr::initialize( sTransCnt,
                                          aCallBack->waitLockFunc,
                                          aCallBack->wakeupLockFunc )
                  != IDE_SUCCESS );
    }

    IDE_TEST( sctTableSpaceMgr::initialize() != IDE_SUCCESS );
    IDE_TEST( smmDirtyPageMgr::initializeStatic() != IDE_SUCCESS );
    IDE_TEST( smmManager::initializeStatic() != IDE_SUCCESS );
    IDE_TEST( sddDiskMgr::initialize( (UInt)gDBFileMaxPageCntOfDRDB ) 
              != IDE_SUCCESS );
    IDE_TEST( sdsBufferMgr::initialize() != IDE_SUCCESS );
    IDE_TEST( sdbBufferMgr::initialize() != IDE_SUCCESS );
    IDE_TEST( smmTBSStartupShutdown::initializeStatic() != IDE_SUCCESS );
    IDE_TEST( svmTBSStartupShutdown::initializeStatic() != IDE_SUCCESS );
    IDE_TEST( smriChangeTrackingMgr::initializeStatic() != IDE_SUCCESS );
    IDE_TEST( smriBackupInfoMgr::initializeStatic() != IDE_SUCCESS );
    IDE_TEST( smrRecoveryMgr::initialize() != IDE_SUCCESS );
    IDE_TEST( scpManager::initializeStatic() != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION(load_error_msb_error);
    {
        smuUtility::outputMsg("ERROR: \n"
                              "Can't Load Error Files. \n");
        smuUtility::outputMsg("Check Directory in $ALTIBASE_HOME"IDL_FILE_SEPARATORS"msg. \n");
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/******************************************************/
/* Utility ���α׷����� ���Ǵ� Shutdown Callback  */
/******************************************************/
IDE_RC smiSmUtilShutdown(smiGlobalCallBackList* /*aCallBack*/)
{
    /* -----------------------------
     * SM Manager Shutdown
     * ----------------------------*/
    IDE_TEST( scpManager::destroyStatic() != IDE_SUCCESS );
    IDE_TEST( smrRecoveryMgr::finalize() != IDE_SUCCESS );
    IDE_TEST( smrRecoveryMgr::destroy() != IDE_SUCCESS );
    IDE_TEST( smriBackupInfoMgr::destroyStatic() != IDE_SUCCESS ); /* BUG-39938 */
    IDE_TEST( smriChangeTrackingMgr::destroyStatic() != IDE_SUCCESS ); /* BUG-39938 */
    IDE_TEST( svmTBSStartupShutdown::destroyStatic() != IDE_SUCCESS );
    IDE_TEST( smmTBSStartupShutdown::destroyStatic() != IDE_SUCCESS );
    IDE_TEST( sdbBufferMgr::destroy() != IDE_SUCCESS );
    IDE_TEST( sdsBufferMgr::destroy() != IDE_SUCCESS );
    IDE_TEST( sddDiskMgr::destroy() != IDE_SUCCESS );
    IDE_TEST( smmManager::destroyStatic( ) != IDE_SUCCESS );
    IDE_TEST( smmDirtyPageMgr::destroyStatic() != IDE_SUCCESS );
    IDE_TEST( sctTableSpaceMgr::destroy() != IDE_SUCCESS );
    IDE_TEST( smlLockMgr::destroy() != IDE_SUCCESS );

    IDE_TEST( idvManager::shutdownService() != IDE_SUCCESS );
    IDE_TEST( idvManager::destroyStatic() != IDE_SUCCESS );

    /* -----------------------------
     * Properties Shutdown
     * ----------------------------*/
    IDE_TEST( idp::destroy() != IDE_SUCCESS );

    /* -----------------------------
     * Idu Module Shutdown
     * ----------------------------*/
    IDE_TEST( iduCond::destroyStatic() != IDE_SUCCESS );
    (void) iduLatch::destroyStatic();
    IDE_TEST( iduMutexMgr::destroyStatic() != IDE_SUCCESS );
    IDE_TEST( iduMemMgr::destroyStatic() != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

#ifdef ALTIBASE_ENABLE_SMARTSSD
static IDE_RC smiSDMOpen( const SChar    * aLogDevice,
                          sdm_handle_t  ** aSDMHandle )
{
    sdm_api_version_t   version;

    version.major = API_VERSION_MAJOR_NUMBER;
    version.minor = API_VERSION_MINOR_NUMBER;

    IDE_TEST_RAISE( sdm_open( (const SChar *) aLogDevice, 
                              aSDMHandle,
                              &version ) 
                    != 0, error_sdm_open_failed );

    return IDE_SUCCESS;

    IDE_EXCEPTION( error_sdm_open_failed );
    {
        IDE_SET( ideSetErrorCode( smERR_ABORT_SDMOpenFailed ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

static IDE_RC smiSDMClose( sdm_handle_t * aSDMHandle )
{
    IDE_TEST( sdm_close( aSDMHandle ) != 0 )

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}
#endif /* ALTIBASE_ENABLE_SMARTSSD */

/******************************************************/
/* Normal Start-Up���� ���Ǵ� �ʱ�ȭ Callback  */
/******************************************************/

static IDE_RC smiStartupPreProcess(smiGlobalCallBackList*   aCallBack )
{
    UInt   sTransCnt = 0;

    /* -------------------------
     * [1] Property Loading
     * ------------------------*/
    IDE_TEST( smuProperty::load() != IDE_SUCCESS );

    /* -------------------------
     * [1-1] Recovery Test Code Added
     * ------------------------*/

    /* -------------------------
     * [2] Init CallBack
     * ------------------------*/
    gSmiGlobalCallBackList = *aCallBack;

    /* -------------------------
     * [2] SM Manager �ʱ�ȭ
     * ------------------------*/
    IDE_TEST( smxTransMgr::calibrateTransCount(&sTransCnt)
              != IDE_SUCCESS );

    ideLog::log(IDE_SERVER_0,"\n");

    ideLog::log(IDE_SERVER_0," [SM-PREPARE] Lock Manager");
    IDE_TEST( smlLockMgr::initialize( sTransCnt,
                                      aCallBack->waitLockFunc,
                                      aCallBack->wakeupLockFunc )
              != IDE_SUCCESS );

    ideLog::log(IDE_SERVER_0," [SM-PREPARE] Transaction Manager");
    IDE_TEST( smxTransMgr::initialize() != IDE_SUCCESS );

    ideLog::log(IDE_SERVER_0," [SM-PREPARE] Legacy Transaction Manager");
    IDE_TEST( smxLegacyTransMgr::initializeStatic() != IDE_SUCCESS );

    ideLog::log(IDE_SERVER_0," [SM-PREPARE] Index Manager");
    IDE_TEST( smnManager::initialize() != IDE_SUCCESS );

    ideLog::log(IDE_SERVER_0," [SM-PREPARE] DataPort Manager");
    IDE_TEST( scpManager::initializeStatic() != IDE_SUCCESS );

    ideLog::log(IDE_SERVER_0," [SM-PREPARE] Direct-Path INSERT Manager");
    IDE_TEST( sdcDPathInsertMgr::initializeStatic()
              != IDE_SUCCESS );

    // PROJ-2118 BUG Reporting
    ideLog::log(IDE_SERVER_0," [SM-PREPARE] Set Debug Info Callback");
    iduFatalCallback::setCallback( smrLogMgr::writeDebugInfo );
    iduFatalCallback::setCallback( smrRecoveryMgr::writeDebugInfo );
    iduFatalCallback::setCallback( sdrRedoMgr::writeDebugInfo );

    //PROJ-2133 Incremental backup
    ideLog::log(IDE_SERVER_0," [SM-PREPARE] ChangeTracking Manager");
    IDE_TEST( smriChangeTrackingMgr::initializeStatic() != IDE_SUCCESS );

    ideLog::log(IDE_SERVER_0," [SM-PREPARE] BackupInfo Manager");
    IDE_TEST( smriBackupInfoMgr::initializeStatic() != IDE_SUCCESS );

    /*
     * TASK-6198 Samsung Smart SSD GC control
     */
#ifdef ALTIBASE_ENABLE_SMARTSSD
    if ( smuProperty::getSmartSSDLogRunGCEnable() != 0 )
    {
        ideLog::log(IDE_SERVER_0," [SM-PREPARE] Samsung Smart SSD GC control");
        IDE_TEST( smiSDMOpen( smuProperty::getSmartSSDLogDevice(),
                              &gLogSDMHandle ) 
                  != IDE_SUCCESS );
    }
#endif

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

static IDE_RC smiStartupProcess(smiGlobalCallBackList*   /*aCallBack*/ )
{
    return IDE_SUCCESS;
}

static IDE_RC smiStartupControl(smiGlobalCallBackList* /*aCallBack*/)
{
    struct rlimit sLimit;

    /* -------------------------
     * [0] Get Properties
     * ------------------------*/
    /* BUG-22201: Disk DataFile�� �ִ�ũ�Ⱑ 32G�� �Ѿ�� ���ϻ����� �����ϰ�
     * �ֽ��ϴ�.
     *
     * �ִ�ũ�Ⱑ 32G�� ���� �ʵ��� ������. ( SD_MAX_FPID_COUNT: 2^22 )
     * */
    IDE_TEST(idlOS::getrlimit( RLIMIT_FSIZE, &sLimit) != 0 );
    gDBFileMaxPageCntOfDRDB = sLimit.rlim_cur / SD_PAGE_SIZE;
    gDBFileMaxPageCntOfDRDB = gDBFileMaxPageCntOfDRDB > SD_MAX_FPID_COUNT ? SD_MAX_FPID_COUNT : gDBFileMaxPageCntOfDRDB;

    /* ---------------------------
     * [1] �⺻ SM Manager �ʱ�ȭ
     * --------------------------*/

    ideLog::log(IDE_SERVER_0,"\n");

    /* 1.�޸� ���̺����̽� ������ */
    ideLog::log(IDE_SERVER_0," [SM-PREPARE] TableSpace Manager");
    IDE_TEST( sctTableSpaceMgr::initialize() != IDE_SUCCESS );

    /* 2.Dirty Page ������ */
    ideLog::log(IDE_SERVER_0," [SM-PREPARE] Dirty Page Manager");
    IDE_TEST( smmDirtyPageMgr::initializeStatic() != IDE_SUCCESS );

    /* 3.�޸� ������ */
    ideLog::log(IDE_SERVER_0," [SM-PREPARE] Memory Manager");
    IDE_TEST( smmManager::initializeStatic() != IDE_SUCCESS );

    /* 4.���̺����̽� ���� */
    ideLog::log(IDE_SERVER_0," [SM-PREPARE] Memory Tablespace");
    IDE_TEST( smmTBSStartupShutdown::initializeStatic() != IDE_SUCCESS );

    /* PROJ-1594 Volatile TBS */
    ideLog::log( IDE_SERVER_0, " [SM-PREPARE] Volatile Manager" );
    IDE_TEST( svmManager::initializeStatic() != IDE_SUCCESS );

    ideLog::log( IDE_SERVER_0, " [SM-PREPARE] Volatile Tablespace" );
    IDE_TEST( svmTBSStartupShutdown::initializeStatic()
              != IDE_SUCCESS );

    /* 5.��ũ ������ */
    ideLog::log(IDE_SERVER_0," [SM-PREPARE] Disk Manager");
    IDE_TEST( sddDiskMgr::initialize( (UInt)gDBFileMaxPageCntOfDRDB )
              != IDE_SUCCESS );

    /*  PROJ_2102 Fast Secondary Buffer */
    ideLog::log( IDE_SERVER_0," [SM-PREPARE] Secondary Buffer Manager" );
    IDE_TEST( sdsBufferMgr::initialize() != IDE_SUCCESS );

    /* 6.���� ������ */
    ideLog::log(IDE_SERVER_0," [SM-PREPARE] Buffer Manager");
    IDE_TEST( sdbBufferMgr::initialize() != IDE_SUCCESS );

    /* ------------------------------------
     * [2] Recovery ���� SM Manager �ʱ�ȭ
     * ----------------------------------*/

    /* 7.���� ������ */
    ideLog::log(IDE_SERVER_0," [SM-PREPARE] Recovery Manager");
    IDE_TEST( smrRecoveryMgr::initialize() != IDE_SUCCESS );

    /* 8.�α� ������ */
    ideLog::log(IDE_SERVER_0," [SM-PREPARE] Log Manager");
    IDE_TEST( smrLogMgr::initialize() != IDE_SUCCESS );

    /* 9. ��ũ ���̺����̽� : ��Ŀ���� ������ ���� ������ ���� �ʰ� �ʱ�ȭ */
    ideLog::log(IDE_SERVER_0," [SM-PREPARE] Tablespace");
    IDE_TEST( sdpTableSpace::initialize() != IDE_SUCCESS );

    /* 10.��� ������ */
    ideLog::log(IDE_SERVER_0," [SM-PREPARE] Backup Manager");
    IDE_TEST( smrBackupMgr::initialize() != IDE_SUCCESS );

    ideLog::log(IDE_SERVER_0," [SM-PREPARE] Index Pool");
    IDE_TEST( smcTable::initialize() != IDE_SUCCESS );
    
    /* 11.���� ���̺� ������ */
    ideLog::log(IDE_SERVER_0," [SM-PREPARE] TempTable Manager      ");
    IDE_TEST( smiTempTable::initializeStatic() != IDE_SUCCESS );
    ideLog::log(IDE_SERVER_0,"[SUCCESS]\n");

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

static IDE_RC smiStartupMetaFirstHalf( UInt     aActionFlag )
{
    smmTBSNode * sTBSNode;
    smmMemBase   sMemBase;
    UInt         sEntryCnt;
    UInt         sCurEntryCnt;

    // aActionFlag�� SMI_STARTUP_RESETLOGS�̸� resetlog�� �Ѵ�.
    ideLog::log(IDE_SERVER_0,"\n");

    // To fix BUG-22158
    ideLog::log(IDE_SERVER_0," [SM-PREPARE] Check DataBase");

    sTBSNode = (smmTBSNode*)sctTableSpaceMgr::getFirstSpaceNode();

    IDE_TEST( smmManager::readMemBaseFromFile( sTBSNode,
                                               &sMemBase )
              != IDE_SUCCESS );
    IDE_TEST( smmDatabase::checkVersion( &sMemBase ) != IDE_SUCCESS );

    if ( smrRecoveryMgr::isCTMgrEnabled() == ID_TRUE )
    {
        IDE_TEST( smriChangeTrackingMgr::checkDBName( sMemBase.mDBname )
                  != IDE_SUCCESS );
    }
    else
    {
        /* nothing to do */
    }

    if ( smrRecoveryMgr::getBIMgrState() != SMRI_BI_MGR_FILE_REMOVED )
    {
        IDE_TEST( smriBackupInfoMgr::checkDBName( sMemBase.mDBname )
                  != IDE_SUCCESS );
    }
    else
    {
        /* nothing to do */
    }

    /* BUG-31862 resize transaction table without db migration */
    IDE_DASSERT( smmDatabase::checkTransTblSize( &sMemBase ) == IDE_SUCCESS );

    ideLog::log(IDE_SERVER_0," [SM-PREPARE] Identify Database");

    //[0] Secondary Buffer�� �Ӽ� �˻� 
    IDE_TEST( sdsBufferMgr::identify( NULL /*aStatistics*/ ) != IDE_SUCCESS );

    // [1] ��� ����Ÿ���ϵ��� �̵������� �˻��Ѵ�
    IDE_TEST( smrRecoveryMgr::identifyDatabase( aActionFlag ) != IDE_SUCCESS );

    if ((aActionFlag & SMI_STARTUP_ACTION_MASK) != SMI_STARTUP_RESETLOGS)
    {
        IDE_TEST( smrRecoveryMgr::identifyLogFiles() != IDE_SUCCESS );
    }

    if( smuProperty::getUseDWBuffer() == ID_TRUE )
    {
        IDE_TEST( sdbDWRecoveryMgr::recoverCorruptedPages()
                  != IDE_SUCCESS );
    }

    // DWFile�� ������ DATA page�� corruption �˻簡 ���� ��
    // flusher�� ����, �ʱ�ȭ�Ѵ�.
    ideLog::log( IDE_SERVER_0," [SM-PREPARE] Secondary Buffer Flusher" );
    IDE_TEST( sdsFlushMgr::initialize( smuProperty::getSBufferFlusherCnt() ) 
              != IDE_SUCCESS );

    ideLog::log(IDE_SERVER_0," [SM-PREPARE] Buffer Flusher");
    IDE_TEST( sdbFlushMgr::initialize(smuProperty::getBufferFlusherCnt() )
              != IDE_SUCCESS );

    ideLog::log(IDE_SERVER_0," [SM-PREPARE] Transaction Segment Manager");

    sEntryCnt = smrRecoveryMgr::getTXSEGEntryCnt();

    IDE_TEST( sdcTXSegMgr::adjustEntryCount( sEntryCnt, &sCurEntryCnt )
              != IDE_SUCCESS );

    IDE_TEST( sdcTXSegMgr::initialize( ID_FALSE /* aIsAttachSegment */ ) 
              != IDE_SUCCESS );

    /* FOR A4 : Disk Table�� ��� Undo �ÿ� Index�� �����Ѵ�.
                �̸� ���Ͽ� ��� table�� index�� undo ���� �����
                �غ� �Ϸ�Ǿ� �־�� �Ѵ�.
                ���� ������ ���� restart ������ �ٲ۴�.

                redo --> prepare index header --> undo
    */

    /* BUG-38962
     * restart recovery ������ MinSCNBuilder�� �ʱ�ȭ �Ѵ�. */
    ideLog::log(IDE_SERVER_0," [SM-PREPARE] Initialize Minimum SCN Builder ");
    IDE_TEST( smxTransMgr::initializeMinSCNBuilder() != IDE_SUCCESS );
    ideLog::log(IDE_SERVER_0,"[SUCCESS]\n");

    /* -------------------------
     * [2] restart recovery
     * ------------------------*/
    if ( smrRecoveryMgr::getArchiveMode() == SMI_LOG_ARCHIVE )
    {
        if ( smuProperty::getArchiveThreadAutoStart() != 0 )
        {
            ideLog::log(IDE_SERVER_0," [SM-PREPARE] Archive Log Thread Start...");
            IDE_TEST( smrLogMgr::startupLogArchiveThread()
                      != IDE_SUCCESS );
        }
    }

    if ( (aActionFlag & SMI_STARTUP_ACTION_MASK) == SMI_STARTUP_RESETLOGS )
    {
        IDE_TEST( smrRecoveryMgr::resetLogFiles() != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

static IDE_RC smiStartupMetaLatterHalf()
{
    UInt         sParallelFactor;

    sParallelFactor = smuProperty::getParallelLoadFactor();

    ideLog::log(IDE_SERVER_0," [SM-PREPARE] CheckPoint Manager");
    IDE_TEST( gSmrChkptThread.initialize() != IDE_SUCCESS );

    ideLog::log(IDE_SERVER_0," [SM-PREPARE] Rebuild Transaction Segment Entries");
    IDE_TEST( sdcTXSegMgr::rebuild() != IDE_SUCCESS );

    /* -------------------------
     * [3] start system threads
     * ------------------------*/
    ideLog::log(IDE_SERVER_0," [SM-PREPARE] Memory Garbage Collector");
    IDE_TEST( smaLogicalAger::initializeStatic() != IDE_SUCCESS );

    ideLog::log(IDE_SERVER_0," [SM-PREPARE] Delete Manager");
    IDE_TEST( smaDeleteThread::initializeStatic() != IDE_SUCCESS );

    ideLog::log(IDE_SERVER_0," [SM-PREPARE] CheckPoint Thread Start");
    IDE_TEST( gSmrChkptThread.startThread() != IDE_SUCCESS );

    /* -------------------------
     * [4] DB Refining
     * ------------------------*/
    ideLog::log(IDE_SERVER_0," [SM-PREPARE] Database Refining");
    IDE_TEST( smapManager::doIt( (SInt)(sParallelFactor) ) != IDE_SUCCESS );

    /* PROJ-2733
       smmManager::initSCN()����
       ( smapManager::doit() ���ο��� ȣ����)
       AgingViewSCN, AccessSCN ���õ��Ŀ� MinSCNBuilder �����尡 ���۵ǵ��� ������� �����մϴ�. */

    /* BUG-38962
     * restart recovery �Ϸ� �� MinSCNBuilder �����带 run �Ѵ�. */
    ideLog::log(IDE_SERVER_0," [SM-PREPARE] Minimum SCN Builder ");
    IDE_TEST( smxTransMgr::startupMinSCNBuilder() != IDE_SUCCESS );

    IDE_TEST( smcCatalogTable::doAction4EachTBL( NULL, /* aStatistics */
                                                 smcTable::initRowTemplate,
                                                 NULL )
              != IDE_SUCCESS );

    // remove unused table backups
    IDE_TEST( smcTable::deleteAllTableBackup() != IDE_SUCCESS );

    // For In-Doubt transaction
    IDE_TEST( smrRecoveryMgr::acquireLockForInDoubt() != IDE_SUCCESS );

    // BUG-28819 [SM] REBUILD_MIN_VIEWSCN_INTERVAL_�� 0���� �����ϰ�
    // ���� restart�ϸ� ������ �����մϴ�.
    // refile�ܰ� �������� Min View SCN�� rebuild�մϴ�.
    IDE_TEST( smxTransMgr::rebuildMinViewSCN( NULL /*idvSQL*/) != IDE_SUCCESS );

    /* ----------------------------
     * [5] memory index rebuiding
     * --------------------------*/

    ideLog::log(IDE_SERVER_0," [SM-PREPARE] Index Rebuilding");
    IDE_TEST( smnManager::rebuildIndexes() != IDE_SUCCESS );

    /* BUG-42724 : server ����� �� XA Ʈ��������� commit/rollback�Ǵ� ��� aging�� ��
     * ���� �÷��װ� �����Ͽ� �״� ������ �߻��Ѵ�. ���� �ε��� ������ �Ϸ� �� XA TX��
     * ���� insert/update�� ���ڵ��� ���� OID flag���� �߰�/���� �Ѵ�. */
    IDE_TEST( smxTransMgr::setOIDFlagForInDoubtTrans() != IDE_SUCCESS );
    
    /* ----------------------------
     * [6] resize transaction table size
     * --------------------------*/

    /*BUG-31862 resize transaction table without db migration */
    IDE_TEST( smmDatabase::refineTransTblSize() != IDE_SUCCESS );

    /* ----------------------------
     * [7] StatThread
     * --------------------------*/

    ideLog::log(IDE_SERVER_0," [SM-PREPARE] Stat Thread Start...");
    IDE_TEST( smiStatistics::initializeStatic() != IDE_SUCCESS );

    IDE_TEST( sdtWAExtentMgr::prepareCachedFreeNExts( NULL ) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

static IDE_RC smiStartupMeta(smiGlobalCallBackList* /*aCallBack*/,
                             UInt                   aActionFlag)
{
    /* startup Meta */
    IDE_TEST( smiStartupMetaFirstHalf( aActionFlag ) != IDE_SUCCESS );

    ideLog::log(IDE_SERVER_0," [SM-PREPARE] Restart Recovery");
    IDE_TEST( smrRecoveryMgr::restart( 
            smuProperty::getEmergencyStartupPolicy() ) != IDE_SUCCESS );

    IDE_TEST( smiStartupMetaLatterHalf() != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

static IDE_RC smiStartupService(smiGlobalCallBackList* /*aCallBack*/)
{
    if ( iduProperty::getEnableRecTest() == 1 )
    {
        IDE_TEST( smiVerifySM( NULL, SMI_VERIFY_TBS )
                  != IDE_SUCCESS );
    }

    /* service startup �ܰ迡�� volatile table���� �ʱ�ȭ�Ѵ�. */
    IDE_TEST( smaRefineDB::initAllVolatileTables() != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC smiSetRecStartupEnd()
{
    return IDE_SUCCESS;
}

static IDE_RC smiStartupShutdown(smiGlobalCallBackList* /*aCallBack*/)
{
    UInt           sParallelFactor;

    sParallelFactor = smuProperty::getParallelLoadFactor();

    switch( gStartupPhase )
    {
        case SMI_STARTUP_SHUTDOWN:
            IDE_ASSERT( 0 );
            break;

        case SMI_STARTUP_SERVICE:
        case SMI_STARTUP_META:
            ideLog::log(IDE_SERVER_0," [SM-SHUTDOWN] Stat Thread");
            IDE_TEST( smiStatistics::finalizeStatic() != IDE_SUCCESS );

            ideLog::log(IDE_SERVER_0," [SM-SHUTDOWN] CheckPoint Manager Thread");
            IDE_TEST( gSmrChkptThread.shutdown() != IDE_SUCCESS );

            ideLog::log(IDE_SERVER_0," [SM-SHUTDOWN] CheckPoint Manager");
            IDE_TEST( gSmrChkptThread.destroy() != IDE_SUCCESS );

            ideLog::log(IDE_SERVER_0," [SM-SHUTDOWN] Memory Garbage Collector Thread");
            IDE_TEST( smaLogicalAger::shutdownAll() != IDE_SUCCESS );

            //BUG-35886 server startup will fail in a test case
            //smaDeleteThread�� smaLogicalAger::destroyStatic() �������� shutdown
            //�Ǿ��Ѵ�.
            ideLog::log(IDE_SERVER_0," [SM-SHUTDOWN] Delete Manager Thread");
            IDE_TEST( smaDeleteThread::shutdownAll() != IDE_SUCCESS );

            ideLog::log(IDE_SERVER_0," [SM-SHUTDOWN] Memory Garbage Collector");
            IDE_TEST( smaLogicalAger::destroyStatic() != IDE_SUCCESS );

            ideLog::log(IDE_SERVER_0," [SM-SHUTDOWN] Delete Manager");
            IDE_TEST( smaDeleteThread::destroyStatic() != IDE_SUCCESS );

            ideLog::log(IDE_SERVER_0," [SM-SHUTDOWN] Minimum SCN Builder");
            IDE_TEST( smxTransMgr::shutdownMinSCNBuilder() != IDE_SUCCESS );

            /* BUG-41541 __FORCE_INDEX_PERSISTENCE_MODE�� ���� 0�� ��� 
             * persistent index ����� ������� �ʴ´�.*/
            if( smuProperty::forceIndexPersistenceMode() != SMN_INDEX_PERSISTENCE_NOUSE )
            {
                /* BUG-34504 - smaLogicalAger���� index header�� �����ϱ� ������,
                 * ���� thread���� ��� shutdown �� ��, persistent index���� ����,
                 * ���������� runtime���� index ���� �ı��Ѵ�. */
                ideLog::log(IDE_SERVER_0," [SM-SHUTDOWN] Write Persistent Indice");
                IDE_TEST( smnpIWManager::doIt((SInt)sParallelFactor) != IDE_SUCCESS );
            }

            ideLog::log(IDE_SERVER_0," [SM-SHUTDOWN] Index Storage Area");
            IDE_TEST( smnManager::destroyIndexes() != IDE_SUCCESS );

            ideLog::log(IDE_SERVER_0," [SM-SHUTDOWN] Transaction Segment Manager");
            IDE_TEST( sdcTXSegMgr::destroy() != IDE_SUCCESS );

            /*
             * BUG-24518 [MDB] Shutdown Phase���� �޸� ���̺� Compaction��
             * �ʿ��մϴ�.
             */
            if( smuProperty::getTableCompactAtShutdown() == 1 )
            {
                ideLog::log(IDE_SERVER_0," [SM-SHUTDOWN] Table Compaction");
                IDE_TEST( smaRefineDB::compactTables() != IDE_SUCCESS );
            }

            ideLog::log(IDE_SERVER_0," [SM-SHUTDOWN] MVRDB Table Runtime Information");
            IDE_TEST( smcCatalogTable::finalizeCatalogTable() != IDE_SUCCESS );


            /* BUG-24781 ��������������� �÷��������ڰ� ���������ں���
             * ������ destroy�Ǿ����. flushForCheckpoint ��������߿� LFG��
             * �����ϱ� ������ ���������ڰ� �����Ǹ� ������ �����������Ѵ�. */
            ideLog::log(IDE_SERVER_0," [SM-SHUTDOWN] Flush All DirtyPages And Checkpoint Database...");
            IDE_TEST( smrRecoveryMgr::finalize() != IDE_SUCCESS );

            ideLog::log(IDE_SERVER_0," [SM-SHUTDOWN] Flush Manager");
            IDE_TEST( sdbFlushMgr::destroy() != IDE_SUCCESS );

            ideLog::log(IDE_SERVER_0," [SM-SHUTDOWN] Secondary buffer Flusher ");
            IDE_TEST( sdsFlushMgr::destroy() != IDE_SUCCESS );
       
            ideLog::log( IDE_SERVER_0," [SM-SHUTDOWN] Flush ALL Secondary Buffer Meta data" );
            IDE_TEST( sdsBufferMgr::finalize() != IDE_SUCCESS );

        case SMI_STARTUP_CONTROL:

            /* �α� ������ �۾� ����*/
            IDE_TEST( smrLogMgr::shutdown() != IDE_SUCCESS );

            /* 11.���� ���̺� ������ */ 
            ideLog::log(IDE_SERVER_0," [SM-SHUTDOWN] TempTable Manager");
            IDE_TEST( smiTempTable::destroyStatic() != IDE_SUCCESS );

            ideLog::log(IDE_SERVER_0," [SM-SHUTDOWN] Index Pool Manager");
            IDE_TEST( smcTable::destroy() != IDE_SUCCESS );
 
            /* 10.��� ������ */
            ideLog::log(IDE_SERVER_0," [SM-SHUTDOWN] Backup Manager");
            IDE_TEST( smrBackupMgr::destroy() != IDE_SUCCESS );

            /* 9.��ũ ���̺����̽� ������ */
            ideLog::log(IDE_SERVER_0," [SM-SHUTDOWN] Tablespace Manager");
            IDE_TEST( sdpTableSpace::destroy() != IDE_SUCCESS );
            
            /* 8.�α� ������ */
            ideLog::log(IDE_SERVER_0," [SM-SHUTDOWN] Log Manager");
            IDE_TEST( smrLogMgr::destroy() != IDE_SUCCESS );
 
            /* 7.���� ������ */
            ideLog::log(IDE_SERVER_0," [SM-SHUTDOWN] Recovery Manager");
            IDE_TEST( smrRecoveryMgr::destroy() != IDE_SUCCESS );

            /* 6.���� ������ */
            ideLog::log(IDE_SERVER_0," [SM-SHUTDOWN] Buffer Manager");
            IDE_TEST( sdbBufferMgr::destroy() != IDE_SUCCESS );

            ideLog::log( IDE_SERVER_0," [SM-SHUTDOWN] Secondary Buffer Manager" );
            IDE_TEST( sdsBufferMgr::destroy() != IDE_SUCCESS );

            /* 5.��ũ ������ */
            ideLog::log(IDE_SERVER_0," [SM-SHUTDOWN] Disk Manager");
            IDE_TEST( sddDiskMgr::destroy() != IDE_SUCCESS );

            /* PROJ-1594 Volatile TBS */
            ideLog::log( IDE_SERVER_0, " [SM-SHUTDOWN] Volatile Tablespace Destroy..." );
            IDE_TEST( svmTBSStartupShutdown::destroyStatic() != IDE_SUCCESS );

            ideLog::log( IDE_SERVER_0, " [SM-SHUTDOWN] Volatile Manager" );
            IDE_TEST( svmManager::destroyStatic() != IDE_SUCCESS );
            
            /* 4.���̺����̽� */
            ideLog::log(IDE_SERVER_0," [SM-SHUTDOWN] Memory Tablespace Destroy...");
            IDE_TEST( smmTBSStartupShutdown::destroyStatic() != IDE_SUCCESS );

            /* 3.�޸� ������ */
            ideLog::log(IDE_SERVER_0," [SM-SHUTDOWN] Memory Manager");
            IDE_TEST( smmManager::destroyStatic() != IDE_SUCCESS );

            /* 2.Dirty Page ������ */
            ideLog::log(IDE_SERVER_0," [SM-SHUTDOWN] Dirty Page Manager");
            IDE_TEST( smmDirtyPageMgr::destroyStatic() != IDE_SUCCESS );

            /* 1.�޸� ���̺����̽� ������ */
            ideLog::log(IDE_SERVER_0," [SM-SHUTDOWN] TableSpace Manager");
            IDE_TEST( sctTableSpaceMgr::destroy() != IDE_SUCCESS );

        case SMI_STARTUP_PROCESS:
        case SMI_STARTUP_PRE_PROCESS:
            ideLog::log(IDE_SERVER_0," [SM-SHUTDOWN] BackupInfo Manager");
            IDE_TEST( smriBackupInfoMgr::destroyStatic() != IDE_SUCCESS );

            ideLog::log(IDE_SERVER_0," [SM-SHUTDOWN] ChangeTracking Manager");
            IDE_TEST( smriChangeTrackingMgr::destroyStatic() != IDE_SUCCESS );

            // PROJ-2118 BUG Reporting
            ideLog::log(IDE_SERVER_0," [SM-SHUTDOWN] Remove Debug Info Callback");
            iduFatalCallback::unsetCallback( smrLogMgr::writeDebugInfo );
            iduFatalCallback::unsetCallback( smrRecoveryMgr::writeDebugInfo );
            iduFatalCallback::unsetCallback( sdrRedoMgr::writeDebugInfo );

            ideLog::log(IDE_SERVER_0," [SM-SHUTDOWN] Direct-Path INSERT Manager");
            IDE_TEST( sdcDPathInsertMgr::destroyStatic() != IDE_SUCCESS );

            ideLog::log(IDE_SERVER_0," [SM-SHUTDOWN] DataPort");
            IDE_TEST( scpManager::destroyStatic() != IDE_SUCCESS );

            ideLog::log(IDE_SERVER_0," [SM-SHUTDOWN] Index Manager");
            IDE_TEST( smnManager::destroy() != IDE_SUCCESS );

            ideLog::log(IDE_SERVER_0," [SM-SHUTDOWN] Legacy Transaction Manager");
            IDE_TEST( smxLegacyTransMgr::destroyStatic() != IDE_SUCCESS );

            ideLog::log(IDE_SERVER_0," [SM-SHUTDOWN] Transaction Manager");
            IDE_TEST( smxTransMgr::destroy() != IDE_SUCCESS );

            ideLog::log(IDE_SERVER_0," [SM-SHUTDOWN] Lock Manager");
            IDE_TEST( smlLockMgr::destroy() != IDE_SUCCESS );

            /*
             * TASK-6198 Samsung Smart SSD GC control
             */
#ifdef ALTIBASE_ENABLE_SMARTSSD
            if( smuProperty::getSmartSSDLogRunGCEnable() != 0 )
            {
                ideLog::log(IDE_SERVER_0," [SM-SHUTDOWN] Samsung Smart SSD GC control");
                IDE_TEST( smiSDMClose( gLogSDMHandle ) 
                          != IDE_SUCCESS );
            }
#endif 
            break;

        default:
            IDE_ASSERT( 0 );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    ideLog::log(IDE_SERVER_0,"[FAILURE]\n");

    return IDE_FAILURE;
}

IDE_RC smiStartup(smiStartupPhase        aPhase,
                  UInt                   aActionFlag,
                  smiGlobalCallBackList* aCallBack)
{
    switch(aPhase)
    {
        case SMI_STARTUP_PRE_PROCESS:  /* for DB Creation    */
            IDE_TEST( smiStartupPreProcess(aCallBack) != IDE_SUCCESS );
            break;
        case SMI_STARTUP_PROCESS:  /* for DB Creation    */
            IDE_TEST( smiStartupProcess(aCallBack) != IDE_SUCCESS );
            break;
        case SMI_STARTUP_CONTROL:  /* for Recovery       */
            IDE_TEST( smiStartupControl(aCallBack) != IDE_SUCCESS );
            break;
        case SMI_STARTUP_META:  /* for upgrade meta   */
            IDE_TEST( smiStartupMeta( aCallBack, aActionFlag ) != IDE_SUCCESS );
            break;
        case SMI_STARTUP_SERVICE:  /* for normal service */
            IDE_TEST( smiStartupService(aCallBack) != IDE_SUCCESS );
            break;
        case SMI_STARTUP_SHUTDOWN:
            IDE_TEST( smiStartupShutdown(aCallBack) != IDE_SUCCESS );
            break;
        default:
            IDE_CALLBACK_FATAL("Invalid Start-Up Phase Request");
    }

    gStartupPhase = aPhase;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

smiStartupPhase smiGetStartupPhase()
{
    return gStartupPhase;
}
