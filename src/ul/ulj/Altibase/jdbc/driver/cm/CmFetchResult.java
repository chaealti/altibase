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

import java.util.List;

import Altibase.jdbc.driver.datatype.RowHandle;
import Altibase.jdbc.driver.datatype.Column;
import Altibase.jdbc.driver.datatype.DynamicArrayRowHandle;
import Altibase.jdbc.driver.util.DynamicArray;

public class CmFetchResult extends CmStatementIdResult
{
    public static final byte MY_OP = CmOperation.DB_OP_FETCH_RESULT;

    private short            mResultSetId;
    private int              mMaxFieldSize;
    private long             mMaxRowCount;
    private int              mFetchedRowCount;                      // PROJ-2625
    private int              mTotalReceivedRowCount;
    private long             mFetchedBytes;                         // PROJ-2625
    private long             mTotalOctetLength;                     // PROJ-2625
    private RowHandle        mRowHandle;
    private boolean          mFetchRemains;
    private List<Column>     mColumns;
    private boolean          mBegun;
    private boolean          mIsPrepared;                           // BUG-47460 prepared fetch���� ����
    private boolean          mUseArrayListRowHandle;                // BUG-48380 ArrayListRowHandle�� ������� ����

    public CmFetchResult()
    {
        mBegun = false;
    }
    
    public short getResultSetId()
    {
        return mResultSetId;
    }
    
    public List<Column> getColumns()
    {
        return mColumns;
    }

    public RowHandle rowHandle()
    {
        if (mRowHandle == null)
        {
            mRowHandle = new DynamicArrayRowHandle();
        }
        return mRowHandle;
    }
    
    public void setMaxFieldSize(int aMaxFieldSize)
    {
        mMaxFieldSize = aMaxFieldSize;
    }
    
    void setMaxRowCount(long aMaxRowCount)
    {
        mMaxRowCount = aMaxRowCount;
    }
    
    /**
     * CMP_OP_DB_FetchV2 operation ��û�� ���� ���ŵ� fetch row ���� ��ȯ�Ѵ�.
     */
    public int getFetchedRowCount()
    {
        return mFetchedRowCount;
    }

    /**
     * CMP_OP_DB_FetchV2 operation ��û�� ���� ���ŵ� fetch byte ���� ��ȯ�Ѵ�.
     */
    public long getFetchedBytes()
    {
        return mFetchedBytes;
    }

    /**
     * SQL/CLI �� SQL_DESC_OCTET_LENGTH �� �����Ǵ� column ���̷μ�, ��� column �� ������ ��ȯ�Ѵ�.
     */
    public long getTotalOctetLength()
    {
        return mTotalOctetLength;
    }

    public boolean fetchRemains()
    {
        // MaxRows�� �Ѿ��ٸ� �� ������ �ʿ䰡 ����.
        if ((mMaxRowCount > 0) && (mTotalReceivedRowCount >= mMaxRowCount))
        {
            return false;
        }

        return mFetchRemains;
    }

    protected byte getResultOp()
    {
        return MY_OP;
    }

    void setResultSetId(short aResultSetId)
    {
        mResultSetId = aResultSetId;
    }
    
    int getMaxFieldSize()
    {
        return mMaxFieldSize;
    }

    /**
     * fetch�� row�� �� ���� ��쿡�� RowHandle�� storeĿ���ε����� ������Ų��.
     */
    void increaseStoreCursor()
    {
        if (fetchRemains())
        {
            mRowHandle.increaseStoreCursor();
        }
    }

    /**
     * fetch�� row�� �� ���� ��쿡��, fetched rows/bytes ������ �����Ѵ�.
     */
    void updateFetchStat(long aRowSize)
    {
        if (fetchRemains())
        {
            mFetchedRowCount++;
            mTotalReceivedRowCount++;

            // PROJ-2625 Semi-async Prefetch, Prefetch Auto-tuning
            //
            //   5 = OpID(1) + row size(4)
            //
            //   +---------+-------------+----------------------------------+
            //   | OpID(1) | row size(4) | row data(row size)               |
            //   +---------+-------------+----------------------------------+

            mFetchedBytes = 5 + aRowSize;
        }
    }

    boolean isBegun()
    {
        return mBegun;
    }

    void init()
    {
        mFetchRemains = true;
        mFetchedRowCount = 0;
        mTotalReceivedRowCount = 0;
        mFetchedBytes = 0;
        mTotalOctetLength = 0;

        if (mRowHandle != null)
        {
            mRowHandle.beforeFirst();
            mRowHandle.initToStore();
        }

        mBegun = false;
    }

    void fetchBegin(List<Column> aColumns)
    {
        mColumns = aColumns;
        mFetchRemains = true;
        mFetchedRowCount = 0;
        mTotalReceivedRowCount = 0;
        mFetchedBytes = 0;
        mTotalOctetLength = 0;

        if (mRowHandle == null)
        {
            mRowHandle = new DynamicArrayRowHandle();
        }
        else
        {
            mRowHandle.beforeFirst();
        }

        mRowHandle.setColumns(aColumns);
        mRowHandle.setPrepared(mIsPrepared);
        mRowHandle.initToStore();

        for (Column sColumn : aColumns)
        {
            mTotalOctetLength += sColumn.getOctetLength();
        }

        mBegun = true;
    }

    /**
     * CMP_OP_DB_FetchV2 operation ��û�ϱ� ���� �ʱ�ȭ�� �����Ѵ�.
     */
    void initFetchRequest()
    {
        mFetchedRowCount = 0;
        mFetchedBytes = 0;
    }
    
    void fetchEnd()
    {
        mFetchRemains = false;
    }

    /**
     * �÷��ε����� �ش��ϴ� DynamicArray�� �����Ѵ�.
     * @param aIndex �÷��ε���
     * @return �÷��ε����� �ش��ϴ� DynamicArray
     */
    public DynamicArray getDynamicArray(int aIndex)
    {
        return mRowHandle.getDynamicArray(aIndex);
    }

    public void setPrepared(boolean aIsPrepared)
    {
        this.mIsPrepared = aIsPrepared;
    }

    public void setRowHandle(RowHandle aRowHandle)
    {
        this.mRowHandle = aRowHandle;
    }

    public void setUseArrayListRowHandle(boolean aUseArrayListRowHandle)
    {
        this.mUseArrayListRowHandle = aUseArrayListRowHandle;
    }

    public boolean useArrayListRowHandle()
    {
        return mUseArrayListRowHandle;
    }
}
