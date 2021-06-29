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
 * $Id: qtcSkip.cpp 82075 2018-01-17 06:39:52Z jina.kim $
 *
 * Description :
 *
 *     SKIP�� �ش��ϴ� Data Type
 *     ������ Data Type�� ���� ������ ���� ���鿡 ����
 *     Column ������ Setting�ϱ� ���� ���ȴ�.
 *
 *     ������ ���� �뵵�� ���� ���ȴ�.
 *     1.  Indirect Node�� Column ����
 *         - qtc::makeIndirect()
 *     2.  ó���� �ʿ䰡 ���� Node�� ���� Column ����
 *         - qtc::modifyQuantifedExpression()
 *         - WHERE I1 IN ( (SELECT A1 FROM T1) )
 *                       *                     *
 *     3. ó���� �ʿ� ���� Procedure Variable
 *         - qtc::makeProcVariable()
 *
 * ��� ���� :
 *
 * ��� :
 *
 **********************************************************************/

#include <idl.h>
#include <mte.h>
#include <qtc.h>

//-----------------------------------------
// Skip ������ Ÿ���� Module �� ���� ����
//-----------------------------------------

static IDE_RC mtdEstimate( UInt * aColumnSize,
                           UInt * aArguments,
                           SInt * aPrecision,
                           SInt * aScale );

mtdModule qtc::skipModule = {
    NULL,           // �̸�
    NULL,           // �÷� ����
    MTD_NULL_ID,    // data type id
    0,              // no
    {0,0,0,0,0,0,0,0}, // index type
    1,              // align ��
    0,              // flag
    0,              // max precision
    0,              // min scale
    0,              // max scale
    NULL,           // staticNull ��
    NULL,           // ���� ������ �ʱ�ȭ �Լ�
    mtdEstimate,    // Estimation �Լ�
    NULL,           // ���ڿ� => Value ��ȯ �Լ�
    NULL,           // ���� Data�� ũ�� ���� �Լ�
    NULL,           // ���� Data�� precision�� ��� �Լ�
    NULL,           // NULL �� ���� �Լ�
    NULL,           // Hash �� ���� �Լ�
    NULL,           // NULL���� �Ǵ� �Լ�
    NULL,           // Boolean TRUE �Ǵ� �Լ�
    {
        mtd::compareNA,           // Logical Comparison
        mtd::compareNA
    },
    {                   // Key Comparison
        {
            mtd::compareNA,    // Ascending Key Comparison
            mtd::compareNA     // Descending Key Comparison
        },
        {
            mtd::compareNA,    // Ascending Key Comparison
            mtd::compareNA     // Descending Key Comparison
        },
        {
            mtd::compareNA,
            mtd::compareNA
        },
        {
            mtd::compareNA,
            mtd::compareNA
        }
    },
    NULL,           // Canonize �Լ�
    NULL,           // Endian ���� �Լ�
    NULL,           // Validation �Լ�
    NULL,           // Selectivity �Լ�
    NULL,           // Enconding �Լ�
    NULL,           // Decoding �Լ�
    NULL,           // Compfile Format �Լ�
    NULL,           // Oracle�κ��� Value ���� �Լ�
    NULL,           // Meta ���� �Լ�

    // BUG-28934
    NULL,
    NULL,
    
    {
        // PROJ-1705
        NULL,           
        // PROJ-2429
        NULL
    },
    NULL,           
    NULL,            
    
    //PROJ-2399
    NULL
};

IDE_RC mtdEstimate( UInt * aColumnSize,
                    UInt * /*aArguments*/,
                    SInt * /*aPrecision*/,
                    SInt * /*aScale*/ )
{
/***********************************************************************
 *
 * Description :
 *
 *    Skip Data Type�� Column ������ Setting��
 *
 * Implementation :
 *
 ***********************************************************************/
    
#define IDE_FN "IDE_RC mtdEstimate"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    /*
    aColumn->column.flag     = SMI_COLUMN_TYPE_FIXED;
    aColumn->column.size     = 0;
    aColumn->type.dataTypeId = qtc::skipModule.id;
    aColumn->flag            = 0;
    aColumn->precision       = 0;
    aColumn->scale           = 0;
    aColumn->module          = &qtc::skipModule;
    */

    *aColumnSize = 0;
    
    return IDE_SUCCESS;
    
#undef IDE_FN
}
