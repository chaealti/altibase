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
 
/***********************************************************************
 * $Id: iduName.h 20179 2007-01-30 00:51:39Z leekmo $
 **********************************************************************/

#ifndef _O_IDU_NAME_H_
#define _O_IDU_NAME_H_ 1

#include <idl.h>

/***********************************************************************
 *
 * Description : To Fix BUG-17430
 *
 *    Object Name �� ���� String�� ���� ������ ���
 *
 * ��� ���� :
 *
 *    ##################################
 *    # Name String�� ���� ��� ����
 *    ##################################
 *
 *    ## Name String �� ����
 *
 *       - Oridinary  Name : ���ڿ� �״�� ���
 *       - Identifier Name : �빮�ڷ� ����Ǿ� ó���Ǵ� �̸�
 *
 *    ## Object Name�� �ؼ�
 *
 *       - Quoted Name : Ordinary Name���� �ؼ�
 *       - Non-Quoted Name : �ɼǿ� ���� �޸� �ؼ�
 *          ; SQL_ATTR_META_ID ���� : Ordinary Name���� �ؼ�
 *          ; SQL_ATTR_META_ID �̼��� : Identifier Name���� �ؼ�
 *          ; ���� ���������� Identifier Name���� �ؼ��� (BUG-17771)
 *
 * Implementation :
 *
 **********************************************************************/

// CLI ���� �Լ� ������ ����� �̸����� ����
void iduNameMakeNameInFunc( SChar * aDstName,
                            SChar * aSrcName,
                            SInt    aSrcLen );

// SQL ���� ������ ����� �̸����� ����
void iduNameMakeNameInSQL( SChar * aDstName,
                           SChar * aSrcName,
                           SInt    aSrcLen );

// Quoted Name�� ����
void iduNameMakeQuotedName( SChar * aDstName,
                            SChar * aSrcName,
                            SInt    aSrcLen );

// Quoted Name���� �Ǵ�
idBool iduNameIsQuotedName( SChar * aSrcName, SInt aSrcLen );

#endif // _O_IDU_NAME_H_

