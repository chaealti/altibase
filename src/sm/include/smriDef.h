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
 * Description :
 *
 * - Incremental chunk change tracking �ڷᱸ��
 * - ��� ����
 *   CT     = change tracking
 *   Ext    = extent
 *   Bmp    = bitmap
 *   Emp    = empty
 *
 *   changeTracking������ ����
 *   http://nok.altibase.com/x/L4C0
 *
 **********************************************************************/
#ifndef _O_SMRI_DEF_H_
#define _O_SMRI_DEF_H_ 1

#include <smiDef.h>
#include <smDef.h>
#include <idTypes.h>
#include <idu.h>

#define SMRI_CT_FILE_NAME                       "changeTracking"

#define SMRI_INCREMENTAL_BACKUP_FILE_EXT        ".ibak"

#define SMRI_CT_BIT_CNT                         (8)

/*CTBody���� EXTMAP block�� index*/
#define SMRI_CT_EXT_MAP_BLOCK_IDX               (0)

/*CT������ �����ϴ� block�� ũ��*/
#define SMRI_CT_BLOCK_SIZE                      (512)

/*CT������ header block�� ��ġ*/
#define SMRI_CT_HDR_OFFSET                      (0)

/*�� CTBody�� ������ �ִ� EXT�� ��*/
#define SMRI_CT_EXT_CNT                         (320)

/*�� CTBody�� ������ �ִ� BMPEXT�� ��
 * (MetaEXT�� EmpEXT�� ��ü EXT������ ����)*/
#define SMRI_CT_BMP_EXT_CNT                     (SMRI_CT_EXT_CNT - 2)

/*ExtMap�� ũ��*/
#define SMRI_CT_EXT_MAP_SIZE                    (SMRI_CT_BMP_EXT_CNT)

/*�� extent�� �����ϴ� block�� ��*/
#define SMRI_CT_EXT_BLOCK_CNT                   (64)

#define SMRI_CT_EXT_META_BLOCK_CNT              (1)

#define SMRI_CT_EXT_BLOCK_CNT_EXCEPT_META_BLOCK  (SMRI_CT_EXT_BLOCK_CNT            \
                                                  - SMRI_CT_EXT_META_BLOCK_CNT)

/*�� BmpBlock�� ������ bitmap�� ũ��*/
#define SMRI_CT_BMP_BLOCK_BITMAP_SIZE           (488)

#define SMRI_CT_DATAFILE_DESC_BLOCK_CNT         (SMRI_CT_EXT_BLOCK_CNT_EXCEPT_META_BLOCK)

/*Datafiel Desc block�� �������ִ� slot�� ��*/
#define SMRI_CT_DATAFILE_DESC_SLOT_CNT          (3)

#define SMRI_CT_DATAFILE_DESC_BMP_EXT_LIST_CNT  (3)

/*Datafile Desc block�� ��������� ���� slot�� �ִ��� Ȯ���ϱ����� flag*/
#define SMRI_CT_DATAFILE_DESC_SLOT_FLAG_MASK    (0x7)

#define SMRI_CT_DATAFILE_DESC_SLOT_FLAG_FIRST   (0x1)
#define SMRI_CT_DATAFILE_DESC_SLOT_FLAG_SECOND  (0x2)
#define SMRI_CT_DATAFILE_DESC_SLOT_FLAG_THIRD   (0x4)                 


/*Datafile Desc slot�� �ִ� BmpEXT list�� ���� hint�� ��*/
#define SMRI_CT_BMP_EXT_LIST_HINT_CNT           (9)

#define SMRI_CT_EXT_HDR_ID_MASK                 (0xFFFFFFC0)


/*differential backup�� ���� BmpExt list ��ȣ*/
#define SMRI_CT_DIFFERENTIAL_LIST0              (0)

/*differential backup�� ���� BmpExt list ��ȣ*/
#define SMRI_CT_DIFFERENTIAL_LIST1              (1)

/*cumulative backup�� ���� BmpExt list ��ȣ*/
#define SMRI_CT_CUMULATIVE_LIST                 (2)


/*
 * CT������ �������ִ� body�� ��
 *  - blockID���� ���� UInt�� ����� �ִ� 209715���� body�� ������ �ִ�.
 */
#define SMRI_CT_MAX_CT_BODY_CNT                 (128)
 
#define SMRI_CT_INVALID_BLOCK_ID                (ID_UINT_MAX)
#define SMRI_CT_DATAFILE_DESC_INVALID_SLOT_IDX  (ID_UINT_MAX)

#define SMRI_CT_EXT_USE                         ((UChar)(1))
#define SMRI_CT_EXT_FREE                        ((UChar)(0))
 
#define SMRI_CT_FILE_HDR_BLOCK_ID               SMRI_CT_INVALID_BLOCK_ID

/*extent�� ũ��*/
#define SMRI_CT_EXT_SIZE                        (SMRI_CT_BLOCK_SIZE            \
                                                 * SMRI_CT_EXT_BLOCK_CNT)

#define SMRI_CT_CAN_ALLOC_DATAFILE_DESC_SLOT( aAllocSlotFlag )      \
    ( ((~aAllocSlotFlag) & (SMRI_CT_DATAFILE_DESC_SLOT_FLAG_MASK)) ?  \
      ID_TRUE : ID_FALSE )


/*-------------------------------------------------------------
 * CT block type
 *-----------------------------------------------------------*/
typedef enum smriCTBlockType
{
    SMRI_CT_FILE_HDR_BLOCK,
    SMRI_CT_EXT_MAP_BLOCK,
    SMRI_CT_DATAFILE_DESC_BLOCK,
    SMRI_CT_BMP_EXT_HDR_BLOCK,
    SMRI_CT_BMP_BLOCK,
    SMRI_CT_EMPTY_BLOCK
}smriCTBlockType;
 
/*-------------------------------------------------------------
 * CT DataFileDesc slot�� �Ҵ�� ������������ tablespace type
 *-----------------------------------------------------------*/
typedef enum smriCTTBSType
{
    SMRI_CT_NONE,
    SMRI_CT_MEM_TBS,
    SMRI_CT_DISK_TBS
}smriCTTBSType;
 
/*-------------------------------------------------------------
 * slot�� ����� ������ ���� ����
 *-----------------------------------------------------------*/
typedef enum smriCTState
{
    SMRI_CT_TRACKING_DEACTIVE,
    SMRI_CT_TRACKING_ACTIVE
}smriCTState;
 
/*-------------------------------------------------------------
 * BMPExt Ÿ��
 *-----------------------------------------------------------*/
typedef enum smriCTBmpExtType
{
    SMRI_CT_DIFFERENTIAL_BMP_EXT_0 = 0,
    SMRI_CT_DIFFERENTIAL_BMP_EXT_1 = 1,
    SMRI_CT_CUMULATIVE_BMP_EXT     = 2,
    SMRI_CT_FREE_BMP_EXT           = 3
}smriCTBmpExtType;
 
 
/*-------------------------------------------------------------
 * change tracking��� Ȱ��ȭ ���� ����
 *-----------------------------------------------------------*/
typedef enum smriCTMgrState
{
    SMRI_CT_MGR_DISABLED,
    SMRI_CT_MGR_FILE_CREATING,
    SMRI_CT_MGR_ENABLED
}smriCTMgrState;

/*-------------------------------------------------------------
 * CT���� ��� ����
 *-----------------------------------------------------------*/
typedef enum smriCTHdrState
{
    SMRI_CT_MGR_STATE_NORMAL,
    SMRI_CT_MGR_STATE_FLUSH,
    SMRI_CT_MGR_STATE_EXTEND
}smriCTHdrState; 

/*-------------------------------------------------------------
 * logAnchor�� ����� CT���� ����
 *-----------------------------------------------------------*/
typedef struct smriCTFileAttr
{
    smLSN           mLastFlushLSN;
    SChar           mCTFileName[ SM_MAX_FILE_NAME ];
    smriCTMgrState  mCTMgrState;
}smriCTFileAttr;

 
/*-------------------------------------------------------------
 * Block header�� ����ü(24 bytes)
 *-----------------------------------------------------------*/
typedef struct smriCTBlockHdr
{
    /*block checksum*/
    UInt            mCheckSum;

    /*block ID*/
    UInt            mBlockID;

    /*block�� ���� extent header�� block ID*/
    UInt            mExtHdrBlockID;

    /*block�� ����*/
    smriCTBlockType    mBlockType;

    /*block�� ���� mutex*/
    iduMutex        mMutex;

#ifndef COMPILE_64BIT
    UChar           mAlign[4];
#endif
}smriCTBlockHdr;

 
/*-------------------------------------------------------------
 * CT file header block�� ����ü(512 bytes)
 *-----------------------------------------------------------*/
typedef struct smriCTFileHdrBlock
{
    /*block header*/
    smriCTBlockHdr      mBlockHdr;

    /*CTBody�� ��*/
    UInt                mCTBodyCNT;

    /*incremental backup���� �� ũ��(������ ��)*/
    UInt                mIBChunkSize;

    /*flush���� ���Ǻ���*/
    iduCond             mFlushCV;

    /*extend���� ���Ǻ���*/
    iduCond             mExtendCV;
               
    /*disk�� flush�� ���� �ֱ��� check point LSN*/
    smLSN               mLastFlushLSN;

    /*���������� ���������� �����ϴ� database �̸�*/
    SChar               mDBName[SM_MAX_DB_NAME];

#ifndef COMPILE_64BIT
    UChar               mAlign[336];
#else
    UChar               mAlign[328];
#endif
}smriCTFileHdrBlock;
 
/*-------------------------------------------------------------
 * extent map block�� ����ü(512 bytes)
 *-----------------------------------------------------------*/
typedef struct smriCTExtMapBlock
{
    //block header
    smriCTBlockHdr  mBlockHdr;

    UInt            mCTBodyID;

    smLSN           mFlushLSN;

    //extent map ����
    UChar           mExtentMap[SMRI_CT_EXT_MAP_SIZE];

    UChar           mAlign[158];
}smriCTExtMapBlock;
 
/*-------------------------------------------------------------
 * BMPExt List�� ����ü(40 bytes)
 *-----------------------------------------------------------*/
typedef struct smriCTBmpExtList
{
    UInt           mSetBitCount;
    /*BMPExt List�� hint, ���������� blockID�� �����ϴ� �迭��
      slot���Ҵ�ȼ�����ι迭��BmpExtHdr�� blockID������ȴ�*/
    UInt           mBmpExtListHint[SMRI_CT_BMP_EXT_LIST_HINT_CNT];
}smriCTBmpExtList;

/*-------------------------------------------------------------
 * datafile descriptor slot�� ����ü(160 bytes)
 *-----------------------------------------------------------*/
typedef struct smriCTDataFileDescSlot
{
    /*datafile descriptor slot ID*/
    smiDataFileDescSlotID       mSlotID;

    /*������ ���濡 ���� ���� Ȱ��ȭ ����*/
    smriCTState                 mTrackingState;

    /*�������� datafile�� ����*/
    smriCTTBSType               mTBSType;

    /*�������� datafile�� ������ ũ��*/
    UInt                        mPageSize;

    /*slot�� �Ҵ�� bitmap extent�� ��*/
    UShort                      mBmpExtCnt;

    /*���� �������� bitmap extent list�� ID*/
    UShort                      mCurTrackingListID;

    /*bitmap extent list(differential0, differential1, cumulative)*/
    smriCTBmpExtList            mBmpExtList[SMRI_CT_DATAFILE_DESC_BMP_EXT_LIST_CNT];

    /*datafile�� ID*/
    /*BUG-33931 FileID check is being wrong during changetracking*/
    UInt                        mFileID;

    /*table space ID*/
    scSpaceID                   mSpaceID;

    idBool                      mDummy;

    UChar                       mAlign[2];
}smriCTDataFileDescSlot;

/*-------------------------------------------------------------
 * datafile descriptor block�� ����ü(512 bytes)
 *-----------------------------------------------------------*/
typedef struct smriCTDataFileDescBlock
{
    /*block header*/
    smriCTBlockHdr          mBlockHdr;

    /*datafile descriptor slot�� ��������� Ȯ���ϱ� ���� flag*/
    UInt                    mAllocSlotFlag;
 
    /*datafile descriptor slot����*/
    smriCTDataFileDescSlot  mSlot[SMRI_CT_DATAFILE_DESC_SLOT_CNT];

    UChar                   mAlign[4];
}smriCTDataFileDescBlock;
 

/*-------------------------------------------------------------
 * Bmp Ext Hdr block�� ����ü(512 bytes)
 *-----------------------------------------------------------*/
typedef struct smriCTBmpExtHdrBlock
{
    /*block header*/
    smriCTBlockHdr              mBlockHdr;

    /*prev bitmap extent header block ID*/
    UInt                        mPrevBmpExtHdrBlockID;

    /*next bitmap extent header block ID*/
    UInt                        mNextBmpExtHdrBlockID;

    /*cumulative bitmap extent header block ID*/
    UInt                        mCumBmpExtHdrBlockID;

    /*bitmap extent�� �������� bitmap ������ Ÿ��(differential, cumulative)*/
    smriCTBmpExtType            mType;

    /*bitmap extent�� �Ҵ�Ǿ��ִ� datafile descriptor slot�� ID*/
    smiDataFileDescSlotID        mDataFileDescSlotID;

    /*bitmap extent�� datafile descriptor slot�� �Ҵ�� ����*/
    UInt                        mBmpExtSeq;

    smLSN                       mFlushLSN;

    /*bitmap extent�� ���� latch*/
    /* BUG-33899 fail to create changeTracking file in window */
    union {
        iduLatch                    mLatch;
        UChar                       mAlign[448];
    };
}smriCTBmpExtHdrBlock;

/*-------------------------------------------------------------
 * BmpBlock�� ����ü(512 bytes)
 *-----------------------------------------------------------*/
typedef struct smriCTBmpBlock
{
    /*block header*/
    smriCTBlockHdr    mBlockHdr;

    /*bitmap ����*/
    SChar             mBitmap[SMRI_CT_BMP_BLOCK_BITMAP_SIZE];
}smriCTBmpBlock;

/*-------------------------------------------------------------
 * meta extent�� ����ü
 *-----------------------------------------------------------*/
typedef struct smriCTMetaExt
{
    /*extent map block*/
    smriCTExtMapBlock       mExtMapBlock;

    /*data descriptor blocks*/
    smriCTDataFileDescBlock mDataFileDescBlock[SMRI_CT_EXT_BLOCK_CNT_EXCEPT_META_BLOCK];
}smriCTMetaExt;
 
/*-------------------------------------------------------------
 * Bitmap extent�� ����ü
 *-----------------------------------------------------------*/
typedef struct smriCTBmpExt
{
    /*bitmap extent header block*/
    smriCTBmpExtHdrBlock    mBmpExtHdrBlock;

    /*bitmap blocks*/
    smriCTBmpBlock          mBmpBlock[SMRI_CT_EXT_BLOCK_CNT_EXCEPT_META_BLOCK];
}smriCTBmpExt;
 
/*-------------------------------------------------------------
 * Empty extent�� ����ü
 *-----------------------------------------------------------*/
typedef struct smriCTEmptyExt
{
    /*empty blocks*/
    smriCTBmpBlock           mEmptyBlock[SMRI_CT_EXT_BLOCK_CNT];
}smriCTEmptyExt;

/*-------------------------------------------------------------
 * CTBody�� ����ü
 *-----------------------------------------------------------*/
typedef struct smriCTBody
{
    /* meta ���� */
    smriCTMetaExt           mMetaExtent;

    /* bitmap ���� */
    smriCTBmpExt            mBmpExtent[SMRI_CT_BMP_EXT_CNT];

    /*empty ����*/
    smriCTEmptyExt          mEmptyExtent;

    /*block������ �����ϱ� ���� self ������*/
    smriCTBmpBlock        * mBlock;
}smriCTBody;

/*BI���� �������� header offset*/
#define SMRI_BI_HDR_OFFSET                  (0)                            
/*INVALID backup info slot index*/
#define SMRI_BI_INVALID_SLOT_IDX            (ID_UINT_MAX)                  

#define SMRI_BI_INVALID_SLOT_CNT            (SMRI_BI_INVALID_SLOT_IDX)                  

#define SMRI_BI_FILE_HDR_SIZE               (512)

#define SMRI_BI_MAX_BACKUP_FILE_NAME_LEN    (SM_MAX_FILE_NAME)

#define SMRI_BI_FILE_NAME                   "backupinfo"

#define SMRI_BI_INVALID_TAG_NAME            ((SChar*)"INVALID_TAG")

#define SMRI_BI_INVALID_BACKUP_PATH         ((SChar*)"INVALID_BACKUP_PATH")

#define SMRI_BI_TIME_LEN                    (24)

#define SMRI_BI_BACKUP_TAG_PREFIX           "TAG_"

/*-------------------------------------------------------------
 * ��� ���
 *-----------------------------------------------------------*/
typedef enum smriBIBackupTarget
{
    SMRI_BI_BACKUP_TARGET_NONE,
    SMRI_BI_BACKUP_TARGET_DATABASE,
    SMRI_BI_BACKUP_TARGET_TABLESPACE
}smriBIBackupTarget;
 
typedef enum smriBIMgrState
{
    SMRI_BI_MGR_FILE_REMOVED,
    SMRI_BI_MGR_FILE_CREATED,
    SMRI_BI_MGR_INITIALIZED,
    SMRI_BI_MGR_DESTROYED
}smriBIMgrState;

/*-------------------------------------------------------------
 * logAnchor�� ����� CT���� ����
 *-----------------------------------------------------------*/
typedef struct smriBIFileAttr
{
    smLSN           mLastBackupLSN;
    smLSN           mBeforeBackupLSN;
    SChar           mBIFileName[ SM_MAX_FILE_NAME ];
    smriBIMgrState  mBIMgrState;
    SChar           mBackupDir[ SM_MAX_FILE_NAME ];
    UInt            mDeleteArchLogFileNo;
}smriBIFileAttr;

/*-------------------------------------------------------------
 * BI file header (512 bytes)
 *-----------------------------------------------------------*/
typedef struct smriBIFileHdr
{
    /*���� ����� üũ��*/
    UInt            mCheckSum;
    
    /*backupinfo slot�� ��ü ����*/
    UInt            mBISlotCnt;
    
    /*�����ͺ��̽� �̸�*/
    SChar           mDBName[SM_MAX_DB_NAME];
    
    /*����� �Ϸ�� LSN*/
    smLSN           mLastBackupLSN;

    UChar           mAlign[368];
} smriBIFileHdr;

/*-------------------------------------------------------------
 * BI slot (676 bytes )
 *-----------------------------------------------------------*/
typedef struct smriBISlot
{
    /*backupinfo slot�� üũ��*/
    UInt                mCheckSum;

    /*��� ���� �ð�*/
    UInt                mBeginBackupTime;

    /*��� �Ϸ� �ð�*/
    UInt                mEndBackupTime;

    /*���Ͽ� ����� incremental backup chunk�� ��*/
    UInt                mIBChunkCNT;

    /*��� ���(database or table space)*/
    smriBIBackupTarget  mBackupTarget;

    /*��� ���� level*/
    smiBackupLevel      mBackupLevel;

    /*incremental backup���� ũ��*/
    UInt                mIBChunkSize;

    /*��� Ÿ��(full, differential, cumulative)*/
    UShort              mBackupType;

    /*����� ������������ table space ID*/
    scSpaceID           mSpaceID;

    union
    {
        /*disk table space�� ���� ID*/
        sdFileID        mFileID;

        /*memory table space�� ���� num*/
        UInt            mFileNo;
    };

    /*������� ������ ���� ���� ũ��*/
    ULong               mOriginalFileSize;

    /* chkpt image�� ���� �������� ����
     * control�ܰ迡���� Membase�� �ε���� ���� ���¶� Membase�� ���� ������
     * �Ұ��ϴ�. incremental backup ����� Membase�� mDBFilePageCnt����
     * �����Ѵ�.
     */
    vULong              mDBFilePageCnt;

    /*����� �±��̸�*/
    SChar               mBackupTag[SMI_MAX_BACKUP_TAG_NAME_LEN];

    /*��� ������ �̸�*/
    SChar               mBackupFileName[SMRI_BI_MAX_BACKUP_FILE_NAME_LEN];
}smriBISlot;


#endif /*_O_SMRI_DEF_H_*/

