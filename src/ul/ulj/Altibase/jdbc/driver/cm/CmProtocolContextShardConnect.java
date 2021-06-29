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

import Altibase.jdbc.driver.AltibaseDataSource;
import Altibase.jdbc.driver.sharding.core.AltibaseShardingConnection;
import Altibase.jdbc.driver.sharding.core.DataNode;
import Altibase.jdbc.driver.sharding.core.ShardNodeConfig;
import Altibase.jdbc.driver.util.AltibaseProperties;
import Altibase.jdbc.driver.util.StringUtils;

import javax.sql.DataSource;
import java.util.*;

import static Altibase.jdbc.driver.sharding.util.ShardingTraceLogger.shard_log;
import static Altibase.jdbc.driver.util.AltibaseProperties.*;
import static Altibase.jdbc.driver.util.AltibaseProperties.PROP_SHARD_META_NUMBER;

public class CmProtocolContextShardConnect extends CmProtocolContextConnect
{
    private ShardNodeConfig            mShardNodeConfig;
    private DataNode                   mShardOnTransactionNode;
    private boolean                    mIsAutoCommit;
    private boolean                    mIsTouched;
    private boolean                    mIsNodeTransactionStarted;
    private Map<DataNode, DataSource>  mDataSourceMap;
    private String                     mServerCharSet;
    private CmConnType                 mShardConnType;
    private long                       mShardPin;
    private long                       mShardMetaNumber;
    private long                       mSMNOfDataNode;
    private boolean                    mNeedToDisconnect; // BUG-46513 smn ������ �߻������� ������ ��������� ����
    private AltibaseShardingConnection mMetaConn;

    public CmProtocolContextShardConnect(AltibaseShardingConnection aMetaConn)
    {
        super();
        // BUG-46513 ��尡 ���ŵǾ����� �ش��ϴ� ��� Ŀ�ؼ��� �����ϱ� ���� meta connection ��ü�� �����Ѵ�.
        mMetaConn = aMetaConn;
    }

    public ShardNodeConfig getShardNodeConfig()
    {
        return mShardNodeConfig;
    }

    public void setShardNodeConfig(ShardNodeConfig aShardNodeConfig)
    {
        mShardNodeConfig = aShardNodeConfig;
    }

    /**
     * getNodeList ���������� ����� ������ ShardNodeConfig��ü�� �����Ѵ�.
     * @param aShardPin �����ɰ�
     * @param aShardMetaNumber getNodeList���������� ���� ���޹��� ShardMetaNumber ��
     * @param aShardNodeConfig getNodeList���������� ���� ���� ������ ShardNodeConfig ��ü
     */
    public void setShardNodeConfig(long aShardPin, long aShardMetaNumber, ShardNodeConfig aShardNodeConfig)
    {
        if (mShardMetaNumber < aShardMetaNumber)
        {
            mShardPin = aShardPin;
            mShardMetaNumber = aShardMetaNumber;

            // BUG-46513 ������ �̹� ShardNodeConfig��ü�� �ִ� ��� ��帮��Ʈ�� �����Ѵ�.
            if (mShardNodeConfig != null)
            {
                Set<DataNode> sOldNodeSet = new LinkedHashSet<DataNode>(mShardNodeConfig.getDataNodes());
                Set<DataNode> sCurrNodeSet = new LinkedHashSet<DataNode>(aShardNodeConfig.getDataNodes());
                mergeNodeList(sOldNodeSet, sCurrNodeSet);
            }

            mShardNodeConfig = aShardNodeConfig;
        }
    }

    /**
     * �ΰ��� ��帮��Ʈ�� ������ ����� ShardNodeConfig��ü�� �ٽ� �����Ѵ�.<br>
     * ���ŵ� ���� ���������� closeó���� �ϰ� �߰��� ��忡 ���ؼ��� ��� ������ ������Ʈ �� ���´�.<br>
     * ���ս� �ߺ���尡 �����Ǵ� ���� �����ϱ� ���� Set�� �����.
     * @param aOldNodes ���� ��帮��Ʈ
     * @param aCurrNodes ���ŵ� ��帮��Ʈ
     */
    private void mergeNodeList(Set<DataNode> aOldNodes, Set<DataNode> aCurrNodes)
    {
        List<DataNode> sRemovedNodeList = new ArrayList<DataNode>();
        for (DataNode sEach : aOldNodes)
        {
            if (!aCurrNodes.contains(sEach))
            {
                sRemovedNodeList.add(sEach);
            }
        }
        // BUG-46513 ���ŵ� ���Ŀ�ؼǿ� ���� close ó���� �ϰ� OldNodes ���� ����
        for (DataNode sRemovedNode : sRemovedNodeList)
        {
            mMetaConn.removeNode(sRemovedNode);
            aOldNodes.remove(sRemovedNode);
        }
        aOldNodes.addAll(aCurrNodes); // BUG-46513 current ����Ʈ�� OldNodes�� ����
        List<DataNode> sResult = new ArrayList<DataNode>(aOldNodes);
        mShardNodeConfig.setDataNodes(sResult);
        mShardNodeConfig.setNodeCount(sResult.size()); // BUG-47357 ���� �� node count ���� ������Ʈ �ؾ� �Ѵ�.
    }

    public boolean isNodeTransactionStarted()
    {
        return mIsNodeTransactionStarted;
    }

    public void setNodeTransactionStarted(boolean aNodeTransactionStarted)
    {
        mIsNodeTransactionStarted = aNodeTransactionStarted;
    }

    public boolean isAutoCommitMode()
    {
        return mIsAutoCommit;
    }

    public void setAutoCommit(boolean aIsAutoCommit)
    {
        mIsAutoCommit = aIsAutoCommit;
    }

    public Map<DataNode, DataSource> getDataSourceMap()
    {
        return mDataSourceMap;
    }

    public void setDataSourceMap(Map<DataNode, DataSource> aDataSourceMap)
    {
        mDataSourceMap = aDataSourceMap;
    }

    public DataNode getShardOnTransactionNode()
    {
        return mShardOnTransactionNode;
    }

    public void setShardOnTransactionNode(DataNode aShardOnTransactionNode)
    {
        mShardOnTransactionNode = aShardOnTransactionNode;
    }

    public boolean isTouched()
    {
        return mIsTouched;
    }

    public void setTouched(boolean aTouched)
    {
        mIsTouched = aTouched;
    }

    public String getServerCharSet()
    {
        return mServerCharSet;
    }

    public void setServerCharSet(String aServerCharSet)
    {
        mServerCharSet = aServerCharSet;
    }

    public long getShardPin()
    {
        return mShardPin;
    }

    public void setShardMetaNumber(long aShardMetaNumber)
    {
        mShardMetaNumber = aShardMetaNumber;
    }

    public long getShardMetaNumber()
    {
        return mShardMetaNumber;
    }

    public void setSMNOfDataNode(long aSMNOfDataNode)
    {
        if ( mSMNOfDataNode < aSMNOfDataNode )
        {
            mSMNOfDataNode = aSMNOfDataNode;
        }
    }

    public long getSMNOfDataNode()
    {
        return mSMNOfDataNode;
    }

    /**
     * DataNode�� ������ �������� DataSource��ü�� �����. �̶� shardpin, shard_meta_number�� ���� ���鵵 �Բ�<br>
     * �����Ѵ�.
     * @param aProp AltibaseProperties ��ü
     */
    public void createDataSources(AltibaseProperties aProp)
    {
        mDataSourceMap = new HashMap<DataNode, DataSource>();
        for (DataNode sNode : mShardNodeConfig.getDataNodes())
        {
            AltibaseProperties sPropsForDataNode = copyProps(aProp);
            removeConnAttributes(sPropsForDataNode);
            sPropsForDataNode.put(PROP_SERVER, sNode.getServerIp());
            sPropsForDataNode.put(PROP_PORT, String.valueOf(sNode.getPortNo()));
            // data node�� dbname�� �Ѿ���� �ʱ⶧���� ���鹮�ڷ� �����Ѵ�. �̷��� �ϸ� ���������� dbname�� üũ���� �ʴ´�.
            sPropsForDataNode.put(PROP_DBNAME, "");
            sPropsForDataNode.put(PROP_SHARD_NODE_NAME, sNode.getNodeName());
            sPropsForDataNode.put(PROP_SHARD_PIN, String.valueOf(mShardPin));
            sPropsForDataNode.put(PROP_SHARD_META_NUMBER, String.valueOf(getShardMetaNumber()));
            if (mShardConnType != CmConnType.TCP)
            {
                sPropsForDataNode.put(PROP_CONNTYPE, mShardConnType.name());
                if (mShardConnType == CmConnType.SSL)
                {
                    // ssl�� ��쿡�� �߰��� ssl_enable�� Ȱ��ȭ ����� �Ѵ�.
                    sPropsForDataNode.put(PROP_SSL_ENABLE, "true");
                }
            }
            // BUG-47324
            sPropsForDataNode.put(PROP_SHARD_CLIENT, "1");
            sPropsForDataNode.put(PROP_SHARD_SESSION_TYPE, "2");
            makeAlternateServerProps(sNode, sPropsForDataNode);
            AltibaseDataSource sDataSource = new AltibaseDataSource(sPropsForDataNode);
            sDataSource.setUser(aProp.getUser());
            sDataSource.setPassword(aProp.getPassword());
            mDataSourceMap.put(sNode, sDataSource);
            shard_log("(MAKE DATASOURCE) {0}", sNode);
        }
    }

    /**
     * DataNode�� AlternateServer ������ �������� connection string�� �����Ѵ�.
     * @param aNode �����ͳ�� ���� ��ü
     * @param aPropsForDataNode alternateservers �Ӽ��� �����ϴ� Property ��ü
     */
    private void makeAlternateServerProps(DataNode aNode, AltibaseProperties aPropsForDataNode)
    {
        String sAlternateServerIp = aNode.getAlternativeServerIp();
        if (!StringUtils.isEmpty(sAlternateServerIp))
        {
            String sAlternateServerPort = String.valueOf(aNode.getAlternativePortNo());
            StringBuilder sSb = new StringBuilder();
            sSb.append("(").append(sAlternateServerIp)
               .append(":").append(sAlternateServerPort).append(")");
            aPropsForDataNode.put(PROP_ALT_SERVERS, sSb.toString());
        }
    }

    /**
     * Meta�� �����Ҷ� ������ Property������ DataNode�� ����ϱ� ���� �����Ѵ�.
     * @param aProp ���� Property����
     * @return ������ Property����
     */
    private AltibaseProperties copyProps(AltibaseProperties aProp)
    {
        AltibaseProperties sResults = new AltibaseProperties();
        AltibaseProperties sProp = aProp.getDefaults();
        while (sProp != null)
        {
            setProperties(sResults, sProp);
            sProp = sProp.getDefaults();
        }
        setProperties(sResults, aProp);

        return sResults;
    }

    /**
     * ���� ������Ƽ�� key�� value�� loop�� ���鼭 �����Ѵ�.
     * @param aResults ����� Property ��ü
     * @param aSourceProps ���� Property ��ü
     */
    private void setProperties(AltibaseProperties aResults, AltibaseProperties aSourceProps)
    {
        for (Map.Entry sEntry : aSourceProps.entrySet())
        {
            String sKey = (String)sEntry.getKey();
            String sValue = (String)sEntry.getValue();
            if (!StringUtils.isEmpty(sKey))
            {
                aResults.setProperty(sKey, sValue);
            }
        }
    }

    /**
     * DataNode�� Property ������ �����Ҷ� �⺻ ������Ƽ�� �����Ѵ�.
     * @param aProp ���� Property����
     */
    private void removeConnAttributes(Properties aProp)
    {
        aProp.remove(AltibaseProperties.PROP_CONNTYPE);
        aProp.remove(AltibaseProperties.PROP_SHARD_CONNTYPE);
        aProp.remove(AltibaseProperties.PROP_SHARD_LAZYCONNECT);
        aProp.remove(AltibaseProperties.PROP_RESHARD_ENABLE);
        aProp.remove(AltibaseProperties.PROP_SSL_ENABLE);
        aProp.remove(AltibaseProperties.PROP_DATASOURCE_NAME);
        aProp.remove(AltibaseProperties.PROP_ALT_SERVERS);
        aProp.remove(AltibaseProperties.PROP_PORT);
        aProp.remove(AltibaseProperties.PROP_SERVER);
        aProp.remove(AltibaseProperties.PROP_DBNAME);
        aProp.remove(AltibaseProperties.PROP_LOAD_BALANCE);
    }

    public void setShardConnType(CmConnType aShardConnType)
    {
        mShardConnType = aShardConnType;
    }

    public boolean needToDisconnect()
    {
        return mNeedToDisconnect;
    }

    public void setNeedToDisconnect(boolean aNeedToDisconnect)
    {
        mNeedToDisconnect = aNeedToDisconnect;
    }

    @Override
    public String toString()
    {
        final StringBuilder sSb = new StringBuilder("CmProtocolContextShardConnect{");
        sSb.append("mShardNodeConfig=").append(mShardNodeConfig);
        //sSb.append(", mGlobalTransactionLevel=").append(mGlobalTransactionLevel);
        sSb.append(", mShardOnTransactionNode=").append(mShardOnTransactionNode);
        sSb.append(", mIsAutoCommit=").append(mIsAutoCommit);
        sSb.append(", mIsTouched=").append(mIsTouched);
        sSb.append(", mIsNodeTransactionStarted=").append(mIsNodeTransactionStarted);
        sSb.append(", mDataSourceMap=").append(mDataSourceMap);
        sSb.append(", mServerCharSet='").append(mServerCharSet).append('\'');
        sSb.append(", mShardConnType=").append(mShardConnType);
        sSb.append(", mShardPin=").append(mShardPin);
        sSb.append(", mShardMetaNumber=").append(mShardMetaNumber);
        sSb.append(", mSMNOfDataNode=").append(mSMNOfDataNode);
        sSb.append(", mNeedToDisconnect=").append(mNeedToDisconnect);
        sSb.append(", mMetaConn=").append(mMetaConn);
        sSb.append('}');

        return sSb.toString();
    }
}
