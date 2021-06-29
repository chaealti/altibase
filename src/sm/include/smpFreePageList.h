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
 * $Id: smpFreePageList.h 85970 2019-08-01 05:57:45Z justin.kwon $
 **********************************************************************/

#ifndef _O_SMP_FREE_PAGELIST_H_
#define _O_SMP_FREE_PAGELIST_H_ 1

#include <smpDef.h>

class smpFreePageList
{
public:
    // Runtime Item�� NULL�� �����Ѵ�.
    static IDE_RC setRuntimeNull( smpPageListEntry* aPageListEntry );

    // PageListEntry���� FreePage�� ���õ� RuntimeEntry�� ���� �ʱ�ȭ
    static IDE_RC initEntryAtRuntime( smpPageListEntry* aPageListEntry );

    // RuntimeEntry ����
    static IDE_RC finEntryAtRuntime( smpPageListEntry* aPageListEntry );

    // PageListEntry���� RuntimeEntry ���� ����
    static void   initializeFreePageListAndPool(
                                            smpPageListEntry* aPageListEntry );

    // FreePageList ����
    static void   distributePagesFromFreePageList0ToTheOthers(
                                            smpPageListEntry* aPageListEntry );

    // FreePagePool ����
    static IDE_RC distributePagesFromFreePageList0ToFreePagePool(
                                            void*             aTrans,
                                            scSpaceID         aSpaceID,
                                            smpPageListEntry* aPageListEntry );

    // FreePagePool���� FreePage�� �Ҵ���� �� �ִ��� �õ�
    static IDE_RC tryForAllocPagesFromPool( smpPageListEntry* aPageListEntry,
                                            UInt              aPageListID,
                                            idBool*           aIsPageAlloced );
    
    // FreePageList�� FreePage �Ҵ�
    // FreePagePool -> FreePageList
    static IDE_RC getPagesFromFreePagePool( smpPageListEntry* aPageListEntry,
                                            UInt              aPageListID );

    // FreePagePool�� FreePage �߰�
    static IDE_RC addPageToFreePagePool( smpPageListEntry*  aPageListEntry,
                                         smpFreePageHeader* aFreePageHeader );

    // FreePage���� PageListEntry���� ����
    static IDE_RC freePagesFromFreePagePoolToDB(
                                            void             * aTrans,
                                            scSpaceID          aSpaceID,
                                            smpPageListEntry * aPageListEntry,
                                            UInt               aPages );

    // FreePageHeader ���� Ȥ�� DB���� ������ �Ҵ�� �ʱ�ȭ
    static void   initializeFreePageHeader(
                                         smpFreePageHeader  * aFreePageHeader );

    // PageListEntry�� ��� FreePageHeader �ʱ�ȭ
    static void   initAllFreePageHeader( scSpaceID         aSpaceID,
                                         smpPageListEntry* aPageListEntry );
    
    // FreePageHeader �ʱ�ȭ
    // smmManager���� PCH�ʱ�ȭ�� Callback���� ȣ��
    static IDE_RC initializeFreePageHeaderAtPCH( scSpaceID aSpaceID,
                                                 scPageID  aPageID );

    // FreePageHeader ����
    static IDE_RC destroyFreePageHeaderAtPCH( scSpaceID aSpaceID,
                                              scPageID  aPageID );

    // FreePage�� ���� SizeClass ����
    static IDE_RC modifyPageSizeClass( void*              aTrans,
                                       smpPageListEntry*  aPageListEntry,
                                       smpFreePageHeader* aFreePageHeader );

    // FreePage�� FreePageList���� ����
    static IDE_RC removePageFromFreePageList(
                                        smpPageListEntry  *  aPageListEntry,
                                        UInt                 aPageListID,
                                        UInt                 aSizeClassID,
                                        smpFreePageHeader  * aFreePageHeader );

    // FreePage�� FreePageList�� �߰�
    static IDE_RC addPageToFreePageListTail(
                                        smpPageListEntry  *  aPageListEntry,
                                        UInt                 aPageListID,
                                        UInt                 aSizeClassID,
                                        smpFreePageHeader  * aFreePageHeader );

    // refineDB�� FreePage�� FreePageList�� �߰�
    static IDE_RC addPageToFreePageListAtInit(
                                        smpPageListEntry   * aPageListEntry,
                                        smpFreePageHeader  * aFreePageHeader );

    // PrivatePageList�� FreePage���� Table�� PageListEntry�� ����
    static IDE_RC addFreePagesToTable( void*              aTrans,
                                       smpPageListEntry*  aPageListEntry,
                                       smpFreePageHeader* aFreePageHead );

    // FreeSlotList �ʱ�ȭ
    static void   initializeFreeSlotListAtPage(
                                        scSpaceID            aSpaceID,
                                        smpPageListEntry   * aPageListEntry,
                                        smpPersPage        * aPagePtr );

    // PCH������ �ִ� aPage�� FreePageHeader�� �����Ѵ�.
    static inline smpFreePageHeader* getFreePageHeader( scSpaceID    aSpaceID,
                                                        smpPersPage* aPage );

    // PCH������ �ִ� aPageID�� FreePageHeader�� �����Ѵ�.
    static inline smpFreePageHeader* getFreePageHeader( scSpaceID aSpaceID,
                                                        scPageID  aPageID );

    // PrivatePageList�� FreePage ����
    static void   addFreePageToPrivatePageList( scSpaceID aSpaceID,
                                                scPageID  aCurPID,
                                                scPageID  aPrevPID,
                                                scPageID  aNextPID );

    // PrivatePageList���� FreePage����
    static void   removeFixedFreePageFromPrivatePageList(
                                    smpPrivatePageListEntry  * aPrivatePageList,
                                    smpFreePageHeader        * aFreePageHeader );

#ifdef DEBUG    
    /* aHeadFreePage~aTailFreePage������ FreePageList�� ������ �ùٸ��� �˻��Ѵ�. */
    static idBool isValidFreePageList( smpFreePageHeader* aHeadFreePage,
                                       smpFreePageHeader* aTailFreePage,
                                       vULong             aPageCount );
#endif

private:

    // SizeClass�� ����Ѵ�.
    static inline UInt   getSizeClass( smpRuntimeEntry * aEntry,
                                       UInt              aTotalSlotCnt,
                                       UInt              aFreeSlotCnt );
};

/**********************************************************************
 * Page�� SizeClass�� ����Ѵ�.
 *
 * aTotalSlotCnt : Page�� ��ü Slot ����
 * aFreeSlotCnt  : Page�� FreeSlot ����
 **********************************************************************/

UInt smpFreePageList::getSizeClass( smpRuntimeEntry * aEntry,
                                    UInt              aTotalSlotCnt,
                                    UInt              aFreeSlotCnt )
{
    // aFreeSlotCnt
    // ------------- * (SMP_LAST_SIZECLASSID)
    // aTotalSlotCnt
    
    return (aFreeSlotCnt * SMP_LAST_SIZECLASSID(aEntry))/ aTotalSlotCnt;
}

/**********************************************************************
 * PCH������ �ִ� aPageID�� FreePageHeader�� �����Ѵ�.
 *
 * aPageID : FreePageHeader�� �ʿ��� PageID
 **********************************************************************/

smpFreePageHeader* smpFreePageList::getFreePageHeader( scSpaceID aSpaceID,
                                                       scPageID  aPageID )
{
    smmPCH* sPCH;

    // BUGBUG : aPageID�� ���� DASSERT �ʿ�

    sPCH = (smmPCH*)(smmManager::getPCH( aSpaceID,
                                         aPageID ));

    IDE_DASSERT( sPCH != NULL );

    return (smpFreePageHeader*)(sPCH->mFreePageHeader);
}

/**********************************************************************
 * PCH������ �ִ� aPage�� FreePageHeader�� �����Ѵ�.
 *
 * aPage : FreePageHeader�� �ʿ��� Page
 **********************************************************************/

smpFreePageHeader* smpFreePageList::getFreePageHeader( scSpaceID    aSpaceID,
                                                       smpPersPage* aPage )
{
    IDE_DASSERT( aPage != NULL );
    
    return getFreePageHeader( aSpaceID, aPage->mHeader.mSelfPageID );
}

#endif /* _O_SMP_FREE_PAGELIST_H_ */
