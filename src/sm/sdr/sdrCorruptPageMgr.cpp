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
 * $Id:$
 *
 * Description :
 *
 * DRDB �������������� Corrupted page ó���� ���� ���������̴�.
 *
 **********************************************************************/

#include <sdp.h>
#include <smErrorCode.h>
#include <smDef.h>
#include <sdrCorruptPageMgr.h>
#include <sct.h>
#include <sdptbExtent.h>

// Redo �ÿ� Direct-Path INSERT ���� �� �߻���
// Partial Write Page�� Skip �ϱ� ���Ͽ� �ʿ��� �ڷ� ����
smuHashBase sdrCorruptPageMgr::mCorruptedPages;
UInt        sdrCorruptPageMgr::mHashTableSize;
sdbCorruptPageReadPolicy sdrCorruptPageMgr::mCorruptPageReadPolicy;

/***********************************************************************
 * Description : Corrupt page �������� �ʱ�ȭ �Լ��̴�.
 *               addCorruptedPage�� redoAll���� �߿� ����ǹǷ�
 *               �� ������ �ʱ�ȭ �Ǿ�� �Ѵ�.
 *
 *  aHashTableSize - [IN] CorruptedPagesHash�� bucket count
 **********************************************************************/
IDE_RC sdrCorruptPageMgr::initialize( UInt  aHashTableSize )
{
    IDE_DASSERT(aHashTableSize >  0);

    mHashTableSize = aHashTableSize;

    // Corrupted Page���� ������ Hash Table
    // HashKey : <space, pageID, unused>
    IDE_TEST( smuHash::initialize( &mCorruptedPages,
                                   1,                 // ConcurrentLevel
                                   mHashTableSize,    // aBucketCount
                                   ID_SIZEOF(scGRID), // aKeyLength
                                   ID_FALSE,          // aUseLatch
                                   genHashValueFunc,
                                   compareFunc ) != IDE_SUCCESS );

    // BUG-45598: ������Ƽ�� ���� Corrupt Page ó�� ��å�� ����
    setPageErrPolicyByProp();
 
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/***********************************************************************
 * Description : Corrupt page �������� ���� �Լ��̴�.
 **********************************************************************/
IDE_RC sdrCorruptPageMgr::destroy()
{
    IDE_TEST( smuHash::destroy(&mCorruptedPages) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/***********************************************************************
 * Description : Corrupted pages hash�� corrupted page������ �����Ѵ�.
 *
 *  aSpaceID - [IN] tablespace ID
 *  aPageID  - [IN] corrupted page ID
 ***********************************************************************/
IDE_RC sdrCorruptPageMgr::addCorruptPage( scSpaceID aSpaceID,
                                          scPageID  aPageID )
{
    scGRID                  sLogGRID;
    sdrCorruptPageHashNode* sNode;
    UInt                    sState = 0;
    smiTableSpaceAttr       sTBSAttr;

    SC_MAKE_GRID( sLogGRID, aSpaceID, aPageID, 0 );

    IDE_TEST( smuHash::findNode( &mCorruptedPages,
                                 &sLogGRID,
                                 (void**)&sNode )
              != IDE_SUCCESS );

    if ( sNode == NULL )
    {
        //------------------------------------------
        // corrupted page�� ��ϵ��� ���� page�̸�
        // �̸� mCorruptedPages�� ���
        //------------------------------------------

        /* sdrCorruptPageMgr_addCorruptPage_malloc_Node.tc */
        IDU_FIT_POINT("sdrCorruptPageMgr::addCorruptPage::malloc::Node");
        IDE_TEST( iduMemMgr::malloc( IDU_MEM_SM_SDR,
                                     ID_SIZEOF( sdrCorruptPageHashNode ),
                                     (void**)&sNode )
                  != IDE_SUCCESS );

        sState = 1;

        sNode->mSpaceID = aSpaceID;
        sNode->mPageID  = aPageID;

        sctTableSpaceMgr::getTBSAttrByID( NULL,
                                          aSpaceID,
                                          &sTBSAttr );
        ideLog::log( SM_TRC_LOG_LEVEL_WARNNING,
                     SM_TRC_DRECOVER_ADD_CORRUPTED_PAGE,
                     sTBSAttr.mName,
                     aPageID );

        IDE_TEST( smuHash::insertNode( &mCorruptedPages,
                                       &sLogGRID,
                                       sNode )
                  != IDE_SUCCESS );
    }
    else
    {
        // �̹� corruptedPages�� ��ϵǾ� �ִ�.
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if (sState != 0)
    {
        IDE_ASSERT( iduMemMgr::free(sNode) == IDE_SUCCESS );
    }

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : Corrupted pages hash�� corrupted page�� �ִٸ� �����Ѵ�.
 *
 *  aSpaceID - [IN] tablespace ID
 *  aPageID  - [IN] corrupted page ID
 ***********************************************************************/
IDE_RC sdrCorruptPageMgr::delCorruptPage( scSpaceID aSpaceID,
                                          scPageID  aPageID )
{
    scGRID                  sLogGRID;
    sdrCorruptPageHashNode* sNode = NULL;
    smiTableSpaceAttr       sTBSAttr;

    SC_MAKE_GRID( sLogGRID, aSpaceID, aPageID, 0 );

    IDE_TEST( smuHash::findNode( &mCorruptedPages,
                                 &sLogGRID,
                                 (void**)&sNode )
              != IDE_SUCCESS );

    if ( sNode != NULL )
    {
        IDE_TEST( smuHash::deleteNode( &mCorruptedPages,
                                       &sLogGRID,
                                       (void**)&sNode )
                  != IDE_SUCCESS );

        sctTableSpaceMgr::getTBSAttrByID( NULL,
                                          aSpaceID,
                                          &sTBSAttr );
        ideLog::log( SM_TRC_LOG_LEVEL_WARNNING,
                     SM_TRC_DRECOVER_DEL_CORRUPTED_PAGE,
                     sTBSAttr.mName,
                     aPageID );

        IDE_ASSERT( iduMemMgr::free(sNode) == IDE_SUCCESS );
    }
    else
    {
        // ���ٸ� �������
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}


/***********************************************************************
 * Description : Corrupted Page���� ���¸� �˻��Ѵ�.
 *
 *    Corrupted pages hash �� ���Ե� page���� ���� Extent�� ���°�
 *    Free���� ��� Ȯ���Ѵ�. Free extent�� �ƴѰ�� Corrupted page��
 *    PID �� extent�� PID�� boot log�� ����ϰ� ������ �����Ѵ�.
 *
 **********************************************************************/
IDE_RC sdrCorruptPageMgr::checkCorruptedPages()
{
    sdrCorruptPageHashNode * sNode;
    UInt                     sState = 0;
    idBool                   sExistCorruptPage;
    idBool                   sIsFreeExt;
    smiTableSpaceAttr        sTBSAttr;
    scSpaceID                sSpaceID = SC_NULL_SPACEID ;
    scPageID                 sPageID  = SC_NULL_PID;

    mCorruptPageReadPolicy = SDB_CORRUPTED_PAGE_READ_ABORT;

    IDE_TEST( smuHash::open( &mCorruptedPages) != IDE_SUCCESS );
    sState = 1;

    IDE_TEST( smuHash::cutNode( &mCorruptedPages, (void **)&sNode )
              != IDE_SUCCESS );

    sExistCorruptPage = ID_FALSE;

    while( sNode != NULL )
    {
        sIsFreeExt = ID_FALSE ;

        // �ش� extent�� ���� free ���� Ȯ���Ѵ�.
        IDE_TEST( sdptbExtent::isFreeExtPage( NULL /*idvSQL*/,
                                              sNode->mSpaceID,
                                              sNode->mPageID,
                                              &sIsFreeExt )
                  != IDE_SUCCESS );

        if( sIsFreeExt == ID_TRUE )
        {
            // page�� ���� extent�� free�̴�
            sctTableSpaceMgr::getTBSAttrByID( NULL,
                                              sNode->mSpaceID,
                                              &sTBSAttr );
            ideLog::log( SM_TRC_LOG_LEVEL_WARNNING,
                         SM_TRC_DRECOVER_DEL_CORRUPTED_PAGE_NOTALLOC,
                         sTBSAttr.mName,
                         sNode->mPageID );
        }
        else
        {
            // page�� ���� extent�� free�� �ƴϴ�.
            sctTableSpaceMgr::getTBSAttrByID( NULL,
                                              sNode->mSpaceID,
                                              &sTBSAttr );
            ideLog::log( SM_TRC_LOG_LEVEL_WARNNING,
                         SM_TRC_DRECOVER_PAGE_IS_CORRUPTED,
                         sTBSAttr.mName,
                         sNode->mPageID );

            sSpaceID = sNode->mSpaceID;
            sPageID  = sNode->mPageID;
            
            sExistCorruptPage = ID_TRUE;
        }

        IDE_TEST( iduMemMgr::free( sNode ) != IDE_SUCCESS);

        IDE_TEST( smuHash::cutNode( &mCorruptedPages, (void **)&sNode )
                 != IDE_SUCCESS);
    }

    if( ( smuProperty::getCorruptPageErrPolicy()
          & SDR_CORRUPT_PAGE_ERR_POLICY_SERVERFATAL )
        == SDR_CORRUPT_PAGE_ERR_POLICY_SERVERFATAL )
    {
        // corrupt page�� �߰� �Ǿ��� �ÿ� ������ ������ ��������
        // ���ϴ� property, �� Property�� ���� ���� ��� corrupt
        // page�� �߰� �ϴ��� GG,LG Hdr�� corrupt �� ��츦
        // �����ϰ�� ������ �������� �ʴ´�. default : 0 (skip)

        IDE_TEST_RAISE( sExistCorruptPage == ID_TRUE,
                        page_corruption_fatal_error );
    }

    sState = 0;
    IDE_TEST( smuHash::close( &mCorruptedPages ) != IDE_SUCCESS );
    
    // BUG-45598: ������Ƽ�� ���� Corrupt Page ó�� ��å�� �� ����
    setPageErrPolicyByProp();

    return IDE_SUCCESS;

    IDE_EXCEPTION( page_corruption_fatal_error );
    {
        IDE_SET( ideSetErrorCode( smERR_FATAL_PageCorrupted,
                                  sSpaceID,
                                  sPageID ));
    }
    IDE_EXCEPTION_END;

    if( sState > 0 )
    {
        IDE_ASSERT( smuHash::close( &mCorruptedPages )
                    == IDE_SUCCESS );
    }

    setPageErrPolicyByProp();

    return IDE_FAILURE;
}


/***********************************************************************
 * Description : redo log�� rid�� �̿��� hash value�� ����
 *
 * space id�� page id�� ������ ��ȯ�Ͽ� ������ ����� �����Ѵ�.
 * �� �Լ��� corrupted page�� ���� hash function���� ���ȴ�.
 * hash key�� GRID�̴�.
 *
 * aGRID - [IN] <spaceid, pageid, 0>�� corrupt page�� ���� grid�̴�.
 **********************************************************************/
UInt sdrCorruptPageMgr::genHashValueFunc( void* aGRID )
{
    IDE_DASSERT( aGRID != NULL );
    IDE_DASSERT( sctTableSpaceMgr::isSystemMemTableSpace( SC_MAKE_SPACE(*(scGRID*)aGRID) )
                 == ID_FALSE );

    return ((UInt)SC_MAKE_SPACE(*(scGRID*)aGRID) +
            (UInt)SC_MAKE_PID(*(scGRID*)aGRID));
}


/***********************************************************************
 * Description : hash-key ���Լ�
 *
 * 2���� GRID�� ������ ���Ѵ�. ������ 0�� �����Ѵ�.
 * �� �Լ��� corrupted page �� ���� hash compare function����
 * ���ȴ�.
 *
 * aLhs - [IN] page�� grid�� <spaceid, pageid, 0>�� ����.
 * aRhs - [IN] page�� grid�� <spaceid, pageid, 0>�� ����.
 **********************************************************************/
SInt sdrCorruptPageMgr::compareFunc( void*  aLhs,
                                     void*  aRhs )
{
    IDE_DASSERT( aLhs != NULL );
    IDE_DASSERT( aRhs != NULL );

    scGRID  *sLhs = (scGRID *)aLhs;
    scGRID  *sRhs = (scGRID *)aRhs;

    return ( ( SC_MAKE_SPACE( *sLhs ) == SC_MAKE_SPACE( *sRhs ) ) &&
             ( SC_MAKE_PID( *sLhs )  == SC_MAKE_PID( *sRhs ) ) ? 0 : -1 );
}

/***********************************************************************
 * Description : page��ü�� Overwrite�ϴ� log���� Ȯ���� �ش�.
 * aLogType - [IN] Ȯ���� log�� logtype
 **********************************************************************/
idBool sdrCorruptPageMgr::isOverwriteLog( sdrLogType aLogType )
{
    idBool sResult = ID_FALSE;

    if( ( smuProperty::getCorruptPageErrPolicy()
          & SDR_CORRUPT_PAGE_ERR_POLICY_OVERWRITE )
        == SDR_CORRUPT_PAGE_ERR_POLICY_OVERWRITE )
    {
        if( ( aLogType == SDR_SDP_WRITE_PAGEIMG ) ||
            ( aLogType == SDR_SDP_WRITE_DPATH_INS_PAGE ) ||
            ( aLogType == SDR_SDP_INIT_PHYSICAL_PAGE) ||
            ( aLogType == SDR_SDP_PAGE_CONSISTENT ) )
        {
            sResult = ID_TRUE;
        }
    }

    return sResult;
}

/***********************************************************************
 * Description : corrupt page�� �о��� �� ������ �����Ű�� �ʴ´�.
 **********************************************************************/
void sdrCorruptPageMgr::allowReadCorruptPage()
{
    if( ( smuProperty::getCorruptPageErrPolicy()
          & SDR_CORRUPT_PAGE_ERR_POLICY_OVERWRITE )
        == SDR_CORRUPT_PAGE_ERR_POLICY_OVERWRITE )
    {
        mCorruptPageReadPolicy = SDB_CORRUPTED_PAGE_READ_PASS;
    }
    else
    {
        mCorruptPageReadPolicy = SDB_CORRUPTED_PAGE_READ_ABORT;
    }
}

/***********************************************************************
 * Description : corrupt page�� �о��� �� ������ �����Ų��.
 **********************************************************************/
void sdrCorruptPageMgr::fatalReadCorruptPage()
{
    mCorruptPageReadPolicy = SDB_CORRUPTED_PAGE_READ_FATAL ;
}

/***********************************************************************
 * Description : ��� corrupt page�� �о��� �� ��å�� CorruptPageErrPolicy
 *              ������Ƽ ���� ���� �����Ѵ�.(BUG-45598)
 **********************************************************************/
void sdrCorruptPageMgr::setPageErrPolicyByProp()
{
    if ( ( smuProperty::getCorruptPageErrPolicy()
           & SDR_CORRUPT_PAGE_ERR_POLICY_SERVERFATAL )
         == SDR_CORRUPT_PAGE_ERR_POLICY_SERVERFATAL )
    {
        mCorruptPageReadPolicy = SDB_CORRUPTED_PAGE_READ_FATAL;
    }
    else
    {
        mCorruptPageReadPolicy = SDB_CORRUPTED_PAGE_READ_ABORT;
    }
}


