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
 * $Id: iduMemory.h 17293 2006-07-25 03:04:24Z mhjeong $
 **********************************************************************/

#ifndef _O_IDUVARMEMLIST_H_
# define _O_IDUVARMEMLIST_H_  1


/**
 * @file
 * @ingroup idu
*/

#include <idl.h>
#include <iduMemMgr.h>
#include <iduList.h>

typedef struct iduVarMemListStatus
{
    SChar *mCursor;
} iduVarMemListStatus;


/// @class iduVarMemList
/// �޸𸮸� �ѹ� �Ҵ��Ҷ����� �Ҵ�� �޸𸮸� ���� ���� ����Ʈ�� �߰��Ѵ�.
class iduVarMemList {
    
public:
    /* iduVarMemList(); */
    /* ~iduVarMemList(); */

    static IDE_RC initializeStatic( void );
    static IDE_RC destroyStatic( void );
    
    /// �޸� �޴����� �ʱ�ȭ �۾��� �����Ѵ�.
    /// ST �Լ� �������� �̹� �ʱ�ȭ�� �޸� �Ŵ����� ���� �����͸� �� ����ϱ� ������ �Ű澵 �ʿ䰡 ����. QP������ ���� aIndex�� ������ IDU_MEM_QMT�� ����ϰ� �ִ�.
    /// @param aIndex ��ü�� ����� ��� ��ȣ
    /// @param aMaxSize �޸� �ִ� ũ��, ���� ����
    IDE_RC               init( iduMemoryClientIndex aIndex, 
                               ULong                aMaxSize = ID_ULONG_MAX );
    /// �޸� �Ŵ����� ���� �۾��� �����Ѵ�.
    /// alloc�̳� realloc �Լ��� ����ؼ� �̹� �Ҵ� �Ǿ����� ���� �������� ����
    /// �޸𸮿� ���� ���� �۾� �� ���������� ���Ǵ� �޸� Ǯ�� ���� ���� �۾��� �����Ѵ�.
    /// ���� �޸� �Ҵ��ڸ� ����ϴ� ��� �޸� �Ҵ��ڸ� �����Ѵ�.
    /// ST �Լ� �������� destroy �Լ��� ����� ȣ���ؼ��� �ȵȴ�.
    IDE_RC               destroy(); 

    /// @a aSize �� ������ ũ�� ��ŭ�� �޸� ������ �Ҵ��Ѵ�.
    /// @param aSize �Ҵ��� �޸� ũ��
    /// @param aBuffer �޸� ������
    IDE_RC               alloc( ULong aSize, void **aBuffer );

    /// 0���� �ʱ�ȭ�� �޸𸮸� �Ҵ��Ѵ�.
    IDE_RC               cralloc( ULong aSize, void **aBuffer );

    /// �Ҵ�� �޸� ������ �����Ѵ�.
    /// alloc �̳� realloc �Լ��� ���ؼ� �Ҵ���� ���� �����͸� �ѱ�� ��� ASSERT ó�� �Ѵ�.
    IDE_RC               free( void *aBuffer );
    /// �Ҵ�� ��� �޸𸮿� ���� ���� �۾��� �����Ѵ�.
    /// ������ ���������� ���Ǵ� �޸� Ǯ�� ���� ���� �۾��� ������� �ʴ´�.
    IDE_RC               freeAll();
    /// ������ ������ ������ �����Ѵ�.
    IDE_RC               freeRange( iduVarMemListStatus *aBegin,
                                    iduVarMemListStatus *aEnd );

    //fix PROJ-1596
    /// ���� ��ü�� ���¸� ���´�.
    /// ���������δ� ���� ������ ��� ������ ǥ���ϱ�����
    /// �� ���� ����Ʈ�� �߰��ϰ�, �� ���� �����͸� �Ѱ��ش�.
    /// ���� getStatus�� ���� ���� �����͸� ����ϸ� �ȵȴ�.
    IDE_RC               getStatus( iduVarMemListStatus *aStatus );
    /// ��ü�� Ư�� ������ ���·� �ǵ�����.
    IDE_RC               setStatus( iduVarMemListStatus *aStatus );
    /// ������� �ʴ� ���鸸 �����Ѵ�.
    void                 freeUnused( void ) {};
    /// ��� ���� �����Ѵ�.
    void                 clear() { (void)freeAll(); };

    // PROJ-1436
    /// ��ü�� �Ҵ��� �޸��� �� ��
    ULong                getAllocSize();
    /// �޸� �Ҵ� Ƚ��
    UInt                 getAllocCount();
    iduList *            getNodeList();
    /// ���� ��ü�� ���� �޸� ������ �����Ѵ�.
    /// @param aMem ������ ����Ʈ
    IDE_RC               clone( iduVarMemList *aMem );
    /// �޸� ������ ������ ���Ѵ�.
    SInt                 compare( iduVarMemList *aMem );

    /// �� �޸� ������ ũ��� ���� ����� ���ڿ��� �͹̳ο� ����Ѵ�.
    void                 dump();

private:
    ULong                getSize( void *aBuffer );
    void                 setSize( void *aBuffer, ULong aSize );

    iduMemoryClientIndex mIndex;
    iduList              mNodeList;
    ULong                mSize;
    ULong                mMaxSize;
    UInt                 mCount;

    iduMemAllocator      mPrivateAlloc; // Thread-local �޸� �Ҵ���
    static idBool        mUsePrivate;   // Thread-local �޸� �Ҵ��� ��� ����
};

#endif /* _O_IDUVARMEMLIST_H_ */
