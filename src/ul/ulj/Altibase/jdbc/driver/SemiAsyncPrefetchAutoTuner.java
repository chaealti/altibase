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

import java.io.IOException;
import java.sql.SQLException;
import java.util.logging.Level;
import java.util.logging.Logger;

import Altibase.jdbc.driver.cm.CmChannel;
import Altibase.jdbc.driver.cm.CmFetchResult;
import Altibase.jdbc.driver.ex.Error;
import Altibase.jdbc.driver.logging.LoggingProxy;
import Altibase.jdbc.driver.logging.TraceFlag;
import Altibase.jdbc.driver.util.AltibaseProperties;

/**
 * Semi-async prefetch ����� auto-tuning ������μ�,
 * network idle time �� �ּ�ȭ�ϵ��� prefetch rows �� ���������� ������. ��, LINUX OS platform �� ������.
 * (PROJ-2625 Semi-async Prefetch, Prefetch Auto-tuning)<br>
 * <pre>
 * [notation]
 * 
 *   d           : application load time (ms)
 *   f           : prefetch time (ms)
 *   r           : elapsed time from last data receive, network idle time (ms)
 *                 r = { d - f      , if d < f  }
 *                     { 0          , otherwise }
 *   k           : socket read time (ms)
 *   t           : time
 *                 (e.g. t-1 = previous time
 *                       t   = current time
 *                       t+1 = next time)
 *   x_(t+1)     : predicted prefetch rows
 * </pre>
 */
class SemiAsyncPrefetchAutoTuner
{
    private enum AutoTuningState
    {
        Init,
        Tuning,
        Stable,
        Optimal,
        Flooded,
        Reset,
        Failed,
        End
    }

    // decision for prediction
    static final int    AUTO_TUNING_DECISION_UNKNOWN  = -2;
    static final int    AUTO_TUNING_DECISION_INCREASE = -1;
    static final int    AUTO_TUNING_DECISION_STABLE   =  0;
    static final int    AUTO_TUNING_DECISION_DECREASE =  1;

    static final int    AUTO_TUNING_PREFETCH_ROWS_MIN = 1;
    static final int    AUTO_TUNING_PREFETCH_ROWS_MAX = Integer.MAX_VALUE;

    static final double AUTO_TUNING_EPSILON           = 0.0000001;

    // auto-tuning parameters
    static final int    AUTO_TUNING_R_THRESHOLD_MIN            = 1;   // 1 ms
    static final int    AUTO_TUNING_R_THRESHOLD_MAX            = 3;   // 3 ms
    static final int    AUTO_TUNING_R_THRESHOLD_MAX_ON_OPTIMAL = 4;   // 4 ms
    static final int    AUTO_TUNING_OPTIMAL_THRESHOLD          = 10;
    static final int    AUTO_TUNING_R_0_IN_OPTIMAL_STATE       = 3;
    static final double AUTO_TUNING_INCREASE_RATIO             = 1.5;
    static final int    AUTO_TUNING_DF_RATIO_MIN_PREFETCH_TIME = 10;   // 10 ms
    static final double AUTO_TUNING_DF_RATIO_STABLE_THRESHOLD  = 0.85;
    static final int    AUTO_TUNING_FLOOD_THRESHOLD            = 5;
    static final int    AUTO_TUNING_FETCHED_BYTES_MAX          = 2 * 1024 * 1024; // 2MB

    private CmFetchResult                mFetchResult;
    private AltibaseForwardOnlyResultSet mResultSet;
    private AltibaseStatement            mStatement;
    private AltibaseConnection           mConnection;
    private AltibaseProperties           mProperties;
    private CmChannel                    mCmChannel;
    private int                          mSocketFD;
    private JniExt                       mJniExt;
    private transient Logger             mAsyncLogger;

    // statistics
    private long             mBeginTimeToMeasure;               // d, f time �� �����ϱ� ���� ���� �ð�
    private long             mEndTimeToMeasure;                 // d, f time �� �����ϱ� ���� �� �ð�
    private long             mTotalElapsedTime;                 // d + k time
    private long             mAppLoadTime;                      // d time
    private long             mPrefetchTime;                     // f time
    private double           mDFRatio;                          // d / f ratio
    private int              mDecision;                         // prefetch rows ��/�� ����
    private int              mAppReadRows;                      // d time ���� application ���� read �� rows
    private int              mFetchedRows;                      // f time ���� fetched rows
    private long             mNetworkIdleTime;                  // r time
    private long             mSockReadTime;                     // k time

    // state
    private AutoTuningState  mState = AutoTuningState.Init;     // auto-tuing state
    private int              mPrefetchRows;                     // x_(t+1) : ������ prefetch rows
    private int              mLastStablePrefetchRows;           // ���������� stable �� prefetch rows
    private boolean          mIsTrialOneMore;                   // ��Ȯ�� feedback �� ���� �ѹ� �� ���������� ����
    private int              mDFRatioUnstableCount;             // ���� ���¸� �˻��ϱ� ���� d / f ������ unstable ȸ��
    private int              mConsecutiveCount;                 // ������ prefetch rows �� ������ ���� ȸ��
    private int              mRZeroCountOnOptimal;              // optimal ���¿��� r = 0 ȸ��
    private boolean          mIsAdjustedSockRcvBuf;             // socket receive buffer �� �����Ͽ������� ����
    private int              mLastAdjustedSockRcvBufSize;       // auto-tuned prefetch rows �� ���� ���������� ������ socket receive buffer size

    // statistics for last prediction
    private AutoTuningState  mLastState = AutoTuningState.Init; // last auto-tuning state
    private int              mLastPrefetchRows;                 // last auto-tuned prefetch rows
    private int              mLastFetchedRows;                  // last fetched rows
    private long             mLastAppLoadTime;                  // last d time
    private long             mLastPrefetchTime;                 // last f time
    private long             mLastNetworkIdleTime;              // last r time
    private long             mLastSockReadTime;                 // last k time
    private int              mLastDecision = AUTO_TUNING_DECISION_UNKNOWN; // last decision

    SemiAsyncPrefetchAutoTuner(AltibaseForwardOnlyResultSet aResultSet, JniExt aJniExt) throws SQLException
    {
        mResultSet   = aResultSet;
        mFetchResult = aResultSet.mContext.getFetchResult();
        mStatement   = aResultSet.getAltibaseStatement();
        mConnection  = mStatement.getAltibaseConnection();
        mProperties  = mConnection.getProperties();
        mCmChannel   = mConnection.channel();
        mSocketFD    = mCmChannel.getSocketFD();
        mJniExt      = aJniExt;

        if (TraceFlag.TRACE_COMPILE && TraceFlag.TRACE_ENABLED)
        {
            mAsyncLogger = Logger.getLogger(LoggingProxy.JDBC_LOGGER_ASYNCFETCH);
        }
    }

    /**
     * auto-tuning �ϰ��� �ϴ� ���� result set �� �����Ѵ�.
     * e.g.) �� �̻��� result set ���� ���ν��� ����, close cursor ���� execute �����
     */
    void nextResultSet(AltibaseForwardOnlyResultSet aResultSet)
    {
        mResultSet   = aResultSet;
        mFetchResult = aResultSet.mContext.getFetchResult();
    }

    /**
     * Auto-tuning �� ������ �� �ִ��� ���θ� Ȯ���Ѵ�. ����, auto-tuning ���� ���� �߻��Ͽ� end �ϸ�
     * AutoTuningState.End ����(last state = AutoTuningState.Failed)�� ���̵ȴ�.
     */
    boolean canAutoTuning()
    {
        if (mState == AutoTuningState.Failed)
        {
            return false;
        }
        else if (mState == AutoTuningState.End && mLastState == AutoTuningState.Failed)
        {
            return false;
        }

        return true;
    }

    /**
     * Auto-tuning �� ����, ��� ���� �ʱ�ȭ�Ѵ�.
     */
    private void initialize()
    {
        mBeginTimeToMeasure               = 0;
        mEndTimeToMeasure                 = 0;
        mTotalElapsedTime                 = 0;
        mAppLoadTime                      = 0;
        mPrefetchTime                     = 0;
        mDFRatio                          = 0;
        mDecision                         = AUTO_TUNING_DECISION_UNKNOWN;
        mAppReadRows                      = 0;
        mFetchedRows                      = 0;
        mNetworkIdleTime                  = 0;
        mSockReadTime                     = 0;

        mState                            = AutoTuningState.Init;
        mPrefetchRows                     = 0;
        mLastStablePrefetchRows           = 0;
        mIsTrialOneMore                   = false;
        mDFRatioUnstableCount             = 0;
        mConsecutiveCount                 = 0;
        mRZeroCountOnOptimal              = 0;
        mIsAdjustedSockRcvBuf             = false;
        mLastAdjustedSockRcvBufSize       = 0;

        mLastState                        = AutoTuningState.Init;
        mLastPrefetchRows                 = 0;
        mLastFetchedRows                  = 0;
        mLastAppLoadTime                  = 0;
        mLastPrefetchTime                 = 0;
        mLastNetworkIdleTime              = 0;
        mLastSockReadTime                 = 0;
        mLastDecision                     = AUTO_TUNING_DECISION_UNKNOWN;
    }

    /**
     * ����ڰ� ������ auto-tuning ���� parameter ���� trace logging �Ѵ�.
     */
    private void traceParameters() throws SQLException
    {
        if (TraceFlag.TRACE_COMPILE && TraceFlag.TRACE_ENABLED)
        {
            // trace logging for auto-tuning parameters
            mAsyncLogger.log(Level.CONFIG, mStatement.getTraceUniqueId() + "auto-tuning : sock_rcvbuf_block_ratio = " + mProperties.getSockRcvBufBlockRatio());
            mAsyncLogger.log(Level.CONFIG, mStatement.getTraceUniqueId() + "auto-tuning : fetch size = " + mResultSet.getFetchSize());
            mAsyncLogger.log(Level.CONFIG, mStatement.getTraceUniqueId() + "auto-tuning : fetch_auto_tuning_init = " + mProperties.getFetchAutoTuningInit());
            mAsyncLogger.log(Level.CONFIG, mStatement.getTraceUniqueId() + "auto-tuning : fetch_auto_tuning_min = " + mProperties.getFetchAutoTuningMin());
            mAsyncLogger.log(Level.CONFIG, mStatement.getTraceUniqueId() + "auto-tuning : fetch_auto_tuning_max = " + mProperties.getFetchAutoTuningMax());
            mAsyncLogger.log(Level.CONFIG, mStatement.getTraceUniqueId() + "auto-tuning : estimated single row size = " + mFetchResult.getTotalOctetLength());
            mAsyncLogger.log(Level.CONFIG, mStatement.getTraceUniqueId() + "auto-tuning : [ d = application load time, f = prefetch time, r = 0 or d - f, k = socket read time ]");
        }
    }

    /**
     * Auto-tuning ��� ������ ������. ��, ������ ������ prefetch rows �� ���� ������ �����Ѵ�.
     */
    private void purgeStat()
    {
        mBeginTimeToMeasure     = 0;
        mEndTimeToMeasure       = 0;
        mTotalElapsedTime       = 0;
        mAppLoadTime            = 0;
        mPrefetchTime           = 0;
        mDFRatio                = 0;
        mAppReadRows            = 0;
        mFetchedRows            = 0;
        mNetworkIdleTime        = 0;
        mSockReadTime           = 0;

        mLastState              = AutoTuningState.Tuning;
        mLastPrefetchRows       = 0;
        mLastFetchedRows        = 0;
        mLastAppLoadTime        = 0;
        mLastPrefetchTime       = 0;
        mLastNetworkIdleTime    = 0;
        mLastSockReadTime       = 0;
        mLastDecision           = 0;
    }

    /**
     * Network idle time (r time) ���� d time �� f time �� �� �����Ѵ�.
     */
    private int compareAppLoadTimeAndPrefetchTime()
    {
        int sRThresholdMin = AUTO_TUNING_R_THRESHOLD_MIN;
        int sRThresholdMax = AUTO_TUNING_R_THRESHOLD_MAX;

        if (mLastState == AutoTuningState.Optimal)
        {
            sRThresholdMax = AUTO_TUNING_R_THRESHOLD_MAX_ON_OPTIMAL;
        }

        if (mNetworkIdleTime < sRThresholdMin)
        {
            return AUTO_TUNING_DECISION_INCREASE;
        }
        else if (mNetworkIdleTime > sRThresholdMax)
        {
            return AUTO_TUNING_DECISION_DECREASE;
        }

        return AUTO_TUNING_DECISION_STABLE;
    }

    /**
     * Auto-tuned prefetch rows �� ������� �ִ� Ȯ���Ǿ�� �ϴ� socket receive buffer �� �����Ѵ�.
     */
    private void adjustAutoTunedSockRcvBuf() throws IOException
    {
        long sSingleRowSize = mFetchResult.getTotalOctetLength();
        double sRowsPerBlock = (double)CmChannel.CM_BLOCK_SIZE / (double)sSingleRowSize;

        // calculate socket receive buffer size that should ensure from auto-tuned prefetch rows
        //
        //                                        n_fetch
        //   s_sock = n_fetch * (5 + s_row) + (------------- + 1) * 16
        //                                      n_per_block
        //
        //   s_sock      : socket receive buffer that should ensure
        //   n_fetch     : prefetch rows
        //   s_row       : single row size
        //   n_per_block : rows per a CM block
        //
        //  cf) 5  = OpID(1) + row length(4)
        //      16 = CM header size

        int sAutoTunedSockRcvBufSize = (int)(
                (mPrefetchRows * (5 + sSingleRowSize)) +
                ((mPrefetchRows / sRowsPerBlock) + 1) * CmChannel.CM_PACKET_HEADER_SIZE);

        // socket receive buffer is 32K unit
        if ((sAutoTunedSockRcvBufSize % CmChannel.CM_BLOCK_SIZE) > 0)
        {
            sAutoTunedSockRcvBufSize =
                    ((sAutoTunedSockRcvBufSize / CmChannel.CM_BLOCK_SIZE) + 1) * CmChannel.CM_BLOCK_SIZE;
        }

        if (sAutoTunedSockRcvBufSize < CmChannel.CM_DEFAULT_RCVBUF_SIZE)
        {
            sAutoTunedSockRcvBufSize = CmChannel.CM_DEFAULT_RCVBUF_SIZE;
        }

        if (mLastAdjustedSockRcvBufSize != sAutoTunedSockRcvBufSize)
        {
            if (mCmChannel.getSockRcvBufSize() != sAutoTunedSockRcvBufSize)
            {
                mCmChannel.setSockRcvBufSize(sAutoTunedSockRcvBufSize);

                mIsAdjustedSockRcvBuf = true;
                mLastAdjustedSockRcvBufSize = sAutoTunedSockRcvBufSize;

                if (TraceFlag.TRACE_COMPILE && TraceFlag.TRACE_ENABLED)
                {
                    mAsyncLogger.log(Level.INFO, mStatement.getTraceUniqueId() + "auto-tuning : socket receive buffer change = " + sAutoTunedSockRcvBufSize);
                }
            }
        }
    }

    /**
     * �Ǽ����� ���� prefetch rows �� �����Ѵ�.
     */
    private void setPrefetchRows(double aPredictedPrefetchRows)
    {
        if (aPredictedPrefetchRows < AUTO_TUNING_PREFETCH_ROWS_MIN)
        {
            mPrefetchRows = AUTO_TUNING_PREFETCH_ROWS_MIN;
        }
        else if (aPredictedPrefetchRows > AUTO_TUNING_PREFETCH_ROWS_MAX)
        {
            mPrefetchRows = AUTO_TUNING_PREFETCH_ROWS_MAX;
        }
        else
        {
            mPrefetchRows = (int)aPredictedPrefetchRows;
        }
    }

    /**
     * ������Ŵ�� �����Ѵ�. ����, ������ prefetch rows �� �����ϴٸ� 1 ������Ų��.
     * ���� ���, ���� prefetch rows �� 1 �� ���, 1.5 �� �����Ͽ��� 1 �̹Ƿ� 2 �� ������Ų��.
     */
    private void checkToIncreasePrefetchRows()
    {
        if (mPrefetchRows < AUTO_TUNING_PREFETCH_ROWS_MAX &&
            mPrefetchRows == mLastPrefetchRows)
        {
            mPrefetchRows++;
        }
    }

    /**
     * d < f (r = 0) �� ����̹Ƿ� prefetch rows �� ������Ų��.
     */
    private void increase()
    {
        if (mLastDecision != AUTO_TUNING_DECISION_INCREASE)
        {
            mRZeroCountOnOptimal = 0;
        }

        // allow transient local error (r = 0) in the optimal state
        if (mLastState == AutoTuningState.Optimal &&
            mRZeroCountOnOptimal < AUTO_TUNING_R_0_IN_OPTIMAL_STATE)
        {
            mRZeroCountOnOptimal++;

            if (TraceFlag.TRACE_COMPILE && TraceFlag.TRACE_ENABLED)
            {
                mAsyncLogger.log(Level.INFO, mStatement.getTraceUniqueId() + "auto-tuning : transient local error (r = 0) on optimal");
            }
        }
        else
        {
            if (mState == AutoTuningState.Stable)
            {
                // stable, so keep the last predicted prefetch rows
            }
            else
            {
                double sPredictedPrefetchRows = mLastPrefetchRows * AUTO_TUNING_INCREASE_RATIO;

                setPrefetchRows(sPredictedPrefetchRows);
                checkToIncreasePrefetchRows();
    
                // increase immediately, if d = 0 and f = 0
                if (mAppLoadTime > 0 && mPrefetchTime > 0)
                {
                    // should get more accurate feedback(error)
                    mIsTrialOneMore = true;
                }
                else
                {
                    // increase rapidly
                }
    
                mState = AutoTuningState.Tuning;
            }

            mRZeroCountOnOptimal = 0;
        }
    }

    /**
     * d > f (r > r_threshold_max) �� ����̹Ƿ� prefetch rows �� ���ҽ�Ų��.
     */
    private void decrease()
    {
        double sPredictedPrefetchRows;

        if (mLastFetchedRows == 0)
        {
            // try to reduce by r ratio, if don't have statistics
            // 
            //              d - r
            //   x_(t+1) = ------- * x_t
            //                d

            sPredictedPrefetchRows = ((double)(mAppLoadTime - mNetworkIdleTime) /
                                     (double)mAppLoadTime) * mLastPrefetchRows;

            setPrefetchRows(sPredictedPrefetchRows);
        }
        else
        {
            // calculate a intersection from systems of linear equations
            //
            //   x : prefetch rows
            //   y : elapsed time (d : application load time, f : prefetch time)
            //
            // 1. data :
            //
            //   (x1, y1_f), (x2, y2_f)
            //   (x1, y1_d), (x2, y2_d)
            //
            // 2. calculate a, c coefficients from data, respectively :
            //
            //   [ y = ax + c ]
            //
            //        y2 - y1
            //   a = --------- , c = y - ax
            //        x2 - x1
            //
            // 3. finally, calculate x-coordinate of the intersection :
            //
            //   a_d * x + c_d = a_f * x + c_f
            //
            //        c_d - c_f
            //   x = -----------                   <--- x_(t+1)
            //        a_f - a_d

            // estimated next application load time
            long sNextAppLoadTime = (long)(((double)(mAppLoadTime + mSockReadTime) /
                                          (double)mAppReadRows) * mFetchedRows);

            // �Ʒ� �������� ���� ���꿡 ���ǹǷ� �ڵ� �������� ���� coding convention �� ������ �ʴ´�.
            long x2   = mLastFetchedRows;
            long x1   = mFetchedRows;
            long y2_f = mLastPrefetchTime;
            long y1_f = mPrefetchTime;
            long y2_d = mAppLoadTime + mSockReadTime;
            long y1_d = sNextAppLoadTime;

            if ((x2 - x1) != 0)
            {
                double a_f = (double)(y2_f - y1_f) / (double)(x2 - x1);
                double c_f = y2_f - (a_f * x2);

                double a_d = (double)(y2_d - y1_d) / (double)(x2 - x1);
                double c_d = y2_d - (a_d * x2);

                if (Math.abs(a_f - a_d) > AUTO_TUNING_EPSILON)
                {
                    sPredictedPrefetchRows = (c_d - c_f) / (a_f - a_d);
                }
                else
                {
                    sPredictedPrefetchRows = 0;
                }
            }
            else
            {
                sPredictedPrefetchRows = 0;
            }

            if ((sPredictedPrefetchRows < AUTO_TUNING_PREFETCH_ROWS_MIN) ||
                (sPredictedPrefetchRows > AUTO_TUNING_PREFETCH_ROWS_MAX))
            {
                // simple reduction because of calculation exception
                sPredictedPrefetchRows = ((double)(mAppLoadTime - mNetworkIdleTime) /
                                         (double)mAppLoadTime) * mLastPrefetchRows;
            }
            else
            {
                // nothing to do
            }

            setPrefetchRows(sPredictedPrefetchRows);
        }

        mState = AutoTuningState.Tuning;
    }

    /**
     * Stable ���� �Ǵ� flooded �������� üũ�Ѵ�.
     */
    private void checkAutoTuningState()
    {
        /* check d / f ratio */
        if (mPrefetchTime >= AUTO_TUNING_DF_RATIO_MIN_PREFETCH_TIME)
        {
            if (mDFRatio < AUTO_TUNING_DF_RATIO_STABLE_THRESHOLD)
            {
                if (mDFRatioUnstableCount <= 0)
                {
                    mLastStablePrefetchRows = mLastPrefetchRows;
                    mDFRatioUnstableCount = 1;
                }
                else if (mLastStablePrefetchRows != mLastPrefetchRows)
                {
                    mDFRatioUnstableCount++;
                }
                else
                {
                    // nothing to do
                }

                if (TraceFlag.TRACE_COMPILE && TraceFlag.TRACE_ENABLED)
                {
                    mAsyncLogger.log(Level.INFO, mStatement.getTraceUniqueId() + "auto-tuning : flooded #" + mDFRatioUnstableCount + " (last stable prefetch rows = " + mLastStablePrefetchRows + ")");
                }

                if (mDFRatioUnstableCount >= AUTO_TUNING_FLOOD_THRESHOLD)
                {
                    if (TraceFlag.TRACE_COMPILE && TraceFlag.TRACE_ENABLED)
                    {
                        mAsyncLogger.log(Level.INFO, mStatement.getTraceUniqueId() + "auto-tuning : FLOODED");
                    }

                    // apply the last stable prefetch rows
                    mPrefetchRows = mLastStablePrefetchRows;

                    mState = AutoTuningState.Flooded;
                    mDFRatioUnstableCount = 0;
                }
                else
                {
                    // nothing to do
                }
            }
            else
            {
                mState = AutoTuningState.Stable;
                mLastStablePrefetchRows = mLastPrefetchRows;

                if (mDFRatioUnstableCount > 0)
                {
                    mDFRatioUnstableCount--;
                }
            }
        }
        else
        {
            mDFRatioUnstableCount = 0;
        }

        // check fetched bytes
        if (mState != AutoTuningState.Flooded)
        {
            long sFetchedBytes = mFetchResult.getFetchedBytes();
            if (sFetchedBytes > AUTO_TUNING_FETCHED_BYTES_MAX)
            {
                if (TraceFlag.TRACE_COMPILE && TraceFlag.TRACE_ENABLED)
                {
                    mAsyncLogger.log(Level.INFO, mStatement.getTraceUniqueId() + "auto-tuning : FLOODED (" + sFetchedBytes + " bytes)");
                }

                // apply the last stable prefetch rows
                mState = AutoTuningState.Flooded;
                mDFRatioUnstableCount = 0;
            }
        }
    }

    /**
     * Trace logging �� ���� auto-tuning state ���� �� ���ڷ� ��ȯ�� �ش�.
     */
    private String getStateChar()
    {
        String sStateChar = "?";

        switch (mState)
        {
            case Init:    sStateChar = "I"; break;
            case Tuning:  sStateChar = "T"; break;
            case Stable:  sStateChar = "S"; break;
            case Optimal: sStateChar = "O"; break;
            case Flooded: sStateChar = "F"; break;
            case Failed:  sStateChar = "X"; break;
            case Reset:   sStateChar = "R"; break;
            case End:     sStateChar = "E"; break;

            default:
                break;
        }

        return sStateChar;
    }

    /**
     * Auto-tuning �� �����Ѵ�.
     */
    void beginAutoTuning() throws Throwable
    {
        try
        {
            if (TraceFlag.TRACE_COMPILE && TraceFlag.TRACE_ENABLED)
            {
                mAsyncLogger.log(Level.INFO, mStatement.getTraceUniqueId() + "auto-tuning : begin");
            }

            // initialize, if auto-tuning was failed
            if (mState == AutoTuningState.Failed || mLastState == AutoTuningState.Failed)
            {
                initialize();
            }

            traceParameters();
        }
        catch (Throwable e)
        {
            if (TraceFlag.TRACE_COMPILE && TraceFlag.TRACE_ENABLED)
            {
                mAsyncLogger.log(Level.WARNING, mStatement.getTraceUniqueId(), e);
            }

            mState = AutoTuningState.Failed;

            throw e;
        }
    }

    /**
     * Socket receive buffer �κ��� read (or receive) ������ ȣ���Ͽ� d time �� �����Ѵ�.
     */
    void doAutoTuningBeforeReceive()
    {
        mEndTimeToMeasure = System.currentTimeMillis();

        mAppLoadTime = mEndTimeToMeasure - mBeginTimeToMeasure;

        if (mLastFetchedRows > 0)
        {
            mAppReadRows = mLastFetchedRows;
        }
        else
        {
            // first auto-tuning
        }
    }

    /**
     * Auto-tuning �� �����Ͽ� predicted prefetch rows �� ��ȯ�Ѵ�.
     */
    int doAutoTuning() throws Throwable
    {
        try
        {
            // 1. update, if last auto-tuning prefetch rows is 0
            if (mLastPrefetchRows == 0)
            {
                mPrefetchRows     = mFetchedRows;
                mLastPrefetchRows = mFetchedRows;
            }

            mFetchedRows = mFetchResult.getFetchedRowCount();
            mDecision = AUTO_TUNING_DECISION_UNKNOWN;

            // 2. call state function for each auto-tuning state
            callStateFunc();

            // 3. prefetch rows limit
            limitPrefetchRows();

            // 4. adjust socket receive buffer that should ensure
            if (mProperties.getSockRcvBufBlockRatio() == 0)
            {
                adjustAutoTunedSockRcvBuf();
            }

            // 5. trace logging
            if (TraceFlag.TRACE_COMPILE && TraceFlag.TRACE_ENABLED)
            {
                String sStateChar = getStateChar();

                String sTraceMsg = String.format("auto-tuning : d = %3d ms (%6d rows), " + 
                                                 "r = %2d ms, " +
                                                 "k = %2d ms, " +
                                                 "f = %3d ms (%6d rows), " +
                                                 "d/f = %f, " +
                                                 "predicted = %6d (%s), " + 
                                                 "state = %s",
                                                 mAppLoadTime, mAppReadRows,
                                                 mNetworkIdleTime,
                                                 mSockReadTime,
                                                 mPrefetchTime, mFetchedRows,
                                                 mDFRatio,
                                                 mPrefetchRows, mDecision,
                                                 sStateChar);

                mAsyncLogger.log(Level.INFO, mStatement.getTraceUniqueId() + sTraceMsg);
            }

            // 6. save statistics
            mLastPrefetchRows    = mPrefetchRows;
            mLastFetchedRows     = mFetchedRows;
            mLastAppLoadTime     = mAppLoadTime;
            mLastPrefetchTime    = mPrefetchTime;
            mLastNetworkIdleTime = mNetworkIdleTime;
            mLastSockReadTime    = mSockReadTime;
            mLastDecision        = mDecision;
            mLastState           = mState;

            // 7. begin to measure new elapsed time
            mBeginTimeToMeasure = System.currentTimeMillis();

            return mPrefetchRows;
        }
        catch (Throwable e)
        {
            if (TraceFlag.TRACE_COMPILE && TraceFlag.TRACE_ENABLED)
            {
                mAsyncLogger.log(Level.WARNING, mStatement.getTraceUniqueId() + "auto-tuning : error and stopped", e);
            }

            mState = AutoTuningState.Failed;

            throw e;
        }
    }

    /**
     * ���¿� ���� �Լ��� ȣ���Ѵ�.
     */
    private void callStateFunc() throws Throwable
    {
        switch (mState)
        {
            case Init:
                doAutoTuningStateInit();
                break;

            case End:
                doAutoTuningStateEnd();
                break;

            case Reset:
                doAutoTuningStateReset();
                break;

            case Tuning:
            case Stable:
            case Optimal:
                doAutoTuningStateTuning();
                break;

            case Flooded:
                doAutoTuningStateFlooded();
                break;

             default:
                 throw new java.lang.AssertionError();
        }
    }

    /**
     * Properties ������ ���� prefetch rows �� �����Ѵ�.
     */
    private void limitPrefetchRows()
    {
        if (mPrefetchRows < mProperties.getFetchAutoTuningMin())
        {
            mPrefetchRows = mProperties.getFetchAutoTuningMin();
        }

        if (mPrefetchRows > mProperties.getFetchAutoTuningMax())
        {
            mPrefetchRows = mProperties.getFetchAutoTuningMax();
        }

        if (mPrefetchRows > AltibaseStatement.getMaxFetchSize())
        {
            mPrefetchRows = AltibaseStatement.getMaxFetchSize();
        }
    }

    /**
     * Auto-tuning �� first prediction ���μ�,
     * 'fetch_auto_tuning_init' �Ǵ� 'fetch_enough' ������Ƽ ������ �����Ѵ�.
     */
    private void doAutoTuningStateInit() throws Throwable
    {
        int sAutoTuningInit = mProperties.getFetchAutoTuningInit();
        if (sAutoTuningInit == 0)
        {
            // 'fetch_auto_tuning_init' = 0 or not set
            mPrefetchRows = mResultSet.getFetchSize();
        }
        else
        {
            // 'fetch_auto_tuning_init' > 0
            mPrefetchRows = sAutoTuningInit;
        }

        mState = AutoTuningState.Tuning;
    }

    /**
     * Network idle time �� �����Ѵ�.
     */
    private void measureNetworkIdleTime()
    {
        // 2. get r time
        mNetworkIdleTime = mJniExt.getTcpiLastDataRecv(mSocketFD);

        // 3. measure f time
        mEndTimeToMeasure = System.currentTimeMillis();
        mTotalElapsedTime = mEndTimeToMeasure - mBeginTimeToMeasure;

        mSockReadTime = mTotalElapsedTime - mAppLoadTime;
        mPrefetchTime = mTotalElapsedTime - mNetworkIdleTime;

        if (mPrefetchTime > 0)
        {
            mDFRatio = (double)mAppLoadTime / (double)mPrefetchTime;
        }
        else
        {
            mPrefetchTime = 0;
            mDFRatio = 0;
        }
    }

    /**
     * Tuning ���·μ�, network idle time �� ������� ������ prefetch rows �� ã�´�.
     */
    private void doAutoTuningStateTuning() throws Throwable
    {
        // 1. change tuning state first, even through the state is stable, optimal state
        mState = AutoTuningState.Tuning;

        // 2. measure network idle time
        measureNetworkIdleTime();

        // 3. decide auto-tuning direction through the statistics
        mDecision = compareAppLoadTimeAndPrefetchTime();

        // 4. apply one more, if needs
        if (mIsTrialOneMore)
        {
            if (TraceFlag.TRACE_COMPILE && TraceFlag.TRACE_ENABLED)
            {
                mAsyncLogger.log(Level.INFO, mStatement.getTraceUniqueId() + "auto-tuning : keep");
            }

            mIsTrialOneMore = false;
            return;
        }

        // 5. check state
        checkAutoTuningState();

        // 6. apply the last stable prefetch rows, if flooded
        if (mState == AutoTuningState.Flooded)
        {
            mPrefetchRows = mLastStablePrefetchRows;
            return;
        }

        // 7. auto-tuning by decision
        if (mDecision == AUTO_TUNING_DECISION_INCREASE)
        {
            // 7-1. d < f : should increase (r = 0)
            increase();
        }
        else if (mDecision == AUTO_TUNING_DECISION_DECREASE)
        {
            // 7-2. d > f : should decrease (r > r_threshold_max)
            decrease();
        }
        else
        {
            // 7-3. d = f : optimized at this time (r_threshold_min <= r <= r_threshold_max)
            mState = AutoTuningState.Stable;
        }

        // 8. check optimal state
        if (mLastPrefetchRows == mPrefetchRows)
        {
            mConsecutiveCount++;

            if (mConsecutiveCount >= AUTO_TUNING_OPTIMAL_THRESHOLD)
            {
                mState = AutoTuningState.Optimal;
            }
        }
        else
        {
            // stable or tuning state
            mConsecutiveCount = 0;
        }
    }

    /**
     * Flooded ���·μ�, auto-tuning �� �����ϰ� last stable prefetch rows �� �����Ѵ�.
     */
    private void doAutoTuningStateFlooded() throws Throwable
    {
        // 1. measure network idle time
        measureNetworkIdleTime();

        // 2. decide auto-tuning direction through the statistics
        mDecision = compareAppLoadTimeAndPrefetchTime();

        // 3. apply the last stable prefetch rows
        mPrefetchRows = mLastStablePrefetchRows;

        mConsecutiveCount = 0;
    }

    /**
     * Reset ���·μ�, Auto-tuning ��� ������ �ʱ�ȭ �Ѵ�.
     * �񵿱� ��û ���� �̸� read �� ��� auto-tuning ��� ������ ����Ȯ�ϱ� ������ �ʱ�ȭ �Ѵ�.
     */
    private void doAutoTuningStateReset()
    {
        purgeStat();

        if (mLastState != AutoTuningState.Optimal)
        {
            mConsecutiveCount    = 0;
            mRZeroCountOnOptimal = 0;
        }

        if (mLastState != AutoTuningState.Flooded)
        {
            mDFRatioUnstableCount = 0;
        }

        // keep the last predicted prefetch rows on next time
        mIsTrialOneMore = true;

        if (mLastState != AutoTuningState.Optimal &&
            mLastState != AutoTuningState.Flooded)
        {
            mState = AutoTuningState.Tuning;
        }
    }

    /**
     * Auto-tuning ���� �� �ٽ� ������ ���,
     * ������ auto-tuned prefetch rows �� �����ϰ� ���/���� ������ �ʱ�ȭ �Ѵ�.
     */
    private void doAutoTuningStateEnd()
    {
        purgeStat();

        mIsTrialOneMore       = false;
        mConsecutiveCount     = 0;
        mRZeroCountOnOptimal  = 0;
        mDFRatioUnstableCount = 0;

        // purge the last stable prefetch rows
        mLastStablePrefetchRows = 0;

        mState = AutoTuningState.Tuning;
    }

    /**
     * �ٸ� �������ݿ� ���� �̹� read �� ��� ��� ������ �������� �����Ƿ� reset �Ѵ�.
     */
    void skipAutoTuning()
    {
        // don't need to reset after auto-tuning end
        if (mState != AutoTuningState.End)
        {
            if (TraceFlag.TRACE_COMPILE && TraceFlag.TRACE_ENABLED)
            {
                mAsyncLogger.log(Level.INFO, mStatement.getTraceUniqueId() + "auto-tuning : skip");
            }
    
            /* will be reset auto-tuning stat. */
            mState = AutoTuningState.Reset;
        }
    }

    /**
     * Server cursor close �� ��� auto-tuning �� �����Ѵ�.
     */
    void endAutoTuning() throws SQLException
    {
        if (mState == AutoTuningState.End)
        {
            // already ended
            return;
        }

        if (mIsAdjustedSockRcvBuf)
        {
            int sSockRcvBufBlockRatio = mProperties.getSockRcvBufBlockRatio();

            try
            {
                mCmChannel.setSockRcvBufBlockRatio(sSockRcvBufBlockRatio);

                if (TraceFlag.TRACE_COMPILE && TraceFlag.TRACE_ENABLED)
                {
                    mAsyncLogger.log(Level.INFO, mStatement.getTraceUniqueId() + "auto-tuning : restored for the adjusted socket receive buffer");
                }
            }
            catch (IOException e)
            {
                Error.throwCommunicationErrorException(e);
            }

            mLastAdjustedSockRcvBufSize = 0;
            mIsAdjustedSockRcvBuf = false;
        }
        else
        {
            // not changed
        }

        mLastState = mState;
        mState = AutoTuningState.End;

        if (TraceFlag.TRACE_COMPILE && TraceFlag.TRACE_ENABLED)
        {
            mAsyncLogger.log(Level.INFO, mStatement.getTraceUniqueId() + "auto-tuning : end");
        }
    }
}
