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

public final class StringUtils
{
    private StringUtils()
    {
    }

    /**
     * ���ڿ��� null�̰ų� ������� Ȯ���Ѵ�.
     * 
     * @param aStr Ȯ���� ���ڿ�
     * @return null �Ǵ� ���̰� 0�� ���ڿ����� ����
     */
    public static boolean isEmpty(String aStr)
    {
        return (aStr == null) || (aStr.length() == 0);
    }

    /**
     * ��ҹ��� ��������, starts string���� �����ϴ��� Ȯ���Ѵ�.
     * 
     * @param aSrcStr Ȯ���� ���ڿ�
     * @param aStartsStr ���� ���ڿ�
     * @return starts string���� �����ϸ� true, �ƴϸ� false
     */
    public static boolean startsWithIgnoreCase(String aSrcStr, String aStartsStr)
    {
        return startsWithIgnoreCase(aSrcStr, 0, aStartsStr);
    }

    /**
     * ��ҹ��� ��������, �� ���ڿ��� ���Ѵ�.
     * 
     * @param aSrcStr Ȯ���� ���ڿ�
     * @param aStartIdx Ȯ���� ���ڿ����� �񱳸� ������ ��ġ(0 base)
     * @param aStartsStr ���� ���ڿ�
     * @return start string���� �����ϸ� true, �ƴϸ� false
     */
    public static boolean startsWithIgnoreCase(String aSrcStr, int aStartIdx, String aStartsStr)
    {
        return compareIgnoreCase(aSrcStr, aStartIdx, aStartsStr, 0, aStartsStr.length()) == 0;
    }

    /**
     * ��ҹ��� ��������, ends string���� �������� Ȯ���Ѵ�.
     * 
     * @param aSrcStr Ȯ���� ���ڿ�
     * @param aEndsStr �� ���ڿ�
     * @return ends string���� ������ true, �ƴϸ� false
     */
    public static boolean endsWithIgnoreCase(String aSrcStr, String aEndsStr)
    {
        int sPos = aSrcStr.length() - aEndsStr.length();
        return (sPos < 0) ? false : startsWithIgnoreCase(aSrcStr, sPos, aEndsStr);
    }

    /**
     * ��ҹ��� ��������, �� ���ڿ��� ���Ѵ�.
     * 
     * @param aStr1 ���ڿ� 1
     * @param aStr2 ���ڿ� 2
     * @return ������ 0, aStr1�� ũ�ų� aStr2�� ��� �����ϸ鼭 ��� ���, �ƴϸ� ����
     */
    public static int compareIgnoreCase(String aStr1, String aStr2)
    {
        return compareIgnoreCase(aStr1, 0, aStr2);
    }

    /**
     * ��ҹ��� ��������, �� ���ڿ��� ���Ѵ�.
     * 
     * @param aStr1 ���ڿ� 1
     * @param aStartIdx1 aStr1���� �񱳸� ������ ��ġ(0 base)
     * @param aStr2 ���ڿ� 2
     * @return ������ 0, aStr1�� ũ�ų� aStr2�� ��� �����ϸ鼭 ��� ���, �ƴϸ� ����
     */
    public static int compareIgnoreCase(String aStr1, int aStartIdx1, String aStr2)
    {
        int sResult = compareIgnoreCase(aStr1, aStartIdx1, aStr2, 0, Math.min(aStr1.length(), aStr2.length()));
        if (sResult == 0)
        {
            // �� ������ �κ��� ��� ���ٸ�, ���� �� ���� ũ��.
            sResult = (aStr1.length() - aStartIdx1) - aStr2.length();
            sResult = (sResult > 0) ? 1 : (sResult < 0) ? -1 : 0;
        }
        return sResult;
    }

    /**
     * ��ҹ��� ��������, �� ���ڿ��� ���Ѵ�.
     * 
     * @param aStr1 ���ڿ� 1
     * @param aStartIdx1 aStr1���� �񱳸� ������ ��ġ(0 base)
     * @param aStr2 ���ڿ� 2
     * @param aStartIdx2 aStr2���� �񱳸� ������ ��ġ(0 base)
     * @param aLength ���� ����
     * @return ������ 0, aStr1�� ũ�� ���, �ƴϸ� ����
     */
    public static int compareIgnoreCase(String aStr1, int aStartIdx1, String aStr2, int aStartIdx2, int aLength)
    {
        if ((aStr1 == aStr2) && (aStartIdx1 == aStartIdx2))
        {
            return 0;
        }

        int sStrLen1 = aStr1.length() - aStartIdx1;
        int sStrLen2 = aStr2.length() - aStartIdx2;
        if (sStrLen1 >= aLength && sStrLen2 < aLength)
        {
            return 1;
        }
        if (sStrLen1 < aLength && sStrLen2 >= aLength)
        {
            return -1;
        }

        for (int i = 0; i < aLength; i++)
        {
            char sChA = aStr1.charAt(aStartIdx1 + i);
            char sChB = aStr2.charAt(aStartIdx2 + i);
            int sCmp = Character.toLowerCase(sChA) - Character.toLowerCase(sChB);
            if (sCmp != 0)
            {
                return (sCmp > 0) ? 1 : -1;
            }
        }
        return 0;
    }
}
