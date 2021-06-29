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
 * $Id: qmcMemPartHashTempTable.h 69506 2015-03-09 12:19:36Z interp $
 *
 * Description :
 *     Memory Hash Temp Table�� ���� ����
 *   - Partitioned Hash Algorithm�� ����Ѵ�.
 *
 * ��� ���� :
 *
 * ��� :
 *
 **********************************************************************/

#ifndef _O_QMC_MEM_PART_HASH_TMEP_TABLE_H_
#define _O_QMC_MEM_PART_HASH_TMEP_TABLE_H_ 1

#include <smi.h>
#include <qtcDef.h>
#include <qcuProperty.h>

// Partition ������ �ּҰ� : pow(2,10)
#define QMC_MEM_PART_HASH_MIN_PART_COUNT        ( 1024 )

// Partition ������ �ִ밪 : pow(2,24)
#define QMC_MEM_PART_HASH_MAX_PART_COUNT        ( 16777216 )

// Page ũ�� : 4KB�� �����ؼ� ����
#define QMC_MEM_PART_HASH_PAGE_SIZE             ( 4096 )

// TLB Entry Ȯ�� ���.
// TLB Entry ������ �� ���� ���� ������ �������� �����ص�
// TLB Miss�� ���ϰ� �Ͼ�� �����Ƿ� ���� ���� Fanout �ϴ� ���� �����ϴ�.
#define QMC_MEM_PART_HASH_TLB_ENTRY_MULTIPLIER  ( 4 )

// Optimal Partition Count�� ����� �� �����
// Partition �� ��� Element ���� (���� ��)
#define QMC_MEM_PART_HASH_AVG_RECORD_COUNT      ( 10 )

/*************************************************************************
 * qmcdMemPartHashTemp Flag
 *************************************************************************/

#define QMC_MEM_PART_HASH_INITIALIZE            (0x00000000)

// ù range search �� �غ� ����
#define QMC_MEM_PART_HASH_SEARCH_READY_MASK     (0x00000001)
#define QMC_MEM_PART_HASH_SEARCH_READY_FALSE    (0x00000000)
#define QMC_MEM_PART_HASH_SEARCH_READY_TRUE     (0x00000001)

//----------------------------------
// �˻� ������ (qmcMemHashItem)
// �迭�� ���ҷ� �����Ǹ�, Hash Key�� ���� Element�� ����Ų��.
//----------------------------------
typedef struct qmcMemHashItem
{
    UInt                mKey;
    qmcMemHashElement * mPtr;

} qmcMemHashItem;

//------------------------------------------------------------------------
// [Memory Hash Temp Table (Partitioned Hashing) �� �ڷ� ���� (PROJ-2553)]
//
//  DISTINCT Hashing������ ������� ������, JOIN�� ���� Hashing ������ ����Ѵ�.
//
//  ó������ �Ʒ��� ���� ���� List�� �޴´�.
//
//      ---------   --------   --------   --------   ------- 
//      | mHead |-->| elem |-->| elem |-->| elem |-->| ... |
//      ---------   --------   --------   --------   -------
//
//  ��� ���ԵǸ�, Element ���� (= Record ����)�� ���� Radix Bit�� �����Ѵ�.
//  Radix Bit�� Hash Key�� Masking �� �� ���Ǹ�, Radix Bit�� Ŭ���� Partition ������ ����.
//
//  �������� Radix Bit�� 2�� ������ ��, Partition ������ ����Ѵ�.
//  �� ����, �Ʒ��� ������ �غ��Ѵ�.
//
//   - �˻� �迭 (Search Array)
//      ���� �˻��� ����ϴ� �迭��,
//      ���Ե� Element�� Hash Key�� �����͸� ������ �ִ� Item�� �迭�� �̷���� �ִ�.
//      �迭�� ũ��� (Item�� ũ��) * (Element ����)�̴�.
//
//   - ������׷� (Histogram)
//      �˻� �迭���� �� Partition�� '���� ��ġ'�� ������ �迭�̴�.
//      �迭�� ũ��� (unsigned long) * (Partition ����)�� �ȴ�.
//
//                            SearchA.
//                            --------
//                        0   | item | --
//              Hist.         --------   | 
//             -------    1   | item |   | P1 
//          P1 |  0  |        --------   | 
//             -------    2   | item | --
//          P2 |  3  |        --------     
//             -------    3   | item | --
//             | ... |        --------   | P2 
//             -------    4   | item | --
//          Pn | X-1 |        --------     
//             -------        |  ... |       
//                            --------     
//                        X-1 | item | --
//                            --------   | Pn 
//                        X   | item | --
//                            --------    
//
//   Partition ������ Radix Bit Masking ���� �̷������,
//   Masking ��� ���� Partition �ε����� �ȴ�.
//
//      -------- 
//      | elem | -> Key = 17, RadixBit = 3 -> Partition 1
//      --------                              Why? 17 & ( (1<<3)-1 ) = 17 & 7 = 1
//
//------------------------------------------------------------------------

typedef struct qmcdMemPartHashTemp
{
    //----------------------------------------------------
    // �⺻ ����
    //----------------------------------------------------

    UInt                     mFlag;          // �˻� �غ� ����
    qcTemplate             * mTemplate;      // Template
    iduMemory              * mMemory;
    qmdMtrNode             * mRecordNode;    // Record ���� ����
    qmdMtrNode             * mHashNode;      // Hashing �� Column ����
    UInt                     mBucketCnt;     // �Է¹��� Bucket�� ����

    //----------------------------------------------------
    // Partitioned Hashing �� ���� ����
    //----------------------------------------------------

    UInt                     mRadixBit;      // Partition�� ���� RBit
    UInt                     mPartitionCnt;  // Partition�� ����
    ULong                  * mHistogram;     // Histogram 
    qmcMemHashItem         * mSearchArray;   // �˻� �迭

    //----------------------------------------------------
    // ������ ���� ����
    //----------------------------------------------------
    
    qmcMemHashElement      * mHead;          // ���Ե� Element�� ó��
    qmcMemHashElement      * mTail;          // ���Ե� Element�� ������
    SLong                    mRecordCnt;     // �� ���� Record ����

    //----------------------------------------------------
    // �˻��� ���� ����
    //----------------------------------------------------

    qmcMemHashElement      * mNextElem;      // ���� Ž���� ���� ���� Element
    UInt                     mKey;           // ���� �˻��� ���� Hash Key ��
    qtcNode                * mFilter;        // ���� �˻��� ���� Filter
    ULong                    mNextIdx;       // Search Array���� �˻��� Index
    ULong                    mBoundaryIdx;   // Search Array���� ���� ��Ƽ���� Index

} qmcdMemPartHashTemp;

class qmcMemPartHash
{
public:

    //------------------------------------------------
    // Memory Partitioned Hash Temp Table�� ����
    //------------------------------------------------
    
    // �ʱ�ȭ�� �Ѵ�.
    static IDE_RC init( qmcdMemPartHashTemp * aTempTable,
                        qcTemplate          * aTemplate,
                        iduMemory           * aMemory,
                        qmdMtrNode          * aRecordNode,
                        qmdMtrNode          * aHashNode,
                        UInt                  aBucketCnt );
    
    // Temp Table���� Bucket�� �ʱ�ȭ�Ѵ�.
    static IDE_RC clear( qmcdMemPartHashTemp * aTempTable );

    // ��� Record�� Hit Flag�� Reset
    static IDE_RC clearHitFlag( qmcdMemPartHashTemp * aTempTable );

    //------------------------------------------------
    // Memory Partitioned Hash Table�� ����
    //------------------------------------------------

    // ������ Record�� �����Ѵ�.
    static IDE_RC insertAny( qmcdMemPartHashTemp * aTempTable,
                             UInt                  aHash, 
                             void                * aElement,
                             void               ** aOutElement );

    //------------------------------------------------
    // Memory Partitioned Hash Temp Table�� �˻�
    //------------------------------------------------

    //----------------------------
    // ���� �˻�
    //----------------------------

    static IDE_RC getFirstSequence( qmcdMemPartHashTemp  * aTempTable,
                                    void                ** aElement );
    
    static IDE_RC getNextSequence( qmcdMemPartHashTemp  * aTempTable,
                                   void                ** aElement);

    //----------------------------
    // Range �˻�
    //----------------------------

    static IDE_RC getFirstRange( qmcdMemPartHashTemp  * aTempTable,
                                 UInt                   aHash,
                                 qtcNode              * aFilter,
                                 void                ** aElement );

    static IDE_RC getNextRange( qmcdMemPartHashTemp  * aTempTable,
                                void                ** aElement );

    //----------------------------
    // Same Row & NonHit �˻�
    // - Set Operation ���� ����ϹǷ�, ���⿡���� �������� �ʴ´�.
    //----------------------------

    //----------------------------
    // Hit �˻�
    //----------------------------

    static IDE_RC getFirstHit( qmcdMemPartHashTemp  * aTempTable,
                               void                ** aElement );

    static IDE_RC getNextHit( qmcdMemPartHashTemp  * aTempTable,
                              void                ** aElement );
    
    //----------------------------
    // NonHit �˻�
    //----------------------------

    static IDE_RC getFirstNonHit( qmcdMemPartHashTemp  * aTempTable,
                                  void                ** aElement );

    static IDE_RC getNextNonHit( qmcdMemPartHashTemp  * aTempTable,
                                 void                ** aElement );
    
    //----------------------------
    // ��Ÿ �Լ�
    //----------------------------

    // ���� ��� ���� ȹ��
    static IDE_RC getDisplayInfo( qmcdMemPartHashTemp * aTempTable,
                                  SLong               * aRecordCnt,
                                  UInt                * aBucketCnt );

private:

    // Range �˻��� ���� �˻�
    static IDE_RC judgeFilter( qmcdMemPartHashTemp * aTempTable,
                               void                * aElem,
                               idBool              * aResult );

    // �˻� �غ�
    static IDE_RC readyForSearch( qmcdMemPartHashTemp * aTempTable );

    // RBit ���
    static UInt calcRadixBit( qmcdMemPartHashTemp * aTempTable );

    // �� ���� �˻� �Լ� ����
    static IDE_RC fanoutSingle( qmcdMemPartHashTemp * aTempTable );

    // �� ���� ���� �˻� �Լ� ����
    static IDE_RC fanoutDouble( qmcdMemPartHashTemp * aTempTable );

    // ceil(log2()) 
    static UInt ceilLog2( ULong aNumber );
};

#endif /* _O_QMC_MEM_PART_HASH_TMEP_TABLE_H_ */
