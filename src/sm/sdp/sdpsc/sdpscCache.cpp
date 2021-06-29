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
 *
 * $Id: sdpscCache.cpp 82075 2018-01-17 06:39:52Z jina.kim $
 *
 * �� ������ Circular-List Managed Segment�� Segment Runtime Cache�� ����
 * STATIC �������̽��� �����Ѵ�.
 *
 ***********************************************************************/

# include <sdpDef.h>

# include <sdpscCache.h>
# include <sdpscSH.h>

# include <sdpSegment.h>


/***********************************************************************
 *
 * Description : [ INTERFACE ] Segment Cache �ʱ�ȭ
 *
 * aSegHandle  - [IN] ���׸�Ʈ �ڵ� ������
 * aSpaceID    - [IN] ���̺����̽� ID
 * aSegType    - [IN] ���׸�Ʈ Ÿ��
 * aTableOID   - [IN] ���׸�Ʈ�� ���� ��ü�� OID (��ü�� Table�� ��츸)
 *
 ***********************************************************************/
IDE_RC sdpscCache::initialize( sdpSegHandle * aSegHandle,
                               scSpaceID      aSpaceID,
                               sdpSegType     aSegType,
                               smOID          aTableOID )
{
    sdpSegInfo          sSegInfo;
    sdpscSegCache     * sSegCache;
    UInt                sState  = 0;

    IDE_ASSERT( aSegHandle != NULL );
    IDE_ASSERT( aTableOID  == SM_NULL_OID );

    //TC/Server/LimitEnv/Bugs/BUG-24163/BUG-24163_2.sql
    //IDU_LIMITPOINT("sdpscCache::initialize::malloc");
    //[TODO] immediate�� �����Ұ�.

    IDE_ASSERT( iduMemMgr::malloc( IDU_MEM_SM_SDP,
                                   ID_SIZEOF(sdpscSegCache),
                                   (void **)&(sSegCache),
                                   IDU_MEM_FORCE )
                == IDE_SUCCESS );
    sState  = 1;

    sdpSegment::initCommonCache( &sSegCache->mCommon,
                                 aSegType,
                                 0,  /* aPageCntInExt */
                                 aTableOID,
                                 SM_NULL_INDEX_ID);

    if ( aSegHandle->mSegPID != SD_NULL_PID )
    {
        IDE_TEST( sdpscSegHdr::getSegInfo( NULL, /* aStatistics */
                                           aSpaceID,
                                           aSegHandle->mSegPID,
                                           NULL, /* aTableHeader */
                                           &sSegInfo )
                  != IDE_SUCCESS );

        IDE_ASSERT( sSegInfo.mType == aSegType );

        sSegCache->mCommon.mPageCntInExt   = sSegInfo.mPageCntInExt;
        sSegCache->mCommon.mSegSizeByBytes =
            ((sSegInfo.mExtCnt * sSegInfo.mPageCntInExt )
             * SD_PAGE_SIZE);
    }

    aSegHandle->mCache       = (void*)sSegCache;
    aSegHandle->mSpaceID     = aSpaceID;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if( sState != 0 )
    {
        IDE_ASSERT( iduMemMgr::free( sSegCache ) == IDE_SUCCESS );
    }

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : SegCacheInfo�� ����ش�.
 *
 * aStatistics   - [IN] ��� ����
 * aSegHandle    - [IN] Segment Handle
 *
 * aSegCacheInfo - [OUT] SegCacheInfo�� �����ȴ�.
 *
 ***********************************************************************/
IDE_RC sdpscCache::getSegCacheInfo( idvSQL          * /*aStatistics*/,
                                    sdpSegHandle    * /*aSegHandle*/,
                                    sdpSegCacheInfo * aSegCacheInfo )
{
    IDE_ASSERT( aSegCacheInfo != NULL );

    aSegCacheInfo->mUseLstAllocPageHint = ID_FALSE;
    aSegCacheInfo->mLstAllocPID         = SD_NULL_PID;
    aSegCacheInfo->mLstAllocSeqNo       = 0;

    return IDE_SUCCESS;
}

IDE_RC sdpscCache::setLstAllocPage( idvSQL          * /*aStatistics*/,
                                    sdpSegHandle    * /*aSegHandle*/,
                                    scPageID          /*aLstAllocPID*/,
                                    ULong             /*aLstAllocSeqNo*/ )
{
    /* TMS�� �ƴϸ� �ƹ��͵� ���� �ʴ´�. */
    return IDE_SUCCESS;
}

/***********************************************************************
 *
 * Description : [ INTERFACE ] Segment Cache ����
 *
 * aSegHandle  - [IN] ���׸�Ʈ �ڵ� ������
 *
 ***********************************************************************/
IDE_RC sdpscCache::destroy( sdpSegHandle * aSegHandle )
{
    IDE_ASSERT( aSegHandle != NULL );

    // Segment Cache�� �޸� �����Ѵ�.
    IDE_ASSERT( iduMemMgr::free( aSegHandle->mCache ) == IDE_SUCCESS );
    aSegHandle->mCache = NULL;

    return IDE_SUCCESS;
}
