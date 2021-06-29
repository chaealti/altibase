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


%pure_parser

%union
{
    char *strval;
}

%initial-action
{
    // parse option�� ���� �ʱ� ���¸� ��������.
    switch ( gUlpProgOption.mOptParseInfo )
    {
        case PARSE_NONE :
            gUlpPPStartCond = PP_ST_INIT_SKIP;
            break;
        case PARSE_PARTIAL :
        case PARSE_FULL :
            gUlpPPStartCond = PP_ST_MACRO;
            break;
        default :
            break;
    }
};


%{

#include <ulpPreproc.h>


#undef YY_READ_BUF_SIZE
#undef YY_BUF_SIZE
#define YY_READ_BUF_SIZE (16384)
#define YY_BUF_SIZE (YY_READ_BUF_SIZE * 2) /* size of default input buffer */

//============== global variables for PPparse ================//

// parser�� ���� ���¸� �����ϴ� ����.
SInt            gUlpPPStartCond = PP_ST_NONE;
// parser�� ���� ���·� �����ϱ����� ����.
SInt            gUlpPPPrevCond  = PP_ST_NONE;

// PPIF parser���� ���Ǵ� ���� ����/�� pointer
SChar *gUlpPPIFbufptr = NULL;
SChar *gUlpPPIFbuflim = NULL;

// �Ʒ��� macro if ó�� class�� ulpPPLexer Ŭ���� ������ �ְ� ������,
// sun��񿡼� initial ���� �ʴ� �˼����� �������־� ������ �ٻ�.
ulpPPifstackMgr *gUlpPPifstackMgr[MAX_HEADER_FILE_NUM];
// gUlpPPifstackMgr index
SInt             gUlpPPifstackInd = -1;

/* externs of PPLexer */
// include header �Ľ������� ����.
extern idBool        gUlpPPisCInc;
extern idBool        gUlpPPisSQLInc;

/* externs of ulpMain.h */
extern ulpProgOption gUlpProgOption;
extern ulpCodeGen    gUlpCodeGen;
extern iduMemory    *gUlpMem;
// Macro table
extern ulpMacroTable gUlpMacroT;
// preprocessor parsing �� �߻��� ���������� �ڵ�����.
extern SChar        *gUlpPPErrCode;

//============================================================//


//============ Function declarations for PPparse =============//

// Macro if ���� ó���� ���� parse �Լ�
extern SInt PPIFparse ( void *aBuf, SInt *aRes );
extern int  PPlex  ( YYSTYPE *lvalp );
extern void PPerror( const SChar* aMsg );

extern void ulpFinalizeError(void);

//============================================================//

%}

/*** EmSQL tokens ***/
%token EM_OPTION_INC
%token EM_INCLUDE
%token EM_SEMI
%token EM_EQUAL
%token EM_COMMA
%token EM_LPAREN
%token EM_RPAREN
%token EM_LBRAC
%token EM_RBRAC
%token EM_DQUOTE
%token EM_PATHNAME
%token EM_FILENAME

/*** MACRO tokens ***/
%token M_INCLUDE
%token M_DEFINE
%token M_IFDEF
%token M_IFNDEF
%token M_UNDEF
%token M_ENDIF
%token M_IF
%token M_ELIF
%token M_ELSE
%token M_SKIP_MACRO
%token M_LBRAC
%token M_RBRAC
%token M_DQUOTE
%token M_FILENAME
%token M_DEFINED
%token M_NUMBER
%token M_CHARACTER
%token M_BACKSLASHNEWLINE
%token M_IDENTIFIER
%token M_FUNCTION
%token M_STRING_LITERAL
%token M_RIGHT_OP
%token M_LEFT_OP
%token M_OR_OP
%token M_LE_OP
%token M_GE_OP
%token M_LPAREN
%token M_RPAREN
%token M_AND_OP
%token M_EQ_OP
%token M_NE_OP
%token M_NEWLINE
%token M_CONSTANT
%%

preprocessor
    :
    | preprocessor Emsql_grammar
    {
        /* ���� ���·� ���� SKIP_NONE ���� Ȥ�� MACRO���� ���� emsql������ �Ľ��� �� �ֱ⶧�� */
        gUlpPPStartCond = gUlpPPPrevCond;
    }
    | preprocessor Macro_grammar
    {
        /* macro ���ǹ��� ��� ���̸� MACRO����, �����̸� MACRO_IFSKIP ���·�
         * ���� �ȴ�. ������ ���� MACRO���·� ���̵�. */
    }
    ;

Emsql_grammar
    : Emsql_include
    {
        /* EXEC SQL INCLUDE ... */
        SChar sStrtmp[MAX_FILE_PATH_LEN];

        // ����� ��������� ������ �˸���. my girlfriend
        idlOS::snprintf( sStrtmp, MAX_FILE_PATH_LEN, "@$LOVELY.K.J.H$ (%s)\n",
                         gUlpProgOption.ulpGetIncList() );
        WRITESTR2BUFPP( sStrtmp );

        /* BUG-27683 : iostream ��� ���� */
        // 0. flex ���� ���� ����.
        ulpPPSaveBufferState();

        gUlpPPisSQLInc = ID_TRUE;

        // 1. doPPparse()�� ��ȣ���Ѵ�.
        doPPparse( gUlpProgOption.ulpGetIncList() );

        gUlpPPisSQLInc = ID_FALSE;

        // ����� ��������� ���� �˸���.
        WRITESTR2BUFPP((SChar *)"#$LOVELY.K.J.H$\n");

        // 2. precompiler�� ������ directory�� current path�� ��setting
        idlOS::strcpy( gUlpProgOption.mCurrentPath, gUlpProgOption.mStartPath );
    }
    | Emsql_include_option
    {
        /* EXEC SQL OPTION( INCLUDE = ... ) */
        // ���� ��忡�� ó����.
    }
    ;

Emsql_include
    : EM_INCLUDE EM_LBRAC EM_FILENAME EM_RBRAC EM_SEMI
    {
        // Emsql_include ������ ���� �ڿ� �ݵ�� ';' �� �;���.
        // check exist header file in include paths
        if ( gUlpProgOption.ulpLookupHeader( $<strval>3, ID_FALSE )
             == IDE_FAILURE )
        {
            ulpErrorMgr sErrorMgr;
            ulpSetErrorCode( &sErrorMgr,
                             ulpERR_ABORT_FILE_NOT_FOUND,
                             $<strval>3 );
            gUlpPPErrCode = ulpGetErrorSTATE( &sErrorMgr );
            PPerror( ulpGetErrorMSG(&sErrorMgr) );
        }

    }
    | EM_INCLUDE EM_DQUOTE EM_FILENAME EM_DQUOTE EM_SEMI
    {
        // check exist header file in include paths
        if ( gUlpProgOption.ulpLookupHeader( $<strval>3, ID_FALSE )
             == IDE_FAILURE )
        {
            ulpErrorMgr sErrorMgr;
            ulpSetErrorCode( &sErrorMgr,
                             ulpERR_ABORT_FILE_NOT_FOUND,
                             $<strval>3 );
            gUlpPPErrCode = ulpGetErrorSTATE( &sErrorMgr );
            PPerror( ulpGetErrorMSG(&sErrorMgr) );
        }

    }
    | EM_INCLUDE EM_FILENAME EM_SEMI
    {
        // check exist header file in include paths
        if ( gUlpProgOption.ulpLookupHeader( $<strval>2, ID_FALSE )
             == IDE_FAILURE )
        {
            ulpErrorMgr sErrorMgr;
            ulpSetErrorCode( &sErrorMgr,
                             ulpERR_ABORT_FILE_NOT_FOUND,
                             $<strval>2 );
            gUlpPPErrCode = ulpGetErrorSTATE( &sErrorMgr );
            PPerror( ulpGetErrorMSG(&sErrorMgr) );
        }

    }
    ;

Emsql_include_option
    : EM_OPTION_INC EM_EQUAL Emsql_include_path_list EM_RPAREN EM_SEMI
    ;

Emsql_include_path_list
    : EM_FILENAME
    {

        SChar sPath[MAX_FILE_PATH_LEN];

        // path name ���� ���� üũ �ʿ�.
        idlOS::strncpy( sPath, $<strval>1, MAX_FILE_PATH_LEN-1 );

        // include path�� �߰��Ǹ� g_ProgOption.path�� �����Ѵ�.
        if ( sPath[0] == IDL_FILE_SEPARATOR )
        {
            // �������� ���
            idlOS::sprintf( gUlpProgOption.mIncludePathList[gUlpProgOption.mIncludePathCnt],
                            sPath );
        }
        else
        {
            // ������� ���
              idlOS::sprintf( gUlpProgOption.mIncludePathList[gUlpProgOption.mIncludePathCnt],
                              "%s%c%s", gUlpProgOption.mCurrentPath, IDL_FILE_SEPARATOR, sPath);
        }
        gUlpProgOption.mIncludePathCnt++;

    }
    | Emsql_include_path_list EM_COMMA EM_FILENAME
    {

        SChar sPath[MAX_FILE_PATH_LEN];

        idlOS::strncpy( sPath, $<strval>3, MAX_FILE_PATH_LEN-1 );

        // include path�� �߰��Ǹ� g_ProgOption.path�� �����Ѵ�.
        if ( sPath[0] == IDL_FILE_SEPARATOR )
        {
            // �������� ���
            idlOS::sprintf( gUlpProgOption.mIncludePathList[gUlpProgOption.mIncludePathCnt],
                            sPath );
        }
        else
        {
            // ������� ���
              idlOS::sprintf( gUlpProgOption.mIncludePathList[gUlpProgOption.mIncludePathCnt],
                              "%s%c%s", gUlpProgOption.mCurrentPath, IDL_FILE_SEPARATOR, sPath);
        }
        gUlpProgOption.mIncludePathCnt++;

    }
    ;


/**********************************************************
 *                                                        *
 *                      MACRO                             *
 *                                                        *
 **********************************************************/

/*** MACRO tokens ****
M_INCLUDE
M_DEFINE
M_IFDEF
M_UNDEF
M_IFNDEF
M_ENDIF
M_ELIF
M_DEFINED
M_INCLUDE_FILE
M_ELSE
M_IF
**********************/

Macro_grammar
        : Macro_include
        {
            gUlpPPStartCond = PP_ST_MACRO;
        }
        | Macro_define
        {
            gUlpPPStartCond = PP_ST_MACRO;
        }
        | Macro_undef
        {
            gUlpPPStartCond = PP_ST_MACRO;
        }
        | Macro_ifdef
        {
            /* macro ���ǹ��� ��� ���̸� MACRO����, �����̸� MACRO_IFSKIP ���·�
             * ���� �ȴ�. */
        }
        | Macro_ifndef
        {
            /* macro ���ǹ��� ��� ���̸� MACRO����, �����̸� MACRO_IFSKIP ���·�
             * ���� �ȴ�. */
        }
        | Macro_if
        {
            /* macro ���ǹ��� ��� ���̸� MACRO����, �����̸� MACRO_IFSKIP ���·�
             * ���� �ȴ�. */
        }
        | Macro_elif
        {
            /* macro ���ǹ��� ��� ���̸� MACRO����, �����̸� MACRO_IFSKIP ���·�
             * ���� �ȴ�. */
        }
        | Macro_else
        {
            /* macro ���ǹ��� ��� ���̸� MACRO����, �����̸� MACRO_IFSKIP ���·�
             * ���� �ȴ�. */
        }
        | Macro_endif
        ;

Macro_include
        : M_INCLUDE M_LBRAC M_FILENAME M_RBRAC
        {
            /* #include <...> */

            // 1. check exist header file in include paths
            if ( gUlpProgOption.ulpLookupHeader( $<strval>3, ID_TRUE )
                 == IDE_FAILURE )
            {
                //do nothing
            }
            else
            {

                // ���� #include ó����.
                gUlpPPisCInc = ID_TRUE;

                /* BUG-27683 : iostream ��� ���� */
                // 2. flex ���� ���� ����.
                ulpPPSaveBufferState();
                // 3. doPPparse()�� ��ȣ���Ѵ�.
                doPPparse( gUlpProgOption.ulpGetIncList() );
                // ���� #inlcude ó�����̾���? Ȯ����
                gUlpPPisCInc = gUlpProgOption.ulpIsHeaderCInclude();

                // 4. precompiler�� ������ directory�� current path�� ��setting
                idlOS::strcpy( gUlpProgOption.mCurrentPath, gUlpProgOption.mStartPath );
            }

        }
        | M_INCLUDE M_DQUOTE M_FILENAME M_DQUOTE
        {

            // 1. check exist header file in include paths
            if ( gUlpProgOption.ulpLookupHeader( $<strval>3, ID_TRUE )
                 == IDE_FAILURE )
            {
                // do nothing
            }
            else
            {

                // ���� #include ó����.
                gUlpPPisCInc = ID_TRUE;
                /* BUG-27683 : iostream ��� ���� */
                // 2. flex ���� ���� ����.
                ulpPPSaveBufferState();
                // 3. doPPparse()�� ��ȣ���Ѵ�.
                doPPparse( gUlpProgOption.ulpGetIncList() );
                // ���� #inlcude ó�����̾���? Ȯ����
                gUlpPPisCInc = gUlpProgOption.ulpIsHeaderCInclude();

                // 4. precompiler�� ������ directory�� current path�� ��setting
                idlOS::strcpy( gUlpProgOption.mCurrentPath, gUlpProgOption.mStartPath );
            }

        }
        ;

Macro_define
        : M_DEFINE M_IDENTIFIER
        {
            SChar sTmpDEFtext[ MAX_MACRO_DEFINE_CONTENT_LEN ];

            ulpPPEraseBN4MacroText( sTmpDEFtext, ID_FALSE );

            //printf("ID=[%s], TEXT=[%s]\n",$<strval>2,sTmpDEFtext);
            if ( sTmpDEFtext[0] == '\0' )
            {
                // macro symbol table�� �߰���.
                if( gUlpMacroT.ulpMDefine( $<strval>2, NULL, ID_FALSE ) == IDE_FAILURE )
                {
                    ulpErrorMgr sErrorMgr;

                    ulpSetErrorCode( &sErrorMgr,
                                     ulpERR_ABORT_Memory_Alloc_Error );
                    gUlpPPErrCode = ulpGetErrorSTATE( &sErrorMgr );
                    PPerror( ulpGetErrorMSG(&sErrorMgr) );
                }
            }
            else
            {
                // macro symbol table�� �߰���.
                if( gUlpMacroT.ulpMDefine( $<strval>2, sTmpDEFtext, ID_FALSE ) == IDE_FAILURE )
                {
                    ulpErrorMgr sErrorMgr;

                    ulpSetErrorCode( &sErrorMgr,
                                     ulpERR_ABORT_Memory_Alloc_Error );
                    gUlpPPErrCode = ulpGetErrorSTATE( &sErrorMgr );
                    PPerror( ulpGetErrorMSG(&sErrorMgr) );
                }
            }

        }
        | M_DEFINE M_FUNCTION
        {
            // function macro�ǰ�� ���� ������ ���� ������� �ʴ´�.
            SChar sTmpDEFtext[ MAX_MACRO_DEFINE_CONTENT_LEN ];

            ulpPPEraseBN4MacroText( sTmpDEFtext, ID_FALSE );

            // #define A() {...} �̸�, macro id�� A�̴�.
            if ( sTmpDEFtext[0] == '\0' )
            {
                // macro symbol table�� �߰���.
                if ( gUlpMacroT.ulpMDefine( $<strval>2, NULL, ID_TRUE ) == IDE_FAILURE )
                {
                    ulpErrorMgr sErrorMgr;

                    ulpSetErrorCode( &sErrorMgr,
                                     ulpERR_ABORT_Memory_Alloc_Error );
                    gUlpPPErrCode = ulpGetErrorSTATE( &sErrorMgr );
                    PPerror( ulpGetErrorMSG(&sErrorMgr) );
                }
            }
            else
            {
                // macro symbol table�� �߰���.
                if ( gUlpMacroT.ulpMDefine( $<strval>2, sTmpDEFtext, ID_TRUE ) == IDE_FAILURE )
                {
                    ulpErrorMgr sErrorMgr;

                    ulpSetErrorCode( &sErrorMgr,
                                     ulpERR_ABORT_Memory_Alloc_Error );
                    gUlpPPErrCode = ulpGetErrorSTATE( &sErrorMgr );
                    PPerror( ulpGetErrorMSG(&sErrorMgr) );
                }
            }

        }
        ;

Macro_undef
        : M_UNDEF M_IDENTIFIER
        {
            // $<strval>2 �� macro symbol table���� ���� �Ѵ�.
            gUlpMacroT.ulpMUndef( $<strval>2 );
        }
        ;

Macro_if
        : M_IF
        {
            SInt  sVal;
            SChar sTmpExpBuf[MAX_MACRO_IF_EXPR_LEN];

            switch( gUlpPPifstackMgr[gUlpPPifstackInd]->ulpPrevIfStatus() )
            {
                case PP_IF_IGNORE :
                    // �ܼ��� token�� �Ҹ��ϴ� �����̴�. PPIFparse ȣ������ �ʴ´�.
                    ulpPPEraseBN4MacroText( sTmpExpBuf, ID_TRUE );
                    gUlpPPStartCond = PP_ST_MACRO_IF_SKIP;
                    // if stack manager �� ��� ���� push
                    gUlpPPifstackMgr[gUlpPPifstackInd]->ulpIfpush( PP_IF, PP_IF_IGNORE );
                    break;

                case PP_IF_TRUE :
                    // #if expression �� �����ؿ´�.
                    ulpPPEraseBN4MacroText( sTmpExpBuf, ID_TRUE );

                    gUlpPPIFbufptr = sTmpExpBuf;
                    gUlpPPIFbuflim = sTmpExpBuf + idlOS::strlen(sTmpExpBuf);
                    if ( PPIFparse( sTmpExpBuf, &sVal ) != 0 )
                    {
                        //error ó��
                        ulpErrorMgr sErrorMgr;

                        ulpSetErrorCode( &sErrorMgr,
                                         ulpERR_ABORT_COMP_IF_Macro_Syntax_Error );
                        gUlpPPErrCode = ulpGetErrorSTATE( &sErrorMgr );
                        PPerror( ulpGetErrorMSG(&sErrorMgr) );
                    }

                    //idlOS::printf("## PPIF result value=%d\n",sVal);
                    /* macro ���ǹ��� ��� ���̸� MACRO����, �����̸� MACRO_IFSKIP ���·�
                    * ���� �ȴ�. */
                    if ( sVal != 0 )
                    {
                        // true
                        gUlpPPStartCond = PP_ST_MACRO;
                        // if stack manager �� ��� ���� push
                        gUlpPPifstackMgr[gUlpPPifstackInd]->ulpIfpush( PP_IF, PP_IF_TRUE );
                    }
                    else
                    {
                        // false
                        gUlpPPStartCond = PP_ST_MACRO_IF_SKIP;
                        // if stack manager �� ��� ���� push
                        gUlpPPifstackMgr[gUlpPPifstackInd]->ulpIfpush( PP_IF, PP_IF_FALSE );
                    }
                    break;

                case PP_IF_FALSE :
                    // �ܼ��� token�� �Ҹ��ϴ� �����̴�. PPIFparse ȣ������ �ʴ´�.
                    ulpPPEraseBN4MacroText( sTmpExpBuf, ID_TRUE );
                    gUlpPPStartCond = PP_ST_MACRO_IF_SKIP;
                    // if stack manager �� ��� ���� push
                    gUlpPPifstackMgr[gUlpPPifstackInd]->ulpIfpush( PP_IF, PP_IF_IGNORE );
                    break;

                default:
                    IDE_ASSERT(0);
            }
        }
        ;

Macro_elif
        : M_ELIF
        {
            SInt  sVal;
            SChar sTmpExpBuf[MAX_MACRO_IF_EXPR_LEN];

            // #elif ���� ���� �˻�.
            if ( gUlpPPifstackMgr[gUlpPPifstackInd]->ulpIfCheckGrammar( PP_ELIF )
                 == ID_FALSE )
            {
                //error ó��
                ulpErrorMgr sErrorMgr;

                ulpSetErrorCode( &sErrorMgr,
                                 ulpERR_ABORT_COMP_ELIF_Macro_Sequence_Error );
                gUlpPPErrCode = ulpGetErrorSTATE( &sErrorMgr );
                PPerror( ulpGetErrorMSG(&sErrorMgr) );
            }

            switch( gUlpPPifstackMgr[gUlpPPifstackInd]->ulpPrevIfStatus() )
            {
                case PP_IF_IGNORE :
                    // �ܼ��� token�� �Ҹ��ϴ� �����̴�. PPIFparse ȣ������ �ʴ´�.
                    ulpPPEraseBN4MacroText( sTmpExpBuf, ID_TRUE );
                    gUlpPPStartCond = PP_ST_MACRO_IF_SKIP;
                    // if stack manager �� ��� ���� push
                    gUlpPPifstackMgr[gUlpPPifstackInd]->ulpIfpush( PP_ELIF, PP_IF_IGNORE );
                    break;

                case PP_IF_TRUE :
                    // �ܼ��� token�� �Ҹ��ϴ� �����̴�. PPIFparse ȣ������ �ʴ´�.
                    ulpPPEraseBN4MacroText( sTmpExpBuf, ID_TRUE );
                    gUlpPPStartCond = PP_ST_MACRO_IF_SKIP;
                    // if stack manager �� ��� ���� push
                    gUlpPPifstackMgr[gUlpPPifstackInd]->ulpIfpush( PP_ELIF, PP_IF_IGNORE );
                    break;

                case PP_IF_FALSE :
                    ulpPPEraseBN4MacroText( sTmpExpBuf, ID_TRUE );

                    gUlpPPIFbufptr = sTmpExpBuf;
                    gUlpPPIFbuflim = sTmpExpBuf + idlOS::strlen(sTmpExpBuf);
                    //idlOS::printf("## start PPELIF parser text:[%s]\n",sTmpExpBuf);
                    if ( PPIFparse( sTmpExpBuf, &sVal ) != 0 )
                    {
                        //error ó��
                        ulpErrorMgr sErrorMgr;

                        ulpSetErrorCode( &sErrorMgr,
                                         ulpERR_ABORT_COMP_ELIF_Macro_Syntax_Error );
                        gUlpPPErrCode = ulpGetErrorSTATE( &sErrorMgr );
                        PPerror( ulpGetErrorMSG(&sErrorMgr) );
                    }
                    //idlOS::printf("## PPELIF result value=%d\n",sVal);

                    /* macro ���ǹ��� ��� ���̸� MACRO����, �����̸� MACRO_IFSKIP ���·�
                     * ���� �ȴ�. */
                    if ( sVal != 0 )
                    {
                        gUlpPPStartCond = PP_ST_MACRO;
                        // if stack manager �� ��� ���� push
                        gUlpPPifstackMgr[gUlpPPifstackInd]->ulpIfpush( PP_ELIF, PP_IF_TRUE );
                    }
                    else
                    {
                        gUlpPPStartCond = PP_ST_MACRO_IF_SKIP;
                        // if stack manager �� ��� ���� push
                        gUlpPPifstackMgr[gUlpPPifstackInd]->ulpIfpush( PP_ELIF, PP_IF_FALSE );
                    }
                    break;

                default:
                    IDE_ASSERT(0);
            }
        }
        ;

Macro_ifdef
        : M_IFDEF M_IDENTIFIER
        {

            switch( gUlpPPifstackMgr[gUlpPPifstackInd]->ulpPrevIfStatus() )
            {
                case PP_IF_IGNORE :
                    gUlpPPStartCond = PP_ST_MACRO_IF_SKIP;
                    // if stack manager �� ��� ���� push
                    gUlpPPifstackMgr[gUlpPPifstackInd]->ulpIfpush( PP_IFDEF, PP_IF_IGNORE );
                    break;

                case PP_IF_TRUE :
                    /* BUG-28162 : SESC_DECLARE ��Ȱ  */
                    // pare�� full �̾ƴϰ�, SESC_DECLARE�� ����, #includeó���� �ƴҶ�
                    // #ifdef SESC_DECLARE ����ó�� ó����.
                    if( (gUlpProgOption.mOptParseInfo != PARSE_FULL) &&
                        (idlOS::strcmp( $<strval>2, "SESC_DECLARE" ) == 0 ) &&
                        (gUlpPPisCInc != ID_TRUE) )
                    {
                        gUlpPPStartCond = PP_ST_MACRO;
                        gUlpPPifstackMgr[gUlpPPifstackInd]->ulpIfpush( PP_IFDEF, PP_IF_SESC_DEC );
                        WRITESTR2BUFPP((SChar *)"EXEC SQL BEGIN DECLARE SECTION;");
                    }
                    else
                    {
                        // $<strval>2 �� macro symbol table�� �����ϴ��� Ȯ���Ѵ�.
                        if ( gUlpMacroT.ulpMLookup($<strval>2) != NULL )
                        {
                            // �����Ѵ�
                            gUlpPPStartCond = PP_ST_MACRO;
                            // if stack manager �� ��� ���� push
                            gUlpPPifstackMgr[gUlpPPifstackInd]->ulpIfpush( PP_IFDEF, PP_IF_TRUE );
                        }
                        else
                        {
                            // ������Ѵ�
                            gUlpPPStartCond = PP_ST_MACRO_IF_SKIP;
                            // if stack manager �� ��� ���� push
                            gUlpPPifstackMgr[gUlpPPifstackInd]->ulpIfpush( PP_IFDEF, PP_IF_FALSE );
                        }
                    }
                    break;

                case PP_IF_FALSE :
                    gUlpPPStartCond = PP_ST_MACRO_IF_SKIP;
                    // if stack manager �� ��� ���� push
                    gUlpPPifstackMgr[gUlpPPifstackInd]->ulpIfpush( PP_IFDEF, PP_IF_IGNORE );
                    break;

                default:
                    IDE_ASSERT(0);
            }
        }
        ;

Macro_ifndef
        : M_IFNDEF M_IDENTIFIER
        {
            switch( gUlpPPifstackMgr[gUlpPPifstackInd]->ulpPrevIfStatus() )
            {
                case PP_IF_IGNORE :
                    gUlpPPStartCond = PP_ST_MACRO_IF_SKIP;
                    // if stack manager �� ��� ���� push
                    gUlpPPifstackMgr[gUlpPPifstackInd]->ulpIfpush( PP_IFNDEF, PP_IF_IGNORE );
                    break;

                case PP_IF_TRUE :
                    // $<strval>2 �� macro symbol table�� �����ϴ��� Ȯ���Ѵ�.
                    if ( gUlpMacroT.ulpMLookup($<strval>2) != NULL )
                    {
                        // �����Ѵ�
                        gUlpPPStartCond = PP_ST_MACRO_IF_SKIP;
                        // if stack manager �� ��� ���� push
                        gUlpPPifstackMgr[gUlpPPifstackInd]->ulpIfpush( PP_IFNDEF, PP_IF_FALSE );
                    }
                    else
                    {
                        // ������Ѵ�
                        gUlpPPStartCond = PP_ST_MACRO;
                        // if stack manager �� ��� ���� push
                        gUlpPPifstackMgr[gUlpPPifstackInd]->ulpIfpush( PP_IFNDEF, PP_IF_TRUE );
                    }
                    break;

                case PP_IF_FALSE :
                    gUlpPPStartCond = PP_ST_MACRO_IF_SKIP;
                    // if stack manager �� ��� ���� push
                    gUlpPPifstackMgr[gUlpPPifstackInd]->ulpIfpush( PP_IFNDEF, PP_IF_IGNORE );
                    break;

                default:
                    IDE_ASSERT(0);
            }
        }
        ;

Macro_else
        : M_ELSE
        {
            // #else ���� ���� �˻�.
            if ( gUlpPPifstackMgr[gUlpPPifstackInd]->ulpIfCheckGrammar( PP_ELSE )
                 == ID_FALSE )
            {
                // error ó��
                ulpErrorMgr sErrorMgr;

                ulpSetErrorCode( &sErrorMgr,
                                 ulpERR_ABORT_COMP_ELSE_Macro_Sequence_Error );
                gUlpPPErrCode = ulpGetErrorSTATE( &sErrorMgr );
                PPerror( ulpGetErrorMSG(&sErrorMgr) );
            }

            switch( gUlpPPifstackMgr[gUlpPPifstackInd]->ulpPrevIfStatus() )
            {
                case PP_IF_IGNORE :
                case PP_IF_TRUE :
                    gUlpPPStartCond = PP_ST_MACRO_IF_SKIP;
                    // if stack manager �� ��� ���� push
                    gUlpPPifstackMgr[gUlpPPifstackInd]->ulpIfpush( PP_ELSE, PP_IF_IGNORE );
                    break;

                case PP_IF_FALSE :
                    gUlpPPStartCond = PP_ST_MACRO;
                    // if stack manager �� ��� ���� push
                    gUlpPPifstackMgr[gUlpPPifstackInd]->ulpIfpush( PP_ELSE, PP_IF_TRUE );
                    break;

                default:
                    IDE_ASSERT(0);
            }
        }
        ;

Macro_endif
        : M_ENDIF
        {
            idBool sSescDEC;

            // #endif ���� ���� �˻�.
            if ( gUlpPPifstackMgr[gUlpPPifstackInd]->ulpIfCheckGrammar( PP_ENDIF )
                 == ID_FALSE )
            {
                // error ó��
                ulpErrorMgr sErrorMgr;

                ulpSetErrorCode( &sErrorMgr,
                                 ulpERR_ABORT_COMP_ENDIF_Macro_Sequence_Error );
                gUlpPPErrCode = ulpGetErrorSTATE( &sErrorMgr );
                PPerror( ulpGetErrorMSG(&sErrorMgr) );
            }

            /* BUG-28162 : SESC_DECLARE ��Ȱ  */
            // if stack �� ���� ���ǹ� ���� pop �Ѵ�.
            sSescDEC = gUlpPPifstackMgr[gUlpPPifstackInd]->ulpIfpop4endif();

            if( sSescDEC == ID_TRUE )
            {
                //#endif �� �Ʒ� string���� �ٲ��ش�.
                WRITESTR2BUFPP((SChar *)"EXEC SQL END DECLARE SECTION;");
            }

            /* BUG-27961 : preprocessor�� ��ø #ifó���� #endif �����ҽ� ������ ����ϴ� ����  */
            if( gUlpPPifstackMgr[gUlpPPifstackInd]->ulpIfneedSkip4Endif() == ID_TRUE )
            {
                // ��� ��������.
                gUlpPPStartCond = PP_ST_MACRO_IF_SKIP;
            }
            else
            {
                // ��� �ض�.
                gUlpPPStartCond = PP_ST_MACRO;
            }
        }
        ;


%%


int doPPparse( SChar *aFilename )
{
/***********************************************************************
 *
 * Description :
 *      The initial function of PPparser.
 *
 ***********************************************************************/
    int sRes;

    ulpPPInitialize( aFilename );

    gUlpPPifstackMgr[++gUlpPPifstackInd] = new ulpPPifstackMgr();

    sRes = PPparse();

    gUlpCodeGen.ulpGenWriteFile();

    delete gUlpPPifstackMgr[gUlpPPifstackInd];

    gUlpPPifstackInd--;

    ulpPPFinalize();

    return sRes;
}

