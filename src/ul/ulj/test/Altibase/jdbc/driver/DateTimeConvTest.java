package Altibase.jdbc.driver;

import Altibase.jdbc.driver.ex.ErrorDef;

import java.sql.*;
import java.text.DateFormat;
import java.text.SimpleDateFormat;

public class DateTimeConvTest extends AltibaseTestCase
{
    private static final String SQL_DATE_FORMAT = "yyyy-MM-dd HH24:mi:ss.FF6";
    private static final String SQL_TO_DATE     = "TO_DATE";

    protected String[] getCleanQueries()
    {
        return new String[] {
                "DROP TABLE t1",
        };
    }

    protected String[] getInitQueries()
    {
        return new String[] {
                "CREATE TABLE t1 (c1 DATE)",
                "INSERT INTO t1 VALUES (" + SQL_TO_DATE + "('1234-01-23 12:23:34.567123','" + SQL_DATE_FORMAT + "'))",
        };
    }

    private static final int TEST_TYPE_SETXXX     = 0;
    private static final int TEST_TYPE_DATE       = 1;
    private static final int TEST_TYPE_TIME       = 2;
    private static final int TEST_TYPE_TIMESTAMP  = 3;

    private static final int TEST_VALUE_STRING    = 0;
    private static final int TEST_VALUE_DATE      = 1;
    private static final int TEST_VALUE_TIME      = 2;
    private static final int TEST_VALUE_TIMESTAMP = 3;

    public void testSetDateTime() throws Exception
    {
        testSetDateTime(TEST_TYPE_SETXXX);
        testSetDateTime(TEST_TYPE_DATE);
        testSetDateTime(TEST_TYPE_TIME);
        testSetDateTime(TEST_TYPE_TIMESTAMP);
    }

    private static final String[][] EXPECTED_VALUES;

    // Oracle�� getTimestamp()�� ���� ���� nanosecond�� �����Ǿ����� �ʴ�. ����;
    private static final String[][] EXPECTED_VALUES_FOR_ORACLE = {
        {"2134-12-23 00:00:00.0"     , "2134-12-23 00:00:00.0"     , "2134-12-23 12:34:56.0"     , "2134-12-23 12:34:56.0"     },
        {"2134-12-23 00:00:00.0"     , "2134-12-23 00:00:00.0"     , null                        , "2134-12-23 12:34:56.0"     },
        {"1970-01-01 12:34:56.0"     , "2134-12-23 12:34:56.0"     , "2134-12-23 12:34:56.0"     , "2134-12-23 12:34:56.0"     },
        {"2134-12-23 12:34:56.0"     , "2134-12-23 00:00:00.0"     , null                        , "2134-12-23 12:34:56.0"     },
    };

    // Date, Time�� millisecond�����ۿ� ����´�. �׷��� �Ϻδ� milllisecond ������ ���´�.
    // Altibase�� microsecond������ �����ϹǷ� �ִ� 6�ڸ������� ǥ�õȴ�.
    private static final String[][] EXPECTED_VALUES_FOR_ALTIBASE = {
        {"2134-12-23 00:00:00.0"     , "2134-12-23 00:00:00.0"     , "2134-12-23 12:34:56.0"     , "2134-12-23 12:34:56.123456"},
        {"2134-12-23 00:00:00.0"     , "2134-12-23 00:00:00.0"     , null                        , "2134-12-23 12:34:56.123456"},
        {"1970-01-01 12:34:56.0"     , "2134-12-23 12:34:56.0"     , "2134-12-23 12:34:56.0"     , "2134-12-23 12:34:56.0"     },
        {"2134-12-23 12:34:56.123456", "2134-12-23 00:00:00.0"     , null                        , "2134-12-23 12:34:56.123456"},
    };

    static
    {
        if (DateTimeConvTest.class.getSuperclass().equals(AltibaseTestCase.class))
            EXPECTED_VALUES = EXPECTED_VALUES_FOR_ALTIBASE;
        else
            EXPECTED_VALUES = EXPECTED_VALUES_FOR_ORACLE;
    }

    private void testSetDateTime(int aTestType) throws Exception
    {
        int sJDBCType = (aTestType == 0) ? 0 : (aTestType + 90);

        PreparedStatement sUpdStmt = connection().prepareStatement("UPDATE t1 SET c1 = ?");

        String sStr = null;
        Timestamp sTS = Timestamp.valueOf("2134-12-23 12:34:56.123456789");
        Date sDate = new Date(sTS.getTime());
        Time sTime = new Time(sTS.getTime());
        assertEquals(sTS.getTime(), sDate.getTime());
        assertEquals(sTS.getTime(), sTime.getTime());

        // STRING
        if (sJDBCType != 0)
        {
            switch (sJDBCType)
            {
                case Types.TIMESTAMP:
                    sStr = sTS.toString();
                    break;
                case Types.DATE:
                    sStr = sDate.toString();
                    break;
                case Types.TIME:
                    sStr = sTime.toString();
                    break;
            }
            sUpdStmt.setObject(1, sStr, sJDBCType);
        }
        else
        {
            if (AltibaseTestCase.class.isInstance(this))
            {
                sStr = "23/DEC/2134";
            }
            else
            {
                sStr = "2134-12-23";
            }
            sUpdStmt.setString(1, sStr);
        }
        assertEquals(1, sUpdStmt.executeUpdate());
        assertTimestamp(EXPECTED_VALUES[aTestType][TEST_VALUE_STRING]);

        // DATE
        if (sJDBCType != 0)
        {
            sUpdStmt.setObject(1, sDate, sJDBCType);
        }
        else
        {
            sUpdStmt.setDate(1, sDate);
        }
        assertEquals(1, sUpdStmt.executeUpdate());
        assertTimestamp(EXPECTED_VALUES[aTestType][TEST_VALUE_DATE]);

        // TIME
        try
        {
            if (sJDBCType != 0)
            {
                sUpdStmt.setObject(1, sTime, sJDBCType);
            }
            else
            {
                sUpdStmt.setTime(1, sTime);
            }
            assertEquals(1, sUpdStmt.executeUpdate());
            assertTimestamp(EXPECTED_VALUES[aTestType][TEST_VALUE_TIME]);
        }
        catch (SQLException sEx)
        {
            // DATE, TIMESTAMP�� Time ���� �ִ°� ������� �ʴ´�.
            assertEquals(null, EXPECTED_VALUES[aTestType][TEST_VALUE_TIME]);
            assertEquals(ErrorDef.UNSUPPORTED_TYPE_CONVERSION, sEx.getErrorCode());
        }

        // TIMESTAMP
        if (sJDBCType != 0)
        {
            sUpdStmt.setObject(1, sTS, sJDBCType);
        }
        else
        {
            sUpdStmt.setTimestamp(1, sTS);
        }
        assertEquals(1, sUpdStmt.executeUpdate());
        assertTimestamp(EXPECTED_VALUES[aTestType][TEST_VALUE_TIMESTAMP]);

        sUpdStmt.close();
    }

    // Timestamp ���� ��� ���ڿ��� ��ȯ�� ���� expected value�� ������ ����.
    private void assertTimestamp(String aExpVal) throws SQLException
    {
        Statement sStmt = connection().createStatement();
        ResultSet sRS = sStmt.executeQuery("SELECT * FROM t1");
        assertEquals(true, sRS.next());
        assertEquals(aExpVal, sRS.getTimestamp(1).toString());
        sRS.close();
        sStmt.close();
    }

    private static final DateFormat DATE_FORM = new SimpleDateFormat("yyyy-MM-dd HH:mm:ss.SSS");
    private static final String[] EXP_GET_VALUE = {
        "1234-01-23 00:00:00.000", "1970-01-01 12:23:34.000", "1234-01-23 12:23:34.567123",
    };

    public void testGetDateTime() throws SQLException
    {
        Statement sStmt = connection().createStatement();
        ResultSet sRS = sStmt.executeQuery("SELECT * FROM t1");
        assertEquals(true, sRS.next());
        assertEquals(EXP_GET_VALUE[0], DATE_FORM.format(sRS.getDate(1)));
        assertEquals(EXP_GET_VALUE[1], DATE_FORM.format(sRS.getTime(1)));
        assertEquals(EXP_GET_VALUE[2], sRS.getTimestamp(1).toString());
    }
}
