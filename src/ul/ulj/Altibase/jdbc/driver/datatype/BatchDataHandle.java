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

import java.sql.SQLException;
import java.util.List;

  // PROJ-2368
  /**
   * @author pss4you
   * ListBufferHandle �� RowHandle �� ���� Interface
   * �� ��ü ���̿� ���� �Ǵ� �κ��� ���⼭ Define �Ͽ� Code �� �����ϰ� ����� ����
   */
public interface BatchDataHandle
{
    /**
     * DataHandle �� ����� �۾� ���ڸ� ��ȯ �Ѵ�.
     * 
     * @return ���� ���� ����� �۾� ����
     */
    int size();

    /**
     * DataHandle �� Binding �� �۾��� �����Ѵ�.
     */
    void store() throws SQLException;

    /**
     * DataHandle �� �ʱ�ȭ �Ѵ�.
     */
    void initToStore();

    /**
     * DataHandle �� Binding �� Column ���� setting �Ѵ�.
     */
    void setColumns(List<Column> aColumns);

    /**
     * ���� Bind Column�� ���ο� Type���� ���ε��� �� ��� �ش��ϴ� ���� type���� bind column type�� �����Ѵ�.
     * @param aIndex �÷��ε��� (base 0)
     * @param aColumn Column ����
     * @param aColumnInfo Column ��Ÿ����
     * @param aInOutType InOutType ����
     */
    void changeBindColumnType(int aIndex, Column aColumn, ColumnInfo aColumnInfo, byte aInOutType);
}
