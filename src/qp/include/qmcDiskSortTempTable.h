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
 * $Id: qmcDiskSortTempTable.h 86786 2020-02-27 08:04:12Z donovan.seo $
 *
 * Disk Sort Temp Table
 *
 **********************************************************************/

#ifndef _O_QMC_DISK_SORT_TMEP_TABLE_H_
#define _O_QMC_DISK_SORT_TMEP_TABLE_H_ 1

#include <qc.h>
#include <qmc.h>
#include <qmcTempTableMgr.h>

#define QMCD_DISK_SORT_TEMP_INITIALIZE            (0x00000000)

//---------------------------------------------------
// [Disk Sort Temp Table ��ü]
//    �ʱ�ȭ ������ ���� ����
//    (I) : ���� �ѹ��� �ʱ�ȭ��
//    (R) : ���� ���� �� �˻��ÿ� �ʱ�ȭ��
//    (X) : ��������
//---------------------------------------------------

typedef struct qmcdDiskSortTemp
{
    UInt             flag;            // (X)
    qcTemplate     * mTemplate;       // (I)

    void           * tableHandle;     // (I)
    void           * indexHandle;     // (I)
    ULong            mTempTableSize;

    // Record ������ ���� �ڷ� ����
    qmdMtrNode     * recordNode;      // (I) Record ���� ����
    smiTableCursor   insertCursor;    // (R) ������ ���� Cursor
    UInt             recordColumnCnt; // (I) Column ���� + 1(Hit Flag)
    smiColumnList  * insertColumn;    // (I) Insert Column ����
    smiValue       * insertValue;     // (I) Insert Value ����
    SChar          * nullRow;         // (I) Disk Temp Table�� ���� Null Row
    scGRID           nullRID;         // (I) SM�� Null Row�� RID

    // Sorting�� ���� �ڷ� ����
    qmdMtrNode     * sortNode;        // (I) Sort�� �÷� ����
    UInt             indexColumnCnt;  // (I) �ε��� �÷��� ����
    smiColumnList  * indexKeyColumnList; // (I) Key���� �ε��� �÷� ����
    mtcColumn      * indexKeyColumn;  // (I) Range�˻��� ���� �ε��� �÷� ����

    // �˻��� ���� �ڷ� ����
    smiSortTempCursor* searchCursor;    // (R) �˻� �� Update�� ���� Cursor
    qtcNode        * rangePredicate;    // (X) Key Range Predicate
    // PROJ-1431 : sparse B-Tree temp index�� ��������
    // leaf page(data page)���� row�� ���ϱ� ���� rowRange�� �߰���
    /* PROJ-2201 : Dense������ �ٽ� ����
     * Sparse������ BufferMiss��Ȳ���� I/O�� �ξ� ���߽�Ŵ */
    smiRange       * keyRange;          // (X) Row Range
    smiRange       * keyRangeArea;      // (X) Row Range Area
    UInt             keyRangeAreaSize;  // (X) Row Range Area Size
    // Update�� ���� �ڷ� ����
    smiColumnList  * updateColumnList;  // (R) Update Column ����
    smiValue       * updateValues;      // (R) Update Value ����
    
} qmcdDiskSortTemp;


class qmcDiskSort
{
public:

    //------------------------------------------------
    // Disk Sort Temp Table�� ����
    //------------------------------------------------

    // Temp Table�� �ʱ�ȭ
    static IDE_RC    init( qmcdDiskSortTemp * aTempTable,
                           qcTemplate       * aTemplate,
                           qmdMtrNode       * aRecordNode,
                           qmdMtrNode       * aSortNode,
                           SDouble            aStoreRowCount,
                           UInt               aMtrRowSize,     // Mtr Record Size
                           UInt               aFlag );

    /* PROJ-2201 ����Ű�� �����Ѵ�. */
    static IDE_RC    setSortNode( qmcdDiskSortTemp * aTempTable,
                                  const qmdMtrNode * aSortNode );

    // Temp Table���� ��� Record�� ����
    static IDE_RC    clear( qmcdDiskSortTemp * aTempTable );

    // Temp Table���� ��� Record�� Flag�� Clear
    static IDE_RC    clearHitFlag( qmcdDiskSortTemp * aTempTable );

    // Record�� ����
    static IDE_RC    sort( qmcdDiskSortTemp * aTempTable );

    //------------------------------------------------
    // Disk Sort Temp Table�� ����
    //------------------------------------------------

    // Record�� ����
    static IDE_RC    insert( qmcdDiskSortTemp * aTempTable, void * aRow );

    // ���� Cursor ��ġ�� Record�� Hit Flag�� Setting
    static IDE_RC    setHitFlag( qmcdDiskSortTemp * aTempTable );

    // ���� Cursor ��ġ�� Record�� Hit Flag�� �ִ��� �Ǵ�
    static idBool    isHitFlagged( qmcdDiskSortTemp * aTempTable );

    //------------------------------------------------
    // Disk Sort Temp Table�� �˻�
    //------------------------------------------------

    //----------------------------
    // ���� �˻�
    //----------------------------

    // ù��° ���� �˻�
    static IDE_RC    getFirstSequence( qmcdDiskSortTemp * aTempTable,
                                       void            ** aRow );

    // ���� ���� �˻�
    static IDE_RC    getNextSequence( qmcdDiskSortTemp * aTempTable,
                                      void             ** aRow );

    //----------------------------
    // Range �˻�
    //----------------------------

    // ù��° Range �˻�
    static IDE_RC    getFirstRange( qmcdDiskSortTemp * aTempTable,
                                    qtcNode          * aRangePredicate,
                                    void            ** aRow );

    // ���� Range �˻�
    static IDE_RC    getNextRange( qmcdDiskSortTemp * aTempTable,
                                   void            ** aRow );

    //----------------------------
    // Hit �˻�
    //----------------------------

    static IDE_RC    getFirstHit( qmcdDiskSortTemp * aTempTable,
                                  void            ** aRow );
    static IDE_RC    getNextHit( qmcdDiskSortTemp * aTempTable,
                                 void            ** aRow );

    //----------------------------
    // Non-Hit �˻�
    //----------------------------

    static IDE_RC    getFirstNonHit( qmcdDiskSortTemp * aTempTable,
                                     void            ** aRow );
    static IDE_RC    getNextNonHit( qmcdDiskSortTemp * aTempTable,
                                    void            ** aRow );

    //----------------------------
    // NULL Row �˻�
    //----------------------------

    static IDE_RC    getNullRow( qmcdDiskSortTemp * aTempTable,
                                 void            ** aRow );


    //------------------------------------------------
    // Disk Sort Temp Table�� ��Ÿ �Լ�
    //------------------------------------------------

    // ���� Cursor ��ġ ����
    static IDE_RC    getCurrPosition( qmcdDiskSortTemp * aTempTable,
                                      smiCursorPosInfo * aCursorInfo );

    // ���� Cursor ��ġ�� ����
    static IDE_RC    setCurrPosition( qmcdDiskSortTemp * aTempTable,
                                      smiCursorPosInfo * aCursorInfo );

    // View Scan��� �����ϱ� ���� Information ����
    static IDE_RC    getCursorInfo( qmcdDiskSortTemp * aTempTable,
                                    void            ** aTableHandle,
                                    void            ** aIndexHandle );

    // Plan Display�� ���� ���� ȹ��
    static IDE_RC    getDisplayInfo( qmcdDiskSortTemp * aTempTable,
                                     ULong            * aPageCount,
                                     SLong            * aRecordCount );

    //------------------------------------------------
    // Window Sort (WNST)�� ���� ���
    //------------------------------------------------

    // update�� ������ column list�� ����
    // ����: Disk Sort Temp�� ��� hitFlag�� �Բ� ��� �Ұ�
    static IDE_RC setUpdateColumnList( qmcdDiskSortTemp * aTempTable,
                                       const qmdMtrNode * aUpdateColumns );

    // ���� �˻� ���� ��ġ�� row�� update
    static IDE_RC updateRow( qmcdDiskSortTemp * aTempTable );

private:

    //------------------------------------------------
    // �ʱ�ȭ�� ���� �Լ�
    //------------------------------------------------
    // Temp Table Size�� ����
    static void    tempSizeEstimate( qmcdDiskSortTemp * aTempTable,
                                     SDouble            aStoreRowCount,
                                     UInt               aMtrRowSize );

    // Disk Temp Table�� ����
    static IDE_RC    createTempTable( qmcdDiskSortTemp * aTempTable );

    // Insert Column List�� ���� ���� ����
    static IDE_RC    setInsertColumnList( qmcdDiskSortTemp * aTempTable );

    // Index Key�������� Index Column ������ ����
    static IDE_RC    makeIndexColumnInfoInKEY( qmcdDiskSortTemp * aTempTable);

    //------------------------------------------------
    // ���� �� �˻��� ���� �Լ�
    //------------------------------------------------

    static IDE_RC    openCursor( qmcdDiskSortTemp  * aTempTable,
                                 UInt                aFlag,
                                 smiRange          * aKeyRange,
                                 smiRange          * aKeyFilter,
                                 smiCallBack       * aRowFilter,
                                 smiSortTempCursor** aTargetCursor );

        // Insert�� ���� Value ���� ����
    static IDE_RC    makeInsertSmiValue ( qmcdDiskSortTemp * aTempTable );

    // PROJ-1431 : Key and Row Range�� �����Ѵ�.
    static IDE_RC    makeRange( qmcdDiskSortTemp * aTempTable );

    //------------------------------------------------
    // Update ó���� ���� �Լ�
    //------------------------------------------------

    // Update�� ���� Value ���� ����
    // Hit Flag�� update�� WNST�� update���� ���
    static IDE_RC    makeUpdateSmiValue ( qmcdDiskSortTemp * aTempTable );

    //------------------------------------------------
    // Hit Flag ó���� ���� �Լ�
    //------------------------------------------------

    static void setColumnLengthType( qmcdDiskSortTemp * aTempTable );

};

#endif /* _O_QMC_DISK_SORT_TMEP_TABLE_H_ */
