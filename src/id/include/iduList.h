/***********************************************************************
 * Copyright 1999-2000, RTBase Corporation or its subsidiaries.
 * All rights reserved.
 **********************************************************************/

/***********************************************************************
 * $Id: iduList.h 34702 2009-08-12 04:31:04Z lswhh $
 **********************************************************************/

#ifndef _IDU_LIST_H_
#define _IDU_LIST_H_

typedef struct iduList iduList;
typedef struct iduList iduListNode;

struct iduList
{
    iduList * mPrev;
    iduList * mNext;

    void    * mObj;
};


/*
 * Example :
 *      struct { iduList mList; int a; } sBar;
 *      iduList * sIterator;
 *      IDU_LIST_ITERATE(&sBar.mList, sIterator)
 *      {
 *          ..... whatever you want to do
 *          ����, ����Ʈ�� ���� ����� �ϳ��� �����ϴ� ���� �ؼ��� �ȵȴ�.
 *          ����Ʈ�� ����� �����ϰ��� �� ������
 *          IDU_LIST_ITERATE_SAFE() �� ������ �ؾ� �Ѵ�.
 *      }
 */

#define IDU_LIST_ITERATE(aHead, aIterator)                              \
    for ((aIterator) = (aHead)->mNext;                                  \
         (aIterator) != (aHead);                                        \
         (aIterator) = (aIterator)->mNext)

#define IDU_LIST_ITERATE_BACK(aHead, aIterator)                         \
    for ((aIterator) = (aHead)->mPrev;                                  \
         (aIterator) != (aHead);                                        \
         (aIterator) = (aIterator)->mPrev)
/***********************************************************************
*
* Description :
*  aHead�� ���� ����Ʈ���� aNode ���� ������ ������ ������ aIterator�� 
*  ��ȯ�� �ش�.
*  aHead          - [IN]  ����Ʈ ��� ������
*  aNode          - [IN]  Iterator�� ���� �˻��� ���ϴ� ����� Previous ���
*  aIterator      - [OUT] ��ȯ�Ǵ� ���
*
**********************************************************************/
#define IDU_LIST_ITERATE_AFTER2LAST(aHead, aNode, aIterator)            \
    for ((aIterator) = (aNode)->mNext;                                  \
         (aIterator) != (aHead);                                        \
         (aIterator) = (aIterator)->mNext)
/*
 * Example :
 *      struct { iduList mList; int a; } sBar;
 *      iduList * sIterator;
 *      iduList * sNodeNext;
 *      IDU_LIST_ITERATE_SAFE(&sBar.mList, sIterator, sNodeNext)
 *      {
 *          ..... whatever you want to do
 *      }
 */

#define IDU_LIST_ITERATE_SAFE(aHead, aIterator, aNodeNext)              \
    for((aIterator) = (aHead)->mNext, (aNodeNext) = (aIterator)->mNext; \
        (aIterator) != (aHead);                                         \
        (aIterator) = (aNodeNext), (aNodeNext) = (aIterator)->mNext)

#define IDU_LIST_ITERATE_BACK_SAFE(aHead, aIterator, aNodeNext)         \
    for((aIterator) = (aHead)->mPrev, (aNodeNext) = (aIterator)->mPrev; \
        (aIterator) != (aHead);                                         \
        (aIterator) = (aNodeNext), (aNodeNext) = (aIterator)->mPrev)

//fix PROJ-1596
#define IDU_LIST_ITERATE_SAFE2(aHead, aList, aIterator, aNodeNext)      \
    for((aIterator) = aList, (aNodeNext) = (aIterator)->mNext;          \
        (aIterator) != (aHead);                                         \
        (aIterator) = (aNodeNext), (aNodeNext) = (aIterator)->mNext)

/*
 * Head List �ʱ�ȭ
 */

#define IDU_LIST_INIT(aList)                                            \
    do                                                                  \
    {                                                                   \
        (aList)->mPrev = (aList);                                       \
        (aList)->mNext = (aList);                                       \
        (aList)->mObj  = NULL;                                          \
    } while (0)

/*
 * List Node �ʱ�ȭ
 */

#define IDU_LIST_INIT_OBJ(aList, aObj)                                  \
    do                                                                  \
    {                                                                   \
        (aList)->mPrev = (aList);                                       \
        (aList)->mNext = (aList);                                       \
        (aList)->mObj  = (aObj);                                        \
    } while (0)

/*
 * =====================================
 * List �� item �߰��ϴ� ��ƾ��
 * =====================================
 *
 * 1. IDU_LIST_ADD_AFTER(aNode1, aNode2)
 *    aNode1 �տ� aNode2 �߰�
 *
 * 2. IDU_LIST_ADD_BEFORE(aNode1, aNode2)
 *    aNode1 �ڿ� aNode2 �߰�
 *
 * 3. IDU_LIST_ADD_FIRST
 *    List ó���� aNode �߰�
 *
 *    ���� ��
 *     +------------------------------------------------+
 *     |                                                |
 *     +--> head <---> item1 <---> item2 <---> item3 <--+
 *                 ^
 *                 |
 *                 +-----���� ����Ʈ
 *    ���� ��
 *     +----------------------------------------------------------+
 *     |                                                          |
 *     +--> head <---> NEW <---> item1 <---> item2 <---> item3 <--+
 *
 *
 * 4. IDU_LIST_ADD_LAST
 *    List �������� aNode �߰�
 *
 *    ���� ��
 *     +--------------------------------------------------+
 *     |                                                  |
 *     +---> head <---> item1 <---> item2 <---> item3 <---+
 *                                                      ^
 *                                                      |
 *                        ���� ����Ʈ-------------------+
 *    ���� ��
 *     +----------------------------------------------------------+
 *     |                                                          |
 *     +--> head <---> item1 <---> item2 <---> item3 <---> NEW <--+
 */

#define IDU_LIST_ADD_AFTER(aNode1, aNode2)                              \
    do                                                                  \
    {                                                                   \
        (aNode1)->mNext->mPrev = (aNode2);                              \
        (aNode2)->mNext        = (aNode1)->mNext;                       \
        (aNode2)->mPrev        = (aNode1);                              \
        (aNode1)->mNext        = (aNode2);                              \
    } while (0)


#define IDU_LIST_ADD_BEFORE(aNode1, aNode2)                             \
    do                                                                  \
    {                                                                   \
        (aNode1)->mPrev->mNext = (aNode2);                              \
        (aNode2)->mPrev        = (aNode1)->mPrev;                       \
        (aNode1)->mPrev        = (aNode2);                              \
        (aNode2)->mNext        = (aNode1);                              \
    } while (0)


#define IDU_LIST_ADD_FIRST(aHead, aNode) IDU_LIST_ADD_AFTER(aHead, aNode)

#define IDU_LIST_ADD_LAST(aHead, aNode)  IDU_LIST_ADD_BEFORE(aHead, aNode)

/*
 * ===================
 * List���� aNode ����
 * ===================
 */

#define IDU_LIST_REMOVE(aNode)                                          \
    do                                                                  \
    {                                                                   \
        (aNode)->mNext->mPrev = (aNode)->mPrev;                         \
        (aNode)->mPrev->mNext = (aNode)->mNext;                         \
    } while (0)

/*
 * ======================
 * List�� ����ִ��� �˻�
 * ======================
 */

#define IDU_LIST_IS_EMPTY(aList)                                        \
    ((((aList)->mPrev == (aList)) && ((aList)->mNext == (aList)))       \
     ? ID_TRUE : ID_FALSE)

/*
 * ==============================
 *
 * List�� �ٸ� List�� Node�� ��ħ
 *
 * ==============================
 *
 * ---------------------
 * 1. IDU_LIST_JOIN_NODE
 * ---------------------
 *
 *      Join ��
 *       +------------------------------------+
 *       |                                    |
 *       +--> List <---> item1 <---> item2 <--+
 *
 *                  +-------------------+
 *                  |                   |
 *       aNode --> ITEMA <---> ITEMB <--+
 *
 *      Join ��
 *       +------------------------------------------------------------+
 *       |                                                            |
 *       +--> List <---> item1 <---> item2 <---> ITEMA <---> ITEMB <--+
 *                                                 ^
 *                                                 |
 *             aNode ------------------------------+
 *
 * ---------------------
 * 2. IDU_LIST_JOIN_LIST
 * ---------------------
 *      - aList2�� ��������� �ƹ� �ϵ� ����
 *      - ������ �� aList2�� �ʱ�ȭ��
 *
 *      Join ��
 *       +------------------------------------+
 *       |                                    |
 *       +--> List1 <--> item1 <---> item2 <--+
 *
 *       +------------------------------------------------+
 *       |                                                |
 *       +--> List2 <--> ITEMA <---> ITEMB <---> ITEMC <--+
 *
 *      Join ��
 *       +------------------------------------------------------------------------+
 *       |                                                                        |
 *       +--> List1 <--> item1 <---> item2 <---> ITEMA <---> ITEMB <---> ITEMC <--+
 *
 *       +--> List2 <--+
 *       |             |
 *       +-------------+
 */

#define IDU_LIST_JOIN_NODE(aList, aNode)                                \
    do                                                                  \
    {                                                                   \
        iduList *sTmpNod;                                               \
                                                                        \
        (aList)->mPrev->mNext = (aNode);                                \
        (aNode)->mPrev->mNext = (aList);                                \
                                                                        \
        sTmpNod               = (aList)->mPrev;                         \
        (aList)->mPrev        = (aNode)->mPrev;                         \
        (aNode)->mPrev        = sTmpNod;                                \
    } while (0)

#define IDU_LIST_JOIN_LIST(aList1, aList2)                              \
    do                                                                  \
    {                                                                   \
        if (IDU_LIST_IS_EMPTY(aList2) != ID_TRUE)                       \
        {                                                               \
            IDU_LIST_REMOVE(aList2);                                    \
            IDU_LIST_JOIN_NODE(aList1, (aList2)->mNext);                \
            IDU_LIST_INIT(aList2);                                      \
        }                                                               \
    } while (0)

/*
 * ==============================
 *
 * List �� �ΰ��� List �� ������
 *
 * ==============================
 *
 * ----------------------
 * 1. IDU_LIST_SPLIT_LIST
 * ----------------------
 *   aSourceList ���� aNode ���� ������ �� ��� aNewList �� �����
 *
 *   a. aSourceList �� aNode �� ���ų�, aSourceList �� ��� �ִ� ���
 *      �ƹ��ϵ� ���Ͼ��.
 *
 *   b. aSourceList �� ��尡 �ϳ��ۿ� ���� ���
 *
 *      Split ��
 *       +-------------------------------+
 *       |                               |
 *       +--> aSourceList <---> item1 <--+
 *                                ^
 *       +----------------+       |
 *       |                |       |
 *       +--> aNewList <--+       +-- aNode
 *
 *      Split ��
 *       +-------------------+
 *       |                   |
 *       +--> aSourceList <--+
 *
 *       +----------------------------+
 *       |                            |
 *       +--> aNewList <---> item1 <--+
 *                             ^
 *                             |
 *                             +------- aNode
 *   c. �Ϲ����� ���
 *
 *      Split ��
 *       +-------------------------------------------------------------------+
 *       |                                                                   |
 *       +--> aSourceList <---> item1 <---> item2 <---> item3 <---> item4 <--+
 *                                                        ^
 *       +----------------+                               |
 *       |                |                               |
 *       +--> aNewList <--+          aNode ---------------+
 *
 *      Split ��
 *       +-------------------------------------------+
 *       |                                           |
 *       +--> aSourceList <---> item1 <---> item2 <--+
 *
 *       +----------------------------------------+
 *       |                                        |
 *       +--> aNewList <---> item3 <---> item4 <--+
 *                             ^
 *                             |
 *                             +------- aNode
 *
 */

#define IDU_LIST_SPLIT_LIST(aSourceList, aNode, aNewList)           \
    do                                                              \
    {                                                               \
        if ((aNode) != (aSourceList))                               \
        {                                                           \
            if (IDU_LIST_IS_EMPTY((aSourceList)) != ID_TRUE)        \
            {                                                       \
                (aNewList)->mPrev           = (aSourceList)->mPrev; \
                (aNewList)->mNext           = (aNode);              \
                                                                    \
                (aSourceList)->mPrev        = (aNode)->mPrev;       \
                (aSourceList)->mPrev->mNext = (aSourceList);        \
                                                                    \
                (aNode)->mPrev              = (aNewList);           \
                (aNewList)->mPrev->mNext    = (aNewList);           \
            }                                                       \
        }                                                           \
    } while (0)

// Previous ���
#define IDU_LIST_GET_PREV(node)  (node)->mPrev

// Next ���
#define IDU_LIST_GET_NEXT(node)  (node)->mNext


// ó�� ���� ����
#define IDU_LIST_GET_FIRST(base)  (base)->mNext

// ������ ���� ����
#define IDU_LIST_GET_LAST(base)  (base)->mPrev

#endif /* _IDU_LIST_H_ */
