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
 * $Id: smrCompResPool.h 15368 2006-03-23 01:14:43Z kmkim $
 **********************************************************************/

#ifndef _O_SMR_COMP_RES_POOL_H_
#define _O_SMR_COMP_RES_POOL_H_ 1

#include <idl.h>
#include <smDef.h>
#include <smrDef.h>
#include <smrCompResList.h>
#include <iduMemoryHandle.h>

/*
   Log Compression�� ���� Resource Pool

   �� Ŭ������ �α� ������ ���� ���ҽ��� �����Ѵ�.

   �α� ������ ���� ���ҽ��� ������ ����
     - ����� �αװ� ��ϵ� �޸�
     - �α� ���࿡ �ӽ������� ����� �۾� �޸�

���Թ�� :
   �α� ������ Log�� ���� Mutex�� ���� ���� ä�� �̷������ ������,
   ���� �α����Ͽ� ����� ���� �ٸ� �α׸�
   ���ÿ� ���� Thread�� �Բ� ������ �� �ִ�.
   
   �ս��� ���� ������δ�
   Transaction�� Log ������ ���� Resource�� �������� �ϴ� ���� ������
   �� �� �ִ�.

   ������, Active Trasaction�� � ���� ��Ȳ����
   Active���� ���� Transaction���� Log ������ ���� Resource��
   ������ �Ǿ� ���ʿ��� �޸� ���� �߻��Ѵ�.

�� ����� ���� :   
   Log�� ����Ҷ��� �ӽ������� ����ϴ�
   �α� ������ ���� ���ҽ�(�޸�)��
   ���� Transaction�� ��Ȱ���� �� �ֵ��� Pool�� �־� �����Ѵ�.
*/

class smrCompResPool
{
public :
    /* ��ü �ʱ�ȭ */
    IDE_RC initialize( SChar * aPoolName,
                       UInt    aInitialResourceCount,    
                       UInt    aMinimumResourceCount,
                       UInt    aGarbageCollectionSecond );
    
    /* ��ü �ı� */
    IDE_RC destroy();

    /* �α� ������ ���� Resource �� ��´� */
    IDE_RC allocCompRes( smrCompRes ** aCompRes );

    /* �α� ������ ���� Resource �� �ݳ� */
    IDE_RC freeCompRes( smrCompRes * aCompRes );

    /* BUG-47365 �α� ������ ���� Resource�� ũ�⸦ ������ */
    IDE_RC tuneCompRes( smrCompRes * aCompRes,
                        UInt         aSize );

private :

    /* �α� ������ ���� Resource �� �����Ѵ� */
    IDE_RC createCompRes( smrCompRes ** aCompRes );

    /* �α� ������ ���� Resource �� �ı��Ѵ� */
    IDE_RC destroyCompRes( smrCompRes * aCompRes );


    /* ���ҽ� Ǯ �������� ���� ������ ���ҽ� �ϳ��� ����
       Garbage Collection�� �ǽ��Ѵ�. */
    IDE_RC garbageCollectOldestRes();
    
    /* �� Pool�� ������ ��� ���� Resource �� �ı��Ѵ� */
    IDE_RC destroyAllCompRes();

    /* ���ҽ��� �� ����ũ�� �� �̻� ������ ���� ���
       Garbage Collection����? */
    UInt           mGarbageCollectionMicro; 

    /* ���ҽ� Ǯ �ȿ� ������ �ּ����� ���ҽ� ����
       Garbage Collection�Ҹ�ŭ �������� ������ �ʴ���
       �̸�ŭ�� ���ҽ��� Ǯ�� �����Ѵ�. 
     */
    UInt           mMinimumResourceCount;
    
    smrCompResList mCompResList;    /* ���� List */
    iduMemPool     mCompResMemPool; /* ���� ���ҽ��� ���� �޸� Ǯ */
};

#endif /* _O_SMR_COMP_RES_POOL_H_ */
