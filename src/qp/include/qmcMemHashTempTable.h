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
 * $Id: qmcMemHashTempTable.h 82075 2018-01-17 06:39:52Z jina.kim $
 *
 * Description :
 *     Memory Hash Temp Table�� ���� ����
 *   - Chained Hash Algorithm�� ����Ѵ�.
 *
 * ��� ���� :
 *
 * ��� :
 *
 **********************************************************************/

#ifndef _O_QMC_MEM_HASH_TMEP_TABLE_H_
#define _O_QMC_MEM_HASH_TMEP_TABLE_H_ 1

#include <smi.h>
#include <qtcDef.h>
#include <qmc.h>

// BUG-38961 hint �� max ���� ���� ����Ҽ� �ִ� max ���� �����ϰ� �Ѵ�.
// �ִ� Bucket ����
#define QMC_MEM_HASH_MAX_BUCKET_CNT  QMS_MAX_BUCKET_CNT

// Bucket�� �ڵ� Ȯ�� ���� ( Record ���� > BucketCnt * 2 )
#define QMC_MEM_HASH_AUTO_EXTEND_CONDITION        ( 2 )

// Bucket�� �ڵ� Ȯ�� ���� (BucketCnt = Record ���� * 4 )
#define QMC_MEM_HASH_AUTO_EXTEND_RATIO            ( 4 )

/* qmcdMemHashTemp.flag                              */
#define QMC_MEM_HASH_INITIALIZE            (0x00000000)

/* qmcdMemHashTemp.flag                              */
// Distinct Insertion ����
#define QMC_MEM_HASH_DISTINCT_MASK         (0x00000001)
#define QMC_MEM_HASH_DISTINCT_FALSE        (0x00000000)
#define QMC_MEM_HASH_DISTINCT_TRUE         (0x00000001)

/* qmcdMemHashTemp.flag                              */
// Bucket�� �ڵ� Ȯ�� ���� ����
#define QMC_MEM_HASH_AUTO_EXTEND_MASK      (0x00000002)
#define QMC_MEM_HASH_AUTO_EXTEND_ENABLE    (0x00000000)
#define QMC_MEM_HASH_AUTO_EXTEND_DISABLE   (0x00000002)

struct qmcdMemHashTemp;

//---------------------------------------
// Insert�� ���� �Լ� ������
//---------------------------------------

// Distinction���� �ƴ����� ���� ������ �Լ��� ȣ���Ѵ�.
typedef IDE_RC (* qmcHashInsertFunc) ( qmcdMemHashTemp * aTempTable,
                                       UInt              aHash, 
                                       void            * aElement,
                                       void           ** aOutElement );

//---------------------------------------
// Bucket �ڷ� ����
//---------------------------------------

typedef struct qmcBucket
{
    qmcMemHashElement * element;   // Bucket ���� ���� Element
    qmcBucket         * next;      // Record�� �ִ� ���� Bucket
} qmcBucket;

//--------------------------------------------------------------
// [Memory Hash Temp Table�� �ڷ� ����]
//
//               mBucket
//               --------
//               | elem |
//               | next |
//               --------
//         mLast | elem |---->[ ]---->[ ]
//        ------>| next |
//        |      --------
//        |      | elem |
//        |      | next |
//        |      --------
//        |  mTop| elem |---->[ ]
//        |   -- | next |
//        |   |   --------
//        |   -->| elem |---->[ ]---->[ ]---->[ ]
//        -------| next |             mNext
//               --------
//
// ���� ���� Bucket���� ���� ���踦 �����Ͽ� ���� �˻���
// ������ ����Ų��.
// ��, element�� ���� Bucket�� �������� �ʵ��� �Ѵ�.
//
//--------------------------------------------------------------

typedef struct qmcdMemHashTemp
{
    //----------------------------------------------------
    // Memory Hash Temp Table�� �⺻ ����
    //----------------------------------------------------

    UInt                 flag;         // Distinction����
    qcTemplate         * mTemplate;    // Template
    iduMemory          * mMemory;
    
    UInt                 mBucketCnt;   // Bucket�� ����
    qmcBucket          * mBucket;      // �Ҵ� ���� Bucket
    qmdMtrNode         * mRecordNode;  // Record ���� ����
    qmdMtrNode         * mHashNode;    // Hashing �� Column ����

    //----------------------------------------------------
    // ������ ���� ����
    //----------------------------------------------------
    
    qmcHashInsertFunc    insert;       // ���� �Լ� ������
    qmcBucket          * mTop;         // ���� Record�� �ִ� Bucket
    qmcBucket          * mLast;        // �������� �Ҵ���� Bucket
    SLong                mRecordCnt;   // �� ���� Record ����

    //----------------------------------------------------
    // �˻��� ���� ����
    //----------------------------------------------------
    
    qmcBucket          * mCurrent;     // ���� �˻����� Bucket
    qmcMemHashElement  * mNext;        // ���� �˻����� Element
    UInt                 mKey;         // �˻��� Hash Key ��
    qtcNode            * mFilter;      // Range �˻��� ���� Filter

    //----------------------------------------------------
    // �ڵ� Bucket Ȯ���� ���� ����
    //----------------------------------------------------

    UInt                 mExtBucketCnt; // Ȯ��� Bucket�� ����
    qmcBucket          * mExtBucket;    // Ȯ�� Bucket
    qmcBucket          * mExtTop;       // Ȯ�� Bucket�� ���� Bucket
    qmcBucket          * mExtLast;      // Ȯ�� Bucket�� ������ Bucket

} qmcdMemHashTemp;

class qmcMemHash
{
public:

    //------------------------------------------------
    // Memory Hash Temp Table�� ����
    //------------------------------------------------
    
    // �ʱ�ȭ�� �Ѵ�.
    static IDE_RC  init( qmcdMemHashTemp * aTempTable,
                         qcTemplate      * aTemplate,
                         iduMemory       * aMemory,
                         qmdMtrNode      * aRecordNode,
                         qmdMtrNode      * aHashNode,
                         UInt              aBucketCnt,
                         idBool            aDistinct );
    
    // Temp Table���� Bucket�� �ʱ�ȭ�Ѵ�.
    static IDE_RC  clear( qmcdMemHashTemp * aTempTable );

    // ��� Record�� Hit Flag�� Reset��
    static IDE_RC clearHitFlag( qmcdMemHashTemp * aTempTable );

    //------------------------------------------------
    // Memory Sort Hash Table�� ����
    //------------------------------------------------

    // ���� Record�� �����Ѵ�.
    static IDE_RC  insertFirst( qmcdMemHashTemp * aTempTable,
                                UInt              aHash,
                                void            * aElement,
                                void           ** aOutElement );

    // ������ Record�� �����Ѵ�.
    static IDE_RC  insertAny( qmcdMemHashTemp * aTempTable,
                              UInt             aHash, 
                              void           * aElement,
                              void          ** aOutElement );

    // ������ Record�� ���� ��� �����Ѵ�.
    static IDE_RC  insertDist( qmcdMemHashTemp * aTempTable,
                               UInt              aHash, 
                               void            * aElement, 
                               void           ** aOutElement );

    //------------------------------------------------
    // Memory Hash Temp Table�� �˻�
    //------------------------------------------------

    //----------------------------
    // ���� �˻�
    //----------------------------

    static IDE_RC  getFirstSequence( qmcdMemHashTemp * aTempTable,
                                     void           ** aElement );
    
    static IDE_RC  getNextSequence( qmcdMemHashTemp * aTempTable,
                                    void           ** aElement);

    //----------------------------
    // Range �˻�
    //----------------------------
    
    static IDE_RC  getFirstRange( qmcdMemHashTemp * aTempTable,
                                  UInt              aHash,
                                  qtcNode         * aFilter,
                                  void           ** aElement );

    static IDE_RC  getNextRange( qmcdMemHashTemp * aTempTable,
                                 void           ** aElement );

    //----------------------------
    // Same Row & NonHit �˻�
    //----------------------------

    static IDE_RC  getSameRowAndNonHit( qmcdMemHashTemp * aTempTable,
                                        UInt              aHash,
                                        void            * aRow,
                                        void           ** aElement );
    
    //----------------------------
    // Hit �˻�
    //----------------------------

    static IDE_RC  getFirstHit( qmcdMemHashTemp * aTempTable,
                                void           ** aElement );

    static IDE_RC  getNextHit( qmcdMemHashTemp * aTempTable,
                               void           ** aElement );
    
    //----------------------------
    // NonHit �˻�
    //----------------------------

    static IDE_RC  getFirstNonHit( qmcdMemHashTemp * aTempTable,
                                   void          ** aElement );

    static IDE_RC  getNextNonHit( qmcdMemHashTemp * aTempTable,
                               void           ** aElement );
    
    //----------------------------
    // ��Ÿ �Լ�
    //----------------------------

    // ���� ��� ���� ȹ��
    static IDE_RC  getDisplayInfo( qmcdMemHashTemp * aTempTable,
                                   SLong           * aRecordCnt,
                                   UInt            * aBucketCnt );
    
private:

    // Bucket ID�� ã�´�.
    static UInt   getBucketID( qmcdMemHashTemp * aTempTable,
                               UInt              aHash );

    // �� record���� ��� ��
    static SInt   compareRow ( qmcdMemHashTemp * aTempTable, 
                               void            * aElem1, 
                               void            * aElem2 );

    // Range �˻��� ���� �˻�
    static IDE_RC judgeFilter ( qmcdMemHashTemp * aTempTable,
                                void            * aElem,
                                idBool          * aResult );

    // Bucket�� �ڵ� Ȯ��
    static IDE_RC extendBucket (qmcdMemHashTemp * aTempTable );

    // ���ο� Bucket�� ���� Record ����
    static IDE_RC insertFirstNewBucket( qmcdMemHashTemp   * aTempTable,
                                        qmcMemHashElement * aElem );

    // ���ο� Bucket�� ���� record ����
    static IDE_RC insertNextNewBucket( qmcdMemHashTemp   * aTempTable,
                                       qmcMemHashElement * aElem );
    
};

#endif /* _O_QMC_MEM_HASH_TMEP_TABLE_H_ */

