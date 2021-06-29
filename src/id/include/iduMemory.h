/***********************************************************************
 * Copyright 1999-2000, AltiBase Corporation or its subsidiaries.
 * All rights reserved.
 **********************************************************************/

/***********************************************************************
 * $Id: iduMemory.h 87767 2020-06-12 04:34:48Z seulki $
 **********************************************************************/

/***********************************************************************
 *
 * NAME
 *   iduMemory.h
 *
 * DESCRIPTION
 *   Dynamic memory allocator.
 *
 * PUBLIC FUNCTION(S)
 *   iduMemory( ULong BufferSize, ULong Align )
 *      BufferSize�� �޸� �Ҵ��� ���� �߰� ������ ũ�� �Ҵ�޴�
 *      �޸��� ũ��� BufferSize�� �ʰ��� �� �����ϴ�.
 *      Align�� �Ҵ�� �޸��� �����͸� Align�� ����� �Ұ���
 *      �����ϴ� ���Դϴ�. �⺻ ���� 8Bytes �Դϴ�.
 *
 *   void* alloc( size_t Size )
 *      Size��ŭ�� �޸𸮸� �Ҵ��� �ݴϴ�.
 *
 *   void clear( )
 *      �Ҵ���� ��� �޸𸮸� ���� �մϴ�.
 *
 * NOTES
 *
 * MODIFIED    (MM/DD/YYYY)
 *    assam     01/12/2000 - Created
 *
 **********************************************************************/

#ifndef _O_IDUMEMORY_H_
# define _O_IDUMEMORY_H_  1

/**
 * @file
 * @ingroup idu
*/

#include <idl.h>
#include <iduMemDefs.h>
#include <iduMemMgr.h>

/// iduMemory �� ���� ���۵��� ����
typedef struct iduMemoryHeader {
    iduMemoryHeader* mNext;      /// ���� ������ ������
    char*            mBuffer;    /// ������ ��ġ
    ULong            mChunkSize; /// ������ ũ��
    ULong            mCursor;    /// ���� ���ο��� �� ������ ��ġ.
                                 /// �� ���۴� ���ۺκк��� ���ǹǷ�,
                                 /// ������� ������ �� ������ �� ������ ���� ��ġ�� �ȴ�.
} iduMemoryHeader;

/// iduMemory ��ü�� ���� ���� ����
typedef struct iduMemoryStatus {
    iduMemoryHeader* mSavedCurr;   /// ���� ������� ����
    ULong            mSavedCursor; /// ���ۿ��� �� ������ ��ġ
} iduMemoryStatus;


///
/// @class iduMemory
/// �ַ� Query Processing���� ���� �䱸�Ǵ� �޸� ��� ������
/// �����Ͱ� ��� �þ�ٰ� �Ѽ����� �����͸� �����ϴ� ���ð� ������ �����̴�.
/// �� ���Ͽ� ���� �޸𸮸� �Ҵ�޾Ƽ� �����͸� ����, �� �Ҵ�޾Ƽ� �����͸� ���⸦
/// �ݺ��ϴٰ� ��� ���� �ʿ信 ���� Ư���� �������� �ǵ��ư���
/// �ٽ� �����͸� ���ų� ���� �����͸� �д� ���� ���� �ϰԵȴ�.
/// �Ǵٸ� �߿��� Ư¡�� ���������� �󸶳� ū �����Ͱ� ���޵��� �𸣹Ƿ�
/// �Ҵ���� �޸� ũ�⸦ �̸� �� �� ���ٴ� ���̴�.
/// ���� iduMemory�� ���� ������ ũ���� �޸𸮸� �Ҵ�޾� ����ϰ�
/// �� �� ���Ŀ��� �� �޸𸮸� �Ҵ�޾� ����ϱ⸦ �ݺ��ϴ�
/// �޸� �����ڰ� �ʿ�������.
/// ��ü�� ������ �޸𸮴� ���������δ� �������� ��������������
/// ��ġ �ϳ��� Ŀ�ٶ� ���ΰ�ó�� ����ϰԵȴ�.
/// 
/// @see struct iduMemoryHeader
class iduMemory {
private:
    iduMemoryHeader*        mHead;        // ���ʷ� ������ ����
    iduMemoryHeader*        mCurHead;     // ���� ������� ����
    ULong                   mDefaultChunkSize;   // ������ ũ��
    ULong                   mChunkCount;  // ������ ����
    iduMemoryClientIndex    mIndex;   // ��ü�� ������ ���
    iduMemAllocator         mPrivateAlloc; // Thread-local �޸� �Ҵ���
    static idBool           mUsePrivate;   // Thread-local �޸� �Ҵ��� ��� ����

    ULong                   mTotalChunkSize;

#if defined(ALTIBASE_MEMORY_CHECK)
    ULong                   mSize;
#endif
    
    IDE_RC header();
    IDE_RC extend( ULong aRequestedSize);
    IDE_RC release( iduMemoryHeader* Clue );
    IDE_RC checkMemoryMaximumLimit(ULong);
    IDE_RC malloc(size_t aSize, void** aPointer);
    IDE_RC free(void* aPointer);
public:

    /// Ŭ���� �ʱ�ȭ
    static IDE_RC initializeStatic( void );

    static IDE_RC destroyStatic( void );

    /// ��ü�� ���� ��� �޸𸮸� �����ϰ� ���� �Ҵ��ڸ� ����� ���
    /// ���� �Ҵ��ڱ��� �����Ѵ�.
    void destroy(void);

    /// ��ü�� ���¸� �ʱ�ȭ�Ѵ�.
    /// @param aIndex ��ü�� ����� ����� �ε���
    /// @param BufferSize ���� ���۸� �ø����� ũ��
    IDE_RC init( iduMemoryClientIndex aIndex,
                 ULong BufferSize = 0);

    /// 0���� �ʱ�ȭ�� �޸𸮸� �Ҵ�޴´�.
    /// @param aSize �ʿ��� �޸��� ũ��
    /// @param aMemPtr �Ҵ���� �޸��� ������
    IDE_RC cralloc( size_t aSize, void** aMemPtr );

    /// �޸𸮸� �Ҵ�޴´�.
    /// ���� ��ü���� �޸𸮰� �����ϸ� ��ü ���ο� ���ο� ���۸� �߰��ؼ� �޸𸮸� ��ȯ�Ѵ�.
    /// @param aSize �ʿ��� �޸��� ũ��
    /// @param aMemPtr �Ҵ���� �޸��� ������
    IDE_RC alloc( size_t aSize, void** aMemPtr );

    /// ��ü�� ���� ���¸� ���´�.
    /// @param Status ���� ����
    SInt  getStatus( iduMemoryStatus* Status );
    /// ������ Ư�� �������� ��ü ���¸� �ǵ�����.
    /// A��� ������������ B��� �������� �޸𸮿� �����͸� ����� �Ŀ�
    /// A~B�Ⱓ���� ����� �����Ͱ� �ʿ���� �� A�������� ���ư��� ����
    /// ���Ǵ� �Լ��̴�. A������ ���� ������ getStatus �Լ��� �̿��ؼ� iduMemoryStatus�� ������ ��
    /// �Լ� ���ڷ� �����ϸ� A���� ���Ŀ� ���� �޸� ������ �����͸� �����ϰ�
    /// �ٽ� ����� �� �ֵ��� �ʱ�ȭ�Ѵ�.
    SInt  setStatus( iduMemoryStatus* Status );

    /// ��ü�� ���� �޸� ���۵��� ���������� ������
    /// �޸� ���� �����͵��� �����ϰ� �޸� ���� �ٽ� ����� �� �ֵ��� �Ѵ�.
    void  clear( void );

    /// ��ü�� ���� �޸� ���۵��� ��� �����Ѵ�. ���� �Ҵ��ڸ� ����� ���
    /// ���� �Ҵ��ڴ� �������� �ʴ´�. ���� �Ҵ��ڴ� �ı��ڿ����� ������ �� �ִ�.
    void  freeAll( UInt a_min_chunk_no );

    /// ������ �ʴ� ���۵鸸 �����Ѵ�.
    void  freeUnused( void );
    /// ��ü�� ������ �ִ� �� �޸��� ũ�⸦ ��ȯ�Ѵ�.
    ULong getSize( void );

    /// �� ������ ��ȣ�� ũ�⸦ �͹̳ο� ����Ѵ�.
    void dump();
};

#endif /* _O_IDUMEMORY_H_ */
