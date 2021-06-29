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
 * $Id: scpDef.h $
 *
 **********************************************************************/

#ifndef _O_SCP_DEF_H_
#define _O_SCP_DEF_H_ 1

#include <smiDef.h>
#include <scpModule.h>

// Export �Ǵ� Import ���� �� ���Ǵ� Handle.
// scpManager�� ���� �����ȴ�.
typedef struct scpHandle
{
    UChar                mJobName[ SMI_DATAPORT_JOBNAME_SIZE ];
    UInt                 mType;
    smiDataPortHeader  * mHeader;       // ���� ���
    scpModule          * mModule;       // ���� scpModule
    void               * mSelf;         // ������ ���� �ڽ��� Node Ptr�� ����
} scpHandle;

 
#endif /*_O_SCP_DEF_H_ */
