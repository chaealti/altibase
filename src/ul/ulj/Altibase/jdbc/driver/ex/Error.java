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

package Altibase.jdbc.driver.ex;

import java.io.IOException;
import java.sql.BatchUpdateException;
import java.sql.SQLException;
import java.sql.SQLFeatureNotSupportedException;
import java.sql.SQLWarning;
import java.util.ArrayList;
import java.util.List;

import javax.transaction.xa.XAException;
import javax.transaction.xa.XAResource;
import javax.transaction.xa.Xid;

import Altibase.jdbc.driver.cm.*;
import Altibase.jdbc.driver.util.WrappedIOException;

public final class Error
{
    private static String getMessage(Throwable aThrowable)
    {
        return (aThrowable.getMessage() == null) ? aThrowable.toString() : aThrowable.getMessage();
    }

    private static String buildErrorMessage(int aJDBCErrorCode)
    {
        return ErrorDef.getErrorMessage(aJDBCErrorCode);
    }

    private static String buildErrorMessage(int aJDBCErrorCode, String aArg1)
    {
        StringBuffer sMessageBuffer = new StringBuffer(ErrorDef.getErrorMessage(aJDBCErrorCode));
        replaceErrorDefStr(sMessageBuffer, aArg1);
        return sMessageBuffer.toString();
    }

    private static String buildErrorMessage(int aJDBCErrorCode, String aArg1, String aArg2)
    {
        StringBuffer sMessageBuffer = new StringBuffer(ErrorDef.getErrorMessage(aJDBCErrorCode));
        replaceErrorDefStr(sMessageBuffer, aArg1);
        replaceErrorDefStr(sMessageBuffer, aArg2);
        return sMessageBuffer.toString();
    }

    private static String buildErrorMessage(int aJDBCErrorCode, String aArg1, String aArg2, String aArg3)
    {
        StringBuffer sMessageBuffer = new StringBuffer(ErrorDef.getErrorMessage(aJDBCErrorCode));
        replaceErrorDefStr(sMessageBuffer, aArg1);
        replaceErrorDefStr(sMessageBuffer, aArg2);
        replaceErrorDefStr(sMessageBuffer, aArg3);
        return sMessageBuffer.toString();
    }

    /**
     * PROJ-2427 StringBuffer�� �̿��Ͽ� %���� ġȯ�۾��� �����Ѵ�.
     */
    private static void replaceErrorDefStr(StringBuffer aMessage, String aArg)
    {
        int sIdx = aMessage.indexOf("%s");
        if (sIdx > -1)
        {
            if (aArg == null)
            {
                aArg = "";
            }
            aMessage.replace(sIdx, sIdx+2, aArg);
        }
    }

    private static String buildXaErrorMessage(int aXaErrorCode, Throwable aCause)
    {
        String sMessage = ErrorDef.getXAErrorMessage(aXaErrorCode);
        if (aCause != null)
        {
            sMessage = sMessage + ": " + getMessage(aCause);
        }
        return sMessage;
    }

    /**
     * JDBCErrorCode�� �ش��ϴ� SQLException�� ������.
     *
     * @param aJDBCErrorCode ���ܸ� ����µ� ����� ���� �ڵ�
     * @throws SQLException
     */
    public static void throwSQLException(int aJDBCErrorCode) throws SQLException
    {
        // PROJ-2427 String[]�� ������� �ʱ� ���� ���� buildErrorMessage�� �� ����� �Ѱ��ش�.
        throwSQLExceptionInternal(aJDBCErrorCode, buildErrorMessage(aJDBCErrorCode, null), null);
    }

    /**
     * JDBCErrorCode�� �ش��ϴ� SQLException�� ������.
     *
     * @param aJDBCErrorCode ���ܸ� ����µ� ����� ���� �ڵ�
     * @param aCause ���� ���� ����
     * @throws SQLException
     */
    public static void throwSQLException(int aJDBCErrorCode, Throwable aCause) throws SQLException
    {
        throwSQLExceptionInternal(aJDBCErrorCode, buildErrorMessage(aJDBCErrorCode, null), aCause);
    }

    /**
     * JDBCErrorCode�� �ش��ϴ� SQLException�� ������.
     *
     * @param aJDBCErrorCode ���ܸ� ����µ� ����� ���� �ڵ�
     * @param aArg �޽����� ù��° ���ڿ� ������ ���ڰ�
     * @throws SQLException
     */
    public static void throwSQLException(int aJDBCErrorCode, String aArg) throws SQLException
    {
        throwSQLExceptionInternal(aJDBCErrorCode, buildErrorMessage(aJDBCErrorCode, aArg), null);
    }

    /**
     * JDBCErrorCode�� �ش��ϴ� SQLException�� ������.
     *
     * @param aJDBCErrorCode ���ܸ� ����µ� ����� ���� �ڵ�
     * @param aArg �޽����� ù��° ���ڿ� ������ ���ڰ�
     * @param aCause ���� ���� ����
     * @throws SQLException
     */
    public static void throwSQLException(int aJDBCErrorCode, String aArg, Throwable aCause) throws SQLException
    {
        throwSQLExceptionInternal(aJDBCErrorCode, buildErrorMessage(aJDBCErrorCode, aArg), aCause);
    }

    /**
     * JDBCErrorCode�� �ش��ϴ� SQLException�� ������.
     *
     * @param aJDBCErrorCode ���ܸ� ����µ� ����� ���� �ڵ�
     * @param aArg1 �޽����� 1��° ���ڿ� ������ ���ڰ�
     * @param aArg2 �޽����� 2��° ���ڿ� ������ ���ڰ�
     * @throws SQLException
     */
    public static void throwSQLException(int aJDBCErrorCode, String aArg1, String aArg2) throws SQLException
    {
        throwSQLExceptionInternal(aJDBCErrorCode, buildErrorMessage(aJDBCErrorCode, aArg1, aArg2), null);
    }

    /**
     * JDBCErrorCode�� �ش��ϴ� SQLException�� ������.
     *
     * @param aJDBCErrorCode ���ܸ� ����µ� ����� ���� �ڵ�
     * @param aArg1 �޽����� 1��° ���ڿ� ������ ���ڰ�
     * @param aArg2 �޽����� 2��° ���ڿ� ������ ���ڰ�
     * @param aCause ���� ���� ����
     * @throws SQLException
     */
    public static void throwSQLException(int aJDBCErrorCode, String aArg1, String aArg2, Throwable aCause) throws SQLException
    {
        throwSQLExceptionInternal(aJDBCErrorCode, buildErrorMessage(aJDBCErrorCode, aArg1, aArg2), aCause);
    }

    /**
     * JDBCErrorCode�� �ش��ϴ� SQLException�� ������.
     *
     * @param aJDBCErrorCode ���ܸ� ����µ� ����� ���� �ڵ�
     * @param aArg1 �޽����� 1��° ���ڿ� ������ ���ڰ�
     * @param aArg2 �޽����� 2��° ���ڿ� ������ ���ڰ�
     * @param aArg2 �޽����� 3��° ���ڿ� ������ ���ڰ�
     * @throws SQLException
     */
    public static void throwSQLException(int aJDBCErrorCode, String aArg1, String aArg2, String aArg3) throws SQLException
    {
        throwSQLExceptionInternal(aJDBCErrorCode, buildErrorMessage(aJDBCErrorCode, aArg1, aArg2, aArg3), null);
    }

    private static void throwSQLExceptionInternal(int aJDBCErrorCode, String aErrorMessage, Throwable aCause) throws SQLException
    {
        // PROJ-2427 aErroMessage �޼����� buildErrorMessage�� �̿����� �ʰ� �Ű������� ���� �޴´�.
        throw createSQLExceptionInternal(aJDBCErrorCode, aErrorMessage, aCause);
    }

    private static SQLException createSQLExceptionInternal(int aJDBCErrorCode, String aErrorMessage,
                                                           Throwable aCause)
    {
        SQLException sException = new SQLException(aErrorMessage,
                                                   ErrorDef.getErrorState(aJDBCErrorCode),
                                                   aJDBCErrorCode);
        if (aCause != null)
        {
            sException.initCause(aCause);
        }
        return sException;
    }

    public static SQLException createSQLException(int aJDBCErrorCode)
    {
        return createSQLExceptionInternal(aJDBCErrorCode, buildErrorMessage(aJDBCErrorCode, null), null);
    }

    public static SQLException createSQLException(int aJDBCErrorCode, String aArg1)
    {
        return createSQLExceptionInternal(aJDBCErrorCode, buildErrorMessage(aJDBCErrorCode, aArg1), null);
    }

    public static SQLException createSQLException(int aJDBCErrorCode, String aArg1, String aArg2)
    {
        return createSQLExceptionInternal(aJDBCErrorCode, buildErrorMessage(aJDBCErrorCode, aArg1, aArg2), null);
    }

    public static SQLException createSQLException(int aJDBCErrorCode, String aArg1, Throwable aCause)
    {
        return createSQLExceptionInternal(aJDBCErrorCode, buildErrorMessage(aJDBCErrorCode, aArg1), aCause);
    }

    /**
     * {@link BatchUpdateException}�� ������.
     *
     * @param aJDBCErrorCode ���ܸ� ����µ� ����� ���� �ڵ�
     * @throws BatchUpdateException
     */
    public static void throwBatchUpdateException(int aJDBCErrorCode) throws BatchUpdateException
    {
        throwBatchUpdateException(aJDBCErrorCode, new int[0]);
    }

    /**
     * {@link BatchUpdateException}�� ������.
     *
     * @param aJDBCErrorCode ���ܸ� ����µ� ����� ���� �ڵ�
     * @param aUpdateCounts �� ������ ���� update counts
     * @throws BatchUpdateException
     */
    public static void throwBatchUpdateException(int aJDBCErrorCode, int[] aUpdateCounts) throws BatchUpdateException
    {
        BatchUpdateException sException = new BatchUpdateException(buildErrorMessage(aJDBCErrorCode),
                                                                   ErrorDef.getErrorState(aJDBCErrorCode),
                                                                   aJDBCErrorCode,
                                                                   aUpdateCounts);
        throw sException;
    }

    /**
     * SQLException�� BatchUpdateException���� �����ؼ� ������.
     *
     * @param aCause ���� ���� ����
     * @param aUpdateCounts �� ������ ���� update count
     * @throws BatchUpdateException
     */
    public static void throwBatchUpdateException(SQLException aCause, int[] aUpdateCounts) throws BatchUpdateException
    {
        BatchUpdateException sBatchEx = new BatchUpdateException(buildErrorMessage(ErrorDef.BATCH_UPDATE_EXCEPTION_OCCURRED, getMessage(aCause)),
                                                                 ErrorDef.getErrorState(ErrorDef.BATCH_UPDATE_EXCEPTION_OCCURRED),
                                                                 ErrorDef.BATCH_UPDATE_EXCEPTION_OCCURRED,
                                                                 aUpdateCounts);
        sBatchEx.setNextException(aCause);
        throw sBatchEx;
    }

    /**
     * communication���� ���� ���ܸ� {@link SQLException}���� �����ؼ� �����ش�.
     * ����, ���� ���ܰ� SQLException�̶�� �׳� ���� ���ܸ� ������.
     * <p>
     * ���� ���ܰ� SQLException�� �ƴϸ� SQLException���� �ѹ� �����ָ�,
     * ���� �޽��� �տ� 'Communication link failure'�� �����δ�.
     *
     * @param aCause ���� ���� ����
     * @throws SQLException 
     */
    public static void throwCommunicationErrorException(Throwable aCause) throws SQLException
    {
        if (aCause instanceof WrappedIOException)
        {
            aCause = aCause.getCause();
        }
        if (aCause instanceof SQLException)
        {
            throw (SQLException) aCause;
        }

        SQLException sException = new SQLException(buildErrorMessage(ErrorDef.COMMUNICATION_ERROR, getMessage(aCause)),
                                                   ErrorDef.getErrorState(ErrorDef.COMMUNICATION_ERROR),
                                                   ErrorDef.COMMUNICATION_ERROR);
        sException.initCause(aCause);
        throw sException;
    }

    public static void throwSQLExceptionForIOException(Throwable aCause) throws SQLException
    {
        SQLException sException = new SQLException(buildErrorMessage(ErrorDef.IOEXCEPTION_FROM_INPUTSTREAM, getMessage(aCause)),
                                                   ErrorDef.getErrorState(ErrorDef.IOEXCEPTION_FROM_INPUTSTREAM),
                                                   ErrorDef.IOEXCEPTION_FROM_INPUTSTREAM);
        sException.initCause(aCause);
        throw sException;
    }
    
    /**
     * Failover success�� ������ ���ܸ� ������.
     * <p>
     * �޽����� ��� 'Failover success'��� �����Ƿ�, ��� �ϴٰ� �� ��쿡�� �� ��.
     *
     * @param aCause ���� ���� ����
     * @throws SQLException
     */
    public static void throwSQLExceptionForFailover(SQLException aCause) throws SQLException
    {
        Error.throwSQLException(ErrorDef.FAILOVER_SUCCESS, aCause.getSQLState(), getMessage(aCause), aCause);
    }

    /**
     * �������� ���� ������ �ֳ� Ȯ���Ѵ�.
     * <p>
     * next error���� ��� Ȯ���Ѵ�. 
     *
     * @param aErrorResult �������� ���� ����/��� ����
     * @return ������ �ִ��� ����
     */
    public static boolean hasServerError(CmErrorResult aErrorResult)
    {
        for (; aErrorResult != null; aErrorResult = aErrorResult.getNextError())
        {
            if (!aErrorResult.isIgnorable())
            {
                return true;
            }
        }
        return false;
    }

    /**
     * �������� ���� ����/��� ������ ó���Ѵ�.
     * ������ ������ SQLException���� �����ؼ� ������, ���� aWarning�� chain���� �����Ѵ�.
     * <p>
     * ����, �������� ���� ������ ��������� �̸� chain���� �����Ѵ�.
     *
     * @param aWarning ��� ������ ��ü. null�̸� ���� �����.
     * @param aErrorResult �������� ���� ����/��� ����
     * @return aWarning�� null�̸� ���� ���� ��ü, �ƴϸ� aWarning
     * @throws SQLException �������� ���� ������ 1���� �ִ� ���
     */
    public static SQLWarning processServerError(SQLWarning aWarning,
                                                CmErrorResult aErrorResult) throws SQLException
    {
        List<SQLException> sExceptionList = new ArrayList<SQLException>();
        List<SQLWarning> sWarningList = new ArrayList<SQLWarning>();

        while (aErrorResult != null)
        {
            int sErrorCode = toClientSideErrorCode(aErrorResult.getErrorCode());
            String sErrorMsg = aErrorResult.getErrorMessage();
            String sSQLState = SQLStateMap.getSQLState(sErrorCode);

            // _PROWID�� ���� �÷��̹Ƿ� �̿����� ���� �޽����� �ٲ㼭 �����Ѵ�.
            switch (sErrorCode)
            {
                case ErrorDef.CANNOT_SELECT_PROWID:
                case ErrorDef.PROWID_NOT_SUPPORTED:
                    sErrorCode = ErrorDef.INVALID_QUERY_STRING;
                    sErrorMsg = ErrorDef.getErrorMessage(sErrorCode);
                    break;

                case ErrorDef.PASSWORD_GRACE_PERIOD:
                    // BUG-38496 Notify users when their password expiry date is approaching.
                    sErrorCode = ErrorDef.PASSWORD_EXPIRATION_DATE_IS_COMING;
                    sErrorMsg = Error.buildErrorMessage(sErrorCode, String.valueOf(aErrorResult.getErrorIndex()));
                    sSQLState = SQLStateMap.getSQLState(sErrorCode);
                    break;

                default:
                    break;
            }

            if (!aErrorResult.isIgnorable())
            {
                if (aErrorResult.isInvalidSMNError())
                {
                    ShardError.processInvalidSMNError(aErrorResult, sExceptionList, sWarningList,
                                                      sErrorCode, sErrorMsg, sSQLState);
                }
                else if (aErrorResult.isShardModuleError())
                {
                    // BUG-46790 �����޼����� �Ľ��Ͽ� failover align������ �����Ѵ�.
                    ShardError.parseShardingError(sExceptionList, aErrorResult, sErrorCode,
                                                  sErrorMsg, sSQLState);
                }
                else
                {
                    sExceptionList.add(new SQLException(sErrorMsg, sSQLState, sErrorCode));
                }
            }
            // BUG-44471 ���� �������� ���� ignore ������ �ƴҶ��� SQLWarning�� �����Ѵ�.
            else if (!aErrorResult.canSkipSQLWarning(sErrorCode))
            {
                sWarningList.add(new SQLWarning(sErrorMsg, sSQLState, sErrorCode));
            }
            aErrorResult = aErrorResult.getNextError();
        }

        SQLWarning sWarning = makeSQLWarningFromList(sWarningList);
        if (aWarning != null)
        {
            aWarning.setNextWarning(sWarning);
        }
        else
        {
            aWarning = sWarning;
        }

        SQLException sExceptionResult = makeSQLExceptionFromList(sExceptionList);
        if (sExceptionResult != null)
        {
            throw sExceptionResult;
        }

        return aWarning;
    }

    private static SQLWarning makeSQLWarningFromList(List<SQLWarning> aWarningList)
    {
        if (aWarningList.size() == 0)
        {
            return null;
        }
        SQLWarning sResult = aWarningList.get(0);
        for (int i = 1; i < aWarningList.size(); i++)
        {
            sResult.setNextWarning(aWarningList.get(i));
        }

        return sResult;
    }

    public static SQLException makeSQLExceptionFromList(List<SQLException> aExceptionList)
    {
        if (aExceptionList.size() == 0)
        {
            return null;
        }
        SQLException sResult = aExceptionList.get(0);
        for (int i = 1; i < aExceptionList.size(); i++)
        {
            sResult.setNextException(aExceptionList.get(i));
        }

        return sResult;
    }


    public static int toClientSideErrorCode(int aErrorCode)
    {
        return (aErrorCode >>> 12);
    }

    public static int toServerSideErrorCode(int aErrorCode, int aSeq)
    {
        return (aErrorCode << 12) | (aSeq & 0xFFF);
    }
    
    /**
     * This checks if there is a serious exception which must be handled as an error by user applications. 
     * If there is no error or only warnings exist, then  this will ignore it. 
     * @param aErrorResult  Errors happened while processing user requests. 
     * @return If there is a serious exception, then true will be returned. 
     */
    public static boolean isException(CmErrorResult aErrorResult)
    {
        boolean sRet = false;
        
        while (aErrorResult != null)
        {
            if(aErrorResult.isIgnorable() != true)
            {
                sRet = true;
                break;
            }
            aErrorResult = aErrorResult.getNextError();
        }
        
        return sRet;
    }

    private static XAException getXAException(CmXAResult aXAResult) /* BUG-42723 */
    {
        if (aXAResult == null)
        {
            return null;
        }

        XAException sException = null;
        int sXAResultValue = aXAResult.getResultValue();

        if (sXAResultValue != XAResource.XA_OK && sXAResultValue != XAResource.XA_RDONLY)
        {
            sException = new XAException(ErrorDef.getXAErrorMessage(aXAResult.getResultValue()));
            sException.errorCode = aXAResult.getResultValue();
        }

        return sException;
    }

    /**
     * �������� ���� ���� ������ XA ����� Ȯ���ϰ�, �ʿ��ϸ� ���ܸ� ������.
     *
     * @param aErrorResult �������� ���� ����/��� ����
     * @param aXAResult �������� ���� XA ���
     * @throws XAException �������� ���� ������ �ִ� ���
     */
    public static void processXaError(CmErrorResult aErrorResult, CmXAResult aXAResult) throws XAException
    {
        XAException sException = getXAException(aXAResult);

        if (aErrorResult != null && !aErrorResult.isIgnorable())
        {
            XAException sXaErr = new XAException(ErrorDef.getXAErrorMessageWith(aErrorResult.getErrorMessage()));
            sXaErr.errorCode = XAException.XAER_PROTO;
            if (sException == null)
            {
                sException = sXaErr;
            }
            else
            {
                sException.initCause(sXaErr);
            }
        }
        if (sException != null)
        {
            throw sException;
        }
    }

    /**
     * Xid�� �ùٸ��� Ȯ���ϰ�, �ʿ��ϸ� ���ܸ� ������.
     *
     * @param aXid �ùٸ��� Ȯ���� Xid
     * @throws XAException Xid�� �ùٸ��� ���� ���
     */
    public static void checkXidAndThrowXAException(Xid aXid) throws XAException
    {
        if (aXid == null) throwXAException(XAException.XAER_INVAL);

        byte[] sGlobalTransnId = aXid.getGlobalTransactionId();
        byte[] sBranchQualifier = aXid.getBranchQualifier();
        if (sGlobalTransnId == null || sGlobalTransnId.length > Xid.MAXGTRIDSIZE ||
            sBranchQualifier == null || sBranchQualifier.length > Xid.MAXBQUALSIZE)
        {
            throwXAException(XAException.XAER_NOTA);
        }
    }

    /**
     * XAException ���ܸ� ������.
     *
     * @param aJDBCErrorCode ���� �ڵ�
     * @param aXaResult XA Result
     * @throws XAException 
     */
    public static void throwXAException(int aJDBCErrorCode, int aXaResult) throws XAException
    {
        XAException sXaEx = new XAException(buildErrorMessage(aJDBCErrorCode, String.valueOf(aXaResult)));
        sXaEx.errorCode = aXaResult;
        throw sXaEx;
    }

    /**
     * SQLException�� XAException���� ���� ������.
     *
     * @param aCause ���� ���� ����
     * @throws XAException
     */
    public static void throwXaException(SQLException aCause) throws XAException
    {
        throwXAException(XAException.XAER_PROTO, aCause);
    }

    private static void throwXAException(int aXaErrorCode) throws XAException
    {
        throwXAException(aXaErrorCode, null);
    }

    private static void throwXAException(int aXaErrorCode, Throwable aCause) throws XAException
    {
        XAException sXaEx = new XAException(buildXaErrorMessage(aXaErrorCode, aCause));
        sXaEx.errorCode = aXaErrorCode;
        if (aCause != null)
        {
            sXaEx.initCause(aCause);
        }
        throw sXaEx;
    }

    /**
     * JDBCErrorCode�� �ش��ϴ� SQLWarning�� �����.
     *
     * @param aParent ��� ������ ��ü. null�̸� ���� �����.
     * @param aJDBCErrorCode ��� ����µ� ����� ���� �ڵ�
     * @return ���� ���� SQLWarning ��ü
     */
    public static SQLWarning createWarning(SQLWarning aParent, int aJDBCErrorCode)
    {
        return createWarningInternal(aParent, aJDBCErrorCode, buildErrorMessage(aJDBCErrorCode), null);
    }

    /**
     * JDBCErrorCode�� �ش��ϴ� SQLWarning�� �����.
     *
     * @param aParent ��� ������ ��ü. null�̸� ���� �����.
     * @param aJDBCErrorCode ��� ����µ� ����� ���� �ڵ�
     * @param aCause ���� ���� ����
     * @return ���� ���� SQLWarning ��ü
     */
    public static SQLWarning createWarning(SQLWarning aParent, int aJDBCErrorCode, Throwable aCause)
    {
        return createWarningInternal(aParent, aJDBCErrorCode, buildErrorMessage(aJDBCErrorCode), aCause);
    }

    /**
     * JDBCErrorCode�� �ش��ϴ� SQLWarning�� �����.
     *
     * @param aParent ��� ������ ��ü. null�̸� ���� �����.
     * @param aJDBCErrorCode ��� ����µ� ����� ���� �ڵ�
     * @param aArg �޽����� ù��° ���ڿ� ������ ���ڰ�
     * @return ���� ���� SQLWarning ��ü
     */
    public static SQLWarning createWarning(SQLWarning aParent, int aJDBCErrorCode, String aArg)
    {
        return createWarningInternal(aParent, aJDBCErrorCode, buildErrorMessage(aJDBCErrorCode, aArg), null);
    }

    /**
     * JDBCErrorCode�� �ش��ϴ� SQLWarning�� �����.
     *
     * @param aParent ��� ������ ��ü. null�̸� ���� �����.
     * @param aJDBCErrorCode ��� ����µ� ����� ���� �ڵ�
     * @param aArg �޽����� ù��° ���ڿ� ������ ���ڰ�
     * @param aCause ���� ���� ����
     * @return ���� ���� SQLWarning ��ü
     */
    public static SQLWarning createWarning(SQLWarning aParent, int aJDBCErrorCode, String aArg, Throwable aCause)
    {
        return createWarningInternal(aParent, aJDBCErrorCode, buildErrorMessage(aJDBCErrorCode, aArg), aCause);
    }

    /**
     * JDBCErrorCode�� �ش��ϴ� SQLWarning�� �����.
     *
     * @param aParent ��� ������ ��ü. null�̸� ���� �����.
     * @param aJDBCErrorCode ��� ����µ� ����� ���� �ڵ�
     * @param aArg1 �޽����� 1��° ���ڿ� ������ ���ڰ�
     * @param aArg2 �޽����� 2��° ���ڿ� ������ ���ڰ�
     * @return ���� ���� SQLWarning ��ü
     */
    public static SQLWarning createWarning(SQLWarning aParent, int aJDBCErrorCode, String aArg1, String aArg2)
    {
        return createWarningInternal(aParent, aJDBCErrorCode, buildErrorMessage(aJDBCErrorCode, aArg1, aArg2), null);
    }

    /**
     * JDBCErrorCode�� �ش��ϴ� SQLWarning�� �����.
     *
     * @param aParent ��� ������ ��ü. null�̸� ���� �����.
     * @param aJDBCErrorCode ��� ����µ� ����� ���� �ڵ�
     * @param aArg1 �޽����� 1��° ���ڿ� ������ ���ڰ�
     * @param aArg2 �޽����� 2��° ���ڿ� ������ ���ڰ�
     * @param aCause ���� ���� ����
     * @return ���� ���� SQLWarning ��ü
     */
    public static SQLWarning createWarning(SQLWarning aParent, int aJDBCErrorCode, String aArg1, String aArg2, Throwable aCause)
    {
        return createWarningInternal(aParent, aJDBCErrorCode, buildErrorMessage(aJDBCErrorCode, aArg1, aArg2), aCause);
    }

    private static SQLWarning createWarningInternal(SQLWarning aParent, int aJDBCErrorCode, String aErrorMessage, Throwable aCause)
    {
        SQLWarning sWarning = new SQLWarning(aErrorMessage,
                                             ErrorDef.getErrorState(aJDBCErrorCode),
                                             aJDBCErrorCode);
        if (aCause != null)
        {
            sWarning.initCause(aCause);
        }

        if (aParent == null)
        {
            aParent = sWarning;
        }
        else
        {
            aParent.setNextWarning(sWarning);
        }
        return aParent;
    }

    public static void throwInternalError(int aJDBCErrorCode)
    {
        throwInternalErrorImpl(aJDBCErrorCode, buildErrorMessage(aJDBCErrorCode), null);
    }

    public static void throwInternalError(int aJDBCErrorCode, Throwable aCause)
    {
        throwInternalErrorImpl(aJDBCErrorCode, buildErrorMessage(aJDBCErrorCode), aCause);
    }

    public static void throwInternalError(int aJDBCErrorCode, String aArg1)
    {
        throwInternalErrorImpl(aJDBCErrorCode, buildErrorMessage(aJDBCErrorCode, aArg1), null);
    }

    public static void throwInternalError(int aJDBCErrorCode, String aArg1, Throwable aCause)
    {
        throwInternalErrorImpl(aJDBCErrorCode, buildErrorMessage(aJDBCErrorCode, aArg1), aCause);
    }

    public static void throwInternalError(int aJDBCErrorCode, String aArg1, String aArg2)
    {
        throwInternalErrorImpl(aJDBCErrorCode, buildErrorMessage(aJDBCErrorCode, aArg1, aArg2), null);
    }

    public static void throwInternalError(int aJDBCErrorCode, String aArg1, String aArg2, Throwable aCause)
    {
        throwInternalErrorImpl(aJDBCErrorCode, buildErrorMessage(aJDBCErrorCode, aArg1, aArg2), aCause);
    }

    public static void throwInternalError(int aJDBCErrorCode, String aArg1, String aArg2, String aArg3)
    {
        throwInternalErrorImpl(aJDBCErrorCode, buildErrorMessage(aJDBCErrorCode, aArg1, aArg2, aArg3), null);
    }

    private static void throwInternalErrorImpl(int aJDBCErrorCode, String aErrorMessage, Throwable aCause)
    {
        InternalError sError = new InternalError(aErrorMessage, aJDBCErrorCode);
        if (aCause != null)
        {
            sError.initCause(aCause);
        }
        throw sError;
    }

    public static void throwIOException(int aJDBCErrorCode) throws IOException
    {
        throw new IOException(buildErrorMessage(aJDBCErrorCode));
    }

    public static void throwIOException(Throwable aCause) throws IOException
    {
        throw new WrappedIOException(aCause);
    }

    public static void throwIllegalArgumentException(int aJDBCErrorCode)
    {
        throwIllegalArgumentExceptionInternal(aJDBCErrorCode, buildErrorMessage(aJDBCErrorCode));
    }

    public static void throwIllegalArgumentException(int aJDBCErrorCode, String aArg1)
    {
        throwIllegalArgumentExceptionInternal(aJDBCErrorCode, buildErrorMessage(aJDBCErrorCode, aArg1));
    }

    public static void throwIllegalArgumentException(int aJDBCErrorCode, String aArg1, String aArg2)
    {
        throwIllegalArgumentExceptionInternal(aJDBCErrorCode, buildErrorMessage(aJDBCErrorCode, aArg1, aArg2));
    }

    public static void throwIllegalArgumentException(int aJDBCErrorCode, String aArg1, String aArg2, String aArg3)
    {
        throwIllegalArgumentExceptionInternal(aJDBCErrorCode, buildErrorMessage(aJDBCErrorCode, aArg1, aArg2, aArg3));
    }

    private static void throwIllegalArgumentExceptionInternal(int aJDBCErrorCode, String aErrorMessage)
    {
        throw new IllegalArgumentException(aErrorMessage);
    }

    public static SQLException createSQLFeatureNotSupportedException()
    {
        return new SQLFeatureNotSupportedException();
    }
}
