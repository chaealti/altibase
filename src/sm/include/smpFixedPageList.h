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
 * $Id: smpFixedPageList.h 86827 2020-03-04 10:35:50Z et16 $
 **********************************************************************/

#ifndef _O_SMP_FIXED_PAGELIST_H_
#define _O_SMP_FIXED_PAGELIST_H_ 1

#include <smr.h>
#include <smpDef.h>

class smpFixedPageList
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
    
    // PageList�� refine�Ѵ�.
    static IDE_RC refinePageList( void*             aTrans,
                                  scSpaceID         aSpaceID,
                                  UInt              aTableType,
                                  smpPageListEntry* aFixedEntry );
    
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

    /* ------------------------------------------------
     * temporary table header�� �����ϱ� ���� fixed row��
     * �α�ó������ �ʰ� �Ҵ��ϴ� �Լ�
     * ----------------------------------------------*/
    static IDE_RC allocSlotForTempTableHdr( scSpaceID         aSpaceID,
                                            smOID             aTableOID,
                                            smpPageListEntry* aFixedEntry,
                                            SChar**           aRow,
                                            smSCN             aInfinite );

    // slot�� �Ҵ��Ѵ�.
    static IDE_RC allocSlot( void*             aTrans,
                             scSpaceID         aSpaceID,
                             void*             aTableInfoPtr,
                             smOID             aTableOID,
                             smpPageListEntry* aFixedEntry,
                             SChar**           aRow,
                             smSCN             aInfinite,
                             ULong             aMaxRow,
                             SInt              aOptFlag,
                             smrOPType         aOPType    = SMR_OP_SMC_FIXED_SLOT_ALLOC);
    
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

    // BUG-25611
    // PrivatePageList�� FreePage���� Table�� PageListEntry�� �����ϸ�, ScanList ���� ������.
    // �̴� ���Ŀ� ���� RefineDB�� ���� ������, ���� restart�� Undo������ �Ҹ������
    static IDE_RC resetScanList( scSpaceID          aSpaceID,
                                 smpPageListEntry  *aPageListEntry);

    // FreePageHeader�� FreeSlotList�� FreeSlot�� �߰��Ѵ�.
    static void   addFreeSlotToFreeSlotList( smpFreePageHeader* aFreePageHeader,
                                             SChar*             aRow );

    // ���� FreeSlot�� FreeSlotList�� �߰��Ѵ�.
    static IDE_RC addFreeSlotPending( void*             aTrans,
                                      scSpaceID         aSpaceID,
                                      smpPageListEntry* aFixedEntry,
                                      SChar*            aRow );

    // aCurRow���� ��ȿ�� aNxtRow�� �����Ѵ�.
    static IDE_RC nextOIDallForRefineDB( scSpaceID           aSpaceID,
                                         smpPageListEntry  * aFixedEntry,
                                         SChar             * aCurRow,
                                         SChar            ** aNxtRow,
                                         scPageID          * aNxtPID);

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


    /* BUG-31206    improve usability of DUMPCI and DUMPDDF */
    /* dumpSlotHeaderByByffer �Լ��� �̿��� boot log�� Slot�� ����Ѵ�. */
    static IDE_RC dumpSlotHeader( smpSlotHeader     * aSlotHeader );

    /* BUG-31206    improve usability of DUMPCI and DUMPDDF */
    /* Slot Header�� �����Ѵ� */
    static IDE_RC dumpSlotHeaderByBuffer( smpSlotHeader  * aSlotHeader,
                                          idBool           aDisplayTable,
                                          SChar          * aOutBuf,
                                          UInt             aOutSize );


    /* BUG-31206    improve usability of DUMPCI and DUMPDDF */
    /* dumpFixedPageByByffer �Լ��� �̿��� boot log�� FixedPage�� ����Ѵ�. */
    static IDE_RC dumpFixedPage( scSpaceID         aSpaceID,
                                 scPageID          aPageID,
                                 UInt              aSlotSize );

    /* BUG-31206    improve usability of DUMPCI and DUMPDDF */
    /* FixedPage�� �����Ѵ� */
    static IDE_RC dumpFixedPageByBuffer( UChar            * aPagePtr,
                                         UInt               aSlotSize,
                                         SChar            * aOutBuf,
                                         UInt               aOutSize );
        
private:
    
    // DB�κ��� PersPages(����Ʈ)�� �޾ƿ� aFixedEntry�� �Ŵܴ�.
    static IDE_RC allocPersPages( void*             aTrans,
                                  scSpaceID         aSpaceID,
                                  smpPageListEntry* aFixedEntry );
    
    // nextOIDall�� ���� aRow���� �ش� Page�� ã���ش�.
    static IDE_RC initForScan( scSpaceID          aSpaceID,
                               smpPageListEntry*  aFixedEntry,
                               SChar*             aRow,
                               smpPersPage**      sPage,
                               SChar**            sPtr );

    // allocSlot�ϱ����� PrivatePageList�� �˻��Ͽ� �õ�
    static IDE_RC tryForAllocSlotFromPrivatePageList(
        void              * aTrans,
        scSpaceID           aSpaceID,
        smOID               aTableOID,
        smpPageListEntry  * aFixedEntry,
        SChar            ** aRow );

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
                                     smpPageListEntry* aPageListEntry );

    // Slot�� ���� FreeSlot Ȯ��
    static IDE_RC refineSlot( void*             aTrans,
                              scSpaceID         aSpaceID,
                              UInt              aTableType,
                              smiColumn**       aArrLobColumn,
                              UInt              aLobColumnCnt,
                              smpPageListEntry* aFixedEntry,
                              SChar*            aCurRow,
                              idBool          * aRefined );

    // Slot�� Free�ؾ� �ϴ����� ���� Ȯ��
    static IDE_RC isNeedFreeSlot( smpSlotHeader    * aCurRowHeader,
                                  scSpaceID          aSpaceID,
                                  smpPageListEntry * aFixedEntry,
                                  idBool           * aIsNeedFreeSlot );
#ifdef DEBUG
    // Page���� FreeSlotList�� ������ �ùٸ��� �˻��Ѵ�.
    static inline idBool isValidFreeSlotList(
                                smpFreePageHeader* aFreePageHeader );
#endif
};

#endif /* _O_SMP_FIXED_PAGELIST_H_ */

