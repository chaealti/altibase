/*
 *  Copyright (c) 1999~2017, Altibase Corp. and/or its affiliates. All rights reserved.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License, version 3,
 *  as published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

package Altibase.jdbc.driver.datatype;

import Altibase.jdbc.driver.AltibaseTypes;

public class FloatColumn extends CommonNumericColumn
{
    private static final int FLOAT_PRECISION = 38;
    
    FloatColumn()
    {
        super();
        addMappedJdbcTypeSet(AltibaseTypes.FLOAT);
    }

    public int getDBColumnType()
    {
        return ColumnTypes.FLOAT;
    }

    public String getDBColumnTypeName()
    {
        return "FLOAT";
    }
    
    public void getDefaultColumnInfo(ColumnInfo aColumnInfo)
    {
        aColumnInfo.modifyColumnInfo(getDBColumnType(), (byte)1, FLOAT_PRECISION, 0);
    }

    protected void checkScaleOverflow(int aScale)
    {
        // BUG-41061 floatŸ���� ��� �ܼ��� scale������ ���ϸ� �ȵǰ� signExponent�� ������
        // üũ�ؾߵǱ� ������ �ش� �޼ҵ带 �������� �ʵ��� �������̵��Ѵ�.
    }
    
    protected void checkPrecisionOverflow(int aPrecision)
    {
        // BUG-41061 floatŸ���� ��� �ܼ��� precision������ ���ϸ� �ȵǰ� signExponent�� ������
        // üũ�ؾߵǱ� ������ �ش� �޼ҵ带 �������� �ʵ��� �������̵��Ѵ�.
    }
}
