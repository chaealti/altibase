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
 * $Id: idnGbkext2.h 65687 2014-06-25 07:55:49Z myungsub.shin $
 **********************************************************************/

#ifndef _O_IDNGBKEXT2_H_
#define  _O_IDNGBKEXT2_H_ 1

#include <idnConv.h>

/* PROJ-2414 [��ɼ�] GBK, CP936 character set �߰� */
#define IDN_GBKEXT2_XOR_VALUE ( 36149 )

/* PROJ-2414 [��ɼ�] GBK, CP936 character set �߰� */
SInt convertMbToWc4Gbkext2( void    * aSrc,
                            SInt      aSrcRemain,
                            void    * aDest,
                            SInt      aDestRemain );

#endif /* _O_IDNGBKEXT2_H_ */
