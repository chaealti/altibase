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
 * $Id: qsxAvl.cpp 82075 2018-01-17 06:39:52Z jina.kim $
 **********************************************************************/

#include <qsxAvl.h>
#include <qsxUtil.h>
#include <qsxArray.h>

#define CMPDIR( sCmp ) ( ( sCmp < 0 ) ? AVL_RIGHT : AVL_LEFT )
#define REVERSEDIR( sDir ) ( ( sDir == AVL_LEFT ) ? AVL_RIGHT : AVL_LEFT )

/***********************************************************************
 * PUBLIC METHODS
 **********************************************************************/

void qsxAvl::initAvlTree( qsxAvlTree    * aAvlTree,
                          mtcColumn     * aKeyColumn,
                          mtcColumn     * aDataColumn,
                          idvSQL        * aStatistics )
{
/***********************************************************************
 *
 * Description : PROJ-1075 avl-tree�� �ʱ�ȭ
 *
 * Implementation :
 *         (1) memory�����ڸ� �ʱ�ȭ.
 *         (1.1) �Ҵ��� chunk�� ũ�� �� dataoffset�� ����Ѵ�.
 *         (1.2) memory�����ڸ� chunkũ��� �Ҵ�޵��� �ʱ�ȭ
 *         (2) �÷� ���� �� timeoutó���� ���� ������ ����
 *
 ***********************************************************************/

    SInt sRowSize;
    SInt sDataOffset;
    
    _getRowSizeAndDataOffset( aKeyColumn, aDataColumn, &sRowSize, &sDataOffset );

    aAvlTree->keyCol     = aKeyColumn;
    aAvlTree->dataCol    = aDataColumn;
    aAvlTree->dataOffset = sDataOffset;
    aAvlTree->rowSize    = sRowSize;
    aAvlTree->root       = NULL;
    aAvlTree->nodeCount  = 0;
    aAvlTree->refCount   = 1;
    aAvlTree->statistics = aStatistics;
    aAvlTree->compare    =
        aKeyColumn->module->keyCompare[MTD_COMPARE_MTDVAL_MTDVAL]
                                      [MTD_COMPARE_ASCENDING];
}


IDE_RC qsxAvl::deleteAll( qsxAvlTree * aAvlTree )
{
/***********************************************************************
 *
 * Description : PROJ-1075 avl-tree���� ��� node ����.
 *
 * Implementation :
 *         (1) tree root �� left���� rotation�� �Ͽ�
 *             left child node�� ���� root�� ����.
 *         (2) root�� child�� ���ʿ� ���ٸ�
 *             root�� ����.
 *
 *       10
 *     /    \
 *    8     12
 *     \   /  \
 *      9 11  13
 * 
 * deleteAll.
 * 
 * currNode : 10
 * saveNode : 8
 * 
 * rotate for empty-left-child root 8.
 * 
 *      8
 *       \
 *       10
 *      /  \
 *     9   12
 *        /  \
 *       11  13
 * 
 * currNode : 8
 * saveNode : 10
 * 
 * delete currNode 8.
 * 
 *       10
 *      /  \
 *     9   12
 *        /  \
 *       11  13
 * 
 * currNode : 10
 * saveNode : 9
 * 
 * rotate for empty-left-child root 9.
 * 
 *      9
 *       \
 *       10
 *         \
 *         12
 *        /  \
 *       11  13
 * 
 * currNode : 9
 * saveNode : 10
 * 
 * delete currNode 9.
 * 
 *       10
 *         \
 *         12
 *        /  \
 *       11  13
 * 
 * currNode : 10
 * 
 * 10 is empty-left-child root.
 * delete currNode 10.
 * 
 *         12
 *        /  \
 *       11  13
 * 
 * currNode : 12
 * saveNode : 11
 * 
 * rotate for empty-left-child root 11.
 * 
 *       11
 *         \
 *         12
 *           \
 *           13
 * 
 * currNode : 11
 * saveNode : 12
 * 
 * delete currNode 11.
 * 
 *         12
 *           \
 *           13
 * 
 * currNode : 12
 * 
 * 12 is empty-left-child root.
 * delete currNode 12.
 * 
 *           13
 * 
 * currNode : 13
 * 
 * 13 is empty-left-child root
 * delete currNode 13.
 * 
 * no data found. finished.
 *
 ***********************************************************************/

    qsxAvlNode * sCurrNode;
    qsxAvlNode * sSaveNode;

    sCurrNode = aAvlTree->root;

    while( sCurrNode != NULL )
    {
        if( sCurrNode->link[AVL_LEFT] == NULL )
        {
            sSaveNode = sCurrNode->link[AVL_RIGHT];

            IDE_TEST( _deleteNode( aAvlTree,
                                   sCurrNode )
                      != IDE_SUCCESS );
            aAvlTree->nodeCount--;
        }
        else
        {
            sSaveNode = sCurrNode->link[AVL_LEFT];
            sCurrNode->link[AVL_LEFT] = sSaveNode->link[AVL_RIGHT];
            sSaveNode->link[AVL_RIGHT] = sCurrNode;
        }

        sCurrNode = sSaveNode;
    }

    aAvlTree->root = NULL;
    
    IDE_DASSERT( aAvlTree->nodeCount == 0 );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qsxAvl::search( qsxAvlTree * aAvlTree,
                       mtcColumn  * aKeyCol,
                       void       * aKey,
                       void      ** aRowPtr,
                       idBool     * aFound )
{
/***********************************************************************
 *
 * Description : PROJ-1075 key search
 *
 * Implementation :
 *         (1) root���� ����.
 *         (2) ���� key���� compare�ؼ� ������ break�ϰ� rowPtr�� ����.
 *         (3) ���� �۰ų� ũ�� link�� ����.
 *             sCmp���� 0���� ������ ( left child ),
 *             sCmp���� 0���� ũ�� ( right child )
 *
 ***********************************************************************/
#define IDE_FN "qsxAvl::search"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(IDE_FN));

    qsxAvlNode  * sCurrNode = aAvlTree->root;
    SInt          sCmp;
    mtdValueInfo  sValueInfo1;
    mtdValueInfo  sValueInfo2;

    *aFound = ID_FALSE;
        
    while ( sCurrNode != NULL )
    {
        IDE_TEST( iduCheckSessionEvent( aAvlTree->statistics )
                  != IDE_SUCCESS );

        sValueInfo1.column = aAvlTree->keyCol;
        sValueInfo1.value  = sCurrNode->row;
        sValueInfo1.flag   = MTD_OFFSET_USELESS;

        sValueInfo2.column = aKeyCol;
        sValueInfo2.value  = aKey;
        sValueInfo2.flag   = MTD_OFFSET_USELESS;
        
        sCmp = aAvlTree->compare( &sValueInfo1, &sValueInfo2 );
        
        if ( sCmp == 0 )
            break;
        
        sCurrNode = sCurrNode->link[CMPDIR(sCmp)];
    }

    if( sCurrNode == NULL )
    {
        *aRowPtr = NULL;
        *aFound = ID_FALSE;
    }
    else
    {
        *aRowPtr = sCurrNode->row;
        *aFound = ID_TRUE;
    }
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC qsxAvl::insert( qsxAvlTree * aAvlTree,
                       mtcColumn  * aKeyCol,
                       void       * aKey,
                       void      ** aRowPtr )
{
/***********************************************************************
 *
 * Description : PROJ-1075 insert
 *
 * Implementation :
 *         (1) root�� parent���� ����.
 *         (2) �ش� Ű���� ���Ե� ��ġ�� �˻�
 *         (2.1) �˻� path�� ���󰡸鼭 balance �������� �ʿ��� node �˻�
 *         (3) new node �� insert
 *         (4) balance ���� �ٽ� ����
 *         (5) balance�� 2�̻��̸� rotation�� ���� balance�� ������
 *         (6) balance ���� ���� balancing node�� parent��
 *             balance ���� ���� node�� �ִ� �ڸ��� balance ���� ������ node����
 *
 * Ex) 4 insert. (..) : balance
 * 
 *    5(-1)
 *   /    \
 * 2(1)   7(0)
 *    \
 *    3(0)
 * 
 * 
 * phase (1),(2)
 * parent of balancing node ->   5(-1)
 *                              /    \
 *         balancing node -> 2(1)   7(0)
 *                              \
 *                              3(0)
 *                                 \
 *                                  * insert position
 * 
 * phase (3)
 *    5(-1)
 *   /    \
 * 2(1)   7(0)
 *    \
 *    3(0)
 *       \
 *       4(0)
 * 
 * phase (4)  * : need rotation
 *    5(-1)                5(-1)
 *   /    \               /    \
 * 2(1+1) 7(0)     =>  *2(2)   7(0)
 *    \                    \
 *    3(0+1)               3(1)
 *       \                    \
 *       4(0)                 4(0)
 * 
 * phase (5) single-left rotation
 *     5(-1)                  5(-1)
 *    /    \                 /    \
 * *2(2)   7(0)    =>  savepos     7(0)
 *     \
 *     3(1)              3(0) <- new balanced node
 *        \              /  \
 *        4(0)        2(0) 4(0)
 * 
 * phase (6) link new balanced node
 *       5(-1)
 *      /    \
 *    3(0)   7(0)
 *    /  \
 *  2(0) 4(0)
 *
 ***********************************************************************/
#define IDE_FN "qsxAvl::insert"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(IDE_FN));

    qsxAvlNode   sHeadNode;       // parent node of root.
    qsxAvlNode * sCurrBalNode;    // balancing node
    qsxAvlNode * sParentBalNode;  // balancing node�� parent
    qsxAvlNode * sCurrIterNode;   // for iteration
    qsxAvlNode * sNextIterNode;   // for iteration
    qsxAvlNode * sNewNode;        // for new node
    qsxAvlNode * sSavedBalNode;   // before balancing node
    SInt         sDir;            // node direction
    SInt         sCmp;            // compare result
    mtdValueInfo sValueInfo1;
    mtdValueInfo sValueInfo2;
    
    // �ƹ��͵� ���� ���
    if ( aAvlTree->root == NULL )
    {
        // root�� �׳� ����
        IDE_TEST( _makeNode( aAvlTree,
                             aKeyCol,
                             aKey,
                             &aAvlTree->root )
                  != IDE_SUCCESS );
        aAvlTree->nodeCount++;
        *aRowPtr = aAvlTree->root->row;
    }
    else
    {
        // (1)
        // root�� balance ���� ���� �ٲ� ���� ����Ͽ�
        // root�� parent���� ����.
        sParentBalNode = &sHeadNode;
        sParentBalNode->link[AVL_RIGHT] = aAvlTree->root;

        // (2)
        // insert�� �ڸ��� search�ϸ鼭
        // balancing node, balancing node�� parent,
        // node�� ���Ե� ��ġ�� �����Ѵ�.
        for ( sCurrIterNode = sParentBalNode->link[AVL_RIGHT],
                  sCurrBalNode = sCurrIterNode;
              ;
              sCurrIterNode = sNextIterNode )
        {
            IDE_TEST( iduCheckSessionEvent( aAvlTree->statistics )
                      != IDE_SUCCESS );

            sValueInfo1.column = aAvlTree->keyCol;
            sValueInfo1.value  = sCurrIterNode->row;
            sValueInfo1.flag   = MTD_OFFSET_USELESS;

            sValueInfo2.column = aAvlTree->keyCol;
            sValueInfo2.value  = aKey;
            sValueInfo2.flag   = MTD_OFFSET_USELESS;

            sCmp = aAvlTree->compare( &sValueInfo1,
                                      &sValueInfo2 );
            
            IDE_TEST_RAISE( sCmp == 0, err_dup_key );
            
            sDir = CMPDIR( sCmp );
            sNextIterNode = sCurrIterNode->link[sDir];

            // ���̻� ã�� ���� ���ٸ� ���ڸ��� ���Ե� �ڸ�.
            // sCurrIterNode->link[sDir] �� ���� �Ǿ�� ��.
            if ( sNextIterNode == NULL )
                break;

            // (2.1)
            // ���Ե� node�� parent��
            // balancing node, balancing node�� parent����
            if ( sNextIterNode->balance != 0 ) {
                sParentBalNode = sCurrIterNode;
                sCurrBalNode = sNextIterNode;
            }
        }

        // (3)
        // ���Ե� ��带 ����
        IDE_TEST( _makeNode( aAvlTree,
                             aKeyCol,
                             aKey,
                             &sNewNode )
                  != IDE_SUCCESS );

        // ���Ե� ����� data�� output argument�� ����
        *aRowPtr = sNewNode->row;

        // ���� ����� �ڸ��� ����
        sCurrIterNode->link[sDir] = sNewNode;
        aAvlTree->nodeCount++;

        // (4)
        // balance���� ������
        // balance ���� ��带 ������ �����Ͽ����Ƿ�
        // �ű⼭���� �����Ͽ� ���Ե� ������ ���󰡸鼭
        // top-down������� balance����
        for ( sCurrIterNode = sCurrBalNode;
              sCurrIterNode != sNewNode;
              sCurrIterNode = sCurrIterNode->link[sDir] )
        {
            IDE_TEST( iduCheckSessionEvent( aAvlTree->statistics )
                      != IDE_SUCCESS );
            
            sValueInfo1.column = aAvlTree->keyCol;
            sValueInfo1.value  = sCurrIterNode->row;
            sValueInfo1.flag   = MTD_OFFSET_USELESS;

            sValueInfo2.column = aAvlTree->keyCol;
            sValueInfo2.value  = aKey;
            sValueInfo2.flag   = MTD_OFFSET_USELESS;
            
            sCmp = aAvlTree->compare( &sValueInfo1,
                                      &sValueInfo2 );
            
            sDir = CMPDIR(sCmp);
            sCurrIterNode->balance += ( ( sDir == AVL_LEFT ) ? -1 : 1 );
        }

        // balance���� ���� balancing node�� ���
        sSavedBalNode = sCurrBalNode;

        // (5)
        // balance�� 1�̻��� ��� �������� �ʿ���
        // rotation�� ���� balance�� ������
        if ( abs( sCurrBalNode->balance ) > 1 )
        {
            sValueInfo1.column = aAvlTree->keyCol;
            sValueInfo1.value  = sCurrBalNode->row;
            sValueInfo1.flag   = MTD_OFFSET_USELESS;

            sValueInfo2.column = aAvlTree->keyCol;
            sValueInfo2.value  = aKey;
            sValueInfo2.flag   = MTD_OFFSET_USELESS;
            
            sCmp = aAvlTree->compare( &sValueInfo1,
                                      &sValueInfo2 );
            
            sDir = CMPDIR(sCmp);
            _insert_balance( &sCurrBalNode, sDir );
        }

        // (6)
        // balance���� ���� balance ���� ����� parent��忡 �ٽ� ����
        if ( sSavedBalNode == sHeadNode.link[AVL_RIGHT] )
        {
            // balance ������ ���� root�� �ٲ� ���
            aAvlTree->root = sCurrBalNode;
        }
        else
        {
            // basic case.
            // balance ���� ���� ��尡 ��� �پ��־����� ã�� ����
            // balance ���� ������ ��带 ���ڸ��� �ٽ� ����
            sDir = ( sSavedBalNode == sParentBalNode->link[AVL_RIGHT] ) ? AVL_RIGHT : AVL_LEFT;        
            sParentBalNode->link[sDir] = sCurrBalNode;
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( err_dup_key);
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QSX_DUP_VAL_ON_INDEX));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}


IDE_RC qsxAvl::deleteKey( qsxAvlTree * aAvlTree,
                          mtcColumn  * aKeyCol,
                          void       * aKey,
                          idBool     * aDeleted )
{
/***********************************************************************
 *
 * Description : PROJ-1075 delete
 *
 * Implementation :
 *         (1) root���� ����.
 *         (2) �ش� Ű���� ������ ��ġ�� �˻�
 *         (2.1) �˻� path�� ����. path direction�� �Բ� ����
 *         (3) node ����
 *         (3.1) node�� child�� ���ų� �ϳ��� ���
 *               child�� ���� node�� parent�� �����ϰ� node����
 *         (3.2) node�� child�� �Ѵ� �ִ� ���
 *               �ش� node�������� ū node�� ã�Ƽ�
 *               �����͸� copy�� ���� ������� ����.
 *               �ش� node�� ����� ���� �ƴ϶� �ش� node��������
 *               ū node�� ������.(delete by copy algorithm)
 *         (4) balance������
 *             ������ �ξ��� search path�� ���󰡸鼭
 *             balance����. ���� balance�� 2�̻��̶�� rotation���� ������.
 *             +-1�ΰ�� ������ �ʿ䰡 ����
 *             0�� ��� ������ �Ϸ�� ���ų� �ƹ��͵� �� �ʿ䰡 ���� �����.
 *
 * Ex) 2 delete. (..) : balance
 * 
 * phase (1)
 * 
 * start=> 6(-1)
 *      /        \
 *    2(1)       8(0)
 *   /   \      /   \
 * 1(0) 4(1)  7(0)  9(0)
 *         \
 *         5(0)
 * 
 * 
 * phase (2)
 * 
 * path    : | 6 |   |
 * pathDir : | L |   |
 * top     :       ^
 * 
 *         6(-1)
 *       /       \
 *   *2(1)       8(0)
 *   /   \      /   \
 * 1(0) 4(1)  7(0)  9(0)
 *         \
 *         5(0)
 * * : current node
 * 
 * 
 * phase (3) -> (3.2) current have 2 childs
 * 
 * path    : | 6 | 2 |   |
 * pathDir : | L | R |   |
 * top     :           ^
 * 
 *         6(-1)
 *       /       \
 *   *2(1)       8(0)
 *   /   \      /   \
 * 1(0) #4(1) 7(0)  9(0)
 *         \
 *         5(0)
 * * : current node
 * # : heir node
 * 
 * 
 * current node := heir node (row copy)
 * 
 *         6(-1)
 *       /       \
 *   *4(1)       8(0)
 *   /   \      /   \
 * 1(0) #4(1) 7(0)  9(0)
 *         \
 *         5(0)
 * * : current node
 * # : heir node
 * 
 * 
 * link heir node->link[AVL_RIGHT]
 * 
 *         6(-1)
 *       /       \
 *   *4(1)       8(0)
 *   /   \      /   \
 * 1(0) +5(0) 7(0)  9(0)
 *         
 *      #4(1)  
 *         \
 *        +5(0)
 * * : current node
 * # : heir node
 * + : new linked node(heir node->link[AVL_RIGHT])
 * 
 * delete heir node.
 * 
 *         6(-1)
 *       /       \
 *   *4(1)       8(0)
 *   /   \      /   \
 * 1(0) +5(0) 7(0)  9(0)
 * 
 * 
 * phase (4) re-balance
 * 
 * path    : | 6 | 4 |   |
 * pathDir : | L | R |   |
 * top     :       ^      if pathDir[top] is R then balance - 1
 * 
 *         6(-1)
 *       /       \
 *   *4(1-1)     8(0)
 *   /   \      /   \
 * 1(0) +5(0) 7(0)  9(0)
 * 
 * path    : | 6 | 4 |   |
 * pathDir : | L | R |   |
 * top     :   ^          if pathDir[top] is L then balance + 1
 * 
 *        6(-1+1)
 *       /       \
 *   *4(1-1)     8(0)
 *   /   \      /   \
 * 1(0) +5(0) 7(0)  9(0)
 * 
 * finish! 
 * 
 *         6(0)
 *      /       \
 *   *4(0)      8(0)
 *   /   \      /   \
 * 1(0) +5(0) 7(0)  9(0)
 * 
 ***********************************************************************/
#define IDE_FN "qsxAvl::deleteKey"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(IDE_FN));

    qsxAvlNode * sCurrNode;
    qsxAvlNode * sHeirNode;
    qsxAvlNode * sPath[QSX_AVL_HEIGHT_LIMIT];
    
    SInt         sPathDir[QSX_AVL_HEIGHT_LIMIT];
    SInt         sCmp;
    SInt         sDir;
    SInt         sTop = 0;
    idBool       sDone = ID_FALSE;
    mtdValueInfo sValueInfo1;
    mtdValueInfo sValueInfo2;
      

    *aDeleted = ID_FALSE;
    
    if ( aAvlTree->root != NULL )
    {
        // (1) root���� ����
        sCurrNode = aAvlTree->root;

        // (2)
        // delete�� node�� �˻��ϸ鼭
        // path�� ����.
        while(1)
        {
            if ( sCurrNode == NULL )
            {
                IDE_CONT(key_not_found);
            }
            else
            {
                sValueInfo1.column = aAvlTree->keyCol;
                sValueInfo1.value  = sCurrNode->row;
                sValueInfo1.flag   = MTD_OFFSET_USELESS;

                sValueInfo2.column = aKeyCol;
                sValueInfo2.value  = aKey;
                sValueInfo2.flag   = MTD_OFFSET_USELESS;
        
                sCmp = aAvlTree->compare( &sValueInfo1,
                                          &sValueInfo2 );
                
                if ( sCmp == 0 )
                {
                    break;
                }
                else
                {
                    /* Push direction and node onto stack */
                    sPathDir[sTop] = CMPDIR(sCmp);
                    sPath[sTop++] = sCurrNode;
                    
                    sCurrNode = sCurrNode->link[sPathDir[sTop - 1]];
                }
            }
        }

        // (3) node�� ����.
        if ( sCurrNode->link[AVL_LEFT] == NULL || sCurrNode->link[AVL_RIGHT] == NULL )
        {
            // (3.1) child�� �ƿ� ���ų� �ϳ��� �ִ� ���
            sDir = ( sCurrNode->link[AVL_LEFT] == NULL ) ? AVL_RIGHT : AVL_LEFT;

            // ������ node�� child node�� ������ node�� parent�� ����
            if ( sTop != 0 )
            {
                // basic case
                sPath[sTop - 1]->link[sPathDir[sTop - 1]] = sCurrNode->link[sDir];
            }
            else
            {
                // root�� ������ ���
                aAvlTree->root = sCurrNode->link[sDir];
            }

            aAvlTree->nodeCount--;

            // �ش� node�� ����
            IDE_TEST( _deleteNode( aAvlTree,
                                   sCurrNode )
                      != IDE_SUCCESS );
        }
        else
        {
            // (3.2) child�� �ΰ� �پ� �ִ� ���.
            /* Find the inorder successor */
            sHeirNode = sCurrNode->link[AVL_RIGHT];
      
            /* Save this path too */
            sPathDir[sTop] = 1;
            sPath[sTop++] = sCurrNode;

            while ( sHeirNode->link[AVL_LEFT] != NULL )
            {
                sPathDir[sTop] = AVL_LEFT;
                sPath[sTop++] = sHeirNode;
                sHeirNode = sHeirNode->link[AVL_LEFT];
            }

            // row copy
            _copyNodeRow( aAvlTree, sCurrNode, sHeirNode );
            
            // node ����
            sPath[sTop - 1]->link[ ( ( sPath[sTop - 1] == sCurrNode ) ? AVL_RIGHT : AVL_LEFT ) ]  =
                sHeirNode->link[AVL_RIGHT];

            aAvlTree->nodeCount--;

            // �ش� node�� ����
            IDE_TEST( _deleteNode( aAvlTree,
                                   sHeirNode )
                      != IDE_SUCCESS );
        }

        // (4) balance ����.
        while ( ( --sTop >= 0 ) &&
                ( sDone == ID_FALSE ) )
        {
            sPath[sTop]->balance += ( ( sPathDir[sTop] != AVL_LEFT ) ? -1 : 1 );

            // balance���� -1�Ǵ� +1 �̸� �׳� ����.
            // balance�� 2�̻��̸� rotation�� ���� balance ����
            if ( abs( sPath[sTop]->balance ) == 1 )
            {    
                break;
            }
            else if ( abs ( sPath[sTop]->balance ) > 1 )
            {
                _remove_balance( &sPath[sTop], sPathDir[sTop], &sDone );
                
                // parent����
                if ( sTop != 0 )
                {
                    // basic case
                    sPath[sTop - 1]->link[sPathDir[sTop - 1]] = sPath[sTop];
                }
                else
                {
                    // root�� ���
                    aAvlTree->root = sPath[0];
                }
            }
            else
            {
                // Nothing to do.
                // balance�� 0�� ���� �ƹ��� ������ �����ų�,
                // ������ 0�� �� �����.
            }
        }

        *aDeleted = ID_TRUE;
    }
    else
    {
        IDE_CONT(key_not_found);
    }

    IDE_EXCEPTION_CONT(key_not_found);
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC qsxAvl::searchMinMax( qsxAvlTree * aAvlTree,
                             SInt         aDir,
                             void      ** aRowPtr,
                             idBool     * aFound )
{
/***********************************************************************
 *
 * Description : PROJ-1075 min, max �˻�
 *
 * Implementation :
 *          (1) direction�� 0�̸� min, direction�� 1�̸� max
 *          (2) ���� �������� node�� next�� null�� ������ ����
 *          (3) ���� root�� ���ٸ� ����.
 *
 ***********************************************************************/
#define IDE_FN "qsxAvl::searchMinMax"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(IDE_FN));

    qsxAvlNode * sCurrNode;

    sCurrNode = aAvlTree->root;
    *aFound   = ID_FALSE;

    if( sCurrNode != NULL )
    {
        while( sCurrNode->link[aDir] != NULL )
        {
            IDE_TEST( iduCheckSessionEvent( aAvlTree->statistics )
                      != IDE_SUCCESS );
        
            sCurrNode = sCurrNode->link[aDir];
        }

        *aRowPtr = sCurrNode->row;
        *aFound  = ID_TRUE;
    }
    else
    {
        *aRowPtr = NULL;
        *aFound  = ID_FALSE;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC qsxAvl::searchNext( qsxAvlTree * aAvlTree,
                           mtcColumn  * aKeyCol,
                           void       * aKey,
                           SInt         aDir,
                           SInt         aStart,
                           void      ** aRowPtr,
                           idBool     * aFound )
{
/***********************************************************************
 *
 * Description : PROJ-1075 next, priv �˻�
 *
 * Implementation :
 *          (1) �Ϲ� search�� ���� logic������ next, priv�� ���� stack�� ���
 *          (2) �ش� key�� ã���� sFound�� TRUE�� �����ϰ� ����.
 *          (3) ���� current���� �����̰� key�� ã�Ҵٸ�
 *              �ش� ��ġ�� row�� ��ȯ.
 *          (4) ���� current���� ������ �ƴ϶�� ���� ��ġ�� row��ȯ.
 *
 ***********************************************************************/
#define IDE_FN "qsxAvl::searchNext"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(IDE_FN));

    qsxAvlNode * sCurrNode;
    qsxAvlNode * sPath[QSX_AVL_HEIGHT_LIMIT];
    SInt         sCmp;
    SInt         sDir = 0;
    SInt         sTop = 0;
    mtdValueInfo sValueInfo1;
    mtdValueInfo sValueInfo2;
      
    
    *aFound = ID_FALSE;

    if ( aAvlTree->root != NULL )
    {
        // (1) root���� ����
        sCurrNode = aAvlTree->root;

        while ( sCurrNode != NULL )
        {
            sPath[sTop++] = sCurrNode;
            
            IDE_TEST( iduCheckSessionEvent( aAvlTree->statistics )
                      != IDE_SUCCESS );

            sValueInfo1.column = aAvlTree->keyCol;
            sValueInfo1.value  = sCurrNode->row;
            sValueInfo1.flag   = MTD_OFFSET_USELESS;

            sValueInfo2.column = aKeyCol;
            sValueInfo2.value  = aKey;
            sValueInfo2.flag   = MTD_OFFSET_USELESS; 
            
            sCmp = aAvlTree->compare ( &sValueInfo1,
                                       &sValueInfo2 );
        
            if ( sCmp == 0 )
            {
                *aFound = ID_TRUE;
                break;
            }
            else
            {
                sDir = CMPDIR(sCmp);

                // sCurrNode->key < aKey �� right��
                // sCurrNode->key > aKey �� left��
                sCurrNode = sCurrNode->link[sDir];
            }
        }

        if( *aFound == ID_TRUE && aStart == AVL_CURRENT )
        {
            // key�� ���� node�� ã�Ұ�, ���۽����� current�̸�
            // ã�� node�� rowPtr�� �Ѱ��ָ� �ȴ�.
            *aRowPtr = sCurrNode->row;
        }
        else
        {
            if( *aFound == ID_FALSE && sDir != aDir )
            {
                // key�� ���� node�� ��ã�Ұ�,
                // �̵��� ����� ������ search������ �ٸ���
                // ������ path�� �ִ� node�� ����.
                *aRowPtr = sPath[--sTop]->row;
                *aFound = ID_TRUE;
            }
            else
            {
                // key�� ���� node�� ã�Ұų�,
                // �̵��� ����� ������ search������ ���ٸ�
                // next�� �̵�.
                IDE_TEST( _move( sPath,
                                 &sTop,
                                 aDir,
                                 aRowPtr,
                                 aFound )
                          != IDE_SUCCESS );
            }
        }
    }
    else
    {
        *aRowPtr = NULL;
        *aFound = ID_FALSE;
    }
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC qsxAvl::searchNext4DR( qsxAvlTree * aAvlTree,
                              mtcColumn  * aKeyCol,
                              void       * aKey,
                              SInt         aDir,
                              SInt         aStart,
                              void      ** aRowPtr,
                              idBool     * aFound )
{
#define IDE_FN "qsxAvl::searchNext4DR"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(IDE_FN));

    qsxAvlNode * sCurrNode;
    qsxAvlNode * sPath[QSX_AVL_HEIGHT_LIMIT];
    SInt         sCmp;
    SInt         sDir = 0;
    SInt         sTop = 0;
    mtdValueInfo sValueInfo1;
    mtdValueInfo sValueInfo2;
      
    
    *aFound = ID_FALSE;

    if ( aAvlTree->root != NULL )
    {
        // (1) root���� ����
        sCurrNode = aAvlTree->root;

        while ( sCurrNode != NULL )
        {
            sPath[sTop++] = sCurrNode;
            
            IDE_TEST( iduCheckSessionEvent( aAvlTree->statistics )
                      != IDE_SUCCESS );

            sValueInfo1.column = aAvlTree->keyCol;
            sValueInfo1.value  = sCurrNode->row;
            sValueInfo1.flag   = MTD_OFFSET_USELESS;

            sValueInfo2.column = aKeyCol;
            sValueInfo2.value  = aKey;
            sValueInfo2.flag   = MTD_OFFSET_USELESS;
        
            sCmp = aAvlTree->compare ( &sValueInfo1,
                                       &sValueInfo2 );
        
            if ( sCmp == 0 )
            {
                *aFound = ID_TRUE;
                break;
            }
            else
            {
                sDir = CMPDIR(sCmp);

                // sCurrNode->key < aKey �� right��
                // sCurrNode->key > aKey �� left��
                sCurrNode = sCurrNode->link[sDir];
            }
        }

        if( *aFound == ID_TRUE && aStart == AVL_CURRENT )
        {
            // key�� ���� node�� ã�Ұ�, ���۽����� current�̸�
            // ã�� node�� rowPtr�� �Ѱ��ָ� �ȴ�.
            idlOS::memcpy( *aRowPtr,
                           sCurrNode->row,
                           aAvlTree->dataOffset );
        }
        else
        {
            if( *aFound == ID_FALSE && sDir != aDir )
            {
                // key�� ���� node�� ��ã�Ұ�,
                // �̵��� ����� ������ search������ �ٸ���
                // ������ path�� �ִ� node�� ����.
                idlOS::memcpy( *aRowPtr,
                               sPath[--sTop]->row,
                               aAvlTree->dataOffset );
                *aFound = ID_TRUE;
            }
            else
            {
                // key�� ���� node�� ã�Ұų�,
                // �̵��� ����� ������ search������ ���ٸ�
                // next�� �̵�.
                IDE_TEST( _move4DR( sPath,
                                    &sTop,
                                    aDir,
                                    aRowPtr,
                                    aFound,
                                    aAvlTree->dataOffset )
                          != IDE_SUCCESS );
            }
        }
    }
    else
    {
        *aFound = ID_FALSE;
    }
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC qsxAvl::searchNth( qsxAvlTree * aAvlTree,
                          UInt         aIndex,
                          void      ** aRowPtr,
                          idBool     * aFound )
{
/***********************************************************************
 *
 * Description : BUG-41311 table function
 *
 * Implementation :
 *     - aIndex�� 0���� �����Ѵ�.
 *     - key�� ������� n��° element�� ã�´�.
 *
 ***********************************************************************/
#define IDE_FN "qsxAvl::searchNth"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(IDE_FN));

    qsxAvlNode * sCurrNode;
    qsxAvlNode * sPath[QSX_AVL_HEIGHT_LIMIT];
    SInt         sTop = 0;
    void       * sRowPtr = NULL;
    idBool       sFound = ID_FALSE;
    UInt         i = 0;

    sCurrNode = aAvlTree->root;

    // inorder traverse
    while ( ( sTop > 0 ) || ( sCurrNode != NULL ) )
    {
        if ( sCurrNode != NULL )
        {
            sPath[sTop++] = sCurrNode;
            
            sCurrNode = sCurrNode->link[AVL_LEFT];
        }
        else
        {
            sCurrNode = sPath[--sTop];
            
            if ( i == aIndex )
            {
                sRowPtr = sCurrNode->row;
                sFound = ID_TRUE;
                break;
            }
            else
            {
                i++;
            }
            
            sCurrNode = sCurrNode->link[AVL_RIGHT];
        }
    }
    
    *aRowPtr = sRowPtr;
    *aFound = sFound;
    
    return IDE_SUCCESS;

#undef IDE_FN
}

IDE_RC qsxAvl::deleteRange( qsxAvlTree * aAvlTree,
                            mtcColumn  * aKeyMinCol,
                            void       * aKeyMin,
                            mtcColumn  * aKeyMaxCol,
                            void       * aKeyMax,
                            SInt       * aCount )
{
/***********************************************************************
 *
 * Description : PROJ-1075 next, priv �˻�
 *
 * Implementation :
 *          (1) min, max�� �����ϰų� ����(GE, LE) start, end key�� ã�Ƴ���
 *          (2) min�� start���� next�� ���󰡸鼭 �ϳ��� ����
 *          (3) end key���� ������ �ߴ�.
 *
 ***********************************************************************/
#define IDE_FN "qsxAvl::deleteRange"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(IDE_FN));

    void * sStartKey = NULL;
    void * sEndKey   = NULL;
    void * sPrevKey  = NULL;
    void * sCurrKey  = NULL;
    idBool sDeleted;
    idBool sFound;
    SInt   sCmp;
    mtdValueInfo sValueInfo1;
    mtdValueInfo sValueInfo2;
      
  
    //fix BUG-18164 
    IDE_TEST( iduMemMgr::malloc( IDU_MEM_QSN,
                                 aAvlTree->dataOffset,
                                 (void **)&sStartKey ) != IDE_SUCCESS );

    IDE_TEST( iduMemMgr::malloc( IDU_MEM_QSN,
                                 aAvlTree->dataOffset,
                                 (void **)&sEndKey ) != IDE_SUCCESS );

    IDE_TEST( iduMemMgr::malloc( IDU_MEM_QSN,
                                 aAvlTree->dataOffset,
                                 (void **)&sPrevKey ) != IDE_SUCCESS );

    IDE_TEST( iduMemMgr::malloc( IDU_MEM_QSN,
                                 aAvlTree->dataOffset,
                                 (void **)&sCurrKey ) != IDE_SUCCESS );
 
    *aCount = 0;

    sValueInfo1.column = aKeyMinCol;
    sValueInfo1.value  = aKeyMin;
    sValueInfo1.flag   = MTD_OFFSET_USELESS;

    sValueInfo2.column = aKeyMaxCol;
    sValueInfo2.value  = aKeyMax;
    sValueInfo2.flag   = MTD_OFFSET_USELESS;

    sCmp = aAvlTree->compare ( &sValueInfo1,
                               &sValueInfo2 );
    
    IDE_TEST_CONT( sCmp > 0, invalid_range );
    
    // min�� start
    IDE_TEST( searchNext4DR( aAvlTree,
                             aKeyMinCol,
                             aKeyMin,
                             AVL_RIGHT,
                             AVL_CURRENT,
                             &sStartKey,
                             &sFound )
              != IDE_SUCCESS );

    IDE_TEST_CONT( sFound == ID_FALSE, invalid_range );
    
    // max�� start
    IDE_TEST( searchNext4DR( aAvlTree,
                             aKeyMaxCol,
                             aKeyMax,
                             AVL_LEFT,
                             AVL_CURRENT,
                             &sEndKey,
                             &sFound )
              != IDE_SUCCESS );

    IDE_TEST_CONT( sFound == ID_FALSE, invalid_range );
    
    //fix BUG-18164 
    idlOS::memcpy( sPrevKey,
                   sStartKey,
                   aAvlTree->dataOffset );

    while( 1 )
    {
        sValueInfo1.column = aAvlTree->keyCol;
        sValueInfo1.value  = sPrevKey;
        sValueInfo1.flag   = MTD_OFFSET_USELESS;

        sValueInfo2.column = aAvlTree->keyCol;
        sValueInfo2.value  = sEndKey;
        sValueInfo2.flag   = MTD_OFFSET_USELESS;
    
        sCmp = aAvlTree->compare ( &sValueInfo1,
                                   &sValueInfo2 );
        if( sCmp == 0 )
        {
            IDE_TEST( deleteKey( aAvlTree,
                                 aAvlTree->keyCol,
                                 sPrevKey,
                                 &sDeleted )
                      != IDE_SUCCESS );

            // key�� search�ؼ� ����� ���̹Ƿ� �ݵ�� �������� ��.
            IDE_DASSERT( sDeleted == ID_TRUE );
            
            (*aCount)++;
            
            break;
        }
        else
        {
            IDE_TEST( searchNext4DR( aAvlTree,
                                     aAvlTree->keyCol,
                                     sPrevKey,
                                     AVL_RIGHT,
                                     AVL_NEXT,
                                     &sCurrKey,
                                     &sFound )
                      != IDE_SUCCESS );

            IDE_TEST( deleteKey( aAvlTree,
                                 aAvlTree->keyCol,
                                 sPrevKey,
                                 &sDeleted )
                      != IDE_SUCCESS );
            // key�� search�ؼ� ����� ���̹Ƿ� �ݵ�� �������� ��.
            IDE_DASSERT( sDeleted == ID_TRUE );
            
            (*aCount)++;

            if( sFound == ID_FALSE )
            {
                break;
            }
            else
            {
                //fix BUG-18164 
                idlOS::memcpy( sPrevKey,
                               sCurrKey,
                               aAvlTree->dataOffset );
            }
        }
    }

    IDE_EXCEPTION_CONT( invalid_range );

    if( sStartKey != NULL )
    {
        IDE_TEST( iduMemMgr::free( sStartKey ) != IDE_SUCCESS );
        sStartKey = NULL;
    }

    if( sEndKey != NULL )
    {
        IDE_TEST( iduMemMgr::free( sEndKey ) != IDE_SUCCESS );
        sEndKey = NULL;
    }

    if( sPrevKey != NULL )
    {
        IDE_TEST( iduMemMgr::free( sPrevKey ) != IDE_SUCCESS );
        sPrevKey = NULL;
    }

    if( sCurrKey != NULL )
    {
        IDE_TEST( iduMemMgr::free( sCurrKey ) != IDE_SUCCESS );
        sCurrKey = NULL;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if( sStartKey != NULL )
    {
        (void)iduMemMgr::free( sStartKey );
        sStartKey = NULL;
    }

    if( sEndKey != NULL )
    {
        (void)iduMemMgr::free( sEndKey );
        sEndKey = NULL;
    }

    if( sPrevKey != NULL )
    {
        (void)iduMemMgr::free( sPrevKey );
        sPrevKey = NULL;
    }

    if( sCurrKey != NULL )
    {
        (void)iduMemMgr::free( sCurrKey );
        sCurrKey = NULL;
    }

    return IDE_FAILURE;

#undef IDE_FN
}

/***********************************************************************
 * PRIVATE METHODS
 **********************************************************************/

IDE_RC qsxAvl::_move( qsxAvlNode ** aPath,
                      SInt        * aTop,
                      SInt          aDir,
                      void       ** aRowPtr,
                      idBool      * aFound )
{
/***********************************************************************
 *
 * Description : PROJ-1075 ���� ��ġ���� next, priv �̵�
 *
 * Implementation :
 *          (1) �Ʒ��� �������� ���� �ش� node�� direction�� node�� �ִ� ���
 *          (2) ���� �ö󰡴� ���� �ش� node�� direction�� NULL�� ���
 *
 *       10
 *     /    \
 *    8     12
 *     \   /  \
 *      9 11  13
 * 
 * case (1) next of 10, direction : AVL_RIGHT
 * 
 * path : | 10 |
 * top  :         ^
 * 
 * 10(--top)->link[1] is 12.
 * path : | 10 |
 * top  :    ^
 * 
 * find link[0] of 12. result is 11.
 * find link[0] of 11. result is null.
 * break.
 * 
 * next node is 11.
 *
 *
 * case (2) next of 9, direction : AVL_RIGHT
 * 
 * path : | 10 | 8 | 9 |
 * top  :            ^
 * 
 * 9 is last, 8(--top) is current.
 * last(9) is link[1] of 8.
 * 
 * path : | 10 | 8 | 9 |
 * top  :        ^
 * 
 * 
 * 8 is last, 10(--top) is current.
 * last(8) is link[0] of 10.
 * break.
 * 
 * path : | 10 | 8 | 9 |
 * top  :   ^
 * 
 * next node is 10.
 *
 ***********************************************************************/
#define IDE_FN "qsxAvl::_move"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(IDE_FN));

    qsxAvlNode * sCurrNode;
    qsxAvlNode * sLastNode;
    
    sCurrNode = aPath[--(*aTop)]->link[aDir];
    *aFound   = ID_FALSE;
    
    if ( sCurrNode != NULL )
    {
        while ( sCurrNode->link[REVERSEDIR(aDir)] != NULL )
        {
            sCurrNode = sCurrNode->link[REVERSEDIR(aDir)];
            aPath[(*aTop)++] = sCurrNode;
        }
    }
    else
    {
        /* Move to the next branch */        
        do
        {
            if ( *aTop == 0 )
            {
                sCurrNode = NULL;
                break;
            }
            
            sLastNode = aPath[*aTop];

            sCurrNode = aPath[--(*aTop)];
        } while ( sLastNode == aPath[*aTop]->link[aDir] );
    }

    if( sCurrNode != NULL )
    {
        *aRowPtr = sCurrNode->row;
        *aFound  = ID_TRUE;
    }
    else
    {
        *aRowPtr = NULL;
        *aFound  = ID_FALSE;
    }
    
    return IDE_SUCCESS;

#undef IDE_FN
}

IDE_RC qsxAvl::_move4DR( qsxAvlNode ** aPath,
                         SInt        * aTop,
                         SInt          aDir,
                         void       ** aRowPtr,
                         idBool      * aFound,
                         SInt          aKeySize )
{
#define IDE_FN "qsxAvl::_move4DR"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(IDE_FN));

    qsxAvlNode * sCurrNode;
    qsxAvlNode * sLastNode;
    
    sCurrNode = aPath[--(*aTop)]->link[aDir];
    *aFound   = ID_FALSE;
    
    if ( sCurrNode != NULL )
    {
        while ( sCurrNode->link[REVERSEDIR(aDir)] != NULL )
        {
            sCurrNode = sCurrNode->link[REVERSEDIR(aDir)];
            aPath[(*aTop)++] = sCurrNode;
        }
    }
    else
    {
        /* Move to the next branch */        
        do
        {
            if ( *aTop == 0 )
            {
                sCurrNode = NULL;
                break;
            }
            
            sLastNode = aPath[*aTop];

            sCurrNode = aPath[--(*aTop)];
        } while ( sLastNode == aPath[*aTop]->link[aDir] );
    }

    if( sCurrNode != NULL )
    {
        idlOS::memcpy( *aRowPtr,
                       sCurrNode->row,
                       aKeySize );
        *aFound  = ID_TRUE;
    }
    else
    {
        *aFound  = ID_FALSE;
    }
    
    return IDE_SUCCESS;

#undef IDE_FN
}


void qsxAvl::_rotate_single( qsxAvlNode ** aNode, SInt aDir )
{
/***********************************************************************
 *
 * Description : PROJ-1075 node�� single rotate
 *
 * Implementation :
 *          (1) 3�� ��� �� ��� ��带 parent�� �ö���� ȸ��.
 *          (2) ���� �������� ġ��ģ ���. L->L, R->R
 *
 * 1. left rotation. direction is 0.
 * 
 *   1
 *  / \             2
 * A   2     =>    / \
 *    / \         1   3
 *   B   3       / \
 *              A   B
 *
 * 2. right rotation. direction is 1.
 * 
 *     3
 *    / \           2
 *   2   B  =>     / \
 *  / \           1   3
 * 1   A             / \
 *                  A   B
 ***********************************************************************/
#define IDE_FN "qsxAvl::_rotate_single"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(IDE_FN));

    qsxAvlNode * sSaveNode;

    SInt sReverseDir;

    sReverseDir = REVERSEDIR(aDir);
        
    sSaveNode = (*aNode)->link[sReverseDir];

    (*aNode)->link[sReverseDir] = sSaveNode->link[aDir];

    sSaveNode->link[aDir] = *aNode;

    *aNode = sSaveNode;

    return;

#undef IDE_FN
}

void qsxAvl::_rotate_double(qsxAvlNode ** aNode, SInt aDir )
{
/***********************************************************************
 *
 * Description : PROJ-1075 node�� double rotate
 *
 * Implementation :
 *          (1) 3�� ��� �� ��� ��带 parent�� �ö���� �ι� ȸ��.
 *          (2) ���� �������� ġ��ģ ���� �ƴ϶� L->R, R->L�� ����.
 *
 * 1. right-left rotation
 *
 *   1                 1
 *  / \               / \                   2
 * A   3       =>    A   2       =>       /   \
 *    / \               / \              1     3
 *   2   D             B   3            / \   / \
 *  / \                   / \          A   B C   D
 * B   C                 C   D
 *
 * 2. left-right rotation
 *
 *     3                 3
 *    / \               / \                 2
 *   1   D     =>      2   D       =>     /   \
 *  / \               / \                1     3
 * A   2             1   C              / \   / \
 *    / \           / \                A   B C   D
 *   B   C         A   B
 *
 ***********************************************************************/
#define IDE_FN "qsxAvl::_rotate_double"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(IDE_FN));

    qsxAvlNode * sSaveNode;
    SInt sReverseDir;

    sReverseDir = REVERSEDIR(aDir);

    sSaveNode = (*aNode)->link[sReverseDir]->link[aDir];

    (*aNode)->link[sReverseDir]->link[aDir]
                        = sSaveNode->link[sReverseDir];

    sSaveNode->link[sReverseDir] = (*aNode)->link[sReverseDir];

    (*aNode)->link[sReverseDir] = sSaveNode;

    sSaveNode = (*aNode)->link[sReverseDir];

    (*aNode)->link[sReverseDir] = sSaveNode->link[aDir];

    sSaveNode->link[aDir] = *aNode;

    *aNode = sSaveNode;

    return;
    
#undef IDE_FN
}

void qsxAvl::_adjust_balance( qsxAvlNode ** aNode, SInt aDir, SInt sBalance )
{
/***********************************************************************
 *
 * Description : PROJ-1075 double rotation�� �ϱ� ���� balance�� �����Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/
#define IDE_FN "qsxAvl::_adjust_balance"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(IDE_FN));

    qsxAvlNode * sNode;
    qsxAvlNode * sChildNode;
    qsxAvlNode * sGrandChildNode;

    sNode = *aNode;

    sChildNode = sNode->link[aDir];
    sGrandChildNode = sChildNode->link[REVERSEDIR(aDir)];

    if( sGrandChildNode->balance == 0 )
    {
        sNode->balance = 0;
        sChildNode->balance = 0;
    }
    else if ( sGrandChildNode->balance == sBalance )
    {
        sNode->balance = -sBalance;
        sChildNode->balance = 0;
    }
    else // if ( sGrandChildNode->balance == -sBalance )
    {
        sNode->balance = 0;
        sChildNode->balance = sBalance;
    }

    sGrandChildNode->balance = 0;

    return;

#undef IDE_FN
}

void qsxAvl::_insert_balance( qsxAvlNode ** aNode, SInt aDir )
{

#define IDE_FN "qsxAvl::_insert_balance"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(IDE_FN));

    qsxAvlNode * sChildNode;
    SInt         sBalance;
    
    sChildNode = (*aNode)->link[aDir];
    sBalance = ( aDir == AVL_LEFT ) ? -1 : 1;
    
    if( sChildNode->balance == sBalance )
    {
        (*aNode)->balance = 0;
        sChildNode->balance = 0;
        _rotate_single( aNode, REVERSEDIR(aDir) );
    }
    else // if( sChildNode->balance == -sBalance )
    {
        _adjust_balance( aNode, aDir, sBalance );
        _rotate_double( aNode, REVERSEDIR(aDir) );
    }

    return;

#undef IDE_FN
}

void qsxAvl::_remove_balance( qsxAvlNode ** aNode, SInt aDir, idBool * aDone )
{

#define IDE_FN "qsxAvl::_remove_balance"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(IDE_FN));

    qsxAvlNode * sChildNode;
    SInt         sBalance;

    sChildNode = (*aNode)->link[REVERSEDIR(aDir)];
    sBalance = ( aDir == AVL_LEFT ) ? -1 : 1;

    if( sChildNode->balance == -sBalance )
    {
        (*aNode)->balance = 0;
        sChildNode->balance = 0;
        _rotate_single( aNode, aDir );
    }
    else if( sChildNode->balance == sBalance )
    {
        _adjust_balance( aNode, REVERSEDIR(aDir), -sBalance );
    }
    else // if( sChildNode->balance == 0 )
    {
        (*aNode)->balance = -sBalance;
        sChildNode->balance = sBalance;
        _rotate_single( aNode, aDir );
        *aDone = ID_TRUE;
    }
    
    return;

#undef IDE_FN
}

void qsxAvl::_getRowSizeAndDataOffset( mtcColumn * aKeyColumn,
                                       mtcColumn * aDataColumn,
                                       SInt      * aRowSize,
                                       SInt      * aDataOffset )
{

#define IDE_FN "qsxAvl::getRowSizeAndDataOffset"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(IDE_FN));

    *aDataOffset = idlOS::align8(aKeyColumn->column.size);

    *aRowSize = *aDataOffset + aDataColumn->column.size;

    return;

#undef IDE_FN
}

IDE_RC qsxAvl::_makeNode( qsxAvlTree  * aAvlTree,
                          mtcColumn   * aKeyCol,
                          void        * aKey,
                          qsxAvlNode ** aNode )
{

#define IDE_FN "qsxAvl::_makeNode"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(IDE_FN));

    void * sCanonizedValue;
    UInt   sActualSize;

    // ���ռ� �˻�. key�� null�� �� �� ����.
    IDE_DASSERT( aKey != NULL );

    // ���ռ� �˻�. key�� module�� �ݵ�� �����ؾ� ��.
    IDE_DASSERT( aAvlTree->keyCol->module == aKeyCol->module );

    // varchar, integer���� �� �� �����Ƿ� canonize�Ҷ� alloc���� �ʴ´�.
    // ���� �ٸ� type�� �����ϰ� �Ǹ� �ٲپ�� ��.
    if ( ( aAvlTree->keyCol->module->flag & MTD_CANON_MASK )
         == MTD_CANON_NEED )
    {
        IDE_TEST( aAvlTree->keyCol->module->canonize(
                      aAvlTree->keyCol,
                      &sCanonizedValue,           // canonized value
                      NULL,
                      aKeyCol,
                      aKey,                     // original value
                      NULL,
                      NULL )
                  != IDE_SUCCESS );
    }
    else
    {
        sCanonizedValue = aKey;
    }

    IDE_TEST( aAvlTree->memory->alloc( (void**)aNode )
              != IDE_SUCCESS );
    
    sActualSize = aAvlTree->keyCol->module->actualSize(
        aKeyCol,
        sCanonizedValue );
    
    idlOS::memcpy( (*aNode)->row,
                   sCanonizedValue,
                   sActualSize );

    (*aNode)->balance = 0;
    (*aNode)->reserved = 0;
    (*aNode)->link[AVL_LEFT] = NULL;
    (*aNode)->link[AVL_RIGHT] = NULL;
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC qsxAvl::_deleteNode( qsxAvlTree  * aAvlTree,
                            qsxAvlNode  * aNode )
{

#define IDE_FN "qsxAvl::_deleteNode"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(IDE_FN));

    void          * sDataPtr   = NULL;
    qsxArrayInfo ** sArrayInfo = NULL;

    sDataPtr = (SChar*)aNode->row + aAvlTree->dataOffset;

    // PROJ-1904 Extend UDT
    if ( aAvlTree->dataCol->type.dataTypeId == MTD_ASSOCIATIVE_ARRAY_ID )
    {
        sArrayInfo = (qsxArrayInfo**)sDataPtr;

        if ( *sArrayInfo != NULL )
        {
            IDE_TEST( qsxArray::finalizeArray( sArrayInfo )
                      != IDE_SUCCESS );
        }
        else
        {
            // Nothing to do.
        }
    }
    else if ( (aAvlTree->dataCol->type.dataTypeId == MTD_ROWTYPE_ID) ||
              (aAvlTree->dataCol->type.dataTypeId == MTD_RECORDTYPE_ID) )
    {
        IDE_TEST( qsxUtil::finalizeRecordVar( aAvlTree->dataCol,
                                              sDataPtr)
                  != IDE_SUCCESS );
    }
    else
    {
        // Nothing to do.
    }

    IDE_TEST( aAvlTree->memory->memfree( aNode )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

void qsxAvl::_copyNodeRow( qsxAvlTree * aAvlTree,
                           qsxAvlNode * aDestNode,
                           qsxAvlNode * aSrcNode )
{

#define IDE_FN "qsxAvl::_copyNodeRow"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(IDE_FN));

    idlOS::memcpy( aDestNode->row, aSrcNode->row, aAvlTree->rowSize );
    
    return;

#undef IDE_FN
}
