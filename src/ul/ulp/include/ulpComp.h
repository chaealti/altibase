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

#ifndef _ULP_COMP_H_
#define _ULP_COMP_H_ 1


#include <idl.h>
#include <idn.h>
#include <ide.h>
#include <iduMemory.h>
#include <ulpProgOption.h>
#include <ulpGenCode.h>
#include <ulpMacroTable.h>
#include <ulpScopeTable.h>
#include <ulpSymTable.h>
#include <ulpIfStackMgr.h>
#include <ulpErrorMgr.h>
#include <ulpTypes.h>
#include <ulpMacro.h>
#include <ulpLibMacro.h>




//========= For Parser ==========//

// parsing function at ulpCompy.y
int doCOMPparse( SChar *aFilename );

// array binding�� �ؾ�����(isarr�� true�� set����) ���θ� üũ�ϱ� ���� �Լ�.
idBool ulpCOMPCheckArray( ulpSymTElement *aSymNode );

// for validating host values
void ulpValidateHostValue( void         *yyvsp,
                           ulpHVarType   aInOutType,
                           ulpHVFileType aFileType,
                           idBool        aTransformQuery,
                           SInt          aNumofTokens,
                           SInt          aHostValIndex,
                           SInt          aRemoveTokIndexs );

void ulpValidateHostValueWithDiagType(
                           void         *yyvsp,
                           ulpHVarType   aInOutType,
                           ulpHVFileType aFileType,
                           idBool        aTransformQuery,
                           SInt          aNumofTokens,
                           SInt          aHostValIndex,
                           SInt          aRemoveTokIndexs,
                           ulpHostDiagType aDiagType );

IDE_RC ulpValidateFORStructArray(ulpSymTElement *aElement);

IDE_RC ulpValidateFORGetDiagnostics(ulpSymTElement *aElement);

/*
 * parsing�߿� ó���ϱ� �Ǵ� host �����鿡 ���� ������
 * ��� �����صα� ���� class.
 */
class ulpParseInfo
{
    public:
        /* attributes */

        // id �� ������ �ų� ���ų� ����.
        idBool          mSaveId;
        // function ���� ����κ����� ����.
        idBool          mFuncDecl;
        // array depth
        SShort          mArrDepth;
        // array ����κ����� ����.
        idBool          mArrExpr;
        // constant_expr
        SChar           mConstantExprStr[MAX_EXPR_LEN];
        // structure node pointer ����.
        ulpStructTNode *mStructPtr;
        // typedef ���� ����.
        ulpSymTElement *mHostValInfo4Typedef;
        
        /* BUG-28118 : system ������ϵ鵵 �Ľ̵ž���.                       *
         * 6th. problem : Nested structure ������ scope�� �߸� ����ϴ� ���� */
        // host value, struct type ���� ���� �迭.
        // ��ø ����ü�ϰ�츦 ó���ϱ� ���� �迭�� �����.
        // ex>
        // int i;             <= mHostValInfo[0] �� ���� ���� ����.
        // struct A           <= mHostValInfo[0]�� ����ü ���� ����, mHostValInfo[1] ���� �Ҵ��� �ʱ�ȭ.
        // {
        //    int a;          <= mHostValInfo[1] �� ���� ���� ����.
        //    struct B        <= mHostValInfo[1]�� ����ü ���� ����, mHostValInfo[2] ���� �Ҵ��� �ʱ�ȭ.
        //    {
        //       int b;       <= mHostValInfo[2] �� ���� ���� ����.
        //    } sB;           <= mHostValInfo[2] ����.
        // } ;                <= mHostValInfo[1] ����.
        ulpSymTElement *mHostValInfo[MAX_NESTED_STRUCT_DEPTH];
        // structure ����κ� depth ����.
        // mHostValInfo�� index�� ����.
        // ex> <depth 0>..struct {..<depth 1>..struct {..<depth 2>..}..}
        SShort          mStructDeclDepth;

        // varchar ���� list
        iduList mVarcharVarList;
        // varchar �������������� ����.
        idBool  mVarcharDecl;

        /* BUG-35518 Shared pointer should be supported in APRE */
        idBool  mIsSharedPtr;
        iduList mSharedPtrVarList;

        // typedef ó���� skip�Ѵ�.
        /* BUG-27875 : ����ü���� typedef type�νĸ���. */
        // �Ʒ� ������ ó���ϱ����� mSkipTypedef ���� �߰���.
        // typedef struct Struct1 Struct1;
        // struct Struct1       <- mSkipTypedef = ID_TRUE  :
        //                          Struct1�� ��� ������ typedef�Ǿ� ������ �������� C_TYPE_NAME�̾ƴ�
        // {                        C_IDENTIFIER�� �νĵǾ�� �Ѵ�.
        //    ...               <- mSkipTypedef = ID_FALSE :
        //    ...                   �ʵ忡 typedef �̸��� ���� C_TYPE_NAME���� �νĵž��Ѵ�.
        // };
        idBool  mSkipTypedef;

        /* BUG-31648 : segv on windows */
        ulpErrorMgr mErrorMgr;

        /* functions */

        // constructor
        ulpParseInfo();

        // finalize
        void ulpFinalize(void);

        // host ���� ���� �ʱ�ȭ �Լ�.
        void ulpInitHostInfo(void);

        // typedef ���� ó���� ���� ulpSymTElement copy �Լ�.
        void ulpCopyHostInfo4Typedef( ulpSymTElement *aD, ulpSymTElement *aS );

        /* print host variable infos. for debugging */
        void ulpPrintHostInfo(void);
};


//========= For Lexer ==========//


/*
 * COMPLexer���� ���Ǵ� internal function��.
 */

IDE_RC ulpCOMPInitialize( SChar *aFileName );

void   ulpCOMPFinalize();

/* ���� buffer ���� ����ü ����. (YY_CURRENT_BUFFER) */
void   ulpCOMPSaveBufferState( void );

/* C comment ó�� �Լ� */
IDE_RC ulpCOMPCommentC( idBool aSkip );

/* C++ comment ó�� �Լ� */
void   ulpCOMPCommentCplus( void );

/* lexer�� start condition�� �������ִ� �Լ�. */
void   ulpCOMPRestoreCond(void);

/* �Ľ� �� �ʿ� ���� macro ������ skip���ִ� �Լ�. */
IDE_RC ulpCOMPSkipMacro(void);

/* macro �������� '//n'���ڸ� ���� ���ִ� �Լ�. */
void   ulpCOMPEraseBN4MacroText( SChar *aTmpDefinedStr, idBool aIsIf );

/* WHENEVER ���� DO function_name �� �Լ��̸��� �����ϴ� �Լ�. */
void   ulpCOMPWheneverFunc( SChar *aTmpStr );

/* EXEC SQL INCLUDE �� ������� �̸��� �����ϱ����� �Լ�. */
void   ulpCOMPSetHeaderName( void );

/* BUG-28061 : preprocessing����ġ�� marco table�� �ʱ�ȭ�ϰ�, *
 *             ulpComp ���� �籸���Ѵ�.                       */
/* yyinput ��� */
SChar  ulpCOMPYYinput(void);
/* unput ��� */
void   ulpCOMPYYunput( SChar aCh );
/* #if�ȿ� comment�ð�� skip */
void   ulpCOMPCommentCplus4IF();
IDE_RC ulpCOMPCommentC4IF();

/* BUG-28250 : :������ �����̸��̿��µ� ������ �־�� �ȵ˴ϴ�. */
// : <ȣ��Ʈ�����̸�> ���� :�� �̸������� ������ �������ش�.
void   ulpCOMPEraseHostValueSpaces( SChar *aString );

/* BUG-28118 : system ������ϵ鵵 �Ľ̵ž���.     *
 * 3th. problem : ��ũ�� �Լ��� Ȯ����� �ʴ� ����. */
// Macro �Լ��� �ð�� id ���� (...) ��ū�� �Ҹ����ֱ� ���� �Լ�.
void   ulpCOMPSkipMacroFunc( void );

#endif
