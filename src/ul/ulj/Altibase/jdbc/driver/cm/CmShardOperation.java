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

package Altibase.jdbc.driver.cm;

import Altibase.jdbc.driver.datatype.Column;
import Altibase.jdbc.driver.datatype.ColumnFactory;
import Altibase.jdbc.driver.ex.Error;
import Altibase.jdbc.driver.ex.ErrorDef;
import Altibase.jdbc.driver.sharding.core.*;
import Altibase.jdbc.driver.sharding.util.Range;

import java.sql.SQLException;
import java.sql.Types;
import java.util.List;

import static Altibase.jdbc.driver.sharding.core.ShardValueType.*;
import static Altibase.jdbc.driver.sharding.util.ShardingTraceLogger.shard_log;

public class CmShardOperation extends CmOperationDef
{
    private static final int MAX_SERVER_IP_LEN = 16;
    private CmChannel        mChannel;
    private ColumnFactory    mColumnFactory;
    
    CmShardOperation(CmChannel aChannel)
    {
        mChannel = aChannel;
        // BUG-46513 ColumnFactory��ü�� ä�ηκ��� �Ҵ� �޴´�.
        mColumnFactory = aChannel.getColumnFactory();
    }

    void readProtocolResult(CmProtocolContext aContext) throws SQLException
    {
        CmChannel sChannel = aContext.channel();
        byte sOp = sChannel.readOp();
        
        if (sOp == DB_OP_ERROR_V3_RESULT)
        {
            CmOperation.readErrorV3Result(sChannel, aContext, sOp);
            return;
        }

        switch (sOp)
        {
            case DB_OP_SHARD_NODE_GET_LIST_RESULT:
                readGetNodeListResult((CmProtocolContextShardConnect)aContext);
                break;
            case DB_OP_SHARD_NODE_UPDATE_LIST_RESULT:
                readUpdateNodeListResult((CmProtocolContextShardConnect)aContext);
                break;
            case DB_OP_SHARD_TRANSACTION_V3_RESULT:
                readShardTransactionResult(aContext);
                break;
            case DB_OP_SET_PROPERTY_V3_RESULT:
            case DB_OP_SHARD_STMT_PARTIAL_ROLLBACK_RESULT:
            case DB_OP_SHARD_NODE_REPORT_RESULT:
                break;
            default:
                Error.throwInternalError(ErrorDef.INVALID_OPERATION_PROTOCOL, String.valueOf(sOp));
                break;
        }
    }

    /**
     * ��Ź��ۿ� DB ShardNodeGetList(91) ���������� ����.
     */
    void writeGetNodeList() throws SQLException
    {
        mChannel.checkWritable(1);
        mChannel.writeOp(DB_OP_SHARD_NODE_GET_LIST);
    }

    /**
     * ��Ź��ۿ� DB ShardNodeUpdateList(89) ���������� ����.
     */
    void writeUpdateNodeList() throws SQLException
    {
        mChannel.checkWritable(1);
        mChannel.writeOp(DB_OP_SHARD_NODE_UPDATE_LIST);
    }

    /**
     * ���������� ���� DB ShardNodeUpdateList(89)�� ����� �����Ѵ�.
     * @param aShardContextConnect �������ؽ�Ʈ ��ü
     */
    void readUpdateNodeListResult(CmProtocolContextShardConnect aShardContextConnect) throws SQLException
    {
        shard_log("(Update Node List Result) Shard Node List Update Result");
        ShardNodeConfig sShardNodeConfig = makeShardNodeConfig();
        aShardContextConnect.setShardNodeConfig(sShardNodeConfig);
        aShardContextConnect.setShardMetaNumber(mChannel.readLong());
        if (sShardNodeConfig.getNodeCount() == 0)
        {
            Error.throwSQLException(ErrorDef.SHARD_NO_NODES);
        }
    }

    /**
     * ���������� ���� DB ShardNodeGetList(91)�� ����� �����Ѵ�.
     * @param aShardContextConnect �������ؽ�Ʈ ��ü
     */
    void readGetNodeListResult(CmProtocolContextShardConnect aShardContextConnect) throws SQLException
    {
        ShardNodeConfig sShardNodeConfig = makeShardNodeConfig();
        long sShardPin = mChannel.readLong();
        long sShardMetaNumber = mChannel.readLong();
        aShardContextConnect.setShardNodeConfig(sShardPin, sShardMetaNumber, sShardNodeConfig);
        if (sShardNodeConfig.getNodeCount() == 0)
        {
            Error.throwSQLException(ErrorDef.SHARD_NO_NODES);
        }
        shard_log("(READ GET NODELIST RESULT) {0} ", aShardContextConnect);
    }

    /**
     * ������ ����� ������ ���������� ���� �޾ƿ� �����Ѵ�.
     * @return ShardNodeConfig��ü
     * @throws SQLException ������ ��� �� ������ �߻����� ��
     */
    private ShardNodeConfig makeShardNodeConfig() throws SQLException
    {
        ShardNodeConfig sShardNodeConfig = new ShardNodeConfig();
        byte sIsTestEnable = mChannel.readByte();
        sShardNodeConfig.setTestEnable(sIsTestEnable == 1);
        if (sIsTestEnable > 1)
        {
            Error.throwSQLException(ErrorDef.SHARD_INVALID_TEST_MARK, "Invalid test mark.");
        }
        int sNodeCount = mChannel.readUnsignedShort();
        sShardNodeConfig.setNodeCount(sNodeCount);
        for (int i = 0; i < sNodeCount; i++)
        {
            DataNode sDataNode = new DataNode();
            sDataNode.setNodeId(mChannel.readInt());
            short sLength = mChannel.readUnsignedByte();
            sDataNode.setNodeName(mChannel.readDecodedString(sLength, 0));
            sDataNode.setServerIp(mChannel.readString(MAX_SERVER_IP_LEN));
            sDataNode.setPortNo(mChannel.readUnsignedShort());
            sDataNode.setAlternativeServerIp(mChannel.readString(MAX_SERVER_IP_LEN));
            sDataNode.setAlternativePortNo(mChannel.readUnsignedShort());
            sShardNodeConfig.addNode(sDataNode);
        }
        return sShardNodeConfig;
    }

    /**
     * ��Ź��ۿ� DB ShardAnalyze(87) ���������� ����.
     * @param aSql sql string
     * @param aStmtID Statement ID
     */
    void writeShardAnalyze(String aSql, int aStmtID) throws SQLException
    {
        int sBytesLen = mChannel.prepareToWriteString(aSql, WRITE_STRING_MODE_DB);

        mChannel.checkWritable(10);
        mChannel.writeOp(DB_OP_SHARD_ANALYZE);
        mChannel.writeInt(aStmtID);       // BUG-47274 ������ 0���� ������ �ʰ� �������� �޾ƿ� ���� ������.
        mChannel.writeByte((byte)0);      // mode : dummy(0)
        mChannel.writeInt(sBytesLen);     // statement string length
        mChannel.writePreparedString();   // statement string
    }

    /**
     * �������� DB ShardAnalyze(87)�� ����� �����Ѵ�.
     * @param aShardContextStmt shard statement context ��ü
     */
    void readShardAnalyze(CmProtocolContextShardStmt aShardContextStmt) throws SQLException
    {
        byte sOp = mChannel.readOp();
        if (sOp != DB_OP_SHARD_ANALYZE_RESULT)
        {
            CmOperation.readErrorV3Result(mChannel, aShardContextStmt, sOp);
            // analyze�� �����ϸ� serverside flag�� Ȱ��ȭ �Ѵ�.
            aShardContextStmt.getShardAnalyzeResult().setShardCoordinate(true);
            shard_log("(SHARD ANALYZE FAILED) use coordinator");
            return;
        }

        makeShardPrepareResult(aShardContextStmt.getShardPrepareResult());
        CmShardAnalyzeResult sShardAnalyzeResult = aShardContextStmt.getShardAnalyzeResult();
        makeShardAnalyzeResult(sShardAnalyzeResult);

        shard_log("(SHARD ANALZE RESULT) {0}", sShardAnalyzeResult);

        ShardNodeConfig sShardNodeConfig = aShardContextStmt.getShardContextConnect().getShardNodeConfig();
        makeShardValueRangeInfo(aShardContextStmt.getShardAnalyzeResult(), sShardNodeConfig);
        makeCoordinateInfo(aShardContextStmt);
    }

    private void makeCoordinateInfo(CmProtocolContextShardStmt aShardContextStmt)
    {
        CmShardAnalyzeResult sShardAnalyzeResult = aShardContextStmt.getShardAnalyzeResult();
        if (!sShardAnalyzeResult.canMerge())
        {
            sShardAnalyzeResult.setShardCoordinate(true);
        }
        else
        {
            // BUG-45640 autocommit on �̰� global transaction ������ ��� server side�� �����Ѵ�.
            int sShardRangeInfoCnt = sShardAnalyzeResult.getShardRangeInfoCnt();
            if (aShardContextStmt.isAutoCommitMode() &&
                aShardContextStmt.getGlobalTransactionLevel() == GlobalTransactionLevel.GLOBAL &&
                    sShardRangeInfoCnt > 1)
            {
                ShardSplitMethod sShardSplitMethod = sShardAnalyzeResult.getShardSplitMethod();
                if (sShardSplitMethod == ShardSplitMethod.HASH ||
                    sShardSplitMethod == ShardSplitMethod.RANGE ||
                    sShardSplitMethod == ShardSplitMethod.LIST)
                {
                    if (!sShardAnalyzeResult.isShardSubKeyExists())
                    {
                        if (sShardAnalyzeResult.getShardValueCount() != 1)
                        {
                            sShardAnalyzeResult.setShardCoordinate(true);
                        }
                    }
                    else
                    {
                        if (sShardAnalyzeResult.getShardValueCount() != 1 ||
                                sShardAnalyzeResult.getShardSubValueCount() != 1)
                        {
                            sShardAnalyzeResult.setShardCoordinate(true);
                        }
                    }
                }
                else if (sShardSplitMethod == ShardSplitMethod.CLONE)
                {
                    if (sShardAnalyzeResult.getShardValueCount() != 0)
                    {
                        sShardAnalyzeResult.setShardCoordinate(true);
                    }
                }
            }
        }
    }

    /**
     * ����Ű ���� ���� Range ������ �޾ƿ´�.
     * @param aShardAnalyzeResult shard statement analyze ��ü
     * @param aShardNodeConfig ���� ������ ��� ���� ����
     * @throws SQLException �������� ��� �� ������ �߻��� ���
     */
    private void makeShardValueRangeInfo(CmShardAnalyzeResult aShardAnalyzeResult,
                                         ShardNodeConfig aShardNodeConfig) throws SQLException
    {
        int sShardRangeInfoCnt = mChannel.readShort();
        ShardKeyDataType sShardKeyDataType = aShardAnalyzeResult.getShardKeyDataType();
        ShardSplitMethod sShardSplitMethod = aShardAnalyzeResult.getShardSplitMethod();
        ShardKeyDataType sShardSubKeyDateType = aShardAnalyzeResult.getShardSubKeyDataType();
        ShardSplitMethod sShardSubSplitMethod = aShardAnalyzeResult.getShardSubSplitMethod();

        ShardRangeList sShardRangeList = new ShardRangeList();
        for (int i = 0; i < sShardRangeInfoCnt; i++)
        {
            Range sRange = readShardRangeInfo(sShardKeyDataType, sShardRangeList.getCurrRange(), sShardSplitMethod);
            Range sSubRange = null;

            if (aShardAnalyzeResult.isShardSubKeyExists())
            {
                Range aCurrRange = sShardRangeList.getCurrRange();
                // subkey�� true�� ��� primary range�� ������ ����Ǿ����� flag�� ������ ��� �Ѵ�.
                if (aCurrRange != null && !sRange.equals(aCurrRange))
                {
                    sShardRangeList.setPrimaryRangeChanged(true);
                }

                sSubRange = readShardRangeInfo(sShardSubKeyDateType, sShardRangeList.getCurrSubRange(),
                                               sShardSubSplitMethod);
            }

            int sNodeId = mChannel.readInt();
            DataNode sNode = aShardNodeConfig.getNode(sNodeId);
            sShardRangeList.addRange(new ShardRange(sNode, sRange, sSubRange));
        }
        aShardAnalyzeResult.setShardRangeInfo(sShardRangeList);
        shard_log("(SHARD RANGE INFO) {0}", sShardRangeList);
    }

    @SuppressWarnings("unchecked")
    private Range readShardRangeInfo(ShardKeyDataType aShardKeyDataType, Range aPrevRange,
                                     ShardSplitMethod aShardSplitMethod) throws SQLException
    {
        Range sRange = null;
        // hash�� ��쿡�� ����Ű���� Integer�� �����Ѵ�.
        if (aShardSplitMethod == ShardSplitMethod.HASH)
        {
            if (aPrevRange == null)
            {
                aPrevRange = Range.getNullRange();
            }
            // BUG-46642 HP Unix�� JDK1.5.0.30 ���� �������� �ȵǴ� ������ �־� ���ʸ��޼ҵ� ȣ��� Ÿ���� ����Ѵ�.
            sRange = Range.<Integer>between(aPrevRange, mChannel.readInt());
        }
        // clone�̳� solo�� ��쿡�� shard range ������ ���� ������ Range������ �������� �ʴ´�.
        else if (aShardSplitMethod == ShardSplitMethod.LIST || aShardSplitMethod == ShardSplitMethod.RANGE)
        {
            aPrevRange = (aPrevRange == null) ? Range.getNullRange() : aPrevRange;
            switch (aShardKeyDataType)
            {
                case SMALLINT:
                    sRange = Range.<Short>between(aPrevRange, mChannel.readShort());
                    break;
                case INTEGER:
                    sRange = Range.<Integer>between(aPrevRange, mChannel.readInt());
                    break;
                case BIGINT:
                    sRange = Range.<Long>between(aPrevRange, mChannel.readLong());
                    break;
                case CHAR:
                case VARCHAR:
                    int sLength = mChannel.readShort();
                    String sStr = mChannel.readDecodedString(sLength, 0);
                    sRange = Range.between(aPrevRange, sStr);
                    break;
            }
        }

        return sRange;
    }

    /**
     * loop�� ���鼭 shard value ���� ��Ÿ ������ �����Ѵ�.
     * @param aStrategy shard value info�� ������ ������ü(primary or subkey)
     * @throws SQLException ������ ��� �� ������ �߻��� ���
     */
    private void makeShardValueMetaInfo(ShardValueInfoStrategy aStrategy) throws SQLException
    {
        for (int i = 0; i < aStrategy.getShardValueCount(); i++)
        {
            ShardValueInfo sShardValueInfo = new ShardValueInfo();
            sShardValueInfo.setType(mChannel.readByte());

            readShardValue(sShardValueInfo);

            aStrategy.addShardValue(sShardValueInfo);

            shard_log("(SHARD VALUE META INFO) {0} ", sShardValueInfo);
        }
    }

    /**
     * ���� ���������� ���� ���� ���� ���� ������ �޾ƿ´�. <br/>
     * ���� ���� ȣ��Ʈ ���� �� ���� ���尪�� �ε�����, ȣ��Ʈ ������ �ƴ� ���� ���尪 ��ü�� �Ѿ�´�.
     * @param aShardValueInfo ���� �� ���� �� ��ü
     * @throws SQLException ������ ��� �� ���ܰ� �߻��� ���
     */
    private void readShardValue(ShardValueInfo aShardValueInfo) throws SQLException
    {
        // BUG-47855 �������� ���濡 ���� host ����, ��� ��� type, length, value ���·� ���� �Ѿ�´�.
        ShardKeyDataType sShardValueType = ShardKeyDataType.get(mChannel.readInt());
        int sShardValueLength = mChannel.readInt();
        if (aShardValueInfo.getType() == HOST_VAR)
        {
            // BUG-47855 host ���� ����϶��� shard value index�� short ������ �Ѿ�´�.
            aShardValueInfo.setBindParamid(mChannel.readShort());
        }
        else
        {
            Column sColumn = null;
            switch (sShardValueType)
            {
                case SMALLINT:
                    sColumn = mColumnFactory.getInstance(Types.SMALLINT);
                    sColumn.setValue(mChannel.readShort());
                    break;
                case INTEGER:
                    sColumn = mColumnFactory.getInstance(Types.INTEGER);
                    sColumn.setValue(mChannel.readInt());
                    break;
                case BIGINT:
                    sColumn = mColumnFactory.getInstance(Types.BIGINT);
                    sColumn.setValue(mChannel.readUnsignedLong());
                    break;
                case CHAR:
                case VARCHAR:
                    sColumn = mColumnFactory.getInstance(Types.VARCHAR);
                    sColumn.setValue(mChannel.readString(sShardValueLength));
                    break;
            }
            aShardValueInfo.setValue(sColumn);
        }
    }

    /**
     * Shard Analyze ���������� ����� �����ϰ� �ؼ��ؼ� ���õ� ��ü�� �����Ѵ�.
     * @param aShardAnalyzeResult ���� ��Ÿ ������ ������ ��ü
     * @throws SQLException ������ ��� �� ������ �߻��� ���
     */
    private void makeShardAnalyzeResult(CmShardAnalyzeResult aShardAnalyzeResult) throws SQLException
    {
        aShardAnalyzeResult.setShardSplitMethod(ShardSplitMethod.get(mChannel.readByte()));
        // TASK-7219 Non-shard DML TEMPCODE (mmtCmsShard.cpp:685)
        mChannel.readByte();
        aShardAnalyzeResult.setShardKeyDataType(ShardKeyDataType.get(mChannel.readInt()));
        aShardAnalyzeResult.setShardSubKeyExists(mChannel.readByte());

        if (aShardAnalyzeResult.isShardSubKeyExists())
        {
            aShardAnalyzeResult.setShardSubSplitMethod(ShardSplitMethod.get(mChannel.readByte()));
            aShardAnalyzeResult.setShardSubKeyDataType(ShardKeyDataType.get(mChannel.readInt()));
        }

        aShardAnalyzeResult.setShardDefaultNodeID(mChannel.readInt());
        aShardAnalyzeResult.setShardCanMerge(mChannel.readByte());
        aShardAnalyzeResult.setShardValueCount(mChannel.readShort());

        if (aShardAnalyzeResult.isShardSubKeyExists())
        {
            aShardAnalyzeResult.setShardSubValueCount(mChannel.readShort());
        }

        makeShardValueMetaInfo(new ShardValueInfoPrimaryStrategy(aShardAnalyzeResult));

        if (aShardAnalyzeResult.isShardSubKeyExists())
        {
            makeShardValueMetaInfo(new ShardValueInfoSubKeyStrategy(aShardAnalyzeResult));
        }
    }

    /**
     * prepare ����� �޾ƿ´�.
     * @param aPrepareResult prepare����� �����ϴ� ��ü
     * @throws SQLException ������ ��� �� ������ �߻��� ���
     */
    private void makeShardPrepareResult(CmPrepareResult aPrepareResult) throws SQLException
    {
        aPrepareResult.setStatementId(mChannel.readInt());
        aPrepareResult.setStatementType(mChannel.readInt());
        aPrepareResult.setParameterCount(mChannel.readShort());
        aPrepareResult.setResultSetCount(mChannel.readShort());
        shard_log("(SHARD PREPARE RESULT) {0} ", aPrepareResult);
    }

    void writeShardTransactionCommitRequest(List<DataNode> aTouchedNodeList) throws SQLException
    {
        short sTouchedNodeCount = (short)aTouchedNodeList.size();
        mChannel.checkWritable(2 + 2 + sTouchedNodeCount * 4);
        mChannel.writeOp(CmOperation.DB_OP_SHARD_TRANSACTION_V3);
        mChannel.writeByte((byte)1);  // commit
        mChannel.writeShort(sTouchedNodeCount);
        for (DataNode sEach : aTouchedNodeList)
        {
            mChannel.writeInt(sEach.getNodeId());
        }
    }

    /* PROJ-2733 */
    void writeShardTransaction(CmChannel aChannel, boolean aIsCommit) throws SQLException
    {
        short sTouchedNodeCount = 0;
        aChannel.checkWritable(2 + 2 + sTouchedNodeCount * 4);
        aChannel.writeOp(DB_OP_SHARD_TRANSACTION_V3);
        if (aIsCommit)
        {
            aChannel.writeByte((byte)1);  // commit
        }
        else
        {
            aChannel.writeByte((byte)2);  // rollback
        }
        aChannel.writeShort(sTouchedNodeCount);
    }

    void readShardTransactionResult(CmProtocolContext aContext) throws SQLException
    {
        CmChannel sChannel = aContext.channel();
        long sSCN = sChannel.readLong();
        if (sSCN > 0) 
        {
            aContext.updateSCN(sSCN);
        }
    }

    void writeShardNodeReport(NodeConnectionReport aReport) throws SQLException
    {
        NodeConnectionReport.NodeReportType sType = aReport.getNodeReportType();
        
        if (sType == NodeConnectionReport.NodeReportType.SHARD_NODE_REPORT_TYPE_CONNECTION)
        {
            mChannel.checkWritable(10);
            mChannel.writeOp(DB_OP_SHARD_NODE_REPORT);
            mChannel.writeInt(SHARD_NODE_REPORT_TYPE_CONNECTION);
            mChannel.writeInt(aReport.getNodeId());
            mChannel.writeByte(aReport.getDestination().getValue());
        }
        else if (sType == NodeConnectionReport.NodeReportType.SHARD_NODE_REPORT_TYPE_TRANSACTION_BROKEN)
        {
            mChannel.checkWritable(5);
            mChannel.writeOp(DB_OP_SHARD_NODE_REPORT);
            mChannel.writeInt(SHARD_NODE_REPORT_TYPE_TRANSACTION_BROKEN);
        }
        else
        {
            Error.throwInternalError(ErrorDef.INVALID_OPERATION_PROTOCOL, "Invalid NodeConnectionReportType");
        }
    }

    void writeShardStmtPartialRollback(CmChannel aChannel) throws SQLException
    {
        aChannel.checkWritable(1);
        aChannel.writeOp(DB_OP_SHARD_STMT_PARTIAL_ROLLBACK);
    }

    public void setChannel(CmChannel aChannel)
    {
        mChannel = aChannel;
    }
}
