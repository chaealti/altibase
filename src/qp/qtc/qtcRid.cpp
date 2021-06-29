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
 * $Id: qtcRid.cpp 36474 2009-11-02 05:48:43Z sungminee $
 **********************************************************************/

#include <idl.h>
#include <mte.h>
#include <qtc.h>
#include <smi.h>
#include <qmvQTC.h>
#include <qtcRid.h>
#include <qcuSqlSourceInfo.h>

extern mtxModule mtxRid; /* PROJ-2632 */

static IDE_RC qtcRidEstimate(mtcNode*     aNode,
                             mtcTemplate* aTemplate,
                             mtcStack*    aStack,
                             SInt         aRemain,
                             mtcCallBack* aCallBack);
static IDE_RC qtcRidCalculate(mtcNode*     aNode,
                              mtcStack*    aStack,
                              SInt         aRemain,
                              void*        aCalcInfo,
                              mtcTemplate* aTemplate);

static mtcName gQtcRidNames[1] = {
    { NULL, 3, (void*)"RID" }
};

mtfModule gQtcRidModule = {
    1|                        // �ϳ��� Column ����
    MTC_NODE_INDEX_UNUSABLE|  // Index�� ����� �� ����
    MTC_NODE_OPERATOR_MISC,   // ��Ÿ ������
    ~0,                       // Indexable Mask : �ǹ� ����
    1.0,                      // default selectivity (�� ������ �ƴ�)
    gQtcRidNames,             // �̸� ����
    NULL,                     // Counter ������ ����
    mtf::initializeDefault,   // ���� ������ �ʱ�ȭ �Լ�, ����
    mtf::finalizeDefault,     // ���� ����� ���� �Լ�, ����
    qtcRidEstimate,           // Estimate �� �Լ�
};

mtcColumn gQtcRidColumn;

mtcExecute gQtcRidExecute = {
    mtf::calculateNA,     // Aggregation �ʱ�ȭ �Լ�, ����
    mtf::calculateNA,     // Aggregation ���� �Լ�, ����
    mtf::calculateNA,
    mtf::calculateNA,     // Aggregation ���� �Լ�, ����
    qtcRidCalculate,      // calculate
    NULL,                 // ������ ���� �ΰ� ����, ����
    mtxRid.mCommon,
    mtk::estimateRangeNA, // Key Range ũ�� ���� �Լ�, ����
    mtk::extractRangeNA   // Key Range ���� �Լ�, ����
};

/*
 * -----------------------------------------
 * select _prowid from t1
 *
 * _prowid �� ���� qtcNode �� ����
 * parse �������� ȣ��ȴ�
 * -----------------------------------------
 */
IDE_RC qtcRidMakeColumn(qcStatement*    aStatement,
                        qtcNode**       aNode,
                        qcNamePosition* aUserPosition,
                        qcNamePosition* aTablePosition,
                        qcNamePosition* aColumnPosition)
{
    IDE_RC   sRc;
    qtcNode* sNode;

    IDU_LIMITPOINT("qtcRidMakeColumn::malloc");
    sRc = STRUCT_ALLOC(QC_QMP_MEM(aStatement), qtcNode , &sNode);
    IDE_TEST(sRc != IDE_SUCCESS);

    QTC_NODE_INIT(sNode);

    if ((aUserPosition != NULL) &&
        (aTablePosition != NULL) &&
        (aColumnPosition != NULL))
    {
        // -------------------------
        // | USER | TABLE | COLUMN |
        // -------------------------
        // |  O   |   O   |   O    |
        // -------------------------
        sNode->position.stmtText = aUserPosition->stmtText;
        sNode->position.offset   = aUserPosition->offset;
        sNode->position.size     = aColumnPosition->offset
            + aColumnPosition->size
            - aUserPosition->offset;
        sNode->userName          = *aUserPosition;
        sNode->tableName         = *aTablePosition;
        sNode->columnName        = *aColumnPosition;
    }
    else if ((aTablePosition != NULL) && (aColumnPosition != NULL))
    {
        // -------------------------
        // | USER | TABLE | COLUMN |
        // -------------------------
        // |  X   |   O   |   O    |
        // -------------------------
        sNode->position.stmtText = aTablePosition->stmtText;
        sNode->position.offset   = aTablePosition->offset;
        sNode->position.size     = aColumnPosition->offset
            + aColumnPosition->size
            - aTablePosition->offset;
        sNode->tableName         = *aTablePosition;
        sNode->columnName        = *aColumnPosition;
    }
    else if (aColumnPosition != NULL )
    {
        // -------------------------
        // | USER | TABLE | COLUMN |
        // -------------------------
        // |  X   |   X   |   O    |
        // -------------------------
        sNode->position        = *aColumnPosition;
        sNode->columnName      = *aColumnPosition;
    }
    else
    {
        IDE_ASSERT(0);
    }

    sNode->node.lflag = gQtcRidModule.lflag & (~MTC_NODE_COLUMN_COUNT_MASK);
    sNode->node.module = &gQtcRidModule;

    *aNode = sNode;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

static IDE_RC qtcRidCalculate(mtcNode*     aNode,
                              mtcStack*    aStack,
                              SInt         aRemain,
                              void*        /*aCalcInfo*/,
                              mtcTemplate* aTemplate)
{
    IDE_TEST_RAISE(aRemain < 1, ERR_STACK_OVERFLOW);

    aStack->column = &gQtcRidColumn;
    aStack->value  = (void*)&aTemplate->rows[aNode->table].rid;

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_STACK_OVERFLOW)
    {
        IDE_SET(ideSetErrorCode(mtERR_ABORT_STACK_OVERFLOW));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


static IDE_RC qtcRidEstimate(mtcNode*     aNode,
                             mtcTemplate* aTemplate,
                             mtcStack*    aStack,
                             SInt         aRemain,
                             mtcCallBack* aCallBack)
{
    qtcNode         * sNode         = (qtcNode *)aNode;
    qtcCallBackInfo * sCallBackInfo = (qtcCallBackInfo*)(aCallBack->info);

    qmsSFWGH        * sSFWGH;
    IDE_RC            sRc = IDE_SUCCESS;

    sSFWGH = sCallBackInfo->SFWGH;

    // ���� estimate �ÿ��� Column ID�� �Ҵ�ް� �ϰ�,
    // ������ estimate ȣ��ÿ��� Column ID�� �Ҵ���� �ʵ��� �Ѵ�.
    // ����, estimate() ������ CallBackInfo�� statement�� �����Ѵ�.
    if (sCallBackInfo->statement != NULL)
    {
        // ���� Column�� ��� Column ID�� Setting�Ѵ�.
        // Column�� �ƴ� ��쿡�� �ش� Node�� ������ Module�� ����ȴ�.
        // ���� ���, ������ ���� ���Ǹ� ���� ����.
        //     SELECT f1 FROM T1;
        // Parsing �ܰ迡���� [f1]�� Column���� �Ǵ�������,
        // �̴� Column�ϼ��� Function�� ���� �ִ�.
        // ���� Module�� ����� �����, ���ο��� estimate�� ����ȴ�.

        if ((sNode->lflag & QTC_NODE_COLUMN_ESTIMATE_MASK) ==
            QTC_NODE_COLUMN_ESTIMATE_TRUE)
        {
            // partition column id�� �����Ͽ����Ƿ� column id�� ���� ���ϴ� ����
            // ���� �ʴ´�.
        }
        else
        {
            sRc = qmvQTC::setColumnID4Rid(sNode, aCallBack);
        }
    }

    if( sRc == IDE_FAILURE )
    {
        // BUG-38507
        // setColumnID4Rid���� NOT EXISTS COLUMN ������ �߻��� ���
        // �Ϲ� column���� estimate �Ѵ�.
        if( ideGetErrorCode() == qpERR_ABORT_QMV_NOT_EXISTS_COLUMN )
        {
            aNode->module = &qtc::columnModule;
            IDE_TEST( aNode->module->estimate( aNode,
                                               aTemplate,
                                               aStack,
                                               aRemain,
                                               aCallBack )
                      != IDE_SUCCESS );
        }
        else
        {
            IDE_TEST( 1 );
        }
    }
    else
    {
        IDE_TEST_RAISE((aTemplate->rows[aNode->table].lflag & MTC_TUPLE_TYPE_MASK)
                        != MTC_TUPLE_TYPE_TABLE,
                        ERR_PROWID_NOT_SUPPORTED);

        //-----------------------------------------------
        // PROJ-1473
        // ���ǿ� ���� �÷������� �����Ѵ�.
        // ���ڵ��������� ó���� ���,
        // ��ũ���̺�� ���� ���ǿ� ���� �÷����� ����.
        //-----------------------------------------------
        IDE_TEST(qtc::setColumnExecutionPosition(aTemplate,
                                                 sNode,
                                                 sSFWGH,
                                                 sCallBackInfo->SFWGH)
                 != IDE_SUCCESS);

        IDE_TEST_RAISE((sNode->lflag & QTC_NODE_PRIOR_MASK) == QTC_NODE_PRIOR_EXIST,
                       ERR_PROWID_NOT_SUPPORTED);

        qtc::dependencySet(sNode->node.table, & sNode->depInfo);

        aTemplate->rows[aNode->table].lflag &= ~MTC_TUPLE_TARGET_RID_MASK;
        aTemplate->rows[aNode->table].lflag |= MTC_TUPLE_TARGET_RID_EXIST;

        aStack->column = &gQtcRidColumn;
        aTemplate->rows[aNode->table].ridExecute = &gQtcRidExecute;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_PROWID_NOT_SUPPORTED)
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QMV_PROWID_NOT_SUPPORTED));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

