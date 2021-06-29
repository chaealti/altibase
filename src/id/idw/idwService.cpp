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
 
#include <ide.h>
#include <idwService.h>
#include <idtContainer.h>


//Static Variable Initialize

#if defined(VC_WIN32) && !defined(VC_WINCE)
    SERVICE_STATUS_HANDLE   idwService::mHService      = NULL;
    SInt                    idwService::mServiceStatus = 0;
#endif  

    char*                   idwService::serviceName    = NULL;
    startFunction           idwService::mWinStart      = NULL;
    runFunction             idwService::mWinRun        = NULL;
    stopFunction            idwService::mWinStop       = NULL;


/*---------------------------------------------------------------
 * PROJ-1699
 *
 * Name : serviceStart()
 *
 * Description :
 * 1. ����� �������� ���� �̸��� �о�´�.
 * 2. SCM�� ��ϵ� ���񽺿��� Start/Stop ���� ����� ���� �� �ֵ���
 *    Dispatcher�� ����ϰ� ���� Altibase�� �����ϱ����� 
 *    serviceMain() �� ȣ���Ѵ�.
---------------------------------------------------------------*/

IDE_RC idwService::serviceStart()
{
#if defined(PDL_WIN32) && !defined(VC_WINCE)
    
    if ( (serviceName = idlOS::getenv(ALTIBASE_ENV_PREFIX"SERVICE")) == NULL )
    {
        //BUGBUG - �̺�Ʈ�α� ���ܾ� ��
        //PS. ȯ�� ������ �о�� �� ����.
        return IDE_FAILURE;   
    }

	SERVICE_TABLE_ENTRY srvTE[]={
		{ serviceName, (LPSERVICE_MAIN_FUNCTION)serviceMain }, 
	    { NULL, NULL }
	};
	
	IDE_TEST( StartServiceCtrlDispatcher(srvTE) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
#else
    // UNIX,LINUX OTHER platform
    return IDE_FAILURE;
#endif  

}


/*---------------------------------------------------------------
 * PROJ-1699
 *
 * Name : serviceMain()
 *
 * Description :
 * Dispatcher���� ������ ���(Start/Stop)�� ó���ϱ� ���� Handler��
 * ����ϰ�, Altibase �� �����Ѵ� (iSQL���� ����� ������ �ʿ�����
 * �ʵ��� ó���Ǿ���).
 *
---------------------------------------------------------------*/ 

IDE_RC idwService::serviceMain( SInt   /*argc*/,
                                SChar */*argv*/ )
{
#if defined(PDL_WIN32) && !defined(VC_WINCE)

    UInt     sOptionFlag      = 0;

	//ServiceHandle�� �����.
	mHService = RegisterServiceCtrlHandler( serviceName, 
	                                        (LPHANDLER_FUNCTION)serviceHandler );
	
    if (mHService == 0)
    {
        //BUGBUG : ���� �޽����� �̺�Ʈ �α׷� ���ܾ� ��		
        //GetLastError�� ����Ͽ� ERROR_INVALID_NAME, 
        //ERROR_SERVICE_DOES_NOT_EXIST�� �Ǻ��ѵ� �̺�Ʈ �α׷� ����        
        return IDE_FAILURE;
    }

    /* BUG-47527 */
    IDE_TEST( idtContainer::initializeStatic(IDU_SERVER_TYPE) != IDE_SUCCESS );

    //Altibase Startup 
	setStatus( SERVICE_START_PENDING );
    IDE_TEST( idwService::mWinStart() != IDE_SUCCESS );
    
    //Listener�� ����
    setStatus( SERVICE_RUNNING );
    IDE_TEST( idwService::mWinRun() != IDE_SUCCESS );    

	setStatus( SERVICE_STOPPED );	
	
    IDE_TEST( idtContainer::destroyStatic() != IDE_SUCCESS );

	CloseHandle( mHService );  
	
	return IDE_SUCCESS;  
	
	IDE_EXCEPTION_END;
    {
        ideLog::logErrorMsg( IDE_SERVER_0 );
    }

    return IDE_FAILURE;
   
#else
    // UNIX,LINUX OTHER platform
    return IDE_FAILURE;
#endif  

}

/*---------------------------------------------------------------
 * PROJ-1699
 *
 * Name : serviceHandler(..)
 * Argument : opCode = ���� ���(START/STOP)
 * Description :
 * Dispatcher���� ���޵� ����� ������ ó���ϴ� �Լ��̴�.
 * SCM���� STOP ��� �Ǵ� �ý��� ����� ���޵Ǵ� SHUTDOWN �����
 * �޾Ƽ�, serviceStop �Լ��� ȣ���Ѵ�.
 * �� ����� ó���� �Ŀ� SCM�� ���� ���� ���¸� �����Ѵ�.
---------------------------------------------------------------*/

#if defined(PDL_WIN32) && !defined(VC_WINCE)
void idwService::serviceHandler( SInt opCode )
#else
void idwService::serviceHandler( SInt /*opCode*/ )
#endif
{
#if defined(PDL_WIN32) && !defined(VC_WINCE)

  	if ( opCode == mServiceStatus )
  	{
		return ;
	}
	
	switch ( opCode )
	{
	case SERVICE_CONTROL_SHUTDOWN:            //SYSTEM SHUTDOWN
	case SERVICE_CONTROL_STOP:
		setStatus( SERVICE_STOP_PENDING );
		IDE_TEST( idwService::mWinStop() != IDE_SUCCESS );
		break;
	case SERVICE_CONTROL_INTERROGATE:
	default:
		setStatus(mServiceStatus);
		break;
	}
	
    IDE_EXCEPTION_END;
    {
        //BUGBUG - �̺�Ʈ�α׷� ���� ������ ���ܾ� ��
        ideLog::logErrorMsg(IDE_SERVER_0);
    }
#else
    // UNIX,LINUX OTHER platform
#endif 
     
}

/*---------------------------------------------------------------
 * PROJ-1699
 *
 * Name : setStatus(..)
 * Argument : dwState = ���� ����
 * Description :
 * SCM�� ���� ���� �Ѵ�.
 * 
---------------------------------------------------------------*/

#if defined(PDL_WIN32) && !defined(VC_WINCE)
void idwService::setStatus( SInt dwState )
#else
void idwService::setStatus( SInt /*dwState*/ )
#endif    
{
#if defined(PDL_WIN32) && !defined(VC_WINCE)

    SERVICE_STATUS srvStatus;
    
	srvStatus.dwServiceType             = SERVICE_WIN32_OWN_PROCESS;
	srvStatus.dwCurrentState            = dwState;
	srvStatus.dwControlsAccepted        = SERVICE_ACCEPT_STOP;
	srvStatus.dwWin32ExitCode           = 0;
	srvStatus.dwServiceSpecificExitCode = 0;
	srvStatus.dwCheckPoint              = 0;
	srvStatus.dwWaitHint                = 0;

	mServiceStatus                      = dwState;
	
	SetServiceStatus( mHService, &srvStatus );

#else
    // UNIX,LINUX OTHER platform

#endif 
   
}

