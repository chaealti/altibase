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
 * $Id: qmcDiskHashTempTable.cpp 85333 2019-04-26 02:34:41Z et16 $
 *
 * Description :
 *     Disk Hash Temp Table
 *
 * ��� ���� :
 *
 * ��� :
 *
 **********************************************************************/

#include <idl.h>
#include <ide.h>
#include <smErrorCode.h>
#include <qcg.h>
#include <qdParseTree.h>
#include <qdx.h>
#include <qcm.h>
#include <qcmTableSpace.h>
#include <qmcDiskHashTempTable.h>
#include <qdbCommon.h>

extern mtdModule mtdInteger;

IDE_RC
qmcDiskHash::init( qmcdDiskHashTemp * aTempTable,
                   qcTemplate       * aTemplate,
                   qmdMtrNode       * aRecordNode,
                   qmdMtrNode       * aHashNode,
                   qmdMtrNode       * aAggrNode,
                   UInt               aBucketCnt,
                   UInt               aMtrRowSize,
                   idBool             aDistinct )
{
/***********************************************************************
 *
 * Description :
 *    Disk Temp Table�� �ʱ�ȭ�Ѵ�.
 *
 * Implementation :
 *    - Disk Temp Table�� �⺻ �ڷ� �ʱ�ȭ
 *    - Record ���� ������ �ʱ�ȭ
 *    - Hashing ���� ������ �ʱ�ȭ
 *    - �˻��� ���� �ڷ� ������ �ʱ�ȭ
 *    - Aggregation�� ���� �ڷ� ������ �ʱ�ȭ
 *    - Temp Table�� ����
 *    - Index�� ����
 *    - Insert Cursor�� Open
 *
 ***********************************************************************/

    UInt         i;
    qmdMtrNode * sNode;
    qmdMtrNode * sAggrNode;
    
    // ���ռ� �˻�
    IDE_DASSERT( aTempTable != NULL );
    IDE_DASSERT( aTemplate != NULL );
    IDE_DASSERT( aRecordNode != NULL );
    IDE_DASSERT( aHashNode != NULL );

    //----------------------------------------------------------------
    // Disk Temp Table ����� �ʱ�ȭ
    //----------------------------------------------------------------

    aTempTable->flag = QMCD_DISK_HASH_TEMP_INITIALIZE;

    if ( aDistinct == ID_TRUE )
    {
        // Distinct Insertion�� ���
        aTempTable->flag &= ~QMCD_DISK_HASH_INSERT_DISTINCT_MASK;
        aTempTable->flag |= QMCD_DISK_HASH_INSERT_DISTINCT_TRUE;
    }
    else
    {
        // Non-Distinct Insertion�� ���
        aTempTable->flag &= ~QMCD_DISK_HASH_INSERT_DISTINCT_MASK;
        aTempTable->flag |= QMCD_DISK_HASH_INSERT_DISTINCT_FALSE;
    }

    aTempTable->mTemplate      = aTemplate;
    aTempTable->tableHandle    = NULL;

    tempSizeEstimate( aTempTable, aBucketCnt, aMtrRowSize );

    //-----------------------------------------
    // Record ���� ������ �ʱ�ȭ
    //-----------------------------------------
    aTempTable->recordNode = aRecordNode;

    // [Record Column ������ ����]
    // Record Node�� ������ �Ǵ����� �ʰ�,
    // Tuple Set�� �����κ��� Column ������ �Ǵ��Ѵ�.
    // �̴� �ϳ��� Record Node�� ���� ���� Column���� ������ �� �ֱ� �����̴�.

    IDE_DASSERT( aRecordNode->dstTuple->columnCount > 0 );
    aTempTable->recordColumnCnt = aRecordNode->dstTuple->columnCount;

    aTempTable->insertColumn = NULL;
    aTempTable->insertValue = NULL;

    //---------------------------------------
    // Hashing �� ���� �ڷ� ������ �ʱ�ȭ
    //---------------------------------------
    aTempTable->hashNode = aHashNode;

    // [Hash Column ������ ����]
    // Record Column�� �޸� Hash Node �����κ��� Column ������ �Ǵ��Ѵ�.
    // �ϳ��� Node�� ���� ���� Column���� �����ȴ� �ϴ���
    // Hashing ����� �Ǵ� Column�� �ϳ��̱� �����̴�.
    for ( i = 0, sNode = aTempTable->hashNode;
          sNode != NULL;
          sNode = sNode->next, i++ ) ;
    aTempTable->hashColumnCnt = i;

    aTempTable->hashKeyColumnList = NULL;
    aTempTable->hashKeyColumn = NULL;

    //---------------------------------------
    // �˻��� ���� �ڷ� ������ �ʱ�ȭ
    //---------------------------------------
    aTempTable->searchCursor    = NULL;
    aTempTable->groupCursor     = NULL;
    aTempTable->groupFiniCursor = NULL;
    aTempTable->hitFlagCursor   = NULL;

    // To Fix PR-8289
    aTempTable->hashFilter = NULL;

    //---------------------------------------
    // Aggregation�� ���� �ڷ� ������ �ʱ�ȭ
    //---------------------------------------

    // [Aggr Column ������ ����]
    // Aggr Column�� �ϳ��� Node�� ���� ���� Column���� �����Ǹ�
    // ��� Column�� UPDATE ����� �ȴ�.  ����, ��� Column
    // ������ ����Ͽ��� �Ѵ�.
    aTempTable->aggrNode = aAggrNode;
    for ( i = 0, sAggrNode = aTempTable->aggrNode;
          sAggrNode != NULL;
          sAggrNode = sAggrNode->next )
    {
        // To Fix PR-7994, PR-8415
        // Source Node�� �̿��Ͽ� Column Count�� ȹ��

        // To Fix PR-12093
        // Destine Node�� ����Ͽ� mtcColumn�� Count�� ���ϴ� ���� ��Ģ�� ����
        // ��������� ���� mtcColumn ������ �����ϴ� ���� ���ո���.
        //     - Memory ���� ����
        //     - offset ���� ���� (PR-12093)
        i += ( sAggrNode->dstNode->node.module->lflag
               & MTC_NODE_COLUMN_COUNT_MASK);
    }
    aTempTable->aggrColumnCnt = i;
    aTempTable->aggrColumnList = NULL;
    aTempTable->aggrValue = NULL;

    //----------------------------------------------------------------
    // Disk Temp Table �� ����
    //----------------------------------------------------------------
    IDE_TEST( createTempTable( aTempTable ) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/******************************************************************************
 *
 * Description : BUG-37778 disk hash temp table size estimate
 *                WAMap size + temp table size
 *
 *****************************************************************************/
void  qmcDiskHash::tempSizeEstimate( qmcdDiskHashTemp * aTempTable,
                                     UInt               aBucketCnt,
                                     UInt               aMtrRowSize )
{
    ULong        sTempSize;

    // BUG-43443 temp table�� ���ؼ� work area size�� estimate�ϴ� ����� off
    if ( QCU_DISK_TEMP_SIZE_ESTIMATE == 1 )
    {
        // sdtUniqueHashModule::destroy( void * aHeader ) �� ������
        sTempSize = (ULong)(SMI_TR_HEADER_SIZE_FULL + aMtrRowSize) * aBucketCnt * 4;

        sTempSize = (ULong)(sTempSize
                            * 100.0 / (100.0 - ( smuProperty::getTempHashGroupRatio() +
                                                 smuProperty::getTempSubHashGroupRatio() )
                                       * 1.2));

        aTempTable->mTempTableSize = IDL_MIN( sTempSize, smuProperty::getHashAreaSize());
    }
    else
    {
        aTempTable->mTempTableSize = 0;
    }

}

IDE_RC
qmcDiskHash::clear( qmcdDiskHashTemp * aTempTable )
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

    //-----------------------------
    // Temp Table�� Truncate�Ѵ�.
    //-----------------------------
    IDE_TEST( smiHashTempTable::clear( aTempTable->tableHandle )
              != IDE_SUCCESS );

    aTempTable->searchCursor    = NULL;
    aTempTable->groupCursor     = NULL;
    aTempTable->groupFiniCursor = NULL;
    aTempTable->hitFlagCursor   = NULL;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    // BUG-39644
    aTempTable->tableHandle = NULL;

    return IDE_FAILURE;
}

IDE_RC
qmcDiskHash::clearHitFlag( qmcdDiskHashTemp * aTempTable )
{
/***********************************************************************
 *
 * Description :
 *    Temp Table���� �����ϴ� ��� Record�� Hit Flag�� �ʱ�ȭ�Ѵ�.
 *
 * Implementation :
 *    PROJ-2201 Innovation in sorting and hashing(temp)
 *    HitFlag�� �� Row�� ������ HitSequence������, Header�� HitSequence����
 *    ������ Hit�� ������ �Ǵ��Ѵ�.
 *    ���� Header�� HitSequence�� 1 �÷��ָ� I/O���� �ʱ�ȭ �����ϴ�.
 *
 ***********************************************************************/

#define IDE_FN "qmcDiskHash::clearHitFlag"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY("qmcDiskHash::clearHitFlag"));

    smiHashTempTable::clearHitFlag( aTempTable->tableHandle );

    return IDE_SUCCESS;
#undef IDE_FN
}

IDE_RC
qmcDiskHash::insert( qmcdDiskHashTemp * aTempTable,
                     UInt               aHashKey,
                     idBool           * aResult )
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
 *    - Distinct Insertion�� ���, ������ ������ �� �ִ�.
 *        - ������ Error Code�� �˻��Ͽ� ������ �������� �����Ѵ�.
 ***********************************************************************/

    // PROJ-1597 Temp record size ���� ����
    // �� row���� smiValue�� �籸���Ѵ�.
    IDE_TEST( makeInsertSmiValue(aTempTable) != IDE_SUCCESS );

    // Temp Table�� Record�� �����Ѵ�.
    IDE_TEST( smiHashTempTable::insert( aTempTable->tableHandle,
                                        aTempTable->insertValue,
                                        aHashKey,
                                        &aTempTable->recordNode->dstTuple->rid,
                                        aResult )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    // BUG-39644
    aTempTable->tableHandle = NULL;

    return IDE_FAILURE;
}


IDE_RC
qmcDiskHash::updateAggr( qmcdDiskHashTemp * aTempTable )
{
/***********************************************************************
 *
 * Description :
 *    Aggregation Column����  Update�Ѵ�.
 *
 * Implementation :
 *    Group Cursor�� �̿��Ͽ� Update�ϸ�,
 *    Group Cursor�� �ݵ�� Open�Ǿ� �־�� �Ѵ�.
 *    Update Value�� Group Cursor Open�� �����Ǿ���,
 *    Value�� ���� pointer ���� row pointer�� ������ �ʱ� ������,
 *    �����ų �ʿ䰡 ����.
 *
 ***********************************************************************/

    // ���ռ� �˻�
    IDE_DASSERT( aTempTable->groupCursor != NULL );

    // Aggregation Column�� ���� smiValue ����
    IDE_TEST( makeAggrSmiValue( aTempTable ) != IDE_SUCCESS );

    // Aggregation Column�� Update�Ѵ�.
    IDE_TEST( smiHashTempTable::update( aTempTable->groupCursor, 
                                        aTempTable->aggrValue )
         != IDE_SUCCESS )

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    // BUG-39644
    aTempTable->tableHandle = NULL;

    return IDE_FAILURE;
}

IDE_RC
qmcDiskHash::updateFiniAggr( qmcdDiskHashTemp * aTempTable )
{
/***********************************************************************
 *
 * Description :
 *    Aggregation Column���� ���� Update�Ѵ�.
 *
 * Implementation :
 *    Search Cursor�� �̿��Ͽ� Update�ϸ�,
 *    Search Cursor�� �ݵ�� Open�Ǿ� �־�� �Ѵ�.
 *
 ***********************************************************************/

    // ���ռ� �˻�
    IDE_DASSERT( aTempTable->groupFiniCursor != NULL );

    // Aggregation Column�� ���� smiValue ����
    IDE_TEST( makeAggrSmiValue( aTempTable ) != IDE_SUCCESS );

    // Aggregation Column�� ���� Update�Ѵ�.
    IDE_TEST( smiHashTempTable::update( aTempTable->groupFiniCursor, 
                                        aTempTable->aggrValue )
         != IDE_SUCCESS )

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    // BUG-39644
    aTempTable->tableHandle = NULL;

    return IDE_FAILURE;
}


IDE_RC
qmcDiskHash::setHitFlag( qmcdDiskHashTemp * aTempTable )
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

#define IDE_FN "qmcDiskHash::setHitFlag"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY("qmcDiskHash::setHitFlag"));

    smiHashTempTable::setHitFlag( aTempTable->searchCursor );

    return IDE_SUCCESS;
#undef IDE_FN
}

idBool qmcDiskHash::isHitFlagged( qmcdDiskHashTemp * aTempTable )
{
/***********************************************************************
 *
 * Description :
 *    ���� Cursor ��ġ�� Record�� Hit Flag�� �ִ��� �Ǵ�.
 *
 * Implementation :
 *    �˻� �Ŀ��� ȣ��ȴ�.
 *    �̹� �˻��� Record�� ���� Hit Flag ���θ� �˻��ϸ� �ȴ�.
 *
 ***********************************************************************/

    return smiHashTempTable::isHitFlagged( aTempTable->searchCursor );
}

IDE_RC
qmcDiskHash::getFirstGroup( qmcdDiskHashTemp * aTempTable,
                            void            ** aRow )
{
/***********************************************************************
 *
 * Description :
 *    ù��° Group ���� �˻��� �Ѵ�.
 *    Group Aggregation�� ���� ó���� ���� Cursor�� ����.
 *    Group�� ���� Sequetial �˻��� �ϰ� �Ǹ�,
 *    Aggregation Column�� ���� ����� �����ϱ� ���� ����Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

    if( aTempTable->groupFiniCursor == NULL )
    {
        //-----------------------------------------
        // 1. Cursor�� ���� ���� ���� ���
        //-----------------------------------------
        IDE_TEST( smiHashTempTable::allocHashCursor( aTempTable->tableHandle,
                                                     aTempTable->aggrColumnList,   // Update Column����
                                                     SMI_HASH_CURSOR_FULL_SCAN,
                                                     &aTempTable->groupFiniCursor )
                  != IDE_SUCCESS );
    }

    // Group�� ���� Aggregation�� ���� Cursor�� ����.
    IDE_TEST( smiHashTempTable::openFullScanCursor( aTempTable->groupFiniCursor, 
                                                    SMI_TCFLAG_FORWARD | 
                                                    SMI_TCFLAG_FULLSCAN |
                                                    SMI_TCFLAG_IGNOREHIT,
                                                    (UChar**) aRow,
                                                    & aTempTable->recordNode->dstTuple->rid )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmcDiskHash::getNextGroup( qmcdDiskHashTemp * aTempTable,
                           void            ** aRow )
{
/***********************************************************************
 *
 * Description :
 *    ���� Group ���� �˻��� �Ѵ�.
 *    �ݵ�� getFirstGroup() ���Ŀ� ȣ��Ǿ�� �Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

    // Record�� ��� �´�.
    IDE_TEST( smiHashTempTable::fetchFullNext( aTempTable->groupFiniCursor,
                                               (UChar**) aRow,
                                               & aTempTable->recordNode->dstTuple->rid )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmcDiskHash::getFirstSequence( qmcdDiskHashTemp * aTempTable,
                               void            ** aRow )
{
/***********************************************************************
 *
 * Description :
 *    ù ���� �˻��� �Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

    if( aTempTable->searchCursor == NULL )
    {
        //-----------------------------------------
        // 1. Cursor�� ���� ���� ���� ���
        //-----------------------------------------
        IDE_TEST( smiHashTempTable::allocHashCursor( aTempTable->tableHandle,
                                                     aTempTable->aggrColumnList,   // Update Column����
                                                     SMI_HASH_CURSOR_FULL_SCAN,
                                                     &aTempTable->searchCursor )
                  != IDE_SUCCESS );
    }

    // ���� �˻� Cursor�� ����.
    IDE_TEST( smiHashTempTable::openFullScanCursor( aTempTable->searchCursor,
                                                    SMI_TCFLAG_FORWARD |
                                                    SMI_TCFLAG_FULLSCAN |
                                                    SMI_TCFLAG_IGNOREHIT,
                                                    (UChar**) aRow,
                                                    & aTempTable->recordNode->dstTuple->rid )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmcDiskHash::getNextSequence( qmcdDiskHashTemp * aTempTable,
                              void            ** aRow )
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

    // Record�� ��� �´�.
    IDE_TEST( smiHashTempTable::fetchFullNext( aTempTable->searchCursor,
                                               (UChar**) aRow,
                                               & aTempTable->recordNode->dstTuple->rid )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmcDiskHash::getFirstRange( qmcdDiskHashTemp * aTempTable,
                            UInt               aHash,
                            qtcNode          * aHashFilter,
                            void            ** aRow )
{
/***********************************************************************
 *
 * Description :
 *    ù��° Range �˻��� �Ѵ�.
 *
 * Implementation :
 *    ������ ���� ������ ���Ͽ� Range�˻��� �����Ѵ�.
 *
 *    - �־��� Hash Value�� �̿��Ͽ� Key Range�� �����Ѵ�.
 *    - Range Cursor�� Open �Ѵ�.
 *    - Cursor�� �̿��Ͽ� ���ǿ� �´� Row�� �����´�.
 *
 ***********************************************************************/

    // ���ռ� �˻�
    IDE_DASSERT( aHashFilter != NULL );

    if( aTempTable->searchCursor == NULL )
    {
        //-----------------------------------------
        // 1. Cursor�� ���� ���� ���� ���
        //-----------------------------------------
        IDE_TEST( smiHashTempTable::allocHashCursor( aTempTable->tableHandle,
                                                     aTempTable->aggrColumnList,   // Update Column����
                                                     SMI_HASH_CURSOR_HASH_SCAN,
                                                     &aTempTable->searchCursor )
            != IDE_SUCCESS );
    }

    if( smiHashTempTable::checkHashSlot( aTempTable->searchCursor,
                                         aHash ) == ID_TRUE )
    {
        // Filter ����
        aTempTable->hashFilter = aHashFilter;
        IDE_TEST( makeHashFilterCallBack( aTempTable ) != IDE_SUCCESS );

        // Range Cursor�� Open�Ѵ�.
        IDE_TEST( smiHashTempTable::openHashCursor( aTempTable->searchCursor,
                                                    SMI_TCFLAG_FORWARD |
                                                    SMI_TCFLAG_HASHSCAN |
                                                    SMI_TCFLAG_IGNOREHIT,
                                                    &aTempTable->filterCallBack,  // Filter
                                                    (UChar**) aRow,
                                                    & aTempTable->recordNode->dstTuple->rid )
                  != IDE_SUCCESS );

        // To Fix PR-8645
        // Hash Key ���� �˻��ϱ� ���� Filter���� Access���� �����Ͽ���,
        // �� �� Execution Node���� �̸� �ٽ� ������Ű�� ��.
        // ����, ������ �ѹ��� �˻翡 ���ؼ��� accessȸ���� ���ҽ��Ѿ� ��
        if ( *aRow != NULL )
        {
            aTempTable->recordNode->dstTuple->modify--;
        }
        else
        {
            // Nothing To Do
        }
    }
    else
    {
        *aRow = NULL;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmcDiskHash::getNextRange( qmcdDiskHashTemp * aTempTable,
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

    // Cursor�� �̿��Ͽ� ���ǿ� �´� Row�� �����´�.
    IDE_TEST( smiHashTempTable::fetchHashNext( aTempTable->searchCursor,
                                               (UChar**) aRow,
                                               & aTempTable->recordNode->dstTuple->rid )
              != IDE_SUCCESS );

    // To Fix PR-8645
    // Hash Key ���� �˻��ϱ� ���� Filter���� Access���� �����Ͽ���,
    // �� �� Execution Node���� �̸� �ٽ� ������Ű�� ��.
    // ����, ������ �ѹ��� �˻翡 ���ؼ��� accessȸ���� ���ҽ��Ѿ� ��
    if ( *aRow != NULL )
    {
        aTempTable->recordNode->dstTuple->modify--;
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
qmcDiskHash::getFirstHit( qmcdDiskHashTemp * aTempTable,
                          void            ** aRow )
{
/***********************************************************************
 *
 * Description :
 *    ���� Hit�� Record�� �˻��Ѵ�.
 *
 * Implementation :
 *    ������ ���� ������ ���Ͽ� Hit �˻��� �����Ѵ�.
 *        - Hit Filter�� �����Ѵ�.
 *        - Hit�� ���� Cursor�� ����.
 *        - ������ �����ϴ� Record�� �о�´�.
 *
 ***********************************************************************/

    if( aTempTable->hitFlagCursor == NULL )
    {
        //-----------------------------------------
        // 1. Cursor�� ���� ���� ���� ���
        //-----------------------------------------
        IDE_TEST( smiHashTempTable::allocHashCursor( aTempTable->tableHandle,
                                                     aTempTable->aggrColumnList,   // Update Column����
                                                     SMI_HASH_CURSOR_FULL_SCAN,
                                                     &aTempTable->hitFlagCursor )
                  != IDE_SUCCESS );
    }

    /* HitFlag�� ������ Row�� �����´�. */
    IDE_TEST( smiHashTempTable::openFullScanCursor( aTempTable->hitFlagCursor,
                                                    SMI_TCFLAG_FORWARD |
                                                    SMI_TCFLAG_FULLSCAN |
                                                    SMI_TCFLAG_HIT,
                                                    (UChar**) aRow,
                                                    & aTempTable->recordNode->dstTuple->rid)
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmcDiskHash::getNextHit( qmcdDiskHashTemp * aTempTable,
                         void            ** aRow )
{
/***********************************************************************
 *
 * Description :
 *    ���� Hit�� Record�� �˻��Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

    // Cursor�� �̿��Ͽ� ���ǿ� �´� Row�� �����´�.
    IDE_TEST( smiHashTempTable::fetchFullNext( aTempTable->hitFlagCursor,
                                               (UChar**) aRow,
                                               & aTempTable->recordNode->dstTuple->rid )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


IDE_RC
qmcDiskHash::getFirstNonHit( qmcdDiskHashTemp * aTempTable,
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

    if( aTempTable->hitFlagCursor == NULL )
    {
        //-----------------------------------------
        // 1. Cursor�� ���� ���� ���� ���
        //-----------------------------------------
        IDE_TEST( smiHashTempTable::allocHashCursor( aTempTable->tableHandle,
                                                     aTempTable->aggrColumnList,   // Update Column����
                                                     SMI_HASH_CURSOR_FULL_SCAN,
                                                     &aTempTable->hitFlagCursor )
                  != IDE_SUCCESS );
    }

    /* HitFlag�� ���� �ȵ� Row�� �����´�. */
    IDE_TEST( smiHashTempTable::openFullScanCursor( aTempTable->hitFlagCursor,
                                                    SMI_TCFLAG_FORWARD |
                                                    SMI_TCFLAG_FULLSCAN |
                                                    SMI_TCFLAG_NOHIT,
                                                    (UChar**) aRow,
                                                    & aTempTable->recordNode->dstTuple->rid )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmcDiskHash::getNextNonHit( qmcdDiskHashTemp * aTempTable,
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

    // Cursor�� �̿��Ͽ� ���ǿ� �´� Row�� �����´�.
    IDE_TEST( smiHashTempTable::fetchFullNext( aTempTable->hitFlagCursor,
                                               (UChar**) aRow,
                                               & aTempTable->recordNode->dstTuple->rid )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmcDiskHash::getSameRowAndNonHit( qmcdDiskHashTemp * aTempTable,
                                  UInt               aHash,
                                  void             * aRow,
                                  void            ** aResultRow )
{
/***********************************************************************
 *
 * Description :
 *    Set Intersection���� �˻��� ���� ����ϸ�,
 *    �־��� Row�� ������ Hash Column�� ���� ����,
 *    Hit���� �ʴ� Row�� ã�� ���� ����Ѵ�.
 *
 * Implementation :
 *    ������ ���� ������ ����ȴ�.
 *        - Range ����
 *        - Filter ����(���� Hash & NonHit)
 *        - ���ϴ� Record �˻� �� Hit Flag ����
 *
 ***********************************************************************/

    //-----------------------------------------
    // Hash Index�� ���� Range ����
    //-----------------------------------------

    //-----------------------------------------
    // Filter ����
    //     - Non-Hit ���� �˻�
    //     - ���� Data ���� �˻�.
    // �̸� ó���ϱ� ���� �Լ��� ���� ������ �� ������,
    // CallBack Function �� �ٸ� �� ��� ������ �����ϹǷ�,
    // ���� ���ϸ� ���̱� ���� �Ʒ��� ���� ó���Ѵ�.
    //-----------------------------------------

    if( aTempTable->searchCursor == NULL )
    {
        //-----------------------------------------
        // 1. Cursor�� ���� ���� ���� ���
        //-----------------------------------------
        IDE_TEST( smiHashTempTable::allocHashCursor( aTempTable->tableHandle,
                                                     aTempTable->aggrColumnList,   // Update Column����
                                                     SMI_HASH_CURSOR_HASH_SCAN,
                                                     &aTempTable->searchCursor )
            != IDE_SUCCESS );
    }

    if( smiHashTempTable::checkHashSlot( aTempTable->searchCursor,
                                         aHash ) == ID_TRUE )
    {
        // ���� ���� �˻縦 ���� Filter ����
        IDE_TEST( makeTwoRowHashFilterCallBack( aTempTable, aRow )
                  != IDE_SUCCESS );

        /* HitFlag�� ���� �ȵ� Row�� �����´�. */
        IDE_TEST( smiHashTempTable::openHashCursor( aTempTable->searchCursor,
                                                    SMI_TCFLAG_FORWARD | 
                                                    SMI_TCFLAG_HASHSCAN |
                                                    SMI_TCFLAG_NOHIT,
                                                    &aTempTable->filterCallBack,  // Filter
                                                    (UChar**) aResultRow,
                                                    & aTempTable->recordNode->dstTuple->rid )
                  != IDE_SUCCESS );
    }
    else
    {
        *aResultRow = NULL;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmcDiskHash::getNullRow( qmcdDiskHashTemp * aTempTable,
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

    IDE_TEST( smiTempTable::getNullRow( aTempTable->tableHandle,
                                        (UChar**)aRow )
              != IDE_SUCCESS );

    SMI_MAKE_VIRTUAL_NULL_GRID( aTempTable->recordNode->dstTuple->rid );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmcDiskHash::getSameGroup( qmcdDiskHashTemp * aTempTable,
                           UInt               aHash,
                           void             * aRow,
                           void            ** aResultRow )
{
/***********************************************************************
 *
 * Description :
 *    Group Aggregation���� ���Ǹ�, ���� Row�� ������
 *    Row�� �����ϴ� ���� �˻��ϰ�, �����Ѵٸ� �̸� Return�ϰ�,
 *    �������� �ʴ´ٸ� Record�� �����Ѵ�.
 *    ���� A3������ Group Aggregation ������ ������ ������,
 *    �̴� Disk Temp Table�� ����� ��� ġ������ ���� ��Ȯ��
 *    ���� �� �� �ֱ� �����̴�.
 *         A3  : Insert --> ���� --> Aggregation --> Update
 *                      --> ���� --> �˻� --> Aggregation --> Update
 *         A4  : Aggregation --> �˻� --> ���� --> Aggregation --> Update
 *                                    --> ���� --> Insert
 *
 *    To Fix PR-8213
 *        - �׷���, ���� ���� ������ Memory Table���� �ɰ��� ����
 *          ���ϸ� �����´�.
 *          ����, ������ ���� ������� �����Ѵ�.
 *
 *    New A4 :
 *        ���� Group �˻� -->
 *        --> ���� --> Aggregation --> Update
 *        --> ����(Memory�� ��� ���Ե�) --> Init, Aggregation
 *            --> Add New Group(Memory�� ��� ������ �۾��� �������� ����)
 *
 * Implementation :
 *
 *    ������ ���� ������ �˻��Ѵ�.
 *        - �˻��� ���� Range�� Filter�� ����
 *        - Group Cursor�� ���� ���ǿ� �´� Record �˻�
 *            - �����Ѵٸ�, �̸� Return
 *            - �������� �ʴ´ٸ�, NULL�� Return
 *
 ***********************************************************************/

    // ���ռ� �˻�
    // Aggregation�� �ִ� ��쿡�� ����Ͽ��� �Ѵ�.
    IDE_DASSERT( aTempTable->aggrNode != NULL );

    if( aTempTable->groupCursor == NULL )
    {
        //-----------------------------------------
        // 1. Cursor�� ���� ���� ���� ���
        // Group Cursor �� ���⿡���� open
        //-----------------------------------------
        IDE_TEST( smiHashTempTable::allocHashCursor( aTempTable->tableHandle,
                                                     aTempTable->aggrColumnList,   // Update Column����
                                                     SMI_HASH_CURSOR_HASH_UPDATE,
                                                     &aTempTable->groupCursor )
                  != IDE_SUCCESS );
    }

    if( smiHashTempTable::checkUpdateHashSlot( aTempTable->groupCursor,
                                               aHash ) == ID_TRUE )
    {
        // BUG-10662
        // qmnGRAG::groupAggregation()�Լ������� ó���ϰ� ����
        // Record�� ���� ���� ����
        // sOrgRow = aTempTable->recordNode->dstTuple->row;

        //-----------------------------------------
        // Hash Index�� ���� �� Row���� Filter ����
        //-----------------------------------------
        IDE_TEST( makeTwoRowHashFilterCallBack( aTempTable, aRow )
                  != IDE_SUCCESS );

        //-----------------------------------------
        // ���� Group�� ã�� ���� Cursor�� ����.
        //-----------------------------------------
        IDE_TEST( smiHashTempTable::openUpdateHashCursor( aTempTable->groupCursor,
                                                          SMI_TCFLAG_FORWARD |
                                                          SMI_TCFLAG_HASHSCAN |
                                                          SMI_TCFLAG_IGNOREHIT,
                                                          &aTempTable->filterCallBack,  // Filter
                                                          (UChar**) aResultRow,
                                                          & aTempTable->recordNode->dstTuple->rid )
                  != IDE_SUCCESS );
    }
    else
    {
        *aResultRow = NULL;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmcDiskHash::getDisplayInfo( qmcdDiskHashTemp * aTempTable,
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

    // BUG-39644
    // temp table create �� �����ص�
    // MM ���� plan text �� �����ϱ� ���ؼ� ȣ���Ҽ� �ִ�.
    if ( aTempTable->tableHandle != NULL )
    {
        smiHashTempTable::getDisplayInfo(
                      aTempTable->tableHandle,
                      aPageCount,
                      aRecordCount );
    }
    else
    {
        *aPageCount   = 0;
        *aRecordCount = 0;
    }

    return IDE_SUCCESS;
}

IDE_RC
qmcDiskHash::createTempTable( qmcdDiskHashTemp * aTempTable )
{
/***********************************************************************
 *
 * Description :
 *    Disk Temp Table�� �����Ѵ�.
 *
 * Implementation :
 *    1.  Disk Temp Table ������ ���� ���� ����
 *        - Hit Flag�� ���� �ڷ� ���� �ʱ�ȭ
 *        - Insert Column List ������ ����
 *        - Aggregation Column List ������ ����
 *        - NULL ROW�� ����
 *    2.  Disk Temp Table ����
 *    3.  Temp Table Manager �� Table Handle ���
 *
 ***********************************************************************/

    UInt                   sFlag;

    //-------------------------------------------------
    // 1.  Disk Temp Table ������ ���� ���� ����
    //-------------------------------------------------

    // Insert Column List ������ ����
    IDE_TEST( setInsertColumnList( aTempTable ) != IDE_SUCCESS );

    // Aggregation Column List ������ ����
    if ( aTempTable->aggrNode != NULL )
    {
        // Aggregation ������ �ִٸ� Column List������ ����
        IDE_TEST( setAggrColumnList( aTempTable ) != IDE_SUCCESS );
    }
    else
    {
        // Nothing To Do
    }

    //-----------------------------------------
    // Key �������� Index Column ������ ����
    // createTempTable�ϱ� ���ؼ��� key column ������ ���� �����ؾ� �Ѵ�.
    //-----------------------------------------
    IDE_TEST( makeIndexColumnInfoInKEY( aTempTable ) != IDE_SUCCESS );

    //-------------------------------------------------
    // 2.  Disk Temp Table�� ����
    //-------------------------------------------------
    // Uniqueness�� ����
    if ( (aTempTable->flag & QMCD_DISK_HASH_INSERT_DISTINCT_MASK)
         == QMCD_DISK_HASH_INSERT_DISTINCT_TRUE )
    {
        // UNIQUE INDEX ����
        sFlag = SMI_TTFLAG_UNIQUE;
    }
    else
    {
        sFlag = SMI_TTFLAG_NONE;
    }

    IDE_TEST( smiHashTempTable::create( 
                aTempTable->mTemplate->stmt->mStatistics,
                QCG_GET_SESSION_TEMPSPACE_ID( aTempTable->mTemplate->stmt ),
                aTempTable->mTempTableSize, /* aWorkAreaSize */
                QC_SMI_STMT( aTempTable->mTemplate->stmt ), // smi statement
                sFlag,
                aTempTable->insertColumn,        // Table�� Column ����
                aTempTable->hashKeyColumnList,   // key column list
                (const void**)& aTempTable->tableHandle ) // Table �ڵ�
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

    //-------------------------------------------------
    // 4.  ������ Value�� ���� ������ �Ҵ����
    //-------------------------------------------------
    IDE_ERROR( aTempTable->insertValue == NULL );

    // Insert Value ������ ���� ������ �Ҵ��Ѵ�.
    IDU_LIMITPOINT("qmcDiskHash::openInsertCursor::malloc");
    IDE_TEST( aTempTable->mTemplate->stmt->qmxMem->alloc(
            ID_SIZEOF(smiValue) * (aTempTable->recordColumnCnt),
            (void**) & aTempTable->insertValue ) != IDE_SUCCESS);


    if( aTempTable->aggrColumnCnt != 0 )
    {
        IDU_LIMITPOINT("qmcDiskHash::openGroupCursor::malloc");
        IDE_TEST( aTempTable->mTemplate->stmt->qmxMem->alloc(
                ID_SIZEOF(smiValue) * aTempTable->aggrColumnCnt,
                (void**) & aTempTable->aggrValue ) != IDE_SUCCESS);
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    // BUG-39644
    aTempTable->tableHandle = NULL;

    return IDE_FAILURE;
}

IDE_RC
qmcDiskHash::setInsertColumnList( qmcdDiskHashTemp * aTempTable )
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

    UInt        i;
    mtcColumn * sColumn;

    // Column ������ŭ Memory �Ҵ�
    IDU_FIT_POINT( "qmcDiskHash::setInsertColumnList::alloc::insertColumn",
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

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmcDiskHash::setAggrColumnList( qmcdDiskHashTemp * aTempTable )
{
/***********************************************************************
 *
 * Description :
 *
 *    Aggregation ������ ���� UPDATE�� �����ϱ� ����
 *    Aggreagtion Column List������ �����Ѵ�.
 *
 * Implementation :
 *    - Column List�� ���� ���� �Ҵ�
 *    - Aggregation Node�� ��ȸ�ϸ�, �ϳ��� Aggregation Node�� ����
 *      Column������ŭ Column List�� �����Ѵ�.
 *
 ***********************************************************************/

    UInt i;
    UInt j;
    UInt sColumnCnt;
    qmdMtrNode * sNode;

    // ���ռ� �˻�.
    IDE_DASSERT ( aTempTable->aggrColumnCnt != 0 );

    // Column ������ŭ Memory �Ҵ�
    IDU_FIT_POINT( "qmcDiskHash::setAggrColumnList::alloc::aggrColumnList",
                    idERR_ABORT_InsufficientMemory );

    IDE_TEST( aTempTable->mTemplate->stmt->qmxMem->alloc(
            ID_SIZEOF(smiColumnList) * aTempTable->aggrColumnCnt,
            (void**) & aTempTable->aggrColumnList ) != IDE_SUCCESS);

    i = 0;
    for ( sNode = aTempTable->aggrNode;
          sNode != NULL;
          sNode = sNode->next )
    {
        // To Fix PR-7994, PR-8415
        // Source Node�� �̿��Ͽ� Column Count�� ȹ��

        // To Fix PR-12093
        // Destine Node�� ����Ͽ� mtcColumn�� Count�� ���ϴ� ���� ��Ģ�� ����
        // ��������� ���� mtcColumn ������ �����ϴ� ���� ���ո���.
        //     - Memory ���� ����
        //     - offset ���� ���� (PR-12093)

        sColumnCnt =
            sNode->dstNode->node.module->lflag & MTC_NODE_COLUMN_COUNT_MASK;

        // Column ������ŭ Column List ������ ����(8365)
        for ( j = 0; j < sColumnCnt; j++, i++ )
        {
            // Column ������ ����
            aTempTable->aggrColumnList[i].column =
                & sNode->dstColumn[j].column;

            // ABR�� ���� ���� �˻�
            if ( (i + 1) < aTempTable->aggrColumnCnt )
            {
                // Column List���� ���� ����
                aTempTable->aggrColumnList[i].next =
                    & aTempTable->aggrColumnList[i+1];
            }
            else
            {
                aTempTable->aggrColumnList[i].next = NULL;
            }
        }
    }

    // ���ռ� �˻�.
    // Aggregation Column������ŭ Column List ������ Setting�Ǿ� �־�� ��
    IDE_DASSERT( i == aTempTable->aggrColumnCnt );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmcDiskHash::makeIndexColumnInfoInKEY( qmcdDiskHashTemp * aTempTable )
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

    UInt            i;
    UInt            sOffset;
    qmdMtrNode    * sNode;

    //------------------------------------------------------------
    // Key �������� Index ��� Column�� ���� ����
    // �ش� Column�� Key������ ����Ǿ��� ��,
    // Record�������� offset ������ �޸� Key �������� offset������
    // ����Ǿ�� �Ѵ�.
    //------------------------------------------------------------

    // Key Column ������ ���� ���� �Ҵ�
    IDU_FIT_POINT( "qmcDiskHash::makeIndexColumnInfoInKEY::alloc::hashKeyColumn",
                    idERR_ABORT_InsufficientMemory );

    IDE_TEST( aTempTable->mTemplate->stmt->qmxMem->alloc(
                        ID_SIZEOF(mtcColumn) * aTempTable->hashColumnCnt,
                        (void**) & aTempTable->hashKeyColumn ) != IDE_SUCCESS);

    //---------------------------------------
    // Key Column�� mtcColumn ���� ����
    //---------------------------------------

    sOffset = 0;
    for ( i = 0, sNode = aTempTable->hashNode;
          i < aTempTable->hashColumnCnt;
          i++, sNode = sNode->next )
    {
        // Record���� Column ������ ����
        idlOS::memcpy( & aTempTable->hashKeyColumn[i],
                       sNode->dstColumn,
                       ID_SIZEOF(mtcColumn) );

        // fix BUG-8763
        // Index Column�� ��� ���� Flag�� ǥ���Ͽ��� ��.
        aTempTable->hashKeyColumn[i].column.flag &= ~SMI_COLUMN_USAGE_MASK;
        aTempTable->hashKeyColumn[i].column.flag |= SMI_COLUMN_USAGE_INDEX;

        // Offset ������
        if ( (aTempTable->hashKeyColumn[i].column.flag
              & SMI_COLUMN_TYPE_MASK ) == SMI_COLUMN_TYPE_FIXED )
        {
            // Fixed Column �� ���
            // Data Type�� �´� align ����
            sOffset =
                idlOS::align( sOffset,
                              aTempTable->hashKeyColumn[i].module->align );
            aTempTable->hashKeyColumn[i].column.offset = sOffset;
            aTempTable->hashKeyColumn[i].column.value = NULL;
            sOffset += aTempTable->hashKeyColumn[i].column.size;
        }
        else
        {
            //------------------------------------------
            // Variable Column�� ���
            // Variable Column Header ũ�Ⱑ �����
            // �̿� �´� align�� �����ؾ� ��.
            //------------------------------------------
            sOffset = idlOS::align( sOffset, 8 );
            aTempTable->hashKeyColumn[i].column.offset = sOffset;
            aTempTable->hashKeyColumn[i].column.value = NULL;

// PROJ_1705_PEH_TODO            
            // Fixed ���� ������ Header�� ����ȴ�.
            sOffset += smiGetVariableColumnSize( SMI_TABLE_DISK );
        }
    }

    //---------------------------------------
    // Key Column�� ���� smiColumnList ����
    // �������� ������ indexKeyColumn������ �̿��Ͽ� �����Ѵ�.
    //---------------------------------------

    // Key Column List�� ���� ���� �Ҵ�
    IDU_FIT_POINT( "qmcDiskHash::makeIndexColumnInfoInKEY::alloc::hashKeyColumnList",
                    idERR_ABORT_InsufficientMemory );

    IDE_TEST( aTempTable->mTemplate->stmt->qmxMem->alloc(
                        ID_SIZEOF(smiColumnList) * aTempTable->hashColumnCnt,
                        (void**) & aTempTable->hashKeyColumnList ) != IDE_SUCCESS);

    // Key Column List�� ���� ����
    for ( i = 0; i < aTempTable->hashColumnCnt; i++ )
    {
        // �������� ������ Column ������ �̿�
        aTempTable->hashKeyColumnList[i].column =
            & aTempTable->hashKeyColumn[i].column;

        // Array Bound Read�� �����ϱ� ���� �˻�.
        if ( (i + 1) < aTempTable->hashColumnCnt )
        {
            aTempTable->hashKeyColumnList[i].next =
                & aTempTable->hashKeyColumnList[i+1];
        }
        else
        {
            aTempTable->hashKeyColumnList[i].next = NULL;
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmcDiskHash::makeInsertSmiValue( qmcdDiskHashTemp * aTempTable )
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

    UInt        i;
    mtcColumn  *sColumn;

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
}

IDE_RC
qmcDiskHash::makeAggrSmiValue( qmcdDiskHashTemp * aTempTable )
{
/***********************************************************************
 *
 * Description :
 *    Aggregation�� UPDATE�� ���� Smi Value ���� ������ �����Ѵ�.
 *
 * Implementation :
 *    Disk Temp Table�� Record�� ���Ͽ� ������ ��������
 *    �� Column�� Value ������ �����Ѵ�.
 *    ���� �� ���� �����ϸ�, �̴� Temp Table�� ����� ������
 *    �̹� �Ҵ�� Record ������ ���� ������ �ʱ� �����̴�.
 *    ���� �� Aggregation Node�κ��� �����Ͽ� �ϳ��� ����
 *    Column������ŭ Value������ �����Ͽ��� �Ѵ�.
 *
 ***********************************************************************/

    UInt         i;
    UInt         j;
    UInt         sColumnCnt;
    qmdMtrNode * sNode;
    mtcColumn  * sColumn;

    // ���ռ� �˻�
    IDE_DASSERT( aTempTable->recordNode->dstTuple->row != NULL );

    i = 0;
    for ( sNode = aTempTable->aggrNode;
          sNode != NULL;
          sNode = sNode->next )
    {
        // To Fix PR-7994, PR-8415
        // Source Node�� �̿��Ͽ� Column Count�� ȹ��

        // To Fix PR-12093

        // Destine Node�� ����Ͽ� mtcColumn�� Count��
        // ���ϴ� ���� ��Ģ�� ����
        // ��������� ���� mtcColumn ������ �����ϴ� ���� ���ո���.
        //     - Memory ���� ����
        //     - offset ���� ���� (PR-12093)
        sColumnCnt =
            sNode->dstNode->node.module->lflag & MTC_NODE_COLUMN_COUNT_MASK;

        // Column ������ŭ Value ������ ����
        for ( j = 0; j < sColumnCnt; j++, i++ )
        {
            sColumn = &sNode->dstColumn[j];

            aTempTable->aggrValue[i].value = 
                (SChar*)sNode->dstTuple->row
                + sColumn->column.offset;

            aTempTable->aggrValue[i].length = sColumn->module->actualSize( 
                sColumn,
                (SChar*)sNode->dstTuple->row
                + sColumn->column.offset );
        }
    }

    // ���ռ� �˻�.
    // Aggregation Column������ŭ Value������ Setting�Ǿ� �־�� ��
    IDE_DASSERT( i == aTempTable->aggrColumnCnt );

    return IDE_SUCCESS;
}

IDE_RC
qmcDiskHash::makeHashFilterCallBack( qmcdDiskHashTemp * aTempTable )
{
/***********************************************************************
 *
 * Description :
 *    �־��� Filter�� �����ϴ� ���� �˻��ϱ� ���� Filter CallBack����
 *    Hash Join��� ���� ����� Filter�� �־����� ��쿡 ���ȴ�.
 *
 * Implementation :
 *    �־��� Filter�� �̿��Ͽ� Filter CallBack�� �����Ѵ�.
 *    ������ �Ϲ� Filter�� ���� ������ ������ ������� �����Ѵ�.
 *        - Call Back Data�� ����
 *        - CallBack�� ����
 *
 ***********************************************************************/

    //���ռ� �˻�
    IDE_DASSERT( aTempTable->hashFilter != NULL);

    //-------------------------------
    // CallBack Data �� ����
    //-------------------------------

    qtc::setSmiCallBack( & aTempTable->filterCallBackData,
                         aTempTable->hashFilter,
                         aTempTable->mTemplate,
                         aTempTable->recordNode->dstNode->node.table );

    //-------------------------------
    // CallBack �� ����
    //-------------------------------

    // CallBack �Լ��� ����
    aTempTable->filterCallBack.callback = qtc::smiCallBack;

    // CallBack Data�� Setting
    aTempTable->filterCallBack.data = & aTempTable->filterCallBackData;

    return IDE_SUCCESS;
}

IDE_RC
qmcDiskHash::makeTwoRowHashFilterCallBack( qmcdDiskHashTemp * aTempTable,
                                           void             * aRow )
{
/***********************************************************************
 *
 * Description :
 *    ������ Record�� ������ hash value�� ���� Record�� ã�� ����
 *    Filter CallBack�� �����Ѵ�.
 *
 * Implementation :
 *    Row���� ������ Hash Column�� ���� �񱳸� �ϱ� ���Ͽ�,
 *    ���ٸ� qtcNode ������ filter�� �������� �����Ƿ�,
 *    Hashing Column�� ���� ������ �̿��Ͽ� Filtering �ؾ� �Ѵ�.
 *
 ***********************************************************************/

    //-------------------------------
    // CallBack Data �� ����
    //-------------------------------

    // CallBack Data�� ���� �⺻ ������ �����Ѵ�.
    // ������ CallBack Data�� ���� �� ��ȿ�� ���� data->table���̴�.
    qtc::setSmiCallBack( & aTempTable->filterCallBackData,
                         aTempTable->recordNode->dstNode,
                         aTempTable->mTemplate,
                         aTempTable->recordNode->dstNode->node.table );

    // �� ��� Row�� ����
    aTempTable->filterCallBackData.calculateInfo = aRow;

    // Hashing Column ������ ������ Type Casting�Ͽ� ����
    aTempTable->filterCallBackData.node = (mtcNode*) aTempTable->hashNode;

    //-------------------------------
    // CallBack �� ����
    //-------------------------------

    // CallBack �Լ��� ����
    aTempTable->filterCallBack.callback =
        qmcDiskHash::hashTwoRowRangeFilter;

    // CallBack Data�� Setting
    aTempTable->filterCallBack.data = & aTempTable->filterCallBackData;

    return IDE_SUCCESS;
}

IDE_RC
qmcDiskHash::hashTwoRowRangeFilter( idBool         * aResult,
                                    const void     * aRow,
                                    void           *, /* aDirectKey */
                                    UInt            , /* aDirectKeyPartialSize */
                                    const scGRID,
                                    void           * aData )
{
/***********************************************************************
 *
 * Description :
 *    ���� Record�� ������ Temp Table���� Record�� ã�� ���� Filter
 *
 * Implementation :
 *    ������ Setting�Ͽ� �ѱ� CallBackData�� �̿��Ͽ�
 *    Filtering�� ������.
 *
 ***********************************************************************/

    qtcSmiCallBackData * sData;
    qmdMtrNode         * sNode;
    SInt                 sResult;
    void               * sRow;
    mtdValueInfo      sValueInfo1;
    mtdValueInfo      sValueInfo2;

    sResult = -1;
    sData = (qtcSmiCallBackData *) aData;

    //-----------------------------------------------------
    // �����Ͽ� �ѱ� CallBackData�κ��� �ʿ��� ������ ����
    //     - sData->caculateInfo : �˻��� Row������ �����
    //     - sData->node :
    //        qmdMtrNode�� hash node�� ������ type casting�Ͽ�
    //        ������.
    //-----------------------------------------------------

    sRow = sData->calculateInfo;
    sNode = (qmdMtrNode*) sData->node;

    for(  ; sNode != NULL; sNode = sNode->next )
    {
        // �� Record�� Hashing Column�� ���� ��
        sValueInfo1.column = (const mtcColumn *)sNode->func.compareColumn;
        sValueInfo1.value  = sNode->func.getRow(sNode, aRow);
        sValueInfo1.flag   = MTD_OFFSET_USE;

        sValueInfo2.column = (const mtcColumn *)sNode->func.compareColumn;
        sValueInfo2.value  = sNode->func.getRow(sNode, sRow);
        sValueInfo2.flag   = MTD_OFFSET_USE;

        sResult = sNode->func.compare( &sValueInfo1, &sValueInfo2 );
        if( sResult != 0)
        {
            // ���� �ٸ� ���, �� �̻� �񱳸� ���� ����.
            break;
        }
    }

    if ( sResult == 0 )
    {
        // ������ Record�� ã��
        *aResult = ID_TRUE;
    }
    else
    {
        // ������ Record�� �ƴ�
        *aResult = ID_FALSE;
    }

    return IDE_SUCCESS;
}

