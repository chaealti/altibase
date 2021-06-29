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
 * $Id: sdpDblRIDList.cpp 24150 2007-11-14 06:36:46Z bskim $
 *
 * Description :
 *
 * File-Based RID Linked-List ����
 *
 **********************************************************************/

#include <sdr.h>
#include <sdpDblRIDList.h>
#include <sdpPhyPage.h>

/***********************************************************************
 * Description : RID list�� base ��带 �ʱ�ȭ
 *
 * + 2nd.code desgin
 *   - base ����� rid�� ��´�.
 *   - base ����� mNodeCnt�� 0�� ����    (SDR_4BYTES)
 *   - base ����� mHead�� self RID�� ����(SDR_8BYTES)
 *   - base ����� mTail�� self RID�� ����(SDR_8BYTES)
 ***********************************************************************/
IDE_RC sdpDblRIDList::initList( sdpDblRIDListBase*    aBaseNode,
                                sdrMtx*               aMtx )
{
    sdRID sBaseRID;

    IDE_DASSERT( aBaseNode != NULL );
    IDE_DASSERT( aMtx      != NULL );

    sBaseRID = getBaseNodeRID( aBaseNode );

    // set length
    IDE_TEST( setNodeCnt( aBaseNode, 0, aMtx ) != IDE_SUCCESS );

    // set head-node
    IDE_TEST( setNxtOfNode( &aBaseNode->mBase, sBaseRID, aMtx )
              != IDE_SUCCESS );

    // set tail-node
    IDE_TEST( setPrvOfNode( &aBaseNode->mBase, sBaseRID, aMtx )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : RID list�� head�� ��带 �߰�
 *
 * + 2nd. code design
 *   => sdRIDList::insertNodeAfter�� ȣ����
 ***********************************************************************/
IDE_RC sdpDblRIDList::insertHeadNode(idvSQL              * aStatistics,
                                     sdpDblRIDListBase   * aBaseNode,
                                     sdpDblRIDListNode   * aNewNode,
                                     sdrMtx              * aMtx)
{
    IDE_DASSERT( aBaseNode != NULL );
    IDE_DASSERT( aNewNode  != NULL );
    IDE_DASSERT( aMtx      != NULL );

    // head rid �� ��ü�Ѵ�.
    IDE_TEST( insertNodeAfter(aStatistics,
                              aBaseNode,
                              &aBaseNode->mBase,
                              aNewNode,
                              aMtx) != IDE_SUCCESS );
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description :  RID list�� tail�� ��带 �߰�
 *
 * + 2nd. code design
 *   - sdpDblRIDList::insertNodeBefore�� ȣ����
 ***********************************************************************/
IDE_RC sdpDblRIDList::insertTailNode( idvSQL             *aStatistics,
                                      sdpDblRIDListBase*  aBaseNode,
                                      sdpDblRIDListNode*  aNewNode,
                                      sdrMtx             *aMtx )
{
    IDE_DASSERT( aBaseNode != NULL );
    IDE_DASSERT( aNewNode  != NULL );
    IDE_DASSERT( aMtx      != NULL );

    // tail rid �� ��ü�Ѵ�.
    IDE_TEST( insertNodeBefore( aStatistics,
                                aBaseNode,
                                &aBaseNode->mBase,
                                aNewNode,
                                aMtx ) != IDE_SUCCESS );
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : RID list�� Ư�� ��� �ڿ� ���ο� ��� �߰�
 *
 * + 2nd. code design
 *   - ���� ����� RID�� ��´�.
 *   - newnode�� RID�� ��´�.
 *   - ���� ����� NEXTNODE�� ��´�.
 *   - newnode�� prev ��带 ���� ���� ���� (SDR_8BYTES)
 *   - newnode�� next ��带 NEXTNODE�� ���� (SDR_8BYTES)
 *   - if newnode�� NEXTNODE�� ������ page�� ���
 *     : ���� NEXTNODE�� ptr�� ��´�.
 *   - else 
 *     : NEXTNODE�� ���۰����ڿ� ��û�Ͽ� fix��Ų��,
 *       ������ ����
 *     => NEXTNODE�� prevnode�� newnode�� ���� (SDR_8BYTE)
 *   - ���� ����� next ��带 newnode�� ����(SDR_8BYTE)
 *   - basenode�� ���̸� ���ؼ� +1 (SDR_4BYTE)
 *
 * - ����
 *   1) ���ڰ� ��� not null Ȯ��
 *   2) node != newnode Ȯ��
 ***********************************************************************/
IDE_RC sdpDblRIDList::insertNodeAfter( idvSQL              * aStatistics,
                                       sdpDblRIDListBase   * aBaseNode,
                                       sdpDblRIDListNode   * aNode,
                                       sdpDblRIDListNode   * aNewNode,
                                       sdrMtx              * aMtx )
{
    scSpaceID           sSpaceID;
    sdRID               sRID;
    sdRID               sNewRID;
    sdRID               sNextRID;
    ULong               sNodeCnt;
    idBool              sTrySuccess;
    sdpDblRIDListNode*  sNextNode;
    UChar              *sPagePtr;

    IDE_DASSERT( aBaseNode != NULL );
    IDE_DASSERT( aNode     != NULL );
    IDE_DASSERT( aNewNode  != NULL );
    IDE_DASSERT( aMtx      != NULL );
    IDE_DASSERT( aNode     != aNewNode );

    // ���� ����� page ID�� ��´�.
    sRID    = sdpPhyPage::getRIDFromPtr( (UChar*)aNode );
    
    // ���ο� ����� page ID�� ��´�.
    sNewRID = sdpPhyPage::getRIDFromPtr( (UChar*)aNewNode );
   
    // ���ο� ����� prev/next�� �����Ѵ�.
    sNextRID = getNxtOfNode( aNode ); // get NEXTNODE

    sSpaceID = sdpPhyPage::getSpaceID( sdpPhyPage::getPageStartPtr( (UChar*)aBaseNode ) );
    
    // ������ page�� ���
    if( isSamePage( &sNewRID, &sNextRID ) == ID_TRUE ) 
    {
        sNextNode = (sdpDblRIDListNode*)( sdpPhyPage::getPageStartPtr( (UChar*)aNewNode )
                                         + SD_MAKE_OFFSET( sNextRID ) );
    }
    else
    {
        // NEXTNODE�� fix ��Ų��.
        sPagePtr = sdrMiniTrans::getPagePtrFromPageID( aMtx,
                                                       sSpaceID,
                                                       SD_MAKE_PID( sNextRID) );

        if( sPagePtr == NULL )
        {
            IDE_TEST( sdbBufferMgr::getPageByRID( aStatistics,
                                                  sSpaceID,
                                                  sNextRID,
                                                  SDB_X_LATCH,
                                                  SDB_WAIT_NORMAL,
                                                  aMtx,
                                                  (UChar**)&sNextNode,
                                                  &sTrySuccess )
                      != IDE_SUCCESS );
        }
        else
        {
            sNextNode = (sdpDblRIDListNode*)( sPagePtr + SD_MAKE_OFFSET( sNextRID ) );
        }

    }

    IDE_TEST( setPrvOfNode( aNewNode, sRID, aMtx ) != IDE_SUCCESS );
    IDE_TEST( setNxtOfNode( aNewNode, sNextRID, aMtx ) != IDE_SUCCESS );

    // NEXTNODE�� prev ��带 ���ο� ���� �����Ѵ�.
    IDE_TEST( setPrvOfNode( sNextNode, sNewRID, aMtx ) != IDE_SUCCESS );

    // ���� ����� next ��带 ���ο� ���� �����Ѵ�.
    IDE_TEST( setNxtOfNode(aNode, sNewRID, aMtx) != IDE_SUCCESS );

    // ���ο� ��� �߰��� ���� base ����� ����Ʈ ���� 1 ����
    sNodeCnt = getNodeCnt( aBaseNode );
    IDE_TEST( setNodeCnt( aBaseNode, ( sNodeCnt + 1 ), aMtx ) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
    
}

/***********************************************************************
 * Description : RID list�� Ư�� ��� �տ� ���ο� ��� �߰�
 *
 * + 2nd. code design
 *   - ���� ����� RID�� ��´�.
 *   - newnode�� RID�� ��´�.
 *   - ���� ����� PREVNODE�� ��´�.
 *   - newnode�� prev ��带 PREVNODE�� ���� (SDR_8BYTES)
 *   - newnode�� next ��带 ���� ���� ���� (SDR_8BYTES)
 *   - if newnode�� PREVNODE�� ������ page�� ���
 *     : ���� PREVNODE�� ptr�� ��´�.
 *   - else 
 *     : PREVNODE�� ���۰����ڿ� ��û�Ͽ� fix��Ų��,
 *       ������ ����
 *     => PREVNODE�� next ��带 newnode�� ���� (SDR_8BYTE)
 *   - ���� ����� prev ��带 newnode�� ����(SDR_8BYTE)
 *   - basenode�� ���̸� ���ؼ� +1 (SDR_4BYTE)
 *
 * - ����
 *   1) ���ڰ� ��� not null Ȯ��
 *   2) node != newnode Ȯ��
 ***********************************************************************/
IDE_RC sdpDblRIDList::insertNodeBefore(idvSQL           *aStatistics,
                                       sdpDblRIDListBase*   aBaseNode,
                                       sdpDblRIDListNode*   aNode,
                                       sdpDblRIDListNode*   aNewNode,
                                       sdrMtx*           aMtx)
{

    scSpaceID            sSpaceID;
    sdRID                sRID;
    sdRID                sNewRID;
    sdRID                sPrevRID;
    idBool               sTrySuccess;
    sdpDblRIDListNode*   sPrevNode;
    UChar              * sPagePtr;

    IDE_DASSERT( aBaseNode != NULL );
    IDE_DASSERT( aNode     != NULL );
    IDE_DASSERT( aNewNode  != NULL );
    IDE_DASSERT( aMtx      != NULL );
    IDE_DASSERT( aNode     != aNewNode );

    // ���� ����� page ID�� ��´�. (tnode)
    sRID    = sdpPhyPage::getRIDFromPtr( (UChar*)aNode );
    
    // ���ο� ����� page ID�� ��´�. (node)
    sNewRID = sdpPhyPage::getRIDFromPtr( (UChar*)aNewNode );
   
    // ���ο� ����� prev/next�� �����Ѵ�. (tnode->prev)
    sPrevRID = getPrvOfNode( aNode ); // get PREVNODE

    sSpaceID = sdpPhyPage::getSpaceID( sdpPhyPage::getPageStartPtr( (UChar*)aBaseNode ) );
    
    // ������ page�� ���
    if (isSamePage( &sNewRID, &sPrevRID ) == ID_TRUE ) 
    {
        sPrevNode = (sdpDblRIDListNode*)( sdpPhyPage::getPageStartPtr( (UChar*)aNewNode )
                                         + SD_MAKE_OFFSET( sPrevRID ) );
    }
    else
    {
        // PREVNODE�� fix ��Ų��.
        // NEXTNODE�� fix ��Ų��.
        sPagePtr = sdrMiniTrans::getPagePtrFromPageID( aMtx,
                                                       sSpaceID,
                                                       SD_MAKE_PID( sPrevRID) );

        if( sPagePtr == NULL )
        {
            IDE_TEST( sdbBufferMgr::getPageByRID( aStatistics,
                                                  sSpaceID,
                                                  sPrevRID,
                                                  SDB_X_LATCH,
                                                  SDB_WAIT_NORMAL,
                                                  aMtx,
                                                  (UChar**)&sPrevNode,
                                                  &sTrySuccess ) != IDE_SUCCESS );
        }
        else
        {
            sPrevNode = (sdpDblRIDListNode*)( sPagePtr + SD_MAKE_OFFSET( sPrevRID ) );
        }

    }

    // (node->next = tnode)
    IDE_TEST( setNxtOfNode( aNewNode, sRID, aMtx ) != IDE_SUCCESS );
    // (node->prev = tnode->prev)
    IDE_TEST( setPrvOfNode( aNewNode, sPrevRID, aMtx ) != IDE_SUCCESS );

    // PREVNODE�� next ��带 ���ο� ���� �����Ѵ�. tnode->prev->next = node
    IDE_TEST( setNxtOfNode( sPrevNode, sNewRID, aMtx ) != IDE_SUCCESS );

    // ���� ����� prev ��带 ���ο� ���� �����Ѵ�. tnode->prev = node
    IDE_TEST( setPrvOfNode( aNode, sNewRID, aMtx ) != IDE_SUCCESS );

    // ���ο� ��� �߰��� ���� base ����� ����Ʈ ���� 1 ����
    IDE_TEST( setNodeCnt( aBaseNode, getNodeCnt( aBaseNode ) + 1, aMtx )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : RID list���� Ư����� ����
 *
 * + 2nd. code design
 *   - ������ ������� PREVNODE/NEXTNODE�� RID�� ��´�
 *   - if PREVNODE == ������ ��� then
 *     :���� PREVNODE�� ptr�� ��´�.
 *   - else
 *     :PREVNODE�� fix ��û 
 *   - PREVNODE�� next ��带 NEXTNODE�� ����(SDR_8BYTES)
 *   - if NEXTNODE == ������ ��� then
 *     :���� NEXTNODE�� ptr�� ��´�.
 *   - else
 *     :NEXTNODE�� fix ��û
 *   -  NEXTNODE�� prevnode�� PREVNODE�� ���� (SDR_8BYTES)
 *   - basenode�� ���̸� ���ؼ� 1 ���� (SDR_4BYTES)
 *
 * - ����
 *   1) ���ڰ� ��� not null Ȯ��
 ***********************************************************************/
IDE_RC sdpDblRIDList::removeNode( idvSQL              * aStatistics,
                                  sdpDblRIDListBase   * aBaseNode,
                                  sdpDblRIDListNode   * aNode,
                                  sdrMtx              * aMtx )
{
    scSpaceID             sSpaceID;
    ULong                 sNodeCnt;
    idBool                sTrySuccess;
    sdRID                 sRID;
    sdRID                 sPrevRID;
    sdRID                 sNextRID;
    sdpDblRIDListNode    *sPrevNode;
    sdpDblRIDListNode    *sNextNode;
    UChar                *sPagePtr;

    IDE_DASSERT( aBaseNode != NULL );
    IDE_DASSERT( aNode     != NULL );
    IDE_DASSERT( aMtx      != NULL );

    IDE_DASSERT( getNodeCnt(aBaseNode) > 0 );

    // ���� ����� page ID�� space ID�� ��´�.
    // ���� ����� page ID�� space ID�� ��´�.
    sRID = sdpPhyPage::getRIDFromPtr( (UChar*)aNode );
    IDE_DASSERT(sRID != SD_NULL_RID);

    //  ���� ����� PREVNODE/NEXTNODE�� ��´�.
    sPrevRID = getPrvOfNode( aNode );
    sNextRID = getNxtOfNode( aNode );

    sSpaceID = sdpPhyPage::getSpaceID(
        sdpPhyPage::getPageStartPtr( (UChar*)aBaseNode ) );

    // PREVNODE�� ���� ��尡 �����ϴٸ� ASSERT!!
    if (isSamePage( &sPrevRID, &sRID ) == ID_TRUE )
    {
        sPrevNode = (sdpDblRIDListNode*)( sdpPhyPage::getPageStartPtr( (UChar*)aNode )
                                          + SD_MAKE_OFFSET( sPrevRID ) );
    }
    else
    {
        // PREVNODE fix ��Ų��.
        sPagePtr = sdrMiniTrans::getPagePtrFromPageID( aMtx,
                                                       sSpaceID,
                                                       SD_MAKE_PID( sPrevRID) );
        if( sPagePtr == NULL )
        {
            IDE_TEST( sdbBufferMgr::getPageByRID( aStatistics,
                                                  sSpaceID,
                                                  sPrevRID,
                                                  SDB_X_LATCH,
                                                  SDB_WAIT_NORMAL,
                                                  aMtx,
                                                  (UChar**)&sPrevNode,
                                                  &sTrySuccess ) != IDE_SUCCESS );
        }
        else
        {
            sPrevNode = (sdpDblRIDListNode*)( sPagePtr + SD_MAKE_OFFSET( sPrevRID ) );

        }
    }
    
    IDE_DASSERT( sPrevNode != NULL );

    // NEXTNODE�� ���� ��尡 �����ϴٸ�
    if (isSamePage(&sNextRID, &sRID) == ID_TRUE)
    {
        sNextNode = (sdpDblRIDListNode*)(sdpPhyPage::getPageStartPtr((UChar*)aNode)
                                         + SD_MAKE_OFFSET(sNextRID));
    }
    else
    {
        // NEXTNODE fix ��Ų��.
        sPagePtr = sdrMiniTrans::getPagePtrFromPageID( aMtx,
                                                       sSpaceID,
                                                       SD_MAKE_PID( sNextRID) );
        if( sPagePtr == NULL )
        {
            IDE_TEST( sdbBufferMgr::getPageByRID( aStatistics,
                                                  sSpaceID,
                                                  sNextRID,
                                                  SDB_X_LATCH,
                                                  SDB_WAIT_NORMAL,
                                                  aMtx,
                                                  (UChar**)&sNextNode,
                                                  &sTrySuccess ) != IDE_SUCCESS );
        }
        else
        {
            sNextNode = (sdpDblRIDListNode*)( sPagePtr + SD_MAKE_OFFSET(sNextRID));
        }
    }

    // PREVNODE�� next ��带 NEXTNODE�� ����
    IDE_TEST( setNxtOfNode(sPrevNode, sNextRID, aMtx) != IDE_SUCCESS );
    
    // NEXTNODE�� prev ��带 PREVNODE�� ����
    IDE_TEST( setPrvOfNode(sNextNode, sPrevRID, aMtx) != IDE_SUCCESS );

    // ��� ���ŷ� ���� base ����� ����Ʈ ���� 1 ����
    sNodeCnt = getNodeCnt(aBaseNode);
    IDE_ASSERT( sNodeCnt != 0 );
    setNodeCnt( aBaseNode, sNodeCnt - 1, aMtx );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*
  ����Ʈ ������ SrcNode�� aDestNode ����(NEXT)�� �ű��.
*/
IDE_RC sdpDblRIDList::moveNodeInList( idvSQL             *  aStatistics,
                                      sdpDblRIDListBase  *  aBaseNode,
                                      sdpDblRIDListNode  *  aDestNode,
                                      sdpDblRIDListNode  *  aSrcNode,
                                      sdrMtx             *  aMtx )
{
    ULong  sNodeCnt;

    IDE_DASSERT( aBaseNode != NULL );
    IDE_DASSERT( aMtx != NULL );
    IDE_DASSERT( aDestNode != NULL );
    IDE_DASSERT( aSrcNode != NULL );

    sNodeCnt = sdpDblRIDList::getNodeCnt( aBaseNode );
    
    IDE_TEST( removeNode( aStatistics,
                          aBaseNode,
                          aSrcNode,
                          aMtx ) != IDE_SUCCESS );
    
    IDE_TEST( insertNodeAfter( aStatistics,
                               aBaseNode,
                               aDestNode,
                               aSrcNode,
                               aMtx ) != IDE_SUCCESS );
    
    IDE_ASSERT( sNodeCnt == sdpDblRIDList::getNodeCnt( aBaseNode ) );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : base ����� length ���� �� logging
 * base ����� list length�� �����Ѵ�.
 ***********************************************************************/
IDE_RC sdpDblRIDList::setNodeCnt(sdpDblRIDListBase*    aBaseNode,
                                 ULong                 aNodeCnt,
                                 sdrMtx*               aMtx)
{

    IDE_DASSERT( aBaseNode != NULL );
    IDE_DASSERT( aMtx != NULL );

    IDE_TEST( sdrMiniTrans::writeNBytes(aMtx,
                                        (UChar*)&aBaseNode->mNodeCnt,
                                        &aNodeCnt,
                                        ID_SIZEOF(aNodeCnt) )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
    
}


/***********************************************************************
 * Description : ����� prev RID ���� �� logging
 ***********************************************************************/
IDE_RC sdpDblRIDList::setPrvOfNode( sdpDblRIDListNode  * aNode,
                                    sdRID                aPrevRID,
                                    sdrMtx             * aMtx )
{
    IDE_DASSERT( aNode    != NULL );
    IDE_DASSERT( aMtx     != NULL );

    IDE_TEST( sdrMiniTrans::writeNBytes( aMtx,
                                         (UChar*)&aNode->mPrev,
                                         &aPrevRID,
                                         ID_SIZEOF( aPrevRID ) )
              != IDE_SUCCESS );
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
    
}

/***********************************************************************
 * Description : ����� next RID ���� �� logging
 ***********************************************************************/
IDE_RC sdpDblRIDList::setNxtOfNode( sdpDblRIDListNode * aNode,
                                    sdRID               aNextRID,
                                    sdrMtx            * aMtx )
{

    IDE_DASSERT( aNode    != NULL );
    IDE_DASSERT( aMtx     != NULL );

    IDE_TEST( sdrMiniTrans::writeNBytes( aMtx,
                                         (UChar*)&aNode->mNext,
                                         &aNextRID,
                                         ID_SIZEOF( aNextRID ) )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : ����Ʈ�� ��� node ���
 ***********************************************************************/
IDE_RC sdpDblRIDList::dumpList( scSpaceID aSpaceID,
                                sdRID     aBaseNodeRID )
{

    UInt                i;
    ULong               sNodeCount;
    sdrMtx              sMtx;
    UChar             * sPagePtr;
    idBool              sTrySuccess;
    sdpDblRIDListBase * sBaseNode;
    sdpDblRIDListNode * sNode;
    sdRID               sCurRID;
    sdRID               sBaseRID;
    SInt                sState = 0;
    
    IDE_ERROR( aBaseNodeRID != SD_NULL_RID );

    IDE_TEST( sdrMiniTrans::begin( NULL, /* idvSQL* */
                                   &sMtx,
                                   (void*)NULL,
                                   SDR_MTX_LOGGING,
                                   ID_FALSE,/*MtxUndoable(PROJ-2162)*/
                                   SM_DLOG_ATTR_DEFAULT )
              != IDE_SUCCESS );
    sState = 1;

    IDE_TEST( sdbBufferMgr::getPageByRID( NULL, /* idvSQL* */
                                          aSpaceID,
                                          aBaseNodeRID, 
                                          SDB_S_LATCH,
                                          SDB_WAIT_NORMAL,
                                          &sMtx,
                                          (UChar**)&sBaseNode,
                                          &sTrySuccess ) != IDE_SUCCESS );

    sNodeCount  = getNodeCnt( sBaseNode );
    sCurRID     = getHeadOfList( sBaseNode );
    sBaseRID    = sdpPhyPage::getRIDFromPtr( (UChar*)&sBaseNode->mBase );

    for( i = 0; sCurRID != sBaseRID; i++ )
    {
        IDE_TEST( sdbBufferMgr::fixPageByPID( NULL,
                                              aSpaceID,
                                              SD_MAKE_PID(sCurRID),
                                              &sPagePtr,
                                              &sTrySuccess ) != IDE_SUCCESS );
        sState = 2;

        sNode = (sdpDblRIDListNode*)( sPagePtr + SD_MAKE_OFFSET( sCurRID ) );

        idlOS::printf("%"ID_UINT32_FMT"/%"ID_UINT32_FMT" : sid %"ID_UINT32_FMT", pid %"ID_UINT32_FMT", offset %"ID_UINT32_FMT"\n",
                      i,
                      sNodeCount,
                      aSpaceID,
                      SD_MAKE_PID( sCurRID ),
                      SD_MAKE_OFFSET( sCurRID ));

        sCurRID = getNxtOfNode( sNode );

        sState = 1;
        IDE_TEST( sdbBufferMgr::unfixPage( NULL, /* idvSQL* */
                                           sPagePtr )
                  != IDE_SUCCESS );

    }

    sState = 0;
    IDE_TEST( sdrMiniTrans::commit( &sMtx ) != IDE_SUCCESS );

    IDE_ASSERT( i == sNodeCount ); // !! assert
    IDE_ASSERT( sState == 0 );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    switch( sState )
    {
        case 2:
            IDE_ASSERT( sdbBufferMgr::unfixPage( NULL, sPagePtr )
                        == IDE_SUCCESS );
        case 1:
            IDE_ASSERT( sdrMiniTrans::rollback( &sMtx )
                        == IDE_SUCCESS );
            break;
        default:
            break;
    }

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : from ������ tail���� �ѹ��� ����
 *
 * + 2nd. code design
 *   - ������ f����� rid�� ���Ѵ�.
 *   - ������ f������� f-PREVNODE�� RID�� ��´�
 *   - ������ t����� rid�� ���Ѵ�.
 *   - ������ t������� t-NEXTNODE�� RID�� ��´�
 *   - if PREVNODE == ������ ��� then
 *     :���� PREVNODE�� ptr�� ��´�.
 *   - else
 *     :PREVNODE�� fix ��û 
 *   - PREVNODE�� next ��带 NEXTNODE�� ����(SDR_8BYTES)
 *   - if NEXTNODE == ������ ��� then
 *     :���� NEXTNODE�� ptr�� ��´�.
 *   - else
 *     :NEXTNODE�� fix ��û
 *   -  NEXTNODE�� prevnode�� PREVNODE�� ���� (SDR_8BYTES)
 *   - basenode�� ���̸� ���ؼ� 1 ���� (SDR_4BYTES)
 *
 * - ����
 *   1) ���ڰ� ��� not null Ȯ��
 ***********************************************************************/
IDE_RC sdpDblRIDList::removeNodesAtOnce( idvSQL            * aStatistics,
                                         sdpDblRIDListBase * aBaseNode,
                                         sdpDblRIDListNode * aFromNode,
                                         sdpDblRIDListNode * aToNode,
                                         ULong               aNodeCount,
                                         sdrMtx            * aMtx )
{
    scSpaceID            sSpaceID;
    ULong                sNodeCnt;
    idBool               sTrySuccess;
    sdRID                sFRID;
    sdRID                sTNextRID;
    sdpDblRIDListNode  * sTNextNode;
    sdRID                sFPrevRID;
    sdpDblRIDListNode  * sFPrevNode;
    UChar              * sPagePtr;

    IDE_DASSERT( aBaseNode != NULL );
    IDE_DASSERT( aFromNode != NULL );
    IDE_DASSERT( aMtx      != NULL );
    IDE_DASSERT( aNodeCount != 0 );

    IDE_DASSERT(getNodeCnt(aBaseNode) > 0);

    sSpaceID = sdpPhyPage::getSpaceID(
        sdpPhyPage::getPageStartPtr( (UChar*) aBaseNode ) );

    if (aNodeCount > 1)
    {
        IDE_ASSERT( aFromNode != aToNode );

        sFPrevRID = sdpDblRIDList::getPrvOfNode(aFromNode);

        IDE_ASSERT(sFPrevRID != SD_NULL_RID);

        // ������ f����� rid�� ���Ѵ�.
        sFRID     = sdpPhyPage::getRIDFromPtr((UChar*)aFromNode);
        IDE_ASSERT(sFRID != SD_NULL_RID);

        // ������ t����� NEXTNODE�� ��´�.
        sTNextRID = getNxtOfNode(aToNode);

        // t-NEXTNODE fix ��Ų��.
        if( isSamePage( &sTNextRID, &sFRID ) == ID_TRUE )
        {
            sTNextNode = (sdpDblRIDListNode*)( sdpPhyPage::getPageStartPtr( (UChar*)aFromNode )
                                               + SD_MAKE_OFFSET( sTNextRID ) );
        }
        else
        {
            sPagePtr = sdrMiniTrans::getPagePtrFromPageID( aMtx,
                                                           sSpaceID,
                                                           SD_MAKE_PID( sTNextRID) );

            if( sPagePtr == NULL )
            {
                IDE_TEST( sdbBufferMgr::getPageByRID( aStatistics,
                                                      sSpaceID,
                                                      sTNextRID,
                                                      SDB_X_LATCH,
                                                      SDB_WAIT_NORMAL,
                                                      aMtx,
                                                      (UChar**)&sTNextNode,
                                                      &sTrySuccess ) != IDE_SUCCESS );
            }
            else
            {
                sTNextNode = (sdpDblRIDListNode*)( sPagePtr + SD_MAKE_OFFSET( sTNextRID ) );
            }
        }

        IDE_DASSERT( sTNextNode != NULL );

        // f-PREVNODE fix ��Ų��.
        if( isSamePage( &sFPrevRID, &sFRID ) == ID_TRUE )
        {
            sFPrevNode = (sdpDblRIDListNode*)( sdpPhyPage::getPageStartPtr( (UChar*)aFromNode )
                                               + SD_MAKE_OFFSET( sFPrevRID ));
        }
        else
        {
            sPagePtr = sdrMiniTrans::getPagePtrFromPageID( aMtx,
                                                           sSpaceID,
                                                           SD_MAKE_PID( sFPrevRID) );

            if( sPagePtr == NULL )
            {
                IDE_TEST( sdbBufferMgr::getPageByRID( aStatistics,
                                                      sSpaceID,
                                                      sFPrevRID,
                                                      SDB_X_LATCH,
                                                      SDB_WAIT_NORMAL,
                                                      aMtx,
                                                      (UChar**)&sFPrevNode,
                                                      &sTrySuccess )
                      != IDE_SUCCESS );
            }
            else
            {
                sFPrevNode = (sdpDblRIDListNode*)( sPagePtr + SD_MAKE_OFFSET( sFPrevRID ));
            }
        }

        IDE_DASSERT( sFPrevNode != NULL );

        // t-NEXTNODE�� prev ��带 base ���� ����
        IDE_TEST( setPrvOfNode(sTNextNode, sFPrevRID, aMtx) != IDE_SUCCESS );
        // base ����� prev ��带 t-NEXTNODE�� ����
        IDE_TEST( setNxtOfNode(sFPrevNode, sTNextRID, aMtx) != IDE_SUCCESS );
        // t����� next ��带 SD_NULL_RID�� ����
        IDE_TEST( setNxtOfNode(aToNode, SD_NULL_RID, aMtx) != IDE_SUCCESS );
        // f����� prev ��带 SD_NULL_RID�� ����
        IDE_TEST( setPrvOfNode(aFromNode, SD_NULL_RID, aMtx) != IDE_SUCCESS );

        // ��� ���ŷ� ���� base ����� ����Ʈ ���� aNodeCount ����
        sNodeCnt = getNodeCnt(aBaseNode);
        IDE_DASSERT( sNodeCnt != 0 );
        IDE_DASSERT( sNodeCnt >= aNodeCount );
        setNodeCnt(aBaseNode, (sNodeCnt - aNodeCount), aMtx);
    }
    else
    {
        IDE_DASSERT( aFromNode == aToNode );

        IDE_TEST( removeNode(aStatistics,
                             aBaseNode, aFromNode, aMtx) != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : rid list�� tail�� �߰�
 *
 * + 2nd. code design
 *   - base ����� RID�� ��´�.
 *   - newnode�� RID�� ��´�.
 *   - base ����� PREVNODE�� ��´�.
 *   - newnode�� prev ��带 PREVNODE�� ���� (SDR_8BYTES)
 *   - newnode�� next ��带 base ���� ���� (SDR_8BYTES)
 *   - if newnode�� PREVNODE�� ������ page�� ���
 *     : ���� PREVNODE�� ptr�� ��´�.
 *   - else 
 *     : PREVNODE�� ���۰����ڿ� ��û�Ͽ� fix��Ų��,
 *       ������ ����
 *     => PREVNODE�� next ��带 newnode�� ���� (SDR_8BYTE)
 *   - base ����� prev ��带 newnode�� ����(SDR_8BYTE)
 *   - basenode�� ���̸� ���ؼ� +1 (SDR_4BYTE)
 *
 * - ����
 *   1) ���ڰ� ��� not null Ȯ��
 *   2) node != newnode Ȯ��
 ***********************************************************************/
IDE_RC sdpDblRIDList::insertNodesAtOnce(idvSQL               * aStatistics,
                                        sdpDblRIDListBase    * aBaseNode,
                                        sdpDblRIDListNode    * aFromNode,
                                        sdpDblRIDListNode    * aToNode,
                                        ULong                  aNodeCount,
                                        sdrMtx               * aMtx)
{

    scSpaceID             sSpaceID;
    sdRID                 sBaseRID;
    sdRID                 sNewFromRID;
    sdRID                 sNewToRID;
    sdRID                 sTailRID;
    ULong                 sNodeCnt;
    idBool                sTrySuccess;
    sdpDblRIDListNode   * sTailNode;
    UChar               * sPagePtr;

    IDE_DASSERT( aBaseNode != NULL );
    IDE_DASSERT( aFromNode != NULL );
    IDE_DASSERT( aToNode   != NULL );
    IDE_DASSERT( aMtx      != NULL );
    IDE_DASSERT( aNodeCount != 0 );

    sSpaceID = sdpPhyPage::getSpaceID( sdpPhyPage::getPageStartPtr( (UChar*)aBaseNode ));

    if( aNodeCount > 1 )
    {
        IDE_DASSERT( aFromNode != aToNode );

        sBaseRID    = sdpPhyPage::getRIDFromPtr( (UChar*)&aBaseNode->mBase );
        sNewFromRID = sdpPhyPage::getRIDFromPtr( (UChar*)aFromNode );
        sNewToRID   = sdpPhyPage::getRIDFromPtr( (UChar*)aToNode );

        // base ����� tail ���(prev RID)�� ��´�.
        sTailRID = getTailOfList( aBaseNode );

        // ������ page�� ���
        if( isSamePage( &sNewToRID, &sTailRID ) == ID_TRUE ) 
        {
            sTailNode = (sdpDblRIDListNode*)(sdpPhyPage::getPageStartPtr( (UChar*)aToNode )
                                             + SD_MAKE_OFFSET( sTailRID ));
        }
        else
        {
            sPagePtr = sdrMiniTrans::getPagePtrFromPageID( aMtx,
                                                           sSpaceID,
                                                           SD_MAKE_PID( sTailRID) );

            if( sPagePtr == NULL )
            {
                // PREVNODE�� fix ��Ų��.
                IDE_TEST( sdbBufferMgr::getPageByRID( aStatistics,
                                                      sSpaceID,
                                                      sTailRID,
                                                      SDB_X_LATCH,
                                                      SDB_WAIT_NORMAL,
                                                      aMtx,
                                                      (UChar**)&sTailNode,
                                                      &sTrySuccess )
                      != IDE_SUCCESS );
            }
            else
            {
                sTailNode = (sdpDblRIDListNode*)( sPagePtr + SD_MAKE_OFFSET( sTailRID ));

            }
        }
        IDE_DASSERT( sTailNode != NULL );

        IDE_TEST( setNxtOfNode( sTailNode, sNewFromRID, aMtx ) != IDE_SUCCESS );
        IDE_TEST( setPrvOfNode( &aBaseNode->mBase, sNewToRID, aMtx) != IDE_SUCCESS );

        IDE_TEST( setNxtOfNode( aToNode, sBaseRID, aMtx ) != IDE_SUCCESS );
        IDE_TEST( setPrvOfNode( aFromNode, sTailRID, aMtx ) != IDE_SUCCESS );

        // ���ο� ��� �߰��� ���� base ����� ����Ʈ ���� 1 ����
        sNodeCnt = getNodeCnt( aBaseNode );
        IDE_TEST( setNodeCnt(aBaseNode, ( sNodeCnt + aNodeCount ), aMtx )
                  != IDE_SUCCESS );
    }
    else
    {
        IDE_DASSERT( aFromNode == aToNode );

        IDE_TEST( insertTailNode( aStatistics,
                                  aBaseNode, aFromNode, aMtx )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}
