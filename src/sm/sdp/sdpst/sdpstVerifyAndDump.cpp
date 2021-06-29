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
 * $Id: sdpstVerifyAndDump.cpp 27229 2008-07-23 17:37:19Z newdaily $
 *
 * �� ������ Treelist Managed Segment�� �ڷᱸ�� Ȯ�� �� ��°� ���õ�
 * STATIC  �������̽��� �����Ѵ�.
 *
 ***********************************************************************/

# include <sdpstVerifyAndDump.h>

/*
 * [ INTERFACE ] Treelist Managed Segment�� ��� Bitmap ��������
 * ǥ��������� ����Ѵ�
 */
void sdpstVerifyAndDump::dump( scSpaceID    /*aSpaceID*/,
                               void        */*aSegDesc*/,
                               idBool       /*aDisplayAll*/)
{
    return;
}

/*
 * [ INTERFACE ] Segment �� ��� �ڷᱸ���� Ȯ���Ѵ�.
 */
IDE_RC sdpstVerifyAndDump::verify( idvSQL     */*aStatistics*/,
                                   scSpaceID   /*aSpaceID*/,
                                   void       */*aSegDesc*/,
                                   UInt        /*aFlag*/,
                                   idBool      /*aAllUsed*/,
                                   scPageID    /*aUsedLimit*/ )
{
    return IDE_SUCCESS;
}
