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
 * $Id: smpVarPageList.cpp 88917 2020-10-15 04:54:02Z et16 $
 **********************************************************************/

#include <idl.h>
#include <ide.h>
#include <smErrorCode.h>
#include <smm.h>
#include <smr.h>
#include <smpDef.h>
#include <smpVarPageList.h>
#include <smpAllocPageList.h>
#include <smpFreePageList.h>
#include <smpReq.h>

/* BUG-26939 
 * �� �迭�� ������ ���� ����ü�� ũ�� �������� ���� �޶��� �� �ִ�.
 *      SMP_PERS_PAGE_BODY_SIZE, smpVarPageHeader, smVCPieceHeader.
 * ���� ������ bit �� ���� ����ü ũ�Ⱑ �޶����Ƿ� �ٸ��� �����Ѵ�.
 * debug mode���� �� ũ�Ⱑ ������� �ʴ��� initializePageListEntry���� �˻��Ѵ�.
 * �� ��Ʈ�� �����ȴٸ� svp�� �Ȱ��� �ڵ尡 �����Ƿ� �����ϰ� �����ؾ��Ѵ�.
 * ���� �Ʒ��� ���� ������ �ڼ��� ������ NOK Variable Page slots ���� ����
 */
/* BUG-43379
 * smVCPieceHeader�� ũ��� slot size�� page�� �ʱ�ȭ ��ų �� ���Խ�Ų��.���� 
 * allocSlot �� ���� ����� �ʿ䰡 ����. �ڼ��� ������ initializePageListEntry�Լ���
 * initializePage �Լ��� ����.   
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
IDE_RC smpVarPageList::setRuntimeNull( UInt              aVarEntryCount,
                                       smpPageListEntry* aVarEntryArray )
{
    UInt i;

    IDE_DASSERT( aVarEntryArray != NULL );

    for ( i=0; i<aVarEntryCount; i++)
    {
        // RuntimeEntry �ʱ�ȭ
        IDE_TEST(smpFreePageList::setRuntimeNull( & aVarEntryArray[i] )
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
IDE_RC smpVarPageList::initEntryAtRuntime(
    smOID                  aTableOID,
    smpPageListEntry*      aVarEntry,
    smpAllocPageListEntry* aAllocPageList )
{
    UInt i;

    IDE_DASSERT( aVarEntry != NULL );
    IDE_DASSERT( aAllocPageList != NULL );

    IDE_ASSERT( aTableOID == aVarEntry->mTableOID );

    for( i = 0; i < SM_VAR_PAGE_LIST_COUNT ; i++ )
    {
        // RuntimeEntry �ʱ�ȭ
        IDE_TEST(smpFreePageList::initEntryAtRuntime( &(aVarEntry[i]) )
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
IDE_RC smpVarPageList::finEntryAtRuntime( smpPageListEntry* aVarEntry )
{
    UInt i;
    UInt sPageListID;

    IDE_DASSERT( aVarEntry != NULL );

    if ( aVarEntry->mRuntimeEntry != NULL )
    {
        for( sPageListID = 0;
             sPageListID < SMP_PAGE_LIST_COUNT;
             sPageListID++ )
        {
            // AllocPageList�� Mutex ����
            IDE_TEST( smpAllocPageList::finEntryAtRuntime(
                          &(aVarEntry->mRuntimeEntry->
                              mAllocPageList[sPageListID]) )
                      != IDE_SUCCESS );
        }
    }
    else
    {
        // Do Nothing
        // OFFLINE/DISCARD�� Table�� ��� mRuntimeEntry�� NULL�� ���� �ִ�.
    }

    for( i = 0; i < SM_VAR_PAGE_LIST_COUNT; i++ )
    {
        // RuntimeEntry ����
        IDE_TEST(smpFreePageList::finEntryAtRuntime(&(aVarEntry[i]))
                 != IDE_SUCCESS);
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
IDE_RC smpVarPageList::freePageListToDB(void*             aTrans,
                                        scSpaceID         aSpaceID,
                                        smOID             aTableOID,
                                        smpPageListEntry* aVarEntry )
{
    UInt i;
    UInt sPageListID;

    IDE_DASSERT( aTrans != NULL );
    IDE_DASSERT( aVarEntry != NULL );
    IDE_DASSERT( aTableOID == aVarEntry->mTableOID );

    for( i = 0; i < SM_VAR_PAGE_LIST_COUNT; i++ )
    {
        // FreePageList ����
        smpFreePageList::initializeFreePageListAndPool(&(aVarEntry[i]));
    }

    for( sPageListID = 0;
         sPageListID < SMP_PAGE_LIST_COUNT;
         sPageListID++ )
    {
        // for AllocPageList
        IDE_TEST( smpAllocPageList::freePageListToDB(
                      aTrans,
                      aSpaceID,
                      &(aVarEntry->mRuntimeEntry->mAllocPageList[sPageListID]),
                      aTableOID )
                  != IDE_SUCCESS );
    }


    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC smpVarPageList::setValue(scSpaceID       aSpaceID,
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

    return smmDirtyPageMgr::insDirtyPage(aSpaceID, SM_MAKE_PID(aPieceOID));
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
SChar* smpVarPageList::getValue( scSpaceID       aSpaceID,
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

    sRet = smpVarPageList::getPieceValuePtr( sVCPieceHeader, sPos );

    /* ����Ÿ�� ���̰� SMP_VC_PIECE_MAX_SIZE���� �۰ų� ���� ���� ����Ÿ
     * �� �������� ���ļ� ������� �ʰ� �ϳ��� �������� ���ٸ� */
    if ( (( sPos + aReadLen)  > SMP_VC_PIECE_MAX_SIZE) 
            && ( aBuffer != NULL ))
    {
        sRemainedReadSize = aReadLen;
        sRowBuffer = aBuffer;

        sRet = aBuffer;
        while( sRemainedReadSize > (SLong)SMP_VC_PIECE_MAX_SIZE )
        {
            sReadPieceSize = sVCPieceHeader->length - sPos;
            IDE_ASSERT( sReadPieceSize > 0 );

            sPos = 0;

            idlOS::memcpy(
                sRowBuffer,
                smpVarPageList::getPieceValuePtr( sVCPieceHeader, sPos ),
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
                smpVarPageList::getPieceValuePtr( sVCPieceHeader, sPos ),
                sRemainedReadSize );
            sRowBuffer += sRemainedReadSize;
            sRemainedReadSize = 0;
        }

        /* ���ۿ� ����� ����Ÿ�� ���̴� ���� ����Ÿ�� ���̿�
         * �����ؾ� �մϴ�. */
        IDE_ASSERT( (UInt)(sRowBuffer - aBuffer) == aReadLen );
    }
    else
    {
        /* Nothing to do */
    }

    return sRet;
}

/**********************************************************************
 * BUG-31206    improve usability of DUMPCI and DUMPDDF
 *
 * Varialbe Column�� ���� Slot Header�� altibase_sm.log�� �����Ѵ�
 *
 * aVarSlotHeader : dump�� slot ���
 **********************************************************************/
void smpVarPageList::dumpVCPieceHeader( smVCPieceHeader *aVCPieceHeader )
{
    ideLog::log(SM_TRC_LOG_LEVEL_FATAL,
                SM_TRC_MPAGE_DUMP_VAR_COL_HEAD,
                (ULong)aVCPieceHeader->nxtPieceOID,
                aVCPieceHeader->length,
                aVCPieceHeader->flag);
}


/**********************************************************************
 * BUG-31206 improve usability of DUMPCI and DUMPDDF
 *           Slot Header�� altibase_sm.log�� �����Ѵ�
 *
 * VCPieceHeader�� �����Ѵ�
 *
 * aVCPieceHeader : dump�� slot ��� 
 * aDisplayTable  : Table���·� ǥ���� ���ΰ�?
 * aOutBuf        : ��� ����
 * aOutSize       : ��� ������ ũ��
 **********************************************************************/
void smpVarPageList::dumpVCPieceHeaderByBuffer( 
    smVCPieceHeader     * aVCPHeader,
    idBool                aDisplayTable,
    SChar               * aOutBuf,
    UInt                  aOutSize )
{   
    if( aDisplayTable == ID_FALSE )
    {
        idlVA::appendFormat( aOutBuf,
                             aOutSize,
                             "nxtPieceOID   : %"ID_vULONG_FMT"\n"
                             "flag          : %"ID_UINT32_FMT"\n"
                             "length        : %"ID_UINT32_FMT"\n",
                             aVCPHeader->nxtPieceOID,
                             aVCPHeader->flag,
                             aVCPHeader->length );
    }
    else
    {
        idlVA::appendFormat( aOutBuf,
                             aOutSize,
                             "%-16"ID_vULONG_FMT
                             "%-16"ID_UINT32_FMT
                             "%-16"ID_UINT32_FMT,
                             aVCPHeader->nxtPieceOID,
                             aVCPHeader->flag,
                             aVCPHeader->length );
    }
}


/**********************************************************************
 * system���κ��� persistent page�� �Ҵ�޴´�.
 *
 * aTrans    : �۾��ϴ� Ʈ����� ��ü
 * aVarEntry : �Ҵ���� PageListEntry
 * aIdx      : �Ҵ���� VarSlot ũ�� idx
 **********************************************************************/
IDE_RC smpVarPageList::allocPersPages( void*             aTrans,
                                       scSpaceID         aSpaceID,
                                       smpPageListEntry* aVarEntry,
                                       UInt              aIdx )
{
    UInt                    sState = 0;
    smLSN                   sNTA;
    smOID                   sTableOID;
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

    sTableOID      = aVarEntry->mTableOID;

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

    IDE_TEST( smLayerCallback::findPrivatePageList( aTrans,
                                                    aVarEntry->mTableOID,
                                                    &sPrivatePageList )
              != IDE_SUCCESS );

    if(sPrivatePageList == NULL)
    {
        // ������ PrivatePageList�� �����ٸ� ���� �����Ѵ�.
        IDE_TEST( smLayerCallback::createPrivatePageList(
                      aTrans,
                      aVarEntry->mTableOID,
                      &sPrivatePageList )
                  != IDE_SUCCESS );
    }

    // Begin NTA[-1-]
    sNTA = smLayerCallback::getLstUndoNxtLSN( aTrans );
    sState = 1;

    // [1] smmManager�κ��� page �Ҵ�
    IDE_TEST( smmManager::allocatePersPageList( aTrans,
                                                aSpaceID,
                                                SMP_ALLOCPAGECOUNT_FROMDB,
                                                (void **)&sAllocPageHead,
                                                (void **)&sAllocPageTail,
                                                &sAllocPageCnt )
              != IDE_SUCCESS);

    IDE_DASSERT( smpAllocPageList::isValidPageList(
                     aSpaceID,
                     sAllocPageHead->mHeader.mSelfPageID,
                     sAllocPageTail->mHeader.mSelfPageID,
                     sAllocPageCnt )
                 == ID_TRUE );

    // �Ҵ���� HeadPage�� PrivatePageList�� ����Ѵ�.
    IDE_DASSERT( sPrivatePageList->mVarFreePageHead[aIdx] == NULL );

    sPrivatePageList->mVarFreePageHead[aIdx] =
        smpFreePageList::getFreePageHeader(aSpaceID,
                                           sAllocPageHead->mHeader.mSelfPageID);

    // [2] page �ʱ�ȭ
    sNextPageID = sAllocPageHead->mHeader.mSelfPageID;

    while(sNextPageID != SM_NULL_PID)
    {
        IDE_ASSERT( smmManager::getPersPagePtr( aSpaceID,
                                                sNextPageID,
                                                (void**)&sPagePtr )
                    == IDE_SUCCESS );
        sState = 3;

        IDE_TEST( smrUpdate::initVarPage(NULL, /* idvSQL* */
                                         aTrans,
                                         aSpaceID,
                                         sPagePtr->mHeader.mSelfPageID,
                                         sPageListID,
                                         aIdx,
                                         aVarEntry->mSlotSize,
                                         aVarEntry->mSlotCount,
                                         aVarEntry->mTableOID)
                  != IDE_SUCCESS);


        // PersPageHeader �ʱ�ȭ�ϰ� (FreeSlot���� �����Ѵ�.)
        initializePage( aIdx,
                        sPageListID,
                        aVarEntry->mSlotSize,
                        aVarEntry->mSlotCount,
                        aVarEntry->mTableOID,
                        sPagePtr );

        sState = 2;
        IDE_TEST( smmDirtyPageMgr::insDirtyPage(aSpaceID,
                                                sPagePtr->mHeader.mSelfPageID)
                  != IDE_SUCCESS );

        // FreePageHeader �ʱ�ȭ�ϰ�
        smpFreePageList::initializeFreePageHeader(
            smpFreePageList::getFreePageHeader(aSpaceID, sPagePtr) );

        // FreeSlotList�� Page�� ����Ѵ�.
        smpFreePageList::initializeFreeSlotListAtPage( aSpaceID,
                                                       aVarEntry,
                                                       sPagePtr );

        sNextPageID = sPagePtr->mHeader.mNextPageID;

        // FreePageHeader�� PrivatePageList�� �����Ѵ�.
        // PrivatePageList�� Var������ �ܹ��⸮��Ʈ�̱⶧���� Prev�� NULL�� ���õȴ�.
        smpFreePageList::addFreePageToPrivatePageList( aSpaceID,
                                                       sPagePtr->mHeader.mSelfPageID,
                                                       SM_NULL_PID,  // Prev
                                                       sNextPageID );
    }

    // [3] AllocPageList ���
    IDE_TEST( sAllocPageList->mMutex->lock(NULL /* idvSQL* */) != IDE_SUCCESS );
    sState = 2;

    IDE_TEST( smpAllocPageList::addPageList( aTrans,
                                             aSpaceID,
                                             sAllocPageList,
                                             sTableOID,
                                             sAllocPageHead,
                                             sAllocPageTail,
                                             sAllocPageCnt )
              != IDE_SUCCESS);

    // End NTA[-1-]
    IDE_TEST( smrLogMgr::writeNTALogRec(NULL, /* idvSQL* */
                                        aTrans,
                                        &sNTA,
                                        SMR_OP_NULL)
              != IDE_SUCCESS );

    sState = 0;
    IDE_TEST(sAllocPageList->mMutex->unlock() != IDE_SUCCESS);


    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    IDE_PUSH();

    switch(sState)
    {
        case 3:
            IDE_ASSERT( smmDirtyPageMgr::insDirtyPage(aSpaceID,
                                                      sPagePtr->mHeader.mSelfPageID)
                        == IDE_SUCCESS );

        case 2:
            // DB���� TAB���� Page���� �����Դµ� ��ó TAB�� ���� ���ߴٸ� �ѹ�
            IDE_ASSERT( smrRecoveryMgr::undoTrans( NULL, /* idvSQL* */
                                                   aTrans,
                                                   &sNTA )
                        == IDE_SUCCESS );
            IDE_ASSERT( sAllocPageList->mMutex->unlock() == IDE_SUCCESS );
            break;

        case 1:
            // DB���� TAB���� Page���� �����Դµ� ��ó TAB�� ���� ���ߴٸ� �ѹ�
            IDE_ASSERT( smrRecoveryMgr::undoTrans( NULL, /* idvSQL* */
                                                   aTrans,
                                                   &sNTA )
                        == IDE_SUCCESS );
            break;

        default:
            break;
    }

    IDE_POP();


    return IDE_FAILURE;
}

/***********************************************************************
 * var slot�� �Ҵ��Ѵ�.
 *
 * aTrans       : �۾��Ϸ��� Ʈ����� ��ü
 * aTableOID    : �Ҵ��Ϸ��� ���̺��� OID
 * aVarEntry    : �Ҵ��Ϸ��� PageListEntry
 * aPieceSize   : Variable Column�� ������ Piece�� ũ��
 * aNxtPieceOID : �Ҵ��� Piece�� nextPieceOID
 * aPieceOID    : �Ҵ�� Piece�� OID
 * aPiecePtr    : �Ҵ��ؼ� ��ȯ�Ϸ��� Row ������
 * aPieceType   : �Ҵ���� Piece�� Type
 * aWriteLog    : Alloc Slot ����� Log�� ������� ����, �⺻�� ID_TRUE (BUG-47366)
 ***********************************************************************/
IDE_RC smpVarPageList::allocSlot( void*              aTrans,
                                  scSpaceID          aSpaceID,
                                  smOID              aTableOID,
                                  smpPageListEntry*  aVarEntry,
                                  UInt               aPieceSize,
                                  smOID              aNxtPieceOID,
                                  smOID*             aPieceOID,
                                  SChar**            aPiecePtr,
                                  UInt               aPieceType,
                                  idBool             aWriteLog )
{
    UInt              sState = 0;
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

    sState = 1;

    // BUG-47366 UnitedVar�� AllocSlot Log ����
    if ( aWriteLog == ID_TRUE )
    {
        //update var row head
        sAfterVCPieceHeader = *sCurVCPieceHeader;
        sAfterVCPieceHeader.flag &= ~SM_VCPIECE_FREE_MASK;
        sAfterVCPieceHeader.flag |= SM_VCPIECE_FREE_NO;
        sAfterVCPieceHeader.flag &= ~SM_VCPIECE_TYPE_MASK;
        sAfterVCPieceHeader.flag |= aPieceType;

        /* BUG-15354: [A4] SM VARCHAR 32K: Varchar����� PieceHeader�� ���� logging��
         * �����Ǿ� PieceHeader�� ���� Redo, Undo�� ��������. */
        sAfterVCPieceHeader.length      = aPieceSize;
        sAfterVCPieceHeader.nxtPieceOID = aNxtPieceOID;

        IDE_TEST( smrUpdate::updateVarRowHead( NULL, /* idvSQL* */
                                               aTrans,
                                               aSpaceID,
                                               *aPieceOID,
                                               sCurVCPieceHeader,
                                               &sAfterVCPieceHeader )
                  != IDE_SUCCESS);

        *sCurVCPieceHeader = sAfterVCPieceHeader;
    }
    else
    {
        // VC Piece�� Refine �ÿ� Free Mask�� Ȯ����.
        // length�� nxtPieceOID�� �ϴ� ��������.
        // refine �ܰ迡�� nxtPieceOID�� Free �̸� NULL�� ������ ��.
        // length�� ��� UnitedVar������ alloc �Ŀ� ColCnt�� �������
        sAfterVCPieceHeader             = *sCurVCPieceHeader;
        sAfterVCPieceHeader.length      = aPieceSize;
        sAfterVCPieceHeader.nxtPieceOID = aNxtPieceOID;

        *sCurVCPieceHeader = sAfterVCPieceHeader;
    }

    sState = 0;

    IDE_TEST(smmDirtyPageMgr::insDirtyPage( aSpaceID, SM_MAKE_PID(*aPieceOID) )
             != IDE_SUCCESS);

    // Record Count ���� // BUG-47706
    idCore::acpAtomicInc64( &(sVarPageList->mRuntimeEntry->mInsRecCnt) );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    IDE_PUSH();

    switch(sState)
    {
        case 1:
            IDE_ASSERT( smmDirtyPageMgr::insDirtyPage(
                            aSpaceID,
                            SM_MAKE_PID(*aPieceOID) )
                        == IDE_SUCCESS );
            break;

        default:
            break;
    }

    IDE_POP();


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

IDE_RC smpVarPageList::tryForAllocSlotFromPrivatePageList(
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

    IDE_TEST( smLayerCallback::findPrivatePageList( aTrans,
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

                smpFreePageList::initializeFreePageHeader(sFreePageHeader);
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
 * aTrans         : �۾��ϴ� Ʈ����� ��ü
 * aSpaceID       : TableSpace ID
 * aVarEntryArray : PageListEntry Array 
 * aIdx           : Slot�� �Ҵ��Ϸ��� PageListEntry Array�� index
 * aPageListID    : Slot�� �Ҵ��Ϸ��� PageListID
 * aPieceOID      : �Ҵ��ؼ� ��ȯ�Ϸ��� Piece OID
 * aPiecePtr      : �Ҵ��ؼ� ��ȯ�Ϸ��� Piece Ptr
 ***********************************************************************/
IDE_RC smpVarPageList::tryForAllocSlotFromFreePageList( void              * aTrans,
                                                        scSpaceID           aSpaceID,
                                                        smpPageListEntry  * aVarEntryArray,
                                                        UInt                aIdx,
                                                        UInt                aPageListID,
                                                        smOID             * aPieceOID,
                                                        SChar            ** aPiecePtr )
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
                IDE_TEST(sFreePageHeader->mMutex.lock(NULL /* idvSQL* */)
                         != IDE_SUCCESS);
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
                    IDE_TEST(smpFreePageList::modifyPageSizeClass(
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
        IDE_TEST( smpFreePageList::tryForAllocPagesFromPool( sVarEntry,
                                                             aPageListID,
                                                             &sIsPageAlloced )
                  != IDE_SUCCESS );

        if ( sIsPageAlloced == ID_FALSE )
        {
            /* BUG-47358
               FreePagePool���� �������� �������� ���ߴٸ�,
               (Slot ũ�Ⱑ) �ٸ� Variable Page�� FreePagePool���� �������� �����´�. */
            tryForAllocPagesFromOtherPools( aTrans,
                                            aSpaceID,
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
 * aNTA       : NTA LSN
 * aTableType : Temp Table�� slot������ ���� ����
 ***********************************************************************/
IDE_RC smpVarPageList::freeSlot( void*             aTrans,
                                 scSpaceID         aSpaceID,
                                 smpPageListEntry* aVarEntry,
                                 smOID             aPieceOID,
                                 SChar*            aPiecePtr,
                                 smLSN*            aNTA,
                                 smpTableType      aTableType,
                                 smSCN             aSCN )
{

    UInt                sIdx;
    scPageID            sPageID;
    smpPageListEntry  * sVarPageList;
    SChar             * sPagePtr;

    IDE_DASSERT( ((aTrans != NULL) && (aNTA != NULL) &&
                  (aTableType == SMP_TABLE_NORMAL)) ||
                 ((aTrans == NULL) && (aNTA == NULL) &&
                  (aTableType == SMP_TABLE_TEMP)) );

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

    IDE_TEST( setFreeSlot(aTrans,
                          aSpaceID,
                          sPageID,
                          aPieceOID,
                          aPiecePtr,
                          aNTA,
                          aTableType )
              != IDE_SUCCESS);

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
void smpVarPageList::initializePage( vULong       aIdx,
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

    IDE_DASSERT( aIdx < SM_VAR_PAGE_LIST_COUNT );
    IDE_DASSERT( aPageListID < SMP_PAGE_LIST_COUNT );
    IDE_DASSERT( aSlotSize > 0 );
    IDE_DASSERT( aSlotCount > 0 );
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

    // fix BUG-26934 : [codeSonar] Null Pointer Dereference
    IDE_ASSERT( sCurVCFreePieceHeader != NULL );

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
 * aNTA        : NTA LSN
 * aTableType  : Temp���̺����� ����
 **********************************************************************/
IDE_RC smpVarPageList::setFreeSlot( void          * aTrans,
                                    scSpaceID       aSpaceID,
                                    scPageID        aPageID,
                                    smOID           aVCPieceOID,
                                    SChar         * aVCPiecePtr,
                                    smLSN         * aNTA,
                                    smpTableType    aTableType )
{
    smVCPieceHeader* sCurFreeVCPieceHdr;
    UInt sState = 0;

    IDE_DASSERT( ((aTrans != NULL) && (aNTA != NULL) &&
                  (aTableType == SMP_TABLE_NORMAL)) ||
                 ((aTrans == NULL) && (aNTA == NULL) &&
                  (aTableType == SMP_TABLE_TEMP)) );

    IDE_DASSERT( aPageID     != SM_NULL_PID );
    IDE_DASSERT( aVCPieceOID != SM_NULL_OID );
    IDE_DASSERT( aVCPiecePtr != NULL );

    ACP_UNUSED( aTrans );
    ACP_UNUSED( aVCPieceOID );
    ACP_UNUSED( aNTA );
    ACP_UNUSED( aTableType );
    
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
        sState = 1;
        sCurFreeVCPieceHdr = (smVCPieceHeader*)aVCPiecePtr;

        // Set Variable Column To be Free.
        sCurFreeVCPieceHdr->flag = SM_VCPIECE_FREE_OK;

        sState = 0;
        IDE_TEST( smmDirtyPageMgr::insDirtyPage(aSpaceID, aPageID)
                  != IDE_SUCCESS );

    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    IDE_PUSH();

    if(sState == 1)
    {
        IDE_ASSERT( smmDirtyPageMgr::insDirtyPage(aSpaceID, aPageID)
                    == IDE_SUCCESS );
    }

    IDE_POP();

    return IDE_FAILURE;
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

IDE_RC smpVarPageList::addFreeSlotPending( void*             aTrans,
                                           scSpaceID         aSpaceID,
                                           smpPageListEntry* aVarEntry,
                                           smOID             aPieceOID,
                                           SChar*            aPiecePtr )
{
    UInt               sState = 0;
    UInt               sIdx;
    scPageID           sPageID;
    smpFreePageHeader* sFreePageHeader;
    SChar            * sPagePtr;

    IDE_DASSERT(aVarEntry != NULL);
    IDE_DASSERT(aPiecePtr != NULL);

    sPageID         = SM_MAKE_PID(aPieceOID);
    IDE_ASSERT( smmManager::getPersPagePtr( aSpaceID,
                                            sPageID,
                                            (void**)&sPagePtr )
                == IDE_SUCCESS );
    sIdx            = getVarIdx( sPagePtr );
    sFreePageHeader = smpFreePageList::getFreePageHeader(aSpaceID, sPageID);

    IDE_ASSERT(sIdx < SM_VAR_PAGE_LIST_COUNT);

    IDE_TEST(sFreePageHeader->mMutex.lock(NULL /* idvSQL* */) != IDE_SUCCESS);
    sState = 1;

    // PrivatePageList������ FreeSlot���� �ʴ´�.
    IDE_ASSERT(sFreePageHeader->mFreeListID != SMP_PRIVATE_PAGELISTID);

    // FreeSlot�� FreeSlotList�� �߰�
    addFreeSlotToFreeSlotList(aSpaceID, sPageID, aPiecePtr);

    // FreeSlot�� �߰��� ���� SizeClass�� ����Ǿ����� Ȯ���Ͽ� �����Ѵ�.
    IDE_TEST(smpFreePageList::modifyPageSizeClass( aTrans,
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
void smpVarPageList::removeSlotFromFreeSlotList(
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
        dumpVCPieceHeader( sFreeVCPieceHdr );

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
 * aSpaceID       TablespaceID
 * aVarEntry      ��ȸ�Ϸ��� PageListEntry
 * aPieceOID      Current Piece OID
 * aPiecePtr      Current Piece Ptr
 * aPage          aPiecePtr�� ���� Page�� ã�Ƽ� ��ȯ
 * aNxtPieceOID   Next Piece OID
 * aPiecePtrPtr   Next Piece Ptr
 ***********************************************************************/
IDE_RC smpVarPageList::initForScan( scSpaceID          aSpaceID,
                                    smpPageListEntry * aVarEntry,
                                    smOID              aPieceOID,
                                    SChar            * aPiecePtr,
                                    smpPersPage     ** aPage,
                                    smOID            * aNxtPieceOID,
                                    SChar           ** aNxtPiecePtr )
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
        IDE_ERROR_MSG( smmManager::getPersPagePtr( aSpaceID,
                                                   SM_MAKE_PID(aPieceOID),
                                                   (void**)aPage )
                       == IDE_SUCCESS,
                       "TableOID : %"ID_UINT64_FMT"\n"
                       "SpaceID  : %"ID_UINT32_FMT"\n"
                       "PageID   : %"ID_UINT32_FMT"\n",
                       aVarEntry->mTableOID,
                       aSpaceID,
                       SM_MAKE_PID(aPieceOID) );
        sIdx     = getVarIdx( *aPage );
        *aNxtPiecePtr = aPiecePtr + aVarEntry[sIdx].mSlotSize;
        *aNxtPieceOID = aPieceOID + aVarEntry[sIdx].mSlotSize;
    }
    else
    {
        sPageID = smpManager::getFirstAllocPageID(aVarEntry);

        if(sPageID != SM_NULL_PID)
        {
            IDE_ERROR_MSG( smmManager::getPersPagePtr( aSpaceID,
                                                       sPageID,
                                                       (void**)aPage )
                           == IDE_SUCCESS,
                           "TableOID : %"ID_UINT64_FMT"\n"
                           "SpaceID  : %"ID_UINT32_FMT"\n"
                           "PageID   : %"ID_UINT32_FMT"\n",
                           aVarEntry->mTableOID,
                           aSpaceID,
                           sPageID );
            *aNxtPiecePtr = (SChar *)((*aPage)->mBody + ID_SIZEOF(smpVarPageHeader));
            *aNxtPieceOID = SM_MAKE_OID(sPageID,
                                        (SChar*)(*aNxtPiecePtr) - (SChar*)((*aPage)));
        }
        else
        {
            /* Allocate�� �������� �������� �ʴ´�.*/
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

/**********************************************************************
 * PageList���� ��� Row�� ��ȸ�ϸ鼭 ��ȿ�� Row�� ã���ش�.
 * VarEntry�� nextOIDall�� RefineDB���� ���ȴ�.
 *
 * aSpaceID       [IN]  TablespaceID
 * aVarEntry      [IN]  ��ȸ�Ϸ��� PageListEntry
 * aCurPieceOID   [IN]  ���������� ã�� Piece OID
 * aCurPiecePtr   [IN]  ���������� ã�� Piece Pointer.
 * aNxtPieceOID   [OUT] ���� ã�� Piece�� OID
 * aNxtPiecePtr   [OUT] ���� ã�� Piece�� Pointer
 * aIdx           [OUT] VarPage�� Index ( ũ�� ��ȣ )
 **********************************************************************/
IDE_RC smpVarPageList::nextOIDallForRefineDB( scSpaceID          aSpaceID,
                                              smpPageListEntry * aVarEntry,
                                              smOID              aCurPieceOID,
                                              SChar            * aCurPiecePtr,
                                              smOID            * aNxtPieceOID,
                                              SChar           ** aNxtPiecePtr,
                                              UInt             * aIdx)
{
    scPageID           sNxtPID;
    smpPersPage*       sPage;
    SChar*             sNxtVCPiecePtr;
    smOID              sNxtVCPieceOID;
    SChar*             sFence;
    smpFreePageHeader* sFreePageHeader;
    UInt               sIdx = 0;
    UInt               sVCPieceSize;

    IDE_DASSERT( aVarEntry != NULL );
    IDE_DASSERT( aNxtPiecePtr != NULL );

    IDE_TEST( initForScan( aSpaceID,
                           aVarEntry,
                           aCurPieceOID,
                           aCurPiecePtr,
                           &sPage,
                           &sNxtVCPieceOID,
                           &sNxtVCPiecePtr )
              != IDE_SUCCESS );

    *aNxtPiecePtr = NULL;
    *aNxtPieceOID = SM_NULL_OID;

    while(sPage != NULL)
    {
        sFence = (SChar*)sPage + ID_SIZEOF(smpPersPage);

        // ���� refine�� slot�� Page�� VarIdx ���� ���ؿ´�.
        sIdx = getVarIdx(sPage);
        IDE_ERROR_MSG( sIdx < SM_VAR_PAGE_LIST_COUNT,
                       "sIdx : %"ID_UINT32_FMT,
                       sIdx );

        sVCPieceSize = aVarEntry[sIdx].mSlotSize;

        for( ;
             sNxtVCPiecePtr + sVCPieceSize <= sFence;
             sNxtVCPiecePtr += sVCPieceSize,
                 sNxtVCPieceOID += sVCPieceSize )
        {
            // In case of free slot
            if((((smVCPieceHeader *)sNxtVCPiecePtr)->flag & SM_VCPIECE_FREE_MASK)
               == SM_VCPIECE_FREE_OK)
            {
                // refineDB�� FreeSlot�� FreeSlotList�� ����Ѵ�.
                addFreeSlotToFreeSlotList(aSpaceID,
                                          sPage->mHeader.mSelfPageID,
                                          sNxtVCPiecePtr);
                continue;
            }

            *aNxtPiecePtr = sNxtVCPiecePtr;
            *aNxtPieceOID = sNxtVCPieceOID;

            IDE_CONT(normal_case);
        } /* for */

        // refineDB�� �ϳ��� Page Scan�� ��ġ�� FreePageList�� ����Ѵ�.

        sFreePageHeader = smpFreePageList::getFreePageHeader(aSpaceID, sPage);

        if( sFreePageHeader->mFreeSlotCount > 0 )
        {
            // FreeSlot�� �־�� FreePage�̰� FreePageList�� ��ϵȴ�.
            IDE_TEST( smpFreePageList::addPageToFreePageListAtInit(
                          aVarEntry + sIdx,
                          smpFreePageList::getFreePageHeader(aSpaceID, sPage))
                      != IDE_SUCCESS );
        }

        sNxtPID = smpManager::getNextAllocPageID( aSpaceID,
                                                  aVarEntry,
                                                  sPage->mHeader.mSelfPageID );

        if(sNxtPID == SM_NULL_PID)
        {
            // NextPage�� NULL�̸� ���̴�.
            IDE_CONT(normal_case);
        }

        IDE_ERROR_MSG( smmManager::getPersPagePtr( aSpaceID,
                                                   sNxtPID,
                                                   (void**)&sPage )
                       == IDE_SUCCESS,
                       "TableOID : %"ID_UINT64_FMT"\n"
                       "SpaceID  : %"ID_UINT32_FMT"\n"
                       "PageID   : %"ID_UINT32_FMT"\n",
                       aVarEntry->mTableOID,
                       aSpaceID,
                       sNxtPID );
        sNxtVCPiecePtr  = (SChar *)sPage->mBody + ID_SIZEOF(smpVarPageHeader);
        sNxtVCPieceOID  = SM_MAKE_OID(sNxtPID, (SChar*)sNxtVCPiecePtr - (SChar*)sPage);
    }

    IDE_EXCEPTION_CONT( normal_case );

    (*aIdx) = sIdx;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**********************************************************************
 * FreePageHeader�� FreeSlot�߰�
 *
 * aPageID    : FreeSlot�߰��Ϸ��� PageID
 * aPiecePtr  : FreeSlot�� Row ������
 **********************************************************************/

void smpVarPageList::addFreeSlotToFreeSlotList( scSpaceID aSpaceID,
                                                scPageID  aPageID,
                                                SChar*    aPiecePtr )
{
    smpFreePageHeader *sFreePageHeader;
    smVCPieceHeader   *sCurVCPieceHdr;
    smVCPieceHeader   *sTailVCPieceHdr;

    IDE_DASSERT( aPageID != SM_NULL_PID );
    IDE_DASSERT( aPiecePtr != NULL );

    sFreePageHeader = smpFreePageList::getFreePageHeader(aSpaceID, aPageID);
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
 * PageList�� FreeSlotList,FreePageList,FreePagePool�� �籸���Ѵ�.
 *
 * FreeSlot/FreePage ���� ������ Disk�� ����Ǵ� Durable�� ������ �ƴϱ⶧����
 * ������ restart�Ǹ� �籸�����־�� �Ѵ�.
 *
 * aTrans    : �۾��� �����ϴ� Ʈ����� ��ü
 * aVarEntry : �����Ϸ��� PageListEntry
 **********************************************************************/

IDE_RC smpVarPageList::refinePageList( void*             aTrans,
                                       scSpaceID         aSpaceID,
                                       smpPageListEntry* aVarEntry )
{
    UInt i;

    IDE_DASSERT( aTrans != NULL );
    IDE_DASSERT( aVarEntry != NULL );

    // Slot�� Refine�ϰ� �� Page���� FreeSlotList�� �����ϰ�
    // �� �������� FreePage�̸� �켱 FreePageList[0]�� ����Ѵ�.
    IDE_TEST( buildFreeSlotList( aSpaceID,
                                 aVarEntry )
              != IDE_SUCCESS );

    for( i = 0; i < SM_VAR_PAGE_LIST_COUNT ; i++ )
    {
        // FreePageList[0]���� N���� FreePageList�� FreePage���� �����ְ�
        smpFreePageList::distributePagesFromFreePageList0ToTheOthers( (aVarEntry + i) );

        // EmptyPage(������������ʴ� FreePage)�� �ʿ��̻��̸�
        // FreePagePool�� �ݳ��ϰ� FreePagePool���� �ʿ��̻��̸�
        // DB�� �ݳ��Ѵ�.
        IDE_TEST(smpFreePageList::distributePagesFromFreePageList0ToFreePagePool(
                     aTrans,
                     aSpaceID,
                     (aVarEntry + i) )
                 != IDE_SUCCESS);
    }


    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**********************************************************************
 * FreeSlotList ����
 *
 * aTrans    : �۾��� �����ϴ� Ʈ����� ��ü
 * aVarEntry : �����Ϸ��� PageListEntry
 **********************************************************************/
IDE_RC smpVarPageList::buildFreeSlotList( scSpaceID          aSpaceID,
                                          smpPageListEntry * aVarEntry )
{
    smVCPieceHeader*      sVCPieceHeaderPtr;
    SChar*                sCurVarRowPtr;
    SChar*                sNxtVarRowPtr;
    smOID                 sCurPieceOID;
    smOID                 sNxtPieceOID;
    UInt                  sIdx;

    IDE_DASSERT( aVarEntry != NULL );

    sCurVarRowPtr = NULL;
    sCurPieceOID  = SM_NULL_OID;

    while(1)
    {
        // FreeSlot�� �����ϰ�
        IDE_TEST(nextOIDallForRefineDB(aSpaceID,
                                       aVarEntry,
                                       sCurPieceOID,
                                       sCurVarRowPtr,
                                       &sNxtPieceOID,
                                       &sNxtVarRowPtr,
                                       &sIdx)
                 != IDE_SUCCESS);

        if(sNxtVarRowPtr == NULL)
        {
            break;
        }

        sVCPieceHeaderPtr = (smVCPieceHeader *)sNxtVarRowPtr;

        /* ----------------------------
         * Variable Slot�� Delete Flag��
         * ������ ��� ����ȭ�� Row�̴�.
         * ---------------------------*/
        IDE_ERROR_MSG( ( sVCPieceHeaderPtr->flag & SM_VCPIECE_FREE_MASK )
                       == SM_VCPIECE_FREE_NO,
                       "VCFlag : %"ID_UINT32_FMT,
                       sVCPieceHeaderPtr->flag );

        (aVarEntry[sIdx].mRuntimeEntry->mInsRecCnt)++;

        sCurVarRowPtr = sNxtVarRowPtr;
        sCurPieceOID  = sNxtPieceOID;
    }


    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**********************************************************************
 * PageList�� ��ȿ�� ���ڵ� ���� ��ȯ
 * for X$TABLE_INFO, X$TEMP_TABLE_INFO
 *
 * aVarEntry    : �˻��ϰ��� �ϴ� PageListEntry
 * aRecordCount : ��ȯ�ϴ� ���ڵ� ����
 **********************************************************************/
ULong smpVarPageList::getRecordCount( smpPageListEntry *aVarEntry )
{
    ULong sInsertCount;
    ULong sDeleteCount;

    IDE_DASSERT( aVarEntry != NULL );

    /* BUG-47706 delete �� ���� �ް� insert �� �޾ƾ� ������ ���� �ʴ´�. */
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
void smpVarPageList::initializePageListEntry( smpPageListEntry* aVarEntry,
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

    for( i = 0; i < SM_VAR_PAGE_LIST_COUNT; i++ )
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
UInt smpVarPageList::getVarIdx( void* aPagePtr )
{
    UInt sIdx;

    IDE_DASSERT( aPagePtr != NULL );

    sIdx = ((smpVarPageHeader *)((SChar *)aPagePtr +
                                 ID_SIZEOF(smpPersPageHeader)))->mIdx;

    return sIdx;
}

/**********************************************************************
 * aValue�� ���� VarEntry�� Idx���� ���Ѵ�.
 *
 * aLength : VarEntry�� �Է��� Value�� ����.
 * aIdx    : aValue�� ���� VarIdx��
 **********************************************************************/
IDE_RC smpVarPageList::calcVarIdx( UInt     aLength,
                                   UInt*    aVarIdx )
{
    IDE_DASSERT( aLength != 0 );
    IDE_DASSERT( aVarIdx != NULL );
    
    IDE_TEST_RAISE( aLength > gVarSlotSizeArray[SM_VAR_PAGE_LIST_COUNT - 1],
                    too_long_var_item );

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
idBool smpVarPageList::isValidFreeSlotList( smpFreePageHeader* aFreePageHeader )
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
 * BUG-31206 improve usability of DUMPCI and DUMPDDF
 *           Slot Header�� altibase_sm.log�� �����Ѵ�
 *
 * VarPage�� �����Ѵ�
 *
 * aPagePtr       : dump�� page 
 * aOutBuf        : ��� ����
 * aOutSize       : ��� ������ ũ��
 **********************************************************************/
void smpVarPageList::dumpVarPageByBuffer( UChar            * aPagePtr,
                                          SChar            * aOutBuf,
                                          UInt               aOutSize )
{
    smVCPieceHeader      * sVCPieceHeader;
    smpVarPageHeader     * sVarPageHeader;
    UInt                   sSlotCnt;
    UInt                   sSlotSize;
    smpPersPageHeader    * sHeader;
    vULong                 sIdx;
    UInt                   i;

    sVarPageHeader = ((smpVarPageHeader *)((SChar *)aPagePtr +
                                           ID_SIZEOF(smpPersPageHeader)));
    sVCPieceHeader = (smVCPieceHeader*)(sVarPageHeader + 1);
    sIdx           = sVarPageHeader->mIdx;

    /* ref : smpVarPageList::initializePageListEntry */
    sSlotSize      = ( 1 << ( sIdx + SM_ITEM_MIN_BIT_SIZE ) )
        + ID_SIZEOF(smVCPieceHeader);
    if( sSlotSize > 0 )
    {
        sSlotCnt       = ( SMP_PERS_PAGE_BODY_SIZE 
                               - ID_SIZEOF( smpVarPageHeader ) )
                            / sSlotSize;
    }
    else
    {
        sSlotCnt = 0;
    }

    sHeader        = (smpPersPageHeader*)aPagePtr;

    ideLog::ideMemToHexStr( aPagePtr,
                            SM_PAGE_SIZE,
                            IDE_DUMP_FORMAT_NORMAL,
                            aOutBuf,
                            aOutSize );

    idlVA::appendFormat( aOutBuf,
                         aOutSize,
                         "\n= VarPage =\n"
                         "SelfPID      : %"ID_UINT32_FMT"\n"
                         "PrevPID      : %"ID_UINT32_FMT"\n"
                         "NextPID      : %"ID_UINT32_FMT"\n"
                         "Type         : %"ID_UINT32_FMT"\n"
                         "TableOID     : %"ID_vULONG_FMT"\n"
                         "AllocListID  : %"ID_UINT32_FMT"\n\n"
                         "mIdx         : %"ID_UINT32_FMT"\n"
                         "SlotSize     : %"ID_UINT32_FMT"\n\n"
                         "nxtPieceOID     Flag            Length\n",
                         sHeader->mSelfPageID,
                         sHeader->mPrevPageID,
                         sHeader->mNextPageID,
                         sHeader->mType,
                         sHeader->mTableOID,
                         sHeader->mAllocListID,
                         sIdx,
                         sSlotSize );

    for( i = 0; i < sSlotCnt; i++)
    {
        dumpVCPieceHeaderByBuffer( sVCPieceHeader,
                                   ID_TRUE,
                                   aOutBuf,
                                   aOutSize );

        sVCPieceHeader = 
            (smVCPieceHeader*)( ((SChar*)sVCPieceHeader) + sSlotSize );

        idlVA::appendFormat( aOutBuf,
                             aOutSize,
                             "\n" );
    }
}


/**********************************************************************
 * BUG-31206    improve usability of DUMPCI and DUMPDDF *
 *
 * Description: VarPage�� Dump�Ͽ� altibase_boot.log�� ��´�.
 *
 * aSpaceID    - [IN] SpaceID
 * aPageID     - [IN] Page ID
 **********************************************************************/
void smpVarPageList::dumpVarPage( scSpaceID         aSpaceID,
                                  scPageID          aPageID )
{
    SChar          * sTempBuffer;
    UChar          * sPagePtr;

    IDE_ASSERT( smmManager::getPersPagePtr( aSpaceID,
                                            aPageID,
                                            (void**)&sPagePtr )
                == IDE_SUCCESS );

    if( iduMemMgr::calloc( IDU_MEM_SM_SMC, 
                           1,
                           ID_SIZEOF( SChar ) * IDE_DUMP_DEST_LIMIT,
                           (void**)&sTempBuffer )
        == IDE_SUCCESS )
    {
        sTempBuffer[0] = '\0';
        dumpVarPageByBuffer( sPagePtr,
                             sTempBuffer,
                             IDE_DUMP_DEST_LIMIT );

        ideLog::log( IDE_SERVER_0,
                     "SpaceID      : %u\n"
                     "PageID       : %u\n"
                     "%s\n",
                     aSpaceID,
                     aPageID,
                     sTempBuffer );

        (void) iduMemMgr::free( sTempBuffer );
    }
}


/**********************************************************************
 *
 * Description: calcVarIdx �� ������ �ϱ� ���� gAllocArray�� �����Ѵ�.
 *              �� ������� �ش�Ǵ� �ε����� �̸� �����س���
 *              ������� �ٷ� �ε����� ã�� �� �ְ� �Ѵ�.
 *
 **********************************************************************/
void smpVarPageList::initAllocArray()
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
 * aTrans         : �۾��ϴ� Ʈ����� ��ü
 * aSpaceID       : TableSpace ID
 * aVarEntryArray : PageListEntry Array 
 * aDstIdx        : Free Page�� �޾ƿ����� FreePageList�� PageListEntry index
 * aPageListID    : PageListID
 * aIsPageAlloced : �� �������� �޾ƿԴ��� ����
 ***********************************************************************/
void smpVarPageList::tryForAllocPagesFromOtherPools( void             * aTrans,
                                                     scSpaceID          aSpaceID,
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
                getPagesFromOtherPool( aTrans,
                                       aSpaceID,
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
 * aTrans            : �۾��ϴ� Ʈ����� ��ü
 * aSpaceID          : TableSpace ID
 * aSrcPool          : ���������� ���� �ִ� source�� �Ǵ� FreePagePool
 * aDstEntry         : target�� �Ǵ� PageListEntry
 * aDstIdx           : target�� �Ǵ� PageListEntry�� index 
 * aPageListID       : PageListID
 * aSrcPoolLockState : source�� �Ǵ� FreePagePool�� LOCK ����.
 ***********************************************************************/
void smpVarPageList::getPagesFromOtherPool( void                 * aTrans,
                                            scSpaceID              aSpaceID,
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

    IDE_DASSERT( smpFreePageList::isValidFreePageList( aSrcPool->mHead,
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

    IDE_DASSERT( smpFreePageList::isValidFreePageList( aSrcPool->mHead,
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

        IDE_ASSERT( smrUpdate::initVarPage( NULL, /* idvSQL* */
                                            aTrans,
                                            aSpaceID,
                                            sPagePtr->mHeader.mSelfPageID,
                                            aPageListID,
                                            aDstIdx,
                                            aDstEntry->mSlotSize,
                                            aDstEntry->mSlotCount,
                                            aDstEntry->mTableOID )
                    == IDE_SUCCESS );

        initializePage( aDstIdx,
                        aPageListID,
                        aDstEntry->mSlotSize,
                        aDstEntry->mSlotCount,
                        aDstEntry->mTableOID,
                        sPagePtr );

        IDE_ASSERT( smmDirtyPageMgr::insDirtyPage( aSpaceID,
                                                   sPagePtr->mHeader.mSelfPageID )
                    == IDE_SUCCESS );

        smpFreePageList::initializeFreeSlotListAtPage( aSpaceID,
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

    IDE_DASSERT( smpFreePageList::isValidFreePageList( sDstList->mHead[sEmptySizeClassID],
                                                       sDstList->mTail[sEmptySizeClassID],
                                                       sDstList->mPageCount[sEmptySizeClassID])
                 == ID_TRUE );

    /* Destination FreePageList UNLOCK */
    (void)(sDstList->mMutex.unlock());

    *aSrcPoolLockState = sSrcPoolLockState;
}

