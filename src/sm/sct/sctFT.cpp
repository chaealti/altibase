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
 * $Id: sctFT.cpp 17358 2006-07-31 03:12:47Z bskim $
 *
 * Description :
 *
 * ���̺����̽� ���� Dump
 *
 * X$TABLESPACES
 * X$TABLESPACES_HEADER
 *
 **********************************************************************/

#include <smDef.h>
#include <sctDef.h>
#include <smErrorCode.h>
#include <idl.h>
#include <ide.h>
#include <idu.h>
#include <smu.h>
#include <sdd.h>
#include <smm.h>
#include <svm.h>
#include <sctReq.h>
#include <sctTableSpaceMgr.h>
#include <sctFT.h>

/* ------------------------------------------------
 *  Fixed Table Define for TableSpace
 *
 * Disk, Memory, Volatile Tablespace�� ����������
 * �����ϴ� Field�� ���� Fixed Table
 *
 * X$TABLESPACES_HEADER
 * ----------------------------------------------*/

static iduFixedTableColDesc gTBSHdrTableColDesc[] =
{
    {
        (SChar*)"ID",
        offsetof(sctTbsHeaderInfo, mSpaceID),
        IDU_FT_SIZEOF(sctTbsHeaderInfo, mSpaceID),
        IDU_FT_TYPE_UINTEGER | IDU_FT_COLUMN_INDEX,
        NULL,
        0, 0,NULL // for internal use
    },
    {
        (SChar*)"NAME",
        offsetof(sctTbsHeaderInfo, mName),
        40,
        IDU_FT_TYPE_VARCHAR | IDU_FT_TYPE_POINTER,
        NULL,
        0, 0,NULL // for internal use
    },
    {
        (SChar*)"TYPE",
        offsetof(sctTbsHeaderInfo, mType),
        IDU_FT_SIZEOF(sctTbsHeaderInfo, mType),
        IDU_FT_TYPE_UINTEGER | IDU_FT_COLUMN_INDEX,
        NULL,
        0, 0,NULL // for internal use
    },
    {
        (SChar*)"STATE",
        offsetof(sctTbsHeaderInfo, mStateName),
        IDU_FT_SIZEOF(sctTbsHeaderInfo, mStateName),
        IDU_FT_TYPE_VARCHAR,
        NULL,
        0, 0,NULL // for internal use
    },
    {
        (SChar*)"STATE_BITSET",
        offsetof(sctTbsHeaderInfo, mStateBitset),
        IDU_FT_SIZEOF(sctTbsHeaderInfo, mStateBitset),
        IDU_FT_TYPE_UINTEGER,
        NULL,
        0, 0,NULL // for internal use
    },
    {
        (SChar*)"ATTR_LOG_COMPRESS",
        offsetof(sctTbsHeaderInfo, mAttrLogCompress),
        IDU_FT_SIZEOF(sctTbsHeaderInfo, mAttrLogCompress),
        IDU_FT_TYPE_UINTEGER,
        NULL,
        0, 0,NULL // for internal use
    },
    {
        NULL,
        0,
        0,
        IDU_FT_TYPE_CHAR,
        NULL,
        0, 0,NULL // for internal use
    }
};

iduFixedTableDesc gTbsHeaderTableDesc =
{
    (SChar *)"X$TABLESPACES_HEADER",
    sctFT::buildRecordForTableSpaceHeader,
    gTBSHdrTableColDesc,
    IDU_STARTUP_CONTROL,
    0,
    0,
    IDU_FT_DESC_TRANS_NOT_USE,
    NULL
};

#define SCT_TBS_PERFVIEW_LOG_COMPRESS_OFF (0)
#define SCT_TBS_PERFVIEW_LOG_COMPRESS_ON  (1)


/*
    Tablespace Header Performance View����ü��
    Attribute Flag �ʵ���� �����Ѵ�.

    [IN] aSpaceNode - Tablespace��  Node
    [OUT] aSpaceHeader - Tablespace�� Header����ü
 */
IDE_RC sctFT::getSpaceHeaderAttrFlags(
                             sctTableSpaceNode * aSpaceNode,
                             sctTbsHeaderInfo  * aSpaceHeader)
{

    IDE_DASSERT( aSpaceNode   != NULL );
    IDE_DASSERT( aSpaceHeader != NULL );

    UInt sAttrFlag;

    IDU_FIT_POINT("BUG-46450@sctFT::getSpaceHeaderAttrFlags::getTBSLocation");

    switch( sctTableSpaceMgr::getTBSLocation( aSpaceNode ) )
    {
        case SMI_TBS_DISK:
            sAttrFlag = sddTableSpace::getTBSAttrFlag( (sddTableSpaceNode*)aSpaceNode );
            break;
        case SMI_TBS_MEMORY:
            sAttrFlag = smmManager::getTBSAttrFlag( (smmTBSNode*)aSpaceNode );
            break;
        case SMI_TBS_VOLATILE:
            sAttrFlag = svmManager::getTBSAttrFlag( (svmTBSNode*)aSpaceNode );
            break;
        default:
           IDE_ERROR_MSG( 0,
                          "Tablespace Type not found ( ID : %"ID_UINT32_FMT", Name : %s ) \n",
                          aSpaceNode->mID,
                          aSpaceNode->mName );
            //break;
    }

    if ( ( sAttrFlag & SMI_TBS_ATTR_LOG_COMPRESS_MASK )
         == SMI_TBS_ATTR_LOG_COMPRESS_TRUE )
    {
        aSpaceHeader->mAttrLogCompress = SCT_TBS_PERFVIEW_LOG_COMPRESS_ON;
    }
    else
    {
        aSpaceHeader->mAttrLogCompress = SCT_TBS_PERFVIEW_LOG_COMPRESS_OFF;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/******************************************************************************
 * Abstraction : TBS�� state bitset�� �޾Ƽ� Stable state�� ���ڿ���
 *               ��ȯ�Ѵ�. X$TABLESPACES_HEADER�� TBS�� state��
 *               ���ڿ��� ����ϱ� ���� ����Ѵ�.
 *
 *  aTBSStateBitset - [IN]  ���ڿ��� ��ȯ �� State BitSet
 *  aTBSStateName   - [OUT] ���ڿ��� ��ȯ�� ������
 *****************************************************************************/
void sctFT::getTBSStateName( UInt    aTBSStateBitset,
                             UChar*  aTBSStateName )
{
    aTBSStateBitset &= SMI_TBS_STATE_USER_MASK ;

    switch( aTBSStateBitset )
    {
        case SMI_TBS_OFFLINE:
               idlOS::snprintf((char*)aTBSStateName, SM_DUMP_VALUE_LENGTH, "%s",
                               "OFFLINE" );
            break;
        case SMI_TBS_ONLINE:
               idlOS::snprintf((char*)aTBSStateName, SM_DUMP_VALUE_LENGTH, "%s",
                               "ONLINE" );
            break;
        case SMI_TBS_DROPPED:
               idlOS::snprintf((char*)aTBSStateName, SM_DUMP_VALUE_LENGTH, "%s",
                               "DROPPED" );
            break;
        case SMI_TBS_DISCARDED:
               idlOS::snprintf((char*)aTBSStateName, SM_DUMP_VALUE_LENGTH, "%s",
                               "DISCARDED" );
            break;
        default:
               idlOS::snprintf((char*)aTBSStateName, SM_DUMP_VALUE_LENGTH, "%s",
                               "CHANGING" );
            break;
    }
}



// table space header������ �����ֱ�����
IDE_RC sctFT::buildRecordForTableSpaceHeader(idvSQL              *aStatistics,
                                             void                *aHeader,
                                             void        * /* aDumpObj */,
                                             iduFixedTableMemory *aMemory)
{
    UInt                i;
    sctTbsHeaderInfo    sSpaceHeader;
    sctTableSpaceNode  *sSpaceNode;
    scSpaceID           sNewTableSpaceID;
    void              * sIndexValues[2];
    idBool              sIsLocked = ID_FALSE;

    IDE_ERROR( aHeader != NULL );
    IDE_ERROR( aMemory != NULL );

    sNewTableSpaceID = sctTableSpaceMgr::getNewTableSpaceID();

    for( i = 0; i < sNewTableSpaceID; i++ )
    {
        sSpaceNode = sctTableSpaceMgr::getSpaceNodeBySpaceID(i);

        if( sSpaceNode == NULL )
        {
            continue;
        }

        /* BUG-43006 FixedTable Indexing Filter
         * Indexing Filter�� ����ؼ� ��ü Record�� ���������ʰ�
         * �κи� ������ Filtering �Ѵ�.
         * 1. void * �迭�� IDU_FT_COLUMN_INDEX �� ������ �÷���
         * �ش��ϴ� ���� ������� �־��־�� �Ѵ�.
         * 2. IDU_FT_COLUMN_INDEX�� �÷��� �ش��ϴ� ���� ��� ��
         * �� �־���Ѵ�.
         */
        sIndexValues[0] = &sSpaceNode->mID;
        sIndexValues[1] = &sSpaceNode->mType;
        if ( iduFixedTable::checkKeyRange( aMemory,
                                           gTBSHdrTableColDesc,
                                           sIndexValues )
             == ID_FALSE )
        {
            continue;
        }
        else
        {
            /* Nothing to do */
        }

        sctTableSpaceMgr::lockSpaceNode( aStatistics,
                                         sSpaceNode );
        sIsLocked = ID_TRUE;

        sSpaceHeader.mSpaceID = sSpaceNode->mID;
        sSpaceHeader.mName    = sSpaceNode->mName;
        sSpaceHeader.mType    = sSpaceNode->mType;

        sSpaceHeader.mStateBitset = sSpaceNode->mState;
        getTBSStateName( sSpaceNode->mState, sSpaceHeader.mStateName );

        IDE_TEST( getSpaceHeaderAttrFlags( sSpaceNode,
                                           & sSpaceHeader )
                  != IDE_SUCCESS );

        sIsLocked = ID_FALSE;
        sctTableSpaceMgr::unlockSpaceNode( sSpaceNode );

        IDE_TEST(iduFixedTable::buildRecord( aHeader,
                                             aMemory,
                                             (void *)&sSpaceHeader)
                 != IDE_SUCCESS);
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if ( sIsLocked == ID_TRUE )
    {
        sctTableSpaceMgr::unlockSpaceNode( sSpaceNode );
    }

    return IDE_FAILURE;
}

/* ------------------------------------------------
 *  Fixed Table Define for TableSpace
 * ----------------------------------------------*/

static iduFixedTableColDesc gTableSpaceTableColDesc[] =
{
    {
        (SChar*)"ID",
        offsetof(sctTbsInfo, mSpaceID),
        IDU_FT_SIZEOF(sctTbsInfo, mSpaceID),
        IDU_FT_TYPE_UINTEGER | IDU_FT_COLUMN_INDEX,
        NULL,
        0, 0,NULL // for internal use
    },
    {
        (SChar*)"NEXT_FILE_ID",
        offsetof(sctTbsInfo, mNewFileID),
        IDU_FT_SIZEOF(sctTbsInfo, mNewFileID),
        IDU_FT_TYPE_UINTEGER,
        NULL,
        0, 0,NULL // for internal use
    },
    {
        (SChar*)"EXTENT_MANAGEMENT",
        offsetof(sctTbsInfo, mExtMgmtName),
        IDU_FT_SIZEOF(sctTbsInfo, mExtMgmtName),
        IDU_FT_TYPE_VARCHAR,
        NULL,
        0, 0,NULL // for internal use
    },
    {
        (SChar*)"SEGMENT_MANAGEMENT",
        offsetof(sctTbsInfo, mSegMgmtName),
        IDU_FT_SIZEOF(sctTbsInfo, mSegMgmtName),
        IDU_FT_TYPE_VARCHAR,
        NULL,
        0, 0,NULL // for internal use
    },
    {
        (SChar*)"DATAFILE_COUNT",
        offsetof(sctTbsInfo, mDataFileCount),
        IDU_FT_SIZEOF(sctTbsInfo, mDataFileCount),
        IDU_FT_TYPE_UINTEGER,
        NULL,
        0, 0,NULL // for internal use
    },
    {
        (SChar*)"TOTAL_PAGE_COUNT",
        offsetof(sctTbsInfo, mTotalPageCount),
        IDU_FT_SIZEOF(sctTbsInfo, mTotalPageCount),
        IDU_FT_TYPE_UBIGINT,
        NULL,
        0, 0,NULL // for internal use
    },
    {
        (SChar*)"ALLOCATED_PAGE_COUNT",
        offsetof(sctTbsInfo,mAllocPageCount),
        IDU_FT_SIZEOF(sctTbsInfo,mAllocPageCount),
        IDU_FT_TYPE_UBIGINT,
        NULL,
        0, 0,NULL // for internal use
    },
    {
        (SChar*)"EXTENT_PAGE_COUNT",
        offsetof(sctTbsInfo, mExtPageCount ),
        IDU_FT_SIZEOF(sctTbsInfo,mExtPageCount ),
        IDU_FT_TYPE_UINTEGER,
        NULL,
        0, 0,NULL // for internal use
    },

    {
        (SChar*)"PAGE_SIZE",
        offsetof(sctTbsInfo, mPageSize ),
        IDU_FT_SIZEOF(sctTbsInfo,mPageSize ),
        IDU_FT_TYPE_UINTEGER,
        NULL,
        0, 0,NULL // for internal use
    },

    {
        (SChar*)"CACHED_FREE_EXTENT_COUNT",
        offsetof(sctTbsInfo, mCachedFreeExtCount ),
        IDU_FT_SIZEOF(sctTbsInfo,mCachedFreeExtCount ),
        IDU_FT_TYPE_UBIGINT,
        NULL,
        0, 0,NULL // for internal use
    },

    {
        NULL,
        0,
        0,
        IDU_FT_TYPE_CHAR,
        NULL,
        0, 0,NULL // for internal use
    }
};

iduFixedTableDesc gTableSpaceTableDesc =
{
    (SChar *)"X$TABLESPACES",
    sctFT::buildRecordForTABLESPACES,
    gTableSpaceTableColDesc,
    IDU_STARTUP_CONTROL,
    0,
    0,
    IDU_FT_DESC_TRANS_NOT_USE,
    NULL
};

/*
    Disk Tablespace�� ������ Fix Table ����ü�� �����Ѵ�.
 */
IDE_RC sctFT::getDiskTBSInfo( sddTableSpaceNode * aDiskSpaceNode,
                              sctTbsInfo        * aSpaceInfo )
{
    IDE_DASSERT( aDiskSpaceNode != NULL );
    IDE_DASSERT( aSpaceInfo != NULL );
    IDE_ASSERT( aDiskSpaceNode->mExtMgmtType < SMI_EXTENT_MGMT_MAX );

    aSpaceInfo->mNewFileID = aDiskSpaceNode->mNewFileID;

    idlOS::snprintf((char*)aSpaceInfo->mExtMgmtName, 20, "%s",
                    "BITMAP" );

    if( aDiskSpaceNode->mSegMgmtType == SMI_SEGMENT_MGMT_FREELIST_TYPE )
    {
        idlOS::snprintf((char*)aSpaceInfo->mSegMgmtName, 20, "%s",
                        "MANUAL" );
    }
    else
    {
        if( aDiskSpaceNode->mSegMgmtType == SMI_SEGMENT_MGMT_TREELIST_TYPE )
        {
            idlOS::snprintf((char*)aSpaceInfo->mSegMgmtName, 20, "%s",
                            "AUTO" );
        }
        else
        {
            IDE_ASSERT( aDiskSpaceNode->mSegMgmtType ==
                        SMI_SEGMENT_MGMT_CIRCULARLIST_TYPE );
            
            idlOS::snprintf((char*)aSpaceInfo->mSegMgmtName, 20, "%s",
                            "CIRCULAR" );
        }
    }

    IDU_FIT_POINT("BUG-47203@sctFT::getDiskTBSInfo::getCachedFreeExtCount");

    aSpaceInfo->mCachedFreeExtCount = smLayerCallback::getCachedFreeExtCount( aDiskSpaceNode );

    aSpaceInfo->mTotalPageCount = sddTableSpace::getTotalPageCount(aDiskSpaceNode);

    IDU_FIT_POINT("BUG-47252@sctFT::getDiskTBSInfo::getAllocPageCount");

    IDE_TEST( smLayerCallback::getAllocPageCount( NULL,
                                                  aDiskSpaceNode,
                                                  &(aSpaceInfo->mAllocPageCount) ) 
              != IDE_SUCCESS );

    aSpaceInfo->mExtPageCount    = aDiskSpaceNode->mExtPageCount;
    aSpaceInfo->mDataFileCount   = aDiskSpaceNode->mDataFileCount;
    aSpaceInfo->mPageSize        = SD_PAGE_SIZE;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*
    Memory Tablespace�� ������ Fix Table ����ü�� �����Ѵ�.
 */
void sctFT::getMemTBSInfo( smmTBSNode   * aMemSpaceNode,
                           sctTbsInfo   * aSpaceInfo )
{
    IDE_DASSERT( aMemSpaceNode != NULL );
    IDE_DASSERT( aSpaceInfo != NULL );

    aSpaceInfo->mNewFileID          = 0;
    aSpaceInfo->mExtPageCount       = aMemSpaceNode->mChunkPageCnt;
    aSpaceInfo->mPageSize           = SM_PAGE_SIZE;
    aSpaceInfo->mCachedFreeExtCount = 0;

    IDE_ASSERT( smmFPLManager::lockGlobalPageCountCheckMutex() == IDE_SUCCESS );

    if ( aMemSpaceNode->mMemBase != NULL )
    {
        aSpaceInfo->mDataFileCount      =
            aMemSpaceNode->mMemBase->mDBFileCount[ aMemSpaceNode->mTBSAttr.mMemAttr.
                                                   mCurrentDB ];
        aSpaceInfo->mAllocPageCount =
            aSpaceInfo->mTotalPageCount =
            aMemSpaceNode->mMemBase->mAllocPersPageCount;
    }
    else
    {
        // Offline�̰ų� ���� Restore���� ���� Tablespace�� ��� mMemBase�� NULL
        aSpaceInfo->mAllocPageCount = 0 ;
        aSpaceInfo->mDataFileCount  = 0;
        aSpaceInfo->mTotalPageCount = 0;
    }
    IDE_ASSERT( smmFPLManager::unlockGlobalPageCountCheckMutex() == IDE_SUCCESS );
}


/*
    Volatile Tablespace�� ������ Fixed Table ����ü�� �����Ѵ�.
 */
void sctFT::getVolTBSInfo( svmTBSNode   * aVolSpaceNode,
                           sctTbsInfo   * aSpaceInfo )
{
    IDE_DASSERT( aVolSpaceNode != NULL );
    IDE_DASSERT( aSpaceInfo != NULL );

    aSpaceInfo->mNewFileID          = 0;
    aSpaceInfo->mExtPageCount       = aVolSpaceNode->mChunkPageCnt;
    aSpaceInfo->mPageSize           = SM_PAGE_SIZE;
    aSpaceInfo->mCachedFreeExtCount = 0;

    // Volatile Tablespace�� ��� Data File�� ����.
    aSpaceInfo->mDataFileCount      = 0;

    aSpaceInfo->mAllocPageCount =
        aSpaceInfo->mTotalPageCount =
        aVolSpaceNode->mMemBase.mAllocPersPageCount;
}


/***********************************************************************
 *
 * Description : ��� ��ũ Tablesapce�� Tablespace���� ��� ������ ����Ѵ�.
 *
 * aHeader  - [IN] Fixed Table ���������
 * aDumpObj - [IN] D$�� ���� ���� ����Ʈ ������
 * aMemory  - [IN] Fixed Table�� ���� Memory Manger
 *
 **********************************************************************/
IDE_RC sctFT::buildRecordForTABLESPACES(idvSQL              * /*aStatistics*/,
                                        void                *aHeader,
                                        void        * /* aDumpObj */,
                                        iduFixedTableMemory *aMemory)
{
    sctTbsInfo         sSpaceInfo;
    sctTableSpaceNode *sCurrSpaceNode;
    void             * sIndexValues[1];

    IDE_ERROR( aHeader != NULL );
    IDE_ERROR( aMemory != NULL );

    IDE_DASSERT( (ID_SIZEOF(smiTableSpaceType) == ID_SIZEOF(UInt)) &&
                 (ID_SIZEOF(smiTableSpaceState) == ID_SIZEOF(UInt)) );

    //----------------------------------------
    // X$TABLESPACES�� ���� Record ����
    //----------------------------------------

    sCurrSpaceNode = sctTableSpaceMgr::getFirstSpaceNode();

    while( sCurrSpaceNode != NULL )
    {
        idlOS::memset( &sSpaceInfo,
                       0x00,
                       ID_SIZEOF( sctTbsInfo ) );

        sSpaceInfo.mSpaceID = sCurrSpaceNode->mID;

        /* BUG-43006 FixedTable Indexing Filter
         * Column Index �� ����ؼ� ��ü Record�� ���������ʰ�
         * �κи� ������ Filtering �Ѵ�.
         * 1. void * �迭�� IDU_FT_COLUMN_INDEX �� ������ �÷���
         * �ش��ϴ� ���� ������� �־��־�� �Ѵ�.
         * 2. IDU_FT_COLUMN_INDEX�� �÷��� �ش��ϴ� ���� ��� ��
         * �� �־���Ѵ�.
         */
        sIndexValues[0] = &sSpaceInfo.mSpaceID;
        if ( iduFixedTable::checkKeyRange( aMemory,
                                           gTableSpaceTableColDesc,
                                           sIndexValues )
             == ID_FALSE )
        {
            /* MEM TBS�� ��쿡�� Drop Pending ���µ� Drop�� ���̱� ������
             * ���ܽ�Ų�� */
            sCurrSpaceNode = sctTableSpaceMgr::getNextSpaceNodeWithoutDropped( sCurrSpaceNode->mID );
            continue;
        }
        else
        {
            /* Nothing to do */
        }

        IDU_FIT_POINT("BUG-46450@sctFT::buildRecordForTABLESPACES::getTBSLocation");

        switch( sctTableSpaceMgr::getTBSLocation( sCurrSpaceNode ) )
        {
            case SMI_TBS_DISK:
                // smLayerCallback::getAllocPageCount ���߿�
                // sddDiskMgr::read �����ϴٰ� sctTableSpaceMgr::lock�� ��� ������
                // �Լ��ȿ��� lock���� �ϰ� �ٽ� ��´�.
                /* �ѹ� Lock�� Ǯ������ ���̻� SpaceNode�� ����ϸ� �ȵȴ�. */
                IDE_TEST( getDiskTBSInfo( (sddTableSpaceNode*)sCurrSpaceNode,
                                          & sSpaceInfo )
                          != IDE_SUCCESS );
                break;

            case SMI_TBS_MEMORY:

                /* To Fix BUG-23912 [AT-F5 ART] MemTBS ���� ��½� Membase�� �����Ҷ�
                 * lockGlobalPageCountCheckMutexfmf ȹ���ؾ���
                 * Dirty Read �߿� membase�� ���ڱ� Null�� �ɼ� �ִ�. */

                getMemTBSInfo( (smmTBSNode*)sCurrSpaceNode,
                               & sSpaceInfo );
                break;

            case SMI_TBS_VOLATILE:

                /* Membase�� Null�� �Ǵ� ��찡 �������� �����Ƿ�,
                 * Dirty Read�ص� �����ϴ� */
                getVolTBSInfo( (svmTBSNode*)sCurrSpaceNode,
                               & sSpaceInfo );
                break;

            default:
                IDE_ERROR_MSG( 0,
                                "Tablespace Type not found ( ID : %"ID_UINT32_FMT", Name : %s ) \n",
                                sCurrSpaceNode->mID,
                                sCurrSpaceNode->mName );
                break;
        }

        if ((sCurrSpaceNode->mState & SMI_TBS_DROPPED) != SMI_TBS_DROPPED )
        {
            // �� ���̿� Drop�Ǿ��� ���� �ִ�. Drop���� ���� �͸� ����Ѵ�.
            IDE_TEST( iduFixedTable::buildRecord( aHeader,
                                                  aMemory,
                                                  &sSpaceInfo )
                      != IDE_SUCCESS );
        }

        /* MEM TBS�� ��쿡�� Drop Pending ���µ� Drop�� ���̱� ������
         * ���ܽ�Ų�� */
        sCurrSpaceNode = sctTableSpaceMgr::getNextSpaceNodeWithoutDropped( sSpaceInfo.mSpaceID );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

