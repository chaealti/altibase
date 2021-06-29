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
 * $Id: qmcMemPartHashTempTable.cpp 82075 2018-01-17 06:39:52Z jina.kim $
 *
 * Description :
 *     Memory Partitioned Hash Temp Table�� ���� �Լ�
 *
 * ��� ���� :
 *
 * ��� :
 *
 **********************************************************************/

#include <idl.h>
#include <ide.h>
#include <qtc.h>
#include <qmcMemPartHashTempTable.h>

IDE_RC qmcMemPartHash::init( qmcdMemPartHashTemp * aTempTable,
                             qcTemplate          * aTemplate,
                             iduMemory           * aMemory,
                             qmdMtrNode          * aRecordNode,
                             qmdMtrNode          * aHashNode,
                             UInt                  aBucketCnt )
{
/***********************************************************************
 *
 * Description :
 *    Memory Partitioned Hash Temp Table�� �ʱ�ȭ�Ѵ�.
 *
 * Implementation :
 *    - �⺻ ���� ����
 *    - ���� ���� ����
 *    - �˻� ���� ����
 *
 ***********************************************************************/

    // ���ռ� �˻�
    IDE_DASSERT( aTempTable  != NULL );
    IDE_DASSERT( aRecordNode != NULL && aHashNode != NULL );

    //----------------------------------------------------
    // Memory Partitioned Hash Temp Table�� �⺻ ���� ����
    //----------------------------------------------------

    aTempTable->mFlag         = QMC_MEM_PART_HASH_INITIALIZE;
    aTempTable->mTemplate     = aTemplate;
    aTempTable->mMemory       = aMemory;
    aTempTable->mRecordNode   = aRecordNode;
    aTempTable->mHashNode     = aHashNode;
    aTempTable->mBucketCnt    = aBucketCnt;

    //----------------------------------------------------
    // Partitioned Hashing �� ���� ����
    // - readyForSearch() ���� ���Ѵ�.
    // - Partition ������ �ʱ� ���� Bucket ������ �ȴ�.
    //----------------------------------------------------

    aTempTable->mRadixBit     = 0;
    aTempTable->mPartitionCnt = aBucketCnt;
    aTempTable->mHistogram    = NULL;
    aTempTable->mSearchArray  = NULL;

    //----------------------------------------------------
    // ������ ���� ����
    //----------------------------------------------------

    aTempTable->mHead         = NULL;
    aTempTable->mTail         = NULL;
    aTempTable->mRecordCnt    = 0;

    //----------------------------------------------------
    // �˻��� ���� ����
    //----------------------------------------------------

    aTempTable->mNextElem     = NULL;  
    aTempTable->mKey          = 0;
    aTempTable->mFilter       = NULL;
    aTempTable->mNextIdx      = 0;
    aTempTable->mBoundaryIdx  = 0;

    return IDE_SUCCESS;
}

IDE_RC qmcMemPartHash::clear( qmcdMemPartHashTemp * aTempTable )
{
/***********************************************************************
 *
 * Description :
 *     Memory Partitioned Hash Temp Table�� �ʱ�ȭ�Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

    //----------------------------------------------------
    // �⺻ ���� �ʱ�ȭ
    //----------------------------------------------------

    aTempTable->mHead         = NULL;
    aTempTable->mTail         = NULL;

    // ���� �˻� ��, �غ� �ϵ��� �Ѵ�.
    aTempTable->mFlag &= ~QMC_MEM_PART_HASH_SEARCH_READY_MASK;
    aTempTable->mFlag |= QMC_MEM_PART_HASH_SEARCH_READY_FALSE;

    // mTemplate, mRecordNode, mHashNode, mBucketCnt�� �ʱ�ȭ���� �ʴ´�.

    //----------------------------------------------------
    // Partitioned Hashing �� ���� ���� �ʱ�ȭ
    //----------------------------------------------------

    aTempTable->mRadixBit     = 0;
    aTempTable->mPartitionCnt = aTempTable->mBucketCnt;
    aTempTable->mHistogram    = NULL;
    aTempTable->mSearchArray  = NULL;

    //----------------------------------------------------
    // ���� ������ �ʱ�ȭ
    //----------------------------------------------------

    aTempTable->mHead         = NULL;
    aTempTable->mTail         = NULL;
    aTempTable->mRecordCnt    = 0;

    //----------------------------------------------------
    // �˻� ������ �ʱ�ȭ
    //----------------------------------------------------
    
    aTempTable->mNextElem     = NULL;  
    aTempTable->mKey          = 0;
    aTempTable->mFilter       = NULL;
    aTempTable->mNextIdx      = 0;
    aTempTable->mBoundaryIdx  = 0;

    return IDE_SUCCESS;
}

IDE_RC qmcMemPartHash::clearHitFlag( qmcdMemPartHashTemp * aTempTable )
{
/***********************************************************************
 *
 * Description :
 *    Temp Table���� ��� Record���� Hit Flag�� �ʱ�ȭ�Ѵ�.
 *
 * Implementation :
 *    ���ڵ尡 ������ ������ ���� �˻��Ͽ� Record���� Hit Flag�� �ʱ�ȭ
 *
 ***********************************************************************/

    qmcMemHashElement * sElement = NULL;

    // ���� Record�� �д´�.
    IDE_TEST( qmcMemPartHash::getFirstSequence( aTempTable,
                                                (void**) & sElement )
              != IDE_SUCCESS );

    while ( sElement != NULL )
    {
        // Hit Flag�� �ʱ�ȭ��.
        sElement->flag &= ~QMC_ROW_HIT_MASK;
        sElement->flag |= QMC_ROW_HIT_FALSE;

        IDE_TEST( qmcMemPartHash::getNextSequence( aTempTable,
                                                   (void**) & sElement )
                  != IDE_SUCCESS );
    }
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


IDE_RC qmcMemPartHash::insertAny( qmcdMemPartHashTemp  * aTempTable,
                                  UInt                   aHash,
                                  void                 * aElement,
                                  void                ** aOutElement )
{
/***********************************************************************
 *
 * Description :
 *    Record�� �����Ѵ�.
 * 
 * Implementation :
 *    ���� List ���� �������� �� Element�� �����Ѵ�.
 *
 ***********************************************************************/

    qmcMemHashElement * sElement = aTempTable->mTail;

    if ( sElement == NULL )
    {
        // mTail�� NULL = ù Element�� �����ϴ� ���
        // mHead = mTail = aElement
        aTempTable->mHead = (qmcMemHashElement*) aElement;
        aTempTable->mTail = (qmcMemHashElement*) aElement;
    }
    else
    {
        // ù Element ������ �ƴ� ���
        // mTail->next = aElement; mTail = aElement
        aTempTable->mTail = (qmcMemHashElement *) aElement;
        sElement->next    = (qmcMemHashElement *) aElement;
    }

    // Hash Key ����
    ((qmcMemHashElement*) aElement)->key = aHash;

    // ������ ���������� ǥ��
    *aOutElement = NULL;

    // Record ���� ����
    aTempTable->mRecordCnt++;
    
    return IDE_SUCCESS;
}

IDE_RC qmcMemPartHash::getFirstSequence( qmcdMemPartHashTemp  * aTempTable,
                                         void                ** aElement )
{
/***********************************************************************
 *
 * Description :
 *    ù ���� �˻��� ����
 *
 * Implementation :
 *    ���� List���� ù Element�� ��ȯ�Ѵ�.
 *
 ***********************************************************************/

    qmcMemHashElement * sElement = aTempTable->mHead;

    if ( sElement != NULL )
    {   
        //-----------------------------------
        // ���� Record�� �����ϴ� ���
        //-----------------------------------
        *aElement = sElement;
        aTempTable->mNextElem = sElement->next;
    }   
    else
    {   
        *aElement = NULL;
        aTempTable->mNextElem = NULL;
    }   

    return IDE_SUCCESS;
}

IDE_RC qmcMemPartHash::getNextSequence( qmcdMemPartHashTemp  * aTempTable,
                                        void                ** aElement )
{
/***********************************************************************
 *
 * Description :
 *    ���� ���� �˻��� ����
 *
 * Implementation :
 *    ���� List���� ���� Element�� ��ȯ�Ѵ�.
 *
 ***********************************************************************/


    qmcMemHashElement * sElement = aTempTable->mNextElem;

    if ( sElement != NULL )
    {   
        //-----------------------------------
        // ���� Record�� �����ϴ� ���
        //-----------------------------------
        *aElement = sElement;
        aTempTable->mNextElem = sElement->next;
    }   
    else
    {   
        *aElement = NULL;
        aTempTable->mNextElem = NULL;
    }   

    return IDE_SUCCESS;
}

IDE_RC qmcMemPartHash::getFirstRange( qmcdMemPartHashTemp  * aTempTable,
                                      UInt                   aHash,
                                      qtcNode              * aFilter,
                                      void                ** aElement )
{
/***********************************************************************
 *
 * Description :
 *     �־��� Key ���� Filter�� �̿��Ͽ� Range �˻��� �Ѵ�.
 *
 * Implementation :
 *    Key ������ Histogram�� ������ Search Array�� ���� Item���� Ž���Ѵ�.
 *    ��ġ�ϴ� Key�� ���� Item�� ����Ű�� Element�� ������ �˻��Ѵ�.
 *    ������ ���̶��, �ش� Element�� ��ȯ�Ѵ�.
 *     
 ***********************************************************************/

    qmcMemHashItem * sSearchArray = NULL;
    UInt             sPartIdx     = 0;
    ULong            sTargetIdx   = 0;
    ULong            sBoundaryIdx = 0;
    idBool           sJudge       = ID_FALSE;

    //-------------------------------------------------
    // ���� �˻� ��, Histogram / Search Array �غ�
    //-------------------------------------------------

    // Record�� �������� ������ readyForSearch()�� ȣ���� �ʿ䰡 ����.
    IDE_TEST_CONT( aTempTable->mRecordCnt == 0, NO_ELEMENT );

    if ( ( aTempTable->mFlag & QMC_MEM_PART_HASH_SEARCH_READY_MASK )
            == QMC_MEM_PART_HASH_SEARCH_READY_FALSE )
    {
        IDE_TEST( readyForSearch( aTempTable ) != IDE_SUCCESS );
    }
    else
    {
        // Nothing to do.
    }

    //-------------------------------------------------
    // �˻� �غ�
    //-------------------------------------------------

    // Histogram / SearchArray�� �����ؾ� �Ѵ�.
    IDE_DASSERT( aTempTable->mHistogram   != NULL );
    IDE_DASSERT( aTempTable->mSearchArray != NULL );

    // ���� Range �˻��� ���� Key���� Filter�� ����
    aTempTable->mKey    = aHash;
    aTempTable->mFilter = aFilter;

    // Hash Key�� ����, Histogram�� ������ Index ȹ��
    sPartIdx     = ( aHash & ( ( 1 << aTempTable->mRadixBit ) - 1 ) );

    // Histogram�� ����Ű�� ���� Partition / ���� Partition�� ���� Item Index ȹ��
    sTargetIdx   = aTempTable->mHistogram[sPartIdx];
    sBoundaryIdx = aTempTable->mHistogram[sPartIdx+1];

    // Search Array ��������
    sSearchArray = aTempTable->mSearchArray;

    //-------------------------------------------------
    // �˻� ����
    //-------------------------------------------------

    while ( sTargetIdx < sBoundaryIdx ) 
    {
        if ( sSearchArray[sTargetIdx].mKey == aHash )
        {
            // ������ Key���� ���� ���, Filter���ǵ� �˻�.
            IDE_TEST( qmcMemPartHash::judgeFilter( aTempTable,
                                                   sSearchArray[sTargetIdx].mPtr,
                                                   & sJudge )
                      != IDE_SUCCESS );

            if ( sJudge == ID_TRUE )
            {
                // To Fix PR-8645
                // Hash Key ���� �˻��ϱ� ���� Filter���� Access����
                // �����Ͽ���, �� �� Execution Node���� �̸� �ٽ� ������Ŵ.
                // ����, ������ �ѹ��� �˻翡 ���ؼ��� accessȸ���� ���ҽ�Ŵ
                aTempTable->mRecordNode->dstTuple->modify--;
                break;
            }
            else
            {
                // Nothing to do.
            }
        }
        else
        {
            // Nothing to do.
        }
        
        sTargetIdx++;
    }

    IDE_EXCEPTION_CONT( NO_ELEMENT );

    if ( sJudge == ID_TRUE )
    {
        // �˻� ����
        *aElement = sSearchArray[sTargetIdx].mPtr;

        // ���� �˻��� ���� ���� ����
        aTempTable->mNextIdx     = sTargetIdx + 1;
        aTempTable->mBoundaryIdx = sBoundaryIdx;
    }
    else
    {
        // �˻� ����
        *aElement = NULL;

        // ���� �˻��� ���� ���ϵ��� ���� �ʱ�ȭ
        aTempTable->mNextIdx     = 0;
        aTempTable->mBoundaryIdx = 0;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmcMemPartHash::getNextRange( qmcdMemPartHashTemp  * aTempTable,
                                     void                ** aElement )
{
/***********************************************************************
 *
 * Description :
 *    ���� Range �˻��� �����Ѵ�.
 *
 * Implementation :
 *    �̹� ����� ������ �̿��Ͽ�, Search Array�� ���� Item���� Ž���Ѵ�.
 *    ��ġ�ϴ� Key�� ���� Item�� ����Ű�� Element�� ������ �˻��Ѵ�.
 *    ������ ���̶��, �ش� Element�� ��ȯ�Ѵ�.
 *
 ***********************************************************************/

    qmcMemHashItem * sSearchArray = NULL;
    UInt             sKey         = 0;
    ULong            sTargetIdx   = 0;
    ULong            sBoundaryIdx = 0;
    idBool           sJudge       = ID_FALSE;

    //-------------------------------------------------
    // �˻� �غ�
    //-------------------------------------------------

    // Search Array ��������
    sSearchArray = aTempTable->mSearchArray;

    // Key�� ��������
    sKey         = aTempTable->mKey;

    // ���� Item Index / ���� Partition ���� Item Index ��������
    sTargetIdx   = aTempTable->mNextIdx;
    sBoundaryIdx = aTempTable->mBoundaryIdx;

    //-------------------------------------------------
    // �˻� ����
    //-------------------------------------------------

    while ( sTargetIdx < sBoundaryIdx ) 
    {
        if ( sSearchArray[sTargetIdx].mKey == sKey )
        {
            // ������ Key���� ���� ���, Filter���ǵ� �˻�.
            IDE_TEST( qmcMemPartHash::judgeFilter( aTempTable,
                                                   sSearchArray[sTargetIdx].mPtr,
                                                   & sJudge )
                      != IDE_SUCCESS );

            if ( sJudge == ID_TRUE )
            {
                // To Fix PR-8645
                // Hash Key ���� �˻��ϱ� ���� Filter���� Access����
                // �����Ͽ���, �� �� Execution Node���� �̸� �ٽ� ������Ŵ.
                // ����, ������ �ѹ��� �˻翡 ���ؼ��� accessȸ���� ���ҽ�Ŵ
                aTempTable->mRecordNode->dstTuple->modify--;
                break;
            }
            else
            {
                // Nothing to do.
            }
        }
        else
        {
            // Nothing to do.
        }
        
        sTargetIdx++;
    }

    if ( sJudge == ID_TRUE )
    {
        // �˻� ����
        *aElement = sSearchArray[sTargetIdx].mPtr;

        // ���� �˻��� ���� ���� ����
        aTempTable->mNextIdx     = sTargetIdx + 1;
        aTempTable->mBoundaryIdx = sBoundaryIdx;
    }
    else
    {
        // �˻� ����
        *aElement = NULL;

        // ���� �˻��� ���� ���ϵ��� ���� �ʱ�ȭ
        aTempTable->mNextIdx     = 0;
        aTempTable->mBoundaryIdx = 0;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmcMemPartHash::getFirstHit( qmcdMemPartHashTemp  * aTempTable,
                                    void                ** aElement )
{
/***********************************************************************
 *
 * Description :
 *    ���� �˻��Ͽ� Hit�� Record�� �����Ѵ�.
 *
 * Implementation :
 *    Hit �� record�� ã�� ������ �ݺ������� �����Ѵ�.
 *
 ***********************************************************************/

    qmcMemHashElement * sElement = NULL;
    
    IDE_TEST( getFirstSequence( aTempTable, aElement ) != IDE_SUCCESS );

    while ( *aElement != NULL )
    {
        sElement = (qmcMemHashElement*) *aElement;

        if ( ( sElement->flag & QMC_ROW_HIT_MASK ) == QMC_ROW_HIT_TRUE )
        {
            // ���ϴ� Record�� ã��
            break;
        }
        else
        {
            // ���� record�� �˻�
            IDE_TEST( getNextSequence( aTempTable, aElement ) != IDE_SUCCESS );
        }
    }
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmcMemPartHash::getNextHit( qmcdMemPartHashTemp  * aTempTable,
                                   void                ** aElement )
{
/***********************************************************************
 *
 * Description :
 *    ���� �˻��Ͽ� Hit�� Record�� �����Ѵ�.
 *
 * Implementation :
 *    Hit �� record�� ã�� ������ �ݺ������� �����Ѵ�.
 *
 ***********************************************************************/

    qmcMemHashElement * sElement = NULL;
    
    IDE_TEST( getNextSequence( aTempTable, aElement ) != IDE_SUCCESS );

    while ( *aElement != NULL )
    {
        sElement = (qmcMemHashElement*) *aElement;
        if ( ( sElement->flag & QMC_ROW_HIT_MASK ) == QMC_ROW_HIT_TRUE )
        {
            // ���ϴ� Record�� ã��
            break;
        }
        else
        {
            // ���� record�� �˻�
            IDE_TEST( getNextSequence( aTempTable, aElement ) != IDE_SUCCESS );
        }
    }
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmcMemPartHash::getFirstNonHit( qmcdMemPartHashTemp  * aTempTable,
                                       void                ** aElement )
{
/***********************************************************************
 *
 * Description :
 *    ���� �˻��Ͽ� NonHit�� Record�� �����Ѵ�.
 *
 * Implementation :
 *    NonHit �� record�� ã�� ������ �ݺ������� �����Ѵ�.
 *
 ***********************************************************************/

    qmcMemHashElement * sElement = NULL;
    
    IDE_TEST( getFirstSequence( aTempTable, aElement ) != IDE_SUCCESS );

    while ( *aElement != NULL )
    {
        sElement = (qmcMemHashElement*) *aElement;
        if ( ( sElement->flag & QMC_ROW_HIT_MASK ) == QMC_ROW_HIT_FALSE )
        {
            // ���ϴ� Record�� ã��
            break;
        }
        else
        {
            // ���� record�� �˻�
            IDE_TEST( getNextSequence( aTempTable, aElement ) != IDE_SUCCESS );
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmcMemPartHash::getNextNonHit( qmcdMemPartHashTemp  * aTempTable,
                                      void                ** aElement )
{
/***********************************************************************
 *
 * Description :
 *    ���� �˻��Ͽ� NonHit�� Record�� �����Ѵ�.
 *
 * Implementation :
 *    NonHit �� record�� ã�� ������ �ݺ������� �����Ѵ�.
 *
 ***********************************************************************/

    qmcMemHashElement * sElement = NULL;
    
    IDE_TEST( getNextSequence( aTempTable, aElement ) != IDE_SUCCESS );

    while ( *aElement != NULL )
    {
        sElement = (qmcMemHashElement*) *aElement;
        if ( ( sElement->flag & QMC_ROW_HIT_MASK ) == QMC_ROW_HIT_FALSE )
        {
            // ���ϴ� Record�� ã��
            break;
        }
        else
        {
            // ���� record�� �˻�
            IDE_TEST( getNextSequence( aTempTable, aElement ) != IDE_SUCCESS );
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmcMemPartHash::getDisplayInfo( qmcdMemPartHashTemp * aTempTable,
                                       SLong               * aRecordCnt,
                                       UInt                * aBucketCnt )
{
/***********************************************************************
 *
 * Description :
 *    ���ԵǾ� �ִ� Record ����, Partition ������ ��ȯ�Ѵ�.
 *
 * Implementation :
 *    �ܺο����� DISTINCT Hash Temp Table�� ������ �� �����Ƿ�
 *    'Bucket ����' ��� ���信 'Partition ����'�� ��ȯ�Ѵ�.
 *
 ***********************************************************************/

    *aRecordCnt = aTempTable->mRecordCnt;
    *aBucketCnt = aTempTable->mPartitionCnt;

    return IDE_SUCCESS;
}

IDE_RC qmcMemPartHash::judgeFilter( qmcdMemPartHashTemp * aTempTable,
                                    void                * aElem,
                                    idBool              * aResult )
{
/***********************************************************************
 *
 * Description :
 *     �־��� Record�� Filter ������ �����ϴ� �� �˻�.
 *
 * Implementation :
 *     Filter�� ������ �� �ֵ��� �ش� Hash Column���� ������Ű��
 *     Filter�� �����Ѵ�.  ���� ��� memory column�� ��� �ش� record��
 *     pointer�� ����Ǿ� �־� �� pointer�� tuple set�� �������Ѿ� �Ѵ�.
 *
 ***********************************************************************/

    qmdMtrNode * sNode = NULL;
    
    //------------------------------------
    // Hashing ��� column���� ������Ŵ
    //------------------------------------

    // To Fix PR-8024
    // ���� ���õ� Row�� Tuple Set�� ��Ͻ��ѵΰ� ó���ؾ� ��
    aTempTable->mHashNode->dstTuple->row = aElem;
    
    for ( sNode = aTempTable->mHashNode; 
          sNode != NULL; 
          sNode = sNode->next )
    {
        IDE_TEST( sNode->func.setTuple( aTempTable->mTemplate, sNode, aElem )
                  != IDE_SUCCESS );
    }

    //------------------------------------
    // Filter ���� ����� ȹ��
    //------------------------------------
    
    IDE_TEST( qtc::judge( aResult, 
                          aTempTable->mFilter, 
                          aTempTable->mTemplate ) != IDE_SUCCESS );

    // To Fix PR-8645
    // Hash Key���� ���� �˻�� ���� Record�� �����ϴ� �˻��̸�,
    // Access ȸ���� �������Ѿ� Hash ȿ���� ������ �� �ִ�.
    aTempTable->mRecordNode->dstTuple->modify++;
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

UInt qmcMemPartHash::calcRadixBit( qmcdMemPartHashTemp * aTempTable )
{
/***********************************************************************
 *
 * Description :
 *     Radix Bit Partitioning�� ���� Radix Bit�� ����Ѵ�.
 *     Radix Bit�� 2�� ������ �� ���� Partition�� ������ �ȴ�.
 *
 * Implementation :
 *     ���� �� �� ���� ������� ������ Partition ������ ���� ���Ѵ�.
 *
 *     - AUTO_BUCKETCNT_DISABLE �� 0 �� �����Ǿ� �ִ� ��� :
 *       ( ���� Record ���� ) / AVG_RECORD_COUNT
 *     - AUTO_BUCKETCNT_DISABLE �� 1 �� �����Ǿ� �ִ� ��� :
 *       ( �ʱ⿡ �Էµ� Bucket ���� )
 *
 *     �� ����, �ּ�/�ִ밪�� ������ �� �Ʒ� ������ Radix Bit�� ���Ѵ�.
 *
 *       RadixBit = ceil( Log(Optimal Partition Count) )
 *       ( Log�� ���� 2 )
 *
 ***********************************************************************/

    ULong sOptPartCnt = 0;
    SInt  sRBit       = 0;

    IDE_DASSERT( aTempTable->mRecordCnt > 0 );

    // 1. Optimal Partition Count�� ���Ѵ�.
    if ( aTempTable->mTemplate->memHashTempManualBucketCnt == ID_TRUE )
    {
        // [ Estimation Mode ]
        // �Էµ� Bucket Count�� �������� �Ѵ�.
        sOptPartCnt = aTempTable->mBucketCnt;
    }
    else
    {
        // [ Calculation Mode ]
        // ���� Bucket Count ��� ����� ������. (qmgJoin::getBucketCntNTmpTblCnt)
        // ��, ���� Record ������ �ƴ� ���� Record ������ ����ϸ�
        // Partition �� ��� Element ������ ������ ����� ���� ����Ѵ�.
        sOptPartCnt = 
            aTempTable->mRecordCnt / QMC_MEM_PART_HASH_AVG_RECORD_COUNT; 
    }

    // 2. Optimal Partition Count�� �ּ�/�ִ밪 ����
    if ( sOptPartCnt < QMC_MEM_PART_HASH_MIN_PART_COUNT )
    {
        sOptPartCnt = QMC_MEM_PART_HASH_MIN_PART_COUNT;
    }
    else if ( sOptPartCnt > QMC_MEM_PART_HASH_MAX_PART_COUNT )
    {
        sOptPartCnt = QMC_MEM_PART_HASH_MAX_PART_COUNT;
    }
    else
    {
        // Nothing to do.
    }

    // 3. Optimal Partition Count���� ũ�鼭 ���� �����
    //    Partition Count�� ���� �� �ִ� Radix Bit�� ����Ѵ�.
    sRBit = ceilLog2( sOptPartCnt );

    return sRBit;
}

IDE_RC qmcMemPartHash::readyForSearch( qmcdMemPartHashTemp * aTempTable )
{
/***********************************************************************
 *
 * Description :
 *      ���� ���� �˻� ����, ������׷��� �˻� �迭�� �غ��Ѵ�.
 *
 * Implementation :
 *
 *  (1) ����� �ϰų�, �Էµ� Bucket ������ ���� Partition ������ ���Ѵ�.
 *
 *  (2) Partition ����, Record ������ �´�
 *      ������׷�, �˻� �迭�� �Ҵ��Ѵ�.
 *
 *  (3) ������׷���, �� Partition�� ���� Element ������ ����.
 *    - �� Element Hash Key�� Radix Bit Masking�� �ؼ� Partition �ε����� ���Ѵ�.
 *    - Partition �ε����� ����Ű�� ������׷� ���� 1 ������Ų��.
 *
 *  (4) ������׷�����, i��° ���� 0 ~ (i-1)��° ���� �հ�� �����Ѵ�.
 *      ���� �����, �˻� �迭���� �� Partition�� ���� �ε����� �ȴ�.
 *
 *  (5) Partition ������ ���� �� ���� �˻� �迭�� ������ ������,
 *      �� ���� ���� ������ ������ ������ ��, �ش� �Լ��� �����Ѵ�.
 *      (�Լ��� fanoutSingle / fanoutDouble �� �����ȴ�.)
 *
 ***********************************************************************/

    qmcMemHashElement * sElement        = NULL;
    ULong             * sHistogram      = NULL;
    ULong               sPartitionCnt   = 0;
    UInt                sPartIdx        = 0;
    UInt                sPartMask       = 0;
    UInt                sSingleRadixBit = 0;
    ULong               sHistoTemp      = 0;
    ULong               sHistoSum       = 0;
    UInt                i               = 0;


    //---------------------------------------------------------------------
    // Radix Bit / Partition Count ���
    //---------------------------------------------------------------------

    IDE_DASSERT( aTempTable->mRecordCnt > 0 );

    aTempTable->mRadixBit     = calcRadixBit( aTempTable );
    aTempTable->mPartitionCnt = (UInt)( 1 << aTempTable->mRadixBit );

    //---------------------------------------------------------------------
    // Histogram �Ҵ�
    //---------------------------------------------------------------------

    IDU_FIT_POINT( "qmcMemPartHash::readyForSearch::malloc1",
                   idERR_ABORT_InsufficientMemory );
    IDE_TEST( aTempTable->mMemory->cralloc( ( aTempTable->mPartitionCnt + 1 ) * ID_SIZEOF(ULong),
                                            (void**) & sHistogram )
              != IDE_SUCCESS );
 
    // Histogram�� �������� Record Count���� �Ѵ�.
    sHistogram[aTempTable->mPartitionCnt] = aTempTable->mRecordCnt;

    //---------------------------------------------------------------------
    // Histogram��, �� Partition�� Element ���� ����
    //
    //  ���� List�� ��ȸ�ϸ鼭 Hash Key�� Radix Bit Masking�� �Ѵ�.
    //  Masking ����� ����Ű�� Histogram ���� 1 ������Ų��.
    //---------------------------------------------------------------------

    sElement  = aTempTable->mHead;
    sPartMask = ( 1 << aTempTable->mRadixBit ) - 1;

    while ( sElement != NULL )
    {
        sPartIdx = sElement->key & sPartMask;
        sHistogram[sPartIdx]++;
        sElement = sElement->next;
    }

    //---------------------------------------------------------------------
    // Histogram��, ���� �հ� ����
    //
    //  ó������, �� Partition�� ���� Element�� ������ ���� ����������
    //  ���� Partition�� Element ���� ���� ���� ���� ��ġ�� �����ϵ��� �Ѵ�.
    //
    //           Histogram             Histogram 
    //           ----------            ----------
    //        P1 |    3   |         P1 |    0   | = 0
    //           ----------            ----------
    //        P2 |    1   |   =>    P2 |    3   | = 0+3
    //           ----------            ----------
    //        P3 |    5   |         P3 |    4   | = 0+3+1
    //           ----------            ----------
    //           |  ...   |            |  ...   |
    //           ----------            ----------
    //           | RecCnt |            | RecCnt |
    //           ----------            ----------
    //
    //---------------------------------------------------------------------

    sPartitionCnt = aTempTable->mPartitionCnt;

    for ( i = 0; i < sPartitionCnt; i++ )
    {
        sHistoTemp = sHistogram[i];
        sHistogram[i] = sHistoSum;
        sHistoSum += sHistoTemp;
    }

    // �ϼ��� Histogram�� Temp Table�� ����
    aTempTable->mHistogram = sHistogram;

    //---------------------------------------------------------------------
    // Search Array �Ҵ�
    //---------------------------------------------------------------------

    IDU_FIT_POINT( "qmcMemPartHash::readyForSearch::malloc2",
                   idERR_ABORT_InsufficientMemory );
    IDE_TEST( aTempTable->mMemory->alloc( aTempTable->mRecordCnt * ID_SIZEOF(qmcMemHashItem),
                                          (void**) & aTempTable->mSearchArray )
              != IDE_SUCCESS );
    
    //---------------------------------------------------------------------
    // Single Fanout ���� Radix Bit ���
    //
    // (1) TLB Entry Count * Page Size * Ȯ�� ��� = TLB�� �´� �޸� ����
    // (2) ���⿡ Histogram Item (ULong) ũ�⸸ŭ ���� �� 
    //     = Single Fanout�� �̻����� Partition ����
    // (3) ���⿡ ceil(log2())�� �ϸ� Single Fanout ���� Radix Bit�� ���ȴ�.
    //---------------------------------------------------------------------

    sSingleRadixBit = ceilLog2( ( QMC_MEM_PART_HASH_TLB_ENTRY_MULTIPLIER * 
                                  aTempTable->mTemplate->memHashTempTlbCount * 
                                  QMC_MEM_PART_HASH_PAGE_SIZE ) / ID_SIZEOF(ULong) );

    //---------------------------------------------------------------------
    // Partitioning ����
    //---------------------------------------------------------------------

    if ( QCU_FORCE_HSJN_MEM_TEMP_FANOUT_MODE == 0 )
    {
        // 0 : Single / Double Fanout �� ����
        if ( aTempTable->mRadixBit <= sSingleRadixBit )
        {
            IDE_TEST( fanoutSingle( aTempTable ) != IDE_SUCCESS );  
        }
        else
        {
            IDE_TEST( fanoutDouble( aTempTable ) != IDE_SUCCESS );  
        }
    }
    else if ( QCU_FORCE_HSJN_MEM_TEMP_FANOUT_MODE == 1 )
    {
        // 1 : Single Fanout �� ����
        IDE_TEST( fanoutSingle( aTempTable ) != IDE_SUCCESS );  
    }
    else
    {
        // 2 : Double Fanout �� ����
        IDE_DASSERT( QCU_FORCE_HSJN_MEM_TEMP_FANOUT_MODE == 2 );
        IDE_TEST( fanoutDouble( aTempTable ) != IDE_SUCCESS );  
    }

    //---------------------------------------------------------------------
    // ������ : Flag ��ȯ
    //---------------------------------------------------------------------

    aTempTable->mFlag &= ~QMC_MEM_PART_HASH_SEARCH_READY_MASK;
    aTempTable->mFlag |= QMC_MEM_PART_HASH_SEARCH_READY_TRUE;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmcMemPartHash::fanoutSingle( qmcdMemPartHashTemp * aTempTable )
{
/***********************************************************************
 *
 * Description :
 *     �� ���� Search Array�� Item�� �����Ѵ�.
 *
 *                         TempHist.            SearchArr. 
 *      LIST                 -----                -----
 *     ------                |---|                |---|
 *     |    |                |---|                |---|
 *     |    |                |---|                |---|
 *     |    | -> PartIdx ->  |---| -> ArrayIdx -> |---|
 *     |    |                |---|                |---|
 *     |    |                |---|                |---|
 *     ------                |---|                |---|
 *                           -----                -----
 *
 *     �� Partition���� ���� ������ ��ġ�� ������ '�ӽ� Histogram'�� �ʿ��ϴ�.
 *     ������ �̷���� �� ���� �ش� ���� +1 �����Ѵ�.
 *     ���� Histogram�� ���� ��ġ ������ �����ؾ� �ϹǷ� ������ �� ���� ������
 *     �ӽ� ������ ����ϴ� ���̴�.
 *
 * Implementation :
 *
 *   (1) '�ӽ� Histogram'�� �Ҵ��Ѵ�.
 *   (2) '�ӽ� Histogram'��, Histogram ������ �����Ѵ�.
 *   (3) List�� �ִ� �� Element����, ������ �����Ѵ�.
 *     - Hash Key�� Radix Bit Masking �ؼ� Partition �ε����� ���Ѵ�.
 *     - Partition �ε����� '�ӽ� Histogram'�� ������ Array �ε����� ���Ѵ�.
 *       �� ��, �ش��ϴ� '�ӽ� Histogram ��'�� +1 ������Ų��.
 *     - Array �ε����� Search Array�� Element ������ �����Ѵ�.  
 *
 ***********************************************************************/

    qmcMemHashElement  * sElement      = NULL;
    qmcMemHashItem     * sSearchArray  = NULL;
    ULong              * sHistogram    = NULL;
    ULong              * sInsertHist   = NULL;
    ULong                sArrayIdx     = 0;
    UInt                 sPartitionCnt = 0; 
    UInt                 sMask         = 0; 
    UInt                 sPartitionIdx = 0;
    UInt                 sHashKey      = 0;
    UInt                 sState        = 0;

    //--------------------------------------------------------------------
    // �⺻ ���� ����
    //--------------------------------------------------------------------

    sPartitionCnt = aTempTable->mPartitionCnt; 
    sHistogram    = aTempTable->mHistogram;
    sSearchArray  = aTempTable->mSearchArray;

    //--------------------------------------------------------------------
    // �ӽ� Histogram �Ҵ�
    //--------------------------------------------------------------------

    // ���� Histogram���� �޸�, �������� Record ������ ���� �ʿ䰡 ����.
    IDU_FIT_POINT( "qmcMemPartHash::fanoutSingle::malloc",
                   idERR_ABORT_InsufficientMemory );
    IDE_TEST( iduMemMgr::malloc( IDU_MEM_QMC,
                                 sPartitionCnt * ID_SIZEOF(ULong),
                                 (void**) & sInsertHist )
              != IDE_SUCCESS );
    sState = 1;

    //--------------------------------------------------------------------
    // �ӽ� Histogram�� Histogram �� ����
    //--------------------------------------------------------------------

    idlOS::memcpy( sInsertHist,
                   sHistogram,
                   sPartitionCnt * ID_SIZEOF(ULong) ); 

    //--------------------------------------------------------------------
    // ���� �˻��ϸ鼭, Search Array�� Item ����
    //--------------------------------------------------------------------

    // ù Element�� Mask �غ�
    sElement = aTempTable->mHead;
    sMask    = ( 1 << aTempTable->mRadixBit ) - 1;

    while ( sElement != NULL )
    {
        // Radix-bit Masking���� Partition Index ���
        sHashKey      = sElement->key;
        sPartitionIdx = sHashKey & sMask;

        IDE_DASSERT( sPartitionIdx < sPartitionCnt );

        // �ӽ� Histogram ������ Search Array Index ���
        sArrayIdx = sInsertHist[sPartitionIdx];
        sInsertHist[sPartitionIdx]++; // ���� ������ ����Ű���� �̸� ����

        // Search Array Index�� Record �������� �۾ƾ� �ϸ�,
        // ���� Partition�� ��踦 �Ѵ� ��쵵 ����� �Ѵ�.
        IDE_DASSERT( sArrayIdx < (UInt)aTempTable->mRecordCnt );
        IDE_DASSERT( sArrayIdx < sHistogram[sPartitionIdx+1] );

        // Search Array�� ���� ����
        sSearchArray[sArrayIdx].mKey = sHashKey;
        sSearchArray[sArrayIdx].mPtr = sElement;

        // ���� Element Ž��
        sElement = sElement->next;
    }

    // ����
    sState = 0;
    IDE_TEST( iduMemMgr::free( sInsertHist ) != IDE_SUCCESS );
    sInsertHist = NULL;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    // BUG-41824
    switch ( sState )
    {
        case 1:
            (void)iduMemMgr::free( sInsertHist );
            sInsertHist = NULL;
            /* fall through */
        case 0:
            break;
        default:
            break;
    }

    return IDE_FAILURE;
}

IDE_RC qmcMemPartHash::fanoutDouble( qmcdMemPartHashTemp * aTempTable )
{
/***********************************************************************
 *
 * Description :
 *     �� ���� ���� Search Array�� Item�� �����Ѵ�.
 *
 *                          TempHist1            TempArr. 
 *      LIST                  -----                -----
 *     ------                 |   |                |   |
 *     |    |                 |---|                |---|
 *     |    |                 |   |                |   |
 *     |    | -> Part1Idx ->  |---| -> ArrayIdx -> |---|
 *     |    |     (RBit1)     |   |                |   |
 *     |    |                 |---|                |---|
 *     ------                 |   |                |   |
 *                            -----                -----
 *
 *     Temp Array�� �� Partition����,
 *
 *                        TempHist2            SearchArr. 
 *     |---|                -----                |---|
 *     |   | -> Part2Idx -> |---| -> ArrayIdx -> |---|
 *     |---|    (RBit2)     -----                |---|
 *
 *
 *     Fanout �����,
 *
 *               SearchArr. 
 *      LIST       -----
 *     ------      |---|
 *     |    |      |---|
 *     |    |      |---|
 *     |    |  ->  |---|
 *     |    |      |---|
 *     |    |      |---|
 *     ------      |---|
 *                 -----
 *
 *     ù ��°�� ���� Radix Bit (RBit1)��, ��ü Radix Bit�� 1/2�� ���Ѵ�.
 *     �� ��°�� ���� Radix Bit (RBit2)��, ��ü Radix Bit���� rB1�� �� ���̴�.
 *
 *     RBit1�� ��ü Radix Bit���� ���� Bit�� ��Ÿ����,
 *     RBit2�� ��ü Radix Bit���� ���� Bit�� ��Ÿ����.
 *
 *     (e.g.) 01010100110101 = 0101010 0110101
 *            |  RadixBit  |   | RB1 | | RB2 |
 *
 *     ��, rB1 Masking���� ��Ÿ���� Partition 1�� �ȿ���
 *         rB2 Masking���� ��Ÿ���� Partition ���� ���� �ٽ� �ɰ��� �� �ִ�.
 *
 *     �� ���� ���� ���ԵǹǷ�, ���� ũ���� �ӽ� Search Array�� �� �ʿ��ϴ�. (TempArr.)
 *     ������ �ӽ� Search Array�� �˻� �������� ���� �ʰ�, ���� Search Array�� ����Ѵ�.
 *
 *     �޸� ���� ������, �ټ��� Partition���� ������ �� ����
 *     Single Fanout ��� �ӵ��� ������. (TLB Miss �ּ�ȭ)
 *
 * Implementation :
 *
 *    RBit1, RBit2�� ���� ����, ������ ���� Single Fanout�� 2ȸ �����Ѵ�.
 *    
 *    �� ��° Fanout ������, ù ��° Fanout���� ������ �� Partition���� �ݺ����� �Ѵ�.
 *    �׷���, �� ��° �ӽ� Histogram�� �����Ѵ�.
 *
 ***********************************************************************/

    qmcMemHashElement  * sElement           = NULL;
    qmcMemHashItem     * sSearchArray       = NULL;
    qmcMemHashItem     * sTempSearchArray   = NULL; 
    ULong              * sHistogram         = NULL;
    ULong              * sFirstInsertHist   = NULL;
    ULong              * sLastInsertHist    = NULL;
    ULong                sRecCount          = 0;
    ULong                sArrayIdx          = 0;
    ULong                sStartArrIdx       = 0;
    ULong                sEndArrIdx         = 0;
    UInt                 sFirstRBit         = 0;
    UInt                 sFirstPartCnt      = 0;
    UInt                 sLastRBit          = 0;
    UInt                 sLastPartCnt       = 0;
    UInt                 sMask              = 0; 
    UInt                 sPartitionIdx      = 0;
    UInt                 sHashKey           = 0; 
    UInt                 sState             = 0; 
    UInt                 i                  = 0;
    UInt                 j                  = 0;

    //--------------------------------------------------------------------
    // �⺻ ���� ����
    //--------------------------------------------------------------------

    sRecCount    = aTempTable->mRecordCnt; 
    sHistogram   = aTempTable->mHistogram;
    sSearchArray = aTempTable->mSearchArray;

    //--------------------------------------------------------------------
    // ù Partition / ������ Partition ���� ���
    //--------------------------------------------------------------------

    // ù Radix Bit : 1/2
    sFirstRBit    = aTempTable->mRadixBit / 2;
    sFirstPartCnt = (UInt)( 1 << sFirstRBit );

    // ������ Radix Bit : (��ü rBit) - (ù rBit)
    sLastRBit     = aTempTable->mRadixBit - sFirstRBit;
    sLastPartCnt  = (UInt)( 1 << sLastRBit );

    //--------------------------------------------------------------------
    // ù / ������ Histogram + �ӽ� Search Array �Ҵ�
    //--------------------------------------------------------------------

    // ù Histogram
    IDU_FIT_POINT( "qmcMemPartHash::fanoutDouble::malloc1",
                   idERR_ABORT_InsufficientMemory );
    IDE_TEST( iduMemMgr::malloc( IDU_MEM_QMC,
                                 ( sFirstPartCnt + 1 ) * ID_SIZEOF(ULong),
                                 (void**) & sFirstInsertHist )
              != IDE_SUCCESS );
    sState = 1;

    // ó�� Histogram�� ������ Record ���� ���� ������ �־�� �Ѵ�.
    sFirstInsertHist[sFirstPartCnt] = sRecCount;

    // �ӽ� Search Array
    IDU_FIT_POINT( "qmcMemPartHash::fanoutDouble::malloc2",
                   idERR_ABORT_InsufficientMemory );
    IDE_TEST( iduMemMgr::malloc( IDU_MEM_QMC,
                                 sRecCount * ID_SIZEOF(qmcMemHashItem),
                                 (void**) & sTempSearchArray )
              != IDE_SUCCESS );
    sState = 2;
 
    // ������ Histogram
    IDU_FIT_POINT( "qmcMemPartHash::fanoutDouble::malloc3",
                   idERR_ABORT_InsufficientMemory );
    IDE_TEST( iduMemMgr::malloc( IDU_MEM_QMC,
                                 sLastPartCnt * ID_SIZEOF(ULong),
                                 (void**) & sLastInsertHist )
              != IDE_SUCCESS );
    sState = 3;

    //--------------------------------------------------------------------
    // ù Histogram�� Histogram �� ����
    //--------------------------------------------------------------------

    // ���� Histogram���� Last Partition ������ŭ �ǳʶٸ鼭 �����´�
    for ( i = 0; i < sFirstPartCnt; i++ )
    {
        sFirstInsertHist[i] = sHistogram[i*sLastPartCnt];
    }

    //--------------------------------------------------------------------
    // �ӽ� Search Array�� ����
    //--------------------------------------------------------------------

    // ù Element�� Mask �غ�
    sElement = aTempTable->mHead;
    sMask    = ( 1 << aTempTable->mRadixBit ) - 1;

    while ( sElement != NULL )
    {
        // ���� Bit�� ���� Radix-bit Masking���� Partition Index ���
        sHashKey = sElement->key;
        sPartitionIdx = ( sHashKey & sMask ) >> sLastRBit;

        IDE_DASSERT( sPartitionIdx < sFirstPartCnt );

        // ù Histogram ������ �ӽ� Search Array Index ���
        sArrayIdx = sFirstInsertHist[sPartitionIdx];
        sFirstInsertHist[sPartitionIdx]++; // ���� ������ ����Ű���� �̸� ����

        // �ӽ� Search Array Index�� Record �������� �۾ƾ� �ϸ�,
        // ���� Partition�� ��踦 �Ѵ� ��쵵 ����� �Ѵ�.
        IDE_DASSERT( sArrayIdx < sRecCount );
        IDE_DASSERT( sArrayIdx < sHistogram[(sPartitionIdx+1)*sLastPartCnt] );

        // Search Array�� ���� ����
        sTempSearchArray[sArrayIdx].mKey = sHashKey;
        sTempSearchArray[sArrayIdx].mPtr = sElement;

        // ���� Element �˻�
        sElement = sElement->next;
    }


    // ù Histogram���� �ɰ� �� Partition ���� ����
    // ���� RBit�� ���ؼ��� Masking�� �Ѵ�.
    sMask = ( 1 << sLastRBit ) - 1;

    for ( i = 0 ; i < sFirstPartCnt; i++ )
    {
        //--------------------------------------------
        // ù Search Array�� i��° Partition ���� ����
        // - i��° Partition�� Search Array ���� ��ġ
        // - i��° Partition�� Search Array �� ��ġ
        //--------------------------------------------
        // ù Histogram�� ������ ����/�� ��ġ�� ���ϴµ�
        // i / i+1�� �ƴ� i-1 / i�� ����Ѵ�.
        // ù Fanout �������� �̹� ù Histogram ������ 
        // ���� Partition ���� ��ġ���� �����Ǿ��� �����̴�.
        // Link : http://nok.altibase.com/x/u9ACAg
        //--------------------------------------------

        if ( i == 0 )
        {
            sStartArrIdx = 0;
        }
        else
        {
            sStartArrIdx = sFirstInsertHist[i-1];
        }

        sEndArrIdx = sFirstInsertHist[i];

        //--------------------------------------------------------------------
        // i��° Partition�� �ش��ϴ� Histogram ������ ������ Histogram�� ����
        //--------------------------------------------------------------------

        idlOS::memcpy( sLastInsertHist,
                       & sHistogram[sLastPartCnt * i],
                       ID_SIZEOF(ULong) * sLastPartCnt );

        //--------------------------------------------------------------------
        // i��° Partition�� ������ ���� Search Array�� ����
        //--------------------------------------------------------------------

        for ( j = sStartArrIdx; j < sEndArrIdx; j++ )
        {
            // Radix-bit Masking���� Partition Index ���
            sPartitionIdx = sTempSearchArray[j].mKey & sMask;

            IDE_DASSERT( sPartitionIdx < sLastPartCnt );

            // ������ Histogram ������ ���� Search Array Index ���
            sArrayIdx = sLastInsertHist[sPartitionIdx];
            sLastInsertHist[sPartitionIdx]++; // ���� ������ ����Ű���� �̸� ����

            // ���� Search Array Index�� Record �������� �۾ƾ� �ϸ�,
            // ���� Partition�� ��踦 �Ѵ� ��쵵 ����� �Ѵ�.
            IDE_DASSERT( sArrayIdx < sRecCount );
            IDE_DASSERT( sArrayIdx < sHistogram[(sLastPartCnt*i)+sPartitionIdx+1] );

            // ���� Search Array�� ���� ����
            sSearchArray[sArrayIdx].mKey = sTempSearchArray[j].mKey;
            sSearchArray[sArrayIdx].mPtr = sTempSearchArray[j].mPtr;
        }
    }

    // ����
    sState = 2;
    IDE_TEST( iduMemMgr::free( sLastInsertHist  ) != IDE_SUCCESS );
    sLastInsertHist  = NULL;

    sState = 1;
    IDE_TEST( iduMemMgr::free( sTempSearchArray ) != IDE_SUCCESS );
    sTempSearchArray = NULL;

    sState = 0;
    IDE_TEST( iduMemMgr::free( sFirstInsertHist ) != IDE_SUCCESS );
    sFirstInsertHist = NULL;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    // BUG-41824
    switch ( sState )
    {
        case 3:
            (void)iduMemMgr::free( sLastInsertHist  );
            sLastInsertHist = NULL;
            /* fall through */
        case 2:
            (void)iduMemMgr::free( sTempSearchArray );
            sTempSearchArray = NULL;
            /* fall through */
        case 1:
            (void)iduMemMgr::free( sFirstInsertHist  );
            sFirstInsertHist = NULL;
            /* fall through */
        case 0:
            break;
        default:
            break;
    }

    return IDE_FAILURE;
}

/*
 * 2^n >= aNumber �� �����ϴ� ���� ���� n �� ���Ѵ�
 */
UInt qmcMemPartHash::ceilLog2( ULong aNumber )
{
    ULong sDummyValue;
    UInt  sRet;

    for ( sDummyValue = 1, sRet = 0; sDummyValue < aNumber; sRet++ )
    {
        sDummyValue <<= 1;
    }

    return sRet;
}
