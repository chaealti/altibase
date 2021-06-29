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

import java.sql.ResultSet;
import java.sql.SQLException;
import java.util.List;

import Altibase.jdbc.driver.cm.CmProtocolContextDirExec;
import Altibase.jdbc.driver.datatype.RowHandle;
import Altibase.jdbc.driver.ex.Error;
import Altibase.jdbc.driver.ex.ErrorDef;
import Altibase.jdbc.driver.util.AltiSqlProcessor;

final class AltibaseScrollSensitiveResultSet extends AltibaseScrollableResultSet
{
    private static final int              DEFAULT_ROWSET_FETCH_SIZE = 20;

    private AltibasePreparedStatement     mRowFetchStmt;
    private AltibaseScrollableResultSet   mKeySet;
    private AltibaseKeySetDrivenResultSet mRowSet;
    private int                           mLastFetchStartIndex;
    private final String                  mBaseSql;
    private String                        mRowSetSql;
    private boolean                       mRowDeleted;

    protected AltibaseScrollSensitiveResultSet(AltibaseStatement aStatement, CmProtocolContextDirExec aContext, int aFetchCount) throws SQLException
    {
        mStatement = aStatement;
        mBaseSql = aStatement.getSql();

        try
        {
            mKeySet = new AltibaseScrollInsensitiveResultSet(mStatement, aContext, aFetchCount);
            mRowSet = new AltibaseKeySetDrivenResultSet();
            setFetchSize(aFetchCount);
            fetchRowSet(1);
        }
        catch (SQLException e)
        {
            close();
            throw e;
        }
    }

    public void close() throws SQLException
    {
        if (isClosed())
        {
            return;
        }

        if (mRowFetchStmt != null)
        {
            mRowFetchStmt.close();
        }
        if (mKeySet != null)
        {
            mKeySet.close();
        }

        super.close();
    }

    private void ensureRowFetchStmt(int aFetchSize) throws SQLException
    {
        if (mRowFetchStmt != null)
        {
            if (mRowFetchStmt.getFetchSize() == aFetchSize)
            {
                // �̹� ���ϴ¹ٴ�� �Ǿ������Ƿ� �ٽ� �� �ʿ� ����.
                return;
            }
            else
            {
                mRowFetchStmt.close();
            }
        }

        mRowSetSql = AltiSqlProcessor.makeRowSetSql(mBaseSql, aFetchSize);
        mRowFetchStmt = (AltibasePreparedStatement)mStatement.mConnection.prepareStatement(mRowSetSql, ResultSet.TYPE_SCROLL_INSENSITIVE, ResultSet.CONCUR_READ_ONLY);
        mRowFetchStmt.setFetchSize(aFetchSize);
    }

    protected RowHandle rowHandle()
    {
        return mRowSet.rowHandle();
    }

    protected List getTargetColumns()
    {
        return mRowSet.getTargetColumns();
    }

    private void checkAndRefreshRowSetData() throws SQLException
    {
        mRowDeleted = false;

        if (mKeySet.isBeforeFirst() || mKeySet.isAfterLast())
        {
            // ������ ���� ���� �� �� �����Ƿ�, �׳� �Ѿ�°� ����.
            return;
        }

        // �� Ŀ�� ��ġ�� ���̳� �ڳĿ� ���� ���� �� �ϳ��� ����
        // - ��: aAbsolutePos - FETCH_SIZE ~ aAbsolutePos
        // - ��: aAbsolutePos ~ aAbsolutePos + FETCH_SIZE
        int sNewPosition = mKeySet.getRow();
        if (sNewPosition < mLastFetchStartIndex)
        {
            fetchRowSet(sNewPosition - mFetchSize + 1);
        }
        else if (sNewPosition >= mLastFetchStartIndex + mRowSet.getFetchSize())
        {
            fetchRowSet(sNewPosition);
        }

        // RowSet�� ������ Hole�̴�.
        long sPRowID = mKeySet.getLong(AltiSqlProcessor.KEY_SET_ROWID_COLUMN_INDEX);
        mRowDeleted = !mRowSet.absoluteByProwID(sPRowID);
    }

    private void fetchRowSet(int aFirstIndex) throws SQLException
    {
        if (size() == 0)
        {
            return;
        }

        ensureRowFetchStmt(mFetchSize);

        aFirstIndex = Math.max(Math.min(aFirstIndex, size() - mFetchSize + 1), 1);
        int sOldPos = mKeySet.rowHandle().getPosition();
        boolean sRow = mKeySet.absolute(aFirstIndex);
        int sValidRowCount = 0;
        for (int i = 1; i <= mFetchSize; i++)
        {
            // TODO BUGBUG rowset statement�� �ٽ� �����ϴ°� ������ ����غ� ��. �ܷ��� �˰��� ����.
            if (sRow)
            {
                mRowFetchStmt.setLong(i, mKeySet.getLong(AltiSqlProcessor.KEY_SET_ROWID_COLUMN_INDEX));
                sValidRowCount++;
            }
            else
            {
                mRowFetchStmt.setLong(i, Long.MAX_VALUE);
            }
            sRow = mKeySet.next();
        }
        // ���� Ȯ���� �ϰ� �����ϹǷ�, ��� 1�� �̻��� �ִ�.
        if (sValidRowCount == 0) 
        {
            Error.throwInternalError(ErrorDef.INTERNAL_DATA_INTEGRITY_BROKEN);
        }

        mRowFetchStmt.executeQuery();
        mRowSet.init(mRowFetchStmt, mRowFetchStmt.mFetchResult, mRowFetchStmt.mFetchSize);

        mLastFetchStartIndex = aFirstIndex;
        mKeySet.rowHandle().setPosition(sOldPos);
    }

    public boolean isAfterLast() throws SQLException
    {
        throwErrorForClosed();
        return mKeySet.isAfterLast();
    }

    public boolean isBeforeFirst() throws SQLException
    {
        throwErrorForClosed();
        return mKeySet.isBeforeFirst();
    }

    public boolean isFirst() throws SQLException
    {
        throwErrorForClosed();
        return mKeySet.isFirst();
    }

    public boolean isLast() throws SQLException
    {
        throwErrorForClosed();
        return mKeySet.isLast();
    }

    public boolean absolute(int aRow) throws SQLException
    {
        throwErrorForClosed();
        boolean sIsSuccess = mKeySet.absolute(aRow);
        if (sIsSuccess)
        {
            checkAndRefreshRowSetData();
        }
        cursorMoved();
        return sIsSuccess;
    }

    public void afterLast() throws SQLException
    {
        throwErrorForClosed();
        mKeySet.afterLast();
        checkAndRefreshRowSetData();
        cursorMoved();
    }

    public void beforeFirst() throws SQLException
    {
        throwErrorForClosed();
        mKeySet.beforeFirst();
        checkAndRefreshRowSetData();
        cursorMoved();
    }

    public boolean previous() throws SQLException
    {
        throwErrorForClosed();
        boolean sIsSuccess = mKeySet.previous();
        if (sIsSuccess)
        {
            checkAndRefreshRowSetData();
        }
        cursorMoved();
        return sIsSuccess;
    }

    public boolean next() throws SQLException
    {
        throwErrorForClosed();
        boolean sIsSuccess = mKeySet.next();
        if (sIsSuccess)
        {
            checkAndRefreshRowSetData();
        }
        cursorMoved();
        return sIsSuccess;
    }

    public void refreshRow() throws SQLException
    {
        throwErrorForClosed();
        if (isBeforeFirst()) 
        {
            Error.throwSQLException(ErrorDef.CURSOR_AT_BEFORE_FIRST);
        }
        if (isAfterLast())
        {
            Error.throwSQLException(ErrorDef.CURSOR_AT_AFTER_LAST);
        }

        // ���� ������Ʈ�� ���� ����
        mLastFetchStartIndex = Integer.MAX_VALUE;

        checkAndRefreshRowSetData();
    }

    public int getFetchSize() throws SQLException
    {
        throwErrorForClosed();
        return mFetchSize;
    }

    public void setFetchSize(int aRows) throws SQLException
    {
        throwErrorForClosed();

        if (aRows == 0)
        {
            // �Ϲ������� fetch count�� 0�̸� ��Ź��۰� ����ϴ� ��ŭ ����������,
            // scrollable resultset�� rowset�� ������ ���� �׷��� �� ���� ���� ������
            // ���Ƿ� ROWSET_FETCH_SIZE�� ���ؼ� �׸�ŭ�� �����´�.
            aRows = DEFAULT_ROWSET_FETCH_SIZE;
        }

        mKeySet.setFetchSize(aRows);
        mFetchSize = aRows;
    }

    public int getRow() throws SQLException
    {
        throwErrorForClosed();
        return mKeySet.getRow();
    }

    public int getType() throws SQLException
    {
        throwErrorForClosed();
        return TYPE_SCROLL_SENSITIVE;
    }

    int size()
    {
        return mKeySet.size();
    }

    public boolean rowDeleted() throws SQLException
    {
        throwErrorForClosed();
        return mRowDeleted;
    }

    void deleteRowInCache() throws SQLException
    {
        mKeySet.deleteRowInCache();
        checkAndRefreshRowSetData();
        cursorMoved();
    }
}
