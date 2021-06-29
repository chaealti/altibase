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
 * $Id: qmcTempTableMgr.cpp 82075 2018-01-17 06:39:52Z jina.kim $
 *
 *   Temp Table Manager
 *
 **********************************************************************/

#include <idl.h>
#include <ide.h>
#include <qmcTempTableMgr.h>
#include <smiTempTable.h>


IDE_RC
qmcTempTableMgr::init( qmcdTempTableMgr * aTableMgr,
                       iduMemory        * aMemory )
{
/***********************************************************************
 *
 * Description :
 *    Temp Table Manager�� �ʱ�ȭ�Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/
    
#define IDE_FN "qmcTempTableMgr::init"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY("qmcTempTableMgr::init"));

    IDE_DASSERT( aTableMgr != NULL );

    // Table Manager�� ��� �ʱ�ȭ
    aTableMgr->mTop = NULL;
    aTableMgr->mCurrent = NULL;
    aTableMgr->mMemory = NULL;

    // Table Handle ��ü ������ ���� Memory �������� �ʱ�ȭ
    IDU_FIT_POINT( "qmcTempTableMgr::init::alloc::mMemory",
                    idERR_ABORT_InsufficientMemory );

    IDE_TEST( aMemory->alloc( ID_SIZEOF(qmcMemory),
                              (void**) & aTableMgr->mMemory )
              != IDE_SUCCESS);

    aTableMgr->mMemory = new (aTableMgr->mMemory)qmcMemory();

    /* BUG-38290 */
    aTableMgr->mMemory->init( ID_SIZEOF(qmcCreatedTable) );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
    
#undef IDE_FN
}

/* BUG-38290 
 * Table manager �� addTempTable �Լ��� ���ÿ� ȣ��� �� ���
 * ���ü� ������ �߻����� �ʴ´�.
 * ���� addTempTable �Լ��� mutex �� ���� ���ü� ��� �� �ʿ䰡 ����.
 *
 * ���ü� ������ �߻��Ϸ��� addTempTable �Լ��� ���ÿ� ȣ��Ǿ�� �Ѵ�.
 * �׷��� ���ؼ��� service thread �� worker thread,
 * Ȥ�� worker thread �� worker thread �� ���ÿ� temp table �� �����ؾ� �Ѵ�.
 *
 * ���� temp table �� �����ϴ� ���� ���� ������ firstInit �Լ��̴�.
 *   AGGR
 *   CUBE
 *   GRAG
 *   HASH
 *   HSDS
 *   ROLL
 *   SDIF
 *   SITS
 *   WNST
 *
 * Worker thread �� ���� init �� ����Ƿ��� PRLQ �� �Ʒ���
 * �÷� ��尡 ��ġ�ؾ� �ϴµ�, �� ������ ���� PRLQ �� �Ʒ��� �� �� ����.
 * �׷��Ƿ� �Ϲ����� ��쿡�� worker thread �� ���� temp table ��
 * ������ �� ����, ���ü� ������ �߻����� �ʴ´�.
 *
 * �ٸ� �Ѱ��� ���ܷμ� �� ������ subquery filter �� ������ ��쿡��
 * worker thread �� ���� init �� �� �ִ�.
 *
 * ������ subquery filter �� partitioned table �� SCAN �� �޸� �� �����Ƿ�
 * PRLQ �Ʒ��� �ִ� SCAN �� subqeury filter �� ������ ����
 * HASH, SORT, GRAG ���� ���� ���ѵȴ�.
 *
 * �̷� ������ �� ��尡 init �� �� SCAN �� doIt �� ��� ��ġ�Ƿ�
 * ���ü� ������ �߻��Ϸ��� �� �� �̻��� HASH, SORT Ȥ�� GRAG ��
 * ���ÿ� init �Ǿ�� �Ѵ�.
 *
 * ������ �÷� ��尡 ���ÿ� init �Ǵ� ���� �������� �����Ƿ�,
 * temp table manager �� ���ü� ������ �߻����� �ʴ´�.
 */
IDE_RC qmcTempTableMgr::addTempTable( iduMemory        * aMemory,
                                      qmcdTempTableMgr * aTableMgr,
                                      void             * aTableHandle )
{
/***********************************************************************
 *
 * Description :
 *    Temp Table Manager�� ������ Temp Table�� Handle�� ����Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/
    
#define IDE_FN "qmcTempTableMgr::addTempTable"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY("qmcTempTableMgr::addTempTable"));

    qmcCreatedTable * sTable;

    // Table Handle ������ ���� ���� �Ҵ�
    IDU_FIT_POINT( "qmcTempTableMgr::addTempTable::alloc::sTable",
                    idERR_ABORT_InsufficientMemory );

    /* BUG-38290 */
    IDE_TEST( aTableMgr->mMemory->alloc( aMemory,
                                         ID_SIZEOF(qmcCreatedTable),
                                         (void**) & sTable )
              != IDE_SUCCESS);

    // Table Handle ������ Setting
    sTable->tableHandle = aTableHandle;
    sTable->next   = NULL;

    // Temp Table Manager�� ����
    if ( aTableMgr->mTop == NULL )
    {
        // ���� ����� ���
        aTableMgr->mTop = aTableMgr->mCurrent = sTable;
    }
    else
    {
        // �̹� ��ϵǾ� �ִ� ���
        aTableMgr->mCurrent->next = sTable;
        aTableMgr->mCurrent = sTable;
    }
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
    
#undef IDE_FN
}


IDE_RC
qmcTempTableMgr::dropAllTempTable( qmcdTempTableMgr   * aTableMgr )
{
/***********************************************************************
 *
 * Description :
 *     Temp Table Manager�� ��ϵ� ��� Table Handle�� ����
 *     Temp Table���� ��� �����Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/
    
#define IDE_FN "qmcTempTableMgr::dropAllTempTable"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY("qmcTempTableMgr::dropAllTempTable"));

    qmcCreatedTable * sTable;

    // ��� Table Handle�� ���Ͽ� Table�� Drop �Ѵ�.
    for ( sTable = aTableMgr->mTop;
          sTable != NULL;
          sTable = sTable->next )
    {
        IDE_TEST(smiTempTable::drop( sTable->tableHandle ) != IDE_SUCCESS);
    }

    // ������ ���� Memory�� Clear�Ѵ�.
    if ( aTableMgr->mMemory != NULL )
    {
        // To fix BUG-17591
        // �� �Լ��� �Ҹ��� ������ qmxMemory�� query statement������
        // execute������������ ���ư� �� �̱� ������
        // �޸𸮸� ���� �ʱ�ȭ �ϱ� ���� qmcMemory::clear�Լ��� ȣ���Ѵ�.
        aTableMgr->mMemory->clear( ID_SIZEOF(qmcCreatedTable) );
    }
    aTableMgr->mTop = aTableMgr->mCurrent = NULL;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    // Error �߻� �� ���� �ִ� Handle�� ��� �����Ѵ�.
    sTable = sTable->next;

    for ( ; sTable != NULL; sTable = sTable->next )
    {
        (void) smiTempTable::drop( sTable->tableHandle );
    } 
    if ( aTableMgr->mMemory != NULL )
    {
        aTableMgr->mMemory->clear( ID_SIZEOF(qmcCreatedTable) );
    }

    aTableMgr->mTop = aTableMgr->mCurrent = NULL;

    return IDE_FAILURE;
    
#undef IDE_FN
}
