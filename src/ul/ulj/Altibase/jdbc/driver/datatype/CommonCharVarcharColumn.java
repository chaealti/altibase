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

import Altibase.jdbc.driver.cm.CmBufferWriter;
import Altibase.jdbc.driver.cm.CmChannel;
import Altibase.jdbc.driver.cm.CmOperation;
import Altibase.jdbc.driver.ex.Error;
import Altibase.jdbc.driver.ex.ErrorDef;
import Altibase.jdbc.driver.util.DynamicArray;
import Altibase.jdbc.driver.util.ObjectDynamicArray;
import Altibase.jdbc.driver.util.TimeZoneUtils;

import java.io.*;
import java.math.BigDecimal;
import java.nio.charset.CharsetEncoder;
import java.sql.Date;
import java.sql.SQLException;
import java.sql.Time;
import java.sql.Timestamp;
import java.util.Calendar;

public abstract class CommonCharVarcharColumn extends AbstractColumn
{
    static final byte           LENGTH_SIZE_INT32              = 4;

    private static final byte   LENGTH_SIZE_INT16              = 2;
    private static final int    DEFAULT_CHAR_VARCHAR_PRECISION = 64;
    private static final int[]  PRECISION_CLASS                = { DEFAULT_CHAR_VARCHAR_PRECISION, 512, 4096, 8192, 16384, 32000 };

    private static final String STRING_NULL_VALUE              = "";

    private final byte          mLengthSize;
    private final int           mMaxLength;
    // ���� �����͸� ��� �ִ� �ʵ�
    private String              mStringValue                   = STRING_NULL_VALUE;
    private int                 mPreparedBytesLen;

    private CharsetEncoder      mDBEncoder;
    private CharsetEncoder      mNCharEncoder;
    private boolean             mIsRedundant                   = false;     // BUG-43807 redundant ��尡 Ȱ��ȭ �Ǿ� �ִ��� ����

    CommonCharVarcharColumn()
    {
        this(LENGTH_SIZE_INT16);
    }

    CommonCharVarcharColumn(byte aLengthSize)
    {
        switch (aLengthSize)
        {
            case LENGTH_SIZE_INT16:
                mMaxLength = ColumnConst.MAX_CHAR_LENGTH;
                break;
            case LENGTH_SIZE_INT32:
                mMaxLength = Integer.MAX_VALUE;
                break;
            default:
                Error.throwInternalError(ErrorDef.INTERNAL_ASSERTION);
                mMaxLength = -1;
                break;
        }

        mLengthSize = aLengthSize;
    }

    public int getMaxDisplaySize()
    {
        return getColumnInfo().getPrecision();
    }

    public int getOctetLength()
    {
        int sOctetLength = mLengthSize;

        if (isNationalCharset())
        {
            sOctetLength += getColumnInfo().getPrecision() * (int)mNCharEncoder.maxBytesPerChar();
        }
        else
        {
            sOctetLength += getColumnInfo().getPrecision();
        }

        return sOctetLength;
    }

    public String getObjectClassName()
    {
        return String.class.getName();
    }

    public DynamicArray createTypedDynamicArray()
    {
        return new ObjectDynamicArray();
    }

    public void storeTo(DynamicArray aArray)
    {
        ((ObjectDynamicArray) aArray).put(mStringValue);
    }

    public void storeTo()
    {
        mValues.add(mStringValue);
    }

    public boolean isArrayCompatible(DynamicArray aArray)
    {
        return (aArray instanceof ObjectDynamicArray);
    }

    public int prepareToWrite(CmBufferWriter aBufferWriter) throws SQLException
    {
        int sWriteStringMode = isNationalCharset()
                             ? CmOperation.WRITE_STRING_MODE_NCHAR
                             : CmOperation.WRITE_STRING_MODE_DB;

        mPreparedBytesLen = aBufferWriter.prepareToWriteString(mStringValue, sWriteStringMode);
        return mPreparedBytesLen + mLengthSize;
    }

    public int writeTo(CmBufferWriter aBufferWriter) throws SQLException
    {
    	
        switch (mLengthSize)
        {
            case LENGTH_SIZE_INT32:
                aBufferWriter.writeInt(mPreparedBytesLen);
                break;
            case LENGTH_SIZE_INT16:
                aBufferWriter.writeShort((short)mPreparedBytesLen);
                break;
            default:
            	break;
        }
        aBufferWriter.writePreparedString();

        return mLengthSize + mPreparedBytesLen;
    }

    public void getDefaultColumnInfo(ColumnInfo aColumnInfo)
    {
        aColumnInfo.modifyColumnInfo(getDBColumnType(), (byte)1, DEFAULT_CHAR_VARCHAR_PRECISION, 0);
    }

    private void adjustPrecisionForCharVarchar()
    {
        if (getColumnInfo() == null)
        {
            return;
        }

        if (ColumnTypes.isNCharType(getDBColumnType()))
        {
            // �Ź� ����ε� ���� �ʵ��� Ŭ���� ����
            if (mStringValue.length() > getColumnInfo().getPrecision())
            {
                getColumnInfo().modifyPrecision(mStringValue.length());
            }
        }
        else
        {
            // char, varchar�� ������ ��� �÷��� �׳� precision�� �����ϸ� ������
            // char, varchar�� character set�� ����� byte ���� precision���� �����ؾ� �Ѵ�.
            // nchar, nvarchar�� precision�� ���ڿ����̱� ������ �ش���� �ʴ´�.
            // �׸��� �����Ͱ� ���õ� ������ precision�� �����̶� ���ϸ�
            // ����ε��ؾ� �ϹǷ�, ������ size class�� �ξ� �Ź� ����ε����� �ʵ��� �Ѵ�.
            int sPrecision = mStringValue.length() * getColumnInfo().getCharPrecisionRate();
            if (sPrecision > getColumnInfo().getPrecision())
            {
                int i;
                for (i=0; i<PRECISION_CLASS.length; i++)
                {
                    if (sPrecision <= PRECISION_CLASS[i] || i == PRECISION_CLASS.length - 1)
                    {
                        break;
                    }
                }
                getColumnInfo().modifyPrecision(PRECISION_CLASS[i]);
            }
        }
    }
    
    protected abstract boolean isNationalCharset();

    // �������� ������ �Ǵ� ���������� �������ڷ� ���� AbstractColumn�� abstract method�� ����� �ؿ��� �����ϴ� �� ���ڴ�.
    private void replaceValue(String aString)
    {
        mStringValue = aString;
    }
    
    public void readParamsFromSub(CmChannel aChannel) throws SQLException
    {
        this.setRemoveRedundantMode(false);
        this.mStringValue = readString(aChannel);
    }

    protected void readFromSub(CmChannel aChannel) throws SQLException
    {
        readParamsFromSub(aChannel);
    }

    protected void readFromSub(CmChannel aChannel, DynamicArray aArray) throws SQLException
    {
        ((ObjectDynamicArray)aArray).put(readString(aChannel));
    }

    protected void readAndStoreValue(CmChannel aChannel) throws SQLException
    {
        mValues.add(readString(aChannel));
    }

    private String readString(CmChannel aChannel) throws SQLException
    {
        int sSize;
        int sSkipSize = 0;
        int sMaxFieldSize = getMaxBinaryLength();

        // BUG-43807 �ߺ��������� ��쿡�� ������ ���õǾ� �ִ� �÷����� �״�� �����ش�.
        if (mIsRedundant && isDuplicatedData(aChannel))
        {
            return getStringSub();
        }

        sSize = aChannel.readUnsignedShort();
        String sValue;

        if (sSize == 0)
        {
            sValue = STRING_NULL_VALUE;
        }
        else
        {
            if (getMaxBinaryLength() > 0)
            {
                // FOR PSM or Built-In Function
                // Remove the exceeded part
                if (sSize > sMaxFieldSize)
                {
                    sSkipSize = sSize - sMaxFieldSize;
                    sSize = sMaxFieldSize;
                }
                // sMaxFieldSize -= sSize;
            }

            // BUG-44206 �Ź� ByteBuffer�� �������� �ʰ� CmChannel�� ���ӽ�Ų��.
            sValue = aChannel.readCharVarcharColumnString(sSize, sSkipSize, isNationalCharset());
        }

        if (mIsRedundant) // BUG-43807 redundant ��尡 Ȱ��ȭ �Ǿ� �ִ� ��쿡�� �÷����� �������ش�.
        {
            replaceValue(sValue);
        }

        return sValue;
    }

    /**
     * �ߺ����������� ����. <br> redundant_transmission�� Ȱ��ȭ �� ��쿡�� �������� �߰������� 1byteũ���� �ߺ�������
     * Ȯ�ο� �÷��װ� �������ݿ� �߰��ȴ�.
     * @param aChannel ������ ���� �����͸� ���޹��� ä�ΰ�ü
     * @return �ߺ������͸� 1 �ƴϸ� 0
     * @throws SQLException ���������� ä�ηκ��� �����͸� ���� ���� ���
     */
    private boolean isDuplicatedData(CmChannel aChannel) throws SQLException
    {
        return aChannel.readByte() != 0;
    }

    void setRemoveRedundantMode(boolean aIsRedundant)
    {
        this.mIsRedundant = aIsRedundant;
    }

    protected void loadFromSub(DynamicArray aArray)
    {
        mStringValue = (String)((ObjectDynamicArray)aArray).get();
    }

    protected void loadFromSub(int aLoadIndex)
    {
        mStringValue = (String)mValues.get(aLoadIndex);
    }

    protected void setNullValue()
    {
        mStringValue = STRING_NULL_VALUE;
    }
    
    protected boolean isNullValueSet()
    {
        // �������� ���� ����Ÿ(mBytesValue)�κ��� ��ȿ�� ���ڿ�(mStringValue)�� ���� ������ ��쿡�� NULL�� �ƴϴ�.
        return (mStringValue.length() == 0);
    }
    
    protected boolean getBooleanSub() throws SQLException
    {
        String sString = getStringSub().trim();
        boolean sResult = false;
        try
        {
            sResult = Double.parseDouble(sString) != 0;
        }
        catch(NumberFormatException sNFE)
        {
            if ( ("TRUE".equalsIgnoreCase(sString)) ||
                 ("T".equalsIgnoreCase(sString)) )
            {
                sResult = true;
            }
            else if ( ("FALSE".equalsIgnoreCase(sString)) ||
                      ("F".equalsIgnoreCase(sString)) )
            {
                sResult = false;
            }
            else
            {
                Error.throwSQLException(ErrorDef.INVALID_DATA_CONVERSION, sString, "boolean");
            }
        }
        catch (Exception sEx)
        {
            Error.throwSQLException(ErrorDef.INVALID_DATA_CONVERSION, sString, "boolean", sEx);
        }
        return sResult;
    }

    protected byte getByteSub() throws SQLException
    {
        String sString = getStringSub().trim();
        try
        {
            return Byte.parseByte(sString);
        }
        catch (Exception sEx)
        {
            Error.throwSQLException(ErrorDef.INVALID_DATA_CONVERSION, sString, "byte", sEx);
            return 0;
        }
    }

    /**
     * PROJ-2427 byte[]���� ���������� ������ ���� �ʱ⶧���� String.getBytes�� �̿��� ����ؼ� �����ش�.
     */
    protected byte[] getBytesSub() throws SQLException
    {
        byte[] sByteArry = null;
        try
        {
            String sCharsetName = isNationalCharset() ? mNCharEncoder.charset().name() : mDBEncoder.charset().name();
            sByteArry = mStringValue.getBytes(sCharsetName);
            // BUG-44466 �������� �޾ƿ� byte array ũ�Ⱑ setMaxFieldSize���� ������ ũ�⺸�� Ŭ ��쿡�� ���� �����Ѵ�.
            if (mMaxBinaryLength > 0 && sByteArry.length > mMaxBinaryLength)
            {
                byte[] sByteArryTmp = new byte[mMaxBinaryLength];
                System.arraycopy(sByteArry,0, sByteArryTmp, 0, mMaxBinaryLength);
                sByteArry = sByteArryTmp;
            }
        }
        catch (UnsupportedEncodingException sEx)
        {
            Error.throwSQLException(ErrorDef.INVALID_DATA_CONVERSION, getStringSub().trim(), "byte[]", sEx);
        }

        return sByteArry;
    }

    protected short getShortSub() throws SQLException
    {
        String sString = getStringSub().trim();
        try
        {
            return Short.parseShort(sString);
        }
        catch (Exception sEx)
        {
            Error.throwSQLException(ErrorDef.INVALID_DATA_CONVERSION, sString, "short", sEx);
            return 0;
        }
    }

    protected int getIntSub() throws SQLException
    {
        String sString = getStringSub().trim();
        try
        {
            return Integer.parseInt(sString);
        }
        catch (Exception sEx)
        {
            Error.throwSQLException(ErrorDef.INVALID_DATA_CONVERSION, sString, "int", sEx);
            return 0;
        }
    }

    protected long getLongSub() throws SQLException
    {
        String sString = getStringSub().trim();
        try
        {
            return Long.parseLong(sString);
        }
        catch (Exception sEx)
        {
            Error.throwSQLException(ErrorDef.INVALID_DATA_CONVERSION, sString, "long", sEx);
            return 0;
        }
    }

    protected float getFloatSub() throws SQLException
    {
        String sString = getStringSub().trim();
        try
        {
            return Float.parseFloat(sString);
        }
        catch (Exception sEx)
        {
            Error.throwSQLException(ErrorDef.INVALID_DATA_CONVERSION, sString, "float", sEx);
            return 0;
        }
    }

    protected double getDoubleSub() throws SQLException
    {
        String sString = getStringSub().trim();
        try
        {
            return Double.parseDouble(sString);
        }
        catch (Exception sEx)
        {
            Error.throwSQLException(ErrorDef.INVALID_DATA_CONVERSION, sString, "double", sEx);
            return 0;
        }
    }

    protected BigDecimal getBigDecimalSub() throws SQLException
    {
        return new BigDecimal(getStringSub().trim());
    }

    protected String getStringSub()
    {
        return mStringValue;
    }

    protected Date getDateSub(Calendar aCalendar) throws SQLException
    {
        String sString = getStringSub().trim();
        try
        {
            Date sDate = Date.valueOf(sString);
            if (aCalendar != null)
            {
                sDate.setTime(TimeZoneUtils.convertTimeZone(sDate.getTime(), aCalendar));
            }
            return sDate;
        }
        catch (Exception sEx)
        {
            Error.throwSQLException(ErrorDef.INVALID_DATA_CONVERSION, sString, "Date", sEx);
            return null;
        }
    }

    protected Time getTimeSub(Calendar aCalendar) throws SQLException
    {
        String sString = getStringSub().trim();
        try
        {
            Time sTime = Time.valueOf(sString);
            if (aCalendar != null)
            {
                sTime.setTime(TimeZoneUtils.convertTimeZone(sTime.getTime(), aCalendar));
            }
            return sTime;
        }
        catch (Exception sEx)
        {
            Error.throwSQLException(ErrorDef.INVALID_DATA_CONVERSION, sString, "Time", sEx);
            return null;
        }
    }

    protected Timestamp getTimestampSub(Calendar aCalendar) throws SQLException
    {
        String sString = getStringSub().trim();
        try
        {
            Timestamp sTS = Timestamp.valueOf(sString);
            if (aCalendar != null)
            {
                sTS.setTime(TimeZoneUtils.convertTimeZone(sTS.getTime(), aCalendar));
            }
            return sTS;
        }
        catch (Exception sEx)
        {
            Error.throwSQLException(ErrorDef.INVALID_DATA_CONVERSION, sString, "Timestamp", sEx);
            return null;
        }
    }

    protected InputStream getAsciiStreamSub() throws SQLException
    {
        return getBinaryStream();
    }

    protected InputStream getBinaryStreamSub() throws SQLException
    {
        return new ByteArrayInputStream(this.getBytesSub());
    }
    
    protected Reader getCharacterStreamSub() throws SQLException
    {
        return new StringReader(getStringSub());
    }

    protected Object getObjectSub() throws SQLException
    {
        return getStringSub();
    }

    public void setTypedValue(boolean aValue)
    {
        mStringValue = aValue ? "1" : "0";
        setNullOrNotNull();
    }
    
    protected void setValueSub(Object aValue) throws SQLException
    {
        if (aValue instanceof CommonCharVarcharColumn)
        {
            CommonCharVarcharColumn sColumn = (CommonCharVarcharColumn)aValue;
            mStringValue = sColumn.mStringValue;
            adjustPrecisionForCharVarchar();
            return;
        }

        String sSource = "";
        if (aValue instanceof Number)
        {
            sSource = aValue.toString();
        }
        else if (aValue instanceof char[])
        {
            sSource = String.valueOf((char[]) aValue);
        }
        else if (aValue instanceof String) 
        {
            sSource = (String)aValue;
        }
        else if (aValue instanceof Boolean)
        {
            if((Boolean)aValue)
            {
                sSource = "1";
            }
            else
            {
                sSource = "0";
            }
        }
        else if (aValue instanceof Date)
        {
            sSource = aValue.toString();
        }
        else if (aValue instanceof Time)
        {
            sSource = aValue.toString();
        }
        else if (aValue instanceof Timestamp)
        {
            sSource = aValue.toString();
        }
        else
        {
            Error.throwSQLException(ErrorDef.UNSUPPORTED_TYPE_CONVERSION,
                                    aValue.getClass().getName(), getDBColumnTypeName());
        }

        if (sSource.length() > mMaxLength)
        {
            Error.throwSQLException(ErrorDef.VALUE_LENGTH_EXCEEDS,
                                    String.valueOf(sSource.length()),
                                    String.valueOf(mMaxLength));
        }

        mStringValue = sSource;
        adjustPrecisionForCharVarchar();
    }
    
    public void storeTo(ListBufferHandle aBufferWriter) throws SQLException
    {
        prepareToWrite(aBufferWriter);
        writeTo(aBufferWriter);
    }

    void setDBEncoder(CharsetEncoder aDBEncoder)
    {
        this.mDBEncoder = aDBEncoder;
    }

    void setNCharEncoder(CharsetEncoder aNCharEncoder)
    {
        this.mNCharEncoder = aNCharEncoder;
    }

}
