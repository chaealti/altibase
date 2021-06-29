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

import java.sql.*;
import java.util.*;
import java.util.logging.Level;
import java.util.logging.Logger;

import Altibase.jdbc.driver.cm.*;
import Altibase.jdbc.driver.datatype.Column;
import Altibase.jdbc.driver.datatype.ColumnFactory;
import Altibase.jdbc.driver.datatype.ColumnInfo;
import Altibase.jdbc.driver.datatype.IntegerColumn;
import Altibase.jdbc.driver.ex.Error;
import Altibase.jdbc.driver.ex.ErrorDef;
import Altibase.jdbc.driver.ex.ShardError;
import Altibase.jdbc.driver.logging.LoggingProxy;
import Altibase.jdbc.driver.logging.TraceFlag;
import Altibase.jdbc.driver.sharding.core.*;
import Altibase.jdbc.driver.util.AltiSqlProcessor;
import Altibase.jdbc.driver.util.AltibaseProperties;
import Altibase.jdbc.driver.util.DynamicArray;
import Altibase.jdbc.driver.util.StringUtils;
import Altibase.jdbc.driver.sharding.core.GlobalTransactionLevel;

import static Altibase.jdbc.driver.sharding.core.AltibaseShardingFailover.*;

class ExecuteResult
{
    boolean mHasResultSet;
    boolean mReturned;
    long mUpdatedCount;
    
    ExecuteResult(boolean aHasResultSet, long aUpdatedCount)
    {
        mHasResultSet = aHasResultSet;
        mUpdatedCount = aUpdatedCount;
        mReturned = false;
    }
}

public class AltibaseStatement extends WrapperAdapter implements Statement
{
    static final int                   DEFAULT_CURSOR_HOLDABILITY = ResultSet.CLOSE_CURSORS_AT_COMMIT;
    public static final int            DEFAULT_UPDATE_COUNT = -1;
    // BUG-42424 ColumnInfo���� BYTES_PER_CHAR�� ����ϱ� ������ public���� ����
    public static final int            BYTES_PER_CHAR             = 2;
    private static final String        PING_SQL_PATTERN           = "/* PING */ SELECT 1";

    protected AltibaseConnection       mConnection;
    protected boolean                  mEscapeProcessing          = true;
    protected ExecuteResultManager     mExecuteResultMgr = new ExecuteResultManager();
    protected short                    mCurrentResultIndex;
    protected AltibaseResultSet        mCurrentResultSet          = null;
    protected List<ResultSet>          mResultSetList;
    protected boolean                  mIsClosed;
    protected SQLWarning               mWarning                   = null;
    /* BUG-37642 Improve performance to fetch */
    protected int                      mFetchSize                 = 0;
    protected int                      mMaxFieldSize;
    protected long                     mMaxRows;
    protected CmPrepareResult          mPrepareResult;
    protected List<Column>             mPrepareResultColumns;
    protected CmExecutionResult        mExecutionResult;
    protected CmFetchResult            mFetchResult;
    protected int                      mStmtCID;
    protected boolean                  mIsDeferred;        // BUG-42424 deferred prepare
    protected String                   mQstr;
    protected boolean                  mReUseResultSet;      // BUG-48380 ResultSet�� �������� ����
    protected boolean                  mResultSetReturned;   // BUG-48380 execute ����� ResultSet�� �����Ǿ����� ����
    private String                     mQstrForGeneratedKeys;
    private CmProtocolContextDirExec   mContext;
    private LinkedList                 mBatchSqlList;
    private final int                  mResultSetType;
    private final int                  mResultSetConcurrency;
    private final int                  mResultSetHoldability;
    protected int                      mTargetResultSetType;
    protected int                      mTargetResultSetConcurrency;
    private AltibaseStatement          mInternalStatement;
    private boolean                    mIsInternalStatement; // PROJ-2625
    private SemiAsyncPrefetch          mSemiAsyncPrefetch;   // PROJ-2625
    private AltibaseResultSet          mGeneratedKeyResultSet;
    private int                        mQueryTimeout;
    private final AltibaseResultSet    mEmptyResultSet;
    private transient Logger           mLogger;
    private AltibaseShardingConnection mMetaConn;
    private boolean                    mCloseOnCompletion;
    // BUG-48892 ���忡 ���� Statement pool�� ���� �����ϰ� ���� �ʴ��� flag ���� ������ �� �ֵ��� ����
    private boolean                    mIsPoolable;
    private boolean                    mIsSuccess = true;    // BUG-48762 sharding statementAC partial rollback ����

    protected class ExecuteResultManager
    {
        private LinkedList mExecuteResults;
        
        private ExecuteResultManager() {}
        
        protected ExecuteResult get(int aIdx) throws SQLException
        {
            throwErrorForStatementNotYetExecuted();
            return (ExecuteResult)mExecuteResults.get(aIdx);
        }
        
        protected void clear() throws SQLException
        {
            if(mExecuteResults != null)
            {
                mExecuteResults.clear();
            }
        }

        protected void add(ExecuteResult aResult)
        {
            if(mExecuteResults == null)
            {
                mExecuteResults = new LinkedList();
            }
            
            mExecuteResults.add(aResult);
        }

        protected int size()
        {
            if(mExecuteResults == null)
            {
                return 0;
            }
            else
            {
                return mExecuteResults.size();
            }
        }

        protected ExecuteResult getFirst() throws SQLException
        {
            throwErrorForStatementNotYetExecuted();
            return (ExecuteResult)mExecuteResults.getFirst();
        }

        public Iterator iterator() throws SQLException
        {
            throwErrorForStatementNotYetExecuted();
            return mExecuteResults.iterator();
        }

        private void throwErrorForStatementNotYetExecuted() throws SQLException
        {
            if (mExecuteResults == null)
            {
                Error.throwSQLException(ErrorDef.STATEMENT_NOT_YET_EXECUTED);
            }
        }
    }
    
    AltibaseStatement(AltibaseConnection aCon) throws SQLException
    {
        // internal statement�� ���� ������.
        this(aCon, ResultSet.TYPE_FORWARD_ONLY, ResultSet.CONCUR_READ_ONLY, DEFAULT_CURSOR_HOLDABILITY);
    }

    public AltibaseStatement(AltibaseConnection aCon, int aResultSetType, int aResultSetConcurrency,
                             int aResultSetHoldability) throws SQLException
    {
        if (TraceFlag.TRACE_COMPILE && TraceFlag.TRACE_ENABLED)
        {
            mLogger = Logger.getLogger(LoggingProxy.JDBC_LOGGER_DEFAULT);
        }

        AltibaseResultSet.checkAttributes(aResultSetType, aResultSetConcurrency, aResultSetHoldability);

        mConnection = aCon;
        mMaxFieldSize = 0;
        mMaxRows = 0;
        mFetchSize = downgradeFetchSize(aCon.getProperties().getFetchEnough());
        createFetchSizeWarning(aCon.getProperties().getFetchEnough());
        mIsDeferred = mConnection.isDeferredPrepare();    // BUG-42424 deferred prepare

        /* BUG-39463 Add new fetch protocol that can request over 65535 rows. */
        if (mFetchSize < 0)
        {
            Error.throwSQLException(ErrorDef.INVALID_ARGUMENT,
                                    "fetch_enough",
                                    AltibaseProperties.RANGE_FETCH_ENOUGH,
                                    String.valueOf(mFetchSize));
        }

        mCurrentResultIndex = 0;
        mResultSetList = new ArrayList<ResultSet>();
        mIsClosed = false;
        mStmtCID = mConnection.makeStatementCID();
        createProtocolContext();
        mTargetResultSetType = mResultSetType = aResultSetType;
        mTargetResultSetConcurrency = mResultSetConcurrency = aResultSetConcurrency;
        mResultSetHoldability = aResultSetHoldability;

        // generated keys�� ���� �� ResultSet
        IntegerColumn sColumn = ColumnFactory.createIntegerColumn();
        ColumnInfo sInfo = new ColumnInfo();
        sColumn.getDefaultColumnInfo(sInfo);
        sColumn.setColumnInfo(sInfo);
        List sColumnList = new ArrayList();
        sColumnList.add(sColumn);
        mEmptyResultSet = new AltibaseEmptyResultSet(this, sColumnList, ResultSet.TYPE_FORWARD_ONLY, ResultSet.CONCUR_READ_ONLY);
        mGeneratedKeyResultSet = mEmptyResultSet;
    }

    int getCID()
    {
        return mStmtCID;
    }

    public int getID()
    {
        return (mPrepareResult == null) ? 0 : mPrepareResult.getStatementId();
    }

    public String getSql()
    {
        return mQstr;
    }

    public void setSql(String aSql)
    {
        mQstr = aSql;
    }
    
    protected void createProtocolContext()
    {
        mContext = new CmProtocolContextDirExec(mConnection.channel());
        mContext.setDistTxInfo(mConnection.getDistTxInfo());
    }
    
    public CmProtocolContextDirExec getProtocolContext()
    {
        return mContext;
    }
    
    protected void afterExecution() throws SQLException
    {
        if (getProtocolContext().getError() != null)
        {
            CmErrorResult sErrorResult = getProtocolContext().getError();
            try
            {
                mWarning = Error.processServerError(mWarning, sErrorResult);
            }
            catch (SQLException aEx)
            {
                mIsSuccess = false;  // For partial rollback
                /* BUG-46790 direct execute�� ��� SQLException�� �߻��ϸ� prepare����� �ʱ�ȭ �Ѵ�.
                   �׷��� ������ Failure to find statement ������ �߻��� �� �ִ�. */
                if (!(this instanceof PreparedStatement))
                {
                    mPrepareResult = null;
                }
                throw aEx;
            }
            finally
            {
                // BUG-46790 Exception�� �߻��ϴ��� shard align�۾��� �����ؾ� �Ѵ�.
                ShardError.processShardError(mConnection.getMetaConnection(), sErrorResult);
            }
        }
        mPrepareResult = getProtocolContext().getPrepareResult();
        if(getProtocolContext().getPrepareResult().getResultSetCount() > 1) 
        {
            mPrepareResultColumns = getProtocolContext().getGetColumnInfoResult().getColumns();
        }
        
        if(mPrepareResultColumns == null)
        {
            mPrepareResultColumns = getProtocolContext().getGetColumnInfoResult().getColumns();
        }
        
        mExecutionResult = getProtocolContext().getExecutionResult();
        mFetchResult = getProtocolContext().getFetchResult();
        
        setProperty4Nodes();
    }
    
    private void setProperty4Nodes()  throws SQLException
    {
        // BUGBUG : alter session set global_transaction_level ~ && shardjdbc�� ���
        if (mMetaConn == null)
        {
            return;
        }
        
        // BUGBUG : CmGetPropertyResult�� ����ϴ°� �� ���� �� Ȯ���غ���...
        short sPropID = mExecutionResult.getSessionPropID();
        String sPropValue = mExecutionResult.getSessionPropValueStr();
        
        // Node Connections�� ���� sendProperties ����
        switch (sPropID)
        {
            case (AltibaseProperties.PROP_CODE_GLOBAL_TRANSACTION_LEVEL):
                mMetaConn.sendGlobalTransactionLevel(GlobalTransactionLevel.get(Short.parseShort(sPropValue)));
                break;
            case (AltibaseProperties.PROP_CODE_SHARD_STATEMENT_RETRY):
                mMetaConn.sendShardStatementRetry(Short.parseShort(sPropValue));
                break;
            case (AltibaseProperties.PROP_CODE_INDOUBT_FETCH_TIMEOUT):
                mMetaConn.sendIndoubtFetchTimeout(Integer.parseInt(sPropValue));
                break;
            case (AltibaseProperties.PROP_CODE_INDOUBT_FETCH_METHOD):
                mMetaConn.sendIndoubtFetchMethod(Short.parseShort(sPropValue));
                break;
            default:
                break;
        }
    }

    protected void clearAllResults() throws SQLException
    {
        // PROJ-2427 closeAllCursor ���������� ������ ������ �ʰ� execute, executeQuery���� �Ѳ����� ������.
        mExecuteResultMgr.clear();
        mResultSetList.clear();

        // BUG-48380 ResultSet�� ��Ȱ���Ҷ��� mCurrentResultSet�� null�� �ʱ�ȭ �ؼ��� �ȵȴ�.
        if (!mReUseResultSet)
        {
            mCurrentResultSet = null;
        }

        if (mGeneratedKeyResultSet != mEmptyResultSet)
        {
            mGeneratedKeyResultSet.close();
            mGeneratedKeyResultSet = mEmptyResultSet;
        }
    }

    /**
     * Generated Keys�� ������� �������� �����.
     * <p>
     * column indexes�� column names�� ���ÿ� ������� �ʴ´�.
     * ����, ���� ��� �Ѱ�ٸ� column indexes�� ����ϰ� column names ���� �����Ѵ�.
     * <p>
     * ���� ���� ������ Generated Keys�� ��⿡ �������� �ʴٸ�, ������ �Ѿ��.
     * 
     * @param aSql ���� ������. �ݵ�� INSERT �����̾�� �Ѵ�.
     * @param aColumnIndexes an array of the indexes of the columns in the inserted row that should be made available for retrieval by a call to the method {@link #getGeneratedKeys()}
     * @param aColumnNames an array of the names of the columns in the inserted row that should be made available for retrieval by a call to the method {@link #getGeneratedKeys()}
     */
    public void makeQstrForGeneratedKeys(String aSql, int[] aColumnIndexes, String[] aColumnNames) throws SQLException
    {
        mQstrForGeneratedKeys = null;

        if (!AltiSqlProcessor.isInsertQuery(aSql))
        {
            return; // INSERT �������� �Ѵ�.
        }

        ArrayList sSeqs = AltiSqlProcessor.getAllSequences(aSql);
        if (sSeqs.size() == 0)
        {
            // BUG-39571 �������� ���� ��� SQLException�� �߻���Ű�� �ʰ� �׳� ���Ͻ�Ų��.
            return;
        }

        if (aColumnIndexes != null)
        {
            mQstrForGeneratedKeys = AltiSqlProcessor.makeGenerateKeysSql(sSeqs, aColumnIndexes);
        }
        else if (aColumnNames != null)
        {
            mQstrForGeneratedKeys = AltiSqlProcessor.makeGenerateKeysSql(sSeqs, aColumnNames);
        }
        else
        {
            mQstrForGeneratedKeys = AltiSqlProcessor.makeGenerateKeysSql(sSeqs);
        }
    }

    public void clearForGeneratedKeys()
    {
        mQstrForGeneratedKeys = null;
    }

    /**
     * Generated Keys�� ��� ���� �غ��� �������� ������ �����Ѵ�.
     * <p>
     * ����� {@link #getGeneratedKeys()}�� ���� ���� �� �ִ�.
     * 
     * @throws SQLException ���� ���࿡ ������ ���
     */
    void executeForGeneratedKeys() throws SQLException
    {
        if (mQstrForGeneratedKeys == null)
        {
            mGeneratedKeyResultSet = mEmptyResultSet;
            return;
        }

        if (mInternalStatement == null)
        {
            mInternalStatement = AltibaseStatement.createInternalStatement(mConnection);
        }
        mGeneratedKeyResultSet = (AltibaseResultSet)mInternalStatement.executeQuery(mQstrForGeneratedKeys);
    }

    protected boolean processExecutionResult() throws SQLException
    {
        for (int i=0; i<mPrepareResult.getResultSetCount(); i++)
        {
            mExecuteResultMgr.add(new ExecuteResult(true, DEFAULT_UPDATE_COUNT));
        }
        
        if (mExecutionResult.getUpdatedRowCount() > 0)
        {
            mExecuteResultMgr.add(new ExecuteResult(false, mExecutionResult.getUpdatedRowCount()));
        }
        else
        {
            if (mExecuteResultMgr.size() == 0)
            {
                // ����� �ϳ��� ���� ��� update count = 0�� �����Ѵ�.
                mExecuteResultMgr.add(new ExecuteResult(false,  0));
            }
        }
        
        mCurrentResultIndex = 0;
        
        return mExecuteResultMgr.getFirst().mHasResultSet;
    }

    protected ResultSet processExecutionQueryResult(String aSql) throws SQLException
    {
        throwErrorForResultSetCount(aSql);
        ExecuteResult sExecResult = new ExecuteResult(true, DEFAULT_UPDATE_COUNT);
        mExecuteResultMgr.add(sExecResult);

        mCurrentResultSet = AltibaseResultSet.createResultSet(this, mTargetResultSetType, mTargetResultSetConcurrency);
        // BUG-47639 lob_null_select jdbc �Ӽ����� AltibaseConnection ��ü�κ��� �޾ƿ´�.
        mCurrentResultSet.setAllowLobNullSelect(mConnection.getAllowLobNullSelect());
        mResultSetList.add(mCurrentResultSet);
        sExecResult.mReturned = true;

        return mCurrentResultSet;
    }
    
    public void addBatch(String aSql) throws SQLException
    {
        throwErrorForClosed();
        throwErrorForNullSqlString(aSql);

        if(mBatchSqlList == null)
        {
            mBatchSqlList = new LinkedList();
        }
        else
        {
            if (mBatchSqlList.size() == Integer.MAX_VALUE) 
            {
                Error.throwSQLException(ErrorDef.TOO_MANY_BATCH_JOBS);
            }
        }

        if (mEscapeProcessing)
        {
            aSql = AltiSqlProcessor.processEscape(aSql);
        }
        mBatchSqlList.add(aSql);
    }

    public void cancel() throws SQLException
    {
        throwErrorForClosed();

        AltibaseConnection sPrivateConnection = mConnection.cloneConnection();
        CmProtocolContextDirExec sContext = new CmProtocolContextDirExec(sPrivateConnection.channel());
        try
        {
            CmProtocol.cancelStatement(sContext, mStmtCID);
        }
        catch (SQLException ex)
        {
            AltibaseFailover.trySTF(mConnection.failoverContext(), ex);
        }
        try
        {
            if (sContext.getError() != null)
            {
                Error.processServerError(null, sContext.getError());
            }
        }
        finally
        {
            sPrivateConnection.close();
        }
    }

    public void closeCursor() throws SQLException
    {
        throwErrorForClosed();

        if (!shouldCloseCursor())
        {
            return;
        }

        CmProtocolContextDirExec sContext = getProtocolContext();
        try
        {
            CmProtocol.closeCursorInternal(sContext);
        }
        catch (SQLException ex)
        {
            // BUGBUG : ���� tryShardFailOver ����ϰ�, ���� trySTF ����ϳ�?
            tryShardFailOver(mConnection, ex);   
            //AltibaseFailover.trySTF(mConnection.failoverContext(), ex);
        }
        if (getProtocolContext().getError() != null)
        {
            try
            {
                mWarning = Error.processServerError(mWarning, getProtocolContext().getError());
            }
            finally
            {
                // BUG-46790 Exception�� �߻��ϴ��� shard align�۾��� �����ؾ� �Ѵ�.
                ShardError.processShardError(getMetaConn(), getProtocolContext().getError());
            }
        }
    }

    boolean shouldCloseCursor()
    {
        return mPrepareResult != null && mPrepareResult.isSelectStatement();
    }

    public void clearBatch() throws SQLException
    {
        throwErrorForClosed();
        if(mBatchSqlList != null)
        {
            mBatchSqlList.clear();
        }
    }

    public void clearWarnings() throws SQLException
    {
        throwErrorForClosed();

        mWarning = null;
    }

    public void close() throws SQLException
    {
        if (isClosed() == true)
        {
            return;
        }

        // PROJ-2625 Semi-async Prefetch, Prefetch Auto-tuning
        if (isAsyncPrefetch())
        {
            ((AltibaseForwardOnlyResultSet)mCurrentResultSet).endFetchAsync();
        }

        mSemiAsyncPrefetch = null;

        if (mPrepareResult != null)
        {
            CmProtocol.freeStatement(getProtocolContext(), mPrepareResult.getStatementId());
            CmProtocol.clientCommit(getProtocolContext(), mConnection.isClientSideAutoCommit());
        }
        mConnection.removeStatement(this);

        if (mInternalStatement != null)
        {
            mInternalStatement.close();
        }

        mIsClosed = true;
    }

    /**
     * {@link Connection#close()}�� ���� ���� ����(opened ==> closed)�� �޼ҵ�
     */
    void closeForRelease()
    {
        if (mInternalStatement != null)
        {
            mInternalStatement.closeForRelease();
        }
        mIsClosed = true;
    }

    synchronized void close4STF() throws SQLException
    {
        if (mIsClosed)
        {
            return;
        }

        // PROJ-2625 Semi-async Prefetch, Prefetch Auto-tuning
        if (isAsyncPrefetch())
        {
            mConnection.clearAsyncPrefetchStatement();
        }

        mSemiAsyncPrefetch = null;

        // for clearAllResults
        mExecuteResultMgr.clear();
        mResultSetList.clear();
        mCurrentResultSet = null;
        mGeneratedKeyResultSet = null;

        clearBatch();
        clearWarnings();
        mQstr = null;
        mStmtCID = 0;
        mContext = null;

        if (mInternalStatement != null)
        {
            mInternalStatement.close4STF();
        }

        mIsClosed = true;
    }

    public synchronized boolean execute(String aSql, int aAutoGeneratedKeys)
            throws SQLException
    {
        checkAutoGeneratedKeys(aAutoGeneratedKeys);

        if (aAutoGeneratedKeys == RETURN_GENERATED_KEYS)
        {
            makeQstrForGeneratedKeys(aSql, null, null);
        }
        else
        {
            clearForGeneratedKeys();
        }
        boolean sResult = execute(aSql);
        executeForGeneratedKeys();
        return sResult;
    }

    public boolean execute(String aSql, int[] aColumnIndexes) throws SQLException
    {
        makeQstrForGeneratedKeys(aSql, aColumnIndexes, null);
        boolean sResult = execute(aSql);
        executeForGeneratedKeys();
        return sResult;
    }

    public boolean execute(String aSql, String[] aColumnNames)
            throws SQLException
    {
        makeQstrForGeneratedKeys(aSql, null, aColumnNames);
        boolean sResult = execute(aSql);
        executeForGeneratedKeys();
        return sResult;
    }

    /*
     * �� �޼ҵ��� ���ϰ��� ���� ����� ���� �ٸ� �κ��� �ִ�.
     * PSM���� query�� �����ϸ� �������� result(ResultSet�� �����ְ�,
     * update count�� ���� �ִ�)�� �߻��� �� �ִµ�,
     * JDBC Spec���� ù��° result�� ResultSet�� �ƴ� ���
     * �� �޼ҵ��� ���ϰ��� false��� �����Ѵ�. 
     * �� Spec�� ������, �� �޼ҵ尡 false�� �����Ѵٰ� �ؼ�
     * result�� ResultSet�� ���ٰ� ������ �� ����.
     * ������, ���� ���������� ResultSet�� ������ �ݵ��
     * ù��° result���� ResultSet�� �������� �����ߴ�.
     * ���� �� �޼ҵ尡 false�� �����Ѵٴ� ����
     * ResultSet�� result�� ���ԵǾ� ���� �ʴٴ� ���� �����Ѵ�. 
     */
    public boolean execute(String aSql) throws SQLException
    {
        throwErrorForClosed();
        throwErrorForNullSqlString(aSql);
        throwErrorForBatchJob("execute");

        clearAllResults();
        
        if (mEscapeProcessing)
        {
            aSql = AltiSqlProcessor.processEscape(aSql);
        }        
        setSql(aSql);

        // BUG-39149 ping sql�� ��쿡�� ���������� ping�޼ҵ带 ȣ���ϰ� light-weight ResultSet�� �����Ѵ�.
        if (isPingSQL(aSql))
        {
            pingAndCreateLightweightResultSet(); 
            return true;
        }
        
        try
        {
            aSql = procDowngradeAndGetTargetSql(aSql);
            // PROJ-2427 cursor�� �ݾƾ��ϴ� ������ �Ű������� �Ѱ��ش�.
            CmProtocol.directExecute(getProtocolContext(),
                                     mStmtCID,
                                     aSql,
                                     (mResultSetHoldability == ResultSet.HOLD_CURSORS_OVER_COMMIT),
                                     usingKeySetDriven(),
                                     mConnection.nliteralReplaceOn(), mConnection.isClientSideAutoCommit(),
                                     mPrepareResult != null && mPrepareResult.isSelectStatement());
        }
        catch (SQLException ex)
        {
            mIsSuccess = false;  // For partial rollback
            tryShardFailOver(mConnection, ex);
        }
        afterExecution();

        return processExecutionResult();
    }

    protected static final long[] EMPTY_LARGE_BATCH_RESULT = new long[0];

    public int[] executeBatch() throws SQLException
    {
        return toIntBatchUpdateCounts(executeLargeBatch());
    }

    @Override
    public long[] executeLargeBatch() throws SQLException
    {
        throwErrorForClosed();
        clearAllResults();

        if (mBatchSqlList == null || mBatchSqlList.isEmpty())
        {
            mExecuteResultMgr.add(new ExecuteResult(false, DEFAULT_UPDATE_COUNT));
            return EMPTY_LARGE_BATCH_RESULT;
        }

        try
        {
            CmProtocol.directExecuteBatch(getProtocolContext(),
                                          mStmtCID,
                                          (String[])mBatchSqlList.toArray(new String[0]),
                                          mConnection.nliteralReplaceOn(), mConnection.isClientSideAutoCommit());
        }
        catch (SQLException ex)
        {
            AltibaseFailover.trySTF(mConnection.failoverContext(), ex);
        }
        finally
        {
            // BUGBUG (2012-11-28) ���忡���� executeBatch ������ clear�Ǵ°� ��Ȯ�ϰ� ������ ���� ��. oracle�� ������.
            clearBatch();
        }
        try
        {
            afterExecution();
        }
        catch (SQLException sEx)
        {
            CmExecutionResult sExecResult = getProtocolContext().getExecutionResult();
            long[] sLongUpdatedRowCounts = sExecResult.getUpdatedRowCounts();
            Error.throwBatchUpdateException(sEx, toIntBatchUpdateCounts(sLongUpdatedRowCounts));
        }
        long[] sUpdatedRowCounts = mExecutionResult.getUpdatedRowCounts();
        long sUpdateCount = 0;
        for (long sUpdatedRowCount : sUpdatedRowCounts)
        {
            sUpdateCount += sUpdatedRowCount;
        }
        mExecuteResultMgr.add(new ExecuteResult(false, sUpdateCount));
        return sUpdatedRowCounts;
    }

    public ResultSet executeQuery(String aSql) throws SQLException
    {
        throwErrorForClosed();
        throwErrorForNullSqlString(aSql);
        throwErrorForBatchJob("executeQuery");
        
        clearAllResults();
        
        if (mEscapeProcessing)
        {
            aSql = AltiSqlProcessor.processEscape(aSql);
        }
        setSql(aSql);

        // BUG-39149 ping sql�� ��쿡�� ���������� ping�޼ҵ带 ȣ���ϰ� light-weight ResultSet�� �����Ѵ�.
        if (isPingSQL(aSql))
        {
            pingAndCreateLightweightResultSet(); 
            return mCurrentResultSet;
        }
        
        try
        {
            aSql = procDowngradeAndGetTargetSql(aSql);
            // PROJ-2427 cursor�� �ݾƾ� �ϴ� ������ ���� �Ѱ��ش�.
            CmProtocol.directExecuteAndFetch(getProtocolContext(),
                                             mStmtCID, 
                                             aSql,
                                             mFetchSize,
                                             mMaxRows,
                                             mMaxFieldSize,
                                             mResultSetHoldability == ResultSet.HOLD_CURSORS_OVER_COMMIT,
                                             usingKeySetDriven(),
                                             mConnection.nliteralReplaceOn(),
                                             mPrepareResult != null && mPrepareResult.isSelectStatement());
        }
        catch (SQLException ex)
        {
            tryShardFailOver(mConnection, ex);
        }
        afterExecution();
        
        return processExecutionQueryResult(aSql);
    }

    /**
     * BUG-39149 ���������� ping�޼ҵ带 ȣ���� ���� �������� ��� SELECT 1�� ����� �ش��ϴ� row�� �������� ������ �� ��ȯ�Ѵ�.
     * @throws SQLException
     */
    private void pingAndCreateLightweightResultSet() throws SQLException
    {
        mConnection.ping();
        ExecuteResult sExecResult = new ExecuteResult(true, DEFAULT_UPDATE_COUNT);
        mExecuteResultMgr.add(sExecResult);
        List<Column> aColumns = new ArrayList<>();
        aColumns.add(ColumnFactory.createSmallintColumn());
        aColumns.get(0).setValue(1);
        ColumnInfo sColumnInfo = new ColumnInfo();
        // BUG-39149 select 1 ������ column meta ������ �������� ������ �ش�.
        sColumnInfo.setColumnInfo(Types.SMALLINT,                               // dataType
                                  0,                                            // language
                                  (byte)0,                                      // arguments
                                  0,                                            // precision
                                  0,                                            // scale
                                  (byte)ColumnInfo.IN_OUT_TARGET_TYPE_TARGET,   // in-out type
                                  true,                                         // nullable
                                  false,                                        // updatable
                                  null,                                         // catalog name
                                  null,                                         // table name
                                  null,                                         // base table name
                                  null,                                         // col name
                                  "1" ,                                         // display name
                                  null,                                         // base column name
                                  null,                                         // schema name
                                  BYTES_PER_CHAR);                              // bytes per char
        aColumns.get(0).setColumnInfo(sColumnInfo);
        mCurrentResultSet = new AltibaseLightWeightResultSet(this, aColumns, this.mResultSetType);
        sExecResult.mReturned = true;
    }

    /**
     * BUG-39149 sql�� validation check�� ping �������� üũ�Ѵ�.
     * @param aSql
     * @return
     */
    private boolean isPingSQL(String aSql)
    {
        if (StringUtils.isEmpty(aSql)) return false;
        if (aSql.charAt(0) == '/')
        {
            if (aSql.toUpperCase().equals(PING_SQL_PATTERN))
            {
                return true;
            }
        }

        return false;
    }

    // BUGBUG (2012-11-06) �����ϴ� ���� ���忡�� �����ϴ°Ͱ� ���� �ٸ���.
    // ���忡���� AUTO_INCREMENT�� ROWIDó�� �ڵ����� �����Ǵ� ���ϰ��� ���� �� �ִ� �޼ҵ�� �Ұ��Ѵ�.
    // �׷���, Altibase�� AUTO_INCREMENT�� ROWID�� �������� �ʰ�,
    // �� INSERT�� ROW�� ������ �ĺ��ڸ� ���� ����� �����Ƿ�,
    // INSERT�� ����� SEQUENCE�� ��(CURRVAL)�� ��ȯ�Ѵ�.
    public int executeUpdate(String aSql, int aAutoGeneratedKeys)
            throws SQLException
    {
        boolean sHasResult = execute(aSql, aAutoGeneratedKeys);        
        // BUGBUG (2013-01-31) spec�� ������ ���ܸ� ������ �Ѵ�. �׷���, oracle�� �ȱ׷���. ������ oracle..
//        Error.checkAndThrowSQLException(sHasResult, ErrorDef.SQL_RETURNS_RESULTSET, aSql);
        return toInt(mExecuteResultMgr.getFirst().mUpdatedCount);
    }

    // BUGBUG (2012-11-06) ����� �ٸ���.
    // ���� ���� ������ column index�� DB TABLE������ �÷� ������ �ǹ��Ѵ�. (3rd, p955)
    // �׷��� �����ζ�� INSERT �������� ���� �÷��̶� ������ �� �ִ�.
    // ������, Altibase JDBC�� ����� SEQUENCE ���� ��ȯ�ϹǷ� ���̺� �÷��� ������ ����� �� ����.
    // ���, SEQUENCE�� ������ ������ ����Ѵ�. �� ��, SEQUENCE�� �ƴ� ���� ������ �ʴ´�.
    // �������, "INSERT INTO t1 VALUES (SEQ1.NEXTVAL, '1', SEQ2.NEXTVAL)"�� ���� INSERT���� �� ��,
    // SEQ1.CURRVAL�� �������� 1, SEQ2.CURRVAL�� �������� 2�� ����ؾ� �Ѵ�.
    public int executeUpdate(String aSql, int[] aColumnIndexes)
            throws SQLException
    {
        boolean sHasResult = execute(aSql, aColumnIndexes);
        // BUGBUG (2013-01-31) spec�� ������ ���ܸ� ������ �Ѵ�. �׷���, oracle�� �ȱ׷���. ������ oracle..
//        Error.checkAndThrowSQLException(sHasResult, ErrorDef.SQL_RETURNS_RESULTSET, aSql);
        return toInt(mExecuteResultMgr.getFirst().mUpdatedCount);
    }

    // BUGBUG (2012-11-06) ����� �ٸ���.
    // ���� ���� ������ column name�� DB TABLE������ �÷� �̸��� �ǹ��Ѵ�. (3rd, p955)
    // �׷��� �����ζ�� INSERT �������� ���� �÷��̶� ������ �� �ִ�.
    // ������, Altibase JDBC�� ����� SEQUENCE ���� ��ȯ�ϹǷ� ���̺� �÷��� �̸��� ����� �� ����.
    // ���, INSERT ���� ����� �÷� �̸��� ����Ѵ�.
    // �׷��Ƿ�, �� �޼ҵ带 ����ϱ� ���ؼ��� �ݵ��
    // "INSERT INTO t1 (c1, c2) VALUES (SEQ1.NEXTVAL, '1')"ó��
    // �÷� �̸��� ����� INSERT ���� ����ؾ��Ѵ�.
    public int executeUpdate(String aSql, String[] aColumnNames)
            throws SQLException
    {
        boolean sHasResult = execute(aSql, aColumnNames);        
        // BUGBUG (2013-01-31) spec�� ������ ���ܸ� ������ �Ѵ�. �׷���, oracle�� �ȱ׷���. ������ oracle..
//        Error.checkAndThrowSQLException(sHasResult, ErrorDef.SQL_RETURNS_RESULTSET, aSql);
        return toInt(mExecuteResultMgr.getFirst().mUpdatedCount);
    }

    public int executeUpdate(String aSql) throws SQLException
    {
        return toInt(executeLargeUpdate(aSql));
    }

    public AltibaseConnection getAltibaseConnection()
    {
        return mConnection;
    }

    public Connection getConnection() throws SQLException
    {
        throwErrorForClosed();

        return mConnection;
    }

    public int getFetchDirection() throws SQLException
    {
        throwErrorForClosed();

        return ResultSet.FETCH_FORWARD;
    }

    public int getFetchSize() throws SQLException
    {
        throwErrorForClosed();

        return mFetchSize;
    }

    public ResultSet getGeneratedKeys() throws SQLException
    {
        throwErrorForClosed();

        return mGeneratedKeyResultSet;
    }

    public int getMaxFieldSize() throws SQLException
    {
        throwErrorForClosed();

        return mMaxFieldSize;
    }

    public int getMaxRows() throws SQLException
    {
        throwErrorForClosed();

        return (int)mMaxRows;
    }

    public boolean getMoreResults() throws SQLException
    {
        throwErrorForClosed();

        if (mCurrentResultIndex >= mExecuteResultMgr.size() - 1)
        {
            /* BUG-47360 getMoreResults()�� ����� �������� mCurrentResultIndex�� �ʱ�ȭ ����� �Ѵ�. �׷��� ������ Statement��
               �ٽ� execute�Ҷ� ������ �߻��Ѵ�. */
            mCurrentResultIndex = 0;
            return false;
        }
        
        incCurrentResultSetIndex();
        
        return true;
    }
    
    protected void incCurrentResultSetIndex()
    {
        mCurrentResultIndex++;
    }

    public boolean getMoreResults(int current) throws SQLException
    {
        throwErrorForClosed();
        
        if (mCurrentResultIndex >= mExecuteResultMgr.size() - 1)
        {
            mCurrentResultIndex = 0;
            return false;
        }
        
        switch(current)
        {
            case Statement.KEEP_CURRENT_RESULT :
                break;
            case Statement.CLOSE_CURRENT_RESULT :
                mCurrentResultSet.close();
                break;
            case Statement.CLOSE_ALL_RESULTS :
                for(int i = 0; i <= mCurrentResultIndex; i++)
                {
                    ResultSet rs = mResultSetList.get(i);
                    rs.close();
                }
                break;
            default :
                Error.throwSQLException(ErrorDef.INVALID_ARGUMENT,
                                        "Current",
                                        "KEEP_CURRENT_RESULT | CLOSE_CURRENT_RESULT | CLOSE_ALL_RESULTS",
                                        String.valueOf(current));
                return false;
        }
        
        incCurrentResultSetIndex();
        
        return true;
    }

    public int getQueryTimeout() throws SQLException
    {
        throwErrorForClosed();

        // BUGBUG (!) �ϴ� ����. ������ timeout �Ӽ��� session �����̱� ����.
        return mQueryTimeout;
    }

    public ResultSet getResultSet() throws SQLException
    {
        throwErrorForClosed();

        getProtocolContext().setResultSetId((short)mCurrentResultIndex);

        // BUG-48380 �̹� ResultSet�� �����Ѵٸ� ������ �ִ� ResultSet�� �����Ѵ�.
        if (mResultSetReturned)
        {
            return mCurrentResultSet;
        }
        ExecuteResult sResult = (ExecuteResult)mExecuteResultMgr.get(mCurrentResultIndex);
        if (sResult.mReturned)
        {
            return mCurrentResultSet;
        }
        if (!sResult.mHasResultSet)
        {
            return null;
        }
        
        try
        {
            CmProtocol.fetch(getProtocolContext(), mFetchSize, mMaxRows, mMaxFieldSize);
        }
        catch (SQLException ex)
        {
            AltibaseFailover.trySTF(mConnection.failoverContext(), ex);
        }
        if (getProtocolContext().getError() != null)
        {
            try
            {
                mWarning = Error.processServerError(mWarning, getProtocolContext().getError());
            }
            finally
            {
                // BUG-46790 Exception�� �߻��ϴ��� shard align�۾��� �����ؾ� �Ѵ�.
                ShardError.processShardError(getMetaConn(), getProtocolContext().getError());
            }
        }
        mFetchResult = getProtocolContext().getFetchResult();
        
        mCurrentResultSet = AltibaseResultSet.createResultSet(this, mTargetResultSetType, mTargetResultSetConcurrency);
        mCurrentResultSet.setAllowLobNullSelect(mConnection.getAllowLobNullSelect());
        mResultSetList.add(mCurrentResultSet);

        sResult.mReturned = true;
        return mCurrentResultSet;
    }

    public int getResultSetConcurrency() throws SQLException
    {
        throwErrorForClosed();

        return mResultSetConcurrency;
    }

    public int getResultSetHoldability() throws SQLException
    {
        throwErrorForClosed();

        return mResultSetHoldability;
    }

    public int getResultSetType() throws SQLException
    {
        throwErrorForClosed();

        return mResultSetType;
    }

    public int getUpdateCount() throws SQLException
    {
        return toInt(getLargeUpdateCount());
    }

    public SQLWarning getWarnings() throws SQLException
    {
        throwErrorForClosed();

        return mWarning;
    }
    
    /**
     * Statement�� �������� Ȯ���Ѵ�. 
     * 
     * @return close�� �� ����Ǿ��ų� �̹� Connection�� close�Ǿ����� true, �ƴϸ� false
     */
    public boolean isClosed() throws SQLException
    {
        return mIsClosed || mConnection.isClosed() == true;
    }

    public void setCursorName(String aName) throws SQLException
    {
        Error.throwSQLException(ErrorDef.UNSUPPORTED_FEATURE, "cursor name and positioned update");
    }

    public void setEscapeProcessing(boolean aEnable) throws SQLException
    {
        throwErrorForClosed();

        mEscapeProcessing = aEnable;
    }

    public void setFetchDirection(int aDirection) throws SQLException
    {
        throwErrorForClosed();

        AltibaseResultSet.checkFetchDirection(aDirection);
    }

    public void setFetchSize(int aRows) throws SQLException
    {
        throwErrorForClosed();
        if (aRows < 0)
        {
            Error.throwSQLException(ErrorDef.INVALID_ARGUMENT,
                                    "Fetch size",
                                    AltibaseProperties.RANGE_FETCH_ENOUGH,
                                    String.valueOf(aRows));
        }
        mFetchSize = downgradeFetchSize(aRows);
        createFetchSizeWarning(aRows);
    }

    private void createFetchSizeWarning(int aRows)
    {
        if (mFetchSize < aRows)
        {
            mWarning = Error.createWarning(mWarning, ErrorDef.TOO_LARGE_FETCH_SIZE,
                                           String.valueOf(mFetchSize),
                                           String.valueOf(aRows));
        }
    }

    /**
     * Dynamic Array���� ����� �� �ִ� �ִ� ������ fetchSize�� �����Ѵ�.
     * @param aRows fetchSize
     */
    protected int downgradeFetchSize(int aRows)
    {
        int sMaxFetchSize = getMaxFetchSize();

        /* BUG-43263 aRows�� DynamicArray�� ������ �� �ִ� ������ ����� ����� �� �ִ� �ִ밪���� ���� �����Ѵ�. */
        if (aRows > sMaxFetchSize)
        {
            /* BUG-43263 ù��° chunk�� 0��° �ε����� beforeFirst�� ���� ����ΰ� loadcursor���� index�� ���� ������Ű�� ������ ������
               �� �� �ִ� �ִ� rows���� DynamicArray�ִ�ġ - 2 �̴�. */
            if (TraceFlag.TRACE_COMPILE && TraceFlag.TRACE_ENABLED)
            {
                mLogger.log(Level.INFO, "Fetch size downgraded from {0} to {1} ",
                            new Object[] { aRows, sMaxFetchSize });
            }
            aRows = sMaxFetchSize;
        }

        return aRows;
    }

    /**
     * �ִ� fetch size �� ��ȯ�Ѵ�.
     */
    protected static int getMaxFetchSize()
    {
        return DynamicArray.getDynamicArrySize() - 2;
    }

    public void setMaxFieldSize(int aMax) throws SQLException
    {
        throwErrorForClosed();
        if (aMax < 0)
        {
            Error.throwSQLException(ErrorDef.INVALID_ARGUMENT,
                                    "Max field size",
                                    "0 ~ Integer.MAX_VALUE",
                                    String.valueOf(aMax));
        }

        mMaxFieldSize = aMax;
    }

    public void setMaxRows(int aMax) throws SQLException
    {
        throwErrorForClosed();
        if (aMax < 0)
        {
            Error.throwSQLException(ErrorDef.INVALID_ARGUMENT,
                                    "Max rows",
                                    "0 ~ Integer.MAX_VALUE",
                                    String.valueOf(aMax));
        }

        mMaxRows = aMax;
    }

    public void setQueryTimeout(int aSeconds) throws SQLException
    {
        throwErrorForClosed();
        if (aSeconds < 0)
        {
            Error.throwSQLException(ErrorDef.INVALID_ARGUMENT,
                                    "Query timeout",
                                    "0 ~ Integer.MAX_VALUE",
                                    String.valueOf(aSeconds));
        }

        // BUGBUG (!) �ϴ� ����. ������ timeout �Ӽ��� session �����̱� ����.
        mQueryTimeout = aSeconds;
    }

    /**
     * Plan text�� ��´�.
     *
     * @return Plan text
     * @throws SQLException Plan text ��û�̳� ����� ��µ� �������� ��
     */
    public String getExplainPlan() throws SQLException
    {
        throwErrorForClosed();
        throwErrorForExplainTurnedOff();
        throwErrorForStatementNotPrepared();

        try
        {
            CmProtocol.getPlan(getProtocolContext(), mPrepareResult.getStatementId(),
                               getProtocolContext().getDeferredRequests());
        }
        catch (SQLException ex)
        {
            AltibaseFailover.trySTF(mConnection.failoverContext(), ex);
        }
        if (getProtocolContext().getError() != null)
        {
            try
            {
                mWarning = Error.processServerError(mWarning, getProtocolContext().getError());
            }
            finally
            {
                // BUG-46790 Exception�� �߻��ϴ��� shard align�۾��� �����ؾ� �Ѵ�.
                ShardError.processShardError(getMetaConn(), getProtocolContext().getError());
            }
        }
        CmGetPlanResult sResult = getProtocolContext().getGetPlanResult();
        if (sResult.getStatementId() != mPrepareResult.getStatementId())
        {
            Error.throwSQLException(ErrorDef.STMT_ID_MISMATCH,
                                    String.valueOf(mPrepareResult.getStatementId()),
                                    String.valueOf(sResult.getStatementId()));
        }
        return sResult.getPlanText();
    }

    // #region Result Set Downgrade

    /**
     * ResultSet ������ KeySet driven�� ���� �Ǿ��ִ��� Ȯ���Ѵ�.
     *
     * @return KeySet driven�� ������ ����
     * @throws SQLException ResultSet ������ Ȯ���ϴµ� ������ ���
     */
    final boolean usingKeySetDriven() throws SQLException
    {
        return (mTargetResultSetType == ResultSet.TYPE_SCROLL_SENSITIVE) ||
               (mTargetResultSetConcurrency == ResultSet.CONCUR_UPDATABLE);
    }

    /**
     * Ŀ�� �Ӽ��� Downgrade �Ѵ�.
     * <p>
     * Downgrade Rule�� ������ ����:
     * <ul>
     * <li>ResultSetType : TYPE_SCROLL_SENSITIVE --> TYPE_SCROLL_INSENSITIVE --> TYPE_FORWARD_ONLY</li>
     * <li>ResultSetConcurrency : CONCUR_UPDATABLE --> CONCUR_READ_ONLY</li>
     * <ul>
     * <p>
     * �� �� TYPE_SCROLL_INSENSITIVE --> TYPE_FORWARD_ONLY�� �����δ� �Ͼ�� �ʴ´�.
     * �ֳ��ϸ� TYPE_SCROLL_INSENSITIVE�� TYPE_FORWARD_ONLY�� ������� �� ���̶�,
     * ������ ��ü�� �߸��Ȱ� �ƴ϶�� ������ ���� ���� �����̴�.
     * <p>
     * Downgrade�� ResultSetType�� ResultSetConcurrency�� ���ÿ� �Ͼ��.
     */
    private final void downgradeTargetResultSetAttrs()
    {
        if (mTargetResultSetType == ResultSet.TYPE_SCROLL_SENSITIVE)
        {
            mTargetResultSetType = ResultSet.TYPE_SCROLL_INSENSITIVE;
            mWarning = Error.createWarning(mWarning, ErrorDef.OPTION_VALUE_CHANGED,
                                           "ResultSet type downgraded to TYPE_SCROLL_INSENSITIVE");
        }

        if (mTargetResultSetConcurrency == ResultSet.CONCUR_UPDATABLE)
        {
            mTargetResultSetConcurrency = ResultSet.CONCUR_READ_ONLY;
            mWarning = Error.createWarning(mWarning, ErrorDef.OPTION_VALUE_CHANGED,
                                           "ResultSet concurrency changed to CONCUR_READ_ONLY");
        }
    }

    /**
     * Downgrade�� �ʿ��ϸ� ó���ϰ�, ���� ����� ��ȯ�� �������� ��´�.
     * <p>
     * ���� Downgrade�� ���� ��ȯ�� �ʿ� ���ٸ� ������ �Ѿ�� ���� �������� ��ȯ�Ѵ�.
     * 
     * @param aOrgQstr ���� ������
     * @return ������ ����� ������
     * @throws SQLException Downgrade ���� Ȯ�� �Ǵ� Downgrade�� ������ ���
     */
    protected final String procDowngradeAndGetTargetSql(String aOrgQstr) throws SQLException
    {
        mTargetResultSetType = getResultSetType();
        mTargetResultSetConcurrency = getResultSetConcurrency();
        mPrepareResultColumns = null;

        if (!usingKeySetDriven())
        {
            return aOrgQstr;
        }

        CmGetColumnInfoResult sColumnInfoResult = null;
        String sQstr = AltiSqlProcessor.makePRowIDAddedSql(aOrgQstr);
        if (sQstr != null)
        {
            // BUG-42424 keyset driven������ prepare����� �ٷ� �޾ƿ;� �ϱ⶧���� deferred�� false�� ������.
            CmProtocol.prepare(getProtocolContext(),
                               mStmtCID,
                               sQstr,
                               (getResultSetHoldability() == ResultSet.HOLD_CURSORS_OVER_COMMIT),
                               true,
                               mConnection.nliteralReplaceOn(),
                               false);

            sColumnInfoResult = getProtocolContext().getGetColumnInfoResult();
        }
        if (sQstr == null ||
            Error.hasServerError(getProtocolContext().getError()) ||
            (mTargetResultSetConcurrency == ResultSet.CONCUR_UPDATABLE &&
             checkUpdatableColumnInfo(sColumnInfoResult) == false))
        {
            downgradeTargetResultSetAttrs();
            sQstr = aOrgQstr;
        }
        else
        {
            // sensitive�� ���� keyset�� �װ� ����Ÿ�� ���� �����´�.
            if (mTargetResultSetType == ResultSet.TYPE_SCROLL_SENSITIVE)
            {
                // getMetaData()�� ���� �÷� ������ ����صд�.
                mPrepareResultColumns = sColumnInfoResult.getColumns();

                HashMap sOrderByMap = new HashMap();
                for (int i = 0; i < mPrepareResultColumns.size(); i++)
                {
                    ColumnInfo sColumnInfo = mPrepareResultColumns.get(i).getColumnInfo();
                    sOrderByMap.put(String.valueOf(i + 1), sColumnInfo.getBaseColumnName());
                    sOrderByMap.put(sColumnInfo.getColumnName(), sColumnInfo.getBaseColumnName());
                    sOrderByMap.put(sColumnInfo.getDisplayColumnName(), sColumnInfo.getBaseColumnName());
                }
                sQstr = AltiSqlProcessor.makeKeySetSql(aOrgQstr, sOrderByMap);
            }
        }
        if (mIsDeferred)  // BUG-42712 deferred �����϶��� Context�� Error�� clear���ش�.
        {
            getProtocolContext().clearError();
        }
        return sQstr;
    }

    private boolean checkUpdatableColumnInfo(CmGetColumnInfoResult aGetColumnInfoResult) throws SQLException
    {
        List<Column> sColumns = aGetColumnInfoResult.getColumns();
        if (sColumns == null || sColumns.size() == 0)
        {
            return false;
        }

        // updatable�̷��� ���� ���̺����� ���������ϰ� ��� �÷��� ���̺� �÷��̾�� �Ѵ�.
        ColumnInfo sColInfo = sColumns.get(0).getColumnInfo();
        if (StringUtils.isEmpty(sColInfo.getBaseColumnName()))
        {
            return false;
        }
        String sBaseTableName = sColInfo.getBaseTableName();
        if (StringUtils.isEmpty(sBaseTableName))
        {
            return false;
        }
        for (int i = 1; i < sColumns.size(); i++)
        {
            sColInfo = sColumns.get(i).getColumnInfo();
            if (StringUtils.isEmpty(sColInfo.getBaseColumnName()))
            {
                return false;
            }
            if (StringUtils.isEmpty(sColInfo.getBaseTableName()))
            {
                return false;
            }
            if (sBaseTableName.compareToIgnoreCase(sColInfo.getBaseTableName()) != 0)
            {
                return false;
            }
        }
        return true;
    }

    // #endregion

    public static void checkAutoGeneratedKeys(int aAutoGeneratedKeys) throws SQLException
    {
        if (!isValidAutoGeneratedKeys(aAutoGeneratedKeys))
        {
            Error.throwSQLException(ErrorDef.INVALID_ARGUMENT,
                                    "Auto generated keys",
                                    "RETURN_GENERATED_KEYS | NO_GENERATED_KEYS",
                                    String.valueOf(aAutoGeneratedKeys));
        }
    }

    static boolean isValidAutoGeneratedKeys(int aAutoGeneratedKeys)
    {
        switch (aAutoGeneratedKeys)
        {
            case RETURN_GENERATED_KEYS:
            case NO_GENERATED_KEYS:
                return true;
            default:
                return false;
        }
    }

    void throwErrorForClosed() throws SQLException
    {
        if (mIsClosed)
        {
            Error.throwSQLException(ErrorDef.CLOSED_STATEMENT);            
        }
        if (mConnection.isClosed())
        {
            Error.throwSQLException(ErrorDef.CLOSED_CONNECTION);            
        }
    }

    private void throwErrorForNullSqlString(String aSql) throws SQLException
    {
        if (aSql == null) 
        {
            Error.throwSQLException(ErrorDef.NULL_SQL_STRING);
        }
    }
    
    private void throwErrorForBatchJob(String aCommand) throws SQLException
    {
        if (mBatchSqlList != null && !mBatchSqlList.isEmpty())  
        { 
            Error.throwSQLException(ErrorDef.SOME_BATCH_JOB, aCommand); 
        }
    }
    
    private void throwErrorForResultSetCount(String aSql) throws SQLException
    {
        if (mPrepareResult.getResultSetCount() == 0) 
        {
            Error.throwSQLException(ErrorDef.NO_RESULTSET, aSql);
        }
        if (mPrepareResult.getResultSetCount() > 1)  
        {
            Error.throwSQLException(ErrorDef.MULTIPLE_RESULTSET_RETURNED, aSql);
        }
    }
    
    // BUG-42424 AltibasePreparedStatement.getMetaData������ ���Ǳ� ������ default scope�� ����
    void throwErrorForStatementNotPrepared() throws SQLException
    {
        if (mPrepareResult == null)
        {
            Error.throwSQLException(ErrorDef.STATEMENT_IS_NOT_PREPARED);
        }
    }
    
    private void throwErrorForExplainTurnedOff() throws SQLException
    {
        if (mConnection.explainPlanMode() == AltibaseConnection.EXPLAIN_PLAN_OFF) 
        {
            Error.throwSQLException(ErrorDef.EXPLAIN_PLAN_TURNED_OFF);
        }
    }

    /**
     * �񵿱������� fetch �� �����ϰ� �ִ� statement �������� Ȯ���Ѵ�.
     */
    protected boolean isAsyncPrefetch()
    {
        return (mConnection.getAsyncPrefetchStatement() == this);
    }

    /**
     * Internal statement �����Ѵ�.
     */
    static protected AltibaseStatement createInternalStatement(AltibaseConnection sConnection) throws SQLException
    {
        AltibaseStatement sInternalStatement = new AltibaseStatement(sConnection);
        sInternalStatement.mIsInternalStatement = true;

        return sInternalStatement;
    }

    /**
     * Internal statement ���θ� Ȯ���ϴ�.
     */
    protected boolean isInternalStatement()
    {
        return mIsInternalStatement;
    }

    /** Prepared ���θ� �����Ѵ�. */
    public boolean isPrepared()
    {
        if (mPrepareResult == null) return false;
        return  mPrepareResult.isPrepared();
    }

    /**
     * Semi-async prefetch ������ ���� SemiAsyncPrefetch ��ü�� ��ȯ�Ѵ�.
     */
    synchronized SemiAsyncPrefetch getSemiAsyncPrefetch()
    {
        return mSemiAsyncPrefetch;
    }

    /**
     * Semi-async prefetch ���� ���� SemiAsyncPrefetch ��ü�� �����ϰ� re-execute �� �����Ѵ�.
     */
    synchronized void setSemiAsyncPrefetch(SemiAsyncPrefetch aSemiAsyncPrefetch)
    {
        mSemiAsyncPrefetch = aSemiAsyncPrefetch;
    }

    /**
     * Trace logging �� statement �� �ĺ��� ���� unique id �� ��ȯ�Ѵ�.
     */
    String getTraceUniqueId()
    {
        return "[StmtId #" + String.valueOf(hashCode()) + "] ";
    }

    /**
     * ���� Ŀ���� �����ִ��� ���θ� �����Ѵ�.
     * @return true Select������ �ƴϰų� ����Ŀ���� ��� ���� ���
     */
    public boolean cursorhasNoData()
    {
        boolean sResult = false;

        AltibaseResultSet sCurrResultSet = mCurrentResultSet;
        if (sCurrResultSet == null || sCurrResultSet instanceof AltibaseEmptyResultSet)
        {
            return true;
        }

        if (mPrepareResult.getResultSetCount() == 0)
        {
            sResult = true;
        }
        // BUG-46513 ResultSet�� �������� ��� ������ ResultSet���� Ȯ���Ѵ�.
        else if ((mCurrentResultIndex >= mExecuteResultMgr.size() - 1) &&
                 (!sCurrResultSet.fetchRemains()))
        {
            sResult = true;
        }

        return sResult;
    }

    public void setMetaConnection(AltibaseShardingConnection aMetaConn)
    {
        mMetaConn = aMetaConn;
    }

    public AltibaseShardingConnection getMetaConn()
    {
        return mMetaConn;
    }

    public void setPrepareResult(CmPrepareResult aPrepareResult)
    {
        // BUG-47274 Sharding statement�κ��� CmPrepareResult��ü�� ���Թ޴´�.
        this.mPrepareResult = aPrepareResult;
    }

    @Override
    public String toString()
    {
        final StringBuilder sSb = new StringBuilder("AltibaseStatement{");
        sSb.append("mStmtCID=").append(mStmtCID);
        sSb.append("mStmtID=").append(getID());
        sSb.append(", mQstr='").append(mQstr).append('\'');
        sSb.append('}');
        return sSb.toString();
    }

    /**
     * ResultSet�� close�ɶ� ȣ��Ǹ� closeOnCompletion�� Ȱ��ȭ �Ǿ� �ִ� ��� Statement�� ���� ResultSet��
     * ��� close �Ǿ��� �� Statement�ڿ��� �����Ѵ�.
     * @throws SQLException ResultSet�� close�Ǿ����� Ȯ���ϴ� ��� ���ܰ� �߻��Ҷ�
     */
    void checkCloseOnCompletion() throws SQLException
    {
        if (!mCloseOnCompletion)
        {
            return;
        }
        // PROJ-2707 ���� getMoreResults()�� �� ������ resultset�� �ִٸ� �׳� ����.
        if (mCurrentResultIndex < mExecuteResultMgr.size() - 1)
        {
            return;
        }
        for (ResultSet sEach : mResultSetList)
        {
            if (!sEach.isClosed())
            {
                return; // PROJ-2707 �ϳ��� close�ȵ� resultset�� ������ �׳� return �Ѵ�.
            }
        }
        try
        {
            close();
        }
        catch (SQLException aEX)
        {
            if (TraceFlag.TRACE_COMPILE && TraceFlag.TRACE_ENABLED)
            {
                mLogger.log(Level.SEVERE, "Statement close failed during closeOnCompletion", aEX);
            }
        }
    }

    @Override
    public void closeOnCompletion() throws SQLException
    {
        mCloseOnCompletion = true;
    }

    @Override
    public boolean isCloseOnCompletion() throws SQLException
    {
        return mCloseOnCompletion;
    }

    @Override
    public long executeLargeUpdate(String aSql) throws SQLException
    {
        execute(aSql);
        return mExecuteResultMgr.getFirst().mUpdatedCount;
    }

    @Override
    public long getLargeUpdateCount() throws SQLException
    {
        throwErrorForClosed();

        if (mResultSetReturned)
        {
            return DEFAULT_UPDATE_COUNT;
        }

        ExecuteResult sResult = mExecuteResultMgr.get(mCurrentResultIndex);

        long sUpdateCount;
        // BUG-38657 ������� ResultSet�̰ų� ���̻� ������� �������� -1�� �����ϰ�
        // �� �ܿ��� ������Ʈ �� �ο���� �����Ѵ�.
        if (sResult.mHasResultSet)
        {
            sUpdateCount = DEFAULT_UPDATE_COUNT;
        }
        else
        {
            sUpdateCount = sResult.mUpdatedCount;
        }
        // BUG-38657 getUpdateCount�� �� �� �̻� ����Ǵ� ��쿡�� -1�� �����ϵ��� �Ѵ�.
        // �� �޼ҵ�� ����� �ѹ��� ����Ǿ�� �Ѵ�.
        sResult.mUpdatedCount = DEFAULT_UPDATE_COUNT;

        return sUpdateCount;
    }

    @Override
    public long getLargeMaxRows() throws SQLException
    {
        return mMaxRows;
    }

    @Override
    public void setLargeMaxRows(long aMax) throws SQLException
    {
        throwErrorForClosed();
        if (aMax < 0)
        {
            Error.throwSQLException(ErrorDef.INVALID_ARGUMENT,
                                    "Max rows",
                                    "0 ~ Long.MAX_VALUE",
                                    String.valueOf(aMax));
        }

        mMaxRows = aMax;
    }

    @Override
    public void setPoolable(boolean aPoolable)
    {
        this.mIsPoolable = aPoolable;
    }

    @Override
    public boolean isPoolable()
    {
        return mIsPoolable;
    }

    protected int[] toIntBatchUpdateCounts(long aLongUpdateCounts[])
    {
        int[] sIntUpdateCounts = new int[aLongUpdateCounts.length];
        for (int i = 0; i < aLongUpdateCounts.length; i++)
        {
            sIntUpdateCounts[i] = toInt(aLongUpdateCounts[i]);
        }
        return sIntUpdateCounts;
    }

    protected int toInt(long aUpdateCount)
    {
        return aUpdateCount > Integer.MAX_VALUE ? Statement.SUCCESS_NO_INFO : (int)aUpdateCount;
    }

    public boolean getIsSuccess()
    {
        return mIsSuccess;
    }
}
