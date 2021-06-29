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
* $Id: sctTBSAlter.cpp 19201 2006-11-30 00:54:40Z kmkim $
**********************************************************************/

#include <idl.h>
#include <idm.h>
#include <idu.h>
#include <ideErrorMgr.h>
#include <smDef.h>
#include <smErrorCode.h>
#include <smu.h>
#include <sctReq.h>
#include <sctTableSpaceMgr.h>
#include <sctTBSAlter.h>

/*
  ������ (�ƹ��͵� ����)
*/
sctTBSAlter::sctTBSAlter()
{

}

/*
    Tablespace Attrbute Flag�� ���� ( ex> ALTER TABLESPACE LOG COMPRESS ON )
    
    aTrans      [IN] Transaction
    aSpaceID    [IN] Tablespace�� ID
    aAttrFlagMask             [IN] ������ Attribute Flag�� MASK
    aAttrFlagNewValue         [IN] ������ Attribute Flag�� ���ο� ��
    
    [ �˰��� ]
      (010) lock TBSNode in X
      (030) �α�ǽ� => ALTER_TBS_ATTR_FLAG
      (040) ATTR FLAG�� ���� 
      (050) Tablespace Node�� Log Anchor�� Flush!
      
    [ ALTER_TBS_ATTR_FLAG �� REDO ó�� ]
      (r-010) TBSNode.AttrFlag := AfterImage.AttrFlag

    [ ALTER_TBS_ATTR_FLAG �� UNDO ó�� ]
      (u-010) �α�ǽ� -> CLR ( ALTER_TBS_ATTR_FLAG )
      (u-020) TBSNode.AttrFlag := BeforeImage.AttrFlag
*/
IDE_RC sctTBSAlter::alterTBSAttrFlag(void      * aTrans,
                                     scSpaceID   aTableSpaceID,
                                     UInt        aAttrFlagMask, 
                                     UInt        aAttrFlagNewValue )
{
    sctTableSpaceNode * sSpaceNode;
    UInt                sBeforeAttrFlag;
    UInt                sAfterAttrFlag;

    IDE_DASSERT( aTrans != NULL );

    ///////////////////////////////////////////////////////////////////////////
    // Tablespace ID�κ��� Node�� �����´�    
    IDE_TEST( sctTableSpaceMgr::findSpaceNodeBySpaceID( aTableSpaceID,
                                                        (void**)&sSpaceNode )
                  != IDE_SUCCESS );

    ///////////////////////////////////////////////////////////////////////////
    // (010) lock TBSNode in X
    //       DROPPED,DISCARD,OFFLINE �� ��� ���⿡�� ������ �߻��Ѵ�.
    IDE_TEST( sctTableSpaceMgr::lockTBSNode( 
                                   aTrans,
                                   sSpaceNode,
                                   ID_FALSE,   /* intent */
                                   ID_TRUE,    /* exclusive */
                                   SCT_VAL_DDL_DML) /* validation */
              != IDE_SUCCESS );

    // Tablespace Attribute������ ���� Attribute Pointer�� �����´�
    sBeforeAttrFlag = sctTableSpaceMgr::getTBSAttrFlag( sSpaceNode );

    ///////////////////////////////////////////////////////////////////////////
    // (e-010) �̹� �ش� Attribute�� �����Ǿ� �ִ� ��� ���� 
    IDE_TEST( checkErrorOnAttrFlag( sSpaceNode,
                                    sBeforeAttrFlag,
                                    aAttrFlagMask,
                                    aAttrFlagNewValue )
              != IDE_SUCCESS );

    // ���ο� AttrFlag := MASK��Ʈ ���� NewValue��Ʈ Ų��
    sAfterAttrFlag   = ( sBeforeAttrFlag & ~aAttrFlagMask ) |
                       aAttrFlagNewValue ;
    
    ///////////////////////////////////////////////////////////////////////////
    // (030) �α�ǽ� => ALTER_TBS_ATTR_FLAG
    IDE_TEST( smLayerCallback::writeTBSAlterAttrFlag ( aTrans,
                                                       aTableSpaceID,
                                                       /* Before Image */
                                                       sBeforeAttrFlag,
                                                       /* After Image */
                                                       sAfterAttrFlag )
              != IDE_SUCCESS );

    ///////////////////////////////////////////////////////////////////////////
    // (040) AttrFlag ����
    sctTableSpaceMgr::setTBSAttrFlag( sSpaceNode ,
                                      sAfterAttrFlag );

    ///////////////////////////////////////////////////////////////////////////
    // (050) Tablespace Node�� Log Anchor�� Flush!
    IDE_TEST( smLayerCallback::updateTBSNodeAndFlush( sSpaceNode ) 
              != IDE_SUCCESS );
    
    return IDE_SUCCESS;
    
    IDE_EXCEPTION_END;

    // (010)���� ȹ���� Tablespace X Lock�� UNDO�Ϸ��� �ڵ����� Ǯ�Եȴ�
    // ���⼭ ���� ó���� �ʿ� ����
    
    return IDE_FAILURE;
}

/*
    Tablespace Attrbute Flag�� ���濡 ���� ����ó�� 
    aSpaceNode                [IN] Tablespace�� Node
    aCurrentAttrFlag          [IN] ���� Tablespace�� ������ Attribute Flag��
    aAttrFlagMask             [IN] ������ Attribute Flag�� MASK
    aAttrFlagNewValue         [IN] ������ Attribute Flag�� ���ο� ��
    
    [ ����ó�� ]
      (e-010) Volatile Tablespace�� ����
              �α� ���� ���θ� �����Ϸ� �ϸ� ���� 
      (e-020) �̹� Attrubute Flag�� �����Ǿ� ������ ����
      
    [ �������� ]
      aTBSNode�� �ش��ϴ� Tablespace�� X���� �����ִ� ���¿��� �Ѵ�.
*/
IDE_RC sctTBSAlter::checkErrorOnAttrFlag( sctTableSpaceNode * aSpaceNode,
                                          UInt        aCurrentAttrFlag,
                                          UInt        aAttrFlagMask, 
                                          UInt        aAttrFlagNewValue)
{
    IDE_DASSERT( aSpaceNode != NULL );

    ///////////////////////////////////////////////////////////////////////////
    //  (e-010) Volatile Tablespace�� ����
    //          �α� ���� �ϵ��� �����ϸ� ���� 
    if ( sctTableSpaceMgr::isVolatileTableSpace( aSpaceNode->mID ) == ID_TRUE )
    {
        if ( (aAttrFlagMask & SMI_TBS_ATTR_LOG_COMPRESS_MASK) != 0 )
        {
            if ( (aAttrFlagNewValue & SMI_TBS_ATTR_LOG_COMPRESS_MASK )
                 == SMI_TBS_ATTR_LOG_COMPRESS_TRUE )
            {
                IDE_RAISE( err_volatile_log_compress );
            }
        }
    }

    ///////////////////////////////////////////////////////////////////////////
    // (e-020) �̹� Attribute Flag�� �����Ǿ� ������ ����
    IDE_TEST_RAISE( ( aCurrentAttrFlag & aAttrFlagMask )
                    == aAttrFlagNewValue,
                    error_already_set_attr_flag);
    
    return IDE_SUCCESS;

    IDE_EXCEPTION( error_already_set_attr_flag );
    {
        IDE_SET(ideSetErrorCode( smERR_ABORT_TBS_ATTR_FLAG_ALREADY_SET ));
    }
    IDE_EXCEPTION( err_volatile_log_compress );
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_UNABLE_TO_COMPRESS_VOLATILE_TBS_LOG));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
    
}

