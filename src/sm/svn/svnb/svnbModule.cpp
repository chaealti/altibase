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
 
#include <idl.h>
#include <ide.h>
#include <idu.h>
#include <smDef.h>
#include <svcRecord.h>
#include <svnModules.h>
#include <smnManager.h>
#include <smnbModule.h>

static IDE_RC svnbIndexMemoryNA(const smnIndexModule*);

smnIndexModule svnbModule =
{
    SMN_MAKE_INDEX_MODULE_ID( SMI_TABLE_VOLATILE,
                              SMI_BUILTIN_B_TREE_INDEXTYPE_ID ),
    SMN_RANGE_ENABLE        |
      SMN_DIMENSION_DISABLE |
      SMN_DEFAULT_ENABLE    |
      SMN_BOTTOMUP_BUILD_ENABLE,
    SMP_VC_PIECE_MAX_SIZE - 1,  // BUG-23113

    /* �Ʒ� �Լ� ������ ����� smnbBTree::prepareXXX, releaseXXX �Լ�����
       ����ؼ��� �ȵȴ�. �� �Լ����� �� ��� �ν��Ͻ���������
       ���Ǿ�� �Ѵ�. ��,
          smnbBTree::prepareIteratorMem
          smnbBTree::releaseIteratorMem
          smnbBTree::prepareFreeNodeMem
          smnbBTree::releaseFreeNodeMem
       ���� ����ϸ� �ȵȴ�. �ֳ��ϸ� smnbModule�� svnbModule��
       �� �Լ����� ���� ����ϸ� �� �Լ����� �����ϴ�
       mIteratorPool, gSmnbNodePool, gSmnbFreeNodeList ���� ���� �Ǵ� �������
       �����ϰ� �Ǵµ�, �̵鿡 ���� �ʱ�ȭ �� ���� �۾��� �ߺ����� �ϰ� �ȴ�.
       ���� volatile B-tree index module������ �� �Լ��� �ҷ��� ���
       �ƹ��� �۾��� ���� �ʵ��� �ϱ� ���� svnbIndexMemoryNA
       �Լ� �����͸� �޵��� �Ѵ�. */

    (smnMemoryFunc)       svnbIndexMemoryNA,
    (smnMemoryFunc)       svnbIndexMemoryNA,
    (smnMemoryFunc)       svnbIndexMemoryNA,
    (smnMemoryFunc)       svnbIndexMemoryNA,
    (smnCreateFunc)       smnbBTree::create,
    (smnDropFunc)         smnbBTree::drop,

    (smTableCursorLockRowFunc) smnManager::lockRow,
    (smnDeleteFunc)            smnbBTree::deleteNA,
    (smnFreeFunc)              smnbBTree::freeSlot,
    (smnInsertRollbackFunc)    NULL,
    (smnDeleteRollbackFunc)    NULL,
    (smnAgingFunc)             NULL,

    (smInitFunc)         smnbBTree::init,
    (smDestFunc)         smnbBTree::dest,
    (smnFreeNodeListFunc) smnbBTree::freeAllNodeList,
    (smnGetPositionFunc)  smnbBTree::getPosition,
    (smnSetPositionFunc)  smnbBTree::setPosition,

    (smnAllocIteratorFunc) smnbBTree::allocIterator,
    (smnFreeIteratorFunc)  smnbBTree::freeIterator,
    (smnReInitFunc)        NULL,
    (smnInitMetaFunc)      NULL,
    (smnMakeDiskImageFunc) smnbBTree::makeDiskImage,

    (smnBuildIndexFunc)     smnbBTree::buildIndex,
    (smnGetSmoNoFunc)       NULL,
    (smnMakeKeyFromRow)     NULL,
    (smnMakeKeyFromSmiValue)NULL,
    (smnRebuildIndexColumn) NULL,
    (smnSetIndexConsistency)NULL,
    (smnGatherStat)         smnbBTree::gatherStat
};

IDE_RC svnbIndexMemoryNA(const smnIndexModule*)
{
    return IDE_SUCCESS;
}

