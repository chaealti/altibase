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
import java.util.logging.Level;
import java.util.logging.Logger;

import Altibase.jdbc.driver.cm.CmSocket;
import Altibase.jdbc.driver.logging.LoggingProxy;
import Altibase.jdbc.driver.logging.TraceFlag;
import Altibase.jdbc.driver.util.AltibaseProperties;

/**
 * �񵿱� fetch ��� �� �ϳ��μ�, TCP kernel buffer �� �̿��� double buffering �ϴ� semi-async prefetch ���.
 * (PROJ-2625 Semi-async Prefetch, Prefetch Auto-tuning)
 */
class SemiAsyncPrefetch
{
    private AltibaseForwardOnlyResultSet mResultSet;
    private AltibaseStatement            mStatement;
    private AltibaseConnection           mConnection;
    private AltibaseProperties           mProperties;

    private SemiAsyncPrefetchAutoTuner   mAutoTuner;
    private JniExt                       mJniExt;
    private int                          mSocketFD = CmSocket.INVALID_SOCKTFD;

    private transient Logger             mLogger;
    private transient Logger             mAsyncLogger;

    SemiAsyncPrefetch(AltibaseForwardOnlyResultSet aResultSet) throws SQLException
    {
        mResultSet   = aResultSet;
        mStatement   = aResultSet.getAltibaseStatement();
        mConnection  = mStatement.getAltibaseConnection();
        mProperties  = mConnection.getProperties();

        if (TraceFlag.TRACE_COMPILE && TraceFlag.TRACE_ENABLED)
        {
            mLogger = Logger.getLogger(LoggingProxy.JDBC_LOGGER_DEFAULT);
            mAsyncLogger = Logger.getLogger(LoggingProxy.JDBC_LOGGER_ASYNCFETCH);

            if (checkJniExtModule())
            {
                try
                {
                    mSocketFD = mConnection.channel().getSocketFD();
                }
                catch (Exception e)
                {
                    // Trace logging �뵵�� socket file descriptor �� ���ϱ� ������ ���� �߻��Ͽ��� ������.
                    mAsyncLogger.log(Level.WARNING, mStatement.getTraceUniqueId() + e);
                }
            }
        }

        if (checkAutoTuning())
        {
            try
            {
                mAutoTuner = new SemiAsyncPrefetchAutoTuner(aResultSet, mJniExt);
            }
            catch (Exception e)
            {
                // SemiAsyncPrefetchAutoTuner ��ü ������ ���� �߻��ϸ� auto-tuning ����� OFF ��Ű�� ���� ������.

                if (TraceFlag.TRACE_COMPILE && TraceFlag.TRACE_ENABLED)
                {
                    mAsyncLogger.log(Level.WARNING, mStatement.getTraceUniqueId() + e);
                }
            }
        }
    }

    /**
     * �񵿱� prefetch �ϰ��� �ϴ� ���� result set �� �����Ѵ�.
     * e.g.) �� �̻��� result set ���� ���ν��� ����, close cursor ���� execute �����
     */
    void nextResultSet(AltibaseForwardOnlyResultSet aResultSet)
    {
        mResultSet = aResultSet;

        if (mAutoTuner != null)
        {
            mAutoTuner.nextResultSet(aResultSet);
        }
    }

    /**
     * �񵿱� fetch ���������� �۽��Ͽ����� ���θ� Ȯ���Ѵ�.
     */
    boolean isAsyncSentFetch()
    {
        return mConnection.channel().isAsyncSent();
    }

    /**
     * Auto-tuning ���� ���θ� Ȯ���Ѵ�.
     */
    boolean isAutoTuning()
    {
        return (mAutoTuner != null);
    }

    /**
     * Auto-tuning �� ������ �� �ִ��� ���θ� Ȯ���Ѵ�. ����, auto-tuning ���� �����Ѵٸ� �ٽ� cursor �� open �ϱ� ������ OFF �ȴ�.
     */
    boolean canAutoTuning()
    {
        if (mAutoTuner == null)
        {
            return false;
        }

        return mAutoTuner.canAutoTuning();
    }

    /**
     * Auto-tuning �� ���۵� ��� SemiAsyncPrefetchAutoTuner ��ü�� ��ȯ�Ѵ�.
     */
    SemiAsyncPrefetchAutoTuner getAutoTuner()
    {
        return mAutoTuner;
    }

    /**
     * Auto-tuning ����� ������ �� �ִ��� �˻��Ͽ� JNI ext. module �� load ��Ų��.
     */
    private boolean checkAutoTuning()
    {
        // check 'FetchAutoTuning' property
        if (!mProperties.isFetchAutoTuning())
        {
            return false;
        }

        return checkJniExtModule();
    }

    /**
     * JNI ext. module �� load ��Ų��.
     */
    private boolean checkJniExtModule()
    {
        if (!JniExtLoader.checkLoading())
        {
            return false;
        }

        mJniExt = new JniExt();

        return true;
    }

    /**
     * Heuristic �� semi-async prefetch ���ۿ� ���� trace logging �Ѵ�.
     */
    void logStatHeuristic()
    {
        if (TraceFlag.TRACE_COMPILE && TraceFlag.TRACE_ENABLED)
        {
            if (JniExtLoader.checkLoading() &&
                mSocketFD != CmSocket.INVALID_SOCKTFD)
            {
                try
                {
                    long sNetworkIdleTime = mJniExt.getTcpiLastDataRecv(mSocketFD);

                    int sFetchSize = mResultSet.getFetchSize();

                    mAsyncLogger.log(Level.INFO, mStatement.getTraceUniqueId() + "fetch size = " + sFetchSize + ", r = " + sNetworkIdleTime);
                }
                catch (Throwable e)
                {
                    // ignore exceptions

                    mAsyncLogger.log(Level.WARNING, e.toString());
                }
            }
        }
    }
}
