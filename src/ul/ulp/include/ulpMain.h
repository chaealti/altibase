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

#ifndef _ULP_MAIN_H_
#define _ULP_MAIN_H_ 1


#include <idl.h>
#include <ide.h>
#include <iduMemory.h>
#include <ulpErrorMgr.h>
#include <ulpProgOption.h>
#include <ulpGenCode.h>
#include <ulpMacroTable.h>
#include <ulpScopeTable.h>
#include <ulpStructTable.h>
#include <ulpMacro.h>



/****************************************
 * Global ���� ����
 ****************************************/

/* command-line option������ ���� ��ü. global�ϰ� �����ȴ�. */
ulpProgOption gUlpProgOption;
/* lexer�� parser���� global�ϰ� ���Ǵ� code generator ��ü. */
ulpCodeGen    gUlpCodeGen;
/* lexer token�� �����ϱ� ���� memory manager*/
iduMemory       *gUlpMem;
/* Symbol tables */
// Macro table
ulpMacroTable  gUlpMacroT;
// Scope table
ulpScopeTable  gUlpScopeT;
// Struct Table
ulpStructTable gUlpStructT;

/* ���� �߻��� �����ڵ� �κп� ���� file���� ���θ� ����. */
SInt          gUlpErrDelFile = ERR_DEL_FILE_NONE;



/* functions */
void ulpFinalizeError(void);

#endif
