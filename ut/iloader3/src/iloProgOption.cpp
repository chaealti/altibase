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
 * $Id: iloProgOption.cpp 90270 2021-03-21 23:20:18Z bethy $
 **********************************************************************/

#include <idp.h>
#include <iduProperty.h>
#include <ilo.h>

/* BUG-47652 Set file permission */
UInt gFilePerm; 

extern SChar gszCommand[COMMAND_LEN];
extern SChar *getpass(const SChar *prompt);

// BUG-17932: ���� ����
static const SChar *gHelpCommand[] =
{
#ifndef COMPILE_SHARDCLI
    "{ in | out | formout | structout | help }\n",
    "[-d datafile or datafiles] [-f formatfile]\n",
    "[-T table_name] [-F firstrow] [-L lastrow]\n",
    "[-t field_term] [-r row_term] [-mode mode_type]\n",
    "[-commit commit_unit] [-bad badfile]\n",
    "[-log logfile] [-e enclosing] [-array count]\n",
    "[-replication true/false] [-split number]\n",
    "[-readsize size] [-errors count]\n",
    "[-lob lob_option_string] [-atomic]\n",
    "[-parallel count] [-direct]\n",
    "[-rule csv]\n",
    "[-partition]\n", /* BUG-32114 aexport must support the import/export of partition tables.*/
    "[-dry-run]\n",
    "[-prefetch_rows]\n",
    "[-async_prefetch off|on|auto]\n",
    "[-geom WKB]\n", /* BUG-48357 WKB compatibility option */
    (SChar*) HELP_WITHOUT_NEWLINE(OPT_STMT_PREFIX) /* BUG-47608 */
#else /* COMPILE_SHARDCLI */
    "{ in | out | formout | structout | help }\n",
    "[-d datafile or datafiles] [-f formatfile]\n",
    "[-T table_name] [-F firstrow] [-L lastrow]\n",
    "[-t field_term] [-r row_term] [-mode mode_type]\n",
    "[-commit commit_unit] [-bad badfile]\n",
    "[-log logfile] [-e enclosing]\n",
    "[-replication true/false] [-split number]\n",
    "[-readsize size] [-errors count]\n",
    "[-parallel count]\n",
    "[-rule csv]\n",
    "[-partition]\n", /* BUG-32114 aexport must support the import/export of partition tables.*/
    "[-dry-run]\n",
    "[-prefetch_rows]\n",
    "[-async_prefetch off|on|auto]"
    "[-geom WKB]\n", /* BUG-48357 WKB compatibility option */
#endif /* COMPILE_SHARDCLI */
};

static const SChar *gHelpMessage[] =
{
    "=====================================================================\n"
    "                         "
    ILO_PRODUCT_NAME
    " HELP Screen\n"
    "=====================================================================\n"
#ifndef COMPILE_SHARDCLI
    "  Usage   : iloader [-h]\n"
#else /* COMPILE_SHARDCLI */
    "Usage : shardLoader [-h]\n"
#endif /* COMPILE_SHARDCLI */
    "                    [-s server_name] [-u user_name] [-p password]\n"
    "                    [-port port_no] [-silent] [-nst] [-displayquery]\n"
    "                    [-NLS_USE nls_name]\n"
    "                    [-prefer_ipv6]\n"
    "                    [-geom WKB]\n"
    "                    [-ssl_ca CA_file_path | -ssl_capath CA_dir_path]\n"
    "                    [-ssl_cert certificate_file_path]\n"
    "                    [-ssl_key key_file_path]\n"
    "                    [-ssl_verify]\n"
    "                    [-ssl_cipher cipher_list]\n"
    "                    [", "]\n"
    "\n"
    "            -h            : This screen\n"
    "            -s            : Specify server name to connect\n"
    "            -u            : Specify user name to connect\n"
    "            -p            : Specify password of specify user name\n"
    "            -port         : Specify port number to communication\n"
    "            -silent       : No display Copyright\n"
    "            -nst          : No display Elapsed Time\n"
    "            -displayquery : display query string\n"
    "            -NLS_USE      : Specify NLS\n"
    "            -prefer_ipv6  : Prefer resolving server_name to IPv6 Address\n"
    "            -geom         : Specify geometry format such as WKB\n"
    "            -ssl_ca       : The path to a CA certificate file\n"
    "            -ssl_cpath    : The path to a directory that contains CA certificates\n"
    "            -ssl_cert     : The path to the client certificate\n"
    "            -ssl_key      : The path to the client private key file\n" 
    "            -ssl_verify   : Whether the client is to check certificates\n"
    "                            that are sent by the server to the client\n"
    "            -ssl_cipher   : A list of SSL ciphers\n"
    "=====================================================================",
};

void PrintHelpScreenCore(ECommandType aType)
{
    const SChar *sPrefix = (aType == NON_COM) ? "                     " : "        ";
    UInt i;

    if (aType == NON_COM)
    {
        idlOS::printf(gHelpMessage[0]);
    }
    else
    {
        idlOS::printf("Usage : ");
    }
    idlOS::printf(gHelpCommand[0]);
    for (i = 1; i < (ID_SIZEOF(gHelpCommand) / ID_SIZEOF(SChar*)); i++)
    {
        idlOS::printf(sPrefix);
        idlOS::printf(gHelpCommand[i]);
    }
    if (aType == NON_COM)
    {
        idlOS::printf(gHelpMessage[1]);
    }
    idlOS::printf("\n");
}

// BUG-17932: ���� ����
void iloProgOption::PrintHelpScreen(ECommandType aType)
{
    switch (aType)
    {
        case DATA_IN:
            idlOS::printf("Ex) in -f $formatfile -d $datafile -bad $badfile"
                          " -log $logfile -e $enclosing\n");
            break;
        case DATA_OUT:
            idlOS::printf("Ex) out -f $formatfile -d $datafile -split $number\n");
            break;
        case FORM_OUT:
            idlOS::printf("Ex) formout -T $table_name -f $formatfile\n");
            break;
        case STRUCT_OUT:
            idlOS::printf("Ex) structout -T $table_name -f $filename\n");
            break;
        case EXIT_COM:
            idlOS::printf("Ex) exit (or quit)\n");
            break;
        case HELP_HELP:
            idlOS::printf("Ex) help [ in | out | formout | structout | exit | help ]\n");
            break;
        case HELP_COM:
            PrintHelpScreenCore(aType);
            break;
        case NON_COM:
        default:
            PrintHelpScreenCore(aType);
            break;
    }
    idlOS::fflush(stdout);
}

iloProgOption::iloProgOption()
{
    // BUG-26287: change default value
    idlOS::strcpy(m_DBName  , "mydb");
    idlOS::strcpy(m_NLS     , "US7ASCII");
    idlOS::strcpy(m_DataNLS , "US7ASCII");
    m_PortNum = 9999;

    /* BUG-24591 : LOB insert error from datafiles of the CSV format */ 
    idlOS::strcpy(m_DefaultFieldTerm, ",");
    idlOS::strcpy(m_DefaultRowTerm, "\n");

    /* TASK-2657 */
    mCSVFieldTerm = ',';
    mCSVEnclosing = '"';

    /* BUG-29932 : [WIN] iloader �� noprompt �ɼ��� �ʿ��մϴ�. */
    mNoPrompt = ILO_FALSE;

    /* BUG-30415: ���� �߻� �÷��� �ʱ�ȭ */
    m_bErrorExist = SQL_FALSE;

    InitOption();
}

void iloProgOption::InitOption()
{
    m_CommandType = NON_COM;
    idlOS::strcpy(m_FieldTerm, m_DefaultFieldTerm);
    idlOS::strcpy(m_RowTerm, m_DefaultRowTerm);

    // BUG-26287: �ɼ� ó����� ����
    // -NLS_USE �ɼ� �߰�
    m_bExist_NLS = ILO_FALSE;
    m_bExist_b = SQL_FALSE; // bad input checker
    m_bExist_T = SQL_FALSE;

    m_bExist_U = SQL_FALSE;
    m_bExist_P = SQL_FALSE;
    m_bExist_S = SQL_FALSE;
    m_bExist_Silent = SQL_FALSE;
    m_bExist_PORT = SQL_FALSE;
    m_bExist_NST = SQL_FALSE;

    m_nTableCount = 0;
    m_bExist_d = SQL_FALSE;
    m_bExist_f = SQL_FALSE;
    m_bExist_F = SQL_FALSE;
    m_bExist_L = SQL_FALSE;
    m_bExist_t = SQL_FALSE;
    m_bExist_r = SQL_FALSE;
    m_bExist_e = SQL_FALSE;

    m_bExist_mode = SQL_FALSE;
    m_LoadMode = ILO_APPEND;

    m_bExist_commit = SQL_FALSE;
    // BUG-13015
    m_CommitUnit = 1000;
    // PROJ-1518
    m_bExist_atomic = SQL_FALSE;
    m_bExist_array  = SQL_FALSE;
    m_ArrayCount    = 0;
    
    // PROJ-1714
    m_bExist_parallel = SQL_FALSE;
    m_ParallelCount   = 1;
    m_DataFileNum     = 0;
    m_DataFilePos     = 0;
    
    // PROJ-1760
    m_bExist_direct     = SQL_FALSE;
    m_bExist_ioParallel = SQL_FALSE;
    m_directLogging     = SQL_TRUE;
    m_ioParallelCount   = 0;           

    m_bExist_errors = SQL_FALSE;
    // BUG-24879 errors �ɼ� ���� �⺻�� 50
    m_ErrorCount = 50;
    // BUG-18803 readsize �ɼ� �߰�
    mReadSizeExist  = SQL_FALSE;
    mReadSzie       = FILE_READ_SIZE_DEFAULT;

    m_bExist_log = SQL_FALSE;
    m_bExist_bad = SQL_FALSE;
    mReplication = SQL_TRUE;

    m_bExist_split = SQL_FALSE;
    m_SplitRowCount = 0;

    m_bExist_informix = SQL_FALSE;
    mInformix = SQL_FALSE;

    m_bExist_noexp = SQL_FALSE;
    mNoExp = SQL_FALSE;

    m_bExist_mssql = SQL_FALSE;
    mMsSql = SQL_FALSE;

    m_bDisplayQuery = SQL_FALSE;
    mInvalidOption = SQL_FALSE;

    /* bug-18707 */
    mExistWaitTime = SQL_FALSE;
    mWaitTime = 0;
    mExistWaitCycle = SQL_FALSE;
    mWaitCycle = 0;

    // BUG-21179
    mRule = csv;
    mExistRule   = SQL_FALSE;
    m_bExist_TabOwner = SQL_FALSE;

    mExistUseLOBFile = ILO_FALSE;
    mUseLOBFile = ILO_FALSE;

    mExistLOBFileSize = ILO_FALSE;
    mLOBFileSize = ID_ULONG(0);

    mExistUseSeparateFiles = ILO_FALSE;
    mUseSeparateFiles = ILO_FALSE;

    mExistLOBIndicator = ILO_FALSE;
    idlOS::snprintf(mLOBIndicator, ID_SIZEOF(mLOBIndicator), "%%%%");

    mFlockFlag = ILO_FALSE;
    mNCharUTF16 = ILO_FALSE;
    mNCharColExist = ILO_FALSE;

    /* BUG-29915 */
    mPreferIPv6 = ILO_FALSE;

    idlOS::memset( m_TableOwner, 0x00, ID_SIZEOF(m_TableOwner));

    /* BUG-30413 row total count */
    mGetTotalRowCount = ILO_FALSE;
    mSetRowFrequency = 0;
    mStopIloader = ILO_FALSE;

    /* BUG-30467 */
    mPartition = ILO_FALSE;

    /* BUG-31387 */
    m_ConnType = ILO_CONNTYPE_NONE;

    /* BUG-41406 SSL */
    m_bExist_SslCa     = ID_FALSE;
    m_bExist_SslCapath = ID_FALSE;
    m_bExist_SslCert   = ID_FALSE;
    m_bExist_SslKey    = ID_FALSE;
    m_bExist_SslCipher = ID_FALSE;
    m_bExist_SslVerify = ID_FALSE;
    m_SslCa[0]         = '\0';
    m_SslCapath[0]     = '\0';
    m_SslCert[0]       = '\0';
    m_SslKey[0]        = '\0';
    m_SslCipher[0]     = '\0';
    m_SslVerify[0]     = '\0';

    /* BUG-43388 */
    mDryrun = ILO_FALSE;

    /* BUG-43277 -prefetch_rows option */
    m_bExist_prefetch_rows = ID_FALSE;
    m_PrefetchRows = 0;

    /* BUG-44187 Support Asynchronous Prefetch */
    m_bExist_async_prefetch = ID_FALSE;
    m_AsyncPrefetchType = ASYNC_PREFETCH_OFF;
    
    /* BUG-47608 insert_prefix: node_meta */
    m_bExist_StmtPrefix = ID_FALSE;
    m_StmtPrefix[0]     = '\0';
    
    /* BUG-48016 Fix skipped commit */
    m_bExist_ShowCommit = ID_FALSE;

    /* BUG-47652 Set file permission */
    mbExistFilePerm == ID_FALSE;
    gFilePerm = DEFAULT_FILE_PERM;
    
    /* BUG-48357 WKB compatibility option */
    m_bExist_geom = ID_FALSE;
    m_bIsGeomWKB = ID_FALSE;

    mExistTxLevel  = SQL_FALSE;
    mTxLevel       = 0;
}

SChar *iloProgOption::MakeCommandLine(SInt argc, SChar **argv)
{
    static SChar szCommand[2048];
    SInt         i;

    szCommand[0] = '\0';
    for (i=1; i<argc; i++)
    {
    idlOS::strcat(szCommand, argv[i]);
    idlOS::strcat(szCommand, " ");
    }

    return szCommand;
}

SInt iloProgOption::ParsingCommandLine( ALTIBASE_ILOADER_HANDLE  aHandle,
                                        SInt                     argc,
                                        SChar                  **argv)
{
    SChar *sPtr;
    SInt   i = 1;
    UInt   sCmdLen;

    iloaderHandle *sHandle = (iloaderHandle *) aHandle;

    /* BUG-30092 remove \r from end of command line */
    SInt   sLastArgvLen;
    if (argc >= 1)
    {
        sLastArgvLen = idlOS::strlen(argv[argc - 1]);
        if (sLastArgvLen >= 1)
        {
            if (idlOS::idlOS_isspace(argv[argc - 1][sLastArgvLen - 1]) != 0)
            {
                argv[argc - 1][sLastArgvLen - 1] = 0;
            }
        }
    }

    while (i<argc)
    {
        // BUG-17932: ���� ����
        // command line���� ������ ����� ���� �׻� ��ü ���� ���
        if (idlOS::strcasecmp(argv[i], "-h") == 0
         || idlOS::strcasecmp(argv[i], "help") == 0
         || idlOS::strcasecmp(argv[i], "--help") == 0)
        {
            IDE_RAISE( print_help_screen );
        }

        if (idlOS::strcasecmp(argv[i], "-u") == 0)
        {
            IDE_TEST_RAISE( m_bExist_U == SQL_TRUE, err_user );
            IDE_TEST_RAISE( i+1 >= argc, err_nouser );

            m_bExist_U = SQL_TRUE;
            if (argv[i+1][0] == '-')
            {
                idlOS::strcpy(m_LoginID, "");
            }
            else
            {
                // BUG-39969: like BUG-17430
                utString::makeNameInCLI(m_LoginID,
                                        ID_SIZEOF(m_LoginID),
                                        argv[i+1],
                                        idlOS::strlen(argv[i+1]));
            }

            i += 2;
        }

        else if (idlOS::strcasecmp(argv[i], "-p") == 0)
        {
            IDE_TEST_RAISE( m_bExist_P == SQL_TRUE, err_passwd );
            IDE_TEST_RAISE( i+1 >= argc, err_nopasswd );

            m_bExist_P = SQL_TRUE;
            if (argv[i+1][0] == '-')
            {
                idlOS::strcpy(m_Password, "");
            }
            else
            {
                idlOS::strcpy(m_Password, argv[i+1]);
            }

            i += 2;
        }

        else if (idlOS::strcmp(argv[i], "-S") == 0 ||
                 idlOS::strcmp(argv[i], "-s") == 0)
        {
            IDE_TEST_RAISE( m_bExist_S == SQL_TRUE, err_server_exist );
            IDE_TEST_RAISE( i+1 >= argc, err_noserver );

            m_bExist_S = SQL_TRUE;
            if (argv[i+1][0] == '-')
            {
                idlOS::strcpy(m_ServerName, "");
            }
            else
            {
                idlOS::strcpy(m_ServerName, argv[i+1]);
            }
            i += 2;
        }
        else if (idlOS::strcmp(argv[i], "-port") == 0 ||
                 idlOS::strcmp(argv[i], "-PORT") == 0)
        {
            IDE_TEST_RAISE( m_bExist_PORT == SQL_TRUE, err_port );
            IDE_TEST_RAISE( i+1 >= argc, err_noport );

            m_bExist_PORT = SQL_TRUE;
            m_PortNum     = idlOS::atoi(argv[i+1]);
            i += 2;
        }
        // BUG-26287: �ɼ� ó����� ����
        // -NLS_USE �ɼ� �߰�
        else if (idlOS::strcasecmp(argv[i], "-NLS_USE") == 0)
        {
            /* NLS�� ���� ��� */
            IDE_TEST_RAISE(argc <= i+1, print_help_screen);
            IDE_TEST_RAISE(argv[i+1][1] == '-', print_help_screen);

            m_bExist_NLS = ILO_TRUE;
            idlOS::strncpy(m_NLS, argv[i+1], ID_SIZEOF(m_NLS));
            i += 2;
        }
        else if (idlOS::strcmp(argv[i], "-nst") == 0 ||
                 idlOS::strcmp(argv[i], "-NST") == 0)
        {
            IDE_TEST_RAISE( m_bExist_NST == SQL_TRUE, err_nst );
            m_bExist_NST = SQL_TRUE;
            i++;
        }
        else if (idlOS::strcmp(argv[i], "-silent") == 0 ||
                 idlOS::strcmp(argv[i], "-SILENT") == 0)
        {
            IDE_TEST_RAISE( m_bExist_Silent == SQL_TRUE, err_silent );
            m_bExist_Silent = SQL_TRUE;
            i++;
        }
        else if (idlOS::strcmp(argv[i], "-displayquery") == 0 ||
                 idlOS::strcmp(argv[i], "-DISPLAYQUERY") == 0)
        {
            IDE_TEST_RAISE( m_bDisplayQuery == SQL_TRUE, err_display );
            m_bDisplayQuery = SQL_TRUE;
            i++;
        }
        /* bug-18792 */
        else if (idlOS::strcmp(argv[i], "-plus") == 0 ||
                 idlOS::strcmp(argv[i], "-PLUS") == 0)
        {
            // BUG-28708 -plus �ɼ��� drop��
            // ȣȯ���� ���� �ɼ� ��ü�� ���ܵε�, �����Ѵ�.
            (void)idlOS::printf("NOTICE: -plus option is deprecated. " \
                                "Thus, the option will be ignored.\n");
            i++;
        }
        /* bug-18707 */
        else if (idlOS::strcmp(argv[i], "-waittime") == 0)
        {
            IDE_TEST_RAISE( mExistWaitTime == SQL_TRUE, err_waittime );
            IDE_TEST_RAISE( i+1 >= argc, err_wait ); 

            mExistWaitTime    = SQL_TRUE;
            mWaitTime         = idlOS::atoi(argv[i+1]);
            i += 2;
        }
        else if (idlOS::strcmp(argv[i], "-waitcycle") == 0)
        {
            IDE_TEST_RAISE( mExistWaitCycle == SQL_TRUE, err_waitcycle );
            IDE_TEST_RAISE( i+1 >= argc, err_wait ); 

            mExistWaitCycle    = SQL_TRUE;
            mWaitCycle         = idlOS::atoi(argv[i+1]);
            i += 2;
        }
        else if (idlOS::strcmp(argv[i], "-atomic") == 0)
        {
            m_bExist_atomic   = SQL_TRUE;
            i += 1;
        }
        else if (idlOS::strcasecmp(argv[i], "-lob") == 0)
        {
            idlVA::appendFormat(gszCommand, ID_SIZEOF(gszCommand), "%s ",
                                argv[i]);
            i++;

            /* ���� "lob_option_string"����
             * ����ǥ�� �����ϰ� '\"'�� '"'���� �����ع��� ����
             * ���󺹱ͽ�Ų��. */
            if (i < argc && argv[i][0] != '-')
            {
#define APPEND_CHAR_TO_CMD(aC) \
                if (sCmdLen < ID_SIZEOF(gszCommand) - 1)\
                {\
                    gszCommand[sCmdLen++] = (SChar)(aC);\
                }

                sCmdLen = (UInt)idlOS::strlen(gszCommand);
                APPEND_CHAR_TO_CMD('"');
                for (sPtr = argv[i]; *sPtr; sPtr++)
                {
                    if (*sPtr != '"')
                    {
                        APPEND_CHAR_TO_CMD(*sPtr);
                    }
                    else
                    {
                        APPEND_CHAR_TO_CMD('\\');
                        APPEND_CHAR_TO_CMD(*sPtr);
                    }
                }
                APPEND_CHAR_TO_CMD('"');
                APPEND_CHAR_TO_CMD(' ');
                gszCommand[sCmdLen] = '\0';
                i++;

#undef APPEND_CHAR_TO_CMD
            }
        }
        /* BUG-29932 : [WIN] iloader �� noprompt �ɼ��� �ʿ��մϴ�. */
        else if (idlOS::strcmp(argv[i], "-noprompt") == 0 ||
                 idlOS::strcmp(argv[i], "-NOPROMPT") == 0)
        {
            IDE_TEST_RAISE( mNoPrompt == ILO_TRUE, err_noprompt );
            mNoPrompt = ILO_TRUE;
            i++;
        }
        /* BUG-29915 */
        else if (idlOS::strcasecmp(argv[i], "-prefer_ipv6") == 0)
        {
            IDE_TEST_RAISE( mPreferIPv6 == ILO_TRUE, err_prefer_ipv6 );
            mPreferIPv6 = ILO_TRUE;
            i++;
        }
        /* BUG-30467 */        
        else if (idlOS::strcmp(argv[i], "-partition") == 0 ||
                 idlOS::strcmp(argv[i], "-PARTITION") == 0)
        {
            IDE_TEST_RAISE( mPartition == ILO_TRUE, err_partition );
            mPartition= ILO_TRUE;
            i++;
        }
        /* BUG-41406 SSL */
        else if (idlOS::strcasecmp(argv[i], "-ssl_ca") == 0)
        {
            IDE_TEST_RAISE(argc <= i + 1, print_help_screen);
            IDE_TEST_RAISE(idlOS::strncmp(argv[i + 1], "-", 1) == 0,
                           print_help_screen);

            i++;
            m_bExist_SslCa = ID_TRUE;
            idlOS::snprintf(m_SslCa, ID_SIZEOF(m_SslCa), "%s",
                            argv[i]);
            i++;
        }
        else if (idlOS::strcasecmp(argv[i], "-ssl_capath") == 0)
        {
            IDE_TEST_RAISE(argc <= i + 1, print_help_screen);
            IDE_TEST_RAISE(idlOS::strncmp(argv[i + 1], "-", 1) == 0,
                           print_help_screen);

            i++;
            m_bExist_SslCapath = ID_TRUE;
            idlOS::snprintf(m_SslCapath, ID_SIZEOF(m_SslCapath), "%s",
                            argv[i]);
            i++;
        }
        else if (idlOS::strcasecmp(argv[i], "-ssl_cert") == 0)
        {
            IDE_TEST_RAISE(argc <= i + 1, print_help_screen);
            IDE_TEST_RAISE(idlOS::strncmp(argv[i + 1], "-", 1) == 0,
                           print_help_screen);

            i++;
            m_bExist_SslCert = ID_TRUE;
            idlOS::snprintf(m_SslCert, ID_SIZEOF(m_SslCert), "%s",
                            argv[i]);
            i++;
        }
        else if (idlOS::strcasecmp(argv[i], "-ssl_key") == 0)
        {
            IDE_TEST_RAISE(argc <= i + 1, print_help_screen);
            IDE_TEST_RAISE(idlOS::strncmp(argv[i + 1], "-", 1) == 0,
                           print_help_screen);

            i++;
            m_bExist_SslKey = ID_TRUE;
            idlOS::snprintf(m_SslKey, ID_SIZEOF(m_SslKey), "%s",
                            argv[i]);
            i++;
        }
        else if (idlOS::strcasecmp(argv[i], "-ssl_cipher") == 0)
        {
            IDE_TEST_RAISE(argc <= i + 1, print_help_screen);
            IDE_TEST_RAISE(idlOS::strncmp(argv[i + 1], "-", 1) == 0,
                           print_help_screen);

            i++;
            m_bExist_SslCipher = ID_TRUE;
            idlOS::snprintf(m_SslCipher, ID_SIZEOF(m_SslCipher), "%s",
                            argv[i]);
            i++;
        }
        else if (idlOS::strcasecmp(argv[i], "-ssl_verify") == 0)
        {
            m_bExist_SslVerify = ID_TRUE;
            idlOS::strcpy(m_SslVerify, "ON");
            i++;
        }
        /* BUG-43388 */        
        else if (idlOS::strcmp(argv[i], "-dry-run") == 0)
        {
            IDE_TEST_RAISE( mDryrun == ILO_TRUE, err_dryrun );
            mDryrun= ILO_TRUE;
            i++;
        }
        /* BUG-47608 stmt_prefix */
        else if (idlOS::strcmp(argv[i], (SChar*) OPT_STMT_PREFIX) == 0)
        {
            IDE_TEST_RAISE( m_bExist_StmtPrefix == ID_TRUE, err_stmt_prefix );
            m_bExist_StmtPrefix= ID_TRUE;
            i++;

            /* If no user-defined prefix, then input default value: NODE [META] */
            if ( (argc <= i) || (idlOS::strncmp(argv[i], "-", 1) == 0) ) 
            {
                idlOS::snprintf(m_StmtPrefix, idlOS::strlen((SChar*) NODE_META) + 1, 
                        "%s", (SChar*) NODE_META);
            }
            /* User-defined prefix */
            else
            {
                idlOS::snprintf(m_StmtPrefix, ID_SIZEOF(m_StmtPrefix), "%s",
                        argv[i]);
                i++;
            }
        }
        /* BUG-48016 Fix skipped commit */
        else if (idlOS::strcmp(argv[i], "-dev-show-commit") == 0)
        {
            m_bExist_ShowCommit = ID_TRUE;
            i++;
        }
        /* BUG-48357 WKB compatibility option */
        else if (idlOS::strcmp(argv[i], (SChar*) OPT_GEOM ) == 0)
        {
            IDE_TEST_RAISE( m_bExist_geom == ID_TRUE, err_geom_dup );
            m_bExist_geom = ID_TRUE;
            i++;

            IDE_TEST_RAISE( argv[i] == NULL, err_geom_no_value );
            IDE_TEST_RAISE( idlOS::strcasecmp(argv[i], "WKB") != 0, 
                            err_geom_invalid_value );
            m_bIsGeomWKB = ID_TRUE;
            i++;
        }
        else
        {
            idlOS::strcat(gszCommand, argv[i]);
            idlOS::strcat(gszCommand, " ");
            i++;
        }
    }

    if( m_ConnType == ILO_CONNTYPE_NONE )
    {
        /* BUG-31387: ConnType�� �ѹ��� ��� ��Ȱ�� */
        sPtr = idlOS::getenv(ENV_ISQL_CONNECTION);
        if (sPtr != NULL)
        {
            if (idlOS::strcasecmp(sPtr, "TCP") == 0)
            {
                m_ConnType = ILO_CONNTYPE_TCP;
            }
            else if (idlOS::strcasecmp(sPtr, "UNIX") == 0)
            {
    #if !defined(VC_WIN32) && !defined(NTO_QNX)
                m_ConnType = ILO_CONNTYPE_UNIX;
    #else
                m_ConnType = ILO_CONNTYPE_TCP;
    #endif
            }
            else if (idlOS::strcasecmp(sPtr, "IPC") == 0)
            {
                m_ConnType = ILO_CONNTYPE_IPC;
            }
            else if( idlOS::strcasecmp( sPtr, "SSL") == 0 )
            {
                m_ConnType = ILO_CONNTYPE_SSL;
            }
            else if (idlOS::strcasecmp(sPtr, "IPCDA") == 0) // BUG-44094
            {
                m_ConnType = ILO_CONNTYPE_IPCDA;
            }
            else
            {
                m_ConnType = ILO_CONNTYPE_TCP;
            }
        }
        else
        {
            m_ConnType = ILO_CONNTYPE_TCP;
        }
    }

    return SQL_TRUE;

    IDE_EXCEPTION( err_user );
    {
        m_bErrorExist = SQL_TRUE;
        uteSetErrorCode(sHandle->mErrorMgr, utERR_ABORT_Dup_Option_Error, "-U");
        
        if ( sHandle->mUseApi != SQL_TRUE )
        {
            utePrintfErrorCode(stdout, sHandle->mErrorMgr);
        }
    }
    IDE_EXCEPTION( err_nouser );
    {
        m_bErrorExist = SQL_TRUE;
        uteSetErrorCode(sHandle->mErrorMgr, utERR_ABORT_UserID_Omit_Error);

        if ( sHandle->mUseApi != SQL_TRUE )
        {
            utePrintfErrorCode(stdout, sHandle->mErrorMgr);
        }
    }
    IDE_EXCEPTION( err_passwd );
    {
        m_bErrorExist = SQL_TRUE;
        uteSetErrorCode(sHandle->mErrorMgr, utERR_ABORT_Dup_Option_Error, "-P");
        
        if ( sHandle->mUseApi != SQL_TRUE )
        {
            utePrintfErrorCode(stdout, sHandle->mErrorMgr);
        }
    }
    IDE_EXCEPTION( err_nopasswd );
    {
        m_bErrorExist = SQL_TRUE;
        uteSetErrorCode(sHandle->mErrorMgr, utERR_ABORT_Password_Omit_Error);
        
        if ( sHandle->mUseApi != SQL_TRUE )
        {
            utePrintfErrorCode(stdout, sHandle->mErrorMgr);
        }
    }
    IDE_EXCEPTION( err_noserver );
    {
        m_bErrorExist = SQL_TRUE;
        uteSetErrorCode(sHandle->mErrorMgr, utERR_ABORT_ServerName_Omit_Error);
        
        if ( sHandle->mUseApi != SQL_TRUE )
        {
            utePrintfErrorCode(stdout, sHandle->mErrorMgr);
        }
    }
    IDE_EXCEPTION( err_noport );
    {
        m_bErrorExist = SQL_TRUE;
        uteSetErrorCode(sHandle->mErrorMgr, utERR_ABORT_Port_Omit_Error);

        if ( sHandle->mUseApi != SQL_TRUE )
        {
            utePrintfErrorCode(stdout, sHandle->mErrorMgr);
        }
    }
    IDE_EXCEPTION( err_server_exist );
    {
        m_bErrorExist = SQL_TRUE;
        uteSetErrorCode(sHandle->mErrorMgr, utERR_ABORT_Dup_Option_Error, "-S");
        
        if ( sHandle->mUseApi != SQL_TRUE )
        {
            utePrintfErrorCode(stdout, sHandle->mErrorMgr);
        }
    }
    IDE_EXCEPTION( err_port );
    {
        m_bErrorExist = SQL_TRUE;
        uteSetErrorCode(sHandle->mErrorMgr, utERR_ABORT_Dup_Option_Error, "-port");
        
        if ( sHandle->mUseApi != SQL_TRUE )
        {
            utePrintfErrorCode(stdout, sHandle->mErrorMgr);
        }
    }
    IDE_EXCEPTION( err_nst );
    {
        m_bErrorExist = SQL_TRUE;
        uteSetErrorCode(sHandle->mErrorMgr, utERR_ABORT_Dup_Option_Error, "-nst");
        
        if ( sHandle->mUseApi != SQL_TRUE )
        {
            utePrintfErrorCode(stdout, sHandle->mErrorMgr);
        }
    }
    IDE_EXCEPTION( err_silent );
    {
        m_bErrorExist = SQL_TRUE;
        uteSetErrorCode(sHandle->mErrorMgr, utERR_ABORT_Dup_Option_Error, "-silent");
        if ( sHandle->mUseApi != SQL_TRUE )
        {
            utePrintfErrorCode(stdout, sHandle->mErrorMgr);
        }
    }
    IDE_EXCEPTION( err_display );
    {
        m_bErrorExist = SQL_TRUE;
        uteSetErrorCode(sHandle->mErrorMgr, utERR_ABORT_Dup_Option_Error,
                        "-displayquery");
        if ( sHandle->mUseApi != SQL_TRUE )
        {
            utePrintfErrorCode(stdout, sHandle->mErrorMgr);
        }
    }
    IDE_EXCEPTION( print_help_screen );
    {
        m_bErrorExist = SQL_TRUE;
        PrintHelpScreen(NON_COM);
    }
    /* bug-18707 */
    IDE_EXCEPTION( err_waittime );
    {
        m_bDisplayQuery = SQL_TRUE;
        uteSetErrorCode(sHandle->mErrorMgr, utERR_ABORT_Dup_Option_Error,
                        "-waittime");
        if ( sHandle->mUseApi != SQL_TRUE )
        {
            utePrintfErrorCode(stdout, sHandle->mErrorMgr);
        }
    }
    IDE_EXCEPTION( err_waitcycle );
    {
        m_bDisplayQuery = SQL_TRUE;
        uteSetErrorCode(sHandle->mErrorMgr, utERR_ABORT_Dup_Option_Error,
                        "-waitcycle");
        if ( sHandle->mUseApi != SQL_TRUE )
        {
            utePrintfErrorCode(stdout, sHandle->mErrorMgr);
        }
    }
    IDE_EXCEPTION( err_wait );
    {
        m_bDisplayQuery = SQL_TRUE;
        uteSetErrorCode(sHandle->mErrorMgr, utERR_ABORT_Option_Incorrect_Error,
                        "-waittime",
                        "use : -waittime milisec -waitcycle rows");
        if ( sHandle->mUseApi != SQL_TRUE )
        {
            utePrintfErrorCode(stdout, sHandle->mErrorMgr);
        }
    }
    /* BUG-29932 : [WIN] iloader �� noprompt �ɼ��� �ʿ��մϴ�. */
    IDE_EXCEPTION( err_noprompt );
    {
        m_bErrorExist = SQL_TRUE;
        uteSetErrorCode( sHandle->mErrorMgr, utERR_ABORT_Dup_Option_Error, "-noprompt");
        if ( sHandle->mUseApi != SQL_TRUE )
        {
            utePrintfErrorCode(stdout, sHandle->mErrorMgr);
        }
    }
    /* BUG-29915 */
    IDE_EXCEPTION( err_prefer_ipv6 );
    {
        m_bErrorExist = SQL_TRUE;
        uteSetErrorCode( sHandle->mErrorMgr, utERR_ABORT_Dup_Option_Error, "-prefer_ipv6");
        if ( sHandle->mUseApi != SQL_TRUE )
        {
            utePrintfErrorCode(stdout, sHandle->mErrorMgr);
        }
    }
    /* BUG-30467 */
    IDE_EXCEPTION( err_partition );
    {
        m_bErrorExist = SQL_TRUE;
        uteSetErrorCode( sHandle->mErrorMgr, utERR_ABORT_Dup_Option_Error, "-partition");
        if ( sHandle->mUseApi != SQL_TRUE )
        {
            utePrintfErrorCode(stdout, sHandle->mErrorMgr);
        }
    }
    /* BUG-43388 */
    IDE_EXCEPTION( err_dryrun );
    {
        m_bErrorExist = SQL_TRUE;
        uteSetErrorCode( sHandle->mErrorMgr, utERR_ABORT_Dup_Option_Error, "-dry-run");
        if ( sHandle->mUseApi != SQL_TRUE )
        {
            utePrintfErrorCode(stdout, sHandle->mErrorMgr);
        }
    }
    /* BUG-47608 */
    IDE_EXCEPTION( err_stmt_prefix );
    {
        m_bErrorExist = SQL_TRUE;
        uteSetErrorCode( sHandle->mErrorMgr, utERR_ABORT_Dup_Option_Error, (SChar*) OPT_STMT_PREFIX);
        if ( sHandle->mUseApi != SQL_TRUE )
        {
            utePrintfErrorCode(stdout, sHandle->mErrorMgr);
        }
    }
    /* BUG-48357 */
    IDE_EXCEPTION( err_geom_dup );
    {
        m_bErrorExist = SQL_TRUE;
        uteSetErrorCode( sHandle->mErrorMgr, utERR_ABORT_Dup_Option_Error, (SChar*) OPT_GEOM );
        if ( sHandle->mUseApi != SQL_TRUE )
        {
            utePrintfErrorCode(stdout, sHandle->mErrorMgr);
        }
    }
    IDE_EXCEPTION( err_geom_no_value );
    {
        m_bErrorExist = SQL_TRUE;
        uteSetErrorCode( sHandle->mErrorMgr, utERR_ABORT_Option_No_Value_Error, (SChar*) OPT_GEOM );
        if ( sHandle->mUseApi != SQL_TRUE )
        {
            utePrintfErrorCode(stdout, sHandle->mErrorMgr);
        }
    }
    IDE_EXCEPTION( err_geom_invalid_value );
    {
        m_bErrorExist = SQL_TRUE;
        uteSetErrorCode( sHandle->mErrorMgr, utERR_ABORT_Option_Invalid_Value_Error,
                (SChar*) OPT_GEOM, argv[i] );
        if ( sHandle->mUseApi != SQL_TRUE )
        {
            utePrintfErrorCode(stdout, sHandle->mErrorMgr);
        }
    }
    IDE_EXCEPTION_END;

    return SQL_FALSE;
}

/* BUG-31387 */
/**
 * IPC�� Unix domain�� localhost�� ���� �� ���� ����� �� �����Ƿ�,
 * ServerName�� localhost�� �ƴ϶�� TCP�� ����ϵ��� ���� ������ �����Ѵ�.
 *
 * ���� ������ ������ �� IPC, Unix domain�� ����ϵ��� �����ߴٸ�
 * ���� ���� ������ ���õ��� �˸��� ��� �޽����� ����Ѵ�.
 * Unix �÷������� Unix domain�� ����� ���� ��Ʈ ��ȣ�� �ʿ����� �����Ƿ�
 * -port �ɼ��� ������������ ��� ����Ѵ�.
 *
 * @param aHandle iLoader �ڵ�
 */
void iloProgOption::AdjustConnType( ALTIBASE_ILOADER_HANDLE aHandle )
{
    iloaderHandle *sHandle = (iloaderHandle *) aHandle;

    if( ( m_ConnType == ILO_CONNTYPE_IPC )
            || ( m_ConnType == ILO_CONNTYPE_UNIX )
            || ( m_ConnType == ILO_CONNTYPE_IPCDA )
        )
    {
        if( ( m_bExist_S == SQL_TRUE )
            && ( idlOS::strcmp( m_ServerName, "::1" ) != 0 )
            && ( idlOS::strcmp( m_ServerName, "0:0:0:0:0:0:0:1" ) != 0 )
            && ( idlOS::strcmp( m_ServerName, "127.0.0.1" ) != 0 )
            && ( idlOS::strcmp( m_ServerName, "localhost" ) != 0 ) )
        {
            if( sHandle->mUseApi != SQL_TRUE )
            {
                idlOS::printf("WARNING: An attempt was made to connect to a "
                              "remote server using an IPC or UNIX domain connection.\n");
            }
            m_ConnType = ILO_CONNTYPE_TCP;
        }
#if !defined(VC_WIN32)
        else if( m_bExist_PORT == SQL_TRUE )
        {
            if ( sHandle->mUseApi != SQL_TRUE )
            {
                idlOS::printf("WARNING: A port number is not required when "
                              "connecting via IPC or UNIX, so the -port option was ignored.\n");
            }
        }
#endif
    }
}

SInt iloProgOption::ReadProgOptionInteractive()
{
    SChar szInStr[MAX_WORD_LEN * 2] = {'\0', };
    SInt sDefPortNo = ( (m_ConnType == ILO_CONNTYPE_IPC) || (m_ConnType == ILO_CONNTYPE_IPCDA) ) ?
                      DEFAULT_PORT_NO+50 : DEFAULT_PORT_NO;

    if (m_bExist_S == SQL_FALSE)
    {
        if (m_ConnType == ILO_CONNTYPE_TCP)
        {
            idlOS::printf("Write Server Name (default:localhost) : ");
            idlOS::fflush(stdout);
            idlOS::gets(szInStr, sizeof(szInStr));

            if (strlen(szInStr) == 0)
            {
                idlOS::strcpy(m_ServerName, "localhost");
            }
            else
            {
                idlOS::strcpy(m_ServerName, szInStr);
            }
        }
        else /* if ((m_ConnType == ILO_CONNTYPE_IPC) || (m_ConnType == ILO_CONNTYPE_UNIX)) */
        {
            idlOS::strcpy(m_ServerName, "localhost");
        }
        m_bExist_S = SQL_TRUE;
    }

    // BUG-26287: �ɼ� ó����� ����
#if defined(VC_WIN32)
    if ( m_bExist_PORT == SQL_FALSE )
#else
    if ((m_bExist_PORT == SQL_FALSE) &&
        (m_ConnType == ILO_CONNTYPE_TCP ||
         m_ConnType == ILO_CONNTYPE_SSL))
#endif
    {
        idlOS::printf("Write PortNo (default:%d) : ", sDefPortNo);
        idlOS::fflush(stdout);
        idlOS::gets(szInStr, sizeof(szInStr));

        if (idlOS::strlen(szInStr) == 0)
        {
            m_PortNum = sDefPortNo;
        }
        else
        {
            m_PortNum = idlOS::atoi(szInStr);
        }
        m_bExist_PORT = SQL_TRUE;
    }

    if (m_bExist_U == SQL_FALSE)
    {
        idlOS::printf("Write UserID : ");
        idlOS::fflush(stdout);
        idlOS::gets(szInStr, sizeof(szInStr));

        m_bExist_U = SQL_TRUE;
        /* BUG-17563 : iloader ���� ū����ǥ �̿��� Naming Rule ���� ����  */
        /*    Interactive ����� ��쿡�� userID�� case�� "..."�� ��������.
         *    - Quoted Name�� ���
         *      : �״�� ��� - "Quoted Name" ==> "Quoted Name"
         *    - Non-Quoted Name�� ���
         *      : �빮�ڷ� ���� - NonQuotedName ==> NONQUOTEDNAME
        */
        utString::makeNameInCLI(m_LoginID,
                                ID_SIZEOF(m_LoginID),
                                szInStr,
                                idlOS::strlen(szInStr));
    }

    if (m_bExist_P == SQL_FALSE)
    {
        m_bExist_P = SQL_TRUE;

        idlOS::strcpy(m_Password, getpass("Write Password : "));
    }

    // BUG-26287: �ɼ� ó����� ����
    // -NLS_USE �ɼ� �߰�
    if (m_bExist_NLS == ILO_FALSE)
    {
        // BUG-24126 isql ���� ALTIBASE_NLS_USE ȯ�溯���� ��� �⺻ NLS�� �����ϵ��� �Ѵ�.
        // ����Ŭ�� �����ϰ� US7ASCII �� �մϴ�.
        idlOS::strncpy(m_NLS, "US7ASCII", ID_SIZEOF(m_NLS));
        m_bExist_NLS = ILO_TRUE;
    }

    // BUG-25359 iloader ���� download condition �� ������� �ʽ��ϴ�.
    // ���� : form file ���� �⺻���� �����ϰ� �־���
    // form file �ļ� ������ �⺻���� �����Ҽ� �����Ƿ� ���⼭ �����ϵ��� ������
    idlOS::strcpy(m_DataNLS, m_NLS);

    return SQL_TRUE;

}

// BUG-26287: �ɼ� ó����� ����
// altibase.properties�� �������� �ʴ°� ����.
IDE_RC iloProgOption::ReadEnvironment( ALTIBASE_ILOADER_HANDLE aHandle )
{
    SChar *sCharData;
    
    /* BUG-47652 Set file permission */
    SChar  sEnvVarName[ENV_NAME_LEN+1];
    
    iloaderHandle *sHandle = (iloaderHandle *) aHandle;

    if (m_bExist_PORT == SQL_FALSE)
    {
        /* BUG-41406 SSL */
        if (m_ConnType == ILO_CONNTYPE_SSL)
        {
            sCharData = idlOS::getenv(ENV_ALTIBASE_SSL_PORT_NO);
        }
        else
        {
            sCharData = idlOS::getenv(ENV_ALTIBASE_PORT_NO);
        }
        if (sCharData != NULL)
        {
            m_PortNum = idlOS::atoi(sCharData);
            m_bExist_PORT = SQL_TRUE;
        }
    }
    if (m_bExist_NLS == ILO_FALSE)
    {
        sCharData = idlOS::getenv(ALTIBASE_ENV_PREFIX"NLS_USE");
        if (sCharData != NULL)
        {
            idlOS::strncpy(m_NLS, sCharData, ID_SIZEOF(m_NLS));
            m_bExist_NLS = ILO_TRUE;
        }
    }
    
    /* BUG-47652 Set file permission */
    if (mbExistFilePerm == ID_FALSE)
    {
        idlOS::sprintf( sEnvVarName, "%s", ENV_ALTIBASE_UT_FILE_PERMISSION );
        sCharData = idlOS::getenv( sEnvVarName );
        IDE_TEST_RAISE ( uttEnv::setFilePermission( sCharData,
                                                    &gFilePerm,
                                                    &mbExistFilePerm ) != IDE_SUCCESS, 
                         FilePerm_error );

        idlOS::sprintf( sEnvVarName, "%s", ENV_ILO_FILE_PERMISSION );
        sCharData = idlOS::getenv( sEnvVarName );
        IDE_TEST_RAISE ( uttEnv::setFilePermission( sCharData,
                                                    &gFilePerm,
                                                    &mbExistFilePerm ) != IDE_SUCCESS, 
                         FilePerm_error );
    }

    return IDE_SUCCESS;
    
    /* BUG-47652 Set file permission */
    IDE_EXCEPTION(FilePerm_error);
    {
        uteSetErrorCode( sHandle->mErrorMgr, utERR_ABORT_FilePerm_OutOfRange_Error, 
                         sEnvVarName, sCharData );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

// BUG-26287: �ɼ� ó����� ����
// ������ ��ġ�� ��� ȯ�溯���� �������� �ʰ� altibase.properties�� �����ؼ�
// �� �� �����Ƿ� altibase.properties�� ������ �о�������ؾ�
// ���� ��ũ��Ʈ���� ������ �ȳ���.
void iloProgOption::ReadServerProperties()
{
    IDE_RC  sRead;
    UInt    sIntData;
    SChar  *sCharData;

    IDE_TEST(idp::initialize() != IDE_SUCCESS);

    (void) iduProperty::load();

    if (m_bExist_PORT == SQL_FALSE)
    {
        if (m_ConnType == ILO_CONNTYPE_SSL)
        {
            sRead = idp::read("SSL_PORT_NO", (void *)&sIntData, 0);
        }
        else if ( m_ConnType== ILO_CONNTYPE_IPCDA ) // BUG-44094
        {
            sRead = idp::read(PROPERTY_IPCDA_PORT_NO, (void *)&sIntData, 0);
        }
        else
        {
            sRead = idp::read(PROPERTY_PORT_NO, (void *)&sIntData, 0);
        }
        if (sRead == IDE_SUCCESS)
        {
            m_PortNum = sIntData;
            m_bExist_PORT = SQL_TRUE;
        }
    }
    
    if (m_bExist_NLS == ILO_FALSE)
    {
        sRead = idp::readPtr("NLS_USE", (void **)&sCharData, 0);
        if (sRead == IDE_SUCCESS)
        {
            idlOS::strncpy(m_NLS, sCharData, ID_SIZEOF(m_NLS));
            m_bExist_NLS = ILO_TRUE;
        }
    }

    sRead = idp::readPtr("DB_NAME", (void **)&sCharData, 0);
    if (sRead == IDE_SUCCESS)
    {
        idlOS::strncpy(m_DBName, sCharData, ID_SIZEOF(m_DBName));
    }

    IDE_EXCEPTION_END;
}

SInt iloProgOption::IsValidOption( ALTIBASE_ILOADER_HANDLE aHandle )
{
    iloaderHandle *sHandle = (iloaderHandle *) aHandle;

    IDE_TEST_RAISE( mInvalidOption == SQL_TRUE, err_option );

    switch (m_CommandType)
    {
    case NON_COM :
        return SQL_FALSE;
    case EXIT_COM :
    case HELP_COM :
        break;
    case DATA_IN :
        IDE_TEST_RAISE( m_bExist_f == SQL_FALSE, err_form );
        IDE_TEST_RAISE( m_bExist_d == SQL_FALSE, err_data );
        IDE_TEST_RAISE(ValidateLOBOptions() != IDE_SUCCESS, LOBOptStrErr);
        IDE_TEST_RAISE( ValidTermString() == SQL_FALSE, err_term );
        IDE_TEST_RAISE( m_bExist_split == SQL_TRUE , err_split );
#ifdef COMPILE_SHARDCLI
        IDE_TEST_RAISE( m_bExist_array == SQL_TRUE && m_ArrayCount > 1,
                        err_array );
        IDE_TEST_RAISE( m_bExist_atomic == SQL_TRUE, err_atomic );
        IDE_TEST_RAISE( m_bExist_direct == SQL_TRUE, err_direct );
        IDE_TEST_RAISE( m_bExist_ioParallel == SQL_TRUE, err_ioparallel );
        IDE_TEST_RAISE( m_LoadMode == ILO_REPLACE ||
                        m_LoadMode == ILO_TRUNCATE, err_mode );
#else /* COMPILE_SHARDCLI */
        IDE_TEST_RAISE( m_bExist_array == SQL_TRUE && m_ArrayCount <= 0,
                        err_array );
        IDE_TEST_RAISE( ( m_bExist_array == SQL_FALSE ) &&
                        ( m_bExist_atomic == SQL_TRUE ), err_atomic );
        //PROJ-1760
        // -ioparallel�� -direct �ɼǰ� �Բ� ����ؾ߸� �Ѵ�.
        IDE_TEST_RAISE( ( m_bExist_direct == SQL_FALSE ) &&
                        ( m_bExist_ioParallel == SQL_TRUE), err_ioparallel );
#endif /* COMPILE_SHARDCLI */
        /* BUG-43277 -prefetch_rows option */
        IDE_TEST_RAISE( m_bExist_prefetch_rows == ID_TRUE, err_prefetch_rows );

        /* BUG-44187 Support Asynchronous Prefetch */
        IDE_TEST_RAISE( m_bExist_async_prefetch == ID_TRUE, err_async_prefetch );
        break;
    case DATA_OUT :
        IDE_TEST_RAISE( m_bExist_f == SQL_FALSE, err_form );
        IDE_TEST_RAISE( m_bExist_d == SQL_FALSE, err_data );
        IDE_TEST_RAISE(ValidateLOBOptions() != IDE_SUCCESS, LOBOptStrErr);
        IDE_TEST_RAISE( ValidTermString() == SQL_FALSE, err_term );
        break;
    case FORM_OUT :
        IDE_TEST_RAISE( m_bExist_f == SQL_FALSE, err_form );
        IDE_TEST_RAISE( m_bExist_T == SQL_FALSE, err_table );
        break;
    case STRUCT_OUT :
        IDE_TEST_RAISE( m_bExist_f == SQL_FALSE, err_form );
        IDE_TEST_RAISE( m_bExist_T == SQL_FALSE, err_table );
        break;
    default :
        break;
    }

    return SQL_TRUE;

    IDE_EXCEPTION( err_data );
    {
        m_bErrorExist = SQL_TRUE;
        uteSetErrorCode(sHandle->mErrorMgr, utERR_ABORT_Option_Incorrect_Error,
                        "-d",
                        "data file name"
                        );

    }
    IDE_EXCEPTION( err_term );
    {
        m_bErrorExist = SQL_TRUE;
        uteSetErrorCode(sHandle->mErrorMgr, utERR_ABORT_Field_Terminator_Error );
    }
    IDE_EXCEPTION( err_form );
    {
        m_bErrorExist = SQL_TRUE;
        uteSetErrorCode(sHandle->mErrorMgr, utERR_ABORT_Option_Incorrect_Error,
                        "-f",
                        "form file name"
                        );
    }
    IDE_EXCEPTION( err_table );
    {
        m_bErrorExist = SQL_TRUE;
        uteSetErrorCode(sHandle->mErrorMgr, utERR_ABORT_Option_Incorrect_Error,
                        "-T",
                        "Table name"
                        );
    }
    IDE_EXCEPTION( err_split );
    {
        m_bErrorExist = SQL_TRUE;
        uteSetErrorCode(sHandle->mErrorMgr, utERR_ABORT_Option_Incorrect_Error,
                        "-split",
                        "SplitRowCount"
                        );
    }
#ifdef COMPILE_SHARDCLI
    IDE_EXCEPTION( err_array );
    {
        m_bErrorExist = SQL_TRUE;
        uteSetErrorCode(sHandle->mErrorMgr, utERR_ABORT_Invalid_Option_Error,
                        "-array");
    }
    IDE_EXCEPTION( err_atomic );
    {
        m_bErrorExist = SQL_TRUE;
        uteSetErrorCode(sHandle->mErrorMgr, utERR_ABORT_Invalid_Option_Error,
                        "-atomic");
    }
    IDE_EXCEPTION( err_direct );
    {
        m_bErrorExist = SQL_TRUE;
        uteSetErrorCode(sHandle->mErrorMgr, utERR_ABORT_Invalid_Option_Error,
                        "-direct");
    }
    IDE_EXCEPTION( err_ioparallel );
    {
        m_bErrorExist = SQL_TRUE;
        uteSetErrorCode(sHandle->mErrorMgr, utERR_ABORT_Invalid_Option_Error,
                        "-ioparallel");
    }
    IDE_EXCEPTION( err_mode );
    {
        m_bErrorExist = SQL_TRUE;
        uteSetErrorCode(sHandle->mErrorMgr, utERR_ABORT_Invalid_Option_Error,
                        "-mode REPLACE | TRUNCATE");
    }
#else /* COMPILE_SHARDCLI */
    IDE_EXCEPTION( err_array );
    {
        m_bErrorExist = SQL_TRUE;
        uteSetErrorCode(sHandle->mErrorMgr, utERR_ABORT_Option_Incorrect_Error,
                        "-array",
                        "ArrayCount"
                        );
    }
    IDE_EXCEPTION( err_atomic );
    {
        m_bErrorExist = SQL_TRUE;
        uteSetErrorCode(sHandle->mErrorMgr, utERR_ABORT_Option_Incorrect_Error,
                        "-atomic",
                        "must use with -array option"
                        );
    }
    IDE_EXCEPTION( err_ioparallel );
    {
        m_bErrorExist = SQL_TRUE;
        uteSetErrorCode(sHandle->mErrorMgr, utERR_ABORT_Option_Incorrect_Error,
                        "-ioparallel",
                        "must use with -direct option"
                        );        
    }    
#endif /* COMPILE_SHARDCLI */
    IDE_EXCEPTION(LOBOptStrErr);
    {
        m_bErrorExist = SQL_TRUE;
        uteSetErrorCode(sHandle->mErrorMgr, utERR_ABORT_LOB_Opt_Str_Error);
    }
    IDE_EXCEPTION( err_option );
    {
        m_bErrorExist = SQL_TRUE;
    }
    IDE_EXCEPTION( err_prefetch_rows );
    {
        m_bErrorExist = SQL_TRUE;
        uteSetErrorCode(sHandle->mErrorMgr, utERR_ABORT_Invalid_Option_Error,
                        "-prefetch_rows"
                        );
    }
    IDE_EXCEPTION( err_async_prefetch );
    {
        m_bErrorExist = SQL_TRUE;
        uteSetErrorCode(sHandle->mErrorMgr, utERR_ABORT_Invalid_Option_Error,
                        "-async_prefetch"
                        );
    }
    IDE_EXCEPTION_END;

    return SQL_FALSE;
}

/* Return Value
 * COMMAND_INVALID   -1
 * COMMAND_PROMPT     0
 * COMMAND_VALID      1
 * COMMAND_HELP       2
 */
SInt iloProgOption::TestCommandLineOption( ALTIBASE_ILOADER_HANDLE aHandle )
{

    iloaderHandle *sHandle = (iloaderHandle *) aHandle;
    
    IDE_TEST_RAISE( mInvalidOption == SQL_TRUE, err_option );

    switch (m_CommandType)
    {
    case NON_COM :
        return COMMAND_PROMPT;
    case EXIT_COM :
        return COMMAND_INVALID;
    case HELP_COM :
        return COMMAND_HELP;
    case DATA_IN :
        IDE_TEST_RAISE( m_bExist_f == SQL_FALSE, err_form );
        IDE_TEST_RAISE( m_bExist_d == SQL_FALSE, err_data );
        IDE_TEST_RAISE(ValidateLOBOptions() != IDE_SUCCESS, LOBOptStrErr);
        IDE_TEST_RAISE( ValidTermString() == SQL_FALSE, err_term );
        IDE_TEST_RAISE( m_bExist_split == SQL_TRUE , err_split );
#ifdef COMPILE_SHARDCLI
        IDE_TEST_RAISE( m_bExist_array == SQL_TRUE && m_ArrayCount > 1,
                        err_array );
        IDE_TEST_RAISE( m_bExist_atomic == SQL_TRUE, err_atomic );
        IDE_TEST_RAISE( m_bExist_direct == SQL_TRUE, err_direct );
        IDE_TEST_RAISE( m_bExist_ioParallel == SQL_TRUE, err_ioparallel );
        IDE_TEST_RAISE( m_LoadMode == ILO_REPLACE ||
                        m_LoadMode == ILO_TRUNCATE, err_mode );
#else /* COMPILE_SHARDCLI */
        IDE_TEST_RAISE( m_bExist_array == SQL_TRUE && m_ArrayCount <= 0,
                        err_array );
        IDE_TEST_RAISE( m_bExist_split == SQL_TRUE , err_split );
        if (mExistWaitTime == SQL_TRUE)
        {
            IDE_TEST_RAISE( mExistWaitCycle == SQL_FALSE, err_wait );
        }
        IDE_TEST_RAISE( ( m_bExist_array == SQL_FALSE ) &&
                        ( m_bExist_atomic == SQL_TRUE ), err_atomic );

        //PROJ-1760
        // 1. -ioparallel�� -direct �ɼǰ� �Բ� ����ؾ߸� �Ѵ�.
        IDE_TEST_RAISE( ( m_bExist_direct == SQL_FALSE ) &&
                        ( m_bExist_ioParallel == SQL_TRUE), err_ioparallel );
#endif /* COMPILE_SHARDCLI */
        /* BUG-43277 -prefetch_rows option */
        IDE_TEST_RAISE( m_bExist_prefetch_rows == ID_TRUE , err_prefetch_rows );

        /* BUG-44187 Support Asynchronous Prefetch */
        IDE_TEST_RAISE( m_bExist_async_prefetch == ID_TRUE, err_async_prefetch );
        break;
    case DATA_OUT :
        IDE_TEST_RAISE( m_bExist_f == SQL_FALSE, err_form );
        IDE_TEST_RAISE( m_bExist_d == SQL_FALSE, err_data );
        IDE_TEST_RAISE(ValidateLOBOptions() != IDE_SUCCESS, LOBOptStrErr);
        IDE_TEST_RAISE( ValidTermString() == SQL_FALSE, err_term );
        if (mExistWaitTime == SQL_TRUE)
        {
            IDE_TEST_RAISE( mExistWaitCycle == SQL_FALSE, err_wait );
        }
        break;
    case FORM_OUT :
        IDE_TEST_RAISE( m_bExist_f == SQL_FALSE, err_form );
        IDE_TEST_RAISE( m_bExist_T == SQL_FALSE, err_table );
        break;
    case STRUCT_OUT :
        IDE_TEST_RAISE( m_bExist_f == SQL_FALSE, err_form );
        IDE_TEST_RAISE( m_bExist_T == SQL_FALSE, err_table );
        break;
    default :
        break;
    }

    return COMMAND_VALID;

    IDE_EXCEPTION( err_data );
    {
        m_bErrorExist = SQL_TRUE;
        uteSetErrorCode(sHandle->mErrorMgr, utERR_ABORT_Option_Incorrect_Error,
                        "-d",
                        "data file name"
                        );
    }
    IDE_EXCEPTION( err_term );
    {
        m_bErrorExist = SQL_TRUE;
        uteSetErrorCode(sHandle->mErrorMgr, utERR_ABORT_Field_Terminator_Error );
    }
    IDE_EXCEPTION( err_form );
    {
        m_bErrorExist = SQL_TRUE;
        uteSetErrorCode(sHandle->mErrorMgr, utERR_ABORT_Option_Incorrect_Error,
                        "-f",
                        "form file name"
                        );
    }
    IDE_EXCEPTION( err_table );
    {
        m_bErrorExist = SQL_TRUE;
        uteSetErrorCode(sHandle->mErrorMgr, utERR_ABORT_Option_Incorrect_Error,
                        "-T",
                        "Table name"
                        );
    }
    IDE_EXCEPTION( err_split );
    {
        m_bErrorExist = SQL_TRUE;
        uteSetErrorCode(sHandle->mErrorMgr, utERR_ABORT_Option_Incorrect_Error,
                        "-split",
                        "SplitRowCount"
                        );
    }
    IDE_EXCEPTION( err_wait );
    {
        m_bErrorExist = SQL_TRUE;
        uteSetErrorCode(sHandle->mErrorMgr, utERR_ABORT_Option_Incorrect_Error,
                        "-waittime",
                        "waittime option must use with -waitcycle"
                        );
    }
#ifdef COMPILE_SHARDCLI
    IDE_EXCEPTION( err_array );
    {
        m_bErrorExist = SQL_TRUE;
        uteSetErrorCode(sHandle->mErrorMgr, utERR_ABORT_Invalid_Option_Error,
                        "-array");
    }
    IDE_EXCEPTION( err_atomic );
    {
        m_bErrorExist = SQL_TRUE;
        uteSetErrorCode(sHandle->mErrorMgr, utERR_ABORT_Invalid_Option_Error,
                        "-atomic");
    }
    IDE_EXCEPTION( err_direct );
    {
        m_bErrorExist = SQL_TRUE;
        uteSetErrorCode(sHandle->mErrorMgr, utERR_ABORT_Invalid_Option_Error,
                        "-direct");
    }
    IDE_EXCEPTION( err_ioparallel );
    {
        m_bErrorExist = SQL_TRUE;
        uteSetErrorCode(sHandle->mErrorMgr, utERR_ABORT_Invalid_Option_Error,
                        "-ioparallel");
    }
    IDE_EXCEPTION( err_mode );
    {
        m_bErrorExist = SQL_TRUE;
        uteSetErrorCode(sHandle->mErrorMgr, utERR_ABORT_Invalid_Option_Error,
                        "-mode REPLACE | TRUNCATE");
    }
#else /* COMPILE_SHARDCLI */
    IDE_EXCEPTION( err_array );
    {
        m_bErrorExist = SQL_TRUE;
        uteSetErrorCode(sHandle->mErrorMgr, utERR_ABORT_Option_Incorrect_Error,
                        "-array",
                        "ArrayCount"
                        );
    }
    IDE_EXCEPTION( err_atomic );
    {
        m_bErrorExist = SQL_TRUE;
        uteSetErrorCode(sHandle->mErrorMgr, utERR_ABORT_Option_Incorrect_Error,
                        "-atomic",
                        "must use with -array option"
                        );
    }
    IDE_EXCEPTION( err_ioparallel );
    {
        m_bErrorExist = SQL_TRUE;
        uteSetErrorCode(sHandle->mErrorMgr, utERR_ABORT_Option_Incorrect_Error,
                        "-ioparallel",
                        "must use with -direct option"
                        );        
    }    
#endif /* COMPILE_SHARDCLI */
    IDE_EXCEPTION(LOBOptStrErr);
    {
        m_bErrorExist = SQL_TRUE;
        uteSetErrorCode(sHandle->mErrorMgr, utERR_ABORT_LOB_Opt_Str_Error);
    }
    IDE_EXCEPTION( err_option );
    {
        m_bErrorExist = SQL_TRUE;
    }
    IDE_EXCEPTION( err_prefetch_rows );
    {
        m_bErrorExist = SQL_TRUE;
        uteSetErrorCode(sHandle->mErrorMgr, utERR_ABORT_Invalid_Option_Error,
                        "-prefetch_rows"
                        );
    }
    IDE_EXCEPTION( err_async_prefetch );
    {
        m_bErrorExist = SQL_TRUE;
        uteSetErrorCode(sHandle->mErrorMgr, utERR_ABORT_Invalid_Option_Error,
                        "-async_prefetch"
                        );
    }
    IDE_EXCEPTION_END;

    return COMMAND_INVALID;
}

/**
 * ValidateLOBOptions.
 *
 * ����ڰ� �Է��� ��ɿ��� LOB �ɼǵ��� �߸��� ���� ���°� �˻��Ѵ�.
 */
IDE_RC iloProgOption::ValidateLOBOptions()
{
    if (mExistLOBFileSize == ILO_TRUE)
    {
        IDE_TEST(mExistUseLOBFile == ILO_TRUE && mUseLOBFile == ILO_FALSE);
        IDE_TEST(mLOBFileSize == ID_ULONG(0));
        mUseLOBFile = ILO_TRUE;
    }
    if (mExistUseSeparateFiles == ILO_TRUE)
    {
        IDE_TEST(mExistUseLOBFile == ILO_TRUE && mUseLOBFile == ILO_FALSE);
        IDE_TEST(mExistLOBFileSize == ILO_TRUE && mUseSeparateFiles == ILO_TRUE);
        mUseLOBFile = ILO_TRUE;
    }
    if (mExistLOBIndicator == ILO_TRUE)
    {
        IDE_TEST(mExistUseLOBFile == ILO_TRUE && mUseLOBFile == ILO_FALSE);
        mUseLOBFile = ILO_TRUE;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

SInt iloProgOption::ValidTermString()
{
    IDE_TEST( idlOS::strcmp(m_FieldTerm, m_RowTerm) == 0 );
    if (m_bExist_e)
    {
        IDE_TEST( idlOS::strcmp(m_FieldTerm, m_EnclosingChar) == 0 );
        IDE_TEST( idlOS::strcmp(m_RowTerm, m_EnclosingChar) == 0 );
        if (mUseLOBFile)
        {
            IDE_TEST( idlOS::strcmp(m_EnclosingChar, mLOBIndicator) == 0 );
        }
    }
    if (mUseLOBFile)
    {
        IDE_TEST( idlOS::strcmp(m_FieldTerm, mLOBIndicator) == 0 );
        IDE_TEST( idlOS::strcmp(m_RowTerm, mLOBIndicator) == 0 );
    }

    return SQL_TRUE;

    IDE_EXCEPTION_END;

    return SQL_FALSE;
}

void iloProgOption::ResetError(void)
{
    m_bErrorExist = SQL_FALSE;
}

/**
 * StrToUpper.
 *
 *
 * ���ڿ��� �빮�ڷ� �����.
 *
 * @param[in,out] aStr
 *  �빮�ڷ� ���� ���ڿ�.
 */
void iloProgOption::StrToUpper(SChar *aStr)
{
    UChar *sPtr;

    for (sPtr = (UChar *)aStr; *sPtr != '\0'; sPtr++)
    {
        if (97 <= *sPtr && *sPtr <= 122)
        {
            *sPtr -= 32;
        }
    }
}

/* PROJ-1714
 * ������ ������ �Է¹޴´�.
 * Parallel�� �ִ� �������� �Է� ���� �� �ִ�.
 */
 
SInt iloProgOption::AddDataFileName( SChar *aFileName )
{    
    IDE_TEST ( m_DataFileNum > MAX_PARALLEL_COUNT );
    IDE_TEST ( idlOS::strcpy(m_DataFile[m_DataFileNum], aFileName) == 0 );
    
#ifdef _ILOADER_DEBUG
    idlOS::printf("INSERTED DATAFILE [%s] [%d]\n", m_DataFile[m_DataFileNum], m_DataFileNum);
#endif
    
    m_DataFileNum++;

    return SQL_TRUE;

    IDE_EXCEPTION_END;

    return SQL_FALSE;    
}

/*
 * PROJ-1714
 * ����ڰ� �Է��� ���� �̸��� ���������� ��� ���ؼ��� ID_TRUE���� (Data Uploading ���� ����)
 * ����ڰ� �Է��� ���� �̸� �ϳ��� ��������� ��� ���ؼ��� ID_FALSE���� ���(Data Downloading���� ����)  
 */

SChar* iloProgOption::GetDataFileName( iloBool aIsUpload )
{
    IDE_TEST ( (m_DataFilePos >= MAX_PARALLEL_COUNT) || (m_DataFilePos > m_DataFileNum) );
    
    if ( aIsUpload == ILO_TRUE )
    {
        return m_DataFile[m_DataFilePos++];
    }
    else
    {
        return m_DataFile[m_DataFilePos];
    }
    
    IDE_EXCEPTION_END;

    return SQL_FALSE; 
}


/* BUG-30693 : table �̸���� owner �̸��� mtlMakeNameInFunc �Լ��� �̿��Ͽ�
               �빮�ڷ� �����ؾ� �� ��� ������.
   CommandParser���� ��ȯ�ϸ� �ȵȴ�. �� ������ ulnDbcInitialize �Լ��� ȣ��Ǳ� ���̶�
   ������ ASCII ��� ���ֵǱ� ������, SHIFTJIS�� ���� ���ڵ��� ���ڿ��� ������� �빮�� ��ȯ��
   �߸��� �� �ִ�.
*/
void iloProgOption::makeTableNameInCLI(void)
{
    SInt  i;
    SChar sTableName[MAX_OBJNAME_LEN];
    SChar sTableOwner[MAX_OBJNAME_LEN];

    for( i = 0 ; i < m_nTableCount ; i++  )
    {
        utString::makeNameInCLI( sTableName,
                                 MAX_OBJNAME_LEN,
                                 m_TableName[i],
                                 idlOS::strlen(m_TableName[i]) );
        idlOS::snprintf( m_TableName[i],
                         MAX_OBJNAME_LEN,
                         "%s", sTableName
                       );
    }

    if( m_bExist_TabOwner == SQL_TRUE )
    {
        utString::makeNameInCLI( sTableOwner,
                                 MAX_OBJNAME_LEN,
                                 m_TableOwner[0],
                                 idlOS::strlen(m_TableOwner[0]) );
        idlOS::snprintf( m_TableOwner[0],
                         MAX_OBJNAME_LEN,
                         "%s", sTableOwner
                       );
    }
}

