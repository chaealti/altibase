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

import Altibase.jdbc.driver.AltibaseConnection;
import Altibase.jdbc.driver.AltibaseStatement;
import Altibase.jdbc.driver.cm.CmProtocolContextShardConnect;
import Altibase.jdbc.driver.cm.CmProtocolContextShardStmt;
import Altibase.jdbc.driver.ex.*;
import Altibase.jdbc.driver.ex.Error;
import Altibase.jdbc.driver.sharding.executor.*;
import Altibase.jdbc.driver.sharding.merger.IteratorStreamResultSetMerger;
import Altibase.jdbc.driver.sharding.routing.*;

import java.sql.*;
import java.util.*;
import java.util.concurrent.ConcurrentHashMap;

import static Altibase.jdbc.driver.sharding.util.ShardingTraceLogger.shard_log;

public class DataNodeShardingStatement implements InternalShardingStatement
{
    AltibaseShardingConnection         mMetaConn;
    CmProtocolContextShardStmt         mShardStmtCtx;
    ResultSet                          mCurrentResultSet;
    // BUG-47460 RouteResult�� ������ ��ü�� ó������ �ʰ� ArrayList�� ó��
    List<DataNode>                     mRouteResult;
    AltibaseShardingStatement          mShardStmt;
    Map<DataNode, Statement>           mRoutedStatementMap;
    int                                mResultSetType;
    int                                mResultSetConcurrency;
    int                                mResultSetHoldability;
    private SQLWarning                 mSqlwarning;
    private int                        mFetchSize;
    private StatementExecutor          mStmtExecutor;

    DataNodeShardingStatement(AltibaseShardingConnection aMetaConn,
                              int aResultSetType, int aResultSetConcurrency,
                              int aResultSetHoldability, AltibaseShardingStatement aShardStmt)
    {
        mMetaConn = aMetaConn;
        mShardStmt = aShardStmt;
        mShardStmtCtx = aShardStmt.mShardStmtCtx;
        mResultSetType = aResultSetType;
        mResultSetConcurrency = aResultSetConcurrency;
        mResultSetHoldability = aResultSetHoldability;
        mRoutedStatementMap = new ConcurrentHashMap<DataNode, Statement>();
        mStmtExecutor = new StatementExecutor(mMetaConn.getExecutorEngine());
    }

    public ResultSet executeQuery(String aSql) throws SQLException
    {
        ResultSet sResult;
        List<Statement> sStatements = null;

        try
        {
            sStatements = route(aSql);
            List<ResultSet> sResultSets = mStmtExecutor.executeQuery(sStatements);
            mShardStmtCtx.setUpdateRowcount(getNodeUpdateRowCount());
            sResult = new AltibaseShardingResultSet(sResultSets,
                                                    new IteratorStreamResultSetMerger(sResultSets),
                                                    mShardStmt);
            setOneNodeTransactionInfo(mRouteResult);
            getNodeSqlWarnings(mMetaConn.getShardContextConnect().needToDisconnect());
        }
        catch (ShardJdbcException aShardJdbcEx)
        {
            clearRoutedStatementMap(aShardJdbcEx);
            getNodeSqlWarnings(true);
            throw aShardJdbcEx;
        }
        catch (SQLException aEx)
        {
            processExecuteError(sStatements, aEx);
            getNodeSqlWarnings(true);
            throw aEx;
        }
        finally
        {
            mCurrentResultSet = null;
        }

        mCurrentResultSet = sResult;
        return sResult;
    }

    public int executeUpdate(String aSql) throws SQLException
    {
        int sResult;
        List<Statement> sStatementUnits = null;

        try
        {
            sStatementUnits = route(aSql);
            sResult = mStmtExecutor.executeUpdate(sStatementUnits);
            mShardStmtCtx.setUpdateRowcount(sResult);
        }
        catch (ShardJdbcException aShardJdbcEx)
        {
            clearRoutedStatementMap(aShardJdbcEx);
            getNodeSqlWarnings(true);
            throw aShardJdbcEx;
        }
        catch (SQLException aEx)
        {
            processExecuteError(sStatementUnits, aEx);
            getNodeSqlWarnings(true);
            throw aEx;
        }
        setOneNodeTransactionInfo(mRouteResult);
        getNodeSqlWarnings(mMetaConn.getShardContextConnect().needToDisconnect());

        return sResult;
    }

    public void close() throws SQLException
    {
        try
        {
            // BUG-47460 node statement close�� ���ķ� ó��
            mMetaConn.getExecutorEngine().closeStatements(mRoutedStatementMap.values());
        }
        finally
        {
            mRoutedStatementMap.clear();
        }
    }

    public int getMaxFieldSize() throws SQLException
    {
        return mRoutedStatementMap.isEmpty() ? 0 : mRoutedStatementMap.values().iterator().next().getMaxFieldSize();
    }

    public void setMaxFieldSize(int aMax) throws SQLException
    {
        for (Statement sEach : mRoutedStatementMap.values())
        {
            sEach.setMaxFieldSize(aMax);
        }
    }

    public int getMaxRows() throws SQLException
    {
        return mRoutedStatementMap.isEmpty() ? -1 : mRoutedStatementMap.values().iterator().next().getMaxRows();
    }

    public void setMaxRows(int aMax) throws SQLException
    {
        for (Statement sEach : mRoutedStatementMap.values())
        {
            sEach.setMaxRows(aMax);
        }
    }

    public void setEscapeProcessing(boolean aEnable) throws SQLException
    {
        for (Statement sEach : mRoutedStatementMap.values())
        {
            sEach.setEscapeProcessing(aEnable);
        }
    }

    public int getQueryTimeout() throws SQLException
    {
        return mRoutedStatementMap.isEmpty() ? 0 : mRoutedStatementMap.values().iterator().next().getQueryTimeout();
    }

    public void setQueryTimeout(int aSeconds) throws SQLException
    {
        for (Statement sEach : mRoutedStatementMap.values())
        {
            sEach.setQueryTimeout(aSeconds);
        }
    }

    public void cancel() throws SQLException
    {
        for (Statement sEach : mRoutedStatementMap.values())
        {
            sEach.cancel();
        }
    }

    public SQLWarning getWarnings()
    {
        return mSqlwarning;
    }

    public void clearWarnings()
    {
        mSqlwarning = null;
    }

    public void setCursorName(String aName) throws SQLException
    {
        Error.throwSQLException(ErrorDef.UNSUPPORTED_FEATURE, "cursor name and positioned update");
    }

    public boolean execute(String aSql) throws SQLException
    {
        boolean sResult;
        List<Statement> sStatementUnits = null;
        
        try
        {
            sStatementUnits = route(aSql);
            sResult = mStmtExecutor.execute(sStatementUnits);
            mShardStmtCtx.setUpdateRowcount(getNodeUpdateRowCount());
            setOneNodeTransactionInfo(mRouteResult);
            getNodeSqlWarnings(mMetaConn.getShardContextConnect().needToDisconnect());
        }
        catch (ShardJdbcException aShardJdbcEx)
        {
            /* BUGBUG : Ŀ�ؼ��� �������µ� sessionfailover=off�� ��� exception�� �׳� �ø���... ���⼭ �ɷ����°�??
                        partial rollback ó�� �ʿ�?
                        failover�� �Ϲ� ���� �� �� �߻��ϸ� � exception�� �ö������ Ȯ�� �ʿ�.
            */
            clearRoutedStatementMap(aShardJdbcEx);
            getNodeSqlWarnings(true);
            throw aShardJdbcEx;
        }
        catch (SQLException aEx)
        {
            processExecuteError(sStatementUnits, aEx);
            getNodeSqlWarnings(true);
            throw aEx;
        }
        finally
        {
            mCurrentResultSet = null;
        }

        return sResult;
    }

    public ResultSet getResultSet() throws SQLException
    {
        if (mCurrentResultSet != null)
        {
            return mCurrentResultSet;
        }

        // BUG-47360 prepare�� execute�� ���ϴ� ��� route result�� ���⶧���� �̷� ��� ���ܸ� �ø���.
        if (mRouteResult == null)
        {
            Error.throwSQLException(ErrorDef.STATEMENT_NOT_YET_EXECUTED);
        }

        List<ResultSet> sResultSets = new ArrayList<ResultSet>(mRouteResult.size());

        // BUG-47360 mRouteResult�κ��� route�� ��带 ã�� Statement.getResultSet()�� �����Ѵ�.
        for (DataNode sEach : mRouteResult)
        {
            ResultSet sRs = mRoutedStatementMap.get(sEach).getResultSet();
            if (sRs != null) // BUG-47360 ResultSet�� ������ ��쿡�� sResultSets�� add�Ѵ�.
            {
                sResultSets.add(sRs);
            }
        }

        if (sResultSets.size() == 1)
        {
            mCurrentResultSet = sResultSets.get(0);
        }
        else if (sResultSets.size() > 1)
        {
            // BUG-47360 node�κ��� ������ ResultSet�� 2�� �̻��� ��� AltibaseShardingResultSet�� �����Ѵ�.
            mCurrentResultSet = new AltibaseShardingResultSet(sResultSets,
                                                              new IteratorStreamResultSetMerger(sResultSets),
                                                              mShardStmt);
        }

        return mCurrentResultSet;
    }

    public int getUpdateCount()
    {
        int sResult = mShardStmtCtx.getUpdateRowcount();
        // BUG-47338 getUpdateCount�� �ѹ� ȣ��� �ڿ� -1 ������ �ʱ�ȭ �ؾ� �Ѵ�.
        mShardStmtCtx.setUpdateRowcount(AltibaseStatement.DEFAULT_UPDATE_COUNT);

        return sResult;
    }

    public boolean getMoreResults() throws SQLException
    {
        return getMoreResultsInternal(new GetMoreResultsTemplate()
        {
            public boolean getMoreResults(Statement aStmt) throws SQLException
            {
                return aStmt.getMoreResults();
            }
        });
    }

    public boolean getMoreResults(final int aCurrent) throws SQLException
    {
        return getMoreResultsInternal(new GetMoreResultsTemplate()
        {
            public boolean getMoreResults(Statement aStmt) throws SQLException
            {
                return aStmt.getMoreResults(aCurrent);
            }
        });
    }

    private boolean getMoreResultsInternal(GetMoreResultsTemplate aTemplate) throws SQLException
    {
        boolean sResult = true;
        int sNoDataCount = 0;

        for (DataNode sEach : mRouteResult)
        {
            Statement sStmt = mRoutedStatementMap.get(sEach);
            if (!aTemplate.getMoreResults(sStmt))
            {
                sNoDataCount++;
            }
        }
        /* BUG-47360 node statement ��ü�� getMoreResults�� ������ ��쿡�� ����� false�� �ø���.
           �� �� ���� getMoreResults�� �����ϸ� true�� �����Ѵ�. */
        if (sNoDataCount == mRouteResult.size())
        {
            sResult = false;
        }

        // BUG-47360 getMoreResults�� ȣ��� ��� ���� resultset�� �����Ѿ� �ϱ⶧���� mCurrentResultSet�� �ʱ�ȭ ���ش�.
        mCurrentResultSet = null;

        return sResult;
    }

    public void setFetchSize(int aRows) throws SQLException
    {
        mFetchSize = aRows;
        for (Statement sEach : mRoutedStatementMap.values())
        {
            sEach.setFetchSize(aRows);
        }
    }

    public int getFetchSize()
    {
        return mFetchSize;
    }

    public void clearBatch() throws SQLException
    {
        Error.throwSQLException(ErrorDef.UNSUPPORTED_FEATURE, "clearBatch is not supported in sharding");
    }

    public int[] executeBatch() throws SQLException
    {
        Error.throwSQLException(ErrorDef.UNSUPPORTED_FEATURE, "executeBatch is not supported in sharding");
        return null;
    }

    public Connection getConnection()
    {
        return mMetaConn;
    }

    public ResultSet getGeneratedKeys() throws SQLException
    {
        Error.throwSQLException(ErrorDef.UNSUPPORTED_FEATURE,
                                "AutoGeneratedKey is not supported in client-side sharding");
        return null;
    }

    public int executeUpdate(String aSql, int aAutoGeneratedKeys) throws SQLException
    {
        Error.throwSQLException(ErrorDef.UNSUPPORTED_FEATURE,
                                "AutoGeneratedKey is not supported in client-side sharding");
        return 0;
    }

    public int executeUpdate(String aSql, int[] aColumnIndexes) throws SQLException
    {
        int sResult;
        List<Statement> sStatementUnits = null;

        try
        {
            sStatementUnits = route(aSql);
            sResult =  mStmtExecutor.executeUpdate(aColumnIndexes, sStatementUnits);
            mShardStmtCtx.setUpdateRowcount(sResult);
            setOneNodeTransactionInfo(mRouteResult);
            getNodeSqlWarnings(mMetaConn.getShardContextConnect().needToDisconnect());
        }
        catch (ShardJdbcException aShardJdbcEx)
        {
            clearRoutedStatementMap(aShardJdbcEx);
            getNodeSqlWarnings(true);
            throw aShardJdbcEx;
        }
        catch (SQLException aEx)
        {
            processExecuteError(sStatementUnits, aEx);
            getNodeSqlWarnings(true);
            throw aEx;
        }
        finally
        {
            mCurrentResultSet = null;
        }

        return sResult;
    }

    public int executeUpdate(String aSql, String[] aColumnNames) throws SQLException
    {
        int sResult;
        List<Statement> sStatementUnits = null;

        try
        {
            sStatementUnits = route(aSql);
            sResult = mStmtExecutor.executeUpdate(aColumnNames, sStatementUnits);
            mShardStmtCtx.setUpdateRowcount(sResult);
            setOneNodeTransactionInfo(mRouteResult);
            getNodeSqlWarnings(mMetaConn.getShardContextConnect().needToDisconnect());
        }
        catch (ShardJdbcException aShardJdbcEx)
        {
            clearRoutedStatementMap(aShardJdbcEx);
            getNodeSqlWarnings(true);
            throw aShardJdbcEx;
        }
        catch (SQLException aEx)
        {
            processExecuteError(sStatementUnits, aEx);
            getNodeSqlWarnings(true);
            throw aEx;
        }
        finally
        {
            mCurrentResultSet = null;
        }

        return sResult;
    }

    public boolean execute(String aSql, int aAutoGeneratedKeys) throws SQLException
    {
        Error.throwSQLException(ErrorDef.UNSUPPORTED_FEATURE,
                                "AutoGeneratedKey is not supported in client-side sharding");
        return false;
    }

    public boolean execute(String aSql, int[] aColumnIndexes) throws SQLException
    {
        Error.throwSQLException(ErrorDef.UNSUPPORTED_FEATURE,
                                "AutoGeneratedKey is not supported in client-side sharding");
        return false;
    }

    public boolean execute(String aSql, String[] aColumnNames) throws SQLException
    {
        Error.throwSQLException(ErrorDef.UNSUPPORTED_FEATURE,
                                "AutoGeneratedKey is not supported in client-side sharding");
        return false;
    }

    private List<Statement> route(final String aSql) throws SQLException
    {
        mCurrentResultSet = null;
        mRouteResult = createRoutingEngine().route(aSql, null);
        ExecutorEngine sExecutorEngine = mMetaConn.getExecutorEngine();

        // PROJ-2690 ExecutorEngine�� ���� ���ķ� Statement ��ü ����
        List<Statement> sResults = sExecutorEngine.generateStatement(
                                mRouteResult,
                                new GenerateCallback<Statement>()
                                {
                                    public Statement generate(DataNode aNode) throws SQLException
                                    {
                                        Connection sConn = mMetaConn.getNodeConnection(aNode);
                                        Statement sStatement = getStatement(aNode, sConn);
                                        ((AltibaseStatement)sStatement).setSql(aSql);
                                        mRoutedStatementMap.put(aNode, sStatement);
                                        return sStatement;
                                    }
                                });
        // BUG-47145 node statement�� ���������� ������ ���� node touch�� �����Ѵ�.
        if (!mShardStmtCtx.isAutoCommitMode())
        {
            touchNodes();
        }

        calcDistTxInfo(sResults);

        return sResults;
    }
    
    void calcDistTxInfo(List<Statement> aResults)
    {
        AltibaseStatement sStatement;
        short sExecuteNodeCnt = (short)aResults.size();
        // CalcDistTxInfoForDataNode���� sExecuteNodeCnt=1�϶��� DistTxInfo.mBeforeExecutedNodeId�� sNodeIndex�� ���ϹǷ�, sNodeIndex�� ù��° ����� id�� �����ϸ� ��.
        // ����� �Ϲ� execute�� ���� mRouteResult�� ó���� ��� ��尡 ���������, batch�� ��� ������ addBatch�� route ����� ����Ǿ� ����.
        // addBatch ������ route ����� mRouteResult�� overwrite��.
        short sNodeIndex = (short)mRouteResult.get(0).getNodeId();
        mMetaConn.getMetaConnection().getDistTxInfo().calcDistTxInfoForDataNode(mMetaConn, sExecuteNodeCnt, sNodeIndex);

        for (int i = 0; i < sExecuteNodeCnt; i++)
        {
            sStatement = (AltibaseStatement)aResults.get(i);
            sStatement.getProtocolContext().getDistTxInfo().propagateDistTxInfoToNode(mMetaConn.getMetaConnection().getDistTxInfo());
        }

        mMetaConn.getMetaConnection().setDistTxInfoForVerify();
    }

    private Statement getStatement(DataNode aNode, Connection aConn) throws SQLException
    {
        Statement sStatement = mRoutedStatementMap.get(aNode);
        if (sStatement != null) return sStatement;
        
        // routed statemap�� �ش� statement�� ���ٸ� ���� �����Ѵ�.
        sStatement = aConn.createStatement(mResultSetType, mResultSetConcurrency, mResultSetHoldability);
        mShardStmt.setShardMetaNumber(mMetaConn.getShardMetaNumber());
        mShardStmt.replayMethodsInvocation(sStatement);
        
        return sStatement;
    }

    RoutingEngine createRoutingEngine()
    {
        int sShardValueCnt = mShardStmtCtx.getShardAnalyzeResult().getShardValueCount();
        int sShardSubValueCnt = mShardStmtCtx.getShardAnalyzeResult().getShardSubValueCount();
        ShardSplitMethod sSplitMethod = mShardStmtCtx.getShardAnalyzeResult().getShardSplitMethod();
        switch (sSplitMethod)
        {
            case CLONE:
                /* shard value�� ���� ���� random�ϰ� ��带 �����ؾ� �ϸ� �׷��� ����
                   ��쿡�� ��� ��尡 �������� �ȴ�. */
                return (sShardValueCnt == 0) ? new RandomNodeRoutingEngine(mShardStmtCtx) :
                       new AllNodesRoutingEngine(mShardStmtCtx);
            case SOLO:
                return new AllNodesRoutingEngine(mShardStmtCtx);
            case LIST:
            case RANGE:
            case HASH:
                if (sShardValueCnt >= 0 && sShardSubValueCnt > 0)
                {
                    return new CompositeShardKeyRoutingEngine(mShardStmtCtx);
                }
                else if (sShardValueCnt > 0 && sShardSubValueCnt == 0)
                {
                    return new SimpleShardKeyRoutingEngine(mShardStmtCtx);
                }
                else  // shard value �� ���ٸ� ��� ��尡 ���� ����� �ȴ�.
                {
                    return new AllNodesRoutingEngine(mShardStmtCtx);
                }
        }

        return null;
    }

    /**
     * Ʈ����� ����϶� ����� touch �÷��׸� �����Ѵ�. ���� �̶� onenode transaction�� ��쿡�� <br>
     * Ʈ������� ��ȿ���� üũ�Ѵ�.
     * @throws SQLException onenode�϶� �������� Ʈ����� ���°� �ƴ� ���
     */
    void touchNodes() throws SQLException
    {
        if (mMetaConn.getGlobalTransactionLevel() == GlobalTransactionLevel.ONE_NODE)
        {
            /* one node tx���� 1���̻� ��尡 ���õ� ��� ���� */
            if (mRouteResult.size() > 1)
            {
                Error.throwSQLException(ErrorDef.SHARD_MULTINODE_TRANSACTION_REQUIRED);
            }
            // one node tx���� ���� ���� �ٸ��� ����
            DataNode sShardOnTransactionNode = mShardStmtCtx.getShardContextConnect().getShardOnTransactionNode();
            DataNode sCurrentNode = mRouteResult.get(0);
            if (!isValidTransaction(sShardOnTransactionNode, sCurrentNode))
            {
                Error.throwSQLException(ErrorDef.SHARD_INVALID_NODE_TOUCH);
            }
        }

        // touch nodes
        for (DataNode sEach : mRouteResult)
        {
            sEach.setTouched(true);
        }
    }

    private boolean isValidTransaction(DataNode sOldNode, DataNode sCurrentNode)
    {
        return !mShardStmtCtx.getShardContextConnect().isNodeTransactionStarted() ||
               sOldNode.equals(sCurrentNode);
    }

    void setOneNodeTransactionInfo(List<DataNode> aSqlExecutionUnits)
    {
        if (aSqlExecutionUnits.size() == 0) return;

        if (mMetaConn.getGlobalTransactionLevel() == GlobalTransactionLevel.ONE_NODE)
        {
            DataNode sNode = aSqlExecutionUnits.iterator().next();
            CmProtocolContextShardConnect sShardContextConnect = mShardStmtCtx.getShardContextConnect();
            sShardContextConnect.setShardOnTransactionNode(sNode);
            sShardContextConnect.setNodeTransactionStarted(true);
        }
    }

    void setShardStmtCtx(CmProtocolContextShardStmt aShardStmtCtx)
    {
        mShardStmtCtx = aShardStmtCtx;
    }

    /**
     * ShardRetryAvailableException�̳� STF success ��Ȳ�� ��쿡�� statement�� ��ȿ���� �ʱ� ������ <br>
     * �ش� statement�� routedStatementMap���� clear�Ѵ�.
     * @param aShardJdbcEx ShardJdbcException ��ü
     */
    void clearRoutedStatementMap(ShardJdbcException aShardJdbcEx)
    {
        ShardNodeConfig sNodeConfig = mMetaConn.getShardNodeConfig();
        String sNodeName = aShardJdbcEx.getNodeName();
        mRoutedStatementMap.remove(sNodeConfig.getNodeByName(sNodeName));
    }

    /**
     * RoutedStatementMap�� ��ȯ�ϸ鼭 SqlWarning�� ������ �����Ѵ�.
     * @throws SQLException ��� Statement���� SqlWarning�� �������� ���� ������ �߻��� ���
     */
    void getNodeSqlWarnings(boolean aNeedToDisconnect) throws SQLException
    {
        boolean sWarningExists = false;
        for (Statement sEach : mRoutedStatementMap.values())
        {
            AltibaseStatement sStmt = (AltibaseStatement)sEach;
            if (!sStmt.isClosed() && sEach.getWarnings() != null)
            {
                sWarningExists = true;
                break;
            }
        }
        if (!sWarningExists) return;

        // ������� SQLWarning�� �����ϱ� ���� TreeMap�� ����Ѵ�.
        Map<DataNode, Statement> sSortedMap = new TreeMap<DataNode, Statement>(mRoutedStatementMap);
        for (Map.Entry<DataNode, Statement> sEntry : sSortedMap.entrySet())
        {
            Statement sStmt = sEntry.getValue();
            SQLWarning sSqlWarning = sStmt.getWarnings();
            if (sSqlWarning == null) continue;
            if (sSqlWarning.getErrorCode() == ErrorDef.SHARD_META_NUMBER_INVALID )
            {
                if (aNeedToDisconnect)
                {
                    mSqlwarning = Error.createWarning(mSqlwarning,
                                                      ErrorDef.SHARD_SMN_OPERATION_FAILED,
                                                      "[" + sEntry.getKey().getNodeName() + "] The " + "Execute",
                                                      sSqlWarning.getMessage());
                }
            }
            else
            {
                getSQLWarningFromDataNode(sEntry.getKey().getNodeName(), sSqlWarning);
            }
        }
    }

    private void getSQLWarningFromDataNode(String aNodeName, SQLWarning aWarning)
    {
        SQLWarning sNewWarning = new SQLWarning("[" + aNodeName + "] " + aWarning.getMessage(),
                                                aWarning.getSQLState(), aWarning.getErrorCode());
        if (mSqlwarning == null)
        {
            mSqlwarning = sNewWarning;
        }
        else
        {
            mSqlwarning.setNextWarning(sNewWarning);
        }

    }

    public boolean isPrepared()
    {
        for (Statement sEach : mRoutedStatementMap.values())
        {
            if (((AltibaseStatement)sEach).isPrepared())
            {
                return true;
            }
        }

        return false;
    }

    public boolean hasNoData()
    {
        for (Statement sEach : mRoutedStatementMap.values())
        {
            AltibaseStatement sStmt = (AltibaseStatement)sEach;
            if (sStmt.isPrepared() && !sStmt.cursorhasNoData())
            {
                return false;
            }
        }

        return true;
    }

    void removeFromStmtRouteMap(DataNode aNode)
    {
        // BUG-46513 connection�� �����ɶ� Statement Free�� �����ϹǷ� ���⼭�� ���������� �����Ѵ�.
        mRoutedStatementMap.remove(aNode);
    }

    int getNodeUpdateRowCount()
    {
        long sResult = 0;
        boolean sHasResult = false;
        for (Statement sEach : mRoutedStatementMap.values())
        {
            int sUpdatedCount = 0;
            try
            {
                sUpdatedCount = sEach.getUpdateCount();
            }
            catch (SQLException aEx)
            {
                /* BUG-46513 lazyConnect �� false�϶��� statement�� ���� �������ϼ� �ֱ⶧���� �ش� exception��
                   �߻��ϸ� �α׸� ����� �Ѿ��. */
                if (aEx.getErrorCode() == ErrorDef.STATEMENT_NOT_YET_EXECUTED)
                {
                    shard_log("Statement not yet executed : {0}", sEach);
                    continue;
                }
            }
            if (sUpdatedCount > -1)
            {
                sHasResult = true;
                sResult += sUpdatedCount;
            }
        }
        if (sResult > Integer.MAX_VALUE)
        {
            sResult = Integer.MAX_VALUE;
        }

        return sHasResult ? Long.valueOf(sResult).intValue() : -1;
    }

    public void makeQstrForGeneratedKeys(String aSql, int[] aColumnIndexes,
                                         String[] aColumnNames) throws SQLException
    {
        // BUG-47168 Ŭ���̾�Ʈ ���̵��϶��� AutoGenerated Key�� �������� �ʴ´�.
        Error.throwSQLException(ErrorDef.UNSUPPORTED_FEATURE,
                                "AutoGeneratedKey is not supported in client-side sharding.");
    }

    public void clearForGeneratedKeys() throws SQLException
    {
        Error.throwSQLException(ErrorDef.UNSUPPORTED_FEATURE,
                                "AutoGeneratedKey is not supported in client-side sharding.");
    }

    void processExecuteError(List<Statement> aStatementUnits, SQLException aEx) throws SQLException
    {
        // BUG-46785 : ������ �ϳ��� ������ partial rollback ó��

        // one node ����ÿ��� partial rollback ���ʿ�.
        if (aStatementUnits == null || aStatementUnits.size() < 2)
        {
            return;
        }
        
        List<SQLException> sExceptionList = new ArrayList<SQLException>();
        List<SQLWarning> sWarningList = new ArrayList<SQLWarning>();

        sExceptionList.add(aEx);
        // BUGBUG : sWarningList.add(e) �ʿ��Ѱ�?
        
        // select for update�� ��� partial rollback ���� close cursor ����
        try
        {
            // BUGBUG : select���θ� ���� üũ�Ͽ� select�϶��� closeCursor ȣ��?
            // AltibaseStatement sStmt0 = (AltibaseStatement)aStatementUnits.get(0);
            mMetaConn.getExecutorEngine().closeCursor(aStatementUnits, 
                    new ParallelProcessCallback<Statement>()
                    {
                        public Void processInParallel(Statement aStmt) throws SQLException 
                        {
                            ((AltibaseStatement)aStmt).closeCursor();
                            return null;
                        }
                    });

            mMetaConn.getExecutorEngine().doPartialRollback(aStatementUnits,
                    new ParallelProcessCallback<Statement>()
                    {
                          public Void processInParallel(Statement aStmt) throws SQLException
                          {
                              if (((AltibaseStatement) aStmt).getIsSuccess())  // ������ ��常 partial rollback ����.
                              {
                                  AltibaseConnection sConn = (AltibaseConnection) aStmt.getConnection();
                                  sConn.shardStmtPartialRollback();
                              }
                              return null;
                          }
                    });
        }
        catch (ShardJdbcException sShardJdbcEx1)
        {
            // BUGBUG : aShardJdbcEx.next ���غ��� �ǳ�?
            sExceptionList.add(sShardJdbcEx1);
            clearRoutedStatementMap(sShardJdbcEx1);
            getNodeSqlWarnings(true);
            ShardError.throwSQLExceptionIfExists(sExceptionList);
        }
        catch (SQLException sEx1)
        {
            sExceptionList.add(sEx1);
            
            try
            {
                mMetaConn.sendNodeTransactionBrokenReport();
            }
            catch (ShardJdbcException sShardJdbcEx2)
            {
                clearRoutedStatementMap(sShardJdbcEx2);
                getNodeSqlWarnings(true);
                throw sShardJdbcEx2;
            }
            catch (SQLException sEx2)
            {
                sExceptionList.add(Error.createSQLException(ErrorDef.SHARD_INTERNAL_ERROR,
                                   "ShardNodeReport(Transaction Broken) fails."));
            }
            
            ShardError.throwSQLExceptionIfExists(sExceptionList);
        }
    }

    private interface GetMoreResultsTemplate
    {
        boolean getMoreResults(Statement aStmt) throws SQLException;
    }
}
