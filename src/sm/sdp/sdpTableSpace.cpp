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
 * $Id: sdpTableSpace.cpp 86110 2019-09-02 04:52:04Z et16 $
 **********************************************************************/

#include <sdd.h>
#include <sdb.h>
#include <sct.h>
#include <sdpModule.h>
#include <sdpTableSpace.h>
#include <sdpReq.h>
#include <smErrorCode.h>
#include <sdptbExtent.h>

/*
 * ��� ��ũ ���̺����̽��� Space Cache �Ҵ� �� �ʱ�ȭ�Ѵ�.
 */
IDE_RC sdpTableSpace::initialize()
{
    // Space Cache�� �Ҵ��ϰ� �ʱ�ȭ�Ѵ�.
    IDE_TEST( sctTableSpaceMgr::doAction4EachTBS(
                  NULL, /* aStatistics */
                  doActAllocSpaceCache,
                  NULL, /* Action Argument*/
                  SCT_ACT_MODE_NONE )
              != IDE_SUCCESS );

    // BUG-24434
    // sdpPageType �� ������ �Ǹ� IDV_SM_PAGE_TYPE_MAX ���� Ȯ���� ��� �մϴ�.  
    IDE_ASSERT( IDV_SM_PAGE_TYPE_MAX == SDP_PAGE_TYPE_MAX );

    return IDE_SUCCESS ;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*
 * ��� ��ũ ���̺����̽��� Space Cache��
 * �޸� �����Ѵ�.
 */
IDE_RC sdpTableSpace::destroy()
{
    return sctTableSpaceMgr::doAction4EachTBS(
        NULL, /* aStatistics */
        doActFreeSpaceCache,
        NULL, /* Action Argument*/
        SCT_ACT_MODE_NONE );
}

/*
 * Tablespace ����
 */
IDE_RC sdpTableSpace::resetTBS( idvSQL           *aStatistics,
                                scSpaceID         aSpaceID,
                                void             *aTrans )
{
    sddTableSpaceNode   * sSpaceNode;

    IDE_TEST( sdptbSpaceDDL::resetTBSCore( aStatistics,
                                           aTrans,
                                           aSpaceID )
              != IDE_SUCCESS );

    IDE_ASSERT( sctTableSpaceMgr::findSpaceNodeBySpaceID( aSpaceID,
                                                          (void**)&sSpaceNode )

              == IDE_SUCCESS );

    if( sctTableSpaceMgr::isTempTableSpace( aSpaceID ) == ID_TRUE )
    {
        IDE_TEST( sdptbGroup::doRefineSpaceCacheCore( sSpaceNode ) != IDE_SUCCESS );
    }
    else
    {
        IDE_ERROR( sctTableSpaceMgr::isUndoTableSpace( aSpaceID ) == ID_TRUE );
    }

    return IDE_SUCCESS ;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/***********************************************************************
 * Description : TableSpace�� extent ���� ���� ����� �����Ѵ�.
 *
 *  aSpaceID - [IN] Ȯ���ϰ��� �ϴ� spage�� id
 **********************************************************************/
smiExtMgmtType sdpTableSpace::getExtMgmtType( scSpaceID   aSpaceID )
{
    sdpSpaceCacheCommon * sCacheHeader;
    smiExtMgmtType        sExtMgmtType;

    if ( sctTableSpaceMgr::hasState( aSpaceID, SCT_SS_INVALID_DISK_TBS )
         == ID_FALSE )
    {
        sCacheHeader =
            (sdpSpaceCacheCommon*) sddDiskMgr::getSpaceCache( aSpaceID );

        IDE_ASSERT( sCacheHeader != NULL );

        sExtMgmtType = sCacheHeader->mExtMgmtType;
    }
    else
    {
        sExtMgmtType = SMI_EXTENT_MGMT_NULL_TYPE;
    }

    return sExtMgmtType;
}


/*
 * Segment �������� ����� Tablespace �������� ����� ������.
 * ���� ���� �ٸ� ����� �߱��� ���� ������, ����ν��
 * ���� ������ �Ѱ������� ���ؼ� Tablespace �������������
 * �������� Segment �������� ��ĵ� ���������� ó���Ѵ�.
 */
smiSegMgmtType sdpTableSpace::getSegMgmtType( scSpaceID   aSpaceID )
{
    sdpSpaceCacheCommon * sCacheHeader;
    smiSegMgmtType        sSegMgmtType;

    if ( sctTableSpaceMgr::hasState( aSpaceID, SCT_SS_INVALID_DISK_TBS )
         == ID_FALSE )
    {
        if ( sctTableSpaceMgr::isUndoTableSpace( aSpaceID ) == ID_TRUE )
        {
            sSegMgmtType = SMI_SEGMENT_MGMT_CIRCULARLIST_TYPE;
        }
        else
        {
            IDE_DASSERT( sctTableSpaceMgr::isDiskTableSpace( aSpaceID ) == ID_TRUE );

            sCacheHeader = (sdpSpaceCacheCommon*) sddDiskMgr::getSpaceCache( aSpaceID );

            IDE_ASSERT( sCacheHeader != NULL );
            sSegMgmtType = sCacheHeader->mSegMgmtType;
        }
    }
    else
    {
        sSegMgmtType = SMI_SEGMENT_MGMT_NULL_TYPE;
    }

    return sSegMgmtType;
}

/***********************************************************************
 * Description : TableSpace�� extent �� page���� ��ȯ�Ѵ�.
 *
 *  aSpaceID - [IN] Ȯ���ϰ��� �ϴ� spage�� id
 **********************************************************************/
UInt sdpTableSpace::getPagesPerExt( scSpaceID     aSpaceID )
{
    sdpSpaceCacheCommon * sCacheHeader;
    UInt                  sPagesPerExt;

    if( sctTableSpaceMgr::hasState( aSpaceID, SCT_SS_INVALID_DISK_TBS ) == ID_FALSE )
    {
        sCacheHeader = (sdpSpaceCacheCommon*)sddDiskMgr::getSpaceCache( aSpaceID );
        IDE_ASSERT( sCacheHeader != NULL );

        sPagesPerExt = sCacheHeader->mPagesPerExt;
    }
    else
    {
        sPagesPerExt = 0;
    }

    return sPagesPerExt;
}



/*
 * ��ũ ���̺����̽��� Space Cache�� �Ҵ��ϰ� �ʱ�ȭ�Ѵ�.
 */
IDE_RC sdpTableSpace::doActAllocSpaceCache( idvSQL            * /*aStatistics*/,
                                            sctTableSpaceNode * aSpaceNode,
                                            void              * /*aActionArg*/ )
{
    sddTableSpaceNode   * sSpaceNode;

    IDE_ASSERT( aSpaceNode != NULL );

    if ( sctTableSpaceMgr::isDiskTableSpace( aSpaceNode ) == ID_TRUE )
    {
        sSpaceNode = (sddTableSpaceNode*)aSpaceNode;

        return sdptbGroup::allocAndInitSpaceCache( aSpaceNode->mID,
                                                   sSpaceNode->mExtMgmtType,
                                                   sSpaceNode->mSegMgmtType,
                                                   sSpaceNode->mExtPageCount );
    }

    return IDE_SUCCESS;
}

/*
 * ��ũ ���̺����̽��� Space Cache�� �����Ѵ�.
 */
IDE_RC sdpTableSpace::doActFreeSpaceCache( idvSQL            * /*aStatistics*/,
                                           sctTableSpaceNode * aSpaceNode,
                                           void              * /*aActionArg*/ )
{
    IDE_ASSERT( aSpaceNode != NULL );

    if ( sctTableSpaceMgr::isDiskTableSpace( aSpaceNode ) == ID_TRUE )
    {
        return sdptbGroup::destroySpaceCache( aSpaceNode );
    }

    return IDE_SUCCESS;
}

/*
 * ��ũ ���̺����̽��� Space Cache�� �����Ѵ�.
 *
 * BUG-29941 - SDP ��⿡ �޸� ������ �����մϴ�.
 * commit pending ���� ����� �� �Լ��� ȣ���Ͽ� tablespace�� �Ҵ��
 * Space Cache�� �����ϵ��� �Ѵ�.
 */
IDE_RC sdpTableSpace::freeSpaceCacheCommitPending(
                                           idvSQL            * /*aStatistics*/,
                                           sctTableSpaceNode * aSpaceNode,
                                           sctPendingOp      * /*aPendingOp*/ )
{
    return doActFreeSpaceCache( NULL /* idvSQL */,
                                aSpaceNode,
                                NULL /* ActionArg */ );
}

/* 
 * space cache �� ������ ���� ������ ��Ʈ�Ѵ�.
 * (��Ʈ���� ����� TBS���� ����ȴ�.)
 *  - GG�� extent �Ҵ簡�ɿ��κ�Ʈ
 *  - TBS�� ����ū GG id(��Ʈ�˻��û����)
 *  - extent�Ҵ����� GG ID��ȣ.(ó�� start�ÿ��� 0���� �ʱ�ȭ�Ѵ�.)
 */
IDE_RC sdpTableSpace::refineDRDBSpaceCache()
{
    return sctTableSpaceMgr::doAction4EachTBS(
        NULL, /* aStatistics */
        doRefineSpaceCache,
        NULL, /* Action Argument*/
        SCT_ACT_MODE_NONE );
}

/* TBS�� ���ؼ� Cache ������ Refine �Ѵ�.
 * �Լ� �����ͷ� �����ؾ� �ϹǷ� inline ȭ ���� �ʴ´�.*/
IDE_RC sdpTableSpace::doRefineSpaceCache( idvSQL            * /* aStatistics*/ ,
                                          sctTableSpaceNode * aSpaceNode,
                                          void              * /*aActionArg*/ )
{
    IDE_ASSERT( aSpaceNode != NULL );

    //temp tablespace�� ���� refine�� reset�ÿ� �ǽ��Ѵ�.
    if ( ( sctTableSpaceMgr::isDiskTableSpace( aSpaceNode ) == ID_TRUE ) &&
         ( sctTableSpaceMgr::isTempTableSpace( aSpaceNode ) == ID_FALSE ) )
    {
        return sdptbGroup::doRefineSpaceCacheCore( (sddTableSpaceNode*)aSpaceNode );
    }

    return IDE_SUCCESS;
}

/**********************************************************************
 * Description: tbs�� ������ �����ϰų� �߰��ϱ����� �� ũ�Ⱑ valid���� �˻���.
 *
 *     [ �˻���� ] (BUG-29566 ������ ������ ũ�⸦ 32G �� �ʰ��Ͽ� �����ص�
 *                            ������ ������� �ʽ��ϴ�.)
 *        1. autoextend on �� ��� init size < max size����
 *        2. max size, Init size �� 32G Ȥ�� OS Limit�� ���� �ʴ���
 *           Ȥ�� �ʹ� ������ ������..
 *
 * aDataFileAttr        - [IN] �������ϴ� ���ϵ鿡 ���� ����
 * aDataFileAttrCount   - [IN] ���ϰ���
 * aValidSmallSize      - [IN] ������ �ּ� ũ��˻翡 ���� ��
 **********************************************************************/
IDE_RC sdpTableSpace::checkPureFileSize( smiDataFileAttr ** aDataFileAttr,
                                         UInt               aDataFileAttrCount,
                                         UInt               aValidSmallSize )
{
    UInt    i;
    UInt    sInitPageCnt;
    UInt    sMaxPageCnt;

    for( i=0; i < aDataFileAttrCount ; i++ )
    {
        sInitPageCnt = aDataFileAttr[i]->mInitSize;
        sMaxPageCnt  = aDataFileAttr[i]->mMaxSize;

        /*
         * BUG-26294 [SD] tbs������ maxsize�� initsize���� �������� 
         *                �ý��ۿ� ���� �����ϴ� ��찡 ����. 
         */
        // BUG-26632 [SD] Tablespace������ maxsize�� unlimited ��
        //           �����ϸ� �������� �ʽ��ϴ�.
        // Unlimited�̸� MaxSize�� 0���� �����˴ϴ�.
        // MaxSize�� 0�� ��� Init Size�� ������ �ʽ��ϴ�.
        // BUG-29566 ������ ������ ũ�⸦ 32G �� �ʰ��Ͽ� �����ص�
        //           ������ ������� �ʽ��ϴ�.
        // 1. Init Size < Max Size
        // 2. Max Size, Init Size�� ������ ������..
        if(( aDataFileAttr[i]->mIsAutoExtend == ID_TRUE ) &&
           ( sMaxPageCnt != 0 ))
        {
            IDE_TEST_RAISE( sInitPageCnt > sMaxPageCnt ,
                            error_initsize_exceed_maxsize );
        }

        if( sddDiskMgr::getMaxDataFileSize() < SD_MAX_FPID_COUNT )
        {
            // OS file limit size
            IDE_TEST_RAISE( sMaxPageCnt > sddDiskMgr::getMaxDataFileSize(),
                            error_maxsize_exceed_oslimit );
        }
        else
        {
            // 32G Maximum file size
            IDE_TEST_RAISE( sMaxPageCnt > SD_MAX_FPID_COUNT,
                            error_maxsize_exceed_maxfilesize );
        }

        if( sddDiskMgr::getMaxDataFileSize() < SD_MAX_FPID_COUNT )
        {
            IDE_TEST_RAISE( sInitPageCnt > sddDiskMgr::getMaxDataFileSize(),
                            error_initsize_exceed_oslimit );
        }
        else
        {
            IDE_TEST_RAISE( sInitPageCnt > SD_MAX_FPID_COUNT,
                            error_initsize_exceed_maxfilesize );
        }

        /*
         * BUG-20972
         * FELT������ �ϳ��� extentũ�⺸�� ����ũ�Ⱑ ������ ������ ó����
         */
        IDE_TEST_RAISE( sInitPageCnt < aValidSmallSize ,
                        error_data_file_is_too_small );
    }

    return IDE_SUCCESS ;

    IDE_EXCEPTION( error_data_file_is_too_small )
    {
        IDE_SET( ideSetErrorCode( smERR_ABORT_FILE_IS_TOO_SMALL,
                                  (ULong)sInitPageCnt,
                                  (ULong)aValidSmallSize ));
    }
    IDE_EXCEPTION( error_initsize_exceed_maxsize )
    {
        IDE_SET( ideSetErrorCode( smERR_ABORT_InitSizeExceedMaxSize ));
    }
    IDE_EXCEPTION( error_initsize_exceed_maxfilesize )
    {
        IDE_SET( ideSetErrorCode( smERR_ABORT_InitExceedMaxFileSize,
                                  (ULong)sInitPageCnt,
                                  (ULong)SD_MAX_FPID_COUNT) );
    }
    IDE_EXCEPTION( error_maxsize_exceed_maxfilesize )
    {
        IDE_SET( ideSetErrorCode( smERR_ABORT_MaxExceedMaxFileSize,
                                  (ULong)sMaxPageCnt,
                                  (ULong)SD_MAX_FPID_COUNT ) );
    }
    IDE_EXCEPTION( error_initsize_exceed_oslimit )
    {
        IDE_SET(ideSetErrorCode( smERR_ABORT_InitSizeExceedOSLimit ,
                                 (ULong)sInitPageCnt,
                                 (ULong)sddDiskMgr::getMaxDataFileSize() ));
    }
    IDE_EXCEPTION( error_maxsize_exceed_oslimit )
    {
        IDE_SET(ideSetErrorCode( smERR_ABORT_MaxSizeExceedOSLimit,
                                 (ULong)sMaxPageCnt,
                                 (ULong)sddDiskMgr::getMaxDataFileSize() ));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

