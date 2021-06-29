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
 * $Id: smiTable.cpp 90302 2021-03-24 01:21:41Z emlee $
 **********************************************************************/
/**************************************************************
 * FILE DESCRIPTION : smiTable.cpp                            *
 * -----------------------------------------------------------*
 *  PROJ-2201 Innovation in sorting and hashing(temp)
 *  �� Project�� ���� DiskTemporary Table�� smiTempTable�� �и��Ǿ���.
 *  Lock/Latch/MVCC �������� TempTable�� ������� �ʱ� ������, �̸� �и��Ͽ�
 *  ���� �� ��뼺 ���� ���δ�.
 **************************************************************/
#include <idl.h>
#include <ide.h>

#include <smErrorCode.h>

#include <sma.h>
#include <smc.h>
#include <smm.h>
#include <svm.h>
#include <sml.h>
#include <smn.h>
#include <sdn.h>
#include <smp.h>
#include <sct.h>
#include <smr.h>
#include <smiTable.h>
#include <smiStatement.h>
#include <smiTrans.h>
#include <smDef.h>
#include <smcTableBackupFile.h>
#include <smiMisc.h>
#include <smiMain.h>
#include <smiTableBackup.h>
#include <smiVolTableBackup.h>
#include <smlAPI.h>
#include <sctTableSpaceMgr.h>
#include <svpFreePageList.h>
#include <sdnnModule.h>

// smiTable���� ����ϴ� callback�Լ� list.

typedef IDE_RC (*smiCreateTableFunc)(idvSQL               * aStatistics,
                                     void*                  aTrans,
                                     scSpaceID              aSpaceID,
                                     const smiColumnList  * aColumnList,
                                     UInt                   aColumnSize,
                                     const void           * aInfo,
                                     UInt                   aInfoSize,
                                     const smiValue*        aNullRow,
                                     UInt                   aFlag,
                                     ULong                  aMaxRow,
                                     smiSegAttr             aSegmentAttr,
                                     smiSegStorageAttr      aSegmentStoAttr,
                                     UInt                   aParallelDegree,
                                     const void**           aTable);

typedef  IDE_RC (*smiDropTableFunc)(
                     void              * aTrans,
                     const void        * aTable,
                     sctTBSLockValidOpt  aTBSLvOpt,
                     UInt                aFlag);


typedef IDE_RC  (*smiCreateIndexFunc)(idvSQL              *aStatistics,
                                      smiStatement*        aStatement,
                                      scSpaceID            aTableSpaceID,
                                      const void*          aTable,
                                      SChar *              aName,
                                      UInt                 aId,
                                      UInt                 aType,
                                      const smiColumnList* aKeyColumns,
                                      UInt                 aFlag,
                                      UInt                 aParallelDegree,
                                      UInt                 aBuildFlag,
                                      smiSegAttr         * aSegmentAttr,
                                      smiSegStorageAttr  * aSegmentStoAttr,
                                      ULong                aDirectKeyMaxSize,
                                      const void**         aIndex);

// create table callback functions.
static smiCreateTableFunc smiCreateTableFuncs[(SMI_TABLE_TYPE_MASK >> 12)+1] =
{
    //SMI_TABLE_META
    smcTable::createTable,
    //SMI_TABLE_TEMP_LEGACY
    NULL,
    //SMI_TABLE_MEMORY
    smcTable::createTable,
    //SMI_TABLE_DISK
    smcTable::createTable,
    //SMI_TABLE_FIXED
    smcTable::createTable,
    //SMI_TABLE_VOLATILE
    smcTable::createTable,
    //SMI_TABLE_REMOTE
    NULL
};

// drop table callback functions.
static smiDropTableFunc smiDropTableFuncs[(SMI_TABLE_TYPE_MASK >> 12)+1] =
{
    //SMI_TABLE_META
    smcTable::dropTable,
    //SMI_TABLE_TEMP_LEGACY
    NULL,
    //SMI_TABLE_MEMORY
    smcTable::dropTable,
    //SMI_TABLE_DISK
    smcTable::dropTable,
    //SMI_TABLE_FIXED
    smcTable::dropTable,
    //SMI_TABLE_VOLATILE
    smcTable::dropTable,
    //SMI_TABLE_REMOTE
    NULL
};

// create index  callback functions.
static smiCreateIndexFunc smiCreateIndexFuncs[(SMI_TABLE_TYPE_MASK >> 12)+1] =
{
    //SMI_TABLE_META
    (smiCreateIndexFunc)smiTable::createMemIndex,
    //SMI_TABLE_TEMP_LEGACY
    NULL,
    //SMI_TABLE_MEMORY
    (smiCreateIndexFunc)smiTable::createMemIndex,
    //SMI_TABLE_DISK
    (smiCreateIndexFunc)smiTable::createDiskIndex,
    //SMI_TABLE_FIXED
    (smiCreateIndexFunc)smiTable::createMemIndex,
    //SMI_TABLE_VOLATILE
    (smiCreateIndexFunc)smiTable::createVolIndex,
    //SMI_TABLE_REMOTE
    NULL
};


/*********************************************************
  function description: createTable

    FOR A4 :
     -  aTableSpaceID���ڰ� �߰���
     -  disk temp table�� create�Ҷ�,table����� SYS_TEMP_TABLE�̶�
        ������ catalog table�� fixed slot�� �Ҵ�޴´�.
        loggin�ּ�ȭ , no-lock�� ���Ͽ� smcTable�� createTempTable
        �� ȣ���Ѵ�.

     ___________________________________          _________________
    | catalog table for disk temp table |         |smcTableHeader |
    | (SYS_TEMP_TABLE)                  |         -----------------
    -------------------------------------          ^
             |                 |                   |
             |                 ��                  SYS_TEMP_TABLE�� fixed slot.
             o                 |
             |                 �� var
             o fixed              page list
               page list entry    entry

***********************************************************/
IDE_RC smiTable::createTable( idvSQL*               aStatistics,
                              scSpaceID             aTableSpaceID,
                              UInt                  /*aTableID*/,
                              smiStatement*         aStatement,
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
                              const void**          aTable )
{


    const smiColumn * sColumn;
    smiColumnList   * sColumnList;
    scSpaceID         sLobTBSID;
    scSpaceID         sLockedTBSID;
    UInt              sTableTypeFlag;

    sTableTypeFlag = aFlag & SMI_TABLE_TYPE_MASK;

    switch( sctTableSpaceMgr::getTBSLocation( aTableSpaceID ) )
    {
        case SMI_TBS_MEMORY:
            // TableSpace�� �޸� ���̺����̽� ���
            // aFlag�� table type�� disk�� �ƴ��� üũ
            IDE_DASSERT( sTableTypeFlag != SMI_TABLE_DISK );
            break;
        case SMI_TBS_VOLATILE:
            IDE_DASSERT( sTableTypeFlag == SMI_TABLE_VOLATILE );
            break;
        case SMI_TBS_DISK:
            // aFlag�� table type�� meta, Ȥ�� memory�� �ƴ��� üũ
            IDE_DASSERT( (sTableTypeFlag != SMI_TABLE_META) &&
                         (sTableTypeFlag != SMI_TABLE_MEMORY) );
            break;
        default:
            IDE_ASSERT(0);
            break;
    }
 

    IDE_TEST( aStatement->prepareDDL((smiTrans*)aStatement->mTrans)
              != IDE_SUCCESS );

    // PRJ-1548 User Memory Tablespace
    // Ʈ������� �Ϸ�ɶ�(commit or abort) TableSpace ����� �����Ѵ�.
    // ��� DML�� DDL�ÿ��� QP Validation Code���� ����� ��û�ϰ�
    // ���� ����ÿ��� ����� ��û�ϵ��� �Ǿ� ������, ����������
    // CREATE TABLE �ÿ��� �׷��� �����Ƿ� �� �Լ����� Table�� TableSpace��
    // IX ȹ���Ѵ�.

    // fix BUG-17121
    // ����, �ٸ� DDL���� �ٸ��� Table�� LockValidate�� �ʿ����
    // Index,lob ���� TableSpace�� IX�� ȹ���Ѵ�.

    // -------------- TBS Node (IX) ------------------ //
    IDE_TEST( sctTableSpaceMgr::lockTBSNodeByID(
                                    (void*)aStatement->mTrans->mTrans,
                                    aTableSpaceID,
                                    ID_TRUE,    /* intent */
                                    ID_TRUE,    /* exclusive */
                                    ID_ULONG_MAX,
                                    SCT_VAL_DDL_DML,
                                    NULL,
                                    NULL )
              != IDE_SUCCESS );

    if ( sTableTypeFlag == SMI_TABLE_DISK )
    {
        // ����, CREATE Disk TABLE ����� LOB Column�� ���ԵǾ� �ִٸ�
        // ���� TableSpace�� IX�� ȹ���ؾ� �Ѵ�.
        // �Լ������� ȹ��ȴ�.

        sLockedTBSID = aTableSpaceID;
        sColumnList = (smiColumnList *)aColumns;

        while ( sColumnList != NULL )
        {
            sColumn = sColumnList->column;

            // PROJ-1583 large geometry
            IDE_ASSERT( sColumn->value == NULL );

            if ( ( sColumn->flag & SMI_COLUMN_TYPE_MASK )
                   == SMI_COLUMN_TYPE_LOB )
            {
                // LOB Column�� ���
                sLobTBSID = sColumn->colSpace;

                // �ϴ�, ���������� �ߺ��� ��쿡 ���ؼ� ����� ȸ���ϵ���
                // ������ ó���ϱ��Ѵ�. (�����̽�)

                if ( sLockedTBSID != sLobTBSID )
                {
                    IDE_TEST( sctTableSpaceMgr::lockTBSNodeByID(
                                (void*)aStatement->mTrans->mTrans,
                                sLobTBSID,
                                ID_TRUE, /* intent */
                                ID_TRUE, /* exclusive */
                                ID_ULONG_MAX, /* wait lock timeout */
                                SCT_VAL_DDL_DML,
                                NULL,
                                NULL ) != IDE_SUCCESS );

                    sLockedTBSID = sLobTBSID;
                }
            }
            else
            {
                // �׿� Column Type
            }

            sColumnList = sColumnList->next;
        }
    }
    else
    {
        // �׿� Table Type�� LOB Column�� ������� �ʴ´�.
    }

    //BUGBUG smiDef.h���� SMI_TABLE_TYPE_MASK�� ����Ǹ�
    // ���ڵ嵵 ����Ǿ����.
    sTableTypeFlag  = sTableTypeFlag >> SMI_SHIFT_TO_TABLE_TYPE;

    IDE_TEST(smiCreateTableFuncs[sTableTypeFlag](
                        aStatistics,
                        (smxTrans*)aStatement->mTrans->mTrans,
                        aTableSpaceID,
                        aColumns,
                        aColumnSize,
                        aInfo,
                        aInfoSize,
                        aNullRow,
                        aFlag,
                        aMaxRow,
                        aSegmentAttr,
                        aSegmentStoAttr,
                        aParallelDegree,
                        aTable )
             != IDE_SUCCESS);

    *aTable = (const void*)((const UChar*)*aTable-SMP_SLOT_HEADER_SIZE);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

/*
 * META/MEMORY/DISK/TEMP/FIXED/VOLATILE TABLE�� Drop �Ѵ�.
 *
 * disk temp table�� drop�Ҷ� no-logging��
 * �ϱ� ���Ͽ�, smcTable::dropTempTable�� �θ���.
 *
 * [ ���� ]
 * [IN] aStatement - Statement
 * [IN] aTable     - Drop �� Table Handle
 * [IN] aTBSLvType - Table ���õ� TBS(table/index/lob)�鿡 ����
 *                   Lock Validation Ÿ��
 */
IDE_RC smiTable::dropTable( smiStatement       * aStatement,
                            const  void        * aTable,
                            smiTBSLockValidType  aTBSLvType )
{
    UInt  sTableTypeFlag;
    const smcTableHeader* sTableHeader;

    sTableHeader = (smcTableHeader*)((UChar*)aTable+SMP_SLOT_HEADER_SIZE);

    sTableTypeFlag = SMI_GET_TABLE_TYPE( sTableHeader );

    IDU_FIT_POINT( "2.BUG-42411@smiTable::dropTable" );
    IDE_TEST( aStatement->prepareDDL((smiTrans*)aStatement->mTrans)
              != IDE_SUCCESS );

    //BUGBUG smiDef.h���� SMI_TABLE_TYPE_MASK�� ����Ǹ�
    // ���ڵ嵵 ����Ǿ����.
    //sTableTypeFlag  = sTableTypeFlag >> 12;
    // table type flag�� ������� 12 bit shift right��
    sTableTypeFlag  = sTableTypeFlag >> SMI_SHIFT_TO_TABLE_TYPE;

    // PRJ-1548 User Memory TableSpace ���䵵��
    // Validate Table ���� ������ ���� Lock�� ȹ���Ѵ�.
    // [1] Table�� TableSpace�� ���� IX
    // [2] Table�� ���� X
    // [3] Table�� Index, Lob Column TableSpace�� ���� IX
    // Table�� ������ Table�� TableSpace�̸� �׿� ���� IX��
    // ȹ���Ѵ�.

    IDE_TEST( smiDropTableFuncs[sTableTypeFlag](
                          (smxTrans*)aStatement->mTrans->mTrans,
                          ((UChar*)aTable+SMP_SLOT_HEADER_SIZE),
                          sctTableSpaceMgr::getTBSLvType2Opt( aTBSLvType ),
                          SMC_LOCK_TABLE ) 
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}
/*********************************************************
 * Description : table info �����ϴ� �Լ�
 * Implementation :
 *    - Input
 *      aStatistics : statistics
 *      aTable      : table header
 *      aColumns    : column list
 *      aColumnSize : column size
 *      aInfo       : ���ο� table info
 *      aInfoSize   : info size
 *      aFlag       : ���ο� table flag
 *                    ( �Ǵ� table_flag ������ ������ �˸��� ���� )
 *      aTBSLvType  : tablespace validation option
 *    - Output
***********************************************************/

IDE_RC smiTable::modifyTableInfo( smiStatement*        aStatement,
                                  const void*          aTable,
                                  const smiColumnList* aColumns,
                                  UInt                 aColumnSize,
                                  const void*          aInfo,
                                  UInt                 aInfoSize,
                                  UInt                 aFlag,
                                  smiTBSLockValidType  aTBSLvType )
{
    IDE_TEST( modifyTableInfo( aStatement,
                               aTable,
                               aColumns,
                               aColumnSize,
                               aInfo,
                               aInfoSize,
                               aFlag,
                               aTBSLvType,
                               0,  // max row
                               0,  // parallel degree �����ȵ��� �ǹ�
                               ID_TRUE ) // aIsInitRowTemplate
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC smiTable::modifyTableInfo( smiStatement*        aStatement,
                                  const void*          aTable,
                                  const smiColumnList* aColumns,
                                  UInt                 aColumnSize,
                                  const void*          aInfo,
                                  UInt                 aInfoSize,
                                  UInt                 aFlag,
                                  smiTBSLockValidType  aTBSLvType,
                                  ULong                aMaxRow,
                                  UInt                 aParallelDegree,
                                  idBool               aIsInitRowTemplate )
{
    IDE_TEST( aStatement->prepareDDL((smiTrans*)aStatement->mTrans)
              != IDE_SUCCESS );

    // PRJ-1548 User Memory TableSpace ���䵵��
    // Validate Table ���� ������ ���� Lock�� ȹ���Ѵ�.
    // [1] Table�� TableSpace�� ���� IX
    // [2] Table�� ���� X
    // [3] Table�� Index, Lob Column TableSpace�� ���� IX
    // Table�� ������ Table�� TableSpace�̸� �׿� ���� IX��
    // ȹ���Ѵ�.
    IDE_TEST( smcTable::modifyTableInfo(
                 (smxTrans*)aStatement->mTrans->mTrans,
                 (smcTableHeader*)((UChar*)aTable+SMP_SLOT_HEADER_SIZE),
                 aColumns,
                 aColumnSize,
                 aInfo,
                 aInfoSize,
                 aFlag,
                 aMaxRow,
                 aParallelDegree,
                 sctTableSpaceMgr::getTBSLvType2Opt( aTBSLvType ),
                 aIsInitRowTemplate )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

/*********************************************************
  function description: createIndex
    FOR A4 :
    - disk temporary table�� �ε��� �����
    disk temporary table�� catalog ���̺���
    SYS_TEMP_TABLE���� variable slot�Ҵ��� �ް�,
    no logging�� ���Ͽ� smcTable::createTempIndex�� �θ���.
    �ε��� ����� �Ҵ������ snap shot�� ������ ����.
     ___________________________________          _________  ______
    | catalog table for disk temp table |       - |smcTableHeader |
    | (SYS_TEMP_TABLE)                  |       |  -----------------
    -------------------------------------       |   ^
             |                 |                |   |
             |                 ��               |   SYS_TEMP_TABLE�� fixed slot.
             o                 |                |
             |                 �� var           |
             o fixed              page list     |_ ----------------
               page list entry    entry            |smnIndexHeader |
                                                   ----------------
                                             SYS_TEMP_TABLE�� variable slot.

   ���Ӱ� �Ҵ�� �ε��� �����
   smnInsertFunc,smnDeleteFunc, smnClusterFunc�� �ε��� �ʱ�ȭ��������
   �ش� index����� create�Լ��� �Ҹ��ԵǾ�  assign�ȴ�.

   % memory table index������ aKeyColumns�� row����
    �ε��� key column offset������ ���,
    disk table index������ aKeyColumns�� key slot ����
    �ε��� key column offset������ ��´�.

***********************************************************/
IDE_RC smiTable::createIndex(idvSQL*              aStatistics,
                             smiStatement*        aStatement,
                             scSpaceID            aTableSpaceID,
                             const void*          aTable,
                             SChar*               aName,
                             UInt                 aId,
                             UInt                 aType,
                             const smiColumnList* aKeyColumns,
                             UInt                 aFlag,
                             UInt                 aParallelDegree,
                             UInt                 aBuildFlag,
                             smiSegAttr           aSegAttr,
                             smiSegStorageAttr    aSegmentStoAttr,
                             ULong                aDirectKeyMaxSize,
                             const void**         aIndex)
{
    UInt sTableTypeFlag;
    UInt sState = 0;

    smcTableHeader * sTable = (smcTableHeader*)((SChar*)aTable + SMP_SLOT_HEADER_SIZE);

    sTableTypeFlag = SMI_GET_TABLE_TYPE( sTable );

    // PRJ-1548 User Memory Tablespace
    // CREATE INDEX ���� TableSpace�� ���Ͽ� ������ IX�� ȹ���Ѵ�.
    // -------------- TBS Node (IX) ------------------ //
    IDE_TEST( sctTableSpaceMgr::lockTBSNodeByID(
                      (void*)aStatement->mTrans->mTrans,
                      aTableSpaceID,
                      ID_TRUE,    /* intent */
                      ID_TRUE,    /* exclusive */
                      ID_ULONG_MAX, /* wait lock timeout */
                      SCT_VAL_DDL_DML,
                      NULL,
                      NULL ) != IDE_SUCCESS );

    // BUGBUG smiDef.h���� SMI_TABLE_TYPE_MASK�� ����Ǹ�
    // ���ڵ嵵 ����Ǿ����.
    sTableTypeFlag  = sTableTypeFlag >> SMI_SHIFT_TO_TABLE_TYPE;

    // PRJ-1548 User Memory TableSpace ���䵵��
    // Validate Table ���� ������ ���� Lock�� ȹ���Ѵ�.
    // [1] Table�� TableSpace�� ���� IX
    // [2] Table�� ���� X
    // [3] �̹� �����Ǿ� �ִ� Table�� Index, Lob Column TableSpace�� ���� IX
    // Table�� ������ Table�� TableSpace�̸� �׿� ���� IX��
    // ȹ���Ѵ�.

    /* BUG-24025: [SC] Index Create�� Statement View SCN�� �����Ǿ� Index Build��
     * ����ɶ����� Ager�� �������� ����.
     *
     * Index Build���� Transaction�� ��� Statement�� ViewSCN�� Clear�ϰ� Create
     * Index�Ŀ� �ٽ� ���ο� ViewSCN�� ���ش�.
     * */
    /* BUG-31993 [sm_interface] The server does not reset Iterator ViewSCN 
     * after building index for Temp Table
     * TempTable�� ��쿡�� ���ο� �̹� ������� Ŀ�� �� Iterator�� �����ϱ�
     * ������, SCN�� �����ϸ� �ȵȴ�. */

    /* TempTable Type�� ���� ����. */
    IDE_ERROR( sTableTypeFlag != 
               ( SMI_TABLE_TEMP_LEGACY >> SMI_SHIFT_TO_TABLE_TYPE ) );

    IDE_TEST( smiStatement::initViewSCNOfAllStmt( aStatement ) 
              != IDE_SUCCESS );
    sState = 1;

    IDE_TEST(smiCreateIndexFuncs[sTableTypeFlag](aStatistics,
                                                 aStatement,
                                                 aTableSpaceID,
                                                 sTable,
                                                 aName,
                                                 aId,
                                                 aType,
                                                 aKeyColumns,
                                                 aFlag,
                                                 aParallelDegree,
                                                 aBuildFlag,
                                                 &aSegAttr,
                                                 &aSegmentStoAttr,
                                                 aDirectKeyMaxSize,
                                                 aIndex)
             != IDE_SUCCESS);

    IDE_DASSERT( sState == 1 );

    sState = 0;
    IDE_TEST( smiStatement::setViewSCNOfAllStmt( aStatement ) 
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if( sState != 0 )
    {
        IDE_DASSERT( sTableTypeFlag 
                     != ( SMI_TABLE_TEMP_LEGACY >> SMI_SHIFT_TO_TABLE_TYPE ) );
        (void)smiStatement::setViewSCNOfAllStmt( aStatement );
    }

    return IDE_FAILURE;
}

/*********************************************************
  function description: createMemIndex
  memory table�� index�� �����Ѵ�.
  -added for A4
***********************************************************/
IDE_RC smiTable::createMemIndex(idvSQL              *aStatistics,
                                smiStatement*        aStatement,
                                scSpaceID            aTableSpaceID,
                                smcTableHeader*      aTable,
                                SChar *              aName,
                                UInt                 aId,
                                UInt                 aType,
                                const smiColumnList* aKeyColumns,
                                UInt                 aFlag,
                                UInt                 aParallelDegree,
                                UInt                 /*aBuildFlag*/,
                                smiSegAttr         * aSegmentAttr,
                                smiSegStorageAttr  * aSegmentStoAttr,
                                ULong                aDirectKeyMaxSize,
                                const void**         aIndex)
{
    smxTrans* sTrans;

    // index type�� ���� validation
    IDE_ASSERT( aType != SMI_BUILTIN_SEQUENTIAL_INDEXTYPE_ID );
    IDE_TEST( aStatement->prepareDDL((smiTrans*)aStatement->mTrans)
              != IDE_SUCCESS);

    sTrans = (smxTrans*)aStatement->mTrans->mTrans;

    aFlag &= ~SMI_INDEX_USE_MASK;

    // fix BUG-10518
    if( (aTable->mFlag & SMI_TABLE_DISABLE_ALL_INDEX_MASK)
        == SMI_TABLE_ENABLE_ALL_INDEX )
    {
        aFlag &= ~(SMI_INDEX_USE_MASK);
        aFlag |= SMI_INDEX_USE_ENABLE;
    }
    else
    {
        aFlag &= ~(SMI_INDEX_USE_MASK);
        aFlag |= SMI_INDEX_USE_DISABLE;
    }

    /* BUG-32655 [sm-mem-index] The MMDB Ager must not ignore the failure 
     * of index aging. 
     * IndexBuilding ������ Aging�� Job�� ���� ���, IndexBuilding ��������
     * �ڱ� �������� ��ȿ�� Row�� �о Build�ϱ� ������ �� ���� Aging
     * Job���� ��ϵ� Row���� Index���� ���� �ȴ�. �̰��� �ϰ�����
     * ū ������ �� �� �����Ƿ�, IndexBuilding ���� �̸� Aging�Ѵ�.
     * 
     * �̶� Table�� XLock�� ��ұ� ������ ������ �� �ִ� ��*/
    IDE_TEST(smaLogicalAger::doInstantAgingWithTable( aTable->mSelfOID )
             != IDE_SUCCESS);

    /* Index�� Header�� �����Ѵ�. */
    IDE_TEST( smcTable::createIndex(aStatistics,
                                    sTrans,
                                    aTableSpaceID,
                                    aTable,
                                    aName,
                                    aId,
                                    aType,
                                    aKeyColumns,
                                    aFlag,
                                    SMI_INDEX_BUILD_DEFAULT,
                                    aSegmentAttr,
                                    aSegmentStoAttr,
                                    aDirectKeyMaxSize,
                                    (void**)aIndex )
              != IDE_SUCCESS );

    if( (aFlag & SMI_INDEX_USE_MASK) == SMI_INDEX_USE_ENABLE )
    {
        // ���� Index�� �����Ѵ�.
        IDE_TEST( smnManager::buildIndex(aStatistics,
                                         sTrans,
                                         aTable,
                                         (smnIndexHeader*)*aIndex,
                                         ID_TRUE,
                                         ID_TRUE,
                                         aParallelDegree,
                                         0,      // aBuildFlag   (for Disk only )
                                         0 )     // aTotalRecCnt (for Disk only )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*********************************************************
  function description: createVolIndex
  volatile table�� index�� �����Ѵ�.
  -added for A4
***********************************************************/
IDE_RC smiTable::createVolIndex(idvSQL              *aStatistics,
                                smiStatement*        aStatement,
                                scSpaceID            aTableSpaceID,
                                smcTableHeader*      aTable,
                                SChar *              aName,
                                UInt                 aId,
                                UInt                 aType,
                                const smiColumnList* aKeyColumns,
                                UInt                 aFlag,
                                UInt                 aParallelDegree,
                                UInt                 /*aBuildFlag*/,
                                smiSegAttr         * aSegmentAttr,
                                smiSegStorageAttr  * aSegmentStoAttr,
                                ULong                /*aDirectKeyMaxSize*/,
                                const void**         aIndex)
{
    smxTrans* sTrans;

    // index type�� ���� validation
    IDE_ASSERT( aType != SMI_BUILTIN_SEQUENTIAL_INDEXTYPE_ID );
    IDE_TEST( aStatement->prepareDDL((smiTrans*)aStatement->mTrans)
              != IDE_SUCCESS);

    sTrans = (smxTrans*)aStatement->mTrans->mTrans;

    aFlag &= ~SMI_INDEX_USE_MASK;

    // fix BUG-10518
    if( (aTable->mFlag & SMI_TABLE_DISABLE_ALL_INDEX_MASK)
        == SMI_TABLE_ENABLE_ALL_INDEX )
    {
        aFlag &= ~(SMI_INDEX_USE_MASK);
        aFlag |= SMI_INDEX_USE_ENABLE;
    }
    else
    {
        aFlag &= ~(SMI_INDEX_USE_MASK);
        aFlag |= SMI_INDEX_USE_DISABLE;
    }

    /* BUG-32655 [sm-mem-index] The MMDB Ager must not ignore the failure 
     * of index aging. 
     * IndexBuilding ������ Aging�� Job�� ���� ���, IndexBuilding ��������
     * �ڱ� �������� ��ȿ�� Row�� �о Build�ϱ� ������ �� ���� Aging
     * Job���� ��ϵ� Row���� Index���� ���� �ȴ�. �̰��� �ϰ�����
     * ū ������ �� �� �����Ƿ�, IndexBuilding ���� �̸� Aging�Ѵ�.
     * 
     * �̶� Table�� XLock�� ��ұ� ������ ������ �� �ִ� ��*/
    IDE_TEST(smaLogicalAger::doInstantAgingWithTable( aTable->mSelfOID )
             != IDE_SUCCESS);

    /* Index�� Header�� �����Ѵ�. */
    IDE_TEST( smcTable::createIndex(aStatistics,
                                    sTrans,
                                    aTableSpaceID,
                                    aTable,
                                    aName,
                                    aId,
                                    aType,
                                    aKeyColumns,
                                    aFlag,
                                    SMI_INDEX_BUILD_DEFAULT,
                                    aSegmentAttr,
                                    aSegmentStoAttr,
                                    0, /* aDirectKeyMaxSize */
                                    (void**)aIndex )
              != IDE_SUCCESS );

    if( (aFlag & SMI_INDEX_USE_MASK) == SMI_INDEX_USE_ENABLE )
    {
        // ���� Index�� �����Ѵ�.
        IDE_TEST( smnManager::buildIndex(aStatistics,
                                         sTrans,
                                         aTable,
                                         (smnIndexHeader*)*aIndex,
                                         ID_TRUE,
                                         ID_TRUE,
                                         aParallelDegree,
                                         0,      // aBuildFlag   (for Disk only )
                                         0 )     // aTotalRecCnt (for Disk only )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*********************************************************
  function description: createDiskIndex
  memory table�� index�� �����Ѵ�.
  -added for A4
***********************************************************/
IDE_RC smiTable::createDiskIndex(idvSQL               * aStatistics,
                                 smiStatement         * aStatement,
                                 scSpaceID              aTableSpaceID,
                                 smcTableHeader       * aTable,
                                 SChar                * aName,
                                 UInt                   aId,
                                 UInt                   aType,
                                 const smiColumnList  * aKeyColumns,
                                 UInt                   aFlag,
                                 UInt                   aParallelDegree,
                                 UInt                   aBuildFlag,
                                 smiSegAttr           * aSegmentAttr,
                                 smiSegStorageAttr    * aSegmentStoAttr,
                                 ULong                  /*aDirectKeyMaxSize*/,
                                 const void          ** aIndex)
{

    smxTrans*       sTrans;
    ULong           sTotalRecCnt = 0;
    UInt            sBUBuildThreshold;

    // index type�� ���� validation
    IDE_ASSERT( (aType == SMI_BUILTIN_B_TREE_INDEXTYPE_ID) ||
                (aType == SMI_ADDITIONAL_RTREE_INDEXTYPE_ID) );

    /* BUG-33803 Table�� ALL INDEX ENABLE ������ ��� index�� ENALBE ���·�
     * �����ϰ�, ALL INDEX DISABLE ������ ��� index�� DISABLE ���·� ������. */
    if( (aTable->mFlag & SMI_TABLE_DISABLE_ALL_INDEX_MASK)
        == SMI_TABLE_ENABLE_ALL_INDEX )
    {
        aFlag &= ~(SMI_INDEX_USE_MASK);
        aFlag |= SMI_INDEX_USE_ENABLE;
    }
    else
    {
        aFlag &= ~(SMI_INDEX_USE_MASK);
        aFlag |= SMI_INDEX_USE_DISABLE;
    }

    if( (aBuildFlag & SMI_INDEX_BUILD_LOGGING_MASK)
        == SMI_INDEX_BUILD_DEFAULT )
    {
        aBuildFlag |= SMI_INDEX_BUILD_LOGGING;
    }

    if( (aBuildFlag & SMI_INDEX_BUILD_FORCE_MASK)
        == SMI_INDEX_BUILD_DEFAULT )
    {
        aBuildFlag |= SMI_INDEX_BUILD_NOFORCE;
    }

    IDE_TEST( smcTable::getRecordCount( aTable, &sTotalRecCnt )
              != IDE_SUCCESS );
    sBUBuildThreshold = smuProperty::getIndexBuildBottomUpThreshold();

    // PROJ-1502 PARTITIONED DISK TABLE
    // Ŀ�Ե��� ���� ROW(SMI_INDEX_BUILD_UNCOMMITTED_ROW_ENABLE)�� ���ؼ�
    // �ε��� ������ �����ϱ� ���ؼ��� __DISK_INDEX_BOTTOM_UP_BUILD_THRESHOLD 
    // ������Ƽ�� �����ؾ߸� �Ѵ�.
    if ( (aBuildFlag & SMI_INDEX_BUILD_UNCOMMITTED_ROW_MASK) ==
         SMI_INDEX_BUILD_UNCOMMITTED_ROW_ENABLE )
    {
        sTotalRecCnt = ID_ULONG_MAX;
    }

    // To fix BUG-17994 : Top-down���� index build�� ���
    //                    ������ LOGGING & NOFORCE option
    if( sTotalRecCnt < sBUBuildThreshold )
    {
        aBuildFlag &= ~(SMI_INDEX_BUILD_LOGGING_MASK);
        aBuildFlag |= SMI_INDEX_BUILD_LOGGING;
        aBuildFlag &= ~(SMI_INDEX_BUILD_FORCE_MASK);
        aBuildFlag |= SMI_INDEX_BUILD_NOFORCE;
        aBuildFlag |= SMI_INDEX_BUILD_TOPDOWN;
    }


    /* DDL �غ� */
    IDE_TEST( aStatement->prepareDDL((smiTrans*)aStatement->mTrans)
              != IDE_SUCCESS );

    sTrans = (smxTrans*)aStatement->mTrans->mTrans;

    /* Index�� Header�� �����Ѵ�. */
    IDE_TEST( smcTable::createIndex(aStatistics,
                                    sTrans,
                                    aTableSpaceID,
                                    aTable,
                                    aName,
                                    aId,
                                    aType,
                                    aKeyColumns,
                                    aFlag,
                                    aBuildFlag,
                                    aSegmentAttr,
                                    aSegmentStoAttr,
                                    0, /* aDirecKeyMaxSize */
                                    (void**)aIndex )
                  != IDE_SUCCESS );

    if( (aFlag & SMI_INDEX_USE_MASK) == SMI_INDEX_USE_ENABLE )
    {
        // ���� Index�� �����Ѵ�.
        IDE_TEST( smnManager::buildIndex(aStatistics,
                                         sTrans,
                                         aTable,
                                         (smnIndexHeader*)*aIndex,
                                         ID_TRUE,  // isNeedValidation
                                         ID_TRUE,  // persistent
                                         aParallelDegree,
                                         aBuildFlag,
                                         sTotalRecCnt )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*
 * MEMORY/DISK TABLE�� Index��  Drop �Ѵ�.
 *
 * [ ���� ]
 *
 * disk temp table�� ���� index drop�� �ݵ��
 * drop table�� ���ؼ� �ҷ��� �Ѵ�.
 *
 * [ ���� ]
 * [IN] aStatement - Statement
 * [IN] aTable     - Drop �� Table Handle
 * [IN] aIndex     - Drop �� Index Handle
 * [IN] aTBSLvType - Table ���õ� TBS(table/index/lob)�鿡 ����
 *                   Lock Validation Ÿ��
 */
IDE_RC smiTable::dropIndex( smiStatement      * aStatement,
                            const void        * aTable,
                            const void        * aIndex,
                            smiTBSLockValidType aTBSLvType )
{

    smcTableHeader* sTable;
    smxTrans*       sTrans;

    sTable = (smcTableHeader*)((UChar*)aTable+SMP_SLOT_HEADER_SIZE);

    IDE_TEST( aStatement->prepareDDL((smiTrans*)aStatement->mTrans)
              != IDE_SUCCESS );

    sTrans = (smxTrans*)aStatement->mTrans->mTrans;

    // PRJ-1548 User Memory TableSpace ���䵵��
    // Validate Table ���� ������ ���� Lock�� ȹ���Ѵ�.
    // [1] Table�� TableSpace�� ���� IX
    // [2] Table�� ���� X
    // [3] Table�� Index, Lob Column TableSpace�� ���� IX
    // Table�� ������ Table�� TableSpace�̸� �׿� ���� IX��
    // ȹ���Ѵ�.
    IDE_TEST( smcTable::dropIndex( sTrans,
                                   sTable,
                                   (void*)aIndex,
                                   sctTableSpaceMgr::getTBSLvType2Opt( aTBSLvType ) )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

IDE_RC smiTable::alterIndexInfo(smiStatement* aStatement,
                                const void*   aTable,
                                const void*   aIndex,
                                const UInt    aFlag)
{
    smcTableHeader* sTable;
    smxTrans*       sTrans;

    IDE_TEST( aStatement->prepareDDL((smiTrans*)aStatement->mTrans) != IDE_SUCCESS );

    sTable = (smcTableHeader*)((UChar*)aTable+SMP_SLOT_HEADER_SIZE);
    sTrans = (smxTrans*)aStatement->mTrans->mTrans;

    // PRJ-1548 User Memory TableSpace ���䵵��
    // Validate Table ���� ������ ���� Lock�� ȹ���Ѵ�.
    // [1] Table�� TableSpace�� ���� IX
    // [2] Table�� ���� X
    // [3] Table�� Index, Lob Column TableSpace�� ���� IX
    // Table�� ������ Table�� TableSpace�̸� �׿� ���� IX��
    // ȹ���Ѵ�.
    IDE_TEST( smcTable::alterIndexInfo( sTrans,
                                        sTable,
                                        (smnIndexHeader*)aIndex,
                                        aFlag)
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

UInt smiTable::getIndexInfo(const void* aIndex)
{
    return ((smnIndexHeader*)aIndex)->mFlag;
}

smiSegAttr smiTable::getIndexSegAttr(const void* aIndex)
{
    return ((smnIndexHeader*)aIndex)->mSegAttr;
}

smiSegStorageAttr smiTable::getIndexSegStoAttr(const void* aIndex)
{
    return ((smnIndexHeader*)aIndex)->mSegStorageAttr;
}

/* PROJ-2433 */
UInt smiTable::getIndexMaxKeySize( const void * aIndex )
{
    return ((smnIndexHeader *)aIndex)->mMaxKeySize;
}
void smiTable::setIndexInfo( void * aIndex,
                             UInt   aFlag )
{
    ((smnIndexHeader *)aIndex)->mFlag = aFlag;
}
void smiTable::setIndexMaxKeySize( void * aIndex,
                                   UInt   aMaxKeySize )
{
    ((smnIndexHeader *)aIndex)->mMaxKeySize = aMaxKeySize;
}
/* PROJ-2433 end */

/* ------------------------------------------------
 * for unpin and PR-4360 ALTER TABLE
 * aFlag : ID_FALSE - unpin
 * aFlag : ID_TRUE  - altertable (add/drop column, compact)
 * ----------------------------------------------*/
IDE_RC smiTable::backupMemTable(smiStatement * aStatement,
                                const void   * aTable,
                                SChar        * aBackupFileName,
                                idvSQL       * aStatistics)
{
    smiTableBackup   sTableBackup;
    SInt             sState = 0;
    smcTableHeader * sTable;
    smmTBSNode     * sTBSNode;

    IDE_TEST(sTableBackup.initialize(aTable, aBackupFileName) != IDE_SUCCESS);
    sState = 1;

    IDE_TEST(sTableBackup.backup(aStatement,
                                 aTable,
                                 aStatistics)
             != IDE_SUCCESS);

    sState = 0;
    IDE_TEST(sTableBackup.destroy() != IDE_SUCCESS);

    /* BUG-31881 ���� Restore�� ����� PageReservation ���� */
    sTable = (smcTableHeader*)((UChar*)aTable+SMP_SLOT_HEADER_SIZE);
    IDE_TEST( sctTableSpaceMgr::findSpaceNodeBySpaceID( sTable->mSpaceID,
                                                        (void**)&sTBSNode )
              != IDE_SUCCESS );
    IDE_TEST( smmFPLManager::beginPageReservation( (smmTBSNode*)sTBSNode,
                                                   aStatement->mTrans->mTrans)
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if(sState != 0)
    {
        IDE_PUSH();

        IDE_ASSERT(sTableBackup.destroy() == IDE_SUCCESS);

        IDE_POP();
    }
    return IDE_FAILURE;
}

/*******************************************************************
 * Description : Volatile Table�� ����Ѵ�.
 *******************************************************************/
IDE_RC smiTable::backupVolatileTable(smiStatement * aStatement,
                                     const void   * aTable,
                                     SChar        * aBackupFileName,
                                     idvSQL       * aStatistics)
{
    smiVolTableBackup   sTableBackup;
    SInt                sState = 0;
    smcTableHeader    * sTable;
    svmTBSNode        * sTBSNode;

    IDE_TEST(sTableBackup.initialize(aTable, aBackupFileName) != IDE_SUCCESS);
    sState = 1;

    IDE_TEST(sTableBackup.backup(aStatement,
                                 aTable,
                                 aStatistics)
             != IDE_SUCCESS);

    sState = 0;
    IDE_TEST(sTableBackup.destroy() != IDE_SUCCESS);

    /* BUG-31881 ���� Restore�� ����� PageReservation ���� */
    sTable = (smcTableHeader*)((UChar*)aTable+SMP_SLOT_HEADER_SIZE);
    IDE_TEST( sctTableSpaceMgr::findSpaceNodeBySpaceID( sTable->mSpaceID,
                                                        (void**)&sTBSNode )
              != IDE_SUCCESS );
    IDE_TEST( svmFPLManager::beginPageReservation( (svmTBSNode*)sTBSNode,
                                                   aStatement->mTrans->mTrans)
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if(sState != 0)
    {
        IDE_PUSH();

        IDE_ASSERT(sTableBackup.destroy() == IDE_SUCCESS);

        IDE_POP();
    }
    return IDE_FAILURE;
}


/* ------------------------------------------------
 * for PR-4360 ALTER TABLE
 * ----------------------------------------------*/
IDE_RC smiTable::backupTableForAlterTable( smiStatement* aStatement,
                                           const void*   aSrcTable,
                                           const void*   aDstTable,
                                           idvSQL       *aStatistics )
{
    smxTrans               * sTrans;
    void                   * sTBSNode;
    SChar                    sBackupFileName[SM_MAX_FILE_NAME];
    idBool                   sRemoveBackupFile = ID_FALSE;
    UInt                     sState = 0;
    smOID                    sSrcOID;
    smOID                    sDstOID;
    smcTableHeader         * sSrcTable;
    smcTableHeader         * sDstTable;

    sTrans = (smxTrans*)aStatement->mTrans->mTrans;

    sSrcTable = (smcTableHeader*)((UChar*)aSrcTable+SMP_SLOT_HEADER_SIZE);
    sDstTable = (smcTableHeader*)((UChar*)aDstTable+SMP_SLOT_HEADER_SIZE);

    IDE_TEST( aStatement->prepareDDL((smiTrans*)aStatement->mTrans) 
              != IDE_SUCCESS );
    sTrans->mDoSkipCheckSCN = ID_TRUE;

    idlOS::memset( sBackupFileName, '\0', SM_MAX_FILE_NAME );

    IDL_MEM_BARRIER;

    //BUG-15627 Alter Add Column�� Abort�� ��� Table�� SCN�� �����ؾ� ��.
    //smaLogicalAger::doInstantAgingWithTable �� abort�� ���� �߻��� �� �ִ�
    //�߸��� view�� ���ԵǴ� ������ ���� �ϱ� ���� abort�ÿ� table�� scn�� �ٲپ�
    //�� table�� ������ �õ��ϰ� �ִ� Ʈ������� rebuild��Ų��.
    IDE_TEST(
        smrLogMgr::writeNTALogRec(NULL, /* idvSQL* */
                                  sTrans,
                                  smxTrans::getTransLstUndoNxtLSNPtr((void*)sTrans),
                                  sSrcTable->mSpaceID,
                                  SMR_OP_INSTANT_AGING_AT_ALTER_TABLE,
                                  sSrcTable->mSelfOID )
        != IDE_SUCCESS );

    IDE_TEST(smaLogicalAger::doInstantAgingWithTable( sSrcTable->mSelfOID )
             != IDE_SUCCESS);

    sRemoveBackupFile = ID_TRUE;

    /* PROJ-1594 Volatile TBS */
    /* Table type�� ���� backupTable �Լ��� �ٸ���.*/
    if( SMI_TABLE_TYPE_IS_VOLATILE( sSrcTable ) == ID_TRUE )
    {
        IDE_TEST( backupVolatileTable( aStatement, 
                                       aSrcTable, 
                                       sBackupFileName,
                                       aStatistics )
                  != IDE_SUCCESS );
    }
    else
    {
        /* Volatile Table�� �ƴϸ� �ݵ�� Memory ���̺��̴�. */
        IDE_DASSERT( SMI_TABLE_TYPE_IS_MEMORY( sSrcTable ) == ID_TRUE );

        IDE_TEST( backupMemTable( aStatement, 
                                  aSrcTable, 
                                  sBackupFileName,
                                  aStatistics )
                  != IDE_SUCCESS );
    }

    /* BUG-31881 backupMem/VolTable�� �ϸ鼭 pageReservation ���� */
    sState = 1;

    IDU_FIT_POINT( "1.BUG-34262@smiTable::backupTableForAlterTable" );

    /* BUG-17954 : Add Column ���� �����, Unable to open a backup file�� ����
     * ������ ����
     *
     * ������ ������ �� Transaction OID List�� Transaction�� ������ File��
     * ���쵵�� OID ������ Add�Ѵ�. ���� backupTable ���� �ϰ� �ǰ� backup����
     * ������ �߻��Ѵٸ� backupTable�� ���������� �����߻��� ������ backup
     * file��
     * �����Ѵ�. ������ ���� File�� ���ؼ� ���� ��û�� �ϰ� �ȴ�.
     * �׷��� ���� Table�� Alter Add Column�� ���̾ �ٸ� Transaction�� 
     * �����Ҷ� Alter�� ���ؼ� backupTable���� backup file�� ������µ� ����
     * Transaction�� �߸� ��û�� Delete Backup File�� LFThread�� �����Ͽ� 
     * ���� Alter�� ������ ���� �ʿ��� ������ ����� ��찡 �߻��Ѵ�. */
    IDE_TEST(sTrans->addOID(sSrcTable->mSelfOID,
                            ID_UINT_MAX,
                            sSrcTable->mSpaceID,
                            SM_OID_DELETE_ALTER_TABLE_BACKUP) != IDE_SUCCESS);

    sRemoveBackupFile = ID_FALSE;

    /* BUG-34438  if server is killed when alter table(memory table) before
     * logging SMR_OP_ALTER_TABLE log, all rows are deleted 
     * ���� �߻��� �߻� �ϸ� dest Table Restore���� �� �����
     * Page���� DB�� ������ �� �ֵ���, �� Table(aDstHeader)�� Logging�Ѵ�. 
     * smcTable::dropTablePageListPending()�Լ����� �ϴ� �۾��� �̵���Ų ��*/
    sSrcOID = sSrcTable->mSelfOID;
    sDstOID = sDstTable->mSelfOID;

    IDE_TEST(smrLogMgr::writeNTALogRec(NULL, /* idvSQL* */
                                   sTrans,
                                   smxTrans::getTransLstUndoNxtLSNPtr((void*)sTrans),
                                   sSrcTable->mSpaceID,
                                   SMR_OP_ALTER_TABLE,
                                   sSrcOID,
                                   sDstOID)
             != IDE_SUCCESS);

    sState = 0;


    IDE_TEST( smcTable::dropTablePageListPending(
                  sTrans,
                  sSrcTable,
                  ID_FALSE) // aUndo
              != IDE_SUCCESS );

    IDU_FIT_POINT( "1.BUG-34384@smiTable::backupTableForAlterTable" );

    IDU_FIT_POINT( "1.BUG-16841@smiTable::backupTableForAlterTable" );

    IDU_FIT_POINT( "1.BUG-32780@smiTable::backupTableForAlterTable" );

    sTrans->mDoSkipCheckSCN = ID_FALSE;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    
    if( sState != 0 )
    {
        (void)sctTableSpaceMgr::findSpaceNodeBySpaceID( sSrcTable->mSpaceID,
                                                        (void**)&sTBSNode );
        if( SMI_TABLE_TYPE_IS_VOLATILE( sSrcTable ) == ID_TRUE )
        {
            (void)svmFPLManager::endPageReservation( (svmTBSNode*)sTBSNode,
                                                     sTrans );
        }
        else
        {
            (void)smmFPLManager::endPageReservation( (smmTBSNode*)sTBSNode,
                                                     sTrans );
        }
    }

    /* BUG-34262 
     * When alter table add/modify/drop column, backup file cannot be
     * deleted. 
     */
    /* BUG-38254 alter table xxx ���� hang�� �ɸ��� �ֽ��ϴ�.
     * ������ ������ �̸��� tbk ������ �ִ� exception ��Ȳ�̸� 
     * ������ ������ �ʽ��ϴ�. 
     */
    if( (sRemoveBackupFile == ID_TRUE) &&
        (idlOS::access( sBackupFileName, F_OK ) == 0) &&
        ( ideGetErrorCode() != smERR_ABORT_UnableToExecuteAlterTable) )
    { 
        if( idf::unlink( sBackupFileName ) != 0 )
        {
            ideLog::log( SM_TRC_LOG_LEVEL_INTERFACE,
                         SM_TRC_INTERFACE_FILE_UNLINK_ERROR,
                         sBackupFileName, errno );
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

    sTrans->mDoSkipCheckSCN = ID_FALSE;

    return IDE_FAILURE;

}

IDE_RC smiTable::restoreMemTable(void                  * aTrans,
                                 const void            * aSrcTable,
                                 const void            * aDstTable,
                                 smOID                   aTableOID,
                                 smiColumnList         * aSrcColumnList,
                                 smiColumnList         * aBothColumnList,
                                 smiValue              * aNewRow,
                                 idBool                  aUndo,
                                 smiAlterTableCallBack * aCallBack )
{
    smiTableBackup   sTableBackup;
    SInt             sState = 0;
    smcTableHeader * sTable;
    smmTBSNode     * sTBSNode;
    
    // ���� ���̺��� ����� ������ ã�Ƽ�
    IDE_TEST(sTableBackup.initialize(aSrcTable, NULL) != IDE_SUCCESS);
    sState = 1;

    // ��� ���̺� ������Ų��.
    IDU_FIT_POINT( "1.BUG-17955@smi:table:restoretable" );
    IDE_TEST(sTableBackup.restore(aTrans,
                                  aDstTable,
                                  aTableOID,
                                  aSrcColumnList,
                                  aBothColumnList,
                                  aNewRow,
                                  aUndo,
                                  aCallBack)
             != IDE_SUCCESS);


    sState = 0;
    IDE_TEST(sTableBackup.destroy() != IDE_SUCCESS);

    /* BUG-31881 ���� PageReservation ���� */
    sTable = (smcTableHeader*)((UChar*)aSrcTable+SMP_SLOT_HEADER_SIZE);
    IDE_TEST( sctTableSpaceMgr::findSpaceNodeBySpaceID( sTable->mSpaceID,
                                                        (void**)&sTBSNode )
              != IDE_SUCCESS );
    IDE_TEST( smmFPLManager::endPageReservation( (smmTBSNode*)sTBSNode,
                                                 aTrans )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if(sState != 0)
    {
        IDE_PUSH();
        IDE_ASSERT(sTableBackup.destroy() == IDE_SUCCESS);
        IDE_POP();
    }
    return IDE_FAILURE;
}

IDE_RC smiTable::restoreVolTable(void                  * aTrans,
                                 const void            * aSrcTable,
                                 const void            * aDstTable,
                                 smOID                   aTableOID,
                                 smiColumnList         * aSrcColumnList,
                                 smiColumnList         * aBothColumnList,
                                 smiValue              * aNewRow,
                                 idBool                  aUndo,
                                 smiAlterTableCallBack * aCallBack )
{
    smiVolTableBackup   sTableBackup;
    SInt                sState = 0;
    smcTableHeader    * sTable;
    svmTBSNode        * sTBSNode;

    // ���� ���̺��� ����� ������ ã�Ƽ�
    IDE_TEST(sTableBackup.initialize(aSrcTable, NULL) != IDE_SUCCESS);
    sState = 1;

    // ��� ���̺� ������Ų��.
    IDE_TEST(sTableBackup.restore(aTrans,
                                  aDstTable,
                                  aTableOID,
                                  aSrcColumnList,
                                  aBothColumnList,
                                  aNewRow,
                                  aUndo,
                                  aCallBack)
             != IDE_SUCCESS);

    sState = 0;
    IDE_TEST(sTableBackup.destroy() != IDE_SUCCESS);

    /* BUG-31881 ���� PageReservation ���� */
    sTable = (smcTableHeader*)((UChar*)aSrcTable+SMP_SLOT_HEADER_SIZE);
    IDE_TEST( sctTableSpaceMgr::findSpaceNodeBySpaceID( sTable->mSpaceID,
                                                        (void**)&sTBSNode )
              != IDE_SUCCESS );
    IDE_TEST( svmFPLManager::endPageReservation( (svmTBSNode*)sTBSNode,
                                                 aTrans )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if(sState != 0)
    {
        IDE_PUSH();
        IDE_ASSERT(sTableBackup.destroy() == IDE_SUCCESS);
        IDE_POP();
    }
    return IDE_FAILURE;
}

IDE_RC smiTable::restoreTableForAlterTable( 
    smiStatement          * aStatement,
    const void            * aSrcTable,
    const void            * aDstTable,
    smOID                   aTableOID,
    smiColumnList         * aSrcColumnList,
    smiColumnList         * aBothColumnList,
    smiValue              * aNewRow,
    smiAlterTableCallBack * aCallBack )
{
    smcTableHeader *sDstTableHeader =
        (smcTableHeader*)( (UChar*)aDstTable + SMP_SLOT_HEADER_SIZE );

    IDU_FIT_POINT( "1.BUG-42411@smiTable::restoreTableForAlterTable" ); 
    /* PROJ-1594 Volatile TBS */
    /* Volatile Table�� ��� smiVolTableBackup�� ����ؾ� �Ѵ�. */
    if( SMI_TABLE_TYPE_IS_VOLATILE( sDstTableHeader ) == ID_TRUE )
    {
        IDE_TEST(restoreVolTable(aStatement->mTrans->mTrans,
                                 aSrcTable,
                                 aDstTable,
                                 aTableOID,
                                 aSrcColumnList,
                                 aBothColumnList,
                                 aNewRow,
                                 ID_FALSE, /* Undo */
                                 aCallBack)
                 != IDE_SUCCESS);
    }
    else
    {
        /* Volatile Table�� �ƴϸ� Memory Table�̾�� �Ѵ�.*/
        IDE_DASSERT( SMI_TABLE_TYPE_IS_MEMORY( sDstTableHeader ) == ID_TRUE );

        IDE_TEST(restoreMemTable(aStatement->mTrans->mTrans,  
                                 aSrcTable,
                                 aDstTable,
                                 aTableOID,
                                 aSrcColumnList,
                                 aBothColumnList,
                                 aNewRow,
                                 ID_FALSE, /* Undo */
                                 aCallBack)
                 != IDE_SUCCESS);
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : alter table �������� exception�߻����� ���� undo
 * �ϴ� ��ƾ�̴�. disk table������ ����� �ʿ����.
 ***********************************************************************/
IDE_RC  smiTable::restoreTableByAbort( void         * aTrans,
                                       smOID          aSrcOID,
                                       smOID          aDstOID,
                                       idBool         aDoRestart )
{

    smcTableHeader * sSrcHeader = NULL;
    smcTableHeader * sDstHeader = NULL;
    void           * sSrcSlot;
    smiValue        *sArrValue;
    UInt             sColumnCnt;
    smiColumnList   *sColumnList;
    UInt             sState     = 0;
    UInt             i;

    /* BUG-30457
     * AlterTable�� ����������, ���� ���̺� ������ ����
     * �� ���̺� ������ ���� ����� �������� ��ȯ�մϴ�. 
     * 
     * �׷��� ����� Old/new TableOid�� ��� Logging������ 
     * ���ſ��� NewTable�� OID�� Logging���� �ʾұ⿡, NewTable�� OID(aDstOID)
     * �� �𸣱⿡, skip�մϴ�. (������ ��뷮�� ���̴� �����̱⿡ ������ ��
     * �Ƶ� ġ�������� ������ �����ϴ�. */
    if( aDstOID != 0 )
    {
        IDE_ASSERT( smmManager::getOIDPtr( 
                        SMI_ID_TABLESPACE_SYSTEM_MEMORY_DIC,
                        aDstOID + SMP_SLOT_HEADER_SIZE,
                        (void**)&sDstHeader )
                    == IDE_SUCCESS );

        /* RestartRecovery�ÿ��� Runtime����ü�� �����ϴ�.
         * ���� �� ó�� ������ ��������� �մϴ�. */
        if( aDoRestart == ID_TRUE )
        {
            IDE_TEST( smcTable::prepare4LogicalUndo( sDstHeader )
                      != IDE_SUCCESS );
        }

        IDE_TEST( smcTable::dropTablePageListPending(aTrans,
                                                     sDstHeader,
                                                     ID_TRUE)   // aUndo
                  != IDE_SUCCESS);

        if( aDoRestart == ID_TRUE )
        {
            IDE_TEST( smcTable::finalizeTable4LogicalUndo( sDstHeader,
                                                           aTrans )
                      != IDE_SUCCESS);
        }
    }

    IDE_ASSERT( smmManager::getOIDPtr( 
                    SMI_ID_TABLESPACE_SYSTEM_MEMORY_DIC,
                    aSrcOID,
                    (void**)&sSrcSlot )
                == IDE_SUCCESS );

    sSrcHeader = (smcTableHeader*)((UChar*)sSrcSlot + SMP_SLOT_HEADER_SIZE);
    sColumnCnt = smcTable::getColumnCount(sSrcHeader);

    IDE_ASSERT( sColumnCnt > 0 );


    /* RestartRecovery�ÿ��� Runtime����ü�� �����ϴ�.
     * ���� �� ó�� ������ ��������� �մϴ�. */
    if( aDoRestart == ID_TRUE)
    {
        IDE_TEST( smcTable::prepare4LogicalUndo( sSrcHeader )
                  != IDE_SUCCESS );
        sState = 1;
    }

    /* BUG-34438  if server is killed when alter 
     * table(memory table) before logging SMR_OP_ALTER_TABLE log, 
     * all rows are deleted
     * �������̺� page���� �Ŵ޷� �ִ� ���¿��� table�� restore�ϸ�
     * ������ �ִ� row�Ӹ� �ƴ϶� ���̺� ��� ���Ϸκ��� ������ row��
     * ���̺� ���Եȴ�. Ȯ���ϰ� �������̺� page�� ��������
     * �ʴ°��� �����ϱ����� �߰��� �ڵ��̴�. */
    IDE_TEST( smcTable::dropTablePageListPending(aTrans,
                                                 sSrcHeader,
                                                 ID_TRUE)   // aUndo
              != IDE_SUCCESS);

    IDU_FIT_POINT( "smiTable::restoreTableByAbort::malloc" );
    /* Abort�ÿ��� Column ���� �� Value�� ������ Array�� ���⿡
     * �Ҵ����ݴϴ�. */
    sArrValue = NULL;
    IDE_TEST( iduMemMgr::malloc(IDU_MEM_SM_SMI,
                                (ULong)ID_SIZEOF(smiValue) * sColumnCnt,
                                (void**)&sArrValue,
                                IDU_MEM_FORCE)
              != IDE_SUCCESS );
    sState = 2;


    sColumnList = NULL;
    IDE_TEST( iduMemMgr::malloc(IDU_MEM_SM_SMI,
                                (ULong)ID_SIZEOF(smiColumnList) * sColumnCnt,
                                (void**)&sColumnList,
                                IDU_MEM_FORCE)
              != IDE_SUCCESS );
    sState = 3;

    for(i = 0 ; i < sColumnCnt; i++)
    {
        sColumnList[i].next = sColumnList + i + 1;
        sColumnList[i].column = smcTable::getColumn(sSrcHeader, i);
    }
    sColumnList[i - 1].next = NULL;

    /* Abort�̱� ������ OldTable�� ������ OldTable�� Restore�Ѵ�.
     * �׷��� Src/Dst ��� �����ϴ�. */
    if ( SMI_TABLE_TYPE_IS_MEMORY( sSrcHeader ) == ID_TRUE )
    {
        IDE_TEST( restoreMemTable( aTrans,
                                   sSrcSlot,
                                   sSrcSlot,
                                   aSrcOID,
                                   sColumnList,
                                   sColumnList,
                                   sArrValue,
                                   ID_TRUE, /* Undo */
                                   NULL )
                  != IDE_SUCCESS );
    }
    else
    {
        IDE_DASSERT( SMI_TABLE_TYPE_IS_VOLATILE( sSrcHeader ) == ID_TRUE );


        /* BUG-34438  if server is killed when alter table(memory table) before
         * logging SMR_OP_ALTER_TABLE log, all rows are deleted
         * volatile table�� ���� ó���ÿ��� restore�� �����Ѵ�.
         * restart recovery�ÿ��� ������ ����Ǿ��� �ٽ� ���������� volatile
         * table�� ���ڵ���� ������ �ʿ䰡 ���⶧���̴�. 
         * server startup�� smaRefineDB::initAllVolatileTables()���� �ʱ�ȭ��*/
        if( aDoRestart == ID_FALSE )
        {
            IDE_TEST( restoreVolTable( aTrans,
                                       sSrcSlot,
                                       sSrcSlot,
                                       aSrcOID,
                                       sColumnList,
                                       sColumnList,
                                       sArrValue,
                                       ID_TRUE, /* Undo */
                                       NULL )
                      != IDE_SUCCESS );
        }
    }

    IDU_FIT_POINT( "3.BUG-42411@smiTable::restoreTableByAbort" ); 
    sState = 2;
    IDE_TEST (iduMemMgr::free(sColumnList) != IDE_SUCCESS);
    sState = 1;
    IDE_TEST (iduMemMgr::free(sArrValue) != IDE_SUCCESS);

    if( aDoRestart == ID_TRUE)
    {
        IDE_TEST( smcTable::finalizeTable4LogicalUndo( sSrcHeader,
                                                       aTrans )
                 != IDE_SUCCESS);
        sState = 1;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    IDE_PUSH();
    switch( sState )
    {
        case 3:
            (void) iduMemMgr::free(sColumnList) ;
            sColumnList = NULL;
        case 2:
            (void) iduMemMgr::free(sArrValue) ;
            sArrValue = NULL;
        case 1:
            if(smrRecoveryMgr::isRestart() == ID_TRUE)
            {
                (void) smcTable::finalizeTable4LogicalUndo( sSrcHeader,
                                                            aTrans );
            }
            break;
        default:
            break;
    }
    IDE_POP();

    return IDE_FAILURE;
}

IDE_RC smiTable::createSequence( smiStatement    * aStatement,
                                 SLong             aStartSequence,
                                 SLong             aIncSequence,
                                 SLong             aSyncInterval,
                                 SLong             aMaxSequence,
                                 SLong             aMinSequence,
                                 UInt              aFlag,
                                 const void     ** aTable )
{

    IDE_TEST( aStatement->prepareDDL((smiTrans*)aStatement->mTrans) != IDE_SUCCESS );

    IDE_TEST(smcSequence::createSequence((smxTrans*)aStatement->mTrans->mTrans,
                                         aStartSequence,
                                         aIncSequence,
                                         aSyncInterval,
                                         aMaxSequence,
                                         aMinSequence,
                                         aFlag,
                                         (smcTableHeader**)aTable)
             != IDE_SUCCESS);

    *aTable = (const void*)((const UChar*)*aTable-SMP_SLOT_HEADER_SIZE);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

IDE_RC smiTable::alterSequence(smiStatement    * aStatement,
                               const void      * aTable,
                               SLong             aIncSequence,
                               SLong             aSyncInterval,
                               SLong             aMaxSequence,
                               SLong             aMinSequence,
                               UInt              aFlag,
                               idBool            aIsRestart,
                               SLong             aStartSequence,
                               SLong           * aLastSyncSeq)
{

    smcTableHeader *sTableHeader;

    IDE_TEST( aStatement->prepareDDL((smiTrans*)aStatement->mTrans) != IDE_SUCCESS );

    IDE_TEST_RAISE( SMP_SLOT_IS_DROP( (smpSlotHeader*)aTable ),
                    already_droped_sequence );

    sTableHeader = (smcTableHeader*)((UChar*)aTable+SMP_SLOT_HEADER_SIZE);

    IDE_TEST(smcSequence::alterSequence((smxTrans*)aStatement->mTrans->mTrans,
                                        sTableHeader,
                                        aIncSequence,
                                        aSyncInterval,
                                        aMaxSequence,
                                        aMinSequence,
                                        aFlag,
                                        aIsRestart,
                                        aStartSequence,
                                        aLastSyncSeq)
             != IDE_SUCCESS);

    return IDE_SUCCESS;

    IDE_EXCEPTION(already_droped_sequence)
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_droped_Sequence));
    }

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

IDE_RC smiTable::resetSequence( smiStatement    * aStatement,
                                const void      * aTable )
{

    smcTableHeader *sTableHeader;

    IDE_TEST( aStatement->prepareDDL( (smiTrans*)aStatement->mTrans ) != IDE_SUCCESS );

    IDE_TEST_RAISE( SMP_SLOT_IS_DROP( (smpSlotHeader*)aTable ),
                    already_droped_sequence );

    sTableHeader = (smcTableHeader*)((UChar*)aTable+SMP_SLOT_HEADER_SIZE);

    IDE_TEST( smcSequence::resetSequence( (smxTrans*)aStatement->mTrans->mTrans,
                                          sTableHeader )
              != IDE_SUCCESS);

    return IDE_SUCCESS;

    IDE_EXCEPTION( already_droped_sequence )
    {
        IDE_SET( ideSetErrorCode( smERR_ABORT_droped_Sequence ) );
    }

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

IDE_RC smiTable::readSequence(smiStatement        * aStatement,
                              const void          * aTable,
                              SInt                  aFlag,
                              SLong               * aSequence,
                              smiSeqTableCallBack * aCallBack )
{

    smxTrans       *sTrans;
    UInt            sState = 0;
    smcTableHeader *sTableHeader = NULL;
    void           *sLockItem    = NULL;

    sTrans = (smxTrans*)aStatement->getTrans()->getTrans();

    sTableHeader = (smcTableHeader*)((UChar*)aTable+SMP_SLOT_HEADER_SIZE);
    sLockItem = SMC_TABLE_LOCK( sTableHeader );

    IDE_TEST_RAISE( SMP_SLOT_IS_DROP( (smpSlotHeader*)aTable ),
                    already_droped_sequence );

    IDE_TEST_RAISE( smlLockMgr::getMutexOfLockItem( sLockItem )
                        ->lock( NULL ) != IDE_SUCCESS,
                    mutex_lock_error );
    sState = 1;

    if ( ( aFlag & SMI_SEQUENCE_MASK ) == SMI_SEQUENCE_CURR )
    {
        IDE_TEST( smcSequence::readSequenceCurr( sTableHeader,
                                                 aSequence )
                  != IDE_SUCCESS );
    }
    else
    {
        if ( ( aFlag & SMI_SEQUENCE_MASK ) == SMI_SEQUENCE_NEXT )
        {
            IDE_TEST(smcSequence::readSequenceNext(sTrans,
                                                   sTableHeader,
                                                   aSequence,
                                                   aCallBack)
                     != IDE_SUCCESS);
        }
        else
        {
            IDE_ASSERT(0);
        }
    }

    sState = 0;
    IDE_TEST_RAISE( smlLockMgr::getMutexOfLockItem( sLockItem )
                        ->unlock( ) != IDE_SUCCESS,
                    mutex_unlock_error );

    return IDE_SUCCESS;

    IDE_EXCEPTION( mutex_lock_error );
    {
        IDE_SET(ideSetErrorCode (smERR_FATAL_ThrMutexLock));
    }
    IDE_EXCEPTION( mutex_unlock_error );
    {
        IDE_SET(ideSetErrorCode (smERR_FATAL_ThrMutexUnlock));
    }
    IDE_EXCEPTION(already_droped_sequence)
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_droped_Sequence));
    }
    IDE_EXCEPTION_END;

    if(sState != 0)
    {
        (void)smlLockMgr::getMutexOfLockItem( sLockItem )->unlock();
    }

    return IDE_FAILURE;


}
/*
 * for internal (queue replicaiton) use
 */
IDE_RC smiTable::setSequence( smiStatement        * aStatement,
                              const void          * aTable,
                              SLong                 aSeqValue )
{
    smxTrans       *sTrans;
    UInt            sState = 0;
    smcTableHeader *sTableHeader = NULL;
    void           *sLockItem    = NULL;

    sTrans = (smxTrans*)aStatement->getTrans()->getTrans();

    sTableHeader = (smcTableHeader*)((UChar*)aTable+SMP_SLOT_HEADER_SIZE);
    sLockItem = SMC_TABLE_LOCK( sTableHeader );

    IDE_TEST_RAISE( SMP_SLOT_IS_DROP( (smpSlotHeader*)aTable ),
                    already_droped_sequence );

    IDE_TEST_RAISE( smlLockMgr::getMutexOfLockItem( sLockItem )
                        ->lock( NULL ) != IDE_SUCCESS,
                    mutex_lock_error );
    sState = 1;

    IDE_TEST( smcSequence::setSequenceCurr( sTrans,
                                            sTableHeader,
                                            aSeqValue )
              != IDE_SUCCESS );

    sState = 0;
    IDE_TEST_RAISE( smlLockMgr::getMutexOfLockItem( sLockItem )
                        ->unlock( ) != IDE_SUCCESS,
                    mutex_unlock_error );

    return IDE_SUCCESS;

    IDE_EXCEPTION( mutex_lock_error );
    {
        IDE_SET( ideSetErrorCode( smERR_FATAL_ThrMutexLock ) );
    }
    IDE_EXCEPTION( mutex_unlock_error );
    {
        IDE_SET( ideSetErrorCode( smERR_FATAL_ThrMutexUnlock ) );
    }
    IDE_EXCEPTION( already_droped_sequence )
    {
        IDE_SET( ideSetErrorCode( smERR_ABORT_droped_Sequence ) );
    }
    IDE_EXCEPTION_END;

    if(sState != 0)
    {
        (void)smlLockMgr::getMutexOfLockItem( sLockItem )->unlock();
    }
    else
    {
        /*do nothing*/
    }

    return IDE_FAILURE;
}

IDE_RC smiTable::dropSequence( smiStatement    * aStatement,
                               const void      * aTable )
{
    IDE_TEST( aStatement->prepareDDL((smiTrans*)aStatement->mTrans)
              != IDE_SUCCESS );

    IDE_TEST_RAISE( SMP_SLOT_IS_DROP( (smpSlotHeader*)aTable ),
                    already_droped_sequence );

    IDE_TEST( smcTable::dropTable(
                 (smxTrans*)aStatement->mTrans->mTrans,
                 (smcTableHeader*)((UChar*)aTable+SMP_SLOT_HEADER_SIZE),
                 sctTableSpaceMgr::getTBSLvType2Opt( SMI_TBSLV_DDL_DML ),
                 SMC_LOCK_MUTEX) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION(already_droped_sequence)
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_droped_Sequence));
    }

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}


IDE_RC smiTable::getSequence( const void      * aTable,
                              SLong           * aStartSequence,
                              SLong           * aIncSequence,
                              SLong           * aSyncInterval,
                              SLong           * aMaxSequence,
                              SLong           * aMinSequence,
                              UInt            * aFlag )
{
    UInt            sState = 0;
    smcTableHeader *sTableHeader = NULL;
    void           *sLockItem    = NULL;

    sTableHeader = (smcTableHeader*)((UChar*)aTable+SMP_SLOT_HEADER_SIZE);
    sLockItem = SMC_TABLE_LOCK( sTableHeader );

    IDE_TEST_RAISE( SMP_SLOT_IS_DROP( (smpSlotHeader*)aTable ),
                    already_droped_sequence );

    IDE_TEST_RAISE( smlLockMgr::getMutexOfLockItem( sLockItem )
                        ->lock( NULL ) != IDE_SUCCESS,
                    mutex_lock_error );
    sState = 1;

    *aStartSequence = sTableHeader->mSequence.mStartSequence;
    *aIncSequence   = sTableHeader->mSequence.mIncSequence;
    *aSyncInterval  = sTableHeader->mSequence.mSyncInterval;
    *aMaxSequence   = sTableHeader->mSequence.mMaxSequence;
    *aMinSequence   = sTableHeader->mSequence.mMinSequence;
    *aFlag          = sTableHeader->mSequence.mFlag;

    sState = 0;
    IDE_TEST_RAISE( smlLockMgr::getMutexOfLockItem( sLockItem )
                        ->unlock( ) != IDE_SUCCESS,
                    mutex_unlock_error );

    return IDE_SUCCESS;

    IDE_EXCEPTION( mutex_lock_error );
    {
        IDE_SET(ideSetErrorCode (smERR_FATAL_ThrMutexLock));
    }
    IDE_EXCEPTION( mutex_unlock_error );
    {
        IDE_SET(ideSetErrorCode (smERR_FATAL_ThrMutexUnlock));
    }
    IDE_EXCEPTION(already_droped_sequence)
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_droped_Sequence));
    }

    IDE_EXCEPTION_END;

    if(sState != 0)
    {
        (void)smlLockMgr::getMutexOfLockItem( sLockItem )->unlock();
    }

    return IDE_FAILURE;

}

IDE_RC smiTable::refineSequence(smiStatement  * aStatement,
                                const void    * aTable)
{
    smcTableHeader *sTableHeader;

    IDE_TEST( aStatement->prepareDDL((smiTrans*)aStatement->mTrans) != IDE_SUCCESS );

    IDE_TEST_RAISE( SMP_SLOT_IS_DROP( (smpSlotHeader*)aTable ),
                    already_droped_sequence );

    sTableHeader = (smcTableHeader*)((UChar*)aTable+SMP_SLOT_HEADER_SIZE);

    IDE_TEST( smcSequence::refineSequence( sTableHeader )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION(already_droped_sequence)
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_droped_Sequence));
    }

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

IDE_RC smiTable::disableIndex( smiStatement* aStatement,
                               const void*   aTable,
                               const void*   aIndex )
{

    UInt sFlag;
    SInt sState = 0;
    smcTableHeader *sTableHeader;
    smnIndexHeader *sIndexHeader;


    sTableHeader = (smcTableHeader*)((UChar*)aTable+SMP_SLOT_HEADER_SIZE);
    sIndexHeader = (smnIndexHeader*)aIndex;

    IDE_TEST(aStatement->prepareDDL((smiTrans*)aStatement->mTrans)
             != IDE_SUCCESS);

    // fix BUG-8672
    // �ڽ��� ����� oid�� ó���ɶ����� wait�ϱ� ������
    // statement begin�� �ϰ�, cursor�� open���� �ʱ� ������
    // memory gc�� skip�ϵ��� �Ѵ�.
   ((smxTrans*)(aStatement->mTrans->mTrans))->mDoSkipCheckSCN = ID_TRUE;
    IDL_MEM_BARRIER;

    // Instant Aging�ǽ�
    IDE_TEST(smaLogicalAger::doInstantAgingWithTable( sTableHeader->mSelfOID )
             != IDE_SUCCESS);

    ((smxTrans*)(aStatement->mTrans->mTrans))->mDoSkipCheckSCN = ID_FALSE;

    IDE_TEST( smcTable::latchExclusive( sTableHeader ) != IDE_SUCCESS );
    sState = 1;

    sFlag = sIndexHeader->mFlag & ~SMI_INDEX_PERSISTENT_MASK;

    // PRJ-1548 User Memory TableSpace ���䵵��
    // Validate Table ���� ������ ���� Lock�� ȹ���Ѵ�.
    // [1] Table�� TableSpace�� ���� IX
    // [2] Table�� ���� X
    // [3] Table�� Index, Lob Column TableSpace�� ���� IX
    // Table�� ������ Table�� TableSpace�̸� �׿� ���� IX�� ȹ���Ѵ�.
    // ��, �޸� Table�� [3] ���׿� ���ؼ� �������� �ʴ´�.
    IDE_TEST(smcTable::alterIndexInfo((smxTrans*)aStatement->mTrans->mTrans,
                                      sTableHeader,
                                      sIndexHeader,
                                      sFlag | SMI_INDEX_USE_DISABLE)
             != IDE_SUCCESS);

    /* Drop Index */
    IDE_TEST( smnManager::dropIndex( sTableHeader,
                                     sIndexHeader )
              != IDE_SUCCESS );

    sState = 0;
    IDE_TEST( smcTable::unlatch( sTableHeader ) != IDE_SUCCESS );


    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if ( sState != 0 )
    {
        IDE_PUSH();

        IDE_ASSERT( smcTable::unlatch( sTableHeader ) == IDE_SUCCESS );

        IDE_POP();
    }

    return IDE_FAILURE;

}

/*******************************************************************************
 * Description: ��� table�� ��� index�� disable �Ѵ�.
 *
 * Related Issues:
 *      PROJ-2184 RP Sync ���� ���
 *
 * aStatement       - [IN] smiStatement
 * aTable           - [IN] ��� table�� smcTableHeader
 ******************************************************************************/
IDE_RC smiTable::disableAllIndex( smiStatement  * aStatement,
                                  const void    * aTable )
{
    smxTrans        * sTrans;
    smcTableHeader  * sTableHeader;
    UInt              sTableTypeFlag;
    UInt              sState = 0;


    sTrans       = (smxTrans*)aStatement->mTrans->mTrans;
    sTableHeader = (smcTableHeader*)((UChar*)aTable+SMP_SLOT_HEADER_SIZE);

    IDE_TEST_CONT( (sTableHeader->mFlag & SMI_TABLE_DISABLE_ALL_INDEX_MASK)
                    == SMI_TABLE_DISABLE_ALL_INDEX, already_disabled );

    // disk temp table�� ���ؼ��� disable all index������� ����.
    // fix BUG-21965
    sTableTypeFlag = sTableHeader->mFlag & SMI_TABLE_TYPE_MASK;
    IDE_ASSERT( (sTableTypeFlag == SMI_TABLE_MEMORY) ||
                (sTableTypeFlag == SMI_TABLE_DISK)   ||
                (sTableTypeFlag == SMI_TABLE_VOLATILE) );

    IDE_TEST(aStatement->prepareDDL((smiTrans*)aStatement->mTrans)
             != IDE_SUCCESS);

    // PRJ-1548 User Memory TableSpace ���䵵��
    // Validate Table ���� ������ ���� Lock�� ȹ���Ѵ�.
    // [1] Table�� TableSpace�� ���� IX
    // [2] Table�� ���� X
    // [3] Table�� Index, Lob Column TableSpace�� ���� IX
    // Table�� ������ Table�� TableSpace�̸� �׿� ���� IX�� ȹ���Ѵ�.
    // ��, �޸� Table�� [3] ���׿� ���ؼ� �������� �ʴ´�.
    IDE_TEST( smcTable::validateTable( sTrans,
                                       sTableHeader,
                                       SCT_VAL_DDL_DML )
              != IDE_SUCCESS );

    IDE_TEST( smcTable::latchExclusive( sTableHeader ) != IDE_SUCCESS );
    sState = 1;

    IDE_TEST( smcTable::setUseFlagOfAllIndex( sTrans,
                                              sTableHeader,
                                              SMI_INDEX_USE_DISABLE )
              != IDE_SUCCESS );

    IDE_TEST( sTrans->addOID( sTableHeader->mSelfOID,
                              ID_UINT_MAX,
                              SMI_ID_TABLESPACE_SYSTEM_MEMORY_DIC,
                              SM_OID_ALL_INDEX_DISABLE )
              != IDE_SUCCESS );

    sState = 0;
    IDE_TEST( smcTable::unlatch( sTableHeader ) != IDE_SUCCESS );

    IDE_EXCEPTION_CONT( already_disabled );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if ( sState != 0 )
    {
        IDE_PUSH();

        IDE_ASSERT( smcTable::unlatch( sTableHeader ) == IDE_SUCCESS );

        IDE_POP();
    }

    return IDE_FAILURE;

}

/*******************************************************************************
 * Description: ��� table�� ��� index�� enable �Ѵ�.
 *
 * Related Issues:
 *      PROJ-2184 RP Sync ���� ���
 *
 * aStatement       - [IN] smiStatement
 * aTable           - [IN] ��� table�� smcTableHeader
 ******************************************************************************/
IDE_RC smiTable::enableAllIndex( smiStatement  * aStatement,
                                 const void    * aTable )
{
    UInt                 sState = 0;
    smcTableHeader     * sTableHeader;
    smxTrans           * sTrans;
    UInt                 sTableTypeFlag;


    sTrans       = (smxTrans*)aStatement->mTrans->mTrans;
    sTableHeader = (smcTableHeader*)((UChar*)aTable+SMP_SLOT_HEADER_SIZE);

    IDE_TEST_CONT( (sTableHeader->mFlag & SMI_TABLE_DISABLE_ALL_INDEX_MASK)
                    == SMI_TABLE_ENABLE_ALL_INDEX, already_enabled );

    // disk temp table�� ���ؼ��� enable  all index������� ����.
    sTableTypeFlag = sTableHeader->mFlag & SMI_TABLE_TYPE_MASK;
    IDE_ASSERT( (sTableTypeFlag == SMI_TABLE_MEMORY) ||
                (sTableTypeFlag == SMI_TABLE_DISK)   ||
                (sTableTypeFlag == SMI_TABLE_VOLATILE) );

    IDE_TEST( aStatement->prepareDDL((smiTrans*)aStatement->mTrans)
              != IDE_SUCCESS );

    if( sTableTypeFlag != SMI_TABLE_DISK )
    {
        // fix BUG-8672
        // �ڽ��� ����� oid�� ó���ɶ����� wait�ϱ� ������
        // statement begin�� �ϰ�, cursor�� open���� �ʱ� ������
        // memory gc�� skip�ϵ��� �Ѵ�.
        sTrans->mDoSkipCheckSCN = ID_TRUE;
        IDL_MEM_BARRIER;

        // Instant Aging�ǽ�
        IDE_TEST( smaLogicalAger::doInstantAgingWithTable(
                                                sTableHeader->mSelfOID )
                  != IDE_SUCCESS);

        sTrans->mDoSkipCheckSCN = ID_FALSE;
    }
    else
    {
        /* Disk table�� aging ó���� ���� �ʾƵ� �ȴ�. */
    }

    // PRJ-1548 User Memory TableSpace ���䵵��
    // Validate Table ���� ������ ���� Lock�� ȹ���Ѵ�.
    // [1] Table�� TableSpace�� ���� IX
    // [2] Table�� ���� X
    // [3] Table�� Index, Lob Column TableSpace�� ���� IX
    // Table�� ������ Table�� TableSpace�̸� �׿� ���� IX�� ȹ���Ѵ�.
    // ��, �޸� Table�� [3] ���׿� ���ؼ� �������� �ʴ´�.
    IDE_TEST( smcTable::validateTable( sTrans,
                                       sTableHeader,
                                       SCT_VAL_DDL_DML )
              != IDE_SUCCESS );

    IDE_TEST( smcTable::latchExclusive( sTableHeader ) != IDE_SUCCESS );
    sState = 1;

    // PROJ-1629
    IDE_TEST( smnManager::enableAllIndex( sTrans->mStatistics,
                                          sTrans,
                                          sTableHeader )
              != IDE_SUCCESS );

    IDE_TEST( smcTable::setUseFlagOfAllIndex( sTrans,
                                              sTableHeader,
                                              SMI_INDEX_USE_ENABLE )
              != IDE_SUCCESS);

    sState = 0;
    IDE_TEST( smcTable::unlatch( sTableHeader ) != IDE_SUCCESS );

    IDE_EXCEPTION_CONT( already_enabled );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if ( sState != 0)
    {
        IDE_PUSH();
        IDE_ASSERT( smcTable::unlatch( sTableHeader ) == IDE_SUCCESS );
        IDE_POP();
    }

    return IDE_FAILURE;

}

/******************************************************************************
 * Description: ��� table�� ��� index�� rebuild �Ѵ�.
 *
 * Related Issues:
 *      PROJ-2184 RP Sync ���� ���
 * 
 * aStatement   - [IN] smiStatement
 * aTable       - [IN] ��� table�� slot pointer
 *****************************************************************************/
IDE_RC smiTable::rebuildAllIndex( smiStatement  * aStatement,
                                  const void    * aTable )
{
    smxTrans        * sTrans;
    smcTableHeader  * sTableHeader;
    UInt              sTableTypeFlag;
    smnIndexHeader  * sIndexHeader;
    smnIndexHeader  * sIndexHeaderList;
    UInt              sIndexHeaderSize;
    smiColumnList     sColumnList[SMI_MAX_IDX_COLUMNS];
    smiColumn         sColumns[SMI_MAX_IDX_COLUMNS];
    UInt              sIndexIdx;
    UInt              sIndexCnt;
    const void      * sDummyIndexHandle;
    UInt              sState = 0;


    sTrans           = (smxTrans*)aStatement->mTrans->mTrans;
    sTableHeader     = (smcTableHeader*)((UChar*)aTable+SMP_SLOT_HEADER_SIZE);
    sIndexHeaderSize = smnManager::getSizeOfIndexHeader();

    IDE_TEST_RAISE( (sTableHeader->mFlag & SMI_TABLE_DISABLE_ALL_INDEX_MASK)
                    == SMI_TABLE_DISABLE_ALL_INDEX, all_index_disabled );

    // disk temp table�� ���ؼ��� disable all index������� ����.
    // fix BUG-21965
    sTableTypeFlag = sTableHeader->mFlag & SMI_TABLE_TYPE_MASK;
    IDE_ASSERT( (sTableTypeFlag == SMI_TABLE_MEMORY) ||
                (sTableTypeFlag == SMI_TABLE_DISK)   ||
                (sTableTypeFlag == SMI_TABLE_VOLATILE) );

    sIndexCnt = smcTable::getIndexCount( sTableHeader );

    IDU_FIT_POINT( "smiTable::rebuildAllIndex::malloc" );

    IDE_TEST( iduMemMgr::malloc( IDU_MEM_SM_SMI,
                                 (ULong)ID_SIZEOF(smnIndexHeader) * sIndexCnt,
                                 (void**)&sIndexHeaderList )
              != IDE_SUCCESS );
    sState = 1;

    /* Index ���� ���� */
    for( sIndexIdx = 0; sIndexIdx < sIndexCnt; sIndexIdx++ )
    {
        sIndexHeader = (smnIndexHeader*)smcTable::getTableIndex( sTableHeader,
                                                                 sIndexIdx );

        IDE_ERROR( (sIndexHeader->mFlag & SMI_INDEX_USE_MASK)
                   == SMI_INDEX_USE_ENABLE );

        idlOS::memcpy( &sIndexHeaderList[sIndexIdx],
                       sIndexHeader,
                       sIndexHeaderSize );
    }

    /* ��� index�� drop */
    for( sIndexIdx = 0; sIndexIdx < sIndexCnt; sIndexIdx++ )
    {
        /* index�� drop �� �� ���� index�� �ϳ��� ���ŵǱ� ������ �Ź� aIdx��
         * 0�� index, �� ù ��° �߰��ϴ� index�� �����ͼ� drop �ؾ� �Ѵ�. */
        sIndexHeader = (smnIndexHeader*)smcTable::getTableIndex( sTableHeader,
                                                                 0 ); /*aIdx*/

        IDE_TEST( dropIndex( aStatement,
                             aTable,
                             sIndexHeader,
                             SMI_TBSLV_DDL_DML )
                  != IDE_SUCCESS );
    }

    IDE_ERROR( sIndexIdx == sIndexCnt );

    /* ��� index�� create */
    for( sIndexIdx = 0; sIndexIdx < sIndexCnt; sIndexIdx++ )
    {
        sIndexHeader = &sIndexHeaderList[sIndexIdx];

        IDE_TEST( makeKeyColumnList( sTableHeader,
                                     sIndexHeader,
                                     sColumnList,
                                     sColumns )
                  != IDE_SUCCESS );

        IDE_TEST( createIndex( sTrans->mStatistics,
                               aStatement,
                               sTableHeader->mSpaceID,
                               aTable,
                               sIndexHeader->mName,
                               sIndexHeader->mId,
                               sIndexHeader->mType,
                               sColumnList,
                               sIndexHeader->mFlag,
                               0, /* aParallelDegree */
                               SMI_INDEX_BUILD_DEFAULT,
                               sIndexHeader->mSegAttr,
                               sIndexHeader->mSegStorageAttr,
                               sIndexHeader->mMaxKeySize,
                               &sDummyIndexHandle )
                  != IDE_SUCCESS );
    }

    IDE_ERROR( sIndexCnt == smcTable::getIndexCount(sTableHeader) );

    sState = 0;
    IDE_TEST( iduMemMgr::free( sIndexHeaderList ) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION( all_index_disabled );
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_ALL_INDEX_DISABLED));
    }
    IDE_EXCEPTION_END;

    switch( sState )
    {
        case 1:
            IDE_ASSERT( iduMemMgr::free( sIndexHeaderList ) == IDE_SUCCESS );
            break;
        default:
            break;
    }

    return IDE_FAILURE;
}


/*******************************************************************************
 * Description: index rebuild �������� create index�� ȣ���� �� ���ڷ� �Ѱ���
 *      key column list�� �����ϴ� �Լ��̴�.
 *
 * Related Issues:
 *      PROJ-2184 RP Sync ���� ���
 *
 * Parameters:
 *  aTableHeader    - [IN] smcTableHeader
 *  aIndexHeader    - [IN] smnIndexHeader
 *  aColumnList     - [IN] ���� ������ key column ������ ����� column list ����
 *  aColumns        - [IN] smiColumn ������ �ӽ÷� ����� ����
 ******************************************************************************/
IDE_RC smiTable::makeKeyColumnList( void            * aTableHeader,
                                    void            * aIndexHeader,
                                    smiColumnList   * aColumnList,
                                    smiColumn       * aColumns  )
{
    smnIndexHeader    * sIndexHeader    = (smnIndexHeader*)aIndexHeader;
    UInt                sColumnCnt      = sIndexHeader->mColumnCount;
    UInt                sColumnIdx;
    const smiColumn   * sColumn;

    for( sColumnIdx = 0; sColumnIdx < sColumnCnt; sColumnIdx++ )
    {
        sColumn = smcTable::getColumn( aTableHeader,
                                       sIndexHeader->mColumns[sColumnIdx]
                                            & SMI_COLUMN_ID_MASK );

        idlOS::memcpy( &aColumns[sColumnIdx],
                       sColumn,
                       ID_SIZEOF(smiColumn) );

        aColumns[sColumnIdx].flag = sIndexHeader->mColumnFlags[sColumnIdx];
        aColumnList[sColumnIdx].column = &aColumns[sColumnIdx];

        if( sColumnIdx == (sColumnCnt - 1) )
        {
            aColumnList[sColumnIdx].next = NULL;
        }
        else
        {
            aColumnList[sColumnIdx].next = &aColumnList[sColumnIdx+1];
        }
    }

    return IDE_SUCCESS;
}


/*
 * MEMORY/DISK TABLE�� ���濡 ���� DDL ����� SCN ������
 * ����Ϸ��� ȣ���Ѵ�.
 *
 * [ ���� ]
 * [IN] aStatement - Statement
 * [IN] aTable     - ������ Table Handle
 * [IN] aTBSLvType - Table ���õ� TBS(table/index/lob)�鿡 ����
 *                   Lock Validation Ÿ��
 */
IDE_RC smiTable::touchTable( smiStatement        * aStatement,
                             const void          * aTable,
                             smiTBSLockValidType   aTBSLvType )
{

    return smiTable::modifyTableInfo(
                     aStatement,
                     aTable,
                     NULL,   // *aColumns
                     0,      // aColumnSize
                     NULL,   // *aInfo
                     0,      // aInfoSize
                     SMI_TABLE_FLAG_UNCHANGE,
                     aTBSLvType,
                     0,      // aMaxRow
                     0,      // aParallelDegree
                     ID_FALSE ); //aIsInitRowTemplate

}

void smiTable::getTableInfo( const void * aTable,
                             void      ** aTableInfo )
{
    smcTableHeader    *sTable;
    smVCDesc          *sColumnVCDesc;

    /* Table info�� �����ϱ� ���� aTableInfo�� ���� �޸� ������
     * qp �ܿ��� �Ҵ�޾ƾ� �� */
    IDE_ASSERT(*aTableInfo != NULL);

    sTable = (smcTableHeader*)((UChar*)aTable+SMP_SLOT_HEADER_SIZE);
    sColumnVCDesc = &(sTable->mInfo);

    if ( sColumnVCDesc->length != 0 )
    {
        smcObject::getObjectInfo(sTable, aTableInfo);
    }
    else
    {
        *aTableInfo = NULL;
    }
}

void smiTable::getTableInfoSize( const void *aTable,
                                 UInt       *aInfoSize )
{
    const smVCDesc       *sInfoVCDesc;
    const smcTableHeader *sTable;

    sTable = (smcTableHeader*)((UChar*)aTable+SMP_SLOT_HEADER_SIZE);
    sInfoVCDesc = &(sTable->mInfo);

    *aInfoSize = sInfoVCDesc->length;
}

/***********************************************************************
 * Description : MEMORY TABLE�� FREE PAGE���� DB�� �ݳ��Ѵ�.
 *
 * aStatement [IN] �۾� STATEMENT ��ü
 * aTable     [IN] COMPACT�� TABLE�� ���
 * aPages	  [IN] COMPACT�� Page ���� (0, UINT_MAX : all)
 **********************************************************************/
IDE_RC smiTable::compactTable( smiStatement * aStatement,
                               const void   * aTable,
                               ULong          aPages )
{
    smcTableHeader * sTable;
    UInt             sPages;

    IDE_DASSERT(aStatement != NULL);
    IDE_DASSERT(aTable != NULL);

    sTable = (smcTableHeader*)((UChar*)aTable + SMP_SLOT_HEADER_SIZE);

    /* BUG-43464 UINT ���� ũ�� ID_UINT_MAX ��ŭ�� compact ���� */
    if ( aPages > (ULong)SC_MAX_PAGE_COUNT )
    {  
        sPages = SC_MAX_PAGE_COUNT; 
    }
    else
    {        
        sPages = aPages; 
    }

    IDE_TEST( smcTable::compact( aStatement->mTrans->mTrans,
                                 sTable,
                                 sPages )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*
   tsm������ ȣ���Ѵ�.
*/
IDE_RC smiTable::lockTable(smiTrans*        aTrans,
                           const void*      aTable,
                           smiTableLockMode aLockMode,
                           ULong            aLockWaitMicroSec)
{
    smcTableHeader*       sTable;
    idBool                sLocked = ID_FALSE;

    IDE_DASSERT( aTrans != NULL );
    IDE_DASSERT( aTable != NULL );

    sTable = (smcTableHeader*)((UChar*)aTable+SMP_SLOT_HEADER_SIZE);

    // ���̺��� ���̺����̽��鿡 ���Ͽ� INTENTION ����� ȹ���Ѵ�.
    IDE_TEST( sctTableSpaceMgr::lockAndValidateTBS(
                  (void*)((smxTrans*)aTrans->getTrans()), /* smxTrans* */
                  smcTable::getTableSpaceID((void*)sTable),/* smcTableHeader */
                  SCT_VAL_DDL_DML,
                  ID_TRUE,                     /* intent lock  ���� */
                  smlLockMgr::isNotISorS((smlLockMode)aLockMode),  /* exclusive lock ���� */
                  aLockWaitMicroSec ) != IDE_SUCCESS );

    // ���̺� ���Ͽ� ����� ȹ���Ѵ�.
    IDE_TEST( smlLockMgr::lockTable(((smxTrans*)aTrans->getTrans())->mSlotN,
                                     (smlLockItem *)(SMC_TABLE_LOCK( sTable )),
                                     (smlLockMode)aLockMode,
                                     aLockWaitMicroSec,
                                     NULL,
                                     &sLocked)
              != IDE_SUCCESS);

    IDE_TEST( sLocked != ID_TRUE );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

/*
    tsm������ ȣ���Ѵ�.
*/
IDE_RC smiTable::lockTable( smiTrans*   aTrans,
                            const void* aTable )
{
    IDE_DASSERT( aTrans != NULL );
    IDE_DASSERT( aTable != NULL );

    // PROJ-1548
    // smiTable::lockTable�� ȣ���ϵ��� ����
    IDE_TEST( lockTable( aTrans,
                         aTable,
                         (smiTableLockMode)SML_ISLOCK,
                         ID_ULONG_MAX ) != IDE_SUCCESS);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

// BUG-17477 : rp���� �ʿ��� �Լ�
IDE_RC smiTable::lockTable( SInt          aSlot,
                            smlLockItem  *aLockItem,
                            smlLockMode   aLockMode,
                            ULong         aLockWaitMicroSec,
                            smlLockMode  *aCurLockMode,
                            idBool       *aLocked,
                            smlLockNode **aLockNode,
                            smlLockSlot **aLockSlot )
{

    IDE_TEST( smlLockMgr::lockTable( aSlot,
                                     aLockItem,
                                     aLockMode,
                                     aLockWaitMicroSec,
                                     aCurLockMode,
                                     aLocked,
                                     aLockNode,
                                     aLockSlot ) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : Table�� Flag�� �����Ѵ�.
 *
 * aStatement [IN] �۾� STATEMENT ��ü
 * aTable     [IN] Flag�� ������ Table
 * aFlagMask  [IN] ������ Table Flag�� Mask
 * aFlagValue [IN] ������ Table Flag�� Value
 **********************************************************************/
IDE_RC smiTable::alterTableFlag( smiStatement *aStatement,
                                 const void   *aTable,
                                 UInt          aFlagMask,
                                 UInt          aFlagValue )
{
    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aTable != NULL );
    IDE_DASSERT( aFlagMask != 0);
    // Mask���� Bit�� Value�� ����ؼ��� �ȵȴ�.
    IDE_DASSERT( (~aFlagMask & aFlagValue) == 0 );

    smxTrans * sSmxTrans = (smxTrans*)aStatement->mTrans->mTrans;

    smcTableHeader * sTableHeader =
        (smcTableHeader*)((UChar*)aTable+SMP_SLOT_HEADER_SIZE);

    IDE_TEST( smcTable::alterTableFlag( sSmxTrans,
                                        sTableHeader,
                                        aFlagMask,
                                        aFlagValue )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

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
IDE_RC smiTable::writeTableMetaLog(smiTrans     * aTrans,
                                   smiTableMeta * aTableMeta,
                                   const void   * aLogBody,
                                   UInt           aLogBodyLen)
{
    return smcTable::writeTableMetaLog(aTrans->getTrans(),
                                       (smrTableMeta *)aTableMeta,
                                       aLogBody,
                                       aLogBodyLen);
}


/***********************************************************************
 * Description : Table�� Insert Limit�� �����Ѵ�.
 *
 * aStatement [IN] �۾� STATEMENT ��ü
 * aTable     [IN] Flag�� ������ Table
 * aSegAttr   [IN] ������ Table Insert Limit
 *
 **********************************************************************/
IDE_RC smiTable::alterTableSegAttr( smiStatement *aStatement,
                                    const void   *aTable,
                                    smiSegAttr    aSegAttr )
{
    smxTrans            * sSmxTrans;
    smcTableHeader      * sTableHeader;

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aTable != NULL );

    sSmxTrans    = (smxTrans*)aStatement->mTrans->mTrans;
    sTableHeader = (smcTableHeader*)((UChar*)aTable+SMP_SLOT_HEADER_SIZE);

    IDE_TEST( aStatement->prepareDDL((smiTrans*)aStatement->mTrans)
              != IDE_SUCCESS );

    IDE_TEST( smcTable::alterTableSegAttr(
                 sSmxTrans,
                 sTableHeader,
                 aSegAttr,
                 sctTableSpaceMgr::getTBSLvType2Opt( SMI_TBSLV_DDL_DML))
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : Table�� Storage Attr�� �����Ѵ�.
 *
 * aStatement [IN] �۾� STATEMENT ��ü
 * aTable     [IN] Flag�� ������ Table
 * aSegStoAttr   [IN] ������ Table Storage Attr
 *
 **********************************************************************/
IDE_RC smiTable::alterTableSegStoAttr( smiStatement        *aStatement,
                                       const void          *aTable,
                                       smiSegStorageAttr    aSegStoAttr )
{
    smxTrans *            sSmxTrans;
    smcTableHeader      * sTableHeader;

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aTable != NULL );

    sSmxTrans    = (smxTrans*)aStatement->mTrans->mTrans;
    sTableHeader = (smcTableHeader*)((UChar*)aTable+SMP_SLOT_HEADER_SIZE);

    IDE_TEST( aStatement->prepareDDL((smiTrans*)aStatement->mTrans)
              != IDE_SUCCESS );

    IDE_TEST( smcTable::alterTableSegStoAttr(
                 sSmxTrans,
                 sTableHeader,
                 aSegStoAttr,
                 sctTableSpaceMgr::getTBSLvType2Opt( SMI_TBSLV_DDL_DML))
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : Index�� INIT/MAX TRANS�� �����Ѵ�.
 *
 * aStatement [IN] �۾� STATEMENT ��ü
 * aTable     [IN] Flag�� ������ Table
 * aSegAttr   [IN] ������ INDEX INIT/MAX TRANS
 *
 **********************************************************************/
IDE_RC smiTable::alterIndexSegAttr( smiStatement *aStatement,
                                    const void   *aTable,
                                    const void   *aIndex,
                                    smiSegAttr    aSegAttr )
{
    smxTrans            * sSmxTrans;
    smcTableHeader      * sTableHeader;

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aTable != NULL );

    sSmxTrans    = (smxTrans*)aStatement->mTrans->mTrans;
    sTableHeader = (smcTableHeader*)((UChar*)aTable+SMP_SLOT_HEADER_SIZE);

    IDE_TEST( aStatement->prepareDDL((smiTrans*)aStatement->mTrans)
              != IDE_SUCCESS );

    IDE_TEST( smcTable::alterIndexSegAttr(
                 sSmxTrans,
                 sTableHeader,
                 (void*)aIndex,
                 aSegAttr,
                 sctTableSpaceMgr::getTBSLvType2Opt( SMI_TBSLV_DDL_DML))
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : Index �� Storage Attr�� �����Ѵ�.
 *
 * aStatement    [IN] �۾� STATEMENT ��ü
 * aTable        [IN] Flag�� ������ Table
 * aIndex        [IN] Flag�� ������ Index
 * aSegStoAttr   [IN] ������ Table Storage Attr
 *
 **********************************************************************/
IDE_RC smiTable::alterIndexSegStoAttr( smiStatement        *aStatement,
                                       const void       *   aTable,
                                       const void       *   aIndex,
                                       smiSegStorageAttr    aSegStoAttr )
{
    smxTrans *            sSmxTrans;
    smcTableHeader      * sTableHeader;

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aTable != NULL );

    sSmxTrans    = (smxTrans*)aStatement->mTrans->mTrans;
    sTableHeader = (smcTableHeader*)((UChar*)aTable+SMP_SLOT_HEADER_SIZE);

    IDE_TEST( aStatement->prepareDDL((smiTrans*)aStatement->mTrans)
              != IDE_SUCCESS );

    IDE_TEST( smcTable::alterIndexSegStoAttr(
                 sSmxTrans,
                 sTableHeader,
                 (void*)aIndex,
                 aSegStoAttr,
                 sctTableSpaceMgr::getTBSLvType2Opt( SMI_TBSLV_DDL_DML))
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC smiTable::alterTableAllocExts( smiStatement* aStatement,
                                      const void*   aTable,
                                      ULong         aExtendPageSize )
{
    smxTrans *            sSmxTrans;
    smcTableHeader      * sTableHeader;

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aTable != NULL );

    sSmxTrans    = (smxTrans*)aStatement->mTrans->mTrans;
    sTableHeader = (smcTableHeader*)((UChar*)aTable+SMP_SLOT_HEADER_SIZE);

    IDE_TEST( aStatement->prepareDDL((smiTrans*)aStatement->mTrans)
              != IDE_SUCCESS );

    IDE_TEST( smcTable::alterTableAllocExts(
                 sSmxTrans->mStatistics,
                 sSmxTrans,
                 sTableHeader,
                 aExtendPageSize,
                 sctTableSpaceMgr::getTBSLvType2Opt( SMI_TBSLV_DDL_DML))
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC smiTable::alterIndexAllocExts( smiStatement     *   aStatement,
                                      const void       *   aTable,
                                      const void       *   aIndex,
                                      ULong                aExtendPageSize )
{
    smxTrans            * sSmxTrans;
    smcTableHeader      * sTableHeader;

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aTable     != NULL );
    IDE_DASSERT( aIndex     != NULL );

    sSmxTrans    = (smxTrans*)aStatement->mTrans->mTrans;
    sTableHeader = (smcTableHeader*)((UChar*)aTable+SMP_SLOT_HEADER_SIZE);

    IDE_TEST( aStatement->prepareDDL((smiTrans*)aStatement->mTrans)
              != IDE_SUCCESS );

    IDE_TEST( smcTable::alterIndexAllocExts(
                 sSmxTrans->mStatistics,
                 sSmxTrans,
                 sTableHeader,
                 (void*)aIndex,
                 aExtendPageSize,
                 sctTableSpaceMgr::getTBSLvType2Opt( SMI_TBSLV_DDL_DML))
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

// fix BUG-22395
IDE_RC smiTable::alterIndexName( idvSQL              *aStatistics,
                                 smiStatement     *   aStatement,
                                 const void       *   aTable,
                                 const void       *   aIndex,
                                 SChar            *   aName)
{
    smxTrans            * sSmxTrans;
    smcTableHeader      * sTableHeader;

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aTable     != NULL );
    IDE_DASSERT( aIndex     != NULL );

    sSmxTrans    = (smxTrans*)aStatement->mTrans->mTrans;
    sTableHeader = (smcTableHeader*)((UChar*)aTable+SMP_SLOT_HEADER_SIZE);

    IDE_TEST( aStatement->prepareDDL((smiTrans*)aStatement->mTrans)
              != IDE_SUCCESS );

    IDE_TEST( smcTable::alterIndexName (
                 aStatistics,
                 sSmxTrans,
                 sTableHeader,
                 (void*)aIndex,
                 aName)
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : ������ Table�� Aging�Ѵ�.
 *
 * aStatement    [IN] �۾� STATEMENT ��ü
 * aTable        [IN] Aging�� Table
 *
 **********************************************************************/
IDE_RC smiTable::agingTable( smiStatement  * aStatement,
                             const void    * aTable )
{
    smxTrans         * sSmxTrans;
    smcTableHeader   * sTableHeader;
    smnIndexModule   * sIndexModule;

    IDE_ASSERT( aStatement != NULL );
    IDE_ASSERT( aTable     != NULL );

    sSmxTrans    = (smxTrans*)aStatement->mTrans->getTrans();
    sTableHeader = (smcTableHeader*)((UChar*)aTable+SMP_SLOT_HEADER_SIZE);

    IDE_TEST( aStatement->prepareDDL((smiTrans*)aStatement->mTrans)
              != IDE_SUCCESS );

    sIndexModule = &SDN_SEQUENTIAL_ITERATOR;

    IDE_TEST( sIndexModule->mAging( sSmxTrans->mStatistics,
                                    (void*)sSmxTrans,
                                    sTableHeader,
                                    NULL )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/***********************************************************************
 * Description : ������ Index�� Aging�Ѵ�.
 *
 * aStatement    [IN] �۾� STATEMENT ��ü
 * aIndex        [IN] Aging�� Index
 *
 **********************************************************************/
IDE_RC smiTable::agingIndex( smiStatement  * aStatement,
                             const void    * aIndex )
{
    smxTrans         * sSmxTrans;
    smnIndexModule   * sIndexModule;

    IDE_DASSERT( aStatement != NULL );

    sSmxTrans    = (smxTrans*)aStatement->mTrans->mTrans;

    IDE_TEST( aStatement->prepareDDL((smiTrans*)aStatement->mTrans)
              != IDE_SUCCESS );

    sIndexModule = (smnIndexModule*)((smnIndexHeader*)aIndex)->mModule;

    IDE_TEST( sIndexModule->mAging( sSmxTrans->mStatistics,
                                    (void*)sSmxTrans,
                                    NULL,
                                    (smnIndexHeader*)aIndex )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : PROJ-2399 row template
 *               alter table�� ����� �÷� ������ �������� row template��
 *               �ٽ� �����.
 ***********************************************************************/
IDE_RC smiTable::modifyRowTemplate( smiStatement * aStatement,
                                    const void   * aTable )
{
    smcTableHeader * sTableHeader;
    smxTrans       * sSmxTrans;

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aTable     != NULL );

    /* �̹� DDL�� �������� Tx�� ���ؼ��� �Ҹ��� �ִ�. */
    sSmxTrans = (smxTrans*)aStatement->mTrans->mTrans;
    IDE_ASSERT( sSmxTrans->mIsDDL == ID_TRUE );

    sTableHeader = (smcTableHeader*)SMI_MISC_TABLE_HEADER( aTable );

    IDE_TEST( smcTable::destroyRowTemplate( sTableHeader ) != IDE_SUCCESS );

    IDE_TEST( smcTable::initRowTemplate( NULL, /* aStatistics */
                                         sTableHeader,
                                         NULL ) /* aActionArg */
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

/***********************************************************************
 * Description : PROJ-2614 Memory Index Reorganization
 *               Ư�� �ε����� ���� reorganization�� �����Ѵ�.
 ***********************************************************************/
IDE_RC smiTable::indexReorganization( void    * aHeader )
{
    IDE_TEST( smnManager::doKeyReorganization( (smnIndexHeader*)aHeader ) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/* BUG-45853 : partiton table swap ����(BUG-45745) ���ؼ� �߰�.
               index header�� index id�� �����Ҽ� �ִ� �������̽� */
IDE_RC smiTable::swapIndexID( smiStatement * aStatement,
                              void         * aTable1,
                              void         * aIndex1,
                              void         * aTable2,
                              void         * aIndex2 )
{
    volatile UInt sIndex1ID;
    volatile UInt sIndex2ID;

    sIndex1ID = ((smnIndexHeader *)aIndex1)->mId;
    sIndex2ID = ((smnIndexHeader *)aIndex2)->mId;

    IDE_TEST( modifyIndexID( aStatement,
                             aTable1,
                             aIndex1,
                             sIndex2ID )
              != IDE_SUCCESS );

    IDE_TEST( modifyIndexID( aStatement,
                             aTable2,
                             aIndex2,
                             sIndex1ID )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC smiTable::modifyIndexID( smiStatement * aStatement,
                                const void   * aTable,
                                void         * aIndex,
                                UInt           aIndexID )
{
    smxTrans       * sTrans;
    smcTableHeader * sTable;
    smnIndexHeader * sIndex;
    scPageID         sPageID;
    scOffset         sOffset;

    sTrans = (smxTrans *)aStatement->mTrans->mTrans;
    sTable = (smcTableHeader *)((SChar *)aTable + SMP_SLOT_HEADER_SIZE);
    sIndex = (smnIndexHeader *)aIndex;

    sPageID = SM_MAKE_PID( sIndex->mSelfOID );
    sOffset = SM_MAKE_OFFSET( sIndex->mSelfOID );
    sOffset += IDU_FT_OFFSETOF( smnIndexHeader, mId );

    IDE_TEST( smrUpdate::physicalUpdate( NULL, /* idvSQL* */
                                         (void *)sTrans,
                                         SMI_ID_TABLESPACE_SYSTEM_MEMORY_DIC,
                                         sPageID,
                                         sOffset,
                                         (SChar *)&sIndex->mId, /* OLD */
                                         ID_SIZEOF( UInt ),
                                         (SChar *)&aIndexID, /* NEW */
                                         ID_SIZEOF( UInt ),
                                         NULL,
                                         0 )
              != IDE_SUCCESS );

    ideLog::log( IDE_SM_0, "[INDEX OID %ul] Index ID is changed : %u => %u",
                 sIndex->mSelfOID, sIndex->mId, aIndexID );

    if ( ( sTable->mFlag & SMI_TABLE_TYPE_MASK ) == SMI_TABLE_DISK )
    {
        sIndex->mId = aIndexID;
        /* disk��� runtime header�� index id�� ���� */
        ((sdnbHeader *)(sIndex->mHeader))->mIndexID = aIndexID;
    }
    else
    {
        sIndex->mId = aIndexID;
    }

    IDE_TEST( smmDirtyPageMgr::insDirtyPage( SMI_ID_TABLESPACE_SYSTEM_MEMORY_DIC, sPageID )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC smiTable::swapIndexColumns( smiStatement * aStatement,
                                   void         * aIndex1,
                                   void         * aIndex2 )
{
    UInt * sColumns1;
    UInt * sColumns2;
    UInt   sColumns[SMI_MAX_IDX_COLUMNS];
    UInt   i;

    sColumns1 = ((smnIndexHeader *)aIndex1)->mColumns;
    sColumns2 = ((smnIndexHeader *)aIndex2)->mColumns;

    for ( i = 0; i < ((smnIndexHeader *)aIndex1)->mColumnCount; i++ )
    {
        sColumns[i] = *( sColumns1 + i );
    }

    IDE_TEST( modifyIndexColumns( aStatement,
                                  aIndex1,
                                  sColumns2 )
              != IDE_SUCCESS );

    IDE_TEST( modifyIndexColumns( aStatement,
                                  aIndex2,
                                  sColumns )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC smiTable::modifyIndexColumns( smiStatement  * aStatement,
                                     void          * aIndex,
                                     UInt          * aColumns )
{
    smxTrans       * sTrans;
    smnIndexHeader * sIndex;
    scPageID         sPageID;
    scOffset         sOffset;
    UInt             i;

    sTrans = (smxTrans *)aStatement->mTrans->mTrans;
    sIndex = (smnIndexHeader *)aIndex;

    sPageID = SM_MAKE_PID( sIndex->mSelfOID );
    sOffset = SM_MAKE_OFFSET( sIndex->mSelfOID );
    sOffset += IDU_FT_OFFSETOF( smnIndexHeader, mColumns );

    IDE_TEST( smrUpdate::physicalUpdate( NULL, /* idvSQL* */
                                         (void *)sTrans,
                                         SMI_ID_TABLESPACE_SYSTEM_MEMORY_DIC,
                                         sPageID,
                                         sOffset,
                                         (SChar *)&sIndex->mColumns, /* OLD */
                                         ID_SIZEOF( UInt ) * sIndex->mColumnCount,
                                         (SChar *)aColumns, /* NEW */
                                         ID_SIZEOF( UInt ) * sIndex->mColumnCount,
                                         NULL,
                                         0 )
              != IDE_SUCCESS );

    ideLog::log( IDE_SM_0, "[INDEX OID %ul] Index Columns is changed : %u => %u",
                 sIndex->mSelfOID, sIndex->mColumns[0], *aColumns );

    for ( i = 0; i < sIndex->mColumnCount; i++ )
    {
        sIndex->mColumns[i] = *( aColumns + i );
    }

    IDE_TEST( smmDirtyPageMgr::insDirtyPage( SMI_ID_TABLESPACE_SYSTEM_MEMORY_DIC, sPageID )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

