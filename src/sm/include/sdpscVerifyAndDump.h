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
 *
 * $Id: sdpscVerifyAndDump.h 82075 2018-01-17 06:39:52Z jina.kim $
 *
 * �� ������ Circular-List Managed Segment�� �ڷᱸ�� Ȯ�� �� ��°� 
 * ���õ� STATIC �������̽��� �����Ѵ�.
 *
 ***********************************************************************/

# ifndef _O_SDPSC_VERIFY_AND_DUMP_PAGE_H_
# define _O_SDPSC_VERIFY_AND_DUMP_H_ 1

# include <sdpDef.h>

class sdpscVerifyAndDump
{
public:

    /* [ INTERFACE ] Segment�� �ڷᱸ���� ǥ��������� ����Ѵ� */
    static void dump( scSpaceID    aSpaceID,
                      void        */* aSegDesc */,
                      idBool       /* aDisplayAll */);

    /*  [ INTERFACE ] Segment �ڷᱸ���� Ȯ���Ѵ� */
    static IDE_RC verify(idvSQL     * aStatistics,
                         scSpaceID    aSpaceID,
                         void       * /* aSegDesc */,
                         UInt         /* aFlag */,
                         idBool       /* aAllUsed */,
                         scPageID     /* aUsedLimit */);

};

#endif // _O_SDPSC_VERIFY_AND_DUMP_H_
