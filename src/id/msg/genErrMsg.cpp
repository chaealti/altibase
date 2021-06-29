/*****************************************************************************
 * Copyright 1999-2000, RTBase Corporation or its subsidiaries.
 * All rights reserved.
 ****************************************************************************/

/*****************************************************************************
 * $Id: genErrMsg.cpp 71017 2015-05-28 07:54:48Z sunyoung $
 *
 * DESC : �����ڵ� ���� ��ũ��Ʈ ȭ���� �Է� �޾� �����ڵ� semi-h ���ȭ��
 *        �� �����޽��� ����Ÿȭ�� ����
 *
 ****************************************************************************/

// ���� �޽����� ������ �� �����ڵ� ȭ���� include ���� �ʴ´�.
#define ID_ERROR_CODE_H

#include <idl.h>
#include <idn.h>
#include <ideErrorMgr.h>
#include <iduVersion.h>

// MSB ȭ�Ͽ� �����ڵ带 ���´�.
//#define MSB_ECODE_ON
/*
 *                      (ALTIBASE 4.0)
 *
 *         0   1   2   3   4   5   6    7    8    9
 *        id  sm  mt  qp  mm  ul  rp  none none  ut
 */


/* ----------------------------------------------------------------------
 *
 * ���� ����
 *
 * ----------------------------------------------------------------------*/

extern ideErrTypeInfo typeInfo[];
void   doTraceGeneration();

idErrorMsbType msbHeader;

FILE *inMsgFP;  // �Է� ȭ��

FILE *outErrHeaderFP;
FILE *outTrcHeaderFP;
FILE *outTrcCppFP;
FILE *outMsbFP;

SChar *inMsgFile = NULL;

SChar *outErrHeaderFile = NULL;
SChar  outMsbFile[256];
SChar  outTrcCppFile[256];
SChar  outTrcHeaderFile[256];



// Server�� ��� MSB, Clinet�� ��� C �ҽ��ڵ�� ����.
const SChar *gExtName[] =
{
    "msb", "c"
};



SChar *Usage =
(SChar *)"Usage :  "PRODUCT_PREFIX"genErrMsg [-n] -i INFILENAME -o HeaderFile\n\n"
"         -n : generatate Error Number\n"
"         -i : specify ErrorCode and Message input file [.msg]\n"
"         -o : specify to store Error Code Header File \n"
"         Caution!!) INFILENAME have to follow \n"
"                    the NLS Naming convention. (refer idnLocal.h) \n"
"                    ex) (o) E_SM_US7ASCII.msg, (o) E_QP_US7ASCII.msg, \n"
"                        (o) E_CM_US7ASCII.msg  (x) E_SM_ASCII.msg \n\n"
;

SInt Section       = -1;    // ���� ��ȣ
SInt StartNum      = -1;
SInt errIdxNum     =  0;
SInt PrintNumber   =  1; // �������� �����Ѵ�.
UInt HexErrorCode;
UInt gClientPart   = 0;
UInt gDebug        = 0;
UInt gMsgOnly      = 0; /* BUG-41330 */

SChar *Header =
(SChar *)"/*   This File was created automatically by "PRODUCT_PREFIX"genErrMsg Utility */\n\n";

SChar *StringError =
(SChar *)" ���� �ڵ�� ������ ���� �����Ǿ�� �մϴ�.\n"
"\n"
"      SubCode, [MODULE]ERR_[ACTION]_[NamingSpace]_[Description]\n"
"          1           2          3            4\n"
"\n"
"  [MODULE] :  sm | qp | cm | ...\n"
"  [ACTION] :  FATAL | ABORT | IGNORE | RETRY | REBUILD\n"
"  [NamingSpace] : qex | qpf | smd | smc ...\n"
"  [Description] : FileWriteError | ....\n\n"
"   Ex) idERR_FATAL_FATAL_smd_InvalidOidAddress \n\n";
/* ----------------------------------------------------------------------
 *
 * ��Ʈ�� ó�� �Լ�
 *
 * ----------------------------------------------------------------------*/

/* ��Ʈ�� �յڿ� �����ϴ� WHITE-SPACE�� ���� */
static void eraseWhiteSpace(SChar *buffer)
{
    SInt i;
    SInt len = idlOS::strlen(buffer);
    SInt ValueRegion = 0; // ���� �����ϴ� ������ ? �̸�����=������
    SInt firstAscii  = 0; // ���������� ù��° ASCII�� ã�� ��..

    // 1. �տ��� ���� �˻� ����..
    for (i = 0; i < len && buffer[i]; i++)
    {
        if (buffer[i] == '#') // �ּ� ó��
        {
            buffer[i]= 0;
            break;
        }
        if (ValueRegion == 0) // �̸� ���� �˻�
        {
            if (buffer[i] == '=')
            {
                ValueRegion = 1;
                continue;
            }

            if (idlOS::idlOS_isspace(buffer[i])) // �����̽� ��
            {
                SInt j;

                for (j = i;  buffer[j]; j++)
                {
                    buffer[j] = buffer[j + 1];
                }
                i--;
            }
        }
        else // ������ �˻�
        {
            if (firstAscii == 0)
            {
                if (idlOS::idlOS_isspace(buffer[i])) // �����̽� ��
                {
                    SInt j;

                    for (j = i;  buffer[j]; j++)
                    {
                        buffer[j] = buffer[j + 1];
                    }
                    i--;
                }
                else
                {
                    break;
                }
            }

        }
    } // for

    // 2. ������ ���� �˻� ����.. : �����̽� ���ֱ�
    len = idlOS::strlen(buffer);
    for (i = len - 1; buffer[i] && len > 0; i--)
    {
        if (idlOS::idlOS_isspace(buffer[i])) // �����̽� ���ֱ�
        {
            buffer[i]= 0;
            continue;
        }
        break;
    }
}

// �̸��� ���� ����
SInt parseBuffer(SChar *buffer,
                 SInt  *SubCode,
                 SChar **State,
                 SChar **Name,
                 SChar **Value,
                 UInt  aParseSubCode = 1)
{
    SInt i;
    SChar SubCodeBuffer[128];
    SChar *buf;

    *SubCode = -1;

    /* ------------------------
     * [1] White Space ����
     * ----------------------*/
    eraseWhiteSpace(buffer);

    /* ---------------------------------
     * [2] ������ ���ų� �ּ��̸� ����
     * -------------------------------*/
    SInt len = idlOS::strlen(buffer);
    if (len == 0 || buffer[0] == '#')
    {
        return IDE_SUCCESS;
    }


    /* -------------------------
     * [3] SubCode �� ���
     * ------------------------*/
    if (aParseSubCode == 1)
    {
        buf = SubCodeBuffer;
        idlOS::memset(SubCodeBuffer, 0, 128);
        for (i = 0; ; i++)
        {
            SChar c;
            c = buf[i] = buffer[i];
            if (c == ',') // , ����
            {
                *SubCode = (SInt)idlOS::strtol(SubCodeBuffer, NULL, 10);
                buffer += i;
                buffer ++; // [,] �ǳʶٱ�

                break;
            }
            else
                if (c == 0)
                {
                    break;
                }
        }
    }

    /* -------------------------
     * [4] Client�� ��� State  ���
     * ------------------------*/
    if ( (gClientPart == 1) &&
         (gMsgOnly == 0) )
    {
        eraseWhiteSpace(buffer);
        buf = buffer;
        idlOS::memset(SubCodeBuffer, 0, 128);
        for (i = 0; ; i++)
        {
            SChar c;
            c = buf[i] = buffer[i];
            if (c == ',') // , ����
            {
                *State = buf;
                buffer += i;
                *buffer = 0; //[,] ����
                buffer ++; // [,] �ǳʶٱ�
                break;
            }
            else
            {
                if (c == 0)
                {
                    break;
                }
            }
        }
    }

    /* --------------------------
     * [5] Name = Value �� ���
     * -------------------------*/
    eraseWhiteSpace(buffer);
    *Name = buffer; // �̸��� ����
    for (i = 0; i < len; i++)
    {
        if (buffer[i] == '=')
        {
            // �����ڰ� �����ϸ�,
            buffer[i] = 0;

            if (buffer[i + 1])
            {
                *Value = &buffer[i + 1];

                if (idlOS::strlen(&buffer[i + 1]) > 512)
                {
                    // �����Ⱚ�� ����..
                    return IDE_FAILURE;
                }
            }
            return IDE_SUCCESS;
        }
    }
    return IDE_SUCCESS;
}

void ErrorOut(SChar *msg, SInt line, SChar *buffer)
{
    idlOS::printf("\n �Ľ̿��� :: %s(%s:%d)\n\n",
                  msg, buffer, line);

    idlOS::exit(0);
}

void eraseOutputFile()
{
    idlOS::fclose(outErrHeaderFP);
    idlOS::fclose(outMsbFP);
    idlOS::unlink(outErrHeaderFile);
    idlOS::unlink(outMsbFile);
}

UInt getAction(SInt line, SChar *Name)
{
    SChar *p = idlOS::strstr(Name, "ERR_");
    if (p)
    {
        p += 4; // skip ERR_

        if (idlOS::strncasecmp(p, "FATAL", 5) == 0)
        {
            return E_ACTION_FATAL;
        }

        if (idlOS::strncasecmp(p, "ABORT", 5) == 0)
        {
            return E_ACTION_ABORT;
        }

        if (idlOS::strncasecmp(p, "IGNORE", 6) == 0)
        {
            return E_ACTION_IGNORE;
        }

        if (idlOS::strncasecmp(p, "RETRY", 5) == 0)
        {
            return E_ACTION_RETRY;
        }

        if (idlOS::strncasecmp(p, "REBUILD", 7) == 0)
        {
            return E_ACTION_REBUILD;
        }

    }
    idlOS::printf("[%d:%s] �����ڵ��� ACTION �������� ������ �߻��Ͽ����ϴ�.\n%s", line, Name, StringError);
    eraseOutputFile();
    idlOS::exit(0);
    return 0;
}

void CheckValidation(SInt line, SChar *Name, SChar *Value)
{
    SInt  i, under_score = 0;
    SChar *fmt;

    /* ----------------------------------------------
     * [1] Name Check :
     *     [sm|id|qpERR_[Abort|Fatal|Ignore]_[....]
     * --------------------------------------------*/
    for (i = 0; Name[i]; i++)
    {
        if (Name[i] == '_')
            under_score++;
    }

    if (under_score < 2)
    {
        idlOS::printf("[%d:%s] �����ڵ带 �м��ϴµ� ������ �߻��Ͽ����ϴ�.\n%s",
                      line,
                      Name,
                      StringError);
        eraseOutputFile();
        idlOS::exit(0);
    }

    /* ----------------------------------------------
     * [2] Value Check
     *     String [<][0-9][%][udldudsc][>] �� ����
     * --------------------------------------------*/
    fmt = Value;
    SChar c;
    SChar ArgumentFlag[MAX_ARGUMENT]; // �ִ� �������� ����
    SInt  argNum;
    SInt  argCount = 0;

    idlOS::memset(ArgumentFlag, 0, sizeof(SChar) * MAX_ARGUMENT);

    while(( c = *fmt++) )
    {
        SChar numBuf[8]; // ���ڹ�ȣ �Է�

        if (c == '<') // [<] ����
        {
            if (isdigit(*fmt) == 0) // ���ڰ� �ƴ�
            {
                continue;
            }
            for (i = 0; i < 8; i++)
            {
                if (*fmt  == '%')
                {
                    numBuf[i] = 0;
                    break;
                }
                numBuf[i] = *fmt++;
            }
            argNum = (UInt)idlOS::strtol(numBuf, NULL, 10);
            if (argNum >= MAX_ARGUMENT)
            {
                idlOS::printf("[%d:%s] �����ڵ�޽����� �������� ����Ʈ ���� �ʹ� Ů�ϴ�. �ִ� %d.\n",
                              line,
                              Name,
                              MAX_ARGUMENT);
                eraseOutputFile();
                idlOS::exit(0);
            }
            ArgumentFlag[argNum] = 1;

            /* ------------------
             * [2] ����Ÿ�� �˻�
             * -----------------*/
            for (i = 0; ; i++)
            {
                if (typeInfo[i].type == IDE_ERR_NONE)
                {
                    idlOS::printf("[%d:%s] �������� ����Ÿ Ÿ���� ����ġ �ʽ��ϴ�. %s.\n",
                                  line,
                                  Name,
                                  fmt);
                    eraseOutputFile();
                    idlOS::exit(0);
                }
                if (idlOS::strncmp(fmt,
                                   typeInfo[i].tmpSpecStr,
                                   typeInfo[i].len) == 0)
                {
                    fmt += typeInfo[i].len;
                    break;
                }
            }
            if ( *fmt != '>')
            {
                idlOS::printf("[%d:%s] > ǥ�ø� ã�� �� �����ϴ�. [%s].\n",
                              line,
                              Name,
                              fmt);
                eraseOutputFile();
                idlOS::exit(0);
            }
            fmt++;
            argCount++;
        }
    }

    if (argCount >= MAX_ARGUMENT)
    {
        idlOS::printf("[%d:%s] �����ڵ�޽����� ���������� ������ %d�� �ʰ��߽��ϴ�.\n",
                      line,
                      Name,
                      MAX_ARGUMENT);
        eraseOutputFile();
        idlOS::exit(0);
    }
    for (i = 0; i < argCount; i++)
    {
        if (ArgumentFlag[i] == 0)
        {
            idlOS::printf("[%d:%s] �������� ����Ʈ�� �Ϸù�ȣ�� ���� ���� �ֽ��ϴ�. ������� ���ԵǾ����� Ȯ���Ͻʽÿ�. [%d]�� ��ȣ�� ����.\n",
                          line,
                          Name,
                          i);
            eraseOutputFile();
            idlOS::exit(0);
        }
    }


}

/* ----------------------------------------------------------------------
 *
 *  main()
 *
 * ----------------------------------------------------------------------*/


int main(int argc, char *argv[])
{
    int opr;

//     fprintf(stderr, "abd\ \qabd\n");
//     exit(0);

    // �ɼ��� �޴´�.
    while ( (opr = idlOS::getopt(argc, argv, "djafvni:o:ctkm")) != EOF)
    {
        switch(opr)
        {
        case 'd':
            gDebug= 1;
            break;
        case 'i':
            //idlOS::printf(" i occurred arg [%s]\n", optarg);
            inMsgFile = optarg;
            break;
        case 'o':
            //idlOS::printf(" o occurred arg [%s]\n", optarg);
            outErrHeaderFile = optarg;
            break;
        case 'n':
            PrintNumber = 1;
            break;
        case 'v':
            idlOS::fprintf(stdout, "version %s %s %s \n",
                iduVersionString,
                iduGetSystemInfoString(),
                iduGetProductionTimeString());
            idlOS::fflush(stdout);
            idlOS::exit(0); /* just exit after print */
        /* BUG-34010 4�ڸ� ���ڷ� �� ���� ���� �ʿ� */
        case 'a':
            idlOS::fprintf(stdout, "%d.%d.%d.%d", 
                    IDU_ALTIBASE_MAJOR_VERSION,
                    IDU_ALTIBASE_MINOR_VERSION,
                    IDU_ALTIBASE_DEV_VERSION,
                    IDU_ALTIBASE_PATCHSET_LEVEL);
            idlOS::fflush(stdout);
            idlOS::exit(0); /* just exit after print */
        case 'f':
            idlOS::fprintf(stdout, "%d.%d.%d.%d.%d", 
                    IDU_ALTIBASE_MAJOR_VERSION,
                    IDU_ALTIBASE_MINOR_VERSION,
                    IDU_ALTIBASE_DEV_VERSION,
                    IDU_ALTIBASE_PATCHSET_LEVEL,
                    IDU_ALTIBASE_PATCH_LEVEL);
            idlOS::fflush(stdout);
            idlOS::exit(0); /* just exit after print */
        case 'j':
            idlOS::fprintf(stdout, IDU_ALTIBASE_VERSION_STRING);
            idlOS::fflush(stdout);
            idlOS::exit(0); /* just exit after print */
        case 't':
            idlOS::fprintf(stdout,iduGetProductionTimeString());
            idlOS::fflush(stdout);
            idlOS::exit(0); /* just exit after print */
        case 'k': /* package string */
            idlOS::fprintf(stdout, iduGetPackageInfoString());
            idlOS::fflush(stdout);
            idlOS::exit(0); /* just exit after print */
        case 'c':
            // Client Part Error Message Generation
            gClientPart = 1;
            break;
        case 'm': 
            gMsgOnly = 1;
            break;
        }
    }

    // �޽��� ���
    if (inMsgFile == NULL || outErrHeaderFile == NULL)
    {
        idlOS::fprintf(stdout, Usage);
        idlOS::fflush(stdout);
        idlOS::exit(0);
    }


    idlOS::umask(0);

    /* ------------------------------------------------
     *  ������ ȭ�ϸ��� �����Ѵ�.
     * ----------------------------------------------*/

    SInt len = idlOS::strlen(inMsgFile);
    if ( idlOS::strstr(inMsgFile, ".msg") == NULL) // Ȯ���ڰ� ���� �Է�
    {
        idlOS::fprintf(stderr, "you should specify the input msg file \n");
        idlOS::exit(-1);
    }
    else
    {
        /* ------------------------------------------------
         *  [MSB/CPP] ȭ�ϸ� ����
         * ----------------------------------------------*/

        idlOS::strcpy(outMsbFile, inMsgFile);
        outMsbFile[len - 3] = 0;
        idlOS::strcat(outMsbFile, gExtName[gClientPart]);

        /* ------------------------------------------------
         *  Trace Log�� C �ҽ��ڵ� ����
         *
         *  inpurt E_ID_XXXXX.msg => TRC_ID_STRING.ic
         *                        => TRC_ID_STRING.ih
         * ----------------------------------------------*/

        idlOS::snprintf(outTrcCppFile,
                        ID_SIZEOF(outTrcCppFile),
                        "%c%c_TRC_CODE.ic",
                        inMsgFile[2],
                        inMsgFile[3]);
        if (gDebug != 0)
        {
            fprintf(stderr, "TRC=>[%s]\n", outTrcCppFile);
        }


        idlOS::snprintf(outTrcHeaderFile,
                        ID_SIZEOF(outTrcCppFile),
                        "%c%c_TRC_CODE.ih",
                        inMsgFile[2],
                        inMsgFile[3]);

        if (gDebug != 0)
        {
            fprintf(stderr, "TRC=>[%s]\n", outTrcHeaderFile);
        }
    }


    inMsgFP  = idlOS::fopen(inMsgFile, "r");
    if (inMsgFP == NULL)
    {
        idlOS::printf("Can't open file %s\n", inMsgFile);
        idlOS::exit(0);
    }

    /* ------------------------------------------------
     *  ȭ�� Open
     * ----------------------------------------------*/

    outErrHeaderFP = idlOS::fopen(outErrHeaderFile, "wb");
    if (outErrHeaderFP == NULL)
    {
        idlOS::printf("Can't create file %s\n", outErrHeaderFile);
        idlOS::exit(0);
    }

    outTrcHeaderFP = idlOS::fopen(outTrcHeaderFile, "wb");
    if (outTrcHeaderFP == NULL)
    {
        idlOS::printf("Can't create file %s\n", outTrcHeaderFile);
        idlOS::exit(0);
    }

    outTrcCppFP = idlOS::fopen(outTrcCppFile, "wb");
    if (outTrcCppFP == NULL)
    {
        idlOS::printf("Can't create file %s\n", outTrcCppFile);
        idlOS::exit(0);
    }

    outMsbFP = idlOS::fopen(outMsbFile, "wb");
    if (outMsbFP == NULL)
    {
        idlOS::printf("Can't create file %s\n", outMsbFile);
        idlOS::exit(0);
    }

    if (gClientPart == 0)
    {
        // Server�� ��� : �޽��� ȭ�� ��� ����ü ��ŭ ��ŵ
        idlOS::fseek(outMsbFP, sizeof(idErrorMsbType), SEEK_SET);
    }

    SChar buffer[1024];
    SChar dupBuf[E_INDEX_MASK];
    SInt  line = 1;

    idlOS::memset(dupBuf, 0, sizeof(dupBuf));

    while(!feof(inMsgFP))
    {
        SInt   SubCode = 0;
        SChar *State   = NULL;
        SChar *Name    = NULL;
        SChar *Value   = NULL;

        idlOS::memset(buffer, 0, 1024);
        if (idlOS::fgets(buffer, 1024, inMsgFP) == NULL)
        {
            // ȭ���� ������ ����
            break;
        }

        // buffer�� ������ ������ ����
        if (parseBuffer(buffer, &SubCode, &State, &Name, &Value) == -1)
        {
            idlOS::fclose(inMsgFP);
            idlOS::printf("����... ���� [%d]:%s\n", line, buffer);
            idlOS::exit(0);
        }


        if (Section == -1) // Section �������� �ʾ��� ���
        {
            if (Name)
            {
                if (idlOS::strcasecmp(Name, "SECTION") == 0)
                {
                    assert( Value != NULL );
                    Section = (SInt)idlOS::strtol(Value, NULL, 10);
                    if (Section < 0 || Section > 15)
                        ErrorOut((SChar *)"Section�� ������ Ʋ�Ƚ��ϴ�.",
                                 line, buffer);
                    // ȭ�Ͽ� ��� ���
                    idlOS::fprintf(outErrHeaderFP, "%s\n", Header);
                }
            }
        }
        else // Section�� �����Ǿ��� : ������ʹ� ��Ȯ�� N=V �䱸
        {
            if ( (Name && Value == NULL) || (Name == NULL && Value) )
            {
                if ((Name != NULL))
                {
                    if (idlOS::strcmp(Name, "INTERNAL_TRACE_MESSAGE_BEGIN") == 0)
                    {
                        // Internal Trace Message Generation Do.
                        doTraceGeneration();

                        break;
                    }
                }

                ErrorOut((SChar *)"�����ڵ�� �����޽����� ���ÿ� �������� �ʽ��ϴ�.",
                         line, buffer);
            }
            else
            {

                if ( (Name != NULL) && ( Value != NULL) ) // ���� ������
                {
                    if (SubCode < 0)
                    {
                        idlOS::fclose(inMsgFP);
                        idlOS::printf("����:SubCode�� ���� ���ǵ��� ���� �� �����ϴ�. \n line%d:%s\n", line, buffer);
                        idlOS::exit(0);
                    }

                    if (SubCode >= E_INDEX_MASK)
                    {
                        idlOS::fclose(inMsgFP);
                        idlOS::printf("����:SubCode�� ���� ���� ��(%d) �̻��Դϴ�. \n line%d:%s\n", E_INDEX_MASK, line, buffer);
                        idlOS::exit(0);
                    }

                    if (dupBuf[SubCode] != 0)
                    {
                        idlOS::fclose(inMsgFP);
                        idlOS::printf("����:SubCode�� ��(%d)�� �ߺ��Ǿ����ϴ�. \n line%d:%s\n", SubCode, line, buffer);
                        idlOS::exit(0);
                    }

                    dupBuf[SubCode] = 1;

                    CheckValidation(line, Name, Value);

                    UInt Action       = getAction(line, Name);

                    HexErrorCode =
                        ((UInt)Section << 28) | (UInt)Action | ((UInt)SubCode << 12) | (UInt)errIdxNum;
                    // 1. ���ȭ�� ����Ÿ ���� (*.ih)
                    idlOS::fprintf(outErrHeaderFP, "    %s = 0x%08x, /* (0x%x) (%d) */\n",
                                   Name, HexErrorCode, E_ERROR_CODE(HexErrorCode), E_ERROR_CODE(HexErrorCode));

                    if (gClientPart == 0)
                    {
                        // Server Part :  �޽��� ȭ�� ����Ÿ ���� (*.msb)
                        len = idlOS::strlen(Value) + 1;
#ifdef MSB_ECODE_ON
                        idlOS::fprintf(outMsbFP, "0x%08x %s\n", errIdxNum, Value);
#else
                        idlOS::fprintf(outMsbFP, "%s\n", Value);
#endif
                    }
                    else
                    {
                        /* client part */

                        if ( gMsgOnly == 0 )
                        {
                            // Client Part : �ҽ��ڵ� ����
                            idlOS::fprintf(outMsbFP, "    { \"%s\", \"%s\"}, \n", State, Value);
                        }
                        else
                        {
                            /* No state code will be printed in the file. */
                            idlOS::fprintf(outMsbFP, "     \"%s\", \n", Value);
                        }
                    }
                    errIdxNum++;
                }
                else
                {
                    // comment. SKIP
                }

            }
        }
        line++;
    }

    msbHeader.value_.header.AltiVersionId = idlOS::hton((UInt)iduVersionID);
    msbHeader.value_.header.ErrorCount    = idlOS::hton((ULong)errIdxNum);
    msbHeader.value_.header.Section       = idlOS::hton((UInt)Section);

    if (gClientPart == 0)
    {
        idlOS::fseek(outMsbFP, 0, SEEK_SET);
        idlOS::fwrite(&msbHeader, sizeof(msbHeader), 1, outMsbFP);
    }

    idlOS::fclose(inMsgFP);
    idlOS::fclose(outErrHeaderFP);
    idlOS::fclose(outTrcHeaderFP);
    idlOS::fclose(outTrcCppFP);
    idlOS::fclose(outMsbFP);
	return 0;
}


extern ideErrTypeInfo typeInfo[];

/* ------------------------------------------------
 *  Formatted String�� ��ȯ�Ѵ�.
    { IDE_ERR_SCHAR,  "%c" ,  "%c", 2},
    { IDE_ERR_STRING, "%s" ,  "%s", 2},
    { IDE_ERR_SINT,   "%d" ,  "%d", 2},
    { IDE_ERR_UINT,   "%u" ,  "%d", 2},
    { IDE_ERR_SLONG,  "%ld" , "%"ID_INT64_FMT,   3},
    { IDE_ERR_ULONG,  "%lu" , "%"ID_UINT64_FMT,  3},
    { IDE_ERR_VSLONG, "%vd" , "%"ID_vSLONG_FMT,  3},
    { IDE_ERR_VULONG, "%vu" , "%"ID_vULONG_FMT,  3},
    { IDE_ERR_HEX32,  "%x" ,  "%"ID_xINT32_FMT,  2},
    { IDE_ERR_HEX64,  "%lx" , "%"ID_xINT64_FMT,  3},
    { IDE_ERR_HEX_V,  "%vx" , "%"ID_vxULONG_FMT, 3},
    { IDE_ERR_NONE,   "" ,    "", 0},
 * ----------------------------------------------*/
UInt convertFormatString(SChar *sWorkPtr, SChar *sFmt)
{
    UInt            i;

    for ( i = 0; typeInfo[i].len != 0; i++)
    {
        if (idlOS::strcmp(sFmt, typeInfo[i].tmpSpecStr) == 0)
        {
            idlOS::strcpy(sWorkPtr, typeInfo[i].realSpecStr);

            return idlOS::strlen(typeInfo[i].realSpecStr);
        }
    }
    idlOS::fprintf(stderr, "formatted string error : [%s]:%s\n", sWorkPtr, sFmt);
    idlOS::exit(-1);
    return 0;
}

/* ------------------------------------------------
 *  print Error Position
 * ----------------------------------------------*/

void printErrorPosition(SChar *aString, UInt aPos)
{
    UInt j;

    idlOS::fprintf(stderr, "!!!!!!!!!!!!!!! ERROR !!!!!!!!!!!!!!!!!\n");

    for (j = 0; j < idlOS::strlen(aString); j++)
    {
        SChar sChar;

        switch((sChar = aString[j]))
        {
            case '\n':
            case '\r':
            case ' ':
            case '\t':
                sChar = ' ';
                break;
            default:
                break;
        }
        if (j == aPos)
        {
            idlOS::fprintf(stderr, "[ERROR] => %c", sChar);
        }
        idlOS::fprintf(stderr, "%c", sChar);

    }
    idlOS::fprintf(stderr, "\n");
    idlOS::exit(-1);
}



/* ------------------------------------------------
 *  Trace Message�� �����Ѵ�.
 * ----------------------------------------------*/
void doTraceGeneration()
{
    UInt   sSeqNum = 0; // Trace Code�� ��ȣ

    SChar  sLineBuffer[1024];
    SChar *sFormatString;
    SChar *sWorkString;

    // 64k�� �Ѵ� ���� �޽����� ������?
    sFormatString = (SChar *)idlOS::malloc(64 * 1024);
    sWorkString   = (SChar *)idlOS::malloc(64 * 1024);

    assert( (sFormatString != NULL) && (sWorkString != NULL));

    while(!feof(inMsgFP))
    {
        SInt   SubCode = 0;
        SChar *State   = NULL;
        SChar *Name    = NULL;
        SChar *Value   = NULL;

        idlOS::memset(sLineBuffer, 0, 1024);
        if (idlOS::fgets(sLineBuffer, 1024, inMsgFP) == NULL)
        {
            // ȭ���� ������ ����
            break;
        }

        // sLineBuffer�� ������ ������ ����
        if (parseBuffer(sLineBuffer, &SubCode, &State, &Name, &Value, 0) == -1)
        {
            idlOS::fclose(inMsgFP);
            idlOS::printf("����... ���� [%d]:%s\n", 0, sLineBuffer);
            idlOS::exit(0);
        }

        /* ------------------------------------------------
         *  �� ���������� ��Ʈ�� ���� �а� ó����.
         * ----------------------------------------------*/
        if ( (Name != NULL) && (Value != NULL ))
        {
            UInt i;

            idlOS::memset(sFormatString, 0, 64 * 1024);

            /* ------------------------------------------------
             *  1.  ;(�����ݷ�)�� �߻��� �� ���� ����.
             * ----------------------------------------------*/
            i = idlOS::strlen(Value);

            idlOS::strcpy(sFormatString, Value);

            if (sFormatString[i - 1] == ';')
            {
                // remove ;
                sFormatString[i - 1] = 0;
            }
            else
            {
                while(!feof(inMsgFP))
                {
                    SChar sCh;

                    if (idlOS::fread(&sCh, 1, 1, inMsgFP) == 1)
                    {
                        if (sCh == ';')
                        {
                            break;
                        }
                        else
                        {
                            sFormatString[i++] = sCh;
                        }
                    }
                    else
                    {
                        break;
                    }
                }
            }


            /* ------------------------------------------------
             *  2. sFormatString���� " "�ܺο� �ִ� ����
             *     �� " ��ü�� �� �����Ѵ�.
             * ----------------------------------------------*/
            {
                UInt sQuoteState = 0;
                SChar *sWorkPtr = sWorkString;

                idlOS::memset(sWorkString, 0, 64 * 1024);

                for (i = 0; sFormatString[i] != 0; i++)
                {
                    switch(sQuoteState)
                    {
                        case 0: /* non quote state : remove space*/

                            if (sFormatString[i] == '"')
                            {
                                sQuoteState = 1;
                            }
                            else
                            {
                                if (idlOS::idlOS_isspace(sFormatString[i]))
                                {
                                    // OK..skip this
                                }
                                else
                                {
                                    // character occurred in no quote state.
                                    // ex)  "abcd"\n   baaa  "abcd"  <== ERR
                                    printErrorPosition(sFormatString, i);
                                }
                            }

                            break;

                        case 1: /* in quote state : just skip*/
                            if (sFormatString[i] == '"')
                            {
                                if (*(sWorkPtr - 1) == '\\')
                                {
                                    *sWorkPtr++ = sFormatString[i];

                                    // "�� ��Ʈ�� ���ο� ���� ��� " .... \"... "
                                    //*(sWorkPtr - 1) = '"'; remove case
                                }
                                else
                                {
                                    sQuoteState = 0;
                                }

                            }
                            else
                            {
                                *sWorkPtr++ = sFormatString[i];
                            }
                            break;
                    }
                }
            }

            // recover
            idlOS::memcpy(sFormatString, sWorkString, 64 * 1024);

            if (gDebug != 0)
            {
                idlOS::fprintf(stderr, "%s = [%s]\n", Name, sFormatString);
            }


            /* ------------------------------------------------
             *  3. sFormatString���� ��Ʈ�� ��ȯ ����.
             *     => \ escape ���� ��ġ
             *     => �������� <0%d>�� ��ġ
             *
             * ----------------------------------------------*/
            {
                SChar *sWorkPtr = sWorkString;

                idlOS::memset(sWorkString, 0, 64 * 1024);

                for (i = 0; sFormatString[i] != 0; i++)
                {
                    switch(sFormatString[i])
                    {
#ifdef NOTDEF // text -> text ������ �ʿ����. text -> bin�� �ʿ���.
                        case '\\': /* escape sequence char*/
                            switch(sFormatString[i + 1])
                            {
                                case 'n': // /n
                                    *sWorkPtr++ = '\n';
                                    break;
                                case 'r': // /n
                                    *sWorkPtr++ = '\r';
                                    break;
                                case 't': // /n
                                    *sWorkPtr++ = '\t';
                                    break;
                                case '\\': // /n
                                    *sWorkPtr++ = '\\';
                                    break;
                                case '\"': // /n
                                    *sWorkPtr++ = '\\';
                                    break;
                                default:
                                    *sWorkPtr++ = sFormatString[i + 1];
                            }
                            i++; // skip next char

                            break;
#endif
                        case '<': // argument control
                        {
                            UInt  k;
                            UInt  sLen;


                            SChar sFmt[128]; // <...>�� �ִ�ũ�⸦ 128�� ������.

                            idlOS::memset(sFmt, 0, ID_SIZEOF(sFmt));

                            for (k = 1; k < ID_SIZEOF(sFmt); k++)
                            {
                                if (sFormatString[i + k]  == '>')
                                {
                                    break;
                                }
                                sFmt[k - 1] = sFormatString[i + k];
                            }

                            // convert formatted string

                            sLen = convertFormatString(sWorkPtr, sFmt);

                            sWorkPtr += sLen;

                            i += k; // skip to next char

                        }
                        break;
                        case '%':
                            if (sFormatString[ i + 1 ] != '%')
                            {
                                printErrorPosition(sFormatString, i);
                            }
                            else
                            {
                                *sWorkPtr++ = sFormatString[i];
                                *sWorkPtr++ = sFormatString[i + 1];
                                i++; // skip double %% as %
                            }

                            break;

                        default:
                            *sWorkPtr++ = sFormatString[i];
                    }
                }
            }

            // recover
            idlOS::memcpy(sFormatString, sWorkString, 64 * 1024);

            /* ------------------------------------------------
             *  !! �Ϸ�
             *  Name�� sFormatString�� ����Ѵ�.
             * ----------------------------------------------*/
            if (gDebug != 0)
            {
                idlOS::fprintf(stderr, "after [%s]\n", sFormatString);
            }
            idlOS::fprintf(outTrcHeaderFP, "#define  %s  g%c%cTraceCode[%d]\n",
                           Name,
                           inMsgFile[2],
                           inMsgFile[3],
                           sSeqNum);

            idlOS::fprintf(outTrcCppFP, "   \"%s\",\n", sFormatString);

            sSeqNum++;

        }
        else
        {
            if ( (Name != NULL) )
            {
                if (idlOS::strcmp(Name, "INTERNAL_TRACE_MESSAGE_END") == 0)
                {
                    return;
                }

                if (idlOS::strlen(Name) > 0)
                {
                    idlOS::fprintf(stderr, "!!!!!!!!!!!!!!! ERROR !!!!!!!!!!!!!!!!!\n");

                    idlOS::fprintf(stderr, "No Value in [Name] %s \n", Name);

                    idlOS::fprintf(stderr, "Check the format : NAME = VALUE \n");

                    idlOS::exit(-1);
                }
            }
        }
    }
}

