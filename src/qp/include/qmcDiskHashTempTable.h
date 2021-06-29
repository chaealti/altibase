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
 * $Id: qmcDiskHashTempTable.h 85333 2019-04-26 02:34:41Z et16 $
 *
 * Description :
 *     Disk Hash Temp Table�� ���� ����
 *
 * ��� ���� :
 *
 * ��� :
 *
 **********************************************************************/

#ifndef _O_QMC_DISK_HASH_TMEP_TABLE_H_
#define _O_QMC_DISK_HASH_TMEP_TABLE_H_ 1

#include <qc.h>
#include <qmc.h>
#include <qmcTempTableMgr.h>
#include <smiDef.h>

/* qmcdDiskHashTemp.flag                                    */
#define QMCD_DISK_HASH_TEMP_INITIALIZE            (0x00000000)

/* qmcdDiskHashTemp.flag                                    */
// Distinct Insertion ����
#define QMCD_DISK_HASH_INSERT_DISTINCT_MASK       (0x00000001)
#define QMCD_DISK_HASH_INSERT_DISTINCT_FALSE      (0x00000000)
#define QMCD_DISK_HASH_INSERT_DISTINCT_TRUE       (0x00000001)

//---------------------------------------------------
// [Disk Hash Temp Table ��ü]
//    �ʱ�ȭ ������ ���� ����
//    (I) : ���� �ѹ��� �ʱ�ȭ��
//    (R) : ���� ���� �� �˻��ÿ� �ʱ�ȭ��
//    (X) : ��������
//---------------------------------------------------

typedef struct qmcdDiskHashTemp
{
    UInt             flag;            // (X)
    qcTemplate     * mTemplate;       // (I)
    void           * tableHandle;     // (I)
    ULong            mTempTableSize;

    // Record ������ ���� �ڷ� ����
    qmdMtrNode     * recordNode;      // (I) Record ���� ����
    UInt             recordColumnCnt; // (I) Column ���� + 1(Hit Flag)
    smiColumnList  * insertColumn;    // (I) Insert Column ����
    smiValue       * insertValue;     // (I) Insert Value ����

    // Hashing �� ���� �ڷ� ����
    qmdMtrNode     * hashNode;          // (I) Hash�� �÷� ����
    UInt             hashColumnCnt;     // (I) �ε��� �÷��� ����
    smiColumnList  * hashKeyColumnList; // (I) Key���� �ε��� �÷� ����
    mtcColumn      * hashKeyColumn;     // (I) �ε��� �÷� ����

    // �˻��� ���� �ڷ� ����
    smiHashTempCursor  * searchCursor;     // (R) �˻� �� Update�� ���� Cursor
    smiHashTempCursor  * groupCursor;      // (R) Group�˻� �� Aggregation Update
    smiHashTempCursor  * groupFiniCursor;  // (R) Group�� Aggregation Finalization
    smiHashTempCursor  * hitFlagCursor;    // (R) Hit Flag �˻縦 ���� Cursor
    qtcNode        * hashFilter;       // (X) Hash Filter

    // Filter ������ ���� �ڷ� ����
    smiCallBack         filterCallBack;     // (R) CallBack
    qtcSmiCallBackData  filterCallBackData; // (R) CallBack Data

    // Aggregation Update�� ���� �ڷ� ����
    qmdMtrNode     * aggrNode;        // (I) Aggregation Column ����
    UInt             aggrColumnCnt;   // (I) Aggregation Column�� ����
    smiColumnList  * aggrColumnList;  // (I) Aggregation Column ����
    smiValue       * aggrValue;       // (R) Aggregation Value ����
} qmcdDiskHashTemp;

class qmcDiskHash
{
public:
    //------------------------------------------------
    // Disk Hash Temp Table�� ����
    //------------------------------------------------

    // Temp Table�� �ʱ�ȭ
    static IDE_RC    init( qmcdDiskHashTemp * aTempTable,
                           qcTemplate       * aTemplate,   // Template
                           qmdMtrNode       * aRecordNode, // Record ����
                           qmdMtrNode       * aHashNode,   // Hashing Column
                           qmdMtrNode       * aAggrNode,   // Aggregation
                           UInt               aBucketCnt,  // Bucket Count
                           UInt               aMtrRowSize, // Mtr Record Size
                           idBool             aDistinct ); // Distinction����

    // Temp Table���� ��� Record�� ����
    static IDE_RC    clear( qmcdDiskHashTemp * aTempTable );

    // Temp Table���� ��� Record�� Flag�� Clear
    static IDE_RC    clearHitFlag( qmcdDiskHashTemp * aTempTable );

    //------------------------------------------------
    // Disk Hash Temp Table�� ����
    //------------------------------------------------

    // Record�� ����
    static IDE_RC    insert( qmcdDiskHashTemp * aTempTable,
                             UInt               aHashKey,
                             idBool           * aResult );

    // Aggregation Column�� Update
    static IDE_RC    updateAggr( qmcdDiskHashTemp * aTempTable );

    // Aggregation Column�� Final Update
    static IDE_RC    updateFiniAggr( qmcdDiskHashTemp * aTempTable );

    // ���� Cursor ��ġ�� Record�� Hit Flag�� Setting
    static IDE_RC    setHitFlag( qmcdDiskHashTemp * aTempTable );

    // ���� Cursor ��ġ�� Record�� Hit Flag�� �ִ��� �Ǵ�
    static idBool    isHitFlagged( qmcdDiskHashTemp * aTempTable );

    // Update Cursor�� �̿��� ���� �˻�
    static IDE_RC    getFirstGroup( qmcdDiskHashTemp * aTempTable,
                                    void            ** aRow );
    // Update Cursor�� �̿��� ���� �˻�
    static IDE_RC    getNextGroup( qmcdDiskHashTemp * aTempTable,
                                   void            ** aRow );

    //------------------------------------------------
    // Disk Hash Temp Table�� �˻�
    //------------------------------------------------

    //----------------------------
    // ���� �˻�
    //----------------------------

    static IDE_RC    getFirstSequence( qmcdDiskHashTemp * aTempTable,
                                       void            ** aRow );
    static IDE_RC    getNextSequence( qmcdDiskHashTemp * aTempTable,
                                      void            ** aRow );

    //----------------------------
    // Range �˻�
    //----------------------------

    static IDE_RC    getFirstRange( qmcdDiskHashTemp * aTempTable,
                                    UInt               aHash,
                                    qtcNode          * aHashFilter,
                                    void            ** aRow );
    static IDE_RC    getNextRange( qmcdDiskHashTemp * aTempTable,
                                   void            ** aRow );

    //----------------------------
    // Hit �˻�
    //----------------------------

    static IDE_RC    getFirstHit( qmcdDiskHashTemp * aTempTable,
                                  void            ** aRow );
    static IDE_RC    getNextHit( qmcdDiskHashTemp * aTempTable,
                                 void            ** aRow );

    //----------------------------
    // NonHit �˻�
    //----------------------------

    static IDE_RC    getFirstNonHit( qmcdDiskHashTemp * aTempTable,
                                     void            ** aRow );
    static IDE_RC    getNextNonHit( qmcdDiskHashTemp * aTempTable,
                                    void            ** aRow );

    //----------------------------
    // Same Row & NonHit �˻�
    //----------------------------

    static IDE_RC    getSameRowAndNonHit( qmcdDiskHashTemp * aTempTable,
                                          UInt               aHash,
                                          void             * aRow,
                                          void            ** aResultRow );

    static IDE_RC    getNullRow( qmcdDiskHashTemp * aTempTable,
                                 void            ** aRow );

    //-------------------------
    // To Fix PR-8213
    // Same Group �˻� (Group Aggregation)������ ���
    //-------------------------

    static IDE_RC    getSameGroup( qmcdDiskHashTemp * aTempTable,
                                   UInt               aHash,
                                   void             * aRow,
                                   void            ** aResultRow );

    //------------------------------------------------
    // ��Ÿ �Լ�
    //------------------------------------------------

    // ���� ��� ���� ȹ��
    static IDE_RC  getDisplayInfo( qmcdDiskHashTemp * aTempTable,
                                   ULong            * aPageCount,
                                   SLong            * aRecordCount );

private:

    //------------------------------------------------
    // �ʱ�ȭ�� ���� �Լ�
    //------------------------------------------------
    // Temp Table Size�� ����
    static void    tempSizeEstimate( qmcdDiskHashTemp * aTempTable,
                                     UInt               aBucketCnt,
                                     UInt               aMtrRowSize );

    // Temp Table�� ����
    static IDE_RC  createTempTable( qmcdDiskHashTemp * aTempTable );

    // Insert Column List�� ���� ���� ����
    static IDE_RC  setInsertColumnList( qmcdDiskHashTemp * aTempTable );

    // Aggregation Column List�� ���� ���� ����
    static IDE_RC  setAggrColumnList( qmcdDiskHashTemp * aTempTable );

    // Index Key�������� Index Column ������ ����
    static IDE_RC  makeIndexColumnInfoInKEY( qmcdDiskHashTemp * aTempTable);

    //------------------------------------------------
    // ���� �� �˻��� ���� �Լ�
    //------------------------------------------------

    // Insert�� ���� Value ���� ����
    static IDE_RC  makeInsertSmiValue ( qmcdDiskHashTemp * aTempTable );

    // Aggregation�� Update�� ���� Value ���� ����
    static IDE_RC  makeAggrSmiValue ( qmcdDiskHashTemp * aTempTable );

    // �־��� Filter�� �����ϴ����� �˻��ϴ� Filter ����
    static IDE_RC  makeHashFilterCallBack( qmcdDiskHashTemp * aTempTable );

    // �� Row�� ���� Hash Value������ �ϴ� Filter CallBack�� ����
    static IDE_RC  makeTwoRowHashFilterCallBack( qmcdDiskHashTemp * aTempTable,
                                                 void             * aRow );

    // �� Row���� Hashing Column�� �ٸ����� �˻�
    static IDE_RC  hashTwoRowRangeFilter( idBool     * aResult,
                                          const void * aRow,
                                          void       *, /* aDirectKey */
                                          UInt        , /* aDirectKeyPartialSize */
                                          const scGRID aRid,
                                          void       * aData );

    // �� Row�� ������ Hashing Column�̰� Hit Flag�� ���� ���� �˻�.
    static IDE_RC  hashTwoRowAndNonHitFilter( idBool     * aResult,
                                              const void * aRow,
                                              const scGRID  aRid,
                                              void       * aData );
};

#endif /* _O_QMC_DISK_HASH_TMEP_TABLE_H_ */
