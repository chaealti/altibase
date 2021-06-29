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
 * $Id: smiFixedTableDef.h 82075 2018-01-17 06:39:52Z jina.kim $
 **********************************************************************/

#ifndef _O_SMI_FIXED_TABLE_DEF_H_
# define _O_SMI_FIXED_TABLE_DEF_H_ 1

#include <idl.h>
#include <smDef.h>
#include <smcDef.h>
#include <smpDef.h>
#include <iduFixedTable.h>

// �� ��ü�� �ؽ��� ����Ǿ�, �̸��� ���� ������.
struct smcTableHeader;

/*
 * �Ʒ��� smiFixedTableHeader type�� �ʿ��� ������
 * Fixed Table���� ���� ���̺� �����
 * �����ϰ� �޸� �������� �Ҵ�� ���Ŀ�
 * �ΰ��� ������ �� �����ؾ� �ϱ� ������ �Ʒ��� ����
 * �ٽ� �����Ͽ���.
 * �̷��� ����� ���̺� ����� QP�� ���� �Ϲ�����
 * ���̺� ��� ó�� ��������, SM������
 * ���� �ΰ����ڸ� �߰������� ����ϰ� �ȴ�.
 */
typedef struct smiFixedTableHeader
{
    smpSlotHeader      mSlotHeader;
    smcTableHeader     mHeader;
    iduFixedTableDesc *mDesc;
    void              *mNullRow;
} smiFixedTableHeader;

typedef struct smiFixedTableNode
{
    SChar               *mName;   //  same with  mDesc->mName
    smiFixedTableHeader *mHeader; //  Table ���
} smiFixedTableNode;

typedef struct smiFixedTableRecord
{
    smiFixedTableRecord *mNext;
    
}smiFixedTableRecord;



#endif /* _O_SMI_FIXED_TABLE_DEF_H_ */
