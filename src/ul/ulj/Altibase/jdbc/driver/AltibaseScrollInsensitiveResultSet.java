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

import java.sql.SQLException;
import java.util.List;

import Altibase.jdbc.driver.cm.CmFetchResult;
import Altibase.jdbc.driver.cm.CmProtocol;
import Altibase.jdbc.driver.cm.CmProtocolContextDirExec;
import Altibase.jdbc.driver.datatype.RowHandle;
import Altibase.jdbc.driver.ex.Error;

public class AltibaseScrollInsensitiveResultSet extends AltibaseScrollableResultSet
{
    protected CmFetchResult            mFetchResult;

    AltibaseScrollInsensitiveResultSet(AltibaseStatement aStatement, CmProtocolContextDirExec aContext, int aFetchCount)
    {
        mContext = aContext;
        mStatement = aStatement;
        mFetchResult = aContext.getFetchResult();
        mFetchSize = aFetchCount;
    }

    /**
     * ���� Ŭ���� ������ �ߺ� �۾��� ���ϱ� ���� ������.
     * �� �����ڸ� �� ���� protected ����� ����Ŭ�������� ���� �ʱ�ȭ ����� �Ѵ�.
     */
    protected AltibaseScrollInsensitiveResultSet()
    {
    }

    public void close() throws SQLException
    {
        if (isClosed())
        {
            return;
        }

        int sResultSetCount = mContext.getPrepareResult().getResultSetCount();
        
        if(mStatement instanceof AltibasePreparedStatement)
        {
            if(sResultSetCount > 1)
            {
                if(mStatement.mCurrentResultIndex == (sResultSetCount - 1))
                {
                    ((AltibasePreparedStatement)mStatement).closeAllResultSet();
                    closeResultSetCursor();
                }
                else
                {
                    ((AltibasePreparedStatement)mStatement).closeCurrentResultSet();
                }
            }
            else
            {
                ((AltibasePreparedStatement)mStatement).closeCurrentResultSet();
                closeResultSetCursor();
            }
        }
        else
        {
            closeResultSetCursor();
        }
        
        super.close();
    }

    protected RowHandle rowHandle()
    {
        return mFetchResult.rowHandle();
    }

    protected List getTargetColumns()
    {
        return mFetchResult.getColumns();
    }

    public boolean isAfterLast() throws SQLException
    {
        throwErrorForClosed();
        return !mFetchResult.fetchRemains() && mFetchResult.rowHandle().isAfterLast();
    }

    public int getRow() throws SQLException
    {
        if (isBeforeFirst() || isAfterLast())
        {
            return 0;
        }
        return mFetchResult.rowHandle().getPosition();
    }

    public boolean isBeforeFirst() throws SQLException
    {
        throwErrorForClosed();
        return mFetchResult.rowHandle().isBeforeFirst();
    }

    public boolean isFirst() throws SQLException
    {
        throwErrorForClosed();
        return mFetchResult.rowHandle().isFirst();
    }

    public boolean isLast() throws SQLException
    {
        throwErrorForClosed();
        if (mFetchResult.fetchRemains())
        {
            // fetch�Ұ� ���� ������ �ּ��� �� row �̻��� �ִٰ� ������ ���̴�.
            // ���� fetch�Ұ� ���� �ִٰ� ������ ���� fetchNext�� ��û���� ��
            // row�� �ϳ��� �ȿ��� fetch end�� �� �� �ִٸ� �� �ڵ�� �����̴�.
            return false;
        }
        else
        {
            return mFetchResult.rowHandle().isLast();
        }
    }

    public boolean absolute(int aRow) throws SQLException
    {
        throwErrorForClosed();
        cursorMoved();
        boolean sResult;
        if (aRow < 0)
        {
            fetchAll();
            // aRow�� -1�̸� last ��ġ�� ����Ų��.
            aRow = mFetchResult.rowHandle().size() + 1 + aRow;
            sResult = mFetchResult.rowHandle().setPosition(aRow);
        }
        else
        {
            if (checkCache(aRow))
            {
                sResult = mFetchResult.rowHandle().setPosition(aRow);
            }
            else
            {
                mFetchResult.rowHandle().afterLast();
                sResult = false;
            }
        }
        return sResult;
    }

    public void afterLast() throws SQLException
    {
        throwErrorForClosed();
        cursorMoved();
        fetchAll();
        mFetchResult.rowHandle().afterLast();
    }

    public void beforeFirst() throws SQLException
    {
        throwErrorForClosed();
        cursorMoved();
        mFetchResult.rowHandle().beforeFirst();
    }

    public boolean previous() throws SQLException
    {
        throwErrorForClosed();
        cursorMoved();
        return mFetchResult.rowHandle().previous();
    }

    public boolean next() throws SQLException
    {
        throwErrorForClosed();
        cursorMoved();
        boolean sResult;
        while (true)
        {
            sResult = mFetchResult.rowHandle().next();
            if (sResult)
            {
                break;
            }
            else
            {
                if (mFetchResult.fetchRemains())
                {
                    // forward only�ʹ� �ٸ��� cache�� ������ �ʴ´�.
                    try
                    {
                        CmProtocol.fetchNext(mStatement.getProtocolContext(), mFetchSize);
                    }
                    catch (SQLException ex)
                    {
                        AltibaseFailover.trySTF(mStatement.getAltibaseConnection().failoverContext(), ex);
                    }
                    if (mStatement.getProtocolContext().getError() != null)
                    {
                        mWarning = Error.processServerError(mWarning, mStatement.getProtocolContext().getError());
                    }
                    // continue�� �ϸ� next()�� �ѹ� �̵��� ���̱� ������ �̸� ��ĭ ����.
                    mFetchResult.rowHandle().previous();
                    continue;
                }
                else
                {
                    break;
                }
            }
        }
        return sResult;
    }

    public int getType() throws SQLException
    {
        throwErrorForClosed();
        return TYPE_SCROLL_INSENSITIVE;
    }

    public void refreshRow() throws SQLException
    {
        // do nothing
    }

    private void fetchAll() throws SQLException
    {
        while (mFetchResult.fetchRemains())
        {
            try
            {
                CmProtocol.fetchNext(mStatement.getProtocolContext(), mFetchSize);
            }
            catch (SQLException ex)
            {
                AltibaseFailover.trySTF(mStatement.getAltibaseConnection().failoverContext(), ex);
            }
            if (mStatement.getProtocolContext().getError() != null)
            {
                mWarning = Error.processServerError(mWarning, mStatement.getProtocolContext().getError());
            }
        }
    }

    private boolean checkCache(int aRow) throws SQLException
    {
        while (mFetchResult.fetchRemains() && mFetchResult.rowHandle().size() < aRow)
        {
            try
            {
                CmProtocol.fetchNext(mStatement.getProtocolContext(), mFetchSize);
            }
            catch (SQLException ex)
            {
                AltibaseFailover.trySTF(mStatement.getAltibaseConnection().failoverContext(), ex);
            }
            if (mStatement.getProtocolContext().getError() != null)
            {
                mWarning = Error.processServerError(mWarning, mStatement.getProtocolContext().getError());
            }
        }
        return mFetchResult.rowHandle().size() >= aRow;
    }

    int size()
    {
        if (mFetchResult.fetchRemains())
        {
            return Integer.MAX_VALUE;
        }
        else
        {
            return mFetchResult.rowHandle().size();
        }
    }

    void deleteRowInCache() throws SQLException
    {
        rowHandle().delete();
        previous();
    }
}
