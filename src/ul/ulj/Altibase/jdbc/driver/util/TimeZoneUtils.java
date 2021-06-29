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

import java.util.Calendar;
import java.util.TimeZone;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

public final class TimeZoneUtils
{
    public static final TimeZone TZ_GMT             = TimeZone.getTimeZone("GMT");
    public static final TimeZone TZ_SEOUL           = TimeZone.getTimeZone("Asia/Seoul");
    public static final TimeZone TZ_LONDON          = TimeZone.getTimeZone("Europe/London");
    public static final TimeZone TZ_NEWYORK         = TimeZone.getTimeZone("America/New_York");
    public static final TimeZone TZ_HONGKONG        = TimeZone.getTimeZone("Asia/Hong_Kong");
    public static final TimeZone TZ_SHANGHAI        = TimeZone.getTimeZone("Asia/Shanghai");
    public static final TimeZone TZ_SINGAPORE       = TimeZone.getTimeZone("Asia/Singapore");

    private static final Pattern NUMERIC_TZ_PATTERN = Pattern.compile("^([+-])([0-9]{1,2}):([0-9]{1,2})$");

    private TimeZoneUtils()
    {
    }

    /**
     * ID�� �ش��ϴ� TimeZone�� ��´�.
     * 
     * @param aID "Asia/Seoul"�� ���� ID ���ڿ��̳� "GMT+09:00"�� ���� offset ���ڿ�.
     *            "+09:00"�� ���� �����̸� �ڵ����� "GMT+09:00"�� ���� TimeZone�� ��´�.
     * @return ID�� �ش��ϴ� TimeZone.
     *         ����, �ش��ϴ� TimeZone�� ������ GMT zone.
     */
    public static TimeZone getTimeZone(String aID)
    {
        Matcher sMatcher = NUMERIC_TZ_PATTERN.matcher(aID);
        if (sMatcher.find())
        {
            // Java���� �˾����� �� �ִ� ���·� ����
            aID = "GMT" + aID;
        }
        return TimeZone.getTimeZone(aID);
    }

    /**
     * TimeZone�� ��ȯ�Ѵ�.
     * 
     * @param aMillis �ٲ� �ð�(ms ����)
     * @param aSrcCal ���� TimeZone�� ������ Calendar
     * @param aDstCal �ٲ� TimeZone�� ������ Calendar
     * @return TimeZone ��ȯ�� milliseconds ��
     */
    public static long convertTimeZone(long aMillis, Calendar aSrcCal, Calendar aDstCal)
    {
        aSrcCal.setTimeInMillis(aMillis);

        aDstCal.set(Calendar.YEAR, aSrcCal.get(Calendar.YEAR));
        aDstCal.set(Calendar.MONTH, aSrcCal.get(Calendar.MONTH));
        aDstCal.set(Calendar.DATE, aSrcCal.get(Calendar.DATE));
        aDstCal.set(Calendar.HOUR_OF_DAY, aSrcCal.get(Calendar.HOUR_OF_DAY));
        aDstCal.set(Calendar.MINUTE, aSrcCal.get(Calendar.MINUTE));
        aDstCal.set(Calendar.SECOND, aSrcCal.get(Calendar.SECOND));
        aDstCal.set(Calendar.MILLISECOND, aSrcCal.get(Calendar.MILLISECOND));

        return aDstCal.getTimeInMillis();
    }

    /**
     * TimeZone�� ��ȯ�Ѵ�.
     * 
     * @param aMillis �ٲ� �ð�(ms ����)
     * @param aDstCal �ٲ� TimeZone�� ������ Calendar
     * @return TimeZone ��ȯ�� milliseconds ��
     */
    public static long convertTimeZone(long aMillis, Calendar aDstCal)
    {
        return convertTimeZone(aMillis, Calendar.getInstance(), aDstCal);
    }

    /**
     * TimeZone�� ��ȯ�Ѵ�.
     * 
     * @param aMillis �ٲ� �ð�(ms ����)
     * @param sSrcCal ���� TimeZone
     * @param aDstCal �ٲ� TimeZone
     * @return TimeZone ��ȯ�� milliseconds ��
     */
    public static long convertTimeZone(long aMillis, TimeZone aSrcTZ, TimeZone aDstTZ)
    {
        return convertTimeZone(aMillis, Calendar.getInstance(aSrcTZ), Calendar.getInstance(aDstTZ));
    }

    /**
     * TimeZone�� ��ȯ�Ѵ�.
     * 
     * @param aMillis �ٲ� �ð�(ms ����)
     * @param aDstCal �ٲ� TimeZone
     * @return TimeZone ��ȯ�� milliseconds ��
     */
    public static long convertTimeZone(long aMillis, TimeZone aDstTZ)
    {
        return convertTimeZone(aMillis, Calendar.getInstance(aDstTZ));
    }
}
