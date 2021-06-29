package Altibase.jdbc.driver;

import java.sql.SQLException;
import java.sql.Statement;

import Altibase.jdbc.driver.ex.ErrorDef;

public class StatementTest extends AltibaseTestCase
{
    protected String[] getCleanQueries()
    {
        return new String[0];
    }

    protected String[] getInitQueries()
    {
        return new String[0];
    }

    public void testFree() throws Exception
    {
        Statement sStmt = connection().createStatement();

        AltibaseTestCase.assertExecuteScalar(sStmt, "1", "Count(*)", "V$STATEMENT");
        Statement sTmpStmt1 = connection().prepareStatement("SELECT 1 FROM DUAL");
        AltibaseTestCase.assertExecuteScalar(sStmt, "2", "Count(*)", "V$STATEMENT");
        Statement sTmpStmt2 = connection().prepareStatement("SELECT 1 FROM DUAL");
        AltibaseTestCase.assertExecuteScalar(sStmt, "3", "Count(*)", "V$STATEMENT");
        sTmpStmt2.close();
        AltibaseTestCase.assertExecuteScalar(sStmt, "2", "Count(*)", "V$STATEMENT");
        sTmpStmt1.close();
        AltibaseTestCase.assertExecuteScalar(sStmt, "1", "Count(*)", "V$STATEMENT");

        sStmt.close();
    }

    public void testMaxStmt() throws SQLException
    {
        // �ִ� 65536������ ���� �� ������, Connection���� ���� ó�������� 1�� ���Ƿ� 65535������ ���� �� �ִ�.
        Statement[] sStmt = new Statement[65535];
        for (int i=0; i<sStmt.length; i++)
        {
            sStmt[i] = connection().createStatement();
        }
        try
        {
            connection().createStatement();
        }
        catch (SQLException sEx)
        {
            assertEquals(ErrorDef.TOO_MANY_STATEMENTS, sEx.getErrorCode());
        }

        // �ݰ� ����� ����������Ѵ�. (������� CID���� ����� �����Ǵ��� Ȯ��)
        sStmt[0].close();
        sStmt[0] = connection().createStatement();
    }
}
