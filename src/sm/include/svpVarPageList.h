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
 
#ifndef _O_SVP_VAR_PAGELIST_H_
#define _O_SVP_VAR_PAGELIST_H_ 1

#include <smDef.h>
#include <smpDef.h>

struct smiValue;

class svpVarPageList
{
public:
    
    // Runtime Item�� NULL�� �����Ѵ�.
    static IDE_RC setRuntimeNull( UInt              aVarEntryCount,
                                  smpPageListEntry* aVarEntryArray );

    /* runtime ���� �� mutex �ʱ�ȭ */
    static IDE_RC initEntryAtRuntime( smOID             aTableOID,
                                      smpPageListEntry* aVarEntry,
                                      smpAllocPageListEntry* aAllocPageList );
    
    /* runtime ���� �� mutex ���� */
    static IDE_RC finEntryAtRuntime( smpPageListEntry* aVarEntry );
    
    // PageList�� �ʱ�ȭ�Ѵ�.
    static void   initializePageListEntry( smpPageListEntry* aVarEntry,
                                           smOID             aTableOID );
        
    // aVarEntry�� �����ϰ� DB�� PageList �ݳ�
    static IDE_RC freePageListToDB( void*             aTrans,
                                    scSpaceID         aSpaceID,
                                    smOID             aTableOID,
                                    smpPageListEntry* aVarEntry );
    
    // aPage�� �ʱ�ȭ�Ѵ�.
    static void   initializePage( vULong        aIdx,
                                  UInt          aPageListID,
                                  vULong        aSlotSize,
                                  vULong        aSlotCount,
                                  smOID         aTableOID,
                                  smpPersPage*  aPageID );
    
    // aOID�� �ش��ϴ� Slot�� aValue�� �����Ѵ�.
    static IDE_RC setValue( scSpaceID       aSpaceID,
                            smOID           aOID,
                            const void*     aValue,
                            UInt            aLength);

    /* aFstPieceOID�� �ش��ϴ� Row�� �о�´�.*/
    static SChar* getValue( scSpaceID       aSpaceID,
                            UInt            aBeginPos,
                            UInt            aReadLen,
                            smOID           aFstPieceOID,
                            SChar          *aBuffer );

    // Variable Slot�� �Ҵ��Ѵ�.
    static IDE_RC allocSlot( void*              aTrans,
                             scSpaceID          aSpaceID,
                             smOID              aTableOID,
                             smpPageListEntry*  aVarEntry,
                             UInt               aPieceSize,
                             smOID              aNextOID,
                             smOID*             aPieceOID,
                             SChar**            aPiecePtr);
    
    // FreeSlotList���� Slot�� �����´�.
    static void   removeSlotFromFreeSlotList(
        scSpaceID          aSpaceID,
        smpFreePageHeader* aFreePageHeader,
        smOID*             aPieceOID,
        SChar**            aPiecePtr);

    // slot�� free�Ѵ�.
    static IDE_RC freeSlot( void*             aTrans,
                            scSpaceID         aSpaceID,
                            smpPageListEntry* aVarEntry,
                            smOID             aPieceOID,
                            SChar*            aPiecePtr,
                            smpTableType      aTableType,
                            smSCN             aSCN );

    // ���� FreeSlot�� FreeSlotList�� �߰��Ѵ�.
    static IDE_RC addFreeSlotPending( void*             aTrans,
                                      scSpaceID         aSpaceID,
                                      smpPageListEntry* aVarEntry,
                                      smOID             aPieceOID,
                                      SChar*            aPiecePtr );

    // PageList�� Record ������ �����Ѵ�.
    static ULong getRecordCount( smpPageListEntry* aVarEntry );

    static inline SChar* getPieceValuePtr( void* aPiecePtr, UInt aPos )
    {
        return (SChar*)((smVCPieceHeader*)aPiecePtr + 1) + aPos;
    }

    // calcVarIdx �� ������ �ϱ� ���� AllocArray�� �����Ѵ�.
    static void initAllocArray();
    
private:
    // Varialbe Column�� ���� Slot Header�� altibase_sm.log�� �����Ѵ�
    static IDE_RC dumpVarColumnHeader( smVCPieceHeader * aVarColumn );
    
    /* FOR A4 :  system���κ��� persistent page�� �Ҵ�޴� �Լ� */
    static IDE_RC allocPersPages( void*             aTrans,
                                  scSpaceID         aSpaceID,
                                  smpPageListEntry* aVarEntry,
                                  UInt              aIdx );
    
    // nextOIDall�� ���� aRow���� �ش� Page�� ã���ش�.
    static void initForScan( scSpaceID         aSpaceID,
                             smpPageListEntry* aVarEntry,
                             smOID             aPieceOID,
                             SChar*            aPiecePtr,
                             smpPersPage**     aPage,
                             smOID*            aNxtPieceOID,
                             SChar**           aNxtPiecePtr );
    
    // allocSlot�ϱ����� PrivatePageList�� �˻��Ͽ� �õ�
    static IDE_RC tryForAllocSlotFromPrivatePageList( void*       aTrans,
                                                      scSpaceID   aSpaceID,
                                                      smOID       aTableOID,
                                                      UInt        aIdx,
                                                      smOID*      aPieceOID,
                                                      SChar**     aPiecePtr );

    // allocSlot�ϱ����� FreePageList�� FreePagePool�� �˻��Ͽ� �õ�
    static IDE_RC tryForAllocSlotFromFreePageList(
        void*             aTrans,
        scSpaceID         aSpaceID,
        smpPageListEntry* aVarEntryArray,
        UInt              aIdx,
        UInt              aPageListID,
        smOID*            aPieceOID,
        SChar**           aPiecePtr );

    // FreeSlot ������ ����Ѵ�.
    static IDE_RC setFreeSlot( void         * aTrans,
                               scPageID       aPageID,
                               smOID          aVCPieceOID,
                               SChar        * aVCPiecePtr,
                               smpTableType   aTableType );

    // FreePageHeader�� FreeSlotList�� FreeSlot�� �߰��Ѵ�.
    static void   addFreeSlotToFreeSlotList( scSpaceID aSpaceID,
                                             scPageID  aPageID,
                                             SChar*    aPiecePtr );

    // aValue�� �ش��ϴ� VarIdx���� ã�´�.
    static inline IDE_RC calcVarIdx( UInt   aLength,
                                     UInt*  aVarIdx );

    // VarPage�� VarIdx���� ���Ѵ�.
    static inline UInt   getVarIdx( void* aPagePtr );
#ifdef DEBUG
    // Page���� FreeSlotList�� ������ �ùٸ��� �˻��Ѵ�.
    static idBool isValidFreeSlotList(smpFreePageHeader* aFreePageHeader );
#endif

    static void tryForAllocPagesFromOtherPools( scSpaceID          aSpaceID,
                                                smpPageListEntry * aVarEntryArray,
                                                UInt               aDstIdx,
                                                UInt               aPageListID,
                                                idBool           * aIsPageAlloced );

    static void getPagesFromOtherPool( scSpaceID              aSpaceID,
                                       smpFreePagePoolEntry * aSrcPool,
                                       smpPageListEntry     * aDstEntry,
                                       UInt                   aDstIdx,
                                       UInt                   aPageListID, 
                                       UInt                 * aSrcPoolLockState );
};

#endif /* _O_SVP_VAR_PAGELIST_H_ */
