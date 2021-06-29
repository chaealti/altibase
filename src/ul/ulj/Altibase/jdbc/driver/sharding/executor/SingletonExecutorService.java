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

package Altibase.jdbc.driver.sharding.executor;

import Altibase.jdbc.driver.util.RuntimeEnvironmentVariables;

import java.util.concurrent.*;

import static Altibase.jdbc.driver.sharding.util.ShardingTraceLogger.shard_log;

public class SingletonExecutorService
{
    private static int CORE_POOL_SIZE    = RuntimeEnvironmentVariables.getIntVariable("SHARD_JDBC_POOL_CORE_SIZE",
                                                                                      RuntimeEnvironmentVariables.getAvailableProcessors());
    private static int MAXIMUM_POOL_SIZE = RuntimeEnvironmentVariables.getIntVariable("SHARD_JDBC_POOL_MAX_SIZE", 128);
    private static int IDLE_TIMEOUT_MIN  = RuntimeEnvironmentVariables.getIntVariable("SHARD_JDBC_IDLE_TIMEOUT", 10);

    static
    {
        if (MAXIMUM_POOL_SIZE < CORE_POOL_SIZE)
        {
            MAXIMUM_POOL_SIZE = CORE_POOL_SIZE;
        }

        shard_log("(SINGLETON_EXECUTOR_SERVICE) create thread pool. core size : {0}, max size : {1}, idle timeout : {2} ",
                  new Object[] { CORE_POOL_SIZE, MAXIMUM_POOL_SIZE, IDLE_TIMEOUT_MIN } );
    }
    // SynchronousQueue�� size�� 0�̱� ������ core pool�� �� �� ��� max size���� �þ �� reject�� �ϰ� �ȴ�.
    private static ExecutorService mInstance =
            new ThreadPoolExecutor(
                    CORE_POOL_SIZE,
                    MAXIMUM_POOL_SIZE,
                    IDLE_TIMEOUT_MIN * 60,
                    TimeUnit.SECONDS, new SynchronousQueue<Runnable>(),
                    new ThreadFactory()
                    {
                        public Thread newThread(Runnable aRunnable)
                        {
                            Thread sThread = Executors.defaultThreadFactory().newThread(aRunnable);
                            sThread.setDaemon(true);
                            return sThread;
                        }
                    },
                    // max���� ���� ������ reject�ϴ� ��� caller �����忡 task�� ���ӽ�Ų��.
                    new ThreadPoolExecutor.CallerRunsPolicy()
            );

    private SingletonExecutorService()
    {
    }

    static ExecutorService getExecutorService()
    {
        return mInstance;
    }
}
