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

#include <idl.h>
#include <ideErrorMgr.h>
#include <idErrorCode.h>
#include <idp.h>
#include <smrDef.h>
#include <checkServerStat.h>
#include <mmErrorCode.h>
#include <mmuProperty.h>


/**
 * ���� unlink()�� ���� �ʴ� flock_destroy()
 *
 * idlOS::flock_destroy()�� unlink()�� �־ ������ ���������� ������ �ִ�.
 * ���� ���� ������ ������ �����ϸ�,
 * altibase.properties ���Ϸ� ���� �����Ƿ�
 * ���϶��� ������ unlink()�� ���� ���ƾ� �Ѵ�.
 * 
 * �׷��ٰ� flock_destroy()�� ���ϸ�
 * flock_init()���� strdup()�� ���� malloc() �ߴ��� ������ mem-leak�� �����.
 * 
 * pd�� ��ġ�� �ٸ� �ڵ忡 ������ �� �� �����Ƿ� �����ؿͼ� unlink()�� �� ��.
 */
PDL_INLINE int
safe_flock_destroy (PDL_OS::pdl_flock_t *lock)
{
  PDL_TRACE ("PDL_OS::flock_destroy");
  if (lock->handle_ != PDL_INVALID_HANDLE)
    {
      PDL_OS::flock_unlock (lock);
      // Close the handle.
      PDL_OS::close (lock->handle_);
      lock->handle_ = PDL_INVALID_HANDLE;
#if defined (CHORUS)
      // Are we the owner?
      if (lock->process_lock_ && lock->lockname_ != 0)
        {
          // Only destroy the lock if we're the owner
          PDL_OS::mutex_destroy (lock->process_lock_);
          PDL_OS::munmap (lock->process_lock_,
                          sizeof (PDL_mutex_t));
          PDL_OS::shm_unlink (lock->lockname_);
          PDL_OS::free (PDL_static_cast (void *,
                                         PDL_const_cast (LPTSTR,
                                                         lock->lockname_)));
        }
      else if (lock->process_lock_)
        // Just unmap the memory.
        PDL_OS::munmap (lock->process_lock_,
                     sizeof (PDL_mutex_t));
#else
      if (lock->lockname_ != 0)
        {
          //PDL_OS::unlink (lock->lockname_); /* don't delete file */
          PDL_OS::free (PDL_static_cast (void *,
                                         PDL_const_cast (LPTSTR,
                                                         lock->lockname_)));
        }
#endif /* CHORUS */
      lock->lockname_ = 0;
    }
  return 0;
}

// Used without initialize()
idBool CheckServerStat::isFileExist()
{
    if ( idlOS::access( mFileName, F_OK ) == 0 )
    {
        return ID_TRUE;
    }
    else
    {
        return ID_FALSE;
    }
}

// Used without initialize()
//fix BUG-18025
IDE_RC CheckServerStat::createLockFile(idBool* )
{
   return IDE_SUCCESS;
}

IDE_RC CheckServerStat::initialize()
{
    SChar *sHomeDir;
 
    
    sHomeDir = idp::getHomeDir();

    idlOS::memset(mFileName, 0, ID_SIZEOF(mFileName));
    
    /* BUG-45135 */
    idlOS::snprintf(mFileName, ID_SIZEOF(mFileName), "%s%c%s",
                   sHomeDir,
                   IDL_FILE_SEPARATOR,
                   IDP_LOCKFILE);

    return IDE_SUCCESS;
}

/* BUG 18294 */
IDE_RC CheckServerStat::initFileLock()
{
    IDE_TEST_RAISE ( idlOS::flock_init( &mLockFile,
                            O_RDWR,
                            mFileName,
                            0644) != 0, init_error);

    return IDE_SUCCESS;
    IDE_EXCEPTION(init_error);
    {
        IDE_SET(ideSetErrorCode(mmERR_FATAL_FLOCK_INIT, mFileName));
    }
    IDE_EXCEPTION_END;
    return IDE_FAILURE;
}

IDE_RC CheckServerStat::destFileLock()
{
    IDE_TEST_RAISE(safe_flock_destroy(&mLockFile) != 0, destroy_error);

    return IDE_SUCCESS;

    IDE_EXCEPTION(destroy_error);
    {
        IDE_SET(ideSetErrorCode(mmERR_FATAL_FLOCK_DESTROY, mFileName));
    }
    IDE_EXCEPTION_END;
    return IDE_FAILURE;
}

IDE_RC CheckServerStat::destroy()
{
    return IDE_SUCCESS;
}

IDE_RC CheckServerStat::hold()
{
    IDE_TEST_RAISE(idlOS::flock_wrlock(&mLockFile) != 0, hold_error);
    return IDE_SUCCESS;

    IDE_EXCEPTION(hold_error);
    {
        IDE_SET(ideSetErrorCode(mmERR_FATAL_FLOCK_WRLOCK));
    }
    IDE_EXCEPTION_END;
    return IDE_FAILURE;

}

IDE_RC CheckServerStat::tryhold()
{
    IDE_TEST_RAISE(idlOS::flock_trywrlock(&mLockFile) != 0, hold_error);

    return IDE_SUCCESS;

    IDE_EXCEPTION(hold_error);
    {
        IDE_SET(ideSetErrorCode(mmERR_FATAL_FLOCK_TRYWRLOCK));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC CheckServerStat::release()
{
    IDE_TEST_RAISE(idlOS::flock_unlock(&mLockFile) != 0, release_error);

    return IDE_SUCCESS;

    IDE_EXCEPTION(release_error);
    {
        IDE_SET(ideSetErrorCode(mmERR_FATAL_FLOCK_UNLOCK));
    }
    IDE_EXCEPTION_END;
    return IDE_FAILURE;
}

IDE_RC CheckServerStat::checkServerRunning(idBool *aRunningFlag)
{
    struct sockaddr_in  servaddr;
    SInt                sState = 0;
    PDL_SOCKET          sFd = PDL_INVALID_SOCKET;
    UInt                sPortNo = 0;

    sPortNo = mmuProperty::getPortNo();

    /* ---------------------------
     *  [1] Server Running �˻� : FileLock
     * --------------------------*/

    if (idlOS::flock_trywrlock(&mLockFile) == 0)
    {
        /* --------------------------------------------------------
         *  [2] Server Running �˻� : Port Bind
         *
         *      [1] �������� Lock�� ��������
         *          LockFile�� �����߱� ������ ����� Ʋ��
         *          ���� �� �ִ�. ����, �ش� ��Ʈ�� Bind�ؼ�
         *          ������ �������� �ƴ��� �ٽ� ��˻� �Ѵ�.
         * ------------------------------------------------------*/

        idlOS::memset(&servaddr, 0, sizeof(servaddr));

        sFd = idlOS::socket(AF_INET, SOCK_STREAM, 0);
        IDE_TEST_RAISE(sFd < 0, socket_error);
        sState = 1;

        servaddr.sin_family      = AF_INET;
        servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
        servaddr.sin_port        = htons(sPortNo);

        IDE_TEST_RAISE(idlVA::setSockReuseraddr(sFd) != 0, reuse_error);
        if ( idlOS::bind(sFd,
                         (struct sockaddr *)(&servaddr),
                         sizeof(servaddr)) < 0)
        {
            *aRunningFlag = ID_TRUE;
        }
        else
        {
            *aRunningFlag = ID_FALSE;
        }
        sState = 0;
        (void)idlOS::closesocket(sFd);
    }
    else
    {
        IDE_TEST_RAISE(errno != EBUSY, flock_error);
        *aRunningFlag = ID_TRUE;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION(reuse_error);
    {
        IDE_SET( ideSetErrorCode( idERR_FATAL_idc_SVC_INET_BIND_ERROR,
                                  sPortNo ));
    }
    IDE_EXCEPTION(flock_error);
    {
        IDE_SET(ideSetErrorCode(mmERR_FATAL_FLOCK_TRYWRLOCK));
    }
    IDE_EXCEPTION(socket_error);
    {
        IDE_SET(ideSetErrorCode(idERR_FATAL_idc_INET_SOCKET_CREATE_FAILED));
    }
    IDE_EXCEPTION_END;
    if (sState == 1)
    {
        (void)idlOS::closesocket(sFd);
    }
    return IDE_FAILURE;
}

