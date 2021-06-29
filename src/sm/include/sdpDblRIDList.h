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
 * $Id: sdpDblRIDList.h 22687 2007-07-23 01:01:41Z bskim $
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
 *                 sdpDblRIDListBase
 *                  ______________
 *                  |____len_____|
 *          ________|tail_|_head_|________
 *         |    ____/        \_____      |
 *         |  _/_________   _______\___  |
 *         |  |next|prev|<--|next|prev|  |
 *         -->|____|____|-->|____|____|<--
 *
 *         sdpDblRIDListNode
 *
 * RID ����Ʈ�� ������ tablespace������ ���� ������ �����ϸ�, ���Ͱ���
 * circular double linked-list�� ������ �����Ѵ�. ����Ʈ�� ���� ���濬����
 * base ��带 fix�� ���¿��� �̷�������Ѵ�.
 * sdpPIDList�� �޸� ������ page���� ����Ʈ�� ���� ��尡 ������ �� �ִ�.
 *
 * # Releate data structure
 *
 * sdpDblRIDListBase ����ü
 * sdpDblRIDListNode ����ü
 *
 **********************************************************************/

#ifndef _O_SDP_DBL_RIDLIST_H_
#define _O_SDP_DBL_RIDLIST_H_ 1

#include <smDef.h>
#include <sdpDef.h>
#include <sdpPhyPage.h>

class sdpDblRIDList
{
public:

    /* RID list�� base ��带 �ʱ�ȭ */
    static IDE_RC initList(sdpDblRIDListBase* aBaseNode,
                           sdrMtx*         aMtx);

    /* RID list�� head�� ��带 �߰� */
    static IDE_RC insertHeadNode(idvSQL          *aStatistics,
                                 sdpDblRIDListBase*  aBaseNode,
                                 sdpDblRIDListNode*  aNewNode,
                                 sdrMtx*          aMtx);

    /* RID list�� tail�� ��带 �߰� */
    static IDE_RC insertTailNode(idvSQL          *aStatistics,
                                 sdpDblRIDListBase*  aBaseNode,
                                 sdpDblRIDListNode*  aNewNode,
                                 sdrMtx*          aMtx);

    /* RID list�� Ư�� ��� �ڿ� ���ο� ��� �߰� */
    static IDE_RC insertNodeAfter(idvSQL            *aStatistics,
                                  sdpDblRIDListBase*   aBaseNode,
                                  sdpDblRIDListNode*   aNode,
                                  sdpDblRIDListNode*   aNewNode,
                                  sdrMtx*           aMtx);

    /* ����Ʈ ������ Node�� �ű��. */
    static IDE_RC moveNodeInList( idvSQL          *  aStatistics,
                                  sdpDblRIDListBase*    aBaseNode,
                                  sdpDblRIDListNode*    aDestNode,
                                  sdpDblRIDListNode*    aSrcNode,
                                  sdrMtx          *  aMtx );

    /* RID list�� Ư�� ��� �տ� ���ο� ��� �߰� */
    static IDE_RC insertNodeBefore(idvSQL          *aStatistics,
                                   sdpDblRIDListBase*  aBaseNode,
                                   sdpDblRIDListNode*  aNode,
                                   sdpDblRIDListNode*  aNewNode,
                                   sdrMtx*          aMtx);

    /* RID list���� Ư����� ���� */
    static IDE_RC removeNode(idvSQL             * aStatistics,
                             sdpDblRIDListBase  * aBaseNode,
                             sdpDblRIDListNode  * aNode,
                             sdrMtx             * aMtx);

    /* from ������ tail���� �ѹ��� ���� */
    static IDE_RC removeNodesAtOnce(idvSQL               *aStatistics,
                                    sdpDblRIDListBase    *aBaseNode,
                                    sdpDblRIDListNode    *aFromNode,
                                    sdpDblRIDListNode    *aToNode,
                                    ULong                 aNodeCount,
                                    sdrMtx               *aMtx);

    /* rid list�� tail�� �߰� */
    static IDE_RC insertNodesAtOnce(idvSQL            *aStatistics,
                                    sdpDblRIDListBase *aBaseNode,
                                    sdpDblRIDListNode *aFromNode,
                                    sdpDblRIDListNode *aToNode,
                                    ULong             aNodeCount,
                                    sdrMtx            *aMtx);

    /* base ����� head ��� ��� */
    static inline sdRID getBaseNodeRID(sdpDblRIDListBase*   aBaseNode);

    /* base ����� length ��� */
    static inline ULong getNodeCnt(sdpDblRIDListBase*   aBaseNode);

    /* base ����� head ��� ��� */
    static inline sdRID getHeadOfList(sdpDblRIDListBase*   aBaseNode);

    /* base ����� tail ��� ��� */
    static inline sdRID getTailOfList(sdpDblRIDListBase*   aBaseNode);

    /* ����� next ��� ��� */
    static inline sdRID getNxtOfNode(sdpDblRIDListNode*   aNode);

    /* ����� prev ��� ��� */
    static inline sdRID getPrvOfNode(sdpDblRIDListNode*   aNode);

    /* ����Ʈ�� ��� ��� ��� */
    static IDE_RC dumpList( scSpaceID aSpaceID,
                            sdRID     aBaseNodeRID );

private:

    // rid�� ������ page�� �����ϴ��� �˻�
    static inline idBool isSamePage(sdRID*     aLhs,
                                    sdRID*     aRhs);

    /* base ����� length ���� �� logging */
    static IDE_RC  setNodeCnt(sdpDblRIDListBase*  aBaseNode,
                              ULong               aNodeCnt,
                              sdrMtx*             aMtx);

    /* ����� next ��� ���� �� logging */
    static IDE_RC  setNxtOfNode(sdpDblRIDListNode*  aNode,
                                sdRID            aNextRID,
                                sdrMtx*          aMtx);

    /* ����� prev ��� ���� �� logging */
    static IDE_RC  setPrvOfNode(sdpDblRIDListNode*   aNode,
                                sdRID             aPrevRID,
                                sdrMtx*           aMtx);
};

/***********************************************************************
 * Description : base->mBase ptr�� rid�� ��ȯ�Ͽ� ��ȯ
 ***********************************************************************/
inline sdRID sdpDblRIDList::getBaseNodeRID(sdpDblRIDListBase*  aBaseNode)
{

    IDE_DASSERT( aBaseNode != NULL );

    return sdpPhyPage::getRIDFromPtr((UChar*)&aBaseNode->mBase);

}

/***********************************************************************
 * Description : ������ Page���� rid���� �˻�
 ***********************************************************************/
idBool sdpDblRIDList::isSamePage(sdRID* aLhs, sdRID* aRhs)
{

    IDE_DASSERT( aLhs != NULL );
    IDE_DASSERT( aRhs != NULL );

    return ((SD_MAKE_PID(*aLhs) == SD_MAKE_PID(*aRhs))) ? ID_TRUE : ID_FALSE;

}

/***********************************************************************
 * Description : Base ����� length ��ȯ
 ***********************************************************************/
inline ULong sdpDblRIDList::getNodeCnt(sdpDblRIDListBase*   aBaseNode)
{
    IDE_DASSERT( aBaseNode != NULL );

    return aBaseNode->mNodeCnt;
}

/***********************************************************************
 * Description : ����� next ��� ��ȯ Ȥ�� base ����� tail ��� ��ȯ
 ***********************************************************************/
inline sdRID sdpDblRIDList::getNxtOfNode(sdpDblRIDListNode*  aNode)
{

    IDE_DASSERT( aNode != NULL );

    return aNode->mNext;

}

/***********************************************************************
 * Description : ����� prev ��� ��ȯ Ȥ�� base ����� head ��� ��ȯ
 ***********************************************************************/
inline sdRID sdpDblRIDList::getPrvOfNode(sdpDblRIDListNode*   aNode)
{

    IDE_DASSERT( aNode != NULL );

    return aNode->mPrev;

}


/***********************************************************************
 * Description : base ����� head ��� ��ȯ
 ***********************************************************************/
inline sdRID sdpDblRIDList::getHeadOfList(sdpDblRIDListBase*   aBaseNode)
{

    IDE_DASSERT( aBaseNode != NULL );

    return getNxtOfNode(&aBaseNode->mBase);

}

/***********************************************************************
 * Description : base ����� tail ��� ��ȯ
 ***********************************************************************/
inline sdRID sdpDblRIDList::getTailOfList(sdpDblRIDListBase*   aBaseNode)
{

    IDE_DASSERT( aBaseNode != NULL );

    return getPrvOfNode(&aBaseNode->mBase);

}

#endif // _O_SDP_DBL_RIDLIST_H_
