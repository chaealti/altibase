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
 * $Id: smxTouchPageList.cpp 88191 2020-07-27 03:08:54Z mason.lee $
 **********************************************************************/

# include <smxTrans.h>
# include <smxTouchPageList.h>

# include <sdnIndexCTL.h>
# include <sdcTableCTL.h>

iduMemPool  smxTouchPageList::mMemPool;
UInt        smxTouchPageList::mTouchNodeSize;
UInt        smxTouchPageList::mTouchPageCntByNode;


/***********************************************************************
 *
 * Description : Ʈ����ǵ鰣�� �����ڷᱸ�� �ʱ�ȭ
 *
 * Ʈ����� ������ �ʱ�ȭ�� PreProcess �ܰ迡�� �ʱ�ȭ�Ǹ�, �̶� Touch Page List��
 * MemPool �� �ؽ����̺��� �ʱ�ȭ�Ѵ�. ����, ����ũ�⸦ ����Ͽ� Touch Page List
 * ���� Ʈ����Ǵ� �ִ� Cache�� ������ ������ ���Ѵ�.
 *
 ***********************************************************************/
IDE_RC smxTouchPageList::initializeStatic()
{
    UInt sPowerOfTwo;

    mTouchPageCntByNode = smuProperty::getTransTouchPageCntByNode();

    mTouchNodeSize = ( ID_SIZEOF( smxTouchInfo ) * ( mTouchPageCntByNode - 1 ) ) +
                     ID_SIZEOF( smxTouchNode );

    /* BUG-46434
       mempool���� IDU_MEMPOOL_TYPE_TIGHT�� ����ϱ� ���ؼ���
       size, align ���� 2^n ������ �־���Ѵ�. 
       2^n ���� �ǵ��� mTouchNodeSize�� �����Ѵ�. */

    /* 1. mTouchNodeSize ���� ���ų� ū 2^n �� ���ϱ� */
    sPowerOfTwo = smuUtility::getPowerofTwo( mTouchNodeSize );

    /* 2. smxTouchInfo�� �� �߰��Ҽ� �ִ��� ��� */
    mTouchPageCntByNode += ( ( sPowerOfTwo - mTouchNodeSize ) / ID_SIZEOF( smxTouchInfo ) );

    /* 3. mTouchNodeSize ���� �����Ѵ�. */
    mTouchNodeSize = sPowerOfTwo;

    IDE_TEST( mMemPool.initialize(
                  IDU_MEM_SM_TRANSACTION_DISKPAGE_TOUCHED_LIST,
                  (SChar*)"TRANSACTION_DISKPAGE_TOUCH_LIST",
                  ID_SCALABILITY_SYS, /* List Count */
                  mTouchNodeSize,
                  1024,    /* itemcount per a chunk */
                  1,       /* xxxx */
                  ID_TRUE, /* Use Mutex */
                  mTouchNodeSize,	/* AlignByte */
                  ID_FALSE,			/* ForcePooling */
                  ID_TRUE,			/* GarbageCollection */
                  ID_TRUE,          /* HWCacheLine */
                  IDU_MEMPOOL_TYPE_TIGHT  /* mempool type */ ) 
              != IDE_SUCCESS );			

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

/***********************************************************************
 *
 * Description : Ʈ����ǵ鰣�� ������ �ڷᱸ�� ����
 *
 * MemPool �� �ؽ����̺��� �����Ѵ�.
 *
 ***********************************************************************/
IDE_RC smxTouchPageList::destroyStatic()
{
    IDE_ASSERT( mMemPool.destroy() == IDE_SUCCESS );

    return IDE_SUCCESS;
}


/***********************************************************************
 *
 * Description : Ʈ������� Touch Page ����Ʈ ��ü �ʱ�ȭ
 *
 * ����� ����Ʈ�� NodeList�� �ʱ�ȭ �� Hash �ʱ�ȭ�ϸ�, Hash �ڷᱸ���� �ߺ�
 * ������ ������ �����ϱ� ���ؼ� ����Ѵ�.
 *
 * aTrans - [IN] Owner Ʈ����� ������
 *
 ***********************************************************************/
IDE_RC smxTouchPageList::initialize( smxTrans * aTrans )
{
    mTrans      = aTrans;
    mItemCnt    = 0;
    mNodeList   = NULL;

    return IDE_SUCCESS;
}

/***********************************************************************
 *
 * Description : Ʈ������� Touch Page List ����
 *
 ***********************************************************************/
IDE_RC smxTouchPageList::destroy()
{
    return IDE_SUCCESS;
}

/***********************************************************************
 *
 * Description : Ʈ������� Touch Page List ����
 *
 * Ʈ����� �Ϸ�ÿ� Ʈ������� Touch Page List�� ��� �޸� �����Ѵ�.
 *
 * (TASK-6950)
 * smxTouchPageList::runFastStamping() ���� �Ҵ�� NODE�� ��� free������,
 * Ȥ�ó� free �ȵɰ�츦 ����� NODE free�ϴ� �ڵ带 ���ܵд�.
 *
 ***********************************************************************/
IDE_RC smxTouchPageList::reset()
{
    smxTouchNode *sCurNode;
    smxTouchNode *sNxtNode = NULL;

    sCurNode = mNodeList;

    while ( sNxtNode != mNodeList )
    {
        sNxtNode = sCurNode->mNxtNode;
        IDE_TEST( mMemPool.memfree( (void *)sCurNode ) != IDE_SUCCESS );
        sCurNode = sNxtNode;
    }

    mNodeList = NULL;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 *
 * Description : Ʈ������� Touch Page List�� ��� �޸� �Ҵ�
 *
 * Ʈ������� ������ ����Ÿ ������ ������ �ϳ��� Touch Page ��帶��
 * TRANSACTION_TOUCH_PAGE_COUNT_BY_NODE_ ������Ƽ��ŭ�� ������ �� ������
 * OverFlow�� �߻��ϸ� ���ο� Touch Page ��带 �޸��Ҵ��Ͽ� Touch Page List
 * �� �����Ѵ�.
 *
 ***********************************************************************/
IDE_RC smxTouchPageList::alloc()
{
    smxTouchNode *sNewNode;

    /* smxTouchPageList_alloc_alloc_NewNode.tc */
    IDU_FIT_POINT("smxTouchPageList::alloc::alloc::NewNode");
    IDE_TEST( mMemPool.alloc( (void**)&sNewNode ) != IDE_SUCCESS );

    initNode( sNewNode );

    if ( mNodeList != NULL )
    {
        sNewNode->mNxtNode = mNodeList->mNxtNode;

        mNodeList->mNxtNode = sNewNode;
    }
    else
    {
        /* nothing to do */
    }

    mNodeList = sNewNode;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

/***********************************************************************
 *
 * Description : Ʈ������� Touch Page List�� ��� ���� (TASK-6950)
 *
 * Touch Page List�� ��ϵ� ������ ���� ������
 * �ִ�ġ(mMaxCachePageCnt)�� �����ϸ�
 * ���̻� ��带 �Ҵ����� �ʰ� ���� ������ ��带 �����Ѵ�.
 *
 ***********************************************************************/
void smxTouchPageList::reuse()
{
    IDE_DASSERT( mNodeList != NULL );

    mNodeList = mNodeList->mNxtNode;

    mItemCnt -= mNodeList->mPageCnt;

    mNodeList->mPageCnt = 0;

    return;
}

/***********************************************************************
 *
 * Description : ������ ���� �߰�
 *
 * Ʈ������� ������ ����Ÿ �������� Touch Page ��忡 �߰��Ѵ�.
 * ������ ����Ÿ ������ ������ SpaceID, PageID, CTS ������ ����ȴ�.
 *
 * aSpaceID   - [IN] ���̺����̽� ID
 * aPageID    - [IN] ����Ÿ�������� PID
 * aCTSlotIdx - [IN] ���ε��� CTS ��ȣ
 *
 ***********************************************************************/
IDE_RC smxTouchPageList::add( scSpaceID     aSpaceID,
                              scPageID      aPageID,
                              SShort        aCTSlotIdx )
{
    smxTouchNode * sCurNode;
    UInt           sItemCnt;

    IDE_ASSERT( aPageID != SC_NULL_PID );
    IDE_ASSERT( (aCTSlotIdx & SDP_CTS_IDX_MASK)
                != SDP_CTS_MAX_IDX );

    sCurNode = mNodeList;

    IDE_TEST_CONT( mMaxCachePageCnt == 0,
                   do_not_use_touch_page_list );

    if ( ( sCurNode == NULL ) ) 
    {
        IDE_TEST( alloc() != IDE_SUCCESS );
        sCurNode = mNodeList;
    }
    else
    {
        if ( sCurNode->mPageCnt >= mTouchPageCntByNode )
        {
            if ( mItemCnt < mMaxCachePageCnt )
            {
                IDE_TEST( alloc() != IDE_SUCCESS );
                sCurNode = mNodeList;
            }
            else
            {
                /* ����� page���� ���ѿ� �����ϸ�,
                   ���� ������ node�� �����Ѵ�. */
                reuse();
                sCurNode = mNodeList;
            }
        }
        else
        {
            /* nothing to do  */
        }
    }

    sItemCnt = sCurNode->mPageCnt;

    sCurNode->mArrTouchInfo[ sItemCnt ].mSpaceID    = aSpaceID;
    sCurNode->mArrTouchInfo[ sItemCnt ].mPageID     = aPageID;
    sCurNode->mArrTouchInfo[ sItemCnt ].mCTSlotIdx  = aCTSlotIdx;

    sCurNode->mPageCnt++;
    mItemCnt++;

    IDE_EXCEPTION_CONT( do_not_use_touch_page_list );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 *
 * Description : Ʈ����� Fast TimeStamping ����
 *
 * Ʈ����� Ŀ�԰������� ����Ǵ� Fast Stamping�� ������ �������� ���ۻ� ���ų�
 * �ٸ� Ʈ������̳� Flusher�� ���ؼ� X-Latch�� ȹ��Ǿ� �ִ� ��쿡�� �����Ѵ�.
 * ��, ���ۻ󿡼� Hit�Ǹ� X-Latch�� �ٷ� ȹ��� �� �ִ� �ε��� �������� ����Ÿ
 * �������� ���ؼ� �������� ���� �����Ѵ�.
 *
 *  aCommitSCN - [IN] Ʈ����� CommitSCN
 *
 ***********************************************************************/
IDE_RC smxTouchPageList::runFastStamping( smSCN * aCommitSCN )
{
    UInt            i;
    sdpPhyPageHdr * sPage     = NULL;
    UInt            sFixState = 0;
    smxTouchNode  * sCurNode;
    smxTouchInfo  * sTouchInfo;
    idBool          sSuccess;
    sdSID           sTransTSSlotSID;
    smSCN           sFstDskViewSCN;
    smxTouchNode  * sNxtNode = NULL;

    sTransTSSlotSID = smxTrans::getTSSlotSID((void*)mTrans);
    sFstDskViewSCN  = smxTrans::getFstDskViewSCN( (void*)mTrans );

    sCurNode = mNodeList;

    /* (TASK-6950) �Ʒ� loop ���ظ� �������� ����

       - mNodeList�� �Ҵ�� NODE�� ���ٸ�(=NULL), loop�� ���� �ʴ´�.
       - mNodeList�� NULL�� �ƴѰ��, sNxtNode�� �ʱⰪ�� NULL�̹Ƿ� �׻� loop�� ����. 
       - mNodeList�� �ϳ��� NODE�� �Ҵ�Ǿ� �ִٸ� �� NODE�� mNxtNode�� �ڱ��ڽ��� ����Ų��. */

    while ( sNxtNode != mNodeList )
    {
        for( i = 0; i < sCurNode->mPageCnt; i++ )
        {
            sPage = NULL;
            sTouchInfo = &(sCurNode->mArrTouchInfo[i]);

            IDE_TEST( sdbBufferMgr::fixPageWithoutIO(
                                         mStatistics,
                                         sTouchInfo->mSpaceID,
                                         sTouchInfo->mPageID,
                                         (UChar**)&sPage ) 
                      != IDE_SUCCESS );
            sFixState = 1;

            if( sPage == NULL )
            {
                continue;
            }

            sdbBufferMgr::latchPage( mStatistics,
                                     (UChar*)sPage,
                                     SDB_X_LATCH,
                                     SDB_WAIT_NO,
                                     &sSuccess );
            if( sSuccess == ID_FALSE )
            {
                sFixState = 0;
                IDE_TEST( sdbBufferMgr::unfixPage( mStatistics,
                                                   (UChar*)sPage )
                          != IDE_SUCCESS );
                continue;
            }

            sFixState = 2;

            if( sPage->mPageType == SDP_PAGE_DATA )
            {
                sdcTableCTL::runFastStamping( &sTransTSSlotSID,
                                              &sFstDskViewSCN,
                                              sPage,
                                              sTouchInfo->mCTSlotIdx,
                                              aCommitSCN );
            }
            else
            {
                /* BUG-29280 - non-auto commit D-Path Insert ������
                 *             rollback �߻��� ��� commit �� ���� �״� ����
                 *
                 * DATA �������� �ƴϸ� Index ���������� �Ѵ�. */
                IDE_ASSERT( (sPage->mPageType == SDP_PAGE_INDEX_BTREE) ||
                            (sPage->mPageType == SDP_PAGE_INDEX_RTREE) );

                sdnIndexCTL::fastStamping( mTrans,
                                           (UChar*)sPage,
                                           sTouchInfo->mCTSlotIdx,
                                           aCommitSCN );
            }

            sdbBufferMgr::setDirtyPageToBCB( mStatistics,
                                             (UChar*)sPage );

            sFixState = 1;
            sdbBufferMgr::unlatchPage( (UChar*)sPage );

            sFixState = 0;
            IDE_TEST( sdbBufferMgr::unfixPage( mStatistics,
                                               (UChar*)sPage )
                      != IDE_SUCCESS );
        }

        sNxtNode = sCurNode->mNxtNode;

        /* FAST STAMPING�� ������ NODE�� �����Ѵ�. (TASK-6950) */
        IDE_TEST( mMemPool.memfree( (void *)sCurNode ) != IDE_SUCCESS );

        sCurNode = sNxtNode;
    }

    mNodeList = NULL;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    switch( sFixState )
    {
        case 2:
            sdbBufferMgr::unlatchPage( (UChar*)sPage );
        case 1:
            IDE_ASSERT( sdbBufferMgr::unfixPage( mStatistics,
                                                 (UChar*)sPage )
                        == IDE_SUCCESS );
        default:
            break;
    }

    return IDE_FAILURE;
}
