/***********************************************************************
 * Copyright 1999-2000, RTBase Corporation or its subsidiaries.
 * All rights reserved.
 **********************************************************************/

/***********************************************************************
 * $Id: iduFile.h 85333 2019-04-26 02:34:41Z et16 $
 **********************************************************************/

/***********************************************************************
 *
 * NAME
 *   iduFile.h
 *
 * DESCRIPTION
 *
 * PUBLIC FUNCTION(S)
 *
 * PRIVATE FUNCTION(S)
 *
 * NOTES
 *
 * MODIFIED    (MM/DD/YYYY)
 *    hyyou     02/11/2000 - Modified
 *
 **********************************************************************/

#ifndef _O_IDU_FILE_H_
#define _O_IDU_FILE_H_ 1

#include <idErrorCode.h>
#include <idl.h>
#include <idv.h>
#include <iduFXStack.h>
#include <iduMemDefs.h>
#include <iduProperty.h>
#include <iduFileIOVec.h>

#define ID_MAX_FILE_DESCRIPTOR_COUNT  1024

#if defined(IBM_AIX)
#define IDU_FILE_DIRECTIO_ALIGN_SIZE 512
#else
#define IDU_FILE_DIRECTIO_ALIGN_SIZE 4096
#endif

/* BUG-30834: Direct IO limitation of Windows */
/* Some Windows versions has the limitation of buffer size for direct IO. */
#if defined(VC_WIN32)
# if (OS_MAJORVER == 5)
#  if defined(ALTI_CFG_CPU_X86) && defined(COMPILE_64BIT)
#   define IDU_FILE_DIRECTIO_MAX_SIZE 33525760 /* 64bit XP/2K3 */
#  else
#   define IDU_FILE_DIRECTIO_MAX_SIZE 67076096 /* 32bit XP/2K3 or IA-64 XP/2K3 */
#  endif
# elif (OS_MAJORVER == 6) && (OS_MINORVER == 0)
#  define IDU_FILE_DIRECTIO_MAX_SIZE 2147479552 /* for Vista/2K8 */
# else
#  define IDU_FILE_DIRECTIO_MAX_SIZE 4294963200 /* for Win7/2K8R2 or upper */
# endif
#else
#define IDU_FILE_DIRECTIO_MAX_SIZE ID_SLONG_MAX /* no limitation */
#endif


/* ------------------------------------------------
 * ���Ϻ� IO �������
 * ----------------------------------------------*/ 
typedef struct iduFIOStat
{
    /* Physical Read I/O �߻� Ƚ�� */
    ULong    mPhyReadCount;
    /* Physical Wrute I/O �߻� Ƚ�� */
    ULong    mPhyWriteCount;
    /* Physical Read�� �ǵ��� Block ���� */
    ULong    mPhyBlockReadCount;
    /* Physical Write�� ����� Block ���� */
    ULong    mPhyBlockWriteCount;
    /* Read I/O time  */
    ULong    mReadTime;
    /* Write I/O time  */
    ULong    mWriteTime;    
    /* ���� Block Read I/O time  */
    ULong    mSingleBlockReadTime;
    /* ������ I/O time  */
    ULong    mLstIOTime;
    /*  I/O �ּ� time  */
    ULong    mMinIOTime;
    /*  Read I/O �ִ� time  */
    ULong    mMaxIOReadTime;
    /*  Write I/O �ִ� time  */
    ULong    mMaxIOWriteTime;
} iduFIOStat;

/* ------------------------------------------------
 * ���Ϻ� IO ������� ���� ��� On/Off �÷���
 * ----------------------------------------------*/ 
typedef enum iduFIOStatOnOff
{
    IDU_FIO_STAT_OFF = 0,
    IDU_FIO_STAT_ON,
    IDU_FIO_STAT_ONOFF_MAX

} iduFIOStatOnOff; 

/* ------------------------------------------------
 * ���Ϻ� IO ������� ���� ��� On/Off �÷���
 * ----------------------------------------------*/ 
typedef enum iduFIOType
{
    IDU_FIO_TYPE_READ = 0,
    IDU_FIO_TYPE_WRITE
    
} iduFIOType;

// DIRECT : pointer should not be NULL!!!
#define IDU_FIO_STAT_ADD(__iduBasePtr__, __Name__, __Value__) \
        {(__iduBasePtr__)->__Name__ += __Value__;}

#define IDU_FIO_STAT_SET(__iduBasePtr__, __Name__, __Value__) \
        {(__iduBasePtr__)->__Name__ = __Value__;}

typedef void (*iduEmergencyFuncType)(idBool aFlag);

typedef void (*iduFIOStatBegin)( idvTime * aTime );

typedef void (*iduFIOStatEnd)( iduFIOStat   * aStatPtr,
                               iduFIOType     aFioType,
                               idvTime      * aBeginTime );

typedef struct iduFIOStatFunc
{
   iduFIOStatBegin  mBeginStat;
   iduFIOStatEnd    mEndStat;

} iduFIOStatFunc;

#define IDU_FD_STACK_INFO_SIZE ( ID_SIZEOF( iduFXStackInfo ) + \
                                 ID_SIZEOF( PDL_HANDLE ) * ID_MAX_FILE_DESCRIPTOR_COUNT )

/* BUG-17954 pread, pwrite�� ���� OS������ ��� IO�� �� �Լ�����
   Global Mutex�� lock�� ��� IO�� �����Ͽ� ��� Disk IO�� �����ϴ�
   ������ ����. �ϳ��� FD�� ���ؼ� �ϳ��� IO ��û�� �ǵ��� ���ü�
   ��� ��. */
class iduFile
{
public:
    IDE_RC     initialize( iduMemoryClientIndex aIndex,
                           UInt                 aMaxMaxFDCnt,
                           iduFIOStatOnOff      aIOStatOnOff,
                           idvWaitIndex         aIOMutexWaitEventID );
    IDE_RC     destroy();

    IDE_RC     create();
    IDE_RC     createUntilSuccess( iduEmergencyFuncType setEmergencyFunc,
                                   idBool aKillServer = ID_FALSE );

    IDE_RC     open(idBool a_bDirect   = ID_FALSE,
                    SInt   aPermission = O_RDWR); /* BUG-31702 add permission parameter */

    IDE_RC     openUntilSuccess(idBool a_bDirect   = ID_FALSE,
                                SInt   aPermission = O_RDWR); /* BUG-31702 add permission parameter */

    IDE_RC     close();
    IDE_RC     sync();
    IDE_RC     syncUntilSuccess(iduEmergencyFuncType setEmergencyFunc,
                                idBool aKillServer = ID_FALSE);

    inline void dump();

    inline IDE_RC    getFileSize( ULong *aSize );
    inline IDE_RC    setFileName( SChar *aFileName );
    inline SChar*    getFileName() { return mFilename; };

    inline idBool exist();

    IDE_RC     write( idvSQL     * aStatSQL,
                      PDL_OFF_T    aWhere,
                      void*        aBuffer,
                      size_t       aSize );

    IDE_RC     writeUntilSuccess( idvSQL             * aStatSQL,
                                  PDL_OFF_T            aWhere,
                                  void*                aBuffer,
                                  size_t               aSize,
                                  iduEmergencyFuncType setEmergencyFunc,
                                  idBool aKillServer = ID_FALSE );

    IDE_RC     writev ( idvSQL*              aStatSQL,
                        PDL_OFF_T            aWhere,
                        iduFileIOVec&        aVec );

    IDE_RC    writevUntilSuccess( idvSQL  *     aStatSQL,
                                  PDL_OFF_T     aWhere,
                                  iduFileIOVec& aVec,
                                  iduEmergencyFuncType aSetEmergencyFunc,
                                  idBool        aKillServer = ID_FALSE );

    IDE_RC     read ( idvSQL *   aStatSQL,
                      PDL_OFF_T  aWhere,
                      void*      aBuffer,
                      size_t     aSize );
    
    IDE_RC     read ( idvSQL *    aStatSQL,
                      PDL_OFF_T   aWhere,
                      void*       aBuffer,
                      size_t      aSize,
                      size_t*     aRealReadSize );

    IDE_RC     readv ( idvSQL*              aStatSQL,
                       PDL_OFF_T            aWhere,
                       iduFileIOVec&        aVec );

    IDE_RC     copy ( idvSQL * aStatSQL, 
                      SChar  * a_pFileName, 
                      idBool   a_bDirect = ID_TRUE );

    static IDE_RC allocBuff4DirectIO( iduMemoryClientIndex   aIndex,
                                      UInt    aSize,
                                      void**  aAllocBuff,
                                      void**  aAllocAlignedBuff );

    iduFIOStatOnOff getFIOStatOnOff() { return mIOStatOnOff; }

    static idBool canDirectIO( PDL_OFF_T  aWhere,
                               void*      aBuffer,
                               size_t     aSize );

    IDE_RC setMaxFDCnt( SInt aMaxFDCnt );

    inline UInt getMaxFDCnt();
    inline UInt getCurFDCnt();

    IDE_RC allocFD( idvSQL     *aStatSQL,
                    PDL_HANDLE *aFD );

    IDE_RC freeFD( PDL_HANDLE aFD );

    /*
     * BUG-27779 iduFile::truncate�Լ� ���ڰ� UInt�� ID_UINT_MAX�� 
     *           �Ѵ� ũ��� truncate�� �������մϴ�. 
     */
    IDE_RC truncate( ULong aFileSize );

    IDE_RC fallocate( SLong  aSize );

    IDE_RC mmap( idvSQL     *aStatSQL,
                 UInt        aSize,
                 idBool      aWrite,
                 void**      aMMapPtr );
    IDE_RC munmap(void*, UInt);

private:
    IDE_RC open( PDL_HANDLE *aFD );

    inline IDE_RC close( PDL_HANDLE  aFD );

    IDE_RC closeAll();

    IDE_RC checkDirectIOArgument( PDL_OFF_T  aWhere,
                                  void*      aBuffer,
                                  size_t     aSize );
    IDE_RC checkDirectIOArgument( PDL_OFF_T      aWhere,
                                  iduFileIOVec&  aVec );
    // File�� Open Flag�� �����Ѵ�.
    SInt getOpenFlag( idBool aIsDirectIO );

    // File IO Stat�� ����� �����Ѵ�.
    static inline void beginFIOStat( idvTime * aTime );
    static void beginFIOStatNA( idvTime * ) { return; }

    // File IO Stat�� ����� �Ϸ��Ѵ�.
    static void endFIOStat( iduFIOStat   * aStatPtr,
                            iduFIOType     aFIOType,
                            idvTime      * aBeginTime );
    static void endFIOStatNA( iduFIOStat * , iduFIOType, idvTime * ){ return; }

public: /* POD class type should make non-static data members as public */
    iduMemoryClientIndex       mIndex;
    SChar                      mFilename[ID_MAX_FILE_NAME];
    /* ���� File�� Direct IO�� ���� ������ ID_TRUE, else ID_FALSE*/
    idBool                     mIsDirectIO;
    /* BUG-31702 file open�ÿ� mode�� �����Ѵ�. */
    SInt                       mPermission;
    /* ���Ϻ� ������� */
    iduFIOStat              mStat;
    iduFIOStatOnOff         mIOStatOnOff;
    static iduFIOStatFunc   mStatFunc[ IDU_FIO_STAT_ONOFF_MAX ];

    /* ���� IO Mutex �� ���� Wait Event ID */
    idvWaitIndex            mWaitEventID;

    /* �� ���Ͽ� Open�� �� �ִ� �ִ� FD ���� */
    UInt                    mMaxFDCount;

    /* �� ���Ͽ� ���� Open�Ǿ� �ִ� FD ���� */
    UInt                    mCurFDCount;

private:
    /* iduFXStackInfo�� ����� Buffer�������� Stack Item���� FD(File
       Descriptor)�� ����ȴ�. �ִ������ �� �ִ� FD ������
       ID_MAX_FILE_DESCRIPTOR_COUNT �̴�. */
    ULong                   mFDStackBuff[ ( IDU_FD_STACK_INFO_SIZE + ID_SIZEOF( ULong )  - 1 ) \
                                          / ID_SIZEOF( ULong ) ];


    /* mFDStackBuff�� ����Ų��. �Ź� Casting�� �����Ƽ� �־����ϴ�. */
    iduFXStackInfo*         mFDStackInfo;

    /* IO ��������� ���ÿ� ���ŵǴ� ���� ���� */
    iduMutex                mIOStatMutex;

    /* FD���� ������ ���� ���ü� ��� ���ؼ� �߰��� */
    iduMutex                mIOOpenMutex;
};

inline IDE_RC iduFile::open( idBool aIsDirectIO, SInt aPermission )
{
    PDL_HANDLE sFD;

    /* �� �Լ��� ó���� �ѹ��� ȣ��Ǿ�� �Ѵ� .*/
    IDE_ASSERT( mCurFDCount == 0 );

    mIsDirectIO = aIsDirectIO;
    mPermission = aPermission;

    /* BUG-15961: DirectIO�� ���� �ʴ� System Property�� �ʿ��� */
    if ( iduProperty::getDirectIOEnabled() == 0 )
    {
        mIsDirectIO = ID_FALSE;
    }

    IDE_TEST( open( &sFD ) != IDE_SUCCESS );

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

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/* ���� �� ���Ͽ� Open�� ��� FD�� Close�Ѵ�. */
inline IDE_RC iduFile::close()
{
    return closeAll();
}

/* aFilename���� �����̸��� �����Ѵ�. */
inline IDE_RC iduFile::setFileName(SChar * aFilename)
{
    IDE_TEST_RAISE( idlOS::strlen( aFilename ) >= ID_MAX_FILE_NAME,
                   too_long_error );

    idlOS::strcpy( mFilename, aFilename );

    return IDE_SUCCESS;

    IDE_EXCEPTION(too_long_error);
    {
        /* BUG-34355 */
        IDE_SET(ideSetErrorCode(idERR_ABORT_idnTooLarge));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/* ���� ũ�⸦ ���´�. */
inline IDE_RC iduFile::getFileSize( ULong *aSize )
{
    PDL_stat   sStat;
    SInt       sState = 0;
    PDL_HANDLE sFD;

    IDE_TEST( allocFD( NULL, &sFD ) != IDE_SUCCESS );
    sState = 1;

    IDE_TEST_RAISE( idf::fstat( sFD, &sStat ) != 0,
                    error_file_fstat );

    sState = 0;
    IDE_TEST( freeFD( sFD ) != IDE_SUCCESS );

    *aSize = (ULong)sStat.st_size;

    return IDE_SUCCESS;

    IDE_EXCEPTION(error_file_fstat);
    {
        IDE_SET(ideSetErrorCode(idERR_FATAL_Sysfstat,
                                        mFilename));
    }
    IDE_EXCEPTION_END;

    if( sState != 0 )
    {
        IDE_ASSERT( freeFD( sFD ) == IDE_SUCCESS );
    }

    return IDE_FAILURE;
}

/* mFilename�� ���� ������ �����ϴ����� �˷��ش�. */
inline idBool iduFile::exist()
{
    if( idf::access( mFilename, F_OK ) == 0 )
    {
        return ID_TRUE;
    }

    return ID_FALSE;
}

/* aFD�� OS���� ��ȯ�Ѵ�. */
inline IDE_RC iduFile::close( PDL_HANDLE aFD )
{
    IDE_ASSERT( aFD != IDL_INVALID_HANDLE );

    IDE_TEST_RAISE( idf::close( aFD ) != 0, close_error );

    return IDE_SUCCESS;

    IDE_EXCEPTION( close_error );
    IDE_EXCEPTION_END;
    {
        IDE_SET( ideSetErrorCode( idERR_FATAL_SysClose,
                                  mFilename ) );
    }
    return IDE_FAILURE;
}

inline void iduFile::dump()
{
    fprintf(stderr, "<FileName: %s Open FD Count: %d>\n",
            mFilename,
            iduFXStack::getItemCnt( mFDStackInfo ) );
}

/***********************************************************************
 * Description : I/O ��������� ���� ����� �����Ѵ�.
 *
 * Related Issue: TASK-2356 DRDB DML ���� �ľ�
 ***********************************************************************/
inline void iduFile::beginFIOStat( idvTime  * aBeginTime )
{
    IDV_TIME_GET( aBeginTime );
}

inline UInt iduFile::getMaxFDCnt()
{
    return mMaxFDCount;
}

inline UInt iduFile::getCurFDCnt()
{
    return mCurFDCount;
}

#endif  // _O_FILE_H_
