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

#include <ulpScopeTable.h>

/* extern of ulpMain.h */
extern ulpStructTable gUlpStructT;


ulpScopeTable::ulpScopeTable()
{

}

void ulpScopeTable::ulpInit()
{
    mCurDepth = 0;
    idlOS::memset( mScopeTable, 0, SCOPE_TABLE_SIZE );
}

ulpScopeTable::~ulpScopeTable()
{

}

void ulpScopeTable::ulpFinalize()
{
    SInt sI;

    for( sI = 0 ; sI < SCOPE_TABLE_SIZE ; sI++ )
    {
        if( mScopeTable[sI] != NULL )
        {
            mScopeTable[sI]->ulpFinalize();
            delete mScopeTable[sI];
        }
    }
}

/* Ư�� scope�� symbol table�� ���ڷ� ���� symbol�� �߰��Ѵ�. */
ulpSymTNode *ulpScopeTable::ulpSAdd ( ulpSymTElement *aSym, SInt aScope )
{
    ulpSymTable    *sSymT;
    ulpStructTNode *sStructNode;
    ulpSymTNode    *sSymNode;

    IDE_TEST_RAISE( (aScope < 0) || (aScope > MAX_SCOPE_DEPTH),
                    ERR_COMP_SCOPE_DEPTH );

    sSymT = mScopeTable[aScope];
    if( sSymT == NULL )
    {
        // �׳� new �� �����. ���߿� delete �ؾ���.
        IDE_TEST_RAISE( (sSymT = new ulpSymTable) == NULL,
                        ERR_MEMORY_ALLOC );
        sSymT->ulpInit();
        mScopeTable[aScope] = sSymT;
    }
    else
    {
        // �̹� ���� scope���� �ش� symbol�� �����ϴ��� Ȯ��.
        IDE_TEST_RAISE( sSymT->ulpSymLookup( aSym->mName ) != NULL,
                        ERR_COMP_SYM_AREADY_EXIST  );
    }

    // struct type�� ���� ���
    // typedef struct �ϰ�쿡�� ���߿� ���� ����� struct table���� �˻��Ѵ�.
    // ���� struct �̸��� ��� ���� �ʾ��� ��쿡�� struct table���� �˻����� ����.
    // �׿��� ��� �Ʒ����� ó��.

    /* BUG-28118 : system ������ϵ鵵 �Ľ̵ž���.            *
     * 5th. problem : ���ǵ��� ���� ����ü ������ ���� ����ȵ�. *
     * 8th. problem : can't resolve extern variable type at declaring section. */
    // struct pointer or extern ������ ��쿡�� �˻����� ����.
    if( (aSym->mIsstruct      == ID_TRUE)  &&
        (aSym->mIsTypedef     == ID_FALSE) &&
        (aSym->mStructName[0] != '\0' )    &&
        (aSym->mPointer       == 0)        &&
        (aSym->mIsExtern      == ID_FALSE)
      )
    {
        // struct table���� struct�̸��� �˻��غ���.
        sStructNode = gUlpStructT.ulpStructLookupAll( aSym->mStructName,
                                                      aScope );

        IDE_TEST_RAISE( sStructNode == NULL, ERR_COMP_STRUCT_NOT_EXIST );

        aSym->mStructLink = sStructNode;
    }

    // symbol �߰�
    IDE_TEST( (sSymNode = sSymT->ulpSymAdd( aSym )) == NULL );

    return sSymNode;

    IDE_EXCEPTION ( ERR_COMP_SCOPE_DEPTH );
    {
        ulpErrorMgr mErrorMgr;
        ulpSetErrorCode( &mErrorMgr,
                         ulpERR_ABORT_COMP_Invalid_Scope_Depth_Error,
                         aScope );
        ulpPrintfErrorCode( stderr,
                            &mErrorMgr);
    }
    IDE_EXCEPTION ( ERR_MEMORY_ALLOC );
    {
        ulpErrorMgr mErrorMgr;
        ulpSetErrorCode( &mErrorMgr,
                         ulpERR_ABORT_Memory_Alloc_Error );
        ulpPrintfErrorCode( stderr,
                            &mErrorMgr);
        IDE_ASSERT(0);
    }
    IDE_EXCEPTION ( ERR_COMP_SYM_AREADY_EXIST );
    {
        ulpErrorMgr mErrorMgr;
        ulpSetErrorCode( &mErrorMgr,
                         ulpERR_ABORT_COMP_Symbol_Redefinition_Error,
                         aSym->mName );
        ulpPrintfErrorCode( stderr,
                            &mErrorMgr);
    }
    IDE_EXCEPTION ( ERR_COMP_STRUCT_NOT_EXIST );
    {
        ulpErrorMgr mErrorMgr;
        ulpSetErrorCode( &mErrorMgr,
                         ulpERR_ABORT_COMP_Type_Not_Exist_Error,
                         aSym->mStructName );
        ulpPrintfErrorCode( stderr,
                            &mErrorMgr);
    }
    IDE_EXCEPTION_END;

    return NULL;
}


/* Ư�� �̸��� ���� ������ symbol table���� scope�� �ٿ����鼭 �˻��Ѵ�. */
ulpSymTElement *ulpScopeTable::ulpSLookupAll( SChar *aName, SInt aScope )
{
    SChar           sTmpStr[MAX_HOSTVAR_NAME_SIZE * 2];
    SChar          *sStrPos;
    SChar          *sBracketPos;
    ulpStructTNode *sStructNode;
    ulpSymTable    *sSymT;
    ulpSymTElement *sSymTE;
   /* BUG-28118 : system ������ϵ鵵 �Ľ̵ž���. *
    * 5th., 8th. problems                     */
    SInt            sScope;
    sStrPos     = NULL;
    sBracketPos = NULL;
    sSymTE      = NULL;
    sScope      = aScope;

    IDE_TEST_RAISE( (aScope < 0) || (aScope > MAX_SCOPE_DEPTH),
                    ERR_COMP_SCOPE_DEPTH );

    // host ���� ���� ':' ����
    if ( aName[0] == ':' )
    {
        idlOS::snprintf( sTmpStr, MAX_HOSTVAR_NAME_SIZE * 2,
                         "%s", aName+1 );
    }
    else
    {
        idlOS::snprintf( sTmpStr, MAX_HOSTVAR_NAME_SIZE * 2,
                         "%s", aName );
    }

    sStrPos = idlOS::strchr( sTmpStr, '.' );
    if ( sStrPos == NULL )
    {
        sStrPos = idlOS::strstr( sTmpStr, "->" );
        if( sStrPos != NULL )
        {
            *sStrPos = '\0';
            sStrPos += 2;
            /*BUG-26376 [valgrind bug] : '['ó����. */
            sBracketPos = idlOS::strchr( sTmpStr, '[' );
            if ( sBracketPos != NULL )
            {
                *sBracketPos = '\0';
            }
        }
        else
        {
            /*BUG-26376 [valgrind bug] : '['ó����. */
            sBracketPos = idlOS::strchr( sTmpStr, '[' );
            if ( sBracketPos != NULL )
            {
                *sBracketPos = '\0';
            }
        }
    }
    else
    {
        *sStrPos = '\0';
        sStrPos += 1;
        /*BUG-26376 [valgrind bug] : '['ó����. */
        sBracketPos = idlOS::strchr( sTmpStr, '[' );
        if ( sBracketPos != NULL )
        {
            *sBracketPos = '\0';
        }
    }

    /* BUG-45685 A member array variable of a structure strips */
    if (sStrPos != NULL)
    {
        sBracketPos = idlOS::strchr( sStrPos, '[' );
        if ( sBracketPos != NULL )
        {
            *sBracketPos = '\0';
        }
    }
    else
    {
        /* A obsolete convention */
    }

    // :xxx, :xxx.yyy, :xxx->yyy,
    // :xxx[], :xxx[].yyy, :xxx[]->yyy �� xxx �κ��� �˻��Ѵ�.
    for ( ; sScope >= 0 ; sScope-- )
    {
        sSymT = mScopeTable[sScope];
        if ( sSymT != NULL )
        {
            sSymTE = sSymT->ulpSymLookup( sTmpStr );
            if( sSymTE == NULL )
            {
                continue;
            }
            else
            {
                break;
            }
        }
    }

    /* BUG-30379 : �߸��� struct�����͸� ȣ��Ʈ ������ ���� �ʵ带 �����ϸ� �Ľ��� segV�߻�. */
    // NULL�� �����ϸ� �ļ����� �����޽��� �������.
    IDE_TEST( sSymTE == NULL );

    /* BUG-28118 : system ������ϵ鵵 �Ľ̵ž���.            *
     * 5th. problem : ���ǵ��� ���� ����ü ������ ���� ����ȵ�. *
     * 8th. problem : can't resolve extern variable type at declaring section. */
    if( (sSymTE -> mIsstruct == ID_TRUE)   &&
        (sSymTE -> mStructName[0] != '\0') &&
        (sSymTE -> mStructLink == NULL)
      )
    {
        if ( (sSymTE -> mPointer > 0) ||
             (sSymTE -> mIsExtern == ID_TRUE) )
        {   // ���ǵ��� ���� ����ü ������ ���� Ȥ�� extern���� ���
            // ���� ���� late binding.
            // ulpStructLookupAll scope���ڿ� aScope�� �ƴ�
            // ����� ������ scope�� sScope�� ����ؾ���.
            sStructNode = gUlpStructT.ulpStructLookupAll( sSymTE->mStructName,
                                                          sScope );

            IDE_TEST_RAISE( sStructNode == NULL, ERR_COMP_STRUCT_NOT_EXIST );

            sSymTE -> mStructLink = sStructNode;
        }
    }

    // :xxx.yyy, :xxx->yyy,
    // :xxx[].yyy, :xxx[]->yyy �� yyy �κ��� �˻��Ѵ�.
    if( sStrPos != NULL )
    {
        IDE_ASSERT( sSymTE != NULL );

        // struct table �˻��� attribute �˻���.
        sStructNode = sSymTE->mStructLink;
        IDE_TEST( sStructNode == NULL );

        sSymTE = sStructNode->mChild->ulpSymLookup( sStrPos );
        IDE_TEST( sSymTE == NULL );

        /* BUG-28118 : system ������ϵ鵵 �Ľ̵ž���.            *
         * 5th. problem : ���ǵ��� ���� ����ü ������ ���� ����ȵ�. */
        if( (sSymTE -> mIsstruct == ID_TRUE)   &&
            (sSymTE -> mPointer > 0)           &&
            (sSymTE -> mStructName[0] != '\0') &&
            (sSymTE -> mStructLink == NULL)
          )
        {   // ���ǵ��� ���� ����ü ������ �������,
            // ���� ���� late binding.
            sStructNode = gUlpStructT.ulpStructLookupAll( sSymTE->mStructName,
                                                          sScope );

            IDE_TEST_RAISE( sStructNode == NULL, ERR_COMP_STRUCT_NOT_EXIST );

            sSymTE -> mStructLink = sStructNode;
        }
    }

    return sSymTE;

    IDE_EXCEPTION ( ERR_COMP_SCOPE_DEPTH );
    {
        ulpErrorMgr mErrorMgr;
        ulpSetErrorCode( &mErrorMgr,
                         ulpERR_ABORT_COMP_Invalid_Scope_Depth_Error,
                         aScope );
        ulpPrintfErrorCode( stderr,
                            &mErrorMgr);
    }
   /* BUG-28118 : system ������ϵ鵵 �Ľ̵ž���.            *
    * 5th. problem : ���ǵ��� ���� ����ü ������ ���� ����ȵ�. *
    * 8th. problem : can't resolve extern variable type at declaring section. */
    IDE_EXCEPTION ( ERR_COMP_STRUCT_NOT_EXIST );
    {
        ulpErrorMgr mErrorMgr;
        ulpSetErrorCode( &mErrorMgr,
                         ulpERR_ABORT_COMP_Type_Not_Exist_Error,
                         sSymTE->mStructName );
        ulpPrintfErrorCode( stderr,
                            &mErrorMgr);
    }
    IDE_EXCEPTION_END;

    return NULL;
}


/* ���� scope���� ��� ���������� symbol table���� �����Ѵ�. */
void ulpScopeTable::ulpSDelScope( SInt aScope )
{
    if( mScopeTable[aScope] != NULL )
    {
        mScopeTable[aScope]->ulpFinalize();
        delete mScopeTable[aScope];
        mScopeTable[aScope] = NULL;
    }
}

void ulpScopeTable::ulpPrintAllSymT( void )
{
    SInt sI;
    const SChar sLineB[90] = // + 3 + 5 + 15 + 10 + 7 + 5 + 10 + 6 + 10 + 6 + (total len=86)
            "+---+-----+---------------+----------+-------+-----+----------+------+----------+------+\n";

    idlOS::printf( "\n\n[[ SYMBOL TABLE ]]\n" );
    idlOS::printf( "%s", sLineB );
    idlOS::printf( "|%-3s|%-5s|%-15s|%-10s|%-7s|%-5s|%-10s|%-6s|%-10s|%-6s|\n",
                   "No.","Scope","ID","TYPE","Typedef","Array","ArraySize","Struct","S_name","Signed" );

    for( sI = 0 ; sI < SCOPE_TABLE_SIZE ; sI++)
    {
        if( mScopeTable[sI] != NULL )
        {
            mScopeTable[sI]->ulpPrintSymT( sI );
        }
    }

    idlOS::printf( "%s", sLineB );
    idlOS::fflush(NULL);
}

SInt ulpScopeTable::ulpGetDepth(void)
{
    return mCurDepth;
}

void ulpScopeTable::ulpSetDepth(SInt aDepth)
{
    mCurDepth = aDepth;
}

void ulpScopeTable::ulpIncDepth(void)
{
    mCurDepth++;
}

void ulpScopeTable::ulpDecDepth(void)
{
    mCurDepth--;
}
