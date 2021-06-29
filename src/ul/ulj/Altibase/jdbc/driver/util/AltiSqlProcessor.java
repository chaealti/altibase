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

package Altibase.jdbc.driver.util;

import java.sql.SQLException;
import java.util.ArrayList;
import java.util.Map;

import Altibase.jdbc.driver.ex.Error;
import Altibase.jdbc.driver.ex.ErrorDef;

public class AltiSqlProcessor
{
    private static final int    INDEX_NOT_FOUND        = -1;

    private static final String INTERNAL_SQL_CURRVAL   = ".CURRVAL ";
    private static final String INTERNAL_SQL_NEXTVAL   = ".NEXTVAL";
    private static final String INTERNAL_SQL_VALUES    = "VALUES";
    private static final String INTERNAL_SQL_SELECT    = "SELECT ";
    private static final String INTERNAL_SQL_FROM_DUAL = " FROM DUAL";
    private static final String INTERNAL_SQL_PROWID    = ",_PROWID ";

    static class SequenceInfo
    {
        final int    mIndex;
        final String mSeqName;
        final String mColumnName;

        SequenceInfo(int aIndex, String aSeqName, String aColumnName)
        {
            mIndex = aIndex;
            mSeqName = aSeqName;
            mColumnName = aColumnName;
        }
    }

    /**
     * �������� �Ľ��� SEQUENCE �̸��� SEQUENCE�� ���� �÷��� �̸��� ��´�.
     * 
     * @param aSql SEQUENCE ������ �Ľ��� INSERT ������
     * @return SEQUENCE ������ ���� ����Ʈ
     */
    public static ArrayList getAllSequences(String aSql)
    {
        ArrayList sSeqInfos = new ArrayList();
        try
        {
            String[] sCols = getColumnListOfInsert(aSql);
            String[] sValues = getValueListOfInsert(aSql);
            for (int i = 0; i < sValues.length; i++)
            {
                if (StringUtils.endsWithIgnoreCase(sValues[i], INTERNAL_SQL_NEXTVAL))
                {
                    String sSeqName = sValues[i].substring(0, sValues[i].length() - INTERNAL_SQL_NEXTVAL.length());
                    String sColName = (sCols == null) ? "\"_AUTO_GENERATED_KEY_COLUMN_\"" : sCols[i];
                    sSeqInfos.add(new SequenceInfo(i + 1, sSeqName, sColName));
                }
            }
        }
        catch (Exception e)
        {
            // parsing error�� �߻��ϸ� empty list�� �����Ѵ�.
            sSeqInfos.clear();
        }
        return sSeqInfos;
    }

    /**
     * INSERT ���������� �÷� ����� ��´�.
     * 
     * @param aInsertSql INSERT ������
     * @return �÷� ���. INSERT �������� �÷� �̸� ����� ���ٸ� null
     */
    private static String[] getColumnListOfInsert(String aInsertSql)
    {
        String sSql = aInsertSql.substring(0, aInsertSql.toUpperCase().indexOf(INTERNAL_SQL_VALUES));
        if (sSql.lastIndexOf("(") < 0)
        {
            // SQL�� column name list�� ���� ����̴�.
            return null;
        }
        sSql = sSql.substring(sSql.lastIndexOf("(") + 1, sSql.lastIndexOf(")")).trim();
        String[] sResult = sSql.split(",");
        for (int i = 0; i < sResult.length; i++)
        {
            sResult[i] = sResult[i].trim();
        }
        return sResult;
    }

    /**
     * INSERT ���������� �� ����� ��´�.
     * 
     * @param aInsertSql INSERT ������
     * @return �� ���
     */
    private static String[] getValueListOfInsert(String aInsertSql)
    {
        String sSql = aInsertSql.substring(aInsertSql.toUpperCase().indexOf(INTERNAL_SQL_VALUES) + INTERNAL_SQL_VALUES.length() + 1).trim();
        sSql = sSql.substring(sSql.indexOf("(") + 1, sSql.lastIndexOf(")"));
        String[] sResult = sSql.split(",");
        for (int i = 0; i < sResult.length; i++)
        {
            sResult[i] = sResult[i].trim();
        }
        return sResult;
    }

    public static String processEscape(String aOrgSql)
    {
        int sIndex;
        sIndex = aOrgSql.indexOf("{");
        if (sIndex < 0)
        {
            // ������ ���� ������� {�� �ִ��� �˻��� ������ �ٷ� �����Ѵ�.
            return aOrgSql;
        }

        String sResult = processEscapeExpr(aOrgSql, sIndex);
        if (sResult.equals(aOrgSql))
        {
            return sResult;
        }
        return processEscape(sResult);
    }

    private static String processEscapeExpr(String aSql, int aBraceIndex)
    {
        boolean sQuatOpen = false;
        for (int i = 0; i < aBraceIndex; i++)
        {
            if (aSql.charAt(i) == '\'')
            {
                sQuatOpen = !sQuatOpen;
            }
        }
        if (sQuatOpen)
        {
            // '' ���� {�̱� ������ �����Ѵ�. ���� {�� ã�ƶ�~
            aBraceIndex = aSql.indexOf("{", aBraceIndex + 1);
            if (aBraceIndex >= 0)
            {
                return processEscapeExpr(aSql, aBraceIndex);
            }
            else
            {
                return aSql;
            }
        }
        int sEndBraceIndex = INDEX_NOT_FOUND;
        for (int i = aBraceIndex + 1; i < aSql.length(); i++)
        {
            if (aSql.charAt(i) == '\'')
            {
                sQuatOpen = !sQuatOpen;
            }
            else if (!sQuatOpen && aSql.charAt(i) == '}')
            {
                sEndBraceIndex = i;
                break;
            }
        }
        if (sEndBraceIndex == INDEX_NOT_FOUND)
        {
            return aSql;
        }
        String sExpr = convertToNative(aSql.substring(aBraceIndex + 1, sEndBraceIndex).trim()); // { }���� ���ڿ��� ���´�.
        if (sExpr == null)
        {
            return aSql;
        }
        String sResult = aSql.substring(0, aBraceIndex) + " " + sExpr;
        if (sEndBraceIndex + 1 < aSql.length())
        {
            sResult += " " + aSql.substring(sEndBraceIndex + 1, aSql.length());
        }
        return sResult;
    }

    private static String convertToNative(String aExpr)
    {
        // input�� { }���� ���ڿ��̴�. {, }�� �� ���̴�.

        int sIndex = aExpr.indexOf(" ");
        if (sIndex < 0)
        {
            return null;
        }
        String sWord = aExpr.substring(0, sIndex);
        if (sWord.equalsIgnoreCase("escape"))
        {
            // { escape ... } ������ �״�� �����ϸ� �ȴ�.
            return aExpr;
        }
        else if (sWord.equalsIgnoreCase("fn"))
        {
            // { fn ... } ���� ���� ...�� �״�� �����ϸ� �ȴ�.
            return aExpr.substring(sIndex + 1, aExpr.length());
        }
        else if (sWord.equalsIgnoreCase("d"))
        {
            // { d '2011-10-17' }�� to_date('2011-10-17', 'yyyy-MM-dd') ���·� �ٲ۴�.
            return "to_date(" + aExpr.substring(sIndex + 1, aExpr.length()) + ", 'yyyy-MM-dd')";
        }
        else if (sWord.equalsIgnoreCase("t"))
        {
            // { t '12:32:56' }�� to_date('12:32:56', 'hh:mi:ss') ���·� �ٲ۴�.
            return "to_date(" + aExpr.substring(sIndex + 1, aExpr.length()) + ", 'hh24:mi:ss')";
        }
        else if (sWord.equalsIgnoreCase("ts"))
        {
            // { ts '2011-10-17 12:32:56.123456' }�� to_date('2011-10-17 12:32:56.123456', 'yyyy-MM-dd hh:mi:ss.ffffff')�� �ٲ۴�.
            if (aExpr.indexOf(".") > 0)
            {
                // fraction�� �ִ� ���
                return "to_date(" + aExpr.substring(sIndex + 1, aExpr.length()) + ", 'yyyy-MM-dd hh24:mi:ss.ff6')";
            }
            // fraction�� ���� ���
            return "to_date(" + aExpr.substring(sIndex + 1, aExpr.length()) + ", 'yyyy-MM-dd hh24:mi:ss')";
        }
        else if (sWord.equalsIgnoreCase("call"))
        {
            // { call procedure_name(...) }�� exec procedure_name(...)�� �ٲ۴�.
            return "execute " + aExpr.substring(sIndex + 1, aExpr.length());
        }
        else if (sWord.startsWith("?"))
        {
            int i;
            // BUG-47424 { ? = call func(?) } ���� ��� sWord�� ? �̱⶧���� aExpr String���� ���Ѵ�.
            for (i = 1; aExpr.charAt(i) == ' '; i++)
                ;
            if (aExpr.charAt(i) != '=')
            {
                return null;
            }
            else
            {
                i++;
            }
            for (; aExpr.charAt(i) == ' '; i++)
                ;

            if (aExpr.substring(i, i + 5).equalsIgnoreCase("call "))
            {
                for (i = i + 5; aExpr.charAt(i) == ' '; i++)
                    ;

                // { ? = call procedure_name(...) }�� exec ? := procedure_name(...)�� �ٲ۴�.
                return "execute ? := " + aExpr.substring(i);
            }
            // ?=call ������ �ƴϹǷ� ��ŵ
        }
        else if (sWord.equalsIgnoreCase("oj"))
        {
            // {oj table1 {left|right|full} outer join table2 on condition}�� oj�� ���� �״�� ����Ѵ�.
            return aExpr.substring(sIndex + 1, aExpr.length());
        }
        return null;
    }

    /**
     * Generated Keys�� ������� �������� �����.
     * 
     * @param aSeqs SEQUENCE ������ ���� ����Ʈ
     * @return Generated Keys�� ������� ������
     */
    public static String makeGenerateKeysSql(ArrayList aSeqs)
    {
        StringBuffer sSql = new StringBuffer(INTERNAL_SQL_SELECT);
        for (int i = 0; i < aSeqs.size(); i++)
        {
            SequenceInfo sSeqInfo = (SequenceInfo)aSeqs.get(i);
            if (i > 0)
            {
                sSql.append(",");
            }
            sSql.append(sSeqInfo.mSeqName).append(INTERNAL_SQL_CURRVAL).append(sSeqInfo.mColumnName);
        }
        sSql.append(INTERNAL_SQL_FROM_DUAL);
        return sSql.toString();
    }

    /**
     * Generated Keys�� ������� �������� �����.
     * 
     * @param aSeqs SEQUENCE ������ ���� ����Ʈ
     * @param aColIndexes ���� ���� SEQUENCE�� ����(1 base)�� ���� �迭
     * @return Generated Keys�� ������� ������
     */
    public static String makeGenerateKeysSql(ArrayList aSeqs, int[] aColIndexes) throws SQLException
    {
        StringBuffer sSql = new StringBuffer(INTERNAL_SQL_SELECT);
        for (int i = 0; i < aColIndexes.length; i++)
        {
            int j = 0;
            for (; j < aSeqs.size(); j++)
            {
                SequenceInfo sSeqInfo = (SequenceInfo)aSeqs.get(j);
                if (sSeqInfo.mIndex == aColIndexes[i])
                {
                    if (i > 0)
                    {
                        sSql.append(",");
                    }
                    sSql.append(sSeqInfo.mSeqName).append(INTERNAL_SQL_CURRVAL).append(sSeqInfo.mColumnName);
                    break;
                }
            }
            if (j == aSeqs.size())
            {
                StringBuffer sExpIndexs = new StringBuffer();
                for (j = 0; j < aSeqs.size(); j++)
                {
                    SequenceInfo sSeqInfo = (SequenceInfo)aSeqs.get(j);
                    if (j > 0)
                    {
                        sExpIndexs.append(',');
                    }
                    sExpIndexs.append(sSeqInfo.mIndex);
                }
                Error.throwSQLException(ErrorDef.INVALID_COLUMN_INDEX,
                                        sExpIndexs.toString(),
                                        String.valueOf(aColIndexes[i]));
            }
        }
        sSql.append(INTERNAL_SQL_FROM_DUAL);
        return sSql.toString();
    }

    /**
     * Generated Keys�� ������� �������� �����.
     * 
     * @param aSeqs SEQUENCE ������ ���� ����Ʈ
     * @param aColNames ���� ���� SEQUENCE�� ���� �÷� �̸��� ���� �迭
     * @return Generated Keys�� ������� ������
     */
    public static String makeGenerateKeysSql(ArrayList aSeqs, String[] aColNames) throws SQLException
    {
        StringBuffer sSql = new StringBuffer(INTERNAL_SQL_SELECT);
        for (int i = 0; i < aColNames.length; i++)
        {
            int j;
            for (j = 0; j < aSeqs.size(); j++)
            {
                SequenceInfo sSeqInfo = (SequenceInfo)aSeqs.get(j);
                if (aColNames[i].equalsIgnoreCase(sSeqInfo.mColumnName))
                {
                    if (i > 0)
                    {
                        sSql.append(',');
                    }
                    sSql.append(sSeqInfo.mSeqName).append(INTERNAL_SQL_CURRVAL).append(sSeqInfo.mColumnName);
                    break;
                }
            }
            if (j == aSeqs.size())
            {
                Error.throwSQLException(ErrorDef.INVALID_COLUMN_NAME, aColNames[i]);
            }
        }
        sSql.append(INTERNAL_SQL_FROM_DUAL);
        return sSql.toString();
    }

    private static int indexOfNonWhitespaceAndComment(String aSrcQstr)
    {
        return indexOfNonWhitespaceAndComment(aSrcQstr, 0);
    }

    private static int indexOfNonWhitespaceAndComment(String aSrcQstr, int aStartIdx)
    {
        for (int i = aStartIdx; i < aSrcQstr.length(); i++)
        {
            char sCh = aSrcQstr.charAt(i);
            if (Character.isWhitespace(sCh))
            {
                continue;
            }
            switch (sCh)
            {
                // C & C++ style comments
                case '/':
                    // block comment
                    if (aSrcQstr.charAt(i + 1) == '*')
                    {
                        for (i += 2; i < aSrcQstr.length(); i++)
                        {
                            if ((aSrcQstr.charAt(i) == '*') && (aSrcQstr.charAt(i + 1) == '/'))
                            {
                                i++;
                                break;
                            }
                        }
                    }
                    // line comment
                    else if (aSrcQstr.charAt(i + 1) == '/')
                    {
                        for (i += 2; (i < aSrcQstr.length()) && (aSrcQstr.charAt(i) != '\n'); i++)
                            ; // skip to end-of-line
                    }
                    break;

                // line comment
                case '-':
                    if (aSrcQstr.charAt(i + 1) == '-')
                    {
                        for (i += 2; (i < aSrcQstr.length()) && (aSrcQstr.charAt(i) != '\n'); i++)
                            ; // skip to end-of-line
                    }
                    break;

                default:
                    return i;
            }
        }
        return INDEX_NOT_FOUND;
    }

    /**
     * FROM ���� ��ġ�� ã�´�.
     * 
     * @param aSrcQstr ���� ������
     * @return FROM ���� ������ FROM�� ���� ��ġ(0 base), �ƴϸ� -1
     */
    private static int indexOfFrom(String aSrcQstr) throws SQLException
    {
        return indexOfFrom(aSrcQstr, 0);
    }

    /**
     * FROM ���� ��ġ�� ã�´�.
     * 
     * @param aSrcQstr ���� ������
     * @param aStartIdx �˻��� ������ index
     * @return FROM ���� ������ FROM�� ���� ��ġ(0 base), �ƴϸ� -1
     */
    private static int indexOfFrom(String aSrcQstr, int aStartIdx) throws SQLException
    {
        for (int i = aStartIdx; i < aSrcQstr.length(); i++)
        {
            switch (aSrcQstr.charAt(i))
            {
                // ' ~ '
                case '\'':
                    for (i++; (i < aSrcQstr.length()) && (aSrcQstr.charAt(i) != '\''); i++)
                        ;
                    break;

                // " ~ "
                case '"':
                    for (i++; (i < aSrcQstr.length()) && (aSrcQstr.charAt(i) != '"'); i++)
                        ;
                    break;

                // ( ~ )
                case '(':
                    for (i++; (i < aSrcQstr.length()) && (aSrcQstr.charAt(i) != ')'); i++)
                        ;
                    break;

                // C & C++ style comments
                case '/':
                    // block comment
                    if (aSrcQstr.charAt(i + 1) == '*')
                    {
                        for (i += 2; i < aSrcQstr.length(); i++)
                        {
                            if ((aSrcQstr.charAt(i) == '*') && (aSrcQstr.charAt(i + 1) == '/'))
                            {
                                i++;
                                break;
                            }
                        }
                    }
                    // line comment
                    else if (aSrcQstr.charAt(i + 1) == '/')
                    {
                        for (i += 2; (i < aSrcQstr.length()) && (aSrcQstr.charAt(i) != '\n'); i++)
                            ; // skip to end-of-line
                    }
                    break;

                // line comment
                case '-':
                    if (aSrcQstr.charAt(i + 1) == '-')
                    {
                        for (i += 2; (i < aSrcQstr.length()) && (aSrcQstr.charAt(i) != '\n'); i++)
                            ; // skip to end-of-line
                    }
                    break;

                case 'F':
                case 'f':
                    // {����}FROM{����} ���� Ȯ��
                    if ((i == 0) || (i + 5 >= aSrcQstr.length()) || !isValidFromPrevChar(aSrcQstr.charAt(i - 1)))
                    {
                        break;
                    }

                    if (StringUtils.startsWithIgnoreCase(aSrcQstr, i, "FROM") &&
                        isValidFromNextChar(aSrcQstr.charAt(i + 4)))
                    {
                        return i;
                    }
                    break;
                default:
                    break;
            }
        }
        return INDEX_NOT_FOUND;
    }

    /**
     * FROM ������ ���� �� �ִ� �������� Ȯ���Ѵ�.
     * 
     * @param aCh Ȯ���� ����
     * @return FROM ������ ���� �� �ִ� �������� ����
     */
    private static boolean isValidFromNextChar(char aCh)
    {
        return (Character.isWhitespace(aCh) || aCh == '"' || aCh == '/' || aCh == '(' || aCh == '-');
    }

    /**
     * FROM �տ� ���� �� �ִ� �������� Ȯ���Ѵ�.
     * 
     * @param aCh Ȯ���� ����
     * @return FROM �տ� ���� �� �ִ� �������� ����
     */
    private static boolean isValidFromPrevChar(char aCh)
    {
        return (Character.isWhitespace(aCh) || aCh == '\'' || aCh == '"' || aCh == '/' || aCh == ')');
    }

    /**
     * FROM�� �� ��ġ�� ã�´�.
     * 
     * @param aSql ���� ������
     * @return FROM ���� ������ FROM�� �� ��ġ(0 base), �ƴϸ� -1
     * @throws SQLException 
     */
    private static int indexOfFromEnd(String aSql) throws SQLException
    {
        int sIdxFromB = indexOfFrom(aSql);
        if (sIdxFromB == INDEX_NOT_FOUND)
        {
            return INDEX_NOT_FOUND;
        }
        else
        {
            return indexOfFromEnd(aSql, sIdxFromB);
        }
    }

    /**
     * FROM�� �� ��ġ�� ã�´�.
     * 
     * @param aSql ���� ������
     * @param aIndexOfFrom FROM�� ���� ��ġ(0 base)
     * @return FROM�� �� ��ġ(0 base)
     */
    private static int indexOfFromEnd(String aSql, int aIndexOfFrom)
    {
        // find index of from end
        int sIdxFromE = aIndexOfFrom + 4;
        for (; sIdxFromE < aSql.length() && Character.isWhitespace(aSql.charAt(sIdxFromE)); sIdxFromE++)
            /* skip white space */;
        for (; sIdxFromE < aSql.length() && !Character.isWhitespace(aSql.charAt(sIdxFromE)); sIdxFromE++)
            /* skip to end of table name */;

        if (sIdxFromE < aSql.length() && aSql.charAt(sIdxFromE) != ';')
        {
            int sIdxTokB = sIdxFromE;
            for (; sIdxTokB < aSql.length() && Character.isWhitespace(aSql.charAt(sIdxTokB)); sIdxTokB++)
                /* skip white space */;
            int sIdxTokE = sIdxTokB;
            for (; sIdxTokE < aSql.length() && !Character.isWhitespace(aSql.charAt(sIdxTokE)); sIdxTokE++)
                /* skip to end of table name */;
            int sTokLen = sIdxTokE - sIdxTokB;
            switch (sTokLen)
            {
                case 2:
                    if (StringUtils.startsWithIgnoreCase(aSql, sIdxTokB, "AS"))
                    {
                        sIdxFromE = sIdxTokE;
                        for (; sIdxFromE < aSql.length() && Character.isWhitespace(aSql.charAt(sIdxFromE)); sIdxFromE++)
                            /* skip white space */;
                        for (; sIdxFromE < aSql.length() && !Character.isWhitespace(aSql.charAt(sIdxFromE)); sIdxFromE++)
                            /* skip to end of alias name */;
                    }
                    break;
                case 3:
                    if (!StringUtils.startsWithIgnoreCase(aSql, sIdxTokB, "FOR"))
                    {
                        sIdxFromE = sIdxTokE;
                    }
                    break;
                case 4:
                    if (!StringUtils.startsWithIgnoreCase(aSql, sIdxTokB, "FOR"))
                    {
                        sIdxFromE = sIdxTokE;
                    }
                    break;
                case 5:
                    if (!StringUtils.startsWithIgnoreCase(aSql, sIdxTokB, "WHERE") &&
                        !StringUtils.startsWithIgnoreCase(aSql, sIdxTokB, "GROUP") &&
                        !StringUtils.startsWithIgnoreCase(aSql, sIdxTokB, "UNION") &&
                        !StringUtils.startsWithIgnoreCase(aSql, sIdxTokB, "MINUS") &&
                        !StringUtils.startsWithIgnoreCase(aSql, sIdxTokB, "START") &&
                        !StringUtils.startsWithIgnoreCase(aSql, sIdxTokB, "ORDER") &&
                        !StringUtils.startsWithIgnoreCase(aSql, sIdxTokB, "LIMIT"))
                    {
                        sIdxFromE = sIdxTokE;
                    }
                    break;
                case 6:
                    if (!StringUtils.startsWithIgnoreCase(aSql, sIdxTokB, "HAVING"))
                    {
                        sIdxFromE = sIdxTokE;
                    }
                    break;
                case 7:
                    if (!StringUtils.startsWithIgnoreCase(aSql, sIdxTokB, "CONNECT"))
                    {
                        sIdxFromE = sIdxTokE;
                    }
                    break;
                case 9:
                    if (!StringUtils.startsWithIgnoreCase(aSql, sIdxTokB, "INTERSECT"))
                    {
                        sIdxFromE = sIdxTokE;
                    }
                    break;
                default:
                    sIdxFromE = sIdxTokE;
                    break;
            }
        }
        return sIdxFromE;
    }

    public static final int KEY_SET_ROWID_COLUMN_INDEX = 1;

    public static String makeKeySetSql(String aSql, Map aOrderByMap) throws SQLException
    {
        if (!isSelectQuery(aSql))
        {
            return aSql;
        }

        int sIdxFrom = indexOfFrom(aSql);
        int sIdxOrderList = indexOfOrderByList(aSql, sIdxFrom);
        StringBuffer sBuf = new StringBuffer(aSql.length());
        // ���� SQL���� ���� KEY_SET_ROWID_COLUMN_INDEX�� 1�� ������.
        sBuf.append("SELECT _PROWID ");
        if (sIdxOrderList == INDEX_NOT_FOUND || aOrderByMap == null || aOrderByMap.size() == 0)
        {
            sBuf.append(aSql.substring(sIdxFrom));
        }
        else
        {
            sBuf.append(aSql.substring(sIdxFrom, sIdxOrderList));
            boolean sNeedComma = false;
            for (int i = sIdxOrderList; i < aSql.length(); i++)
            {
                i = indexOfNonWhitespaceAndComment(aSql, i);
                char sCh = aSql.charAt(i);
                String sOrderBy;
                // value
                if (sCh == '\'')
                {
                    int sStartIdx = i;
                    for (i++; i < aSql.length(); i++)
                    {
                        if (aSql.charAt(i) == '\'' && (i + 1 == aSql.length() || aSql.charAt(i + 1) != '\''))
                        {
                            break;
                        }
                    }
                    i++;
                    sOrderBy = aSql.substring(sStartIdx, i);
                }
                // quoted identifier
                else if (sCh == '"')
                {
                    int sStartIdx = i + 1;
                    for (i++; i < aSql.length() && aSql.charAt(i) != '"'; i++)
                        ;
                    sOrderBy = aSql.substring(sStartIdx, i);
                    i++;
                }
                // column position
                else if (Character.isDigit(sCh))
                {
                    int sStartIdx = i;
                    for (i++; i < aSql.length() && Character.isDigit(aSql.charAt(i)); i++)
                        ;
                    sOrderBy = aSql.substring(sStartIdx, i);
                }
                // column name, alias, ...
                else if (isSQLIdentifierStart(sCh))
                {
                    int sStartIdx = i;
                    for (i++; i < aSql.length() && isSQLIdentifierPart(aSql.charAt(i)); i++)
                        ;
                    // non-quoted identifier�� UPPERCASE��
                    sOrderBy = aSql.substring(sStartIdx, i).toUpperCase();
                }
                else
                {
                    throw new AssertionError("Invalid query string");
                }
                String sColNameKey = sOrderBy;
                // order by ������ ����, alias, column name �� �ٸ��͵� �� �� �ִ�.
                String sBaseColumnName = (String)aOrderByMap.get(sColNameKey);
                if (sBaseColumnName != null)
                {
                    sOrderBy = "\"" + sBaseColumnName + "\"";
                }
                if (sNeedComma)
                {
                    sBuf.append(',').append(sOrderBy);
                }
                else
                {
                    sBuf.append(sOrderBy);
                    sNeedComma = true;
                }
                i = indexOfNonWhitespaceAndComment(aSql, i);
                if (i == INDEX_NOT_FOUND)
                {
                    break;
                }
                if (StringUtils.startsWithIgnoreCase(aSql, i, "ASC") && isValidNextCharForOrderByList(aSql, i+3))
                {
                    sBuf.append(" ASC");
                    i += 3;
                }
                else if (StringUtils.startsWithIgnoreCase(aSql, i, "DESC") && isValidNextCharForOrderByList(aSql, i+4))
                {
                    sBuf.append(" DESC");
                    i += 4;
                }
                i = indexOfNonWhitespaceAndComment(aSql, i);
                if (i == INDEX_NOT_FOUND)
                {
                    break;
                }
                // ,�� ������ ORDER BY���� ���� ��
                if (aSql.charAt(i) != ',')
                {
                    sBuf.append(aSql.substring(i));
                    break;
                }
            }
        }
        return sBuf.toString();
    }

    private static boolean isValidNextCharForOrderByList(String aSql, int i)
    {
        if (i >= aSql.length())
        {
            return true;
        }
        char sCh = aSql.charAt(i);
        if (Character.isWhitespace(sCh))
        {
            return true;
        }
        switch (sCh)
        {
            case '-': // comment
            case '/': // C/C++ style comment
            case ',':
                return true;
        }
        return false;
    }

    private static boolean isSQLIdentifierStart(char aCh)
    {
        return ('a' <= aCh && aCh <= 'z') || ('A' <= aCh && aCh <= 'Z') || aCh == '_';
    }

    private static boolean isSQLIdentifierPart(char aCh)
    {
        return isSQLIdentifierStart(aCh) || ('0' <= aCh && aCh <= '9');
    }

    private static int indexOfOrderByList(String aSrcQstr, int aStartIdx)
    {
        for (int i = aStartIdx; i < aSrcQstr.length(); i++)
        {
            switch (aSrcQstr.charAt(i))
            {
                // ' ~ '
                case '\'':
                    for (i++; (i < aSrcQstr.length()) && (aSrcQstr.charAt(i) != '\''); i++)
                        ;
                    break;

                // " ~ "
                case '"':
                    for (i++; (i < aSrcQstr.length()) && (aSrcQstr.charAt(i) != '"'); i++)
                        ;
                    break;

                // ( ~ )
                case '(':
                    for (i++; (i < aSrcQstr.length()) && (aSrcQstr.charAt(i) != ')'); i++)
                        ;
                    break;

                // C & C++ style comments
                case '/':
                    // block comment
                    if (aSrcQstr.charAt(i + 1) == '*')
                    {
                        for (i += 2; i < aSrcQstr.length(); i++)
                        {
                            if ((aSrcQstr.charAt(i) == '*') && (aSrcQstr.charAt(i + 1) == '/'))
                            {
                                i++;
                                break;
                            }
                        }
                    }
                    // line comment
                    else if (aSrcQstr.charAt(i + 1) == '/')
                    {
                        for (i += 2; (i < aSrcQstr.length()) && (aSrcQstr.charAt(i) != '\n'); i++)
                            ; // skip to end-of-line
                    }
                    break;

                // line comment
                case '-':
                    if (aSrcQstr.charAt(i + 1) == '-')
                    {
                        for (i += 2; (i < aSrcQstr.length()) && (aSrcQstr.charAt(i) != '\n'); i++)
                            ; // skip to end-of-line
                    }
                    break;

                case 'O':
                case 'o':
                    // {����}ORDER{����}BY{����} ���� Ȯ��
                    if ((i == 0) || (i + 5 >= aSrcQstr.length()) || !isValidFromPrevChar(aSrcQstr.charAt(i - 1)))
                    {
                        break;
                    }

                    if (!StringUtils.startsWithIgnoreCase(aSrcQstr, i, "ORDER") ||
                        !Character.isWhitespace(aSrcQstr.charAt(i + 5)))
                    {
                        break;
                    }
                    int sOrgIdx = indexOfNonWhitespaceAndComment(aSrcQstr, i + 5);
                    for (i = sOrgIdx; Character.isWhitespace(aSrcQstr.charAt(i)); i++)
                        /* skip white space */;
                    if (!StringUtils.startsWithIgnoreCase(aSrcQstr, i, "BY") ||
                        !isValidFromNextChar(aSrcQstr.charAt(i + 2)))
                    {
                        i = sOrgIdx;
                        break;
                    }
                    return i + 3;

                default:
                    break;
            }
        }
        return INDEX_NOT_FOUND;
    }

    public static String makeRowSetSql(String aSql, int aRowIdCount) throws SQLException
    {
        if (aSql.indexOf(INTERNAL_SQL_PROWID) != INDEX_NOT_FOUND)
        {
            Error.throwInternalError(ErrorDef.ALREADY_CONVERTED);
        }

        int sIdxSelectB = indexOfSelect(aSql);
        if (sIdxSelectB == INDEX_NOT_FOUND)
        {
            return aSql;
        }

        char[] sSqlChArr = aSql.toCharArray();
        StringBuffer sBuf = new StringBuffer(aSql.length());
        int sIdxFromB = indexOfFrom(aSql, sIdxSelectB);
        int sIdxFromE = indexOfFromEnd(aSql, sIdxFromB);
        sBuf.append(sSqlChArr, sIdxSelectB, sIdxFromB - sIdxSelectB);
        sBuf.append(INTERNAL_SQL_PROWID);
        sBuf.append(sSqlChArr, sIdxFromB, sIdxFromE - sIdxFromB);
        sBuf.append(" WHERE _PROWID IN (?");
        for (sIdxFromE = 2; sIdxFromE <= aRowIdCount; sIdxFromE++)
        {
            sBuf.append(",?");
        }
        sBuf.append(")");
        return sBuf.toString();
    }

    /**
     * _PROWID �÷��� �߰��� �������� ��´�.
     * 
     * @param aSql ���� ������
     * @return �ܼ� SELECT���� �ƴϸ� null,
     *         �̹� _PROWID �÷��� �߰��Ǿ������� ���� ������,
     *         _PROWID �÷��� �߰��������� �ܼ� SELECT ���̸� _PROWID �÷��� �߰��� ������
     * @throws SQLException 
     */
    public static String makePRowIDAddedSql(String aSql) throws SQLException
    {
        int sIdxSelectB = indexOfSelect(aSql);
        if (sIdxSelectB == INDEX_NOT_FOUND)
        {
            return null; // ��ȯ�� �� ���� ���������� ���θ� Ȯ���ϱ� ����
        }

        if (aSql.indexOf(INTERNAL_SQL_PROWID) != INDEX_NOT_FOUND)
        {
            return aSql;
        }

        int sIndex = indexOfFrom(aSql, sIdxSelectB);
        return aSql.substring(sIdxSelectB, sIndex)
               + INTERNAL_SQL_PROWID
               + aSql.substring(sIndex, aSql.length());
    }

    /**
     * SELECT�� ���� ��ġ�� ã�´�.
     * 
     * @param aSql ���� ������
     * @return SELECT�� �����ϸ� SELECT�� ���� ��ġ(0 base), �ƴϸ� -1
     */
    private static int indexOfSelect(String aSql)
    {
        int sStartPos = indexOfNonWhitespaceAndComment(aSql);
        if (StringUtils.startsWithIgnoreCase(aSql, sStartPos, "SELECT"))
        {
            return sStartPos;
        }
        return INDEX_NOT_FOUND;
    }

    /**
     * SELECT�� �����ϴ� �������� Ȯ���Ѵ�.
     * 
     * @param aSql Ȯ���� ������
     * @return SELECT�� �����ϴ��� ����
     */
    public static boolean isSelectQuery(String aSql)
    {
        int sIdxSelectB = indexOfSelect(aSql);
        return (sIdxSelectB != INDEX_NOT_FOUND);
    }

    /**
     * INSERT�� �����ϴ� �������� Ȯ���Ѵ�.
     * 
     * @param aSql Ȯ���� ������
     * @return INSERT�� �����ϴ��� ����
     */
    public static boolean isInsertQuery(String aSql)
    {
        int sStartPos = indexOfNonWhitespaceAndComment(aSql);
        return StringUtils.startsWithIgnoreCase(aSql, sStartPos, "INSERT");
    }

    public static String makeDeleteRowSql(String aTableName)
    {
        return "DELETE FROM " + aTableName + " WHERE _PROWID=?";
    }
}
