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

import java.nio.ByteBuffer;

import Altibase.jdbc.driver.ex.Error;
import Altibase.jdbc.driver.ex.ErrorDef;

public final class ByteUtils
{
    private static final char[] HEX_LITERALS_L   = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f' };
    private static final char[] HEX_LITERALS_U   = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F' };

    private ByteUtils()
    {
    }

    /**
     * Hex String���� �� �� �ִ� �������� Ȯ���Ѵ�.
     *
     * @param c Ȯ���� ����
     * @return [0-9a-fA-F]�̸� true, �ƴϸ� false
     */
    public static boolean isHexCharacter(char c)
    {
        if (('0' <= c && c <= '9') ||
            ('a' <= c && c <= 'f') ||
            ('A' <= c && c <= 'F'))
        {
            return true;
        }
        return false;
    }

    /**
     * hex string�� byte array�� ��ȯ�Ѵ�.
     * <p>
     * ���� hex string�� ¦���� �ƴ϶��, ������ ����Ʈ�� ���� 4bit�� 0���� ä���.
     * 
     * @param aHexString byte array�� ��ȯ�� hex string
     * @return ��ȯ�� byte array. hex string�� null�̸� null, �� ���ڿ��̸� ���̰� 0�� �迭.
     */
    public static byte[] parseByteArray(String aHexString)
    {
        return parseByteArray(aHexString, true);
    }

    /**
     * hex string�� byte array�� ��ȯ�Ѵ�.
     * <p>
     * hex string�� ���̰� 2�� ����� �ƴҶ�, 0���� �е��� ���� �ְ� ���ܸ� �� ���� �ִ�.
     * ���� 2�� ����� �ƴ� ��, �е��� ����Ѵٸ� ������ ����Ʈ�� ���� 4bit�� 0���� ä���,
     * �е��� ������� �ʴ´ٸ� ���ܸ� ������.
     *
     * @param aHexString byte array�� ��ȯ�� hex string
     * @param aUsePadding hex string�� 2�� ����� �ƴ� ��, 0���� �е����� ����.
     * @return ��ȯ�� byte array. hex string�� null�̸� null, �� ���ڿ��̸� ���̰� 0�� �迭.
     * @exception IllegalArgumentException hex string�� �ùٸ��� ���� ���
     * @exception IllegalArgumentException �е��� ������� ���� ��, hex string�� 2�� ����� �ƴ� ���
     */
    public static byte[] parseByteArray(String aHexString, boolean aUsePadding)
    {
        if (aHexString == null)
        {
            return null;
        }

        int sBufSize = aHexString.length() / 2;
        if ((aHexString.length() % 2) == 1)
        {
            if (aUsePadding)
            {
                sBufSize++;
            }
            else
            {
                Error.throwIllegalArgumentException(ErrorDef.INVALID_HEX_STRING_LENGTH);
            }
        }
        byte[] sBuf = new byte[sBufSize];
        for (int i = 0; i < aHexString.length(); i++)
        {
            char c = aHexString.charAt(i);
            if (!ByteUtils.isHexCharacter(c))
            {
                Error.throwIllegalArgumentException(ErrorDef.INVALID_HEX_STRING_ELEMENT, String.valueOf(i), String.valueOf(c));
            }
            c |= 0x20; // to lowercase. hext char���� Ȯ�������Ƿ� �̷��� �ص� �ȴ�.
            int v = (c < 'a') ? (c - '0') : (10 + c - 'a');
            if ((i % 2) == 0)
            {
                v = v << 4;
            }
            sBuf[i / 2] |= v;
        }
        return sBuf;
    }

    /**
     * byte array�� hex string���� ��ȯ�Ѵ�.
     * 
     * @param aByteArray hex string���� ��ȯ�� byte array
     * @return ��ȯ�� hex string. byte array�� null�̸� "null", ���̰� 0�̸� �� ���ڿ�.
     */
    public static String toHexString(byte[] aByteArray)
    {
        return toHexString(aByteArray, 0);
    }

    /**
     * byte array�� hex string���� ��ȯ�Ѵ�.
     * 
     * @param aByteArray hex string���� ��ȯ�� byte array
     * @param aStartIdx ù index (inclusive)
     * @param aEndIdx �� index (exclusive)
     * @return ��ȯ�� hex string. byte array�� null�̸� "null", ���̰� 0�̸� �� ���ڿ�.
     * @exception IllegalArgumentException ���ڰ� �ùٸ��� ���� ���
     */
    public static String toHexString(byte[] aByteArray, int aStartIdx, int aEndIdx)
    {
        return toHexString(aByteArray, aStartIdx, aEndIdx, 0, null, false);
    }

    /**
     * byte array�� hex string���� ��ȯ�Ѵ�.
     * 
     * @param aByteArray hex string���� ��ȯ�� byte array
     * @param aSpacingBase ������ ������ ����. 0�̸� ������ ������ �ʴ´�.
     * @return ��ȯ�� hex string. byte array�� null�̸� "null", ���̰� 0�̸� �� ���ڿ�.
     */
    public static String toHexString(byte[] aByteArray, int aSpacingBase)
    {
        return toHexString(aByteArray, aSpacingBase, " ");
    }

    /**
     * byte array�� hex string���� ��ȯ�Ѵ�.
     * 
     * @param aByteArray hex string���� ��ȯ�� byte array
     * @param aAppendingBase aAppendingChar�� ������ ����. 0�̸� ������ �ʴ´�.
     * @param aAppendingString aAppendingBase ���� �߰��� ���ڿ�
     * @return ��ȯ�� hex string. byte array�� null�̸� "null", ���̰� 0�̸� �� ���ڿ�.
     */
    public static String toHexString(byte[] aByteArray, int aAppendingBase, String aAppendingString)
    {
        return toHexString(aByteArray, 0, 0, aAppendingBase, aAppendingString, false);
    }

    /**
     * byte array�� hex string���� ��ȯ�Ѵ�.
     * 
     * @param aByteArray hex string���� ��ȯ�� byte array
     * @param aStartIdx ù index (inclusive)
     * @param aEndIdx �� index (exclusive)
     * @param aAppendingBase aAppendingChar�� ������ ����. 0�̸� ������ �ʴ´�.
     * @param aAppendingString aAppendingBase ���� �߰��� ���ڿ�
     * @param aToUpper Upper case�� ��ȯ�Ұ����� ����
     * @return ��ȯ�� hex string. byte array�� null�̸� "null", ���̰� 0�̸� �� ���ڿ�.
     * @exception IllegalArgumentException ���ڰ� �ùٸ��� ���� ���
     */
    public static String toHexString(byte[] aByteArray, int aStartIdx, int aEndIdx, int aAppendingBase, String aAppendingString, boolean aToUpper)
    {
        if (aByteArray == null)
        {
            return "null";
        }
        if (aByteArray.length == 0)
        {
            return "";
        }
        if (aEndIdx == 0)
        {
            aEndIdx = aByteArray.length;
        }
        if (aEndIdx == aStartIdx)
        {
            return "";
        }
        if (aStartIdx < 0)
        {
            Error.throwIllegalArgumentException(ErrorDef.INVALID_ARGUMENT,
                                                "Start index",
                                                "0 ~ Integer.MAX_VALUE",
                                                String.valueOf(aStartIdx));
        }
        if (aEndIdx < aStartIdx || aByteArray.length < aEndIdx)
        {
            Error.throwIllegalArgumentException(ErrorDef.INVALID_ARGUMENT,
                                                "End index",
                                                aStartIdx + " ~ " + aByteArray.length,
                                                String.valueOf(aEndIdx));
        }

        int sBufSize = (aEndIdx - aStartIdx) * 2;
        if (aAppendingString == null)
        {
            aAppendingBase = 0;
        }
        else if (aAppendingBase > 0)
        {
            sBufSize += (aByteArray.length % aAppendingBase) * aAppendingString.length();
        }

        final char[] HEX_LITERALS = aToUpper ? HEX_LITERALS_U : HEX_LITERALS_L;
        StringBuffer sBuf = new StringBuffer(sBufSize);
        for (int i = aStartIdx; i < aEndIdx; i++)
        {
            if ((aAppendingBase > 0) && (i > 0) && (i % aAppendingBase == 0))
            {
                sBuf.append(aAppendingString);
            }
            sBuf.append(HEX_LITERALS[(aByteArray[i] & 0xF0) >>> 4]);
            sBuf.append(HEX_LITERALS[(aByteArray[i] & 0x0F)]);
        }
        return sBuf.toString();
    }

    /**
     * ByteBuffer�� hex string���� ��ȯ�Ѵ�.
     * 
     * @param aByteArray hex string���� ��ȯ�� ByteBuffer
     * @return ��ȯ�� hex string. byte array�� null�̸� "null", ���̰� 0�̸� �� ���ڿ�.
     */
    public static String toHexString(ByteBuffer mByteBuffer)
    {
        if (mByteBuffer == null)
        {
            return "null";
        }
        return toHexString(mByteBuffer, 0, mByteBuffer.remaining());
    }

    /**
     * ByteBuffer�� hex string���� ��ȯ�Ѵ�.
     * 
     * @param aByteArray hex string���� ��ȯ�� ByteBuffer
     * @param aStartIdx ù index (inclusive)
     * @param aEndIdx �� index (exclusive)
     * @return ��ȯ�� hex string. byte array�� null�̸� "null", ���̰� 0�̸� �� ���ڿ�.
     */
    public static String toHexString(ByteBuffer mByteBuffer, int aStartIdx, int aEndIdx)
    {
        if (mByteBuffer == null)
        {
            return "null";
        }
        if (mByteBuffer.remaining() == 0)
        {
            return "";
        }

        if (mByteBuffer.hasArray())
        {
            return toHexString(mByteBuffer.array(), mByteBuffer.position() + aStartIdx, mByteBuffer.position() + aEndIdx);
        }
        else
        {
            byte[] sBuf = new byte[mByteBuffer.remaining()];
            int sOrgPos = mByteBuffer.position();
            mByteBuffer.get(sBuf);
            mByteBuffer.position(sOrgPos);
            return toHexString(sBuf, aStartIdx, aEndIdx);
        }
    }
}
