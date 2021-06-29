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
 * $Id: qmc.cpp 89218 2020-11-12 01:36:24Z donovan.seo $
 *
 * Description :
 *     Execution���� ����ϴ� ���� ����
 *     Materialized Column�� ���� ó���� �ϴ� �۾��� �ָ� �̷��.
 *
 *
 * ��� ���� :
 *
 * ��� :
 *
 **********************************************************************/

#include <idl.h>
#include <ideErrorMgr.h>
#include <idErrorCode.h>
#include <idu.h>
#include <smi.h>
#include <mtcDef.h>
#include <qtc.h>
#include <qmc.h>
#include <qdbCommon.h>
#include <qmn.h>
#include <qci.h>
#include <qmv.h>
#include <qsxUtil.h>

extern mtdModule mtdBigint;
extern mtdModule mtdByte;
extern mtdModule mtdList;

mtcExecute qmc::valueExecute = {
    mtf::calculateNA,     // Aggregation �ʱ�ȭ �Լ�, ����
    mtf::calculateNA,     // Aggregation ���� �Լ�, ����
    mtf::calculateNA,
    mtf::calculateNA,     // Aggregation ���� �Լ�, ����
    qmc::calculateValue,  // VALUE ���� �Լ�
    NULL,                 // ������ ���� �ΰ� ����, ����
    mtx::calculateNA,
    mtk::estimateRangeNA, // Key Range ũ�� ���� �Լ�, ����
    mtk::extractRangeNA   // Key Range ���� �Լ�, ����
};

IDE_RC
qmc::calculateValue( mtcNode*     aNode,
                     mtcStack*    aStack,
                     SInt         aRemain,
                     void*,
                     mtcTemplate* aTemplate )
{
/***********************************************************************
 *
 * Description :
 *
 *    Value�� ������ �����Ѵ�.
 *
 * Implementation :
 *
 *    Stack�� column������ Value ������ Setting�Ѵ�.
 *
 ***********************************************************************/

    IDE_TEST_RAISE( aRemain < 1, ERR_STACK_OVERFLOW );

    aStack->column = aTemplate->rows[aNode->table].columns + aNode->column;

    aStack->value  = (void*) mtd::valueForModule(
        (smiColumn*)aStack->column,
        aTemplate->rows[aNode->table].row,
        MTD_OFFSET_USE,
        aStack->column->module->staticNull );

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_STACK_OVERFLOW );
    IDE_SET(ideSetErrorCode(mtERR_ABORT_STACK_OVERFLOW));

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmc::setMtrNA( qcTemplate  * /* aTemplate */,
               qmdMtrNode  * /* aNode */,
               void        * /* aRow */ )
{
    // �� �Լ��� ȣ��Ǹ� �ȵ�
    IDE_DASSERT(0);

    return IDE_FAILURE;
}

IDE_RC
qmc::setMtrByPointer( qcTemplate  * /* aTemplate */,
                      qmdMtrNode  * aNode,
                      void        * aRow )
{
/***********************************************************************
 *
 * Description :
 *    Pointer�� Column�� Materialized Row�� �����Ѵ�.
 *    ������ ���� �뵵�� ���� ���ȴ�.
 *        - Memory Base Table�� ����
 *        - Memory Column�� ����
 *
 * Implementation :
 *    Materialized Row������ ���� ��ġ�� ���ϰ�,
 *    Source Tuple�� row pointer�� �� ��ġ�� �����Ѵ�.
 ***********************************************************************/

    SChar ** sPos = (SChar**)((SChar*)aRow + aNode->dstColumn->column.offset);

    *sPos = (SChar*) aNode->srcTuple->row;

    return IDE_SUCCESS;
}

IDE_RC
qmc::setMtrByPointerAndTupleID( qcTemplate  * /* aTemplate */,
                                qmdMtrNode  * aNode,
                                void        * aRow )
{
/***********************************************************************
 *
 * Description :
 *    Pointer�� Column�� Materialized Row�� �����Ѵ�.
 *    ������ ���� �뵵�� ���� ���ȴ�.
 *        - Memory Partitioned Table�� ����
 *        - Memory Column�� ����
 *
 * Implementation :
 *    Materialized Row������ ���� ��ġ�� ���ϰ�,
 *    Source Tuple�� row pointer�� �� ��ġ�� �����Ѵ�.
 ***********************************************************************/

    qmcMemPartRowInfo   sRowInfo;
    mtdByteType       * sByte = (mtdByteType*)((SChar*)aRow + aNode->dstColumn->column.offset);

    sRowInfo.partitionTupleID = aNode->srcTuple->partitionTupleID;

    // BUG-38309
    // �޸� ��Ƽ���϶��� rid �� �����ؾ� �Ѵ�.
    sRowInfo.grid             = aNode->srcTuple->rid;
    sRowInfo.position         = (SChar*) aNode->srcTuple->row;

    sByte->length = ID_SIZEOF(qmcMemPartRowInfo);
    idlOS::memcpy( sByte->value, & sRowInfo, ID_SIZEOF(qmcMemPartRowInfo) );

    return IDE_SUCCESS;
}

IDE_RC
qmc::setMtrByRID( qcTemplate  * /* aTemplate */,
                  qmdMtrNode  * aNode,
                  void        * aRow )
{
/***********************************************************************
 *
 * Description :
 *    RID�� Materialized Row�� �����Ѵ�.
 *    ������ ���� �뵵�� ���� ���ȴ�.
 *        - Disk Base Table�� ����
 *
 * Implementation :
 *    Materialized Row������ ���� ��ġ�� ���ϰ�,
 *    Source Tuple�� RID�� �� ��ġ�� �����Ѵ�.
 ***********************************************************************/

    SChar * sPos = (SChar*)((SChar*)aRow + aNode->dstColumn->column.offset);

    idlOS::memcpy( sPos, & aNode->srcTuple->rid, ID_SIZEOF(scGRID) );

    return IDE_SUCCESS;
}

IDE_RC
qmc::setMtrByRIDAndTupleID( qcTemplate  * /* aTemplate */,
                            qmdMtrNode  * aNode,
                            void        * aRow )
{
/***********************************************************************
 *
 * Description :
 *    RID, tupleID�� Materialized Row�� �����Ѵ�.
 *    ������ ���� �뵵�� ���� ���ȴ�.
 *        - Disk Partitioned Table�� ����
 *
 * Implementation :
 *    Materialized Row������ ���� ��ġ�� ���ϰ�,
 *    Source Tuple�� RID, partitionTupleID�� �� ��ġ�� �����Ѵ�.
 ***********************************************************************/

    qmnCursorInfo       * sCursorInfo;
    qmcDiskPartRowInfo    sRowInfo;
    mtdByteType         * sByte = (mtdByteType*)((SChar*)aRow + aNode->dstColumn->column.offset);

    IDE_DASSERT( ( aNode->srcTuple->lflag & MTC_TUPLE_PARTITIONED_TABLE_MASK )
                 == MTC_TUPLE_PARTITIONED_TABLE_TRUE );

    sCursorInfo = (qmnCursorInfo *) aNode->srcTuple->cursorInfo;

    sRowInfo.partitionTupleID = aNode->srcTuple->partitionTupleID;
    sRowInfo.grid             = aNode->srcTuple->rid;

    if ( sCursorInfo != NULL )
    {
        if ( sCursorInfo->selectedIndexTuple != NULL )
        {
            sRowInfo.indexGrid = sCursorInfo->selectedIndexTuple->rid;
        }
        else
        {
            /* Nothing to do */
        }
    }
    else
    {
        /* Nothing to do */
    }

    sByte->length = ID_SIZEOF(qmcDiskPartRowInfo);

    idlOS::memcpy( sByte->value, & sRowInfo, ID_SIZEOF(qmcDiskPartRowInfo) );

    return IDE_SUCCESS;
}

IDE_RC
qmc::setMtrByValue( qcTemplate  * aTemplate,
                    qmdMtrNode  * aNode,
                    void        * /* aRow */)
{
/***********************************************************************
 *
 * Description :
 *    �������� ������ Column�� Materialized Row�� �����Ѵ�.
 *    ������ ���� �뵵�� ���� ���ȴ�.
 *        - i1 + 1 �� [+]�� ���� Expression���� ������ Column
 *
 * Implementation :
 *    Source node�� ���������μ� Materialized Row���� ������
 *    �ڿ������� �̷������.
 *    �̴� Source Column�� ���� ������ �״�� Destination�� ���������μ�
 *    �����ϸ�, Source�� ���� �� Destination���� �����ϴ� ���ϸ� ���ֱ�
 *    �����̴�.
 ***********************************************************************/

#define IDE_FN ""
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    // PROJ-2179 dstNode ��� srcNode�� �����Ѵ�.
    // ���� ����� ������ dstColumn�� ��µȴ�.
    return qtc::calculate( aNode->srcNode, aTemplate );

#undef IDE_FN
}

IDE_RC
qmc::setMtrByCopy( qcTemplate  * /*aTemplate*/,
                   qmdMtrNode  * aNode,
                   void        * aRow )
{
/***********************************************************************
 *
 * Description :
 *    Source Column�� Materialized Row�� �����Ѵ�.
 *    �ش� Data�� ���� �����ϰ� �־�� ó���� ������ Column�� ���Ͽ�
 *    Materialized Row�� ������ �� �̸� ����Ѵ�.
 *
 * Implementation :
 *    Source Column�� Data�� Destination Column�� ��ġ�� �����Ѵ�.
 *
 ***********************************************************************/

    SChar * sSrcValue;
    SChar * sDstValue;

    sSrcValue = (SChar*) mtd::valueForModule(
        (smiColumn*)aNode->srcColumn,
        aNode->srcTuple->row,
        MTD_OFFSET_USE,
        aNode->srcColumn->module->staticNull );

    sDstValue = (SChar*) mtd::valueForModule(
        (smiColumn*)aNode->dstColumn,
        aRow,
        MTD_OFFSET_USE,
        aNode->dstColumn->module->staticNull );

    idlOS::memcpy( sDstValue,
                   sSrcValue,
                   aNode->srcColumn->module->actualSize( aNode->srcColumn,
                                                         sSrcValue ) );

    return IDE_SUCCESS;
}

IDE_RC
qmc::setMtrByConvert( qcTemplate  * aTemplate,
                      qmdMtrNode  * aNode,
                      void        * aRow )
{
/***********************************************************************
 *
 * Description :
 *    Source Column�� ���� ����� Materialized Row�� �����Ѵ�.
 *    ������ ���� �뵵�� ���� ���ȴ�.
 *        - Indirect node�� ���
 *    Source Node�� ���� ����� Stack�� �����Ͽ� �� ����� �����ؾ�
 *    �ϴ� ��쿡 ����Ѵ�.
 *
 * Implementation :
 *    Source Column�� �����ϰ�,
 *    Stack ������ �̿��Ͽ�, Destination Column�� �����Ѵ�.
 *
 ***********************************************************************/

    void * sValue;

    IDE_TEST( qtc::calculate( aNode->srcNode, aTemplate ) != IDE_SUCCESS );

    sValue = (void*) mtd::valueForModule(
        (smiColumn*)aNode->dstColumn,
        aRow,
        MTD_OFFSET_USE,
        aNode->dstColumn->module->staticNull );

    idlOS::memcpy( (SChar*) sValue,
                   aTemplate->tmplate.stack->value,
                   aTemplate->tmplate.stack->column->module->actualSize(
                       aTemplate->tmplate.stack->column,
                       aTemplate->tmplate.stack->value ) );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 *
 * Description :
 *    Tuple ID�� ���� ��ü�� �Ǵ��Ͽ�, Source Column �Ǵ� Source Column ���� ����� Materialized Row�� �����Ѵ�.
 *
 * Implementation :
 ***********************************************************************/
IDE_RC qmc::setMtrByCopyOrConvert( qcTemplate  * aTemplate,
                                   qmdMtrNode  * aNode,
                                   void        * aRow )
{
    mtcColumn * sColumn = NULL;

    if ( ( aNode->dstColumn->column.flag & SMI_COLUMN_TYPE_MASK )
         == SMI_COLUMN_TYPE_FIXED )
    {
        sColumn = &aNode->srcTuple->columns[aNode->srcNode->node.column];

        IDE_DASSERT( aNode->srcColumn->column.id == sColumn->column.id );

        aNode->srcColumn = sColumn;

        IDE_TEST( qmc::setMtrByCopy( aTemplate,
                                     aNode,
                                     aRow )
                  != IDE_SUCCESS );
    }
    else
    {
        IDE_TEST( qmc::setMtrByConvert( aTemplate,
                                        aNode,
                                        aRow )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 *
 * Description :
 *    Tuple ID�� ���� ��ü�� �Ǵ��Ͽ�, Pointer �Ǵ� RID�� Materialized Row�� �����Ѵ�.
 *
 * Implementation :
 ***********************************************************************/
IDE_RC qmc::setMtrByPointerOrRIDAndTupleID( qcTemplate  * aTemplate,
                                            qmdMtrNode  * aNode,
                                            void        * aRow )
{
    qmcPartRowInfo   sRowInfo;
    mtdByteType    * sByte = (mtdByteType *)((SChar *)aRow + aNode->dstColumn->column.offset);

    if ( ( aTemplate->tmplate.rows[aNode->srcTuple->partitionTupleID].lflag & MTC_TUPLE_STORAGE_MASK )
         == MTC_TUPLE_STORAGE_MEMORY )
    {
        IDE_TEST( qmc::setMtrByPointerAndTupleID( aTemplate,
                                                  aNode,
                                                  aRow )
                  != IDE_SUCCESS );

        // byte align�̾ �����ؾ� �Ѵ�.
        idlOS::memcpy( &sRowInfo, sByte->value, ID_SIZEOF(qmcMemPartRowInfo) );

        sRowInfo.isDisk = ID_FALSE;

        idlOS::memcpy( sByte->value, & sRowInfo, ID_SIZEOF(qmcPartRowInfo) );
        sByte->length = ID_SIZEOF(qmcPartRowInfo);
    }
    else
    {
        IDE_TEST( qmc::setMtrByRIDAndTupleID( aTemplate,
                                              aNode,
                                              aRow )
                  != IDE_SUCCESS );

        // byte align�̾ �����ؾ� �Ѵ�.
        idlOS::memcpy( &sRowInfo, sByte->value, ID_SIZEOF(qmcDiskPartRowInfo) );

        sRowInfo.isDisk = ID_TRUE;

        idlOS::memcpy( sByte->value, & sRowInfo, ID_SIZEOF(qmcPartRowInfo) );
        sByte->length = ID_SIZEOF(qmcPartRowInfo);
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmc::setMtrByNull( qcTemplate  * /*aTemplate*/,
                   qmdMtrNode  * aNode,
                   void        * aRow )
{
/***********************************************************************
 *
 * Description : 
 *    �������� ������ �ʴ� Materialized Node�� ��쿡 ����
 *    ����� �� �־� null�� �ʱ�ȭ�Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

    void * sValue;

    sValue = (void*) mtd::valueForModule(
        (smiColumn*)aNode->dstColumn,
        aRow,
        MTD_OFFSET_USE,
        aNode->dstColumn->module->staticNull );

    // BUG-41681 null�� �ʱ�ȭ�Ѵ�.
    aNode->dstColumn->module->null( aNode->dstColumn, sValue );

    return IDE_SUCCESS;
}

IDE_RC
qmc::setTupleNA( qcTemplate  * /* aTemplate */,
                 qmdMtrNode  * /* aNode */,
                 void        * /* aRow */ )
{
    // �� �Լ��� ȣ��Ǹ� �ȵ�.
    IDE_DASSERT(0);

    return IDE_SUCCESS;
}

IDE_RC
qmc::setTupleByPointer( qcTemplate  * /* aTemplate */,
                        qmdMtrNode  * aNode,
                        void        * aRow )
{
/***********************************************************************
 *
 * Description :
 *    Materialized Row�� ����� pointer�� column�� Tuple Set�� ������Ų��.
 *    ������ ���� �뵵�� ���� ���ȴ�.
 *        - Temp Table�� ����� Memory Base Table�� ����
 *        - Memory Temp Table�� ���嵵�� Memory Column�� ����
 *
 * Implementation :
 *    Materialized Row���� ����� ��ġ�� ���ϰ�,
 *    �� ��ġ�� ����� pointer�� ���� Tuple�� ������Ų��.
 ***********************************************************************/

    SChar     ** sPos = (SChar**) ((SChar*)aRow + aNode->dstColumn->column.offset);
    mtcColumn  * sColumn;
    UInt         sTempTypeOffset;
    UInt         i;

    aNode->srcTuple->row = (void*) *sPos;

    /* PROJ-2334 PMT memory variable column */
    if ( ( ( aNode->srcTuple->lflag & MTC_TUPLE_PARTITIONED_TABLE_MASK )
           == MTC_TUPLE_PARTITIONED_TABLE_TRUE )
         &&
         ( ( ( aNode->srcTuple->columns[aNode->srcNode->node.column].
               column.flag & SMI_COLUMN_TYPE_MASK )
             == SMI_COLUMN_TYPE_VARIABLE ) ||
           ( ( aNode->srcTuple->columns[aNode->srcNode->node.column].
               column.flag & SMI_COLUMN_TYPE_MASK )
             == SMI_COLUMN_TYPE_VARIABLE_LARGE ) ) )
    {
        sColumn = &aNode->srcTuple->columns[aNode->srcNode->node.column];

        IDE_DASSERT( aNode->srcColumn->column.id == sColumn->column.id );

        aNode->srcColumn = sColumn;

        // ���� �ƴ� Pointer�� ����Ǵ� ���
        aNode->func.compareColumn = sColumn;
    }
    else
    {
        // Nothing to do.
    }
    
    if ( ( aNode->flag & QMC_MTR_BASETABLE_MASK ) == QMC_MTR_BASETABLE_TRUE )
    {
        // PROJ-2362 memory temp ���� ȿ���� ����
        // baseTable�� ������ TEMP_TYPE�� ����ؾ� �Ѵ�.
        sColumn = aNode->srcTuple->columns;

        for ( i = 0; i < aNode->srcTuple->columnCount; i++, sColumn++ )
        {
            if ( ( sColumn->column.flag & SMI_COLUMN_TYPE_MASK )
                 == SMI_COLUMN_TYPE_TEMP_1B )
            {
                sTempTypeOffset =
                    *(UChar*)((UChar*)aNode->srcTuple->row + sColumn->column.offset);
            }
            else if ( ( sColumn->column.flag & SMI_COLUMN_TYPE_MASK )
                      == SMI_COLUMN_TYPE_TEMP_2B )
            {
                sTempTypeOffset =
                    *(UShort*)((UChar*)aNode->srcTuple->row + sColumn->column.offset);
            }
            else if ( ( sColumn->column.flag & SMI_COLUMN_TYPE_MASK )
                      == SMI_COLUMN_TYPE_TEMP_4B )
            {
                sTempTypeOffset =
                    *(UInt*)((UChar*)aNode->srcTuple->row + sColumn->column.offset);
            }
            else
            {
                sTempTypeOffset = 0;
            }

            if ( sTempTypeOffset > 0 )
            {
                IDE_DASSERT( sColumn->module->actualSize(
                                 sColumn,
                                 (SChar*)aNode->srcTuple->row + sTempTypeOffset )
                             <= sColumn->column.size );

                idlOS::memcpy( (SChar*)sColumn->column.value,
                               (SChar*)aNode->srcTuple->row + sTempTypeOffset,
                               sColumn->module->actualSize(
                                   sColumn,
                                   (SChar*)aNode->srcTuple->row + sTempTypeOffset ) );
            }
            else
            {
                // Nothing to do.
            }
        }
    }
    else
    {
        // PROJ-2362 memory temp ���� ȿ���� ����
        // src�� TEMP_TYPE�� ��� ������ ����ؾ� �Ѵ�.
        if ( ( aNode->srcColumn->column.flag & SMI_COLUMN_TYPE_MASK )
             == SMI_COLUMN_TYPE_TEMP_1B )
        {
            sTempTypeOffset =
                *(UChar*)((UChar*)aNode->srcTuple->row + aNode->srcColumn->column.offset);
        }
        else if ( ( aNode->srcColumn->column.flag & SMI_COLUMN_TYPE_MASK )
                  == SMI_COLUMN_TYPE_TEMP_2B )
        {
            sTempTypeOffset =
                *(UShort*)((UChar*)aNode->srcTuple->row + aNode->srcColumn->column.offset);
        }
        else if ( ( aNode->srcColumn->column.flag & SMI_COLUMN_TYPE_MASK )
                  == SMI_COLUMN_TYPE_TEMP_4B )
        {
            sTempTypeOffset =
                *(UInt*)((UChar*)aNode->srcTuple->row + aNode->srcColumn->column.offset);
        }
        else
        {
            sTempTypeOffset = 0;
        }

        if ( sTempTypeOffset > 0 )
        {
            IDE_DASSERT( aNode->srcColumn->module->actualSize(
                             aNode->srcColumn,
                             (SChar*)aNode->srcTuple->row + sTempTypeOffset )
                         <= aNode->srcColumn->column.size );

            idlOS::memcpy( (SChar*)aNode->srcColumn->column.value,
                           (SChar*)aNode->srcTuple->row + sTempTypeOffset,
                           aNode->srcColumn->module->actualSize(
                               aNode->srcColumn,
                               (SChar*)aNode->srcTuple->row + sTempTypeOffset ) );
        }
        else
        {
            // Nothing to do.
        }
    }

    return IDE_SUCCESS;
}

IDE_RC qmc::setTupleByPointer4Rid(qcTemplate  * /* aTemplate */,
                                  qmdMtrNode  * aNode,
                                  void        * aRow)
{
    IDE_RC  sRc;
    SChar** sPos;

    sPos = (SChar**)((SChar*)aRow + aNode->dstColumn->column.offset);
    aNode->srcTuple->row = (void*)*sPos;

    sRc = smiGetGRIDFromRowPtr((void*)*sPos, &aNode->srcTuple->rid);
    IDE_TEST(sRc != IDE_SUCCESS);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmc::setTupleByPointerAndTupleID( qcTemplate  * aTemplate,
                                  qmdMtrNode  * aNode,
                                  void        * aRow )
{
/***********************************************************************
 *
 * Description :
 *    Materialized Row�� ����� pointer�� column�� Tuple Set�� ������Ų��.
 *    ������ ���� �뵵�� ���� ���ȴ�.
 *        - Temp Table�� ����� Memory Base Table�� ����
 *        - Memory Temp Table�� ���嵵�� Memory Column�� ����
 *
 * Implementation :
 *    Materialized Row���� ����� ��ġ�� ���ϰ�,
 *    �� ��ġ�� ����� pointer�� ���� Tuple�� ������Ų��.
 ***********************************************************************/

    qmcMemPartRowInfo   sRowInfo;
    mtdByteType       * sByte = (mtdByteType*) ((SChar*)aRow + aNode->dstColumn->column.offset);

    mtcColumn  * sColumn;
    UInt         sTempTypeOffset;
    UInt         i;

    IDE_DASSERT( ( sByte->length == ID_SIZEOF(qmcMemPartRowInfo) ) ||
                 ( sByte->length == ID_SIZEOF(qmcPartRowInfo) ) );

    // byte align�̾ �����ؾ��Ѵ�.
    idlOS::memcpy( & sRowInfo, sByte->value, ID_SIZEOF(qmcMemPartRowInfo) );

    aNode->srcTuple->partitionTupleID = sRowInfo.partitionTupleID;
    aNode->srcTuple->row = (void*)(sRowInfo.position);

    aTemplate->tmplate.rows[sRowInfo.partitionTupleID].row =
        aNode->srcTuple->row;

    aNode->srcTuple->columns =
        aTemplate->tmplate.rows[sRowInfo.partitionTupleID].columns;

    /* PROJ-2464 hybrid partitioned table ���� */
    aNode->srcTuple->rowOffset   = aTemplate->tmplate.rows[sRowInfo.partitionTupleID].rowOffset;
    aNode->srcTuple->rowMaximum  = aTemplate->tmplate.rows[sRowInfo.partitionTupleID].rowMaximum;
    aNode->srcTuple->tableHandle = aTemplate->tmplate.rows[sRowInfo.partitionTupleID].tableHandle;

    // BUG-38309
    // �޸� ��Ƽ���϶��� rid �� �����ؾ� �Ѵ�.
    aNode->srcTuple->rid = sRowInfo.grid;

    /* PROJ-2334 PMT memory variable column */
    /* PROJ-2464 hybrid partitioned table ���� */
    if ( ( sByte->length == ID_SIZEOF(qmcPartRowInfo) )
         ||
         ( ( ( aNode->srcTuple->lflag & MTC_TUPLE_PARTITIONED_TABLE_MASK )
             == MTC_TUPLE_PARTITIONED_TABLE_TRUE )
           &&
           ( ( ( aNode->srcTuple->columns[aNode->srcNode->node.column].
                 column.flag & SMI_COLUMN_TYPE_MASK )
               == SMI_COLUMN_TYPE_VARIABLE ) ||
             ( ( aNode->srcTuple->columns[aNode->srcNode->node.column].
                 column.flag & SMI_COLUMN_TYPE_MASK )
               == SMI_COLUMN_TYPE_VARIABLE_LARGE ) ) ) )
    {
        sColumn = &aNode->srcTuple->columns[aNode->srcNode->node.column];

        IDE_DASSERT( aNode->srcColumn->column.id == sColumn->column.id );

        aNode->srcColumn = sColumn;

        // ���� �ƴ� Pointer�� ����Ǵ� ���
        aNode->func.compareColumn = sColumn;
    }
    else
    {
        // Nothing to do.
    }
    
    if ( ( aNode->flag & QMC_MTR_BASETABLE_MASK ) == QMC_MTR_BASETABLE_TRUE )
    {
        // PROJ-2362 memory temp ���� ȿ���� ����
        // baseTable�� ������ TEMP_TYPE�� ����ؾ� �Ѵ�.
        sColumn = aNode->srcTuple->columns;

        for ( i = 0; i < aNode->srcTuple->columnCount; i++, sColumn++ )
        {
            if ( ( sColumn->column.flag & SMI_COLUMN_TYPE_MASK )
                 == SMI_COLUMN_TYPE_TEMP_1B )
            {
                sTempTypeOffset =
                    *(UChar*)((UChar*)aNode->srcTuple->row + sColumn->column.offset);
            }
            else if ( ( sColumn->column.flag & SMI_COLUMN_TYPE_MASK )
                      == SMI_COLUMN_TYPE_TEMP_2B )
            {
                sTempTypeOffset =
                    *(UShort*)((UChar*)aNode->srcTuple->row + sColumn->column.offset);
            }
            else if ( ( sColumn->column.flag & SMI_COLUMN_TYPE_MASK )
                      == SMI_COLUMN_TYPE_TEMP_4B )
            {
                sTempTypeOffset =
                    *(UInt*)((UChar*)aNode->srcTuple->row + sColumn->column.offset);
            }
            else
            {
                sTempTypeOffset = 0;
            }

            if ( sTempTypeOffset > 0 )
            {
                IDE_DASSERT( sColumn->module->actualSize(
                                 sColumn,
                                 (SChar*)aNode->srcTuple->row + sTempTypeOffset )
                             <= sColumn->column.size );

                idlOS::memcpy( (SChar*)sColumn->column.value,
                               (SChar*)aNode->srcTuple->row + sTempTypeOffset,
                               sColumn->module->actualSize(
                                   sColumn,
                                   (SChar*)aNode->srcTuple->row + sTempTypeOffset ) );
            }
            else
            {
                // Nothing to do.
            }
        }
    }
    else
    {
        // PROJ-2362 memory temp ���� ȿ���� ����
        // src�� TEMP_TYPE�� ��� ������ ����ؾ� �Ѵ�.
        if ( ( aNode->srcColumn->column.flag & SMI_COLUMN_TYPE_MASK )
             == SMI_COLUMN_TYPE_TEMP_1B )
        {
            sTempTypeOffset =
                *(UChar*)((UChar*)aNode->srcTuple->row + aNode->srcColumn->column.offset);
        }
        else if ( ( aNode->srcColumn->column.flag & SMI_COLUMN_TYPE_MASK )
                  == SMI_COLUMN_TYPE_TEMP_2B )
        {
            sTempTypeOffset =
                *(UShort*)((UChar*)aNode->srcTuple->row + aNode->srcColumn->column.offset);
        }
        else if ( ( aNode->srcColumn->column.flag & SMI_COLUMN_TYPE_MASK )
                  == SMI_COLUMN_TYPE_TEMP_4B )
        {
            sTempTypeOffset =
                *(UInt*)((UChar*)aNode->srcTuple->row + aNode->srcColumn->column.offset);
        }
        else
        {
            sTempTypeOffset = 0;
        }

        if ( sTempTypeOffset > 0 )
        {
            IDE_DASSERT( aNode->srcColumn->module->actualSize(
                             aNode->srcColumn,
                             (SChar*)aNode->srcTuple->row + sTempTypeOffset )
                         <= aNode->srcColumn->column.size );

            idlOS::memcpy( (SChar*)aNode->srcColumn->column.value,
                           (SChar*)aNode->srcTuple->row + sTempTypeOffset,
                           aNode->srcColumn->module->actualSize(
                               aNode->srcColumn,
                               (SChar*)aNode->srcTuple->row + sTempTypeOffset ) );
        }
        else
        {
            // Nothing to do.
        }
    }

    return IDE_SUCCESS;
}

IDE_RC
qmc::setTupleByRID( qcTemplate  * aTemplate,
                    qmdMtrNode  * aNode,
                    void        * aRow )
{
/***********************************************************************
 *
 * Description :
 *    Materialized Row�� ����� RID column�� Tuple Set�� ������Ų��.
 *    ������ ���� �뵵�� ���� ���ȴ�.
 *        - Temp Table�� ����� Disk Base Table�� ����
 *
 * Implementation :
 *    Materialized Row���� ����� ��ġ�� ���ϰ�,
 *    RID�� RID�� �ش��ϴ� Record�� ������Ų��.
 ***********************************************************************/

    smiStatement * sStmt;
    IDE_RC         sRc;
    mtcColumn    * sColumn;
    void         * sValueTemp;
    UInt           i;

    scGRID * sRID = (scGRID*) ((SChar*)aRow + aNode->dstColumn->column.offset);

    //--------------------------------
    // RID ����
    //--------------------------------

    idlOS::memcpy( & aNode->srcTuple->rid, sRID, ID_SIZEOF(scGRID) );

    //--------------------------------
    // RID�κ��� Record ����
    //--------------------------------

    // BUG-24330
    // outer-join ��� ������ null rid�� ��� ���� null row�� �����Ѵ�.
    if ( SMI_GRID_IS_VIRTUAL_NULL( aNode->srcTuple->rid ) )
    {
        for ( i = 0, sColumn = aNode->srcTuple->columns;
              i < aNode->srcTuple->columnCount;
              i++, sColumn++ )
        {
            // BUG-39238
            // Set NULL OID for compressed column.
            if ( ( ((smiColumn*)sColumn)->flag & SMI_COLUMN_COMPRESSION_MASK )
                 == SMI_COLUMN_COMPRESSION_TRUE )
            {
                *((smOID*)((SChar*)aNode->srcTuple->row + ((smiColumn*)sColumn)->offset)) = SMI_NULL_OID;
            }
            else
            {
                // NULL Padding
                sValueTemp = (void*) mtd::valueForModule(
                    (smiColumn*)sColumn,
                    aNode->srcTuple->row,
                    MTD_OFFSET_USE,
                    sColumn->module->staticNull );

                sColumn->module->null( sColumn,
                                       sValueTemp );
            }
        }
    }
    else
    {
        sStmt = QC_SMI_STMT( aTemplate->stmt );

        // PROJ-1705
        // rid�� ���ڵ带 ��ġ�ϴ� ���,
        // disk table�� disk temp table�� �����ϱ� ���� �÷�������.
        // PROJ-1705 ������Ʈ�� ��ũ���̺� ���ؼ��� ����Ǳ⶧����
        // ��ũ���̺�� ��ũ�������̺��� rid�� �ٸ���.
        // �� rid�� qp�� GRID��� ����ü��
        // ��ũ���̺�� ��ũ�������̺� ���о��� ����ϱ�� �ϰ�,
        // rid�� ���ڵ���ġ�� ( smiFetchRowFromGRID() )
        // sm�� ��ũ���̺�� ��ũ�������̺� �´� rid��
        // �����ؼ� �����ֱ�� ��.

        // BUG-37277
        IDE_TEST_RAISE( SC_GRID_IS_NULL( aNode->srcTuple->rid ),
                        ERR_NULL_GRID );

        if( ( aNode->flag & QMC_MTR_BASETABLE_TYPE_MASK )
            == QMC_MTR_BASETABLE_TYPE_DISKTABLE )
        {
            sRc = smiFetchRowFromGRID( aTemplate->stmt->mStatistics,
                                       sStmt,
                                       SMI_TABLE_DISK,
                                       aNode->srcTuple->rid,
                                       aNode->fetchColumnList,
                                       aNode->srcTuple->tableHandle,
                                       aNode->srcTuple->row );
        }
        else
        {
            /* PROJ-2201 Innovation in sorting and hashing(temp) */
            sRc = smiTempTable::fetchFromGRID(
                aNode->srcTuple->tableHandle,
                aNode->srcTuple->rid,
                aNode->srcTuple->row );
        }

        IDE_TEST( sRc != IDE_SUCCESS );

        // ���ϴ� Data�� ������ �ȵ�
        IDE_DASSERT( aNode->srcTuple->row != NULL );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_NULL_GRID )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmc::setTupleByRID",
                                  "null grid" ));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmc::setTupleByRIDAndTupleID( qcTemplate  * aTemplate,
                              qmdMtrNode  * aNode,
                              void        * aRow )
{
/***********************************************************************
 *
 * Description :
 *    Materialized Row�� ����� RID column�� Tuple Set�� ������Ų��.
 *    ������ ���� �뵵�� ���� ���ȴ�.
 *        - Temp Table�� ����� Disk Base Table�� ����
 *
 * Implementation :
 *    Materialized Row���� ����� ��ġ�� ���ϰ�,
 *    RID�� RID�� �ش��ϴ� Record�� ������Ų��.
 ***********************************************************************/

    smiStatement * sStmt;
    mtcTuple     * sPartTuple;
    mtcColumn    * sColumn;
    void         * sValueTemp;
    UInt           i;

    qmcDiskPartRowInfo   sRowInfo;
    mtdByteType        * sByte       = (mtdByteType*) ((SChar*)aRow + aNode->dstColumn->column.offset);
    qmnCursorInfo      * sCursorInfo = (qmnCursorInfo *) aNode->srcTuple->cursorInfo;

    //--------------------------------
    // RID ����
    //--------------------------------

    IDE_DASSERT( ( sByte->length == ID_SIZEOF(qmcDiskPartRowInfo) ) ||
                 ( sByte->length == ID_SIZEOF(qmcPartRowInfo) ) );

    // byte align�̾ �����ؾ��Ѵ�.
    idlOS::memcpy( & sRowInfo, sByte->value, ID_SIZEOF(qmcDiskPartRowInfo) );

    // BUG-37507 partitionTupleID �� ID_USHORT_MAX �϶��� �ܺ������϶��̴�.
    // ���ʿ��� mtr node �̴�.
    IDE_TEST_CONT( sRowInfo.partitionTupleID == ID_USHORT_MAX, skip );

    sPartTuple = & aTemplate->tmplate.rows[sRowInfo.partitionTupleID];

    aNode->srcTuple->rid = sRowInfo.grid;
    aNode->srcTuple->partitionTupleID = sRowInfo.partitionTupleID;

    if ( sCursorInfo != NULL )
    {
        if ( sCursorInfo->selectedIndexTuple != NULL )
        {
            sCursorInfo->selectedIndexTuple->rid = sRowInfo.indexGrid;
        }
    }

    aNode->srcTuple->columns = sPartTuple->columns;

    /* PROJ-2464 hybrid partitioned table ���� */
    aNode->srcTuple->rowOffset   = sPartTuple->rowOffset;
    aNode->srcTuple->rowMaximum  = sPartTuple->rowMaximum;
    aNode->srcTuple->tableHandle = sPartTuple->tableHandle;

    /* PROJ-2464 hybrid partitioned table ���� */
    if ( sByte->length == ID_SIZEOF(qmcPartRowInfo) )
    {
        sColumn = &aNode->srcTuple->columns[aNode->srcNode->node.column];

        IDE_DASSERT( aNode->srcColumn->column.id == sColumn->column.id );

        aNode->srcColumn = sColumn;

        // ���� ������� �ʴ� ���
        aNode->func.compareColumn = sColumn;
    }
    else
    {
        /* Nothing to do */
    }

    //--------------------------------
    // RID�κ��� Record ����
    //--------------------------------

    // BUG-24330
    // outer-join ��� ������ null rid�� ��� ���� null row�� �����Ѵ�.
    if ( SMI_GRID_IS_VIRTUAL_NULL( aNode->srcTuple->rid ) )
    {
        // ���ϴ� Data�� ������ �ȵ�
        IDE_DASSERT( sPartTuple->row != NULL );

        for ( i = 0, sColumn = sPartTuple->columns;
              i < sPartTuple->columnCount;
              i++, sColumn++ )
        {
            // BUG-39238
            // Set NULL OID for compressed column.
            // BUG-42417 Partitioned Table�� Compressed Column�� �������� �ʴ´�.

            // NULL Padding
            sValueTemp = (void*) mtd::valueForModule(
                (smiColumn*)sColumn,
                sPartTuple->row,
                MTD_OFFSET_USE,
                sColumn->module->staticNull );

            sColumn->module->null( sColumn,
                                   sValueTemp );
        }

        sPartTuple->rid = aNode->srcTuple->rid;

        aNode->srcTuple->row = sPartTuple->row;
    }
    else
    {
        sStmt = QC_SMI_STMT( aTemplate->stmt );

        //-------------------------------------------------
        // PROJ-1705
        // QMC_MTR_TYPE_DISK_TABLE �� ���
        // rid�� ���ڵ� ��ġ�� smiColumnList�� �ʿ���.
        // ���⼭ ������ smiColumnList�� smiFetchRowFromGRID()�Լ��� ���ڷ� ����.
        // (��ũ�������̺��� �ش���� �ʴ´�.)
        //-------------------------------------------------

        /* PROJ-2464 hybrid partitioned table ����
         *  Hybrid�� ��쿡�� Partitioned�� Type�� Disk�� �ƴ� �� �ִ�.
         *  ����, qmc::setMtrNode()���� �Ҵ��� fetchColumnList�� Disk�� �°� �����Ѵ�.
         */
        if ( aNode->fetchColumnList != NULL )
        {
            if ( ( aNode->fetchColumnList->column->flag & SMI_COLUMN_STORAGE_MASK ) !=
                 ( sPartTuple->columns->column.flag & SMI_COLUMN_STORAGE_MASK ) )
            {
                IDE_TEST( qdbCommon::makeFetchColumnList4TupleID(
                              aTemplate,
                              sRowInfo.partitionTupleID,
                              ID_FALSE,  // aIsNeedAllFetchColumn
                              NULL,      // index ����
                              ID_FALSE,  // aIsAllocSmiColumnList
                              & aNode->fetchColumnList )
                          != IDE_SUCCESS );
            }
            else
            {
                /* Nothing to do */
            }
        }
        else
        {
            /* Nothing to do */
        }

        // BUG-37277
        IDE_TEST_RAISE( SC_GRID_IS_NULL( aNode->srcTuple->rid ),
                        ERR_NULL_GRID );

        IDE_TEST( smiFetchRowFromGRID( aTemplate->stmt->mStatistics,
                                       sStmt,
                                       SMI_TABLE_DISK,
                                       aNode->srcTuple->rid,
                                       aNode->fetchColumnList,
                                       aNode->srcTuple->tableHandle,
                                       sPartTuple->row )
                  != IDE_SUCCESS );

        // ���ϴ� Data�� ������ �ȵ�
        IDE_DASSERT( sPartTuple->row != NULL );

        sPartTuple->rid = aNode->srcTuple->rid;

        aNode->srcTuple->row = sPartTuple->row;
    }

    IDE_EXCEPTION_CONT( skip );

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_NULL_GRID )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmc::setTupleByRIDAndTupleID",
                                  "null grid" ));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


IDE_RC
qmc::setTupleByValue( qcTemplate  * /* aTemplate */,
                      qmdMtrNode  * aNode,
                      void        * aRow )
{
/***********************************************************************
 *
 * Description :
 *    ó�� ������ ������Ű�� �ʾƵ� �Ǵ� Column���� �ƹ� �ϵ� ���� �ʴ´�.
 *
 * Implementation :
 *
 ***********************************************************************/

    mtcStack * sStack;
    UChar    * sValue;
    UInt       sTempTypeOffset;
    SInt       i;

    if ( ( aNode->dstColumn->column.flag & SMI_COLUMN_TYPE_MASK )
         == SMI_COLUMN_TYPE_TEMP_1B )
    {
        sTempTypeOffset = *(UChar*)((UChar*)aRow + aNode->dstColumn->column.offset);
    }
    else if ( ( aNode->dstColumn->column.flag & SMI_COLUMN_TYPE_MASK )
              == SMI_COLUMN_TYPE_TEMP_2B )
    {
        sTempTypeOffset = *(UShort*)((UChar*)aRow + aNode->dstColumn->column.offset);
    }
    else if ( ( aNode->dstColumn->column.flag & SMI_COLUMN_TYPE_MASK )
              == SMI_COLUMN_TYPE_TEMP_4B )
    {
        sTempTypeOffset = *(UInt*)((UChar*)aRow + aNode->dstColumn->column.offset);
    }
    else
    {
        sTempTypeOffset = 0;
    }

    if ( sTempTypeOffset > 0 )
    {
        IDE_DASSERT( aNode->dstColumn->module->actualSize(
                         aNode->dstColumn,
                         (SChar*)aRow + sTempTypeOffset )
                     <= aNode->dstColumn->column.size );

        idlOS::memcpy( (SChar*)aNode->dstColumn->column.value,
                       (SChar*)aRow + sTempTypeOffset,
                       aNode->dstColumn->module->actualSize(
                           aNode->dstColumn,
                           (SChar*)aRow + sTempTypeOffset ) );
    }
    else
    {
        // Nothing to do.
    }

    // BUG-39552 list�� value pointer ������
    if ( ( aNode->dstColumn->module == &mtdList ) &&
         ( aNode->dstColumn->precision > 0 ) &&
         ( aNode->dstColumn->scale > 0 ) )
    {
        IDE_DASSERT( ( aNode->dstColumn->column.flag & SMI_COLUMN_TYPE_MASK )
                     == SMI_COLUMN_TYPE_FIXED );
        IDE_DASSERT( ID_SIZEOF(mtcStack) * aNode->dstColumn->precision +
                     aNode->dstColumn->scale
                     == aNode->dstColumn->column.size );

        sStack = (mtcStack*)((UChar*)aRow + aNode->dstColumn->column.offset);
        sValue = (UChar*)(sStack + aNode->dstColumn->precision);

        for ( i = 0; i < aNode->dstColumn->precision; i++, sStack++ )
        {
            sStack->value = (void*)sValue;
            /* BUG-48273 */
            sValue += idlOS::align( sStack->column->column.size, sStack->column->module->align );
        }
        
        //IDE_DASSERT( (UChar*)aRow + aNode->dstColumn->column.offset +
        //             aNode->dstColumn->column.size
        //             == sValue );
    }
    else
    {
        // Nothing to do.
    }

    return IDE_SUCCESS;
}

/***********************************************************************
 *
 * Description :
 *    Tuple ID�� ���� ��ü�� �Ǵ��Ͽ�, Materialized Row�� ����� Pointer �Ǵ� RID�� Tuple Set�� �����Ѵ�.
 *
 * Implementation :
 ***********************************************************************/
IDE_RC qmc::setTupleByPointerOrRIDAndTupleID( qcTemplate  * aTemplate,
                                              qmdMtrNode  * aNode,
                                              void        * aRow )
{
    qmcPartRowInfo   sRowInfo;
    mtdByteType    * sByte = (mtdByteType *)((SChar *)aRow + aNode->dstColumn->column.offset);

    IDE_DASSERT( sByte->length == ID_SIZEOF(qmcPartRowInfo) );

    // byte align�̾ �����ؾ� �Ѵ�.
    idlOS::memcpy( &sRowInfo, sByte->value, ID_SIZEOF(qmcPartRowInfo) );

    if ( sRowInfo.isDisk != ID_TRUE )
    {
        IDE_TEST( qmc::setTupleByPointerAndTupleID( aTemplate,
                                                    aNode,
                                                    aRow )
                  != IDE_SUCCESS );
    }
    else
    {
        IDE_TEST( qmc::setTupleByRIDAndTupleID( aTemplate,
                                                aNode,
                                                aRow )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmc::setTupleByNull( qcTemplate  * /* aTemplate */,
                     qmdMtrNode  * /* aNode */,
                     void        * /* aRow */ )
{
/***********************************************************************
 *
 * Description :
 *    �������� ������ �ʴ� Materialized Node�� ����
 *    �ƹ��͵� ���� �ʴ´�.
 *
 * Implementation :
 *
 ***********************************************************************/

    return IDE_SUCCESS;
}

UInt
qmc::getHashNA( UInt         /* aValue */,
                qmdMtrNode * /* aNode */,
                void       * /* aRow */ )
{
    // �� �Լ��� ȣ��Ǹ� �ȵ�.

    IDE_DASSERT(0);

    return 0;
}

UInt
qmc::getHashByPointer( UInt         aValue,
                       qmdMtrNode * aNode,
                       void       * aRow )
{
/***********************************************************************
 *
 * Description :
 *    �ش� Materialized Column�� Hash���� ��´�.
 *
 * Implementation :
 *    Pointer�� ����� ���� ���� Record Pointer�� ȹ���ϰ�,
 *    Source Column������ �̿��Ͽ� Hash���� ��´�.
 *
 *    PROJ-2334 PMT memory partitioned table or variable column�� ���
 *    partition table�� column������ �̿� �Ͽ� Hash���� ��´�.
 ***********************************************************************/

    void * sRow = qmc::getRowByPointer( aNode, aRow );
    void * sValue;

    sValue = (void*) mtd::valueForModule(
        (smiColumn*)aNode->srcColumn,
        sRow,
        MTD_OFFSET_USE,
        aNode->srcColumn->module->staticNull );

    return aNode->srcColumn->module->hash( aValue,
                                           aNode->srcColumn,
                                           sValue );
}

UInt
qmc::getHashByPointerAndTupleID( UInt         aValue,
                                 qmdMtrNode * aNode,
                                 void       * aRow )
{
/***********************************************************************
 *
 * Description :
 *    �ش� Materialized Column�� Hash���� ��´�.
 *
 * Implementation :
 *    Pointer�� ����� ���� ���� Record Pointer�� ȹ���ϰ�,
 *    Source Column������ �̿��Ͽ� Hash���� ��´�.
 *
 *    PROJ-2334 PMT memory partitioned table or variable column�� ���
 *    partition table�� column������ �̿� �Ͽ� Hash���� ��´�.
 ***********************************************************************/

    void * sRow = qmc::getRowByPointerAndTupleID( aNode, aRow );
    void * sValue;

    sValue = (void*) mtd::valueForModule(
        (smiColumn*)aNode->srcColumn,
        sRow,
        MTD_OFFSET_USE,
        aNode->srcColumn->module->staticNull );

    return aNode->srcColumn->module->hash( aValue,
                                           aNode->srcColumn,
                                           sValue );
}

UInt
qmc::getHashByValue( UInt         aValue,
                     qmdMtrNode * aNode,
                     void       * aRow )
{
/***********************************************************************
 *
 * Description :
 *    �ش� Materialized Column�� Hash���� ��´�.
 *
 * Implementation :
 *    Value�� ��ü�� ����� ����,
 *    Destination Column������ �̿��Ͽ� Hash���� ��´�.
 *
 ***********************************************************************/

    void * sValue;

    sValue = (void*) mtd::valueForModule(
        (smiColumn*)aNode->dstColumn,
        aRow,
        MTD_OFFSET_USE,
        aNode->dstColumn->module->staticNull );

    return aNode->dstColumn->module->hash( aValue,
                                           aNode->dstColumn,
                                           sValue );
}

idBool
qmc::isNullNA( qmdMtrNode * /* aNode */,
               void       * /* aRow */ )
{
    // �� �Լ��� ȣ��Ǹ� �ȵ�.

    IDE_DASSERT(0);

    return ID_FALSE;
}

idBool
qmc::isNullByPointer( qmdMtrNode * aNode,
                      void       * aRow )
{
/***********************************************************************
 *
 * Description :
 *    �ش� Materialized Row�� ����� pointer ������ hash ���� ��´�.
 *
 * Implementation :
 *    Row�� pointer���� ��´�.
 *    Source Node�� ������ Row�κ��� hash���� ��´�.
 *
 ***********************************************************************/

    void * sRow = qmc::getRowByPointer( aNode, aRow );
    void * sValue;

    sValue = (void*) mtd::valueForModule(
        (smiColumn*)aNode->srcColumn,
        sRow,
        MTD_OFFSET_USE,
        aNode->srcColumn->module->staticNull );

    return aNode->srcColumn->module->isNull( aNode->srcColumn,
                                             sValue );
}

idBool
qmc::isNullByPointerAndTupleID(  qmdMtrNode * aNode,
                                 void       * aRow )
{
/***********************************************************************
 *
 * Description :
 *    �ش� Materialized Row�� ����� pointer ������ hash ���� ��´�.
 *
 * Implementation :
 *    Row�� pointer���� ��´�.
 *    Source Node�� ������ Row�κ��� hash���� ��´�.
 *
 ***********************************************************************/

    void * sRow = qmc::getRowByPointerAndTupleID( aNode, aRow );
    void * sValue;

    sValue = (void*) mtd::valueForModule(
        (smiColumn*)aNode->srcColumn,
        sRow,
        MTD_OFFSET_USE,
        aNode->srcColumn->module->staticNull );

    return aNode->srcColumn->module->isNull( aNode->srcColumn,
                                             sValue );
}

idBool
qmc::isNullByValue( qmdMtrNode * aNode,
                    void       * aRow )
{
/***********************************************************************
 *
 * Description :
 *    �ش� Materialized Row�� ����� Value������ NULL ���θ� �Ǵ��Ѵ�.
 *
 * Implementation :
 *    Destine Node�� ������ ���� Row�κ��� hash���� ��´�.
 *
 ***********************************************************************/

    void * sValue;

    sValue = (void*) mtd::valueForModule(
        (smiColumn*)aNode->dstColumn,
        aRow,
        MTD_OFFSET_USE,
        aNode->dstColumn->module->staticNull );

    return aNode->dstColumn->module->isNull( aNode->dstColumn,
                                             sValue );
}

void
qmc::makeNullNA( qmdMtrNode * /* aNode */,
                 void       * /* aRow */ )
{
    // �� �Լ��� ȣ��Ǹ� �ȵ�.

    IDE_DASSERT(0);
}

void
qmc::makeNullNothing(  qmdMtrNode * /* aNode */,
                       void       * /* aRow */)
{
/***********************************************************************
 *
 * Description :
 *    �ش� Materialized Column�� NULL Value�� �������� �ʴ´�.
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qmc::makeNullNothing"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    return;

#undef IDE_FN
}

void
qmc::makeNullValue( qmdMtrNode * aNode,
                    void       * aRow )
{
/***********************************************************************
 *
 * Description :
 *    �ش� Materialized Column�� NULL Value�������Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

    void * sValue;

    sValue = (void*) mtd::valueForModule(
        (smiColumn*)aNode->dstColumn,
        aRow,
        MTD_OFFSET_USE,
        aNode->dstColumn->module->staticNull );

    // To Fix PR-8005
    aNode->dstColumn->module->null( aNode->dstColumn,
                                    (void*) sValue );
}

void *
qmc::getRowNA ( qmdMtrNode * /* aNode */,
                const void * /* aRow  */ )
{
    // �� �Լ��� ȣ��Ǹ� �ȵ�
    IDE_DASSERT( 0 );

    return NULL;
}

void *
qmc::getRowByPointer ( qmdMtrNode * aNode,
                       const void * aRow )
{
/***********************************************************************
 *
 * Description :
 *
 *    Materialized Row�� ����� Row Pointer���� Return�Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

    SChar ** sPos = (SChar**)( (SChar*) aRow + aNode->dstColumn->column.offset);
    UInt     sTempTypeOffset;
    void   * sRow;
    mtcColumn * sColumn;

    sRow = (void*) *sPos;

    /* PROJ-2334 PMT memory variable column */
    if ( ( ( aNode->srcTuple->lflag & MTC_TUPLE_PARTITIONED_TABLE_MASK )
           == MTC_TUPLE_PARTITIONED_TABLE_TRUE )
         &&
         ( ( ( aNode->srcTuple->columns[aNode->srcNode->node.column].
               column.flag & SMI_COLUMN_TYPE_MASK )
             == SMI_COLUMN_TYPE_VARIABLE ) ||
           ( ( aNode->srcTuple->columns[aNode->srcNode->node.column].
               column.flag & SMI_COLUMN_TYPE_MASK )
             == SMI_COLUMN_TYPE_VARIABLE_LARGE ) ) )
    {
        sColumn = &aNode->srcTuple->columns[aNode->srcNode->node.column];

        IDE_DASSERT( aNode->srcColumn->column.id == sColumn->column.id );

        aNode->srcColumn = sColumn;
        
        // ���� �ƴ� Pointer�� ����Ǵ� ���
        aNode->func.compareColumn = sColumn;
    }
    else
    {
        // Nothing to do.
    }
    
    if ( ( aNode->srcColumn->column.flag & SMI_COLUMN_TYPE_MASK )
         == SMI_COLUMN_TYPE_TEMP_1B )
    {
        sTempTypeOffset = *(UChar*)((UChar*)sRow + aNode->srcColumn->column.offset);
    }
    else if ( ( aNode->srcColumn->column.flag & SMI_COLUMN_TYPE_MASK )
              == SMI_COLUMN_TYPE_TEMP_2B )
    {
        sTempTypeOffset = *(UShort*)((UChar*)sRow + aNode->srcColumn->column.offset);
    }
    else if ( ( aNode->srcColumn->column.flag & SMI_COLUMN_TYPE_MASK )
              == SMI_COLUMN_TYPE_TEMP_4B )
    {
        sTempTypeOffset = *(UInt*)((UChar*)sRow + aNode->srcColumn->column.offset);
    }
    else
    {
        sTempTypeOffset = 0;
    }

    if ( sTempTypeOffset > 0 )
    {
        sRow = (void*)((SChar*)sRow + sTempTypeOffset);

        // TEMP_TYPE�� ��� OFFSET_USELESS�� ���Ѵ�.
        // ���ʿ� setCompareFunction���� compare�Լ��� ��� �����ϴ� ���� ������
        // ���� parent�� mtrNode�� TEMP_TYPE�� �ƴϾ�����, child�� mtrNode�� ����
        // TEMP_TYPE�� ����� �� �־� ����ð����� �Ǵ��ؼ� �����Ѵ�.
        if ( ( aNode->flag & QMC_MTR_SORT_ORDER_MASK )
             == QMC_MTR_SORT_ASCENDING )
        {
            if ( aNode->func.compare !=
                 aNode->srcColumn->module->logicalCompare[MTD_COMPARE_ASCENDING] )
            {
                aNode->func.compare =
                    aNode->srcColumn->module->logicalCompare[MTD_COMPARE_ASCENDING];
            }
            else
            {
                // Nothing to do.
            }
        }
        else
        {
            if ( aNode->func.compare !=
                 aNode->srcColumn->module->logicalCompare[MTD_COMPARE_DESCENDING] )
            {
                aNode->func.compare =
                    aNode->srcColumn->module->logicalCompare[MTD_COMPARE_DESCENDING];
            }
            else
            {
                // Nothing to do.
            }
        }
    }
    else
    {
        // Nothing to do.
    }

    return sRow;
}

void *
qmc::getRowByPointerAndTupleID( qmdMtrNode * aNode,
                                const void * aRow )
{
/***********************************************************************
 *
 * Description :
 *    Materialized Row�� ����� pointer�� column�� Tuple Set�� ������Ų��.
 *    ������ ���� �뵵�� ���� ���ȴ�.
 *        - Temp Table�� ����� Memory Base Table�� ����
 *        - Memory Temp Table�� ���嵵�� Memory Column�� ����
 *
 * Implementation :
 *    Materialized Row���� ����� ��ġ�� ���ϰ�,
 *    �� ��ġ�� ����� pointer�� ���� Tuple�� ������Ų��.
 ***********************************************************************/

    qmcMemPartRowInfo   sRowInfo;
    mtdByteType       * sByte = (mtdByteType*)((SChar*)aRow + aNode->dstColumn->column.offset);

    mtcColumn  * sColumn;
    UInt         sTempTypeOffset;
    UShort       sPartitionTupleID;
    void       * sRow;
    UInt         i;

    IDE_DASSERT( ( sByte->length == ID_SIZEOF(qmcMemPartRowInfo) ) ||
                 ( sByte->length == ID_SIZEOF(qmcPartRowInfo) ) );

    // byte align�̾ �����ؾ��Ѵ�.
    idlOS::memcpy( & sRowInfo, sByte->value, ID_SIZEOF(qmcMemPartRowInfo) );

    sPartitionTupleID = sRowInfo.partitionTupleID;
    sRow = (void*)(sRowInfo.position);

    /* PROJ-2334 PMT memory variable column */
    if ( ( ( aNode->srcTuple->lflag & MTC_TUPLE_PARTITIONED_TABLE_MASK )
           == MTC_TUPLE_PARTITIONED_TABLE_TRUE )
         &&
         ( ( ( aNode->tmplate->rows[sPartitionTupleID].columns[aNode->srcNode->node.column].
               column.flag & SMI_COLUMN_TYPE_MASK )
             == SMI_COLUMN_TYPE_VARIABLE ) ||
           ( ( aNode->tmplate->rows[sPartitionTupleID].columns[aNode->srcNode->node.column].
               column.flag & SMI_COLUMN_TYPE_MASK )
             == SMI_COLUMN_TYPE_VARIABLE_LARGE ) ) )
    {
        sColumn = &aNode->tmplate->rows[sPartitionTupleID].columns[aNode->srcNode->node.column];

        IDE_DASSERT( aNode->srcColumn->column.id == sColumn->column.id );

        aNode->srcColumn = sColumn;

        // ���� �ƴ� Pointer�� ����Ǵ� ���
        aNode->func.compareColumn = sColumn;
    }
    else
    {
        // Nothing to do.
    }

    // PROJ-2362 memory temp ���� ȿ���� ����
    // baseTable�� ������ TEMP_TYPE�� ����ؾ� �Ѵ�.
    sColumn = aNode->tmplate->rows[sPartitionTupleID].columns;
    
    if ( ( aNode->flag & QMC_MTR_BASETABLE_MASK ) == QMC_MTR_BASETABLE_TRUE )
    {
        for ( i = 0; i < aNode->tmplate->rows[sPartitionTupleID].columnCount; i++, sColumn++ )
        {
            if ( ( sColumn->column.flag & SMI_COLUMN_TYPE_MASK )
                 == SMI_COLUMN_TYPE_TEMP_1B )
            {
                sTempTypeOffset = *(UChar*)((UChar*)aRow + sColumn->column.offset);
            }
            else if ( ( sColumn->column.flag & SMI_COLUMN_TYPE_MASK )
                      == SMI_COLUMN_TYPE_TEMP_2B )
            {
                sTempTypeOffset = *(UShort*)((UChar*)aRow + sColumn->column.offset);
            }
            else if ( ( sColumn->column.flag & SMI_COLUMN_TYPE_MASK )
                      == SMI_COLUMN_TYPE_TEMP_4B )
            {
                sTempTypeOffset = *(UInt*)((UChar*)aRow + sColumn->column.offset);
            }
            else
            {
                sTempTypeOffset = 0;
            }

            if ( sTempTypeOffset > 0 )
            {
                IDE_DASSERT( sColumn->module->actualSize( sColumn,
                                                          (SChar*)sRow + sTempTypeOffset )
                             <= sColumn->column.size );

                idlOS::memcpy( (SChar*)sColumn->column.value,
                               (SChar*)sRow + sTempTypeOffset,
                               sColumn->module->actualSize( sColumn,
                                                            (SChar*)sRow + sTempTypeOffset ) );
            }
            else
            {
                // Nothing to do.
            }
        }
    }
    else
    {
        // PROJ-2362 memory temp ���� ȿ���� ����
        // src�� TEMP_TYPE�� ��� ������ ����ؾ� �Ѵ�.
        if ( ( aNode->srcColumn->column.flag & SMI_COLUMN_TYPE_MASK )
             == SMI_COLUMN_TYPE_TEMP_1B )
        {
            sTempTypeOffset = *(UChar*)((UChar*)sRow + aNode->srcColumn->column.offset);
        }
        else if ( ( aNode->srcColumn->column.flag & SMI_COLUMN_TYPE_MASK )
                  == SMI_COLUMN_TYPE_TEMP_2B )
        {
            sTempTypeOffset = *(UShort*)((UChar*)sRow + aNode->srcColumn->column.offset);
        }
        else if ( ( aNode->srcColumn->column.flag & SMI_COLUMN_TYPE_MASK )
                  == SMI_COLUMN_TYPE_TEMP_4B )
        {
            sTempTypeOffset = *(UInt*)((UChar*)sRow + aNode->srcColumn->column.offset);
        }
        else
        {
            sTempTypeOffset = 0;
        }

        if ( sTempTypeOffset > 0 )
        {
            IDE_DASSERT( aNode->srcColumn->module->actualSize(
                             aNode->srcColumn,
                             (SChar*)sRow + sTempTypeOffset )
                         <= aNode->srcColumn->column.size );

            idlOS::memcpy( (SChar*)aNode->srcColumn->column.value,
                           (SChar*)sRow + sTempTypeOffset,
                           aNode->srcColumn->module->actualSize(
                               aNode->srcColumn,
                               (SChar*)sRow + sTempTypeOffset ) );
        }
        else
        {
            // Nothing to do.
        }
    }

    return sRow;
}

void *
qmc::getRowByValue ( qmdMtrNode * aNode,
                     const void * aRow )
{
/***********************************************************************
 *
 * Description :
 *    Materialized Row�� VALUE ��ü�� ����Ǿ� �����Ƿ�,
 *    ���� Row ��ü�� Return�Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

    UInt   sTempTypeOffset;
    void * sRow = (void*)aRow;

    if ( ( aNode->dstColumn->column.flag & SMI_COLUMN_TYPE_MASK )
         == SMI_COLUMN_TYPE_TEMP_1B )
    {
        sTempTypeOffset = *(UChar*)((UChar*)aRow + aNode->dstColumn->column.offset);
    }
    else if ( ( aNode->dstColumn->column.flag & SMI_COLUMN_TYPE_MASK )
              == SMI_COLUMN_TYPE_TEMP_2B )
    {
        sTempTypeOffset = *(UShort*)((UChar*)aRow + aNode->dstColumn->column.offset);
    }
    else if ( ( aNode->dstColumn->column.flag & SMI_COLUMN_TYPE_MASK )
              == SMI_COLUMN_TYPE_TEMP_4B )
    {
        sTempTypeOffset = *(UInt*)((UChar*)aRow + aNode->dstColumn->column.offset);
    }
    else
    {
        sTempTypeOffset = 0;
    }

    if ( sTempTypeOffset > 0 )
    {
        sRow = (void*)((SChar*)aRow + sTempTypeOffset);

        // TEMP_TYPE�� ��� OFFSET_USELESS�� ���Ѵ�.
        // ���ʿ� setCompareFunction���� compare�Լ��� ��� �����ϴ� ���� ������
        // ���� parent�� mtrNode�� TEMP_TYPE�� �ƴϾ�����, child�� mtrNode�� ����
        // TEMP_TYPE�� ����� �� �־� ����ð����� �Ǵ��ؼ� �����Ѵ�.
        if ( ( aNode->flag & QMC_MTR_SORT_ORDER_MASK )
             == QMC_MTR_SORT_ASCENDING )
        {
            if ( aNode->func.compare !=
                 aNode->dstColumn->module->logicalCompare[MTD_COMPARE_ASCENDING] )
            {
                aNode->func.compare =
                    aNode->dstColumn->module->logicalCompare[MTD_COMPARE_ASCENDING];
            }
            else
            {
                // Nothing to do.
            }
        }
        else
        {
            if ( aNode->func.compare !=
                 aNode->dstColumn->module->logicalCompare[MTD_COMPARE_DESCENDING] )
            {
                aNode->func.compare =
                    aNode->dstColumn->module->logicalCompare[MTD_COMPARE_DESCENDING];
            }
            else
            {
                // Nothing to do.
            }
        }
    }
    else
    {
        // Nothing to do.
    }

    return sRow;
}

IDE_RC
qmc::linkMtrNode( const qmcMtrNode * aCodeNode,
                  qmdMtrNode       * aDataNode )
{
/***********************************************************************
 *
 * Description :
 *    Data ������ Materialized Node�� �����Ѵ�.
 *
 * Implementation :
 *    Data ������ ����� Code ������ Materialize Node ������ŭ �ݺ��Ѵ�.
 *
 ***********************************************************************/

    const qmcMtrNode * sCNode;
    qmdMtrNode       * sDNode;

    for ( sCNode = aCodeNode, sDNode = aDataNode;
          sCNode != NULL;
          sCNode = sCNode->next, sDNode = sDNode->next )
    {
        IDE_ASSERT( sDNode != NULL );

        sDNode->myNode = (qmcMtrNode*)sCNode;
        sDNode->srcNode = NULL;
        sDNode->next = sDNode + 1;
        if ( sCNode->next == NULL )
        {
            sDNode->next = NULL;
        }
    }

    return IDE_SUCCESS;
}

IDE_RC
qmc::initMtrNode( qcTemplate * aTemplate,
                  qmdMtrNode * aNode,
                  UShort       aBaseTableCount )
{
/***********************************************************************
 *
 * Description :
 *    Code Materialized Node(aNode->myNode) �����κ���
 *    Data Materialized Node(aNode)�� �����Ѵ�.
 *
 * Implementation :
 *
 *
 ***********************************************************************/

    qmdMtrNode * sNode;
    UShort       sTableCount = 0;
    idBool       sBaseTable;

    for ( sNode = aNode;
          sNode != NULL;
          sNode = sNode->next )
    {
        if ( sTableCount < aBaseTableCount )
        {
            sBaseTable = ID_TRUE;
        }
        else
        {
            sBaseTable = ID_FALSE;
        }
        sTableCount++;

        IDE_TEST( qmc::setMtrNode( aTemplate,
                                   sNode,
                                   sNode->myNode,
                                   sBaseTable ) != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmc::refineOffsets( qmdMtrNode * aNode, UInt aStartOffset )
{
/***********************************************************************
 *
 * Description :
 *    Materialized Row�� �����ϴ� Column���� offset�� �������Ѵ�.
 *
 * Implementation :
 *    Materialized Row�� ������ ���� �����ȴ�.
 *       - Record Header�� ũ�� : aStartOffset
 *    �� Column�� ������ Memory/Disk Temp Table�� ���ο� ����,
 *    �� Offset�� Size�� ������ �޶�����.
 ***********************************************************************/

    qmdMtrNode * sNode;
    qmdMtrNode * sTmpNode;
    UInt         sOffset;
    UInt         sColumnCnt;
    UInt         sTempTypeColCount = 0;
    UInt         sTempTypeColSize;
    UInt         sArgument;
    SInt         sPrecision;
    SInt         sScale;
    UInt         i;

    //--------------------------------------------------
    // ������ Materialzied Column���� ��ȸ�ϸ�,
    // offset�� size���� �������Ѵ�.
    // �ʿ��� ���, Fixed->Variable�� �����ϴ� �۾���
    // �����Ѵ�.
    //--------------------------------------------------

    sOffset = aStartOffset;

    for ( sNode = aNode; sNode != NULL; sNode = sNode->next )
    {
        // BUG-46678
        // Precision�� ������� ���� return column�� size�� �������� �ʴ� ��찡 �ֽ��ϴ�.
        if ( (sNode->dstColumn->flag & MTC_COLUMN_SP_ADJUST_PRECISION_MASK) == MTC_COLUMN_SP_ADJUST_PRECISION_TRUE )
        {
            IDE_TEST( qsxUtil::finalizeParamAndReturnColumnInfo( sNode->dstColumn )
                      != IDE_SUCCESS );
        }

        if ( (sNode->srcColumn->flag & MTC_COLUMN_SP_ADJUST_PRECISION_MASK) == MTC_COLUMN_SP_ADJUST_PRECISION_TRUE )
        {
            IDE_TEST( qsxUtil::finalizeParamAndReturnColumnInfo( sNode->srcColumn )
                      != IDE_SUCCESS );
        }

        switch ( sNode->flag & QMC_MTR_TYPE_MASK )
        {
            //-----------------------------------------------------------
            // Base Table�� ���� ó�� ���
            //-----------------------------------------------------------

            case QMC_MTR_TYPE_MEMORY_TABLE :
            {
                //-----------------------------------
                // Memory Base Table�� ���
                //-----------------------------------
                // Disk Temp Table������ ó���� ���� Column��
                // BIGINT Type���� ����( 32/64bit pointer�� ����,
                // RID�� ������ ũ�� ����). RID�� ũ�Ⱑ BIGINTũ�⸦ �ʰ���
                // ���, �̿� ���� ������ �̷������ �Ѵ�.
                IDE_DASSERT( ID_SIZEOF(scGRID) <= ID_SIZEOF(mtdBigintType) );

                IDE_TEST( mtc::initializeColumn( sNode->dstColumn,
                                                 & mtdBigint,
                                                 0,
                                                 0,
                                                 0 )
                          != IDE_SUCCESS );

                // fix BUG-10243
                sOffset = idlOS::align( sOffset,
                                        sNode->dstColumn->module->align );

                sNode->dstColumn->column.offset = sOffset;

                sOffset += sNode->dstColumn->column.size;
                break;
            }
            case QMC_MTR_TYPE_DISK_TABLE :
            {
                //-----------------------------------
                // Disk Base Table�� ���
                //-----------------------------------
                // Disk Temp Table������ ó���� ����
                // Column�� BIGINT Type���� ����
                // 32/64bit pointer�� ����, RID�� ������ ũ�� ����
                // RID�� ũ�Ⱑ BIGINTũ�⸦ �ʰ��� ���,
                // �̿� ���� ������ �̷������ �Ѵ�.
                IDE_DASSERT( ID_SIZEOF(scGRID) <= ID_SIZEOF(mtdBigintType) );

                IDE_TEST( mtc::initializeColumn( sNode->dstColumn,
                                                 & mtdBigint,
                                                 0,
                                                 0,
                                                 0 )
                          != IDE_SUCCESS );

                // fix BUG-10243
                sOffset = idlOS::align( sOffset,
                                        sNode->dstColumn->module->align );

                sNode->dstColumn->column.offset = sOffset;

                sOffset += sNode->dstColumn->column.size;
                break;
            }

            // PROJ-1502 PARTITIONED DISK TABLE
            case QMC_MTR_TYPE_MEMORY_PARTITIONED_TABLE :
            {
                //-----------------------------------
                // Memory Partitioned Table�� ���
                //-----------------------------------

                // BUG-38309
                // ������ 16byte���� Ŀ�� mtdByte�� ����Ѵ�.
                IDE_TEST( mtc::initializeColumn( sNode->dstColumn,
                                                 & mtdByte,
                                                 1,
                                                 ID_SIZEOF(qmcMemPartRowInfo),
                                                 0 )
                          != IDE_SUCCESS );

                // fix BUG-10243
                sOffset = idlOS::align( sOffset,
                                        sNode->dstColumn->module->align );

                sNode->dstColumn->column.offset = sOffset;

                sOffset += sNode->dstColumn->column.size;
                break;
            }

            case QMC_MTR_TYPE_DISK_PARTITIONED_TABLE :
            {
                //-----------------------------------
                // Disk Partitioned Table�� ���
                //-----------------------------------

                // BUG-38309
                // ������ 16byte���� Ŀ�� mtdByte�� ����Ѵ�.
                // PROJ-2204 join update, delete
                // rid�Ӹ��ƴ϶� indexTuple rid�� �����ϰ� �����Ѵ�.
                IDE_TEST( mtc::initializeColumn( sNode->dstColumn,
                                                 & mtdByte,
                                                 1,
                                                 ID_SIZEOF(qmcDiskPartRowInfo),
                                                 0 )
                          != IDE_SUCCESS );

                // fix BUG-10243
                sOffset = idlOS::align( sOffset,
                                        sNode->dstColumn->module->align );

                sNode->dstColumn->column.offset = sOffset;

                sOffset += sNode->dstColumn->column.size;
                break;
            }

            case QMC_MTR_TYPE_HYBRID_PARTITIONED_TABLE : /* PROJ-2464 hybrid partitioned table ���� */
            {
                //-----------------------------------
                // Hybrid Partitioned Table�� ���
                //-----------------------------------

                // BUG-38309
                // ������ 16byte���� Ŀ�� mtdByte�� ����Ѵ�.
                // PROJ-2204 join update, delete
                // rid�Ӹ��ƴ϶� indexTuple rid�� �����ϰ� �����Ѵ�.
                IDE_TEST( mtc::initializeColumn( sNode->dstColumn,
                                                 & mtdByte,
                                                 1,
                                                 ID_SIZEOF(qmcPartRowInfo),
                                                 0 )
                          != IDE_SUCCESS );

                // fix BUG-10243
                sOffset = idlOS::align( sOffset,
                                        sNode->dstColumn->module->align );

                sNode->dstColumn->column.offset = sOffset;

                sOffset += sNode->dstColumn->column.size;
                break;
            }

            //-----------------------------------------------------------
            // Table ���� Column�� ���� ó��
            //-----------------------------------------------------------

            case QMC_MTR_TYPE_MEMORY_PARTITION_KEY_COLUMN :
            {
                //-----------------------------------
                // Memory Column�� ���
                //-----------------------------------

                IDE_TEST( refineOffset4MemoryPartitionColumn( aNode, sNode, & sOffset )
                          != IDE_SUCCESS );
                break;
            }
           
            case QMC_MTR_TYPE_MEMORY_KEY_COLUMN :
            {
                //-----------------------------------
                // Memory Column�� ���
                //-----------------------------------

                IDE_TEST( refineOffset4MemoryColumn( aNode, sNode, & sOffset )
                          != IDE_SUCCESS );
                break;
            }

            //-----------------------------------------------------------
            // Expression �� Ư�� Column�� ���� ó��
            //-----------------------------------------------------------
            case QMC_MTR_TYPE_CALCULATE:
            {
                // To Fix PR-8146
                // AVG(), STDDEV()���� ó���� ����
                // Offset ����� ���ؼ� �ش� Expression�� Column������
                // �̿��Ͽ� ����Ͽ��� �Ѵ�.

                // To Fix PR-12093
                // Destine Node�� ����Ͽ�
                // mtcColumn�� Count�� ���ϴ� ���� ��Ģ�� ����
                // ��������� ���� mtcColumn ������ �����ϴ� ���� ���ո���.
                //     - Memory ���� ����
                //     - offset ���� ���� (PR-12093)
                sColumnCnt =
                    sNode->dstNode->node.module->lflag &
                    MTC_NODE_COLUMN_COUNT_MASK;

                for ( i = 0; i < sColumnCnt; i++ )
                {
                    // �� ��ü�� ����Ǵ� ����̹Ƿ�
                    // Data Type�� �µ��� offset���� align�Ѵ�.
                    sNode->dstColumn[i].column.flag &= ~SMI_COLUMN_TYPE_MASK;
                    sNode->dstColumn[i].column.flag |= SMI_COLUMN_TYPE_FIXED;

                    // BUG-38494
                    // Compressed Column ���� �� ��ü�� ����ǹǷ�
                    // Compressed �Ӽ��� �����Ѵ�
                    sNode->dstColumn[i].column.flag &= ~SMI_COLUMN_COMPRESSION_MASK;
                    sNode->dstColumn[i].column.flag |= SMI_COLUMN_COMPRESSION_FALSE;

                    sOffset =
                        idlOS::align( sOffset,
                                      sNode->dstColumn[i].module->align );
                    sNode->dstColumn[i].column.offset = sOffset;
                    sOffset += sNode->dstColumn[i].column.size;
                }

                break;
            }

            case QMC_MTR_TYPE_COPY_VALUE:
            case QMC_MTR_TYPE_CALCULATE_AND_COPY_VALUE:
            case QMC_MTR_TYPE_HYBRID_PARTITION_KEY_COLUMN : /* PROJ-2464 hybrid partitioned table ���� */
            {
                // To Fix PR-8146
                // AVG(), STDDEV()���� ó���� ����
                // Offset ����� ���ؼ� �ش� Expression�� Column������
                // �̿��Ͽ� ����Ͽ��� �Ѵ�.

                // To Fix PR-12093
                // Destine Node�� ����Ͽ�
                // mtcColumn�� Count�� ���ϴ� ���� ��Ģ�� ����
                // ��������� ���� mtcColumn ������ �����ϴ� ���� ���ո���.
                //     - Memory ���� ����
                //     - offset ���� ���� (PR-12093)
                sColumnCnt =
                    sNode->dstNode->node.module->lflag &
                    MTC_NODE_COLUMN_COUNT_MASK;

                for ( i = 0; i < sColumnCnt; i++ )
                {
                    // �� ��ü�� ����Ǵ� ����̹Ƿ�
                    // Data Type�� �µ��� offset���� align�Ѵ�.
                    sNode->dstColumn[i].column.flag &= ~SMI_COLUMN_TYPE_MASK;
                    sNode->dstColumn[i].column.flag |= SMI_COLUMN_TYPE_FIXED;

                    // BUG-38494
                    // Compressed Column ���� �� ��ü�� ����ǹǷ�
                    // Compressed �Ӽ��� �����Ѵ�
                    sNode->dstColumn[i].column.flag &= ~SMI_COLUMN_COMPRESSION_MASK;
                    sNode->dstColumn[i].column.flag |= SMI_COLUMN_COMPRESSION_FALSE;

                    sOffset =
                        idlOS::align( sOffset,
                                      sNode->dstColumn[i].module->align );
                    sNode->dstColumn[i].column.offset = sOffset;
                    sOffset += sNode->dstColumn[i].column.size;

                    // PROJ-2362 memory temp ���� ȿ���� ����
                    if ( ( ( sNode->flag & QMC_MTR_TEMP_VAR_TYPE_ENABLE_MASK )
                           == QMC_MTR_TEMP_VAR_TYPE_ENABLE_TRUE ) &&
                         ( QMC_IS_MTR_TEMP_VAR_COLUMN( sNode->dstColumn[i] ) == ID_TRUE ) )
                    {
                        sTempTypeColCount++;
                    }
                    else
                    {
                        // Nothing to do.
                    }
                }

                break;
            }
            case QMC_MTR_TYPE_USELESS_COLUMN:
            {
                // PROJ-2469 Optimize View Materialization
                // BUG-41681
                // ���� Ÿ���� useless column�� �ִٸ� ���� ����ϰ�
                // �������� ������� �ʾ�, Materialize �� �ʿ� ����
                // View Column�� ���ؼ� null value�� �����ϵ��� �Ѵ�.
                for ( sTmpNode = aNode; sTmpNode != sNode; sTmpNode = sTmpNode->next )
                {
                    if ( ( ( sTmpNode->flag & QMC_MTR_TYPE_MASK )
                           == QMC_MTR_TYPE_USELESS_COLUMN ) &&
                         ( sTmpNode->dstColumn->module->id
                           == sNode->dstColumn->module->id ) )
                    {
                        break;
                    }
                    else
                    {
                        // Nothing to do.
                    }
                }

                if ( sTmpNode != sNode )
                {
                    mtc::initializeColumn( sNode->dstColumn,
                                           sTmpNode->dstColumn );
                        
                    sNode->dstColumn->column.offset =
                        sTmpNode->dstColumn->column.offset;
                }
                else
                {
                    // column type�� �����ϵ�, null value�� �����ϵ���
                    // column size�� ���δ�.
                    if ( ( sNode->dstColumn->module->flag & MTD_CREATE_PARAM_MASK )
                         == MTD_CREATE_PARAM_NONE )
                    {
                        sArgument  = 0;
                        sPrecision = 0;
                        sScale     = 0;
                    }
                    else
                    {
                        if ( ( sNode->dstColumn->module->flag & MTD_CREATE_PARAM_MASK )
                             == MTD_CREATE_PARAM_PRECISION )
                        {
                            sArgument = 1;
                        }
                        else
                        {
                            sArgument = 2;
                        }

                        IDE_TEST( sNode->dstColumn->module->getPrecision(
                                      sNode->dstColumn,
                                      sNode->dstColumn->module->staticNull,
                                      & sPrecision,
                                      & sScale )
                                  != IDE_SUCCESS );
                    }

                    IDE_TEST( mtc::initializeColumn( sNode->dstColumn,
                                                     sNode->dstColumn->module,
                                                     sArgument,
                                                     sPrecision,
                                                     sScale )
                              != IDE_SUCCESS );
                        
                    IDE_DASSERT( sNode->dstColumn->column.size >=
                                 sNode->dstColumn->module->actualSize(
                                     sNode->dstColumn,
                                     sNode->dstColumn->module->staticNull ) );
                    
                    sOffset = idlOS::align( sOffset,
                                            sNode->dstColumn->module->align );
                    
                    sNode->dstColumn->column.offset = sOffset;

                    sOffset += sNode->dstColumn->column.size;
                }
                break;
            }
            default :
            {
                // Materialized Column�� ������ �����Ǿ� �־�� ��.
                IDE_DASSERT(0);

                break;
            }
        } // end of switch

        //----------------------------------------------------
        // Column�� ����� ��ü�� ����
        //     - Memory Temp Table�� ���
        //     - Disk Temp Table�� ���
        //----------------------------------------------------

        if ( (sNode->dstTuple->lflag & MTC_TUPLE_STORAGE_MASK)
             == MTC_TUPLE_STORAGE_MEMORY )
        {
            sNode->dstColumn->column.flag &= ~SMI_COLUMN_STORAGE_MASK;
            sNode->dstColumn->column.flag |= SMI_COLUMN_STORAGE_MEMORY;
        }
        else
        {
            sNode->dstColumn->column.flag &= ~SMI_COLUMN_STORAGE_MASK;
            sNode->dstColumn->column.flag |= SMI_COLUMN_STORAGE_DISK;
        }
    }

    //-----------------------------------------------------
    // PROJ-2362 memory temp ���� ȿ���� ����
    // TEMP_TYPE�� ����� �÷� ũ���� ���
    //-----------------------------------------------------

    if ( sTempTypeColCount > 0 )
    {
        if ( sOffset + sTempTypeColCount < 256 )
        {
            // 1 byte offset
            sTempTypeColSize = 1;
        }
        else if ( sOffset + ( sTempTypeColCount + 1 ) * 2 < 65536 ) // 2byte align�� ����Ѵ�.
        {
            // 2 byte offset
            sTempTypeColSize = 2;
        }
        else
        {
            // 4 byte offset
            sTempTypeColSize = 4;
        }

        //--------------------------------------------------
        // ������ Materialzied Column���� ��ȸ�ϸ�,
        // offset�� size���� �������Ѵ�.
        // �ʿ��� ���, Fixed->Variable�� �����ϴ� �۾���
        // �����Ѵ�.
        //--------------------------------------------------

        sOffset = aStartOffset;

        for ( sNode = aNode; sNode != NULL; sNode = sNode->next )
        {
            switch ( sNode->flag & QMC_MTR_TYPE_MASK )
            {
                //-----------------------------------------------------------
                // Base Table�� ���� ó�� ���
                //-----------------------------------------------------------

                case QMC_MTR_TYPE_MEMORY_TABLE :
                {
                    //-----------------------------------
                    // Memory Base Table�� ���
                    //-----------------------------------
                    // Disk Temp Table������ ó���� ���� Column��
                    // BIGINT Type���� ����( 32/64bit pointer�� ����,
                    // RID�� ������ ũ�� ����). RID�� ũ�Ⱑ BIGINTũ�⸦ �ʰ���
                    // ���, �̿� ���� ������ �̷������ �Ѵ�.
                    IDE_DASSERT( ID_SIZEOF(scGRID) <= ID_SIZEOF(mtdBigintType) );

                    IDE_TEST( mtc::initializeColumn( sNode->dstColumn,
                                                     & mtdBigint,
                                                     0,
                                                     0,
                                                     0 )
                              != IDE_SUCCESS );

                    // fix BUG-10243
                    sOffset = idlOS::align( sOffset,
                                            sNode->dstColumn->module->align );

                    sNode->dstColumn->column.offset = sOffset;

                    sOffset += sNode->dstColumn->column.size;
                    break;
                }
                case QMC_MTR_TYPE_DISK_TABLE :
                {
                    //-----------------------------------
                    // Disk Base Table�� ���
                    //-----------------------------------
                    // Disk Temp Table������ ó���� ����
                    // Column�� BIGINT Type���� ����
                    // 32/64bit pointer�� ����, RID�� ������ ũ�� ����
                    // RID�� ũ�Ⱑ BIGINTũ�⸦ �ʰ��� ���,
                    // �̿� ���� ������ �̷������ �Ѵ�.
                    IDE_DASSERT( ID_SIZEOF(scGRID) <= ID_SIZEOF(mtdBigintType) );

                    IDE_TEST( mtc::initializeColumn( sNode->dstColumn,
                                                     & mtdBigint,
                                                     0,
                                                     0,
                                                     0 )
                              != IDE_SUCCESS );

                    // fix BUG-10243
                    sOffset = idlOS::align( sOffset,
                                            sNode->dstColumn->module->align );

                    sNode->dstColumn->column.offset = sOffset;

                    sOffset += sNode->dstColumn->column.size;
                    break;
                }

                // PROJ-1502 PARTITIONED DISK TABLE
                case QMC_MTR_TYPE_MEMORY_PARTITIONED_TABLE :
                {
                    //-----------------------------------
                    // Memory Partitioned Table�� ���
                    //-----------------------------------

                    // BUG-38309
                    // ������ 16byte���� Ŀ�� mtdByte�� ����Ѵ�.
                    IDE_TEST( mtc::initializeColumn( sNode->dstColumn,
                                                     & mtdByte,
                                                     1,
                                                     ID_SIZEOF(qmcMemPartRowInfo),
                                                     0 )
                              != IDE_SUCCESS );

                    // fix BUG-10243
                    sOffset = idlOS::align( sOffset,
                                            sNode->dstColumn->module->align );

                    sNode->dstColumn->column.offset = sOffset;

                    sOffset += sNode->dstColumn->column.size;
                    break;
                }

                case QMC_MTR_TYPE_DISK_PARTITIONED_TABLE :
                {
                    //-----------------------------------
                    // Disk Partitioned Table�� ���
                    //-----------------------------------

                    // BUG-38309
                    // ������ 16byte���� Ŀ�� mtdByte�� ����Ѵ�.
                    // PROJ-2204 join update, delete
                    // rid�Ӹ��ƴ϶� indexTuple rid�� �����ϰ� �����Ѵ�.
                    IDE_TEST( mtc::initializeColumn( sNode->dstColumn,
                                                     & mtdByte,
                                                     1,
                                                     ID_SIZEOF(qmcDiskPartRowInfo),
                                                     0 )
                              != IDE_SUCCESS );

                    // fix BUG-10243
                    sOffset = idlOS::align( sOffset,
                                            sNode->dstColumn->module->align );

                    sNode->dstColumn->column.offset = sOffset;

                    sOffset += sNode->dstColumn->column.size;
                    break;
                }

                case QMC_MTR_TYPE_HYBRID_PARTITIONED_TABLE : /* PROJ-2464 hybrid partitioned table ���� */
                {
                    //-----------------------------------
                    // Hybrid Partitioned Table�� ���
                    //-----------------------------------

                    // BUG-38309
                    // ������ 16byte���� Ŀ�� mtdByte�� ����Ѵ�.
                    // PROJ-2204 join update, delete
                    // rid�Ӹ��ƴ϶� indexTuple rid�� �����ϰ� �����Ѵ�.
                    IDE_TEST( mtc::initializeColumn( sNode->dstColumn,
                                                     & mtdByte,
                                                     1,
                                                     ID_SIZEOF(qmcPartRowInfo),
                                                     0 )
                              != IDE_SUCCESS );

                    // fix BUG-10243
                    sOffset = idlOS::align( sOffset,
                                            sNode->dstColumn->module->align );

                    sNode->dstColumn->column.offset = sOffset;

                    sOffset += sNode->dstColumn->column.size;
                    break;
                }

                //-----------------------------------------------------------
                // Table ���� Column�� ���� ó��
                //-----------------------------------------------------------

                case QMC_MTR_TYPE_MEMORY_PARTITION_KEY_COLUMN :
                {
                    //-----------------------------------
                    // Memory Column�� ���
                    //-----------------------------------

                    IDE_TEST( refineOffset4MemoryPartitionColumn( aNode, sNode, & sOffset )
                              != IDE_SUCCESS );
                    break;
                }

                case QMC_MTR_TYPE_MEMORY_KEY_COLUMN :
                {
                    //-----------------------------------
                    // Memory Column�� ���
                    //-----------------------------------

                    IDE_TEST( refineOffset4MemoryColumn( aNode, sNode, & sOffset )
                              != IDE_SUCCESS );
                    break;
                }

                //-----------------------------------------------------------
                // Expression �� Ư�� Column�� ���� ó��
                //-----------------------------------------------------------
                case QMC_MTR_TYPE_CALCULATE:
                {
                    // To Fix PR-8146
                    // AVG(), STDDEV()���� ó���� ����
                    // Offset ����� ���ؼ� �ش� Expression�� Column������
                    // �̿��Ͽ� ����Ͽ��� �Ѵ�.

                    // To Fix PR-12093
                    // Destine Node�� ����Ͽ�
                    // mtcColumn�� Count�� ���ϴ� ���� ��Ģ�� ����
                    // ��������� ���� mtcColumn ������ �����ϴ� ���� ���ո���.
                    //     - Memory ���� ����
                    //     - offset ���� ���� (PR-12093)
                    sColumnCnt =
                        sNode->dstNode->node.module->lflag &
                        MTC_NODE_COLUMN_COUNT_MASK;

                    for ( i = 0; i < sColumnCnt; i++ )
                    {
                        // �� ��ü�� ����Ǵ� ����̹Ƿ�
                        // Data Type�� �µ��� offset���� align�Ѵ�.
                        sNode->dstColumn[i].column.flag &= ~SMI_COLUMN_TYPE_MASK;
                        sNode->dstColumn[i].column.flag |= SMI_COLUMN_TYPE_FIXED;

                        // BUG-38494
                        // Compressed Column ���� �� ��ü�� ����ǹǷ�
                        // Compressed �Ӽ��� �����Ѵ�
                        sNode->dstColumn[i].column.flag &= ~SMI_COLUMN_COMPRESSION_MASK;
                        sNode->dstColumn[i].column.flag |= SMI_COLUMN_COMPRESSION_FALSE;

                        sOffset =
                            idlOS::align( sOffset,
                                          sNode->dstColumn[i].module->align );
                        sNode->dstColumn[i].column.offset = sOffset;
                        sOffset += sNode->dstColumn[i].column.size;
                    }

                    break;
                }

                case QMC_MTR_TYPE_COPY_VALUE:
                case QMC_MTR_TYPE_CALCULATE_AND_COPY_VALUE:
                case QMC_MTR_TYPE_HYBRID_PARTITION_KEY_COLUMN : /* PROJ-2464 hybrid partitioned table ���� */
                {
                    // To Fix PR-8146
                    // AVG(), STDDEV()���� ó���� ����
                    // Offset ����� ���ؼ� �ش� Expression�� Column������
                    // �̿��Ͽ� ����Ͽ��� �Ѵ�.

                    // To Fix PR-12093
                    // Destine Node�� ����Ͽ�
                    // mtcColumn�� Count�� ���ϴ� ���� ��Ģ�� ����
                    // ��������� ���� mtcColumn ������ �����ϴ� ���� ���ո���.
                    //     - Memory ���� ����
                    //     - offset ���� ���� (PR-12093)
                    sColumnCnt =
                        sNode->dstNode->node.module->lflag &
                        MTC_NODE_COLUMN_COUNT_MASK;

                    for ( i = 0; i < sColumnCnt; i++ )
                    {
                        // PROJ-2362 memory temp ���� ȿ���� ����
                        if ( ( ( sNode->flag & QMC_MTR_TEMP_VAR_TYPE_ENABLE_MASK )
                               == QMC_MTR_TEMP_VAR_TYPE_ENABLE_TRUE ) &&
                             ( QMC_IS_MTR_TEMP_VAR_COLUMN( sNode->dstColumn[i] ) == ID_TRUE ) )
                        {
                            // TEMP_TYPE���� �����Ѵ�.
                            if ( sTempTypeColSize == 1 )
                            {
                                sNode->dstColumn[i].column.flag &= ~SMI_COLUMN_TYPE_MASK;
                                sNode->dstColumn[i].column.flag |= SMI_COLUMN_TYPE_TEMP_1B;
                            }
                            else if ( sTempTypeColSize == 2 )
                            {
                                sNode->dstColumn[i].column.flag &= ~SMI_COLUMN_TYPE_MASK;
                                sNode->dstColumn[i].column.flag |= SMI_COLUMN_TYPE_TEMP_2B;
                            }
                            else
                            {
                                sNode->dstColumn[i].column.flag &= ~SMI_COLUMN_TYPE_MASK;
                                sNode->dstColumn[i].column.flag |= SMI_COLUMN_TYPE_TEMP_4B;
                            }

                            sOffset =
                                idlOS::align( sOffset,
                                              sTempTypeColSize );
                            sNode->dstColumn[i].column.offset = sOffset;
                            sOffset += sTempTypeColSize;
                        }
                        else
                        {
                            // �� ��ü�� ����Ǵ� ����̹Ƿ�
                            // Data Type�� �µ��� offset���� align�Ѵ�.
                            sNode->dstColumn[i].column.flag &= ~SMI_COLUMN_TYPE_MASK;
                            sNode->dstColumn[i].column.flag |= SMI_COLUMN_TYPE_FIXED;

                            // BUG-38494
                            // Compressed Column ���� �� ��ü�� ����ǹǷ�
                            // Compressed �Ӽ��� �����Ѵ�
                            sNode->dstColumn[i].column.flag &= ~SMI_COLUMN_COMPRESSION_MASK;
                            sNode->dstColumn[i].column.flag |= SMI_COLUMN_COMPRESSION_FALSE;

                            sOffset =
                                idlOS::align( sOffset,
                                              sNode->dstColumn[i].module->align );
                            sNode->dstColumn[i].column.offset = sOffset;
                            sOffset += sNode->dstColumn[i].column.size;
                        }
                    }

                    break;
                }
                case QMC_MTR_TYPE_USELESS_COLUMN:
                {
                    // PROJ-2469 Optimize View Materialization
                    // BUG-41681
                    // ���� Ÿ���� useless column�� �ִٸ� ���� ����ϰ�
                    // �������� ������� �ʾ�, Materialize �� �ʿ� ����
                    // View Column�� ���ؼ� null value�� �����ϵ��� �Ѵ�.
                    for ( sTmpNode = aNode; sTmpNode != sNode; sTmpNode = sTmpNode->next )
                    {
                        if ( ( ( sTmpNode->flag & QMC_MTR_TYPE_MASK )
                               == QMC_MTR_TYPE_USELESS_COLUMN ) &&
                             ( sTmpNode->dstColumn->module->id
                               == sNode->dstColumn->module->id ) )
                        {
                            break;
                        }
                        else
                        {
                            // Nothing to do.
                        }
                    }

                    if ( sTmpNode != sNode )
                    {
                        mtc::initializeColumn( sNode->dstColumn,
                                               sTmpNode->dstColumn );
                        
                        sNode->dstColumn->column.offset =
                            sTmpNode->dstColumn->column.offset;
                    }
                    else
                    {
                        // column type�� �����ϵ�, null value�� �����ϵ���
                        // column size�� ���δ�.
                        if ( ( sNode->dstColumn->module->flag & MTD_CREATE_PARAM_MASK )
                             == MTD_CREATE_PARAM_NONE )
                        {
                            sArgument  = 0;
                            sPrecision = 0;
                            sScale     = 0;
                        }
                        else
                        {
                            if ( ( sNode->dstColumn->module->flag & MTD_CREATE_PARAM_MASK )
                                 == MTD_CREATE_PARAM_PRECISION )
                            {
                                sArgument = 1;
                            }
                            else
                            {
                                sArgument = 2;
                            }

                            IDE_TEST( sNode->dstColumn->module->getPrecision(
                                          sNode->dstColumn,
                                          sNode->dstColumn->module->staticNull,
                                          & sPrecision,
                                          & sScale )
                                      != IDE_SUCCESS );
                        }

                        IDE_TEST( mtc::initializeColumn( sNode->dstColumn,
                                                         sNode->dstColumn->module,
                                                         sArgument,
                                                         sPrecision,
                                                         sScale )
                                  != IDE_SUCCESS );
                        
                        IDE_DASSERT( sNode->dstColumn->column.size >=
                                     sNode->dstColumn->module->actualSize(
                                         sNode->dstColumn,
                                         sNode->dstColumn->module->staticNull ) );
                        
                        sOffset = idlOS::align( sOffset,
                                                sNode->dstColumn->module->align );
                        
                        sNode->dstColumn->column.offset = sOffset;
                        
                        sOffset += sNode->dstColumn->column.size;
                    }
                    break;                
                }                   
                default :
                {
                    // Materialized Column�� ������ �����Ǿ� �־�� ��.
                    IDE_DASSERT(0);

                    break;
                }
            } // end of switch
        }
    }
    else
    {
        // Nothing to do.
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 *
 * Description :
 *    �ش� Tuple�� Fixed Record Size�� ����Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/
UInt qmc::getRowOffsetForTuple( mtcTemplate * aTemplate,
                                UShort        aTupleID )
{

    mtcColumn * sColumn;
    UInt        sFixedRecordSize;
    UShort      i;

    //-----------------------------------------------------
    // Fixed Record�� Size ���
    //-----------------------------------------------------

    // To Fix PR-8200
    // ���� ������ ũ�� ���� ������ ���� ������ ����Ͽ��� �Ѵ�.
    // ���� ��� ������ ���� ���� �÷��� ������ ��
    // [T1]-->[T2]-->[T3]-->[T3.i1]-->[T2.i1]-->[T3.i1]
    // ������ offset�� ������ ���� �����ȴ�.
    //  0  -->  8 --> 16 -->  16   -->  8    -->   0
    // �̴� ::refineOffsets()�� ���� �޸� �÷��� ������
    // ���� ������ Ȯ������ �ʱ� �����̴�.
    // ����, ���� Offset�� ū �÷��� �������� Fixed Record��
    // Size�� ����Ͽ��� �Ѵ�.

    // ���� Offset�� ū Column�� ȹ��
    sColumn = aTemplate->rows[aTupleID].columns;
    for ( i = 1; i < aTemplate->rows[aTupleID].columnCount; i++ )
    {
        /* PROJ-2419 �ݿ� �������� ��� Column�� Offset�� �޶�����,
         * PROJ-2419 �ݿ� ���Ŀ��� Offset�� 0�� Column�� ���� ���� �� �ִ�.
         * �׷���, Fixed Column/Large Variable Column�� ���,
         * Fixed Slot Header Size(32) ������ Offset�� 32 �̻��̹Ƿ� ������ ����.
         */
        if ( aTemplate->rows[aTupleID].columns[i].column.offset
             > sColumn->column.offset )
        {
            sColumn = & aTemplate->rows[aTupleID].columns[i];
        }
        else
        {
            // Nothing To Do
        }
    }

    // PROJ-2264 Dictionary table
    if( (sColumn->column.flag & SMI_COLUMN_COMPRESSION_MASK)
        == SMI_COLUMN_COMPRESSION_FALSE )
    {
        if ( (sColumn->column.flag & SMI_COLUMN_TYPE_MASK)
             == SMI_COLUMN_TYPE_VARIABLE )
        {
            // Disk Variable Column�� ���� ����.
            IDE_ASSERT( ( sColumn->column.flag & SMI_COLUMN_STORAGE_MASK )
                        == SMI_COLUMN_STORAGE_MEMORY );

            // ������ Column�� Variable Column�� ���
            // Variable Column�� Header Size�� ���
            /* Fixed Slot�� Column�� ���� ���, Fixed Slot Header Size ��ŭ�� ������ �ʿ��ϴ�.
             * Variable Slot�� OID�� Fixed Slot Header�� �����Ѵ�.
             */
            sFixedRecordSize = smiGetRowHeaderSize( SMI_TABLE_MEMORY );
        }
        else if ( (sColumn->column.flag & SMI_COLUMN_TYPE_MASK)
                  == SMI_COLUMN_TYPE_VARIABLE_LARGE )
        {
            // Disk Variable Column�� ���� ����.
            IDE_ASSERT( ( sColumn->column.flag & SMI_COLUMN_STORAGE_MASK )
                        == SMI_COLUMN_STORAGE_MEMORY );

            // ������ Column�� Variable Column�� ���
            // Variable Column�� Header Size�� ���
            // Large Variable Column�� ���
            sFixedRecordSize = sColumn->column.offset +
                IDL_MAX( smiGetVariableColumnSize( SMI_TABLE_MEMORY ),
                         smiGetVCDescInModeSize() + sColumn->column.vcInOutBaseSize );
        }
        else if ( (sColumn->column.flag & SMI_COLUMN_TYPE_MASK)
                  == SMI_COLUMN_TYPE_LOB )
        {
            // PROJ-1362
            if ( (sColumn->column.flag & SMI_COLUMN_STORAGE_MASK)
                 == SMI_COLUMN_STORAGE_MEMORY )
            {
                sFixedRecordSize = sColumn->column.offset +
                    IDL_MAX( smiGetLobColumnSize( SMI_TABLE_MEMORY ),
                             smiGetVCDescInModeSize() + sColumn->column.vcInOutBaseSize );
            }
            else
            {
                sFixedRecordSize = sColumn->column.offset +
                    smiGetLobColumnSize( SMI_TABLE_DISK );
            }
        }
        // PROJ-2362 memory temp ���� ȿ���� ����
        else if ( (sColumn->column.flag & SMI_COLUMN_TYPE_MASK)
                  == SMI_COLUMN_TYPE_TEMP_1B )
        {
            sFixedRecordSize = sColumn->column.offset + 1;
        }
        else if ( (sColumn->column.flag & SMI_COLUMN_TYPE_MASK)
                  == SMI_COLUMN_TYPE_TEMP_2B )
        {
            sFixedRecordSize = sColumn->column.offset + 2;
        }
        else if ( (sColumn->column.flag & SMI_COLUMN_TYPE_MASK)
                  == SMI_COLUMN_TYPE_TEMP_4B )
        {
            sFixedRecordSize = sColumn->column.offset + 4;
        }
        else
        {
            // ������ Column�� Fixed Column�� ���
            sFixedRecordSize = sColumn->column.offset + sColumn->column.size;
        }
    }
    else
    {
        // PROJ-2264 Dictionary table
        sFixedRecordSize = sColumn->column.offset + ID_SIZEOF( smOID );
    }

    sFixedRecordSize = idlOS::align8( sFixedRecordSize );

    return sFixedRecordSize;
}

IDE_RC qmc::setRowSize( iduMemory  * aMemory,
                        mtcTemplate* aTemplate,
                        UShort       aTupleID )
{
/***********************************************************************
 *
 * Description :
 *    �ش� Tuple�� Record Size�� ����ϰ�, Memory�� �Ҵ���.
 *    Disk Table, Disk Temp Table, Memory Temp Table�� ���Ͽ� �����.
 *
 * Implementation :
 *
 *    - Fixed Record�� Size�� ���Ѵ�.
 *        - Offset�� ���� ū Column�� ����(Fixed/Variable)�� ����,
 *          Record ũ�⸦ �����Ѵ�.
 *    - Disk Table(Disk Temp Table)�� ���
 *        - Variable Column���� ���� ������ ũ�⸦ ����Ѵ�.
 *        - �� Variable Column�� ������ Variable Column
 *    - ũ�⿡ �´� Memory�� �Ҵ���.
 *    - Disk Table(Disk Temp Table)�� ���
 *        - Variable Column�� ���� value pointer�� �����Ѵ�.
 *
 ***********************************************************************/

    mtcColumn * sColumn             = NULL;
    UInt        sVariableRecordSize = 0;
    UInt        sVarColumnOffset    = 0;
    SChar     * sColumnBuffer       = NULL;
    UShort      i                   = 0;

    //-----------------------------------------------------
    // Memory Table�� ��� Variable Column�� ����
    // ������ ũ�� ���
    //-----------------------------------------------------

    sVariableRecordSize = 0;

    if ( ( aTemplate->rows[aTupleID].lflag & MTC_TUPLE_STORAGE_MASK)
         == MTC_TUPLE_STORAGE_DISK )
    {
        // Nothing To Do
    }
    else
    {
        /* PROJ-2160 */
        if( (aTemplate->rows[aTupleID].lflag & MTC_TUPLE_ROW_GEOMETRY_MASK)
            == MTC_TUPLE_ROW_GEOMETRY_TRUE )
        {
            //PROJ-1583 large geometry
            for ( i = 0; i < aTemplate->rows[aTupleID].columnCount; i++ )
            {
                sColumn = aTemplate->rows[aTupleID].columns + i;

                // To fix BUG-24356
                // geometry�� ���ؼ��� value buffer�Ҵ�
                if ( ( (sColumn->column.flag & SMI_COLUMN_TYPE_MASK)
                       == SMI_COLUMN_TYPE_VARIABLE_LARGE ) &&
                     (sColumn->module->id == MTD_GEOMETRY_ID) )
                {
                    sVariableRecordSize = idlOS::align8( sVariableRecordSize );
                    sVariableRecordSize = sVariableRecordSize +
                        smiGetVariableColumnSize(SMI_TABLE_MEMORY) +
                        sColumn->column.size;
                }
                else
                {
                    // Nothing To Do
                }
            }
        }
        else
        {
            // Nothing To Do
        }

        // Memory Table(Temp Table)�� ���,
        // Variable Column�� ���� ������ ������ �ʿ� ����.
        // Memory Temp Table�� Disk Variable Column => Fixed Column���� ó��
        // Disk Temp Table�� Memory Variable Column => Fixed Column���� ó��

        // PROJ-2362 memory temp ���� ȿ���� ����
        // disk column�� memory temp�� ���̴� ��� variable length type�� ���ؼ�
        // SMI_COLUMN_TEMP_TYPE���� �����Ѵ�.
        for ( i = 0; i < aTemplate->rows[aTupleID].columnCount; i++ )
        {
            sColumn = aTemplate->rows[aTupleID].columns + i;

            if ( SMI_COLUMN_TYPE_IS_TEMP( sColumn->column.flag ) == ID_TRUE )
            {
                sVariableRecordSize = idlOS::align( sVariableRecordSize,
                                                    sColumn->module->align );
                sVariableRecordSize = sVariableRecordSize +
                    sColumn->column.size;
            }
            else
            {
                // Nothing To Do
            }
        }

        sVariableRecordSize = idlOS::align8( sVariableRecordSize );

        // BUG-10858
        aTemplate->rows[aTupleID].row = NULL;
    }

    //-----------------------------------------------------
    // Record Size�� Setting
    //-----------------------------------------------------

    aTemplate->rows[aTupleID].rowOffset = qmc::getRowOffsetForTuple( aTemplate, aTupleID );

    //-----------------------------------------------------
    // ���� �Ҵ� �� Variable Column Value Pointer Setting
    //-----------------------------------------------------

    if ( ( aTemplate->rows[aTupleID].lflag & MTC_TUPLE_STORAGE_MASK)
         == MTC_TUPLE_STORAGE_DISK )
    {
        // ��ũ���̺� ���� ���� �Ҵ�
        IDU_LIMITPOINT("qmc::setRowSize::malloc2");
        IDE_TEST( aMemory->cralloc( aTemplate->rows[aTupleID].rowOffset,
                                    & aTemplate->rows[aTupleID].row )
                  != IDE_SUCCESS );
    }
    else
    {
        // PROJ-1583 large geometry
        // ���� �Ҵ� �� Variable Column Value Pointer Setting

        // ���� �Ҵ�
        // To Fix PR-8561
        // Variable Column�� RID���� ������ ��� �ֱ� ������
        // �ʱ�ȭ�Ͽ��� ��.
        if( sVariableRecordSize > 0 )
        {
            IDU_LIMITPOINT("qmc::setRowSize::malloc3");
            IDE_TEST( aMemory->cralloc( sVariableRecordSize,
                                        (void **)&sColumnBuffer )
                      != IDE_SUCCESS );

            sVarColumnOffset = 0;

            // Memory Variable Column�� �Ҵ�� ������ Memory�� Setting��.
            for ( i = 0; i < aTemplate->rows[aTupleID].columnCount; i++ )
            {
                sColumn = aTemplate->rows[aTupleID].columns + i;

                // To fix BUG-24356
                // geometry�� ���ؼ��� value buffer�Ҵ�
                if ( ( (sColumn->column.flag & SMI_COLUMN_TYPE_MASK)
                       == SMI_COLUMN_TYPE_VARIABLE_LARGE ) &&
                     (sColumn->module->id == MTD_GEOMETRY_ID) )
                {
                    sVarColumnOffset = idlOS::align8( sVarColumnOffset );
                    sColumn->column.value =
                        (void *)(sColumnBuffer + sVarColumnOffset);
                    sVarColumnOffset = sVarColumnOffset +
                        smiGetVariableColumnSize(SMI_TABLE_MEMORY) +
                        sColumn->column.size;
                }
                else
                {
                    // Nothing To Do
                }
            }

            // PROJ-2362 memory temp ���� ȿ���� ����
            for ( i = 0; i < aTemplate->rows[aTupleID].columnCount; i++ )
            {
                sColumn = aTemplate->rows[aTupleID].columns + i;

                if ( SMI_COLUMN_TYPE_IS_TEMP( sColumn->column.flag ) == ID_TRUE )
                {
                    sVarColumnOffset = idlOS::align( sVarColumnOffset,
                                                     sColumn->module->align );
                    sColumn->column.value =
                        (void *)(sColumnBuffer + sVarColumnOffset);
                    sVarColumnOffset = sVarColumnOffset +
                        sColumn->column.size;
                }
                else
                {
                    // Nothing To Do
                }
            }
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

UInt
qmc::getMtrRowSize( qmdMtrNode * aNode )
{
/***********************************************************************
 *
 * Description :
 *    Materialized Row�� ���� ũ�⸦ ���Ѵ�.
 *    ::setRowSize()�Ŀ� ȣ��Ǿ�� �Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qmc::getMtrRowSize"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    return aNode->dstTuple->rowOffset;

#undef IDE_FN
}

IDE_RC
qmc::setMtrNode( qcTemplate * aTemplate,
                 qmdMtrNode * aDataNode,
                 qmcMtrNode * aCodeNode,
                 idBool       aBaseTable )
{
/***********************************************************************
 *
 * Description :
 *    Code ������ Materialized Node ����(aCodeNode)�κ���
 *    Data ������ Materialized Node ����(aDataNode)�� �����Ѵ�.
 *    �� ��, Destination Node������ Column���� �� Execute ������ �����Ѵ�.
 *
 * Implementation :
 *                 destine node
 *                  ^
 *    qmcMtrNode----|
 *                  V
 *                 source node
 *
 *    ���� ���� ������ ���� qmdMtrNode���� node ���� �� �ƴ϶�,
 *    Source�� Destine�� Column ���� �� Execute ������ ������ �����Ѵ�.
 *    ����, source node�� ������ �̿��Ͽ� destine node�� column���� ��
 *    exectue ������ �� �������� �����ȴ�.
 *
 ***********************************************************************/

    qtcNode    * sSrcNode;
    mtcColumn  * sColumn;
    mtcExecute * sExecute;
    UInt         sColumnCnt;

    //----------------------------------------------------
    // Base Table ���ο� ����,
    // ��Ȯ�� Source Node�� �����Ѵ�.
    //----------------------------------------------------

    if ( aBaseTable == ID_TRUE )
    {
        // To Fix PR-11567
        // Base Table �߿��� Pass Node�� ������ �� �ִ�.
        // Ex) SELECT /+ DISTINCT SORT +/
        //     DISTINCT i1 + 1, i2 FROM T1 ORDER BY (i1 + 1) % 2, i2;
        // Pass Node�� ��� Size�� ���� Column ������ ���� ������
        // �̸� �̿��Ͽ� destination column ������ �����ϸ� �ȵȴ�.

        // To FIX-PR-20820
        // Base Table�� SubQuery �� ��쵵 �ִ�.
        // Order by �� ���� Column�� Group By �� ������ Column �Ǵ� Aggregation Function ���� �ƴ� ���� �ִ�.
        // Ex) SELECT
        //         ( SELECT s1 FROM t1 )
        //     FROM t1
        //        GROUP BY s1, s2
        //        ORDER BY s2;
        if ( QTC_IS_SUBQUERY( aCodeNode->srcNode ) == ID_TRUE )
        {
            sSrcNode = (qtcNode*)mtf::convertedNode( &aCodeNode->srcNode->node,
                                                     &aTemplate->tmplate );
        }
        else
        {
            // Aggregation Column�� ��쵵 Conversion�� ����
            // ������� �ʵ��� Base Table�� ó���Ǿ�� �Ѵ�.
            sSrcNode = aCodeNode->srcNode;
        }
    }
    else
    {
        // Base Table�� �ƴ϶��,
        // ���� ���� ����� Node�� �߽����� �� ������ �����Ͽ��� �Ѵ�.
        sSrcNode = (qtcNode*)mtf::convertedNode( &aCodeNode->srcNode->node,
                                                 &aTemplate->tmplate );
    }

    //------------------------------------------------------
    // Data Materialized Node �� �⺻ ���� Setting
    //    - flag ���� ����
    //    - Destination ������ ���� Setting
    //        - dstNode : destination Node
    //        - dstTuple : destination node�� tuple ��ġ
    //        - dstColumn : destination node�� column ��ġ
    //    - Source Node ������ ���� Setting
    //        - srcNode :
    //        - srcTuple
    //        - srcColumn
    //------------------------------------------------------

    // Destie Node�� ���� Setting
    aDataNode->flag = aCodeNode->flag;
    aDataNode->dstNode = aCodeNode->dstNode;
    aDataNode->dstTuple =
        & aTemplate->tmplate.rows[aCodeNode->dstNode->node.table];
    aDataNode->dstColumn =
        QTC_TUPLE_COLUMN(aDataNode->dstTuple, aCodeNode->dstNode);

    // Source Node�� ���� Setting
    aDataNode->srcNode = aCodeNode->srcNode;
    aDataNode->srcTuple =
        & aTemplate->tmplate.rows[aCodeNode->srcNode->node.table];
    aDataNode->srcColumn =
        QTC_TUPLE_COLUMN(aDataNode->srcTuple, aCodeNode->srcNode);

    //--------------------------------------------------------
    // Destination Node������ �ش��ϴ� Column ���� �� Execute
    // ������ �����Ѵ�.
    // �� ��, ���� Source Node�� �ƴ�
    // ���� ����� �Ǵ� Source Node�� Column ������
    // Execute ������ �����Ѵ�.
    //
    // ����) Host ������ ���� �����, Destination Node�� ������
    //       �̹� �����Ǿ� �ֱ⵵ ������, ������ ����� �����ϱ�
    //       ������ ū ������ ���� �ʴ´�.
    //--------------------------------------------------------

    // ������ Source Node�� Column ���� �� Execute ���� ȹ��
    sColumn  = QTC_TMPL_COLUMN(aTemplate, sSrcNode);
    sExecute = QTC_TMPL_EXECUTE(aTemplate, sSrcNode);

    // To Fix PR-8146
    // Destine Node�� �ƴ� Source Node�� ����Ͽ��� ��.
    // To Fix PR-12093
    // Destine Node�� ����Ͽ� mtcColumn�� Count�� ���ϴ� ���� ��Ģ�� ����
    // ��������� ���� mtcColumn ������ �����ϴ� ���� ���ո���.
    //     - Memory ���� ����
    //     - offset ���� ���� (PR-12093)
    sColumnCnt =
        aDataNode->dstNode->node.module->lflag & MTC_NODE_COLUMN_COUNT_MASK;

    // Column ������ ����
    if( aDataNode->dstColumn != sColumn )
    {
        // mtc::copyColumn()�� ������� �ʴ´�.
        // Variable�� Variable �״�� �����Ѵ�.
        // �ʿ�� Fixed�� �����Ѵ�.
        idlOS::memcpy( aDataNode->dstColumn,
                       sColumn,
                       ID_SIZEOF(mtcColumn) * sColumnCnt );
    }
    else
    {
        // BUG-34737
        // src�� dst�� ���� ��� ������ �ʿ䰡 ����.
        // Nothing to do.
    }

    // Execute ������ ����
    if( aDataNode->dstTuple->execute + aDataNode->dstNode->node.column != sExecute )
    {
        idlOS::memcpy(
            aDataNode->dstTuple->execute + aDataNode->dstNode->node.column,
            sExecute,
            ID_SIZEOF(mtcExecute) * sColumnCnt );
    }
    else
    {
        // Nothing to do.
    }

    //--------------------------------------------------------
    // [Materialized Node�� �Լ� ������ ����]
    // Materialized Node�� �Լ� �����͸� �����Ѵ�.
    //    - Conversion�� �߻��� ���
    //    - �� Materialized Node�� ������ ���� ����
    //--------------------------------------------------------

    // BUG-36472
    // dstNode �� valueModule �� ��� ����
    // valueModule �� QMC_MTR_TYPE_CALCULATE �� �� ����
    IDE_DASSERT( ( aDataNode->dstNode->node.module != &qtc::valueModule ) ||
                 ( ( aCodeNode->flag & QMC_MTR_TYPE_MASK) != QMC_MTR_TYPE_CALCULATE ) );

    if( ( aDataNode->dstNode->node.module == &qtc::valueModule ) ||
        ( ( sSrcNode != aCodeNode->srcNode ) &&
          ( ( aCodeNode->flag & QMC_MTR_TYPE_MASK) != QMC_MTR_TYPE_CALCULATE ) ) )
    {
        // Conversion�� �߻��ϴ� ��� �Ǵ� Indirection�� �߻��ϴ� ���

        // To Fix PR-8333
        // Value�� ������ ����� �ؾ� ��.
        aDataNode->dstTuple->execute[aDataNode->dstNode->node.column] =
            qmc::valueExecute;

        aDataNode->dstColumn->column.flag &= ~SMI_COLUMN_TYPE_MASK;
        aDataNode->dstColumn->column.flag |= SMI_COLUMN_TYPE_FIXED;

        // BUG-38494
        // Compressed Column ���� �� ��ü�� ����ǹǷ�
        // Compressed �Ӽ��� �����Ѵ�
        aDataNode->dstColumn->column.flag &= ~SMI_COLUMN_COMPRESSION_MASK;
        aDataNode->dstColumn->column.flag |= SMI_COLUMN_COMPRESSION_FALSE;
    }
    else
    {
        // Nothing to do.
    }

    IDE_TEST( setFunctionPointer( aDataNode ) != IDE_SUCCESS );

    IDE_TEST( setCompareFunction( aDataNode ) != IDE_SUCCESS );

    //-------------------------------------------------
    // PROJ-1705
    // QMC_MTR_TYPE_DISK_TABLE �� ���
    // rid�� ���ڵ� ��ġ�� smiColumnList�� �ʿ���.
    // ���⼭ ������ smiColumnList�� smiFetchRowFromGRID()�Լ��� ���ڷ� ����.
    // (��ũ�������̺��� �ش���� �ʴ´�.)
    //-------------------------------------------------

    if( ( ( ( aDataNode->flag & QMC_MTR_TYPE_MASK )  == QMC_MTR_TYPE_DISK_TABLE ) &&
          ( ( aDataNode->flag & QMC_MTR_BASETABLE_TYPE_MASK ) == QMC_MTR_BASETABLE_TYPE_DISKTABLE ) )
        ||
        ( ( aDataNode->flag & QMC_MTR_TYPE_MASK ) == QMC_MTR_TYPE_DISK_PARTITIONED_TABLE )
        ||
        ( ( aDataNode->flag & QMC_MTR_TYPE_MASK ) == QMC_MTR_TYPE_HYBRID_PARTITIONED_TABLE )
        ||
        ((aDataNode->flag & QMC_MTR_REMOTE_TABLE_MASK) == QMC_MTR_REMOTE_TABLE_TRUE)
        )
    {
        /*
         * ���̽����̺��� ��ũ���̺�(��ũ�������̺�������)
         * (QMC_MTR_TYPE_DISK_TABLE, QMC_MTR_BASETABLE_DISKTABLE)�� ����
         * ���̽����̺��� ��ũ ��Ƽ�ǵ����̺�
         * (QMC_MTR_TYPE_DISK_PARTITIONED_TABLE)�� ���
         * ���̽����̺��� Hybrid Partitioned Table
         * (QMC_MTR_TYPE_HYBRID_PARTITIONED_TABLE)�� ��� (PROJ-2464 hybrid partitioned table ����)
         * Remote Table �� ��� (BUG-39837)
         */

        IDE_TEST( qdbCommon::makeFetchColumnList4TupleID(
                      aTemplate,
                      aDataNode->srcNode->node.table,
                      ID_FALSE, // aIsNeedAllFetchColumn
                      NULL,     // index ����
                      ID_TRUE,  // aIsAllocSmiColumnList
                      & aDataNode->fetchColumnList ) != IDE_SUCCESS );
    }
    else
    {
        aDataNode->fetchColumnList = NULL;
    }

    // tmplate �߰�
    aDataNode->tmplate = & aTemplate->tmplate;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmc::setFunctionPointer( qmdMtrNode * aDataNode )
{
/***********************************************************************
 *
 * Description :
 *    Materialized Node�� ������ ���� �ش� �Լ� Pointer�� �����Ѵ�.
 *
 * Implementation :
 *
 *    �ش� Column�� ������ ���� �Լ� pointer�� Setting�Ѵ�.
 *
 ***********************************************************************/

    // Column ������ ���� Function Pointer ����

    switch ( aDataNode->flag & QMC_MTR_TYPE_MASK )
    {
        //-----------------------------------------------------------
        // Base Table�� ������ ���� ó��
        //-----------------------------------------------------------

        case QMC_MTR_TYPE_MEMORY_TABLE :
        {
            //-----------------------------------
            // Memory Base Table�� ���
            //-----------------------------------

            aDataNode->func.setMtr   = qmc::setMtrByPointer;
            aDataNode->func.getHash  = qmc::getHashNA;   // N/A
            aDataNode->func.isNull   = qmc::isNullNA;    // N/A
            aDataNode->func.makeNull = qmc::makeNullNothing;
            aDataNode->func.getRow   = qmc::getRowNA;    // N/A

            if ((aDataNode->flag & QMC_MTR_RECOVER_RID_MASK) ==
                QMC_MTR_RECOVER_RID_TRUE)
            {
                /*
                 * select target �� _prowid �� ����
                 * => rid ������ �ʿ�
                 */
                aDataNode->func.setTuple = qmc::setTupleByPointer4Rid;
            }
            else
            {
                aDataNode->func.setTuple = qmc::setTupleByPointer;
            }
            break;
        }
        case QMC_MTR_TYPE_DISK_TABLE :
        {
            //-----------------------------------
            // Disk Base Table�� ���
            //-----------------------------------

            aDataNode->func.setMtr   = qmc::setMtrByRID;
            aDataNode->func.getHash  = qmc::getHashNA;   // N/A
            aDataNode->func.isNull   = qmc::isNullNA;    // N/A
            aDataNode->func.makeNull = qmc::makeNullNothing;
            aDataNode->func.getRow   = qmc::getRowNA;    // N/A
            aDataNode->func.setTuple = qmc::setTupleByRID;

            break;
        }
        // PROJ-1502 PARTITIONED DISK TABLE
        // partitioned table�� �� row���� �´� tuple id�� �����Ͽ��� �Ѵ�.
        case QMC_MTR_TYPE_MEMORY_PARTITIONED_TABLE :
        {
            //-----------------------------------
            // Memory Partitioned Table�� ���
            //-----------------------------------

            aDataNode->func.setMtr   = qmc::setMtrByPointerAndTupleID;
            aDataNode->func.getHash  = qmc::getHashNA;   // N/A
            aDataNode->func.isNull   = qmc::isNullNA;    // N/A
            aDataNode->func.makeNull = qmc::makeNullNothing;
            aDataNode->func.getRow   = qmc::getRowNA;    // N/A
            aDataNode->func.setTuple = qmc::setTupleByPointerAndTupleID;

            break;
        }
        case QMC_MTR_TYPE_DISK_PARTITIONED_TABLE :
        {
            //-----------------------------------
            // Disk Partitioned Table�� ���
            //-----------------------------------

            aDataNode->func.setMtr   = qmc::setMtrByRIDAndTupleID;
            aDataNode->func.getHash  = qmc::getHashNA;   // N/A
            aDataNode->func.isNull   = qmc::isNullNA;    // N/A
            aDataNode->func.makeNull = qmc::makeNullNothing;
            aDataNode->func.getRow   = qmc::getRowNA;    // N/A
            aDataNode->func.setTuple = qmc::setTupleByRIDAndTupleID;

            break;
        }
        case QMC_MTR_TYPE_HYBRID_PARTITIONED_TABLE : /* PROJ-2464 hybrid partitioned table ���� */
        {
            //-----------------------------------
            // Hybrid Partitioned Table�� ���
            //-----------------------------------

            aDataNode->func.setMtr   = qmc::setMtrByPointerOrRIDAndTupleID;
            aDataNode->func.getHash  = qmc::getHashNA;   // N/A
            aDataNode->func.isNull   = qmc::isNullNA;    // N/A
            aDataNode->func.makeNull = qmc::makeNullNothing;
            aDataNode->func.getRow   = qmc::getRowNA;    // N/A
            aDataNode->func.setTuple = qmc::setTupleByPointerOrRIDAndTupleID;

            break;
        }

        //-----------------------------------------------------------
        // Column �� ���� ó��
        //-----------------------------------------------------------

        case QMC_MTR_TYPE_MEMORY_PARTITION_KEY_COLUMN :
        {
            //-----------------------------------
            // Memory Column�� ���
            //-----------------------------------

            IDE_TEST( setFunction4MemoryPartitionColumn( aDataNode ) != IDE_SUCCESS );

            break;
        }

        case QMC_MTR_TYPE_MEMORY_KEY_COLUMN :
        {
            //-----------------------------------
            // Memory Column�� ���
            //-----------------------------------

            IDE_TEST( setFunction4MemoryColumn( aDataNode ) != IDE_SUCCESS );

            break;
        }

        case QMC_MTR_TYPE_HYBRID_PARTITION_KEY_COLUMN : /* PROJ-2464 hybrid partitioned table ���� */
        {
            aDataNode->func.setMtr   = qmc::setMtrByCopyOrConvert;
            aDataNode->func.getHash  = qmc::getHashByValue;
            aDataNode->func.isNull   = qmc::isNullByValue;
            aDataNode->func.makeNull = qmc::makeNullValue;
            aDataNode->func.getRow   = qmc::getRowByValue;
            aDataNode->func.setTuple = qmc::setTupleByValue;
            break;
        }

        case QMC_MTR_TYPE_COPY_VALUE :
        {
            aDataNode->func.setMtr   = qmc::setMtrByCopy;
            aDataNode->func.getHash  = qmc::getHashByValue;
            aDataNode->func.isNull   = qmc::isNullByValue;
            aDataNode->func.makeNull = qmc::makeNullValue;
            aDataNode->func.getRow   = qmc::getRowByValue;
            aDataNode->func.setTuple = qmc::setTupleByValue;
            break;
        }
        case QMC_MTR_TYPE_CALCULATE :
        {
            aDataNode->func.setMtr   = qmc::setMtrByValue;
            aDataNode->func.getHash  = qmc::getHashByValue;
            aDataNode->func.isNull   = qmc::isNullByValue;
            aDataNode->func.makeNull = qmc::makeNullValue;
            aDataNode->func.getRow   = qmc::getRowByValue;
            aDataNode->func.setTuple = qmc::setTupleByValue;
            break;
        }
        case QMC_MTR_TYPE_CALCULATE_AND_COPY_VALUE :
        {
            aDataNode->func.setMtr   = qmc::setMtrByConvert;
            aDataNode->func.getHash  = qmc::getHashByValue;
            aDataNode->func.isNull   = qmc::isNullByValue;
            aDataNode->func.makeNull = qmc::makeNullValue;
            aDataNode->func.getRow   = qmc::getRowByValue;
            aDataNode->func.setTuple = qmc::setTupleByValue;
            break;
        }
        case QMC_MTR_TYPE_USELESS_COLUMN :
        {
            /***********************************************************
             * PROJ-2469 Optimize View Materialization
             * View Materialization�� MtrNode ��
             * ���� Query Block���� ��� ���� �ʴ� Column Node��
             * Dummy ����Ѵ�.
             ***********************************************************/
            aDataNode->func.setMtr   = qmc::setMtrByNull;
            aDataNode->func.getHash  = qmc::getHashNA;   // N/A
            aDataNode->func.isNull   = qmc::isNullNA;    // N/A
            aDataNode->func.makeNull = qmc::makeNullNothing;             
            aDataNode->func.getRow   = qmc::getRowNA;    // N/A
            aDataNode->func.setTuple = qmc::setTupleByNull;
            break;
        }            
        default :
        {
            // Materialized Column�� ������ �����Ǿ� �־�� ��.
            IDE_DASSERT(0);

            break;
        }
    } // end of switch

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmc::setCompareFunction( qmdMtrNode * aDataNode )
{
/***********************************************************************
 *
 * Description :
 *    Materialized Node�� �� �Լ� �� �� ��� Column�� �����Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

    //----------------------------------------------------
    // Column�� Sorting Order�� ���� �� �Լ� ����
    // - Asc/Desc�� ���� �� �Լ��� �����Ѵ�.
    // - ���� ����/Pointer�� ���忡 ���� ��� Column�� �����Ѵ�.
    //----------------------------------------------------
    UInt    sCompareType;

    // �� ��� Column�� ����
    // fix BUG-10114
    // disk variable column�� ���, ���� ������ ���� ���̱� ����,
    // mtrRow ������, smiVarColumn�� header�� �����ϰ� �ȴ�.
    // �� ��쵵 pointer�� �ƴ�, ���� ����Ǵ� ����̹Ƿ�,
    // dstColumn�� compareColumn�� �ǵ��� �����ؾ� �Ѵ�.
    //
    // PROJ-2469 Optimize View Materialization
    // QMC_MTR_TYPE_USELESS_COLUMN Type�� ��� dstColumn�� compareColumn�� �ǵ��� �����ؾ� �Ѵ�.
    if ( aDataNode->func.getRow == qmc::getRowByValue )
    {
        // �� ��ü�� ����Ǵ� ���
        aDataNode->func.compareColumn = aDataNode->dstColumn;
    }
    else
    {
        // ���� �ƴ� Pointer�� ����Ǵ� ���
        aDataNode->func.compareColumn = aDataNode->srcColumn;
    }

    // PROJ-2362 memory temp ���� ȿ���� ����
    // TEMP_TYPE�� ��� OFFSET_USELESS�� ���Ѵ�.
    if ( SMI_COLUMN_TYPE_IS_TEMP( aDataNode->func.compareColumn->column.flag ) == ID_TRUE )
    {
        if ( ( aDataNode->flag & QMC_MTR_SORT_ORDER_MASK )
             == QMC_MTR_SORT_ASCENDING )
        {
            aDataNode->func.compare =
                aDataNode->func.compareColumn->module->logicalCompare
                [MTD_COMPARE_ASCENDING];
        }
        else
        {
            aDataNode->func.compare =
                aDataNode->func.compareColumn->module->logicalCompare
                [MTD_COMPARE_DESCENDING];
        }
    }
    else
    {
        // BUG-38492 Compressed Column's Key Compare Function
        if ( (aDataNode->func.compareColumn->column.flag & SMI_COLUMN_COMPRESSION_MASK )
             == SMI_COLUMN_COMPRESSION_TRUE )
        {
            sCompareType = MTD_COMPARE_MTDVAL_MTDVAL;
        }
        else
        {
            if ( (aDataNode->func.compareColumn->column.flag & SMI_COLUMN_TYPE_MASK)
                 == SMI_COLUMN_TYPE_FIXED )
            {
                sCompareType = MTD_COMPARE_FIXED_MTDVAL_FIXED_MTDVAL;
            }
            else
            {
                sCompareType = MTD_COMPARE_MTDVAL_MTDVAL;
            }
        }

        // �� �Լ��� ����
        if ( ( aDataNode->flag & QMC_MTR_SORT_ORDER_MASK )
             == QMC_MTR_SORT_ASCENDING )
        {
            aDataNode->func.compare =
                aDataNode->func.compareColumn->module->keyCompare[sCompareType]
                [MTD_COMPARE_ASCENDING];
        }
        else
        {
            aDataNode->func.compare =
                aDataNode->func.compareColumn->module->keyCompare[sCompareType]
                [MTD_COMPARE_DESCENDING];
        }
    }

    return IDE_SUCCESS;
}

IDE_RC
qmc::setFunction4MemoryColumn( qmdMtrNode * aDataNode )
{
/***********************************************************************
 *
 * Description :
 *    Memory Column�� ���� �Լ� �����͸� �����Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

    if ( (aDataNode->dstTuple->lflag & MTC_TUPLE_STORAGE_MASK)
         == MTC_TUPLE_STORAGE_MEMORY )
    {
        //----------------------------------
        // Memory Temp Table�� ����� ���
        //----------------------------------

        // Fixed/Variable�� ���� ���� Pointer�� ������.
        aDataNode->func.setMtr   = qmc::setMtrByPointer;
        aDataNode->func.getHash  = qmc::getHashByPointer;
        aDataNode->func.isNull   = qmc::isNullByPointer;
        aDataNode->func.makeNull = qmc::makeNullNothing;
        aDataNode->func.getRow   = qmc::getRowByPointer;
        aDataNode->func.setTuple = qmc::setTupleByPointer;
    }
    else
    {
        //----------------------------------
        // Disk Temp Table�� ����� ���
        //----------------------------------

        if ( (aDataNode->dstColumn->column.flag & SMI_COLUMN_TYPE_MASK)
             == SMI_COLUMN_TYPE_FIXED )
        {
            // Fixed Memory Column�� ���
            aDataNode->func.setMtr   = qmc::setMtrByCopy;
            aDataNode->func.getHash  = qmc::getHashByValue;
            aDataNode->func.isNull   = qmc::isNullByValue;
            aDataNode->func.makeNull = qmc::makeNullNA;
            aDataNode->func.getRow   = qmc::getRowByValue;
            aDataNode->func.setTuple = qmc::setTupleByValue;
        }
        else
        {
            // Variable Memory Column�� ���
            aDataNode->func.setMtr   = qmc::setMtrByConvert;
            aDataNode->func.getHash  = qmc::getHashByValue;
            aDataNode->func.isNull   = qmc::isNullByValue;
            aDataNode->func.makeNull = qmc::makeNullNA;
            aDataNode->func.getRow   = qmc::getRowByValue;
            aDataNode->func.setTuple = qmc::setTupleByValue;
        }
    }

    return IDE_SUCCESS;
}

IDE_RC
qmc::setFunction4MemoryPartitionColumn( qmdMtrNode * aDataNode )
{
/***********************************************************************
 *
 * Description :
 *    Memory Column�� ���� �Լ� �����͸� �����Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

    if ( (aDataNode->dstTuple->lflag & MTC_TUPLE_STORAGE_MASK)
         == MTC_TUPLE_STORAGE_MEMORY )
    {
        //----------------------------------
        // Memory Temp Table�� ����� ���
        //----------------------------------

        // Fixed/Variable�� ���� ���� Partition Tuple ID�� Pointer�� ������.
        aDataNode->func.setMtr   = qmc::setMtrByPointerAndTupleID;
        aDataNode->func.getHash  = qmc::getHashByPointerAndTupleID;
        aDataNode->func.isNull   = qmc::isNullByPointerAndTupleID;
        aDataNode->func.makeNull = qmc::makeNullNothing;
        aDataNode->func.getRow   = qmc::getRowByPointerAndTupleID;
        aDataNode->func.setTuple = qmc::setTupleByPointerAndTupleID;
    }
    else
    {
        //----------------------------------
        // Disk Temp Table�� ����� ���
        //----------------------------------

        IDE_TEST( setFunction4MemoryColumn( aDataNode )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmc::refineOffset4MemoryColumn( qmdMtrNode * aMtrList,
                                qmdMtrNode * aColumnNode,
                                UInt       * aOffset )
{
/***********************************************************************
 *
 * Description :
 *     Memory Column�� Temp Table�� ����� ����� offset�� �����Ѵ�.
 *
 * Implementation :
 *     Memory Temp Table�� ����Ǵ� ���� Disk Temp Table�� ����Ǵ�
 *     ��츦 �����Ͽ� ó���Ͽ��� �Ѵ�.
 *         - Memory Temp Table�� ����Ǵ� ��� pointer�� �ߺ� ��������
 *           �ʵ��� �ϸ�,
 *         - Disk Temp Table�� ����Ǵ� ���� �� ��ü�� ������ �� �ֵ���
 *           �Ͽ��� �Ѵ�.
 *
 ***********************************************************************/

    qmdMtrNode * sFindNode;

    // ���ռ� �˻�
    IDE_DASSERT( aMtrList != NULL );
    IDE_DASSERT( aColumnNode != NULL );
    IDE_DASSERT( aOffset != NULL );

    if ( (aColumnNode->dstTuple->lflag & MTC_TUPLE_STORAGE_MASK)
         == MTC_TUPLE_STORAGE_MEMORY )
    {
        //----------------------------------
        // Memory Temp Table�� ����� ���
        //----------------------------------
        // �̹� �ش� Pointer�� �����ϴ� Node�� ���� ���,
        // ������ ������ �Ҵ���� �ʰ�, ������ ������ �״��
        // ����Ѵ�.

        // ������ Node�� �ִ� �� ã�´�.

        for ( sFindNode = aMtrList;
              sFindNode != aColumnNode;
              sFindNode = sFindNode->next )
        {
            if ( ( aColumnNode->srcTuple == sFindNode->srcTuple ) &&
                 ( ( ( sFindNode->flag & QMC_MTR_TYPE_MASK )
                     == QMC_MTR_TYPE_MEMORY_TABLE )
                   ||
                   ( ( sFindNode->flag & QMC_MTR_TYPE_MASK )
                     == QMC_MTR_TYPE_MEMORY_KEY_COLUMN ) ) )
            {
                if( aColumnNode->srcNode->node.conversion == NULL )
                {
                    // ������ Source Tuple�� ����
                    // ã�� ��尡 Memory Base Table�̰ų�,
                    // Memory Column�� ��쿡 ������ ���� ������
                    // ����� �� �ִ�.
                    break;
                }
                else
                {
                    // jhseong, BUG-8737, if conversion exists, passNode is used.
                    continue;
                }
            }
            else
            {
                // nothing to do
            }
        }
        if ( aColumnNode == sFindNode )
        {
            // ������ Node�� ã�� ���� ���
            // 32/64bit pointer�� ������ ������ �Ҵ�
            // RID�� ������ ���� ������ �Ҵ���.
            *aOffset = idlOS::align8( *aOffset );

            aColumnNode->dstColumn->column.offset = *aOffset;
            aColumnNode->dstColumn->column.size = ID_SIZEOF(scGRID);

            *aOffset += ID_SIZEOF(scGRID);
        }
        else
        {
            // ������ Node�� ã�� ���
            // ã�� Node�� ������ ����Ѵ�.
            aColumnNode->dstColumn->column.offset =
                sFindNode->dstColumn->column.offset;
            aColumnNode->dstColumn->column.size = ID_SIZEOF(scGRID);

            // offset�� ���� �ʿ� ����.
        }

        // pointer�� ����ǹǷ� fixed�� �����Ѵ�.
        aColumnNode->dstColumn->column.flag &= ~SMI_COLUMN_TYPE_MASK;
        aColumnNode->dstColumn->column.flag |= SMI_COLUMN_TYPE_FIXED;

        // BUG-38494
        // Compressed Column ���� �� ��ü�� ����ǹǷ�
        // Compressed �Ӽ��� �����Ѵ�
        aColumnNode->dstColumn->column.flag &= ~SMI_COLUMN_COMPRESSION_MASK;
        aColumnNode->dstColumn->column.flag |= SMI_COLUMN_COMPRESSION_FALSE;
    }
    else
    {
        //----------------------------------
        // Disk Temp Table�� ����� ���
        //----------------------------------

        // BUG-38592
        // Temp Table���� ���� ���� �����Ƿ�,
        // Fixed/Variable�� ������� Compressed �Ӽ��� �����Ѵ�
        aColumnNode->dstColumn->column.flag &= ~SMI_COLUMN_COMPRESSION_MASK;
        aColumnNode->dstColumn->column.flag |= SMI_COLUMN_COMPRESSION_FALSE;

        if ( (aColumnNode->dstColumn->column.flag & SMI_COLUMN_TYPE_MASK)
             == SMI_COLUMN_TYPE_FIXED )
        {
            // Fixed Memory Column�� ���
        }
        else
        {
            // Variable Memory Column�� ���

            // To Fix PR-8367
            // mtcColumn�� �ƴ� smiColumn�� ����ؾ� ��.
            // Memory Variable Column�� ��� Disk Temp Table��
            // ����� �� Data ��ü�� �����Ͽ��� ��.
            aColumnNode->dstColumn->column.flag &= ~SMI_COLUMN_TYPE_MASK;
            aColumnNode->dstColumn->column.flag |= SMI_COLUMN_TYPE_FIXED;
        }

        // Offset�� �����ϸ� �ȴ�.
        *aOffset = idlOS::align( *aOffset,
                                 aColumnNode->dstColumn->module->align );
        aColumnNode->dstColumn->column.offset = *aOffset;

        *aOffset += aColumnNode->dstColumn->column.size;
    }

    return IDE_SUCCESS;
}

IDE_RC
qmc::refineOffset4MemoryPartitionColumn( qmdMtrNode * aMtrList,
                                         qmdMtrNode * aColumnNode,
                                         UInt       * aOffset )
{
/***********************************************************************
 *
 * Description :
 *     Memory Column�� Temp Table�� ����� ����� offset�� �����Ѵ�.
 *
 * Implementation :
 *     Memory Temp Table�� ����Ǵ� ���� Disk Temp Table�� ����Ǵ�
 *     ��츦 �����Ͽ� ó���Ͽ��� �Ѵ�.
 *         - Memory Temp Table�� ����Ǵ� ��� pointer�� �ߺ� ��������
 *           �ʵ��� �ϸ�,
 *         - Disk Temp Table�� ����Ǵ� ���� �� ��ü�� ������ �� �ֵ���
 *           �Ͽ��� �Ѵ�.
 *
 ***********************************************************************/

    // ���ռ� �˻�
    IDE_DASSERT( aMtrList != NULL );
    IDE_DASSERT( aColumnNode != NULL );
    IDE_DASSERT( aOffset != NULL );

    if ( (aColumnNode->dstTuple->lflag & MTC_TUPLE_STORAGE_MASK)
         == MTC_TUPLE_STORAGE_MEMORY )
    {
        //----------------------------------
        // Memory Temp Table�� ����� ���
        //----------------------------------
        
        // BUG-38309
        // ������ 16byte���� Ŀ�� mtdByte�� ����Ѵ�.
        IDE_TEST( mtc::initializeColumn( aColumnNode->dstColumn,
                                         & mtdByte,
                                         1,
                                         ID_SIZEOF(qmcMemPartRowInfo),
                                         0 )
                  != IDE_SUCCESS );

        // fix BUG-10243
        *aOffset = idlOS::align( *aOffset,
                                 aColumnNode->dstColumn->module->align );

        aColumnNode->dstColumn->column.offset = *aOffset;

        *aOffset += aColumnNode->dstColumn->column.size;
    }
    else
    {
        //----------------------------------
        // Disk Temp Table�� ����� ���
        //----------------------------------

        IDE_TEST( refineOffset4MemoryColumn( aMtrList, aColumnNode, aOffset )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmc::findAttribute( qcStatement  * aStatement,
                    qmcAttrDesc  * aResult,
                    qtcNode      * aExpr,
                    idBool         aMakePassNode,
                    qmcAttrDesc ** aAttr )
{
/***********************************************************************
 *
 * Description :
 *    �־��� result descriptor���� Ư���� attribute�� ã�� ��ȯ�Ѵ�.
 *
 * Implementation :
 *     Result descriptor�� �� attribute���� ���� Ž���ϸ� ����
 *     expression���� Ȯ���Ѵ�.
 *     qtc::isEquivalentExpression�� ��� �� conversion���� ���ƾ�
 *     ���� ������ �����Ѵ�.
 *
 ***********************************************************************/

    mtcNode * sExpr1;
    mtcNode * sExpr2;
    mtcNode * sConverted1;
    mtcNode * sConverted2;
    qmcAttrDesc * sItrAttr;
    idBool    sIsSame;

    IDU_FIT_POINT_FATAL( "qmc::findAttribute::__FT__" );

    sExpr1 = &aExpr->node;

    while( sExpr1->module == &qtc::passModule )
    {
        sExpr1 = sExpr1->arguments;
    }

    for( sItrAttr = aResult;
         sItrAttr != NULL;
         sItrAttr = sItrAttr->next )
    {
        sExpr2 = &sItrAttr->expr->node;

        if( sExpr1 == sExpr2 )
        {
            break;
        }
        else
        {
            // Nothing to do.
        }

        IDE_TEST( qtc::isEquivalentExpression( aStatement,
                                               (qtcNode *)sExpr1,
                                               (qtcNode *)sExpr2,
                                               &sIsSame )
                  != IDE_SUCCESS );
        if( sIsSame == ID_TRUE )
        {
            sConverted1 = mtf::convertedNode(sExpr1, NULL);
            sConverted2 = mtf::convertedNode(sExpr2, NULL);

            // ���� conversion�� ������� ���ƾ� ���� expression���� �����Ѵ�.
            if ( QTC_STMT_COLUMN( aStatement, (qtcNode*)sConverted1 )->module ==
                 QTC_STMT_COLUMN( aStatement, (qtcNode*)sConverted2 )->module )
            {
                break;
            }
            else
            {
                /* Nothing to do */
            }

            // BUG-39875
            // UNION �� ���ÿ��� select �� target �� ��������尡 �޸��� �ִ�.
            // �̶����� aMakePassNode �� TRUE �̸� �������� �־ ���ٰ� �Ǵ��ؾ� �Ѵ�.
            // qmc::makeReference �Լ����� �������� �������ش�.
            if( aMakePassNode == ID_TRUE )
            {
                break;
            }
            else
            {
                /* Nothing to do */
            }
        }
        else
        {
            // Nothing to do.
        }
    }

    *aAttr = sItrAttr;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmc::makeReference( qcStatement  * aStatement,
                    idBool         aMakePassNode,
                    qtcNode      * aRefNode,
                    qtcNode     ** aDepNode )
{
/***********************************************************************
 *
 * Description :
 *    aDepNode�� aRefNode�� �����ϵ��� ���踦 �����Ѵ�.
 *    (�� node�� ���� column �Ǵ� expression���� �����Ѵ�)
 *
 * Implementation :
 *    - aDepNode�� pass node��� argument�� aRefNode�� ����Ų��.
 *    - aDepNode�� pass node�� �ƴ� ���
 *      - ���� aMakePassNode�� true�̸� aDepNode�� expression�̶��
 *        aRefNode�� ����Ű�� pass node�� �����Ͽ� aDepNode�� �����Ѵ�.
 *      - �� ���� ��� tuple-set ���� ��ġ�� �����ϰ� �����Ѵ�.
 *
 ***********************************************************************/

    qtcNode * sPassNode;

    IDU_FIT_POINT_FATAL( "qmc::makeReference::__FT__" );

    if( (*aDepNode)->node.module == &qtc::passModule )
    {
        // Parent�� node�� pass node�� ������ ���
        (*aDepNode)->node.arguments = &aRefNode->node;
    }
    else
    {
        // BUG-36997
        // _prowid �� ������ �������� �ʰ� column,value �� ���� �б⸸ �ϸ�ȴ�.
        // ���� PASS ��带 ������ �ʿ䰡 ����.
        if( ( aMakePassNode == ID_TRUE ) &&
            ( QMC_NEED_CALC( *aDepNode ) == ID_TRUE ) &&
            ( (*aDepNode)->node.module != &gQtcRidModule ) )
        {
            // Pass node�� ������ �ʿ䰡 �ִ� ���
            if( ( (*aDepNode)->node.module == &qtc::subqueryModule ) &&
                ( (*aDepNode) == aRefNode ) )
            {
                // Nothing to do.
                // Subquery�� ��� materialize�� �Ŀ� value node�� ����ǹǷ�
                // pass node�� �������� �ʾƵ� �ȴ�.
            }
            else
            {
                IDE_TEST( qtc::makePassNode( aStatement,
                                             NULL,
                                             aRefNode,
                                             &sPassNode )
                          != IDE_SUCCESS );

                // BUG-39875
                // PASS ��带 ���鶧 ������ �������� �������ش�.
                // ������ UNION �� ����ϸ� target �� �������� �����Ҽ� �ֱ� �����̴�.
                sPassNode->node.conversion = ((*aDepNode)->node).conversion;
                sPassNode->node.next       = ((*aDepNode)->node).next;

                *aDepNode = sPassNode;
            }
        }
        else
        {
            // Pass node�� ������ �ʿ䰡 ���� ���
            (*aDepNode)->node.table  = aRefNode->node.table;
            (*aDepNode)->node.column = aRefNode->node.column;
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmc::appendPredicate( qcStatement  * aStatement,
                      qmsQuerySet  * aQuerySet,
                      qmcAttrDesc ** aResult,
                      qmoPredicate * aPredicate,
                      UInt           aFlag )
{
/***********************************************************************
 *
 * Description :
 *    Result descriptor�� predicate�� ���Ե� attribute���� �߰��Ѵ�.
 *
 * Implementation :
 *    qmoPredicate�� next�� more�� ��ȸ�ϸ� ���Ե� expression���� �߰��Ѵ�.
 *
 ***********************************************************************/

    const qmoPredicate * sCurrent;

    IDU_FIT_POINT_FATAL( "qmc::appendPredicate::__FT__" );

    for( sCurrent = aPredicate;
         sCurrent != NULL;
         sCurrent = sCurrent->next )
    {
        IDE_TEST( qmc::appendPredicate( aStatement,
                                        aQuerySet,
                                        aResult,
                                        sCurrent->node,
                                        aFlag )
                  != IDE_SUCCESS );
        if( sCurrent->more != NULL )
        {
            IDE_TEST( qmc::appendPredicate( aStatement,
                                            aQuerySet,
                                            aResult,
                                            sCurrent->more,
                                            aFlag )
                      != IDE_SUCCESS );
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmc::appendPredicate( qcStatement  * aStatement,
                      qmsQuerySet  * aQuerySet,
                      qmcAttrDesc ** aResult,
                      qtcNode      * aPredicate,
                      UInt           aFlag )
{
/***********************************************************************
 *
 * Description :
 *    Result descriptor�� predicate�� ���Ե� attribute���� �߰��Ѵ�.
 *
 * Implementation :
 *    - �������ڳ� �񱳿������� ��� argument��� ���ȣ���Ѵ�.
 *    - �׷��� ���� ��� appendAttribute�� ȣ���Ѵ�.
 *
 ***********************************************************************/

    const mtcNode * sArg;

    IDU_FIT_POINT_FATAL( "qmc::appendPredicate::__FT__" );

    if( ( ( aPredicate->node.module->lflag & MTC_NODE_COMPARISON_MASK )
          == MTC_NODE_COMPARISON_TRUE ) ||
        ( ( aPredicate->node.module->lflag & MTC_NODE_LOGICAL_CONDITION_MASK )
          == MTC_NODE_LOGICAL_CONDITION_TRUE ) ||
        ( ( aPredicate->node.module->lflag & MTC_NODE_OPERATOR_MASK )
          == MTC_NODE_OPERATOR_LIST ) )
    {
        // �������� �Ǵ� �� �������� ���
        for( sArg = aPredicate->node.arguments;
             sArg != NULL;
             sArg = sArg->next )
        {
            IDE_TEST( qmc::appendPredicate( aStatement,
                                            aQuerySet,
                                            aResult,
                                            (qtcNode *)sArg,
                                            aFlag )
                      != IDE_SUCCESS );
        }
    }
    else
    {
        /* BUG-39611 The sys_connect_by_path function support expression
         * argument.
         * SYS_CONNEC_BY_PATH�� ���� ��� ��ü�� result descript�� 
         * �������� ���߿� CNBY Plan�������̹� ������ Arguments��
         * Tuple�� ������ �� �ִ�.
         */
        if ( ( aPredicate->node.lflag & MTC_NODE_FUNCTION_CONNECT_BY_MASK )
             == MTC_NODE_FUNCTION_CONNECT_BY_TRUE )
        {
            IDE_TEST( qmc::appendAttribute( aStatement,
                                            aQuerySet,
                                            aResult,
                                            aPredicate,
                                            QMC_ATTR_SEALED_TRUE,
                                            QMC_APPEND_EXPRESSION_TRUE,
                                            ID_FALSE )
                      != IDE_SUCCESS );
        }
        else
        {
            IDE_TEST( qmc::appendAttribute( aStatement,
                                            aQuerySet,
                                            aResult,
                                            aPredicate,
                                            aFlag,
                                            0,
                                            ID_FALSE )
                      != IDE_SUCCESS );
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmc::appendAttribute( qcStatement  * aStatement,
                      qmsQuerySet  * aQuerySet,
                      qmcAttrDesc ** aResult,
                      qtcNode      * aExpr,
                      UInt           aFlag,
                      UInt           aAppendOption,
                      idBool         aMakePassNode )
{
/***********************************************************************
 *
 * Description :
 *    Result descriptor�� ������ option�� ���� attribute�� �߰��Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

    qmcAttrDesc   * sAttrs;
    qmcAttrDesc   * sNewAttr;
    qmcAttrDesc   * sItrAttr;
    qtcNode       * sCopiedNode;
    mtcNode       * sArg;
    mtcNode       * sExpr;
    idBool          sExist = ID_FALSE;
    UInt            sFlag1;
    UInt            sFlag2;
    UInt            sMask;

    IDU_FIT_POINT_FATAL( "qmc::appendAttribute::__FT__" );

    sExpr = &aExpr->node;

    /*BUG-44649
      ���� �޼ҵ忡 ���� level �÷��� ����ϴ� ������ ������� �޶��� �� �ֽ��ϴ�. 
      result descriptor���� level ������ hierarchy ������ ���� ��� ������ ���� �ָ� �� �˴ϴ�.*/
    if( (aQuerySet->SFWGH != NULL) && (sExpr->arguments == NULL) )
    {
        if( aQuerySet->SFWGH->hierarchy == NULL )
        {
            if( ((aExpr->lflag & QTC_NODE_LEVEL_MASK) == QTC_NODE_LEVEL_EXIST) ||
                ((aExpr->lflag & QTC_NODE_ISLEAF_MASK) == QTC_NODE_ISLEAF_EXIST) )
            {
                aFlag &= ~QMC_ATTR_TERMINAL_MASK;
                aFlag |= QMC_ATTR_TERMINAL_TRUE;
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
    }
    else
    {
        // Nothing to do.
    }

    while( sExpr->module == &qtc::passModule )
    {
        sExpr = sExpr->arguments;

        // Pass node�� ��� expression�� ����Ű���� ���� ����� �ϳ��� attribute�� ����.
        aAppendOption &= ~QMC_APPEND_EXPRESSION_MASK;
        aAppendOption |= QMC_APPEND_EXPRESSION_TRUE;

        // Pass node�� ��� calculate�� �ʿ� ���� ������ �����ϸ� �ȴ�.
        aFlag &= ~QMC_ATTR_SEALED_MASK;
        aFlag |= QMC_ATTR_SEALED_TRUE;
    }

    if( ( aFlag & QMC_ATTR_SEALED_MASK ) == QMC_ATTR_SEALED_TRUE )
    {
        // Expression�� �ƴ� ��� sealed flag�� �����Ѵ�.
        if( ( sExpr->module == &qtc::columnModule ) ||
            ( sExpr->module == &qtc::valueModule ) )
        {
            aFlag &= ~QMC_ATTR_SEALED_MASK;
            aFlag |= QMC_ATTR_SEALED_FALSE;
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

    if( ( aAppendOption & QMC_APPEND_ALLOW_DUP_MASK ) == QMC_APPEND_ALLOW_DUP_FALSE )
    {
        // �ߺ��� ����ϵ��� flag�� �������� �ʾҴٸ� �̹� �����ϴ� attribute���� Ȯ��
        IDE_TEST( findAttribute( aStatement,
                                 *aResult,
                                 (qtcNode *)sExpr,
                                 ID_FALSE,
                                 &sAttrs )
                  != IDE_SUCCESS );
        if( sAttrs == NULL )
        {
            sExist = ID_FALSE;
        }
        else
        {
            if( ( aAppendOption & QMC_APPEND_CHECK_ANALYTIC_MASK ) == QMC_APPEND_CHECK_ANALYTIC_TRUE )
            {
                // Analytic function�� analytic�� Ȯ��
                // ex) RANK() OVER (ORDER BY c1, c2)
                //     ���� c1, c2�� ��� ���� ���ο� ���� ������ ���ƾ� �Ѵ�.
                //     Window sorting������ �� flag�� �����Ѵ�.
                // BUG-42145 NULLS ORDER ���� ���ƾ��Ѵ�.
                sMask = (QMC_ATTR_ANALYTIC_SORT_MASK | QMC_ATTR_SORT_ORDER_MASK | QMC_ATTR_SORT_NULLS_ORDER_MASK);
                sFlag1 = (sAttrs->flag & sMask);
                sFlag2 = (aFlag & sMask);

                if( sFlag1 == sFlag2 )
                {
                    sExist = ID_TRUE;
                }
                else
                {
                    // Nothing to do.
                }
            }
            else
            {
                sExist = ID_TRUE;
            }

            if( sExist == ID_TRUE )
            {
                // �̹� �����ϴ� ��� flag�� �����Ѵ�.
                sMask = (QMC_ATTR_SEALED_MASK | QMC_ATTR_DISTINCT_MASK | QMC_ATTR_ORDER_BY_MASK);
                sAttrs->flag |= (aFlag & sMask);

                if( &sAttrs->expr->node != sExpr )
                {
                    sCopiedNode = (qtcNode *)sExpr;

                    IDE_TEST( makeReference( aStatement,
                                             aMakePassNode,
                                             sAttrs->expr,
                                             (qtcNode**)&sExpr )
                              != IDE_SUCCESS );
             
                    // BUG-46424
                    if ( sExpr->module == &qtc::passModule )
                    {
                        for ( sArg = sCopiedNode->node.arguments;
                              sArg != NULL;
                              sArg = sArg->next )
                        {
                            IDE_TEST( appendAttribute( aStatement,
                                                       aQuerySet,
                                                       aResult,
                                                       (qtcNode *)sArg,
                                                       0,
                                                       0,
                                                       ID_FALSE )
                                      != IDE_SUCCESS );
                        }
                    }
                }
                else
                {
                    // Nothing to do.
                }
            }
        }
    }
    else
    {
        // Nothing to do.
    }

    if( sExist == ID_FALSE )
    {
        // �������� �ʴ� ��� ���� �߰��Ѵ�.

        if( ( ( aFlag & QMC_ATTR_CONVERSION_MASK ) == QMC_ATTR_CONVERSION_FALSE ) &&
            ( sExpr->conversion != NULL ) )
        {
            // Conversion flag�� false�̸鼭 conversion�� �����ϴ� ���
            // conversion�� ���ŵ� node�� �߰��Ѵ�.
            IDE_TEST( QC_QMP_MEM( aStatement )->alloc( ID_SIZEOF( qtcNode ),
                                                       (void **)&sCopiedNode )
                      != IDE_SUCCESS );

            idlOS::memcpy( sCopiedNode, sExpr, ID_SIZEOF( qtcNode ) );
            sCopiedNode->node.conversion = NULL;

            IDE_TEST( appendAttribute( aStatement,
                                       aQuerySet,
                                       aResult,
                                       sCopiedNode,
                                       aFlag,
                                       aAppendOption,
                                       aMakePassNode )
                      != IDE_SUCCESS );

            sExpr->table  = sCopiedNode->node.table;
            sExpr->column = sCopiedNode->node.column;

            // BUG-45320 ����� table column������ �ٲٱ����� flag�� �����Ѵ�. 
            sExpr->lflag &= ~MTC_NODE_COLUMN_LOCATE_CHANGE_MASK;
            sExpr->lflag |= MTC_NODE_COLUMN_LOCATE_CHANGE_TRUE;
        }
        else
        {
            if( ( QMC_NEED_CALC( (qtcNode*)sExpr ) == ID_FALSE ) ||
                ( ( aAppendOption & QMC_APPEND_EXPRESSION_MASK ) == QMC_APPEND_EXPRESSION_TRUE ) )
            {
                /*
                 * ������ �ش��ϴ� ��� ���� node�� ��ȸ���� �ʰ� �ٷ� �߰��Ѵ�.
                 *
                 *  1. ������ evaluation�� �ʿ����� ���� ���
                 *     Column, value, aggregate function�� ��� ����� �ٷ� �����ϸ� �ȴ�.
                 *
                 *  2. Expression flag�� ������ ���
                 *     sorting key, grouping key���� expression ��ü�� ����� ����ؾ� �Ѵ�.
                 *     �ݸ� sorting key�� �ƴ� column�� select������ ����ϴ� ��쿡�� ����
                 *     node���� Ž���Ͽ� ������ column���� �����ؾ� �Ѵ�.
                 *
                 *     ex) SELECT c1 * c2, c2 * c3 FROM t1 ORDER BY c1 * c2;
                 *         => PROJ -- [c1 * c2], [c2 * c3]
                 *            SORT -- #[c1 * c2], [c2], [c3]
                 *            SCAN -- [c1], [c2], [c3]
                 *            + #ǥ�ô� sorting key
                 */
                if( ( ( aAppendOption & QMC_APPEND_ALLOW_CONST_MASK ) == QMC_APPEND_ALLOW_CONST_FALSE ) &&
                    ( sExpr->module == &qtc::valueModule ) )
                {
                    // Nothing to do.
                    // ����� bind ������ ������ flag�� �����Ǿ����� �ʴٸ� �����Ѵ�.
                }
                else if( ( ( (qtcNode *)sExpr)->lflag & QTC_NODE_SEQUENCE_MASK ) == QTC_NODE_SEQUENCE_EXIST )
                {
                    // Nothing to do.
                    // Sequence�� ��� projection���� ������ �� �����Ƿ� �߰����� �ʴ´�.
                }
                else
                {
                    // Result descriptor���� ������ attribute�� ã�� �̾���δ�.
                    IDE_TEST( QC_QMP_MEM(aStatement)->alloc(
                                  ID_SIZEOF(qmcAttrDesc),
                                  (void**)&sNewAttr )
                              != IDE_SUCCESS );

                    sNewAttr->expr = (qtcNode*)sExpr;
                    sNewAttr->flag = aFlag;
                    sNewAttr->next = NULL;

                    if( *aResult == NULL )
                    {
                        // Result descriptor�� ����ִ� ���
                        *aResult = sNewAttr;
                    }
                    else
                    {
                        // Result descriptor�� ������� ���� ��� ������ node�� �̾��ش�.
                        for( sItrAttr = *aResult;
                             sItrAttr->next != NULL;
                             sItrAttr = sItrAttr->next )
                        {
                        }
                        sItrAttr->next = sNewAttr;
                    }
                }
            }
            else
            {
                if( sExpr->module == &qtc::subqueryModule )
                {
                    IDE_TEST( appendSubqueryCorrelation( aStatement,
                                                         aQuerySet,
                                                         aResult,
                                                         aExpr )
                              != IDE_SUCCESS );

                }
                else
                {
                    for( sArg = sExpr->arguments;
                         sArg != NULL;
                         sArg = sArg->next )
                    {
                        IDE_TEST( appendAttribute( aStatement,
                                                   aQuerySet,
                                                   aResult,
                                                   (qtcNode *)sArg,
                                                   aFlag,
                                                   aAppendOption,
                                                   aMakePassNode )
                                  != IDE_SUCCESS );
                    }
                }
            }
        }
    }
    else
    {
        // Nothing to do.
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmc::createResultFromQuerySet( qcStatement  * aStatement,
                               qmsQuerySet  * aQuerySet,
                               qmcAttrDesc ** aResult )
{
/***********************************************************************
 *
 * Description :
 *    Query-set �ڷᱸ���κ��� result descriptor�� �����Ѵ�.
 *    ���� �� operator���� ����Ѵ�.
 *    1. PROJ(top projection�� ���)
 *    2. VMTR
 *
 * Implementation :
 *    Query-set�� ������ target list�� Ž���ϸ� attribute�� �߰��Ѵ�.
 *
 ***********************************************************************/

    UInt                sFlag = 0;
    qmsTarget         * sTarget;
    qmcAttrDesc       * sNewAttr;
    qmcAttrDesc       * sLastAttr = NULL;

    IDU_FIT_POINT_FATAL( "qmc::createResultFromQuerySet::__FT__" );

    sFlag &= ~QMC_ATTR_CONVERSION_MASK;
    sFlag |= QMC_ATTR_CONVERSION_TRUE;

    // Query-set�� �ֻ��� projection�� ���
    for( sTarget = aQuerySet->target;
         sTarget != NULL;
         sTarget = sTarget->next )
    {
        if( aQuerySet->SFWGH != NULL )
        {
            // �Ϲ����� PROJ �Ǵ� VMTR
            if( ( sTarget->targetColumn->node.module != &qtc::columnModule ) &&
                ( sTarget->targetColumn->node.module != &qtc::valueModule ) )
            {
                // �ܼ� column�̳� ��� �� expression�� ���
                if( ( aQuerySet->SFWGH->selectType == QMS_DISTINCT ) ||
                    ( sTarget->targetColumn->node.module == &qtc::passModule ) ||
                    ( QTC_IS_AGGREGATE( sTarget->targetColumn ) == ID_TRUE ) )
                {
                    // DISTINCT���� ����� ��� HSDS����,
                    // Pass node�� �����ǰų� aggregate function�� ����� ��쿡��
                    // GRAG/AGGR���� ����� �����Ѵ�.
                    sFlag &= ~QMC_ATTR_SEALED_MASK;
                    sFlag |= QMC_ATTR_SEALED_TRUE;
                }
                else
                {
                    sFlag &= ~QMC_ATTR_SEALED_MASK;
                    sFlag |= QMC_ATTR_SEALED_FALSE;
                }
            }
            else
            {
                sFlag &= ~QMC_ATTR_SEALED_MASK;
                sFlag |= QMC_ATTR_SEALED_FALSE;
            }

            if( aQuerySet->SFWGH->selectType == QMS_DISTINCT )
            {
                sFlag &= ~QMC_ATTR_DISTINCT_MASK;
                sFlag |= QMC_ATTR_DISTINCT_TRUE;
            }
            else
            {
                sFlag &= ~QMC_ATTR_DISTINCT_MASK;
                sFlag |= QMC_ATTR_DISTINCT_FALSE;
            }
        }
        else
        {
            // UNION �� SET �������� ���� PROJ
            sFlag &= ~QMC_ATTR_SEALED_MASK;
            sFlag |= QMC_ATTR_SEALED_FALSE;
        }

        if( ( sTarget->flag & QMS_TARGET_ORDER_BY_MASK ) == QMS_TARGET_ORDER_BY_TRUE )
        {
            // ORDER BY������ alias�� indicator�� �����ϴ� ���
            sFlag &= ~QMC_ATTR_ORDER_BY_MASK;
            sFlag |= QMC_ATTR_ORDER_BY_TRUE;

            if( ( sTarget->targetColumn->node.module != &qtc::columnModule ) &&
                ( sTarget->targetColumn->node.module != &qtc::valueModule ) )
            {
                sFlag &= ~QMC_ATTR_SEALED_MASK;
                sFlag |= QMC_ATTR_SEALED_TRUE;
            }
        }
        else
        {
            sFlag &= ~QMC_ATTR_ORDER_BY_MASK;
            sFlag |= QMC_ATTR_ORDER_BY_FALSE;
        }

        // PROJ-2469 Optimize View Materialization
        // ���� Query Block���� �������� �ʴ� View�� Column�� ���ؼ� flag ó���Ѵ�.
        if ( ( sTarget->flag & QMS_TARGET_IS_USELESS_MASK ) == QMS_TARGET_IS_USELESS_TRUE )
        {
            sFlag &= ~QMC_ATTR_USELESS_RESULT_MASK;
            sFlag |=  QMC_ATTR_USELESS_RESULT_TRUE;
        }
        else
        {
            sFlag &= ~QMC_ATTR_USELESS_RESULT_MASK;
            sFlag |=  QMC_ATTR_USELESS_RESULT_FALSE;
        }
        
        IDE_TEST( QC_QMP_MEM( aStatement )->alloc( ID_SIZEOF( qmcAttrDesc ),
                                                   (void**)&sNewAttr )
                  != IDE_SUCCESS );

        sNewAttr->expr = sTarget->targetColumn;
        sNewAttr->flag = sFlag;
        sNewAttr->next = NULL;

        if( sLastAttr == NULL )
        {
            *aResult = sNewAttr;
        }
        else
        {
            sLastAttr->next = sNewAttr;
        }
        sLastAttr = sNewAttr;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmc::copyResultDesc( qcStatement        * aStatement,
                     const qmcAttrDesc  * aParent,
                     qmcAttrDesc       ** aChild )
{
/***********************************************************************
 *
 * Description :
 *    Parent���� child�� result descriptor�� �����Ѵ�.
 *
 * Implementation :
 *    �Ϲ����� linked-list�� ����� �����ϴ�.
 *
 ***********************************************************************/
    const qmcAttrDesc * sItrAttr;
    qmcAttrDesc       * sNewAttr;
    qmcAttrDesc       * sLastAttr = NULL;

    IDU_FIT_POINT_FATAL( "qmc::copyResultDesc::__FT__" );

    IDE_FT_ASSERT( *aChild == NULL );

    for( sItrAttr = aParent;
         sItrAttr != NULL;
         sItrAttr = sItrAttr->next )
    {
        IDE_TEST( QC_QMP_MEM(aStatement)->alloc(
                      ID_SIZEOF(qmcAttrDesc),
                      (void**)&sNewAttr )
                  != IDE_SUCCESS );

        sNewAttr->expr = sItrAttr->expr;
        sNewAttr->flag = sItrAttr->flag;
        sNewAttr->next = NULL;

        if( sLastAttr == NULL )
        {
            *aChild = sNewAttr;
        }
        else
        {
            sLastAttr->next = sNewAttr;
        }
        sLastAttr = sNewAttr;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmc::appendQuerySetCorrelation( qcStatement  * aStatement,
                                qmsQuerySet  * aQuerySet,
                                qmcAttrDesc ** aResult,
                                qmsQuerySet  * aSubquerySet,
                                qcDepInfo    * aDepInfo )
{
/***********************************************************************
 *
 * Description :
 *    Query-set�� ��������� ��ȸ�ϸ� correlation attribute���� �߰��Ѵ�.
 *
 * Implementation :
 *    Set ������ �ƴ� ��� SFWGH���� outerColumn���� correlation�� ã�´�.
 *
 ***********************************************************************/

    qmsSFWGH     * sSFWGH;
    qmsOuterNode * sOuterNode;
    qcDepInfo      sDepInfo;

    IDU_FIT_POINT_FATAL( "qmc::appendQuerySetCorrelation::__FT__" );

    if( aSubquerySet->setOp == QMS_NONE )
    {
        sSFWGH = aSubquerySet->SFWGH;

        for( sOuterNode = sSFWGH->outerColumns;
             sOuterNode != NULL;
             sOuterNode = sOuterNode->next )
        {
            qtc::dependencyAnd( &sOuterNode->column->depInfo, aDepInfo, &sDepInfo );

            if( qtc::haveDependencies( &sDepInfo ) == ID_TRUE )
            {
                IDE_TEST( appendAttribute( aStatement,
                                           aQuerySet,
                                           aResult,
                                           sOuterNode->column,
                                           0,
                                           0,
                                           ID_FALSE )
                          != IDE_SUCCESS );
            }
            else
            {
                // Nothing to do.
            }
        }
    }
    else
    {
        IDE_TEST( appendQuerySetCorrelation( aStatement,
                                             aQuerySet,
                                             aResult,
                                             aSubquerySet->left,
                                             aDepInfo )
                  != IDE_SUCCESS );

        IDE_TEST( appendQuerySetCorrelation( aStatement,
                                             aQuerySet,
                                             aResult,
                                             aSubquerySet->right,
                                             aDepInfo )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmc::appendSubqueryCorrelation( qcStatement  * aStatement,
                                qmsQuerySet  * aQuerySet,
                                qmcAttrDesc ** aResult,
                                qtcNode      * aSubqueryNode )
{
/***********************************************************************
 *
 * Description :
 *    Subquery�� correlation attribute���� result descriptor�� �߰��Ѵ�.
 *
 * Implementation :
 *    Outer query�� dependency�� ���� �����ϸ� subquery���� correlation
 *    ���θ� �Ǵ��Ѵ�.
 *
 ***********************************************************************/

    qmsParseTree     * sParseTree;
    qmsQuerySet      * sSubquerySet;
    qcDepInfo        * sDepInfo;

    IDU_FIT_POINT_FATAL( "qmc::appendSubqueryCorrelation::__FT__" );

    IDE_FT_ASSERT( aSubqueryNode->node.module == &qtc::subqueryModule );

    if( aQuerySet->setOp == QMS_NONE )
    {
        sParseTree = (qmsParseTree*)aSubqueryNode->subquery->myPlan->parseTree;
        sSubquerySet = sParseTree->querySet;

        // Outer query�� dependency�� ���´�.
        sDepInfo = &aQuerySet->SFWGH->depInfo;

        IDE_TEST( appendQuerySetCorrelation( aStatement,
                                             aQuerySet,
                                             aResult,
                                             sSubquerySet,
                                             sDepInfo )
                  != IDE_SUCCESS );
    }
    else
    {
        // Nothing to do.
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmc::pushResultDesc( qcStatement  * aStatement,
                     qmsQuerySet  * aQuerySet,
                     idBool         aMakePassNode,
                     qmcAttrDesc  * aParent,
                     qmcAttrDesc ** aChild )
{
/***********************************************************************
 *
 * Description :
 *    Parent���� child�� result descriptor�� �����Ѵ�.
 *    �� ������ ���� �� operator�鸶�� push projection�� �����ȴ�.
 *
 * Implementation :
 *    Parent�� �� attribute�� sealed flag�� ������ ��� expression��
 *    �״�� child�� �߰��ϰ�, �׷��� ���� ��� expression�� �� node����
 *    ������ child�� �߰��Ѵ�.
 *
 ***********************************************************************/

    UInt sFlag = 0;
    UInt sMask;
    const mtcNode * sArg;
    qmcAttrDesc * sAttr;
    qmcAttrDesc * sItrAttr;

    IDU_FIT_POINT_FATAL( "qmc::pushResultDesc::__FT__" );

    for( sItrAttr = aParent;
         sItrAttr != NULL;
         sItrAttr = sItrAttr->next )
    {
        if( ( sItrAttr->flag & QMC_ATTR_TERMINAL_MASK ) == QMC_ATTR_TERMINAL_TRUE )
        {
            continue;
        }
        else
        {
            // Nothing to do.
        }

        if( ( sItrAttr->expr->node.module == &qtc::subqueryModule ) &&
            ( ( sItrAttr->flag & QMC_ATTR_DISTINCT_MASK ) == QMC_ATTR_DISTINCT_FALSE ) &&
            ( ( sItrAttr->flag & QMC_ATTR_ORDER_BY_MASK ) == QMC_ATTR_ORDER_BY_FALSE ) )
        {
            // Subquery�� ������ ���� �� �ش����� ������ �����Ѵ�.
            // (Subquery�� ���� ����� �����Ƿ� ������ PROJ���� ���� ����)
            // 1. SELECT DISTINCT������ ���� ���(HSDS���� ����)
            // 2. SELECT������ ���ǰ� ORDER BY������ ������ ���(SORT���� ����)

            // BUG-35082 subquery�� correlation�� �ִ� attribute���� �߰����ش�.
            IDE_TEST( appendSubqueryCorrelation( aStatement,
                                                 aQuerySet,
                                                 aChild,
                                                 sItrAttr->expr )
                      != IDE_SUCCESS );
            continue;
        }
        else
        {
            // Nothing to do.
        }

        IDE_TEST( findAttribute( aStatement,
                                 *aChild,
                                 sItrAttr->expr,
                                 ID_FALSE,
                                 &sAttr )
                  != IDE_SUCCESS );

        if( sAttr != NULL )
        {
            // Result descriptor�� �̹� �����ϴ� ��� �������踦 �����.
            IDE_TEST( makeReference( aStatement,
                                     aMakePassNode,
                                     sAttr->expr,
                                     &sItrAttr->expr )
                      != IDE_SUCCESS );

            sMask = (QMC_ATTR_DISTINCT_MASK | QMC_ATTR_ORDER_BY_MASK);
            sAttr->flag |= (sItrAttr->flag & sMask);
        }
        else
        {
            // Result descriptor�� �������� �ʴ� ���

            /*
             * �Ʒ� ������ ��� �����Ǿ�� expression�� argument���� �߰��Ѵ�.
             * 1. GROUP BY, DISTINCT, ORDER BY���� expression�� �ƴϾ�� �Ѵ�.
             *    (���⿡ �ش�Ǹ� sealed flag�� ������)
             * 2. Column/value module�� �ƴϾ�� �Ѵ�(argument�� �������� ����).
             * 3. Subquery�� �ƴϾ�� �Ѵ�.
             */
            if( ( ( sItrAttr->flag & QMC_ATTR_SEALED_MASK ) == QMC_ATTR_SEALED_FALSE ) &&
                ( sItrAttr->expr->node.module != &qtc::columnModule ) &&
                ( sItrAttr->expr->node.module != &gQtcRidModule ) &&
                ( sItrAttr->expr->node.module != &qtc::valueModule ) &&
                ( sItrAttr->expr->node.module != &qtc::subqueryModule ) )
            {
                for( sArg = sItrAttr->expr->node.arguments;
                     sArg != NULL;
                     sArg = sArg->next )
                {
                    IDE_TEST( appendAttribute( aStatement,
                                               aQuerySet,
                                               aChild,
                                               (qtcNode *)sArg,
                                               0,
                                               0,
                                               aMakePassNode )
                              != IDE_SUCCESS );
                }
            }
            else
            {
                sMask = (QMC_ATTR_SEALED_MASK | QMC_ATTR_DISTINCT_MASK | QMC_ATTR_ORDER_BY_MASK);
                sFlag = sItrAttr->flag & sMask;

                IDE_TEST( appendAttribute( aStatement,
                                           aQuerySet,
                                           aChild,
                                           sItrAttr->expr,
                                           sFlag,
                                           QMC_APPEND_EXPRESSION_TRUE,
                                           aMakePassNode )
                          != IDE_SUCCESS );
            }
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmc::makeReferenceResult( qcStatement * aStatement,
                          idBool        aMakePassNode,
                          qmcAttrDesc * aParent,
                          qmcAttrDesc * aChild )
{
/***********************************************************************
 *
 * Description :
 *    �� result descriptor���� ���� ���踦 �������ش�.
 *    pushResultDesc�� ȣ������ �ʴ� ���� operator�鿡�� ȣ���Ѵ�.
 *      - HSDS
 *      - GRBY
 *
 * Implementation :
 *    �� result�� ������ attribute�� �����ϸ� makeReference�� ȣ���Ѵ�.
 *
 ***********************************************************************/

    qmcAttrDesc * sAttr;
    qmcAttrDesc * sItrAttr;

    IDU_FIT_POINT_FATAL( "qmc::makeReferenceResult::__FT__" );

    // BUG-48101 parents�� �ߺ��� result desc�� �������谡 �������� �ʽ��ϴ�.
    for( sItrAttr = aParent;
         sItrAttr != NULL;
         sItrAttr = sItrAttr->next )
    {
        IDE_TEST( findAttribute( aStatement,
                                 aChild,
                                 sItrAttr->expr,
                                 aMakePassNode,
                                 &sAttr )
                  != IDE_SUCCESS );

        if( sAttr != NULL )
        {
            IDE_TEST( makeReference( aStatement,
                                     aMakePassNode,
                                     sAttr->expr,
                                     &sItrAttr->expr )
                      != IDE_SUCCESS );
        }
        else
        {
            // Nothing to do.
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmc::filterResultDesc( qcStatement  * aStatement,
                       qmcAttrDesc ** aResult,
                       qcDepInfo    * aDepInfo,
                       qcDepInfo    * aDepInfo2 )
{
/***********************************************************************
 *
 * Description :
 *    Result descriptor���� attribute�� dependency�� aDepInfo��
 *    ���Ե��� �ʴ� ��� �����Ѵ�.
 *    ��, ���������� key flag�� ������ ��� �̸� �����ϰ� �����Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/
    qmcAttrDesc * sItrAttr  = *aResult;
    qmcAttrDesc * sPrevAttr = NULL;
    qmcAttrDesc * sNextAttr;
    qcDepInfo     sAndDependencies;

    IDU_FIT_POINT_FATAL( "qmc::filterResultDesc::__FT__" );

    // BUG-37057
    // target�� ������ �ܺ����� �÷��� ��� result desc �� �ɼ� ����.
    qtc::dependencyAnd( aDepInfo,
                        aDepInfo2,
                        &sAndDependencies );

    while ( sItrAttr != NULL )
    {
        sNextAttr = sItrAttr->next;

        // aDepInfo�� ���Ե��� �ʰ� key attribute�� �ƴ� ��� ����
        if( ( qtc::dependencyContains(
                  &sAndDependencies,
                  &sItrAttr->expr->depInfo ) == ID_FALSE ) &&
            ( ( sItrAttr->flag & QMC_ATTR_KEY_MASK ) == QMC_ATTR_KEY_FALSE ) )
        {
            if( sPrevAttr == NULL )
            {
                // Result descriptr�� ù ��° attribute�� ���
                *aResult = sNextAttr;
            }
            else
            {
                // Result descriptr�� ù ��°�� �ƴ� attribute�� ���
                sPrevAttr->next = sNextAttr;
            }

            IDE_TEST( QC_QMP_MEM( aStatement )->free( sItrAttr ) != IDE_SUCCESS );
        }
        else
        {
            sPrevAttr = sItrAttr;
        }

        sItrAttr = sNextAttr;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmc::appendViewPredicate( qcStatement  * aStatement,
                                 qmsQuerySet  * aQuerySet,
                                 qmcAttrDesc ** aResult,
                                 UInt           aFlag )
{
/***********************************************************************
 *
 * Description :
 *    BUG-43077
 *    view�ȿ��� �����ϴ� �ܺ� ���� �÷����� Result descriptor�� �߰��Ѵ�.
 *    ������ ���� ��쿡�� �ܺ� ���� �÷����� Result descriptor�� ������ �ִ�.
 *    SELECT count(a.i1) FROM t2 a, t2 b, Lateral (select * from t1 where t1.i1=a.i1 and t1.i1=b.i1) v;
 *
 * Implementation :
 *
 ***********************************************************************/
    qmsOuterNode * sNode;

    IDU_FIT_POINT_FATAL( "qmc::appendViewPredicate::__FT__" );

    if ( aQuerySet->setOp == QMS_NONE )
    {
        for ( sNode = aQuerySet->SFWGH->outerColumns;
              sNode != NULL;
              sNode = sNode->next )
        {
            IDE_TEST( appendPredicate( aStatement,
                                       aQuerySet,
                                       aResult,
                                       sNode->column,
                                       aFlag )
                      != IDE_SUCCESS );
        }
    }
    else
    {
        IDE_TEST( appendViewPredicate( aStatement,
                                       aQuerySet->left,
                                       aResult,
                                       aFlag )
                  != IDE_SUCCESS );

        IDE_TEST( appendViewPredicate( aStatement,
                                       aQuerySet->right,
                                       aResult,
                                       aFlag )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmc::duplicateGroupExpression( qcStatement * aStatement,
                               qtcNode     * aExpr )
{
/***********************************************************************
 *
 * Description :
 *    HAVING�� �Ǵ� analytic function�� ���Ե� group expression�� �ݵ��
 *    GRAG�� ����� �����Ѿ� �Ѵ�. �׷��� DISTINCT/ORDER BY���� �����
 *    ��� HSDS/SORT�� ����� ����Ű���� ����� �� �־� ������ �����Ѵ�.
 *
 * Implementation :
 *    Group expression�� pass node�� ���� �����ϹǷ� pass node�� ã��
 *    argument�� �������� ����Ű���� �Ѵ�.
 *
 ***********************************************************************/

    qtcNode * sNode;

    IDU_FIT_POINT_FATAL( "qmc::duplicateGroupExpression::__FT__" );

    if( aExpr->node.module == &qtc::passModule )
    {
        IDE_TEST( QC_QMP_MEM( aStatement )->alloc( ID_SIZEOF( qtcNode ),
                                                   (void**)&sNode )
                  != IDE_SUCCESS );
        idlOS::memcpy( sNode, aExpr->node.arguments, ID_SIZEOF( qtcNode ) );
        aExpr->node.arguments = (mtcNode *)sNode;
    }
    else
    {
        sNode = (qtcNode *)aExpr->node.arguments;

        for( sNode = (qtcNode *)aExpr->node.arguments;
             sNode != NULL;
             sNode = (qtcNode *)sNode->node.next )
        {
            IDE_TEST( qmc::duplicateGroupExpression( aStatement,
                                                     sNode )
                      != IDE_SUCCESS );
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

void qmc::disableSealTrueFlag( qmcAttrDesc * aResult )
{
    qmcAttrDesc * sItrAttr;

    for ( sItrAttr = aResult;
          sItrAttr != NULL;
          sItrAttr = sItrAttr->next )
    {
        if ( ( sItrAttr->flag & QMC_ATTR_SEALED_MASK ) == QMC_ATTR_SEALED_TRUE )
        {
            sItrAttr->flag &= ~QMC_ATTR_SEALED_MASK;
            sItrAttr->flag |= QMC_ATTR_SEALED_FALSE;
        }
        else
        {
            /* Nothing to do */
        }
    }
}

