/***********************************************************************
 * Copyright 1999-2000, RTBase Corporation or its subsidiaries.
 * All rights reserved.
 **********************************************************************/

/***********************************************************************
 * $Id: iduFile.cpp 90315 2021-03-25 02:04:41Z jiwon.kim $
 **********************************************************************/

#include <idl.h>
#include <ideErrorMgr.h>
#include <ideLog.h>
#include <idErrorCode.h>
#include <iduFile.h>
#include <iduMemMgr.h>
#include <iduFitManager.h>

/* BUG-17818: DIRECT I/O�� ���ؼ� �����Ѵ�.
 *
 * # OS Level���� File������ DirectIO�� �ϴ� ���
 * # OS Name           How can use?
 *   Solaris           directio per file.
 *   Windows NT/2000   CreateFile�� flag�� FILE_FLAG_NO_BUFFERING�� �ѱ��.
 *   Tru64 Unix        AdvFS-Only File Flag, open�� flag��  O_DIRECTIO�� ���
 *   AIX               none
 *   Linux             available (2.4 kernels), when open, use O_DIRECT
 *   HP-UX             only when OnlineJFS, use VX_SETCACHE by ioctl
 *   QNX               none
 *
 * # File System���� Mount Option�� ���� DirectIO���
 * # File System         Mount option
 *   Solaris UFS         forcedirectio
 *   HP-UX HFS           none
 *   HP-UX BaseJFS       none
 *   HP-UX OnlineJFS     convosync=direct
 *   Veritas VxFS        convosync=direct
 *   (including HP-UX, Solaris and AIX)
 *   AIX JFS             mount -o dio
 *   Tru64 Unix AdvFS    none
 *   Tru64 Unix UFS      none
 *   QNX                 none
 *
 * # Altibase���� DirectIO ����� (�Ʒ� Flag�� �⺻ )
 * : DirectIO�� ����ϱ� ���ؼ��� 
 *    - LOG_IO_TYPE : LogFile�� ���ؼ� DirectIO ��� ����
 *      0 : Normal
 *      1 : DirectIO
 *    - DATABASE_IO_TYPE : Database File�� ���ؼ� Direct IO ��� ����
 *      0 : Normal
 *      1 : DirectIO
 *
 *    - DIRECT_IO_ENABLED : DirectIO�� �ֻ��� Property. �� Flag�� DirectIO��
 *      �ƴϸ� ������ Flag�� ���õǰ� Normal�ϰ� ó���ȴ�.
 *      0 : Normal
 *      1 : DirectIO
 *
 *   ex) Log File�� ���ؼ��� DirectIO�� �ϰ� �ʹٸ�
 *       DIRECT_IO_ENABLED = 1
 *       LOG_IO_TYPE = 1
 *       DATABASE_IO_TYPE = 0
 *
 * �� Property�����̿ܿ� DirectIO�� ����ϱ� ���ؼ� ������ �ؾ��ϴ� ��.
 * # OS          # File System        # something that you have to do.
 * Solaris         UFS                 none
 * HP-UX           Veritas VxFS        use convosync=direct when mount.
 * Solaris         Veritas VxFS        use convosync=direct when mount.
 * AIX             Veritas VxFS        use convosync=direct when mount.
 * AIX             JFS                 use -o dio when mount.
 * Windows NT/2000 *                   none
 * Tru64 Unix      AdvFS               none
 * Linux(2.4 > k)  *                   none
 *
 * ������ ���õ� OS�ܿ��� DirectIO�� �������� �ʽ��ϴ�. �������� �ʴ� Platform����
 * Direct IO�� ����� ��� �⺻������ Buffered IO�� Synchronous I/O�� ����.
 *
 * # DirectIO�� ���� OS�� ���� ����
 * - Solaris :  1. the application's buffer have to be  aligned  on  a  two-byte  (short)
 *                 boundary
 *              2. the offset into the file have to be  on a device sector boundary.
 *              3. and the size of  the operation have to be a multiple of device sectors
 *
 * - HP-UX   :  finfo�� ffinfo�� ���ؼ� �������� ������
 *              1. dio_offset : the offset into the file have to be aligned on dio_offset
 *              2. dio_max : max dio request size
 *              3. dio_min : min dio request size
 *              4. dio_align: the application's buffer have to be  aligned  on dio_align
 *
 * - WINDOWS : 1. File access must begin at byte offsets within a file that are integer
 *                multiples of the volume sector size.
 *             2. File access must be for numbers of bytes that are integer multiples
 *                of the volume sector size.
 *             3. Buffer addresses for read and write operations should be sector aligned,
 *                which means aligned on addresses in memory that are integer multiples of
 *                the volume sector size
 *
 * - AIX: 1. The alignment of the read requests must be on 4K boundaries
 *        2. In AIX 5.2 ML01 this alignment must be according to "agblksize",
 *           a parameter which is specified during file system creation
 *
 * - LINUX: 1. transfer sizes, and the alignment of user  buffer  and
 *             file  offset  must all be multiples of the logical block size of
 *             the file system. Under Linux 2.6 alignment  to  512-byte  bound-
 *             aries suffices.
 * # Caution
 * - DirectIO�� �⺻������ Kernel Buffer�� ��ġ�� �ʰ� ����Ÿ�� �ٷ� Disk�� �����⶧����
 *   ������ sync�� ���ʿ������� File Meta���濡 ���ؼ��� DirectIO�� ������� �ʰ�
 *   �Ϲ������� Buffered IO�� �����̵ȴ�.
 *   Data�� Disk�� Sync�� ���� �����ϰ� file�� Meta���� sync�Ǵ� ���� �����ϱ����ؼ�
 *   fsync�� ����ؾ� �Ѵ�. OS���� �ٸ��⶧���� ��Ƽ���̽������� �������� ���Ǽ��� ���ؼ�
 *   ������ fsync�� �Ѵ�. ������ �̰��� O_DSYNC�� ���� Flag�� �̿��ؼ� �����Ҽ� ����
 *   ������ �����մϴ�. ���Ŀ� fsync�� ������ �ȴٸ� ������ �ʿ䰡 ������ �����ϴ�.
 *
 */

// To fix BUG-4378
//#define IDU_FILE_COPY_BUFFER (256 * 1024 * 1024)
// #define IDU_FILE_COPY_BUFFER (256 * 1024)
// A3�� copy�� ���߱� ���Ͽ� ������ ���� ��.
#define IDU_FILE_COPY_BUFFER (8 * 1024 * 1024)

/*
 * XDB DA applications require write access to trace log and they may
 * run from a different account than the server process.
 */
#if defined( ALTIBASE_PRODUCT_HDB )
# define IDU_FILE_PERM S_IRUSR | S_IWUSR | S_IRGRP
#else
# define IDU_FILE_PERM 0666 /* rwrwrw */
#endif

iduFIOStatFunc iduFile::mStatFunc[ IDU_FIO_STAT_ONOFF_MAX ] =
{
    // IDU_FIO_STAT_OFF
    {
        beginFIOStatNA,
        endFIOStatNA
    },
    // IDU_FIO_STAT_ON
    {
        beginFIOStat,
        endFIOStat
    }
};

/***********************************************************************
 * Description : �ʱ�ȭ �۾��� �����Ѵ�.
 *
 * aIndex               - [IN] iduFile�� ��� Mdule���� ����ϴ°�?
 * aMaxMaxFDCnt         - [IN] open�� �� �ִ� �ִ� FD����
 * aIOStatOnOff         - [IN] File IO�� ��������� �������� ����
 * aIOMutexWaitEventID  - [IN] IO Event�� �����
 ***********************************************************************/
IDE_RC iduFile::initialize( iduMemoryClientIndex aIndex,
                            UInt                 aMaxMaxFDCnt,
                            iduFIOStatOnOff      aIOStatOnOff,
                            idvWaitIndex         aIOMutexWaitEventID )
{
    mFDStackInfo = (iduFXStackInfo*)mFDStackBuff;
    mIndex       = aIndex;

    idlOS::memset( mFilename, 0, ID_MAX_FILE_NAME );

    mIsDirectIO = ID_FALSE;
    mPermission = O_RDWR;

    // ���� IO ������� �ʱ�ȭ
    if( aIOStatOnOff == IDU_FIO_STAT_ON )
    {
        idlOS::memset( &mStat, 0x00, ID_SIZEOF( iduFIOStat ) );
        mStat.mMinIOTime = ID_ULONG_MAX;
    }

    IDE_TEST( mIOOpenMutex.initialize(
                  (SChar*)"IDU_FILE_IOOPEN_MUTEX",
                  IDU_MUTEX_KIND_POSIX,
                  aIOMutexWaitEventID )
              != IDE_SUCCESS );

    mIOStatOnOff = aIOStatOnOff;
    mWaitEventID = aIOMutexWaitEventID;

    /* Stackũ�Ⱑ �����Ǿ� �����Ƿ� �� ũ�⺸�� �۾ƾ� �Ѵ�.
     * ���� ũ��� �������� malloc�� ���� �ʱ����ؼ� �̴�. */
    IDE_ASSERT( aMaxMaxFDCnt <= ID_MAX_FILE_DESCRIPTOR_COUNT );

    mMaxFDCount = aMaxMaxFDCnt;
    mCurFDCount = 0;

    IDE_TEST( iduFXStack::initialize( (SChar*)"IDU_FILE_FD_STACK_MUTEX",
                                      mFDStackInfo,
                                      ID_MAX_FILE_DESCRIPTOR_COUNT,
                                      ID_SIZEOF( PDL_HANDLE ) )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : ������ ���� �����۾��� �����Ѵ�.
 *
 *  1. ��� Open FD�� Close�Ѵ�.
 *  2. mIOOpenMutex�� �����Ѵ�.
 *  3. mFDStackInfo�� �����Ѵ�.
 ***********************************************************************/
IDE_RC iduFile::destroy()
{
    /* ���� Open�Ǿ� �ִ� ��� FD�� Close�Ѵ�. */
    IDE_TEST( closeAll() != IDE_SUCCESS );

    IDE_TEST( mIOOpenMutex.destroy() != IDE_SUCCESS );

    IDE_TEST( iduFXStack::destroy( mFDStackInfo ) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : mFilename�� �ش��ϴ� File�� �����Ѵ�.
 ***********************************************************************/
IDE_RC iduFile::create()
{
    PDL_HANDLE sCreateFd;
    SInt       sSystemErrno = 0;

    /* BUG-24519 logfile ������ group-read ������ �ʿ��մϴ�. */
    sCreateFd = idf::creat( mFilename, IDU_FILE_PERM );

    IDE_TEST_RAISE( sCreateFd == IDL_INVALID_HANDLE , CREATEFAILED);
    IDE_TEST_RAISE( idf::close( sCreateFd ) != 0,
                    close_error);

    return IDE_SUCCESS;

    IDE_EXCEPTION(CREATEFAILED)
    {
        sSystemErrno = errno;

        switch(sSystemErrno)
        {
        case EEXIST:
            IDE_SET(ideSetErrorCode(idERR_ABORT_FILE_ALREADY_EXIST, mFilename));
            break;
        case ENOENT:
            IDE_SET(ideSetErrorCode(idERR_ABORT_NO_SUCH_FILE, mFilename));
            break;
        case EACCES:
            IDE_SET(ideSetErrorCode(idERR_ABORT_INVALID_ACCESS, mFilename));
            break;
        case ENOMEM:
        case ENOSPC:
            IDE_SET(ideSetErrorCode(
                        idERR_ABORT_DISK_SPACE_EXHAUSTED,
                        mFilename, 0, 0));
            break;
#if defined(EDQUOT)
        case EDQUOT:
            IDE_SET(ideSetErrorCode(
                        idERR_ABORT_DISK_SPACE_EXHAUSTED,
                        mFilename, 0, 0));
            break;
#endif
        case EMFILE:
        case ENFILE:
            IDE_SET(ideSetErrorCode(
                        idERR_ABORT_EXCEED_OPEN_FILE_LIMIT,
                        mFilename, 0, 0));
            break;
#if defined(ENAMETOOLONG)
        case ENAMETOOLONG:
            IDE_SET(ideSetErrorCode(idERR_ABORT_FILENAME_TOO_LONG, mFilename));
            break;
#endif
        default:
            IDE_SET(ideSetErrorCode(idERR_ABORT_SysCreat, mFilename));
            break;
        }
    }
    
    IDE_EXCEPTION(close_error);
    {
        IDE_SET(ideSetErrorCode(idERR_FATAL_SysClose,
                                mFilename));
    }

    IDE_EXCEPTION_END;

    ideLog::log(IDE_SERVER_0, ID_TRC_CREATE_FAILED,
                sSystemErrno,
                getFileName());

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : 
 ***********************************************************************/
IDE_RC iduFile::createUntilSuccess(iduEmergencyFuncType aSetEmergencyFunc,
                                   idBool               aKillServer)
{
    IDE_RC sRet;
    SInt   sErrorCode;

    while(1)
    {
        sRet = create();

        if(sRet != IDE_SUCCESS)
        {
            sErrorCode = ideGetErrorCode();

            if((sErrorCode == idERR_ABORT_DISK_SPACE_EXHAUSTED)     ||
               (sErrorCode == idERR_ABORT_EXCEED_OPEN_FILE_LIMIT))
            {

                aSetEmergencyFunc(ID_TRUE);

                if( aKillServer == ID_TRUE )
                {
                    IDE_WARNING(IDE_SERVER_0, "System Abnormal Shutdown!!\n"
                                 "ERROR: \n\n"
                                "File Creation Failed...\n"
                                "Check Disk Space And Open File Count!!\n");
                    idlOS::exit(0);
                }
                else
                {
                    IDE_WARNING(IDE_SERVER_0, "File Creation Failed. : "
                                        "The disk space has been exhausted or "
                                        "the limit on the total number of opened files "
                                        "has been reached\n");
                    idlOS::sleep(2);
                    continue;
                }
            }
            else
            {
                aSetEmergencyFunc(ID_FALSE);
                IDE_TEST(sRet != IDE_SUCCESS);
            }
        }
        else
        {
            break;
        }
    }

    aSetEmergencyFunc(ID_FALSE);


    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : ���ο� FD�� Open�Ѵ�.
 *
 * aFD - [OUT] ���� Open�� FD�� �����ȴ�.
 ***********************************************************************/
IDE_RC iduFile::open( PDL_HANDLE *aFD )
{
    SInt        sFOFlag;
    PDL_HANDLE  sFD;
    SInt        sState = 0;
    SInt        sSystemErrno;

    *aFD = IDL_INVALID_HANDLE;

    sFOFlag = getOpenFlag( mIsDirectIO );

    sFD = idf::open( mFilename, sFOFlag );
    
    IDE_TEST_RAISE( sFD == IDL_INVALID_HANDLE, open_error );
    sState = 1;

#if defined(SPARC_SOLARIS)  /* SPARC Series */
    if( (mIsDirectIO == ID_TRUE) && (idf::isVFS() != ID_TRUE) )
    {
        IDE_TEST_RAISE( idlOS::directio( sFD, DIRECTIO_ON ) != 0,
                        err_FATAL_DirectIO );
    }
#endif

    *aFD = sFD;

    return IDE_SUCCESS;

#if defined(SPARC_SOLARIS) /* SPARC Series */
    IDE_EXCEPTION( err_FATAL_DirectIO );
    {
        IDE_SET( ideSetErrorCode( idERR_FATAL_SysDirectIO,
                                  mFilename ) );
    }
#endif
    IDE_EXCEPTION( open_error );
    {
        sSystemErrno = errno;

        switch(sSystemErrno)
        {
        case EEXIST:
            IDE_SET(ideSetErrorCode(idERR_ABORT_FILE_ALREADY_EXIST, mFilename));
            break;
        case ENOENT:
            IDE_SET(ideSetErrorCode(idERR_ABORT_NO_SUCH_FILE, mFilename));
            break;
        case EACCES:
            IDE_SET(ideSetErrorCode(idERR_ABORT_INVALID_ACCESS, mFilename));
            break;
        case ENOMEM:
        case ENOSPC:
            IDE_SET(ideSetErrorCode(
                        idERR_ABORT_DISK_SPACE_EXHAUSTED,
                        mFilename, 0, 0));
            break;
#if defined(EDQUOT)
        case EDQUOT:
            IDE_SET(ideSetErrorCode(
                        idERR_ABORT_DISK_SPACE_EXHAUSTED,
                        mFilename, 0, 0));
            break;
#endif
        case EMFILE:
        case ENFILE:
            IDE_SET(ideSetErrorCode(
                        idERR_ABORT_EXCEED_OPEN_FILE_LIMIT,
                        mFilename, 0, 0));
            break;
#if defined(ENAMETOOLONG)
        case ENAMETOOLONG:
            IDE_SET(ideSetErrorCode(idERR_ABORT_FILENAME_TOO_LONG, mFilename));
            break;
#endif

#if defined(ETXTBSY)
        case ETXTBSY:
#endif
        case EAGAIN:
            IDE_SET(ideSetErrorCode(idERR_ABORT_DISK_OR_DEVICE_BUSY,
                                    mFilename,
                                    0,
                                    0));
        default:
            IDE_SET( ideSetErrorCode( idERR_ABORT_SysOpen, mFilename ) );
            break;
        }
    }
    IDE_EXCEPTION_END;

    if( sState != 0 )
    {
        IDE_ASSERT( idf::close( sFD ) == 0 );
    }

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : ���ο� FD�� Open�Ѵ�. 
 * DiskFull�̳� File Descriptor Full�� ���� open�� ������ ��� ������ ������ ��õ��Ѵ�.
 * BUG-48761 : Permission denied ( ERRNO 13 ) �߰� 
 ***********************************************************************/
IDE_RC iduFile::openUntilSuccess( idBool aIsDirectIO, SInt aPermission )
{
    SInt        sFOFlag;
    PDL_HANDLE  sFD;
    SInt        sState = 0;
    SInt        sSystemErrno;

    IDE_ASSERT( mCurFDCount == 0 );
    mIsDirectIO = aIsDirectIO;
    mPermission = aPermission;

    /* BUG-15961: DirectIO�� ���� �ʴ� System Property�� �ʿ��� */
    if ( iduProperty::getDirectIOEnabled() == 0 ) 
    {   
        mIsDirectIO = ID_FALSE;
    }   

    sFOFlag = getOpenFlag( mIsDirectIO );

    while( 1 )
    {
        sState = 0;

        IDU_FIT_POINT_RAISE( "iduFile::openUntilSuccess::exceptionTest", 
                             exception_test );
        IDU_FIT_POINT_RAISE( "BUG-46596@iduFile::openUntilSuccess::exceptionTest", 
                             exception_test );
        sFD = idf::open( mFilename, sFOFlag );

        if( sFD != IDL_INVALID_HANDLE )
        {
            sState = 1;
            break;
        }
        else
        {
            sSystemErrno = errno;

#if defined(EDQUOT) && defined(ETXTBSY)
            IDE_TEST_RAISE( ( sSystemErrno != ENOMEM ) && // Not enough space, Out of memory
                            ( sSystemErrno != EACCES ) && // Permission denied 
                            ( sSystemErrno != ENOSPC ) && // No space left on device 
                            ( sSystemErrno != EDQUOT ) && // Disc quota exceeded
                            ( sSystemErrno != ENFILE ) && // Too many open files
                            ( sSystemErrno != EMFILE ) && // Too many open files
                            ( sSystemErrno != EAGAIN ) && // Resource temporarily unavailable
                            ( sSystemErrno != ETXTBSY ),  // Resource busy 
                            open_error );
#elif !defined(EDQUOT) && defined(ETXTBSY)
            IDE_TEST_RAISE( ( sSystemErrno != ENOMEM ) && // Not enough space, Out of memory 
                            ( sSystemErrno != EACCES ) && // Permission denied
                            ( sSystemErrno != ENOSPC ) && // No space left on device 
                            ( sSystemErrno != ENFILE ) && // Too many open files
                            ( sSystemErrno != EMFILE ) && // Too many open files
                            ( sSystemErrno != EAGAIN ) && // Resource temporarily unavailable
                            ( sSystemErrno != ETXTBSY ),  // Resource busy 
                            open_error );
#elif defined(EDQUOT) && !defined(ETXTBSY)
            IDE_TEST_RAISE( ( sSystemErrno != ENOMEM ) && // Not enough space, Out of memory
                            ( sSystemErrno != EACCES ) && // Permission denied
                            ( sSystemErrno != ENOSPC ) && // No space left on device 
                            ( sSystemErrno != EDQUOT ) && // Disc quota exceeded
                            ( sSystemErrno != ENFILE ) && // Too many open files
                            ( sSystemErrno != EMFILE ) && // Too many open files
                            ( sSystemErrno != EAGAIN ), // Resource temporarily unavailable
                            open_error );
#else
            IDE_TEST_RAISE( ( sSystemErrno != ENOMEM ) && 
                            ( sSystemErrno != EACCES ) &&
                            ( sSystemErrno != ENOSPC ) && 
                            ( sSystemErrno != ENFILE ) &&
                            ( sSystemErrno != EMFILE ) &&
                            ( sSystemErrno != EAGAIN ),
                            open_error );

#endif 

#ifdef ALTIBASE_FIT_CHECK
            IDE_EXCEPTION_CONT( exception_test );
#endif

            ideLog::log(IDE_SERVER_0, "File: [%s] Open Fail caused by ( errno = %"ID_INT32_FMT" )."
                                      "Retry now.", mFilename, sSystemErrno );

            /* Disk Full/FileDescriptor Full�� ���� ������ ��� 2�� ��� �� ����� */
            idlOS::sleep(2);

            continue;
        }
    }

#if defined(SPARC_SOLARIS)  /* SPARC Series */
    if( (mIsDirectIO == ID_TRUE) && (idf::isVFS() != ID_TRUE) )
    {
        IDE_TEST_RAISE( idlOS::directio( sFD, DIRECTIO_ON ) != 0,
                        err_FATAL_DirectIO );
    }
#endif

    // to remove false alarms from codesonar test
#ifdef __CSURF__
    IDE_ASSERT( mFDStackInfo->mItemSize == ID_SIZEOF(PDL_HANDLE) );
#endif

    /* ���ο� FD�� FDStackInfo�� ����Ѵ�. */
    IDE_TEST( iduFXStack::push( NULL /* idvSQL */, mFDStackInfo, &sFD )
              != IDE_SUCCESS );

    /* ù��° Open�̹Ƿ� mIOOpenMutex�� ���� �ʿ䰡 ����. */
    mCurFDCount++;

    return IDE_SUCCESS;

#if defined(SPARC_SOLARIS) /* SPARC Series */
    IDE_EXCEPTION( err_FATAL_DirectIO );
    {
        IDE_SET( ideSetErrorCode( idERR_FATAL_SysDirectIO,
                                  mFilename ) );
    }
#endif
    IDE_EXCEPTION( open_error );
    {    
        sSystemErrno = errno;

        switch(sSystemErrno)
        {
        case EEXIST:
            IDE_SET(ideSetErrorCode(idERR_ABORT_FILE_ALREADY_EXIST, mFilename));
            break;
        case ENOENT:
            IDE_SET(ideSetErrorCode(idERR_ABORT_NO_SUCH_FILE, mFilename));
            break;
#if defined(ENAMETOOLONG)
        case ENAMETOOLONG:
            IDE_SET(ideSetErrorCode(idERR_ABORT_FILENAME_TOO_LONG, mFilename));
            break;
#endif
        default:
            IDE_SET( ideSetErrorCode( idERR_ABORT_SysOpen, mFilename ) );
            break;
        }
    }    
    IDE_EXCEPTION_END;

    if( sState != 0 )
    {
        IDE_ASSERT( idf::close( sFD ) == 0 );
    }

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : 
 ***********************************************************************/
IDE_RC iduFile::closeAll()
{
    idBool       sIsEmpty;
    PDL_HANDLE   sFD;

    while( 1 )
    {

        // to remove false alarms from codesonar test
#ifdef __CSURF__
        IDE_ASSERT( mFDStackInfo->mItemSize == ID_SIZEOF(PDL_HANDLE) );
#endif

        IDE_TEST( iduFXStack::pop( NULL, /* idvSQL */
                                   mFDStackInfo,
                                   IDU_FXSTACK_POP_NOWAIT,
                                   &sFD,
                                   &sIsEmpty )
                  != IDE_SUCCESS );

        if( sIsEmpty == ID_TRUE )
        {
            break;
        }

        IDE_ASSERT( sFD != IDL_INVALID_HANDLE );

        IDE_ASSERT( mCurFDCount != 0 );

        mCurFDCount--;
        IDE_TEST_RAISE( idf::close( sFD ) != 0,
                        close_error );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( close_error );
    IDE_EXCEPTION_END;

    IDE_SET( ideSetErrorCode( idERR_FATAL_SysClose,
                              mFilename ) );

    return IDE_FAILURE;
}


/***********************************************************************
 * Description : ���� ������ aWhere���� aSize��ŭ Read�� �ؼ� aBuffer
 *               �������ش�.
 *
 * aStatSQL - [IN] �������
 * aWhere   - [IN] Read ���� ��ġ (Byte)
 * aBuffer  - [IN] Read Memory Buffer
 * aSize    - [IN] Read ũ��(Byte)
 ***********************************************************************/
IDE_RC iduFile::read ( idvSQL   * aStatSQL,
                       PDL_OFF_T  aWhere,
                       void*      aBuffer,
                       size_t     aSize )
{
    size_t  sReadSize;

    IDE_TEST( read( aStatSQL,
                    aWhere,
                    aBuffer,
                    aSize,
                    &sReadSize )
              != IDE_SUCCESS );

    IDE_TEST_RAISE( sReadSize != aSize, err_read );

    return IDE_SUCCESS;

    IDE_EXCEPTION( err_read );
    {
        IDE_SET( ideSetErrorCode( idERR_ABORT_SysSeek,
                                  mFilename ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : ���� ������ aWhere���� aSize��ŭ Read�� �ؼ� aBuffer
 *               �������ش�.
 *
 * aStatSQL         - [IN] �������
 * aWhere           - [IN] Read ���� ��ġ (Byte)
 * aBuffer          - [IN] Read Memory Buffer
 * aSize            - [IN] ��û�� Read ũ��(Byte)
 * aRealReadSize    - [OUT] aSize��ŭ Read�� ��û������ ���� �� Byte Read
 *                          �ߴ���.
 ***********************************************************************/
IDE_RC iduFile::read( idvSQL    * aStatSQL,
                      PDL_OFF_T   aWhere,
                      void*       aBuffer,
                      size_t      aSize,
                      size_t *    aRealReadSize )
{
    size_t      sRS;
    idvTime     sBeforeTime;
    PDL_HANDLE  sFD;
    SInt        sState = 0;

    /* Read�� ����� FD�� �Ҵ�޴´� .*/
    IDE_TEST( allocFD( aStatSQL, &sFD ) != IDE_SUCCESS );
    sState = 1;

    mStatFunc[ mIOStatOnOff ].mBeginStat( &sBeforeTime );

    sRS = idf::pread( sFD, aBuffer, aSize, aWhere );
    IDE_TEST_RAISE( (ssize_t)sRS == -1, read_error);

    sState = 0;
    /* Read�� ����� FD�� ��ȯ�Ѵ� .*/
    IDE_TEST( freeFD( sFD ) != IDE_SUCCESS );

    mStatFunc[ mIOStatOnOff ].mEndStat( &mStat,
                                        IDU_FIO_TYPE_READ,
                                        &sBeforeTime );
    if( aRealReadSize != NULL )
    {
        *aRealReadSize = sRS;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION(read_error);
    {
        IDE_SET(ideSetErrorCode(idERR_FATAL_SysRead,
                                mFilename));
    }
    IDE_EXCEPTION_END;

    if( sState != 0 )
    {
        IDE_ASSERT( freeFD( sFD ) == IDE_SUCCESS );
    }

    return IDE_FAILURE;
}

IDE_RC iduFile::readv( idvSQL*              aStatSQL,
                       PDL_OFF_T            aWhere,
                       iduFileIOVec&        aVec )
{
    idvTime     sBeforeTime;
    PDL_HANDLE  sFD;
    SInt        sState = 0;

    /* Read�� ����� FD�� �Ҵ�޴´� .*/
    IDE_TEST( allocFD( aStatSQL, &sFD ) != IDE_SUCCESS );
    sState = 1;

    mStatFunc[ mIOStatOnOff ].mBeginStat( &sBeforeTime );

    IDE_TEST_RAISE( idf::preadv( sFD, aVec, aVec, aWhere )
                    == -1, read_error );

    sState = 0;
    /* Read�� ����� FD�� ��ȯ�Ѵ� .*/
    IDE_TEST( freeFD( sFD ) != IDE_SUCCESS );

    mStatFunc[ mIOStatOnOff ].mEndStat( &mStat,
                                        IDU_FIO_TYPE_READ,
                                        &sBeforeTime );
    return IDE_SUCCESS;

    IDE_EXCEPTION(read_error);
    {
        IDE_SET(ideSetErrorCode(idERR_FATAL_SysRead,
                                mFilename));
    }
    IDE_EXCEPTION_END;

    if( sState != 0 )
    {
        IDE_ASSERT( freeFD( sFD ) == IDE_SUCCESS );
    }

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : File�� aWhere��ġ�� aBuffer�� ������ aSize��ŭ ����Ѵ�.
 *
 * aStatSQL - [IN] �������
 * aWhere   - [IN] Write ���� ��ġ (Byte)
 * aBuffer  - [IN] Write Memory Buffer
 * aSize    - [IN] Write ũ��(Byte)
 ***********************************************************************/
IDE_RC iduFile::write(idvSQL    * aStatSQL,
                      PDL_OFF_T   aWhere,
                      void*       aBuffer,
                      size_t      aSize)
{
    size_t     sRS;
    int        sSystemErrno = 0;
    idvTime    sBeforeTime;
    PDL_HANDLE sFD = PDL_INVALID_HANDLE;
    SInt       sState = 0;

    /* Write�� ����� FD�� �Ҵ�޴´� .*/
    IDE_TEST( allocFD( aStatSQL, &sFD ) != IDE_SUCCESS );
    sState = 1;

    IDE_TEST( checkDirectIOArgument( aWhere,
                                     aBuffer,
                                     aSize )
              != IDE_SUCCESS );

    mStatFunc[ mIOStatOnOff ].mBeginStat( &sBeforeTime );

    sRS = idf::pwrite( sFD, aBuffer, aSize, aWhere );
        
    if( (ssize_t)sRS == -1 )
    {
        sSystemErrno = errno;

        // ENOSPC : disk ���� ����
        // EDQUOT : user disk ���� ����
        IDE_TEST_RAISE( (sSystemErrno == ENOSPC) ||
                        (sSystemErrno == EDQUOT),
                        no_space_error );

        // EFBIG  : ���η����� ȭ�� ũ�� �Ѱ� �ʰ�,
        //          �ý��� max ȭ�� ũ�� �Ѱ� �ʰ�
        IDE_TEST_RAISE( sSystemErrno == EFBIG,
                        exceeed_limit_error );

        IDE_RAISE( error_write );
    }

    sState = 0;
    /* Write�� ����� FD�� ��ȯ�Ѵ� .*/
    IDE_TEST( freeFD( sFD ) != IDE_SUCCESS );

    mStatFunc[ mIOStatOnOff ].mEndStat( &mStat,
                                        IDU_FIO_TYPE_WRITE,
                                        &sBeforeTime );

    IDE_TEST_RAISE( sRS != aSize, no_space_error );

    return IDE_SUCCESS;

    IDE_EXCEPTION(error_write);
    {
        IDE_SET(ideSetErrorCode(idERR_ABORT_SysWrite,
                                mFilename));
    }
    IDE_EXCEPTION(no_space_error);
    {
        IDE_SET(ideSetErrorCode(idERR_ABORT_DISK_SPACE_EXHAUSTED,
                                mFilename,
                                aWhere,
                                aSize));
    }
    IDE_EXCEPTION(exceeed_limit_error);
    {
        IDE_SET(ideSetErrorCode(idERR_ABORT_EXCEED_FILE_SIZE_LIMIT,
                                mFilename,
                                aWhere,
                                aSize));
    }
    IDE_EXCEPTION_END;

    ideLog::log(IDE_SERVER_0, ID_TRC_WRITE_FAILED,
                sSystemErrno,
                getFileName(),
                sFD,
                aSize,
                sRS );

    if( sState != 0 )
    {
        IDE_ASSERT( freeFD( sFD ) == IDE_SUCCESS );
    }

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : 
 ***********************************************************************/
IDE_RC iduFile::writeUntilSuccess(idvSQL  *  aStatSQL,
                                  PDL_OFF_T  aWhere,
                                  void*      aBuffer,
                                  size_t     aSize,
                                  iduEmergencyFuncType aSetEmergencyFunc,
                                  idBool     aKillServer)
{
    IDE_RC  sRet;
    SInt    sErrorCode;
    SChar   sErrorMsg[100];

    IDE_TEST( checkDirectIOArgument( aWhere,
                                     aBuffer,
                                     aSize )
              != IDE_SUCCESS );

    while(1)
    {
        sRet = write( aStatSQL,
                      aWhere,
                      aBuffer,
                      aSize );

        if(sRet != IDE_SUCCESS)
        {
            sErrorCode = ideGetErrorCode();


            if(sErrorCode == idERR_ABORT_DISK_SPACE_EXHAUSTED)
            {
                aSetEmergencyFunc(ID_TRUE);

                idlOS::snprintf(sErrorMsg, ID_SIZEOF(sErrorMsg),
                                "Extending DataFile : from %"ID_UINT32_FMT", "
                                "size %"ID_UINT32_FMT"",
                                aWhere,
                                aSize);

                if( aKillServer == ID_TRUE )
                {
                    ideLog::log(IDE_SERVER_0, ID_TRC_SERVER_ABNORMAL_SHUTDOWN, sErrorMsg);
                    idlOS::exit(0);
                }
                else
                {
                    ideLog::log(IDE_SERVER_0, ID_TRC_FILE_EXTEND_FAILED, sErrorMsg);
                    idlOS::sleep(2);
                    continue;
                }
            }
            else
            {
                aSetEmergencyFunc(ID_FALSE);
                IDE_TEST(sRet != IDE_SUCCESS);
            }
        }
        else
        {
            break;
        }
    }

    aSetEmergencyFunc(ID_FALSE);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC iduFile::writev( idvSQL*              aStatSQL,
                        PDL_OFF_T            aWhere,
                        iduFileIOVec&        aVec )
{
    idvTime     sBeforeTime;
    PDL_HANDLE  sFD;
    SInt        sState = 0;

    IDE_TEST( checkDirectIOArgument( aWhere,
                                     aVec )
              != IDE_SUCCESS );

    /* Read�� ����� FD�� �Ҵ�޴´� .*/
    IDE_TEST( allocFD( aStatSQL, &sFD ) != IDE_SUCCESS );
    sState = 1;

    mStatFunc[ mIOStatOnOff ].mBeginStat( &sBeforeTime );

    IDE_TEST_RAISE( idf::pwritev( sFD, aVec, aVec, aWhere )
                    == -1, write_error );

    sState = 0;
    /* Read�� ����� FD�� ��ȯ�Ѵ� .*/
    IDE_TEST( freeFD( sFD ) != IDE_SUCCESS );

    mStatFunc[ mIOStatOnOff ].mEndStat( &mStat,
                                        IDU_FIO_TYPE_WRITE,
                                        &sBeforeTime );
    return IDE_SUCCESS;

    IDE_EXCEPTION(write_error);
    {
        IDE_SET(ideSetErrorCode(idERR_ABORT_SysWrite,
                                mFilename));
    }
    IDE_EXCEPTION_END;

    if( sState != 0 )
    {
        IDE_ASSERT( freeFD( sFD ) == IDE_SUCCESS );
    }

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : WriteV�� UntilSuccess ���� (PROJ-2647 �� �߰�)
 *               iduFile::writeUntilSuccess()�� �Űܿͼ� ����,
 *               �ڵ� ���� �� �Բ� ���� �ؾ� ��
 ***********************************************************************/
IDE_RC iduFile::writevUntilSuccess(idvSQL  *     aStatSQL,
                                   PDL_OFF_T     aWhere,
                                   iduFileIOVec& aVec,
                                   iduEmergencyFuncType aSetEmergencyFunc,
                                   idBool        aKillServer )
{
    IDE_RC  sRet;
    SInt    sErrorCode;
    SChar   sErrorMsg[100];

    while(1)
    {
        sRet = writev( aStatSQL,
                       aWhere,
                       aVec );

        if( sRet != IDE_SUCCESS )
        {
            sErrorCode = ideGetErrorCode();

            if(sErrorCode == idERR_ABORT_DISK_SPACE_EXHAUSTED)
            {
                aSetEmergencyFunc(ID_TRUE);

                idlOS::snprintf(sErrorMsg, ID_SIZEOF(sErrorMsg),
                                "Extending DataFile : from %"ID_UINT32_FMT"\n",
                                aWhere );

                if( aKillServer == ID_TRUE )
                {
                    ideLog::log(IDE_SERVER_0, ID_TRC_SERVER_ABNORMAL_SHUTDOWN, sErrorMsg);
                    idlOS::exit(0);
                }
                else
                {
                    ideLog::log(IDE_SERVER_0, ID_TRC_FILE_EXTEND_FAILED, sErrorMsg);
                    idlOS::sleep(2);
                    continue;
                }
            }
            else
            {
                aSetEmergencyFunc(ID_FALSE);
                IDE_TEST(sRet != IDE_SUCCESS);
            }
        }
        else
        {
            break;
        }
    }

    aSetEmergencyFunc(ID_FALSE);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : 
 ***********************************************************************/
IDE_RC iduFile::sync()
{
    SInt       sRet;
    SInt       sSystemErrno = 0;
    PDL_HANDLE sFD = PDL_INVALID_HANDLE;
    SInt       sState = 0;

#if !defined(VC_WINCE)
    IDE_TEST( allocFD( NULL, &sFD ) != IDE_SUCCESS );
    sState = 1;

    /* TC/FIT/Server/id/Bugs/BUG-39682/BUG-39682.tc */
    IDU_FIT_POINT_RAISE( "iduFile::sync::EBUSY_ERROR", 
                          device_busy_error ); 
    sRet = idf::fsync( sFD );

    if( sRet == -1 )
    {
        sSystemErrno = errno;

        IDE_TEST_RAISE( sSystemErrno == ENOSPC, no_space_error );
        IDE_TEST_RAISE( sSystemErrno == EBUSY , device_busy_error );
        IDE_RAISE( sync_error );
    }

    sState = 0;
    IDE_TEST( freeFD( sFD ) != IDE_SUCCESS );
#endif

    return IDE_SUCCESS;

    IDE_EXCEPTION(sync_error);
    {
        IDE_SET(ideSetErrorCode(idERR_ABORT_SyncError,
                                mFilename));
    }

    IDE_EXCEPTION(no_space_error);
    {
        IDE_SET(ideSetErrorCode(idERR_ABORT_DISK_SPACE_EXHAUSTED,
                                mFilename,
                                0,
                                0));
    }

    IDE_EXCEPTION(device_busy_error);
    {
        IDE_SET(ideSetErrorCode(idERR_ABORT_DISK_OR_DEVICE_BUSY,
                                mFilename,
                                0,
                                0));
    }
 
    IDE_EXCEPTION_END;

    ideLog::log(IDE_SERVER_0, ID_TRC_FILE_SYNC_FAILED,
                sSystemErrno,
                getFileName(),
                sFD);

    if( sState != 0 )
    {
        IDE_ASSERT( freeFD( sFD ) == IDE_SUCCESS );
    }

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : 
 ***********************************************************************/
IDE_RC iduFile::syncUntilSuccess(iduEmergencyFuncType aSetEmergencyFunc,
                                 idBool               aKillServer)
{
    IDE_RC sRet;
    SInt   sErrorCode;
    idBool sContinue = ID_TRUE;

    while(sContinue == ID_TRUE)
    {
        sRet = sync();

        IDU_FIT_POINT_RAISE( "iduFile::syncUntilSuccess::exceptionTest",
                             exception_test );

        if(sRet != IDE_SUCCESS)
        {
            sErrorCode = ideGetErrorCode();

            switch(sErrorCode)
            {
            case idERR_ABORT_DISK_SPACE_EXHAUSTED:
                if(aSetEmergencyFunc != NULL)
                {
                    aSetEmergencyFunc(ID_TRUE);
                }

                if( aKillServer == ID_TRUE )
                {
                    IDE_WARNING(IDE_SERVER_0, "System Abnormal Shutdown!!\n"
                                "ERROR: \n\n"
                                "File Sync Failed...\n"
                                "Check Disk Space!!\n");
                    idlOS::exit(0);
                }
                else
                {
                    IDE_EXCEPTION_CONT( exception_test );

                    IDE_WARNING(IDE_SERVER_0, "File Sync Failed. : "
                                "The disk space has been exhausted.\n");
                    idlOS::sleep(2);
                }
                break;

            case idERR_ABORT_DISK_OR_DEVICE_BUSY:
                if(aSetEmergencyFunc != NULL)
                {
                    aSetEmergencyFunc(ID_TRUE);
                }

                if( aKillServer == ID_TRUE )
                {
                    IDE_WARNING(IDE_SERVER_0, "System Abnormal Shutdown!!\n"
                                "ERROR: \n\n"
                                "File Sync Failed...\n"
                                "Check Disk Space!!\n");
                    idlOS::exit(0);
                }
                else
                {
                    IDE_WARNING(IDE_SERVER_0, "File Sync Failed. : "
                                "The disk space has been exhausted.\n");
                    idlOS::sleep(2);
                }
                break;

            default:
                if(aSetEmergencyFunc != NULL)
                {
                    aSetEmergencyFunc(ID_FALSE);
                    IDE_TEST( sRet != IDE_SUCCESS );
                }
                else
                {
                }
                break;
            }
        }
        else
        {
            break;
        }

    }

    aSetEmergencyFunc(ID_FALSE);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : 
 ***********************************************************************/
IDE_RC iduFile::copy(idvSQL  * aStatSQL,
                     SChar   * aFileName,
                     idBool    aUseDIO)
{
    iduFile s_destFile;
    UInt    s_nPageSize;

    SInt     s_state = 0;
    SChar   *sBuffer = NULL;
    SChar   *sBufferPtr = NULL;
    ULong    s_nOffset;
    size_t   s_nReadSize;

    IDE_ASSERT(IDU_FILE_COPY_BUFFER % IDU_FILE_DIRECTIO_ALIGN_SIZE == 0);

    IDE_ASSERT(aFileName != NULL);

    s_nPageSize  = idlOS::getpagesize();

    IDE_TEST(iduMemMgr::malloc(mIndex,
                               IDU_FILE_COPY_BUFFER + s_nPageSize - 1,
                               (void**)&sBufferPtr,
                               IDU_MEM_FORCE)
             != IDE_SUCCESS);
    s_state = 1;

    sBuffer = (SChar*)idlOS::align((void*)sBufferPtr, s_nPageSize);

    s_nOffset = 0;

    IDE_TEST( s_destFile.initialize( mIndex,
                                     1, /* Max Open FD Count */
                                     IDU_FIO_STAT_OFF,
                                     mWaitEventID )
              != IDE_SUCCESS );
    s_state = 2;

    IDE_TEST(s_destFile.setFileName(aFileName)
             != IDE_SUCCESS);

    IDE_TEST(s_destFile.create() != IDE_SUCCESS);

    IDE_TEST(s_destFile.open(aUseDIO) != IDE_SUCCESS);
    s_state = 3;

    while(1)
    {
        IDE_TEST(read( aStatSQL,
                       s_nOffset,
                       sBuffer,
                       IDU_FILE_COPY_BUFFER,
                       &s_nReadSize )
                 != IDE_SUCCESS);

        if(aUseDIO == ID_TRUE)
        {
            if(s_nReadSize < IDU_FILE_COPY_BUFFER)
            {
                s_nReadSize = (s_nReadSize + IDU_FILE_DIRECTIO_ALIGN_SIZE - 1) /
                    IDU_FILE_DIRECTIO_ALIGN_SIZE;
                s_nReadSize = s_nReadSize * IDU_FILE_DIRECTIO_ALIGN_SIZE;

                IDE_TEST(s_destFile.write(aStatSQL, s_nOffset, sBuffer, s_nReadSize)
                         != IDE_SUCCESS);
                break;
            }
            else
            {
                IDE_TEST(s_destFile.write(aStatSQL, s_nOffset, sBuffer, s_nReadSize)
                         != IDE_SUCCESS);
            }
        }
        else
        {
            IDE_TEST(s_destFile.write(aStatSQL, s_nOffset, sBuffer, s_nReadSize)
                     != IDE_SUCCESS);

            if(s_nReadSize < IDU_FILE_COPY_BUFFER)
            {
                break;
            }
        }

        s_nOffset += s_nReadSize;
    }

    IDE_TEST(s_destFile.sync() != IDE_SUCCESS);

    s_state = 2;
    IDE_TEST(s_destFile.close() != IDE_SUCCESS);
    s_state = 1;
    IDE_TEST(s_destFile.destroy() != IDE_SUCCESS);

    s_state = 0;
    IDE_TEST(iduMemMgr::free(sBufferPtr) != IDE_SUCCESS);

    sBufferPtr = NULL;
    sBuffer = NULL;
    s_state = 0;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    switch(s_state)
    {
        case 3:
            IDE_ASSERT( s_destFile.close() == IDE_SUCCESS );

        case 2:
            IDE_ASSERT( s_destFile.destroy() == IDE_SUCCESS );

        case 1:
            IDE_ASSERT( iduMemMgr::free( sBufferPtr ) == IDE_SUCCESS );
            sBuffer = NULL;
            sBufferPtr = NULL;

        default:
            break;
    }

    (void)idf::unlink(aFileName);

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : DirectIO�� File�� open�Ǿ��� ��� ������ OS���� �� �ٸ�����
 *               �����ϰ� ó���ϱ� ���ؼ� ������ ���� ��������� �Ӵϴ�.
 *
 *               * read or write�� �Ѱ����� ���ڴ� DIRECT_IO_PAGE_SIZE��
 *                 Align�Ǿ�� �մϴ�. 
 *
 * aWhere  - [IN] ������ read or write������ �ǹ��ϴ� offset
 * aBuffer - [IN] ����� ������ �����ּ�
 * aSize   - [IN] �󸶳� read or write������ �ǹ��ϴ� size
 * 
 ***********************************************************************/
IDE_RC iduFile::checkDirectIOArgument( PDL_OFF_T  aWhere,
                                       void*      aBuffer,
                                       size_t     aSize )
{
    if( mIsDirectIO == ID_TRUE )
    {
        IDE_TEST_RAISE( ( aWhere %
                          iduProperty::getDirectIOPageSize() ) != 0,
                        err_invalid_argument_directio );

        IDE_TEST_RAISE( ( (vULong)aBuffer %
                          iduProperty::getDirectIOPageSize() ) != 0,
                        err_invalid_argument_directio );

        IDE_TEST_RAISE( ( (ULong)aSize %
                          iduProperty::getDirectIOPageSize() ) != 0,
                        err_invalid_argument_directio );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( err_invalid_argument_directio )
    {
        IDE_SET(ideSetErrorCode(idERR_FATAL_DirectIO_Invalid_Argument,
                                mFilename,
                                aWhere,
                                aBuffer,
                                aSize ));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : DirectIO�� File�� open�Ǿ��� ��� ������ OS���� �� �ٸ�����
 *               �����ϰ� ó���ϱ� ���ؼ� ������ ���� ��������� �Ӵϴ�.
 *
 *               * read or write�� �Ѱ����� ���ڴ� DIRECT_IO_PAGE_SIZE��
 *                 Align�Ǿ�� �մϴ�. 
 *
 * aWhere  - [IN] ������ read or write������ �ǹ��ϴ� offset
 * aVec    - [IN] writev system call�� ���Ǵ� IO Vector
 * 
 ***********************************************************************/
IDE_RC iduFile::checkDirectIOArgument( PDL_OFF_T      aWhere,
                                       iduFileIOVec&  aVec )
{
    SInt        i;
    iovec     * sIOVec;

    if( mIsDirectIO == ID_TRUE )
    {
        i = 0;
        sIOVec = aVec.getIOVec();

        IDE_TEST_RAISE( ( aWhere %
                          iduProperty::getDirectIOPageSize() ) != 0,
                        err_invalid_argument_directio );

        for( ; i < aVec.getCount() ; i++ )
        {
            IDE_TEST_RAISE( ( (vULong)sIOVec[i].iov_base %
                              iduProperty::getDirectIOPageSize() ) != 0,
                            err_invalid_argument_directio );

            IDE_TEST_RAISE( ( (ULong)sIOVec[i].iov_len %
                              iduProperty::getDirectIOPageSize() ) != 0,
                            err_invalid_argument_directio );
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( err_invalid_argument_directio )
    {
        IDE_SET(ideSetErrorCode(idERR_FATAL_DirectIO_Invalid_Argument,
                                mFilename,
                                aWhere,
                                sIOVec[i].iov_base,
                                sIOVec[i].iov_len ));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * BUG-14625 [WIN-ATAF] natc/TC/Server/sm4/sm4.ts��
 * �������� �ʽ��ϴ�.
 *
 * ���� Direct I/O�� �Ѵٸ� Log Buffer�� ���� �ּ� ����
 * Direct I/O Pageũ�⿡ �°� Align�� ���־�� �Ѵ�.
 * �̿� ����Ͽ� �α� ���� �Ҵ�� Direct I/O Page ũ�⸸ŭ
 * �� �Ҵ��Ѵ�.
***********************************************************************/
IDE_RC iduFile::allocBuff4DirectIO( iduMemoryClientIndex   aIndex,
                                    UInt                   aSize,
                                    void**                 aAllocBuff,
                                    void**                 aAllocAlignedBuff )
{
    aSize += iduProperty::getDirectIOPageSize();

    IDE_TEST(iduMemMgr::malloc( aIndex,
                                aSize,
                                aAllocBuff )
             != IDE_SUCCESS);

    *aAllocAlignedBuff= (SChar*)idlOS::align(
        *aAllocBuff,
        iduProperty::getDirectIOPageSize() );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : �ش� ���۰� Direct IO�� �Ҽ� �ִ� �������� �Ǵ��Ѵ�.
 *
 * aMemPtr - [IN] ������ �����ּ�
 *
 ***********************************************************************/
idBool iduFile::canDirectIO( PDL_OFF_T  aWhere,
                             void*      aBuffer,
                             size_t     aSize )
{
    idBool sCanDirectIO = ID_TRUE;

    if( aWhere % iduProperty::getDirectIOPageSize() != 0 )
    {
        sCanDirectIO = ID_FALSE;
    }

    if( (vULong)aBuffer % iduProperty::getDirectIOPageSize() != 0 )
    {
        sCanDirectIO = ID_FALSE;
    }

    if( (ULong)aSize % iduProperty::getDirectIOPageSize() != 0)
    {
        sCanDirectIO = ID_FALSE;
    }

    return sCanDirectIO;
}

/***********************************************************************
 * Description : DirectIO�� �����ҷ��� File�� Flag���� �����Ѵ�.
 *
 * aIsDirectIO - [IN] open�ҷ��� File�� ���ؼ� DirectIO�� �Ѵٸ� ID_TRUE,
 *                    �ƴϸ� ID_FALSE
 *
 * + Related Issue
 *   - BUG-17818 : DirectIO�� ���� �����մϴ�.
 *
 ***********************************************************************/
SInt iduFile::getOpenFlag( idBool aIsDirectIO )
{
    SInt sFlag = mPermission;

    if( aIsDirectIO == ID_TRUE )
    {
#if defined( SPARC_SOLARIS )
        /* SPARC Solaris */
        sFlag |= O_DSYNC ;

#elif defined( DEC_TRU64 )
        /* DEC True64 */
        sFlag |= O_DSYNC;

    #if OS_MAJORVER > 4
        sFlag |= O_DIRECTIO;
    #else
        sFlag |= O_SYNC;
    #endif

#elif defined( VC_WIN32 )
        /* WINDOWS */
    #if !defined ( VC_WINCE )
        sFlag |= ( FILE_FLAG_NO_BUFFERING | FILE_FLAG_WRITE_THROUGH );
    #else
         /* WIN CE */
        sFlag |= O_SYNC;
    #endif /* VC_WINCE */

#elif defined( ALPHA_LINUX ) || defined( IA64_LINUX ) || \
      defined( POWERPC_LINUX ) || defined( POWERPC64_LINUX) || defined( IBM_AIX ) || defined( INTEL_LINUX ) || defined( XEON_LINUX )
        /* LINUX */
        sFlag |= ( O_DIRECT | O_SYNC );

#elif ( defined( HP_HPUX ) || defined( IA64_HP_HPUX ))  && \
      (( OS_MAJORVER == 11 ) && ( OS_MINORVER > 0 ) || ( OS_MAJORVER > 11 ))
        /* HPUX on PARISK, HPUX on IA64 */
        sFlag |= O_DSYNC;
#else
        sFlag |= O_SYNC;
#endif
    }
    else
    {
        /* Bufferd IO */
    }

    return sFlag;
}

/***********************************************************************
 * Description : IO�� ���� ��������� �����Ѵ�.
 *
 * aStatPtr    - [IN] �������
 * aFIOType    - [IN] IO Type:IDU_FIO_TYPE_READ, IDU_FIO_TYPE_WRITE
 * aBeginTime  - [IN] IO ���� �ð�.
 ***********************************************************************/
void iduFile::endFIOStat( iduFIOStat    * aStatPtr,
                          iduFIOType      aFIOType,
                          idvTime       * aBeginTime )
{
    idvTime  sEndTime;
    ULong    sTimeDiff;

    IDE_DASSERT( aStatPtr   != NULL );
    IDE_DASSERT( aBeginTime != NULL );

    IDV_TIME_GET( &sEndTime );
    sTimeDiff = IDV_TIME_DIFF_MICRO( aBeginTime, &sEndTime );

    switch ( aFIOType )
    {
        case IDU_FIO_TYPE_READ:
            IDU_FIO_STAT_ADD( aStatPtr, mReadTime, sTimeDiff );
            // ��Ƽ���̽��� Disk Read�� ��� ���� Block Read �̴�. 
            IDU_FIO_STAT_ADD( aStatPtr, mSingleBlockReadTime, sTimeDiff );

            if ( aStatPtr->mMaxIOReadTime < sTimeDiff )
            {
                IDU_FIO_STAT_SET( aStatPtr, mMaxIOReadTime, sTimeDiff );
            }

            IDU_FIO_STAT_ADD( aStatPtr, mPhyReadCount, 1 );
            IDU_FIO_STAT_ADD( aStatPtr, mPhyBlockReadCount, 1 );
            break;

        case IDU_FIO_TYPE_WRITE:
            IDU_FIO_STAT_ADD( aStatPtr, mWriteTime, sTimeDiff );

            if ( aStatPtr->mMaxIOWriteTime < sTimeDiff )
            {
                IDU_FIO_STAT_SET( aStatPtr, mMaxIOWriteTime, sTimeDiff );
            }

            IDU_FIO_STAT_ADD( aStatPtr, mPhyWriteCount, 1 );
            IDU_FIO_STAT_ADD( aStatPtr, mPhyBlockWriteCount, 1 );
            break;

        default:
            // 2���� Type �̿ܿ��� ���� ����!!
            IDE_ASSERT( 0 );
            break;
    }

    // I/O �ּ� �ҿ�ð� ����
    if ( aStatPtr->mMinIOTime > sTimeDiff )
    {
        IDU_FIO_STAT_SET( aStatPtr, mMinIOTime, sTimeDiff );
    }

    // I/O ������ �ҿ�ð� ����
    IDU_FIO_STAT_SET( aStatPtr, mLstIOTime, sTimeDiff );
}

/***********************************************************************
 * Description : FD�� �Ҵ�޴´�.
 *
 * aFD - [OUT] �Ҵ�� FD�� ����� out ����.
 *
 ***********************************************************************/
IDE_RC iduFile::allocFD( idvSQL     *aStatSQL,
                         PDL_HANDLE *aFD )
{
    idBool              sIsEmpty;
    PDL_HANDLE          sFD;
    SInt                sState = 0;
    iduFXStackWaitMode  sWaitMode = IDU_FXSTACK_POP_NOWAIT;

    IDE_ASSERT( mCurFDCount != 0 );

    IDU_FIT_POINT( "1.BUG-29452@iduFile::allocFD" );
                
    while(1)
    {

        // to remove false alarms from codesonar test
#ifdef __CSURF__
        IDE_ASSERT( mFDStackInfo->mItemSize == ID_SIZEOF(PDL_HANDLE) );
#endif

        /* mFDStackInfo�� ���� ��������� �ʴ� FD�� �ִ�����
           NO Wait Mode�� ���캻��. */
        IDE_TEST( iduFXStack::pop( aStatSQL,
                                   mFDStackInfo,
                                   sWaitMode,
                                   (void*)aFD,
                                   &sIsEmpty )
                  != IDE_SUCCESS );

        if( sIsEmpty == ID_TRUE )
        {
            /* ��� �ִٸ� ���� Open�� FD������ open�� ����
               �ִ� FD������ �ʰ��ߴ����� �����Ѵ�. */
            IDE_TEST( mIOOpenMutex.lock( NULL ) != IDE_SUCCESS );
            sState = 1;

            if( mCurFDCount < mMaxFDCount )
            {
                /* ���� Open�� FD������ �ִ� FD�������� �����Ƿ�
                 * ���ο� FD�� ������� Open�Ѵ�. */

                mCurFDCount++;

                /* ���ü� ����� ���ؼ� open system call�� mutex��
                   Ǯ�� �����Ѵ�. mCurFDCount�� �������ѳ��� �ϱ�
                   ������ �ٸ� Thread�� ���ؼ� Max���� �ʰ��ؼ�
                   Open�ϴ� ���� �߻����� �ʴ´�. */
                sState = 0;
                IDE_TEST( mIOOpenMutex.unlock() != IDE_SUCCESS );

                IDE_TEST_RAISE( open( &sFD ) != IDE_SUCCESS,
                                err_file_open );

                /* ���� Open�� FD�� �ٷ� �Ҵ��� �ش�. */
                *aFD = sFD;
                break;
            }
            else
            {
                sState = 0;
                IDE_TEST( mIOOpenMutex.unlock() != IDE_SUCCESS );

                /* �� �̻� Open�� �� �����Ƿ� ��� Mode�� Stack��
                 * Pop�� ��û�Ѵ�. */
                sWaitMode = IDU_FXSTACK_POP_WAIT;
            }
        }
        else
        {
            break;
        }
    }
    return IDE_SUCCESS;

    IDE_EXCEPTION( err_file_open );
    {
        /* mCurFDCount�� ������Ű�� Open�� �ϱ⶧���� �����ϸ�
           ���ҽ�Ų��. */
        IDE_ASSERT( mIOOpenMutex.lock( NULL ) == IDE_SUCCESS );
        mCurFDCount--;
        IDE_ASSERT( mIOOpenMutex.unlock() == IDE_SUCCESS );
    }
    IDE_EXCEPTION_END;

    if( sState != 0 )
    {
        IDE_ASSERT( mIOOpenMutex.unlock() == IDE_SUCCESS );
    }

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : read, write���� ���ؼ� �Ҵ��ߴ� FD�� ��ȯ�Ѵ�.
 *
 * aFD - [IN] File Descriptor
 *
 ***********************************************************************/
IDE_RC iduFile::freeFD( PDL_HANDLE aFD )
{
    SInt sState = 0;

    IDE_ASSERT( aFD != IDL_INVALID_HANDLE );

    if( mCurFDCount > mMaxFDCount )
    {
        IDE_TEST( mIOOpenMutex.lock( NULL ) != IDE_SUCCESS );
        sState = 1;

        if( mCurFDCount > mMaxFDCount )
        {
            mCurFDCount--;

            sState = 0;
            IDE_TEST( mIOOpenMutex.unlock() != IDE_SUCCESS );

            IDE_TEST( close( aFD ) != IDE_SUCCESS );

            /* FD��  OS���� ��ȯ�Ͽ����Ƿ� Stack��
               ���� �ʴ´�. */
            IDE_RAISE( skip_push_fd_to_stack );
        }
        else
        {
            sState = 0;
            IDE_TEST( mIOOpenMutex.unlock() != IDE_SUCCESS );
        }
    }

    // to remove false alarms from codesonar test
#ifdef __CSURF__
    IDE_ASSERT( mFDStackInfo->mItemSize == ID_SIZEOF(PDL_HANDLE) );
#endif

    IDE_TEST( iduFXStack::push( NULL, /* idvSQL */
                                mFDStackInfo,
                                &aFD )
              != IDE_SUCCESS );

    IDE_EXCEPTION_CONT( skip_push_fd_to_stack );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if( sState != 0 )
    {
        IDE_ASSERT( mIOOpenMutex.unlock() == IDE_SUCCESS );
    }

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : aMaxFDCnt�� iduFile�� �ִ� FD ������ �����Ѵ�.
 *
 * aMaxFDCnt - [IN] ���� ������ FD �ִ� ����
 *
 ***********************************************************************/
IDE_RC iduFile::setMaxFDCnt( SInt aMaxFDCnt )
{
    SInt        sState = 0;
    PDL_HANDLE  sFD;

    /* �ִ� FD������ ID_MAX_FILE_DESCRIPTOR_COUNT���� Ŭ�� ����.*/
    IDE_ASSERT( aMaxFDCnt <= ID_MAX_FILE_DESCRIPTOR_COUNT );

    IDE_TEST( mIOOpenMutex.lock( NULL ) != IDE_SUCCESS );
    sState = 1;

    mMaxFDCount = aMaxFDCnt;

    while( (UInt)aMaxFDCnt < mCurFDCount )
    {
        mCurFDCount--;

        sState = 0;
        IDE_TEST( mIOOpenMutex.unlock() != IDE_SUCCESS );

        IDE_TEST( allocFD( NULL, &sFD ) != IDE_SUCCESS );

        IDE_TEST( close( sFD ) != IDE_SUCCESS );

        IDE_TEST( mIOOpenMutex.lock( NULL ) != IDE_SUCCESS );
        sState = 1;
    }

    sState = 0;
    IDE_TEST( mIOOpenMutex.unlock() != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if( sState != 0 )
    {
        IDE_ASSERT( mIOOpenMutex.unlock() == IDE_SUCCESS );
    }

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : aFileSize�� Truncate�� �����Ѵ�.
 *
 * aFileSize - [IN] File�� Truncate�� ũ��.
 *
 ***********************************************************************/
IDE_RC iduFile::truncate( ULong aFileSize )
{
    PDL_HANDLE sFD;
    SInt       sState = 0;

    IDE_TEST( allocFD( NULL /* idvSQL */, &sFD )
              != IDE_SUCCESS );
    sState = 1;

    IDE_TEST_RAISE(idf::ftruncate( sFD, aFileSize) != 0,
                   error_cannot_shrink_file );

    sState = 0;
    IDE_TEST( freeFD( sFD )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION( error_cannot_shrink_file );
    {
        IDE_SET( ideSetErrorCode( idERR_ABORT_CannotShrinkFile,
                                  mFilename ));
    }
    IDE_EXCEPTION_END;

    if( sState != 0 )
    {
        IDE_ASSERT( freeFD( sFD ) == IDE_SUCCESS );
    }

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : manipulate file space
 *
 * aSize    - [IN] Write ũ��(Byte)
 ***********************************************************************/
IDE_RC iduFile::fallocate( SLong  aSize )
{
    SInt       sSystemErrno = 0;
    idvTime    sBeforeTime;
    PDL_HANDLE sFD = PDL_INVALID_HANDLE;
    SInt       sState = 0;

    /* ����� FD�� �Ҵ�޴´� .*/
    IDE_TEST( allocFD( NULL/* idvSQL */, &sFD ) != IDE_SUCCESS );
    sState = 1;

    mStatFunc[ mIOStatOnOff ].mBeginStat( &sBeforeTime );

    IDE_TEST_RAISE( idlOS::fallocate( sFD, 0, 0, aSize ) != 0,
                    error_fallocate );
        
    sState = 0;
    /* ����� FD�� ��ȯ�Ѵ� .*/
    IDE_TEST( freeFD( sFD ) != IDE_SUCCESS );

    mStatFunc[ mIOStatOnOff ].mEndStat( &mStat,
                                        IDU_FIO_TYPE_WRITE,
                                        &sBeforeTime );

    return IDE_SUCCESS;

    IDE_EXCEPTION( error_fallocate )
    {
        sSystemErrno = errno;

        switch(sSystemErrno) 
        {
#if defined(EDQUOT)
        case EDQUOT:
#endif
        /*  ENOSPC 
              There is not enough space left on the device containing the
         *    file referred to by fd. */
        case ENOSPC:
            IDE_SET(ideSetErrorCode( idERR_ABORT_DISK_SPACE_EXHAUSTED,
                                     mFilename,
                                     0,
                                     aSize) );
            break; 
        /* EFBIG  
              offset+len exceeds the maximum file size. */
        case EFBIG:
            IDE_SET(ideSetErrorCode( idERR_ABORT_EXCEED_FILE_SIZE_LIMIT,
                                     mFilename,
                                     0,
                                     aSize) );
            break; 
        /* ENOSYS 
         *    This kernel does not implement fallocate().
         * EOPNOTSUPP
         *    The filesystem containing the file referred to by fd does not
         *    support this operation; or the mode is not supported by the
         *    filesystem containing the file referred to by fd.  */
        case EOPNOTSUPP:
        case ENOSYS:
            IDE_SET(ideSetErrorCode( idERR_ABORT_NOT_SUPPORT_FALLOCATE,
                                     mFilename ) );
            break; 
        default:
            IDE_SET(ideSetErrorCode( idERR_ABORT_Sysfallocate,
                                     mFilename ) );
        break; 
        }
    }
    IDE_EXCEPTION_END;

    ideLog::log( IDE_SERVER_0, 
                 ID_TRC_WRITE_FAILED,
                 sSystemErrno,
                 getFileName(),
                 sFD,
                 aSize,
                 0 );

    if( sState != 0 )
    {
        IDE_ASSERT( freeFD( sFD ) == IDE_SUCCESS );
    }

    return IDE_FAILURE;
}


/***********************************************************************
 * Description : aSize�� aWrite Mode�� mmap�Ѵ�.
 *
 * aSize   - [IN] mmap�� ������ ũ��.
 * aWrite  - [IN] mmap�� ������ ���ؼ� write������ ����.
 ***********************************************************************/
IDE_RC iduFile::mmap( idvSQL     *aStatSQL,
                      UInt        aSize,
                      idBool      aWrite,
                      void**      aMMapPtr )
{
    SInt       sProt;
    SInt       sFlag;
    SChar      sMsgBuf[256];
    void*      sMMapPtr;
    PDL_HANDLE sFD;
    SInt       sState = 0;

#if defined(NTO_QNX) || defined (VC_WINCE)
    IDE_ASSERT(0); //Not Support In QNX and WINCE
#endif

    sProt = PROT_READ;
    sFlag = MAP_PRIVATE;

    if( aWrite == ID_TRUE )
    {
        sProt |= PROT_WRITE;
        sFlag  = MAP_SHARED;
    }

    IDE_TEST( allocFD( aStatSQL, &sFD ) != IDE_SUCCESS );
    sState = 1;
    
    while(1)
    {
        sMMapPtr = (SChar*)idlOS::mmap( 0, aSize, sProt, sFlag, sFD, 0 );

        if ( sMMapPtr != MAP_FAILED )
        {
            break;
        }
        else
        {
            IDE_TEST_RAISE((errno != EAGAIN) && (errno != ENOMEM),
                           error_file_mmap);

            ideLog::log(IDE_SERVER_0,
                        ID_TRC_MRECOVERY_FILE_MMAP_ERROR,
                        errno);

            if ( errno == ENOMEM )
            {
                // To Fix PR-14859 redhat90���� redo���� hang�߻�
                // Hang�߻��� ��������� ����. �޸� �����̶��
                // �޼����� �ֿܼ� �ѷ��־� ����ڰ� Hang��Ȳ��
                // �ƴ϶� �޸� ������Ȳ���� �ľ��ϵ��� �˷��ش�.
                idlOS::memset( sMsgBuf, 0, 256 );
                idlOS::snprintf( sMsgBuf, 256, "Failed to mmmap log file"
                                 "( errno=ENOMEM(%"ID_UINT32_FMT"),"
                                 "Not enough memory \n", (UInt) errno);

                IDE_CALLBACK_SEND_SYM( sMsgBuf );
            }

            idlOS::sleep(2);
        }
    }

    (void)idlOS::madvise((caddr_t)sMMapPtr, (size_t)aSize, MADV_WILLNEED | MADV_SEQUENTIAL);

    sState = 0;
    IDE_TEST( freeFD( sFD ) != IDE_SUCCESS );
    iduMemMgr::server_statupdate(IDU_MEM_MAPPED, aSize, 1);

    *aMMapPtr = sMMapPtr;

    return IDE_SUCCESS;

    IDE_EXCEPTION( error_file_mmap );
    {
        IDE_SET( ideSetErrorCode( idERR_FATAL_MmapFail ) );
    }
    IDE_EXCEPTION_END;

    if( sState != 0 )
    {
        IDE_PUSH();
        sState = 0;
        IDE_ASSERT( freeFD( sFD ) == IDE_SUCCESS );
        IDE_POP();
    }

    *aMMapPtr = NULL;

    return IDE_FAILURE;
}

IDE_RC iduFile::munmap(void* aPtr, UInt aSize)
{
#if !defined(VC_WIN32) && !defined(IA64_HP_HPUX)
    (void)idlOS::madvise((caddr_t)aPtr, (size_t)aSize, MADV_DONTNEED);
#else
    //�ش� ������ ������ �Լ��� ã�� �־�� �մϴ�.
#endif

    IDE_TEST(idlOS::munmap(aPtr, aSize ) != 0);
    iduMemMgr::server_statupdate(IDU_MEM_MAPPED, -(SLong)aSize, -1);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    return IDE_FAILURE;
}
