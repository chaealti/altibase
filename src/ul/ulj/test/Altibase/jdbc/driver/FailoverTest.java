package Altibase.jdbc.driver;

import java.sql.Connection;
import java.sql.DriverManager;
import java.sql.SQLException;

public class FailoverTest extends AltibaseTestCase
{
    protected String getURL()
    {
        return "jdbc:Altibase://newjdbc_failovertest";
    }

    protected boolean useConnectionPreserve()
    {
        return false;
    }

    public void testFailover() throws SQLException
    {
        Connection sConn = DriverManager.getConnection(getURL());
        // � ���������� ������ �����ؾ� �Ѵ�.
        sConn.close();
    }
}
