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
 * $Id: smiTableSpace.cpp 86110 2019-09-02 04:52:04Z et16 $
 **********************************************************************/

/***********************************************************************
 * FILE DESCRIPTION : smiTableSpace.cpp
 *   �� ������ smiTableSpace Ŭ������ ���� �����̴�.
 *   smiTableSpace Ŭ������ Disk Resident DB���� ���Ǵ�
 *   user table space�� ����, ���� �� ������ �߰���
 *   �� ��ƾ�� �����ϰ� �ִ�.
 *   ���� TableSpace�� ������ Meta(Catalog)�� ������� �ʰ�
 *   sdpTableSpace ���������� ���ǵ� Ư�� �޸� ����ü�� ����� ����
 *   �̴�. �� �޸� ����ü�� Log Anchor File�� ���ǵ� �������� �״��
 *   �����Ѵ�.
 **********************************************************************/

#include <idl.h>
#include <ideErrorMgr.h>
#include <smErrorCode.h>
#include <smDef.h>
#include <smiTableSpace.h>
#include <smiMain.h>
#include <smiMisc.h>
#include <smiTrans.h>
#include <smm.h>
#include <svm.h>
#include <sdpTableSpace.h>
#include <sddTableSpace.h>
#include <sddDiskMgr.h>
#include <sddDataFile.h>
#include <sctTableSpaceMgr.h>
#include <sctDef.h>
#include <sctTBSAlter.h>
#include <smpTBSAlterOnOff.h>
#include <smuProperty.h>

/* (����) Memory Tablespace�� ���ؼ��� �� Function Pointer Array��
          ���� �������� �ʵ��� �����Ǿ��ִ�
*/
static const smiCreateDiskTBSFunc smiCreateDiskTBSFunctions[SMI_TABLESPACE_TYPE_MAX]=
{
    //SMI_MEMORY_SYSTEM_DICTIONARY
    NULL,
    //SMI_MEMORY_SYSTEM_DATA
    NULL,
    //SMI_MEMORY_USER_DATA
    NULL,
    //SMI_DISK_SYSTEM_DATA
    sdpTableSpace::createTBS,
    //SMI_DISK_USER_DATA
    sdpTableSpace::createTBS,
    // SMI_DISK_SYSTEM_TEMP
    sdpTableSpace::createTBS,
    // SMI_DISK_USER_TEMP
    sdpTableSpace::createTBS,
    // SMI_DISK_SYSTEM_UNDO,
    sdpTableSpace::createTBS
};

/***********************************************************************
 * FUNCTION DESCRIPTION : smiTableSpace::createDiskTBS()
 *   �� �Լ��� create tablespace ������ ���Ͽ� �Ҹ���
 *   ���ο� Disk TableSpace�� �����Ѵ�.
 *   ���ڷ� �Ѿ�� ���̺� �����̽� �Ӽ��� ����Ÿ ���� �Ӽ���
 *   list�� �޾Ƽ� sdpTableSpace�� �Լ��� ȣ���Ѵ�.
 *   �� ����� table space�� id�� aTableSpaceAttr.mID��
 *   assign�ϰ� �ȴ�.
 *   ���̺� �����̽� type�� ������ ����.
 *   1> �Ϲ����̺� �����̽�   :  SMI_DISK_USER_DATA
 *   2> system temp table �����̽� :  SMI_DISK_SYSTEM_TEMP
 *   3> user   temp table �����̽� :  SMI_DISK_USER_TEMP
 *   4> undo table �����̽�   :  SMI_DISK_SYSTEM_UNDO
 *   5> system table �����̽� :  SMI_DISK_SYSTEM_DATA
 * BUGBUG : ext page count�� attr�� �Ӽ��� �Ǿ����Ƿ�
 * �̸� ���ڷ� ���� ���� �ʿ䰡 ����. QP�� ���� �κ��� �ٲ��� �ϹǷ�
 * �۾��� ���� �� ���� ������ ������.
 **********************************************************************/
IDE_RC smiTableSpace::createDiskTBS(idvSQL            * aStatistics,
                                    smiTrans          * aTrans,
                                    smiTableSpaceAttr * aTableSpaceAttr,
                                    smiDataFileAttr  ** aDataFileAttrList,
                                    UInt                aFileAttrCnt,
                                    UInt                aExtPageCnt,
				                    ULong               aExtSize)
{
    IDE_DASSERT( aTrans != NULL );
    IDE_DASSERT( aTableSpaceAttr != NULL );
    IDE_DASSERT( (aTableSpaceAttr->mType >  SMI_MEMORY_SYSTEM_DICTIONARY) &&
                 (aTableSpaceAttr->mType <  SMI_TABLESPACE_TYPE_MAX) );
    IDE_DASSERT( aTableSpaceAttr->mAttrType == SMI_TBS_ATTR );
    IDE_ASSERT( aTableSpaceAttr->mDiskAttr.mExtMgmtType == 
                SMI_EXTENT_MGMT_BITMAP_TYPE );

    /* To Fix BUG-21394 */
    IDE_TEST_RAISE( aExtSize < 
		    ( smiGetPageSize(aTableSpaceAttr->mType) * smiGetMinExtPageCnt()),
		      ERR_INVALID_EXTENTSIZE);

    // BUGBUG-1548 CONTROL/PROCESS�ܰ��̸� ������ ������ ó���ؾ���
    aTableSpaceAttr->mDiskAttr.mExtPageCount = aExtPageCnt;

    IDE_TEST( smiCreateDiskTBSFunctions[aTableSpaceAttr->mType](
                  aStatistics,
                  aTableSpaceAttr,
                  aDataFileAttrList,
                  aFileAttrCnt,
                  aTrans->getTrans() )
              != IDE_SUCCESS);

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_INVALID_EXTENTSIZE)
    {
      IDE_SET(ideSetErrorCode(smERR_ABORT_EXTENT_SIZE_IS_TOO_SMALL));
    } 

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*
     �޸� Tablespace�� �����Ѵ�.
     [IN] aTrans          - Tablespace�� �����Ϸ��� Transaction
     [IN] aName           - Tablespace�� �̸�
     [IN] aAttrFlag       - Tablespace�� �Ӽ� Flag
     [IN] aChkptPathList  - Tablespace�� Checkpoint Image���� ������ Path��
     [IN] aSplitFileSize  - Checkpoint Image File�� ũ��
     [IN] aInitSize       - Tablespace�� �ʱ�ũ��
     [IN] aIsAutoExtend   - Tablespace�� �ڵ�Ȯ�� ����
     [IN] aNextSize       - Tablespace�� �ڵ�Ȯ�� ũ��
     [IN] aMaxSize        - Tablespace�� �ִ�ũ��
     [IN] aIsOnline       - Tablespace�� �ʱ� ���� (ONLINE�̸� ID_TRUE)
     [IN] aDBCharSet      - �����ͺ��̽� ĳ���� ��
     [IN] aNationalCharSet- ���ų� ĳ���� ��
     [OUT] aTBSID         - ������ Tablespace�� ID
 */
IDE_RC smiTableSpace::createMemoryTBS(
                          smiTrans             * aTrans,
                          SChar                * aName,
                          UInt                   aAttrFlag,
                          smiChkptPathAttrList * aChkptPathList,
                          ULong                  aSplitFileSize,
                          ULong                  aInitSize,
                          idBool                 aIsAutoExtend,
                          ULong                  aNextSize,
                          ULong                  aMaxSize,
                          idBool                 aIsOnline,
                          scSpaceID            * aTBSID )
{
    IDE_DASSERT( aTrans != NULL );
    IDE_DASSERT( aName != NULL );
    // aChkptPathList�� ����ڰ� �������� ���� ��� NULL�̴�.
    // aSplitFileSize�� ����ڰ� �������� ���� ��� 0�̴�.
    // aInitSize�� ����ڰ� �������� ���� ��� 0�̴�.
    // aNextSize�� ����ڰ� �������� ���� ��� 0�̴�.
    // aMaxSize�� ����ڰ� �������� ���� ��� 0�̴�.

    // BUGBUG-1548 CONTROL/PROCESS�ܰ��̸� ������ ������ ó���ؾ���
    
    IDE_TEST( smmTBSCreate::createTBS( aTrans->getTrans(),
                                       smmDatabase::getDBName(),
                                       aName,
                                       aAttrFlag,
                                       SMI_MEMORY_USER_DATA,
                                       aChkptPathList,
                                       aSplitFileSize,
                                       aInitSize,
                                       aIsAutoExtend,
                                       aNextSize,
                                       aMaxSize,
                                       aIsOnline,
                                       smmDatabase::getDBCharSet(),
                                       smmDatabase::getNationalCharSet(),
                                       aTBSID )
              != IDE_SUCCESS);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*   PROJ-1594 Volatile TBS
     Volatile Tablespace�� �����Ѵ�.
     [IN] aTrans          - Tablespace�� �����Ϸ��� Transaction
     [IN] aName           - Tablespace�� �̸�
     [IN] aAttrFlag       - Tablespace�� �Ӽ� Flag
     [IN] aInitSize       - Tablespace�� �ʱ�ũ��
     [IN] aIsAutoExtend   - Tablespace�� �ڵ�Ȯ�� ����
     [IN] aNextSize       - Tablespace�� �ڵ�Ȯ�� ũ��
     [IN] aMaxSize        - Tablespace�� �ִ�ũ��
     [IN] aState          - Tablespace�� ����
     [OUT] aTBSID         - ������ Tablespace�� ID
*/
IDE_RC smiTableSpace::createVolatileTBS(
                          smiTrans             * aTrans,
                          SChar                * aName,
                          UInt                   aAttrFlag,
                          ULong                  aInitSize,
                          idBool                 aIsAutoExtend,
                          ULong                  aNextSize,
                          ULong                  aMaxSize,
                          UInt                   aState,
                          scSpaceID            * aTBSID )
{
    IDE_DASSERT( aTrans != NULL );
    IDE_DASSERT( aName != NULL );
    // aInitSize�� ����ڰ� �������� ���� ��� 0�̴�.
    // aNextSize�� ����ڰ� �������� ���� ��� 0�̴�.
    // aMaxSize�� ����ڰ� �������� ���� ��� 0�̴�.

    IDE_TEST( svmTBSCreate::createTBS(aTrans->getTrans(),
                                       smmDatabase::getDBName(),
                                       aName,
                                       SMI_VOLATILE_USER_DATA,
                                       aAttrFlag,
                                       aInitSize,
                                       aIsAutoExtend,
                                       aNextSize,
                                       aMaxSize,
                                       aState,
                                       aTBSID )
              != IDE_SUCCESS);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
  FUNCTION DESCRIPTION : smiTableSpace::drop()
    �� �Լ��� drop tablespace ������ ���Ͽ� �Ҹ���,
     ������ TableSpace�� �����Ҷ� ȣ��Ǵ� �Լ��̴�.
    touch mode�� ���� ������ ����.

 SMI_ALL_TOUCH = 0,  ->tablespace�� ��� datafile �����
                          datafile�� �ǵ帰��.
 SMI_ALL_NOTOUCH,   ->tablespace�� ��� datafile �����
                          datafile�� �ǵ帮�� �ʴ´�.
 SMI_EACH_BYMODE    -> �� datafile ����� create ��忡 ������.
 **********************************************************************/
IDE_RC smiTableSpace::drop( idvSQL       */*aStatistics*/,
                            smiTrans*     aTrans,
                            scSpaceID     aTableSpaceID,
                            smiTouchMode  aTouchMode )
{
    sctTableSpaceNode * sTBSNode;

    IDE_DASSERT( aTrans != NULL );

    // fix bug-9563.
    // system tablespace�� drop�Ҽ� ����.
    IDE_TEST_RAISE( aTableSpaceID <= SMI_ID_TABLESPACE_SYSTEM_DISK_TEMP,
                    error_drop_system_tablespace);

    // BUGBUG-1548 CONTROL/PROCESS�ܰ��̸� ������ ������ ó���ؾ���

    // TBSID�� TBSNode�� �˾Ƴ���.
    IDE_TEST( sctTableSpaceMgr::findSpaceNodeBySpaceID(
                                          aTableSpaceID,
                                          (void**) & sTBSNode )
              != IDE_SUCCESS );

    switch ( sTBSNode->mType )
    {
        case SMI_MEMORY_USER_DATA:
            IDE_TEST( smmTBSDrop::dropTBS(
                          aTrans->getTrans(),
                          (smmTBSNode*) sTBSNode,
                          aTouchMode )
                      != IDE_SUCCESS );
            break;
        case SMI_VOLATILE_USER_DATA:
            /* PROJ-1594 Volatile TBS */
            IDE_TEST( svmTBSDrop::dropTBS( aTrans->getTrans(),
                                              (svmTBSNode*)sTBSNode )
                      != IDE_SUCCESS );
            break;
        case SMI_DISK_USER_DATA:
        case SMI_DISK_USER_TEMP:
            //sddTableSpace�� TableSpace�� �����ϴ� �Լ� ȣ��
            IDE_TEST(sdpTableSpace::dropTBS(
                         NULL, // BUGBUG : �߰� ��� from QP
                         aTrans->getTrans(),
                         aTableSpaceID,
                         aTouchMode)
                     != IDE_SUCCESS);
            break;

        case SMI_MEMORY_SYSTEM_DICTIONARY :
        case SMI_MEMORY_SYSTEM_DATA:
        case SMI_DISK_SYSTEM_TEMP:
        case SMI_DISK_SYSTEM_UNDO:
        case SMI_DISK_SYSTEM_DATA:
        default :
            // ������ Drop�Ұ����� TBS���� üũ�Ͽ����Ƿ�
            // ���⿡���� Drop�Ұ����� TBS�� ���� �� ����
            IDE_ASSERT(0);
            break;
    }

    return IDE_SUCCESS;
    
    IDE_EXCEPTION(error_drop_system_tablespace);
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_CannotDropTableSpace));
    }
    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

/***********************************************************************
 * FUNCTION DESCRIPTION : smiTableSpace::alterStatus()
 *   ALTER TABLESPACE ONLINE/OFFLINE�� ���� 
 *
 *
 *   �� �Լ��� alter tablespace tablespace�̸� [online| offline]
 *   ������ ���Ͽ� �Ҹ���, Ư�� TableSpace�� ���¸�
 *   online/offline���� �����Ѵ�.
 *   ������ �߰��� ������ ��� �۾����� online ���¿����� �����ϴ�.
 *
 * [IN] aTrans        - ���¸� �����Ϸ��� Transaction
 * [IN] aTableSpaceID - ���¸� �����Ϸ��� Tablespace�� ID
 * [IN] aState        - ���� ������ ���� ( Online or Offline )
 **********************************************************************/
IDE_RC smiTableSpace::alterStatus(idvSQL*     aStatistics,
                                  smiTrans  * aTrans,
                                  scSpaceID   aTableSpaceID,
                                  UInt        aState )
{
    sctTableSpaceNode * sTBSNode;

    IDE_DASSERT( aTrans != NULL );

    if ( ( smiGetStartupPhase() != SMI_STARTUP_META ) &&
         ( smiGetStartupPhase() != SMI_STARTUP_SERVICE ) )
    {
        IDE_RAISE( error_alter_tbs_onoff_allowed_only_at_meta_service_phase);
    }
    
    // TBSID�� TBSNode�� �˾Ƴ���.
    IDE_TEST( sctTableSpaceMgr::findSpaceNodeBySpaceID( aTableSpaceID,
                                                        (void**) & sTBSNode )
              != IDE_SUCCESS );

    switch ( sTBSNode->mType )
    {
        case SMI_MEMORY_USER_DATA:

            IDE_TEST( smpTBSAlterOnOff::alterTBSStatus( aTrans->getTrans(),
                                                        (smmTBSNode*) sTBSNode,
                                                        aState )
                      != IDE_SUCCESS );                
            break;
            
        case SMI_DISK_USER_DATA:
            IDE_TEST( sdpTableSpace::alterTBSStatus(
                          aStatistics, 
                          aTrans->getTrans(),
                          (sddTableSpaceNode*) sTBSNode,
                          aState ) != IDE_SUCCESS );
            break;
            
        case SMI_MEMORY_SYSTEM_DICTIONARY:
        case SMI_MEMORY_SYSTEM_DATA:
        case SMI_DISK_USER_TEMP:
        case SMI_DISK_SYSTEM_TEMP:
        case SMI_DISK_SYSTEM_UNDO:
        case SMI_DISK_SYSTEM_DATA:
        default :
            // ������ Drop�Ұ����� TBS���� üũ�Ͽ����Ƿ�
            // ���⿡���� Drop�Ұ����� TBS�� ���� �� ����
            IDE_ASSERT(0);
            break;
            
    }
    
    return IDE_SUCCESS;
    
    IDE_EXCEPTION(error_alter_tbs_onoff_allowed_only_at_meta_service_phase);
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_ALTER_TBS_ONOFF_ALLOWED_ONLY_AT_META_SERVICE_PHASE));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**
    Tablespace�� Attribute Flag�� �����Ѵ�.
    
    [IN] aTrans - Transaction
    [IN] aTableSpaceID - Tablespace�� ID
    [IN] aAttrFlagMask  - Attribute Flag�� Mask
    [IN] aAttrFlagValue - Attribute Flag�� Value
 */
IDE_RC smiTableSpace::alterTBSAttrFlag(smiTrans  * aTrans,
                                       scSpaceID   aTableSpaceID,
                                       UInt        aAttrFlagMask,
                                       UInt        aAttrFlagValue )
{
    IDE_DASSERT( aTrans != NULL );

    // Attribute Mask�� 0�� �� ����.
    IDE_ASSERT( aAttrFlagMask != 0 );
    
    // Attribute Mask���� ��Ʈ��
    // Attribute Value�� ���õ� �� ����.
    IDE_ASSERT( (~aAttrFlagMask & aAttrFlagValue) == 0 );
    
    IDE_TEST( sctTBSAlter::alterTBSAttrFlag(
                               aTrans->getTrans(),
                               aTableSpaceID,
                               aAttrFlagMask,
                               aAttrFlagValue )
              != IDE_SUCCESS );
    
    return IDE_SUCCESS;
    
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * FUNCTION DESCRIPTION : smiTableSpace::addFile()
 *   �� �Լ��� alter tablespace add datafile ..������ ���Ͽ� �Ҹ���,
 *   Ư�� TableSpace�� ����Ÿ ���ϵ��� �߰��Ѵ�.
 **********************************************************************/
IDE_RC smiTableSpace::addDataFile( idvSQL          * aStatistics,
                                   smiTrans*         aTrans,
                                   scSpaceID         aTblSpaceID,
                                   smiDataFileAttr** aDataFileAttr,
                                   UInt              aDataFileAttrCnt)
{
    IDE_DASSERT( aTrans != NULL );
    
    IDE_TEST(sdpTableSpace::createDataFiles(aStatistics,
                                            aTrans->getTrans(),
                                            aTblSpaceID,
                                            aDataFileAttr,
                                            aDataFileAttrCnt)
             != IDE_SUCCESS );
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * FUNCTION DESCRIPTION : smiTableSpace::alterDataFileAutoExtend()
 *   �� �Լ��� alter tablespace modify datafile ���� ���Ͽ�
 *   �Ҹ���,autoextend���� �Ӽ��� �����Ѵ�.
 *   next size, max size, autoextend on/off.
 **********************************************************************/
IDE_RC  smiTableSpace::alterDataFileAutoExtend(smiTrans*   aTrans,
                                               scSpaceID   aSpaceID,
                                               SChar      *aFileName,
                                               idBool      aAutoExtend,
                                               ULong       aNextSize,
                                               ULong       aMaxSize,
                                               SChar*      aValidFileName)
{

    IDE_DASSERT( aFileName != NULL );
    IDE_DASSERT( aFileName != NULL );
    
    IDE_TEST( sdpTableSpace::alterDataFileAutoExtend(NULL, // BUGBUG : �߰� ��� from QP
                                                     aTrans->getTrans(),
                                                     aSpaceID,
                                                     aFileName,
                                                     aAutoExtend,aNextSize,
                                                     aMaxSize,
                                                     aValidFileName)
              != IDE_SUCCESS);
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * FUNCTION DESCRIPTION : smiTableSpace::alterDataFileOnlineMode()
 *   �� �Լ��� alter tablespace modify datafile ���� ���Ͽ�
 *   �Ҹ���,online���� �Ӽ��� �����Ѵ�.
 **********************************************************************/
IDE_RC smiTableSpace::alterDataFileOnLineMode( scSpaceID  /* aSpaceID */,
                                               SChar *    /* aFileName */,
                                               UInt       /* aOnline */)
{
    // datafile  offline /online�� altibase 4������
    // �ǹ̰� ����((BUG-11341).
    IDE_SET(ideSetErrorCode(smERR_ABORT_NotSupport,"DATAFILE ONLINE/OFFLINE"));
    
    return IDE_FAILURE;
}

/***********************************************************************
 * FUNCTION DESCRIPTION : smiTableSpace:resizeDataFileSize()
 *   �� �Լ��� alter tablespace modify datafile.. resize ���� ���Ͽ�
 *   �Ҹ���, Ư�� TableSpace�� ���� ����Ÿ������ ũ�⸦ �����Ѵ�.
 **********************************************************************/
IDE_RC smiTableSpace::resizeDataFile(idvSQL    * aStatistics,
                                     smiTrans  * aTrans,
                                     scSpaceID   aTblSpaceID,
                                     SChar     * aFileName,
                                     ULong       aSizeWanted,
                                     ULong     * aSizeChanged,
                                     SChar     * aValidFileName )
{
    IDE_DASSERT( aTrans != NULL );
    
    IDE_TEST(sdpTableSpace::alterDataFileReSize(aStatistics,
                                                aTrans->getTrans(),
                                                aTblSpaceID,
                                                aFileName,
                                                aSizeWanted,
                                                aSizeChanged,
                                                aValidFileName)
             != IDE_SUCCESS);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * FUNCTION DESCRIPTION : smiTableSpace::renameDataFile()
 * �� �Լ��� alter tablespace rename datafile ���� ���Ͽ�
   �Ҹ���, Ư�� TableSpace�� ���� ������ �̸��� �����Ѵ�.
 **********************************************************************/
IDE_RC smiTableSpace::renameDataFile( scSpaceID  aTblSpaceID,
                                      SChar*     aOldFileName,
                                      SChar*     aNewFileName )
{
    IDE_TEST_RAISE( smiGetStartupPhase() != SMI_STARTUP_CONTROL,
                    err_startup_phase );        

    IDE_TEST(sdpTableSpace::alterDataFileName( NULL, /* idvSQL* */
                                               aTblSpaceID,
                                               aOldFileName,
                                               aNewFileName )
             != IDE_SUCCESS);

    
    return IDE_SUCCESS;

    IDE_EXCEPTION( err_startup_phase );
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_WrongStartupPhase));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * FUNCTION DESCRIPTION : smiTableSpace::removeDataFile()
 *   �� �Լ��� alter tablespace drop datafile���� ���Ͽ�
 *   �Ҹ��� Ư�� TableSpace�� ���� ����Ÿ���� ��带 �����Ѵ�.
 *   !! ������ �������� �ʴ´�.
 **********************************************************************/
IDE_RC smiTableSpace::removeDataFile( smiTrans* aTrans,
                                      scSpaceID aTblSpaceID,
                                      SChar*    aFileName,
                                      SChar*    aValidFileName)
{                                     
    IDE_DASSERT( aTrans != NULL );
    
    IDE_TEST(sdpTableSpace::removeDataFile(NULL, // BUGBUG : �߰� ��� from QP
                                           aTrans->getTrans(),
                                           aTblSpaceID,
                                           aFileName,
                                           aValidFileName)
             != IDE_SUCCESS);
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/* ------------------------------------------------
 *
 * Description :
 *
 * ��� ��θ� �޾Ƽ� ���� ��θ� �����Ѵ�.
 * ��� ��η� ������ ȭ���� ALTIBASE_HOME/dbs ���丮�� ȭ�Ϸ� ��޵ȴ�.
 * Abs : Absolute
 * Rel : Relative
 * space ID�� system, undo, temp�� ���� file�� �̸����� ���� ��
 * �ִ� �� �����ϱ� ���� �ʿ��ϴ� user ���̺����̽��� datafile��
 * �ý��� ���� �� ȭ���� ����� �� ����.
 * 
 * tablespace ID�� �� �� ���� ��쿡
 * �� �Լ��� ����ϱ� ���ؼ��� 
 * SMI_ID_TABLESPACE_SYSTEM_TEMP���� ū ���� �ѱ�� �ȴ�.
 *
 * BUG-29812
 * getAbsPath �Լ��� Memory/Disk TBS���� ��� ����� �� �ֵ��� �����Ѵ�.
 *
 * Memory TBS�� ��� �� ���� ���ۿ� ����θ� ���ڷ� �Ѱ��ְ�,
 * �����θ� �ޱ� ������ aRelName�� aAbsName�� ������ �ּ��̴�.
 * 
 * ----------------------------------------------*/
IDE_RC smiTableSpace::getAbsPath( SChar         * aRelName, 
                                  SChar         * aAbsName,
                                  smiTBSLocation  aTBSLocation )
{                                     
    UInt   sNameSize;
    
    IDE_DASSERT( aRelName != NULL );
    IDE_DASSERT( aAbsName != NULL );

    if( aRelName != aAbsName )
    {
        idlOS::strcpy( aAbsName,
                       aRelName );
    }
    
    sNameSize = idlOS::strlen(aAbsName);
 

    IDE_TEST( sctTableSpaceMgr::makeValidABSPath( 
                                ID_TRUE,
                                aAbsName,
                                &sNameSize,
                                aTBSLocation )
              != IDE_SUCCESS );

    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/* ------------------------------------------------
 *
 * Description :
 * tablespace id�� tablespace�� ã��
 * �� �Ӽ��� ��ȯ�Ѵ�.
 *
 * ----------------------------------------------*/
IDE_RC smiTableSpace::getAttrByID( scSpaceID           aTableSpaceID,
                                   smiTableSpaceAttr * aTBSAttr )
{                                     
    
    IDE_DASSERT( aTBSAttr != NULL );

    IDE_TEST( sctTableSpaceMgr::getTBSAttrByID( NULL,
                                                aTableSpaceID,
                                                aTBSAttr )
              != IDE_SUCCESS );
    
    return IDE_SUCCESS;
    
    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

/* ------------------------------------------------
 *
 * Description :
 * tablespace name���� tablespace�� ã��
 * �� �Ӽ��� ��ȯ�Ѵ�.
 * ----------------------------------------------*/
IDE_RC smiTableSpace::getAttrByName( SChar             * aTableSpaceName,
                                     smiTableSpaceAttr * aTBSAttr )
{
    IDE_DASSERT( aTableSpaceName != NULL );
    IDE_DASSERT( aTBSAttr != NULL );
    
    IDE_TEST( sctTableSpaceMgr::getTBSAttrByName(aTableSpaceName,
                                                 aTBSAttr)
              != IDE_SUCCESS );

    return IDE_SUCCESS;
    
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/* ------------------------------------------------
 *
 * Description :
 *
 * tablespace id�� datafile�� �̸��� �޾�
 * �ش� datafile�� �����ϴ��� �˻��Ѵ�.
 * ----------------------------------------------*/
IDE_RC smiTableSpace::existDataFile( scSpaceID         aTableSpaceID,
                                     SChar*            aDataFileName,
                                     idBool*           aExist)
{                                     
    IDE_DASSERT( aDataFileName != NULL );
    IDE_DASSERT( aExist != NULL );
    
    IDE_TEST( sddDiskMgr::existDataFile(NULL, /* idvSQL* */
                                        aTableSpaceID,
                                        aDataFileName,
                                        aExist)
              != IDE_SUCCESS );

    
    return IDE_SUCCESS;
    
    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

/* ------------------------------------------------
 *
 * Description :
 *
 * ��� ���̺����̽��� ���� datafile��
 * �̹� �����ϴ��� �����ϴ��� �˻��Ѵ�.
 * ----------------------------------------------*/
IDE_RC smiTableSpace::existDataFile( SChar*            aDataFileName,
                                     idBool*           aExist)
{                                     
    IDE_DASSERT( aDataFileName != NULL );
    IDE_DASSERT( aExist != NULL );

    IDE_TEST( sddDiskMgr::existDataFile(aDataFileName,
                                        aExist)
              != IDE_SUCCESS );

    
    return IDE_SUCCESS;
    
    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

// fix BUG-13646
IDE_RC smiTableSpace::getExtentAnTotalPageCnt(scSpaceID  aTableSpaceID,
                                              UInt*      aExtPageCount,
                                              ULong*     aTotalPageCount)
{
    return sddDiskMgr::getExtentAnTotalPageCnt(NULL, /* idvSQL* */
                                               aTableSpaceID,
                                               aExtPageCount,
                                               aTotalPageCount);
}
/*
 * �ý��� ���̺����̽� ���� ��ȯ
 *
 * [IN] aSpaceID - Tablespace�� ID
 */
idBool smiTableSpace::isSystemTableSpace( scSpaceID aSpaceID )
{
    return sctTableSpaceMgr::isSystemTableSpace( aSpaceID );
}

idBool smiTableSpace::isMemTableSpace( scSpaceID aTableSpaceID )
{
    return sctTableSpaceMgr::isMemTableSpace( aTableSpaceID );
}

idBool smiTableSpace::isVolatileTableSpace( scSpaceID aTableSpaceID )
{
    return sctTableSpaceMgr::isVolatileTableSpace( aTableSpaceID );
}

idBool smiTableSpace::isDiskTableSpace( scSpaceID  aTableSpaceID )
{
    return sctTableSpaceMgr::isDiskTableSpace( aTableSpaceID );
}

idBool smiTableSpace::isTempTableSpace( scSpaceID  aTableSpaceID )
{
    return sctTableSpaceMgr::isTempTableSpace( aTableSpaceID );
}

idBool smiTableSpace::isMemTableSpaceType( smiTableSpaceType  aType )
{
    return sctTableSpaceMgr::isMemTableSpaceType( aType );
}

idBool smiTableSpace::isVolatileTableSpaceType( smiTableSpaceType aType )
{
    return sctTableSpaceMgr::isVolatileTableSpaceType( aType );
}

idBool smiTableSpace::isDiskTableSpaceType( smiTableSpaceType  aType )
{
    return sctTableSpaceMgr::isDiskTableSpaceType( aType );
}

idBool smiTableSpace::isTempTableSpaceType( smiTableSpaceType  aType )
{
    return sctTableSpaceMgr::isTempTableSpaceType( aType );
}

idBool smiTableSpace::isDataTableSpaceType( smiTableSpaceType  aType )
{
    return sctTableSpaceMgr::isDataTableSpaceType( aType );
}

/*
    ALTER TABLESPACE TBSNAME ADD CHECKPOINT PATH ... �� ����

    [IN] aSpaceID   - Tablespace ID
    [IN] aChkptPath - �߰��� Checkpoint Path
*/
IDE_RC  smiTableSpace::alterMemoryTBSAddChkptPath( scSpaceID      aSpaceID,
                                                   SChar        * aChkptPath )
{
    IDE_DASSERT( aChkptPath != NULL );

    // �ٴܰ�startup�ܰ��� control �ܰ迡���� �Ҹ��� �ִ�.
    IDE_TEST_RAISE(smiGetStartupPhase() != SMI_STARTUP_CONTROL,
                   err_startup_phase);

    IDE_TEST( smmTBSAlterChkptPath::alterTBSaddChkptPath(
                                 aSpaceID,
                                 aChkptPath ) != IDE_SUCCESS );
        
    return IDE_SUCCESS;

    IDE_EXCEPTION(err_startup_phase);
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_INVALID_STARTUP_PHASE_NOT_CONTROL,
                                "ALTER TABLESPACE ADD CHECKPOINT PATH"));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*
    ALTER TABLESPACE TBSNAME RENAME CHECKPOINT PATH ... �� ����

    [IN] aSpaceID      - Tablespace ID
    [IN] aOrgChkptPath - ������ Checkpoint Path
    [IN] aNEwChkptPath - ������ Checkpoint Path
*/
IDE_RC  smiTableSpace::alterMemoryTBSRenameChkptPath( scSpaceID   aSpaceID,
                                                      SChar     * aOrgChkptPath,
                                                      SChar     * aNewChkptPath )
{
    IDE_DASSERT( aOrgChkptPath != NULL );
    IDE_DASSERT( aNewChkptPath != NULL );
    
    // �ٴܰ�startup�ܰ��� control �ܰ迡���� �Ҹ��� �ִ�.
    IDE_TEST_RAISE(smiGetStartupPhase() != SMI_STARTUP_CONTROL,
                   err_startup_phase);
    
    IDE_TEST( smmTBSAlterChkptPath::alterTBSrenameChkptPath(
                                 aSpaceID,
                                 aOrgChkptPath,
                                 aNewChkptPath ) != IDE_SUCCESS );
    return IDE_SUCCESS;

    IDE_EXCEPTION(err_startup_phase);
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_INVALID_STARTUP_PHASE_NOT_CONTROL,
                                "ALTER TABLESPACE RENAME CHECKPOINT PATH"));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/*
    ALTER TABLESPACE TBSNAME DROP CHECKPOINT PATH ... �� ����

    [IN] aSpaceID   - Tablespace ID
    [IN] aChkptPath - ������ Checkpoint Path
*/
IDE_RC  smiTableSpace::alterMemoryTBSDropChkptPath( scSpaceID      aSpaceID,
                                                    SChar        * aChkptPath )
{
    IDE_DASSERT( aChkptPath != NULL );

    // �ٴܰ�startup�ܰ��� control �ܰ迡���� �Ҹ��� �ִ�.
    IDE_TEST_RAISE(smiGetStartupPhase() != SMI_STARTUP_CONTROL,
                   err_startup_phase);
    
    IDE_TEST( smmTBSAlterChkptPath::alterTBSdropChkptPath(
                                 aSpaceID,
                                 aChkptPath ) != IDE_SUCCESS );
    
    return IDE_SUCCESS;

    IDE_EXCEPTION(err_startup_phase);
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_INVALID_STARTUP_PHASE_NOT_CONTROL,
                                "ALTER TABLESPACE DROP CHECKPOINT PATH"));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}



/*
    ALTER TABLESPACE TBSNAME AUTOEXTEND ... �� �����Ѵ� 

   ���� �� �ִ� ���� ����:
     - ALTER TABLESPACE TBSNAME AUTOEXTEND OFF
     - ALTER TABLESPACE TBSNAME AUTOEXTEND ON NEXT 10M
     - ALTER TABLESPACE TBSNAME AUTOEXTEND ON MAXSIZE 10M/UNLIMITTED
     - ALTER TABLESPACE TBSNAME AUTOEXTEND ON NEXT 10M MAXSIZE 10M/UNLIMITTED
    
*/
IDE_RC  smiTableSpace::alterMemoryTBSAutoExtend(smiTrans*   aTrans,
                                                scSpaceID   aSpaceID,
                                                idBool      aAutoExtend,
                                                ULong       aNextSize,
                                                ULong       aMaxSize )
{
    IDE_DASSERT( aTrans != NULL );

    // dictionary tablespace�� alter autoextend�Ҽ� ����.
    IDE_TEST_RAISE( aSpaceID == SMI_ID_TABLESPACE_SYSTEM_MEMORY_DIC,
                    error_alter_autoextend_dictionary_tablespace);
    
    // ALTER TABLESPACE TBSNAME AUTOEXTEND ON/OFF
    IDE_TEST( smmTBSAlterAutoExtend::alterTBSsetAutoExtend( aTrans->getTrans(),
                                                    aSpaceID,
                                                    aAutoExtend,
                                                    aNextSize,
                                                    aMaxSize)
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    
    IDE_EXCEPTION(error_alter_autoextend_dictionary_tablespace);
    {
        IDE_SET(ideSetErrorCode(
                   smERR_ABORT_CANNOT_ALTER_AUTOEXTEND_DICTIONARY_TABLESPACE));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*  PROJ-1594 Volatile TBS
    ALTER TABLESPACE TBSNAME AUTOEXTEND ... �� �����Ѵ�
*/
IDE_RC  smiTableSpace::alterVolatileTBSAutoExtend(smiTrans*   aTrans,
                                                  scSpaceID   aSpaceID,
                                                  idBool      aAutoExtend,
                                                  ULong       aNextSize,
                                                  ULong       aMaxSize )
{
    IDE_DASSERT( aTrans != NULL );

    // ALTER TABLESPACE TBSNAME AUTOEXTEND ON/OFF
    IDE_TEST( svmTBSAlterAutoExtend::alterTBSsetAutoExtend( aTrans->getTrans(),
                                                    aSpaceID,
                                                    aAutoExtend,
                                                    aNextSize,
                                                    aMaxSize)
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * FUNCTION DESCRIPTION : smiTableSpace::alterDiscard
 *   ALTER TABLESPACE DISCARD�� ���� 
 *
 *   �� �Լ��� Ư�� tablespace�� disacrd ���·� �����Ѵ�.
 *   startup control �ܰ迡���� �����ϴ�. 
 *
 *   DISCARD�� Tablespace�� ����� �� ���� �Ǹ� ���� Drop�� �����ϴ�.
 *   �� ���� Table, Data���� Drop�� �����ϴ�.
 *
 *   ���ü� ���� - CONTROL�ܰ迡�� ȣ��ǹǷ� �ʿ����� �ʴ�
 *
 * [IN] aTableSpaceID - ���¸� �����Ϸ��� Tablespace�� ID
 **********************************************************************/
IDE_RC smiTableSpace::alterDiscard( scSpaceID aTableSpaceID )
{
    sctTableSpaceNode * sTBSNode;

    IDE_TEST_RAISE( smiGetStartupPhase() != SMI_STARTUP_CONTROL,
                    err_startup_phase );        
    
    // system tablespace�� online/offline�Ҽ� ����.
    IDE_TEST_RAISE( aTableSpaceID <= SMI_ID_TABLESPACE_SYSTEM_DISK_TEMP,
                    error_discard_system_tablespace);
    
    // TBSID�� TBSNode�� �˾Ƴ���.
    IDE_TEST( sctTableSpaceMgr::findSpaceNodeBySpaceID(  
                                    aTableSpaceID,
                                    (void**) & sTBSNode )
              != IDE_SUCCESS );

    switch ( sTBSNode->mType )
    {
        case SMI_MEMORY_USER_DATA:
            IDE_TEST( smmTBSAlterDiscard::alterTBSdiscard(
                          (smmTBSNode*) sTBSNode )
                      != IDE_SUCCESS );                
            break;
            
        case SMI_DISK_USER_DATA:
        case SMI_DISK_USER_TEMP:
            IDE_TEST( sdpTableSpace::alterTBSdiscard(
                          ( sddTableSpaceNode *) sTBSNode )
                      != IDE_SUCCESS );                
            break;
        case SMI_MEMORY_SYSTEM_DICTIONARY :
        case SMI_MEMORY_SYSTEM_DATA:
        case SMI_DISK_SYSTEM_TEMP:
        case SMI_DISK_SYSTEM_UNDO:
        case SMI_DISK_SYSTEM_DATA:
        default :
            // ������ Discard�Ұ����� TBS���� üũ�Ͽ����Ƿ�
            // ���⿡���� Discard�Ұ����� TBS�� ���� �� ����
            IDE_ASSERT(0);
            break;
            
    }
    
    return IDE_SUCCESS;
    
    IDE_EXCEPTION( err_startup_phase );
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_WrongStartupPhase));
    }
    IDE_EXCEPTION(error_discard_system_tablespace);
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_CannotDiscardTableSpace));
    }
    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

 
ULong smiTableSpace::getSysDataTBSExtentSize()
{
    return smuProperty::getSysDataTBSExtentSize();
}

// BUG-14897 - �Ӽ� SYS_DATA_FILE_INIT_SIZE �� ����.
ULong smiTableSpace::getSysDataFileInitSize()
{
    return getValidSize4Disk( smuProperty::getSysDataFileInitSize() );
}

// BUG-14897 - �Ӽ� SYS_DATA_FILE_MAX_SIZE �� ����.
ULong smiTableSpace::getSysDataFileMaxSize()
{
    return getValidSize4Disk( smuProperty::getSysDataFileMaxSize() );
}

// BUG-14897 - �Ӽ� SYS_DATA_FILE_NEXT_SIZE �� ����.
ULong smiTableSpace::getSysDataFileNextSize()
{
    return smuProperty::getSysDataFileNextSize();
}

// BUG-14897 - �Ӽ� SYS_UNDO_TBS_EXTENT_SIZE �� ����.
ULong smiTableSpace::getSysUndoTBSExtentSize()
{
    return smuProperty::getSysUndoTBSExtentSize();
}

// BUG-14897 - �Ӽ� SYS_UNDO_FILE_INIT_SIZE �� ����.
ULong smiTableSpace::getSysUndoFileInitSize()
{
    return getValidSize4Disk( smuProperty::getSysUndoFileInitSize() );
}

// BUG-14897 - �Ӽ� SYS_UNDO_FILE_MAX_SIZE �� ����.
ULong smiTableSpace::getSysUndoFileMaxSize()
{
    return getValidSize4Disk( smuProperty::getSysUndoFileMaxSize() );
}

// BUG-14897 - �Ӽ� SYS_UNDO_FILE_NEXT_SIZE �� ����.
ULong smiTableSpace::getSysUndoFileNextSize()
{
    return smuProperty::getSysUndoFileNextSize();
}

// BUG-14897 - �Ӽ� SYS_TEMP_TBS_EXTENT_SIZE �� ����.
ULong smiTableSpace::getSysTempTBSExtentSize()
{
    return smuProperty::getSysTempTBSExtentSize();
}

// BUG-14897 - �Ӽ� SYS_TEMP_FILE_INIT_SIZE �� ����.
ULong smiTableSpace::getSysTempFileInitSize()
{
    return getValidSize4Disk( smuProperty::getSysTempFileInitSize() );
}

// BUG-14897 - �Ӽ� SYS_TEMP_FILE_MAX_SIZE �� ����.
ULong smiTableSpace::getSysTempFileMaxSize()
{
    return getValidSize4Disk( smuProperty::getSysTempFileMaxSize() );
}

// BUG-14897 - �Ӽ� SYS_TEMP_FILE_NEXT_SIZE �� ����.
ULong smiTableSpace::getSysTempFileNextSize()
{
    return smuProperty::getSysTempFileNextSize();
}

// BUG-14897 - �Ӽ� USER_DATA_TBS_EXTENT_SIZE �� ����.
ULong smiTableSpace::getUserDataTBSExtentSize()
{
    return smuProperty::getUserDataTBSExtentSize();
}

// BUG-14897 - �Ӽ� USER_DATA_FILE_INIT_SIZE �� ����.
ULong smiTableSpace::getUserDataFileInitSize()
{
    return getValidSize4Disk( smuProperty::getUserDataFileInitSize() );
}

// BUG-14897 - �Ӽ� USER_DATA_FILE_MAX_SIZE �� ����.
ULong smiTableSpace::getUserDataFileMaxSize()
{
    return getValidSize4Disk( smuProperty::getUserDataFileMaxSize() );
}

// BUG-14897 - �Ӽ� USER_DATA_FILE_NEXT_SIZE �� ����.
ULong smiTableSpace::getUserDataFileNextSize()
{
    return smuProperty::getUserDataFileNextSize();
}

// BUG-14897 - �Ӽ� USER_TEMP_TBS_EXTENT_SIZE �� ����.
ULong smiTableSpace::getUserTempTBSExtentSize()
{
    return smuProperty::getUserTempTBSExtentSize();
}

// BUG-14897 - �Ӽ� USER_TEMP_FILE_INIT_SIZE �� ����.
ULong smiTableSpace::getUserTempFileInitSize()
{
    return getValidSize4Disk( smuProperty::getUserTempFileInitSize() );
}

// BUG-14897 - �Ӽ� USER_TEMP_FILE_MAX_SIZE �� ����.
ULong smiTableSpace::getUserTempFileMaxSize()
{
    return getValidSize4Disk( smuProperty::getUserTempFileMaxSize() );
}

// BUG-14897 - �Ӽ� USER_TEMP_FILE_NEXT_SIZE �� ����.
ULong smiTableSpace::getUserTempFileNextSize()
{
    return smuProperty::getUserTempFileNextSize();
}

/**********************************************************************
 * Description:OS limit,file header�� ����� ������ ũ�⸦ ������ ��.
 *
 * aSizeInBytes       - [IN]   ����Ʈ������ ũ�� 
 **********************************************************************/
ULong  smiTableSpace::getValidSize4Disk( ULong aSizeInBytes )
{
    ULong sPageCount = aSizeInBytes / SD_PAGE_SIZE ;
    ULong sRet;
    UInt  sFileHdrSizeInBytes;
    UInt  sFileHdrPageCnt;

    // BUG-27911 file header ũ�⸦ define ���ǹ����� ��ü.
    sFileHdrSizeInBytes = idlOS::align( SM_DBFILE_METAHDR_PAGE_SIZE, SD_PAGE_SIZE );
    sFileHdrPageCnt = sFileHdrSizeInBytes / SD_PAGE_SIZE;

    if( ( sPageCount + sFileHdrPageCnt ) > sddDiskMgr::getMaxDataFileSize() )
    {
        sRet = (sddDiskMgr::getMaxDataFileSize() - sFileHdrPageCnt) * SD_PAGE_SIZE;
    }
    else
    {
        sRet = aSizeInBytes;
    }
    
    return sRet;
}
