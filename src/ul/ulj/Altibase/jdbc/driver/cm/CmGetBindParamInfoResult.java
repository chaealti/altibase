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

import java.util.ArrayList;
import java.util.List;

import Altibase.jdbc.driver.datatype.ColumnInfo;

public class CmGetBindParamInfoResult extends CmStatementIdResult
{
    public static final byte MY_OP = CmOperation.DB_OP_GET_PARAM_INFO_RESULT;
    
    private List mColumnInfoList = null;
    
    public CmGetBindParamInfoResult()
    {
        mColumnInfoList = new ArrayList();
    }
    
    public ColumnInfo getColumnInfo(int aParamIdx)
    {
        return (ColumnInfo)mColumnInfoList.get(aParamIdx - 1);
    }
    
    protected byte getResultOp()
    {
        return MY_OP;
    }

    public void addColumnInfo(int aParamIdx, ColumnInfo aColumnInfo)
    {
        if (mColumnInfoList.size() >= aParamIdx)
        {
            // BUG-42879 �̹� �÷������� �������� �ش��ϴ� ������ �����Ѵ�.
            mColumnInfoList.set(aParamIdx - 1, aColumnInfo);
        }
        else
        {
            /*
             * BUG-42879 deferred���¿��� ������ ������� setXXX�� ȣ��Ǵ� ��츦 ���� ó��.
             * ������� setXXX(3, 1)�� ���� ���� 3��° �ε����� ���� ȣ��Ǹ� �÷���������Ʈ�� ù��°��
             * �ι�° ����� null�� �ʱ�ȭ�ϰ� ����° ����� null�� �߰��Ѵ�.
             */
            for (int i = mColumnInfoList.size(); i < aParamIdx - 1; i++)
            {
                mColumnInfoList.add(null);
            }
            mColumnInfoList.add(aColumnInfo);
        }
    }

    public int getColumnInfoListSize()
    {
        return mColumnInfoList.size();
    }
    public void clear()
    {
        mColumnInfoList.clear();
    }
}
