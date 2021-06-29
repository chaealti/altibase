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
 * $Id: altiWrapParseMgr.cpp 82075 2018-01-17 06:39:52Z jina.kim $ 
 **********************************************************************/

#include <altiWrapParseMgr.h>
#include <altiWrapEncrypt.h>
#include "altiWraplx.h"

#if defined(BISON_POSTFIX_HPP)
#include "altiWraply.hpp"
#else /* BISON_POSTFIX_CPP_H */
#include "altiWraply.cpp.h"
#endif

#include "altiWrapll.h"



extern int altiWraplparse(void * aParam);

IDE_RC altiWrapParseMgr::parseIt( altiWrap * aAltiWrap )
{
    altiWrapText * sPlainText = NULL;

    IDE_DASSERT( aAltiWrap != NULL );

    sPlainText = aAltiWrap->mPlainText;

    altiWraplLexer  s_altiWraplLexer( sPlainText->mText,
                                      sPlainText->mTextLen,
                                      0,
                                      sPlainText->mTextLen,
                                      0 );

    IDE_TEST( parseInternal( aAltiWrap, & s_altiWraplLexer )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC altiWrapParseMgr::parseInternal( altiWrap         * aAltiWrap,
                                        altiWraplLexer   * aLexer )
{
    altiWraplx   s_altiWraplx;
    SChar      * sPlainText    = NULL;
    SInt         sPlainTextLen = 0;
    SInt         sState        = 0;

    IDE_DASSERT( aAltiWrap != NULL );
    IDE_DASSERT( aLexer != NULL );

    // set members of altiWraplx
    s_altiWraplx.mLexer            = aLexer;
    s_altiWraplx.mLexer->mAltiWrap = aAltiWrap;
    s_altiWraplx.mAltiWrap         = aAltiWrap;
   
    /* syntax error�� �߻��ߴٴ� �ǹ̴� tokenization error �̰ų�,
       pms statement�� �ƴ� ���̴�.
       �̷� ���, input text���� �о �����س��� text�� �״��
       aAltiWrap->mEncryptedText�� ���� �� �ش�. 
       �̴� output file�� input ������ ����ǵ��� ���ֱ� �����̴�. */ 
    if ( altiWraplparse(&s_altiWraplx) != IDE_SUCCESS )
    {
        sPlainTextLen = aAltiWrap->mPlainText->mTextLen;

        /* sPlainText�� ���� free�� altiWrapi::finalizeAltiWrap���� �̷������. */
        sPlainText = (SChar *)idlOS::malloc( sPlainTextLen + 1 );
        IDE_TEST_RAISE( sPlainText == NULL, ERR_ALLOC_MEMORY );
        sState = 1;

        idlOS::memcpy( sPlainText,
                       aAltiWrap->mPlainText->mText,
                       sPlainTextLen );

        IDE_TEST( altiWrapEncrypt::setEncryptedText( aAltiWrap,
                                                     sPlainText,
                                                     sPlainTextLen,
                                                     ID_FALSE )
                  != IDE_SUCCESS );
    }
    else /* create psm statement */
    {
        IDE_TEST( altiWrapEncrypt::doEncryption( aAltiWrap )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_ALLOC_MEMORY )
    {
        uteSetErrorCode( aAltiWrap->mErrorMgr, utERR_ABORT_MEMORY_ALLOCATION );
        utePrintfErrorCode( stdout, aAltiWrap->mErrorMgr);
    }
    IDE_EXCEPTION_END;

    if ( sState == 1 )
    {
        idlOS::free( sPlainText );
        sPlainText = NULL;
    }
    else
    {
        // Nothing to do.
    }

    return IDE_FAILURE;
}

void altiWrapPreLexer::initialize( FILE  * aFP,
                                   SChar * aBuffer,
                                   SInt    aMaxSize )
{
    IDE_DASSERT( aFP != NULL );
    IDE_DASSERT( aBuffer != NULL );
    IDE_DASSERT( aMaxSize != 0 );

    mFP       = aFP;
    mBuffer   = aBuffer;
    mSize     = 0;
    mMaxSize  = aMaxSize;
    mIsEOF    = ID_FALSE;
}


void altiWrapPreLexer::append( SChar * aString )
{
    SInt sSize = 0;

    IDE_DASSERT( aString != NULL );

    sSize = idlOS::strlen( aString );

    idlOS::memcpy( mBuffer + mSize, aString, sSize );
    mSize += sSize;
}
