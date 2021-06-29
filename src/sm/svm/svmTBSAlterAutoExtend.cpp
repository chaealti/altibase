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
 
#include <idl.h>
#include <idm.h>
#include <idu.h>
#include <ideErrorMgr.h>
#include <smDef.h>
#include <smErrorCode.h>
#include <smu.h>
#include <svmReq.h>
#include <sctTableSpaceMgr.h>
#include <svmDatabase.h>
#include <svmManager.h>
#include <svmTBSAlterAutoExtend.h>

/*
  ������ (�ƹ��͵� ����)
*/
svmTBSAlterAutoExtend::svmTBSAlterAutoExtend()
{

}


/*
    ALTER TABLESPACE AUTOEXTEND ... �� ���� �ǽ� 
    
    aTrans      [IN] Transaction
    aSpaceID    [IN] Tablespace�� ID
    aAutoExtend [IN] ����ڰ� ������ Auto Extend ����
                     ID_TRUE => Auto Extend �ǽ�
    aNextSize   [IN] ����ڰ� ������ NEXT (�ڵ�Ȯ��)ũ�� ( byte���� )
                     �������� ���� ��� 0
    aMaxSize    [IN] ����ڰ� ������ MAX (�ִ�)ũ�� ( byte���� )
                     �������� ���� ��� 0
                     UNLIMITTED�� ������ ��� ID_ULONG_MAX

    [ �䱸���� ]
      - Auto Extend On/Off�� NEXT�� MAXSIZE �� ���� ( Oracle�� ���� )
        - Off�� ����� NEXT�� MAXSIZE�� 0���� ����
        - On���� ����� NEXT�� MAXSIZE�� �ý��� �⺻������ ����
          - ����ڰ� NEXT�� MAXSIZE�� ���� ������ ���, ������ ������ ����
    
    [ �˰��� ]
      (010) lock TBSNode in X
      (020) NextPageCount, MaxPageCount ��� 
      (030) �α�ǽ� => ALTER_TBS_AUTO_EXTEND
      (040) AutoExtendMode, NextSize, MaxSize ���� 
      
    [ ALTER_TBS_AUTO_EXTEND �� REDO ó�� ]
      (r-010) TBSNode.AutoExtend := AfterImage.AutoExtend
      (r-020) TBSNode.NextSize   := AfterImage.NextSize
      (r-030) TBSNode.MaxSize    := AfterImage.MaxSize

    [ ALTER_TBS_AUTO_EXTEND �� UNDO ó�� ]
      (u-010) �α�ǽ� -> CLR ( ALTER_TBS_AUTO_EXTEND )
      (u-020) TBSNode.AutoExtend := BeforeImage.AutoExtend
      (u-030) TBSNode.NextSize   := BeforeImage.NextSize
      (u-040) TBSNode.MaxSize    := BeforeImage.MaxSize
*/
IDE_RC svmTBSAlterAutoExtend::alterTBSsetAutoExtend(void      * aTrans,
                                            scSpaceID   aTableSpaceID,
                                            idBool      aAutoExtendMode,
                                            ULong       aNextSize,
                                            ULong       aMaxSize )
{
    svmTBSNode        * sTBSNode;
    smiTableSpaceAttr * sTBSAttr;
    
    scPageID            sNextPageCount;
    scPageID            sMaxPageCount;
    
    IDE_DASSERT( aTrans != NULL );
    IDE_DASSERT( sctTableSpaceMgr::isVolatileTableSpace( aTableSpaceID ) == ID_TRUE );

    ///////////////////////////////////////////////////////////////////////////
    // Tablespace ID�κ��� Node�� �����´�
    IDE_TEST( sctTableSpaceMgr::findSpaceNodeBySpaceID( aTableSpaceID,
                                                        (void**)&sTBSNode )
                  != IDE_SUCCESS );

    sTBSAttr = & sTBSNode->mTBSAttr;

    ///////////////////////////////////////////////////////////////////////////
    // (010) lock TBSNode in X
    
    // BUGBUG-1548 check for lock timeout
    IDE_TEST( sctTableSpaceMgr::lockTBSNode(
                                   aTrans,
                                   &sTBSNode->mHeader,
                                   ID_FALSE,   /* intent */
                                   ID_TRUE,    /* exclusive */
                                   ID_ULONG_MAX, /* lock wait micro sec */
                                   SCT_VAL_DDL_DML,
                                   NULL,       /* is locked */
                                   NULL ) /* lockslot */
              != IDE_SUCCESS );

    ///////////////////////////////////////////////////////////////////////////
    // (e-010) TBSNode.state �� DROPPED�̸� ���� 
    // (e-020) TBSNode.state �� OFFLINE�̸� ���� 
    // (e-030) �̹� AUTOEXTEND == ON  �ε� �Ǵٽ� ON������ ����
    // (e-040) �̹� AUTOEXTEND == OFF �ε� �Ǵٽ� OFF������ ����
    // (e-050) NextSize�� EXPAND_CHUNK_PAGE_COUNT*SM_PAGE_SIZE��
    //         ������ �������� ������ ����
    // (e-060) Tablespace�� ����ũ�� > MAXSIZE �̸� ����
    IDE_TEST( checkErrorOnAutoExtendAttrs( sTBSNode,
                                           aAutoExtendMode,
                                           aNextSize,
                                           aMaxSize )
              != IDE_SUCCESS );


    ///////////////////////////////////////////////////////////////////////////
    // (020)   NextPageCount, MaxPageCount ��� 
    IDE_TEST( calcAutoExtendAttrs( sTBSNode,
                                   aAutoExtendMode,
                                   aNextSize,
                                   aMaxSize,
                                   & sNextPageCount,
                                   & sMaxPageCount )
              != IDE_SUCCESS );
    
    ///////////////////////////////////////////////////////////////////////////
    // (030) �α�ǽ� => ALTER_TBS_AUTO_EXTEND
    IDE_TEST( smLayerCallback::writeVolatileTBSAlterAutoExtend ( NULL, /* idvSQL* */
                                                                 aTrans,
                                                                 aTableSpaceID,
                                                                 /* Before Image */
                                                                 sTBSAttr->mVolAttr.mIsAutoExtend,
                                                                 sTBSAttr->mVolAttr.mNextPageCount,
                                                                 sTBSAttr->mVolAttr.mMaxPageCount,
                                                                 /* After Image */
                                                                 aAutoExtendMode,
                                                                 sNextPageCount,
                                                                 sMaxPageCount )
              != IDE_SUCCESS );

    ///////////////////////////////////////////////////////////////////////////
    // (040) AutoExtendMode, NextSize, MaxSize ���� 
    sTBSAttr->mVolAttr.mIsAutoExtend  = aAutoExtendMode;
    sTBSAttr->mVolAttr.mNextPageCount = sNextPageCount;
    sTBSAttr->mVolAttr.mMaxPageCount  = sMaxPageCount;
        
    return IDE_SUCCESS;
    
    IDE_EXCEPTION_END;

    // (010)���� ȹ���� Tablespace X Lock�� UNDO�Ϸ��� �ڵ����� Ǯ�Եȴ�
    // ���⼭ ���� ó���� �ʿ� ����
    
    return IDE_FAILURE;
}

/*
    ALTER TABLESPACE AUTOEXTEND ... �� ���� ����ó��

    [ ����ó�� ]
      (e-010) TBSNode.state �� DROPPED�̸� ����
      (e-020) TBSNode.state �� OFFLINE�̸� ���� 
      (e-050) NextSize�� EXPAND_CHUNK_PAGE_COUNT*SM_PAGE_SIZE��
              ������ �������� ������ ����
      (e-060) Tablespace�� ����ũ�� > MAXSIZE �̸� ����
      
    [ �������� ]
      aTBSNode�� �ش��ϴ� Tablespace�� X���� �����ִ� ���¿��� �Ѵ�.
*/
IDE_RC svmTBSAlterAutoExtend::checkErrorOnAutoExtendAttrs( svmTBSNode * aTBSNode,
                                                   idBool       aAutoExtendMode,
                                                   ULong        aNextSize,
                                                   ULong        aMaxSize )
{
    scPageID            sChunkPageCount;
    ULong               sCurrTBSSize;

    IDE_DASSERT( aTBSNode != NULL );
        
    ///////////////////////////////////////////////////////////////////////////
    // (e-010) TBSNode.state �� DROPPED�̸� ���� 
    IDE_TEST_RAISE( SMI_TBS_IS_DROPPED(aTBSNode->mHeader.mState),
                    error_dropped_tbs );

    ///////////////////////////////////////////////////////////////////////////
    // (e-020) TBSNode.state �� OFFLINE�̸� ���� 
    IDE_TEST_RAISE( SMI_TBS_IS_OFFLINE(aTBSNode->mHeader.mState),
                    error_offline_tbs );
    
    sChunkPageCount = svmDatabase::getExpandChunkPageCnt( &aTBSNode->mMemBase );

    if ( aNextSize != ID_ULONG_MAX ) // ����ڰ� Next Size�� ������ ���
    {
        ////////////////////////////////////////////////////////////////////////
        // (e-050) NextSize�� EXPAND_CHUNK_PAGE_COUNT*SM_PAGE_SIZE��
        //         ������ �������� ������ ����
        IDE_TEST_RAISE( (aNextSize % ( sChunkPageCount * SM_PAGE_SIZE )) != 0,
                        error_alter_tbs_nextsize_not_aligned_to_chunk_size );
    }

    if ( (aMaxSize != ID_ULONG_MAX) && // ����ڰ� Max Size�� ������ ���
         (aMaxSize != 0 ) )            // MAXSIZE UNLIMITED�� �ƴ� ���
    {
        ////////////////////////////////////////////////////////////////////////
        // (e-060) Tablespace�� ����ũ�� > MAXSIZE �̸� ����
        sCurrTBSSize = svmDatabase::getAllocPersPageCount( &aTBSNode->mMemBase )
                       * SM_PAGE_SIZE ;

        IDE_TEST_RAISE( (aAutoExtendMode == ID_TRUE) &&
                        ( sCurrTBSSize > aMaxSize),
                        error_maxsize_lessthan_current_size );
        
    }
    
    return IDE_SUCCESS;

    IDE_EXCEPTION( error_alter_tbs_nextsize_not_aligned_to_chunk_size )
    {
        IDE_SET(ideSetErrorCode(
                    smERR_ABORT_ALTER_TBS_NEXTSIZE_NOT_ALIGNED_TO_CHUNK_SIZE,
                    /* K byte ������ Expand Chunkũ�� */
                    (ULong) ( (ULong)sChunkPageCount * SM_PAGE_SIZE / 1024)
                    ));
    }
    IDE_EXCEPTION( error_maxsize_lessthan_current_size );
    {
        IDE_SET(ideSetErrorCode(
                    smERR_ABORT_ALTER_TBS_MAXSIZE_LESSTHAN_CURRENT_SIZE,
                    /* K byte ������ Current Tablespace Size */
                    (ULong) ( sCurrTBSSize / 1024 )
                    ));
    }
    IDE_EXCEPTION( error_dropped_tbs );
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_ALTER_TBS_AT_DROPPED_TBS));
    }
    IDE_EXCEPTION( error_offline_tbs );
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_ALTER_TBS_AT_OFFLINE_TBS));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
    
}

/*
    Next Page Count �� Max Page Count�� ����Ѵ�.
    
    aMemBase    [IN] Tablespace�� 0�� Page�� �����ϴ� Membase
    aAutoExtend [IN] ����ڰ� ������ Auto Extend ����
                     ID_TRUE => Auto Extend �ǽ�
    aNextSize   [IN] ����ڰ� ������ NEXT (�ڵ�Ȯ��)ũ�� ( byte���� )
                     �������� ���� ��� 0
    aMaxSize    [IN] ����ڰ� ������ MAX (�ִ�)ũ�� ( byte���� )
                     �������� ���� ��� 0
                     UNLIMITTED�� ������ ��� ID_ULONG_MAX

    aNextPageCount [OUT] Tablespace�� �ڵ�Ȯ�� ���� ( SM Page ���� )
    aMaxPageCount  [OUT] Tablespace�� �ִ� ũ��      ( SM Page ���� )


    [ �˰��� ]
    
      if AUTO EXTEND == ON
         (010) NewNextSize := DEFAULT Next Size Ȥ�� ����ڰ� ������ ��
         (020) NewMaxSize  := DEFAULT Max Size  Ȥ�� ����ڰ� ������ ��
      else // AUTO EXTEND OFF
         (030) NewNextSize := 0
         (040) NewMaxSize  := 0
      fi
    
    

    [ �������� ]
      aTBSNode�� �ش��ϴ� Tablespace�� X���� �����ִ� ���¿��� �Ѵ�.
 */
IDE_RC svmTBSAlterAutoExtend::calcAutoExtendAttrs(
                          svmTBSNode * aTBSNode,
                          idBool       aAutoExtendMode,
                          ULong        aNextSize,
                          ULong        aMaxSize,
                          scPageID   * aNextPageCount,
                          scPageID   * aMaxPageCount )
{
    scPageID sNextPageCount;
    scPageID sMaxPageCount;
    
    IDE_DASSERT( aTBSNode != NULL );
    IDE_DASSERT( aNextPageCount != NULL );
    IDE_DASSERT( aMaxPageCount != NULL );
    
    if ( aAutoExtendMode == ID_TRUE ) /* Auto Extend On */
    {
        // Oracle�� �����ϰ� ó��
        // Auto Extend On�� Next/Max Size�� �������� �ʾҴٸ�
        // �ý����� �⺻���� ��� 
        if ( aNextSize == ID_ULONG_MAX ) // ����ڰ� Next Size�� �������� ���� ���
        {
            // �⺻�� => EXPAND CHUNK�� ũ�� 
            sNextPageCount =
                svmDatabase::getExpandChunkPageCnt( &aTBSNode->mMemBase );
        }
        else                  // ����ڰ� Next Size�� ������ ��� 
        {
            sNextPageCount = aNextSize / SM_PAGE_SIZE ;
        }
        
        
        // ����ڰ� Max Size�� �������� ���� ���  �Ǵ� UNLIMITED�� ������ ���
        if ( (aMaxSize == ID_ULONG_MAX) || (aMaxSize == 0) )
        {
            // �⺻�� => UNLIMITTED
            sMaxPageCount = SM_MAX_PID + 1;
        }
        else
        {
            sMaxPageCount = aMaxSize / SM_PAGE_SIZE ;
        }
    } 
    else /* Auto Extend Off */
    {
        // Oracle�� �����ϰ� ó��
        // Auto Extend Off�� Next/Max Size�� 0���� �ʱ�ȭ 
        sNextPageCount = 0;
        sMaxPageCount = 0;
    }
    
    *aNextPageCount = sNextPageCount;
    *aMaxPageCount = sMaxPageCount;

    return IDE_SUCCESS;

}

