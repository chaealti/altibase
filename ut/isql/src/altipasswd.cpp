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
 * $Id: altipasswd.cpp 88351 2020-08-14 05:02:49Z chkim $
 **********************************************************************/
#include <idl.h>
#include <idp.h>
#include <mtcl.h>
#include <ideErrorMgr.h>
#include <idsPassword.h>

#define QC_MAX_NAME_LEN 40

SChar * getpass(const SChar *prompt);
SChar * altipasswd_toupper(SChar *aPasswd);
IDE_RC checkPrevPassword(SChar *aPasswordFile, SChar *aPassword);
/* BUG-48000 Need to print ideGetErrorMsg first */
/* BUG-47889 Case sensitive password */
IDE_RC GetIsCaseSensitivePasswd ( idBool* aIsCaseSensitivePasswd );

int main(int /*__ argc __*/ , char* /*__ argv __*/ [])
{
    SChar      *sHomeDir;
    SChar       sUserPasswd[ QC_MAX_NAME_LEN + 1 ] = {0,};
    SChar       sPasswordFile[256];
    SChar       sPrevPasswd[ QC_MAX_NAME_LEN + 1 ] = {0,};
    SChar       sNewPasswd1[256];
    SChar       sNewPasswd2[256];
    FILE       *fp;
    SChar       sCryptStr[IDS_MAX_PASSWORD_BUFFER_LEN + 1];
    UInt        sUserPassLen;
    
    /* BUG-48000 Need to print ideGetErrorMsg first */
    idBool sIsCaseSensitivePasswd = ID_FALSE;
    
    sHomeDir = idlOS::getenv(IDP_HOME_ENV);
    IDE_TEST_RAISE( sHomeDir == NULL, ERR_HOME_DIR );
   
    /* BUG-48000 Need to print ideGetErrorMsg first */
    /* BUG-47889 Case sensitive password */
    IDE_TEST( GetIsCaseSensitivePasswd( &sIsCaseSensitivePasswd ) != IDE_SUCCESS );

    idlOS::printf("Previous Password : ");
    idlOS::fflush(stdout);
    strcpy(sPrevPasswd, getpass(""));

    if (sIsCaseSensitivePasswd == ID_FALSE) 
    {
        altipasswd_toupper(sPrevPasswd);
    }

    idlOS::sprintf(sPasswordFile,
                   "%s%cconf%csyspassword",
                   sHomeDir,
                   IDL_FILE_SEPARATOR,
                   IDL_FILE_SEPARATOR);

    IDE_TEST( checkPrevPassword(sPasswordFile, sPrevPasswd)
              != IDE_SUCCESS );

    idlOS::printf("New Password : ");
    idlOS::fflush(stdout);
    strcpy(sNewPasswd1, getpass(""));

    if (sIsCaseSensitivePasswd == ID_FALSE) 
    {
        altipasswd_toupper(sNewPasswd1);
    }

    idlOS::printf("Retype New Password : ");
    idlOS::fflush(stdout);
    strcpy(sNewPasswd2, getpass(""));

    if (sIsCaseSensitivePasswd == ID_FALSE) 
    {
        altipasswd_toupper(sNewPasswd2);
    }

    IDE_TEST_RAISE(idlOS::strncmp(sNewPasswd1,
                                  sNewPasswd2,
                                  sizeof(sNewPasswd1)) != 0,
                   ERR_NOT_MATCH);

    idlOS::snprintf(sUserPasswd, ID_SIZEOF(sUserPasswd), "%s",
                    sNewPasswd1);

    sUserPassLen = idlOS::strlen( sUserPasswd );
    IDE_TEST_RAISE( (sUserPassLen == 0) || (sUserPassLen > IDS_MAX_PASSWORD_LEN),
                    invalid_passwd_error );
    
    fp = idlOS::fopen(sPasswordFile, "w+");
    IDE_TEST_RAISE( fp == NULL, ERR_PASSWD_FILE );

    // BUG-38565 password ��ȣȭ �˰��� ����
    idsPassword::crypt( sCryptStr, sUserPasswd, sUserPassLen, NULL );
    
    idlOS::fwrite(sCryptStr, 1, idlOS::strlen(sCryptStr), fp);
    idlOS::fclose(fp);

    exit(0);

    IDE_EXCEPTION( ERR_HOME_DIR );
    {
        idlOS::fprintf(stderr, "set environment ALTIBASE_HOME\n");
    }
    IDE_EXCEPTION( ERR_PASSWD_FILE );
    {
        idlOS::fprintf(stderr, "File syspassword open error\n");
    }
    IDE_EXCEPTION( ERR_NOT_MATCH );
    {
        idlOS::fprintf(stderr, "passwords do not match\n");
    }
    IDE_EXCEPTION( invalid_passwd_error );
    {
        idlOS::fprintf(stderr, "Incorrect Password\n");
    }
    IDE_EXCEPTION_END;
    
    exit(1);
}

SChar * altipasswd_toupper(SChar *aPasswd)
{
    SChar  * sPasswd;
    SChar  * sFence;
    
    const mtlModule * sDefaultModule;

    sDefaultModule = mtlDefaultModule();

    if ( aPasswd == NULL )
    {
        return NULL;
    }

    sFence = aPasswd + idlOS::strlen(aPasswd);
    
    // PRJ-1678 : For multi-byte character set string
    for(sPasswd = aPasswd; *sPasswd != 0;)
    {
        *sPasswd = idlOS::idlOS_toupper(*sPasswd);
        // bug-21949: nextChar error check
        if (sDefaultModule->nextCharPtr((UChar**)&sPasswd,
                    (UChar*)sFence) != NC_VALID)
        {
            break;
        }

    }
    return aPasswd;
}

IDE_RC checkPrevPassword(SChar *aPasswordFile, SChar *aPassword)
{
    SInt       sReadSize = 0;
    SChar      sUserPass[256 + 1];
    SChar      sCryptStr[IDS_MAX_PASSWORD_BUFFER_LEN + 1];
    FILE      *fp = NULL;
    UInt       sUserPassLen;

    fp = idlOS::fopen(aPasswordFile, "r");
    IDE_TEST_RAISE( fp == NULL, ERR_PASSWD_FILE );

    sReadSize = idlOS::fread(sUserPass, 1, 256, fp);
    idlOS::fclose(fp);

    sUserPass[sReadSize] = '\0';
    sUserPassLen = idlOS::strlen(aPassword);

    IDE_TEST_RAISE( (sUserPassLen == 0) || (sUserPassLen > IDS_MAX_PASSWORD_LEN),
                    invalid_passwd_error );

    // BUG-38565 password ��ȣȭ �˰��� ����
    idsPassword::crypt( sCryptStr, aPassword, sUserPassLen, sUserPass );
    
    IDE_TEST_RAISE( idlOS::strcmp( sCryptStr, sUserPass ) != 0,
                    invalid_passwd_error );

    return IDE_SUCCESS;

    IDE_EXCEPTION( invalid_passwd_error );
    {
        idlOS::fprintf(stderr, "Incorrect Password\n");
    }
    IDE_EXCEPTION( ERR_PASSWD_FILE );
    {
        idlOS::fprintf(stderr, "File syspassword open error\n");
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/* BUG-48000 Need to print ideGetErrorMsg first */
/* BUG-47889 Case sensitive password */
IDE_RC GetIsCaseSensitivePasswd( idBool *aIsCaseSensitivePasswd )
{
    IDE_RC  sPropRead;
    UInt    sPropIntValue;

    IDE_TEST_RAISE( idp::initialize() != IDE_SUCCESS, AltiPropIdeError );
    
    // Read CASE_SENSITIVE_PASSWORD from altibase.properteis
    sPropRead = idp::read("CASE_SENSITIVE_PASSWORD", (void *)&sPropIntValue, 0);

    if ( (sPropRead == IDE_SUCCESS) && (sPropIntValue == 1) )
    {
        *aIsCaseSensitivePasswd = ID_TRUE;
    }
    else
    {
        *aIsCaseSensitivePasswd = ID_FALSE;
    }
    
    (void) idp::destroy();

    return IDE_SUCCESS;
    
    /* TODO: merge ut error message */
    IDE_EXCEPTION(AltiPropIdeError) ;
    {
        // print error msg from ide directly
        idlOS::fprintf(stderr, "%s\n", ideGetErrorMsg() );
    }

    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}
