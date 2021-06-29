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

import java.io.InputStream;
import java.io.Reader;
import java.math.BigDecimal;
import java.sql.Blob;
import java.sql.Clob;
import java.sql.Date;
import java.sql.SQLException;
import java.sql.Time;
import java.sql.Timestamp;
import java.util.Calendar;
import java.util.Set;

import Altibase.jdbc.driver.cm.CmBufferWriter;
import Altibase.jdbc.driver.cm.CmChannel;
import Altibase.jdbc.driver.cm.CmFetchResult;
import Altibase.jdbc.driver.util.DynamicArray;

public interface Column
{
    int getDBColumnType();

    Set<Integer> getMappedJDBCTypes();

    boolean isMappedJDBCType(int aSqlType);

    String getDBColumnTypeName();

    String getObjectClassName();

    int getMaxDisplaySize();

    /**
     * SQL/CLI �� SQL_DESC_OCTET_LENGTH �� �����Ǵ� column �� �ִ� ���̸� ��ȯ�Ѵ�.
     */
    int getOctetLength();

    DynamicArray createTypedDynamicArray();

    boolean isArrayCompatible(DynamicArray aArray);

    void storeTo(DynamicArray aArray);

    // PROJ-2368
    /**
     * List Protocol �� ���� ByteBuffer �� �������ݿ��� ������ ���̳ʸ� ���·� ����Ÿ�� ����.
     * 
     * @param aBufferWriter ����Ÿ�� �� ByteBuffer Writer
     * @throws SQLException ����Ÿ ���ۿ� �������� ���
     */
    void storeTo(ListBufferHandle aBufferWriter) throws SQLException;

    /**
     * {@link #writeTo(CmBufferWriter)}�� �� �� �غ� �ϰ�, �� ����Ÿ�� ���̸� ��ȯ�Ѵ�.
     * 
     * @param aBufferWriter ����Ÿ�� �� ByteBuffer Writer
     * @return �� ����Ÿ�� ����Ʈ ����
     * @throws SQLException �� ����Ÿ�� �غ��ϴµ� �������� ���
     */
    int prepareToWrite(CmBufferWriter aBufferWriter) throws SQLException;

    /**
     * ByteBuffer�� �������ݿ��� ������ ���̳ʸ� ���·� ����Ÿ�� ����.
     * 
     * @param aBufferWriter ����Ÿ�� �� ByteBuffer Writer
     * @return ByteBuffer�� �� ����Ÿ ������(byte ����)
     * @throws SQLException ����Ÿ ���ۿ� �������� ���
     */
    int writeTo(CmBufferWriter aBufferWriter) throws SQLException;

    void getDefaultColumnInfo(ColumnInfo aInfo);

    boolean isNumberType();

    /**
     * ä�ηκ��� �÷������͸� �о�� �ش��ϴ� �ε����� DynamicArray�� �ٷ� �����Ѵ�.
     * @param aChannel ��������� ���� ä�ΰ�ü
     * @param aFetchResult DynamicArray�� ���� �������̽��� ������ �ִ� ��ü
     * @throws SQLException ������Ž� ���ܰ� �߻��� ���
     */
    void readFrom(CmChannel aChannel, CmFetchResult aFetchResult) throws SQLException;

    void readParamsFrom(CmChannel aChannel) throws SQLException;

    void setMaxBinaryLength(int aMaxLength);

    void loadFrom(DynamicArray aArray);

    void setColumnInfo(ColumnInfo aInfo);

    ColumnInfo getColumnInfo();

    boolean isNull();

    void setValue(Object aValue) throws SQLException;

    boolean getBoolean() throws SQLException;

    byte getByte() throws SQLException;

    byte[] getBytes() throws SQLException;

    short getShort() throws SQLException;

    int getInt() throws SQLException;

    long getLong() throws SQLException;

    float getFloat() throws SQLException;

    double getDouble() throws SQLException;

    BigDecimal getBigDecimal() throws SQLException;

    String getString() throws SQLException;

    Date getDate() throws SQLException;

    Date getDate(Calendar aCalendar) throws SQLException;

    Time getTime() throws SQLException;

    Time getTime(Calendar aCalendar) throws SQLException;

    Timestamp getTimestamp() throws SQLException;

    Timestamp getTimestamp(Calendar aCalendar) throws SQLException;

    InputStream getAsciiStream() throws SQLException;

    InputStream getBinaryStream() throws SQLException;

    Reader getCharacterStream() throws SQLException;

    Blob getBlob() throws SQLException;

    Clob getClob() throws SQLException;

    Object getObject() throws SQLException;

    /**
     * �÷� �ε����� �����Ѵ�.
     * @param aColumnIndex �÷� �ε���
     */
    void setColumnIndex(int aColumnIndex);

    /**
     * �÷��� �ε����� �����´�.
     * @return �÷� �ε���
     */
    int getColumnIndex();

    /**
     * �÷� �����͸� ArrayList�� �����Ѵ�.
     */
    void storeTo();

    /**
     * ArrayList�� ���� �÷������͸� �о���δ�.
     * @param aLoadIndex
     */
    void loadFrom(int aLoadIndex);

    /**
     * �÷������Ͱ� ����Ǿ� �ִ� ArrayList�� �ʱ�ȭ �Ѵ�.
     */
    void clearValues();
}
