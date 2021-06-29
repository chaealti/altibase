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
 * $Id: qmcSortTempTable.h 86786 2020-02-27 08:04:12Z donovan.seo $
 *
 * Description :
 *   [Sort Temp Table]
 *
 *   A4������ Memory �� Disk Table�� ��� �����Ѵ�.
 *   �߰� ����� �����ϴ� Node�� �� Sort�� �̿��� �߰� ����� �����
 *   Materialized Node���� Memory Sort Temp Table �Ǵ� Disk Sort Temp
 *   Table�� ����ϰ� �ȴ�.
 *
 *   �� ��, �� ��尡 Memory/Disk Sort Temp Table�� ���� ���� �ٸ�
 *   ������ ���� �ʵ��� �ϱ� ���Ͽ� ������ ���� Sort Temp Table��
 *   �̿��Ͽ� Transparent�� ���� �� ������ �����ϵ��� �Ѵ�.
 *
 *   �̷��� ������ ��Ÿ�� �׸��� �Ʒ��� ����.
 *
 *                                                     -------------
 *                                                  -->[Memory Sort]
 *     ---------------     -------------------      |  -------------
 *     | Plan Node   |---->| Sort Temp Table |------|  -----------
 *     ---------------     -------------------      -->[Disk Sort]
 *                                                     -----------
 *
 * ��� ���� :
 *
 * ��� :
 *
 **********************************************************************/

#ifndef _O_QMC_SORT_TMEP_TABLE_H_
#define _O_QMC_SORT_TMEP_TABLE_H_ 1

#include <qmcMemSortTempTable.h>
#include <qmcDiskSortTempTable.h>

/* qmcdSortTempTable.flag                                  */
#define QMCD_SORT_TMP_INITIALIZE                 (0x00000000)

/* qmcdSortTempTable.flag                                  */
// Sort Temp Table�� ���� ��ü
#define QMCD_SORT_TMP_STORAGE_TYPE               (0x00000004)
#define QMCD_SORT_TMP_STORAGE_MEMORY             (0x00000000)
#define QMCD_SORT_TMP_STORAGE_DISK               (0x00000004)

/* PROJ-2201 Innovation in sorting and hashing(temp)
 * QMNC_SORT_SEARCH_MASK �� ����� Flag�� ������ */
/* qmcdSortTempTable.flag                                  */
// Sort Temp Table�� Ž�� ���
#define QMCD_SORT_TMP_SEARCH_MASK               (0x00000008)
#define QMCD_SORT_TMP_SEARCH_SEQUENTIAL         (0x00000000)
#define QMCD_SORT_TMP_SEARCH_RANGE              (0x00000008)

// Sort Temp Table �ڷ� ����
typedef struct qmcdSortTemp
{
    UInt               flag;

    qcTemplate       * mTemplate;
    qmdMtrNode       * recordNode;  // Record ���� ����
    qmdMtrNode       * sortNode;    // Sort�� Column ����

    UInt               mtrRowSize;  // ���� Record�� ũ��
    UInt               nullRowSize; // ���� null record�� ũ��
    SLong              recordCnt;   // ����� Record ����
    qmcMemory        * memoryMgr;   // ���� Record�� ���� �޸� ������
    iduMemory        * memory;
    UInt               memoryIdx;

    SChar            * nullRow;     // Memory Temp Table�� ���� Null Row

    // Memory Temp Table�� ���� ����
    smiRange         * range;       // Range �˻��� ���� Key Range
    smiRange         * rangeArea;   // Range �˻��� ���� Key Range Area
    UInt               rangeAreaSize; // Range �˻��� ���� Key Range Area Size

    qmcdMemSortTemp  * memoryTemp;  // Memory Sort Temp Table
    qmcdDiskSortTemp * diskTemp;    // Disk Sort Temp Table

    idBool             existTempType;  // TEMP_TYPE �÷��� �����ϴ���
    void             * insertRow;   // insert�� ���� �ӽ� ����
    
} qmcdSortTemp;

class qmcSortTemp
{
public:

    //------------------------------------------------
    // Sort Temp Table�� ����
    //------------------------------------------------

    // Sort Temp Table�� �ʱ�ȭ�Ѵ�.
    static IDE_RC init( qmcdSortTemp * aTempTable,
                        qcTemplate   * aTemplate,
                        UInt           aMemoryIdx,
                        qmdMtrNode   * aRecordNode,
                        qmdMtrNode   * aSortNode,
                        SDouble        aStoreRowCount,
                        UInt           aFlag );

    // Sort Temp Table�� Data�� �����Ѵ�.
    static IDE_RC clear( qmcdSortTemp * aTempTable );

    // Sort Temp Table���� ��� Data�� flag�� �ʱ�ȭ�Ѵ�.
    static IDE_RC clearHitFlag( qmcdSortTemp * aTempTable );

    //------------------------------------------------
    // Sort Temp Table�� ����
    //------------------------------------------------

    // Data�� ���� Memory �Ҵ�
    static IDE_RC alloc( qmcdSortTemp  * aTempTable,
                         void         ** aRow );

    // Data�� ����
    static IDE_RC addRow( qmcdSortTemp * aTempTable,
                          void         * aRow );

    // insert row ����
    static IDE_RC makeTempTypeRow( qmcdSortTemp  * aTempTable,
                                   void          * aRow,
                                   void         ** aExtRow );
    
    // ����� Data���� ������ ����
    static IDE_RC sort( qmcdSortTemp * aTempTable );

    // Limit Sorting�� ������
    static IDE_RC shiftAndAppend( qmcdSortTemp * aTempTable,
                                  void         * aRow,
                                  void        ** aReturnRow );

    // Min-Max ������ ���� Limit Sorting�� ������
    static IDE_RC changeMinMax( qmcdSortTemp * aTempTable,
                                void         * aRow,
                                void        ** aReturnRow );

    //------------------------------------------------
    // Sort Temp Table�� �˻�
    //------------------------------------------------

    //-------------------------
    // ���� �˻�
    //-------------------------

    // ù��° ���� �˻� (ORDER BY, Two Pass Sort Join�� Left���� ���)
    static IDE_RC getFirstSequence( qmcdSortTemp * aTempTable,
                                    void        ** aRow );

    // �ι�° ���� �˻�
    static IDE_RC getNextSequence( qmcdSortTemp * aTempTable,
                                   void        ** aRow );

    //-------------------------
    // Range �˻�
    //-------------------------

    // ù��° Range �˻� (Sort Join�� Right���� ���)
    static IDE_RC getFirstRange( qmcdSortTemp * aTempTable,
                                 qtcNode      * aRangePredicate,
                                 void        ** aRow );

    // �ι�° Range �˻�
    static IDE_RC getNextRange( qmcdSortTemp * aTempTable,
                                void        ** aRow );

    //-------------------------
    // Hit �˻�
    //-------------------------

    // ù��° Hit Record �˻�
    static IDE_RC getFirstHit( qmcdSortTemp * aTempTable,
                               void        ** aRow );

    // �ι�° Hit Record �˻�
    static IDE_RC getNextHit( qmcdSortTemp * aTempTable,
                              void        ** aRow );

    //-------------------------
    // Non-Hit �˻�
    //-------------------------

    // ù��° Non Hit Record �˻�
    static IDE_RC getFirstNonHit( qmcdSortTemp * aTempTable,
                                  void        ** aRow );

    // �ι�° Non Hit Record �˻�
    static IDE_RC getNextNonHit( qmcdSortTemp * aTempTable,
                                 void        ** aRow );

    // Null Row �˻� (����� Null Row ȹ��)
    static IDE_RC getNullRow( qmcdSortTemp * aTempTable,
                              void        ** aRow );

    //------------------------------------------------
    // Sort Temp Table�� ��Ÿ �Լ�
    //------------------------------------------------

    // �˻��� Record�� Hit Flag ����
    static IDE_RC setHitFlag( qmcdSortTemp * aTempTable );

    // �˻��� Record�� Hit Flag�� �����Ǿ� �ִ��� Ȯ��
    static idBool isHitFlagged( qmcdSortTemp * aTempTable );

    // ���� Cursor ��ġ�� ���� (Merge Join���� ���)
    static IDE_RC storeCursor( qmcdSortTemp     * aTempTable,
                               smiCursorPosInfo * aCursorInfo );

    // ������ Cursor ��ġ�� ���� (Merge Join���� ���)
    static IDE_RC restoreCursor( qmcdSortTemp     * aTempTable,
                                 smiCursorPosInfo * aCursorInfo );

    // View Scan��� �����ϱ� ���� Information ����
    static IDE_RC getCursorInfo( qmcdSortTemp     * aTempTable,
                                 void            ** aTableHandle,
                                 void            ** aIndexHandle,
                                 qmcdMemSortTemp ** aMemSortTemp,
                                 qmdMtrNode      ** aMemSortRecord );

    // Plan Display�� ���� ���� ȹ��
    static IDE_RC getDisplayInfo( qmcdSortTemp * aTempTable,
                                  ULong        * aDiskPageCount,
                                  SLong        * aRecordCount );

    //------------------------------------------------
    // Window Sort (WNST)�� ���� ���
    //------------------------------------------------
    
    // �ʱ�ȭ�ǰ�, �����Ͱ� �Էµ� Temp Table�� ����Ű�� ����
    // (Window Sort���� ���)
    static IDE_RC setSortNode( qmcdSortTemp     * aTempTable,
                               const qmdMtrNode * aSortNode );

    // update�� ������ column list�� ����
    // ����: Disk Sort Temp�� ��� hitFlag�� �Բ� ��� �Ұ�
    static IDE_RC setUpdateColumnList( qmcdSortTemp     * aTempTable,
                                       const qmdMtrNode * aUpdateColumns );

    // ���� �˻� ���� ��ġ�� row�� update
    static IDE_RC updateRow( qmcdSortTemp * aTempTable );

    /* PROJ-1715 Get Last Key */
    static IDE_RC getLastKey( qmcdSortTemp * aTempTable,
                              SLong        * aLastKey );
    /* PROJ-1715 Set Last Key */
    static IDE_RC setLastKey( qmcdSortTemp * aTemptable,
                              SLong          aLastKey );

private:

    //------------------------------------------------
    // �ʱ�ȭ�� ���� �Լ�
    //------------------------------------------------

    // Memory Temp Table�� ���� NULL ROW ����
    static IDE_RC makeMemNullRow( qmcdSortTemp * aTempTable );

    //------------------------------------------------
    // Range �˻��� ���� �Լ�
    //------------------------------------------------

    // Memory Temp Table�� ���� Key Range ����
    static IDE_RC makeMemKeyRange( qmcdSortTemp * aTempTable,
                                   qtcNode      * aRangePredicate );

};

#endif /* _O_QMC_SORT_TMEP_TABLE_H_ */
