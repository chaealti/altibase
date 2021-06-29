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
 * $Id: smcTableSpace.cpp 19201 2006-11-30 00:54:40Z kmkim $
 **********************************************************************/

#include <idl.h>
#include <idm.h>
#include <idu.h>
#include <ideErrorMgr.h>
#include <smDef.h>
#include <smErrorCode.h>
#include <smc.h>
#include <smu.h>
#include <smcReq.h>
#include <smcTableSpace.h>
#include <smpFixedPageList.h>
#include <smpVarPageList.h>

/*
    smcTableSpace.cpp

    Tablespace���� �ڵ��� catalog table�� ������ �ʿ��� �ڵ�
*/

smcTableSpace::smcTableSpace()
{

}

/*
    Tablespace���� ������ Table�� ���� Ư�� �۾��� �����Ѵ�.

    [IN] aTBSID      - �˻��� Tablespace�� ID
                       SC_NULL_SPACEID�� �ѱ�� ��� Tablespace�� �˻�
    [IN] aActionFunc - ������ Action�Լ�
    [IN] aActionArg  - Action�Լ��� ������ ����
 */
IDE_RC smcTableSpace::run4TablesInTBS(
                                   idvSQL*           aStatistics,
                                   scSpaceID         aTBSID,
                                   smcAction4Table   aActionFunc,
                                   void            * aActionArg)
{

    IDE_DASSERT( aActionFunc     != NULL );

    /* Catalog Table���� TBSID�� ����� Table�� ���� Action ����*/
    IDE_TEST( run4TablesInTBS( aStatistics,
                               SMC_CAT_TABLE,
                               aTBSID,
                               aActionFunc,
                               aActionArg ) != IDE_SUCCESS );

    /* Temp Catalog Table���� TBSID�� ����� Table�� ���� Action ����*/
    IDE_TEST( run4TablesInTBS( aStatistics,
                               SMC_CAT_TEMPTABLE,
                               aTBSID,
                               aActionFunc,
                               aActionArg ) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*
    Catalog Table���� Ư�� Tablespace�� ����� Table�� ����
    Action�Լ��� �����Ѵ�.

    [IN] aCatTableHeader - Table�� �˻��� Catalog Table �� Header
    [IN] aTBSID      - �˻��� Tablespace�� ID
                       SC_NULL_SPACEID�� �ѱ�� ��� Tablespace�� �˻�
    [IN] aActionFunc - ������ Action�Լ�
    [IN] aActionArg  - Action�Լ��� ������ ����
 */
IDE_RC smcTableSpace::run4TablesInTBS(
                               idvSQL*           aStatistics,
                               smcTableHeader  * aCatTableHeader,
                               scSpaceID         aTBSID,
                               smcAction4Table   aActionFunc,
                               void            * aActionArg)
{
    smcTableHeader    * sCatTblHdr;
    smcTableHeader    * sTableHdr;
    smpSlotHeader     * sSlotHdr;
    SChar             * sCurPtr;
    SChar             * sNxtPtr;
    smSCN               sSlotSCN;

    IDE_DASSERT( aCatTableHeader != NULL);
    IDE_DASSERT( aActionFunc     != NULL );

    sCatTblHdr = (smcTableHeader*)aCatTableHeader;
    sCurPtr = NULL;

    while(1)
    {
        /* ���� Record�� Fetch: if sCurPtr == NULL, fetch first record,
           else fetch next record.*/
        IDE_TEST( smcRecord::nextOIDall( sCatTblHdr, sCurPtr, &sNxtPtr )
                  != IDE_SUCCESS );

        if( sNxtPtr == NULL )
        {
            break;
        }

        sSlotHdr = (smpSlotHeader *)sNxtPtr;
        SM_GET_SCN( &sSlotSCN, &(sSlotHdr->mCreateSCN) );

        sTableHdr = (smcTableHeader *)( sSlotHdr + 1 );

        if ( aTBSID == SC_NULL_SPACEID ||  // ��� Tablespace �˻�
             aTBSID == sTableHdr->mSpaceID ) // Ư�� Tablespace �˻�
        {
            // Action�Լ� ȣ��
            IDE_TEST( (*aActionFunc)( aStatistics,
                                      SMP_SLOT_GET_FLAGS( sSlotHdr ),
                                      sSlotSCN,
                                      sTableHdr,
                                      aActionArg )
                      != IDE_SUCCESS );
        }

        sCurPtr = sNxtPtr;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*
    Online->Offline���� ���ϴ� Tablespace�� ���� Table�鿡 ���� ó��

    Refine�ÿ� Offline�� Tablespace���� Table�� ���� ȣ���Ѵ�.

    [IN] aTBSID - Online->Online���� ���ϴ� Tablespace�� ID
 */
IDE_RC smcTableSpace::alterTBSOffline4Tables( idvSQL*     aStatistics,
                                              scSpaceID   aTBSID )
{
    sctTableSpaceNode * sSpaceNode;
    ULong               sMaxSmoNo = 0;

    // BUG-24403
    IDE_ASSERT( sctTableSpaceMgr::findSpaceNodeBySpaceID( aTBSID,
                                                          (void**)&sSpaceNode )
                == IDE_SUCCESS );


    // BUG-24403
    /* BUG-27714 TC/Server/sm4/Project2/PRJ-1548/dynmem/../suites/conc/dt_dml.
     * sql���� ���� ������ ����
     * MaxSmoNoForOffline ������ �ִ� ���� Disk TableSBS���̸�, DiskTBS��
     * ��쿡�� �� ���� ����մϴ�. */
    if( sctTableSpaceMgr::isDiskTableSpace( aTBSID ) == ID_TRUE )
    {
        IDE_TEST ( run4TablesInTBS( aStatistics,
                                    aTBSID,
                                    alterTBSOfflineAction,
                                    (void*)&sMaxSmoNo )
                   != IDE_SUCCESS );

        ((sddTableSpaceNode*)sSpaceNode)->mMaxSmoNoForOffline = sMaxSmoNo;
    }
    else
    {
        //Memory TBS
        IDE_ERROR( sctTableSpaceMgr::isMemTableSpace( aTBSID ) == ID_TRUE );

        IDE_TEST ( run4TablesInTBS( aStatistics,
                                    aTBSID,
                                    alterTBSOfflineAction,
                                    NULL )
                   != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/*
    Offline->Online���� ���ϴ� Tablespace�� ���� Table�鿡 ���� ó��

    Refine�ÿ� Offline�� Tablespace���� Table�� ���� ȣ���Ѵ�.

    [IN] aTrans - Refine�� ������ Transaction
    [IN] aTBSID - Offline->Online���� ���ϴ� Tablespace�� ID
 */
IDE_RC smcTableSpace::alterTBSOnline4Tables(idvSQL*      aStatistics,
                                            void       * aTrans,
                                            scSpaceID    aTBSID )
{
    sddTableSpaceNode      * sSpaceNode;
    smcTBSOnlineActionArgs   sTBSOnlineActionArg;

    sTBSOnlineActionArg.mTrans = aTrans;
    /* BUG-27714 TC/Server/sm4/Project2/PRJ-1548/dynmem/../suites/conc/dt_dml.
     * sql���� ���� ������ ����
     * MaxSmoNoForOffline ������ �ִ� ���� Disk TableSBS���̸�, DiskTBS��
     * ��쿡�� �� ���� ����մϴ�. */
    if( sctTableSpaceMgr::isDiskTableSpace( aTBSID ) == ID_TRUE )
    {
        // BUG-24403
        IDE_ASSERT( sctTableSpaceMgr::findSpaceNodeBySpaceID( aTBSID,
                                                              (void**)&sSpaceNode )
                    == IDE_SUCCESS );

        sTBSOnlineActionArg.mMaxSmoNo = sSpaceNode->mMaxSmoNoForOffline;
    }

    IDE_TEST ( run4TablesInTBS( aStatistics,
                                aTBSID,
                                alterTBSOnlineAction,
                                (void*)&sTBSOnlineActionArg )
               != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}



/*
    Online->Offline���� ���ϴ� Tablespace�� ���� Table�� ���� Action�Լ�

    [IN] aTableHeader - Offline���� ���ϴ� Tablespace�� ���� Table�� Header
    [IN] aActionArg   - Action�Լ� ����
 */
IDE_RC smcTableSpace::alterTBSOfflineAction(
                           idvSQL         * /* aStatistics */,
                           ULong            aSlotFlag,
                           smSCN            aSlotSCN,
                           smcTableHeader * aTableHeader,
                           void           * aActionArg )
{
    UInt             sTableType;

    IDE_DASSERT( aTableHeader != NULL );

    sTableType = SMI_GET_TABLE_TYPE( aTableHeader );

    if ( sTableType == SMI_TABLE_DISK )
    {
        IDE_TEST( alterDiskTBSOfflineAction( aSlotFlag,
                                             aSlotSCN,
                                             aTableHeader,
                                             aActionArg )
                  != IDE_SUCCESS );
    }
    else
    {
        if (sTableType == SMI_TABLE_MEMORY)
        {
            IDE_TEST( alterMemTBSOfflineAction( aTableHeader )
                      != IDE_SUCCESS );
        }

        // Temp Table�� ��� �ƹ� ó�� ����.
        // ���� : Alter Tablespace Online/Offline �Ұ���
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}



/*
    Offline->Online ���� ���ϴ� Tablespace�� ���� Table�� ���� Action�Լ�

    [IN] aTableHeader - Online���� ���ϴ� Tablespace�� ���� Table�� Header
    [IN] aActionArg   - Action�Լ� ����
 */
IDE_RC smcTableSpace::alterTBSOnlineAction(
                           idvSQL*          aStatistics,
                           ULong            aSlotFlag,
                           smSCN            aSlotSCN,
                           smcTableHeader * aTableHeader,
                           void           * aActionArg )
{
    UInt             sTableType;

    IDE_DASSERT( aTableHeader != NULL );

    sTableType = SMI_GET_TABLE_TYPE( aTableHeader );

    if( sTableType == SMI_TABLE_DISK )
    {
        IDE_TEST( alterDiskTBSOnlineAction( aStatistics,
                                            aSlotFlag,
                                            aSlotSCN,
                                            aTableHeader,
                                            aActionArg )
                  != IDE_SUCCESS );
    }
    else
    {
        if (sTableType == SMI_TABLE_MEMORY)
        {
            IDE_TEST( alterMemTBSOnlineAction( aSlotFlag,
                                               aSlotSCN,
                                               aTableHeader,
                                               aActionArg )
                      != IDE_SUCCESS );
        }


        // Temp Table�� ��� �ƹ� ó�� ����.
        // ���� : Alter Tablespace Online/Offline �Ұ���
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}



/*
    Offline->Online ���� ���ϴ� Disk Tablespace�� ����
    Table�� ���� Action�Լ�

    [ �˰��� ]
    Alloc/Init Runtime Info At Table Header
    Rebuild IndexRuntime Header

    �� �Լ��� Alter Tablespace Online���� ȣ��ȴ�.

    [IN] aTableHeader - Online���� ���ϴ� Tablespace�� ���� Table�� Header
    [IN] aActionArg   - Action�Լ� ����
 */
IDE_RC smcTableSpace::alterDiskTBSOnlineAction( idvSQL         * aStatistics,
                                                ULong            aSlotFlag,
                                                smSCN            aSlotSCN,
                                                smcTableHeader * aTableHeader,
                                                void           * aActionArg )
{
    ULong                    sMaxSmoNo;
    idBool                   sIsCheckIdxIntegrity = ID_FALSE;
    smcTBSOnlineActionArgs * sTBSOnlineActionArg;

    IDE_DASSERT( aTableHeader != NULL );
    IDE_DASSERT( aActionArg   != NULL );

    /* To fix CASE-6829 ��ũ ���̺����̽��� �¶������� �����Ҷ�����
     * �ε��� Integrity Level1�̳� Level2�� ���̰� �����Ƿ�
     * ������Ƽ�� ��Ȱ��ȭ�� �ƴϸ� Integrity üũ�� �����Ѵ�. */
    sTBSOnlineActionArg = (smcTBSOnlineActionArgs*)aActionArg;
    sMaxSmoNo = sTBSOnlineActionArg->mMaxSmoNo;

    // To fix CASE-6829
    if( smuProperty::getCheckDiskIndexIntegrity()
        != SMU_CHECK_DSKINDEX_INTEGRITY_DISABLED )
    {
        sIsCheckIdxIntegrity = ID_TRUE;
    }

    if( sIsCheckIdxIntegrity == ID_TRUE )
    {
        IDE_CALLBACK_SEND_SYM( "  [SM] [BEGIN : CHECK DISK INDEX INTEGRITY]\n" );
    }

    // BUGBUG-1548 Refine�� �ڵ带 �״�� ��������.
    // Ager�� �����ص� ���������� �����ʿ�

    /*
       1. normal table
       DROP_FALSE
       2. droped table
       DROP_TRUE
       3. create table by abort ( NTA�αױ��� ������� Abort�Ͽ� Logical Undo )
       DROP_TRUE deletebit
       4. create table by abort ( allocslot ���� �ǰ� NTA �α� ����� Physical Undo)
       DROP_FALSE deletebit

       case 1,2,3  -> initLockAndRuntimeItem
       -  1��  => ������� Table�̹Ƿ� �ʱ�ȭ�ؾ���
       -  2��, => catalog table�ÿ� drop Table pending�� ȣ���
       3��    ( drop table pending������ ���ؼ� �ʱ�ȭ �ʿ�)
       case 4      -> skip
       -  4��  => catalog table refine�� catalog table row�� ������
    */

    if (!(( ( aSlotFlag & SMP_SLOT_DROP_MASK )
            == SMP_SLOT_DROP_FALSE ) &&
          ( SM_SCN_IS_DELETED( aSlotSCN ) )))
    {
        // Offline Tablespace���� Table�� ����
        // Table Header�� mLock�� �ʱ�ȭ�� ä�� �����ȴ�.
        IDE_ASSERT( aTableHeader->mLock != NULL );

        ///////////////////////////////////////////////////////////
        // (010) Init Runtime Info At Table Header
        //        - Table�� Runtime���� �ʱ�ȭ �ǽ�
        IDE_TEST( smcTable::initRuntimeItem( aTableHeader )
                  != IDE_SUCCESS );
    }

    if( ( aSlotFlag & SMP_SLOT_DROP_MASK )
        == SMP_SLOT_DROP_TRUE )
    {
        // 2��, 3���� ���
        //  : �̹� Drop�� Table�̰ų� Create���� Abort�� Table
        //  => �ƹ��� ó�� ����
    }
    else
    {
        if( SM_SCN_IS_DELETED( aSlotSCN ) )
        {
            // 4���� ��� ->  Create���� Abort�� Table
            //  => �ƹ��� ó�� ����
        }
        else
        {
            // fix BUG-17157 [PROJ-1548] Disk Tablespace Online/Offline
            // ����� �ùٸ��� Index Runtime Header �������� ����
            // TBS�� X  ����� ȹ��� �����̸�, X ���ȹ��ÿ�
            // Dropped/Discared ������ TBS�� ���ؼ� Validation�� �ϱ�
            // ������ online�� ������ �� ����.
            // ����, TBS�� X ����� ȹ���ϰ� �ֱ� ������ online �ϴ�
            // �߿� ���̺����̽��� dropped�� �Ǵ� ��찡 ����.

            // Rebuild All Index Runtime Header
            // Online�� TableSpace�� �������� ��� Index Runtime Header��
            // rebuild �Ѵ�.
            // Offline�ÿ��� �ݴ�� �Ͽ���.

            /* fix BUG-17456
             * Disk Tablespace online���� update �߻��� index ���ѷ���
             * Online�� rebuild�� DRDB Index Header�� SmoNo�� Buffer Pool��
             * �����ϴ� Index Page���� SmoNo�� ���� ū ������ rebuild �Ѵ�. */
            IDE_TEST( smcTable::rebuildRuntimeIndexHeaders(
                                       aStatistics,
                                       aTableHeader,
                                       sMaxSmoNo ) != IDE_SUCCESS );

            if ( sIsCheckIdxIntegrity == ID_TRUE )
            {
                IDE_TEST( smcTable::verifyIndexIntegrity(
                                    aStatistics,
                                    aTableHeader,
                                    NULL /* aActionArgs */ ) != IDE_SUCCESS );
            }

            /* PROJ-1671 LOB Segment�� ���� Segment Handle�� �����ϰ�, �ʱ�ȭ�Ѵ�.*/
            IDE_TEST( smcTable::createLOBSegmentDesc( aTableHeader )
                      != IDE_SUCCESS );
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/*
    Online->Offline ���� ���ϴ� Disk Tablespace�� ����
    Table�� ���� Action�Լ�

    [IN] aTableHeader - Online���� ���ϴ� Tablespace�� ���� Table�� Header
    [IN] aActionArg   - Action�Լ� ����

    [ �˰��� ]
    Free All IndexRuntime Header Disk of TBS
    Free Runtime Item At Table Header

    [ ���� ]
     1. �� �Լ��� Alter Tablespace Offline�� Commit Pending���� ȣ��ȴ�
     2. Table�� Lock Item�� ������ �� ���� ���� :
        offline �� TBS �� Drop �� �� �ֱ� ������ ���� Table�� X �����
        ȹ���Ͽ��� �ϱ� �����̴�.
 */
IDE_RC smcTableSpace::alterDiskTBSOfflineAction(
                          ULong            aSlotFlag,
                          smSCN            aSlotSCN,
                          smcTableHeader * aTableHeader,
                          void           * aActionArg )
{
    IDE_DASSERT( aTableHeader != NULL );

    /*
       1. normal table
       DROP_FALSE
       2. droped table
       DROP_TRUE
       3. create table by abort ( NTA�αױ��� ������� Abort�Ͽ� Logical Undo )
       DROP_TRUE deletebit
       4. create table by abort ( allocslot ���� �ǰ� NTA �α� ����� Physical Undo)
       DROP_FALSE deletebit

       case 1,2,3  -> initLockAndRuntimeItem
       -  1��  => ������� Table�̹Ƿ� �ʱ�ȭ�ؾ���
       -  2��, => catalog table�ÿ� drop Table pending�� ȣ���
       3��    ( drop table pending������ ���ؼ� �ʱ�ȭ �ʿ�)
       case 4      -> skip
       -  4��  => catalog table refine�� catalog table row�� ������
    */

    if( ( aSlotFlag & SMP_SLOT_DROP_MASK )
        == SMP_SLOT_DROP_TRUE )
    {
        // 2��, 3���� ���
        //  : �̹� Drop�� Table�̰ų� Create���� Abort�� Table
        //  => �ƹ��� ó�� ����
    }
    else
    {
        if( SM_SCN_IS_DELETED( aSlotSCN ) )
        {
            // 4���� ��� ->  Create���� Abort�� Table
            //  => �ƹ��� ó�� ����
        }
        else
        {
            // fix BUG-17157 [PROJ-1548] Disk Tablespace Online/Offline
            // ����� �ùٸ��� Index Runtime Header �������� ����
            // TBS�� X  ����� ȹ��� �����̸�, X ���ȹ��ÿ�
            // Dropped/Discared ������ TBS�� ���ؼ� Validation�� �ϱ�
            // ������ offline�� ������ �� ����.
            // ����, TBS�� X ����� ȹ���ϰ� �ֱ� ������ offline �ϴ�
            // �߿� ���̺����̽��� dropped�� �Ǵ� ��찡 ����.

            // 1 ���� ���
            // (010) Free All IndexRuntime Header Disk of TBS
            // Offline TBS�� ���Ե� ���̺��� ��������
            // ��� Index Runtime Header�� Free �Ѵ�.

            /* PROJ-1671 LOB Segment�� ���� Segment Handle�� �����Ѵ�. */
            IDE_TEST( smcTable::destroyLOBSegmentDesc( aTableHeader )
                      != IDE_SUCCESS );

            // BUG-24403
            smLayerCallback::getMaxSmoNoOfAllIndexes( aTableHeader,
                                                      (ULong*)aActionArg );

            IDE_TEST( smLayerCallback::dropIndexes( aTableHeader )
                      != IDE_SUCCESS );

            ///////////////////////////////////////////////////////
            // (020) Free Runtime Item  At Table Header
            /* Table�� Mutex�� Runtime������ ���� */
            IDE_TEST( smcTable::finRuntimeItem( aTableHeader )
                    != IDE_SUCCESS );
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/*
    Offline->Online ���� ���ϴ� Memory Tablespace�� ����
    Table�� ���� Action�Լ�

    [IN] aTableHeader - Online���� ���ϴ� Tablespace�� ���� Table�� Header
    [IN] aActionArg   - Action�Լ� ����

    [ �˰��� ]
     (010) Alloc/Init Runtime Info At Table Header
            - Mutex ��ü, Free Page���� ���� �ʱ�ȭ �ǽ�
     (020) Refine Table Pages
            - ��� Page�� ���� Free Slot�� ã�Ƽ� �� Page�� �޾��ش�.
            - ���̺� ����� Runtime������ Free Page���� �������ش�.
     (030) Rebuild Indexes

    [ ���� ]
     �� �Լ��� Alter Tablespace Offline���� ȣ��ȴ�.

    [ ���� ]
     Table�� Lock Item�� �̹� �ʱ�ȭ �Ǿ� �ִ� �����̴�.
        - Alter TBS Offline���� Server ��⵿ �� ���
          => Refine�������� Lock Item�� ���� �ʱ�ȭ �Ǿ���.
        - Alter TBS Offline���� Server ��⵿ ���� ���� ���
          => Alter TBS Offline�ÿ��� Lock Item�� �������� �ʴ´�.

 */
IDE_RC smcTableSpace::alterMemTBSOnlineAction( ULong            aSlotFlag,
                                               smSCN            aSlotSCN,
                                               smcTableHeader * aTableHeader,
                                               void           * aActionArg )
{
    UInt                     sStage = 0;
    iduPtrList               sOIDList;
    void                   * sTrans;
    smcTBSOnlineActionArgs * sTBSOnlineActionArg;

    IDE_DASSERT( aTableHeader != NULL );

    sTBSOnlineActionArg = (smcTBSOnlineActionArgs*)aActionArg;
    sTrans = sTBSOnlineActionArg->mTrans;

    // BUGBUG-1548 Refine�� �ڵ带 �״�� ��������.
    // Ager�� �����ص� ���������� �����ʿ�

    /*
       1. normal table
       DROP_FALSE
       2. droped table
       DROP_TRUE
       3. create table by abort ( NTA�αױ��� ������� Abort�Ͽ� Logical Undo )
       DROP_TRUE deletebit
       4. create table by abort ( allocslot ���� �ǰ� NTA �α� ����� Physical Undo)
       DROP_FALSE deletebit

       case 1,2,3  -> initLockAndRuntimeItem
       -  1��  => ������� Table�̹Ƿ� �ʱ�ȭ�ؾ���
       -  2��, => catalog table�ÿ� drop Table pending�� ȣ���
       3��    ( drop table pending������ ���ؼ� �ʱ�ȭ �ʿ�)
       case 4      -> skip
       -  4��  => catalog table refine�� catalog table row�� ������
    */

    if (!(( ( aSlotFlag & SMP_SLOT_DROP_MASK )
            == SMP_SLOT_DROP_FALSE ) &&
          ( SM_SCN_IS_DELETED( aSlotSCN ) )))
    {
        // Offline Tablespace���� Table�� ����
        // Table Header�� mLock�� �ʱ�ȭ�� ä�� �����ȴ�.
        IDE_ASSERT( aTableHeader->mLock != NULL );

        ///////////////////////////////////////////////////////////
        // (010) Init Runtime Info At Table Header
        //        - Table�� Runtime���� �ʱ�ȭ �ǽ�
        IDE_TEST( smcTable::initRuntimeItem( aTableHeader )
                  != IDE_SUCCESS );
    }

    if( ( aSlotFlag & SMP_SLOT_DROP_MASK )
        == SMP_SLOT_DROP_TRUE )
    {
        // 2��, 3���� ���
        //  : �̹� Drop�� Table�̰ų� Create���� Abort�� Table
        //  => �ƹ��� ó�� ����
    }
    else
    {
        if( SM_SCN_IS_DELETED( aSlotSCN ) )
        {
            // 4���� ��� ->  Create���� Abort�� Table
            //  => �ƹ��� ó�� ����
        }
        else
        {
            // BUGBUG-1548 �� �κ� �ڵ� �� ���� �ʿ�. ���� �Ҿ���

            /////////////////////////////////////////////////////////////
            // (020) Refine Table Pages
            //  - ��� Page�� ���� Free Slot�� ã�Ƽ� �� Page�� �޾��ش�.
            //  - ���̺� ����� Runtime������ Free Page���� �������ش�.

            IDE_TEST( sOIDList.initialize(IDU_MEM_SM_SMM ) != IDE_SUCCESS );
            sStage = 1;


            // refinePageList���� sOIDList�� �ʿ�� �ϱ⶧���� �ѱ��
            // �׷���, ���⿡�� sOIDList�� ��������� �ʴ´�.
            // aTableType ���ڸ� 0���� �ѱ�µ�, �� ������
            // aTableType�� sOIDList�� OID�� �߰������� ���� ���Ǳ� ����
            IDE_TEST(smpFixedPageList::refinePageList(
                                           sTrans,
                                           aTableHeader->mSpaceID,
                                           0, /* aTableType */
                                           & (aTableHeader->mFixed.mMRDB) )
             != IDE_SUCCESS);

            sStage = 0;
            IDE_TEST( sOIDList.destroy() != IDE_SUCCESS );

            IDE_TEST(smpVarPageList::refinePageList( sTrans,
                                                     aTableHeader->mSpaceID,
                                                     aTableHeader->mVar.mMRDB )
                     != IDE_SUCCESS);


            ////////////////////////////////////////////////////////////
            // (030) Rebuild Indexes
            IDE_TEST( smLayerCallback::createIndexes( NULL,    /* idvSQL* */
                                                      sTrans,
                                                      aTableHeader,
                                                      ID_FALSE,/* aIsRestartRebuild */
                                                      ID_FALSE /* aIsNeedValidation */,
                                                      NULL,    /* Segment Attr */
                                                      NULL )   /* Storage Attr */
                      != IDE_SUCCESS );

        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    IDE_PUSH();
    switch( sStage )
    {
        case 1:
            IDE_ASSERT( sOIDList.destroy() == IDE_SUCCESS );
            break;
        default:
            break;
    }
    IDE_POP();


    return IDE_FAILURE;
}


/*
    Online->Offline ���� ���ϴ� Memory Tablespace�� ����
    Table�� ���� Action�Լ�

    [IN] aTableHeader - Online���� ���ϴ� Tablespace�� ���� Table�� Header
    [IN] aActionArg   - Action�Լ� ����

    [ �˰��� ]
     (010) Free All Index Memory of TBS ( �����޸��� ��� ��� ���� )
     (020) Free Runtime Item At Table Header

    [ ���� ]
     1. �� �Լ��� Alter Tablespace Offline�� Commit Pending���� ȣ��ȴ�
     2. Table�� Lock Item�� ������ �� �ִ� ���� :
          Tablespace�� X���� ����ä�� �� �Լ��� ȣ��Ǳ� ������,
          Table�� Lock Item�� �������� Transaction�� ���� �� ���� ����
          ( Tablespace�� ���� IX, IS���� ��� Table�� X, S, IX, IS�� ��´� )

 */
IDE_RC smcTableSpace::alterMemTBSOfflineAction( smcTableHeader * aTableHeader )
{
    IDE_DASSERT( aTableHeader != NULL );

    // BUGBUG-1548 drop�� Table, Create���� ������ Table�� ���� ��� �ʿ�

    ////////////////////////////////////////////////////////////////////////
    // (010) Free All Index Memory of TBS
    IDE_TEST( smLayerCallback::dropIndexes( aTableHeader )
              != IDE_SUCCESS );

    ////////////////////////////////////////////////////////////////////////
    // (020)  Free Runtime Item At Table Header
    /* Table�� Lock Item�� Runtime������ ���� */
    IDE_TEST( smcTable::finRuntimeItem( aTableHeader ) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}
