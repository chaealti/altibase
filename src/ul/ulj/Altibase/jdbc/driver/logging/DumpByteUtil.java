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

package Altibase.jdbc.driver.logging;

/**
 * FINEST������ ��Ʈ��ũ ��Ŷ������ ���涧 ����Ʈ ������ ����ϱ� ���� ����Ŭ����
 * 
 * @author yjpark
 * 
 */
public final class DumpByteUtil
{
    // cm protocol�� header��Ŷ ũ�Ⱑ 16�̱⶧���� �⺻���� 16���� �Ѵ�.
    private static final int     DEFAULT_BYTES_PER_LINE = 16;
    private static final int     HEX_LIST_SIZE          = 256;
    public static final String[] HEX_VALUES             = makeHexValues();
    public static final char[]   CHAR_VALUES            = makeCharValues();
    
    // utility class�̱⶧���� �����ڸ� private���� ����
    private DumpByteUtil()
    {
    }

    private static String[] makeHexValues()
    {
        String[] sHexValues = new String[HEX_LIST_SIZE];
        StringBuffer sSb = new StringBuffer();
        for (int i=0; i < sHexValues.length; i++)
        {
            sSb.delete(0, sSb.length());
            sSb.append(' ');
            String sHex = Integer.toHexString(i).toUpperCase();
            
            if (sHex.length() < 2)
            {
                sSb.append('0').append(sHex);
            }
            else 
            {
                sSb.append(sHex);
            }
            sHexValues[i] = sSb.toString();
        }
        
        return sHexValues;
    }
    
    private static char[] makeCharValues()
    {
        char[] sCharValues = new char[HEX_LIST_SIZE];
        for (int i=0; i < sCharValues.length; i++)
        {
            if (i < (int)' ' || i > (int)'~') // ȭ�鿡 ǥ���� �� ���� ���ڴ� �׳� '.'���� ǥ���Ѵ�.
            {
                sCharValues[i] = '.';
            }
            else
            {
                sCharValues[i] = (char)i;
            }
        }
        
        return sCharValues;
    }

    public static String dumpBytes(byte[] aByteArry, int aStart, int aLength)
    {
        return dumpBytes(aByteArry, aStart, aLength, DEFAULT_BYTES_PER_LINE);
    }

    private static String dumpBytes(byte[] aByteArry, int aStart, int aLength, int aBytesPerLine)
    {
        if (aByteArry == null)
        {
            return "NULL";
        }

        int sLength = aStart + aLength;
        int sIdx = aStart;
        int sBytesThisLine = 0;

        StringBuffer sByteSb = new StringBuffer();
        StringBuffer sCharSb = new StringBuffer();

        for (; sIdx < sLength; sIdx++)
        {
            int sTmp = aByteArry[sIdx] & 0xFF;
            sByteSb.append(HEX_VALUES[sTmp]);
            sCharSb.append(CHAR_VALUES[sTmp]);

            sBytesThisLine++;

            if (sBytesThisLine == aBytesPerLine)
            {
                sByteSb.append("     |");
                sByteSb.append(sCharSb.substring(0, sCharSb.length()));
                sByteSb.append("|\n");
                sCharSb.delete(0, sCharSb.length());
                sBytesThisLine = 0;
            }
        }

        if (sBytesThisLine > 0)
        {
            int spaces = aBytesPerLine - sBytesThisLine - 1;

            for (int i = 0; i <= spaces; i++)
            {
                sByteSb.append("   ");
            }
            sByteSb.append("     |");
            sByteSb.append(sCharSb.substring(0, sCharSb.length()));
            for (int i = 0; i <= spaces; i++)
            {
                sByteSb.append(' ');
            }
            sByteSb.append("|\n");
            sCharSb.delete(0, sCharSb.length());
        }

        return sByteSb.substring(0, sByteSb.length());
    }
}
