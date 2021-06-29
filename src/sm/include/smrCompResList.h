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
 * $Id: smrCompResList.h 15368 2006-03-23 01:14:43Z kmkim $
 **********************************************************************/

#ifndef _O_SMR_COMP_RES_LIST_H_
#define _O_SMR_COMP_RES_LIST_H_ 1

#include <idl.h>
#include <iduMemPool.h>
#include <iduMutex.h>
#include <smDef.h>
#include <smrDef.h>
#include <smuList.h>
#include <smuSynchroList.h>

/*
   Compression Resource List

   ���� ���ҽ� Ǯ���� ����� �������� ������
   ���ü� ����� ��ũ�� ����Ʈ

   smuSynchroList�� ���� Ŭ�����̱� ������,
   ���� ���ҽ� ���� �ڵ带 ���� �� ����.

   ������ Ŭ������ ���� Ŭ���� �Ͽ�
   ���� ���ҽ� ����Ʈ�� ����Ѵ�.
*/

class smrCompResList : public smuSynchroList
{
public :
    /*  Ư�� �ð����� ������ ���� ���� ���ҽ���
        Linked List�� Tail�κ��� ���� */
    IDE_RC removeGarbageFromTail(
               UInt          aMinimumResourceCount,
               ULong         aGarbageCollectionMicro,
               smrCompRes ** aCompRes );
    
};

#endif /* _O_SMR_COMP_RES_LIST_H_ */
