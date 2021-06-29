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

import java.util.logging.*;
import java.io.*;
import java.text.*;
import java.util.Date;

/**
 * �б� ������ LogRecord�� 1~2�������� �������ִ� customized formatter
 * 
 * @author yjpark
 *
 */
public class SingleLineFormatter extends Formatter
{
    private final static String DATE_FORMAT = "{0,date} {0,time}";
    private MessageFormat       mFormatter;

    private String mLineSeparator = System.getProperty("line.separator");

    /**
     * LogRecord�� format�� �����Ѵ�.
     * 
     * @param aRecord format�� log record
     * @return format�� log record
     */
    public String format(LogRecord aRecord)
    {
        Date sDate = new Date();
        Object sArgs[] = new Object[1];
        StringBuffer sb = new StringBuffer();

        sDate.setTime(aRecord.getMillis());
        sArgs[0] = sDate;

        // Date, Time
        StringBuffer sText = new StringBuffer();
        if (mFormatter == null)
        {
            mFormatter = new MessageFormat(DATE_FORMAT);
        }
        mFormatter.format(sArgs, sText, null);
        sb.append(sText).append(' ');

        // Ŭ������
        if (aRecord.getSourceClassName() == null)
        {
            sb.append(aRecord.getLoggerName());
        }
        else
        {
            sb.append(aRecord.getSourceClassName());
        }

        // �޼ҵ��
        if (aRecord.getSourceMethodName() != null)
        {
            sb.append('.').append(aRecord.getSourceMethodName());
        }
        sb.append(" - ");

        String sMessage = formatMessage(aRecord);

        // ����
        sb.append(aRecord.getLevel().getLocalizedName()).append(": ");

        int sOffset = (1000 - aRecord.getLevel().intValue()) / 100;
        for (int i = 0; i < sOffset; i++)
        {
            sb.append(' ');
        }

        sb.append(sMessage).append(mLineSeparator);
        if (aRecord.getThrown() != null)
        {
            try
            {
                StringWriter sStringWriter = new StringWriter();
                PrintWriter sPrintWriter = new PrintWriter(sStringWriter);
                aRecord.getThrown().printStackTrace(sPrintWriter);
                sPrintWriter.close();
                sb.append(sStringWriter.toString());
            }
            catch (Exception ex)
            {
            }
        }
        return sb.toString();
    }
}
