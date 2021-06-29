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
 * $Id: smcObject.h 88191 2020-07-27 03:08:54Z mason.lee $
 *
 * �� ������ smcObject�� ���� ��� �����̴�.
 * 
 **********************************************************************/

#ifndef _O_SMC_OBJECT_H_
#define _O_SMC_OBJECT_H_ 1


#include <smDef.h>
#include <smcDef.h>

class smcObject
{
    
public:
    
//object
    
    static IDE_RC createObject( void*              aTrans,
                                const void*        aInfo,
                                UInt               aInfoSize,
                                const void*        aTempInfo,
                                smcObjectType      aObjectType,
                                smcTableHeader**   aTable );

    static void getObjectInfo( smcTableHeader*  aTable,
                               void**           aObjectInfo);
    
};

#endif /* _O_SMC_OBJECT_H_ */

