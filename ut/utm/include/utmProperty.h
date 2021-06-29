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
 * $Id: utmProperty.h 80542 2017-07-19 08:01:20Z daramix $
 **********************************************************************/

/*****************************************************************************
 *   NAME
 *     utmProperty.h - ���θ�� ���ȭ��
 *
 *   DESCRIPTION
 *
 *   PUBLIC FUNCTION(S)
 *
 *   PRIVATE FUNCTION(S)
 *
 *   NOTES
 *
 *   MODIFIED   (MM/DD/YY)
 ****************************************************************************/

#ifndef _O_UTM_PROPERTY_H_
#define _O_UTM_PROPERTY_H_ 1

#include <utmPropertyMgr.h>

#define IDUP_MAX_NAME_LEN  1024
#define IDUP_PINGPONG_COUNT  2 /* BUGBUG : SMM_PINGPONG_COUNT�� ���� */

class utmProperty
{
    static utmPropertyMgr * iduPM_;

public:
    static SChar          * home_dir_;

public:
    static SChar * getEnv(SChar *);
};

#endif  /* _O_UTM_PROPERTY_H_ */
