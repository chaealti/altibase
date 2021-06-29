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
 * $id$
 **********************************************************************/

#ifndef _O_DKD_DATA_BUFFER_MGR_H_
#define _O_DKD_DATA_BUFFER_MGR_H_ 1


#include <dkd.h>

/************************************************************************
 * DK DATA BUFFER MANAGER (���ĺ����� ���� comment)
 *
 *  1. Concept
 *
 *   ���⼭ data buffer �� DK ����� ���� remote query �� ����� ���,
 *  AltiLinker  ���μ����� ���� ���� �����κ��� ���۹��� ���ݰ�ü��
 *  �����͸� QP �� fetch �� �Ϸ�Ǳ� ������ �����ϱ� ���� �Ͻ������� 
 *  �����Ǵ� record buffer ���� ������ �ǹ��Ѵ�. 
 *   �׷��� ��� �޸𸮿����� ������� buffer �� �Ҵ��ϵ��� �� ���� ����
 *  ���ѵ� ũ�� ( DKU_DBLINK_DATA_BUFFER_SIZE ) �� �ý��� ������Ƽ�� 
 *  �Է¹޾� �ش� ũ�⸸ŭ�� ����� �� �ֵ��� �Ѵ�.
 *
 *  2. Implementation
 *  
 *   ������Ƽ�κ��� �Է¹��� �ִ� ũ�� �ȿ��� �����Ҵ��ϴ� ������� 
 *  �����Ѵ�. iduMemPool �� �̿��Ͽ� block * count ��ŭ �Ҵ��ϴ� ����� 
 *  ����ϴ� ���, data buffer allocation mechanism ����� alloc list �� 
 *  free list �� ���� �����Ͽ� �����ؾ� �ϴ� ���ŷο��� �����Ƿ� �ϴ��� 
 *  record buffer �� �Ҵ���� ������ data buffer �ִ� ũ�⸦ ���Ͽ� 
 *  �� �ȿ����� �Ҵ���� �� �ֵ��� �ϴ� ������� �Ѵ�. ���� iduMemMgr
 *  �� �̿��Ͽ� �ʿ��� ������ record buffer �� �Ҵ� �� ������ �ϴ� ������ 
 *  �����ϸ� dkdDataBufferMgr �ʱ�ȭ �� ������ memory pool �� �Ҵ���� 
 *  �ʿ䰡 ���� �׷��Ƿ� mutex ���� ���� memory �� ���������� ���� �ʿ䵵 
 *  ����. 
 *   ���� record buffer �� �Ҵ� �� ������ ���ɻ� overhead �� �ȴٸ� ���� 
 *  iduMemPool �� �̿��ϴ� ����� �����غ����� �Ѵ�. 
 *
 *   �׷��� ���� ������ ������ �����Ͽ� ������ ���� Ʋ�ȿ��� �����Ѵ�.
 *
 *      data_buffer_size >= SUM( allocated_record_buffer_size )
 *      record_buffer_size = block_size * count
 *
 ***********************************************************************/
class dkdDataBufferMgr
{
private:
    static UInt     mBufferBlockSize;        /* ���ۺ���� ũ�� */
    static UInt     mMaxBufferBlockCnt;      /* �ִ� �Ҵ簡���� ���ۺ�ϰ��� */
    static UInt     mUsedBufferBlockCnt;     /* ����� ���ۺ�ϰ��� */
    static UInt     mRecordBufferAllocRatio; /* ���ڵ���� �Ҵ���� *//* BUG-36895: SDouble -> UInt */

    static iduMemAllocator * mAllocator;       /* TLSF memory allocator BUG-37215 */
    static iduMutex mDbmMutex;

public:
    /* Initialize / Finalize */
    static IDE_RC       initializeStatic(); /* allocate DK buffer blocks */
    static IDE_RC       finalizeStatic();   /* free DK buffer blocks */

    /* �Է¹��� ������ŭ buffer block �� �Ҵ� �� ��ȯ */
    static IDE_RC       allocRecordBuffer( UInt *aSize, void  **aRecBuf );
    static IDE_RC       deallocRecordBuffer( void *aRecBuf, UInt aBlockCnt );

    /* TLSF memory allocator BUG-37215 */
    static inline iduMemAllocator* getTlsfAllocator(); 

    /* Data buffer block */
    static inline UInt      getBufferBlockSize();
    static inline UInt      getAllocableBufferBlockCnt();
    static inline void      incUsedBufferBlockCount( UInt aUsed );
    static inline void      decUsedBufferBlockCount( UInt aUsed );
    static inline IDE_RC    lock();
    static inline IDE_RC    unlock();
};

/* BUG-37215 */
inline iduMemAllocator* dkdDataBufferMgr::getTlsfAllocator()
{
    return mAllocator;
}

inline UInt  dkdDataBufferMgr::getBufferBlockSize()
{
    return mBufferBlockSize;
}

/* BUG-36895 */
inline UInt  dkdDataBufferMgr::getAllocableBufferBlockCnt()
{
    return (UInt)( ( mMaxBufferBlockCnt - mUsedBufferBlockCnt ) 
                   * mRecordBufferAllocRatio ) / 100;
}

inline void  dkdDataBufferMgr::incUsedBufferBlockCount( UInt aUsed )
{
    mUsedBufferBlockCnt += aUsed;
}

inline void  dkdDataBufferMgr::decUsedBufferBlockCount( UInt aUsed )
{
    mUsedBufferBlockCnt -= aUsed;
}

inline IDE_RC dkdDataBufferMgr::lock()
{
    return mDbmMutex.lock( NULL /* idvSQL* */ );
}

inline IDE_RC dkdDataBufferMgr::unlock()
{
    return mDbmMutex.unlock();
}

#endif /* _O_DKD_DATA_BUFFER_MGR_H_ */

