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
 * $Id: sddDef.h 88191 2020-07-27 03:08:54Z mason.lee $
 *
 * Description :
 *
 * �� ������ Resource Layer �ڷᱸ���� ��� �����̴�.
 *
 **********************************************************************/

#ifndef _O_SDD_DEF_H_
#define _O_SDD_DEF_H_ 1

#include <idu.h>
#include <smu.h>
#include <sctDef.h>
#include <smriDef.h>

/* --------------------------------------------------------------------
 * Description : tablespace�� ���� �ؽ� ���̺��� ũ��
 * ----------------------------------------------------------------- */
#define SDD_HASH_TABLE_SIZE  (128)

/* ------------------------------------------------
 * IO ��� : sddDiskMgr::completeIO���� ó����
 * ----------------------------------------------*/
typedef enum
{
    SDD_IO_READ  = 0,
    SDD_IO_WRITE
} sddIOMode;

#define SDD_CALC_PAGEOFFSET(aPageCnt) \
        (SM_DBFILE_METAHDR_PAGE_OFFSET + SM_DBFILE_METAHDR_PAGE_SIZE + \
        (aPageCnt * SD_PAGE_SIZE))

typedef enum sddSyncType
{
    /* ��ũ ���� �Ҵ�/���� �� ���濡 ���� Ÿ�� */
    SDD_SYNC_NORMAL = 0,
    SDD_SYNC_CHKPT
} sddSyncType;

// ��ũ ����Ÿ������ ��Ÿ���
typedef struct sddDataFileHdr
{
    UInt    mSmVersion;

     // �̵����� ���� RedoLSN
    smLSN   mRedoLSN;

     // �̵����� ���� CreateLSN
    smLSN   mCreateLSN;

     // �̵����� ���� DiskLstLSN
    smLSN   mMustRedoToLSN;

    // PROJ-2133 incremental backup
    smiDataFileDescSlotID    mDataFileDescSlotID;
    
    // PROJ-2133 incremental backup
    // incremental backup�� ���Ͽ��� �����ϴ� ����
    smriBISlot  mBackupInfo;

} sddDataFileHdr;

/* --------------------------------------------------------------------
 * Description : ����Ÿ ȭ�Ͽ� ���� ����
 * ----------------------------------------------------------------- */
typedef struct sddDataFileNode
{
    // tablespace���� �����ϰ� �ĺ��Ǵ� ����Ÿ ȭ���� ID
    scSpaceID        mSpaceID;
    sdFileID         mID;
    // state of the data file node(not used, but will be used by msjung)
    UInt             mState;
    sddDataFileHdr   mDBFileHdr;
    /* ------------------------------------------------
     * ������ mNextSize ���� mIsAutoExtend �Ӽ��� sddDataFileAttr����
     * ���ǵǾ� �ִ�. �ֳ��ϸ�, �α׾�Ŀ�� ����� sddDataFileAttr��
     * �о datafile ��带 �ʱ�ȭ�ϵ��� �ϱ� �����̴�.
     * ------------------------------------------------ */
    ULong            mNextSize;       // Ȯ��� ������ ����
    ULong            mMaxSize;        // �ִ� ������ ����
    ULong            mInitSize;       // �ʱ� ������ ����

    /* ------------------------------------------------
     * - ����Ÿ������ ���� ������ ����
     * ó���� INIT SIZE�� �Ҵ�ǰ� ȭ���� autoextend, resize�ɶ�
     * Ȯ��ȴ�.
     * startup�� �� ũ��� ���� ȭ���� ũ�⸦ ���� �ʿ䰡 ����.
     * ȭ�� Ȯ�� ��, �α׾�Ŀ�� ���� ���� �ý����� abort�� �߻��ϴ���
     * Ȯ��� �κ��� �ʱ�ȭ�� �Ϸ���� ���� ��Ȳ�̶� ������� ���Ѵ�.
     * ���� restart recovery�ÿ� undo ó���������� �ٽ� Ȯ�忡 ����
     * �䱸������ �߻��ϰų� ���� �����Ŀ� ��Ȯ�� �� �ʱ�ȭ�� �õ���
     * �� �ִ�.
     * ----------------------------------------------*/
    ULong             mCurrSize;
    smiDataFileMode   mCreateMode;     // datafile ���� ���
    SChar*            mName;           // ȭ�� �̸�
    idBool            mIsAutoExtend;   // �ڵ�Ȯ�� ����
    UInt              mIOCount;        // IO�� ���� ��� ���� ���ΰ�
    idBool            mIsOpened;       // Open �� ���¿���
    idBool            mIsModified;     // ������ �������� �� flush�Ҷ� �ʿ�
    smuList          mNode4LRUList;   // open�� datafile ����Ʈ
    iduFile           mFile;           // ����Ÿ ȭ��

    UInt              mAnchorOffset;    // Loganchor �޸� ���۳��� DBF �Ӽ� ��ġ

    UChar*            mPageBuffPtr;
    UChar*            mAlignedPageBuff;

    iduMutex          mMutex;           // File�� Open�� Close�׸��� IOCount IsOpened �� ��ȣ�Ѵ�.
} sddDataFileNode;

/* ------------------------------------------------
 * Description : tablespace ������ �����ϴ� �ڷᱸ��
 *
 * �������� tablespace �� ���� �޸� ��带 ǥ���ϴ�
 * �ڷᱸ���̴�.
 * ----------------------------------------------*/
typedef struct sddTableSpaceNode
{
    sctTableSpaceNode  mHeader;

    /* PROJ-1671 Bitmap-based Tablespace And Segment
     * Space Management */
    void              * mSpaceCache;     /* Space Cache */

    /* loganchor�κ��� �ʱ�ȭ�ǰų�, ���̺����̽� ������ ������ */
    smiExtMgmtType     mExtMgmtType;     /* Extemt�� �������� ��� */
    smiSegMgmtType     mSegMgmtType;     /* Segment�� �������� ��� */
    UInt               mExtPageCount;    /* Extent ������ ���� */

    // tablespace �Ӽ� Flag
    // ( ex> Tablespace���� ������ ���濡 ���� Log Compress���� )
    UInt               mAttrFlag; 

    sdFileID           mNewFileID; // tablespace�� ���� ����Ÿȭ�Ͽ� id�ο�

    UInt               mDataFileCount; // ���̺����̽��� �Ҽӵ� ���ϰ���

    //PRJ-1671  Bitmap-based Tablespace And Segment Space Management
    //data file node�� ��ҷ� ���� �迭
    sddDataFileNode  * mFileNodeArr[ SD_MAX_FID_COUNT] ; 

    ULong              mTotalPageCount;     // TBS�� ���Ե� �� ������ ����

    /* fix BUG-17456 Disk Tablespace online���� update �߻��� index ���ѷ���
     * �ʱⰪ�� (0,0)�̸�, Online �ÿ��� Online TBS LSN�� ��ϵȴ�. */
    smLSN              mOnlineTBSLSN4Idx;
    
    /* fix BUG-24403 Disk Tablespace online���� hang �߻� ������ ����
     * offline �Ҷ� SMO ���� �����Ѵ�. */
    ULong              mMaxSmoNoForOffline;

    UInt               mAnchorOffset;       // Loganchor �޸� ���۳��� TBS �Ӽ� ��ġ
} sddTableSpaceNode;

typedef IDE_RC (*sddReadPageFunc)(idvSQL          * aStatistics,
                                  sddDataFileNode * aFileNode,
                                  scPageID          aFstPID,
                                  ULong             aPageCnt,
                                  UChar           * aBuffer,
                                  UInt            * aState );

typedef IDE_RC (*sddWritePageFunc)(idvSQL          * aStatistics,
                                   sddDataFileNode * aFileNode,
                                   scPageID          aPageID,
                                   UChar           * aBuffer,
                                   UInt            * aState );

/* DoubleWrite File Prefix ���� */
#define   SDD_DWFILE_NAME_PREFIX     "dwfile"

/* DoubleWrite File Prefix ����2 */
#define   SDD_SBUFFER_DWFILE_NAME_PREFIX    "sdwfile"


#endif // _O_SDD_DEF_H_
