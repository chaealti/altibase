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
 
#ifndef _O_SVP_FIXED_PAGELIST_H_
#define _O_SVP_FIXED_PAGELIST_H_ 1

#include <smr.h>

class svpFixedPageList
{
public:
    
    // Runtime Item�� NULL�� �����Ѵ�.
    static IDE_RC setRuntimeNull( smpPageListEntry* aFixedEntry );

    /* FOR A4 : runtime ���� �� mutex �ʱ�ȭ �Ǵ� ���� */
    static IDE_RC initEntryAtRuntime( smOID                  aTableOID,
                                      smpPageListEntry*      aFixedEntry,
                                      smpAllocPageListEntry* aAllocPageList );

    /* runtime ���� �� mutex ���� */
    static IDE_RC finEntryAtRuntime( smpPageListEntry*  aFixedEntry );

    // PageList�� �ʱ�ȭ�Ѵ�.
    static void   initializePageListEntry( smpPageListEntry* aFixedEntry,
                                           smOID             aTableOID,
                                           vULong            aSlotSize );

    // aFixedEntry�� �����ϰ� DB�� PageList �ݳ�
    static IDE_RC freePageListToDB( void*             aTrans,
                                    scSpaceID         aSpaceID,
                                    smOID             aTableOID,
                                    smpPageListEntry* aFixedEntry );

    // aPage�� �ʱ�ȭ�Ѵ�.
    static void   initializePage( vULong       aSlotSize,
                                  vULong       aSlotCount,
                                  UInt         aPageListID,
                                  smOID        aTableOID,
                                  smpPersPage* aPage );

    // slot�� �Ҵ��Ѵ�.
    static IDE_RC allocSlot( void*             aTrans,
                             scSpaceID         aSpaceID,
                             void*             aTableInfoPtr,
                             smOID             aTableOID,
                             smpPageListEntry* aFixedEntry,
                             SChar**           aRow,
                             smSCN             aInfinite,
                             ULong             aMaxRow,
                             SInt              aOptFlag);
    
    // FreeSlotList���� Slot�� �����´�.
    static void   removeSlotFromFreeSlotList(
        smpFreePageHeader* aFreePageHeader,
        SChar**            aRow );

    // slot�� free�Ѵ�.
    static IDE_RC freeSlot ( void*              aTrans,
                             scSpaceID          aSpaceID,
                             smpPageListEntry*  aFixedEntry,
                             SChar*             aRow,
                             smpTableType       aTableType,
                             smSCN              aSCN );

    // FreeSlot ������ ����Ѵ�.
    static IDE_RC setFreeSlot( void         * aTrans,
                               scSpaceID      aSpaceID,
                               scPageID       aPageID,
                               SChar        * aRow,
                               smpTableType   aTableType );

    // FreePageHeader�� FreeSlotList�� FreeSlot�� �߰��Ѵ�.
    static void   addFreeSlotToFreeSlotList( smpFreePageHeader* aFreePageHeader,
                                             SChar*             aRow );

    // ���� FreeSlot�� FreeSlotList�� �߰��Ѵ�.
    static IDE_RC addFreeSlotPending( void*             aTrans,
                                      scSpaceID         aSpaceID,
                                      smpPageListEntry* aFixedEntry,
                                      SChar*            aRow );

    /*
     * BUG-25179 [SMM] Full Scan�� ���� �������� Scan List�� �ʿ��մϴ�.
     */
    static IDE_RC linkScanList( scSpaceID          aSpaceID,
                                scPageID           aPageID,
                                smpPageListEntry * aFixedEntry );
    
    static IDE_RC unlinkScanList( scSpaceID          aSpaceID,
                                  scPageID           aPageID,
                                  smpPageListEntry * aFixedEntry );
    
    // PageList�� Record ������ �����Ѵ�.
    static ULong getRecordCount( smpPageListEntry* aFixedEntry );

    static IDE_RC setRecordCount( smpPageListEntry* aFixedEntry,
                                  ULong             aRecordCount );

    static void setAllocatedSlot( smSCN  aInfinite,
                                  SChar *aRow );
    
    // Slot Header�� altibase_sm.log�� �����Ѵ�
    static IDE_RC dumpSlotHeader( smpSlotHeader     * aSlotHeader );

    static IDE_RC dumpFixedPage( scSpaceID         aSpaceID,
                                 scPageID          aPageID,
                                 smpPageListEntry *aFixedEntry );
    
private:
    // DB�κ��� PersPages(����Ʈ)�� �޾ƿ� aFixedEntry�� �Ŵܴ�.
    static IDE_RC allocPersPages( void*             aTrans,
                                  scSpaceID         aSpaceID,
                                  smpPageListEntry* aFixedEntry );
    
    // nextOIDall�� ���� aRow���� �ش� Page�� ã���ش�.
    static inline void initForScan( scSpaceID          aSpaceID,
                                    smpPageListEntry*  aFixedEntry,
                                    SChar*             aRow,
                                    smpPersPage**      sPage,
                                    SChar**            sPtr );
    
    // allocSlot�ϱ����� PrivatePageList�� �˻��Ͽ� �õ�
    static IDE_RC tryForAllocSlotFromPrivatePageList(
        void             * aTrans,
        scSpaceID          aSpaceID,
        smOID              aTableOID,
        smpPageListEntry * aFixedEntry,
        SChar           ** aRow );

    // allocSlot�ϱ����� FreePageList�� FreePagePool�� �˻��Ͽ� �õ�
    static IDE_RC tryForAllocSlotFromFreePageList(
        void*             aTrans,
        scSpaceID         aSpaceID,
        smpPageListEntry* aFixedEntry,
        UInt              aPageListID,
        SChar**           aRow );

    // FreeSlotList ����
    static IDE_RC buildFreeSlotList( void*             aTrans,
                                     scSpaceID         aSpaceID,
                                     UInt              aTableType,
                                     smpPageListEntry* aPageListEntry,
                                     iduPtrList*       aPtrList );

    // Slot�� ���� FreeSlot Ȯ��
    static IDE_RC refineSlot( void*             aTrans,
                              scSpaceID         aSpaceID,
                              UInt              aTableType,
                              smiColumn**       aArrLobColumn,
                              UInt              aLobColumnCnt,
                              smpPageListEntry* aFixedEntry,
                              SChar*            aCurRow,
                              idBool          * aRefined,
                              iduPtrList*       aPtrList );

#ifdef DEBUG
    // Page���� FreeSlotList�� ������ �ùٸ��� �˻��Ѵ�.
    static inline idBool isValidFreeSlotList(
                              smpFreePageHeader* aFreePageHeader );
#endif
};

#endif /* _O_SVP_FIXED_PAGELIST_H_ */

