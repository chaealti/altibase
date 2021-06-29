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

#ifndef _ULP_STRUCTTABLE_H_
#define _ULP_STRUCTTABLE_H_ 1

#include <idl.h>
#include <ide.h>
#include <ulpSymTable.h>
#include <ulpErrorMgr.h>
#include <ulpHash.h>
#include <ulpTypes.h>
#include <ulpMacro.h>

class ulpSymTable;

typedef struct ulpStructTNode
{
    // struct tag name
    SChar             mName[MAX_HOSTVAR_NAME_SIZE];
    // struct field symbol table.
    ulpSymTable      *mChild;
    // scope depth
    SInt              mScope;
    // doubly linked-list
    ulpStructTNode   *mNext;
    ulpStructTNode   *mPrev;
    // Scope link (list)
    ulpStructTNode   *mSLink;
    // bucket index
    // bucket-list ó�� node�� ��� mPrev�� null�̱� ������
    // ulpStructDelScope()���� ���� ������ ���ȴ�.
    UInt              mIndex;
} ulpStructTNode;


/******************************
 *
 * structure type�� ����, �����ѱ� ���� class
 ******************************/
class ulpStructTable
{
/* Method */
public:
    ulpStructTable();
    ~ulpStructTable();

    void ulpInit();

    void ulpFinalize();

    // struct table�� ���ο� tag�� �����Ѵ�.
    ulpStructTNode *ulpStructAdd ( SChar *aTag, SInt aScope );

    // tag�� ���� ��� struct table ������ buket�� �����Ѵ�.
    ulpStructTNode *ulpNoTagStructAdd ( void );

    // Ư�� �̸��� ���� tag�� struct table���� scope�� �ٿ����鼭 �˻��Ѵ�.
    ulpStructTNode *ulpStructLookupAll( SChar *aTag, SInt aScope );

    // Ư�� �̸��� ���� tag�� Ư�� scope���� �����ϴ��� struct table�� �˻��Ѵ�.
    ulpStructTNode *ulpStructLookup( SChar *aTag, SInt aScope );

    // Ư�� �̸��� ���� tag�� struct table���� �����Ѵ�.
    //void            ulpStructDelete( SChar *aTag, SInt aScope );

    // ���� scope���� ��� ���������� struct table���� �����Ѵ�.
    void            ulpStructDelScope( SInt aScope );

    // print struct table for debug
    void            ulpPrintStructT( void );

/* ATTRIBUTES */
public:
    SInt mCnt;  // m_SymbolTable�� ����� host variables�� ����
    SInt mSize; // max number of symbol table buckets

private:
    UInt     (*mHash) (UChar *);       /* hash function */

    // Struct table
    ulpStructTNode* mStructTable[MAX_SYMTABLE_ELEMENTS + 1];
    // struct Scope table
    ulpStructTNode* mStructScopeT[MAX_SCOPE_DEPTH + 1];
};

#endif
