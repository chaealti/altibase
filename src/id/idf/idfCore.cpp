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

PDL_HANDLE idfCore::open(const SChar *aPathName, SInt aFlag, ...)
{
    SInt     sFileID;
    SChar   *sPath = NULL;
    SInt     sFd;
    SInt     sIndex;
    idfMeta *sMeta = NULL;
    idBool   sLock = ID_FALSE;

    (void)idf::lock();
    sLock = ID_TRUE;

    IDE_TEST_RAISE((sFd = idf::getUnusedFd()) == IDF_INVALID_HANDLE, fd_error);

    sPath = idf::getpath(aPathName);

    IDE_TEST(sPath == NULL);

    // ������ �����ϴ� ���
    if((sFileID = idf::getFileIDByName(sPath)) != IDF_INVALID_FILE_ID) 
    {
        // O_TRUNC �Ӽ��� ������ ������ ũ�⸦ 0���� �����.
        if(aFlag & O_TRUNC)
        {
            sMeta = idf::mMetaList[sFileID].mMeta;

            sMeta->mSize = 0;

            idf::mFdList[sFd].mFileID = sFileID;
            idf::mFdList[sFd].mCursor = 0;

            idf::mMetaList[sFileID].mFileOpenCount++;
        }
        else
        {
            idf::mFdList[sFd].mFileID = sFileID;
            idf::mFdList[sFd].mCursor = 0;

            // ������ ���� ȸ��(mFileOpenCount)�� �����ϰ�,
            // idfCore::close() ȣ��� �ϳ��� �����Ѵ�.
            idf::mMetaList[sFileID].mFileOpenCount++;
        }
    }
    // ������ �������� ������ aFlag�� �����Ͽ� O_CREAT�� ������ ������
    // �����Ѵ�. ó�� ������ ������ ���� �������� �Ҵ����� �ʴ´�.
    else
    {
        if(aFlag & O_CREAT) // aFlag�� O_CREAT �Ӽ��� �ִ� ���
        {
            if((sFileID = idf::getUnusedFileID()) != IDF_INVALID_FILE_ID)
            {
                // �� file ID�� �����ϸ� Meta�� �Ҵ��ϰ� ������ �����Ѵ�.
                (void)idf::alloc((void **)&sMeta, ID_SIZEOF(idfMeta));

                idlOS::memset(sMeta, 
                              '\0', 
                              ID_SIZEOF(idfMeta));

                idf::mMetaList[sFileID].mMeta = sMeta;
                idf::mMetaList[sFileID].mFileOpenCount= 1;

                idf::mFdList[sFd].mFileID = sFileID;
                idf::mFdList[sFd].mCursor = 0;

                idlOS::strncpy(sMeta->mFileName, 
                               sPath, 
                               IDF_MAX_FILE_NAME_LEN);

                sMeta->mFileID      = sFileID;
                sMeta->mNumOfPagesU = 0;
                sMeta->mNumOfPagesA = 0;
                sMeta->mSignature   = htonl(0xABCDABCD);

                for(sIndex = 0; sIndex < IDF_DIRECT_MAP_SIZE; sIndex++)
                {
                    sMeta->mDirectPages[sIndex] = 0;
                }

                for(sIndex = 0; sIndex < 16; sIndex++)
                {
                    sMeta->mIndPages[sIndex] = 0;
                }

                sMeta->mSize = 0;

                idf::mMaster.mNumOfFiles++;
            }
            else
            {
                // �� file ID�� ������ ���Ͽ� ���� Meta�� ������ �� ����
                // ������ �� �̻� ������ ������ �� ����.
                (void)idf::freeFd(sFd);

                sFd = IDF_INVALID_HANDLE;

                errno = ENFILE; // File table overflow
            }
        }
        else // ������ �������� �ʰ�, aFlag�� O_CREAT �Ӽ��� ���� ���
        {
            (void)idf::freeFd(sFd);

            sFd = IDF_INVALID_HANDLE;

            errno = ENOENT; // No such file or directory
        }
    }

    if((sFd != IDF_INVALID_HANDLE) && (sMeta != NULL))
    {
        if((aFlag & O_CREAT) || (aFlag & O_TRUNC))
        {
            idf::mIsSync = ID_FALSE;

            // ����� �α׸� üũ�Ѵ�.
            if(idf::mLogList[sFileID] == 0)
            {
                idf::mLogList[sFileID] = 1;

                idf::mLogListCount++;
            }

            IDE_TEST(idf::masterLog(0) != IDE_SUCCESS);
        }
    }

    if(sPath != NULL)
    {
        (void)idf::free(sPath);

        sPath = NULL;
    }

    sLock = ID_FALSE;
    (void)idf::unlock();

    return (PDL_HANDLE)sFd;

    IDE_EXCEPTION(fd_error);
    {
        errno = EMFILE; // Too many open files
    }
    
    IDE_EXCEPTION_END;

    if(sPath != NULL)
    {
        (void)idf::free(sPath);

        sPath = NULL;
    }

    if(sLock == ID_TRUE)
    {
        sLock = ID_FALSE;
        (void)idf::unlock();
    }

    return (PDL_HANDLE)IDF_INVALID_HANDLE;
}

SInt idfCore::close(PDL_HANDLE aFd)
{
    SInt   sFd = (SInt)aFd;
    SInt   sFileID;
    idBool sLock = ID_FALSE;

    (void)idf::lock();
    sLock = ID_TRUE;
    
    // Fd�� ��������� Ȯ���Ѵ�. ������� Fd�� �ƴϸ� ������ �߻��Ѵ�.
    IDE_TEST_RAISE((idf::mFdList[sFd].mIsUsed) == IDF_FD_UNUSED, fd_error);

    sFileID = idf::mFdList[sFd].mFileID;

    IDE_TEST_RAISE(sFileID == IDF_INVALID_FILE_ID, fd_error);

    // Meta List�� ���ؼ��� �ܼ��� ������ ���� Ƚ���� �����Ѵ�.
    // File Descriptor List�� mIsUsed�� IDF_FD_USED(1)�� �ƴϸ� �̹� ���Ͽ�
    // ���� Meta�� ���� ����̹Ƿ� ��Ÿ�� ���ؼ��� ó���� ���� �ʴ´�.
    // ���� Meta�� ���� �����͸� ����, ��ũ�� �����ϸ� �ٸ� ������ Meta��
    // �߸� ������ �� �����Ƿ� Meta�� �������� �ʵ��� �����Ѵ�.
    if(idf::mFdList[sFd].mIsUsed == IDF_FD_USED)
    {
        idf::mMetaList[sFileID].mFileOpenCount--;
    }

    // Fd�� ��ȯ�Ͽ� ������ ������ �� �� ����ϵ��� �Ѵ�.
    (void)idf::freeFd(sFd);

    sLock = ID_FALSE;
    (void)idf::unlock();

    // Retrun Value
    //      0 : Success
    //     -1 : Failure
    return 0;

    IDE_EXCEPTION(fd_error);
    {
        errno = EBADF; // Bad file number
    } 
    
    IDE_EXCEPTION_END;

    if(sLock == ID_TRUE)
    {
        sLock = ID_FALSE;
        (void)idf::unlock();
    }

    return -1;
}

#if defined(VC_WINCE)
PDL_HANDLE idfCore::creat(SChar *aPathName, mode_t /*aMode*/)
#else
PDL_HANDLE idfCore::creat(const SChar *aPathName, mode_t /*aMode*/)
#endif
{
    // ���� �������� �ʴ´�.
    // open()�� ȣ���ϸ� errno�� open()���� �����Ѵ�.
    // ���� �ܼ��� open()ȣ���� ����� ��ȯ�ϸ� �ȴ�.
    PDL_HANDLE sFd = idfCore::open(aPathName, O_CREAT | O_RDWR | O_TRUNC);

    return sFd;
}

ssize_t idfCore::read(PDL_HANDLE aFd, void *aBuf, size_t aCount)
{
    SInt     sFd        = (SInt)aFd;
    SInt     sReadCount = 0;
    SInt     sFileID;
    UInt     sPageIndex;
    UInt     sPageOffset;
    UInt     sReadSize;
    UInt     sPageRemain;
    UInt     sLogicalPage = 0;
    SChar   *sBuf    = (SChar*)aBuf;
    SLong    sCount  = aCount;
    ULong    sCursor = 0;
    idfMeta *sMeta;
    idBool   sLock = ID_FALSE;

    (void)idf::lock();
    sLock = ID_TRUE;

    // Fd�� ��������� Ȯ���Ѵ�. ������� Fd�� �ƴϸ� ������ �߻��Ѵ�.
    IDE_TEST_RAISE(idf::mFdList[sFd].mIsUsed != IDF_FD_USED, fd_error);

    sFileID = idf::mFdList[sFd].mFileID;

    IDE_TEST_RAISE(sFileID == IDF_INVALID_FILE_ID, fd_error);

    sMeta = idf::mMetaList[sFileID].mMeta;

    // sCursor�� ���� ���� �����Ͱ� ��ġ�� ������ �ּҸ� ã�´�.
    sCursor = idf::mFdList[sFd].mCursor;

    sPageIndex = sCursor / idf::mPageSize;

    IDE_TEST_RAISE(sCount < 0, invalid_arg_error);

    // ��Ÿ���� ���� ũ�⸦ ���ͼ� ���� ũ�⸦ �缳���Ѵ�.
    if((SLong)(sMeta->mSize - sCursor) < sCount)
    {
        sCount = sMeta->mSize - sCursor;
    }

    // ���������� �����͸� �о���� ���� ���̸� �����Ѵ�.
    if(sCount > 0)
    {
        while(sCount > 0)
        {
            sPageOffset = sCursor % idf::mPageSize;

            sPageRemain = (idf::mPageSize - sPageOffset);

            // �������� ���� ������ ���� ������ �� ���� ũ�⸸ŭ �д´�.
            //     sPageRemain : ������ ���� ���� ����
            //     sCount      : ���� ���� ���� ������
            sReadSize = (sPageRemain < sCount) ? sPageRemain : sCount;

            if((sLogicalPage = idf::getPageAddrR(sMeta, sFileID, sPageIndex))
               <= 0)
            {
                idlOS::memset(sBuf, 
                              '\0', 
                              sReadSize);
            }
            else
            {
                IDE_TEST(idf::lseekFs(idf::mFd,
                                      sLogicalPage * idf::mPageSize + sPageOffset,
                                      SEEK_SET) == -1);

                IDE_TEST(idf::readFs(idf::mFd, 
                                     sBuf, 
                                     sReadSize) != sReadSize);
            }

            sBuf       += sReadSize;
            sCursor    += sReadSize;
            sReadCount += sReadSize;
            sCount     -= sReadSize;

            // �������� �� ���� ��� ���� �������� �е��� �Ѵ�.
            if(((sCursor % idf::mPageSize) == 0) && (sReadSize > 0))
            {
                sPageIndex++;
            }
        }
    }

    idf::mFdList[sFd].mCursor = sCursor;

    sLock = ID_FALSE;
    (void)idf::unlock();

    // Return Value
    //     sReadCount ==  0 : End Of File
    //     sReadCount  >  0 : The number of bytes read
    //     sReadCount == -1 : Failure
    return (ssize_t)sReadCount;

    IDE_EXCEPTION(invalid_arg_error);
    {
        errno = EINVAL;
    }

    IDE_EXCEPTION(fd_error);
    {
        errno = EBADF; // Bad file number
    } 
    
    IDE_EXCEPTION_END;

    if(sLock == ID_TRUE)
    {
        sLock = ID_FALSE;
        (void)idf::unlock();
    }

    return -1;
}

ssize_t idfCore::write(PDL_HANDLE aFd, const void* aBuf, size_t aCount)
{
    SInt     sFd = (SInt)aFd;
    SInt     sFileID;
    SInt     sPageIndex;
    SInt     sPageOffset;
    SInt     sWriteSize;
    SInt     sPageRemain;
    SInt     sLogicalPage = 0;
    SLong    sCount       = aCount;
    ULong    sCursor      = 0;
    SChar   *sBuf         = (SChar*)aBuf;
    size_t   sWriteCount  = 0;
    idfMeta *sMeta        = NULL;
    idBool   sLock        = ID_FALSE;

    (void)idf::lock();
    sLock = ID_TRUE;

    // Fd�� ��������� Ȯ���Ѵ�. ������� Fd�� �ƴϸ� ������ �߻��Ѵ�.
    IDE_TEST_RAISE(idf::mFdList[sFd].mIsUsed != IDF_FD_USED, fd_error);

    sFileID = idf::mFdList[sFd].mFileID;

    IDE_TEST_RAISE(sFileID == IDF_INVALID_FILE_ID, fd_error);

    sMeta = idf::mMetaList[sFileID].mMeta;

    // sCursor�� ���� ������ �ε����� ���� �� 
    // ���� �����Ͱ� ��ġ�� ������ �ּҸ� ã�´�.
    sCursor = idf::mFdList[sFd].mCursor;

    sPageIndex = sCursor / idf::mPageSize;

    IDE_TEST_RAISE(sCount < 0, invalid_arg_error);

    if(sCount > 0)
    {
        idf::mIsSync = ID_FALSE;

        idf::mIsDirty = ID_FALSE;

        // ������ �ε����� �̿��Ͽ� ������ �ּҸ� ���´�.
        sLogicalPage = idf::getPageAddrW(sMeta, sFileID, sPageIndex);

        IDE_TEST_RAISE(sLogicalPage == -1, free_page_error);

        while(sCount > 0)
        {
            sPageOffset = sCursor % idf::mPageSize;

            sPageRemain = (idf::mPageSize - sPageOffset);

            // �������� ���� ������ ���� ������ ũ�� �� ���� ũ�⸸ŭ ����.
            sWriteSize = (sPageRemain < sCount) ? sPageRemain : sCount;

            IDE_TEST(idf::lseekFs(idf::mFd,
                                  sLogicalPage * idf::mPageSize + sPageOffset,
                                  SEEK_SET) == -1);

            IDE_TEST(idf::writeFs(idf::mFd, 
                                  sBuf, 
                                  sWriteSize) != sWriteSize);

            sBuf        += sWriteSize;
            sCursor     += sWriteSize;
            sWriteCount += sWriteSize;
            sCount      -= sWriteSize;

            // �������� �� ����� ���� �������� ������ �Ѵ�.
            if(((sCursor % idf::mPageSize) == 0) && (sWriteSize> 0) && (sCount > 0))
            {
                sPageIndex++;

                IDE_TEST_RAISE((sLogicalPage = idf::getPageAddrW(sMeta,
                                                                 sFileID,
                                                                 sPageIndex))
                               == -1, free_page_error);

            }
        }

        // Meta�� ���� ũ�� ������ �����Ѵ�.
        if(sMeta->mSize < sCursor)
        {
            sMeta->mSize = sCursor;

            idf::mIsDirty = ID_TRUE;
        }

        // ���Ͽ� �����͸� ����� ��쿡�� Meta�� ����Ѵ�.
        // Meta�� ������� �ʾ����� Meta�� �������� �ʴ´�.
        if(idf::mIsDirty == ID_TRUE)
        {
            if(idf::mLogList[sFileID] == 0)
            {
                idf::mLogList[sFileID] = 1;

                idf::mLogListCount++;
            }

            IDE_TEST(idf::masterLog(0) != IDE_SUCCESS);
        }

        // Ŀ���� ��ġ�� �̵��Ѵ�.
        idf::mFdList[sFd].mCursor = sCursor;
    }

    sLock = ID_FALSE;
    (void)idf::unlock();

    // Return Value
    //     sWriteCount ==  0 : Nothing was written
    //     sWriteCount  >  0 : The number of bytes written
    //     sWriteCount == -1 : Failure
    return (ssize_t)sWriteCount;

    IDE_EXCEPTION(free_page_error);
    {
    }

    IDE_EXCEPTION(invalid_arg_error);
    {
        errno = EINVAL;
    }

    IDE_EXCEPTION(fd_error);
    {
        errno = EBADF; // Bad file number
    } 
    
    IDE_EXCEPTION_END;

    if(sLock == ID_TRUE)
    {
        sLock = ID_FALSE;
        (void)idf::unlock();
    }

    return -1;
}

PDL_OFF_T idfCore::lseek(PDL_HANDLE aFd, PDL_OFF_T aOffset, SInt aWhence)
{
    SInt      sFd = (SInt)aFd;
    SInt      sFileID;
    PDL_OFF_T sCursor;
    idBool    sLock = ID_FALSE;

    (void)idf::lock();
    sLock = ID_TRUE;

    // Fd�� ��������� Ȯ���Ѵ�. ������� Fd�� �ƴϸ� ������ �߻��Ѵ�.
    IDE_TEST_RAISE(idf::mFdList[sFd].mIsUsed != IDF_FD_USED, fd_error);

    sFileID = idf::mFdList[sFd].mFileID;

    IDE_TEST_RAISE(sFileID == IDF_INVALID_FILE_ID, fd_error);

    // aWhence�� ���� ��ġ�� �����Ѵ�.
    // SEEK_SET, SEEK_CUR, SEEK_END �̿��� ���� ���� ������ �߻��Ѵ�.
    switch(aWhence)
    {
    case SEEK_SET:
        sCursor = 0;

        break;
    case SEEK_CUR:
        sCursor = idf::mFdList[sFd].mCursor;

        break;
    case SEEK_END:
        sCursor = idf::mMetaList[sFileID].mMeta->mSize;

        break;
    default:
        errno = EINVAL; // Invalid argument

        sCursor = -1;

        break;
    }

    if(sCursor >= 0)
    {
        sCursor += aOffset;

        if(sCursor < 0)
        {
            errno = EINVAL; // Invalid argument

            sCursor = -1;
        }
        else
        {
            idf::mFdList[sFd].mCursor = sCursor;
        }
    }

    sLock = ID_FALSE;
    (void)idf::unlock();

    // Return Value
    //     sCursor  >  0 : Success
    //     sCursor == -1 : Failure
    return sCursor;

    IDE_EXCEPTION(fd_error);
    {
        errno = EBADF; // Bad file number
    } 

    IDE_EXCEPTION_END;

    if(sLock == ID_TRUE)
    {
        sLock = ID_FALSE;
        (void)idf::unlock();
    }

    return -1; 
}

SInt idfCore::fstat(PDL_HANDLE aFd, PDL_stat *aBuf)
{
    SInt   sFd = (SInt)aFd;
    SInt   sFileID;
    idBool sLock = ID_FALSE;

    IDE_TEST_RAISE(aBuf == NULL, bad_address_error);

    (void)idf::lock();
    sLock = ID_TRUE;
    
    // Fd�� ��������� Ȯ���Ѵ�. ������� Fd�� �ƴϸ� ������ �߻��Ѵ�.
    IDE_TEST_RAISE(idf::mFdList[sFd].mIsUsed != IDF_FD_USED, fd_error);

    sFileID = idf::mFdList[sFd].mFileID;

    IDE_TEST_RAISE(sFileID == IDF_INVALID_FILE_ID, fd_error);

    aBuf->st_size = (idf::mMetaList[sFileID].mMeta)->mSize;

     sLock = ID_FALSE;
     (void)idf::unlock();

    // Return Value
    //      0 : Success
    //     -1 : Failure
    return 0;

    IDE_EXCEPTION(bad_address_error);
    {
        errno = EFAULT; // Bad address
    } 

    IDE_EXCEPTION(fd_error);
    {
        errno = EBADF; // Bad file number
    } 
    
    IDE_EXCEPTION_END;

    if(sLock == ID_TRUE)
    {
        sLock = ID_FALSE;
        (void)idf::unlock();
    }

    return -1;
}

SInt idfCore::unlink(const SChar* aPathName)
{
    SInt   sFileID;
    SInt   sRc   = -1;
    SChar *sPath = NULL;
    idBool sLock = ID_FALSE;

    (void)idf::lock();
    sLock = ID_TRUE;

    sPath = idf::getpath(aPathName);

    IDE_TEST(sPath == NULL);

    sFileID = idf::getFileIDByName(sPath);

    IDE_TEST_RAISE(sFileID == IDF_INVALID_FILE_ID, fd_error);

    if(idf::freeMeta(sFileID) == IDE_SUCCESS)
    {
        sRc = 0;
    }
    else
    {
        sRc = -1;
    }

    if(idf::mLogList[sFileID] == 0)
    {
        idf::mLogList[sFileID] = 1;

        idf::mLogListCount++;
    }

    IDE_TEST(idf::masterLog(1) != IDE_SUCCESS);

    IDE_TEST(idf::fsyncFs(idf::mFd) == -1);

    IDE_TEST(idf::initLog() != IDE_SUCCESS);

    idf::mIsSync = ID_TRUE;

    if(sPath != NULL)
    {
        (void)idf::free(sPath);

        sPath = NULL;
    }

    sLock = ID_FALSE;
    (void)idf::unlock();

    // Return Value
    //     sRc ==  0 : Success
    //     sRc == -1 : Failure
    return sRc;

    IDE_EXCEPTION(fd_error);
    {
        errno = ENOENT; // No such file or directory
    }
    
    IDE_EXCEPTION_END;

    if(sPath != NULL)
    {
        (void)idf::free(sPath);

        sPath = NULL;
    }

    if(sLock == ID_TRUE)
    {
        sLock = ID_FALSE;
        (void)idf::unlock();
    }

    return -1; 
}

SInt idfCore::rename(const SChar *aOldpath, const SChar *aNewpath)
{
    SInt     sFileID;
    SInt     sNewFileID;
    SChar   *sOldPath = NULL;
    SChar   *sNewPath = NULL;
    idfMeta *sMeta    = NULL;
    idBool   sLock    = ID_FALSE;

    (void)idf::lock();
    sLock = ID_TRUE;

    sOldPath = idf::getpath(aOldpath);
    sNewPath = idf::getpath(aNewpath);

    IDE_TEST(sOldPath == NULL);
    IDE_TEST(sNewPath == NULL);

    sFileID    = idf::getFileIDByName(sOldPath);
    sNewFileID = idf::getFileIDByName(sNewPath);

    // aOldPath�� �ش��ϴ� ������ ���ٸ� errno�� ENOENT�� �����а�
    // -1�� ��ȯ�Ѵ�.
    IDE_TEST_RAISE(sFileID == IDF_INVALID_FILE_ID, fd_error);

    idf::mIsSync = ID_FALSE;

    if(sNewFileID != IDF_INVALID_FILE_ID)
    {
        // aNewPath�� �ش��ϴ� ������ �����ϸ� ������ �����Ѵ�.
        IDE_TEST(idf::freeMeta(sNewFileID) != IDE_SUCCESS);

        if(idf::mLogList[sNewFileID] == 0)
        {
            idf::mLogList[sNewFileID] = 1;

            idf::mLogListCount++;
        }
    }

    sMeta = idf::mMetaList[sFileID].mMeta;

    idlOS::strncpy(sMeta->mFileName, 
                   sNewPath, 
                   IDF_MAX_FILE_NAME_LEN);

    if(idf::mLogList[sFileID] == 0)
    {
        idf::mLogList[sFileID] = 1;

        idf::mLogListCount++;
    }

    if(sNewFileID != IDF_INVALID_FILE_ID)
    {
        IDE_TEST(idf::masterLog(1) != IDE_SUCCESS);
    }
    else
    {
        IDE_TEST(idf::masterLog(0) != IDE_SUCCESS);
    }

    if(sOldPath != NULL)
    {
        (void)idf::free(sOldPath);

        sOldPath = NULL;
    }

    if(sNewPath != NULL)
    {
        (void)idf::free(sNewPath);

        sNewPath = NULL;
    }

     sLock = ID_FALSE;
     (void)idf::unlock();

    // Return Value
    //       0 : Success
    //      -1 : Failure
    return 0;

    IDE_EXCEPTION(fd_error);
    {
        errno = ENOENT; // No such file or directory
    }
    
    IDE_EXCEPTION_END;

    if(sOldPath != NULL)
    {
        (void)idf::free(sOldPath);

        sOldPath = NULL;
    }

    if(sNewPath != NULL)
    {
        (void)idf::free(sNewPath);

        sNewPath = NULL;
    }

    if(sLock == ID_TRUE)
    {
        sLock = ID_FALSE;
        (void)idf::unlock();
    }

    return -1; 
}

SInt idfCore::access(const SChar* aPathName, SInt /*aMode*/)
{
    SInt   sFileID;
    SChar *sPathName = NULL;
    idBool sLock     = ID_FALSE;

    // access �Լ����� aMode�� ������� �ʴ´�.

    (void)idf::lock();
    sLock = ID_TRUE;

    // aPathName�� NULL�̸� errno�� EINVAL(Invalid argument)�� �����ϰ�
    // NULL�� ��ȯ�Ѵ�.
    IDE_TEST_RAISE(aPathName == NULL, invalid_arg_error);

    sPathName = idf::getpath(aPathName);

    IDE_TEST(sPathName == NULL);

    IDE_TEST_RAISE(idlOS::strcmp(sPathName, "") == 0, not_exist_error);

    // sFileID�� ���� IDF_INVALID_FILE_ID�� �ƴϸ� 0�� ��ȯ�Ѵ�.
    // ���� IDF_INVALID_FILE_ID�̸� �ش� sPath�� ���丮���� Ȯ���Ѵ�.
    // sPath�� �ش��ϴ� ���� �Ǵ� ���丮�� �������� ������ errno�� ENOENT
    // �� �����ϰ� -1�� ��ȯ�Ѵ�.
    if((sFileID = idf::getFileIDByName(sPathName)) == IDF_INVALID_FILE_ID)
    {
        // idf::isDir()�� IDE_SUCCESS�� ��ȯ�ϸ� ���丮�̴�.
        IDE_TEST_RAISE(idf::isDir(sPathName) != IDE_SUCCESS, not_exist_error);
    }

    if(sPathName != NULL)
    {
        (void)idf::free(sPathName);

        sPathName = NULL;
    }

    sLock = ID_FALSE;
    (void)idf::unlock();

    // Return Value
    //      0 : Success
    //     -1 : Failure
    return 0;

    IDE_EXCEPTION(invalid_arg_error);
    {
        errno = EINVAL; // Invalid argument
    }

    IDE_EXCEPTION(not_exist_error);
    {
        errno = ENOENT; // No such file or directory
    }
    
    IDE_EXCEPTION_END;

    if(sPathName != NULL)
    {
        (void)idf::free(sPathName);

        sPathName = NULL;
    }

    if(sLock == ID_TRUE)
    {
        sLock = ID_FALSE;
        (void)idf::unlock();
    }

    return -1; 
}

SInt idfCore::fsync(PDL_HANDLE aFd)
{
    SInt   sFileID;
    SInt   sFd   = (SInt)aFd;
    idBool sLock = ID_FALSE;

    (void)idf::lock();
    sLock = ID_TRUE;
    
    // Fd�� ��������� Ȯ���Ѵ�. ������� Fd�� �ƴϸ� ������ �߻��Ѵ�.
    IDE_TEST_RAISE(idf::mFdList[sFd].mIsUsed != IDF_FD_USED, fd_error);

    sFileID = idf::mFdList[sFd].mFileID;

    IDE_TEST_RAISE(sFileID == IDF_INVALID_FILE_ID, fd_error);

    if(idf::mIsSync == ID_FALSE)
    {
        if(idf::mTransCount > 0)
        {
            IDE_TEST(idf::masterLog(1) != IDE_SUCCESS);
        }

        IDE_TEST(idf::fsyncFs(idf::mFd) == -1);

        IDE_TEST(idf::initLog() != IDE_SUCCESS);

        idf::mIsSync = ID_TRUE;
    }

    sLock = ID_FALSE;
    (void)idf::unlock();

    // Return Value
    //      0 : Success
    //     -1 : Failure
    return 0;

    IDE_EXCEPTION(fd_error);
    {
        errno = EBADF; // Bad file number
    } 
    
    IDE_EXCEPTION_END;

    if(sLock == ID_TRUE)
    {
        sLock = ID_FALSE;
        (void)idf::unlock();
    }

    return -1;
}

SInt idfCore::ftruncate(PDL_HANDLE aFd, PDL_OFF_T aLength)
{
    SInt     sFileID;
    SInt     sFd = (SInt)aFd;
    idfMeta *sMeta;
    idBool   sLock = ID_FALSE;

    (void)idf::lock();
    sLock = ID_TRUE;
    
    // Fd�� ��������� Ȯ���Ѵ�. ������� Fd�� �ƴϸ� ������ �߻��Ѵ�.
    IDE_TEST_RAISE(idf::mFdList[sFd].mIsUsed != IDF_FD_USED, fd_error);

    // aLength�� �ݵ�� 0���� ũ�ų� ���� ���̾�� �Ѵ�.
    IDE_TEST_RAISE(aLength < 0, invalid_arg_error);

    sFileID = idf::mFdList[sFd].mFileID;

    IDE_TEST_RAISE(sFileID == IDF_INVALID_FILE_ID, fd_error);

    // aLength�� ������ ũ�⺸�� ū ��� ������ ũ�Ⱑ �þ��,
    // aLength�� ������ ũ�⺸�� ���� ��� ������ ũ�Ⱑ �پ���.
    // ������ �þ �κп� ���� read()�� �ϸ� ���ۿ� '\0'�� ä������.

    // aLength�� 0�̸� ������ ��ȭ�� �����Ƿ� �ƹ� �۾��� ���� �ʴ´�.
    if(aLength > 0)
    {
        idf::mIsSync = ID_FALSE;

        sMeta = idf::mMetaList[sFileID].mMeta;

        sMeta->mSize = aLength;

        if(idf::mLogList[sFileID] == 0)
        {
            idf::mLogList[sFileID] = 1;

            idf::mLogListCount++;
        }

        IDE_TEST(idf::masterLog(0) != IDE_SUCCESS);
    }

    sLock = ID_FALSE;
    (void)idf::unlock();

    // Return Value
    //      0 : Success
    //     -1 : Failure
    return 0;

    IDE_EXCEPTION(invalid_arg_error);
    {
        errno = EINVAL; // Invalid argument
    }

    IDE_EXCEPTION(fd_error);
    {
        errno = EBADF; // Bad file number
    } 
    
    IDE_EXCEPTION_END;

    if(sLock == ID_TRUE)
    {
        sLock = ID_FALSE;
        (void)idf::unlock();
    }

    return -1;
}

DIR* idfCore::opendir(const SChar* aName)
{
    SChar  *sName = NULL;
    idfDir *sDir;
    idBool  sLock = ID_FALSE;

    // aName�� NULL�̸� errno�� EINVAL(Invalid argument)�� �����ϰ�
    // NULL�� ��ȯ�Ѵ�.
    IDE_TEST_RAISE(aName == NULL, invalid_arg_error);

    (void)idf::lock();
    sLock = ID_TRUE;

    sName = idf::getpath(aName);

    IDE_TEST(sName == NULL);

    IDE_TEST_RAISE(idlOS::strcmp(sName, "") == 0, not_exist_error);

    IDE_TEST_RAISE(idf::isDir(sName) == IDE_FAILURE, not_exist_error);

    sDir = (idfDir*)idf::getDir(sName);

    if(sName != NULL)
    {
        (void)idf::free(sName);

        sName = NULL;
    }

    sLock = ID_FALSE;
    (void)idf::unlock();

    // Return Value
    //     sDir != NULL : Success
    //     sDir == NULL : Failure
    return (DIR*)sDir;

    IDE_EXCEPTION(invalid_arg_error);
    {
        errno = EINVAL; // Invalid argument
    }

    IDE_EXCEPTION(not_exist_error);
    {
        errno = ENOENT; // No such file or directory
    }
    
    IDE_EXCEPTION_END;

    if(sName != NULL)
    {
        (void)idf::free(sName);

        sName = NULL;
    }

    if(sLock == ID_TRUE)
    {
        sLock = ID_FALSE;
        (void)idf::unlock();
    }

    return NULL;
}

SInt idfCore::readdir_r(DIR            *aDirp,
                        struct dirent  *aEntry,
                        struct dirent **aResult)
{
    SInt       sRc = -1;
    SInt       sIndex;
    SInt       sDirIndex;
    SInt       sDirCount;
    idfDir    *sDir = (idfDir*)aDirp;
    dirent    *sDirent;
    idfDirent *sDirentP;

    sDirIndex = sDir->mDirIndex;
    sDirCount = sDir->mDirCount;

    if(sDirIndex == sDirCount)
    {
        // DIR ��ü���� dirent��ü�� ��� �о���.
        *aResult = NULL;
    }
    else
    {
        sDirentP = (idfDirent*)sDir->mFirst;

        for(sIndex = 0; sIndex < sDirIndex; sIndex++)
        {
            sDirentP = sDirentP->mNext;
        }

        sDirent = (dirent*)sDirentP->mDirent;

        idlOS::strncpy(aEntry->d_name,
                       sDirent->d_name,
                       idlOS::strlen(sDirent->d_name));

        sDir->mDirIndex++;

        *aResult = sDirent;

        sRc = 0;
    }

    // Return Value
    //     sRc ==  0 : Success
    //     sRc == -1 : Failure
    return sRc;
}

struct dirent * idfCore::readdir(DIR *aDirp)
{
    SInt       sIndex;
    SInt       sDirIndex;
    SInt       sDirCount;
    idfDir    *sDir = (idfDir*)aDirp;
    dirent    *sDirent = NULL;
    idfDirent *sDirentP;

    sDirIndex = sDir->mDirIndex;
    sDirCount = sDir->mDirCount;

    if(sDirIndex == sDirCount)
    {
        // DIR ��ü���� dirent��ü�� ��� �о���.
        sDirent = NULL;
    }
    else
    {
        sDirentP = (idfDirent*)sDir->mFirst;

        for(sIndex = 0; sIndex < sDirIndex; sIndex++)
        {
            sDirentP = sDirentP->mNext;
        }

        sDirent = (dirent*)sDirentP->mDirent;

        sDir->mDirIndex++;
    }

    // Return Value
    //     sDirent != NULL : Success
    //     sDirent == NULL : Failure
    return sDirent;
}

void idfCore::closedir(DIR *aDirp)
{
    idfDir    *sDir = (idfDir*)aDirp;
    idfDirent *sDirentP;
    idfDirent *sTemp;

    sDirentP = sDir->mFirst;

    while(sDirentP != NULL)
    {
        sTemp = sDirentP->mNext;

        (void)idf::free(sDirentP->mDirent);
        (void)idf::free(sDirentP);

        sDirentP = sTemp;
    }

    // 1. Dirent ��ü�� ��ȯ�Ѵ�.
    // 2. Dir ��ü�� ��ȯ�Ѵ�.

    (void)idf::free(sDir);
}

SLong idfCore::getDiskFreeSpace(const SChar* /*aPathName*/)
{
    SLong  sRc = -1;
    SLong  sTotal;
    SLong  sUsed;

    // ��Ƽ���� �Ѱ��̹Ƿ� aPathName�� ������� �ʴ´�.
    // aPathName�� ������� ���� ��ũ �뷮�� ��ȯ�Ѵ�.
    (void)idf::lock();

    // ��ü ũ��
    sTotal = ((SLong)idf::mPageSize * idf::mPageNum);

    // ����� ũ��
    sUsed = ((SLong)idf::mPageSize * idf::mMaster.mAllocedPages);

    // ��� ������ ũ��
    sRc = sTotal - sUsed;

    (void)idf::unlock();

    // Return Value
    //    sRc ==  0 : Disk Has No Free Space
    //    sRc  >  0 : Disk's Free Space
    //    sRc == -1 : Failure
    return sRc;
}

PDL_OFF_T idfCore::filesize(PDL_HANDLE handle)
{
    PDL_stat sb;

    return (idf::fstat(handle, &sb) == -1) ? -1 : (PDL_OFF_T) sb.st_size;
}
