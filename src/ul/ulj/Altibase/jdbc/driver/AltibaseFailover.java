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

import java.sql.Connection;
import java.sql.SQLException;
import java.sql.SQLWarning;
import java.util.Collections;
import java.util.List;
import java.util.Map;
import java.util.logging.Level;
import java.util.logging.Logger;

import Altibase.jdbc.driver.cm.CmChannel;
import Altibase.jdbc.driver.cm.CmOperation;
import Altibase.jdbc.driver.ex.Error;
import Altibase.jdbc.driver.ex.ErrorDef;
import Altibase.jdbc.driver.ex.ShardFailOverSuccessException;
import Altibase.jdbc.driver.logging.LoggingProxy;
import Altibase.jdbc.driver.logging.TraceFlag;
import Altibase.jdbc.driver.sharding.core.AltibaseShardingConnection;
import Altibase.jdbc.driver.sharding.core.AltibaseShardingFailover;
import Altibase.jdbc.driver.sharding.core.DataNode;
import Altibase.jdbc.driver.sharding.core.ShardNodeConfig;
import Altibase.jdbc.driver.util.AltibaseProperties;
import Altibase.jdbc.driver.util.StringUtils;

import static Altibase.jdbc.driver.util.AltibaseProperties.PROP_SHARD_PIN;

public final class AltibaseFailover
{
    public enum CallbackState
    {
        STOPPED,
        IN_PROGRESS,
    }

    /* BUG-31390 Failover info for v$session */
    public enum FailoverType
    {
        CTF,
        STF,
    }

    private static Logger    mLogger;

    static
    {
        if (TraceFlag.TRACE_COMPILE && TraceFlag.TRACE_ENABLED)
        {
            mLogger = Logger.getLogger(LoggingProxy.JDBC_LOGGER_FAILOVER);
        }
    }

    private AltibaseFailover()
    {
    }

    /**
     * CTF�� �õ��ϰ�, ����� ���� ��� ����ų� �������� ���ܸ� �ٽ� ������.
     *
     * @param aContext Failover context
     * @param aWarning ��� ���� ��ü. null�̸� �� ��ü�� �����.
     * @param aException ���� ���� ����
     * @return ��� ���� ��ü
     * @throws SQLException CTF�� �� ��Ȳ�� �ƴϰų� CTF�� ������ ���
     */
    static SQLWarning tryCTF(AltibaseFailoverContext aContext, SQLWarning aWarning, SQLException aException) throws SQLException
    {
        if (!AltibaseFailover.checkNeedCTF(aContext, aException))
        {
            throw aException;
        }
        AltibaseConnection sConn = aContext.getConnection();
        if (!AltibaseFailover.doCTF(aContext))
        {
            /* BUG-47145 node connection�̰� lazy����϶� ctf�� ������ ���
               ShardFailoverIsNotAvailableException�� �ø���. */
            if (sConn.isNodeConnection() && sConn.getMetaConnection().isLazyNodeConnect())
            {
                CmOperation.throwShardFailoverIsNotAvailableException(sConn.getNodeName(),
                                                                      sConn.getServer(),
                                                                      sConn.getPort());
            }
            throw aException;
        }
        // BUG-46790 shard connection�� ��쿡�� notify failover �޼����� ������.
        if (sConn.isShardConnection())
        {
            AltibaseShardingConnection sMetaCon = sConn.getMetaConnection();
            AltibaseShardingFailover sShardFailover = sMetaCon.getShardFailover();
            sShardFailover.notifyFailover(sConn);

            /* BUG-47145 node connection���� CTF�� �߻������� autocommit false, lazy node connect ���¶��
               STF success�� �÷��� �Ѵ�. */
            if (sConn.isNodeConnection() && !sMetaCon.getAutoCommit() && sMetaCon.isLazyNodeConnect())
            {
                ShardNodeConfig sNodeConfg = sMetaCon.getShardNodeConfig();
                DataNode sNode = sNodeConfg.getNodeByName(sConn.getNodeName());
                Map<DataNode, Connection> sCachedMap = sMetaCon.getCachedConnections();
                // BUG-47145 CTF�� ������ connection�� cached node connection map�� �����Ѵ�.
                sCachedMap.put(sNode, sConn);
                throw new ShardFailOverSuccessException(aException.getMessage(),
                                                        ErrorDef.FAILOVER_SUCCESS,
                                                        sConn.getNodeName(), sConn.getServer(), sConn.getPort());
            }
        }

        /* BUG-32917 Connection Time Failover(CTF) information should be logged */
        aWarning = Error.createWarning(aWarning, ErrorDef.FAILOVER_SUCCESS, aException.getSQLState(), aException.getMessage(), aException);
        if (TraceFlag.TRACE_COMPILE && TraceFlag.TRACE_ENABLED)
        {
            mLogger.log(Level.WARNING, "Failover completed");
        }

        return aWarning;
    }

    /**
     * STF�� �õ��ϰ�, ����� ���� ���ܸ� ������.
     * ������������ ���ܸ� ���� STF ������ �˸���.
     * <p>
     * ���� failover context�� �ʱ�ȭ���� �ʾҴٸ� ���� ���� ���ܸ� �����ų� ����ڰ� �ѱ� ��� ��ü�� �״�� ��ȯ�Ѵ�.
     *
     * @param aContext Failover context
     * @param aException ���� ���� ����
     * @throws SQLException STF�� �����ߴٸ� ���� ���� ����, �ƴϸ� STF ������ �˸��� ����
     */
    public static void trySTF(AltibaseFailoverContext aContext, SQLException aException) throws SQLException
    {
        if (!AltibaseFailover.checkNeedSTF(aContext, aException))
        {
            throw aException;
        }
        AltibaseFailoverServerInfo sOldServerInfo = aContext.getCurrentServer();
        boolean sSTFSuccess = AltibaseFailover.doSTF(aContext);
        AltibaseConnection sConn = aContext.getConnection();
        /* BUG-46790 meta connection �̰� autocommit�� false �� ��쿡�� stf�������ο� �������
           �����鿡 touch�� ���ش�. */
        AltibaseProperties sProps = aContext.connectionProperties();
        if (sConn.isMetaConnection() && !sProps.isAutoCommit())
        {
            AltibaseShardingFailover sShardFailover = sConn.getMetaConnection().getShardFailover();
            sShardFailover.setTouchedToAllNodes();
        }
        if (sSTFSuccess)
        {
            // BUG-46790 shard connection(meta, node)�� ��쿡�� ShardFailOverSuccessException�� ������.
            if (sConn.isShardConnection())
            {
                throw new ShardFailOverSuccessException(aException.getMessage(),
                                                        ErrorDef.FAILOVER_SUCCESS,
                                                        sConn.getNodeName(),
                                                        sConn.getServer(),
                                                        sOldServerInfo.getPort());
            }
            Error.throwSQLExceptionForFailover(aException);
        }
        else
        {
            // BUG-47145 node connection�̰� stf�� ������ ��� ShardFailoverIsNotAvailableException�� �ø���.
            if (sConn.isNodeConnection())
            {
                CmOperation.throwShardFailoverIsNotAvailableException(sConn.getNodeName(),
                                                                      sConn.getServer(),
                                                                      sConn.getPort());
            }
            throw aException;
        }
    }

    /**
     * CTF�� ���� alternateservers���� ��� ������ ������ ������ �����Ѵ�.
     *
     * @param aContext Failover context
     * @return CTF ���� ����
     */
    private static boolean doCTF(AltibaseFailoverContext aContext)
    {
        return doCTF(aContext, FailoverType.CTF);
    }

    /**
     * shard failover align�� �Ҷ� ����Ǹ� Ư�� ������ CTF�� �õ��Ѵ�.
     * @param aContext Failover context ��ü
     * @param aFailoverType Failover Ÿ��
     * @param aNewServerInfo ������ ����
     * @return aNewServerInfo �������� CTF ��������
     */
    public static boolean doCTF(AltibaseFailoverContext aContext, FailoverType aFailoverType,
                                AltibaseFailoverServerInfo aNewServerInfo)
    {

        return connectToAlternateServer(aContext, aFailoverType, aContext.connectionProperties(), aNewServerInfo);

    }

    /**
     * CTF �Ǵ� STF�� ���� alternateservers���� ��� ������ ������ ������ �����Ѵ�.
     *
     * @param aContext Failover context
     * @param aFailoverType Failover ����
     * @return CTF ���� ����
     */
    private static boolean doCTF(AltibaseFailoverContext aContext, FailoverType aFailoverType)
    {
        final AltibaseProperties sProp = aContext.connectionProperties();
        final AltibaseFailoverServerInfo sOldServerInfo = aContext.getCurrentServer();

        List<AltibaseFailoverServerInfo> sTryList = aContext.getFailoverServerList().getList();
        if (sProp.useLoadBalance())
        {
            Collections.shuffle(sTryList);
        }
        if (sTryList.get(0) != sOldServerInfo)
        {
            sTryList.remove(sOldServerInfo);
            sTryList.add(0, sOldServerInfo);
        }
        for (AltibaseFailoverServerInfo sNewServerInfo : sTryList)
        {
            if (connectToAlternateServer(aContext, aFailoverType, sProp, sNewServerInfo))
            {
                return true;
            }
        }

        return false;
    }

    public static boolean connectToAlternateServer(AltibaseFailoverContext aContext,
                                                    FailoverType aFailoverType, 
                                                    AltibaseProperties aProp, 
                                                    AltibaseFailoverServerInfo aNewServerInfo)
    {
        int sMaxConnectionRetry = aProp.getConnectionRetryCount();
        int sConnectionRetryDelay = aProp.getConnectionRetryDelay() * 1000; // millisecond
        AltibaseFailoverServerInfo sOldServerInfo = aContext.getCurrentServer();
        
        for (int sRemainRetryCnt = Math.max(1, sMaxConnectionRetry); sRemainRetryCnt > 0; sRemainRetryCnt--)
        {
            /* BUG-43219 �� ���ӽø��� ä���� ������ؾ� �Ѵ�. �׷��� ������ ù��° ���� ������ �ڿ����� ���ӿ� ������ ��ģ��. */
            CmChannel sNewChannel = new CmChannel();
            try
            {
                sNewChannel.open(aNewServerInfo.getServer(),
                                 aProp.getSockBindAddr(),
                                 aNewServerInfo.getPort(),
                                 aProp.getLoginTimeout());
                setFailoverSource(aContext, aFailoverType, sOldServerInfo);
                aContext.setCurrentServer(aNewServerInfo);
                AltibaseFailover.changeChannelAndConnect(aContext, sNewChannel, aNewServerInfo);
                if (TraceFlag.TRACE_COMPILE && TraceFlag.TRACE_ENABLED)
                {
                    mLogger.log(Level.INFO, "Success to connect to (" + aNewServerInfo + ")");
                }
                
                return true;
            }
            catch (Exception aEx)
            {
                if (TraceFlag.TRACE_COMPILE && TraceFlag.TRACE_ENABLED)
                {
                    mLogger.log(Level.INFO, "Failure to connect to (" + aNewServerInfo + ")", aEx);
                    mLogger.log(Level.INFO, "Sleep " + sConnectionRetryDelay);
                }
                try
                {
                    Thread.sleep(sConnectionRetryDelay);
                }
                catch (InterruptedException sExSleep)
                {
                    // ignore
                }
            }
        }

        return false;
    }

    /**
     * <tt>CmChannel</tt>�� �ٲٰ�, �׸� �̿��� <tt>connect</tt>�� �õ��Ѵ�.
     *
     * @param aContext Failover contest
     * @param aChannel �ٲ� <tt>CmChannel</tt>
     * @param aServerInfo AltibaseFailoverServerInfo ����
     * @throws SQLException �� <tt>CmChannel</tt>�� �̿��� <tt>connect</tt>�� ������ ���
     */
    private static void changeChannelAndConnect(AltibaseFailoverContext aContext, CmChannel aChannel,
                                                AltibaseFailoverServerInfo aServerInfo) throws SQLException
    {
        AltibaseConnection sConn = aContext.getConnection();
        AltibaseProperties sProps = aContext.connectionProperties();
        sConn.setChannel(aChannel);
        sConn.handshake();
        if (sConn.isShardConnection())
        {
            sConn.shardHandshake();
            if (sConn.isMetaConnection())  // BUG-46790 meta connection�� ��쿡�� CTF ������ channel��ü�� ��ü�� �ش�.
            {
                AltibaseShardingConnection sMetaConn = sConn.getMetaConnection();
                sMetaConn.setChannel(aChannel);
                long sShardPin = sMetaConn.getShardContextConnect().getShardPin();
                // BUG-46790 meta node�� ��� channel�� ��ü�� �� ���� shardpin ���� �ٽ� ������ �ش�.
                sProps.put(PROP_SHARD_PIN, String.valueOf(sShardPin));
            }
        }
        sConn.setClosed(false);
        sProps.setDatabase(aServerInfo.getDatabase());
        sProps.setAutoCommit(sConn.getAutoCommit());
        sConn.connect(sProps);
    }

    /**
     * CTF�� Failover callback�� �����Ѵ�.
     * callback ����� ���� STF�� ��� �������� �׸������� �����Ѵ�.
     *
     * @param aContext Failover context
     * @return STF ���� ����
     * @throws SQLException STF�� ���� <tt>Statement</tt>�� �����ϴٰ� ������ ���
     */
    private static boolean doSTF(AltibaseFailoverContext aContext) throws SQLException
    {
        int sFailoverIntention;
        boolean sFOSuccess;
        int sFailoverEvent;
        AltibaseFailoverCallback sFailoverCallback = aContext.getCallback();
        AltibaseConnection sConn = aContext.getConnection();

        // BUG-46790 node connection���� �߻��� STF�� ��쿡�� dummy callback���� �����Ѵ�.
        if (sConn.isNodeConnection() && !(sFailoverCallback instanceof AltibaseFailoverCallbackDummy))
        {
            sFailoverCallback = new AltibaseFailoverCallbackDummy();
            aContext.setCallback(sFailoverCallback);
        }

        aContext.setCallbackState(CallbackState.IN_PROGRESS);
        sFailoverIntention = sFailoverCallback.failoverCallback(aContext.getConnection(),
                                                                aContext.getAppContext(),
                                                                AltibaseFailoverCallback.Event.BEGIN);
        aContext.setCallbackState(CallbackState.STOPPED);

        if (sFailoverIntention == AltibaseFailoverCallback.Result.QUIT)
        {
            sFOSuccess = false;
        }
        else
        {
            sFOSuccess = doCTF(aContext, FailoverType.STF);
            sConn.clearStatements4STF();

            if (sFOSuccess)
            {
                // BUG-46790 shard connection�� ��쿡�� notify failover �޼����� ������.
                if (aContext.getConnection().isShardConnection())
                {
                    notifyShardFailover(aContext);
                }
                aContext.setCallbackState(CallbackState.IN_PROGRESS);
                try
                {
                    AltibaseXAResource sXAResource = aContext.getRelatedXAResource();
                    if ((sXAResource != null) && (sXAResource.isOpen()))
                    {
                        sXAResource.xaOpen();
                    }
                    sFailoverEvent = AltibaseFailoverCallback.Event.COMPLETED;
                }
                catch (SQLException e)
                {
                    sFailoverEvent = AltibaseFailoverCallback.Event.ABORT;
                }
                sFailoverIntention = sFailoverCallback.failoverCallback(aContext.getConnection(), aContext.getAppContext(), sFailoverEvent);
                aContext.setCallbackState(CallbackState.STOPPED);

                if ((sFailoverEvent == AltibaseFailoverCallback.Event.ABORT) ||
                    (sFailoverIntention == AltibaseFailoverCallback.Result.QUIT))
                {
                    sFOSuccess = false;
                    aContext.getConnection().quiteClose();
                }
            }
            else
            {
                aContext.setCallbackState(CallbackState.IN_PROGRESS);
                sFailoverCallback.failoverCallback(aContext.getConnection(), aContext.getAppContext(), AltibaseFailoverCallback.Event.ABORT);
                aContext.setCallbackState(CallbackState.STOPPED);
                // BUG-46790 CTF�� ������ ��� close flag�� ������ �ش�.
                sConn.setClosed(true);
            }
        }
        return sFOSuccess;

    }

    private static void notifyShardFailover(AltibaseFailoverContext aContext) throws SQLException
    {
        AltibaseShardingConnection sMetaCon = aContext.getConnection().getMetaConnection();
        AltibaseShardingFailover sShardFailover = sMetaCon.getShardFailover();
        sShardFailover.updateShardNodeList(aContext.getConnection());
        sShardFailover.notifyFailover(aContext.getConnection());
    }

    /* BUG-31390 Failover info for v$session */
    /**
     * V$SESSION.FAILOVER_SOURCE�� ������ ���� ������ Connection property�� �����Ѵ�.
     *
     * @param aContext Failover Context
     * @param aFailoverType Failover ����
     * @param aServerInfo ������ ���� ����
     */
    private static void setFailoverSource(AltibaseFailoverContext aContext, FailoverType aFailoverType, AltibaseFailoverServerInfo aServerInfo)
    {
        StringBuilder sStrBdr = new StringBuilder();

        switch (aFailoverType)
        {
            case CTF:
                sStrBdr.append("CTF ");
                break;

            case STF:
                sStrBdr.append("STF ");
                break;

            default:
                // ���ο����� ���Ƿ�, �̷����� ���� �Ͼ�� �ȵȴ�.
                Error.throwInternalError(ErrorDef.INTERNAL_ASSERTION);
                break;
        }

        sStrBdr.append(aServerInfo.getServer());
        sStrBdr.append(':');
        sStrBdr.append(aServerInfo.getPort());
        if (!StringUtils.isEmpty(aServerInfo.getDatabase()))
        {
            sStrBdr.append('/');
            sStrBdr.append(aServerInfo.getDatabase());
        }
        aContext.setFailoverSource(sStrBdr.toString());
    }

    /**
     * CTF�� �ʿ����� Ȯ���Ѵ�.
     *
     * @param aContext Failover�� ���� ������ ���� ��ü
     * @param aException CTF�� �ʿ����� �� ����
     * @return CTF�� �ʿ��ϸ� true, �ƴϸ� false
     */
    private static boolean checkNeedCTF(AltibaseFailoverContext aContext, SQLException aException)
    {
        return (aContext != null) &&
               (aContext.getFailoverServerList().size() > 0) &&
               (isNeedToFailover(aException));
    }

    /**
     * STF�� �ʿ����� Ȯ���Ѵ�.
     *
     * @param aContext Failover�� ���� ������ ���� ��ü
     * @param aException STF�� �ʿ����� �� ����
     * @return STF�� �ʿ��ϸ� true, �ƴϸ� false
     */
    private static boolean checkNeedSTF(AltibaseFailoverContext aContext, SQLException aException)
    {
        return (aContext != null) &&
               (aContext.useSessionFailover()) &&
               (aContext.getFailoverServerList().size() > 0) &&
               (isNeedToFailover(aException)) &&
               (aContext.getCallbackState() == CallbackState.STOPPED);
    }

    /**
     * Failover�� �ʿ��� �������� Ȯ���Ѵ�.
     *
     * @param aException Failover�� �ʿ����� �� ����
     * @return Failover�� �ʿ��ϸ� true, �ƴϸ� false
     */
    public static boolean isNeedToFailover(SQLException aException)
    {
        switch (aException.getErrorCode())
        {
            case ErrorDef.COMMUNICATION_ERROR:
            case ErrorDef.UNKNOWN_HOST:
            case ErrorDef.CLIENT_UNABLE_ESTABLISH_CONNECTION:
            case ErrorDef.CLOSED_CONNECTION:
            case ErrorDef.RESPONSE_TIMEOUT:
            case ErrorDef.ADMIN_MODE_ERROR:
                return true;
            default:
                return false;
        }
    }
}
