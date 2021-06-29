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
 * $Id: smuList.h 82075 2018-01-17 06:39:52Z jina.kim $
 **********************************************************************/

#ifndef _O_SMU_LIST_H_
#define _O_SMU_LIST_H_ 1

#include <idl.h>
#include <smDef.h>

/*
 *  SM ���ο��� ���Ǵ� �޸� ����Ʈ ��忡 ���� ���̺귯���� �����Ѵ�.
 *  smuList�� ����Ͽ�, ���ΰ��� ��ũ�� �����Ѵ�.
 *
 *  ���� : !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
 *           BaseNode�� ��쿡�� �ݵ�� �ʱ�ȭ ��ũ�θ� �̿��Ͽ�,
 *                                    (SMU_LIST_INIT_BASE)
 *           Circular ������ ��ũ�� �����ϵ��� �ؾ� �Ѵ�. �׷��� ������,
 *           �Ʒ��� ���ǵ� ���� ��ũ���� ��Ȯ���� ������ �� ����.
 *         !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
 *
 * Sample)
 *  struct SampleNode {
 *     UInt mCount;
 *     smuList mBaseNode;
 *  };
 *
 *  void initializeSampleObject(struct SampleNode *aNode)
 *  {
 *       aNode->mCount = 0;
 *       SMU_LIST_INIT_BASE( &(aNode->mBaseNode) );  <== ����!!!!!!!!!
 *  }
 *
 *  void operateNode(struct SampleNode *aNode)
 *  {
 *      smuList  sSample1;
 *      smuList  sSample2;
 *
 *      SMU_LIST_ADD_AFTER( &(aNode->mBaseNode), &sSample1);
 *      // ���� ���� : BASE-->Sample1
 *
 *      SMU_LIST_ADD_BEFORE( &(aNode->mBaseNode)->mNext, &sSample2);
 *      // ���� ���� : BASE-->Sample2-->Sample1 
 *      
 *      SMU_LIST_DELETE(&sSample1);
 *      // ���� ���� : BASE-->Sample2
 *
 *      SMU_LIST_GET_PREV(&sSample1) = NULL;
 *      SMU_LIST_GET_NEXT(&sSample1) = NULL;
 *  }
 *
 */

typedef struct smuList
{
    smuList *mPrev;
    smuList *mNext;
    void    *mData;
} smuList;

/*
 * Example :
 *      struct { smuList mList; int a; } sBar;
 *      smuList * sIterator;
 *      SMU_LIST_ITERATE(&sBar.mList, sIterator)
 *      {
 *          ..... whatever you want to do
 *          ����, ����Ʈ�� ���� ����� �ϳ��� �����ϴ� ���� �ؼ��� �ȵȴ�.
 *          ����Ʈ�� ����� �����ϰ��� �� ������
 *          SMU_LIST_ITERATE_SAFE() �� ������ �ؾ� �Ѵ�.
 *      }
 */

#define SMU_LIST_ITERATE(aHead, aIterator)                              \
    for ((aIterator) = (aHead)->mNext;                                  \
         (aIterator) != (aHead);                                        \
         (aIterator) = (aIterator)->mNext)

#define SMU_LIST_ITERATE_BACK(aHead, aIterator)                         \
    for ((aIterator) = (aHead)->mPrev;                                  \
         (aIterator) != (aHead);                                        \
         (aIterator) = (aIterator)->mPrev)

/*
 * Example :
 *      struct { smuList mList; int a; } sBar;
 *      smuList * sIterator;
 *      smuList * sNodeNext;
 *      SMU_LIST_ITERATE_SAFE(&sBar.mList, sIterator, sNodeNext)
 *      {
 *          ..... whatever you want to do
 *      }
 */

#define SMU_LIST_ITERATE_SAFE(aHead, aIterator, aNodeNext)              \
    for((aIterator) = (aHead)->mNext, (aNodeNext) = (aIterator)->mNext; \
        (aIterator) != (aHead);                                         \
        (aIterator) = (aNodeNext), (aNodeNext) = (aIterator)->mNext)

#define SMU_LIST_ITERATE_BACK_SAFE(aHead, aIterator, aNodeNext)         \
    for((aIterator) = (aHead)->mPrev, (aNodeNext) = (aIterator)->mPrev; \
        (aIterator) != (aHead);                                         \
        (aIterator) = (aNodeNext), (aNodeNext) = (aIterator)->mPrev)



//  Base Node�� ���� �ʱ�ȭ ��ũ�� 

#define SMU_LIST_INIT_BASE(node) { \
        (node)->mPrev = (node);    \
        (node)->mNext = (node);    \
        (node)->mData =  NULL;    \
}

#define SMU_LIST_IS_EMPTY(node) (((node)->mPrev == (node)) && ((node)->mNext == (node)))

#define SMU_LIST_IS_NODE_LINKED(node) (((node)->mPrev != NULL) && ((node)->mNext != NULL))

#define SMU_LIST_INIT_NODE(node) { \
        (node)->mPrev = NULL;      \
        (node)->mNext = NULL;      \
}

// Previous ���
#define SMU_LIST_GET_PREV(node)  ((node)->mPrev)

// Next ���
#define SMU_LIST_GET_NEXT(node)  ((node)->mNext)


// ó�� ���� ����
#define SMU_LIST_GET_FIRST(base)  ((base)->mNext)

// ������ ���� ����
#define SMU_LIST_GET_LAST(base)  ((base)->mPrev)


// tnode�� �տ� ��带 �ִ´�.
#define SMU_LIST_ADD_BEFORE(tnode, node) {   \
            (node)->mNext = (tnode);         \
            (node)->mPrev = (tnode)->mPrev;  \
            (tnode)->mPrev->mNext = (node);  \
            (tnode)->mPrev = (node);         \
}

// tnode�� �ڿ� ��带 �ִ´�.
#define SMU_LIST_ADD_AFTER(tnode, node)   {  \
            (node)->mPrev = (tnode);         \
            (node)->mNext = (tnode)->mNext;  \
            (tnode)->mNext->mPrev = (node);  \
            (tnode)->mNext = (node);         \
}

// tnode�� �ڿ� ��带 �ִ´�.
#define SMU_LIST_ADD_AFTER_SERIAL_OP(tnode, node)   {  \
        ID_SERIAL_BEGIN((node)->mPrev = (tnode));      \
        ID_SERIAL_END((node)->mNext = (tnode)->mNext); \
            (tnode)->mNext->mPrev = (node);  \
            (tnode)->mNext = (node);         \
}

#define SMU_LIST_ADD_FIRST_SERIAL_OP(base, node)  SMU_LIST_ADD_AFTER_SERIAL_OP(base, node)
#define SMU_LIST_ADD_FIRST(base, node)  SMU_LIST_ADD_AFTER(base, node)
#define SMU_LIST_ADD_LAST(base, node)   SMU_LIST_ADD_BEFORE(base, node)

#define SMU_LIST_CUT_BETWEEN(prev, next)   { \
            (prev)->mNext = NULL;            \
            (next)->mPrev = NULL;            \
}

#define SMU_LIST_ADD_LIST_FIRST(base, first, last) {  \
            (first)->mPrev = (base);                  \
            (last)->mNext = (base)->mNext;            \
            (base)->mNext->mPrev = (last);            \
            (base)->mNext = (first);                  \
}

#define SMU_LIST_ADD_LIST_FIRST_SERIAL_OP(base, first, last) { \
            ID_SERIAL_BEGIN((first)->mPrev = (base));          \
            ID_SERIAL_END((last)->mNext = (base)->mNext);      \
            (base)->mNext->mPrev = (last);                     \
            (base)->mNext = (first);                           \
}

// ��带 ����Ʈ�κ��� �����Ѵ�. 
#define SMU_LIST_DELETE(node)  {                  \
            (node)->mPrev->mNext = (node)->mNext; \
            (node)->mNext->mPrev = (node)->mPrev; \
}

#endif  // _O_SMU_LIST_H_
    
