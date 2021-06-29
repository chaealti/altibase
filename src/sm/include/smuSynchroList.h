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
 * $Id: smuSynchroList.h 15368 2006-03-23 01:14:43Z kmkim $
 **********************************************************************/

#ifndef _O_SMU_SYNCHRO_LIST_H_
#define _O_SMU_SYNCHRO_LIST_H_ 1

#include <idl.h>
#include <iduMemPool.h>
#include <iduMutex.h>
#include <smDef.h>
#include <smuList.h>

/*
   Synchronized List

   ���ü� ����� ��ũ�� ����Ʈ�� ����
*/

class smuSynchroList
{
public :
    /* ��ü �ʱ�ȭ */
    IDE_RC initialize( SChar * aListName,
                       iduMemoryClientIndex aMemoryIndex);
    
    /* ��ü �ı� */
    IDE_RC destroy();

    /* Linked List�� Head�� Data�� Add */
    IDE_RC addToHead( void * aData );

    /* Linked List�� Tail�� Data�� Add */
    IDE_RC addToTail( void * aData );
    
    /* Linked List�� Head�κ��� Data�� ���� */
    IDE_RC removeFromHead( void ** aData );

    /*  Linked List�� Tail�κ��� Data�� ���� */
    IDE_RC removeFromTail( void ** aData );

    /* Linked List�� Tail�� ����, ����Ʈ���� ���������� ���� */
    IDE_RC peekTail( void ** aData );

    /* Linked List���� Element������ �����Ѵ� */
    IDE_RC getElementCount( UInt * aElemCount );
    

protected :
    UInt       mElemCount;    /* Linked List���� Element ���� */
    smuList    mList;         /* Linked List �� Base Node */
    iduMemPool mListNodePool; /* Linked List Node�� Memory Pool */
    iduMutex   mListMutex;    /* ���ü� ��� ���� Mutex */
};

#endif /* _O_SMU_SYNCHRO_LIST_H_ */
