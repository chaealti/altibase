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
 * $Id: sdpPageList.h 86435 2019-12-13 05:59:48Z jiwon.kim $
 *
 * Description :
 *
 *  �� ������ disk table list entry�� ���� ��� �����̴�.
 *
 * # ����
 *
 *   disk Ÿ�� table�� page list entry(segment)�� �����Ѵ�.
 *
 * # ����
 *
 * # �����ڷᱸ��
 *
 * # ����
 *
 **********************************************************************/

#ifndef _O_SDP_PAGE_LIST_H_
#define _O_SDP_PAGE_LIST_H_ 1

#include <sct.h>
#include <sdr.h>
#include <sdpModule.h>
#include <sdpDef.h>
#include <sdpTableSpace.h>

class sdpPageList
{
public:

    /* page list entry �ʱ�ȭ */
    static void initializePageListEntry( sdpPageListEntry*  aPageEntry,
                                         vULong             aSlotSize,
                                         smiSegAttr         aSegAttr,
                                         smiSegStorageAttr  aSegStoAttr);

    // Page List Entry�� Runtime Item�� NULL�� �����Ѵ�.
    static IDE_RC setRuntimeNull( sdpPageListEntry* aPageEntry );

    /* runtime ���� �� mutex �ʱ�ȭ */
    static IDE_RC initEntryAtRuntime( scSpaceID          aSpaceID,
                                      smOID              aTableOID,
                                      UInt               aIndexID,
                                      sdpPageListEntry * aPageEntry,
                                      idBool             aDoInitMutex = ID_TRUE );

    /* runtime ���� �� mutex ���� */
    static IDE_RC finEntryAtRuntime( sdpPageListEntry* aPageEntry );

    static IDE_RC findPage( idvSQL            *aStatistics,
                            scSpaceID          aSpaceID,
                            sdpPageListEntry  *aPageEntry,
                            void              *aTableInfoPtr,
                            UInt               aRecSize,
                            ULong              aMaxRow,
                            idBool             aAllocNewPage,
                            idBool             aNeedSlotEntry,
                            sdrMtx            *aMtx,
                            UChar            **aPagePtr,
                            UChar             *aCTSlotIdx );

    // PROJ-1566 : insert append ���� ��, free page�� ã�� �Լ�
    static IDE_RC allocPage4AppendRec( idvSQL           * aStatistics,
                                       scSpaceID          aSpaceID,
                                       sdpPageListEntry * aPageEntry,
                                       sdpDPathSegInfo  * aDPathSegInfo,
                                       sdpPageType        aPageType,
                                       sdrMtx           * aMtx,
                                       idBool             aIsLogging,
                                       UChar           ** aAllocPagePtr );

    // PROJ-1566 : insert append ���� ��, insert�� ���� slot�� �Ҵ����ִ�
    //             �Լ�
    static IDE_RC findSlot4AppendRec( idvSQL           * aStatistics,
                                      scSpaceID          aSpaceID,
                                      sdpPageListEntry * aPageEntry,
                                      sdpDPathSegInfo  * aDPathSegInfo,
                                      UInt               aRecSize,
                                      idBool             aIsLogging,
                                      sdrMtx           * aMtx,
                                      UChar           ** aPageHdr,
                                      UChar            * aNewCTSlotIdx );

    static IDE_RC allocSlot( UChar            *aPagePtr,
                             UShort            aSlotSize,
                             UChar           **aSlotPtr,
                             sdSID            *aAllocSlotSID );

    static IDE_RC updatePageState( idvSQL            * aStatistics,
                                   scSpaceID           aSpaceID,
                                   sdpPageListEntry  * aEntry,
                                   UChar             * aPagePtr,
                                   sdrMtx            * aMtx );

    /* slot ���� */
    static IDE_RC freeSlot( idvSQL            *aStatistics,
                            scSpaceID          aTableSpaceID,
                            sdpPageListEntry  *aPageEntry,
                            UChar             *aSlotPtr,
                            scGRID             aSlotGRID,
                            sdrMtx            *aMtx );

    /* record ������ ��ȯ */
    static IDE_RC getRecordCount( idvSQL           * aStatistics,
                                  sdpPageListEntry * aPageEntry,
                                  ULong            * aRecordCount,
                                  idBool             aLockMutex = ID_TRUE);

    /* page list entry�� item size�� align��Ų slot size ��ȯ */
    static UInt getSlotSize( sdpPageListEntry* aPageEntry );

    /* page list entry�� seg desc PID ��ȯ */
    static scPageID getTableSegDescPID( sdpPageListEntry* aPageEntry );

    /* page list entry�� insert rec count ��ȯ */
    static ULong getRecCnt( sdpPageListEntry*  aPageEntry );

    /* �α뿡 �ʿ��� page list entry ���� ��ȯ */
    static void getPageListEntryInfo( void*     aPageEntry,
                                      scPageID* aSegPID );

    static UShort getSizeOfInitTrans( sdpPageListEntry * aPageEntry );

    static idBool needAllocNewPage( sdpPageListEntry *aEntry,
                                    UInt              aDataSize );

    static idBool canInsert2Page4Append( sdpPageListEntry *aEntry,
                                         UChar            *aPagePtr,
                                         UInt              aDataSize );


    /* �̸� page ���� ���� �˻� */
    static void check4FreeSlot( sdRID      aRowRID,
                                sdrMtx*    aMtx );

    // PROJ-1566
    static idBool isTablePageType ( UChar * aWritePtr );

    static IDE_RC validateMaxRow( sdpPageListEntry * aPageEntry,
                                  void             * aTableInfoPtr,
                                  ULong              aMaxRow );

    static IDE_RC getAllocPageCnt( idvSQL*         aStatistics,
                                   scSpaceID       aSpaceID,
                                   sdpSegmentDesc *aSegDesc,
                                   ULong          *aAllocPageCnt );

    static IDE_RC addPage4TempSeg( idvSQL       *aStatistics,
                                   sdpSegHandle *aSegHandle,
                                   UChar        *aPrvPagePtr,
                                   scPageID      aPrvPID,
                                   UChar        *aPagePtr,
                                   scPageID      aPID,
                                   sdrMtx       *aMtx );
private:

    inline static IDE_RC lock( idvSQL           * aStatistics,
                               sdpPageListEntry * aPageEntry )
    { return aPageEntry->mMutex->lock( aStatistics ); }

    inline static IDE_RC unlock( sdpPageListEntry * aPageEntry )
    { return aPageEntry->mMutex->unlock(); }

};


#endif // _O_MIX_PAGE_LIST_ENTRY_H_
