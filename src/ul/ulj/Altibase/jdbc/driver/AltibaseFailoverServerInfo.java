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

import Altibase.jdbc.driver.util.StringUtils;

public class AltibaseFailoverServerInfo
{
    private final String mServer;
    private final String mDbName; // BUG-43219 add dbname property to failover
    private final int    mPortNo;
    private long         mFailStartTime;

    public AltibaseFailoverServerInfo(String aServer, int aPortNo, String aDbName)
    {
        mServer = aServer;
        mPortNo = aPortNo;
        mDbName = aDbName;
    }

    /**
     * ������ ������ host ���� ��´�.
     *
     * @return host
     */
    public String getServer()
    {
        return mServer;
    }

    /**
     * ������ ������ port number�� ��´�.
     *
     * @return port number
     */
    public int getPort()
    {
        return mPortNo;
    }

    // #region ���� ��� ����, ���� ���� �ð� ����

    /**
     * ���� ���ӿ� ������ �ð��� ��´�.
     *
     * @return ���� ���ӿ� ������ �ð�
     */
    public long getFailStartTime()
    {
        return mFailStartTime;

    }

    /**
     * ������ ������ DB Name�� ��´�.
     *
     * @return DB Name
     */

    public String getDatabase()
    {
        return mDbName;
    }

    public void setFailStartTime()
    {
        mFailStartTime = (System.currentTimeMillis() / 1000 + 1);
    }

    // #endregion

    // #region Object �������̵�

    /**
     * alternate servers string ���˿� �´� ���ڿ� ���� ��´�.
     */
    public String toString()
    {
        /* alternateservers ���˿� ���� */
        String sDbName = (StringUtils.isEmpty(mDbName)) ? "" : "/" + mDbName;
        return mServer + ":" + mPortNo + sDbName;
    }

    public int hashCode()
    {
        return toString().hashCode();
    }

    /**
     * Server�� Port, Database Name�� ������ Ȯ���Ѵ�.
     */
    public boolean equals(Object aObj)
    {
        if (this == aObj)
        {
            return true;
        }
        if (aObj == null)
        {
            return false;
        }
        if (this.getClass() != aObj.getClass())
        {
            return false;
        }

        AltibaseFailoverServerInfo sServer = (AltibaseFailoverServerInfo) aObj;

        if (mServer == null)
        {
            if (sServer.getServer() != null)
            {
                return false;
            }
        }
        else /* mServer != null */
        {
            if (mServer.equals(sServer.getServer()) == false)
            {
                return false;
            }
        }

        if (mDbName != sServer.getDatabase())
        {
            return false;
        }

        if (mPortNo != sServer.getPort())
        {
            return false;
        }

        return true;
    }

    // #endregion
}
