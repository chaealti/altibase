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
 * $Id: iduReusedMemoryHandle.h 15368 2006-03-23 01:14:43Z leekmo $
 **********************************************************************/

#ifndef _O_IDU_REUSED_MEMORY_HANDLE_H_
#define _O_IDU_REUSED_MEMORY_HANDLE_H_ 1

#include <idl.h>
#include <iduMemoryHandle.h>

/*
   Reused Memory Handle : 

   ���� ������ �޸𸮸� ������ ���밡���� ��� ����ϴ� Memory Handle
   
   �뵵 :
      <prepare�� ���� ���޹��� �޸𸮸� ��ȸ������ ����ϴ� ���>
      
      1. Logging�ÿ� ������ ������ �޸𸮷� ���.
      
         �α׸� �α����Ͽ� ����ϱ� ���� ������ �ϱ����� �ӽ� ���۷�
         ����ϴ� ���, ����� �α׸� �α����Ͽ� ����� �Ŀ���
         ����α� ���۸� �ٽ� �� ���� ���� ������
         Reused Memory Handle�� ���ϰ� �ִ� �޸𸮸� ��Ȱ���Ѵ�.

      2. Transaction Rollback�� Undo�� �α׸� ���������ϴµ� ���.
      
         Transaction Rollback�� ����� �α��� ���������� �ϴ� ���,
         Undo�ÿ� ����� �α��� �ּҴ� ���� Undo�α� �����߿���
         ������ ���� �����Ƿ�, Reused Memory Handle�� �̿��Ͽ�
         Handle�� ���ϰ� �ִ� �޸𸮸� ��Ȱ���Ѵ�.
*/
class iduReusedMemoryHandle : public iduMemoryHandle
{
public :
    iduReusedMemoryHandle();
    virtual ~iduReusedMemoryHandle();
    
    ///
    /// Reused Memory Handle�� �ʱ�ȭ �Ѵ�.
    /// @param aMemoryClient Ŭ������ ����� ��� �ε���
    IDE_RC initialize( iduMemoryClientIndex   aMemoryClient );
    
    ///
    /// Reused Memory Handle�� �ı� �Ѵ�.
    ///
    IDE_RC destroy();
    
    ///
    /// Reused Memory Handle�� aSize�̻��� �޸𸮸� �Ҵ��Ѵ�.
    /// �̹� aSize�̻� �޸𸮰� �Ҵ�Ǿ� �ִٸ� �ƹ��ϵ� ���� �ʴ´�.
    /// ��, �޸𸮴� Ŀ���⸸�ϰ� �۾����� �ʴ´�.
    /// @param aSize �޸� �Ҵ� ũ��. 2�� ���������� ���ĵ� (1000->1024)
    /// @param aPrepareMemory ���Ӱ� �Ҵ�� �޸��� �ּ� ��ȯ
    virtual IDE_RC prepareMemory( UInt    aSize,
                                  void ** aPreparedMemory);

    ///
    /// �� Memory Handle�� ���� OS�κ��� �Ҵ���� �޸��� �ѷ��� ����
    ///
    virtual ULong getSize( void );

    /* BUG-47365 �� Memory Handle�� Size�� �����Ѵ�. */
    IDE_RC tuneSize( UInt aSize );
    
private:
    /*
       Reused Memory Handle�� ���ռ� ����(ASSERT)
     */
    void assertConsistency();

    /*
       aSize�� 2�� N���� ũ��� Align up�Ѵ�.
     */
    static UInt alignUpPowerOfTwo( UInt aSize );

private:
    UInt                     mSize;        /// �Ҵ�� ũ��
    void                 * mMemory;        /// �Ҵ�� �޸�
    iduMemoryClientIndex   mMemoryClient;  /// �޸� �Ҵ� Client �νĹ�ȣ
};

    


#endif /*  _O_IDU_REUSED_MEMORY_HANDLE_H_ */
