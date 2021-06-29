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
 
/***********************************************************************
 * $Id: ideErrorMgr.cpp 66821 2014-09-23 01:33:48Z youngchul.ma $
 **********************************************************************/

/***********************************************************************
 *	File Name 		:	ideErrorMgr.cpp
 *
 *	Author			:	Sung-Jin, Kim
 *
 *	Related Files	:
 *
 *	Description		:	���� ó�� ���
 *
 *
 **********************************************************************/

/* ----------------------------------------------------------------------------
 * ���� ����� ���� class ��� struct �� plain function ���� ����
 * ---------------------------------------------------------------------------*/

#include <idl.h>
#include <idp.h>

#if defined(VC_WIN32)
#    include <stdio.h>
#endif

#include <ideErrorMgr.h>
#include <iduVersion.h>
#ifndef GEN_ERR_MSG
#include <idErrorCode.h>
#include <ideLog.h>
#endif
#define idERR_IGNORE_NoError 0x42000000
#include <idtContainer.h>

/* ----------------------------------------------------------------------------
 *
 *  PDL ���� ����
 *
 *  Sparc Solaris 2.7���� 64��Ʈ ������ ����� ���
 *  ���� �ý��� �����ڵ��ȣ ������ sys_nerr�� ��������.
 *  �׷���, PDL������ �̰��� ������� �ʰ� ����ϱ� ������
 *  ��ũ������ �߻��Ѵ�. �̰��� �����ϱ� ���� �ӽ÷� sys_nerr�� �����Ѵ�.
 *
 * --------------------------------------------------------------------------*/

#ifdef SPARC_SOLARIS
# ifdef COMPILE_64BIT
#  ifndef __GNUC__
int sys_nerr;
#  endif
# endif
#endif

#if defined(ITRON) || defined(WIN32_ODBC_CLI2)
struct ideErrorMgr *toppers_error = NULL;
#endif

/* TASK-6739 Altibase 710 ���� ���� */
IDTHREAD ideErrorMgr gIdeErrorMgr;

ideErrTypeInfo typeInfo[] = // ���� �޽��� ȭ�Ͽ� ���Ǵ� ����Ÿ Ÿ�� ����
{
    { IDE_ERR_SCHAR   , "%c"  , "%c"  , 2 },
    { IDE_ERR_STRING  , "%s"  , "%s"  , 2 },
    { IDE_ERR_STRING_N, "%.*s", "%.*s", 4 }, /* BUG-45567 */
    { IDE_ERR_SINT    , "%d"  , "%d"  , 2 },
    { IDE_ERR_UINT    , "%u"  , "%u"  , 2 },
    { IDE_ERR_SLONG   , "%ld" , "%lld", 3 },
    { IDE_ERR_ULONG   , "%lu" , "%llu", 3 },
    { IDE_ERR_VSLONG  , "%vd" , "%ld" , 3 },
    { IDE_ERR_VULONG  , "%vu" , "%lu" , 3 },
    { IDE_ERR_HEX32   , "%x"  , "%x"  , 2 },
    { IDE_ERR_HEX64   , "%lx" , "%llx", 3 },
    { IDE_ERR_HEX_V   , "%vx" , "%lx" , 3 },
    { IDE_ERR_NONE    , ""    , ""    , 0 },
};

/* ----------------------------------------------------------------------
 *
 *  Error Code Conversion Matrix
 *
 * ---------------------------------------------------------------------- */

const idBool ideErrorConversionMatrix[IDE_MAX_ERROR_ACTION][IDE_MAX_ERROR_ACTION] = // [OLD][NEW]
{
    //             OLD : FATAL
    //  NEW : FATAL     ABORT     IGNORE    RETRY    REBUILD
    {        ID_FALSE, ID_FALSE, ID_FALSE, ID_FALSE, ID_FALSE },
    //             OLD : ABORT
    //  NEW : FATAL     ABORT     IGNORE    RETRY    REBUILD
    {        ID_TRUE,  ID_FALSE, ID_FALSE, ID_FALSE, ID_FALSE },
    //             OLD : IGNORE
    //  NEW : FATAL     ABORT     IGNORE    RETRY    REBUILD
    {        ID_TRUE,  ID_TRUE,  ID_FALSE, ID_TRUE,  ID_TRUE  },
    //             OLD : RETRY
    //  NEW : FATAL     ABORT     IGNORE    RETRY    REBUILD
    {        ID_TRUE,  ID_TRUE,  ID_FALSE, ID_FALSE, ID_TRUE  },
    //             OLD : REBUILD
    //  NEW : FATAL     ABORT     IGNORE    RETRY    REBUILD
    {        ID_TRUE,  ID_TRUE,  ID_FALSE, ID_FALSE, ID_FALSE },
};

/* ----------------------------------------------------------------------
 *
 *  MT Ŭ���̾�Ʈ�� ���� ������ - shore storage manager ����
 *
 * ---------------------------------------------------------------------- */

// fix BUG-17287

void  ideClearTSDError(ideErrorMgr* aIdErr)
{
    assert(aIdErr != NULL);
    aIdErr->Stack.LastError        = idERR_IGNORE_NoError;
    aIdErr->Stack.LastSystemErrno  = 0;
    aIdErr->Stack.HasErrorPosition = ID_FALSE;
    idlOS::memset(aIdErr->Stack.LastErrorMsg, 0, MAX_ERROR_MSG_LEN);
}

/* PROJ-2617 */
struct ideFaultMgr* ideGetFaultMgr()
{
    return NULL;
}

void ideLogError(SChar* aErrInfo,
                 idBool aAcceptFaultTolerance, /* PROJ-2617 */
                 SChar* aFile,
                 SInt   aLine,
                 SChar* aFormat, ...)
{
    va_list args;

    ACP_UNUSED(aAcceptFaultTolerance);

    va_start(args, aFormat);
    fprintf(stderr, "%s:%d %s failure with errno=%d\n",
            idlVA::basename(aFile), aLine, aErrInfo, errno);
    vfprintf(stderr, aFormat, args);
    va_end(args);
}


/* ----------------------------------------------------------------------
 *
 *  ID ���� �ڵ尪 ��� : SERVER, CLIENT ���� �Լ�
 *
 * ----------------------------------------------------------------------*/

UInt ideGetErrorCode()
{
    ideErrorMgr *iderr = ideGetErrorMgr();
    return iderr->Stack.LastError;
}

SChar* ideGetErrorMsg()
{
    ideErrorMgr *iderr = ideGetErrorMgr();
    return iderr->Stack.LastErrorMsg;
}

SInt ideGetSystemErrno()
{
    ideErrorMgr *iderr = ideGetErrorMgr();
    return iderr->Stack.LastSystemErrno;
}

UInt   ideGetErrorLine()
{
    ideErrorMgr *iderr = ideGetErrorMgr();
    return iderr->Stack.LastErrorLine;
}

SChar *ideGetErrorFile()
{
    ideErrorMgr *iderr = ideGetErrorMgr();
    return iderr->Stack.LastErrorFile;
}

// BUG-38883
idBool ideHasErrorPosition()
{
    ideErrorMgr *iderr = ideGetErrorMgr();
    return iderr->Stack.HasErrorPosition;
}

// BUG-38883
void   ideSetHasErrorPosition()
{
    ideErrorMgr *iderr = ideGetErrorMgr();
    iderr->Stack.HasErrorPosition = ID_TRUE;
}

// BUG-38883
void   ideInitHasErrorPosition()
{
    ideErrorMgr *iderr = ideGetErrorMgr();
    iderr->Stack.HasErrorPosition = ID_FALSE;
}

SInt ideFindErrorCode(UInt ErrorCode)
{
    ideErrorMgr *iderr = ideGetErrorMgr();

    if (iderr->Stack.LastError == ErrorCode)
    {
        return IDE_SUCCESS;
    }
    return IDE_FAILURE;
}

SInt ideFindErrorAction(UInt Action)
{
    ideErrorMgr *iderr = ideGetErrorMgr();

    if ((iderr->Stack.LastError & E_ACTION_MASK) == Action)
    {
        return IDE_SUCCESS;
    }
    return IDE_FAILURE;
}

SInt   ideIsAbort()
{
    return ideFindErrorAction(E_ACTION_ABORT);
}

SInt   ideIsFatal()
{
    return ideFindErrorAction(E_ACTION_FATAL);
}

SInt   ideIsIgnore()
{
    return ideFindErrorAction(E_ACTION_IGNORE);
}

SInt   ideIsRetry()
{
    return ideFindErrorAction(E_ACTION_RETRY);
}

SInt   ideIsRebuild()
{
    return ideFindErrorAction(E_ACTION_REBUILD);
}


/* ----------------------------------------------------------------------
 *
 *  ���� �ڵ尪 ���� + �޽��� ����
 *
 * ----------------------------------------------------------------------*/

IDL_EXTERN_C void ideClearError()
{
    ideErrorMgr *iderr = ideGetErrorMgr();

    assert(iderr != NULL);

    iderr->Stack.LastError        = idERR_IGNORE_NoError;
    iderr->Stack.LastSystemErrno  = 0;
    iderr->Stack.HasErrorPosition = ID_FALSE;
    idlOS::memset(iderr->Stack.LastErrorMsg, 0, MAX_ERROR_MSG_LEN);
}

/* ----------------------------------------------------------------------
 *
 *  ������ ���� ID ���� �޽��� �ε� �� ���
 *
 * ----------------------------------------------------------------------*/

static ideErrorFactory ideErrorStorage[E_MODULE_COUNT];

// formatted stirng�� �о �������� ������ ����
static SInt ideGetArgumentInfo(SChar *orgFmt, ideArgInfo *orgInfo, va_list args)
{
    UInt  maxInfoCnt = 0;
    UInt  outputOrder = 0;
    UInt  inputOrder;
    SChar c;
    SInt  i;
    ideArgInfo *info;
    SChar      *fmt;

    fmt = orgFmt;
    orgInfo[0].type_info = NULL; // �ʱ�ȭ

    while(( c = *fmt++) )
    {
        if (c == '<') // [<] ����
        {
            SChar numBuf[8]; // ���ڹ�ȣ �Է�

            /* ------------------
             * [1] ���ڹ�ȣ ���
             * -----------------*/
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
            // ���° �Է� �����ΰ�? ������ ����
            inputOrder        = (UInt)idlOS::strtol(numBuf, NULL, 10);
            if (inputOrder >= MAX_ARGUMENT)
            {
                 return IDE_FAILURE;
            }
            info              = &orgInfo[inputOrder];

            orgInfo[outputOrder++].outputOrder = info;

            if (inputOrder > maxInfoCnt) maxInfoCnt = inputOrder;

            /* ------------------
             * [2] ����Ÿ�� ���
             * -----------------*/
            for (i = 0; ; i++)
            {
                if (typeInfo[i].type == IDE_ERR_NONE)
                {
                    return IDE_FAILURE;
                }
                if (idlOS::strncmp(fmt,
                                   typeInfo[i].tmpSpecStr,
                                   typeInfo[i].len) == 0)
                {
                    info->type_info = &typeInfo[i];
                    fmt += typeInfo[i].len;
                    break;
                }
            }
            if ( *fmt != '>')
            {
                return IDE_FAILURE;
            }
            fmt++;
        }
    }
    if (maxInfoCnt >= MAX_ARGUMENT - 1)
    {
        return IDE_FAILURE;
    }
    orgInfo[maxInfoCnt + 1].type_info = NULL; // NULL�� ���� ; ������ flag

    /* ------------------
     * [3] ���� ������ ����
     //     argument *�� �ٽ� ����.
     * -----------------*/

    for (i = 0; ; i++) // BUGBUG : sizeof(data types)�� �ؼ� �̸� ����� �� ����.
    {
        ideErrTypeInfo *typeinfo = orgInfo[i].type_info;
        if (typeinfo == NULL) break;
        switch(typeinfo->type)
        {
        case IDE_ERR_NONE:
            goto out;
        case IDE_ERR_SCHAR:
            orgInfo[i].data.schar  = (SChar)va_arg(args, SInt); /* BUGBUG gcc 2.96 */
            break;
        case IDE_ERR_STRING:
            orgInfo[i].data.string = va_arg(args, SChar *);
            orgInfo[i].stringLen = strlen(orgInfo[i].data.string);
            break;
        case IDE_ERR_STRING_N:
            orgInfo[i].stringLen  = va_arg(args, SInt);
            orgInfo[i].data.string = va_arg(args, SChar *);
            break;
        case IDE_ERR_SINT:
            orgInfo[i].data.sint  = va_arg(args, SInt);
            break;
        case IDE_ERR_UINT:
            orgInfo[i].data.uint  = va_arg(args, UInt);
            break;
        case IDE_ERR_SLONG:
            orgInfo[i].data.slong  = va_arg(args, SLong);
            break;
        case IDE_ERR_ULONG:
            orgInfo[i].data.ulong  = va_arg(args, ULong);
            break;
        case IDE_ERR_VSLONG:
            orgInfo[i].data.vslong = va_arg(args, vSLong);
            break;
        case IDE_ERR_VULONG:
            orgInfo[i].data.vulong = va_arg(args, vULong);
            break;
        case IDE_ERR_HEX32:
            orgInfo[i].data.hex32  = va_arg(args, UInt);
            break;
        case IDE_ERR_HEX64:
            orgInfo[i].data.hex64  = va_arg(args, ULong);
            break;
        case IDE_ERR_HEX_V:
            orgInfo[i].data.hexv  = va_arg(args, vULong);
            break;
        default:
            return IDE_FAILURE;
        }
    }
out:;

    return IDE_SUCCESS;
}

static SInt ideConcatErrorArg(SChar *orgBuf, SChar *fmt, ideArgInfo *info)
{
    SChar c;
    SChar *curBuf = orgBuf;
    UInt  orgBufLen;

    /* ---------------------------
     * [1] formatted string ����
     * --------------------------*/
    while( (c = *fmt++) != 0)
    {
        if (c == '<') // [<] ����
        {
            while(*fmt++ != '>') ; //[>]�� ���ö� ���� skip �� �޽��� ����

            // BUG-21296 : HP��񿡼� �� error message�� ���� �޸��� ����
            // orgBuf�� append��Ű�� orgBufLen�� �����ؾ� �ϰ�
            // MAX_ERROR_MSG_LENG���� �� error message�� truncate��
            orgBufLen = idlOS::strlen(orgBuf);

            switch(info->outputOrder->type_info->type)
            {
            case IDE_ERR_SCHAR:
                if (orgBufLen + MAX_FORMAT_ITEM_LEN < MAX_ERROR_MSG_LEN)
                {
                    idlVA::appendFormat(orgBuf, MAX_ERROR_MSG_LEN,
                                        info->outputOrder->type_info->realSpecStr,
                                        info->outputOrder->data.schar);
                }
                break;
            case IDE_ERR_STRING:
                if (orgBufLen + info->outputOrder->stringLen
                    < MAX_ERROR_MSG_LEN)
                {
                    idlVA::appendFormat(orgBuf, MAX_ERROR_MSG_LEN,
                                        info->outputOrder->type_info->realSpecStr,
                                        info->outputOrder->data.string);
                }
                break;
            case IDE_ERR_STRING_N:
                if (orgBufLen + info->outputOrder->stringLen
                    < MAX_ERROR_MSG_LEN)
                {
                    idlVA::appendFormat(orgBuf, MAX_ERROR_MSG_LEN,
                                        info->outputOrder->type_info->realSpecStr,
                                        info->outputOrder->stringLen,
                                        info->outputOrder->data.string);
                }
                break;
            case IDE_ERR_SINT:
                if (orgBufLen + MAX_FORMAT_ITEM_LEN < MAX_ERROR_MSG_LEN)
                {
                    idlVA::appendFormat(orgBuf, MAX_ERROR_MSG_LEN,
                                        info->outputOrder->type_info->realSpecStr,
                                        info->outputOrder->data.sint);
                }
                break;
            case IDE_ERR_UINT:
                if (orgBufLen + MAX_FORMAT_ITEM_LEN < MAX_ERROR_MSG_LEN)
                {
                    idlVA::appendFormat(orgBuf, MAX_ERROR_MSG_LEN,
                                        info->outputOrder->type_info->realSpecStr,
                                        info->outputOrder->data.uint);
                }
                break;
            case IDE_ERR_SLONG:
                if (orgBufLen + MAX_FORMAT_ITEM_LEN < MAX_ERROR_MSG_LEN)
                {
                    idlVA::appendFormat(orgBuf, MAX_ERROR_MSG_LEN,
                                        info->outputOrder->type_info->realSpecStr,
                                        info->outputOrder->data.slong);
                }
                break;
            case IDE_ERR_ULONG:
                if (orgBufLen + MAX_FORMAT_ITEM_LEN < MAX_ERROR_MSG_LEN)
                {
                    idlVA::appendFormat(orgBuf, MAX_ERROR_MSG_LEN,
                                        info->outputOrder->type_info->realSpecStr,
                                        info->outputOrder->data.ulong);
                }
                break;
            case IDE_ERR_VSLONG:
                if (orgBufLen + MAX_FORMAT_ITEM_LEN < MAX_ERROR_MSG_LEN)
                {
                    idlVA::appendFormat(orgBuf, MAX_ERROR_MSG_LEN,
                                        info->outputOrder->type_info->realSpecStr,
                                        info->outputOrder->data.vslong);
                }
                break;
            case IDE_ERR_VULONG:
                if (orgBufLen + MAX_FORMAT_ITEM_LEN < MAX_ERROR_MSG_LEN)
                {
                    idlVA::appendFormat(orgBuf, MAX_ERROR_MSG_LEN,
                                        info->outputOrder->type_info->realSpecStr,
                                        info->outputOrder->data.vulong);
                }
                break;
            case IDE_ERR_HEX32:
                if (orgBufLen + MAX_FORMAT_ITEM_LEN < MAX_ERROR_MSG_LEN)
                {
                    idlVA::appendFormat(orgBuf, MAX_ERROR_MSG_LEN,
                                        info->outputOrder->type_info->realSpecStr,
                                        info->outputOrder->data.hex32);
                }
                break;
            case IDE_ERR_HEX64:
                if (orgBufLen + MAX_FORMAT_ITEM_LEN < MAX_ERROR_MSG_LEN)
                {
                    idlVA::appendFormat(orgBuf, MAX_ERROR_MSG_LEN,
                                        info->outputOrder->type_info->realSpecStr,
                                        info->outputOrder->data.hex64);
                }
                break;
            case IDE_ERR_HEX_V:
                if (orgBufLen + MAX_FORMAT_ITEM_LEN < MAX_ERROR_MSG_LEN)
                {
                    idlVA::appendFormat(orgBuf, MAX_ERROR_MSG_LEN,
                                        info->outputOrder->type_info->realSpecStr,
                                        info->outputOrder->data.hexv);
                }
            break;
            default:
                break;
            }
            curBuf = orgBuf + idlOS::strlen(orgBuf);
            info++;
        }
        else
        {
            *curBuf++ = c;
        }
    }

    return IDE_SUCCESS;
}

SChar *ideGetErrorMsg(UInt ErrorCode )
{
    static SChar *ideFIND_FAILURE = (SChar *)"Can't find Error Message";
    ideErrorMgr *iderr = ideGetErrorMgr();

    if (iderr->Stack.LastError == ErrorCode)
    {
        return iderr->Stack.LastErrorMsg;
    }
    return ideFIND_FAILURE;
}

// BUG-15166
UInt ideGetErrorArgCount(UInt ErrorCode)
{
    UInt      Section;
    UInt      Index;
    UInt      Count;
    SChar   * Fmt;
    SChar     c;
        
    Section = (ErrorCode & E_MODULE_MASK) >> 28;
    Index   =  ErrorCode & E_INDEX_MASK;
    Fmt     = ideErrorStorage[Section].MsgBuf[Index];
    Count   = 0;
    
    while( (c = *Fmt) )
    {
        if (c == '<') // [<] ����
        {
            Count++;
            Fmt++;
            while( (c = *Fmt) )
            {
                if (c == '>')
                {
                    break;
                }
                Fmt++;
            }
        }
        Fmt++;
    }

    return Count;
}

// Error Code�� �Ҵ��� �� ����.
SInt ideRegistErrorMsb(SChar *fn)
{
    int             i;
    PDL_HANDLE      sFD;
    SChar           **MsgBuf;
    UInt            AltiVer;
    ULong           errCount, Section;
    idErrorMsbType  TempMsbHeader;
    ideErrorFactory *Storage;

    /* --------------------------
     * [0] ���� �˻�
     * -------------------------*/

    if (fn == NULL)
    {
        return IDE_FAILURE;
    }

    /* --------------------------
     * [1] MSB ȭ���� �д´�.
     * -------------------------*/
#ifndef GEN_ERR_MSG
    if ( (sFD = idf::open(fn, O_RDONLY)) == PDL_INVALID_HANDLE)
#else
    if ( (sFD = idlOS::open(fn, O_RDONLY)) == PDL_INVALID_HANDLE)
#endif
    {
#ifndef GEN_ERR_MSG
        ideLog::log(IDE_SERVER_0, ID_TRC_MSB_NOT_FOUND, fn);
#endif
        return IDE_FAILURE;
    }
#ifndef GEN_ERR_MSG
    if (idf::read(sFD, &TempMsbHeader, sizeof(idErrorMsbType)) <= 0)
#else
    if (idlOS::read(sFD, &TempMsbHeader, sizeof(idErrorMsbType)) <= 0)
#endif
    {
        IDE_RAISE(failure_process);
    }

    /* ------------------------------------------
     * [2] ���� �ӽ� ��� ����Ÿ ����
     * -----------------------------------------*/
    AltiVer  = idlOS::ntoh(TempMsbHeader.value_.header.AltiVersionId);
    errCount = idlOS::ntoh(TempMsbHeader.value_.header.ErrorCount);
    Section  = idlOS::ntoh(TempMsbHeader.value_.header.Section);
    TempMsbHeader.value_.header.AltiVersionId = AltiVer;
    TempMsbHeader.value_.header.ErrorCount    = errCount;
    TempMsbHeader.value_.header.Section       = Section;

    /* ------------------------------------------
     * [2.5] ����� ���� ���� �˻�
     * -----------------------------------------*/

    if (AltiVer != iduVersionID)
    {
#ifndef GEN_ERR_MSG
        ideLog::log(IDE_SERVER_0, ID_TRC_MSB_VERSION_INVALID, fn);
#endif
        IDE_RAISE(failure_process);
    }

    if (Section >= E_MODULE_COUNT)
    {
        IDE_RAISE(failure_process);
    }

    /* ------------------------------------------
     * [3] Error Storage�� Section ���� �� ��� ����
     * -----------------------------------------*/
    Storage = &ideErrorStorage[Section];
    idlOS::memcpy(&Storage->MsbHeader, &TempMsbHeader, sizeof(idErrorMsbType));

    /* ------------------------------------------
     * [4] ���� ���� �޽����� �о� ����
     * -----------------------------------------*/
    MsgBuf = (SChar **)idlOS::calloc(errCount, sizeof(SChar *));
    if (MsgBuf == NULL)
    {
        IDE_RAISE(failure_process);
    }

#ifndef GEN_ERR_MSG
    for (i = 0; !idf::fdeof(sFD) && i < (SInt)errCount; i++)
#else
    for (i = 0; !idlOS::fdeof(sFD) && i < (SInt)errCount; i++)
#endif
    {
        // ���� �޽����� �о ����
        SChar buffer[1024];

#ifndef GEN_ERR_MSG
        if (idf::fdgets(buffer, 1024, sFD))
#else
        if (idlOS::fdgets(buffer, 1024, sFD))
#endif
        {
            UInt len = idlOS::strlen(buffer);
            MsgBuf[i] = (SChar *)idlOS::calloc(1, len + 1);

            if (MsgBuf[i] == NULL)
            {
                IDE_RAISE(failure_process);
            }

            IDE_TEST_RAISE((len == 0) || (len > sizeof(buffer)), failure_process);
            if (isspace(buffer[len - 1])) buffer[len - 1] = 0;
            idlOS::strcpy(MsgBuf[i], buffer);
        }
        else
        {
            break;
        }
    }

#ifndef GEN_ERR_MSG
    (void)idf::close(sFD);
#else
    (void)idlOS::close(sFD);
#endif
    /* ------------------------------------------
     * [5] Error Storage �ϼ�
     * -----------------------------------------*/
    Storage->MsgBuf = MsgBuf;

    return IDE_SUCCESS;

    IDE_EXCEPTION(failure_process);
#ifndef GEN_ERR_MSG
    (void)idf::close(sFD);
#else
    (void)idlOS::close(sFD);
#endif

    IDE_EXCEPTION_END;
    return IDE_FAILURE;
}


static void ideSetServerErrorCode(ideErrorMgr *aErrorMgr,
                                  UInt         aErrorCode,
                                  SInt         aSystemErrno,
                                  va_list      aArgs)
{
    static SChar *NULL_MESSAGE           = (SChar *)"No Error Message Loaded";
    static SChar *OUT_BOUND_MESSAGE      = (SChar *)"Out of ErrorCode Bound";
    static SChar *ERR_PARSE_ARG_MESSAGE  = (SChar *)"Invalid Error-Formatted String";
    static SChar *ERR_CONCAT_ARG_MESSAGE = (SChar *)"Invalid Error-Concatenation";

    /* ���� �޽��� ���� �غ�*/
    UInt         Section;
    UInt         Index;
    ideArgInfo   argInfo[MAX_ARGUMENT];
    Section = (aErrorCode & E_MODULE_MASK) >> 28;
    Index   =  aErrorCode & E_INDEX_MASK;
    ideErrorFactory *Storage = &ideErrorStorage[Section];

    aErrorMgr->Stack.LastError       = aErrorCode;
    aErrorMgr->Stack.LastSystemErrno = aSystemErrno;

    if (Storage->MsbHeader.value_.header.ErrorCount <= 0 )
    {
        aErrorMgr->Stack.HasErrorPosition = ID_FALSE;
        idlOS::strcpy(aErrorMgr->Stack.LastErrorMsg, NULL_MESSAGE );
    }
    else if (Index < Storage->MsbHeader.value_.header.ErrorCount) // ���� �޽����� ��������.
    {
        SChar *fmt = Storage->MsgBuf[Index];

        if (fmt == NULL)
        {
            aErrorMgr->Stack.HasErrorPosition = ID_FALSE;
            idlOS::strcpy(aErrorMgr->Stack.LastErrorMsg, NULL_MESSAGE);
        }
        else
        {
            if (ideGetArgumentInfo(fmt, &argInfo[0], aArgs) == IDE_SUCCESS)
            {
                idlOS::memset(aErrorMgr->Stack.LastErrorMsg, 0,
                              sizeof(aErrorMgr->Stack.LastErrorMsg));
                aErrorMgr->Stack.LastErrorMsg[0] = 0;

                if(ideConcatErrorArg(aErrorMgr->Stack.LastErrorMsg,
                                     fmt,
                                     &argInfo[0]) != IDE_SUCCESS)
                {
                    aErrorMgr->Stack.HasErrorPosition = ID_FALSE;
                    idlOS::strcpy(aErrorMgr->Stack.LastErrorMsg, ERR_CONCAT_ARG_MESSAGE);
                }
            }
            else
            {
                aErrorMgr->Stack.HasErrorPosition = ID_FALSE;
                idlOS::strcpy(aErrorMgr->Stack.LastErrorMsg, ERR_PARSE_ARG_MESSAGE);
            }
        }
    }
    else
    {
        aErrorMgr->Stack.HasErrorPosition = ID_FALSE;
        idlOS::strcpy(aErrorMgr->Stack.LastErrorMsg, OUT_BOUND_MESSAGE);
    }

#ifndef GEN_ERR_MSG
    {
        UInt sFlag;
        sFlag = aErrorCode & (E_MODULE_MASK | E_ACTION_MASK);

        if (sFlag == (E_ACTION_FATAL | E_MODULE_SM) ||
            sFlag == (E_ACTION_FATAL | E_MODULE_QP) ||
            sFlag == (E_ACTION_FATAL | E_MODULE_MT) ||
            sFlag == (E_ACTION_FATAL | E_MODULE_RP)
            )
        {
            ideLog::logCallStack( IDE_ERR_0 );
            IDE_CALLBACK_FATAL("FATAL Error Code Occurred.");
        }
    }
#endif
}

ideErrorMgr* ideSetErrorCode(UInt ErrorCode, ...)
{
    SInt         systemErrno = errno; // �̸� ����
    va_list      args;
    ideErrorMgr* sErrMgr = ideGetErrorMgr();

    va_start(args, ErrorCode);

    ideSetServerErrorCode(sErrMgr,
                          ErrorCode,
                          systemErrno,
                          args);
     va_end(args);

    return sErrMgr;
}

#ifdef ALTIBASE_FIT_CHECK

ideErrorMgr* ideFitSetErrorCode( UInt aErrorCode, va_list aArgs )
{
    SInt         sSystemErrno = errno; // �̸� ����
    ideErrorMgr* sErrMgr = ideGetErrorMgr();

    ideSetServerErrorCode( sErrMgr,
                           aErrorCode,
                           sSystemErrno,
                           aArgs );

    return sErrMgr;
}
#endif

// PROJ-1335 errorCode, errorMsg�� ���� ����
ideErrorMgr* ideSetErrorCodeAndMsg( UInt ErrorCode, SChar* ErrorMsg )
{
    SInt         systemErrno = errno; // �̸� ����
    ideErrorMgr* sErrMgr = ideGetErrorMgr();

    sErrMgr->Stack.LastError       = ErrorCode;
    sErrMgr->Stack.LastSystemErrno = systemErrno;

    idlOS::snprintf( sErrMgr->Stack.LastErrorMsg, MAX_ERROR_MSG_LEN,
                     "%s", ErrorMsg );
    
    return sErrMgr;
}

void ideDump()
{
    SChar *errbuf;
    SInt   errbuflen = 1024 * 1024;

    errbuf = (SChar *)idlOS::calloc(1, errbuflen);
    /* BUG-25586
     * [CodeSonar::NullPointerDereference] ideDump() ���� �߻� */
    IDE_TEST_RAISE( errbuf == NULL, skip_err_dump );

    {
        ideErrorMgr *iderr = ideGetErrorMgr();
#ifndef GEN_ERR_MSG
        UInt sSourceInfo = 0;
        /* ------------------------------------------------
         *  genErrMsg�� ������ property��
         *  �������� �ʵ���!
         * ----------------------------------------------*/
        (void)idp::read("SOURCE_INFO", &sSourceInfo);

        if ( (sSourceInfo & IDE_SOURCE_INFO_SERVER) == 0)
        {
            idlVA::appendFormat(errbuf, errbuflen, " ERR-%05X(errno=%d): %s\n",
                                E_ERROR_CODE(ideGetErrorCode()),
                                ideGetSystemErrno(),
                                ideGetErrorMsg(ideGetErrorCode()));
        }
        else
#endif
        {
            idlOS::snprintf(errbuf, errbuflen, "ERR-%05X (%s:%d)[errno=%d] : %s\n",
                            E_ERROR_CODE(ideGetErrorCode()),
                            iderr->Stack.LastErrorFile,
                            iderr->Stack.LastErrorLine,
                            ideGetSystemErrno(),
                            ideGetErrorMsg(ideGetErrorCode()));
        }
    }
    idlOS::fprintf(stderr, "%s\n", errbuf);
    idlOS::free(errbuf);
    ideClearError();

    IDE_EXCEPTION_CONT( skip_err_dump );
}

void ideSetClientErrorCode(ideClientErrorMgr     *aErrorMgr,
                           ideClientErrorFactory *aFactory,
                           UInt                   aErrorCode,
                           va_list                aArgs)
{
    static SChar *NULL_MESSAGE           = (SChar *)"No Error Message Loaded";
    static SChar *OUT_BOUND_MESSAGE      = (SChar *)"Out of ErrorCode Bound";
    static SChar *ERR_PARSE_ARG_MESSAGE  = (SChar *)"Invalid Error-Formatted String";
    static SChar *ERR_CONCAT_ARG_MESSAGE = (SChar *)"Invalid Error-Concatenation";

    /* ���� �޽��� ���� �غ�*/
    UInt         Index;
    ideArgInfo   argInfo[MAX_ARGUMENT];
    Index   =  aErrorCode & E_INDEX_MASK;
    
    aErrorMgr->mErrorCode       = aErrorCode;

    if (1)//Index < Storage->MsbHeader.value_.header.ErrorCount) // ���� �޽����� ��������.
    {
        SChar *fmt = (SChar *)aFactory[Index].mErrorMsg;

        if (fmt == NULL)
        {
            idlOS::strcpy(aErrorMgr->mErrorMessage, NULL_MESSAGE);
        }
        else
        {
            if (ideGetArgumentInfo(fmt, &argInfo[0], aArgs) == IDE_SUCCESS)
            {
                /* ------------------------------------------------
                 * Normal Error Code Setting
                 * ----------------------------------------------*/
                
                idlOS::memset(aErrorMgr->mErrorMessage, 0, sizeof(aErrorMgr->mErrorMessage));
                aErrorMgr->mErrorMessage[0] = 0;
                if(ideConcatErrorArg(aErrorMgr->mErrorMessage,
                                     fmt,
                                     &argInfo[0]) != IDE_SUCCESS)
                {
                    idlOS::strcpy(aErrorMgr->mErrorMessage, ERR_CONCAT_ARG_MESSAGE);
                }
                idlOS::strncpy(aErrorMgr->mErrorState,
                               aFactory[Index].mErrorState,
                               ID_SIZEOF(aErrorMgr->mErrorState));
            }
            else
            {
                idlOS::strcpy(aErrorMgr->mErrorMessage, ERR_PARSE_ARG_MESSAGE);
            }
        }
    }
    else
    {
        idlOS::strcpy(aErrorMgr->mErrorMessage, OUT_BOUND_MESSAGE);
    }
}


