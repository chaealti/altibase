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
 
/*****************************************************************************
 * $Id:
 ****************************************************************************/

/*****************************************************************************
 *   NAME
 *     iddTRBTree.cpp - Red Black tree
 *
 *   DESCRIPTION
 *     Red-Black tree implementation
 *
 *   MODIFIED   (04/07/2017)
 ****************************************************************************/

#include <iddTRBTree.h>

void* iddTRBTree::node::getKey(void) const
{
    return (this==NULL)? NULL:(void*)mKey;
}

void* iddTRBTree::node::getData(void) const
{
    return (this==NULL)? NULL:(void*)mData;
}

iddTRBTree::color iddTRBTree::node::getColor(void) const
{
    return (this==NULL)? IDD_BLACK:mColor;
}

void iddTRBTree::node::setColor(const iddTRBTree::color aColor)
{
    IDE_DASSERT(this != NULL);
    IDE_DASSERT((aColor == IDD_RED) || (aColor == IDD_BLACK));
    mColor = aColor;
}

iddTRBTree::node* iddTRBTree::node::setParent(iddTRBTree::node* aParent)
{
    iddTRBTree::node* sOldParent;
    if(this != NULL)
    {
        sOldParent = mParent;
        mParent = aParent;
    }
    else
    {
        sOldParent = NULL;
    }

    return sOldParent;
}

iddTRBTree::node* iddTRBTree::node::getPrev(void)
{
    return (this == NULL)? NULL:mPrev;
}

iddTRBTree::node* iddTRBTree::node::getNext(void)
{
    return (this == NULL)? NULL:mNext;
}

iddTRBTree::node* iddTRBTree::node::setPrev(iddTRBTree::node* aNewPrev)
{
    iddTRBTree::node* sOldPrev;
    if(this != NULL)
    {
        sOldPrev = mPrev;
        mPrev = aNewPrev;
    }
    else
    {
        sOldPrev = NULL;
    }

    return sOldPrev;
}

iddTRBTree::node* iddTRBTree::node::setNext(iddTRBTree::node* aNewNext)
{
    iddTRBTree::node* sOldNext;
    if(this != NULL)
    {
        sOldNext = mNext;
        mNext = aNewNext;
    }
    else
    {
        sOldNext = NULL;
    }

    return sOldNext;
}

idBool iddTRBTree::node::isLeftChild(void)
{
    IDE_DASSERT(this != NULL);
    IDE_DASSERT(mParent != NULL);
    return (mParent->mLeft == this)? ID_TRUE:ID_FALSE;
}

/**
 * Red-Black Tree�� �ʱ�ȭ�Ѵ�.
 *
 * @param aIndex : ���������� ����� �޸� �Ҵ� �ε���
 * @param aKeyLength : Ű ũ��(bytes)
 * @param aCompFunc : �񱳿� �Լ�.
 *                    SInt(const void* aKey1, const void* aKey2) �����̸�
 *                    aKey1 >  aKey2�̸� 1 �̻���,
 *                    aKey1 == aKey2�̸� 0 ��
 *                    aKey1 < aKey2�̸� -1 ���ϸ� �����ؾ� �Ѵ�.
 * @return IDE_SUCCESS
 *         IDE_FAILURE
 */
IDE_RC iddTRBTree::initialize(const iduMemoryClientIndex    aIndex,
                             const ULong                    aKeyLength,
                             const iddCompFunc              aCompFunc)
{
    mMemIndex   = aIndex;
    mKeyLength  = aKeyLength;
    mNodeSize   = idlOS::align8(offsetof(iddTRBTree::node, mKey) + mKeyLength);
    mNodeCount  = 0;
    mRoot       = NULL;
    mCompFunc   = aCompFunc;

    mCount              = 0;
    mSearchCount        = 0;

    return IDE_SUCCESS;
}

/**
 * clear�Ѵ�
 * �߰� destructor ������ ���� �� ����
 * @return IDE_SUCCESS
 *         IDE_FAILURE
 */
IDE_RC iddTRBTree::destroy(void)
{
    /* free all nodes */
    return clear();
}

/**
 * Red-Black Ʈ���� Ű/�����͸� �߰��ϰ� �ұ����� �ؼ��Ѵ�
 *
 * @param aKey : Ű
 * @param aData : ���� ���ϰ� �ִ� ������
 * @return IDE_SUCCESS
 *         IDE_FAILURE �ߺ� ���� �ְų� �޸� �Ҵ翡 �������� ��
 */
IDE_RC iddTRBTree::insert(const void* aKey, void* aData)
{
    iddTRBTree::node*   sNewNode;
    iddTRBTree::node*   sNode;
    iddTRBTree::finder  sFinder(mCompFunc);

    sNode = findPosition(aKey, sFinder);
    IDE_TEST_RAISE( (sNode != NULL) && (sFinder.isFound() == ID_TRUE),
                    EDUPLICATED );
    IDE_TEST( allocNode(&sNewNode) != IDE_SUCCESS );
    idlOS::memcpy(sNewNode->mKey, aKey, mKeyLength);
    sNewNode->mData = aData;
    
    if(mRoot == NULL)
    {
        mRoot = sNewNode;
        mRoot->setPrev(NULL);
        mRoot->setNext(NULL);
    }
    else
    {
        if(sFinder.isLesser())
        {
            sNode->mLeft = sNewNode;

            sNewNode->setNext(sNode);
            sNewNode->setPrev(sNode->getPrev());
            sNode->getPrev()->setNext(sNewNode);
            sNode->setPrev(sNewNode);
        }
        else
        {
            sNode->mRight = sNewNode;

            sNewNode->setPrev(sNode);
            sNewNode->setNext(sNode->getNext());
            sNode->getNext()->setPrev(sNewNode);
            sNode->setNext(sNewNode);
        }

        sNewNode->mParent = sNode;
    }

    adjustInsertCase1(sNewNode);
    mCount++;
    mInsertLeft     += sFinder.getLeftMove();
    mInsertRight    += sFinder.getRightMove();

    return IDE_SUCCESS;

    IDE_EXCEPTION( EDUPLICATED )
    {
        /* do nothing */
    }

    IDE_EXCEPTION_END;
    return IDE_FAILURE;
}

/**
 * Red-Black Ʈ������ Ű�� �˻��Ѵ�
 * �˻��� Ű�� �ش��ϴ� �����ʹ� aData�� ����ȴ�
 *
 * @param aKey : Ű
 * @param aData : �˻��� ���� ���� ������. ���� ������ NULL�� ����ȴ�.
 * @return ID_TRUE  aKey�� ã���� ��
 *         ID_FALSE aKey�� ã�� ������ ��
 */
idBool iddTRBTree::search(const void* aKey, void** aData)
{
    iddTRBTree::node*    sNode;
    iddTRBTree::finder   sFinder(mCompFunc);
    void*               sData;

    sNode = findPosition(aKey, sFinder);

    if(sFinder.isFound() == ID_TRUE)
    {
        sData = sNode->mData;
        mSearchCount++;
    }
    else
    {
        sData  = NULL;
    }

    if(aData != NULL)
    {
        *aData = sData;
    }

    return sFinder.isFound();
}

/**
 * Red-Black Tree���� aKey�� �ش��ϴ� �����͸� aNewData�� ��ü�Ѵ�
 * ������ �����ʹ� aOldData�� �Ƿ����´�
 *
 * @param aKey : Ű
 * @param aNewData : �� ��
 * @param aOldData : ���� �����͸� ������ ������. NULL�� �� �� �ִ�.
 * @return IDE_SUCCESS ���� ã���� ��
 *         IDE_FAILURE ���� ���� ��
 */
IDE_RC iddTRBTree::update(const void* aKey, void* aNewData, void** aOldData)
{
    iddTRBTree::node*    sNode;
    void*               sData = NULL;
    iddTRBTree::finder   sFinder(mCompFunc);

    sNode = findPosition(aKey, sFinder);
    IDE_TEST( sFinder.isFound() == ID_FALSE );
    sData = sNode->mData;
    sNode->mData = aNewData;

    if(aOldData != NULL)
    {
        *aOldData = sData;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    return IDE_FAILURE;
}

/**
 * Red-Black Ʈ������ Ű/�����͸� �����Ѵ�
 * ������ ���� aData�� ����ȴ�.
 *
 * @param aKey : Ű
 * @param aData : ������ ���� ���� ������. ���� ������ NULL�� ����ȴ�.
 * @return IDE_SUCCESS
 *         IDE_FAILURE �޸� ������ �������� ��
 */
IDE_RC iddTRBTree::remove(const void* aKey, void** aData)
{
    iddTRBTree::node*    sNode;
    void*               sData   = NULL;
    iddTRBTree::finder   sFinder(mCompFunc);

    sNode = findPosition(aKey, sFinder);
    if(sFinder.isFound() == ID_TRUE)
    {
        sData = sNode->mData;
        IDE_TEST( removeNode(sNode) != IDE_SUCCESS );
        mCount--;
    }
    else
    {
        /* do nothing */
    }

    if(aData != NULL)
    {
        *aData = sData;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    return IDE_FAILURE;
}

/**
 * Red-Black Ʈ������ ��� Ű/�����͸� �����ϰ� �޸𸮸� ��ȯ�Ѵ�
 *
 * @return IDE_SUCCESS
 *         IDE_FAILURE �޸� ������ �������� ��
 */
IDE_RC iddTRBTree::clear(void)
{
    IDE_TEST( freeAllNodes(mRoot) != IDE_SUCCESS );
    mCount  = 0;
    mRoot   = NULL;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    return IDE_FAILURE;
}

/**
 * Red-Black Ʈ���� ��������� ��� �ʱ�ȭ�Ѵ�
 */
void iddTRBTree::clearStat(void)
{
    mSearchCount    = 0;
    mInsertLeft     = 0;
    mInsertRight    = 0;
}

/**
 * Red-Black Ʈ���� ����� ���ο� ��� �Ҵ�
 *
 * @param aNode : �� ��� ������
 * @return IDE_SUCCESS
 *         IDE_FAILURE �޸� �Ҵ翡 �������� ��
 */
IDE_RC iddTRBTree::allocNode(iddTRBTree::node** aNode)
{
    iddTRBTree::node* sNewNode;
    IDE_TEST( iduMemMgr::malloc(mMemIndex, mNodeSize,
                                (void**)&sNewNode) != IDE_SUCCESS );
    sNewNode->mColor    = IDD_RED;
    sNewNode->mParent   = NULL;
    sNewNode->mLeft     = NULL;
    sNewNode->mRight    = NULL;
    sNewNode->mPrev     = NULL;
    sNewNode->mNext     = NULL;
    sNewNode->mData     = NULL;

    *aNode = sNewNode;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    return IDE_FAILURE;
}

/**
 * ����� �޸𸮸� �����Ѵ�
 *
 * @param aNode : ������ ��� ������
 * @return IDE_SUCCESS
 *         IDE_FAILURE �޸� ������ �������� ��
 */
IDE_RC iddTRBTree::freeNode(iddTRBTree::node* aNode)
{
    return iduMemMgr::free(aNode);
}

/**
 * Ʈ������ ���� ���� ��带 ã�´�.
 *
 * @return ����� ������. Ʈ���� ��������� NULL
 */
iddTRBTree::node* iddTRBTree::findFirstNode(void) const
{
    iddTRBTree::node* sNode = mRoot;
    iddTRBTree::node* sPrev = mRoot;

    while(sNode != NULL)
    {
        sPrev = sNode;
        sNode = sPrev->mLeft;
    }

    return sPrev;
}

/**
 * Ʈ������ aIter���� ū ���� ���� ��带 ã�´�
 * DEBUG ��忡���� aIter�� ���� �ν��Ͻ� �ȿ� �ִ°�
 * �˻��Ͽ� ������ altibase_misc.log�� ���� Ʈ����
 * �����ϰ� ASSERTION�Ѵ�
 * RELEASE ��忡���� aIter�� ���� �ν��Ͻ� �ȿ� ���� ���� �ൿ��
 * ���ǵ��� �ʾҴ�
 *
 * @param aIter : ���
 * @return ����� ������. Ʈ���� ��������� NULL
 */
iddTRBTree::node* iddTRBTree::findNextNode(iddTRBTree::node* aIter)
    const
{
    iddTRBTree::node* sNode = aIter;
    iddTRBTree::node* sPrev;

#if defined(DEBUG)
    {
        iddTRBTree::finder sFinder(mCompFunc);
        findPosition(aIter->getKey(), sFinder);
        if(sFinder.isFound() == ID_FALSE)
        {
            validate();
            idlOS::fprintf(stderr, "Not a valid node : %p\n", aIter);
            IDE_DASSERT(0);
        }
    }
#endif

    if(aIter->mRight == NULL)
    {
        sPrev = sNode->mParent;
        if(sPrev == NULL)
        {
            /* root and no right child */
            sNode = NULL;
        }
        else if(sNode == sPrev->mLeft)
        {
            sNode = sPrev;
        }
        else
        {
            do
            {
                sPrev = sNode;
                sNode = sPrev->mParent;
            } while( (sNode != NULL) && (sPrev == sNode->mRight) );
        }
    }
    else
    {
        sNode = aIter->mRight;
        while(sNode != NULL)
        {
            sPrev = sNode;
            sNode = sPrev->mLeft;
        }
        sNode = sPrev;
    }

    return sNode;
}

/**
 * Ʈ�� ���ο��� aKey�� ��ġ�� ã�´�
 *
 * @param aKey : Ű
 * @param aFinder : Ž���� �Լ���.
 *                  ���ÿ� Ž���� ����/������ �̵� ȸ���� ����Ѵ�.
 * @return ����� ������. aKey�� ã���� �ش� ����� �����͸�,
 *         ã�� ���ϸ� aKey�� ���� ����� Ű�� ���� ��带 �����Ѵ�.
 *         insert�� ����� ���ϰ��� �� ����� �θ� �ȴ�.
 *         Ʈ���� ��������� NULL�� �����Ѵ�.
 */
iddTRBTree::node* iddTRBTree::findPosition(const void*        aKey,
                                         iddTRBTree::finder& aFinder)
    const
{
    iddTRBTree::node*    sNode = mRoot;
    idBool              sFound = ID_FALSE;
    SInt                sValue;

    while((sNode != NULL) && (sFound != ID_TRUE))
    {
        sValue = aFinder(aKey, sNode->mKey);

        if(sValue == 0)
        {
            sFound = ID_TRUE;
        }
        else
        {
            if(sValue > 0)
            {
                if(sNode->mRight == NULL)
                {
                    sFound = ID_TRUE;
                }
                else
                {
                    sNode = sNode->mRight;
                }
            }
            else
            {
                if(sNode->mLeft == NULL)
                {
                    sFound = ID_TRUE;
                }
                else
                {
                    sNode = sNode->mLeft;
                }
            }
        }
    }

    return sNode;
}

/**
 * aNode�� �θ� ����� �θ� ��带 ã�Ƴ���
 *
 * @param aNode : ��� ������
 * @return aNode�� �θ� ����� �θ� ���. ���ٸ� NULL�� �����Ѵ�.
 */
iddTRBTree::node* iddTRBTree::getGrandParent(const iddTRBTree::node* aNode) const
{
    iddTRBTree::node* sNode;
    IDE_DASSERT(aNode != NULL);

    sNode = aNode->mParent->mParent;
    return sNode;
}

/**
 * aNode�� �θ� ����� ���� ��带 ã�Ƴ���
 *
 * @param aNode : ��� ������
 * @return aNode�� �θ� ����� ���� ���. ���ٸ� NULL�� �����Ѵ�.
 */
iddTRBTree::node* iddTRBTree::getUncle(const iddTRBTree::node* aNode) const
{
    iddTRBTree::node* sNode;
    iddTRBTree::node* sGP;
    IDE_DASSERT(aNode != NULL);

    sGP = getGrandParent(aNode);

    if(sGP != NULL)
    {
        sNode = aNode->mParent;

        if(sNode == sGP->mLeft)
        {
            sNode = sGP->mRight;
        }
        else
        {
            sNode = sGP->mLeft;
        }
    }
    else
    {
        sNode = NULL;
    }

    return sNode;
}

/**
 * aNode�� ���� ��带 ã�Ƴ���
 *
 * @param aNode : ��� ������
 * @return aNode�� ���� ���. ���ٸ� NULL�� �����Ѵ�.
 */
iddTRBTree::node* iddTRBTree::getSibling(const iddTRBTree::node* aNode) const
{
    iddTRBTree::node* sNode;
    IDE_DASSERT(aNode != NULL);

    sNode = aNode->mParent;
    if(aNode == sNode->mLeft)
    {
        sNode = sNode->mRight;
    }
    else
    {
        sNode = sNode->mLeft;
    }

    return sNode;
}

/**
 * insert�ÿ� ���� �ұ����� �ذ��Ѵ� : case 1
 * aNode�� root��� BLACK���� �����ϰ� ��
 * �ƴϸ� case 2�� ����
 * Note : ���� �Ҵ�޴� ���� �׻� RED�̴�
 *
 * @param aNode : �ұ����� �ذ��� ��� ������
 */
void iddTRBTree::adjustInsertCase1(iddTRBTree::node* aNode)
{
    IDE_DASSERT(aNode != NULL);
    if( aNode->mParent == NULL)
    {
        aNode->setColor(IDD_BLACK);
    }
    else
    {
        adjustInsertCase2(aNode);
    }
}

/**
 * insert�ÿ� ���� �ұ����� �ذ��Ѵ� : case 2
 * aNode�� BLACK�̶�� ������ �ʿ䰡 ���� ��
 * �ƴϸ� case 3�� ����
 *
 * @param aNode : �ұ����� �ذ��� ��� ������
 */
void iddTRBTree::adjustInsertCase2(iddTRBTree::node* aNode)
{
    if( aNode->mParent->mColor == IDD_BLACK )
    {
        /* return; */
    }
    else
    {
        adjustInsertCase3(aNode);
    }
}

/**
 * insert�ÿ� ���� �ұ����� �ذ��Ѵ� : case 3
 * �ű� ��� N�� �������̴�
 * �θ� ��� P�� ���� ��� U�� ��� RED�̸�
 * P�� U�� ��İ� ĥ�ϰ� �Ҿƹ��� ��� GP�� ������ ĥ�Ѵ�
 * ���� GP�� ���� case 1���� �ٽ� �����Ѵ�
 * �׷��� ������ case 4�� �����Ѵ�
 *
 * @param aNode : �ұ����� �ذ��� ��� ������
 */
void iddTRBTree::adjustInsertCase3(iddTRBTree::node* aNode)
{
    iddTRBTree::node* sUncle;
    iddTRBTree::node* sGP;

    sUncle = getUncle(aNode);
    if( (sUncle != NULL) && (sUncle->mColor == IDD_RED) )
    {
        aNode->mParent->mColor = IDD_BLACK;
        sUncle->mColor = IDD_BLACK;
        sGP = getGrandParent(aNode);
        IDE_DASSERT(sGP != NULL);
        sGP->mColor = IDD_RED;

        adjustInsertCase1(sGP);
    }
    else
    {
        adjustInsertCase4(aNode);
    }
}

/**
 * insert�ÿ� ���� �ұ����� �ذ��Ѵ� : case 4
 * �ű� ��� N�� �������̴�
 * �ű� ��� N�� �θ� ��� P�� RED�̰� ���� ��� U�� BLACK�� ��
 * P�� ȸ������ P�� ��ġ�� N�� ����ְ� P�� ���� 5�� ���̽��� �����Ѵ�
 *
 * @param aNode : �ұ����� �ذ��� ��� ������
 */
void iddTRBTree::adjustInsertCase4(iddTRBTree::node* aNode)
{
    iddTRBTree::node* sGP = getGrandParent(aNode);
    iddTRBTree::node* sNode;
    IDE_DASSERT(sGP != NULL);

    if( (aNode == aNode->mParent->mRight) &&
        (aNode->mParent == sGP->mLeft) )
    {
        rotateLeft(aNode->mParent);
        sNode = aNode->mLeft;
    }
    else if( (aNode == aNode->mParent->mLeft) &&
             (aNode->mParent == sGP->mRight) )
    {
        rotateRight(aNode->mParent);
        sNode = aNode->mRight;
    }
    else
    {
        sNode = aNode;
    }

    adjustInsertCase5(sNode);
}

/**
 * insert�ÿ� ���� �ұ����� �ذ��Ѵ� : case 5
 * �ű� ��� N�� �������̴�
 * N�� ��İ� ĥ�ϰ� �Ҿƹ��� ��� GP�� ������ ĥ�Ѵ�
 * ���� GP�� ȸ������ N�� �θ��� P��
 * GP�� N�� �θ� �ǰ� �Ѵ�
 * ���� P�� N, GP, P �� ������ ������ ����̴�
 * Before : GP(?)->P (?)->N (R)
 *               ->U (B)
 * After  :  P(B)->GP(R)->U (B)
 *               ->N (R)
 *
 * @param aNode : �ұ����� �ذ��� ��� ������
 */
void iddTRBTree::adjustInsertCase5(iddTRBTree::node* aNode)
{
    iddTRBTree::node* sGP = getGrandParent(aNode);
    IDE_DASSERT(sGP != NULL);

    aNode->mParent->mColor = IDD_BLACK;
    sGP->mColor = IDD_RED;

    if(aNode == aNode->mParent->mLeft)
    {
        rotateRight(sGP);
    }
    else
    {
        rotateLeft(sGP);
    }
}

/**
 * aNode�� �߽����� ���� ȸ��
 */
void iddTRBTree::rotateLeft(iddTRBTree::node* aNode)
{
    iddTRBTree::node* sChild  = aNode->mRight;
    iddTRBTree::node* sParent = aNode->mParent;

    if( sChild != NULL )
    {
        if( sChild->mLeft != NULL )
        {
            sChild->mLeft->mParent = aNode;
        }

        aNode->mRight = sChild->mLeft;
        aNode->setParent(sChild);
        sChild->mLeft  = aNode;
        sChild->mParent = sParent;

        if( sParent != NULL )
        {
            if( sParent->mLeft == aNode)
            {
                sParent->mLeft = sChild;
            }
            else
            {
                sParent->mRight = sChild;
            }
        }
        else
        {
            mRoot = sChild;
        }
    }
}

/**
 * aNode�� �߽����� ������ ȸ��
 */
void iddTRBTree::rotateRight(iddTRBTree::node* aNode)
{
    iddTRBTree::node* sChild  = aNode->mLeft;
    iddTRBTree::node* sParent = aNode->mParent;

    if( sChild != NULL )
    {
        if( sChild->mRight != NULL )
        {
            sChild->mRight->mParent = aNode;
        }

        aNode->mLeft = sChild->mRight;
        aNode->mParent = sChild;
        sChild->mRight  = aNode;
        sChild->mParent = sParent;

        if( sParent != NULL )
        {
            if( sParent->mRight == aNode)
            {
                sParent->mRight = sChild;
            }
            else
            {
                sParent->mLeft = sChild;
            }
        }
        else
        {
            mRoot = sChild;
        }
    }
}

/**
 * aNode(D)�� Ʈ������ ��� �޸𸮸� �����ϰ� �ұ����� �ؼ��Ѵ�
 * 1 D�� �ڽĳ�尡 ��� NULL�̶��
 * 1-1 D�� �����
 * 1-2 T<-D �ϰ� 5
 * 2 D�� �ڽĳ�尡 �ϳ����
 * 2-1 D�� ����� �ڽ� ���(C)�� ��ü�Ѵ�
 * 2-2 A<-C, T<-D
 * 2-3 4 ����
 * 3 D�� �ڽ��� ���̶�� D�� successor S�� ã�Ƴ���
 * 3-1 S�� Key, Data�� D�� ����� �� �� D�� ������ �ٲ��� �ʴ´�
 * 3-2 S�� ���� �ڽ��� �׻� NULL�̴� S�� ������ �ڽ�(C)�� S�� ��ü�Ѵ�(A)
 * 3-3 A<-C, T<-S
 * 3-4 4 ����
 * 4 A�� ���� �ұ����� �ؼ��Ѵ�
 * 5 T�� �����Ѵ�
 * 6 ��
 *
 * @param aNode : ������ ���.
 * @return IDE_SUCCESS
 *         IDE_FAILRE �޸� ���� ���н�
 */
IDE_RC iddTRBTree::removeNode(iddTRBTree::node* aNode)
{
    iddTRBTree::node* sSuccessor;
    iddTRBTree::node* sTarget;
    iddTRBTree::node* sAdjust;

    IDE_DASSERT(aNode != NULL);

    if(aNode->mLeft == NULL && aNode->mRight == NULL)
    {
        if(aNode->mParent == NULL)
        {
            mRoot = NULL;
        }
        else
        {
            unlinkNode(aNode);
        }

        sTarget = aNode;
    }
    else
    {
        if(aNode->mRight == NULL)
        {
            unlinkNode(aNode, aNode->mLeft);
            sTarget = aNode;
            sAdjust = aNode->mLeft;
        }
        else if(aNode->mLeft == NULL)
        {
            unlinkNode(aNode, aNode->mRight);
            sTarget = aNode;
            sAdjust = aNode->mRight;
        }
        else
        {
            sSuccessor = findSuccessor(aNode);
            IDE_DASSERT(sSuccessor != NULL);
            IDE_DASSERT(sSuccessor->mLeft == NULL);
            replaceNode(aNode, sSuccessor);
            unlinkNode(sSuccessor, sSuccessor->mRight);

            sTarget = sSuccessor;
            sAdjust = sSuccessor->mRight;
        }

        if(sTarget->getColor() == IDD_BLACK)
        {
            adjustRemove(sAdjust);
        }
    }

    if(mRoot != NULL)
    {
        mRoot->setColor(IDD_BLACK);
    }

    sTarget->getPrev()->setNext(sTarget->getNext());
    sTarget->getNext()->setPrev(sTarget->getPrev());
    IDE_TEST( freeNode(sTarget) != IDE_SUCCESS );
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    return IDE_FAILURE;
}

/**
 * aNode�� �� �ڼ� ����� �޸𸮸� ��� �����Ѵ�
 *
 * @param aNode : ������ ���.
 * @return IDE_SUCCESS
 *         IDE_FAILRE �޸� ���� ���н�
 */
IDE_RC iddTRBTree::freeAllNodes(iddTRBTree::node* aNode)
{
    if(aNode != NULL)
    {
        /* BUGBUG iteration���� �ٲ� ����� �����غ��� */
        IDE_TEST( freeAllNodes(aNode->mLeft) != IDE_SUCCESS );
        IDE_TEST( freeAllNodes(aNode->mRight) != IDE_SUCCESS );
        IDE_TEST( freeNode(aNode) != IDE_SUCCESS );
    }
    else
    {
        /* fall through */
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    return IDE_FAILURE;
}

/**
 * aOldNode�� ����� �� ��ġ�� aNewNode�� �ִ´�
 *
 * @param aOldNode : ������ ���.
 * @param aNewNode : �ű� ���.
 */
void iddTRBTree::unlinkNode(const iddTRBTree::node* aOldNode,
                           iddTRBTree::node*       aNewNode)
{
    iddTRBTree::node* sParent;
    IDE_DASSERT(aOldNode != NULL);

    sParent = aOldNode->mParent;
    if(sParent == NULL)
    {
        /* this is root */
        mRoot = aNewNode;
    }
    else
    {
        if(sParent->mLeft == aOldNode)
        {
            sParent->mLeft = aNewNode;
        }
        else
        {
            IDE_DASSERT(sParent->mRight == aOldNode);
            sParent->mRight = aNewNode;
        }
    }

    if(aNewNode != NULL)
    {
        aNewNode->mParent = sParent;

        if(aNewNode == aOldNode->mLeft)
        {
            IDE_DASSERT(aOldNode->mRight == NULL);
        }
        else if(aNewNode == aOldNode->mRight)
        {
            IDE_DASSERT(aOldNode->mLeft == NULL);
        }
        else
        {
            aNewNode->mLeft   = aOldNode->mLeft;
            aNewNode->mRight  = aOldNode->mRight;
            aNewNode->mLeft->setParent(aNewNode);
            aNewNode->mRight->setParent(aNewNode);
        }
    }
    else
    {
        /* fall through */
    }
}

/**
 * ��� ��ü
 * aSrcNode�� Key�� Data�� aDestNode�� �����
 * �� �� aSrcNode�� ������ ��������� �ʴ´�
 *
 * @param aDestNode : ��� ���
 * @param aSrcNode : ���� ���
 */
void iddTRBTree::replaceNode(iddTRBTree::node* aDestNode,
                            iddTRBTree::node* aSrcNode)
{
    IDE_DASSERT(aDestNode != NULL);
    IDE_DASSERT(aSrcNode != NULL);

    idlOS::memcpy(aDestNode->mKey, aSrcNode->mKey, mKeyLength);
    aDestNode->mData = aSrcNode->mData;
}

/**
 * ��带 ������ �� �߻��� �ұ����� �ؼ��Ѵ� : case 1
 * aNode�� ��Ʈ�̸� �׳� ��İ� ĥ�ϰ� ��
 * aNode�� ��Ʈ�� �ƴϸ� case 2�� �����Ѵ�.
 *
 * @param aNode : ������ ���� ���.
 */
void iddTRBTree::adjustRemove(iddTRBTree::node* aNode)
{
    if(aNode != NULL)
    {
        if(aNode->getColor() == IDD_RED)
        {
            aNode->setColor(IDD_BLACK);
        }
        else if(aNode != mRoot)
        {
            adjustRemoveCases(aNode);
        }
    }
    else
    {
        /* return; */
    }
}

/**
 * ��带 ������ �� �߻��� �ұ����� �ؼ��Ѵ� : case 2
 * �ڼ��� ������ �˰��� �������� �����ϼ���
 *
 * @param aNode : ������ ���� ���.
 */
void iddTRBTree::adjustRemoveCases(iddTRBTree::node* aNode)
{
    idBool              sDone = ID_FALSE;
    iddTRBTree::color    sColor;
    iddTRBTree::node*    sNode = aNode;
    iddTRBTree::node*    sSibling = getSibling(sNode);
    IDE_DASSERT(aNode->getColor() == IDD_BLACK);

    if(sSibling == NULL)
    {
        /* just return */
    }
    else
    {
        do
        {
            if( sSibling->getColor() == IDD_RED )
            {
                sNode->mParent->setColor(IDD_RED);
                sSibling->setColor(IDD_BLACK);
                if( sNode->isLeftChild() == ID_TRUE )
                {
                    rotateLeft(sNode->mParent);
                }
                else
                {
                    rotateRight(sNode->mParent);
                }

                sSibling = getSibling(sNode);
                if( sSibling == NULL )
                {
                    break;
                }
            }

            if( (sSibling->getColor() == IDD_BLACK)         &&
                (sSibling->mLeft->getColor() == IDD_BLACK)  &&
                (sSibling->mRight->getColor() == IDD_BLACK) )
            {
                sSibling->setColor(IDD_RED);
                sNode = sNode->mParent;

                if( sNode->getColor() == IDD_RED )
                {
                    sNode->setColor(IDD_BLACK);
                    sDone = ID_TRUE;
                }
                else
                {
                    /* sDone = ID_FALSE; */
                }
            }
            else
            {
                if( sNode->isLeftChild() == ID_TRUE )
                {
                    if( (sSibling->getColor() == IDD_BLACK)         &&
                        (sSibling->mLeft->getColor() == IDD_RED)    &&
                        (sSibling->mRight->getColor() == IDD_BLACK) )
                    {
                        sSibling->mLeft->setColor(IDD_BLACK);
                        sSibling->setColor(IDD_RED);
                        rotateRight(sSibling);

                        sSibling = getSibling(sNode);
                    }

                    if( (sSibling->getColor() == IDD_BLACK) &&
                        (sSibling->mRight->getColor() == IDD_RED) )
                    {
                        sColor = sNode->mParent->getColor();
                        sNode->mParent->setColor(sSibling->getColor());
                        sSibling->setColor(sColor);
                        rotateLeft(sNode->mParent);

                        sDone = ID_TRUE;
                    }
                }
                else
                {
                    if( (sSibling->getColor() == IDD_BLACK)         &&
                        (sSibling->mRight->getColor() == IDD_RED)    &&
                        (sSibling->mLeft->getColor() == IDD_BLACK) )
                    {
                        sSibling->mRight->setColor(IDD_BLACK);
                        sSibling->setColor(IDD_RED);
                        rotateLeft(sSibling);

                        sSibling = getSibling(sNode);
                    }

                    if( (sSibling->getColor() == IDD_BLACK) &&
                        (sSibling->mLeft->getColor() == IDD_RED) )
                    {
                        sColor = sNode->mParent->getColor();
                        sNode->mParent->setColor(sSibling->getColor());
                        sSibling->setColor(sColor);
                        rotateRight(sNode->mParent);

                        sDone = ID_TRUE;
                    }
                }
            }
        } while( (sNode != mRoot) && (sDone == ID_FALSE) );
    }
}

/**
 * aNode�� successor�� ã�´�
 * successor�� aNode�� ������ �ڽ��� ���� ���� �ļ��̴�
 *
 * @param aNode : ã�� ���
 * @return successor ��� ������ NULL�� �����Ѵ�
 */
iddTRBTree::node* iddTRBTree::findSuccessor(const iddTRBTree::node* aNode) const
{
    iddTRBTree::node* sPrev;
    iddTRBTree::node* sNode = aNode->mRight;

    sPrev = sNode;
    while(sNode != NULL)
    {
        sPrev = sNode;
        sNode = sNode->mLeft;
    }
    sNode = sPrev;

    return sPrev;
}

#if defined(DEBUG)

void iddTRBTree::dumpNode(iddTRBTree::node* aNode, ideLogEntry& aLog) const
{
    if(aNode == NULL)
    {
        aLog.append("NULL[BLACK]");
    }
    else
    {
        aLog.appendFormat("%p[%s]", aNode,
                aNode->getColor() == IDD_BLACK?"IDD_BLACK":"IDD_RED");
    }
}

SInt iddTRBTree::validateNode(iddTRBTree::node* aNode, SInt aLevel) const
{
    SInt sRet;
    SInt i;

    if(aNode != NULL)
    {
        ideLogEntry sLog(IDE_MISC_0);
        for(i = 0; i < aLevel; i++)
        {
            sLog.append(" ");
        }
        dumpNode(aNode, sLog);
        sLog.append("->");
        dumpNode(aNode->mLeft, sLog);
        sLog.append("/");
        dumpNode(aNode->mRight, sLog);

        if(aNode->getColor() == IDD_RED)
        {
            if(aNode->mLeft->getColor() == IDD_RED)
            {
                sLog.append(" invalid(left)!");
            }
            if(aNode->mRight->getColor() == IDD_RED)
            {
                sLog.append(" invalid(right)!");
            }
        }

        sLog.write();

        sRet = 1;
        sRet += validateNode(aNode->mLeft, aLevel + 1);
        sRet += validateNode(aNode->mRight, aLevel + 1);
    }
    else
    {
        sRet = 0;
    }

    return sRet;
}

SInt iddTRBTree::validate() const
{
    return validateNode(mRoot, 0);
}

#endif

/**
 * ���� RBTree �ν��Ͻ��� ������� ����
 *
 * @param aStat : ������� ������ ����ü
 */
void iddTRBTree::fillStat(iddRBHashStat* aStat)
{
	aStat->mKeyLength       = mKeyLength;
    aStat->mCount           = mCount;
	aStat->mSearchCount     = mSearchCount;
    aStat->mInsertLeft      = mInsertLeft;
    aStat->mInsertRight     = mInsertRight;
}

/**
 * ���ü� ���� ����� �ִ� Red-Black Tree�� �ʱ�ȭ�Ѵ�.
 *
 * @param aIndex : ���������� ����� �޸� �Ҵ� �ε���
 * @param aKeyLength : Ű ũ��(bytes)
 * @param aCompFunc : �񱳿� �Լ�.
 *                    SInt(const void* aKey1, const void* aKey2) �����̸�
 *                    aKey1 >  aKey2�̸� 1 �̻���,
 *                    aKey1 == aKey2�̸� 0 ��
 *                    aKey1 < aKey2�̸� -1 ���ϸ� �����ؾ� �Ѵ�.
 * @param aUseLatch : ��ġ ��� ����(ID_TRUE by default)
 * @param aLatchType : ��ġ ����. �������� ������ LATCH_TYPE
 *                     ������Ƽ�� ������.
 * @return IDE_SUCCESS
 *         IDE_FAILURE
 */
IDE_RC iddTRBLatchTree::initialize(const iduMemoryClientIndex    aIndex,
                                  const ULong                   aKeyLength,
                                  const iddCompFunc            aCompFunc,
                                  const idBool                  aUseLatch,
                                  const iduLatchType            aLatchType)
{
    iduLatchType sLatchType;

    mUseLatch = aUseLatch;

    if( mUseLatch == ID_TRUE )
    {
        sLatchType = (aLatchType == IDU_LATCH_TYPE_MAX)?
            (iduLatchType)iduProperty::getLatchType() :
            aLatchType;

        IDE_TEST( mLatch.initialize((SChar*)"Latch4RBTree", sLatchType)
                  != IDE_SUCCESS );
    }
    else
    {
        /* do nothing */
    }

    IDE_TEST( iddTRBTree::initialize(aIndex, aKeyLength, aCompFunc)
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    return IDE_FAILURE;
}

/**
 * ���� �� ȹ��
 * @return IDE_SUCCESS
 *         IDE_FAILURE
 */
IDE_RC iddTRBLatchTree::lockRead(void)
{
    if( mUseLatch == ID_TRUE )
    {
        IDE_TEST( mLatch.lockRead(NULL, NULL) != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    return IDE_FAILURE;
}

/**
 * ���� �� ȹ��
 * @return IDE_SUCCESS
 *         IDE_FAILURE
 */
IDE_RC iddTRBLatchTree::lockWrite(void)
{
    if( mUseLatch == ID_TRUE )
    {
        IDE_TEST( mLatch.lockWrite(NULL, NULL) != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    return IDE_FAILURE;
}

/**
 * �� ����
 * @return IDE_SUCCESS
 *         IDE_FAILURE
 */
IDE_RC iddTRBLatchTree::unlock(void)
{
    if( mUseLatch == ID_TRUE )
    {
        IDE_TEST( mLatch.unlock() != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    return IDE_FAILURE;
}

/**
 * ���� ���� ȹ���ϰ� Ű/�����͸� �߰��Ѵ�
 * @param aKey : Ű
 * @param aData : ���� ���ϰ� �ִ� ������
 * @return IDE_SUCCESS
 *         IDE_FAILURE �ߺ� ���� �ְų� �� ȹ��, �޸� �Ҵ翡 �������� ��
 */
IDE_RC iddTRBLatchTree::insert(const void* aKey, void* aData)
{
    IDE_TEST      ( lockWrite() != IDE_SUCCESS );
    IDE_TEST_RAISE( iddTRBTree::insert(aKey, aData) != IDE_SUCCESS,
                    INSERTFAILED );
    IDE_TEST      ( unlock() != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION( INSERTFAILED )
    {
        IDE_ASSERT( unlock() == IDE_SUCCESS );
    }

    IDE_EXCEPTION_END;
    return IDE_FAILURE;
}

/**
 * ���� ���� ȹ���ϰ� Ű/�����͸� �˻��Ѵ�
 * @param aKey : Ű
 * @param aData : ã�Ƴ� ���� ������ ������
 * @return IDE_SUCCESS
 *         IDE_FAILURE �ߺ� ���� �ְų� �� ȹ��, �޸� �Ҵ翡 �������� ��
 */
IDE_RC iddTRBLatchTree::search(const void* aKey, void** aData)
{
    IDE_TEST( lockRead() != IDE_SUCCESS );
    (void)iddTRBTree::search(aKey, aData);
    IDE_TEST( unlock() != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    return IDE_FAILURE;
}

/**
 * ���� ���� ȹ���ϰ� aKey�� �ش��ϴ� �����͸� aNewData�� ��ü�Ѵ�
 * ������ �����ʹ� aOldData�� �Ƿ����´�
 *
 * @param aKey : Ű
 * @param aNewData : �� ��
 * @param aOldData : ���� �����͸� ������ ������. NULL�� �� �� �ִ�.
 * @return IDE_SUCCESS ���� ã���� ��
 *         IDE_FAILURE ���� ���� ��
 */
IDE_RC iddTRBLatchTree::update(const void* aKey, void* aNewData, void** aOldData)
{
    IDE_TEST      ( lockWrite() != IDE_SUCCESS );
    IDE_TEST_RAISE( iddTRBTree::update(aKey, aNewData, aOldData) != IDE_SUCCESS,
                    UPDATEFAILED );
    IDE_TEST      ( unlock() != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION( UPDATEFAILED )
    {
        IDE_ASSERT( unlock() == IDE_SUCCESS );
    }
    IDE_EXCEPTION_END;
    return IDE_FAILURE;
}

/**
 * ���� ���� ȹ���ϰ� aKey�� �ش��ϴ� Ű/�����͸� �����Ѵ�
 * ������ �����ʹ� aOldData�� �Ƿ����´�
 *
 * @param aKey : Ű
 * @param aNewData : �� ��
 * @param aOldData : ���� �����͸� ������ ������. NULL�� �� �� �ִ�.
 * @return IDE_SUCCESS ���� ã���� ��
 *         IDE_FAILURE ���� ���� ��
 */
IDE_RC iddTRBLatchTree::remove(const void* aKey, void** aData)
{
    IDE_TEST      ( lockWrite() != IDE_SUCCESS );
    IDE_TEST_RAISE( iddTRBTree::remove(aKey, aData) != IDE_SUCCESS,
                    REMOVEFAILED );
    IDE_TEST      ( unlock() != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION( REMOVEFAILED )
    {
        IDE_ASSERT( unlock() == IDE_SUCCESS );
    }
    IDE_EXCEPTION_END;
    return IDE_FAILURE;
}

