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

#ifndef _O_SMM_SHM_KEY_MGR_H_
#define _O_SMM_SHM_KEY_MGR_H_ (1)

#include <idl.h>
#include <idu.h>
#include <smm.h>
#include <smmDef.h>
#include <smmShmKeyList.h>

/*
    smmShmKeyMgr - DB�� �����޸� Key �ĺ��� �����Ѵ�.

    [PROJ-1548] User Memory Tablespace
    
    - ���� �޸� Ű �ĺ���?
      - �����޸� Key�� ��� �������� ���θ�
        ���� �����޸� ������  �����غ��� �� �� �ִ�.
      - �ش� Key�� �̿��Ͽ� �� �����޸� ������ ������ �� ���� ����
        �ְ� �̹� �ش� Key�� �����޸� ������ �����Ǿ� �־
        ����� �Ұ����� Key�� ���� �ִ�.

    - ��밡���� �����޸�Ű�� ã�� �˰��� 
      - SHM_DB_KEY ������Ƽ�� �������� 1�� �����ذ���
        ��밡���� Key�� ã�� �������
        
    - Tablespace�� �����޸� Ű ����
      - Tablespace���� �����޸� Key������ ���� �и��Ǿ� ���� �ʴ�.
      - ��, SHM_DB_KEY�κ��� �����Ͽ� �ϳ��� �����ذ��� ����
        ���̺� �����̽��� �����޸� Ű�� ������ ��������.

    - ���ü� ����
      - ���� Tablespace�� �����ϴ� ���� Thread�� ���ÿ�
        �����޸� Key�ĺ��� �������� �� �� �ִ�.
      - Mutex�� �ξ� �ѹ��� �ϳ��� Thread���� ���� �޸� Key �ĺ���
        ������ �� �ֵ��� �����Ѵ�.

    - �����޸� Key�� ��Ȱ��
      - �����ϸ� ������ �ٸ� Tablespace���� ���Ǿ��� Key��
        ��Ȱ���Ͽ� ����Ѵ�.
        
      -  Tablespace drop�ÿ� Tablespace���� ������̴� �����޸� Key��
         �ٸ� Tablespace���� ��Ȱ���� �� �־�� �Ѵ�.

         - ����
           Tablespace �� drop�ɶ� �ش� Tablespace���� ������̴�
           �����޸� Key�� ��Ȱ������ ���� ���
           Tablespace create/drop�� �ݺ��ϸ� �����޸� Key��
           ���Ǵ� ������ ����

         - �ذ�å
           Tablespace�� drop�ɶ� �ش� Tablespace���� ������̴�
           �����޸� Key�� ��Ȱ���Ѵ�.
       
 */

class smmShmKeyMgr
{
private :
    static iduMutex      mMutex;       // mSeekKey�� ��ȣ�ϴ� Mutex
    static smmShmKeyList mFreeKeyList; // ��Ȱ�� �����޸� Key List
    // ��Ȱ�� Key�� ������ 1�� �����ذ��� �����޸� �ĺ�Ű ã�� ���� Ű
    static key_t         mSeekKey;     
    
    
    smmShmKeyMgr();
    ~smmShmKeyMgr();

public :

    // ���� ����� �����޸� Key �ĺ��� ã�� �����Ѵ�.
    static IDE_RC getShmKeyCandidate(key_t * aShmKeyCandidate) ;


    // ���� ������� Key�� �˷��ش�.
    static IDE_RC notifyUsedKey(key_t aUsedKey);

    // ���̻� ������ �ʴ� Key�� �˷��ش�.
    static IDE_RC notifyUnusedKey(key_t aUnusedKey);
    
    // shmShmKeyMgr�� static �ʱ�ȭ ���� 
    static IDE_RC initializeStatic();
    

    // shmShmKeyMgr�� static �ı� ���� 
    static IDE_RC destroyStatic();

    static IDE_RC lock() { return mMutex.lock( NULL ); }
    static IDE_RC unlock() { return mMutex.unlock(); }
    
};


#endif // _O_SMM_SHM_KEY_MGR_H_
