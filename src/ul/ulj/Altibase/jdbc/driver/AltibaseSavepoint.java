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

import java.sql.Connection;
import java.sql.SQLException;
import java.sql.Savepoint;
import java.util.regex.Pattern;

import Altibase.jdbc.driver.ex.Error;
import Altibase.jdbc.driver.ex.ErrorDef;

// BUGBUG Savepoint�� �̸����θ� �����Ǿ�� �Ѵ�. 
// commit�ǰų� rollback�Ǵ��� statmenet�� �����ϰ� �־���Ѵ�. 
// �ֳĸ� �Ȱ��� �̸����� ��ü�� �����Ǹ� ������ �����ϴ� ���� �̸��� ��ü�� �ٽ� ���۰����ؾ� �ϱ� �����̴�.

class AltibaseSavepoint implements Savepoint
{
    public static final int      SAVEPOINT_TYPE_NAMED            = 1;
    public static final int      SAVEPOINT_TYPE_UNNAMED          = 2;

    private static final Pattern SAVEPOINT_NAME_PATTERN          = Pattern.compile("^[a-zA-Z_][\\w]*$");
    private static final int     INVALID_SAVEPOINT_ID            = 0;
    private static final String  GENERATED_NAME_PREFIX           = "SP_";
    private static final String  INTERNAL_SQL_PREFIX_SAVEPOINT   = "SAVEPOINT \"";
    private static final String  INTERNAL_SQL_PREFIX_ROLLBACK_TO = "ROLLBACK TO SAVEPOINT \"";
    private static final String  INTERNAL_SQL_POSTFIX            = "\"";

    private AltibaseConnection   mConnection;
    private final int            mType;
    private final String         mName;
    private final int            mId;

    AltibaseSavepoint(AltibaseConnection aConn) throws SQLException
    {
        this(aConn, null);
    }

    AltibaseSavepoint(AltibaseConnection aConn, String aName) throws SQLException
    {
        mConnection = aConn;
        if (aName == null)
        {
            mId = mConnection.newSavepointId();
            mName = GENERATED_NAME_PREFIX + Integer.toHexString(mId);
            mType = SAVEPOINT_TYPE_UNNAMED;
        }
        else
        {
            if (!isValidSavepointName(aName)) 
            {    
                Error.throwSQLException(ErrorDef.INVALID_SAVEPOINT_NAME, aName);
            }
            mId = INVALID_SAVEPOINT_ID;
            mName = aName;
            mType = SAVEPOINT_TYPE_NAMED;
        }
    }

    private boolean isValidSavepointName(String aName)
    {
        return SAVEPOINT_NAME_PATTERN.matcher(aName).matches();
    }

    Connection getConnection() throws SQLException
    {
        return mConnection;
    }

    Savepoint setSavepoint() throws SQLException
    {
        AltibaseStatement sStmt = (AltibaseStatement)mConnection.createStatement();
        sStmt.executeUpdate(INTERNAL_SQL_PREFIX_SAVEPOINT + mName + INTERNAL_SQL_POSTFIX);
        sStmt.close();
        return this;
    }

    void rollback() throws SQLException
    {
        AltibaseStatement sStmt = (AltibaseStatement)mConnection.createStatement();
        sStmt.executeUpdate(INTERNAL_SQL_PREFIX_ROLLBACK_TO + mName + INTERNAL_SQL_POSTFIX);
        sStmt.close();
    }

    void releaseSavepoint() throws SQLException
    {
        if (mId != INVALID_SAVEPOINT_ID)
        {
            mConnection.releaseSavepointId(mId);
        }
        mConnection = null;
    }

    int getType()
    {
        return mType;
    }

    public int getSavepointId() throws SQLException
    {
        if (mType == SAVEPOINT_TYPE_NAMED)
        {
            Error.throwSQLException(ErrorDef.NOT_SUPPORTED_OPERATION_ON_NAMED_SAVEPOINT);
        }
        return mId;
    }

    public String getSavepointName() throws SQLException
    {
        if (mType == SAVEPOINT_TYPE_UNNAMED) 
        {
            Error.throwSQLException(ErrorDef.NOT_SUPPORTED_OPERATION_ON_UNNAMED_SAVEPOINT);
        }
        return mName;
    }
}
