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

import java.io.ByteArrayInputStream;
import java.io.InputStream;
import java.io.Reader;
import java.io.StringReader;
import java.nio.ByteBuffer;
import java.sql.SQLException;

import Altibase.jdbc.driver.cm.CmBufferWriter;
import Altibase.jdbc.driver.cm.CmChannel;
import Altibase.jdbc.driver.ex.Error;
import Altibase.jdbc.driver.ex.ErrorDef;
import Altibase.jdbc.driver.util.DynamicArray;
import Altibase.jdbc.driver.util.ObjectDynamicArray;

abstract class AbstractBinaryColumn extends AbstractColumn
{
    private static class This
    {
        int    mLength;
        byte[] mByteArray;

        This(int aLength, byte[] aByteArray)
        {
            mLength = aLength;
            mByteArray = aByteArray;
        }
    }

    protected static final int      LENGTH_SIZE_INT8    = 1;
    protected static final int      LENGTH_SIZE_INT16   = 2;
    protected static final int      LENGTH_SIZE_INT32   = 4;
    protected static final int      LENGTH_SIZE_BINARY  = 8;

    private static final ByteBuffer NULL_BYTE_BUFFER    = ByteBuffer.allocate(0);

    private final int               mLengthSize;
    /**
     * ����Ÿ�� ����. Byte ���̰� �ƴ϶�, �� ���� Ÿ���� �����̴�.
     * �������, x1234�� BYTE Ÿ���� ���� ���̰� 2����, NIBBLE Ÿ���̸� 4���� �Ѵ�.
     */
    protected int                   mLength;
    /** ����Ÿ�� byte array ���·� ���� ����. */
    protected ByteBuffer            mByteBuffer         = NULL_BYTE_BUFFER;

    protected AbstractBinaryColumn(int aLengthSize)
    {
        switch (aLengthSize)
        {
            case LENGTH_SIZE_INT8:
            case LENGTH_SIZE_INT16:
            case LENGTH_SIZE_INT32:
            case LENGTH_SIZE_BINARY:
                break;
            default:
                Error.throwInternalError(ErrorDef.INVALID_TYPE,
                                         "LENGTH_SIZE_INT8 | LENGTH_SIZE_INT16 | LENGTH_SIZE_INT32 | LENGTH_SIZE_BINARY",
                                         String.valueOf(aLengthSize));
                break;
        }
        mLengthSize = aLengthSize;
    }

    protected AbstractBinaryColumn(int aLengthSize, int aPrecision)
    {
        this(aLengthSize);
        ensureAlloc(toByteLength(aPrecision));
    }

    protected abstract int toByteLength(int aLength);

    protected abstract int  nullLength();

    protected void clear()
    {
    }

    public void getDefaultColumnInfo(ColumnInfo aColumnInfo)
    {
        aColumnInfo.modifyColumnInfo(getDBColumnType(), (byte)1, aColumnInfo.getPrecision(), 0);
    }

    public int getOctetLength()
    {
        return getColumnInfo().getPrecision();
    }

    protected final void ensureAlloc(int aByteSize)
    {
        if (mByteBuffer.capacity() < aByteSize)
        {
            mByteBuffer = ByteBuffer.allocate(aByteSize);
        }
        else
        {
            mByteBuffer.clear();
            mByteBuffer.limit(aByteSize);
        }
    }

    public DynamicArray createTypedDynamicArray()
    {
        return new ObjectDynamicArray();
    }

    public boolean isArrayCompatible(DynamicArray aArray)
    {
        return (aArray instanceof ObjectDynamicArray);
    }

    public void storeTo(DynamicArray aArray)
    {
        /*
         * ���������� �����Ǹ� �� �޼ҵ�� ����ȭ�� ������ �� �ִ�. ��, mByteBuffer�� byte�� �����ߴٰ� �ٽ�
         * array�� ���� ����, �ٷ� array�� �ֵ��� �ؾ� �Ѵ�.
         */
        ((ObjectDynamicArray)aArray).put(new This(mLength, getBinaryByteArry()));
    }

    public void storeTo()
    {
        mValues.add(new This(mLength, getBinaryByteArry()));
    }

    private byte[] getBinaryByteArry()
    {
        mByteBuffer.rewind();
        byte[] sData = new byte[mByteBuffer.remaining()];
        mByteBuffer.get(sData);
        return sData;
    }

    protected void loadFromSub(DynamicArray aArray)
    {
        // BUG-43807 load ������ ByteBuffer �� allocate�Ѵ�.
        makeByteBufferFromByteArry((This)((ObjectDynamicArray)aArray).get());
        clear();
    }

    protected void loadFromSub(int aLoadIndex)
    {
        // BUG-43807 load ������ ByteBuffer �� allocate�Ѵ�.
        makeByteBufferFromByteArry((This)(mValues.get(aLoadIndex)));
        clear();
    }

    private void makeByteBufferFromByteArry(This aThis)
    {
        ensureAlloc(toByteLength(aThis.mLength));
        mByteBuffer.put(aThis.mByteArray);
        mByteBuffer.flip();
        mLength = aThis.mLength;
    }

    public int prepareToWrite(CmBufferWriter aBufferWriter) throws SQLException
    {
        int sByteLen = isNullValueSet() ? 0 : toByteLength(mLength);
        return mLengthSize + sByteLen;
    }

    public int writeTo(CmBufferWriter aBufferWriter) throws SQLException
    {
        /*
         * ���� �̸޼ҵ��� ������ �Ϻ����� �ʴ�. cmtBinary�� �����µ� ���� cmtBinary�� �ɰ��� ������ ���� �������
         * �ʾҴ�. binary�� ���� ��Ŷ���� ���� ��ŭ ū �����͸� ������ ��찡 �� ����, ������ ��ٷӰ� �������ϸ�, ���Ŀ�
         * ���������� �����Ǹ� ���ʿ��� �����̱� ������ �����ߴ�.
         */
        mByteBuffer.rewind();
        int sByteLen = isNullValueSet() ? 0 : toByteLength(mLength);
        if (sByteLen != mByteBuffer.remaining())
        {
            Error.throwInternalError(ErrorDef.INTERNAL_ASSERTION);
        }

        switch (mLengthSize)
        {
            case LENGTH_SIZE_INT8:
                aBufferWriter.writeByte((byte)mLength);
                break;
            case LENGTH_SIZE_INT16:
                aBufferWriter.writeShort((short)mLength);
                break;
            case LENGTH_SIZE_INT32:
                aBufferWriter.writeInt(mLength);
                break;
            case LENGTH_SIZE_BINARY:
                aBufferWriter.writeInt(mLength);
                aBufferWriter.writeInt(mLength); // BINARY�� length�� 2�� ����.
                break;
            default:
                Error.throwInternalError(ErrorDef.INTERNAL_ASSERTION);
                break;
        }
        if (sByteLen > 0)
        {
            aBufferWriter.writeBytes(mByteBuffer);
        }

        return mLengthSize + sByteLen;
    }

    protected void readFromSub(CmChannel aChannel) throws SQLException
    {
        mLength = readBinaryLength(aChannel);
        if (mLength == nullLength())
        {
            setNullValue();
        }
        else
        {
            ByteBuffer.wrap(readBytes(aChannel, this.mLength));
        }
    }

    protected void readFromSub(CmChannel aChannel, DynamicArray aArray) throws SQLException
    {
        int sLength = readBinaryLength(aChannel);
        ((ObjectDynamicArray)aArray).put(new This(sLength, readBytes(aChannel, sLength)));
    }

    protected void readAndStoreValue(CmChannel aChannel) throws SQLException
    {
        int sLength = readBinaryLength(aChannel);
        mValues.add(new This(sLength, readBytes(aChannel, sLength)));
    }

    /**
     * ä�ηκ��� ���̳ʸ� �÷��� length�� �޾ƿ� �����Ѵ�.
     * @param aChannel ��������� ���� ä�ΰ�ü
     * @return ���̳ʸ� �÷� length
     * @throws SQLException ä�ηκ��� ���������� binary length�� �������� ���� ���
     */
    private int readBinaryLength(CmChannel aChannel) throws SQLException
    {
        int sLength = nullLength();

        switch (mLengthSize)
        {
            case LENGTH_SIZE_INT8:
                sLength = aChannel.readUnsignedByte();
                break;
            case LENGTH_SIZE_INT16:
                sLength = aChannel.readUnsignedShort();
                break;
            case LENGTH_SIZE_INT32:
                sLength = aChannel.readInt();
                break;
            case LENGTH_SIZE_BINARY:
                sLength = aChannel.readInt();
                aChannel.readInt(); // skip dup length
                break;
            default:
                Error.throwInternalError(ErrorDef.INTERNAL_ASSERTION);
                break;
        }

        return sLength;
    }

    /**
     * ä�ηκ��� aLength �����ŭ ���̳ʸ� �����͸� �޾ƿ´�.
     * @param aChannel  ��������� ���� ä�� ��ü
     * @param aLength ���̳ʸ� �÷� length
     * @return ���̳ʸ� ������ �迭
     * @throws SQLException ���̳ʸ� �����͸� �޾ƿ��� ���� ������ �߻��� ���
     */
    private byte[] readBytes(CmChannel aChannel, int aLength) throws SQLException
    {
        byte[] sData;
        if (aLength == nullLength())
        {
            sData = new byte[0];
        }
        else
        {
            sData = new byte[toByteLength(aLength)];
            aChannel.readBytes(sData);
        }
        return sData;
    }

    protected void setNullValue()
    {
        mByteBuffer.clear().flip();
        mLength = nullLength();
        clear();
    }

    protected boolean isNullValueSet()
    {
        return (mLength == nullLength());
    }

    /**
     * @return ��� binary stream �� �ϳ��� 0�� �ƴ� ����Ʈ�� �����ϸ� true, �ƴϸ� false
     */
    protected boolean getBooleanSub() throws SQLException
    {
        mByteBuffer.rewind();
        while (mByteBuffer.hasRemaining())
        {
            if (mByteBuffer.get() != 0)
            {
                return true;
            }
        }
        return false;
    }

    protected short getShortSub() throws SQLException
    {
        short sResult = 0;
        mByteBuffer.rewind();
        try
        {
            sResult = mByteBuffer.getShort();
        }
        catch (Exception sEx)
        {
            Error.throwSQLException(ErrorDef.INVALID_DATA_CONVERSION, getStringSub(), "short", sEx);
        }
        throwErrorForInvalidDataConversion("short");
        return sResult;
    }

    protected int getIntSub() throws SQLException
    {
        int sResult = 0;
        mByteBuffer.rewind();
        try
        {
            sResult = mByteBuffer.getInt();
        }
        catch (Exception sEx)
        {
            Error.throwSQLException(ErrorDef.INVALID_DATA_CONVERSION, getStringSub(), "int", sEx);
        }
        throwErrorForInvalidDataConversion("int");
        return sResult;
    }

    protected long getLongSub() throws SQLException
    {
        long sResult = 0;
        mByteBuffer.rewind();
        try
        {
            sResult = mByteBuffer.getLong();
        }
        catch (Exception sEx)
        {
            Error.throwSQLException(ErrorDef.INVALID_DATA_CONVERSION, getStringSub(), "long", sEx);
        }
        throwErrorForInvalidDataConversion("long");
        return sResult;
    }

    protected float getFloatSub() throws SQLException
    {
        float sResult = 0;
        mByteBuffer.rewind();
        try
        {
            sResult = mByteBuffer.getFloat();
        }
        catch (Exception sEx)
        {
            Error.throwSQLException(ErrorDef.INVALID_DATA_CONVERSION, getStringSub(), "float", sEx);
        }
        throwErrorForInvalidDataConversion("float");
        return sResult;
    }

    protected double getDoubleSub() throws SQLException
    {
        double sResult = 0;
        mByteBuffer.rewind();
        try
        {
            sResult = mByteBuffer.getDouble();
        }
        catch (Exception sEx)
        {
            Error.throwSQLException(ErrorDef.INVALID_DATA_CONVERSION, getStringSub(), "double", sEx);
        }
        throwErrorForInvalidDataConversion("double");
        return sResult;
    }

    protected InputStream getAsciiStreamSub() throws SQLException
    {
        return new ByteArrayInputStream(getStringSub().getBytes());
    }

    protected Reader getCharacterStreamSub() throws SQLException
    {
        return new StringReader(getStringSub());
    }

    protected InputStream getBinaryStreamSub() throws SQLException
    {
        return new ByteArrayInputStream(getBytesSub());
    }
    
    private void throwErrorForInvalidDataConversion(String aType) throws SQLException
    {
        if (mByteBuffer.hasRemaining())
        {
            Error.throwSQLException(ErrorDef.INVALID_DATA_CONVERSION, getStringSub(), aType);
        }
    }
}
