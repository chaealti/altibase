package Altibase.jdbc.driver.datatype;

import Altibase.jdbc.driver.AltibaseBlob;
import Altibase.jdbc.driver.AltibaseTestCase;

import java.io.StringReader;
import java.sql.*;

import static org.junit.Assert.assertArrayEquals;

public class BatchLobSetNullTest extends AltibaseTestCase
{
    protected String[] getCleanQueries()
    {
        return new String[] {
                "DROP TABLE t1",
        };
    }

    protected String[] getInitQueries()
    {
        return new String[] {
                "CREATE TABLE t1 (i1 INTEGER, i2 CLOB, i3 BLOB)",
        };
    }

    protected String getURL()
    {
        return super.getURL() + "?CLIENTSIDE_AUTO_COMMIT=on";
    }
    
    /**
     * clob,blob �÷��� ���� 1��° not null, 2��° null, 3��° not null�� ���� �� �ι�° �ο��� ���� null���� üũ
     * @throws SQLException
     */
    public void testBatchSetNullAndWasNull() throws SQLException
    {
        insertData();
        updateData();
        selectData();
    }

    private void updateData() throws SQLException
    {
        String sClobText = "clobtext";
        PreparedStatement sStmt = null;
        try
        {
            sStmt = getConnection().prepareStatement("UPDATE t1 SET i2 = ?, i3 = ? WHERE i1 = ? ");
            for (int i=0; i < 3; i++)
            {
                if (i == 0)
                {
                    // ù��° �࿡�� ������ �ִ� ���� null�� ������Ʈ�Ѵ�.
                    sStmt.setNull(1, Types.VARCHAR);
                    sStmt.setNull(2, Types.VARCHAR);
                }
                else
                {
                    // �ι�°�� ����° �࿡�� ���� �����͸� ������Ʈ�Ѵ�.
                    sStmt.setCharacterStream(1, new StringReader(sClobText), sClobText.length());
                    sStmt.setBytes(2, new byte[] {0x1f, 0x1d});
                }
                sStmt.setInt(3, i+1);
                sStmt.addBatch();
            }
            sStmt.executeBatch();
            sStmt.close();
        }
        finally
        {
            // clientside_auto_commit�� on�̱⶧���� finally������ Statement�� close���ش�.
            sStmt.close();
        }
    }

    private void selectData() throws SQLException
    {
        Statement sSelStmt = null;
        ResultSet sRs = null;
        try
        {
            sSelStmt = getConnection().createStatement();
            sRs = sSelStmt.executeQuery("SELECT * FROM t1 ORDER BY i1 ");
            for (int i=0; sRs.next(); i++)
            {
                int idx = sRs.getInt(1);
                assertEquals(i + 1, idx);
                checkClobValue(i, sRs);

                if (i == 0)
                {
                    assertTrue(i+" th wasNull", sRs.wasNull());
                }
                else 
                {
                    assertFalse(i + "th wasNotNull", sRs.wasNull());
                }
                checkBlobValue(i, sRs);

                if (i == 0)
                {
                    assertTrue(i+" th wasNull", sRs.wasNull());
                }
                else 
                {
                    assertFalse(i + "th wasNotNull", sRs.wasNull());
                }
            }
        }
        finally
        {
            // clientside_auto_commit�� on�̱⶧���� ResultSet �� Statement�� close���ش�.
            sRs.close();
            sSelStmt.close();
        }
    }

    private void checkClobValue(int idx, ResultSet sRs) throws SQLException
    {
        Object sClobVal = sRs.getObject(2);

        if (idx == 0)
        {
            assertNull(sClobVal);
        }
        else
        {
            assertEquals("clobtext", sClobVal.toString());
        }
    }

    private void checkBlobValue(int idx, ResultSet sRs) throws SQLException
    {
        Object sBlobVal = sRs.getObject(3);

        if (idx == 0)
        {
            assertNull(sBlobVal);
        }
        else
        {
            assertArrayEquals(new byte[] { 0x1f, 0x1d }, ((AltibaseBlob)sBlobVal).getBytes(1, 2));
        }
    }

    private void insertData() throws SQLException
    {
        String sClobText = "clobtext";
        PreparedStatement sStmt = null;
        try
        {
            sStmt = getConnection().prepareStatement("INSERT INTO t1 VALUES (?, ?, ?); ");
            for (int i=0; i < 3; i++)
            {
                sStmt.setInt(1, i+1);
                if (i == 1)
                {
                    // 2��° �࿡�� null���� insert�Ѵ�.
                    sStmt.setNull(2, Types.VARCHAR);
                    sStmt.setNull(3, Types.VARCHAR);
                }
                else
                {  
                    // ù��°�� ����° �࿡�� ���� �����͸� insert�Ѵ�.
                    sStmt.setCharacterStream(2, new StringReader(sClobText), sClobText.length());
                    sStmt.setBytes(3, new byte[] {0x1f, 0x1d});
                }
                sStmt.addBatch();
            }
            sStmt.executeBatch();
            sStmt.close();
        }
        finally
        {
            // clientside_auto_commit�� on�̱⶧���� finally������ Statement�� close���ش�.
            sStmt.close();
        }
    }
}