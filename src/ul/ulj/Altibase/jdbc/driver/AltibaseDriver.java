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

package Altibase.jdbc.driver;

import java.io.PrintWriter;
import java.sql.*;
import java.util.Properties;
import java.util.logging.Level;
import java.util.logging.Logger;

import Altibase.jdbc.driver.logging.LoggingProxy;
import Altibase.jdbc.driver.logging.LoggingProxyFactory;
import Altibase.jdbc.driver.logging.TraceFlag;
import Altibase.jdbc.driver.sharding.core.AltibaseShardingConnection;
import Altibase.jdbc.driver.util.AltibaseProperties;

public final class AltibaseDriver implements Driver
{
    static PrintWriter      mLogWriter;
    static transient Logger mLogger;

    static
    {
        try
        {
            DriverManager.registerDriver(new AltibaseDriver());
        }
        catch (SQLException e)
        {
            // LOGGING
        }

        mLogWriter = DriverManager.getLogWriter();
        if (TraceFlag.TRACE_COMPILE && TraceFlag.TRACE_ENABLED)
        {
            mLogger = Logger.getLogger(LoggingProxy.JDBC_LOGGER_DEFAULT);
        }
    }

    public AltibaseDriver()
    {

    }

    public boolean acceptsURL(String aURL) throws SQLException
    {
        return AltibaseUrlParser.acceptsURL(aURL);
    }

    public Connection connect(String aURL, Properties aInfo) throws SQLException
    {
        if (!acceptsURL(aURL))
        {
            return null;
        }
        if (TraceFlag.TRACE_COMPILE && TraceFlag.TRACE_ENABLED)
        {
            mLogger.log(Level.INFO, "URL : {0}, Properties : {1}", new Object[] { aURL, aInfo});
        }
        AltibaseProperties sAltiProp = new AltibaseProperties(aInfo);
        // BUG-43349 url parsing�� ������ Ŭ�������� ó���Ѵ�.
        return createConnection(sAltiProp, AltibaseUrlParser.parseURL(aURL, sAltiProp));
    }

    private Connection createConnection(AltibaseProperties aAltiProp,
                                        boolean aIncludesShardPrefix) throws SQLException
    {
        // PROJ-2690 shard prefix�� ���ԵǾ� �ִ� ��� AltibaseShardingConnection�� �����Ѵ�.
        Connection sConn = (aIncludesShardPrefix) ? new AltibaseShardingConnection(aAltiProp) :
                                                    new AltibaseConnection(aAltiProp, null, null);

        if (TraceFlag.TRACE_COMPILE && TraceFlag.TRACE_ENABLED)
        {
            sConn = LoggingProxyFactory.createConnectionProxy(sConn);
        }
        return sConn;
    }

    public int getMajorVersion()
    {
        return AltibaseVersion.ALTIBASE_MAJOR_VERSION;
    }

    public int getMinorVersion()
    {
        return AltibaseVersion.ALTIBASE_MINOR_VERSION;
    }

    public DriverPropertyInfo[] getPropertyInfo(String aURL, Properties aInfo) throws SQLException
    {
        AltibaseProperties sProp = new AltibaseProperties(aInfo);
        AltibaseUrlParser.parseURL(aURL, sProp);

        return sProp.toPropertyInfo();
    }

    public boolean jdbcCompliant()
    {
        return false;
    }

    // BUG-43349 AltibaseDataSource.setURL()�κп��� �����ϰ� �ֱ⶧���� �ش� �޼ҵ带 �����Ѵ�.
    static void parseURL(String aURL, AltibaseProperties aProps) throws SQLException
    {
        AltibaseUrlParser.parseURL(aURL, aProps);
    }

    @Override
    public Logger getParentLogger()
    {
        if (TraceFlag.TRACE_COMPILE && TraceFlag.TRACE_ENABLED)
        {
            return Logger.getLogger(LoggingProxy.JDBC_LOGGER_DEFAULT);
        }

        return null;
    }
}
