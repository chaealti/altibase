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
 * $Id: smmShmKeyMgr.h 18299 2006-09-26 07:51:56Z xcom73 $
 **********************************************************************/

#ifndef _O_SMM_SHM_KEY_LIST_H_
#define _O_SMM_SHM_KEY_LIST_H_ (1)

#include <idl.h>
#include <idu.h>
#include <smm.h>
#include <smmDef.h>


/*
    smmShmKeyList - DB�� �����޸� Key ����Ʈ�� �����Ѵ�.

    - ���� operation
      - addKey
        - �����޸� Key�� Key List�� �߰��Ѵ�.
      - removeKey
        - Key List���� �����޸� Key�� �����Ѵ�.
*/
    

class smmShmKeyList 
{
private :
    smuList      mKeyList;      // �����޸� Key List
    iduMemPool   mListNodePool; // �����޸� Key List�� Node Mempool

public :
    smmShmKeyList();
    ~smmShmKeyList();

    // �����޸� Key List�� �ʱ�ȭ�Ѵ�.
    IDE_RC initialize();
    
    // �����޸� Key List�� �ı��Ѵ�
    IDE_RC destroy();
    
    // ��Ȱ�� Key�� ��Ȱ�� Key List�� �߰��Ѵ�
    IDE_RC addKey(key_t aKey);

    // ��Ȱ�� Key List���� ��Ȱ�� Key�� ������.
    IDE_RC removeKey(key_t * aKey);

    // �����޸� Key List���� ��� Key�� �����Ѵ�
    IDE_RC removeAll();
    
    // ��Ȱ�� Key List�� ����ִ��� üũ�Ѵ�.
    idBool isEmpty();

};


#endif /* _O_SMM_SHM_KEY_LIST_H_ */
