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
 */

package Altibase.jdbc.driver;

import java.sql.*;

import Altibase.jdbc.driver.ex.Error;
import Altibase.jdbc.driver.ex.ErrorDef;

/**
 * java.sql.Connection �������̽� �� �������� �ʴ� �������̽��� �����Ѵ�.
 */
public abstract class AbstractConnection extends WrapperAdapter implements Connection
{
    protected boolean mIsClosed = true;

    @Override
    public Clob createClob() throws SQLException
    {
        throw Error.createSQLFeatureNotSupportedException();
    }

    @Override
    public Blob createBlob() throws SQLException
    {
        throw Error.createSQLFeatureNotSupportedException();
    }

    @Override
    public void setSchema(String aSchema) throws SQLException
    {
        throwErrorForClosed();
        // PROJ-2707 ���忡 ������ ��Ű���� �������� ���� ��� �׳� �����϶�� �Ǿ� �ִ�.
    }

    @Override
    public String getSchema() throws SQLException
    {
        throwErrorForClosed();
        return null;
    }

    @Override
    public NClob createNClob() throws SQLException
    {
        throw Error.createSQLFeatureNotSupportedException();
    }

    @Override
    public SQLXML createSQLXML() throws SQLException
    {
        throw Error.createSQLFeatureNotSupportedException();
    }

    @Override
    public Array createArrayOf(String aTypeName, Object[] aElements) throws SQLException
    {
        throw Error.createSQLFeatureNotSupportedException();
    }

    @Override
    public Struct createStruct(String aTypeName, Object[] aAttributes) throws SQLException
    {
        throw Error.createSQLFeatureNotSupportedException();
    }

    void throwErrorForClosed() throws SQLException
    {
        if (mIsClosed)
        {
            Error.throwSQLException(ErrorDef.CLOSED_CONNECTION);
        }
    }
}
