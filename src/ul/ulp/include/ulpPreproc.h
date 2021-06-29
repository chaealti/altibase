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

#ifndef _ULP_PREPROC_H_
#define _ULP_PREPROC_H_ 1

#include <idl.h>
#include <ide.h>
#include <idn.h>
#include <iduMemory.h>
#include <ulpProgOption.h>
#include <ulpGenCode.h>
#include <ulpMacroTable.h>
#include <ulpScopeTable.h>
#include <ulpSymTable.h>
#include <ulpIfStackMgr.h>
#include <ulpErrorMgr.h>
#include <ulpMacro.h>


//========= For Parser ==========//

// parsing function at ulpPreprocy.y
int doPPparse( SChar *aFilename );



//========= For Lexer ==========//


/*
 * Lexer ���Ǵ� ��� function���� ���´�.
 */

IDE_RC ulpPPInitialize( SChar *aFileName );

void   ulpPPFinalize();

/* ���� buffer ���� ����ü ����. (YY_CURRENT_BUFFER) */
void   ulpPPSaveBufferState( void );

/* C comment ó�� �Լ� */
IDE_RC ulpPPCommentC( void );

/* #if ���� C comment ó�� �Լ� (newline�� file�� write�Ѵ�.) */
IDE_RC ulpPPCommentC4IF( void );

/* C++ comment ó�� �Լ� */
void   ulpPPCommentCplus( void );

/* #if ���� C++ comment ó�� �Լ� (file�� write ���� �ʴ´�.)*/
void   ulpPPCommentCplus4IF( void );

/* macro �������� '//n'���ڸ� ���� ���ִ� �Լ�. */
void   ulpPPEraseBN4MacroText(SChar *aTmpDefinedStr, idBool aIsIf);

/* ó�� ���ʿ� ���� macro ������ skip���ִ� �Լ�. */
IDE_RC ulpPPSkipUnknownMacro( void );

/* lexer�� start condition�� �������ִ� �Լ�. */
void   ulpPPRestoreCond(void);

/* yyinput ��� */
SChar  ulpPPYYinput(void);

/* unput ��� */
void  ulpPPYYunput( SChar aCh );

/*
 * extern functions
 */

/* for delete output files when error occurs. */
extern void ulpFinalizeError();

#endif
