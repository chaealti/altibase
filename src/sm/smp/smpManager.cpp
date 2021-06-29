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
 * $Id: smpManager.cpp 82075 2018-01-17 06:39:52Z jina.kim $
 **********************************************************************/

#include <idl.h>
#include <ideErrorMgr.h>
#include <idErrorCode.h>
#include <smErrorCode.h>
#include <smpDef.h>
#include <smpManager.h>
#include <smpAllocPageList.h>
#include <smpFreePageList.h>

smOID    smpManager::getTableOID(void* aPage)
{
    return ((smpPersPageHeader*)aPage)->mTableOID;
}

scPageID smpManager::getPersPageID(void* aPage)
{

    scPageID sPID;

    sPID = SMP_GET_PERS_PAGE_ID(aPage);

    return sPID ;
    
}

scPageID smpManager::getPrevPersPageID(void* aPage)
{

    return SMP_GET_PREV_PERS_PAGE_ID(aPage);

}

scPageID smpManager::getNextPersPageID(void* aPage)
{
    
    return SMP_GET_NEXT_PERS_PAGE_ID(aPage);

}

void  smpManager::setPrevPersPageID(void*     aPage,
                                    scPageID  aPageID)
{
    
    SMP_SET_PREV_PERS_PAGE_ID(aPage, aPageID);

}

void smpManager::setNextPersPageID(void*     aPage,
                                   scPageID  aPageID)
{
    
    SMP_SET_NEXT_PERS_PAGE_ID(aPage, aPageID);

}

void smpManager::linkPersPage(void*     aPage,
                              scPageID  aSelf,
                              scPageID  aPrev,
                              scPageID  aNext)
{

    SMP_SET_PERS_PAGE_ID(aPage, aSelf);
    SMP_SET_PREV_PERS_PAGE_ID(aPage, aPrev);
    SMP_SET_NEXT_PERS_PAGE_ID(aPage, aNext);
    
}

void smpManager::initPersPageType(void* aPage)
{
    
    SMP_SET_PERS_PAGE_TYPE(aPage, SMP_PAGETYPE_NONE);

}

UInt smpManager::getSlotSize()
{
    
    return SMP_SLOT_HEADER_SIZE;

}

/**********************************************************************
 * aPageListEntry�� ù PageID�� ��ȯ
 *
 * aPageListEntry�� AllocPageList�� ����ȭ�Ǿ� �ֱ� ������
 * 0��° ����Ʈ�� Head�� NULL�̴��� 1��° ����Ʈ�� Head�� NULL�� �ƴ϶��
 * ù PageID�� 1��° ����Ʈ�� Head�� �ȴ�.
 *
 * aPageListEntry : Ž���Ϸ��� PageListEntry
 **********************************************************************/

scPageID smpManager::getFirstAllocPageID(smpPageListEntry* aPageListEntry)
{
    IDE_DASSERT( aPageListEntry != NULL );

    return smpAllocPageList::getFirstAllocPageID(
        aPageListEntry->mRuntimeEntry->mAllocPageList);
}

/**********************************************************************
 * aPageListEntry�� ������ PageID�� ��ȯ
 *
 * ����ȭ�Ǿ� �ִ� AllocPageList���� NULL�� �ƴ� ���� �ڿ� �ִ� Tail��
 * aPageListEntry�� ������ PageID�� �ȴ�.
 *
 * aPageListEntry : Ž���Ϸ��� PageListEntry
 **********************************************************************/

scPageID smpManager::getLastAllocPageID(smpPageListEntry* aPageListEntry)
{
    IDE_DASSERT( aPageListEntry != NULL );

    return smpAllocPageList::getLastAllocPageID(
        aPageListEntry->mRuntimeEntry->mAllocPageList);
}

/**********************************************************************
 * aPageListEntry���� aPageID ���� PageID
 *
 * ����ȭ�Ǿ��ִ� AllocPageList���� aPageID�� Head���
 * aPageID�� ���� Page�� ���� ����Ʈ�� Tail�� �ȴ�.
 *
 * aPageListEntry : Ž���Ϸ��� PageListEntry
 * aPageID        : Ž���Ϸ��� PageID
 **********************************************************************/

scPageID smpManager::getPrevAllocPageID(scSpaceID         aSpaceID,
                                        smpPageListEntry* aPageListEntry,
                                        scPageID          aPageID)
{
    IDE_DASSERT( aPageListEntry != NULL );

    return smpAllocPageList::getPrevAllocPageID(
        aSpaceID,
        aPageListEntry->mRuntimeEntry->mAllocPageList,
        aPageID);
}

/**********************************************************************
 * aPageListEntry���� aPageID ���� PageID
 *
 * ����ȭ�Ǿ��ִ� AllocPageList���� aPageID�� Tail�̶��
 * aPageID�� ���� Page�� ���� ����Ʈ�� Head�� �ȴ�.
 * 
 * aPageListEntry : Ž���Ϸ��� PageListEntry
 * aPageID        : Ž���Ϸ��� PageID
 **********************************************************************/

scPageID smpManager::getNextAllocPageID(scSpaceID         aSpaceID,
                                        smpPageListEntry* aPageListEntry,
                                        scPageID          aPageID)
{
    IDE_DASSERT( aPageListEntry != NULL );

    return smpAllocPageList::getNextAllocPageID(
        aSpaceID,
        aPageListEntry->mRuntimeEntry->mAllocPageList,
        aPageID);
}

/**********************************************************************
 *
 * Scan List���� ù��° �������� �����Ѵ�.
 *
 * aPageListEntry : Ž���Ϸ��� PageListEntry
 * 
 **********************************************************************/
scPageID smpManager::getFirstScanPageID(smpPageListEntry* aPageListEntry)
{
    IDE_DASSERT( aPageListEntry != NULL );

    return aPageListEntry->mRuntimeEntry->mScanPageList.mHeadPageID;
}

/**********************************************************************
 *
 * Scan List���� ������ �������� �����Ѵ�.
 *
 * aPageListEntry : Ž���Ϸ��� PageListEntry
 * 
 **********************************************************************/
scPageID smpManager::getLastScanPageID(smpPageListEntry* aPageListEntry)
{
    IDE_DASSERT( aPageListEntry != NULL );

    return aPageListEntry->mRuntimeEntry->mScanPageList.mTailPageID;
}

/**********************************************************************
 * 
 * ���� �������κ��� ���� �������� �����Ѵ�.
 *
 * aSpaceID : Scan List�� ���� ���̺����̽� ���̵�
 * aPageID  : ���� ������ ���̵�
 * 
 **********************************************************************/
scPageID smpManager::getPrevScanPageID(scSpaceID         aSpaceID,
                                       scPageID          aPageID)
{
    smmPCH * sPCH;
    
    sPCH = (smmPCH*)(smmManager::getPCH( aSpaceID,
                                         aPageID ));
    IDE_DASSERT( sPCH != NULL );
    
    return idCore::acpAtomicGet32( &(sPCH->mPrvScanPID) );
}

/**********************************************************************
 * 
 * ���� �������κ��� ���� �������� �����Ѵ�.
 * 
 * aSpaceID : Scan List�� ���� ���̺����̽� ���̵�
 * aPageID  : ���� ������ ���̵�
 * 
 **********************************************************************/
scPageID smpManager::getNextScanPageID(scSpaceID         aSpaceID,
                                       scPageID          aPageID)
{
    smmPCH * sPCH;
    
    sPCH = (smmPCH*)(smmManager::getPCH( aSpaceID,
                                         aPageID ));
    IDE_DASSERT( sPCH != NULL );
    
    return idCore::acpAtomicGet32( &(sPCH->mNxtScanPID) );
}

/**********************************************************************
 * 
 * ���� �������� Modify Sequence�� �����Ѵ�.
 * 
 * aSpaceID : Scan List�� ���� ���̺����̽� ���̵�
 * aPageID  : ���� ������ ���̵�
 * 
 **********************************************************************/
scPageID smpManager::getModifySeqForScan(scSpaceID         aSpaceID,
                                         scPageID          aPageID)
{
    smmPCH * sPCH;

    sPCH = (smmPCH*)(smmManager::getPCH( aSpaceID,
                                         aPageID ));
    IDE_DASSERT( sPCH != NULL );

    return idCore::acpAtomicGet64( &(sPCH->mModifySeqForScan) );
}


/**********************************************************************
 * 
 * ���� �������� Next & Previous Link�� �Ķ���ͷ� ���� �����
 * �������� �˻��Ѵ�.
 * 
 * aSpaceID : Scan List�� ���� ���̺����̽� ���̵�
 * aPageID  : ���� ������ ���̵�
 * aPrevPID : ���� �������� ���� ������ ���̵�
 * aNextPID : ���� �������� ���� ������ ���̵�
 * 
 **********************************************************************/
idBool smpManager::validateScanList( scSpaceID  aSpaceID,
                                     scPageID   aPageID,
                                     scPageID   aPrevPID,
                                     scPageID   aNextPID )
{
    smmPCH * sPCH;
    
    sPCH = (smmPCH*)(smmManager::getPCH( aSpaceID,
                                         aPageID ));
    IDE_DASSERT( sPCH != NULL );

    if( (sPCH->mNxtScanPID == aNextPID) && (sPCH->mPrvScanPID == aPrevPID) )
    {
        return ID_TRUE;
    }
    
    return ID_FALSE;
}

/**********************************************************************
 * aAllocPageList ���� ��ȯ
 *
 * aAllocPageList : Ž���Ϸ��� AllocPageList
 * aPageCount     : aAllocPageList�� PageCount
 * aHeadPID       : aAllocPageList�� Head PID
 * aTailPID       : aAllocPageList�� Tail PID
 **********************************************************************/

void smpManager::getAllocPageListInfo(void*     aAllocPageList,
                                      vULong*   aPageCount,
                                      scPageID* aHeadPID,
                                      scPageID* aTailPID)
{
    smpAllocPageListEntry* sAllocPageList;

    IDE_DASSERT( aAllocPageList != NULL );
    IDE_DASSERT( aPageCount != NULL );
    IDE_DASSERT( aHeadPID != NULL );
    IDE_DASSERT( aTailPID != NULL );
    
    sAllocPageList = (smpAllocPageListEntry*)aAllocPageList;

    *aPageCount = sAllocPageList->mPageCount;
    *aHeadPID   = sAllocPageList->mHeadPageID;
    *aTailPID   = sAllocPageList->mTailPageID;
}

/**********************************************************************
 * PageList�� �޸� ��� AllocPage ������ ����
 *
 * aPageListEntry : Ž���Ϸ��� PageListEntry
 **********************************************************************/
vULong smpManager::getAllocPageCount(smpPageListEntry* aPageListEntry)
{
    UInt   sPageListID;
    vULong sPageCount = 0;

    IDE_DASSERT( aPageListEntry != NULL );
    
    for( sPageListID = 0;
         sPageListID < SMP_PAGE_LIST_COUNT;
         sPageListID++ )
    {
        sPageCount += aPageListEntry->mRuntimeEntry->mAllocPageList[sPageListID].mPageCount;
    }

    return sPageCount;
}

UInt smpManager::getPersPageBodyOffset()
{
    return SMP_PERS_PAGE_BODY_OFFSET ;
}

UInt smpManager::getPersPageBodySize()
{
    return SMP_PERS_PAGE_BODY_SIZE ;
}

/* PROJ-2162 RestartRiskReduction */
IDE_RC smpManager::setPersPageInconsistency( scSpaceID   aSpaceID,
                                             scPageID    aPageID )
{
    smpPersPageHeader * sMemPagePtr;

    IDE_TEST( smmManager::getPersPagePtr( aSpaceID, 
                                          aPageID,
                                          (void**)&sMemPagePtr )
              != IDE_SUCCESS );

    /* �߸��� �������� ���� ������ ����� */
    ideLog::logMem( IDE_DUMP_0,
                    (UChar*)sMemPagePtr,
                    SM_PAGE_SIZE,
                    "MRDB PAGE:");

    IDE_TEST( smrUpdate::setPersPageInconsistency( aSpaceID,
                                                   aPageID,
                                                   sMemPagePtr->mType )
              != IDE_SUCCESS );

    SMP_SET_PERS_PAGE_TYPE( sMemPagePtr,
                            SMP_GET_PERS_PAGE_TYPE( sMemPagePtr ) |
                            SMP_PAGEINCONSISTENCY_TRUE );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

