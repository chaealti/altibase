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

import java.util.ArrayList;
import java.util.List;

public abstract class DynamicArray
{
    /*
     * chunk�� ����� ������ ����.
     * data[][]: 2���� �迭
     * data[0] = [][][][][]... : 64��
     * data[1] = [][][][][][][][][][][][][][][][]... : 256�� (4�辿 �þ��) ...
     */
    private static final int     INIT_CHUNK_SIZE   = 64;
    private static final int     MAX_CHUNK_SIZE    = 65536;
    private static final int     GROW_FACTOR       = 4;
    private static int           DYNAMIC_ARRY_SIZE = 349504; // BUG-43263 ������ DynamicArray�� �� �� �־��� �ִ�ġ
    private static List<Integer> CHUNK_SIZES       = makeChunkSizeArray();

    protected DynamicArrayCursor mStoreCursor;
    protected DynamicArrayCursor mLoadCursor;
    
    private int mLastAllocatedChunkIndex = -1;

    public DynamicArray()
    {
        allocChunkEntry(CHUNK_SIZES.size());
    }

    /**
     * BUG-43263 DynamicArray�� �� �� �ִ� �ִ밪�� �̿��� CHUNK SIZE ����Ʈ�� �����.<br>
     * CHUNK SIZE�� �ʱⰪ�� 64�̰� 4�辿 �����ϸ� 65536�� �ʰ����� �ʴ´�.
     * @return the list of chunk sizes.
     */
    private static List<Integer> makeChunkSizeArray()
    {
        List<Integer> sChunkSizeArray = new ArrayList<Integer>();
        int sChunkSize = INIT_CHUNK_SIZE;
        int sDynamicArrySize = DYNAMIC_ARRY_SIZE;

        while (sDynamicArrySize > 0)
        {
            sChunkSizeArray.add(sChunkSize);
            sDynamicArrySize -= sChunkSize;
            if (sChunkSize < MAX_CHUNK_SIZE)
            {
                sChunkSize *= GROW_FACTOR;
            }
        }

        return sChunkSizeArray;
    }

    public void setCursor(DynamicArrayCursor aStoreCursor, DynamicArrayCursor aLoadCursor)
    {
        mStoreCursor = aStoreCursor;
        mLoadCursor = aLoadCursor;
    }

    protected void checkChunk()
    {
        while (true)
        {
            if (mLastAllocatedChunkIndex < mStoreCursor.chunkIndex())
            {
                mLastAllocatedChunkIndex++;
                expand(mLastAllocatedChunkIndex, CHUNK_SIZES.get(mLastAllocatedChunkIndex));
            }
            else
            {
                break;
            }
        }
    }

    public static int getDynamicArrySize()
    {
        return DYNAMIC_ARRY_SIZE;
    }

    public static List<Integer> getChunkSizes()
    {
        return CHUNK_SIZES;
    }

    protected abstract void allocChunkEntry(int aChunkCount);
    
    protected abstract void expand(int aChunkIndex, int aChunkSize);
}
