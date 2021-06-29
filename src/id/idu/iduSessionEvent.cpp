/***********************************************************************
 * Copyright 1999-2000, RTBase Corporation or its subsidiaries.
 * All rights reserved.
 **********************************************************************/

/***********************************************************************
 * $Id: iduSessionEvent.cpp 64386 2014-04-09 01:34:40Z raysiasd $
 **********************************************************************/

#include <idl.h>
#include <ideErrorMgr.h>
#include <idErrorCode.h>
#include <idu.h>

/*----------------------------------------------------------------------
  Name:
      iduSessionErrorFilter() -- �����ڵ带 ����� ������ �ǵ���

  Argument:
      aFlag - Event Flag

  Return:
      Event�� �߻������� IDE_FAILURE

  Description:
      �� �Լ��� �������� ȣ���Ͽ�, ������ Event�� �����ϴ�
      �����μ� Close, Timeout Event�� �����Ѵ�.
      ����, �߰��� ��� �� �κп� �߰��Ǵ� �ڵ带 ������ �ȴ�.
 *--------------------------------------------------------------------*/

IDE_RC iduSessionErrorFilter( ULong aFlag, UInt aCurrStmtID )
{
    IDE_RC sRet = IDE_FAILURE;

    if (aFlag & IDU_SESSION_EVENT_CLOSED)
    {
        IDE_SET(ideSetErrorCode(idERR_ABORT_Session_Closed));
    }
    else
    {
        if (aFlag & IDU_SESSION_EVENT_CANCELED)
        {
            IDE_SET(ideSetErrorCode(idERR_ABORT_Query_Canceled));
        }
        else
        {
            if ( IDU_SESSION_CHK_TIMEOUT( aFlag, aCurrStmtID ) )
            {
                IDE_SET(ideSetErrorCode(idERR_ABORT_Query_Timeout));
            }
            else
            {
                sRet = IDE_SUCCESS;
            }
        }
    }
    return sRet;
}
