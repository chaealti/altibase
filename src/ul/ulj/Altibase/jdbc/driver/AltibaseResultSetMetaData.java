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

package Altibase.jdbc.driver;

import java.sql.ResultSetMetaData;
import java.sql.SQLException;
import java.util.List;
import java.util.Set;

import Altibase.jdbc.driver.datatype.Column;
import Altibase.jdbc.driver.datatype.ColumnInfo;
import Altibase.jdbc.driver.ex.Error;
import Altibase.jdbc.driver.ex.ErrorDef;

/*
 * Catalog name = DB name
 * Schema name  = User name
 * Table name   = Table name
 */
public class AltibaseResultSetMetaData extends WrapperAdapter implements ResultSetMetaData
{
    private final List   mColumns;
    private final int    mColumnCount;
    private       String mCatalogName;

    AltibaseResultSetMetaData(List aColumns)
    {
        this(aColumns, aColumns.size());
    }

    AltibaseResultSetMetaData(List aColumns, int aColumnCount)
    {
        mColumns = aColumns;
        mColumnCount = aColumnCount;
    }

    public int getColumnCount() throws SQLException
    {
        return mColumnCount;
    }

    private Column column(int aColumnIndex) throws SQLException
    {
        if (aColumnIndex < 1 || aColumnIndex > mColumnCount)
        {
            Error.throwSQLException(ErrorDef.INVALID_COLUMN_INDEX,
                                    "1 ~ " + mColumnCount,
                                    String.valueOf(aColumnIndex));
        }

        return (Column)mColumns.get(aColumnIndex - 1);
    }

    private ColumnInfo columnInfo(int aColumnIndex) throws SQLException
    {
        return column(aColumnIndex).getColumnInfo();
    }

    public String getCatalogName(int aColumnIndex) throws SQLException
    {
        // BUGBUG (2013-01-17) ������ �׻� null�� ��������. ���� catalog ������ Altibase�� ���� ����.
        // BUG-32879�� ���� db_name�� ����ص״ٰ� ��ȯ���ֵ��� �Ѵ�.
        //return columnInfo(aColumnIndex).getCatalogName();
        return mCatalogName;
    }

    void setCatalogName(String aCatalogName)
    {
        mCatalogName = aCatalogName;
    }

    public String getColumnClassName(int aColumnIndex) throws SQLException
    {
        return column(aColumnIndex).getObjectClassName();
    }

    public int getColumnDisplaySize(int aColumnIndex) throws SQLException
    {
        return column(aColumnIndex).getMaxDisplaySize();
    }

    public String getColumnLabel(int aColumnIndex) throws SQLException
    {
        return columnInfo(aColumnIndex).getDisplayColumnName();
    }

    public String getColumnName(int aColumnIndex) throws SQLException
    {
        return columnInfo(aColumnIndex).getDisplayColumnName();
    }

    public int getColumnType(int aColumnIndex) throws SQLException
    {
        Set<Integer> sMappedJdbcTypeSet = column(aColumnIndex).getMappedJDBCTypes();
        return sMappedJdbcTypeSet.iterator().next();
    }

    public String getColumnTypeName(int aColumnIndex) throws SQLException
    {
        return column(aColumnIndex).getDBColumnTypeName();
    }

    public int getPrecision(int aColumnIndex) throws SQLException
    {
        return columnInfo(aColumnIndex).getPrecision();
    }

    public int getScale(int aColumnIndex) throws SQLException
    {
        return columnInfo(aColumnIndex).getScale();
    }

    public String getSchemaName(int aColumnIndex) throws SQLException
    {
        return columnInfo(aColumnIndex).getOwnerName();
    }

    public String getTableName(int aColumnIndex) throws SQLException
    {
        return columnInfo(aColumnIndex).getTableName();
    }

    public boolean isAutoIncrement(int aColumnIndex) throws SQLException
    {
        // auto increment �÷��� �������� �ʱ� ������ �׻� false
        return false;
    }

    public boolean isCaseSensitive(int aColumnIndex) throws SQLException
    {
        if (column(aColumnIndex).isNumberType())
        {
            // ���� Ÿ���� ��� case insensitive
            return false;
        }
        return true;
    }

    public boolean isCurrency(int aColumnIndex) throws SQLException
    {
        // cash value ���� Ÿ���� DB�� ���������� �ʱ� ������ false
        return false;
    }

    public boolean isDefinitelyWritable(int aColumnIndex) throws SQLException
    {
        // Ȯ�������� write ���� ���������� �� �� ����. �������, lock�� ������ ���� �ֱ� ����.
        return false;
    }

    public int isNullable(int aColumnIndex) throws SQLException
    {
        boolean sNullable = columnInfo(aColumnIndex).getNullable();
        if (sNullable == ColumnInfo.NULLABLE)
        {
            return ResultSetMetaData.columnNullable;
        }
        return ResultSetMetaData.columnNoNulls;
    }

    public boolean isReadOnly(int aColumnIndex) throws SQLException
    {
        return (columnInfo(aColumnIndex).getUpdatable() == false) ||
               (columnInfo(aColumnIndex).getBaseColumnName() == null);
    }

    public boolean isSearchable(int aColumnIndex) throws SQLException
    {
        // ��� �÷��� where���� ���� �� �ֱ� ������ �׻� true
        return true;
    }

    public boolean isSigned(int aColumnIndex) throws SQLException
    {
        return column(aColumnIndex).isNumberType();
    }

    public boolean isWritable(int aColumnIndex) throws SQLException
    {
        return (columnInfo(aColumnIndex).getUpdatable() == true) &&
               (columnInfo(aColumnIndex).getBaseColumnName() != null);
    }
}
