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
 * $Id: qmcInsertCursor.h 18910 2006-11-13 01:56:34Z mhjeong $
 **********************************************************************/

/***********************************************************************
 *
 *  Description : PROJ-1502 PARTITIONED DISK TABLE
 *
 *  partitioned table�� non partitioned table�� ����
 *  insert cursor�� �����Ѵ�.
 *
 ***********************************************************************/


#ifndef _O_QMC_INSERT_CURSOR_H_
#define _O_QMC_INSERT_CURSOR_H_ 1

typedef struct qmcInsertPartCursor
{
    idBool            isSetColumnsForNewRow;
    qmsPartitionRef * partitionRef;
    qcmColumn       * columnsForNewRow;
    smiTableCursor    cursor;
} qmcInsertPartCursor;

typedef struct qmxLobInfo qmxLobInfo;

//-----------------------------------------------------------
// BUG-34085 partition lock pruning
// partition �˻������ ���̱� ���� cursor array�� index�� ����Ѵ�.
//
// partition count = 6
// mCursorIndexCount = 2
//
// mCursorIndex
// +--+--+--+--+--+--+
// |* |* |  |  |  |  |
// +--+--+--+--+--+--+
//  |  |
//  |  +-------------------------+
//  |                             |
//  +---------------+             |
//                  |             |
//                  v             v
// mCursors
// +------+------+------+------+------+------+
// |cursor|cursor|cursor|cursor|cursor|cursor|
// |      |      |opened|      |opened|      |
// +------+------+------+------+------+------+
//    p0     p1     p2     p3     p4     p5
//
//-----------------------------------------------------------

class qmcInsertCursor
{
public:
    // partition �˻������ ���̱� ���� cursor array�� index�� ����Ѵ�.
    qmcInsertPartCursor ** mCursorIndex;
    UInt                   mCursorIndexCount;
    qmcInsertPartCursor  * mCursors;

    qmcInsertPartCursor  * mSelectedCursor;
    
private:

    idBool                 mIsPartitioned;
    qmsTableRef          * mTableRef;

    // non-partitioned table inset cursor�� ��� alloc���� �ʰ�
    // internal cursor�� ����Ѵ�.
    qmcInsertPartCursor  * mInternalCursorIndex;
    qmcInsertPartCursor    mInternalCursor;

    // �������� cursor�� open�� �� �ֵ��� ������ �����Ѵ�.
    smiStatement         * mCursorSmiStmt;
    UInt                   mCursorFlag;
    smiCursorProperties    mCursorProperties;
    
public:

    IDE_RC initialize( iduMemory   * aMemory,
                       qmsTableRef * aTableRef,
                       idBool        aAllocPartCursors,
                       idBool        aInitPartCursors );

    IDE_RC openCursor( qcStatement         * aStatement,
                       UInt                  aFlag,
                       smiCursorProperties * aProperties );

    IDE_RC partitionFilteringWithRow( smiValue      * aValues,
                                      qmxLobInfo    * aLobInfo,
                                      qcmTableInfo ** aSelectedTableInfo );

    IDE_RC getCursor( smiTableCursor ** aCursor );

    IDE_RC getSelectedPartitionOID( smOID * aPartOID );

    /* PROJ-2464 hybrid partitioned table ���� */
    IDE_RC getSelectedPartitionTupleID( UShort * aPartTupleID );

    IDE_RC closeCursor();

    IDE_RC setColumnsForNewRow();

    IDE_RC getColumnsForNewRow( qcmColumn ** aColumnsForNewRow );

    IDE_RC clearColumnsForNewRow();

private:

    IDE_RC openInsertPartCursor( qmcInsertPartCursor * aPartitionCursor,
                                 qmsPartitionRef     * aPartitionRef );

    IDE_RC addInsertPartCursor( qmcInsertPartCursor * aPartitionCursor,
                                qmsPartitionRef     * aPartitionRef );
};

#endif /* _O_QMC_INSERT_CURSOR_H_ */
