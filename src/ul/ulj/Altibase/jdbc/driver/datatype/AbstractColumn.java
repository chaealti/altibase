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

import Altibase.jdbc.driver.cm.CmChannel;
import Altibase.jdbc.driver.cm.CmFetchResult;
import Altibase.jdbc.driver.ex.Error;
import Altibase.jdbc.driver.ex.ErrorDef;
import Altibase.jdbc.driver.util.DynamicArray;

import java.io.InputStream;
import java.io.Reader;
import java.math.BigDecimal;
import java.sql.*;
import java.sql.Date;
import java.util.*;

abstract class AbstractColumn implements Column
{
    // JDBC ���忡�� ���� null value
    // �� ���� ��Ƽ���̽����� ������ �� ����ϴ� null value�� �ƴϴ�.
    // mt type���� ���� null value�� �� Ÿ�Ժ� class�� ���ǵǾ� �ִ�.
    private static final boolean BOOLEAN_NULL_VALUE             = false;
    private static final byte    BYTE_NULL_VALUE                = 0;
    private static final short   SHORT_NULL_VALUE               = 0;
    private static final int     INT_NULL_VALUE                 = 0;
    private static final long    LONG_NULL_VALUE                = 0;
    private static final float   FLOAT_NULL_VALUE               = 0;
    private static final double  DOUBLE_NULL_VALUE              = 0;

    protected boolean            mIsNull            = false;
    private ColumnInfo           mColumnInfo;

    protected int                mMaxBinaryLength   = 0;

    // BUG-48380 ArrayListRowHandle���� ����ϴ� resultset row data ����� ArrayList
    protected List<Object>       mValues            = new ArrayList<>();

    /**
     * fetch�� �÷��� ������ �����ϰ� �ִ�.
     */
    private int                  mColumnIdx;

    protected abstract void setNullValue();

    protected abstract boolean isNullValueSet();

    protected abstract void loadFromSub(DynamicArray aArray);

    protected abstract void loadFromSub(int aLoadIndex);

    // BUG-46480 JdbcType ������ �����ϱ� ���� LinkedHashSet ��ü ����
    protected Set<Integer> mMappedJdbcTypeSet = new LinkedHashSet<Integer>();

    protected void addMappedJdbcTypeSet(int aType)
    {
        mMappedJdbcTypeSet.add(aType);
    }

    public Set<Integer> getMappedJDBCTypes()
    {
        return mMappedJdbcTypeSet;
    }

    protected abstract void readFromSub(CmChannel aChannel) throws SQLException;

    protected abstract void readAndStoreValue(CmChannel aChannel) throws SQLException;

    /**
     * ä�ηκ��� �÷������͸� �о� �ٷ� DynamicArray�� �����Ѵ�.
     * @param aChannel ��������� ���� ä�ΰ�ü
     * @param aArray �÷������͸� ������ DynamicArray��ü
     * @throws SQLException ���������� �÷������͸� �ε����� ���� ���
     */
    protected abstract void readFromSub(CmChannel aChannel, DynamicArray aArray) throws SQLException;

    protected abstract void setValueSub(Object aValue) throws SQLException;

    protected abstract Object getObjectSub() throws SQLException;

    protected boolean getBooleanSub() throws SQLException
    {
        Error.throwSQLException(ErrorDef.INVALID_TYPE_CONVERSION, getDBColumnTypeName(), "boolean");
        return BOOLEAN_NULL_VALUE;
    }

    protected byte getByteSub() throws SQLException
    {
        Error.throwSQLException(ErrorDef.INVALID_TYPE_CONVERSION, getDBColumnTypeName(), "byte");
        return BYTE_NULL_VALUE;
    }

    protected byte[] getBytesSub() throws SQLException
    {
        Error.throwSQLException(ErrorDef.INVALID_TYPE_CONVERSION, getDBColumnTypeName(), "byte[]");
        return null;
    }

    protected short getShortSub() throws SQLException
    {
        Error.throwSQLException(ErrorDef.INVALID_TYPE_CONVERSION, getDBColumnTypeName(), "short");
        return SHORT_NULL_VALUE;
    }

    protected int getIntSub() throws SQLException
    {
        Error.throwSQLException(ErrorDef.INVALID_TYPE_CONVERSION, getDBColumnTypeName(), "int");
        return INT_NULL_VALUE;
    }

    protected long getLongSub() throws SQLException
    {
        Error.throwSQLException(ErrorDef.INVALID_TYPE_CONVERSION, getDBColumnTypeName(), "long");
        return LONG_NULL_VALUE;
    }

    protected float getFloatSub() throws SQLException
    {
        Error.throwSQLException(ErrorDef.INVALID_TYPE_CONVERSION, getDBColumnTypeName(), "float");
        return FLOAT_NULL_VALUE;
    }

    protected double getDoubleSub() throws SQLException
    {
        Error.throwSQLException(ErrorDef.INVALID_TYPE_CONVERSION, getDBColumnTypeName(), "double");
        return DOUBLE_NULL_VALUE;
    }

    protected BigDecimal getBigDecimalSub() throws SQLException
    {
        Error.throwSQLException(ErrorDef.INVALID_TYPE_CONVERSION, getDBColumnTypeName(), "BigDecimal");
        return null;
    }

    protected String getStringSub() throws SQLException
    {
        Error.throwSQLException(ErrorDef.INVALID_TYPE_CONVERSION, getDBColumnTypeName(), "String");
        return null;
    }

    protected final Date getDateSub() throws SQLException
    {
        return getDateSub(null);
    }

    protected Date getDateSub(Calendar aCalendar) throws SQLException
    {
        Error.throwSQLException(ErrorDef.INVALID_TYPE_CONVERSION, getDBColumnTypeName(), "Date");
        return null;
    }

    protected final Time getTimeSub() throws SQLException
    {
        return getTimeSub(null);
    }

    protected Time getTimeSub(Calendar aCalendar) throws SQLException
    {
        Error.throwSQLException(ErrorDef.INVALID_TYPE_CONVERSION, getDBColumnTypeName(), "Time");
        return null;
    }

    protected final Timestamp getTimestampSub() throws SQLException
    {
        return getTimestampSub(null);
    }

    protected Timestamp getTimestampSub(Calendar aCalendar) throws SQLException
    {
        Error.throwSQLException(ErrorDef.INVALID_TYPE_CONVERSION, getDBColumnTypeName(), "Timestamp");
        return null;
    }

    protected InputStream getAsciiStreamSub() throws SQLException
    {
        Error.throwSQLException(ErrorDef.INVALID_TYPE_CONVERSION, getDBColumnTypeName(), "AsciiStream");
        return null;
    }

    protected InputStream getBinaryStreamSub() throws SQLException
    {
        Error.throwSQLException(ErrorDef.INVALID_TYPE_CONVERSION, getDBColumnTypeName(), "BinaryStream");
        return null;
    }

    protected Reader getCharacterStreamSub() throws SQLException
    {
        Error.throwSQLException(ErrorDef.INVALID_TYPE_CONVERSION, getDBColumnTypeName(), "CharacterStream");
        return null;
    }

    protected Blob getBlobSub() throws SQLException
    {
        Error.throwSQLException(ErrorDef.INVALID_TYPE_CONVERSION, getDBColumnTypeName(), "Blob");
        return null;
    }

    protected Clob getClobSub() throws SQLException
    {
        Error.throwSQLException(ErrorDef.INVALID_TYPE_CONVERSION, getDBColumnTypeName(), "Clob");
        return null;
    }

    protected void setNull()
    {
        mIsNull = true;
    }

    protected void setNullOrNotNull()
    {
        if (isNullValueSet())
        {
            setNull();
        }
        else
        {
            mIsNull = false;
        }
    }

    public final boolean isNumberType()
    {
        switch (getDBColumnType())
        {
            case ColumnTypes.SMALLINT:
            case ColumnTypes.INTEGER:
            case ColumnTypes.BIGINT:
            case ColumnTypes.REAL:
            case ColumnTypes.DOUBLE:
            case ColumnTypes.FLOAT:
            case ColumnTypes.NUMBER:
            case ColumnTypes.NUMERIC:
                return true;
            default:
                return false;
        }
    }

    public boolean isMappedJDBCType(int aSqlType)
    {
        // BUG-46480 ������ ���� HashSet.contains�� ����Ѵ�.
        return mMappedJdbcTypeSet.contains(aSqlType);
    }

    public void readParamsFrom(CmChannel aChannel) throws SQLException
    {
        readParamsFromSub(aChannel);

        if (isNullValueSet())
        {
            setNull();
        }
    }
    
    public void readParamsFromSub(CmChannel aChannel) throws SQLException
    {
        readFromSub(aChannel);
    }

    public void readFrom(CmChannel aChannel, CmFetchResult aFetchResult) throws SQLException
    {
        if (aFetchResult.fetchRemains())
        {
            // BUG-48380 ArrayListRowHandle�� ����� �� ������ ArrayList�� �����͸� �����ϰ� �׷��� ���� ��� ���� ������ ź��.
            if (aFetchResult.useArrayListRowHandle())
            {
                readAndStoreValue(aChannel);
            }
            else
            {
                readFromSub(aChannel, aFetchResult.getDynamicArray(this.getColumnIndex()));
            }
        }
        else
        {
            /* BUG-43807 TotalReceivedRowCount�� MaxRowCount���� ũ�ų� ������ �������� �����δ�
               �����Ͱ� ���ƿ��� ������ �ش� �����͸� �о���δ�.  */
            readFromSub(aChannel);
        }
    }

    public int getMaxBinaryLength()
    {
        return mMaxBinaryLength;
    }

    public void setMaxBinaryLength(int aMaxLength)
    {
        mMaxBinaryLength = aMaxLength;
    }

    public void loadFrom(DynamicArray aArray)
    {
        loadFromSub(aArray);
        setNullOrNotNull();
    }

    public void loadFrom(int aLoadIndex)
    {
        loadFromSub(aLoadIndex);
        setNullOrNotNull();
    }

    public void setColumnInfo(ColumnInfo aInfo)
    {
        mColumnInfo = aInfo;
        if (aInfo.getDataType() != getDBColumnType())
        {
            aInfo.modifyDataType(getDBColumnType());
        }
    }

    public ColumnInfo getColumnInfo()
    {
        return mColumnInfo;
    }

    public boolean isNull()
    {
        return mIsNull;
    }

    public void setValue(Object aValue) throws SQLException
    {
        if (aValue == null)
        {
            setNull();
            setNullValue();
        }
        else
        {
            setValueSub(aValue);
            setNullOrNotNull();
        }
    }

    public boolean getBoolean() throws SQLException
    {
        if (mIsNull)
        {
            return BOOLEAN_NULL_VALUE;
        }
        return getBooleanSub();
    }

    public byte getByte() throws SQLException
    {
        if (mIsNull)
        {
            return BYTE_NULL_VALUE;
        }
        return getByteSub();
    }

    public byte[] getBytes() throws SQLException
    {
        if (mIsNull)
        {
            return null;
        }
        return getBytesSub();
    }

    public short getShort() throws SQLException
    {
        if (mIsNull)
        {
            return SHORT_NULL_VALUE;
        }
        return getShortSub();
    }

    public int getInt() throws SQLException
    {
        if (mIsNull)
        {
            return INT_NULL_VALUE;
        }
        return getIntSub();
    }

    public long getLong() throws SQLException
    {
        if (mIsNull)
        {
            return LONG_NULL_VALUE;
        }
        return getLongSub();
    }

    public float getFloat() throws SQLException
    {
        if (mIsNull)
        {
            return FLOAT_NULL_VALUE;
        }
        return getFloatSub();
    }

    public double getDouble() throws SQLException
    {
        if (mIsNull)
        {
            return DOUBLE_NULL_VALUE;
        }
        return getDoubleSub();
    }

    public BigDecimal getBigDecimal() throws SQLException
    {
        if (mIsNull)
        {
            return null;
        }
        return getBigDecimalSub();
    }

    public String getString() throws SQLException
    {
        if (mIsNull)
        {
            return null;
        }
        return getStringSub();
    }

    public Date getDate() throws SQLException
    {
        return getDate(null);
    }

    public Date getDate(Calendar aCalendar) throws SQLException
    {
        if (mIsNull)
        {
            return null;
        }
        return getDateSub(aCalendar);
    }

    public Time getTime() throws SQLException
    {
        return getTime(null);
    }

    public Time getTime(Calendar aCalendar) throws SQLException
    {
        if (mIsNull)
        {
            return null;
        }
        return getTimeSub(aCalendar);
    }

    public Timestamp getTimestamp() throws SQLException
    {
        return getTimestamp(null);
    }

    public Timestamp getTimestamp(Calendar aCalendar) throws SQLException
    {
        if (mIsNull)
        {
            return null;
        }
        return getTimestampSub(aCalendar);
    }

    public InputStream getAsciiStream() throws SQLException
    {
        if (mIsNull)
        {
            return null;
        }
        return getAsciiStreamSub();
    }

    public InputStream getBinaryStream() throws SQLException
    {
        if (mIsNull)
        {
            return null;
        }
        return getBinaryStreamSub();
    }

    public Reader getCharacterStream() throws SQLException
    {
        if (mIsNull)
        {
            return null;
        }
        return getCharacterStreamSub();
    }

    public Blob getBlob() throws SQLException
    {
        if (canReturnNullObject())
        {
            return null;
        }
        return getBlobSub();
    }

    public Clob getClob() throws SQLException
    {
        if (canReturnNullObject())
        {
            return null;
        }
        return getClobSub();
    }

    public Object getObject() throws SQLException
    {
        if (canReturnNullObject())
        {
            return null;
        }
        return getObjectSub();
    }

    // BUG-37418 LOB�� null locator�� �ƴ϶�� ��ü�� ���� �� �־�� �Ѵ�.
    private boolean canReturnNullObject()
    {
        if (this instanceof LobLocatorColumn)
        {
            if (((LobLocatorColumn)this).isNullLocator())
            {
                return true;
            }
        }
        else if (mIsNull)
        {
            return true;
        }
        return false;
    }

    public void storeTo(ListBufferHandle aBufferWriter) throws SQLException
    {
        writeTo(aBufferWriter);
    }

    public int getColumnIndex()
    {
        return mColumnIdx;
    }

    public void setColumnIndex(int aColumnIndex)
    {
        this.mColumnIdx = aColumnIndex;
    }

    public void clearValues()
    {
        mValues.clear();
    }
}
