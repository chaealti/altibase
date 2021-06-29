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

package Altibase.jdbc.driver.util;

import Altibase.jdbc.driver.ex.Error;
import Altibase.jdbc.driver.ex.ErrorDef;

/**
 * nibble array�� ���� ��ƿ��Ƽ Ŭ����.
 * <p>
 * nibble array�� nibble ���� �� ����Ʈ�� �ϴ� byte[]��.
 * ��, nibble ����Ÿ 'F0123'�� nibble array�� byte[]{0xF, 0x1, 0x2, 0x3}�� ǥ���Ѵ�.
 * �� ���� ���ڿ��� 'F0123'ó�� ǥ���Ѵ�.
 */
public final class NibbleUtils
{
    private static final byte[] EMTRY_BYTE_ARRAY = new byte[0];

    private NibbleUtils()
    {
    }

    /**
     * nibble ������ Ȯ���Ѵ�.
     * 
     * @param aNibble Ȯ���� ��
     * @return nibble ���̸� true, �ƴϸ� false
     */
    public static boolean isNibble(int aNibble)
    {
        return (0 <= aNibble) && (aNibble <= 0xF);
    }

    /**
     * ��ȿ�� nibble array���� Ȯ���Ѵ�.
     *
     * @param aNibbleArray Ȯ���� byte array
     * @return byte array�� �� ��� ���� ��� nibble�̸� true, �ƴϸ� false
     */
    public static boolean isValid(byte[] aNibbleArray)
    {
        if (aNibbleArray == null)
        {
            return true;
        }

        for (int i = 0; i < aNibbleArray.length; i++)
        {
            if (!isNibble(aNibbleArray[i]))
            {
                return false;
            }
        }
        return true;
    }

    /**
     * hex string�� nibble array�� ��ȯ�Ѵ�.
     * 
     * @param aHexString nibble array�� ��ȯ�� hex string
     * @return ��ȯ�� nibble array. hex string�� null�̸� null, �� ���ڿ��̸� ���̰� 0�� �迭.
     * @exception IllegalArgumentException hex string�� �ùٸ��� ���� ���
     */
    public static byte[] parseNibbleArray(String aHexString)
    {
        if (aHexString == null)
        {
            return null;
        }

        byte[] sBuf = new byte[aHexString.length()];
        for (int i=0; i<aHexString.length(); i++)
        {
            char c = Character.toLowerCase(aHexString.charAt(i));
            if (!ByteUtils.isHexCharacter(c))
            {
                Error.throwIllegalArgumentException(ErrorDef.INVALID_HEX_STRING_ELEMENT,
                                                    String.valueOf(i),
                                                    String.valueOf(c));
            }
            int v = (c < 'a') ? (c - '0') : (10 + c - 'a');
            sBuf[i] = (byte)v;
        }
        return sBuf;
    }

    /**
     * nibble array�� byte array�� ��ȯ�Ѵ�.
     *
     * @param aNibbleArray ��ȯ�� nibble array
     * @return ��ȯ�� byte array
     * @exception IllegalArgumentException ��ȿ�� nibble array�� �ƴ� ���
     */
    public static byte[] toByteArray(byte[] aNibbleArray)
    {
        if (aNibbleArray == null)
        {
            return null;
        }
        if (aNibbleArray.length == 0)
        {
            return EMTRY_BYTE_ARRAY;
        }
        for (int i = 0; i < aNibbleArray.length; i++)
        {
            if (!isNibble(aNibbleArray[i]))
            {
                Error.throwIllegalArgumentException(ErrorDef.INVALID_NIBBLE_ARRAY_ELEMENT,
                                                    String.valueOf(i),
                                                    String.valueOf(aNibbleArray[i]));
            }
        }

        int sBufLen = aNibbleArray.length / 2;
        if ((aNibbleArray.length % 2) == 1)
        {
            sBufLen++;
        }
        byte[] sBuf = new byte[sBufLen];
        for (int i = 0; i < aNibbleArray.length; i++)
        {
            int v = aNibbleArray[i];
            if ((i % 2) == 0)
            {
                v = v << 4;
            }
            sBuf[i / 2] |= v;
        }
        return sBuf;
    }

    /**
     * byte array�� nibble array�� ��ȯ�Ѵ�.
     *
     * @param aByteArray nibble array�� ��ȯ�� byte array
     * @return ��ȯ�� nibble array
     */
    public static byte[] fromByteArray(byte[] aByteArray)
    {
        return fromByteArray(aByteArray, Integer.MAX_VALUE);
    }

    /**
     * byte array�� nibble array�� ��ȯ�Ѵ�.
     * <p>
     * max nibble length�� �����ϸ� byte array�κ��� ������ nibble array�� ���̸� ������ �� �ִ�.
     * ��, max nibble length�� �ִ� ���̸� �����ϴµ��� ����ϹǷ�,
     * �־��� byte array�� ���� �� �ִ� nibble array�� ���̰� max nibble array���� �۴ٸ�
     * �׸�ŭ�� ���̸� ���� nibble array�� �����.
     *
     * @param aByteArray nibble array�� ��ȯ�� byte array
     * @param aMaxNibbleLength nibble array�� �ִ� ����
     * @return ��ȯ�� nibble array
     * @exception IllegalArgumentException max nibble length�� 0���� ���� ���
     */
    public static byte[] fromByteArray(byte[] aByteArray, int aMaxNibbleLength)
    {
        if (aMaxNibbleLength < 0)
        {
            Error.throwIllegalArgumentException(ErrorDef.INVALID_ARGUMENT,
                                                "Max nibble length",
                                                "0 ~ Integer.MAX_VALUE",
                                                String.valueOf(aMaxNibbleLength));
        }
        if (aByteArray == null)
        {
            return null;
        }
        if ((aMaxNibbleLength == 0) || (aByteArray.length == 0))
        {
            return EMTRY_BYTE_ARRAY;
        }

        int sByteLen = Math.min(aMaxNibbleLength, aByteArray.length * 2);
        byte[] sNibbleArray = new byte[sByteLen];
        for (int i = 0; i < sByteLen; i++)
        {
            int v = ((i % 2) == 0)
                  ? ((aByteArray[i / 2] & 0xF0) >>> 4)
                  : ((aByteArray[i / 2] & 0x0F));
            sNibbleArray[i] = (byte)v;
        }
        return sNibbleArray;
    }

    /**
     * nibble array�� hex string���� ��ȯ�Ѵ�.
     * 
     * @param aNibbleArray hex string���� ��ȯ�� nibble array
     * @return ��ȯ�� hex string. nibble array�� null�̸� "null", ���̰� 0�̸� �� ���ڿ�.
     */
    public static String toHexString(byte[] aNibbleArray)
    {
        return toHexString(aNibbleArray, 0);
    }

    /**
     * nibble array�� hex string���� ��ȯ�Ѵ�.
     * 
     * @param aNibbleArray hex string���� ��ȯ�� nibble array
     * @param aSpacingBase ������ ������ ����. 0�̸� ������ ������ �ʴ´�.
     * @return ��ȯ�� hex string. nibble array�� null�̸� "null", ���̰� 0�̸� �� ���ڿ�.
     */
    public static String toHexString(byte[] aNibbleArray, int aSpacingBase)
    {
        return toHexString(aNibbleArray, aSpacingBase, " ");
    }

    /**
     * nibble array�� hex string���� ��ȯ�Ѵ�.
     * 
     * @param aNibbleArray hex string���� ��ȯ�� nibble array
     * @param aAppendingBase aAppendingChar�� ������ ����. 0�̸� ������ �ʴ´�.
     * @param aAppendingString aAppendingBase ���� �߰��� ���ڿ�
     * @return ��ȯ�� hex string. nibble array�� null�̸� "null", ���̰� 0�̸� �� ���ڿ�.
     * @exception IllegalArgumentException array�� nibble ����(0x0~0xF)�� �Ѿ�� ���� ���� ���
     */
    public static String toHexString(byte[] aNibbleArray, int aAppendingBase, String aAppendingString)
    {
        return toHexString(aNibbleArray, 0, (aNibbleArray == null) ? 0 : aNibbleArray.length, aAppendingBase, aAppendingString);
    }

    /**
     * nibble array�� hex string���� ��ȯ�Ѵ�.
     * 
     * @param aNibbleArray hex string���� ��ȯ�� nibble array
     * @param aStartIdx ���� index (inclusive)
     * @param aEndFence �� index (exclusive)
     * @return ��ȯ�� hex string. nibble array�� null�̸� "null", ���̰� 0�̸� �� ���ڿ�.
     */
    public static String toHexString(byte[] aNibbleArray, int aStartIdx, int aEndFence)
    {
        return toHexString(aNibbleArray, aStartIdx, aEndFence, 0, " ");
    }

    /**
     * nibble array�� hex string���� ��ȯ�Ѵ�.
     * 
     * @param aNibbleArray hex string���� ��ȯ�� nibble array
     * @param aStartIdx ���� index (inclusive)
     * @param aEndIndex �� index (exclusive)
     * @param aAppendingBase aAppendingChar�� ������ ����. 0�̸� ������ �ʴ´�.
     * @param aAppendingString aAppendingBase ���� �߰��� ���ڿ�
     * @return ��ȯ�� hex string. nibble array�� null�̸� "null", ���̰� 0�̸� �� ���ڿ�.
     * @exception IllegalArgumentException array�� nibble ����(0x0~0xF)�� �Ѿ�� ���� ���� ���
     */
    public static String toHexString(byte[] aNibbleArray, int aStartIdx, int aEndIndex, int aAppendingBase, String aAppendingString)
    {
        if (aNibbleArray == null)
        {
            return "null";
        }
        if (aStartIdx < 0 || aStartIdx > aNibbleArray.length)
        {
            Error.throwIllegalArgumentException(ErrorDef.INVALID_ARGUMENT,
                                                "Start index",
                                                "0 ~ " + aNibbleArray.length,
                                                String.valueOf(aStartIdx));
        }
        if (aEndIndex < aStartIdx || aEndIndex > aNibbleArray.length)
        {
            Error.throwIllegalArgumentException(ErrorDef.INVALID_ARGUMENT,
                                                "End index",
                                                aStartIdx + " ~ " + aNibbleArray.length,
                                                String.valueOf(aEndIndex));
        }
        if (aEndIndex == aStartIdx)
        {
            return "";
        }

        int sBufSize = aEndIndex - aStartIdx;
        if (aAppendingString == null)
        {
            aAppendingBase = 0;
        }
        else if (aAppendingBase > 0)
        {
            sBufSize += (aNibbleArray.length % aAppendingBase) * aAppendingString.length();
        }

        StringBuffer sBuf = new StringBuffer(sBufSize);
        for (int i = aStartIdx; i < aEndIndex; i++)
        {
            if (!isNibble(aNibbleArray[i]))
            {
                Error.throwIllegalArgumentException(ErrorDef.INVALID_NIBBLE_ARRAY_ELEMENT,
                                                    String.valueOf(i),
                                                    String.valueOf(aNibbleArray[i]));
            }
            if ((aAppendingBase > 0) && (i > 0) && (i % aAppendingBase == 0))
            {
                sBuf.append(aAppendingString);
            }
            sBuf.append(Character.forDigit(aNibbleArray[i], 16));
        }
        return sBuf.toString();
    }
}
