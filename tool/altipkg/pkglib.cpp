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
 * $Id$
 **********************************************************************/

#include <idl.h>
#include <altipkg.h>


/* ��Ʈ�� �յڿ� �����ϴ� WHITE-SPACE�� ���� */
void eraseWhiteSpace(SChar *buffer)
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

void changeFileSeparator(SChar *aBuf)
{
#if defined(VC_WIN32)

    SChar *sPtr = aBuf;

    for (sPtr = aBuf; *sPtr != NULL; sPtr++)
    {
        if (*sPtr == '/')
        {
            *sPtr = IDL_FILE_SEPARATOR;
        }
    }
#else
    PDL_UNUSED_ARG( aBuf );
#endif
}

void debugMsg(const char *format, ...)
{
    if (gDebug == ID_TRUE)
    {
        va_list ap;

        va_start (ap, format);
        idlOS::fprintf(stderr, "  ");
        vfprintf(stderr, format, ap);
        idlOS::fflush(stderr);
        va_end (ap);
    }
}

void reportMsg(const char *format, ...)
{
    if (gReport == ID_TRUE)
    {
        va_list ap;
        va_start (ap, format);
        vfprintf(gReportFP, format, ap);
        va_end (ap);
    }
}


void altiPkgError(const char *format, ...)
{
    va_list ap;

    va_start (ap, format);
    idlOS::fprintf(stderr, "[ERR]=>");
    vfprintf(stderr, format, ap);
    idlOS::fflush(stderr);
    va_end (ap);
    idlOS::fflush(stderr);
    idlOS::exit(-1);
}

void getFileStat(SChar *aFileName, PDL_stat *aStat)
{
    PDL_HANDLE    sHandle;

    sHandle = idlOS::open(aFileName, O_RDONLY);
    if (sHandle == PDL_INVALID_HANDLE)
    {
        altiPkgError("Open for Fstat Error  [%s] (errno=%"ID_UINT32_FMT")\n",
                     aFileName, errno);
    }

    if (idlOS::fstat(sHandle, aStat) != 0)
    {
        altiPkgError("Fstat Error (errno=%"ID_UINT32_FMT")\n", errno);
    }

    idlOS::close(sHandle);

}


ULong getFileSize(SChar *aFileName)
{
    PDL_stat aStat;

    getFileStat(aFileName, &aStat);

    return (ULong)aStat.st_size;
}



