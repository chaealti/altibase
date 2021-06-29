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
 * $Id: sdpSglRIDList.h 22687 2007-07-23 01:01:41Z bskim $
 *
 * Description :
 *
 * File Based RID Linked-List ������
 *
 *
 * # ����
 *
 * tablespace�� ������ ���屸���� �����ϱ� ���� 
 * �ڷᱸ���̴�.
 *
 * # ����
 *  + tablespace�� ����Ǵ� ���� ������ RID list ����
 *      - tablespace�� ext.desc free list   (base)
 *      - segment desc�� ext.desc.full list (base)
 *      - segment desc�� ext.desc.free list (base)
 *      - extent desc�� ext.desc.list       (node)
 *
 *    �Ӹ��ƴ϶�,
 *
 *      - TSS RID list (base)
 *      - used USH list (base)
 *      - free USH list (base)
 *
 * 
 * # ����
 *                 sdpSglRIDListBase
 *                  ______________
 *                  |____len_____|        
 *          ________|tail_|_head_|________
 *         |    ____/        \_____      |
 *         |  _/_________   _______\___  |
 *         |  |next|prev|<--|next|prev|  |
 *         -->|____|____|-->|____|____|<--
 *
 *         sdpSglRIDListNode
 *
 * RID ����Ʈ�� ������ tablespace������ ���� ������ �����ϸ�, ���Ͱ���
 * circular double linked-list�� ������ �����Ѵ�. ����Ʈ�� ���� ���濬����
 * base ��带 fix�� ���¿��� �̷�������Ѵ�.
 * sdpPIDList�� �޸� ������ page���� ����Ʈ�� ���� ��尡 ������ �� �ִ�.
 *
 * # Releate data structure
 *
 * sdpSglRIDListBase ����ü
 * sdpSglRIDListNode ����ü
 *
 **********************************************************************/

#ifndef _O_SDP_SGL_RIDLIST_H_
#define _O_SDP_SGL_RIDLIST_H_ 1

#include <smDef.h>
#include <sdpDef.h>
#include <sdpPhyPage.h>

class sdpSglRIDList
{
public:

    /* RID list�� base ��带 �ʱ�ȭ */
    static IDE_RC initList( sdpSglRIDListBase*    aBaseNode,
                            sdrMtx*               aMtx );

    static IDE_RC initList( sdpSglRIDListBase* aBaseNode,
                            sdRID              aHeadRID,
                            sdRID              aTailRID,
                            ULong              aNodeCnt,
                            sdrMtx*            aMtx );

    /* RID list�� head�� ��带 �߰� */
    static IDE_RC addNode2Head( sdpSglRIDListBase   * aBaseNode,
                                sdpSglRIDListNode   * aNewNode,
                                sdrMtx              * aMtx );

    static IDE_RC addNode2Tail( idvSQL             * aStatistics,
                                scSpaceID            aSpaceID,
                                sdpSglRIDListBase  * aBase,
                                sdRID                aNodeRID,
                                sdrMtx             * aMtx );

    /* RID list�� tail�� ��带 �߰� */
    static IDE_RC addNode2Tail( idvSQL             * aStatistics,
                                scSpaceID            aSpaceID,
                                sdpSglRIDListBase  * aBaseNode,
                                sdpSglRIDListNode  * aNewNode,
                                sdrMtx             * aMtx );

    static IDE_RC addListToHead( sdpSglRIDListBase  * aBase,
                                 sdRID                aFstRID,
                                 sdRID                aLstRID,
                                 sdpSglRIDListNode  * aLstNode,
                                 ULong                aItemCnt,
                                 sdrMtx             * aMtx );

    static IDE_RC addListToTail( idvSQL             * aStatistics,
                                 sdpSglRIDListBase  * aBase,
                                 scSpaceID            aSpaceID,
                                 sdRID                aFstRID,
                                 sdRID                aLstRID,
                                 ULong                aItemCnt,
                                 sdrMtx             * aMtx );

    static IDE_RC removeNodeAtHead( idvSQL             * aStatistics,
                                    scSpaceID            aSpaceID,
                                    sdpSglRIDListBase  * aBase,
                                    sdrMtx             * aMtx,
                                    sdRID              * aRemovedRID,
                                    sdpSglRIDListNode ** aRemovedListNode );

    static IDE_RC initNode( sdpSglRIDListNode   * aNode,
                            sdrMtx              * aMtx );

    /* ����Ʈ�� ��� ��� ��� */
    static IDE_RC dumpList( scSpaceID aSpaceID,
                            sdRID     aBaseNodeRID );

    /* base ����� head ��� ��� */
    static inline sdRID getBaseNodeRID(sdpSglRIDListBase*   aBaseNode);

    /* base ����� length ��� */
    static inline ULong getNodeCnt(sdpSglRIDListBase*   aBaseNode);

    /* base ����� head ��� ��� */
    static inline sdRID getHeadOfList(sdpSglRIDListBase*   aBaseNode);

    /* base ����� tail ��� ��� */
    static inline sdRID getTailOfList(sdpSglRIDListBase*   aBaseNode);

    /* ����� next ��� ��� */
    static inline sdRID getNxtOfNode(sdpSglRIDListNode*   aNode);

    static inline IDE_RC setHeadOfList( sdpSglRIDListBase * aBaseNode,
                                        sdRID               aHead,
                                        sdrMtx            * aMtx );

    static inline IDE_RC setTailOfList( sdpSglRIDListBase * aBaseNode,
                                        sdRID                aTail,
                                        sdrMtx             * aMtx );

    /* base ����� length ���� �� logging */
    static inline IDE_RC setNodeCnt( sdpSglRIDListBase*  aBaseNode,
                                     ULong               aNodeCnt,
                                     sdrMtx*             aMtx );

    /* ����� next ��� ���� �� logging */
    static inline IDE_RC setNxtOfNode( sdpSglRIDListNode*  aNode,
                                       sdRID               aNextRID,
                                       sdrMtx*             aMtx );

private:
    // rid�� ������ page�� �����ϴ��� �˻�
    inline static idBool isSamePage( sdRID*     aLhs,
                                     sdRID*     aRhs );

    inline static void validate( sdpSglRIDListBase * aBase );

    // Validate Function
    inline static IDE_RC validateNxtIsNULL( idvSQL    * aStatistics,
                                            scSpaceID   aSpaceID,
                                            sdRID       aRID );
};

/***********************************************************************
 * Description : base->mBase ptr�� rid�� ��ȯ�Ͽ� ��ȯ
 ***********************************************************************/
inline sdRID sdpSglRIDList::getBaseNodeRID(sdpSglRIDListBase*  aBaseNode)
{
    IDE_DASSERT( aBaseNode != NULL );
    return sdpPhyPage::getRIDFromPtr((UChar*)aBaseNode);
}

/***********************************************************************
 * Description : ������ Page���� rid���� �˻�
 ***********************************************************************/
inline idBool sdpSglRIDList::isSamePage( sdRID* aLhs, sdRID* aRhs )
{
    IDE_DASSERT( aLhs != NULL );
    IDE_DASSERT( aRhs != NULL );

    return ((SD_MAKE_PID(*aLhs) == SD_MAKE_PID(*aRhs))) ? ID_TRUE : ID_FALSE;
}

/***********************************************************************
 * Description : Base ����� length ��ȯ
 ***********************************************************************/
inline ULong sdpSglRIDList::getNodeCnt( sdpSglRIDListBase*   aBaseNode )
{
    IDE_DASSERT( aBaseNode != NULL );

    return aBaseNode->mNodeCnt;
}

/***********************************************************************
 * Description : ����� next ��� ��ȯ Ȥ�� base ����� tail ��� ��ȯ
 ***********************************************************************/
inline sdRID sdpSglRIDList::getNxtOfNode( sdpSglRIDListNode*  aNode)
{
    IDE_DASSERT( aNode != NULL );

    return aNode->mNext;
}

/***********************************************************************
 * Description : base ����� head ��� ��ȯ
 ***********************************************************************/
inline sdRID sdpSglRIDList::getHeadOfList( sdpSglRIDListBase*   aBaseNode )
{

    IDE_DASSERT( aBaseNode != NULL );

    return aBaseNode->mHead;
}

/***********************************************************************
 * Description : base ����� tail ��� ��ȯ
 ***********************************************************************/
inline sdRID sdpSglRIDList::getTailOfList( sdpSglRIDListBase*   aBaseNode )
{
    IDE_DASSERT( aBaseNode != NULL );

    return aBaseNode->mTail;
}

/***********************************************************************
 * Description : base ����� length ���� �� logging
 * base ����� list length�� �����Ѵ�.
 ***********************************************************************/
inline IDE_RC sdpSglRIDList::setNodeCnt( sdpSglRIDListBase* aBaseNode,
                                         ULong              aNodeCnt,
                                         sdrMtx*            aMtx )
{
    IDE_DASSERT( aBaseNode != NULL );
    IDE_DASSERT( aMtx      != NULL );

    IDE_TEST( sdrMiniTrans::writeNBytes( aMtx,
                                         (UChar*)&aBaseNode->mNodeCnt,
                                         &aNodeCnt,
                                         ID_SIZEOF( aNodeCnt ) )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : ����� Head RID ���� �� logging
 ***********************************************************************/
inline IDE_RC sdpSglRIDList::setHeadOfList( sdpSglRIDListBase * aBaseNode,
                                            sdRID               aHead,
                                            sdrMtx            * aMtx )
{
    IDE_DASSERT( aBaseNode != NULL );
    IDE_DASSERT( aMtx      != NULL );

    IDE_TEST( sdrMiniTrans::writeNBytes( aMtx,
                                         (UChar*)&aBaseNode->mHead,
                                         &aHead,
                                         ID_SIZEOF( aHead ) )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : ����� Tail RID ���� �� logging
 ***********************************************************************/
inline IDE_RC sdpSglRIDList::setTailOfList( sdpSglRIDListBase  * aBaseNode,
                                            sdRID                aTail,
                                            sdrMtx             * aMtx )
{
    IDE_DASSERT( aBaseNode != NULL );
    IDE_DASSERT( aMtx      != NULL );

    IDE_TEST( sdrMiniTrans::writeNBytes( aMtx,
                                         (UChar*)&aBaseNode->mTail,
                                         &aTail,
                                         ID_SIZEOF( aTail ) )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : ����� next RID ���� �� logging
 ***********************************************************************/
inline IDE_RC sdpSglRIDList::setNxtOfNode( sdpSglRIDListNode * aNode,
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

inline void sdpSglRIDList::validate( sdpSglRIDListBase * aBase )
{
    if( getNodeCnt( aBase ) == 1 )
    {
        IDE_ASSERT( aBase->mHead == aBase->mTail );
        IDE_ASSERT( aBase->mHead != SD_NULL_RID );
    }

    if( getNodeCnt( aBase ) == 0 )
    {
        IDE_ASSERT( aBase->mHead == aBase->mTail );
        IDE_ASSERT( aBase->mHead == SD_NULL_RID );
    }

    if( getNodeCnt( aBase ) != 0 )
    {
        IDE_ASSERT( aBase->mHead != SD_NULL_RID );
        IDE_ASSERT( aBase->mTail != SD_NULL_RID );
    }
}

inline IDE_RC sdpSglRIDList::validateNxtIsNULL( idvSQL    * aStatistics,
                                                scSpaceID   aSpaceID,
                                                sdRID       aRID )
{
    sdpSglRIDListNode *sNode;
    idBool             sDummy;

    IDE_TEST( sdbBufferMgr::fixPageByRID( aStatistics,
                                          aSpaceID,
                                          aRID,
                                          (UChar**)&sNode,
                                          &sDummy )
              != IDE_SUCCESS );

    IDE_ASSERT( sNode->mNext == SD_NULL_RID );

    IDE_TEST( sdbBufferMgr::unfixPage( aStatistics,
                                       (UChar*)sNode )
              != IDE_SUCCESS );


    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

#endif // _O_SDP_SGL_RIDLIST_H_

