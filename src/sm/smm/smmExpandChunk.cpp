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
* $Id: smmExpandChunk.cpp 86424 2019-12-11 05:19:34Z jiwon.kim $
**********************************************************************/

#include <idl.h>
#include <idm.h>
#include <idu.h>
#include <ideErrorMgr.h>
#include <smm.h>
#include <smErrorCode.h>
#include <smDef.h>
#include <smu.h>
#include <smuUtility.h>
#include <smmReq.h>
#include <smmExpandChunk.h>

UInt   smmExpandChunk::mFLISlotCount;

smmExpandChunk::smmExpandChunk()
{
}

smmExpandChunk::~smmExpandChunk()
{
}


/******************************************************************
 * Private Member Functions
 ******************************************************************/

/* ExpandChunk �����ڸ� �ʱ�ȭ�Ѵ�.
 */
IDE_RC smmExpandChunk::initialize(smmTBSNode * aTBSNode)
{

    // �ϳ��� Free List Info Page�� ����� �� �ִ� Slot�� ��
    mFLISlotCount = smLayerCallback::getPersPageBodySize()
                    /  SMM_FLI_SLOT_SIZE ;
    // Free List Info Page���� Slot����� ������ ��ġ 
    aTBSNode->mFLISlotBase  = smLayerCallback::getPersPageBodyOffset();
    
    aTBSNode->mChunkPageCnt        = 0 ;
    
    return IDE_SUCCESS;
}

/*
 * ExpandChunk�� Page���� �����Ѵ�.
 *
 * aChunkPageCnt [IN] �ϳ��� Expand Chunk�� ������ Page�� ��
 * 
 */
IDE_RC smmExpandChunk::setChunkPageCnt( smmTBSNode * aTBSNode,
                                        vULong       aChunkPageCnt )
{
    aTBSNode->mChunkPageCnt        = aChunkPageCnt ;

    IDE_DASSERT( mFLISlotCount != 0 );
    
    // Expand Chunk���� ��� Page���� Next Free Page ID�� ����� �� �ִ�
    // Free List Info Page�� ���� ���
    aTBSNode->mChunkFLIPageCnt = aTBSNode->mChunkPageCnt / mFLISlotCount ;
    if ( ( aTBSNode->mChunkPageCnt % mFLISlotCount ) != 0 )
    {
        aTBSNode->mChunkFLIPageCnt ++;
    }

    
    return IDE_SUCCESS;
}



/* ExpandChunk �����ڸ� �ı��Ѵ�.
 */
IDE_RC smmExpandChunk::destroy(smmTBSNode * /* aTBSNode */)
{
    return IDE_SUCCESS;
}

/*
 * �ּ��� Ư�� Page�� ��ŭ ������ �� �ִ� Expand Chunk�� ���� ����Ѵ�.
 *
 * aRequiredPageCnt    [IN] �������� Expand Chunk�� ������ �� Page�� ��
 */
vULong smmExpandChunk::getExpandChunkCount( smmTBSNode * aTBSNode,
                                            vULong       aRequiredPageCnt )
{
    // setChunkPageCnt�� �ҷȴ��� üũ
    IDE_DASSERT( aTBSNode->mChunkPageCnt    != 0 );
    IDE_DASSERT( aRequiredPageCnt           != 0 );
    
    UInt sChunks =  aRequiredPageCnt / aTBSNode->mChunkPageCnt ;

    // ��Ȯ�� Expand Chunk Page���� ������ �������� �ʴ´ٸ�,
    // Expand Chunk�� �ϳ� �� ����Ѵ�.
    if ( ( aRequiredPageCnt % aTBSNode->mChunkPageCnt ) != 0 )
    {
        sChunks ++ ;
    }


    return sChunks ;
}



/*
 * Next Free Page ID�� �����Ѵ�.
 *
 * aTrans �� NULL�� �ƴ� ���, �α��� �ǽ��Ѵ�.
 *
 * �� �Լ����� FLI Page�� latch�� ���� �ʴ´�.
 * �� �Լ� ȣ�� ���� FLI Page���� ������ Slot��
 * ���� Ʈ������� ���ÿ� ������ ������ �ʵ��� ���ü� ��� �ؾ� �Ѵ�.
 *
 * aTrans          [IN] Next Free Page�� �����Ϸ��� Ʈ�����
 * aPageID         [IN] Next Free Page�� ������ Free Page
 * aNextFreePageID [IN] Next Free PAge ID
 */
IDE_RC smmExpandChunk::logAndSetNextFreePage( smmTBSNode * aTBSNode,
                                              void        *aTrans,
                                              scPageID     aFreePID,
                                              scPageID     aNextFreePID )
{
    scPageID       sFLIPageID;
    UInt           sSlotOffset;
    void         * sFLIPageAddr;
    smmFLISlot   * sSlotAddr;
    scPageID       sOrgNextFreePageID;

    IDE_DASSERT( isDataPageID( aTBSNode, aFreePID ) == ID_TRUE );
    IDE_DASSERT( ( aNextFreePID == SM_NULL_PID ||
                   aNextFreePID == SMM_FLI_ALLOCATED_PID ) ?
                 1 : isDataPageID( aTBSNode, aNextFreePID ) == ID_TRUE );
    
    IDE_DASSERT( aTBSNode->mFLISlotBase != 0 );
    
    // setChunkPageCnt�� �ҷȴ��� üũ
    IDE_DASSERT( aTBSNode->mChunkPageCnt != 0 );
    
    // ������ �������� Slot�� �����ϴ� FLI Page�� ID�� �˾Ƴ���.
    sFLIPageID = getFLIPageID( aTBSNode, aFreePID) ;

    // FLI Page�ȿ��� �������������� Slot Offset�� ���Ѵ�.
    sSlotOffset = getSlotOffsetInFLIPage( aTBSNode, aFreePID );

    // Free List Info Page �� �ּҸ� �˾Ƴ���.
    IDE_ASSERT( smmManager::getPersPagePtr( aTBSNode->mTBSAttr.mID, 
                                            sFLIPageID,
                                            &sFLIPageAddr )
                == IDE_SUCCESS );

    // Slot �� �ּҸ� ����Ѵ�.
    sSlotAddr = (smmFLISlot * ) ( ( (SChar* )sFLIPageAddr ) +
                                  aTBSNode->mFLISlotBase + 
                                  sSlotOffset );

    IDE_DASSERT( (SChar*)sSlotAddr == (SChar*)& sSlotAddr->mNextFreePageID );

    // aTrans �� NULL�� ��쿡�� �α����� �ʴ´�.
    // allocNewExpandChunk �� Logical Redo�Ǵµ�,
    // �� �� Free Page Link�� ���� �α����� �ʴ´�.
    if ( aTrans != NULL )
    {
        sOrgNextFreePageID = sSlotAddr->mNextFreePageID;

        IDE_TEST( smLayerCallback::updateLinkAtFreePage (
                      NULL, /* idvSQL* */
                      aTrans,
                      aTBSNode->mHeader.mID,
                      sFLIPageID,         // Page ID   
                      aTBSNode->mFLISlotBase + sSlotOffset, // Offset In Page
                      sOrgNextFreePageID, // Before Image 
                      aNextFreePID )      // After Image
                  != IDE_SUCCESS );
    
    }
    
    
    sSlotAddr->mNextFreePageID = aNextFreePID ;

    IDE_TEST( smmDirtyPageMgr::insDirtyPage( aTBSNode->mHeader.mID, sFLIPageID ) != IDE_SUCCESS );

    
    return IDE_SUCCESS;
    
    IDE_EXCEPTION_END;

    
    return IDE_FAILURE;
}



/*
 * Next Free Page ID�� �����´�.
 *
 * aPageID         [IN] Next Free Page�� ������ Free Page
 * aNextFreePageID [IN] Next Free PAge ID
 */
IDE_RC smmExpandChunk::getNextFreePage( smmTBSNode * aTBSNode,
                                        scPageID     aFreePageID,
                                        scPageID *   aNextFreePageID )
{
    scPageID       sFLIPageID;
    UInt           sSlotOffset;
    void         * sFLIPageAddr;
    smmFLISlot   * sSlotAddr;

    IDE_DASSERT( isDataPageID( aTBSNode, aFreePageID ) == ID_TRUE );
    IDE_DASSERT( aNextFreePageID != NULL );

    // setChunkPageCnt�� �ҷȴ��� üũ
    IDE_DASSERT( aTBSNode->mChunkPageCnt != 0 );

    IDE_DASSERT( aTBSNode->mFLISlotBase != 0 );
    
    // ������ �������� Slot�� �����ϴ� FLI Page�� ID�� �˾Ƴ���.
    sFLIPageID = getFLIPageID( aTBSNode, aFreePageID) ;

    // FLI Page�ȿ��� �������������� Slot Offset�� ���Ѵ�.
    sSlotOffset = getSlotOffsetInFLIPage( aTBSNode, aFreePageID );

    // Free List Info Page �� �ּҸ� �˾Ƴ���.
    IDE_ERROR( smmManager::getPersPagePtr( aTBSNode->mTBSAttr.mID, 
                                           sFLIPageID,
                                           &sFLIPageAddr )
               == IDE_SUCCESS );

    // Slot �� �ּҸ� ����Ѵ�.
    sSlotAddr = (smmFLISlot * ) ( ( (SChar* )sFLIPageAddr ) +
                                  aTBSNode->mFLISlotBase + 
                                  sSlotOffset );

    * aNextFreePageID = sSlotAddr->mNextFreePageID ;

    IDE_DASSERT( ( * aNextFreePageID == SM_NULL_PID ||
                   * aNextFreePageID == SMM_FLI_ALLOCATED_PID ) ?
                 1 : isDataPageID( aTBSNode, *aNextFreePageID ) == ID_TRUE );
    
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/*
 * Data Page������ ���θ� �Ǵ��Ѵ�.
 * 
 * aPageID [IN] Data Page���� ���θ� �Ǵ��� Page�� ID
 * return aPageID�� �ش��ϴ� Page�� Data Page�̸� ID_TRUE,
 *        Free List Info Page�̸� ID_FALSE
 */
idBool smmExpandChunk::isDataPageID( smmTBSNode * aTBSNode,
                                     scPageID     aPageID )
{
    vULong sPageNoInChunk ;
    idBool sIsDataPage = ID_TRUE ;

    IDE_DASSERT( smmManager::isValidPageID( aTBSNode->mTBSAttr.mID, aPageID )
                 == ID_TRUE );
    IDE_DASSERT( aTBSNode->mChunkPageCnt != 0 );
    
    // Membase�� Catalog Table�� �ִ� Page�� Data Page�� �ƴϴ�.
    if ( aPageID < SMM_DATABASE_META_PAGE_CNT )
    {
        sIsDataPage = ID_FALSE;
    }
    
    // Chunk �ȿ��� 0���� �����ϴ� Page ��ȣ ���
    sPageNoInChunk = ( aPageID - SMM_DATABASE_META_PAGE_CNT ) %
        aTBSNode->mChunkPageCnt ;

    // Chunk ������ Free List Info Page ������ ��ġ�ϸ�,
    // FLI Page����, Data Page�� �ƴϴ�.
    if ( sPageNoInChunk < aTBSNode->mChunkFLIPageCnt )
    {
        sIsDataPage = ID_FALSE;
    }


    return sIsDataPage;
}

// BUG-47487: FLI Page������ ���θ� �Ǵ��Ѵ�.
idBool smmExpandChunk::isFLIPageID( smmTBSNode * aTBSNode, 
                                    scPageID     aPageID )
{
    vULong sPageNoInChunk ;

    // Membase�� Catalog Table�� �ִ� Page
    if ( aPageID < SMM_DATABASE_META_PAGE_CNT )
    {
        return ID_FALSE;
    }

    IDE_DASSERT( aPageID >= SMM_DATABASE_META_PAGE_CNT);    

    // Chunk �ȿ��� 0���� �����ϴ� Page ��ȣ ���
    sPageNoInChunk = ( aPageID - SMM_DATABASE_META_PAGE_CNT ) %
        aTBSNode->mChunkPageCnt ;

    // Chunk ������ Free List Info Page ������ ��ġ�ϸ�,
    // FLI Page�̴�.
    if ( sPageNoInChunk < aTBSNode->mChunkFLIPageCnt )
    {
        return ID_TRUE;
    }
    else
    {
        return ID_FALSE;
    }
}

/* �����ͺ��̽� Page�� Free Page���� ���θ� �����Ѵ�.
 *
 * aPageID     [IN] �˻��ϰ��� �ϴ� Page�� ID
 * aShouldLoad [OUT] Free Page�̸� ID_TRUE, �ƴϸ� ID_FALSE
 */
IDE_RC smmExpandChunk::isFreePageID(smmTBSNode * aTBSNode,
                                    scPageID     aPageID,
                                    idBool *     aIsFreePage )
{
    scPageID sNextFreePID;

    IDE_DASSERT( smmManager::isValidPageID( aTBSNode->mTBSAttr.mID, aPageID )
                 == ID_TRUE );
    IDE_DASSERT( aIsFreePage != NULL );
    
    if ( isDataPageID( aTBSNode, aPageID ) )
    {
        // ���̺� �Ҵ�� Page���� �˾ƺ��� ����
        // Page�� Next Free Page �� �˾Ƴ���.
        IDE_TEST( getNextFreePage( aTBSNode, aPageID, & sNextFreePID )
                  != IDE_SUCCESS );

        // ���̺� �Ҵ�� Page�� ���
        // Free List Info Page�� SMM_FLI_ALLOCATED_PID�� �����ȴ�.
        // ( Page�� ���̺�� �Ҵ�� ��
        //   smmFPLManager::allocFreePageMemoryList ���� ������ )
        if ( sNextFreePID == SMM_FLI_ALLOCATED_PID )
        {
            *aIsFreePage = ID_FALSE;
        }
        else // ������ �������̸鼭 Free Page�� ��� 
        {
            *aIsFreePage = ID_TRUE;
        }
    }
    else // ������ �������� �ƴ϶�� Free Page�� �ƴϴ�.
    {
        *aIsFreePage = ID_FALSE;
    }

    
    return IDE_SUCCESS;
    
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}



/******************************************************************
 * Private Member Functions
 ******************************************************************/


/*
 * Ư�� Data Page�� ���� Expand Chunk�� �˾Ƴ���.
 *
 * ��������� �� ó���� ����� ���� ���� �� �Լ��� ������ �˾ƺ���.
 * ������ ���� Free List Info Page�� �ΰ�, Data Page�� �װ��� ExpandChunk
 * �ΰ��� �����ϴ� �����ͺ��̽��� ��츦 �����غ���.
 * 
 * < ExpandChunk #0 >
 *   | FLI-Page#0 | FLI-Page#1 | D-Page#2 | D-Page#3 | D-Page#4  | D-Page#5  |
 * < ExpandChunk #1 >
 *   | FLI-Page#6 | FLI-Page#7 | D-Page#8 | D-Page#9 | D-Page#10 | D-Page#11 |
 * 
 * �� �Լ��� Ư�� Data Page�� ���� ExpandChunk�� �˾Ƴ��� �Լ��̸�,
 * ���� ������ D-Page #4�� Expand Chunk�� 1���̴�.
 * ExpandChunk#1���� ���� Page ID�� 6�� �̹Ƿ� �� �Լ��� 6�� �����ϰ� �ȴ�.
 *
 * aDataPageID [IN] Page�� ���� Expand Chunk�� �˰��� �ϴ� Page�� ID
 *
 * return aPID�� ���� ExpandChunk�� Page ID
 */
scPageID smmExpandChunk::getExpandChunkPID( smmTBSNode * aTBSNode,
                                            scPageID     aDataPageID )
{
    vULong sChunkNo ;
    scPageID sChunkFirstPID;

    IDE_DASSERT( isDataPageID( aTBSNode, aDataPageID ) == ID_TRUE );
    
    // Page ID�� �ϳ��� Expand Chunk�� ������ �� �ִ� Page�� ���� ������.
    sChunkNo = (aDataPageID - SMM_DATABASE_META_PAGE_CNT) /
        aTBSNode->mChunkPageCnt ;

    // Expand Chunk���� ù��° Page ( Free List Info Page)�� Page ID�� ����Ѵ�.
    sChunkFirstPID = sChunkNo * aTBSNode->mChunkPageCnt +
        SMM_DATABASE_META_PAGE_CNT;


    return sChunkFirstPID ;
}

/*
 * �ϳ��� Expand Chunk�ȿ��� Free List Info Page�� ������
 * ������ �������鿡 ���� ������� 0���� 1�� �����ϴ� ��ȣ�� �ű�
 * Data Page No �� �����Ѵ�.
 *
 * ��������� �� ó���� ����� ���� ���� �� �Լ��� ������ �˾ƺ���.
 * ������ ���� Free List Info Page�� �ΰ�, Data Page�� �װ��� ExpandChunk
 * �ΰ��� �����ϴ� �����ͺ��̽��� ��츦 �����غ���.
 * 
 * < ExpandChunk #0 >
 *   | FLI-Page#0 | FLI-Page#1 | D-Page#2 | D-Page#3 | D-Page#4  | D-Page#5  |
 * < ExpandChunk #1 >
 *   | FLI-Page#6 | FLI-Page#7 | D-Page#8 | D-Page#9 | D-Page#10 | D-Page#11 |
 *
 * �� ���� ��� �������������� D-Page#2,3,4,5,8,9,10,11�̸�,
 * �̵��� Data Page No �� 0,1,2,3,0,1,2,3 �̴�
 *
 * aDataPageID [IN] �ڽ��� ���� Expand Chunk�� �˾Ƴ����� �ϴ� Data Page
 * return           Epxand Chunk�ȿ��� Data Page�鿡 ����
 *                  ������� 0���� 1�� �����ϵ��� �ű� ��ȣ 
 */
vULong smmExpandChunk::getDataPageNoInChunk( smmTBSNode * aTBSNode,
                                             scPageID     aDataPageID )
{
    vULong sDataPageNo;
    scPageID sChunkPID;
    scPageID sFirstDataPagePID;

    // ������ ���������� Ȯ���Ѵ�.
    IDE_ASSERT( isDataPageID( aTBSNode, aDataPageID ) == ID_TRUE );
    
    // Expand Chunk���� ù��° Page�� Page ID�� ����Ѵ�.
    sChunkPID = getExpandChunkPID( aTBSNode, aDataPageID );

    // Expand Chunk���� ù��° Data Page ID
    // Expand Chunk���� Page ID + Free List Info Page�� �� 
    sFirstDataPagePID = sChunkPID + aTBSNode->mChunkFLIPageCnt;
    
    // Expand Chunk���� Data Page�� 0���� 1�� �����Ͽ� �ű� ��ȣ�� ����Ѵ�.
    // Data Page ID���� Expand Chunk���� ù��° Data Page ID�� ����.
    sDataPageNo = aDataPageID - sFirstDataPagePID ;


    return sDataPageNo ;
}


/*
 * Ư�� Data Page�� Next Free Page ID�� ����� Slot�� �����ϴ�
 * Free List Info Page�� �˾Ƴ���.
 *
 * ��������� �� ó���� ����� ���� ���� �� �Լ��� ������ �˾ƺ���.
 * ������ ���� Free List Info Page�� �ΰ�, Data Page�� �װ��� ExpandChunk
 * �ΰ��� �����ϴ� �����ͺ��̽��� ��츦 �����غ���.
 * 
 * < ExpandChunk #0 >
 *   | FLI-Page#0 | FLI-Page#1 | D-Page#2 | D-Page#3 | D-Page#4  | D-Page#5  |
 * < ExpandChunk #1 >
 *   | FLI-Page#6 | FLI-Page#7 | D-Page#8 | D-Page#9 | D-Page#10 | D-Page#11 |
 *
 * ���� �� �Լ��� aPID���ڷ� 9�� ���Դٸ�, D-Page#9�� Free List Info Page��
 * FLI-Page#6 �̹Ƿ�, Data Page 9�� Slot�� ��ϵǴ� Free List Info Page��
 * PageID�� 7�� ���ϵȴ�.
 *
 * ����! Free List Info Page���� Slot��
 * Free List Info Page ��ü�� Slot�� ������ �ʴ´�
 * ���� ������, FLI-Page#0 �� D-Page#2, D-Page#3�� 1:1�� �����ϴ� Slot�� ������,
 * FLI-Page#0 �� �����ϴ� Slot�� �� ��������� ������� �ʴ´�.
 *
 * aDataPageID [IN] Free List Info Page�� �˾Ƴ����� �ϴ� Page�� ID
 * return Free List Info Page�� ID
 */
scPageID smmExpandChunk::getFLIPageID( smmTBSNode * aTBSNode,
                                       scPageID     aDataPageID )
{
    vULong sDataPageNo, sFLINo;
    scPageID sChunkPID;

    IDE_DASSERT( mFLISlotCount != 0 );
    
    // ������ ���������� Ȯ���Ѵ�.
    IDE_ASSERT( isDataPageID( aTBSNode, aDataPageID ) == ID_TRUE );
    
    // Expand Chunk�� Page ID�� �˾Ƴ���.
    sChunkPID = getExpandChunkPID( aTBSNode, aDataPageID );

    // Expand Chunk���� Data Page No�� �˾Ƴ���.
    sDataPageNo = getDataPageNoInChunk( aTBSNode, aDataPageID );

    // Data Page�� Slot�� ��ϵ� Free List Info Page�� ����ϱ� ����
    // Expand Chunk���� FLI Page�� 0���� 1�� �����Ͽ� �ű� ��ȣ�� ����Ѵ�.
    sFLINo = sDataPageNo / mFLISlotCount ;


    // Free List Info Page�� ID�� �����Ѵ�.
    return sChunkPID + sFLINo ;
}

/* FLI Page ������ Data Page�� 1:1�� ���εǴ� Slot�� offset�� ����Ѵ�.
 *
 * aDataPageID [IN] Slot Offset�� ����ϰ��� �ϴ� Page�� ID
 * return Data Page�� 1:1�� ���εǴ� Slot�� Offset
 */
UInt smmExpandChunk::getSlotOffsetInFLIPage( smmTBSNode * aTBSNode,
                                             scPageID     aDataPageID )
{
    vULong sDataPageNo;
    UInt sSlotNo;

    IDE_DASSERT( isDataPageID( aTBSNode, aDataPageID ) == ID_TRUE );
    IDE_DASSERT( mFLISlotCount != 0 );
    
    // Expand Chunk���� Data Page�� 0���� 1�� �����Ͽ� �ű� ��ȣ�� ����Ѵ�. 
    sDataPageNo = getDataPageNoInChunk( aTBSNode, aDataPageID );

    // FLI Page �������� Slot��ȣ�� ����Ѵ�.
    sSlotNo = (UInt) ( sDataPageNo % mFLISlotCount );


    // FLI Page �������� Slot offset�� �����Ѵ�.
    return sSlotNo * SMM_FLI_SLOT_SIZE ;
}

/*****************************************************************************
 *
 * BUG-32461 [sm-mem-resource] add getPageState functions to 
 * smmExpandChunk module 
 *
 * Description: Persistent Page�� �Ҵ� ���¸� �����Ѵ�.
 *
 * Parameters:
 *  - aTBSNode      [IN] �˻��� Page�� ���� TBSNode
 *  - aPageID       [IN] �˻��ϰ��� �ϴ� Page�� ID
 *  - aPageState    [OUT] ��� page�� ���¸� ��ȯ
 * 
 *                      <-----------DataPage--------->
 *        +-------------------------------------------------------
 * Table  | META | FLI | alloc | free | alloc | alloc | FLI | ...
 * Space  +-------------------------------------------------------
 *
 ****************************************************************************/
IDE_RC smmExpandChunk::getPageState( smmTBSNode     * aTBSNode,
                                     scPageID         aPageID,
                                     smmPageState   * aPageState )
{
    smmPageState    sPageState;
    idBool          sIsFreePage;
 
    IDE_ERROR( aPageState != NULL );

    if( isDataPageID( aTBSNode, aPageID ) == ID_TRUE )
    {
        /* DataPage, �� Table�� �Ҵ��� �� �ִ� Page�� ��� */
        IDE_TEST( isFreePageID( aTBSNode, 
                                aPageID, 
                                &sIsFreePage )
                  != IDE_SUCCESS );

        if( sIsFreePage == ID_TRUE )
        {
            sPageState = SMM_PAGE_STATE_FREE;
        }
        else
        {
            sPageState = SMM_PAGE_STATE_ALLOC;
        }
    }
    else
    {
        /* Meta �Ǵ� FLI���� Table�� �Ҵ��� �� ���� Page�� ��� */
        if ( aPageID < SMM_DATABASE_META_PAGE_CNT )
        {
            sPageState = SMM_PAGE_STATE_META;
        }
        else
        {
            sPageState = SMM_PAGE_STATE_FLI;
        }

    }

    *aPageState = sPageState;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

