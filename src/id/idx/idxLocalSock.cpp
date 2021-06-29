/***********************************************************************
 * Copyright 1999-2013, Altibase Corporation or its subsidiaries.
 * All rights reserved.
 **********************************************************************/

/***********************************************************************
 * $Id: idxLocalSock.cpp 83346 2018-06-26 03:32:55Z kclee $
 **********************************************************************/

#include <idx.h>
#include <idp.h>

SChar* idxLocalSock::mHomePath = NULL;

void idxLocalSock::initializeStatic()
{
    idxLocalSock::mHomePath = idlOS::getenv(IDP_HOME_ENV);
}

void idxLocalSock::destroyStatic()
{
    /* Nothing to do. */
}

IDE_RC idxLocalSock::ping( UInt            aPID,
                           IDX_LOCALSOCK * aSocket,
                           idBool        * aIsAlive )
{
/***********************************************************************
 *
 * Description : Agent Process�� ���� ������ �������� Ȯ���Ѵ�.
 *          
 *       - Named Pipe�� ���μ����� �����ϴ����� �����Ϸ��� �߰� �ڵ��� �ʿ��ѵ�, 
 *         �� ����� �� ũ�� ������ �̹� ������ �ִ� Pipe �ڵ��� �̿��� Peek�� �����Ѵ�.
 *         
 *       - Unix Domain Socket�� kill -0 �� ������ ���μ����� �����ϴ��� �����Ѵ�. 
 *
 ***********************************************************************/
#if defined(ALTI_CFG_OS_WINDOWS)
    BOOL  sIsAliveConn;
    DWORD sErr;
    
    PDL_UNUSED_ARG( aPID );

    sIsAliveConn = PeekNamedPipe( aSocket->mHandle, 
                                  NULL,
                                  1,
                                  NULL,
                                  NULL,
                                  NULL );
                       
    if ( sIsAliveConn == TRUE )
    {
        *aIsAlive = ID_TRUE;
    }
    else
    {
        sErr = GetLastError();
        IDE_TEST( ( sErr != ERROR_BROKEN_PIPE ) && 
                  ( sErr != ERROR_PIPE_NOT_CONNECTED ) );
        
        // if Last Error is BROKEN_PIPE || PIPE_NOT_CONNECTED, then...
        *aIsAlive = ID_FALSE;
    }
#else
    int retval = PDL_OS::kill ( aPID, 0 );

    PDL_UNUSED_ARG( aSocket );
    
    if ( retval == 0 )
    {
        *aIsAlive = ID_TRUE;
    }
    else
    {
        IDE_TEST( errno != ESRCH );
        
        // if Last Error is NO_SUCH_PROCESS, then...
        *aIsAlive = ID_FALSE;
    }
#endif /* ALTI_CFG_OS_WINDOWS */

    return IDE_SUCCESS;
    IDE_EXCEPTION_END;
    return IDE_FAILURE;
}

void idxLocalSock::setPath( SChar         * aBuffer,
                            SChar         * aSockName,
                            idBool          aNeedUniCode )
{
/***********************************************************************
 *
 * Description : Socket ����(Named Pipe ����)�� ��θ� �����Ѵ�.
 *          
 *       - Named Pipe�� ��ΰ� �����Ǿ� �ִ�. (IDX_PIPEPATH ����)
 *         ���⿡ Named Pipe �̸��� �߰��ϸ� ������, 
 *         Unicode ������ ���� HDB/Agent�� �̸� ���� ����� �޸� �Ѵ�.
 *  
 *       - Unix Domain Socket�� $ALTIBASE_HOME/trc/(Socket�̸�)�̴�. 
 *
 ***********************************************************************/
#if defined(ALTI_CFG_OS_WINDOWS)
    UInt    sLen = idlOS::strlen(IDX_PIPEPATH);

    idlOS::strcpy( aBuffer, IDX_PIPEPATH );

    if( aNeedUniCode == ID_TRUE )
    {
        idlOS::memcpy( (aBuffer + sLen),
                       PDL_TEXT(aSockName),
                       idlOS::strlen(aSockName) ); 

        sLen           += idlOS::strlen(aSockName);
        aBuffer[sLen]   = '\0';
        aBuffer[sLen+1] = '\0';
    }
    else
    {
        idlOS::memcpy( (aBuffer + sLen),
                       aSockName,
                       idlOS::strlen(aSockName) ); 

        sLen           += idlOS::strlen(aSockName);
        aBuffer[sLen]   = '\0';
    }
#else

    PDL_UNUSED_ARG( aNeedUniCode );

    // BUG-44652 Socket file path of EXTPROC AGENT could be set by property.
    idlOS::snprintf( aBuffer,
                     IDX_SOCK_PATH_MAXLEN,
                     "%s%c%s",
                     iduProperty::getExtprocAgentSocketFilepath(),
                     IDL_FILE_SEPARATOR,
                     aSockName );
#endif /* ALTI_CFG_OS_WINDOWS */
}

IDE_RC idxLocalSock::initializeSocket( IDX_LOCALSOCK * aSocket,
                                       iduMemory     * aMemory )
{
/***********************************************************************
 *
 * Description : Socket�� �ʱ�ȭ�Ѵ�. (iduMemory)
 *
 *       - Named Pipe�� Overlapped I/O�� ���� Event Handle�� �����Ѵ�.
 *         �� ��, iduMemory�� ������, iduMemory::alloc()�� ȣ���Ѵ�.
 *
 *       - Unix Domain Socket�� Socket ���� �ʱ�ȭ�Ѵ�. 
 *
 ***********************************************************************/
#if defined(ALTI_CFG_OS_WINDOWS)
    iduMemoryStatus sMemoryStatus;
    UInt            sState = 0;

    aSocket->mHandle = PDL_INVALID_HANDLE;

    IDE_TEST( aMemory->getStatus( &sMemoryStatus ) != IDE_SUCCESS );
    IDE_TEST( aMemory->alloc( sizeof(OVERLAPPED), 
                              (void**)&aSocket->mOverlap ) != IDE_SUCCESS );
    sState = 1;
               
    // BUG-39814 offset / offsetHigh �ʱ�ȭ
    aSocket->mOverlap->Offset = 0;
    aSocket->mOverlap->OffsetHigh = 0;
    
    aSocket->mOverlap->hEvent = CreateEvent( 
                                    NULL,    // default security attribute 
                                    TRUE,    // manual-reset event 
                                    TRUE,    // initial state = signaled 
                                    NULL);   // unnamed event object 
    IDE_TEST( aSocket->mOverlap->hEvent == NULL );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    switch( sState )
    {
        case 1:
            (void)aMemory->setStatus( &sMemoryStatus );
            /*@fallthrough*/
        default :
            break;
    }

    return IDE_FAILURE;
#else
    PDL_UNUSED_ARG( aMemory );
    *aSocket = PDL_INVALID_HANDLE;
    return IDE_SUCCESS;
#endif /* ALTI_CFG_OS_WINDOWS */
}

IDE_RC idxLocalSock::initializeSocket( IDX_LOCALSOCK        * aSocket,
                                       iduMemoryClientIndex   aIndex )
{
/***********************************************************************
 *
 * Description : Socket�� �ʱ�ȭ�Ѵ�. (iduMemMgr)
 *
 *       - Named Pipe�� Overlapped I/O�� ���� Event Handle�� �����Ѵ�.
 *         �� ��, iduMemoryClientIndex�� iduMemMgr::malloc()�� ȣ���Ѵ�.
 *
 *       - Unix Domain Socket�� Socket ���� �ʱ�ȭ�Ѵ�. 
 *
 ***********************************************************************/
#if defined(ALTI_CFG_OS_WINDOWS)
    UInt sState = 0;

    aSocket->mHandle = PDL_INVALID_HANDLE;
    IDE_TEST( iduMemMgr::malloc( aIndex,
                                 ID_SIZEOF(OVERLAPPED),
                                 (void**)&aSocket->mOverlap,
                                 IDU_MEM_IMMEDIATE ) != IDE_SUCCESS );
    sState = 1;
    
    // BUG-39814 offset / offsetHigh �ʱ�ȭ
    aSocket->mOverlap->Offset = 0;
    aSocket->mOverlap->OffsetHigh = 0;
    
    aSocket->mOverlap->hEvent = CreateEvent( 
                                    NULL,    // default security attribute 
                                    TRUE,    // manual-reset event 
                                    TRUE,    // initial state = signaled 
                                    NULL);   // unnamed event object 
    IDE_TEST( aSocket->mOverlap->hEvent == NULL );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    switch( sState )
    {
        case 1:
            (void)iduMemMgr::free( aSocket->mOverlap );
            /*@fallthrough*/
        default :
            break;
    }

    return IDE_FAILURE;
#else
    PDL_UNUSED_ARG( aIndex );
    *aSocket = PDL_INVALID_HANDLE;
    return IDE_SUCCESS;
#endif /* ALTI_CFG_OS_WINDOWS */
}

void idxLocalSock::finalizeSocket( IDX_LOCALSOCK * aSock )
{
/***********************************************************************
 *
 * Description : Socket ���ο� �Ҵ�� ������ �����Ѵ�. (iduMemMgr)
 *
 *          - Agent Process�� ���� �� ���� �����ǹǷ�, ȣ������ �ʴ´�.
 *
 ***********************************************************************/
#if defined(ALTI_CFG_OS_WINDOWS)
    (void)iduMemMgr::free( aSock->mOverlap );
#else
    PDL_UNUSED_ARG( aSock );
    // Nothing to do.
#endif
}

IDE_RC idxLocalSock::socket( IDX_LOCALSOCK * aSock )
{
/***********************************************************************
 *
 * Description : Socket ���� �ʱ�ȭ�Ѵ�.
 *
 *       - Named Pipe�� �̹� initializeSocket ���� �ʱ�ȭ�Ǿ���.
 *
 *       - Unix Domain Socket�� socket() �Լ��� ȣ��, �� ���� �޴´�.
 *
 ***********************************************************************/
#if defined(ALTI_CFG_OS_WINDOWS)
    /* Nothing to do. */
#else
    *aSock = idlOS::socket( AF_UNIX, SOCK_STREAM, 0 );
    IDE_TEST( *aSock == PDL_INVALID_HANDLE );
#endif /* ALTI_CFG_OS_WINDOWS */

    return IDE_SUCCESS;
    IDE_EXCEPTION_END;
    return IDE_FAILURE;
}

IDE_RC idxLocalSock::bind( IDX_LOCALSOCK * aSock,
                           SChar         * aPath,
                           iduMemory     * aMemory )
{
/***********************************************************************
 *
 * Description : Socket ��ü�� �� Socket ����(Named Pipe ����)�� �����Ѵ�.
 *
 *       - Named Pipe��, ���� ��ġ�� wchar_t�� ��Ÿ���� �ϹǷ�
 *         ���� Path�� wchar_t�� ��ȯ�� Named Pipe�� �����Ѵ�.
 *
 *       - Unix Domain Socket�� ���� ��ġ�� ������ bind()�� ȣ���Ѵ�.
 *         ���� ������ unlink()�� ������� ���ο� ���� ������ �����ȴ�.
 *         �� ����, ���� �ɼ��� REUSEADDR�� �ּ� ������ �̹� Binding
 *         �Ǿ� �ִٰ� �ص� ������ �����ϵ��� �����Ѵ�.
 *
 ***********************************************************************/
#if defined(ALTI_CFG_OS_WINDOWS)
    wchar_t         * sWStr;
    iduMemoryStatus   sMemoryStatus;
    UInt              sState = 0;
    SInt              sLen = idlOS::strlen( aPath ) + 1;

    IDE_TEST( aMemory->getStatus( &sMemoryStatus ) != IDE_SUCCESS );
    IDE_TEST( aMemory->alloc( ( ID_SIZEOF(wchar_t) * sLen ),
                              (void **)&sWStr ) 
              != IDE_SUCCESS );
    sState = 1;
    
    // char > wchar_t
    mbstowcs( sWStr, aPath, sLen );
    
    aSock->mHandle = CreateNamedPipeW(
        sWStr,                      // name of the pipe
        PIPE_ACCESS_DUPLEX |        // 2-way pipe
        FILE_FLAG_OVERLAPPED,       // overlapped mode
        PIPE_TYPE_BYTE |            // send data as a byte stream
        PIPE_WAIT,                  // blocking mode
        1,                          // only allow 1 instance of this pipe
        0,                          // no outbound buffer
        0,                          // no inbound buffer
        0,                          // use default wait time
        NULL                        // use default security attributes
    );
    
    IDE_TEST( aSock->mHandle == 0 || aSock->mHandle == INVALID_HANDLE_VALUE )

    sState = 0;
    IDE_TEST( aMemory->setStatus( &sMemoryStatus ) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    switch( sState )
    {
        case 1:
            (void)aMemory->setStatus( &sMemoryStatus );
            /*@fallthrough*/
        default :
            break;
    }

    return IDE_FAILURE;
#else
    struct sockaddr_un sAddr;
    SInt               sOption;
    
    PDL_UNUSED_ARG( aMemory );

    // Socket is already connected.
    IDE_TEST( *aSock == PDL_INVALID_HANDLE );

    // Add socket option : SO_REUSEADDR
    sOption = 1;
    IDE_TEST( idlOS::setsockopt( *aSock,
                                 SOL_SOCKET,
                                 SO_REUSEADDR,
                                 (SChar *)&sOption,
                                 ID_SIZEOF(sOption) ) < 0 );

    idlOS::memset( &sAddr, 0, ID_SIZEOF( sAddr ) );
    sAddr.sun_family  = AF_UNIX;
    idlOS::strcpy( (SChar*)sAddr.sun_path, aPath );

    // unlink socket file before binding
    idlOS::unlink( sAddr.sun_path );

    IDE_TEST( idlOS::bind( *aSock,
                           (struct sockaddr *)&sAddr,
                           ID_SIZEOF(sAddr) ) != 0 );
    return IDE_SUCCESS;
    IDE_EXCEPTION_END;
    return IDE_FAILURE;
#endif /* ALTI_CFG_OS_WINDOWS */
}

IDE_RC idxLocalSock::listen( IDX_LOCALSOCK aSock )
{
/***********************************************************************
 *
 * Description : Socket ������ ��ٸ���.
 *
 *       - Named Pipe�� accept()���� ������ ��ٸ���.
 *
 *       - Unix Domain Socket�� 
 *         listen()���� ���� ��ȣ�� �� �� ���� Blocking �ȴ�.
 *
 ***********************************************************************/
#if defined(ALTI_CFG_OS_WINDOWS)
    /* Nothing to do. */
#else
    IDE_TEST( idlOS::listen( aSock, IDX_LOCALSOCK_BACKLOG ) < 0 );
#endif /* ALTI_CFG_OS_WINDOWS */

    return IDE_SUCCESS;
    IDE_EXCEPTION_END;
    return IDE_FAILURE;
}

IDE_RC idxLocalSock::accept( IDX_LOCALSOCK   aServSock,
                             IDX_LOCALSOCK * aClntSock,
                             PDL_Time_Value  aWaitTime,  
                             idBool        * aIsTimeout )
{
/***********************************************************************
 *
 * Description : Socket ������ �����Ѵ�.
 *
 *       - Named Pipe�� ���� �̺�Ʈ�� Overlapped I/O�� ����� ��,
 *         ���� �̺�Ʈ�� �Ͼ�� ����� �������� ��ȯ�Ѵ�.
 *
 *       - Unix Domain Socket�� listen()���� ���� ��ȣ�� �޾����Ƿ�
 *         accept()�� ȣ���ϰ� �ȴ�.
 *
 ***********************************************************************/
#if defined(ALTI_CFG_OS_WINDOWS)
    IDE_RC  sRC = IDE_SUCCESS;
    
    BOOL    sPending;
    BOOL    sResult;
    DWORD   sWait;
    DWORD   sTransferred;
    DWORD   sWaitMsec;
    
    /* Non-blocking mode������ ����� 0�ε��� ������ ���� (����) ������ �߻��� �� �ִ�.
     *
     * - ERROR_IO_PENDING : ������ ������ Ŭ���̾�Ʈ�� ���� �߰ߵ��� �ʾҴ�.
     *                      �׷��� PENDING�� true�� �ΰ� IDLE_TIMEOUT ���� ��ٷ� ����.
     * - ERROR_PIPE_CONNECTED : ������, ���� ������ Ŭ���̾�Ʈ�� �����Ѵ�.
     *                          PENDING �� �ʿ� ���� ������ �����Ѵ�.
     *
     * ����� 0�� �ƴ� ��쿡�� ���� ��Ȳ�̸�, �� ��� accept() �Լ��� ���з� ������.
     */

    sWaitMsec = (DWORD)( aWaitTime.sec() * 1000 );
    *aIsTimeout = ID_FALSE;
    
    sResult = ConnectNamedPipe( aServSock.mHandle, aServSock.mOverlap );
    IDE_TEST( sResult != 0 );
    
    switch (GetLastError()) 
    { 
        // The overlapped connection in progress. 
        case ERROR_IO_PENDING: 
            sPending = ID_TRUE; 
            break; 
        // Client is already connected
        case ERROR_PIPE_CONNECTED: 
            sPending = ID_FALSE; 
            break; 
        // If an error occurs during the connect operation
        default: 
            IDE_RAISE(IDE_EXCEPTION_END_LABEL);
            break;
    }
    
    // in connection mode, we don't have to put while loop in the code.
    if( sPending == ID_TRUE )
    {
        // waits IDLE_TIMEOUT second
        sWait = WaitForSingleObject( aServSock.mOverlap->hEvent, sWaitMsec );        

        if( sWait == WAIT_TIMEOUT )
        {
            // Connection timeout
            *aIsTimeout = ID_TRUE;
        }
        else
        {
            sResult = GetOverlappedResult( 
                            aServSock.mHandle,      // pipe handle
                            aServSock.mOverlap,     // OVERLAPPED structure 
                            &sTransferred,          // bytes transferred 
                            FALSE );                // do not wait
                            
            // if it is still pending, it has an error.
            IDE_TEST( sResult != TRUE )
            // set established handle to client handle
            aClntSock->mHandle = aServSock.mHandle;
        }
    }
    else
    {
        // set established handle to client handle
        aClntSock->mHandle = aServSock.mHandle;
    }
#else
    struct sockaddr_un    sClntAddr;
    SInt                  sAddrLen;

    PDL_UNUSED_ARG( aWaitTime );
    PDL_UNUSED_ARG( aIsTimeout );

    sAddrLen  = ID_SIZEOF( sClntAddr );
    *aClntSock = idlOS::accept(
                         aServSock,
                         (struct sockaddr *)&sClntAddr,
                         &sAddrLen );

    IDE_TEST( *aClntSock == PDL_INVALID_HANDLE );
#endif /* ALTI_CFG_OS_WINDOWS */

    return IDE_SUCCESS;
    IDE_EXCEPTION_END;
    return IDE_FAILURE;
}

IDE_RC idxLocalSock::connect( IDX_LOCALSOCK * aSock,
                              SChar         * aPath,
                              iduMemory     * aMemory,
                              idBool        * aIsRefused )
{
/***********************************************************************
 *
 * Description : Socket ������ �õ��Ѵ�.
 *
 *       - Named Pipe��, ���� ��ġ�� wchar_t�� ��Ÿ���� �ϹǷ�
 *         ���� Path�� wchar_t�� ��ȯ�� Named Pipe�� �����.
 *
 *       - Unix Domain Socket�� connect()�� ȣ���� ������ �õ��Ѵ�.
 *
 ***********************************************************************/
#if defined(ALTI_CFG_OS_WINDOWS)
    wchar_t         * sWStr;
    iduMemoryStatus   sMemoryStatus;
    UInt              sState = 0;
    SInt              sLen = idlOS::strlen( aPath ) + 1;
    
    IDE_TEST( aMemory->getStatus( &sMemoryStatus ) != IDE_SUCCESS );
    IDE_TEST( aMemory->alloc( ( ID_SIZEOF(wchar_t) * sLen ),
                              (void **)&sWStr ) 
              != IDE_SUCCESS );
    sState = 1;
    
    // char > wchar_t
    mbstowcs( sWStr, aPath, sLen );

    aSock->mHandle = CreateFileW( 
                           sWStr,                               // location
                           GENERIC_WRITE | GENERIC_READ,        // only write
                           FILE_SHARE_READ | FILE_SHARE_WRITE,  // file flags
                           NULL,
                           OPEN_EXISTING,
                           FILE_ATTRIBUTE_NORMAL,
                           NULL );
                           
    IDE_TEST( aSock->mHandle == 0 || aSock->mHandle == INVALID_HANDLE_VALUE )

    sState = 0;
    IDE_TEST( aMemory->setStatus( &sMemoryStatus ) != IDE_SUCCESS );

    // BUG-37957
    // Windows platform doesn't care about ECONNREFUSED
    *aIsRefused = ID_FALSE;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    switch( sState )
    {
        case 1:
            (void)aMemory->setStatus( &sMemoryStatus );
            /*@fallthrough*/
        default :
            break;
    }
    return IDE_FAILURE;
#else
    struct sockaddr_un sAddr;
    SInt   sSockRet = 0;

    PDL_UNUSED_ARG( aMemory );

    idlOS::memset( &sAddr, 0, ID_SIZEOF( sAddr ) );
    sAddr.sun_family  = AF_UNIX;
    idlOS::strncpy( (SChar*)sAddr.sun_path, aPath, ID_SIZEOF(sAddr.sun_path) - 1 ); 
    sAddr.sun_path[ID_SIZEOF(sAddr.sun_path) - 1] = 0; 

    sSockRet = idlOS::connect( *aSock,
                               (struct sockaddr *)&sAddr,
                               ID_SIZEOF(sAddr) );
    IDE_TEST( sSockRet < 0 );

    *aIsRefused = ID_FALSE;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    /* BUG-37957
     * ECONNREFUSED�� �߻��ϸ�, ������ ������ؾ� �Ѵ�.
     */
    if( errno == ECONNREFUSED )
    {
        *aIsRefused = ID_TRUE;
    }
    else
    {
        *aIsRefused = ID_FALSE;
    }

    return IDE_FAILURE;
#endif /* ALTI_CFG_OS_WINDOWS */
}

IDE_RC idxLocalSock::select( IDX_LOCALSOCK    aSock,
                             PDL_Time_Value * aWaitTime,
                             idBool         * aIsTimeout )
{
/***********************************************************************
 *
 * Description : Socket�� ���¸� ����Ѵ�.
 *
 *       - Named Pipe��, Overlapped I/O�� recv(), send()�� ����ϹǷ�
 *         ���⼭�� �ƹ��� �ϵ� ���� �ʴ´�.
 *
 *       - Unix Domain Socket�� select()�� ���� ���� ��ũ������
 *         ���¸� �����Ѵ�. �ش� ������ ���� �� �ִ� ���·� ��ȯ�Ǹ� 
 *         select()�� Blocking�� Ǯ����, �� �ܿ��� IDLE_TIMEOUT����
 *         ����ϴٰ� Blocking�� Ǭ�� (TIMEOUT)
 *
 ***********************************************************************/
#if defined(ALTI_CFG_OS_WINDOWS)
    /* Nothing to do. */
#else
    SInt    sNum = 0;
    fd_set  sSocketFd;
    
    *aIsTimeout = ID_FALSE;

    FD_ZERO( &sSocketFd );
    FD_SET( aSock, &sSocketFd );

    sNum = idlOS::select( aSock + 1, &sSocketFd, NULL, NULL, aWaitTime );
    IDE_TEST( sNum < 0 );
    
    if( sNum == 0 )
    {
        *aIsTimeout = ID_TRUE;
    }
    else
    {
        /* Nothing to do : socket is signaled */
    }
#endif /* ALTI_CFG_OS_WINDOWS */

    return IDE_SUCCESS;
    IDE_EXCEPTION_END;
    return IDE_FAILURE;
}

IDE_RC idxLocalSock::send( IDX_LOCALSOCK   aSock,
                           void          * aBuffer,
                           UInt            aBufferSize )
{
/***********************************************************************
 *
 * Description : Blocking Mode Message Send
 *
 *       - Named Pipe��, Pipe ���Ͽ� �޽����� �ۼ��Ѵ�.
 *
 *       - Unix Domain Socket�� send() �Լ��� ȣ���Ѵ�.
 *
 ***********************************************************************/
#if defined(ALTI_CFG_OS_WINDOWS)
    DWORD numByteWritten;
    BOOL result;

    IDE_TEST( aBufferSize <= 0 );
    result = WriteFile( aSock.mHandle, aBuffer, aBufferSize, &numByteWritten, NULL );
    IDE_TEST( !result ); 
    IDE_TEST( numByteWritten != (DWORD)aBufferSize );
#else
    SInt sSockRet = 0;

    IDE_TEST( aBufferSize <= 0 );
    sSockRet = idlVA::send_i( aSock, aBuffer, aBufferSize );
    IDE_TEST( sSockRet <= 0 );
    IDE_TEST( (UInt)sSockRet != aBufferSize );
#endif /* ALTI_CFG_OS_WINDOWS */
    return IDE_SUCCESS;
    IDE_EXCEPTION_END;
    return IDE_FAILURE;
}

IDE_RC idxLocalSock::sendTimedWait( IDX_LOCALSOCK   aSock,
                                    void          * aBuffer,
                                    UInt            aBufferSize,
                                    PDL_Time_Value  aWaitTime,
                                    idBool        * aIsTimeout )
{
/***********************************************************************
 *
 * Description : Non-Blocking Mode Message Send
 *
 *       - Named Pipe��, �޽����� ���� ���� �� ���� Overlapped I/O�� ���,
 *         �޽����� ���� �� ���� �� ���� ��ٷȴٰ� �޽����� ��� ������.
 *         ������ �޽����� ���� �� ���� ���, Timeout Error�� ��ȯ�Ѵ�.
 *
 *       - Unix Domain Socket�� send() �Լ��� �����ϴ�.
 *
 ***********************************************************************/
#if defined(ALTI_CFG_OS_WINDOWS)
    /* Send with Overlapped I/O-based Asynchronous Communication */
    IDE_RC sRC = IDE_SUCCESS;

    DWORD sWaitMsec;
    DWORD numByteWritten = 0;
    DWORD sErr;
    DWORD sWait;
    DWORD sTransferred;
    BOOL  sResult;
    BOOL  sPending;
    
    sWaitMsec = (DWORD)( aWaitTime.sec() * 1000 );
    *aIsTimeout = ID_FALSE;
    
    sResult = WriteFile( 
                    aSock.mHandle, 
                    aBuffer, 
                    aBufferSize, 
                    &numByteWritten, 
                    aSock.mOverlap );
                    
    if( ( sResult == TRUE ) && ( numByteWritten == aBufferSize ) )
    {
        sPending = FALSE; 
    }
    else if ( ( sResult == TRUE ) && ( numByteWritten < aBufferSize ) )
    {
        sPending = TRUE; 
    }
    else
    {
        sErr = GetLastError(); 
        IDE_TEST( ( sResult != FALSE ) || ( sErr != ERROR_IO_PENDING ) ) ;
        sPending = TRUE; 
    }
    
    if( sPending == TRUE )
    {
        while( numByteWritten < aBufferSize )
        {
            sWait = WaitForSingleObject( aSock.mOverlap->hEvent, sWaitMsec );        
            sErr = GetLastError();
            IDE_TEST( sErr == ERROR_BROKEN_PIPE ); 
                        
            if( sWait == WAIT_TIMEOUT )
            {
                *aIsTimeout = ID_TRUE;
                break;
            }
            else
            {
                sResult = GetOverlappedResult( 
                            aSock.mHandle,      // pipe handle
                            aSock.mOverlap,     // OVERLAPPED structure 
                            &sTransferred,      // bytes transferred 
                            FALSE );            // do not wait
                        
                // if (pending condition)
                if( ( sResult == FALSE ) || sTransferred == 0 ) 
                { 
                    continue;
                }
                else
                {
                    numByteWritten += sTransferred;
                }
            }
        }
    }
    else
    {
        /* Nothing to do. */
    }
#else
    /* Same as send() */
    PDL_UNUSED_ARG( aWaitTime  );
    PDL_UNUSED_ARG( aIsTimeout );
    IDE_TEST( send( aSock, aBuffer, aBufferSize ) != IDE_SUCCESS );
#endif /* ALTI_CFG_OS_WINDOWS */
    
    return IDE_SUCCESS;
    IDE_EXCEPTION_END;
    return IDE_FAILURE;
}

IDE_RC idxLocalSock::recv( IDX_LOCALSOCK   aSock,
                           void          * aBuffer,
                           UInt            aBufferSize,
                           UInt          * aReceivedSize )
{
/***********************************************************************
 *
 * Description : Blocking Mode Message Receive 
 *
 *       - Named Pipe��, Pipe ���Ͽ� ������ �޽����� �д´�.
 *
 *       - Unix Domain Socket�� recv() �Լ��� ȣ���Ѵ�.
 *
 ***********************************************************************/
#if defined(ALTI_CFG_OS_WINDOWS)
    DWORD numByteRead;
    BOOL result;

    IDE_TEST( aBufferSize <= 0 );
    result = ReadFile( aSock.mHandle, aBuffer, aBufferSize, &numByteRead, NULL );
    IDE_TEST( !result ); 
    *aReceivedSize = numByteRead;
#else
    SInt           sSockRet       = 0;
    UInt           sTotalRecvSize = 0;
    SChar *        sBuffer        = NULL; 
    PDL_Time_Value sWaitTime;
    idBool         sIsTimeout     = ID_FALSE;

    /* BUG-39814
     * recv()��, ���۵Ǵ� ������ �� ���� �� ���� ��ٸ��� �ʴ´�.
     * ���� recv()�� ���ϴ� ���̺��� ���� ������ ������ �޾Ҵٸ�
     * �󸶰��� �ð� ���� �� ���� �����Ͱ� �������� ������ ���� �ٽ� recv() �ؾ� �Ѵ�.
     * �� �ð� ���� �����Ͱ� �� �̻� ������ �ʴ´ٸ�, recv()�� �����Ѵ�.
     */

    IDE_TEST( aBufferSize <= 0 );

    sWaitTime.set( IDX_LOCALSOCK_RECV_WAITSEC, 0 );
    *aReceivedSize  = 0;
    sBuffer         = (SChar*)aBuffer;
    
    // ù recv_i()�� ȣ���� ����.
    sSockRet = idlVA::recv_i( aSock, 
                              (void*)sBuffer, 
                              aBufferSize );
    
    IDE_TEST( sSockRet < 0 );
    sTotalRecvSize += (UInt)sSockRet;

    // �޾ƾ� �� ��ŭ ���� ���ߴٸ�, ��� �޴´�.
    while ( sTotalRecvSize < aBufferSize )
    {
        IDE_TEST( select( aSock, &sWaitTime, &sIsTimeout ) );

        if ( sIsTimeout == ID_TRUE )
        {
            break;
        }
        else
        {
            // Nothing to do.
        }

        /* recv_i()�� ��ȯ�� (Blocking-mode ���� �������)
         * 
         *  return > 0  : ������ ���� �����Ͱ� �����ؼ�, �ɷ²� �о�� ���
         *  return = 0  : select() �� '���� �� �ִ�' �� ������ 
         *                ��� end-of-file�̶� �׳� ����Ǵ� ���
         *  return = -1 : ���� ��Ȳ (�дٰ� ������ ����ų�, �ٸ� ������)
         */
        sSockRet = idlVA::recv_i( aSock, 
                                  (void*)(sBuffer + sTotalRecvSize), 
                                  (aBufferSize - sTotalRecvSize) );
    
        IDE_TEST( sSockRet < 0 );
        
        if ( sSockRet == 0 )
        {
            // end-of-file. �� ������ �������� recv_i() �� ȣ��� ��Ȳ
            break;
        }
        else
        {
            // recv_i() �� ���������� �����͸� ���� ��Ȳ
            sTotalRecvSize += (UInt)sSockRet;
        }
    }

    *aReceivedSize = sTotalRecvSize;
#endif /* ALTI_CFG_OS_WINDOWS */

    return IDE_SUCCESS;
    IDE_EXCEPTION_END;
    return IDE_FAILURE;
}

IDE_RC idxLocalSock::recvTimedWait( IDX_LOCALSOCK   aSock,
                                    void          * aBuffer,
                                    UInt            aBufferSize,
                                    PDL_Time_Value  aWaitTime,
                                    UInt          * aReceivedSize,
                                    idBool        * aIsTimeout )
{
/***********************************************************************
 *
 * Description : Non-Blocking Mode Message Receive 
 *
 *       - Named Pipe��, �޽����� ���� ���� �� ���� Overlapped I/O�� ���,
 *         �޽����� ���� �� ���� �� ���� ��ٷȴٰ� �޽����� ��� �޴´�.
 *         ������ �޽����� ���� �ǻ簡 ���� ���, Timeout�� ��ȯ�Ѵ�.
 *
 *       - Unix Domain Socket�� recv() �Լ��� �����ϴ�.
 *
 ***********************************************************************/
#if defined(ALTI_CFG_OS_WINDOWS)
    /* Recv with Overlapped I/O-based Asynchronous Communication */
    IDE_RC  sRC = IDE_SUCCESS;
    
    DWORD sWaitMsec;
    DWORD numByteRead = 0;
    DWORD sErr;
    DWORD sWait;
    DWORD sTransferred;
    BOOL  sResult;
    BOOL  sPending;

    *aReceivedSize = 0;
    sWaitMsec = (DWORD)( aWaitTime.sec() * 1000 );
    *aIsTimeout = ID_FALSE;
    
    sResult = ReadFile( 
                    aSock.mHandle, 
                    aBuffer, 
                    aBufferSize, 
                    &numByteRead, 
                    aSock.mOverlap );
                    
    if( ( sResult == TRUE ) && ( numByteRead == aBufferSize ) )
    {
        sPending = FALSE; 
    }
    else if ( ( sResult == TRUE ) && ( numByteRead < aBufferSize ) )
    {
        sPending = TRUE;
    }
    else
    {
        sErr = GetLastError(); 
        
        if( ( sResult == FALSE ) && ( sErr == ERROR_IO_PENDING ) )
        {
            sPending = TRUE;
        }
        else
        {
            IDE_TEST( sErr != ERROR_BROKEN_PIPE );
            
            // sErr should be ERROR_BROKEN_PIPE
            // return SUCCESS but set numByteRead = 0
            sPending = FALSE;
        }
    }
    
    if( sPending == TRUE )
    {
        while( numByteRead < aBufferSize )
        {
            sWait = WaitForSingleObject( aSock.mOverlap->hEvent, sWaitMsec );        
            sErr = GetLastError();
            
            // sErr can be BROKEN_PIPE
            if( sErr == ERROR_BROKEN_PIPE )
            {
                break;
            }
            else
            {
                // Nothing to do..
            }

            if( sWait == WAIT_TIMEOUT )
            {
                *aIsTimeout = ID_TRUE;
                break;
            }
            else
            {
                sResult = GetOverlappedResult( 
                            aSock.mHandle,      // pipe handle
                            aSock.mOverlap,     // OVERLAPPED structure 
                            &sTransferred,      // bytes transferred 
                            FALSE );            // do not wait
                        
                // if (pending condition)
                if( ( sResult == FALSE ) || sTransferred == 0 ) 
                {
                    continue;
                }
                else
                {
                    numByteRead += sTransferred;
                }
            }
        }
    }
    else
    {
        /* Nothing to do. */
    }

    *aReceivedSize = numByteRead;
#else
    /* Same as recv() */
    PDL_UNUSED_ARG( aWaitTime  );
    PDL_UNUSED_ARG( aIsTimeout );
    IDE_TEST( recv( aSock, aBuffer, aBufferSize, aReceivedSize ) != IDE_SUCCESS );
#endif /* ALTI_CFG_OS_WINDOWS */
    
    return IDE_SUCCESS;
    IDE_EXCEPTION_END;
    return IDE_FAILURE;
}

IDE_RC idxLocalSock::setBlockMode( IDX_LOCALSOCK aSock )
{
/***********************************************************************
 *
 * Description : ������ Blocking Mode�� ��ȯ�Ѵ�.
 *               [PROJ-1685] Extproc Execution���� Blocking Mode�θ� ����
 *
 *       - Named Pipe�� ��� ū �ǹ̰� ���µ�, Overlapped I/O��
 *         Blocking Mode�ʹ� �����ϰ� �۵��ϱ� �����̴�.
 *
 *       - Unix Domain Socket�� setBlock()�� ȣ���Ѵ�.
 *
 ***********************************************************************/
#if defined(ALTI_CFG_OS_WINDOWS)
    DWORD dwMode = PIPE_WAIT;
    BOOL  result = SetNamedPipeHandleState( aSock.mHandle, &dwMode, NULL, NULL );
    IDE_TEST( !result );
#else
    IDE_TEST( idlVA::setBlock( aSock ) != IDE_SUCCESS );
#endif /* ALTI_CFG_OS_WINDOWS */

    return IDE_SUCCESS;
    IDE_EXCEPTION_END;
    return IDE_FAILURE;
}

IDE_RC idxLocalSock::setNonBlockMode( IDX_LOCALSOCK aSock )
{
/***********************************************************************
 *
 * Description : ������ Non-Blocking Mode�� ��ȯ�Ѵ�.
 *
 ***********************************************************************/
#if defined(ALTI_CFG_OS_WINDOWS)
    DWORD dwMode = PIPE_NOWAIT;
    BOOL  result = SetNamedPipeHandleState( aSock.mHandle, &dwMode, NULL, NULL );
    IDE_TEST( !result );
#else
    IDE_TEST( idlVA::setNonBlock( aSock ) != IDE_SUCCESS );
#endif /* ALTI_CFG_OS_WINDOWS */

    return IDE_SUCCESS;
    IDE_EXCEPTION_END;
    return IDE_FAILURE;
}

IDE_RC idxLocalSock::close( IDX_LOCALSOCK *aSock )
{
/***********************************************************************
 *
 * Description : ������ �ݴ´�.
 *
 *       - Named Pipe�� ���� ������ �ִ� Pipe ��ü�� ����/Ŭ���̾�Ʈ
 *         ������ ����. close() �Լ��� Pipe ��ü�� ����ϴ� �Լ��̴�.
 *
 *       - Unix Domain Socket�� ���� ������ �ִ� Socket ��ü�� �ݴ´�.
 *         ��, Client�� close()�� �ϴ��� Server Socket�� ����ִ�.
 *
 ***********************************************************************/
#if defined(ALTI_CFG_OS_WINDOWS)
    if( aSock->mHandle != PDL_INVALID_HANDLE )
    {
        CloseHandle( aSock->mHandle );
        aSock->mHandle = PDL_INVALID_HANDLE;
    }
    else
    {
        // Nothing to do.
    }
#else
    if( *aSock != PDL_INVALID_HANDLE )
    {
        IDE_TEST( idlOS::closesocket( *aSock ) != IDE_SUCCESS );
        *aSock = PDL_INVALID_HANDLE;
    }
    else
    {
        // Nothing to do.
    }
#endif /* ALTI_CFG_OS_WINDOWS */

    return IDE_SUCCESS;
    IDE_EXCEPTION_END;
    return IDE_FAILURE;
}

IDE_RC idxLocalSock::disconnect( IDX_LOCALSOCK * aSock )
{
/***********************************************************************
 *
 * Description : Ŭ���̾�Ʈ �� ������ �ݴ´�.
 *
 *       - Named Pipe�� ���� ������ �ִ� Pipe ��ü�� ����/Ŭ���̾�Ʈ
 *         ������ ����. disconnect() �Լ��� ���ӵ� Ŭ���̾�Ʈ ���Ḹ ���´�.
 *
 *       - Unix Domain Socket�� ���� ������ �ִ� Socket ��ü�� �ݴ´�.
 *         ��ǻ�, close() �Լ��� �����ϴ�.
 *
 ***********************************************************************/
#if defined(ALTI_CFG_OS_WINDOWS)
    BOOL  result;

    if( aSock->mHandle != PDL_INVALID_HANDLE )
    {
        result = DisconnectNamedPipe( aSock->mHandle );
        IDE_TEST( !result );
        aSock->mHandle = PDL_INVALID_HANDLE;
    }
    else
    {
        // Nothing to do.
    }
#else
    if( *aSock != PDL_INVALID_HANDLE )
    {
        IDE_TEST( idlOS::closesocket( *aSock ) != IDE_SUCCESS );
        *aSock = PDL_INVALID_HANDLE;
    }
    else
    {
        // Nothing to do.
    }
#endif /* ALTI_CFG_OS_WINDOWS */

    return IDE_SUCCESS;
    IDE_EXCEPTION_END;
    return IDE_FAILURE;
}
