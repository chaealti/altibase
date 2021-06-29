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

package Altibase.jdbc.driver.datatype;

import java.sql.BatchUpdateException;
import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;

import Altibase.jdbc.driver.util.DynamicArray;
import Altibase.jdbc.driver.util.DynamicArrayCursor;

public class DynamicArrayRowHandle implements RowHandle
{
    protected DynamicArrayCursor mStoreCursor;
    protected DynamicArrayCursor mLoadCursor;
    private List<DynamicArray>   mArrays;
    private List<Column>         mColumns;
    private HashSet<Integer>     mHoleSet;
    private boolean              mIsPrepared;     // BUG-47460 prepared fetch���� ����

    public DynamicArrayRowHandle()
    {
        mStoreCursor = new DynamicArrayCursor();
        mLoadCursor = new DynamicArrayCursor();
        mArrays = null;
        mIsPrepared = false;
    }

    public void setColumns(List<Column> aColumns)
    {
        mColumns = aColumns;
        if (mArrays == null || mArrays.size() != mColumns.size())
        {
            mArrays = new ArrayList<DynamicArray>(aColumns.size());
            for (Column sBindColumn : aColumns)
            {
                // BUG-47460 �÷���ü�� �ʱ�ȭ�ϸ鼭 DynaminArray�� �̸� �����Ѵ�.
                DynamicArray sArray = sBindColumn.createTypedDynamicArray();
                sArray.setCursor(mStoreCursor, mLoadCursor);
                mArrays.add(sArray);
            }
        }
    }

    /*
     * store ���� �޼ҵ�
     *  - initToStore(): ĳ�ø� �ʱ�ȭ�ϰ� ó������ store�ϱ� ���� ȣ���Ѵ�.
     *  - cacheSize(): ���� ĳ�õǾ� �ִ� row�� ������ ���Ѵ�.
     *  - store(): row�� �ϳ� ĳ���Ѵ�.
     */
    public void initToStore()
    {
        // BUG-47460 ������ ���� direct execute�϶��� Dynamic Array�� üũ�Ѵ�.
        // BUG-48280 dequeue ���� ��� ���� fetch ����� ���� ������ mColumns���� null�� ���� �� �ִ�.
        if (!mIsPrepared && mColumns != null)
        {
            for (int i = 0; i < mColumns.size(); i++)
            {
                Column sBindColumn = mColumns.get(i);
                DynamicArray sArray = mArrays.get(i);
                if (!sBindColumn.isArrayCompatible(sArray))
                {
                    // ���� column�� ���õ� �� array�� ��������� �ʾ��� ��쳪
                    // array�� ������µ� �� ���� �÷� Ÿ���� �ٲ� ���
                    sArray = sBindColumn.createTypedDynamicArray();
                    sArray.setCursor(mStoreCursor, mLoadCursor);
                    mArrays.set(i, sArray);
                }
            }
        }
        mStoreCursor.setPosition(0);
        if (mHoleSet != null)
        {
            mHoleSet.clear();
        }
    }

    public int size()
    {
        return mStoreCursor.getPosition();
    }

    public void store()
    {
        // ù��° index�� ������.
        // 0�� beforeFirst�� ���� index�� ����ϱ� �����̴�.
        mStoreCursor.next();
        for (int i = 0; i < mColumns.size(); i++)
        {
            mColumns.get(i).storeTo(mArrays.get(i));
        }
    }

    public void increaseStoreCursor()
    {
        mStoreCursor.next();
    }

    public void storeLobResult4Batch() throws BatchUpdateException
    {
        mStoreCursor.next();
        mLoadCursor.next();
        for (int i = 0; i < mColumns.size(); i++)
        {
            Column sBindColumn = mColumns.get(i);
            DynamicArray sArray = mArrays.get(i);
            if (sBindColumn instanceof BlobLocatorColumn)
            {
                ((BlobLocatorColumn)sBindColumn).storeToLobArray(sArray);
            }
            else if (sBindColumn instanceof ClobLocatorColumn)
            {
                ((ClobLocatorColumn)sBindColumn).storeToLobArray(sArray);
            }
        }
    }

    public void update()
    {
        int sOldPos = mStoreCursor.getPosition();
        mStoreCursor.setPosition(mLoadCursor.getPosition());
        for (int i = 0; i < mColumns.size(); i++)
        {
            mColumns.get(i).storeTo(mArrays.get(i));
        }
        mStoreCursor.setPosition(sOldPos);
    }

    /*
     * row cursor position getter
     *  - getPosition(): ���� row ��ġ�� ���Ѵ�.
     *  - isBeforeFirst()
     *  - isAfterLast()
     *  - isFirst()
     *  - isLast()
     */
    public int getPosition()
    {
        return mLoadCursor.getPosition();
    }

    public boolean isBeforeFirst()
    {
        return mLoadCursor.isBeforeFirst();
    }

    public boolean isFirst()
    {
        return mLoadCursor.isFirst();
    }

    public boolean isLast()
    {
        return mLoadCursor.equals(mStoreCursor);
    }

    public boolean isAfterLast()
    {
        return !mStoreCursor.geThan(mLoadCursor);
    }

    /*
     * row cursor positioning �޼ҵ�
     *  - setPosition()
     *  - beforeFirst()
     *  - afterLast()
     *  - next()
     *  - previous()
     */
    public boolean setPosition(int aPos)
    {
        if (aPos <= 0)
        {
            mLoadCursor.setPosition(0);
            return false;
        }
        else if (aPos > mStoreCursor.getPosition())
        {
            mLoadCursor.setPosition(mStoreCursor.getPosition() + 1);
            return false;
        }
        else
        {
            mLoadCursor.setPosition(aPos);
            load();
            return true;
        }
    }

    public void beforeFirst()
    {
        mLoadCursor.setPosition(0);
    }

    public void afterLast()
    {
        mLoadCursor.setPosition(mStoreCursor.getPosition() + 1);
    }

    public boolean next()
    {
        if (isAfterLast())
        {
            return false;
        }

        do
        {
            mLoadCursor.next();
        } while (mHoleSet != null && mHoleSet.contains(mLoadCursor.getPosition()));

        return checkAndLoad();
    }

    public boolean previous()
    {
        if (isBeforeFirst())
        {
            return false;
        }

        do
        {
            mLoadCursor.previous();
        } while (mHoleSet != null && mHoleSet.contains(mLoadCursor.getPosition()));

        return checkAndLoad();
    }

    public void reload()
    {
        checkAndLoad();
    }

    private boolean checkAndLoad()
    {
        if (!mLoadCursor.isBeforeFirst() && mStoreCursor.geThan(mLoadCursor))
        {
            load();
            return true;
        }
        return false;
    }

    private void load()
    {
        for (int i = 0; i < mColumns.size(); i++)
        {
            mColumns.get(i).loadFrom(mArrays.get(i));
        }
    }

    /**
     * ���� Ŀ�� ��ġ�� Row�� �����.
     */
    public void delete()
    {
        if (mHoleSet == null)
        {
            mHoleSet = new HashSet<Integer>();
        }
        mHoleSet.add(mLoadCursor.getPosition());
    }

    public void changeBindColumnType(int aIndex, Column aColumn, ColumnInfo aColumnInfo,
                                     byte aInOutType)
    {
        mColumns.set(aIndex, aColumn);
        mArrays.set(aIndex, aColumn.createTypedDynamicArray());
        mArrays.get(aIndex).setCursor(mStoreCursor, mLoadCursor);
        ColumnInfo sColumnInfo = (ColumnInfo)aColumnInfo.clone();
        sColumnInfo.modifyInOutType(aInOutType);
        mColumns.get(aIndex).getDefaultColumnInfo(sColumnInfo);
        sColumnInfo.addInType();
        mColumns.get(aIndex).setColumnInfo(sColumnInfo);
    }

    /**
     * �÷��ε����� �ش��ϴ� DynamicArray�� �����Ѵ�.
     * @param aIndex �÷��ε���
     * @return �÷��ε����� �ش��ϴ� DynamicArray
     */
    public DynamicArray getDynamicArray(int aIndex)
    {
        return mArrays.get(aIndex);
    }

    public void setPrepared(boolean aIsPrepared)
    {
        mIsPrepared = aIsPrepared;
    }
}
