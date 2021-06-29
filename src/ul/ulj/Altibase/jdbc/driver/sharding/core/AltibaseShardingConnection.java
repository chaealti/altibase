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

package Altibase.jdbc.driver.sharding.core;

import Altibase.jdbc.driver.*;
import Altibase.jdbc.driver.cm.*;
import Altibase.jdbc.driver.ex.Error;
import Altibase.jdbc.driver.ex.ErrorDef;
import Altibase.jdbc.driver.ex.ShardError;
import Altibase.jdbc.driver.ex.ShardJdbcException;
import Altibase.jdbc.driver.sharding.executor.ParallelProcessCallback;
import Altibase.jdbc.driver.sharding.executor.ExecutorEngine;
import Altibase.jdbc.driver.sharding.executor.ExecutorExceptionHandler;
import Altibase.jdbc.driver.sharding.executor.MultiThreadExecutorEngine;
import Altibase.jdbc.driver.util.AltiSqlProcessor;
import Altibase.jdbc.driver.util.AltibaseProperties;
import Altibase.jdbc.driver.util.StringUtils;

import javax.sql.DataSource;
import java.sql.*;
import java.util.*;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.Executor;

import static Altibase.jdbc.driver.sharding.util.ShardingTraceLogger.shard_log;
import static Altibase.jdbc.driver.util.AltibaseProperties.*;

/**
 * java.sql.Connection �� �����ϸ� ��Ÿ������ Ŀ�ؼ��� ������ �� �����ͳ�� ����Ʈ�� ���޹޴´�.
 */
public class AltibaseShardingConnection extends AbstractConnection
{
    public static final byte                DEFAULT_SHARD_CLIENT         = (byte)1;
    public static final byte                DEFAULT_SHARD_SESSION_TYPE   = (byte)0;
    private AltibaseConnection              mMetaConnection;
    private CmProtocolContextShardConnect   mShardContextConnect;
    private CmShardProtocol                 mCmShardProtocol;
    private Map<DataNode, Connection>       mCachedConnections;
    private AltibaseProperties              mProps;
    private ExecutorEngine                  mExecutorEngine;
    private boolean                         mIsClosed;
    private SQLWarning                      mWarning;
    private int                             mTransactionIsolation        = TRANSACTION_READ_UNCOMMITTED;
    private int                             mDefaultResultSetType        = ResultSet.TYPE_FORWARD_ONLY;
    private int                             mDefaultResultSetConcurrency = ResultSet.CONCUR_READ_ONLY;
    private int                             mDefaultResultSetHoldability = ResultSet.CLOSE_CURSORS_AT_COMMIT;
    private boolean                         mAutoCommit                  = true;
    private GlobalTransactionLevel          mGlobalTransactionLevel;
    private final LinkedList<Statement>     mOpenStatements;
    private boolean                         mIsLazyNodeConnect;
    private boolean                         mIsReshardEnabled;        // BUG-47460 resharding ��뿩��
    private AltibaseShardingFailover        mShardFailover;
    private JdbcMethodInvoker               mJdbcMethodInvoker           = new JdbcMethodInvoker();
    private Shard2PhaseCommitState          mShard2PhaseCommitState;
    private short                           mShardStatementRetry;
    private int                             mIndoubtFetchTimeout;
    private short                           mIndoubtFetchMethod;


    public AltibaseShardingConnection(AltibaseProperties aProp) throws SQLException
    {
        mShardContextConnect = new CmProtocolContextShardConnect(this);
        // connstr�� autocommit ������ ���, AltibaseShardingConnection.mAutoCommit�� �����ؾ� ��.
        mAutoCommit = aProp.isAutoCommit();
        mMetaConnection = new AltibaseConnection(aProp, null, this);
        mProps = mMetaConnection.getProp();
        mCmShardProtocol = new CmShardProtocol(mShardContextConnect);
        mOpenStatements = new LinkedList<Statement>();
        // mMetaConnection.mDistTxInfo�� mShardContextConnect.mDistTxInfo�� �����Ѵ�.
        // mShardContextConnect ���� ������ ���� mMetaConnection�� �������� �ʾƼ� ���Ŀ� setDistTxInfo�� ������ �������ش�.
        mShardContextConnect.setDistTxInfo(mMetaConnection.getDistTxInfo());
        // TCP, SSL, IB
        mShardContextConnect.setShardConnType(mProps.isSet(PROP_SHARD_CONNTYPE) ?
                                              CmConnType.toConnType(mProps.getShardConnType()) :
                                              CmConnType.toConnType(mProps.getConnType()));
        mCachedConnections = new ConcurrentHashMap<DataNode, Connection>();
        mIsLazyNodeConnect = mProps.isShardLazyConnectEnabled();
        mIsReshardEnabled = mProps.isReshardEnabled();
        mShardFailover = new AltibaseShardingFailover(this);
        mShard2PhaseCommitState = Shard2PhaseCommitState.SHARD_2PC_NORMAL;

        try
        {
            mCmShardProtocol.getNodeList();
            // BUG-46513 getNodeList�� ȣ�������� ���������� �߻��� ��� ó���� �ش�.
            if (mShardContextConnect.getError() != null)
            {
                mWarning = Error.processServerError(mWarning, mShardContextConnect.getError());
            }
            mShardContextConnect.createDataSources(mProps);
            // PROJ-2690 shard_lazy_connect�� false�϶��� Meta���� �� �ٷ� ���Ŀ�ؼ��� Ȯ���Ѵ�.
            if (!mIsLazyNodeConnect)
            {
                for (DataNode sEach : mShardContextConnect.getShardNodeConfig().getDataNodes())
                {
                    try
                    {
                        getNodeConnection(sEach, false);
                    }
                    catch (SQLException aEx)
                    {
                        // BUG-46513 lazy_connect false�� ��쿡�� node connect�� ������ �߻������� node name�� �߰��� ��� �Ѵ�.
                        String sSqlState = aEx.getSQLState();
                        int sErrorCode = aEx.getErrorCode();
                        throw new SQLException("[" + sEach.getNodeName() + "] " + aEx.getMessage(), sSqlState, sErrorCode);
                    }
                }
            }
        }
        catch (SQLException aException)
        {
            AltibaseFailover.trySTF(mMetaConnection.failoverContext(), aException);
        }
        mShardContextConnect.setAutoCommit(mAutoCommit);
        mExecutorEngine = new MultiThreadExecutorEngine();
        mShardContextConnect.setServerCharSet(mMetaConnection.getServerCharacterSet());
    }

    public String nativeSQL(String aSql)
    {
        return AltiSqlProcessor.processEscape(aSql);
    }

    public void setAutoCommit(boolean aAutoCommit) throws SQLException
    {
        mAutoCommit = aAutoCommit;
        mShardContextConnect.setAutoCommit(aAutoCommit);
        mJdbcMethodInvoker.recordMethodInvocation(Connection.class, "setAutoCommit",
                                                  new Class[] {boolean.class},
                                                  new Object[] {aAutoCommit});
        for (Connection sEach : mCachedConnections.values())
        {
            sEach.setAutoCommit(aAutoCommit);
        }
        mMetaConnection.setAutoCommit(mAutoCommit);
    }

    public void sendGlobalTransactionLevel(GlobalTransactionLevel aGlobalTransactionLevel) throws SQLException
    {
        AltibaseConnection sNodeConn;
        CmProtocolContextConnect sContext; 
        
        setGlobalTransactionLevel(aGlobalTransactionLevel);

        // alter session set�� ������ ���������Ƿ� metaConn�� sendProperties ���ʿ�. nodeConn�� ���ؼ��� ó��.
        for (Connection sEach : mCachedConnections.values())
        {
            sNodeConn = (AltibaseConnection)sEach;
            sContext = sNodeConn.getContext();
            sContext.clearProperties();
            sContext.addProperty(PROP_CODE_GLOBAL_TRANSACTION_LEVEL, aGlobalTransactionLevel.getValue());
            CmProtocol.sendProperties(sContext);
            if (sContext.getError() != null) 
            {
                mWarning = Error.processServerError(mWarning, sContext.getError());
            }
            sNodeConn.getProp().setProperty(AltibaseProperties.PROP_GLOBAL_TRANSACTION_LEVEL, aGlobalTransactionLevel.getValue());
        }
        
        getMetaConnection().getDistTxInfo().initDistTxInfo();
        getMetaConnection().setDistTxInfoForVerify();
    }

    public void sendShardStatementRetry(short aShardStatementRetry) throws SQLException
    {
        AltibaseConnection sNodeConn;
        CmProtocolContextConnect sContext; 
        
        setShardStatementRetry(aShardStatementRetry);

        // alter session set�� ������ ���������Ƿ� metaConn�� sendProperties ���ʿ�. nodeConn�� ���ؼ��� ó��.
        for (Connection sEach : mCachedConnections.values())
        {
            sNodeConn = (AltibaseConnection)sEach;
            sContext = sNodeConn.getContext();
            sContext.clearProperties();
            sContext.addProperty(PROP_CODE_SHARD_STATEMENT_RETRY, (byte)aShardStatementRetry);
            CmProtocol.sendProperties(sContext);
            if (sContext.getError() != null) 
            {
                mWarning = Error.processServerError(mWarning, sContext.getError());
            }
            sNodeConn.getProp().setProperty(AltibaseProperties.PROP_SHARD_STATEMENT_RETRY, aShardStatementRetry);
        }
    }

    public void sendIndoubtFetchTimeout(int aIndoubtFetchTimeout) throws SQLException
    {
        AltibaseConnection sNodeConn;
        CmProtocolContextConnect sContext; 
        
        setIndoubtFetchTimeout(aIndoubtFetchTimeout);

        // alter session set�� ������ ���������Ƿ� metaConn�� sendProperties ���ʿ�. nodeConn�� ���ؼ��� ó��.
        for (Connection sEach : mCachedConnections.values())
        {
            sNodeConn = (AltibaseConnection)sEach;
            sContext = sNodeConn.getContext();
            sContext.clearProperties();
            sContext.addProperty(PROP_CODE_INDOUBT_FETCH_TIMEOUT, aIndoubtFetchTimeout);
            CmProtocol.sendProperties(sContext);
            if (sContext.getError() != null) 
            {
                mWarning = Error.processServerError(mWarning, sContext.getError());
            }
            sNodeConn.getProp().setProperty(AltibaseProperties.PROP_INDOUBT_FETCH_TIMEOUT, aIndoubtFetchTimeout);
        }
    }

    public void sendIndoubtFetchMethod(short aIndoubtFetchMethod) throws SQLException
    {
        AltibaseConnection sNodeConn;
        CmProtocolContextConnect sContext; 
        
        setIndoubtFetchMethod(aIndoubtFetchMethod);

        // alter session set�� ������ ���������Ƿ� metaConn�� sendProperties ���ʿ�. nodeConn�� ���ؼ��� ó��.
        for (Connection sEach : mCachedConnections.values())
        {
            sNodeConn = (AltibaseConnection)sEach;
            sContext = sNodeConn.getContext();
            sContext.clearProperties();
            sContext.addProperty(PROP_CODE_INDOUBT_FETCH_METHOD, (byte)aIndoubtFetchMethod);
            CmProtocol.sendProperties(sContext);
            if (sContext.getError() != null) 
            {
                mWarning = Error.processServerError(mWarning, sContext.getError());
            }
            sNodeConn.getProp().setProperty(AltibaseProperties.PROP_INDOUBT_FETCH_METHOD, aIndoubtFetchMethod);
        }
    }

    public boolean getAutoCommit()
    {
        return mAutoCommit;
    }

    public void commit() throws SQLException
    {
        try
        {
            doTransaction(new TransactionCallback()
            {
                public void doTransaction(Connection aConn) throws SQLException
                {
                    shard_log("(NODE CONNECTION COMMIT)");
                    aConn.commit();
                }
            }, true);
        }
        catch (SQLException aEx)
        {
            // BUG-46790 ���� ��Ȳ������ Disconnect ���ο� ������� SMN ���� SQLWarning�� ó���ؾ� �Ѵ�.
            getNodeSqlWarnings(true, true);
            throw aEx;
        }
        finally
        {
            // BUG-46790 SMN ������Ʈ�� ���ܰ� �߻��� ���¿����� ����Ǿ�� �Ѵ�.
            if (shouldUpdateShardMetaNumber())
            {
                updateShardMetaNumber();
            }
        }
    }

    public void rollback() throws SQLException
    {
        try
        {
            doTransaction(new TransactionCallback()
            {
                public void doTransaction(Connection aConn) throws SQLException
                {
                    shard_log("(NODE CONNECTION ROLLBACK)");
                    aConn.rollback();
                }
            }, false);
        }
        catch (SQLException aEx)
        {
            getNodeSqlWarnings(false, true);
            throw aEx;
        }
        finally
        {
            // BUG-46790 SMN ������Ʈ�� ���ܰ� �߻��� ���¿����� ����Ǿ�� �Ѵ�.
            if (shouldUpdateShardMetaNumber())
            {
                updateShardMetaNumber();
            }
        }
    }

    private void doTransaction(final TransactionCallback aTransactionCallback,
                               boolean aIsCommit) throws SQLException
    {
        ShardNodeConfig sShardNodeConfig = mShardContextConnect.getShardNodeConfig();
        if (mGlobalTransactionLevel == GlobalTransactionLevel.ONE_NODE)
        {
            shard_log("(ONE NODE TRANSACTION)");
            DataNode sNode;
            if (mShardContextConnect.isNodeTransactionStarted())
            {
                sNode = mShardContextConnect.getShardOnTransactionNode();
                shard_log("(ON TRANSACTION NODE) {0}", sNode.getNodeName());
                aTransactionCallback.doTransaction(getNodeConnection(sNode));
            }
            mShardContextConnect.setShardOnTransactionNode(null);
            mShardContextConnect.setNodeTransactionStarted(false);
        }
        else if (mGlobalTransactionLevel == GlobalTransactionLevel.MULTI_NODE)
        {
            shard_log("(MULTI NODE TRANSACTION)");
            doMultiNodeTransaction(aTransactionCallback);
        }
        else
        {
            shard_log("(GLOBAL TRANSACTION)");
            doGlobalTransaction(aIsCommit, sShardNodeConfig);

            // PROJ-2733
            // commit ���� ��, ��Ÿ����� DistTxInfo�� �ʱ�ȭ ���ش�. �����ͳ���� DistTxInfo�� �� ��������� PropagateDistTxInfoToNode�� ���� overwrite �Ǳ� ������ �ʱ�ȭ ���ʿ�.
            mMetaConnection.getDistTxInfo().initDistTxInfo();
            mMetaConnection.setDistTxInfoForVerify();
        }

        getNodeSqlWarnings(aIsCommit, mShardContextConnect.needToDisconnect());
    }

    public void sendNodeTransactionBrokenReport() throws SQLException
    {
        if (mMetaConnection.isClosed())
        {
            Error.throwSQLException(ErrorDef.CLOSED_CONNECTION);
        }

        // transaction broken�� ���� meta connection���׸� node report ������ ��.
        NodeConnectionReport sReport = new NodeConnectionReport();
        sReport.setNodeReportType(NodeConnectionReport.NodeReportType.SHARD_NODE_REPORT_TYPE_TRANSACTION_BROKEN);
        getShardContextConnect().setTouched(true);
            
        try
        {
            mCmShardProtocol.shardNodeReport(sReport);
        }
        catch (SQLException aEx)
        {
            AltibaseFailover.trySTF(mMetaConnection.failoverContext(), aEx);
        }
        if (mShardContextConnect.getError() != null)
        {
            try
            {
                mWarning = Error.processServerError(mWarning, mShardContextConnect.getError());
            }
            finally
            {
                // BUG-46790 Exception�� �߻��ϴ��� shard align�۾��� �����ؾ� �Ѵ�.
                ShardError.processShardError(this, mShardContextConnect.getError());
            }
        }
    }
    
    private void doGlobalTransaction(final boolean aIsCommit,
                                     ShardNodeConfig aShardNodeConfig) throws SQLException
    {
        int sTouchedNodeCnt = 0;
        for (DataNode sEach : aShardNodeConfig.getDataNodes())
        {
            if (sEach.isTouched())
            {
                sTouchedNodeCnt++;
            }
        }
        if (mShardContextConnect.isTouched()) // BUG-46790 touch�� ��带 ī��Ʈ�Ҷ� ��Ÿ��嵵 �߰��ؾ� �Ѵ�.
        {
            sTouchedNodeCnt++;
        }
        
        try
        {
            /* 2��� �̻��̰� Commit�� ��츸 global transaction�� ����Ѵ�. */
            if (sTouchedNodeCnt >= 2 && aIsCommit) 
            {
                sendShardTransactionCommitRequest(aShardNodeConfig.getTouchedNodeList());
            } 
            else 
            {
                doMultiNodeTransaction(new TransactionCallback() 
                {
                    public void doTransaction(Connection aConn) throws SQLException 
                    {
                        shard_log("(NODE CONNECTION shardTransaction)");
                        ((AltibaseConnection) aConn).shardTransaction(aIsCommit);
                    }
                });
            }
            setShard2PhaseCommitState(Shard2PhaseCommitState.SHARD_2PC_NORMAL);
        }
        catch (SQLException aEx)
        {
            setShard2PhaseCommitState(Shard2PhaseCommitState.SHARD_2PC_COMMIT_FAIL);
            throw aEx;
        }
    }

    /**
     * �����ͳ��κ��� SQLWarning�� �����Ǿ��ٸ� �����Ͽ� mSqlWarning�� �����Ѵ�.<br>
     * �̶� warning �޼����� �������� ���Խ�Ų��.
     * @throws SQLException ���κ��� SQLWarning�� �޴� ���� ������ �߻��� ���.
     */
    private void getNodeSqlWarnings(boolean aIsCommit, boolean aNeedToDisconnect) throws SQLException
    {
        // node name ������ �����Ͽ� SQLWarning�� �����ϱ� ���� TreeMap ���
        Map<DataNode, Connection> sSortedMap = new TreeMap<DataNode, Connection>(mCachedConnections);
        for (Connection sEach : sSortedMap.values())
        {
            SQLWarning sSqlWarning = sEach.getWarnings();
            if (sSqlWarning == null) continue;
            if (sSqlWarning.getErrorCode() == ErrorDef.SHARD_META_NUMBER_INVALID)
            {
                if (aNeedToDisconnect)
                {
                    mWarning = Error.createWarning(mWarning,
                                                   ErrorDef.SHARD_SMN_OPERATION_FAILED,
                                                   "[" + ((AltibaseConnection)sEach).getNodeName() + "] The " +
                                                   ((aIsCommit) ? "Commit" : "Rollback"), sSqlWarning.getMessage());
                }
            }
            else
            {
                getSQLWarningFromDataNode(sEach);
            }
        }
    }

    private void doMultiNodeTransaction(final TransactionCallback aTransactionCallback) throws SQLException
    {
        try
        {
            /* BUG-46790 meta node�� ���ķ� ó������ �ʰ� ������ ó���Ѵ�. 
             * ���� node Ʈ����� ó���� ���ܰ� �߻��ϴ��� ����Ǿ�� �Ѵ�.  
             */
            if (mShardContextConnect.isTouched())
            {
                aTransactionCallback.doTransaction(mMetaConnection);
                mShardContextConnect.setTouched(false);
            }
        }
        finally
        {
            mExecutorEngine.doTransaction(getTouchedNodeConnections(),
                  new ParallelProcessCallback<Connection>()
                  {
                      public Void processInParallel(Connection aConn) throws SQLException
                      {
                          try
                          {
                              aTransactionCallback.doTransaction(aConn);
                              // BUG-46790 node touch clear�� ��庰�� Ʈ������� ���������� ���� �����ؾ� �Ѵ�.
                              clearNodeTouch((AltibaseConnection)aConn);
                          }
                          catch (ShardJdbcException aShardJdbcEx)
                          {
                              throw aShardJdbcEx;
                          }
                          catch (SQLException aException)
                          {
                              String sNodeName = ((AltibaseConnection)aConn).getNodeName();
                              ExecutorExceptionHandler.handleException(aException, sNodeName);
                          }
                          return null;
                      }
                  });
        }
    }

    private void clearNodeTouch(AltibaseConnection aNodeConn)
    {
        String sNodeName = aNodeConn.getNodeName();
        DataNode sNode = getShardNodeConfig().getNodeByName(sNodeName);
        sNode.setTouched(false);
    }

    private void sendShardTransactionCommitRequest(List<DataNode> aTouchedNodeList) throws SQLException
    {
        if (mMetaConnection.isClosed())
        {
            Error.throwSQLException(ErrorDef.CLOSED_CONNECTION);
        }

        if (!mAutoCommit)
        {
            try
            {
                shard_log("(SEND SHARD TRANSACTION COMMIT REQUEST) {0}", aTouchedNodeList);
                
                /* sendShardTransactionCommitRequest��  ���� �� ���
                   rollback�� ������ �� meta connection���ε� rollback�� ������ ����
                 */   
                mShardContextConnect.setTouched(true);  

                mCmShardProtocol.sendShardTransactionCommitRequest(aTouchedNodeList);
            }
            catch (SQLException aEx)
            {
                AltibaseFailover.trySTF(mMetaConnection.failoverContext(), aEx);
            }
            if (mShardContextConnect.getError() != null)
            {
                try
                {
                    mWarning = Error.processServerError(mWarning, mShardContextConnect.getError());
                }
                finally
                {
                    // BUG-46790 Exception�� �߻��ϴ��� shard align�۾��� �����ؾ� �Ѵ�.
                    ShardError.processShardError(this, mShardContextConnect.getError());
                }
            }

            // BUG-46790 global transaction �������� ������ ���������� ������ �����ġ�� clear ���ش�.
            for (DataNode sEach : getShardNodeConfig().getDataNodes())
            {
                sEach.setTouched(false);
            }
            mShardContextConnect.setTouched(false);
        }
    }

    private List<Connection> getTouchedNodeConnections()
    {
        List<DataNode> sTouchedNodeList = getShardNodeConfig().getTouchedNodeList();
        List<Connection> sResult = new ArrayList<Connection>();
        for (DataNode sEach : sTouchedNodeList)
        {
            Connection sTouchedNodeConn = mCachedConnections.get(sEach);
            if (sTouchedNodeConn != null)
            {
                sResult.add(sTouchedNodeConn);
            }
        }

        return sResult;
    }

    public void rollback(Savepoint aSavepoint) throws SQLException
    {
        Error.throwSQLException(ErrorDef.UNSUPPORTED_FEATURE,
                                "rollback(savepoint) is not supported in sharding");
    }

    public void close() throws SQLException
    {
        if (mIsClosed) return;
        closeAllOpenStatements();

        List<SQLException> sExceptions = new LinkedList<SQLException>();
        for (Map.Entry<DataNode, Connection> sEntry : mCachedConnections.entrySet())
        {
            try
            {
                Connection sCon = sEntry.getValue();
                shard_log("(NODE CONNECTION CLOSE) {0}", sEntry);
                sCon.close();
            }
            catch (SQLException aEx)
            {
                sExceptions.add(aEx);
            }
            mCachedConnections.remove(sEntry.getKey());
        }
        mJdbcMethodInvoker.throwSQLExceptionIfNecessary(sExceptions);
        shard_log("(META CONNECTION CLOSE) {0}", mMetaConnection);
        mMetaConnection.close();
        mIsClosed = true;
    }

    private void closeAllOpenStatements() throws SQLException
    {
        List<SQLException> sExceptions = new LinkedList<SQLException>();

        synchronized (mOpenStatements)
        {
            while (!mOpenStatements.isEmpty())
            {
                Statement sStmt = mOpenStatements.getFirst();
                try
                {
                    sStmt.close();
                }
                catch (SQLException aException)
                {
                    // BUG-46513 close ���� error �� �߻��ص� �ϴ� �� �ݰ� ���߿� ü������ ���´�.
                    sExceptions.add(aException);
                }
            }
        }
        mJdbcMethodInvoker.throwSQLExceptionIfNecessary(sExceptions);
    }

    void registerStatement(AltibaseShardingStatement aStatement)
    {
        synchronized (mOpenStatements)
        {
            mOpenStatements.add(aStatement);
        }
    }

    void unregisterStatement(AltibaseShardingStatement aStatement)
    {
        synchronized (mOpenStatements)
        {
            mOpenStatements.remove(aStatement);
        }
    }

    public boolean isClosed()
    {
        return mIsClosed;
    }

    public DatabaseMetaData getMetaData() throws SQLException
    {
        // BUG-46513 ���� node connection���� meta data���� �������� �ʰ� �ٷ� meta connection���� �����´�.
        return mMetaConnection.getMetaData();
    }

    public void setReadOnly(boolean aReadOnly)
    {
        mWarning = (aReadOnly) ? Error.createWarning(mWarning,
                                                     ErrorDef.READONLY_CONNECTION_NOT_SUPPORTED) : mWarning;
    }

    public boolean isReadOnly()
    {
        return false;
    }

    public void setCatalog(String catalog)
    {
        // �ƹ��� ������ ���� �ʴ´�. dbname�� �ٲ� �� ����.
        mWarning = Error.createWarning(mWarning, ErrorDef.CANNOT_RENAME_DB_NAME);
    }

    public String getCatalog()
    {
        return mProps.getDatabase();
    }

    public void setTransactionIsolation(int aLevel) throws SQLException
    {
        mTransactionIsolation = aLevel;
        mJdbcMethodInvoker.recordMethodInvocation(Connection.class, "setTransactionIsolation",
                                                  new Class[] {int.class},
                                                  new Object[] {aLevel});
        for (Connection sEach : mCachedConnections.values())
        {
            sEach.setTransactionIsolation(aLevel);
        }
    }

    public int getTransactionIsolation()
    {
        return mTransactionIsolation;
    }

    public SQLWarning getWarnings()
    {
        return mWarning;
    }

    public void clearWarnings() throws SQLException
    {
        mWarning = null;
        for (Connection sEach : mCachedConnections.values())
        {
            sEach.clearWarnings();
        }
    }

    public Statement createStatement() throws SQLException
    {
        return new AltibaseShardingStatement(this);
    }

    public Statement createStatement(int aResultSetType, int aResultSetConcurrency) throws SQLException
    {
        return new AltibaseShardingStatement(this, aResultSetType, aResultSetConcurrency);
    }

    public Statement createStatement(int aResultSetType, int aResultSetConcurrency,
                                     int aResultSetHoldability) throws SQLException
    {
        return new AltibaseShardingStatement(this, aResultSetType, aResultSetConcurrency,
                                             aResultSetHoldability);
    }

    public PreparedStatement prepareStatement(String aSql) throws SQLException
    {
        return new AltibaseShardingPreparedStatement(this, aSql, mDefaultResultSetType,
                                                     mDefaultResultSetConcurrency,
                                                     mDefaultResultSetHoldability);
    }

    public PreparedStatement prepareStatement(String aSql, int aResultSetType,
                                              int aResultSetConcurrency) throws SQLException
    {
        return new AltibaseShardingPreparedStatement(this, aSql, aResultSetType, aResultSetConcurrency,
                                                     mDefaultResultSetHoldability);
    }

    public PreparedStatement prepareStatement(String aSql, int aResultSetType, int aResultSetConcurrency,
                                              int aResultSetHoldability) throws SQLException
    {
        return new AltibaseShardingPreparedStatement(this, aSql, aResultSetType, aResultSetConcurrency,
                                                     aResultSetHoldability);
    }

    public PreparedStatement prepareStatement(String aSql, int aAutoGeneratedKeys) throws SQLException
    {
        AltibaseStatement.checkAutoGeneratedKeys(aAutoGeneratedKeys);

        AltibaseShardingPreparedStatement sShardStmt =
                new AltibaseShardingPreparedStatement(this, aSql, mDefaultResultSetType,
                                                      mDefaultResultSetConcurrency,
                                                      mDefaultResultSetHoldability);
        if (aAutoGeneratedKeys == Statement.RETURN_GENERATED_KEYS)
        {
            // BUG-47168 AutoGeneratedKey ���� �������� ������ �����Ѵ�.
            sShardStmt.makeQstrForGeneratedKeys(aSql, null, null);
        }
        else
        {
            sShardStmt.clearForGeneratedKeys();
        }

        return sShardStmt;
    }

    public PreparedStatement prepareStatement(String aSql, int[] aColumnIndexes) throws SQLException
    {
        AltibaseShardingPreparedStatement sShardStmt =
                new AltibaseShardingPreparedStatement(this, aSql, mDefaultResultSetType,
                                                      mDefaultResultSetConcurrency,
                                                      mDefaultResultSetHoldability);
        sShardStmt.makeQstrForGeneratedKeys(aSql, aColumnIndexes, null);

        return sShardStmt;
    }

    public PreparedStatement prepareStatement(String aSql, String[] aColumnNames) throws SQLException
    {
        AltibaseShardingPreparedStatement sShardStmt =
                new AltibaseShardingPreparedStatement(this, aSql, mDefaultResultSetType,
                                                      mDefaultResultSetConcurrency,
                                                      mDefaultResultSetHoldability);
        sShardStmt.makeQstrForGeneratedKeys(aSql, null, aColumnNames);

        return sShardStmt;
    }


    public CallableStatement prepareCall(String aSql) throws SQLException
    {
        return new AltibaseShardingCallableStatement(this, aSql, mDefaultResultSetType,
                                                     mDefaultResultSetConcurrency,
                                                     mDefaultResultSetHoldability);
    }

    public CallableStatement prepareCall(String aSql, int aResultSetType, int aResultSetConcurrency) throws SQLException
    {
        return new AltibaseShardingCallableStatement(this, aSql, aResultSetType,
                                                     aResultSetConcurrency,
                                                     mDefaultResultSetHoldability);
    }

    public CallableStatement prepareCall(String aSql, int aResultSetType, int aResultSetConcurrency,
                                         int aResultSetHoldability) throws SQLException
    {
        return new AltibaseShardingCallableStatement(this, aSql, aResultSetType,
                                                     aResultSetConcurrency,
                                                     aResultSetHoldability);
    }

    public Map<String, Class<?>> getTypeMap() throws SQLException
    {
        Error.throwSQLException(ErrorDef.UNSUPPORTED_FEATURE, "getTypeMap is not supported");
        return null;
    }

    public void setTypeMap(Map aMap) throws SQLException
    {
        Error.throwSQLException(ErrorDef.UNSUPPORTED_FEATURE, "User defined type");
    }

    public void setHoldability(int aHoldability)
    {
    }

    public int getHoldability()
    {
        return mDefaultResultSetHoldability;
    }

    public Savepoint setSavepoint() throws SQLException
    {
        Error.throwSQLException(ErrorDef.UNSUPPORTED_FEATURE, "setSavepoint is not supported in sharding");
        return null;
    }

    public Savepoint setSavepoint(String aName) throws SQLException
    {
        Error.throwSQLException(ErrorDef.UNSUPPORTED_FEATURE, "setSavepoint is not supported in sharding");
        return null;
    }

    public void releaseSavepoint(Savepoint aSavepoint) throws SQLException
    {
        Error.throwSQLException(ErrorDef.UNSUPPORTED_FEATURE, "releaseSavepoint is not supported in sharding");
    }

    public void setChannel(CmChannel aChannel)
    {
        mShardContextConnect.setChannel(aChannel);
        // BUG-46790 CmShardProtocol ��ü���� channel��ü�� �����Ǳ� ������ setChannel�� �ʱ�ȭ ���ش�.
        if (mCmShardProtocol != null)
        {
            mCmShardProtocol.setChannel(aChannel);
        }
    }

    public CmShardProtocol getShardProtocol()
    {
        return mCmShardProtocol;
    }

    Connection getNodeConnection(DataNode aNode) throws SQLException
    {
        return getNodeConnection(aNode, false);
    }

    /**
     * �����ͳ���� Connection�� �����´�. ���� �ش��ϴ� ����� Connection�� ĳ�ÿ� �����ϸ� �ٷ� �����ϰ� <br>
     * �ƴѰ�� ���� �����Ѵ�.
     * @param aNode �����ͳ�� ��ü
     * @param aIsToAlternate alternate���� ���� ����
     * @return �����ͳ�� Ŀ�ؼ�
     * @throws SQLException Ŀ�ؼ� ���� ���� ������ �߻��� ���
     */
    public Connection getNodeConnection(DataNode aNode, boolean aIsToAlternate) throws SQLException
    {
        if (mCachedConnections.containsKey(aNode))
        {
            shard_log("(CACHED NODE CONNECTION EXISTS) {0}", aNode);
            return mCachedConnections.get(aNode);
        }
        Connection sResult = makeNodeConnection(aNode, aIsToAlternate);
        mCachedConnections.put(aNode, sResult);
        mJdbcMethodInvoker.replayMethodsInvocation(sResult);
        return sResult;
    }

    private Connection makeNodeConnection(DataNode aNode, boolean aIsToAlternate) throws SQLException
    {
        Map<DataNode, DataSource> sDataSourceMap = mShardContextConnect.getDataSourceMap();
        Connection sResult;
        try
        {
            AltibaseDataSource sDataSource = (AltibaseDataSource)sDataSourceMap.get(aNode);
            // BUG-46790 failover align�� data node ���� ���� ���̶�� alternate�� �����Ѵ�.
            if (aIsToAlternate)
            {
                String sAlternateIp = aNode.getAlternativeServerIp();
                int sAlternatePort = aNode.getAlternativePortNo();
                if (!StringUtils.isEmpty(sAlternateIp) && sAlternatePort > 0)
                {
                    sDataSource.setServerName(sAlternateIp);
                    sDataSource.setPortNumber(sAlternatePort);
                }
            }
            sResult = sDataSource.getConnection(this);
            SQLWarning sWarning = sResult.getWarnings();
            if (sWarning != null)
            {
                if (sWarning.getErrorCode() == ErrorDef.SHARD_META_NUMBER_INVALID)
                {
                    if (mShardContextConnect.needToDisconnect())
                    {
                        getSQLWarningFromDataNode(sResult);
                    }
                }
                else
                {
                    getSQLWarningFromDataNode(sResult);
                }
            }
            if (aIsToAlternate) // BUG-46790 alternate���� ������ ������ �Ŀ��� primary server�� �ٽ� �����ش�.
            {
                setPrimaryServerInfo((AltibaseConnection)sResult, aNode);
            }
        }
        catch (SQLException aException)
        {
            // BUG-45967 ���Ŀ�ؼ� ���� SMN Invalid ������ �߻��� ��쿡�� SQLException�� ������.
            if (aException.getErrorCode() == ErrorDef.SHARD_META_NUMBER_INVALID)
            {
                Error.throwSQLException(ErrorDef.SHARD_SMN_OPERATION_FAILED,
                                        "The Node Connection", aException.getMessage());
            }
            throw aException;
        }

        shard_log("(DATASOURCE GETCONNECTION) {0}", aNode);

        return sResult;
    }

    /**
     * alternate server�� �ٷ� �����߱� ������ failover server list�� { alternate ip, alternate ip } �� ���� �����ȴ�.
     * primary ������ ������ { primary ip, alternate ip}�� ���� �ٲ�� �Ѵ�.
     * @param aNodeConn ��� Ŀ�ؼ�
     * @param aNode ������ ��� ��ü
     */
    private void setPrimaryServerInfo(AltibaseConnection aNodeConn, DataNode aNode)
    {
        AltibaseFailoverContext sFailoverContext = aNodeConn.failoverContext();
        AltibaseFailoverServerInfoList sFailoverServerInfoList = sFailoverContext.getFailoverServerList();
        AltibaseProperties sProps = sFailoverContext.connectionProperties();
        AltibaseFailoverServerInfo sPrimaryServerInfo = new AltibaseFailoverServerInfo(aNode.getServerIp(), aNode.getPortNo(),
                                                                                       sProps.getDatabase());
        sFailoverServerInfoList.set(0, sPrimaryServerInfo);
    }

    private void getSQLWarningFromDataNode(Connection aNodeConn) throws SQLException
    {
        SQLWarning sWarning = aNodeConn.getWarnings();
        String sNodeName = ((AltibaseConnection)aNodeConn).getNodeName();
        SQLWarning sNewWarning = new SQLWarning("[" + sNodeName + "] " + sWarning.getMessage(),
                                                sWarning.getSQLState(),
                                                sWarning.getErrorCode());
        if (mWarning == null)
        {
            mWarning = sNewWarning;
        }
        else
        {
            mWarning.setNextWarning(sNewWarning);
        }
    }

    private void setShardPinToDataNode(Connection aConnection, long aShardPin) throws SQLException
    {
        /* ���õ� �������̽��� ���� ������ setCatalog�� �̿��Ѵ�.
           ex : conn.setCatalog("shardpin:213213123123123");
         */
        aConnection.setCatalog("shardpin:" + aShardPin);
    }

    ExecutorEngine getExecutorEngine()
    {
        return mExecutorEngine;
    }

    public AltibaseConnection getMetaConnection()
    {
        return mMetaConnection;
    }

    public Map<DataNode, Connection> getCachedConnections()
    {
        return mCachedConnections;
    }

    public AltibaseProperties getProps()
    {
        return mProps;
    }

    public ShardNodeConfig getShardNodeConfig()
    {
        return mShardContextConnect.getShardNodeConfig();
    }

    /**
     * DataNode�� SMN�� Ŭ���̾�Ʈ�� SMN���� Ŭ ��쿡�� DataNode�� SMN�� �����Ѵ�.
     * @param aSMN error message���� �Ľ��� SMN��
     */
    public void setShardMetaNumberOfDataNode(long aSMN)
    {
        mShardContextConnect.setSMNOfDataNode(aSMN);
    }

    public void setNeedToDisconnect(boolean aNeedToDisconnect)
    {
        mShardContextConnect.setNeedToDisconnect(aNeedToDisconnect);
    }

    public boolean isLazyNodeConnect()
    {
        return mIsLazyNodeConnect;
    }

    boolean isReshardEnabled()
    {
        return mIsReshardEnabled;
    }

    /**
     * Meta�� ������ �ִ� ShardMetaNumer�� DataNode�� SMN���� ���Ͽ� ShardMetaNumber�� DataNode SMN����
     * ���� ��� true�� �����Ѵ�.
     * @return DataNode SMN�� �� �ֽ����� ����
     */
    public boolean shouldUpdateShardMetaNumber()
    {
        long sSMN = mShardContextConnect.getShardMetaNumber();
        long sSMNOfDataNode = mShardContextConnect.getSMNOfDataNode();
        boolean sResult = false;

        if ( ( sSMN != 0 ) &&
             ( sSMN < sSMNOfDataNode ) &&
             ( !mShardContextConnect.needToDisconnect() ) &&
             ( hasNodata()) )
        {
            sResult = true;
        }

        return sResult;
    }

    private boolean hasNodata()
    {
        synchronized (mOpenStatements)
        {
            for (Statement sEach : mOpenStatements)
            {
                AltibaseShardingStatement sStmt = (AltibaseShardingStatement)sEach;
                if (sStmt.isPrepared() && !sStmt.hasNoData())
                {
                    return false;
                }
            }
        }

        return true;
    }

    /**
     * ��Ÿ������ getNodeList���������� �ٽ� ��û�� SMN���� �����Ѵ�.
     */
    public void updateShardMetaNumber() throws SQLException
    {
        long sOldShardPin = mShardContextConnect.getShardPin();
        long sOldSMN = mShardContextConnect.getShardMetaNumber();
        mCmShardProtocol.getNodeList();
        if (mShardContextConnect.getError() != null)
        {
            mWarning = Error.processServerError(mWarning, mShardContextConnect.getError());
        }

        if (sOldShardPin != mShardContextConnect.getShardPin())
        {
            Error.throwSQLException(ErrorDef.SHARD_PIN_CHANGED);
        }

        mShardContextConnect.createDataSources(mProps);

        long sNewSMN = mShardContextConnect.getShardMetaNumber();
        if (sOldSMN < sNewSMN)
        {
            for (Connection sEach : mCachedConnections.values())
            {
                AltibaseConnection sNodeConn = (AltibaseConnection)sEach;
                sNodeConn.sendSMNProperty(sNewSMN);
            }
        }
    }

    long getShardMetaNumber()
    {
        return mShardContextConnect.getShardMetaNumber();
    }

    public void removeNode(DataNode aNode)
    {
        try
        {
            Connection sRemovedConn =  mCachedConnections.remove(aNode);
            if (sRemovedConn != null)
            {
                sRemovedConn.close();
            }
        }
        catch (SQLException aEx)
        {
            shard_log("(CLOSE NODE CONNECTION) Error occurred : {0} {1} ",
                      new Object[] { aNode, aEx.getMessage() });
        }
        /* BUG-46513 node connection�� close�ϴ��� �ش��ϴ� connection�� ���� statement�� statement route map
           �� ������ �� �ֱ� ������ clear���ش�. */
        synchronized (mOpenStatements)
        {
            for (Statement sEach : mOpenStatements)
            {
                ((AltibaseShardingStatement)sEach).removeFromStmtRouteMap(aNode);
            }
        }
    }

    private interface TransactionCallback
    {
        void doTransaction(Connection acon) throws SQLException;
    }

    public CmProtocolContextShardConnect getShardContextConnect()
    {
        return mShardContextConnect;
    }

    public AltibaseShardingFailover getShardFailover()
    {
        return mShardFailover;
    }

    public void setWarning(SQLWarning aWarning)
    {
        mWarning = aWarning;
    }

    public void registerFailoverCallback(AltibaseFailoverCallback aFailoverCallback,
                                         Object aAppContext) throws SQLException
    {
        // BUG-46790 meta connection stf�� ���� registerFailoverCallback�� ����Ѵ�.
        mMetaConnection.registerFailoverCallback(aFailoverCallback, aAppContext);
    }

    public void deregisterFailoverCallback() throws SQLException
    {
        mMetaConnection.deregisterFailoverCallback();
    }

    public void setClosed(boolean aIsClosed)
    {
        mIsClosed = aIsClosed;
    }

    void setCachedConnections(Map<DataNode, Connection> aCachedConnections)
    {
        mCachedConnections = aCachedConnections;
    }

    boolean isSessionFailoverOn()
    {
        return mProps.getBooleanProperty(AltibaseProperties.PROP_FAILOVER_USE_STF);
    }

    public GlobalTransactionLevel getGlobalTransactionLevel()
    {
        return mGlobalTransactionLevel;
    }

    public void setGlobalTransactionLevel(GlobalTransactionLevel aGlobalTransactionLevel)
    {
        mGlobalTransactionLevel = aGlobalTransactionLevel;
    }

    public short getShardStatementRetry()
    {
        return mShardStatementRetry;
    }

    public void setShardStatementRetry(short aShardStatementRetry)
    {
        mShardStatementRetry = aShardStatementRetry;
    }

    public int getIndoubtFetchTimeout()
    {
        return mIndoubtFetchTimeout;
    }

    public void setIndoubtFetchTimeout(int aIndoubtFetchTimeout)
    {
        mIndoubtFetchTimeout = aIndoubtFetchTimeout;
    }

    public short getIndoubtFetchMethod()
    {
        return mIndoubtFetchMethod;
    }

    public void setIndoubtFetchMethod(short aIndoubtFetchMethod)
    {
        mIndoubtFetchMethod = aIndoubtFetchMethod;
    }

    public void setShard2PhaseCommitState(Shard2PhaseCommitState aShard2PhaseCommitState)
    {
        mShard2PhaseCommitState = aShard2PhaseCommitState;
    }

    public Shard2PhaseCommitState getShard2PhaseCommitState()
    {
        return mShard2PhaseCommitState;
    }

    public void checkCommitState() throws SQLException
    {
        if (getShard2PhaseCommitState() == Shard2PhaseCommitState.SHARD_2PC_COMMIT_FAIL)
        {   
            Error.throwSQLException(ErrorDef.SHARD_NEED_ROLLBACK);
        }  
    }

    @Override
    public boolean isValid(int aTimeout) throws SQLException
    {
        throw Error.createSQLFeatureNotSupportedException();
    }

    @Override
    public void setClientInfo(String aName, String aValue) throws SQLClientInfoException
    {
        throw new SQLClientInfoException("setClientInfo is not supported in sharding", null);
    }

    @Override
    public void setClientInfo(Properties aProperties) throws SQLClientInfoException
    {
        throw new SQLClientInfoException("setClientInfo is not supported in sharding", null);
    }

    @Override
    public String getClientInfo(String aName) throws SQLException
    {
        throw new SQLClientInfoException("getClientInfo is not supported in sharding", null);
    }

    @Override
    public Properties getClientInfo() throws SQLException
    {
        throw new SQLClientInfoException("getClientInfo is not supported in sharding", null);
    }

    @Override
    public void abort(Executor aExecutor) throws SQLException
    {
        throw Error.createSQLFeatureNotSupportedException();
    }

    @Override
    public void setNetworkTimeout(Executor aExecutor, int aMillisecs) throws SQLException
    {
        throw Error.createSQLFeatureNotSupportedException();
    }

    @Override
    public int getNetworkTimeout() throws SQLException
    {
        throw Error.createSQLFeatureNotSupportedException();
    }
}
