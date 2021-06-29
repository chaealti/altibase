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

import Altibase.jdbc.driver.ex.Error;
import Altibase.jdbc.driver.ex.ErrorDef;
import Altibase.jdbc.driver.util.AltibaseProperties;

import java.sql.SQLException;
import java.util.Properties;
import java.util.StringTokenizer;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

/**
 * ����ǥ������ �̿��� connection string�̳� alternate servers string�� �Ľ��Ѵ�.
 */
public class AltibaseUrlParser
{
    public static final String   URL_PREFIX              = "jdbc:Altibase_" + AltibaseVersion.CM_VERSION_STRING + "://";

    // URL PATTERN GROUPS
    // 1: Shard Prefix
    // 2: Server or DSN
    // 3: WRAPPER for Port
    // 4: Port
    // 5: WRAPPER for DBName
    // 6: DBName
    // 7: WRAPPER for Properties
    // 8: Properties
    private static final int     URL_GRP_SHARD_PREFIX    = 1;
    private static final int     URL_GRP_SERVER_DSN      = 3;
    private static final int     URL_GRP_PORT            = 5;
    private static final int     URL_GRP_DBNAME          = 7;
    private static final int     URL_GRP_PROPERTIES      = 9;

    private static final String  URL_PATTERN_IP4         = "\\d{1,3}\\.\\d{1,3}\\.\\d{1,3}\\.\\d{1,3}";
    private static final String  URL_PATTERN_IP6         = "\\[[^\\]]+\\]";
    private static final String  URL_PATTERN_DOMAIN      = "(?:[a-zA-Z0-9][a-zA-Z0-9\\-]{0,61}[a-zA-Z0-9]|[a-zA-Z0-9])"
                                                           + "(?:\\.(?:[a-zA-Z0-9][a-zA-Z0-9\\-]{0,61}[a-zA-Z0-9]|[a-zA-Z0-9]))*";
    private static final String  URL_PATTERN_DSN         = "[a-zA-Z_][\\w]*";
    private static final String  URL_PATTERN_PORT        = "(:([\\d]+))?";
    private static final String  URL_PATTERN_DBNAME      = "(/(" + URL_PATTERN_DSN + "))?";
    private static final String  URL_PATTERN_PROPS       = "(\\?([a-zA-Z_][\\w]*=[^&]*(&[a-zA-Z_][\\w]*=[^&]*)*))?";
    private static final String  URL_PATTERN_ALT_SERVER  = "(" + URL_PATTERN_IP4 + "|" + URL_PATTERN_IP6 + "|" + URL_PATTERN_DOMAIN + "):[\\d]+" + URL_PATTERN_DBNAME;

    private static final Pattern URL_PATTERN_4PARSE      = Pattern.compile("^jdbc(:sharding)?:Altibase(_" + AltibaseVersion.CM_VERSION_STRING + ")?://("
                                                                           + URL_PATTERN_IP4 + "|"
                                                                           + URL_PATTERN_IP6 + "|"
                                                                           + URL_PATTERN_DOMAIN + "|"
                                                                           + URL_PATTERN_DSN + ")"
                                                                           + URL_PATTERN_PORT
                                                                           + URL_PATTERN_DBNAME
                                                                           + URL_PATTERN_PROPS + "$");
    private static final Pattern URL_PATTERN_4ACCEPTS    = Pattern.compile("^jdbc(:sharding)?:Altibase(_" + AltibaseVersion.CM_VERSION_STRING + ")?://.*$");
    private static final Pattern URL_PATTERN_4VARNAME    = Pattern.compile("^" + URL_PATTERN_DSN + "$");
    private static final Pattern URL_PATTERN_ALT_SERVERS = Pattern.compile("^\\s*\\(\\s*" + URL_PATTERN_ALT_SERVER + "(\\s*,\\s*" + URL_PATTERN_ALT_SERVER + ")*\\s*\\)\\s*$");

    private AltibaseUrlParser()
    {
    }

    public static boolean acceptsURL(String aURL)
    {
        return URL_PATTERN_4ACCEPTS.matcher(aURL).matches();
    }

    /**
     * URL�� �Ľ��� aDestProp�� property�� �����Ѵ�. <br>
     * �̶� URL�� shard prefix�� ���ԵǾ� �ִ� ��� true�� �����ϰ� �׷��� ���� ��� false�� �����Ѵ�.
     * <p>
     * ��ȿ�� URL ������ ������ ����:
     * <ul>
     * <li>jdbc:Altibase://123.123.123.123:20300/mydb</li>
     * <li>jdbc:Altibase://abc.abc.abc.abc:20300/mydb</li>
     * <li>jdbc:Altibase://123.123.123.123/mydb</li>
     * <li>jdbc:Altibase://[::1]:20300/mydb</li>
     * <li>jdbc:Altibase://[::1]:20300/mydb?prop1=val1&prop2=val2</li>
     * <li>jdbc:Altibase://[::1]/mydb</li>
     * <li>jdbc:Altibase://DataSourceName</li>
     * <li>jdbc:Altibase://DataSourceName:20300</li>
     * <li>jdbc:Altibase://DataSourceName:20300?prop1=val1&prop2=val2</li>
     * <li>jdbc:sharding:Altibase://123.123.123.123:20300/mydb</li>
     * </ul>
     * <p>
     * ���� �Ӽ��� connection url�� ���� �� 3������ ������ �浹�� �� �ִµ�, �� �� �������� �켱������ ������ ����:
     * <ol>
     * <li>connection url�� ������ (= aURL)</li>
     * <li>������ ������ �� (= aDestProp = connect �޼ҵ�� �Ѿ�� ��)</li>
     * <li>altibase_cli.ini ���� (DSN�� ����ϴ� ���)</li>
     * </ol>
     *
     * @param aURL connection url
     * @param aDestProp �Ľ� ����� ���� Property
     * @return shard prefix�� ���ԵǾ� �ִ��� ����
     * @throws SQLException URL ������ �ùٸ��� ���� ���
     */
    public static boolean parseURL(String aURL, AltibaseProperties aDestProp) throws SQLException
    {
        Matcher sMatcher = URL_PATTERN_4PARSE.matcher(aURL);
        throwErrorForInvalidConnectionUrl(!sMatcher.matches(), aURL);

        String sServerOrDSN = sMatcher.group(URL_GRP_SERVER_DSN);
        throwErrorForInvalidConnectionUrl(sServerOrDSN == null, aURL);

        String sProperties = sMatcher.group(URL_GRP_PROPERTIES);
        if (sProperties != null)
        {
            parseProperties(sProperties, aDestProp);
        }

        String sPort = sMatcher.group(URL_GRP_PORT);
        if (sPort != null)
        {
            aDestProp.setProperty(AltibaseProperties.PROP_PORT, sPort);
        }

        String sDbName = sMatcher.group(URL_GRP_DBNAME);
        if (sDbName == null) // DbName�� ������ DSN���� ����
        {
            throwErrorForInvalidConnectionUrl(!URL_PATTERN_4VARNAME.matcher(sServerOrDSN).matches(), aURL);
            aDestProp.setDataSource(sServerOrDSN);
        }
        else
        {
            aDestProp.setProperty(AltibaseProperties.PROP_DBNAME, sDbName);
            aDestProp.setServer(sServerOrDSN);
        }

        // PROJ-2690 sharding prefix�� ���ԵǾ� �ִ� ��� true�� �����Ѵ�.
        return sMatcher.group(URL_GRP_SHARD_PREFIX) != null;
    }

    /**
     * alternate servers string�� �Ľ��� <tt>AltibaseFailoverServerInfoList</tt>�� �����.
     * <p>
     * alternate servers string�� "( host_name:port[/dbname][, host_name:port[/dbname]]* )"�� ���� �����̾�� �Ѵ�.
     * ���� ��� ������ ����:<br />
     * (192.168.3.54:20300, 192.168.3.55:20301)
     * (abc.abc.abc.abc:20300, abc.abc.abc.abc:20301)
     * (192.168.3.54:20300/mydb1, 192.168.3.55:20301/mydb2)
     * (abc.abc.abc.abc:20300/mydb1, abc.abc.abc.abc:20301/mydb2)
     * <p>
     * ���� alternate servers string�� IPv6 ������ �ּҸ� ������ �Ѵٸ�, ip�� [, ]�� ���ξ� �Ѵ�.
     * �������, "::1"�� ������ �Ѵٸ� "[::1]:20300" ó�� ����� "::1:20300" ó�� ���� �ȵȴ�.
     * �̷� ��쿡�� ParseException�� ����.
     * <p>
     * alternate servers string�� null�̰ų� ���� ������ ��� null ��� �� ����Ʈ�� ��ȯ�Ѵ�.
     * �̴� �޴��ʿ��� null ó�� ���� �� �� �ְ� �ϱ� �����̴�.
     *
     * @param aAlternateServersStr ��� ���� ����� ���� ���ڿ�
     * @return ��ȯ�� <tt>AltibaseFailoverServerInfoList</tt> ��ü
     * @throws SQLException alternate servers string�� ������ �ùٸ��� ���� ���
    */
    public static AltibaseFailoverServerInfoList parseAlternateServers(String aAlternateServersStr) throws SQLException
    {
        AltibaseFailoverServerInfoList sServerList = new AltibaseFailoverServerInfoList();

        if (aAlternateServersStr == null)
        {
            return sServerList;
        }

        matchAlternateServers(aAlternateServersStr);

        int l = aAlternateServersStr.indexOf('(');
        int r = aAlternateServersStr.lastIndexOf(')');
        String sTokenizableStr = aAlternateServersStr.substring(l + 1, r);

        StringTokenizer sTokenizer = new StringTokenizer(sTokenizableStr, ",");
        while (sTokenizer.hasMoreTokens())
        {
            String sTokenStr = sTokenizer.nextToken();
            int sIdxColon = sTokenStr.lastIndexOf(':');
            String sServer = sTokenStr.substring(0, sIdxColon).trim();
            String sPortStr = sTokenStr.substring(sIdxColon + 1).trim();

            /* BUG-43219 parse dbname property */
            int sIdxSlash = sPortStr.lastIndexOf("/");
            String sDbName = "";
            if (sIdxSlash > 0)
            {
                sDbName = sPortStr.substring(sIdxSlash + 1).trim();
                sPortStr = sPortStr.substring(0, sIdxSlash).trim();
            }

            int sPort = Integer.parseInt(sPortStr);
            sServerList.add(sServer, sPort, sDbName);
        }
        return sServerList;
    }

    /**
     * connection url�� ���Ե� ���ڸ� �Ľ��� aDestProp�� property�� �����Ѵ�.
     *
     * @param aArg ���ڸ� ���� "{k}={v}(&{k}={v})*" ������ ���ڿ�
     * @param aDestProp �Ľ� ����� ���� Property
     * @throws SQLException ���ڿ� ������ �ùٸ��� ���� ���
     */
    private static void parseProperties(String aArg, Properties aDestProp) throws SQLException
    {
        try
        {
            String[] sPropExpr = aArg.split("&");
            for (String aSPropExpr : sPropExpr)
            {
                int sEqIndex = aSPropExpr.indexOf("=");
                String sKey = aSPropExpr.substring(0, sEqIndex);
                String sValue = aSPropExpr.substring(sEqIndex + 1, aSPropExpr.length());
                aDestProp.setProperty(sKey, sValue);
            }
        }
        catch (Exception ex)
        {
            Error.throwSQLException(ErrorDef.INVALID_CONNECTION_URL, aArg, ex);
        }
    }

    /**
     * alternateservers url�� �������� �������� ����ǥ������ �̿��� Ȯ���Ѵ�.<br>
     *
     * @param aAlternateServersStr alternate servers url
     * @throws SQLException INVALID_FORMAT_OF_ALTERNATE_SERVERS
     */
    private static void matchAlternateServers(String aAlternateServersStr) throws SQLException
    {
        Matcher sMatcher = URL_PATTERN_ALT_SERVERS.matcher(aAlternateServersStr);
        if (!sMatcher.matches())
        {
            Error.throwSQLException(ErrorDef.INVALID_FORMAT_OF_ALTERNATE_SERVERS, aAlternateServersStr);
        }
    }

    private static void throwErrorForInvalidConnectionUrl(boolean aTest, String aURL) throws SQLException
    {
        if (aTest)
        {
            Error.throwSQLException(ErrorDef.INVALID_CONNECTION_URL, aURL);
        }
    }
}
