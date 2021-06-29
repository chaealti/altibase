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

#include <ulpMacroTable.h>


ulpMacroTable::ulpMacroTable()
{

}

void ulpMacroTable::ulpInit()
{
    SInt sI;
    for (sI = 0; sI < MAX_SYMTABLE_ELEMENTS; sI++)
    {
        mMacroTable[sI] = NULL;
    }
    mCnt         = 0;
    mHash        = ulpHashFunc;
    mSize        = MAX_SYMTABLE_ELEMENTS;
}

ulpMacroTable::~ulpMacroTable()
{

}


void ulpMacroTable::ulpFinalize()
{
    SInt sI;
    ulpMacroNode *sNode;
    ulpMacroNode *sPNode;

    for (sI = 0; sI < MAX_SYMTABLE_ELEMENTS; sI++)
    {
        sNode = mMacroTable[sI];
        while (sNode != NULL)
        {
            sPNode  = sNode;
            sNode   = sNode->mNext;
            idlOS::free(sPNode);
        }
        /* BUG-28061 : preprocessing����ġ�� marco table�� �ʱ�ȭ�ϰ�, *
         *             ulpComp ���� �籸���Ѵ�.                       */
        mMacroTable[sI] = NULL;
    }
}

/* BUG-28118 : system ������ϵ鵵 �Ľ̵ž���.                    *
 * 11th. problem : C preoprocessor���� ��ũ�� �Լ� ���� ó�� ����. *
 * 12th. problem : C preprocessor���� �� ��ū�� concatenation�Ҷ� ���Ǵ� '##' ��ū ó���ؾ���. */
void ulpMacroTable::ulpMEraseSharp4MFunc( SChar *aText )
{
/***********************************************************************
 *
 * Description :
 *    Macro �Լ��� �ð�� parameter��� �Լ����� '##' (concatenation) ��ū��
 *    ó���ؾ��ϴµ� ������ �ʾ�, �ϴ� text���� '##'�� ���� �Ľ̿����� ����.
 *
 * Implementation :
 *
 ***********************************************************************/
    SInt   i;
    SInt   sLen;
    SInt   sShift;
    idBool sDquote;
    idBool sSquote;

    sShift  = 0;
    sDquote = sSquote = ID_FALSE;
    sLen    = idlOS::strlen( aText );

    for( i = 0 ; i < sLen ; i++ )
    {
        switch( aText[i] )
        {
            case '#':
                if ( (sDquote == ID_FALSE) && (sSquote == ID_FALSE) )
                {
                    if ( aText[i+1] == '#' )
                    {
                        i ++;
                        sShift += 2;
                    }
                }
                else
                {
                    aText[i-sShift] = '#';
                }
                break;
            case '"':
                if ( sDquote == ID_TRUE )
                {
                    sDquote = ID_FALSE;
                }
                else
                {
                    sDquote = ID_TRUE;
                }
                aText[i-sShift] = '"';
                break;
            case '\'':
                if ( sSquote == ID_TRUE )
                {
                    sSquote = ID_FALSE;
                }
                else
                {
                    sSquote = ID_TRUE;
                }
                aText[i-sShift] = '\'';
                break;
            default:
                aText[i-sShift] = aText[i];
                break;
        }
    }

    aText[i-sShift] = '\0';
}

// defined macro�� hash table�� �����Ѵ�.
IDE_RC ulpMacroTable::ulpMDefine ( SChar *aName, SChar *aText, idBool aIsFunc )
{
    SInt             sIndex;
    ulpMacroNode    *sMNode;
    ulpMacroNode    *sMNode2;

    /* BUG-28118 : system ������ϵ鵵 �Ľ̵ž���.                    *
     * 11th. problem : C preoprocessor���� ��ũ�� �Լ� ���� ó�� ����. *
     * 12th. problem : C preprocessor���� �� ��ū�� concatenation�Ҷ� ���Ǵ� '##' ��ū ó���ؾ���. */
    if( (aIsFunc == ID_TRUE) && (aText != NULL) )
    {
        ulpMEraseSharp4MFunc( aText );
    }

    // Does same name already exist?
    if ( (sMNode = ulpMLookup( aName )) == NULL )
    {
        // no
        // alloc new node
        /* BUG-32413 APRE memory allocation failure should be fixed */
        sMNode = (ulpMacroNode *) idlOS::malloc( ID_SIZEOF(ulpMacroNode) );
        IDE_TEST_RAISE( sMNode == NULL, ERR_MEMORY_ALLOC );
        idlOS::memset(sMNode, 0, ID_SIZEOF(ulpMacroNode) );

        // set fields
        idlOS::strncpy( sMNode->mName, aName, MAX_MACRO_DEFINE_NAME_LEN - 1 );
        if ( aText != NULL )
        {
            // PPIF flex lexer �� ���� ó�� ���� ������,
            // The last two bytes of mText must be ASCII NUL.
            idlOS::strncpy( sMNode->mText, aText, MAX_MACRO_DEFINE_CONTENT_LEN - 2 );
        }
        else
        {
            // memset �ؼ� ���ʿ� ������~
            // PPIF flex lexer �� ���� ó�� ���� ������,
            // The last two bytes of mText must be ASCII NUL.
            sMNode->mText[0] = '\0';
            sMNode->mText[1] = '\0';
        }
        sMNode->mIsFunc = aIsFunc;

        // add to hash table
        sIndex = (*mHash)( (UChar *)sMNode->mName ) % ( mSize );

        // bucket list�� ���� �տ��� �߰���.
        sMNode2 = mMacroTable[sIndex];

        mMacroTable[sIndex] = sMNode;

        sMNode->mNext = sMNode2;

        // ��ü macro �� ����
        mCnt++;
    }
    else
    {
        // yes
        // overwrite fields... that's it
        idlOS::strncpy( sMNode->mName, aName, MAX_MACRO_DEFINE_NAME_LEN - 1 );
        if ( aText != NULL )
        {
            idlOS::memset( sMNode->mText, 0 , MAX_MACRO_DEFINE_CONTENT_LEN );
            idlOS::strncpy( sMNode->mText, aText, MAX_MACRO_DEFINE_CONTENT_LEN - 2 );
        }
        else
        {
            sMNode->mText[0] = '\0';
            sMNode->mText[1] = '\0';
        }
        sMNode->mIsFunc = aIsFunc;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION ( ERR_MEMORY_ALLOC );
    {
        //ulpErrorMgr mErrorMgr;
        //ulpSetErrorCode( &mErrorMgr,
        //                 ulpERR_ABORT_Memory_Alloc_Error );
        //ulpPrintfErrorCode( stderr,
        //                    &mErrorMgr);
        //IDE_ASSERT(0);
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


// Ư�� �̸��� ���� defined macro�� hash table���� �˻��Ѵ�.
ulpMacroNode *ulpMacroTable::ulpMLookup( SChar *aName )
{
    SInt sIndex;
    ulpMacroNode *sMNode;

    IDE_TEST( mCnt == 0 );

    sIndex = (*mHash)( (UChar *)aName ) % ( mSize );
    sMNode = mMacroTable[sIndex];
    while ( ( sMNode != NULL ) &&
            idlOS::strcmp( aName, sMNode->mName )  )
    {
        sMNode = sMNode->mNext;
    }

    return sMNode;

    IDE_EXCEPTION_END;

    return NULL;
}


// Ư�� �̸��� ���� defined macro�� hash table���� �����Ѵ�.
void ulpMacroTable::ulpMUndef( SChar *aName )
{
    SInt sIndex;
    ulpMacroNode *sMNode;
    ulpMacroNode *sPMNode;

    sPMNode = NULL;
    sIndex = (*mHash)( (UChar *)aName ) % ( mSize );
    sMNode = mMacroTable[sIndex];

    while (  sMNode != NULL )
    {
        if ( !idlOS::strcmp( aName, sMNode->mName ) )
        {
            if( sPMNode == NULL )
            {
                mMacroTable[sIndex] = sMNode->mNext;
            }
            else
            {
                sPMNode -> mNext = sMNode->mNext;
            }
            idlOS::free(sMNode);
            mCnt--;
            break;
        }
        sPMNode = sMNode;
        sMNode = sMNode -> mNext;
    }
}


// for debugging
void ulpMacroTable::ulpMPrint( void )
{
    SInt   sI, sJ;
    SInt   sCnt;
    SInt   sMLineCnt;
    SInt   sIDcnt;
    SInt   sTEXTcnt;
    idBool sIsIDEnd;    // id ���̰� 15�ڸ� ���� ��� ���� ���ο� ����ϱ� ���� ����.
    idBool sIsTEXTEnd;  // text���̰� 45�ڸ� ���� ��� ����.
    idBool sIsFirst;
    ulpMacroNode *sNode;

    /*BUG-28414*/
    sMLineCnt = 1;
    sIDcnt    = 0;
    sTEXTcnt  = 0;
    sCnt = 1;
    sIsIDEnd   = ID_FALSE;
    sIsTEXTEnd = ID_FALSE;
    sIsFirst   = ID_TRUE;
    const SChar sLineB[80] = // + 3 + 15 + 45 + 6 +  (total len=74)
            "+---+---------------+---------------------------------------------+------+\n";

    idlOS::printf( "\n\n[[ MACRO TABLE (total num.:%d)]]\n", mCnt );
    idlOS::printf( "%s", sLineB );
    idlOS::printf( "|%-3s|%-15s|%-45s|%-6s|\n",
                   "No.","ID","TEXT","isFunc" );

    for ( sI = 0 ; sI < MAX_SYMTABLE_ELEMENTS; sI++)
    {
        sNode = mMacroTable[sI];
        while (sNode != NULL)
        {
            if( sIsFirst == ID_TRUE )
            {
                sMLineCnt = 1;
                idlOS::printf( "%s", sLineB );
                idlOS::printf( "|%-3d|", sCnt++ );
            }
            else
            {
                idlOS::printf( "|   |" ); // 3 spaces
            }

            if( sIsIDEnd != ID_TRUE )
            {
                for( (sIsFirst == ID_TRUE )?sIDcnt = 0 : sIDcnt=sIDcnt
                     ; sIDcnt < 15 * sMLineCnt ; sIDcnt++ )
                {
                    if( sNode->mName[sIDcnt] == '\0' )
                    {
                        sIsIDEnd = ID_TRUE;
                        for( sJ = 15 * sMLineCnt - sIDcnt ; sJ > 0 ; sJ-- )
                        {
                            idlOS::printf(" ");
                        }
                        break;
                    }
                    else
                    {
                        idlOS::printf( "%c", sNode->mName[sIDcnt] );
                    }
                }
            }
            else
            {
                for( sJ = 0 ; sJ < 15 ; sJ++ )
                {
                    idlOS::printf(" ");
                }
            }

            idlOS::printf( "|" );

            if( sIsTEXTEnd != ID_TRUE )
            {
                for( (sIsFirst == ID_TRUE )?sTEXTcnt = 0:sTEXTcnt=sTEXTcnt
                    ; sTEXTcnt < (45 * sMLineCnt) ; sTEXTcnt++ )
                {
                    if( sNode->mText[sTEXTcnt] == '\0' )
                    {
                        sIsTEXTEnd = ID_TRUE;
                        for( sJ = 45 * sMLineCnt - sTEXTcnt ; sJ > 0 ; sJ-- )
                        {
                            idlOS::printf(" ");
                        }
                        break;
                    }
                    else
                    {
                        if ( sNode->mText[sTEXTcnt] == '\t' )
                        {
                            idlOS::printf(" ");
                        }
                        else
                        {
                            idlOS::printf( "%c", sNode->mText[sTEXTcnt] );
                        }
                    }
                }
            }
            else
            {
                for( sJ = 0 ; sJ < 45 ; sJ++ )
                {
                    idlOS::printf(" ");
                }
            }

            idlOS::printf( "|" );

            if ( sIsFirst )
            {
                idlOS::printf( "%-6c|\n", (sNode->mIsFunc == ID_TRUE)?'O':'X' );
            }
            else
            {
                idlOS::printf( "      |\n" ); // 6 spaces
            }

            if ( ( sIsIDEnd == ID_FALSE ) || ( sIsTEXTEnd == ID_FALSE ) )
            {
                sMLineCnt++;
                sIsFirst = ID_FALSE;
                continue;
            }

            sNode   = sNode->mNext;
            sIsFirst = ID_TRUE;
            sIsIDEnd   = ID_FALSE;
            sIsTEXTEnd = ID_FALSE;
        }
    }

    idlOS::printf( "%s\n", sLineB );
}

