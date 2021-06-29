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

package Altibase.jdbc.driver.cm;

import Altibase.jdbc.driver.AltibaseStatement;
import Altibase.jdbc.driver.sharding.core.AltibaseShardingConnection;
import Altibase.jdbc.driver.sharding.core.GlobalTransactionLevel;

public class CmProtocolContextShardStmt extends CmProtocolContext
{
    private AltibaseShardingConnection    mMetaConn;
    private CmShardAnalyzeResult          mShardAnalyzeResult;
    private CmPrepareResult               mShardPrepareResult;

    /* BUG-46513 Node ���� ���Ŀ� getUpdateCount()�� �����ص� ����� ���ҽ�Ű�� �ʱ� ����
       row count�� Statement shard context�� �����Ѵ�.  */
    private int                           mUpdateRowcount;

    public CmProtocolContextShardStmt(AltibaseShardingConnection aMetaConn, AltibaseStatement aStmt)
    {
        mShardAnalyzeResult = new CmShardAnalyzeResult();
        mShardPrepareResult = new CmPrepareResult();
        aStmt.setPrepareResult(mShardPrepareResult); // BUG-47274 Analyze������ ������ statement�� CmPrepareResult��ü�� �����Ѵ�.
        /* BUG-46790 failover�� shard context��ü�� ���� �����Ǳ⶧���� context��ü�� ������ �ִ�
           AltibaseShardingConnection ��ü�� �����Ѵ�. */
        mMetaConn = aMetaConn;
        // mMetaConnection.mDistTxInfo�� CmProtocolContextShardStmt.mDistTxInfo�� �����Ѵ�.
        setDistTxInfo(aStmt.getAltibaseConnection().getDistTxInfo());
    }

    public CmProtocolContextShardStmt(AltibaseShardingConnection aMetaConn, AltibaseStatement aStmt,
                                      CmPrepareResult aPrepareResult)
    {
        mShardAnalyzeResult = new CmShardAnalyzeResult();
        mShardPrepareResult = aPrepareResult;
        aStmt.setPrepareResult(mShardPrepareResult);
        mMetaConn = aMetaConn;
        // mMetaConnection.mDistTxInfo�� CmProtocolContextShardStmt.mDistTxInfo�� �����Ѵ�.
        setDistTxInfo(aStmt.getAltibaseConnection().getDistTxInfo());
    }

    public CmShardAnalyzeResult getShardAnalyzeResult()
    {
        return mShardAnalyzeResult;
    }

    public CmPrepareResult getShardPrepareResult()
    {
        return mShardPrepareResult;
    }

    public void setShardPrepareResult(CmPrepareResult aShardPrepareResult)
    {
        this.mShardPrepareResult = aShardPrepareResult;
    }

    public CmProtocolContextShardConnect getShardContextConnect()
    {
        return mMetaConn.getShardContextConnect();
    }

    public GlobalTransactionLevel getGlobalTransactionLevel()
    {
        return mMetaConn.getGlobalTransactionLevel();
    }

    public boolean isAutoCommitMode()
    {
        return mMetaConn.getShardContextConnect().isAutoCommitMode();
    }

    public int getUpdateRowcount()
    {
        return mUpdateRowcount;
    }

    public void setUpdateRowcount(int aUpdateRowcount)
    {
        mUpdateRowcount = aUpdateRowcount;
    }
}
