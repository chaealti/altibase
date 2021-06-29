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
 * $Id: sdcTXSegFreeList.h 82075 2018-01-17 06:39:52Z jina.kim $
 **********************************************************************/

# ifndef _O_SDC_TXSEG_FREE_LIST_H_
# define _O_SDC_TXSEG_FREE_LIST_H_ 1

# include <idu.h>
# include <sdcDef.h>
# include <sdcTSSegment.h>
# include <sdcUndoSegment.h>

class sdcTXSegFreeList;

/*
 * Ʈ����� ���׸�Ʈ ��Ʈ�� ����
 */
typedef struct sdcTXSegEntry
{
    smuList            mListNode;       // Dbl-Linked List Node

    sdcTXSegFreeList * mFreeList;       // FreeList Pointer
    sdcTXSegStatus     mStatus;         // ��Ʈ������
    UInt               mEntryIdx;       // ��Ʈ������

    smSCN              mMaxCommitSCN;   // ���� ����� Ʈ������� CSCN

    sdcTSSegment       mTSSegmt;        // TSS Segment
    sdcUndoSegment     mUDSegmt;        // Undo Segment

    sdSID              mTSSlotSID;      // TSS Slot�� SID
    sdRID              mExtRID4TSS;     // �Ҵ��� TSS �������� ExtRID

    sdRID              mFstExtRID4UDS;  // ó�� �Ҵ��� Undo �������� ExtRID
    scPageID           mFstUndoPID;     // ó�� �Ҵ��� Undo �������� PID
    scSlotNum          mFstUndoSlotNum; // ó�� �Ҵ��� Undo Record�� SlotNum

    sdRID              mLstExtRID4UDS;  // ������ �Ҵ��� Undo �������� ExtRID
    scPageID           mLstUndoPID;     // ������ �Ҵ��� Undo �������� PID
    scSlotNum          mLstUndoSlotNum; // ������ �Ҵ��� Undo Record�� SlotNum

} sdcTXSegEntry;

class sdcTXSegFreeList
{

public:

    IDE_RC initialize( sdcTXSegEntry       * aArrEntry,
                       UInt                  aEntryIdx,
                       SInt                  aFstItem,
                       SInt                  aLstItem );

    IDE_RC destroy();

    void   allocEntry( sdcTXSegEntry ** aEntry );

    // BUG-29839 ����� undo page���� ���� CTS�� ������ �� �� ����.
    // �����ϱ� ���� transaction�� Ư�� segment entry�� binding�ϴ� ��� �߰�
    void   allocEntryByEntryID( UInt             aEntryID,
                                sdcTXSegEntry ** aEntry );

    void   freeEntry( sdcTXSegEntry * aEntry,
                      idBool          aMoveToFirst );

    static void  tryAllocExpiredEntryByIdx( UInt             aEntryIdx,
                                            sdpSegType       aSegType,
                                            smSCN          * aOldestTransBSCN,
                                            sdcTXSegEntry ** aEntry );

    static void   tryAllocEntryByIdx( UInt             aEntryIdx, 
                                      sdcTXSegEntry ** aEntry );

    IDE_RC lock()   { return mMutex.lock( NULL /* idvSQL* */); }
    IDE_RC unlock() { return mMutex.unlock(); }

    inline void   initEntry4Runtime( sdcTXSegEntry    * aEntry,
                                     sdcTXSegFreeList * aFreeList );

    static inline idBool isEntryExpired( sdcTXSegEntry * aEntry,
                                         sdpSegType      aSegType,
                                         smSCN         * aCheckSCN );

public:

    SInt            mEntryCnt;       /* �� Entry ���� */
    SInt            mFreeEntryCnt;   /* Free Entry ���� */
    smuList         mBase;           /* FreeList�� Base Node */

private:

    iduMutex        mMutex;          /* FreeList�� ���ü� ���� */
};

/***********************************************************************
 *
 * Description : Ʈ����� ���׸�Ʈ ��Ʈ�� �ʱ�ȭ
 *
 * Ʈ����� ���׸�Ʈ ��Ʈ���� �ʱ�ȭ�Ѵ�.
 *
 * aEntry    - [IN] Ʈ����� ���׸�Ʈ Entry ������
 * aFreeList - [IN] Ʈ����� ���׸�Ʈ FreeList ������
 *
 ***********************************************************************/
inline void sdcTXSegFreeList::initEntry4Runtime( sdcTXSegEntry    * aEntry,
                                                 sdcTXSegFreeList * aFreeList )
{
    IDE_ASSERT( aEntry != NULL );

    aEntry->mFreeList       = aFreeList;

    aEntry->mTSSlotSID      = SD_NULL_SID;
    aEntry->mExtRID4TSS     = SD_NULL_RID;
    aEntry->mFstExtRID4UDS  = SD_NULL_RID;
    aEntry->mLstExtRID4UDS  = SD_NULL_RID;
    aEntry->mFstUndoPID     = SD_NULL_PID;
    aEntry->mFstUndoSlotNum = SC_NULL_SLOTNUM;
    aEntry->mLstUndoPID     = SD_NULL_PID;
    aEntry->mLstUndoSlotNum = SC_NULL_SLOTNUM;
}


/******************************************************************************
 *
 * Description : Steal ������ ���� Ʈ����� ���׸�Ʈ ��Ʈ�� �Ҵ� ���ɼ���
 *               Optimistics�ϰ� Ȯ��
 *
 * �� ��Ʈ���� Max CommitSCN�� aCheckSCN ���� �۴ٴ� ���� Ȯ���ϰ�,
 * Segment Size�� ExtDir ������ 2���̻��� ��쿡 �ش� ��Ʈ���� 
 * ���׸�Ʈ���� ��� Expired �Ǿ����� �����Ѵ�.
 *
 * aEntry           - [IN] Ʈ����� ���׸�Ʈ ��Ʈ�� ������
 * aSegType         - [IN] Segment Type
 * aOldestTransBSCN - [IN] Active�� Ʈ����ǵ��� ���� Statement �߿���
 *                         ���� �������� ������ Statement�� SCN
 *
 ******************************************************************************/
inline idBool sdcTXSegFreeList::isEntryExpired( 
                                         sdcTXSegEntry * aEntry,
                                         sdpSegType      aSegType,
                                         smSCN         * aOldestTransBSCN )
{
    sdpSegCCache * sSegCache;
    idBool         sResult            = ID_FALSE;
    ULong          sExtDirSizeByBytes = 0 ;

    if ( SM_SCN_IS_LT( aOldestTransBSCN, &aEntry->mMaxCommitSCN ) )
    {
        return sResult;
    }

    switch( aSegType )
    {
        case SDP_SEG_TYPE_TSS:
            sSegCache = 
                (sdpSegCCache*)aEntry->mTSSegmt.getSegHandle()->mCache;
            sExtDirSizeByBytes = 
                         ( smuProperty::getTSSegExtDescCntPerExtDir() *
                           sSegCache->mPageCntInExt * 
                           SD_PAGE_SIZE );
            break;
        
        case SDP_SEG_TYPE_UNDO:
            sSegCache = 
                (sdpSegCCache*)aEntry->mUDSegmt.getSegHandle()->mCache;
            sExtDirSizeByBytes = 
                         ( smuProperty::getUDSegExtDescCntPerExtDir() *
                           sSegCache->mPageCntInExt * 
                           SD_PAGE_SIZE );
            break;

        default:
            IDE_ASSERT( 0 );
            break;
    }

    if ( sSegCache->mSegSizeByBytes >= (sExtDirSizeByBytes* 2) ) 
    {
        sResult = ID_TRUE;
    }

    return sResult;
}


# endif
