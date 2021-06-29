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
 * $Id: utmPropertyMgr.cpp 80542 2017-07-19 08:01:20Z daramix $
 **********************************************************************/

/***********************************************************************
 *
 * NAME
 *   utmPropertyMgr.cpp
 *
 * DESCRIPTION
 *   utmPropertyMgr ����
 *
 * PUBLIC FUNCTION(S)
 *
 *
 * PRIVATE FUNCTION(S)
 *
 *
 * NOTES
 *
 * MODIFIED    (MM/DD/YYYY)
 *    sjkim     2/14/2000 - Created
 *
 **********************************************************************/

//#define DEBUG_TRACE

#include <ideLog.h>
#include <idp.h>
#include <utm.h>
#include <utmPropertyMgr.h>

void utmPropertyMgr::initialize(SChar *HomeDir, SChar *ConfFile)
{

#define IDE_FN "utmPropertyMgr::utmPropertyMgr(SChar *HomeDir, SChar *ConfFile)"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY("HomeDir=%s ConfFilg=%s", HomeDir, ConfFile));

    cnt_of_properties_ = 0;
    idlOS::memset(ppair_, 0, sizeof(utmPropertyPair) * MAX_PROPERTIES);
    idlOS::strncpy(home_dir_,  HomeDir,  255);
    idlOS::strncpy(conf_file_, ConfFile, 255);

#undef IDE_FN
}

SInt utmPropertyMgr::readProperties()
{

#define IDE_FN "SInt utmPropertyMgr::readProperties()"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    SChar propfile[512];
    FILE *fp;
    SChar buffer[1024];
    SChar *Name;
    SChar *Value;

    idlOS::memset(propfile, 0, 512);
#if !defined(VC_WIN32)
    if ( conf_file_[0] == IDL_FILE_SEPARATOR )
#else
    if ( conf_file_[1] == ':'
      && conf_file_[2] == IDL_FILE_SEPARATOR )
#endif /* VC_WIN32 */
    {
        idlOS::sprintf(propfile, "%s", conf_file_);
    }
    else
    {
        idlOS::sprintf(propfile, "%s%c%s", home_dir_, IDL_FILE_SEPARATOR, conf_file_);
    }

    fp = idlOS::fopen(propfile, "r");
    IDE_TEST(fp == NULL);

    while(!feof(fp))
    {
        idlOS::memset(buffer, 0, 1024);
        if (idlOS::fgets(buffer, 1024, fp) == NULL)
        {
            // ȭ���� ������ ����
            break;
        }
        // buffer�� ������ ������ ����

        Name  = NULL;
        Value = NULL;

        if (parseBuffer(buffer, &Name, &Value) == -1)
        {
            idlOS::fclose(fp);
            return IDE_FAILURE;
        }

        if (Name) // ������Ƽ ���ο� ������ ����(�̸�+�� ����)
        {
            utmPropertyPair *curr = &ppair_[cnt_of_properties_];

            curr->name_ = (SChar *)idlOS::calloc(1, idlOS::strlen(Name) + 1);
            IDE_TEST(curr->name_ == NULL);
            idlOS::strcpy(curr->name_, Name);

            if (Value)
            {
                if (Value[0] == '?') // ġȯ���ڿ� ����
                {
                    int length = idlOS::strlen(home_dir_) +
                        idlOS::strlen(Value) + 2;
                    curr->value_ = (SChar *)idlOS::calloc(1, length);
                    IDE_TEST(curr->value_ == NULL);

                    idlOS::sprintf(curr->value_, "%s%s", home_dir_, Value + 1);
                }
                else
                {
                    curr->value_ =
                        (SChar *)idlOS::calloc(1, idlOS::strlen(Value) + 1);
                    IDE_TEST(curr->value_ == NULL);
                    idlOS::strcpy(curr->value_, Value);
                }
            }
            cnt_of_properties_++;
        }
    }

    // BUGBUG : üũ ���
    idlOS::fclose(fp);

    return IDE_SUCCESS;

    // ���� �������̽� utmGetPropertyMgr()�� ����ϴ� ������ ���� ó��

    IDE_EXCEPTION_END;

    if (fp != NULL)
    {
        (void) idlOS::fclose(fp);
    }

    return IDE_FAILURE;
#undef IDE_FN
}

/* ��Ʈ���� �����ϴ� ��� WHITE-SPACE�� ���� */
static void eraseWhiteSpaceForProperty(SChar *buffer)
{

#define IDE_FN "static void eraseWhiteSpaceForProperty(SChar *buffer)"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    SInt i;
    SInt len = idlOS::strlen(buffer);

    for (i = 0; i < len && buffer[i]; i++)
    {
        if (buffer[i] == '#')
        {
            buffer[i]= 0;
            return;
        }
        if (isspace(buffer[i])) // �����̽� ��
        {
            SInt j;

            for (j = i;  buffer[j]; j++)
            {
                buffer[j] = buffer[j + 1];
            }
            i--;
        }
    }


#undef IDE_FN
}

SInt utmPropertyMgr::parseBuffer(SChar *buffer,
                                 SChar **Name,
                                 SChar **Value)
{

#define IDE_FN "SInt utmPropertyMgr::parseBuffer(SChar *buffer, SChar **Name, SChar **Value)"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY("buffer=%s", buffer));

    // 1. White Space ����
    eraseWhiteSpaceForProperty(buffer);

    // 2. ������ ���ų� �ּ��̸� ����
    SInt len = idlOS::strlen(buffer);
    if (len == 0 || buffer[0] == '#')
    {
        return IDE_SUCCESS;
    }

    *Name = buffer; // �̸��� ����

    // 3. �� �������� �˻�
    SInt i;

    for (i = 0; i < len; i++)
    {
        if (buffer[i] == '=')
        {
            // �����ڰ� �����ϸ�,
            buffer[i] = 0;

            if (buffer[i + 1])
            {
                *Value = &buffer[i + 1];

                if (idlOS::strlen(&buffer[i + 1]) > 256)
                {
                    // �����Ⱚ�� ����..
                    return IDE_FAILURE;
                }
            }
            return IDE_SUCCESS;
        }
    }
    return IDE_SUCCESS;


#undef IDE_FN
}


SChar* utmPropertyMgr::getValue(SChar *name)
{

#define IDE_FN "SChar* utmPropertyMgr::getValue(SChar *name)"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY("name=%s", (name == NULL ? "" : name)));

    SInt i;

    if (name)
    {
        for (i = 0; i < cnt_of_properties_; i++)
        {
            utmPropertyPair *curr = &ppair_[i];
            if (idlOS::strcmp(name, curr->name_) == 0)
            {
                return curr->value_;
            }
        }
    }
    return NULL;


#undef IDE_FN
}

SChar* utmPropertyMgr::getHomeDir()
{

#define IDE_FN "SChar* utmPropertyMgr::getHomeDir()"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    return home_dir_;


#undef IDE_FN
}

SChar* utmPropertyMgr::getConfFile()
{

#define IDE_FN "SChar* utmPropertyMgr::getConfFile()"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    return conf_file_;


#undef IDE_FN
}

/* ==============================================================
 *
 * utmGetPropertyMgr()
 *
 * return ���� NULL �̸� ����..
 * �� �Լ��� ȣ��� �ʱ�ȭ���� ���� �����̸�, �ʱ�ȭ�� �����
 * �����ش�.
 *
 * ============================================================*/

utmPropertyMgr* utmGetPropertyMgr(SChar *EnvString,
                                  SChar *HomeDir,
                                  SChar *ConfFile)
{

#define IDE_FN "utmPropertyMgr* utmGetPropertyMgr(SChar *EnvString, SChar *HomeDir, SChar *ConfFile)"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY("EnvString=%s HomeDir=%s ConfFile=%s",
                                    (EnvString == NULL ? "NULL" : EnvString),
                                    (HomeDir   == NULL ? "NULL" : HomeDir),
                                    (ConfFile  == NULL ? "NULL" : ConfFile)));

    static utmPropertyMgr *iduBP = NULL;

    if (iduBP == NULL) // �ʱ�ȭ ���� ���� �ý��� ������Ƽ ����
    {
        SChar *home_dir  = NULL;
        SChar *conf_file = NULL;

        // EnvString���� [ALTIBASE_HOME]�� ȯ�溯������ ��� ���� ��Ʈ�� ����
        if (EnvString == NULL)
        {
            EnvString = IDP_HOME_ENV;
        }
        else
        {
            size_t  envlen = idlOS::strlen(EnvString);
            if (envlen == 0)
            {
                EnvString = IDP_HOME_ENV;
            }
        }

        /* ---------------------
         *  [1] HOME ���丮 ����
         * -------------------*/

        if (HomeDir == NULL) // ����Ʈ ȯ�溯������ ������Ƽ �ε�
        {
            home_dir = idlOS::getenv(EnvString);
            if (home_dir == NULL)
            {
                return NULL; // ȯ�溯���� ���õǾ� ���� ����
            }
        }
        else // HomeDir�� NULL�� �ƴ�
        {
            size_t  homelen = idlOS::strlen(HomeDir);
            if (homelen == 0)
            {
                home_dir = idlOS::getenv(EnvString);
                if (home_dir == NULL)
                {
                    return NULL; // ȯ�溯���� ���õǾ� ���� ����
                }
            }
            else
            {
                home_dir = HomeDir;
            }
        }

        /* ---------------------
         *  [2] Conf File  ����
         * -------------------*/

        if (ConfFile != NULL)
        {
            size_t  conflen = idlOS::strlen(ConfFile);
            if (conflen > 0)
            {
                conf_file = ConfFile;
                /*
                 * ���� ���´� home_dir�� ALTIBASE�� HOME ���丮��
                 * Conf ȭ���� �����Ǿ� ����
                 */
                iduBP = utmReadPropertyMgr(home_dir, conf_file);
            }
        }

    }

    return iduBP;


#undef IDE_FN
}


/* ==============================================================
 *
 * utmReadPropertyMgr()
 *
 * ============================================================*/

utmPropertyMgr* utmReadPropertyMgr(SChar *HomeDir,
                                   SChar *ConfFile)
{

#define IDE_FN "utmPropertyMgr* utmReadPropertyMgr(SChar *HomeDir, SChar *ConfFile)"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY("HomeDir=%s ConfFile=%s",
                                    (HomeDir   == NULL ? "NULL" : HomeDir),
                                    (ConfFile  == NULL ? "NULL" : ConfFile)));

    utmPropertyMgr *bp;
    if (HomeDir == NULL || ConfFile == NULL)
    {
        return NULL;
    }

    bp = (utmPropertyMgr*) idlOS::malloc(sizeof(utmPropertyMgr));
    if (bp != NULL )
    {
        bp->initialize(HomeDir, ConfFile);
        if (bp->readProperties() == 0)
        {
            return bp;
        }
    }
    idlOS::free( bp );
    /* �޸� ���� or read �ÿ� ���� �߻� */
    return NULL;


#undef IDE_FN
}
