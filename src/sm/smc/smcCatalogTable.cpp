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
 * $Id: smcCatalogTable.cpp 90259 2021-03-19 01:22:22Z emlee $
 **********************************************************************/

#include <idl.h>
#include <ide.h>
#include <idu.h>
#include <ideErrorMgr.h>
#include <smErrorCode.h>
#include <smm.h>
#include <smp.h>
#include <smcDef.h>
#include <smcCatalogTable.h>
#include <smcRecord.h>
#include <smi.h>
#include <smcReq.h>

UInt  smcCatalogTable::getCatTempTableOffset()
{

    return SMC_CAT_TEMPTABLE_OFFSET;

}

/***********************************************************************
 * Description : DB �����ÿ� Catalog Table�� Temp Catalog Table ����
 *
 **********************************************************************/
IDE_RC smcCatalogTable::createCatalogTable()
{
    IDE_TEST( createCatalog(SMC_CAT_TABLE, SMM_CAT_TABLE_OFFSET)
              != IDE_SUCCESS );

    IDE_TEST( createCatalog(SMC_CAT_TEMPTABLE, SMC_CAT_TEMPTABLE_OFFSET)
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : Server shutdown�� Catalog Table�� ���� �޸� ���� ����
 *
 * Catalog Table�� Temp Catalog Table�� Record�� Temp Table
 * Header�� Lock Item�� RunTime ������ �����Ѵ�.
 **********************************************************************/
IDE_RC smcCatalogTable::finalizeCatalogTable()
{
    /* Catalog Table�� Record���� Lock Item�� RunTime���� ����*/
    IDE_TEST( finCatalog( SMC_CAT_TABLE ) != IDE_SUCCESS );
    /* Temp Catalog Table�� Record���� Lock Item�� RunTime���� ����*/
    IDE_TEST( finCatalog( SMC_CAT_TEMPTABLE ) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : �������̽��� ���߱� ���� Dummy�Լ�
 **********************************************************************/
IDE_RC smcCatalogTable::initialize()
{
    return IDE_SUCCESS;
}

/***********************************************************************
 * Description : �������̽��� ���߱� ���� Dummy�Լ�
 **********************************************************************/
IDE_RC smcCatalogTable::destroy()
{
    return IDE_SUCCESS;
}

/***********************************************************************
 * Description : Shutdown�� Catalog Table�� �׾���
 *               ��� Table�� Runtime���� ����
 *
 * aCatTableHeader - [IN] Catalog Table Header (Normal or Temp)
 *
 **********************************************************************/
IDE_RC smcCatalogTable::finCatalog( void* aCatTableHeader )
{
    IDE_ASSERT(aCatTableHeader != NULL);

    smcTableHeader * sCatTblHdr;

    sCatTblHdr = (smcTableHeader*) aCatTableHeader;

    /* Catalog Table���� Used Slot�鿡 ���� Runtime���� ���� */
    IDE_TEST( finAllocedTableSlots( sCatTblHdr )
              != IDE_SUCCESS );

    /* aCatTableHeader�� Lock Item�� Runtime������ ���� */
    IDE_TEST( smcTable::finLockAndRuntimeItem( sCatTblHdr )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/***********************************************************************
 * Description : Catalog Table�� �����ϱ� ���� Catalog Tabler Header�� �ʱ�
 *               ȭ�Ѵ�.
 *
 * aCatTableHeader - [IN] Catalog Table Header Ponter
 * aOffset         - [IN] Offset: Catalog Table Header�� 0��° Page�� ��ġ�ϴ�
 *                        �� �� Offset�� �� 0�� �������� ������ġ������ offset�̴�.
 **********************************************************************/
IDE_RC smcCatalogTable::createCatalog( void*   aCatTableHeader,
                                       UShort  aOffset )
{
    smcTableHeader  * sCatTblHdr;
    smpSlotHeader   * sSlotHeader;
    UInt i;


    /* Catalog Table�� DB �����ÿ� �ʱ�ȭ�Ѵ�. */
    sCatTblHdr = (smcTableHeader*)aCatTableHeader;

    /* īŻ�α� ���̺��� Page��踦 ������� �ʴ��� üũ */
    IDE_ASSERT( aOffset + ID_SIZEOF(smpSlotHeader) + ID_SIZEOF(smcTableHeader)
                <= SM_PAGE_SIZE );
    /* aCatTableHeader�� Slot Header�� �ʱ�ȭ �Ѵ�.*/
    idlOS::memset((UChar *)sCatTblHdr - SMP_SLOT_HEADER_SIZE, 0,
                  ID_SIZEOF(smcTableHeader) + SMP_SLOT_HEADER_SIZE);

    /* m_self of record header contains offset from page header */
    sSlotHeader = (smpSlotHeader *)((UChar *)sCatTblHdr - SMP_SLOT_HEADER_SIZE);
    SMP_SLOT_SET_OFFSET( sSlotHeader, aOffset );

    for(i = 0 ; i < SMC_MAX_INDEX_OID_CNT; i++)
    {
        sCatTblHdr->mIndexes[i].length = 0;
        sCatTblHdr->mIndexes[i].fstPieceOID = SM_NULL_OID;
        sCatTblHdr->mIndexes[i].flag = SM_VCDESC_MODE_OUT;
    }

    /* etc members in catalog table header */
    sCatTblHdr->mType          = SMC_TABLE_CATALOG;
    sCatTblHdr->mSelfOID       = (SM_NULL_PID | (aOffset));
    sCatTblHdr->mFlag          = SMI_TABLE_REPLICATION_DISABLE
        | SMI_TABLE_LOCK_ESCALATION_DISABLE | SMI_TABLE_META ;

    /* Catalog Table�� ���� Į�� ������ variable slot������ �Ҵ��Ѵ�.*/
    sCatTblHdr->mColumnSize      = 0;
    sCatTblHdr->mColumnCount     = 0;
    sCatTblHdr->mColumns.length  = 0;
    sCatTblHdr->mColumns.fstPieceOID = SM_NULL_OID;
    sCatTblHdr->mMaxRow              = ID_ULONG_MAX;

    sCatTblHdr->mNullOID = SM_NULL_OID;

    /* Catalog Table�� Page List�� �ʱ�ȭ �Ѵ�. */
    smpFixedPageList::initializePageListEntry(
                       &sCatTblHdr->mFixed.mMRDB,
                       sCatTblHdr->mSelfOID,
                       idlOS::align8( (UInt)( ID_SIZEOF(smcTableHeader) + SMP_SLOT_HEADER_SIZE) ) );

    smpVarPageList::initializePageListEntry( sCatTblHdr->mVar.mMRDB,
                                             sCatTblHdr->mSelfOID);

    smpAllocPageList::initializePageListEntry( sCatTblHdr->mFixedAllocList.mMRDB );
    smpAllocPageList::initializePageListEntry( sCatTblHdr->mVarAllocList.mMRDB );

    IDE_TEST(smmDirtyPageMgr::insDirtyPage(SMI_ID_TABLESPACE_SYSTEM_MEMORY_DIC,
                                           SMM_MEMBASE_PAGEID)
             != IDE_SUCCESS);

    IDE_TEST( smcTable::initLockAndRuntimeItem( sCatTblHdr ) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    IDE_PUSH();
    IDE_ASSERT(smmDirtyPageMgr::insDirtyPage(SMI_ID_TABLESPACE_SYSTEM_MEMORY_DIC, 
                                             SMM_MEMBASE_PAGEID)
               == IDE_SUCCESS);
    IDE_POP();

    return IDE_FAILURE;
}

/**
 *  Catalog Table���� Used Slot�� ���� Lock Item�� Runtime Item����
 *
 *  ��� ���̺� :
 *    Drop���� ���� Table�̰ų�, Create Table���� Abort�Ͽ�
 *    Drop Flag�� ���õ� ���̺�
 *
 *  aCatTableHeader�� ����Ű�� Table�� Record���� Table Header���̴�.
 *  �� Table Header���� Server Start�� �Ҵ�Ǵ� Lock Item�� RunTime��
 *  ���� �ִ�. ������ Server Stop�� �� ������ Free�ϱ� ����
 *  aCatTableHeader�� ��� Create�� Table�� ���ؼ�
 *  smcTable::finLockAndRuntimeItem�� �����Ѵ�.
 *
 * aCatTableHeader - [IN] Catalog Table Header (Normal or Temp)
 */
IDE_RC smcCatalogTable::finAllocedTableSlots( smcTableHeader * aCatTblHdr )
{
    IDE_ASSERT(aCatTblHdr != NULL);

    smcTableHeader * sHeader;
    smpSlotHeader  * sPtr;
    SChar          * sCurPtr;
    SChar          * sNxtPtr;
    smSCN            sSCN;

    sCurPtr = NULL;

    while(1)
    {
        /* ���� Record�� Fetch: if sCurPtr == NULL, fetch first record,
           else fetch next record.*/
        IDE_TEST( smcRecord::nextOIDall( aCatTblHdr, sCurPtr, &sNxtPtr )
                  != IDE_SUCCESS );

        if( sNxtPtr == NULL )
        {
            break;
        }
        sPtr = (smpSlotHeader *)sNxtPtr;
        SM_GET_SCN( &sSCN, &(sPtr->mCreateSCN) );

        sHeader = (smcTableHeader *)( sPtr + 1 );

        /* BUG-15653: server stop�� mutex leak�� ���� �߻���.
           �޸� ���̺��� Drop Pending�ÿ� finLockAndRuntimeItem�� ȣ������ �ʾ���.
           ��ũ�� �����ϰ� DB�� �������� üũ�ؼ� Lock, Mutex�� Free����*/
        if( SM_SCN_IS_NOT_DELETED( sSCN ) ||
            SMP_SLOT_IS_DROP( sPtr ) )
        {
            /* Disk LOB Column�� ���� Segment Handle �����Ѵ�. */
            if( SMI_TABLE_TYPE_IS_DISK( sHeader ) == ID_TRUE )
            {
                IDE_TEST( smcTable::destroyLOBSegmentDesc( sHeader )
                          != IDE_SUCCESS );

                IDE_TEST( smcTable::destroyRowTemplate( sHeader )!= IDE_SUCCESS );
            }

            /* Table�� Lock Item�� Runtime������ ���� */
            IDE_TEST( smcTable::finLockAndRuntimeItem( sHeader )
                      != IDE_SUCCESS );

        }

        sCurPtr = sNxtPtr;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/***********************************************************************
 * Description : DRDB(Disk Resident Database)�� Restart Recovery�� Undo�ÿ�
 *               Index Header�� �����Ѵ�. ������ Redo�� MMDB�� �ִ� Catalog
 *               Table�� Record�� �� Disk Table�� Header�� ���� Lock Item��
 *               Runtime������ �ʱ�ȭ�ϰ� Index�鿡 ���� ������ ������ �� �ֵ���
 *               ������ ������ �����ؾ� �Ѵ�.
 *
 *
 **********************************************************************/
IDE_RC smcCatalogTable::refineDRDBTables()
{
    idBool    sInitLockAndRuntimeItem = ID_TRUE;

    /* A. ��ũ �ε����� ��Ÿ�� ����� ��ũ ���̺��� LOB ���׸�Ʈ��
     *    ����ڸ� �ʱ�ȭ�Ѵ�. */
    IDE_TEST( doAction4EachTBL( NULL, /* aStatistics */
                                smcTable::initRuntimeInfos,
                                (void*)&sInitLockAndRuntimeItem )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/***********************************************************************
 *
 * Description : ������ Table�� ���� Ư�� Action�� �����Ѵ�.
 *
 *   aStatistics - [IN] �������
 *   aAction     - [IN] ������ Action�Լ�
 *   aActionArg  - [IN] Action�Լ��� ���� Argument
 *
 **********************************************************************/
IDE_RC smcCatalogTable::doAction4EachTBL(idvSQL            * aStatistics,
                                         smcAction4TBL       aAction,
                                         void              * aActionArg )
{
    smcTableHeader * sCatTblHdr;
    smcTableHeader * sHeader;
    smpSlotHeader  * sPtr;
    SChar          * sCurPtr;
    SChar          * sNxtPtr;
    smSCN            sSCN;

    sCatTblHdr = (smcTableHeader*)SMC_CAT_TABLE;
    sCurPtr    = NULL;

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
        sPtr = (smpSlotHeader *)sNxtPtr;
        SM_GET_SCN( &sSCN, &(sPtr->mCreateSCN) );

/*
        BUG-13936

        Table�� ���� ���߿� ������ ��� ������ ���� ó���� ���� Slot�� �����ȴ�.

        Create Table
          Catalog Table���� Row�Ҵ�
             ==> Drop=0(Not Dropped), Used='U'(Used), Delete=0
             (����) Create Table���� ����? Abort����!
                 ==> Delete Bit=1       .... (��)
                     Ager�� Slot����
                          ==> Used='F'  .... (��)

        ���� Ager�� �����Ͽ� (��)�� ���±��� ���ٸ� Used='F'�̹Ƿ� nextOIDAll
        �� ���� �ɷ��� ���̴�.

        �׷���, Ager�� ���� ������� �ʾƼ� (��)�� ���¿� �ִٸ�,
        Table�� ���� ���߿� ������ ����̹Ƿ� refine��󿡼� �����ؾ��Ѵ�.

        �����, Drop Table���� Flag��ȭ�� ������ ����.

        Drop Table
          ==> Drop = 1(Dropped)
          (����) Commit�Ѵٸ�?
               ==> Delete Bit=1
                   Ager�� �̸� ������ �ʴ´�.(Prepare�� Tx�� �����ϰ� ���� �� �����Ƿ�)
                   Shutdown�� Startup�� Refine���߿� Catalog Table ���� row����
                   => Used='F'
*/
        // 1. Table�� ���� ���߿� ������ ��� refine��󿡼� �����ؾ��Ѵ�.
        // 2. sPtr->mDropFlag�� TRUE�̴��� Rollback���� FALSE�� �ɼ� �ִ�.
        if ( SMP_SLOT_IS_NOT_DROP( sPtr ) &&
             SM_SCN_IS_DELETED( sSCN ) )
        {
            sCurPtr = sNxtPtr;
            continue;
        }

        sHeader = (smcTableHeader *)( sPtr + 1 );

        IDE_TEST( (*aAction)( aStatistics,
                              sHeader,
                              aActionArg ) != IDE_SUCCESS );

        sCurPtr = sNxtPtr;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}



