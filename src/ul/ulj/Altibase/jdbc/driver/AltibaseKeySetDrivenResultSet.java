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
import java.util.HashMap;
import java.util.Map;

import Altibase.jdbc.driver.cm.CmFetchResult;
import Altibase.jdbc.driver.ex.Error;
import Altibase.jdbc.driver.ex.ErrorDef;

final class AltibaseKeySetDrivenResultSet extends AltibaseScrollInsensitiveResultSet
{
    private int                mRowIdColumnIndex;
    private Map<Long, Integer> mKeyMap;
    private int                mNextFillIndex;

    AltibaseKeySetDrivenResultSet(AltibaseStatement aStatement, CmFetchResult aFetchResult, int aFetchCount)
    {
        init(aStatement, aFetchResult, aFetchCount);
    }

    /**
     * Keyset-driven���� ������ Result Set�� ���Ѵ�.
     * <p>
     * base result set�� �ݵ�� Keyset-driven(scroll-sensitive �Ǵ� updatable)�� �� ���̾�� �ϸ�,
     * key column�� SQL BIGINT ������ target�� ���� �������� �־�� �Ѵ�.
     *
     * @param aBaseResultSet Keyset-driven���� ������ Result Set
     */
    AltibaseKeySetDrivenResultSet(AltibaseScrollInsensitiveResultSet aBaseResultSet)
    {
        init(aBaseResultSet.mStatement, aBaseResultSet.mFetchResult, aBaseResultSet.mFetchSize);
    }

    /**
     * ��ü�� ������ �� �ְ� ���ִ� ������.
     * �� �����ڸ� ���� ���� ��ü�� �ݵ�� ��� ���� ���� �Լ��� ���� �ʱ�ȭ�� �ؾ��Ѵ�:
     * {@link #init(AltibaseStatement, CmFetchResult, int)}
     * <p>
     * ���߿� �ʱ�ȭ�ϴ� ����� ����� �� �ְ� �ϴ°��� ������ ���� ��� �� ����� ���̷��� ���̴�.
     */
    AltibaseKeySetDrivenResultSet()
    {
    }

    void init(AltibaseStatement aStatement, CmFetchResult aFetchResult, int aFetchCount)
    {
        mStatement = aStatement;
        mFetchResult = aFetchResult;
        mFetchSize = aFetchCount;

        mRowIdColumnIndex = getTargetColumnCount();
        if (mKeyMap == null)
        {
            mKeyMap = new HashMap<Long, Integer>();
        }
        else
        {
            mKeyMap.clear();
        }
        mNextFillIndex = 1;
    }

    public boolean absoluteByProwID(long sProwID) throws SQLException
    {
        throwErrorForClosed();

        ensureFillKeySetMap();

        Integer sRowPos = mKeyMap.get(sProwID);
        if (sRowPos == null)
        {
            return false;
        }
        return absolute(sRowPos);
    }

    private void ensureFillKeySetMap() throws SQLException
    {
        if (mNextFillIndex > rowHandle().size())
        {
            return;
        }

        int sOldPos = rowHandle().getPosition();
        rowHandle().setPosition(mNextFillIndex - 1);
        for (; mNextFillIndex <= rowHandle().size(); mNextFillIndex++)
        {
            boolean sNextResult = rowHandle().next();
            if (!sNextResult)
            {
                Error.throwInternalError(ErrorDef.INTERNAL_DATA_INTEGRITY_BROKEN);
            }
            long sProwID = getTargetColumn(mRowIdColumnIndex).getLong();
            mKeyMap.put(sProwID, mNextFillIndex);
        }
        rowHandle().setPosition(sOldPos);
    }
}
