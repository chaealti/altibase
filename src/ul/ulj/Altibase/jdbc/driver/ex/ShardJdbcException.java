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

package Altibase.jdbc.driver.ex;

import java.sql.SQLException;

/**
 * ���� ��� ��Ȳ���� �߻��ϴ� Shard jdbc exception <br>
 * ���������� node name�� ������ ������ ���Ŀ�ؼǿ��� �߻��� �����޼����� ���ؼ��� <br>
 * node name, ip address, port�� �߰��ؼ� �����ش�.
 */
public class ShardJdbcException extends SQLException
{
    private String mNodeName;

    public ShardJdbcException(String aErrorMsg, int aErrorCode, String aNodeName, String aIpAddress,
                              int aPortNo)
    {
        super(makeString(aErrorMsg, aNodeName, aIpAddress, aPortNo),
              ErrorDef.getErrorState(aErrorCode), aErrorCode);
        mNodeName = aNodeName;
    }

    private static String makeString(String aErrorMsg, String aNodeName, String aIpAddress,
                                     int aPortNo)
    {
        StringBuilder sSb = new StringBuilder();
        // BUG-46790 meta���� �߻��� failover exception�� ��쿡�� node ������ ǥ������ �ʴ´�.
        if (aNodeName != null)
        {
            sSb.append("[").append(aNodeName);
            sSb.append(",").append(aIpAddress).append(":").append(aPortNo);
            sSb.append("] ");
        }
        sSb.append(aErrorMsg);

        return sSb.toString();
    }

    public String getNodeName()
    {
        return mNodeName;
    }

}
