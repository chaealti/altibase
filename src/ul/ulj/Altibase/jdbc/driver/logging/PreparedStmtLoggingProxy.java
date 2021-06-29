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

import java.lang.reflect.Method;
import java.sql.Connection;
import java.util.logging.Level;

import Altibase.jdbc.driver.AltibasePreparedStatement;

/**
 * java.sql.PreparedStatement �������̽��� hooking�ϴ� ProxyŬ����</br>
 * logSql�޼ҵ�� getUniqueId�޼ҵ常 �������̵��ϰ� �������� �θ�Ŭ�������� ��ӹ޾� ����Ѵ�.
 * 
 * @author yjpark
 * 
 */
public class PreparedStmtLoggingProxy extends StatementLoggingProxy
{
    private static final String METHOD_PREFIX_SET = "set";

    public PreparedStmtLoggingProxy(Object aTarget, Connection aConn)
    {
        this(aTarget, "PreparedStatement", aConn);
    }
    
    public PreparedStmtLoggingProxy(Object aTarget, String aTargetName, Connection aConn)
    {
        super(aTarget, aTargetName, aConn);
    }
    
    public void logSql(Method aMethod, Object aResult, Object[] aArgs)
    {
        if (aMethod.getName().startsWith(METHOD_PREFIX_EXECUTE) && aArgs != null)
        {
            mLogger.log(Level.CONFIG, "{0} executing prepared sql : {1}", new Object[] { getUniqueId(), aArgs[0] });
        }
        else if (aMethod.getName().startsWith(METHOD_PREFIX_SET)) // �޼ҵ���� setXXX�϶��� �ش� �ƱԸ�Ʈ������ CONFIG������ ����Ѵ�.
        {
            mLogger.log(Level.CONFIG, "{0} {1}{2}", new Object[] { getUniqueId(), aMethod.getName(), makeArgStr(aArgs) });
        }
    }
    
    /**
     * PreparedStatement ���� ��� �������� �޾ƿ� Statement id�� unique id�� ����Ѵ�.
     */
    public String getUniqueId()
    {
        return "[StmtId #" + ((AltibasePreparedStatement)mTarget).getStmtId() + "] ";
    }
}
