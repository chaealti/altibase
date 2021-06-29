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
 * $Id: qmcMemSortTempTable.cpp 82075 2018-01-17 06:39:52Z jina.kim $
 *
 * Description :
 *     Memory Sort Temp Table�� ���� �Լ�
 *
 * ��� ���� :
 *
 * ��� :
 *
 *
 **********************************************************************/

#include <idl.h>
#include <ide.h>
#include <qmc.h>
#include <qcg.h>
#include <qmcMemSortTempTable.h>

#define QMC_MEM_SORT_SWAP( elem1, elem2 )               \
    { temp = elem1; elem1 = elem2; elem2 = temp; }

IDE_RC
qmcMemSort::init( qmcdMemSortTemp * aTempTable,
                  qcTemplate      * aTemplate,
                  iduMemory       * aMemory,
                  qmdMtrNode      * aSortNode )
{
/***********************************************************************
 *
 * Description :
 *
 *    �ش� Memory Sort Temp Table��ü�� �ʱ�ȭ�Ѵ�.
 *
 * Implementation :
 *
 *    BUG-38290
 *    Temp table �� ���ο� qcTemplate �� qmcMemory �� ������
 *    temp table ������ ����Ѵ�.
 *    �� �ΰ����� temp table �� init �� ���� template �� �� template ��
 *    ����� QMX memory �̴�.
 *    ���� temp table init ������ temp table build ������ ���� �ٸ�
 *    template �� ����ؾ� �Ѵٸ� �� ������ ����Ǿ�� �Ѵ�.
 *    Parallel query ����� �����ϸ鼭 temp table build �� parallel ��
 *    ����� ��� �� ������ ����ؾ� �Ѵ�.
 *
 *    Temp table build �� temp table �� �����ϴ� plan node ��
 *    init ������ ����ȴ�.
 *    ������ ���� parallel query �� partitioned table �̳� HASH, SORT,
 *    GRAG  ��忡�� PRLQ ��带 ����, parallel �����ϹǷ� 
 *    temp table �� �����ϴ� plan node �� ���� init  ���� �ʴ´�.
 *
 *    �ٸ� subqeury filter ������ temp table �� ����� ���� ���ܰ� ���� ��
 *    ������, subquery filter �� plan node �� ����� �� �ʱ�ȭ �ǹǷ�
 *    ���ÿ� temp table  �� �����Ǵ� ���� ����.
 *    (qmcTempTableMgr::addTempTable �� ���� �ּ� ����)
 *
 ***********************************************************************/

    IDE_DASSERT( aTempTable != NULL );

    //------------------------------------------------------------
    // Sort Index Table �� ������ ���� Memory �������� ���� �� �ʱ�ȭ
    //------------------------------------------------------------
    aTempTable->mMemory = aMemory;

    IDU_FIT_POINT( "qmcMemSort::init::alloc::mMemory",
                    idERR_ABORT_InsufficientMemory );

    IDE_TEST(aTempTable->mMemory->alloc( ID_SIZEOF(qmcMemory),
                                      (void**)&aTempTable->mMemoryMgr)
             != IDE_SUCCESS);

    aTempTable->mMemoryMgr = new (aTempTable->mMemoryMgr)qmcMemory();

    /* BUG-38290 */
    aTempTable->mMemoryMgr->init( ID_SIZEOF(qmcSortArray) );

    IDU_FIT_POINT( "qmcMemSort::init::cralloc::mArray",
                    idERR_ABORT_InsufficientMemory );

    IDE_TEST(aTempTable->mMemoryMgr->cralloc( aTempTable->mMemory,
                                           ID_SIZEOF(qmcSortArray),
                                           (void**)&aTempTable->mArray )
             != IDE_SUCCESS);

    //------------------------------------------------------------
    // Memory Sort Temp Table ��ü�� �ʱ�ȭ
    //------------------------------------------------------------

    aTempTable->mArray->parent      = NULL;
    aTempTable->mArray->depth       = 0;
    aTempTable->mArray->numIndex    = QMC_MEM_SORT_MAX_ELEMENTS;
    aTempTable->mArray->shift       = 0;
    aTempTable->mArray->mask        = 0;
    aTempTable->mArray->index       = 0;
    aTempTable->mArray->elements    = aTempTable->mArray->internalElem;

    aTempTable->mTemplate           = aTemplate;
    aTempTable->mSortNode           = aSortNode;

    // TASK-6445
    aTempTable->mUseOldSort         = QCG_GET_SESSION_USE_OLD_SORT( aTemplate->stmt );
    aTempTable->mRunStack           = NULL;
    aTempTable->mRunStackSize       = 0;
    aTempTable->mTempArray          = NULL;

    aTempTable->mStatistics   = aTemplate->stmt->mStatistics;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmcMemSort::clear( qmcdMemSortTemp * aTempTable )
{
/***********************************************************************
 *
 * Description :
 *    ����� Sorting Data ������ �����Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

    if ( aTempTable->mMemoryMgr != NULL )
    {
        aTempTable->mMemoryMgr->clearForReuse();

        aTempTable->mArray = NULL;

        IDU_FIT_POINT( "qmcMemSort::clear::cralloc::mArray",
                        idERR_ABORT_InsufficientMemory );

        /* BUG-38290 */
        IDE_TEST(aTempTable->mMemoryMgr->cralloc(
                aTempTable->mMemory,
                ID_SIZEOF(qmcSortArray),
                (void**)&aTempTable->mArray)
            != IDE_SUCCESS);

        aTempTable->mArray->parent      = NULL;
        aTempTable->mArray->depth       = 0;
        aTempTable->mArray->numIndex    = QMC_MEM_SORT_MAX_ELEMENTS;
        aTempTable->mArray->shift       = 0;
        aTempTable->mArray->mask        = 0;
        aTempTable->mArray->index       = 0;
        aTempTable->mArray->elements    = aTempTable->mArray->internalElem;
    }
    else
    {
        // Nothing to do.
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmcMemSort::clearHitFlag( qmcdMemSortTemp * aTempTable )
{
/***********************************************************************
 *
 * Description :
 *    ����� ��� Record�� Hit Flag�� reset�Ѵ�.
 *
 * Implementation :
 *    ���� ��ġ�� �̵��Ͽ� ��� Record�� �˻��ϰ�,
 *    �̿� ���� Hit Flag�� reset�Ѵ�.
 *
 ***********************************************************************/

    qmcMemSortElement * sRow;

    IDE_TEST( beforeFirstElement( aTempTable ) != IDE_SUCCESS );

    IDE_TEST( getNextElement( aTempTable, (void**) & sRow ) != IDE_SUCCESS );

    while ( sRow != NULL )
    {
        sRow->flag &= ~QMC_ROW_HIT_MASK;
        sRow->flag |= QMC_ROW_HIT_FALSE;

        IDE_TEST( getNextElement( aTempTable, (void**) & sRow )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmcMemSort::attach( qmcdMemSortTemp * aTempTable,
                    void            * aRow )
{
/***********************************************************************
 *
 * Description :
 *    Memory Sort Temp Table�� Record�� �߰��Ѵ�.
 *
 * Implementation :
 *    ����� ��ġ�� ã��, Pointer�� �����Ѵ�.
 *    ���� Sort Array�� ������Ų��.
 *
 ***********************************************************************/

    void** sDestination;

    get( aTempTable,
         aTempTable->mArray,
         aTempTable->mArray->index,
         & sDestination );

    *sDestination = aRow;

    IDE_TEST( increase( aTempTable,
                        aTempTable->mArray ) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


IDE_RC
qmcMemSort::sort( qmcdMemSortTemp * aTempTable )
{
/***********************************************************************
 *
 * Description :
 *     Sorting�� �����Ѵ�.
 *
 * Implementation :
 *     ����� Record ����ŭ quick sort�� �����Ѵ�.
 *
 ***********************************************************************/

    if( aTempTable->mArray->index > 0 )
    {
        // TASK-6445
        if ( aTempTable->mUseOldSort == 0 )
        {
            IDE_TEST( timsort( aTempTable,
                               0,                             
                               aTempTable->mArray->index - 1 )
                      != IDE_SUCCESS );
        }
        else
        {
            IDE_TEST( quicksort( aTempTable,
                                 0,                             
                                 aTempTable->mArray->index - 1 )
                      != IDE_SUCCESS );
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmcMemSort::shiftAndAppend( qmcdMemSortTemp * aTempTable,
                            void            * aRow,
                            void           ** aReturnRow )
{
/***********************************************************************
 *
 * Description :
 *     Limit Sorting�� �����Ѵ�.
 *
 * Implementation :
 *     �Է��� Row�� �Է� ���������� �˻�
 *     �Է� ������ ��� ������ Row�� �о�� Row ����
 *
 *     Ex)   3�� �Է�
 *                  +-- [3]�� ���Ե� ��ġ
 *                  V
 *           [1, 2, 3, 4, 5]
 *                 ||
 *                \  /
 *                 \/
 *           [1, 2, 3, 3, 4]     [5] : return row
 *
 ***********************************************************************/

    SLong  sMaxIndex;
    SLong  sInsertPos;
    SLong  i;

    void  ** sSrcIndex;
    void  ** sDstIndex;

    void  ** sDiscardRow;

    //-------------------------------------------
    // ���� ���� ���� ����
    // - �������� ����� Data�� ���Ͽ�
    //   ���ʿ� �ش��ϸ� ������ Data�̸�,
    //   �� ���� ����� ������ �� Data�̴�.
    //-------------------------------------------

    IDE_TEST( findInsertPosition ( aTempTable, aRow, & sInsertPos )
              != IDE_SUCCESS );

    if ( sInsertPos >= 0 )
    {
        // Discard Row����
        // ������ ��ġ�� Row�� discard row�� ����

        sMaxIndex = aTempTable->mArray->index - 1;

        get( aTempTable, aTempTable->mArray, sMaxIndex, & sDiscardRow );
        *aReturnRow = *sDiscardRow;

        // Shifting ����
        // ���������� �ش� ���� ��ġ���� Shifting

        if ( sInsertPos < sMaxIndex )
        {
            for ( i = sMaxIndex; i > sInsertPos; i-- )
            {
                get( aTempTable, aTempTable->mArray,   i, & sDstIndex );
                get( aTempTable, aTempTable->mArray, i-1, & sSrcIndex );
                *sDstIndex = *sSrcIndex;
            }
            // ���� ����
            *sSrcIndex = aRow;
        }
        else
        {
            // To Fix PR-8201
            // ���� ���Ե� ���
            IDE_DASSERT( sInsertPos == sMaxIndex );

            get( aTempTable, aTempTable->mArray, sInsertPos, & sSrcIndex );
            *sSrcIndex = aRow;
        }
    }
    else
    {
        // ������ Row�� �ƴ�
        *aReturnRow = aRow;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmcMemSort::changeMinMax( qmcdMemSortTemp * aTempTable,
                          void            * aRow,
                          void           ** aReturnRow )
{
/***********************************************************************
 *
 * Description :
 *     Min-Max ������ ���� Limit Sorting�� �����Ѵ�.
 *
 * Implementation :
 *     Right-Most�� ���Ͽ� ���� Right�̸� Right-Most ��ü
 *     Left-Most�� ���Ͽ� ���� Left�̸� Left-Most ��ü
 *     �� ���� ���� �������� ����.
 *
 ***********************************************************************/

    void  ** sIndex;
    SInt     sOrder;

    //-------------------------------------------
    // ���ռ� �˻�
    //-------------------------------------------

    // Min-Max �� �Ǹ��� ����Ǿ� �־�� ��.
    IDE_DASSERT( aTempTable->mArray->index == 2 );

    //-------------------------------------------
    // Right-Most �� ��ü ���� ����
    //-------------------------------------------

    get( aTempTable, aTempTable->mArray, 1, & sIndex );

    sOrder = compare( aTempTable, aRow, *sIndex );

    if ( sOrder > 0 )
    {
        // Right-Most ���� ���� �ʿ� �ִ� ���
        // Right-Most�� ��ü�Ѵ�.
        *aReturnRow = *sIndex;
        *sIndex = aRow;
    }
    else
    {
        // Right-Most ���� ���ų� ���ʿ� �ִ� ���

        get( aTempTable, aTempTable->mArray, 0, & sIndex );

        sOrder = compare( aTempTable, aRow, *sIndex );

        if ( sOrder < 0 )
        {
            // Left-Most���� ���ʿ� �ִ� ���
            // Left-Most�� ��ü�Ѵ�.
            *aReturnRow = *sIndex;
            *sIndex = aRow;
        }
        else
        {
            // Left-Most�� Right-Most�� �߰��� �ִ� ����
            // ���� ����� �ƴϴ�.
            *aReturnRow = aRow;
        }
    }

    return IDE_SUCCESS;
}

IDE_RC
qmcMemSort::getElement( qmcdMemSortTemp * aTempTable,
                        SLong aIndex,
                        void** aElement )
{
/***********************************************************************
 *
 * Description :
 *    ������ ��ġ�� Record�� �����´�.
 *
 * Implementation :
 *
 ***********************************************************************/

    void** sBuffer;

    // fix BUG-10441
    IDE_DASSERT ( aIndex >= 0 );

    if( aTempTable->mArray->index > 0 )
    {
        // sort temp table�� ����� ���ڵ尡 �ϳ� �̻��� ����
        // ����� ��ġ�� record�� �����´�.
        IDE_DASSERT ( aIndex < aTempTable->mArray->index );

        get( aTempTable, aTempTable->mArray, aIndex, &sBuffer );
        *aElement = *sBuffer;
    }
    else
    {
        // sort temp table�� ����� ���ڵ尡 �ϳ��� ���� ���
        IDE_DASSERT( aIndex == 0 );

        *aElement = NULL;
    }

    return IDE_SUCCESS;
}

IDE_RC
qmcMemSort::getFirstSequence( qmcdMemSortTemp * aTempTable,
                              void           ** aRow )
{
/***********************************************************************
 *
 * Description :
 *    ù ��° ���� �˻�
 *
 * Implementation :
 *    ���� ��ġ�� �̵��Ͽ� �ش� Record�� ȹ���Ѵ�.
 *
 ***********************************************************************/

    IDE_TEST( beforeFirstElement( aTempTable ) != IDE_SUCCESS );

    IDE_TEST( getNextElement( aTempTable,
                              aRow ) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmcMemSort::getNextSequence( qmcdMemSortTemp * aTempTable,
                             void           ** aRow )
{
/***********************************************************************
 *
 * Description :
 *    ���� ���� �˻�
 *
 * Implementation :
 *    �ش� Record�� ȹ���Ѵ�.
 *
 ***********************************************************************/

    IDE_TEST( getNextElement( aTempTable,
                              aRow ) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmcMemSort::getFirstRange( qmcdMemSortTemp * aTempTable,
                           smiRange        * aRange,
                           void           ** aRow )
{
/***********************************************************************
 *
 * Description :
 *    ù ��° Range �˻�
 *
 * Implementation :
 *    �־��� Range�� �����ϰ�, �̿� �����ϴ� Record�� �˻���.
 *
 ***********************************************************************/

    setKeyRange( aTempTable, aRange );

    IDE_TEST( getFirstKey( aTempTable, aRow ) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmcMemSort::getNextRange( qmcdMemSortTemp * aTempTable,
                          void           ** aRow )
{
/***********************************************************************
 *
 * Description :
 *    ���� Range �˻�
 *
 * Implementation :
 *    Range�� �����ϴ� Record�� �˻���.
 *
 ***********************************************************************/

    IDE_TEST( getNextKey( aTempTable, aRow ) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmcMemSort::getFirstHit( qmcdMemSortTemp * aTempTable,
                                void           ** aRow )
{
/***********************************************************************
 *
 * Description : PROJ-2385
 *    Hit�� ���� Record �˻�
 *
 * Implementation :
 *    ���� ��ġ�� �̵��Ͽ�, Hit�� Record�� ã�� ������
 *    �ݺ� �˻���.
 *
 ***********************************************************************/

    qmcMemSortElement * sRow;

    IDE_TEST( beforeFirstElement( aTempTable ) != IDE_SUCCESS );

    IDE_TEST( getNextElement( aTempTable, (void**) & sRow ) != IDE_SUCCESS );

    while ( sRow != NULL )
    {
        if ( (sRow->flag & QMC_ROW_HIT_MASK) == QMC_ROW_HIT_TRUE )
        {
            // Hit�� Record�� ���
            break;
        }
        else
        {
            // Hit���� ���� Reocrd�� ��� ���� Recordȹ��
            IDE_TEST( getNextElement( aTempTable,
                                      (void**) & sRow ) != IDE_SUCCESS );
        }
    }

    *aRow = (void*) sRow;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmcMemSort::getNextHit( qmcdMemSortTemp * aTempTable,
                               void           ** aRow )
{
/***********************************************************************
 *
 * Description : PROJ-2385
 *    Hit�� ���� Record �˻�
 *
 * Implementation :
 *    Hit�� Record�� ã�� ������ �ݺ� �˻���.
 *
 ***********************************************************************/

    qmcMemSortElement * sRow;

    IDE_TEST( getNextElement( aTempTable, (void**) & sRow ) != IDE_SUCCESS );

    while ( sRow != NULL )
    {
        if ( (sRow->flag & QMC_ROW_HIT_MASK) == QMC_ROW_HIT_TRUE )
        {
            // Hit�� Record�� ���
            break;
        }
        else
        {
            // Hit���� ���� Reocrd�� ��� ���� Recordȹ��
            IDE_TEST( getNextElement( aTempTable,
                                      (void**) & sRow ) != IDE_SUCCESS );
        }
    }

    *aRow = (void*) sRow;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmcMemSort::getFirstNonHit( qmcdMemSortTemp * aTempTable,
                            void           ** aRow )
{
/***********************************************************************
 *
 * Description :
 *    Hit���� ���� ���� Record �˻�
 *
 * Implementation :
 *    ���� ��ġ�� �̵��Ͽ�, Hit���� ���� Record�� ã�� ������
 *    �ݺ� �˻���.
 *
 ***********************************************************************/

    qmcMemSortElement * sRow;

    IDE_TEST( beforeFirstElement( aTempTable ) != IDE_SUCCESS );

    IDE_TEST( getNextElement( aTempTable, (void**) & sRow ) != IDE_SUCCESS );

    while ( sRow != NULL )
    {
        if ( (sRow->flag & QMC_ROW_HIT_MASK) == QMC_ROW_HIT_FALSE )
        {
            // Hit ���� ���� Record�� ���
            break;
        }
        else
        {
            // Hit �� Reocrd�� ��� ���� Recordȹ��
            IDE_TEST( getNextElement( aTempTable,
                                      (void**) & sRow ) != IDE_SUCCESS );
        }
    }

    *aRow = (void*) sRow;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmcMemSort::getNextNonHit( qmcdMemSortTemp * aTempTable,
                           void           ** aRow )
{
/***********************************************************************
 *
 * Description :
 *    Hit���� ���� ���� Record �˻�
 *
 * Implementation :
 *    Hit���� ���� Record�� ã�� ������ �ݺ� �˻���.
 *
 ***********************************************************************/

    qmcMemSortElement * sRow;

    IDE_TEST( getNextElement( aTempTable, (void**) & sRow ) != IDE_SUCCESS );

    while ( sRow != NULL )
    {
        if ( (sRow->flag & QMC_ROW_HIT_MASK) == QMC_ROW_HIT_FALSE )
        {
            // Hit ���� ���� Record�� ���
            break;
        }
        else
        {
            // Hit �� Reocrd�� ��� ���� Recordȹ��
            IDE_TEST( getNextElement( aTempTable,
                                      (void**) & sRow ) != IDE_SUCCESS );
        }
    }

    *aRow = (void*) sRow;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


IDE_RC
qmcMemSort::getNumOfElements( qmcdMemSortTemp * aTempTable,
                              SLong           * aNumElements )
{
/***********************************************************************
 *
 * Description :
 *    ����Ǿ� �ִ� Record�� ������ ȹ���Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

    IDE_TEST_RAISE( aTempTable == NULL,
                    ERR_INVALID_ARGUMENT );
    
    *aNumElements = aTempTable->mArray->index;

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_INVALID_ARGUMENT )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmcMemSort::getNumOfElements",
                                  "aTempTable is NULL" ));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmcMemSort::getCurrPosition( qmcdMemSortTemp * aTempTable,
                             SLong           * aPosition )
{
/***********************************************************************
 *
 * Description :
 *    ���� �˻����� Record�� ��ġ���� ȹ���Ѵ�.
 *    Merge Join���� Cursor ��ġ�� �����ϱ� ���Ͽ� �����.
 *
 * Implementation :
 *    ���� �˻� ��ġ�� ������.
 *
 ***********************************************************************/

    // ��ȿ�� �˻�
    IDE_DASSERT( aTempTable->mIndex >= 0 &&
                 aTempTable->mIndex < aTempTable->mArray->index );

    *aPosition = aTempTable->mIndex;

    return IDE_SUCCESS;
}

IDE_RC
qmcMemSort::setCurrPosition( qmcdMemSortTemp * aTempTable,
                             SLong             aPosition )
{
/***********************************************************************
 *
 * Description :
 *    ������ ��ġ�� Cursor�� �ű�
 *    Merge Join���� Cursor ��ġ�� ������ �ϱ� ���Ͽ� �����.
 *
 * Implementation :
 *    ������ ��ġ�� index���� �����Ŵ.
 *
 ***********************************************************************/

    // ��ȿ�� �˻�
    IDE_DASSERT( aPosition >= 0 &&
                 aPosition < aTempTable->mArray->index );

    aTempTable->mIndex = aPosition;

    return IDE_SUCCESS;
}

IDE_RC
qmcMemSort::setSortNode( qmcdMemSortTemp  * aTempTable,
                         const qmdMtrNode * aSortNode )
{
/***********************************************************************
 *
 * Description 
 *    ���� ����Ű(sortNode)�� ����
 *    Window Sort(WNST)���� ������ �ݺ��ϱ� ���� �����.
 *
 * Implementation :
 *
 ***********************************************************************/

    aTempTable->mSortNode = (qmdMtrNode*) aSortNode;

    return IDE_SUCCESS;
}

void
qmcMemSort::get( qmcdMemSortTemp * aTempTable,
                 qmcSortArray* aArray,
                 SLong aIndex,
                 void*** aElement )
{
/***********************************************************************
 *
 * Description :
 *    ������ ��ġ�� Reocrd ��ġ�� ȹ���Ѵ�.
 *
 * Implementation :
 *    Leaf�� �ش��ϴ� Sort Array�� ã��,
 *    Sort Array������ ������ ��ġ�� Record ��ġ�� ȹ���Ѵ�.
 *
 ***********************************************************************/

    if( aArray->depth != 0 )
    {
        get( aTempTable,
             aArray->elements[aIndex>>(aArray->shift)],
             aIndex & (aArray->mask),
             aElement );
    }
    else
    {
        *aElement = (void**) (&(aArray->elements[aIndex]));
    }

}

IDE_RC qmcMemSort::increase( qmcdMemSortTemp * aTempTable,
                             qmcSortArray    * aArray )
{
/***********************************************************************
 *
 * Description :
 *    Sort Array�� ���� ������ ������Ų��.
 *
 * Implementation :
 *    Sort Array�� ���� ������ ������Ű��,
 *    ���� ������ ���� ���, Sort Array�� �߰��Ѵ�.
 *
 ***********************************************************************/

    qmcSortArray* sParent;
    SLong          sDepth;
    SLong          sReachEnd;

    IDE_TEST( increase( aTempTable,
                        aArray,
                        & sReachEnd ) != IDE_SUCCESS );

    if( sReachEnd )
    {
        IDU_LIMITPOINT("qmcMemSort::increase::malloc1");
        /* BUG-38290 */
        IDE_TEST(aTempTable->mMemoryMgr->cralloc(
                aTempTable->mMemory,
                ID_SIZEOF(qmcSortArray),
                (void**)&aTempTable->mArray)
            != IDE_SUCCESS);

        aTempTable->mArray->parent      = NULL;
        aTempTable->mArray->depth       = aArray->depth + 1;
        aTempTable->mArray->numIndex    =
            (aArray->numIndex) << QMC_MEM_SORT_SHIFT;
        aTempTable->mArray->shift       = aArray->shift + QMC_MEM_SORT_SHIFT;
        aTempTable->mArray->mask        =
            ( (aArray->mask) << QMC_MEM_SORT_SHIFT ) | QMC_MEM_SORT_MASK;
        aTempTable->mArray->index       = aArray->numIndex;
        aTempTable->mArray->elements    = aTempTable->mArray->internalElem;
        aTempTable->mArray->elements[0] = aArray;

        IDU_LIMITPOINT("qmcMemSort::increase::malloc2");
        IDE_TEST(aTempTable->mMemoryMgr->cralloc(
                aTempTable->mMemory,
                ID_SIZEOF(qmcSortArray),
                (void**)&(aTempTable->mArray->elements[1]))
            != IDE_SUCCESS);

        sParent              = aTempTable->mArray;

        aArray->parent       = sParent;
        aArray               = aTempTable->mArray->elements[1];
        aArray->parent       = sParent;
        aArray->depth        = sParent->depth - 1;
        aArray->numIndex     = (sParent->numIndex) >> QMC_MEM_SORT_SHIFT;
        aArray->shift        = sParent->shift - QMC_MEM_SORT_SHIFT;
        aArray->mask         = (sParent->mask) >> QMC_MEM_SORT_SHIFT;
        aArray->index        = 0;
        aArray->elements     = aArray->internalElem;

        for( sDepth = aArray->depth; sDepth > 0; sDepth-- )
        {
            IDU_LIMITPOINT("qmcMemSort::increase::malloc3");
            IDE_TEST(aTempTable->mMemoryMgr->cralloc(
                    aTempTable->mMemory,
                    ID_SIZEOF(qmcSortArray),
                    (void**)&(aArray->elements[0]))
                != IDE_SUCCESS);

            sParent = aArray;
            aArray  = aArray->elements[0];
            aArray->parent      = sParent;
            aArray->depth       = sParent->depth - 1;
            aArray->numIndex    = (sParent->numIndex) >> QMC_MEM_SORT_SHIFT;
            aArray->shift       = sParent->shift - QMC_MEM_SORT_SHIFT;
            aArray->mask        = (sParent->mask) >> QMC_MEM_SORT_SHIFT;
            aArray->index       = 0;
            aArray->elements    = aArray->internalElem;
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmcMemSort::increase( qmcdMemSortTemp * aTempTable,
                             qmcSortArray    * aArray,
                             SLong           * aReachEnd )
{
/***********************************************************************
 *
 * Description :
 *    Sort Array�� ���� ������ ������Ų��.
 *
 * Implementation :
 *    Sort Array�� ���� ������ ������Ű��,
 *    ���� ������ ���� ���, Sort Array�� �߰��Ѵ�.
 *
 ***********************************************************************/

    qmcSortArray* sParent;
    SLong          sDepth;

    if( aArray->depth == 0 )
    {
        aArray->index++;
        if( aArray->index >= aArray->numIndex )
        {
            *aReachEnd = 1;
        }else
        {
            *aReachEnd = 0;
        }
    }
    else
    {
        IDE_TEST( increase( aTempTable,
                            aArray->elements[(aArray->index)>>(aArray->shift)],
                            aReachEnd ) != IDE_SUCCESS );
        aArray->index++;
        if( aArray->index >= aArray->numIndex )
        {
            *aReachEnd = 1;
        }
        else
        {
            if( *aReachEnd )
            {
                IDU_LIMITPOINT("qmcMemSort::increase::malloc4");
                /* BUG-38290 */
                IDE_TEST( aTempTable->mMemoryMgr->cralloc(
                        aTempTable->mMemory,
                        ID_SIZEOF(qmcSortArray),
                        (void**)&(aArray->elements[(aArray->index) >>
                                  (aArray->shift)]))
                    != IDE_SUCCESS);

                sParent = aArray;
                aArray  = aArray->elements[(aArray->index)>>(aArray->shift)];

                aArray->parent    = sParent;
                aArray->depth     = sParent->depth - 1;
                aArray->numIndex  = (sParent->numIndex) >> QMC_MEM_SORT_SHIFT;
                aArray->shift     = sParent->shift - QMC_MEM_SORT_SHIFT;
                aArray->mask      = (sParent->mask) >> QMC_MEM_SORT_SHIFT;
                aArray->index     = 0;
                aArray->elements  = aArray->internalElem;

                for( sDepth = aArray->depth; sDepth > 0; sDepth-- )
                {
                    IDU_LIMITPOINT("qmcMemSort::increase::malloc5");
                    IDE_TEST(aTempTable->
                             mMemoryMgr->cralloc( aTempTable->mMemory,
                                               ID_SIZEOF(qmcSortArray),
                                               (void**)&(aArray->elements[0]))
                             != IDE_SUCCESS);

                    sParent = aArray;
                    aArray  =
                        aArray->elements[(aArray->index)>>(aArray->shift)];

                    aArray->parent   = sParent;
                    aArray->depth    = sParent->depth - 1;
                    aArray->numIndex =
                        (sParent->numIndex) >> QMC_MEM_SORT_SHIFT;
                    aArray->shift    = sParent->shift - QMC_MEM_SORT_SHIFT;
                    aArray->mask     = (sParent->mask) >> QMC_MEM_SORT_SHIFT;
                    aArray->index    = 0;
                    aArray->elements = aArray->internalElem;
                }
                *aReachEnd = 0;
            }
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmcMemSort::quicksort( qmcdMemSortTemp * aTempTable,
                       SLong aLow,
                       SLong aHigh )
{
/***********************************************************************
 *
 * Description :
 *    ������ low�� high���� Data���� Quick Sorting�Ѵ�.
 *
 * Implementation :
 *
 *    Stack Overflow�� �߻����� �ʵ��� Stack������ �����Ѵ�.
 *    Partition�� �����Ͽ� �ش� Partition ������ Recursive�ϰ�
 *    Quick Sorting�Ѵ�.
 *    �ڼ��� ������ �Ϲ����� quick sorting algorithm�� ����.
 *
 ***********************************************************************/

    SLong           sLow;
    SLong           sHigh;
    SLong           sPartition;
    qmcSortStack * sSortStack = NULL;
    qmcSortStack * sStack = NULL;
    idBool          sAllEqual;
    SLong           sRecCount = aHigh - aLow + 1;

    IDU_LIMITPOINT("qmcMemSort::quicksort::malloc");
    IDE_TEST(iduMemMgr::malloc(IDU_MEM_QMC,
                               sRecCount * ID_SIZEOF(qmcSortStack),
                               (void**)&sSortStack)
             != IDE_SUCCESS);

    sStack = sSortStack;
    sStack->low = aLow;
    sStack->high = aHigh;
    sStack++;

    sLow = aLow;
    sHigh = aHigh;

    while( sStack != sSortStack )
    {
        while ( sLow < sHigh )
        {
            IDE_TEST( iduCheckSessionEvent( aTempTable->mStatistics )
                      != IDE_SUCCESS );

            sAllEqual = ID_FALSE;
            IDE_TEST( partition( aTempTable,
                                 sLow,
                                 sHigh,
                                 & sPartition,
                                 & sAllEqual ) != IDE_SUCCESS );

            if ( sAllEqual == ID_TRUE )
            {
                break;
            }
            else
            {
                sStack->low = sPartition;
                sStack->high = sHigh;
                sStack++;
                sHigh = sPartition - 1;
            }
        }

        sStack--;
        sLow = sStack->low + 1;
        sHigh = sStack->high;
    }

    IDE_TEST(iduMemMgr::free(sSortStack)
             != IDE_SUCCESS);
    // To Fix PR-12539
    // sStack = NULL;
    sSortStack = NULL;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if ( sSortStack != NULL )
    {
        (void)iduMemMgr::free(sSortStack);
        sSortStack = NULL;
    }
    else
    {
        // nothing to do
    }

    return IDE_FAILURE;
}


IDE_RC
qmcMemSort::partition( qmcdMemSortTemp * aTempTable,
                       SLong    aLow,
                       SLong    aHigh,
                       SLong  * aPartition,
                       idBool * aAllEqual )
{
/***********************************************************************
 *
 * Description :
 *     Partition�������� ������ �����Ѵ�.
 *
 * Implementation :
 *     Partition�������� ������ �����ϰ�,
 *     ������ �Ǵ� Partition�� ��ġ�� �����Ѵ�.
 *     �ڼ��� ������ �Ϲ����� quick sorting algorithm ����
 *
 ***********************************************************************/

    void ** sPivotRow = NULL;
    void ** sLowRow   = NULL;
    void ** sHighRow  = NULL;
    void  * temp      = NULL;
    SInt    sOrder    = 0;
    SLong   sLow      = aLow;
    SLong   sHigh     = aHigh + 1;
    idBool  sAllEqual = ID_TRUE;

    if ( aLow + QMC_MEM_SORT_SMALL_SIZE < aHigh )
    {
        get( aTempTable, aTempTable->mArray, (aLow + aHigh) / 2,
             &sLowRow );

        get( aTempTable, aTempTable->mArray, aLow,
             &sPivotRow );

        QMC_MEM_SORT_SWAP( *sPivotRow, *sLowRow );

        do
        {
            do
            {
                sLow++;
                if( sLow < sHigh )
                {
                    get( aTempTable, aTempTable->mArray, sLow,
                         &sLowRow );

                    sOrder = compare( aTempTable,
                                      *sPivotRow,
                                      *sLowRow);

                    if( sOrder )
                    {
                        sAllEqual = ID_FALSE;
                    }
                    else
                    {
                        // nothing to do
                    }
                }
            } while( sLow < sHigh && sOrder > 0 );

            do
            {
                sHigh--;
                get( aTempTable, aTempTable->mArray, sHigh,
                     &sHighRow );

                sOrder = compare( aTempTable,
                                  *sPivotRow,
                                  *sHighRow);
                if( sOrder )
                {
                    sAllEqual = ID_FALSE;
                }
                else
                {
                    // nothing to do
                }
            } while( sOrder < 0  );

            if( sLow < sHigh )
            {
                QMC_MEM_SORT_SWAP( *sLowRow, *sHighRow );
            }
            else
            {
                // nothing to do
            }
        } while( sLow < sHigh );

        QMC_MEM_SORT_SWAP( *sPivotRow, *sHighRow );
    }
    else
    {
        // BUG-41048 Improve orderby sort algorithm.
        // �����ؾ��ϴ� �������� ������ QMC_MEM_SORT_SMALL_SIZE ���� ��������
        // insertion sort �� �̿��Ѵ�.
        IDE_TEST( insertionSort( aTempTable,
                                 aLow,
                                 aHigh )
                  != IDE_SUCCESS );
    }

    *aPartition = sHigh;
    *aAllEqual = sAllEqual;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


SInt
qmcMemSort::compare( qmcdMemSortTemp * aTempTable,
                     const void      * aElem1,
                     const void      * aElem2 )
{
/***********************************************************************
 *
 * Description :
 *     Sorting�ÿ� �� Record���� ��� �񱳸� �Ѵ�.
 *     ORDER BY���� ���� ������ ���ؼ��� ���� ���� Column�� ����
 *     ��� �񱳰� ����������, Range�˻��� ���� ���Ŀ����� �ϳ���
 *     Column�� ���� ���ĸ��� �����ϴ�.
 *
 * Implementation :
 *     �ش� Sort Node�� ������ �̿��Ͽ�, ��� �񱳸� �����Ѵ�.
 *
 ***********************************************************************/

    qmdMtrNode   * sNode;
    SInt           sResult = -1;
    mtdValueInfo   sValueInfo1;
    mtdValueInfo   sValueInfo2;
    idBool         sIsNull1;
    idBool         sIsNull2;

    for( sNode = aTempTable->mSortNode;
         sNode != NULL;
         sNode = sNode->next )
    {
        sValueInfo1.value  = sNode->func.getRow(sNode, aElem1);
        sValueInfo1.column = (const mtcColumn *)sNode->func.compareColumn;
        sValueInfo1.flag   = MTD_OFFSET_USE;

        sValueInfo2.value  = sNode->func.getRow(sNode, aElem2);
        sValueInfo2.column = (const mtcColumn *)sNode->func.compareColumn;
        sValueInfo2.flag   = MTD_OFFSET_USE;
       
        /* PROJ-2435 order by nulls first/last */
        if ( ( sNode->flag & QMC_MTR_SORT_NULLS_ORDER_MASK )
             == QMC_MTR_SORT_NULLS_ORDER_NONE )
        {
            /* NULLS first/last �� ������� �������� ����*/
            sResult = sNode->func.compare( &sValueInfo1,
                                           &sValueInfo2 );
        }
        else
        {
            /* 1. 2���� value�� Null ���θ� �����Ѵ�. */
            sIsNull1 = sNode->func.isNull( sNode,
                                           (void *)aElem1 );
            sIsNull2 = sNode->func.isNull( sNode,
                                           (void *)aElem2 );

            if ( sIsNull1 == sIsNull2 )
            {
                if ( sIsNull1 == ID_FALSE )
                {
                    /* 2. NULL�̾������ �������� ����*/
                    sResult = sNode->func.compare( &sValueInfo1,
                                                   &sValueInfo2 );
                }
                else
                {
                    /* 3. �Ѵ� NULL �� ��� 0 */
                    sResult = 0;
                }
            }
            else
            {
                if ( ( sNode->flag & QMC_MTR_SORT_NULLS_ORDER_MASK )
                     == QMC_MTR_SORT_NULLS_ORDER_FIRST )
                {
                    /* 4. NULLS FIRST �ϰ�� Null�� �ּҷ� �Ѵ�. */
                    if ( sIsNull1 == ID_TRUE )
                    {
                        sResult = -1;
                    }
                    else
                    {
                        sResult = 1;
                    }
                }
                else
                {
                    /* 5. NULLS LAST �ϰ�� Null�� �ִ�� �Ѵ�. */
                    if ( sIsNull1 == ID_TRUE )
                    {
                        sResult = 1;
                    }
                    else
                    {
                        sResult = -1;
                    }
                }
            }
        }

        if( sResult )
        {
            break;
        }
        else
        {
            continue;
        }
    }

    return sResult;
}

IDE_RC
qmcMemSort::beforeFirstElement( qmcdMemSortTemp * aTempTable )
{
/***********************************************************************
 *
 * Description :
 *    ���� �˻��� ���� ������ Record ������ ��ġ�� �̵��Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

    aTempTable->mIndex = -1;

    return IDE_SUCCESS;
}

IDE_RC
qmcMemSort::getNextElement( qmcdMemSortTemp * aTempTable,
                            void           ** aElement )
{
/***********************************************************************
 *
 * Description :
 *    ���� ��ġ�� Record�� �˻��Ѵ�.
 *
 * Implementation :
 *
 *    ���� ��ġ�� �̵��Ͽ�, �� ��ġ�� Record�� �˻��Ѵ�.
 *    Record�� ���� ���, NULL�� �����Ѵ�.
 *
 ***********************************************************************/

    void** sBuffer;

    aTempTable->mIndex++;

    if( aTempTable->mIndex >= 0 &&
        aTempTable->mIndex < aTempTable->mArray->index )
    {
        get( aTempTable,
             aTempTable->mArray,
             aTempTable->mIndex,
             & sBuffer );
        *aElement = *sBuffer;
    }
    else
    {
        *aElement = NULL;
    }

    return IDE_SUCCESS;
}

void
qmcMemSort::setKeyRange( qmcdMemSortTemp * aTempTable,
                         smiRange * aRange )
{
/***********************************************************************
 *
 * Description :
 *    Key Range �˻��� ���� Range�� �����Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

    aTempTable->mRange = aRange;

}

IDE_RC
qmcMemSort::getFirstKey( qmcdMemSortTemp * aTempTable,
                         void ** aElement )
{
/***********************************************************************
 *
 * Description :
 *    Range�� �����ϴ� ������ Record�� �˻��Ѵ�.
 *
 * Implementation :
 *
 *    Range�� �����ϴ� Minimum Key�� ��ġ�� ã��,
 *    ���� ���, Maximum Key�� ��ġ�� ã�´�.
 *    Minimum Key�� Record�� �����Ѵ�.
 *
 ***********************************************************************/

    idBool sResult = ID_FALSE;

    aTempTable->mFirstKey = 0;
    aTempTable->mLastKey = aTempTable->mArray->index - 1;
    aTempTable->mCurRange = aTempTable->mRange;

    IDE_TEST( setFirstKey( aTempTable,
                           & sResult ) != IDE_SUCCESS );
    if ( sResult == ID_TRUE )
    {
        IDE_TEST( setLastKey( aTempTable ) != IDE_SUCCESS );

        aTempTable->mIndex = aTempTable->mFirstKey;
        IDE_TEST( getElement( aTempTable,
                              aTempTable->mIndex,
                              aElement ) != IDE_SUCCESS );
    }
    else
    {
        *aElement = NULL;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmcMemSort::getNextKey( qmcdMemSortTemp * aTempTable,
                        void ** aElement )
{
/***********************************************************************
 *
 * Description :
 *    Range�� �����ϴ� ���� Record�� �˻��Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

    aTempTable->mIndex++;

    if ( aTempTable->mIndex > aTempTable->mLastKey )
    {
        *aElement = NULL;
    }
    else
    {
        IDE_TEST( getElement( aTempTable,
                              aTempTable->mIndex,
                              aElement ) != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmcMemSort::setFirstKey( qmcdMemSortTemp * aTempTable,
                         idBool          * aResult )
{
/***********************************************************************
 *
 * Description :
 *    Range�� �����ϴ� Minimum Key�� ��ġ�� �����Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

    SLong sMin;
    SLong sMax;
    SLong sMed;
    idBool sResult;
    idBool sFind;
    void * sElement;
    const void * sRow;
    qmdMtrNode * sNode;

    sMin = aTempTable->mFirstKey;
    sMax = aTempTable->mLastKey;

    sResult = ID_FALSE;
    sFind = ID_FALSE;

    // find range
    while ( aTempTable->mCurRange != NULL )
    {
        IDE_TEST( getElement( aTempTable,
                              sMin,
                              & sElement ) != IDE_SUCCESS );

        if( sElement == NULL )
        {
            break;
            // fix BUG-10503
            // sResult = ID_FALSE;
        }

        // PROJ-2362 memory temp ���� ȿ���� ����
        for( sNode = aTempTable->mSortNode;
             sNode != NULL;
             sNode = sNode->next )
        {
            IDE_TEST( aTempTable->mSortNode->func.setTuple( aTempTable->mTemplate,
                                                            sNode,
                                                            sElement )
                      != IDE_SUCCESS );
        }
        
        sRow = aTempTable->mSortNode->func.getRow( aTempTable->mSortNode,
                                                   sElement );

        /* PROJ-2334 PMT memory variable column */
        changePartitionColumn( aTempTable->mSortNode,
                               aTempTable->mCurRange->maximum.data );
                
        IDE_TEST( aTempTable->mCurRange->maximum.callback(
                      & sResult,
                      sRow,
                      NULL,
                      0,
                      SC_NULL_GRID,
                      aTempTable->mCurRange->maximum.data )
                  != IDE_SUCCESS );
        
        if ( sResult == ID_TRUE )
        {
            break;
        }
        else
        {
            aTempTable->mCurRange = aTempTable->mCurRange->next;
        }
    }

    if ( sResult == ID_TRUE )
    {
        do
        {
            sMed = (sMin + sMax) >> 1;
            IDE_TEST( getElement ( aTempTable,
                                   sMed,
                                   & sElement ) != IDE_SUCCESS );

            // PROJ-2362 memory temp ���� ȿ���� ����
            for( sNode = aTempTable->mSortNode;
                 sNode != NULL;
                 sNode = sNode->next )
            {
                IDE_TEST( aTempTable->mSortNode->func.setTuple( aTempTable->mTemplate,
                                                                sNode,
                                                                sElement )
                          != IDE_SUCCESS );
            }
        
            sRow = aTempTable->mSortNode->func.getRow( aTempTable->mSortNode,
                                                       sElement );

            /* PROJ-2334 PMT memory variable column */
            changePartitionColumn( aTempTable->mSortNode,
                                   aTempTable->mCurRange->minimum.data );
                    
            IDE_TEST( aTempTable->mCurRange->minimum.callback(
                          &sResult,
                          sRow,
                          NULL,
                          0,
                          SC_NULL_GRID,
                          aTempTable->mCurRange->minimum.data )
                      != IDE_SUCCESS );

            if ( sResult == ID_TRUE )
            {
                sMax = sMed - 1;

                /* PROJ-2334 PMT memory variable column */
                changePartitionColumn( aTempTable->mSortNode,
                                       aTempTable->mCurRange->maximum.data );

                IDE_TEST( aTempTable->mCurRange->maximum.callback(
                              &sResult,
                              sRow, 
                              NULL,
                              0,
                              SC_NULL_GRID,
                              aTempTable->mCurRange->maximum.data )
                          != IDE_SUCCESS );

                if ( sResult == ID_TRUE )
                {
                    aTempTable->mFirstKey = sMed;
                    sFind = ID_TRUE;
                }
                else
                {
                    // nothing to do
                }
            }
            else
            {
                sMin = sMed + 1;
            }
        } while ( sMin <= sMax );

        if ( sFind == ID_TRUE )
        {
            *aResult = ID_TRUE;
        }
        else
        {
            *aResult = ID_FALSE;
        }
    }
    else
    {
        *aResult = ID_FALSE;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmcMemSort::setLastKey(qmcdMemSortTemp * aTempTable)
{
/***********************************************************************
 *
 * Description :
 *     Range�� �����ϴ� Maximum Key�� ��ġ�� �����Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

    SLong sMin;
    SLong sMax;
    SLong sMed;
    idBool sResult;
    void * sElement;
    const void * sRow;
    qmdMtrNode * sNode;

    sMin = aTempTable->mFirstKey;
    sMax = aTempTable->mLastKey;

    do
    {
        sMed = (sMin + sMax) >> 1;
        IDE_TEST( getElement ( aTempTable,
                               sMed,
                               & sElement ) != IDE_SUCCESS );

        // PROJ-2362 memory temp ���� ȿ���� ����
        for( sNode = aTempTable->mSortNode;
             sNode != NULL;
             sNode = sNode->next )
        {
            IDE_TEST( aTempTable->mSortNode->func.setTuple( aTempTable->mTemplate,
                                                            sNode,
                                                            sElement )
                      != IDE_SUCCESS );
        }
        
        sRow = aTempTable->mSortNode->func.getRow( aTempTable->mSortNode,
                                                   sElement );

        /* PROJ-2334 PMT memory variable column */
        changePartitionColumn( aTempTable->mSortNode,
                               aTempTable->mCurRange->maximum.data );
        
        IDE_TEST( aTempTable->mCurRange->maximum.callback(
                      & sResult,
                      sRow, 
                      NULL,
                      0,
                      SC_NULL_GRID,
                      aTempTable->mCurRange->maximum.data )
                  != IDE_SUCCESS );

        if ( sResult == ID_TRUE )
        {
            sMin = sMed + 1;
            aTempTable->mLastKey = sMed;
        }
        else
        {
            sMax = sMed - 1;
            aTempTable->mLastKey = sMax;
        }
    } while ( sMin <= sMax );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


IDE_RC
qmcMemSort::findInsertPosition( qmcdMemSortTemp * aTempTable,
                                void            * aRow,
                                SLong           * aInsertPos )
{
/***********************************************************************
 *
 * Description :
 *    Limit Sorting���� Shift And Append�� �ϱ� ���� ��ġ��
 *    �˻��Ѵ�.
 *
 * Implementation :
 *    ������ Row���� ���ʿ� ��ġ�� ���, �� ��ġ�� �˻��Ѵ�.
 *    ���� ����� �ƴ� ��� -1 ���� �����Ѵ�.
 *
 ***********************************************************************/

    SInt sOrder;

    SLong sMin;
    SLong sMed;
    SLong sMax;
    SLong sInsertPos;

    void  ** sMedRow;
    void  ** sMaxRow;

    //------------------------------------------
    // ���� ���� Row���� ��
    //------------------------------------------

    sMax = aTempTable->mArray->index - 1;
    get( aTempTable, aTempTable->mArray, sMax, & sMaxRow );

    sOrder = compare( aTempTable, aRow, *sMaxRow );

    //------------------------------------------
    // ������ ��ġ ����
    //------------------------------------------

    sInsertPos = -1;

    if ( sOrder < 0 )
    {
        // ������ ��ġ�� �˻���.
        sMin = 0;
        do
        {
            sMed = (sMin + sMax) >> 1;
            get( aTempTable, aTempTable->mArray, sMed, & sMedRow );

            sOrder = compare( aTempTable, aRow, *sMedRow );
            if ( sOrder <= 0 )
            {
                sInsertPos = sMed;
                sMax = sMed - 1;
            }
            else
            {
                sMin = sMed + 1;
            }
        } while ( sMin <= sMax );

        IDE_DASSERT( sInsertPos != -1 );
    }
    else
    {
        // Right-Most Row���� ������ �ִ� Row��
        // ���� ����� �ƴ��� ǥ���ϰ� ��
    }

    *aInsertPos = sInsertPos;

    return IDE_SUCCESS;
}

/***********************************************************************
 *
 * Description : Proj-2334 PMT memory variable column
 *
 * Implementation :
 *     1. MEMORY PARTITIONED TABLE ��ũ, MEMORY VARIABLE ��ũ.
 *     2. columnDesc�� partition�� column�̱� ������ srcTuple column�� ����
 *        colum id�� ã�´�.
 *        (sortNode�� srcTuple�� column�� partition�� column�̴�.
 *         PCRD���� srcTuple�� columns, row������ partition ������ ���� �Ǳ� ����)
 *     3. columnDesc column id == srcTuple column id ���� �ϸ� ����.
 *     
 ***********************************************************************/
void qmcMemSort::changePartitionColumn( qmdMtrNode  * aNode,
                                        void        * aData )
{
    mtkRangeCallBack  * sData;
    mtcColumn         * sColumn;
    qmdMtrNode        * sNode;

    if ( ( ( aNode->srcTuple->lflag & MTC_TUPLE_PARTITIONED_TABLE_MASK )
           == MTC_TUPLE_PARTITIONED_TABLE_TRUE ) ||
         ( ( aNode->srcTuple->lflag & MTC_TUPLE_PARTITION_MASK )
           == MTC_TUPLE_PARTITION_TRUE ) )
    {
        for ( sData  = (mtkRangeCallBack*)aData;
              sData != NULL;
              sData  = sData->next )
        {
            /* BUG-39957 null range�� columnDesc�� �����Ǿ����� �ʴ�. */
            if ( sData->compare != mtk::compareMinimumLimit )
            {
                for ( sNode = aNode;
                      sNode != NULL;
                      sNode = sNode->next )
                {
                    sColumn = sNode->func.compareColumn;

                    if ( ( ( (sColumn->column.flag & SMI_COLUMN_TYPE_MASK)
                             == SMI_COLUMN_TYPE_VARIABLE ) ||
                           ( (sColumn->column.flag & SMI_COLUMN_TYPE_MASK)
                             == SMI_COLUMN_TYPE_VARIABLE_LARGE ) )&&
                         ( (sNode->flag & QMC_MTR_TYPE_MASK)
                           == QMC_MTR_TYPE_MEMORY_PARTITION_KEY_COLUMN ) )
                    {
                        IDE_DASSERT( sData->columnDesc.column.id == sColumn->column.id );

                        if ( sData->columnDesc.column.colSpace != sColumn->column.colSpace )
                        {
                            /*
                              idlOS::memcpy( &sData->columnDesc.column,
                              &sColumn->column,
                              ID_SIZEOF(smiColumn) );
                            */

                            /* memcpy ��� colSpace�� �ٲ۴�. */
                            sData->columnDesc.column.colSpace = sColumn->column.colSpace;
                        }
                        else
                        {
                            /* Nothing To Do */
                        }
                    }
                    else
                    {
                        /* Nothing To Do */
                    }
                }
            }
            else
            {
                /* Nothing To Do */
            }
        }
    }
    else
    {
        /* Nothing To Do */
    }

}

IDE_RC qmcMemSort::timsort( qmcdMemSortTemp * aTempTable,
                            SLong             aLow,
                            SLong             aHigh )
{
/****************************************************************************
 *
 * Description : TASK-6445
 *
 *  Timsort (Insertion Sort + Mergesort) �� �����Ѵ�.
 *
 *  1. �迭���� Run�� ã��, Run Stack�� ����
 *     1-a. �迭���� Natural Run�� �ִٸ�, �״�� Run���� ����
 *     1-b. �迭���� Natural Run�� ���ٸ�, MIN(minrun, ���� Record ����) ��ŭ��
 *          �κ� �迭�� Insertion Sort�� ���� ������ Run ����
 *
 *  2. Run Stack ���� ������ �����ϴ��� Ȯ��
 *     2-a. ���������� �����ϸ�, (1)�� ���ư��� ��� Run�� Ž��
 *     2-b. ���������� �������� ������, ������ �� ���� Stack�� Run���� Mergesort
 *
 *  �ڼ��� ������ NoK Page (http://nok.altibase.com/x/qaMRAg) �� �����Ѵ�.
 *
 ****************************************************************************/

    qmcSortRun   * sCurrRun       = NULL;
    qmcSortArray * sTempArray     = NULL;
    SLong          sMinRun        = 0;
    SLong          sRecCount      = aHigh - aLow + 1;
    SLong          sStartIdx      = 0;
    SLong          sLength        = 0;
    idBool         sIsReverseRun  = ID_FALSE;
    UShort         sState         = 0;

    //--------------------------------------------------------------
    // minrun (Run�� �� �ּ� ����) ���
    //--------------------------------------------------------------
    sMinRun = calcMinrun( sRecCount );

    //--------------------------------------------------------------
    // Run Stack ���� �Ҵ�
    // Run�� �ִ� (N/minrun) + 1 ���� ������ �� �ִ�.
    //--------------------------------------------------------------
    IDU_FIT_POINT( "qmcMemSort::timsort::malloc",
                   idERR_ABORT_InsufficientMemory );
    IDE_TEST( iduMemMgr::malloc( IDU_MEM_QMC,
                                 ( ( sRecCount / sMinRun ) + 1 ) * ID_SIZEOF(qmcSortRun),
                                 (void**) & aTempTable->mRunStack )
              != IDE_SUCCESS );
    sState = 1;

    //-------------------------------------------------
    // �ӽ� ������ ���� Temp Array�� �����.
    // ����ϴ� �ӽ� ������ (N/2) �� ���� �� ����.
    //-------------------------------------------------
    IDU_FIT_POINT( "qmcMemSort::timsort::malloc2",
                   idERR_ABORT_InsufficientMemory );
    IDE_TEST( iduMemMgr::malloc( IDU_MEM_QMC,
                                 ID_SIZEOF(qmcSortArray),
                                 (void**) & sTempArray )
              != IDE_SUCCESS );
    sState = 2;

    sTempArray->parent   = NULL;
    sTempArray->depth    = 0;
    sTempArray->numIndex = sRecCount / 2;
    sTempArray->shift    = 0;
    sTempArray->mask     = 0;
    sTempArray->index    = 0;

    IDU_FIT_POINT( "qmcMemSort::timsort::malloc3",
                   idERR_ABORT_InsufficientMemory );
    IDE_TEST( iduMemMgr::malloc( IDU_MEM_QMC,
                                 sTempArray->numIndex * ID_SIZEOF(void*),
                                 (void**) & sTempArray->elements )
              != IDE_SUCCESS );
    sState = 3;

    // Temp Table�� ����
    aTempTable->mTempArray = sTempArray;

    //--------------------------------------------------------------
    // Run Ž�� & Merging
    //--------------------------------------------------------------
    sStartIdx = aLow;

    while ( sRecCount > 0 ) 
    {
        // Run Ž��
        searchRun( aTempTable,        // Array
                   sStartIdx,         // Start Index
                   aHigh + 1,         // Fence
                   & sLength,         // Returned Length
                   & sIsReverseRun ); // Returned Order

        if ( sLength < sMinRun )
        {
            // Ž���� Run�� minrun���� ���� ���
            // �����ִ� ���ڵ� ���� vs. minrun �� �� ���� ���� Length�� ����
            sLength = IDL_MIN( sRecCount, sMinRun );

            // Run ����
            IDE_TEST( insertionSort( aTempTable,
                                     sStartIdx,
                                     sStartIdx + sLength - 1 )
                      != IDE_SUCCESS );
        }
        else
        {
            // Ž���� Run�� ����� �� ���
            if ( sIsReverseRun == ID_TRUE )
            {
                // ��ġ �ʴ� ������ �� Run�� ��� �����´�.
                reverseOrder( aTempTable, 
                              sStartIdx,                 // aLow
                              sStartIdx + sLength - 1 ); // aHigh (not fence)
            }
            else
            {
                // ���ϴ� ������� Run�� ������ ���
                // Noting to do.
            }
        }

        // [ sStartIdx, sStartIdx+sLength ) �� Run�� Stack�� �ֱ�
        sCurrRun = & aTempTable->mRunStack[aTempTable->mRunStackSize];
        sCurrRun->mStart  = sStartIdx;
        sCurrRun->mEnd    = sStartIdx + sLength - 1;
        sCurrRun->mLength = sLength;

        // Run Stack Size ����
        aTempTable->mRunStackSize++;

        // ���� Run Ž�� �غ�
        sStartIdx += sLength;
        sRecCount -= sLength;

        //----------------------------------------------------------
        // Stack ���� ������ �˻��ϰ�, ������ ������ �� ���� Merge
        //----------------------------------------------------------
        IDE_TEST( collapseStack( aTempTable ) != IDE_SUCCESS );
    }

    //--------------------------------------------------------------
    // Stack�� �����ִ� Run���� ������ Merging
    //--------------------------------------------------------------
    IDE_TEST( collapseStackForce( aTempTable ) != IDE_SUCCESS );

    // ���Ŀ��� Stack�� Run�� �ϳ��� ��� �־�� �Ѵ�.
    IDE_DASSERT( aTempTable->mRunStackSize == 1 );

    //--------------------------------------------------------------
    // �ӽ� ���� �ʱ�ȭ
    //--------------------------------------------------------------
    sState = 2;
    IDE_TEST( iduMemMgr::free( sTempArray->elements ) != IDE_SUCCESS );
    sTempArray->elements = NULL;

    sState = 1;
    IDE_TEST( iduMemMgr::free( sTempArray ) != IDE_SUCCESS );
    sTempArray = NULL;

    // Temp Table ����
    aTempTable->mTempArray = NULL;

    //--------------------------------------------------------------
    // RunStack ���� / RunStack Size �ʱ�ȭ
    //--------------------------------------------------------------
    sState = 0;
    IDE_TEST( iduMemMgr::free( aTempTable->mRunStack ) != IDE_SUCCESS );
    aTempTable->mRunStack     = NULL;
    aTempTable->mRunStackSize = 0;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    switch ( sState )
    {
        case 3:
            (void)iduMemMgr::free( sTempArray->elements );
            sTempArray->elements = NULL;
            /* fall through */
        case 2:
            (void)iduMemMgr::free( sTempArray );
            sTempArray = NULL;
            /* fall through */
        case 1:
            (void)iduMemMgr::free( aTempTable->mRunStack );
            /* fall through */
        default:
            // free() ���ο� ������� Temp Table ������ �ʱ�ȭ�ؾ� �Ѵ�.
            aTempTable->mRunStack     = NULL;
            aTempTable->mRunStackSize = 0;
            aTempTable->mTempArray    = NULL;
            break;
    }

    return IDE_FAILURE;
}

SLong qmcMemSort::calcMinrun( SLong aLength )
{
/****************************************************************************
 *
 * Description : TASK-6445
 *
 *  Timsort�� �ʿ��� Minimum Run Length (minrun)�� ����Ѵ�.
 *  ������ MINRUN_FENCE/2 <= sMinRun <= MINRUN_FENCE �̴�.
 *
 *  �⺻������, ��ü �迭�� minrun���� ���� ��
 *  Run ������ 2�� �ŵ��������� �Ǵ� minrun�� ����Ѵ�.
 *  �Ұ����� ���, Run ������ 2�� �ŵ��������� ��������
 *  �ŵ����������ٴ� ���ڶ�� ����� minrun�� ã�´�.
 *
 *  �̷��� �ؾ�, ������ Run�� ũ�Ⱑ minrun�� ���������
 *  ���� Merge �ܰ迡�� �����ϰų� ����� ũ���� Run ���� ����Ǳ� �����̴�.
 *  Mergesort ������ �� Run�� ���� ���̰� ���� �� ���� ���� ���ϰ� �����.
 *
 *  �ڼ��� ��� ���ذ� ���� ���� ������ ���� �ڼ��� ������
 *  NoK Page (http://nok.altibase.com/x/qaMRAg) �� �����Ѵ�.
 *
 * Implementation : 
 *
 *  MINRUN_FENCE�� 2^N�� ���, sQuotient�� sRemain�� �Ʒ��� ���� �����
 *  �� ���� ���ϸ� minrun�� �ȴ�.
 *
 *  - sQuotient = ���� ���� N Bit
 *  - sRemain   = ������ Bit�� �����ϸ� 1, �ƴϸ� 0
 *
 *  (e.g.) 2174 = 100001111110(2)
 *                | Q  || Re |
 *                  33 + 1    = 34
 *
 *  2174/34 = 63 2174%34 = 33 
 *  (��, ���� 34�� Run�� 63��, 32�� Run 1�� ����)
 *
 ****************************************************************************/

    SLong sQuotient = aLength;
    SLong sRemain   = 0;

    IDE_DASSERT( sQuotient > 0 );

    while ( sQuotient >= QMC_MEM_SORT_MINRUN_FENCE ) 
    {
        sRemain |= ( sQuotient & 1 );
        sQuotient >>= 1;
    }

    return sQuotient + sRemain;
}

void qmcMemSort::searchRun( qmcdMemSortTemp * aTempTable,
                            SLong             aStart,
                            SLong             aFence,
                            SLong           * aRunLength,
                            idBool          * aReverseOrder )
{
/****************************************************************************
 *
 * Description : TASK-6445
 *
 *  ���ϴ� �������� �̸� ���ĵ� Natural Run�� ã�´�.
 *  ������ ���ϴ� ������ �ݴ� ������ Run�� Natural Run���� �����Ѵ�.
 *
 *  Run�� ã�� �������� �߰� ����� ������, ����� �׷��� �ʴ�.
 *  �ǹ��ִ� Natural Run�� ã�� ���ٸ� Insertion Sort ����� �������� �����̴�.
 *
 ****************************************************************************/

    void  ** sCurrRow   = NULL;
    void  ** sNextRow   = NULL;
    SLong    sNext      = aStart + 1;

    IDE_DASSERT( aStart < aFence );

    if ( sNext == aFence )
    {
        // Run Length�� 1
        *aRunLength = 1;
    }
    else
    {
        get( aTempTable, aTempTable->mArray, sNext - 1, & sCurrRow );
        get( aTempTable, aTempTable->mArray, sNext, & sNextRow );

        // �Ʒ� if-else ������ ��ġ�� �Ǹ� ���ʿ��� �ڵ尡 �پ�� �� ������,
        // for�� �ȿ��� Reverse ���θ� �׻� üũ�ϴ� ���� ���� ���鿡�� ��ȿ�����̴�.
        if ( compare( aTempTable, *sNextRow, *sCurrRow ) >= 0 )
        {
            // ���ߴ� ����
            *aReverseOrder = ID_FALSE;
            
            // ��� �� ������ Ž���Ѵ�.
            for ( sNext = sNext + 1; sNext < aFence; sNext++ )
            {
                get( aTempTable, aTempTable->mArray, sNext - 1, & sCurrRow );
                get( aTempTable, aTempTable->mArray, sNext, & sNextRow );

                if ( compare( aTempTable, *sNextRow, *sCurrRow ) >= 0 )
                {
                    // Nothing to do.
                }
                else
                {
                    // ���� ������ �ݴ� ������ �ǹǷ� ���� ���´�.
                    break;
                }
            }
        }
        else
        {
            // �ݴ� ����
            *aReverseOrder = ID_TRUE;

            // ��� �� ������ Ž���Ѵ�.
            for ( sNext = sNext + 1; sNext < aFence; sNext++ )
            {
                get( aTempTable, aTempTable->mArray, sNext - 1, & sCurrRow );
                get( aTempTable, aTempTable->mArray, sNext, & sNextRow );

                if ( compare( aTempTable, *sNextRow, *sCurrRow ) > 0 )
                {
                    // ���� ������ �ݴ� ������ �ǹǷ� ���� ���´�.
                    break;
                }
                else
                {
                    // Nothing to do.
                }
            }
        }

        // Run Length ����
        *aRunLength = sNext - aStart;
    }

}

IDE_RC qmcMemSort::insertionSort( qmcdMemSortTemp * aTempTable,
                                  SLong             aLow, 
                                  SLong             aHigh )
{
/****************************************************************************
 *
 * Description : 
 *
 *  [aLow, aHigh] ���� Insertion Sort�� �����Ѵ�.
 *  2���� �˰��� ���, ������ ��쿡 ���� �� �Լ��� �ʿ�� �Ѵ�.
 *
 *  - Quicksort������ Partition�� ���̰� QMC_MEM_SORT_SMALL_SIZE ���� ���� ���,
 *    Partitioning�� �ϴ� �ͺ��� ���� �����ϴ� ���� �� ����.
 *
 *  - Timsort������ Natural Run ���̰� minrun���� ���ų� Natural Run�� ���� ���,
 *    minrun ���̸�ŭ ���Ľ��� Run�� �����ؾ� �Ѵ�.
 *
 ****************************************************************************/

    void  ** sLowRow   = NULL;
    void  ** sHighRow  = NULL;
    void  ** sPivotRow = NULL;
    void   * sTempRow  = NULL;
    SLong    sStart    = 0;
    SLong    sTemp     = 0;
    SInt     sOrder    = 0;

    for ( sStart = aLow + 1; sStart <= aHigh; sStart++ )
    {
        get( aTempTable, aTempTable->mArray, sStart, &sPivotRow );

        sTempRow = *sPivotRow;

        for ( sTemp = sStart - 1; sTemp >= aLow; --sTemp )
        {
            get( aTempTable, aTempTable->mArray, sTemp, &sLowRow );
            get( aTempTable, aTempTable->mArray, sTemp + 1, &sHighRow );
            sOrder = compare( aTempTable, *sLowRow, sTempRow ); 

            if ( sOrder > 0 )
            {
                *sHighRow = *sLowRow;
            }
            else
            {
                break;
            }
        }
        get( aTempTable, aTempTable->mArray, sTemp + 1, &sHighRow );
        
        *sHighRow = sTempRow;
    }
    
    return IDE_SUCCESS;
}

void qmcMemSort::reverseOrder( qmcdMemSortTemp * aTempTable,
                               SLong             aLow, 
                               SLong             aHigh )
{
/****************************************************************************
 *
 * Description : TASK-6445
 *
 *  [aLow, aHigh] �� ���� ������ �ٲ۴�.
 *  ���ϴ� ����� �������� Run�� ������� ȣ���Ѵ�.
 *
 * Implementation :
 *
 *  �� ���ܺ��� ������ �߽����� ���鼭, ������ ���� �ٲ㳪����.
 *
 ****************************************************************************/

    void  ** sLowRow  = NULL;
    void  ** sHighRow = NULL;
    void   * temp     = NULL;
    SLong    sLow     = aLow;
    SLong    sHigh    = aHigh;

    while ( sLow < sHigh )
    {
        get( aTempTable, aTempTable->mArray, sLow, & sLowRow );
        get( aTempTable, aTempTable->mArray, sHigh, & sHighRow );
        QMC_MEM_SORT_SWAP( *sLowRow, *sHighRow );

        sLow++;
        sHigh--;
    }

}

IDE_RC qmcMemSort::collapseStack( qmcdMemSortTemp * aTempTable )
{
/****************************************************************************
 *
 * Description : TASK-6445
 *
 *  Run Stack�� �ִ� Run���� Merge �������� �˻��Ѵ�.
 *
 *  Stack�� ������ 3�� ( ���� ���� ������� A, B, C )�� A <= B + C �� ������ ��
 *   - A <  C --> A�� B�� Merge
 *   - A >= C --> B�� C�� Merge
 *
 *  �� ���� ��� ( Stack�� Run�� 2��, �Ǵ� A > B + C )��
 *   - B <= C --> B�� C�� Merge
 *
 * Implementation :  
 *
 *  �������� B�� ���Ѵ�.
 *  mergeRuns() �� '������'�� '������ ���� Run'�� Merge ��Ų��.
 *
 *  ������ 3���� �����ϴ� ��Ȳ���� A <= B + C ���,
 *  A, C�� ��Һ񱳸� �� ���� ���� ������ Merge�Ѵ�.
 *
 *   - A >= C : B�� C�� �Ѵ�. ( ������ B �״�� mergeRuns() )
 *   - A <  C : A�� B�� �Ѵ�. ( ������ B���� A�� �ű� �� mergeRuns() )
 *
 *  2���� ���Ұų� �� ������ �������� ������,
 *  B <= C�� ��Ȳ������ �������� B �״�� �ΰ� mergeRuns() �� ȣ���Ѵ�.
 *
 * Note :
 *  
 *  Merge ��� Run�� ���� �����ִ� ������ Run�� ũ�� ���̰�
 *  ���� �۰� �Ͼ�� ������ �����ϴ� ���̴�. ( �� : 30, 20, 10 => 30, 30 )
 *  
 *  Merge ���ǿ� ���� �ڼ��� ������
 *  NoK Page (http://nok.altibase.com/x/qaMRAg) �� �����Ѵ�.
 *
 ****************************************************************************/

    qmcSortRun * sRunStack;
    SInt         sRunStackIdx = 0;

    sRunStack    = aTempTable->mRunStack;

    while ( aTempTable->mRunStackSize > 1 )
    {
        // �ֱ� 3���� Run�� R[n-1], R[n], R[n+1] �� ����
        sRunStackIdx = aTempTable->mRunStackSize - 2;

        IDE_DASSERT( sRunStackIdx >= 0 );

        // 3�� ��� �����ϴ� ���
        if ( sRunStackIdx > 0 )
        {
            // R[n-1].length <= R[n].length + R[n+1].length�� �����ϸ�
            if ( sRunStack[sRunStackIdx - 1].mLength <=
                 ( sRunStack[sRunStackIdx].mLength + sRunStack[sRunStackIdx + 1].mLength ) )
            {
                // R[n-1].length < R[n+1].length
                if ( sRunStack[sRunStackIdx - 1].mLength < sRunStack[sRunStackIdx + 1].mLength )
                {
                    // Merge ���� �ε����� �Űܼ� R[n-1]�� R[n]�� Merge
                    sRunStackIdx--;
                }
                else
                {
                    // Merge ���� �ε����� �ű��� �ʰ� R[n]�� R[n+1]�� Merge
                }

                // merge
                IDE_TEST( mergeRuns( aTempTable, sRunStackIdx ) != IDE_SUCCESS );

                // while�� ������ �ٽ� �˻��ϰ� �Ѵ�.
                continue;
            }
            else
            {
                // ���� ������ �� �Ǵ� ���, �Ʒ� ������ �˻��Ѵ�.
                // Nothing to do.
            }
        }
        else
        {
            // Run�� 2�� ������ ���, �Ʒ� ������ �˻��Ѵ�.
            // Nothing to do.
        }

        // R[n].length <= R[n+1].length
        if ( sRunStack[sRunStackIdx].mLength <= sRunStack[sRunStackIdx + 1].mLength )
        {
            // merge
            IDE_TEST( mergeRuns( aTempTable, sRunStackIdx ) != IDE_SUCCESS );
        }
        else
        {
            // Merge�� �����ϰ�, Stack�� �����Ѵ�.
            break;
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmcMemSort::collapseStackForce( qmcdMemSortTemp * aTempTable )
{
/****************************************************************************
 *
 * Description : TASK-6445
 *
 *  Timsort Run Stack�� �ִ� Run���� ������ Merge�Ѵ�.
 *  ��Ģ�� �����ϳ�, ���� ����ȭ �� �����̴�.
 *
 * Implementation :
 *
 *  while() �� �ȿ��� 1���� ���� �� ���� �Ʒ� mergeRuns()�� ȣ��ȴ�.
 *  3���� ���� ��Ȳ������ A, C�� ��Һ񱳸� �� ���� ���� ������ Merge�Ѵ�.
 *   - A >= C : B�� C�� �Ѵ�. ( ������ B �״�� mergeRuns() )
 *   - A <  C : A�� B�� �Ѵ�. ( ������ B���� A�� �ű� �� mergeRuns() )
 *
 *  2���� ���� ��Ȳ������ 2���� ��ٷ� Merge�Ѵ�.
 *
 ****************************************************************************/

    qmcSortRun * sRunStack;
    SInt         sRunStackIdx = 0;

    sRunStack    = aTempTable->mRunStack;

    while ( aTempTable->mRunStackSize > 1 )
    {
        // ���� 3���� Run�� R[n-1], R[n], R[n+1] �� ����
        sRunStackIdx = aTempTable->mRunStackSize - 2;

        IDE_DASSERT( sRunStackIdx >= 0 );

        if ( ( sRunStackIdx > 0 ) &&
             ( sRunStack[sRunStackIdx - 1].mLength < sRunStack[sRunStackIdx + 1].mLength ) )
        {
                sRunStackIdx--;
        }
        else
        {
            // Nothing to do.
        }

        // merge
        IDE_TEST( mergeRuns( aTempTable, sRunStackIdx ) != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmcMemSort::mergeRuns( qmcdMemSortTemp * aTempTable,
                              UInt              aRunStackIdx )
{
/****************************************************************************
 *
 * Description : TASK-6445
 *
 *  �� ���� Run (A)�� Run (B) �� Mergesort�� �����Ѵ�.
 *
 * Implementation : 
 *
 *  Mergesort ����, �� Run���� ������ �ʿ䰡 ���� �κ��� �ǳʶٵ��� �Ѵ�.
 *
 *  Mergesort�� �������� �ʰ� �� �ڸ��� ��� ���� �� �ִ� ���Ҵ�,
 *  (�������� ��������)
 *
 *  - A�� ���� ū �� (= ���� �������� ��)���� �� ũ�ų� ���� B�� ���ҵ�
 *  - B�� ���� ���� �� (=���� ������ ��)���� �� �۰ų� ���� A�� ���ҵ�
 *
 *    [A]                       [B]
 *                     (max)    (min)
 *    ----------------------    -----------------------
 *    | 1 | 3 | 5 | 7 | 10 |    | 4 | 6 | 8 | 10 | 12 |
 *    ----------------------    -----------------------
 *          ^                                 ^
 *          ������� 4���� ����               ������� 10���� ũ�ų� ����
 *
 *     (gallopRight->) [A']           [B']           (<-gallopLeft)
 *     ---------       -------------- -------------  -----------
 *  => | 1 | 3 |       | 5 | 7 | 10 | | 4 | 6 | 8 |  | 10 | 12 |
 *     ---------       -------------- -------------  -----------
 *
 *  => (A'�� B'�� Mergesort�� ����)
 *
 ****************************************************************************/

    void       ** sGallopRow = NULL;
    qmcSortRun  * sRun1;
    qmcSortRun  * sRun2;
    qmcSortRun  * sRun3      = NULL;
    SLong         sBase1;
    SLong         sLen1;
    SLong         sBase2;
    SLong         sLen2;
    SLong         sGallopCnt = 0;

    sRun1  = & aTempTable->mRunStack[aRunStackIdx];
    sRun2  = & aTempTable->mRunStack[aRunStackIdx + 1];
    sBase1 = sRun1->mStart;
    sLen1  = sRun1->mLength;
    sBase2 = sRun2->mStart;
    sLen2  = sRun2->mLength;

    // Stack ���� ����
    sRun1->mLength = sLen1 + sLen2;

    // A�� Stack�� (������) 3��° Run�̾��ٸ�
    // �߰����� Merge�� �Ͼ�� ���̹Ƿ� ������ Run (C) ������ �����ؾ� �Ѵ�.
    // B�� ������ C�� ������ ���� ����. (B�� ��� �Ϸ��)
    if ( aRunStackIdx == aTempTable->mRunStackSize - 3 )
    {
        sRun3 = & aTempTable->mRunStack[aRunStackIdx + 2];
        sRun2->mStart  = sRun3->mStart;
        sRun2->mEnd    = sRun3->mEnd;
        sRun2->mLength = sRun3->mLength;
    }
    else
    {
        // Nothing to do.
    }

    // RunStack Size ����
    aTempTable->mRunStackSize--;
    
    //-------------------------------------------------------
    // Trimming
    //-------------------------------------------------------

    // Right Galloping
    get( aTempTable, 
         aTempTable->mArray, 
         sBase2, 
         & sGallopRow );

    sGallopCnt = gallopRight( aTempTable, 
                              aTempTable->mArray,
                              sGallopRow, 
                              sBase1, 
                              sLen1, 
                              0 );

    sBase1 += sGallopCnt;
    sLen1  -= sGallopCnt;
    IDE_TEST_CONT( sLen1 == 0, NO_MORE_MERGE );

    // Left Galloping
    get( aTempTable, 
         aTempTable->mArray, 
         ( sBase1 + ( sLen1 - 1 ) ), 
         & sGallopRow );

    sLen2 = gallopLeft( aTempTable, 
                        aTempTable->mArray,
                        sGallopRow, 
                        sBase2, 
                        sLen2, 
                        sLen2 - 1 );

    IDE_TEST_CONT( sLen2 == 0, NO_MORE_MERGE );

    //-------------------------------------------------------
    // Merging
    //-------------------------------------------------------

    if ( sLen1 <= sLen2 )
    {
        IDE_TEST( mergeLowerRun( aTempTable,
                                 sBase1,
                                 sLen1,
                                 sBase2,
                                 sLen2 ) 
                  != IDE_SUCCESS );
    }
    else
    {
        IDE_TEST( mergeHigherRun( aTempTable,
                                  sBase1,
                                  sLen1,
                                  sBase2,
                                  sLen2 ) 
                  != IDE_SUCCESS );
    }

    IDE_EXCEPTION_CONT( NO_MORE_MERGE );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

SLong qmcMemSort::gallopLeft( qmcdMemSortTemp  * aTempTable,
                              qmcSortArray     * aArray,
                              void            ** aKey,
                              SLong              aBase,
                              SLong              aLength,
                              SLong              aHint )
{
/****************************************************************************
 *
 * Desciption : TASK-6445
 *
 *  Run�� [aBase, aBase+aLength) ��������,
 *  ���� ���� �� aKey�� ��ġ�ؾ� �ϴ� �ε����� ��ȯ�Ѵ�.
 *  �� �ε����� aBase������ ����� ��ġ�� ��ȯ�ȴ�.
 *  
 *  ���� ���������̶��
 *  a[aBase+Ret-1] < key <= a[aBase+Ret] �� �����ϴ� Ret�� ��ȯ�Ѵ�.
 *
 *  ���⼭ Left�� '���� ����' �̶� �ǹ̵� ������,
 *  ���� ���� �����ϴ� ��� ���� '����'�� ��ġ�ϴ� �ε����� ���϶�� �ǹ��̴�.
 *
 * Implementation :
 *
 *  ��Ʈ(Hint) ��ġ���� �����Ѵ�.
 *  Galloping �� ����, 1ĭ�� �ǳʶ����ʰ�, 2^n-1ĭ�� �ǳ� �ڴ�.
 *
 *   - a[Hint] -> Key ������ �������̸�,
 *     ������ ������ �� ���� ���������� Galloping
 *     > ASC  : if ( a[Hint] < Key ) searchCond = ( Key <= a[Hint + Offset] )
 *     > DESC : if ( a[Hint] > Key ) searchCond = ( Key >= a[Hint + Offset] )
 *
 *   - a[Hint] -> Key ������ '���� ���̰ų�' �������� ���,
 *     ������ ������ �� ���� �������� Galloping
 *     > ASC  : if ( a[Hint] >= Key ) searchCond = ( a[Hint - Offset] < Key )
 *     > DESC : if ( a[Hint] <= Key ) searchCond = ( a[Hint - Offset] > Key )
 *
 *  Galloping�� �Ϸ�Ǹ�, (���� Offset - ���� Offset) ���� �ȿ���
 *  Binary Search�� �ؼ� ��Ȯ�� ��ġ�� ã�´�.
 *
 *  �ڼ��� ������ NoK ������ ���� (http://nok.altibase.com/x/qaMRAg)
 *
 ****************************************************************************/

    void ** sGallopRow = NULL;
    SLong   sStartIdx  = aBase + aHint;
    SLong   sIdx       = 0;
    SLong   sLastIdx   = 0;
    SLong   sMaxIdx    = 0;
    SLong   sTempIdx   = 0;

    get( aTempTable, aArray, sStartIdx, & sGallopRow );

    if ( compare( aTempTable, *aKey, *sGallopRow ) > 0 )
    {
        // a[Hint] -> Key ������ ������ => ���������� Galloping
        // sMaxIdx / sIdx / sLastIdx�� sStartIdx������ ��� �ε����̴�.
        sMaxIdx  = aLength - aHint;
        sLastIdx = 0;
        sIdx     = 1;

        while ( sIdx < sMaxIdx )
        {
            get( aTempTable, aArray, ( sStartIdx + sIdx ), & sGallopRow );

            if ( compare( aTempTable, *aKey, *sGallopRow ) > 0 )
            {
                // ��� ������ ������
                sLastIdx = sIdx;

                // 2^n-1 ��ŭ Index ����
                sIdx = ( sIdx << 1 ) + 1;

                // overflow ó��
                if ( sIdx <= 0 )
                {
                    sIdx = sMaxIdx;
                }
                else
                {
                    // Nothing to do.
                }
            }
            else
            {
                // ó������ ���� ���̰ų� ������ �������� ���� ����
                break;
            }
        }

        // sIdx�� sMaxIdx�� ����
        if ( sIdx > sMaxIdx )
        {
            sIdx = sMaxIdx;
        }
        else
        {
            // Nothing to do.
        }

        // sLastIdx / sIdx�� ��ġ�� ������ġ�� ����
        sLastIdx += sStartIdx;
        sIdx     += sStartIdx;
    }
    else
    {
        // a[Hint] -> Key ������ �������̰ų�, ���� �� => �������� Galloping
        // sMaxIdx / sIdx / sLastIdx�� sStartIdx������ ��� �ε����̴�.
        sMaxIdx  = aHint + 1;
        sLastIdx = 0;
        sIdx     = 1;

        while ( sIdx < sMaxIdx )
        {
            get( aTempTable, aArray, ( sStartIdx - sIdx ), & sGallopRow );

            if ( compare( aTempTable, *aKey, *sGallopRow ) > 0 )
            {
                // ó������ ������ �������� ���� ����
                break;
            }
            else
            {
                // ��� ���� ���ų� ������
                sLastIdx = sIdx;

                // 2^n-1 ��ŭ Index ����
                sIdx = ( sIdx << 1 ) + 1;

                // overflow ó��
                if ( sIdx <= 0 )
                {
                    sIdx = sMaxIdx;
                }
                else
                {
                    // Nothing to do.
                }
            }
        }

        // sIdx�� sMaxIdx�� ����
        if ( sIdx > sMaxIdx )
        {
            sIdx = sMaxIdx;
        }
        else
        {
            // Nothing to do.
        }

        // sLastIdx / sIdx�� ��ġ�� ������ġ�� ����
        // �� ��, sLastIdx < sIdx�� �ǵ��� �ٲ۴�.
        sTempIdx = sLastIdx;
        sLastIdx = sStartIdx - sIdx;
        sIdx     = sStartIdx - sTempIdx;
    }

    // �������� ���� ���������� ����,
    // ��Ȯ�� ������ ã�� ���� sLastIdx -> sIdx �� Binary Search
    sLastIdx++;
    while ( sLastIdx < sIdx )
    {
        sTempIdx = ( sIdx + sLastIdx ) / 2;
        get( aTempTable, aArray, sTempIdx, & sGallopRow );

        if ( compare( aTempTable, *aKey, *sGallopRow ) > 0 )
        {
            // ���Ұ� Key���� �������� ���
            // �ش� ��ġ�� End
            sLastIdx = sTempIdx + 1;
        }
        else
        {
            // ���Ұ� Key�� ���ų� �������� ���
            // �ش� ��ġ ������ Start
            sIdx = sTempIdx;
        }
    }

    IDE_DASSERT( sLastIdx == sIdx );
    IDE_DASSERT( sIdx - aBase <= aLength );

    // �ε������� aBase�� �����Ѵ�.
    return ( sIdx - aBase );
}

SLong qmcMemSort::gallopRight( qmcdMemSortTemp  * aTempTable,
                               qmcSortArray     * aArray,
                               void            ** aKey,
                               SLong              aBase,
                               SLong              aLength,
                               SLong              aHint )
{
/****************************************************************************
 *
 * Desciption : TASK-6445
 * 
 *  Run�� [aBase, aBase+aLength) ��������,
 *  ���� ���� �� key�� ��ġ�ؾ� �ϴ� �ε����� ��ȯ�Ѵ�.
 *  �� �ε����� aBase������ ����� ��ġ�� ��ȯ�ȴ�.
 *
 *  ���� ���������̶��
 *  a[aBase+Ret-1] <= key < a[aBase+Ret] �� �����ϴ� Ret�� ��ȯ�Ѵ�.
 *
 *  ���⼭ Right�� '������ ����' �̶� �ǹ̵� ������,
 *  ���� ���� �����ϴ� ��� ���� '������'�� ��ġ�ϴ� �ε����� ���϶�� �ǹ��̴�.
 *
 * Implementation :
 *
 *  gallopLeft()�� �� ���Ǹ� �ٸ���.
 *
 *   - a[Hint] -> Key ������ '���� ���̰ų�' �������� ���,
 *     ������ ������ �� ���� ���������� Galloping
 *     > ASC  : if ( a[Hint] <= Key ) searchCond = ( Key < a[Hint + Offset] )
 *     > DESC : if ( a[Hint] >= Key ) searchCond = ( Key > a[Hint + Offset] )
 *
 *   - a[Hint] -> Key ������ �������̸�,
 *     ������ ������ �� ���� �������� Galloping
 *     > ASC  : if ( a[Hint] > Key ) searchCond = ( a[Hint - Offset] <= Key )
 *     > DESC : if ( a[Hint] < Key ) searchCond = ( a[Hint - Offset] >= Key )
 *
 *  Galloping�� �Ϸ�Ǹ�, (���� Offset - ���� Offset) ���� �ȿ���
 *  Binary Search�� �ؼ� ��Ȯ�� ��ġ�� ã�´�.
 *
 *  �ڼ��� ������ NoK ������ ���� (http://nok.altibase.com/x/qaMRAg)
 *
 * Note :
 *
 *  gallopLeft() �� �ſ� ����������, �ڵ带 ������ ��� ���������� ����� �Ӵ���
 *  Loop ���ο� Branch�� �������Ƿ� ���� ���ϸ� �����´�.
 *
 *  ����, ���� �Լ��� �ۼ��ߴ�.
 *
 ****************************************************************************/

    void ** sGallopRow = NULL;
    SLong   sStartIdx  = aBase + aHint;
    SLong   sIdx       = 0;
    SLong   sLastIdx   = 0;
    SLong   sMaxIdx    = 0;
    SLong   sTempIdx   = 0;

    get( aTempTable, aArray, sStartIdx, & sGallopRow );

    if ( compare( aTempTable, *aKey, *sGallopRow ) >= 0 )
    {
        // a[Hint] -> Key ������ �������̰ų�, ���� �� => ���������� Galloping
        // sMaxIdx / sIdx / sLastIdx�� sStartIdx������ ��� �ε����̴�.
        sMaxIdx  = aLength - aHint;
        sLastIdx = 0;
        sIdx     = 1;

        while ( sIdx < sMaxIdx )
        {
            get( aTempTable, aArray, ( sStartIdx + sIdx ), & sGallopRow );

            if ( compare( aTempTable, *sGallopRow, *aKey ) > 0 )
            {
                // ó������ ������ �������� ���� ����
                break;
            }
            else
            {
                // ��� ������ �������̰ų�, ���� ��
                sLastIdx = sIdx;

                // 2^n-1 ��ŭ Index ����
                sIdx = ( sIdx << 1 ) + 1;

                // overflow ó��
                if ( sIdx <= 0 )
                {
                    sIdx = sMaxIdx;
                }
                else
                {
                    // Nothing to do.
                }
            }
        }

        // sIdx�� sMaxIdx�� ����
        if ( sIdx > sMaxIdx )
        {
            sIdx = sMaxIdx;
        }
        else
        {
            // Nothing to do.
        }

        // sLastIdx / sIdx�� ��ġ�� ������ġ�� ����
        sLastIdx += sStartIdx;
        sIdx     += sStartIdx;
    }
    else
    {
        // a[Hint] -> Key ������ ������ => �������� Galloping
        // sMaxIdx / sIdx / sLastIdx�� sStartIdx������ ��� �ε����̴�.
        sMaxIdx  = aHint + 1;
        sLastIdx = 0;
        sIdx     = 1;

        while ( sIdx < sMaxIdx )
        {
            get( aTempTable, aArray, ( sStartIdx - sIdx ), & sGallopRow );

            if ( compare( aTempTable, *sGallopRow, *aKey ) > 0 )
            {
                // ��� ������
                sLastIdx = sIdx;

                // Gallop(2^n-1)
                sIdx = ( sIdx << 1 ) + 1;

                // overflow ó��
                if ( sIdx <= 0 )
                {
                    sIdx = sMaxIdx;
                }
                else
                {
                    // Nothing to do.
                }
            }
            else
            {
                // ó������ ���� ���ų� ������ �������� ���� ����
                break;
            }
        }

        // sIdx�� sMaxIdx�� ����
        if ( sIdx > sMaxIdx )
        {
            sIdx = sMaxIdx;
        }
        else
        {
            // Nothing to do.
        }

        // sLastIdx / sIdx�� ��ġ�� ������ġ�� ����
        // �� ��, sLastIdx < sIdx�� �ǵ��� �ٲ۴�.
        sTempIdx = sLastIdx;
        sLastIdx = sStartIdx - sIdx;
        sIdx     = sStartIdx - sTempIdx;
    }

    // �������� ���� ���������� ����,
    // ��Ȯ�� ������ ã�� ���� sLastIdx -> sIdx �� Binary Search
    sLastIdx++;
    while ( sLastIdx < sIdx )
    {
        sTempIdx = ( sIdx + sLastIdx ) / 2;
        get( aTempTable, aArray, sTempIdx, & sGallopRow );

        if ( compare( aTempTable, *sGallopRow, *aKey ) > 0 )
        {
            // ���Ұ� Key���� �������� ���
            // �ش� ��ġ�� End
            sIdx = sTempIdx;
        }
        else
        {
            // ���Ұ� Key�� ���ų� �������� ���
            // �ش� ��ġ ������ Start
            sLastIdx = sTempIdx + 1;
        }
    }

    IDE_DASSERT( sLastIdx == sIdx );
    IDE_DASSERT( sIdx - aBase <= aLength );

    // �ε������� aBase�� �����Ѵ�.
    return ( sIdx - aBase );
}

IDE_RC qmcMemSort::mergeLowerRun( qmcdMemSortTemp * aTempTable,
                                  SLong             aBase1,
                                  SLong             aLen1,
                                  SLong             aBase2,
                                  SLong             aLen2 )
{
/*******************************************************************
 *
 * Description : TASK-6445
 *
 *  ù ��° Run�� Left, �� ��° Run�� Right��� �� ��
 *  Left�� Right���� ���� ��쿡 ����Ǵ� �Լ��̴�.
 *
 *  Left L�� �ӽ� ���� L'�� �����ϰ�, ���� ������ L'�� Right R�� Mergesort�Ѵ�.
 *  �� �˰����� In-place Sorting �̹Ƿ� ���� ������ ���Ľ��Ѿ� �Ѵ�.
 *
 *  --------
 *  |  L'  | (Temp)
 *  --------                           ---------------------
 *     ^ Copy             => Merge =>  |  Merged Array(M)  |
 *  --------------------               ---------------------
 *  |  L   |     R     |
 *  --------------------
 *
 * Implementation : 
 *
 *  Merging �� Galloping���� ����, �Ʒ� Ư¡�� Ȱ���� �� �ִ�.
 *
 *  - R�� ù ��° ���Ҵ� �׻� M�� ù ��° ���Ұ� �ȴ�.
 *  - L�� ������ ���Ҵ� �׻� M�� ������ ���Ұ� �ȴ�.
 *
 *  ���� ������ �����̴�.
 *  �Ʒ��� �� �ε����� ���� ������ ���������� �̵��Ѵ�.
 *
 *  - sTargetIdx  : ���� ���� ����� ���� �ϴ� ��ġ
 *  - sLeftIdx    : L���� ���ĵ��� ���� ���� ��ġ
 *  - sRightIdx   : R���� ���ĵ��� ���� ���� ��ġ
 *
 *  �� �ε����� Fence�� ������ ���� ��踦 �����.
 *
 *  - TargetFence�� Right�� Fence�� �����ϴ�.
 *
 *  Galloping Mode ���� ������ GALLOPING_WINCNT�� �����ȴ�.
 *  Galloping Mode ���� ������ sMinToGallopMode�� �����Ѵ�.
 *
 *******************************************************************/

    qmcSortArray  * sLeftArray    = NULL;
    void         ** sLeftRows     = NULL;
    void         ** sTargetRow    = NULL;
    void          * sLeftRow      = NULL;
    void         ** sRightRow     = NULL;
    SLong           sTargetIdx    = aBase1;
    SLong           sLeftIdx      = 0;
    SLong           sLeftFence    = aLen1;
    SLong           sRightIdx     = aBase2;
    SLong           sRightFence   = aBase2 + aLen2;
    SLong           sLeftWinCnt   = 0;
    SLong           sRightWinCnt  = 0;
    SInt            sMinGalloping = QMC_MEM_SORT_GALLOPING_WINCNT;
    
    //-------------------------------------------------
    // L' ����
    //-------------------------------------------------

    sLeftArray = aTempTable->mTempArray;
    sLeftRows  = (void **)sLeftArray->elements;

    //-------------------------------------------------
    // L�� L'�� ����
    //-------------------------------------------------

    IDE_TEST( moveArrayToArray( aTempTable,
                                aTempTable->mArray, // SrcArr
                                aBase1,             // SrcBase
                                sLeftArray,         // DestArr
                                0,                  // DestBase
                                aLen1,              // MoveLength
                                ID_FALSE )          // IsOverlapped
              != IDE_SUCCESS );

    //--------------------------------------------------
    // Row �غ�, ù ���� ����
    //--------------------------------------------------

    // L / R�� ù Row �غ�
    sLeftRow = sLeftRows[sLeftIdx];
    get( aTempTable, aTempTable->mArray, sRightIdx, & sRightRow );

    // Target Row �غ�
    get( aTempTable, aTempTable->mArray, sTargetIdx, & sTargetRow );
    sTargetIdx++;

    // L'�� ���Ұ� �ϳ��� �ִ� ���, �ٷ� �ǳʶڴ�.
    IDE_TEST_CONT( aLen1 == 1, FINISH_LINE );

    // R�� ù ��° ���Ҵ� ������ ù ���Ұ� �ǹǷ�, �̸� ����ִ´�.
    IDE_DASSERT( compare( aTempTable, *sTargetRow, *sRightRow ) > 0 );
    *sTargetRow = *sRightRow;
    sRightIdx++;

    // ���� R�� Row �غ�
    get( aTempTable, aTempTable->mArray, sRightIdx, & sRightRow );

    //--------------------------------------------------
    // Ž�� ����
    //--------------------------------------------------

    while ( ID_TRUE )
    {
        //----------------------------------------
        // Normal Mode
        // �� �Ǿ� ���ذ��鼭 �����Ѵ�.
        //----------------------------------------
        while ( ID_TRUE )
        {
            get( aTempTable, aTempTable->mArray, sTargetIdx, & sTargetRow );
            sTargetIdx++;

            if ( compare( aTempTable, *sRightRow, sLeftRow ) >= 0 )
            {
                // L'�� ���� ���ĵǾ�� �ϴ� ���
                *sTargetRow = sLeftRow;
                sLeftIdx++;
                sLeftWinCnt++;
                sRightWinCnt = 0;

                if ( ( sLeftIdx + 1 ) < sLeftFence )
                {
                    // ���� L'�� Row �غ�
                    sLeftRow = sLeftRows[sLeftIdx];

                    if ( sLeftWinCnt >= sMinGalloping )
                    {
                        // Galloping Mode�� ��ȯ
                        break;
                    }
                    else
                    {
                        // Nothing to do.
                    }
                }
                else
                {
                    // L'�� �������� ���� ����̰ų� ��� ������ ��� ����
                    IDE_CONT( FINISH_LINE );
                }
            }
            else
            {
                // R�� ���� ���ĵǾ�� �ϴ� ���
                *sTargetRow = *sRightRow;
                sRightIdx++;
                sRightWinCnt++;
                sLeftWinCnt = 0;

                if ( sRightIdx < sRightFence )
                {
                    // ���� R�� Row �غ�
                    get( aTempTable, aTempTable->mArray, sRightIdx, & sRightRow );

                    if ( sRightWinCnt >= sMinGalloping )
                    {
                        // Galloping Mode�� ��ȯ
                        break;
                    }
                    else
                    {
                        // Nothing to do.
                    }
                }
                else
                {
                    // R�� ��� ������ ��� ����
                    IDE_CONT( FINISH_LINE );
                }
            }

        } // End of while : Normal Mode ����
        
        //--------------------------------------------------------------
        // Galloping Mode
        // Normal Mode���� ���Դٸ�, ��� �� ���� ��� ���ĵǾ�� �ϴ� ���
        // �� gallop �Լ��� ����� GALLOPING_WINCNT���� ��� ���� �� ����
        // �Ʒ� ������ �ݺ��Ѵ�.
        //
        // (1) R  Row�� �������� L'�� ���� gallopRight() ����
        //     (GallopCnt) ��ŭ �迭�� �̵�
        // (2) R Row�� ������ �迭�� �̵�
        // (3) L' Row�� �������� R �� ���� gallopLeft()  ����
        //     (GallopCnt) ��ŭ �迭�� �̵�
        // (4) L' Row�� ������ �迭�� �̵�
        //--------------------------------------------------------------

        // ���� Galloping Mode ���� ���� ��ȭ
        sMinGalloping++;

        while ( ( sLeftWinCnt  >= QMC_MEM_SORT_GALLOPING_WINCNT ) || 
                ( sRightWinCnt >= QMC_MEM_SORT_GALLOPING_WINCNT ) )
        {
            // 1ȸ ������ �� ���� Galloping ���ذ��� ���ҽ�Ų��.
            // ��, GALLOPING_WINCNT_MIN ���� ���������� �ȵȴ�.
            if ( sMinGalloping > QMC_MEM_SORT_GALLOPING_WINCNT_MIN )
            {
                sMinGalloping--;
            }
            else
            {
                // Nothing to do.
            }

            // R�� ���� Row�� ��������, L'�� ���� Right Galloping
            sLeftWinCnt = gallopRight( aTempTable, 
                                       sLeftArray,                // TargetArray
                                       sRightRow,                 // Key  
                                       sLeftIdx,                  // Start
                                       ( sLeftFence - sLeftIdx ), // Length
                                       0 );                       // Hint

            if ( sLeftWinCnt > 0 )
            {
                // �ش� ������ŭ L'�� �״�� ����
                IDE_TEST( moveArrayToArray( aTempTable,
                                            sLeftArray,         // SrcArr
                                            sLeftIdx,           // SrcBase
                                            aTempTable->mArray, // DestArr
                                            sTargetIdx,         // DestBase
                                            sLeftWinCnt,        // MoveLength
                                            ID_FALSE )          // IsOverlapped
                          != IDE_SUCCESS );

                // Index ����
                sLeftIdx   += sLeftWinCnt;
                sTargetIdx += sLeftWinCnt;

                if ( ( sLeftIdx + 1 ) < sLeftFence )
                {
                    // L'�� ��ġ�� �ű��.
                    sLeftRow = sLeftRows[sLeftIdx];
                }
                else
                {
                    // L'�� �������� ���� ����̰ų� ��� ������ ��� ����
                    IDE_CONT( FINISH_LINE );
                }
            }
            else
            {
                // Nothing to do.
            }

            // R�� ���� Row�� �Ű� ��´�. (���� ���ʿ��� ������ ���ĵǾ�� �ϱ� ����)
            get( aTempTable, aTempTable->mArray, sTargetIdx, & sTargetRow );
            *sTargetRow = *sRightRow;
            sTargetIdx++;
            sRightIdx++;

            if ( sRightIdx < sRightFence )
            {
                // R�� ��ġ�� �ű��.
                get( aTempTable, aTempTable->mArray, sRightIdx, & sRightRow );
            }
            else
            {
                // R�� ��� ������ ��� ����
                IDE_CONT( FINISH_LINE );
            }

            // L'�� ���� Row�� ��������, R�� ���� Left Galloping
            sRightWinCnt = gallopLeft( aTempTable, 
                                       aTempTable->mArray,
                                       & sLeftRow,
                                       sRightIdx,
                                       ( sRightFence - sRightIdx ),
                                       ( sRightFence - sRightIdx - 1 ) );
            
            if ( sRightWinCnt > 0 )
            {
                // �ش� ������ŭ R�� �ű�
                IDE_TEST( moveArrayToArray( aTempTable,
                                            aTempTable->mArray, // SrcArr
                                            sRightIdx,          // SrcBase
                                            aTempTable->mArray, // DestArr
                                            sTargetIdx,         // DestBase
                                            sRightWinCnt,       // MoveLength
                                            ID_TRUE )           // IsOverlapped
                          != IDE_SUCCESS );

                // Index ����
                sRightIdx += sRightWinCnt;
                sTargetIdx  += sRightWinCnt;

                if ( sRightIdx < sRightFence )
                {
                    // R�� ��ġ�� �ű��.
                    get( aTempTable, aTempTable->mArray, sRightIdx, & sRightRow );
                }
                else
                {
                    // R�� ��� ������ ��� ����
                    IDE_CONT( FINISH_LINE );
                }
            }
            else
            {
                // Nothing to do.
            }

            // L'�� ���� Row�� �Ű� ��´�. (���� ���ʿ��� ������ ���ĵǾ�� �ϱ� ����)
            get( aTempTable, aTempTable->mArray, sTargetIdx, & sTargetRow );
            *sTargetRow = sLeftRow;
            sTargetIdx++;
            sLeftIdx++;

            if ( ( sLeftIdx + 1 ) < sLeftFence )
            {
                // L'�� ��ġ�� �ű��.
                sLeftRow = sLeftRows[sLeftIdx];
            }
            else
            {
                // L'�� �������� ���� ����̰ų� ��� ������ ��� ����
                IDE_CONT( FINISH_LINE );
            }

            // sTargetIdx�� sRightFence���� �۾ƾ� �Ѵ�.
            IDE_DASSERT( sRightFence > sTargetIdx );

        } // End of while : Galloping Mode ����

        // ���� Galloping Mode ���� ���� ��ȭ
        sMinGalloping++;
    }

    //-------------------------------------------
    // ������ ����
    //-------------------------------------------

    IDE_EXCEPTION_CONT( FINISH_LINE ); 

    // L'�� �����ִ� ���
    if ( sLeftIdx < sLeftFence )
    {
        // R�� �����ִٸ�, R�� ��� �ű� ������ L'�� �ű��.
        if ( sRightIdx < sRightFence )
        {
            // �� �����, L'�� �� �ϳ��� �־�� �Ѵ�.
            // �� �� �̻��� ��쿴�ٸ� ������ ó���Ǿ�� �ϱ� �����̴�.
            IDE_DASSERT( sLeftIdx + 1 == sLeftFence );

            // R�� �� ĭ ������ �ű��.
            IDE_TEST( moveArrayToArray( aTempTable,
                                        aTempTable->mArray,          // SrcArr
                                        sRightIdx,                   // SrcBase
                                        aTempTable->mArray,          // DestArr
                                        ( sRightIdx - 1 ),           // DestBase
                                        ( sRightFence - sRightIdx ), // MoveLength
                                        ID_TRUE )                    // IsOverlapped
                      != IDE_SUCCESS );

            // L'�� ������ ���Ҹ� R�� �������� �ֱ�
            sLeftRow = sLeftRows[sLeftIdx];
            get( aTempTable, aTempTable->mArray, ( sRightFence - 1 ), & sRightRow );

            *sRightRow = sLeftRow;
        }
        else
        {
            // R�� �̹� ������ �Ǿ� �ְ�, L'�� �Ϻ� ���� ��쿣,
            // L'�� ��� �ű��.
            IDE_TEST( moveArrayToArray( aTempTable,
                                        sLeftArray,                // SrcArr
                                        sLeftIdx,                  // SrcBase
                                        aTempTable->mArray,        // DestArr
                                        sTargetIdx,                // DestBase
                                        ( sLeftFence - sLeftIdx ), // MoveLength
                                        ID_FALSE )                 // IsOverlapped
                      != IDE_SUCCESS );
        }
    }
    else
    {
        // Galloping Mode�� ���� L'�� �������� ���� ���
        // �̹� ������ �Ϸ�Ǿ���.

        // Nothing to do.
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmcMemSort::mergeHigherRun( qmcdMemSortTemp * aTempTable,
                                   SLong             aBase1,
                                   SLong             aLen1,
                                   SLong             aBase2,
                                   SLong             aLen2 )
{
/*******************************************************************
 *
 * Description : TASK-6445
 *
 *  ù ��° Run�� Left, �� ��° Run�� Right��� �� ��
 *  Rightt�� Left���� ���� ��쿡 ����Ǵ� �Լ��̴�.
 *
 *  Right R�� �ӽ� ���� R'�� �����ϰ�, ���� ������ Left L�� R�� Mergesort�Ѵ�.
 *  �� �˰����� In-place Sorting �̹Ƿ� ���� ������ ���Ľ��Ѿ� �Ѵ�.
 *
 *              -------
 *       (Temp) |  R' |
 *              -------              ---------------------
 *               ^ Copy => Merge =>  |  Merged Array(M)  |
 *  -------------------              ---------------------
 *  |     L     |  R  |
 *  -------------------
 *
 * Implementation : 
 * 
 *  mergeLowerRun() �� ����������, ���� �������� ����ȴ�.
 *  �������� �ƴϹǷ� ���� �� �����ϸ�,
 *  ��� �ε����� ������ ������ �������� �̵��Ѵ�.
 *
 *  - sTargetIdx  : ���� ���� ����� ���� �ϴ� ��ġ
 *  - sLeftIdx    : L���� ���ĵ��� ���� ���� ��ġ
 *  - sRightIdx   : R���� ���ĵ��� ���� ���� ��ġ
 *
 *  �� �ε����� Fence�� ���� ���� ��踦 �����.
 *
 *  - LeftFence�� Base1�� �����ϹǷ� �״�� ����Ѵ�.
 *  - RightFence�� ������ -1�̹Ƿ� ������ �������� �ʴ´�.
 *  - TargetFence�� Left�� Fence�� �����ϴ�.
 *
 *  Galloping Mode ���� ������ GALLOPING_WINCNT�� �����ȴ�.
 *  Galloping Mode ���� ������ sMinToGallopMode�� �����Ѵ�.
 *
 *******************************************************************/

    qmcSortArray  * sRightArray   = NULL;
    void         ** sRightRows    = NULL;
    void         ** sTargetRow    = NULL;
    void         ** sLeftRow      = NULL;
    void          * sRightRow     = NULL;
    SLong           sTargetIdx    = aBase2 + aLen2 - 1;
    SLong           sLeftIdx      = aBase1 + aLen1 - 1;
    SLong           sRightIdx     = aLen2 - 1;
    SLong           sGallopIndex  = 0;
    SLong           sLeftWinCnt   = 0;
    SLong           sRightWinCnt  = 0;
    SInt            sMinToGallopMode = QMC_MEM_SORT_GALLOPING_WINCNT;

    //-------------------------------------------------
    // R' ����
    //-------------------------------------------------

    sRightArray = aTempTable->mTempArray;
    sRightRows  = (void **)sRightArray->elements;

    //-------------------------------------------------
    // R�� R'�� ����
    //-------------------------------------------------

    IDE_TEST( moveArrayToArray( aTempTable,
                                aTempTable->mArray, // SrcArr
                                aBase2,             // SrcBase
                                sRightArray,        // DestArr
                                0,                  // DestBase
                                aLen2,              // MoveLength
                                ID_FALSE )          // IsOverlapped
              != IDE_SUCCESS );

    //--------------------------------------------------
    // Row �غ�, ù ���� ����
    //--------------------------------------------------

    // L / R�� ������ Row �غ�
    get( aTempTable, aTempTable->mArray, sLeftIdx, & sLeftRow );
    sRightRow = sRightRows[sRightIdx];

    // Target Row (������ Row) �غ�
    get( aTempTable, aTempTable->mArray, sTargetIdx, & sTargetRow );
    sTargetIdx--;

    // R'�� ���Ұ� �ϳ��� �ִ� ���, �ٷ� �ǳʶڴ�.
    IDE_TEST_CONT( aLen2 == 1, FINISH_LINE );

    // L�� ������ ���Ҵ� �׻� �������� ���ĵǾ�� �ϴ� �̸� ����ִ´�.
    // Left Row�� Target Row���� �տ� ��ġ�ؾ� �Ѵ�.
    IDE_DASSERT( compare( aTempTable, *sLeftRow, *sTargetRow ) > 0 );
    *sTargetRow = *sLeftRow;
    sLeftIdx--;

    // L�� ���� Row �غ�
    get( aTempTable, aTempTable->mArray, sLeftIdx, & sLeftRow );

    //--------------------------------------------------
    // Ž�� ����
    //--------------------------------------------------

    while ( ID_TRUE )
    {
        //----------------------------------------
        // Normal Mode
        // �� �Ǿ� ���ذ��鼭 �����Ѵ�.
        //----------------------------------------
        while ( ID_TRUE )
        {
            get( aTempTable, aTempTable->mArray, sTargetIdx, & sTargetRow );
            sTargetIdx--;

            if ( compare( aTempTable, *sLeftRow, sRightRow ) >= 0 )
            {
                // L�� ���� ���ĵǾ�� �ϴ� ���
                *sTargetRow = *sLeftRow;
                sLeftIdx--;
                sLeftWinCnt++;
                sRightWinCnt = 0;

                if ( sLeftIdx >= aBase1 )
                {
                    // L�� ���� Row �غ�
                    get( aTempTable, aTempTable->mArray, sLeftIdx, & sLeftRow );

                    if ( sLeftWinCnt >= sMinToGallopMode )
                    {
                        // Galloping Mode�� ��ȯ
                        break;
                    }
                    else
                    {
                        // Nothing to do.
                    }
                }
                else
                {
                    // L�� ��� ������ ��� ����
                    IDE_CONT( FINISH_LINE );
                }
            }
            else
            {
                // R'�� ���� ���ĵǾ�� �ϴ� ���
                *sTargetRow = sRightRow;
                sRightIdx--;
                sRightWinCnt++;
                sLeftWinCnt = 0;

                if ( sRightIdx > 0 )
                {
                    // R�� ���� Row �غ�
                    sRightRow = sRightRows[sRightIdx];

                    if ( sRightWinCnt >= sMinToGallopMode )
                    {
                        // Galloping Mode�� ��ȯ
                        break;
                    }
                    else
                    {
                        // Nothing to do.
                    }
                }
                else
                {
                    // R'�� �������� ���� ����̰ų� ��� ������ ��� ����
                    IDE_CONT( FINISH_LINE );
                }
            }

        } // End of while : Normal Mode ����
        
        //--------------------------------------------------------------
        // Galloping Mode
        // Normal Mode���� ���Դٸ�, ��� �� ���� ��� ���� ���ĵǴ� ���.
        // �� gallop �Լ��� ����� GALLOPING_WINCNT���� ��� ���� �� ����
        // �Ʒ� ������ �ݺ��Ѵ�.
        //
        // (1) R' Row�� �������� L �� ���� gallopRight() ����
        //     (���� Length) - (GallopCnt) ��ŭ �� �������� �迭�� �̵�
        // (2) R' Row�� ������ �迭�� �̵�
        // (3) L  Row�� �������� R'�� ���� gallopLeft()  ����
        //     (���� Length) - (GallopCnt) ��ŭ �� �������� �迭�� �̵�
        // (4) L  Row�� ������ �迭�� �̵�
        //--------------------------------------------------------------

        // ���� Galloping Mode ���� ���� ��ȭ
        sMinToGallopMode++;

        while ( ( sLeftWinCnt  >= QMC_MEM_SORT_GALLOPING_WINCNT ) || 
                ( sRightWinCnt >= QMC_MEM_SORT_GALLOPING_WINCNT ) )
        {
            // 1ȸ ������ �� ���� Galloping ���ذ��� ���ҽ�Ų��.
            // ��, GALLOPING_WINCNT_MIN ���� ���������� �ȵȴ�.
            if ( sMinToGallopMode > QMC_MEM_SORT_GALLOPING_WINCNT_MIN )
            {
                sMinToGallopMode--;
            }
            else
            {
                // Nothing to do.
            }

            // R'�� ���� Row�� ��������, L�� ���� Right Galloping
            sGallopIndex = gallopRight( aTempTable,                
                                        aTempTable->mArray,        // TargetArray
                                        & sRightRow,               // Key  
                                        aBase1,                    // Start
                                        ( sLeftIdx - aBase1 + 1 ), // Length
                                        0 );                       // Hint

            // GallopIndex�� ����Ű�� �� '����' ���κб��� �Űܾ� �ϹǷ�
            // Winning Count�� �Ʒ��� ���� ����Ѵ�.
            // (Length) - (GallopCnt)
            sLeftWinCnt = ( sLeftIdx - aBase1 + 1 ) - sGallopIndex;

            if ( sLeftWinCnt > 0 )
            {
                // �ش� ������ŭ �ε��� ����
                // Winning Count�� ����������, 1�� �� ������ �ε����� �����Ѵ�.
                sTargetIdx -= ( sLeftWinCnt - 1 );
                sLeftIdx   -= ( sLeftWinCnt - 1 );

                // �ش� ������ŭ L�� �״�� �ű�
                IDE_TEST( moveArrayToArray( aTempTable,
                                            aTempTable->mArray, // SrcArr
                                            sLeftIdx,           // SrcBase
                                            aTempTable->mArray, // DestArr
                                            sTargetIdx,         // DestBase
                                            sLeftWinCnt,        // MoveLength
                                            ID_TRUE )           // IsOverlapped
                          != IDE_SUCCESS );

                // ��ó �� ���� 1ĭ�� �� ������.
                sTargetIdx--;
                sLeftIdx--;

                if ( sLeftIdx >= aBase1 )
                {
                    // L�� ���� Row �غ�
                    get( aTempTable, aTempTable->mArray, sLeftIdx, & sLeftRow );
                }
                else
                {
                    // L�� ��� ������ ��� ����
                    IDE_CONT( FINISH_LINE );
                }
            }
            else
            {
                // Nothing to do.
            }

            // R'�� ���� Row�� �Ű� ��´�. (���� ���ʿ��� ������ ���ĵǾ�� �ϱ� ����)
            get( aTempTable, aTempTable->mArray, sTargetIdx, & sTargetRow );
            *sTargetRow = sRightRow;
            sTargetIdx--;
            sRightIdx--;

            if ( sRightIdx > 0 )
            {
                // R'�� ���� Row �غ�
                sRightRow = sRightRows[sRightIdx];
            }
            else
            {
                // R'�� �������� ���� ����̰ų� ��� ������ ��� ����
                IDE_CONT( FINISH_LINE );
            }

            // L�� ���� Row�� ��������, R'�� ���� Left Galloping
            sGallopIndex = gallopLeft( aTempTable, 
                                       sRightArray,
                                       sLeftRow,
                                       0,
                                       ( sRightIdx + 1 ),
                                       sRightIdx );
            
            // GallopIndex�� ����Ű�� �� ���� ������ �κ��� �Űܾ� �ϹǷ�
            // Winning Count�� �Ʒ��� ���� ����Ѵ�.
            // (Length) - (GallopIndex)
            sRightWinCnt = ( sRightIdx + 1 ) - sGallopIndex;

            if ( sRightWinCnt > 0 )
            {
                // �ش� ������ŭ �ε��� ����
                // Winning Count�� ����������, 1�� �� ������ �ε����� �����Ѵ�.
                sTargetIdx -= ( sRightWinCnt - 1 );
                sRightIdx  -= ( sRightWinCnt - 1 );

                // �ش� ������ŭ R�� �ű�
                IDE_TEST( moveArrayToArray( aTempTable,
                                            sRightArray,        // SrcArr
                                            sRightIdx,          // SrcBase
                                            aTempTable->mArray, // DestArr
                                            sTargetIdx,         // DestBase
                                            sRightWinCnt,       // MoveLength
                                            ID_FALSE )          // IsOverlapped
                          != IDE_SUCCESS );

                // ��ó �� ���� 1ĭ�� �� ������.
                sTargetIdx--;
                sRightIdx--;

                if ( sRightIdx > 0 )
                {
                    // R'�� ���� Row �غ�
                    sRightRow = sRightRows[sRightIdx];
                }
                else
                {
                    // R'�� �������� ���� ����̰ų� ��� ������ ��� ����
                    IDE_CONT( FINISH_LINE );
                }
            }
            else
            {
                // Nothing to do.
            }

            // L�� ���� Row�� �Ű� ��´�. (���� ���ʿ��� ������ ���ĵǾ�� �ϱ� ����)
            get( aTempTable, aTempTable->mArray, sTargetIdx, & sTargetRow );
            *sTargetRow = *sLeftRow;
            sTargetIdx--;
            sLeftIdx--;

            if ( sLeftIdx >= aBase1 )
            {
                // L�� ���� Row �غ�
                get( aTempTable, aTempTable->mArray, sLeftIdx, & sLeftRow );
            }
            else
            {
                // L�� ��� ������ ��� ����
                IDE_CONT( FINISH_LINE );
            }

            // sTargetIdx�� aBase1���� ũ�ų� ���ƾ� �Ѵ�.
            IDE_DASSERT( sTargetIdx >= aBase1 );

        } // End of while : Galloping Mode ����

        // ���� Galloping Mode ���� ���� ��ȭ
        sMinToGallopMode++;
    }

    //-------------------------------------------
    // ������ ����
    //-------------------------------------------

    IDE_EXCEPTION_CONT( FINISH_LINE ); 

    // R'�� �����ִ� ���
    if ( sRightIdx >= 0 )
    {
        // L�� �����ִٸ�, L�� ��� �ű� ������ R'�� �ű��.
        if ( sLeftIdx >= aBase1 )
        {
            // �� �����, R'�� �� �ϳ��� �־�� �Ѵ�.
            // �� �� �̻��� ��쿴�ٸ� ������ ó���Ǿ�� �ϱ� �����̴�.
            IDE_DASSERT( sRightIdx == 0 );

            // L�� �������� �� ĭ �ڷ� �ű��.
            IDE_TEST( moveArrayToArray( aTempTable,
                                        aTempTable->mArray,        // SrcArr
                                        aBase1,                    // SrcBase
                                        aTempTable->mArray,        // DestArr
                                        ( aBase1 + 1 ),            // DestBase = SrcBase+1
                                        ( sLeftIdx - aBase1 + 1 ), // MoveLength
                                        ID_TRUE )                  // IsOverlapped
                      != IDE_SUCCESS );

            // R'�� ������ ���Ҹ� L�� ó���� �ֱ�
            sRightRow = sRightRows[sRightIdx];
            get( aTempTable, aTempTable->mArray, aBase1, & sLeftRow );

            *sLeftRow = sRightRow;
        }
        else
        {
            // L�� �̹� ������ �Ǿ� �ְ�, R'�� �Ϻ� ���� ��쿣,
            // R'�� ��� �ű��.
            // ���⼭ DestBase�� aBase1 �� ������
            // R' ������ ��� '����'���� ���� �ϱ� �����̴�.
            IDE_TEST( moveArrayToArray( aTempTable,
                                        sRightArray,        // SrcArr
                                        0,                  // SrcBase
                                        aTempTable->mArray, // DestArr
                                        aBase1,             // DestBase
                                        ( sRightIdx + 1 ),  // MoveLength
                                        ID_FALSE )          // IsOverlapped
                      != IDE_SUCCESS );
        }
    }
    else
    {
        // Galloping Mode�� ���� R'�� �������� ���� ���
        // �̹� ������ �Ϸ�Ǿ���.

        // Nothing to do.
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

//---------------------------------------------------------------
// TASK-6445 Utilities
// �˰������ ������, SortArray�� Ȱ���ϱ� ���� ���� �Լ���
//---------------------------------------------------------------

IDE_RC qmcMemSort::moveArrayToArray( qmcdMemSortTemp * aTempTable,
                                     qmcSortArray    * aSrcArray,
                                     SLong             aSrcBase,
                                     qmcSortArray    * aDestArray,
                                     SLong             aDestBase,
                                     SLong             aMoveLength,
                                     idBool            aIsOverlapped )
{
/*******************************************************************
 *
 * Description :
 *
 *  aSrcBase ���� aMoveLength ��ŭ�� elements ������ �ű��.
 *
 *  �� ������ ��ġ�� �� ���, Src�� ���� �ӽ� ������ ����� �ȴ�.
 *  �� ������ Src ������ ���� �� Dest�� �̵��Ѵ�.
 *
 ********************************************************************/

    qmcSortArray    sTempArray;
    qmcSortArray  * sBaseArray    = NULL;
    qmcSortArray  * sSrcArray     = NULL;
    qmcSortArray  * sDestArray    = NULL;
    SLong           sSrcBase      = aSrcBase;
    SLong           sDestBase     = aDestBase;
    SLong           sMoveLength   = aMoveLength;
    SLong           sSrcIdx       = 0;
    SLong           sDestIdx      = 0;
    SLong           sSrcLength    = 0;
    SLong           sDestLength   = 0;
    SLong           sLength       = 0;
    idBool          sIsSrcRemain  = ID_FALSE;
    idBool          sIsDestRemain = ID_FALSE;

    //-------------------------------------------------
    // Src�� ������ �Ǵ� Base Array ����/����
    //-------------------------------------------------

    if ( aIsOverlapped == ID_TRUE )
    {
        // ��ĥ �� �ִٸ�, �ӽ� Array�� ����
        sTempArray.parent   = NULL;
        sTempArray.depth    = 0;
        sTempArray.numIndex = aMoveLength;
        sTempArray.shift    = 0;
        sTempArray.mask     = 0;
        sTempArray.index    = 0;

        IDU_FIT_POINT( "qmcMemSort::moveArrayToArray::malloc",
                       idERR_ABORT_InsufficientMemory );
        IDE_TEST( iduMemMgr::malloc( IDU_MEM_QMC,
                                     aMoveLength * ID_SIZEOF(void*),
                                     (void**) & sTempArray.elements )
                  != IDE_SUCCESS );
        
        // ������ Array�� Src ����
        IDE_TEST( moveArrayToArray( aTempTable,
                                    aSrcArray,     // SrcArr
                                    aSrcBase,      // SrcBase
                                    & sTempArray,  // DestArr
                                    0,             // DestBase
                                    aMoveLength,   // MoveLength
                                    ID_FALSE )     // IsOverlapped
                  != IDE_SUCCESS );

        sBaseArray = & sTempArray;
        sSrcBase   = 0; 
    }
    else
    {
        sTempArray.elements = NULL;
        sBaseArray          = aSrcArray;
        sSrcBase            = aSrcBase; 
    }

    //---------------------------------------------------
    // �̵�
    //---------------------------------------------------
    
    while ( sMoveLength > 0 )
    {
        // Array ����

        if ( sIsSrcRemain == ID_FALSE )
        {
            getLeafArray( sBaseArray,
                          sSrcBase,
                          & sSrcArray,
                          & sSrcIdx );
        }
        else
        {
            // SrcArray�� ���� �� �������� ���� ���
            // sSrcBase / sSrcIdx �״�� ���
        }

        if ( sIsDestRemain == ID_FALSE )
        {
            getLeafArray( aDestArray,
                          sDestBase,
                          & sDestArray,
                          & sDestIdx );
        }
        else
        {
            // DestArray�� ���� �� ���� ���� ���
            // sDestBase / sDestIdx �״�� ���
        }

        //---------------------------------------------------
        // Remain?
        //---------------------------------------------------

        sSrcLength    = sSrcArray->numIndex - sSrcIdx;
        sDestLength   = sDestArray->numIndex - sDestIdx;
        sIsSrcRemain  = ID_FALSE;
        sIsDestRemain = ID_FALSE;

        if ( sSrcLength > sDestLength )
        {
            sLength      = sDestLength;
            sIsSrcRemain = ID_TRUE;
        }
        else if ( sSrcLength < sDestLength )
        {
            sLength       = sSrcLength;
            sIsDestRemain = ID_TRUE;
        }
        else
        {
            // sSrcLength == sDestLength
            sLength = sDestLength;
        }

        // sMoveLength�� �� ������ sMoveLength�� ����
        sLength = IDL_MIN( sLength, sMoveLength );

        // memcpy()�� �̵�
        // ����̴� ������ �̹� ���� ������ �̵��߰ų� �ӽ� ������ �����Ѵ�.
        // �� ���� ��찡 �Ͼ�� �ʵ��� �Ѵ�.
        idlOS::memcpy( (void **)sDestArray->elements + sDestIdx,
                       (void **)sSrcArray->elements + sSrcIdx,
                       ID_SIZEOF(void*) * sLength );

        // Index ����
        sSrcBase    += sLength;
        sDestBase   += sLength;
        sSrcIdx     += sLength;
        sDestIdx    += sLength;
        sMoveLength -= sLength;
    }

    // �Ҵ��ߴ� �ӽ� ������ ����
    if ( aIsOverlapped == ID_TRUE )
    {
        IDE_DASSERT( sTempArray.elements != NULL );
        (void)iduMemMgr::free( sTempArray.elements );
        sTempArray.elements = NULL;
    }
    else
    {
        // Nothing to do.
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    // �Ҵ��ߴ� �ӽ� ������ ����
    if ( sTempArray.elements != NULL )
    {
        (void)iduMemMgr::free( sTempArray.elements );
        sTempArray.elements = NULL;
    }
    else
    {
        // Nothing to do.
    }

    return IDE_FAILURE;
}

void qmcMemSort::getLeafArray( qmcSortArray  * aArray,
                               SLong           aIndex,
                               qmcSortArray ** aRetArray,
                               SLong         * aRetIdx )
{
/*******************************************************************
 *
 * Description :
 *   
 *   SortArray���� aIndex ��ġ�� �ش��ϴ� Leaf Array�� ��ȯ�ϰ�,
 *   Leaf Array������ ������� �ε��� ���� ��ȯ�Ѵ�.
 *
 *******************************************************************/

    if ( aArray->depth != 0 )
    {
        getLeafArray( aArray->elements[aIndex >> (aArray->shift)],
                      aIndex & ( aArray->mask ),
                      aRetArray,
                      aRetIdx );
    }
    else
    {
        if ( aRetArray != NULL )
        {
            *aRetArray = aArray;
        }
        else
        {
            // Nothing to do.
        }

        if ( aRetIdx != NULL )
        {
            *aRetIdx = aIndex;
        }
        else
        {
            // Nothing to do.
        }
    }

}
