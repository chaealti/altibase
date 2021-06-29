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
 * $Id: sctTBSUpdate.cpp 23652 2007-10-01 23:20:28Z bskim $
 **********************************************************************/

#include <idl.h>
#include <ideErrorMgr.h>
#include <smErrorCode.h>
#include <sct.h>
#include <sctReq.h>
#include <sctTBSUpdate.h>

/*
    Tablespace�� Attribute Flag���濡 ���� �α׸� �м��Ѵ�.

    [IN]  aValueSize     - Log Image �� ũ�� 
    [IN]  aValuePtr      - Log Image
    [OUT] aAutoExtMode   - Auto extent mode
 */
IDE_RC sctTBSUpdate::getAlterAttrFlagImage( UInt       aValueSize,
                                            SChar    * aValuePtr,
                                            UInt     * aAttrFlag )
{
    IDE_DASSERT( aValuePtr != NULL );
    IDE_DASSERT( aValueSize == (UInt)( ID_SIZEOF(*aAttrFlag) ) );
    IDE_DASSERT( aAttrFlag  != NULL );

    ACP_UNUSED( aValueSize );
    
    idlOS::memcpy(aAttrFlag, aValuePtr, ID_SIZEOF(*aAttrFlag));
    aValuePtr += ID_SIZEOF(*aAttrFlag);

    return IDE_SUCCESS;
}



/*
    Tablespace�� Attribute Flag���濡 ���� �α��� Redo����

    [ �α� ���� ]
    After Image   --------------------------------------------
      UInt                aAfterAttrFlag
    
    [ ALTER_TBS_AUTO_EXTEND �� REDO ó�� ]
      (r-010) TBSNode.AttrFlag := AfterImage.AttrFlag
*/
IDE_RC sctTBSUpdate::redo_SCT_UPDATE_ALTER_ATTR_FLAG(
                       idvSQL*              /*aStatistics*/, 
                       void*                aTrans,
                       smLSN                /* aCurLSN */,
                       scSpaceID            aSpaceID,
                       UInt                 /*aFileID*/,
                       UInt                 aValueSize,
                       SChar*               aValuePtr,
                       idBool               /* aIsRestart */ )
{

    sctTableSpaceNode  * sSpaceNode;
    UInt                 sAttrFlag;
    
    IDE_DASSERT( aTrans != NULL );
    ACP_UNUSED( aTrans );
    
    // aValueSize, aValuePtr �� ���� ���� DASSERTION��
    // getAlterAttrFlagImage ���� �ǽ�.
    IDE_TEST( getAlterAttrFlagImage( aValueSize,
                                       aValuePtr,
                                       & sAttrFlag ) != IDE_SUCCESS );

    sSpaceNode = sctTableSpaceMgr::findSpaceNodeWithoutException( aSpaceID );
    
    if ( sSpaceNode != NULL )
    {
        // Tablespace Attribute�����Ѵ�
        sctTableSpaceMgr::setTBSAttrFlag( sSpaceNode,
                                          sAttrFlag );
    }
    else
    {
        // �̹� Drop�� Tablespace�� ��� 
        // nothing to do ...
    }
        
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}


/*
    Tablespace�� Attribute Flag���濡 ���� �α��� Undo����

    [ �α� ���� ]
    After Image   --------------------------------------------
      UInt                aBeforeAttrFlag
    
    [ ALTER_TBS_AUTO_EXTEND �� REDO ó�� ]
      (r-010) TBSNode.AttrFlag := AfterImage.AttrFlag
*/
IDE_RC sctTBSUpdate::undo_SCT_UPDATE_ALTER_ATTR_FLAG(
                       idvSQL*              /*aStatistics*/, 
                       void*                aTrans,
                       smLSN                /* aCurLSN */,
                       scSpaceID            aSpaceID,
                       UInt                 /*aFileID*/,
                       UInt                 aValueSize,
                       SChar*               aValuePtr,
                       idBool               aIsRestart )
{

    sctTableSpaceNode  * sSpaceNode;
    UInt                 sAttrFlag;
    
    IDE_DASSERT( aTrans != NULL );
    ACP_UNUSED( aTrans );
    
    // aValueSize, aValuePtr �� ���� ���� DASSERTION��
    // getAlterAttrFlagImage ���� �ǽ�.
    IDE_TEST( getAlterAttrFlagImage( aValueSize,
                                       aValuePtr,
                                       & sAttrFlag ) != IDE_SUCCESS );

    sSpaceNode = sctTableSpaceMgr::findSpaceNodeWithoutException( aSpaceID );
    
    if ( sSpaceNode != NULL )
    {
        // Tablespace Attribute�����Ѵ�
        sctTableSpaceMgr::setTBSAttrFlag( sSpaceNode,
                                          sAttrFlag );

        if (aIsRestart == ID_FALSE)
        {
            // Log Anchor�� flush.
            IDE_TEST( smLayerCallback::updateTBSNodeAndFlush( (sctTableSpaceNode*)sSpaceNode )
                      != IDE_SUCCESS );
        }
        else
        {
            // RESTART�ÿ��� Loganchor�� flush���� �ʴ´�.
        }
    }
    else
    {
        // �̹� Drop�� Tablespace�� ��� 
        // nothing to do ...
    }
        
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

