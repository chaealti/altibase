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
 * $Id: qmcMemHashTempTable.cpp 82075 2018-01-17 06:39:52Z jina.kim $
 *
 * Description :
 *     Memory Hash Temp Table�� ���� �Լ�
 *
 * ��� ���� :
 *
 * ��� :
 *
 **********************************************************************/

#include <idl.h>
#include <ide.h>
#include <qtc.h>
#include <qmcMemHashTempTable.h>

IDE_RC 
qmcMemHash::init( qmcdMemHashTemp * aTempTable,
                  qcTemplate      * aTemplate,
                  iduMemory       * aMemory,
                  qmdMtrNode      * aRecordNode,
                  qmdMtrNode      * aHashNode,
                  UInt              aBucketCnt,
                  idBool            aDistinct )
{
/***********************************************************************
 *
 * Description :
 *    Memory Hash Temp Table�� �ʱ�ȭ�Ѵ�.
 *
 * Implementation :
 *    - �⺻ ���� ����
 *    - ���� ���� ����
 *    - �˻� ���� ����
 *
 ***********************************************************************/

    // ���ռ� �˻�
    IDE_DASSERT( aTempTable != NULL );
    IDE_DASSERT( aBucketCnt > 0 && aBucketCnt <= QMC_MEM_HASH_MAX_BUCKET_CNT );
    IDE_DASSERT( aRecordNode != NULL && aHashNode != NULL );

    //----------------------------------------------------
    // Memory Hash Temp Table�� �⺻ ���� ����
    //----------------------------------------------------

    aTempTable->flag = QMC_MEM_HASH_INITIALIZE;
    
    aTempTable->mTemplate  = aTemplate;
    aTempTable->mMemory    = aMemory;
    aTempTable->mBucketCnt = aBucketCnt;

    // Bucket�� ���� Memory �Ҵ�
    // Bucket���� ���� ������ �ʱ�ȭ�� ���� cralloc�� ����Ѵ�.
    IDU_FIT_POINT( "qmcMemHash::init::alloc::mBucket",
                    idERR_ABORT_InsufficientMemory );

    IDE_TEST( aTempTable->mMemory->cralloc( aTempTable->mBucketCnt * ID_SIZEOF(qmcBucket),
                                            (void**)&aTempTable->mBucket )
              != IDE_SUCCESS);

    aTempTable->mRecordNode = aRecordNode;
    aTempTable->mHashNode   = aHashNode;

    //----------------------------------------------------
    // ������ ���� ����
    //----------------------------------------------------

    if ( aDistinct == ID_TRUE )
    {
        // Distinct Insertion�� ���
        aTempTable->flag &= ~QMC_MEM_HASH_DISTINCT_MASK;
        aTempTable->flag |= QMC_MEM_HASH_DISTINCT_TRUE;
    }
    else
    {
        aTempTable->flag &= ~QMC_MEM_HASH_DISTINCT_MASK;
        aTempTable->flag |= QMC_MEM_HASH_DISTINCT_FALSE;
    }
        
    aTempTable->mTop = NULL;
    aTempTable->mLast = NULL;
    
    aTempTable->mRecordCnt = 0;

    // Distinction�� ���ο� ���� ���� ó�� ������ �׻� �����Ѵ�.
    // �� ���� �ʱ�ȭ ó���� ���� ���� �Լ� �����͸� �����Ѵ�.
    
    aTempTable->insert = qmcMemHash::insertFirst;

    //----------------------------------------------------
    // �˻��� ���� ����
    //----------------------------------------------------

    aTempTable->mKey = 0;
    aTempTable->mCurrent = NULL;
    aTempTable->mNext = NULL;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC 
qmcMemHash::clear( qmcdMemHashTemp * aTempTable )
{
/***********************************************************************
 *
 * Description :
 *     Memory Hash Temp Table�� �ʱ�ȭ�Ѵ�.
 *
 * Implementation :
 *     Record�� ���� �޸𸮴� ���� Hash Temp Table���� �����ϸ�,
 *     Bucket�� ���� ���� ������ �ʱ�ȭ�Ѵ�.
 *
 ***********************************************************************/

    // Bucket�� �ʱ�ȭ
    idlOS::memset( aTempTable->mBucket,
                   0x00,
                   aTempTable->mBucketCnt * ID_SIZEOF(qmcBucket) );

    // ���� ������ �ʱ�ȭ
    aTempTable->mTop = NULL;
    aTempTable->mLast = NULL;
    aTempTable->mRecordCnt = 0;

    aTempTable->insert = qmcMemHash::insertFirst;

    // �˻� ������ �ʱ�ȭ
    aTempTable->mKey = 0;
    aTempTable->mCurrent = NULL;
    aTempTable->mNext = NULL;

    return IDE_SUCCESS;
}

IDE_RC 
qmcMemHash::clearHitFlag( qmcdMemHashTemp * aTempTable )
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

    qmcMemHashElement * sElement;

    // ���� Record�� �д´�.
    IDE_TEST( qmcMemHash::getFirstSequence( aTempTable,
                                            (void**) & sElement )
              != IDE_SUCCESS );

    while ( sElement != NULL )
    {
        // Hit Flag�� �ʱ�ȭ��.
        sElement->flag &= ~QMC_ROW_HIT_MASK;
        sElement->flag |= QMC_ROW_HIT_FALSE;

        IDE_TEST( qmcMemHash::getNextSequence( aTempTable,
                                               (void**) & sElement )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


IDE_RC 
qmcMemHash::insertFirst( qmcdMemHashTemp * aTempTable,
                         UInt              aHash,
                         void            * aElement,
                         void           ** aOutElement )
{
/***********************************************************************
 *
 * Description :
 *    ���� Record�� �����Ѵ�.
 *    Distinction ���ο� ���� ���� ���� record������ �����ϸ�,
 *    ���� Bucket�� ���� �ʱ�ȭ�� �̷������.
 * 
 * Implementation :
 *
 ***********************************************************************/

    UInt sKey;

    // �־��� Hash Key������ Bucket�� ��ġ�� ��´�.
    sKey = qmcMemHash::getBucketID(aTempTable, aHash);

    // ���� ������ ���� �ڷ� ������ �ʱ�ȭ�Ѵ�.
    // Bucket�� ���� ������踦 �����.
    aTempTable->mTop = & aTempTable->mBucket[sKey];
    aTempTable->mLast = & aTempTable->mBucket[sKey];

    // �ش� Bucket�� Element�� �����Ѵ�.
    aTempTable->mLast->element = (qmcMemHashElement*) aElement;
    aTempTable->mLast->element->key = aHash;

    // ������ �����Ͽ����� ǥ��
    *aOutElement = NULL;

    //-------------------------------------------------
    // Distinction ���ο� ���� Insert�� �Լ��� ������
    //-------------------------------------------------

    if ( (aTempTable->flag & QMC_MEM_HASH_DISTINCT_MASK)
         == QMC_MEM_HASH_DISTINCT_TRUE )
    {
        // Distinction�� ���
        aTempTable->insert = qmcMemHash::insertDist;

        // Distinct Insert�� ��츸 �ڵ� Ȯ���Ѵ�.
        aTempTable->flag &= ~QMC_MEM_HASH_AUTO_EXTEND_MASK;
        aTempTable->flag |= QMC_MEM_HASH_AUTO_EXTEND_ENABLE;
    }
    else
    {
        // Non-distinction�� ���
        aTempTable->insert = qmcMemHash::insertAny;

        // Non-Distinct Insert�� ���, �ڵ� Ȯ������ �ʴ´�.
        // �̴� Bucket�� Ȯ���� even-distribution�� ������ ��
        // ���� �����̴�.
        aTempTable->flag &= ~QMC_MEM_HASH_AUTO_EXTEND_MASK;
        aTempTable->flag |= QMC_MEM_HASH_AUTO_EXTEND_DISABLE;
    }

    aTempTable->mRecordCnt++;

    return IDE_SUCCESS;
}

IDE_RC 
qmcMemHash::insertAny( qmcdMemHashTemp * aTempTable,
                       UInt              aHash, 
                       void            * aElement, 
                       void           ** aOutElement )
{
/***********************************************************************
 *
 * Description :
 *    Non-Distinct Insertion�� �����Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

    // Bucket�� ��ġ ȹ��
    UInt sKey;
    qmcMemHashElement * sElement;

    sKey     = qmcMemHash::getBucketID(aTempTable, aHash);
    sElement = aTempTable->mBucket[sKey].element;

    if ( sElement == NULL )
    {
        //----------------------------------
        // ���ο� Bucket�� �Ҵ���� ���
        //----------------------------------

        // Bucket���� ���� ���� ����
        aTempTable->mLast->next = & aTempTable->mBucket[sKey];
        aTempTable->mLast = & aTempTable->mBucket[sKey];

        // ���ο� Bucket�� element ����
        aTempTable->mLast->element = (qmcMemHashElement *) aElement;
        aTempTable->mLast->element->key = aHash;
    }
    else
    {
        //----------------------------------
        // �̹� ����ϰ� �ִ� Bucket�� ���
        //----------------------------------

        // Bucket���� element��� �����Ѵ�.
        ((qmcMemHashElement *)aElement)->next =
            aTempTable->mBucket[sKey].element;
        aTempTable->mBucket[sKey].element = (qmcMemHashElement *)aElement;
        aTempTable->mBucket[sKey].element->key = aHash;
    }

    // ������ ���������� ǥ��
    *aOutElement = NULL;

    aTempTable->mRecordCnt++;

    return IDE_SUCCESS;
}

IDE_RC 
qmcMemHash::insertDist( qmcdMemHashTemp * aTempTable,
                        UInt              aHash, 
                        void            * aElement, 
                        void           ** aOutElement )
{
/***********************************************************************
 *
 * Description :
 *    Distinction Insert�� �����Ѵ�.
 *  
 * Implementation :
 *    ������ Hash Key�� ���� Record�� �켱 �˻��ϰ�,
 *    Key�� �����ϴٸ�, Data ��ü�� ���Ѵ�.
 *    �ٸ� record��� �����ϰ�,  ���� Record��� ���� ������ ������ ��
 *    Record�� �����ش�.
 *
 ***********************************************************************/

    // Bucket ��ġ ȹ��
    UInt sKey;
    qmcMemHashElement * sElement;
    qmcMemHashElement * sLastElement = NULL;
    SInt sResult;

    sKey     = qmcMemHash::getBucketID( aTempTable, aHash);
    sElement = aTempTable->mBucket[sKey].element;

    if ( sElement == NULL )
    {
        //--------------------------------------------
        // Bucket�� ��� �ִ� ���, Record�� �����Ѵ�.
        //--------------------------------------------

        aTempTable->mLast->next = & aTempTable->mBucket[sKey];
        aTempTable->mLast = & aTempTable->mBucket[sKey];
        aTempTable->mLast->element = (qmcMemHashElement *) aElement;
        aTempTable->mLast->element->key = aHash;
        *aOutElement = NULL;

        // Record ���� ����
        aTempTable->mRecordCnt++;
    }
    else
    {
        //--------------------------------------------
        // Bucket�� ��� ���� ���� ���
        // 1. Hash Key �� ������ ��
        // 2. Hashing Column�� ������ ���� ��
        //--------------------------------------------

        sResult = 1;

        // Bucket���� ��� Record �� ���Ͽ� �˻�
        while ( sElement != NULL )
        {
            if ( sElement->key == aHash )
            {
                // Hash Key�� ���� ���, Hashing Column�� ��
                sResult = qmcMemHash::compareRow( aTempTable, 
                                                  sElement, 
                                                  aElement );
                if ( sResult == 0 )
                {
                    //-----------------------
                    // Column�� Data�� ���ٸ�
                    //-----------------------
                    break;
                }
            }
            else
            {
                // nothing to do
            }
            
            sLastElement = sElement;
            sElement = sElement->next;
        }

        if ( sResult == 0 )
        {
            // ������ Record�� �����ϴ� ���,
            // ���� ���� ������ �� record�� ����
            *aOutElement = sElement;
        }
        else
        {
            // ������ Record�� ���� ���, record�� ����
            sLastElement->next = (qmcMemHashElement *) aElement;
            sLastElement->next->key = aHash;
            *aOutElement = NULL;

            // Record ���� ����
            aTempTable->mRecordCnt++;
        }
    }

    // [�ڵ� Ȯ�� ���� �˻�.]
    // ���� ������ �˻�.
    // 1. Record�� ������ Bucket���� Ư�� ���� �̻󺸴� ���� ���
    // 2. Bucket�� ������ų �� �ִ� ���
    if ( ( aTempTable->mRecordCnt >
           (aTempTable->mBucketCnt * QMC_MEM_HASH_AUTO_EXTEND_CONDITION) )
         &&
         ( ( aTempTable->flag & QMC_MEM_HASH_AUTO_EXTEND_MASK )
           == QMC_MEM_HASH_AUTO_EXTEND_ENABLE ) )
    {
        // Bucket�� �ڵ� Ȯ�� ����
        IDE_TEST( extendBucket( aTempTable ) != IDE_SUCCESS );
    }
    else
    {
        // Nothing To Do
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC 
qmcMemHash::getFirstSequence( qmcdMemHashTemp * aTempTable,
                              void           ** aElement )
{
/***********************************************************************
 *
 * Description :
 *    ���� �˻��� ����
 *
 * Implementation :
 *    ���� Bucket�� ��ġ�� �̵��Ͽ� Record�� �����Ѵ�.
 *
 ***********************************************************************/

    if ( aTempTable->mTop == NULL )
    {
        //--------------------------
        // ���� Bucket�� ���� ���
        //--------------------------
        
        *aElement = NULL;
        aTempTable->mCurrent = NULL;
    }
    else
    {
        //--------------------------
        // ���� Bucket�� �ִ� ���
        //--------------------------

        // Bucket���� ���� Record�� ����
        *aElement = aTempTable->mTop->element;

        // ���� Bucket�� ����
        aTempTable->mCurrent = aTempTable->mTop;

        // ���� Bucket���� ���� record�� ����
        aTempTable->mNext = aTempTable->mTop->element->next;
    }

    return IDE_SUCCESS;
}

IDE_RC 
qmcMemHash::getNextSequence( qmcdMemHashTemp * aTempTable,
                             void           ** aElement)
{
/***********************************************************************
 *
 * Description :
 *    ���� ���� �˻��� ����
 *
 * Implementation :
 *    �ش� Record�� ���ٸ�, ���� Bucket���� Record�� �����Ѵ�.
 *
 ***********************************************************************/

    qmcMemHashElement * sElement;

    // ���� Record ���� ȹ��
    sElement = aTempTable->mNext;
    
    if ( sElement != NULL )
    {
        //-----------------------------------
        // ���� Record�� �����ϴ� ���
        //-----------------------------------
        
        *aElement = sElement;
        aTempTable->mNext = sElement->next;
    }
    else
    {
        //-----------------------------------
        // ���� Record�� ���� ���
        //-----------------------------------

        // ���� Bucket�� ȹ��
        aTempTable->mCurrent = aTempTable->mCurrent->next;
        if ( aTempTable->mCurrent != NULL )
        {
            // ���� Bucket�� �����ϴ� ���

            // �ش� Bucket���� record�� ����
            sElement = aTempTable->mCurrent->element;
            *aElement = sElement;

            // ���� Record�� ����
            aTempTable->mNext = sElement->next;
        }
        else
        {
            // ���� Bucket�� ���� ���
            // �� �̻� �����Ͱ� �������� �ʴ´�.
            *aElement = NULL;

            // ���� Record ������ ����
            aTempTable->mNext = NULL;
        }
    }

    return IDE_SUCCESS;
}

IDE_RC 
qmcMemHash::getFirstRange( qmcdMemHashTemp * aTempTable,
                           UInt              aHash,
                           qtcNode         * aFilter,
                           void           ** aElement )
{
/***********************************************************************
 *
 * Description :
 *     �־��� Key ���� Filter�� �̿��Ͽ� Range �˻��� �Ѵ�.
 *
 * Implementation :
 *     Key������ Bucket�� ȹ���ϰ�,
 *     Key���� ���� �־��� Filter�� �����ϴ� record�� ȹ���Ѵ�.
 *     
 ***********************************************************************/

    // Bucket�� ��ġ ȹ��
    UInt sKey;
    
    qmcMemHashElement * sElement;
    idBool sJudge = ID_FALSE;

    sKey = getBucketID(aTempTable, aHash);

    // ���� Range �˻��� ���� Key���� Filter�� ����
    aTempTable->mKey = aHash;
    aTempTable->mFilter = aFilter;

    sElement = aTempTable->mBucket[sKey].element;

    // Bucket���� record���� �˻�
    while ( sElement != NULL )
    {
        if ( sElement->key == aHash )
        {
            // ������ Key���� ���� ���, Filter���ǵ� �˻�.
            IDE_TEST( qmcMemHash::judgeFilter( aTempTable,
                                               sElement,
                                               & sJudge)
                      != IDE_SUCCESS );
            
            if ( sJudge == ID_TRUE )
            {
                // ���ϴ� record�� ã�� ���
                
                // To Fix PR-8645
                // Hash Key ���� �˻��ϱ� ���� Filter���� Access����
                // �����Ͽ���, �� �� Execution Node���� �̸� �ٽ� ������Ŵ.
                // ����, ������ �ѹ��� �˻翡 ���ؼ��� accessȸ���� ���ҽ�Ŵ
                aTempTable->mRecordNode->dstTuple->modify--;
                break;
            }
        }
        else
        {
            // nothing to do
        }
        
        sElement = sElement->next;
    }

    if ( sJudge == ID_TRUE )
    {
        // ���ϴ� Record�� ã�� ���
        *aElement = (void*)sElement;

        // ���� �˻��� ���Ͽ� ���� ��ġ�� ����
        aTempTable->mNext = sElement->next;
    }
    else
    {
        // ���ϴ� Record�� ã�� ���� ���
        *aElement = NULL;

        // ���� Range �˻��� �� �� ������ ����
        aTempTable->mNext = NULL;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC 
qmcMemHash::getNextRange( qmcdMemHashTemp * aTempTable,
                          void           ** aElement )
{
/***********************************************************************
 *
 * Description :
 *    ���� Range �˻��� �����Ѵ�.
 *
 * Implementation :
 *    �̹� ����� ������ �̿��Ͽ�, Bucket���� ���� record���� filter
 *    ������ �˻��Ͽ� record�� �˻��Ѵ�.
 *
 ***********************************************************************/

    qmcMemHashElement * sElement;
    idBool sJudge = ID_FALSE;

    // ���� Bucket������ ������ �˻��� record�� ���� record�� ȹ��
    sElement = aTempTable->mNext;

    // Bucket���� record�� ���� ������ �ݺ�
    while ( sElement != NULL )
    {
        if ( sElement->key == aTempTable->mKey )
        {
            IDE_TEST( qmcMemHash::judgeFilter( aTempTable,
                                               sElement,
                                               & sJudge )
                      != IDE_SUCCESS );
            
            if ( sJudge == ID_TRUE )
            {
                // key���� �����ϰ� ������ ������ ���
                
                // To Fix PR-8645
                // Hash Key ���� �˻��ϱ� ���� Filter���� Access����
                // �����Ͽ���, �� �� Execution Node���� �̸� �ٽ� ������Ŵ.
                // ����, ������ �ѹ��� �˻翡 ���ؼ��� accessȸ���� ���ҽ�Ŵ
                aTempTable->mRecordNode->dstTuple->modify--;
                
                break;
            }
        }
        else
        {
            // nothing to do
        }
        
        sElement = sElement->next;
    }

    if ( sJudge == ID_TRUE )
    {
        // ������ �����ϴ� record�� �˻��� ���
        *aElement = (void*)sElement;

        // ���� �˻��� ���Ͽ� ���� record ��ġ�� ����
        aTempTable->mNext = sElement->next;
    }
    else
    {
        // ������ �����ϴ� record�� ���� ���
        *aElement = NULL;
        aTempTable->mNext = NULL;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC 
qmcMemHash::getSameRowAndNonHit( qmcdMemHashTemp * aTempTable,
                               UInt              aHash,
                               void            * aRow,
                               void           ** aElement )
{
/***********************************************************************
 *
 * Description :
 *    �ش� Row�� ������ Hashing Column�� ���� �����鼭,
 *    Hit���� ���� record�� �˻��Ѵ�.
 *    Set Intersectoin���� ����ϸ�, �� �Ǹ��� �˻��ȴ�.
 *
 * Implementation :
 *    �־��� Key�����κ��� Bucket�� �����Ѵ�.
 *    �־��� ��� record�� ���Ͽ� ������ ���� �˻縦 �����Ͽ�
 *    ��� ������ �����ϴ� record�� ã�´�.
 *        - Key �� ������.
 *        - Non-Hit
 *        - Hash Column�� ���� ������.
 *
 ***********************************************************************/

    qmcMemHashElement * sElement;
    UInt                sKey;
    SInt                sResult;

    sKey = getBucketID( aTempTable, aHash );

    sElement = aTempTable->mBucket[sKey].element;
    
    while ( sElement != NULL )
    {
        //------------------------------------
        // Key�� ����, Hit���� �ʰ�
        // Data�� ������ Record�� ã�´�.
        //------------------------------------
        
        // Hash Key�� �� Hit Flag �˻�.
        if ( (sElement->key == aHash) &&
             ( (sElement->flag & QMC_ROW_HIT_MASK) == QMC_ROW_HIT_FALSE ) )
        {  
            sResult = compareRow( aTempTable,
                                  (void*) aRow,
                                  (void*) sElement );
            if ( sResult == 0 )
            {
                // ������ Record�� ���
                break;
            }
        }
        sElement = sElement->next;
    }

    *aElement = sElement;

    return IDE_SUCCESS;
}


IDE_RC 
qmcMemHash::getFirstHit( qmcdMemHashTemp * aTempTable,
                         void           ** aElement )
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

    qmcMemHashElement * sElement;

    IDE_TEST( getFirstSequence( aTempTable, aElement ) != IDE_SUCCESS );

    while ( *aElement != NULL )
    {
        sElement = (qmcMemHashElement*) *aElement;
        if ( (sElement->flag & QMC_ROW_HIT_MASK) == QMC_ROW_HIT_TRUE )
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

IDE_RC 
qmcMemHash::getNextHit( qmcdMemHashTemp * aTempTable,
                        void           ** aElement )
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

    qmcMemHashElement * sElement;

    IDE_TEST( getNextSequence( aTempTable, aElement ) != IDE_SUCCESS );

    while ( *aElement != NULL )
    {
        sElement = (qmcMemHashElement*) *aElement;
        if ( (sElement->flag & QMC_ROW_HIT_MASK) == QMC_ROW_HIT_TRUE )
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

IDE_RC 
qmcMemHash::getFirstNonHit( qmcdMemHashTemp * aTempTable,
                            void           ** aElement )
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

    qmcMemHashElement * sElement;

    IDE_TEST( getFirstSequence( aTempTable, aElement ) != IDE_SUCCESS );

    while ( *aElement != NULL )
    {
        sElement = (qmcMemHashElement*) *aElement;
        if ( (sElement->flag & QMC_ROW_HIT_MASK) == QMC_ROW_HIT_FALSE )
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

IDE_RC 
qmcMemHash::getNextNonHit( qmcdMemHashTemp * aTempTable,
                            void           ** aElement )
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

    qmcMemHashElement * sElement;

    IDE_TEST( getNextSequence( aTempTable, aElement ) != IDE_SUCCESS );

    while ( *aElement != NULL )
    {
        sElement = (qmcMemHashElement*) *aElement;
        if ( (sElement->flag & QMC_ROW_HIT_MASK) == QMC_ROW_HIT_FALSE )
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

IDE_RC 
qmcMemHash::getDisplayInfo( qmcdMemHashTemp * aTempTable,
                            SLong           * aRecordCnt,
                            UInt            * aBucketCnt )
{
/***********************************************************************
 *
 * Description :
 *    ���ԵǾ� �ִ� Record ������ ȹ���Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

    *aRecordCnt = aTempTable->mRecordCnt;
    *aBucketCnt = aTempTable->mBucketCnt;

    return IDE_SUCCESS;
}

UInt        
qmcMemHash::getBucketID( qmcdMemHashTemp * aTempTable,
                         UInt              aHash )
{
/***********************************************************************
 *
 * Description :
 *    �־��� Hash Key�κ��� Bucket ID�� ��´�.
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qmcMemHash::getBucketID"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY("qmcMemHash::getBucketID"));

    return aHash % aTempTable->mBucketCnt;

#undef IDE_FN
}

SInt          
qmcMemHash::compareRow ( qmcdMemHashTemp * aTempTable,
                         void            * aElem1,
                         void            * aElem2 )

{
/***********************************************************************
 *
 * Description :
 *    ����� Record�� Hashing Column�� ���� ���Ͽ�
 *    ���� ������ �� �ٸ� ���� Return��.
 *
 * Implementation :
 *
 ***********************************************************************/

    qmdMtrNode * sNode;
    mtdValueInfo sValueInfo1;
    mtdValueInfo sValueInfo2;
    
    SInt sResult = -1;

    for( sNode = aTempTable->mHashNode; 
         sNode != NULL; 
         sNode = sNode->next )
    {
        // �� Record�� Hashing Column�� ���� ��
        sValueInfo1.value  = sNode->func.getRow(sNode, aElem1);
        sValueInfo1.column = (const mtcColumn *)sNode->func.compareColumn;
        sValueInfo1.flag   = MTD_OFFSET_USE;
        
        sValueInfo2.value  = sNode->func.getRow(sNode, aElem2);
        sValueInfo2.column = (const mtcColumn *)sNode->func.compareColumn;
        sValueInfo2.flag   = MTD_OFFSET_USE;
        
        sResult = sNode->func.compare( &sValueInfo1,
                                       &sValueInfo2 );
        if( sResult != 0)
        {
            // ���� �ٸ� ���, �� �̻� �񱳸� ���� ����.
            break;
        }
    }

    return sResult;
}

IDE_RC          
qmcMemHash::judgeFilter ( qmcdMemHashTemp * aTempTable,
                          void *            aElem,
                          idBool          * aResult )

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

    qmdMtrNode * sNode;

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

IDE_RC          
qmcMemHash::extendBucket ( qmcdMemHashTemp * aTempTable )

{
/***********************************************************************
 *
 * Description :
 *    Ư�� ����(Bucket���� Record�� �ſ� ���� ���)�� ���
 *    Bucket�� ������ �ڵ� Ȯ���Ѵ�.
 *
 * Implementation :
 *    ������ ���� ������ ����ȴ�.
 *        - Bucket�� Ȯ�� ���ɼ� �˻�.
 *        - ���ο� Bucket�� �Ҵ����.
 *        - ���� Bucket���κ��� record�� ���� ���ο� Bucket��
 *          �����Ѵ�.
 *        - �� ��, Distinction ���θ� �˻��� �ʿ�� ����.
 *
 ***********************************************************************/

    qmcMemHashElement * sElement;

    aTempTable->mExtBucket = NULL;
    aTempTable->mExtTop = NULL;
    aTempTable->mExtLast = NULL;
    

    // Bucket Ȯ���� ���������� �˻�.
    if ( (aTempTable->mBucketCnt * QMC_MEM_HASH_AUTO_EXTEND_RATIO) >
         QMC_MEM_HASH_MAX_BUCKET_CNT )
    {
        // Bucket Count�� ���������� �� �ִ� Bucket Size�� �Ѵ� �����,
        // ������ų �� ����.

        // �� �̻� ������ �� ������ ǥ��
        aTempTable->flag &= ~QMC_MEM_HASH_AUTO_EXTEND_MASK;
        aTempTable->flag |= QMC_MEM_HASH_AUTO_EXTEND_DISABLE;
    }
    else
    {
        // Bucket Count�� ������ų �� �ִ� ���

        //-------------------------------------------------
        // ���Ӱ� Ȯ���� Bucket�� ���� ��� �� Memory �Ҵ�
        //-------------------------------------------------
        
        aTempTable->mExtBucketCnt = aTempTable->mBucketCnt * QMC_MEM_HASH_AUTO_EXTEND_RATIO;
        
        IDU_FIT_POINT( "qmcMemHash::extendBucket::cralloc::mExtBucket",
                        idERR_ABORT_InsufficientMemory );

        IDE_TEST( aTempTable->mMemory->cralloc( aTempTable->mExtBucketCnt * ID_SIZEOF(qmcBucket),
                                                (void**)&aTempTable->mExtBucket )
                  != IDE_SUCCESS);

        //-------------------------------------------------
        // ������ Bucket���κ��� Record�� ���
        // ���ο� Bucket�� ����
        //-------------------------------------------------

        // ���� Record ȹ��
        IDE_TEST( qmcMemHash::getFirstSequence( aTempTable,
                                                (void**) & sElement )
                  != IDE_SUCCESS );
        IDE_ASSERT( sElement != NULL );

        // ���� Record�� ����
        IDE_TEST( qmcMemHash::insertFirstNewBucket( aTempTable, sElement )
                  != IDE_SUCCESS );

        //-------------------------------------------------
        // Record�� �������� ���� ������ �ݺ���
        //-------------------------------------------------
        
        IDE_TEST( qmcMemHash::getNextSequence( aTempTable,
                                               (void**) & sElement )
                  != IDE_SUCCESS );

        while ( sElement != NULL )
        {
            // ���� Record�� ����
            IDE_TEST( qmcMemHash::insertNextNewBucket( aTempTable, sElement )
                      != IDE_SUCCESS );

            // ���� Record ȹ��
            IDE_TEST( qmcMemHash::getNextSequence( aTempTable,
                                                   (void**) & sElement )
                      != IDE_SUCCESS );
        }

        //-------------------------------------------------
        // ���ο� Bucket���� ��ȯ��Ŵ
        // ������ Bucket Memory�� dangling�ǳ�,
        // ���������� qmxMemory �����ڿ� ���Ͽ� ���ŵȴ�.
        // Bucket�� Ȯ���� insert���� �߿� �߻��ϸ�,
        // Insert���� �ֿ� �˻� ������ �������� �����Ƿ�,
        // �ٸ� ������ ���� �ʱ�ȭ�� �ʿ����.
        //-------------------------------------------------

        aTempTable->mBucketCnt = aTempTable->mExtBucketCnt;
        aTempTable->mBucket = aTempTable->mExtBucket;
        aTempTable->mTop = aTempTable->mExtTop;
        aTempTable->mLast = aTempTable->mExtLast;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

IDE_RC
qmcMemHash::insertFirstNewBucket( qmcdMemHashTemp   * aTempTable,
                                  qmcMemHashElement * aElem )
{
/***********************************************************************
 *
 * Description :
 *    Bucket Ȯ�� �� ���� record�� ������.
 *
 * Implementation :
 *    ���ο� Bucket�� ���� �ʱ�ȭ �� record�� �����Ѵ�.
 *    ������ ���� ����� �޸�,
 *    Hash Element�� �̹� Key���� �����ϹǷ� �̿� ����
 *    ������ ���嵵 �ʿ� ����.
 *    
 ***********************************************************************/

    UInt    sBucketID;

    // �־��� Hash Key������ ���ο� Bucket�� ��ġ�� ��´�.
    sBucketID = aElem->key % aTempTable->mExtBucketCnt;

    // ���ο� ���� ������ ���Ͽ� clear�Ѵ�.
    aElem->next = NULL;
    
    // ���� ������ ���� �ڷ� ������ �ʱ�ȭ�Ѵ�.
    // Bucket�� ���� ������踦 �����.
    aTempTable->mExtTop = & aTempTable->mExtBucket[sBucketID];
    aTempTable->mExtLast = & aTempTable->mExtBucket[sBucketID];

    // �ش� Bucket�� Element�� �����Ѵ�.
    aTempTable->mExtLast->element = (qmcMemHashElement*) aElem;

    return IDE_SUCCESS;
}

IDE_RC
qmcMemHash::insertNextNewBucket( qmcdMemHashTemp   * aTempTable,
                                 qmcMemHashElement * aElem )
{
/***********************************************************************
 *
 * Description :
 *    Bucket Ȯ�� �� ���� record�� ������.
 *
 * Implementation :
 *    ������ ���� ����� �޸�,
 *    Hash Element�� �̹� Key���� �����ϹǷ� �̿� ����
 *    ������ ���嵵 �ʿ� ����.
 *    �̹� Distinction�� ����Ǿ� �ֱ� ������,
 *    Hash Key �˻� �� Column�� �� �˻� ���� �ʿ� ����.
 *
 ***********************************************************************/

    UInt sBucketID;
    qmcMemHashElement * sElement;

    // To Fix PR-8382
    // Extend Bucket�� ����Ͽ� ���� ���踦 �����ؾ� ��.
    // �־��� Hash Key������ ���ο� Bucket�� ��ġ�� ��´�.
    sBucketID = aElem->key % aTempTable->mExtBucketCnt;
    sElement = aTempTable->mExtBucket[sBucketID].element;

    // ���ο� ���� ������ ���Ͽ� clear�Ѵ�.
    aElem->next = NULL;

    if ( sElement == NULL )
    {
        //----------------------------------
        // ���ο� Bucket�� �Ҵ���� ���
        //----------------------------------

        // Bucket���� ���� ���� ����
        aTempTable->mExtLast->next = & aTempTable->mExtBucket[sBucketID];
        aTempTable->mExtLast = & aTempTable->mExtBucket[sBucketID];

        // ���ο� Bucket�� element ����
        aTempTable->mExtLast->element = (qmcMemHashElement *) aElem;
    }
    else
    {
        //----------------------------------
        // �̹� ����ϰ� �ִ� Bucket�� ���
        //----------------------------------

        // Bucket���� element��� �����Ѵ�.
        ((qmcMemHashElement *)aElem)->next =
            aTempTable->mExtBucket[sBucketID].element;
        aTempTable->mExtBucket[sBucketID].element = (qmcMemHashElement *)aElem;
    }
    
    return IDE_SUCCESS;
}
