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
 * $Id: altiWrapMain.cpp 82075 2018-01-17 06:39:52Z jina.kim $
 **********************************************************************/

#include <altiWrap.h>
#include <altiWrapi.h>
#include <altiWrapFileMgr.h>
#include <altiWrapParseMgr.h>
#include <altiWrapEncrypt.h>



extern SInt altiWrapPreLexerlex();

altiWrapPreLexer * gPreLexer = NULL;

acp_sint32_t main( acp_sint32_t aArgc, acp_char_t *aArgv[] )
{
/***********************************************************
 * Description :
 *     altiWrap�� main�Լ�
 *     altiWrap�� syntax�� �Ʒ��� ����.
 *         + altiwrap --iname inpath (--oname outpath) 
 **********************************************************/

    SInt       sState      = 0;
    altiWrap * sAltiWrap   = NULL;
    /* read file */
    FILE     * sFP         = NULL;
    SChar    * sText       = NULL;
    SInt       sTextLen    = 0;
    /* for command */
    idBool     sHasHelpOpt = ID_FALSE;
    /* print result */
    SChar    * sInPath     = NULL;
    SChar    * sOutPath    = NULL;

    if ( aArgc > 1 )
    {
        IDE_TEST( altiWrapi::allocAltiWrap( &sAltiWrap ) != IDE_SUCCESS );
        sState = 1;

        gPreLexer = ( altiWrapPreLexer *)idlOS::calloc( 1, ID_SIZEOF( altiWrapPreLexer ) );
        IDE_TEST_RAISE( gPreLexer == NULL, ERR_ALLOC_MEMORY );
        sState = 2;

        /* Parsing Command */
        IDE_TEST( altiWrapi::parsingCommand( sAltiWrap,
                                             aArgc,
                                             aArgv,
                                             &sHasHelpOpt )
                  != IDE_SUCCESS );

        if ( sHasHelpOpt == ID_FALSE )
        {
            sInPath  = sAltiWrap->mFilePathInfo->mInFilePath;
            sOutPath = sAltiWrap->mFilePathInfo->mOutFilePath;

            /* ������ ���� */
            sFP = idlOS::fopen( sInPath, "r" );
            IDE_TEST_RAISE( sFP == NULL , FAIL_OPEN_FILE );

            /* ���Ͽ� �ִ� �ؽ�Ʈ�� ��ü ���̸� ���Ѵ�.
               �� ���̴� parsing �� ���̴� �޸��� �ִ� ũ��� ����Ѵ�.
               prelexer�� ��ġ�鼭 ���̰� ���� ( new line �߰��ǰų� �� ) */
            (void)idlOS::fseek( sFP, 0, SEEK_END );
            sTextLen = idlOS::ftell( sFP ) + 1;

            /* parsing�� ���� ���� �޸𸮸� �Ҵ�޴´�.
               �ش� �޸𸮴� input file�� ũ�� + 1�� �ȴ�. */
            sText    = (SChar *)idlOS::calloc(1, sTextLen + 1 ); 
            IDE_TEST_RAISE( sText == NULL, ERR_ALLOC_MEMORY );
            sState = 3;

            /* prelexer�� �ʱ�ȭ ���� */
            (void)idlOS::fseek( sFP, 0, SEEK_SET );

            /* �ϳ��� statemtmt�� text���� ������ ���� prelexer�� �Ѵ�. */
            gPreLexer->initialize( sFP, sText, sTextLen );

            while ( 1 )
            {
                /* prelexer������ prelexer�� rule�� ����
                   �ϳ��� statement�� �о�´�. */
                (void) altiWrapPreLexerlex();

                if ( idlOS::strlen(gPreLexer->mBuffer) != 0 )
                {
                    /* parsing�ϱ� ���� statement ���� */
                    sAltiWrap->mPlainText->mTextLen = idlOS::strlen(gPreLexer->mBuffer);
                    sAltiWrap->mPlainText->mText    = gPreLexer->mBuffer;
                    sAltiWrap->mPlainText->mText[sAltiWrap->mPlainText->mTextLen] = '\0';

                    /* parsing text
                       create psm statement�̸� ��ȣȭ�Ͽ� sAltiWrap->mEncryptedTextList�� �ް�,
                       �� ���� statement�̸�, plain text�� �״�� �մ´�. */
                    IDE_TEST( altiWrapParseMgr::parseIt( sAltiWrap ) != IDE_SUCCESS );
                }
                else
                {
                    /* gPreLexer->mBuffer�� new line�� ��� */
                    altiWrapEncrypt::setEncryptedText(
                        sAltiWrap,
                        NULL,
                        0,
                        ID_TRUE );
                }

                /* ������ �� �о�����, �Ҵ�� �޸𸮸� �����ϰ�,
                   ����������, �ʱ�ȭ�Ͽ� ���� statement�� ���� �غ� �Ѵ�. */
                if ( gPreLexer->mIsEOF == ID_TRUE )
                {
                    sState = 2;
                    idlOS::free( sText );
                    sText = NULL;
                    break;
                }
                else
                {
                    gPreLexer->mSize = 0;
                    idlOS::memset( gPreLexer->mBuffer, 0x00, gPreLexer->mMaxSize + 1 );
                }
            }

            /* ������ �ݴ´�. */
            idlOS::fclose( sFP );

            /* Write encrypted text to output file */
            IDE_TEST( altiWrapFileMgr::writeFile( sAltiWrap,
                                                  sAltiWrap->mFilePathInfo )
                      != IDE_SUCCESS );

            idlOS::fprintf( stdout, "Processing %s to %s\n", sInPath, sOutPath );
            idlOS::fflush( stdout );
        }
        else
        {
            // Nothing to do.
        }

        sState = 1;
        idlOS::free( gPreLexer );
        gPreLexer = NULL;

        sState = 0;
        altiWrapi::finalizeAltiWrap( sAltiWrap );
    }
    else
    {
        /* ex) SHELL> altiwrap (enter) 
           �ƹ��� �������� ������, error�߻����� ����. */
        /* Nothing to do. */
    }

    return 0;

    IDE_EXCEPTION( ERR_ALLOC_MEMORY )
    {
        uteSetErrorCode( sAltiWrap->mErrorMgr,
                         utERR_ABORT_MEMORY_ALLOCATION );
        utePrintfErrorCode( stdout, sAltiWrap->mErrorMgr);
    }
    IDE_EXCEPTION( FAIL_OPEN_FILE );
    {
        uteSetErrorCode( sAltiWrap->mErrorMgr,
                         utERR_ABORT_openFileError,
                         sAltiWrap->mFilePathInfo->mInFilePath );
        utePrintfErrorCode( stdout, sAltiWrap->mErrorMgr);
    }
    IDE_EXCEPTION_END;

    switch ( sState )
    {
        case 3:
            idlOS::free( sText );
            sText = NULL;
        case 2:
            idlOS::free( gPreLexer );
            gPreLexer = NULL;
        case 1:
            altiWrapi::finalizeAltiWrap( sAltiWrap );
        case 0:
            break;
        default:
            break;
    }

    return -1;
}
