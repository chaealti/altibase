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
 * $Id: smuDynArray.h 82075 2018-01-17 06:39:52Z jina.kim $
 **********************************************************************/

#ifndef _O_SMU_DYN_ARRAY_H_
#define _O_SMU_DYN_ARRAY_H_ 1

#include <idl.h>
#include <iduMemPool.h>
#include <smDef.h>
#include <smuList.h>
#include <iduLatch.h>
/*
 *  smuDynArray�� ����
 *
 *  �̸� ũ�⸦ ������ �� ���� ����Ÿ(���� ��� �α�)�� ������ ����
 *  �������� �޸� ������ �����ϰ�, ���Ŀ� �����͸� ������ ����
 *  �������� ����Ǿ���.
 *
 *  ũ�� ���� �Լ� store()�� ������ �Լ� load()�� ����������,
 *  store()�� ��� ȣ���ڰ� ������ ũ���� �����͸� ������ �� �ֵ��� �Ѵ�.
 *
 *  load()�� ��� smuDynArray ��ü�� ����� �����͸� �Էµ� ��� �޸�
 *  ������ ���������� ���縦 �ϵ��� �Ѵ�.
 *
 *  �׿��� API�� �������� ������ �䱸������ �߻��� ��� �����Ѵ�.
 *
 *
 *  LayOut :
 *
 *  BaseNode     SubNode     SubNode
 *  +------+    +------+    +------+
 *  | Size |<-->|      |<-->|      |<-- ��� ������.
 *  | Off  |    |      |    |      |
 *  | mem  |    | mem  |    | mem  |
 *  | (2k) |    | (?k) |    | (?k) |
 *  +------+    +------+    +------+
 *
 *  ���� �̽� 
 *  - BaseNode���� ���� ����� ũ�� �� ������ ����� ��� �� �����͸� �����Ѵ�.
 *    Ư��, ù��° ��忡 ���� �޸� �Ҵ� ����� ���̱� ����,
 *    BaseNode ���ο� ����ũ���� ���۸�(SMU_DA_BASE_SIZE) �����صξ�,
 *    ũ�Ⱑ ���� ��� ���� ó���� �����Ѵ�. 
 *
 *
 *  ���� ����
 *  - ���� ���α׷��� ������  initializeStatic() �Լ��� �ҷ�
 *    ���� �޸� ��ü�� �ʱ�ȭ�ϰ�, ����ÿ��� destroyStatic()�� �ҷ�
 *    ��ü�� �Ҹ���Ѿ� �Ѵ�. 
 *
 */


#define SMU_DA_BASE_SIZE      (2 * 1024)  // use 2k for base

typedef struct smuDynArrayNode
{
    smuList mList;       // List ���� 
    UInt    mStoredSize; // mChunkSize ��ŭ Ŀ�� �� ����.
    UInt    mCapacity;   // ���� �ִ� ũ�� 
    ULong   mBuffer[1];  // Node�� ���� �޸� ���� ���� �ּ�
                         //  smuDynArrayNode�� ������ �޸� ������ ����Ű�� 
                         //  �����͸� �����ϴ� ��� Node �Ҵ�ÿ� 
                         //  (ID_SIZEOF(smuDynArrayNode) + �޸�ũ��) ��ŭ �ƿ�
                         //  �Ҵ��� �������� ����Ǿ���.
                         //  ULong�� ������ ����� ������ ��������� ULong��
                         // ���� ��� SIGBUS�� ���� ������.
}smuDynArrayNode;

typedef struct smuDynArrayBase
{
    UInt            mTotalSize;  // �� ��ü�� ����� ��ü �޸� ũ��
    smuDynArrayNode mNode;
    ULong           mBuffer[SMU_DA_BASE_SIZE / ID_SIZEOF(ULong) - 1];// for align8
}smuDynArrayBase;


class smuDynArray
{
    static iduMemPool mNodePool;
    static UInt       mChunkSize;

    static IDE_RC allocDynNode(smuDynArrayNode **aNode);
    static IDE_RC freeDynNode(smuDynArrayNode *aNode);

    // ������ �޸� ��� ������ �����͸� �����Ѵ�.
    static IDE_RC copyData(smuDynArrayBase *aBase,
                           void *aDest, UInt *aStoredSize, UInt aDestCapacity,
                           void *aSrc,  UInt aSize);
    
public:
    static IDE_RC initializeStatic(UInt aNodeMemSize); // ���� �ʱ�ȭ 
    static IDE_RC destroyStatic();                     // ���� �Ҹ� 

    static IDE_RC initialize(smuDynArrayBase *aBase);  // ��ü ���� �ʱ�ȭ 
    static IDE_RC destroy(smuDynArrayBase *aBase);     // ��ü ���� ���� 

    // DynArray�� �޸� ����
    inline static IDE_RC store( smuDynArrayBase * aBase,
                                void            * aSrc,
                                UInt              aSize );
    // DynArray�� �޸𸮸� Dest�� ����.
    static void load(smuDynArrayBase *aBase, void *aDest, UInt aDestBuffSize);
    // ���� ����� �޸� ũ�⸦ ��ȯ��.
    inline static UInt getSize( smuDynArrayBase * aBase );
    
};

/*
 *  Tail Node�� ���ڷ� �Ѱ�, copyData�� �����Ѵ�. 
 */
inline IDE_RC smuDynArray::store( smuDynArrayBase * aBase,
                                  void            * aSrc,
                                  UInt              aSize )
{
    smuList *sTailList;    // for interation
    smuDynArrayNode *sTailNode;
    
    sTailList = SMU_LIST_GET_PREV( &(aBase->mNode.mList) );
    
    sTailNode = (smuDynArrayNode *)sTailList->mData;
    
    IDE_TEST( copyData( aBase,
                        (void *)(sTailNode->mBuffer),
                        &(sTailNode->mStoredSize),
                        sTailNode->mCapacity,
                        aSrc,
                        aSize ) != IDE_SUCCESS );
    
    return IDE_SUCCESS;
    
    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

/* ũ�� ���� */
inline UInt smuDynArray::getSize( smuDynArrayBase * aBase )
{
    return aBase->mTotalSize;
}

#endif  // _O_SMU_DYN_ARRAY_H_
    
