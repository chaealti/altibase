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

import java.nio.ByteBuffer;
import java.sql.Date;
import java.sql.SQLException;
import java.sql.Timestamp;
import java.text.SimpleDateFormat;
import java.util.Calendar;

import Altibase.jdbc.driver.AltibaseTypes;
import Altibase.jdbc.driver.ex.Error;
import Altibase.jdbc.driver.ex.ErrorDef;

public class TimestampColumn extends CommonDateTimeColumn
{
    TimestampColumn()
    {
        addMappedJdbcTypeSet(AltibaseTypes.TIMESTAMP);
    }

    public int getDBColumnType()
    {
        return ColumnTypes.TIMESTAMP;
    }

    public String getObjectClassName()
    {
        return Timestamp.class.getName();
    }

    public int getMaxDisplaySize()
    {
        return "yyyy-mm-dd hh:mm:ss.fffffffff".length();
    }

    /*
     * �� �޼ҵ��� ������ ������ ����.
     * 
     * [0]~[1] : ����
     * [2]     : ��
     * [3]     : ��
     * [4]     : ��
     * [5]     : ��
     * [6]     : ��
     * [7]~[10]: ������
     */
    protected byte[] getBytesSub() throws SQLException
    {
        ByteBuffer sResult = ByteBuffer.allocate(11);
        mCalendar.setTimeInMillis(getTimeInMillis());
        sResult.putShort((short) mCalendar.get(Calendar.YEAR));
        sResult.put((byte) mCalendar.get(Calendar.MONTH));
        sResult.put((byte) mCalendar.get(Calendar.DATE));
        sResult.put((byte) mCalendar.get(Calendar.HOUR_OF_DAY));
        sResult.put((byte) mCalendar.get(Calendar.MINUTE));
        sResult.put((byte) mCalendar.get(Calendar.SECOND));
        sResult.putInt(getNanos());
        return sResult.array();
    }

    protected String getStringSub() throws SQLException
    {
        String sResult;
        Timestamp sTimestamp = getTimestampSub();
        String sDateFormat = getDateFormat();
        /* BUG-46513 date_format�� null�̶�� timestamp�� toString�� ȣ���ϰ� �׷��� ���� ��쿡��
           SimpleDateFormat���� ���� formatting�Ѵ�.  */
        if (sDateFormat == null)
        {
            sResult = sTimestamp.toString();
        }
        else
        {
            try
            {
                SimpleDateFormat sFormat = new SimpleDateFormat(sDateFormat);
                sResult = sFormat.format(sTimestamp);
            }
            catch (IllegalArgumentException aEx)
            {
                // BUG-46513 formatting�� ������ �߻��� ��쿡�� ���� timestamp��ü�� toString�� ȣ���Ѵ�.
                sResult = sTimestamp.toString();
            }
        }

        return sResult;
    }

    protected Object getObjectSub() throws SQLException
    {
        return getTimestampSub();
    }

    protected void setValueSub(Object aValue) throws SQLException
    {
        if (aValue instanceof CommonDateTimeColumn)
        {
            setColumn((CommonDateTimeColumn)aValue);
            return;
        }

        long sTimeInMillis = 0;
        int sNanos = 0;
        if (aValue instanceof Timestamp)
        {
            Timestamp sTS = (Timestamp)aValue;
            sTimeInMillis = sTS.getTime();
            sNanos = sTS.getNanos();
        }
        else if (aValue instanceof Date)
        {
            // Oracle�� Date Ÿ������ ���� ������ �� yyyy-mm-dd�� �޾Ƶ��δ�.
            mCalendar.setTime((Date)aValue);
            int sYY = mCalendar.get(Calendar.YEAR);
            int sMM = mCalendar.get(Calendar.MONTH);
            int sDD = mCalendar.get(Calendar.DATE);
            mCalendar.clear();
            mCalendar.set(sYY, sMM, sDD);
            sTimeInMillis = mCalendar.getTimeInMillis();
        }
        // Oracle�� Timestamp�� Time�� ���� �� ����.
/*
        else if (aValue instanceof Time)
        {
            sTimeInMillis = ((Time)aValue).getTime();
        }
*/
        else if (aValue instanceof Long)
        {
            sTimeInMillis = ((Long)aValue).longValue();
        }
        else if (aValue instanceof String)
        {
            Timestamp sTS = Timestamp.valueOf((String)aValue);
            sTimeInMillis = sTS.getTime();
            sNanos = sTS.getNanos();
        }
        else
        {
            Error.throwSQLException(ErrorDef.UNSUPPORTED_TYPE_CONVERSION,
                                    aValue.getClass().getName(), getDBColumnTypeName());
        }
        setTime(sTimeInMillis, sNanos);
    }
}
