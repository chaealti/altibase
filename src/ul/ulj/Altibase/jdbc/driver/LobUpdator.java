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
import java.io.InputStream;
import java.io.Reader;
import java.sql.Blob;
import java.sql.Clob;
import java.sql.SQLException;
import java.util.LinkedList;

import Altibase.jdbc.driver.datatype.BlobLocatorColumn;
import Altibase.jdbc.driver.datatype.ClobLocatorColumn;
import Altibase.jdbc.driver.datatype.Column;
import Altibase.jdbc.driver.datatype.LobLocatorColumn;
import Altibase.jdbc.driver.datatype.DynamicArrayRowHandle;
import Altibase.jdbc.driver.ex.Error;
import Altibase.jdbc.driver.ex.ErrorDef;

class LobUpdator
{
    // LobUpdator�� add method�� Update �۾��� ���� �⺻�� ������ ����Ѵ�.
    // LobUpdator�� update method���� �� data�� ������� protocol�� �����Ͽ� ������ �����Ѵ�.
    private class LobParams
    {
        Object mTargetLob;
        Object mSource;
        long mLength;

        LobParams(Object aTargetLobLocator, Object aSource, long aLength)
        {
            mTargetLob = aTargetLobLocator;
            mSource = aSource;
            mLength = aLength;
        }
    }

    private LinkedList  mUpdatees   = new LinkedList();
    private AltibasePreparedStatement mPreparedStatement;
    
    LobUpdator(AltibasePreparedStatement aPreparedStmt)
    {
        this.mPreparedStatement = aPreparedStmt;
    }
    
    public void addLobColumn(LobLocatorColumn aTargetLobLocator, Object aSourceValue, long aLength)
    {
        mUpdatees.add(new LobParams(aTargetLobLocator, aSourceValue, aLength));
    }
    
    public void addClobColumn(LobLocatorColumn aTargetLobLocator, Object aSourceValue) throws SQLException
    {
        if(aSourceValue instanceof char[])
        {
            mUpdatees.add(new LobParams(aTargetLobLocator, aSourceValue, ((char[])aSourceValue).length));
        }
        else if(aSourceValue instanceof String)
        {
            mUpdatees.add(new LobParams(aTargetLobLocator, aSourceValue, ((String) aSourceValue).length()));
        }
        else if(aSourceValue instanceof Clob || aSourceValue == null)
        {
            // Clob ��ü�� ���̸� ��ü���� ȹ���� �� �����Ƿ� 0���� �����Ѵ�.
            // BUG-38681 value�� null�϶��� mUpdatees�� �־��ش�.
            mUpdatees.add(new LobParams(aTargetLobLocator, aSourceValue, 0));
        }
        else
        {
            Error.throwSQLException(ErrorDef.INVALID_TYPE_CONVERSION, aSourceValue.getClass().getName(), "CLOB");
        }
    }
    
    public void addBlobColumn(LobLocatorColumn aTargetLobLocator, Object aSourceValue) throws SQLException
    {
        if(aSourceValue instanceof byte[])
        {
            mUpdatees.add(new LobParams(aTargetLobLocator, aSourceValue, ((byte[])aSourceValue).length));
        }
        else if(aSourceValue instanceof Blob || aSourceValue == null)
        {
            // Blob ��ü�� ���̸� ��ü���� ȹ���� �� �����Ƿ� 0���� �����Ѵ�.
            // BUG-38681 value�� null�϶��� mUpdatees�� �־��ش�.
            mUpdatees.add(new LobParams(aTargetLobLocator, aSourceValue, 0));
        }
        else
        {
            Error.throwSQLException(ErrorDef.INVALID_TYPE_CONVERSION, aSourceValue.getClass().getName(), "BLOB");
        }
    }
    
    void updateLobColumns() throws SQLException
    {
        int sLobColumnCount = 0;
        int sExecutionCount = 0;
        
        if (mPreparedStatement.mBatchAdded)
        {
            for (int i = 0; i < mPreparedStatement.mBindColumns.size(); i++)
            {
                if ((Column)mPreparedStatement.mBindColumns.get(i) instanceof LobLocatorColumn)
                {
                    sLobColumnCount++;
                }
            }
        }
        
        while (!mUpdatees.isEmpty())
        {
            LobParams sParams = (LobParams)mUpdatees.removeFirst();
         
            // addBatch�� ���� BatchRowHandle�� dynamic array�� �����ʹ� �̹� ����Ǿ� �ִ�. 
            // ���������� executeBatch()�� ������ ��� �� ��ƾ�� �����Ѵ�.
            if(mPreparedStatement.mBatchAdded)
            {
                // �� row�� LOB Column ������ŭ update�� ������ �� ���� row�� �Ѿ�� ���� rowhandle�� ���� row�� �ѱ��.
                if(sExecutionCount % sLobColumnCount == 0)
                {
                	((DynamicArrayRowHandle)mPreparedStatement.mBatchRowHandle).next();
                }
                
                sExecutionCount++;
            }
            
            // BUG-26327, BUG-37418
            if(((LobLocatorColumn)sParams.mTargetLob).isNullLocator())
            {
                continue;
            }
            
            // Target�� data�� �����Ϸ��� ��� Table
            // Source�� �����Ϸ��� data�� �ǹ��Ѵ�.
            // Column Type�� ���� ������ �޸��Ѵ�.
            if (sParams.mTargetLob instanceof BlobLocatorColumn)
            {
                AltibaseBlob sBlob = (AltibaseBlob)((BlobLocatorColumn)sParams.mTargetLob).getBlob();
                InputStream sSource = null;
                long sLobLength = 0;
                
                if (sParams.mSource instanceof Blob)
                {
                    // Blob ��ü�� BinaryStream���� ��ȯ�Ͽ� ó���Ѵ�.
                    sSource = ((Blob)sParams.mSource).getBinaryStream();
                    sLobLength = ((Blob)sParams.mSource).length();
                    updateBlob(sBlob, sSource, sLobLength);
                }
                else if(sParams.mSource instanceof InputStream)
                {
                    sSource = (InputStream)sParams.mSource;
                    sLobLength = sParams.mLength;
                    updateBlob(sBlob, sSource, sLobLength);
                }
                else if(sParams.mSource instanceof byte[])
                {
                    byte[] sSourceAsByteArr = (byte[])sParams.mSource;
                    sLobLength = sSourceAsByteArr.length;
                    updateBlob(sBlob, sSourceAsByteArr);
                }
                else if (sParams.mSource == null)
                {
                    // BUG-38681 CmProtocol.preparedExecuteBatch�� ���� ȣ���ϸ鼭 �ش� ���� null�� ������Ʈ �Ǳ� ������
                    // ���⼭�� nullüũ�� �ϰ� �Ѿ��.
                }
                else
                {
                    Error.throwSQLException(ErrorDef.INVALID_DATA_CONVERSION, String.valueOf(sParams.mSource), "BLOB");
                }
                
                // �ش� Column�� ������ �ϳ��� �����ϸ� �ش� locator�� �����Ѵ�.
                // INSERT�� ��� ������ ���� LOCATOR�� ȹ���� ����� �����Ƿ� �����ص� ������ ����.
                sBlob.free();
            }
            else if (sParams.mTargetLob instanceof ClobLocatorColumn)
            {
                AltibaseClob sClob = (AltibaseClob)((ClobLocatorColumn)sParams.mTargetLob).getClob();
                
                if (sParams.mSource instanceof Clob)
                {
                    updateClob(sClob, ((Clob)sParams.mSource).getCharacterStream(), 0);
                }
                else if(sParams.mSource instanceof Reader)
                {
                    updateClob(sClob, (Reader)sParams.mSource, sParams.mLength);
                }
                else if(sParams.mSource instanceof InputStream)
                {
                    InputStream sSource = (InputStream)sParams.mSource;
                    // Byte Length
                    updateClob(sClob, sSource, sParams.mLength);
                }
                else if(sParams.mSource instanceof char[])
                {
                    updateClob(sClob, (char[])sParams.mSource);
                }
                else if(sParams.mSource instanceof String)
                {
                    updateClob(sClob, ((String)sParams.mSource).toCharArray());
                }
                else if (sParams.mSource == null)
                {
                    // BUG-38681 CmProtocol.preparedExecuteBatch�� ���� ȣ���ϸ鼭 �ش� ���� null�� ������Ʈ �Ǳ� ������
                    // ���⼭�� nullüũ�� �ϰ� �Ѿ��.
                }
                else
                {
                    Error.throwSQLException(ErrorDef.INVALID_DATA_CONVERSION, String.valueOf(sParams.mSource), "CLOB");
                }
                
                sClob.free();
            }
        }
    }

    private void updateClob(AltibaseClob aTarget, char[] aSource) throws SQLException
    {
        aTarget.open(mPreparedStatement.mConnection.channel());
        ClobWriter sOut = (ClobWriter) aTarget.setCharacterStream(1);

        try
        {
            sOut.write(aSource);
        } 
        catch (IOException sException)
        {
            Error.throwCommunicationErrorException(sException);
        }
    }

    private void updateBlob(AltibaseBlob aTarget, byte[] sSourceAsByteArr) throws SQLException
    {
        aTarget.open(mPreparedStatement.mConnection.channel());
        aTarget.setBytes(1, sSourceAsByteArr);
    }

    private void updateLob(BlobOutputStream aTarget, InputStream aSource, long aLength) throws SQLException
    {
        try
        {
            // ���� �� Connection���� Source�� Target�� �־����ٸ�
            // ���� channel�� ����ϱ� ������ channel�� byte buffer�� ������ �����Ҷ����� �ݵ�� �� ����� �Ѵ�.
            // ������ ������ buffer�� �ξ� �����͸� �׻� buffer�� ������ �ξ����� �̴� �������� �ҷ������� ������ ���Ͻ�Ų��.
            // Source�� Target�� �ٸ� connection�� ����ϰ� �ִٸ� buffer copy ����� �����̹Ƿ�
            // ���� connection�� ����ϴ� ��쿡�� ������ buffer�� �ϰ� �����ϵ��� ó���Ѵ�.
            // ��, �ٸ� Connection�� ���� ���������� ������ ũ�Ⱑ (�����δ� ��ü �����Ͱ� �� Ŭ���� ��û�ϴ� ���� ���� �� �ִ�.)
            // �ſ� ���� ��� ������ buffer�� �̿��ϴ� ���� ��ŷ��� ���� �� �־� ȿ�����̹Ƿ� �̶��� ������ buffer�� Ȱ���Ѵ�.
            
            if(aSource instanceof ConnectionSharable)
            {
                ConnectionSharable sConnSharable = (ConnectionSharable)aSource;
                if(sConnSharable.isSameConnectionWith(mPreparedStatement.mConnection.channel()))
                {
                    ((ConnectionSharable)aSource).setCopyMode();
                    ((ConnectionSharable)aSource).readyToCopy();
                    aTarget.setCopyMode();
                }
            }
            
            aTarget.write(aSource, aLength);
        }
        catch (IOException sException)
        {
            Error.throwCommunicationErrorException(sException);
        }
    }
    
    private void updateBlob(AltibaseBlob aTarget, InputStream aSource, long aLength) throws SQLException
    {
        // �����Ϸ��� Column ��ü (aTarget)�� channel ���۷����� �����Ѵ�.
        aTarget.open(mPreparedStatement.mConnection.channel());
        // OutputStream�� ȹ���Ͽ� update�۾��� ������
        BlobOutputStream sOut = (BlobOutputStream)aTarget.setBinaryStream(1);
        updateLob(sOut, aSource, aLength);
    }

    private void updateClob(AltibaseClob aTarget, InputStream aSource, long aLength) throws SQLException
    {
        aTarget.open(mPreparedStatement.mConnection.channel());
        BlobOutputStream sOut = (BlobOutputStream)aTarget.setAsciiStream(1);
        updateLob(sOut, aSource, aLength);
    }
    
    private void updateClob(AltibaseClob aTarget, Reader aSource, long aLength) throws SQLException
    {
        aTarget.open(mPreparedStatement.mConnection.channel());
        ClobWriter sOut = (ClobWriter)aTarget.setCharacterStream(1);
        
        try
        {
            if ( (aSource instanceof ConnectionSharable)
              && ((ConnectionSharable) aSource).isSameConnectionWith(mPreparedStatement.mConnection.channel()) )
            {
                ((ConnectionSharable)aSource).setCopyMode();
                ((ConnectionSharable)aSource).readyToCopy();
                sOut.setCopyMode();

                sOut.write(aSource, (int)aLength);

                ((ConnectionSharable)aSource).releaseCopyMode();
                sOut.releaseCopyMode();
            }
            else
            {
                sOut.write(aSource, (int)aLength);
            }
        } 
        catch (IOException sException)
        {
            Error.throwCommunicationErrorException(sException);
        }
    }
}
