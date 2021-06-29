/** 
 *  Copyright (c) 1999~2017, Altibase Corp. and/or its affiliates. All rights reserved.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU Affero General Public License, version 3,
 *  as published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU Affero General Public License for more details.
 *
 *  You should have received a copy of the GNU Affero General Public License
 *  along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
 
package com.altibase.altilinker.adlp.op;

import java.nio.BufferUnderflowException;
import java.nio.ByteBuffer;

import com.altibase.altilinker.adlp.*;
import com.altibase.altilinker.adlp.type.*;

public class BindRemoteVariableOp extends RequestOperation
{
    public long   mRemoteStatementId  = 0;
    public int    mBindVariableIndex  = 0;
    public int    mBindVariableType   = AltibaseSQLType.SQL_VARCHAR;
    public String mBindVariableString = null;
    
    public BindRemoteVariableOp()
    {
        super(OpId.BindRemoteVariable, true, true);
    }
    
    public ResultOperation newResultOperation()
    {
        return new BindRemoteVariableResultOp();
    }

    protected boolean readOperation(CommonHeader aCommonHeader,
                                    ByteBuffer   aOpPayload)
    {
        // common header
        setCommonHeader(aCommonHeader);
        
        if (aCommonHeader.getDataLength() > 0)
        {
            try
            {
                mRemoteStatementId = readLong(aOpPayload);
                mBindVariableIndex = readInt (aOpPayload);
                mBindVariableType  = readInt (aOpPayload);
    
                if (mBindVariableType == AltibaseSQLType.SQL_VARCHAR)
                    mBindVariableString = readVariableString(aOpPayload, true);
            }
            catch (BufferUnderflowException e)
            {
                return false;
            }
        }
        
        return true;
    }

    protected boolean validate()
    {
        if (getSessionId() == 0)
        {
            return false;
        }
        
        if (mRemoteStatementId == 0)
        {
            return false;
        }
        
        if (mBindVariableIndex < 0)
        {
            return false;
        }
        
        if (mBindVariableType != AltibaseSQLType.SQL_VARCHAR)
        {
            return false;
        }
        
        if (mBindVariableString == null)
        {
            return false;
        }
        
        return true;
    }
}
