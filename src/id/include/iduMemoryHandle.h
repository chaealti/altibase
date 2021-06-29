/** 
 *  Copyright (c) 1999~2017, Altibase Corp. and/or its affiliates. All rights reserved.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License, version 3,
 *  as published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
 
/***********************************************************************
 * $Id: iduMemoryHandle.h 15368 2006-03-23 01:14:43Z leekmo $
 **********************************************************************/

#ifndef _O_IDU_MEMORY_HANDLE_H_
#define _O_IDU_MEMORY_HANDLE_H_ 1

#include <idl.h>

/*
   iduMemoryHandle : 
     ���ӵ� �޸� ������ alloc,realloc�ϴ� ǥ�� interface�� �����Ѵ�.
     
   �뵵 :
     �޸��� �Ҵ�� ������ ����ϴ� ����
     �޸𸮸� ���ο� ũ��� ���Ҵ��� �ϴ� ����� ���� �ٸ� ���
     Memory Handle�� �̿��Ͽ� �޸𸮸� �Ҵ�/���Ҵ�/�����Ѵ�.

   ��뿹:
     �α� ���������� ��� ���������� �ϴ� Thread�� �α��� ����
     ���� ���۸� �Ҵ��ϰ� �����Ѵ�.
     (iduMemoryHandle�� �����ϴ� Concrete class�� initialize, destroy�� ȣ��)

     �ݸ�, ������ ������ Ǫ�� �۾��� �α� ��⿡�� �����ϱ� ������,
     �α� ��⿡���� ���� ���� ������ ũ�⸦ ������� ������
     �����ؾ� �Ѵ�.
     (iduMemoryHandle�� �����ϴ� Concrete class�� prepare�� ȣ���Ͽ�
      �ʿ��� �޸� ũ�⸦ �˷���.)
 */
class iduMemoryHandle
{
public :
    inline iduMemoryHandle()
    {
    }
    
    inline virtual ~iduMemoryHandle()
    {
    }
    
    
    /*
       Memory Handle�κ��� aSize�̻��� �޸𸮸� �Ҵ��Ѵ�.

       �׸��� �Ҵ�� �޸� �ּҸ� ��ȯ�Ѵ�
       
     */
    virtual IDE_RC prepareMemory( UInt    aSize,
                                  void ** aPreparedMemory) = 0;

    /*
      �� Memory Handle�� ���� �Ҵ���� �޸��� ũ�⸦ ����
    */
    virtual ULong getSize( void ) = 0;

};

    


#endif /*  _O_IDU_RESIZABLE_MEMORY_H_ */
