/***********************************************************************
 * Copyright 1999-2001, ALTIBase Corporation or its subsidiaries.
 * All rights reserved.
 **********************************************************************/

/***********************************************************************
 * $Id: iduFileStream.cpp 66405 2014-08-13 07:15:26Z djin $
 **********************************************************************/

#include <idl.h>
#include <ideCallback.h>
#include <ideErrorMgr.h>
#include <idErrorCode.h>
#include <ideLog.h>
#include <iduFileStream.h>

IDE_RC iduFileStream::openFile( FILE** aFp,
                                SChar* aFilePath,
                                SChar* aMode )
{
/***********************************************************************
 *
 * Description : file�� open��
 *
 * Implementation :
 *    1. fopen �Լ� ȣ��
 *    2. ��ȯ�� fp�� null�̸� errno �˻�
 *    3. errno�� ���� ������ error ��ȯ
 *
 ***********************************************************************/
#define IDE_FN "iduFileStream::openFile"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(IDE_FN));

    SInt sSystemErrno = 0;
    
    *aFp = idlOS::fopen( aFilePath, aMode );
    
    if( *aFp == NULL )
    {
        sSystemErrno = errno;

#if defined(WRS_VXWORKS)
        IDE_TEST_RAISE( sSystemErrno == ENOSPC, NO_SPACE_ERROR );
#else
        IDE_TEST_RAISE( (sSystemErrno == ENOSPC) ||
                        (sSystemErrno == EDQUOT),
                        NO_SPACE_ERROR );
#endif
        IDE_TEST_RAISE( (sSystemErrno == ENFILE) ||
                        (sSystemErrno == EMFILE),
                        OPEN_LIMIT_ERROR );
        
        IDE_RAISE(INVALID_OPERATION);
    }
    else
    {
        // Nothing to do
    }
    
    return IDE_SUCCESS;

    IDE_EXCEPTION(NO_SPACE_ERROR);
    {
        IDE_SET(ideSetErrorCode(idERR_ABORT_DISK_SPACE_EXHAUSTED,
                                aFilePath,
                                0,
                                0));
    }
    IDE_EXCEPTION(OPEN_LIMIT_ERROR);
    {
        IDE_SET(ideSetErrorCode(idERR_ABORT_EXCEED_OPEN_FILE_LIMIT,
                                aFilePath,
                                0,
                                0));
    }
    IDE_EXCEPTION(INVALID_OPERATION);
    {
        IDE_SET(ideSetErrorCode(idERR_ABORT_IDU_FILE_INVALID_OPERATION));
    }
    
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
    
#undef IDE_FN
}

IDE_RC iduFileStream::closeFile( FILE* aFp )
{
/***********************************************************************
 *
 * Description : file�� close��
 *
 * Implementation :
 *    1. fclose �Լ� ȣ��
 *    2. return value�� 0�� �ƴϸ� ����
 *    3. errno�� ���� ������ error ��ȯ
 *
 ***********************************************************************/
#define IDE_FN "iduFileStream::closeFile"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(IDE_FN));

    SInt sRet;

    sRet = idlOS::fclose( aFp );

    IDE_TEST_RAISE( sRet != 0, CLOSE_ERROR );
        
    return IDE_SUCCESS;
    
    IDE_EXCEPTION(CLOSE_ERROR);
    {
        // BUG-21760 PSM���� ������ ���Ͽ� ���Ͽ� close�� ���д�
        // ABORT�� ����ϴ�.
        IDE_SET(ideSetErrorCode(idERR_ABORT_FILE_CLOSE));
    }

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
    
#undef IDE_FN
}

IDE_RC iduFileStream::getString( SChar* aStr,
                                 SInt   aLength,
                                 FILE* aFp )
{
/***********************************************************************
 *
 * Description : �� line�� ���Ͽ��� �о��
 *
 * Implementation :
 *    1. fgets �Լ� ȣ��
 *    2. return value�� NULL�̸� ����
 *    3. ������ ���̶�� no data found, �׷��� �ʴٸ� invalid operation
 *
 ***********************************************************************/
#define IDE_FN "iduFileStream::getString"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(IDE_FN));

    SChar* sRet;

    sRet = idlOS::fgets( aStr, aLength, aFp );

    if( sRet == NULL )
    {
        if( idlOS::idlOS_feof( aFp ) )
        {
            // NO_DATA_FOUND
            IDE_RAISE(NO_DATA_FOUND);
        }
        else
        {
            IDE_RAISE(READ_ERROR);
        }
    }
    else
    {
        // Nothing to do
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION(NO_DATA_FOUND);
    {
        IDE_SET(ideSetErrorCode(idERR_ABORT_IDU_FILE_NO_DATA_FOUND));
    }
    IDE_EXCEPTION(READ_ERROR);
    {
        IDE_SET(ideSetErrorCode(idERR_ABORT_IDU_FILE_READ_ERROR));
    }
    
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
    
#undef IDE_FN
}

IDE_RC iduFileStream::putString( SChar* aStr,
                                 FILE* aFp )
{
/***********************************************************************
 *
 * Description : string�� ���Ͽ� ���
 *
 * Implementation :
 *    1. fputs �Լ� ȣ��
 *    2. return value�� EOF(-1)�̸� ����
 *
 ***********************************************************************/
#define IDE_FN "iduFileStream::putString"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(IDE_FN));

    SInt sRet;

    sRet = idlOS::fputs( aStr, aFp );

    IDE_TEST_RAISE( sRet == EOF, WRITE_ERROR );
    
    return IDE_SUCCESS;
    
    IDE_EXCEPTION(WRITE_ERROR);
    {
        IDE_SET(ideSetErrorCode(idERR_ABORT_IDU_FILE_WRITE_ERROR));
    }
    
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
    
#undef IDE_FN
}

IDE_RC iduFileStream::flushFile( FILE* aFp )
{
/***********************************************************************
 *
 * Description : string�� ���Ͽ� ���
 *
 * Implementation :
 *    1. fflush �Լ� ȣ��
 *    2. return value�� 0�� �ƴ϶�� ����
 *    3. errno�� ���� ������ error ��ȯ
 *
 ***********************************************************************/
#define IDE_FN "iduFileStream::flushFile"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(IDE_FN));

    SInt sRet;
    SInt sSystemErrno = 0;
    
    IDE_DASSERT( aFp != NULL );
    
    sRet = idlOS::fflush( aFp );

    if( sRet != 0 )
    {
        sSystemErrno = errno;

#if !defined(VC_WINCE)
        IDE_TEST_RAISE(sSystemErrno == EBADF,
                       INVALID_FILEHANDLE);
#endif /* VC_WINCE */

#if defined(WRS_VXWORKS)
        IDE_TEST_RAISE( sSystemErrno == ENOSPC, NO_SPACE_ERROR );
#else
        IDE_TEST_RAISE((sSystemErrno == ENOSPC)||
                       (sSystemErrno == EDQUOT),
                       NO_SPACE_ERROR);
#endif
        
        IDE_RAISE(WRITE_ERROR);
    }
    else
    {
        // Nothing to do
    }
    
    return IDE_SUCCESS;
    
    IDE_EXCEPTION(INVALID_FILEHANDLE);
    {
        IDE_SET(ideSetErrorCode(idERR_ABORT_IDU_FILE_INVALID_FILEHANDLE));
    }
    IDE_EXCEPTION(WRITE_ERROR);
    {
        IDE_SET(ideSetErrorCode(idERR_ABORT_IDU_FILE_WRITE_ERROR));
    }
    IDE_EXCEPTION(NO_SPACE_ERROR);
    {
        IDE_SET(ideSetErrorCode(idERR_ABORT_DISK_SPACE_EXHAUSTED,
                                "filehandle in FFLUSH",
                                0,
                                0));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC iduFileStream::removeFile( SChar* aFilePath )
{
/***********************************************************************
 *
 * Description : ���� ����
 *
 * Implementation :
 *    1. remove �Լ� ȣ��
 *    2. return value�� 0�� �ƴ϶�� ����
 *
 ***********************************************************************/
#define IDE_FN "iduFileStream::removeFile"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(IDE_FN));

    SInt sRet;

    sRet = idlOS::remove( aFilePath );

    IDE_TEST_RAISE( sRet != 0, DELETE_FAILED );
    
    return IDE_SUCCESS;

    IDE_EXCEPTION(DELETE_FAILED);
    {
        IDE_SET(ideSetErrorCode(idERR_ABORT_IDU_FILE_DELETE_FAILED));
    }
    
    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC iduFileStream::renameFile( SChar* aOldFilePath,
                                  SChar* aNewFilePath,
                                  idBool aOverWrite )
{
/***********************************************************************
 *
 * Description : ���� �̵�(or rename)
 *
 * Implementation :
 *    1. overwrite�� ���õǾ� ������ aNewFilePath�� �̹� �����ϸ� ����
 *    2. remove �Լ� ȣ��
 *    3. return value�� 0�� �ƴ϶�� ����
 *
 ***********************************************************************/
#define IDE_FN "iduFileStream::renameFile"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(IDE_FN));

    FILE*  sFp;
    idBool sExist;
    SInt   sRet;
    
    sFp = idlOS::fopen( aNewFilePath, "r" );
    if( sFp == NULL )
    {
        sExist = ID_FALSE;
    }
    else
    {
        sExist = ID_TRUE;
        idlOS::fclose( sFp );
    }
    
    if( aOverWrite == ID_TRUE )
    {
        // execute remove function
        sRet = idlOS::rename( aOldFilePath, aNewFilePath );
        if( sRet != 0 )
        {
            IDE_RAISE(RENAME_FAILED);
        }
        else
        {
            // Nothing to do
        }
    }
    else
    {
        // if newfilepath is exists, return error
        if( sExist == ID_TRUE )
        {
            IDE_RAISE(RENAME_FAILED);
        }
        else
        {    
            sRet = idlOS::rename( aOldFilePath, aNewFilePath );

            IDE_TEST_RAISE( sRet != 0, RENAME_FAILED );
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION(RENAME_FAILED);
    {
        IDE_SET(ideSetErrorCode(idERR_ABORT_IDU_FILE_RENAME_FAILED));
    }
    
    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}
