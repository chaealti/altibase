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
 * $Id: sdpDblPIDList.cpp 25608 2008-04-11 09:52:13Z kclee $
 *
 * Description :
 *
 * File-Based PID Double Linked-List ����
 *
 **********************************************************************/

#include <sdr.h>
#include <sdpDblPIDList.h>
#include <sdpPhyPage.h>

/***********************************************************************
 * Description : page list�� base ��带 �ʱ�ȭ
 *
 * + 2nd.code desgin
 *   - base ����� page ID�� ��´�.
 *   - base ����� mNodeCnt�� 0���� �α��Ѵ�. (SDR_4BYTE)
 *   - base ����� mHead�� Page ID�� self Page ID�� �α��Ѵ�.(SDR_4BYTE)
 *   - base ����� mTail�� Page ID�� self Page ID�� �α��Ѵ�.(SDR_4BYTE)
 ***********************************************************************/
IDE_RC sdpDblPIDList::initBaseNode(sdpDblPIDListBase  * aBaseNode,
                                   sdrMtx             * aMtx)
{
    scPageID sPID;
    UChar*   sPagePtr;

    IDE_DASSERT( aBaseNode != NULL );
    IDE_DASSERT( aMtx      != NULL );

    // get base page ID
    sPagePtr = sdpPhyPage::getPageStartPtr((UChar*)aBaseNode);
    sPID     = sdpPhyPage::getPageID(sPagePtr);

    // set length
    IDE_TEST( setNodeCnt(aBaseNode, 0, aMtx) != IDE_SUCCESS );
    // set head-node
    IDE_TEST( setNxtOfNode(&aBaseNode->mBase, sPID, aMtx) != IDE_SUCCESS );
    // set tail-node
    IDE_TEST( setPrvOfNode(&aBaseNode->mBase, sPID, aMtx) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : page list�� head�� ��带 �߰�
 *
 * + 2nd. code design
 *   => sdpDblPIDList::insertNodeAfter�� ȣ����
 ***********************************************************************/
IDE_RC sdpDblPIDList::insertHeadNode(idvSQL            * aStatistics,
                                     sdpDblPIDListBase * aBaseNode,
                                     sdpDblPIDListNode * aNewNode,
                                     sdrMtx            * aMtx)
{

    IDE_DASSERT( aBaseNode != NULL );
    IDE_DASSERT( aNewNode  != NULL );
    IDE_DASSERT( aMtx      != NULL );

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
 * Description :  page list�� tail�� ��带 �߰�
 *
 * + 2nd. code design
 *   => sdpDblPIDList::insertNodeBefore�� ȣ����
 ***********************************************************************/
IDE_RC sdpDblPIDList::insertTailNode(idvSQL              * aStatistics,
                                     sdpDblPIDListBase   * aBaseNode,
                                     sdpDblPIDListNode   * aNewNode,
                                     sdrMtx              * aMtx)
{

    IDE_DASSERT( aBaseNode != NULL );
    IDE_DASSERT( aNewNode  != NULL );
    IDE_DASSERT( aMtx      != NULL );


    IDE_TEST( insertNodeBefore(aStatistics,
                               aBaseNode,
                               &aBaseNode->mBase,
                               aNewNode,
                               aMtx) != IDE_SUCCESS );
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}


/***********************************************************************
 * Description :  page list�� tail�� Node List�� �߰�
 ***********************************************************************/
IDE_RC sdpDblPIDList::insertTailList( idvSQL             * aStatistics,
                                      sdpDblPIDListBase  * aBaseNode,
                                      sdpDblPIDListNode  * aFstNode,
                                      sdpDblPIDListNode  * aLstNode,
                                      ULong                aListLen,
                                      sdrMtx             * aMtx )
{

    scSpaceID       sSpaceID;
    scPageID        sBasePID;
    scPageID        sPrevPID;
    scPageID        sFstPID;
    scPageID        sLstPID;
    ULong           sListLen;
    idBool          sTrySuccess;
    UChar*          sPagePtr;      // base ����� page
    UChar*          sPrevPagePtr;  // ���� ����� previous page
    sdpDblPIDListNode* sPrevNode;

    IDE_DASSERT( aBaseNode != NULL );
    IDE_DASSERT( aFstNode  != NULL );
    IDE_DASSERT( aLstNode  != NULL );
    IDE_DASSERT( aMtx      != NULL );

    sPagePtr = (UChar*)sdpPhyPage::getHdr((UChar*)aBaseNode);
    sSpaceID = sdpPhyPage::getSpaceID((UChar*)sPagePtr);
    sBasePID = sdpPhyPage::getPageID(sPagePtr);
    sPrevPID = getPrvOfNode(&aBaseNode->mBase);

    IDE_ASSERT(sSpaceID == (sdpPhyPage::getSpaceID(
                                (UChar*)sdpPhyPage::getHdr((UChar*)aFstNode))));
    IDE_ASSERT(sSpaceID == (sdpPhyPage::getSpaceID(
                                (UChar*)sdpPhyPage::getHdr((UChar*)aLstNode))));

    sFstPID  = sdpPhyPage::getPageID((UChar*)sdpPhyPage::getHdr((UChar*)aFstNode));
    sLstPID  = sdpPhyPage::getPageID((UChar*)sdpPhyPage::getHdr((UChar*)aLstNode));

    IDE_TEST( setNxtOfNode(aLstNode, sBasePID, aMtx) != IDE_SUCCESS );
    IDE_TEST( setPrvOfNode(aFstNode, sPrevPID, aMtx) != IDE_SUCCESS );

    // PREVNODE�� next ��带 ���ο� ���� �����Ѵ�.
    if (sPrevPID != sBasePID )
    {
        sPrevPagePtr = sdrMiniTrans::getPagePtrFromPageID( aMtx,
                                                           sSpaceID,
                                                           sPrevPID );
        if( sPrevPagePtr == NULL )
        {
            IDE_TEST( sdbBufferMgr::getPageByPID( aStatistics,
                                                  sSpaceID,
                                                  sPrevPID,
                                                  SDB_X_LATCH,
                                                  SDB_WAIT_NORMAL,
                                                  SDB_SINGLE_PAGE_READ,
                                                  aMtx,
                                                  &sPrevPagePtr,
                                                  &sTrySuccess,
                                                  NULL /*IsCorruptPage*/ )
                      != IDE_SUCCESS );
            IDE_DASSERT( sPrevPagePtr != NULL );
        }

        IDE_ASSERT(sSpaceID == sdpPhyPage::getSpaceID(sPrevPagePtr));
        sPrevNode = sdpPhyPage::getDblPIDListNode((sdpPhyPageHdr*)sPrevPagePtr);
    }
    else
    {
        sPrevNode = &aBaseNode->mBase;
    }

    IDE_TEST( setNxtOfNode(sPrevNode, sFstPID, aMtx) != IDE_SUCCESS );

    // ���� ����� prev ��带 ���ο� ���� �����Ѵ�.
    IDE_TEST( setPrvOfNode(&aBaseNode->mBase, sLstPID, aMtx) != IDE_SUCCESS );

    // ���ο� ��� �߰��� ���� base ����� ����Ʈ ���� aListLen ����
    sListLen = getNodeCnt(aBaseNode);
    IDE_TEST( setNodeCnt(aBaseNode, (sListLen + aListLen), aMtx) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}


/***********************************************************************
 * Description : page list�� Ư�� ��� �ڿ� ���ο� ��� �߰�
 *
 * + 2nd. code design
 *   - ���� ����� page ID�� space ID�� ��´�.
 *   - newnode�� page ID�� ��´�.
 *   - ���� ����� NEXTNODE�� ��´�.
 *   - newnode�� prev ��带 ���� ���� ���� (SDR_4BYTES)
 *   - newnode�� next ��带 NEXTNODE�� ���� (SDR_4BYTES)
 *   - if newnode�� NEXTNODE�� ������ page�� ���
 *     : !! assert
 *   - else �׷��� ���� ���
 *     : NEXTNODE�� ���۰����ڿ� ��û�Ͽ� fix��Ų��,
 *       ������ ����
 *     => NEXTNODE�� prev ��带 newnode�� ���� (SDR_4BYTES)
 *   - ���� ����� next ��带 newnode�� ����(SDR_4BYTES)
 *   - basenode�� ���̸� ���ؼ� +1 (SDR_4BYTES)
 *
 * - ����
 *   1) ���ڰ� ��� not null Ȯ��
 *   2) node != newnode Ȯ��
 ***********************************************************************/
IDE_RC sdpDblPIDList::insertNodeAfter(idvSQL              * aStatistics,
                                      sdpDblPIDListBase   * aBaseNode,
                                      sdpDblPIDListNode   * aNode,
                                      sdpDblPIDListNode   * aNewNode,
                                      sdrMtx              * aMtx)
{

    scPageID        sPID;
    scPageID        sNewPID;
    scPageID        sNextPID;
    scSpaceID       sSpaceID;
    ULong           sListLen;
    idBool          sTrySuccess;
    UChar*          sPagePtr;
    UChar*          sNewPagePtr;
    UChar*          sNextPagePtr;
    sdpDblPIDListNode* sNextNode;

    IDE_DASSERT( aBaseNode != NULL );
    IDE_DASSERT( aNode     != NULL );
    IDE_DASSERT( aNewNode  != NULL );
    IDE_DASSERT( aMtx      != NULL );
    IDE_DASSERT( aNode     != aNewNode );

    // ���� ����� page ID�� ��´�. tnode
    sPagePtr    = sdpPhyPage::getPageStartPtr((UChar*)aNode);
    sSpaceID    = sdpPhyPage::getSpaceID(sPagePtr);
    sPID        = sdpPhyPage::getPageID(sPagePtr);

    // ���ο� ����� page ID�� ��´�. node
    sNewPagePtr = sdpPhyPage::getPageStartPtr((UChar*)aNewNode);
    sNewPID     = sdpPhyPage::getPageID(sNewPagePtr);

    IDE_ASSERT(sSpaceID == sdpPhyPage::getSpaceID(sNewPagePtr));
    IDE_ASSERT(sSpaceID == sdpPhyPage::getSpaceID(
                   (UChar*)sdpPhyPage::getHdr((UChar*)aNode)));

    // ���ο� ����� prev/next�� �����Ѵ�.
    sNextPID = getNxtOfNode(aNode); // get NEXTNODE

    // ������ page�� ��� Assert!!
    IDE_ASSERT( sNewPID != sNextPID );

    // NEXTNODE�� fix ��Ų��.
    IDE_TEST( sdbBufferMgr::getPageByPID( aStatistics,
                                          sSpaceID,
                                          sNextPID,
                                          SDB_X_LATCH,
                                          SDB_WAIT_NORMAL,
                                          SDB_SINGLE_PAGE_READ,
                                          aMtx,
                                          &sNextPagePtr,
                                          &sTrySuccess,
                                          NULL /*IsCorruptPage*/ )
              != IDE_SUCCESS );

    IDE_ASSERT(sSpaceID == sdpPhyPage::getSpaceID(sNextPagePtr));

    IDE_TEST( setPrvOfNode(aNewNode, sPID, aMtx) != IDE_SUCCESS ); // node->prev = tnode
    IDE_TEST( setNxtOfNode(aNewNode, sNextPID, aMtx) != IDE_SUCCESS ); // node->next = tnode->next

    // NEXTNODE�� prev ��带 ���ο� ���� �����Ѵ�.
    if (sNextPagePtr != sdpPhyPage::getPageStartPtr((UChar*)aBaseNode))
    {
        sNextNode = sdpPhyPage::getDblPIDListNode((sdpPhyPageHdr*)sNextPagePtr);
    }
    else
    {
        sNextNode = &aBaseNode->mBase;
    }

    IDE_TEST( setPrvOfNode(sNextNode, sNewPID, aMtx) != IDE_SUCCESS ); // tnode->next->prev = node

    // ���� ����� next ��带 ���ο� ���� �����Ѵ�.
    IDE_TEST( setNxtOfNode(aNode, sNewPID, aMtx) != IDE_SUCCESS ); // tnode->next = node

    // ���ο� ��� �߰��� ���� base ����� ����Ʈ ���� 1 ����
    sListLen = getNodeCnt(aBaseNode);
    IDE_TEST( setNodeCnt(aBaseNode, (sListLen + 1), aMtx) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

/***********************************************************************
 * Description : page list�� Ư�� ��� �տ� ���ο� ��� �߰�
 *
 * + 2nd. code design
 *   - ���� ����� page ID�� space id�� �˾Ƴ�
 *   - newnode�� page ID�� ����
 *   - ���� ����� PREVNODE�� ����
 *   - newnode�� prev ��带 PREVNODE�� ���� (SDR_4BYTES)
 *   - newnode�� next ��带 ���� ���� ���� (SDR_4BYTES)
 *   - if newnode�� PREVNODE�� ������ page�� ���
 *     : !! assert
 *   - else
 *     : PREVNODE�� ���۰����ڿ� ��û�Ͽ� fix��Ų��, ������ ����
 *     => PREVNODE�� next ��带 new ���� ����(SDR_4BYTES)
 *   - ���� ����� prev ��带 newnode�� ���� ����(SDR_4BYTES)
 *   - basenode�� ���̸� ���ؼ� +1 (SDR_4BYTES)
 *
 * - ����
 *   1) ���ڰ� ��� not null Ȯ��
 *   2) node != newnode Ȯ��
 ***********************************************************************/
IDE_RC sdpDblPIDList::insertNodeBeforeLow(idvSQL             * aStatistics,
                                          sdpDblPIDListBase  * aBaseNode,
                                          sdpDblPIDListNode  * aNode,    // tnode
                                          sdpDblPIDListNode  * aNewNode, // node
                                          sdrMtx             * aMtx,
                                          idBool               aNeedGetPage)
{
    scPageID            sPID;
    scPageID            sNewPID;
    scPageID            sPrevPID;
    scSpaceID           sSpaceID;
    ULong               sListLen;
    idBool              sTrySuccess;
    UChar             * sPagePtr;      // ���� ����� page
    UChar             * sNewPagePtr;   // ���ο� ����� page
    UChar             * sPrevPagePtr;  // ���� ����� previous page
    sdpDblPIDListNode * sPrevNode;

    IDE_DASSERT( aBaseNode != NULL );
    IDE_DASSERT( aNode     != NULL );
    IDE_DASSERT( aNewNode  != NULL );
    IDE_DASSERT( aMtx      != NULL );
    IDE_DASSERT( aNode     != aNewNode );

    // ���� ����� page ID�� ��´�. tnode
    sPagePtr = sdpPhyPage::getPageStartPtr((UChar*)aNode);
    sPID     = sdpPhyPage::getPageID(sPagePtr);
    sSpaceID = sdpPhyPage::getSpaceID(sPagePtr);

    // ���ο� ����� page ID�� ��´�. node
    sNewPagePtr = sdpPhyPage::getPageStartPtr((UChar*)aNewNode);
    sNewPID     = sdpPhyPage::getPageID(sNewPagePtr);

    IDE_ASSERT(sSpaceID == sdpPhyPage::getSpaceID(sNewPagePtr));
    IDE_ASSERT(sSpaceID == sdpPhyPage::getSpaceID(
                   (UChar*)sdpPhyPage::getHdr((UChar*)aNode)));

    // ���ο� ����� prev/next�� �����Ѵ�.
    sPrevPID = getPrvOfNode(aNode); // get PREVNODE

    if( ( aNeedGetPage == ID_TRUE ) && ( sNewPID != sPrevPID ) )
    {
        // PREVNODE fix ��Ų��.
        IDE_TEST( sdbBufferMgr::getPageByPID( aStatistics,
                                              sSpaceID,
                                              sPrevPID,
                                              SDB_X_LATCH,
                                              SDB_WAIT_NORMAL,
                                              SDB_SINGLE_PAGE_READ,
                                              aMtx,
                                              &sPrevPagePtr,
                                              &sTrySuccess,
                                              NULL /*IsCorruptPage*/ )
                  != IDE_SUCCESS );
    }
    else
    {
        sPrevPagePtr = sdrMiniTrans::getPagePtrFromPageID( aMtx,
                                                           sSpaceID,
                                                           sPrevPID );
        IDE_ASSERT( sPrevPagePtr != NULL );
    }

    IDE_ASSERT(sSpaceID == sdpPhyPage::getSpaceID(sPrevPagePtr));

    IDE_TEST( setNxtOfNode(aNewNode, sPID, aMtx) != IDE_SUCCESS );     // node->next = tnode
    IDE_TEST( setPrvOfNode(aNewNode, sPrevPID, aMtx) != IDE_SUCCESS ); // node->prev = tnode->prev

    // ���ο� ��� �߰��� ���� base ����� ����Ʈ ���� 1 ����
    sListLen = getNodeCnt(aBaseNode);

    // PREVNODE�� next ��带 ���ο� ���� �����Ѵ�.
    if (sPrevPagePtr != sdpPhyPage::getPageStartPtr((UChar*)aBaseNode) || sListLen != 0 )
    {
        sPrevNode = sdpPhyPage::getDblPIDListNode((sdpPhyPageHdr*)sPrevPagePtr);
    }
    else
    {
        sPrevNode = &aBaseNode->mBase;
    }

    IDE_TEST( setNxtOfNode(sPrevNode, sNewPID, aMtx) != IDE_SUCCESS ); // tnode->prev->next = node

    // ���� ����� prev ��带 ���ο� ���� �����Ѵ�.
    IDE_TEST( setPrvOfNode(aNode, sNewPID, aMtx) != IDE_SUCCESS );  // tnode->prev = node

    IDE_TEST( setNodeCnt(aBaseNode, sListLen + 1, aMtx) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

/***********************************************************************
 * Description :
 *
 ***********************************************************************/
IDE_RC sdpDblPIDList::insertNodeBefore(idvSQL             * aStatistics,
                                       sdpDblPIDListBase  * aBaseNode,
                                       sdpDblPIDListNode  * aNode,    // tnode
                                       sdpDblPIDListNode  * aNewNode, // node
                                       sdrMtx             * aMtx)
{


    IDE_TEST( insertNodeBeforeLow( aStatistics,
                                   aBaseNode,
                                   aNode,
                                   aNewNode,
                                   aMtx,
                                   ID_TRUE )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

/***********************************************************************
 * Description :
 *
 ***********************************************************************/
IDE_RC sdpDblPIDList::insertNodeBeforeWithNoFix(idvSQL             * aStatistics,
                                                sdpDblPIDListBase  * aBaseNode,
                                                sdpDblPIDListNode  * aNode,
                                                sdpDblPIDListNode  * aNewNode,
                                                sdrMtx             * aMtx)
{


    IDE_TEST( insertNodeBeforeLow( aStatistics,
                                   aBaseNode,
                                   aNode,
                                   aNewNode,
                                   aMtx,
                                   ID_FALSE )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

/***********************************************************************
 * Description : page list���� Ư����� ����
 *
 * + 2nd. code design
 *   - ������ ������� PREVNODE/NEXTNODE�� page ID�� ��´�
 *   - if PREVNODE == ������ ��� then
 *     :!! assert
 *   - else
 *     :PREVNODE�� fix ��û
 *   - PREVNODE�� next ��带 NEXTNODE�� ����(SDR_4BYTES)
 *   - if NEXTNODE == ������ ��� then
 *     :!! assert
 *   - else
 *     :NEXTNODE�� fix ��û
 *   - NEXTNODE�� prevnode�� PREVNODE�� ���� (SDR_4BYTES)
 *   - basenode�� ���̸� ���ؼ� 1 ���� (SDR_4BYTES)
 *
 * - ����
 *   1) ���ڰ� ��� not null Ȯ��
 *   2) basenode != node
 *   3) basenode, node�� ���� fix(x-latch) Ȯ��
 ***********************************************************************/
IDE_RC sdpDblPIDList::removeNodeLow( idvSQL             * aStatistics,
                                     sdpDblPIDListBase  * aBaseNode,
                                     sdpDblPIDListNode  * aNode,
                                     sdrMtx             * aMtx,
                                     idBool               aCheckMtxStack )
{
    ULong               sListLen;
    idBool              sTrySuccess;
    scSpaceID           sSpaceID;
    scPageID            sPrevPID;
    scPageID            sNextPID;
    UChar              *sPagePtr;
    UChar              *sPrevPagePtr;
    UChar              *sNextPagePtr;
    sdpDblPIDListNode  *sNextNode;
    sdpDblPIDListNode  *sPrevNode;

    IDE_DASSERT( aBaseNode != NULL );
    IDE_DASSERT( aNode     != NULL );
    IDE_DASSERT( aMtx      != NULL );

    // ���� ����� page ID�� space ID�� ��´�.
    sPagePtr = sdpPhyPage::getPageStartPtr((UChar*)aNode);
    sSpaceID = sdpPhyPage::getSpaceID(sPagePtr);

    IDE_ASSERT(sSpaceID == sdpPhyPage::getSpaceID(sPagePtr));

    // ���� ����� PREVNODE/NEXTNODE�� ��´�.
    sPrevPID = getPrvOfNode(aNode);
    sNextPID = getNxtOfNode(aNode);

    if(aCheckMtxStack == ID_FALSE)
    {
        // PREVNODE fix ��Ų��.
        IDE_TEST( sdbBufferMgr::getPageByPID( aStatistics,
                                              sSpaceID,
                                              sPrevPID,
                                              SDB_X_LATCH,
                                              SDB_WAIT_NORMAL,
                                              SDB_SINGLE_PAGE_READ,
                                              aMtx,
                                              &sPrevPagePtr,
                                              &sTrySuccess,
                                              NULL /*IsCorruptPage*/ )
                  != IDE_SUCCESS );

        // NEXTNODE fix ��Ų��.
        IDE_TEST( sdbBufferMgr::getPageByPID( aStatistics,
                                              sSpaceID,
                                              sNextPID,
                                              SDB_X_LATCH,
                                              SDB_WAIT_NORMAL,
                                              SDB_SINGLE_PAGE_READ,
                                              aMtx,
                                              &sNextPagePtr,
                                              &sTrySuccess,
                                              NULL /*IsCorruptPage*/ ) != IDE_SUCCESS );
    }
    else
    {
        sPrevPagePtr = sdrMiniTrans::getPagePtrFromPageID(
            aMtx,
            sSpaceID,
            sPrevPID );
        if ( sPrevPagePtr == NULL )
        {
            // PREVNODE fix ��Ų��.
            IDE_TEST( sdbBufferMgr::getPageByPID( aStatistics,
                                                  sSpaceID,
                                                  sPrevPID,
                                                  SDB_X_LATCH,
                                                  SDB_WAIT_NORMAL,
                                                  SDB_SINGLE_PAGE_READ,
                                                  aMtx,
                                                  &sPrevPagePtr,
                                                  &sTrySuccess,
                                                  NULL /*IsCorruptPage*/ )
                      != IDE_SUCCESS );
        }

        sNextPagePtr = sdrMiniTrans::getPagePtrFromPageID(
            aMtx,
            sSpaceID,
            sNextPID );
        if (sNextPagePtr == NULL)
        {
            // NEXTNODE fix ��Ų��.
            IDE_TEST( sdbBufferMgr::getPageByPID( aStatistics,
                                                  sSpaceID,
                                                  sNextPID,
                                                  SDB_X_LATCH,
                                                  SDB_WAIT_NORMAL,
                                                  SDB_SINGLE_PAGE_READ,
                                                  aMtx,
                                                  &sNextPagePtr,
                                                  &sTrySuccess,
                                                  NULL /*IsCorruptPage*/ )
                      != IDE_SUCCESS );
        }
    }

    IDE_ASSERT(sSpaceID == sdpPhyPage::getSpaceID(sNextPagePtr));
    IDE_ASSERT(sSpaceID == sdpPhyPage::getSpaceID(sPrevPagePtr));

    // PREVNODE�� next ��带 NEXTNODE�� ����
    if (sPrevPagePtr != sdpPhyPage::getPageStartPtr((UChar*)aBaseNode))
    {
        sPrevNode = sdpPhyPage::getDblPIDListNode((sdpPhyPageHdr*)sPrevPagePtr);
    }
    else
    {
        sPrevNode = &aBaseNode->mBase;
    }
    IDE_TEST( setNxtOfNode(sPrevNode, sNextPID, aMtx) != IDE_SUCCESS );

    // NEXTNODE�� prev ��带 ���ο� ���� �����Ѵ�.
    if (sNextPagePtr != sdpPhyPage::getPageStartPtr((UChar*)aBaseNode))
    {
        sNextNode = sdpPhyPage::getDblPIDListNode((sdpPhyPageHdr*)sNextPagePtr);
    }
    else
    {
        sNextNode = &aBaseNode->mBase;
    }
    IDE_TEST( setPrvOfNode(sNextNode, sPrevPID, aMtx) != IDE_SUCCESS );

    // ��� ���ŷ� ���� base ����� ����Ʈ ���� 1 ����
    sListLen = getNodeCnt(aBaseNode);
    IDE_ASSERT( sListLen != 0 );
    IDE_TEST( setNodeCnt(aBaseNode, (sListLen - 1), aMtx) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description :
 ***********************************************************************/
IDE_RC sdpDblPIDList::removeNode(idvSQL              * aStatistics,
                                 sdpDblPIDListBase   * aBaseNode,
                                 sdpDblPIDListNode   * aNode,
                                 sdrMtx              * aMtx)
{
    IDE_TEST( removeNodeLow(aStatistics,
                            aBaseNode,
                            aNode,
                            aMtx,
                            ID_FALSE)
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/***********************************************************************
 * Description :
 *
 ***********************************************************************/
IDE_RC sdpDblPIDList::removeNodeWithNoFix(idvSQL              * aStatistics,
                                          sdpDblPIDListBase   * aBaseNode,
                                          sdpDblPIDListNode   * aNode,
                                          sdrMtx              * aMtx)
{
    IDE_TEST( removeNodeLow(aStatistics,
                            aBaseNode,
                            aNode,
                            aMtx,
                            ID_TRUE)
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : base ����� length ���� �� logging
 * base ����� list length�� �����Ѵ�.
 ***********************************************************************/
IDE_RC sdpDblPIDList::setNodeCnt( sdpDblPIDListBase *aBaseNode,
                                  ULong              aLen,
                                  sdrMtx            *aMtx )
{

    IDE_DASSERT( aBaseNode != NULL );
    IDE_DASSERT( aMtx != NULL );

    IDE_TEST( sdrMiniTrans::writeNBytes(aMtx,
                                        (UChar*)&aBaseNode->mNodeCnt,
                                        &aLen,
                                        ID_SIZEOF(aLen) ) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

/***********************************************************************
 * Description : ����� prev page ID ���� �� logging
 * aNode�� base ����̸� tail ��带 �����ϰ�, �׷��� ������ next ��带
 * �����Ѵ�.
 ***********************************************************************/
IDE_RC sdpDblPIDList::setPrvOfNode(sdpDblPIDListNode  * aNode,
                                   scPageID             aPageID,
                                   sdrMtx             * aMtx)
{

    IDE_DASSERT( aNode != NULL );
    IDE_DASSERT( aMtx != NULL );

    IDE_TEST( sdrMiniTrans::writeNBytes(aMtx,
                                        (UChar*)&aNode->mPrev,
                                        &aPageID,
                                        ID_SIZEOF(aPageID) ) != IDE_SUCCESS );
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

/***********************************************************************
 * Description : ����� prev page ID ���� �� logging
 ***********************************************************************/
IDE_RC sdpDblPIDList::setNxtOfNode(sdpDblPIDListNode  * aNode,
                                   scPageID             aPageID,
                                   sdrMtx             * aMtx)
{

    IDE_DASSERT( aNode != NULL );
    IDE_DASSERT( aMtx != NULL );

    IDE_TEST( sdrMiniTrans::writeNBytes(aMtx,
                                        (UChar*)&aNode->mNext,
                                        &aPageID,
                                        ID_SIZEOF(aPageID) ) != IDE_SUCCESS );
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

/***********************************************************************
 * Description : ����Ʈ�� ��� node ���
 ***********************************************************************/
IDE_RC sdpDblPIDList::dumpList( scSpaceID  aSpaceID,
                                sdRID      aBaseNodeRID )
{
    UInt                i;
    ULong               sNodeCount;
    sdrMtx              sMtx;
    UChar             * sPagePtr;
    sdpDblPIDListBase * sBaseNode;
    idBool              sTrySuccess;
    scPageID            sCurPageID;
    scPageID            sBasePageID;
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

    IDE_TEST( sdbBufferMgr::getPageByRID(NULL, /* idvSQL* */
                                         aSpaceID,
                                         aBaseNodeRID,
                                         SDB_S_LATCH,
                                         SDB_WAIT_NORMAL,
                                         &sMtx,
                                         (UChar**)&sBaseNode,
                                         &sTrySuccess) != IDE_SUCCESS );

    sBasePageID = SD_MAKE_PID(aBaseNodeRID);
    sNodeCount  = getNodeCnt(sBaseNode);
    sCurPageID = getListHeadNode(sBaseNode);

    for (i = 0; sCurPageID != sBasePageID; i++)
    {
        IDE_TEST( sdbBufferMgr::fixPageByPID( NULL,
                                              aSpaceID,
                                              sCurPageID,
                                              &sPagePtr,
                                              &sTrySuccess) != IDE_SUCCESS );
        sState = 2;

        idlOS::printf("%"ID_UINT32_FMT"/%"ID_UINT32_FMT" : pageID %"ID_UINT32_FMT"\n",
                      i + 1,
                      sNodeCount,
                      sdpPhyPage::getPageID(sPagePtr));

        sCurPageID = getNxtOfNode(sdpPhyPage::getDblPIDListNode((sdpPhyPageHdr*)sPagePtr));

        sState = 1;
        IDE_TEST( sdbBufferMgr::unfixPage( NULL, /* idvSQL* */
                                           sPagePtr )
                  != IDE_SUCCESS );
    }

    sState = 0;
    IDE_TEST( sdrMiniTrans::commit(&sMtx) != IDE_SUCCESS );

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
 * Description : ����Ʈ�� dump check.
 ***********************************************************************/
IDE_RC sdpDblPIDList::dumpCheck( sdpDblPIDListBase  * aBaseNode,
                                 scSpaceID            aSpaceID,
                                 scPageID             aPageID )
{
    UInt                i;
    ULong               sNodeCount;
    UChar*              sPagePtr;
    idBool              sTrySuccess;
    scPageID            sCurPageID;
    UShort              sState = 0;

    IDE_ERROR( aBaseNode != NULL );

    sNodeCount  = getNodeCnt(aBaseNode);
    sCurPageID  = getListHeadNode(aBaseNode);

    for (i = 0;  i < sNodeCount; i++)
    {
        IDE_TEST( sdbBufferMgr::fixPageByPID( NULL,
                                              aSpaceID,
                                              sCurPageID,
                                              &sPagePtr,
                                              &sTrySuccess) != IDE_SUCCESS );
        sState = 1;
        if( sCurPageID ==  aPageID)
        {
            IDE_ASSERT(0);
        }

        sCurPageID = getNxtOfNode(sdpPhyPage::getDblPIDListNode((sdpPhyPageHdr*)sPagePtr));

        sState = 0;

        IDE_TEST( sdbBufferMgr::unfixPage( NULL, /* idvSQL* */
                                           sPagePtr ) != IDE_SUCCESS );
    }
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    {
        if( sState != 0)
        {
            IDE_ASSERT( sdbBufferMgr::unfixPage(NULL, /* idvSQL* */
                                                sPagePtr)
                        == IDE_SUCCESS);
        }
    }
    return IDE_FAILURE;
}
