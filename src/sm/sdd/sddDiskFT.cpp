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
 * $Id$
 *
 * ��ũ ���̺����� ���� Fixed Table ����  
 ***********************************************************************/

#include <idu.h>
#include <sddDef.h>
#include <sdptbExtent.h>
#include <sddDiskFT.h>
#include <sctTableSpaceMgr.h>

/* ------------------------------------------------
 *  Fixed Table Define for X$DATAFILES
 * ----------------------------------------------*/
typedef struct sddDataFileNodeInfo
{
    scSpaceID mSpaceID;
    sdFileID  mID;
    SChar*    mName;

    smLSN     mDiskRedoLSN;
    smLSN     mDiskCreateLSN;

    UInt      mSmVersion;

    ULong     mNextSize;
    ULong     mMaxSize;
    ULong     mInitSize;
    ULong     mCurrSize;
    idBool    mIsAutoExtend;
    UInt      mIOCount;
    idBool    mIsOpened;
    idBool    mIsModified;
    UInt      mState;
    UInt      mMaxOpenFDCnt;
    UInt      mCurOpenFDCnt;
    idBool    mFreenessOfGG;  // BUG-47666
} sddDataFileNodeInfo;

static iduFixedTableColDesc gDataFileTableColDesc[] =
{
    {
        (SChar*)"ID",
        offsetof(sddDataFileNodeInfo, mID),
        IDU_FT_SIZEOF(sddDataFileNodeInfo, mID),
        IDU_FT_TYPE_UINTEGER | IDU_FT_COLUMN_INDEX,  // BUG-48160
        NULL,
        0, 0,NULL // for internal use
    },

    {
        (SChar*)"NAME",
        offsetof(sddDataFileNodeInfo, mName),
        256,
        IDU_FT_TYPE_VARCHAR | IDU_FT_TYPE_POINTER | IDU_FT_COLUMN_INDEX, // BUG-48160
        NULL,
        0, 0,NULL // for internal use
    },

    {
        (SChar*)"SPACEID",
        offsetof(sddDataFileNodeInfo, mSpaceID),
        IDU_FT_SIZEOF(sddDataFileNodeInfo, mSpaceID),
        IDU_FT_TYPE_UINTEGER | IDU_FT_COLUMN_INDEX,
        NULL,
        0, 0,NULL // for internal use
    },
    
    {
        (SChar*)"DISK_REDO_LSN_FILENO",
        offsetof(sddDataFileNodeInfo, mDiskRedoLSN)    +
        offsetof(smLSN, mFileNo),
        IDU_FT_SIZEOF(smLSN, mFileNo),
        IDU_FT_TYPE_UINTEGER,
        NULL,
        0, 0,NULL // for internal use
    },
    {
        (SChar*)"DISK_REDO_LSN_OFFSET",
        offsetof(sddDataFileNodeInfo, mDiskRedoLSN)    +
        offsetof(smLSN, mOffset),
        IDU_FT_SIZEOF(smLSN, mOffset),
        IDU_FT_TYPE_UINTEGER,
        NULL,
        0, 0,NULL // for internal use
    },

    {
        (SChar*)"CREATE_LSN_FILENO",
        offsetof(sddDataFileNodeInfo, mDiskCreateLSN)    +
        offsetof(smLSN, mFileNo),
        IDU_FT_SIZEOF(smLSN, mFileNo),
        IDU_FT_TYPE_UINTEGER,
        NULL,
        0, 0,NULL // for internal use
    },
    {
        (SChar*)"CREATE_LSN_OFFSET",
        offsetof(sddDataFileNodeInfo, mDiskCreateLSN)    +
        offsetof(smLSN, mOffset),
        IDU_FT_SIZEOF(smLSN, mOffset),
        IDU_FT_TYPE_UINTEGER,
        NULL,
        0, 0,NULL // for internal use
    },

    {
        (SChar*)"SM_VERSION",
        offsetof(sddDataFileNodeInfo, mSmVersion),
        IDU_FT_SIZEOF(sddDataFileNodeInfo, mSmVersion),
        IDU_FT_TYPE_UINTEGER,
        NULL,
        0, 0,NULL // for internal use
    },

    {
        (SChar*)"NEXTSIZE",
        offsetof(sddDataFileNodeInfo, mNextSize),
        IDU_FT_SIZEOF(sddDataFileNodeInfo, mNextSize),
        IDU_FT_TYPE_BIGINT,
        NULL,
        0, 0,NULL // for internal use
    },

    {
        (SChar*)"MAXSIZE",
        offsetof(sddDataFileNodeInfo, mMaxSize),
        IDU_FT_SIZEOF(sddDataFileNodeInfo, mMaxSize),
        IDU_FT_TYPE_BIGINT,
        NULL,
        0, 0,NULL // for internal use
    },

    {
        (SChar*)"INITSIZE",
        offsetof(sddDataFileNodeInfo, mInitSize),
        IDU_FT_SIZEOF(sddDataFileNodeInfo, mInitSize),
        IDU_FT_TYPE_BIGINT,
        NULL,
        0, 0,NULL // for internal use
    },

    {
        (SChar*)"CURRSIZE",
        offsetof(sddDataFileNodeInfo, mCurrSize),
        IDU_FT_SIZEOF(sddDataFileNodeInfo, mCurrSize),
        IDU_FT_TYPE_BIGINT,
        NULL,
        0, 0,NULL // for internal use
    },

    {
        (SChar*)"AUTOEXTEND",
        offsetof(sddDataFileNodeInfo, mIsAutoExtend),
        IDU_FT_SIZEOF(sddDataFileNodeInfo, mIsAutoExtend),
        IDU_FT_TYPE_UINTEGER,
        NULL,
        0, 0,NULL // for internal use
    },

    {
        (SChar*)"IOCOUNT",
        offsetof(sddDataFileNodeInfo, mIOCount),
        IDU_FT_SIZEOF(sddDataFileNodeInfo, mIOCount),
        IDU_FT_TYPE_UINTEGER,
        NULL,
        0, 0,NULL // for internal use
    },

    {
        (SChar*)"OPENED",
        offsetof(sddDataFileNodeInfo, mIsOpened), // BUGBUG : 4byte���� Ȯ��
        IDU_FT_SIZEOF(sddDataFileNodeInfo, mIsOpened), // BUGBUG : 4byte���� Ȯ,
        IDU_FT_TYPE_UINTEGER,
        NULL,
        0, 0,NULL // for internal use
    },

    {
        (SChar*)"MODIFIED",
        offsetof(sddDataFileNodeInfo, mIsModified), // BUGBUG : 4byte���� Ȯ��
        IDU_FT_SIZEOF(sddDataFileNodeInfo, mIsModified), // BUGBUG : 4byte���� Ȯ,
        IDU_FT_TYPE_UINTEGER,
        NULL,
        0, 0,NULL // for internal use
    },

    {
        (SChar*)"STATE",
        offsetof(sddDataFileNodeInfo, mState), // BUGBUG : 4byte���� Ȯ��
        IDU_FT_SIZEOF(sddDataFileNodeInfo, mState), // BUGBUG : 4byte���� Ȯ,
        IDU_FT_TYPE_UINTEGER,
        NULL,
        0, 0,NULL // for internal use
    },
    {
        (SChar*)"MAX_OPEN_FD_COUNT",
        offsetof(sddDataFileNodeInfo, mMaxOpenFDCnt), // BUGBUG : 4byte���� Ȯ��
        IDU_FT_SIZEOF(sddDataFileNodeInfo, mMaxOpenFDCnt), // BUGBUG : 4byte���� Ȯ,
        IDU_FT_TYPE_UINTEGER,
        NULL,
        0, 0,NULL // for internal use
    },
    {
        (SChar*)"CUR_OPEN_FD_COUNT",
        offsetof(sddDataFileNodeInfo, mCurOpenFDCnt),
        IDU_FT_SIZEOF(sddDataFileNodeInfo, mCurOpenFDCnt),
        IDU_FT_TYPE_UINTEGER,
        NULL,
        0, 0,NULL // for internal use
    },
    {
        (SChar*)"FREENESS_OF_GG",
        offsetof(sddDataFileNodeInfo, mFreenessOfGG),     /* BUG-47666 Freeness Bit �� Ȯ�� */
        IDU_FT_SIZEOF(sddDataFileNodeInfo, mFreenessOfGG),
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

/* ------------------------------------------------
 *  Fixed Table Define for X$PAGE_SIZE 
 * ----------------------------------------------*/
static iduFixedTableColDesc  gPageSizeTableColDesc[] =
{
    {
        (SChar*)"PAGE_SIZE",
        0, // none-structure.
        ID_SIZEOF(UInt),
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

/* ------------------------------------------------
 *  Build Fixed Table Define for X$DATAFILES
 * ----------------------------------------------*/
IDE_RC sddDiskFT::buildFT4DATAFILES( idvSQL		 * aStatistics,
                                     void                * aHeader,
                                     void                * /* aDumpObj */,
                                     iduFixedTableMemory * aMemory )
{
    sddDataFileNode     *sFileNode;
    sddTableSpaceNode   *sCurrSpaceNode;
    UInt                 sState = 0;
    UInt                 i;
    sddDataFileNodeInfo  sDataFileNodeInfo;
    void               * sIndexValues[3];

    IDE_DASSERT( (ID_SIZEOF(smiDataFileState) == ID_SIZEOF(UInt)) &&
                 (ID_SIZEOF(smiDataFileMode) == ID_SIZEOF(UInt)) &&
                 (ID_SIZEOF(idBool) == ID_SIZEOF(UInt)) );

    sCurrSpaceNode = (sddTableSpaceNode*)sctTableSpaceMgr::getFirstSpaceNode();

    while( sCurrSpaceNode != NULL )
    {
        /* Disk Table Space�� �ƴϸ� ����*/
        if( sctTableSpaceMgr::isDiskTableSpace( sCurrSpaceNode ) == ID_TRUE )
        {
            sctTableSpaceMgr::lockSpaceNode( aStatistics,
                                             sCurrSpaceNode );
            sState = 1;

            /* Drop�Ǿ����� ���� */
            if ( (sCurrSpaceNode->mHeader.mState & SMI_TBS_DROPPED) != SMI_TBS_DROPPED )
            {
                for (i=0; i < sCurrSpaceNode->mNewFileID ; i++ )
                {
                    sFileNode = sCurrSpaceNode->mFileNodeArr[i] ;

                    if( sFileNode == NULL )
                    {
                        continue;
                    }

                    if( SMI_FILE_STATE_IS_DROPPED( sFileNode->mState ) )
                    {
                        continue;
                    }

                    /* BUG-43006 FixedTable Indexing Filter
                     * Column Index �� ����ؼ� ��ü Record�� ���������ʰ�
                     * �κи� ������ Filtering �Ѵ�.
                     * 1. void * �迭�� IDU_FT_COLUMN_INDEX �� ������ �÷���
                     * �ش��ϴ� ���� ������� �־��־�� �Ѵ�.
                     * 2. IDU_FT_COLUMN_INDEX�� �÷��� �ش��ϴ� ���� ��� ��
                     * �� �־���Ѵ�.
                     */
                    /* BUG-48160 ���͸� �׸� �߰� */
                    sIndexValues[0] = &sFileNode->mID;
                    sIndexValues[1] = &sFileNode->mName;
                    sIndexValues[2] = &sFileNode->mSpaceID;
                    if ( iduFixedTable::checkKeyRange( aMemory,
                                                       gDataFileTableColDesc,
                                                       sIndexValues )
                         == ID_FALSE )
                    {
                        continue;
                    }
                    else
                    {
                        /* Nothing to do */
                    }
                    sDataFileNodeInfo.mSpaceID       = sFileNode->mSpaceID;
                    sDataFileNodeInfo.mID            = sFileNode->mID;
                    sDataFileNodeInfo.mName          = sFileNode->mName;

                    sDataFileNodeInfo.mDiskRedoLSN   =
                        sFileNode->mDBFileHdr.mRedoLSN;
                    sDataFileNodeInfo.mDiskCreateLSN =
                        sFileNode->mDBFileHdr.mCreateLSN;

                    sDataFileNodeInfo.mSmVersion     =
                        sFileNode->mDBFileHdr.mSmVersion;

                    sDataFileNodeInfo.mNextSize      = sFileNode->mNextSize;
                    sDataFileNodeInfo.mMaxSize       = sFileNode->mMaxSize;
                    sDataFileNodeInfo.mInitSize      = sFileNode->mInitSize;
                    sDataFileNodeInfo.mCurrSize      = sFileNode->mCurrSize;
                    sDataFileNodeInfo.mIsAutoExtend  = sFileNode->mIsAutoExtend;
                    sDataFileNodeInfo.mIOCount       = sFileNode->mIOCount;
                    sDataFileNodeInfo.mIsOpened      = sFileNode->mIsOpened;
                    sDataFileNodeInfo.mIsModified    = sFileNode->mIsModified;
                    sDataFileNodeInfo.mState         = sFileNode->mState;
                    sDataFileNodeInfo.mMaxOpenFDCnt  = sFileNode->mFile.getMaxFDCnt();
                    sDataFileNodeInfo.mCurOpenFDCnt  = sFileNode->mFile.getCurFDCnt();
                    sDataFileNodeInfo.mFreenessOfGG  = sdptbExtent::getFreenessOfGG( sCurrSpaceNode,
                                                                                     sFileNode->mID );

                    /* [2] make one fixed table record */
                    IDE_TEST(iduFixedTable::buildRecord(aHeader,
                                                        aMemory,
                                                        (void *)&sDataFileNodeInfo)
                             != IDE_SUCCESS);
                }
            }
            sState = 0;
            sctTableSpaceMgr::unlockSpaceNode( sCurrSpaceNode );
        }
        sCurrSpaceNode = (sddTableSpaceNode*)sctTableSpaceMgr::getNextSpaceNode( sCurrSpaceNode->mHeader.mID );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if ( sState != 0 )
    {
        sctTableSpaceMgr::unlockSpaceNode( sCurrSpaceNode );
    }

    return IDE_FAILURE;
}

iduFixedTableDesc gDataFileTableDesc =
{
    (SChar *)"X$DATAFILES",
    sddDiskFT::buildFT4DATAFILES,
    gDataFileTableColDesc,
    IDU_STARTUP_CONTROL,
    0,
    0,
    IDU_FT_DESC_TRANS_NOT_USE,
    NULL
};

/* ------------------------------------------------
 *  Build Fixed Table Define for X$PAGE_SIZE
 * ----------------------------------------------*/
IDE_RC sddDiskFT::buildFT4PAGESIZE( idvSQL		* /* aStatistics */,
                                    void                * aHeader,
                                    void                * /* aDumpObj */,
                                    iduFixedTableMemory * aMemory )
{
    UInt     sPageSize;
    
    sPageSize = SD_PAGE_SIZE;
    
    /* [2] make one fixed table record */
    IDE_TEST(iduFixedTable::buildRecord(aHeader,
                                        aMemory,
                                        (void *)&sPageSize)
             != IDE_SUCCESS);
    
    return IDE_SUCCESS;
    
    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
    
}

iduFixedTableDesc gPageSizeTableDesc =
{
    (SChar *)"X$PAGE_SIZE",
    sddDiskFT::buildFT4PAGESIZE,
    gPageSizeTableColDesc,
    IDU_STARTUP_CONTROL,
    0,
    0,
    IDU_FT_DESC_TRANS_NOT_USE,
    NULL
};

/* ------------------------------------------------
 *  Fixed Table Define for X$FILESTAT
 * ----------------------------------------------*/
static iduFixedTableColDesc gFileStatTableColDesc[] =
{
    {
        (SChar*)"SPACEID",
        offsetof(sddFileStatFT, mSpaceID),
        IDU_FT_SIZEOF(sddFileStatFT, mSpaceID),
        IDU_FT_TYPE_UINTEGER | IDU_FT_COLUMN_INDEX,
        NULL,
        0, 0,NULL // for internal use
    },
    {
        (SChar*)"FILEID",
        offsetof(sddFileStatFT, mFileID),
        IDU_FT_SIZEOF(sddFileStatFT, mFileID),
        IDU_FT_TYPE_UINTEGER | IDU_FT_COLUMN_INDEX,
        NULL,
        0, 0,NULL // for internal use
    },
    //  Physical Read I/O Count
    {
        (SChar*)"PHYRDS",
        offsetof(sddFileStatFT, mFileIOStat) +
        offsetof(iduFIOStat, mPhyReadCount),
        IDU_FT_SIZEOF(iduFIOStat, mPhyWriteCount),
        IDU_FT_TYPE_BIGINT,
        NULL,
        0, 0,NULL // for internal use
    },
    //  Physical Write I/O Count
    {
        (SChar*)"PHYWRTS",
        offsetof(sddFileStatFT, mFileIOStat) +
        offsetof(iduFIOStat, mPhyWriteCount),
        IDU_FT_SIZEOF(iduFIOStat, mPhyWriteCount),
        IDU_FT_TYPE_BIGINT,
        NULL,
        0, 0,NULL // for internal use
    },
    //  Physical Block Read Count
    {
        (SChar*)"PHYBLKRD",
        offsetof(sddFileStatFT, mFileIOStat) +
        offsetof(iduFIOStat, mPhyBlockReadCount),
        IDU_FT_SIZEOF(iduFIOStat, mPhyBlockReadCount),
        IDU_FT_TYPE_BIGINT,
        NULL,
        0, 0,NULL // for internal use
    },
    //  Physical Block Write Count
    {
        (SChar*)"PHYBLKWRT",
        offsetof(sddFileStatFT, mFileIOStat) +
        offsetof(iduFIOStat, mPhyBlockWriteCount),
        IDU_FT_SIZEOF(iduFIOStat, mPhyBlockWriteCount),
        IDU_FT_TYPE_BIGINT,
        NULL,
        0, 0,NULL // for internal use
    },
    // Single Block Read I/O Count
    // ��Ƽ���̽��� ���� Block I/O�� �����Ѵ�. 
    {
        (SChar*)"SINGLEBLKRDS",
        offsetof(sddFileStatFT, mFileIOStat) +
        offsetof(iduFIOStat, mPhyBlockReadCount),
        IDU_FT_SIZEOF(iduFIOStat, mPhyBlockReadCount),
        IDU_FT_TYPE_BIGINT | IDU_FT_COLUMN_INDEX,
        NULL,
        0, 0,NULL // for internal use
    },
    //  Read I/O Time  ( milli-sec. )
    {
        (SChar*)"READTIM",
        offsetof(sddFileStatFT, mFileIOStat) +
        offsetof(iduFIOStat, mReadTime),
        IDU_FT_SIZEOF(iduFIOStat, mReadTime),
        IDU_FT_TYPE_BIGINT,
        NULL,
        0, 0,NULL // for internal use
    },
    //  Write I/O Time ( milli-sec. )
    {
        (SChar*)"WRITETIM",
        offsetof(sddFileStatFT, mFileIOStat) +
        offsetof(iduFIOStat, mWriteTime),
        IDU_FT_SIZEOF(iduFIOStat, mWriteTime),
        IDU_FT_TYPE_BIGINT,
        NULL,
        0, 0,NULL // for internal use
    },
    //  Single Block Read I/O Time ( milli-sec. )
    {
        (SChar*)"SINGLEBLKRDTIM",
        offsetof(sddFileStatFT, mFileIOStat) +
        offsetof(iduFIOStat, mSingleBlockReadTime),
        IDU_FT_SIZEOF(iduFIOStat, mSingleBlockReadTime),
        IDU_FT_TYPE_BIGINT,
        NULL,
        0, 0,NULL // for internal use
    },
    // ��� I/O Time ( milli-sec. )
    {
        (SChar*)"AVGIOTIM",
        offsetof(sddFileStatFT, mAvgIOTime),
        IDU_FT_SIZEOF(sddFileStatFT, mAvgIOTime),
        IDU_FT_TYPE_BIGINT,
        NULL,
        0, 0,NULL // for internal use
    },
    // ������ I/O Tim e( milli-sec. )
    {
        (SChar*)"LSTIOTIM",
        offsetof(sddFileStatFT, mFileIOStat) +
        offsetof(iduFIOStat, mLstIOTime),
        IDU_FT_SIZEOF(iduFIOStat, mLstIOTime),
        IDU_FT_TYPE_BIGINT,
        NULL,
        0, 0,NULL // for internal use
    }, 
    //  �ּ� I/O Time ( milli-sec. )
    {
        (SChar*)"MINIOTIM",
        offsetof(sddFileStatFT, mFileIOStat) +
        offsetof(iduFIOStat, mMinIOTime),
        IDU_FT_SIZEOF(iduFIOStat, mMinIOTime),
        IDU_FT_TYPE_BIGINT,
        NULL,
        0, 0,NULL // for internal use
    },
    //  �ִ� Read I/O Time ( milli-sec. )
    {
        (SChar*)"MAXIORTM",
        offsetof(sddFileStatFT, mFileIOStat) +
        offsetof(iduFIOStat, mMaxIOReadTime),
        IDU_FT_SIZEOF(iduFIOStat, mMaxIOReadTime),
        IDU_FT_TYPE_BIGINT,
        NULL,
        0, 0,NULL // for internal use
    },
    //  �ִ� Write I/O Time ( milli-sec. )
    {
        (SChar*)"MAXIOWTM",
        offsetof(sddFileStatFT, mFileIOStat) +
        offsetof(iduFIOStat, mMaxIOWriteTime),
        IDU_FT_SIZEOF(iduFIOStat, mMaxIOWriteTime),
        IDU_FT_TYPE_BIGINT,
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


/* ------------------------------------------------
 *  Build Fixed Table Define for X$FILESTAT
 * ----------------------------------------------*/

IDE_RC sddDiskFT::buildFT4FILESTAT( idvSQL              * aStatistics,
                                    void                * aHeader,
                                    void                * /* aDumpObj */,
                                    iduFixedTableMemory * aMemory )
{
    sddDataFileNode   *sFileNode;
    sddTableSpaceNode *sCurrSpaceNode;
    UInt               sState = 0;
    sddFileStatFT      sFileStatFT;
    UInt               i;
    void             * sIndexValues[3];

    IDE_ERROR( aHeader != NULL );
    IDE_ERROR( aMemory != NULL );

    sCurrSpaceNode = (sddTableSpaceNode*)sctTableSpaceMgr::getFirstSpaceNode();

    while ( sCurrSpaceNode != NULL )
    {
        /* Disk Table Space�� �ƴϸ� ����*/
        if( sctTableSpaceMgr::isDiskTableSpace( sCurrSpaceNode ) == ID_TRUE )
        {
            sctTableSpaceMgr::lockSpaceNode( aStatistics,
                                             sCurrSpaceNode );
            sState = 1;

            /* lock��� �ٽ� Ȯ��, Drop�Ǿ����� ���� */
            if ( (sCurrSpaceNode->mHeader.mState & SMI_TBS_DROPPED) != SMI_TBS_DROPPED )
            {
                for (i=0; i < sCurrSpaceNode->mNewFileID ; i++ )
                {
                    sFileNode = sCurrSpaceNode->mFileNodeArr[i] ;

                    if( sFileNode == NULL )
                    {
                        continue;
                    }

                    if( SMI_FILE_STATE_IS_DROPPED( sFileNode->mState ) )
                    {
                        continue;
                    }

                    if ( sFileNode->mFile.getFIOStatOnOff() == IDU_FIO_STAT_OFF)
                    {
                        continue;
                    }
                
                    /* BUG-43006 FixedTable Indexing Filter
                     * Column Index �� ����ؼ� ��ü Record�� ���������ʰ�
                     * �κи� ������ Filtering �Ѵ�.
                     * 1. void * �迭�� IDU_FT_COLUMN_INDEX �� ������ �÷���
                     * �ش��ϴ� ���� ������� �־��־�� �Ѵ�.
                     * 2. IDU_FT_COLUMN_INDEX�� �÷��� �ش��ϴ� ���� ��� ��
                     * �� �־���Ѵ�.
                     */
                    /* BUG-48160 ���͸� �׸� �߰� */
                    sIndexValues[0] = &sFileNode->mSpaceID;
                    sIndexValues[1] = &sFileNode->mID;
                    sIndexValues[2] = &sFileNode->mFile.mStat.mPhyBlockReadCount;
                    if ( iduFixedTable::checkKeyRange( aMemory,
                                                       gFileStatTableColDesc,
                                                       sIndexValues )
                         == ID_FALSE )
                    {
                        continue;
                    }
                    else
                    {
                        /* Nothing to do */
                    }

                    sFileStatFT.mSpaceID    = sFileNode->mSpaceID;
                    sFileStatFT.mFileID     = sFileNode->mID;

                    idlOS::memcpy( &(sFileStatFT.mFileIOStat),
                                   &(sFileNode->mFile.mStat),
                                   ID_SIZEOF(iduFIOStat) );
                
                    // I/O ��� �ð� ���
                    if ( (sFileStatFT.mFileIOStat.mPhyReadCount+
                          sFileStatFT.mFileIOStat.mPhyWriteCount) > 0 )
                    {
                        // �� I/O ���� �ð��� �� I/O ȸ���� ������ ���Ѵ�. 
                        sFileStatFT.mAvgIOTime =
                            (sFileStatFT.mFileIOStat.mReadTime + 
                             sFileStatFT.mFileIOStat.mWriteTime ) /
                            (sFileStatFT.mFileIOStat.mPhyReadCount+
                             sFileStatFT.mFileIOStat.mPhyWriteCount);
                    }
                    else
                    {
                        sFileStatFT.mAvgIOTime = 0;
                    }

                    // I/O �ּ� Time 
                    if ( sFileStatFT.mFileIOStat.mMinIOTime == ID_ULONG_MAX )
                    {
                        sFileStatFT.mFileIOStat.mMinIOTime = 0;
                    }

                    /* [2] make one fixed table record */
                    IDE_TEST(iduFixedTable::buildRecord(aHeader,
                                                        aMemory,
                                                        (void *)&sFileStatFT)
                             != IDE_SUCCESS);
                }
            }
            sState = 0;
            sctTableSpaceMgr::unlockSpaceNode( sCurrSpaceNode );
        }
        sCurrSpaceNode = (sddTableSpaceNode*)sctTableSpaceMgr::getNextSpaceNode( sCurrSpaceNode->mHeader.mID );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if ( sState != 0 )
    {
        sctTableSpaceMgr::unlockSpaceNode( sCurrSpaceNode );
    }

    return IDE_FAILURE;
}

iduFixedTableDesc gFileStatTableDesc =
{
    (SChar *)"X$FILESTAT",
    sddDiskFT::buildFT4FILESTAT,
    gFileStatTableColDesc,
    IDU_STARTUP_CONTROL,
    0,
    0,
    IDU_FT_DESC_TRANS_NOT_USE,
    NULL
};
