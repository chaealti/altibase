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
 
#ifndef _O_SVP_ALLOC_PAGELIST_H_
#define _O_SVP_ALLOC_PAGELIST_H_ 1

#include <svm.h>

class svpAllocPageList
{
public:

    // AllocPageList�� �ʱ�ȭ�Ѵ�
    static void     initializePageListEntry(
        smpAllocPageListEntry* aAllocPageList );

    // Runtime Item�� NULL�� �����Ѵ�.
    static IDE_RC setRuntimeNull(
        UInt                   aAllocPageListCount,
        smpAllocPageListEntry* aAllocPageListArray );

    // AllocPageList�� Runtime ����(Mutex) �ʱ�ȭ
    static IDE_RC   initEntryAtRuntime( smpAllocPageListEntry* aAllocPageList,
                                        smOID                  aTableOID,
                                        smpPageType            aPageType );

    // AllocPageList�� Runtime ���� ����
    static IDE_RC   finEntryAtRuntime( smpAllocPageListEntry* aAllocPageList );

    // aAllocPageHead~aAllocPageTail �� AllocPageList�� �߰�
    static IDE_RC   addPageList( scSpaceID                aSpaceID,
                                 smpAllocPageListEntry  * aAllocPageList,
                                 smpPersPage            * aAllocPageHead,
                                 smpPersPage            * aAllocPageTail,
                                 UInt                     aAllocPageCnt );

    // AllocPageList �����ؼ� DB�� �ݳ�
    static IDE_RC   freePageListToDB( void*                  aTrans,
                                      scSpaceID              aSpaceID,
                                      smpAllocPageListEntry* aAllocPageList );

    // AllocPageList���� aPageID�� �����ؼ� DB�� �ݳ�
    static IDE_RC   removePage( scSpaceID               aSpaceID,
                                smpAllocPageListEntry * aAllocPageList,
                                scPageID                aPageID );
    
    // aHeadPage~aTailPage������ PageList�� ������ �ùٸ��� �˻��Ѵ�.
    static idBool   isValidPageList( scSpaceID aSpaceID,
                                     scPageID  aHeadPage,
                                     scPageID  aTailPage,
                                     vULong    aPageCount );

    // aCurRow���� ��ȿ�� aNxtRow�� �����Ѵ�.
    static IDE_RC   nextOIDall( scSpaceID              aSpaceID,
                                smpAllocPageListEntry* aAllocPageList,
                                SChar*                 aCurRow,
                                vULong                 aSlotSize,
                                SChar**                aNxtRow );

    // ����ȭ�� aAllocPageList���� aPageID�� ���� PageID ��ȯ
    static scPageID getNextAllocPageID( scSpaceID              aSpaceID,
                                        smpAllocPageListEntry* aAllocPageList,
                                        scPageID               aPageID );
    // ����ȭ�� aAllocPageList���� aPagePtr�� ���� PageID ��ȯ
    // added for dumpci
    static scPageID getNextAllocPageID( smpAllocPageListEntry * aAllocPageList,
                                        smpPersPageHeader     * aPagePtr );

    // ����ȭ�� aAllocPageList���� aPageID�� ���� PageID ��ȯ
    static scPageID getPrevAllocPageID( scSpaceID              aSpaceID,
                                        smpAllocPageListEntry* aAllocPageList,
                                        scPageID               aPageID);

    // aAllocPageList�� ù PageID ��ȯ
    static scPageID getFirstAllocPageID( smpAllocPageListEntry* aAllocPageList );

    // aAllocPageList�� ������ PageID ��ȯ
    static scPageID getLastAllocPageID( smpAllocPageListEntry* aAllocPageList );

private:

    // nextOIDall�� ���� aRow���� �ش� Page�� ã���ش�.
    static inline void     initForScan(
        scSpaceID              aSpaceID,
        smpAllocPageListEntry* aAllocPageList,
        SChar*                 aRow,
        vULong                 aSlotSize,
        smpPersPage**          aPage,
        SChar**                aRowPtr );

};


#endif /* _O_SVP_ALLOC_PAGELIST_H_ */
