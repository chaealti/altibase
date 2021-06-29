/** 
 *  Copyright (c) 1999~2017, Altibase Corp. and/or its affiliates. All rights reserved.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU Affero General Public License, version 3,
 *  as published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU Affero General Public License for more details.
 *
 *  You should have received a copy of the GNU Affero General Public License
 *  along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
 

/***********************************************************************
 * $Id: qmcTempTableMgr.h 82075 2018-01-17 06:39:52Z jina.kim $
 *
 *  Temp Table Manager
 *
 *     ���� ó�� ���� �߿� ������ Temp Table����
 *     ���� ���� �� ��� �ڿ��� �����Ѵ�.
 *     Memory Temp Table�� ��� Memory �����ڿ� ���� �ڵ� �����Ǹ�,
 *     Disk Temp Table�� ��� Table Handle�� ����ϰ�,
 *     ���� ���� �� Table Handle�� �̿��Ͽ� ��� Temp Table���� �����Ѵ�.
 *
 **********************************************************************/

#ifndef _O_QMC_TEMP_TABLE_MGR_H_
#define _O_QMC_TEMP_TABLE_MGR_H_ 1

#include <smiTable.h>
#include <qcuError.h>
#include <qmcMemory.h>

//---------------------------------
// ������ Temp Table�� Handle�� list
//---------------------------------

typedef struct qmcCreatedTable
{
    void            * tableHandle;
    qmcCreatedTable * next;
} qmcCreatedTable;

//---------------------------------
// Temp Table ������
//---------------------------------

typedef struct qmcdTempTableMgr
{
    qmcMemory       * mMemory;
    qmcCreatedTable * mTop;
    qmcCreatedTable * mCurrent;
} qmcdTempTableMgr;

class qmcTempTableMgr
{
public:
    static IDE_RC init( qmcdTempTableMgr * aTableMgr,
                        iduMemory * aMemory );
    static IDE_RC addTempTable( iduMemory        * aMemory,
                                qmcdTempTableMgr * aTableMgr,
                                void             * aTableHandle );
    static IDE_RC dropAllTempTable( qmcdTempTableMgr   * aTableMgr );
};

#endif /* _O_QMC_TEMP_TABLE_MGR_H_ */

