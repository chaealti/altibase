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

import java.lang.reflect.InvocationHandler;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.sql.Connection;
import java.sql.ResultSet;
import java.sql.SQLWarning;
import java.util.logging.Level;
import java.util.logging.Logger;

import javax.transaction.xa.XAResource;

import Altibase.jdbc.driver.AltibaseLob;
import Altibase.jdbc.driver.AltibasePooledConnection;

/**
 * JDBC �������̽��� hooking�Ͽ� �α����� �����ϴ� ������ Ŭ����</br>
 * 
 * �� Ŭ������ JDBC API�� hooking�Ͽ� �αװ��� ������ �Ϻ� �߰��Ѵ�.
 * ��� �޼ҵ�� ����Ŭ�������� overrideing �ȴ�.
 * 
 * @author yjpark
 *
 */
public class LoggingProxy implements InvocationHandler
{
    public static final String  JDBC_LOGGER_DEFAULT    = "altibase.jdbc";
    public static final String  JDBC_LOGGER_ROWSET     = "altibase.jdbc.rowset";
    public static final String  JDBC_LOGGER_POOL       = "altibase.jdbc.pool";
    public static final String  JDBC_LOGGER_FAILOVER   = "altibase.jdbc.failover";
    public static final String  JDBC_LOGGER_CM         = "altibase.jdbc.cm";
    public static final String  JDBC_LOGGER_XA         = "altibase.jdbc.xa";
    public static final String  JDBC_LOGGER_ASYNCFETCH = "altibase.jdbc.asyncfetch"; // PROJ-2625
    private static final String METHOD_PREFIX_TOSTRING = "toString";
    private static final String METHOD_PREFIX_GET      = "get";
    protected static final String METHOD_PREFIX_CLOSE    = "close";
    protected transient Logger  mLogger;
    protected Object            mTarget;
    protected String            mTargetName;
    
    protected LoggingProxy(Object aTarget, String aTargetName)
    {
        this(aTarget, aTargetName, JDBC_LOGGER_DEFAULT);
    }
    
    protected LoggingProxy(Object aTarget, String aTargetName, String aLoggerName)
    {
        this.mTarget = aTarget;
        this.mTargetName = aTargetName;
        this.mLogger = Logger.getLogger(aLoggerName);
    }

    /**
     * JDBC API�� hooking �ɶ� ����Ǵ� �޼ҵ�.</br>
     * JDBC API�� hooking�� �Ϻ� �α� ����� �����ϰ� invoke�� �ش� �޼ҵ带 �����Ѵ�.
     */
    public Object invoke(Object aProxy, Method aMethod, Object[] aArgs) throws Throwable
    {
        Object sResult = null;

        try
        {
            logPublicStart(aMethod, aArgs);
            logSql(aMethod, sResult, aArgs);

            long sStartTime = System.currentTimeMillis();
            sResult = aMethod.invoke(mTarget, aArgs);
            
            logSqlTiming(aMethod, sStartTime);
            sResult = createProxyWithReturnValue(sResult);

            logClose(aMethod);
            logReturnValue(aMethod, sResult);
            logEnd(aMethod);
        }
        catch (InvocationTargetException sEx)
        {
            Throwable sCause = sEx.getTargetException();
            mLogger.log(Level.SEVERE, sCause.getLocalizedMessage(), sCause);
            throw sCause;
        }

        return sResult;
    }

    /**
     * �޼ҵ尡 �����ϴ� ������ �޼ҵ��̸��� �ƱԸ�Ʈ ������ �α׷� �����.</br>
     * �̶� �żҵ���� toString�� ���� �����Ѵ�.
     * 
     * @param aMethod
     * @param aArgs
     */
    private void logPublicStart(Method aMethod, Object[] aArgs)
    {
        if (!aMethod.getName().equals(METHOD_PREFIX_TOSTRING))
        {
            mLogger.log(Level.FINE, "{0} Public Start : {1}.{2}", 
                        new Object[] { getUniqueId(), this.mTarget.getClass().getName(), aMethod.getName() } );
            logArgumentInfo(aArgs);
        }
    }

    /**
     * CONFIG������ ���õǾ��� �� sql���� ����Ѵ�.
     * @param aMethod
     * @param aResult
     * @param aArgs
     */
    protected void logSql(Method aMethod, Object aResult, Object[] aArgs)
    {
        // �⺻���� Proxy��ü�� sql �α��� ���� �ʴ´�.
    }
    
    /**
     * sql���� ����Ǵµ� �ɸ� �ð��� ����Ѵ�.
     * @param aMethod
     * @param aStartTime
     */
    protected void logSqlTiming(Method aMethod, long aStartTime)
    {
        // �⺻���� Proxy��ü�� sql timing ������ �α����� �ʴ´�.
    }
    
    /**
     * �޼ҵ� �������� �̿��� �ʿ��ϸ� Proxy��ü�� ����� �α׸� �����.
     * @param aResult
     * @return
     */
    protected Object createProxyWithReturnValue(Object aResult)
    {
        Object sProxy;
        
        if (aResult instanceof Connection)
        {
            sProxy = LoggingProxyFactory.createConnectionProxy(aResult);
        }
        else if (aResult instanceof XAResource)
        {
            sProxy = LoggingProxyFactory.createXaResourceProxy(aResult);
        }
        else
        {
            sProxy = aResult;
        }
        
        if (aResult instanceof SQLWarning)
        {
            mLogger.log(Level.WARNING, "SQLWarning : ", (SQLWarning)aResult);
        }
        
        return sProxy;
    }
    
    /**
     * �޼ҵ� ���� ������� �α׷� �����.</br>
     * �޼ҵ���� toString�� ���� �����ϰ� ���ϰ��� �������� FINE������ �ش� Object�� ����Ѵ�.
     * 
     * @param aMethod
     * @param aResult
     */
    protected void logReturnValue(Method aMethod, Object aResult)
    {
        if (!aMethod.getName().equals(METHOD_PREFIX_TOSTRING))
        {
            if (!aMethod.getReturnType().equals(Void.TYPE))
            {
                if (aMethod.getName().startsWith(METHOD_PREFIX_GET) && mTarget instanceof ResultSet 
                        && mLogger.getLevel() == Level.CONFIG)
                {
                    mLogger.log(Level.CONFIG, "ResultSet.{0} : {1}", new Object[] { aMethod.getName(), aResult });
                }
                mLogger.log(Level.FINE, "{0} Return : {1}", new Object[] { getUniqueId(),aResult });
            }
        }
    }
    
    /**
     * close �޼ҵ带 �α��Ѵ�.</br>
     * �̶� �ش��ϴ� ������Ʈ�� unique id�� ���� ����Ѵ�.</br>
     * ���� ��� Connection�̳� Statement���� ��� session id�� stmt id�� ����ϰ� �� �̿��� ��쿡��</br> 
     * Object�� hashcode�� ����Ѵ�.
     * 
     * @param aMethod
     */
    protected void logClose(Method aMethod)
    {
        if (aMethod.getName().startsWith(METHOD_PREFIX_CLOSE))
        {
            mLogger.log(Level.INFO, "{0} {1} closed. ", new Object[] { getUniqueId(), mTargetName });
        }        
    }
    
    private void logEnd(Method aMethod)
    {
        if (!aMethod.getName().equals(METHOD_PREFIX_TOSTRING))
        {
            mLogger.log(Level.FINE, "{0} End", getUniqueId());
        }
    }
    
    /**
     * �ƱԸ�Ʈ�� �ε���, Ÿ��, �� ������ FINE������ �����.
     * 
     * @param aArgs
     * @return
     */
    private void logArgumentInfo(Object[] aArgs)
    {
        if (aArgs == null) return;
        for (int sIdx = 0; sIdx < aArgs.length; sIdx++)
        {
            mLogger.log(Level.FINE, "{0} Argument Idx : {1}", new Object [] { getUniqueId(), String.valueOf(sIdx) });
            if (aArgs[sIdx] == null || aArgs[sIdx] instanceof AltibaseLob)
            {
                continue;
            }
            mLogger.log(Level.FINE, "{0} Argument Type : {1}", new Object [] { getUniqueId(), aArgs[sIdx].getClass().getName() });
            mLogger.log(Level.FINE, "{0} Argument Value : {1}", new Object [] { getUniqueId(), aArgs[sIdx] });
        }
    }

    /**
     * �ƱԸ�Ʈ������ �̿��� String�� �����Ѵ�..</br>
     * �̶� �ƱԸ�Ʈ�� String�� ��쿡�� "�� �߰��Ͽ� �Ʒ��� ���� ���°� �Ǹ� CONFIG������ sql�� ����Ҷ� ���ȴ�.</br>
     * (1, "aaa", "bbb", 222)
     * 
     * @param aArgs
     * @return
     */
    protected String makeArgStr(Object[] aArgs)
    {
        StringBuffer sStr = new StringBuffer();
        for (int sIdx = 0; sIdx < aArgs.length; sIdx++)
        {
            if (sIdx == 0)
            {
                sStr.append('(');
            }
            if (aArgs[sIdx] instanceof String)
            {
                sStr.append('\"');
            }
            
            if (aArgs[sIdx] != null && !(aArgs[sIdx] instanceof AltibaseLob))
            {
                sStr.append(aArgs[sIdx]);
            }
            
            if (aArgs[sIdx] instanceof String)
            {
                sStr.append('\"');
            }
            if (sIdx != aArgs.length - 1)
            {
                sStr.append(", ");
            }
            if (sIdx == aArgs.length - 1)
            {
                sStr.append(')');
            }
        }
        return sStr.toString();
    }
        
    protected String getUniqueId()
    {
        if (mTarget instanceof AltibasePooledConnection)
        {
            return mTarget.toString();
        }
        return "";
    }
}
