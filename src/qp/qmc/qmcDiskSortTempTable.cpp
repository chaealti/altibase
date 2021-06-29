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
 * $Id: qmcDiskSortTempTable.cpp 86786 2020-02-27 08:04:12Z donovan.seo $
 *
 * Description :
 *     Disk Sort Temp Table
 *
 * ��� ���� :
 *
 * ��� :
 *
 **********************************************************************/

#include <idl.h>
#include <ide.h>
#include <qcg.h>
#include <qdParseTree.h>
#include <qdx.h>
#include <qmoKeyRange.h>
#include <qcm.h>
#include <qcmTableSpace.h>
#include <qmcSortTempTable.h>
#include <qmcDiskSortTempTable.h>
#include <qdbCommon.h>

extern mtdModule mtdInteger;

IDE_RC
qmcDiskSort::init( qmcdDiskSortTemp * aTempTable,
                   qcTemplate       * aTemplate,
                   qmdMtrNode       * aRecordNode,
                   qmdMtrNode       * aSortNode,
                   SDouble            aStoreRowCount,
                   UInt               aMtrRowSize,
                   UInt               aFlag )
{
/***********************************************************************
 *
 * Description :
 *    Disk Temp Table�� �ʱ�ȭ�Ѵ�.
 *
 * Implementation :
 *    - Disk Temp Table�� Member�� �ʱ�ȭ�Ѵ�.
 *    - Record ���� �����κ��� Disk Temp Table�� �����Ѵ�.
 *    - Sort ���� �����κ��� Index�� �����Ѵ�.
 *    - Insert�� ���� Cursor�� ����.
 *
 ***********************************************************************/

#define IDE_FN "qmcDiskSort::init"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY("qmcDiskSort::init"));

    UInt i;
    qmdMtrNode * sNode;

    IDE_DASSERT( aTempTable != NULL );
    IDE_DASSERT( aRecordNode != NULL );

    //----------------------------------------------------------------
    // Disk Temp Table ����� �ʱ�ȭ
    //----------------------------------------------------------------
    aTempTable->flag      = aFlag;
    aTempTable->mTemplate = aTemplate;

    aTempTable->tableHandle = NULL;
    aTempTable->indexHandle = NULL;

    tempSizeEstimate( aTempTable, aStoreRowCount, aMtrRowSize );

    //---------------------------------------
    // Record ������ ���� �ڷ� ������ �ʱ�ȭ
    //---------------------------------------
    aTempTable->recordNode = aRecordNode;

    // [Record Column ������ ����]
    // Record Node�� ������ �Ǵ����� �ʰ�,
    // Tuple Set�� �����κ��� Column ������ �Ǵ��Ѵ�.
    // �̴� �ϳ��� Record Node�� ���� ���� Column���� ������ �� �ֱ� �����̴�.

    IDE_DASSERT( aRecordNode->dstTuple->columnCount > 0 );
    aTempTable->recordColumnCnt = aRecordNode->dstTuple->columnCount;

    aTempTable->insertColumn = NULL;
    aTempTable->insertValue = NULL;
    aTempTable->nullRow = NULL;

    //---------------------------------------
    // Sorting�� ���� �ڷ� ������ �ʱ�ȭ
    //---------------------------------------

    aTempTable->sortNode = aSortNode;

    // [Sort Column ������ ����]
    // Record Column�� �޸� Sort Node �����κ��� Column ������ �Ǵ��Ѵ�.
    // �ϳ��� Node�� ���� ���� Column���� �����ȴ� �ϴ���
    // Sorting ����� �Ǵ� Column�� �ϳ��̱� �����̴�.

    for ( i = 0, sNode = aTempTable->sortNode;
          sNode != NULL;
          sNode = sNode->next, i++ ) ;
    aTempTable->indexColumnCnt = i;

    aTempTable->indexKeyColumnList = NULL;
    aTempTable->indexKeyColumn = NULL;
    aTempTable->searchCursor = NULL;

    aTempTable->rangePredicate = NULL;

    // PROJ-1431 : sparse B-Tree temp index�� ��������
    // leaf page(data page)���� row�� ���ϱ� ���� rowRange�� �߰���
    /* PROJ-2201 : Dense������ �ٽ� ����
     * Sparse������ BufferMiss��Ȳ���� I/O�� ���� ���߽�Ŵ */
    aTempTable->keyRange       = NULL;
    aTempTable->keyRangeArea   = NULL;
    aTempTable->keyRangeAreaSize = 0;

    // ����(update)�� ���� �ڷ� ������ �ʱ�ȭ
    aTempTable->updateColumnList = NULL;
    aTempTable->updateValues     = NULL;

    //----------------------------------------------------------------
    // Disk Temp Table �� ����
    //----------------------------------------------------------------

    IDE_TEST( createTempTable( aTempTable ) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

/******************************************************************************
 *
 * Description : BUG-37862 disk sort temp table size estimate
 *                WAMap size + temp table size
 *
 *****************************************************************************/
void  qmcDiskSort::tempSizeEstimate( qmcdDiskSortTemp * aTempTable,
                                     SDouble            aStoreRowCount,
                                     UInt               aMtrRowSize )
{
    ULong        sTempSize;

    // BUG-37862 disk sort temp table size estimate
    // aStoreRowCount �� disk temp table�� ���� stored row count�̴�.
    // BUG-43443 temp table�� ���ؼ� work area size�� estimate�ϴ� ����� off
    if ( (QCU_DISK_TEMP_SIZE_ESTIMATE == 1) &&
         (aStoreRowCount != 0) )
    {
        // sdtSortModule::calcEstimatedStats( smiTempTableHeader * aHeader ) �� ������
        sTempSize = (ULong)(SMI_TR_HEADER_SIZE_FULL + aMtrRowSize) * DOUBLE_TO_UINT64(aStoreRowCount);
        sTempSize = (ULong)(sTempSize * 100.0 / smuProperty::getTempSortGroupRatio() * 1.3);

        aTempTable->mTempTableSize = IDL_MIN( sTempSize, smuProperty::getSortAreaSize());
    }
    else
    {
        // 0�� �����ϸ� SM���� smuProperty::getSortAreaSize() �� �����Ѵ�.
        aTempTable->mTempTableSize = 0;
    }
}

IDE_RC
qmcDiskSort::clear( qmcdDiskSortTemp * aTempTable )
{
/***********************************************************************
 *
 * Description :
 *    �ش� Temp Table�� Truncate�Ѵ�.
 *
 * Implementation :
 *    ���� �ִ� Ŀ���� ��� �ݰ�, Table�� Truncate�Ѵ�.
 *
 ***********************************************************************/

#define IDE_FN "qmcDiskSort::clear"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY("qmcDiskSort::clear"));

    IDE_TEST( smiSortTempTable::clear( aTempTable->tableHandle )
              != IDE_SUCCESS );

    aTempTable->searchCursor = NULL;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
#undef IDE_FN
}

IDE_RC
qmcDiskSort::clearHitFlag( qmcdDiskSortTemp * aTempTable )
{
/***********************************************************************
 *
 * Description :
 *    Temp Table���� �����ϴ� ��� Record�� Hit Flag�� �ʱ�ȭ�Ѵ�.
 *
 * Implementation :
 *    Memory Temp Table�� �޸� ��� Record�� ���Ͽ� �ʱ�ȭ���� �ʰ�,
 *    Hit Flag�� Setting�� �͸��� �ʱ�ȭ�Ѵ�.
 *    �̴� Hit Flag�� �ʱ�ȭ�� Update������ �߻�����, Disk I/O ���ϰ�
 *    ������ �� �ֱ� �����̴�.
 *
 *    - Hit Flag �� ������ ���� �˻��ϴ� Filter CallBack�� �����Ѵ�.
 *    - Hit Flag Search Cursor�� Open�Ѵ�.
 *    - ������ �ݺ��Ѵ�.
 *        - �����ϴ� Record �� ���� ���, Hit Flag�� Clear�Ѵ�.
 *        - �ش� Record�� Update�Ѵ�.
 *
 ***********************************************************************/

#define IDE_FN "qmcDiskSort::clearHitFlag"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY("qmcDiskSort::clearHitFlag"));

    smiSortTempTable::clearHitFlag( aTempTable->tableHandle );

    return IDE_SUCCESS;
#undef IDE_FN
}



IDE_RC
qmcDiskSort::insert( qmcdDiskSortTemp * aTempTable, void * aRow )
{
/***********************************************************************
 *
 * Description :
 *    Record�� Temp Table�� �����Ѵ�.
 *
 * Implementation :
 *    - Cursor�� ������ ���� �ʴ´�.  �ʱ�ȭ �Ǵ� Clear�� Insert Cursor��
 *      �̹� ���� ���´�.
 *    - Row�� Hit Flag�� Clear�Ѵ�.
 *    - smiValue�� �����Ѵ�
 ***********************************************************************/

#define IDE_FN "qmcDiskSort::insert"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY("qmcDiskSort::insert"));

    // ���ռ� �˻�
    // insert value ������ dstTuple->row�� �߽����� �����ȴ�.
    // ����, aRow�� ���� �׻� �����Ͽ��� �Ѵ�.
    IDE_DASSERT( aRow == aTempTable->recordNode->dstTuple->row );

    // PROJ-1597 Temp record size ���� ����
    // �� row���� smiValue�� �籸���Ѵ�.
    IDE_TEST( makeInsertSmiValue(aTempTable) != IDE_SUCCESS );

    // Temp Table�� Record�� �����Ѵ�.
    IDE_TEST( smiSortTempTable::insert( aTempTable->tableHandle,
                                        aTempTable->insertValue )
         != IDE_SUCCESS )

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}


IDE_RC
qmcDiskSort::setHitFlag( qmcdDiskSortTemp * aTempTable )
{
/***********************************************************************
 *
 * Description :
 *    ���� Cursor ��ġ�� Record�� Hit Flag�� Setting�Ѵ�.
 *
 * Implementation :
 *    �˻� �Ŀ��� ȣ��ȴ�.
 *    �̹� �˻��� Record�� ���� Hit Flag�� Setting�ϸ� �ȴ�.
 *
 ***********************************************************************/

#define IDE_FN "qmcDiskSort::setHitFlag"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY("qmcDiskSort::setHitFlag"));

    smiSortTempTable::setHitFlag( aTempTable->searchCursor );

    return IDE_SUCCESS;
#undef IDE_FN
}

idBool qmcDiskSort::isHitFlagged( qmcdDiskSortTemp * aTempTable )
{
/***********************************************************************
 *
 * Description : PROJ-2385
 *    ���� Cursor ��ġ�� Record�� Hit Flag�� �ִ��� �Ǵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

    return smiSortTempTable::isHitFlagged( aTempTable->searchCursor );
}


IDE_RC
qmcDiskSort::getFirstSequence( qmcdDiskSortTemp * aTempTable,
                               void            ** aRow )
{
/***********************************************************************
 *
 * Description :
 *    ���� �˻��� �Ѵ�.
 *
 * Implementation :
 *    Search Cursor�� ���� �˻��� ���� ����, �ش� Row�� ���´�.
 *    Record�� ���� ���� ����(���� ������ ���)��
 *    Plan Node���� ���� ��� ó���Ͽ��� �Ѵ�.
 *
 ***********************************************************************/

#define IDE_FN "qmcDiskSort::getFirstSequence"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY("qmcDiskSort::getFirstSequence"));

    // ���ռ� �˻�
    // To Fix PR-7995
    IDE_DASSERT( *aRow == aTempTable->recordNode->dstTuple->row );

    // ���� �˻� Cursor�� ����.
    IDE_TEST( openCursor(aTempTable, 
                         SMI_TCFLAG_FORWARD | 
                         SMI_TCFLAG_ORDEREDSCAN |
                         SMI_TCFLAG_IGNOREHIT,
                         smiGetDefaultKeyRange(),
                         smiGetDefaultKeyRange(),
                         smiGetDefaultFilter(),
                         &aTempTable->searchCursor )
              != IDE_SUCCESS );

    // Record�� ��� �´�.
    IDE_TEST( smiSortTempTable::fetch( aTempTable->searchCursor,
                                       (UChar**) aRow,
                                       & aTempTable->recordNode->dstTuple->rid )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qmcDiskSort::getNextSequence( qmcdDiskSortTemp  * aTempTable,
                              void             ** aRow )
{
/***********************************************************************
 *
 * Description :
 *    ���� ���� �˻��� �Ѵ�.
 *    �ݵ�� getFirstSequence() ���Ŀ� ȣ��Ǿ�� �Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qmcDiskSort::getNextSequence"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY("qmcDiskSort::getNextSequence"));

    // ���ռ� �˻�
    // To Fix PR-7995
    IDE_DASSERT( *aRow == aTempTable->recordNode->dstTuple->row );

    // Record�� ��� �´�.
    IDE_TEST( smiSortTempTable::fetch( aTempTable->searchCursor,
                                       (UChar**) aRow,
                                       & aTempTable->recordNode->dstTuple->rid )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qmcDiskSort::getFirstRange( qmcdDiskSortTemp  * aTempTable,
                            qtcNode           * aRangePredicate,
                            void             ** aRow )
{
/***********************************************************************
 *
 * Description :
 *    ù��° Range �˻��� �Ѵ�.
 *
 * Implementation :
 *    ������ ���� ������ ���Ͽ� Range�˻��� �����Ѵ�.
 *
 *        - �־��� Predicate�� ���� Key Range�� �����Ѵ�.
 *        - Range Cursor�� Open �Ѵ�.
 *        - Cursor�� �̿��Ͽ� ���ǿ� �´� Row�� �����´�.
 *
 ***********************************************************************/

#define IDE_FN "qmcDiskSort::getFirstRange"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY("qmcDiskSort::getFirstRange"));

    // ���ռ� �˻�
    IDE_DASSERT( aRangePredicate != NULL );
    IDE_DASSERT( *aRow == aTempTable->recordNode->dstTuple->row );

    // Key Range ����
    aTempTable->rangePredicate = aRangePredicate;
    // PROJ-1431 : key range�� row range�� ����
    IDE_TEST( makeRange( aTempTable ) != IDE_SUCCESS );

    // Range Cursor�� Open�Ѵ�.
    IDE_TEST( openCursor(aTempTable, 
                         SMI_TCFLAG_FORWARD | 
                         SMI_TCFLAG_RANGESCAN |
                         SMI_TCFLAG_IGNOREHIT,
                         aTempTable->keyRange,
                         smiGetDefaultKeyRange(),
                         smiGetDefaultFilter(),
                         &aTempTable->searchCursor )
              != IDE_SUCCESS );

    // Record�� ��� �´�.
    IDE_TEST( smiSortTempTable::fetch( aTempTable->searchCursor,
                                       (UChar**) aRow,
                                       & aTempTable->recordNode->dstTuple->rid )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}


IDE_RC
qmcDiskSort::getNextRange( qmcdDiskSortTemp * aTempTable,
                           void            ** aRow )
{
/***********************************************************************
 *
 * Description :
 *    ���� Range �˻��� �Ѵ�.
 *    �ݵ�� getFirstRange() ���Ŀ� ȣ��Ǿ�� �Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qmcDiskSort::getNextRange"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY("qmcDiskSort::getNextRange"));

    // ���ռ� �˻�
    IDE_DASSERT( *aRow == aTempTable->recordNode->dstTuple->row );

    // Cursor�� �̿��Ͽ� ���ǿ� �´� Row�� �����´�.
    IDE_TEST( smiSortTempTable::fetch( aTempTable->searchCursor,
                                       (UChar**) aRow,
                                       & aTempTable->recordNode->dstTuple->rid )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}


IDE_RC qmcDiskSort::getFirstHit( qmcdDiskSortTemp * aTempTable,
                                 void            ** aRow )
{
/***********************************************************************
 *
 * Description : PROJ-2385
 *    ���� Hit�� Record�� �˻��Ѵ�.
 *
 * Implementation :
 *    ������ ���� ������ ���Ͽ� Hit �˻��� �����Ѵ�.
 *        - Hit Filter�� �����Ѵ�.
 *        - Hit�� ���� Cursor�� ����.
 *        - ������ �����ϴ� Record�� �о�´�.
 *
 ***********************************************************************/

    // ���ռ� �˻�
    IDE_DASSERT( *aRow == aTempTable->recordNode->dstTuple->row );

    IDE_TEST( openCursor(aTempTable, 
                         SMI_TCFLAG_FORWARD | 
                         SMI_TCFLAG_ORDEREDSCAN |
                         SMI_TCFLAG_HIT,
                         smiGetDefaultKeyRange(),
                         smiGetDefaultKeyRange(),
                         smiGetDefaultFilter(),
                         &aTempTable->searchCursor )
              != IDE_SUCCESS );

    // Record�� ��� �´�.
    IDE_TEST( smiSortTempTable::fetch( aTempTable->searchCursor,
                                       (UChar**) aRow,
                                       & aTempTable->recordNode->dstTuple->rid )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


IDE_RC qmcDiskSort::getNextHit( qmcdDiskSortTemp * aTempTable,
                                void            ** aRow )
{
/***********************************************************************
 *
 * Description : PROJ-2385
 *    ���� Hit�� Record�� �˻��Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

    // ���ռ� �˻�
    IDE_DASSERT( *aRow == aTempTable->recordNode->dstTuple->row );

    // Cursor�� �̿��Ͽ� ���ǿ� �´� Row�� �����´�.
    IDE_TEST( smiSortTempTable::fetch( aTempTable->searchCursor,
                                       (UChar**) aRow,
                                       & aTempTable->recordNode->dstTuple->rid )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


IDE_RC
qmcDiskSort::getFirstNonHit( qmcdDiskSortTemp * aTempTable,
                             void            ** aRow )
{
/***********************************************************************
 *
 * Description :
 *    ���� Hit���� ���� Record�� �˻��Ѵ�.
 *
 * Implementation :
 *    ������ ���� ������ ���Ͽ� NonHiit �˻��� �����Ѵ�.
 *        - NonHit Filter�� �����Ѵ�.
 *        - NonHit�� ���� Cursor�� ����.
 *        - ������ �����ϴ� Record�� �о�´�.
 *
 ***********************************************************************/

#define IDE_FN "qmcDiskSort::getFirstNonHit"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY("qmcDiskSort::getFirstNonHit"));

    // ���ռ� �˻�
    IDE_DASSERT( *aRow == aTempTable->recordNode->dstTuple->row );

    IDE_TEST( openCursor(aTempTable, 
                         SMI_TCFLAG_FORWARD | 
                         SMI_TCFLAG_ORDEREDSCAN |
                         SMI_TCFLAG_NOHIT,
                         smiGetDefaultKeyRange(),
                         smiGetDefaultKeyRange(),
                         smiGetDefaultFilter(),
                         &aTempTable->searchCursor )
              != IDE_SUCCESS );

    // Record�� ��� �´�.
    IDE_TEST( smiSortTempTable::fetch( aTempTable->searchCursor,
                                       (UChar**) aRow,
                                       & aTempTable->recordNode->dstTuple->rid )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}


IDE_RC
qmcDiskSort::getNextNonHit( qmcdDiskSortTemp * aTempTable,
                            void            ** aRow )
{
/***********************************************************************
 *
 * Description :
 *    ���� Hit���� ���� Record�� �˻��Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qmcDiskSort::getNextNonHit"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY("qmcDiskSort::getNextNonHit"));

    // ���ռ� �˻�
    IDE_DASSERT( *aRow == aTempTable->recordNode->dstTuple->row );

    // Cursor�� �̿��Ͽ� ���ǿ� �´� Row�� �����´�.
    IDE_TEST( smiSortTempTable::fetch( aTempTable->searchCursor,
                                       (UChar**) aRow,
                                       & aTempTable->recordNode->dstTuple->rid )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qmcDiskSort::getNullRow( qmcdDiskSortTemp * aTempTable,
                         void            ** aRow )
{
/***********************************************************************
 *
 * Description :
 *    NULL ROW�� ȹ���Ѵ�.
 *
 * Implementation :
 *    Null Row�� ȹ���� ���� ���ٸ�, Null Row�� ȹ��
 *    ����� �ش� ������ Null Row�� �����Ѵ�.
 *    SM�� Null Row�� �����ϴ� ������ ����� ȣ��� ���� Disk I/O��
 *    �����ϱ� �����̴�.
 *
 ***********************************************************************/

#define IDE_FN "qmcDiskSort::getNullRow"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY("qmcDiskSort::getNullRow"));

    // ���ռ� �˻�
    IDE_DASSERT( *aRow == aTempTable->recordNode->dstTuple->row );

    // ����� Null Row�� �ش� ������ ������ �ش�.
    IDE_TEST( smiTempTable::getNullRow( aTempTable->tableHandle,
                                        (UChar**)aRow )
              != IDE_SUCCESS );

    SMI_MAKE_VIRTUAL_NULL_GRID( aTempTable->recordNode->dstTuple->rid );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}


IDE_RC
qmcDiskSort::getCurrPosition( qmcdDiskSortTemp * aTempTable,
                              smiCursorPosInfo * aCursorInfo )
{
/***********************************************************************
 *
 * Description :
 *    ���� ��ġ�� Cursor�� �����Ѵ�.
 *
 * Implementation :
 *    Cursor ������ RID������ �����Ѵ�.
 *
 *
 ***********************************************************************/

#define IDE_FN "qmcDiskSort::getCurrPosition"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY("qmcDiskSort::getCurrPosition"));

    // ���ռ� �˻�
    // Search Cursor�� ���� �־�� ��.
    IDE_DASSERT( aTempTable->searchCursor != NULL );
    IDE_DASSERT( aCursorInfo != NULL );

    // Cursor ������ ������.
    IDE_TEST( smiSortTempTable::storeCursor( aTempTable->searchCursor,
                                             (smiTempPosition**)
                                             &aCursorInfo->mCursor.mDTPos.mPos )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}


IDE_RC
qmcDiskSort::setCurrPosition( qmcdDiskSortTemp * aTempTable,
                              smiCursorPosInfo * aCursorInfo )
{
/***********************************************************************
 *
 * Description :
 *    ������ Cursor ��ġ�� ������Ų��.
 *
 * Implementation :
 *    Cursor�� �����ϰ� RID�κ��� Data�� �����Ѵ�.
 *
 ***********************************************************************/

#define IDE_FN "qmcDiskSort::setCurrPosition"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY("qmcDiskSort::setCurrPosition"));

    // ���ռ� �˻�
    // Search Cursor�� ���� �־�� ��.
    IDE_DASSERT( aTempTable->searchCursor != NULL );
    IDE_DASSERT( aCursorInfo != NULL );

    IDE_TEST( smiSortTempTable::restoreCursor( 
                aTempTable->searchCursor,
                (smiTempPosition*) aCursorInfo->mCursor.mDTPos.mPos,
                (UChar**)&aTempTable->recordNode->dstTuple->row,
                &aTempTable->recordNode->dstTuple->rid )
            != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qmcDiskSort::getCursorInfo( qmcdDiskSortTemp * aTempTable,
                            void            ** aTableHandle,
                            void            ** aIndexHandle )
{
/***********************************************************************
 *
 * Description :
 *
 *    View SCAN��� ���� �����ϱ� ���� ���� ����
 *
 * Implementation :
 *
 *    Table Handle�� Index Handle�� �Ѱ��ش�.
 *
 ***********************************************************************/

#define IDE_FN "qmcDiskSort::getCursorInfo"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY("qmcDiskSort::getCursorInfo"));

    *aTableHandle = aTempTable->tableHandle;
    *aIndexHandle = aTempTable->indexHandle;

    return IDE_SUCCESS;

#undef IDE_FN
}


IDE_RC
qmcDiskSort::getDisplayInfo( qmcdDiskSortTemp * aTempTable,
                             ULong            * aPageCount,
                             SLong            * aRecordCount )
{
/***********************************************************************
 *
 * Description :
 *    Temp Table�� �����ϰ� �ִ� Data�� ���� ��� ���� ����
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qmcDiskSort::getDisplayInfo"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY("qmcDiskSort::getDisplayInfo"));

    smiSortTempTable::getDisplayInfo( aTempTable->tableHandle,
                                      aPageCount,
                                      aRecordCount );
    return IDE_SUCCESS;
#undef IDE_FN
}

IDE_RC
qmcDiskSort::setSortNode( qmcdDiskSortTemp * aTempTable,
                          const qmdMtrNode * aSortNode )
{
/***********************************************************************
 *
 * Description 
 *    ���� ����Ű(sortNode)�� ����
 *    Window Sort(WNST)������������ �ݺ��ϱ� ���� ���ȴ�.
 *
 * Implementation :
 *
 ***********************************************************************/
#define IDE_FN "qmcDiskSort::setSortNode"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY("qmcDiskSort::setSortNode"));

    qmdMtrNode * sNode;
    UInt         i;

    // Search Cursor�� ���� �ִٸ� �ݾƾ� ��
    IDE_TEST( smiSortTempTable::closeAllCursor( aTempTable->tableHandle )
              != IDE_SUCCESS );
    aTempTable->searchCursor = NULL;

    /* SortNode ������ */
    aTempTable->sortNode = (qmdMtrNode*) aSortNode;

    /* �ٽ� SortNode�� ������ ����, IndexColumn�� ������ �˾Ƴ� */
    for ( i = 0, sNode = aTempTable->sortNode;
          sNode != NULL;
          sNode = sNode->next, i++ );
    aTempTable->indexColumnCnt = i;

    IDE_TEST( makeIndexColumnInfoInKEY( aTempTable ) != IDE_SUCCESS );

    IDE_TEST( smiSortTempTable::resetKeyColumn( 
            aTempTable->tableHandle,
            aTempTable->indexKeyColumnList )  // key column list
        != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
#undef IDE_FN    
}


IDE_RC
qmcDiskSort::setUpdateColumnList( qmcdDiskSortTemp * aTempTable,
                                  const qmdMtrNode * aUpdateColumns )
/***********************************************************************
 *
 * Description :
 *    update�� ������ column list�� ����
 *    ����: hitFlag�� �Բ� ��� �Ұ�
 *
 * Implementation :
 *
 ***********************************************************************/
{
#define IDE_FN "qmcDiskSort::setUpdateColumnList"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY("qmcDiskSort::setUpdateColumnList"));

    const qmdMtrNode * sNode;
    smiColumnList    * sColumn;

    // ���ռ� �˻�
    IDE_DASSERT( aUpdateColumns != NULL );

    // Search Cursor�� ���� �ִٸ� �ݾƾ� ��
    IDE_TEST( smiSortTempTable::closeAllCursor( aTempTable->tableHandle )
              != IDE_SUCCESS );
    aTempTable->searchCursor = NULL;

    // updateColumnList�� ���� ���� �Ҵ�
    // �̹� setUpdateColumnListForHitFlag���� �̸� �Ҵ�Ǿ� ���� ��
    if( aTempTable->updateColumnList == NULL )
    {
        // WNST�� ��� update column�� ���Ǵµ�,
        // update column�� ������ �̸� ��� Ȯ���� �� ���� ������
        // �ߺ����� �޸� ������ �Ҵ� �ޱ� �ʱ� ���� �̸� ��� Į���� ���� �Ҵ���
        // insertColumn�� ���� ������ ���� ũ��
        IDU_LIMITPOINT("qmcDiskSort::setUpdateColumnList::malloc");
        IDE_TEST( aTempTable->mTemplate->stmt->qmxMem->alloc(
                ID_SIZEOF(smiColumnList) * aTempTable->recordColumnCnt,
                (void**) & aTempTable->updateColumnList )
            != IDE_SUCCESS);
    }
    else
    {
        // �̹� �Ҵ� �� ���
        // do nothing
    }

    // mtrNode �����κ��� update�� ���� smiColumnList�� �����Ѵ�.
    for( sNode = aUpdateColumns,
         sColumn = aTempTable->updateColumnList;
         sNode != NULL;
         sNode = sNode->next,
         sColumn++)
    {
        // smiColumnList�� ���� ������ ���������� �Ҵ�Ǿ� �����Ƿ�
        // ���� �ּ� ��ġ�� next
        sColumn->next = sColumn+1;

        // column ������ ����
        sColumn->column = & sNode->dstColumn->column;
        
        if( sNode->next == NULL )
        {
            // ������ Į���� next�� ������ ����
            sColumn->next = NULL;
        }
    }

    // update�� ���� smiValue ����
    IDE_TEST( makeUpdateSmiValue( aTempTable ) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qmcDiskSort::updateRow( qmcdDiskSortTemp * aTempTable )
/***********************************************************************
 *
 * Description :
 *    ���� �˻� ���� ��ġ�� row�� update
 *
 * Implementation :
 *
 ***********************************************************************/
{
#define IDE_FN "qmcDiskSort::updateRow"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY("qmcDiskSort::updateRow"));

    // ���ռ� �˻�
    // Search Cursor�� ���� �־�� ��.
    IDE_DASSERT( aTempTable->searchCursor != NULL );

    // PROJ-1597 Temp record size ���� ����
    // update�� value���� �����Ѵ�.
    makeUpdateSmiValue( aTempTable );

    // Disk Temp Table�� ���� Record�� Update�Ѵ�.
    IDE_TEST( smiSortTempTable::update( aTempTable->searchCursor, 
                                        aTempTable->updateValues )
         != IDE_SUCCESS )
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;    

#undef IDE_FN
}

IDE_RC
qmcDiskSort::createTempTable( qmcdDiskSortTemp * aTempTable )
{
/***********************************************************************
 *
 * Description :
 *    Disk Temp Table�� �����Ѵ�.
 *
 * Implementation :
 *
 *    1.  Disk Temp Table ������ ���� ���� ����
 *        - Hit Flag�� ���� �ڷ� ���� �ʱ�ȭ
 *        - Column List ������ ����
 *        - NULL ROW�� ����
 *    2.  Disk Temp Table ����
 *    3.  Temp Table Manager �� Table Handle ���
 *
 ***********************************************************************/

#define IDE_FN "qmcDiskSort::createTempTable"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY("qmcDiskSort::createTempTable"));

    UInt                   sFlag ;

    //-------------------------------------------------
    // 1.  Disk Temp Table ������ ���� ���� ����
    //-------------------------------------------------
    if( ( aTempTable->flag & QMCD_SORT_TMP_SEARCH_MASK )
        == QMCD_SORT_TMP_SEARCH_RANGE )
    {
        sFlag = SMI_TTFLAG_RANGESCAN;
    }
    else
    {
        sFlag = SMI_TTFLAG_NONE;
    }

    // Insert Column List ������ ����
    IDE_TEST( setInsertColumnList( aTempTable ) != IDE_SUCCESS );

    //-----------------------------------------
    // Key �������� Index Column ������ ����
    // createTempTable�� �ϱ� ���ؼ��� key column ������ ���� ����Ǿ� �־�� �Ѵ�.
    //-----------------------------------------
    IDE_TEST( makeIndexColumnInfoInKEY( aTempTable ) != IDE_SUCCESS );

    //-------------------------------------------------
    // 2.  Disk Temp Table�� ����
    //-------------------------------------------------
    IDE_TEST( smiSortTempTable::create( 
                aTempTable->mTemplate->stmt->mStatistics,
                QCG_GET_SESSION_TEMPSPACE_ID( aTempTable->mTemplate->stmt ),
                aTempTable->mTempTableSize, /* aWorkAreaSize */
                QC_SMI_STMT( aTempTable->mTemplate->stmt ), // smi statement
                sFlag,
                aTempTable->insertColumn,        // Table�� Column ����
                aTempTable->indexKeyColumnList,  // key column list
                0,                               // WorkGroupRatio
                (const void**) & aTempTable->tableHandle ) // Table �ڵ�
        != IDE_SUCCESS );
    aTempTable->recordNode->dstTuple->tableHandle = aTempTable->tableHandle;

    //-------------------------------------------------
    // 3.  Table Handle�� Temp Table Manager�� ���
    //-------------------------------------------------
    /* BUG-38290 
     * Temp table manager �� addTempTable �Լ��� ���ÿ� ȣ��� �� �����Ƿ�
     * mutex �� ���� ���ü� ��� ���� �ʴ´�.
     */
    IDE_TEST(
        qmcTempTableMgr::addTempTable( aTempTable->mTemplate->stmt->qmxMem,
                                       aTempTable->mTemplate->tempTableMgr,
                                       aTempTable->tableHandle )
        != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qmcDiskSort::setInsertColumnList( qmcdDiskSortTemp * aTempTable )
{
/***********************************************************************
 *
 * Description :
 *
 *    Table�� Column List������ �����Ѵ�.
 *
 * Implementation :
 *    - Column List�� ���� ���� �Ҵ�
 *    - Column List�� Column ������ ����
 *        - Hit Flag�� ���� ���� ����
 *        - �� Column ������ ����
 *
 ***********************************************************************/

#define IDE_FN "qmcDiskSort::setInsertColumnList"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY("qmcDiskSort::setInsertColumnList"));

    UInt i;
    mtcColumn * sColumn;

    // Column ������ŭ Memory �Ҵ�
    IDU_FIT_POINT( "qmcDiskSort::setInsertColumnList::alloc::insertColumn",
                    idERR_ABORT_InsufficientMemory );

    IDE_TEST( aTempTable->mTemplate->stmt->qmxMem->alloc(
            ID_SIZEOF(smiColumnList) * aTempTable->recordColumnCnt,
            (void**) & aTempTable->insertColumn ) != IDE_SUCCESS);

    //-------------------------------------------
    // ���� Column�� Hit Flag�� �����ϰ�,
    // ������ Column�� Tuple Set ������ �̿��Ѵ�.
    //-------------------------------------------

    // To Fix PR-7995
    // Column ID�� �ʱ�ȭ���־�� ��
    // Hit Flag�� Column ���� ����
    // �� Column���� Column ���� ����
    for ( i = 0, sColumn = aTempTable->recordNode->dstTuple->columns;
          i < aTempTable->recordColumnCnt;
          i++ )
    {
        // ���� Column List�� ���� ���� ����
        if( i < aTempTable->recordColumnCnt - 1 )
        {
            aTempTable->insertColumn[i].next = & aTempTable->insertColumn[i+1];
        }
        else
        {
            /* ������ Į���̸�, Null�� ���� */
            aTempTable->insertColumn[i].next = NULL;
        }
        // To Fix PR-7995
        // Column ID�� �ʱ�ȭ���־�� ��
        // ���� Column List�� Column ���� ����
        sColumn[i].column.id = i;
        aTempTable->insertColumn[i].column = & sColumn[i].column;
    }

    // To Fix PR-7995
    // Fix UMR
    IDE_ERROR( aTempTable->insertColumn[i-1].next == NULL );

    IDE_TEST( aTempTable->mTemplate->stmt->qmxMem->alloc(
            ID_SIZEOF(smiValue) * aTempTable->recordColumnCnt,
            (void**) & aTempTable->insertValue ) != IDE_SUCCESS);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qmcDiskSort::sort( qmcdDiskSortTemp * aTempTable )
{
/***********************************************************************
 *
 * Description :
 *    Temp Table���� Row���� �����Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qmcDiskSort::sort"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY("qmcDiskSort::sort"));

    return smiSortTempTable::sort( aTempTable->tableHandle );
#undef IDE_FN
}

IDE_RC
qmcDiskSort::makeIndexColumnInfoInKEY( qmcdDiskSortTemp * aTempTable )
{
/***********************************************************************
 *
 * Description :
 *    Index���� �� Record������ Index Key�� ���Ե� �÷��� ������
 *    �����Ѵ�.  Disk Index�� ��� Key ���� ������ �����ϱ� ������,
 *    Index ���� ��, Record�������� Index Key Column�� ������
 *    Key�������� Index Key Column������ �����Ͽ� ó���Ͽ��� �Ѵ�.
 *
 *    ��, Index Column�� ��� Record�������� offset�� Key��������
 *    offset�� �ٸ��� �ȴ�.
 *
 * Implementation :
 *
 *    - Key Column�� ���� mtcColumn ���� ����
 *    - Key Column�� ���� smiColumnList ����
 *
 ***********************************************************************/

#define IDE_FN "qmcDiskSort::makeIndexColumnInfoInKEY"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY("qmcDiskSort::makeIndexColumnInfoInKEY"));

    UInt            i;
    UInt            sOffset;
    qmdMtrNode    * sNode;

    //------------------------------------------------------------
    // Key �������� Index ��� Column�� ���� ����
    // �ش� Column�� Key������ ����Ǿ��� ��,
    // Record�������� offset ������ �޸� Key �������� offset������
    // ����Ǿ�� �Ѵ�.
    //------------------------------------------------------------

    // WNST�� ��� update column�� ���Ǵµ�,
    // ���� �����ĵǸ鼭 ����� KeyColumn�� ������ �̸� ��� Ȯ���� �� ����
    // ������ �ߺ����� �޸� ������ �Ҵ� �ޱ� �ʱ� ���� �̸� ��� Į����
    // ���� �Ҵ���
    if( ( aTempTable->indexKeyColumn == NULL ) &&
        ( aTempTable->indexColumnCnt > 0 ) )
    {
        IDU_FIT_POINT( "qmcDiskSort::makeIndexColumnInfoInKEY::alloc::indexKeyColumn",
                        idERR_ABORT_InsufficientMemory );

        IDE_TEST( aTempTable->mTemplate->stmt->qmxMem->alloc(
                ID_SIZEOF(mtcColumn) * aTempTable->recordColumnCnt,
                (void**) & aTempTable->indexKeyColumn ) != IDE_SUCCESS);

        // Key Column List�� ���� ���� �Ҵ�
        IDU_FIT_POINT( "qmcDiskSort::makeIndexColumnInfoInKEY::alloc::indexKeyColumnList",
                        idERR_ABORT_InsufficientMemory );

        IDE_TEST( aTempTable->mTemplate->stmt->qmxMem->alloc(
                ID_SIZEOF(smiColumnList) * aTempTable->recordColumnCnt,
                (void**) & aTempTable->indexKeyColumnList ) != IDE_SUCCESS);
    }
    else
    {
        // �̹� �Ҵ� �� ���
        // do nothing
    }

    //---------------------------------------
    // Key Column�� mtcColumn ���� ����
    //---------------------------------------

    sOffset = 0;
    for ( i = 0, sNode = aTempTable->sortNode;
          i < aTempTable->indexColumnCnt;
          i++, sNode = sNode->next )
    {
        // Record���� Column ������ ����
        idlOS::memcpy( & aTempTable->indexKeyColumn[i],
                       sNode->dstColumn,
                       ID_SIZEOF(mtcColumn) );

        // fix BUG-8763
        // Index Column�� ��� ���� Flag�� ǥ���Ͽ��� ��.
        aTempTable->indexKeyColumn[i].column.flag &= ~SMI_COLUMN_USAGE_MASK;
        aTempTable->indexKeyColumn[i].column.flag |= SMI_COLUMN_USAGE_INDEX;

        // Offset ������
        if ( (aTempTable->indexKeyColumn[i].column.flag
              & SMI_COLUMN_TYPE_MASK ) == SMI_COLUMN_TYPE_FIXED )
        {
            // Fixed Column �� ���
            // Data Type�� �´� align ����
            sOffset =
                idlOS::align( sOffset,
                              aTempTable->indexKeyColumn[i].module->align );
            aTempTable->indexKeyColumn[i].column.offset = sOffset;
            aTempTable->indexKeyColumn[i].column.value = NULL;
            sOffset += aTempTable->indexKeyColumn[i].column.size;
        }
        else
        {
            //------------------------------------------
            // Variable Column�� ���
            // Variable Column Header ũ�Ⱑ �����
            // �̿� �´� align�� �����ؾ� ��.
            //------------------------------------------
            sOffset = idlOS::align( sOffset, 8 );
            aTempTable->indexKeyColumn[i].column.offset = sOffset;
            aTempTable->indexKeyColumn[i].column.value = NULL;

// PROJ_1705_PEH_TODO            
            // Fixed ���� ������ Header�� ����ȴ�.
            sOffset += smiGetVariableColumnSize( SMI_TABLE_DISK );
        }

        if ( (sNode->myNode->flag & QMC_MTR_SORT_ORDER_MASK )
             == QMC_MTR_SORT_ASCENDING )
        {
            // Ascending Order ����
            aTempTable->indexKeyColumn[i].column.flag &=
                ~SMI_COLUMN_ORDER_MASK;
            aTempTable->indexKeyColumn[i].column.flag |=
                SMI_COLUMN_ORDER_ASCENDING;
        }
        else
        {
            // Descending Order ����
            aTempTable->indexKeyColumn[i].column.flag &=
                ~SMI_COLUMN_ORDER_MASK;
            aTempTable->indexKeyColumn[i].column.flag |=
                SMI_COLUMN_ORDER_DESCENDING;
        }

        /* PROJ-2435 order by nulls first/last */
        if ( ( sNode->myNode->flag & QMC_MTR_SORT_NULLS_ORDER_MASK ) 
             != QMC_MTR_SORT_NULLS_ORDER_NONE )
        {
            if ( ( sNode->myNode->flag & QMC_MTR_SORT_NULLS_ORDER_MASK ) 
                 == QMC_MTR_SORT_NULLS_ORDER_FIRST )
            {
                aTempTable->indexKeyColumn[i].column.flag &= 
                    ~SMI_COLUMN_NULLS_ORDER_MASK;
                aTempTable->indexKeyColumn[i].column.flag |= 
                    SMI_COLUMN_NULLS_ORDER_FIRST;
            }
            else
            {
                aTempTable->indexKeyColumn[i].column.flag &= 
                    ~SMI_COLUMN_NULLS_ORDER_MASK;
                aTempTable->indexKeyColumn[i].column.flag |= 
                    SMI_COLUMN_NULLS_ORDER_LAST;
            }
        }
        else
        {
            aTempTable->indexKeyColumn[i].column.flag &= 
                ~SMI_COLUMN_NULLS_ORDER_MASK;
            aTempTable->indexKeyColumn[i].column.flag |= 
                SMI_COLUMN_NULLS_ORDER_NONE;
        }
    }

    //---------------------------------------
    // Key Column�� ���� smiColumnList ����
    // �������� ������ indexKeyColumn������ �̿��Ͽ� �����Ѵ�.
    //---------------------------------------

    // Key Column List�� ���� ����
    for ( i = 0; i < aTempTable->indexColumnCnt; i++ )
    {
        // �������� ������ Column ������ �̿�
        aTempTable->indexKeyColumnList[i].column =
            & aTempTable->indexKeyColumn[i].column;

        // Array Bound Read�� �����ϱ� ���� �˻�.
        if ( (i + 1) < aTempTable->indexColumnCnt )
        {
            aTempTable->indexKeyColumnList[i].next =
                & aTempTable->indexKeyColumnList[i+1];
        }
        else
        {
            aTempTable->indexKeyColumnList[i].next = NULL;
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}


IDE_RC qmcDiskSort::openCursor( qmcdDiskSortTemp  * aTempTable,
                                UInt                aFlag,
                                smiRange          * aKeyRange,
                                smiRange          * aKeyFilter,
                                smiCallBack       * aRowFilter,
                                smiSortTempCursor** aTargetCursor )
{
/***********************************************************************
 *
 * Description :
 *    GroupFini/Group/Search/HitFlag�� ���� Cursor�� ����.
 *
 * Implementation :
 *    1. Cursor�� �� ���� Open���� ���� ���,
 *        - Cursor�� Open
 *    2. Cursor�� ���� �ִ� ���,
 *        - Cursor�� Restart
 *
 ***********************************************************************/
#define IDE_FN "qmcDiskSort::openCursor"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY("qmcDiskSort::openCursor"));

    if( *aTargetCursor == NULL )
    {
        //-----------------------------------------
        // 1. Cursor�� ���� ���� ���� ���
        //-----------------------------------------
        IDE_TEST( smiSortTempTable::openCursor( 
                aTempTable->tableHandle,
                aFlag,
                aTempTable->updateColumnList,  // Update Column����
                aKeyRange,
                aKeyFilter,
                aRowFilter,
                aTargetCursor )
            != IDE_SUCCESS );
    }
    else
    {
        //-----------------------------------------
        // 2. Cursor�� ���� �ִ� ���
        //-----------------------------------------
        IDE_TEST( smiSortTempTable::restartCursor( 
                *aTargetCursor,
                aFlag,
                aKeyRange,
                aKeyFilter,
                aRowFilter )
            != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}


IDE_RC
qmcDiskSort::makeInsertSmiValue( qmcdDiskSortTemp * aTempTable )
{
/***********************************************************************
 *
 * Description :
 *    Insert�� ���� Smi Value ���� ������ �����Ѵ�.
 *
 * Implementation :
 *    Disk Temp Table�� Record�� ���Ͽ� ������ ��������
 *    �� Column�� Value ������ �����Ѵ�.
 *    ���� �� ���� �����ϸ�, �̴� Temp Table�� ����� ������
 *    �̹� �Ҵ�� Record ������ ���� ������ �ʱ� �����̴�.
 *
 ***********************************************************************/

#define IDE_FN "qmcDiskSort::makeInsertSmiValue"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY("qmcDiskSort::makeInsertSmiValue"));

    UInt         i;
    mtcColumn  * sColumn;

    for ( i = 0; i < aTempTable->recordColumnCnt; i++ )
    {
        // PROJ-1597 Temp record size ���� ����
        // smiValue�� length, value�� storing format���� �ٲ۴�.

        sColumn = &aTempTable->recordNode->dstTuple->columns[i];

        aTempTable->insertValue[i].value = 
            (SChar*)aTempTable->recordNode->dstTuple->row
            + sColumn->column.offset;

        aTempTable->insertValue[i].length = 
            sColumn->module->actualSize( sColumn,
                                         (SChar*)aTempTable->recordNode->dstTuple->row
                                         + sColumn->column.offset );
    }

    return IDE_SUCCESS;

#undef IDE_FN
}

IDE_RC
qmcDiskSort::makeRange( qmcdDiskSortTemp * aTempTable )
{
/***********************************************************************
 *
 * Description :
 *    Range �˻��� ���� Key/Row Range�� �����Ѵ�.
 *
 * Implementation :
 *    Range ������ ���� ���,
 *        - Range ������ ũ�⸦ ����ϰ�, �޸𸮸� �Ҵ�޴´�.
 *    Range ����
 *        - �־��� Key Range Predicate�� �̿��Ͽ� Key Range�� �����Ѵ�.
 *
 * Modified :
 *    Key Range�� ������ �� Row Range�� �Բ� �����Ѵ�.
 *    PROJ-1431���� Temp B-Tree�� sparse index�� ��������ʿ� ����
 *    data page���� range�� �ʿ���
 ***********************************************************************/

#define IDE_FN "qmcDiskSort::makeRange"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY("qmcDiskSort::makeRange"));

    UInt      sKeyColsFlag;
    qtcNode * sKeyFilter;
    UInt      sCompareType;

    // ���ռ� �˻�
    // Range �˻��� �� Column�� ���ؼ��� �����ϴ�.
    IDE_DASSERT( aTempTable->indexColumnCnt == 1 );

    //-----------------------------------
    // Key Range ������ �Ҵ����.
    //-----------------------------------
    if ( aTempTable->keyRangeArea == NULL )
    {
        // Range�� ���� ������ �Ҵ�޴´�.
        // Predicate�� Value�� �ٲ� Predicate�� ������ �ٲ��� �ʴ´�.
        // ����, �� ���� �Ҵ��ϰ� ��� ����� �� �ֵ��� �Ѵ�.

        //---------------------------
        // Range�� ũ�� ���
        //---------------------------

        IDE_TEST(
            qmoKeyRange::estimateKeyRange( aTempTable->mTemplate,
                                           aTempTable->rangePredicate,
                                           &aTempTable->keyRangeAreaSize )
            != IDE_SUCCESS );

        //---------------------------
        // Key Range�� ���� ������ �Ҵ��Ѵ�.
        //---------------------------
        IDU_FIT_POINT( "qmcDiskSort::makeRange::alloc::keyRangeArea",
                        idERR_ABORT_InsufficientMemory );

        IDE_TEST( aTempTable->mTemplate->stmt->qmxMem->cralloc(
                aTempTable->keyRangeAreaSize,
                (void**) & aTempTable->keyRangeArea ) != IDE_SUCCESS);
    }
    else
    {
        // Nothing To Do
    }

    if ( (aTempTable->sortNode->myNode->flag & QMC_MTR_SORT_ORDER_MASK)
         == QMC_MTR_SORT_ASCENDING )
    {
        sKeyColsFlag = SMI_COLUMN_ORDER_ASCENDING;
    }
    else
    {
        sKeyColsFlag = SMI_COLUMN_ORDER_DESCENDING;
    }

    // ���ռ� �˻�
    // �ݵ�� Key Range������ �����ؾ� ��

    //-----------------------------------
    // Key Range�� ������.
    //-----------------------------------

    // disk temp table�� key range�� MT Ÿ�԰��� compare
    sCompareType = MTD_COMPARE_MTDVAL_MTDVAL;

    IDE_TEST( qmoKeyRange::makeKeyRange(
                  aTempTable->mTemplate,
                  aTempTable->rangePredicate,
                  1,
                  aTempTable->sortNode->func.compareColumn,
                  & sKeyColsFlag,
                  sCompareType,
                  aTempTable->keyRangeArea,
                  aTempTable->keyRangeAreaSize,
                  & (aTempTable->keyRange),
                  & sKeyFilter )
              != IDE_SUCCESS );

    // ���ռ� �˻�
    // �ݵ�� Key Range������ �����ؾ� ��
    IDE_DASSERT( sKeyFilter == NULL );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qmcDiskSort::makeUpdateSmiValue( qmcdDiskSortTemp * aTempTable )
{
/***********************************************************************
 *
 * Description :
 *    Update Column�� ���� Smi Value ���� ������ �����Ѵ�.
 *
 * Implementation :
 *    PROJ-1597 Temp record size ���� ���Ÿ� ����
 *    �Ź� update�� ȣ���ϱ� ���� �� �Լ��� �ҷ���� �Ѵ�.
 *    update value���� storing �����̱� ������ �Ź� ��������� �Ѵ�.:
 *
 ***********************************************************************/

#define IDE_FN "qmcDiskSort::makeUpdateSmiValue"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY("qmcDiskSort::makeUpdateSmiValue"));
    
    smiColumnList * sColumn;
    smiValue      * sValue;
    mtcColumn     * sMtcColumn;

    if( aTempTable->updateValues == NULL )
    {
        // Update Value ������ ���� ������ �Ҵ��Ѵ�.
        IDU_FIT_POINT( "qmcDiskSort::makeUpdateSmiValue::alloc::updateValues",
                        idERR_ABORT_InsufficientMemory );

        IDE_TEST( aTempTable->mTemplate->stmt->qmxMem->alloc(
                ID_SIZEOF(smiValue) * aTempTable->recordColumnCnt,
                (void**) & aTempTable->updateValues ) != IDE_SUCCESS);
    }
    else
    {
        // nothing to do
    }

    for( sColumn = aTempTable->updateColumnList,
             sValue = aTempTable->updateValues;
         sColumn != NULL;
         sColumn = sColumn->next, sValue++ )
    {
        // smiColumn�� mtcColumn���ο� ��ü�� �����ϱ� ������
        // ���� �ּҸ� �����ٰ� �� �� �ִ�.
        sMtcColumn = (mtcColumn*)sColumn->column;

        sValue->value = 
            (SChar*)aTempTable->recordNode->dstTuple->row
            + sColumn->column->offset;

        sValue->length = sMtcColumn->module->actualSize( 
                        sMtcColumn,
                        (SChar*)aTempTable->recordNode->dstTuple->row
                        + sColumn->column->offset );
    }

    // updateColumnList�� ������ �Ҵ� ���� updateValues���� ���� ���
    // �߻����� �ʴ� ���
    IDE_DASSERT( (UInt)(sValue-aTempTable->updateValues) <= aTempTable->recordColumnCnt );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}


