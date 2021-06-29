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
 * $Id: sdpscVerifyAndDump.cpp 82075 2018-01-17 06:39:52Z jina.kim $
 *
 * �� ������ Circular-List Managed Segment�� �ڷᱸ�� Ȯ�� �� ��°� ���õ�
 * STATIC  �������̽��� �����Ѵ�.
 *
 ***********************************************************************/

# include <sdpscVerifyAndDump.h>

/*
 * [ INTERFACE ] Circular-List Managed Segment�� �ڷᱸ����
 * ǥ��������� ����Ѵ�
 */
void sdpscVerifyAndDump::dump( scSpaceID    /*aSpaceID*/,
                               void        */*aSegDesc*/,
                               idBool       /*aDisplayAll*/)
{
    return;
}

/*
 * [ INTERFACE ] Segment �� ��� �ڷᱸ���� Ȯ���Ѵ�.
 */
IDE_RC sdpscVerifyAndDump::verify( idvSQL     */*aStatistics*/,
                                   scSpaceID   /*aSpaceID*/,
                                   void       */*aSegDesc*/,
                                   UInt        /*aFlag*/,
                                   idBool      /*aAllUsed*/,
                                   scPageID    /*aUsedLimit*/)
{
    return IDE_SUCCESS;
}
