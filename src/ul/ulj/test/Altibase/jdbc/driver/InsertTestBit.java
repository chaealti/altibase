package Altibase.jdbc.driver;

import java.sql.PreparedStatement;
import java.sql.ResultSet;
import java.sql.SQLException;
import java.sql.Statement;
import java.sql.Types;
import java.util.BitSet;

import static org.junit.Assert.assertArrayEquals;

public class InsertTestBit extends AltibaseTestCase
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
                "CREATE TABLE t1 (c1 BIT(3))"
        };
    }

    public void testInsertByBitSet() throws Exception
    {
        PreparedStatement sStmt;

        BitSet sBitSet = new BitSet(3);
        sBitSet.clear();
        sBitSet.set(1);

        sStmt = connection().prepareStatement("INSERT INTO t1 VALUES (?)");
        sStmt.setObject(1, sBitSet);
        assertEquals(1, sStmt.executeUpdate());
        assertExecuteScalar("010");

        sStmt = connection().prepareStatement("UPDATE t1 SET c1 = ?");
        sBitSet.clear();
        sBitSet.set(0);
        sBitSet.set(2);
        sStmt.setObject(1, sBitSet);
        assertEquals(1, sStmt.executeUpdate());
        assertExecuteScalar("101");

        sStmt.setObject(1, null, Types.BIT);
        assertEquals(1, sStmt.executeUpdate());
        assertExecuteScalar(null);

        sStmt.close();
    }

    public void testInsertByString() throws SQLException
    {
        PreparedStatement sStmt;

        sStmt = connection().prepareStatement("INSERT INTO t1 VALUES (?)");
        sStmt.setObject(1, "01", Types.BIT);
        assertEquals(1, sStmt.executeUpdate());
        assertExecuteScalar("010");

        sStmt = connection().prepareStatement("UPDATE t1 SET c1 = ?");
        sStmt.setObject(1, "101", Types.BIT);
        assertEquals(1, sStmt.executeUpdate());
        assertExecuteScalar("101");

        sStmt.setObject(1, "1", Types.BIT);
        assertEquals(1, sStmt.executeUpdate());
        assertExecuteScalar("100");

        sStmt.setObject(1, null, Types.BIT);
        assertEquals(1, sStmt.executeUpdate());
        assertExecuteScalar(null);

        sStmt.close();
    }

    public void testPrecisionInit() throws SQLException
    {
        Statement sInsStmt = connection().createStatement();
        assertEquals(1, sInsStmt.executeUpdate("INSERT INTO t1 VALUES (BIT'0')"));
        sInsStmt.close();

        PreparedStatement sStmt = connection().prepareStatement("UPDATE t1 SET c1 = ?");
        sStmt.setObject(1, "101", Types.BIT);
        assertEquals(1, sStmt.executeUpdate());
        assertExecuteScalar("101");

        // ColumnInfo�� �ʱ�ȭ�ϱ� ���ؼ� �ٸ� Ÿ������ �ѹ� set �Ѵ�.
        // �������� �ʴ� Ÿ���̹Ƿ� �翬�� ������ ��������.
        sStmt.setObject(1, new java.sql.Date(1));
        try
        {
            sStmt.executeUpdate();
            fail();
        }
        catch (SQLException sEx)
        {
        }

        // BIT Ÿ�Կ� ColumnInfo�� �ٽ� ����Ƿ� Precision�� 0���� �ʱ�ȭ�Ǿ��ִ�.
        sStmt.setObject(1, "1", Types.BIT);
        assertEquals(1, sStmt.executeUpdate());
        assertExecuteScalar("100");

        sStmt.setObject(1, null, Types.BIT);
        assertEquals(1, sStmt.executeUpdate());
        assertExecuteScalar(null);

        sStmt.close();
    }

    public void testGetBytes() throws SQLException
    {
        Statement sStmt = connection().createStatement();
        assertEquals(1, sStmt.executeUpdate("INSERT INTO t1 VALUES (BIT'101')"));
        assertEquals(1, sStmt.executeUpdate("INSERT INTO t1 VALUES (BIT'111')"));

        ResultSet sRS = sStmt.executeQuery("SELECT * FROM t1");
        assertEquals(true, sRS.next());
        assertEquals("101", sRS.getString(1));
        assertArrayEquals(new byte[]{(byte)0xA0}, sRS.getBytes(1));
        assertEquals(true, sRS.next());
        assertEquals("111", sRS.getString(1));
        assertArrayEquals(new byte[]{(byte)0xE0}, sRS.getBytes(1));
        assertEquals(false, sRS.next());
        sStmt.close();
    }
}
