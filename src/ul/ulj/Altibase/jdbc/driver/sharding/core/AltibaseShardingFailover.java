/*
 * Copyright (c) 1999~2017, Altibase Corp. and/or its affiliates. All rights reserved.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License, version 3,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 */

package Altibase.jdbc.driver.sharding.core;

import Altibase.jdbc.driver.*;
import Altibase.jdbc.driver.cm.CmOperation;
import Altibase.jdbc.driver.cm.CmProtocolContextShardConnect;
import Altibase.jdbc.driver.cm.CmShardProtocol;
import Altibase.jdbc.driver.ex.*;
import Altibase.jdbc.driver.ex.Error;
import Altibase.jdbc.driver.util.AltibaseProperties;

import java.sql.Connection;
import java.sql.SQLException;
import java.sql.SQLWarning;
import java.util.Map;
import java.util.TreeMap;
import java.util.concurrent.ConcurrentHashMap;
import java.util.logging.Level;

import static Altibase.jdbc.driver.sharding.util.ShardingTraceLogger.shard_log;

/**
 * ������ ���õ� Failover ������ �����Ѵ�.
 */
public class AltibaseShardingFailover
{
    private AltibaseShardingConnection    mMetaConn;

    public AltibaseShardingFailover(AltibaseShardingConnection aMetaConn)
    {
        mMetaConn = aMetaConn;
    }

    public void notifyFailover(AltibaseConnection aConn)
    {
        if (aConn.isNodeConnection())
        {
            notifyFailoverOnDataNode(aConn);
        }
        else if (aConn.isMetaConnection())
        {
            notifyFailoverOnMeta(aConn);
        }
    }

    private void notifyFailoverOnMeta(AltibaseConnection aConn)
    {
        AltibaseShardingConnection sMetaConn = aConn.getMetaConnection();
        TreeMap<DataNode, Connection> sSortedNodeConnMap = new TreeMap<DataNode, Connection> (sMetaConn.getCachedConnections());
        /* BUG-46790 meta���� failover�� �߻��� ��� ��� �����ͳ�忡�� notify�� �����ؾ� �Ѵ�. ������ ��忡 ���� ������
           touch ������ ��� Ȯ���ȴ�. */
        for (Connection sEach : sSortedNodeConnMap.values())
        {
            notifyFailoverOnDataNode((AltibaseConnection)sEach);
        }
    }

    private void notifyFailoverOnDataNode(AltibaseConnection aConn)
    {
        NodeConnectionReport sReport = getNodeConnectionReport(aConn);
        try
        {
            // BUG-46790 meta connection �� close�� ��쿡�� connection report�� ������ �ʴ´�.
            Connection sMetaConn = aConn.getMetaConnection();
            if (!sMetaConn.isClosed())
            {
                sReport.setNodeReportType(NodeConnectionReport.NodeReportType.SHARD_NODE_REPORT_TYPE_CONNECTION);
                sendNodeConnectionReport(sReport, mMetaConn.getShardProtocol());
            }
            // BUG-47145 ctf�������� autocommit���ΰ� ���� ���ĵǱ� ���̱� ������ metaConn�� autocommit���η� ���Ѵ�.
            if (!sMetaConn.getAutoCommit())
            {
                mMetaConn.getShardContextConnect().setTouched(true);
            }
        }
        catch (SQLException aEx)
        {
            try
            {
                AltibaseFailover.trySTF(mMetaConn.getMetaConnection().failoverContext(), aEx);
            }
            catch (SQLException aSqlEx)
            {
                // BUG-46790 notify ���� STF�� �߻��ϸ� ����� ������� ���ܷ� ó������ �ʰ� �α׸� �����.
                shard_log(Level.INFO, aSqlEx.getMessage(), aSqlEx);
            }
        }
    }

    private void sendNodeConnectionReport(NodeConnectionReport aReport,
                                          CmShardProtocol aShardProtocol) throws SQLException
    {
        CmProtocolContextShardConnect sShardContextConnect = mMetaConn.getShardContextConnect();
        sShardContextConnect.clearProperties();
        aShardProtocol.shardNodeReport(aReport);
        if (sShardContextConnect.getError() != null)
        {
            SQLWarning sWarning = mMetaConn.getWarnings();
            sWarning = Error.processServerError(sWarning, sShardContextConnect.getError());
            mMetaConn.setWarning(sWarning);
        }
    }

    private NodeConnectionReport getNodeConnectionReport(AltibaseConnection aConn)
    {
        NodeConnectionReport sReport = new NodeConnectionReport();
        String sNodeName = aConn.getNodeName();
        DataNode sNode = mMetaConn.getShardNodeConfig().getNodeByName(sNodeName);
        AltibaseFailoverContext sFailoverContext = aConn.failoverContext();

        if (sFailoverContext == null)
        {
            // BUG-46790 alternate server ������ ���� ��쿡�� active�� �����Ѵ�.
            sReport.setDestination(DataNodeFailoverDestination.ACTIVE);
        }
        else
        {
            AltibaseFailoverServerInfo sCurServerInfo = sFailoverContext.getCurrentServer();
            if (sCurServerInfo.getServer().equals(sNode.getServerIp()) &&
                sCurServerInfo.getPort() == sNode.getPortNo())
            {
                sReport.setDestination(DataNodeFailoverDestination.ACTIVE);
            }
            else
            {
                sReport.setDestination(DataNodeFailoverDestination.ALTERNATE);
            }
        }

        sReport.setNodeId(sNode.getNodeId());

        return sReport;
    }

    public void updateShardNodeList(AltibaseConnection aConn) throws SQLException
    {
        if (aConn.isMetaConnection())
        {
            AltibaseShardingConnection sShardCon = aConn.getMetaConnection();
            CmShardProtocol sShardProtocol = sShardCon.getShardProtocol();
            AltibaseProperties sProps = sShardCon.getProps();
            try
            {
                sShardProtocol.updateNodeList();
                // BUG-46790 node list�� ������ �Ŀ��� cache map�� �ִ� DataNode��ü�鵵 �����ؾ� �Ѵ�.
                ShardNodeConfig sShardNodeConfig = sShardCon.getShardNodeConfig();
                Map<DataNode, Connection> sOldCachedConnMap = sShardCon.getCachedConnections();
                Map<DataNode, Connection> sNewCachedConnMap = new ConcurrentHashMap<DataNode, Connection>();
                for (Map.Entry<DataNode, Connection> sEntry : sOldCachedConnMap.entrySet())
                {
                    DataNode sOldNode = sEntry.getKey();
                    Connection sConn = sEntry.getValue();
                    DataNode sNewNode = sShardNodeConfig.getNodeByName(sOldNode.getNodeName());
                    if (sNewNode != null)
                    {
                        sNewCachedConnMap.put(sNewNode, sConn);
                    }
                }
                sShardCon.setCachedConnections(sNewCachedConnMap);
            }
            catch (SQLException aEx)
            {
                AltibaseFailover.trySTF(aConn.failoverContext(), aEx);
            }
            CmProtocolContextShardConnect sShardConnectContext = sShardCon.getShardContextConnect();
            if (sShardConnectContext.getError() == null && aConn.isNodeConnection())
            {
                sShardConnectContext.createDataSources(sProps);
            }
        }
    }

    public void alignDataNodeConnection(AltibaseConnection aNodeConn,
                                        FailoverAlignInfo aAlignInfo) throws SQLException
    {
        if (aNodeConn.failoverContext() != null)
        {
            AltibaseFailoverContext sFailoverContext = aNodeConn.failoverContext();
            AltibaseFailoverServerInfo sOldServerInfo = sFailoverContext.getCurrentServer();
            setNextFailoverServer(sFailoverContext, sOldServerInfo);

            AltibaseFailoverServerInfo sNewServerInfo = sFailoverContext.getCurrentServer();
            boolean sFOResult = failoverConnectToSpecificServer(sFailoverContext, sNewServerInfo);

            try
            {
                if (sFOResult)
                {
                    throw new ShardFailOverSuccessException(aAlignInfo.getMessageText(),
                                                            ErrorDef.FAILOVER_SUCCESS,
                                                            aNodeConn.getNodeName(),
                                                            aNodeConn.getServer(),
                                                            sOldServerInfo.getPort());
                }
                else
                {
                    throw new ShardFailoverIsNotAvailableException(aAlignInfo.getMessageText(),
                                                                   ErrorDef.SHARD_NODE_FAILOVER_IS_NOT_AVAILABLE,
                                                                   aNodeConn.getNodeName(),
                                                                   aNodeConn.getServer(),
                                                                   sOldServerInfo.getPort());
                }
            }
            finally
            {
                aAlignInfo.setNeedAlign(false);
            }
        }
        else
        {
            // BUG-46790 node�� failover������ ���ٸ� ������ �߻���Ų��.
            throw new ShardFailoverIsNotAvailableException(aAlignInfo.getMessageText(),
                                                           ErrorDef.SHARD_NODE_FAILOVER_IS_NOT_AVAILABLE);
        }
    }

    private boolean failoverConnectToSpecificServer(AltibaseFailoverContext aContext,
                                                    AltibaseFailoverServerInfo aNewServerInfo) throws SQLException
    {
        shard_log("[failoverConnectToSpecificServer] Server: {0}:{1}",
                  new Object[] { aNewServerInfo.getServer(), aNewServerInfo.getPort() });

        boolean sFOSuccess = AltibaseFailover.doCTF(aContext, AltibaseFailover.FailoverType.STF,
                                                    aNewServerInfo);
        if (sFOSuccess)
        {
            aContext.getConnection().clearStatements4STF();
            // BUG-46790 shard connection�� ��쿡�� notify failover �޼����� ������.
            if (aContext.getConnection().isShardConnection())
            {
                updateShardNodeList(aContext.getConnection());
                notifyFailover(aContext.getConnection());
            }
            AltibaseXAResource sXAResource = aContext.getRelatedXAResource();
            if ((sXAResource != null) && (sXAResource.isOpen()))
            {
                aContext.setCallbackState(AltibaseFailover.CallbackState.IN_PROGRESS);
                sXAResource.xaOpen();
                aContext.setCallbackState(AltibaseFailover.CallbackState.STOPPED);
            }
        }
        else
        {
            shard_log(Level.SEVERE, "FailoverConnectToSpecificServer failed : {0}", aNewServerInfo);
        }

        return sFOSuccess;
    }

    private void setNextFailoverServer(AltibaseFailoverContext aFailoverContext,
                                       AltibaseFailoverServerInfo aOldServerInfo)
    {
        AltibaseFailoverServerInfoList sFailoverServerList = aFailoverContext.getFailoverServerList();

        for (AltibaseFailoverServerInfo sServerInfo : sFailoverServerList.getList())
        {
            if (sServerInfo == aOldServerInfo)
            {
                continue;
            }
            aFailoverContext.setCurrentServer(sServerInfo);

            break;
        }
    }

    public void setTouchedToAllNodes() throws SQLException
    {
        ShardNodeConfig sNodeConfig = mMetaConn.getShardContextConnect().getShardNodeConfig();
        sNodeConfig.setTouchedToAllNodes();
        for (DataNode sNode : sNodeConfig.getDataNodes())
        {
            // BUG-46790 ���Ŀ�ؼ��� ���� ��쿡�� ���� ������Ų��.
            if (!mMetaConn.getCachedConnections().containsKey(sNode))
            {
                mMetaConn.getNodeConnection(sNode);
            }
        }
    }

    private void tryReconnect(AltibaseConnection aNodeConn) throws ShardJdbcException
    {
        AltibaseProperties sProps = aNodeConn.getProp();
        AltibaseFailoverServerInfoList sServerList = new AltibaseFailoverServerInfoList();
        AltibaseFailoverServerInfo sPrimaryServerInfo = new AltibaseFailoverServerInfo(sProps.getServer(),
                                                                                       sProps.getPort(),
                                                                                       sProps.getDatabase());
        sServerList.add(0, sPrimaryServerInfo);

        AltibaseFailoverContext sFailoverContext = new AltibaseFailoverContext(aNodeConn, sProps, sServerList);
        sFailoverContext.setCurrentServer(sPrimaryServerInfo);

        boolean sResult = AltibaseFailover.connectToAlternateServer(sFailoverContext,
                                                                    AltibaseFailover.FailoverType.CTF,
                                                                    sProps,
                                                                    sPrimaryServerInfo);
        if (sResult)
        {
            throw new ShardFailOverSuccessException("Reconnect Success", ErrorDef.FAILOVER_SUCCESS,
                                                    aNodeConn.getNodeName(),
                                                    aNodeConn.getServer(),
                                                    aNodeConn.getPort());
        }
        else
        {
            CmOperation.throwShardFailoverIsNotAvailableException(aNodeConn.getNodeName(),
                                                                  aNodeConn.getServer(),
                                                                  aNodeConn.getPort());
        }
    }

    /**
     * ���Ŀ�ؼ��̰� ��ſ����� ������ alternateservers ������ ������ FailoverIsNotAvailableException�� �ø���
     * �׷��� ���� ��� STF�� ó���Ѵ�.
     * @param aEx �߻��� �ͼ���
     * @throws SQLException prepare, execute, fetch ���� ������ �߻��� ���
     */
    public static void tryShardFailOver(AltibaseConnection aNodeConn, SQLException aEx) throws SQLException
    {
        if (aNodeConn.isNodeConnection() && AltibaseFailover.isNeedToFailover(aEx) &&
            ( aNodeConn.failoverContext() == null))
        {
            AltibaseShardingConnection sMetaConn = aNodeConn.getMetaConnection();
            /* BUG-46790 failover ������ �ȵǾ� �ִ��� stf�� true�� ��� reconnect�� �õ��ؾ� �Ѵ�.  */
            if (sMetaConn.isSessionFailoverOn())
            {
                AltibaseShardingFailover sShardFailover = sMetaConn.getShardFailover();
                sShardFailover.tryReconnect(aNodeConn);
            }
            else
            {
                CmOperation.throwShardFailoverIsNotAvailableException(aNodeConn.getNodeName(),
                                                                      aNodeConn.getServer(),
                                                                      aNodeConn.getPort());
            }
        }
        else
        {
            AltibaseFailover.trySTF(aNodeConn.failoverContext(), aEx);
        }
    }
}
