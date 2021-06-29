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
 * $Id: smuHash.h 82075 2018-01-17 06:39:52Z jina.kim $
 **********************************************************************/

#ifndef _O_SMU_HASH_H_
#define _O_SMU_HASH_H_ 1

#include <idl.h>
#include <smDef.h>
#include <smuList.h>
#include <iduLatch.h>
/*
 * Hash
 * ����ڴ� �ؽ� ��ü�� ����ϱ� ���� �Ʒ��� �Ӽ��� initialize()�� �Ѱܾ� �Ѵ�.
 * 
 *  - ���� ����   : �Էµ� ����� �ִ� ������ ����� �����Ǿ�� ��.
 *  - ���ü� ���� : ���� ��ü�� �̿��ϴ� ������, �ټ� �����尡 ���ÿ� �̿��ϴ�
 *                  �������� ���� ��ġ (�� ��� �Ϲ������� CPU���� * 2)
 *  - Mutex�� ����� �������� ���� Flag 
 *  - ���� Ű�� ����
 *  - Ű�κ��� ��� �ؽ� �Լ�
 *  - Ű���� �� �Լ�
 */


typedef struct smuHashChain
{
    smuList  mList; /* for double-linked list  */
    void    *mNode; /* real Hash Target Node   */
    ULong    mKey[1];  /* ��忡 ���� Ű�� �����Ͱ� ���� : 8�� align��  */
}smuHashChain;

typedef struct smuHashBucket
{
    iduLatch     * mLock;      /* bucket�� ���� ���ü� ���� */
    UInt           mCount;     /* ���� bucket�� ����� ��� ���� */
    smuList        mBaseList;  /* double-link list�� base list */
}smuHashBucket;

typedef UInt (*smuHashGenFunc)(void *);
typedef SInt (*smuHashCompFunc)(void *, void *);

struct smuHashBase;

typedef struct smuHashLatchFunc
{
    IDE_RC (*findNode)  (smuHashBase   *aBase, 
                         smuHashBucket *aBucket, 
                         void          *aKeyPtr, 
                         void         **aNode);
    IDE_RC (*insertNode)(smuHashBase   *aBase, 
                         smuHashBucket *aBucket, 
                         void          *aKeyPtr, 
                         void          *aNode);
    IDE_RC (*deleteNode)(smuHashBase   *aBase, 
                         smuHashBucket *aBucket, 
                         void          *aKeyPtr, 
                         void         **aNode);
}smuHashLatchFunc;

typedef struct smuHashBase
{
    iduMutex           mMutex;       /* Hash ��ü�� mutex */

    void              *mMemPool;     /* Chain�� �޸� ������ : 
                                        struct���� class ��� �Ұ�. so, void */
    UInt               mKeyLength;
    UInt               mBucketCount;
    smuHashBucket     *mBucket;      /* Bucket List */
    smuHashLatchFunc  *mLatchVector;

    smuHashGenFunc     mHashFunc;    /* HASH �Լ� : callback */
    smuHashCompFunc    mCompFunc;    /* �� �Լ� : callback */

    /* for Traverse  */
    idBool             mOpened;      /* Traverse�� Open ���� */
    UInt               mCurBucket;   /* ��������   Bucket ��ȣ */
    smuHashChain      *mCurChain;    /* ���� ����  Chain ������ */
    //fix BUG-21311
    idBool             mDidAllocChain;

} smuHashBase;


class smuHash
{
    static smuHashLatchFunc mLatchVector[2];

    static smuHashChain* findNodeInternal(smuHashBase   *aBase, 
                                          smuHashBucket *aBucket, 
                                          void          *aKeyPtr);

    static IDE_RC findNodeLatch(smuHashBase   *aBase, 
                                smuHashBucket *aBucket, 
                                void          *aKeyPtr, 
                                void         **aNode);
    static IDE_RC insertNodeLatch(smuHashBase   *aBase, 
                                  smuHashBucket *aBucket, 
                                  void          *aKeyPtr, 
                                  void          *aNode);
    static IDE_RC deleteNodeLatch(smuHashBase   *aBase, 
                                  smuHashBucket *aBucket, 
                                  void          *aKeyPtr, 
                                  void         **aNode);

    static IDE_RC findNodeNoLatch(smuHashBase   *aBase, 
                                  smuHashBucket *aBucket, 
                                  void          *aKeyPtr, 
                                  void         **aNode);
    static IDE_RC insertNodeNoLatch(smuHashBase   *aBase, 
                                    smuHashBucket *aBucket, 
                                    void          *aKeyPtr, 
                                    void          *aNode);
    static IDE_RC deleteNodeNoLatch(smuHashBase   *aBase, 
                                    smuHashBucket *aBucket, 
                                    void          *aKeyPtr, 
                                    void         **aNode);

    static IDE_RC allocChain(smuHashBase *aBase, smuHashChain **aChain);
    static IDE_RC freeChain( smuHashBase *aBase, smuHashChain *aChain);

    static smuHashChain *searchFirstNode(smuHashBase *aBase);
    static smuHashChain *searchNextNode (smuHashBase *aBase);
    
    

public:
    static IDE_RC initialize(smuHashBase    *aBase, 
                             UInt            aConcurrentLevel,/* > 0 */
                             UInt            aBucketCount,
                             UInt            aKeyLength,
                             idBool          aUseLatch,
                             smuHashGenFunc  aHashFunc,
                             smuHashCompFunc aCompFunc);

    static IDE_RC destroy(smuHashBase  *aBase);
    static IDE_RC reset(smuHashBase  *aBase);
    
    static IDE_RC findNode(smuHashBase  *aBase, void *aKeyPtr, void **aNode);
    static IDE_RC insertNode(smuHashBase  *aBase, void *aKeyPtr, void *aNode);
    static IDE_RC deleteNode(smuHashBase  *aBase, void *aKeyPtr, void **aNode);

    /*
     * !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! ���� !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
     *
     *  lock(), unlock() �Լ��� findNode(), deleteNode(), insertNode() ����
     *  �����Լ��� �����ϱ� ������ 
     *  lock(), unlock() �Լ��� ����Ѵٰ� �ؼ� �̷���
     *  �����Լ��� ������ ������ �� �ִٰ� �����ؼ��� �ȵȴ�!!!!
     *  �� �Լ��� ������ �ܺο��� ��� �ǵ��� ���Ǵ� Mutex �ǹ̸� ����
     *  ���� �Լ� �̻� �ƹ� �͵� �ƴϴ�.
     */
    static IDE_RC lock(smuHashBase  *aBase);
    static IDE_RC unlock(smuHashBase  *aBase);

    /*
     * !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! ���� !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
     *
     *  �Ʒ��� �Լ��� ȣ���� ��쿡�� �ƹ��� Concurrency Control��
     *  ���� �ʱ� ������, ȣ���ڴ� �Ʒ��� cut�Լ��� ��뿡 �־
     *  �����ؾ� �Ѵ�.
     *  ����, open(), cutNode() �������� �ٸ� �����尡 Insert Ȥ�� delete��
     *  �Ѵٸ�, ����ġ ���� ��Ȳ�� �߻��� �� �����Ƿ�,
     *  ��� �Լ� lock(), unlock()�� �̿��ؼ� �ܺ� ȣ�� �������� ���������
     *  ��� ���� �Լ��� ���� ó���� �� �־�� �Ѵ�.
     */

    // ���� Traverse & Node ���� �۾�
    //fix BUG-21311
    static inline IDE_RC open(smuHashBase *aBase);
    static inline IDE_RC cutNode(smuHashBase *aBase, void **aNode);
    static inline IDE_RC close(smuHashBase *aBase);
    static IDE_RC isEmpty(smuHashBase *aBase, idBool *aIsEmpty);

    /* BUG-40427 */
    static inline IDE_RC getCurNode(smuHashBase *aBase, void **aNode);
    static inline IDE_RC getNxtNode(smuHashBase *aBase, void **aNode);
    static IDE_RC delCurNode(smuHashBase *aBase, void **aNode);
};
//fix BUG-21311 transform inline function.
inline IDE_RC smuHash::open(smuHashBase *aBase)
{
    IDE_ASSERT(aBase->mOpened == ID_FALSE);
    
    aBase->mOpened    = ID_TRUE;
    aBase->mCurBucket = 0; // before search
    if(aBase->mDidAllocChain == ID_TRUE)
    {    
        aBase->mCurChain  = searchFirstNode(aBase);
    }
    else
    {
        //empty
        aBase->mCurChain  = NULL;
    }
    return IDE_SUCCESS;
}

IDE_RC smuHash::cutNode(smuHashBase *aBase, void **aNode)
{
    smuHashChain  *sDeleteChain;

    if (aBase->mCurChain != NULL)
    {
        // delete current node & return the node pointer
        sDeleteChain = aBase->mCurChain;
        *aNode       = sDeleteChain->mNode;

        // find next
        aBase->mCurChain = searchNextNode(aBase);
        
        SMU_LIST_DELETE(&(sDeleteChain->mList));
        IDE_TEST(freeChain(aBase, sDeleteChain) != IDE_SUCCESS);
    
    }
    else // Traverse? ??.
    {
        *aNode = NULL; 
    }
    return IDE_SUCCESS;
    
    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

inline IDE_RC smuHash::close(smuHashBase *aBase)
{
    IDE_ASSERT(aBase->mOpened == ID_TRUE);
    
    aBase->mOpened = ID_FALSE;
    
    return IDE_SUCCESS;
}

IDE_RC smuHash::getCurNode(smuHashBase *aBase, void **aNode)
{
    if (aBase->mCurChain != NULL)
    {   
        // return the node pointer
        *aNode = aBase->mCurChain->mNode;
    }   
    else // Traverse ����
    {   
        *aNode = NULL; 
    }   
    return IDE_SUCCESS;
}

IDE_RC smuHash::getNxtNode(smuHashBase *aBase, void **aNode)
{
    if (aBase->mCurChain != NULL)
    {   
        // find next
        aBase->mCurChain = searchNextNode(aBase);

        if( aBase->mCurChain != NULL )
        {   
            *aNode = aBase->mCurChain->mNode;
        }   
        else
        {   
            *aNode = NULL; 
        }   

    }   
    else
    {   
        *aNode = NULL;
    }   

    return IDE_SUCCESS;
}
#endif  // _O_SMU_HASH_H_
    
