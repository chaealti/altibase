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
 * $Id: qdtCommon.cpp 82075 2018-01-17 06:39:52Z jina.kim $
 **********************************************************************/

#include <idl.h>
#include <ide.h>
#include <smiDef.h>
#include <smiTableSpace.h>
#include <qdtCommon.h>
#include <qtc.h>
#include <qcm.h>
#include <qmc.h>
#include <qcmTableSpace.h>
#include <qcuProperty.h>
#include <qcuSqlSourceInfo.h>
#include <qdpRole.h>

IDE_RC qdtCommon::validateFilesSpec(
        qcStatement       * /* aStatement */,
        smiTableSpaceType   aType,
        qdTBSFilesSpec    * aFilesSpec,
        ULong               aExtentSize,
        UInt              * aDataFileCount)
{
/***********************************************************************
 *
 * Description :
 *    file specification validation �Լ�
 *
 * Implementation :
 *    for (each file spec)
 *    {
 *    1.1 SIZE �� ������� ���� ��� �⺻ ũ�� 100M �� �Ľ�Ʈ���� ����
 *    1.2 AUTOEXTEND ON �� ����� ���
 *      1.2.1 NEXT size �� ������� ���� ��� size of extent * n ������
 *              �����Ѵ�. N ���� ������Ƽ ���Ͽ� ���ǵ� ���� �о�´�.
 *      1.2.2 MAXSIZE �� ������� ���� ��� : Property ������ ����
 *            MAXSIZE�� ����� ��� : ����� ������ ����
 *            MAXSIZE�� UNLIMITED �� ��� : MAXSIZE = UNLIMITED ����
 *    1.3 AUTOEXTEND OFF ����� ���
 *      1.3.1 MAXSIZE = 0 ����
 *    1.4 AUTOEXTEND ������� ���� ��� OFF ���� �⺻��
 *    }
 *    2 file ���� counting �� ���� smiTableSpaceAttr->mDataFileCount �� ����
 *
 ***********************************************************************/

    UInt                sFileCnt;
    qdTBSFilesSpec    * sFilesSpec;
    smiDataFileAttr   * sFileAttr;
    ULong               sNextSize;
    ULong               sMaxSize;
    ULong               sInitSize;
    ULong               sDiskMaxDBSize;
    ULong               sNewDiskSize;
    ULong               sCurTotalDiskDBPageCnt;
    idBool              sIsSizeUnlimited;

    sNewDiskSize   = 0;
    sIsSizeUnlimited   = ID_FALSE;

    sFileCnt = 0;
    for ( sFilesSpec = aFilesSpec;
          sFilesSpec != NULL;
          sFilesSpec = sFilesSpec->next )
    {
        sFileAttr = sFilesSpec->fileAttr;

        // BUG-32255
        // If nextsize was not defined, it is defined to property value.
        if ( sFileAttr->mNextSize == ID_ULONG_MAX )
        {
            switch( aType )
            {
                case SMI_DISK_SYSTEM_DATA:
                {
                    sFileAttr->mNextSize =
                        smiTableSpace::getSysDataFileNextSize();
                    break;
                }
                case SMI_DISK_USER_DATA:
                {
                    sFileAttr->mNextSize =
                        smiTableSpace::getUserDataFileNextSize();
                    break;
                }
                case SMI_DISK_SYSTEM_TEMP:
                {
                    sFileAttr->mNextSize =
                        smiTableSpace::getSysTempFileNextSize();
                    break;
                }
                case SMI_DISK_USER_TEMP:
                {
                    sFileAttr->mNextSize =
                        smiTableSpace::getUserTempFileNextSize();
                    break;
                }
                case SMI_DISK_SYSTEM_UNDO:
                {
                    sFileAttr->mNextSize =
                        smiTableSpace::getSysUndoFileNextSize();
                    break;
                }
                default:
                {
                    IDE_ASSERT(0);
                }
            }
        }

        IDE_TEST_RAISE( sFileAttr->mIsAutoExtend == ID_TRUE &&
                        sFileAttr->mNextSize < aExtentSize,
                        ERR_INVALID_NEXT_SIZE );

        if ( sFileAttr->mMaxSize == ID_ULONG_MAX )
        {
            // Max Size�� ������� ���� ���, Property�� ���� ������
            switch( aType )
            {
                case SMI_DISK_SYSTEM_DATA:
                {
                    sFileAttr->mMaxSize =
                        smiTableSpace::getSysDataFileMaxSize();
                    break;
                }
                case SMI_DISK_USER_DATA:
                {
                    sFileAttr->mMaxSize =
                        smiTableSpace::getUserDataFileMaxSize();
                    break;
                }
                case SMI_DISK_SYSTEM_TEMP:
                {
                    sFileAttr->mMaxSize =
                        smiTableSpace::getSysTempFileMaxSize();
                    break;
                }
                case SMI_DISK_USER_TEMP:
                {
                    sFileAttr->mMaxSize =
                        smiTableSpace::getUserTempFileMaxSize();
                    break;
                }
                case SMI_DISK_SYSTEM_UNDO:
                {
                    sFileAttr->mMaxSize =
                        smiTableSpace::getSysUndoFileMaxSize();
                    break;
                }
                default:
                {
                    IDE_ASSERT(0);
                }
            }
        }
        else
        {
            // Max Size�� ����� ��� : ����� ������ ������ 
            // unlimited �� ����� ��� : mMaxSize�� 0��
        }

        //------------------------------------------------
        // To Fix BUG-10415
        // Init Size�� �־����� �ʾ��� ���, property���� init size��
        // �о�;���
        //------------------------------------------------
        
        if ( sFileAttr->mInitSize == 0 )
        {
            // Init Size�� �־����� �ʾ��� ���,
            // Property ���� �о�� Init Size �� ������
            switch( aType )
            {
                case SMI_DISK_SYSTEM_DATA:
                {
                    sFileAttr->mInitSize =
                        smiTableSpace::getSysDataFileInitSize();
                    break;
                }
                case SMI_DISK_USER_DATA:
                {
                    sFileAttr->mInitSize =
                        smiTableSpace::getUserDataFileInitSize();
                    break;
                }
                case SMI_DISK_SYSTEM_TEMP:
                {
                    sFileAttr->mInitSize =
                        smiTableSpace::getSysTempFileInitSize();
                    break;
                }
                case SMI_DISK_USER_TEMP:
                {
                    sFileAttr->mInitSize =
                        smiTableSpace::getUserTempFileInitSize();
                    break;
                }
                case SMI_DISK_SYSTEM_UNDO:
                {
                    sFileAttr->mInitSize =
                        smiTableSpace::getSysUndoFileInitSize();
                    break;
                }
                default:
                {
                    IDE_ASSERT(0);
                }
            }
        }
        else
        {
            // nothing to do
        }
        
        // To Fix BUG-10378
        sNextSize =  sFileAttr->mNextSize / (ULong)smiGetPageSize(aType);
        sNextSize = ( sNextSize < 1 ) ? 1 : sNextSize;
        sFileAttr->mNextSize = sNextSize;

        // To Fix BUG-10501
        sMaxSize =  sFileAttr->mMaxSize;
        if ( sMaxSize != 0 )
        {
            //------------------------------------------------
            // MaxSize�� 0�� �ƴ� ���
            // - Max Size�� ��õ� ���
            // - Max Size�� ��õ��� �ʾ� Property������ ������ ���
            //------------------------------------------------
            
            sMaxSize =  sMaxSize / (ULong)smiGetPageSize(aType);
            sMaxSize = ( sMaxSize < 1 ) ? 1 : sMaxSize;
            sFileAttr->mMaxSize = sMaxSize;
        }
        else
        {
            //------------------------------------------------
            // Max Size�� 0�� ���
            // - Auto Extend Off �� ���
            // - Max Size�� unlimited�� ������ �����
            //------------------------------------------------
        }

        sInitSize =  sFileAttr->mInitSize / (ULong)smiGetPageSize(aType);
        sInitSize = ( sInitSize < 1 ) ? 1 : sInitSize;
        sFileAttr->mInitSize = sInitSize;
        
        sFileAttr->mCurrSize = sFileAttr->mInitSize;
        sFileCnt++;

        if (sFileAttr->mIsAutoExtend == ID_TRUE)
        {
            if (sFileAttr->mMaxSize == 0)
            {
                sIsSizeUnlimited = ID_TRUE;
            }
            else
            {
                sNewDiskSize += sFileAttr->mMaxSize;
            }
        }
        else
        {
            sNewDiskSize += sFileAttr->mInitSize;
        }
    }
    *aDataFileCount = sFileCnt;

    sDiskMaxDBSize = iduProperty::getDiskMaxDBSize();

    if (sDiskMaxDBSize != ID_ULONG_MAX)
    {
        /*
         * TASK-6327 Community Edition License
         * ���� �����ϴ� tablespace �� ���Ͽ�
         * �� maxsize �� DISK_MAX_DB_SIZE �� �Ѿ ��� ����
         */
        sCurTotalDiskDBPageCnt = smiGetDiskDBFullSize();

        IDE_TEST_RAISE((sIsSizeUnlimited == ID_TRUE) ||
                       (sCurTotalDiskDBPageCnt + sNewDiskSize >
                        sDiskMaxDBSize / (ULong)smiGetPageSize(aType)),
                       ERR_DB_MAXSIZE_EXCEED);
    }
    else
    {
        /* DISK_MAX_DB_SIZE = unlimited */
        /* nothing to do */
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_INVALID_NEXT_SIZE);
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QDT_INVALID_NEXT_SIZE));
    }
    IDE_EXCEPTION(ERR_DB_MAXSIZE_EXCEED)
    {
        // BUG-44570
        // DISK_MAX_DB_SIZE �ʰ��� ���� �߻��� ����ڿ��� �ΰ����� ������ �˷��ֵ��� �մϴ�.
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QDT_DB_MAXSIZE_EXCEED,
                                (sDiskMaxDBSize/1024/1024),
                                ((sDiskMaxDBSize - (sCurTotalDiskDBPageCnt*smiGetPageSize(aType)))/1024/1024)));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qdtCommon::getAndValidateIndexTBS( qcStatement       * aStatement,
                                   scSpaceID           aTableTBSID,
                                   smiTableSpaceType   aTableTBSType,
                                   qcNamePosition      aIndexTBSName,
                                   UInt                aIndexOwnerID,
                                   scSpaceID         * aIndexTBSID,
                                   smiTableSpaceType * aIndexTBSType )
{
/***********************************************************************
 *
 * Description :
 *     INDEX TABLESPACE tbs_name�� ���� Validation�� �ϰ�
 *     Index Tablespace�� ID�� Type�� ȹ���Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qdtCommon::getAndValidateIndexTBS"

    smiTableSpaceAttr     sIndexTBSAttr;
    
    //-----------------------------------------
    // ���ռ� �˻�
    //-----------------------------------------
    
    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aIndexTBSID != NULL );
    IDE_DASSERT( aIndexTBSType != NULL );

    if (QC_IS_NULL_NAME( aIndexTBSName ) != ID_TRUE)
    {
        IDE_TEST( qcmTablespace::getTBSAttrByName(
                      aStatement,
                      aIndexTBSName.stmtText + aIndexTBSName.offset,
                      aIndexTBSName.size,
                      & sIndexTBSAttr ) != IDE_SUCCESS );

        IDE_TEST_RAISE( sIndexTBSAttr.mType == SMI_DISK_SYSTEM_TEMP ||
                        sIndexTBSAttr.mType == SMI_DISK_USER_TEMP   ||
                        sIndexTBSAttr.mType == SMI_DISK_SYSTEM_UNDO,
                        ERR_NO_CREATE_IN_SYSTEM_TBS );

        IDE_TEST( qdpRole::checkAccessTBS( aStatement,
                                           aIndexOwnerID,
                                           sIndexTBSAttr.mID)
                  != IDE_SUCCESS );

        IDE_TEST_RAISE( sIndexTBSAttr.mType == SMI_MEMORY_SYSTEM_DICTIONARY,
            ERR_INVALID_INDEX_TABLESPACE_MEDIA );

        // To Fix PR-9770
        // Memory Table�� Disk Index�� �Ǵ�
        // Disk Table�� Memory Index�� ������ �� ����.
        // ���� ��ü�� �������� �˻��Ͽ� ���� ��ü�� �ٸ��� ����
        IDE_TEST_RAISE( isSameTBSType( aTableTBSType,
                                       sIndexTBSAttr.mType ) == ID_FALSE,
                        ERR_INVALID_INDEX_TABLESPACE_MEDIA );

        *aIndexTBSID   = sIndexTBSAttr.mID;
        *aIndexTBSType = sIndexTBSAttr.mType;
    }
    else // TABLESPACENAME ������� ���� ���
    {
        *aIndexTBSID   = aTableTBSID;
        *aIndexTBSType = aTableTBSType;
    }
    
    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_NO_CREATE_IN_SYSTEM_TBS)
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QDT_NO_CREATE_IN_SYSTEM_TBS));
    }
    IDE_EXCEPTION(ERR_INVALID_INDEX_TABLESPACE_MEDIA);
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QDX_INVALID_INDEX_MEDIA));
    }
    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
    
#undef IDE_FN
}

IDE_RC
qdtCommon::getAndValidateTBSOfIndexPartition( 
    qcStatement       * aStatement,
    scSpaceID           aTablePartTBSID,
    smiTableSpaceType   aTablePartTBSType,
    qcNamePosition      aIndexTBSName,
    UInt                aIndexOwnerID,
    scSpaceID         * aIndexTBSID,
    smiTableSpaceType * aIndexTBSType )
{
/***********************************************************************
 *
 * Description :
 *      PROJ-1502 PARTITIONED DISK TABLE
 *
 *    �� ���̺� ��Ƽ���� TABLESPACE�� ���� validation
 *
 * Implementation :
 *    1. TABLESPACE ����� ���
 *      1.1 SM ���� �����ϴ� ���̺����̽����� üũ
 *      1.2 ���̺����̽��� ������ UNDO tablespace �Ǵ� 
 *          temporary tablespace �̸� ����
 *
 *    2. TABLESPACE ������� ���� ���
 *      2.1 USER_ID �� SYS_USERS_ �˻��ؼ� DEFAULT_TBS_ID ���� �о� ���̺���
 *          ���� ���̺����̽��� ����
 *
 *
 ***********************************************************************/

    smiTableSpaceAttr     sIndexTBSAttr;
    
    //-----------------------------------------
    // ���ռ� �˻�
    //-----------------------------------------
    
    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aIndexTBSID != NULL );
    IDE_DASSERT( aIndexTBSType != NULL );

    //-----------------------------------------
    // 1. TABLESPACE ����� ���
    //-----------------------------------------
    if (QC_IS_NULL_NAME( aIndexTBSName ) == ID_FALSE )
    {
        IDE_TEST( qcmTablespace::getTBSAttrByName(
                      aStatement,
                      aIndexTBSName.stmtText + aIndexTBSName.offset,
                      aIndexTBSName.size,
                      & sIndexTBSAttr ) != IDE_SUCCESS );

        IDE_TEST_RAISE( sIndexTBSAttr.mType == SMI_DISK_SYSTEM_TEMP ||
                        sIndexTBSAttr.mType == SMI_DISK_USER_TEMP   ||
                        sIndexTBSAttr.mType == SMI_DISK_SYSTEM_UNDO,
                        ERR_NO_CREATE_IN_SYSTEM_TBS );

        IDE_TEST( qdpRole::checkAccessTBS( aStatement,
                                           aIndexOwnerID,
                                           sIndexTBSAttr.mID)
                  != IDE_SUCCESS );

        // ���̺� ��Ƽ���� TBS�� �ε��� ��Ƽ���� TBS�� ������ �ٸ��� ����
        // ���� ��ü�� �������� �˻��Ͽ� ���� ��ü�� �ٸ��� ����
        IDE_TEST_RAISE( isSameTBSType( aTablePartTBSType,
                                       sIndexTBSAttr.mType ) == ID_FALSE,
                        ERR_INVALID_INDEX_TABLESPACE_MEDIA );

        *aIndexTBSID   = sIndexTBSAttr.mID;
        *aIndexTBSType = sIndexTBSAttr.mType;
    }
    //-----------------------------------------
    // 2. TABLESPACENAME ������� ���� ���
    //    ���̺� ��Ƽ���� TBS�� ������.
    //-----------------------------------------
    else
    {
        *aIndexTBSID   = aTablePartTBSID;
        *aIndexTBSType = aTablePartTBSType;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_NO_CREATE_IN_SYSTEM_TBS)
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QDT_NO_CREATE_IN_SYSTEM_TBS));
    }
    IDE_EXCEPTION(ERR_INVALID_INDEX_TABLESPACE_MEDIA);
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QDX_INVALID_INDEX_MEDIA));
    }
    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

/* ���� ���� Attribute Flag List�� Flag����
   Bitwise Or���� �Ͽ� �ϳ��� UInt ���� Flag ���� �����

   [IN] aAttrFlagList - Tablespace Attribute Flag�� List
   [OUT] aAttrFlag - Bitwise OR�� Flag
*/
IDE_RC qdtCommon::getTBSAttrFlagFromList(qdTBSAttrFlagList * aAttrFlagList,
                                         UInt              * aAttrFlag )
{
    IDE_DASSERT( aAttrFlagList != NULL );
    IDE_DASSERT( aAttrFlag != NULL );

    UInt sAttrFlag = 0;
    
    for ( ; aAttrFlagList != NULL ; aAttrFlagList = aAttrFlagList->next )
    {
        sAttrFlag |= aAttrFlagList->attrValue;
    }

    *aAttrFlag = sAttrFlag;
    
    return IDE_SUCCESS;
}


/*
    Tablespace�� Attribute Flag List�� ���� Validation����

   [IN] qcStatement - Tablespace Attribute�� ���� Statement
   [IN] aAttrFlagList - Tablespace Attribute Flag�� List
 */
IDE_RC qdtCommon::validateTBSAttrFlagList(qcStatement       * aStatement,
                                          qdTBSAttrFlagList * aAttrFlagList)
{
    IDE_DASSERT( aAttrFlagList != NULL );
    
    // ���� �̸��� Attribute Flag�� ���� �ϸ� ���� ó��
    IDE_TEST( checkTBSAttrIsUnique( aStatement,
                                    aAttrFlagList ) != IDE_SUCCESS );

    return IDE_SUCCESS;
    
    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

                                  
/*
    Tablespace�� Attribute Flag List���� ������
    Attribute List�� ������ ��� ����ó��

    ex>  ������ Attribute�� �ٸ� ������ �� �� ������ ����
         COMPRESSED LOGGING UNCOMPRESSED LOGGING 

    Detect�� ��� :
         Mask�� ��ġ�� Attribute�� �����ϴ��� üũ�Ѵ�.

   [IN] qcStatement - Tablespace Attribute�� ���� Statement
   [IN] aAttrFlagList - Tablespace Attribute Flag�� List
 */
IDE_RC qdtCommon::checkTBSAttrIsUnique(qcStatement       * aStatement,
                                       qdTBSAttrFlagList * aAttrFlagList)
{
    IDE_DASSERT( aAttrFlagList != NULL );

    qcuSqlSourceInfo    sqlInfo;
    qdTBSAttrFlagList * sAttrFlag;
    
    for ( ; aAttrFlagList != NULL ; aAttrFlagList = aAttrFlagList->next )
    {
        for ( sAttrFlag = aAttrFlagList->next;
              sAttrFlag != NULL;
              sAttrFlag = sAttrFlag->next )
        {
            if ( aAttrFlagList->attrMask & sAttrFlag->attrMask )
            {
                IDE_RAISE(err_same_duplicate_attribute);
            }
        }
    }
    
    return IDE_SUCCESS;

    IDE_EXCEPTION(err_same_duplicate_attribute);
    {
        sqlInfo.setSourceInfo( aStatement,
                               & sAttrFlag->attrPosition );

        (void)sqlInfo.init(aStatement->qmeMem);
        IDE_SET(
            ideSetErrorCode( qpERR_ABORT_QDT_DUPLICATE_TBS_ATTRIBUTE,
                             sqlInfo.getErrMessage() ));
        (void)sqlInfo.fini();
    }
    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

idBool qdtCommon::isSameTBSType( smiTableSpaceType  aTBSType1,
                                 smiTableSpaceType  aTBSType2 )
{
    idBool  sIsSame = ID_TRUE;
    
    if ( smiTableSpace::isMemTableSpaceType( aTBSType1 ) == ID_TRUE )
    {
        // Memory Table�� ���
        if ( smiTableSpace::isMemTableSpaceType( aTBSType2 ) == ID_FALSE )
        {
            sIsSame = ID_FALSE;
        }
        else
        {
            // Nothing to do.
        }
    }
    else
    {
        if ( smiTableSpace::isVolatileTableSpaceType( aTBSType1 ) == ID_TRUE )
        {
            // Volatile Table�� ���
            if ( smiTableSpace::isVolatileTableSpaceType( aTBSType2 ) == ID_FALSE )
            {
                sIsSame = ID_FALSE;
            }
            else
            {
                // Nothing to do.
            }
        }
        else
        {
            // Disk Table�� ���
            IDE_DASSERT( smiTableSpace::isDiskTableSpaceType( aTBSType1 )
                         == ID_TRUE );
        
            if ( smiTableSpace::isDiskTableSpaceType( aTBSType2 ) == ID_FALSE )
            {
                sIsSame = ID_FALSE;
            }
            else
            {
                // Nothing to do.
            }
        }
    }

    return sIsSame;
}
