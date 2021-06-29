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
 
#include <idl.h>
#include <ide.h>
#include <smErrorCode.h>
#include <svm.h>
#include <svpVarPageList.h>
#include <svpAllocPageList.h>
#include <svpFreePageList.h>
#include <svpReq.h>

/* BUG-26939 
 * �� �迭�� ������ ���� ����ü�� ũ�� �������� ���� �޶��� �� �ִ�.
 *      SMP_PERS_PAGE_BODY_SIZE, smpVarPageHeader, smVCPieceHeader.
 * ���� ������ bit �� ���� ����ü ũ�Ⱑ �޶����Ƿ� �ٸ��� �����Ѵ�.
 * debug mode���� �� ũ�Ⱑ ������� �ʴ��� initializePageListEntry���� �˻��Ѵ�.
 * �� ��Ʈ�� �����ȴٸ� svp�� �Ȱ��� �ڵ尡 �����Ƿ� �����ϰ� �����ؾ��Ѵ�.
 * ���� �Ʒ��� ���� ������ �ڼ��� ������ NOK Variable Page slots ���� ����
 */
#if defined(COMPILE_64BIT)
static UInt gVarSlotSizeArray[SM_VAR_PAGE_LIST_COUNT]=
{
    40, 64, 104, 152, 232, 320, 488, 664, 1000, 1344,
    2024, 2704, 4072, 5432, 8160, 10888, 16344, 32712
};
#else
static UInt gVarSlotSizeArray[SM_VAR_PAGE_LIST_COUNT]=
{
    48, 72, 112, 160, 240, 328, 496, 672, 1008, 1352,
    2032, 2720, 4080, 5448, 8176, 10904, 16360, 32728
};
#endif
/* size ���� �´� index�� ��Ī�����ִ� �迭 */
static UChar gAllocArray[SMP_PERS_PAGE_BODY_SIZE] = {0, };

/***********************************************************************
 * Runtime Item�� NULL�� �����Ѵ�.
 * DISCARD/OFFLINE Tablespace�� ���� Table�鿡 ���� ����ȴ�.
 *
 * [IN] aVarEntryCount : �ʱ�ȭ�Ϸ��� PageListEntry�� ��
 * [IN] aVarEntryArray : �ʱ�ȭ�Ϸ��� PageListEntry���� Array
 ***********************************************************************/
IDE_RC svpVarPageList::setRuntimeNull( UInt              aVarEntryCount,
                                       smpPageListEntry* aVarEntryArray )
{
    UInt i;

    IDE_DASSERT( aVarEntryArray != NULL );

    for ( i=0; i<aVarEntryCount; i++)
    {
        // RuntimeEntry �ʱ�ȭ
        IDE_TEST(svpFreePageList::setRuntimeNull( & aVarEntryArray[i] )
                 != IDE_SUCCESS);
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**********************************************************************
 * memory table��  variable page list entry�� �����ϴ� runtime ���� �ʱ�ȭ
 *
 * aTableOID : PageListEntry�� ���ϴ� ���̺� OID
 * aVarEntry : �ʱ�ȭ�Ϸ��� PageListEntry
 **********************************************************************/
IDE_RC svpVarPageList::initEntryAtRuntime(
    smOID                  aTableOID,
    smpPageListEntry*      aVarEntry,
    smpAllocPageListEntry* aAllocPageList )
{
    UInt i;

    IDE_DASSERT( aVarEntry != NULL );
    IDE_ASSERT( aTableOID == aVarEntry->mTableOID );
    IDE_DASSERT( aAllocPageList != NULL );

    for( i = 0; i < SM_VAR_PAGE_LIST_COUNT ; i++ )
    {
        // RuntimeEntry �ʱ�ȭ
        IDE_TEST(svpFreePageList::initEntryAtRuntime( &(aVarEntry[i]) )
                 != IDE_SUCCESS);

        aVarEntry[i].mRuntimeEntry->mAllocPageList = aAllocPageList;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**********************************************************************
 * memory table��  variable page list entry�� �����ϴ� runtime ���� ����
 *
 * aVarEntry : �����Ϸ��� PageListEntry
 **********************************************************************/
IDE_RC svpVarPageList::finEntryAtRuntime( smpPageListEntry* aVarEntry )
{
    UInt i;
    UInt sPageListID;

    IDE_DASSERT( aVarEntry != NULL );

    if (aVarEntry->mRuntimeEntry != NULL)
    {
        for( sPageListID = 0;
             sPageListID < SMP_PAGE_LIST_COUNT;
             sPageListID++ )
        {
            // AllocPageList�� Mutex ����
            IDE_TEST( svpAllocPageList::finEntryAtRuntime(
                          &(aVarEntry->mRuntimeEntry->
                               mAllocPageList[sPageListID]) )
                      != IDE_SUCCESS );
        }

        for( i = 0; i < SM_VAR_PAGE_LIST_COUNT; i++ )
        {
            // RuntimeEntry ����
            IDE_TEST(svpFreePageList::finEntryAtRuntime(&(aVarEntry[i]))
                     != IDE_SUCCESS);
        }
    }
    else
    {
        /* Memory Table�� ��쿣 aVarEntry->mRuntimeEntry�� NULL�� ���
           (OFFLINE/DISCARD)�� ������ Volatile Table�� ���ؼ���
           aVarEntry->mRuntimeEntry�� �̹� NULL�� ���� ����.
           ���� aVarEntry->mRuntimeEntry�� NULL�̶��
           ��򰡿� ���װ� �ִٴ� �ǹ��̴�. */

        IDE_DASSERT(0);
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * PageListEntry�� ������ �����ϰ� DB�� �ݳ��Ѵ�.
 *
 * aTrans    : �۾��� �����ϴ� Ʈ����� ��ü
 * aTableOID : ������ ���̺� OID
 * aVarEntry : ������ PageListEntry
 ***********************************************************************/
IDE_RC svpVarPageList::freePageListToDB(void*             aTrans,
                                        scSpaceID         aSpaceID,
                                        smOID             aTableOID,
                                        smpPageListEntry* aVarEntry )
{
    UInt i;
    UInt sPageListID;

    IDE_DASSERT( aTrans != NULL );
    IDE_DASSERT( aVarEntry != NULL );
    IDE_ASSERT( aTableOID == aVarEntry->mTableOID );

    for( i = 0; i < SM_VAR_PAGE_LIST_COUNT; i++ )
    {
        // FreePageList ����
        svpFreePageList::initializeFreePageListAndPool(&(aVarEntry[i]));
    }

    for( sPageListID = 0;
         sPageListID < SMP_PAGE_LIST_COUNT;
         sPageListID++ )
    {
        // for AllocPageList
        IDE_TEST( svpAllocPageList::freePageListToDB(
                      aTrans,
                      aSpaceID,
                      &(aVarEntry->mRuntimeEntry->mAllocPageList[sPageListID]) )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC svpVarPageList::setValue(scSpaceID       aSpaceID,
                                smOID           aPieceOID,
                                const void*     aValue,
                                UInt            aLength)
{
    smVCPieceHeader  *sVCPieceHeader;

    IDE_DASSERT( aPieceOID != SM_NULL_OID );
    IDE_DASSERT( aValue != NULL );

    IDE_ASSERT( smmManager::getOIDPtr( aSpaceID,
                                       aPieceOID,
                                       (void**)&sVCPieceHeader )
                == IDE_SUCCESS );

    /* Set Length Of VCPieceHeader*/
    sVCPieceHeader->length = aLength;

    /* Set Value */
    idlOS::memcpy( (SChar*)(sVCPieceHeader + 1), aValue, aLength );

    return IDE_SUCCESS;
}

/***********************************************************************
 * Description: Variable Length Data�� �д´�. ���� ���̰� �ִ� Piece���� ����
 *              �۰� �о�� �� Piece�� �ϳ��� �������� �����Ѵٸ� �о�� �� ��ġ��
 *              ù��° ����Ʈ�� ��ġ�� �޸� �����͸� �����Ѵ�. �׷��� ���� ��� Row��
 *              ���� �����ؼ� �Ѱ��ش�.
 *
 * aSpaceID     - [IN] Space ID
 * aBeginPos    - [IN] ���� ��ġ
 * aReadLen     - [IN] ���� ����Ÿ�� ����
 * aFstPieceOID - [IN] ù��° Piece�� OID
 * aBuffer      - [IN] value�� �����.
 *
 ***********************************************************************/
SChar* svpVarPageList::getValue( scSpaceID       aSpaceID,
                                 UInt            aBeginPos,
                                 UInt            aReadLen,
                                 smOID           aFstPieceOID,
                                 SChar          *aBuffer )
{
    smVCPieceHeader  *sVCPieceHeader;
    SLong             sRemainedReadSize;
    UInt              sPos;
    SChar            *sRowBuffer;
    SInt              sReadPieceSize;
    SChar            *sRet;

    IDE_ASSERT( aFstPieceOID != SM_NULL_OID );

    /* ù��° VC Piece Header�� �����´�. */
    IDE_ASSERT( smmManager::getOIDPtr( aSpaceID,
                                       aFstPieceOID,
                                       (void**)&sVCPieceHeader )
                == IDE_SUCCESS );

    /* ���� ù��° ����Ʈ ��ġ�� ã�´�. */
    sPos = aBeginPos;
    while( sPos >= SMP_VC_PIECE_MAX_SIZE )
    {
        sPos -= sVCPieceHeader->length;

        IDE_ASSERT( smmManager::getOIDPtr( aSpaceID,
                                           sVCPieceHeader->nxtPieceOID,
                                           (void**)&sVCPieceHeader )
                    == IDE_SUCCESS );
    }

    IDE_ASSERT( sPos < SMP_VC_PIECE_MAX_SIZE );

    sRet = svpVarPageList::getPieceValuePtr( sVCPieceHeader, sPos );

    /* ����Ÿ�� ���̰� SMP_VC_PIECE_MAX_SIZE���� �۰ų� ���� ���� ����Ÿ
     * �� �������� ���ļ� ������� �ʰ� �ϳ��� �������� ���ٸ� */
    if( sPos + aReadLen > SMP_VC_PIECE_MAX_SIZE )
    {
        sRemainedReadSize = aReadLen;
        sRowBuffer = aBuffer;

        sRet = aBuffer;
        while( sRemainedReadSize > (SLong)SMP_VC_PIECE_MAX_SIZE )
        {
            sReadPieceSize = sVCPieceHeader->length - sPos;
            IDE_ASSERT( sReadPieceSize > 0 );
            // BUG-27649 CodeSonar::Null Pointer Dereference (9)
            IDE_ASSERT( sRowBuffer != NULL );

            sPos = 0;

            idlOS::memcpy(
                sRowBuffer,
                svpVarPageList::getPieceValuePtr( sVCPieceHeader, sPos ),
                sReadPieceSize );
            sRowBuffer += sReadPieceSize;
            sRemainedReadSize -= sReadPieceSize;

            if( sRemainedReadSize != 0 )
            {
                IDE_ASSERT( smmManager::getOIDPtr( 
                                aSpaceID,
                                sVCPieceHeader->nxtPieceOID,
                                (void**)&sVCPieceHeader )
                            == IDE_SUCCESS );
            }
        }

        if( sRemainedReadSize != 0 )
        {
            // BUG-27649 CodeSonar::Null Pointer Dereference (9)
            IDE_ASSERT( sRowBuffer != NULL );

            idlOS::memcpy(
                sRowBuffer,
                svpVarPageList::getPieceValuePtr( sVCPieceHeader, sPos ),
                sRemainedReadSize );

            sRowBuffer += sRemainedReadSize;

            // BUG-27331 CodeSonar::Uninitialized Variable
            sRemainedReadSize = 0;
        }

        /* ���ۿ� ����� ����Ÿ�� ���̴� ���� ����Ÿ�� ���̿�
         * �����ؾ� �մϴ�. */
        IDE_ASSERT( (UInt)(sRowBuffer - aBuffer) == aReadLen );
    }

    return sRet;
}

/**********************************************************************
 *  Varialbe Column�� ���� Slot Header�� altibase_sm.log�� �����Ѵ�
 *
 *  aVarSlotHeader : dump�� slot ���
 **********************************************************************/
IDE_RC svpVarPageList::dumpVarColumnHeader( smVCPieceHeader *aVCPieceHeader )
{
    IDE_ERROR( aVCPieceHeader != NULL );

    ideLog::log(SM_TRC_LOG_LEVEL_FATAL,
                SM_TRC_MPAGE_DUMP_VAR_COL_HEAD,
                (ULong)aVCPieceHeader->nxtPieceOID,
                aVCPieceHeader->length,
                aVCPieceHeader->flag);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/**********************************************************************
 * system���κ��� persistent page�� �Ҵ�޴´�.
 *
 * aTrans    : �۾��ϴ� Ʈ����� ��ü
 * aVarEntry : �Ҵ���� PageListEntry
 * aIdx      : �Ҵ���� VarSlot ũ�� idx
 **********************************************************************/
IDE_RC svpVarPageList::allocPersPages( void*             aTrans,
                                       scSpaceID         aSpaceID,
                                       smpPageListEntry* aVarEntry,
                                       UInt              aIdx )
{
    UInt                    sState   = 0;
    smpPersPage*            sPagePtr = NULL;
    smpPersPage*            sAllocPageHead;
    smpPersPage*            sAllocPageTail;
    scPageID                sNextPageID;
    UInt                    sPageListID;
#ifdef DEBUG
    smpFreePagePoolEntry*   sFreePagePool;
    smpFreePageListEntry*   sFreePageList;
#endif
    smpAllocPageListEntry*  sAllocPageList;
    smpPrivatePageListEntry* sPrivatePageList = NULL;
    UInt                    sAllocPageCnt = 0;

    IDE_DASSERT( aTrans != NULL );
    IDE_DASSERT( aVarEntry != NULL );
    IDE_DASSERT( aIdx < SM_VAR_PAGE_LIST_COUNT );

    smLayerCallback::allocRSGroupID( aTrans,
                                     &sPageListID );

    sAllocPageList = &(aVarEntry->mRuntimeEntry->mAllocPageList[sPageListID]);

#ifdef DEBUG
    sFreePagePool  = &(aVarEntry->mRuntimeEntry->mFreePagePool);
    sFreePageList  = &(aVarEntry->mRuntimeEntry->mFreePageList[sPageListID]);
#endif
    IDE_DASSERT( sAllocPageList != NULL );
    IDE_DASSERT( sFreePagePool != NULL );
    IDE_DASSERT( sFreePageList != NULL );

    // DB���� Page���� �Ҵ������ FreePageList��
    // Tx's Private Page List�� ���� ����� ��
    // Ʈ������� ����� �� �ش� ���̺��� PageListEntry�� ����ϰ� �ȴ�.

    IDE_TEST( smLayerCallback::findVolPrivatePageList( aTrans,
                                                       aVarEntry->mTableOID,
                                                       &sPrivatePageList )
              != IDE_SUCCESS );

    if(sPrivatePageList == NULL)
    {
        // ������ PrivatePageList�� �����ٸ� ���� �����Ѵ�.
        IDE_TEST( smLayerCallback::createVolPrivatePageList( aTrans,
                                                             aVarEntry->mTableOID,
                                                             &sPrivatePageList )
                  != IDE_SUCCESS );
    }

    // [1] svmManager�κ��� page �Ҵ�
    IDE_TEST( svmManager::allocatePersPageList( aTrans,
                                                aSpaceID,
                                                SMP_ALLOCPAGECOUNT_FROMDB,
                                                (void **)&sAllocPageHead,
                                                (void **)&sAllocPageTail,
                                                &sAllocPageCnt )
              != IDE_SUCCESS);

    IDE_DASSERT( svpAllocPageList::isValidPageList(
                     aSpaceID,
                     sAllocPageHead->mHeader.mSelfPageID,
                     sAllocPageTail->mHeader.mSelfPageID,
                     sAllocPageCnt )
                 == ID_TRUE );
    sState = 1;

    // �Ҵ���� HeadPage�� PrivatePageList�� ����Ѵ�.
    IDE_DASSERT( sPrivatePageList->mVarFreePageHead[aIdx] == NULL );

    sPrivatePageList->mVarFreePageHead[aIdx] =
        svpFreePageList::getFreePageHeader(aSpaceID,
                                           sAllocPageHead->mHeader.mSelfPageID);

    // [2] page �ʱ�ȭ
    sNextPageID = sAllocPageHead->mHeader.mSelfPageID;

    while(sNextPageID != SM_NULL_PID)
    {
        IDE_ASSERT( smmManager::getPersPagePtr( aSpaceID,
                                                sNextPageID,
                                                (void**)&sPagePtr )
                    == IDE_SUCCESS );

        // PersPageHeader �ʱ�ȭ�ϰ� (FreeSlot���� �����Ѵ�.)
        initializePage( aIdx,
                        sPageListID,
                        aVarEntry->mSlotSize,
                        aVarEntry->mSlotCount,
                        aVarEntry->mTableOID,
                        sPagePtr );


        // FreePageHeader �ʱ�ȭ�ϰ�
        svpFreePageList::initializeFreePageHeader(
            svpFreePageList::getFreePageHeader(aSpaceID, sPagePtr) );

        // FreeSlotList�� Page�� ����Ѵ�.
        svpFreePageList::initializeFreeSlotListAtPage( aSpaceID,
                                                       aVarEntry,
                                                       sPagePtr );

        sNextPageID = sPagePtr->mHeader.mNextPageID;

        // FreePageHeader�� PrivatePageList�� �����Ѵ�.
        // PrivatePageList�� Var������ �ܹ��⸮��Ʈ�̱⶧���� Prev�� NULL�� ���õȴ�.
        svpFreePageList::addFreePageToPrivatePageList( aSpaceID,
                                                       sPagePtr->mHeader.mSelfPageID,
                                                       SM_NULL_PID,  // Prev
                                                       sNextPageID );
    }

    // [3] AllocPageList ���
    IDE_TEST( sAllocPageList->mMutex->lock(NULL) != IDE_SUCCESS );

    IDE_TEST( svpAllocPageList::addPageList( aSpaceID,
                                             sAllocPageList,
                                             sAllocPageHead,
                                             sAllocPageTail,
                                             sAllocPageCnt )
              != IDE_SUCCESS );

    IDE_TEST(sAllocPageList->mMutex->unlock() != IDE_SUCCESS);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    switch(sState)
    {
        case 1:
            /* rollback�� �Ͼ�� �ȵȴ�. ������ �����ؾ� �Ѵ�. */
            /* BUGBUG assert ���� �ٸ� ó�� ��� ����غ� �� */
            IDE_ASSERT(0);
            break;
        default:
            break;
    }

    return IDE_FAILURE;
}

/***********************************************************************
 * var slot�� �Ҵ��Ѵ�.
 *
 * aTrans    : �۾��Ϸ��� Ʈ����� ��ü
 * aTableOID : �Ҵ��Ϸ��� ���̺��� OID
 * aVarEntry : �Ҵ��Ϸ��� PageListEntry
 * aPieceSize: Variable Column�� ������ Piece�� ũ��
 * aNxtPieceOID  : �Ҵ��� Piece�� nextPieceOID
 * aPieceOID : �Ҵ�� Piece�� OID
 * aPiecePtr      : �Ҵ��ؼ� ��ȯ�Ϸ��� Row ������
 ***********************************************************************/
IDE_RC svpVarPageList::allocSlot( void*              aTrans,
                                  scSpaceID          aSpaceID,
                                  smOID              aTableOID,
                                  smpPageListEntry*  aVarEntry,
                                  UInt               aPieceSize,
                                  smOID              aNxtPieceOID,
                                  smOID*             aPieceOID,
                                  SChar**            aPiecePtr)
{
    UInt              sIdx   = 0;
    UInt              sPageListID;
    smVCPieceHeader*  sCurVCPieceHeader = NULL;
    smVCPieceHeader   sAfterVCPieceHeader;
    smpPageListEntry* sVarPageList = NULL;

    IDE_DASSERT( aTrans != NULL );
    IDE_DASSERT( aVarEntry != NULL );
    IDE_DASSERT( aPieceOID != NULL );
    IDE_DASSERT( aPiecePtr != NULL );

    /* ----------------------------
     * ũ�⿡ �´� variable page list ����
     * ---------------------------*/
    IDE_TEST( calcVarIdx( aPieceSize, &sIdx ) != IDE_SUCCESS );

    IDE_ASSERT( sIdx < SM_VAR_PAGE_LIST_COUNT );
    IDE_ASSERT( aTableOID == aVarEntry[sIdx].mTableOID );

    sVarPageList = aVarEntry + sIdx;

    smLayerCallback::allocRSGroupID( aTrans,
                                     &sPageListID );

    while(1)
    {
        // 1) Tx's PrivatePageList���� ã��
        IDE_TEST( tryForAllocSlotFromPrivatePageList( aTrans,
                                                      aSpaceID,
                                                      aTableOID,
                                                      sIdx,
                                                      aPieceOID,
                                                      aPiecePtr )
                  != IDE_SUCCESS );

        if(*aPiecePtr != NULL)
        {
            break;
        }

        // 2) FreePageList���� ã��
        IDE_TEST( tryForAllocSlotFromFreePageList( aTrans,
                                                   aSpaceID,
                                                   aVarEntry,
                                                   sIdx,
                                                   sPageListID,
                                                   aPieceOID,
                                                   aPiecePtr )
                  != IDE_SUCCESS );

        if( *aPiecePtr != NULL)
        {
            break;
        }

        // 3) system���κ��� page�� �Ҵ�޴´�.
        IDE_TEST( allocPersPages( aTrans,
                                  aSpaceID,
                                  sVarPageList,
                                  sIdx )
                  != IDE_SUCCESS );
    }

    sCurVCPieceHeader = (smVCPieceHeader*)(*aPiecePtr);

    //update var row head
    sAfterVCPieceHeader      = *sCurVCPieceHeader;
    sAfterVCPieceHeader.flag = SM_VCPIECE_FREE_NO |
                               SM_VCPIECE_TYPE_OTHER;

    /* BUG-15354: [A4] SM VARCHAR 32K: Varchar����� PieceHeader�� ���� logging��
     * �����Ǿ� PieceHeader�� ���� Redo, Undo�� ��������. */
    sAfterVCPieceHeader.length      = aPieceSize;
    sAfterVCPieceHeader.nxtPieceOID = aNxtPieceOID;

    *sCurVCPieceHeader = sAfterVCPieceHeader;

    // Record Count ���� // BUG-47706
    idCore::acpAtomicInc64( &(sVarPageList->mRuntimeEntry->mInsRecCnt) );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**********************************************************************
 * Tx's PrivatePageList�� FreePage�κ��� Slot�� �Ҵ��� �� ������ �˻��ϰ�
 * �����ϸ� �Ҵ��Ѵ�.
 *
 * aTrans    : �۾��ϴ� Ʈ����� ��ü
 * aTableOID : �Ҵ��Ϸ��� ���̺� OID
 * aIdx      : �Ҵ��Ϸ��� VarPage�� Idx��
 * aPieceOID : �Ҵ��ؼ� ��ȯ�Ϸ��� Slot �� OID
 * aPiecePtr : �Ҵ��ؼ� ��ȯ�Ϸ��� Slot ������
 **********************************************************************/

IDE_RC svpVarPageList::tryForAllocSlotFromPrivatePageList(
    void*       aTrans,
    scSpaceID   aSpaceID,
    smOID       aTableOID,
    UInt        aIdx,
    smOID*      aPieceOID,
    SChar**     aPiecePtr )
{
    smpPrivatePageListEntry* sPrivatePageList = NULL;
    smpFreePageHeader*       sFreePageHeader;

    IDE_DASSERT(aTrans != NULL);
    IDE_DASSERT(aIdx < SM_VAR_PAGE_LIST_COUNT);
    IDE_DASSERT(aPiecePtr != NULL);

    *aPiecePtr = NULL;

    IDE_TEST( smLayerCallback::findVolPrivatePageList( aTrans,
                                                       aTableOID,
                                                       &sPrivatePageList )
              != IDE_SUCCESS );

    if(sPrivatePageList != NULL)
    {
        sFreePageHeader = sPrivatePageList->mVarFreePageHead[aIdx];

        if(sFreePageHeader != NULL)
        {
            IDE_DASSERT(sFreePageHeader->mFreeSlotCount > 0);

            removeSlotFromFreeSlotList( aSpaceID,
                                        sFreePageHeader,
                                        aPieceOID,
                                        aPiecePtr );

            if(sFreePageHeader->mFreeSlotCount == 0)
            {
                sPrivatePageList->mVarFreePageHead[aIdx] =
                    sFreePageHeader->mFreeNext;

                svpFreePageList::initializeFreePageHeader(sFreePageHeader);
            }
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * FreePageList�� FreePagePool���� FreeSlot�� �Ҵ��� �� �ִ��� �õ�
 * �Ҵ��� �Ǹ� aPiecePtr�� ��ȯ�ϰ� �Ҵ��� FreeSlot�� ���ٸ� aPiecePtr�� NULL�� ��ȯ
 *
 * aTrans      : �۾��ϴ� Ʈ����� ��ü
 * aVarEntry   : Slot�� �Ҵ��Ϸ��� PageListEntry
 * aPageListID : Slot�� �Ҵ��Ϸ��� PageListID
 * aPieceOID   : �Ҵ��ؼ� ��ȯ�Ϸ��� Piece OID
 * aPiecePtr   : �Ҵ��ؼ� ��ȯ�Ϸ��� Piece Ptr
 ***********************************************************************/
IDE_RC svpVarPageList::tryForAllocSlotFromFreePageList(
    void*             aTrans,
    scSpaceID         aSpaceID,
    smpPageListEntry* aVarEntryArray,
    UInt              aIdx,
    UInt              aPageListID,
    smOID*            aPieceOID,
    SChar**           aPiecePtr )
{
    UInt                  sState = 0;
    UInt                  sSizeClassID;
    idBool                sIsPageAlloced;
    smpFreePageHeader*    sFreePageHeader;
    smpFreePageListEntry* sFreePageList;
    UInt                  sSizeClassCount;
    smpPageListEntry    * sVarEntry = NULL;

    IDE_DASSERT( aTrans != NULL );
    IDE_DASSERT( aVarEntryArray != NULL );
    IDE_DASSERT( aPageListID < SMP_PAGE_LIST_COUNT );
    IDE_DASSERT( aPiecePtr != NULL );

    sVarEntry     = aVarEntryArray + aIdx;
    sFreePageList = &(sVarEntry->mRuntimeEntry->mFreePageList[aPageListID]);

    IDE_DASSERT( sFreePageList != NULL );

    *aPiecePtr = NULL;
    *aPieceOID = SM_NULL_OID;

    sSizeClassCount = SMP_SIZE_CLASS_COUNT( sVarEntry->mRuntimeEntry );

    while(1)
    {
        // FreePageList�� SizeClass�� ��ȸ�ϸ鼭 tryAllocSlot�Ѵ�.
        for( sSizeClassID = 0;
             sSizeClassID < sSizeClassCount;
             sSizeClassID++ )
        {
            sFreePageHeader  = sFreePageList->mHead[sSizeClassID];

            while(sFreePageHeader != NULL)
            {
                // �ش� Page�� ���� Slot�� �Ҵ��Ϸ��� �ϰ�
                // �Ҵ��ϰԵǸ� �ش� Page�� �Ӽ��� ����ǹǷ� lock���� ��ȣ
                IDE_TEST(sFreePageHeader->mMutex.lock(NULL) != IDE_SUCCESS);
                sState = 1;

                // lock������� �ش� Page�� ���� �ٸ� Tx�� ���� ����Ǿ����� �˻�
                if(sFreePageHeader->mFreeListID == aPageListID)
                {
                    IDE_ASSERT(sFreePageHeader->mFreeSlotCount > 0);

                    removeSlotFromFreeSlotList(aSpaceID,
                                               sFreePageHeader,
                                               aPieceOID,
                                               aPiecePtr);

                    // FreeSlot�� �Ҵ��� Page�� SizeClass�� ����Ǿ�����
                    // Ȯ���Ͽ� ����
                    IDE_TEST(svpFreePageList::modifyPageSizeClass(
                                 aTrans,
                                 sVarEntry,
                                 sFreePageHeader)
                             != IDE_SUCCESS);

                    sState = 0;
                    IDE_TEST(sFreePageHeader->mMutex.unlock() != IDE_SUCCESS);

                    IDE_CONT(normal_case);
                }

                sState = 0;
                IDE_TEST(sFreePageHeader->mMutex.unlock() != IDE_SUCCESS);

                // �ش� Page�� ����� ���̶�� List���� �ٽ� Head�� �����´�.
                sFreePageHeader = sFreePageList->mHead[sSizeClassID];
            }
        }

        // FreePageList���� FreeSlot�� ã�� ���ߴٸ�
        // FreePagePool���� Ȯ���Ͽ� �����´�.

        IDE_TEST( svpFreePageList::tryForAllocPagesFromPool( sVarEntry,
                                                             aPageListID,
                                                             &sIsPageAlloced )
                  != IDE_SUCCESS );

        if ( sIsPageAlloced == ID_FALSE )
        {
            /* BUG-47358
               FreePagePool���� �������� �������� ���ߴٸ�,
               (Slot ũ�Ⱑ) �ٸ� Variable Page�� FreePagePool���� �������� �����´�. */
            tryForAllocPagesFromOtherPools( aSpaceID,
                                            aVarEntryArray,
                                            aIdx,
                                            aPageListID,
                                            &sIsPageAlloced );

            if ( sIsPageAlloced == ID_FALSE )
            {
                /* Pool���� �������Դ�. */
                IDE_CONT(normal_case);
            }
        }
    }

    IDE_EXCEPTION_CONT( normal_case );

    IDE_ASSERT(sState == 0);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    IDE_PUSH();

    if(sState == 1)
    {
        IDE_ASSERT( sFreePageHeader->mMutex.unlock() == IDE_SUCCESS );
    }

    IDE_POP();

    return IDE_FAILURE;
}

/***********************************************************************
 * slot�� free �Ѵ�.
 *
 * BUG-14093 Ager Tx�� freeSlot�� ���� commit���� ���� ��Ȳ����
 *           �ٸ� Tx�� �Ҵ�޾� ��������� ���� ����� �����߻�
 *           ���� Ager Tx�� Commit���Ŀ� FreeSlot�� FreeSlotList�� �Ŵܴ�.
 *
 * aTrans     : �۾��� �����ϴ� Ʈ����� ��ü
 * aVarEntry  : aPiecePtr�� ���� PageListEntry
 * aPieceOID  : free�Ϸ��� Piece OID
 * aPiecePtr  : free�Ϸ��� Piece Ptr
 * aTableType : Temp Table�� slot������ ���� ����
 ***********************************************************************/
IDE_RC svpVarPageList::freeSlot( void*             aTrans,
                                 scSpaceID         aSpaceID,
                                 smpPageListEntry* aVarEntry,
                                 smOID             aPieceOID,
                                 SChar*            aPiecePtr,
                                 smpTableType      aTableType,
                                 smSCN             aSCN )
{

    UInt                sIdx;
    scPageID            sPageID;
    smpPageListEntry  * sVarPageList;
    SChar             * sPagePtr;

    IDE_DASSERT( ((aTrans != NULL) && (aTableType == SMP_TABLE_NORMAL)) ||
                 ((aTrans == NULL) && (aTableType == SMP_TABLE_TEMP)) );

    IDE_DASSERT( aVarEntry != NULL );
    IDE_DASSERT( aPiecePtr != NULL );

    /* ----------------------------
     * BUG-14093
     * freeSlot������ slot�� ���� Free�۾��� �����ϰ�
     * ager Tx�� commit�� ���Ŀ� addFreeSlotPending�� �����Ѵ�.
     * ---------------------------*/

    sPageID        = SM_MAKE_PID(aPieceOID);
    IDE_ASSERT( smmManager::getPersPagePtr( aSpaceID,
                                            sPageID,
                                            (void**)&sPagePtr )
                == IDE_SUCCESS );
    sIdx = getVarIdx( sPagePtr );

    IDE_ASSERT(sIdx < SM_VAR_PAGE_LIST_COUNT);

    sVarPageList    = aVarEntry + sIdx;

    IDE_TEST( setFreeSlot( aTrans,
                           sPageID,
                           aPieceOID,
                           aPiecePtr,
                           aTableType )
              != IDE_SUCCESS );

    // Record Count ���� // BUG-47706
    idCore::acpAtomicInc64( &(sVarPageList->mRuntimeEntry->mDelRecCnt) );

    if(aTableType == SMP_TABLE_NORMAL)
    {
        // BUG-14093 freeSlot�ϴ� ager�� commit�ϱ� ������
        //           freeSlotList�� �Ŵ��� �ʰ� ager TX��
        //           commit ���Ŀ� �Ŵ޵��� OIDList�� �߰��Ѵ�.
        IDE_TEST( smLayerCallback::addOID( aTrans,
                                           aVarEntry->mTableOID,
                                           aPieceOID,
                                           aSpaceID,
                                           SM_OID_TYPE_FREE_VAR_SLOT,
                                           aSCN )
                  != IDE_SUCCESS );
    }
    else
    {
        // TEMP Table�� �ٷ� FreeSlotList�� �߰��Ѵ�.
        IDE_TEST( addFreeSlotPending(aTrans,
                                     aSpaceID,
                                     aVarEntry,
                                     aPieceOID,
                                     aPiecePtr)
                 != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Page�� �ʱ�ȭ �Ѵ�.
 * Page���� ��� Slot�鵵 �ʱ�ȭ�ϸ� Next ��ũ�� �����Ѵ�.
 *
 * aIdx        : Page�� VarEntry�� Idx ��
 * aPageListID : Page�� ���� PageListID
 * aSlotSize   : Page�� ���� Slot�� ũ��
 * aSlotCount  : Page���� ��� Slot ����
 * aPage       : �ʱ�ȭ�� Page
 ***********************************************************************/
void svpVarPageList::initializePage( vULong       aIdx,
                                     UInt         aPageListID,
                                     vULong       aSlotSize,
                                     vULong       aSlotCount,
                                     smOID        aTableOID,
                                     smpPersPage* aPage )
{
    UInt                   i;
    smVCPieceHeader*       sCurVCFreePieceHeader = NULL;
    smVCPieceHeader*       sNxtVCFreePieceHeader;
    smpVarPageHeader*      sVarPageHeader;

    // BUG-26937 CodeSonar::NULL Pointer Dereference (4)
    IDE_DASSERT( aIdx < SM_VAR_PAGE_LIST_COUNT );
    IDE_DASSERT( aPageListID < SMP_PAGE_LIST_COUNT );
    IDE_DASSERT( aSlotSize > 0 );
    IDE_ASSERT( aSlotCount > 0 );
    IDE_DASSERT( aPage != NULL );

    sVarPageHeader       = (smpVarPageHeader*)(aPage->mBody);
    sVarPageHeader->mIdx = aIdx;

    aPage->mHeader.mType        = SMP_PAGETYPE_VAR;
    aPage->mHeader.mAllocListID = aPageListID;
    aPage->mHeader.mTableOID    = aTableOID;

    sNxtVCFreePieceHeader   = (smVCPieceHeader*)(sVarPageHeader + 1);

    // Variable Page�� �����ִ� ��� VC Piece�� free�� Setting.
    for( i = 0; i < aSlotCount; i++ )
    {
        sCurVCFreePieceHeader = sNxtVCFreePieceHeader;

        // Set VC Free Piece
        sCurVCFreePieceHeader->flag = SM_VCPIECE_FREE_OK ;

        sNxtVCFreePieceHeader = (smVCPieceHeader*)((SChar*)sCurVCFreePieceHeader + aSlotSize);
        sCurVCFreePieceHeader->nxtPieceOID  = (smOID)sNxtVCFreePieceHeader;
    }

    // ������ Next = NULL
    sCurVCFreePieceHeader->nxtPieceOID = (smOID)NULL;

    IDL_MEM_BARRIER;

    return;
}

/**********************************************************************
 * FreeSlot ������ ����Ѵ�.
 *
 * aTrans      : �۾��Ϸ��� Ʈ����� ��ü
 * aPageID     : FreeSlot�߰��Ϸ��� PageID
 * aVCPieceOID : Free�ҷ��� Piece OID
 * aVCPiecePtr : Free�ҷ��� Piece Ptr
 * aTableType  : Temp���̺����� ����
 **********************************************************************/
IDE_RC svpVarPageList::setFreeSlot( void        * aTrans,
                                    scPageID      aPageID,
                                    smOID         aVCPieceOID,
                                    SChar       * aVCPiecePtr,
                                    smpTableType  aTableType )
{
    smVCPieceHeader* sCurFreeVCPieceHdr;

    IDE_ASSERT( ((aTrans != NULL) && (aTableType == SMP_TABLE_NORMAL)) ||
                ((aTrans == NULL) && (aTableType == SMP_TABLE_TEMP)) );

    IDE_ASSERT( aPageID     != SM_NULL_PID );
    IDE_ASSERT( aVCPieceOID != SM_NULL_OID );
    IDE_DASSERT( aVCPiecePtr != NULL );

    sCurFreeVCPieceHdr = (smVCPieceHeader*)aVCPiecePtr;
    sCurFreeVCPieceHdr->nxtPieceOID = SM_NULL_OID;

    /* Variable Column�� Transaction�� Commit������ Variable Column������
       �̸� Variable Column�� ��� VC Piece�� Flag�� Free�Ǿ��ٰ� ǥ���Ѵ�.
       ������ Transaction���� Variable Column�� ���� Fixed Row�� ���� �ڽ���
       �� Variable Column�� �о�� ���� �����ϱ� ������ Transaction Commit������
       Flag�� �����Ͽ��� ������ �ȵȴ�. �� Flag�� �����ϴ� ���� refine�� Free
       VC Piece List�� ������ ���̴�. ���� Transaction�� Alloc, Free�� Flag��
       Setting������ ���������� �ʴ´�.*/
    if( (sCurFreeVCPieceHdr->flag & SM_VCPIECE_FREE_MASK)
        == SM_VCPIECE_FREE_NO )
    {
        sCurFreeVCPieceHdr = (smVCPieceHeader*)aVCPiecePtr;

        // Set Variable Column To be Free.
        sCurFreeVCPieceHdr->flag = SM_VCPIECE_FREE_OK;
    }

    return IDE_SUCCESS;
}

/**********************************************************************
 * ���� FreeSlot�� FreeSlotList�� �߰��Ѵ�.
 *
 * BUG-14093 Commit���Ŀ� FreeSlot�� ���� FreeSlotList�� �Ŵܴ�.
 *
 * aTrans    : �۾��ϴ� Ʈ����� ��ü
 * aVarEntry : FreeSlot�� ���� PageListEntry
 * aPieceOID : FreeSlot�� OID
 * aPiecePtr : FreeSlot�� Row ������
 **********************************************************************/

IDE_RC svpVarPageList::addFreeSlotPending( void*             aTrans,
                                           scSpaceID         aSpaceID,
                                           smpPageListEntry* aVarEntry,
                                           smOID             aPieceOID,
                                           SChar*            aPiecePtr )
{
    UInt               sState = 0;
    UInt               sIdx;
    scPageID           sPageID;
    SChar            * sPagePtr;
    smpFreePageHeader* sFreePageHeader;

    IDE_DASSERT(aVarEntry != NULL);
    IDE_DASSERT(aPiecePtr != NULL);

    sPageID         = SM_MAKE_PID(aPieceOID);
    IDE_ASSERT( smmManager::getPersPagePtr( aSpaceID,
                                            sPageID,
                                            (void**)&sPagePtr )
                == IDE_SUCCESS );
    sIdx            = getVarIdx( sPagePtr );
    sFreePageHeader = svpFreePageList::getFreePageHeader(aSpaceID, sPageID);

    IDE_ASSERT(sIdx < SM_VAR_PAGE_LIST_COUNT);

    IDE_TEST(sFreePageHeader->mMutex.lock(NULL) != IDE_SUCCESS);
    sState = 1;

    // PrivatePageList������ FreeSlot���� �ʴ´�.
    IDE_ASSERT(sFreePageHeader->mFreeListID != SMP_PRIVATE_PAGELISTID);

    // FreeSlot�� FreeSlotList�� �߰�
    addFreeSlotToFreeSlotList(aSpaceID, sPageID, aPiecePtr);

    // FreeSlot�� �߰��� ���� SizeClass�� ����Ǿ����� Ȯ���Ͽ� �����Ѵ�.
    IDE_TEST(svpFreePageList::modifyPageSizeClass( aTrans,
                                                   aVarEntry + sIdx,
                                                   sFreePageHeader )
             != IDE_SUCCESS);

    sState = 0;
    IDE_TEST(sFreePageHeader->mMutex.unlock() != IDE_SUCCESS);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    IDE_PUSH();

    if(sState == 1)
    {
        IDE_ASSERT(sFreePageHeader->mMutex.unlock() == IDE_SUCCESS);
    }

    IDE_POP();

    return IDE_FAILURE;
}

/**********************************************************************
 * FreePageHeader���� FreeSlot����
 *
 * �ش� Page�� Lock�� ��� ���;� �Ѵ�.
 *
 * aFreePageHeader : �����Ϸ��� FreePageHeader
 * aPieceOID       : ������ Variable Column Piece OID ��ȯ
 * aPiecePtr       : ������ Free VC Piece�� ������ ��ȯ
 **********************************************************************/
void svpVarPageList::removeSlotFromFreeSlotList(
                                            scSpaceID          aSpaceID,
                                            smpFreePageHeader* aFreePageHeader,
                                            smOID*             aPieceOID,
                                            SChar**            aPiecePtr )
{
    smVCPieceHeader* sFreeVCPieceHdr;
    SChar          * sPagePtr;

    IDE_DASSERT(aFreePageHeader != NULL);
    IDE_DASSERT(aFreePageHeader->mFreeSlotCount > 0);
    IDE_DASSERT(aPieceOID != NULL);
    IDE_DASSERT(aPiecePtr != NULL);

    IDE_DASSERT( isValidFreeSlotList(aFreePageHeader) == ID_TRUE );

    sFreeVCPieceHdr = (smVCPieceHeader*)(aFreePageHeader->mFreeSlotHead);

    if ( (sFreeVCPieceHdr->flag & SM_VCPIECE_FREE_MASK)
         == SM_VCPIECE_FREE_NO )
    {
        ideLog::log(SM_TRC_LOG_LEVEL_FATAL, SM_TRC_MPAGE_REMOVE_VAR_PAGE_FATAL1);
        ideLog::log(SM_TRC_LOG_LEVEL_FATAL, SM_TRC_MPAGE_REMOVE_VAR_PAGE_FATAL2);
        (void)dumpVarColumnHeader( sFreeVCPieceHdr );

        IDE_ASSERT(0);
    }

    IDE_ASSERT( smmManager::getPersPagePtr( aSpaceID,
                                            aFreePageHeader->mSelfPageID,
                                            (void**)&sPagePtr )
                == IDE_SUCCESS );
    *aPieceOID = SM_MAKE_OID(aFreePageHeader->mSelfPageID,
                             (SChar*)sFreeVCPieceHdr - (SChar*)sPagePtr);

    *aPiecePtr = (SChar*)sFreeVCPieceHdr;

    aFreePageHeader->mFreeSlotCount--;

    if(sFreeVCPieceHdr->nxtPieceOID == (smOID)NULL)
    {
        // Next�� ���ٸ� ������ FreeSlot�̴�.
        IDE_ASSERT(aFreePageHeader->mFreeSlotCount == 0);

        aFreePageHeader->mFreeSlotTail = NULL;
    }
    else
    {
        // ���� FreeSlot�� Head�� ����Ѵ�.
        IDE_ASSERT(aFreePageHeader->mFreeSlotCount > 0);
    }

    aFreePageHeader->mFreeSlotHead = (SChar*)(sFreeVCPieceHdr->nxtPieceOID);

    IDE_DASSERT( isValidFreeSlotList(aFreePageHeader) == ID_TRUE );

    return;
}

/***********************************************************************
 * nextOIDall�� ���� aPiecePtr��  Page�� Next Piece Ptr, OID�� ���Ѵ�.
 * aPiecePtr�� ���� NULL�̶�� aVarEntry���� ù��° Allocated Row�� ã�´�.
 *
 * aVarEntry : ��ȸ�Ϸ��� PageListEntry
 * aPieceOID : Current Piece OID
 * aPiecePtr : Current Piece Ptr
 * aPage     : aPiecePtr�� ���� Page�� ã�Ƽ� ��ȯ
 * aNxtPieceOID : Next Piece OID
 * aPiecePtrPtr : Next Piece Ptr
 ***********************************************************************/
void svpVarPageList::initForScan( scSpaceID         aSpaceID,
                                  smpPageListEntry* aVarEntry,
                                  smOID             aPieceOID,
                                  SChar*            aPiecePtr,
                                  smpPersPage**     aPage,
                                  smOID*            aNxtPieceOID,
                                  SChar**           aNxtPiecePtr )
{
    UInt             sIdx;
    scPageID         sPageID;

    IDE_DASSERT( aVarEntry != NULL );
    IDE_DASSERT( aPage != NULL );
    IDE_DASSERT( aNxtPiecePtr != NULL );

    *aPage   = NULL;
    *aNxtPiecePtr = NULL;

    if(aPiecePtr != NULL)
    {
        IDE_ASSERT( smmManager::getPersPagePtr( aSpaceID,
                                                SM_MAKE_PID(aPieceOID),
                                                (void**)aPage )
                    == IDE_SUCCESS );
        sIdx     = getVarIdx( *aPage );
        *aNxtPiecePtr = aPiecePtr + aVarEntry[sIdx].mSlotSize;
        *aNxtPieceOID = aPieceOID + aVarEntry[sIdx].mSlotSize;
    }
    else
    {
        sPageID = svpManager::getFirstAllocPageID(aVarEntry);

        if(sPageID != SM_NULL_PID)
        {
            IDE_ASSERT( smmManager::getPersPagePtr( aSpaceID,
                                                    sPageID,
                                                    (void**)aPage )
                        == IDE_SUCCESS );
            *aNxtPiecePtr = (SChar *)((*aPage)->mBody + ID_SIZEOF(smpVarPageHeader));
            *aNxtPieceOID = SM_MAKE_OID(sPageID,
                                        (SChar*)(*aNxtPiecePtr) - (SChar*)((*aPage)));
        }
        else
        {
            /* Allocate�� �������� �������� �ʴ´�.*/
        }
    }
}

/**********************************************************************
 * FreePageHeader�� FreeSlot�߰�
 *
 * aPageID    : FreeSlot�߰��Ϸ��� PageID
 * aPiecePtr  : FreeSlot�� Row ������
 **********************************************************************/

void svpVarPageList::addFreeSlotToFreeSlotList( scSpaceID aSpaceID,
                                                scPageID  aPageID,
                                                SChar*    aPiecePtr )
{
    smpFreePageHeader *sFreePageHeader;
    smVCPieceHeader   *sCurVCPieceHdr;
    smVCPieceHeader   *sTailVCPieceHdr;

    IDE_DASSERT( aPageID != SM_NULL_PID );
    IDE_DASSERT( aPiecePtr != NULL );

    sFreePageHeader = svpFreePageList::getFreePageHeader(aSpaceID, aPageID);
    sCurVCPieceHdr = (smVCPieceHeader*)aPiecePtr;

    IDE_DASSERT( isValidFreeSlotList(sFreePageHeader) == ID_TRUE );

    sCurVCPieceHdr->nxtPieceOID = (smOID)NULL;
    sTailVCPieceHdr = (smVCPieceHeader*)sFreePageHeader->mFreeSlotTail;

    if(sTailVCPieceHdr == NULL)
    {
        IDE_DASSERT( sFreePageHeader->mFreeSlotHead == NULL );

        sFreePageHeader->mFreeSlotHead = aPiecePtr;
    }
    else
    {
        sTailVCPieceHdr->nxtPieceOID = (smOID)aPiecePtr;
    }

    sFreePageHeader->mFreeSlotTail = aPiecePtr;
    sFreePageHeader->mFreeSlotCount++;

    IDE_DASSERT( isValidFreeSlotList(sFreePageHeader) == ID_TRUE );

    return;
}

/**********************************************************************
 * PageList�� ��ȿ�� ���ڵ� ���� ��ȯ,
 * for X$TABLE_INFO, X$TEMP_TABLE_INFO 
 *
 * aVarEntry    : �˻��ϰ��� �ϴ� PageListEntry
 * return       : ��ȯ�ϴ� ���ڵ� ����
 **********************************************************************/
ULong svpVarPageList::getRecordCount( smpPageListEntry *aVarEntry )
{
    ULong sInsertCount;
    ULong sDeleteCount;

    IDE_DASSERT( aVarEntry != NULL );

    /* BUG-47706 insert, delete ��� �� ������ �ϴ� ������ 
     * delete �� ���� �ް� insert �� �޾ƾ� ������ ���� �ʴ´�. */
    sDeleteCount = idCore::acpAtomicGet64( &(aVarEntry->mRuntimeEntry->mDelRecCnt) );
    sInsertCount = idCore::acpAtomicGet64( &(aVarEntry->mRuntimeEntry->mInsRecCnt) );

    return sInsertCount - sDeleteCount;
}


/**********************************************************************
 * PageListEntry�� �ʱ�ȭ�Ѵ�.
 *
 * aVarEntry : �ʱ�ȭ�Ϸ��� PageListEntry
 * aTableOID : PageListEntry�� ���̺� OID
 **********************************************************************/
void svpVarPageList::initializePageListEntry( smpPageListEntry* aVarEntry,
                                              smOID             aTableOID )
{
    UInt i;

    IDE_DASSERT(aTableOID != 0);
    IDE_DASSERT(aVarEntry != NULL);

    /* BUG-26939 inefficient variable slot
     * Slot size is decided by slot counts(2^n -> basic slots).
     * avalable space(which in 1 page) / slot count = slot size
     * new median slots have inserted between basic slots.
     * Basic slots have even slot numbers (sEven).
     * And median slots have odd slot numbers (sOdd).
     */
#if defined(DEBUG)
    UInt sEven;
    UInt sOdd;
    UInt sCount;
    UInt sSlotSize;

    sEven   = 1 << ( SM_VAR_PAGE_LIST_COUNT / 2 );
    sOdd    = ( sEven + ( sEven >> 1 )) / 2 ;
    
    for( i = 0; i < SM_VAR_PAGE_LIST_COUNT; i++ )
    {
        switch( i%2 )
        {
            case 0 :
                sCount = sEven;
                sEven >>= 1;
                break;
            case 1 : 
                sCount = sOdd;
                sOdd >>= 1;
                break;
            default : 
                /* do nothing */
                break;
        }

        sSlotSize = ((SMP_PERS_PAGE_BODY_SIZE - ID_SIZEOF(smpVarPageHeader))
                / sCount) - ID_SIZEOF(smVCPieceHeader);
        /* 8 byte Align */
        sSlotSize -= (sSlotSize % 8 );

        IDE_DASSERT( sSlotSize == gVarSlotSizeArray[i] );
    }
#endif


    for( i = 0; i < SM_VAR_PAGE_LIST_COUNT; i++)
    {
        aVarEntry[i].mSlotSize  = gVarSlotSizeArray[i] + ID_SIZEOF(smVCPieceHeader);
        aVarEntry[i].mSlotCount =
            (SMP_PERS_PAGE_BODY_SIZE - ID_SIZEOF(smpVarPageHeader))
            /aVarEntry[i].mSlotSize;

        aVarEntry[i].mTableOID     = aTableOID;
        aVarEntry[i].mRuntimeEntry = NULL;
    }

    return;
}

/**********************************************************************
 * VarPage�� VarIdx���� ���Ѵ�.
 *
 * aPagePtr : Idx���� �˰����ϴ� Page ������
 **********************************************************************/
UInt svpVarPageList::getVarIdx( void* aPagePtr )
{
    UInt sIdx;

    IDE_DASSERT( aPagePtr != NULL );

    sIdx = ((smpVarPageHeader *)((SChar *)aPagePtr +
                                 ID_SIZEOF(smpPersPageHeader)))->mIdx;

    IDE_DASSERT( sIdx < SM_VAR_PAGE_LIST_COUNT );

    return sIdx;
}

/**********************************************************************
 * aValue�� ���� VarEntry�� Idx���� ���Ѵ�.
 *
 * aLength : VarEntry�� �Է��� Value�� ����.
 * aIdx    : aValue�� ���� VarIdx��
 **********************************************************************/
IDE_RC svpVarPageList::calcVarIdx( UInt     aLength,
                                   UInt*    aVarIdx )
{
    IDE_DASSERT( aLength != 0 );
    IDE_DASSERT( aVarIdx != NULL );

    IDE_TEST_RAISE( aLength > gVarSlotSizeArray[SM_VAR_PAGE_LIST_COUNT -1],
                    too_long_var_item);

    *aVarIdx = (UInt)gAllocArray[aLength];

    return IDE_SUCCESS;

    IDE_EXCEPTION( too_long_var_item );
    {
        IDE_SET(ideSetErrorCode (smERR_ABORT_Too_Long_Var_Data));
    }

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

#ifdef DEBUG
/**********************************************************************
 * Page���� FreeSlotList�� ������ �ùٸ��� �˻��Ѵ�.
 *
 * aFreePageHeader : �˻��Ϸ��� FreeSlotList�� �ִ� Page�� FreePageHeader
 **********************************************************************/
idBool svpVarPageList::isValidFreeSlotList( smpFreePageHeader* aFreePageHeader )
{
    idBool                sIsValid;

    if ( iduProperty::getEnableRecTest() == 1 )
    {
        vULong                sPageCount = 0;
        smVCPieceHeader*      sCurFreeSlotHeader = NULL;
        smVCPieceHeader*      sNxtFreeSlotHeader;

        IDE_DASSERT( aFreePageHeader != NULL );

        sIsValid = ID_FALSE;

        sNxtFreeSlotHeader = (smVCPieceHeader*)aFreePageHeader->mFreeSlotHead;

        while(sNxtFreeSlotHeader != NULL)
        {
            sCurFreeSlotHeader = sNxtFreeSlotHeader;

            sPageCount++;

            sNxtFreeSlotHeader =
                (smVCPieceHeader*)sCurFreeSlotHeader->nxtPieceOID;
        }

        if(aFreePageHeader->mFreeSlotCount == sPageCount &&
           aFreePageHeader->mFreeSlotTail  == (SChar*)sCurFreeSlotHeader)
        {
            sIsValid = ID_TRUE;
        }

        if ( sIsValid == ID_FALSE )
        {
            ideLog::log(SM_TRC_LOG_LEVEL_MPAGE,
                        SM_TRC_MPAGE_INVALID_FREE_SLOT_LIST1,
                        (ULong)aFreePageHeader->mSelfPageID);
            ideLog::log(SM_TRC_LOG_LEVEL_MPAGE,
                        SM_TRC_MPAGE_INVALID_FREE_SLOT_LIST2,
                        aFreePageHeader->mFreeSlotCount);
            ideLog::log(SM_TRC_LOG_LEVEL_MPAGE,
                        SM_TRC_MPAGE_INVALID_FREE_SLOT_LIST3,
                        sPageCount);
            ideLog::log(SM_TRC_LOG_LEVEL_MPAGE,
                        SM_TRC_MPAGE_INVALID_FREE_SLOT_LIST4,
                        aFreePageHeader->mFreeSlotHead);
            ideLog::log(SM_TRC_LOG_LEVEL_MPAGE,
                        SM_TRC_MPAGE_INVALID_FREE_SLOT_LIST5,
                        aFreePageHeader->mFreeSlotTail);
            ideLog::log(SM_TRC_LOG_LEVEL_MPAGE,
                        SM_TRC_MPAGE_INVALID_FREE_SLOT_LIST6,
                        sCurFreeSlotHeader);

#    if defined(TSM_DEBUG)
            idlOS::printf( "Invalid Free Slot List Detected. Page #%"ID_UINT64_FMT"\n",
                           (ULong) aFreePageHeader->mSelfPageID );
            idlOS::printf( "Free Slot Count on Page ==> %"ID_UINT64_FMT"\n",
                           aFreePageHeader->mFreeSlotCount );
            idlOS::printf( "Free Slot Count on List ==> %"ID_UINT64_FMT"\n",
                           sPageCount );
            idlOS::printf( "Free Slot Head on Page ==> %"ID_xPOINTER_FMT"\n",
                           aFreePageHeader->mFreeSlotHead );
            idlOS::printf( "Free Slot Tail on Page ==> %"ID_xPOINTER_FMT"\n",
                           aFreePageHeader->mFreeSlotTail );
            idlOS::printf( "Free Slot Tail on List ==> %"ID_xPOINTER_FMT"\n",
                           sCurFreeSlotHeader );
            fflush( stdout );
#    endif // TSM_DEBUG
        }
    }
    else
    {
        sIsValid = ID_TRUE;
    }

    return sIsValid;
}
#endif

/**********************************************************************
 *
 * Description: calcVarIdx �� ������ �ϱ� ���� gAllocArray�� �����Ѵ�.
 *              �� ������� �ش�Ǵ� �ε����� �̸� �����س���
 *              ������� �ٷ� �ε����� ã�� �� �ְ� �Ѵ�.
 *
 **********************************************************************/
void svpVarPageList::initAllocArray()
{
    UInt sIdx;
    UInt sSize;

    for ( sIdx = 0, sSize = 0; sIdx < SM_VAR_PAGE_LIST_COUNT; sIdx++ )
    {
        for (; sSize <= gVarSlotSizeArray[sIdx]; sSize++ )
        {
            gAllocArray[sSize] = sIdx;
        }
    }

    IDE_DASSERT( ( sSize - 1) == ( SMP_PERS_PAGE_BODY_SIZE
                                 - ID_SIZEOF(smpVarPageHeader)
                                 - ID_SIZEOF(smVCPieceHeader))
               );

}

/***********************************************************************
 * slot size�� �ٸ� FreePagePool���� �� �������� �ִ��� Ȯ���Ѵ�.
 * �� �������� �ִٸ� ���� FreePageList�� �� �������� �޾ƿ´�.
 *
 * aSpaceID       : TableSpace ID
 * aVarEntryArray : PageListEntry Array 
 * aDstIdx        : Free Page�� �޾ƿ����� FreePageList�� PageListEntry index
 * aPageListID    : PageListID
 * aIsPageAlloced : �� �������� �޾ƿԴ��� ����
 ***********************************************************************/
void svpVarPageList::tryForAllocPagesFromOtherPools( scSpaceID          aSpaceID,
                                                     smpPageListEntry * aVarEntryArray,
                                                     UInt               aDstIdx,
                                                     UInt               aPageListID,
                                                     idBool           * aIsPageAlloced )
{
    UInt                   sState           = 0;
    UInt                   sIdx             = 0;
    idBool                 sIsPageAlloced   = ID_FALSE;
    smpPageListEntry     * sSrcEntry        = NULL;
    smpFreePagePoolEntry * sSrcPool         = NULL;
    smpPageListEntry     * sDstEntry        = NULL;

    IDE_DASSERT( aVarEntryArray != NULL );
    IDE_DASSERT( aDstIdx < SM_VAR_PAGE_LIST_COUNT );
    IDE_DASSERT( aPageListID < SMP_PAGE_LIST_COUNT );
    IDE_DASSERT( aIsPageAlloced != NULL );

    sDstEntry = aVarEntryArray + aDstIdx;

    for ( sIdx = 0;
          sIdx < SM_VAR_PAGE_LIST_COUNT;
          sIdx++ )
    {
        sSrcEntry = aVarEntryArray + sIdx;

        if ( sSrcEntry == sDstEntry )
        {
            /* ������ Page List Entry�� ��� skip �Ѵ�. */ 
            continue;
        }

        sSrcPool = &(sSrcEntry->mRuntimeEntry->mFreePagePool);

        IDE_DASSERT( sSrcPool != NULL );

        if ( sSrcPool->mPageCount >= SMP_MOVEPAGECOUNT_POOL2LIST )
        {
            /* Pool�� ������ ���� ������ ��ŭ �ִ��� ����,
               Pool�� lock�� ��� �ٽ� Ȯ���Ѵ�. List�� lock�� ���� �Լ����� ��´�. */
            (void)(sSrcPool->mMutex.lock( NULL /* idvSQL* */ ));
            sState = 1;

            if ( sSrcPool->mPageCount >= SMP_MOVEPAGECOUNT_POOL2LIST )
            {
                getPagesFromOtherPool( aSpaceID,
                                       sSrcPool,
                                       sDstEntry,
                                       aDstIdx,
                                       aPageListID,
                                       &sState );

                sIsPageAlloced = ID_TRUE;
                break;
            }

            if ( sState == 1 )
            {
                sState = 0;
                (void)(sSrcPool->mMutex.unlock());
            }
        }
    }

    if ( sState == 1 )
    {
        (void)(sSrcPool->mMutex.unlock());
    }

    *aIsPageAlloced = sIsPageAlloced;
}

/***********************************************************************
 * Ư�� slot size�� FreePagePool(source)�� ����������
 * �ٸ� slot size�� FreePageList(destination)�� �̵���Ų��.
 *
 * ���� : �� �Լ� ȣ������ aSrcPool LOCK �� ��ƾ��Ѵ�.
 *
 * aSpaceID          : TableSpace ID
 * aSrcPool          : ���������� ���� �ִ� source�� �Ǵ� FreePagePool
 * aDstEntry         : target�� �Ǵ� PageListEntry
 * aDstIdx           : target�� �Ǵ� PageListEntry�� index 
 * aPageListID       : PageListID
 * aSrcPoolLockState : source�� �Ǵ� FreePagePool�� LOCK ����.
 ***********************************************************************/
void svpVarPageList::getPagesFromOtherPool( scSpaceID              aSpaceID,
                                            smpFreePagePoolEntry * aSrcPool,
                                            smpPageListEntry     * aDstEntry,
                                            UInt                   aDstIdx,
                                            UInt                   aPageListID,
                                            UInt                 * aSrcPoolLockState )
{
    UInt                   sPageCounter;
    UInt                   sAssignCount;
    smpFreePageHeader    * sNode    = NULL;
    smpFreePageListEntry * sDstList = NULL;

    smpFreePageHeader    * sSrcPoolHead = NULL;
    smpFreePageHeader    * sMovHead = NULL;
    smpFreePageHeader    * sMovTail = NULL;

    UInt                   sEmptySizeClassID;
    UInt                   sSrcPoolLockState;
    smpPersPage          * sPagePtr = NULL;

    IDE_DASSERT( aSrcPool != NULL );
    IDE_DASSERT( aDstEntry != NULL );
    IDE_DASSERT( aDstIdx < SM_VAR_PAGE_LIST_COUNT );
    IDE_DASSERT( aPageListID < SMP_PAGE_LIST_COUNT );
    IDE_DASSERT( ( aSrcPoolLockState != NULL ) && 
                 ( *aSrcPoolLockState == 1 ) );

    sSrcPoolLockState = *aSrcPoolLockState;

    sEmptySizeClassID = SMP_EMPTYPAGE_CLASSID( aDstEntry->mRuntimeEntry );
    sAssignCount      = SMP_MOVEPAGECOUNT_POOL2LIST;

    sDstList = &(aDstEntry->mRuntimeEntry->mFreePageList[aPageListID]);

    IDE_DASSERT( sDstList != NULL );

    IDE_DASSERT( svpFreePageList::isValidFreePageList( aSrcPool->mHead,
                                                       aSrcPool->mTail,
                                                       aSrcPool->mPageCount )
                 == ID_TRUE );

    /********************************************************/
    /* 1. source�� FreePagePool �պκп��� ���������� ����. */
    /********************************************************/

    sSrcPoolHead = aSrcPool->mHead;
    sMovHead     = aSrcPool->mHead;

    IDE_DASSERT( sAssignCount >= 1 );

    for( sPageCounter = 0;
         sPageCounter < sAssignCount;
         sPageCounter++ )
    {
        sMovTail     = sSrcPoolHead;
        sSrcPoolHead = sSrcPoolHead->mFreeNext;
    }

    if ( sSrcPoolHead == NULL )
    {
        /* pool�� �����մ� �������� ���°�� */

        sMovTail->mFreeNext = NULL;

        aSrcPool->mTail = NULL;
        aSrcPool->mHead = NULL;
    }
    else
    {
        sMovTail->mFreeNext     = NULL;
        sSrcPoolHead->mFreePrev = NULL;

        aSrcPool->mHead = sSrcPoolHead;
    }

    aSrcPool->mPageCount -= sAssignCount;

    IDE_DASSERT( svpFreePageList::isValidFreePageList( aSrcPool->mHead,
                                                       aSrcPool->mTail,
                                                       aSrcPool->mPageCount )
                 == ID_TRUE );

    /* Source FreePagePool UNLOCK */
    if ( sSrcPoolLockState == 1 )
    {
        sSrcPoolLockState = 0;
        (void)(aSrcPool->mMutex.unlock());
    }

    /********************************************************/
    /* 2. source���� �� ���������� ������ dest�� ������ �����Ѵ�. */
    /********************************************************/

    for ( sNode = sMovHead;
          sNode != NULL;
          sNode = sNode->mFreeNext )
    {
        sNode->mFreeListID  = aPageListID;
        sNode->mSizeClassID = sEmptySizeClassID;

        IDE_ASSERT( smmManager::getPersPagePtr( aSpaceID,
                                                sNode->mSelfPageID,
                                                (void**)&sPagePtr )
                    == IDE_SUCCESS );

        initializePage( aDstIdx,
                        aPageListID,
                        aDstEntry->mSlotSize,
                        aDstEntry->mSlotCount,
                        aDstEntry->mTableOID,
                        sPagePtr );

        svpFreePageList::initializeFreeSlotListAtPage( aSpaceID,
                                                       aDstEntry,
                                                       sPagePtr );
    }

    /********************************************************/
    /* 3. dest�� FreePageList�� �޺κп� �����Ѵ�. */
    /********************************************************/

    /* Destination FreePageList LOCK */
    (void)(sDstList->mMutex.lock(NULL /* idvSQL* */) );

    if ( sDstList->mTail[sEmptySizeClassID] == NULL )
    {
        IDE_DASSERT( sDstList->mHead[sEmptySizeClassID] == NULL );

        sDstList->mHead[sEmptySizeClassID] = sMovHead;
    }
    else
    {
        IDE_DASSERT( sDstList->mHead[sEmptySizeClassID] != NULL );

        sDstList->mTail[sEmptySizeClassID]->mFreeNext = sMovHead;
        sMovHead->mFreePrev = sDstList->mTail[sEmptySizeClassID];
    }
    sDstList->mTail[sEmptySizeClassID] = sMovTail;

    sDstList->mPageCount[sEmptySizeClassID] += sAssignCount;

    IDE_DASSERT( svpFreePageList::isValidFreePageList( sDstList->mHead[sEmptySizeClassID],
                                                       sDstList->mTail[sEmptySizeClassID],
                                                       sDstList->mPageCount[sEmptySizeClassID])
                 == ID_TRUE );

    /* Destination FreePageList UNLOCK */
    (void)(sDstList->mMutex.unlock());

    *aSrcPoolLockState = sSrcPoolLockState;
}

