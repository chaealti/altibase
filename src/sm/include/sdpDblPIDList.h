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
 * $Id: sdpDblPIDList.h 24150 2007-11-14 06:36:46Z bskim $
 *
 * Description :
 *
 * File Based PID Linked-list ������
 *
 *
 * # ����
 *
 * ���� tablespace�� ������ page list�� �����ϱ� ����
 * �������̽�
 *
 * # ����
 *  
 *  + tablespace�� ����Ǵ� ���� ������ page list ����
 * 
 *      - tablespace�� seg.dir full list (base)
 *      - tablespace�� seg.dir free list (base)
 *      - tablespace�� ext.dir full list (base)
 *      - segment dir�� seg.dir list     (node)
 *      - segment desc�� used page list  (base)
 *      - extent  dir�� ext.dir list     (node)
 *      - persistent page�� page list    (node)
 *
 *
 * # ����
 *              sdpPageListBase
 *                  ______________
 *                  |____len_____|        
 *          ________|tail_|_head_|________
 *         |    ____/        \_____      |
 *         |  _/_________   _______\___  |
 *         |  |next|prev|<--|next|prev|  |
 *         -->|____|____|-->|____|____|<--
 *
 *         sdpPageListNode
 *
 * PID ����Ʈ�� ������ tablespace������ ���� ������ �����ϸ�, ���Ͱ���
 * circular double linked-list�� ������ �����Ѵ�. ����Ʈ�� ���� ���濬����
 * base ��带 fix�� ���¿��� �̷�������Ѵ�.
 * ������ page���� ����Ʈ�� ���� ��尡 ������ �� ����(ASSERT ó��)
 *
 * # Releate data structure
 *
 * sdpDblPIDListBase ����ü
 * sdpDblPIDListNode ����ü
 *
 **********************************************************************/

#ifndef _O_SDP_DBL_PIDLIST_H_
#define _O_SDP_DBL_PIDLIST_H_ 1

#include <sdr.h>
#include <sdpDef.h>

class sdpDblPIDList
{
public:

    /* page list�� base ��带 �ʱ�ȭ */
    static IDE_RC initBaseNode(sdpDblPIDListBase*    aBaseNode,  
                               sdrMtx*               aMtx);
    
    /* page list�� head�� ��带 �߰� */
    static IDE_RC insertHeadNode(idvSQL           *aStatistics,
                                 sdpDblPIDListBase*   aBaseNode, 
                                 sdpDblPIDListNode*   aNewNode,
                                 sdrMtx*           aMtx);

    /* page list�� tail�� ��带 �߰� */
    static IDE_RC insertTailNode(idvSQL           *aStatistics,
                                 sdpDblPIDListBase*   aBaseNode, 
                                 sdpDblPIDListNode*   aNewNode,
                                 sdrMtx*           aMtx);

    /* page list�� tail�� list�� �߰� */
    static IDE_RC insertTailList(idvSQL              *aStatistics,
                                 sdpDblPIDListBase   *aBaseNode, 
                                 sdpDblPIDListNode   *aFstNode,
                                 sdpDblPIDListNode   *aLstNode,
                                 ULong                aListLen,
                                 sdrMtx*              aMtx);
    
    /* page list�� Ư�� ��� �ڿ� ���ο� ��� �߰� */
    static IDE_RC insertNodeAfter(idvSQL*           aStatistics,
                                  sdpDblPIDListBase*   aBaseNode,
                                  sdpDblPIDListNode*   aNode,
                                  sdpDblPIDListNode*   aNewNode,
                                  sdrMtx*           aMtx);
    
    /* page list�� Ư�� ��� �տ� ���ο� ��� �߰� */
    static IDE_RC insertNodeBefore(idvSQL*           aStatistics,
                                   sdpDblPIDListBase*   aBaseNode,
                                   sdpDblPIDListNode*   aNode,
                                   sdpDblPIDListNode*   aNewNode,
                                   sdrMtx*           aMtx);

    static IDE_RC insertNodeBeforeWithNoFix(idvSQL*          aStatistics,
                                            sdpDblPIDListBase*  aBaseNode,
                                            sdpDblPIDListNode*  aNode,
                                            sdpDblPIDListNode*  aNewNode,
                                            sdrMtx*          aMtx);
    
    static IDE_RC insertNodeBeforeLow(idvSQL *aStatistics,
                                      sdpDblPIDListBase*  aBaseNode,
                                      sdpDblPIDListNode*  aNode,
                                      sdpDblPIDListNode*  aNewNode,
                                      sdrMtx*          aMtx,
                                      idBool           aNeedGetPage);
    
    /* page list���� Ư����� ���� */
    static IDE_RC removeNode(idvSQL                *aStatistics,
                             sdpDblPIDListBase*     aBaseNode,
                             sdpDblPIDListNode*     aNode,
                             sdrMtx*                aMtx);
    
    static IDE_RC removeNodeLow(idvSQL          *aStatistics,
                                sdpDblPIDListBase*  aBaseNode,
                                sdpDblPIDListNode*  aNode,
                                sdrMtx*          aMtx,
                                idBool           aCheckMtxStack);
    
    static IDE_RC removeNodeWithNoFix(idvSQL          *aStatistics,
                                      sdpDblPIDListBase*  aBaseNode,
                                      sdpDblPIDListNode*  aNode,
                                      sdrMtx*          aMtx);
    
    static IDE_RC dumpCheck( sdpDblPIDListBase*  aBaseNode,
                             scSpaceID           aSpaceID,
                             scPageID            aPageID );

    /* base ����� length ��� */
    static inline ULong getNodeCnt(sdpDblPIDListBase*   aBaseNode);
    
    /* base ����� head ��� ��� */
    static inline scPageID getListHeadNode(sdpDblPIDListBase*   aBaseNode);
    
    /* base ����� tail ��� ��� */
    static inline scPageID getListTailNode(sdpDblPIDListBase*   aBaseNode);
    
    /* ����� next ��� ��� */
    static inline scPageID getNxtOfNode(sdpDblPIDListNode*   aNode);
    
    /* ����� prev ��� ��� */
    static inline scPageID getPrvOfNode(sdpDblPIDListNode*   aNode);
    
    /* list�� ��� ��� ��� */
    static IDE_RC dumpList( scSpaceID aSpaceID,
                            sdRID     aBaseNodeRID );
    
    /* ����� Ư�� prev page ID ���� �� logging */
    static IDE_RC setPrvOfNode(sdpDblPIDListNode*  aNode,
                               scPageID            aPageID,
                               sdrMtx*             aMtx);
    
    /* ����� Ư�� next page ID ���� �� logging */
    static IDE_RC setNxtOfNode(sdpDblPIDListNode*  aNode,
                               scPageID            aPageID,
                               sdrMtx*             aMtx);
    
    /* base ����� length ���� �� logging */
    static IDE_RC setNodeCnt(sdpDblPIDListBase  *aBaseNode,
                             ULong               aLen,
                             sdrMtx             *aMtx);
};


/***********************************************************************
 * Description : Base ����� length ��ȯ
 ***********************************************************************/
inline ULong sdpDblPIDList::getNodeCnt(sdpDblPIDListBase*   aBaseNode)
{
    IDE_DASSERT( aBaseNode != NULL );

    return aBaseNode->mNodeCnt;
}

/***********************************************************************
 * Description : ����� next ��� ��ȯ Ȥ�� base ����� tail ��� ��ȯ
 ***********************************************************************/
inline scPageID sdpDblPIDList::getNxtOfNode(sdpDblPIDListNode*  aNode)
{

    IDE_DASSERT( aNode != NULL );

    return aNode->mNext;
    
}

/***********************************************************************
 * Description : ����� prev ��� ��ȯ Ȥ�� base ����� head ��� ��ȯ
 ***********************************************************************/
inline scPageID sdpDblPIDList::getPrvOfNode(sdpDblPIDListNode* aNode)
{

    IDE_DASSERT( aNode != NULL );

    return aNode->mPrev;
    
}

/***********************************************************************
 * Description : base ����� head ��� ��ȯ
 ***********************************************************************/
inline scPageID sdpDblPIDList::getListHeadNode(sdpDblPIDListBase* aBaseNode)
{

    IDE_DASSERT( aBaseNode != NULL );

    return getNxtOfNode(&aBaseNode->mBase);
    
}
    
/***********************************************************************
 * Description : base ����� tail ��� ��ȯ
 ***********************************************************************/
inline scPageID sdpDblPIDList::getListTailNode(sdpDblPIDListBase* aBaseNode)
{
    
    IDE_DASSERT( aBaseNode != NULL );

    return getPrvOfNode(&aBaseNode->mBase);
    
}

#endif // _O_SDP_DBL_PIDLIST_H_

