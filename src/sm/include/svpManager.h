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
 
#ifndef _O_SVP_MANAGER_H_
#define _O_SVP_MANAGER_H_ 1

#include <smm.h>

class svpManager
{
public:

    // aPage�� PageID ��ȯ
    static scPageID getPersPageID     (void* aPage);

    // aPage�� PrevPageID ��ȯ
    static scPageID getPrevPersPageID (void* aPage);

    // aPage�� NextPageID ��ȯ
    static scPageID getNextPersPageID (void* aPage);

    // aPage�� PrevPageID ����
    static void     setPrevPersPageID (void*    aPage,
                                       scPageID aPageID);

    // aPage�� NextPageID ����
    static void     setNextPersPageID (void*     aPage,
                                       scPageID  aPageID);

    // aPage�� SelfID, PrevID, NextID ����
    static void     linkPersPage(void*     aPage,
                                 scPageID  aSelf,
                                 scPageID  aPrev,
                                 scPageID  aNext);

    // aPage�� PageType ����
    static void     initPersPageType(void* aPage);

    // PROJ-1490 : PageList ����ȭ
    //             ����ȭ�� PageList���� Head,Tail,Prev,Next�� ��ȯ
    // aPageListEntry�� ù PageID ��ȯ
    static scPageID getFirstAllocPageID(
        smpPageListEntry* aPageListEntry);

    // aPageListEntry�� ������ PageID ��ȯ
    static scPageID getLastAllocPageID(
        smpPageListEntry* aPageListEntry);

    // ����ȭ�� aPageListEntry���� aPageID�� ���� PageID ��ȯ
    static scPageID getPrevAllocPageID(scSpaceID         aSpaceID,
                                       smpPageListEntry* aPageListEntry,
                                       scPageID          aPageID);

    // ����ȭ�� aPageListEntry���� aPageID�� ���� PageID ��ȯ
    static scPageID getNextAllocPageID(scSpaceID         aSpaceID,
                                       smpPageListEntry* aPageListEntry,
                                       scPageID          aPageID);

    /*
     * BUG-25179 [SMM] Full Scan�� ���� �������� Scan List�� �ʿ��մϴ�.
     */
    static scPageID getFirstScanPageID( smpPageListEntry* aPageListEntry );
    static scPageID getLastScanPageID( smpPageListEntry* aPageListEntry );
    static scPageID getPrevScanPageID( scSpaceID         aSpaceID,
                                       scPageID          aPageID );
    static scPageID getModifySeqForScan(scSpaceID         aSpaceID,
                                        scPageID          aPageID);
    static scPageID getNextScanPageID( scSpaceID         aSpaceID,
                                       scPageID          aPageID );
    static idBool validateScanList( scSpaceID  aSpaceID,
                                    scPageID   aPageID,
                                    scPageID   aPrevPID,
                                    scPageID   aNextPID );
    
    // aAllocPageList�� PageCount, Head, Tail ���� ��ȯ
    // smrUpdate���� �ݹ����� ����ϱ� ������ aAllocPageList�� void*�� ���
    static void     getAllocPageListInfo(void*     aAllocPageList,
                                         vULong*   aPageCount,
                                         scPageID* aHeadPID,
                                         scPageID* aTailPID);

    // SMP_SLOT_HEADER_SIZE�� ����
    static UInt     getSlotSize();

    // PageList�� �޸� ��� Page ����
    static vULong   getAllocPageCount(smpPageListEntry* aPageListEntry);

    static UInt     getPersPageBodyOffset();
    static UInt     getPersPageBodySize();
};

#endif /* _O_SVP_MANAGER_H_ */
