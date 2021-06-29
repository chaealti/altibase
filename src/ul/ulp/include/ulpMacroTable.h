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

#ifndef _ULP_MACROTABLE_H_
#define _ULP_MACROTABLE_H_ 1

#include <idl.h>
#include <ide.h>
#include <ulpErrorMgr.h>
#include <ulpHash.h>
#include <ulpTypes.h>
#include <ulpMacro.h>

// the node for macro hash table
typedef struct ulpMacroNode
{
    // macro define name
    SChar  mName[MAX_MACRO_DEFINE_NAME_LEN];
    // macro define text
    SChar  mText[MAX_MACRO_DEFINE_CONTENT_LEN];
    // Is it macro function?
    idBool mIsFunc;

    ulpMacroNode *mNext;
} ulpMacroNode;


/******************************
 *
 * macro�� ����, �����ѱ� ���� class
 ******************************/
class ulpMacroTable
{
/* METHODS */
public:
    ulpMacroTable();
    ~ulpMacroTable();

    void ulpInit();

    void ulpFinalize();

    // defined macro�� hash table�� �����Ѵ�.
    IDE_RC          ulpMDefine ( SChar *aName, SChar *aText, idBool aIsFunc );

    // Ư�� �̸��� ���� defined macro�� hash table���� �˻��Ѵ�.
    ulpMacroNode   *ulpMLookup( SChar *aName );

    // Ư�� �̸��� ���� defined macro�� hash table���� �����Ѵ�.
    void            ulpMUndef( SChar *aName );

    /* BUG-28118 : system ������ϵ鵵 �Ľ̵ž���.                    *
     * 11th. problem : C preoprocessor���� ��ũ�� �Լ� ���� ó�� ����. *
     * 12th. problem : C preprocessor���� �� ��ū�� concatenation�Ҷ� ���Ǵ� '##' ��ū ó���ؾ���. */
     void ulpMEraseSharp4MFunc( SChar *aText );

    // for debugging
    void            ulpMPrint( void );

private:

/* ATTRIBUTES */
public:
    // ulpMacroTable ����� defined macro�� ����
    SInt mCnt;

    // max number of ulpMacroTable buckets
    SInt mSize;

private:
    // hash function
    UInt     (*mHash) (UChar *);

    // marco�� ������ hash table
    ulpMacroNode* mMacroTable[MAX_SYMTABLE_ELEMENTS];
};

#endif
