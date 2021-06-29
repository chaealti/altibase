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

/*****************************************************************************
 * $Id: utpCommandOptions.h$
 ****************************************************************************/

#ifndef _O_UTP_COMMADN_OPTIONS_H_
#define _O_UTP_COMMADN_OPTIONS_H_ 1

#include <idl.h>
#include <utpDef.h>

// �������� �÷� �������� �⺻ �ִ� ��� ����: 160bytes
// ��: �����Ͱ� ū ��� �� ����� �ʿ䰡 ������ ���Ƽ�
// �����÷��� ��� ����� 80 bytes�� �ϹǷ� �ִ� 2������ ��µȴ�
#define UTP_BIND_UNLIMIT_MAX_LEN  0x7fffffff
#define UTP_BIND_DEFAULT_MAX_LEN  160

class utpCommandOptions
{
public:
    static utpCommandType   mCommandType;
    static utpStatOutFormat mStatFormat;
    static SInt             mArgc;
    static SInt             mArgIdx;
    static SInt             mBindMaxLen;

public:
    static IDE_RC parse(SInt aArgc, SChar **aArgv);

};

#endif
