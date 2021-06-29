/** 
 *  Copyright (c) 1999~2017, Altibase Corp. and/or its affiliates. All rights reserved.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License, version 3,
 *  as published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
 
#include <idu.h>
#include <idf.h>
#include <idfMemory.h>

//========================
// �Լ� ������ (I/O APIs)
//========================
idfopen      idf::open      = idf::idlopen;
idfclose     idf::close     = idlOS::close;
idfread      idf::read      = &idlOS::read;
idfwrite     idf::write     = &idlOS::write;
idfpread     idf::pread     = idlOS::pread;
idfpwrite    idf::pwrite    = idlOS::pwrite;
idfreadv     idf::readv     = idlOS::readv;
idfwritev    idf::writev    = idlOS::writev;
#if !defined(PDL_LACKS_PREADV)
idfpreadv    idf::preadv    = idlOS::preadv;
idfpwritev   idf::pwritev   = idlOS::pwritev;
#endif
idfcreat     idf::creat     = idlOS::creat;
idflseek     idf::lseek     = idlOS::lseek;
idffstat     idf::fstat     = idlOS::fstat;
idfunlink    idf::unlink    = idlOS::unlink;
idfrename    idf::rename    = idlOS::rename;
idfaccess    idf::access    = idlOS::access;
idffsync     idf::fsync     = idlOS::fsync;
idfftruncate idf::ftruncate = idlOS::ftruncate;
idfopendir   idf::opendir   = idlOS::opendir;
idfreaddir_r idf::readdir_r = idlOS::readdir_r;
idfreaddir   idf::readdir   = idlOS::readdir;
idfclosedir  idf::closedir  = idlOS::closedir;
idffilesize  idf::filesize  = idlOS::filesize;
idfgetDiskFreeSpace idf::getDiskFreeSpace = idlVA::getDiskFreeSpace;

//===============================================
// ��� ����
//     1. File Handle
//     2. File Name
//     3. System
//     4. Setting
//     5. Mutex
//===============================================

//==============================
// 1-1. File Handle
// 1-2. File Handle for Logging
//==============================
PDL_HANDLE idf::mFd    = PDL_INVALID_HANDLE;
PDL_HANDLE idf::mLogFd = PDL_INVALID_HANDLE;

//============================
// 2-1. File Name
// 2-2. File Name for Logging
//============================
SChar *idf::mFileName    = NULL;
SChar *idf::mLogFileName = NULL;

// 3. System
//===============================================
// Block Map�� 1byte�� 1���� Block�� ����Ų��.
// �������� �Ҵ��� �� Block�� ũ�⸸ŭ �Ҵ��Ѵ�.
//===============================================
UChar        *idf::mBlockMap = NULL;
SInt         *idf::mBlockList = NULL;
UChar        *idf::mEmptyBuf = NULL;
UInt          idf::mFileOpenCount = 0;
idfMaster     idf::mMaster;
idfFdEntry   *idf::mFdList   = NULL;
idfMetaEntry *idf::mMetaList = NULL;
idfPageMap   *idf::mPageMapPool = NULL;
UInt          idf::mPageMapPoolCount;
UInt         *idf::mCRCTable = NULL; // 256 bytes CRC Table
idfLogHeader *idf::mLogHeader = NULL;

UChar *idf::mLogBuffer = NULL;
UInt   idf::mLogBufferCursor;
UInt   idf::mTransCount;
UChar *idf::mLogList  = NULL;
UInt   idf::mLogListCount;

idfMapHeader *idf::mMapHeader = NULL;
UInt         *idf::mMapLog = NULL;
UInt          idf::mMapLogCount;

UInt idf::mPageNum;
UInt idf::mPageSize;
UInt idf::mPagesPerBlock;
UInt idf::mMaxFileOpenCount;
UInt idf::mMaxFileCount;
UInt idf::mSystemSize;

UInt idf::mDirectPageNum;
UInt idf::mIndPageNum;
UInt idf::mIndMapSize;
UInt idf::mIndEntrySize;
UInt idf::mMapPoolSize;
SInt idf::mMetaSize;
SInt idf::mMasterSize;

idBool idf::mIsSync;
idBool idf::mIsDirty;

UChar idf::mLogMode;

PDL_thread_mutex_t  idf::mMutex;
PDL_OS::pdl_flock_t idf::mLockFile;

idfMemory gAlloca;

UChar idf::mRecCheck = IDF_END_RECOVERY;

void idf::initWithIdlOS()
{
    idf::open      = idf::idlopen;
    idf::close     = idlOS::close;
#if defined(WRS_VXWORKS)
//fix assuming & on overloaded member function error
    idf::read      = &idlOS::read;
    idf::write     = &idlOS::write;
#else
    idf::read      = idlOS::read;
    idf::write     = idlOS::write;
#endif
    idf::readv     = idlOS::readv;
    idf::writev    = idlOS::writev;
#if !defined(PDL_LACKS_PREADV)
    idf::preadv    = idlOS::preadv;
    idf::pwritev   = idlOS::pwritev;
#endif
    idf::creat     = idlOS::creat;
    idf::lseek     = idlOS::lseek;
    idf::fstat     = idlOS::fstat;
    idf::unlink    = idlOS::unlink;
    idf::rename    = idlOS::rename;
    idf::access    = idlOS::access;
    idf::fsync     = idlOS::fsync;
    idf::ftruncate = idlOS::ftruncate;
    idf::opendir   = idlOS::opendir;
    idf::readdir_r = idlOS::readdir_r;
    idf::readdir   = idlOS::readdir;
    idf::closedir  = idlOS::closedir;
#if defined(WRS_VXWORKS)
//fix assuming & on overloaded member function error
    idf::filesize  = &idlOS::filesize;
#else
    idf::filesize  = idlOS::filesize;
#endif
    idf::getDiskFreeSpace = idlVA::getDiskFreeSpace;
}

void idf::initWithIdfCore()
{
    idf::open      = idfCore::open;
    idf::close     = idfCore::close;
    idf::read      = idfCore::read;
    idf::write     = idfCore::write;
    idf::readv     = idlOS::readv;
    idf::writev    = idlOS::writev;
#if !defined(PDL_LACKS_PREADV)
    idf::preadv    = idlOS::preadv;
    idf::pwritev   = idlOS::pwritev;
#endif
    idf::creat     = idfCore::creat;
    idf::lseek     = idfCore::lseek;
    idf::fstat     = idfCore::fstat;
    idf::unlink    = idfCore::unlink;
    idf::rename    = idfCore::rename;
    idf::access    = idfCore::access;
    idf::fsync     = idfCore::fsync;
    idf::ftruncate = idfCore::ftruncate;
    idf::opendir   = idfCore::opendir;
    idf::readdir_r = idfCore::readdir_r;
    idf::readdir   = idfCore::readdir;
    idf::closedir  = idfCore::closedir;
    idf::filesize  = idfCore::filesize;
    idf::getDiskFreeSpace = idfCore::getDiskFreeSpace;

    //===================================================================
    // ���� �������� default�� �����Ѵ�.
    // ������ �����ϰ�, Master Page�� ��ȿ�ϸ� Master�� ������ �����Ѵ�.
    //===================================================================
    idf::mPageNum          = IDF_PAGE_NUM;
    idf::mPageSize         = IDF_PAGE_SIZE;
    idf::mPagesPerBlock    = IDF_PAGES_PER_BLOCK;
    idf::mMaxFileOpenCount = IDF_MAX_FILE_OPEN_COUNT;
    idf::mMaxFileCount     = IDF_MAX_FILE_COUNT;;

    idf::mMapPoolSize      = IDF_MAP_POOL_SIZE;
    idf::mDirectPageNum    = IDF_DIRECT_PAGE_NUM;
    idf::mIndPageNum       = IDF_INDIRECT_PAGE_NUM;
    idf::mIndMapSize       = IDF_INDIRECT_MAP_SIZE;
    idf::mIndEntrySize     = IDF_INDIRECT_ENTRY_SIZE;

    idf::mMetaSize   = ID_SIZEOF(idfMeta);
    idf::mMasterSize = ID_SIZEOF(idfMaster);

    idf::mIsSync = ID_FALSE;
    idf::mIsDirty = ID_FALSE;

    idf::mLogMode = IDF_DEFAULT_LOG_MODE;

    idf::mTransCount   = 0;

    idf::mRecCheck = IDF_END_RECOVERY;
}

void idf::initMasterPage(idfMaster *aMaster)
{
    if(idf::mFd != PDL_INVALID_HANDLE)
    {
        // aMaster�� NULL�̸� �⺻ ������ idf::mMaster�� �����Ѵ�.
        if(aMaster == NULL)
        {
            idf::mMaster.mMajorVersion = IDF_MAJOR_VERSION;
            idf::mMaster.mMinorVersion = IDF_MINOR_VERSION;

            idf::mMaster.mNumOfPages = IDF_PAGE_NUM;
            idf::mMaster.mSizeOfPage = IDF_PAGE_SIZE;
            idf::mMaster.mPagesPerBlock = IDF_PAGES_PER_BLOCK;
            idf::mMaster.mFSMode = 0;

            idf::mMaster.mLogMode = IDF_DEFAULT_LOG_MODE;
            idf::mMaster.mNumOfFiles = 0;
            idf::mMaster.mMaxFileCount = IDF_MAX_FILE_COUNT;
            idf::mMaster.mMaxFileOpenCount = IDF_MAX_FILE_OPEN_COUNT;
            idf::mMaster.mSignature = idlOS::hton(IDF_SYSTEM_SIGN);
        }
        // aMaster�� NULL�� �ƴϸ� aMaster������ idf::mMaster��
        // idf�� ������ �ʱ�ȭ �Ѵ�.
        else
        {
            // ����ڷ� ���� ���� Master�� �ʱ�ȭ �Ѵ�.
            if(&(idf::mMaster) != aMaster)
            {
                idf::mMaster.mMajorVersion = IDF_MAJOR_VERSION;
                idf::mMaster.mMinorVersion = IDF_MINOR_VERSION;

                if(idf::mMaster.mNumOfPages == 0)
                {
                    idf::mMaster.mNumOfPages = IDF_PAGE_NUM;
                }
                if(idf::mMaster.mSizeOfPage == 0)
                {
                    idf::mMaster.mSizeOfPage = idf::mPageSize;
                }

                if((idf::mMaster.mFSMode != 0) && (idf::mMaster.mFSMode != 1))
                {
                    idf::mMaster.mFSMode = 0;
                }

                if((idf::mMaster.mLogMode == IDF_NO_LOG_MODE) ||
                   (idf::mMaster.mLogMode == IDF_LOG_MODE))
                {
                    idf::mMaster.mLogMode = IDF_DEFAULT_LOG_MODE;
                }

                idf::mMaster.mNumOfFiles = 0;

                idf::mMaster.mTimestamp = 0;
                // idf::mMaster.mPagesPerBlock�� 4�� ������� �Ѵ�.
                if((idf::mMaster.mPagesPerBlock == 0) ||
                   (idf::mMaster.mPagesPerBlock % 4 != 0))
                {
                    idf::mMaster.mPagesPerBlock = idf::mPagesPerBlock;
                }

                if(idf::mMaster.mMaxFileCount == 0)
                {
                    idf::mMaster.mMaxFileCount = IDF_MAX_FILE_COUNT;
                }
                if(idf::mMaster.mMaxFileOpenCount == 0)
                {
                    idf::mMaster.mMaxFileOpenCount = IDF_MAX_FILE_OPEN_COUNT;
                }

                idf::mMaster.mSignature = idlOS::hton(IDF_SYSTEM_SIGN);
            }

        }

        if((aMaster != NULL) & (&(idf::mMaster) == aMaster))
        {   
            idf::mMaster.mNumOfFiles       = idlOS::ntoh(idf::mMaster.mNumOfFiles);
            idf::mMaster.mMajorVersion     = idlOS::ntoh(idf::mMaster.mMajorVersion);
            idf::mMaster.mMinorVersion     = idlOS::ntoh(idf::mMaster.mMinorVersion);
            idf::mMaster.mAllocedPages     = idlOS::ntoh(idf::mMaster.mAllocedPages);
            idf::mMaster.mNumOfPages       = idlOS::ntoh(idf::mMaster.mNumOfPages);
            idf::mMaster.mSizeOfPage       = idlOS::ntoh(idf::mMaster.mSizeOfPage);
            idf::mMaster.mPagesPerBlock    = idlOS::ntoh(idf::mMaster.mPagesPerBlock);
            idf::mMaster.mMaxFileOpenCount = idlOS::ntoh(idf::mMaster.mMaxFileOpenCount);
            idf::mMaster.mMaxFileCount     = idlOS::ntoh(idf::mMaster.mMaxFileCount);
        }
        
        // ���� ����Ÿ ���� ������ Master �ý��� ���� �ʱ�ȭ.
        idf::mPageNum          = idf::mMaster.mNumOfPages;
        idf::mPageNum          = idf::mMaster.mNumOfPages;
        idf::mPageSize         = idf::mMaster.mSizeOfPage;
        idf::mPagesPerBlock    = idf::mMaster.mPagesPerBlock;
        idf::mMaxFileOpenCount = idf::mMaster.mMaxFileOpenCount;
        idf::mMaxFileCount     = idf::mMaster.mMaxFileCount;
        idf::mIndMapSize       = idf::mMaster.mSizeOfPage / ID_SIZEOF(UInt);
        idf::mIndEntrySize     = ID_SIZEOF(UInt) * idf::mMaster.mPagesPerBlock;
        idf::mLogMode          = idf::mMaster.mLogMode;

        idf::mSystemSize = IDF_FIRST_META_POSITION;

        idf::mSystemSize += idf::mMaxFileCount * IDF_META_SIZE;

        if((idf::mSystemSize % idf::mPageSize) == 0)
        {
            idf::mSystemSize = (idf::mSystemSize / idf::mPageSize);
        }
        else
        {
            idf::mSystemSize = (idf::mSystemSize / idf::mPageSize) + 1;
        }
        if((idf::mSystemSize % idf::mPagesPerBlock) == 0)
        {
            idf::mSystemSize = (idf::mSystemSize / idf::mPagesPerBlock);
        }
        else
        {
            idf::mSystemSize = (idf::mSystemSize / idf::mPagesPerBlock) + 1;
        }

        if((aMaster == NULL) || (&(idf::mMaster) != aMaster))
        {
            idf::mMaster.mAllocedPages = idf::mSystemSize * idf::mPagesPerBlock;
        }
    }
}

//===================================================================
// File Descriptor ����Ʈ�� �ʱ�ȭ �Ѵ�.
// ���� �ý����� �ʱ�ȭ �Ǹ� FD�� ��� ������� ���� �����̱� ������
// ��� 0���� �ʱ�ȭ �Ѵ�.
//===================================================================
void idf::initFdList()
{
    UInt sIndex;
    UInt sFdListSize = (UInt)(ID_SIZEOF(idfFdEntry) * idf::mMaxFileOpenCount);

    (void)idf::alloc((void **)&idf::mFdList, sFdListSize);

    for(sIndex = 0; sIndex < idf::mMaxFileOpenCount; sIndex++)
    {
        idf::mFdList[sIndex].mIsUsed = IDF_FD_UNUSED;
        idf::mFdList[sIndex].mFileID = IDF_INVALID_FILE_ID;
        idf::mFdList[sIndex].mCursor = 0;
    }
}

//============================
// Meta�� �о �ʱ�ȭ �Ѵ�.
//============================
IDE_RC idf::initMetaList()
{
    UInt     sIndex;
    UInt     sOffsetIndex;
    UInt     sMapIndex;
    UInt     sPageIndex;
    UInt     sBlockIndex;
    UInt     sBlockOffset;
    UInt     sCount  = 0;
    UInt    *sMap    = NULL;
    UInt    *sIndMap = NULL;
    idfMeta *sMeta   = NULL;
    UInt     sNum;

    (void)idf::alloc((void **)&idf::mMetaList, 
                     ID_SIZEOF(idfMetaEntry) * idf::mMaxFileCount);

    (void)idf::alloc((void **)&sMeta, idf::mMetaSize);

    (void)idf::alloc((void **)&sIndMap, idf::mPageSize);

    for(sIndex = 0; sIndex < idf::mMaxFileCount; sIndex++)
    {
        IDE_TEST(idf::lseekFs(idf::mFd,
                              IDF_FIRST_META_POSITION + sIndex * IDF_META_SIZE,
                              SEEK_SET) == -1);

        IDE_TEST(idf::readFs(idf::mFd, 
                             sMeta, 
                             idf::mMetaSize) < idf::mMetaSize);

        sMeta->mFileID      = idlOS::ntoh(sMeta->mFileID);
        sMeta->mSize        = idlOS::ntoh(sMeta->mSize);
        sMeta->mCTime       = idlOS::ntoh(sMeta->mCTime);
        sMeta->mNumOfPagesU = idlOS::ntoh(sMeta->mNumOfPagesU);
        sMeta->mNumOfPagesA = idlOS::ntoh(sMeta->mNumOfPagesA);
        sMeta->mCRC         = idlOS::ntoh(sMeta->mCRC);        

        for(sNum = 0; sNum < IDF_PAGES_PER_BLOCK; sNum++)
        {
            sMeta->mDirectPages[sNum] = idlOS::ntoh(sMeta->mDirectPages[sNum]);
        }

        for(sNum = 0; sNum < IDF_INDIRECT_PAGE_NUM; sNum++)
        {
            sMeta->mIndPages[sNum] = idlOS::ntoh(sMeta->mIndPages[sNum]);
        }

        if(sMeta->mSignature != idlOS::ntoh(IDF_SYSTEM_SIGN))
        {
            idf::mMetaList[sIndex].mMeta = NULL;

            continue;
        }

        idf::mMetaList[sIndex].mMeta = sMeta;
        idf::mMetaList[sIndex].mFileOpenCount = 0;

        sMap = &(sMeta->mDirectPages[0]);

        //======================
        // Direct Page Map �˻�
        //======================
        for(sMapIndex = 0;
            sMapIndex < idf::mDirectPageNum;
            sMapIndex += idf::mPagesPerBlock)
        {
            sBlockIndex = sMap[sMapIndex] & IDF_FILE_HOLE_INVERTER;

            sBlockIndex /= idf::mPagesPerBlock;

            if(sBlockIndex != 0)
            {
                IDE_ASSERT(idf::mBlockMap[sBlockIndex] == 0x00);

                idf::mBlockMap[sBlockIndex] = 0xFF;

                idf::mBlockList[sBlockIndex] = sIndex;
            }
        }

        //========================
        // Indirect Page Map �˻�
        //========================

        sMap = &(sMeta->mIndPages[0]);

        for(sMapIndex = 0; sMapIndex < idf::mIndPageNum; sMapIndex++)
        {
            if(sMap[sMapIndex] == 0)
            {
                continue;
            }

            // Indirect Map�� �Ҵ�Ǿ� �ִ�.
            sPageIndex = sMap[sMapIndex] & IDF_FILE_HOLE_INVERTER;

            sBlockOffset = sPageIndex % idf::mPagesPerBlock;

            sBlockIndex = sPageIndex / idf::mPagesPerBlock;

            if(sBlockIndex != 0 && sBlockOffset == 0)
            {
                IDE_ASSERT(idf::mBlockMap[sBlockIndex] == 0x00);

                idf::mBlockMap[sBlockIndex] = 0xFF;

                idf::mBlockList[sBlockIndex] = sIndex;
            }

            // Indirect Map�� ������� ���� ��� ���� Map�� �����Ѵ�.
            if((sMap[sMapIndex] & IDF_FILE_HOLE_MASK) != 0)
            {
                continue;
            }

            // Indirect Map�� �о Block Map�� �ʱ�ȭ �Ѵ�.
            IDE_TEST(idf::lseekFs(idf::mFd,
                                  sPageIndex * idf::mPageSize,
                                  SEEK_SET) == -1);

            IDE_TEST(idf::readFs(idf::mFd,
                                 sIndMap,
                                 idf::mPageSize) != idf::mPageSize);

            for(sOffsetIndex = 0;
                sOffsetIndex < idf::mIndMapSize;
                sOffsetIndex += idf::mPagesPerBlock)
            {
                sIndMap[sOffsetIndex] = idlOS::ntoh(sIndMap[sOffsetIndex]);
            }

            for(sOffsetIndex = 0;
                sOffsetIndex < idf::mIndMapSize;
                sOffsetIndex += idf::mPagesPerBlock)
            {
                if(sIndMap[sOffsetIndex] == 0)
                {
                    continue;
                }

                sBlockIndex = sIndMap[sOffsetIndex] & IDF_FILE_HOLE_INVERTER;

                sBlockIndex /= idf::mPagesPerBlock;

                if(sBlockIndex != 0)
                {
                    IDE_ASSERT(idf::mBlockMap[sBlockIndex] == 0x00);

                    idf::mBlockMap[sBlockIndex] = 0xFF;

                    idf::mBlockList[sBlockIndex] = sIndex;
                }
            }
        }

        (void)idf::alloc((void **)&sMeta, idf::mMetaSize);

        sCount++;
    }

    // ���� Meta�� ������ Master�� ��ϵ� Meta�� ������ �ٸ��� ����
    IDE_TEST(sCount != idf::mMaster.mNumOfFiles);

    (void)idf::free(sIndMap);

    (void)idf::free(sMeta);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if(sIndMap != NULL)
    {
        (void)idf::free(sIndMap);
    }

    if(sMeta != NULL)
    {
        (void)idf::free(sMeta);
    }

    return IDE_FAILURE;
}

//===========================================================
// Meta�� �ʱ�ȭ �Ѵ�.
// initMetaList�� Meta�� �о �ʱ�ȭ ������,
// initMetaList2�� �� ó�� ���Ͻý����� ������ �� ȣ���ϹǷ�
// ��� 0���� �ʱ�ȭ �Ѵ�.
//===========================================================
IDE_RC idf::initMetaList2()
{
    UInt   sIndex;
    UInt   sSize;
    UInt   sSystemSize;

    sSize = ID_SIZEOF(idfMetaEntry) * idf::mMaxFileCount;
    
    (void)idf::alloc((void **)&idf::mMetaList, sSize);

    for(sIndex = 0; sIndex < idf::mMaxFileCount; sIndex++)
    {
        idf::mMetaList[sIndex].mMeta = NULL;
        idf::mMetaList[sIndex].mFileOpenCount = 0;
    }

    sSystemSize = idf::mSystemSize * idf::mPagesPerBlock;

    IDE_TEST(idf::lseekFs(mFd,
                          0,
                          SEEK_SET) == -1);

    for(sIndex = 0; sIndex < sSystemSize; sIndex++)
    {
        IDE_TEST(idf::writeFs(mFd,
                              idf::mEmptyBuf,
                              idf::mPageSize) != idf::mPageSize);
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

//=======================================================================
// initFileName()�� ���� ���Ͻý����� �̸��� �α������� �̸��� �����Ѵ�.
// �� �Լ��� initializeStatic() �Լ������� ȣ��ȴ�.
// ���� �޸� �Ҵ� �߿� �״� ��� �޸� ������ initializeStatic()
// �Լ����� ó���Ѵ�.
//=======================================================================
void idf::initFileName(const SChar * aFileName)
{
    SInt sFileNameLen = idlOS::strlen(aFileName) + 1;

    idf::alloc((void **)&idf::mFileName, sFileNameLen);
    
    idlOS::strncpy(idf::mFileName, 
                   aFileName, 
                   sFileNameLen);

    // aFileName�� '.log'�� �ٿ� log file �̸��� �����.
    sFileNameLen += 4;

    idf::alloc((void **)&idf::mLogFileName, sFileNameLen);

    idlOS::snprintf(idf::mLogFileName, sFileNameLen, "%s.log", aFileName);
}

void idf::initMutex()
{
    IDE_ASSERT(idlOS::thread_mutex_init(&idf::mMutex) == 0);
}

void idf::initPageMapPool()
{
    UInt sIndex;
    UInt sSize;
    idfPageMap *sPageMap;

    sSize = ID_SIZEOF(idfPageMap);

    (void)idf::alloc((void **)&idf::mPageMapPool, sSize);

    idf::mPageMapPool->mMap = NULL;
    idf::mPageMapPool->mFileID = IDF_INVALID_FILE_ID;

    (void)idf::alloc((void **)&sPageMap, sSize);

    sPageMap->mMap = NULL;
    sPageMap->mFileID = IDF_INVALID_FILE_ID;

    sPageMap->mPrev = idf::mPageMapPool;
    sPageMap->mNext = idf::mPageMapPool;

    idf::mPageMapPool->mNext = sPageMap;
    idf::mPageMapPool->mPrev = sPageMap;

    for(sIndex = 2; sIndex < idf::mMapPoolSize; sIndex++)
    {
        (void)idf::alloc((void **)&sPageMap, sSize);

        sPageMap->mMap = NULL;
        sPageMap->mFileID = IDF_INVALID_FILE_ID;

        sPageMap->mNext = idf::mPageMapPool->mNext;
        sPageMap->mPrev = idf::mPageMapPool;

        idf::mPageMapPool->mNext->mPrev = sPageMap;
        idf::mPageMapPool->mNext = sPageMap;
    }

    idf::mPageMapPoolCount = 0;
}

//======================================================================
// initBlockMap�� ��� ������ŭ ����Ʈ�� ����� 0���� �ʱ�ȭ �Ѵ�.
// Block Map�� �� �Լ������� ������ �ϰ�, initMeta���� Meta�� �����鼭
// ����� ��Ͽ� ���ؼ� üũ�� �Ѵ�.
// �ý����� ����� ����(Master + Meta)�� initBlockMap���� ����� ������
// ǥ���Ѵ�.
//======================================================================
void idf::initBlockMap()
{
    SInt sMapSize = idf::mPageNum / idf::mPagesPerBlock;

    (void)idf::alloc((void **)&idf::mBlockMap, sMapSize);

    idlOS::memset(idf::mBlockMap, 
                  0x00, 
                  sMapSize);

    (void)idf::alloc((void**)&idf::mBlockList, sMapSize * ID_SIZEOF(SInt));

    idlOS::memset(idf::mBlockList,
                  0xFF,
                  sMapSize * ID_SIZEOF(SInt));

    //================================================================
    // System Page ũ�� ��ŭ�� ����� ������ �����Ͽ� ������ ��������
    // �Ҵ����� ���ϵ��� �Ѵ�.
    //================================================================
    idlOS::memset(idf::mBlockMap, 
                  0xFF, 
                  idf::mSystemSize);
}

void idf::initLogList()
{
    (void)idf::alloc((void **)&idf::mLogBuffer, IDF_LOG_BUFFER_SIZE);

    (void)idf::alloc((void **)&idf::mLogList, idf::mMaxFileCount);

    (void)idf::alloc((void **)&idf::mMapHeader, 
                     ID_SIZEOF(idfMapHeader) * IDF_PAGE_MAP_LOG_SIZE);

    (void)idf::alloc((void **)&idf::mMapLog, 
                     idf::mIndEntrySize * IDF_PAGE_MAP_LOG_SIZE);

    idlOS::memset(idf::mLogList, 
                  0x00, 
                  idf::mMaxFileCount);

    idf::mLogBufferCursor = 0;

    idf::mLogListCount = 0;

    idf::mMapLogCount = 0;
}

//========================================================================
// open()���� ������ �� �� FD ����Ʈ�� ��ȸ�Ͽ� �� FD�� ã�� ��ȯ�� �ش�.
//========================================================================
SInt idf::getUnusedFd()
{
    UInt sIndex;
    SInt sFd = IDF_INVALID_HANDLE;

    if(idf::mFileOpenCount < idf::mMaxFileOpenCount)
    {   
        for(sIndex = 0; sIndex < idf::mMaxFileOpenCount; sIndex++)
        {
            if(idf::mFdList[sIndex].mIsUsed == IDF_FD_UNUSED)
            {
                sFd = sIndex;

                idf::mFdList[sIndex].mIsUsed = IDF_FD_USED;
                idf::mFileOpenCount++;

                break;
            }
        }
    }   

    return sFd;
}

//=====================================
// ������ �̸����� ������ ID�� ��´�.
// ���� ID�� Meta�� 1:1�� ��Ī�ȴ�.
//=====================================
SInt idf::getFileIDByName(const SChar *aPathName)
{
    UInt   sIndex;
    UInt   sPathNameLen;
    UInt   sCount = 0;
    SInt   sFileID = IDF_INVALID_FILE_ID;
    SChar *sPathName = NULL;

    sPathName = getpath(aPathName);

    IDE_TEST(sPathName == NULL);

    sPathNameLen = idlOS::strlen(sPathName);

    for(sIndex = 0;
        (sIndex < idf::mMaxFileCount) && (sCount < idf::mMaster.mNumOfFiles);
        sIndex++)
    {
        if(idf::mMetaList[sIndex].mMeta == NULL)
        {
            continue;
        }

        sCount++;

        if((idlOS::strncmp(sPathName,
                           idf::mMetaList[sIndex].mMeta->mFileName,
                           sPathNameLen))
           == 0)
        {
            if(sPathNameLen == 
                    idlOS::strlen(idf::mMetaList[sIndex].mMeta->mFileName))
            {
                sFileID = sIndex;
                break;
            }
        }
    }

    if(sPathName != NULL)
    {
        (void)idf::free(sPathName);
    }

    return sFileID;

    IDE_EXCEPTION_END;

    if(sPathName != NULL)
    {
        (void)idf::free(sPathName);
    }

    return IDF_INVALID_FILE_ID;
}

SInt idf::getUnusedFileID()
{
    UInt sIndex;
    SInt sFileID = IDF_INVALID_FILE_ID;

    for(sIndex = 0; sIndex < idf::mMaxFileCount; sIndex++)
    {
        if(idf::mMetaList[sIndex].mMeta == NULL)
        {
            sFileID = sIndex;

            break;
        }
    }

    return sFileID;
}

IDE_RC idf::getFreePage(UInt *aMap, UInt aFileID)
{
    UInt sIndex;
    UInt sMax;
    UInt sPageIndex;
    UInt sTotalBlocks = idf::mPageNum / idf::mPagesPerBlock;

    IDE_TEST(aMap == NULL);

    // free �������� ���� �� �ý��� �������� �˻����� �ʴ´�.
    for(sIndex = idf::mSystemSize; sIndex < sTotalBlocks; sIndex++)
    {
        if(idf::mBlockMap[sIndex] == 0x00)
        {
            // �� �������� ������ �Ҵ��Ѵ�.
            idf::mBlockMap[sIndex] = 0xFF;

            idf::mBlockList[sIndex] = aFileID;

            idf::mMaster.mAllocedPages += idf::mPagesPerBlock;

            sIndex *= idf::mPagesPerBlock;

            for(sPageIndex = 0;
                sPageIndex < idf::mPagesPerBlock;
                sPageIndex++)
            {
                aMap[sPageIndex] = ((sIndex + sPageIndex) | IDF_FILE_HOLE_MASK);
            }

            sIndex /= idf::mPagesPerBlock;

            break;
        }
    }

    // �� �������� ���� ��� No space error�� �߻��Ѵ�.
    IDE_TEST_RAISE(sIndex == sTotalBlocks, nospace_error);

    sIndex = sIndex * idf::mPagesPerBlock * idf::mPageSize;

    IDE_TEST(idf::lseekFs(idf::mFd, 
                          sIndex, 
                          SEEK_SET) == -1);

    sMax = idf::mPagesPerBlock * idf::mPageSize / EMPTY_BUFFER_SIZE;

    for(sIndex = 0; sIndex < sMax; sIndex++)
    {
        IDE_TEST(idf::writeFs(idf::mFd, 
                              idf::mEmptyBuf, 
                              EMPTY_BUFFER_SIZE) != EMPTY_BUFFER_SIZE);
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION(nospace_error)
    {
        // �� �������� ���� ��� No space error�� �߻��Ѵ�.
        errno = ENOSPC; // No space left on device
    }

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

SChar *idf::getpath(const SChar *aPathName)
{
    UInt   sIndex = 0;
    SChar *sPathName;
    SChar *sNewName;
    SChar *sDiv;

    sPathName = (SChar*)idlOS::strstr(aPathName, "altibase_home/");

#if defined(VC_WIN32)
    if(sPathName == NULL)
    {
        sPathName = (SChar*)idlOS::strstr(aPathName, "altibase_home\\");
    }
#endif // VC_WIN32

    if(sPathName == NULL)
    {
        sPathName = (SChar*)aPathName;
    }
    else
    {
        sPathName += 14; // idlOS::strlen("altibase_home\") == 14
    }

    //=======================================================================
    // sNewName�� getpath()�Լ����� �޸𸮸� �������� �ʴ´�.
    // �޸� ������ �� �Լ��� ȣ���� �Լ����� �� ����� �Ŀ� �����ؾ� �Ѵ�.
    //=======================================================================
    (void)idf::alloc((void **)&sNewName, IDF_MAX_FILE_NAME_LEN);

    // ���� �տ� ���� '.', '/', '\\'�� �����Ҷ� ����Ѵ�.
#if defined(VC_WIN32)
    sDiv = (SChar*)"./\\";
#else
    sDiv = (SChar*)"./";
#endif // VC_WIN32

    // ���� �տ� ���� '.', '/', '\\'�� �����Ѵ�.
    while(idlOS::strchr(sDiv, *sPathName) != NULL)
    {
        sPathName++;
    }

    // path�� �߰��� �ݺ��Ͽ� ��Ÿ���� '/', '\\'�� �����Ҷ� ����Ѵ�.
#if defined(VC_WIN32)
    sDiv = (SChar*)"/\\";
#else
    sDiv = (SChar*)"/";
#endif // VC_WIN32

    do
    {
        // path�� �߰��� �ݺ��Ͽ� ��Ÿ���� '/', '\\'�� �����Ѵ�.
        if(idlOS::strchr(sDiv, *sPathName) != NULL)
        {
            while((*(sPathName + 1) != '\0') &&
                  (idlOS::strchr(sDiv, *(sPathName + 1)) != NULL))
            {
                sPathName++;
            }
        }

#if defined(VC_WIN32)
        // Windows�迭�� OS���� '/'���ڸ� '\\'�� �����Ѵ�.
        if(*sPathName == '/')
        {
            sNewName[sIndex] = '\\';
        }
        else
        {
            sNewName[sIndex] = *sPathName;
        }
#else
        sNewName[sIndex] = *sPathName;
#endif // VC_WIN32

        sIndex++;
        sPathName++;

        if(sIndex == IDF_MAX_FILE_NAME_LEN - 1)
        {
            break;
        }
    } while(*sPathName != '\0');

    // path�� ���� �������� ��ġ�� '/', '\\'���ڸ� �����Ѵ�.
    if(idlOS::strchr(sDiv, sNewName[sIndex - 1]) != NULL)
    {
        sNewName[sIndex -1] = '\0';
    }
    else
    {
        sNewName[sIndex] = '\0';
    }

    return sNewName;
}

//=======================================================================
// ������ Indirect Page Map�� �����´�.
// �켱 Indirect Page Map Pool�� �˻��Ͽ� Indirect page�� ã��,
// Indirect Page Map�� Pool�� ������ ��ũ���� �д´�.
//
//     aFileID       : File ID
//     aIndex        : �� ��° Indrect Page Map������ ��Ÿ����.
//     aMapIndex     : Indirect Page Map�� �� ��° ���������� ��Ÿ����.
//     aLogicalPage  : Indirect Page Map�� �����ϰ� �ִ� ������ ��ȣ.
//
//
// * Indirect Page Map Pool�� ����
//     �� Page Map�� Double Linked List�� ����ȴ�.
//
//  ------------------- 
// | idf::mPageMapPool |
//  -------------------
//         |
//         v
//     ----------       ----------       ----------             ----------
//    | Page Map | <-> | Page Map | <-> | Page Map | <- ... -> | Page Map |
//     ----------       ----------       ----------             ----------
//         ^                                                        ^
//         |________________________________________________________|
//
//=======================================================================
UInt *idf::getPageMap(SInt aFileID,   UInt aIndex,
                      UInt aMapIndex, UInt aLogicalPage)
{
    // 1. idf::mPageMapPool���� �˻��Ͽ� �������� �����ϴ��� Ȯ���Ѵ�. 
    // 2. idf::mPageMapPool�� ������ LRU �˰������� Page Map�� �����´�.

    UInt        sIndex;
    UInt       *sMap;
    UInt        sFind = 0;
    idfPageMap *sPageMap;
    UInt        sCount;

    sPageMap = idf::mPageMapPool;

    for(sIndex = 0; sIndex < idf::mMapPoolSize; sIndex++)
    {
        if((sPageMap->mFileID == aFileID) &&
           (sPageMap->mPageIndex == aIndex) &&
           (sPageMap->mMapIndex == aMapIndex))
        {
            // PageMap�� ã�� ��� ã�� ��带 ���� ������ �̵��Ѵ�. (LRU)
            if(sIndex != 0)
            {
                if(sPageMap != idf::mPageMapPool)
                {
                    sPageMap->mPrev->mNext = sPageMap->mNext;
                    sPageMap->mNext->mPrev = sPageMap->mPrev;

                    // �Ʒ��� ������ ����Ǹ� �ȵȴ�.
                    idf::mPageMapPool->mPrev->mNext = sPageMap;

                    sPageMap->mPrev = idf::mPageMapPool->mPrev;

                    idf::mPageMapPool->mPrev = sPageMap;

                    sPageMap->mNext = idf::mPageMapPool;

                    idf::mPageMapPool = sPageMap;
                }
            }

            sMap = idf::mPageMapPool->mMap;

            sFind = 1;

            break;
        }

        sPageMap = sPageMap->mNext;
    }

    if(sFind == 0)
    {
        // PageMap�� ã�� ���� ���
        // PageMapPool�� �� ���� ���� ��� PageMapPoolCount�� �����Ѵ�.
        // PageMap�� shift�Ͽ� ���� �������� ������� ���� PageMap��
        // �ʱ�ȭ�ϰ� ����ϵ��� �Ѵ�.
        if(idf::mPageMapPoolCount != idf::mMapPoolSize)
        {
            idf::mPageMapPoolCount++;
        }

        idf::mPageMapPool = idf::mPageMapPool->mPrev;

        // PageMap�� �������� ������ PageMap�� ���� �Ҵ��Ѵ�.
        if(mPageMapPool->mMap == NULL)
        {
            (void)idf::alloc((void **)&sMap, idf::mIndEntrySize);
        }
        else
        {
            sMap = idf::mPageMapPool->mMap;
        }

        idlOS::memset(sMap, 
                      0x00, 
                      idf::mIndEntrySize);

        idf::mPageMapPool->mFileID = aFileID;
        idf::mPageMapPool->mPageIndex = aIndex;
        idf::mPageMapPool->mMapIndex = aMapIndex;
        idf::mPageMapPool->mMap = sMap;
    }

    // Logical Page�� 0�̸� �Ҵ��� �������� �����Ƿ� 0�� ��ȯ�Ѵ�.
    // Logical Page�� 0�� �ƴϰ� Map Pool�� ������ �켱 Page Map Log Pool����
    // �˻��Ͽ� �ش� Page Map�� �ִ��� �˻��Ѵ�.
    // Page Map Log Pool���� �ش� Page Map�� ������ ��ũ���� Page Map��
    // �о ��ȯ�Ѵ�.
    if((!sFind) && (aLogicalPage != 0))
    {
        idfMapHeader *sMapHeader = NULL;

        for(sIndex = (idf::mMapLogCount - 1); (SInt)sIndex >= 0; sIndex--)
        {
            sMapHeader = &(idf::mMapHeader[sIndex]);

            if((sMapHeader->mFileID == aFileID) &&
               (sMapHeader->mIndex == aLogicalPage) &&
               (sMapHeader->mMapIndex == aMapIndex))
            {
                sFind = 1;

                idlOS::memcpy((void*)sMap, 
                              (void*)(&idf::mMapLog[(idf::mIndEntrySize * sIndex)]), 
                              idf::mIndEntrySize);
                break;
            }
        }

        if(!sFind)
        {
            IDE_TEST(idf::lseekFs(idf::mFd,
                                  ((aLogicalPage * idf::mPageSize) + (aMapIndex * ID_SIZEOF(UInt))),
                                  SEEK_SET) == -1);

            IDE_TEST(idf::readFs(idf::mFd, 
                                 sMap, 
                                 idf::mIndEntrySize) != idf::mIndEntrySize);

            for(sCount = 0; sCount < idf::mPagesPerBlock; sCount++)
            {
                sMap[sCount] = idlOS::ntoh(sMap[sCount]);
            }
        }
    }

    return sMap;

    IDE_EXCEPTION_END;

    return NULL;
}

UInt idf::getPageAddrW(idfMeta *aMeta, UInt aFileID, SInt aPageIndex)
{
    UInt  sLogicalPage = 0;
    SInt  sPageOffset = aPageIndex % idf::mPagesPerBlock;
    UInt *sPage;
    UInt *sMap;
    UInt  sIndPage;
    UInt  sIndIndex = 0;
    UInt  sIndOffset;
    UInt  sMapIndex;
    UInt  sMapOffset = 0;

    if(aPageIndex < IDF_DIRECT_MAP_SIZE)
    {
        // cursor�� Direct Page Map ���� �ִ� ���
        // Direct Page Map�� aPageIndex�� ��ġ�� Logical Page�� Page Addr�̴�.
        sPage = &(aMeta->mDirectPages[aPageIndex]);

        // sMap�� �������� �Ҵ��ϱ� ���� getFreePage�� ȣ���� �� ����Ѵ�.
        sMap = &(aMeta->mDirectPages[aPageIndex - sPageOffset]);

        sMapIndex = 0;
    }
    else
    {
        //==========================================================
        // cursor�� Indirect Page Map ��ġ�� �ִ� ���
        //
        // sIndIndex  : Indirect Map�� Index
        // sIndOffset : Indirect Map�� Offset
        //              (Indirect Map�� �Ҵ��� �� ���)
        // sMapIndex  : Inidirect Map ���� Index
        // sMapOffset : Indirect Map ���� Offset
        //              (Indirect Map ���� �������� �Ҵ��� �� ���)
        //
        // ex>
        // Indirect Map 0 [ ... ] <- sIndOffset
        // Indirect Map 1 [ ... ] <- sIndIndex
        //                   |_______________
        //   ...                            |
        //                                  |
        // Indirect Map n [ ... ]           |
        //         _________________________|
        //        |
        //        v
        // Indirect Map #1
        // [index 0] [index 1] [index 2] [[index 3]
        //  ^^ sMapOffset       ^^ sMapIndex
        //   ...
        //==========================================================
        sIndIndex  = (aPageIndex - IDF_DIRECT_MAP_SIZE) / idf::mIndMapSize;
        sIndOffset = sIndIndex - (sIndIndex % idf::mPagesPerBlock);
        sMapIndex  = (aPageIndex - IDF_DIRECT_MAP_SIZE) % idf::mIndMapSize;
        sMapOffset = sMapIndex - (sMapIndex % idf::mPagesPerBlock);

        // Indirect Page Map�� ������ ����� ������ �߻��Ѵ�.
        IDE_TEST_RAISE(sIndIndex >= idf::mIndPageNum, toolarge_error);

        // Meta�� IndirectPage�� �����Ͽ� �����ϴ��� Ȯ���Ѵ�.
        // IndirectPage�� �����ϸ� Indirect Page Map�� ���� �� �ֵ���
        // getPageMap�� ���ڷ� �Ѱ��ش�.
        if(aMeta->mIndPages[sIndIndex] & IDF_FILE_HOLE_MASK)
        {
            sIndPage = 0;

            aMeta->mIndPages[sIndIndex] &= IDF_FILE_HOLE_INVERTER;

            idf::mIsDirty = ID_TRUE;
        }
        else
        {
            sIndPage = aMeta->mIndPages[sIndIndex];
        }

        // ������ �� Pool���� ������ ���� ���´�.
        // aMeta->mIndPages[sIndIndex]�� 0�̸�
        // 0���� ä�� �迭�� ��ȯ�Ѵ�.
        sMap = getPageMap(aFileID, sIndIndex, sMapOffset, sIndPage); 

        IDE_TEST(sMap == NULL);

        // aMeta->mIndPages[sIndIndex]�� 0�̸�
        // Indirect Page Map�� �������� �ʴ� ���̹Ƿ�,
        // Indirect Page Map�� ������ �������� ���´�.
        if(aMeta->mIndPages[sIndIndex] == 0)
        {
            IDE_TEST(idf::getFreePage(&(aMeta->mIndPages[sIndOffset]), aFileID)
                     != IDE_SUCCESS);

            aMeta->mIndPages[sIndIndex] &= IDF_FILE_HOLE_INVERTER;

            idf::mIsDirty = ID_TRUE;
        }

        sPage = &(sMap[sMapIndex - sMapOffset]);
    }

    if(*sPage == 0)
    {
        // Data�� ������ Logical Address�� �Ҵ����� �ʾ����Ƿ�
        // Page�� �Ҵ��� �ش�.
        IDE_TEST(idf::getFreePage(sMap, aFileID) != IDE_SUCCESS);

        aMeta->mNumOfPagesA += idf::mPagesPerBlock;

        idf::mIsDirty = ID_TRUE;
    }

    if(*sPage & IDF_FILE_HOLE_MASK)
    {
        *sPage &= IDF_FILE_HOLE_INVERTER;

        if(aPageIndex >= IDF_DIRECT_MAP_SIZE)
        {
            // aPageIndex�� Indirect Page Map�� ���� �ִ� ���
            IDE_TEST(idf::appandPageMapLog(aFileID, sIndIndex, sMapOffset)
                     != IDE_SUCCESS);
        }

        aMeta->mNumOfPagesU++;

        idf::mIsDirty = ID_TRUE;
    }

    sLogicalPage = *sPage;

    return sLogicalPage;

    IDE_EXCEPTION(toolarge_error)
    {
        // ������ ũ�Ⱑ �ʹ� ū ��� File Too Large error�� �߻��Ѵ�.
        errno = EFBIG; // File too large
    }

    IDE_EXCEPTION_END;

    return ((UInt)(-1));
}

UInt idf::getPageAddrR(idfMeta *aMeta, UInt aFileID, SInt aPageIndex)
{
    UInt  sLogicalPage = 0;
    UInt  sIndIndex = 0;
    UInt  sMapIndex = 0;
    UInt  sMapOffset;
    UInt  sIndPage;
    UInt *sMap;

    if(aPageIndex < IDF_DIRECT_MAP_SIZE)
    {
        // cursor�� Direct Page Map ���� �ִ� ���
        sLogicalPage = aMeta->mDirectPages[aPageIndex];

        // sLogicalPage�� 0�̰ų� IDF_FILE_HOLE_MASK�� & ������ ������
        // 0�� �ƴϸ� File Hole�̴�.
        if(sLogicalPage & IDF_FILE_HOLE_MASK)
        {
            // File Hole
            sLogicalPage = 0;
        }
    }
    else
    {
        // cursor�� Indirect Page Map ��ġ�� �ִ� ���
        sIndIndex  = (aPageIndex - IDF_DIRECT_MAP_SIZE) / idf::mIndMapSize;
        sMapIndex  = (aPageIndex - IDF_DIRECT_MAP_SIZE) % idf::mIndMapSize;
        sMapOffset = sMapIndex - (sMapIndex % idf::mPagesPerBlock);

        IDE_TEST(idf::mIndPageNum < sIndIndex);

        // Meta�� IndirectPage�� �����Ͽ� �����ϴ��� Ȯ���Ѵ�.
        sIndPage = aMeta->mIndPages[sIndIndex];

        // �������� �ʰų� File Hole�� ��� 0�� ��ȯ�Ѵ�.
        if((sIndPage == 0) || (sIndPage & IDF_FILE_HOLE_MASK))
        {
            sLogicalPage = 0;
        }
        else
        {
            // Indirect Page�� 0�� �ƴ� ��� page map�� ���´�.
            sMap = getPageMap(aFileID,
                              sIndIndex,
                              sMapOffset,
                              aMeta->mIndPages[sIndIndex]);

            IDE_TEST(sMap == NULL);

            sLogicalPage = sMap[sMapIndex - sMapOffset];

            if(sLogicalPage & IDF_FILE_HOLE_MASK)
            {
                sLogicalPage = 0;
            }
        }
    }

    return sLogicalPage;

    IDE_EXCEPTION_END;

    return ((UInt)(-1));
}

UInt idf::getCRC(const UChar *aBuf, SInt aSize)
{         
    UInt sCRC = 0;
        
    while(aSize--) 
    {        
        sCRC = idf::mCRCTable[(sCRC ^ *(aBuf++)) & 0xFF] ^ (sCRC >> 8);
    } 
    
    return sCRC; 
}   

void idf::initCRCTable()
{   
    UInt i, j, k; 
    UInt id = 0xEDB88320; 
   
    (void)idf::alloc((void **)&idf::mCRCTable, ID_SIZEOF(UInt) * 256);

    for(i = 0; i < 256; ++i) {
        k = i;
        for(j = 0; j < 8; ++j) {
            if (k & 1) k = (k >> 1) ^ id;
            else k >>= 1;
        }
        idf::mCRCTable[i] = k;
    }
}

void idf::dumpMaster(idfMaster * aMaster)
{
    if(idf::mFd != PDL_INVALID_HANDLE)
    {
        SLong  sTotal = ((SLong)aMaster->mSizeOfPage * aMaster->mNumOfPages);
        SLong  sUsed  = ((SLong)aMaster->mSizeOfPage * aMaster->mAllocedPages);
        SFloat sUsedP = ((SFloat)sUsed / sTotal) * 100;
        SFloat sFreeP = (((SFloat)sTotal - sUsed) / sTotal) * 100;

        idlOS::printf(" VERSION          : v%"ID_INT32_FMT".%"ID_INT32_FMT"\n",
                aMaster->mMajorVersion, aMaster->mMinorVersion);
        idlOS::printf(" Total Size       : %"ID_UINT64_FMT"\n", sTotal);
        idlOS::printf(" Used Size        : %"ID_UINT64_FMT" (%3.2f%%)\n", sUsed, sUsedP);
        idlOS::printf(" Free Size        : %"ID_UINT64_FMT" (%3.2f%%)\n",
                sTotal - sUsed, sFreeP);
        idlOS::printf(" Pages Per Block  : %"ID_UINT32_FMT"\n", idf::mPagesPerBlock);
        idlOS::printf(" Num Of Pages     : %"ID_UINT32_FMT"\n", aMaster->mNumOfPages);
        idlOS::printf(" Size Of Page     : %"ID_UINT32_FMT"\n", aMaster->mSizeOfPage);
        idlOS::printf(" Alloced Pages    : %"ID_UINT32_FMT"\n", aMaster->mAllocedPages);
        idlOS::printf(" File System Mode : %"ID_INT32_FMT"\n", (UInt)aMaster->mFSMode);
        idlOS::printf(" Log Mode         : %"ID_INT32_FMT"\n", aMaster->mLogMode);
        idlOS::printf(" Num Of Files     : %"ID_UINT32_FMT"\n", aMaster->mNumOfFiles);
    }
}

void idf::dumpMeta(PDL_HANDLE aFd)
{
    if(idf::mFd != PDL_INVALID_HANDLE)
    {
        SInt     sFileID;
        SInt     sFd = (SInt)aFd;
        idfMeta *sMeta;

        if(idf::mFdList[sFd].mIsUsed == IDF_FD_UNUSED)
        {
            return;
        }

        sFileID = idf::mFdList[sFd].mFileID;

        sMeta = idf::mMetaList[sFileID].mMeta;

        if(sMeta != NULL)
        {
            idlOS::printf("%10s %4"ID_INT32_FMT" %10"ID_UINT32_FMT" %5"ID_INT32_FMT" %5"ID_INT32_FMT" %s\n", 
                          "-rw-------", 
                          sMeta->mFileID,
                          sMeta->mSize,
                          sMeta->mNumOfPagesU,
                          sMeta->mNumOfPagesA,
                          sMeta->mFileName);             
        }
    }
}

void idf::dumpMetaP(const SChar *aPath)
{
    SChar   *sPath;
    SInt     sFileID;
    UInt     sIndex;
    UInt     sCount;
    UInt     sPageMax;
    UInt     sMaxIndex;
    UInt     sMaxOffset;
    idfMeta *sMeta;
    UInt    *sMap = NULL;

    if(idf::mFd != PDL_INVALID_HANDLE)
    {
        sPath = idf::getpath(aPath);

        sFileID = idf::getFileIDByName(aPath);

        sMeta = idf::mMetaList[sFileID].mMeta;

        if((sMeta != NULL) && (sMeta->mSize > 0))
        {
            idlOS::printf("\n");
            idlOS::printf("=======================================================\n");
            idlOS::printf("                   META PAGE MAP DUMP\n");
            idlOS::printf("=======================================================\n");
            idlOS::printf(" [File : %s][Size : %"ID_UINT32_FMT"]\n", sPath, sMeta->mSize);
            idlOS::printf("---------------------- Direct Page --------------------\n");

            for(sIndex = 0; sIndex < IDF_DIRECT_MAP_SIZE; sIndex++)
            {
                if((sIndex != 0) && (sIndex % 4 == 0))
                {
                    idlOS::printf("\n");
                }

                idlOS::printf("[%5"ID_INT32_FMT"] %5"ID_INT32_FMT" ",
                              sIndex,
                              sMeta->mDirectPages[sIndex] & IDF_FILE_HOLE_INVERTER);
            }

            idlOS::printf("\n");

            sPageMax = sMeta->mSize / idf::mPageSize;

            if(sPageMax >= idf::mDirectPageNum)
            {
                (void)idf::alloc((void **)&sMap, idf::mPageSize);

                idlOS::printf("------------------ Indirect Page Map ------------------\n");

                for(sIndex = 0; sIndex < idf::mIndPageNum; sIndex++)
                {
                    if((sIndex != 0) && (sIndex % 4 == 0))
                    {
                        idlOS::printf("\n");
                    }

                    idlOS::printf("[%5"ID_INT32_FMT"] %5"ID_INT32_FMT" ", sIndex, sMeta->mIndPages[sIndex] & IDF_FILE_HOLE_INVERTER);
                }

                idlOS::printf("\n");

                sMaxIndex  = (sPageMax - idf::mDirectPageNum) / idf::mIndMapSize + 1;
                sMaxOffset = (sPageMax - idf::mDirectPageNum) % idf::mIndMapSize;

                idlOS::printf("-------------------- Indirect Page --------------------\n");

                for(sIndex = 0; sIndex < sMaxIndex; sIndex++)
                {
                    if((sMeta->mIndPages[sIndex] != 0) &&
                       ((sMeta->mIndPages[sIndex] & IDF_FILE_HOLE_MASK)
                       == 0))
                    {
                        (void)idf::lseekFs(idf::mFd,
                                           sMeta->mIndPages[sIndex] * idf::mPageSize,
                                           SEEK_SET);

                        (void)idf::readFs(idf::mFd, 
                                          sMap, 
                                          idf::mPageSize);

                        for(sCount = 0; sCount < idf::mIndMapSize; sCount++)
                        {
                            if((sIndex == sMaxIndex -1) && (sMaxOffset == sCount))
                            {
                                break;
                            }

                            if((sCount != 0) && (sCount % 4 == 0))
                            {
                                idlOS::printf("\n");
                            }

                            sMap[sCount] = idlOS::ntoh(sMap[sCount]);
                            
                            idlOS::printf("[%5"ID_INT32_FMT"] %5"ID_INT32_FMT" ", sCount, sMap[sCount]);
                        }
                    }
                }

                idlOS::printf("\n");

                (void)idf::free(sMap);
            }

            idlOS::printf("\n");
        }

        if(sPath != NULL)
        {
            (void)idf::free(sPath);
        }
    }
}

void idf::dumpAllMeta()
{
    if(idf::mFd != PDL_INVALID_HANDLE)
    {
        UInt sFileCount;
        UInt sIndex;
        UInt sCount = 0;
        idfMeta *sMeta;

        sFileCount = idf::mMaster.mNumOfFiles;

        idlOS::printf("total %"ID_UINT32_FMT"\n", idf::mMaster.mAllocedPages);
#if 0
        idlOS::printf("%10s %4s %10s %5s %5s %s\n", 
                      "MODE", 
                      "ID",
                      "SIZE",
                      "U",
                      "A",
                      "NAME");             
#endif

        for(sIndex = 0; sIndex < idf::mMaxFileCount; sIndex++)
        {
            if(sCount == sFileCount)
            {
                break;
            }

            sMeta = idf::mMetaList[sIndex].mMeta;
            if(sMeta != NULL)
            {
                sCount++;

                idlOS::printf("%10s %4"ID_INT32_FMT" %10"ID_UINT32_FMT" %5"ID_INT32_FMT" %5"ID_INT32_FMT" %s\n", 
                              "-rw-------", 
                              sMeta->mFileID,
                              sMeta->mSize,
                              sMeta->mNumOfPagesU,
                              sMeta->mNumOfPagesA,
                              sMeta->mFileName);             
            }
        }
    }
}

void idf::dumpBlockMap()
{
    UInt sIndex;
    UInt sCount;
    UInt sTotalBlock = idf::mPageNum / idf::mPagesPerBlock;

    idlOS::printf("===============================================================\n");
    idlOS::printf("                        DUMP BLOCK MAP\n");
    idlOS::printf("===============================================================\n");

    for(sCount = 0; sCount < sTotalBlock; sCount += (idf::mPagesPerBlock*2))
    {
        idlOS::printf("%8"ID_INT32_FMT" : ", sCount);

        for(sIndex = 0; sIndex < idf::mPagesPerBlock; sIndex++)
        {
            idlOS::printf("%2x ", idf::mBlockMap[sCount + sIndex]);
        }

        idlOS::printf("    ");

        for(sIndex = 0; sIndex < idf::mPagesPerBlock; sIndex++)
        {
            idlOS::printf("%2x ", idf::mBlockMap[sCount + sIndex]);
        }

        idlOS::printf("\n");
    }
    idlOS::printf("===========================================\n");
}

/*
IDE_RC idf::validateMaster()
{
    UInt   sIndex;
    UInt   sCount = 0;
    UInt   sMax = idf::mPageNum / idf::mPagesPerBlock;
    IDE_RC sRc;

    for(sIndex = 0; sIndex < sMax; sIndex++)
    {
        if(idf::mBlockMap[sIndex] == 0xFF)
        {
            sCount++;
        }
    }

    sCount *= idf::mPagesPerBlock;

    if((sCount)== idf::mMaster.mAllocedPages)
    {
        sRc = IDE_SUCCESS;
    }
    else
    {
        sRc = IDE_FAILURE;
    }

    return sRc;
}
*/

IDE_RC idf::initializeCore(const SChar * aFileName)
{
    //=============================================================
    // aFileName�� NULL�� �ƴϸ� idfCore�� �Լ��� ����ϵ��� �Ѵ�.
    //=============================================================
    (void)idf::initWithIdfCore();

    (void)gAlloca.init(IDF_PAGE_SIZE, IDF_ALIGN_SIZE, ID_FALSE);

    (void)idf::initCRCTable();

    (void)idf::initPageMapPool();

    (void)idf::alloc((void **)&idf::mEmptyBuf, EMPTY_BUFFER_SIZE);

    idlOS::memset(idf::mEmptyBuf, 
                  '\0', 
                  EMPTY_BUFFER_SIZE);

    (void)idf::alloc((void **)&idf::mLogHeader, ID_SIZEOF(idfLogHeader));

    //======================
    // Mutex�� �ʱ�ȭ �Ѵ�.
    //======================
    (void)idf::initMutex();

    (void)idf::initFileName(aFileName);

#if !defined(USE_RAW_DEVICE)
    (void)idf::initLockFile();
#endif

    return IDE_SUCCESS;
}

//====================================================================
// idf Ŭ������ �ʱ�ȭ �Ѵ�. initializeStatic()�� �ٸ� ���� �� �Լ���
// Utility���� ���ʷ� ������ �����ϰ��� �� �� ����Ѵ�.
// �� �̿ܿ��� �� �Լ��� ������� �ʴ´�.
//====================================================================
IDE_RC idf::initializeStatic2(const SChar * aFileName, idfMaster *aMaster)
{
    UInt   sIndex;
    SInt   sRc;
    SChar *sBuf = NULL;

    //fix for VxWorks raw device
    if(idf::mFd != PDL_INVALID_HANDLE)
    {
        IDE_RAISE(pass);
    }

    if(aFileName == NULL)
    {
        //======================================================
        // aFileName�� NULL�̸� idlOS�� �Լ��� ����ϵ��� �Ѵ�.
        //======================================================
        (void)idf::initWithIdlOS();
    }
    else
    {
        IDE_TEST(idf::initializeCore(aFileName) != IDE_SUCCESS);

        // aFileName�� NULL�� �ƴϸ� idf::initializeCore()�� ȣ���� ��
        // mFileName�� NULL�� �� ����.
        // Klocwork error�� �����ϱ� ���� ����
        IDE_TEST(idf::mFileName == NULL);

        IDE_TEST((idf::mFd = idlOS::open(idf::mFileName,
                        O_RDWR | O_CREAT | O_TRUNC | O_BINARY,
                        S_IRUSR | S_IWUSR))
                 == PDL_INVALID_HANDLE);

        // ������ �ʱ�ȭ���� ������ �ٸ� �۾��� �Ұ��� �ϴ�.
        // ����ڰ� �Է��� Master�� ������ Master �������� �ʱ�ȭ �Ѵ�.
        if(aMaster != NULL)
        {
            idlOS::memcpy(&idf::mMaster, 
                          aMaster, 
                          idf::mMasterSize);
        }

        // Master �ʱ�ȭ
        (void)idf::initMasterPage(aMaster);

        //==================================================================
        // File System�� Mode�� 1�̸� ������ disk�� ũ�⸸ŭ �̸� �Ҵ��Ѵ�.
        // File System�� ȯ�� ������ initMasterPage()���� �ϹǷ� �� �Լ�
        // ������ disk ������ ��� �Ҵ��Ѵ�.
        //==================================================================
        if(idf::mMaster.mFSMode == 1)
        {
            (void)idf::alloc((void **)&sBuf, idf::mPageSize);

            idlOS::memset(sBuf, 
                          '\0', 
                          idf::mPageSize);

            for(sIndex = 0; sIndex < idf::mPageNum; sIndex++)
            {
                IDE_TEST((sRc = idf::writeFs(idf::mFd, 
                                             sBuf, 
                                             idf::mPageSize)) != (SInt)idf::mPageSize);
            }

            (void)idf::free(sBuf);

            sBuf = NULL;
        }

        (void)idf::initBlockMap();

        // Meta �ʱ�ȭ
        IDE_TEST(idf::initMetaList2() != IDE_SUCCESS);

        // Fd List �ʱ�ȭ
        (void)idf::initFdList();

        if(idf::mMaster.mLogMode == IDF_LOG_MODE)
        {
            // �α������� �����Ѵ�.
            idf::mLogFd = idlOS::open(idf::mLogFileName,
                                      O_RDWR | O_CREAT | O_TRUNC | O_BINARY,
                                      S_IRUSR | S_IWUSR);

            IDE_TEST(idf::mLogFd == PDL_INVALID_HANDLE);
        }

        (void)initLogList();

        IDE_TEST(idf::masterLog(1) != IDE_SUCCESS);

        sIndex = idlOS::hton(IDF_SYSTEM_SIGN);

        IDE_TEST(idf::lseekFs(idf::mFd, 
                              0, 
                              SEEK_SET) == -1);

        IDE_TEST(idf::writeFs(idf::mFd, 
                              &sIndex, 
                              4) != 4);

        IDE_TEST(idf::writeMaster() != IDE_SUCCESS);
    }

    IDE_EXCEPTION_CONT(pass);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if(idf::mFileName!= NULL)
    {
        (void)idf::free(idf::mFileName);

        idf::mFileName = NULL;
    }

    if(idf::mLogFileName!= NULL)
    {
        (void)idf::free(idf::mLogFileName);

        idf::mLogFileName = NULL;
    }

    if(sBuf != NULL)
    {
        (void)idf::free(sBuf);

        sBuf = NULL;
    }

    return IDE_FAILURE;
}

//================
// initialize idf 
//================
IDE_RC idf::initializeStatic(const SChar * aFileName)
{
    SChar *sCurrentVFS;
    UInt   sCheck;

    //fix for VxWorks raw device
    if(idf::mFd != PDL_INVALID_HANDLE)
    {
        IDE_RAISE(pass);
    }

    if(aFileName == NULL)
    {
        if((sCurrentVFS = idlOS::getenv(ALTIBASE_ENV_PREFIX"VFS")) !=  NULL)
        {
            aFileName = sCurrentVFS;
        }
    }

    if(aFileName == NULL)
    {
        //======================================================
        // aFileName�� NULL�̸� idlOS�� �Լ��� ����ϵ��� �Ѵ�.
        //======================================================
        (void)idf::initWithIdlOS();
    }
    else
    {
        IDE_TEST(idf::initializeCore(aFileName) != IDE_SUCCESS);

        // aFileName�� NULL�� �ƴϸ� idf::initializeCore()�� ȣ���� ��
        // mFileName�� NULL�� �� ����.
        // Klocwork error�� �����ϱ� ���� ����
        IDE_TEST(idf::mFileName == NULL);

        // serverStart() �Լ����� initializeStatic() �Լ��� ȣ���� ���
        // ������ ������ �⺻ �������� ������ ������ �����Ѵ�.
        if((idf::mFd = idlOS::open(idf::mFileName, O_RDWR | O_BINARY))
           == PDL_INVALID_HANDLE)
        {
            IDE_TEST((idf::mFd = idlOS::open(idf::mFileName,
                            O_RDWR | O_CREAT | O_TRUNC | O_BINARY,
                            S_IRUSR | S_IWUSR))
                     == PDL_INVALID_HANDLE);

            sCheck = idlOS::hton(IDF_SYSTEM_SIGN);

            IDE_TEST(idf::lseekFs(idf::mFd, 
                                  0, 
                                  SEEK_SET) == -1);

            IDE_TEST(idf::writeFs(idf::mFd, 
                                  &sCheck, 
                                  4) != 4);

            //===================================================
            // ������ �ʱ�ȭ���� ������ �ٸ� �۾��� �Ұ��� �ϴ�.
            //===================================================

            // Master �ʱ�ȭ
            (void)idf::initMasterPage(NULL);

            // Block Map �ʱ�ȭ
            (void)idf::initBlockMap();

            // Meta �ʱ�ȭ
            IDE_TEST(idf::initMetaList2() != IDE_SUCCESS);

            // Fd List �ʱ�ȭ
            (void)idf::initFdList();

            if(idf::mMaster.mLogMode == IDF_LOG_MODE)
            {
                // �α������� �����Ѵ�.
                idf::mLogFd = idlOS::open(idf::mLogFileName,
                                          O_RDWR | O_CREAT | O_TRUNC | O_BINARY,
                                          S_IRUSR | S_IWUSR);
            }

            IDE_TEST(idf::mLogFd == PDL_INVALID_HANDLE);
        }
        else
        {
            //================================================================
            // ������ Master, Meta �������� �о ���� �ý����� �ʱ�ȭ �Ѵ�.
            //================================================================

            // Master �������� �д´�.
            IDE_TEST(idf::lseekFs(idf::mFd, 
                                  0, 
                                  SEEK_SET) == -1);

            IDE_TEST(idf::readFs(idf::mFd, 
                                 &sCheck, 
                                 4) != 4);

            if(sCheck != idlOS::hton(IDF_SYSTEM_SIGN))
            {
                idlOS::exit(-1);
            }

            IDE_TEST(idf::readFs(idf::mFd, 
                                 &idf::mMaster, 
                                 idf::mMasterSize) != idf::mMasterSize);

            if(idf::mMaster.mSignature == idlOS::hton(IDF_SYSTEM_SIGN))
            {
                idf::mLogMode = idf::mMaster.mLogMode;

                if((idf::mLogMode != IDF_LOG_MODE) &&
                   (idf::mLogMode != IDF_NO_LOG_MODE))
                {
                    idf::mLogMode = IDF_DEFAULT_LOG_MODE;
                }
            }
            else
            {
                idf::mLogMode = IDF_DEFAULT_LOG_MODE;
            }

            if(idf::mLogMode == IDF_LOG_MODE)
            {
                // mFilename.log�� �����ϴ��� Ȯ���Ѵ�. 
                if(idlOS::access(idf::mLogFileName, O_RDWR) == 0)
                {
                    idf::mLogFd = idlOS::open(idf::mLogFileName, O_RDWR);

                    IDE_TEST(idf::mLogFd == PDL_INVALID_HANDLE);

                    // �α� ������ �����ϸ� recovery�� �����Ѵ�.
                    IDE_TEST(idf::doRecovery() != IDE_SUCCESS);

                    IDE_TEST(idf::lseekFs(idf::mLogFd, 
                                          0, 
                                          SEEK_SET) == -1);
                }
                else
                {
                    // �α� ������ �������� ������ ���� �����Ѵ�.
                    idf::mLogFd = idlOS::open(idf::mLogFileName,
                            O_RDWR | O_CREAT | O_TRUNC | O_BINARY,
                            S_IRUSR | S_IWUSR);

                    IDE_TEST(idf::mLogFd == PDL_INVALID_HANDLE);
                }
            }

            IDE_TEST(idf::lseekFs(idf::mFd, 
                                  4, 
                                  SEEK_SET) == -1);

            IDE_TEST(idf::readFs(idf::mFd, 
                                 &(idf::mMaster), 
                                 idf::mMasterSize) != idf::mMasterSize);

            (void)idf::initMasterPage(&(idf::mMaster));

            (void)idf::initFdList();

            (void)idf::initBlockMap();

            IDE_TEST(idf::initMetaList() != IDE_SUCCESS);

            //===========================================================
            // Master�� Signature�� �ùٸ��� Ȯ���Ѵ�.
            // Recovery �������� Master�� ������ �� �ֱ� ������
            // Recovery�� ������ �� ȣ���Ѵ�.
            // Recovery �Ŀ��� Master�� Signature�� �ùٸ��� ������
            // ���� �ý����� ���� ����̹Ƿ� ���̻��� ������ ���ǹ��ϴ�.
            //===========================================================
            if(idf::mMaster.mSignature != idlOS::hton(IDF_SYSTEM_SIGN))
            {
                idlOS::exit(-1);
            }
        }

        (void)idf::initLogList();

        IDE_TEST(idf::masterLog(1) != IDE_SUCCESS);

        IDE_TEST(idf::writeMaster() != IDE_SUCCESS);
    }

    IDE_EXCEPTION_CONT(pass);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if(idf::mFileName != NULL)
    {
        (void)idf::free(idf::mFileName);

        idf::mFileName = NULL;
    }

    if(idf::mLogFileName != NULL)
    {
        (void)idf::free(idf::mLogFileName);

        idf::mLogFileName = NULL;
    }

    return IDE_FAILURE;
}

void idf::finalFiles()
{
    if(idf::mFd != PDL_INVALID_HANDLE)
    {
        (void)idlOS::close(idf::mFd);

        idf::mFd = PDL_INVALID_HANDLE;
    }
    if(idf::mLogFd != PDL_INVALID_HANDLE)
    {
        (void)idlOS::close(idf::mLogFd);

        if(idf::mLogFileName != NULL)
        {
            (void)idlOS::unlink(idf::mLogFileName);
        }

        idf::mLogFd = PDL_INVALID_HANDLE;
    }

    if(idf::mFileName != NULL)
    {
        (void)idf::free(idf::mFileName);

        idf::mFileName = NULL;
    }
    if(idf::mLogFileName != NULL)
    {
        (void)idf::free(idf::mLogFileName);

        idf::mLogFileName = NULL;
    }
}

//==============
// finalize idf
//==============
IDE_RC idf::finalizeStatic()
{
    UInt        sIndex;
    idfPageMap *sTemp;
    idfPageMap *sMap;

    if(idf::mFd == PDL_INVALID_HANDLE)
    {
        IDE_RAISE(pass);
    }

    //===================================================================
    // finalize ���� �ٸ� �����尡 �۾��� �����Ϸ� �� �� �����Ƿ� Meta��
    // ���� ��´�.
    // ����, finalize ���߿� �ٸ� �����尡 �����ؼ��� �ȵȴ�.
    //===================================================================
    (void)idf::lock();

    // �α׸� ���� �� �α��� ������ ��� ��ũ�� �ݿ��Ѵ�.
    if(idf::mTransCount != 0)
    {
        idf::masterLog(1);
    }

    IDE_TEST(idf::fsyncFs(idf::mFd) == -1);

    (void)idf::finalFiles();

    //==============================
    // �Ҵ��� �ڿ��� ��� �����Ѵ�.
    //==============================

    if(idf::mEmptyBuf != NULL)
    {
        (void)idf::free(idf::mEmptyBuf);

        idf::mEmptyBuf = NULL;
    }

    if(idf::mLogHeader != NULL)
    {
        (void)idf::free(idf::mLogHeader);

        idf::mLogHeader = NULL;
    }

    // Block Map ����
    if(idf::mBlockMap != NULL)
    {
        (void)idf::free(idf::mBlockMap);

        idf::mBlockMap = NULL;
    }

    if(idf::mBlockList != NULL)
    {
        (void)idf::free(idf::mBlockList);

        idf::mBlockList = NULL;
    }

    if(idf::mCRCTable != NULL)
    {
        (void)idf::free(idf::mCRCTable);

        idf::mCRCTable = NULL;
    }

    // Page Map ����
    sMap = idf::mPageMapPool;
    sTemp = sMap->mNext;

    while(sMap != NULL)
    {
        if(sMap->mMap != NULL)
        {
            (void)idf::free(sMap->mMap);
            sMap->mMap = NULL;
            sMap->mNext = NULL;
        }

        (void)idf::free(sMap);

        sMap = sTemp;

        if(sMap == idf::mPageMapPool)
        {
            break;
        }

        sTemp = sMap->mNext;
    }

    // Fd List ����
    if(idf::mFdList != NULL)
    {
        (void)idf::free(idf::mFdList);

        idf::mFdList = NULL;
    }

    // Meta List ����
    if(idf::mMetaList != NULL)
    {
        for(sIndex = 0; sIndex < idf::mMaxFileCount; sIndex++)
        {
            if(idf::mMetaList[sIndex].mMeta != NULL)
            {
                (void)idf::free(idf::mMetaList[sIndex].mMeta);
            }
        }

        (void)idf::free(idf::mMetaList);
    }

    if(idf::mLogBuffer != NULL)
    {
        (void)idf::free(idf::mLogBuffer);
    }

    if(idf::mLogList != NULL)
    {
        (void)idf::free(idf::mLogList);
    }

    if(idf::mMapHeader != NULL)
    {
        (void)idf::free(idf::mMapHeader);
    }

    if(idf::mMapLog != NULL)
    {
        (void)idf::free(idf::mMapLog);
    }

    (void)gAlloca.destroy();

    (void)idf::unlock();

    (void)idf::finalMutex();

#if !defined(USE_RAW_DEVICE)
    (void)idf::finalLockFile();
#endif

    IDE_EXCEPTION_CONT(pass);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

void idf::finalMutex()
{
    IDE_ASSERT(idlOS::thread_mutex_destroy(&idf::mMutex) == 0);
}

//======================================================================
// idlOS::open(conat SChar*, SInt, SInt, SInt) �Լ��� ȣ���ϱ� ���� API
//======================================================================
PDL_HANDLE idf::idlopen(const SChar * pathname, SInt flag, ...)
{
    SInt mode = 0;

    if(flag & O_CREAT)
    {
        va_list arg;
        va_start(arg, flag);
        mode = va_arg(arg, SInt);
        va_end(arg);
    }

    return idlOS::open(pathname, flag, mode, 0);
}

void idf::lock()
{
    IDE_ASSERT(idlOS::thread_mutex_lock(&idf::mMutex) == 0);
}

void idf::unlock()
{
    IDE_ASSERT(idlOS::thread_mutex_unlock(&idf::mMutex) == 0);
}

//===========================================
// ����� �Ϸ��� File Descriptor�� ��ȯ�Ѵ�.
// ��ȯ�� File Descriptor�� �����Ѵ�.
//===========================================
IDE_RC idf::freeFd(SInt aFd)
{
    idfFdEntry *sFd = &(idf::mFdList[aFd]);

    sFd->mFileID = IDF_INVALID_FILE_ID;
    sFd->mCursor = 0;
    sFd->mIsUsed = IDF_FD_UNUSED;

    idf::mFileOpenCount--;

    return IDE_SUCCESS;
}

//============================================================================
// ����� �Ϸ��� Meta�� ��ȯ�Ѵ�.
// ��ȯ�� Meta�� FileID�� ���� �ϹǷ� FileID�� ���õ� �ڿ��� ��� �����Ѵ�.
//============================================================================
IDE_RC idf::freeMeta(SInt aFileID)
{
    idfMetaEntry *sMetaEntry = &(idf::mMetaList[aFileID]);
    idfMeta      *sMeta      = sMetaEntry->mMeta;
    idfPageMap   *sPageMap;
    idfPageMap   *sNextPageMap;
    UInt          sIndex;
    UInt          sCount = 0;
    UInt          sTotalBlocks = idf::mPageNum / idf::mPagesPerBlock;

    IDE_ASSERT(sMeta != NULL);

    //============================================================
    // ���� �ش� ������ ���� �ִ� File Descriptor�� �ִٸ�
    // ��� �Ұ��� �ϵ��� mIsUsed�� IDF_FD_INVALID(2)�� �����Ѵ�.
    // * mIsUsed ���� �ǹ�
    //   IDF_FD_UNUSED  (0) : ����ϰ� ���� ����,
    //   IDF_FD_USED    (1) : ����ϰ� ����,
    //   IDF_FD_INVALID (2) : ����ϰ� ������ ��ȿ���� ����
    //============================================================
    if(sMetaEntry->mFileOpenCount != 0)
    {
        for(sIndex = 0; sIndex < idf::mMaxFileOpenCount; sIndex++)
        {
            if(idf::mFdList[sIndex].mFileID == aFileID)
            {
                idf::mFdList[sIndex].mIsUsed = IDF_FD_INVALID;

                if((++sCount) == idf::mFileOpenCount)
                {
                    break;
                }
            }
        }
    }

    idlOS::memset(sMetaEntry, 
                  0x00, 
                  ID_SIZEOF(idfMetaEntry));

    idf::mMaster.mNumOfFiles--;

    //==========================================================================
    // Page Map Pool���� aFileID�� �ش��ϴ� Page Map�� �����Ѵ�.
    // Page Map Pool���� Page Map�� ������ �� ����Ʈ�� ���� ���������� �̵��Ѵ�.
    // Page Map�� �޷��ִ� mMap�� �޸𸮴� �̰����� �������� �ʴ´�.
    // Page Map�� �޷��ִ� mMap�� finalizeStatic�� ȣ���ϸ� �װ�����
    // Page Map Pool�� �޸𸮸� �����ϸ鼭 mMap�� �޸𸮵� �Բ� �����Ѵ�.
    //==========================================================================
    if(idf::mPageMapPoolCount > 0)
    {
        sPageMap = idf::mPageMapPool;

        do
        {
            sNextPageMap = sPageMap->mNext;

            if(sPageMap->mFileID == aFileID)
            {
                if(sPageMap == idf::mPageMapPool)
                {
                    // Page Map Pool�� ��� ������̰� ���� ���� Page Map��
                    // �ش��ϴ� Meta�� ������ ��� �ܼ��� ����Ʈ�� shift�Ѵ�.
                    sPageMap->mFileID = IDF_INVALID_FILE_ID;

                    idf::mPageMapPool = idf::mPageMapPool->mNext;
                }
                else
                {
                    // ������ Meta�� PageMap�� ���� ������ ��ġ�� �̵��Ѵ�.
                    sPageMap->mNext->mPrev = sPageMap->mPrev;
                    sPageMap->mPrev->mNext = sPageMap->mNext;

                    idf::mPageMapPool->mPrev->mNext = sPageMap;

                    sPageMap->mPrev = idf::mPageMapPool->mPrev;
                    sPageMap->mNext = idf::mPageMapPool;

                    idf::mPageMapPool->mPrev = sPageMap;

                    sPageMap->mFileID = IDF_INVALID_FILE_ID;
                }

                idf::mPageMapPoolCount--;

                // ������� PageMap�� ��� �˻������� �����Ѵ�.
                if(idf::mPageMapPoolCount == 0)
                {
                    break;
                }
            }

            sPageMap = sNextPageMap;
        } while(sPageMap != idf::mPageMapPool);
    }

    for(sIndex = idf::mSystemSize; sIndex < sTotalBlocks; sIndex++)
    {
        if(idf::mBlockList[sIndex] == aFileID)
        {
            idf::mBlockList[sIndex] = -1;

            idf::mBlockMap[sIndex] = 0x00;

            idf::mMaster.mAllocedPages -= idf::mPagesPerBlock;
        }
    }

    (void)idf::free(sMeta);

    sMeta = NULL;

    return IDE_SUCCESS;
}

IDE_RC idf::isDir(const SChar *aName)
{
    UInt     sNumOfFiles;
    UInt     sIndex;
    UInt     sCount = 0;
    UInt     sNameLen;
    idfMeta *sMeta;
    IDE_RC   sRc = IDE_FAILURE;;

    sNumOfFiles = idf::mMaster.mNumOfFiles;

    // �� �Լ��� ȣ���ϱ� ���� aName�� NULL���� Ȯ���ϹǷ� �� �Լ����� �ٽ�
    // aName�� NULL���� Ȯ������ �ʴ´�.
    assert(aName != NULL);

    sNameLen = idlOS::strlen(aName);

    // ��ü Meta���� ��ŭ loop�� �����Ѵ�.
    for(sIndex = 0; sIndex < idf::mMaxFileCount; sIndex++)
    {
        // Meta�� NULL�� �ƴϸ� �̸��� �˻��Ѵ�.
        if(idf::mMetaList[sIndex].mMeta == NULL)
        {
            continue;
        }

        sMeta = idf::mMetaList[sIndex].mMeta;

        // ���� �ý��۳��� ���丮�� ������ �������� �����Ƿ�
        // aName�� Meta�� ������ ���� �̸��� substring�̸� 
        // aName�� ���丮�� �����Ѵ�.
        if((idlOS::strncmp(sMeta->mFileName, aName, sNameLen) == 0) &&
           (idlOS::strlen(sMeta->mFileName) > sNameLen))
        {
            // aName�� ���丮�� ���� Ȯ���Ͽ��ٸ� �ٷ� loop�� �����Ѵ�.
            sRc = IDE_SUCCESS;
            break;
        }

        // Meta�� �����ϴ� ������ ��� �о����� �����Ѵ�.
        ++sCount;

        if(sCount >= sNumOfFiles)
        {
            break;
        }
    }

    return sRc;
}

idfDir* idf::getDir(const SChar *aName)
{
    UInt       sNumOfFiles;
    UInt       sIndex;
    UInt       sNameLen;
    UInt       sDirSize;
    idfMeta   *sMeta;
    idfDir    *sDir = NULL;
    idfDirent *sDirentP = NULL;
    dirent    *sDirent = NULL;
    UInt       sFence = 0;

    sNumOfFiles = idf::mMaster.mNumOfFiles;

    sNameLen = idlOS::strlen(aName);

    (void)idf::alloc((void **)&sDir, ID_SIZEOF(idfDir));

    sDir->mDirCount = 0;
    sDir->mDirIndex = 0;
    sDir->mFirst = NULL;

    sDirSize = ID_SIZEOF(dirent);

    // ��ü Meta���� ��ŭ loop�� �����Ѵ�.
    for(sIndex = 0; sIndex < idf::mMaxFileCount; sIndex++)
    {
        // Meta�� NULL�� �ƴϸ� �̸��� �˻��Ѵ�.
        if(idf::mMetaList[sIndex].mMeta == NULL)
        {
            continue;
        }

        sMeta = idf::mMetaList[sIndex].mMeta;

        // ���� �ý��۳��� ���丮�� ������ �������� �����Ƿ�
        // aName�� Meta�� ������ ���� �̸��� substring�̸� 
        // aName�� ���丮�� �����Ѵ�.
        if((idlOS::strncmp(sMeta->mFileName, aName, sNameLen) == 0) &&
           (idlOS::strlen(sMeta->mFileName) > sNameLen) &&
           ((*(sMeta->mFileName + sNameLen) == '\\') || (*(sMeta->mFileName + sNameLen) == '/')))
        {
            (void)idf::alloc((void **)&sDirentP, ID_SIZEOF(idfDirent));

            sDirentP->mNext = sDir->mFirst;
            sDir->mFirst = sDirentP;

            //==================================================================
            // struct dirent�� platform���� ���°� �ణ�� �ٸ���.
            // ������ d_name�� �������� ���� �ִ�.
            // hpux, linux, aix, dec, windows�� d_name�� char �迭�� �Ǿ��ִ�.
            // �� ���� platform�� d_name�� char[1], char*�̴�.
            // (��ǥ������ Solaris�� char[1]�̰� windows�� struct dirent�� ����.
            //  PD�� windows�� ���� dirent�� �ִ�.)
            // ���� d_name�� char �迭�� �Ǿ����� ���� platform������
            // struct dirent�� ũ�� + ���� �̸� ���� + 1 ��ŭ��
            // struct dirent�� ũ��� �Ͽ� malloc�Ѵ�.
            //==================================================================
#if (!defined(HPUX) && !defined(LINUX) && !defined(AIX) && !defined(DEC)) && !defined(VC_WIN32)
            (void)idf::alloc((void **)&sDirent, 
                             sDirSize + idlOS::strlen(sMeta->mFileName) + 1);
#else
            (void)idf::alloc((void **)&sDirent, sDirSize);
#endif

            //fix for SPARC_SOLARIS 5.10
            //signal SEGV (no mapping at the fault address) in _malloc_unlocked 
            sFence = idlOS::strlen(sMeta->mFileName + sNameLen + 1);

            idlOS::memcpy(sDirent->d_name,
                          sMeta->mFileName + sNameLen + 1,
                          sFence);
            sDirent->d_name[sFence] = '\0';

            sDirentP->mDirent = sDirent;
            sDir->mDirCount++;
        }

        // Meta�� �����ϴ� ������ ��� �о����� �����Ѵ�.
        if(--sNumOfFiles == 0)
        {
            break;
        }
    }

    return sDir;;
}

void idf::getMetaCRC(idfMeta *aMeta)
{
    aMeta->mCRC = idf::getCRC((const UChar*)aMeta,
                              (idf::mMetaSize - ID_SIZEOF(UInt) * 2));
}

void idf::getMasterCRC()
{
    idf::mMaster.mCRC = idf::getCRC((const UChar*)&(idf::mMaster),
                                  (idf::mMasterSize - ID_SIZEOF(UInt) * 2));
}

IDE_RC idf::writeMaster()
{
    idfMaster sBufMaster;

    // Master�� CRC ���� ���Ѵ�.
    idf::getMasterCRC();

    // Master �������� ��ũ�� �����Ѵ�.
    IDE_TEST(idf::lseekFs(idf::mFd, 
                          4, 
                          SEEK_SET) == -1);

#if defined (ENDIAN_IS_BIG_ENDIAN)
    IDE_TEST(idf::writeFs(idf::mFd, 
                          &(idf::mMaster), 
                          idf::mMasterSize) != idf::mMasterSize);
#else
    if(mRecCheck == IDF_START_RECOVERY)
    {
        IDE_TEST(idf::writeFs(idf::mFd, 
                 &(idf::mMaster), 
                 idf::mMasterSize) != idf::mMasterSize);
    }
    else
    {
        // idf::mMaster is not pointer and &(idf::mMaster) cannot be NULL.
        // Therefore code here checking &(idf::mMaster) is NULL is removed

        idlOS::memcpy(&sBufMaster, 
                      &(idf::mMaster), 
                      idf::mMasterSize);

        sBufMaster.mNumOfFiles       = idlOS::hton(sBufMaster.mNumOfFiles);
        sBufMaster.mMajorVersion     = idlOS::hton(sBufMaster.mMajorVersion);
        sBufMaster.mMinorVersion     = idlOS::hton(sBufMaster.mMinorVersion);
        sBufMaster.mAllocedPages     = idlOS::hton(sBufMaster.mAllocedPages);
        sBufMaster.mNumOfPages       = idlOS::hton(sBufMaster.mNumOfPages);
        sBufMaster.mSizeOfPage       = idlOS::hton(sBufMaster.mSizeOfPage);
        sBufMaster.mPagesPerBlock    = idlOS::hton(sBufMaster.mPagesPerBlock);
        sBufMaster.mMaxFileOpenCount = idlOS::hton(sBufMaster.mMaxFileOpenCount);
        sBufMaster.mMaxFileCount     = idlOS::hton(sBufMaster.mMaxFileCount);

        IDE_TEST(idf::writeFs(idf::mFd, 
                              &(sBufMaster), 
                              idf::mMasterSize) != idf::mMasterSize);
    }
#endif

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

SInt idf::fdeof(PDL_HANDLE aHandle)
{
    PDL_OFF_T sEndPos;
    PDL_OFF_T sCurPos;
    SInt      sResult;

    sCurPos = idf::lseek(aHandle, 
                         0, 
                         SEEK_CUR);

    sEndPos = idf::filesize(aHandle);

    if(sEndPos == sCurPos)
    {
        sResult = 1;
    }
    else
    {
        sResult = 0;
    }

    return sResult;
}

SChar *idf::fdgets(SChar *aBuf, SInt aSize, PDL_HANDLE aHandle)
{
    SInt i;
    SInt sSize = aSize - 1;

    for(i = 0; i < sSize; i++)
    {
        if(idf::read(aHandle, aBuf + i, 1) <= 0)
        {
            aBuf[i] = '\0';

            break;
        }

        if(aBuf[i] == '\n')
        {
            break;
        }
    }

    if(i == sSize)
    {
        aBuf[i] = '\0';
    }
    else
    {
        aBuf[i + 1] = '\0';
    }

    if(aBuf[0] == '\0')
    {
        return NULL;
    }

    return aBuf;
}

idBool idf::isVFS()
{
    return (idf::open == idfCore::open) ? ID_TRUE : ID_FALSE;
}

void idf::initLockFile()
{
    PDL_HANDLE sFD;
    SChar      sBuffer[4] = { 0, 0, 0, 0 };
    SChar      sFileName[ID_MAX_FILE_NAME];

    idlOS::snprintf(sFileName,
                    ID_MAX_FILE_NAME,
                    "%s.lock",
                    idf::mFileName);

    if(idlOS::access(sFileName, F_OK) != 0)
    {
        sFD = idlOS::open(sFileName, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);

        IDE_TEST(sFD == PDL_INVALID_HANDLE);

        IDE_TEST(idf::writeFs(sFD,
                              sBuffer,
                              ID_SIZEOF(sBuffer)) != ID_SIZEOF(sBuffer));

        IDE_TEST(idlOS::close(sFD) != 0);
    }

    IDE_TEST(idlOS::flock_init(&idf::mLockFile,
                               O_RDWR,
                               sFileName,
                               0644) != 0);

    IDE_TEST(idlOS::flock_trywrlock(&mLockFile) != 0);

    return;

    IDE_EXCEPTION_END;

    idlOS::printf("Failed to initialize flock: errno=%"ID_INT32_FMT"\n", errno);

    idlOS::exit(-1);
}

void idf::finalLockFile()
{
    IDE_TEST(idlOS::flock_unlock(&idf::mLockFile) != 0);

    IDE_TEST(idlOS::flock_destroy(&idf::mLockFile) != 0);

    return;

    IDE_EXCEPTION_END;

    idlOS::printf("Failed to finalize flock: errno=%"ID_INT32_FMT"\n", errno);

    idlOS::exit(-1);
}

IDE_RC idf::masterLog(UInt aMode)
{
    UInt  sSize;
    UInt  sIndex;
    UInt  sHeaderSize;
    UInt  sMapHeaderSize;
    UInt  sCRC;
    UInt  sCount;
    UInt  sRc;

    idfMeta      *sMeta         = NULL;
    idfMapHeader *sMapHeader    = NULL;
    UInt         *sBufMapLog    = NULL;
    idfMeta      *sBufMetaLog   = NULL;
    idfMaster    *sMasterLog    = NULL;
    idfMapHeader *sMapHeaderLog = NULL;   
    UInt          sNum;
    UInt         *sBufMap;
    idfMeta       sBufMeta;

    idf::mTransCount++;

    //=============================================================
    // Ʈ�������� ������ ���� ������ �ý��� ��Ÿ�����͸� �����Ѵ�.
    // ��Ÿ �����ʹ� �켱 �α׿� ������ �ϰ�, ������ �α��� ũ�Ⱑ
    // ���� ũ�⸦ ������ ��ũ�� �ݿ��Ѵ�.
    // �α׿� ������ ���� idf::mLogBuffer�� ����Ͽ� writeȽ����
    // �����Ѵ�.
    //=============================================================

    if((idf::mTransCount > IDF_LOG_TRANS_SIZE) || (aMode != 0))
    {
        //=================================================
        // �α׿� ������ ��Ÿ�����͸� �����Ѵ�.
        // �α� ��带 ������� ������ �� �κ��� �����Ѵ�.
        //=================================================
        if(idf::mLogMode == IDF_LOG_MODE)
        {
            sHeaderSize = ID_SIZEOF(idfLogHeader);
            sIndex = 0;
            sCount = idf::mLogListCount;

            //=====================
            // 1. Meta�� �����Ѵ�.
            //=====================
            if(sCount > 0)
            {
                sSize = sHeaderSize + idf::mMetaSize;

                while(sCount != 0 && sIndex < idf::mMaxFileCount)
                {
                    if(idf::mLogList[sIndex] == 0)
                    {
                        sIndex++;

                        continue;
                    }

                    sMeta = idf::mMetaList[sIndex].mMeta;

                    if(sMeta == NULL)
                    {
                        (void)idf::alloc((void **)&sMeta, idf::mMetaSize);

                        idlOS::memset(sMeta, 
                                      0x00, 
                                      idf::mMetaSize);
                    }

                    idf::mLogHeader->mFileID = idlOS::hton(sIndex);
                    idf::mLogHeader->mType   = idlOS::hton(IDF_META_LOG);

                    idf::getMetaCRC(sMeta);

                    if(idf::mLogBufferCursor + sSize > IDF_LOG_BUFFER_SIZE)
                    {
                        sRc = idf::writeFs(idf::mLogFd, 
                                           idf::mLogBuffer, 
                                           idf::mLogBufferCursor);

                        IDE_TEST(sRc != idf::mLogBufferCursor);

                        idf::mLogBufferCursor = 0;
                    }

                    idlOS::memcpy((idf::mLogBuffer + idf::mLogBufferCursor), 
                                  idf::mLogHeader, 
                                  sHeaderSize);

                    idf::mLogBufferCursor += sHeaderSize;

                    idlOS::memcpy((idf::mLogBuffer + idf::mLogBufferCursor), 
                                  sMeta, 
                                  idf::mMetaSize);

#if !defined (ENDIAN_IS_BIG_ENDIAN)
                    sBufMetaLog = (idfMeta *)(idf::mLogBuffer + idf::mLogBufferCursor); 

                    sBufMetaLog->mFileID      = idlOS::hton(sBufMetaLog->mFileID);
                    sBufMetaLog->mSize        = idlOS::hton(sBufMetaLog->mSize);
                    sBufMetaLog->mCTime       = idlOS::hton(sBufMetaLog->mCTime);
                    sBufMetaLog->mNumOfPagesU = idlOS::hton(sBufMetaLog->mNumOfPagesU);
                    sBufMetaLog->mNumOfPagesA = idlOS::hton(sBufMetaLog->mNumOfPagesA);

                    for(sNum = 0; sNum < IDF_PAGES_PER_BLOCK; sNum++)
                    {
                        sBufMetaLog->mDirectPages[sNum] = idlOS::hton(sBufMetaLog->mDirectPages[sNum]);
                    }

                    for(sNum = 0; sNum < IDF_INDIRECT_PAGE_NUM; sNum++)
                    {
                        sBufMetaLog->mIndPages[sNum] = idlOS::hton(sBufMetaLog->mIndPages[sNum]);
                    }

                    sCRC = getCRC((UChar*)sBufMetaLog, idf::mMetaSize - (ID_SIZEOF(UInt) * 2));

                    sBufMetaLog->mCRC = idlOS::hton(sCRC);
#endif

                    idf::mLogBufferCursor += idf::mMetaSize;

                    sCount--;

                    if(idf::mMetaList[sIndex].mMeta == NULL)
                    {
                        idf::free(sMeta);
                    }

                    sIndex++;
                }
            }

            //========================
            // 2. PageMap�� �����Ѵ�.
            //========================
            if(idf::mMapLogCount > 0)
            {
                sMapHeaderSize = ID_SIZEOF(idfMapHeader);

                sSize = sHeaderSize + sMapHeaderSize + idf::mIndEntrySize + ID_SIZEOF(UInt);

                idf::mLogHeader->mType   = idlOS::hton(IDF_PAGEMAP_LOG);

                for(sIndex = 0; sIndex < idf::mMapLogCount; sIndex++)
                {
                    if(idf::mLogBufferCursor + sSize > IDF_LOG_BUFFER_SIZE)
                    {
                        sRc = idf::writeFs(idf::mLogFd, 
                                           idf::mLogBuffer, 
                                           idf::mLogBufferCursor);

                        IDE_TEST(sRc != idf::mLogBufferCursor);

                        idf::mLogBufferCursor = 0;
                    }

                    sMapHeader = &(idf::mMapHeader[sIndex]);

                    idf::mLogHeader->mFileID = idlOS::hton(sMapHeader->mFileID);
 
                    idlOS::memcpy((idf::mLogBuffer + idf::mLogBufferCursor), 
                                  idf::mLogHeader, 
                                  sHeaderSize);

                    idf::mLogBufferCursor += sHeaderSize;

                    idlOS::memcpy((void*)(idf::mLogBuffer + idf::mLogBufferCursor), 
                                  sMapHeader, 
                                  sMapHeaderSize);

#if !defined (ENDIAN_IS_BIG_ENDIAN)
                    sMapHeaderLog = (idfMapHeader *)(idf::mLogBuffer + idf::mLogBufferCursor);
                    
                    sMapHeaderLog->mFileID   = idlOS::hton(sMapHeaderLog->mFileID);
                    sMapHeaderLog->mIndex    = idlOS::hton(sMapHeaderLog->mIndex);
                    sMapHeaderLog->mMapIndex = idlOS::hton(sMapHeaderLog->mMapIndex);
#endif
 
                    idf::mLogBufferCursor += sMapHeaderSize;

                    idlOS::memcpy((void*)(idf::mLogBuffer + idf::mLogBufferCursor), 
                                  (void*)&(idf::mMapLog[idf::mPagesPerBlock * sIndex]), 
                                  idf::mIndEntrySize);

#if !defined (ENDIAN_IS_BIG_ENDIAN)
                    sBufMapLog = (UInt *) (idf::mLogBuffer + idf::mLogBufferCursor);
                    
                    for(sNum= 0; sNum < idf::mPagesPerBlock; sNum++)
                    {
                        sBufMapLog[sNum] = idlOS::hton(sBufMapLog[sNum]);
                    }
#endif

                    idf::mLogBufferCursor += idf::mIndEntrySize;

#if defined (ENDIAN_IS_BIG_ENDIAN)
                    sCRC = idf::getCRC((const UChar*)&(idf::mMapLog[idf::mPagesPerBlock * sIndex]),
                            idf::mIndEntrySize);
#else
                    sCRC = idf::getCRC((const UChar*)&(sBufMapLog[0]),
                            idf::mIndEntrySize);

                    sCRC = idlOS::hton(sCRC);
#endif

                    idlOS::memcpy((void*)(idf::mLogBuffer + idf::mLogBufferCursor), 
                                  (void*)(&sCRC), 
                                  ID_SIZEOF(UInt));

                    idf::mLogBufferCursor += ID_SIZEOF(UInt);
                }
            }

            //=======================
            // 3. Master�� �����Ѵ�.
            //=======================
            if(idf::mLogBufferCursor + idf::mMasterSize > IDF_LOG_BUFFER_SIZE)
            {
                sRc = idf::writeFs(idf::mLogFd,
                                   idf::mLogBuffer,
                                   idf::mLogBufferCursor);

                IDE_TEST(sRc != idf::mLogBufferCursor);

                idf::mLogBufferCursor = 0;
            }

            idf::mLogHeader->mFileID = 0;
            idf::mLogHeader->mType   = idlOS::hton(IDF_MASTER_LOG);

            getMasterCRC();

            idlOS::memcpy((idf::mLogBuffer + idf::mLogBufferCursor),
                          idf::mLogHeader,
                          sHeaderSize);

            idf::mLogBufferCursor += sHeaderSize;

            idlOS::memcpy((idf::mLogBuffer + idf::mLogBufferCursor),
                          &(idf::mMaster),
                          idf::mMasterSize);

#if !defined (ENDIAN_IS_BIG_ENDIAN)
            sMasterLog = (idfMaster *) (idf::mLogBuffer + idf::mLogBufferCursor);
            sMasterLog->mNumOfFiles       = idlOS::hton(sMasterLog->mNumOfFiles);
            sMasterLog->mMajorVersion     = idlOS::hton(sMasterLog->mMajorVersion);
            sMasterLog->mMinorVersion     = idlOS::hton(sMasterLog->mMinorVersion);
            sMasterLog->mAllocedPages     = idlOS::hton(sMasterLog->mAllocedPages);
            sMasterLog->mNumOfPages       = idlOS::hton(sMasterLog->mNumOfPages);
            sMasterLog->mSizeOfPage       = idlOS::hton(sMasterLog->mSizeOfPage);
            sMasterLog->mPagesPerBlock    = idlOS::hton(sMasterLog->mPagesPerBlock);
            sMasterLog->mMaxFileOpenCount = idlOS::hton(sMasterLog->mMaxFileOpenCount);
            sMasterLog->mMaxFileCount     = idlOS::hton(sMasterLog->mMaxFileCount);

            sCRC = getCRC((UChar*)sMasterLog, idf::mMasterSize - (ID_SIZEOF(UInt) * 2));

            sMasterLog->mCRC = idlOS::hton (sCRC);
#endif

            idf::mLogBufferCursor += idf::mMasterSize;

            sRc = idf::writeFs(idf::mLogFd,
                               idf::mLogBuffer,
                               idf::mLogBufferCursor);

            IDE_TEST(sRc != idf::mLogBufferCursor);

            idf::mLogBufferCursor = 0;

            IDE_TEST(idf::fsyncFs(idf::mLogFd) == -1);
        }

        //========================================
        // ��ũ�� ������ ��Ÿ�����͸� ����Ѵ�.
        //========================================

        //=====================
        // 1. Meta�� �����Ѵ�.
        //=====================
        if(idf::mLogListCount > 0)
        {
            sIndex = 0;

            while(idf::mLogListCount != 0 && sIndex < idf::mMaxFileCount)
            {
                if(idf::mLogList[sIndex] == 0)
                {
                    sIndex++;

                    continue;
                }

                idf::mLogList[sIndex] = 0;

                sMeta = idf::mMetaList[sIndex].mMeta;

                if(sMeta == NULL)
                {
                    (void)idf::alloc((void **)&sMeta, idf::mMetaSize);

                    idlOS::memset(sMeta, 
                                  0x00, 
                                  idf::mMetaSize);
                }

                idf::mLogHeader->mFileID = idlOS::hton(sIndex);
                idf::mLogHeader->mType   = idlOS::hton(IDF_META_LOG);

                // aFileID�� �ش��ϴ� Meta�� ��ũ�� �����Ѵ�.
                IDE_TEST(idf::lseekFs(idf::mFd,
                                      IDF_FIRST_META_POSITION + sIndex * IDF_META_SIZE,
                                      SEEK_SET) == -1); 

#if defined (ENDIAN_IS_BIG_ENDIAN)
                IDE_TEST(idf::writeFs(idf::mFd, 
                                      sMeta, 
                                      idf::mMetaSize) != idf::mMetaSize);
#else
                idlOS::memcpy(&sBufMeta , sMeta, idf::mMetaSize);
                
                sBufMeta.mFileID      = idlOS::hton(sBufMeta.mFileID);
                sBufMeta.mSize        = idlOS::hton(sBufMeta.mSize);
                sBufMeta.mCTime       = idlOS::hton(sBufMeta.mCTime);
                sBufMeta.mNumOfPagesU = idlOS::hton(sBufMeta.mNumOfPagesU);
                sBufMeta.mNumOfPagesA = idlOS::hton(sBufMeta.mNumOfPagesA);
                sBufMeta.mCRC         = idlOS::hton(sBufMeta.mCRC);

                for(sNum = 0; sNum < IDF_PAGES_PER_BLOCK; sNum++)
                {
                    sBufMeta.mDirectPages[sNum] = idlOS::hton(sBufMeta.mDirectPages[sNum]);
                }

                for(sNum = 0; sNum < IDF_INDIRECT_PAGE_NUM; sNum++)
                {
                    sBufMeta.mIndPages[sNum] = idlOS::hton(sBufMeta.mIndPages[sNum]);
                }

                IDE_TEST(idf::writeFs(idf::mFd, 
                            &sBufMeta, 
                            idf::mMetaSize) != idf::mMetaSize);
#endif

                if(idf::mMetaList[sIndex].mMeta == NULL)
                {
                    idf::free(sMeta);
                }

                sIndex++;

                idf::mLogListCount--;
            }
        }

        //========================
        // 2. PageMap�� �����Ѵ�.
        //========================
        if(idf::mMapLogCount > 0)
        {
            idf::mLogHeader->mType = idlOS::hton(IDF_PAGEMAP_LOG);

            for(sIndex = 0; sIndex < idf::mMapLogCount; sIndex++)
            {
                sMapHeader = &(idf::mMapHeader[sIndex]);

                IDE_TEST(idf::lseekFs(idf::mFd,
                                      ((sMapHeader->mIndex * idf::mPageSize) + (sMapHeader->mMapIndex * ID_SIZEOF(UInt))),
                                      SEEK_SET) == -1);

#if defined (ENDIAN_IS_BIG_ENDIAN)
                sRc = idf::writeFs(idf::mFd,
                                   (void*)&(idf::mMapLog[idf::mPagesPerBlock * sIndex]),
                                   idf::mIndEntrySize);
#else
                idlOS::memcpy((void*)&sBufMap,
                        (void*)&idf::mMapLog,
                        idf::mIndEntrySize);

                for(sNum = sIndex; sNum < idf::mPagesPerBlock * (sIndex + 1); sNum++)
                {
                    sBufMap[sNum] = idlOS::hton(sBufMap[sNum]);
                }

                sRc = idf::writeFs(idf::mFd,
                        (void*)&(sBufMap[idf::mPagesPerBlock * sIndex]),
                        idf::mIndEntrySize);
#endif
 
                IDE_TEST(sRc != idf::mIndEntrySize);
            }

            // ������ �� �α��� ������ 0���� �ʱ�ȭ �Ѵ�.
            idf::mMapLogCount = 0;
        }

        //=======================
        // 3. Master�� �����Ѵ�.
        //=======================
        IDE_TEST(idf::writeMaster() != IDE_SUCCESS);

        if(idlOS::filesize(idf::mLogFd) > 10*1024*1024)
        {
            IDE_TEST(idf::fsyncFs(idf::mFd) == -1);

            IDE_TEST(idf::initLog() != IDE_SUCCESS);

            idf::mIsSync = ID_TRUE;
        }

        idf::mTransCount = 0;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC idf::appandPageMapLog(SInt aFileID, SInt aIndex, SInt aMapIndex)
{
    UInt sIndex;
    UInt sPageIndex;

    idfMapHeader *sMapHeader = NULL;
    idfMeta      *sMeta = NULL;

    sMeta = idf::mMetaList[aFileID].mMeta;

    sPageIndex = sMeta->mIndPages[aIndex];

    for(sIndex = 0; sIndex < idf::mMapLogCount; sIndex++)
    {
        sMapHeader = &(idf::mMapHeader[sIndex]);

        if((sMapHeader->mFileID == aFileID) &&
           (sMapHeader->mIndex == sPageIndex) &&
           (sMapHeader->mMapIndex == (UInt)aMapIndex))
        {
            break;
        }
    }

    if(sIndex < IDF_PAGE_MAP_LOG_SIZE)
    {
        if(sIndex == idf::mMapLogCount)
        {
            sMapHeader = &(idf::mMapHeader[sIndex]);

            sMapHeader->mFileID = aFileID;
            sMapHeader->mIndex = sMeta->mIndPages[aIndex];
            sMapHeader->mMapIndex = aMapIndex;

            idf::mMapLogCount++;
        }

        idlOS::memcpy((void*)(&idf::mMapLog[(idf::mPagesPerBlock * sIndex)]),
                      (void*)idf::mPageMapPool->mMap,
                      idf::mIndEntrySize);
    }
    else
    {
        IDE_ASSERT(0);
    }

    return IDE_SUCCESS;
}

IDE_RC idf::initLog()
{
    if(idf::mLogMode == IDF_LOG_MODE)
    {
        IDE_TEST(idf::mLogFd == PDL_INVALID_HANDLE);

        IDE_TEST(idlOS::ftruncate(idf::mLogFd, 0) == -1);

        IDE_TEST(idf::lseekFs(idf::mLogFd, 
                              0, 
                              SEEK_SET) == -1);

        idf::mLogHeader->mFileID = 0;
        idf::mLogHeader->mType   = IDF_MASTER_LOG;

        getMasterCRC();

        IDE_TEST(idf::writeFs(idf::mLogFd,
                              idf::mLogHeader,
                              ID_SIZEOF(idfLogHeader)) != ID_SIZEOF(idfLogHeader));

        IDE_TEST(idf::writeFs(idf::mLogFd, 
                              &(idf::mMaster), 
                              idf::mMasterSize) != idf::mMasterSize);

        IDE_TEST(idf::fsyncFs(idf::mLogFd) == -1);
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC idf::doRecovery()
{
    idfMaster     sMaster;
    idfMeta       sMeta1;
    idfLogHeader  sHeader;
    idfMapHeader  sMapHeader;

    UInt         *sPageMap   = NULL;
    idfMeta      *sMeta      = NULL;
    SInt         *sCheckList = NULL;

    SInt          sHeaderSize;
    SInt          sMapHeaderSize;
    UInt          sCRC;
    SInt          sTxCount = 0;
    SInt          sRc;
    SInt          sState = 0;
    UInt          sIndex;

    // �α׸� ������� ������ Recovery�� �������� �ʴ´�.
    if(idf::mLogMode == IDF_NO_LOG_MODE)
    {
        IDE_RAISE(pass);
    }

    if(idlOS::filesize(idf::mLogFd) == 0)
    {
        IDE_RAISE(pass);
    }

    sHeaderSize    = ID_SIZEOF(idfLogHeader);
    sMapHeaderSize = ID_SIZEOF(idfMapHeader);

    IDE_TEST(idf::lseekFs(idf::mLogFd, 
                          0, 
                          SEEK_SET) == -1);

    sRc = idf::readFs(idf::mLogFd, 
                      &sHeader, 
                      sHeaderSize);

    if(sRc != sHeaderSize)
    {
        IDE_RAISE(pass2);
    }

    sRc = idf::readFs(idf::mLogFd, 
                      &sMaster, 
                      idf::mMasterSize);

    if(sRc != idf::mMasterSize)
    {
        IDE_RAISE(pass2);
    }

    if(sMaster.mSignature != idlOS::hton(IDF_SYSTEM_SIGN))
    {
        IDE_RAISE(pass2);
    }

    sCRC = getCRC((UChar*)&sMaster, idf::mMasterSize - (ID_SIZEOF(UInt) * 2));

    sCRC = idlOS::hton(sCRC);

    if(sCRC != sMaster.mCRC)
    {
        IDE_RAISE(pass2);
    }

    sMaster.mPagesPerBlock    = idlOS::ntoh(sMaster.mPagesPerBlock);
    sMaster.mMaxFileOpenCount = idlOS::ntoh(sMaster.mMaxFileOpenCount);

    // �ӽ÷� �ʱ�ȭ �Ѵ�.
    idf::mIndEntrySize = ID_SIZEOF(UInt) * sMaster.mPagesPerBlock;
    idf::mMaxFileOpenCount = sMaster.mMaxFileOpenCount;

    sTxCount++;

    (void)idf::alloc((void **)&sPageMap, idf::mIndEntrySize);

    (void)idf::alloc((void **)&sCheckList, 
                     idf::mMaxFileOpenCount * ID_SIZEOF(UInt));

    (void)idf::alloc((void **)&sMeta,
                     idf::mMetaSize * idf::mMaxFileCount);

    for(sIndex = 0; sIndex < idf::mMaxFileCount; sIndex++)
    {
        sCheckList[sIndex] = 0;
    }

    //===============
    // 1. Test Phase
    //===============

    while((sRc = idf::readFs(idf::mLogFd, 
                             &sHeader, 
                             sHeaderSize)) == sHeaderSize)
    {

        sHeader.mType = idlOS::hton(sHeader.mType);
        
        switch(sHeader.mType)
        {
        case IDF_MASTER_LOG:
            sRc = idf::readFs(idf::mLogFd, 
                              &sMaster, 
                              idf::mMasterSize);

            if(sRc != idf::mMasterSize)
            {
                sState = 1;
                break;
            }

            if(sMaster.mSignature != idlOS::hton(IDF_SYSTEM_SIGN))
            {
                sState = 1;
                break;
            }

            sCRC = getCRC((UChar*)&sMaster,
                          idf::mMasterSize - (ID_SIZEOF(UInt) * 2));

            sCRC = idlOS::hton(sCRC);

            if(sCRC != sMaster.mCRC)
            {
                sState = 1;
                break;
            }

            sTxCount++;

            sState = 0;

            break;
        case IDF_META_LOG:
            sRc = idf::readFs(idf::mLogFd, 
                              &sMeta1, 
                              idf::mMetaSize);

            if(sRc != idf::mMetaSize)
            {
                sState = 1;
                break;
            }

            sCRC = getCRC((UChar*)&sMeta1,
                          idf::mMetaSize - (ID_SIZEOF(UInt) * 2));

            sCRC = idlOS::hton(sCRC);

            sMeta1.mFileID = idlOS::ntoh(sMeta1.mFileID);

            if(sCRC != sMeta1.mCRC)
            {
                sState = 1;
                break;
            }

            sCheckList[sMeta1.mFileID]++;

            break;
        case IDF_PAGEMAP_LOG:
            sRc = idf::readFs(idf::mLogFd, 
                              &sMapHeader, 
                              sMapHeaderSize);

            if(sRc != sMapHeaderSize)
            {
                sState = 1;
                break;
            }

            sRc = idf::readFs(idf::mLogFd, 
                              sPageMap, 
                              idf::mIndEntrySize);

            if(sRc != (SInt)idf::mIndEntrySize)
            {
                sState = 1;
                break;
            }

            sRc = idf::readFs(idf::mLogFd, 
                              &sCRC, 
                              4);

            if(sRc != 4)
            {
                sState = 1;
                break;
            }

            sRc = idf::getCRC((UChar*)sPageMap, idf::mIndEntrySize);
 
            sRc = idlOS::hton(sRc);
            
            if((UInt)sRc != sCRC)
            {
                sState = 1;
                break;
            }

            break;
        default:
            //�α������� ������ �ִ�.
            sState = 1;
            break;
        }

        if(sState == 1)
        {
            break;
        }
    }

    //===================
    // 2. Recovery Phase
    //===================

    IDE_TEST(idf::lseekFs(idf::mLogFd, 
                          0, 
                          SEEK_SET) == -1);

    while((sRc = idf::readFs(idf::mLogFd, 
                             &sHeader, 
                             sHeaderSize)) == sHeaderSize)
    {
        if(sTxCount <= 0)
        {
            break;
        }

        sHeader.mType = idlOS::hton(sHeader.mType);
 
        switch(sHeader.mType)
        {
        case IDF_MASTER_LOG:
            sRc = idf::readFs(idf::mLogFd, 
                              &sMaster, 
                              idf::mMasterSize);

            IDE_ASSERT(sRc == idf::mMasterSize);

            // �������� Master�� ����ϵ��� �Ѵ�.
            if(sTxCount == 1)
            {
                IDE_ASSERT(sMaster.mSignature == idlOS::hton(IDF_SYSTEM_SIGN));

                sCRC = getCRC((UChar*)&sMaster,
                              idf::mMasterSize - (ID_SIZEOF(UInt) * 2));

                sCRC = idlOS::hton(sCRC);

                IDE_ASSERT(sCRC == sMaster.mCRC);

                if(idlOS::memcmp((void*)&idf::mMaster,
                                 (void*)&sMaster,
                                 idf::mMasterSize) != 0)
                {
                    idlOS::memcpy((void*)&idf::mMaster,
                                  (void*)&sMaster,
                                  idf::mMasterSize);

                    sTxCount = -1;
                }
                else
                {
                    sTxCount = 0;
                }
            }
            else
            {
                sTxCount--;
            }

            break;
        case IDF_META_LOG:
            sRc = idf::readFs(idf::mLogFd, 
                              &sMeta1, 
                              idf::mMetaSize);

            sHeader.mFileID = idlOS::ntoh(sHeader.mFileID);

            sCheckList[sHeader.mFileID]--;

            IDE_ASSERT(sRc == idf::mMetaSize);

            // ������ Meta�� ����ϵ��� �Ѵ�.
            if(sCheckList[sHeader.mFileID] == 0 || sTxCount == 1)
            {
                sCRC = getCRC((UChar*)&sMeta1,
                              idf::mMetaSize - (ID_SIZEOF(UInt) * 2));

                sCRC = idlOS::hton(sCRC);

                IDE_ASSERT(sCRC == sMeta1.mCRC);

                idlOS::memcpy(&(sMeta[sHeader.mFileID]), 
                              &sMeta1, 
                              idf::mMetaSize);

                sCheckList[sHeader.mFileID] = -1;
            }

            break;
        case IDF_PAGEMAP_LOG:
            // Page Map Log�� CRC���� �ùٸ��� ����ϵ��� �Ѵ�.
            sRc = idf::readFs(idf::mLogFd, 
                              &sMapHeader, 
                              sMapHeaderSize);

            sMapHeader.mFileID   = idlOS::ntoh(sMapHeader.mFileID);
            sMapHeader.mIndex    = idlOS::ntoh(sMapHeader.mIndex);
            sMapHeader.mMapIndex = idlOS::ntoh(sMapHeader.mMapIndex);

            IDE_ASSERT(sRc == sMapHeaderSize);

            sRc = idf::readFs(idf::mLogFd, 
                              sPageMap, 
                              idf::mIndEntrySize);
            
            IDE_ASSERT(sRc == (SInt)idf::mIndEntrySize);
            
            sRc = idf::readFs(idf::mLogFd, 
                              &sCRC, 
                              4);

            IDE_ASSERT(sRc == 4);

            sRc = idf::getCRC((UChar*)sPageMap, idf::mIndEntrySize);
 
            sRc = idlOS::hton(sRc);
           
            IDE_ASSERT((UInt)sRc == sCRC);

            IDE_TEST(idf::lseekFs(idf::mFd,
                                  sMapHeader.mIndex * idf::mPageSize + sMapHeader.mMapIndex * ID_SIZEOF(UInt),
                                  SEEK_SET) == -1);

#ifdef __CSURF__
            IDE_ASSERT( *sPageMap > idf::mIndEntrySize );
#endif
            idf::writeFs(idf::mFd, 
                         sPageMap, 
                         idf::mIndEntrySize);

            break;
        default:
            IDE_ASSERT(0);
            break;
        }
    }

    for(sIndex = 0; sIndex < idf::mMaxFileCount; sIndex++)
    {
        if(sCheckList[sIndex] == -1) // ����Ѵ�.
        {
            // Meta�� CRC ���� ���Ѵ�.
            (void)idf::getMetaCRC(&(sMeta[sIndex]));

            // aFileID�� �ش��ϴ� Meta�� ��ũ�� �����Ѵ�.
            IDE_TEST(idf::lseekFs(idf::mFd,
                                  IDF_FIRST_META_POSITION + sIndex * IDF_META_SIZE,
                                  SEEK_SET) == -1); 

            IDE_TEST(idf::writeFs(idf::mFd, 
                                  &(sMeta[sIndex]), 
                                  idf::mMetaSize) != idf::mMetaSize);
        }
    }

    mRecCheck = IDF_START_RECOVERY;

    IDE_TEST(idf::writeMaster() != IDE_SUCCESS);

    mRecCheck = IDF_END_RECOVERY;

    // Recovery�� �Ϸ�Ǿ����� ��ũ�� ����ȭ �ϰ� �α׸� �ʱ�ȭ �Ѵ�.
    IDE_TEST(idf::fsyncFs(idf::mFd) == -1);

    IDE_EXCEPTION_CONT(pass2);

    IDE_TEST(idf::initLog() != IDE_SUCCESS);

    if(sPageMap != NULL)
    {
        (void)idf::free(sPageMap);

        sPageMap = NULL;
    }

    if(sCheckList != NULL)
    {
        (void)idf::free(sCheckList);

        sCheckList = NULL;
    }

    if(sMeta != NULL)
    {
        (void)idf::free(sMeta);

        sMeta = NULL;
    }

    IDE_EXCEPTION_CONT(pass);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if(sPageMap != NULL)
    {
        (void)idf::free(sPageMap);
    }

    if(sCheckList != NULL)
    {
        (void)idf::free(sCheckList);
    }

    if(sMeta != NULL)
    {
        (void)idf::free(sMeta);
    }

    return IDE_FAILURE;
}

void idf::alloc(void **aBuffer, size_t aSize)
{
    (void)gAlloca.alloc(aBuffer, aSize);
}

void idf::free(void *aBuffer)
{
    (void)gAlloca.free(aBuffer);
}

ssize_t idf::writeFs(PDL_HANDLE aFd, const void* aBuf, size_t aCount)
{
#if !defined(USE_RAW_DEVICE) || defined(WRS_VXWORKS)
    return idlOS::write(aFd, 
                        aBuf, 
                        aCount);
#else
    SChar *sBufW    = NULL;
    SChar *sBufR    = NULL;
    UInt   sSize    = aCount;
    UInt   sWsize   = 0;
    UInt   sRemnant = 0;
    SInt   sCursor  = 0;
    SInt   sRewind  = 0;

    IDE_ASSERT(aCount <= IDF_PAGE_SIZE);

    //32K write buffer
    sBufW = gAlloca.getBufferW();

    //IDF_ALIGNED_BUFFER_SIZE read buffer
    sBufR = gAlloca.getBufferR();

    idlOS::memcpy(sBufW,
                  aBuf,
                  aCount);

    sCursor = idf::lseekFs(aFd,
                           0,
                           SEEK_CUR);

    IDE_TEST(sCursor == -1);

    sRewind = sCursor;

    while(sSize > 0)
    {
        sRemnant = sCursor % IDF_ALIGNED_BUFFER_SIZE;

        if((sRemnant == 0) && (sSize >= IDF_ALIGNED_BUFFER_SIZE))
        {
            sWsize = IDF_ALIGNED_BUFFER_SIZE;

            IDE_TEST(idf::lseekFs(aFd,
                                  sCursor,
                                  SEEK_SET) == -1);

            IDE_TEST(idlOS::write(aFd,
                                  sBufW + aCount - sSize,
                                  sWsize) != sWsize);
        }
        else
        {
            idlOS::memset(sBufR,
                          0x00,
                          IDF_ALIGNED_BUFFER_SIZE);

            if(sRemnant == 0)
            {
                sWsize = sSize;
            }
            else
            {
                sWsize = (IDF_ALIGNED_BUFFER_SIZE - sRemnant) < sSize ? 
                         (IDF_ALIGNED_BUFFER_SIZE - sRemnant) : sSize;
            }

            IDE_TEST(idf::lseekFs(aFd,
                                  sCursor - sRemnant,
                                  SEEK_SET) == -1);

            //���� ũ�Ⱑ IDF_ALIGNED_BUFFER_SIZE ���� ���� ��쵵 �ִ�. 
            IDE_TEST(idlOS::read(aFd,
                                 sBufR,
                                 IDF_ALIGNED_BUFFER_SIZE) == -1);

            idlOS::memcpy(sBufR + sRemnant,
                          sBufW + aCount - sSize,
                          sWsize);
 
            IDE_TEST(idf::lseekFs(aFd,
                                  sCursor - sRemnant,
                                  SEEK_SET) == -1);
  
            IDE_TEST(idlOS::write(aFd,
                                  sBufR,
                                  IDF_ALIGNED_BUFFER_SIZE) != IDF_ALIGNED_BUFFER_SIZE);
        }

        sSize -= sWsize;
        sCursor += sWsize;
    }

    sRewind += (aCount - sSize);

    IDE_TEST(idf::lseekFs(aFd,
                          sRewind,
                          SEEK_SET) == -1);

    return aCount - sSize;

    IDE_EXCEPTION_END;

    return -1;
#endif
}

ssize_t idf::readFs(PDL_HANDLE aFd, void *aBuf, size_t aCount)
{
#if !defined(USE_RAW_DEVICE) || defined(WRS_VXWORKS)
    return idlOS::read(aFd, 
                       aBuf, 
                       aCount);
#else
    SChar *sBufW    = NULL;
    SChar *sBufR    = NULL;
    UInt   sSize    = aCount;
    UInt   sRsize   = 0;
    UInt   sRemnant = 0;
    SInt   sCursor  = 0;
    SInt   sRewind  = 0;

    IDE_ASSERT(aCount <= IDF_PAGE_SIZE);

    //32K write buffer
    sBufW = gAlloca.getBufferW();

    //IDF_ALIGNED_BUFFER_SIZE read buffer
    sBufR = gAlloca.getBufferR();

    sCursor = idf::lseekFs(aFd,
                           0,
                           SEEK_CUR);

    IDE_TEST(sCursor == -1);

    sRewind = sCursor;

    while(sSize > 0)
    {
        sRemnant = sCursor % IDF_ALIGNED_BUFFER_SIZE;

        if((sRemnant == 0) && (sSize >= IDF_ALIGNED_BUFFER_SIZE))
        {
            sRsize = IDF_ALIGNED_BUFFER_SIZE;

            IDE_TEST(idf::lseekFs(aFd,
                                  sCursor,
                                  SEEK_SET) == -1);

            IDE_TEST(idlOS::read(aFd,
                                 sBufR,
                                 sRsize) != sRsize);

            idlOS::memcpy(sBufW + aCount - sSize,
                          sBufR,
                          sRsize);
        }
        else
        {
            if(sRemnant == 0)
            {
                sRsize = sSize;
            }
            else
            {
                sRsize = (IDF_ALIGNED_BUFFER_SIZE - sRemnant) < sSize ? 
                         (IDF_ALIGNED_BUFFER_SIZE - sRemnant) : sSize;
            }

            IDE_TEST(idf::lseekFs(aFd,
                                  sCursor - sRemnant,
                                  SEEK_SET) == -1);
 
            //���� ũ�Ⱑ IDF_ALIGNED_BUFFER_SIZE ���� ���� ��쵵 �ִ�. 
            IDE_TEST(idlOS::read(aFd,
                                 sBufR,
                                 IDF_ALIGNED_BUFFER_SIZE) == -1);

            idlOS::memcpy(sBufW + aCount - sSize,
                          sBufR + sRemnant,
                          sRsize);
        }

        sSize -= sRsize;
        sCursor += sRsize;
    }

    idlOS::memcpy(aBuf,
                  sBufW,
                  aCount - sSize);

    sRewind += (aCount - sSize);

    IDE_TEST(idf::lseekFs(aFd,
                          sRewind,
                          SEEK_SET) == -1);

    return aCount - sSize;

    IDE_EXCEPTION_END;

    return -1;
#endif
}

PDL_OFF_T idf::lseekFs(PDL_HANDLE aFd, PDL_OFF_T aOffset, SInt aWhence)
{
#if !defined(WRS_VXWORKS)
    return idlOS::lseek(aFd, 
                        aOffset, 
                        aWhence);
#else
    SInt sCursor = 0;
    SInt sRc     = 0;

    if(aWhence == SEEK_SET)
    {
        sCursor = aOffset;;

        sRc = ::ioctl(aFd,
                      FIOSEEK,
                      sCursor);
    }
    else
    {
        if(aWhence == SEEK_CUR)
        {
            sCursor = ::ioctl(aFd, 
                              FIOWHERE, 
                              0);

            IDE_TEST(sCursor == -1);

            sCursor += aOffset;

            sRc = ::ioctl(aFd,
                          FIOSEEK,
                          sCursor);
        }
        else
        {
            IDE_ASSERT(0);
        }
    }

    return sRc;

    IDE_EXCEPTION_END;

    return -1;
#endif
}

SInt idf::fsyncFs(PDL_HANDLE aFd)
{
#if !defined(WRS_VXWORKS)
    return idlOS::fsync(aFd);
#else
    return ::ioctl(aFd, 
                   FIOFLUSH, 
                   0);
#endif
}

#if defined(PDL_LACKS_PREADV)
ssize_t idf::preadv(PDL_HANDLE aFD, const iovec* aVec, int aCount, PDL_OFF_T aWhere)
{
    ssize_t sRet;
    IDE_TEST( ::lseek(aFD, aWhere, SEEK_SET) == -1 );
    sRet = ::readv(aFD, aVec, aCount);

    return sRet;

    IDE_EXCEPTION_END;
    return -1;
}

ssize_t idf::pwritev(PDL_HANDLE aFD, const iovec* aVec, int aCount, PDL_OFF_T aWhere)
{
    ssize_t sRet;
    IDE_TEST( ::lseek(aFD, aWhere, SEEK_SET) == -1 );
    sRet = ::writev(aFD, aVec, aCount);

    return sRet;

    IDE_EXCEPTION_END;
    return -1;
}
#endif

