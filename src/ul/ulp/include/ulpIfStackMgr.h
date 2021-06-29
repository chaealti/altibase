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

#ifndef _ULP_IFSTACKMGR_H_
#define _ULP_IFSTACKMGR_H_ 1

#include <idl.h>
#include <ulpMacro.h>

/* macro if ���ǹ�ó���� ���� �ڷᱸ�� */
typedef struct ulpPPifstack
{
    ulpPPiftype   mType;
    ulpPPifresult mVal;
    /* BUG-28162 : SESC_DECLARE ��Ȱ  */
    idBool        mSescDEC;
} ulpPPifstack;


class ulpPPifstackMgr
{
    private:

        ulpPPifstack mIfstack[MAX_MACRO_IF_STACK_SIZE];

        SInt         mIndex;

    public:

        ulpPPifstackMgr();

        SInt ulpIfgetIndex();

        idBool ulpIfCheckGrammar( ulpPPiftype aIftype );

        /* BUG-28162 : SESC_DECLARE ��Ȱ  */
        idBool ulpIfpop4endif();

        IDE_RC ulpIfpush( ulpPPiftype aIftype, ulpPPifresult aVal );

        ulpPPifresult ulpPrevIfStatus ( void );

        /* BUG-27961 : preprocessor�� ��ø #ifó���� #endif �����ҽ� ������ ����ϴ� ����  */
        // preprocessor�� #endif �� ��ģ�� ���������� �ڵ����
        // ���Ϸ� ����� �ؾ��ϴ��� �׳� skip�ؾ��ϴ����� �������ִ� �Լ�.
        idBool ulpIfneedSkip4Endif(void);
};

#endif
