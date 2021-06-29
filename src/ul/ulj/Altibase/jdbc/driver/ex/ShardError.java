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
 *
 */

package Altibase.jdbc.driver.ex;

import Altibase.jdbc.driver.AltibaseConnection;
import Altibase.jdbc.driver.cm.*;
import Altibase.jdbc.driver.sharding.core.*;
import Altibase.jdbc.driver.util.StringUtils;

import java.math.BigInteger;
import java.sql.Connection;
import java.sql.SQLException;
import java.sql.SQLWarning;
import java.util.ArrayList;
import java.util.List;
import java.util.logging.Level;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

import static Altibase.jdbc.driver.sharding.util.ShardingTraceLogger.shard_log;

/**
 * Sharding ���õ� ���� �޼����� �Ľ��Ͽ� SMN, Failover Align �� ���� �۾����� ó���Ѵ�.
 */
public class ShardError
{
    private static final String DATA_NODE_SMN_PREFIX                = "Data Node SMN=";
    private static final String SHARD_DISCONNECT_PREFIX             = "Disconnect=N";
    private static final String EXECUTE_RESULT_PREFIX               = "ExecuteV2Result";   // ErrorResult�� ErrMsg. �������� �������� ��Ʈ���� �޸� ���� �ʰ� ��ǥ ��Ʈ���� ��� 

    // BUG-46790 �����޼����κ��� node id, error code�� �ٷ� �������� ���� ����ǥ���� ����
    private static final String SHARD_FOOT_PRINT_REGULAR_EXPRESSION = ".*?(\\[<<ERC-(\\w*) NODE-ID:(\\d*)>>])";

    /**
     * �����κ��� �Ѿ�� SMN Invalid ������ ó���Ѵ�.
     * @param aErrorResult �������� ���� ����/��� ����
     * @param aExceptionList SQLException ����Ʈ
     * @param aWarningList SQLWarning ����Ʈ
     * @param aErrorCode �����ڵ�
     * @param aErrorMsg �����޼���
     * @param aSQLState SQLState
     */
    static void processInvalidSMNError(CmErrorResult aErrorResult, List<SQLException> aExceptionList,
                                       List<SQLWarning> aWarningList,
                                       int aErrorCode, String aErrorMsg, String aSQLState)
    {
        // BUG-46513 �����޼����� �Ľ��� SMN���� NeedToDisconnect���� ���´�.
        setShardMetaNumberOfDataNode(aErrorResult, aErrorMsg);
        boolean sNeedToDisconnect = setShardDisconnectValue(aErrorResult, aErrorMsg);

        switch (aErrorResult.getErrorOp())
        {
            case CmOperationDef.DB_OP_SET_PROPERTY_V3 :
                if (sNeedToDisconnect)
                {
                    aExceptionList.add(new SQLException(aErrorMsg, aSQLState, aErrorCode));
                }
                else
                {
                    aWarningList.add(new SQLWarning(aErrorMsg, aSQLState, aErrorCode));
                }
                break;
            case CmOperationDef.DB_OP_EXECUTE_V3:
            case CmOperationDef.DB_OP_PARAM_DATA_IN_LIST_V3:
                aWarningList.add(new SQLWarning(aErrorMsg, aSQLState, aErrorCode));
                parseExecuteResults(aErrorResult.getProtocolContext(), aErrorMsg);
                break;
            case CmOperationDef.DB_OP_TRANSACTION:
            case CmOperationDef.DB_OP_SHARD_TRANSACTION_V3:
                aWarningList.add(new SQLWarning(aErrorMsg, aSQLState, aErrorCode));
                break;
            default:
                break;
        }
    }

    /**
     * �����޼����� �Ľ��Ͽ� ���� SMN���� node Ŀ�ؼ� �� meta Ŀ�ؼǿ� �ݿ��Ѵ�.
     * @param aErrorResult �������� ���� ����/��� ����
     * @param aErrorMsg �����޼���
     */
    private static void setShardMetaNumberOfDataNode(CmErrorResult aErrorResult, String aErrorMsg)
    {
        long sSMNOfDataNode = getSMNOfDataNodeFromErrorMsg(aErrorMsg);
        aErrorResult.setSMNOfDataNode(sSMNOfDataNode);
    }

    /**
     * �����޼����� �Ľ��Ͽ� SMN���� ���Ѵ�.
     * @param aErrorMsg �����޼���
     * @return SMN��
     */
    static long getSMNOfDataNodeFromErrorMsg(String aErrorMsg)
    {
        if (StringUtils.isEmpty(aErrorMsg)) return 0;

        int sSMNIndex = aErrorMsg.indexOf(DATA_NODE_SMN_PREFIX);
        if (sSMNIndex < 0)  return 0;

        String sSMNRemainStr = aErrorMsg.substring(sSMNIndex + DATA_NODE_SMN_PREFIX.length());
        StringBuilder sSb = new StringBuilder();
        for (char sEach : sSMNRemainStr.toCharArray())
        {
            if (Character.isDigit(sEach))
            {
                sSb.append(sEach);
            }
            else
            {
                break;
            }
        }

        return Long.parseLong(sSb.toString());
    }

    /**
     * SMN Invalid ������ �߻����� �� �����޼����� �Ľ��Ͽ� executeV2�� ����� �����Ѵ�.<br>
     * ExecuteV2Result���� �� : ExecuteV2Result 65536 1 1 18446744073709551615 0 0
     * @param aContext �������� ���ؽ�Ʈ ��ü
     * @param aErrorMsg �����޼���
     */
    static void parseExecuteResults(CmProtocolContext aContext, String aErrorMsg)
    {
        CmExecutionResult sExecResult = (CmExecutionResult)aContext.getCmResult(CmOperation.DB_OP_EXECUTE_V3_RESULT);
        int sIndex = aErrorMsg.indexOf(EXECUTE_RESULT_PREFIX);
        sIndex += EXECUTE_RESULT_PREFIX.length() + 1;
        String sRemain = aErrorMsg.substring(sIndex);
        String[] sValues = sRemain.split(" ");

        sExecResult.setStatementId(Integer.parseInt(sValues[0]));
        sExecResult.setRowNumber(Integer.parseInt(sValues[1]));
        sExecResult.setResultSetCount(Integer.parseInt(sValues[2]));
        String sUpdatedRowCount = sValues[3];
        // BUG-46513 updatedRowCount�� unsigned long ������ text�� �Ѿ� ���� ������ BigInteger�� ó���Ѵ�.
        BigInteger sBigInt = new BigInteger(sUpdatedRowCount);
        sExecResult.setUpdatedRowCount(sBigInt.longValue());
    }

    /**
     * �����޼����� �Ľ��Ͽ� Disconnect���� �ݿ��Ѵ�.
     * @param aErrorResult �������� ���� ����/��� ����
     * @param aErrorMsg ���� �޼���
     */
    private static boolean setShardDisconnectValue(CmErrorResult aErrorResult, String aErrorMsg)
    {
        boolean sNeedToDisconnect = getShardDiconnectFromErrorMsg(aErrorMsg);
        aErrorResult.setNeedToDisconnect(sNeedToDisconnect);

        return sNeedToDisconnect;
    }

    /**
     * �����޼����� �Ľ��Ͽ� ShardDisconnect ���� ���Ѵ�.
     * @param aErrorMsg �����޼���
     * @return needToDisconnect���� <br>
     *         false  : ���� SMN�� ���� Connection�� ���(default) <br>
     *         true   : ���� SMN�� ���� Connection�� ������� ����
     */
    static boolean getShardDiconnectFromErrorMsg(String aErrorMsg)
    {
        if (StringUtils.isEmpty(aErrorMsg)) return false;

        boolean sIsDisconnect = true;
        if (aErrorMsg.contains(SHARD_DISCONNECT_PREFIX))
        {
            sIsDisconnect = false;
        }

        return sIsDisconnect;
    }

    /**
     * shard sd ��� �����޼����� ��¡�Ͽ� �����ڵ� �� nodeID���� errorResult��ü�� �����Ѵ�.
     * @param aExceptionList SQLException�� ����Ǵ� ArrayList
     * @param aErrorResult �������� ���� ����/��� ����
     * @param aErrorCode �����ڵ�
     * @param aErrorMsg footprint [<<ERC-NNNNN NODE-ID:NNN>>] �� ���Ե� �����޼���
     * @param aSQLState SQLState
     */
    static void parseShardingError(List<SQLException> aExceptionList, CmErrorResult aErrorResult, int aErrorCode,
                                   String aErrorMsg, String aSQLState)
    {
        // BUG-46790 ����ǥ���� ��Ī�Ҷ� ���๮�ڸ� �����ϱ� ���� Patter.DOTALL�� ������ �ش�.
        Pattern sFootPrintPattern = Pattern.compile(SHARD_FOOT_PRINT_REGULAR_EXPRESSION, Pattern.DOTALL);
        Matcher sMatcher = sFootPrintPattern.matcher(aErrorMsg);
        boolean sMatchFounded = false;
        boolean sIsServerSideNormalErrorExist = false;
        SQLException sCause = new SQLException(aErrorMsg, aSQLState, aErrorCode);
        while (sMatcher.find())
        {
            sMatchFounded = true;
            String sFootPrint = sMatcher.group(1);
            int sNativeErrorCode = Integer.parseInt(sMatcher.group(2), 16);
            int sNodeId = Integer.parseInt(sMatcher.group(3));
            shard_log("[FAILOVER FOOT_PRINT ERROR_CODE NODE_ID] : {0}, {1}, {2} ",
                      new Object[] {sFootPrint, sNativeErrorCode, sNodeId} );
            if (isServerSideFailoverErrorCode(sNativeErrorCode))
            {
                switch (sNativeErrorCode)
                {
                    case ErrorDef.SHARD_LIBRARY_LINK_FAILURE :
                        FailoverAlignInfo sAlignInfo = new FailoverAlignInfo(true, aSQLState, sNativeErrorCode,
                                                                             aErrorMsg, sNodeId);
                        aErrorResult.addFailoverAlignInfo(sAlignInfo);
                        break;
                    case ErrorDef.SHARD_LIBRARY_FAILOVER_SUCCESS :
                        ShardFailOverSuccessException sFOException = new ShardFailOverSuccessException(
                                aErrorMsg, ErrorDef.FAILOVER_SUCCESS);
                        sFOException.initCause(sCause);
                        aExceptionList.add(sFOException);
                        break;
                    case ErrorDef.SHARD_LIBRARY_FAILOVER_IS_NOT_AVAILABLE :
                        ShardFailoverIsNotAvailableException sFailoverNotAvailableEx =
                                new ShardFailoverIsNotAvailableException(aErrorMsg,
                                                                         ErrorDef.SHARD_NODE_FAILOVER_IS_NOT_AVAILABLE );
                        sFailoverNotAvailableEx.initCause(sCause);
                        aExceptionList.add(sFailoverNotAvailableEx);
                        break;
                }
            }
            else
            {
                // BUG-46790 �������̵� failover ���ܴ� �ƴ����� foot print�� ���Ե� ��� SQLException �ϳ��� ó���Ѵ�.
                sIsServerSideNormalErrorExist = true;
            }
        }
        /* BUG-46790 �����޼����� failover foot print�� ���ų� failover�� �ƴ� �ٸ� sd ������ �߻��� ��� �Ϲ�
           SQLException���� ó���Ѵ�.  */
        if (!sMatchFounded || sIsServerSideNormalErrorExist)
        {
            aExceptionList.add(sCause);
        }
    }

    /**
     * Shard ���� ����(SMN���� or Align����)�� ó���Ѵ�.
     * @param aErrorResult ���� ��� ��ü
     * @throws SQLException align data node ������ ������ �߻��� ���
     */
    public static void processShardError(AltibaseShardingConnection aMetaConn,
                                         CmErrorResult aErrorResult) throws SQLException
    {
        if (aMetaConn == null)
        {
            return;
        }
        List<SQLException> sFailoverExceptionList = new ArrayList<SQLException>();
        // BUG-46513 SMN Invalid ������ �߻��� ��� SMN���� needToDisconnect���� �����Ѵ�.
        while (aErrorResult != null)
        {
            if (aErrorResult.isInvalidSMNError())
            {
                long sSMN = aErrorResult.getSMNOfDataNode();
                aMetaConn.setShardMetaNumberOfDataNode(sSMN);

                boolean sIsNeedToDisconnect = aErrorResult.isNeedToDisconnect();
                aMetaConn.setNeedToDisconnect(sIsNeedToDisconnect);
            }

            // BUG-46790 serverside failover�� ���� align�� �ʿ����� üũ�Ѵ�.
            AltibaseShardingFailover sShardFailover = aMetaConn.getShardFailover();
            for (FailoverAlignInfo sAlignInfo : aErrorResult.getFailoverAlignInfoList())
            {
                ShardNodeConfig sNodeConfig = aMetaConn.getShardNodeConfig();
                DataNode sNode = sNodeConfig.getNode(sAlignInfo.getNodeId());

                Connection sNodeConn = aMetaConn.getCachedConnections().get(sNode);
                if (sNodeConn == null)
                {
                   /* BUG-46790 ���� ���Ŀ�ؼ��� ���� ��쿡�� �������� alternate server���� ������ �����ϰ�
                      ������ notify�� ������. */
                    // BUGBUG : getNodeConnection ���п� ���� ó���� ����.
                    AltibaseConnection sAlternateConn = (AltibaseConnection)aMetaConn.getNodeConnection(sNode, true);
                    sShardFailover.notifyFailover(sAlternateConn);
                    sFailoverExceptionList.add(new ShardFailOverSuccessException(sAlignInfo.getMessageText(),
                                                                                 ErrorDef.FAILOVER_SUCCESS,
                                                                                 sNode.getNodeName(),
                                                                                 sNode.getServerIp(),
                                                                                 sNode.getPortNo()));
                }
                else
                {
                    try
                    {
                        // BUGBUG : disconnection�� ���� ������ ����... /nok ���蹮�� ����� �ٸ�.
                        sShardFailover.alignDataNodeConnection((AltibaseConnection)sNodeConn, sAlignInfo);
                    }
                    catch (ShardJdbcException aEx)
                    {
                        sFailoverExceptionList.add(aEx);
                    }
                }
            }
            aErrorResult = aErrorResult.getNextError();
        }

        throwSQLExceptionIfExists(sFailoverExceptionList);
    }

    private static boolean isServerSideFailoverErrorCode(int aNativeErrorCode)
    {
        switch (aNativeErrorCode)
        {
            case ErrorDef.SHARD_LIBRARY_LINK_FAILURE :
            case ErrorDef.SHARD_LIBRARY_FAILOVER_SUCCESS :
            case ErrorDef.SHARD_LIBRARY_FAILOVER_IS_NOT_AVAILABLE:
                return true;
        }

        return false;
    }

    public static void throwSQLExceptionIfExists(List<SQLException> aExceptionList) throws SQLException
    {
        if (aExceptionList.size() > 0)
        {
            SQLException sException = null;

            if (aExceptionList.size() > 1)
            {
                sException = makeMultiErrors(aExceptionList);
            }

            for (SQLException sEach : aExceptionList)
            {
                if (sException == null)
                {
                    sException = sEach;
                }
                else
                {
                    sException.setNextException(sEach);
                }
            }
            shard_log(Level.SEVERE, "(NODE EXECUTION EXCEPTION) ", sException);

            throw sException;
        }
    }

    static SQLException makeMultiErrors(List<SQLException> aExceptionList)
    {
        boolean isFirst = true;
        boolean isAllSame = true;
        int sErrorCode = 0;
        String sSqlState = "";
        StringBuilder sSb = new StringBuilder();
        
        for (SQLException sEach : aExceptionList)
        {
             if (isFirst)
             {
                 sErrorCode = sEach.getErrorCode();
                 sSqlState = sEach.getSQLState();
             }
             else
             {
                 if (isAllSame && (sErrorCode != sEach.getErrorCode()))
                 {
                     sErrorCode = ErrorDef.SHARD_MULTIPLE_ERRORS;
                     sSqlState = SQLStateMap.getSQLState(sErrorCode);
                     isAllSame = false;
                 }
                 sSb.append("\n");
             }
             
             sSb.append(sEach.getMessage());
             isFirst = false;
        }

        SQLException sException = new SQLException(sSb.toString(), sSqlState, sErrorCode);

        return sException;
    }
}
