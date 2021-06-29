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

package Altibase.jdbc.driver.cm;

import Altibase.jdbc.driver.ex.ErrorDef;
import Altibase.jdbc.driver.sharding.core.FailoverAlignInfo;

import java.sql.SQLException;
import java.util.ArrayList;
import java.util.List;

public class CmErrorResult
{
    private static final int ERROR_ACTION_IGNORE = 0x02000000;

    // BUG-46790 shard failover
    private static final int ERROR_MODULE_SD     = 0xE0000000;
    private static final int ERROR_MODULE_MASK   = 0xF0000000;

    private byte                    mOp;
    private int                     mErrorIndex;
    private int                     mErrorCode;
    private String                  mErrorMessage;
    private boolean                 mIsBatchError;
    private CmErrorResult           mNext;
    private CmProtocolContext       mProtocolContext;          // BUG-46513 ��������κ��� execute����� �����ϱ� ���� ���ؽ�Ʈ ��ü
    private long                    mSMNOfDataNode;            // BUG-46513 DataNode�� ���� ������ SMN ��
    private boolean                 mIsNeedToDisconnect;       // BUG-46513 SMN ������ �߻����� �� Disconnect �ؾ��ϴ��� ����
    private List<FailoverAlignInfo> mAlignInfoList;            // BUG-46790 shard failover align������ ��� �ִ� ArrayList
    private long                    mSCN;                      // TASK-7220 ���� �л����Ʈ����� ���ռ� 
    private long                    mNodeId;                   // TASK-7218 Multi-Error Handling

    CmErrorResult()
    {
        mAlignInfoList = new ArrayList<FailoverAlignInfo>();
    }

    public CmErrorResult(byte aOp, int aErrorIndex, int aErrorCode, String aErrorMessage, boolean aIsBatchError)
    {
        mOp = aOp;
        mErrorIndex = aErrorIndex;
        mErrorCode = aErrorCode;
        mErrorMessage = aErrorMessage;
        mIsBatchError = aIsBatchError;
    }

    public void readFrom(CmChannel aChannel, byte aResultOp) throws SQLException
    {
        mOp = aChannel.readByte();
        mErrorIndex = aChannel.readInt();
        mErrorCode = aChannel.readInt();
        mErrorMessage = aChannel.readStringForErrorResult();
        // TASK-7220 ���� �л����Ʈ����� ���ռ�
        if (aResultOp == CmOperationDef.DB_OP_ERROR_V3_RESULT)
        {
            mSCN = aChannel.readLong();
            mNodeId = aChannel.readUnsignedInt();
        }
    }

    public void addError(CmErrorResult aNext)
    {
        if (aNext == null)
        {
            return;
        }
        CmErrorResult sError = this;
        while (sError.mNext != null)
        {
            sError = sError.mNext;
        }
        sError.mNext = aNext;
    }

    public void clear()
    {
        if (mNext != null)
        {
            mNext.clear();
        }
        mNext = null;
    }

    public short getErrorOp()
    {
        return mOp;
    }
    
    public int getErrorIndex()
    {
        return mErrorIndex;
    }

    public void initErrorIndex(int aErrorIndex)
    {
        mErrorIndex = aErrorIndex;
    }

    public int getErrorCode()
    {
        return mErrorCode;
    }

    public String getErrorMessage()
    {
        if (mIsBatchError)
        {
            return "[Batch " + mErrorIndex + "]:" + mErrorMessage;
        }
        else
        {
            return mErrorMessage;
        }
    }

    /**
     * �����ص� �Ǵ� �������� Ȯ���Ѵ�.
     *
     * @return �����ص� �Ǵ��� ����
     */
    public boolean isIgnorable()
    {
        return ((getErrorCode() & ERROR_ACTION_IGNORE) == ERROR_ACTION_IGNORE);
    }

    /**
     * SESSION_WITH_INVALID_SMN �������� ���θ� �����Ѵ�.
     * @return smn invalid ����
     */
    public boolean isInvalidSMNError()
    {
        return (getErrorCode() >>> 12) == ErrorDef.SHARD_META_NUMBER_INVALID;
    }

    /**
     * shard ���� sd ��⿡�� �߻��� �������� üũ�Ѵ�.
     * @return sd ��� ���� ����
     */
    public boolean isShardModuleError()
    {
        return ((getErrorCode() & ERROR_MODULE_MASK) == ERROR_MODULE_SD);
    }

    /**
     * ignorable ���� �� ������ SQLWarning�� �������Ѿ� �ϴ��� Ȯ���Ѵ�.
     * @param aErrorCode �����ڵ�
     * @return true ���� �������� ���� ignore�̱� ������ SQLWarning�� ������ �� �ִ�.
     * <br> false SQLWarning�� �������Ѿ� �Ѵ�.
     */
    public boolean canSkipSQLWarning(int aErrorCode)
    {
        switch (aErrorCode)
        {
            case ErrorDef.IGNORE_NO_ERROR:
            case ErrorDef.IGNORE_NO_COLUMN:
            case ErrorDef.IGNORE_NO_CURSOR:
            case ErrorDef.IGNORE_NO_PARAMETER:
                return true;
        }
        return false;
    }

    public boolean isBatchError()
    {
        return mIsBatchError;
    }
    
    public void setBatchError(boolean aIsBatchError)
    {
        mIsBatchError = aIsBatchError;
    }

    public CmErrorResult getNextError()
    {
        return mNext;
    }

    @Override
    public String toString()
    {
        int sOp = Byte.toUnsignedInt(mOp);  // BUG-48775
        final StringBuilder sSb = new StringBuilder("CmErrorResult{");
        sSb.append("mOp=").append(CmOperation.getOperationName(sOp, true));
        sSb.append(", mErrorIndex=").append(mErrorIndex);
        sSb.append(", mErrorCode=").append(mErrorCode);
        sSb.append(", mErrorMessage='").append(mErrorMessage).append('\'');
        sSb.append(", mIsBatchError=").append(mIsBatchError);
        sSb.append(", mNext=").append(mNext);
        sSb.append(", mProtocolContext=").append(mProtocolContext);
        sSb.append(", mSMNOfDataNode=").append(mSMNOfDataNode);
        sSb.append(", mIsNeedToDisconnect=").append(mIsNeedToDisconnect);
        sSb.append('}');
        return sSb.toString();
    }

    public void setContext(CmProtocolContext aProtocolContext)
    {
        mProtocolContext = aProtocolContext;
    }

    public CmProtocolContext getProtocolContext()
    {
        return mProtocolContext;
    }

    public void setSMNOfDataNode(long aSMNOfDataNode)
    {
        mSMNOfDataNode = aSMNOfDataNode;
    }

    public long getSMNOfDataNode()
    {
        return mSMNOfDataNode;
    }

    public void setNeedToDisconnect(boolean aNeedToDisconnect)
    {
        mIsNeedToDisconnect = aNeedToDisconnect;
    }

    public boolean isNeedToDisconnect()
    {
        return mIsNeedToDisconnect;
    }

    public List<FailoverAlignInfo> getFailoverAlignInfoList()
    {
        return mAlignInfoList;
    }

    public void addFailoverAlignInfo(FailoverAlignInfo aAlignInfo)
    {
        mAlignInfoList.add(aAlignInfo);
    }

    long getSCN()
    {
        return mSCN;
    }
}
