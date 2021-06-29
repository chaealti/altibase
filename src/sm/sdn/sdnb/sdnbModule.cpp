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
 * $Id: sdnbModule.cpp 90899 2021-05-27 08:55:20Z jiwon.kim $
 **********************************************************************/

/*********************************************************************
 * FILE DESCRIPTION : sdnbModule.cpp                                 *
 * ------------------------------------------------------------------*
 * �� ������ ��ũ ����� �Ϲ� ���̺� �����Ǵ� B-Tree �ε����� �� *
 * �� Ŭ������ sdnbBTree�� ���� �����̴�.                            *
 *                                                                   *
 * ��ũ ��� �ε����� ���, �޸� �ε����� �����ϰ� static index  *
 * header�� meta table�� variable slot�� ����Ǹ�, run time header�� *
 * ���� �޸� ������ �����ȴ�.                                      *
 *                                                                   *
 * �޸� �ε����� �ٸ� ���� �ε��� ��ü�� ��ũ�� �����ϸ�, �ε��� *
 * ���� ������ logging �� recovery �Ǹ�, system restart�ÿ� re-build *
 * ���� �ʰ�, run time header�� �ٽ� �����ȴٴ� ���̴�.              *
 *                                                                   *
 * ���� leaf node�� key�� ����Ǵ� ���� row�� �����Ͱ� �ƴ�, ����    *
 * key���� ���纻�̸�, internal node���� �� key���� prefix���� ����  *
 * �ȴ�. ����, key�� ���� �񱳽�(key range �����), �ش� �÷���    *
 * offset�� ���� row���� ���� �� (filter �����)�� �ٸ���, �̸� �� *
 * �� key������ column offset�� ����ؼ� header�� ������ �ξ�� �Ѵ�.*
 *                                                                   *
 * index�� ������ �� �ִ� �ִ� key���� ���̴� �� node�� �ּ��� 2���� *
 * key�� �� �� �ֵ��� �ϱ� ���Ͽ� ���� ���Ŀ� ���Ѵ�.            *
 *                                                                   *
 *   Max_Key_Len = ( Page_Size * ( 100 - PCT_FREE ) / 100 ) / 2      *
 *                                                                   *
 * �� �ε����� SMO�� index�� run-time header�� �����ϴ� latch��      *
 * SmoNo�� �̿��Ͽ� serialize�ϸ�, SMO�� ������ Node�鿡 ���� SmoNo  *
 * �� ����� �ξ� SMO�� �߻��� ������ ����Ѵ�.                      *
 *                                                                   *
 * internal node traverse�ÿ� latch�� ���� �ʰ� peterson algorithm�� *
 * �̿��Ͽ� SMO�� �˻��ϸ�, SMO�� �߻��ϸ� run-time header�� �޸�    *
 * tree latch���� ����� �� �ٽ� �õ��Ѵ�.                           *
 *                                                                   *
 * Insert ������ �켱 optimistic����(no latch traverse �� target     *
 * leaf�� x latch) ������ �õ��� ����, SMO�� �߻��ؾ� �Ѵٸ� ���    *
 * latch�� �����ϰ�, tree latch�� ��� �ٽ� traverse�� �� ��, taget  *
 * �� left node, target node, target�� right node ������ x-latch��   *
 * ȹ������ SMO ������ �����ϰ� �� key�� Insert�� �Ѵ�. �Էµ� key�� *
 * �ʱ� createSCN�� �� key�� �Է��� stmt�� beginSCN�̸�, limit SCN�� *
 * ���Ѵ� ���̴�.                                                    *
 *                                                                   *
 * delete ������ Garbage Collector�� ���� ����ȴ�. ���� �����      *
 * Insert�� ���� �����ϰ� optimistic�� �õ��� �� SMO�� �߻��ϸ�    *
 * pessimistic���� �����Ѵ�. �Ϲ� Ʈ����ǿ� ���� delete�� �ܼ���    *
 * key slot�� limit SCN�� key�� delete�� stmt�� begin SCN����        *
 * �����ϴ� ���̴�.                                                  *
 *                                                                   *
 * SMO �������� ���� �и��� �� node�� splitter(prefix)�� ���� ���� *
 * �ø� ��, prefix�� ū ���� key������ ���õȴ�.                     *
 *                                                                   *
 * index key�� ���� update ������ �������� �ʴ´�.                   *
 *                                                                   *
 * �� �ε������� ���� leaf node�� ���� latch�� ���ÿ� ��ƾ� �� ��� *
 * ���� !!�ݵ��!! left���� right �������� �õ��ؾ� �Ѵ�. �̸� ��Ű  *
 * �� ������ deadlock�� �߻��� �� �ִ�.                              *
 *                                                                   *
 * fetch ���� �� Node���� �̵��� �ʿ��� ��쿡, fetchNext�� left���� *
 * right�� �̵��ϹǷ� ����� ������, fetchPrev�� ��쿡�� ��� �̵�  *
 * ���� left node�� SMO�� �߻��ߴ����� üũ�ϱ� ����, ���� ����    *
 * left ��带 ���ÿ� latch �Ͽ��� �Ѵ�.                             *
 * �̶� deadlock�� �߻��ϴ� ���� ���� ���Ͽ�, ���� ��带 Ǯ��, left *
 * ��带 ��� ���� ���� ��带 �ٽ� ��� ����� ����Ѵ�.           *
 *********************************************************************/

#include <mte.h>

#include <smDef.h>
#include <smnDef.h>
#include <smErrorCode.h>
#include <smu.h>
#include <sdp.h>
#include <sdc.h>
#include <sdb.h>
#include <sds.h>
#include <sdn.h>
#include <sdnReq.h>
#include <sdbMPRMgr.h>
#include <smxTrans.h>
#include <sdrMiniTrans.h>
#include <smiMisc.h>

extern smiGlobalCallBackList gSmiGlobalCallBackList;

static UInt  gMtxDLogType = SM_DLOG_ATTR_DEFAULT;

sdnCallbackFuncs gCallbackFuncs4CTL =
{
    (sdnSoftKeyStamping)sdnbBTree::softKeyStamping,
    (sdnHardKeyStamping)sdnbBTree::hardKeyStamping
};

smnIndexModule sdnbModule =
{
    SMN_MAKE_INDEX_MODULE_ID( SMI_TABLE_DISK,
                              SMI_BUILTIN_B_TREE_INDEXTYPE_ID ),
    SMN_RANGE_ENABLE|SMN_DIMENSION_DISABLE|SMN_DEFAULT_ENABLE|SMN_BOTTOMUP_BUILD_ENABLE,
    0,
    (smnMemoryFunc)       sdnbBTree::prepareIteratorMem,
    (smnMemoryFunc)       sdnbBTree::releaseIteratorMem,
    (smnMemoryFunc)       NULL, // prepareFreeNodeMem
    (smnMemoryFunc)       NULL, // releaseFreeNodeMem
    (smnCreateFunc)       sdnbBTree::create,
    (smnDropFunc)         sdnbBTree::drop,

    (smTableCursorLockRowFunc) sdnbBTree::lockRow,
    (smnDeleteFunc)            sdnbBTree::deleteKey,
    (smnFreeFunc)              NULL,
    (smnInsertRollbackFunc)    sdnbBTree::insertKeyRollback,
    (smnDeleteRollbackFunc)    sdnbBTree::deleteKeyRollback,
    (smnAgingFunc)             sdnbBTree::aging,

    (smInitFunc)          sdnbBTree::init,
    (smDestFunc)          sdnbBTree::dest,
    (smnFreeNodeListFunc) sdnbBTree::freeAllNodeList,
    (smnGetPositionFunc)  sdnbBTree::getPosition,
    (smnSetPositionFunc)  sdnbBTree::setPosition,

    (smnAllocIteratorFunc) sdnbBTree::allocIterator,
    (smnFreeIteratorFunc)  sdnbBTree::freeIterator,
    (smnReInitFunc)        NULL,
    (smnInitMetaFunc)      sdnbBTree::initMeta,

    /* FOR A4 : Index building
       DRDB Table�� ���� parallel index build�� �������� ����.
       index�� create�Ҷ��� sequential iterator�� �̿��Ͽ�
       row�� �ϳ��� �ϳ��� insert�� ����.
       ���� �ε����� disk �̹����� ���� �ʿ䵵 ����.
    */
    (smnMakeDiskImageFunc)  NULL,
    (smnBuildIndexFunc)     sdnbBTree::buildIndex,
    (smnGetSmoNoFunc)       sdnbBTree::getSmoNo,
    (smnMakeKeyFromRow)     sdnbBTree::makeKeyValueFromRow,
    (smnMakeKeyFromSmiValue)sdnbBTree::makeKeyValueFromSmiValueList,
    (smnRebuildIndexColumn) sdnbBTree::rebuildIndexColumn,
    (smnSetIndexConsistency)sdnbBTree::setConsistent,
    (smnGatherStat)         sdnbBTree::gatherStat
};


static const  smSeekFunc sdnbSeekFunctions[32][12] =
{
    { /* SMI_TRAVERSE_FORWARD  SMI_PREVIOUS_DISABLE SMI_LOCK_READ            */
        (smSeekFunc)sdnbBTree::fetchNext,
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::beforeFirst,
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::retraverse,
        (smSeekFunc)sdnbBTree::beforeFirst,
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::retraverse,
        (smSeekFunc)sdnbBTree::beforeFirst
    },
    { /* SMI_TRAVERSE_FORWARD  SMI_PREVIOUS_DISABLE SMI_LOCK_WRITE           */
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::beforeFirstW,
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::retraverse,
        (smSeekFunc)sdnbBTree::beforeFirst,
        (smSeekFunc)sdnbBTree::fetchNext,
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::retraverse,
        (smSeekFunc)sdnbBTree::beforeFirst
    },
    { /* SMI_TRAVERSE_FORWARD  SMI_PREVIOUS_DISABLE SMI_LOCK_REPEATABLE      */
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::beforeFirstRR,
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::retraverse,
        (smSeekFunc)sdnbBTree::beforeFirst,
        (smSeekFunc)sdnbBTree::fetchNext,
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::beforeFirst,
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::retraverse,
        (smSeekFunc)sdnbBTree::beforeFirst
    },
    { /* SMI_TRAVERSE_FORWARD  SMI_PREVIOUS_DISABLE SMI_LOCK_TABLE_SHARED    */
        (smSeekFunc)sdnbBTree::fetchNext,
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::beforeFirst,
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::retraverse,
        (smSeekFunc)sdnbBTree::beforeFirst,
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::retraverse,
        (smSeekFunc)sdnbBTree::beforeFirst
    },
    { /* SMI_TRAVERSE_FORWARD  SMI_PREVIOUS_DISABLE SMI_LOCK_TABLE_EXCLUSIVE */
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::beforeFirstW,
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::retraverse,
        (smSeekFunc)sdnbBTree::beforeFirst,
        (smSeekFunc)sdnbBTree::fetchNext,
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::retraverse,
        (smSeekFunc)sdnbBTree::beforeFirst
    },
    { /* UNUSED                                                              */
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::NA,
    },
    { /* UNUSED                                                              */
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::NA,
    },
    { /* UNUSED                                                              */
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::NA,
    },
    { /* SMI_TRAVERSE_FORWARD  SMI_PREVIOUS_ENABLE  SMI_LOCK_READ            */
        (smSeekFunc)sdnbBTree::fetchNext,
        (smSeekFunc)sdnbBTree::fetchPrev,
        (smSeekFunc)sdnbBTree::beforeFirst,
        (smSeekFunc)sdnbBTree::afterLast,
        (smSeekFunc)sdnbBTree::retraverse,
        (smSeekFunc)sdnbBTree::beforeFirst,
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::retraverse,
        (smSeekFunc)sdnbBTree::beforeFirst
    },
    { /* SMI_TRAVERSE_FORWARD  SMI_PREVIOUS_ENABLE  SMI_LOCK_WRITE           */
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::beforeFirstW,
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::retraverse,
        (smSeekFunc)sdnbBTree::beforeFirst,
        (smSeekFunc)sdnbBTree::fetchNext,
        (smSeekFunc)sdnbBTree::fetchPrev,
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::afterLast,
        (smSeekFunc)sdnbBTree::retraverse,
        (smSeekFunc)sdnbBTree::beforeFirst
    },
    { /* SMI_TRAVERSE_FORWARD  SMI_PREVIOUS_ENABLE  SMI_LOCK_REPEATABLE      */
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::beforeFirstRR,
        (smSeekFunc)sdnbBTree::afterLastRR,
        (smSeekFunc)sdnbBTree::retraverse,
        (smSeekFunc)sdnbBTree::beforeFirst,
        (smSeekFunc)sdnbBTree::fetchNext,
        (smSeekFunc)sdnbBTree::fetchPrev,
        (smSeekFunc)sdnbBTree::beforeFirst,
        (smSeekFunc)sdnbBTree::afterLast,
        (smSeekFunc)sdnbBTree::retraverse,
        (smSeekFunc)sdnbBTree::beforeFirst
    },
    { /* SMI_TRAVERSE_FORWARD  SMI_PREVIOUS_ENABLE  SMI_LOCK_TABLE_SHARED    */
        (smSeekFunc)sdnbBTree::fetchNext,
        (smSeekFunc)sdnbBTree::fetchPrev,
        (smSeekFunc)sdnbBTree::beforeFirst,
        (smSeekFunc)sdnbBTree::afterLast,
        (smSeekFunc)sdnbBTree::retraverse,
        (smSeekFunc)sdnbBTree::beforeFirst,
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::retraverse,
        (smSeekFunc)sdnbBTree::beforeFirst
    },
    { /* SMI_TRAVERSE_FORWARD  SMI_PREVIOUS_ENABLE  SMI_LOCK_TABLE_EXCLUSIVE */
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::beforeFirstW,
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::retraverse,
        (smSeekFunc)sdnbBTree::beforeFirst,
        (smSeekFunc)sdnbBTree::fetchNext,
        (smSeekFunc)sdnbBTree::fetchPrev,
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::afterLast,
        (smSeekFunc)sdnbBTree::retraverse,
        (smSeekFunc)sdnbBTree::beforeFirst
    },
    { /* UNUSED                                                              */
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::NA,
    },
    { /* UNUSED                                                              */
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::NA
    },
    { /* UNUSED                                                              */
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::NA
    },
    { /* SMI_TRAVERSE_BACKWARD SMI_PREVIOUS_DISABLE SMI_LOCK_READ            */
        (smSeekFunc)sdnbBTree::fetchPrev,
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::afterLast,
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::retraverse,
        (smSeekFunc)sdnbBTree::afterLast,
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::retraverse,
        (smSeekFunc)sdnbBTree::afterLast
    },
    { /* SMI_TRAVERSE_BACKWARD SMI_PREVIOUS_DISABLE SMI_LOCK_WRITE           */
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::afterLastW,
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::retraverse,
        (smSeekFunc)sdnbBTree::afterLast,
        (smSeekFunc)sdnbBTree::fetchPrev,
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::retraverse,
        (smSeekFunc)sdnbBTree::afterLast
    },
    { /* SMI_TRAVERSE_BACKWARD SMI_PREVIOUS_DISABLE SMI_LOCK_REPEATABLE      */
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::afterLastRR,
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::retraverse,
        (smSeekFunc)sdnbBTree::afterLast,
        (smSeekFunc)sdnbBTree::fetchPrev,
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::afterLast,
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::retraverse,
        (smSeekFunc)sdnbBTree::afterLast
    },
    { /* SMI_TRAVERSE_BACKWARD SMI_PREVIOUS_DISABLE SMI_LOCK_TABLE_SHARED    */
        (smSeekFunc)sdnbBTree::fetchPrev,
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::afterLast,
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::retraverse,
        (smSeekFunc)sdnbBTree::afterLast,
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::retraverse,
        (smSeekFunc)sdnbBTree::afterLast
    },
    { /* SMI_TRAVERSE_BACKWARD SMI_PREVIOUS_DISABLE SMI_LOCK_TABLE_EXCLUSIVE */
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::afterLastW,
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::retraverse,
        (smSeekFunc)sdnbBTree::afterLast,
        (smSeekFunc)sdnbBTree::fetchPrev,
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::retraverse,
        (smSeekFunc)sdnbBTree::afterLast
    },
    { /* UNUSED                                                              */
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::NA
    },
    { /* UNUSED                                                              */
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::NA
    },
    { /* UNUSED                                                              */
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::NA
    },
    { /* SMI_TRAVERSE_BACKWARD SMI_PREVIOUS_ENABLE  SMI_LOCK_READ            */
        (smSeekFunc)sdnbBTree::fetchPrev,
        (smSeekFunc)sdnbBTree::fetchNext,
        (smSeekFunc)sdnbBTree::afterLast,
        (smSeekFunc)sdnbBTree::beforeFirst,
        (smSeekFunc)sdnbBTree::retraverse,
        (smSeekFunc)sdnbBTree::afterLast,
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::retraverse,
        (smSeekFunc)sdnbBTree::afterLast
    },
    { /* SMI_TRAVERSE_BACKWARD SMI_PREVIOUS_ENABLE  SMI_LOCK_WRITE           */
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::afterLastW,
        (smSeekFunc)sdnbBTree::beforeFirst,
        (smSeekFunc)sdnbBTree::retraverse,
        (smSeekFunc)sdnbBTree::afterLast,
        (smSeekFunc)sdnbBTree::fetchPrev,
        (smSeekFunc)sdnbBTree::fetchNext,
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::retraverse,
        (smSeekFunc)sdnbBTree::afterLast
    },
    { /* SMI_TRAVERSE_BACKWARD SMI_PREVIOUS_ENABLE  SMI_LOCK_REPEATABLE      */
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::afterLastRR,
        (smSeekFunc)sdnbBTree::beforeFirstRR,
        (smSeekFunc)sdnbBTree::retraverse,
        (smSeekFunc)sdnbBTree::afterLast,
        (smSeekFunc)sdnbBTree::fetchPrev,
        (smSeekFunc)sdnbBTree::fetchNext,
        (smSeekFunc)sdnbBTree::afterLast,
        (smSeekFunc)sdnbBTree::beforeFirst,
        (smSeekFunc)sdnbBTree::retraverse,
        (smSeekFunc)sdnbBTree::afterLast
    },
    { /* SMI_TRAVERSE_BACKWARD SMI_PREVIOUS_ENABLE  SMI_LOCK_TABLE_SHARED    */
        (smSeekFunc)sdnbBTree::fetchPrev,
        (smSeekFunc)sdnbBTree::fetchNext,
        (smSeekFunc)sdnbBTree::afterLast,
        (smSeekFunc)sdnbBTree::beforeFirst,
        (smSeekFunc)sdnbBTree::retraverse,
        (smSeekFunc)sdnbBTree::afterLast,
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::retraverse,
        (smSeekFunc)sdnbBTree::afterLast
    },
    { /* SMI_TRAVERSE_BACKWARD SMI_PREVIOUS_ENABLE  SMI_LOCK_TABLE_EXCLUSIVE */
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::afterLastW,
        (smSeekFunc)sdnbBTree::beforeFirst,
        (smSeekFunc)sdnbBTree::retraverse,
        (smSeekFunc)sdnbBTree::afterLast,
        (smSeekFunc)sdnbBTree::fetchPrev,
        (smSeekFunc)sdnbBTree::fetchNext,
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::retraverse,
        (smSeekFunc)sdnbBTree::afterLast
    },
    { /* UNUSED                                                              */
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::NA,
    },
    { /* UNUSED                                                              */
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::NA
    },
    { /* UNUSED                                                              */
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::NA,
        (smSeekFunc)sdnbBTree::NA,
    }
};

extern "C"  SInt gCompareSmiColumnByColId(
                                    const void *aLhs,
                                    const void *aRhs )
{
    const smiColumn **sLhs = (const smiColumn **)aLhs;
    const smiColumn **sRhs = (const smiColumn **)aRhs;

    IDE_DASSERT(aLhs != NULL);
    IDE_DASSERT(aRhs != NULL);

    if ( (*sLhs)->id == (*sRhs)->id )
    {
        return 0;
    }
    else
    {
        if ( (*sLhs)->id > (*sRhs)->id )
        {
            return 1;
        }
        else
        {
            return -1;
        }
    }
}

extern "C"  SInt gCompareSmiColumnListByColId(
                                     const void *aLhs,
                                     const void *aRhs )
{
    const smiFetchColumnList *sLhs = (const smiFetchColumnList *)aLhs;
    const smiFetchColumnList *sRhs = (const smiFetchColumnList *)aRhs;

    IDE_DASSERT(aLhs != NULL);
    IDE_DASSERT(aRhs != NULL);

    if ( sLhs->column->id == sRhs->column->id )
    {
        return 0;
    }
    else
    {
        if ( sLhs->column->id > sRhs->column->id )
        {
            return 1;
        }
        else
        {
            return -1;
        }
    }
}

/*********************************************************************
 * FUNCTION DESCRIPTION : sdnbBTree::prepareIteratorMem              *
 * ------------------------------------------------------------------*
 * �� �Լ��� Iterator�� �Ҵ��� �� memory pool�� �ʱ�ȭ               *
 *********************************************************************/
IDE_RC sdnbBTree::prepareIteratorMem( smnIndexModule* aIndexModule)
{

    /* FOR A4 : Node Pool �Ҵ�, Ager init.....
       �� �� ����.
    */

    aIndexModule->mMaxKeySize = sdnbBTree::getMaxKeySize();

    return IDE_SUCCESS;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : sdnbBTree::releaseIteratorMem              *
 * ------------------------------------------------------------------*
 * �� �Լ��� Iterator memory pool�� ����                             *
 *********************************************************************/
IDE_RC sdnbBTree::releaseIteratorMem( const smnIndexModule* )
{

    /* FOR A4 : Node Pool ����, Ager destroy.....
       �� �� ����.
    */
    return IDE_SUCCESS;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : sdnbBTree::freeAllNodeList                 *
 * ------------------------------------------------------------------*
 * �ε����� ���� build�ϴٰ� ������ �߻��ϸ� ���ݱ��� ������ �ε���*
 * ������ �����ؾ� �Ѵ�. �� �Լ��� �̷��� ȣ��ȴ�.                *
 * ���׸�Ʈ���� �Ҵ�� ��� Page�� �����Ѵ�.                         *
 *********************************************************************/
IDE_RC sdnbBTree::freeAllNodeList(idvSQL          * aStatistics,
                                  smnIndexHeader  * aIndex,
                                  void            * aTrans)
{

    sdrMtx       sMtx;
    idBool       sMtxStart = ID_FALSE;

    sdnbHeader      * sIndex;
    sdpSegmentDesc  * sSegmentDesc;
    sdpSegMgmtOp    * sSegMgmtOp;

    sIndex = (sdnbHeader*)((smnIndexHeader*)aIndex)->mHeader;

    /* FOR A4 : index build�ÿ� ���� �߻��ϸ� ȣ���
       �ε����� ������ ��� ������ ������
    */
    // Index Header���� Segment Descriptor�� ������ ��� page�� �����Ѵ�.
    IDE_TEST( sdrMiniTrans::begin(aStatistics,
                                  &sMtx,
                                  aTrans,
                                  SDR_MTX_LOGGING,
                                  ID_FALSE,/*MtxUndoable(PROJ-2162)*/
                                  gMtxDLogType)
              != IDE_SUCCESS );
    sMtxStart = ID_TRUE;

    sSegmentDesc = &(sIndex->mSegmentDesc);
    sSegMgmtOp   = sdpSegDescMgr::getSegMgmtOp( &(sIndex->mSegmentDesc) );
    IDE_ERROR( sSegMgmtOp != NULL );
    IDE_TEST( sSegMgmtOp->mResetSegment( aStatistics,
                                         &sMtx,
                                         SC_MAKE_SPACE(aIndex->mIndexSegDesc),
                                         &(sSegmentDesc->mSegHandle),
                                         SDP_SEG_TYPE_INDEX )
              != IDE_SUCCESS );

    sMtxStart = ID_FALSE;
    IDE_TEST( sdrMiniTrans::commit(&sMtx) != IDE_SUCCESS );


    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if (sMtxStart == ID_TRUE)
    {
        (void)sdrMiniTrans::rollback(&sMtx);
    }

    return IDE_FAILURE;

}

/*********************************************************************
 * FUNCTION DESCRIPTION : PROJ-1872 Disk index ���� ���� ����ȭ      *
 * ------------------------------------------------------------------*
 *   Ű�� �����ϱ� ���ؼ��� Row�� Fetch�ؾ� �Ѵ�. �̶� ��� Į����   *
 * �ƴ�, Index�� �ɸ� Į���� Fetch�ؾ� �ϱ� ������ ���� FetchColumn- *
 * List4Key�� �����Ѵ�.                                              *
 *********************************************************************/
IDE_RC sdnbBTree::makeFetchColumnList4Index(
                                void                   *aTableHeader,
                                const sdnbHeader       *aIndexHeader,
                                UShort                  aIndexColCount)
{
    sdnbColumn          *sIndexColumn;
    const smiColumn     *sTableColumn;
    smiColumn           *sSmiColumnInFetchColumn;
    smiFetchColumnList  *sFetchColumnList;
    UInt                 sPrevColumnID    = 0;
    idBool               sIsOrderAsc      = ID_TRUE;
    UShort               sColumnSeq;
    UShort               sLoop;

    IDE_DASSERT( aTableHeader  != NULL );
    IDE_DASSERT( aIndexHeader  != NULL );
    IDE_DASSERT( aIndexColCount > 0 );

    //QP ������ Fetch Column List�� ������
    for ( sLoop = 0 ; sLoop < aIndexColCount ; sLoop++ )
    {
        sIndexColumn = &aIndexHeader->mColumns[sLoop];

        sColumnSeq   = sIndexColumn->mKeyColumn.id % SMI_COLUMN_ID_MAXIMUM;
        sTableColumn = smcTable::getColumn(aTableHeader, sColumnSeq);

        /* Proj-1872 DiskIndex ���屸�� ����ȭ
         *   FetchColumnList�� column�� �޸𸮴� index runtime header ������
         * FetchColumnList�� ���� �޸𸮿� �Բ� ���� �Ҵ�ȴ�.
         *   �� �Լ��� RebuildIndexColumn�� �Բ� �ǽð� AlterDDL�� �ٽ� ȣ��
         * �ɶ�, �ߺ� �޸� �Ҵ��ϴ� ���� ���ϱ� ���ؼ��̴�. */

        sFetchColumnList        = &(aIndexHeader->mFetchColumnListToMakeKey[ sLoop ]);
        sSmiColumnInFetchColumn = (smiColumn*)sFetchColumnList->column;

        /* set fetch column */
        idlOS::memcpy( sSmiColumnInFetchColumn,
                       sTableColumn,
                       ID_SIZEOF(smiColumn) );

        /* Proj-1872 Disk Index ���屸�� ����ȭ
         * Index�� �޸� FetchColumnList�� VRow�� ������ �ʴ´�. ���� Offset��
         * �ǹ̰� �����Ƿ� 0���� �����Ѵ�. */
        sSmiColumnInFetchColumn->offset = 0;

        sFetchColumnList->columnSeq = sColumnSeq;
        /* set fetch column list */
        sFetchColumnList->copyDiskColumn = (void *)sIndexColumn->mCopyDiskColumnFunc;

        if ( sLoop == (aIndexColCount - 1) )
        {
            sFetchColumnList->next = NULL;
        }
        else
        {
            sFetchColumnList->next = sFetchColumnList + 1;
        }

        /* check index column order */
        if ( sIndexColumn->mVRowColumn.id == sPrevColumnID )
        {
            ideLog::log( IDE_ERR_0,
                         "VRowColumnID       : %"ID_UINT32_FMT"\n"
                         "Index column order : %"ID_UINT32_FMT"\n",
                         sIndexColumn->mVRowColumn.id,
                         sLoop );
            dumpHeadersAndIteratorToSMTrc( NULL,
                                           (sdnbHeader*)aIndexHeader,
                                           NULL );
            IDE_ASSERT( 0 );
        }
        else
        {
                /* nothing to do */
        }

        if ( sIndexColumn->mVRowColumn.id < sPrevColumnID )
        {
            sIsOrderAsc = ID_FALSE;
        }
        else
        {
            /* nothing to do */
        }
        sPrevColumnID = sIndexColumn->mVRowColumn.id;
    }

    /* fetch column list��
     * column id ������������ ���ĵǾ� �־�� �Ѵ�.
     * �׷��� index key column���� ������
     * column id ���������� �ƴϸ�,
     * qsort�� �Ѵ�.
     *
     * ex) create table t1(i1 int, i2 int, i3 int)
     *     tablespace sys_tbs_diks_data;
     *
     *     create index t1_idx1 on t1(i3, i1, i2)
     */
    if ( sIsOrderAsc == ID_FALSE )
    {
        /* sort fetch column list */
        idlOS::qsort( aIndexHeader->mFetchColumnListToMakeKey,
                      aIndexColCount,
                      ID_SIZEOF(smiFetchColumnList),
                      gCompareSmiColumnListByColId );

        /* qsort�Ŀ� fetch column list���� ������踦 �������Ѵ�. */
        for ( sLoop = 0 ; sLoop < aIndexColCount ; sLoop++ )
        {
            sFetchColumnList = &(aIndexHeader->mFetchColumnListToMakeKey[ sLoop ]);
            if ( sLoop == (aIndexColCount - 1) )
            {
                sFetchColumnList->next = NULL;
            }
            else
            {
                sFetchColumnList->next = sFetchColumnList + 1;
            }
        }
    }

    return IDE_SUCCESS;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : sdnbBTree::rebuildIndexColumn                  *
 * ------------------------------------------------------------------*
 * create�Լ��� ���� �ε��� Į�� ������ �����ϱ� ���� ȣ��ȴ�.      *
 * ���� �߰������� �ǽð� Alter DDL�� ���� Į�������� ������ �ʿ䰡  *
 * �������� ȣ��ǰ� �ȴ�.                                           *
 *********************************************************************/
IDE_RC sdnbBTree::rebuildIndexColumn( smnIndexHeader * aIndex,
                                      smcTableHeader * aTable,
                                      void           * aHeader )
{
    SInt              i;
    UInt              sColID;
    sdnbColumn      * sIndexColumn = NULL;
    const smiColumn * sTableColumn;
    sdnbHeader      * sHeader;
    UInt              sNonStoringSize = 0;

    sHeader = (sdnbHeader*) aHeader;

    // for every key column�鿡 ����
    for ( i = 0 ; i < (SInt)aIndex->mColumnCount ; i++ )
    {
        sIndexColumn = &sHeader->mColumns[i];

        // �÷� ����(KeyColumn, mt callback functions,...) �ʱ�ȭ
        sColID = aIndex->mColumns[i] & SMI_COLUMN_ID_MASK;
        IDE_TEST_RAISE(sColID >= aTable->mColumnCount, ERR_COLUMN_NOT_FOUND);

        sTableColumn = smcTable::getColumn(aTable,sColID);

        IDE_TEST_RAISE( sTableColumn->id != aIndex->mColumns[i],
                        ERR_COLUMN_NOT_FOUND );

        // make mVRowColumn (for cursor level visibility )
        idlOS::memcpy( &sIndexColumn->mVRowColumn,
                       sTableColumn,
                       ID_SIZEOF(smiColumn) );

        // make mKeyColumn
        idlOS::memcpy( &sIndexColumn->mKeyColumn,
                       &sIndexColumn->mVRowColumn,
                       ID_SIZEOF(smiColumn) );

        sIndexColumn->mKeyColumn.flag  = aIndex->mColumnFlags[i];
        sIndexColumn->mKeyColumn.flag &= ~SMI_COLUMN_USAGE_MASK;
        sIndexColumn->mKeyColumn.flag |= SMI_COLUMN_USAGE_INDEX;

        /* PROJ-1872 Disk Index ���屸�� ����ȭ
         * ���� ����� ���� KeyAndVRow(CursorlevelVisibility), KeyAndKey(Traverse)
         * �� �뵵�� �ٸ� Compare�Լ��� ȣ���Ѵ�. */
        IDE_TEST(gSmiGlobalCallBackList.findCompare(
                    sTableColumn,
                    (aIndex->mColumnFlags[i] & SMI_COLUMN_ORDER_MASK)
                            | SMI_COLUMN_COMPARE_KEY_AND_KEY,
                    &sIndexColumn->mCompareKeyAndKey)
                != IDE_SUCCESS );
        IDE_TEST( gSmiGlobalCallBackList.findCompare(
                     sTableColumn,
                     (aIndex->mColumnFlags[i] & SMI_COLUMN_ORDER_MASK)
                            | SMI_COLUMN_COMPARE_KEY_AND_VROW,
                    &sIndexColumn->mCompareKeyAndVRow)
                 != IDE_SUCCESS );

        IDE_TEST( gSmiGlobalCallBackList.findCopyDiskColumnValue(
                                       sTableColumn,
                                       &sIndexColumn->mCopyDiskColumnFunc)
                  != IDE_SUCCESS );
        IDE_TEST( gSmiGlobalCallBackList.findKey2String(
                                               sTableColumn,
                                               aIndex->mColumnFlags[i],
                                               &sIndexColumn->mKey2String)
                  != IDE_SUCCESS );
        IDE_TEST( gSmiGlobalCallBackList.findIsNull(
                                                sTableColumn,
                                                aIndex->mColumnFlags[i],
                                                &sIndexColumn->mIsNull )
                  != IDE_SUCCESS );
        /* PROJ-1872 Disk Index ���屸�� ����ȭ
         * MakeKeyValueFromRow��, Row�� Length-KnownŸ���� Null�� 1Byte�� ����
         * �Ͽ� ǥ���ϱ� ������ NullValue�� ���� ���Ѵ�. ���� �� Null�� ����
         * ���� ���� mNull �Լ��� �����Ѵ�. */
        IDE_TEST( gSmiGlobalCallBackList.findNull(
                                                sTableColumn,
                                                aIndex->mColumnFlags[i],
                                                &sIndexColumn->mNull )
                  != IDE_SUCCESS );

        /* BUG-24449
         * Ű�� ��� ũ��� Ÿ�Կ� ���� �ٸ���. */
        IDE_TEST( gSmiGlobalCallBackList.getNonStoringSize( sTableColumn,
                                                            &sNonStoringSize )
                  != IDE_SUCCESS );
        sIndexColumn->mMtdHeaderLength  = sNonStoringSize;
    } // for

    /* BUG-22946
     * index key�� insert�Ϸ��� row�κ��� index key column value�� fetch�ؿ;�
     * �Ѵ�. row�κ��� column ������ fetch�Ϸ��� fetch column list�� �����־��
     * �ϴµ�, insert key insert �Ҷ����� fetch column list�� �����ϸ� ��������
     * �� �߻��Ѵ�.
     * �׷��� rebuildIndexColumn �ÿ� fetch column list�� �����ϰ� sdnbHeader��
     * �Ŵ޾Ƶε��� �����Ѵ�. */
    IDE_TEST( makeFetchColumnList4Index( aTable,
                                         sHeader,
                                         aIndex->mColumnCount)
            != IDE_SUCCESS );

    /* PROJ-1872 Disk index ���� ���� ����ȭ
     *  Key���̸� �˱� ���ؼ��� Ű�� �� Į���� � Ÿ������, Length�� ���
     * ��ϵǾ������� ���� ������ �ʿ��ϴ�. �׷��� ColLenInfo�� �̷��� ������
     * ����Ѵ�. */
    makeColLenInfoList( sHeader->mColumns,
                        sHeader->mColumnFence,
                        &sHeader->mColLenInfoList );

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_COLUMN_NOT_FOUND );

    IDE_SET( ideSetErrorCode( smERR_FATAL_smnColumnNotFound ) );

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : sdnbBTree::create                          *
 * ------------------------------------------------------------------*
 * �� �Լ��� �ý��� ��ŸƮ���� Ȥ�� �ε����� ���� create�� ��        *
 * run-time index header�� �����Ͽ� �ٴ� ������ �Ѵ�.                *
 * �޸� �ε����� �����ϰ� �ϱ� ���� smmManager�� ���� temp Page��  *
 * �Ҵ�޾� ����Ѵ�.                                                *
 * ���߿� memmgr���� �޵��� ���� ���.(��� �ε����� ���ؼ�)         *
 *********************************************************************/
IDE_RC sdnbBTree::create( idvSQL              * aStatistics,
                          smcTableHeader      * aTable,
                          smnIndexHeader      * aIndex,
                          smiSegAttr          * aSegAttr,
                          smiSegStorageAttr   * aSegStorageAttr,
                          smnInsertFunc       * aInsert,
                          smnIndexHeader     ** /*aRebuildIndexHeader*/,
                          ULong                 aSmoNo )
{
    SInt                 i= 0;
    UChar              * sPage;
    UChar              * sMetaPagePtr;
    sdnbMeta           * sMeta;
    sdnbNodeHdr        * sNodeHdr;
    sdnbHeader         * sHeader = NULL;
    idBool               sTrySuccess;
    UChar              * sSlotDirPtr;
    sdnbLKey           * sKey;
    sdpSegState          sIndexSegState;
    smLSN                sRecRedoLSN;              // disk redo LSN
    sdpSegMgmtOp       * sSegMgmtOp;
    scPageID             sSegPID;
    scPageID             sMetaPID;
    UInt                 sState = 0;
    UShort               sSlotCount;
    SChar                sBuffer[IDU_MUTEX_NAME_LEN];
    UChar              * sFixPagePtr = NULL;

    /* PROJ-2162 RestartRiskReduction
     * Refine ���и� �����, Refine�ϴ� Index�� ����� */
    ideLog::log( IDE_SM_0,
                 "=========================================\n"
                 " [DRDB_IDX_REFINE] NAME : %s\n"
                 "                   ID   : %"ID_UINT32_FMT"\n"
                 "                   TOID : %"ID_UINT64_FMT"\n"
                 " =========================================",
                 aIndex->mName,
                 aIndex->mId,
                 aTable->mSelfOID );

    /* TC/FIT/Limit/sm/sdn/sdnb/sdnbBTree_create_malloc1.sql */
    IDU_FIT_POINT_RAISE( "sdnbBTree::create::malloc1",
                          insufficient_memory );

    // ��ũ b-tree�� Run Time Header ����
    IDE_TEST_RAISE( iduMemMgr::malloc( IDU_MEM_SM_SDN,
                                 ID_SIZEOF(sdnbHeader),
                                 (void **)&sHeader ) != IDE_SUCCESS,
                    insufficient_memory );
    aIndex->mHeader = (smnRuntimeHeader*) sHeader;
    sState = 1;

    /* TC/FIT/Limit/sm/sdn/sdnb/sdnbBTree_create_malloc2.sql */
    IDU_FIT_POINT_RAISE( "sdnbBTree::create::malloc2",
                          insufficient_memory );

    // Header�� Column ���� ���� �Ҵ�
    // fix bug-22840
    IDE_TEST_RAISE (iduMemMgr::malloc( IDU_MEM_SM_SDN,
                                 ID_SIZEOF(sdnbColumn)*(aIndex->mColumnCount),
                                 (void **) &(sHeader->mColumns)) != IDE_SUCCESS,
                    insufficient_memory );
    sState = 2;

    /* TC/Server/LimitEnv/sm/sdn/sdnb/sdnbBTree_create_malloc3.sql */
    IDU_FIT_POINT_RAISE( "sdnbBTree::create::malloc3",
                          insufficient_memory );

    /* BUG-22946
     * index key�� insert�Ϸ��� row�κ��� index key column value�� fetch�ؿ;�
     * �Ѵ�. row�κ��� column ������ fetch�Ϸ��� fetch column list�� �����־��
     * �ϴµ�, insert key insert �Ҷ����� fetch column list�� �����ϸ� ��������
     * �� �߻��Ѵ�.
     * �׷��� rebuildIndexColumn �ÿ� fetch column list�� �����ϰ� sdnbHeader��
     * �Ŵ޾Ƶε��� �����Ѵ�. */
    IDE_TEST_RAISE ( iduMemMgr::malloc(IDU_MEM_SM_SDN,
                                 ID_SIZEOF(smiFetchColumnList)*(aIndex->mColumnCount),
                                 (void **) &(sHeader->mFetchColumnListToMakeKey)) != IDE_SUCCESS,
                     insufficient_memory );
    sState = 3;

    /* Proj-1872 DiskIndex ���屸�� ����ȭ
     * FetchColumnList�� ���� Key�� �����Ҷ� �䱸�Ǵ� smiColumn�� ��Ƶ� ���� �Ҵ�*/
    for ( i = 0 ; i < (SInt)aIndex->mColumnCount ; i++ )
    {
        /* TC/FIT/Limit/sm/sdn/sdnb/sdnbBTree_create_malloc4.sql */
        IDU_FIT_POINT_RAISE( "sdnbBTree::create::malloc4",
                              insufficient_memory );

        IDE_TEST_RAISE ( iduMemMgr::malloc(IDU_MEM_SM_SDN,
                                 ID_SIZEOF(smiColumn),
                                 (void **) &(sHeader->mFetchColumnListToMakeKey[ i ].column )) != IDE_SUCCESS,
                    insufficient_memory );
    }
    sState = 4;


    idlOS::snprintf( sBuffer,
                     ID_SIZEOF(sBuffer),
                     "INDEX_HEADER_LATCH_%"ID_UINT32_FMT,
                     aIndex->mId );

    IDE_TEST( sHeader->mLatch.initialize( sBuffer) != IDE_SUCCESS );

    // FOR A4 : run-time header ��� ���� �ʱ�ȭ
    sHeader->mIsCreated = ID_FALSE;
    sHeader->mLogging   = ID_TRUE;

    if ( ( (aIndex->mFlag & SMI_INDEX_UNIQUE_MASK) ==
           SMI_INDEX_UNIQUE_ENABLE ) ||
         ( (aIndex->mFlag & SMI_INDEX_LOCAL_UNIQUE_MASK) ==
           SMI_INDEX_LOCAL_UNIQUE_ENABLE ) )
    {
        sHeader->mIsUnique = ID_TRUE;
    }
    else
    {
        sHeader->mIsUnique = ID_FALSE;
    }

    if ( (aIndex->mFlag & SMI_INDEX_TYPE_MASK) == SMI_INDEX_TYPE_PRIMARY_KEY)
    {
        sHeader->mIsNotNull = ID_TRUE;
    }
    else
    {
        sHeader->mIsNotNull = ID_FALSE;
    }

    sHeader->mSmoNoAtomicA = 0;
    sHeader->mSmoNoAtomicB = 0;

    sHeader->mTableTSID = ((smcTableHeader*)aTable)->mSpaceID;
    sHeader->mIndexTSID = SC_MAKE_SPACE(aIndex->mIndexSegDesc);
    sHeader->mSmoNo = aSmoNo; // �ý��� startup�ÿ� 1�� ����(0 �ƴ�)
                              // 0���� ���� startup�ÿ� node�� ��ϵ�
                              // SmoNo�� reset�ϴµ� ����

    sHeader->mTableOID = aIndex->mTableOID;
    sHeader->mIndexID  = aIndex->mId;

    /*
     * PROJ-1671 Bitmap-based Tablespace And Segment Space Management
     * Segment ���� ���� �������̽� ��� �ʱ�ȭ
     */

    /* insert high limit�� insert low limit�� ������� ������ �����Ѵ�. */
    sdpSegDescMgr::setDefaultSegAttr(
                            &(sHeader->mSegmentDesc.mSegHandle.mSegAttr),
                            SDP_SEG_TYPE_INDEX);

    sdpSegDescMgr::setSegAttr( &sHeader->mSegmentDesc,
                               aSegAttr );

    IDE_DASSERT( aSegAttr->mInitTrans <= aSegAttr->mMaxTrans );
    IDE_DASSERT( aSegAttr->mInitTrans <= SMI_MAXIMUM_INDEX_CTL_SIZE );
    IDE_DASSERT( aSegAttr->mMaxTrans <= SMI_MAXIMUM_INDEX_CTL_SIZE );

    /* Storage �Ӽ��� �����Ѵ�. */
    sdpSegDescMgr::setSegStoAttr( &sHeader->mSegmentDesc,
                                  aSegStorageAttr );

    // Segment ����������������  Cache �Ҵ� �� �ʱ�ȭ
    // assign Index Segment Descriptor RID
    IDE_TEST( sdpSegDescMgr::initSegDesc(
                                 &sHeader->mSegmentDesc,
                                 SC_MAKE_SPACE(aIndex->mIndexSegDesc),
                                 SC_MAKE_PID(aIndex->mIndexSegDesc),
                                 SDP_SEG_TYPE_INDEX,
                                 aTable->mSelfOID,
                                 aIndex->mId ) != IDE_SUCCESS );

    if ( sHeader->mSegmentDesc.mSegMgmtType !=
                sdpTableSpace::getSegMgmtType( SC_MAKE_SPACE(aIndex->mIndexSegDesc)) )
    {
        ideLog::log( IDE_ERR_0, "\
===================================================\n\
               IDX Name : %s                       \n\
               ID       : %"ID_UINT32_FMT"                       \n\
                                                   \n\
sHeader->mSegmentDesc.mSegMgmtType : %"ID_UINT32_FMT"            \n\
sdpTableSpace::getSegMgmtType(SC_MAKE_SPACE(aIndex->mIndexSegDesc)) : %"ID_UINT32_FMT"\n",
                     aIndex->mName,
                     aIndex->mId,
                     sHeader->mSegmentDesc.mSegMgmtType,
                     sdpTableSpace::getSegMgmtType(SC_MAKE_SPACE(aIndex->mIndexSegDesc)) );

        dumpHeadersAndIteratorToSMTrc( aIndex,
                                       sHeader,
                                       NULL );

        IDE_ASSERT( 0 );
    }

    sSegMgmtOp = sdpSegDescMgr::getSegMgmtOp( &sHeader->mSegmentDesc );
    // codesonar::Null Pointer Dereference
    IDE_ERROR( sSegMgmtOp != NULL );

    /* Statistics */
    IDE_TEST( smnManager::initIndexStatistics( aIndex,
                                               (smnRuntimeHeader*)sHeader,
                                               aIndex->mId )
              != IDE_SUCCESS );

    sHeader->mColumnFence = &sHeader->mColumns[aIndex->mColumnCount];

    /* Proj-1872 Disk index ���� ���� ����ȭ
     * ���� Alter DDL�� ���� �ε��� �ʱ�ȭ �Լ��� �и���.
     * Į�� ������ ����� ��� �� �Լ��� ȣ�����ָ� ��. */

    IDE_TEST( rebuildIndexColumn( aIndex,
                                  aTable,
                                  sHeader )
              != IDE_SUCCESS );

    IDE_TEST( sSegMgmtOp->mGetSegState( aStatistics,
                                        SC_MAKE_SPACE(aIndex->mIndexSegDesc),
                                        sHeader->mSegmentDesc.mSegHandle.mSegPID,
                                        &sIndexSegState )
              != IDE_SUCCESS );

    if ( sIndexSegState ==  SDP_SEG_FREE )
    {
        // for PBT
        // restart disk index rebuild�� ������ �־ ������ ����.
        ideLog::log(SM_TRC_LOG_LEVEL_DINDEX,
                    SM_TRC_DINDEX_INDEX_SEG_FREE,
                    aTable->mFixed.mDRDB.mSegDesc.mSegHandle.mSegPID,
                    aIndex->mIndexSegDesc);
        return IDE_SUCCESS;
    }

    sSegPID  = sdpSegDescMgr::getSegPID(&(sHeader->mSegmentDesc));

    IDE_TEST( sSegMgmtOp->mGetMetaPID( aStatistics,
                                       sHeader->mIndexTSID,
                                       sSegPID,
                                       0, /* Seg Meta PID Array Index */
                                       &sMetaPID )
              != IDE_SUCCESS );

    sHeader->mMetaRID = SD_MAKE_RID( sMetaPID, SMN_INDEX_META_OFFSET );

    // SegHdr �����͸� ����
    IDE_TEST( sdnbBTree::fixPageByRID( aStatistics,
                                       NULL, // sdnbPageStat
                                       sHeader->mIndexTSID,
                                       sHeader->mMetaRID,
                                       (UChar **)&sMeta,
                                       &sTrySuccess )
              != IDE_SUCCESS );

    sMetaPagePtr = sdpPhyPage::getPageStartPtr( sMeta );
    sFixPagePtr  = sMetaPagePtr;

    // NumDist, root node, min node, max node�� ����
    sHeader->mRootNode                  = sMeta->mRootNode;
    sHeader->mEmptyNodeHead             = sMeta->mEmptyNodeHead;
    sHeader->mEmptyNodeTail             = sMeta->mEmptyNodeTail;
    sHeader->mMinNode                   = sMeta->mMinNode;
    sHeader->mMaxNode                   = sMeta->mMaxNode;
    sHeader->mFreeNodeHead              = sMeta->mFreeNodeHead;
    sHeader->mFreeNodeCnt               = sMeta->mFreeNodeCnt;

    // PROJ-1469 : disk index�� consistent flag
    // mIsConsistent = ID_FALSE : index access �Ұ�
    sHeader->mIsConsistent = sMeta->mIsConsistent;

    // BUG-17957
    // disk index�� run-time header�� creation option(logging, force) ����
    sHeader->mIsCreatedWithLogging = sMeta->mIsCreatedWithLogging;
    sHeader->mIsCreatedWithForce = sMeta->mIsCreatedWithForce;

    sHeader->mCompletionLSN = sMeta->mNologgingCompletionLSN;

    sFixPagePtr = NULL;
    IDE_TEST( sdnbBTree::unfixPage( aStatistics,
                                    sMetaPagePtr ) != IDE_SUCCESS );

    // mIsConsistent = ID_TRUE �̰� NOLOGGING/NOFORCE�� �����Ǿ��� ���
    // index build�� index page���� disk�� force�Ǿ����� check
    if ( (sHeader->mIsConsistent         == ID_TRUE ) &&
         (sHeader->mIsCreatedWithLogging == ID_FALSE) &&
         (sHeader->mIsCreatedWithForce   == ID_FALSE) )
    {
        // index build�� index page���� disk�� force���� �ʾ�����
        // sHeader->mCompletionLSN�� sRecRedoLSN�� ���ؼ�
        // sHeader->mCompletionLSN�� sRecRedoLSN���� ũ��
        // sHeader->mIsConsistent = FALSE
        (void)smrRecoveryMgr::getDiskRedoLSNFromLogAnchor( &sRecRedoLSN );

        if ( smrCompareLSN::isGTE( &(sHeader->mCompletionLSN),
                                   &sRecRedoLSN ) == ID_TRUE )
        {
            sHeader->mIsConsistent = ID_FALSE;

            /*
             * To fix BUG-21925
             * 2���� Restart�Ŀ��� mIsConsistent�� TRUE�� �����ɼ� �ִ�.
             * �̸� ���� ���� Meta �������� mIsConsistent�� FALSE�� �����Ѵ�.
             */
            IDE_TEST( setConsistent( aIndex,
                                     ID_FALSE )
                      != IDE_SUCCESS );
        }
        else
        {
            /* nothing to do */
        }
    }

    if ( sHeader->mIsConsistent == ID_TRUE )
    {
        // fix BUG-16153 Disk Index�� null�� �ִ� ��� ���� �籸�� ������.
        if ( sHeader->mMinNode !=  SD_NULL_PID )
        {
            // min node�� ù��° slot�� key���� min value�� ����
            IDE_TEST( sdnbBTree::fixPage( aStatistics,
                                          NULL, // sdnbPageStat
                                          sHeader->mIndexTSID,
                                          sHeader->mMinNode,
                                          (UChar **)&sPage,
                                          &sTrySuccess )
                      != IDE_SUCCESS );
            sFixPagePtr = sPage;

            sNodeHdr = (sdnbNodeHdr*)sdpPhyPage::getLogicalHdrStartPtr((UChar *)sPage);

            /* BUG-30639 Disk Index���� Internal Node��
             *           Min/Max Node��� �ν��Ͽ� ����մϴ�.
             * InternalNode�̸� �ƿ� �ٽ� �����մϴ�. */
            if ( SDNB_IS_LEAF_NODE( sNodeHdr ) == ID_FALSE )
            {
                /* BUG-30605: Deleted/Dead�� Ű�� Min/Max�� ���Խ�ŵ�ϴ�
                 * min/max node�� �߸� �����Ǵ� ���װ� �Ƚ��Ǿ����ϴ�.
                 * ���� ���� �̰�찡 �߻��ϸ� DASSERT�� ó���մϴ�.*/
                IDE_DASSERT( needToUpdateStat() == ID_FALSE );
                
                ideLog::log( IDE_SM_0,"\
========================================\n\
 Rebuilding MinNode Index %s            \n\
========================================\n",
                             aIndex->mName);

                (void) rebuildMinStat( aStatistics,
                                       NULL, /* aTrans */
                                       aIndex,
                                       sHeader );

                // rebuild�� ���� minNode�� ���� �Ǿ�����,
                // �缳���� MinNode�� ���ġ ����
                if ( sHeader->mMinNode != SD_NULL_PID )
                {
                    sFixPagePtr = NULL;
                    IDE_TEST( sdnbBTree::unfixPage( aStatistics,
                                                    sPage ) != IDE_SUCCESS );
                    IDE_TEST( sdnbBTree::fixPage( aStatistics,
                                                  NULL, // sdnbPageStat
                                                  sHeader->mIndexTSID,
                                                  sHeader->mMinNode,
                                                  (UChar **)&sPage,
                                                  &sTrySuccess )
                              != IDE_SUCCESS );
                    sFixPagePtr = sPage;
                }
            }

            if ( sHeader->mMinNode != SD_NULL_PID )
            {
                //BUG-24829 ������� ����� deleted key, dead key�� skip�ؾ� �մϴ�.
                sSlotDirPtr = sdpPhyPage::getSlotDirStartPtr( sPage );
                sSlotCount  = sdpSlotDirectory::getCount( sSlotDirPtr );

                // To Fix BUG-15670
                // Variable Null�� ��� Garbage Value�� ����� �� �ִ�.
                for ( i = 0 ; i < sSlotCount ; i++ )
                {
                    IDE_ERROR( sdpSlotDirectory::getPagePtrFromSlotNum(
                                                                 sSlotDirPtr, 
                                                                 i,
                                                                 (UChar **)&sKey )
                               == IDE_SUCCESS );

                    if ( isIgnoredKey4MinMaxStat(sKey, &(sHeader->mColumns[0]) )
                         != ID_TRUE )
                    { 
                        IDE_ERROR( setMinMaxValue( 
                                                sHeader,
                                                sPage,
                                                i,
                                                (UChar *)aIndex->mStat.mMinValue )
                                   == IDE_SUCCESS );
                        break;
                    }
                }
            }

            sFixPagePtr = NULL;
            IDE_TEST( sdnbBTree::unfixPage( aStatistics,
                                            sPage ) != IDE_SUCCESS );

            // max node���� ������ Key���� null ���� �ƴ� ���� ã�� max value�� ����
            IDE_TEST( sdnbBTree::fixPage(aStatistics,
                                         NULL, // sdnbPageStat
                                         sHeader->mIndexTSID,
                                         sHeader->mMaxNode,
                                         (UChar **)&sPage,
                                         &sTrySuccess ) != IDE_SUCCESS );
            sFixPagePtr = sPage;
            sNodeHdr = (sdnbNodeHdr*)sdpPhyPage::getLogicalHdrStartPtr((UChar *)sPage);

            /* BUG-30639 Disk Index���� Internal Node��
             *           Min/Max Node��� �ν��Ͽ� ����մϴ�.
             * InternalNode�̸� �ƿ� �ٽ� �����մϴ�. */
            if ( SDNB_IS_LEAF_NODE( sNodeHdr ) == ID_FALSE )
            {
                /* BUG-30605: Deleted/Dead�� Ű�� Min/Max�� ���Խ�ŵ�ϴ�
                 * min/max node�� �߸� �����Ǵ� ���װ� �Ƚ��Ǿ����ϴ�.
                 * ���� ���� �̰�찡 �߻��ϸ� DASSERT�� ó���մϴ�.*/
                IDE_DASSERT( needToUpdateStat() == ID_FALSE );

                ideLog::log( IDE_SM_0,"\
========================================\n\
 Rebuilding MaxNode Index %s            \n\
========================================\n",
                             aIndex->mName);

                (void) rebuildMaxStat( aStatistics,
                                       NULL, /* aTrans */
                                       aIndex,
                                       sHeader );

                // rebuild�� ���� MaxNode�� ���� �Ǿ�����,
                // �缳���� MaxNode�� ���ġ ����
                if ( sHeader->mMaxNode != SD_NULL_PID )
                {
                    sFixPagePtr = NULL;
                    IDE_TEST( sdnbBTree::unfixPage( aStatistics,
                                                    sPage )
                              != IDE_SUCCESS );
                    IDE_TEST( sdnbBTree::fixPage( aStatistics,
                                                  NULL, // sdnbPageStat
                                                  sHeader->mIndexTSID,
                                                  sHeader->mMaxNode,
                                                  (UChar **)&sPage,
                                                  &sTrySuccess )
                              != IDE_SUCCESS );
                    sFixPagePtr = sPage;
                }
                else 
                {
                    /* nothing to do */
                }

            }
            else
            {
                /* nothing to do */
            }
            if ( sHeader->mMaxNode != SD_NULL_PID )
            {
                sSlotDirPtr = sdpPhyPage::getSlotDirStartPtr( sPage );

                for ( i = sdpSlotDirectory::getCount( sSlotDirPtr ) - 1 ; i >= 0 ; i-- )
                {
                    // To Fix BUG-16122
                    // NULL �˻縦 ���ؼ��� NULL�� ���� CallBack ó���� �ʿ��Ͽ�
                    // Key Column�� ��ġ�� �ƴ� Key Pointer�� ��ġ�� �ݵ�� �ʿ���.
                    // ����, setMinMaxValue()�Լ��� ����� �� null�˻縦 �� �� ����.
                    IDE_ERROR( sdpSlotDirectory::getPagePtrFromSlotNum(
                                                                sSlotDirPtr, 
                                                                i,
                                                                (UChar **)&sKey )
                               == IDE_SUCCESS );

                    if ( isIgnoredKey4MinMaxStat(sKey, &(sHeader->mColumns[0]))
                        != ID_TRUE )
                    {
                        // To Fix BUG-15670, BUG-16122
                        IDE_ERROR( setMinMaxValue( sHeader,
                                                   sPage,
                                                   i,
                                                   (UChar *)aIndex->mStat.mMaxValue )
                                   == IDE_SUCCESS );
                        break;
                    }
                }
            }
            sFixPagePtr = NULL;
            IDE_TEST( sdnbBTree::unfixPage( aStatistics,
                                            sPage ) != IDE_SUCCESS );
        }
    }

    /*
     * fix BUG-17164
     * Runtime ������ Free�� Key�� ���� ū Commit SCN �� �����Ͽ�,
     * Key�� �̹� Free�Ǿ����� �Ǵ��ϴµ� �̿��Ѵ�.
     */
    idlOS::memset( &(sHeader->mDMLStat), 0x00, ID_SIZEOF(sdnbStatistic) );
    idlOS::memset( &(sHeader->mQueryStat), 0x00, ID_SIZEOF(sdnbStatistic) );

    // Insert, Delete �Լ� ����
    if ( sHeader->mIsUnique == ID_TRUE )
    {
        *aInsert = sdnbBTree::insertKeyUnique;
    }
    else
    {
        *aInsert = sdnbBTree::insertKey;
    }


    return IDE_SUCCESS;

    IDE_EXCEPTION( insufficient_memory );
    {
        IDE_SET(ideSetErrorCode(idERR_ABORT_InsufficientMemory));
    }
    IDE_EXCEPTION_END;

    if ( sFixPagePtr != NULL )
    {
        IDE_ASSERT( sdnbBTree::unfixPage( aStatistics,
                                          sFixPagePtr ) == IDE_SUCCESS );
        sFixPagePtr = NULL;
    }
    switch ( sState )
    // fix bug-22840
    {
        case 4:
            while( i-- )
            {
                (void) iduMemMgr::free( (void *)sHeader->mFetchColumnListToMakeKey[ i ].column ) ;
            }
        case 3:
            (void) iduMemMgr::free( sHeader->mFetchColumnListToMakeKey ) ;
        case 2:
            (void) iduMemMgr::free( sHeader->mColumns ) ;
        case 1:
            (void) iduMemMgr::free( sHeader ) ;
            break;
        default:
            break;
    }
    aIndex->mHeader = NULL;

    return IDE_FAILURE;
}


/*********************************************************************
 * FUNCTION DESCRIPTION : sdnbBTree::buildIndex                      *
 * ------------------------------------------------------------------*
 *********************************************************************/
IDE_RC sdnbBTree::buildIndex( idvSQL              * aStatistics,
                              void                * aTrans,
                              smcTableHeader      * aTable,
                              smnIndexHeader      * aIndex,
                              smnGetPageFunc        /*aGetPageFunc */,
                              smnGetRowFunc         /*aGetRowFunc  */,
                              SChar               * /*aNullRow     */,
                              idBool                aIsNeedValidation,
                              idBool                /* aIsEnableTable */,
                              UInt                  aParallelDegree,
                              UInt                  aBuildFlag,
                              UInt                  aTotalRecCnt )

{
    sdnbHeader * sHeader;
    smxTrans   * sSmxTrans = (smxTrans *)aTrans;
    UInt         sStatFlag = SMI_INDEX_BUILD_RT_STAT_UPDATE;


    if ( aTotalRecCnt > 0 )
    {
        if ( (aBuildFlag & SMI_INDEX_BUILD_FASHION_MASK) ==
             SMI_INDEX_BUILD_BOTTOMUP )
        {
            IDE_TEST( buildDRBottomUp(aStatistics,
                                      aTrans,
                                      aTable,
                                      aIndex,
                                      aIsNeedValidation,
                                      aBuildFlag,
                                      aParallelDegree )
                      != IDE_SUCCESS );
        }
        else
        {
            IDE_TEST( buildDRTopDown(aStatistics,
                                     aTrans,
                                     aTable,
                                     aIndex,
                                     aIsNeedValidation,
                                     aBuildFlag,
                                     aParallelDegree )
                      != IDE_SUCCESS );
        }
    }
    else
    {
        /* BUG-44794 �ε��� ����� �ε��� ��� ������ �������� �ʴ� ���� ������Ƽ �߰� */
        SMI_INDEX_BUILD_NEED_RT_STAT( sStatFlag, sSmxTrans );

        sHeader = (sdnbHeader*)aIndex->mHeader;
        sHeader->mIsCreated    = ID_TRUE;

        IDE_TEST( sdnbBUBuild::updateStatistics( 
                                    aStatistics,
                                    NULL,        /* aMtx */
                                    aIndex,         
                                    SD_NULL_PID, /* aMinNode */
                                    SD_NULL_PID, /* aMaxNode */
                                    0,           /* aNumPage */
                                    0,           /* aClusteringFactor */
                                    0,           /* aIndexHeight */
                                    0,           /* aNumDist */
                                    0,           /* aKeyCount */
                                    sStatFlag )
                  != IDE_SUCCESS );

        IDE_TEST( sdnbBTree::buildMeta( aStatistics,
                                        aTrans,
                                        sHeader )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : sdnbBTree::buildDRTopDown                  *
 * ------------------------------------------------------------------*
 *********************************************************************/
IDE_RC sdnbBTree::buildDRTopDown(idvSQL          * aStatistics,
                                  void           * aTrans,
                                  smcTableHeader * aTable,
                                  smnIndexHeader * aIndex,
                                  idBool           aIsNeedValidation,
                                  UInt             aBuildFlag,
                                  UInt             aParallelDegree )
{
    SInt             i;
    SInt             sThreadCnt = 0;
    idBool           sCreateSuccess = ID_TRUE;
    ULong            sAllocPageCnt;
    sdnbTDBuild    * sThreads = NULL;
    sdnbHeader     * sHeader;
    SInt             sInitThreadCnt = 0;
    UInt             sState = 0;

    // disk temp table�� cluster index�̱⶧����
    // build index�� ���� �ʴ´�.
    // �� create cluster index�� , key�� insert�ϴ� ������.
    IDE_DASSERT( SMI_TABLE_TYPE_IS_DISK( aTable ) == ID_TRUE );
    IDE_DASSERT( aIndex->mType == SMI_BUILTIN_B_TREE_INDEXTYPE_ID );

    sHeader = (sdnbHeader*)((smnIndexHeader*)aIndex)->mHeader;

    // create index �ÿ��� meta page�� ���� �ʱ����� ID_FALSE�� �ؾ��Ѵ�.
    // No-logging �ÿ��� index runtime header�� �����Ѵ�.
    sHeader->mIsCreated    = ID_FALSE;

    IDE_TEST( buildMeta( aStatistics,
                         aTrans,
                         sHeader )
              != IDE_SUCCESS );

    if ( aParallelDegree == 0 )
    {
        sThreadCnt = smuProperty::getIndexBuildThreadCount();
    }
    else
    {
        sThreadCnt = aParallelDegree;
    }

    IDE_TEST( sdpPageList::getAllocPageCnt(
                                     aStatistics,
                                     aTable->mSpaceID,
                                     &( aTable->mFixed.mDRDB.mSegDesc ),
                                     &sAllocPageCnt )
              != IDE_SUCCESS );

    if ( sAllocPageCnt < (UInt)sThreadCnt )
    {
        sThreadCnt = 1;
    }

    /* sdnbBTree_buildDRTopDown_malloc_Threads.tc */
    IDU_FIT_POINT( "sdnbBTree::buildDRTopDown::malloc::Threads",
                    idERR_ABORT_InsufficientMemory );

    IDE_TEST(iduMemMgr::malloc(IDU_MEM_SM_SDN,
                               (ULong)ID_SIZEOF(sdnbTDBuild)*sThreadCnt,
                               (void **)&sThreads)
             != IDE_SUCCESS );
    sState = 1;

    for ( i = 0 ; i < sThreadCnt ; i++ )
    {
        new (sThreads + i) sdnbTDBuild;
        IDE_TEST(sThreads[i].initialize(
                              sThreadCnt,
                              i,
                              aTable,
                              aIndex,
                              smLayerCallback::getFstDskViewSCN( aTrans ),
                              aIsNeedValidation,
                              aBuildFlag,
                              aStatistics,
                              &sCreateSuccess ) != IDE_SUCCESS );
        sInitThreadCnt++;
    }

    for ( i = 0 ; i < sThreadCnt ; i++ )
    {
        IDE_TEST(sThreads[i].start( ) != IDE_SUCCESS );
        IDE_TEST(sThreads[i].waitToStart(0) != IDE_SUCCESS );
    }

    for ( i = 0 ; i < sThreadCnt ; i++ )
    {
        IDE_TEST(sThreads[i].join() != IDE_SUCCESS );
    }

    if ( sCreateSuccess != ID_TRUE )
    {
        IDE_TEST(iduCheckSessionEvent(aStatistics) != IDE_SUCCESS );
        for ( i = 0 ; i < sThreadCnt ; i++ )
        {
            if ( sThreads[i].getErrorCode() != 0 )
            {
                ideCopyErrorInfo( ideGetErrorMgr(),
                                  sThreads[i].getErrorMgr() );
                break;
            }
        }
        IDE_TEST( 1 );
    }

    for ( i = ( sThreadCnt - 1 ) ; i >= 0 ; i-- )
    {
        sInitThreadCnt--;
        IDE_TEST( sThreads[i].destroy( ) != IDE_SUCCESS );
    }

    sState = 0;
    IDE_TEST( iduMemMgr::free(sThreads) != IDE_SUCCESS );


    sHeader->mIsCreated    = ID_TRUE;
    sHeader->mIsConsistent = ID_TRUE;
    smrLogMgr::getLstLSN( &(sHeader->mCompletionLSN) );

    IDE_TEST( buildMeta( aStatistics,
                         aTrans,
                         sHeader )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    IDE_PUSH();

    // BUG-27612 CodeSonar::Use After Free (4)
    if ( sState == 1 )
    {
        for ( i = 0 ; i < sInitThreadCnt; i++ )
        {
            (void)sThreads[i].destroy();
        }
        (void)iduMemMgr::free(sThreads);
    }
    IDE_POP();

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : sdnbBTree::buildDRBottomUp                 *
 * ------------------------------------------------------------------*
 *********************************************************************/
IDE_RC sdnbBTree::buildDRBottomUp(idvSQL         * aStatistics,
                                  void           * aTrans,
                                  smcTableHeader * aTable,
                                  smnIndexHeader * aIndex,
                                  idBool           aIsNeedValidation,
                                  UInt             aBuildFlag,
                                  UInt             aParallelDegree )
{

    SInt         sThreadCnt;
    ULong        sTotalExtentCnt;
    ULong        sTotalSortAreaSize;
    UInt         sTotalMergePageCnt;
    sdpSegInfo   sSegInfo;
    sdnbHeader * sHeader;
    smxTrans   * sSmxTrans = (smxTrans *)aTrans;
    UInt         sStatFlag = SMI_INDEX_BUILD_RT_STAT_UPDATE;

    // disk temp table�� cluster index�̱⶧����
    // build index�� ���� �ʴ´�.
    // �� create cluster index�� , key�� insert�ϴ� ������.
    IDE_DASSERT( SMI_TABLE_TYPE_IS_DISK( aTable ) == ID_TRUE );
    IDE_DASSERT( aIndex->mType == SMI_BUILTIN_B_TREE_INDEXTYPE_ID );

    /* BUG-44794 �ε��� ����� �ε��� ��� ������ �������� �ʴ� ���� ������Ƽ �߰� */
    SMI_INDEX_BUILD_NEED_RT_STAT( sStatFlag, sSmxTrans );

    sHeader = (sdnbHeader*)((smnIndexHeader*)aIndex)->mHeader;

    // create index �ÿ��� meta page�� ���� �ʱ����� ID_FALSE�� �ؾ��Ѵ�.
    // No-logging �ÿ��� index runtime header�� �����Ѵ�.
    sHeader->mIsCreated = ID_FALSE;

    if ( aParallelDegree == 0 )
    {
        sThreadCnt = smuProperty::getIndexBuildThreadCount();
    }
    else
    {
        sThreadCnt = aParallelDegree;
    }

    IDE_TEST( sdpSegment::getSegInfo(
                  aStatistics,
                  aTable->mSpaceID,
                  sdpSegDescMgr::getSegPID( &(aTable->mFixed.mDRDB) ),
                  &sSegInfo )
              != IDE_SUCCESS );

    sTotalExtentCnt = sSegInfo.mExtCnt;

    if ( sTotalExtentCnt < (UInt)sThreadCnt )
    {
        sThreadCnt = 1;
    }

    sTotalSortAreaSize = smuProperty::getSortAreaSize();

    // ������� SORT_AREA_SIZE�� 4���������� Ŀ�� �Ѵ�.
    while ( ( sTotalSortAreaSize / sThreadCnt ) < ( SD_PAGE_SIZE * 4 ) )
    {
        sThreadCnt = sThreadCnt / 2;
        if ( sThreadCnt == 0 )
        {
            sThreadCnt = 1;
            break;
        }
    }

    sTotalMergePageCnt = smuProperty::getMergePageCount();

    // ������� MERGE_PAGE_COUNT�� 2���������� Ŀ�� �Ѵ�.
    while ( ( sTotalMergePageCnt / sThreadCnt ) < 2 )
    {
        sThreadCnt = sThreadCnt / 2;
        if ( sThreadCnt == 0 )
        {
            sThreadCnt = 1;
            break;
        }
    }


    // Parallel Index Build
    IDE_TEST( sdnbBUBuild::main( aStatistics,
                                 aTable,
                                 aIndex,
                                 sThreadCnt,
                                 sTotalSortAreaSize,
                                 sTotalMergePageCnt,
                                 aIsNeedValidation,
                                 aBuildFlag,
                                 sStatFlag )
              != IDE_SUCCESS );


    sHeader->mIsCreated = ID_TRUE;

    IDE_TEST( buildMeta( aStatistics,
                         aTrans,
                         sHeader )
              != IDE_SUCCESS );


    // nologging & force�� ��� modify�� ���������� ������ flush�Ѵ�.
    if ( (aBuildFlag & SMI_INDEX_BUILD_FORCE_MASK) == SMI_INDEX_BUILD_FORCE )
    {
        IDE_DASSERT( (aBuildFlag & SMI_INDEX_BUILD_LOGGING_MASK) ==
                     SMI_INDEX_BUILD_NOLOGGING );

        ideLog::log( IDE_SM_0, "\
========================================\n\
 [IDX_CRE] 3.2 Buffer Flush             \n\
========================================\n" );
        IDE_TEST( sdsBufferMgr::flushPagesInRange(
                      NULL,
                      SC_MAKE_SPACE(aIndex->mIndexSegDesc), /* aSpaceID */
                      0,                                    /* aStartPID */
                      ID_UINT_MAX ) != IDE_SUCCESS );

        IDE_TEST( sdbBufferMgr::flushPagesInRange(
                      NULL,
                      SC_MAKE_SPACE(aIndex->mIndexSegDesc),
                      0,
                      ID_UINT_MAX ) != IDE_SUCCESS );

        ideLog::log( IDE_SM_0, "\
========================================\n\
 [IDX_CRE] Flushed Page Count           \n\
========================================\n");
    }


    ideLog::log( IDE_SM_0, "\
========================================\n\
 [IDX_CRE] BUILD INDEX COMPLETED        \n\
========================================\n" );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

/*********************************************************************
 * FUNCTION DESCRIPTION : sdnbBTree::drop                            *
 * ------------------------------------------------------------------*
 * �� �Լ��� index�� drop�ϰų� system�� shutdown�� �� ȣ��ȴ�.     *
 * drop�ÿ��� run-time header�� �����Ѵ�.                            *
 * �� �Լ��� commit �α׸� ���� ��, Ȥ�� shutdown�ÿ��� ������,    *
 * index segment�� TSS�� �̹� RID�� �޸� �����̴�.                   *
 *********************************************************************/
IDE_RC sdnbBTree::drop( smnIndexHeader * aIndex )
{
    UInt i;
    sdnbHeader   * sHeader;
    sdpSegMgmtOp * sSegMgmtOp;

    sHeader = (sdnbHeader*)(aIndex->mHeader);

    /* FOR A4 : Index body(segment)�� drop �� TSS�� �޸� ��,
       GC�� pending operation���� free�ȴ�.
    */
    if ( sHeader != NULL )
    {
        sHeader->mSegmentDesc.mSegHandle.mSegPID = SD_NULL_PID;

        sSegMgmtOp = sdpSegDescMgr::getSegMgmtOp( &(sHeader->mSegmentDesc) );
        IDE_ERROR( sSegMgmtOp != NULL );
        // Segment �������� Cache ����
        IDE_TEST( sSegMgmtOp->mDestroy( &(sHeader->mSegmentDesc.mSegHandle) )
                  != IDE_SUCCESS );

        sHeader->mSegmentDesc.mSegHandle.mCache = NULL;

        IDE_TEST( sHeader->mLatch.destroy() != IDE_SUCCESS );

        IDE_TEST( smnManager::destIndexStatistics( aIndex,
                                                   (smnRuntimeHeader*)sHeader )
                  != IDE_SUCCESS );

        /* BUG-22943 index bottom up build ���ɰ��� */
        /* Proj-1872 DiskIndex ���屸�� ����ȭ
         * fetch column�� �Ŵ޸� smi column �޸� ����*/
        for ( i = 0 ; i < aIndex->mColumnCount ; i++ )
        {
            IDE_TEST( iduMemMgr::free(
                            (void *) sHeader->mFetchColumnListToMakeKey[ i ].column )
                      != IDE_SUCCESS );
        }
        IDE_TEST( iduMemMgr::free( sHeader->mFetchColumnListToMakeKey )
                  != IDE_SUCCESS );

        // fix bug-22840
        IDE_TEST( iduMemMgr::free( sHeader->mColumns ) != IDE_SUCCESS );
        IDE_TEST( iduMemMgr::free( sHeader ) != IDE_SUCCESS );
        aIndex->mHeader = NULL;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : sdnbBTree::init                            *
 * ------------------------------------------------------------------*
 * �� �Լ��� �ε����� traverse�ϱ� ���� iterator�� �ʱ�ȭ�Ѵ�.       *
 * �޸� ���̺� ���� iterator�ʹ� �ٸ��� timestamp�� ���� �ʴ´�. *
 *********************************************************************/
IDE_RC sdnbBTree::init( sdnbIterator        * aIterator,
                        void                * aTrans,
                        smcTableHeader      * aTable,
                        smnIndexHeader      * aIndex,
                        void                * /*aDumpObject*/,
                        const smiRange      * aKeyRange,
                        const smiRange      * aKeyFilter,
                        const smiCallBack   * aRowFilter,
                        UInt                  aFlag,
                        smSCN                 aSCN,
                        smSCN                 aInfinite,
                        idBool                aUntouchable,
                        smiCursorProperties * aProperties,
                        const smSeekFunc   ** aSeekFunc,
                        smiStatement        * aStatement )
{

    idvSQL                    *sSQLStat;

    aIterator->mSCN            = aSCN;
    aIterator->mInfinite       = aInfinite;
    aIterator->mTrans          = aTrans;
    aIterator->mTable          = aTable;
    aIterator->mCurRecPtr      = NULL;
    aIterator->mLstFetchRecPtr = NULL;
    SC_MAKE_NULL_GRID( aIterator->mRowGRID );
    aIterator->mTID            = smLayerCallback::getTransID( aTrans );
    aIterator->mFlag           = aUntouchable == ID_TRUE ? SMI_ITERATOR_READ : SMI_ITERATOR_WRITE;
    aIterator->mProperties     = aProperties;

    aIterator->mIndex         = aIndex;
    aIterator->mKeyRange      = aKeyRange;
    aIterator->mKeyFilter     = aKeyFilter;
    aIterator->mRowFilter     = aRowFilter;
    aIterator->mCurLeafNode   =
    aIterator->mNextLeafNode  = SD_NULL_PID;
    aIterator->mCurNodeSmoNo  = 0;
    aIterator->mIndexSmoNo    = 0;
    aIterator->mScanBackNode  = SD_NULL_PID;
    aIterator->mIsLastNodeInRange = ID_TRUE;
    aIterator->mPrevKey = NULL;
    aIterator->mCurRowPtr  = &aIterator->mRowCache[0];
    aIterator->mCacheFence = &aIterator->mRowCache[0];
    // BUG-18557
    aIterator->mPage       = (UChar *)aIterator->mAlignedPage;
    aIterator->mStatement  = aStatement;

    IDE_ERROR( checkFetchColumnList( 
                   (sdnbHeader*)aIndex->mHeader,
                   aIterator->mProperties->mFetchColumnList,
                   &aIterator->mFullIndexScan )
               == IDE_SUCCESS );

    idlOS::memset( aIterator->mPage, 0x00, SD_PAGE_SIZE );

    // FOR A4 : DRDB Cursor������ TimeStamp�� Open���� ����
    *aSeekFunc = sdnbSeekFunctions[aFlag&(SMI_TRAVERSE_MASK|
                                          SMI_PREVIOUS_MASK|
                                          SMI_LOCK_MASK)];

    sSQLStat = aIterator->mProperties->mStatistics;
    IDV_SQL_ADD(sSQLStat, mDiskCursorIndexScan, 1);

    if (sSQLStat != NULL)
    {
        IDV_SESS_ADD(sSQLStat->mSess,
                     IDV_STAT_INDEX_DISK_CURSOR_IDX_SCAN, 1);
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    // BUG-35335
    /* BUG-31845 [sm-disk-index] Debugging information is needed 
     * for PBT when fail to check visibility using DRDB Index. */
    dumpHeadersAndIteratorToSMTrc( aIndex,
                                   NULL,
                                   aIterator );

    return IDE_FAILURE;
}

IDE_RC sdnbBTree::dest( sdnbIterator* /*aIterator*/ )
{
    return IDE_SUCCESS;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : sdnbBTree::traverse                        *
 * ------------------------------------------------------------------*
 * Index���� Row�� RID�� �̿��Ͽ� �ش��ϴ� key slot�� ã�ų� ����*
 * �� ��ġ�� ã�Ƴ��� �Լ��̴�. (leaf node�� x latch)                *
 *                                                                   *
 * stack�� �Ѱ� �ްԵǴµ�, �� stack�� ���¿� ���� Root���� Ȥ��     *
 * ���� depth���� �ٽ� traverse�ϱ⵵ �Ѵ�. root���� �Ҷ��� depth��  *
 * -1�̾�� �ϸ�, aStartNode�� NULL�� �������Եǰ�, �߰� ���� ������ *
 * ������ depth���� 0���� ũ�ų� ���� ���� ������ �Ǹ�, aStartNode�� *
 * ���� traverse�� ������ ��带 fix�� ������ ���̾�� �Ѵ�.         *
 *                                                                   *
 * [ROW_PID, ROW_SLOTNUM, KEY_VALUE]�� �̿��ؼ� ROOT�� ���� LEAF���� *
 * ���� Ž���ϸ�, LEAF_NODE�� LEAF_SLOTNUM�� �����Ѵ�.               *
 *                                                                   *
 * aIsPessimistic�� ID_TRUE�� ���, tree lach�� ��� �� �Լ��� ���  *
 * ���� �ȴ�. ���� traverse�߿� SMO�� �߰��� �� ����.                *
 *                                                                   *
 * aIsPessimistic�� ID_FALSE�� ���, Traverse �߿� SMO�� ������      *
 * SMO�� �����⸦ ��ٷȴٰ� ���������� ��ƺ���, SMO�� �߻�����   *
 * �ʾ����� �ű⼭ ���� �ٽ� traverse�ϰ�, root ���� �߻�������      *
 * ó������ retry�� �Ѵ�. �̷��� �߰� ���� �ٽ� �����ϸ� �� ����     *
 * ��忡 SMO�� �߻����� ���� �ִµ�, optimistic������ stack�� ����  *
 * propagation�� �߻����� �ʱ� ������ ����� ����.                   *
 *********************************************************************/
IDE_RC sdnbBTree::traverse( idvSQL         * aStatistics,
                            sdnbHeader     * aIndex,
                            sdrMtx         * aMtx,
                            sdnbKeyInfo    * aKeyInfo,
                            sdnbStack      * aStack,
                            sdnbStatistic  * aIndexStat,
                            idBool           aIsPessimistic,
                            sdpPhyPageHdr ** aLeafNode,      /* [OUT] ã���� Leaf Node ( X-Latch ) */
                            SShort         * aLeafKeySeq ,   /* [OUT] ã���� Leaf Node �� Sequence */
                            sdrSavePoint   * aLeafSP,
                            idBool         * aIsSameKey,     /* [OUT] Node���� aKeyInfo�� ������ KEY�� �����ϴ��� ����
                                                                      (KEY���� DATA RID ��� �����Ѱ��) */
                            SLong          * aTotalTraverseLength )
{

    ULong                sIndexSmoNo;
    scPageID             sPrevPID;
    scPageID             sPID;
    scPageID             sNextPID;
    sdnbNodeHdr *        sNodeHdr;
    sdpPhyPageHdr *      sPage;
    sdpPhyPageHdr *      sTmpPage;
    idBool               sIsSuccess;
    idBool               sRetry;
    idBool               sIsSameKey;
    idBool               sIsSameValueInSiblingNodes;
    void *               sTrans = sdrMiniTrans::getTrans(aMtx);
    UInt                 sHeight;
    scPageID             sChildPID;
    scPageID             sNextChildPID;
    SShort               sSlotSeq;
    idBool               sTrySuccess;
    ULong                sNodeSmoNo;
    UShort               sNextSlotSize;
    idBool               sMtxStart = ID_TRUE;
    idvWeArgs            sWeArgs;
    sdnbConvertedKeyInfo sConvertedKeyInfo;
    smiValue             sSmiValueList[SMI_MAX_IDX_COLUMNS];
    idBool               sIsLatched = ID_FALSE;
    SLong                sMaxTraverseLength;
    idBool               sIsRollback;
    SChar *              sOutBuffer4Dump;

    /* BUG-45377 �ε��� ���� ���� ���� ��� ���� */
    if ( aTotalTraverseLength == NULL )
    {
        /* �� ��� insertKeyRollback�̳� deleteKeyRollback���� ȣ��� ����̹Ƿ�
         * count over / session check�� ���� �Լ� ����ó���� ���� �ʵ��� �Ѵ�. */
        sIsRollback = ID_TRUE;
    }
    else
    {
        sIsRollback = ID_FALSE;

        sMaxTraverseLength = smuProperty::getMaxTraverseLength();
    }

    sConvertedKeyInfo.mSmiValueList = (smiValue*)sSmiValueList;
    SDNB_KEYINFO_TO_CONVERTED_KEYINFO( *aKeyInfo,
                                       sConvertedKeyInfo,
                                       &aIndex->mColLenInfoList );

retry:

    // index�� SmoNo�� ���Ѵ�.(sSmoNo)
    getSmoNo( (void *)aIndex, &sIndexSmoNo );

    IDL_MEM_BARRIER;

    if ( aStack->mIndexDepth ==  -1 ) // root�� ���� traverse
    {
        // index�� root page PID�� ���Ѵ�.
        sPID = aIndex->mRootNode;
        if ( sPID == SD_NULL_PID )
        {
            *aLeafNode   = NULL;
            *aLeafKeySeq = 0;
            return IDE_SUCCESS;
        }
    }
    else
    {
        sPID = aStack->mStack[aStack->mIndexDepth + 1].mNode;
    }

    IDE_TEST( sdnbBTree::getPage( aStatistics,
                                  &(aIndexStat->mIndexPage),
                                  aIndex->mIndexTSID,
                                  sPID,
                                  SDB_S_LATCH,
                                  SDB_WAIT_NORMAL,
                                  NULL,
                                  (UChar **)&sPage,
                                  &sIsSuccess) != IDE_SUCCESS );
    sIsLatched = ID_TRUE;

    while (1)
    {
        /* BUG-45377 �ε��� ���� ���� ���� ��� ���� */
        if ( sIsRollback == ID_FALSE )
        {
            /* loop count üũ */
            if ( sMaxTraverseLength >= 0 )
            {
                IDE_TEST_RAISE( (*aTotalTraverseLength) > sMaxTraverseLength, ERR_TOO_MANY_TRAVERSAL );

                (*aTotalTraverseLength)++;
            }

            /* session close ���� üũ */ 
            IDE_TEST( iduCheckSessionEvent( aStatistics ) != IDE_SUCCESS );
        }
        else
        {
            /* rollback ������ ������ �����ؾ� �ϹǷ� 
               count over / session check�� ���� ����ó���� ���� �ʴ´�. */
        }

        sNodeHdr = (sdnbNodeHdr*)sdpPhyPage::getLogicalHdrStartPtr((UChar *)sPage);
        sHeight = sNodeHdr->mHeight;

        if ( sHeight > 0 ) //Internal node
        {
            IDE_ERROR( findInternalKey( aIndex,
                                        sPage,
                                        aIndexStat,
                                        aIndex->mColumns,
                                        aIndex->mColumnFence,
                                        &sConvertedKeyInfo,
                                        sIndexSmoNo,
                                        &sNodeSmoNo,
                                        &sChildPID,
                                        &sSlotSeq,
                                        &sRetry,
                                        &sIsSameValueInSiblingNodes,
                                        &sNextChildPID,
                                        &sNextSlotSize )
                       == IDE_SUCCESS );

            sIsLatched = ID_FALSE;
            IDE_TEST( sdbBufferMgr::releasePage( aStatistics,
                                                 (UChar *)sPage )
                      != IDE_SUCCESS );

            if ( sRetry == ID_TRUE )
            {
                IDE_DASSERT( aIsPessimistic == ID_FALSE );
                sMtxStart = ID_FALSE;
                IDE_TEST( sdrMiniTrans::commit( aMtx ) != IDE_SUCCESS );


                IDV_WEARGS_SET(
                    &sWeArgs,
                    IDV_WAIT_INDEX_LATCH_DRDB_BTREE_INDEX_SMO_BY_OTHER_SESSION,
                    0,   // WaitParam1
                    0,   // WaitParam2
                    0 ); // WaitParam3

                // s latch tree latch
                IDE_TEST( aIndex->mLatch.lockRead( aStatistics,
                                                   &sWeArgs ) != IDE_SUCCESS );

                // blocked...

                // unlatch tree latch
                IDE_TEST( aIndex->mLatch.unlock() != IDE_SUCCESS );
                IDE_TEST( sdrMiniTrans::begin(aStatistics,
                                              aMtx,
                                              sTrans,
                                              SDR_MTX_LOGGING,
                                              ID_TRUE,/*Undoable(PROJ-2162)*/
                                              gMtxDLogType) != IDE_SUCCESS );
                sMtxStart = ID_TRUE;

                // optimistic traverse������ stack�� ������
                // �κ������� invalid�ص� �������..
                IDE_TEST( findValidStackDepth( aStatistics,
                                               aIndexStat,
                                               aIndex,
                                               aStack,
                                               &sIndexSmoNo )
                          != IDE_SUCCESS );

                if ( aStack->mIndexDepth < 0 ) // SMO�� Root���� �Ͼ.
                {
                    initStack( aStack );
                    goto retry;     // ó���� ���� �ٽ� ����
                }
                else
                {
                    // mIndexDepth - 1���� ����...�� �������� ����
                    sPID = aStack->mStack[aStack->mIndexDepth+1].mNode;

                    IDE_TEST( sdnbBTree::getPage( aStatistics,
                                                  &(aIndexStat->mIndexPage),
                                                  aIndex->mIndexTSID,
                                                  sPID,
                                                  SDB_S_LATCH,
                                                  SDB_WAIT_NORMAL,
                                                  NULL,
                                                  (UChar **)&sPage,
                                                  &sTrySuccess) != IDE_SUCCESS );
                    sIsLatched = ID_TRUE;
                    continue;
                }
            }
            else
            {
                // stack�� ���� ��� PID�� �����Ѵ�.
                aStack->mIndexDepth++;
                aStack->mStack[aStack->mIndexDepth].mNode = sPID;
                aStack->mStack[aStack->mIndexDepth].mKeyMapSeq = sSlotSeq;
                aStack->mStack[aStack->mIndexDepth].mSmoNo = sNodeSmoNo;
                aStack->mStack[aStack->mIndexDepth].mNextSlotSize = sNextSlotSize;

                if ( sIsSameValueInSiblingNodes == ID_TRUE )
                {
                    aStack->mIsSameValueInSiblingNodes = sIsSameValueInSiblingNodes;
                }

                // fix sChildPID to sPage
                IDE_TEST( sdnbBTree::getPage( aStatistics,
                                              &(aIndexStat->mIndexPage),
                                              aIndex->mIndexTSID,
                                              sChildPID,
                                              SDB_S_LATCH,
                                              SDB_WAIT_NORMAL,
                                              NULL,
                                              (UChar **)&sPage,
                                              &sTrySuccess) != IDE_SUCCESS );
                sIsLatched = ID_TRUE;
                sPID = sChildPID;
                // Next node PID�� depth 1 �Ʒ� stack�� ����Ѵ�.
                aStack->mStack[aStack->mIndexDepth+1].mNextNode = sNextChildPID;
            }
        }
        else // leaf node
        {
            if ( aIsPessimistic == ID_TRUE )
            {
                //-------------------------------
                // pessimistic
                //-------------------------------
                // BUG-15553
                if ( aLeafSP != NULL )
                {
                    sdrMiniTrans::setSavePoint( aMtx, aLeafSP );
                }

                // prev, target, next page�� x latch�� ��´�.
                sPrevPID = sdpPhyPage::getPrvPIDOfDblList(sPage);
                sNextPID = sdpPhyPage::getNxtPIDOfDblList(sPage);

                // sNode�� unfix�Ѵ�.
                sIsLatched = ID_FALSE;
                IDE_TEST( sdbBufferMgr::releasePage( aStatistics,
                                                     (UChar *)sPage )
                          != IDE_SUCCESS );


                if ( sPrevPID != SD_NULL_PID )
                {
                    IDE_TEST( sdnbBTree::getPage( aStatistics,
                                                  &(aIndexStat->mIndexPage),
                                                  aIndex->mIndexTSID,
                                                  sPrevPID,
                                                  SDB_X_LATCH,
                                                  SDB_WAIT_NORMAL,
                                                  aMtx,
                                                  (UChar **)&sTmpPage,
                                                  &sIsSuccess )
                              != IDE_SUCCESS );
                }
                IDE_ERROR( sPID != SD_NULL_PID );
                IDE_TEST( sdnbBTree::getPage( aStatistics,
                                              &(aIndexStat->mIndexPage),
                                              aIndex->mIndexTSID,
                                              sPID,
                                              SDB_X_LATCH,
                                              SDB_WAIT_NORMAL,
                                              aMtx,
                                              (UChar **)&sPage,
                                              &sIsSuccess) != IDE_SUCCESS );

                if ( sNextPID != SD_NULL_PID )
                {
                    IDE_TEST( sdnbBTree::getPage( aStatistics,
                                                  &(aIndexStat->mIndexPage),
                                                  aIndex->mIndexTSID,
                                                  sNextPID,
                                                  SDB_X_LATCH,
                                                  SDB_WAIT_NORMAL,
                                                  aMtx,
                                                  (UChar **)&sTmpPage,
                                                  &sIsSuccess)
                              != IDE_SUCCESS );
                }

            }
            else
            {
                //-------------------------------
                // optimistic
                //-------------------------------

                // sNode�� unfix�Ѵ�.
                sIsLatched = ID_FALSE;
                IDE_TEST( sdbBufferMgr::releasePage( aStatistics,
                                                     (UChar *)sPage )
                          != IDE_SUCCESS );

                // sNode PID�� x-latch with mtx
                IDE_ERROR( sPID != SD_NULL_PID);
                IDE_TEST( sdnbBTree::getPage( aStatistics,
                                              &(aIndexStat->mIndexPage),
                                              aIndex->mIndexTSID,
                                              sPID,
                                              SDB_X_LATCH,
                                              SDB_WAIT_NORMAL,
                                              aMtx,
                                              (UChar **)&sPage,
                                              &sIsSuccess) != IDE_SUCCESS );
            }

            IDE_ERROR( findLeafKey( sPage,
                                    aIndexStat,
                                    aIndex->mColumns,
                                    aIndex->mColumnFence,
                                    &sConvertedKeyInfo,
                                    sIndexSmoNo,
                                    &sNodeSmoNo,
                                    &sSlotSeq,
                                    &sRetry,
                                    &sIsSameKey )
                       == IDE_SUCCESS );

            if ( sRetry == ID_TRUE )
            {
                IDE_DASSERT( aIsPessimistic == ID_FALSE );
                sMtxStart = ID_FALSE;
                IDE_TEST( sdrMiniTrans::commit( aMtx ) != IDE_SUCCESS );


                IDV_WEARGS_SET(
                    &sWeArgs,
                    IDV_WAIT_INDEX_LATCH_DRDB_BTREE_INDEX_SMO_BY_OTHER_SESSION,
                    0,   // WaitParam1
                    0,   // WaitParam2
                    0 ); // WaitParam3

                // s latch tree latch
                IDE_TEST( aIndex->mLatch.lockRead( aStatistics,
                                                   &sWeArgs ) != IDE_SUCCESS );
                // blocked...

                // unlatch tree latch
                IDE_TEST( aIndex->mLatch.unlock() != IDE_SUCCESS );
                IDE_TEST( sdrMiniTrans::begin( aStatistics,
                                               aMtx,
                                               sTrans,
                                               SDR_MTX_LOGGING,
                                               ID_TRUE,/*Undoable(PROJ-2162)*/
                                               gMtxDLogType ) != IDE_SUCCESS );
                sMtxStart = ID_TRUE;

                IDE_TEST( findValidStackDepth( aStatistics,
                                               aIndexStat,
                                               aIndex,
                                               aStack,
                                               &sIndexSmoNo )
                          != IDE_SUCCESS );
                if ( aStack->mIndexDepth < 0 ) // SMO�� Root���� �Ͼ.
                {
                    initStack( aStack );
                    goto retry;     // ó���� ���� �ٽ� ����
                }
                else
                {
                    // mIndexDepth - 1���� ����...�� �������� ����
                    sPID = aStack->mStack[aStack->mIndexDepth+1].mNode;

                    IDE_TEST( sdnbBTree::getPage( aStatistics,
                                                  &(aIndexStat->mIndexPage),
                                                  aIndex->mIndexTSID,
                                                  sPID,
                                                  SDB_S_LATCH,
                                                  SDB_WAIT_NORMAL,
                                                  NULL,
                                                  (UChar **)&sPage,
                                                  &sTrySuccess) != IDE_SUCCESS );
                    sIsLatched = ID_TRUE;
                    continue;
                }
            }
            else
            {
                aStack->mIndexDepth++;
                aStack->mStack[aStack->mIndexDepth].mNode =
                                    sdpPhyPage::getPageID((UChar *)sPage);
                aStack->mStack[aStack->mIndexDepth].mKeyMapSeq = sSlotSeq;
                aStack->mStack[aStack->mIndexDepth].mSmoNo =
                         sdpPhyPage::getIndexSMONo((sdpPhyPageHdr*)sPage);

                if ( aIsSameKey != NULL )
                {
                    *aIsSameKey = sIsSameKey;
                }
                
                *aLeafNode = sPage;
                *aLeafKeySeq  = sSlotSeq;
            }
            break;
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_TOO_MANY_TRAVERSAL )
    {
        IDE_SET( ideSetErrorCode( smERR_ABORT_TOO_MANY_TRAVERSAL, aIndex->mTableOID, aIndex->mIndexID ) );

        ideLog::log( IDE_SM_0,
                     "An index is corrupt ( It takes too long to retrieve the index ) "
                     "Table OID   : %"ID_vULONG_FMT", "
                     "Index ID    : %"ID_UINT32_FMT"\n",
                     aIndex->mTableOID,
                     aIndex->mIndexID );

        if ( iduMemMgr::calloc( IDU_MEM_SM_SDN, 
                                1,    
                                ID_SIZEOF( SChar ) * IDE_DUMP_DEST_LIMIT,
                                (void **)&sOutBuffer4Dump )
             == IDE_SUCCESS )
        {
            (void) dumpKeyInfo( aKeyInfo, 
                                &(aIndex->mColLenInfoList),
                                sOutBuffer4Dump,
                                IDE_DUMP_DEST_LIMIT );
            (void) dumpStack( aStack, 
                              sOutBuffer4Dump,
                              IDE_DUMP_DEST_LIMIT );

            (void) iduMemMgr::free( sOutBuffer4Dump );
        }
        else
        {
            /* nothing to do... */
        }
    }
    IDE_EXCEPTION_END;

    if ( sMtxStart == ID_FALSE )
    {
        (void)sdrMiniTrans::begin( aStatistics,
                                   aMtx,
                                   sTrans,
                                   SDR_MTX_LOGGING,
                                   ID_TRUE,/*Undoable(PROJ-2162)*/
                                   gMtxDLogType );
    }

    if ( sIsLatched == ID_TRUE )
    {
        sIsLatched = ID_FALSE;
        (void)sdbBufferMgr::releasePage( aStatistics,
                                         (UChar *)sPage );
    }

    return IDE_FAILURE;

}


/*********************************************************************
 * FUNCTION DESCRIPTION : sdnbBTree::findValidStackDepth             *
 * ------------------------------------------------------------------*
 * Index Traverse �߿� SMO�� ������ tree latch�� ��� �� �Ŀ� ȣ��� *
 * stack�� ���ٷ� �ö󰡸鼭 SMO�� �Ͼ ������ �˾Ƴ���, stack��   *
 * depth�� SMO�� �ֻ�� ���� �ǵ�����.                             *
 * stack�� ��� ��忡�� SMO�� �߻����� ��� depth�� 0�� ���õ� ��   *
 * ��ȯ�ȴ�.                                                         *
 *                                                                   *
 * Optimistic Traverse�ÿ��� ���� �� �ִ�.                         *
 *                                                                   *
 * !!CAUTION!! : �� �Լ��� ��� �ص� �� stack ������ ��� ��尡     *
 * valid�ϴٰ� ���� ���� ����. �� ������ �ٸ� ������ ���۵� SMO��    *
 * stack�� ���� �κп� ������ �־��� �� �ֱ� �����̴�.               *
 *********************************************************************/
IDE_RC sdnbBTree::findValidStackDepth( idvSQL          *aStatistics,
                                       sdnbStatistic *  aIndexStat,
                                       sdnbHeader *     aIndex,
                                       sdnbStack *      aStack,
                                       ULong *          aSmoNo )
{

    ULong           sNodeSmoNo;
    ULong           sNewIndexSmoNo;
    scPageID        sPID;
    sdpPhyPageHdr * sNode;
    idBool          sTrySuccess;

    /* BUG-27412 Disk Index���� Stack�� �Ųٷ� Ž���Ҷ� Index SMO No��
     * ���� ������ ������ �ֽ��ϴ�.
     *
     * Index SMO No�� Node�� ��ȸ�ϱ� ���� Ȯ���ص־� �մϴ�. �׷��� ��
     * Node�� ��ȸ�ϴ� ������ SMO No�� ���� ������ �޶�, �� ���̿� �Ͼ�
     * �� SMO�� ã�Ƴ��� ���մϴ�. */
    getSmoNo((void *)aIndex, &sNewIndexSmoNo );
    IDL_MEM_BARRIER;

    while (1) // ������ ���� �ö� ����
    {
        aStack->mIndexDepth--;
        if ( aStack->mIndexDepth <= -1 ) // root���� SMO �߻�
        {
            aStack->mIndexDepth = -1;
            break;
        }
        else
        {
            sPID = aStack->mStack[aStack->mIndexDepth].mNode;

            // fix last node in stack to *sNode
            IDE_TEST( sdnbBTree::getPage( aStatistics,
                                          &(aIndexStat->mIndexPage),
                                          aIndex->mIndexTSID,
                                          sPID,
                                          SDB_S_LATCH,
                                          SDB_WAIT_NORMAL,
                                          NULL,
                                          (UChar **)&sNode,
                                          &sTrySuccess) != IDE_SUCCESS );

            sNodeSmoNo = sdpPhyPage::getIndexSMONo( sNode );

            IDE_TEST( sdbBufferMgr::releasePage( aStatistics,
                                                 (UChar *)sNode )
                      != IDE_SUCCESS );

            if ( sNodeSmoNo < *aSmoNo ) // SMO ������ ���
            {
                // index�� ���� SmoNo�� �ٽ� ����.
                *aSmoNo = sNewIndexSmoNo;
                IDL_MEM_BARRIER;

                break; // �� ��� �������� �ٽ� traverse
            }
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

#ifdef DEBUG 
/*********************************************************************
 * FUNCTION DESCRIPTION : sdnbBTree::compareInternalKey              *
 * ------------------------------------------------------------------*
 * Internal node���� �־��� Slot�� �� ���� Key�� ���Ѵ�.           *
 *********************************************************************/
IDE_RC sdnbBTree::compareInternalKey( sdpPhyPageHdr    * aNode,
                                      sdnbStatistic    * aIndexStat,
                                      SShort             aSrcSeq,
                                      SShort             aDestSeq,
                                      sdnbColumn       * aColumns,
                                      sdnbColumn       * aColumnFence,
                                      idBool           * aIsSameValue,
                                      SInt             * aResult )
{
    SInt            sRet;
    sdnbIKey      * sIKey;
    sdnbKeyInfo     sKeyInfo1;
    sdnbKeyInfo     sKeyInfo2;
    UChar         * sSlotDirPtr;

    sSlotDirPtr = sdpPhyPage::getSlotDirStartPtr( (UChar *)aNode );

    IDE_TEST( sdpSlotDirectory::getPagePtrFromSlotNum( sSlotDirPtr, 
                                                       aSrcSeq,
                                                       (UChar **)&sIKey )
              != IDE_SUCCESS );
    SDNB_IKEY_TO_KEYINFO( sIKey, sKeyInfo1 );

    IDE_TEST( sdpSlotDirectory::getPagePtrFromSlotNum( sSlotDirPtr, 
                                                       aDestSeq,
                                                       (UChar **)&sIKey )
              != IDE_SUCCESS );
    SDNB_IKEY_TO_KEYINFO( sIKey, sKeyInfo2 );

    sRet = compareKeyAndKey( aIndexStat,
                             aColumns,
                             aColumnFence,
                             &sKeyInfo1,
                             &sKeyInfo2,
                             ( SDNB_COMPARE_VALUE |
                               SDNB_COMPARE_PID   |
                               SDNB_COMPARE_OFFSET ),
                             aIsSameValue );

    *aResult = sRet;

    return IDE_SUCCESS;
    
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}
#endif

#ifdef DEBUG 
/*********************************************************************
 * FUNCTION DESCRIPTION : sdnbBTree::validateInternal                *
 * ------------------------------------------------------------------*
 * �־��� Internal node�� ���ռ��� �˻��Ѵ�. Key�� ������� sort�Ǿ� *
 * �ִ����� �˻��Ѵ�.                                                *
 *********************************************************************/
IDE_RC sdnbBTree::validateInternal( sdpPhyPageHdr * aNode,
                                    sdnbStatistic * aIndexStat,
                                    sdnbColumn    * aColumns,
                                    sdnbColumn    * aColumnFence )
{
    UChar         *sSlotDirPtr;
    SInt           sRet;
    SInt           sSlotCount;
    SInt           i;
    idBool         sIsSameValue = ID_FALSE;

    sSlotDirPtr = sdpPhyPage::getSlotDirStartPtr( (UChar *)aNode );
    sSlotCount  = sdpSlotDirectory::getCount( sSlotDirPtr );

    for (i = 0 ; i < (sSlotCount - 1) ; i ++ )
    {
        IDE_TEST( compareInternalKey( aNode,
                                      aIndexStat,
                                      i,
                                      i + 1,
                                      aColumns,
                                      aColumnFence,
                                      &sIsSameValue,
                                      &sRet )
                  != IDE_SUCCESS );

        if ( sRet >= 0 )
        {
            ideLog::log( IDE_ERR_0,
                         "%"ID_INT32_FMT"-th VS. %"ID_INT32_FMT"-th column problem.\n",
                         i, i + 1 );
            ideLog::logMem( IDE_DUMP_0, (UChar *)aColumns,
                            (vULong)aColumnFence - (vULong)aColumns,
                            "Column Dump:\n" );

            dumpIndexNode( aNode );
            IDE_ASSERT( 0 );
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}
#endif

/*********************************************************************
 * FUNCTION DESCRIPTION : sdnbBTree::findInternalKey                 *
 * ------------------------------------------------------------------*
 * Internal node���� �־��� ���� Key�� �̿��Ͽ� �б⸦ �Ѵ�.       *
 *********************************************************************/
IDE_RC sdnbBTree::findInternalKey( sdnbHeader           * aIndex,
                                   sdpPhyPageHdr        * aNode,
                                   sdnbStatistic        * aIndexStat,
                                   sdnbColumn           * aColumns,
                                   sdnbColumn           * aColumnFence,
                                   sdnbConvertedKeyInfo * aConvertedKeyInfo,
                                   ULong                  aIndexSmoNo,
                                   ULong                * aNodeSmoNo,
                                   scPageID             * aChildPID,
                                   SShort               * aKeyMapSeq,
                                   idBool               * aRetry,
                                   idBool               * aIsSameValueInSiblingNodes,
                                   scPageID             * aNextChildPID,
                                   UShort               * aNextSlotSize )
{
    UShort         sKeySlotCount;
    SShort         sHigh;
    SShort         sLow;
    SShort         sMed;
    UChar        * sPtr;
    sdnbIKey     * sIKey;
    SInt           sRet;
    idBool         sIsSameValue = ID_FALSE;
    sdnbKeyInfo    sKeyInfo;
    UChar        * sSlotDirPtr;

    *aRetry = ID_FALSE;

    IDL_MEM_BARRIER;

    if ( checkSMO(aIndexStat, aNode, aIndexSmoNo, aNodeSmoNo) > 0 )
    {
        *aRetry = ID_TRUE;
        return IDE_SUCCESS;
    }

    sSlotDirPtr   = sdpPhyPage::getSlotDirStartPtr( (UChar *)aNode );
    sKeySlotCount = sdpSlotDirectory::getCount( sSlotDirPtr );

    IDL_MEM_BARRIER;

    if ( checkSMO(aIndexStat, aNode, aIndexSmoNo, aNodeSmoNo) > 0 )
    {
        *aRetry = ID_TRUE;
        return IDE_SUCCESS;
    }

    sLow = 0;
    sHigh = sKeySlotCount -1;

    while( sLow <= sHigh )
    {
        sMed = (sLow + sHigh) >> 1;

        IDU_FIT_POINT( "3.BUG-43091@sdnbBTree::findInternalKey::Jump" );
        IDE_TEST( sdpSlotDirectory::getPagePtrFromSlotNum( sSlotDirPtr,
                                                           sMed,
                                                           (UChar **)&sIKey )
                  != IDE_SUCCESS );
        SDNB_IKEY_TO_KEYINFO( sIKey, sKeyInfo );

        sRet = compareConvertedKeyAndKey( aIndexStat,
                                          aColumns,
                                          aColumnFence,
                                          aConvertedKeyInfo,
                                          &sKeyInfo,
                                          SDNB_COMPARE_VALUE   |
                                          SDNB_COMPARE_PID     |
                                          SDNB_COMPARE_OFFSET,
                                          &sIsSameValue );

        if ( sRet >= 0 )
        {
            sLow = sMed + 1;
        }
        else
        {
            sHigh = sMed - 1;
        }
    }
    sMed = (sLow + sHigh) >> 1;

    *aKeyMapSeq = sMed;

    if ( sMed < 0 )
    {
        IDE_DASSERT(sMed == -1);
        sPtr = sdpPhyPage::getLogicalHdrStartPtr((UChar *)aNode);
        *aChildPID = ((sdnbNodeHdr*)sPtr)->mLeftmostChild;

        sMed = -1;

        *aIsSameValueInSiblingNodes = ID_FALSE;
    }
    else
    {
        IDE_TEST( sdpSlotDirectory::getPagePtrFromSlotNum( sSlotDirPtr,
                                                           sMed,
                                                           (UChar **)&sIKey )
                  != IDE_SUCCESS );

        SDNB_GET_RIGHT_CHILD_PID( aChildPID, sIKey );
        sKeyInfo.mKeyValue = SDNB_IKEY_KEY_PTR( sIKey ) ;

        sRet = compareConvertedKeyAndKey( aIndexStat,
                                          aColumns,
                                          aColumnFence,
                                          aConvertedKeyInfo,
                                          &sKeyInfo,
                                          SDNB_COMPARE_VALUE,
                                          &sIsSameValue );

        *aIsSameValueInSiblingNodes = sIsSameValue;
    }

    /*
     * [BUG-27210] [5.3.5] �� ������ �̻� ���� Uniqueness�˻��
     *             TreeLatch�� ��ȣ�Ǿ�� �մϴ�.
     * Next Key�� ���� Key Value�� ���´ٸ� Next Node���� ���� Ű��
     * ������ ���ɼ��� �ֱ� ������ TreeLatch�� ȹ���Ҽ� �־�� �մϴ�.
     */
    if ( *aIsSameValueInSiblingNodes == ID_FALSE )
    {
        if ( (sMed + 1) < sKeySlotCount )
        {
            IDE_TEST( sdpSlotDirectory::getPagePtrFromSlotNum( sSlotDirPtr,
                                                               sMed + 1,
                                                               (UChar **)&sIKey )
                      != IDE_SUCCESS );

            sKeyInfo.mKeyValue = SDNB_IKEY_KEY_PTR( sIKey ) ;
            sRet = compareConvertedKeyAndKey( aIndexStat,
                                              aColumns,
                                              aColumnFence,
                                              aConvertedKeyInfo,
                                              &sKeyInfo,
                                              SDNB_COMPARE_VALUE,
                                              &sIsSameValue );

            *aIsSameValueInSiblingNodes = sIsSameValue;
        }
    }
        
    if (*aKeyMapSeq == (sKeySlotCount - 1))
    {
        *aNextChildPID = SD_NULL_PID;
        *aNextSlotSize = 0;
    }
    else
    {
        IDE_TEST( sdpSlotDirectory::getPagePtrFromSlotNum( sSlotDirPtr,
                                                           sMed + 1,
                                                           (UChar **)&sIKey )
                  !=IDE_SUCCESS );
        *aNextSlotSize = getKeyLength( &(aIndex->mColLenInfoList),
                                       (UChar *)sIKey,
                                       ID_FALSE /* aIsLeaf */ );
        SDNB_GET_RIGHT_CHILD_PID( aNextChildPID, sIKey );
    }

    IDL_MEM_BARRIER;

    if ( checkSMO(aIndexStat, aNode, aIndexSmoNo, aNodeSmoNo) > 0 )
    {
        *aRetry = ID_TRUE;
    }

    return IDE_SUCCESS;
    
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : sdnbBTree::compareLeafKey                  *
 * ------------------------------------------------------------------*
 * Leaf ���������� �� ���� Leaf Key�� ���Ѵ�.                      *
 *********************************************************************/
IDE_RC sdnbBTree::compareLeafKey( idvSQL          * aStatistics,
                                  sdpPhyPageHdr   * aNode,
                                  sdnbStatistic   * aIndexStat,
                                  sdnbHeader      * aIndex,
                                  SShort            aSrcSeq,
                                  SShort            aDestSeq,
                                  sdnbColumn      * aColumns,
                                  sdnbColumn      * aColumnFence,
                                  idBool          * aIsSameValue,
                                  SInt            * sResult)
{
    SInt            sRet = 1 ;
    sdnbLKey      * sLKey;
    SInt            sSlotCount;
    sdnbKeyInfo     sKeyInfo1;
    sdnbKeyInfo     sKeyInfo2;
    UShort          sDummyLen;
    scPageID        sDummyPID;
    scPageID        sPID;
    ULong           sTmpBuf[SDNB_MAX_KEY_BUFFER_SIZE/ID_SIZEOF(ULong)];
    UChar         * sSlotDirPtr;

    sSlotDirPtr = sdpPhyPage::getSlotDirStartPtr( (UChar *)aNode );
    sSlotCount  = sdpSlotDirectory::getCount( sSlotDirPtr );

    IDE_TEST( sdpSlotDirectory::getPagePtrFromSlotNum( sSlotDirPtr, 
                                                       aSrcSeq,
                                                       (UChar **)&sLKey )
              != IDE_SUCCESS );
    SDNB_LKEY_TO_KEYINFO( sLKey, sKeyInfo1 );

    if ((sSlotCount - 1) == aSrcSeq)
    {
        sPID = sdpPhyPage::getNxtPIDOfDblList( aNode );

        if ( sPID == SD_NULL_PID )
        {
            *sResult = -1;
            return IDE_SUCCESS;
        }

        IDE_TEST(getKeyInfoFromPIDAndKey (aStatistics,
                                          aIndexStat,
                                          aIndex,
                                          sPID,
                                          0,
                                          ID_TRUE,  //aIsLeaf
                                          &sKeyInfo2,
                                          &sDummyLen,
                                          &sDummyPID,
                                          (UChar *)sTmpBuf)
                 != IDE_SUCCESS );
    }
    else
    {
        IDE_TEST( sdpSlotDirectory::getPagePtrFromSlotNum( sSlotDirPtr, 
                                                           aDestSeq,
                                                           (UChar **)&sLKey )
                  != IDE_SUCCESS );
        SDNB_LKEY_TO_KEYINFO( sLKey, sKeyInfo2 );
    }

    sRet = compareKeyAndKey( aIndexStat,
                             aColumns,
                             aColumnFence,
                             &sKeyInfo1,
                             &sKeyInfo2,
                             ( SDNB_COMPARE_VALUE |
                               SDNB_COMPARE_PID   |
                               SDNB_COMPARE_OFFSET ),
                             aIsSameValue );

    if ( (sKeyInfo1.mKeyState == SDNB_KEY_DEAD) ||
         (sKeyInfo2.mKeyState == SDNB_KEY_DEAD) )
    {
        if ( sRet > 0 )
        {
            ideLog::log( IDE_ERR_0,
                         "Source sequence : %"ID_INT32_FMT", Dest sequence : %"ID_INT32_FMT"\n",
                         aSrcSeq, aDestSeq );
            dumpIndexNode( aNode );
            IDE_ASSERT_MSG( 0, "Error Ouccurs in DRDB Index ID : %"ID_UINT32_FMT, aIndex->mIndexID );
        }
    }
    else
    {
        if ( sRet >= 0 )
        {
            ideLog::log( IDE_ERR_0,
                         "Source sequence : %"ID_INT32_FMT", Dest sequence : %"ID_INT32_FMT"\n",
                         aSrcSeq, aDestSeq );
            dumpIndexNode( aNode );
            IDE_ASSERT_MSG( 0, "Error Ouccurs in DRDB Index ID : %"ID_UINT32_FMT, aIndex->mIndexID );
        }
    }

    *sResult = sRet;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

#ifdef DEBUG 
/*********************************************************************
 * FUNCTION DESCRIPTION : sdnbBTree::validateNodeSpace               *
 * ------------------------------------------------------------------*
 * Leaf/Internal ���������� ���� ��� ������ �����Ѵ�.               *
 * �� ������ ũ�� (Ű�� �ϳ��� �������� �ʾ������� ũ��) =           *
 *   ���� ����� ��� KEY Ű���� ũ�� + ����ȭ���� ���� ������ ��    *
 *********************************************************************/
IDE_RC sdnbBTree::validateNodeSpace( sdnbHeader    * aIndex,
                                     sdpPhyPageHdr * aNode,
                                     idBool          aIsLeaf )
{
    UChar        * sSlotDirPtr;
    UShort         sSlotCount;
    UShort         sTotalKeySize = 0;
    UShort         sKeySize;
    UInt           i;
    UChar        * sKey;

    sSlotDirPtr = sdpPhyPage::getSlotDirStartPtr( (UChar *)aNode );
    sSlotCount  = sdpSlotDirectory::getCount( sSlotDirPtr );

    for ( i = 0; i < sSlotCount ; i++ )
    {
        IDE_ERROR( sdpSlotDirectory::getPagePtrFromSlotNum( sSlotDirPtr,
                                                            i,
                                                            &sKey )
                   == IDE_SUCCESS );

        sKeySize = getKeyLength( &(aIndex->mColLenInfoList),
                                 (UChar *)sKey,
                                 aIsLeaf );

        sTotalKeySize += sKeySize;
    }

    if ( calcPageFreeSize( aNode ) !=
         (sdpPhyPage::getTotalFreeSize( aNode ) + sTotalKeySize) )
    {
        ideLog::log( IDE_ERR_0,
                     "CalcPageFreeSize : %"ID_UINT32_FMT""
                     ", Page Total Free Size : %"ID_UINT32_FMT""
                     ", Total Key Size : %"ID_UINT32_FMT""
                     ", Key Count : %"ID_UINT32_FMT"\n",
                     calcPageFreeSize( aNode ),
                     sdpPhyPage::getTotalFreeSize( aNode ),
                     sTotalKeySize, sSlotCount );
        dumpIndexNode( aNode );
        IDE_ASSERT( 0 );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}
#endif

#ifdef DEBUG 
/*********************************************************************
 * FUNCTION DESCRIPTION : sdnbBTree::validateLeaf                    *
 * ------------------------------------------------------------------*
 * �־��� Leaf node�� ���ռ��� �˻��Ѵ�.                             *
 * 1. Key Ordering : Key�� ������� sort�Ǿ� �ִ��� �˻��Ѵ�.        *
 * 2. Reference Key : CTS�� Reference ������ ��Ȯ���� �˻��Ѵ�.      *
 *********************************************************************/
IDE_RC sdnbBTree::validateLeaf(idvSQL *          aStatistics,
                               sdpPhyPageHdr *   aNode,
                               sdnbStatistic *   aIndexStat,
                               sdnbHeader *      aIndex,
                               sdnbColumn *      aColumns,
                               sdnbColumn *      aColumnFence)
{
    UChar        * sSlotDirPtr;
    SInt           sSlotCount;
    SShort         sSlotNum;
    SInt           i;
    SInt           j;
    idBool         sIsSameValue = ID_FALSE;
    sdnbLKey     * sLeafKey;
    sdnCTL       * sCTL;
    sdnCTS       * sCTS;
    UShort         sRefKeyCount;
    SInt           sRet;

    sSlotDirPtr = sdpPhyPage::getSlotDirStartPtr( (UChar *)aNode );
    sSlotCount  = sdpSlotDirectory::getCount( sSlotDirPtr );

    /*
     * 1. Key Ordering
     * Ű���� ������ ��Ȯ���� �����Ѵ�.
     */
    for ( i = 0 ; i < (sSlotCount - 1) ; i ++ )
    {
        IDE_TEST( compareLeafKey( aStatistics,
                                  aNode,
                                  aIndexStat,
                                  aIndex,
                                  i,
                                  i + 1,
                                  aColumns,
                                  aColumnFence,
                                  &sIsSameValue,
                                  &sRet )
                  != IDE_SUCCESS );
    }

    sCTL = sdnIndexCTL::getCTL( aNode );

    /*
     * 2. Reference Key
     * CTS�� Reference Info�� �˻��Ѵ�.
     */
    for ( i = 0 ; i < sdnIndexCTL::getCount( sCTL ) ; i++ )
    {
        sCTS = sdnIndexCTL::getCTS( sCTL, i );

        /*
         * 2.1 Reference Key Offset
         * CTS->mRefKey[i]�� offset�� SlotEntryDir���� �����ؾ� �Ѵ�.
         */
        if ( (sdnIndexCTL::getCTSlotState( sCTS ) == SDN_CTS_STAMPED) ||
             (sdnIndexCTL::getCTSlotState( sCTS ) == SDN_CTS_UNCOMMITTED) )
        {
            for ( j = 0 ; j < SDN_CTS_MAX_KEY_CACHE ; j++ )
            {
                if ( sCTS->mRefKey[j] != SDN_CTS_KEY_CACHE_NULL )
                {
                    IDE_TEST( sdpSlotDirectory::find( sSlotDirPtr,
                                                      sCTS->mRefKey[j],
                                                      &sSlotNum )
                              != IDE_SUCCESS );
                    if ( sSlotNum < 0 )
                    {
                        ideLog::log( IDE_ERR_0,
                                     "Current CTL idx : %"ID_INT32_FMT""
                                     ", Current CTS cache idx : %"ID_INT32_FMT"\n",
                                     i, j );
                        dumpIndexNode( aNode );
                        IDE_ASSERT( 0 );
                    }
                }
            }
        }

        /*
         * 2.2 Reference Key Count
         * ���������� Key���� ����Ű�� CTS#�� ������
         * CTS->mRefCnt�� �����ؾ� �Ѵ�.
         */
        if ( (sdnIndexCTL::getCTSlotState( sCTS ) == SDN_CTS_STAMPED) ||
             (sdnIndexCTL::getCTSlotState( sCTS ) == SDN_CTS_UNCOMMITTED) )
        {
            sRefKeyCount = 0;

            for ( j = 0 ; j < sSlotCount ; j++ )
            {
                IDE_ERROR( sdpSlotDirectory::getPagePtrFromSlotNum( 
                                                            sSlotDirPtr,
                                                            j,
                                                            (UChar **)&sLeafKey )
                           == IDE_SUCCESS );


                if ( SDNB_GET_STATE( sLeafKey ) == SDNB_KEY_DEAD )
                {
                    continue;
                }

                if ( SDNB_GET_CCTS_NO( sLeafKey ) == i )
                {
                    sRefKeyCount++;
                }
                if ( SDNB_GET_LCTS_NO( sLeafKey ) == i )
                {
                    sRefKeyCount++;
                }
            }

            if ( sRefKeyCount > sCTS->mRefCnt )
            {
                ideLog::log( IDE_ERR_0,
                             "Current CTL idx : %"ID_INT32_FMT""
                             ", Slot count : %"ID_INT32_FMT""
                             ", Ref Key Count : %"ID_UINT32_FMT""
                             ", CTS ref count : %"ID_UINT32_FMT"\n",
                             i, sSlotCount, sRefKeyCount, sCTS->mRefCnt );
                dumpIndexNode( aNode );
                IDE_ASSERT( 0 );
            }
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}
#endif

/*********************************************************************
 * FUNCTION DESCRIPTION : sdnbBTree::findLeafKey                     *
 * ------------------------------------------------------------------*
 * �־��� Leaf node���� �ش� Row, RID�� ã�´�. aSlotSeq�� aRow��    *
 * �ش��ϴ� key�� �ִٸ� �����ؾ� �ϴ� slot ��ȣ�̴�.                *
 *********************************************************************/
IDE_RC sdnbBTree::findLeafKey( sdpPhyPageHdr        * aNode,
                               sdnbStatistic        * aIndexStat,
                               sdnbColumn           * aColumns,
                               sdnbColumn           * aColumnFence,
                               sdnbConvertedKeyInfo * aConvertedKeyInfo,
                               ULong                  aIndexSmoNo,
                               ULong                * aNodeSmoNo,
                               SShort               * aKeyMapSeq,
                               idBool               * aRetry,
                               idBool               * aIsSameKey )
{
    UChar                 *sSlotDirPtr;
    SShort                 sHigh;
    SShort                 sLow;
    SShort                 sMed;
    SInt                   sRet;
    sdnbLKey              *sLKey;
    sdnbKeyInfo            sKeyInfo;
    idBool                 sIsSameValue = ID_FALSE;

    *aRetry = ID_FALSE;

    IDL_MEM_BARRIER;

    if ( checkSMO(aIndexStat, aNode, aIndexSmoNo, aNodeSmoNo) > 0 )
    {
        *aRetry = ID_TRUE;
        IDE_CONT( RETRY );
    }

    *aIsSameKey = ID_FALSE;
    sLow = 0;

    sSlotDirPtr = sdpPhyPage::getSlotDirStartPtr( (UChar *)aNode );
    sHigh = sdpSlotDirectory::getCount( sSlotDirPtr ) - 1;

    while( sLow <= sHigh )
    {
        sMed = (sLow + sHigh) >> 1;

        IDE_ERROR( sdpSlotDirectory::getPagePtrFromSlotNum( sSlotDirPtr,
                                                            sMed,
                                                            (UChar **)&sLKey )
                    == IDE_SUCCESS );
        SDNB_LKEY_TO_KEYINFO( sLKey, sKeyInfo );

        sRet = compareConvertedKeyAndKey( aIndexStat,
                                          aColumns,
                                          aColumnFence,
                                          aConvertedKeyInfo,
                                          &sKeyInfo,
                                          SDNB_COMPARE_VALUE   |
                                          SDNB_COMPARE_PID     |
                                          SDNB_COMPARE_OFFSET,
                                          &sIsSameValue );

        if ( sRet > 0 )
        {
            sLow = sMed + 1;
        }
        else
        {
            if ( sRet == 0 )
            {
                *aIsSameKey = ID_TRUE;
            }
            sHigh = sMed - 1;
        }
    }

    sMed = (sLow + sHigh) >> 1;

    // sMed�� beforeFirst�� ���� ��ġ�̹Ƿ�, ���� ��ġ�� �� ���� ����.
    *aKeyMapSeq = sMed + 1;

    IDE_EXCEPTION_CONT( RETRY );

    return IDE_SUCCESS;
    
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : compareKeyAndVRow                          *
 * ------------------------------------------------------------------*
 * CursorLevelVisibilityCheck�ÿ��� ȣ��ȴ�. VRow�� Key�� ���ϸ�, *
 * �� ����� VALUE ���̴�. �׿��� ���� ����ϸ� �ȵȴ�.          *
 *********************************************************************/
SInt sdnbBTree::compareKeyAndVRow( sdnbStatistic      * aIndexStat,
                                   const sdnbColumn   * aColumns,
                                   const sdnbColumn   * aColumnFence,
                                   sdnbKeyInfo        * aKeyInfo,
                                   sdnbKeyInfo        * aVRowInfo )
{
    SInt              sResult;
    smiValueInfo      sValueInfo1;
    smiValueInfo      sValueInfo2;
    UInt              sColumnHeaderLength;
    UChar           * sKeyValue;

    sKeyValue = aKeyInfo->mKeyValue;

    for ( ;
          aColumns < aColumnFence ;
          aColumns++ )
    {
        // PROJ-2429 Dictionary based data compress for on-disk DB
        // UMR�� ���� mCompareKeyAndKey�Լ� ���� mtd::valueForModule����
        // valueInfo�� flag�� �߸� �д� ��찡 �߻�
        sValueInfo1.flag = SMI_OFFSET_USE;

        SDNB_KEY_TO_SMIVALUEINFO( &(aColumns->mKeyColumn),
                                  sKeyValue,
                                  &sColumnHeaderLength,
                                  &sValueInfo1 );

        sValueInfo2.column = &(aColumns->mVRowColumn);
        sValueInfo2.value  = aVRowInfo->mKeyValue;
        sValueInfo2.flag   = SMI_OFFSET_USE;

        sResult = aColumns->mCompareKeyAndVRow( &sValueInfo1,
                                                &sValueInfo2 );
        aIndexStat->mKeyCompareCount++;

        if ( sResult != 0 )
        {
            return sResult;
        }
    }

    return 0;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : compareConvertedKeyAndKey                  *
 * ------------------------------------------------------------------*
 * ConvertedKey(smiValue����)�� Key�� ���Ѵ�.                      *
 * �� ����� VALUE, PID, SlotNum �̸�, ������ ���� �� ���δ�     *
 * aCompareFlag��  ���� �����ȴ�.                                  *
 *********************************************************************/
SInt sdnbBTree::compareConvertedKeyAndKey( sdnbStatistic        * aIndexStat,
                                           const sdnbColumn     * aColumns,
                                           const sdnbColumn     * aColumnFence,
                                           sdnbConvertedKeyInfo * aConvertedKeyInfo,
                                           sdnbKeyInfo          * aKeyInfo,
                                           UInt                   aCompareFlag,
                                           idBool               * aIsSameValue )
{
    SInt              sResult;
    smiValue        * sConvertedKeySmiValue;
    smiValueInfo      sValueInfo1;
    smiValueInfo      sValueInfo2;
    UInt              sColumnHeaderLength;
    UChar           * sKeyValue;

    IDE_DASSERT( (aCompareFlag & SDNB_COMPARE_VALUE) == SDNB_COMPARE_VALUE );

    *aIsSameValue = ID_FALSE;

    sConvertedKeySmiValue = aConvertedKeyInfo->mSmiValueList;
    sKeyValue             = aKeyInfo->mKeyValue;

    for ( ;
         aColumns < aColumnFence ;
         aColumns++ )
    {
        // PROJ-2429 Dictionary based data compress for on-disk DB
        // UMR�� ���� mCompareKeyAndKey�Լ� ���� mtd::valueForModule����
        // valueInfo�� flag�� �߸� �д� ��찡 �߻�
        sValueInfo1.flag = SMI_OFFSET_USE;
        sValueInfo2.flag = SMI_OFFSET_USE;

        sValueInfo1.column = &(aColumns->mKeyColumn);
        sValueInfo1.value  = sConvertedKeySmiValue->value;
        sValueInfo1.length = sConvertedKeySmiValue->length;

        SDNB_KEY_TO_SMIVALUEINFO( &(aColumns->mKeyColumn),
                                  sKeyValue,
                                  &sColumnHeaderLength,
                                  &sValueInfo2 );

        sResult = aColumns->mCompareKeyAndKey( &sValueInfo1,
                                               &sValueInfo2 );
        aIndexStat->mKeyCompareCount++;
        sConvertedKeySmiValue ++;

        if ( sResult != 0 )
        {
            return sResult;
        }
    }
    *aIsSameValue = ID_TRUE;

    if ( ( aCompareFlag & SDNB_COMPARE_PID ) == SDNB_COMPARE_PID )
    {
        if ( aConvertedKeyInfo->mKeyInfo.mRowPID > aKeyInfo->mRowPID )
        {
            return 1;
        }
        if ( aConvertedKeyInfo->mKeyInfo.mRowPID < aKeyInfo->mRowPID )
        {
            return -1;
        }
    }

    if ( ( aCompareFlag & SDNB_COMPARE_OFFSET ) == SDNB_COMPARE_OFFSET )
    {
        if ( aConvertedKeyInfo->mKeyInfo.mRowSlotNum > aKeyInfo->mRowSlotNum )
        {
            return 1;
        }
        if ( aConvertedKeyInfo->mKeyInfo.mRowSlotNum < aKeyInfo->mRowSlotNum )
        {
            return -1;
        }
    }

    return 0;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : CompareKeyAndKey                           *
 * ------------------------------------------------------------------*
 * Key�� Key�� ���Ѵ�. �� ����� VALUE, PID, SlotNum �̸�,       *
 * ������ ���� �� ���δ� aCompareFlag��  ���� �����ȴ�.          *
 *********************************************************************/
SInt sdnbBTree::compareKeyAndKey( sdnbStatistic    * aIndexStat,
                                  const sdnbColumn * aColumns,
                                  const sdnbColumn * aColumnFence,
                                  sdnbKeyInfo      * aKey1Info,
                                  sdnbKeyInfo      * aKey2Info,
                                  UInt               aCompareFlag,
                                  idBool           * aIsSameValue )
{
    SInt              sResult;
    smiValueInfo      sValueInfo1;
    smiValueInfo      sValueInfo2;
    UInt              sColumnHeaderLength;
    UChar           * sKey1Value;
    UChar           * sKey2Value;

    IDE_DASSERT( (aCompareFlag & SDNB_COMPARE_VALUE) == SDNB_COMPARE_VALUE );

    *aIsSameValue = ID_FALSE;

    sKey1Value = aKey1Info->mKeyValue;
    sKey2Value = aKey2Info->mKeyValue;

    for ( ;
         aColumns < aColumnFence ;
         aColumns++ )
    {
        // PROJ-2429 Dictionary based data compress for on-disk DB
        // UMR�� ���� mCompareKeyAndKey�Լ� ���� mtd::valueForModule����
        // valueInfo�� flag�� �߸� �д� ��찡 �߻�
        sValueInfo1.flag = SMI_OFFSET_USE;
        sValueInfo2.flag = SMI_OFFSET_USE;

        SDNB_KEY_TO_SMIVALUEINFO( &(aColumns->mKeyColumn),
                                  sKey1Value,
                                  &sColumnHeaderLength,
                                  &sValueInfo1 );
        SDNB_KEY_TO_SMIVALUEINFO( &(aColumns->mKeyColumn),
                                  sKey2Value,
                                  &sColumnHeaderLength,
                                  &sValueInfo2 );

        sResult = aColumns->mCompareKeyAndKey( &sValueInfo1,
                                               &sValueInfo2 );
        aIndexStat->mKeyCompareCount++;

        if ( sResult != 0 )
        {
            return sResult;
        }
    }

    *aIsSameValue = ID_TRUE;

    if ( ( aCompareFlag & SDNB_COMPARE_PID ) == SDNB_COMPARE_PID )
    {
        if ( aKey1Info->mRowPID > aKey2Info->mRowPID )
        {
            return 1;
        }
        if ( aKey1Info->mRowPID < aKey2Info->mRowPID )
        {
            return -1;
        }
    }

    if ( ( aCompareFlag & SDNB_COMPARE_OFFSET ) == SDNB_COMPARE_OFFSET )
    {
        if ( aKey1Info->mRowSlotNum > aKey2Info->mRowSlotNum )
        {
            return 1;
        }
        if ( aKey1Info->mRowSlotNum < aKey2Info->mRowSlotNum )
        {
            return -1;
        }
    }

    return 0;
}

/*********************************************************************
 * FUNCTION DESCRIPTION :                                            *
 * ------------------------------------------------------------------*
 * Index Key������ ����� �ش� Column�� Null ���θ� üũ�Ѵ�.        *
 * Null�̸� ID_TRUE, Null�� �ƴϸ� ID_FALSE��                        *
 *********************************************************************/
idBool sdnbBTree::isNullColumn( sdnbColumn * aIndexColumn,
                                UChar      * aColumnPtr)
{
    idBool       sResult;
    smiColumn  * sKeyColumn;
    ULong        sTempLengthKnownKeyBuf[ SDNB_MAX_LENGTH_KNOWN_COLUMN_SIZE
                                         / ID_SIZEOF(ULong) ];
    smOID        sOID;
    UInt         sLength;
    const void * sDictValue;

    IDE_DASSERT( aIndexColumn != NULL );
    IDE_DASSERT( aColumnPtr   != NULL );

    sResult      = ID_FALSE;
    sKeyColumn   = &aIndexColumn->mKeyColumn;

    if ( ( sKeyColumn->flag & SMI_COLUMN_COMPRESSION_MASK ) 
         != SMI_COLUMN_COMPRESSION_TRUE )
    {
        if ( ( sKeyColumn->flag & SMI_COLUMN_LENGTH_TYPE_MASK ) ==
                              SMI_COLUMN_LENGTH_TYPE_KNOWN ) //LENGTH_KNOWN
        {
            idlOS::memcpy( sTempLengthKnownKeyBuf, aColumnPtr, sKeyColumn->size );
            sResult = aIndexColumn->mIsNull( NULL, /* mtcColumn�� �Ѿ���� */
                                             sTempLengthKnownKeyBuf ) ;
        }
        else //LENGTH_UNKNOWN
        {
            sResult = (aColumnPtr[0] == SDNB_NULL_COLUMN_PREFIX)
                            ? ID_TRUE : ID_FALSE;
        }
    }
    else
    {
        // BUG-37464 
        // compressed column�� ���ؼ� index ������ null �˻縦 �ùٷ� ���� ����
        // PROJ-2429 Dictionary based data compress for on-disk DB
        if ( ( sKeyColumn->flag & SMI_COLUMN_LENGTH_TYPE_MASK ) ==
                                   SMI_COLUMN_LENGTH_TYPE_KNOWN ) //LENGTH_KNOWN
        {
            idlOS::memcpy( &sOID, 
                           aColumnPtr, 
                           ID_SIZEOF(smOID));
        }
        else
        {
            idlOS::memcpy( &sOID, 
                           aColumnPtr + SDNB_SMALL_COLUMN_HEADER_LENGTH, 
                           ID_SIZEOF(smOID));
        }

        sDictValue = smiGetCompressionColumn( (const void *)&sOID, 
                                              sKeyColumn, 
                                              ID_FALSE, 
                                              &sLength );

        sResult = aIndexColumn->mIsNull( NULL,/* mtcColumn�� �Ѿ���� */
                                         sDictValue ) ;
    }

    return sResult;
}

/* BUG-30074 disk table�� unique index���� NULL key ���� ��/
 *           non-unique index���� deleted key �߰� ��
 *           Cardinality�� ��Ȯ���� ���� �������ϴ�.
 * Key ��ü�� Null���� Ȯ���Ѵ�. ��� Null�̾�� �Ѵ�. */
idBool sdnbBTree::isNullKey( sdnbHeader * aHeader,
                             UChar      * aKey )
{
    sdnbColumn         * sColumn;
    idBool               sIsNull = ID_TRUE;
    scOffset             sOffset = 0;
    UInt                 sColumnHeaderLen;
    UInt                 sColumnValueLen;

    for ( sColumn = aHeader->mColumns ; 
          sColumn != aHeader->mColumnFence ; 
          sColumn++ )
    {
        if ( sdnbBTree::isNullColumn( sColumn, (UChar *)(aKey) + sOffset ) 
            == ID_FALSE )
        {
            sIsNull = ID_FALSE;
            break;
        }
        else
        {
            /* nothing to do */
        } 

        sOffset += sdnbBTree::getColumnLength( 
                                SDNB_GET_COLLENINFO( &(sColumn->mKeyColumn) ),
                                ((UChar *)aKey) + sOffset,
                                &sColumnHeaderLen,
                                &sColumnValueLen,
                                NULL );
    }

    return sIsNull;
}

/*********************************************************************
 * FUNCTION DESCRIPTION :                                            *
 * ------------------------------------------------------------------*
 * smiValue ������ ���� String���� ��ȯ�Ѵ�.                         *
 * D$DISK_BTREE_KEY, X$DISK_BTREE_HEADER���� value24b����� ���� ���*
 * �Ѵ�.                                                             *
 *********************************************************************/
IDE_RC sdnbBTree::columnValue2String ( sdnbColumn       * aIndexColumn,
                                       UChar              aTargetColLenInfo,
                                       UChar            * aColumnPtr,
                                       UChar            * aText,
                                       UInt             * aTextLen,
                                       IDE_RC           * aRet )
{
    ULong            sTempColumnBuf[SDNB_MAX_KEY_BUFFER_SIZE/ ID_SIZEOF(ULong)];
    UInt             sColumnHeaderLength;
    UInt             sColumnValueLength;
    void           * sColumnValuePtr;
    UInt             sLength;

    IDE_DASSERT(  aColumnPtr!= NULL );
    IDE_DASSERT(  aText     != NULL );
    IDE_DASSERT( *aTextLen   > 0 );
    IDE_DASSERT(  aRet      != NULL );

    getColumnLength( aTargetColLenInfo,
                     aColumnPtr,
                     &sColumnHeaderLength,
                     &sColumnValueLength,
                     NULL );
    aIndexColumn->mCopyDiskColumnFunc( aIndexColumn->mKeyColumn.size,
                                       sTempColumnBuf,
                                       0, /* aDestValueOffset */
                                       sColumnValueLength,
                                       ((UChar *)aColumnPtr)+sColumnHeaderLength );

    /* PROJ-2429 Dictionary based data compress for on-disk DB */
    if ( ( aIndexColumn->mKeyColumn.flag & SMI_COLUMN_COMPRESSION_MASK ) 
         == SMI_COLUMN_COMPRESSION_TRUE )
    {
        sColumnValuePtr = (void *)smiGetCompressionColumn( sTempColumnBuf,
                                                           &aIndexColumn->mKeyColumn,
                                                           ID_FALSE, // aUseColumnOffset
                                                           &sLength );
    }
    else
    {
        sColumnValuePtr = (void *)sTempColumnBuf;
    }

    return aIndexColumn->mKey2String( &aIndexColumn->mKeyColumn,
                                      sColumnValuePtr,
                                      aIndexColumn->mKeyColumn.size,
                                      (UChar *) SM_DUMP_VALUE_DATE_FMT,
                                      idlOS::strlen( SM_DUMP_VALUE_DATE_FMT ),
                                      aText,
                                      aTextLen,
                                      aRet ) ;
}

/*********************************************************************
 * FUNCTION DESCRIPTION :                                            *
 * ------------------------------------------------------------------*
 * �α�� Ű ���̸� �� �� �ֵ��� Ű ���� ������ �������ش�.          *
 * Ű ���� ������                                                    *
 *                                                                   *
 * +-------------+-------------+-------------+      +-------------+  *
 * | Col Cnt(1B) | Col Len(1B) | Col Len(1B) | .... | Col Len (1B)|  *
 * +-------------+-------------+-------------+      +-------------+  *
 *                                                                   *
 *  ó������ SlotHdrLen�� ����. �̴� Islot�̳� LKey�̳Ŀ� ����  *
 * ������ ũ������ �̴�. ���� Column Count�� ��ϵȴ�.               *
 *                                                                   *
 *  ���Ŀ��� Column Length�� 1Byte�� ��ġ�Ѵ�.                       *
 * Length-known Type (ex-Integer,Date)���� ���� ���̰� ����Ǹ�,   *
 * Length-Unknown Type (ex-Char, Varchar)�� ��� 0�� ����ȴ�. ��    *
 * ���̰� 0�� ��� Stored Length������ ����� Į�� ���̸� ������ �ȴ�*
 *                                                                   *
 *  ��� ���� 1Byte�� ��ϵ� �� �ִ� ������ Length-Knwon type�� ���� *
 * �� 40 �����̱� �����̴�.                                          *
 *                                                                   *
 *********************************************************************/
void sdnbBTree::makeColLenInfoList ( const sdnbColumn   * aColumns,
                                     const sdnbColumn   * aColumnFence,
                                     sdnbColLenInfoList * aColLenInfoList )
{
    UInt       sLoop;

    IDE_DASSERT( aColumns        != NULL );
    IDE_DASSERT( aColumnFence    != NULL );
    IDE_DASSERT( aColLenInfoList != NULL );

    for ( sLoop = 0 ;
          aColumns < aColumnFence ;
          aColumns++, sLoop++ )
    {
        aColLenInfoList->mColLenInfo[ sLoop ] = SDNB_GET_COLLENINFO( &(aColumns->mKeyColumn) );
    }

    aColLenInfoList->mColumnCount = (UChar)sLoop;
}

#if 0  //not used
/*********************************************************************
 * FUNCTION DESCRIPTION :                                            *
 * ------------------------------------------------------------------*
 * Unchained Key(Normal Key)�� Chained Key�� �����Ѵ�.               *
 * Chained Key��� ���� Key�� ����Ű�� Ʈ����� ������ CTS Chain�� *
 * ������ �ǹ��Ѵ�.                                                  *
 * �̹� Chained Key�� �� �ٽ� Chained Key�� �ɼ� ������, Chained     *
 * Key�� ���� ������ UNDO�� ��ϵǰ�, ���� Visibility �˻�ÿ�       *
 * �̿�ȴ�.                                                         *
 *********************************************************************/
IDE_RC sdnbBTree::logAndMakeChainedKeys( sdrMtx        * aMtx,
                                         sdpPhyPageHdr * aPage,
                                         UChar           aCTSlotNum,
                                         UChar         * aContext,
                                         UChar         * aKeyList,
                                         UShort        * aKeyListSize,
                                         UShort        * aChainedKeyCount )
{
    IDE_TEST( writeChainedKeysLog( aMtx,
                                   aPage,
                                   aCTSlotNum )
              != IDE_SUCCESS );

    IDE_TEST( makeChainedKeys( aPage,
                               aCTSlotNum,
                               aContext,
                               aKeyList,
                               aKeyListSize,
                               aChainedKeyCount )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}
#endif
#if 0
/*********************************************************************
 * FUNCTION DESCRIPTION :                                            *
 * ------------------------------------------------------------------*
 * Unchained Key(Normal Key)�� Chained Key�� ���濡 ���� �α׸�      *
 * �����.                                                           *
 *********************************************************************/
IDE_RC sdnbBTree::writeChainedKeysLog( sdrMtx        * aMtx,
                                       sdpPhyPageHdr * aPage,
                                       UChar           aCTSlotNum )
{
    IDE_ASSERT( aMtx  != NULL );
    IDE_ASSERT( aPage != NULL );

    IDE_TEST( sdrMiniTrans::writeLogRec( aMtx,
                                         (UChar *)aPage,
                                         NULL,
                                         ID_SIZEOF(UChar),
                                         SDR_SDN_MAKE_CHAINED_KEYS )
              != IDE_SUCCESS );

    IDE_TEST( sdrMiniTrans::write( aMtx,
                                   (void *)&aCTSlotNum,
                                   ID_SIZEOF(UChar) )
              != IDE_SUCCESS );


    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}
#endif
#if 0
/*********************************************************************
 * FUNCTION DESCRIPTION :                                            *
 * ------------------------------------------------------------------*
 * Unchained Key(Normal Key)�� Chained Key�� �����Ѵ�.               *
 *********************************************************************/
IDE_RC sdnbBTree::makeChainedKeys( sdpPhyPageHdr * aPage,
                                   UChar           aCTSlotNum,
                                   UChar         * aContext,
                                   UChar         * aKeyList,
                                   UShort        * aKeyListSize,
                                   UShort        * aChainedKeyCount )
{
    UShort                sSlotCount;
    sdnbLKey            * sLeafKey;
    UInt                  i;
    UChar               * sKeyList;
    UChar                 sChainedCCTS;
    UChar                 sChainedLCTS;
    sdnbCallbackContext * sContext;
    UShort                sKeySize;
    sdnbHeader          * sIndex;
    UChar               * sSlotDirPtr;

    sContext = (sdnbCallbackContext*)aContext;
    sIndex   = sContext->mIndex;
    sKeyList = aKeyList;
    *aKeyListSize = 0;

    sSlotDirPtr = sdpPhyPage::getSlotDirStartPtr( (UChar *)aPage );
    sSlotCount  = sdpSlotDirectory::getCount( sSlotDirPtr );

    for ( i = 0 ; i < sSlotCount ; i++ )
    {
        IDE_TEST( sdpSlotDirectory::getPagePtrFromSlotNum( sSlotDirPtr,
                                                           i,
                                                           (UChar **)&sLeafKey )
                  != IDE_SUCCESS );

        /*
         * DEAD KEY�� STABLE KEY�� Chained Key�� �ɼ� ����.
         */
        if ( ( SDNB_GET_STATE( sLeafKey ) == SDNB_KEY_DEAD ) ||
             ( SDNB_GET_STATE( sLeafKey ) == SDNB_KEY_STABLE ) )
        {
            continue;
        }
        else
        {
            /* nothing to do */
        } 

        sChainedCCTS = SDN_CHAINED_NO;
        sChainedLCTS = SDN_CHAINED_NO;

        /*
         * �̹� Chained Key�� �ٽ� Chained Key�� �ɼ� ����.
         */
        if ( ( SDNB_GET_CCTS_NO( sLeafKey ) == aCTSlotNum ) &&
             ( SDNB_GET_CHAINED_CCTS( sLeafKey ) == SDN_CHAINED_NO ) )
        {
            SDNB_SET_CHAINED_CCTS( sLeafKey, SDN_CHAINED_YES );
            sChainedCCTS = SDN_CHAINED_YES;
            (*aChainedKeyCount)++;
        }
        else
        {
            /* nothing to do */
        } 

        if ( ( SDNB_GET_LCTS_NO( sLeafKey ) == aCTSlotNum ) &&
             ( SDNB_GET_CHAINED_LCTS( sLeafKey ) == SDN_CHAINED_NO ) )
        {
            SDNB_SET_CHAINED_LCTS( sLeafKey, SDN_CHAINED_YES );
            sChainedLCTS = SDN_CHAINED_YES;
            (*aChainedKeyCount)++;
        }
        else
        {
            /* nothing to do */
        } 

        if ( ( sChainedCCTS == SDN_CHAINED_YES ) ||
             ( sChainedLCTS == SDN_CHAINED_YES ) )
        {
            sKeySize = getKeyLength( &(sIndex->mColLenInfoList),
                                     (UChar *)sLeafKey,
                                     ID_TRUE );
            idlOS::memcpy( sKeyList, sLeafKey, sKeySize );

            SDNB_SET_CHAINED_CCTS( ((sdnbLKey*)sKeyList), sChainedCCTS );
            SDNB_SET_CHAINED_LCTS( ((sdnbLKey*)sKeyList), sChainedLCTS );

            sKeyList += sKeySize;
            *aKeyListSize += sKeySize;
        }
        else
        {
            /* nothing to do */
        } 
    }

    /*
     * Key�� ���°� DEAD�� ���( LimitCTS�� Stamping�� �� ���) ���
     * CTS.mRefCnt���� ������ �ִ�
     */
    if ( ( *aChainedKeyCount > sdnIndexCTL::getRefKeyCount( aPage, aCTSlotNum ) ) ||
         ( (ID_SIZEOF(sdnCTS) + ID_SIZEOF(UShort) + *aKeyListSize) >= SD_PAGE_SIZE) )
    {
        ideLog::log( IDE_ERR_0,
                     "CTS slot number : %"ID_UINT32_FMT""
                     ", Chained key count : %"ID_UINT32_FMT""
                     ", Ref Key count : %"ID_UINT32_FMT""
                     ", Key List size : %"ID_UINT32_FMT"\n",
                     aCTSlotNum, *aChainedKeyCount,
                     sdnIndexCTL::getRefKeyCount( aPage, aCTSlotNum ),
                     *aKeyListSize );

        dumpIndexNode( aPage );
        IDE_ASSERT( 0 );
    }
    else
    {
        /* nothing to do */
    } 

    return IDE_SUCCESS;
    
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}
#endif
#if 0
/*********************************************************************
 * FUNCTION DESCRIPTION :                                            *
 * ------------------------------------------------------------------*
 * Chained Key�� Unchained Key(Normal Key)�� �����Ѵ�.               *
 * Unchained Key��� ���� Key�� ����Ű�� Ʈ����� ������ Key.CTS#��  *
 * ����Ű�� CTS�� ������ �ǹ��Ѵ�.                                   *
 *********************************************************************/
IDE_RC sdnbBTree::logAndMakeUnchainedKeys( idvSQL        * aStatistics,
                                           sdrMtx        * aMtx,
                                           sdpPhyPageHdr * aPage,
                                           sdnCTS        * aCTS,
                                           UChar           aCTSlotNum,
                                           UChar         * aChainedKeyList,
                                           UShort          aChainedKeySize,
                                           UShort        * aUnchainedKeyCount,
                                           UChar         * aContext )
{
    UChar       * sSlotDirPtr;
    UChar       * sUnchainedKey = NULL;
    UInt          sUnchainedKeySize = 0;
    UInt          sState = 0;
    UShort        sSlotCount;

    sSlotDirPtr = sdpPhyPage::getSlotDirStartPtr( (UChar *)aPage );
    sSlotCount  = sdpSlotDirectory::getCount( sSlotDirPtr );

    /* SlotNum(UShort) + CreateCTS(UChar) + LimitCTS(UChar) */
    /* sdnbBTree_logAndMakeUnchainedKeys_malloc_UnchainedKey.tc */
    IDU_FIT_POINT("sdnbBTree::logAndMakeUnchainedKeys::malloc::UnchainedKey");
    IDE_TEST( iduMemMgr::malloc( IDU_MEM_SM_SDN,
                                 ID_SIZEOF(UChar) * 4 * sSlotCount,
                                 (void **)&sUnchainedKey,
                                 IDU_MEM_IMMEDIATE )
            != IDE_SUCCESS );
    sState = 1;

    IDE_TEST( makeUnchainedKeys( aStatistics,
                                 aPage,
                                 aCTS,
                                 aCTSlotNum,
                                 aChainedKeyList,
                                 aChainedKeySize,
                                 aContext,
                                 sUnchainedKey,
                                 &sUnchainedKeySize,
                                 aUnchainedKeyCount )
              != IDE_SUCCESS );
    sState = 1;

    IDE_TEST( writeUnchainedKeysLog( aMtx,
                                     aPage,
                                     *aUnchainedKeyCount,
                                     sUnchainedKey,
                                     sUnchainedKeySize )
              != IDE_SUCCESS );

    sState = 0;
    IDE_TEST( iduMemMgr::free(sUnchainedKey) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if ( sState == 1 )
    {
        (void)iduMemMgr::free(sUnchainedKey);
    }

    return IDE_FAILURE;
}
#endif
#if 0
/*********************************************************************
 * FUNCTION DESCRIPTION :                                            *
 * ------------------------------------------------------------------*
 * Chained Key�� Unchained Key(Normal Key)�� ���濡 ���� �α׸�      *
 * �����.                                                           *
 *********************************************************************/
IDE_RC sdnbBTree::writeUnchainedKeysLog( sdrMtx        * aMtx,
                                         sdpPhyPageHdr * aPage,
                                         UShort          aUnchainedKeyCount,
                                         UChar         * aUnchainedKey,
                                         UInt            aUnchainedKeySize )
{
    IDE_TEST( sdrMiniTrans::writeLogRec( aMtx,
                                         (UChar *)aPage,
                                         NULL,
                                         ID_SIZEOF(UShort) + aUnchainedKeySize,
                                         SDR_SDN_MAKE_UNCHAINED_KEYS )
              != IDE_SUCCESS );

    IDE_TEST( sdrMiniTrans::write( aMtx,
                                   (void *)&aUnchainedKeyCount,
                                   ID_SIZEOF(UShort) )
              != IDE_SUCCESS );

    IDE_TEST( sdrMiniTrans::write( aMtx,
                                   (void *)aUnchainedKey,
                                   aUnchainedKeySize )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}
#endif
#if 0
/*********************************************************************
 * FUNCTION DESCRIPTION :                                            *
 * ------------------------------------------------------------------*
 * Chained Key�� Unchained Key(Normal Key)�� �����Ѵ�.               *
 *********************************************************************/
IDE_RC sdnbBTree::makeUnchainedKeys( idvSQL        * aStatistics,
                                     sdpPhyPageHdr * aPage,
                                     sdnCTS        * aCTS,
                                     UChar           aCTSlotNum,
                                     UChar         * aChainedKeyList,
                                     UShort          aChainedKeySize,
                                     UChar         * aContext,
                                     UChar         * aUnchainedKey,
                                     UInt          * aUnchainedKeySize,
                                     UShort        * aUnchainedKeyCount )
{
    UShort                sSlotCount;
    scOffset              sSlotOffset = 0;
    sdnbLKey            * sLeafKey;
    UShort                i;
    UChar                 sChainedCCTS;
    UChar                 sChainedLCTS;
    UChar               * sSlotDirPtr;
    UChar               * sUnchainedKey;
    UShort                sUnchainedKeyCount = 0;
    UInt                  sCursor = 0;
    sdnbCallbackContext * sContext;

    IDE_ASSERT( aContext      != NULL );
    IDE_ASSERT( aUnchainedKey != NULL );

    *aUnchainedKeyCount = 0;
    *aUnchainedKeySize  = 0;

    sSlotDirPtr   = sdpPhyPage::getSlotDirStartPtr( (UChar *)aPage );
    sSlotCount    = sdpSlotDirectory::getCount( sSlotDirPtr );
    sUnchainedKey = aUnchainedKey;

    for ( i = 0 ; i < sSlotCount ; i++ )
    {
        IDE_TEST(sdpSlotDirectory::getPagePtrFromSlotNum( sSlotDirPtr,
                                                          i,
                                                          (UChar **)&sLeafKey )
                 != IDE_SUCCESS );

        if ( ( SDNB_GET_STATE( sLeafKey ) == SDNB_KEY_DEAD ) ||
             ( SDNB_GET_STATE( sLeafKey ) == SDNB_KEY_STABLE ) )
        {
            continue;
        }
        else
        {
            /* nothing to do */
        } 

        sChainedCCTS = SDN_CHAINED_NO;
        sChainedLCTS = SDN_CHAINED_NO;

        sContext = (sdnbCallbackContext*)aContext;
        sContext->mLeafKey  = sLeafKey;

        /*
         * Chained Key���
         */
        if ( ( SDNB_GET_CCTS_NO( sLeafKey ) == aCTSlotNum ) &&
             ( SDNB_GET_CHAINED_CCTS( sLeafKey ) == SDN_CHAINED_YES ) )
        {
            /*
             * Chain�� ��ÿ��� �־�����, Chaind Key�� �ش� ����������
             * �������� �������� �ִ�.
             * 1. SMO�� ���ؼ� Chaind Key�� �̵��� ���
             * 2. Chaind Key�� DEAD���� �϶�
             *    (LIMIT CTS�� Soft Key Stamping�� �� ���)
             */
            if ( findChainedKey( aStatistics,
                                 aCTS,
                                 aChainedKeyList,
                                 aChainedKeySize,
                                 &sChainedCCTS,
                                 &sChainedLCTS,
                                 (UChar *)sContext ) == ID_TRUE )
            {
                idlOS::memcpy( sUnchainedKey + sCursor, 
                               &i, 
                               ID_SIZEOF(UShort) );
                sCursor += 2;
                *(sUnchainedKey + sCursor) = sChainedCCTS;
                sCursor += 1;
                *(sUnchainedKey + sCursor) = sChainedLCTS;
                sCursor += 1;

                if ( sChainedCCTS == SDN_CHAINED_YES )
                {
                    sUnchainedKeyCount++;
                    SDNB_SET_CHAINED_CCTS( sLeafKey, SDN_CHAINED_NO );
                    IDE_TEST( sdpSlotDirectory::getValue( sSlotDirPtr, 
                                                          i,
                                                          &sSlotOffset )
                             != IDE_SUCCESS );

                    sdnIndexCTL::addRefKey( aPage,
                                            aCTSlotNum,
                                            sSlotOffset );
                }
                else
                {
                    /* nothing to do */
                } 

                if ( sChainedLCTS == SDN_CHAINED_YES )
                {
                    sUnchainedKeyCount++;
                    SDNB_SET_CHAINED_LCTS( sLeafKey, SDN_CHAINED_NO );
                    IDE_TEST( sdpSlotDirectory::getValue( sSlotDirPtr, 
                                                          i,
                                                          &sSlotOffset ) 
                              != IDE_SUCCESS );
                    sdnIndexCTL::addRefKey( aPage,
                                            aCTSlotNum,
                                            sSlotOffset );
                }
                else
                {
                    /* nothing to do */
                } 
            }
            else
            {
                if ( sdnIndexCTL::hasChainedCTS( aPage, aCTSlotNum ) != ID_TRUE )
                {
                    ideLog::log( IDE_ERR_0,
                                 "CTS slot number : %"ID_UINT32_FMT""
                                 ", Key slot number : %"ID_UINT32_FMT"\n",
                                 aCTSlotNum, i );

                    dumpIndexNode( aPage );
                    IDE_ASSERT( 0 );
                }
                else
                {
                    /* nothing to do */
                } 
            }
        }
        else
        {
            if ( ( SDNB_GET_LCTS_NO( sLeafKey ) == aCTSlotNum ) &&
                 ( SDNB_GET_CHAINED_LCTS( sLeafKey ) == SDN_CHAINED_YES ) )
            {
                if ( findChainedKey( aStatistics,
                                     aCTS,
                                     aChainedKeyList,
                                     aChainedKeySize,
                                     &sChainedCCTS,
                                     &sChainedLCTS,
                                     (UChar *)sContext ) == ID_TRUE )
                {
                    idlOS::memcpy( sUnchainedKey + sCursor, 
                                   &i, 
                                   ID_SIZEOF(UShort) );
                    sCursor += 2;
                    *(sUnchainedKey + sCursor) = SDN_CHAINED_NO;
                    sCursor += 1;
                    *(sUnchainedKey + sCursor) = sChainedLCTS;
                    sCursor += 1;

                    if ( sChainedLCTS == SDN_CHAINED_YES )
                    {
                        sUnchainedKeyCount++;
                        SDNB_SET_CHAINED_LCTS( sLeafKey, SDN_CHAINED_NO );
                        IDE_TEST( sdpSlotDirectory::getValue( sSlotDirPtr,
                                                              i,
                                                              &sSlotOffset )
                                  != IDE_SUCCESS );
                        sdnIndexCTL::addRefKey( aPage,
                                                aCTSlotNum,
                                                sSlotOffset );
                    }
                    else
                    {
                        /* nothing to do */
                    } 
                }
                else
                {
                    if ( sdnIndexCTL::hasChainedCTS( aPage, aCTSlotNum ) != ID_TRUE )
                    {
                        ideLog::log( IDE_ERR_0,
                                     "CTS slot number : %"ID_UINT32_FMT""
                                     ", Key slot number : %"ID_UINT32_FMT"\n",
                                     aCTSlotNum, i );
                        dumpIndexNode( aPage );
                        IDE_ASSERT( 0 );
                    }
                    else
                    {
                        /* nothing to do */
                    } 
                }
            }
        }
    }

    *aUnchainedKeyCount = sUnchainedKeyCount;
    *aUnchainedKeySize  = sCursor;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}
#endif
#if 0
/*********************************************************************
 * FUNCTION DESCRIPTION :                                            *
 * ------------------------------------------------------------------*
 * �־��� Leaf Key�� Chained Key List�� �����ϴ��� �˻��Ѵ�.       *
 * [ROW_PID, ROW_SLOTNUM, KEY_VALUE]�� ������Ű�� Key�� ã�´�.      *
 *********************************************************************/
idBool sdnbBTree::findChainedKey( idvSQL * /* aStatistics */,
                                  sdnCTS * /*sCTS*/,
                                  UChar *  aChainedKeyList,
                                  UShort   aChainedKeySize,
                                  UChar *  aChainedCCTS,
                                  UChar *  aChainedLCTS,
                                  UChar *  aContext )
{
    sdnbCallbackContext * sContext;
    sdnbKeyInfo           sKey1Info;
    sdnbKeyInfo           sKey2Info;
    SInt                  sResult;
    idBool                sIsSameValue;
    sdnbHeader          * sIndex;
    UChar               * sCurKey;
    UShort                sKeySize;
    sdnbLKey            * sLeafKey;
    UShort                sCurKeyPos;

    IDE_DASSERT( aContext != NULL );

    sContext  = (sdnbCallbackContext*)aContext;

    IDE_DASSERT( sContext->mIndex != NULL );
    IDE_DASSERT( sContext->mLeafKey  != NULL );
    IDE_DASSERT( sContext->mStatistics != NULL );

    sIndex    = sContext->mIndex;

    SDNB_LKEY_TO_KEYINFO( sContext->mLeafKey, sKey1Info );

    sCurKey = aChainedKeyList;

    for ( sCurKeyPos = 0 ; sCurKeyPos < aChainedKeySize ; )
    {
        sLeafKey = (sdnbLKey*)sCurKey;

        sKeySize = getKeyLength( &(sIndex->mColLenInfoList),
                                 (UChar *)sCurKey,
                                 ID_TRUE );

        IDE_DASSERT( sKeySize < SDNB_MAX_KEY_BUFFER_SIZE );

        SDNB_LKEY_TO_KEYINFO( sLeafKey, sKey2Info );

        if ( ( sKey1Info.mRowPID == sKey2Info.mRowPID ) &&
             ( sKey1Info.mRowSlotNum == sKey2Info.mRowSlotNum ) )
        {
            sResult = compareKeyAndKey( sContext->mStatistics,
                                        sIndex->mColumns,
                                        sIndex->mColumnFence,
                                        &sKey1Info,
                                        &sKey2Info,
                                        SDNB_COMPARE_VALUE,
                                        &sIsSameValue );

            if ( sResult == 0 )
            {
                if ( aChainedCCTS != NULL )
                {
                    *aChainedCCTS = SDNB_GET_CHAINED_CCTS( sLeafKey );
                }
                else
                {
                    /* nothing to do */
                } 
                if ( aChainedLCTS != NULL )
                {
                    *aChainedLCTS = SDNB_GET_CHAINED_LCTS( sLeafKey );
                }
                else
                {
                    /* nothing to do */
                } 

                return ID_TRUE;
            }
            else
            {
                /* nothing to do */
            } 
        }
        else
        {
            /* nothing to do */
        } 

        sCurKey += sKeySize;
        sCurKeyPos += sKeySize;
    }

    return ID_FALSE;
}
#endif
/*********************************************************************
 * FUNCTION DESCRIPTION : sdnbBTree::backupRuntimeHeader             *
 * ------------------------------------------------------------------*
 * MtxRollback���� ���� RuntimeHeader ������ ����, ������ ����صд�.
 *
 * aMtx      - [In] ��� Mtx
 * aIndex    - [In] ����� RuntimeHeader
 *********************************************************************/
IDE_RC sdnbBTree::backupRuntimeHeader( sdrMtx     * aMtx,
                                       sdnbHeader * aIndex )
{
    /* Mtx�� Abort�Ǹ�, PageImage�� Rollback���� RuntimeValud��
     * �������� �ʽ��ϴ�.
     * ���� Rollback�� ���� ������ �����ϵ��� �մϴ�.
     * ������ ��� Page�� XLatch�� ��� ������ ���ÿ� �� Mtx��
     * �����մϴ�. ���� ������� �ϳ��� ������ �˴ϴ�.*/
    IDE_TEST( sdrMiniTrans::addPendingJob( aMtx,
                                           ID_FALSE, // isCommitJob
                                           sdnbBTree::restoreRuntimeHeader,
                                           (void *)aIndex )
              != IDE_SUCCESS );

    aIndex->mRootNode4MtxRollback      = aIndex->mRootNode;
    aIndex->mEmptyNodeHead4MtxRollback = aIndex->mEmptyNodeHead;
    aIndex->mEmptyNodeTail4MtxRollback = aIndex->mEmptyNodeTail;
    aIndex->mMinNode4MtxRollback       = aIndex->mMinNode;
    aIndex->mMaxNode4MtxRollback       = aIndex->mMaxNode;
    aIndex->mFreeNodeCnt4MtxRollback   = aIndex->mFreeNodeCnt;
    aIndex->mFreeNodeHead4MtxRollback  = aIndex->mFreeNodeHead;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : sdnbBTree::restoreRuntimeHeader            *
 * ------------------------------------------------------------------*
 * MtxRollback���� ���� RuntimeHeader�� Meta������ ������
 *
 * aMtx      - [In] ��� Mtx
 * aIndex    - [In] ����� RuntimeHeader
 *********************************************************************/
IDE_RC sdnbBTree::restoreRuntimeHeader( void * aIndex )
{
    sdnbHeader  * sIndex;

    IDE_ASSERT( aIndex != NULL );

    sIndex = (sdnbHeader*) aIndex;

    sIndex->mRootNode      = sIndex->mRootNode4MtxRollback;
    sIndex->mEmptyNodeHead = sIndex->mEmptyNodeHead4MtxRollback;
    sIndex->mEmptyNodeTail = sIndex->mEmptyNodeTail4MtxRollback;
    sIndex->mMinNode       = sIndex->mMinNode4MtxRollback;
    sIndex->mMaxNode       = sIndex->mMaxNode4MtxRollback;
    sIndex->mFreeNodeCnt   = sIndex->mFreeNodeCnt4MtxRollback;
    sIndex->mFreeNodeHead  = sIndex->mFreeNodeHead4MtxRollback;

    return IDE_SUCCESS;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : sdnbBTree::setIndexMetaInfo                *
 * ------------------------------------------------------------------*
 * index segment�� ù��° �������� ����� ��������� ������ �α��Ѵ�.*
 *********************************************************************/
IDE_RC sdnbBTree::setIndexMetaInfo( idvSQL        * aStatistics,
                                    sdnbHeader    * aIndex,
                                    sdnbStatistic * aIndexStat,
                                    sdrMtx        * aMtx,
                                    scPageID      * aRootPID,
                                    scPageID      * aMinPID,
                                    scPageID      * aMaxPID,
                                    scPageID      * aFreeNodeHead,
                                    ULong         * aFreeNodeCnt,
                                    idBool        * aIsCreatedWithLogging,
                                    idBool        * aIsConsistent,
                                    idBool        * aIsCreatedWithForce,
                                    smLSN         * aNologgingCompletionLSN )
{
    UChar    * sPage;
    sdnbMeta * sMeta;
    idBool     sIsSuccess;

    IDE_DASSERT( ( aRootPID != NULL ) || 
                 ( aMinPID  != NULL ) ||
                 ( aMaxPID  != NULL ) || 
                 ( aFreeNodeHead           != NULL ) ||
                 ( aFreeNodeCnt            != NULL ) ||
                 ( aIsConsistent           != NULL ) ||
                 ( aIsCreatedWithLogging   != NULL ) ||
                 ( aIsCreatedWithForce     != NULL ) ||
                 ( aNologgingCompletionLSN != NULL ) );

    sPage = sdrMiniTrans::getPagePtrFromPageID( aMtx,
                                                aIndex->mIndexTSID,
                                                SD_MAKE_PID( aIndex->mMetaRID ));
    if ( sPage == NULL )
    {
        // SegHdr ������ �����͸� ����
        IDE_TEST( sdnbBTree::getPage(
                                 aStatistics,
                                 &(aIndexStat->mMetaPage),
                                 aIndex->mIndexTSID,
                                 SD_MAKE_PID( aIndex->mMetaRID ),
                                 SDB_X_LATCH,
                                 SDB_WAIT_NORMAL,
                                 aMtx,
                                 (UChar **)&sPage,
                                 &sIsSuccess ) != IDE_SUCCESS );
    }
    else
    {
        /* nothing to do */
    } 

    sMeta = (sdnbMeta*)( sPage + SMN_INDEX_META_OFFSET );

    if ( aRootPID != NULL )
    {
        IDE_TEST( sdrMiniTrans::writeNBytes( aMtx,
                                             (UChar *)&sMeta->mRootNode,
                                             (void *)aRootPID,
                                             ID_SIZEOF(*aRootPID) )
                  != IDE_SUCCESS );
    }
    else
    {
        /* nothing to do */
    } 

    if ( aMinPID != NULL )
    {
        IDE_TEST( sdrMiniTrans::writeNBytes( aMtx,
                                             (UChar *)&sMeta->mMinNode,
                                             (void *)aMinPID,
                                             ID_SIZEOF(*aMinPID) )
                  != IDE_SUCCESS );
    }
    else
    {
        /* nothing to do */
    } 

    if ( aMaxPID != NULL )
    {
        IDE_TEST( sdrMiniTrans::writeNBytes( aMtx,
                                             (UChar *)&sMeta->mMaxNode,
                                             (void *)aMaxPID,
                                             ID_SIZEOF(*aMaxPID) )
                  != IDE_SUCCESS );
    }
    else
    {
        /* nothing to do */
    } 

    if ( aFreeNodeHead != NULL )
    {
        IDE_TEST( sdrMiniTrans::writeNBytes( aMtx,
                                             (UChar *)&sMeta->mFreeNodeHead,
                                             (void *)aFreeNodeHead,
                                             ID_SIZEOF(*aFreeNodeHead) )
                  != IDE_SUCCESS );
    }
    else
    {
        /* nothing to do */
    } 

    if ( aFreeNodeCnt != NULL )
    {
        IDE_TEST( sdrMiniTrans::writeNBytes( aMtx,
                                             (UChar *)&sMeta->mFreeNodeCnt,
                                             (void *)aFreeNodeCnt,
                                             ID_SIZEOF(*aFreeNodeCnt) )
                  != IDE_SUCCESS );
    }
    else
    {
        /* nothing to do */
    } 

    if ( aIsConsistent != NULL )
    {
        IDE_TEST( sdrMiniTrans::writeNBytes( aMtx,
                                             (UChar *)&sMeta->mIsConsistent,
                                             (void *)aIsConsistent,
                                             ID_SIZEOF(*aIsConsistent) )
                  != IDE_SUCCESS );
    }
    else
    {
        /* nothing to do */
    } 

    if ( aIsCreatedWithLogging != NULL )
    {
        IDE_TEST( sdrMiniTrans::writeNBytes( aMtx,
                                             (UChar *)&sMeta->mIsCreatedWithLogging,
                                             (void *)aIsCreatedWithLogging,
                                             ID_SIZEOF(*aIsCreatedWithLogging) )
                  != IDE_SUCCESS );
    }
    else
    {
        /* nothing to do */
    } 

    if ( aIsCreatedWithForce != NULL )
    {
        IDE_TEST( sdrMiniTrans::writeNBytes( aMtx,
                                             (UChar *)&sMeta->mIsCreatedWithForce,
                                             (void *)aIsCreatedWithForce,
                                             ID_SIZEOF(*aIsCreatedWithForce) )
                  != IDE_SUCCESS );
    }
    else
    {
        /* nothing to do */
    } 

    if ( aNologgingCompletionLSN != NULL )
    {
        IDE_TEST( sdrMiniTrans::writeNBytes(
                      aMtx,
                      (UChar *)&(sMeta->mNologgingCompletionLSN.mFileNo),
                      (void *)&(aNologgingCompletionLSN->mFileNo),
                      ID_SIZEOF(aNologgingCompletionLSN->mFileNo) )
                  != IDE_SUCCESS );
        IDE_TEST( sdrMiniTrans::writeNBytes(
                      aMtx,
                      (UChar *)&(sMeta->mNologgingCompletionLSN.mOffset),
                      (void *)&(aNologgingCompletionLSN->mOffset),
                      ID_SIZEOF(aNologgingCompletionLSN->mOffset) )
                  != IDE_SUCCESS );
    }
    else
    {
        /* nothing to do */
    } 

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}


/*********************************************************************
 * FUNCTION DESCRIPTION : sdnbBTree::setIndexEmptyNodeInfo           *
 * ------------------------------------------------------------------*
 * index segment�� ù��° �������� ����� Free Node List�� ������    *
 * �α��Ѵ�.                                                         *
 *********************************************************************/
IDE_RC sdnbBTree::setIndexEmptyNodeInfo( idvSQL          * aStatistics,
                                         sdnbHeader      * aIndex,
                                         sdnbStatistic   * aIndexStat,
                                         sdrMtx          * aMtx,
                                         scPageID        * aEmptyNodeHead,
                                         scPageID        * aEmptyNodeTail )
{

    UChar    * sPage;
    sdnbMeta * sMeta;
    idBool     sIsSuccess;

    sPage = sdrMiniTrans::getPagePtrFromPageID( aMtx,
                                                aIndex->mIndexTSID,
                                                SD_MAKE_PID( aIndex->mMetaRID ));
    if ( sPage == NULL )
    {
        // SegHdr ������ �����͸� ����
        IDE_TEST( sdnbBTree::getPage(
                                 aStatistics,
                                 &(aIndexStat->mMetaPage),
                                 aIndex->mIndexTSID,
                                 SD_MAKE_PID( aIndex->mMetaRID ),
                                 SDB_X_LATCH,
                                 SDB_WAIT_NORMAL,
                                 aMtx,
                                 (UChar **)&sPage,
                                 &sIsSuccess ) != IDE_SUCCESS );
    }
    else
    {
        /* nothing to do */
    } 

    sMeta = (sdnbMeta*)( sPage + SMN_INDEX_META_OFFSET );

    if ( aEmptyNodeHead != NULL )
    {
        IDE_TEST( sdrMiniTrans::writeNBytes( aMtx,
                                             (UChar *)&sMeta->mEmptyNodeHead,
                                             (void *)aEmptyNodeHead,
                                             ID_SIZEOF(*aEmptyNodeHead) )
                  != IDE_SUCCESS );
    }
    else
    {
        /* nothing to do */
    } 

    if ( aEmptyNodeTail != NULL )
    {
        IDE_TEST( sdrMiniTrans::writeNBytes( aMtx,
                                             (UChar *)&sMeta->mEmptyNodeTail,
                                             (void *)aEmptyNodeTail,
                                             ID_SIZEOF(*aEmptyNodeTail) )
                  != IDE_SUCCESS );
    }
    else
    {
        /* nothing to do */
    } 

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}


/*********************************************************************
 * FUNCTION DESCRIPTION : sdnbBTree::setFreeNodeInfo                 *
 * ------------------------------------------------------------------*
 * To fix BUG-23287                                                  *
 * Free Node ������ Meta �������� �����Ѵ�.                          *
 * 1. Free Node Head�� ����                                          *
 * 2. Free Node Count�� ����                                         *
 *********************************************************************/
IDE_RC sdnbBTree::setFreeNodeInfo( idvSQL         * aStatistics,
                                   sdnbHeader     * aIndex,
                                   sdnbStatistic  * aIndexStat,
                                   sdrMtx         * aMtx,
                                   scPageID         aFreeNodeHead,
                                   ULong            aFreeNodeCnt )
{
    UChar    * sPage;
    sdnbMeta * sMeta;
    idBool     sIsSuccess;

    IDE_DASSERT( aMtx != NULL );

    sPage = sdrMiniTrans::getPagePtrFromPageID( aMtx,
                                                aIndex->mIndexTSID,
                                                SD_MAKE_PID( aIndex->mMetaRID ));
    if ( sPage == NULL )
    {
        // SegHdr ������ �����͸� ����
        IDE_TEST( sdnbBTree::getPage(
                         aStatistics,
                         &(aIndexStat->mMetaPage),
                         aIndex->mIndexTSID,
                         SD_MAKE_PID( aIndex->mMetaRID ),
                         SDB_X_LATCH,
                         SDB_WAIT_NORMAL,
                         aMtx,
                         (UChar **)&sPage,
                         &sIsSuccess ) != IDE_SUCCESS );
    }
    else
    {
        /* nothing to do */
    } 

    sMeta = (sdnbMeta*)( sPage + SMN_INDEX_META_OFFSET );

    IDE_TEST( sdrMiniTrans::writeNBytes( aMtx,
                                         (UChar *)&sMeta->mFreeNodeHead,
                                         (void *)&aFreeNodeHead,
                                         ID_SIZEOF(aFreeNodeHead) )
              != IDE_SUCCESS );

    IDE_TEST( sdrMiniTrans::writeNBytes( aMtx,
                                         (UChar *)&sMeta->mFreeNodeCnt,
                                         (void *)&aFreeNodeCnt,
                                         ID_SIZEOF(aFreeNodeCnt) )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : sdnbBTree::rebuildMinStat                  *
 * ------------------------------------------------------------------*
 * BUG-30639 Disk Index���� Internal Node��                          *
 *           Min/Max Node��� �ν��Ͽ� ����մϴ�.                   *
 *                                                                   *
 * MinValue�� �ƿ� �ٽ� �����Ѵ�.                                    *
 *********************************************************************/
IDE_RC sdnbBTree::rebuildMinStat( idvSQL         * aStatistics,
                                  void           * aTrans,
                                  smnIndexHeader * aPersIndex,
                                  sdnbHeader     * aIndex )
{
    scPageID               sCurPID;
    UChar                * sPage;
    sdnbNodeHdr          * sNodeHdr;
    sdnbStatistic          sDummyStat;
    sdrMtx                 sMtx;
    UInt                   sMtxDLogType;
    idBool                 sTrySuccess = ID_FALSE;
    UInt                   sState = 0;
    smxTrans             * sSmxTrans = (smxTrans *)aTrans;
    UInt                   sStatFlag = SMI_INDEX_BUILD_RT_STAT_UPDATE;

    IDE_DASSERT( aIndex != NULL );

    // BUG-27328 CodeSonar::Uninitialized Variable
    idlOS::memset( &sDummyStat, 0, ID_SIZEOF(sDummyStat) );

    if ( aTrans == NULL )
    {
        sMtxDLogType = SM_DLOG_ATTR_DUMMY;
    }
    else
    {
        sMtxDLogType = gMtxDLogType;
    }

    /* BUG-44794 �ε��� ����� �ε��� ��� ������ �������� �ʴ� ���� ������Ƽ �߰� */
    SMI_INDEX_BUILD_NEED_RT_STAT( sStatFlag, sSmxTrans );

    IDE_TEST( sdrMiniTrans::begin( aStatistics,
                                   &sMtx,
                                   aTrans,
                                   SDR_MTX_LOGGING,
                                   ID_FALSE,/*Undoable(PROJ-2162)*/
                                   sMtxDLogType )
              != IDE_SUCCESS );
    sState = 1;

    /* �ϴ� SC_NULL_PID�� �����Ѵ�. �׷����� adjustMinStat�Լ�����
     * Min ������ �����Ѵ�.
     *  ���� �� �Լ��� ServerRefine�ÿ��� �Ҹ� ���� �����ϰ� �ֱ�
     * ������ Peterson �˰����� ���� ���ü� üũ�� ���� ������
     * Logging���� ���� �ʴ´�. ( ���Ŀ� adjustMinStat�Լ����� �Ѵ� ) */
    aIndex->mMinNode = SC_NULL_PID;
    sCurPID          = aIndex->mRootNode;

    while ( sCurPID != SC_NULL_PID )
    {
        /* BUG-30812 : Rebuild �� Stack Overflow�� �� �� �ֽ��ϴ�. */
        IDE_TEST( sdbBufferMgr::getPageByPID( aStatistics,
                                              aIndex->mIndexTSID,
                                              sCurPID,
                                              SDB_S_LATCH,
                                              SDB_WAIT_NORMAL,
                                              SDB_SINGLE_PAGE_READ,
                                              NULL, /* aMtx */
                                              (UChar **)&sPage,
                                              &sTrySuccess,
                                              NULL /* IsCorruptPage */ )
                  != IDE_SUCCESS );
        sState = 2;

        IDE_ASSERT_MSG( sTrySuccess == ID_TRUE,
                        "Error Ouccurs in DRDB Index ID : %"ID_UINT32_FMT, aPersIndex->mId );

        sNodeHdr = (sdnbNodeHdr*)sdpPhyPage::getLogicalHdrStartPtr( (UChar *)sPage );

        if ( SDNB_IS_LEAF_NODE( sNodeHdr ) == ID_TRUE )
        {
            /* Leaf�� Min��������� �����ɶ����� ���� ��带 Ž���ذ���. */
            IDE_TEST( adjustMinStat( aStatistics,
                                     &sDummyStat,
                                     aPersIndex,
                                     aIndex,
                                     &sMtx,
                                     (sdpPhyPageHdr *)sPage, 
                                     (sdpPhyPageHdr *)sPage,
                                     ID_FALSE, /* aIsLoggingMeta */
                                     sStatFlag ) 
                      != IDE_SUCCESS );

            sState = 1;
            IDE_TEST( sdbBufferMgr::releasePage( aStatistics, sPage ) != IDE_SUCCESS );
            break;
        }
        else
        {
            /* Internal�̸�, LeftMost�� ��������. */
            sCurPID = sNodeHdr->mLeftmostChild;
        }
        sState = 1;
        IDE_TEST( sdbBufferMgr::releasePage( aStatistics, sPage ) != IDE_SUCCESS );
    }

    // logging index meta
    IDE_TEST( setIndexMetaInfo( aStatistics,
                                aIndex,
                                &sDummyStat,
                                &sMtx,
                                NULL,   /* aRootPID */
                                &aIndex->mMinNode,
                                NULL,   /* aMaxPID */
                                NULL,   /* aFreeNodeHead */
                                NULL,   /* aFreeNodeCnt */
                                NULL,   /* aIsCreatedWithLogging */
                                NULL,   /* aIsConsistent */
                                NULL,   /* aIsCreatedWithForce */
                                NULL )  /* aNologgingCompletionLSN */
            != IDE_SUCCESS );

    sState = 0;
    IDE_TEST( sdrMiniTrans::commit( &sMtx ) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    switch( sState )
    {
        case 2:
            (void)sdbBufferMgr::releasePage( aStatistics, sPage );

        case 1:
            (void)sdrMiniTrans::rollback( &sMtx );

        default:
            break;
    }

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : sdnbBTree::rebuildMaxStat                  *
 * ------------------------------------------------------------------*
 * BUG-30639 Disk Index���� Internal Node��                          *
 *           Min/Max Node��� �ν��Ͽ� ����մϴ�.                   *
 *                                                                   *
 * MaxValue�� �ƿ� �ٽ� �����Ѵ�.                                    *
 *********************************************************************/
IDE_RC sdnbBTree::rebuildMaxStat( idvSQL         * aStatistics,
                                  void           * aTrans,
                                  smnIndexHeader * aPersIndex,
                                  sdnbHeader     * aIndex )
{
    sdnbConvertedKeyInfo   sConvertedKeyInfo;
    smiValue               sNullKeyValue[SMI_MAX_IDX_COLUMNS];
    scPageID               sCurPID;
    UChar                * sPage;
    sdnbNodeHdr          * sNodeHdr;
    idBool                 sTrySuccess;
    sdnbStatistic          sDummyStat;
    sdrMtx                 sMtx;
    ULong                  sIndexSmoNo;
    ULong                  sNodeSmoNo;
    scPageID               sChildPID;
    scPageID               sNextChildPID;
    UShort                 sNextSlotSize;
    SShort                 sSlotSeq;
    idBool                 sRetry;
    idBool                 sIsSameValueInSiblingNodes;
    /* Null �Լ��� 8Byte Align�� ���� �������� �Ѵ�. */
    ULong                  sTempNullColumnBuf
        [SMI_MAX_IDX_COLUMNS]
        [ SDNB_MAX_LENGTH_KNOWN_COLUMN_SIZE/ID_SIZEOF(ULong) ];
    sdnbColumn           * sIndexColumn;
    UInt                   sMtxDLogType;
    UInt                   sState = 0;
    UInt                   i;
    smcTableHeader       * sDicTable;
    smxTrans             * sSmxTrans = (smxTrans *)aTrans;
    UInt                   sStatFlag = SMI_INDEX_BUILD_RT_STAT_UPDATE;

    // BUG-27328 CodeSonar::Uninitialized Variable
    idlOS::memset( &sDummyStat, 0, ID_SIZEOF(sDummyStat) );

    IDE_DASSERT( aIndex != NULL );

    /* BUG-32623 When index statistics is rebuilded, the log's size
     * can exceed stack log buffer's size in mini-transaction. */
    if ( aTrans == NULL )
    {
        sMtxDLogType = SM_DLOG_ATTR_DUMMY;
    }
    else
    {
        sMtxDLogType = gMtxDLogType;
    }

    /* BUG-44794 �ε��� ����� �ε��� ��� ������ �������� �ʴ� ���� ������Ƽ �߰� */
    SMI_INDEX_BUILD_NEED_RT_STAT( sStatFlag, sSmxTrans );

    IDE_TEST( sdrMiniTrans::begin( aStatistics,
                                   &sMtx,
                                   aTrans,
                                   SDR_MTX_LOGGING,
                                   ID_FALSE,/*Undoable(PROJ-2162)*/
                                   sMtxDLogType )
              != IDE_SUCCESS );
    sState = 1;

    /* �ϴ� SC_NULL_PID�� �����Ѵ�. �׷����� adjustMaxStat�Լ�����
     * Max ������ �����Ѵ�.
     *  ���� �� �Լ��� ServerRefine�ÿ��� �Ҹ� ���� �����ϰ� �ֱ�
     * ������ Peterson �˰����� ���� ���ü� üũ�� ���� ������
     * Logging���� ���� �ʴ´�. ( ���Ŀ� adjustMaxStat�Լ����� �Ѵ� ) */
    aIndex->mMaxNode = SC_NULL_PID;

    /* NullKey�� �������ش� */
    /* BUG-30639 Disk Index���� Internal Node��
     *           Min/Max Node��� �ν��Ͽ� ����մϴ�.
     * Index�� Length-knwon�϶� ���� NullValue�� ����ϰ�
     *         Length-unknown�϶� length�� 0�� �����Ͽ� Null�� �����Ѵ�.*/
    for ( i = 0 ; i < aIndex->mColLenInfoList.mColumnCount ; i++ )
    {
        sIndexColumn = &aIndex->mColumns[ i ];

        /* PROJ-2429 Dictionary based data compress for on-disk DB */
        if ( ( sIndexColumn->mKeyColumn.flag & SMI_COLUMN_COMPRESSION_MASK ) 
             != SMI_COLUMN_COMPRESSION_TRUE )
        {
            sNullKeyValue[i].length = 0;
 
            sIndexColumn->mNull( &sIndexColumn->mKeyColumn,
                                 sTempNullColumnBuf[i] );
        }
        else
        {
            sNullKeyValue[i].length = ID_SIZEOF(smOID);

            IDE_TEST( smcTable::getTableHeaderFromOID( 
                                    sIndexColumn->mKeyColumn.mDictionaryTableOID,
                                    (void **)&sDicTable ) != IDE_SUCCESS );

            idlOS::memcpy( sTempNullColumnBuf[i], 
                           &sDicTable->mNullOID, 
                           ID_SIZEOF(smOID) );
            
        }

        sNullKeyValue[i].value = sTempNullColumnBuf[i];
    }
    sConvertedKeyInfo.mSmiValueList         = (smiValue*)sNullKeyValue;
    sConvertedKeyInfo.mKeyInfo.mKeyValue    = NULL;
    sConvertedKeyInfo.mKeyInfo.mRowPID      = SC_NULL_PID;
    sConvertedKeyInfo.mKeyInfo.mRowSlotNum  = 0;
    sConvertedKeyInfo.mKeyInfo.mKeyState    = 0;
    
    IDE_EXCEPTION_CONT( RETRY_TRAVERSE );
    sRetry = ID_FALSE;

    getSmoNo( (void *)aIndex, &sIndexSmoNo );
    sCurPID = aIndex->mRootNode;

    while ( sCurPID != SC_NULL_PID )
    {
        /* BUG-30812 : Rebuild �� Stack Overflow�� �� �� �ֽ��ϴ�. */
        IDE_TEST( sdbBufferMgr::getPageByPID( aStatistics,
                                              aIndex->mIndexTSID,
                                              sCurPID,
                                              SDB_S_LATCH,
                                              SDB_WAIT_NORMAL,
                                              SDB_SINGLE_PAGE_READ,
                                              NULL, /* aMtx */
                                              (UChar **)&sPage,
                                              &sTrySuccess,
                                              NULL /* IsCurruptPage */ )
                  != IDE_SUCCESS );
        sState = 2;

        IDE_ASSERT_MSG( sTrySuccess == ID_TRUE,
                        "Error Ouccurs in DRDB Index ID : %"ID_UINT32_FMT, aPersIndex->mId );

        sNodeHdr = (sdnbNodeHdr*)sdpPhyPage::getLogicalHdrStartPtr((UChar *)sPage);

        if ( SDNB_IS_LEAF_NODE( sNodeHdr ) == ID_TRUE )
        {
            /* Leaf�� Max��������� �����ɶ����� ���� ��带 Ž���ذ���. */
            IDE_TEST( adjustMaxStat( aStatistics,
                                     &sDummyStat,
                                     aPersIndex,
                                     aIndex,
                                     &sMtx,
                                     (sdpPhyPageHdr *)sPage, 
                                     (sdpPhyPageHdr *)sPage,
                                     ID_FALSE, /* aIsLoggingMeta */
                                     sStatFlag ) 
                      != IDE_SUCCESS );

            /* Max���� �����Ǹ� Ż���Ѵ� */
            if ( aIndex->mMaxNode == sCurPID )
            {
                sCurPID = SC_NULL_PID;
                sState = 1;
                IDE_TEST( sdbBufferMgr::releasePage( aStatistics, sPage ) != IDE_SUCCESS );
                break;
            }
            else
            {
                /* nothing to do */
            } 

            /* �̹��� ��������, ���� ���������� ã�´�. */
            sCurPID = sdpPhyPage::getPrvPIDOfDblList( (sdpPhyPageHdr*)sPage );
        }
        else
        {
            /* Internal�̸�, NullKey�������� ã�� ��������. */
            IDE_ERROR( findInternalKey( aIndex,
                                        (sdpPhyPageHdr*)sPage,
                                        &sDummyStat,
                                        aIndex->mColumns,
                                        aIndex->mColumnFence,
                                        &sConvertedKeyInfo,
                                        sIndexSmoNo,
                                        &sNodeSmoNo,
                                        &sChildPID,
                                        &sSlotSeq,
                                        &sRetry,
                                        &sIsSameValueInSiblingNodes,
                                        &sNextChildPID,
                                        &sNextSlotSize )
                        == IDE_SUCCESS );

            sCurPID = sChildPID;
        }
        sState = 1;
        IDE_TEST( sdbBufferMgr::releasePage( aStatistics, sPage ) != IDE_SUCCESS );

        IDE_TEST_CONT( sRetry == ID_TRUE, RETRY_TRAVERSE );

    }

    // logging index meta page
    IDE_TEST( setIndexMetaInfo( aStatistics,
                                aIndex,
                                &sDummyStat,
                                &sMtx,
                                NULL,   /* aRootPID */
                                NULL,   /* aMinPID */
                                &aIndex->mMaxNode,
                                NULL,   /* aFreeNodeHead */
                                NULL,   /* aFreeNodeCnt */
                                NULL,   /* aIsCreatedWithLogging */
                                NULL,   /* aIsConsistent */
                                NULL,   /* aIsCreatedWithForce */
                                NULL )  /* aNologgingCompletionLSN */
            != IDE_SUCCESS );

    sState = 0;
    IDE_TEST( sdrMiniTrans::commit( &sMtx ) != IDE_SUCCESS );
   
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    switch( sState )
    {
        case 2:
            (void)sdbBufferMgr::releasePage( aStatistics, sPage );

        case 1:
            (void)sdrMiniTrans::rollback( &sMtx );

        default:
            break;
    }

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : sdnbBTree::adjustMaxStat                   *
 * ------------------------------------------------------------------*
 * MaxValue�� �����Ѵ�.                                              *
 * Non-Null Value���� MaxValue�� �ɼ� �ְ�, Null Value�� ������      *
 * �������� ������ Slot�̴�.                                         *
 * freeNode �ÿ��� aOrgNode, aNewNode�� �ٸ���.                      *
 *********************************************************************/
IDE_RC sdnbBTree::adjustMaxStat( idvSQL         * aStatistics,
                                 sdnbStatistic  * aIndexStat,
                                 smnIndexHeader * aPersIndex,
                                 sdnbHeader     * aIndex,
                                 sdrMtx         * aMtx,
                                 sdpPhyPageHdr  * aOrgNode,
                                 sdpPhyPageHdr  * aNewNode,
                                 idBool           aIsLoggingMeta,
                                 UInt             aStatFlag )
{
    sdnbColumn       * sColumn;
    scPageID           sLeafPID;
    scPageID           sOldMaxNode;
    scPageID           sNewMaxNode;
    sdnbLKey         * sKey;
    SInt               sMaxSeq;
    SInt               i;
    UShort             sSlotCount;
    UChar            * sSlotDirPtr;
    sdnbNodeHdr      * sNodeHdr;
    UChar              sMinMaxValue[ MAX_MINMAX_VALUE_SIZE ];

    IDE_ASSERT( aIndex   != NULL );
    IDE_ASSERT( aOrgNode != NULL );

    sLeafPID    = sdpPhyPage::getPageID( (UChar *)aOrgNode );
    sOldMaxNode = aIndex->mMaxNode;

    /* ������ ��� max stat�� �����Ѵ�.
     * - ���� leaf node�� max node �� ���
     * - max node�� ���� �����Ǿ� ���� ���� ��� */
    IDE_TEST_CONT( (sLeafPID    != aIndex->mMaxNode ) &&
                   (SD_NULL_PID != aIndex->mMaxNode ),
                    SKIP_ADJUST_MAX_STAT );

    /* BUG-30639 Disk Index���� Internal Node��aOrgNode
     *           Min/Max Node��� �ν��Ͽ� ����մϴ�.
     * Insert/Delete ���� ����� �� Node�� aOrgNode�� ���� ���̱�
     * ������ Internal�� �� �����ϴ�.
     * 
     * Index�� MaxNode�� Internal�� Ȯ���� ������, �̸� �������� �ʽ��ϴ�.
     * Internal���� Ȯ���Ϸ���, Max/Min Node�� getPage�ؾ� �ϱ� ������
     * �߰����� ����� ��� �����Դϴ�.
     * ���� ���� ����� server restart ���Դϴ�. */
    sNodeHdr = (sdnbNodeHdr*)sdpPhyPage::getLogicalHdrStartPtr( (UChar *)aOrgNode ) ;

    if ( SDNB_IS_LEAF_NODE( sNodeHdr ) == ID_FALSE )
    {
        dumpHeadersAndIteratorToSMTrc( NULL,
                                       aIndex,
                                       NULL );
        dumpIndexNode( (sdpPhyPageHdr *)aOrgNode );
        IDE_ASSERT( 0 );
    }

    /*
     * get new max stat(max node, max value)
     */
    if ( aNewNode != NULL )
    {
        sColumn = &(aIndex->mColumns[0]);

        sSlotDirPtr = sdpPhyPage::getSlotDirStartPtr( (UChar *)aNewNode );
        IDE_TEST( sdpSlotDirectory::getPagePtrFromSlotNum( sSlotDirPtr,
                                                           0,
                                                           (UChar **)&sKey )
                 != IDE_SUCCESS );

        if ( isIgnoredKey4MinMaxStat( sKey, sColumn ) == ID_TRUE )
        {
            // ù��° Ű�� null �̸� ����� ��� Ű�� null �̴�.
            sMaxSeq = -1;
            sNewMaxNode = sdpPhyPage::getPrvPIDOfDblList((sdpPhyPageHdr*)aNewNode);
        }
        else
        {
            sMaxSeq = 0;
            sSlotCount  = sdpSlotDirectory::getCount( sSlotDirPtr );

            for ( i = sSlotCount - 1 ; i >= 0 ; i-- )
            {
                IDE_TEST( sdpSlotDirectory::getPagePtrFromSlotNum( sSlotDirPtr,
                                                                   i,
                                                                   (UChar **)&sKey )
                          != IDE_SUCCESS );

                /* BUG-30605: Deleted/Dead�� Ű�� Min/Max�� ���Խ�ŵ�ϴ� */
                if ( isIgnoredKey4MinMaxStat(sKey, sColumn) != ID_TRUE )
                {
                    sMaxSeq = i;
                    break;
                }
                else
                {
                    /* nothing to do */
                } 
            }

            IDE_ASSERT_MSG( i >= 0, "Error Ouccurs in DRDB Index ID : %"ID_UINT32_FMT, aPersIndex->mId );

            sNewMaxNode = sdpPhyPage::getPageID((UChar *)aNewNode);
        }
    }
    else
    {
        sMaxSeq     = -1;
        sNewMaxNode = SD_NULL_PID;
    }

    /*
     * set new max stat
     */
    if ( sNewMaxNode == SD_NULL_PID )
    {
        idlOS::memset( sMinMaxValue,
                       0x00,
                       ID_SIZEOF(sMinMaxValue) );
        IDE_TEST( smLayerCallback::setIndexMaxValue( aPersIndex,
                                                     sdrMiniTrans::getTrans( aMtx ),
                                                     sMinMaxValue,
                                                     aStatFlag )
                  != IDE_SUCCESS );
    }
    else
    {
        if ( sMaxSeq >= 0 )
        {
            IDE_ERROR( setMinMaxValue( aIndex,
                                       (UChar *)aNewNode,
                                       sMaxSeq,
                                       sMinMaxValue )        
                       == IDE_SUCCESS );
            IDE_TEST( smLayerCallback::setIndexMaxValue( aPersIndex,
                                                         sdrMiniTrans::getTrans( aMtx ),
                                                         sMinMaxValue,
                                                         aStatFlag )
                      != IDE_SUCCESS );
        }
        else
        {
            /* nothing to do */
        } 
    }
    aIndex->mMaxNode = sNewMaxNode;


    /* BUG-32623 When index statistics is rebuilded, the log's size
     * can exceed stack log buffer's size in mini-transaction. */
    
    // max node�� ���ŵɶ��� logging �Ѵ�.
    if ( ( aIsLoggingMeta == ID_TRUE ) && ( aIndex->mMaxNode != sOldMaxNode ) )
    {
       IDE_TEST( setIndexMetaInfo( aStatistics,
                                    aIndex,
                                    aIndexStat,
                                    aMtx,
                                    NULL,   /* aRootPID */
                                    NULL,   /* aMinPID */
                                    &aIndex->mMaxNode,
                                    NULL,   /* aFreeNodeHead */
                                    NULL,   /* aFreeNodeCnt */
                                    NULL,   /* aIsCreatedWithLogging */
                                    NULL,   /* aIsConsistent */
                                    NULL,   /* aIsCreatedWithForce */
                                    NULL )  /* aNologgingCompletionLSN */
                != IDE_SUCCESS );
    }
    else
    {
        // nothing to do
    }

    IDE_EXCEPTION_CONT( SKIP_ADJUST_MAX_STAT );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : sdnbBTree::adjustMaxNode                   *
 * ------------------------------------------------------------------*
 * Split���� Max Node���� �����Ѵ�.                                  *
 *********************************************************************/
IDE_RC sdnbBTree::adjustMaxNode( idvSQL        * aStatistics,
                                 sdnbStatistic * aIndexStat,
                                 sdnbHeader    * aIndex,
                                 sdrMtx        * aMtx,
                                 sdnbKeyInfo   * aRowInfo,
                                 sdpPhyPageHdr * aOrgNode,
                                 sdpPhyPageHdr * aNewNode,
                                 sdpPhyPageHdr * aTargetNode,
                                 SShort          aTargetSlotSeq )
{
    sdnbColumn * sColumn;
    scPageID     sLeafPID;
    UChar      * sFstKeyOfNewNode;
    UChar      * sFstKeyValueOfNewNode;
    UChar      * sSlotDirPtr;
    UShort       sSlotCount;

    IDE_DASSERT( aOrgNode != NULL );
    IDE_DASSERT( aNewNode != NULL );

    sLeafPID = sdpPhyPage::getPageID((UChar *)aOrgNode);
    sColumn = &aIndex->mColumns[0];

    IDE_TEST_CONT( ( sLeafPID != aIndex->mMaxNode ) &&
                   ( SD_NULL_PID != aIndex->mMaxNode ), SKIP_ADJUST_MAX_STAT );

    /* BUG-32644: When the leaf node containing null key is split or
     * redistributed, the max node isn't adjusted correctly. */
    if ( ( aTargetNode == aNewNode ) && ( aTargetSlotSeq == 0 ) )
    {
        sFstKeyValueOfNewNode = aRowInfo->mKeyValue;
    }
    else
    {
        sSlotDirPtr = sdpPhyPage::getSlotDirStartPtr( (UChar *)aNewNode );
        sSlotCount  = sdpSlotDirectory::getCount( sSlotDirPtr );

        IDE_ASSERT_MSG( sSlotCount > 0, "Error Ouccurs in DRDB Index ID : %"ID_UINT32_FMT, aIndex->mIndexID );

        IDE_TEST( sdpSlotDirectory::getPagePtrFromSlotNum( sSlotDirPtr, 
                                                           0,
                                                           &sFstKeyOfNewNode )
                  != IDE_SUCCESS );

        sFstKeyValueOfNewNode = SDNB_LKEY_KEY_PTR(sFstKeyOfNewNode);
    }

    IDE_TEST_CONT( isNullColumn( sColumn, sFstKeyValueOfNewNode ) == ID_TRUE,
                   SKIP_ADJUST_MAX_STAT );

    IDE_TEST( backupRuntimeHeader( aMtx, aIndex ) != IDE_SUCCESS );

    aIndex->mMaxNode = sdpPhyPage::getPageID((UChar *)aNewNode);

    IDE_TEST( setIndexMetaInfo( aStatistics,
                                aIndex,
                                aIndexStat,
                                aMtx,
                                NULL,   /* aRootPID */
                                NULL,   /* aMinPID */
                                &aIndex->mMaxNode,
                                NULL,   /* aFreeNodeHead */
                                NULL,   /* aFreeNodeCnt */
                                NULL,   /* aIsCreatedWithLogging */
                                NULL,   /* aIsConsistent */
                                NULL,   /* aIsCreatedWithForce */
                                NULL )  /* aNologgingCompletionLSN */
              != IDE_SUCCESS );


    IDE_EXCEPTION_CONT( SKIP_ADJUST_MAX_STAT );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : sdnbBTree::adjustMinStat                   *
 * ------------------------------------------------------------------*
 * MinValue�� �����Ѵ�.                                              *
 * Non-Null Value���� MinValue�� �ɼ� �ְ�, �������� ù��° Slot�̴�.*
 * freeNode �ÿ��� aOrgNode, aNewNode�� �ٸ���.                      *
 *********************************************************************/
IDE_RC sdnbBTree::adjustMinStat( idvSQL         * aStatistics,
                                 sdnbStatistic  * aIndexStat,
                                 smnIndexHeader * aPersIndex,
                                 sdnbHeader     * aIndex,
                                 sdrMtx         * aMtx,
                                 sdpPhyPageHdr  * aOrgNode,
                                 sdpPhyPageHdr  * aNewNode,
                                 idBool           aIsLoggingMeta,
                                 UInt             aStatFlag )
{
    sdnbColumn       * sColumn;
    scPageID           sLeafPID;
    scPageID           sOldMinNode;
    scPageID           sNewMinNode;
    sdnbLKey         * sKey;
    SInt               sMinSeq = 0;
    sdnbNodeHdr      * sNodeHdr;
    UChar              sMinMaxValue[ MAX_MINMAX_VALUE_SIZE ];
    UChar            * sSlotDirPtr;

    IDE_ASSERT( aIndex   != NULL );
    IDE_ASSERT( aOrgNode != NULL );

    sLeafPID    = sdpPhyPage::getPageID((UChar *)aOrgNode);
    sOldMinNode = aIndex->mMinNode;

    /* ������ ��� min stat�� �����Ѵ�.
     * - ���� leaf node�� min node �� ���
     * - min node�� ���� �����Ǿ� ���� ���� ��� */
    IDE_TEST_CONT( ( sLeafPID    != aIndex->mMinNode ) &&
                   ( SD_NULL_PID != aIndex->mMinNode ),
                    SKIP_ADJUST_MIN_STAT );

    /* BUG-30639 Disk Index���� Internal Node��
     *           Min/Max Node��� �ν��Ͽ� ����մϴ�.
     * Insert/Delete ���� ����� �� Node�� aOrgNode�� ���� ���̱�
     * ������ Internal�� �� �����ϴ�.
     * 
     * Index�� MaxNode�� Internal�� Ȯ���� ������, �̸� �������� �ʽ��ϴ�.
     * Internal���� Ȯ���Ϸ���, Max/Min Node�� getPage�ؾ� �ϱ� ������
     * �߰����� ����� ��� �����Դϴ�.
     * ���� ���� ����� server restart ���Դϴ�. */
    sNodeHdr = (sdnbNodeHdr*)sdpPhyPage::getLogicalHdrStartPtr( (UChar *)aOrgNode );

    if ( SDNB_IS_LEAF_NODE( sNodeHdr ) == ID_FALSE )
    {
        dumpHeadersAndIteratorToSMTrc( NULL,
                                       aIndex,
                                       NULL );
        dumpIndexNode( (sdpPhyPageHdr *)aOrgNode );
        IDE_ASSERT( 0 );
    }
    else
    {
        /* nothing to do */
    } 

    /*
     * get new min stat(min node, min value)
     */
    if ( aNewNode != NULL )
    {
        sColumn = &aIndex->mColumns[0];

        sSlotDirPtr = sdpPhyPage::getSlotDirStartPtr( (UChar *)aNewNode );
        IDE_TEST( sdpSlotDirectory::getPagePtrFromSlotNum( sSlotDirPtr,
                                                           0,
                                                           (UChar **)&sKey )
                  != IDE_SUCCESS );

        if ( isIgnoredKey4MinMaxStat( sKey, sColumn ) == ID_TRUE )
        {
            // ù��° Ű�� null �̸� ����� ��� Ű�� null �̴�.
            sMinSeq = -1;
            sNewMinNode = SD_NULL_PID;
        }
        else
        {
            sMinSeq = 0;
            sNewMinNode = sdpPhyPage::getPageID( (UChar *)aNewNode );
        }
    }
    else
    {
        sMinSeq = -1;
        sNewMinNode = SD_NULL_PID;
    }

    /*
     * set new min stat
     */
    if ( sNewMinNode == SD_NULL_PID )
    {
        idlOS::memset( sMinMaxValue,
                       0x00,
                       ID_SIZEOF(sMinMaxValue) );
        IDE_TEST(smLayerCallback::setIndexMinValue( aPersIndex,
                                                    sdrMiniTrans::getTrans( aMtx ),
                                                    sMinMaxValue,
                                                    aStatFlag )
                 != IDE_SUCCESS );

    }
    else
    {
        if ( sMinSeq >= 0 )
        {
            IDE_ERROR( setMinMaxValue( aIndex,
                                       (UChar *)aNewNode,
                                       sMinSeq,
                                       sMinMaxValue )
                       == IDE_SUCCESS );
            IDE_TEST( smLayerCallback::setIndexMinValue( aPersIndex,
                                                         sdrMiniTrans::getTrans( aMtx ),
                                                         sMinMaxValue,
                                                         aStatFlag )
                      != IDE_SUCCESS );
        }
        else
        {
            /* nothing to do */
        } 
    }
    aIndex->mMinNode = sNewMinNode;

    /* BUG-32623 When index statistics is rebuilded, the log's size
     * can exceed stack log buffer's size in mini-transaction. */

    // min node�� ���ŵɶ��� logging �Ѵ�.
    if ( (aIsLoggingMeta == ID_TRUE) && (aIndex->mMinNode != sOldMinNode) )
    {
        IDE_TEST( backupRuntimeHeader( aMtx, aIndex ) != IDE_SUCCESS );

        IDE_TEST( setIndexMetaInfo( aStatistics,
                                    aIndex,
                                    aIndexStat,
                                    aMtx,
                                    NULL,   /* aRootPID */
                                    &aIndex->mMinNode,
                                    NULL,   /* aMaxPID */
                                    NULL,   /* aFreeNodeHead */
                                    NULL,   /* aFreeNodeCnt */
                                    NULL,   /* aIsCreatedWithLogging */
                                    NULL,   /* aIsConsistent */
                                    NULL,   /* aIsCreatedWithForce */
                                    NULL )  /* aNologgingCompletionLSN */
                  != IDE_SUCCESS );
    }
    else
    {
        // nothing to do
    }

    IDE_EXCEPTION_CONT( SKIP_ADJUST_MIN_STAT );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : sdnbBTree::adjustNumDistStat               *
 * ------------------------------------------------------------------*
 * NumDist������ �����Ѵ�.                                           *
 * Unique Index�� ������ �����Ѵ�.                                   *
 *********************************************************************/
IDE_RC sdnbBTree::adjustNumDistStat( smnIndexHeader * aPersIndex,
                                     sdnbHeader     * aIndex,
                                     sdrMtx         * aMtx,
                                     sdpPhyPageHdr  * aLeafNode,
                                     SShort           aSlotSeq,
                                     SShort           aAddCount )
{
    UChar         * sKey;
    sdnbLKey      * sPrevKey;
    sdnbLKey      * sNextKey;
    UChar         * sKeyValue;
    idBool          sIsSameValue = ID_FALSE;
    sdnbKeyInfo     sKeyInfo;
    UChar         * sSlotDirPtr;

    IDE_TEST_CONT( isPrimaryKey(aIndex) == ID_TRUE,
                    SKIP_ADJUST_NUMDIST );

    sSlotDirPtr = sdpPhyPage::getSlotDirStartPtr( (UChar *)aLeafNode ); 
    IDE_TEST( sdpSlotDirectory::getPagePtrFromSlotNum( sSlotDirPtr, 
                                                       aSlotSeq,
                                                       &sKey )
              != IDE_SUCCESS );

    sKeyValue = SDNB_LKEY_KEY_PTR( sKey );

    /* BUG-30074 - disk table�� unique index���� NULL key ���� ��/
     *             non-unique index���� deleted key �߰� �� Cardinality��
     *             ��Ȯ���� ���� �������ϴ�.
     *
     * Null Key�� ��� NumDist�� �������� �ʵ��� �Ѵ�. */
    IDE_TEST_CONT( isNullKey( aIndex, sKeyValue ) == ID_TRUE,
                    SKIP_ADJUST_NUMDIST );

    /*
     * unique index�� ������ NumDist ����
     */
    IDE_TEST_CONT( aIndex->mIsUnique == ID_TRUE, SKIP_COMPARE );

    sKeyInfo.mKeyValue = sKeyValue;
    sIsSameValue       = ID_FALSE;

    /* check same value key */
    IDE_TEST ( findSameValueKey( aIndex,
                                 aLeafNode,
                                 aSlotSeq - 1,
                                 &sKeyInfo,
                                 SDNB_FIND_PREV_SAME_VALUE_KEY,
                                 &sPrevKey )
               != IDE_SUCCESS );

    if ( sPrevKey != NULL )
    {
        sIsSameValue = ID_TRUE;
    }
    else
    {
        IDE_TEST( findSameValueKey( aIndex,
                                    aLeafNode,
                                    aSlotSeq + 1,
                                    &sKeyInfo,
                                    SDNB_FIND_NEXT_SAME_VALUE_KEY,
                                    &sNextKey )
                  != IDE_SUCCESS );

        if ( sNextKey != NULL )
        {
            sIsSameValue = ID_TRUE;
        }
        else
        {
            /* nothing to do */
        } 
    }

    IDE_EXCEPTION_CONT( SKIP_COMPARE );

    if ( sIsSameValue != ID_TRUE )
    {
        IDE_TEST( smLayerCallback::incIndexNumDist( aPersIndex,
                                                    sdrMiniTrans::getTrans( aMtx ),
                                                    aAddCount )
                  != IDE_SUCCESS );
    }
    else
    {
        /* nothing to do */
    } 

    IDE_EXCEPTION_CONT( SKIP_ADJUST_NUMDIST );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : sdnbBTree::adjustNumDistStatByDistributeKeys
 * ------------------------------------------------------------------*
 * BUG-32313: The values of DRDB index Cardinality converge on 0     *
 *                                                                   *
 * Same Value Key���� �� ��忡 distribute�� ��쿡,                 *
 * Cardinality�� �����Ѵ�.                                           *
 * ��, Distributed Same Value Keys �� ��� Cardinality�� �����Ѵ�.   *
 *                                                                   *
 * split mode                                                        *
 *  o split���� Distributed Same Value Keys�� �� ���                *
 *      - Cardinality: +1                                            *
 *  o split���� Distributed Same Value Keys�� �ȵ� ���              *
 *      - Cardinality: N/A                                           *
 *                                                                   *
 * redistribute mode                                                 *
 *  o redistribute ������ Distributed Same Value Keys �� ���        *
 *      - redistribute�� Distributed Same Value Keys�� �� ���       *
 *        * Cardinality: N/A                                         *
 *      - redistribute�� Distributed Same Value Keys�� �ȵ� ���     *
 *        * Cardinality: -1                                          *
 *  o redistribute ������ Distributed Same Value Keys �ƴ� ���      *
 *      - redistribute�� Distributed Same Value Keys�� �� ���       *
 *        * Cardinality: +1                                          *
 *      - redistribute�� Distributed Same Value Keys�� �ȵ� ���     *
 *        * Cardinality: N/A                                         *
 *********************************************************************/
IDE_RC sdnbBTree::adjustNumDistStatByDistributeKeys( smnIndexHeader * aPersIndex,
                                                     sdnbHeader     * aIndex,
                                                     sdrMtx         * aMtx,
                                                     sdnbKeyInfo    * aPropagateKeyInfo,
                                                     sdpPhyPageHdr  * aLeafNode,
                                                     UShort           aMoveStartSeq,
                                                     UInt             aMode )
{
    sdnbLKey      * sKey      = NULL;
    sdnbLKey      * sPrevKey  = NULL;
    sdnbLKey      * sNextKey  = NULL;
    sdnbKeyInfo     sKeyInfo;
    UChar         * sSlotDirPtr;
    UShort          sKeyCount;
    sdpPhyPageHdr * sNextNode = NULL;
    scPageID        sNextPID;
    idBool          sIsDistributedSameValueKeysBefore = ID_FALSE;
    idBool          sIsDistributedSameValueKeys       = ID_FALSE;
    SShort          sAddCount    = 0;

    IDE_TEST_CONT( isPrimaryKey(aIndex) == ID_TRUE,
                    SKIP_ADJUST_NUMDIST );

    IDE_TEST_CONT( isNullKey( aIndex, aPropagateKeyInfo->mKeyValue ) == ID_TRUE,
                    SKIP_ADJUST_NUMDIST );

    /* redistribute ����� ��� redistribute ���� ����,
     * aLeafNode�� Ű�� Distributed Same Value Keys���� �˻��Ѵ�. */
    if ( aMode == SDNB_SMO_MODE_KEY_REDIST )
    {
        sNextPID  = sdpPhyPage::getNxtPIDOfDblList( aLeafNode );
        sNextNode = (sdpPhyPageHdr *)sdrMiniTrans::getPagePtrFromPageID(
                                                            aMtx, 
                                                            aIndex->mIndexTSID,
                                                            sNextPID );
        
        if ( sNextNode == NULL )
        {
            ideLog::log( IDE_ERR_0,
                         "index TSID : %"ID_UINT32_FMT""
                         ", get page ID : %"ID_UINT32_FMT""
                         "\n",
                         aIndex->mIndexTSID, aLeafNode->mListNode.mNext );
            dumpHeadersAndIteratorToSMTrc( NULL,
                                           aIndex,
                                           NULL );
            dumpIndexNode( aLeafNode );
            IDE_ASSERT( 0 );
        }
        else
        {
            /* nothing to do */
        } 

        sSlotDirPtr = sdpPhyPage::getSlotDirStartPtr( (UChar *)sNextNode ); 
        sKeyCount   = sdpSlotDirectory::getCount( sSlotDirPtr );
        
        if ( sKeyCount > 0 )
        {
            IDE_TEST( sdpSlotDirectory::getPagePtrFromSlotNum( sSlotDirPtr, 
                                                               0,
                                                               (UChar **)&sKey )
                      != IDE_SUCCESS );
            sKeyInfo.mKeyValue = SDNB_LKEY_KEY_PTR(sKey);

            if ( isNullKey(aIndex, sKeyInfo.mKeyValue) != ID_TRUE )
            {
                sSlotDirPtr = sdpPhyPage::getSlotDirStartPtr( (UChar *)aLeafNode ); 
                sKeyCount = sdpSlotDirectory::getCount( sSlotDirPtr );
        
                IDE_TEST( findSameValueKey( aIndex,
                                            aLeafNode,
                                            sKeyCount - 1,
                                            &sKeyInfo,
                                            SDNB_FIND_PREV_SAME_VALUE_KEY,
                                            &sPrevKey )
                          != IDE_SUCCESS );

                IDE_TEST ( findSameValueKey( aIndex,
                                             sNextNode,
                                             0,
                                             &sKeyInfo,
                                             SDNB_FIND_NEXT_SAME_VALUE_KEY,
                                             &sNextKey )
                           != IDE_SUCCESS );

                if ( ( sPrevKey != NULL ) && ( sNextKey != NULL ) )
                {
                    sIsDistributedSameValueKeysBefore = ID_TRUE;
                }
                else
                {
                    /* nothing to do */
                } 
            }
            else
            {
                /* nothing to do */
            } 
        }
        else
        {
            /* nothing to do */
        } 
    }

    // check Distributed Same Value Keys
    IDE_TEST ( findSameValueKey( aIndex,
                                 aLeafNode,
                                 aMoveStartSeq - 1,
                                 aPropagateKeyInfo,
                                 SDNB_FIND_PREV_SAME_VALUE_KEY,
                                 &sPrevKey )
              != IDE_SUCCESS );

    IDE_TEST ( findSameValueKey( aIndex,
                                 aLeafNode,
                                 aMoveStartSeq,
                                 aPropagateKeyInfo,
                                 SDNB_FIND_NEXT_SAME_VALUE_KEY,
                                 &sNextKey )
               != IDE_SUCCESS );

    if ( ( sPrevKey != NULL ) && ( sNextKey != NULL ) )
    {
        sIsDistributedSameValueKeys = ID_TRUE;
    }
    else
    {
        /* nothing to do */
    } 

    // update NumDist
    if ( sIsDistributedSameValueKeysBefore == ID_TRUE )
    {
        if ( sIsDistributedSameValueKeys == ID_FALSE )
        {
            sAddCount--;
        }
        else
        {
            /* nothing to do */
        } 
    }
    else
    {
        if ( sIsDistributedSameValueKeys == ID_TRUE )
        {
            sAddCount++;
        }
        else
        {
            /* nothing to do */
        } 
    }

    if ( sAddCount != 0 )
    {
        IDE_TEST( smLayerCallback::incIndexNumDist( aPersIndex,
                                                    sdrMiniTrans::getTrans( aMtx ),
                                                    sAddCount )
                  != IDE_SUCCESS );
    }
    else
    {
        /* nothing to do */
    } 

    IDE_EXCEPTION_CONT( SKIP_ADJUST_NUMDIST );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : sdnbBTree::deleteKeyFromLeafNode           *
 * ------------------------------------------------------------------*
 * BUG-25226: [5.3.1 SN/SD] 30�� �̻��� Ʈ����ǿ��� ���� �̹ݿ�     *
 *            ���ڵ尡 1�� �̻��� ���, allocCTS�� �����մϴ�.       *
 *                                                                   *
 * Leaf Node���� �־��� Ű�� �����Ѵ�.                               *
 * CTS�� �Ҵ� �����ϴٸ� TBT(Transactin info Bound in CTS)�� Ű��    *
 * ������ �ϰ�, �ݴ��� TBK(Transaction info Bound in Key)��      *
 * ������ �Ѵ�.                                                    *
 *********************************************************************/
IDE_RC sdnbBTree::deleteKeyFromLeafNode(idvSQL        * aStatistics,
                                        sdnbStatistic * aIndexStat,
                                        sdrMtx        * aMtx,
                                        sdnbHeader    * aIndex,
                                        sdpPhyPageHdr * aLeafPage,
                                        SShort        * aLeafKeySeq ,
                                        idBool          aIsPessimistic,
                                        idBool        * aIsSuccess )
{
    UChar                 sCTSlotNum;
    sdnbCallbackContext   sCallbackContext;
    UChar               * sSlotDirPtr;
    sdnbLKey            * sLeafKey;
    UShort                sKeyState;

    *aIsSuccess = ID_TRUE;

    sCallbackContext.mIndex = (sdnbHeader*)aIndex;
    sCallbackContext.mStatistics = aIndexStat;

    sSlotDirPtr = sdpPhyPage::getSlotDirStartPtr( (UChar *)aLeafPage );
    IDE_TEST( sdpSlotDirectory::getPagePtrFromSlotNum( sSlotDirPtr,
                                                       *aLeafKeySeq,
                                                       (UChar **)&sLeafKey  )
              != IDE_SUCCESS );
    sKeyState = SDNB_GET_STATE( sLeafKey );

    if ( ( sKeyState != SDNB_KEY_UNSTABLE ) &&
         ( sKeyState != SDNB_KEY_STABLE ) )
    {
        ideLog::log( IDE_ERR_0,
                     "leaf sequence number : %"ID_INT32_FMT"\n",
                     *aLeafKeySeq );
        dumpIndexNode( aLeafPage );
        IDE_ASSERT_MSG( 0, "Error Ouccurs in DRDB Index ID : %"ID_UINT32_FMT, aIndex->mIndexID );
    }
    else
    {
        /* nothing to do */
    } 

    IDE_TEST( sdnIndexCTL::allocCTS( aStatistics,
                                     aMtx,
                                     &aIndex->mSegmentDesc.mSegHandle,
                                     aLeafPage,
                                     &sCTSlotNum,
                                     &gCallbackFuncs4CTL,
                                     (UChar *)&sCallbackContext )
              != IDE_SUCCESS );

    /*
     * CTS �Ҵ��� ������ ���� TBK�� Ű�� �����Ѵ�.
     */
    if ( sCTSlotNum == SDN_CTS_INFINITE )
    {
        IDE_TEST( deleteLeafKeyWithTBK( aStatistics,
                                        aMtx,
                                        aIndex,
                                        aLeafPage,
                                        aLeafKeySeq ,
                                        aIsPessimistic,
                                        aIsSuccess )
                  != IDE_SUCCESS );

        IDE_TEST_CONT( *aIsSuccess == ID_FALSE, ALLOC_FAILURE );
    }
    else
    {
        IDE_TEST( deleteLeafKeyWithTBT( aMtx,
                                        aIndex,
                                        sCTSlotNum,
                                        aLeafPage,
                                        *aLeafKeySeq )
                  != IDE_SUCCESS );
    }

    IDE_EXCEPTION_CONT( ALLOC_FAILURE );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : sdnbBTree::insertKeyIntoLeafNode           *
 * ------------------------------------------------------------------*
 * ���ο� key�� leaf node�� Insert�Ѵ�.                              *
 * �� �Լ��� x latch�� ���� �����̸�, insert�� ��ġ�� �����Ǿ�����,  *
 * insert�� ���� SMO�� �߻��� ���� ����Ǹ� ���и� �����Ѵ�.         *
 * CTS �Ҵ��� ������ ���� TBK ���·� Ű�� �����.                  *
 * �ʿ��� ��� index�� min, max, NumDist�� �����Ų��.           *
 *********************************************************************/
IDE_RC sdnbBTree::insertKeyIntoLeafNode( idvSQL         * aStatistics,
                                         sdnbStatistic  * aIndexStat,
                                         sdrMtx         * aMtx,
                                         smnIndexHeader * aPersIndex,
                                         sdnbHeader     * aIndex,
                                         sdpPhyPageHdr  * aLeafPage,
                                         SShort         * aKeyMapSeq,
                                         sdnbKeyInfo    * aKeyInfo,
                                         idBool           aIsPessimistic,
                                         idBool         * aIsSuccess )
{
    UChar                 sCTSlotNum = SDN_CTS_INFINITE;
    sdnbCallbackContext   sContext;
    idBool                sIsDupKey = ID_FALSE;
    idBool                sIsSuccess = ID_FALSE;
    UChar                 sAgedCount = 0;
    UShort                sKeyValueSize;
    UShort                sKeyLength;

    *aIsSuccess = ID_TRUE;

    sContext.mIndex = aIndex;
    sContext.mStatistics = aIndexStat;

    sKeyValueSize = getKeyValueLength( &(aIndex->mColLenInfoList),
                                       aKeyInfo->mKeyValue );

    /*
     * BUG-26060 [SN] BTree Top-Down Build�� �߸��� CTS#��
     * �����ǰ� �ֽ��ϴ�.
     */
    if ( aKeyInfo->mKeyState == SDNB_KEY_STABLE )
    {
        sKeyLength = SDNB_LKEY_LEN( sKeyValueSize, SDNB_KEY_TB_CTS );
        IDE_CONT( SKIP_DUP_CHECK_AND_ALLOC_CTS );
    }
    else
    {
        /* nothing to do */
    }

    /*
     * DupKey���� �˻��Ѵ�.
     */
    IDE_TEST( isDuplicateKey( aIndexStat,
                              aIndex,
                              aLeafPage,
                              *aKeyMapSeq,
                              aKeyInfo,
                              &sIsDupKey ) 
              != IDE_SUCCESS )
    if ( sIsDupKey == ID_TRUE )
    {
        IDE_TEST( removeDuplicateKey( aStatistics,
                                      aMtx,
                                      aIndex,
                                      aLeafPage,
                                      *aKeyMapSeq,
                                      &sIsSuccess )
                  != IDE_SUCCESS );

        if ( sIsSuccess == ID_FALSE )
        {
            sIsDupKey = ID_TRUE;
        }
        else
        {
            sIsDupKey = ID_FALSE;
        }
    }  
    else
    {
        /* nothing to do */
    }

    IDU_FIT_POINT("BUG-48920@sdnbBTree::insertKeyIntoLeafNode::DupKey");

    IDE_TEST( sdnbBTree::allocCTS( aStatistics,
                                   aIndex,
                                   aMtx,
                                   aLeafPage,
                                   &sCTSlotNum,
                                   &gCallbackFuncs4CTL,
                                   (UChar *)&sContext,
                                   aKeyMapSeq )
              != IDE_SUCCESS );

    /*
     * 1. DUP Key�� ������ TBK�� ��ȯ�Ѵ�.(���� ���ԵǴ� Ű�� TBK�� ���� �ϴ� ���� �ƴ�)
     * 2. allocCTS�� ������ ��쵵 TBK�� ��ȯ�Ѵ�.
     */
    if ( ( sIsDupKey == ID_TRUE ) || ( sCTSlotNum == SDN_CTS_INFINITE ) )
    {
        sKeyLength = SDNB_LKEY_LEN( sKeyValueSize, SDNB_KEY_TB_KEY );
    }
    else
    {
        sKeyLength = SDNB_LKEY_LEN( sKeyValueSize, SDNB_KEY_TB_CTS );
    }

    IDE_EXCEPTION_CONT( SKIP_DUP_CHECK_AND_ALLOC_CTS );

    /*
     * canAllocLeafKey ������ Compaction���� ���Ͽ�
     * KeyMapSeq�� ����ɼ� �ִ�.
     */
    if ( canAllocLeafKey ( aMtx,
                           aIndex,
                           aLeafPage,
                           (UInt)sKeyLength,
                           aIsPessimistic,
                           aKeyMapSeq ) != IDE_SUCCESS )
    {
        /*
         * ���������� ���� �Ҵ��� ���ؼ� Self Aging�� �Ѵ�.
         */
        IDE_TEST( selfAging( aStatistics,
                             aIndex,
                             aMtx,
                             aLeafPage,
                             &sAgedCount )
                  != IDE_SUCCESS );

        if ( sAgedCount > 0 )
        {
            if ( canAllocLeafKey ( aMtx,
                                   aIndex,
                                   aLeafPage,
                                   (UInt)sKeyLength,
                                   aIsPessimistic,
                                   aKeyMapSeq )
                 != IDE_SUCCESS )
            {
                *aIsSuccess = ID_FALSE;
            }
            else
            {
                /* nothing to do */
            }                            
        }
        else
        {
            *aIsSuccess = ID_FALSE;
        }
    }
    else
    {
        /* nothing to do */
    }                            

    IDE_TEST_CONT( *aIsSuccess == ID_FALSE, ALLOC_FAILURE );

    /*
     * [BUG-25946] [SN] Dup-Key ������ allocCTS ���Ŀ� �����Ǿ�� �մϴ�.
     * AllocCTS�� SelfAging ������ ������ DUP KEY ���δ�
     * AllocCTS�� SelfAging���� ���Ͽ� KEY�� DEAD ���·� ����ɼ� �ֱ� ������,
     * AllocCTS�� SelfAging ���Ŀ� �ٽ� �ѹ� DUP-KEY ���θ� �Ǵ��ؾ� �Ѵ�.
     */
    if ( sIsDupKey == ID_TRUE )
    {
        IDE_TEST( isDuplicateKey( aIndexStat,
                                  aIndex,
                                  aLeafPage,
                                  *aKeyMapSeq,
                                  aKeyInfo,
                                  &sIsDupKey ) 
                  != IDE_SUCCESS )
    }
    else
    {
        /* nothing to do */
    }                            

    /*
     * Top-Down Build�� �ƴ� ����߿� CTS �Ҵ��� ������ ����
     * TBK�� Ű�� �����Ѵ�.
     */
    if ( ( aKeyInfo->mKeyState != SDNB_KEY_STABLE ) &&
         ( sCTSlotNum == SDN_CTS_INFINITE ) )
    {
        IDE_TEST( insertLeafKeyWithTBK( aMtx,
                                        aIndex,
                                        aLeafPage,
                                        aKeyInfo,
                                        sKeyValueSize,
                                        sIsDupKey,
                                        *aKeyMapSeq )
                  != IDE_SUCCESS );
    }
    else
    {
        //BUG-47715: TBT�� key insert�� �����ϸ� TBK������� �ٽ� �õ��Ѵ�.
        if ( insertLeafKeyWithTBT( aMtx,
                                   aIndex,
                                   sCTSlotNum,
                                   aLeafPage,
                                   aKeyInfo,
                                   sKeyValueSize,
                                   sIsDupKey,
                                   *aKeyMapSeq )
             != IDE_SUCCESS )
        {

            sKeyLength = SDNB_LKEY_LEN( sKeyValueSize, SDNB_KEY_TB_KEY );

            /*
             * canAllocLeafKey ������ Compaction���� ���Ͽ�
             * KeyMapSeq�� ����ɼ� �ִ�.
             */
            if ( canAllocLeafKey ( aMtx,
                                   aIndex,
                                   aLeafPage,
                                   (UInt)sKeyLength,
                                   aIsPessimistic,
                                   aKeyMapSeq ) != IDE_SUCCESS )
            {
                *aIsSuccess = ID_FALSE;
            }

            IDE_TEST_CONT( *aIsSuccess == ID_FALSE, ALLOC_FAILURE );

            IDE_TEST( insertLeafKeyWithTBK( aMtx,
                                            aIndex,
                                            aLeafPage,
                                            aKeyInfo,
                                            sKeyValueSize,
                                            sIsDupKey,
                                            *aKeyMapSeq )
                      != IDE_SUCCESS );
        }
    }

    if ( ( sIsDupKey == ID_FALSE ) &&
         ( needToUpdateStat() == ID_TRUE ) )
    {
        IDE_TEST( adjustNumDistStat( aPersIndex,
                                     aIndex,
                                     aMtx,
                                     aLeafPage,
                                     *aKeyMapSeq,
                                     1 )
                  != IDE_SUCCESS );

        IDE_TEST( adjustMaxStat( aStatistics,
                                 aIndexStat,
                                 aPersIndex,
                                 aIndex,
                                 aMtx,
                                 aLeafPage,
                                 aLeafPage,
                                 ID_TRUE, /* aIsLoggingMeta */
                                 SMI_INDEX_BUILD_RT_STAT_UPDATE )
                  != IDE_SUCCESS );

        IDE_TEST( adjustMinStat( aStatistics,
                                 aIndexStat,
                                 aPersIndex,
                                 aIndex,
                                 aMtx,
                                 aLeafPage,
                                 aLeafPage,
                                 ID_TRUE, /* aIsLoggingMeta */
                                 SMI_INDEX_BUILD_RT_STAT_UPDATE )
                  != IDE_SUCCESS );

        /* BUG-32943 [sm-disk-index] After executing DELETE ROW, the KeyCount
         * of DRDB Index is not decreased  */
        idCore::acpAtomicInc64( (void *)&aPersIndex->mStat.mKeyCount );
    }
    else
    {
        /* nothing to do ... */
    }

    IDE_DASSERT( validateNodeSpace( aIndex,
                                    aLeafPage,
                                    ID_TRUE /* aIsLeaf */)
                 == IDE_SUCCESS );

    IDE_EXCEPTION_CONT( ALLOC_FAILURE );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

/*********************************************************************
 * FUNCTION DESCRIPTION : sdnbBTree::allocCTS                        *
 * ------------------------------------------------------------------*
 * CTS�� �Ҵ��Ѵ�.                                                   *
 * ������ ������ ��쿡�� Compaction���Ŀ� Ȯ�嵵 ����Ѵ�.          *
 *********************************************************************/
IDE_RC sdnbBTree::allocCTS( idvSQL             * aStatistics,
                            sdnbHeader         * aIndex,
                            sdrMtx             * aMtx,
                            sdpPhyPageHdr      * aPage,
                            UChar              * aCTSlotNum,
                            sdnCallbackFuncs   * aCallbackFunc,
                            UChar              * aContext,
                            SShort             * aKeyMapSeq )
{
    UShort        sNeedSize;
    UShort        sFreeSize;
    sdnbNodeHdr * sNodeHdr;
    idBool        sSuccess;

    IDE_TEST( sdnIndexCTL::allocCTS( aStatistics,
                                     aMtx,
                                     &aIndex->mSegmentDesc.mSegHandle,
                                     aPage,
                                     aCTSlotNum,
                                     aCallbackFunc,
                                     aContext )
              != IDE_SUCCESS );

    if ( *aCTSlotNum == SDN_CTS_INFINITE )
    {
        sNodeHdr = (sdnbNodeHdr*)sdpPhyPage::getLogicalHdrStartPtr((UChar *)aPage);

        sNeedSize = ID_SIZEOF(sdnCTS);
        sFreeSize = sdpPhyPage::getTotalFreeSize(aPage);
        sFreeSize += sNodeHdr->mTotalDeadKeySize;

        if ( sNeedSize <= sFreeSize )
        {
            /*
             * Non-Fragment Free Space�� �ʿ��� ������� ���� ����
             * Compaction���� �ʴ´�.
             */
            if ( sNeedSize > sdpPhyPage::getNonFragFreeSize(aPage) )
            {
                if ( aKeyMapSeq != NULL )
                {
                    IDE_TEST( adjustKeyPosition( aPage, aKeyMapSeq )
                              != IDE_SUCCESS );
                }
                else
                {
                    /* nothing to do */
                }                            

                IDE_TEST( compactPage( aIndex,
                                       aMtx,
                                       aPage,
                                       ID_TRUE )
                          != IDE_SUCCESS );
            }
            else
            {
                /* nothing to do */
            }                            

            *aCTSlotNum = sdnIndexCTL::getCount( aPage );

            IDE_TEST( sdnIndexCTL::extend( aMtx,
                                           &aIndex->mSegmentDesc.mSegHandle,
                                           aPage,
                                           ID_TRUE, //aLogging
                                           &sSuccess )
                      != IDE_SUCCESS );

            if ( sSuccess == ID_FALSE )
            {
                *aCTSlotNum = SDN_CTS_INFINITE;
            }
            else
            {
                /* nothing to do */
            }                            
        }
        else
        {
            /* nothing to do */
        }                            
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : sdnbBTree::isDuplicateKey                  *
 * ------------------------------------------------------------------*
 * �����Ϸ��� [ROW_PID, ROW_SLOTNUM, VALUE]�� ���� Ű�� �̹�         *
 * �����ϴ��� �˻��Ѵ�.                                              *
 *********************************************************************/
IDE_RC sdnbBTree::isDuplicateKey( sdnbStatistic * aIndexStat,
                                  sdnbHeader    * aIndex,
                                  sdpPhyPageHdr * aLeafPage,
                                  SShort          aKeyMapSeq,
                                  sdnbKeyInfo   * aKeyInfo,
                                  idBool        * aIsDupKey )
{
    UShort        sSlotCount;
    sdnbLKey    * sLeafKey;
    sdnbKeyInfo   sKeyInfo;
    UInt          sRet;
    idBool        sIsSameValue;
    UChar       * sSlotDirPtr;
    idBool        sIsDupKey = ID_FALSE;

    sSlotDirPtr = sdpPhyPage::getSlotDirStartPtr( (UChar *)aLeafPage );
    sSlotCount  = sdpSlotDirectory::getCount( sSlotDirPtr );

    if ( aKeyMapSeq < sSlotCount )
    {
        IDE_TEST( sdpSlotDirectory::getPagePtrFromSlotNum( 
                                                     sSlotDirPtr,
                                                     aKeyMapSeq,
                                                     (UChar **)&sLeafKey )
                  != IDE_SUCCESS );
        /*
         * DEAD KEY��� ������ DUP-KEY�� �ƴϴ�.
         */
        if ( SDNB_GET_STATE( sLeafKey ) != SDNB_KEY_DEAD )
        {
            if ( SDNB_EQUAL_PID_AND_SLOTNUM(sLeafKey, aKeyInfo) )
            {
                sKeyInfo.mKeyValue = SDNB_LKEY_KEY_PTR( sLeafKey );

                sRet = compareKeyAndKey( aIndexStat,
                                         aIndex->mColumns,
                                         aIndex->mColumnFence,
                                         aKeyInfo,
                                         &sKeyInfo,
                                         SDNB_COMPARE_VALUE,
                                         &sIsSameValue );

                if ( sRet == 0 )
                {
                    sIsDupKey = ID_TRUE;
                }
                else
                {
                    /* nothing to do */
                }                            
            }
            else
            {
                /* nothing to do */
            }                            
        }
        else
        {
            /* nothing to do */
        }                            
    }
    else
    {
        /* nothing to do */
    }                            
    *aIsDupKey = sIsDupKey;

    return IDE_SUCCESS;
    
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : sdnbBTree::removeDuplicateKey              
 * ------------------------------------------------------------------
 * Duplicate Key�� ������ �ʱ� ���ؼ� Hard Key Stamping��
 * �õ��� ����.
 *                                                                   
 * BUG-38304 hp����� �����Ϸ� reordering ����
 * hp��񿡼� release��带 ���� �����Ϸ��� reordering�� ����
 * createCTS�� ���� �� hardKeyStamping�� ����Ǳ� ���� limitCTS�� ������ ������ �߻��Ѵ�.
 * �Ϲ����� ��Ȳ������ ������ ���� ������ createCTS�� limitCTS�� ���� ���
 * ���� CTS�� ������� �ι��� hardKeyStamping�� ȣ��Ǿ� CTS�� CTL�� ����ġ�� �߻��� �� �ִ�.
 * �̷� ������ limitCTS�� reordering�� ���� ���� leafKey�� volatile�� �����ϵ��� �Ѵ�.
 *********************************************************************/
IDE_RC sdnbBTree::removeDuplicateKey( idvSQL        * aStatistics,
                                      sdrMtx        * aMtx,
                                      sdnbHeader    * aIndex,
                                      sdpPhyPageHdr * aLeafPage,
                                      SShort          aKeyMapSeq,
                                      idBool        * aIsSuccess )
{
    volatile sdnbLKey    * sLeafKey;
    UChar                * sSlotDirPtr;
    UChar                  sCreateCTS;
    UChar                  sLimitCTS;
    idBool                 sSuccess;
    UShort                 sDeadKeySize  = 0;
    UShort                 sTBKCount     = 0;
    sdnbNodeHdr          * sNodeHdr      = NULL;
    idBool                 sIsCSCN       = ID_FALSE;
    idBool                 sIsLSCN       = ID_FALSE;
    sdnbLKey               sTmpLeafKey;

    *aIsSuccess = ID_FALSE;

    sSlotDirPtr = sdpPhyPage::getSlotDirStartPtr( (UChar *)aLeafPage );

    IDE_TEST( sdpSlotDirectory::getPagePtrFromSlotNum( sSlotDirPtr,
                                                       aKeyMapSeq,
                                                       (UChar **)&sLeafKey )
              != IDE_SUCCESS );
    IDE_DASSERT( SDNB_GET_STATE( sLeafKey ) == SDNB_KEY_DELETED );

    /* TBT�Ӿƴ϶� TBK�� stamping �õ��Ͽ�
       key�� ���°� DEAD�� �Ǵ��� Ȯ���ؾ��Ѵ�. (BUG-44973)
       (key���°� DEAD�� �Ǹ� Duplicated Key�� �ƴϴ�.) */
    if ( SDNB_GET_TB_TYPE( sLeafKey ) == SDNB_KEY_TB_KEY )
    {
        IDE_TEST( checkTBKStamping( aStatistics, 
                                    aLeafPage,
                                    (sdnbLKey *)sLeafKey,
                                    &sIsCSCN,
                                    &sIsLSCN ) != IDE_SUCCESS );

        if ( ( sIsCSCN == ID_TRUE ) ||
             ( sIsLSCN == ID_TRUE ) )
        {
            idlOS::memcpy( &sTmpLeafKey,
                           (void *)sLeafKey,
                           ID_SIZEOF(sdnbLKey) );

            if ( sIsCSCN == ID_TRUE )
            {
                SDNB_SET_CCTS_NO( &sTmpLeafKey, SDN_CTS_INFINITE );

                if ( SDNB_GET_STATE( &sTmpLeafKey ) == SDNB_KEY_UNSTABLE )
                {
                    SDNB_SET_STATE( &sTmpLeafKey, SDNB_KEY_STABLE );
                }
                else
                {
                    /* nothing to do */
                }
            }
            else
            {
                /* nothing to do */
            }

            sNodeHdr = (sdnbNodeHdr *)sdpPhyPage::getLogicalHdrStartPtr((UChar *)aLeafPage);

            if ( sIsLSCN == ID_TRUE )
            {
                SDNB_SET_STATE( &sTmpLeafKey, SDNB_KEY_DEAD );
                SDNB_SET_CCTS_NO( &sTmpLeafKey, SDN_CTS_INFINITE );
                SDNB_SET_LCTS_NO( &sTmpLeafKey, SDN_CTS_INFINITE );

                sDeadKeySize += getKeyLength( &(aIndex->mColLenInfoList),
                                              (UChar *)sLeafKey,
                                              ID_TRUE );

                sDeadKeySize += ID_SIZEOF( sdpSlotEntry );

                sDeadKeySize += sNodeHdr->mTotalDeadKeySize;

                sTBKCount = sNodeHdr->mTBKCount - 1;
            }
            else
            {
                /* nothing to do */
            }

            IDE_TEST( sdrMiniTrans::writeNBytes( aMtx,
                                                 (UChar *)sLeafKey->mTxInfo,
                                                 (void *)&(sTmpLeafKey.mTxInfo),
                                                 ID_SIZEOF(UChar) * 2 )
                      != IDE_SUCCESS );

            if ( sDeadKeySize > 0 )
            {
                IDE_TEST( sdrMiniTrans::writeNBytes( aMtx,
                                                     (UChar *)&sNodeHdr->mTotalDeadKeySize,
                                                     (void *)&sDeadKeySize,
                                                     ID_SIZEOF(UShort) )
                          != IDE_SUCCESS );


                IDE_TEST( sdrMiniTrans::writeNBytes( aMtx,
                                                     (UChar *)&sNodeHdr->mTBKCount,
                                                     (void *)&sTBKCount,
                                                     ID_SIZEOF(UShort) )
                          != IDE_SUCCESS );
            }
            else
            {
                /* nothing to do */
            }
        }
        else
        {
            /* nothing to do */
        }
    }
    else
    {
        /* nothing to do */
    }
    /* (BUG-44973) �� */

    sCreateCTS = SDNB_GET_CCTS_NO( sLeafKey );
    if ( SDN_IS_VALID_CTS( sCreateCTS ) )
    {
        if ( sdnIndexCTL::isMyTransaction( aMtx->mTrans,
                                           aLeafPage,
                                           sCreateCTS )
            == ID_FALSE )
        {
            IDE_TEST( hardKeyStamping( aStatistics,
                                       aIndex,
                                       aMtx,
                                       aLeafPage,
                                       sCreateCTS,
                                       &sSuccess )
                      != IDE_SUCCESS );
        }
        else
        {
            /* nothing to do */
        }                            
    }
    else
    {
        /* nothing to do */
    }                            

    sLimitCTS  = SDNB_GET_LCTS_NO( sLeafKey );
    if ( SDN_IS_VALID_CTS( sLimitCTS ) )
    {
        if ( sdnIndexCTL::isMyTransaction( aMtx->mTrans,
                                           aLeafPage,
                                           sLimitCTS )
            == ID_FALSE )
        {
            IDE_TEST( hardKeyStamping( aStatistics,
                                       aIndex,
                                       aMtx,
                                       aLeafPage,
                                       sLimitCTS,
                                       &sSuccess )
                      != IDE_SUCCESS );
        }
        else
        {
            /* nothing to do */
        }                            
    }
    else
    {
        /* nothing to do */
    }                            

    if ( SDNB_GET_STATE( sLeafKey ) == SDNB_KEY_DEAD )
    {
        *aIsSuccess = ID_TRUE;
    }
    else
    {
        /* nothing to do */
    }                            

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : sdnbBTree::makeDuplicateKey                *
 * ------------------------------------------------------------------*
 * Unique Key�� Duplicate Key(���� DupKey)�� �����.                 *
 * DupKey�� �Ǳ� ������ Transaction ������ ��������,                 *
 * ���ο� Transaction������ ��ϵȴ�. ���������� ������ Visibility   *
 * �˻縦 �Ұ����ϰ� �����. ����, SCAN�� DupKey�� ������ ��쿡�� *
 * ���ڵ��� Transaction������ �̿��ؼ� Visibility�˻縦 �ؾ� �Ѵ�.   *
 *********************************************************************/
IDE_RC sdnbBTree::makeDuplicateKey( sdrMtx        * aMtx,
                                    sdnbHeader    * aIndex,
                                    UChar           aCTSlotNum,
                                    sdpPhyPageHdr * aLeafPage,
                                    SShort          aKeyMapSeq,
                                    UChar           aTxBoundType )
{
    sdnbLKey            * sLeafKey;
    sdnbLKey            * sRemoveKey;
    UChar                 sCreateCTS;
    UChar                 sLimitCTS;
    UShort                sKeyLength;
    sdnbRollbackContextEx sRollbackContextEx;
    UChar               * sSlotDirPtr;
    UShort                sKeyOffset = 0;
    UShort                sNewKeyOffset = SC_NULL_OFFSET;
    sdnbNodeHdr         * sNodeHdr;
    UShort                sAllowedSize;
    UShort                sNewKeyLength;
    UChar                 sTxBoundType;
    UChar                 sRemoveInsert = ID_FALSE;
    UShort                sTotalTBKCount = 0;
    smSCN                 sFstDiskViewSCN;
    sdSID                 sTSSlotSID;
    sdnbLTxInfo         * sTxInfoEx;

    sFstDiskViewSCN = smLayerCallback::getFstDskViewSCN( aMtx->mTrans );
    sTSSlotSID      = smLayerCallback::getTSSlotSID( aMtx->mTrans );

    sNodeHdr = (sdnbNodeHdr*)sdpPhyPage::getLogicalHdrStartPtr( (UChar *)aLeafPage );

    sSlotDirPtr = sdpPhyPage::getSlotDirStartPtr( (UChar *)aLeafPage );
    IDE_TEST( sdpSlotDirectory::getPagePtrFromSlotNum( sSlotDirPtr,
                                                       aKeyMapSeq,
                                                       (UChar **)&sLeafKey )
              != IDE_SUCCESS );

    IDE_TEST( sdpSlotDirectory::getValue( sSlotDirPtr, aKeyMapSeq, &sKeyOffset )
              != IDE_SUCCESS );
    sCreateCTS = SDNB_GET_CCTS_NO( sLeafKey );
    sLimitCTS  = SDNB_GET_LCTS_NO( sLeafKey );
    sTxBoundType = SDNB_GET_TB_TYPE( sLeafKey );

    idlOS::memset( &sRollbackContextEx,
                   0x00,
                   ID_SIZEOF( sdnbRollbackContextEx ) );
    sRollbackContextEx.mRollbackContext.mTableOID = aIndex->mTableOID;
    sRollbackContextEx.mRollbackContext.mIndexID  = aIndex->mIndexID;

    idlOS::memcpy( sRollbackContextEx.mRollbackContext.mTxInfo,
                   sLeafKey->mTxInfo,
                   ID_SIZEOF(UChar) * 2 );

    /* duplicated key�� rollback�ɶ� 
       sdnbBTree:: insertKeyRollback()�Լ����� ���� �������� ����,
       rollback ������ �����ϴ� �̰����� �ٸ� ������ �α��ϵ��� �ٲ�. (BUG-44973) */
    SDNB_SET_STATE( &sRollbackContextEx.mRollbackContext, SDNB_KEY_DELETED );
    SDNB_SET_TB_TYPE( &sRollbackContextEx.mRollbackContext, SDNB_KEY_TB_KEY );

    sTxInfoEx = &sRollbackContextEx.mTxInfoEx;
    if ( sTxBoundType == SDNB_KEY_TB_KEY )
    {
        idlOS::memcpy( sTxInfoEx,
                       &((sdnbLKeyEx*)sLeafKey)->mTxInfoEx,
                       ID_SIZEOF( sdnbLTxInfo ) );
    }
    else
    {
        /* nothing to do */
    }                            

    if ( SDN_IS_VALID_CTS( sCreateCTS ) )
    {
        if ( (sdnIndexCTL::getCTSlotState( aLeafPage,
                                           sCreateCTS ) != SDN_CTS_DEAD) )
        {
            sTxInfoEx->mCreateCSCN = sdnIndexCTL::getCommitSCN( aLeafPage, sCreateCTS );
            sTxInfoEx->mCreateTSS  = sdnIndexCTL::getTSSlotSID( aLeafPage, sCreateCTS );

            /* TBT�̰� CTS�� ���� ���� ���,
               rollback ������ CTS No�� SDN_CTS_IN_KEY�� �����Ѵ�. (BUG-44973) */
            SDNB_SET_CCTS_NO( &sRollbackContextEx.mRollbackContext, SDN_CTS_IN_KEY );
        }
        else
        {
            /* nothing to do */
        }                            
    }
    else
    {
        /* nothing to do */
    }                            
                                                            
    if ( SDN_IS_VALID_CTS( sLimitCTS ) )
    {
        if ( (sdnIndexCTL::getCTSlotState( aLeafPage,
                                           sLimitCTS ) != SDN_CTS_DEAD) )
        {
            sTxInfoEx->mLimitCSCN = sdnIndexCTL::getCommitSCN( aLeafPage, sLimitCTS );
            sTxInfoEx->mLimitTSS  = sdnIndexCTL::getTSSlotSID( aLeafPage, sLimitCTS );

            /* TBT�̰� CTS�� ���� ���� ���,
               rollback ������ CTS No�� SDN_CTS_IN_KEY�� �����Ѵ�. (BUG-44973) */
            SDNB_SET_LCTS_NO( &sRollbackContextEx.mRollbackContext, SDN_CTS_IN_KEY );
        }
        else
        {
            /* nothing to do */
        }                            
    }
    else
    {
        /* nothing to do */
    }                            

    if ( aTxBoundType == SDNB_KEY_TB_CTS )
    {
        IDE_TEST( sdnIndexCTL::bindCTS( aMtx,
                                        aIndex->mIndexTSID,
                                        aLeafPage,
                                        aCTSlotNum,
                                        sKeyOffset )
                  != IDE_SUCCESS );
    }
    else
    {
        /* nothing to do */
    }                            
    
    /*
     * ���ο� Ű�� �Ҵ�Ǿ�� �Ѵٸ� TBK Ű�� �����.
     */
    if ( sTxBoundType != SDNB_KEY_TB_KEY )
    {
        sRemoveInsert = ID_TRUE;
        sNewKeyLength = SDNB_LKEY_LEN( getKeyValueLength( &(aIndex->mColLenInfoList),
                                                          SDNB_LKEY_KEY_PTR(sLeafKey) ),
                                       SDNB_KEY_TB_KEY );
        IDE_TEST( sdpPhyPage::allocSlot( (sdpPhyPageHdr*)aLeafPage,
                                         aKeyMapSeq,
                                         sNewKeyLength,
                                         ID_TRUE,
                                         &sAllowedSize,
                                         (UChar **)&sLeafKey,
                                         &sNewKeyOffset,
                                         1 )
                  != IDE_SUCCESS );

        idlOS::memset( sLeafKey, 0x00, sNewKeyLength );

        IDE_TEST( sdpSlotDirectory::getPagePtrFromSlotNum( sSlotDirPtr,
                                                           aKeyMapSeq + 1,
                                                           (UChar **)&sRemoveKey )
                  != IDE_SUCCESS );
        /*
         * BUG-25832 [SN] Key ���°� TBK���� TBT�� ��ȯ�� Key Header��
         *           ������� �ʽ��ϴ�.
         */
        idlOS::memcpy( sLeafKey,
                       sRemoveKey,
                       ID_SIZEOF( sdnbLKey ) );
        SDNB_SET_TB_TYPE( sLeafKey, SDNB_KEY_TB_KEY );
        idlOS::memcpy( SDNB_LKEY_KEY_PTR( sLeafKey ),
                       SDNB_LKEY_KEY_PTR( sRemoveKey ),
                       getKeyValueLength( &(aIndex->mColLenInfoList),
                                          SDNB_LKEY_KEY_PTR( sRemoveKey ) ) );
        
        if ( aTxBoundType == SDNB_KEY_TB_CTS )
        {
            IDE_TEST( sdnIndexCTL::updateRefKey( aMtx,
                                                 aLeafPage,
                                                 aCTSlotNum,
                                                 sKeyOffset,
                                                 sNewKeyOffset )
                      != IDE_SUCCESS );
        }
        else
        {
            /* nothing to do */
        }                            
    }
    else
    {
        /* nothing to do */
    }                            
    
    SDNB_SET_CCTS_NO( sLeafKey, aCTSlotNum );
    SDNB_SET_DUPLICATED( sLeafKey, SDNB_DUPKEY_YES );
    SDNB_SET_LCTS_NO( sLeafKey, SDN_CTS_INFINITE );
    SDNB_SET_STATE( sLeafKey, SDNB_KEY_UNSTABLE );
    SDNB_SET_TB_TYPE( sLeafKey, SDNB_KEY_TB_KEY );

    /*
     * Ʈ����� ������ KEY�� �����Ѵ�.
     */
    if ( aTxBoundType == SDNB_KEY_TB_KEY )
    {
        SDNB_SET_TBK_CSCN( ((sdnbLKeyEx*)sLeafKey), &sFstDiskViewSCN );
        SDNB_SET_TBK_CTSS( ((sdnbLKeyEx*)sLeafKey), &sTSSlotSID );
    }
    else
    {
        /* nothing to do */
    }                            

    IDE_DASSERT( sLimitCTS != SDN_CTS_INFINITE );

    sNodeHdr->mUnlimitedKeyCount++;
    IDE_TEST( sdrMiniTrans::writeNBytes( aMtx,
                                         (UChar *)&(sNodeHdr->mUnlimitedKeyCount),
                                         (void *)&sNodeHdr->mUnlimitedKeyCount,
                                         ID_SIZEOF(UShort) )
              != IDE_SUCCESS );

    sKeyLength = getKeyLength( &(aIndex->mColLenInfoList),
                               (UChar *)sLeafKey,
                               ID_TRUE /* aIsLeaf */ );

    sdrMiniTrans::setRefOffset( aMtx, SM_OID_NULL );
    // write log
    IDE_TEST( sdrMiniTrans::writeLogRec( aMtx,
                                         (UChar *)sLeafKey,
                                         (void *)NULL,
                                         ID_SIZEOF(SShort) +
                                         ID_SIZEOF(UChar) +
                                         ID_SIZEOF(sdnbRollbackContextEx) +
                                         sKeyLength,
                                         SDR_SDN_INSERT_DUP_KEY )
              != IDE_SUCCESS );

    IDE_TEST( sdrMiniTrans::write( aMtx,
                                   (void *)&aKeyMapSeq,
                                   ID_SIZEOF(SShort) )
              != IDE_SUCCESS );

    IDE_TEST( sdrMiniTrans::write( aMtx,
                                   (void *)&sRemoveInsert,
                                   ID_SIZEOF(UChar) )
              != IDE_SUCCESS );

    IDE_TEST( sdrMiniTrans::write( aMtx,
                                   (void *)&sRollbackContextEx,
                                   ID_SIZEOF(sdnbRollbackContextEx))
              != IDE_SUCCESS );

    IDE_TEST( sdrMiniTrans::write( aMtx,
                                   (void *)sLeafKey,
                                   sKeyLength )
              != IDE_SUCCESS );

    /*
     * Dup Key�� Remove & Insert�� ó���Ǿ��ٸ� ���� KEY�� �����Ѵ�.
     */
    if ( sRemoveInsert == ID_TRUE )
    {
        sTotalTBKCount = sNodeHdr->mTBKCount + 1;
        IDE_TEST( sdrMiniTrans::writeNBytes( aMtx,
                                             (UChar *)&(sNodeHdr->mTBKCount),
                                             (void *)&sTotalTBKCount,
                                             ID_SIZEOF(UShort) )
                  != IDE_SUCCESS );

        IDE_TEST( sdpSlotDirectory::getPagePtrFromSlotNum( sSlotDirPtr,
                                                           aKeyMapSeq + 1,
                                                           (UChar **)&sLeafKey )
                  != IDE_SUCCESS );

        sKeyLength = getKeyLength( &(aIndex->mColLenInfoList),
                                   (UChar *)sLeafKey,
                                   ID_TRUE /* aIsLeaf */ );

        IDE_TEST( sdrMiniTrans::writeLogRec( aMtx,
                                             (UChar *)sLeafKey,
                                             (void *)&sKeyLength,
                                             ID_SIZEOF(UShort),
                                             SDR_SDN_FREE_INDEX_KEY )
                 != IDE_SUCCESS );

        IDE_TEST( sdpPhyPage::freeSlot( aLeafPage,
                                        aKeyMapSeq + 1,
                                        sKeyLength,
                                        1 )
                  != IDE_SUCCESS );
    }
    else
    {
        /* nothing to do */
    }                            

    if ( SDN_IS_VALID_CTS( sCreateCTS ) )
    {
        if ( (sdnIndexCTL::getCTSlotState( aLeafPage,
                                           sCreateCTS ) != SDN_CTS_DEAD) )
        {
            IDE_TEST( sdnIndexCTL::unbindCTS( aMtx,
                                              aLeafPage,
                                              sCreateCTS,
                                              sKeyOffset )
                      != IDE_SUCCESS );
        }
        else
        {
            /* nothing to do */
        }                            
    }
    else
    {
        /* nothing to do */
    }                            

    if ( sLimitCTS == SDN_CTS_INFINITE )
    {
        ideLog::log( IDE_ERR_0,
                     "sequence number : %"ID_INT32_FMT""
                     ", CTS slot number : %"ID_UINT32_FMT"\n",
                     aKeyMapSeq, aCTSlotNum );
        dumpIndexNode( aLeafPage );
        IDE_ASSERT_MSG( 0, "Error Ouccurs in DRDB Index ID : %"ID_UINT32_FMT, aIndex->mIndexID );
    }
    else
    {
        /* nothing to do */
    }                            

    if ( SDN_IS_VALID_CTS( sLimitCTS ) )
    {
        IDE_TEST( sdnIndexCTL::unbindCTS( aMtx,
                                          aLeafPage,
                                          sLimitCTS,
                                          sKeyOffset )
                  != IDE_SUCCESS );
    }
    else
    {
        /* nothing to do */
    }                            

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : sdnbBTree::makeNewRootNode                 *
 * ------------------------------------------------------------------*
 * ���ο� node�� �Ҵ��Ͽ� root���� �����.                         *
 * Root�� ���� ������ ���� �ݵ�� Key�� 1���� �ö󰣴�.              *
 *********************************************************************/
IDE_RC sdnbBTree::makeNewRootNode( idvSQL          * aStatistics,
                                   sdnbStatistic   * aIndexStat,
                                   sdrMtx          * aMtx,
                                   idBool          * aMtxStart, 
                                   smnIndexHeader  * aPersIndex,
                                   sdnbHeader      * aIndex,
                                   sdnbStack       * aStack,
                                   sdnbKeyInfo     * aInfo,
                                   UShort            aKeyValueLength,
                                   sdpPhyPageHdr  ** aNewChildPage,
                                   UInt            * aAllocPageCount )
{

    sdpPhyPageHdr * sNewRootPage;
    UShort          sHeight;
    scPageID        sNewRootPID;
    scPageID        sLeftmostPID;
    scPageID        sRightChildPID;
    sdnbNodeHdr   * sNodeHdr;
    UShort          sKeyLen;
    UShort          sAllowedSize;
    scOffset        sSlotOffset;
    sdnbIKey      * sIKey;
    SInt            i;
    SShort          sSlotSeq = 0;
    sdnbKeyInfo   * sKeyInfo;
    idBool          sIsSuccess = ID_FALSE;
    ULong           sSmoNo;
    
    // SMO�� �ֻ�� -- allocPages
    // allocate new pages, stack tmp depth + 2 ��ŭ
    IDE_TEST( preparePages( aStatistics,
                            aIndex,
                            aMtx,
                            aMtxStart,
                            aStack->mIndexDepth + 2 )
              != IDE_SUCCESS );

    // init root node
    IDE_ASSERT( allocPage( aStatistics,
                           aIndexStat,
                           aIndex,
                           aMtx,
                           (UChar **)&sNewRootPage ) == IDE_SUCCESS );
    (*aAllocPageCount)++;

    IDE_DASSERT( ( (vULong)sNewRootPage % SD_PAGE_SIZE ) == 0 );

    sNewRootPID = sdpPhyPage::getPageID( (UChar *)sNewRootPage );

    getSmoNo( (void *)aIndex, &sSmoNo );
    sdpPhyPage::setIndexSMONo((UChar *)sNewRootPage, sSmoNo + 1 );

    IDL_MEM_BARRIER;

    sNodeHdr = (sdnbNodeHdr*)sdpPhyPage::getLogicalHdrStartPtr( (UChar *)sNewRootPage );

    // set height
    sHeight = aStack->mIndexDepth + 1;

    IDE_TEST( initializeNodeHdr( aMtx,
                                 sNewRootPage,
                                 SD_NULL_PID,
                                 sHeight,
                                 ID_TRUE )
              != IDE_SUCCESS );

    if ( sHeight == 0 ) // �� root�� leaf node
    {
        sKeyInfo = aInfo;

        IDE_TEST( sdnIndexCTL::init( aMtx,
                                     &aIndex->mSegmentDesc.mSegHandle,
                                     sNewRootPage,
                                     0 )
                  != IDE_SUCCESS );

        IDE_DASSERT( aIndex->mRootNode == SD_NULL_PID );
        *aNewChildPage = sNewRootPage;
        sLeftmostPID = SD_NULL_PID;

        IDE_TEST( insertKeyIntoLeafNode( aStatistics,
                                         aIndexStat,
                                         aMtx,
                                         aPersIndex,
                                         aIndex,
                                         sNewRootPage,
                                         &sSlotSeq,
                                         sKeyInfo,
                                         ID_TRUE   /* aIsPessimistic */,
                                         &sIsSuccess )
                  != IDE_SUCCESS );

        IDE_DASSERT( sIsSuccess == ID_TRUE );
    }
    else
    {
        sKeyInfo = aInfo;

        // �� child node��  PID�� �� root�� �� slot�� child PID
        IDE_ASSERT( allocPage( aStatistics,
                               aIndexStat,
                               aIndex,
                               aMtx,
                               (UChar **)aNewChildPage ) == IDE_SUCCESS );
        (*aAllocPageCount)++;

        sLeftmostPID = aStack->mStack[0].mNode;
        sKeyLen = SDNB_IKEY_LEN( aKeyValueLength );
        IDE_TEST( sdpPhyPage::allocSlot( (sdpPhyPageHdr*)sNewRootPage,
                                         0,
                                         sKeyLen,
                                         ID_TRUE,
                                         &sAllowedSize,
                                         (UChar **)&sIKey,
                                         &sSlotOffset,
                                         1 )
                  != IDE_SUCCESS );
        sRightChildPID = sdpPhyPage::getPageID((UChar *)*aNewChildPage);
        SDNB_KEYINFO_TO_IKEY( (*sKeyInfo), 
                              sRightChildPID, 
                              aKeyValueLength, 
                              sIKey );

        if ( aIndex->mLogging == ID_TRUE )
        {
            IDE_TEST( sdrMiniTrans::writeLogRec( aMtx,
                                                 (UChar *)sIKey,
                                                 (void *)NULL,
                                                 ID_SIZEOF(SShort)+sKeyLen,
                                                 SDR_SDN_INSERT_INDEX_KEY )
                      != IDE_SUCCESS );

            IDE_TEST( sdrMiniTrans::write( aMtx,
                                           (void *)&sSlotSeq,
                                           ID_SIZEOF(SShort) )
                      != IDE_SUCCESS );

            IDE_TEST( sdrMiniTrans::write( aMtx,
                                           (void *)sIKey,
                                           sKeyLen)
                      != IDE_SUCCESS );
        }
        else
        {
            /* nothing to do */
        }                            

        // leftmost child PID
        IDE_TEST( sdrMiniTrans::writeNBytes( aMtx,
                                             (UChar *)&sNodeHdr->mLeftmostChild,
                                             (void *)&sLeftmostPID,
                                             ID_SIZEOF(sLeftmostPID) )
                  != IDE_SUCCESS );
    }

    // insert root node, keymap seq 0 into stack's bottom
    for ( i = aStack->mIndexDepth ; i >= 0 ; i-- )
    {
        idlOS::memcpy( &aStack->mStack[i+1],
                       &aStack->mStack[i],
                       ID_SIZEOF(sdnbStackSlot) );
    }
    aStack->mStack[0].mNode = sdpPhyPage::getPageID( (UChar *)sNewRootPage );

    getSmoNo( (void *)aIndex, &sSmoNo );
    aStack->mStack[0].mSmoNo = sSmoNo + 1;
    aStack->mStack[0].mKeyMapSeq = -1; // �ϴ�. ������忡�� ����.

    aStack->mIndexDepth++;
    aStack->mCurrentDepth++;

    IDE_TEST( backupRuntimeHeader( aMtx, aIndex ) != IDE_SUCCESS );
    aIndex->mRootNode = sNewRootPID;

    IDE_TEST( setIndexMetaInfo( aStatistics,
                                aIndex,
                                aIndexStat,
                                aMtx,
                                &sNewRootPID,
                                NULL,   /* aMinPID */
                                NULL,   /* aMaxPID */
                                NULL,   /* aFreeNodeHead */
                                NULL,   /* aFreeNodeCnt */
                                NULL,   /* aIsCreatedWithLogging */
                                NULL,   /* aIsConsistent */
                                NULL,   /* aIsCreatedWithForce */
                                NULL )  /* aNologgingCompletionLSN */
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

/*********************************************************************
 * FUNCTION DESCRIPTION : sdnbBTree::moveSlots                       *
 * ------------------------------------------------------------------*
 * aSrcNode�κ��� aDstNode�� slot���� �ű�� �α��Ѵ�.               *
 *********************************************************************/
IDE_RC sdnbBTree::moveSlots( sdrMtx         * aMtx,
                             sdnbHeader     * aIndex,
                             sdpPhyPageHdr  * aSrcNode,
                             SInt             aHeight,
                             SShort           aFromSeq,
                             SShort           aToSeq,
                             sdpPhyPageHdr  * aDstNode )
{
    SInt                  i;
    SShort                sDstSeq;
    UShort                sAllowedSize;
    UChar               * sSlotDirPtr;
    sdnbLKey            * sSrcSlot = NULL;
    sdnbLKey            * sDstSlot = NULL;
    UChar                 sSrcCreateCTS;
    UChar                 sSrcLimitCTS;
    UChar                 sDstCreateCTS;
    UChar                 sDstLimitCTS;
    UShort                sKeyLen  = 0;
    UShort                sUnlimitedKeyCount = 0;
    UShort                sKeyOffset   = SC_NULL_OFFSET;
    sdnbNodeHdr         * sSrcNodeHdr  = NULL;
    sdnbNodeHdr         * sDstNodeHdr  = NULL;
    IDE_RC                sRc;
    UShort                sSrcTBKCount = 0;
    UShort                sDstTBKCount = 0;

    sSrcNodeHdr = (sdnbNodeHdr*)
                      sdpPhyPage::getLogicalHdrStartPtr( (UChar *)aSrcNode );
    sDstNodeHdr = (sdnbNodeHdr*)
                      sdpPhyPage::getLogicalHdrStartPtr( (UChar *)aDstNode );

    sSlotDirPtr = sdpPhyPage::getSlotDirStartPtr( (UChar *)aSrcNode );

    /*
     * Leaf Node�� �׻� Compaction�� �Ϸ�� ���¿��� �Ѵ�.
     */
    IDE_DASSERT( ( SDNB_IS_LEAF_NODE(sSrcNodeHdr) == ID_FALSE )  ||
                 ( sdpPhyPage::getNonFragFreeSize( aSrcNode ) == sdpPhyPage::getTotalFreeSize( aSrcNode ) ) );

    /*
     * 1. Copy Source Key to Destination Node
     */
    sUnlimitedKeyCount = sDstNodeHdr->mUnlimitedKeyCount;
    // BUG-29538 split�� TBK count�� �������� �ʰ� �ֽ��ϴ�.
    sSrcTBKCount       = sSrcNodeHdr->mTBKCount;

    for ( i = aFromSeq ; i <= aToSeq ; i++ )
    {
        IDE_TEST( sdpSlotDirectory::getPagePtrFromSlotNum( sSlotDirPtr, 
                                                           i,
                                                           (UChar **)&sSrcSlot )
                  != IDE_SUCCESS );
        sKeyLen = getKeyLength( &(aIndex->mColLenInfoList),
                                (UChar *)sSrcSlot,
                                (aHeight == 0) ? ID_TRUE : ID_FALSE );

        sDstSeq = i - aFromSeq;

        if ( SDNB_IS_LEAF_NODE( sSrcNodeHdr ) == ID_TRUE )
        {
            sRc = sdnbBTree::canAllocLeafKey ( aMtx,
                                               aIndex,
                                               aDstNode,
                                               sKeyLen,
                                               ID_TRUE, /* aIsPessimistic */
                                               NULL );

            if ( sRc != IDE_SUCCESS )
            {
                ideLog::log( IDE_ERR_0,
                             "From <%"ID_INT32_FMT"> to <%"ID_INT32_FMT"> : "
                             "Current sequence number : %"ID_INT32_FMT""
                             ", KeyLen %"ID_UINT32_FMT"\n",
                             aFromSeq, 
                             aToSeq, 
                             i, 
                             sKeyLen );
                dumpIndexNode( aSrcNode );
                dumpIndexNode( aDstNode );
                IDE_ASSERT_MSG( 0, "Error Ouccurs in DRDB Index ID : %"ID_UINT32_FMT, aIndex->mIndexID );
            }
            else
            {
                /* nothing to do */
            }                            
        }
        else
        {
            sRc = sdnbBTree::canAllocInternalKey ( aMtx,
                                                   aIndex,
                                                   aDstNode,
                                                   sKeyLen,
                                                   ID_TRUE,   /* aExecCompact */
                                                   ID_TRUE ); /* aIsLogging */

            if ( sRc != IDE_SUCCESS )
            {
                ideLog::log( IDE_ERR_0,
                             "From <%"ID_INT32_FMT"> to <%"ID_INT32_FMT"> : "
                             "Current sequence number : %"ID_INT32_FMT""
                             ", KeyLen %"ID_UINT32_FMT"\n",
                             aFromSeq, 
                             aToSeq, 
                             i, 
                             sKeyLen );
                dumpIndexNode( aSrcNode );
                dumpIndexNode( aDstNode );
                IDE_ASSERT_MSG( 0, "Error Ouccurs in DRDB Index ID : %"ID_UINT32_FMT, aIndex->mIndexID );
            }
            else
            {
                /* nothing to do */
            }                            
        }

        IDE_TEST( sdpPhyPage::allocSlot( (sdpPhyPageHdr*)aDstNode,
                                          sDstSeq,
                                          sKeyLen,
                                          ID_TRUE,
                                          &sAllowedSize,
                                          (UChar **)&sDstSlot,
                                          &sKeyOffset,
                                          1 )
                  != IDE_SUCCESS );
        IDE_DASSERT( sAllowedSize >= sKeyLen );

        if ( SDNB_IS_LEAF_NODE(sSrcNodeHdr) == ID_TRUE )
        {
            sSrcCreateCTS = SDNB_GET_CCTS_NO( sSrcSlot );
            sSrcLimitCTS  = SDNB_GET_LCTS_NO( sSrcSlot );

            sDstCreateCTS = sSrcCreateCTS;
            sDstLimitCTS = sSrcLimitCTS;

            /*
             * Copy Source CTS to Destination Node
             */
            if ( SDN_IS_VALID_CTS( sSrcCreateCTS ) )
            {
                if ( SDNB_GET_STATE( sSrcSlot ) == SDNB_KEY_DEAD )
                {
                    sDstCreateCTS = SDN_CTS_INFINITE;
                }
                else
                {
                    sRc = sdnIndexCTL::allocCTS( aSrcNode,
                                                 sSrcCreateCTS,
                                                 aDstNode,
                                                 &sDstCreateCTS );
                    if ( sRc != IDE_SUCCESS )
                    {
                        ideLog::log( IDE_ERR_0,
                                     "From <%"ID_INT32_FMT"> to <%"ID_INT32_FMT"> : "
                                     "Current sequence number : %"ID_INT32_FMT""
                                     "\nSource create CTS : %"ID_UINT32_FMT""
                                     ", Dest create CTS : %"ID_UINT32_FMT"\n",
                                     aFromSeq, 
                                     aToSeq, 
                                     i,
                                     sSrcCreateCTS, 
                                     sDstCreateCTS );
                        dumpIndexNode( aSrcNode );
                        dumpIndexNode( aDstNode );
                        IDE_ASSERT_MSG( 0, "Error Ouccurs in DRDB Index ID : %"ID_UINT32_FMT, aIndex->mIndexID );
                    }
                    else
                    {
                        /* nothing to do */
                    }                            

                    sRc = sdnIndexCTL::bindCTS( aMtx,
                                                aIndex->mIndexTSID,
                                                sKeyOffset,
                                                aSrcNode,
                                                sSrcCreateCTS,
                                                aDstNode,
                                                sDstCreateCTS );
                    if ( sRc != IDE_SUCCESS )
                    {
                        ideLog::log( IDE_ERR_0,
                                     "From <%"ID_INT32_FMT"> to <%"ID_INT32_FMT"> : "
                                     "Current sequence number : %"ID_INT32_FMT""
                                     "\nSource create CTS : %"ID_UINT32_FMT""
                                     ", Dest create CTS : %"ID_UINT32_FMT""
                                     "\nKey Offset : %"ID_UINT32_FMT"\n",
                                     aFromSeq, 
                                     aToSeq, 
                                     i,
                                     sSrcCreateCTS, 
                                     sDstCreateCTS, 
                                     sKeyOffset );
                        dumpIndexNode( aSrcNode );
                        dumpIndexNode( aDstNode );
                        IDE_ASSERT_MSG( 0, "Error Ouccurs in DRDB Index ID : %"ID_UINT32_FMT, aIndex->mIndexID );
                    }
                }
            }

            if ( SDN_IS_VALID_CTS( sSrcLimitCTS ) )
            {
                sRc = sdnIndexCTL::allocCTS( aSrcNode,
                                             sSrcLimitCTS,
                                             aDstNode,
                                             &sDstLimitCTS );
                if ( sRc != IDE_SUCCESS )
                {
                    ideLog::log( IDE_ERR_0,
                                 "From <%"ID_INT32_FMT"> to <%"ID_INT32_FMT"> : "
                                 "Current sequence number : %"ID_INT32_FMT""
                                 "\nSource create CTS : %"ID_UINT32_FMT""
                                 ", Dest create CTS : %"ID_UINT32_FMT"\n",
                                 aFromSeq, 
                                 aToSeq, 
                                 i,
                                 sSrcCreateCTS, 
                                 sDstCreateCTS );
                    dumpIndexNode( aSrcNode );
                    dumpIndexNode( aDstNode );
                    IDE_ASSERT_MSG( 0, "Error Ouccurs in DRDB Index ID : %"ID_UINT32_FMT, aIndex->mIndexID );
                }
                else
                {
                    /* nothing to do */
                }                            

                sRc = sdnIndexCTL::bindCTS( aMtx,
                                            aIndex->mIndexTSID,
                                            sKeyOffset,
                                            aSrcNode,
                                            sSrcLimitCTS,
                                            aDstNode,
                                            sDstLimitCTS );
                if ( sRc != IDE_SUCCESS )
                {
                    ideLog::log( IDE_ERR_0,
                                 "From <%"ID_INT32_FMT"> to <%"ID_INT32_FMT"> : "
                                 "Current sequence number : %"ID_INT32_FMT""
                                 "\nSource create CTS : %"ID_UINT32_FMT""
                                 ", Dest create CTS : %"ID_UINT32_FMT""
                                 "\nKey Offset : %"ID_UINT32_FMT"\n",
                                 aFromSeq, 
                                 aToSeq, 
                                 i,
                                 sSrcCreateCTS, 
                                 sDstCreateCTS, 
                                 sKeyOffset );
                    dumpIndexNode( aSrcNode );
                    dumpIndexNode( aDstNode );
                    IDE_ASSERT_MSG( 0, "Error Ouccurs in DRDB Index ID : %"ID_UINT32_FMT, aIndex->mIndexID );
                }
            }

            idlOS::memcpy( (UChar *)sDstSlot, 
                           (UChar *)sSrcSlot, 
                           sKeyLen );

            SDNB_SET_CCTS_NO( sDstSlot, sDstCreateCTS );
            SDNB_SET_LCTS_NO( sDstSlot, sDstLimitCTS );

            if ( ( SDNB_GET_STATE( sDstSlot ) == SDNB_KEY_UNSTABLE ) ||
                 ( SDNB_GET_STATE( sDstSlot ) == SDNB_KEY_STABLE ) )
            {
                sUnlimitedKeyCount++;
            }
            else
            {
                /* nothing to do */
            }                            

            // BUG-29538 split�� TBK count�� �������� �ʰ� �ֽ��ϴ�.
            if ( SDNB_GET_TB_TYPE( sSrcSlot ) == SDNB_KEY_TB_KEY )
            {
                /* BUG-42141
                 * [INC-30651] ��ȭ���� �м� ���� ����� �ڵ� �߰�. */
                if ( sSrcTBKCount == 0 )
                {
                    ideLog::log( IDE_SERVER_0,
                                 "INDEX ID : %"ID_UINT32_FMT"\n"
                                 "From <%"ID_INT32_FMT"> to <%"ID_INT32_FMT"> : "
                                 "Current sequence number : %"ID_INT32_FMT
                                 "\nSource create CTS : %"ID_UINT32_FMT
                                 ", Dest create CTS : %"ID_UINT32_FMT"\n"
                                 "mTBKCount : %"ID_UINT32_FMT"\n",
                                 aIndex->mIndexID,
                                 aFromSeq, aToSeq, i,
                                 sSrcCreateCTS, sDstCreateCTS,
                                 sSrcNodeHdr->mTBKCount );

                    /* BUG-42286
                     * �ε��� ��� ����� mTBKCount���� ���� TBK Slot������ �����ʾ� ASSERT�Ǵ� ������ �־���.
                     * �������� mTBKCount���� �����ϴ� �ڵ尡 ������ ������ ���δ�.
                     * �׷���
                     * mTBKCount���� �����ϴ� �ڵ�� �״�� �ΰ� ����ϴ� �ڵ�� �����Ѵ�.
                     * (mTBKCount���� ����ϴ� ���� sdnbBTree::nodeAging()�Լ����̴�.) */
                }
                else
                {
                    sSrcTBKCount--;
                }
            }
            else
            {
                /* nothing to do */
            }                            
        }
        else
        {
            idlOS::memcpy( (UChar *)sDstSlot, (UChar *)sSrcSlot, sKeyLen );
        }
        // write log
        IDE_TEST( sdrMiniTrans::writeLogRec( aMtx,
                                             (UChar *)sDstSlot,
                                             (void *)NULL,
                                             ID_SIZEOF(SShort)+sKeyLen,
                                             SDR_SDN_INSERT_INDEX_KEY )
                  != IDE_SUCCESS );
        IDE_TEST( sdrMiniTrans::write( aMtx,
                                       (void *)&sDstSeq,
                                       ID_SIZEOF(SShort) )
                  != IDE_SUCCESS );
        IDE_TEST( sdrMiniTrans::write( aMtx,
                                       (void *)sDstSlot,
                                       sKeyLen)
                  != IDE_SUCCESS );
    }

    IDE_TEST( sdrMiniTrans::writeNBytes( aMtx,
                                         (UChar *)&sDstNodeHdr->mUnlimitedKeyCount,
                                         (void *)&sUnlimitedKeyCount,
                                         ID_SIZEOF(sUnlimitedKeyCount) )
             != IDE_SUCCESS );

    // BUG-29538 split�� TBK count�� �������� �ʰ� �ֽ��ϴ�.
    // Destination leaf node�� header�� TBK count�� �����ϰ� �α�
    // redistribution�� ��� dst node�� �����ϴ� TBK count�� �Բ� ����.
    IDE_ASSERT_MSG( sSrcNodeHdr->mTBKCount >= sSrcTBKCount,
                    "Error Ouccurs in DRDB Index ID : %"ID_UINT32_FMT, aIndex->mIndexID );
    sDstTBKCount = sDstNodeHdr->mTBKCount + sSrcNodeHdr->mTBKCount - sSrcTBKCount;

    IDE_TEST( sdrMiniTrans::writeNBytes( aMtx,
                                         (UChar *)&sDstNodeHdr->mTBKCount,
                                         (void *)&sDstTBKCount,
                                         ID_SIZEOF(sDstTBKCount))
              != IDE_SUCCESS );

    /*
     * 2. Unbind Source Key
     */
    for ( i = aToSeq ; i >= aFromSeq ; i-- )
    {
        IDE_TEST( sdpSlotDirectory::getPagePtrFromSlotNum( sSlotDirPtr, 
                                                           i,
                                                           (UChar **)&sSrcSlot )
                  != IDE_SUCCESS );
        IDE_TEST( sdpSlotDirectory::getValue( sSlotDirPtr, i, &sKeyOffset )
                  != IDE_SUCCESS );

        if ( SDNB_IS_LEAF_NODE(sSrcNodeHdr) == ID_TRUE )
        {
            if ( SDN_IS_VALID_CTS( SDNB_GET_CCTS_NO( sSrcSlot ) ) )
            {
                IDE_TEST( sdnIndexCTL::unbindCTS( aMtx,
                                                  aSrcNode,
                                                  SDNB_GET_CCTS_NO( sSrcSlot ),
                                                  sKeyOffset )
                          != IDE_SUCCESS );
            }

            if ( SDN_IS_VALID_CTS( SDNB_GET_LCTS_NO( sSrcSlot ) ) )
            {
                IDE_TEST( sdnIndexCTL::unbindCTS( aMtx,
                                                  aSrcNode,
                                                  SDNB_GET_LCTS_NO( sSrcSlot ),
                                                  sKeyOffset )
                          != IDE_SUCCESS );
            }
        }
    }

    /*
     * 3. Free Source Key and Adjust UnlimitedKeyCount
     */
    IDE_TEST( sdnbBTree::writeLogFreeKeys( aMtx,
                                           (UChar *)aSrcNode,
                                           aIndex,
                                           aFromSeq,
                                           aToSeq )
             != IDE_SUCCESS );

    sUnlimitedKeyCount = sSrcNodeHdr->mUnlimitedKeyCount;

    for ( i = aToSeq ; i >= aFromSeq ; i-- )
    {
        IDE_TEST( sdpSlotDirectory::getPagePtrFromSlotNum( sSlotDirPtr, 
                                                           i,
                                                           (UChar **)&sSrcSlot )
                  != IDE_SUCCESS );
        IDE_TEST( sdpSlotDirectory::getValue( sSlotDirPtr, i, &sKeyOffset )
                  != IDE_SUCCESS );

        sKeyLen = getKeyLength( &(aIndex->mColLenInfoList),
                                (UChar *)sSrcSlot,
                                (aHeight == 0) ? ID_TRUE : ID_FALSE );

        if ( ( SDNB_GET_STATE( sSrcSlot ) == SDNB_KEY_UNSTABLE ) ||
             ( SDNB_GET_STATE( sSrcSlot ) == SDNB_KEY_STABLE ) )
        {
            sUnlimitedKeyCount--;
        }
        else
        {
            /* nothing to do */
        }                            

        IDE_TEST( sdpPhyPage::freeSlot( aSrcNode,
                                        i,
                                        sKeyLen,
                                        1 )
                  != IDE_SUCCESS );
    }

    IDE_TEST( sdrMiniTrans::writeNBytes( aMtx,
                                         (UChar *)&sSrcNodeHdr->mUnlimitedKeyCount,
                                         (void *)&sUnlimitedKeyCount,
                                         ID_SIZEOF(sUnlimitedKeyCount) )
             != IDE_SUCCESS );

    // BUG-29538 split�� TBK count�� �������� �ʰ� �ֽ��ϴ�.
    // Source leaf node�� header�� TBK count�� �����ϰ� �α�
    IDE_TEST( sdrMiniTrans::writeNBytes( aMtx,
                                         (UChar *)&sSrcNodeHdr->mTBKCount,
                                         (void *)&sSrcTBKCount,
                                         ID_SIZEOF(sSrcTBKCount) )
              != IDE_SUCCESS );

    IDE_DASSERT( validateNodeSpace( aIndex,
                                    aSrcNode,
                                    SDNB_IS_LEAF_NODE(sSrcNodeHdr) )
                 == IDE_SUCCESS );

    IDE_DASSERT( validateNodeSpace( aIndex,
                                    aDstNode,
                                    SDNB_IS_LEAF_NODE(sDstNodeHdr) )
                 == IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

/*********************************************************************
 * FUNCTION DESCRIPTION : sdnbBTree::writeLogFreeKeys                *
 * ------------------------------------------------------------------*
 * �α���� ���̱� ���ؼ� FREE KEY���� logical log�� ����Ѵ�.       *
 *********************************************************************/
IDE_RC sdnbBTree::writeLogFreeKeys( sdrMtx        *aMtx,
                                    UChar         *aPagePtr,
                                    sdnbHeader    *aIndex,
                                    UShort         aFromSeq,
                                    UShort         aToSeq )
{
    UChar     * sSlotDirPtr;
    UChar     * sSlotOffset;

    sSlotDirPtr = sdpPhyPage::getSlotDirStartPtr( aPagePtr );

    if ( aFromSeq <= aToSeq )
    {
        IDE_TEST( sdpSlotDirectory::getPagePtrFromSlotNum( sSlotDirPtr,
                                                           aFromSeq,
                                                           &sSlotOffset )
                  !=IDE_SUCCESS );
        IDE_TEST( sdrMiniTrans::writeLogRec( aMtx,
                                             sSlotOffset,
                                             NULL,
                                             (ID_SIZEOF(UShort) * 2) + SDNB_COLLENINFO_LIST_SIZE( aIndex->mColLenInfoList ),
                                             SDR_SDN_FREE_KEYS )
                  != IDE_SUCCESS );
        IDE_TEST( sdrMiniTrans::write( aMtx,
                                       (void *)&(aIndex->mColLenInfoList),
                                       SDNB_COLLENINFO_LIST_SIZE( aIndex->mColLenInfoList ) )
                  != IDE_SUCCESS );
        IDE_TEST( sdrMiniTrans::write( aMtx,
                                       (void *)&aFromSeq,
                                       ID_SIZEOF(SShort) )
                  != IDE_SUCCESS );
        IDE_TEST( sdrMiniTrans::write( aMtx,
                                       (void *)&aToSeq,
                                       ID_SIZEOF(SShort) )
                  != IDE_SUCCESS );
    }
    else
    {
        /* nothing to do */
    }                            

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : sdnbBTree::insertIKey                     *
 * ------------------------------------------------------------------*
 * Internal node�� aNode �� slot�� insert�Ѵ�.                       *
 *********************************************************************/
IDE_RC sdnbBTree::insertIKey( idvSQL          * aStatistics,
                              sdrMtx          * aMtx,
                              sdnbHeader      * aIndex,
                              sdpPhyPageHdr   * aNode,
                              SShort            aSlotSeq,
                              sdnbKeyInfo     * aKeyInfo,
                              UShort            aKeyValueLen,
                              scPageID          aRightChildPID,
                              idBool            aIsNeedLogging )
{

    UShort        sAllowedSize;
#if DEBUG
    sdnbStatistic sIndexStat;
#endif
    scOffset      sSlotOffset;
    sdnbIKey *    sIKey = NULL;
    UShort        sKeyLength = SDNB_IKEY_LEN( aKeyValueLen );

    ACP_UNUSED( aStatistics );

    IDE_TEST( sdnbBTree::canAllocInternalKey( aMtx,
                                              aIndex,
                                              aNode,
                                              sKeyLength,
                                              ID_TRUE, //aExecCompact
                                              aIsNeedLogging ) != IDE_SUCCESS );

    IDE_TEST( sdpPhyPage::allocSlot( (sdpPhyPageHdr*)aNode,
                                     aSlotSeq,
                                     sKeyLength,
                                     ID_TRUE,
                                     &sAllowedSize,
                                     (UChar **)&sIKey,
                                     &sSlotOffset,
                                     1 )
              != IDE_SUCCESS );
    IDE_DASSERT( sAllowedSize >= sKeyLength );

    SDNB_KEYINFO_TO_IKEY( *aKeyInfo, 
                          aRightChildPID, 
                          aKeyValueLen, 
                          sIKey );

    // write log
    if ( aIsNeedLogging == ID_TRUE )
    {
        IDE_TEST( sdrMiniTrans::writeLogRec( aMtx,
                                             (UChar *)sIKey,
                                             (void *)NULL,
                                             ID_SIZEOF(SShort)+sKeyLength,
                                             SDR_SDN_INSERT_INDEX_KEY )
                  != IDE_SUCCESS );

        IDE_TEST( sdrMiniTrans::write( aMtx,
                                       (void *)&aSlotSeq,
                                       ID_SIZEOF(SShort) )
                  != IDE_SUCCESS );

        IDE_TEST( sdrMiniTrans::write( aMtx,
                                       (void *)sIKey,
                                       sKeyLength)
                  != IDE_SUCCESS );
    }
    else
    {
        /* nothing to do */
    }

#if DEBUG
    /* BUG-39926 [PROJ-2506] Insure++ Warning
     * - �ʱ�ȭ�� �ʿ��մϴ�.
     */
    idlOS::memset( &sIndexStat, 0, ID_SIZEOF(sIndexStat) );
#endif

    IDE_DASSERT( validateInternal( aNode, 
                                   &sIndexStat,
                                   aIndex->mColumns, 
                                   aIndex->mColumnFence )
                 == IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

/*********************************************************************
 * FUNCTION DESCRIPTION : sdnbBTree::deleteIKey                     *
 * ------------------------------------------------------------------*
 * Internal node�� aNode���� slot seq�� delete �Ѵ�.                 *
 *********************************************************************/
IDE_RC sdnbBTree::deleteIKey( idvSQL *          aStatistics,
                              sdrMtx *          aMtx,
                              sdnbHeader *      aIndex,
                              sdpPhyPageHdr *   aNode,
                              SShort            aSlotSeq,
                              idBool            aIsNeedLogging,
                              scPageID          aChildPID)
{
    sdnbNodeHdr   * sNodeHdr;
#if DEBUG
    sdnbStatistic   sIndexStat;
#endif
    sdnbIKey      * sIKey;
    SShort          sSeq = aSlotSeq;
    UShort          sKeyLen;
    UChar         * sFreeSlot;
    UChar         * sSlotDirPtr;
    scPageID        sChildPIDInKey;

    ACP_UNUSED( aStatistics );

    sNodeHdr = (sdnbNodeHdr*)sdpPhyPage::getLogicalHdrStartPtr( (UChar *)aNode );

    sSlotDirPtr = sdpPhyPage::getSlotDirStartPtr( (UChar *)aNode );
    IDE_TEST( sdpSlotDirectory::getPagePtrFromSlotNum( sSlotDirPtr, 
                                                       0, 
                                                       (UChar **)&sIKey )
              != IDE_SUCCESS );

    if ( sSeq == -1 )
    {
        if (aIsNeedLogging == ID_TRUE)
        {
            IDE_TEST( sdrMiniTrans::writeNBytes(
                                         aMtx,
                                         (UChar *)&sNodeHdr->mLeftmostChild,
                                         (void *)&sIKey->mRightChild,
                                         ID_SIZEOF(sIKey->mRightChild) ) 
                      != IDE_SUCCESS );
        }
        else
        {
            idlOS::memcpy( &sNodeHdr->mLeftmostChild,
                           &sIKey->mRightChild,
                           ID_SIZEOF(sIKey->mRightChild) );
        }
        sSeq = 0;
    }

    IDE_TEST( sdpSlotDirectory::getPagePtrFromSlotNum( sSlotDirPtr,
                                                       sSeq,
                                                       &sFreeSlot )
              != IDE_SUCCESS );
    sKeyLen = getKeyLength( &(aIndex->mColLenInfoList),
                            (UChar *)sFreeSlot,
                            ID_FALSE /* aIsLeaf */ );

    /* BUG-27255 Leaf Node�� Tree�� �Ŵ޸�ü EmptyNodeList�� �Ŵ޸��� ����
     *���� ���� üũ�� �ʿ��մϴ�.
     *
     * ������ϴ� InternalSlot�� ����Ű�� ChildPID�� ������ ��������
     * Child-Node�� PID�� ��ġ�ϴ��� Ȯ���Ѵ�.
     * ��, Simulation�ϴ� ���(childPID�� NullPID�� ���)�� �����Ѵ�.*/
    sIKey = (sdnbIKey*)sFreeSlot;
    SDNB_GET_RIGHT_CHILD_PID( &sChildPIDInKey, sIKey );
    if ( ( aChildPID != sChildPIDInKey ) &&
         ( aChildPID != SC_NULL_PID ) )
    {
        ideLog::log( IDE_ERR_0,
                     "Slot sequence number : %"ID_INT32_FMT""
                     "\nChild PID : %"ID_UINT32_FMT", Child PID in Key : %"ID_UINT32_FMT"\n",
                     aSlotSeq, 
                     aChildPID, 
                     sChildPIDInKey );
        dumpIndexNode( aNode );
        IDE_ASSERT_MSG( 0, "Error Ouccurs in DRDB Index ID : %"ID_UINT32_FMT, aIndex->mIndexID );
    }
    else
    {
        /* nothing to do */
    }

    if ( aIsNeedLogging == ID_TRUE )
    {
        IDE_TEST( sdrMiniTrans::writeLogRec( aMtx,
                                             (UChar *)sFreeSlot,
                                             (void *)&sKeyLen,
                                             ID_SIZEOF(UShort),
                                             SDR_SDN_FREE_INDEX_KEY )
                  != IDE_SUCCESS );
    }
    else
    {
        /* nothing to do */
    }

    IDE_TEST( sdpPhyPage::freeSlot( aNode,
                                    sSeq,
                                    sKeyLen,
                                    1 )
              != IDE_SUCCESS );

#if DEBUG
    /* BUG-39926 [PROJ-2506] Insure++ Warning
     * - �ʱ�ȭ�� �ʿ��մϴ�.
     */
    idlOS::memset( &sIndexStat, 0, ID_SIZEOF(sIndexStat) );
#endif

    IDE_DASSERT( validateInternal( aNode,
                                   &sIndexStat,
                                   aIndex->mColumns,
                                   aIndex->mColumnFence )
                 == IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

/*********************************************************************
 * FUNCTION DESCRIPTION : sdnbBTree::updateAndInsertIKey            *
 * ------------------------------------------------------------------*
 * Mode�� ����, Slot�� Key�� Update �ϰų�, insert �Ѵ�.           *
 *********************************************************************/
IDE_RC sdnbBTree::updateAndInsertIKey(idvSQL *          aStatistics,
                                      sdrMtx *          aMtx,
                                      sdnbHeader *      aIndex,
                                      sdpPhyPageHdr *   aNode,
                                      SShort            aSlotSeq,
                                      sdnbKeyInfo *     aKeyInfo,
                                      UShort *          aKeyValueLen,
                                      scPageID          aRightChildPID,
                                      UInt              aMode,
                                      idBool            aIsNeedLogging)
{
    sdnbIKey *      sIKey;
    scPageID        sUpdateChildPID;
    UChar         * sSlotDirPtr;

    switch ( aMode )
    {
        case SDNB_SMO_MODE_KEY_REDIST:
        {
            sSlotDirPtr = sdpPhyPage::getSlotDirStartPtr( (UChar *)aNode );
            IDE_TEST( sdpSlotDirectory::getPagePtrFromSlotNum(
                                                            sSlotDirPtr, 
                                                            aSlotSeq,
                                                            (UChar **)&sIKey )
                      != IDE_SUCCESS );

            SDNB_GET_RIGHT_CHILD_PID( &sUpdateChildPID, sIKey );

            IDE_TEST( deleteIKey( aStatistics, 
                                  aMtx, 
                                  aIndex, 
                                  aNode, 
                                  aSlotSeq, 
                                  aIsNeedLogging,
                                  sUpdateChildPID)
                      != IDE_SUCCESS );

            IDE_TEST( insertIKey( aStatistics, 
                                  aMtx, 
                                  aIndex, 
                                  aNode, 
                                  aSlotSeq,
                                  &aKeyInfo[0], 
                                  aKeyValueLen[0], 
                                  sUpdateChildPID,
                                  aIsNeedLogging )
                      != IDE_SUCCESS );
        }
        break;

        case SDNB_SMO_MODE_SPLIT_1_2:
        {
            IDE_TEST( insertIKey( aStatistics, 
                                  aMtx, 
                                  aIndex, 
                                  aNode, 
                                  aSlotSeq,
                                  &aKeyInfo[0], 
                                  aKeyValueLen[0], 
                                  aRightChildPID,
                                  aIsNeedLogging )
                      != IDE_SUCCESS );
        }
        break;

        default:
            break;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : sdnbBTree::compactPage                     *
 * ------------------------------------------------------------------*
 * SMO �� �߻��� page split�� ���� ���� ��带 compact�Ѵ�.          *
 * Logging�� �� �� ���� �����Լ��� ȣ���Ѵ�.                         *
 *********************************************************************/
IDE_RC sdnbBTree::compactPage( sdnbHeader *    aIndex,
                               sdrMtx *        aMtx,
                               sdpPhyPageHdr * aPage,
                               idBool          aIsLogging )
{

    sdnbNodeHdr *  sNodeHdr;

    sNodeHdr = (sdnbNodeHdr*)sdpPhyPage::getLogicalHdrStartPtr( (UChar *)aPage );

    if ( aIsLogging == ID_TRUE )
    {
        IDE_TEST( sdrMiniTrans::writeLogRec( aMtx,
                                             (UChar *)aPage,
                                             (void *)&(aIndex->mColLenInfoList),
                                             SDNB_COLLENINFO_LIST_SIZE( aIndex->mColLenInfoList ),
                                             SDR_SDN_COMPACT_INDEX_PAGE )
                  != IDE_SUCCESS );
    }

    if ( SDNB_IS_LEAF_NODE(sNodeHdr) == ID_TRUE )
    {
        IDE_TEST( compactPageLeaf( aIndex,
                                   aMtx,
                                   aPage,
                                   &(aIndex->mColLenInfoList))
                  != IDE_SUCCESS );
    }
    else
    {
        IDE_TEST( compactPageInternal( aIndex,
                                       aPage,
                                       &(aIndex->mColLenInfoList) )
                  != IDE_SUCCESS );
    }

    IDE_DASSERT( validateNodeSpace( aIndex,
                                    aPage,
                                    SDNB_IS_LEAF_NODE(sNodeHdr) )
                 == IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

/*********************************************************************
 * FUNCTION DESCRIPTION : sdnbBTree::selfAging                       *
 * ------------------------------------------------------------------*
 * Transaction�� OldestSCN���� ���� CommitSCN�� ���� CTS�� ���ؼ�    *
 * Soft Key Stamping(Aging)�� �����Ѵ�.                              *
 *********************************************************************/
IDE_RC sdnbBTree::selfAging( idvSQL        * aStatistics,
                             sdnbHeader    * aIndex,
                             sdrMtx *        aMtx,
                             sdpPhyPageHdr * aPage,
                             UChar         * aAgedCount )
{
    sdnCTL *       sCTL;
    sdnCTS *       sCTS;
    UInt           i;
    smSCN          sCommitSCN;
    smSCN          sSysMinDskViewSCN;
    UChar          sAgedCount        = 0;
    UShort         sTBKStampingCount = 0;
    sdnbNodeHdr  * sNodeHdr          = NULL;

    SMX_GET_MIN_DISK_VIEW( &sSysMinDskViewSCN );

    sNodeHdr = (sdnbNodeHdr*)sdpPhyPage::getLogicalHdrStartPtr((UChar *)aPage);

    if ( sNodeHdr->mTBKCount > 0 )
    {
        IDE_TEST( allTBKStamping( aStatistics,
                                  aIndex,
                                  aMtx,
                                  aPage,
                                  &sTBKStampingCount ) != IDE_SUCCESS );
    }
    else
    {
        /* nothing to do */
    }

    /* sAgedCount���� 0���� �ƴ����� �߿��ϴ�. */
    sAgedCount = ( ( sTBKStampingCount > 0 )  ? ( sAgedCount + 1 ) : sAgedCount );

    sCTL = sdnIndexCTL::getCTL( aPage );

    for ( i = 0 ; i < sdnIndexCTL::getCount(sCTL) ; i++ )
    {
        sCTS = sdnIndexCTL::getCTS( sCTL, i );

        if ( sdnIndexCTL::getCTSlotState( sCTS ) == SDN_CTS_STAMPED )
        {
            sCommitSCN = sdnIndexCTL::getCommitSCN( sCTS );
            if ( SM_SCN_IS_LT( &sCommitSCN, &sSysMinDskViewSCN ) )
            {
                IDE_TEST( softKeyStamping( aIndex,
                                           aMtx,
                                           aPage,
                                           i )
                          != IDE_SUCCESS );
                sAgedCount++;
            }
            else
            {
                /* nothing to do */
            }
        }
        else
        {
            /* nothing to do */
        }
    }

    *aAgedCount = sAgedCount;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

/*********************************************************************
 * FUNCTION DESCRIPTION : sdnbBTree::softKeyStamping                 *
 * ------------------------------------------------------------------*
 * Internal SoftKeyStamping�� ���� Wrapper Function                  *
 *********************************************************************/
IDE_RC sdnbBTree::softKeyStamping( sdrMtx        * aMtx,
                                   sdpPhyPageHdr * aPage,
                                   UChar           aCTSlotNum,
                                   UChar         * aContext )
{
    sdnbCallbackContext * sContext;

    sContext = (sdnbCallbackContext*)aContext;
    return softKeyStamping( sContext->mIndex,
                            aMtx,
                            aPage,
                            aCTSlotNum );
}

/*********************************************************************
 * FUNCTION DESCRIPTION : sdnbBTree::softKeyStamping                 *
 * ------------------------------------------------------------------*
 * Soft Key Stamping�� CTS�� STAMPED���¿��� ����ȴ�.               *
 * CTS#�� ���� ��� KEY�鿡 ���ؼ� CTS#�� ���Ѵ�� �����Ѵ�.         *
 * ���� CreateCTS�� ���Ѵ�� ����Ǹ� Key�� ���´� STABLE���·�      *
 * ����ǰ�, LimitCTS�� ���Ѵ��� ���� DEAD���·� �����Ų��.       *
 *********************************************************************/
IDE_RC sdnbBTree::softKeyStamping( sdnbHeader    * aIndex,
                                   sdrMtx        * aMtx,
                                   sdpPhyPageHdr * aPage,
                                   UChar           aCTSlotNum )
{
    UShort         sSlotCount;
    sdnbLKey     * sLeafKey;
    UInt           i;
    sdnbNodeHdr  * sNodeHdr;
    UShort         sKeyLen;
    UShort         sTotalDeadKeySize = 0;
    UShort       * sArrRefKey;
    UShort         sRefKeyCount;
    UShort         sAffectedKeyCount = 0;
    UChar        * sSlotDirPtr;
    UShort         sDeadTBKCount    = 0;
    UShort         sTotalTBKCount   = 0;

    sNodeHdr = (sdnbNodeHdr*)sdpPhyPage::getLogicalHdrStartPtr((UChar *)aPage);
    sTotalDeadKeySize = sNodeHdr->mTotalDeadKeySize;
    IDE_TEST( sdrMiniTrans::writeLogRec( aMtx,
                                         (UChar *)aPage,
                                         NULL,
                                         ID_SIZEOF(UChar) + SDNB_COLLENINFO_LIST_SIZE( aIndex->mColLenInfoList ),
                                         SDR_SDN_KEY_STAMPING )
              != IDE_SUCCESS );

    IDE_TEST( sdrMiniTrans::write( aMtx,
                                   (void *)&aCTSlotNum,
                                   ID_SIZEOF(UChar) )
              != IDE_SUCCESS );

    IDE_TEST( sdrMiniTrans::write( aMtx,
                                   (void *)&(aIndex->mColLenInfoList),
                                   SDNB_COLLENINFO_LIST_SIZE( aIndex->mColLenInfoList ) )
              != IDE_SUCCESS );

    sSlotDirPtr = sdpPhyPage::getSlotDirStartPtr( (UChar *)aPage );
    sSlotCount  = sdpSlotDirectory::getCount( sSlotDirPtr );

    sdnIndexCTL::getRefKey( aPage,
                            aCTSlotNum,
                            &sRefKeyCount,
                            &sArrRefKey );

    for ( i = 0 ; i < SDN_CTS_MAX_KEY_CACHE ; i++ )
    {
        if ( sArrRefKey[i] == SDN_CTS_KEY_CACHE_NULL )
        {
            continue;
        }

        sAffectedKeyCount++;
        sLeafKey = (sdnbLKey*)(((UChar *)aPage) + sArrRefKey[i]);

        if ( SDNB_GET_CCTS_NO( sLeafKey ) == aCTSlotNum )
        {
            SDNB_SET_CCTS_NO( sLeafKey, SDN_CTS_INFINITE );
            SDNB_SET_DUPLICATED( sLeafKey, SDNB_DUPKEY_NO );
            /*
             * Create CTS�� Stamping�� ���� �ʰ� Limit CTS�� Stamping��
             * ���� DEAD�����ϼ� �ֱ� ������ SKIP�Ѵ�. ����
             * SDNB_KEY_DELETED ���´� �������� �ʴ´�.
             */
            if ( SDNB_GET_STATE( sLeafKey ) == SDNB_KEY_UNSTABLE )
            {
                SDNB_SET_STATE( sLeafKey, SDNB_KEY_STABLE );
            }
        }

        if ( SDNB_GET_LCTS_NO( sLeafKey ) == aCTSlotNum )
        {
            if ( SDNB_GET_STATE( sLeafKey ) != SDNB_KEY_DELETED )
            {
                ideLog::log( IDE_ERR_0,
                             "CTS slot number : %"ID_UINT32_FMT""
                             "\nCTS key cache idx : %"ID_UINT32_FMT"\n",
                             aCTSlotNum, i );
                dumpIndexNode( aPage );
                IDE_ASSERT_MSG( 0, "Error Ouccurs in DRDB Index ID : %"ID_UINT32_FMT, aIndex->mIndexID );
            }

            sKeyLen = getKeyLength( &(aIndex->mColLenInfoList),
                                    (UChar *)sLeafKey,
                                    ID_TRUE /* aIsLeaf */ );
            sTotalDeadKeySize += sKeyLen + ID_SIZEOF( sdpSlotEntry );

            SDNB_SET_CCTS_NO( sLeafKey, SDN_CTS_INFINITE );
            SDNB_SET_LCTS_NO( sLeafKey, SDN_CTS_INFINITE );
            SDNB_SET_STATE( sLeafKey, SDNB_KEY_DEAD );
            SDNB_SET_DUPLICATED( sLeafKey, SDNB_DUPKEY_NO );

            /* BUG-42286 */
            if ( SDNB_GET_TB_TYPE( sLeafKey ) == SDNB_KEY_TB_KEY )
            {
                sDeadTBKCount++;
            }
        }
    }

    if ( sAffectedKeyCount < sRefKeyCount )
    {
        /*
         * full scan�ؼ� Key Stamping�� �Ѵ�.
         */
        for ( i = 0 ; i < sSlotCount ; i++ )
        {
            IDE_TEST( sdpSlotDirectory::getPagePtrFromSlotNum(
                                                        (UChar *)sSlotDirPtr,
                                                        i,
                                                        (UChar **)&sLeafKey )
                      != IDE_SUCCESS );

            if ( SDNB_GET_CCTS_NO( sLeafKey ) == aCTSlotNum )
            {
                SDNB_SET_CCTS_NO( sLeafKey, SDN_CTS_INFINITE );
                SDNB_SET_DUPLICATED( sLeafKey, SDNB_DUPKEY_NO );

                /*
                 * Create CTS�� Stamping�� ���� �ʰ� Limit CTS�� Stamping��
                 * ���� DEAD�����ϼ� �ֱ� ������ SKIP�Ѵ�. ����
                 * SDNB_KEY_DELETED ���´� �������� �ʴ´�.
                 */
                if ( SDNB_GET_STATE( sLeafKey ) == SDNB_KEY_UNSTABLE )
                {
                    SDNB_SET_STATE( sLeafKey, SDNB_KEY_STABLE );
                }
            }

            if ( SDNB_GET_LCTS_NO( sLeafKey ) == aCTSlotNum )
            {
                if ( SDNB_GET_STATE( sLeafKey ) != SDNB_KEY_DELETED )
                {
                    ideLog::log( IDE_ERR_0,
                                 "CTS slot number : %"ID_UINT32_FMT""
                                 "\nslot number : %"ID_UINT32_FMT"\n",
                                 aCTSlotNum, i );
                    dumpIndexNode( aPage );
                    IDE_ASSERT_MSG( 0, "Error Ouccurs in DRDB Index ID : %"ID_UINT32_FMT, aIndex->mIndexID );
                }
                else
                {
                    /* nothing to do */
                }

                sKeyLen = getKeyLength( &(aIndex->mColLenInfoList),
                                        (UChar *)sLeafKey,
                                        ID_TRUE /* aIsLeaf */ );
                sTotalDeadKeySize += sKeyLen + ID_SIZEOF( sdpSlotEntry );

                SDNB_SET_CCTS_NO( sLeafKey, SDN_CTS_INFINITE );
                SDNB_SET_LCTS_NO( sLeafKey, SDN_CTS_INFINITE );
                SDNB_SET_STATE( sLeafKey, SDNB_KEY_DEAD );
                SDNB_SET_DUPLICATED( sLeafKey, SDNB_DUPKEY_NO );

                /* BUG-42286 */
                if ( SDNB_GET_TB_TYPE( sLeafKey ) == SDNB_KEY_TB_KEY )
                {
                    sDeadTBKCount++;
                }
                else
                {
                    /* nothing to do */
                }
            }
            else
            {
                /* nothing to do */
            }
        }
    }

    IDE_TEST(sdrMiniTrans::writeNBytes( aMtx,
                                        (UChar *)&sNodeHdr->mTotalDeadKeySize,
                                        (void *)&sTotalDeadKeySize,
                                        ID_SIZEOF(sNodeHdr->mTotalDeadKeySize) )
             != IDE_SUCCESS );

    /* BUG-42286
     * mTBKCount���� ���ҽ�Ű�� �ڵ尡 �����Ǿ� �߰���. */
    if ( sDeadTBKCount > 0 )
    {
        sTotalTBKCount = sNodeHdr->mTBKCount - sDeadTBKCount;

        IDE_TEST( sdrMiniTrans::writeNBytes( aMtx,
                                             (UChar *)&sNodeHdr->mTBKCount,
                                             (void *)&sTotalTBKCount,
                                             ID_SIZEOF(UShort) )
                  != IDE_SUCCESS );
    }
    else
    {
        /* nothing to do */
    }

    IDE_TEST( sdnIndexCTL::freeCTS( aMtx,
                                    aPage,
                                    aCTSlotNum,
                                    ID_TRUE )
              != IDE_SUCCESS );

#if DEBUG
    for ( i = 0 ; i < sSlotCount ; i++ )
    {
        IDE_TEST( sdpSlotDirectory::getPagePtrFromSlotNum( (UChar *)sSlotDirPtr,
                                                           i,
                                                           (UChar **)&sLeafKey )
                  != IDE_SUCCESS );

        if ( SDNB_GET_STATE( sLeafKey ) == SDNB_KEY_DEAD )
        {
            continue;
        }

        /*
         * SoftKeyStamping�� �ߴµ��� CTS#�� ������� ���� Key�� ������
         * ����.
         */
        if ( (SDNB_GET_CCTS_NO( sLeafKey ) == aCTSlotNum) ||
             (SDNB_GET_LCTS_NO( sLeafKey ) == aCTSlotNum) )
        {
            ideLog::log( IDE_ERR_0,
                         "CTS slot number : %"ID_UINT32_FMT""
                         "\nslot number : %"ID_UINT32_FMT"\n",
                         aCTSlotNum, i );
            dumpIndexNode( aPage );
            IDE_ASSERT( 0 );
        }
        else
        {
            /* nothing to do */
        }
    }
#endif

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

/*********************************************************************
 * FUNCTION DESCRIPTION : sdnbBTree::hardKeyStamping                 *
 * ------------------------------------------------------------------*
 * Internal HardKeyStamping�� ���� Wrapper Function                  *
 *********************************************************************/
IDE_RC sdnbBTree::hardKeyStamping( idvSQL        * aStatistics,
                                   sdrMtx        * aMtx,
                                   sdpPhyPageHdr * aPage,
                                   UChar           aCTSlotNum,
                                   UChar         * aContext,
                                   idBool        * aSuccess )
{
    sdnbCallbackContext * sContext = (sdnbCallbackContext*)aContext;

    IDE_TEST( hardKeyStamping( aStatistics,
                               sContext->mIndex,
                               aMtx,
                               aPage,
                               aCTSlotNum,
                               aSuccess )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

/*********************************************************************
 * FUNCTION DESCRIPTION : sdnbBTree::hardKeyStamping                 *
 * ------------------------------------------------------------------*
 * �ʿ��ϴٸ� TSS�� ���� CommitSCN�� ���ؿͼ�, SoftKeyStamping��     *
 * �õ� �Ѵ�.                                                        *
 *********************************************************************/
IDE_RC sdnbBTree::hardKeyStamping( idvSQL        * aStatistics,
                                   sdnbHeader    * aIndex,
                                   sdrMtx        * aMtx,
                                   sdpPhyPageHdr * aPage,
                                   UChar           aCTSlotNum,
                                   idBool        * aSuccess )
{
    sdnCTL  * sCTL;
    sdnCTS  * sCTS;
    idBool    sSuccess = ID_TRUE;
    smSCN     sSysMinDskViewSCN;
    smSCN     sCommitSCN;

    sCTL = sdnIndexCTL::getCTL( aPage );
    sCTS = sdnIndexCTL::getCTS( sCTL, aCTSlotNum );

    if ( sCTS->mState == SDN_CTS_UNCOMMITTED )
    {
        IDE_TEST( sdnIndexCTL::delayedStamping( aStatistics,
                                                NULL, /*sdrMiniTrans::getTrans( aMtx )*/
                                                sCTS,
                                                SDB_SINGLE_PAGE_READ,
                                                SM_SCN_INIT, /* aStmtViewSCN */ 
                                                &sCommitSCN,
                                                &sSuccess )
                  != IDE_SUCCESS );
    }

    if ( sSuccess == ID_TRUE )
    {
        IDE_DASSERT( sdnIndexCTL::getCTSlotState( sCTS ) == SDN_CTS_STAMPED );

        SMX_GET_MIN_DISK_VIEW( &sSysMinDskViewSCN );

        sCommitSCN = sdnIndexCTL::getCommitSCN( sCTS );

        /* BUG-38962
         * sSysMinDskSCN�� �ʱⰪ�̸�, restart recovery ���� ���� �ǹ��Ѵ�. */
        if ( SM_SCN_IS_LT( &sCommitSCN, &sSysMinDskViewSCN ) ||
             SM_SCN_IS_INIT( sSysMinDskViewSCN ) )
        {
            IDE_TEST( softKeyStamping( aIndex,
                                       aMtx,
                                       aPage,
                                       aCTSlotNum )
                      != IDE_SUCCESS );
            *aSuccess = ID_TRUE;
        }
        else
        {
            *aSuccess = ID_FALSE;
        }
    }
    else
    {
        *aSuccess = ID_FALSE;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

/*********************************************************************
 * FUNCTION DESCRIPTION : sdnbBTree::compactPageInternal             *
 * ------------------------------------------------------------------*
 * SMO �� �߻��� page split�� ���� ���� ��带 compact�Ѵ�.          *
 *********************************************************************/
IDE_RC sdnbBTree::compactPageInternal( sdnbHeader         * aIndex,
                                       sdpPhyPageHdr      * aPage,
                                       sdnbColLenInfoList * aColLenInfoList )
{

    SInt           i;
    UShort         sTmpBuf[SD_PAGE_SIZE]; // 2 * Page size -> align ����...
    UChar *        sTmpPage;
    UChar        * sSlotDirPtr;
    UShort         sKeyLength;
    UShort         sAllowedSize;
    scOffset       sSlotOffset;
    UChar *        sSrcSlot;
    UChar *        sDstSlot;
    UShort         sSlotCount;

    sSlotDirPtr = sdpPhyPage::getSlotDirStartPtr( (UChar *)aPage );
    sSlotCount  = sdpSlotDirectory::getCount( sSlotDirPtr );

    sTmpPage = ((vULong)sTmpBuf) % SD_PAGE_SIZE == 0 ?  (UChar *)sTmpBuf
        : (UChar *)sTmpBuf + SD_PAGE_SIZE - (((vULong)sTmpBuf) % SD_PAGE_SIZE);

    idlOS::memcpy(sTmpPage, aPage, SD_PAGE_SIZE);
    if ( aPage->mPageID != ((sdpPhyPageHdr*)sTmpPage)->mPageID )
    {
        dumpIndexNode( aPage );
        dumpIndexNode( (sdpPhyPageHdr *)sTmpPage );

        if ( aIndex != NULL )
        {
            IDE_ASSERT_MSG( 0, "Error Ouccurs in DRDB Index ID : %"ID_UINT32_FMT, aIndex->mIndexID );
        }
        else
        {
            IDE_ASSERT( 0 );
        }
    }
    else
    {
        /* nothing to do */
    }

    (void)sdpPhyPage::reset( aPage,
                             ID_SIZEOF(sdnbNodeHdr),
                             NULL );

    sdpSlotDirectory::init( aPage );
    sSlotDirPtr = sdpPhyPage::getSlotDirStartPtr( sTmpPage );

    for ( i = 0 ; i < sSlotCount ; i++ )
    {
        IDE_ERROR( sdpSlotDirectory::getPagePtrFromSlotNum( sSlotDirPtr, 
                                                            i, 
                                                            &sSrcSlot )
                   == IDE_SUCCESS );

        sKeyLength = getKeyLength( aColLenInfoList,
                                   sSrcSlot,
                                   ID_FALSE ); //aIsLeaf

        IDE_ERROR( sdpPhyPage::allocSlot( (sdpPhyPageHdr*)aPage,
                                          i,
                                          sKeyLength,
                                          ID_TRUE,
                                          &sAllowedSize,
                                          &sDstSlot,
                                          &sSlotOffset,
                                          1 )
                    == IDE_SUCCESS );
        idlOS::memcpy( sDstSlot,
                       sSrcSlot,
                       sKeyLength);
        // Insert Logging�� �ʿ� ����.
    }
    
    return IDE_SUCCESS;
    
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : sdnbBTree::compactPageLeaf                 *
 * ------------------------------------------------------------------*
 * SMO �� �߻��� page split�� ���� ���� ��带 compact�Ѵ�.          *
 * Compaction���Ŀ� ���� CTS.refKey ������ invalid�ϱ� ������ �̸�   *
 * �������� �ʿ䰡 �ִ�.                                             *
 *********************************************************************/
IDE_RC sdnbBTree::compactPageLeaf( sdnbHeader         * aIndex,
                                   sdrMtx             * aMtx,
                                   sdpPhyPageHdr      * aPage,
                                   sdnbColLenInfoList * aColLenInfoList)
{
    SInt           i;
    UShort         sTmpBuf[SD_PAGE_SIZE]; // 2 * Page size -> align ����...
    UChar        * sTmpPage;
    UChar        * sSlotDirPtr;
    UShort         sKeyLength;
    UShort         sAllowedSize;
    scOffset       sSlotOffset;
    UChar        * sSrcSlot;
    UChar        * sDstSlot;
    UShort         sSlotCount;
    sdnbLKey     * sLeafKey;
    UChar          sCreateCTS;
    UChar          sLimitCTS;
    sdnbNodeHdr  * sNodeHdr;
    SShort         sSlotSeq;
    UChar        * sDummy;
    UShort         sTBKCount = 0; /* mTBKCount �������ؼ� (BUG-44973) */

    sSlotDirPtr = sdpPhyPage::getSlotDirStartPtr( (UChar *)aPage );
    sSlotCount  = sdpSlotDirectory::getCount( sSlotDirPtr );
    sNodeHdr    = (sdnbNodeHdr*)sdpPhyPage::getLogicalHdrStartPtr( (UChar *)aPage );

    sTmpPage = ((vULong)sTmpBuf) % SD_PAGE_SIZE == 0 ?  (UChar *)sTmpBuf
        : (UChar *)sTmpBuf + SD_PAGE_SIZE - (((vULong)sTmpBuf) % SD_PAGE_SIZE);

    idlOS::memcpy(sTmpPage, aPage, SD_PAGE_SIZE);
    if ( aPage->mPageID != ((sdpPhyPageHdr*)sTmpPage)->mPageID )
    {
        dumpIndexNode( aPage );
        dumpIndexNode( (sdpPhyPageHdr *)sTmpPage );

        if ( aIndex != NULL )
        {
            IDE_ASSERT_MSG( 0, "Error Ouccurs in DRDB Index ID : %"ID_UINT32_FMT, aIndex->mIndexID );
        }
        else
        {
            IDE_ASSERT( 0 );
        }
    }
    else
    {
        /* nothing to do */
    }

    (void)sdpPhyPage::reset( aPage,
                             ID_SIZEOF(sdnbNodeHdr),
                             NULL );
    /*
     * sdpPhyPage::reset������ CTL�� �ʱ�ȭ �������� �ʴ´�.
     */
    sdpPhyPage::initCTL( aPage,
                         (UInt)sdnIndexCTL::getCTLayerSize(sTmpPage),
                         &sDummy );

    sdpSlotDirectory::init( aPage );

    sdnIndexCTL::cleanAllRefInfo( aPage );

    sNodeHdr->mTotalDeadKeySize = 0;

    sSlotDirPtr = sdpPhyPage::getSlotDirStartPtr( sTmpPage );
    for ( i = 0, sSlotSeq = 0 ; i < sSlotCount ; i++ )
    {
        IDE_ERROR( sdpSlotDirectory::getPagePtrFromSlotNum( sSlotDirPtr, 
                                                            i,
                                                            &sSrcSlot )
                   == IDE_SUCCESS );
        sLeafKey = (sdnbLKey*)sSrcSlot;

        if ( SDNB_GET_STATE( sLeafKey ) == SDNB_KEY_DEAD )
        {
            continue;
        }
        else
        {
            /* nothing to do */
        }

        if ( SDNB_GET_TB_TYPE( sLeafKey ) == SDNB_KEY_TB_KEY )
        {
            sTBKCount++;
        }
        else
        {
            /* nothing to do */
        }

        sKeyLength = getKeyLength( aColLenInfoList,
                                   sSrcSlot,
                                   ID_TRUE ); //aIsLeaf

        IDE_ERROR( sdpPhyPage::allocSlot((sdpPhyPageHdr*)aPage,
                                          sSlotSeq,
                                          sKeyLength,
                                          ID_TRUE,
                                          &sAllowedSize,
                                          &sDstSlot,
                                          &sSlotOffset,
                                          1 )
                    == IDE_SUCCESS );

        sSlotSeq++;

        idlOS::memcpy( sDstSlot,
                       sSrcSlot,
                       sKeyLength);

        sCreateCTS = SDNB_GET_CCTS_NO( sLeafKey );
        sLimitCTS  = SDNB_GET_LCTS_NO( sLeafKey );

        if ( SDN_IS_VALID_CTS( sCreateCTS ) )
        {
            sdnIndexCTL::addRefKey( aPage,
                                    sCreateCTS,
                                    sSlotOffset );
        }

        if ( SDN_IS_VALID_CTS( sLimitCTS ) )
        {
            sdnIndexCTL::addRefKey( aPage,
                                    sLimitCTS,
                                    sSlotOffset );
        }
    }

    /* mTBKCount���� �߸��Ǿ��ִٸ� �����Ѵ�. (BUG-44973) */
    if ( sNodeHdr->mTBKCount != sTBKCount ) 
    {
        if ( aMtx != NULL )
        {
            IDE_TEST( sdrMiniTrans::writeNBytes( aMtx,
                                                 (UChar *)&sNodeHdr->mTBKCount,
                                                 (void *)&sTBKCount,
                                                 ID_SIZEOF(UShort) )
                      != IDE_SUCCESS );
        }
        else
        {
            /* aMtx�� NULL�̸� recovery ���̴�. */
        }
    }
    else
    {
        /* nothing to do */
    }
    
    return IDE_SUCCESS;
    
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : sdnbBTree::propagateKeyInternalNode        *
 * ------------------------------------------------------------------*
 * �Ϻ� ����� �������� ���Ͽ� �߻��ϴ� Key�� ���� ������ �ݿ��Ѵ�.  *
 * Child node�� 1-to-2 split�� �߻��ϰ� �Ǹ� branch key�� insert�ǰ�,*
 * Child node���� Key redistribution�� �߻��ϰ� �Ǵ� ��쿡��        *
 * branch Key�� Update�ȴ�.                                          *
 * �� �Լ��� SMO latch(tree latch)�� x latch�� ���� ���¿� ȣ��Ǹ�, *
 * internal node�� ���� split�� �߻��� ��쵵 �ִ�. �̶��� ��Ȳ��    *
 * ���� Unbalanced split, Key redistribution, balanced 1-to-2 split��*
 * �߻��Ҽ� ������, �̿� ���� Key ���� ���Ĵ� ��� ȣ���� ����       *
 * �̷������.                                                       *
 * ������ �Ϸ�Ǹ� �׷� ���� �Ҵ���� �������� �̿��� ���� �۾���    *
 * �����Ѵ�.                                                         *
 * ��� ȣ���� ���ؼ� ������ �ö󰡴� Key�� ���ؾ� �ϴµ�, �̸� ��� *
 * �� ���ؼ� �˾Ƴ��� ������ ������ �����Ƿ�, ���� �߻��� ������     *
 * try�ؼ� ���� ������ ����, ������ �� ���� ������ �����Ѵ�.         *
 * �ϳ��� mtx���� ������ allocPage�� ������ ������ �� ���⶧����     *
 * SMO�� �ֻ�� ��忡�� �ѹ��� �ʿ��� ������ ��ŭ �Ҵ��ϰ�, ����    *
 * ���鼭 ���� split�� �����Ѵ�.                                     *
 *                                                                   *
 * ù��°Key : aKeyInfo[0], aKeyValueLen[0], aSeq   : delete/insert��*
 * �ι�°Key : aKeyInfo[1], aKeyValueLen[1], aSeq+1 : insert��       *
 *********************************************************************/
IDE_RC sdnbBTree::propagateKeyInternalNode(idvSQL         * aStatistics,
                                           sdnbStatistic  * aIndexStat,
                                           sdrMtx         * aMtx,
                                           idBool         * aMtxStart,
                                           smnIndexHeader * aPersIndex,
                                           sdnbHeader     * aIndex,
                                           sdnbStack      * aStack,
                                           sdnbKeyInfo    * aKeyInfo,
                                           UShort         * aKeyValueLen,
                                           UInt             aMode,
                                           sdpPhyPageHdr ** aNewChildPage,
                                           UInt           * aAllocPageCount )
{
    sdpPhyPageHdr * sTmpPage = NULL;
    sdpPhyPageHdr * sPage;
    scPageID        sPID;
    scPageID        sNewChildPID;
    UShort          sKeyLength;
    SShort          sKeyMapSeq;
    idBool          sIsSuccess;
    idBool          sInsertable = ID_TRUE;
    IDE_RC          sRc;
    ULong           sSmoNo;

    if ( aStack->mCurrentDepth == -1 ) // increase index height
    {
        IDE_TEST( makeNewRootNode( aStatistics,
                                   aIndexStat,
                                   aMtx,
                                   aMtxStart,
                                   aPersIndex,
                                   aIndex,
                                   aStack,
                                   aKeyInfo,
                                   aKeyValueLen[0],
                                   aNewChildPage,
                                   aAllocPageCount )
                  != IDE_SUCCESS );
    }
    else
    {
        sPID = aStack->mStack[aStack->mCurrentDepth].mNode;

        // x latch current internal node
        IDE_DASSERT( sPID != SD_NULL_PID);
        IDE_TEST( sdnbBTree::getPage( aStatistics,
                                      &(aIndexStat->mIndexPage),
                                      aIndex->mIndexTSID,
                                      sPID,
                                      SDB_X_LATCH,
                                      SDB_WAIT_NORMAL,
                                      aMtx,
                                      (UChar **)&sPage,
                                      &sIsSuccess )
                  != IDE_SUCCESS );

        // ���� index SmoNo + 1 �� ��忡 ����
        getSmoNo( (void *)aIndex, &sSmoNo );
        sdpPhyPage::setIndexSMONo( (UChar *)sPage, sSmoNo + 1 );
        IDL_MEM_BARRIER;

        // �� key�� ���� �� ��ġ
        sKeyMapSeq = aStack->mStack[aStack->mCurrentDepth].mKeyMapSeq + 1;
        sKeyLength = SDNB_IKEY_LEN( aKeyValueLen[0] );

        /* Temp page�� ���� simulation�� �� ����. */
        /* sdnbBTree_propagateKeyInternalNode_alloc_TmpPage.tc */
        IDU_FIT_POINT("sdnbBTree::propagateKeyInternalNode::alloc::TmpPage");
        IDE_TEST( smnManager::mDiskTempPagePool.alloc( (void **)&sTmpPage )
                  != IDE_SUCCESS );
        idlOS::memcpy( sTmpPage, 
                       sPage, 
                       SD_PAGE_SIZE );

        // Update�� �ؾ��Ѵٸ�, delete/insert �̹Ƿ�, ������ Key�� delete �Ѵ�.
        if ( aMode == SDNB_SMO_MODE_KEY_REDIST )
        {
            IDE_TEST( deleteIKey( aStatistics,
                                  aMtx,
                                  aIndex,
                                  sTmpPage,
                                  sKeyMapSeq,
                                  ID_FALSE,
                                  SC_NULL_PID ) //simulation
                     != IDE_SUCCESS );
        }
        else
        {
            /* nothing to do */
        }

        // node�� ���� slot ������ �Ҵ�޴´�(sSlot)
        if ( sdnbBTree::canAllocInternalKey( aMtx,
                                             aIndex,
                                             sTmpPage,
                                             sKeyLength,
                                             ID_FALSE, //aExecCompcat
                                             ID_FALSE ) //aIsLogging
           != IDE_SUCCESS )
        {
            sInsertable = ID_FALSE;
        }
        else
        {
            /* nothing to do */
        }

        if ( sInsertable != ID_TRUE )
        {
            IDE_TEST(processInternalFull(aStatistics,
                                         aIndexStat,
                                         aMtx,
                                         aMtxStart,
                                         aPersIndex,
                                         aIndex,
                                         aStack,
                                         aKeyInfo,
                                         aKeyValueLen,
                                         sKeyMapSeq,
                                         aMode,
                                         sPage,
                                         aNewChildPage,
                                         aAllocPageCount)
                     != IDE_SUCCESS );
        }
        else
        {
            IDE_TEST( preparePages( aStatistics,
                                    aIndex,
                                    aMtx,
                                    aMtxStart,
                                    aStack->mIndexDepth -
                                    aStack->mCurrentDepth )
                      != IDE_SUCCESS );

            // SMO�� �ֻ�� -- allocPages
            if ( aMode == SDNB_SMO_MODE_SPLIT_1_2 )
            {
                // �� Child ��带 �Ҵ�޴´�.
                IDE_ASSERT( allocPage( aStatistics,
                                       aIndexStat,
                                       aIndex,
                                       aMtx,
                                       (UChar **)aNewChildPage ) == IDE_SUCCESS );
                (*aAllocPageCount)++;

                sNewChildPID = sdpPhyPage::getPageID((UChar *)*aNewChildPage);

            }
            else
            {
                sNewChildPID = SD_NULL_PID;
            }

            IDE_DASSERT( verifyPrefixPos( aIndex,
                                          sPage,
                                          sNewChildPID,
                                          sKeyMapSeq,
                                          aKeyInfo )
                         == ID_TRUE );

            sRc = updateAndInsertIKey( aStatistics, 
                                       aMtx, 
                                       aIndex, 
                                       sPage, 
                                       sKeyMapSeq,
                                       aKeyInfo, 
                                       aKeyValueLen, 
                                       sNewChildPID,
                                       aMode, 
                                       aIndex->mLogging );
            if ( sRc != IDE_SUCCESS )
            {
                ideLog::log( IDE_ERR_0,
                             "KeyMap sequence : %"ID_INT32_FMT""
                             ", key value length : %"ID_UINT32_FMT""
                             ", mode : %"ID_UINT32_FMT"\n",
                             sKeyMapSeq, *aKeyValueLen, aMode );
                dumpIndexNode( sPage );
                IDE_ASSERT_MSG( 0, "Error Ouccurs in DRDB Index ID : %"ID_UINT32_FMT, aIndex->mIndexID );
            }
            else
            {
                /* nothing to do */
            }
        }
    }

    if (sTmpPage != NULL)
    {
        (void)smnManager::mDiskTempPagePool.memfree(sTmpPage);
        sTmpPage = NULL;
    }
    else
    {
        /* nothing to do */
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if (sTmpPage != NULL)
    {
        (void)smnManager::mDiskTempPagePool.memfree(sTmpPage);
        sTmpPage = NULL;
    }

    return IDE_FAILURE;

}

/*********************************************************************
 * FUNCTION DESCRIPTION : sdnbBTree::canAllocInternalKey             *
 * ------------------------------------------------------------------*
 * Page�� �־��� ũ���� slot�� �Ҵ��� �� �ִ��� �˻��ϰ�, �ʿ��ϸ�   *
 * compactPage���� �����Ѵ�.                                         *
 *********************************************************************/
IDE_RC sdnbBTree::canAllocInternalKey ( sdrMtx *         aMtx,
                                        sdnbHeader *     aIndex,
                                        sdpPhyPageHdr *  aPageHdr,
                                        UInt             aSaveSize,
                                        idBool           aExecCompact,
                                        idBool           aIsLogging )
{
    UShort        sNeededFreeSize;
    UShort        sBeforeFreeSize;
    sdnbNodeHdr * sNodeHdr;

    sNeededFreeSize = aSaveSize + ID_SIZEOF(sdpSlotEntry);

    if ( sdpPhyPage::getNonFragFreeSize( aPageHdr ) < sNeededFreeSize )
    {
        sNodeHdr = (sdnbNodeHdr*)sdpPhyPage::getLogicalHdrStartPtr( (UChar *)aPageHdr );

        sBeforeFreeSize = sdpPhyPage::getTotalFreeSize(aPageHdr);

        // compact page�� �ص� slot�� �Ҵ���� ���ϴ� ���
        IDE_TEST( (UInt)(sBeforeFreeSize + sNodeHdr->mTotalDeadKeySize) < (UInt)sNeededFreeSize );

        if ( aExecCompact == ID_TRUE )
        {
            IDE_TEST( compactPage( aIndex,
                                   aMtx,
                                   aPageHdr,
                                   aIsLogging )
                      != IDE_SUCCESS );
        }
        else
        {
            /* nothing to do */
        }
    }
    else
    {
        /* nothing to do */
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : sdnbBTree::canAllocLeafKey                 *
 * ------------------------------------------------------------------*
 * Page�� �־��� ũ���� slot�� �Ҵ��� �� �ִ��� �˻��ϰ�, �ʿ��ϸ�   *
 * compactPage���� �����Ѵ�.                                         *
 *********************************************************************/
IDE_RC sdnbBTree::canAllocLeafKey ( sdrMtx         * aMtx,
                                    sdnbHeader     * aIndex,
                                    sdpPhyPageHdr  * aPageHdr,
                                    UInt             aSaveSize,
                                    idBool           aIsPessimistic,
                                    SShort         * aKeyMapSeq )
{
    UShort        sNeedFreeSize;
    UShort        sBeforeFreeSize;
    sdnbNodeHdr * sNodeHdr;

    sNeedFreeSize = aSaveSize + ID_SIZEOF(sdpSlotEntry);

    if ( sdpPhyPage::getNonFragFreeSize( aPageHdr ) < sNeedFreeSize )
    {
        sNodeHdr = (sdnbNodeHdr*)sdpPhyPage::getLogicalHdrStartPtr( (UChar *)aPageHdr );

        sBeforeFreeSize = sdpPhyPage::getTotalFreeSize(aPageHdr);

        // compact page�� �ص� slot�� �Ҵ���� ���ϴ� ���
        if ( sBeforeFreeSize + sNodeHdr->mTotalDeadKeySize < sNeedFreeSize )
        {
            if ( aIsPessimistic == ID_TRUE )
            {
                if ( aKeyMapSeq != NULL )
                {
                    IDE_TEST( adjustKeyPosition( aPageHdr, aKeyMapSeq )
                              != IDE_SUCCESS );
                }
                else
                {
                    /* nothing to do */
                }

                IDE_TEST( compactPage( aIndex,
                                       aMtx,
                                       aPageHdr,
                                       ID_TRUE )
                          != IDE_SUCCESS );
            }
            else
            {
                // Optimistic ��忡���� Pessimistic ���� �ٽ� �����ϱ� ������
                // compaction�� ������ �ʿ䰡 ����.
            }

            IDE_TEST( 1 );
        }
        else
        {
            if ( aKeyMapSeq != NULL )
            {
                IDE_TEST( adjustKeyPosition( aPageHdr, aKeyMapSeq )
                          != IDE_SUCCESS );
            }
            else
            {
                /* nothing to do */
            }

            IDE_TEST( compactPage( aIndex,
                                   aMtx,
                                   aPageHdr,
                                   ID_TRUE )
                      != IDE_SUCCESS );

            IDE_DASSERT( sdpPhyPage::getNonFragFreeSize( aPageHdr ) >=
                         sNeedFreeSize );
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : sdnbBTree::adjustKeyPosition               *
 * ------------------------------------------------------------------*
 * Insert Position�� ȹ���� ����, Compaction���� ���Ͽ� Insertable   *
 * Position�� ����ɼ� ������, �ش� �Լ��� �̸� �������ִ� ������    *
 * �Ѵ�.                                                             *
 *********************************************************************/
IDE_RC sdnbBTree::adjustKeyPosition( sdpPhyPageHdr  * aPage,
                                     SShort         * aKeyPosition )
{
    UChar        * sSlotDirPtr;
    SInt           i;
    UChar        * sSlot;
    sdnbLKey     * sLeafKey;
    SShort         sOrgKeyPosition;
    SShort         sAdjustedKeyPosition;

    sSlotDirPtr = sdpPhyPage::getSlotDirStartPtr( (UChar *)aPage );

    sOrgKeyPosition = *aKeyPosition;
    sAdjustedKeyPosition = *aKeyPosition;

    for ( i = 0 ; i < sOrgKeyPosition ; i++ )
    {
        IDE_TEST( sdpSlotDirectory::getPagePtrFromSlotNum( sSlotDirPtr, 
                                                           i, 
                                                           &sSlot )
                  !=  IDE_SUCCESS );
        sLeafKey = (sdnbLKey*)sSlot;

        if ( SDNB_GET_STATE( sLeafKey ) == SDNB_KEY_DEAD )
        {
            sAdjustedKeyPosition--;
        }
        else
        {
            /* nothing to do */
        }
    }

    *aKeyPosition = sAdjustedKeyPosition;
    
    return IDE_SUCCESS;
    
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : sdnbBTree::splitInternalNode               *
 * ------------------------------------------------------------------*
 * Internal node�� ������ child node����, unbalanced split�� ����    *
 * �ؾ� �ϴ� ��쿡 ȣ��ȴ�.                                        *
 *********************************************************************/
IDE_RC sdnbBTree::splitInternalNode(idvSQL         * aStatistics,
                                    sdnbStatistic  * aIndexStat,
                                    sdrMtx         * aMtx,
                                    idBool         * aMtxStart, 
                                    smnIndexHeader * aPersIndex,
                                    sdnbHeader     * aIndex,
                                    sdnbStack      * aStack,
                                    sdnbKeyInfo    * aKeyInfo,
                                    UShort         * aKeyValueLen,
                                    UInt             aMode,
                                    UShort           aPCTFree,
                                    sdpPhyPageHdr  * aPage,
                                    sdpPhyPageHdr ** aNewChildPage,
                                    UInt           * aAllocPageCount )
{
    UShort           sKeyLength;
    SShort           sKeyMapSeq;
    UShort           sSplitPoint;
    sdpPhyPageHdr   *sNewPage;
    UShort           sTotalKeyCount;
    scPageID         sLeftMostPID;
    scPageID         sNewChildPID;
    sdnbIKey        *sIKey;
    sdnbKeyInfo      sKeyInfo;
    UShort           sKeyValueLen;
    sdnbKeyInfo     *sPropagateKeyInfo;
    UShort          *sPropagateKeyValueLen;
    UChar           *sSlotDirPtr;
    IDE_RC           sRc;
    ULong            sSmoNo;

    sSlotDirPtr    = sdpPhyPage::getSlotDirStartPtr( (UChar *)aPage );
    sTotalKeyCount = sdpSlotDirectory::getCount( sSlotDirPtr );

    sKeyLength = SDNB_IKEY_LEN(aKeyValueLen[0]);

    // �� key�� ���� �� ��ġ
    sKeyMapSeq = aStack->mStack[aStack->mCurrentDepth].mKeyMapSeq + 1;

    // split internal node
    IDE_TEST( findSplitPoint( aIndex,
                              aPage,
                              aStack->mIndexDepth - aStack->mCurrentDepth,
                              sKeyMapSeq,
                              sKeyLength,
                              aPCTFree,
                              aMode,
                              ID_FALSE,
                              &sSplitPoint )
              != IDE_SUCCESS );

    aStack->mCurrentDepth--;

    switch ( aMode )
    {
        case SDNB_SMO_MODE_SPLIT_1_2:
        {
            if ( sSplitPoint == SDNB_SPLIT_POINT_NEW_KEY )
            {
                // propagated prefix key from child
                IDE_TEST( propagateKeyInternalNode( aStatistics,
                                                    aIndexStat,
                                                    aMtx,
                                                    aMtxStart,
                                                    aPersIndex,
                                                    aIndex,
                                                    aStack,
                                                    aKeyInfo,
                                                    aKeyValueLen,
                                                    SDNB_SMO_MODE_SPLIT_1_2,
                                                    &sNewPage,
                                                    aAllocPageCount)
                          != IDE_SUCCESS );
                aStack->mCurrentDepth++;

                // �� ����� SMONo�� ���� SmoNo + 1 ����
                getSmoNo( (void *)aIndex, &sSmoNo );
                sdpPhyPage::setIndexSMONo((UChar *)sNewPage, sSmoNo + 1);

                IDL_MEM_BARRIER;

                // �� Child ��带 �Ҵ�޴´�.
                IDE_ASSERT( allocPage( aStatistics,
                                       aIndexStat,
                                       aIndex,
                                       aMtx,
                                       (UChar **)aNewChildPage ) == IDE_SUCCESS );
                (*aAllocPageCount)++;

                IDE_DASSERT( (((vULong)*aNewChildPage) % SD_PAGE_SIZE) == 0 );

                // �� ����� leftmost child node�� �� child node�� PID�� �ȴ�.
                sLeftMostPID = sdpPhyPage::getPageID((UChar *)*aNewChildPage);
                IDE_TEST(initializeNodeHdr(aMtx,
                                           sNewPage,
                                           sLeftMostPID,
                                           aStack->mIndexDepth - aStack->mCurrentDepth,
                                           aIndex->mLogging)
                         != IDE_SUCCESS );

                // ���� ��忡�� �� prefix�� ���� �ߴ� ��ġ ���ĸ�
                // �� ���� �̵��ϸ鼭 �����Ѵ�.
                if ( sKeyMapSeq <= (sTotalKeyCount - 1) )
                {
                    sRc = moveSlots( aMtx,
                                     aIndex,
                                     aPage,
                                     aStack->mIndexDepth - aStack->mCurrentDepth,
                                     sKeyMapSeq,
                                     sTotalKeyCount - 1,
                                     sNewPage );
                    if ( sRc != IDE_SUCCESS )
                    {
                        ideLog::log( IDE_ERR_0,
                                     "move from <%"ID_INT32_FMT"> to <%"ID_UINT32_FMT">"
                                     "\nIndex depth : %"ID_INT32_FMT""
                                     ", Index current depth : %"ID_INT32_FMT"\n",
                                     sKeyMapSeq, sTotalKeyCount - 1,
                                     aStack->mIndexDepth, aStack->mCurrentDepth );
                        dumpIndexNode( aPage );
                        dumpIndexNode( sNewPage );
                        IDE_ASSERT_MSG( 0, "Error Ouccurs in DRDB Index ID : %"ID_UINT32_FMT, aIndex->mIndexID );
                    }
                    else
                    {
                        /* nothing to do */
                    }
                }
                else
                {
                    /* nothing to do */
                }
            }
            else
            {
                IDE_TEST( sdpSlotDirectory::getPagePtrFromSlotNum( 
                                                            sSlotDirPtr,
                                                            sSplitPoint,
                                                            (UChar **)&sIKey )
                          != IDE_SUCCESS );
                SDNB_IKEY_TO_KEYINFO( sIKey, sKeyInfo );

                sKeyValueLen = getKeyValueLength( &(aIndex->mColLenInfoList),
                                                  SDNB_IKEY_KEY_PTR(  sIKey ) );

                IDE_TEST( propagateKeyInternalNode(aStatistics,
                                                   aIndexStat,
                                                   aMtx,
                                                   aMtxStart,
                                                   aPersIndex,
                                                   aIndex,
                                                   aStack,
                                                   &sKeyInfo,
                                                   &sKeyValueLen,
                                                   SDNB_SMO_MODE_SPLIT_1_2,
                                                   &sNewPage,
                                                   aAllocPageCount)
                          != IDE_SUCCESS );
                aStack->mCurrentDepth++;

                // �� ����� SMONo�� ���� SmoNo + 1 ����
                getSmoNo( (void *)aIndex, &sSmoNo );
                sdpPhyPage::setIndexSMONo( (UChar *)sNewPage, sSmoNo + 1 );
                IDL_MEM_BARRIER;

                // �� Child ��带 �Ҵ�޴´�.
                IDE_ASSERT( allocPage( aStatistics,
                                       aIndexStat,
                                       aIndex,
                                       aMtx,
                                       (UChar **)aNewChildPage ) == IDE_SUCCESS );
                (*aAllocPageCount)++;

                IDE_DASSERT( (((vULong)*aNewChildPage) % SD_PAGE_SIZE) == 0 );

                sNewChildPID = sdpPhyPage::getPageID((UChar *)*aNewChildPage);

                // �� ����� leftmost child node��
                // splitpoint slot�� ChildPID�� �ȴ�.

                SDNB_GET_RIGHT_CHILD_PID( &sLeftMostPID, sIKey );

                IDE_TEST(initializeNodeHdr(aMtx,
                                           sNewPage,
                                           sLeftMostPID,
                                           aStack->mIndexDepth - aStack->mCurrentDepth,
                                           aIndex->mLogging)
                         != IDE_SUCCESS );

                IDL_MEM_BARRIER;

                if ( (sSplitPoint + 1) <= (sTotalKeyCount - 1) )
                {
                    sRc = moveSlots( aMtx,
                                     aIndex,
                                     aPage,
                                     aStack->mIndexDepth - aStack->mCurrentDepth,
                                     sSplitPoint + 1,
                                     sTotalKeyCount - 1,
                                     sNewPage );
                    if ( sRc != IDE_SUCCESS )
                    {
                        ideLog::log( IDE_ERR_0,
                                     "move from <%"ID_INT32_FMT"> to <%"ID_UINT32_FMT">"
                                     "\nIndex depth : %"ID_INT32_FMT""
                                     ", Index current depth : %"ID_INT32_FMT"\n",
                                     sSplitPoint + 1, sTotalKeyCount - 1,
                                     aStack->mIndexDepth, aStack->mCurrentDepth );
                        dumpIndexNode( aPage );
                        dumpIndexNode( sNewPage );
                        IDE_ASSERT_MSG( 0, "Error Ouccurs in DRDB Index ID : %"ID_UINT32_FMT, aIndex->mIndexID );
                    }
                    else
                    {
                        /* nothing to do */
                    }

                    sRc = deleteIKey( aStatistics,
                                      aMtx,
                                      aIndex,
                                      aPage,
                                      (SShort)sSplitPoint,
                                      aIndex->mLogging,
                                      sLeftMostPID );
                    if ( sRc != IDE_SUCCESS )
                    {
                        ideLog::log( IDE_ERR_0,
                                     "delete Key sequence : %"ID_UINT32_FMT"\n",
                                     sSplitPoint );
                        dumpIndexNode( aPage );
                        IDE_ASSERT_MSG( 0, "Error Ouccurs in DRDB Index ID : %"ID_UINT32_FMT, aIndex->mIndexID );
                    }
                    else
                    {
                        /* nothing to do */
                    }
                }
                else
                {
                    sRc = deleteIKey( aStatistics,
                                      aMtx,
                                      aIndex,
                                      aPage,
                                      (SShort)sSplitPoint,
                                      aIndex->mLogging,
                                      sLeftMostPID );
                    if ( sRc != IDE_SUCCESS )
                    {
                        ideLog::log( IDE_ERR_0,
                                     "delete Key sequence : %"ID_UINT32_FMT"\n",
                                     sSplitPoint );
                        dumpIndexNode( aPage );
                        IDE_ASSERT_MSG( 0, "Error Ouccurs in DRDB Index ID : %"ID_UINT32_FMT, aIndex->mIndexID );
                    }
                    else
                    {
                        /* nothing to do */
                    }
                }

                if ( (UShort)sKeyMapSeq <= sSplitPoint )
                {
                    // �����κ��� propagate�� prefix�� insert�Ѵ�.
                    sRc = updateAndInsertIKey( aStatistics, aMtx, aIndex, aPage, sKeyMapSeq,
                                               aKeyInfo, aKeyValueLen, sNewChildPID,
                                               aMode, aIndex->mLogging );
                    if ( sRc != IDE_SUCCESS )
                    {
                        ideLog::log( IDE_ERR_0,
                                     "KeyMap sequence : %"ID_INT32_FMT""
                                     ", key value length : %"ID_UINT32_FMT""
                                     ", mode : %"ID_UINT32_FMT"\n",
                                     sKeyMapSeq, *aKeyValueLen, aMode );
                        dumpIndexNode( aPage );
                        IDE_ASSERT_MSG( 0, "Error Ouccurs in DRDB Index ID : %"ID_UINT32_FMT, aIndex->mIndexID );
                    }
                    else
                    {
                        /* nothing to do */
                    }
                }
                else
                {
                    // �����κ��� propagate�� prefix�� insert�Ѵ�.
                    sRc = updateAndInsertIKey( aStatistics,
                                               aMtx,
                                               aIndex,
                                               sNewPage,
                                               sKeyMapSeq - sSplitPoint - 1,
                                               aKeyInfo,
                                               aKeyValueLen,
                                               sNewChildPID,
                                               aMode,
                                               aIndex->mLogging);
                    if ( sRc != IDE_SUCCESS )
                    {
                        ideLog::log( IDE_ERR_0,
                                     "KeyMap sequence : %"ID_INT32_FMT""
                                     ", Split point : %"ID_INT32_FMT""
                                     ", key value length : %"ID_UINT32_FMT""
                                     ", mode : %"ID_UINT32_FMT"\n",
                                     sKeyMapSeq, sSplitPoint, *aKeyValueLen, aMode );
                        dumpIndexNode( sNewPage );
                        IDE_ASSERT_MSG( 0, "Error Ouccurs in DRDB Index ID : %"ID_UINT32_FMT, aIndex->mIndexID );
                    }
                    else
                    {
                        /* nothing to do */
                    }
                }
            }
        } // SDNB_SMO_MODE_SPLIT_1_2
        break;

        case SDNB_SMO_MODE_KEY_REDIST:
        {
            IDE_DASSERT(sSplitPoint != sKeyMapSeq);

            if ( sSplitPoint == SDNB_SPLIT_POINT_NEW_KEY )
            {
                // Key�� Update�� �߻��Ҷ�, Split point�� ������ �������� Key�� ��ġ�� ���� ���
                IDE_TEST( sdpSlotDirectory::getPagePtrFromSlotNum( 
                                                            sSlotDirPtr,
                                                            sKeyMapSeq,
                                                            (UChar **)&sIKey )
                          != IDE_SUCCESS );
                sPropagateKeyInfo     = &aKeyInfo[0];
                sPropagateKeyValueLen = &aKeyValueLen[0];

                IDE_TEST(propagateKeyInternalNode(aStatistics,
                                                  aIndexStat,
                                                  aMtx,
                                                  aMtxStart,
                                                  aPersIndex,
                                                  aIndex,
                                                  aStack,
                                                  sPropagateKeyInfo,
                                                  sPropagateKeyValueLen,
                                                  SDNB_SMO_MODE_SPLIT_1_2,
                                                  &sNewPage,
                                                  aAllocPageCount)
                         != IDE_SUCCESS );
                aStack->mCurrentDepth++;

                // �� ����� SMONo�� ���� SmoNo + 1 ����
                getSmoNo( (void *)aIndex, &sSmoNo );
                sdpPhyPage::setIndexSMONo( (UChar *)sNewPage, sSmoNo + 1 );
                IDL_MEM_BARRIER;

                // Update mode������ �� ��带 �Ҵ���� �ʿ䰡 ����.
                sNewChildPID = SD_NULL_PID;

                // �� ����� leftmost child node�� splitpoint slot�� ChildPID�� �ȴ�.
                SDNB_GET_RIGHT_CHILD_PID( &sLeftMostPID, sIKey );
                IDE_TEST( initializeNodeHdr( aMtx,
                                             sNewPage,
                                             sLeftMostPID,
                                             aStack->mIndexDepth - aStack->mCurrentDepth,
                                             aIndex->mLogging )
                         != IDE_SUCCESS );

                IDL_MEM_BARRIER;

                if ( ( sKeyMapSeq + 1 ) <= ( sTotalKeyCount - 1 ) )
                {
                    // ���� ��忡�� sKeyMapSeq ���ĸ� �� ���� �̵��ϸ鼭 ����.
                    sRc = moveSlots( aMtx,
                                     aIndex,
                                     aPage,
                                     aStack->mIndexDepth - aStack->mCurrentDepth,
                                     sKeyMapSeq + 1,
                                     sTotalKeyCount - 1,
                                     sNewPage);
                    if ( sRc != IDE_SUCCESS )
                    {
                        ideLog::log( IDE_ERR_0,
                                     "move from <%"ID_INT32_FMT"> to <%"ID_UINT32_FMT">"
                                     "\nIndex depth : %"ID_INT32_FMT""
                                     ", Index current depth : %"ID_INT32_FMT"\n",
                                     sKeyMapSeq + 1, sTotalKeyCount - 1,
                                     aStack->mIndexDepth, aStack->mCurrentDepth );
                        dumpIndexNode( aPage );
                        dumpIndexNode( sNewPage );
                        IDE_ASSERT_MSG( 0, "Error Ouccurs in DRDB Index ID : %"ID_UINT32_FMT, aIndex->mIndexID );
                    }
                    else
                    {
                        /* nothing to do */
                    }

                    sRc = deleteIKey( aStatistics,
                                      aMtx,
                                      aIndex,
                                      aPage,
                                      sKeyMapSeq,
                                      aIndex->mLogging,
                                      aStack->mStack[ aStack->mCurrentDepth+1 ].mNextNode);
                    if ( sRc != IDE_SUCCESS )
                    {
                        ideLog::log( IDE_ERR_0,
                                     "delete Key sequence : %"ID_UINT32_FMT"\n",
                                     sKeyMapSeq );
                        dumpIndexNode( aPage );
                        ideLog::logMem( IDE_DUMP_0, (UChar *)aStack, ID_SIZEOF(sdnbStack) );
                        IDE_ASSERT_MSG( 0, "Error Ouccurs in DRDB Index ID : %"ID_UINT32_FMT, aIndex->mIndexID );
                    }
                    else
                    {
                        /* nothing to do */
                    }
                }
                else
                {
                    sRc = deleteIKey( aStatistics,
                                      aMtx,
                                      aIndex,
                                      aPage,
                                      sKeyMapSeq,
                                      aIndex->mLogging,
                                      aStack->mStack[ aStack->mCurrentDepth+1 ].mNextNode);
                    if ( sRc != IDE_SUCCESS )
                    {
                        ideLog::log( IDE_ERR_0,
                                     "delete Key sequence : %"ID_UINT32_FMT"\n",
                                     sKeyMapSeq );
                        dumpIndexNode( aPage );
                        ideLog::logMem( IDE_DUMP_0, (UChar *)aStack, ID_SIZEOF(sdnbStack) );
                        IDE_ASSERT_MSG( 0, "Error Ouccurs in DRDB Index ID : %"ID_UINT32_FMT, aIndex->mIndexID );
                    }
                    else
                    {
                        /* nothing to do */
                    }
                }
            }
            else
            {
                IDE_TEST( sdpSlotDirectory::getPagePtrFromSlotNum( 
                                                            sSlotDirPtr,
                                                            sSplitPoint, 
                                                            (UChar **)&sIKey )
                          != IDE_SUCCESS );
                SDNB_IKEY_TO_KEYINFO( sIKey, sKeyInfo );

                sKeyValueLen = getKeyValueLength( &(aIndex->mColLenInfoList),
                                                  SDNB_IKEY_KEY_PTR(  sIKey ) );

                IDE_TEST( propagateKeyInternalNode( aStatistics,
                                                    aIndexStat,
                                                    aMtx,
                                                    aMtxStart,
                                                    aPersIndex,
                                                    aIndex,
                                                    aStack,
                                                    &sKeyInfo,
                                                    &sKeyValueLen,
                                                    SDNB_SMO_MODE_SPLIT_1_2,
                                                    &sNewPage,
                                                    aAllocPageCount )
                          != IDE_SUCCESS );
                aStack->mCurrentDepth++;

                // �� ����� SMONo�� ���� SmoNo + 1 ����
                getSmoNo( (void *)aIndex, &sSmoNo );
                sdpPhyPage::setIndexSMONo( (UChar *)sNewPage, sSmoNo + 1 );
                IDL_MEM_BARRIER;

                // Update mode������ �� ��带 �Ҵ���� �ʿ䰡 ����.
                sNewChildPID = 0;

                // �� ����� leftmost child node�� splitpoint slot�� ChildPID�� �ȴ�.
                SDNB_GET_RIGHT_CHILD_PID( &sLeftMostPID, sIKey );
                IDE_TEST( initializeNodeHdr( aMtx,
                                             sNewPage,
                                             sLeftMostPID,
                                             aStack->mIndexDepth - aStack->mCurrentDepth,
                                             aIndex->mLogging )
                         != IDE_SUCCESS );

                IDL_MEM_BARRIER;

                if ( ( sSplitPoint + 1 ) <= ( sTotalKeyCount - 1 ) )
                {
                    // ���� ��忡�� sSplitPoint ���ĸ� �� ���� �̵��ϸ鼭 ����.
                    sRc = moveSlots( aMtx,
                                     aIndex,
                                     aPage,
                                     aStack->mIndexDepth - aStack->mCurrentDepth,
                                     sSplitPoint + 1,
                                     sTotalKeyCount - 1,
                                     sNewPage);
                    if ( sRc != IDE_SUCCESS )
                    {
                        ideLog::log( IDE_ERR_0,
                                     "move from <%"ID_INT32_FMT"> to <%"ID_UINT32_FMT">"
                                     "\nIndex depth : %"ID_INT32_FMT""
                                     ", Index current depth : %"ID_INT32_FMT"\n",
                                     sSplitPoint + 1, sTotalKeyCount - 1,
                                     aStack->mIndexDepth, aStack->mCurrentDepth );
                        dumpIndexNode( aPage );
                        dumpIndexNode( sNewPage );
                        IDE_ASSERT_MSG( 0, "Error Ouccurs in DRDB Index ID : %"ID_UINT32_FMT, aIndex->mIndexID );
                    }
                    else
                    {
                        /* nothing to do */
                    }

                    sRc = deleteIKey( aStatistics,
                                      aMtx,
                                      aIndex,
                                      aPage,
                                      (SShort)sSplitPoint,
                                      aIndex->mLogging,
                                      sLeftMostPID);
                    if ( sRc != IDE_SUCCESS )
                    {
                        ideLog::log( IDE_ERR_0,
                                     "delete Key sequence : %"ID_UINT32_FMT"\n",
                                     sSplitPoint );
                        dumpIndexNode( aPage );
                        IDE_ASSERT_MSG( 0, "Error Ouccurs in DRDB Index ID : %"ID_UINT32_FMT, aIndex->mIndexID );
                    }
                    else
                    {
                        /* nothing to do */
                    }
                }
                else
                {
                    sRc = deleteIKey( aStatistics,
                                      aMtx,
                                      aIndex,
                                      aPage,
                                      (SShort)sSplitPoint,
                                      aIndex->mLogging,
                                      sLeftMostPID);
                    if ( sRc != IDE_SUCCESS )
                    {
                        ideLog::log( IDE_ERR_0,
                                     "delete Key sequence : %"ID_UINT32_FMT"\n",
                                     sSplitPoint );
                        dumpIndexNode( aPage );
                        IDE_ASSERT_MSG( 0, "Error Ouccurs in DRDB Index ID : %"ID_UINT32_FMT, aIndex->mIndexID );
                    }
                    else
                    {
                        /* nothing to do */
                    }
                }

                if ( sKeyMapSeq <= sSplitPoint )
                {
                    // �����κ��� propagate�� prefix�� insert�Ѵ�.
                    sRc = updateAndInsertIKey( aStatistics, 
                                               aMtx, 
                                               aIndex, 
                                               aPage, 
                                               sKeyMapSeq,
                                               aKeyInfo, 
                                               aKeyValueLen, 
                                               sNewChildPID,
                                               aMode, 
                                               aIndex->mLogging);
                    if ( sRc != IDE_SUCCESS )
                    {
                        ideLog::log( IDE_ERR_0,
                                     "KeyMap sequence : %"ID_INT32_FMT""
                                     ", key value length : %"ID_UINT32_FMT""
                                     ", mode : %"ID_UINT32_FMT"\n",
                                     sKeyMapSeq, *aKeyValueLen, aMode );
                        dumpIndexNode( aPage );
                        IDE_ASSERT_MSG( 0, "Error Ouccurs in DRDB Index ID : %"ID_UINT32_FMT, aIndex->mIndexID );
                    }
                    else
                    {
                        /* nothing to do */
                    }
                }
                else
                {
                    // �����κ��� propagate�� prefix�� insert�Ѵ�.
                    sRc = updateAndInsertIKey( aStatistics, 
                                               aMtx, 
                                               aIndex, 
                                               sNewPage, 
                                               sKeyMapSeq - sSplitPoint - 1,
                                               aKeyInfo, 
                                               aKeyValueLen, 
                                               sNewChildPID,
                                               aMode, 
                                               aIndex->mLogging);
                    if ( sRc != IDE_SUCCESS )
                    {
                        ideLog::log( IDE_ERR_0,
                                     "KeyMap sequence : %"ID_INT32_FMT""
                                     "Split point : %"ID_INT32_FMT""
                                     ", key value length : %"ID_UINT32_FMT""
                                     ", mode : %"ID_UINT32_FMT"\n",
                                     sKeyMapSeq, 
                                     sSplitPoint, 
                                     *aKeyValueLen, 
                                     aMode );
                        dumpIndexNode( sNewPage );
                        IDE_ASSERT_MSG( 0, "Error Ouccurs in DRDB Index ID : %"ID_UINT32_FMT, aIndex->mIndexID );
                    }
                    else
                    {
                        /* nothing to do */
                    }
                }
            } // sSplitPoint == sKeyMapSeq
        } // SDNB_SMO_MODE_KEY_REDIST
        break;

        default:
            break;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : sdnbBTree::redistributeKeyInternal         *
 * ------------------------------------------------------------------*
 * ���� Internal node�� Key ��й踦 �����Ѵ�.                       *
 * ��й踦 �õ��Ͽ� �����ϸ� ��й踦 ������,                       *
 * �׷��� ������ balanced 1-to-2 split�� �ϰ� �ȴ�.                  *
 *********************************************************************/
IDE_RC sdnbBTree::redistributeKeyInternal( idvSQL         * aStatistics,
                                           sdnbStatistic  * aIndexStat,
                                           sdrMtx         * aMtx,
                                           idBool         * aMtxStart,
                                           smnIndexHeader * aPersIndex,
                                           sdnbHeader     * aIndex,
                                           sdnbStack      * aStack,
                                           sdnbKeyInfo    * aKeyInfo,
                                           UShort         * aKeyValueLen,
                                           SShort           aKeyMapSeq,
                                           UInt             aMode,
                                           idBool         * aCanRedistribute,
                                           sdpPhyPageHdr  * aNode,
                                           sdpPhyPageHdr ** aNewChildPage,
                                           UInt           * aAllocPageCount )
{
    scPageID         sNextPID;
    scPageID         sNewChildPID;
    sdpPhyPageHdr *  sNextPage;
    SShort           sMoveStartSeq;
    idBool           sIsSuccess;
    sdnbNodeHdr *    sNodeHdr;
    sdpPhyPageHdr *  sNewPage = NULL;
    sdnbKeyInfo      sPropagateKeyInfo;
    sdnbKeyInfo      sBranchKeyInfo;
    UShort           sPropagateKeyValueLen;
    UShort           sBranchKeyValueLen;
    scPageID         sRightChildPID;
    scPageID         sLeftMostChildPID;
    scPageID         sDummyPID;
    UShort           sCurSlotCount;
    UChar          * sSlotDirPtr;
    // Branch Key�� ������ ��ҷμ�, Key�� Page size 1/2�� ������ �����Ƿ�
    // Page size ���� ũ���� ���۸� �Ҵ��Ѵ�.
    ULong            sTmpBuf[SDNB_MAX_KEY_BUFFER_SIZE/ID_SIZEOF(ULong)];
    IDE_RC           sRc;
    ULong            sSmoNo;

    sNextPID = aStack->mStack[aStack->mCurrentDepth].mNextNode;

    IDE_TEST( sdnbBTree::getPage( aStatistics,
                                  &(aIndexStat->mIndexPage),
                                  aIndex->mIndexTSID,
                                  sNextPID,
                                  SDB_X_LATCH,
                                  SDB_WAIT_NORMAL,
                                  aMtx,
                                  (UChar **)&sNextPage,
                                  &sIsSuccess )
              != IDE_SUCCESS );

    IDE_TEST( findRedistributeKeyPoint( aIndex,
                                        aStack,
                                        aKeyValueLen,
                                        aKeyMapSeq,
                                        aMode,
                                        aNode,
                                        sNextPage,
                                        ID_FALSE, //aIsLeaf
                                        &sMoveStartSeq,
                                        aCanRedistribute )
              != IDE_SUCCESS );

    if ( *aCanRedistribute != ID_TRUE )
    {
        IDE_CONT(ERR_KEY_REDISTRIBUTE);
    }
    else
    {
        /* nothing to do */
    }

    // ���� index SmoNo + 1 �� ��忡 ����
    getSmoNo( (void *)aIndex, &sSmoNo );
    sdpPhyPage::setIndexSMONo((UChar *)sNextPage, sSmoNo + 1);
    IDL_MEM_BARRIER;

    /* ������ �����ϰ�, ����� ���� Prefix Key�� ���� Internal node��
     * �����Ѵ�. �̴� SMO �ֻ�ܿ��� �Ʒ��� SMO�� �߻��ؾ� �ϱ� �����̴�.
     */
    // internal node������ ���� branch key�� �����ͼ� ���ԵǾ�� �Ѵ�.
    // ����, Key�� Propagation �ϱ� ���� branch Key�� �̸� ���س��´�.
    IDE_TEST_CONT(getKeyInfoFromPIDAndKey (aStatistics,
                                            aIndexStat,
                                            aIndex,
                                            aStack->mStack[aStack->mCurrentDepth-1].mNode,
                                            aStack->mStack[aStack->mCurrentDepth-1].mKeyMapSeq + 1,
                                            ID_FALSE, //aIsLeaf
                                            &sBranchKeyInfo,
                                            &sBranchKeyValueLen,
                                            &sDummyPID,
                                            (UChar *)sTmpBuf)
                   != IDE_SUCCESS, ERR_KEY_REDISTRIBUTE);

    /* Prefix Key ���ϱ� : Current node���� �������� ù��° Key */
    IDE_TEST( getKeyInfoFromSlot( aIndex,
                                  aNode,
                                  sMoveStartSeq,
                                  ID_FALSE, //aIsLeaf
                                  &sPropagateKeyInfo,
                                  &sPropagateKeyValueLen,
                                  &sRightChildPID )
              != IDE_SUCCESS );

    switch ( aMode )
    {
        case SDNB_SMO_MODE_KEY_REDIST:
            if ( sMoveStartSeq == aKeyMapSeq )
            {
                ideLog::log( IDE_ERR_0,
                             "Move start sequence : %"ID_INT32_FMT"\n",
                             sMoveStartSeq );
                dumpIndexNode( aNode );
                IDE_ASSERT_MSG( 0, "Error Ouccurs in DRDB Index ID : %"ID_UINT32_FMT, aIndex->mIndexID );
            }
            else
            {
                /* nothing to do */
            }
            break;

        case SDNB_SMO_MODE_SPLIT_1_2:
        default:
            break;
    }

    /* No logging���� Key ��й踦 �����Ѵ�. */
    // ������ �ö� Key�� Right child PID�� Next node�� Left most child PID�� �ȴ�.
    sNodeHdr = (sdnbNodeHdr*)sdpPhyPage::getLogicalHdrStartPtr( (UChar *)sNextPage );
    sLeftMostChildPID = sNodeHdr->mLeftmostChild;

    sSlotDirPtr   = sdpPhyPage::getSlotDirStartPtr( (UChar *)aNode );
    sCurSlotCount = sdpSlotDirectory::getCount( sSlotDirPtr );

    /* No logging���� ������ ������ �Ŀ�, ����� ���� Prefix Key��
     * ���� Internal node�� �����Ѵ�. �̴� SMO �ֻ�ܿ��� �Ʒ��� SMO��
     * �߻��ؾ� �ϱ� �����̴�. �� ������ Logging ���� �����ϵ��� �Ѵ�.
     */
    /* Key propagation */
    aStack->mCurrentDepth--;
    IDE_TEST( propagateKeyInternalNode( aStatistics,
                                        aIndexStat,
                                        aMtx,
                                        aMtxStart,
                                        aPersIndex,
                                        aIndex,
                                        aStack,
                                        &sPropagateKeyInfo,
                                        &sPropagateKeyValueLen,
                                        SDNB_SMO_MODE_KEY_REDIST,
                                        &sNewPage,
                                        aAllocPageCount) != IDE_SUCCESS );
    aStack->mCurrentDepth++;

    // Key ��й�ÿ��� ���� �Ҵ�޴� �������� ����� �Ѵ�.
    IDE_ASSERT_MSG( sNewPage == NULL, "Error Ouccurs in DRDB Index ID : %"ID_UINT32_FMT, aIndex->mIndexID );

    // ������ �ö� Key�� Right child PID�� Next node�� Left most child PID�� �ȴ�.

    // BUG-21539
    // mLeftmostChild�� ������ ��ŷ�ؾ� �Ѵ�.
    // sNodeHdr->mLeftmostChild = sRightChildPID;
    IDE_TEST( sdrMiniTrans::writeNBytes( aMtx,
                                         (UChar *)&sNodeHdr->mLeftmostChild,
                                         (void *)&sRightChildPID,
                                         ID_SIZEOF(sRightChildPID) )
              != IDE_SUCCESS );

    sRc = insertIKey( aStatistics,
                      aMtx,
                      aIndex,
                      sNextPage,
                      0,
                      &sBranchKeyInfo,
                      sBranchKeyValueLen,
                      sLeftMostChildPID,
                      ID_TRUE );
    if ( sRc != IDE_SUCCESS )
    {
        ideLog::log( IDE_ERR_0,
                     "Branch KeyValue length : %"ID_UINT32_FMT""
                     "\nBranch Key Info:\n",
                     sBranchKeyValueLen );
        ideLog::logMem( IDE_DUMP_0,
                        (UChar *)&sBranchKeyInfo,
                        ID_SIZEOF(sdnbKeyInfo) );
        dumpIndexNode( sNextPage );
        IDE_ASSERT_MSG( 0, "Error Ouccurs in DRDB Index ID : %"ID_UINT32_FMT, aIndex->mIndexID );
    }
    else
    {
        /* nothing to do */
    }

    // ���� �̵��� ������ ���޵� Key ���������̹Ƿ�,
    // �̵� ���� Sequence�� 1 �������Ѽ� �̵���Ų��.
    sRc = moveSlots( aMtx,
                     aIndex,
                     aNode,
                     aStack->mIndexDepth - aStack->mCurrentDepth,
                     sMoveStartSeq + 1,
                     sCurSlotCount - 1,
                     sNextPage );
    if ( sRc != IDE_SUCCESS )
    {
        ideLog::log( IDE_ERR_0,
                     "move from <%"ID_INT32_FMT"> to <%"ID_UINT32_FMT">"
                     "\nIndex depth : %"ID_INT32_FMT""
                     ", Index current depth : %"ID_INT32_FMT"\n",
                     sMoveStartSeq + 1, sCurSlotCount - 1,
                     aStack->mIndexDepth, aStack->mCurrentDepth );
        dumpIndexNode( aNode );
        dumpIndexNode( sNextPage );
        IDE_ASSERT_MSG( 0, "Error Ouccurs in DRDB Index ID : %"ID_UINT32_FMT, aIndex->mIndexID );
    }
    else
    {
        /* nothing to do */
    }

    // ������ ���޵� Key�� �����Ѵ�.
    sRc = deleteIKey( aStatistics,
                      aMtx,
                      aIndex,
                      aNode,
                      sMoveStartSeq,
                      ID_TRUE,
                      sRightChildPID );
    if ( sRc != IDE_SUCCESS )
    {
        ideLog::log( IDE_ERR_0,
                     "delete Key sequence : %"ID_UINT32_FMT"\n",
                     sMoveStartSeq );
        dumpIndexNode( aNode );
        IDE_ASSERT_MSG( 0, "Error Ouccurs in DRDB Index ID : %"ID_UINT32_FMT, aIndex->mIndexID );
    }
    else
    {
        /* nothing to do */
    }

    IDE_TEST(compactPage(aIndex, aMtx, sNextPage, ID_TRUE) != IDE_SUCCESS );

    /* aMode�� INSERT, INSERT_UPDATE�� ��쿡�� Child���� split��
     * �߻��� ����̹Ƿ�, �̶��� �� �������� �Ҵ�޾Ƽ� �Ѱ��־�� �Ѵ�.
     * ����, �̶� �̸� �Ҵ�޴� ������ ���� depth�� internal node����
     * �ش� IKey�� branch PID�� �����ؾ� �ϱ� �����̴�.
     */
    if ( aMode == SDNB_SMO_MODE_SPLIT_1_2 )
    {
        // �� Child ��带 �Ҵ�޴´�.
        IDE_ASSERT( allocPage( aStatistics,
                               aIndexStat,
                               aIndex,
                               aMtx,
                               (UChar **)aNewChildPage ) == IDE_SUCCESS );
        (*aAllocPageCount)++;

        IDE_DASSERT( (((vULong)*aNewChildPage) % SD_PAGE_SIZE) == 0 );

        // �� ����� leftmost child node ���� �ʿ�
        sNewChildPID = sdpPhyPage::getPageID((UChar *)*aNewChildPage);
    }
    else
    {
        sNewChildPID = SD_NULL_PID;
    }

    if ( (UShort)aKeyMapSeq <= sMoveStartSeq )
    {
        sRc = updateAndInsertIKey( aStatistics, 
                                   aMtx, 
                                   aIndex, 
                                   aNode, 
                                   aKeyMapSeq,
                                   aKeyInfo, 
                                   aKeyValueLen, 
                                   sNewChildPID,
                                   aMode, 
                                   aIndex->mLogging );
        if ( sRc != IDE_SUCCESS )
        {
            ideLog::log( IDE_ERR_0,
                         "KeyMap sequence : %"ID_INT32_FMT""
                         ", key value length : %"ID_UINT32_FMT""
                         ", mode : %"ID_UINT32_FMT"\n",
                         aKeyMapSeq, 
                         *aKeyValueLen, 
                         aMode );
            dumpIndexNode( aNode );
            IDE_ASSERT_MSG( 0, "Error Ouccurs in DRDB Index ID : %"ID_UINT32_FMT, aIndex->mIndexID );
        }
        else
        {
            /* nothing to do */
        }
    }
    else
    {
        sRc = updateAndInsertIKey( aStatistics, 
                                   aMtx, 
                                   aIndex, 
                                   sNextPage, 
                                   aKeyMapSeq - sMoveStartSeq - 1,
                                   aKeyInfo, 
                                   aKeyValueLen, 
                                   sNewChildPID,
                                   aMode, 
                                   aIndex->mLogging );
        if ( sRc != IDE_SUCCESS )
        {
            ideLog::log( IDE_ERR_0,
                         "KeyMap sequence : %"ID_INT32_FMT""
                         "Move start sequence : %"ID_INT32_FMT""
                         ", key value length : %"ID_UINT32_FMT""
                         ", mode : %"ID_UINT32_FMT"\n",
                         aKeyMapSeq, 
                         sMoveStartSeq, 
                         *aKeyValueLen, 
                         aMode );
            dumpIndexNode( sNextPage );
            IDE_ASSERT_MSG( 0, "Error Ouccurs in DRDB Index ID : %"ID_UINT32_FMT, aIndex->mIndexID );
        }
        else
        {
            /* nothing to do */
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_CONT(ERR_KEY_REDISTRIBUTE);

    *aCanRedistribute = ID_FALSE;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    *aCanRedistribute = ID_FALSE;

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : sdnbBTree::insertLKey                      *
 * ------------------------------------------------------------------*
 * Internal node�� aNode �� slot�� insert�Ѵ�.                       *
 *********************************************************************/
IDE_RC sdnbBTree::insertLKey( sdrMtx          * aMtx,
                              sdnbHeader      * aIndex,
                              sdpPhyPageHdr   * aNode,
                              SShort            aSlotSeq,
                              UChar             aCTSlotNum,
                              UChar             aTxBoundType,
                              sdnbKeyInfo     * aKeyInfo,
                              UShort            aKeyValueSize,
                              idBool            aIsLoggableSlot,
                              UShort          * aKeyOffset ) // Out Parameter
{
    UShort                sAllowedSize;
    sdnbLKey            * sLKey          = NULL;
    UShort                sKeyLength;
    sdnbRollbackContext   sRollbackContext;
    sdnbNodeHdr         * sNodeHdr;
    scOffset              sKeyOffset     = SC_NULL_OFFSET;
    smSCN                 sFstDskViewSCN;
    sdSID                 sTSSlotSID;

    sFstDskViewSCN = smLayerCallback::getFstDskViewSCN( aMtx->mTrans );
    sTSSlotSID     = smLayerCallback::getTSSlotSID( aMtx->mTrans );

    sKeyLength = SDNB_LKEY_LEN( aKeyValueSize, aTxBoundType );

    sNodeHdr = (sdnbNodeHdr*)sdpPhyPage::getLogicalHdrStartPtr( (UChar *)aNode );

    IDE_TEST( sdpPhyPage::allocSlot( (sdpPhyPageHdr*)aNode,
                                     aSlotSeq,
                                     sKeyLength,
                                     ID_TRUE,
                                     &sAllowedSize,
                                     (UChar **)&sLKey,
                                     &sKeyOffset,
                                     1 )
              != IDE_SUCCESS );

    if ( aKeyOffset != NULL )    // aKeyOffset�� NULL�� �ƴ� ���, Return�ش޶�� ��
    {
        *aKeyOffset = sKeyOffset;
    }
    else
    {
        /* nothing to do */
    }

    IDE_DASSERT( aKeyInfo->mKeyState == SDNB_KEY_UNSTABLE ||
                 aKeyInfo->mKeyState == SDNB_KEY_STABLE );
    IDE_DASSERT( sAllowedSize >= sKeyLength );
    idlOS::memset(sLKey, 0x00, sKeyLength );

    SDNB_KEYINFO_TO_LKEY( (*aKeyInfo), aKeyValueSize, sLKey,
                          aCTSlotNum,           //CCTS_NO
                          SDNB_DUPKEY_NO,       //DUPLICATED
                          SDN_CTS_INFINITE,     //LCTS_NO
                          aKeyInfo->mKeyState,  //STATE
                          aTxBoundType );       //TB_TYPE

    IDE_DASSERT( (SDNB_GET_STATE( sLKey ) == SDNB_KEY_UNSTABLE) ||
                 (SDNB_GET_STATE( sLKey ) == SDNB_KEY_STABLE) );

    if ( aTxBoundType == SDNB_KEY_TB_KEY )
    {
        SDNB_SET_TBK_CSCN( ((sdnbLKeyEx*)sLKey), &sFstDskViewSCN );
        SDNB_SET_TBK_CTSS( ((sdnbLKeyEx*)sLKey), &sTSSlotSID );
    }
    else
    {
        /* nothing to do */
    }

    sRollbackContext.mTableOID   = aIndex->mTableOID;
    sRollbackContext.mIndexID    = aIndex->mIndexID;

    sNodeHdr->mUnlimitedKeyCount++;

    /* BUG-44973 */
    if ( aTxBoundType == SDNB_KEY_TB_KEY )
    {
        sNodeHdr->mTBKCount++;
    }
    else
    {
        /* nothing to do */
    }

    if ( ( aIsLoggableSlot == ID_TRUE ) && ( aIndex->mLogging == ID_TRUE ) )
    {
        if ( aTxBoundType == SDNB_KEY_TB_KEY )
        {
            IDE_TEST( sdrMiniTrans::writeNBytes( aMtx,
                                                 (UChar *)&(sNodeHdr->mTBKCount),
                                                 (void *)&(sNodeHdr->mTBKCount),
                                                 ID_SIZEOF(UShort) )
                      != IDE_SUCCESS );
        }
        else
        {
            /* nothing to do */
        }

        IDE_TEST( sdrMiniTrans::writeNBytes( aMtx,
                                             (UChar *)&(sNodeHdr->mUnlimitedKeyCount),
                                             (void *)&sNodeHdr->mUnlimitedKeyCount,
                                             ID_SIZEOF(UShort) )
                  != IDE_SUCCESS );

        sdrMiniTrans::setRefOffset( aMtx, SM_OID_NULL );
        // write log
        IDE_TEST( sdrMiniTrans::writeLogRec( aMtx,
                                             (UChar *)sLKey,
                                             (void *)NULL,
                                             ID_SIZEOF(SShort) + sKeyLength +
                                             ID_SIZEOF(sdnbRollbackContext),
                                             SDR_SDN_INSERT_UNIQUE_KEY )
                  != IDE_SUCCESS );

        IDE_TEST( sdrMiniTrans::write( aMtx,
                                       (void *)&aSlotSeq,
                                       ID_SIZEOF(SShort) )
                  != IDE_SUCCESS );

        IDE_TEST( sdrMiniTrans::write( aMtx,
                                       (void *)&sRollbackContext,
                                       ID_SIZEOF(sdnbRollbackContext))
                  != IDE_SUCCESS );

        IDE_TEST( sdrMiniTrans::write( aMtx,
                                       (void *)sLKey,
                                       sKeyLength )
                  != IDE_SUCCESS );
    }
    else
    {
        /* nothing to do */
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

/*********************************************************************
 * FUNCTION DESCRIPTION : sdnbBTree::isLastChildLeaf                 *
 * ------------------------------------------------------------------*
 * Parent�� ���� ������ Child node������ Ȯ���Ѵ�.                   *
 * : traverse �ÿ� stack���� �̸� last�� check�Ͽ� ����س��´�.     *
 *********************************************************************/
idBool sdnbBTree::isLastChildLeaf(sdnbStack * aStack)
{
    return aStack->mStack[aStack->mIndexDepth].mNextNode == SD_NULL_PID ?
        ID_TRUE : ID_FALSE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : sdnbBTree::isLastChildInternal             *
 * ------------------------------------------------------------------*
 * Parent�� ���� ������ Child node������ Ȯ���Ѵ�.                   *
 * : traverse �ÿ� stack���� �̸� last�� check�Ͽ� ����س��´�.     *
 *********************************************************************/
idBool sdnbBTree::isLastChildInternal(sdnbStack         *aStack)
{
    return aStack->mStack[aStack->mCurrentDepth].mNextNode == SD_NULL_PID ?
        ID_TRUE : ID_FALSE;
}


/*********************************************************************
 * FUNCTION DESCRIPTION : sdnbBTree::processLeafFull                 *
 * ------------------------------------------------------------------*
 * Leaf node�� Full �����϶� ȣ��ȴ�.                               *
 * ������ Leaf�� ��쿡�� unbalanced split�� �ϰ� �ǰ�,              *
 * �� ���� ��쿡�� key redistribution �Ǵ� balanced 1-to-2 split��  *
 * �����Ѵ�.                                                         *
 *********************************************************************/
IDE_RC sdnbBTree::processLeafFull(idvSQL         * aStatistics,
                                  sdnbStatistic  * aIndexStat,
                                  sdrMtx         * aMtx,
                                  idBool         * aMtxStart,
                                  smnIndexHeader * aPersIndex,
                                  sdnbHeader     * aIndex,
                                  sdnbStack      * aStack,
                                  sdpPhyPageHdr  * aNode,
                                  sdnbKeyInfo    * aKeyInfo,
                                  UShort           aKeySize,
                                  SShort           aInsertSeq,
                                  sdpPhyPageHdr ** aTargetNode,
                                  SShort         * aTargetSlotSeq,
                                  UInt           * aAllocPageCount,
                                  idBool           aIsDeleteLeafKey )
{
    idBool  sCanRedistribute = ID_TRUE;

    if ( isLastChildLeaf( aStack ) == ID_TRUE )
    {
        IDE_TEST(splitLeafNode(aStatistics,
                               aIndexStat,
                               aMtx,
                               aMtxStart,
                               aPersIndex,
                               aIndex,
                               aStack,
                               aNode,
                               aKeyInfo,
                               aKeySize,
                               aInsertSeq,
                               smuProperty::getUnbalancedSplitRate(),
                               aTargetNode,
                               aTargetSlotSeq,
                               aAllocPageCount,
                               aIsDeleteLeafKey )
                 != IDE_SUCCESS );
    }
    else
    {
        // Key Redistribution
        IDE_TEST(redistributeKeyLeaf(aStatistics,
                                     aIndexStat,
                                     aMtx,
                                     aMtxStart,
                                     aPersIndex,
                                     aIndex,
                                     aStack,
                                     aNode,
                                     aKeyInfo,
                                     aKeySize,
                                     aInsertSeq,
                                     aTargetNode,
                                     aTargetSlotSeq,
                                     &sCanRedistribute,
                                     aAllocPageCount)
                 != IDE_SUCCESS );

        if ( sCanRedistribute != ID_TRUE )
        {
            /* Key ��й踦 ���� ���ϴ� ��Ȳ�̸�,
             * ������ 50:50 1-to-2 split�� �Ѵ�. */
            IDE_TEST(splitLeafNode(aStatistics,
                                   aIndexStat,
                                   aMtx,
                                   aMtxStart,
                                   aPersIndex,
                                   aIndex,
                                   aStack,
                                   aNode,
                                   aKeyInfo,
                                   aKeySize,
                                   aInsertSeq,
                                   50,
                                   aTargetNode,
                                   aTargetSlotSeq,
                                   aAllocPageCount,
                                   aIsDeleteLeafKey )
                     != IDE_SUCCESS );
        }
        else
        {
            /* nothing to do */
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    /* SMO �߰��� ���а� �߻��ϸ�, �׶����� ���İ��� ��� Node����
       SMO no�� ������ �����̹Ƿ�, �̸� �ٽ� �ǵ����� �ͺ���
       Index�� SMO no�� �������Ѽ� �������ش�.
    */
    // aIndex->mSmoNo++;
    increaseSmoNo( aIndex );
    
    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : sdnbBTree::processInternalFull             *
 * ------------------------------------------------------------------*
 * Internal node�� Full �����϶� ȣ��ȴ�.                           *
 * ������ Child �� ��쿡�� unbalanced split�� �ϰ� �ǰ�,            *
 * �� ���� ��쿡�� key redistribution �Ǵ� balanced 1-to-2 split��  *
 * �����Ѵ�.                                                         *
 *********************************************************************/
IDE_RC sdnbBTree::processInternalFull(idvSQL         * aStatistics,
                                      sdnbStatistic  * aIndexStat,
                                      sdrMtx         * aMtx,
                                      idBool         * aMtxStart,
                                      smnIndexHeader * aPersIndex,
                                      sdnbHeader     * aIndex,
                                      sdnbStack      * aStack,
                                      sdnbKeyInfo    * aKeyInfo,
                                      UShort         * aKeySize,
                                      SShort           aKeyMapSeq,
                                      UInt             aMode,
                                      sdpPhyPageHdr  * aNode,
                                      sdpPhyPageHdr ** aNewChildNode,
                                      UInt           * aAllocPageCount)
{
    idBool sCanRedistribute;

    if ( isLastChildInternal( aStack ) == ID_TRUE )
    {
        IDE_TEST(splitInternalNode(aStatistics,
                                   aIndexStat,
                                   aMtx,
                                   aMtxStart,
                                   aPersIndex,
                                   aIndex,
                                   aStack,
                                   aKeyInfo,
                                   aKeySize,
                                   aMode,
                                   smuProperty::getUnbalancedSplitRate(),
                                   aNode,
                                   aNewChildNode,
                                   aAllocPageCount)
                 != IDE_SUCCESS );
    }
    else
    {
        // Key redistribution �õ�
        IDE_TEST(redistributeKeyInternal(aStatistics,
                                         aIndexStat,
                                         aMtx,
                                         aMtxStart,
                                         aPersIndex,
                                         aIndex,
                                         aStack,
                                         aKeyInfo,
                                         aKeySize,
                                         aKeyMapSeq,
                                         aMode,
                                         &sCanRedistribute,
                                         aNode,
                                         aNewChildNode,
                                         aAllocPageCount)
                 != IDE_SUCCESS );

        if ( sCanRedistribute != ID_TRUE )
        {
            /* Key ��й踦 ���� ���ϴ� ��Ȳ�̸�,
             * ������ 50:50 1-to-2 split�� �Ѵ�. */
            IDE_TEST(splitInternalNode(aStatistics,
                                       aIndexStat,
                                       aMtx,
                                       aMtxStart,
                                       aPersIndex,
                                       aIndex,
                                       aStack,
                                       aKeyInfo,
                                       aKeySize,
                                       aMode,
                                       50,
                                       aNode,
                                       aNewChildNode,
                                       aAllocPageCount)
                     != IDE_SUCCESS );
        }
        else
        {
            /* nothing to do */
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : sdnbBTree::splitLeafNode                   *
 * ------------------------------------------------------------------*
 * ���ο� key�� Insert �ϱ� ���� leaf node�� split �ϰ� insert �Ѵ�. *
 *                                                                   *
 * �� �Լ��� SMO latch(tree latch)�� x latch�� ���� ���¿� ȣ��Ǹ�, *
 * Split�� ������ �� ���� ����� �ִ� Ű�� �� ����� �ּ�Ű�� prefix *
 * �� ���ؼ� ���� Internal node�� insert�Ѵ�. ��Ȳ�� ���� ���� ���  *
 * �� split�� �߻��� �� �ִ�.                                        *
 *                                                                   *
 * �ϳ��� mtx���� ������ allocPage�� ������ ������ �� ���⶧����     *
 * SMO�� �ֻ�� ��忡�� �ѹ��� �ʿ��� ������ ��ŭ �Ҵ��ϰ�, ����    *
 * ���鼭 ���� split�� �����Ѵ�.                                     *
 *********************************************************************/
IDE_RC sdnbBTree::splitLeafNode( idvSQL         * aStatistics,
                                 sdnbStatistic  * aIndexStat,
                                 sdrMtx         * aMtx,
                                 idBool         * aMtxStart,
                                 smnIndexHeader * aPersIndex,
                                 sdnbHeader     * aIndex,
                                 sdnbStack      * aStack,
                                 sdpPhyPageHdr  * aNode,
                                 sdnbKeyInfo    * aKeyInfo,
                                 UShort           aKeySize,
                                 SShort           aInsertSeq,
                                 UShort           aPCTFree,
                                 sdpPhyPageHdr ** aTargetNode,
                                 SShort         * aTargetSlotSeq,
                                 UInt           * aAllocPageCount,
                                 idBool           aIsDeleteLeafKey )
{
    sdpPhyPageHdr       * sNewPage;
    sdpPhyPageHdr       * sNextPage;
    sdnbLKey            * sLKey;
    scPageID              sLeftMostPID = SD_NULL_PID;
    UShort                sKeyLength;
    sdnbKeyInfo           sPropagateKeyInfo;
    UShort                sPropagateKeyValueSize;
    UChar               * sSlotDirPtr;
    UShort                sTotalKeyCount;
    UShort                sMoveStartSeq;
    UShort                sSplitPoint;
    IDE_RC                sRc;
    ULong                 sSmoNo;
    
    sKeyLength  = SDNB_LKEY_LEN( aKeySize, SDNB_KEY_TB_KEY );
    IDE_TEST( findSplitPoint( aIndex,
                              aNode,
                              0,
                              aInsertSeq,
                              sKeyLength,
                              aPCTFree,
                              SDNB_SMO_MODE_SPLIT_1_2,
                              aIsDeleteLeafKey,
                              &sSplitPoint )
              != IDE_SUCCESS );

    sSlotDirPtr    = sdpPhyPage::getSlotDirStartPtr( (UChar *)aNode );
    sTotalKeyCount = sdpSlotDirectory::getCount( sSlotDirPtr );

    // ���� �������� SMONo�� ���� SmoNo + 1 ����
    getSmoNo( (void *)aIndex, &sSmoNo );
    sdpPhyPage::setIndexSMONo((UChar *)aNode, sSmoNo + 1);
    IDL_MEM_BARRIER;

    aStack->mCurrentDepth = aStack->mIndexDepth - 1;

    if ( sSplitPoint == SDNB_SPLIT_POINT_NEW_KEY ) // new key should be propagated
    {
        idlOS::memcpy( &sPropagateKeyInfo, aKeyInfo, ID_SIZEOF( sdnbKeyInfo ) );

        sPropagateKeyValueSize = aKeySize;
        sMoveStartSeq = aInsertSeq;
    }
    else
    {
        IDE_TEST( sdpSlotDirectory::getPagePtrFromSlotNum( sSlotDirPtr,
                                                           sSplitPoint,
                                                           (UChar **)&sLKey )
                  != IDE_SUCCESS );
        SDNB_LKEY_TO_KEYINFO( sLKey, sPropagateKeyInfo );

        sPropagateKeyValueSize = getKeyValueLength( &(aIndex->mColLenInfoList),
                                                    SDNB_LKEY_KEY_PTR(  sLKey ) );
        sMoveStartSeq = sSplitPoint;
    }

    /* BUG-32313: The values of DRDB index Cardinality converge on 0
     * 
     * ���� Ű���� ���� Ű���� �� ��忡 ���� ����� ���,
     * NumDist�� �����Ѵ�. */
    if ( needToUpdateStat() == ID_TRUE )
    {
        IDE_TEST( adjustNumDistStatByDistributeKeys( aPersIndex,
                                                     aIndex,
                                                     aMtx,
                                                     &sPropagateKeyInfo,
                                                     aNode,
                                                     sMoveStartSeq,
                                                     SDNB_SMO_MODE_SPLIT_1_2 )
                  != IDE_SUCCESS );
    }
    else
    {
        /* nothing to do ... */
    }

    IDE_TEST( propagateKeyInternalNode( aStatistics,
                                        aIndexStat,
                                        aMtx,
                                        aMtxStart,
                                        aPersIndex,
                                        aIndex,
                                        aStack,
                                        &sPropagateKeyInfo,
                                        &sPropagateKeyValueSize,
                                        SDNB_SMO_MODE_SPLIT_1_2,
                                        &sNewPage,
                                        aAllocPageCount)
              != IDE_SUCCESS );

    aStack->mCurrentDepth++;

    // �� ����� SMONo�� ���� SmoNo + 1 ����
    getSmoNo( (void *)aIndex, &sSmoNo );
    sdpPhyPage::setIndexSMONo( (UChar *)sNewPage, sSmoNo + 1 );

    IDE_TEST( initializeNodeHdr( aMtx,
                                 sNewPage,
                                 sLeftMostPID,
                                 0,
                                 aIndex->mLogging )
              != IDE_SUCCESS );

    IDE_TEST( sdnIndexCTL::init( aMtx,
                                 &aIndex->mSegmentDesc.mSegHandle,
                                 sNewPage,
                                 sdnIndexCTL::getUsedCount( aNode ) )
              != IDE_SUCCESS );

    IDL_MEM_BARRIER;

    if ( sMoveStartSeq <= (sTotalKeyCount - 1) )
    {
        // ���� ����� aInsertSeq ���� ������ �� ���� �̵��ϸ鼭 �����Ѵ�.
        sRc = moveSlots( aMtx,
                         aIndex,
                         aNode,
                         aStack->mIndexDepth - aStack->mCurrentDepth,
                         sMoveStartSeq,
                         sTotalKeyCount - 1,
                         sNewPage );
        if ( sRc != IDE_SUCCESS )
        {
            ideLog::log( IDE_ERR_0,
                         "move from <%"ID_INT32_FMT"> to <%"ID_UINT32_FMT">"
                         "\nIndex depth : %"ID_INT32_FMT""
                         ", Index current depth : %"ID_INT32_FMT"\n",
                         sMoveStartSeq, sTotalKeyCount - 1,
                         aStack->mIndexDepth, aStack->mCurrentDepth );
            dumpIndexNode( aNode );
            dumpIndexNode( sNewPage );
            IDE_ASSERT_MSG( 0, "Error Ouccurs in DRDB Index ID : %"ID_UINT32_FMT, aIndex->mIndexID );
        }
    }

    // �� ��带 ���� ��� �������� link��Ų��.
    sdpDblPIDList::setPrvOfNode( &sNewPage->mListNode,
                                 aNode->mPageID,
                                 aMtx);
    sdpDblPIDList::setNxtOfNode( &sNewPage->mListNode,
                                 aNode->mListNode.mNext,
                                 aMtx);
    if ( aNode->mListNode.mNext != SD_NULL_PID )
    {
        sNextPage = (sdpPhyPageHdr*)sdrMiniTrans::getPagePtrFromPageID(
                              aMtx, aIndex->mIndexTSID, aNode->mListNode.mNext );
        if ( sNextPage == NULL )
        {
            ideLog::log( IDE_ERR_0,
                         "index TSID : %"ID_UINT32_FMT""
                         ", get page ID : %"ID_UINT32_FMT""
                         "\n",
                         aIndex->mIndexTSID, aNode->mListNode.mNext );
            dumpHeadersAndIteratorToSMTrc( NULL,
                                           aIndex,
                                           NULL );
            dumpIndexNode( aNode );
            IDE_ASSERT( 0 );
        }

        sdpDblPIDList::setPrvOfNode( &sNextPage->mListNode,
                                     sNewPage->mPageID,
                                     aMtx);
    }

    sdpDblPIDList::setNxtOfNode( &aNode->mListNode,
                                 sNewPage->mPageID,
                                 aMtx );

    if ( sSplitPoint == SDNB_SPLIT_POINT_NEW_KEY ) // new key should be propagated
    {
        *aTargetNode = sNewPage;
        *aTargetSlotSeq = 0;
    }
    else
    {
        if (aInsertSeq <= sSplitPoint)
        {
            *aTargetNode = aNode;
            *aTargetSlotSeq = aInsertSeq;
        }
        else
        {
            *aTargetNode = sNewPage;
            *aTargetSlotSeq = aInsertSeq - sSplitPoint;
        }
    }

    if ( needToUpdateStat() == ID_TRUE )
    {
        IDE_TEST( adjustMaxNode( aStatistics,
                                 aIndexStat,
                                 aIndex,
                                 aMtx,
                                 aKeyInfo,
                                 aNode,
                                 sNewPage,
                                 *aTargetNode,
                                 *aTargetSlotSeq )
                  != IDE_SUCCESS );
    }
    else
    {
        /* nothing to do ... */
    }

    IDE_DASSERT( validateLeaf( aStatistics,
                               aNode,
                               aIndexStat,
                               aIndex,
                               aIndex->mColumns,
                               aIndex->mColumnFence )
                 == IDE_SUCCESS );

    IDE_DASSERT( validateLeaf( aStatistics,
                               sNewPage,
                               aIndexStat,
                               aIndex,
                               aIndex->mColumns,
                               aIndex->mColumnFence )
                 == IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : sdnbBTree::initializeNodeHdr               *
 * ------------------------------------------------------------------*
 * Index node�� Header(sdnbNodeHdr)�� �ʱ�ȭ�Ѵ�. �� Index node��    *
 * �Ҵ�ް� �Ǹ�, �ݵ�� �� �Լ��� ���Ͽ� Node header�� �ʱ�ȭ�ϰ�   *
 * �Ǹ�, Logging option�� ����, logging/no-logging�� �����Ѵ�.     *
 *********************************************************************/
IDE_RC sdnbBTree::initializeNodeHdr(sdrMtx *         aMtx,
                                    sdpPhyPageHdr *  aPage,
                                    scPageID         aLeftmostChild,
                                    UShort           aHeight,
                                    idBool           aIsLogging)
{
    sdnbNodeHdr * sNodeHdr;
    UShort        sKeyCount  = 0;
    UChar         sNodeState = SDNB_IN_USED;
    UShort        sTotalDeadKeySize = 0;
    scPageID      sEmptyPID = SD_NULL_PID;
    UShort        sTBKCount = 0;

    sNodeHdr = (sdnbNodeHdr*)sdpPhyPage::getLogicalHdrStartPtr((UChar *)aPage);

    if (aIsLogging == ID_TRUE)
    {
        IDE_TEST(sdrMiniTrans::writeNBytes(aMtx,
                                           (UChar *)&sNodeHdr->mHeight,
                                           (void *)&aHeight,
                                           ID_SIZEOF(aHeight))
                 != IDE_SUCCESS );
        IDE_TEST(sdrMiniTrans::writeNBytes(aMtx,
                                           (UChar *)&sNodeHdr->mLeftmostChild,
                                           (void *)&aLeftmostChild,
                                           ID_SIZEOF(aLeftmostChild))
                 != IDE_SUCCESS );
        IDE_TEST( sdrMiniTrans::writeNBytes( aMtx,
                                             (UChar *)&sNodeHdr->mNextEmptyNode,
                                             (void *)&sEmptyPID,
                                             ID_SIZEOF(sNodeHdr->mNextEmptyNode) )
                  != IDE_SUCCESS );

        IDE_TEST(sdrMiniTrans::writeNBytes(aMtx,
                                           (UChar *)&sNodeHdr->mUnlimitedKeyCount,
                                           (void *)&sKeyCount,
                                           ID_SIZEOF(sKeyCount))
                 != IDE_SUCCESS );
        IDE_TEST(sdrMiniTrans::writeNBytes(aMtx,
                                           (UChar *)&sNodeHdr->mState,
                                           (void *)&sNodeState,
                                           ID_SIZEOF(sNodeState))
                 != IDE_SUCCESS );
        IDE_TEST(sdrMiniTrans::writeNBytes(aMtx,
                                           (UChar *)&sNodeHdr->mTotalDeadKeySize,
                                           (void *)&sTotalDeadKeySize,
                                           ID_SIZEOF(sTotalDeadKeySize))
                 != IDE_SUCCESS );
        IDE_TEST(sdrMiniTrans::writeNBytes(aMtx,
                                           (UChar *)&sNodeHdr->mTBKCount,
                                           (void *)&sTBKCount,
                                           ID_SIZEOF(sTBKCount))
                 != IDE_SUCCESS );
    }
    else
    {
        sNodeHdr->mHeight            = aHeight;
        sNodeHdr->mLeftmostChild     = aLeftmostChild;
        sNodeHdr->mNextEmptyNode     = sEmptyPID;
        sNodeHdr->mUnlimitedKeyCount = sKeyCount;
        sNodeHdr->mTotalDeadKeySize  = sTotalDeadKeySize;
        sNodeHdr->mState             = sNodeState;
        sNodeHdr->mTBKCount          = sTBKCount;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : sdnbBTree::redistributeKeyLeaf             *
 * ------------------------------------------------------------------*
 * Next Leaf node�� Key ��й踦 �����Ѵ�.                           *
 * ��й踦 �õ��Ͽ� �����ϸ� ��й踦 ������,                       *
 * �׷��� ������ balanced 1-to-1 split�� �ϰ� �ȴ�.                  *
 *********************************************************************/
IDE_RC sdnbBTree::redistributeKeyLeaf(idvSQL         * aStatistics,
                                      sdnbStatistic  * aIndexStat,
                                      sdrMtx         * aMtx,
                                      idBool         * aMtxStart,
                                      smnIndexHeader * aPersIndex,
                                      sdnbHeader     * aIndex,
                                      sdnbStack      * aStack,
                                      sdpPhyPageHdr  * aNode,
                                      sdnbKeyInfo    * aKeyInfo,
                                      UShort           aKeySize,
                                      SShort           aInsertSeq,
                                      sdpPhyPageHdr ** aTargetNode,
                                      SShort         * aTargetSlotSeq,
                                      idBool         * aCanRedistribute,
                                      UInt           * aAllocPageCount)
{
    scPageID            sNextPID;
    sdpPhyPageHdr     * sNextPage;
    SShort              sMoveStartSeq;
    sdpPhyPageHdr     * sNewPage = NULL;
    scPageID            sRightChildPID;
    UChar             * sSlotDirPtr;
    UShort              sCurSlotCount;
    sdnbKeyInfo         sPropagateKeyInfo;
    UShort              sPropagateKeyValueSize;
    UChar               sAgedCount;
    // Move Key�� ������ ��ҷμ�, Key�� Page size 1/2�� ������ �����Ƿ�
    // Page size ���� ũ���� ���۸� �Ҵ��Ѵ�.
    ULong               sTmpBuf[SDNB_MAX_KEY_BUFFER_SIZE/ID_SIZEOF(ULong)];
    IDE_RC              sRc;
    ULong               sSmoNo;

    sNextPID  = sdpPhyPage::getNxtPIDOfDblList( aNode );
    sNextPage = (sdpPhyPageHdr*)sdrMiniTrans::getPagePtrFromPageID(
                                                            aMtx, 
                                                            aIndex->mIndexTSID, 
                                                            sNextPID );

    if ( sNextPage == NULL )
    {
        ideLog::log( IDE_ERR_0,
                     "index TSID : %"ID_UINT32_FMT""
                     ", get page ID : %"ID_UINT32_FMT""
                     "\n",
                     aIndex->mIndexTSID, aNode->mListNode.mNext );
        dumpHeadersAndIteratorToSMTrc( NULL,
                                       aIndex,
                                       NULL );
        dumpIndexNode( aNode );
        IDE_ASSERT( 0 );
    }
    else
    {
        /* nothing to do */
    }

    aStack->mCurrentDepth = aStack->mIndexDepth;

    IDE_TEST( selfAging( aStatistics,
                         aIndex,
                         aMtx,
                         (sdpPhyPageHdr*)sNextPage,
                         &sAgedCount )
              != IDE_SUCCESS );

    IDE_TEST( findRedistributeKeyPoint( aIndex,
                                        aStack,
                                        &aKeySize,
                                        aInsertSeq,
                                        SDNB_SMO_MODE_SPLIT_1_2,
                                        aNode,
                                        sNextPage,
                                        ID_TRUE, //aIsLeaf
                                        &sMoveStartSeq,
                                        aCanRedistribute )
              != IDE_SUCCESS );

    IDE_TEST_RAISE( *aCanRedistribute != ID_TRUE, ERR_KEY_REDISTRIBUTE );

    // ���� index SmoNo + 1 �� ��忡 ����
    getSmoNo( (void *)aIndex, &sSmoNo );
    sdpPhyPage::setIndexSMONo((UChar *)aNode, sSmoNo + 1);
    sdpPhyPage::setIndexSMONo((UChar *)sNextPage, sSmoNo + 1);
    IDL_MEM_BARRIER;

    /* Prefix Key ���ϱ� : Current node���� �̵��� ù��° Key */
    IDE_TEST( getKeyInfoFromSlot( aIndex,
                                  aNode,
                                  sMoveStartSeq,
                                  ID_TRUE, //aIsLeaf
                                  &sPropagateKeyInfo,
                                  &sPropagateKeyValueSize,
                                  &sRightChildPID )
              != IDE_SUCCESS );

    idlOS::memcpy( (UChar *)sTmpBuf,
                   sPropagateKeyInfo.mKeyValue,
                   sPropagateKeyValueSize );
    sPropagateKeyInfo.mKeyValue = (UChar *)sTmpBuf;

    /* No logging���� Key ��й踦 �����Ѵ�. */
    sSlotDirPtr   = sdpPhyPage::getSlotDirStartPtr( (UChar *)aNode );
    sCurSlotCount = sdpSlotDirectory::getCount( sSlotDirPtr );

    /* BUG-32313: The values of DRDB index Cardinality converge on 0
     * 
     * ���� Ű���� ���� Ű���� �� ��忡 ���� ����� ���,
     * NumDist�� �����Ѵ�. */
    if ( needToUpdateStat() == ID_TRUE )
    {
        IDE_TEST( adjustNumDistStatByDistributeKeys( aPersIndex,
                                                     aIndex,
                                                     aMtx,
                                                     &sPropagateKeyInfo,
                                                     aNode,
                                                     sMoveStartSeq,
                                                     SDNB_SMO_MODE_KEY_REDIST )
                  != IDE_SUCCESS );
    }
    else
    {
        /* nothing to do ... */
    }

    /* No logging���� ������ ������ �Ŀ�, ����� ���� Prefix Key��
     * ���� Internal node�� �����Ѵ�. �̴� SMO �ֻ�ܿ��� �Ʒ��� SMO��
     * �߻��ؾ� �ϱ� �����̴�. �� ������ Logging ���� �����ϵ��� �Ѵ�.
     */
    /* Key propagation */
    aStack->mCurrentDepth --;
    IDE_TEST( propagateKeyInternalNode( aStatistics,
                                        aIndexStat,
                                        aMtx,
                                        aMtxStart,
                                        aPersIndex,
                                        aIndex,
                                        aStack,
                                        &sPropagateKeyInfo,
                                        &sPropagateKeyValueSize,
                                        SDNB_SMO_MODE_KEY_REDIST,
                                        &sNewPage,
                                        aAllocPageCount)
              != IDE_SUCCESS );
    aStack->mCurrentDepth ++;

    // Key ��й�ÿ��� ���� �Ҵ�޴� �������� ����� �Ѵ�.
    IDE_ASSERT_MSG( sNewPage == NULL, 
                    "Error Ouccurs in DRDB Index ID : %"ID_UINT32_FMT, aIndex->mIndexID );

    sRc = moveSlots( aMtx,
                     aIndex,
                     aNode,
                     aStack->mIndexDepth - aStack->mCurrentDepth,
                     (SShort)sMoveStartSeq,
                     sCurSlotCount - 1,
                     sNextPage );
    if ( sRc != IDE_SUCCESS )
    {
        ideLog::log( IDE_ERR_0,
                     "move from <%"ID_INT32_FMT"> to <%"ID_UINT32_FMT">"
                     "\nIndex depth : %"ID_INT32_FMT""
                     ", Index current depth : %"ID_INT32_FMT"\n",
                     sMoveStartSeq, sCurSlotCount - 1,
                     aStack->mIndexDepth, aStack->mCurrentDepth );
        dumpIndexNode( aNode );
        dumpIndexNode( sNextPage );
        IDE_ASSERT_MSG( 0, "Error Ouccurs in DRDB Index ID : %"ID_UINT32_FMT, aIndex->mIndexID );
    }

    if ( (UShort)aInsertSeq <= sMoveStartSeq )
    {
        *aTargetNode = aNode;
        *aTargetSlotSeq = aInsertSeq;
    }
    else
    {
        *aTargetNode = sNextPage;
        *aTargetSlotSeq = aInsertSeq - sMoveStartSeq;
    }

    if ( needToUpdateStat() == ID_TRUE )
    {
        IDE_TEST( adjustMaxNode( aStatistics,
                                 aIndexStat,
                                 aIndex,
                                 aMtx,
                                 aKeyInfo,
                                 aNode,
                                 sNextPage,
                                 *aTargetNode,
                                 *aTargetSlotSeq )
                  != IDE_SUCCESS );
    }
    else
    {
        /* nothing to do ... */
    }


    IDE_DASSERT( validateLeaf( aStatistics,
                               aNode,
                               aIndexStat,
                               aIndex,
                               aIndex->mColumns,
                               aIndex->mColumnFence )
                 == IDE_SUCCESS );

    IDE_DASSERT( validateLeaf( aStatistics,
                               sNextPage,
                               aIndexStat,
                               aIndex,
                               aIndex->mColumns,
                               aIndex->mColumnFence )
                 == IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_KEY_REDISTRIBUTE );

    *aCanRedistribute = ID_FALSE;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    *aCanRedistribute = ID_FALSE;

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : sdnbBTree::getKeyInfoFromPIDAndKey         *
 * ------------------------------------------------------------------*
 * Page ID�� Slot ��ȣ�� ������, Key�� ���� ������ �����Ѵ�. �̶�    *
 * �����ϴ� PID�� buffer�� fix���� ���� ���¿��߸� �ϸ�, �� �Լ�     *
 * ���������� �ش� PID�� fix�ϰ� Key ������ ������ ���� buffer unfix *
 * ���� �����Ѵ�.                                                    *
 *********************************************************************/
IDE_RC sdnbBTree::getKeyInfoFromPIDAndKey( idvSQL *         aStatistics,
                                           sdnbStatistic *  aIndexStat,
                                           sdnbHeader *     aIndex,
                                           scPageID         aPID,
                                           UShort           aSeq,
                                           idBool           aIsLeaf,
                                           sdnbKeyInfo *    aKeyInfo,
                                           UShort *         aKeyValueLen,
                                           scPageID *       aRightChild,
                                           UChar *          aKeyValueBuf )
{
    sdpPhyPageHdr  *sNode;
    idBool          sTrySuccess;
    idBool          sIsLatched = ID_FALSE;

    // Parent Node�� �����ϱ� ���� Buffer�� �÷��� Fix �Ѵ�
    IDE_TEST( sdnbBTree::getPage( aStatistics,
                                  &(aIndexStat->mIndexPage),
                                  aIndex->mIndexTSID,
                                  aPID,
                                  SDB_S_LATCH,
                                  SDB_WAIT_NORMAL,
                                  NULL,
                                  (UChar **)&sNode,
                                  &sTrySuccess) 
               != IDE_SUCCESS );
    sIsLatched = ID_TRUE;

    IDU_FIT_POINT( "2.BUG-43091@sdnbBTree::getKeyInfoFromPIDAndKey::Jump" );
    IDE_ERROR( getKeyInfoFromSlot(aIndex, sNode, aSeq, aIsLeaf,
                                  aKeyInfo, aKeyValueLen, aRightChild)
               == IDE_SUCCESS );

    // KeyInfo�� ����Ű�� Row ������ Fix�� Buffer ������ ���� �������̹Ƿ�
    // �̸� ���� �������ֵ��� �Ѵ�. �׷��� ������, unfix�� buffer�� ���Ŀ�
    // �߻��ϴ� �ٸ� �۾��� ���� ������ Key�� �Ѽյ� �� �ִ�.
    idlOS::memcpy(aKeyValueBuf, aKeyInfo->mKeyValue, *aKeyValueLen);
    aKeyInfo->mKeyValue = aKeyValueBuf;

    // unfix buffer page
    sIsLatched = ID_FALSE;
    IDE_TEST( sdbBufferMgr::releasePage( aStatistics,
                                         (UChar *)sNode )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if ( sIsLatched == ID_TRUE )
    {
        sIsLatched = ID_FALSE;
        (void)sdbBufferMgr::releasePage( aStatistics,
                                         (UChar *)sNode );
    }

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : sdnbBTree::getKeyInfoFromSlot              *
 * ------------------------------------------------------------------*
 * Node�� Slot ��ȣ�� ������, Key�� ���� ������ �����Ѵ�.            *
 *********************************************************************/
IDE_RC sdnbBTree::getKeyInfoFromSlot( sdnbHeader *    aIndex,
                                      sdpPhyPageHdr * aPage,
                                      UShort          aSeq,
                                      idBool          aIsLeaf,
                                      sdnbKeyInfo *   aKeyInfo,
                                      UShort *        aKeyValueLen,
                                      scPageID *      aRightChild )
{
    UChar           *sSlotDirPtr;
    sdnbIKey        *sIKey;
    sdnbLKey        *sLKey;
    

    sSlotDirPtr = sdpPhyPage::getSlotDirStartPtr( (UChar *)aPage );

    if ( aIsLeaf == ID_TRUE )
    {
         IDE_TEST( sdpSlotDirectory::getPagePtrFromSlotNum( sSlotDirPtr, 
                                                            aSeq, 
                                                            (UChar **)&sLKey )
                   != IDE_SUCCESS );
        *aKeyValueLen = getKeyValueLength( &(aIndex->mColLenInfoList),
                                           SDNB_LKEY_KEY_PTR( sLKey ));
        SDNB_LKEY_TO_KEYINFO( sLKey, (*aKeyInfo) );
    }
    else
    {
         IDE_TEST( sdpSlotDirectory::getPagePtrFromSlotNum( sSlotDirPtr, 
                                                            aSeq, 
                                                            (UChar **)&sIKey )
                   != IDE_SUCCESS );
        *aKeyValueLen = getKeyValueLength( &(aIndex->mColLenInfoList),
                                           SDNB_IKEY_KEY_PTR( sIKey ));
        SDNB_IKEY_TO_KEYINFO( sIKey, (*aKeyInfo) );
        SDNB_GET_RIGHT_CHILD_PID( aRightChild, sIKey );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : sdnbBTree::makeKeyArray                    *
 * ------------------------------------------------------------------*
 * Key array�� �����Ѵ�. 2 node�� Key��� ���� ���ԵǴ� Key�� ���Ͽ� *
 * ���� �����. Key�� ��� sort�Ǿ� �ִ� �����̹Ƿ�, size ���� ����  *
 * �� Ȱ���Ѵ�.                                                      *
 *********************************************************************/
IDE_RC sdnbBTree::makeKeyArray( sdnbHeader    * aIndex,
                                sdnbStack     * aStack,
                                UShort        * aKeySize,
                                SShort          aInsertSeq,
                                UInt            aMode,
                                idBool          aIsLeaf,
                                sdpPhyPageHdr * aNode,
                                sdpPhyPageHdr * aNextNode,
                                SShort        * aAllKeyCount,
                                sdnbKeyArray ** aKeyArray )
{
    UChar          * sSlot;
    sdnbKeyArray   * sKeyArray = NULL;
    UShort           sCurSlotCount;
    UShort           sNextSlotCount;
    UShort           sInfoSize;
    UShort           sSlotIdx;
    SShort           i;
    UChar          * sSlotDirPtr;
    UChar          * sNextSlotDirPtr;

    *aKeyArray = NULL;

    if ( aIsLeaf == ID_TRUE )
    {
        sInfoSize = ID_SIZEOF(sdnCTS) + SDNB_LKEY_HEADER_LEN_EX;
    }
    else
    {
        sInfoSize = SDNB_IKEY_HEADER_LEN;
    }
    
    sSlotDirPtr   = sdpPhyPage::getSlotDirStartPtr( (UChar *)aNode );
    sCurSlotCount = sdpSlotDirectory::getCount( sSlotDirPtr );

    sNextSlotDirPtr = sdpPhyPage::getSlotDirStartPtr( (UChar *)aNextNode );
    sNextSlotCount  = sdpSlotDirectory::getCount( sNextSlotDirPtr );

    if ( aMode == SDNB_SMO_MODE_KEY_REDIST )
    {
        *aAllKeyCount = sCurSlotCount + sNextSlotCount;
    }
    else
    {
        *aAllKeyCount = sCurSlotCount + sNextSlotCount + 1;
    }

    // Leaf�� �ƴ� ��쿡��, �������� branch key�� �ϳ� �� �����;� �ϹǷ�
    // Key�� ������ �ϳ� �� ������Ų��.
    if ( aIsLeaf != ID_TRUE )
    {
        (*aAllKeyCount) ++;
    }
    else
    {
        /* nothing to do */
    }

    /* sdnbBTree_makeKeyArray_malloc_KeyArray.tc */
    IDU_FIT_POINT_RAISE( "sdnbBTree::makeKeyArray::malloc::KeyArray",
                         insufficient_memory );

    IDE_TEST_RAISE( iduMemMgr::malloc( IDU_MEM_SM_SDN,
                                       ID_SIZEOF(sdnbKeyArray) * *aAllKeyCount,
                                       (void **)&sKeyArray,
                                       IDU_MEM_IMMEDIATE) != IDE_SUCCESS, 
                    insufficient_memory );

    // Split ����� 2���� ����� ��� Key�� ���Ե� Key�� ���Ͽ�
    // sKeyArray�� ���ʴ�� �����Ѵ�.

    IDL_MEM_BARRIER;

    // ù��° ��忡��, ���� ���Ե� KeySequence����
    sSlotIdx = 0;
    for ( i = 0 ; i < aInsertSeq ; i++ )
    {
        IDE_TEST( sdpSlotDirectory::getPagePtrFromSlotNum( sSlotDirPtr, 
                                                           i,
                                                           &sSlot )
                  != IDE_SUCCESS );
        sKeyArray[sSlotIdx].mLength = getKeyLength( &(aIndex->mColLenInfoList),
                                                    (UChar *)sSlot,
                                                    aIsLeaf);
        sKeyArray[sSlotIdx].mPage = SDNB_CURRENT_PAGE;
        sKeyArray[sSlotIdx].mSeq = i;
        sSlotIdx ++;
    }

    // ���� ���Ե� Key
    switch ( aMode )
    {
        case SDNB_SMO_MODE_SPLIT_1_2 :
        {
            sKeyArray[sSlotIdx].mLength = aKeySize[0] + sInfoSize;
            sKeyArray[sSlotIdx].mPage = SDNB_NO_PAGE;
            sKeyArray[sSlotIdx].mSeq = sSlotIdx;
            sSlotIdx ++;
        }
        break;

        case SDNB_SMO_MODE_KEY_REDIST :
        {
            sKeyArray[sSlotIdx].mLength = aKeySize[0] + sInfoSize;
            sKeyArray[sSlotIdx].mPage = SDNB_NO_PAGE;
            sKeyArray[sSlotIdx].mSeq = sSlotIdx;
            sSlotIdx ++;

            // update�� �߻������Ƿ�, Key�� 1�� skip�Ѵ�.
            i ++;
        }
        break;

        default:
            break;
    }

    // ù��° ��忡�� ���� Key��
    for ( ; i < sCurSlotCount ; i++ )
    {
        IDE_TEST( sdpSlotDirectory::getPagePtrFromSlotNum( sSlotDirPtr, 
                                                           i,
                                                           &sSlot )
                  != IDE_SUCCESS );

        sKeyArray[sSlotIdx].mLength = getKeyLength( &(aIndex->mColLenInfoList),
                                                    (UChar *)sSlot,
                                                    aIsLeaf );
        sKeyArray[sSlotIdx].mPage = SDNB_CURRENT_PAGE;
        sKeyArray[sSlotIdx].mSeq = i;
        sSlotIdx ++;
    }

    // ù��° ���� ���� ��� ������ branch Key(internal node���� �ش�)
    if ( aIsLeaf != ID_TRUE )
    {
        sKeyArray[sSlotIdx].mLength = aStack->mStack[aStack->mCurrentDepth-1].mNextSlotSize ;
        sKeyArray[sSlotIdx].mPage = SDNB_PARENT_PAGE;
        sKeyArray[sSlotIdx].mSeq = aStack->mStack[aStack->mCurrentDepth-1].mKeyMapSeq + 1;
        sSlotIdx ++;
    }
    else
    {
        /* nothing to do */
    }

    // �ι�° ����� Key
    for ( i = 0 ; i < sNextSlotCount ; i++ )
    {
        IDE_TEST( sdpSlotDirectory::getPagePtrFromSlotNum( sNextSlotDirPtr, 
                                                           i,
                                                           &sSlot )
                  != IDE_SUCCESS );
        sKeyArray[sSlotIdx].mLength = getKeyLength( &(aIndex->mColLenInfoList),
                                                    (UChar *)sSlot,
                                                    aIsLeaf );
        sKeyArray[sSlotIdx].mPage = SDNB_NEXT_PAGE;
        sKeyArray[sSlotIdx].mSeq = i;
        sSlotIdx ++;
    }

    *aKeyArray = sKeyArray;

    IDE_DASSERT((*aAllKeyCount) == sSlotIdx);

    // move slot �� �� slot entry�� �̵��ϹǷ� ����Ǿ�� �Ѵ�.
    for ( i = 0 ; i < sSlotIdx ; i ++ )
    {
        sKeyArray[i].mLength += ID_SIZEOF(sdpSlotEntry);
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( insufficient_memory );
    {
        IDE_SET(ideSetErrorCode(idERR_ABORT_InsufficientMemory));
    }
    IDE_EXCEPTION_END;

    if  (sKeyArray != NULL )
    {
        (void)iduMemMgr::free(sKeyArray);
        sKeyArray = NULL;
    }

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : sdnbBTree::findRedistributeKeyPoint        *
 * ------------------------------------------------------------------*
 * Key ��й谡 ����� point�� ã�´�. 2 node�� Key��� ���� ���Ե�  *
 * �� Key�� ���Ͽ� ���� �������, ������ �й������� ã�´�. Key�� �� *
 * �й�� ����, Property�� ���ǵ� Free ������ Ȯ���ϰ� �־�߸� ���� *
 * �� �����ϰ� �ǰ�, ������ ��쿡�� ���з� ����.                    *
 *********************************************************************/
IDE_RC sdnbBTree::findRedistributeKeyPoint(sdnbHeader    * aIndex,
                                           sdnbStack     * aStack,
                                           UShort        * aKeySize,
                                           SShort          aInsertSeq,
                                           UInt            aMode,
                                           sdpPhyPageHdr * aNode,
                                           sdpPhyPageHdr * aNextNode,
                                           idBool          aIsLeaf,
                                           SShort        * aPoint,
                                           idBool        * aCanRedistribute)
{
    sdnbKeyArray   * sKeyArray = NULL;
    UShort           sSlotCount;
    UChar          * sSlotDirPtr;
    UInt             sSrcPageAvailSize;
    UInt             sDstPageAvailSize;
    UInt             sTotalSize;
    UInt             sOccupied;
    SShort           sAllKeyCount;
    SShort           sMoveStartSeq = -1;
    SShort           i;

    /**************************************************
     * Pre Test
     **************************************************/
    if ( aIsLeaf == ID_TRUE )
    {
        if ( sdnIndexCTL::canAllocCTS( aNextNode,
                                       sdnIndexCTL::getUsedCount( aNode ) )
             == ID_FALSE )
        {
            *aCanRedistribute = ID_FALSE;
            IDE_CONT(exit_success);
        }
        else
        {
            /* nothing to do */
        }
    }
    else
    {
        /* nothing to do */
    }

    /**************************************************
     * Construction
     **************************************************/
    sSlotDirPtr = sdpPhyPage::getSlotDirStartPtr( (UChar *)aNode ) ;
    sSlotCount  = sdpSlotDirectory::getCount( sSlotDirPtr );

    sSrcPageAvailSize = sdnbBTree::calcPageFreeSize( aNode );
    sDstPageAvailSize = sdnbBTree::calcPageFreeSize( aNextNode );

    IDE_TEST(sdnbBTree::makeKeyArray(aIndex,
                                     aStack,
                                     aKeySize,
                                     aInsertSeq,
                                     aMode,
                                     aIsLeaf,
                                     aNode,
                                     aNextNode,
                                     &sAllKeyCount,
                                     &sKeyArray)
             != IDE_SUCCESS );

    // Total size ���
    sTotalSize = 0;
    for (i = 0; i < sAllKeyCount; i ++)
    {
        sTotalSize += sKeyArray[i].mLength;
    }

    // �� ��忡 �յ� �й踦 ���Ͽ� slot�� ����Ѵ�. ���� Ű����
    // size�� �����ذ��ٰ�, ��ü ���������� 1/2�� ���������� �����Ѵ�.
    sOccupied = 0;
    for ( i = 0 ; i < sAllKeyCount ; i ++ )
    {
        sOccupied += sKeyArray[i].mLength;
        if ( sOccupied >= (sTotalSize / 2) )
        {
            // ù��° ����� ���� ������ Key�� ������,
            // page ũ�⸦ �Ѱ� �Ǵ� ��쿡�� ������ 1���� ����.
            if ( sOccupied > sSrcPageAvailSize )
            {
                sOccupied -= sKeyArray[i].mLength;
                i --;
            }
            else
            {
                /* nothing to do */
            }

            // ������� ���� i�� 1/2 ������ ����� ���̹Ƿ�, �� ��忡
            // �����ϰ� �־�� �ϴ� Key�� �ȴ�. ���� �̵��� �� ���� Key
            // ���� �̵��� �ؾ� �ϹǷ�, i + 1�� Move start�� ��´�.
            sMoveStartSeq = i + 1;

            break;
        }
        else
        {
            /* nothing to do */
        }
    }//for

    /**************************************************
     * Post Test
     **************************************************/
    if ( sMoveStartSeq == -1 )
    {
        *aCanRedistribute = ID_FALSE;
        IDE_CONT(exit_success);
    }
    else
    {
        /* nothing to do */
    }

    // Key redistribution ���Ŀ� �� ����� Free ������
    // DISK_INDEX_SPLIT_LOW_LIMIT ���� ���� ��쿡��
    // Key redistribution�� ���� �ʰ� 1-to-2 split�� �Ѵ�
    if ( ( 100 * (SInt)(sSrcPageAvailSize - sOccupied) / 
           (SInt)sSrcPageAvailSize )
          <= (SInt)smuProperty::getKeyRedistributionLowLimit())
    {
        *aCanRedistribute = ID_FALSE;
        IDE_CONT(exit_success);
    }
    else
    {
        /* nothing to do */
    }

    // 2���� node�� Key�� ���� ������ �־, 2�� node�� key�� ����
    // ���Ե� Key�� ������ ��, 2�� ������ ������ �Ѿ�԰� �Ǵ� ��찡
    // �߻��� �� �ִ�. �̶���
    // 2��° node�� free ������ ����ϴ� (sPageAvailSize - (sTotalSize -
    // sOccupied)) ����� ������ ���� �� �ִ�.
    // �̴� -(minus) % �� ����� �Ǿ�� �ϴµ�, �� �������� ��� UInt ��
    // �����̹Ƿ� �ſ� ū ��������� ��������.
    // ����, �� ����� �����ϱ� ���ؼ��� Sint ������ type casting�Ͽ�
    // �񱳸� �ؾ߸� �Ѵ�.
    if ( ( 100 * (SInt)(sDstPageAvailSize - (sTotalSize - sOccupied) ) / 
           (SInt)sDstPageAvailSize )
         <= (SInt)smuProperty::getKeyRedistributionLowLimit() )
    {
        *aCanRedistribute = ID_FALSE;
        IDE_CONT(exit_success);
    }
    else
    {
        /* nothing to do */
    }

    if ( sKeyArray[sMoveStartSeq].mPage != SDNB_CURRENT_PAGE )
    {
        *aCanRedistribute = ID_FALSE;
        IDE_CONT(exit_success);
    }
    else
    {
        /* nothing to do */
    }

    // MoveStartSeq�� ������ Key�� �Ǿ�� �� �ȴ�. Internal node������
    // MoveStartSeq Key�� ������ �ø��� MoveStartSeq + 1���� move�ϱ�
    // ������, �ּ��� MoveStartSeq ���ķ� 2���� Key�� �־�� �Ѵ�.
    if ( sKeyArray[sMoveStartSeq].mSeq >= (sSlotCount - 1) )
    {
        *aCanRedistribute = ID_FALSE;
        IDE_CONT(exit_success);
    }
    else
    {
        /* nothing to do */
    }

    *aPoint = sKeyArray[sMoveStartSeq].mSeq;
    *aCanRedistribute = ID_TRUE;

    IDE_EXCEPTION_CONT(exit_success);

    if ( sKeyArray != NULL )
    {
        (void)iduMemMgr::free(sKeyArray);
        sKeyArray = NULL;
    }
    else
    {
        /* nothing to do */
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if (sKeyArray != NULL)
    {
        (void)iduMemMgr::free(sKeyArray);
        sKeyArray = NULL;
    }

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : sdnbBTree::calcPageFreeSize                *
 * ------------------------------------------------------------------*
 * �� ������ ���� �ִ�� ������ �� �ִ� ������ ũ�⸦ �˾Ƴ���.      *
 * EMPTY PAGE SIZE - LOGICAL LAYER SIZE - CTL SIZE                   *
 *********************************************************************/
UInt sdnbBTree::calcPageFreeSize( sdpPhyPageHdr * aPageHdr )
{
    return( sdpPhyPage::getEmptyPageFreeSize()
            - idlOS::align8(ID_SIZEOF(sdnbNodeHdr))
            - sdpPhyPage::getSizeOfCTL( aPageHdr )
            - sdpSlotDirectory::getSize(aPageHdr) );
}

/* *******************************************************************
 * PROJ-1872 Disk index ���� ���� ����ȭ
 *  getColumnLength
 *
 *  Description :
 *   Column�� �м��Ͽ�, ColumnValueLength�� �����ϰ�, �߰��� Column
 *  HeaderLength���� ��ȯ�Ѵ�.
 *
 *  aTargetColLenInfo - [IN]     Column�� ���� ColLenInfo
 *  aColumnPtr        - [IN]     Column�� ����� ������
 *  aColumnHeaderLen  - [OUT]    ColumnHeader�� ����
 *  aColumnValueLen   - [OUT]    ColumnValue�� ����
 *  aColumnValuePtr   - [OUT]    ColumnValue�� ��ġ (Null ���� )
 *
 *  return        -          ���� Column�� ����
 *
 * *******************************************************************/
UShort sdnbBTree::getColumnLength( UChar         aTargetColLenInfo,
                                   UChar       * aColumnPtr,
                                   UInt        * aColumnHeaderLen,
                                   UInt        * aColumnValueLen,
                                   const void  **aColumnValuePtr )
{
    UChar    sPrefix;
    UShort   sLargeLen;

    IDE_DASSERT( aColumnPtr       != NULL );
    IDE_DASSERT( aColumnHeaderLen != NULL );
    IDE_DASSERT( aColumnValueLen  != NULL );

    if ( aTargetColLenInfo == SDNB_COLLENINFO_LENGTH_UNKNOWN )
    {
        ID_READ_1B_AND_MOVE_PTR( aColumnPtr, &sPrefix );
        switch ( sPrefix )
        {
        case SDNB_NULL_COLUMN_PREFIX:
            *aColumnValueLen = 0;
            *aColumnHeaderLen = SDNB_SMALL_COLUMN_HEADER_LENGTH;
            break;

        case SDNB_LARGE_COLUMN_PREFIX:
            ID_READ_2B_AND_MOVE_PTR( aColumnPtr, &sLargeLen );
            *aColumnValueLen  = (UInt)sLargeLen;
            *aColumnHeaderLen = SDNB_LARGE_COLUMN_HEADER_LENGTH;
            IDE_DASSERT( SDNB_SMALL_COLUMN_VALUE_LENGTH_THRESHOLD < sLargeLen );
            break;

        default:
            *aColumnValueLen =  (UInt)sPrefix;
            *aColumnHeaderLen = SDNB_SMALL_COLUMN_HEADER_LENGTH;
            IDE_DASSERT( sPrefix <= SDNB_SMALL_COLUMN_VALUE_LENGTH_THRESHOLD );
            break;
        }
        if ( aColumnValuePtr != NULL )
        {
            *aColumnValuePtr = aColumnPtr;
        }
    }
    else
    {
        IDE_DASSERT( aTargetColLenInfo < SDNB_MAX_LENGTH_KNOWN_COLUMN_SIZE );

        *aColumnHeaderLen = 0;
        *aColumnValueLen  = aTargetColLenInfo;
        if ( aColumnValuePtr != NULL )
        {
            *aColumnValuePtr = aColumnPtr;
        }
    }

    return (*aColumnHeaderLen) + (*aColumnValueLen);
}

/*********************************************************************
 * FUNCTION DESCRIPTION : sdnbBTree::getKeyLength                    *
 * ------------------------------------------------------------------*
 * Key slot�� ColLenInfo�� �̿��� key�� ũ�⸦ �˾Ƴ���.             *
 *********************************************************************/
UShort sdnbBTree::getKeyLength( sdnbColLenInfoList * aColLenInfoList,
                                UChar              * aKey,
                                idBool               aIsLeaf )
{
    UShort       sTotalKeySize ;

    IDE_DASSERT( aColLenInfoList != NULL );
    IDE_DASSERT( aKey            != NULL );

    if ( aIsLeaf == ID_TRUE )
    {
        sTotalKeySize = getKeyValueLength( aColLenInfoList,
                                           SDNB_LKEY_KEY_PTR( aKey ) );
        sTotalKeySize = SDNB_LKEY_LEN( sTotalKeySize,
                                        SDNB_GET_TB_TYPE( (sdnbLKey*)aKey ) );
    }
    else
    {
        sTotalKeySize = getKeyValueLength( aColLenInfoList,
                                           SDNB_IKEY_KEY_PTR( aKey ) );
        sTotalKeySize = SDNB_IKEY_LEN( sTotalKeySize );
    }

    if ( sTotalKeySize >= SDNB_MAX_KEY_BUFFER_SIZE )
    {
        ideLog::log( IDE_ERR_0,
                     "Leaf? %s\nTotal Key Size : %"ID_UINT32_FMT"\n",
                     aIsLeaf == ID_TRUE ? "Y" : "N",
                     sTotalKeySize );
        ideLog::log( IDE_ERR_0, "Column lengh info :\n" );
        ideLog::logMem( IDE_DUMP_0,
                        (UChar *)aColLenInfoList,
                        ID_SIZEOF(sdnbColLenInfoList) );
        ideLog::log( IDE_ERR_0, "Key :\n" );
        ideLog::logMem( IDE_DUMP_0, (UChar *)aKey, SDNB_MAX_KEY_BUFFER_SIZE );

        IDE_ASSERT( 0 );
    }

    return sTotalKeySize;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : sdnbBTree::getKeyValueLength               *
 * ------------------------------------------------------------------*
 * Key Value�� ColLenInfoList�� �̿��� key value�� ũ�⸦ �˾Ƴ���.  *
 *********************************************************************/
UShort sdnbBTree::getKeyValueLength ( sdnbColLenInfoList * aColLenInfoList,
                                      UChar              * aKeyValue)
{
    UInt         sColumnIdx;
    UInt         sColumnValueLength;
    UInt         sColumnHeaderLen;
    UShort       sTotalKeySize;

    IDE_DASSERT( aColLenInfoList != NULL );
    IDE_DASSERT( aKeyValue       != NULL );

    sTotalKeySize = 0;

    for ( sColumnIdx = 0 ; sColumnIdx < aColLenInfoList->mColumnCount ; sColumnIdx++ )
    {
        sTotalKeySize += getColumnLength(
                                 aColLenInfoList->mColLenInfo[ sColumnIdx ],
                                 aKeyValue + sTotalKeySize,
                                 &sColumnHeaderLen,
                                 &sColumnValueLength,
                                 NULL );
    }

    return sTotalKeySize;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : PROJ-1872 Disk index ���� ���� ����ȭ      *
 * ------------------------------------------------------------------*
 * �񱳸� ���� (�Ǵ� Nullüũ�� ����) smiValue���·� Key�� �ٲ۴�.   *
 *********************************************************************/
void sdnbBTree::makeSmiValueListFromKeyValue( sdnbColLenInfoList * aColLenInfoList,
                                              UChar              * aKey,
                                              smiValue           * aSmiValue )
{
    UInt sColumnHeaderLength;
    UInt i;

    IDE_DASSERT( aColLenInfoList  != NULL );
    IDE_DASSERT( aKey             != NULL );
    IDE_DASSERT( aSmiValue        != NULL );

    for ( i = 0 ;
          i < aColLenInfoList->mColumnCount ;
          i++ )
    {
        aKey += getColumnLength( aColLenInfoList->mColLenInfo[ i ],
                                 aKey,
                                 &sColumnHeaderLength,
                                 &(aSmiValue->length),
                                 &(aSmiValue->value) );
        aSmiValue ++;
    }
}
/***********************************************************************
 *
 * Description :
 *  row�� fetch�ϸ鼭 Index�� KeyValue�� �����Ѵ�.
 *
 *  aStatistics   - [IN]  �������
 *  aMtx          - [IN]  mini Ʈ�����
 *  aSP           - [IN]  save point
 *  aTrans        - [IN]  Ʈ����� ����
 *  aTableHeader  - [IN]  ���̺� �ش�
 *  aIndex        - [IN]  �ε��� ���
 *  aRow          - [IN]  row ptr
 *  aPageReadMode - [IN]  page read mode(SPR or MPR)
 *  aTableSpaceID - [IN]  tablespace id
 *  aFetchVersion - [IN]  (�ֽ� ���� fetch) or (valid version fetch) ����
 *  aTSSlotSID    - [IN]  argument for mvcc
 *  aSCN          - [IN]  argument for mvcc
 *  aInfiniteSCN  - [IN]  argument for mvcc
 *  aDestBuf             - [OUT] dest buffer
 *  aIsRowDeleted        - [OUT] row�� �̹� delete�Ǿ����� ����
 *  aIsPageLatchReleased - [OUT] fetch�߿� page latch�� Ǯ�ȴ��� ����
 *
 **********************************************************************/
IDE_RC sdnbBTree::makeKeyValueFromRow(
                                idvSQL                 * aStatistics,
                                sdrMtx                 * aMtx,
                                sdrSavePoint           * aSP,
                                void                   * aTrans,
                                void                   * aTableHeader,
                                const smnIndexHeader   * aIndex,
                                const UChar            * aRow,
                                sdbPageReadMode          aPageReadMode,
                                scSpaceID                aTableSpaceID,
                                smFetchVersion           aFetchVersion,
                                sdSID                    aTSSlotSID,
                                const smSCN            * aSCN,
                                const smSCN            * aInfiniteSCN,
                                UChar                  * aDestBuf,
                                idBool                 * aIsRowDeleted,
                                idBool                 * aIsPageLatchReleased )
{
    sdnbHeader          * sIndexHeader;
    sdcIndexInfo4Fetch    sIndexInfo4Fetch;
    ULong                 sValueBuffer[SDNB_MAX_KEY_BUFFER_SIZE/ID_SIZEOF(ULong)];

    IDE_DASSERT( aTrans           != NULL );
    IDE_DASSERT( aTableHeader     != NULL );
    IDE_DASSERT( aIndex           != NULL );
    IDE_DASSERT( aRow             != NULL );
    IDE_DASSERT( aDestBuf         != NULL );

    if ( aIndex->mType != SMI_BUILTIN_B_TREE_INDEXTYPE_ID )
    {
        dumpHeadersAndIteratorToSMTrc(
                                  (smnIndexHeader*)aIndex,
                                  ((sdnbHeader*)aIndex->mHeader),
                                  NULL );

        IDE_ASSERT( 0 );
    }
    else
    {
        /* nothing to do */
    }

    sIndexHeader = ((sdnbHeader*)aIndex->mHeader);

    /* sdcRow::fetch() �Լ��� �Ѱ��� row info �� callback �Լ� ���� */
    sIndexInfo4Fetch.mTableHeader            = aTableHeader;
    sIndexInfo4Fetch.mCallbackFunc4Index     = makeSmiValueListInFetch;
    sIndexInfo4Fetch.mBuffer                 = (UChar *)sValueBuffer;
    sIndexInfo4Fetch.mBufferCursor           = (UChar *)sValueBuffer;
    sIndexInfo4Fetch.mFetchSize              = SDN_FETCH_SIZE_UNLIMITED;

    IDE_TEST( sdcRow::fetch(
                          aStatistics,
                          aMtx,
                          aSP,
                          aTrans,
                          aTableSpaceID,
                          (UChar *)aRow,
                          ID_TRUE, /* aIsPersSlot */
                          aPageReadMode,
                          sIndexHeader->mFetchColumnListToMakeKey,
                          aFetchVersion,
                          aTSSlotSID,
                          aSCN,
                          aInfiniteSCN,
                          &sIndexInfo4Fetch,
                          NULL, /* aLobInfo4Fetch */
                          ((smcTableHeader*)aTableHeader)->mRowTemplate,
                          (UChar *)sValueBuffer,
                          aIsRowDeleted,
                          aIsPageLatchReleased )
              != IDE_SUCCESS );

    if ( *aIsRowDeleted == ID_FALSE )
    {
        (void)makeKeyValueFromSmiValueList( aIndex,
                                            sIndexInfo4Fetch.mValueList,
                                            aDestBuf );
    }
    else
    {
        /* nothing to do */
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Proj-1872 Disk index ���屸�� ����ȭ
 *
 *   Row�� �������� VRow������ fetchcolumnlist�� ���ȴ�. �� FetchCol-
 * umnList�� �ݵ�� �ε��� Į������ �κ� �������� �����ϰ� �־�� �Ѵ�.
 *   ������ VRow�� ���� CursorLevelVisibilityCheck�� �Ҷ�, �ε��� Ű����
 * ���ϱ� ������, �ε��� Į���� �ش��ϴ� ������ ��� ������ �־�߸�
 * �������� Compare�� �����ϱ� �����̴�.
 ***********************************************************************/
IDE_RC sdnbBTree::checkFetchColumnList( sdnbHeader         * aIndex,
                                        smiFetchColumnList * aFetchColumnList,
                                        idBool             * aFullIndexScan )
{
    sdnbColumn         * sColumn;
    smiFetchColumnList * sFetchColumnList;
    idBool               sFullIndexScan;
    UInt                 sExistSameColumn;

    IDE_DASSERT( aIndex           != NULL );
    IDE_DASSERT( aFetchColumnList != NULL );
    IDE_DASSERT( aFullIndexScan   != NULL );

    sFullIndexScan = ID_TRUE;

    /* IndexColumn �� FetchColumnList=> valid  */
    for ( sColumn = aIndex->mColumns ;
          sColumn != aIndex->mColumnFence ;
          sColumn++ )
    {
        sExistSameColumn = ID_FALSE;
        for ( sFetchColumnList = aFetchColumnList ;
              sFetchColumnList != NULL ;
              sFetchColumnList = sFetchColumnList->next )
        {
            if ( sColumn->mVRowColumn.id == sFetchColumnList->column->id )
            {
                sExistSameColumn = ID_TRUE;
                IDE_TEST( sFetchColumnList->column->offset != sColumn->mVRowColumn.offset );
                break;
            }
            else
            {
                /* nothing to do */
            }
        }//for

        IDE_TEST( sExistSameColumn == ID_FALSE );
    }//for

    /* BUG-32017 [sm-disk-index] Full index scan in DRDB 
     * FetchColumnList == IndexColumn => enable FullIndexScan */
    for ( sFetchColumnList  = aFetchColumnList ;
          sFetchColumnList != NULL ;
          sFetchColumnList  = sFetchColumnList->next )
    {
        sExistSameColumn = ID_FALSE;
        for ( sColumn  = aIndex->mColumns;
              sColumn != aIndex->mColumnFence;
              sColumn++ )
        {
            if ( sColumn->mVRowColumn.id == sFetchColumnList->column->id )
            {
                sExistSameColumn = ID_TRUE;
                break;
            }
            else
            {
                /* nothing to do */
            }
        }//for

        if ( sExistSameColumn == ID_FALSE )
        {
            sFullIndexScan = ID_FALSE;
            break;
        }
        else
        {
            /* nothing to do */
        }
    }//for
    *aFullIndexScan = sFullIndexScan;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 *
 * Description :
 *  row�κ��� VRow�� �����Ѵ�. ������ Fetch�������� ȣ��Ǹ�, RowFilter,
 *  CursorLevelVisibilityCheck, Qp�� �÷��ִ� �뵵�θ� ����Ѵ�.
 *
 *  aStatistics      - [IN]  �������
 *  aMtx             - [IN]  mini Ʈ�����
 *  aSP              - [IN]  save point
 *  aTrans           - [IN]  Ʈ����� ����
 *  aTableSpaceID    - [IN]  tablespace id
 *  aRow             - [IN]  row ptr
 *  aPageReadMode    - [IN]  page read mode(SPR or MPR)
 *  aFetchColumnList - [IN]  QP���� �䱸�ϴ� Fetch�� Column ���
 *  aFetchVersion    - [IN]  (�ֽ� ���� fetch) or (valid version fetch) ����
 *  aTssRID          - [IN]  argument for mvcc
 *  aSCN             - [IN]  argument for mvcc
 *  aInfiniteSCN     - [IN]  argument for mvcc        
 *  aDestBuf             - [OUT] dest buffer
 *  aIsRowDeleted        - [OUT] row�� �̹� delete�Ǿ����� ����
 *  aIsPageLatchReleased - [OUT] fetch�߿� page latch�� Ǯ�ȴ��� ����
 *
 **********************************************************************/
IDE_RC sdnbBTree::makeVRowFromRow( idvSQL                 * aStatistics,
                                   sdrMtx                 * aMtx,
                                   sdrSavePoint           * aSP,
                                   void                   * aTrans,
                                   void                   * aTable,
                                   scSpaceID                aTableSpaceID,
                                   const UChar            * aRow,
                                   sdbPageReadMode          aPageReadMode,
                                   smiFetchColumnList     * aFetchColumnList,
                                   smFetchVersion           aFetchVersion,
                                   sdSID                    aTSSlotSID,
                                   const smSCN            * aSCN,
                                   const smSCN            * aInfiniteSCN,
                                   UChar                  * aDestBuf,
                                   idBool                 * aIsRowDeleted,
                                   idBool                 * aIsPageLatchReleased )
{
    IDE_DASSERT( aTrans    != NULL );
    IDE_DASSERT( aRow      != NULL );
    IDE_DASSERT( aDestBuf  != NULL );

    IDE_TEST( sdcRow::fetch( aStatistics,
                             aMtx,
                             aSP,
                             aTrans,
                             aTableSpaceID,
                             (UChar *)aRow,
                             ID_TRUE, /* aIsPersSlot */
                             aPageReadMode,
                             aFetchColumnList,
                             aFetchVersion,
                             aTSSlotSID,
                             aSCN,
                             aInfiniteSCN,
                             NULL, /* aIndexInfo4Fetch */
                             NULL, /* aLobInfo4Fetch */
                             ((smcTableHeader*)aTable)->mRowTemplate,
                             aDestBuf,
                             aIsRowDeleted,
                             aIsPageLatchReleased)
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/***********************************************************************
 *
 * Description :
 *  smiValue list�� ������ Key (index���� ����ϴ� ��)�� �����.
 *  insert DML�ÿ� �� �Լ��� ȣ���Ѵ�.
 *
 *  aIndex       - [IN]  �ε��� ���
 *  aValueList   - [IN]  insert value list
 *  aDestBuf     - [OUT] dest ptr
 *
 **********************************************************************/
IDE_RC sdnbBTree::makeKeyValueFromSmiValueList( const smnIndexHeader * aIndex,
                                                const smiValue       * aValueList,
                                                UChar                * aDestBuf )
{
    smiValue     * sIndexValue;
    UShort         sColumnSeq;
    UShort         sColCount = 0;
    UShort         sLoop;
    UChar        * sBufferCursor;
    UChar          sPrefix;
    UChar          sSmallLen;
    UShort         sLargeLen;
    smiColumn    * sKeyColumn;
    sdnbColumn   * sIndexColumn;
    ULong          sTempNullColumnBuf[ SDNB_MAX_LENGTH_KNOWN_COLUMN_SIZE/ID_SIZEOF(ULong) ];

    IDE_DASSERT( aIndex           != NULL );
    IDE_DASSERT( aValueList       != NULL );
    IDE_DASSERT( aDestBuf         != NULL );

    sBufferCursor = aDestBuf;
    sColCount = aIndex->mColumnCount;

    // BUG-27611 CodeSonar::Uninitialized Variable (7)
    IDE_ASSERT_MSG( sColCount > 0, "Error Ouccurs in DRDB Index ID : %"ID_UINT32_FMT, aIndex->mId );

    for ( sLoop = 0 ; sLoop < sColCount ; sLoop++ )
    {
        sColumnSeq   = aIndex->mColumns[ sLoop ] & SMI_COLUMN_ID_MASK;
        sIndexValue  = (smiValue*)(aValueList + sColumnSeq);

        sIndexColumn = &((sdnbHeader*)aIndex->mHeader)->mColumns[ sLoop ];
        sKeyColumn   = &sIndexColumn->mKeyColumn;

        if ( sColumnSeq != ( sKeyColumn->id % SMI_COLUMN_ID_MAXIMUM ) )
        {
            ideLog::log( IDE_ERR_0,
                         "Loop count : %"ID_UINT32_FMT""
                         ", Column sequence : %"ID_UINT32_FMT""
                         ", Column id : %"ID_UINT32_FMT""
                         "\nColumn information :\n",
                         sLoop, sColumnSeq, sKeyColumn->id );
            ideLog::logMem( IDE_DUMP_0,
                            (UChar *)sKeyColumn,
                            ID_SIZEOF(smiColumn) );
            IDE_ASSERT_MSG( 0, "Error Ouccurs in DRDB Index ID : %"ID_UINT32_FMT, aIndex->mId );
        }

        if ( (sKeyColumn->flag & SMI_COLUMN_LENGTH_TYPE_MASK)  == SMI_COLUMN_LENGTH_TYPE_KNOWN )
        {
            if ( sIndexValue->length > SDNB_MAX_LENGTH_KNOWN_COLUMN_SIZE )
            {
                ideLog::log( IDE_ERR_0,
                             "Loop count : %"ID_UINT32_FMT""
                             ", Value length : %"ID_UINT32_FMT""
                             "\nValue :\n",
                             sLoop, sIndexValue->length );
                ideLog::logMem( IDE_DUMP_0, (UChar *)sIndexValue, ID_SIZEOF(smiValue) );
                ideLog::logMem( IDE_DUMP_0, (UChar *)sIndexValue->value, SDNB_MAX_LENGTH_KNOWN_COLUMN_SIZE );
                IDE_ASSERT_MSG( 0, "Error Ouccurs in DRDB Index ID : %"ID_UINT32_FMT, aIndex->mId );
            }

            if ( sIndexValue->length == 0 ) // Null
            {
                sIndexColumn->mNull( sKeyColumn, sTempNullColumnBuf );
                ID_WRITE_AND_MOVE_DEST( sBufferCursor, sTempNullColumnBuf, sKeyColumn->size );
            }
            else    //Not Null
            {
                ID_WRITE_AND_MOVE_DEST( sBufferCursor, sIndexValue->value, sIndexValue->length );
            }
        }
        else // Length-unknown
        {
            if ( sIndexValue->length == 0 ) //NULL
            {
                sPrefix = SDNB_NULL_COLUMN_PREFIX;
                ID_WRITE_1B_AND_MOVE_DEST( sBufferCursor, &sPrefix );
            }
            else
            {
                if ( sIndexValue->length > SDNB_SMALL_COLUMN_VALUE_LENGTH_THRESHOLD ) //Large column
                {
                    sPrefix = SDNB_LARGE_COLUMN_PREFIX;
                    ID_WRITE_1B_AND_MOVE_DEST( sBufferCursor, &sPrefix );

                    sLargeLen = sIndexValue->length;
                    ID_WRITE_2B_AND_MOVE_DEST( sBufferCursor, &sLargeLen  );
                }
                else //Small Column
                {
                    sSmallLen = sIndexValue->length;
                    ID_WRITE_1B_AND_MOVE_DEST( sBufferCursor, &sSmallLen );
                }
                ID_WRITE_AND_MOVE_DEST( sBufferCursor, sIndexValue->value, sIndexValue->length );
            }
        }
    }

    return IDE_SUCCESS;
}

/******************************************************************
 *
 * Description :
 *  sdcRow�� �Ѱ��� ������ �������� smiValue���¸� �ٽ� �����Ѵ�.
 *  �ε��� Ű�� �ٽ� ����� smiValue�� �������� Ű�� �����ϰ� �ȴ�.
 *
 *  aIndexColumn        - [IN]     �ε��� Į�� ����
 *  aCopyOffset         - [IN]     column�� ���� rowpiece�� ������ ����� �� �����Ƿ�,
 *                                 copy offset ������ ���ڷ� �ѱ��.
 *                                 aCopyOffset�� 0�̸� first colpiece�̰�
 *                                 aCopyOffset�� 0�� �ƴϸ� first colpiece�� �ƴϴ�.
 *  aColumnValue        - [IN]     ������ column value
 *  aKeyInfo            - [OUT]    �̾Ƴ� Row�� ���� ������ ������ ���̴�.
 *
 * ****************************************************************/

IDE_RC sdnbBTree::makeSmiValueListInFetch( const smiColumn * aIndexColumn,
                                           UInt              aCopyOffset,
                                           const smiValue  * aColumnValue,
                                           void            * aIndexInfo )
{
    smiValue           *sValue;
    UShort              sColumnSeq;
    sdcIndexInfo4Fetch *sIndexInfo;

    IDE_DASSERT( aIndexColumn     != NULL );
    IDE_DASSERT( aColumnValue     != NULL );
    IDE_DASSERT( aIndexInfo       != NULL );

    sIndexInfo   = (sdcIndexInfo4Fetch*)aIndexInfo;
    sColumnSeq   = aIndexColumn->id & SMI_COLUMN_ID_MASK;
    sValue       = &sIndexInfo->mValueList[ sColumnSeq ];

    /* Proj-1872 Disk Index ���屸�� ����ȭ
     * �� �Լ��� Ű ������ �Ҹ���, Ű ������ mFetchColumnListToMakeKey��
     * �̿��Ѵ�. mFetchColumnListToMakeKey�� column(aIndexColumn)�� VRow��
     * �����Ҷ��� �̿���� �ʱ� ������, �׻� Offset�� 0�̴�. */
    IDE_DASSERT( aIndexColumn->offset == 0 );
    if ( sIndexInfo->mFetchSize <= 0 ) //FetchSize�� Rtree���� ����
    {
        ideLog::log( IDE_ERR_0, "Index info:\n" );
        ideLog::logMem( IDE_DUMP_0, (UChar *)sIndexInfo, ID_SIZEOF(sdcIndexInfo4Fetch) );
        IDE_ASSERT( 0 );
    }
    else
    {
        /* nothing to do */
    }

    if ( aColumnValue->length >= sdnbBTree::getMaxKeySize() )
    {
        ideLog::log( IDE_ERR_0,
                     "Column legnth : %"ID_UINT32_FMT"\nColumn Value :\n",
                     aColumnValue->length );
        ideLog::logMem( IDE_DUMP_0, (UChar *)aColumnValue, ID_SIZEOF(smiValue) );
        ideLog::logMem( IDE_DUMP_0,
                        (UChar *)aColumnValue->value,
                        sdnbBTree::getMaxKeySize() );
        IDE_ASSERT( 0 );
    }
    else
    {
        /* nothing to do */
    }

    if ( aCopyOffset == 0 ) //first col-piece
    {
        sValue->length = aColumnValue->length;
        sValue->value  = sIndexInfo->mBufferCursor;
    }
    else                   //first col-piece�� �ƴ� ���
    {
        sValue->length += aColumnValue->length;
    }

    if ( 0 < aColumnValue->length ) //NULL�� ��� length�� 0
    {
        if ( ( (UInt)(sIndexInfo->mBufferCursor - sIndexInfo->mBuffer)
                        + aColumnValue->length ) > SDNB_MAX_KEY_BUFFER_SIZE )
        {
            ideLog::log( IDE_ERR_0,
                         "Buffer Cursor pointer : 0x%"ID_vULONG_FMT", Buffer pointer : 0x%"ID_vULONG_FMT"\n"
                         "Buffer Size : %"ID_UINT32_FMT""
                         ", Value Length : %"ID_UINT32_FMT"\n",
                         sIndexInfo->mBufferCursor, sIndexInfo->mBuffer,
                         (UInt)(sIndexInfo->mBufferCursor - sIndexInfo->mBuffer),
                         aColumnValue->length );
            IDE_ASSERT( 0 );
        }
        else
        {
            /* nothing to do */
        }

        ID_WRITE_AND_MOVE_DEST( sIndexInfo->mBufferCursor,
                                aColumnValue->value,
                                aColumnValue->length );
    }
    else
    {
        /* nothing to do */
    }

    return IDE_SUCCESS;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : sdnbBTree::findSplitPoint                  *
 * ------------------------------------------------------------------*
 * node�� split�� �Ͼ�� aPCTFree�� ���� ���� �� �ִ� ������     *
 * ��ġ�� ã�Ƴ���.(leaf, internal node ����)                        *
 * ���� �� Ű�� �������� split�� �Ǿ�� �� ��� -1�� �����Ѵ�.   *
 * aPCTFree�� right node �������� Ȯ���ϴ� Free ������ �ǹ��Ѵ�.     *
 * ����, aPCTFree�� left node�� ���� �������ε� �ؼ��ɼ� �ִ�.     *
 *********************************************************************/
IDE_RC sdnbBTree::findSplitPoint(sdnbHeader    * aIndex,
                                 sdpPhyPageHdr * aNode,
                                 UInt            aHeight,
                                 SShort          aNewSlotPos,
                                 UShort          aNewSlotSize,
                                 UShort          aPCTFree,
                                 UInt            aMode,
                                 idBool          aIsDeleteLeafKey,
                                 UShort        * aSplitPoint)
{
    UChar         *sSlotDirPtr;
    UChar *        sSlot;
    UInt           sPartialSize = 0;
    UInt           sPartialSum;
    UInt           sPageFreeSize;
    UShort         sSeq = 0;
    UShort         sSplitPoint;
    UShort         sSlotCount;
    UShort         sExtraSize = ID_SIZEOF(sdpSlotEntry);
    UInt           sPageAvailSize;
    idBool         sSplitPointIsNewKey = ID_FALSE;

    sSlotDirPtr = sdpPhyPage::getSlotDirStartPtr( (UChar *)aNode );
    sSlotCount  = sdpSlotDirectory::getCount( sSlotDirPtr );

    IDE_ASSERT_MSG( sSlotCount >= 1,
                    "Error Ouccurs in DRDB Index ID : %"ID_UINT32_FMT, aIndex->mIndexID );

    // Page�� �ִ� ���� Size�� ����Ѵ�.
    sPageAvailSize = sdnbBTree::calcPageFreeSize( aNode );

    sPartialSum = 0;

    while ( sSeq < sSlotCount )
    {
        if ( sSeq == aNewSlotPos )
        {
            sPartialSize = aNewSlotSize + sExtraSize ;

            sPartialSum += sPartialSize;

            if ( aMode == SDNB_SMO_MODE_KEY_REDIST )
            {
                sSeq ++;
            }
            else
            {
                /* nothing to do */
            }

            if ( sPartialSum > ((sPageAvailSize * aPCTFree) / 100) )
            {
                sSplitPointIsNewKey = ID_TRUE;
                break;
            }
            else
            {
                /* nothing to do */
            }
        }
        else
        {
            /* nothing to do */
        }

        IDE_TEST( sdpSlotDirectory::getPagePtrFromSlotNum( sSlotDirPtr, 
                                                           sSeq,
                                                           &sSlot)
                  != IDE_SUCCESS );

        sPartialSize = ( getKeyLength( &(aIndex->mColLenInfoList),
                                       (UChar *)sSlot,
                                       (aHeight == 0) ? ID_TRUE : ID_FALSE )
                         + sExtraSize );

        sPartialSum += sPartialSize;

        if ( sPartialSum > ( (sPageAvailSize * aPCTFree) / 100 ) )
        {
            break;
        }
        else
        {
            /* nothing to do */
        }
        sSeq++;
    }

    IDE_ERROR ( sPartialSize != 0 );

    // Key�� ���� Node�� Slot���� ���� �������� ���ԵǾ�� �ϴµ�,
    // ���� Node�� Key���� PCTFree ���� �ȿ� ���ԵǾ�, ���� while loop����
    // ���� ���ԵǴ� Key�� ������� ���Ͽ�����, ���� ���ԵǴ� Key��
    // Split point�� �Ǿ�� �Ѵ�.
    // �� ����� ������ ������ 2������ �����ϴ� ��찡 �ȴ�.
    // �̷��� ����� �߻���, ������ Node ���� Key���� �����ϴ� ������
    // (sPageAvailSize * aPCTFree / 100) ���� ���� ����� ���Եǰ� �ִ�
    // ���¿���, ���� ���ԵǴ� Key�� ��忡 ������ �ɼ� ���� ������
    // Key ���̸� ���� ��쿡 �߻��� �� �ִ�.
    if ( ( sSeq == sSlotCount ) && ( sSeq == aNewSlotPos ) )
    {
        sPageFreeSize = sPageAvailSize - sPartialSum;

        sSplitPoint   = SDNB_SPLIT_POINT_NEW_KEY;
    }
    else
    {
        // Split point�� ����Ҷ�, aPCTFree���� �����Ͽ� �� ��踦 �Ѱ� �ϴ� Key����
        // ���� Node�� �����, �� ������ Key���� split�� �����Ͽ� move�ؾ� �Ѵ�.
        // (natc/TC/Server/sm4/Design/5_Index/Project/PROJ-1628/Bugs/BUG-6 �׽�Ʈ ���̽� ����)
        // �׷��� ������, Next node�� �����Ǵ� ���� Key�� �ִٰ� FATAL�� �߻��ϴ� ��찡 �ִ�.
        // ����, aPCTFree ��踦 �Ѵ� Key�� ��ġ�� ���� ������,
        // �� ���� Key�� ��ġ�� split point�� ��ȯ�ؾ� �Ѵ�.
        // �׷���, aPCTFree ��踦 �Ѱ� �� Key�� ���� ����� Page size�� �Ѿ������ �Ѵٸ�,
        // �� ����, ���� ��忡 ������ �����Ͽ� FATAL�� �߻��� �� �ִ�.
        // ����, ������� Key size�� �ջ��Ͽ� Page size�� ���� ���� ������,
        // ���� Key�� �ѱ⵵�� �ϰ�, Page size�� �Ѿ�����ٸ� Key�� 1�� ������ ��⵵�� �Ѵ�.
        if ( sPartialSum <= sPageAvailSize )
        {
            // �� ��쿡�� Page size�� �ѱ��� �����Ƿ�, ���� Key�� ��ġ�� ��ȯ�Ѵ�.
            sPageFreeSize = sPageAvailSize - sPartialSum;

            if ( sSplitPointIsNewKey == ID_TRUE )
            {
                // ��踦 �Ѵ� Key�� ���� ���ԵǴ� Key�̹Ƿ�, ���� Key�� sSeq�� �ȴ�.
                sSplitPoint = sSeq;
            }
            else
            {
                if ( (sSeq + 1) == aNewSlotPos )
                {
                    // ��� ���� Key�� ���� ���ԵǴ� Key�̹Ƿ�
                    // SDNB_SPLIT_POINT_NEW_KEY(= ID_USHORT_MAX)��
                    // ��ȯ�Ѵ�.
                    sSplitPoint = SDNB_SPLIT_POINT_NEW_KEY;
                }
                else
                {
                    // ���� Key�� ��ȯ
                    sSplitPoint = sSeq + 1;
                }
            }
        }
        else
        {
            // �� ��쿣 Page size�� �ѱ�� ����̹Ƿ�, ���� ���� Key�� ��ȯ�Ѵ�.
            sPageFreeSize = sPageAvailSize - (sPartialSum - sPartialSize);

            if ( sSplitPointIsNewKey == ID_TRUE )
            {
                sSplitPoint = SDNB_SPLIT_POINT_NEW_KEY;
            }
            else
            {
                sSplitPoint = sSeq;
            }
        }
    }

    if ( ( sSplitPoint != SDNB_SPLIT_POINT_NEW_KEY ) && 
         ( sSplitPoint > (sSlotCount - 1) ) )
    {
        // Split point�� ���� Left node�� ������ Key �����̶��,
        // �����δ� move�� �ϳ��� �Ͼ�� �ʰ� �ȴ�. �̶���
        // sSplitPoint�� aInsertSeq�� ���� ���� ������ ������,
        // ���� ���ԵǴ� Key�� Next node�� ������ �Ǿ�� �Ѵ�.
        // �̶��� ���� ���ԵǴ� Key �������� Split�� �߻��ϴ� ó���� �Ѵ�.
        IDE_DASSERT(sSplitPoint == aNewSlotPos);

        sSplitPoint = SDNB_SPLIT_POINT_NEW_KEY;
    }
    else
    {
        /* nothing to do */
    }

    // BUG-34011 fix - The server is abnormally terminated
    //                 when a user insert some keys which is half of node sized key
    //                 in DESC order into DRDB B-Tree.
    //
    // ���� �Էµ� key�� ���� �۴ٸ� �������� �Է��� ���ɼ��� ũ�Ƿ�
    // node split�� ���̱� ���� ���ʺ��ҿ� ��� �Ѱ��� key ������ �����Ѵ�.
    // (�����ʺ��ҿ��� �׻� free ���������Ƿ� �������� �Է½ô� ������� �ʴ´�)
    if ( ( aNewSlotPos == 0 ) &&
         ( sPageFreeSize < sPartialSize ) &&
         ( aIsDeleteLeafKey == ID_FALSE ) ) /* BUG-48064: LEAF KEY �����ô� ������ PAGE�� TBK�� ������ ������ �ʿ��ϴ�. */
    {
        if ( ( sSplitPoint > 0 ) && ( sSplitPoint != SDNB_SPLIT_POINT_NEW_KEY ) )
        {
            sSplitPoint--;
        }
        else 
        {
            if ( sSplitPoint == 0 )
            {
                sSplitPoint = SDNB_SPLIT_POINT_NEW_KEY;
            }
            else
            {
                ; /* nothing to do */
            }
        }
    }
    else
    {
        ; /* nothing to do */
    }
    *aSplitPoint = sSplitPoint;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : sdnbBTree::isMyTransaction                 *
 * ------------------------------------------------------------------*
 * TBK Key�� �ڽ��� Ʈ������� ������ Ű���� �˻��Ѵ�.               *
 *********************************************************************/
idBool sdnbBTree::isMyTransaction( void   * aTrans,
                                   smSCN    aBeginSCN,
                                   sdSID    aTSSlotSID )
{
    smSCN sSCN;

    if ( aTSSlotSID == smLayerCallback::getTSSlotSID( aTrans ) )
    {
        sSCN = smLayerCallback::getFstDskViewSCN( aTrans );

        if ( SM_SCN_IS_EQ( &aBeginSCN, &sSCN ) )
        {
            return ID_TRUE;
        }
        else
        {
            /* nothing to do */
        }
    }
    else
    {
        /* nothing to do */
    }

    return ID_FALSE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : sdnbBTree::isAgableTBK                     *
 * ------------------------------------------------------------------*
 * TBK Ű�� ���ؼ� �־��� CommitSCN�� Agable���� �˻��Ѵ�.           *
 *********************************************************************/
idBool sdnbBTree::isAgableTBK( smSCN    aCommitSCN )
{
    smSCN  sSysMinDskViewSCN;
    idBool sIsSuccess = ID_TRUE;

    if ( SM_SCN_IS_INFINITE( aCommitSCN ) == ID_TRUE )
    {
        sIsSuccess = ID_FALSE;
    }
    else
    {
        SMX_GET_MIN_DISK_VIEW( &sSysMinDskViewSCN );

        /*
         * Restart Undo�ÿ� ȣ��Ǿ��ų�, Service ���¿��� MinDiskViewSCN
         * ���� ���� ���� Aging�� �����ϴ�.
         */
        if ( SM_SCN_IS_INIT( sSysMinDskViewSCN ) ||
             SM_SCN_IS_LT( &aCommitSCN, &sSysMinDskViewSCN ) )
        {
            sIsSuccess = ID_TRUE;
        }
        else
        {
            sIsSuccess = ID_FALSE;
        }
    }

    return sIsSuccess;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : sdnbBTree::getCommitSCNFromTBK             *
 * ------------------------------------------------------------------*
 * TBK ������ Ű���� Commit SCN�� ���� �Լ�                      *
 * �ش� Ʈ������� Commit�Ǿ��ٸ� Delayed Stamping�� �õ��� ����.    *
 * aTrans        - [IN]  �ڽ��� TX�� ������
 * aIsLimit      - [IN]  LimitSCN
 * aStmtViewSCN  - [IN]  �ùٸ� ������ row�� �б� ���� Statement viewscn
 * aCommitSCN    - [OUT] Ʈ������� CommitSCN�̳� Unbound CommitSCN Ȥ�� TSS��
                          ������ Ʈ������� BeginSCN�ϼ� �ִ�.
 *********************************************************************/
IDE_RC sdnbBTree::getCommitSCNFromTBK( idvSQL        * aStatistics,
                                       void          * aTrans,
                                       sdpPhyPageHdr * aPage,
                                       sdnbLKeyEx    * aLeafKeyEx,
                                       idBool          aIsLimit,
                                       smSCN           aStmtViewSCN,
                                       smSCN         * aCommitSCN )
{
    smSCN           sBeginSCN;
    smSCN           sCommitSCN;
    sdSID           sTSSlotSID;
    idBool          sTrySuccess = ID_FALSE;
    smTID           sDummyTID4Wait;

    if ( aIsLimit == ID_FALSE )
    {
        SDNB_GET_TBK_CSCN( aLeafKeyEx, &sBeginSCN );
        SDNB_GET_TBK_CTSS( aLeafKeyEx, &sTSSlotSID );
    }
    else
    {
        SDNB_GET_TBK_LSCN( aLeafKeyEx, &sBeginSCN );
        SDNB_GET_TBK_LTSS( aLeafKeyEx, &sTSSlotSID );
    }

    if ( SM_SCN_IS_VIEWSCN( sBeginSCN ) )
    {
        IDE_TEST( sdcTSSegment::getCommitSCN( aStatistics,
                                              aTrans,
                                              sTSSlotSID,
                                              &sBeginSCN,
                                              aStmtViewSCN,
                                              &sDummyTID4Wait,
                                              &sCommitSCN )
                  != IDE_SUCCESS );

        /*
         * �ش� Ʈ������� Commit�Ǿ��ٸ� Delayed Stamping�� �õ��� ����.
         */
        if ( SM_SCN_IS_INFINITE( sCommitSCN ) == ID_FALSE )
        {
            sdbBufferMgr::tryEscalateLatchPage( aStatistics,
                                                (UChar *)aPage,
                                                SDB_SINGLE_PAGE_READ,
                                                &sTrySuccess );

            if ( sTrySuccess == ID_TRUE )
            {
                sdbBufferMgr::setDirtyPageToBCB( aStatistics,
                                                 (UChar *)aPage );

                if ( aIsLimit == ID_FALSE )
                {
                    SDNB_SET_TBK_CSCN( aLeafKeyEx, &sCommitSCN );
                }
                else
                {
                    SDNB_SET_TBK_LSCN( aLeafKeyEx, &sCommitSCN );
                }
            }
            else
            {
                /* nothing to do */
            }
        }
        else
        {
            /* nothing to do */
        }
    }
    else
    {
        sCommitSCN = sBeginSCN;
    }

    *aCommitSCN = sCommitSCN;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : sdnbBTree::agingAllTBK                     *
 * ------------------------------------------------------------------*
 * �ش� ��忡 �ִ� ��� TBK Ű���� Aging �Ѵ�.                      *
 * �ش� �Լ��� Node Aging������ ����Ǳ� ������ Create�� Ʈ����ǿ�  *
 * ���ؼ� Agable ���θ� �˻����� �ʴ´�.                             *
 * - TBK Aging ������ key�� ���°� DEAD�� �����ʴ� key�� ������      *
 *   TBK Aging�� �ߴ��Ѵ�. (�����߰�)                                *
 *********************************************************************/
IDE_RC sdnbBTree::agingAllTBK( idvSQL        * aStatistics,
                               sdnbHeader    * aIndex,
                               sdrMtx        * aMtx,
                               sdpPhyPageHdr * aPage,
                               idBool        * aIsSuccess )
{
    UChar       * sSlotDirPtr;
    UShort        sSlotCount;
    sdnbLKey    * sLeafKey;
    idBool        sIsSuccess = ID_TRUE;
    UInt          i;
    sdnbNodeHdr * sNodeHdr;
    UShort        sDeadKeySize   = 0;
    UShort        sDeadTBKCount  = 0;
    UShort        sTotalTBKCount = 0;
    ULong         sTempBuf[SD_PAGE_SIZE / ID_SIZEOF(ULong)];
    UChar       * sKeyList       = (UChar *)sTempBuf;
    UShort        sKeyListSize   = 0;
    UChar         sCTSInKey      = SDN_CTS_IN_KEY; /* recovery�� �̰����� TBK STAMPING�� Ȯ���Ѵ�.(BUG-44973) */
    scOffset      sSlotOffset    = 0;
    idBool        sIsCSCN        = ID_FALSE;
    idBool        sIsLSCN        = ID_FALSE;
    sdnbTBKStamping sKeyEntry;

    sSlotDirPtr = sdpPhyPage::getSlotDirStartPtr( (UChar *)aPage );
    sSlotCount  = sdpSlotDirectory::getCount( sSlotDirPtr );

    for ( i = 0 ; i < sSlotCount ; i++ )
    {
        IDE_TEST( sdpSlotDirectory::getPagePtrFromSlotNum( sSlotDirPtr,
                                                           i,
                                                           (UChar **)&sLeafKey )
                  != IDE_SUCCESS );

        if ( SDNB_GET_STATE( sLeafKey ) == SDNB_KEY_DEAD )
        {
            continue;
        }
        else
        {
            /* nothing to do */
        }

        if ( SDNB_GET_TB_TYPE( sLeafKey ) == SDNB_KEY_TB_CTS )
        {
            continue;
        }
        else
        {
            /* nothing to do */
        }

        IDE_DASSERT( SDNB_GET_STATE( sLeafKey ) == SDNB_KEY_DELETED );

        /*
         * Create�� Ʈ������� Agable�˻�� ���� �ʴ´�.
         * Limit Ʈ������� Agable�ϴٰ� �ǴܵǸ� Create�� Agable�ϴٰ�
         * �Ǵ��Ҽ� �ִ�.
         */

        /* (BUG-44973)
           ������� �����ߴٸ�
           key�� DELETE �����̰� TBK �̴�. */

        IDE_TEST( checkTBKStamping( aStatistics,
                                    aPage,
                                    sLeafKey,
                                    &sIsCSCN,
                                    &sIsLSCN ) != IDE_SUCCESS );

        if ( sIsLSCN == ID_FALSE )
        {
            if ( SDNB_GET_LCTS_NO( sLeafKey ) == SDN_CTS_IN_KEY )
            {
                /* TBK�� Limit SCN�� �����Ǿ��ִµ�, stamping�Ҽ����ٸ�
                   key���¸� DEAD�� �ٲܼ� ���ٴ� �ǹ��̴�.
                   ���⼭ stamping�� �ߴ��Ѵ�. */
                sIsSuccess = ID_FALSE; // nowtodo: FATAL
                break;
            }
            else
            {
                /* TBT stamping�Ҷ� DEAD�� �ɼ��ִ�. 
                   PASS�Ѵ�. */
            }
        }
        else
        {
            /* STMAPING�ϴ� TBK ����Ʈ�� �����. (�α��) */
            IDE_TEST( sdpSlotDirectory::getValue( sSlotDirPtr, 
                                                  i,
                                                  &sSlotOffset )
                      != IDE_SUCCESS );

            SDNB_SET_TBK_STAMPING( &sKeyEntry,
                                   sSlotOffset,
                                   sIsCSCN,
                                   sIsLSCN );

            idlOS::memcpy( sKeyList,
                           &sKeyEntry,
                           ID_SIZEOF(sdnbTBKStamping) );

            sKeyList     += ID_SIZEOF(sdnbTBKStamping);
            sKeyListSize += ID_SIZEOF(sdnbTBKStamping);

            sDeadTBKCount++;

            sDeadKeySize += getKeyLength( &(aIndex->mColLenInfoList),
                                          (UChar *)sLeafKey,
                                          ID_TRUE );

            sDeadKeySize += ID_SIZEOF( sdpSlotEntry );
        }
    }

    sKeyList = (UChar *)sTempBuf; /* sKeyList�� ���� ���� offset���� �̵��Ѵ�. */

    if ( sKeyListSize > 0 )
    {
        IDE_TEST( sdrMiniTrans::writeLogRec( aMtx,
                                             (UChar *)aPage,
                                             NULL,
                                             ID_SIZEOF(UChar) + SDNB_COLLENINFO_LIST_SIZE( aIndex->mColLenInfoList )
                                                 + ID_SIZEOF(UShort) +  sKeyListSize,
                                             SDR_SDN_KEY_STAMPING )
                  != IDE_SUCCESS );

        IDE_TEST( sdrMiniTrans::write( aMtx,
                                       (void *)&sCTSInKey,
                                       ID_SIZEOF(UChar) )
                  != IDE_SUCCESS );

        IDE_TEST( sdrMiniTrans::write( aMtx,
                                       (void *)&(aIndex->mColLenInfoList),
                                       SDNB_COLLENINFO_LIST_SIZE( aIndex->mColLenInfoList ) )
                  != IDE_SUCCESS );

        IDE_TEST( sdrMiniTrans::write( aMtx,
                                       &sKeyListSize,
                                       ID_SIZEOF(UShort) )
                  != IDE_SUCCESS );

        IDE_TEST( sdrMiniTrans::write( aMtx,
                                       (void *)sKeyList,
                                       sKeyListSize )
                  != IDE_SUCCESS );
    }
    else
    {
        /* nothing to do */
    }

    /* �α׸� ��������� ���� TBK stamping�� �����Ѵ�. */
    for ( i = 0; i < sKeyListSize; i += ID_SIZEOF(sdnbTBKStamping) )
    {
        sKeyEntry = *(sdnbTBKStamping *)(sKeyList + i);

        sLeafKey = (sdnbLKey *)( ((UChar *)aPage) + sKeyEntry.mSlotOffset );

        /* ����Ʈ�� �ִ� TBK�� ��� STAMPING�ؾ��Ѵ�. */
        if ( SDNB_IS_TBK_STAMPING_WITH_CSCN( &sKeyEntry ) == ID_TRUE )
        {
            SDNB_SET_CCTS_NO( sLeafKey, SDN_CTS_INFINITE );

            if ( SDNB_GET_STATE( sLeafKey ) == SDNB_KEY_UNSTABLE )
            {
                SDNB_SET_STATE( sLeafKey, SDNB_KEY_STABLE );
            }
            else
            {
                /* nothing to do */
            }
        }
        else
        {
            /* nothing to do */
        }

        if ( SDNB_IS_TBK_STAMPING_WITH_LSCN( &sKeyEntry ) == ID_TRUE )
        {
            SDNB_SET_STATE( sLeafKey, SDNB_KEY_DEAD );
            SDNB_SET_CCTS_NO( sLeafKey, SDN_CTS_INFINITE );
            SDNB_SET_LCTS_NO( sLeafKey, SDN_CTS_INFINITE );
        }
        else
        {
            /* nothing to do */
        }
    }

    if ( sDeadKeySize > 0 )
    {
        sNodeHdr = (sdnbNodeHdr*)sdpPhyPage::getLogicalHdrStartPtr((UChar *)aPage);
        sDeadKeySize += sNodeHdr->mTotalDeadKeySize;

        IDE_TEST(sdrMiniTrans::writeNBytes(
                                     aMtx,
                                     (UChar *)&sNodeHdr->mTotalDeadKeySize,
                                     (void *)&sDeadKeySize,
                                     ID_SIZEOF(UShort) )
                 != IDE_SUCCESS );
    }
    else
    {
        /* nothing to do */
    }

    if ( sDeadTBKCount > 0 )
    {
        sNodeHdr = (sdnbNodeHdr*)sdpPhyPage::getLogicalHdrStartPtr((UChar *)aPage);
        sTotalTBKCount = sNodeHdr->mTBKCount - sDeadTBKCount;

        IDE_TEST(sdrMiniTrans::writeNBytes(
                                     aMtx,
                                     (UChar *)&sNodeHdr->mTBKCount,
                                     (void *)&sTotalTBKCount,
                                     ID_SIZEOF(UShort) )
                 != IDE_SUCCESS );
    }
    else
    {
        /* nothing to do */
    }

    *aIsSuccess = sIsSuccess;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : sdnbBTree::cursorLevelVisibility           *
 * ------------------------------------------------------------------*
 * Ŀ�� �������� Transaction�� Snapshot�� ���� Ű���� Ȯ���Ѵ�.      *
 * �ش� �Լ��� ȣ��Ǵ� ���� Ʈ����� ������ Row�� ���� ���;�   *
 * �Ѵ�.                                                             *
 * �Ʒ��� ���� ��찡 �̿� �ش�ȴ�.                                 *
 * 1. CreateSCN�� Shnapshot.SCN���� ���� Duplicate Key�� ������ ��� *
 * 2. �ڽ��� ����/������ Ű�� ������ ���                            *
 *********************************************************************/
IDE_RC sdnbBTree::cursorLevelVisibility( void          * aIndex,
                                         sdnbStatistic * aIndexStat,
                                         UChar         * aVRow,
                                         UChar         * aKey,
                                         idBool        * aIsVisible )
{
    sdnbHeader      * sIndex;
    sdnbLKey        * sLKey;
    sdnbKeyInfo       sVRowInfo;
    sdnbKeyInfo       sKeyInfo;
    SInt              sRet;

    sIndex = (sdnbHeader*)((smnIndexHeader*)aIndex)->mHeader;
    sLKey  = (sdnbLKey*)aKey;

    // KEY�� ROW�� ���� ���� ������ �˻��Ѵ�.
    // ���� �ٸ� ���� ���´ٸ� �ش� KEY�� ���� ������ Ű�� �ƴϴ�.
    // ����, �ش� Ű�� ��ȿ�� Ű�� �ƴϴ�.
    sKeyInfo.mKeyValue  = SDNB_LKEY_KEY_PTR( sLKey );
    sVRowInfo.mKeyValue = (UChar *)aVRow;
    sRet = compareKeyAndVRow( aIndexStat,
                              sIndex->mColumns,
                              sIndex->mColumnFence,
                              &sKeyInfo,
                              &sVRowInfo );

    if ( sRet == 0 )
    {
        *aIsVisible = ID_TRUE;
    }
    else
    {
        *aIsVisible = ID_FALSE;
    }

    return IDE_SUCCESS;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : sdnbBTree::transLevelVisibility            *
 * ------------------------------------------------------------------*
 * Transaction Level���� Transaction�� Snapshot�� ���� Ű����        *
 * Ȯ���Ѵ�. �ش� �Լ��� Key�� �ִ� Ʈ����� ���������� Visibility�� *
 * Ȯ���Ҽ� �ִ� ��������� �˻��Ѵ�. ���� �Ǵ��� ���� �ʴ´ٸ�      *
 * Cursor Level visibility�� �˻��ؾ� �Ѵ�.                          *
 *                                                                   *
 * �Ʒ��� ��찡 Visibility�� Ȯ���Ҽ� ���� ����̴�.                *
 *                                                                   *
 * 1. CreateSCN�� Shnapshot.SCN���� ���� Duplicate Key�� ������ ��� *
 * 2. �ڽ��� ����/������ Ű�� ������ ���                            *
 *                                                                   *
 * ���� ��츦 ������ ��� Ű�� �Ʒ��� ���� 4���� ���� �з��ȴ�.   *
 *                                                                   *
 * 1. LimitSCN < CreateSCN < StmtSCN                                 *
 *    : LimitSCN�� Upper Bound SCN�� ��찡 CreateSCN���� LimitSCN�� *
 *      �� ���� �� �ִ�. Upper Bound SCN�� "0"�̴�.                  *
 *      �ش� ���� "Visible = TRUE"�̴�.                            *
 *                                                                   *
 * 2. CreateSCN < StmtSCN < LimitSCN                                 *
 *    : ���� �������� �ʾұ� ������ "Visible = TRUE"�̴�.            *
 *                                                                   *
 * 3. CreateSCN < LimitSCN < StmtSCN                                *
 *    : �̹� ������ Ű�̱� ������ "Visible = FALSE"�̴�.             *
 *                                                                   *
 * 4. StmtSCN < CreateSCN < LimitSCN                                 *
 *    : Select�� ������ ��� ������ ������ �ʾҾ��� Ű�̱� ������    *
 *      "Visible = FALSE"�̴�.                                       *
 *********************************************************************/
IDE_RC sdnbBTree::transLevelVisibility( idvSQL         * aStatistics,
                                        void           * aTrans, /* smxTrans */
                                        sdnbHeader     * aIndex,
                                        UChar          * aLeafNode,
                                        UChar          * aLeafKey ,
                                        smSCN          * aStmtViewSCN,
                                        sdnbVisibility * aVisibility )
{
    sdnbLKey            * sLeafKey;
    sdnbKeyInfo           sKeyInfo;
    smSCN                 sCreateSCN;
    smSCN                 sLimitSCN;
    smSCN                 sBeginSCN;
    sdSID                 sTSSlotSID;

    sLeafKey = (sdnbLKey*)aLeafKey;
    SDNB_LKEY_TO_KEYINFO( sLeafKey, sKeyInfo );

    if ( SDNB_GET_STATE( sLeafKey ) == SDNB_KEY_DEAD )
    {
        *aVisibility = SDNB_VISIBILITY_NO;
        IDE_CONT( RETURN_SUCCESS );
    }
    else
    {
        /* nothing to do */
    }

    if ( SDNB_GET_STATE( sLeafKey ) == SDNB_KEY_STABLE )
    {
        *aVisibility = SDNB_VISIBILITY_YES;
        IDE_CONT( RETURN_SUCCESS );
    }
    else
    {
        /* nothing to do */
    }

    if ( SDNB_GET_CCTS_NO( sLeafKey ) == SDN_CTS_INFINITE )
    {
        SM_INIT_SCN( &sCreateSCN );
        if ( SDNB_GET_LCTS_NO( sLeafKey ) == SDN_CTS_INFINITE )
        {
            ideLog::logMem( IDE_DUMP_0, 
                            (UChar *)sLeafKey, 
                            ID_SIZEOF( sdnbLKey ),
                            "Key:\n" );
            dumpIndexNode( (sdpPhyPageHdr *)aLeafNode );
            IDE_ASSERT_MSG( 0, "Error Ouccurs in DRDB Index ID : %"ID_UINT32_FMT, aIndex->mIndexID );
        }
        else
        {
            /* nothing to do */
        }
    }
    else
    {
        /* BUG-48353 : Leaf Node�� ���� Unlatch �ϱ� ����
           CreateSCN�� ���Ҷ� Pending Wait ���� �ʵ��� �Ѵ�. */
        if ( SDNB_GET_CCTS_NO( sLeafKey ) == SDN_CTS_IN_KEY )
        {
            IDE_TEST( getCommitSCNFromTBK( aStatistics,
                                           NULL,
                                           (sdpPhyPageHdr *)aLeafNode,
                                           (sdnbLKeyEx *)sLeafKey,
                                           ID_FALSE, /* aIsLimitSCN */
                                           SM_SCN_INIT,
                                           &sCreateSCN )
                      != IDE_SUCCESS );
        }
        else
        {
            IDE_TEST( sdnIndexCTL::getCommitSCN( aStatistics,
                                                 NULL,
                                                 (sdpPhyPageHdr*)aLeafNode,
                                                 SDB_SINGLE_PAGE_READ,
                                                 SDNB_GET_CCTS_NO( sLeafKey ),
                                                 SM_SCN_INIT,
                                                 &sCreateSCN )
                      != IDE_SUCCESS );
        }
    }

    if ( SDNB_GET_LCTS_NO( sLeafKey ) == SDN_CTS_INFINITE )
    {
        SM_MAX_SCN( &sLimitSCN );
    }
    else
    {
        /* BUG-48353 : Leaf Node�� ���� Unlatch �ϱ� ����
           LimitSCN�� ���Ҷ� Pending Wait ���� �ʵ��� �Ѵ�. */
        if ( SDNB_GET_LCTS_NO( sLeafKey ) == SDN_CTS_IN_KEY )
        {
            IDE_TEST( getCommitSCNFromTBK( aStatistics,
                                           NULL,
                                           (sdpPhyPageHdr *)aLeafNode,
                                           (sdnbLKeyEx *)sLeafKey,
                                           ID_TRUE, /* aIsLimitSCN */
                                           SM_SCN_INIT,
                                           &sLimitSCN )
                      != IDE_SUCCESS );
        }
        else
        {
            IDE_TEST( sdnIndexCTL::getCommitSCN( aStatistics,
                                                 NULL,
                                                 (sdpPhyPageHdr*)aLeafNode,
                                                 SDB_SINGLE_PAGE_READ,
                                                 SDNB_GET_LCTS_NO( sLeafKey ),
                                                 SM_SCN_INIT,
                                                 &sLimitSCN )
                      != IDE_SUCCESS );
        }
    }

    if ( SM_SCN_IS_INFINITE(sCreateSCN) == ID_TRUE )
    {
        if ( SDNB_GET_CCTS_NO( sLeafKey ) == SDN_CTS_IN_KEY )
        {
            SDNB_GET_TBK_CSCN( ((sdnbLKeyEx*)sLeafKey), &sBeginSCN );
            SDNB_GET_TBK_CTSS( ((sdnbLKeyEx*)sLeafKey), &sTSSlotSID );

            if ( isMyTransaction( aTrans, sBeginSCN, sTSSlotSID ) )
            {
                *aVisibility = SDNB_VISIBILITY_UNKNOWN;
                IDE_CONT( RETURN_SUCCESS );
            }
            else
            {
                /* nothing to do */
            }
        }
        else
        {
            if ( sdnIndexCTL::isMyTransaction( aTrans,
                                               (sdpPhyPageHdr*)aLeafNode,
                                               SDNB_GET_CCTS_NO( sLeafKey ) )
                == ID_TRUE )
            {
                *aVisibility = SDNB_VISIBILITY_UNKNOWN;
                IDE_CONT( RETURN_SUCCESS );
            }
            else
            {
                /* nothing to do */
            }
        }
    }
    else
    {
        /******************************************************************
         * PROJ-1381 - FAC�� Ŀ���� key�� ���ؼ��� visibility�� unkown����
         * �����ؼ� table�� ��ȸ�� �����ν� visibility�� �����ؾ� �Ѵ�.
         *
         * FAC fetch cursor�� ���� �ִ� Trans ��ü�� FAC�� Ŀ���� �����
         * Ʈ������� �ƴ϶�, FAC Ŀ�� ���Ŀ� ���� begin�� Ʈ������̴�.
         * ���� index������ key�� insert�� ����� infinite SCN�� ���ܵ�����
         * �ʴ´�. ���� FAC fetch cursor�� FAC�� Ŀ���� key�� �߰��ϴ���,
         * �ڽ��� insert�� key���� ���θ� Ȯ���� �� ����.
         *
         * ������ FAC�� Ŀ���� key�� �߰��ϸ� infinite SCN�� �����ϴ�
         * table�κ��� �ڽ��� �� �� �ִ� key�� �´��� Ȯ���� ���ƾ� �Ѵ�.
         *****************************************************************/
        if ( SDC_CTS_SCN_IS_LEGACY(sCreateSCN) == ID_TRUE )
        {
            *aVisibility = SDNB_VISIBILITY_UNKNOWN;
            IDE_CONT( RETURN_SUCCESS );
        }
        else
        {
            /* nothing to do */
        }

        if ( SM_SCN_IS_INFINITE(sLimitSCN) == ID_TRUE )
        {
            if ( SDNB_GET_LCTS_NO( sLeafKey ) == SDN_CTS_IN_KEY )
            {
                SDNB_GET_TBK_LSCN( ((sdnbLKeyEx*)sLeafKey), &sBeginSCN );
                SDNB_GET_TBK_LTSS( ((sdnbLKeyEx*)sLeafKey), &sTSSlotSID );

                if ( isMyTransaction( aTrans, sBeginSCN, sTSSlotSID ) )
                {
                    *aVisibility = SDNB_VISIBILITY_UNKNOWN;
                    IDE_CONT( RETURN_SUCCESS );
                }
                else
                {
                    /* nothing to do */
                }
            }
            else
            {
                if ( sdnIndexCTL::isMyTransaction( aTrans,
                                                   (sdpPhyPageHdr*)aLeafNode,
                                                   SDNB_GET_LCTS_NO( sLeafKey ) )
                    == ID_TRUE )
                {
                    *aVisibility = SDNB_VISIBILITY_UNKNOWN;
                    IDE_CONT( RETURN_SUCCESS );
                }
                else
                {
                    /* nothing to do */
                }
            }

            if ( SM_SCN_IS_LT( &sCreateSCN, aStmtViewSCN ) )
            {
                /*
                 *      CreateSCN < StmtSCN
                 *
                 *             -----------
                 *             |
                 *  ----------------------------
                 *          Create   ^
                 *                  Stmt
                 */
                if ( ((smxTrans *)aTrans)->mIsGCTx == ID_TRUE )
                {
                    /* BUG-48353
                       GCTX(Global TX Level=3) �ΰ��
                       ���� getCommitSCNFromTBK()/getCommitSCN() �Լ����� Pending Wait�� ���� �ʾұ⶧����
                       LimitSCN�� ��Ȯ���� �ʴ�.
                       UNKNOWN���� �����ϰ�, fetch �������� visibility�� �ٽ� Ȯ���ϵ��� �Ѵ�. */
                    *aVisibility = SDNB_VISIBILITY_UNKNOWN;
                }
                else
                {
                    /* HDB �Ǵ� Global TX Level 1,2 �ΰ�� */
                    *aVisibility = SDNB_VISIBILITY_YES;
                }

                IDE_CONT( RETURN_SUCCESS );
            }
            else
            {
                /* nothing to do */
            }
        }
        else
        {
            /* PROJ-1381 - Fetch Across Commits */
            if ( SDC_CTS_SCN_IS_LEGACY(sLimitSCN) == ID_TRUE )
            {
                *aVisibility = SDNB_VISIBILITY_UNKNOWN;
                IDE_CONT( RETURN_SUCCESS );
            }
            else
            {
                /* nothing to do */
            }

            if ( SM_SCN_IS_LT( &sCreateSCN, aStmtViewSCN ) )
            {
                if ( SM_SCN_IS_LT( aStmtViewSCN, &sLimitSCN ) )
                {
                    /*
                     * CreateSCN < StmtSCN < LimitSCN
                     *
                     *          ----------------
                     *          |              |
                     *  ----------------------------
                     *        Create    ^     Limit
                     *                 Stmt
                     */
                    *aVisibility = SDNB_VISIBILITY_YES;
                    IDE_CONT( RETURN_SUCCESS );
                }
                else
                {
                    /*
                     *     LimitSCN < StmtSCN
                     *
                     *          ---------
                     *          |       |
                     *  ----------------------------
                     *        Create  Limit    ^
                     *                        Stmt
                     */
                    *aVisibility = SDNB_VISIBILITY_NO;
                    IDE_CONT( RETURN_SUCCESS );
                }
            }
            else
            {
                /* nothing to do */
            }
        }

    }

    /*
     *     StmtSCN < CreateSCN
     *
     *              ---------
     *              |       |
     *  ----------------------------
     *      ^     Create  Limit
     *     Stmt
     */
    if ( SDNB_GET_DUPLICATED( sLeafKey ) == SDNB_DUPKEY_YES )
    {
        /*
         * DupKey�� cursor level visibility�� �ؾ� �Ѵ�.
         * StmtSCN�� CreateSCN���� �۱� ������
         * �ش� row�� aging���� �ʾ����� �����Ѵ�
         */
        *aVisibility = SDNB_VISIBILITY_UNKNOWN;
    }
    else
    {
        if ( ((smxTrans *)aTrans)->mIsGCTx == ID_TRUE )
        {
            /* BUG-48353
               GCTX(Global TX Level=3) �ΰ��
               ���� getCommitSCNFromTBK()/getCommitSCN() �Լ����� Pending Wait�� ���� �ʾұ⶧����
               CommitSCN/LimitSCN�� ��Ȯ���� �ʴ�.
               UNKNOWN���� �����ϰ�, fetch �������� visibility�� �ٽ� Ȯ���ϵ��� �Ѵ�. */
            *aVisibility = SDNB_VISIBILITY_UNKNOWN;
        }
        else
        {
            /* HDB �Ǵ� Global TX Level 1,2 �ΰ�� */
            *aVisibility = SDNB_VISIBILITY_NO;
        }
    }

    IDE_EXCEPTION_CONT( RETURN_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : sdnbBTree::uniqueVisibility                *
 * ------------------------------------------------------------------*
 * Uniqueness �˻縦 ����  Visibility�� Ȯ���Ѵ�.                    *
 * Ŀ���� �ȵ� ���� Ʈ������� Ŀ�Եɶ����� ��ٷ��� �ϰ�, �׷���  *
 * ���� ��쿡�� �����Ǿ����� �˻��Ѵ�. ���� �����Ǿ��ٸ� Unique     *
 * Violation�� �ش���� �ʰ�, �׷��� ���� ����� Unique Violation  *
 * �� �ش�ȴ�.                                                      *
 *********************************************************************/
IDE_RC sdnbBTree::uniqueVisibility( idvSQL         * aStatistics,
                                    void           * aTrans,
                                    sdnbHeader     * aIndex,
                                    UChar          * aLeafNode,
                                    UChar          * aLeafKey ,
                                    smSCN          * aStmtViewSCN,
                                    sdnbVisibility * aVisibility )
{
    sdnbLKey            * sLeafKey;
    sdnbKeyInfo           sKeyInfo;
    smSCN                 sCreateSCN;
    smSCN                 sLimitSCN;

    sLeafKey = (sdnbLKey*)aLeafKey;
    SDNB_LKEY_TO_KEYINFO( sLeafKey, sKeyInfo );

    if ( SDNB_GET_STATE( sLeafKey ) == SDNB_KEY_DEAD )
    {
        *aVisibility = SDNB_VISIBILITY_NO;
        IDE_CONT( RETURN_SUCCESS );
    }
    else
    {
        /* nothing to do */
    }

    if ( SDNB_GET_STATE( sLeafKey ) == SDNB_KEY_STABLE )
    {
        *aVisibility = SDNB_VISIBILITY_YES;
        IDE_CONT( RETURN_SUCCESS );
    }
    else
    {
        /* nothing to do */
    }

    if ( SDNB_GET_CCTS_NO( sLeafKey ) == SDN_CTS_INFINITE )
    {
        SM_INIT_SCN( &sCreateSCN );

        if ( SDNB_GET_LCTS_NO( sLeafKey ) == SDN_CTS_INFINITE )
        {
            ideLog::log( IDE_ERR_0, "Key:\n" );
            ideLog::logMem( IDE_DUMP_0, (UChar *)sLeafKey, ID_SIZEOF( sdnbLKey ) );
            dumpIndexNode( (sdpPhyPageHdr *)aLeafNode );
            IDE_ASSERT_MSG( 0, "Error Ouccurs in DRDB Index ID : %"ID_UINT32_FMT, aIndex->mIndexID );
        }
        else
        {
            /* nothing to do */
        }
    }
    else
    {
        if ( SDNB_GET_CCTS_NO( sLeafKey ) == SDN_CTS_IN_KEY )
        {
            IDE_TEST( getCommitSCNFromTBK( aStatistics,
                                           aTrans,
                                           (sdpPhyPageHdr *)aLeafNode,
                                           (sdnbLKeyEx *)sLeafKey,
                                           ID_FALSE,    /* aIsLimitSCN */
                                           *aStmtViewSCN,
                                           &sCreateSCN )
                      != IDE_SUCCESS );
        }
        else
        {
            IDE_TEST( sdnIndexCTL::getCommitSCN( aStatistics,
                                                 aTrans,
                                                 (sdpPhyPageHdr*)aLeafNode,
                                                 SDB_SINGLE_PAGE_READ,
                                                 SDNB_GET_CCTS_NO( sLeafKey ),
                                                 *aStmtViewSCN,
                                                 &sCreateSCN )
                      != IDE_SUCCESS );
        }
    }

    if ( SDNB_GET_LCTS_NO( sLeafKey ) == SDN_CTS_INFINITE )
    {
        SM_MAX_SCN( &sLimitSCN );
    }
    else
    {
        if ( SDNB_GET_LCTS_NO( sLeafKey ) == SDN_CTS_IN_KEY )
        {
            IDE_TEST( getCommitSCNFromTBK( aStatistics,
                                           aTrans,
                                           (sdpPhyPageHdr *)aLeafNode,
                                           (sdnbLKeyEx *)sLeafKey,
                                           ID_TRUE,     /* aIsLimitSCN */
                                           *aStmtViewSCN,
                                           &sLimitSCN )
                      != IDE_SUCCESS );
        }
        else
        {
            IDE_TEST( sdnIndexCTL::getCommitSCN( aStatistics,
                                                 aTrans,
                                                 (sdpPhyPageHdr*)aLeafNode,
                                                 SDB_SINGLE_PAGE_READ,
                                                 SDNB_GET_LCTS_NO( sLeafKey ),
                                                 *aStmtViewSCN,
                                                 &sLimitSCN )
                      != IDE_SUCCESS );
        }
    }

    /* PROJ-1381 - Fetch Across Commits */
    if ( ( SM_SCN_IS_INFINITE( sCreateSCN ) == ID_TRUE ) ||
         ( SM_SCN_IS_INFINITE( sLimitSCN )  == ID_TRUE ) ||
         ( SM_SCN_IS_NOT_INFINITE(sCreateSCN) &&
                   SDC_CTS_SCN_IS_LEGACY(sCreateSCN) ) ||
         ( SM_SCN_IS_NOT_INFINITE(sLimitSCN) &&
                   SDC_CTS_SCN_IS_LEGACY(sLimitSCN) ))
    {
        /*
         * ���� Ŀ���� �ȵǾ� �ְų� FAC Ŀ�� �����̱� ������ ��ٷ� ���� �Ѵ�.
         */
        *aVisibility = SDNB_VISIBILITY_UNKNOWN;
        IDE_CONT( RETURN_SUCCESS );
    }
    else
    {
        if ( SDNB_GET_LCTS_NO( sLeafKey ) == SDN_CTS_INFINITE )
        {
            /*
             * Delete���� ���� ���´� Unique Violation
             */
            IDE_DASSERT( SDNB_GET_STATE( sLeafKey ) == SDNB_KEY_UNSTABLE );
            *aVisibility = SDNB_VISIBILITY_YES;
        }
        else
        {
            /*
             * Delete�� ���¿��� Commit�� ���
             *
             * BUG-30911 - 2 rows can be selected during executing index scan
             *             on unique index.
             *
             * BUG-31451 - [SM] checking the unique visibility on disk index,
             *             the statement has to be restarted if same key has
             *             been updated.
             *
             * ������ Ű�� LimitSCN�� Statement SCN ���Ķ��
             * visible ������ Statement�� ������ ���� ���ŵ� Key �̱� ������
             * RETRY�� �ʿ��� ���� �ִ�.
             * ���� unknown true�� �����Ͽ� update������ row���� Ȯ���غ�
             * �ʿ䰡 �ִ�.
             */
            if ( SM_SCN_IS_GT(&sLimitSCN, aStmtViewSCN) )
            {
                *aVisibility = SDNB_VISIBILITY_UNKNOWN;
            }
            else
            {
                *aVisibility = SDNB_VISIBILITY_NO;
            }
        }
    }

    IDE_EXCEPTION_CONT( RETURN_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/*********************************************************************
 * FUNCTION DESCRIPTION : sdnbBTree::checkUniqueness                 *
 * ------------------------------------------------------------------*
 * �� �Լ������� Ư�� Row�� insert �� ��� Uniqueness�� ���� �ʴ���  *
 * �˻縦 �Ѵ�.                                                      *
 *                                                                   *
 * ū ��(value)�� ���ö� ���� ��� ù��° ���Ժ��� ����������        *
 * �̵��ϸ鼭 ����ũ �˻縦 �����ϸ�, �ش� slot�� ����Ű�� Row��     *
 * commit�� �����̸� Uniqueness Violation�� �����Ѵ�.                *
 *                                                                   *
 * �� �Լ��� ȣ���ϴ� �Լ��� aRetFlag�� Ȯ���Ͽ� ������ ó����       *
 * �Ͽ��� �Ѵ�.                                                      *
 *                                                                   *
 * [ RetFlag ]                                                       *
 * SDC_UPTSTATE_UPDATABLE:                 ���� ó��                 *
 * SDC_UPTSTATE_UPDATE_BYOTHER:            retry �õ�                *
 * SDC_UPTSTATE_UNIQUE_VIOLATION:          ���� ó��                 *
 * SDC_UPTSTATE_REBUILD_ALREADY_MODIFIED:  ���� ó��                 *
 *********************************************************************/
IDE_RC sdnbBTree::checkUniqueness(idvSQL           * aStatistics,
                                  void             * aTable,
                                  void             * aIndex,
                                  sdnbStatistic    * aIndexStat,
                                  sdrMtx           * aMtx,
                                  sdnbHeader       * aIndexHeader,
                                  smSCN              aStmtViewSCN,
                                  smSCN              aInfiniteSCN,
                                  sdnbKeyInfo      * aKeyInfo,
                                  sdpPhyPageHdr    * aStartNode,
                                  SShort             aStartSlotSeq,
                                  sdcUpdateState   * aRetFlag,
                                  smTID            * aWaitTID )
{
    ULong                  sTempKeyValueBuf[SDNB_MAX_KEY_BUFFER_SIZE/ID_SIZEOF(ULong)];
    SShort                 sSlotSeq;
    UShort                 sSlotCount;
    UChar                * sSlotDirPtr;
    UChar                * sRow;
    sdnbLKey             * sLKey;
    scPageID               sNextPID;
    sdpPhyPageHdr        * sCurNode = aStartNode;
    idBool                 sIsSuccess;
    idBool                 sIsUnusedSlot = ID_FALSE;
    SInt                   sRet;
    UInt                   sIndexState = 0;
    sdnbVisibility         sVisibility = SDNB_VISIBILITY_UNKNOWN;
    sdnbKeyInfo            sKeyInfo;
    sdnbKeyInfo            sKeyInfoFromRow;
    sdSID                  sRowSID;
    idBool                 sIsSameValue        = ID_FALSE;
    idBool                 sIsRowDeleted       = ID_FALSE;
    idBool                 sIsDataPageReleased = ID_TRUE;
    sdSID                  sTSSlotSID;
    void                 * sTrans;
    sdrMtxStartInfo        sMtxStartInfo;
    sdnbConvertedKeyInfo   sConvertedKeyInfo;
    smiValue               sSmiValueList[SMI_MAX_IDX_COLUMNS];

    sConvertedKeyInfo.mSmiValueList = (smiValue*)sSmiValueList;
    SDNB_KEYINFO_TO_CONVERTED_KEYINFO( *aKeyInfo,
                                       sConvertedKeyInfo,
                                       &(aIndexHeader->mColLenInfoList) );

    sTrans     = sdrMiniTrans::getTrans( aMtx );
    sTSSlotSID = smLayerCallback::getTSSlotSID( sTrans );

    sdrMiniTrans::makeStartInfo( aMtx, &sMtxStartInfo );

    sSlotSeq    = aStartSlotSeq;
    sSlotDirPtr = sdpPhyPage::getSlotDirStartPtr( (UChar *)sCurNode );
    sSlotCount  = sdpSlotDirectory::getCount( sSlotDirPtr );

    while (1)
    {
        *aRetFlag = SDC_UPTSTATE_UPDATABLE;
        
        if ( sSlotSeq == sSlotCount )
        {
            /* BUG-38216 detect hang on index module in abnormal state */
            IDE_TEST( iduCheckSessionEvent( aStatistics ) != IDE_SUCCESS );
        
            sNextPID = sdpPhyPage::getNxtPIDOfDblList(sCurNode);

            if ( sNextPID == SD_NULL_PID )
            {
                break;
            }
            else
            {
                /* nothing to do */
            }

            /*
             * aStartNode�� Backward Scan�ÿ� Mini-transaction Stack���� ���ԵǾ� �ִ�
             * �������̱� ������ release�ϸ� �ȵȴ�.
             * �ش� �Լ����� ���� ���������� release�ؾ� �Ѵ�.
             */
            if ( sIndexState == 1 )
            {
                sIndexState = 0;
                IDE_TEST( sdbBufferMgr::releasePage( aStatistics,
                                                     (UChar *)sCurNode )
                          != IDE_SUCCESS );
            }
            else
            {
                /* nothing to do */
            }

            /*
             * BUG-24954 [SN] ��ũ BTREE ����ũ �˻�� �����ϰ� Mini-Transaction
             * Stack�� ����մϴ�.
             * ����ũ �˻�� Mini-Transaction�� ������� �ʴ´�.
             * �̹� 3���� Leaf�� X-LATCH�� ȹ��Ǿ� �ִ� �����ϼ� �ֱ� ������
             * Mini-Transaction�� X-LATCH�� ȹ��Ǿ� �ִ� ���¶�� Mini-Transaction�� �ִ�
             * �������� ����ؾ� �ϰ�, �׷��� ���� ����� S-LATCH�� ����ؼ� Fetch �Ѵ�.
             */
            sCurNode = (sdpPhyPageHdr*)sdrMiniTrans::getPagePtrFromPageID( aMtx,
                                                                           aIndexHeader->mIndexTSID,
                                                                           sNextPID );
            if ( sCurNode == NULL )
            {
                IDE_TEST(sdnbBTree::getPage(aStatistics,
                                            &(aIndexStat->mIndexPage),
                                            aIndexHeader->mIndexTSID,
                                            sNextPID,
                                            SDB_S_LATCH,
                                            SDB_WAIT_NORMAL,
                                            NULL,
                                            (UChar **)&sCurNode,
                                            &sIsSuccess ) != IDE_SUCCESS );
                sIndexState = 1;
            }
            
            sSlotDirPtr = sdpPhyPage::getSlotDirStartPtr( (UChar *)sCurNode );
            sSlotCount  = sdpSlotDirectory::getCount( sSlotDirPtr );
            sSlotSeq    = 0;
        }

        IDE_TEST( sdpSlotDirectory::getPagePtrFromSlotNum( sSlotDirPtr,
                                                           sSlotSeq,
                                                           (UChar **)&sLKey )
                  != IDE_SUCCESS );
        SDNB_LKEY_TO_KEYINFO( sLKey, sKeyInfo );

        sIsSameValue = ID_FALSE;
        sRet = compareConvertedKeyAndKey( aIndexStat,
                                          aIndexHeader->mColumns,
                                          aIndexHeader->mColumnFence,
                                          &sConvertedKeyInfo, /* Unique üũ�ϴ� Insert KEY */
                                          &sKeyInfo,          /* Leaf Node�� KEY */
                                          ( SDNB_COMPARE_VALUE   |
                                            SDNB_COMPARE_PID     |
                                            SDNB_COMPARE_OFFSET ),
                                          &sIsSameValue );

        // Row�� value���� Slot�� value�� �ٸ��ٸ�
        if ( sIsSameValue == ID_FALSE )
        {
            // Row�� value���� Slot�� value���� Ŀ���� �ȵȴ�.
            if ( sRet >= 0 )
            {
                sSlotSeq++;
                continue;
            }
            else
            {
                /* nothing to do */
            }
            break;
        }

        sVisibility = SDNB_VISIBILITY_NO;

        IDE_TEST( uniqueVisibility( aStatistics,
                                    sTrans, 
                                    aIndexHeader,
                                    (UChar *)sCurNode,
                                    (UChar *)sLKey,
                                    &aStmtViewSCN,
                                    &sVisibility )
                  != IDE_SUCCESS );

        if ( sVisibility == SDNB_VISIBILITY_YES )
        {
            // caller must check this flag: uniqueness violation
            (*aRetFlag) = SDC_UPTSTATE_UNIQUE_VIOLATION;
            break;
        }
        else if ( sVisibility == SDNB_VISIBILITY_NO )
        {
            sSlotSeq++;
            continue;
        }

        // sCurNode, sSlotSeq�� ����Ű�� row�� ã�´�.
        sRowSID = SD_MAKE_SID( sKeyInfo.mRowPID, sKeyInfo.mRowSlotNum );

        /* FIT/ART/sm/Bugs/BUG-23648/BUG-23648.tc */
        IDU_FIT_POINT( "1.BUG-23648@sdnbBTree::checkUniqueness" );

        IDE_TEST( sdbBufferMgr::getPageBySID( aStatistics,
                                              aIndexHeader->mTableTSID,
                                              sRowSID,
                                              SDB_S_LATCH,
                                              SDB_WAIT_NORMAL,
                                              NULL,
                                              (UChar **)&sRow,
                                              &sIsSuccess,
                                              &sIsUnusedSlot )
                  != IDE_SUCCESS );
        sIsDataPageReleased = ID_FALSE;

        /* BUG-44802 : INDEX KEY�� ����Ű�� DATA slot�� ���� AGING �Ǿ� freeSlot �ɼ��ִ�. �̷���� SKIP�Ѵ�.
           BUG-46068 : AGING�� DATA slot�� ����ǰ�, ù��° record piece�� �ƴϸ� SKIP�Ѵ�.
                       (ù��° record piece�� ���� �����ڵ忡�� Ȯ���� SKIP�Ѵ�.) */
        if ( ( sIsUnusedSlot == ID_TRUE ) ||
             ( sdcRow::isHeadRowPiece( sRow ) == ID_FALSE ) )
        {
            if( sIsDataPageReleased == ID_FALSE )
            {
                sIsDataPageReleased = ID_TRUE;
                /* BUG-45009 */
                IDE_TEST( sdbBufferMgr::releasePage( aStatistics,
                                                     (UChar*)sRow )
                          != IDE_SUCCESS );
            }

            sSlotSeq++;
            continue;
        }

        IDE_TEST( sdcRow::canUpdateRowPieceInternal(
                                     aStatistics,
                                     &sMtxStartInfo,
                                     (UChar *)sRow,
                                     sTSSlotSID,
                                     SDB_SINGLE_PAGE_READ,
                                     &aStmtViewSCN,
                                     &aInfiniteSCN,
                                     ID_FALSE, /* aIsUptLobByAPI */
                                     aRetFlag,
                                     aWaitTID ) != IDE_SUCCESS );

        if ( (*aRetFlag) == SDC_UPTSTATE_REBUILD_ALREADY_MODIFIED )
        {
            // caller must check this flag: already modified
            break;
        }

        aIndexStat->mKeyValidationCount++;

        if ( (*aRetFlag) == SDC_UPTSTATE_UPDATE_BYOTHER )
        {
            IDE_DASSERT( *aWaitTID != SM_NULL_TID );

            /* TASK-6548 duplicated unique key */
            IDE_TEST_RAISE( ( smxTransMgr::isReplTrans( sTrans ) == ID_TRUE ) &&
                            ( smxTransMgr::isSameReplID( sTrans, *aWaitTID ) == ID_TRUE ) &&
                            ( smxTransMgr::isTransForReplConflictResolution( *aWaitTID ) == ID_TRUE ),
                            ERR_UNIQUE_VIOLATION_REPL );

            // üũ�߿� commit ����  row�� �߰�
            // PROJ-1553 Replication self-deadlock
            // ���� sTrans�� aWaitTID�� transaction��
            // ��� replication tx�̰�, ���� ID�̸�
            // ������� �ʰ� �׳� pass�Ѵ�.
            if ( smLayerCallback::isWaitForTransCase( sTrans,
                                                      *aWaitTID ) == ID_TRUE )
            {
                // caller must check this flag: retry check uniqueness
                break;
            }
            else
            {
                *aRetFlag = SDC_UPTSTATE_INVISIBLE_MYUPTVERSION;
            }
        }
        else
        {
            /* Proj-1872 DiskIndex ���屸�� ����ȭ
             *  UniquenssCheck�� ����,Table�� Row��  Key���·� ���� ��
             * �װ��� Index�� Key�� ���Ͽ� �� �� �ִ� Ű���� �ƴ�����
             * Vsibility�� �˻��Ѵ�. */
            IDE_TEST( makeKeyValueFromRow(
                                      aStatistics,
                                      NULL, /* sdrMtx */
                                      NULL, /* sdrSavepoint */
                                      sdrMiniTrans::getTrans(aMtx),
                                      aTable,
                                      (const smnIndexHeader*)aIndex,
                                      sRow,
                                      SDB_SINGLE_PAGE_READ,
                                      aIndexHeader->mTableTSID,
                                      SMI_FETCH_VERSION_LAST,
                                      SD_NULL_RID, /* aTssRID */
                                      NULL,        /* aSCN */
                                      NULL,        /* aInfiniteSCN */
                                      (UChar *)sTempKeyValueBuf,
                                      &sIsRowDeleted,
                                      &sIsDataPageReleased )
                      != IDE_SUCCESS );

            if ( sIsRowDeleted == ID_TRUE ) //������������, ���� ���� Ű
            {
                sVisibility = SDNB_VISIBILITY_NO;
            }
            else
            {
                /* Row�κ��� ���� Ű��, �ڽ��� ���� Ű�� ���� ���� ������ �˻���
                 * ��. ���� �ٸ� ���� ���´ٸ� �ش� KEY�� ���� ������ Ű�� �ƴ�
                 * ��. ����, �ش� Ű�� ��ȿ�� Ű�� �ƴϴ�. */
                sKeyInfoFromRow.mKeyValue = (UChar *)sTempKeyValueBuf;

                if ( compareKeyAndKey( aIndexStat,
                                       aIndexHeader->mColumns,
                                       aIndexHeader->mColumnFence,
                                       &sKeyInfo,        //���� Ű
                                       &sKeyInfoFromRow, //Row�κ��� ���� Ű
                                       SDNB_COMPARE_VALUE,
                                       &sIsSameValue ) == 0 )
                {
                    sVisibility = SDNB_VISIBILITY_YES;
                }
                else
                {
                    sVisibility = SDNB_VISIBILITY_NO;
                }
            }

            if ( sVisibility == SDNB_VISIBILITY_YES )
            {
                // �ڽ��� ������ ���ڵ尡 �ƴ϶��
                if ( (aKeyInfo->mRowPID != sKeyInfo.mRowPID) ||
                     (aKeyInfo->mRowSlotNum != sKeyInfo.mRowSlotNum) )
                {
                    // caller must check this flag:  uniqueness violation
                    (*aRetFlag) = SDC_UPTSTATE_UNIQUE_VIOLATION;
                    break;
                }
            }
        }

        if ( sIsDataPageReleased == ID_FALSE )
        {
            sIsDataPageReleased = ID_TRUE;
            IDE_TEST( sdbBufferMgr::releasePage( aStatistics,
                                                 (UChar *)sRow )
                      != IDE_SUCCESS );
        }
        else
        {
            /* nothing to do */
        }

        // ���� slot���� �̵�
        sSlotSeq++;
    }

    if ( sIndexState == 1 )
    {
        sIndexState = 0;
        IDE_TEST( sdbBufferMgr::releasePage( aStatistics,
                                             (UChar *)sCurNode )
                  != IDE_SUCCESS );
    }
    else
    {
        /* nothing to do */
    }

    if ( sIsDataPageReleased == ID_FALSE )
    {
        sIsDataPageReleased = ID_TRUE;
        IDE_TEST( sdbBufferMgr::releasePage( aStatistics,
                                             (UChar *)sRow )
                  != IDE_SUCCESS );
    }
    else
    {
        /* nothing to do */
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_UNIQUE_VIOLATION_REPL )
    {
        IDE_SET( ideSetErrorCode( smERR_ABORT_smnUniqueViolationInReplTrans ) );
    }
    IDE_EXCEPTION_END;

    IDE_PUSH();

    if ( sIndexState == 1 )
    {
        (void)sdbBufferMgr::releasePage( aStatistics,
                                         (UChar *)sCurNode );
    }

    if ( sIsDataPageReleased == ID_FALSE )
    {
        (void)sdbBufferMgr::releasePage( aStatistics,
                                         (UChar *)sRow );
    }

    IDE_POP();

    return IDE_FAILURE;
}


/*********************************************************************
 * FUNCTION DESCRIPTION : sdnbBTree::insertKeyUnique                 *
 * ------------------------------------------------------------------*
 * Unique Index�� ���� insert�� �����Ѵ�. insertKey()�� ȣ���Ѵ�.    *
 * NULL key�� ���ؼ��� Unique check�� ���� �ʴ´�.                   *
 *********************************************************************/
IDE_RC sdnbBTree::insertKeyUnique( idvSQL          * aStatistics,
                                   void            * aTrans,
                                   void            * aTable,
                                   void            * aIndex,
                                   smSCN             aInfiniteSCN,
                                   SChar           * aKey,
                                   SChar           * aNull,
                                   idBool            aUniqueCheck,
                                   smSCN             aStmtViewSCN,
                                   void            * aRowSID,
                                   SChar          ** aExistUniqueRow,
                                   ULong             aInsertWaitTime,
                                   idBool            aForbiddenToRetry )
{

    sdnbHeader         * sHeader;

    sHeader  = (sdnbHeader*)((smnIndexHeader*)aIndex)->mHeader;
    IDE_DASSERT( sHeader != NULL );

    if ( ( aUniqueCheck == ID_TRUE ) && ( sHeader->mIsNotNull == ID_FALSE ) )
    {
        // NullKey�� Uniqueness check�� ���� ����
        if ( isNullKey( sHeader, (UChar *)aKey ) == ID_TRUE )
        {
            aUniqueCheck = ID_FALSE;
        }
        else
        {
            /* nothing to do */
        }
    }
    else
    {
        /* nothing to do */
    }

    IDE_TEST( insertKey( aStatistics,
                         aTrans,
                         aTable,
                         aIndex,
                         aInfiniteSCN,
                         aKey,
                         aNull,
                         aUniqueCheck,
                         aStmtViewSCN,
                         aRowSID,
                         aExistUniqueRow,
                         aInsertWaitTime,
                         aForbiddenToRetry )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

/*********************************************************************
 * FUNCTION DESCRIPTION : sdnbBTree::backwardScanNode                *
 * ------------------------------------------------------------------*
 * BUG-15553                                                         *
 * ����ũ �˻縦 ���� ���� ��(value)�� ���� �ּ� Ű�� ã��           *
 * ���������� ��带 ��ȸ�Ѵ�. �̴� ������ ����ũ �˻縦 ���� ù��° *
 * ��带 ã�� �����̴�. (�ݵ�� Pessimistic Mode���߸� ��)          *
 *********************************************************************/
IDE_RC sdnbBTree::backwardScanNode( idvSQL         * aStatistics,
                                    sdnbStatistic  * aIndexStat,
                                    sdrMtx         * aMtx,
                                    sdnbHeader     * aIndex,
                                    scPageID         aStartPID,
                                    sdnbKeyInfo    * aKeyInfo,
                                    sdpPhyPageHdr ** aDestNode )
{
    sdnbLKey           * sLKey;
    idBool               sIsSameValue;
    sdnbKeyInfo          sKeyInfo;
    scPageID             sCurPID;
    sdpPhyPageHdr      * sCurNode;
    idBool               sIsSuccess;
    sdrSavePoint         sSP;
    UShort               sSlotCount;
    UChar              * sSlotDirPtr;
    sdnbConvertedKeyInfo sConvertedKeyInfo;
    smiValue             sSmiValueList[SMI_MAX_IDX_COLUMNS];

    IDE_DASSERT( aStartPID != SD_NULL_PID );

    sCurPID = aStartPID;
    sConvertedKeyInfo.mSmiValueList = (smiValue*)sSmiValueList;
    SDNB_KEYINFO_TO_CONVERTED_KEYINFO( *aKeyInfo,
                                       sConvertedKeyInfo,
                                       &(aIndex->mColLenInfoList) );

    while ( 1 )
    {
        sdrMiniTrans::setSavePoint( aMtx, &sSP );

        IDE_TEST( sdnbBTree::getPage( aStatistics,
                                      &(aIndexStat->mIndexPage),
                                      aIndex->mIndexTSID,
                                      sCurPID,
                                      SDB_X_LATCH,
                                      SDB_WAIT_NORMAL,
                                      aMtx,
                                      (UChar **)&sCurNode,
                                      &sIsSuccess )
                  != IDE_SUCCESS );

        sSlotDirPtr = sdpPhyPage::getSlotDirStartPtr( (UChar *)sCurNode );
        sSlotCount  = sdpSlotDirectory::getCount( sSlotDirPtr );

        if ( sSlotCount > 0 )
        {
            IDE_TEST( sdpSlotDirectory::getPagePtrFromSlotNum( sSlotDirPtr, 
                                                               0,
                                                               (UChar **)&sLKey )
                      != IDE_SUCCESS );
            sKeyInfo.mKeyValue = SDNB_LKEY_KEY_PTR( sLKey );

            sIsSameValue = ID_FALSE;
            (void) compareConvertedKeyAndKey( aIndexStat,
                                              aIndex->mColumns,
                                              aIndex->mColumnFence,
                                              &sConvertedKeyInfo,
                                              &sKeyInfo,
                                              SDNB_COMPARE_VALUE,
                                              &sIsSameValue );

            if ( sIsSameValue == ID_FALSE )
            {
                break;
            }
            else
            {
                /* nothing to do */
            }
        }
        else
        {
            /* nothing to do */
        }

        sCurPID = sdpPhyPage::getPrvPIDOfDblList( sCurNode );

        if ( sCurPID == SD_NULL_PID )
        {
            break;
        }
        else
        {
            /* nothing to do */
        }

        sdrMiniTrans::releaseLatchToSP( aMtx, &sSP );
    }

    *aDestNode = sCurNode;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION                                              *
 * ------------------------------------------------------------------*
 * BUG-16702                                                         *
 * ����ũ �˻縦 ���� ���� ��(value)�� ���� �ּ� Ű�� ã��           *
 * ���������� ������ ��ȸ�Ѵ�. �̴� ������ ����ũ �˻縦 ���� ù��° *
 * ������  ã�� �����̴�.                                            *
 *********************************************************************/
IDE_RC sdnbBTree::backwardScanSlot( sdnbHeader     * aIndex,
                                    sdnbStatistic  * aIndexStat,
                                    sdnbKeyInfo    * aKeyInfo,
                                    sdpPhyPageHdr  * aLeafNode,
                                    SShort           aStartSlotSeq,
                                    SShort         * aDestSlotSeq )
{
    UChar               *sSlotDirPtr;
    SShort               sSlotSeq;
    sdnbLKey            *sLKey;
    sdnbKeyInfo          sKeyInfo;
    idBool               sIsSameValue;
    sdnbConvertedKeyInfo sConvertedKeyInfo;
    smiValue             sSmiValueList[SMI_MAX_IDX_COLUMNS];

    sSlotSeq = aStartSlotSeq;
    sConvertedKeyInfo.mSmiValueList = (smiValue*)sSmiValueList;
    SDNB_KEYINFO_TO_CONVERTED_KEYINFO( *aKeyInfo,
                                       sConvertedKeyInfo,
                                       &aIndex->mColLenInfoList );

    sSlotDirPtr = sdpPhyPage::getSlotDirStartPtr( (UChar *)aLeafNode );

    IDE_DASSERT( (aStartSlotSeq >= -1) &&
                 (aStartSlotSeq < sdpSlotDirectory::getCount( sSlotDirPtr )) );

    while ( 1 )
    {
        if ( sSlotSeq < 0 )
        {
            break;
        }
        else
        {
            /* nothing to do */
        }

        IDE_TEST( sdpSlotDirectory::getPagePtrFromSlotNum( sSlotDirPtr,
                                                           sSlotSeq,
                                                           (UChar **)&sLKey )
                  != IDE_SUCCESS );
        sKeyInfo.mKeyValue = SDNB_LKEY_KEY_PTR( sLKey );

        sIsSameValue = ID_FALSE;
        (void) compareConvertedKeyAndKey( aIndexStat,
                                          aIndex->mColumns,
                                          aIndex->mColumnFence,
                                          &sConvertedKeyInfo,
                                          &sKeyInfo,
                                          SDNB_COMPARE_VALUE,
                                          &sIsSameValue );

        if ( sIsSameValue == ID_FALSE )
        {
            break;
        }
        else
        {
            /* nothing to do */
        }

        // ���������� ���� �̵�
        sSlotSeq = sSlotSeq - 1;
    }

    // sSlotSeq�� �ٸ� Ű�� ���� �����⿡���� ù��° �����̸� ����ũ
    // �˻縦 �ؾ��� ������ ���� ���� ���� �ּ� �����̴�.
    *aDestSlotSeq = sSlotSeq + 1;
    
    return IDE_SUCCESS;
    
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/*********************************************************************
 * FUNCTION DESCRIPTION : sdnbBTree::findInsertPos4Unique            *
 * ------------------------------------------------------------------*
 * Unique Key �϶�, insert position ã�� �Լ�                        *
 *********************************************************************/
IDE_RC sdnbBTree::findInsertPos4Unique( idvSQL         * aStatistics,
                                        void           * aTrans,
                                        void           * aTable,
                                        void           * aIndex,
                                        sdnbStatistic  * aIndexStat,
                                        smSCN            aStmtViewSCN,
                                        smSCN            aInfiniteSCN,
                                        sdrMtx         * aMtx,
                                        idBool         * aMtxStart,
                                        sdnbStack      * aStack,
                                        sdnbHeader     * aIndexHeader,
                                        sdnbKeyInfo    * aKeyInfo,
                                        idBool         * aIsPessimistic,
                                        sdpPhyPageHdr ** aLeafNode,
                                        SShort         * aLeafKeySeq ,
                                        idBool         * aIsSameKey,
                                        idBool         * aIsRetry,
                                        idBool         * aIsRetraverse,
                                        ULong          * aIdxSmoNo,
                                        ULong            aInsertWaitTime,
                                        idBool           aForbiddenToRetry,
                                        SLong          * aTotalTraverseLength )
{
    sdpPhyPageHdr        * sLeafNode;
    sdpPhyPageHdr        * sStartNode;
    SShort                 sLeafKeySeq;
    SShort                 sStartSlotSeq;
    UChar                * sSlotDirPtr;
    sdcUpdateState         sRetFlag;
    smTID                  sWaitTID = SM_NULL_TID;
    idBool                 sIsSameKey = ID_FALSE;
    idBool                 sIsSuccess = ID_FALSE;
    sdrSavePoint           sLeafSP;
    sdpPhyPageHdr        * sPrevNode;
    sdpPhyPageHdr        * sNextNode;
    scPageID               sPrevPID = SC_NULL_PID;
    scPageID               sLeafPID = SC_NULL_PID;
    scPageID               sNextPID = SC_NULL_PID;
    scPageID               sStartPID = SC_NULL_PID;
    ULong                  sNodeSmoNo;
    ULong                  sIndexSmoNo;
    idBool                 sRetry;
    idvWeArgs              sWeArgs;
    sdnbConvertedKeyInfo   sConvertedKeyInfo;
    smiValue               sSmiValueList[SMI_MAX_IDX_COLUMNS];

    idlOS::memset( &sLeafSP, 0, ID_SIZEOF(sLeafSP) );

    //------------------------------------------
    // ������ key ���� ���� ù��° ��ġ�� ���Ѵ�.
    // ( multi versioning ����̹Ƿ�, unique key�� ������ ����
    //   ������ �� ���� )
    //------------------------------------------

    IDE_TEST( traverse( aStatistics,
                        aIndexHeader,
                        aMtx,
                        aKeyInfo,
                        aStack,
                        aIndexStat,
                        *aIsPessimistic,
                        &sLeafNode,
                        &sLeafKeySeq,
                        &sLeafSP,
                        &sIsSameKey,
                        aTotalTraverseLength )
              != IDE_SUCCESS );

    if ( sLeafNode == NULL )
    {
        // traverse �ϴ� ���̿� root node�� �����.
        *aMtxStart = ID_FALSE;
        IDE_TEST( sdrMiniTrans::commit( aMtx ) != IDE_SUCCESS );

        *aIsRetry = ID_TRUE;


        return IDE_SUCCESS;
    }
    else
    {
        /* nothing to do */
    }

    // BUG-15553
    // ������ Ž���� ȣ�� ���� Ȯ��
    if ( aStack->mIsSameValueInSiblingNodes == ID_TRUE )
    {
        if ( *aIsPessimistic == ID_FALSE )
        {
            // Pessimistic Mode�� ��ȯ
            *aIsRetraverse = ID_TRUE;
            aIndexStat->mPessimisticCount++;
            IDE_CONT( RETRY_PESSIMISTIC );
        }
        else
        {
            /* nothing to do */
        }

        // To fix BUG-17543
        // releaseLatchToSP���� �ٽ� 3���� ��忡 X-LATCH�� ȹ���ϱ� ����
        // ������ ������.
        sPrevPID = sdpPhyPage::getPrvPIDOfDblList(sLeafNode);
        sLeafPID = sdpPhyPage::getPageID((UChar *)sLeafNode);
        sNextPID = sdpPhyPage::getNxtPIDOfDblList(sLeafNode);

        sdrMiniTrans::releaseLatchToSP( aMtx, &sLeafSP );

        // BUG-27210: ���� Leaf Node �� Left Most ����̰�, ������ Ű��
        //            ������ Value�� ������ Sibling�� ������ ��� sPrevPID��
        //            SD_NULL_PID �� �� �ִ�.
        sStartPID = ( sPrevPID != SD_NULL_PID ) ? sPrevPID : sLeafPID;

        // BUG-15553
        // ������ Ž���� ���� ����ũ �˻縦 �ؾ��� ù��° ��带 ����.
        aIndexStat->mBackwardScanCount++;
        IDE_TEST( backwardScanNode( aStatistics,
                                    aIndexStat,
                                    aMtx,
                                    aIndexHeader,
                                    sStartPID,     // [IN] aStartPID
                                    aKeyInfo,      // [IN] aKeyINfo
                                    &sStartNode )  // [OUT] aDestNode
                  != IDE_SUCCESS );
        
        // backwardScanSlot�� ���� seed slot�� ����
        sSlotDirPtr   = sdpPhyPage::getSlotDirStartPtr( (UChar *)sStartNode );
        sStartSlotSeq = sdpSlotDirectory::getCount( sSlotDirPtr ) - 1;
    }
    else
    {
        sStartNode = sLeafNode;
        // backwardScanSlot�� ���� seed slot�� ����
        sStartSlotSeq = sLeafKeySeq - 1;
    }

    // BUG-16702
    IDE_TEST( backwardScanSlot( aIndexHeader,
                                aIndexStat,
                                aKeyInfo,
                                sStartNode,       /* [IN] */
                                sStartSlotSeq,    /* [IN] */
                                &sStartSlotSeq )  /* [OUT] */
              != IDE_SUCCESS )

    //------------------------------------------
    // ������ key ���� ���� ��ȿ�� key�� �ϳ����� �˻�
    //------------------------------------------
    IDE_TEST( checkUniqueness( aStatistics,
                               aTable,
                               aIndex,
                               aIndexStat,
                               aMtx,
                               aIndexHeader,
                               aStmtViewSCN,
                               aInfiniteSCN,
                               aKeyInfo,
                               sStartNode,
                               sStartSlotSeq,
                               &sRetFlag,
                               &sWaitTID ) != IDE_SUCCESS );

    /* aRetFlag�� ���� ó���� ���⼭ ���־�� �Ѵ�. */
    if ( sRetFlag == SDC_UPTSTATE_REBUILD_ALREADY_MODIFIED )
    {
        /*
         * BUG-32579 The MiniTransaction commit should not be used
         * in exception handling area.
         */
        *aMtxStart = ID_FALSE;
        IDE_TEST( sdrMiniTrans::commit( aMtx ) != IDE_SUCCESS );

        IDE_RAISE( ERR_ALREADY_MODIFIED );
    }
    else
    {
        /* nothing to do */
    }

    if ( sRetFlag == SDC_UPTSTATE_UNIQUE_VIOLATION )
    {
        /*
         * BUG-32579 The MiniTransaction commit should not be used
         * in exception handling area.
         */
        *aMtxStart = ID_FALSE;
        IDE_TEST( sdrMiniTrans::commit( aMtx ) != IDE_SUCCESS );
        
        IDE_RAISE( ERR_UNIQUENESS_VIOLATION );
    }
    else
    {
        /* nothing to do */
    }

    if ( sRetFlag == SDC_UPTSTATE_UPDATE_BYOTHER )
    {
        *aMtxStart = ID_FALSE;
        IDE_TEST( sdrMiniTrans::commit( aMtx ) != IDE_SUCCESS );


        /* BUG-38216 detect hang on index module in abnormal state */
        IDE_TEST( iduCheckSessionEvent( aStatistics ) != IDE_SUCCESS );

        IDE_TEST( smLayerCallback::waitForTrans( aTrans,
                                                 sWaitTID,
                                                 ((smcTableHeader *)aTable)->mSpaceID,
                                                 aInsertWaitTime )
                  != IDE_SUCCESS );

        *aIsRetry = ID_TRUE;

        return IDE_SUCCESS;
    }
    else
    {
        /* nothing to do */
    }
    
    IDE_EXCEPTION_CONT( RETRY_PESSIMISTIC );

    if ( *aIsRetraverse == ID_TRUE )
    {
        IDE_DASSERT( *aIsPessimistic == ID_FALSE );

        *aIsPessimistic = ID_TRUE;
        *aMtxStart = ID_FALSE;
        IDE_TEST( sdrMiniTrans::commit( aMtx ) != IDE_SUCCESS );

        IDE_TEST( sdrMiniTrans::begin( aStatistics,
                                       aMtx,
                                       aTrans,
                                       SDR_MTX_LOGGING,
                                       ID_TRUE,/*Undoable(PROJ-2162)*/
                                       gMtxDLogType )
                  != IDE_SUCCESS );
        *aMtxStart = ID_TRUE;

        IDV_WEARGS_SET( &sWeArgs,
                        IDV_WAIT_INDEX_LATCH_DRDB_BTREE_INDEX_SMO,
                        0,   // WaitParam1
                        0,   // WaitParam2
                        0 ); // WaitParam3

        // tree x larch�� ��´�.
        IDE_TEST( aIndexHeader->mLatch.lockWrite(
                                             aStatistics,
                                             &sWeArgs ) != IDE_SUCCESS );

        IDE_TEST( sdrMiniTrans::push( aMtx,
                                      (void *)&aIndexHeader->mLatch,
                                      SDR_MTX_LATCH_X )
                  != IDE_SUCCESS );

        if ( validatePath( aStatistics,
                           aIndexHeader->mIndexTSID,
                           aIndexStat,
                           aStack,
                           *aIdxSmoNo )
             != IDE_SUCCESS )
        {
            // init stack
            initStack( aStack );
        }
        else
        {
            aStack->mIndexDepth--; // leaf �ٷ� �� ����
        }

        // SmoNO�� ���� ����.
        getSmoNo( (void *)aIndexHeader, aIdxSmoNo );
        
        IDL_MEM_BARRIER;

        return IDE_SUCCESS;
    }

    if ( aStack->mIsSameValueInSiblingNodes == ID_TRUE )
    {
        // releaseLatchToSP(sLeafSP)�� ���� �������� �ٽ� 3���� ��忡
        // X-LATCH�� ȹ���ؾ� �Ѵ�.
        if ( *aIsPessimistic == ID_TRUE )
        {
            if ( sPrevPID != SD_NULL_PID )
            {
                sPrevNode = (sdpPhyPageHdr*)sdrMiniTrans::getPagePtrFromPageID(
                    aMtx, aIndexHeader->mIndexTSID, sPrevPID );

                if ( sPrevNode == NULL )
                {
                    IDE_TEST( sdnbBTree::getPage( aStatistics,
                                                  &(aIndexStat->mIndexPage),
                                                  aIndexHeader->mIndexTSID,
                                                  sPrevPID,
                                                  SDB_X_LATCH,
                                                  SDB_WAIT_NORMAL,
                                                  aMtx,
                                                  (UChar **)&sPrevNode,
                                                  &sIsSuccess )
                              != IDE_SUCCESS );
                }
                else
                {
                    /* nothing to do */
                }
            }
            else
            {
                /* nothing to do */
            }

            sLeafNode = (sdpPhyPageHdr*)sdrMiniTrans::getPagePtrFromPageID(
                                                               aMtx, 
                                                               aIndexHeader->mIndexTSID, 
                                                               sLeafPID );

            if ( sLeafNode == NULL )
            {
                IDE_TEST( sdnbBTree::getPage( aStatistics,
                                              &(aIndexStat->mIndexPage),
                                              aIndexHeader->mIndexTSID,
                                              sLeafPID,
                                              SDB_X_LATCH,
                                              SDB_WAIT_NORMAL,
                                              aMtx,
                                              (UChar **)&sLeafNode,
                                              &sIsSuccess )
                          != IDE_SUCCESS );
            }
            else
            {
                /* nothing to do */
            }

            if ( sNextPID != SD_NULL_PID )
            {
                IDE_TEST( sdnbBTree::getPage( aStatistics,
                                              &(aIndexStat->mIndexPage),
                                              aIndexHeader->mIndexTSID,
                                              sNextPID,
                                              SDB_X_LATCH,
                                              SDB_WAIT_NORMAL,
                                              aMtx,
                                              (UChar **)&sNextNode,
                                              &sIsSuccess )
                          != IDE_SUCCESS );
            }
            else
            {
                /* nothing to do */
            }

            // re-searching insertable position

            sConvertedKeyInfo.mSmiValueList = (smiValue*)sSmiValueList;
            SDNB_KEYINFO_TO_CONVERTED_KEYINFO( *aKeyInfo,
                                               sConvertedKeyInfo,
                                               &aIndexHeader->mColLenInfoList );

            getSmoNo( (void *)aIndexHeader, &sIndexSmoNo );
            
            IDE_ERROR( findLeafKey( sLeafNode,
                                    aIndexStat,
                                    aIndexHeader->mColumns,
                                    aIndexHeader->mColumnFence,
                                    &sConvertedKeyInfo,
                                    sIndexSmoNo,
                                    &sNodeSmoNo,  // dummy
                                    &sLeafKeySeq, // [OUT]
                                    &sRetry,      // dummy
                                    &sIsSameKey )
                       == IDE_SUCCESS );
        }
        else
        {
            // releaseLatchToSP�� ������� �ʾұ� ������
            // X-LATCH�� �ٽ� ȹ���� �ʿ䰡 ����.
        }
    }
    else
    {
        // backwardScan�� ������ ���� ������ �������� �۾��� ����.
    }

    *aLeafNode   = sLeafNode;
    *aLeafKeySeq = sLeafKeySeq;
    *aIsSameKey  = sIsSameKey;

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_ALREADY_MODIFIED );
    {
        if( aForbiddenToRetry == ID_TRUE )
        {
            IDE_DASSERT( ((smxTrans*)aTrans)->mIsGCTx == ID_TRUE );

            SChar sMsgBuf[SMI_MAX_ERR_MSG_LEN];

            idlOS::snprintf( sMsgBuf,
                             SMI_MAX_ERR_MSG_LEN,
                             "[UNIQUE INSERT VALIDATION] "
                             "SpaceID:%"ID_UINT32_FMT", "
                             "TableOID:%"ID_vULONG_FMT", "
                             "ViewSCN:%"ID_UINT64_FMT", "
                             "CSInfiniteSCN:%"ID_UINT64_FMT,
                             ((smcTableHeader *)aTable)->mSpaceID,
                             ((smcTableHeader *)aTable)->mSelfOID,
                             SM_SCN_TO_LONG( aStmtViewSCN ),
                             SM_SCN_TO_LONG( aInfiniteSCN ) );

            IDE_SET( ideSetErrorCode(smERR_ABORT_StatementTooOld, sMsgBuf) );

            IDE_ERRLOG( IDE_SD_19 );
        }
        else
        {
            IDE_SET( ideSetErrorCode( smERR_RETRY_Already_Modified ) );
        }
    }
    IDE_EXCEPTION( ERR_UNIQUENESS_VIOLATION );
    {
        IDE_SET( ideSetErrorCode( smERR_ABORT_smnUniqueViolation ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : sdnbBTree::findInsertPos4NonUnique         *
 * ------------------------------------------------------------------*
 * Non Unique Key �϶�, insert position ã�� �Լ�                    *
 *********************************************************************/
IDE_RC sdnbBTree::findInsertPos4NonUnique( idvSQL*          aStatistics,
                                           sdnbStatistic  * aIndexStat,
                                           sdrMtx         * aMtx,
                                           idBool         * aMtxStart,
                                           sdnbStack      * aStack,
                                           sdnbHeader     * aIndexHeader,
                                           sdnbKeyInfo    * aKeyInfo,
                                           idBool         * aIsPessimistic,
                                           sdpPhyPageHdr ** aLeafNode,
                                           SShort         * aLeafKeySeq ,
                                           idBool         * aIsSameKey,
                                           idBool         * aIsRetry,
                                           SLong          * aTotalTraverseLength )
{
    sdpPhyPageHdr * sLeafNode;
    SShort          sLeafKeySeq;
    idBool          sIsSameKey = ID_FALSE;


    IDE_TEST( traverse( aStatistics,
                        aIndexHeader,
                        aMtx,
                        aKeyInfo,
                        aStack,
                        aIndexStat,
                        *aIsPessimistic,
                        &sLeafNode,
                        &sLeafKeySeq,
                        NULL,   /* aLeafSP */
                        &sIsSameKey,
                        aTotalTraverseLength ) != IDE_SUCCESS );

    if ( sLeafNode == NULL )
    {
        // traverse �ϴ� ���̿� root node�� �����.
        *aMtxStart = ID_FALSE;
        IDE_TEST( sdrMiniTrans::commit( aMtx ) != IDE_SUCCESS );


        *aIsRetry = ID_TRUE;

        return IDE_SUCCESS;
    }
    else
    {
        /* nothing to do */
    }

    *aLeafNode   = sLeafNode;
    *aLeafKeySeq = sLeafKeySeq;
    *aIsSameKey  = sIsSameKey;
    *aIsRetry    = ID_FALSE;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : sdnbBTree::insertKey                       *
 * ------------------------------------------------------------------*
 * index�� Ư�� Row�� �ش��ϴ� key�� insert�Ѵ�.                     *
 * �켱 �ش� Page(Node)�� X latch�� ��� insert�� ������ free ������ *
 * �ִ��� Ȯ���ϰ�, �����ϸ� insert�� �����Ѵ�.(Optimistic insert)   *
 *                                                                   *
 * ���� ������ �����Ͽ� Node Split���� SMO�� �߻��ؾ� �� ���, ���  *
 * latch�� Ǯ��, index�� SMO latch(tree latch)�� ���� ��, �ٽ�       *
 * traverse �Ͽ�, leaf node�� left->target->right ������ X latch��   *
 * ȹ���� ��, SMO�� �߻����� �Ϸ��� �Ŀ�, ������ Node�� row�� insert *
 * �Ѵ�.(Pessimistic Insert)                                         *
 *                                                                   *
 * Unique Index�� ��쿡�� Insertable Position�� ã��, ã�� ��ġ��   *
 * ���� Backward Scan and Forward Scan�ϸ鼭 Unique Violation�� �˻� *
 * �Ѵ�.                                                             *
 *                                                                   *
 * ���� Transaction������ ������ ����(CTS) �Ҵ��� �����ϴ� ��쿡��  *
 * ���� Active Transaction���� ������ Transaction�� �����ؼ� Waiting *
 * �ؾ� �ϸ� ��� ���Ŀ� �ٽ� ó������ �õ��Ѵ�.                   *
 *********************************************************************/
IDE_RC sdnbBTree::insertKey( idvSQL          * aStatistics,
                             void            * aTrans,
                             void            * aTable,
                             void            * aIndex,
                             smSCN             aInfiniteSCN,
                             SChar           * aKeyValue,
                             SChar           * /* aNullRow */,
                             idBool            aUniqueCheck,
                             smSCN             aStmtViewSCN,
                             void            * aRowSID,
                             SChar          ** /* aExistUniqueRow */,
                             ULong             aInsertWaitTime,
                             idBool            aForbiddenToRetry )
{

    sdrMtx           sMtx;
    sdnbStack        sStack;
    sdpPhyPageHdr *  sNewChildPage;
    UShort           sKeyValueLength;
    sdpPhyPageHdr *  sLeafNode;
    SShort           sLeafKeySeq;
    ULong            sIdxSmoNo;
    idBool           sIsPessimistic;
    idBool           sIsSuccess;
    idBool           sIsInsertSuccess = ID_TRUE;
    sdnbKeyInfo      sKeyInfo;
    idBool           sIsSameKey = ID_FALSE;
    idBool           sMtxStart = ID_FALSE;
    idBool           sIsRetry = ID_FALSE;
    idBool           sIsRetraverse = ID_FALSE;
    sdnbHeader     * sHeader = (sdnbHeader*)((smnIndexHeader*)aIndex)->mHeader;
    smLSN            sNTA;
    idvWeArgs        sWeArgs;
    UInt             sAllocPageCount = 0;
    sdSID            sRowSID;
    smSCN            sStmtSCN;
    sdpPhyPageHdr  * sTargetNode;
    SShort           sTargetSlotSeq;
    scPageID         sEmptyNodePID[2] = { SD_NULL_PID, SD_NULL_PID };
    UInt             sEmptyNodeCount = 0;
    scPageID         sPID;
    sdpPhyPageHdr  * sDumpNode;
    SLong            sTotalTraverseLength = 0;

    sRowSID = *((sdSID*)aRowSID);

    SM_SET_SCN( &sStmtSCN, &aStmtViewSCN );
    SM_CLEAR_SCN_VIEW_BIT( &sStmtSCN );

    sKeyInfo.mKeyValue   = (UChar *)aKeyValue;
    sKeyInfo.mRowPID     = SD_MAKE_PID( sRowSID );
    sKeyInfo.mRowSlotNum = SD_MAKE_SLOTNUM( sRowSID );
    sKeyInfo.mKeyState   = SM_SCN_IS_MAX( sStmtSCN ) ? SDNB_KEY_STABLE : SDNB_KEY_UNSTABLE;


    sNTA = smxTrans::getTransLstUndoNxtLSN( aTrans );


retry:
    // mtx�� �����Ѵ�.
    IDE_TEST( sdrMiniTrans::begin( aStatistics,
                                   &sMtx,
                                   aTrans,
                                   SDR_MTX_LOGGING,
                                   ID_TRUE,/*Undoable(PROJ-2162)*/
                                   gMtxDLogType ) != IDE_SUCCESS );
    sMtxStart = ID_TRUE;
    //initialize stack
    initStack( &sStack );

    if ( sHeader->mRootNode == SD_NULL_PID )
    {
        // tree x larch�� ��´�.

        IDV_WEARGS_SET( &sWeArgs,
                        IDV_WAIT_INDEX_LATCH_DRDB_BTREE_INDEX_SMO,
                        0,   // WaitParam1
                        0,   // WaitParam2
                        0 ); // WaitParam3

        IDE_TEST( sHeader->mLatch.lockWrite(
                                         aStatistics,
                                         &sWeArgs ) != IDE_SUCCESS );

        IDE_TEST( sdrMiniTrans::push( &sMtx,
                                      (void *)&sHeader->mLatch,
                                      SDR_MTX_LATCH_X )
                  != IDE_SUCCESS );

        if ( sHeader->mRootNode != SD_NULL_PID )
        {
            // latch ��� ���̿� root node�� ����.
            sMtxStart = ID_FALSE;
            IDE_TEST( sdrMiniTrans::commit( &sMtx ) != IDE_SUCCESS );


            goto retry;
        }
        else
        {
            /* nothing to do */
        }

        sKeyValueLength = getKeyValueLength( &(sHeader->mColLenInfoList),
                                             (UChar *)aKeyValue );

        IDE_TEST( makeNewRootNode( aStatistics,
                                   &(sHeader->mDMLStat),
                                   &sMtx,
                                   &sMtxStart,
                                   (smnIndexHeader*)aIndex,
                                   sHeader,
                                   &sStack,
                                   &sKeyInfo,
                                   sKeyValueLength,
                                   &sNewChildPage,
                                   &sAllocPageCount )
                  != IDE_SUCCESS );

        sLeafNode = sNewChildPage;
        sLeafKeySeq = 0;

        IDL_MEM_BARRIER;

        // run-time index header�� �ִ� SmoNo�� 1 ������Ų��.
        // sHeader->mSmoNo++;
        increaseSmoNo( sHeader );
    }
    else
    {
        sLeafKeySeq  = -1;

        getSmoNo( (void *)sHeader, &sIdxSmoNo );
        IDL_MEM_BARRIER;
        sIsPessimistic = ID_FALSE;

      retraverse:
        if ( aUniqueCheck == ID_TRUE )
        {
            // Unique Key�� insert position ã��
            IDE_TEST( sdnbBTree::findInsertPos4Unique( aStatistics,
                                                       aTrans,
                                                       aTable,
                                                       aIndex,
                                                       &(sHeader->mDMLStat),
                                                       aStmtViewSCN,
                                                       aInfiniteSCN,
                                                       &sMtx,
                                                       &sMtxStart,
                                                       &sStack,
                                                       sHeader,
                                                       &sKeyInfo,
                                                       &sIsPessimistic,
                                                       &sLeafNode,
                                                       &sLeafKeySeq,
                                                       &sIsSameKey,
                                                       &sIsRetry,
                                                       &sIsRetraverse,
                                                       &sIdxSmoNo,
                                                       aInsertWaitTime,
                                                       aForbiddenToRetry,
                                                       &sTotalTraverseLength )
                      != IDE_SUCCESS );
        }
        else
        {
            // Non Unique Key�� insert position ã��
            IDE_TEST( sdnbBTree::findInsertPos4NonUnique( aStatistics,
                                                          &(sHeader->mDMLStat),
                                                          &sMtx,
                                                          &sMtxStart,
                                                          &sStack,
                                                          sHeader,
                                                          &sKeyInfo,
                                                          &sIsPessimistic,
                                                          &sLeafNode,
                                                          &sLeafKeySeq,
                                                          &sIsSameKey,
                                                          &sIsRetry,
                                                          &sTotalTraverseLength )
                      != IDE_SUCCESS );

        }

        if ( sIsRetraverse == ID_TRUE )
        {
            sHeader->mDMLStat.mRetraverseCount++;
            sIsRetraverse = ID_FALSE;
            goto retraverse;
        }
        else
        {
            // nothing to do
        }

        if ( sIsRetry == ID_TRUE )
        {
            sHeader->mDMLStat.mOpRetryCount++;
            sIsRetry = ID_FALSE;
            goto retry;
        }
        else
        {
            // nothing to do
        }

        IDE_TEST( insertKeyIntoLeafNode( aStatistics,
                                         &(sHeader->mDMLStat),
                                         &sMtx,
                                         (smnIndexHeader*)aIndex,
                                         sHeader,
                                         sLeafNode,
                                         &sLeafKeySeq,
                                         &sKeyInfo,
                                         sIsPessimistic,
                                         &sIsSuccess )
                  != IDE_SUCCESS );

        if ( sIsSuccess != ID_TRUE )
        {
            if ( sIsPessimistic == ID_FALSE )
            {
                sHeader->mDMLStat.mPessimisticCount++;

                sIsPessimistic = ID_TRUE;
                sMtxStart = ID_FALSE;
                IDE_TEST( sdrMiniTrans::commit(&sMtx) != IDE_SUCCESS );


                // BUG-32481 Disk index can't execute node aging if
                // index segment does not have free space.
                //
                // node aging�� ���� �����Ͽ� Free Node��
                // Ȯ���Ѵ�. split�� �ʿ��� �ִ� ������ �� ��ŭ Ȯ����
                // �õ��Ѵ�. root���� split�ȴٰ� �����ϸ� (depth + 1)
                // ��ŭ Ȯ���ϸ� �ȴ�.
                IDE_TEST( nodeAging( aStatistics,
                                     aTrans,
                                     &(sHeader->mDMLStat),
                                     (smnIndexHeader*)aIndex,
                                     sHeader,
                                     (sStack.mIndexDepth + 1) )
                          != IDE_SUCCESS );

                IDE_TEST( sdrMiniTrans::begin(aStatistics,
                                              &sMtx,
                                              aTrans,
                                              SDR_MTX_LOGGING,
                                              ID_TRUE,/*Undoable(PROJ-2162)*/
                                              gMtxDLogType ) != IDE_SUCCESS );
                sMtxStart = ID_TRUE;

                IDV_WEARGS_SET( &sWeArgs,
                                IDV_WAIT_INDEX_LATCH_DRDB_BTREE_INDEX_SMO,
                                0,   // WaitParam1
                                0,   // WaitParam2
                                0 ); // WaitParam3

                // tree x larch�� ��´�.
                IDE_TEST( sHeader->mLatch.lockWrite(
                                                 aStatistics,
                                                 &sWeArgs ) != IDE_SUCCESS );

                IDE_TEST( sdrMiniTrans::push( &sMtx,
                                              (void *)&sHeader->mLatch,
                                              SDR_MTX_LATCH_X )
                          != IDE_SUCCESS );
                if ( validatePath( aStatistics,
                                   sHeader->mIndexTSID,
                                   &(sHeader->mDMLStat),
                                   &sStack,
                                   sIdxSmoNo )
                    != IDE_SUCCESS )
                {
                    // init stack
                    initStack( &sStack );
                }
                else
                {
                    sStack.mIndexDepth--; // leaf �ٷ� �� ����
                }

                // SmoNO�� ���� ����.
                getSmoNo( (void *)sHeader, &sIdxSmoNo );
                IDL_MEM_BARRIER;
                sHeader->mDMLStat.mRetraverseCount++;
    
                IDU_FIT_POINT("BUG-48920@sdnbBTree::insertKey::pessimistic::retraverse");
    
                goto retraverse;
            }
            else
            {
                sKeyValueLength = getKeyValueLength( &(sHeader->mColLenInfoList),
                                                     (UChar *)aKeyValue );

                // Leaf�� full�� �߻��� ��Ȳ�� ���� ó���� �Ѵ�.
                IDE_TEST( processLeafFull( aStatistics,
                                           &(sHeader->mDMLStat),
                                           &sMtx,
                                           &sMtxStart,
                                           (smnIndexHeader*)aIndex,
                                           sHeader,
                                           &sStack,
                                           sLeafNode,
                                           &sKeyInfo,
                                           sKeyValueLength,
                                           sLeafKeySeq,
                                           &sTargetNode,
                                           &sTargetSlotSeq,
                                           &sAllocPageCount,
                                           ID_FALSE ) // aIsDeleteLeafKey
                          != IDE_SUCCESS );

                if ( sIsSameKey == ID_TRUE )
                {
                    /*
                     * DupKey�� ���� Insertable Position�� DupKey�� ����Ű��
                     * �������� ������ �׷��� ���� TargetNode�� ��������
                     * �̵����Ѿ� �Ѵ�.
                     */
                    IDE_TEST( findTargetKeyForDupKey( &sMtx,
                                                      sHeader,
                                                      &(sHeader->mDMLStat),
                                                      &sKeyInfo,
                                                      &sTargetNode,
                                                      &sTargetSlotSeq )
                              != IDE_SUCCESS );

                    /* BUG-48920 : ���� insertKeyIntoLeafNode() ������ Aging �Ǿ
                     * DupKey�� ����� �� �ִ�. �� ��쿡 �Ʒ��� �ִ� insert��
                     * ó���ϸ� �ȴ�.
                     */
                }
                else
                {
                    /* nothing to do */
                }

                IDE_TEST( insertKeyIntoLeafNode( aStatistics,
                                                 &(sHeader->mDMLStat),
                                                 &sMtx,
                                                 (smnIndexHeader*)aIndex,
                                                 sHeader,
                                                 sTargetNode,
                                                 &sTargetSlotSeq,
                                                 &sKeyInfo,
                                                 ID_TRUE, /* aIsPessimistic */
                                                 &sIsSuccess )
                          != IDE_SUCCESS );

                /*
                 * Split ���Ŀ� Empty Node�� ���� �ȵ� ��������
                 * ����� ������, Empty Node Link�� Fail�� �߻��Ҽ� �ֱ�
                 * ������ �̴� Ʈ����� Commit ���Ŀ� �����Ѵ�.
                 */
                findEmptyNodes( &sMtx,
                                sHeader,
                                sLeafNode,
                                sEmptyNodePID,
                                &sEmptyNodeCount );

                IDE_DASSERT( validateLeaf( aStatistics,
                                           sTargetNode,
                                           &(sHeader->mDMLStat),
                                           sHeader,
                                           sHeader->mColumns,
                                           sHeader->mColumnFence )
                             == IDE_SUCCESS );

                if ( sIsSuccess != ID_TRUE )
                {
                    ideLog::log( IDE_ERR_0, "[ fail to insert key ]\n" );
 
                    // dump header
                    dumpHeadersAndIteratorToSMTrc( (smnIndexHeader*)aIndex,
                                                   sHeader,
                                                   NULL );
 
                    // dump key info
                    ideLog::logMem( IDE_DUMP_0,
                                    (UChar *)&sKeyInfo,
                                    ID_SIZEOF(sdnbKeyInfo),
                                    "Key Info:\n" );
                    ideLog::logMem( IDE_DUMP_0,
                                    (UChar *)aKeyValue,
                                    sKeyValueLength,
                                    "Key Value:\n" );
                    ideLog::log( IDE_DUMP_0, "Target slot sequence : %"ID_INT32_FMT"\n",
                                 sTargetSlotSeq );
 
                    // dump target node
                    dumpIndexNode( sTargetNode );
  
                    // dump prev node
                    sPID = sdpPhyPage::getPrvPIDOfDblList((sdpPhyPageHdr*)sTargetNode);
                    if ( sPID != SD_NULL_PID )
                    {
                        sDumpNode = (sdpPhyPageHdr*)
                            sdrMiniTrans::getPagePtrFromPageID( &sMtx,
                                                                sHeader->mIndexTSID,
                                                                sPID );
                        if ( sDumpNode != NULL )
                        {
                            ideLog::log( IDE_DUMP_0, "Prev Node:\n" );
                            dumpIndexNode( sDumpNode );
                        }
                        else
                        {
                            /* nothing to do */
                        }
                    }
                    else
                    {
                        /* nothing to do */
                    }

                    // dump next node
                    sPID = sdpPhyPage::getNxtPIDOfDblList((sdpPhyPageHdr*)sTargetNode);
                    if ( sPID != SD_NULL_PID )
                    {
                        sDumpNode = (sdpPhyPageHdr*)
                            sdrMiniTrans::getPagePtrFromPageID( &sMtx,
                                                                sHeader->mIndexTSID,
                                                                sPID );
                        if ( sDumpNode != NULL )
                        {
                            ideLog::log( IDE_DUMP_0, "Nxt Node:\n" );
                            dumpIndexNode( sDumpNode );
                        }
                        else
                        {
                            /* nothing to do */
                        }
                    }
                    else
                    {
                        /* nothing to do */
                    }

                    sIsInsertSuccess = ID_FALSE;
 
                    // Debug ��忡���� assert�� ó����
                    IDE_DASSERT( 0 );
                }
                else
                {
                    /* nothing to do */
                }

                IDL_MEM_BARRIER;

                sHeader->mDMLStat.mNodeSplitCount++;
                // run-time index header�� �ִ� SmoNo�� 1 ������Ų��.
                // sHeader->mSmoNo++;
                increaseSmoNo( sHeader );
            }
        }
    }

    IDU_FIT_POINT( "2.BUG-42785@sdnbBTree::insertKey::jump" );

    /* BUG-41101: BUG-41101.tc���� mini Tx. rollback�� �߻���Ű�� FIT ����Ʈ  */
    IDU_FIT_POINT_RAISE("1.BUG-41101@sdnbBTree::insertKey::rollback", MTX_ROLLBACK);

    if ( sIsInsertSuccess == ID_TRUE )
    {
        sdrMiniTrans::setRefNTA( &sMtx,
                                 sHeader->mIndexTSID,
                                 SDR_OP_SDN_INSERT_KEY_WITH_NTA,
                                 &sNTA );
    }
    else
    {
        /* nothing to do */
    }
    
    sMtxStart = ID_FALSE;
    IDE_TEST( sdrMiniTrans::commit(&sMtx) != IDE_SUCCESS );

    /* FIT/ART/sm/Bugs/BUG-25128/BUG-25128.tc */
    IDU_FIT_POINT( "6.PROJ-1552@sdnbBTree::insertKey" ); // !!CHECK RECOVERY POINT

    if ( sEmptyNodeCount > 0 )
    {
        IDE_TEST( linkEmptyNodes( aStatistics,
                                  aTrans,
                                  sHeader,
                                  &(sHeader->mDMLStat),
                                  sEmptyNodePID )
                  != IDE_SUCCESS );
    }
    else
    {
        /* nothing to do */
    }

    IDE_TEST_RAISE( sIsInsertSuccess != ID_TRUE, ERR_INSERT_KEY );
    
    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_INSERT_KEY );
    {
        /* BUG-33112: [sm-disk-index] The disk index makes a mistake in
         * key redistibution judgment. */
        IDE_SET( ideSetErrorCode(smERR_ABORT_INTERNAL_ARG, 
                                 "fail to insert key") );
    }
#ifdef ALTIBASE_FIT_CHECK
    /* BUG-41101 */
    IDE_EXCEPTION( MTX_ROLLBACK );
#endif
    IDE_EXCEPTION_END;

    if ( sMtxStart == ID_TRUE )
    {
        (void)sdrMiniTrans::rollback(&sMtx);
    }
    else
    {
        /* nothing to do ... */
    }

    return IDE_FAILURE;

}

/*********************************************************************
 * FUNCTION DESCRIPTION : sdnbBTree::deleteLeafKeyWithTBT            *
 * ------------------------------------------------------------------*
 * TBT ������ Ű�� �����Ѵ�.                                         *
 * CTS�� �����ϴ� Ʈ������� ������ Binding�Ѵ�.                     *
 *********************************************************************/
IDE_RC sdnbBTree::deleteLeafKeyWithTBT( sdrMtx               * aMtx,
                                        sdnbHeader           * aIndex,
                                        UChar                  aCTSlotNum,
                                        sdpPhyPageHdr        * aLeafPage,
                                        SShort                 aLeafKeySeq  )
{
    UChar               * sSlotDirPtr;
    sdnbLKey            * sLeafKey;
    scOffset              sKeyOffset = 0;
    UShort                sKeyLength;
    sdnbRollbackContext   sRollbackContext;
    sdnbNodeHdr         * sNodeHdr;
    SShort                sDummySlotSeq;
    idBool                sRemoveInsert;
    UShort                sUnlimitedKeyCount;

    sSlotDirPtr = sdpPhyPage::getSlotDirStartPtr( (UChar *)aLeafPage );
    IDE_TEST( sdpSlotDirectory::getPagePtrFromSlotNum( sSlotDirPtr,
                                                       aLeafKeySeq,
                                                       (UChar **)&sLeafKey )
              != IDE_SUCCESS );
    IDE_TEST( sdpSlotDirectory::getValue( sSlotDirPtr, aLeafKeySeq, &sKeyOffset )
              != IDE_SUCCESS );
    sKeyLength = getKeyLength( &(aIndex->mColLenInfoList),
                               (UChar *)sLeafKey,
                               ID_TRUE /* aIsLeaf */ );

    IDE_TEST( sdnIndexCTL::bindCTS( aMtx,
                                    aIndex->mIndexTSID,
                                    aLeafPage,
                                    aCTSlotNum,
                                    sKeyOffset )
              != IDE_SUCCESS );

    sRollbackContext.mTableOID = aIndex->mTableOID;
    sRollbackContext.mIndexID  = aIndex->mIndexID;

    SDNB_SET_LCTS_NO( sLeafKey, aCTSlotNum );
    SDNB_SET_STATE( sLeafKey, SDNB_KEY_DELETED );

    sNodeHdr = (sdnbNodeHdr*)sdpPhyPage::getLogicalHdrStartPtr( (UChar *)aLeafPage );
    sUnlimitedKeyCount = sNodeHdr->mUnlimitedKeyCount - 1;
    IDE_DASSERT( sUnlimitedKeyCount < sdpSlotDirectory::getCount( sSlotDirPtr ) );

    IDE_TEST( sdrMiniTrans::writeNBytes( aMtx,
                                         (UChar *)&(sNodeHdr->mUnlimitedKeyCount),
                                         (void *)&sUnlimitedKeyCount,
                                         ID_SIZEOF(UShort) )
              != IDE_SUCCESS );

    sdrMiniTrans::setRefOffset( aMtx, SM_OID_NULL );
    IDE_TEST( sdrMiniTrans::writeLogRec( aMtx,
                                         (UChar *)sLeafKey,
                                         (void *)NULL,
                                         sKeyLength +
                                         ID_SIZEOF(sdnbRollbackContext) +
                                         ID_SIZEOF(SShort) +
                                         ID_SIZEOF(UChar),
                                         SDR_SDN_DELETE_KEY_WITH_NTA )
              != IDE_SUCCESS );

    IDE_TEST( sdrMiniTrans::write( aMtx,
                                   (void *)&sRollbackContext,
                                   ID_SIZEOF(sdnbRollbackContext) )
              != IDE_SUCCESS );

    sRemoveInsert = ID_FALSE;
    IDE_TEST( sdrMiniTrans::write( aMtx,
                                   (void *)&sRemoveInsert,
                                   ID_SIZEOF(UChar) )
              != IDE_SUCCESS );

    sDummySlotSeq = 0; /* �ǹ̾��� �� */
    IDE_TEST( sdrMiniTrans::write( aMtx,
                                   (void *)&sDummySlotSeq,
                                   ID_SIZEOF(SShort) )
              != IDE_SUCCESS );

    IDE_TEST( sdrMiniTrans::write( aMtx,
                                   (void *)sLeafKey,
                                   sKeyLength )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : sdnbBTree::deleteLeafKeyWithTBK            *
 * ------------------------------------------------------------------*
 * TBK ������ Ű�� �����Ѵ�.                                         *
 * Key ��ü�� �����ϴ� Ʈ������� ������ Binding �Ѵ�.               *
 * ���� Ű�� TBK��� ���ο� ������ �Ҵ��� �ʿ䰡 ������ �ݴ���       *
 * ���� Ű�� ���� ������ �Ҵ��ؾ� �Ѵ�.                            *
 *********************************************************************/
IDE_RC sdnbBTree::deleteLeafKeyWithTBK( idvSQL        * aStatistics,
                                        sdrMtx        * aMtx,
                                        sdnbHeader    * aIndex,
                                        sdpPhyPageHdr * aLeafPage,
                                        SShort        * aLeafKeySeq ,
                                        idBool          aIsPessimistic,
                                        idBool        * aIsSuccess )
{
    UChar                 sAgedCount = 0;
    UShort                sKeyLength;
    UShort                sAllowedSize;
    UChar               * sSlotDirPtr;
    sdnbLKey            * sLeafKey = NULL;
    UChar                 sTxBoundType;
    sdnbLKey            * sRemoveKey;
    sdnbRollbackContext   sRollbackContext;
    UShort                sUnlimitedKeyCount;
    sdnbNodeHdr         * sNodeHdr;
    UShort                sTotalTBKCount = 0;
    UChar                 sRemoveInsert = ID_FALSE;
    smSCN                 sFstDiskViewSCN;
    sdSID                 sTSSlotSID;
    scOffset              sOldKeyOffset = SC_NULL_OFFSET;
    scOffset              sNewKeyOffset = SC_NULL_OFFSET;

    *aIsSuccess = ID_TRUE;

    sFstDiskViewSCN = smLayerCallback::getFstDskViewSCN( aMtx->mTrans );
    sTSSlotSID      = smLayerCallback::getTSSlotSID( aMtx->mTrans );

    sSlotDirPtr = sdpPhyPage::getSlotDirStartPtr( (UChar *)aLeafPage );
    IDE_TEST( sdpSlotDirectory::getPagePtrFromSlotNum( sSlotDirPtr,
                                                       *aLeafKeySeq,
                                                       (UChar **)&sRemoveKey )
              != IDE_SUCCESS );

    sTxBoundType = SDNB_GET_TB_TYPE( sRemoveKey );

    sKeyLength = SDNB_LKEY_LEN( getKeyValueLength( &(aIndex->mColLenInfoList),
                                                   SDNB_LKEY_KEY_PTR( sRemoveKey ) ),
                                 SDNB_KEY_TB_KEY );

    if ( sTxBoundType == SDNB_KEY_TB_KEY )
    {
        sRemoveInsert = ID_FALSE;
        sLeafKey = sRemoveKey;
        IDE_CONT( SKIP_ALLOC_SLOT );
    }
    else
    {
        /* nothing to do */
    }

    /*
     * canAllocLeafKey ������ Compaction���� ���Ͽ�
     * KeyMapSeq�� ����ɼ� �ִ�.
     */
    if ( canAllocLeafKey( aMtx,
                          aIndex,
                          aLeafPage,
                          (UInt)sKeyLength,
                          aIsPessimistic,
                          aLeafKeySeq  ) != IDE_SUCCESS )
    {
        /*
         * ���������� ���� �Ҵ��� ���ؼ� Self Aging�� �Ѵ�.
         */
        IDE_TEST( selfAging( aStatistics,
                             aIndex,
                             aMtx,
                             aLeafPage,
                             &sAgedCount )
                  != IDE_SUCCESS );

        if ( sAgedCount > 0 )
        {
            if ( canAllocLeafKey( aMtx,
                                  aIndex,
                                  aLeafPage,
                                  (UInt)sKeyLength,
                                  aIsPessimistic,
                                  aLeafKeySeq  )
                != IDE_SUCCESS )
            {
                *aIsSuccess = ID_FALSE;
            }
        }
        else
        {
            *aIsSuccess = ID_FALSE;
        }
    }
    else
    {
        /* nothing to do */
    }

    IDE_TEST_CONT( *aIsSuccess == ID_FALSE, ALLOC_FAILURE );

    sRemoveInsert = ID_TRUE;
    IDE_TEST( sdpPhyPage::allocSlot( (sdpPhyPageHdr*)aLeafPage,
                                     *aLeafKeySeq,
                                     sKeyLength,
                                     ID_TRUE,
                                     &sAllowedSize,
                                     (UChar **)&sLeafKey,
                                     &sNewKeyOffset,
                                     1 )
              != IDE_SUCCESS );

    idlOS::memset( sLeafKey, 0x00, sKeyLength );

    IDE_TEST( sdpSlotDirectory::getPagePtrFromSlotNum( sSlotDirPtr,
                                                       *aLeafKeySeq + 1,
                                                       (UChar **)&sRemoveKey )
              != IDE_SUCCESS );
    idlOS::memcpy( sLeafKey, sRemoveKey, ID_SIZEOF( sdnbLKey ) );
    SDNB_SET_TB_TYPE( sLeafKey, SDNB_KEY_TB_KEY );
    idlOS::memcpy( SDNB_LKEY_KEY_PTR( sLeafKey ),
                   SDNB_LKEY_KEY_PTR( sRemoveKey ),
                   getKeyValueLength( &(aIndex->mColLenInfoList),
                                      SDNB_LKEY_KEY_PTR( sRemoveKey ) ) );

    // BUG-29506 TBT�� TBK�� ��ȯ�� offset�� CTS�� �ݿ����� �ʽ��ϴ�.
    // ���� offset���� TBK�� �����Ǵ� key�� offset���� ����
    IDE_TEST( sdpSlotDirectory::getValue( sSlotDirPtr,
                                          *aLeafKeySeq + 1,
                                          &sOldKeyOffset )
              != IDE_SUCCESS );

    if ( SDN_IS_VALID_CTS(SDNB_GET_CCTS_NO(sLeafKey)) )
    {
        IDE_TEST( sdnIndexCTL::updateRefKey( aMtx,
                                             aLeafPage,
                                             SDNB_GET_CCTS_NO( sLeafKey ),
                                             sOldKeyOffset,
                                             sNewKeyOffset )
                  != IDE_SUCCESS );
    }
    else
    {
        /* nothing to do */
    }

    IDE_EXCEPTION_CONT( SKIP_ALLOC_SLOT );

    SDNB_SET_TBK_LSCN( ((sdnbLKeyEx*)sLeafKey), &sFstDiskViewSCN );
    SDNB_SET_TBK_LTSS( ((sdnbLKeyEx*)sLeafKey), &sTSSlotSID );

    SDNB_SET_LCTS_NO( sLeafKey, SDN_CTS_IN_KEY );
    SDNB_SET_STATE( sLeafKey, SDNB_KEY_DELETED );

    sRollbackContext.mTableOID = aIndex->mTableOID;
    sRollbackContext.mIndexID  = aIndex->mIndexID;

    sNodeHdr = (sdnbNodeHdr*)sdpPhyPage::getLogicalHdrStartPtr( (UChar *)aLeafPage );
    sUnlimitedKeyCount = sNodeHdr->mUnlimitedKeyCount - 1;

    IDE_TEST( sdrMiniTrans::writeNBytes( aMtx,
                                         (UChar *)&(sNodeHdr->mUnlimitedKeyCount),
                                         (void *)&sUnlimitedKeyCount,
                                         ID_SIZEOF(UShort) )
              != IDE_SUCCESS );

    if ( sRemoveInsert == ID_TRUE )
    {
        sTotalTBKCount = sNodeHdr->mTBKCount + 1;
        IDE_TEST( sdrMiniTrans::writeNBytes( aMtx,
                                             (UChar *)&(sNodeHdr->mTBKCount),
                                             (void *)&sTotalTBKCount,
                                             ID_SIZEOF(UShort) )
                  != IDE_SUCCESS );
    }
    else
    {
        /* nothing to do */
    }

    sdrMiniTrans::setRefOffset( aMtx, SM_OID_NULL );
    IDE_TEST( sdrMiniTrans::writeLogRec( aMtx,
                                         (UChar *)sLeafKey,
                                         (void *)NULL,
                                         sKeyLength +
                                         ID_SIZEOF(sdnbRollbackContext) +
                                         ID_SIZEOF(SShort) +
                                         ID_SIZEOF(UChar),
                                         SDR_SDN_DELETE_KEY_WITH_NTA )
              != IDE_SUCCESS );

    IDE_TEST( sdrMiniTrans::write( aMtx,
                                   (void *)&sRollbackContext,
                                   ID_SIZEOF(sdnbRollbackContext) )
              != IDE_SUCCESS );

    IDE_TEST( sdrMiniTrans::write( aMtx,
                                   (void *)&sRemoveInsert,
                                   ID_SIZEOF(UChar) )
              != IDE_SUCCESS );
    IDE_TEST( sdrMiniTrans::write( aMtx,
                                   (void *)aLeafKeySeq ,
                                   ID_SIZEOF(SShort) )
              != IDE_SUCCESS );

    IDE_TEST( sdrMiniTrans::write( aMtx,
                                   (void *)sLeafKey,
                                   sKeyLength )
              != IDE_SUCCESS );

    /*
     * ���ο� KEY�� �Ҵ�Ǿ��ٸ� ���� KEY�� �����Ѵ�.
     */
    if ( sRemoveInsert == ID_TRUE )
    {
        IDE_DASSERT( sTxBoundType != SDNB_KEY_TB_KEY );

        sKeyLength = getKeyLength( &(aIndex->mColLenInfoList),
                                   (UChar *)sRemoveKey,
                                   ID_TRUE /* aIsLeaf */ );

        IDE_TEST(sdrMiniTrans::writeLogRec(aMtx,
                                           (UChar *)sRemoveKey,
                                           (void *)&sKeyLength,
                                           ID_SIZEOF(UShort),
                                           SDR_SDN_FREE_INDEX_KEY )
                 != IDE_SUCCESS );

        IDE_TEST( sdpPhyPage::freeSlot( aLeafPage,
                                        *aLeafKeySeq + 1,
                                        sKeyLength,
                                        1 )
                  != IDE_SUCCESS );
    }
    else
    {
        /* nothing to do */
    }

    IDE_EXCEPTION_CONT( ALLOC_FAILURE );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/*********************************************************************
 * FUNCTION DESCRIPTION : sdnbBTree::insertLeafKeyWithTBT            *
 * ------------------------------------------------------------------*
 * TBT ������ Ű�� �����Ѵ�.                                         *
 * Ʈ������� ������ CTS�� Binding �Ѵ�.                             *
 *********************************************************************/
IDE_RC sdnbBTree::insertLeafKeyWithTBT( sdrMtx               * aMtx,
                                        sdnbHeader           * aIndex,
                                        UChar                  aCTSlotNum,
                                        sdpPhyPageHdr        * aLeafPage,
                                        sdnbKeyInfo          * aKeyInfo,
                                        UShort                 aKeyValueSize,
                                        idBool                 aIsDupKey,
                                        SShort                 aKeyMapSeq )
{
#ifdef DEBUG
    UChar      * sSlotDirPtr;
    sdnbLKey   * sLeafKey;
#endif
    UShort       sKeyOffset;
    UShort       sAllocatedKeyOffset;
    UShort       sKeyLength;

    sKeyLength = SDNB_LKEY_LEN( aKeyValueSize, SDNB_KEY_TB_CTS );

    if ( aIsDupKey == ID_TRUE )
    {
#ifdef DEBUG
        sSlotDirPtr = sdpPhyPage::getSlotDirStartPtr( (UChar *)aLeafPage );
        IDE_TEST( sdpSlotDirectory::getPagePtrFromSlotNum( sSlotDirPtr,
                                                           aKeyMapSeq,
                                                           (UChar **)&sLeafKey )
                  != IDE_SUCCESS );

        IDE_DASSERT( SDNB_EQUAL_PID_AND_SLOTNUM( sLeafKey, aKeyInfo ) );
#endif
        /*
         * Ʈ����� ������ Ű�� �����Ǵ� ��쿡 DupKey�� �����Ҵ��� �ʿ����.
         */
        IDE_TEST( makeDuplicateKey( aMtx,
                                    aIndex,
                                    aCTSlotNum,
                                    aLeafPage,
                                    aKeyMapSeq,
                                    SDNB_KEY_TB_CTS )
                  != IDE_SUCCESS );
    }
    else
    {
        sKeyOffset = sdpPhyPage::getAllocSlotOffset( aLeafPage, sKeyLength );

        if ( aKeyInfo->mKeyState != SDNB_KEY_STABLE )
        {
            IDE_DASSERT( aCTSlotNum != SDN_CTS_INFINITE );

            IDE_TEST( sdnIndexCTL::bindCTS( aMtx,
                                            aIndex->mIndexTSID,
                                            aLeafPage,
                                            aCTSlotNum,
                                            sKeyOffset )
                      != IDE_SUCCESS );
        }
        else
        {
            /* nothing to do */
        }

        IDE_TEST( insertLKey( aMtx,
                              aIndex,
                              (sdpPhyPageHdr*)aLeafPage,
                              aKeyMapSeq,
                              aCTSlotNum,
                              SDNB_KEY_TB_CTS,
                              aKeyInfo,
                              aKeyValueSize,
                              ID_TRUE, //aIsLogging
                              &sAllocatedKeyOffset )
                  != IDE_SUCCESS );

        if ( sKeyOffset != sAllocatedKeyOffset )
        {
            dumpHeadersAndIteratorToSMTrc( NULL,
                                           aIndex,
                                           NULL );
            ideLog::log( IDE_ERR_0,
                         "Key Offset : %"ID_UINT32_FMT""
                         ", Allocated Key Offset : %"ID_UINT32_FMT""
                         "\nKeyMap sequence : %"ID_INT32_FMT""
                         ", CT slot number : %"ID_UINT32_FMT""
                         ", Key Value size : %"ID_UINT32_FMT""
                         "\nKey Info :\n",
                         sKeyOffset, sAllocatedKeyOffset,
                         aKeyMapSeq, aCTSlotNum, aKeyValueSize );
            ideLog::logMem( IDE_DUMP_0,
                            (UChar *)aKeyInfo, ID_SIZEOF(sdnbKeyInfo) );
            dumpIndexNode( (sdpPhyPageHdr *)aLeafPage );
            IDE_ASSERT( 0 );
        }
        else
        {
            /* nothing to do */
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : sdnbBTree::insertLeafKeyWithTBK            *
 * ------------------------------------------------------------------*
 * TBK ������ Ű�� �����Ѵ�.                                         *
 * �ش� �Լ��� CTS�� �Ҵ��Ҽ� ���� ��쿡 ȣ��Ǹ�, Ʈ�������       *
 * ������ KEY ��ü�� Binding �Ѵ�.                                   *
 *********************************************************************/
IDE_RC sdnbBTree::insertLeafKeyWithTBK( sdrMtx               * aMtx,
                                        sdnbHeader           * aIndex,
                                        sdpPhyPageHdr        * aLeafPage,
                                        sdnbKeyInfo          * aKeyInfo,
                                        UShort                 aKeyValueSize,
                                        idBool                 aIsDupKey,
                                        SShort                 aKeyMapSeq )
{
    UShort sKeyLength;
    UShort sKeyOffset;
    UShort sAllocatedKeyOffset;

    sKeyLength = SDNB_LKEY_LEN( aKeyValueSize, SDNB_KEY_TB_KEY );

    if ( aIsDupKey == ID_TRUE )
    {
        /*
         * Ʈ����� ������ Ű�� �����Ǿ�� �Ѵٸ� ���ο� Ű��
         * �Ҵ�Ǿ�� �Ѵ�.
         */
        IDE_TEST( makeDuplicateKey( aMtx,
                                    aIndex,
                                    SDN_CTS_IN_KEY,
                                    aLeafPage,
                                    aKeyMapSeq,
                                    SDNB_KEY_TB_KEY )
                  != IDE_SUCCESS );
    }
    else
    {
        sKeyOffset = sdpPhyPage::getAllocSlotOffset( aLeafPage, sKeyLength );

        IDE_TEST( insertLKey( aMtx,
                              aIndex,
                              (sdpPhyPageHdr*)aLeafPage,
                              aKeyMapSeq,
                              SDN_CTS_IN_KEY,
                              SDNB_KEY_TB_KEY,
                              aKeyInfo,
                              aKeyValueSize,
                              ID_TRUE, //aIsLogging
                              &sAllocatedKeyOffset )
                  != IDE_SUCCESS );
        if ( sKeyOffset != sAllocatedKeyOffset )
        {
            dumpHeadersAndIteratorToSMTrc( NULL,
                                           aIndex,
                                           NULL );
            ideLog::log( IDE_ERR_0,
                         "Key Offset : %"ID_UINT32_FMT""
                         ", Allocated Key Offset : %"ID_UINT32_FMT""
                         "\nKeyMap sequence : %"ID_INT32_FMT""
                         ", CT slot number : %"ID_UINT32_FMT""
                         ", Key Value size : %"ID_UINT32_FMT""
                         "\nKey Info :\n",
                         sKeyOffset, sAllocatedKeyOffset,
                         aKeyMapSeq, SDN_CTS_IN_KEY, aKeyValueSize );
            ideLog::logMem( IDE_DUMP_0,
                            (UChar *)aKeyInfo, ID_SIZEOF(sdnbKeyInfo) );
            dumpIndexNode( (sdpPhyPageHdr *)aLeafPage );
            IDE_ASSERT( 0 );
        }
        else
        {
            /* nothing to do */
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

/*********************************************************************
 * FUNCTION DESCRIPTION : sdnbBTree::validatePath                    *
 * ------------------------------------------------------------------*
 * Index�� SMO�� �߻��Ұ��� ����Ǿ� optimistic ����� pessimistic   *
 * ������� ��ȯ�Ҷ�, optimistic���� traverse�� stack�� ��� node��  *
 * ������ �ڷ� SMO�� �߻����� �ʾҴ��� üũ�ϴ� �Լ��̴�.            *
 * SMO�� �߰ߵǸ� IDE_FAILURE�� �����Ѵ�.                            *
 * tree latch�� x latch�� ��� ȣ��Ǹ�, aNode���� Leaf �ٷ� ����    *
 * ��带 ��ȯ�Ѵ�.                                                  *
 *********************************************************************/
IDE_RC sdnbBTree::validatePath( idvSQL*          aStatistics,
                                scSpaceID        aIndexTSID,
                                sdnbStatistic *  aIndexStat,
                                sdnbStack *      aStack,
                                ULong            aSmoNo )
{
    sdpPhyPageHdr * sNode;
    SInt            sDepth  = -1;
    idBool          sTrySuccess;

    while (1) // ������ ���� ������ ����
    {
        sDepth++;
        if ( sDepth == aStack->mIndexDepth )
        {
            break;
        }
        else
        {
            // fix last node in stack to sNode
            IDE_TEST( sdnbBTree::getPage( aStatistics,
                                          &(aIndexStat->mIndexPage),
                                          aIndexTSID,
                                          aStack->mStack[sDepth].mNode,
                                          SDB_S_LATCH,
                                          SDB_WAIT_NORMAL,
                                          NULL,
                                          (UChar **)&sNode,
                                          &sTrySuccess) != IDE_SUCCESS );

            IDE_TEST_RAISE( sdpPhyPage::getIndexSMONo(sNode) > aSmoNo,
                            ERR_SMO_PROCCESSING );

            //  unfix sNode
            IDE_TEST( sdbBufferMgr::releasePage( aStatistics,
                                                 (UChar *)sNode )
                      != IDE_SUCCESS );
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_SMO_PROCCESSING );
    {
        (void) sdbBufferMgr::releasePage( aStatistics,
                                          (UChar *)sNode );
    }

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

/*********************************************************************
 * FUNCTION DESCRIPTION : sdnbBTree::deleteInternalKey               *
 * ------------------------------------------------------------------*
 * Internal node���� �ش� Row�� ����Ű�� slot�� �����Ѵ�.            *
 * �� �Լ��� SMO latch(tree latch)�� x latch�� ���� ���¿� ȣ��Ǹ�, *
 * internal node�� ���� merge ������ �߻��� ��쵵 �ִ�.             *
 *                                                                   *
 * �� �Լ������� internal node�� ������ slot�� �����Ǿ� slot count�� *
 * 0�� �Ǵ� node�� �߻��� ���� ������, �̷� ���� traverse�ÿ�      *
 * ������ leftmost child�� �б��Ѵ�.
 *********************************************************************/
IDE_RC sdnbBTree::deleteInternalKey ( idvSQL *        aStatistics,
                                      sdnbHeader    * aIndex,
                                      sdnbStatistic * aIndexStat,
                                      scPageID        aSegPID,
                                      sdrMtx        * aMtx,
                                      sdnbStack     * aStack,
                                      UInt          * aFreePageCount )
{

    idBool          sIsSuccess;
    UShort          sSlotCount;
    sdpPhyPageHdr  *sPage;
    sdnbNodeHdr    *sNodeHdr;
    sdnbIKey       *sIKey;
    SShort          sSeq;
    UChar          *sSlotDirPtr;
    UChar          *sFreeKey;
    UShort          sKeyLen;
    scPageID        sPID = aStack->mStack[aStack->mCurrentDepth].mNode;
    scPageID        sChildPID = SC_NULL_PID;
    ULong           sSmoNo;

    // x latch current internal node
    IDE_TEST( sdnbBTree::getPage( aStatistics,
                                  &(aIndexStat->mIndexPage),
                                  aIndex->mIndexTSID,
                                  sPID,
                                  SDB_X_LATCH,
                                  SDB_WAIT_NORMAL,
                                  aMtx,
                                  (UChar **)&sPage,
                                  &sIsSuccess ) != IDE_SUCCESS );
    // set SmoNo
    getSmoNo( (void *)aIndex, &sSmoNo );
    sdpPhyPage::setIndexSMONo( (UChar *)sPage, sSmoNo + 1 );

    IDL_MEM_BARRIER;

    sSlotDirPtr = sdpPhyPage::getSlotDirStartPtr( (UChar *)sPage );
    sSlotCount  = sdpSlotDirectory::getCount( sSlotDirPtr );

    if ( sSlotCount == 0 ) // leftmost child�� ���� �б�� path
    {
        IDE_DASSERT( aStack->mStack[aStack->mCurrentDepth].mKeyMapSeq == -1 );

        if ( aStack->mCurrentDepth == 0 )
        {
            IDE_TEST( unsetRootNode( aStatistics,
                                     aMtx,
                                     aIndex,
                                     aIndexStat )
                      != IDE_SUCCESS );
        }
        else
        {
            aStack->mCurrentDepth--;
            IDE_TEST( deleteInternalKey ( aStatistics,
                                          aIndex,
                                          aIndexStat,
                                          aSegPID,
                                          aMtx,
                                          aStack,
                                          aFreePageCount )
                      != IDE_SUCCESS );
            aStack->mCurrentDepth++;
        }

        IDE_TEST( freePage( aStatistics,
                            aIndexStat,
                            aIndex,
                            aMtx,
                            (UChar *)sPage )
                 != IDE_SUCCESS );
        aIndexStat->mNodeDeleteCount++;

        (*aFreePageCount)++;
    }
    else
    {
        sSeq     = aStack->mStack[aStack->mCurrentDepth].mKeyMapSeq;
        sNodeHdr = (sdnbNodeHdr*)sdpPhyPage::getLogicalHdrStartPtr((UChar *)sPage);

        /* BUG-27255 Leaf Node�� Tree�� �Ŵ޸�ü EmptyNodeList�� �Ŵ޸��� ����
         *���� ���� üũ�� �ʿ��մϴ�.                                      */
        if ( sSeq == -1 )    //LeftMostChildPID
        {
            sChildPID = sNodeHdr->mLeftmostChild;
        }
        else //RightChildPID
        {
            IDE_TEST( sdpSlotDirectory::getPagePtrFromSlotNum( sSlotDirPtr, 
                                                               sSeq,
                                                               (UChar **)&sIKey )
                      != IDE_SUCCESS );

            SDNB_GET_RIGHT_CHILD_PID( &sChildPID, sIKey );
        }

        if ( sChildPID != aStack->mStack[aStack->mCurrentDepth+1].mNode )
        {
            dumpHeadersAndIteratorToSMTrc( NULL,
                                           aIndex,
                                           NULL );
            ideLog::log( IDE_ERR_0,
                         "Child node PID : %"ID_UINT32_FMT""
                         ", Next Depth PID : %"ID_UINT32_FMT""
                         "\nCurrent Depth : %"ID_INT32_FMT"\n",
                         sChildPID, aStack->mStack[aStack->mCurrentDepth+1].mNode,
                         aStack->mCurrentDepth);
            ideLog::log( IDE_DUMP_0, "Index traverse stack:\n" );
            ideLog::logMem( IDE_DUMP_0, (UChar *)aStack, ID_SIZEOF(sdnbStack) );
            ideLog::log( IDE_DUMP_0, "Key sequence : %"ID_INT32_FMT"\n", sSeq );
            dumpIndexNode( sPage );
            IDE_ASSERT( 0 );
        }
        else
        {
            /* nothing to do */
        }

        IDE_TEST( sdpSlotDirectory::getPagePtrFromSlotNum( sSlotDirPtr, 
                                                           0,
                                                           (UChar **)&sIKey )
                  != IDE_SUCCESS );
        if ( ( aStack->mCurrentDepth == 0 ) && ( sSlotCount == 1 ) ) // root
        {
            IDE_TEST( backupRuntimeHeader( aMtx, aIndex ) != IDE_SUCCESS );
            if ( sSeq == -1 )
            {
                // leftmost branch�� ������...-> right child�� root�� ��
                SDNB_GET_RIGHT_CHILD_PID( &aIndex->mRootNode, sIKey );
                sSeq = 0;
            }
            else
            {
                IDE_DASSERT( sSeq == 0 );
                // right child branch�� ������...-> leftmost child�� root�� ��
                aIndex->mRootNode = sNodeHdr->mLeftmostChild;
            }
            IDE_TEST( setIndexMetaInfo( aStatistics,
                                        aIndex,
                                        aIndexStat,
                                        aMtx,
                                        &aIndex->mRootNode,
                                        NULL,   /* aMinPID */
                                        NULL,   /* aMaxPID */
                                        NULL,   /* aFreeNodeHead */
                                        NULL,   /* aFreeNodeCnt */
                                        NULL,   /* aIsCreatedWithLogging */
                                        NULL,   /* aIsConsistent */
                                        NULL,   /* aIsCreatedWithForce */
                                        NULL )  /* aNologgingCompletionLSN */
                      != IDE_SUCCESS );

            // remove 0'th slot
            IDE_TEST( sdpSlotDirectory::getPagePtrFromSlotNum( sSlotDirPtr, 
                                                               0, 
                                                               &sFreeKey )
                      != IDE_SUCCESS );
            sKeyLen = getKeyLength( &(aIndex->mColLenInfoList),
                                    (UChar *)sFreeKey,
                                    ID_FALSE /* aIsLeaf */);

            IDE_TEST( sdrMiniTrans::writeLogRec( aMtx,
                                                 (UChar *)sFreeKey,
                                                 (void *)&sKeyLen,
                                                 ID_SIZEOF(UShort),
                                                 SDR_SDN_FREE_INDEX_KEY )
                      != IDE_SUCCESS );

            IDE_TEST( sdpPhyPage::freeSlot( sPage,
                                            sSeq,
                                            sKeyLen,
                                            1 )
                      != IDE_SUCCESS );

            IDE_TEST( freePage( aStatistics,
                                aIndexStat,
                                aIndex,
                                aMtx,
                                (UChar *)sPage )
                      != IDE_SUCCESS );
            aIndexStat->mNodeDeleteCount++;
        }
        else // internal node
        {
            if ( sSeq == -1 )
            {
                IDE_TEST(sdrMiniTrans::writeNBytes(
                                         aMtx,
                                         (UChar *)&sNodeHdr->mLeftmostChild,
                                         (void *)&sIKey->mRightChild,
                                         ID_SIZEOF(sIKey->mRightChild) ) 
                         != IDE_SUCCESS );
                sSeq = 0;
            }
            else
            {
                /* nothing to do */
            }

            IDE_TEST( sdpSlotDirectory::getPagePtrFromSlotNum( sSlotDirPtr, 
                                                               sSeq,
                                                               &sFreeKey )
                      != IDE_SUCCESS );
            sKeyLen = getKeyLength( &(aIndex->mColLenInfoList),
                                    (UChar *)sFreeKey,
                                    ID_FALSE /* aIsLeaf */);

            IDE_TEST(sdrMiniTrans::writeLogRec( aMtx,
                                                (UChar *)sFreeKey,
                                                (void *)&sKeyLen,
                                                ID_SIZEOF(UShort),
                                                SDR_SDN_FREE_INDEX_KEY )
                     != IDE_SUCCESS );

            IDE_TEST( sdpPhyPage::freeSlot(sPage,
                                           sSeq,
                                           sKeyLen,
                                           1 )
                      != IDE_SUCCESS );
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}


/*********************************************************************
 * FUNCTION DESCRIPTION : sdnbBTree::unlinkNode                      *
 * ------------------------------------------------------------------*
 * index leaf node�� free�Ҷ� ��,�� ������ ������ ���´�.          *
 *********************************************************************/
IDE_RC sdnbBTree::unlinkNode( sdrMtx *        aMtx,
                              sdnbHeader *    aIndex,
                              sdpPhyPageHdr * aNode,
                              ULong           aSmoNo )
{

    scPageID            sPrevPID;
    scPageID            sNextPID;
    scPageID            sTSID;
    sdpPhyPageHdr     * sPrevNode;
    sdpPhyPageHdr     * sNextNode;

    sPrevPID = sdpPhyPage::getPrvPIDOfDblList(aNode);
    sNextPID = sdpPhyPage::getNxtPIDOfDblList(aNode);
    sTSID = aIndex->mIndexTSID;

    if ( sPrevPID != SD_NULL_PID )
    {
        sPrevNode = (sdpPhyPageHdr*)sdrMiniTrans::getPagePtrFromPageID(
                                                                aMtx, 
                                                                sTSID,
                                                                sPrevPID ); 
        if ( sPrevNode == NULL )
        {
            ideLog::log( IDE_ERR_0,
                         "TS ID : %"ID_UINT32_FMT""
                         "\nPrev PID : %"ID_UINT32_FMT", Next PID : %"ID_UINT32_FMT"\n",
                         sTSID, 
                         sPrevPID, 
                         sNextPID );
            dumpIndexNode( aNode );
            IDE_ASSERT_MSG( 0, "Error Ouccurs in DRDB Index ID : %"ID_UINT32_FMT, aIndex->mIndexID );
        }
        else
        {
            /* nothing to do */
        }

        /*
         * [BUG-29132] [SM] BTree Fetch�� SMO ���⿡ ������ �ֽ��ϴ�.
         */
        sdpPhyPage::setIndexSMONo( (UChar *)sPrevNode, aSmoNo );

        IDE_TEST( sdpDblPIDList::setNxtOfNode(
                                    sdpPhyPage::getDblPIDListNode(sPrevNode),
                                    sNextPID,
                                    aMtx ) != IDE_SUCCESS );
    
        /* BUG-30546 - [SM] BTree Index���� makeNextRowCacheBackward��
         *             split ������ ������� �߻��մϴ�. */
        IDE_TEST( sdpDblPIDList::setPrvOfNode(
                                    sdpPhyPage::getDblPIDListNode(aNode),
                                    SD_NULL_PID,
                                    aMtx ) != IDE_SUCCESS );
    }
    else
    {
        /* nothing to do */
    }

    if ( sNextPID != SD_NULL_PID )
    {
        sNextNode = (sdpPhyPageHdr*)sdrMiniTrans::getPagePtrFromPageID( aMtx, 
                                                                        sTSID, 
                                                                        sNextPID );
        if ( sNextNode == NULL )
        {
            ideLog::log( IDE_ERR_0,
                         "TS ID : %"ID_UINT32_FMT""
                         "\nPrev PID : %"ID_UINT32_FMT", Next PID : %"ID_UINT32_FMT"\n",
                         sTSID,
                         sPrevPID, 
                         sNextPID );
            dumpIndexNode( aNode );
            IDE_ASSERT( 0 );
        }
        else
        {
            /* nothing to do */
        }

        /*
         * [BUG-29132] [SM] BTree Fetch�� SMO ���⿡ ������ �ֽ��ϴ�.
         */
        sdpPhyPage::setIndexSMONo( (UChar *)sNextNode, aSmoNo );
    

        IDE_TEST( sdpDblPIDList::setPrvOfNode(
                                    sdpPhyPage::getDblPIDListNode(sNextNode),
                                    sPrevPID,
                                    aMtx ) != IDE_SUCCESS );

        /* BUG-30546 - [SM] BTree Index���� makeNextRowCacheBackward��
         *             split ������ ������� �߻��մϴ�. */
        IDE_TEST( sdpDblPIDList::setNxtOfNode(
                                    sdpPhyPage::getDblPIDListNode(aNode),
                                    SD_NULL_PID,
                                    aMtx ) != IDE_SUCCESS );
    }
    else
    {
        /* nothing to do */
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}


/*********************************************************************
 * FUNCTION DESCRIPTION : sdnbBTree::unsetRootNode                   *
 * ------------------------------------------------------------------*
 * ��Ÿ���������� Root Node���� link�� �����Ѵ�.                     *
 *********************************************************************/
IDE_RC sdnbBTree::unsetRootNode( idvSQL *        aStatistics,
                                 sdrMtx *        aMtx,
                                 sdnbHeader *    aIndex,
                                 sdnbStatistic * aIndexStat )
{
    IDE_TEST( backupRuntimeHeader( aMtx, aIndex ) != IDE_SUCCESS );

    aIndex->mRootNode = SD_NULL_PID;
    aIndex->mMinNode  = SD_NULL_PID;
    aIndex->mMaxNode  = SD_NULL_PID;

    IDE_TEST( setIndexMetaInfo( aStatistics,
                                aIndex,
                                aIndexStat,
                                aMtx,
                                &aIndex->mRootNode,
                                &aIndex->mMinNode,
                                &aIndex->mMaxNode,
                                NULL,   /* aFreeNodeHead */
                                NULL,   /* aFreeNodeCnt */
                                NULL,   /* aIsCreatedWithLogging */
                                NULL,   /* aIsConsistent */
                                NULL,   /* aIsCreatedWithForce */
                                NULL )  /* aNologgingCompletionLSN */
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}


/*********************************************************************
 * FUNCTION DESCRIPTION : sdnbBTree::deleteKey                       *
 * ------------------------------------------------------------------*
 * ������ Ű�� ã��, ã�� ��忡 Ű�� �ϳ��� �����Ѵٸ� Empty Node�� *
 * �����Ѵ�. ����� Node�� ������ ���������Ҷ� Insert Transaction��  *
 * ���ؼ� ����ȴ�.                                                *
 * �������굵 Ʈ����������� ����� ����(CTS)�� �ʿ��ϸ�, �̸� �Ҵ�  *
 * ������ ���� ��쿡�� �Ҵ������ ���������� ��ٷ��� �Ѵ�.         *
 *********************************************************************/
IDE_RC sdnbBTree::deleteKey( idvSQL      * aStatistics,
                             void        * aTrans,
                             void        * aIndex,
                             SChar       * aKeyValue,
                             smiIterator * /*aIterator*/,
                             sdSID         aRowSID )
{
    sdrMtx              sMtx;
    sdnbStack           sStack;
    sdpPhyPageHdr     * sLeafNode       = NULL;
    SShort              sLeafKeySeq     = -1;
    sdpPhyPageHdr     * sTargetNode;
    SShort              sTargetSlotSeq;
    scPageID            sEmptyNodePID[2]    = { SD_NULL_PID, SD_NULL_PID };
    UInt                sEmptyNodeCount     = 0;
    sdnbKeyInfo         sKeyInfo;
    idBool              sMtxStart   = ID_FALSE;
    smnIndexHeader    * sIndex;
    sdnbHeader        * sHeader;
    sdnbNodeHdr       * sNodeHdr;
    smLSN               sNTA;
    UShort              sKeySize;
    idBool              sIsSameKey  = ID_FALSE;
    idBool              sIsSuccess;
    idBool              sIsPessimistic  = ID_FALSE;
    SChar             * sOutBuffer4Dump;
    idvWeArgs           sWeArgs;
    UInt                sAllocPageCount = 0;
    UInt                sState          = 0;
    SLong               sTotalTraverseLength = 0;

    sIndex  = (smnIndexHeader*)aIndex;
    sHeader = (sdnbHeader*)sIndex->mHeader;

    // To fix BUG-17939
    IDE_DASSERT( sHeader->mIsConsistent == ID_TRUE );

    sKeyInfo.mKeyValue   = (UChar *)aKeyValue;
    sKeyInfo.mRowPID     = SD_MAKE_PID(aRowSID);
    sKeyInfo.mRowSlotNum = SD_MAKE_SLOTNUM(aRowSID);


    IDL_MEM_BARRIER;

    sNTA = smxTrans::getTransLstUndoNxtLSN( aTrans );

    IDE_TEST( sdrMiniTrans::begin( aStatistics,
                                   &sMtx,
                                   aTrans,
                                   SDR_MTX_LOGGING,
                                   ID_TRUE,/*Undoable(PROJ-2162)*/
                                   gMtxDLogType) != IDE_SUCCESS );
    sMtxStart = ID_TRUE;

    initStack( &sStack );

retraverse:

    sIsSuccess = ID_TRUE;

    IDE_TEST( traverse( aStatistics,
                        sHeader,
                        &sMtx,
                        &sKeyInfo,
                        &sStack,
                        &(sHeader->mDMLStat),
                        sIsPessimistic,
                        &sLeafNode,
                        &sLeafKeySeq,
                        NULL,     /* aLeafSP */
                        &sIsSameKey,
                        &sTotalTraverseLength )
              != IDE_SUCCESS );

    if ( ( sLeafNode == NULL ) ||
         ( sIsSameKey == ID_FALSE ) )
    {
        dumpHeadersAndIteratorToSMTrc( (smnIndexHeader*)aIndex,
                                       sHeader,
                                       NULL );

        if ( iduMemMgr::calloc( IDU_MEM_SM_SDN, 
                                1,
                                ID_SIZEOF( SChar ) * IDE_DUMP_DEST_LIMIT,
                                (void **)&sOutBuffer4Dump )
            == IDE_SUCCESS )
        {
            /* PROJ-2162 Restart Recovery Reduction
             * sKeyInfo ���Ŀ��� ����ó���Ǵ� ������ ����� */
            sState = 1;

            (void) dumpKeyInfo( &sKeyInfo, 
                                &(sHeader->mColLenInfoList),
                                sOutBuffer4Dump,
                                IDE_DUMP_DEST_LIMIT );
            (void) dumpStack( &sStack, 
                              sOutBuffer4Dump,
                              IDE_DUMP_DEST_LIMIT );
            ideLog::log( IDE_ERR_0, "%s\n", sOutBuffer4Dump );
            sState = 0;
            (void) iduMemMgr::free( sOutBuffer4Dump ) ;
        }
        else
        {
            /* nothing to do */
        }

        if ( sLeafNode != NULL )
        {
            dumpIndexNode( sLeafNode );
        }
        else
        {
            /* nothing to do */
        }

        IDE_ERROR( 0 );
    }

    IDE_TEST( deleteKeyFromLeafNode( aStatistics,
                                     &(sHeader->mDMLStat),
                                     &sMtx,
                                     sHeader,
                                     sLeafNode,
                                     &sLeafKeySeq,
                                     sIsPessimistic,
                                     &sIsSuccess )
              != IDE_SUCCESS );

    if ( sIsSuccess == ID_FALSE )
    {
        if ( sIsPessimistic == ID_FALSE )
        {
            sHeader->mDMLStat.mPessimisticCount++;

            sIsPessimistic = ID_TRUE;
            sMtxStart = ID_FALSE;
            IDE_TEST( sdrMiniTrans::commit(&sMtx) != IDE_SUCCESS );


            // BUG-32481 Disk index can't execute node aging if
            // index segment does not have free space.
            //
            // node aging�� ���� �����Ͽ� Free Node��
            // Ȯ���Ѵ�. split�� �ʿ��� �ִ� ������ �� ��ŭ Ȯ����
            // �õ��Ѵ�. root���� split�ȴٰ� �����ϸ� (depth + 1)
            // ��ŭ Ȯ���ϸ� �ȴ�.
            IDE_TEST( nodeAging( aStatistics,
                                 aTrans,
                                 &(sHeader->mDMLStat),
                                 (smnIndexHeader*)aIndex,
                                 sHeader,
                                 (sStack.mIndexDepth + 1) )
                      != IDE_SUCCESS );

            IDE_TEST( sdrMiniTrans::begin( aStatistics,
                                           &sMtx,
                                           aTrans,
                                           SDR_MTX_LOGGING,
                                           ID_TRUE,/*Undoable(PROJ-2162)*/
                                           gMtxDLogType ) != IDE_SUCCESS );
            sMtxStart = ID_TRUE;

            IDV_WEARGS_SET( &sWeArgs,
                            IDV_WAIT_INDEX_LATCH_DRDB_BTREE_INDEX_SMO,
                            0,   // WaitParam1
                            0,   // WaitParam2
                            0 ); // WaitParam3

            IDE_TEST( sHeader->mLatch.lockWrite( aStatistics,
                                                 &sWeArgs ) 
                      != IDE_SUCCESS );

            IDE_TEST( sdrMiniTrans::push( &sMtx,
                                          (void *)&sHeader->mLatch,
                                          SDR_MTX_LATCH_X )
                      != IDE_SUCCESS );

            initStack( &sStack );
            sHeader->mDMLStat.mRetraverseCount++;

            goto retraverse;
        }
        else
        {
            sKeySize = getKeyValueLength( &(sHeader->mColLenInfoList),
                                          (UChar *)aKeyValue);

            IDE_TEST( processLeafFull( aStatistics,
                                       &(sHeader->mDMLStat),
                                       &sMtx,
                                       &sMtxStart,
                                       (smnIndexHeader*)aIndex,
                                       sHeader,
                                       &sStack,
                                       sLeafNode,
                                       &sKeyInfo,
                                       sKeySize,
                                       sLeafKeySeq,
                                       &sTargetNode,
                                       &sTargetSlotSeq,
                                       &sAllocPageCount,
                                       ID_TRUE ) /* delete key */
                      != IDE_SUCCESS );

            /*
             * Split ���� TargetNode�� ������ ������ ������ �Ѵٸ�
             * ���� ��忡 ������ ������ �����Ѵ�.
             */
            findTargetKeyForDeleteKey( &sMtx,
                                       sHeader,
                                       &sTargetNode,
                                       &sTargetSlotSeq );

            IDE_TEST( isSameRowAndKey( sHeader,
                                       &(sHeader->mDMLStat),
                                       &sKeyInfo,
                                       sTargetNode,
                                       sTargetSlotSeq,
                                       &sIsSameKey )
                      != IDE_SUCCESS );
            IDE_DASSERT( sIsSameKey == ID_TRUE );

            IDE_TEST( deleteKeyFromLeafNode( aStatistics,
                                             &(sHeader->mDMLStat),
                                             &sMtx,
                                             sHeader,
                                             sTargetNode,
                                             &sTargetSlotSeq,
                                             sIsPessimistic,
                                             &sIsSuccess )
                      != IDE_SUCCESS );

            if ( sIsSuccess != ID_TRUE )
            {
                dumpHeadersAndIteratorToSMTrc( sIndex,
                                               sHeader,
                                               NULL );
                ideLog::log( IDE_ERR_0,
                             "Target slot sequence : %"ID_INT32_FMT"\n",
                             sTargetSlotSeq );
                dumpIndexNode( sTargetNode );
                IDE_ASSERT( 0 );
            }
            else
            {
                /* nothing to do */
            }

            /*
             * Split ���Ŀ� Empty Node�� ���� �ȵ� ��������
             * ����� ������, Empty Node Link�� Fail�� �߻��Ҽ� �ֱ�
             * ������ �̴� Ʈ����� Commit ���Ŀ� �����Ѵ�.
             */
            findEmptyNodes( &sMtx,
                            sHeader,
                            sLeafNode,
                            sEmptyNodePID,
                            &sEmptyNodeCount );

            sLeafNode = sTargetNode;
            sLeafKeySeq = sTargetSlotSeq;

            sHeader->mDMLStat.mNodeSplitCount++;
            // sHeader->mSmoNo++;
            increaseSmoNo( sHeader );
        }
    }
    else
    {
        IDE_DASSERT( validateNodeSpace( sHeader,
                                        sLeafNode,
                                        ID_TRUE /* aIsLeaf */ )
                     == IDE_SUCCESS );

        IDE_DASSERT( sEmptyNodeCount == 0 );
        sNodeHdr = (sdnbNodeHdr*)sdpPhyPage::getLogicalHdrStartPtr( (UChar *)sLeafNode );

        if ( sNodeHdr->mUnlimitedKeyCount == 0 )
        {
            sEmptyNodePID[0] = sLeafNode->mPageID;
            sEmptyNodeCount  = 1;
        }
        else
        {
            /* nothing to do */
        }
    }

    if ( needToUpdateStat() == ID_TRUE )
    {
        IDE_TEST( adjustNumDistStat( sIndex,
                                     sHeader,
                                     &sMtx,
                                     sLeafNode,
                                     sLeafKeySeq,
                                     -1 )
                  != IDE_SUCCESS );
        /* BUG-32943 [sm-disk-index] After executing DELETE ROW, the KeyCount
         * of DRDB Index is not decreased  */
        idCore::acpAtomicDec64( (void *)&sIndex->mStat.mKeyCount );
    }
    else
    {
        /* nothing to do ... */
    }

    sdrMiniTrans::setRefNTA( &sMtx,
                             sHeader->mIndexTSID,
                             SDR_OP_SDN_DELETE_KEY_WITH_NTA,
                             &sNTA );

    sMtxStart = ID_FALSE;
    IDE_TEST( sdrMiniTrans::commit(&sMtx) != IDE_SUCCESS );

    /* BUG-28423 - [SM] sdbBufferPool::latchPage���� ASSERT�� ������
     * �����մϴ�. */

    /* FIT/ART/sm/Bugs/BUG-28423/BUG-28423.tc */
    IDU_FIT_POINT( "1.BUG-28423@sdnbBTree::deleteKey" );

    if ( sEmptyNodeCount > 0 )
    {
        IDE_TEST( linkEmptyNodes( aStatistics,
                                  aTrans,
                                  sHeader,
                                  &(sHeader->mDMLStat),
                                  sEmptyNodePID )
                  != IDE_SUCCESS );
    }
    else
    {
        /* nothing to do */
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if (sMtxStart == ID_TRUE)
    {
        (void)sdrMiniTrans::rollback(&sMtx);
    }
    else
    {
        /* nothing to do ... */
    }

    switch ( sState )
    {
        case 1:
            IDE_ASSERT( iduMemMgr::free( sOutBuffer4Dump ) == IDE_SUCCESS );
            sOutBuffer4Dump = NULL;
        default:
            break;
    }

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : sdnbBTree::findTargetKeyForDupKey          *
 * ------------------------------------------------------------------*
 * DupKey�� ���� Target Key�� �缳���Ѵ�.                            *
 * DupKey�� ���� Insertable Position�� DupKey�� ����Ű�� ������    *
 * �ִ�. �׷��� ���� TargetPage�� �������� �̵����Ѿ� �Ѵ�.      *
 *********************************************************************/
IDE_RC sdnbBTree::findTargetKeyForDupKey( sdrMtx         * aMtx,
                                          sdnbHeader     * aIndex,
                                          sdnbStatistic  * aIndexStat,
                                          sdnbKeyInfo    * aKeyInfo,
                                          sdpPhyPageHdr ** aTargetPage,
                                          SShort         * aTargetSlotSeq )
{
    UChar         * sSlotDirPtr;
    sdnbLKey      * sLeafKey;
    scPageID        sNxtPID;
    sdnbKeyInfo     sKeyInfo;
    idBool          sIsSameValue;
    sdpPhyPageHdr * sNxtPage;
    sdpPhyPageHdr * sCurPage;

    sCurPage = *aTargetPage;

    sSlotDirPtr = sdpPhyPage::getSlotDirStartPtr( (UChar *)sCurPage );

    if ( sdpSlotDirectory::getCount( sSlotDirPtr ) == *aTargetSlotSeq )
    {
        sNxtPID = sdpPhyPage::getNxtPIDOfDblList(sCurPage);
        IDE_DASSERT( sNxtPID != SD_NULL_PID );

        sNxtPage = (sdpPhyPageHdr*)sdrMiniTrans::getPagePtrFromPageID(
                                                       aMtx,
                                                       aIndex->mIndexTSID,
                                                       sNxtPID );
        if ( sNxtPage == NULL )
        {
            ideLog::log( IDE_ERR_0,
                         "index TSID : %"ID_UINT32_FMT""
                         ", get page ID : %"ID_UINT32_FMT""
                         "\n",
                         aIndex->mIndexTSID, sNxtPID );
            dumpHeadersAndIteratorToSMTrc( NULL,
                                           aIndex,
                                           NULL );
            dumpIndexNode( sCurPage );
            IDE_ASSERT( 0 );
        }
        else
        {
            /* nothing to do */
        }

        sSlotDirPtr = sdpPhyPage::getSlotDirStartPtr( (UChar *)sNxtPage );

        IDE_TEST( sdpSlotDirectory::getPagePtrFromSlotNum( sSlotDirPtr,
                                                           0,
                                                           (UChar **)&sLeafKey )
                  != IDE_SUCCESS );
        SDNB_LKEY_TO_KEYINFO( sLeafKey, sKeyInfo );

        sIsSameValue = ID_FALSE;
        if ( compareKeyAndKey( aIndexStat,
                               aIndex->mColumns,
                               aIndex->mColumnFence,
                               aKeyInfo,
                               &sKeyInfo,
                               SDNB_COMPARE_VALUE   |
                               SDNB_COMPARE_PID     |
                               SDNB_COMPARE_OFFSET,
                               &sIsSameValue ) == 0 )
        {
            *aTargetPage    = sNxtPage;
            *aTargetSlotSeq = 0;
        }
        else
        {
            /* nothing to do */
        }
    }
    else
    {
        /* nothing to do */
    }

    return IDE_SUCCESS;
    
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : sdnbBTree::findTargetKeyForDeleteKey       *
 * ------------------------------------------------------------------*
 * Key ������ Target Key�� �缳���Ѵ�.                               *
 * Key ������ Remove & Insert�� ó���Ǵ� ��찡 ������, �ش� ����  *
 * Insertable Position�� ������ Ű�� ����Ű�� ������ �ִ�.           *
 * ���� TargetPage�� �������� �̵����Ѿ� �Ѵ�.                   *
 *********************************************************************/
void sdnbBTree::findTargetKeyForDeleteKey( sdrMtx         * aMtx,
                                           sdnbHeader     * aIndex,
                                           sdpPhyPageHdr ** aTargetPage,
                                           SShort         * aTargetSlotSeq )
{
    UChar     * sSlotDirPtr;
    scPageID    sNextPID;
    sdpPhyPageHdr *sOrgPage;

    sSlotDirPtr = sdpPhyPage::getSlotDirStartPtr( (UChar *)*aTargetPage );

    if ( sdpSlotDirectory::getCount( sSlotDirPtr ) == *aTargetSlotSeq )
    {
        sNextPID = sdpPhyPage::getNxtPIDOfDblList(*aTargetPage);
        IDE_DASSERT( sNextPID != SD_NULL_PID );
        
        sOrgPage = *aTargetPage;

        *aTargetPage = (sdpPhyPageHdr*)sdrMiniTrans::getPagePtrFromPageID(
                                                       aMtx,
                                                       aIndex->mIndexTSID,
                                                       sNextPID );
        if ( *aTargetPage == NULL )
        {
            ideLog::log( IDE_ERR_0,
                         "index TSID : %"ID_UINT32_FMT""
                         ", get page ID : %"ID_UINT32_FMT""
                         "\n",
                         aIndex->mIndexTSID, sNextPID );
            dumpHeadersAndIteratorToSMTrc( NULL,
                                           aIndex,
                                           NULL );
            dumpIndexNode( sOrgPage );
            IDE_ASSERT( 0 );
        }
        else
        {
            /* nothing to do */
        }

        *aTargetSlotSeq = 0;
    }
    else
    {
        /* nothing to do */
    }
}

/*********************************************************************
 * FUNCTION DESCRIPTION : sdnbBTree::isSameRowAndKey                 *
 * ------------------------------------------------------------------*
 * ������� ���� �Լ��̸�, Ű�� Row�� Exact Matching�ϴ��� �˻��Ѵ�. *
 *********************************************************************/
IDE_RC sdnbBTree::isSameRowAndKey( sdnbHeader    * aIndex,
                                   sdnbStatistic * aIndexStat,
                                   sdnbKeyInfo   * aRowInfo,
                                   sdpPhyPageHdr * aLeafPage,
                                   SShort          aLeafKeySeq,
                                   idBool        * aIsSameKey )
{
    sdnbLKey    * sLeafKey;
    sdnbKeyInfo   sKeyInfo;
    idBool        sIsSameValue = ID_FALSE;
    idBool        sIsSameKey   = ID_FALSE;
    UChar       * sSlotDirPtr;
    SInt          sRet;

    sSlotDirPtr = sdpPhyPage::getSlotDirStartPtr( (UChar *)aLeafPage );
    IDE_TEST( sdpSlotDirectory::getPagePtrFromSlotNum( sSlotDirPtr,
                                                       aLeafKeySeq,
                                                       (UChar **)&sLeafKey )
              != IDE_SUCCESS );

    SDNB_LKEY_TO_KEYINFO( sLeafKey, sKeyInfo );

    sIsSameValue = ID_FALSE;
    sRet = compareKeyAndKey( aIndexStat,
                             aIndex->mColumns,
                             aIndex->mColumnFence,
                             aRowInfo,
                             &sKeyInfo,
                             ( SDNB_COMPARE_VALUE   |
                               SDNB_COMPARE_PID     |
                               SDNB_COMPARE_OFFSET ),
                             &sIsSameValue );

    if ( sRet == 0 )
    {
        sIsSameKey = ID_TRUE;
    }
    else
    {
        /* nothing to do */
    }

    *aIsSameKey = sIsSameKey; 

    return IDE_SUCCESS;
    
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : sdnbBTree::findEmptyNodes                  *
 * ------------------------------------------------------------------*
 * Unlimited Key�� 0�� ������ ã�´�.                              *
 * Split ���Ŀ� ȣ��Ǳ� ������ ����� ���� ��常�� �˻��Ѵ�.       *
 *********************************************************************/
void sdnbBTree::findEmptyNodes( sdrMtx        * aMtx,
                                sdnbHeader    * aIndex,
                                sdpPhyPageHdr * aStartPage,
                                scPageID      * aEmptyNodePID,
                                UInt          * aEmptyNodeCount )
{
    sdpPhyPageHdr * sCurPage;
    sdpPhyPageHdr * sPrevPage;
    sdnbNodeHdr   * sNodeHdr;
    UInt            sEmptyNodeSeq = 0;
    UInt            i;
    scPageID        sNextPID;

    aEmptyNodePID[0] = SD_NULL_PID;
    aEmptyNodePID[1] = SD_NULL_PID;

    sCurPage = aStartPage;

    for ( i = 0 ; i < 2 ; i++ )
    {
        if ( sCurPage == NULL )
        {
            if ( i == 1 )
            {
                ideLog::log( IDE_ERR_0,
                             "index TSID : %"ID_UINT32_FMT""
                             ", get page ID : %"ID_UINT32_FMT"\n",
                             aIndex->mIndexTSID, 
                             sNextPID );
                dumpHeadersAndIteratorToSMTrc( NULL,
                                               aIndex,
                                               NULL );
                dumpIndexNode( sPrevPage );
            }
            else
            {
                /* nothing to do */
            }
            IDE_ASSERT( 0 );
        }
        else
        {
            /* nothing to do */
        }

        sNodeHdr = (sdnbNodeHdr*)sdpPhyPage::getLogicalHdrStartPtr((UChar *)sCurPage);

        if ( sNodeHdr->mUnlimitedKeyCount == 0 )
        {
            aEmptyNodePID[sEmptyNodeSeq] = sCurPage->mPageID;
            sEmptyNodeSeq++;
        }
        else
        {
            /* nothing to do */
        }

        sNextPID = sdpPhyPage::getNxtPIDOfDblList( sCurPage );

        if ( sNextPID != SD_NULL_PID )
        {
            sPrevPage = sCurPage;
            sCurPage = (sdpPhyPageHdr*)
                sdrMiniTrans::getPagePtrFromPageID( aMtx,
                                                    aIndex->mIndexTSID,
                                                    sNextPID );
        }
        else
        {
            /* nothing to do */
        }
    }//for

    *aEmptyNodeCount = sEmptyNodeSeq;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : sdnbBTree::linkEmptyNodes                  *
 * ------------------------------------------------------------------*
 * �־��� Empty Node���� List�� �����Ѵ�.                            *
 * ���ü� ������ ������ ��ġ ȹ���� �����ߴٸ� ���� ��ٷȴٰ�       *
 * �ٽ� �õ��Ѵ�.                                                    *
 *********************************************************************/
IDE_RC sdnbBTree::linkEmptyNodes( idvSQL         * aStatistics,
                                  void           * aTrans,
                                  sdnbHeader     * aIndex,
                                  sdnbStatistic  * aIndexStat,
                                  scPageID       * aEmptyNodePID )
{
    sdrMtx          sMtx;
    idBool          sMtxStart = ID_FALSE;
    sdnbNodeHdr   * sNodeHdr;
    UInt            sEmptyNodeSeq;
    sdpPhyPageHdr * sPage;
    idBool          sIsSuccess;

    for ( sEmptyNodeSeq = 0 ; sEmptyNodeSeq < 2 ; )
    {
        if ( aEmptyNodePID[sEmptyNodeSeq] == SD_NULL_PID )
        {
            sEmptyNodeSeq++;
            continue;
        }
        else
        {
            /* nothing to do */
        }

        IDE_TEST( sdrMiniTrans::begin(aStatistics,
                                      &sMtx,
                                      aTrans,
                                      SDR_MTX_LOGGING,
                                      ID_TRUE,/*Undoable(PROJ-2162)*/
                                      gMtxDLogType) != IDE_SUCCESS );
        sMtxStart = ID_TRUE;

        IDE_TEST( sdnbBTree::getPage( aStatistics,
                                      &(aIndexStat->mIndexPage),
                                      aIndex->mIndexTSID,
                                      aEmptyNodePID[sEmptyNodeSeq],
                                      SDB_X_LATCH,
                                      SDB_WAIT_NORMAL,
                                      &sMtx,
                                      (UChar **)&sPage,
                                      &sIsSuccess )
                  != IDE_SUCCESS );

        sNodeHdr = (sdnbNodeHdr*)sdpPhyPage::getLogicalHdrStartPtr( (UChar *)sPage );

        if ( sNodeHdr->mUnlimitedKeyCount != 0 )
        {
            sMtxStart = ID_FALSE;
            IDE_TEST( sdrMiniTrans::commit( &sMtx ) != IDE_SUCCESS );

            sEmptyNodeSeq++;
            continue;
        }
        else
        {
            /* nothing to do */
        }

        IDE_TEST( linkEmptyNode( aStatistics,
                                 aIndexStat,
                                 aIndex,
                                 &sMtx,
                                 sPage,
                                 &sIsSuccess )
                  != IDE_SUCCESS );

        sMtxStart = ID_FALSE;
        IDE_TEST( sdrMiniTrans::commit( &sMtx ) != IDE_SUCCESS );

        if ( sIsSuccess == ID_TRUE )
        {
            sEmptyNodeSeq++;
        }
        else
        {
            /*
             * ���ü� ������ ������ ��ġ ȹ���� �����ߴٸ� ���� ��ٷȴٰ�
             * �ٽ� �õ��Ѵ�.
             */
            idlOS::thr_yield();
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if ( sMtxStart == ID_TRUE )
    {
        sMtxStart = ID_FALSE;
        IDE_ASSERT( sdrMiniTrans::rollback( &sMtx ) == IDE_SUCCESS );
    }

    return IDE_FAILURE;

}

/*********************************************************************
 * FUNCTION DESCRIPTION : sdnbBTree::insertKeyRollback               *
 * ------------------------------------------------------------------*
 * InsertKey�� Rollback�Ǵ� ��쿡 �α׿� ����Ͽ� ȣ��ȴ�.         *
 * �ѹ��� Ű�� ã��, ã�� ��忡 Ű�� �ϳ��� �����Ѵٸ� Empty Node�� *
 * �����Ѵ�. ����� Node�� ������ ���������Ҷ� Insert Transaction��  *
 * ���ؼ� ����ȴ�.                                                *
 * �ش翬���� Ʈ����������� �Ҵ��� �ʿ䰡 ����. ��, Rollback����    *
 * �̹� Ʈ������� �Ҵ� ���� ������ ����Ѵ�.                        *
 * ���� Duplicate Key�� �ѹ�Ǵ� ��쿡�� Old CTS�� Aging �Ǿ��ų�   *
 * ����� ��찡 ������ �ֱ� ������ �̷��� ��쿡�� CTS�� ���Ѵ�� *
 * �����ؾ� �ϸ�, �׷��� ���� ��쿡�� �α�� CTS#�� �����Ѵ�.       *
 *                                                                   *
 * BUG-40779 hp����� �����Ϸ� optimizing ����                       *
 * hp��񿡼� release��带 ���� �����Ϸ��� optimizing�� ����      *
 * non autocommit mode ���� rollback ����� sLeafKey �ǰ��� ������� *
 * �ʴ� ������ �ִ�. volatile �� ������ �����ϵ��� �Ѵ�.             *
 *********************************************************************/
IDE_RC sdnbBTree::insertKeyRollback( idvSQL  * aStatistics,
                                     void    * aMtx,
                                     void    * aIndex,
                                     SChar   * aKeyValue,
                                     sdSID     aRowSID,
                                     UChar   * aRollbackContext,
                                     idBool    aIsDupKey )
{
    sdnbStack             sStack;
    sdnbHeader          * sHeader;
    sdnbKeyInfo           sKeyInfo;
    sdnbRollbackContext * sRollbackContext;
    volatile sdnbLKey   * sLeafKey          = NULL;
    sdpPhyPageHdr       * sLeafNode         = NULL;
    sdnbNodeHdr         * sNodeHdr;
    UShort                sUnlimitedKeyCount;
    SShort                sLeafKeySeq       = -1;
    UShort                sTotalDeadKeySize = 0;
    UShort                sKeyOffset = 0;
    UChar               * sSlotDirPtr;
    sdrMtx              * sMtx;
    UChar                 sCurCreateCTS;
    idBool                sIsSuccess = ID_TRUE;
    idBool                sIsSameKey = ID_FALSE;
    UShort                sTotalTBKCount = 0;
    SChar               * sOutBuffer4Dump;

    sMtx    = (sdrMtx*)aMtx;
    sHeader = (sdnbHeader*)((smnIndexHeader*)aIndex)->mHeader;

    IDE_TEST_CONT( sHeader->mIsConsistent == ID_FALSE, SKIP_UNDO );

    sRollbackContext = (sdnbRollbackContext*)aRollbackContext;

    initStack( &sStack );

    sKeyInfo.mKeyValue   = (UChar *)aKeyValue;
    sKeyInfo.mRowPID     = SD_MAKE_PID( aRowSID );
    sKeyInfo.mRowSlotNum = SD_MAKE_SLOTNUM( aRowSID );

    IDU_FIT_POINT( "sdnbBTree::insertKeyRollback" );

retraverse:

    IDE_TEST( traverse( aStatistics,
                        sHeader,
                        sMtx,
                        &sKeyInfo,
                        &sStack,
                        &(sHeader->mDMLStat),
                        ID_FALSE,/* aPessimistic */
                        &sLeafNode,
                        &sLeafKeySeq,
                        NULL,    /* aLeafSP */
                        &sIsSameKey,
                        NULL     /* aTotalTraverseLength */)
              != IDE_SUCCESS );

    IDE_ERROR( sLeafNode != NULL );
    IDE_ERROR( sIsSameKey == ID_TRUE );

    sNodeHdr    = (sdnbNodeHdr*)sdpPhyPage::getLogicalHdrStartPtr((UChar *)sLeafNode);
    sSlotDirPtr = sdpPhyPage::getSlotDirStartPtr( (UChar *)sLeafNode );
    
    /* ���⼭ �����ϸ� undoable off �Լ��� �ѹ��̶� ������ ��������� ���� ó�� */
    IDE_TEST( sdpSlotDirectory::getPagePtrFromSlotNum( sSlotDirPtr,
                                                       sLeafKeySeq,
                                                       (UChar **)&sLeafKey )
              != IDE_SUCCESS );
    IDE_TEST( sdpSlotDirectory::getValue( sSlotDirPtr, sLeafKeySeq, &sKeyOffset )
              != IDE_SUCCESS );

    if ( sNodeHdr->mUnlimitedKeyCount == 1 )
    {
        IDE_TEST( linkEmptyNode( aStatistics,
                                 &(sHeader->mDMLStat),
                                 sHeader,
                                 sMtx,
                                 sLeafNode,
                                 &sIsSuccess )
                  != IDE_SUCCESS );

        if ( sIsSuccess == ID_FALSE )
        {
            IDE_TEST( sdrMiniTrans::commit( sMtx ) != IDE_SUCCESS );

            IDE_TEST( sdrMiniTrans::begin( aStatistics,
                                           sMtx,
                                           sdrMiniTrans::getTrans(sMtx),
                                           SDR_MTX_LOGGING,
                                           ID_FALSE,/*Undoable(PROJ-2162)*/
                                           gMtxDLogType ) != IDE_SUCCESS );

            initStack( &sStack );
            goto retraverse;
        }
        else
        {
            /* nothing to do */
        }
    }
    else
    {
        /* nothing to do */
    }

    sCurCreateCTS = SDNB_GET_CCTS_NO( sLeafKey );

    /*
     * Duplicated key�� ���� rollback�̶��
     */
    if ( aIsDupKey == ID_TRUE )
    {
        IDE_ERROR( SDNB_GET_TB_TYPE( sLeafKey ) == SDNB_KEY_TB_KEY );
        
        idlOS::memcpy( (void *)sLeafKey->mTxInfo,
                       sRollbackContext->mTxInfo,
                       ID_SIZEOF(UChar) * 2 );

        idlOS::memcpy( &((sdnbLKeyEx*)sLeafKey)->mTxInfoEx,
                       &((sdnbRollbackContextEx*)aRollbackContext)->mTxInfoEx,
                       ID_SIZEOF( sdnbLTxInfo ) );
        
        IDE_TEST( sdrMiniTrans::writeLogRec( sMtx,
                                             (UChar *)sLeafKey,
                                             (void *)sLeafKey,
                                             ID_SIZEOF(sdnbLKeyEx),
                                             SDR_SDP_BINARY )
                  != IDE_SUCCESS );

        IDE_CONT( SKIP_ADJUST_SPACE );
    }
    else
    {
        /* nothing to do */
    }

    sTotalDeadKeySize = sNodeHdr->mTotalDeadKeySize;
    sTotalDeadKeySize += getKeyLength( &(sHeader->mColLenInfoList),
                                       (UChar *)sLeafKey,
                                       ID_TRUE /* aIsLeaf */ );
    sTotalDeadKeySize += ID_SIZEOF( sdpSlotEntry );

    SDNB_SET_CCTS_NO( sLeafKey, SDN_CTS_INFINITE );
    SDNB_SET_STATE( sLeafKey, SDNB_KEY_DEAD );

    IDE_TEST(sdrMiniTrans::writeNBytes( sMtx,
                                        (UChar *)&sNodeHdr->mTotalDeadKeySize,
                                        (void *)&sTotalDeadKeySize,
                                        ID_SIZEOF(sNodeHdr->mTotalDeadKeySize) )
             != IDE_SUCCESS );

    IDE_TEST( sdrMiniTrans::writeNBytes( sMtx,
                                         (UChar *)sLeafKey->mTxInfo,
                                         (void *)sLeafKey->mTxInfo,
                                         ID_SIZEOF(UChar)*2 )
              != IDE_SUCCESS );

    if ( needToUpdateStat() == ID_TRUE )
    {
        IDE_TEST( adjustNumDistStat( (smnIndexHeader*)aIndex,
                                     sHeader,
                                     sMtx,
                                     sLeafNode,
                                     sLeafKeySeq,
                                     -1 ) != IDE_SUCCESS );

        /* BUG-32943 [sm-disk-index] After executing DELETE ROW, the KeyCount
         * of DRDB Index is not decreased  */
        idCore::acpAtomicDec64( 
            (void *)&((smnIndexHeader*)aIndex)->mStat.mKeyCount );
    }
    else
    {
        /* nothing to do ... */
    }


    IDE_EXCEPTION_CONT( SKIP_ADJUST_SPACE );

    sUnlimitedKeyCount = sNodeHdr->mUnlimitedKeyCount - 1;
    IDE_ERROR_MSG( sUnlimitedKeyCount <  sdpSlotDirectory::getCount( sSlotDirPtr ),
                   "sUnlimitedKeyCount : %"ID_UINT32_FMT,
                   sUnlimitedKeyCount );
    IDE_TEST( sdrMiniTrans::writeNBytes( sMtx,
                                         (UChar *)&(sNodeHdr->mUnlimitedKeyCount),
                                         (void *)&sUnlimitedKeyCount,
                                         ID_SIZEOF(UShort) )
              != IDE_SUCCESS );

    if ( ( SDNB_GET_STATE( sLeafKey ) == SDNB_KEY_DEAD ) &&
         ( SDNB_GET_TB_TYPE( sLeafKey ) == SDNB_KEY_TB_KEY ) )
    {
        sTotalTBKCount = sNodeHdr->mTBKCount - 1;
        IDE_TEST( sdrMiniTrans::writeNBytes( sMtx,
                                             (UChar *)&(sNodeHdr->mTBKCount),
                                             (void *)&sTotalTBKCount,
                                             ID_SIZEOF(UShort) )
                  != IDE_SUCCESS );
    }
    else
    {
        /* nothing to do */
    }

    if ( SDN_IS_VALID_CTS( sCurCreateCTS ) )
    {
        IDE_TEST( sdnIndexCTL::unbindCTS( sMtx,
                                          sLeafNode,
                                          sCurCreateCTS,
                                          sKeyOffset )
                  != IDE_SUCCESS );
    }
    else
    {
        /* nothing to do */
    }

    IDE_EXCEPTION_CONT( SKIP_UNDO );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    dumpHeadersAndIteratorToSMTrc( (smnIndexHeader*)aIndex,
                                   sHeader,
                                   NULL );

    if ( iduMemMgr::calloc( IDU_MEM_SM_SDN, 
                            1,
                            ID_SIZEOF( SChar ) * IDE_DUMP_DEST_LIMIT,
                            (void **)&sOutBuffer4Dump )
        == IDE_SUCCESS )
    {
        /* PROJ-2162 Restart Recovery Reduction
         * sKeyInfo ���Ŀ��� ����ó���Ǵ� ������ ����� */
        (void) dumpKeyInfo( &sKeyInfo, 
                            &(sHeader->mColLenInfoList),
                            sOutBuffer4Dump,
                            IDE_DUMP_DEST_LIMIT );
        (void) dumpStack( &sStack, 
                          sOutBuffer4Dump,
                          IDE_DUMP_DEST_LIMIT );
        ideLog::log( IDE_ERR_0, "%s\n", sOutBuffer4Dump );

        /* ���� Traverse �� key�� ã�Ҵµ� ������ ���� ���, �ش� Ű�� ����
         * ������ ����Ѵ�. */
        if ( sLeafKeySeq != -1 )
        {
            ideLog::log( IDE_ERR_0,
                         "Leaf key sequence : %"ID_INT32_FMT"\n",
                         sLeafKeySeq );

            if ( sLeafKey != NULL )
            {
                SDNB_LKEY_TO_KEYINFO( sLeafKey, sKeyInfo );
                (void) dumpKeyInfo( &sKeyInfo, 
                                    &(sHeader->mColLenInfoList),
                                    sOutBuffer4Dump,
                                    IDE_DUMP_DEST_LIMIT );
            }
        }

        (void) iduMemMgr::free( sOutBuffer4Dump ) ;
    }

    if ( sLeafNode != NULL )
    {
        dumpIndexNode( sLeafNode );
    }

    /* BUG-45460 ������ ���ܵ� rollback�� �����ؼ��� �ȵȴ�. */
    smnManager::setIsConsistentOfIndexHeader( aIndex, ID_FALSE );

    ideLog::log(IDE_SERVER_0, "Corrupted Index founded in UNDO Action\n"
                              "Table OID    : %"ID_vULONG_FMT"\n"
                              "Index ID     : %"ID_UINT32_FMT"\n"
                              "Please Rebuild Index\n",
                              sHeader->mTableOID, sHeader->mIndexID );

    return IDE_SUCCESS;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : sdnbBTree::deleteKeyRollback               *
 * ------------------------------------------------------------------*
 * DeleteKey�� Rollback�Ǵ� ��쿡 �α׿� ����Ͽ� ȣ��ȴ�.         *
 * �ѹ��� Ű�� ã��, ã�� Ű�� ���¸� STABLE�̳� UNSTABLE�� �����Ѵ�.*
 * �ش翬���� Ʈ����������� �Ҵ��� �ʿ䰡 ����. ��, Rollback����    *
 * �̹� Ʈ������� �Ҵ� ���� ������ ����Ѵ�.                        *
 *                                                                   *
 * BUG-40779 hp����� �����Ϸ� optimizing ����                       *
 * hp��񿡼� release��带 ���� �����Ϸ��� optimizing�� ����      *
 * non autocommit mode ���� rollback ����� sLeafKey �ǰ��� ������� *
 * �ʴ� ������ �ִ�. volatile �� ������ �����ϵ��� �Ѵ�.             *
 *********************************************************************/
IDE_RC sdnbBTree::deleteKeyRollback( idvSQL  * aStatistics,
                                     void    * aMtx,
                                     void    * aIndex,
                                     SChar   * aKeyValue,
                                     sdSID     aRowSID,
                                     UChar   * /*aRollbackContext*/ )
{
    sdnbStack             sStack;
    sdnbHeader          * sHeader;
    sdnbKeyInfo           sKeyInfo;
    volatile sdnbLKey   * sLeafKey    = NULL;
    sdpPhyPageHdr       * sLeafNode   = NULL;
    SShort                sLeafKeySeq = -1;
    sdnbNodeHdr         * sNodeHdr;
    UShort                sUnlimitedKeyCount;
    UChar                 sLimitCTS;
    smSCN                 sCommitSCN;
    UShort                sKeyOffset = 0;
    UChar               * sSlotDirPtr;
    sdrMtx              * sMtx;
    idBool                sIsSameKey = ID_FALSE;
    SChar               * sOutBuffer4Dump;

    sMtx    = (sdrMtx*)aMtx;
    sHeader = (sdnbHeader*)((smnIndexHeader*)aIndex)->mHeader;

    IDE_TEST_CONT( sHeader->mIsConsistent == ID_FALSE, SKIP_UNDO );

    initStack( &sStack );

    sKeyInfo.mKeyValue   = (UChar *)aKeyValue;
    sKeyInfo.mRowPID     = SD_MAKE_PID( aRowSID );
    sKeyInfo.mRowSlotNum = SD_MAKE_SLOTNUM( aRowSID );

    IDU_FIT_POINT( "sdnbBTree::deleteKeyRollback" );

    IDE_TEST( traverse( aStatistics,
                        sHeader,
                        sMtx,
                        &sKeyInfo,
                        &sStack,
                        &(sHeader->mDMLStat),
                        ID_FALSE,/* aPessimistic */
                        &sLeafNode,
                        &sLeafKeySeq,
                        NULL,    /* aLeafSP */
                        &sIsSameKey,
                        NULL     /* aTotalTraverseLength */ )
              != IDE_SUCCESS );

    IDE_ERROR( sLeafNode != NULL );
    IDE_ERROR( sIsSameKey == ID_TRUE );

    sNodeHdr    = (sdnbNodeHdr*)sdpPhyPage::getLogicalHdrStartPtr( (UChar *)sLeafNode );
    sSlotDirPtr = sdpPhyPage::getSlotDirStartPtr( (UChar *)sLeafNode );
    IDE_TEST( sdpSlotDirectory::getPagePtrFromSlotNum( sSlotDirPtr,
                                                       sLeafKeySeq,
                                                       (UChar **)&sLeafKey )
              != IDE_SUCCESS );
    IDE_TEST( sdpSlotDirectory::getValue( sSlotDirPtr, sLeafKeySeq, &sKeyOffset )
              != IDE_SUCCESS );
    sLimitCTS = SDNB_GET_LCTS_NO( sLeafKey );

    sUnlimitedKeyCount = sNodeHdr->mUnlimitedKeyCount + 1;
    IDE_ERROR_MSG( sUnlimitedKeyCount <=  sdpSlotDirectory::getCount( sSlotDirPtr ),
                   "sUnlimitedKeyCount : %"ID_UINT32_FMT,
                   sUnlimitedKeyCount );


    IDE_TEST( sdrMiniTrans::writeNBytes( sMtx,
                                         (UChar *)&(sNodeHdr->mUnlimitedKeyCount),
                                         (void *)&sUnlimitedKeyCount,
                                         ID_SIZEOF(UShort) )
              != IDE_SUCCESS );

    if ( SDN_IS_VALID_CTS( sLimitCTS ) )
    {
        IDE_TEST( sdnIndexCTL::unbindCTS( sMtx,
                                          sLeafNode,
                                          sLimitCTS,
                                          sKeyOffset )
                  != IDE_SUCCESS );
    }
    else
    {
        /* nothing to do */
    }

    SDNB_SET_LCTS_NO( sLeafKey, SDN_CTS_INFINITE );

    if ( SDNB_GET_CCTS_NO( sLeafKey ) == SDN_CTS_INFINITE )
    {
        SDNB_SET_STATE( sLeafKey, SDNB_KEY_STABLE );
    }
    else
    {
        if ( SDNB_GET_CCTS_NO( sLeafKey ) == SDN_CTS_IN_KEY )
        {
            IDE_TEST( getCommitSCNFromTBK( aStatistics,
                                           NULL, /* sdrMiniTrans::getTrans( sMtx ) */
                                           sLeafNode,
                                           (sdnbLKeyEx *)sLeafKey,
                                           ID_FALSE,    /* aIsLimitSCN */
                                           SM_SCN_INIT, /* aStmtViewSCN */
                                           &sCommitSCN )
                      != IDE_SUCCESS );

            if ( isAgableTBK( sCommitSCN ) == ID_TRUE )
            {
                SDNB_SET_STATE( sLeafKey, SDNB_KEY_STABLE );
            }
            else
            {
                SDNB_SET_STATE( sLeafKey, SDNB_KEY_UNSTABLE );
            }
        }
        else
        {
            SDNB_SET_STATE( sLeafKey, SDNB_KEY_UNSTABLE );
        }
    }

    IDE_TEST( sdrMiniTrans::writeNBytes( sMtx,
                                         (UChar *)sLeafKey->mTxInfo,
                                         (void *)sLeafKey->mTxInfo,
                                         ID_SIZEOF(UChar)*2 )
              != IDE_SUCCESS );

    if ( needToUpdateStat() == ID_TRUE )
    {
        IDE_TEST( adjustNumDistStat( (smnIndexHeader*)aIndex,
                                     sHeader,
                                     sMtx,
                                     sLeafNode,
                                     sLeafKeySeq,
                                     1 ) != IDE_SUCCESS );
        /* BUG-32943 [sm-disk-index] After executing DELETE ROW, the KeyCount
         * of DRDB Index is not decreased  */
        idCore::acpAtomicInc64(
                          (void *)&((smnIndexHeader*)aIndex)->mStat.mKeyCount );
    }
    else
    {
        /* nothing to do ... */
    }

    IDE_EXCEPTION_CONT( SKIP_UNDO );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    dumpHeadersAndIteratorToSMTrc( (smnIndexHeader*)aIndex,
                                   sHeader,
                                   NULL );

    if ( iduMemMgr::calloc( IDU_MEM_SM_SDN, 
                            1,
                            ID_SIZEOF( SChar ) * IDE_DUMP_DEST_LIMIT,
                            (void **)&sOutBuffer4Dump )
         == IDE_SUCCESS )
    {
        /* PROJ-2162 Restart Recovery Reduction
         * sKeyInfo ���Ŀ��� ����ó���Ǵ� ������ ����� */
        
        (void) dumpKeyInfo( &sKeyInfo, 
                            &(sHeader->mColLenInfoList),
                            sOutBuffer4Dump,
                            IDE_DUMP_DEST_LIMIT );
        (void) dumpStack( &sStack, 
                          sOutBuffer4Dump,
                          IDE_DUMP_DEST_LIMIT );
        ideLog::log( IDE_ERR_0, "%s\n", sOutBuffer4Dump );

        /* ���� Traverse �� key�� ã�Ҵµ� ������ ���� ���, �ش� Ű�� ����
         * ������ ����Ѵ�. */
        if ( sLeafKeySeq != -1 )
        {
            ideLog::log( IDE_ERR_0,
                         "Leaf key sequence : %"ID_INT32_FMT"\n",
                         sLeafKeySeq );

            if ( sLeafKey != NULL )
            {
                SDNB_LKEY_TO_KEYINFO( sLeafKey, sKeyInfo );
                (void) dumpKeyInfo( &sKeyInfo, 
                                    &(sHeader->mColLenInfoList),
                                    sOutBuffer4Dump,
                                    IDE_DUMP_DEST_LIMIT );
            }
        }

        (void) iduMemMgr::free( sOutBuffer4Dump ) ;
    }

    if ( sLeafNode != NULL )
    {
        dumpIndexNode( sLeafNode );
    }

    /* BUG-45460 ������ ���ܵ� rollback�� �����ؼ��� �ȵȴ�. */
    smnManager::setIsConsistentOfIndexHeader( aIndex, ID_FALSE );

    ideLog::log(IDE_SERVER_0, "Corrupted Index founded in UNDO Action\n"
                              "Table OID    : %"ID_vULONG_FMT"\n"
                              "Index ID     : %"ID_UINT32_FMT"\n"
                              "Please Rebuild Index\n",
                              sHeader->mTableOID, sHeader->mIndexID );

    return IDE_SUCCESS;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : sdnbBTree::nodeAging                       *
 * ------------------------------------------------------------------*
 * ���� EMPTY LIST�� link�� ���Ŀ�, �ٸ� Ʈ����ǿ� ���ؼ� Ű�� ���� *
 * �� ��쿡�� EMPTY LIST���� �ش� ��带 �����Ѵ�.                  *
 * Node������ ���̻� �ش� ��忡 Ű�� ������ �����ؾ� �Ѵ�. ��, ��� *
 * CTS�� DEAD���¸� �����ؾ� �Ѵ�. ����, Hard Key Stamping�� ����  *
 * ��� CTS�� DEAD�� �ɼ� �ֵ��� �õ��Ѵ�.                           *
 *********************************************************************/
IDE_RC sdnbBTree::nodeAging( idvSQL         * aStatistics,
                             void           * aTrans,
                             sdnbStatistic  * aIndexStat,
                             smnIndexHeader * aPersIndex,
                             sdnbHeader     * aIndex,
                             UInt             aFreePageCount )
{
    sdnCTL        * sCTL;
    sdnCTS        * sCTS;
    UInt            i;
    UChar           sAgedCount = 0;
    scPageID        sFreeNode;
    sdpPhyPageHdr * sPage;
    UChar           sDeadCTSlotCount = 0;
    sdrMtx          sMtx;
    sdnbKeyInfo     sKeyInfo;
    ULong           sTempKeyBuf[SDNB_MAX_KEY_BUFFER_SIZE/ID_SIZEOF(ULong)];
    sdnbLKey      * sLeafKey;
    UShort          sKeyLength;
    UInt            sMtxStart = 0;
    UInt            sFreePageCount = 0;
    idBool          sSuccess = ID_FALSE;
    sdnbNodeHdr   * sNodeHdr;
    idvWeArgs       sWeArgs;
    UChar         * sSlotDirPtr;

    /*
     * Link�� ȹ���� ���Ŀ� �ٸ� Ʈ����ǿ� ���ؼ� ������ ����
     * NodeAging�� ���� �ʴ´�.
     */
    IDE_TEST_CONT( aIndex->mEmptyNodeHead == SD_NULL_PID, SKIP_AGING );

    while ( 1 )
    {
        /*
         * [BUG-27780] [SM] Disk BTree Index���� Node Aging�ÿ� Hang��
         *             �ɸ��� �ֽ��ϴ�.
         */
        sDeadCTSlotCount = 0;

        sFreeNode = aIndex->mEmptyNodeHead;

        if ( sFreeNode == SD_NULL_PID )
        {
            break;
        }
        else
        {
            /* nothing to do */
        }

        IDE_TEST( sdrMiniTrans::begin(aStatistics,
                                      &sMtx,
                                      aTrans,
                                      SDR_MTX_LOGGING,
                                      ID_FALSE,/*Undoable(PROJ-2162)*/
                                      gMtxDLogType )
                  != IDE_SUCCESS );
        sMtxStart = 1;

        IDV_WEARGS_SET( &sWeArgs,
                        IDV_WAIT_INDEX_LATCH_DRDB_BTREE_INDEX_SMO,
                        0,   // WaitParam1
                        0,   // WaitParam2
                        0 ); // WaitParam3

        IDE_TEST( aIndex->mLatch.lockWrite( aStatistics,
                                       &sWeArgs ) != IDE_SUCCESS );

        IDE_TEST( sdrMiniTrans::push( &sMtx,
                                      (void *)&aIndex->mLatch,
                                      SDR_MTX_LATCH_X )
                  != IDE_SUCCESS );

        /*
         * ������ ������ sFreeNode�� �״�� ����ϸ� �ȵȴ�.
         * X Latch�� ȹ���� ���Ŀ� FreeNode�� �ٽ� Ȯ���ؾ� ��.
         */
        sFreeNode = aIndex->mEmptyNodeHead;

        if ( sFreeNode == SD_NULL_PID )
        {
            sMtxStart = 0;
            IDE_TEST( sdrMiniTrans::commit( &sMtx ) != IDE_SUCCESS );
            break;
        }
        else
        {
            /* nothing to do */
        }

        IDE_TEST( sdnbBTree::getPage( aStatistics,
                                      &(aIndexStat->mIndexPage),
                                      aIndex->mIndexTSID,
                                      sFreeNode,
                                      SDB_X_LATCH,
                                      SDB_WAIT_NORMAL,
                                      &sMtx,
                                      (UChar **)&sPage,
                                      &sSuccess )
                  != IDE_SUCCESS );

        sNodeHdr = (sdnbNodeHdr*)sdpPhyPage::getLogicalHdrStartPtr((UChar *)sPage);

        /*
         * EMPTY LIST�� �ִ� �ð��߿� �ٸ� Ʈ����ǿ� ���ؼ� Ű�� ���Ե� ��쿡��
         * EMPTY LIST���� unlink�Ѵ�.
         */
        if ( sNodeHdr->mUnlimitedKeyCount > 0 )
        {
            IDE_TEST( unlinkEmptyNode( aStatistics,
                                       aIndexStat,
                                       aIndex,
                                       &sMtx,
                                       sPage,
                                       SDNB_IN_USED )
                      != IDE_SUCCESS );

            sMtxStart = 0;
            IDE_TEST( sdrMiniTrans::commit( &sMtx ) != IDE_SUCCESS );

            continue;
        }
        else
        {
            /* nothing to do */
        }

        sSuccess = ID_TRUE;
        if ( sNodeHdr->mTBKCount > 0 )
        {
            IDE_TEST( agingAllTBK( aStatistics,
                                   aIndex,
                                   &sMtx,
                                   sPage,
                                   &sSuccess ) /* TBK KEY�鿡 ���� Aging�� �����Ѵ�. */
                      != IDE_SUCCESS );
        }
        else
        {
            /* nothing to do */
        }

        if ( sSuccess == ID_FALSE )
        {
            sMtxStart = 0;
            IDE_TEST( sdrMiniTrans::commit( &sMtx ) != IDE_SUCCESS );

            IDE_CONT( SKIP_AGING );
        }
        else
        {
            /* nothing to do */
        }

        /*
         * HardKeyStamping�� �ؼ� ��� CTS�� DEAD���¸� �����.
         */
        sCTL = sdnIndexCTL::getCTL( sPage );

        for ( i = 0 ; i < sdnIndexCTL::getCount( sCTL ) ; i++ )
        {
            sCTS = sdnIndexCTL::getCTS( sCTL, i );

            if ( sdnIndexCTL::getCTSlotState( sCTS ) != SDN_CTS_DEAD )
            {
                IDE_TEST( hardKeyStamping( aStatistics,
                                           aIndex,
                                           &sMtx,
                                           sPage,
                                           i,
                                           &sSuccess )
                          != IDE_SUCCESS );

                if ( sSuccess == ID_FALSE )
                {
                    sMtxStart = 0;
                    IDE_TEST( sdrMiniTrans::commit( &sMtx ) != IDE_SUCCESS );

                    IDE_CONT( SKIP_AGING );
                }
                else
                {
                    /* nothing to do */
                }
            }
            else
            {
                /* nothing to do */
            }

            sDeadCTSlotCount++;
        }//for

        /*
         * ��� CTS�� DEAD������ ���� ��带 �����Ҽ� �ִ� �����̴�.
         */
        if ( sDeadCTSlotCount == sdnIndexCTL::getCount( sCTL ) )
        {
            sSlotDirPtr = sdpPhyPage::getSlotDirStartPtr( (UChar *)sPage );
            IDE_TEST( sdpSlotDirectory::getPagePtrFromSlotNum( sSlotDirPtr, 
                                                               0,
                                                               (UChar **)&sLeafKey )
                      != IDE_SUCCESS );

            sKeyLength = getKeyLength( &(aIndex->mColLenInfoList),
                                       (UChar *)sLeafKey,
                                       ID_TRUE /* aIsLeaf */ );

            idlOS::memcpy( (UChar *)sTempKeyBuf, sLeafKey, sKeyLength );
            SDNB_LKEY_TO_KEYINFO( sLeafKey, sKeyInfo);
            sKeyInfo.mKeyValue = SDNB_LKEY_KEY_PTR( (UChar *)sTempKeyBuf );

            sMtxStart = 0;
            IDE_TEST( sdrMiniTrans::commit( &sMtx ) != IDE_SUCCESS );

            /*
             * ���������� Node�� �����Ѵ�.
             */
            IDE_TEST( freeNode( aStatistics,
                                aTrans,
                                aPersIndex,
                                aIndex,
                                sFreeNode,
                                &sKeyInfo,
                                aIndexStat,
                                &sFreePageCount ) != IDE_SUCCESS );
            sAgedCount += sFreePageCount;
        }
        else
        {
            sMtxStart = 0;
            IDE_TEST( sdrMiniTrans::commit( &sMtx ) != IDE_SUCCESS );
        }

        if ( sAgedCount >= aFreePageCount )
        {
            break;
        }
    }

    IDE_EXCEPTION_CONT( SKIP_AGING );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if ( sMtxStart == 1 )
    {
        sMtxStart = 0;
        IDE_ASSERT( sdrMiniTrans::rollback( &sMtx ) == IDE_SUCCESS );
    }

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : sdnbBTree::freeNode                        *
 * ------------------------------------------------------------------*
 * DEAD Key�߿� �ϳ��� �����ؼ� ������ ��带 ã�´�.                *
 * ã�� ��忡 Ű�� �ϳ��� �����Ѵٸ� EMPTY LIST�� �������Ŀ� �ٸ� *
 * Ʈ����ǿ� ���ؼ� Ű�� ���Ե� ����̱� ������, �̷��� ��쿡��    *
 * �ش� ��带 EMTPY LIST���� �����Ѵ�.                              *
 * �׷��� ���� ��쿡�� ��� ������ ���� SMO�� �����Ѵ�.             *
 * Free Node ���� EMPTY NODE�� FREE LIST�� �̵��Ǿ� ����ȴ�.      *
 *********************************************************************/
IDE_RC sdnbBTree::freeNode(idvSQL          * aStatistics,
                           void            * aTrans,
                           smnIndexHeader  * aPersIndex,
                           sdnbHeader      * aIndex,
                           scPageID          aFreeNode,
                           sdnbKeyInfo     * aKeyInfo,
                           sdnbStatistic   * aIndexStat,
                           UInt            * aFreePageCount )
{
    UChar         * sLeafNode;
    SShort          sLeafKeySeq;
    sdrMtx          sMtx;
    UInt            sMtxStart = 0;
    idvWeArgs       sWeArgs;
    scPageID        sSegPID;
    sdnbStack       sStack;
    sdnbNodeHdr   * sNodeHdr;
    scPageID        sPrevPID;
    scPageID        sNextPID;
    sdpPhyPageHdr * sPrevNode;
    sdpPhyPageHdr * sNextNode;
    ULong           sSmoNo;
    SLong           sTotalTraverseLength = 0;

    sSegPID = aIndex->mSegmentDesc.mSegHandle.mSegPID;

    initStack( &sStack );

    IDE_TEST( sdrMiniTrans::begin( aStatistics,
                                   &sMtx,
                                   aTrans,
                                   SDR_MTX_LOGGING,
                                   ID_FALSE,/*Undoable(PROJ-2162)*/
                                   gMtxDLogType )
              != IDE_SUCCESS );
    sMtxStart = 1;

    IDV_WEARGS_SET( &sWeArgs,
                    IDV_WAIT_INDEX_LATCH_DRDB_BTREE_INDEX_SMO,
                    0,   // WaitParam1
                    0,   // WaitParam2
                    0 ); // WaitParam3

    IDE_TEST( aIndex->mLatch.lockWrite( aStatistics,
                                   &sWeArgs ) != IDE_SUCCESS );

    IDE_TEST( sdrMiniTrans::push( &sMtx,
                                  (void *)&aIndex->mLatch,
                                  SDR_MTX_LATCH_X )
              != IDE_SUCCESS );

    /*
     * TREE LATCH�� ȹ���ϱ� ������ ������ aFreeNode�� ��ȿ���� �˻��Ѵ�.
     */
    IDE_TEST_CONT( aIndex->mEmptyNodeHead != aFreeNode, SKIP_UNLINK_NODE );

    IDE_TEST( traverse( aStatistics,
                        aIndex,
                        &sMtx,
                        aKeyInfo,
                        &sStack,
                        aIndexStat,
                        ID_TRUE, /* aIsPessimistic */
                        (sdpPhyPageHdr**)&sLeafNode,
                        &sLeafKeySeq,
                        NULL,  /* aLeafSP */
                        NULL,  /* aIsSameKey */
                        &sTotalTraverseLength )
              != IDE_SUCCESS );

    sNodeHdr = (sdnbNodeHdr*)sdpPhyPage::getLogicalHdrStartPtr( (UChar *)sLeafNode );

    if ( sNodeHdr->mUnlimitedKeyCount == 0 )
    {
        sPrevPID = sdpPhyPage::getPrvPIDOfDblList( (sdpPhyPageHdr*)sLeafNode );
        sNextPID = sdpPhyPage::getNxtPIDOfDblList( (sdpPhyPageHdr*)sLeafNode );

        getSmoNo( (void *)aIndex, &sSmoNo );
        
        if ( sStack.mIndexDepth > 0 )
        {
            sStack.mCurrentDepth = sStack.mIndexDepth - 1;

            IDE_TEST( deleteInternalKey ( aStatistics,
                                          aIndex,
                                          aIndexStat,
                                          sSegPID,
                                          &sMtx,
                                          &sStack,
                                          aFreePageCount ) != IDE_SUCCESS );

            IDE_TEST( unlinkNode( &sMtx,
                                  aIndex,
                                  (sdpPhyPageHdr*)sLeafNode,
                                  sSmoNo + 1 )
                      != IDE_SUCCESS );
        }
        else
        {
            unsetRootNode(aStatistics,
                          &sMtx,
                          aIndex,
                          aIndexStat);
        }

        sdpPhyPage::setIndexSMONo( (UChar *)sLeafNode, sSmoNo + 1 );

        IDE_TEST( unlinkEmptyNode( aStatistics,
                                   aIndexStat,
                                   aIndex,
                                   &sMtx,
                                   (sdpPhyPageHdr*)sLeafNode,
                                   SDNB_IN_FREE_LIST )
                  != IDE_SUCCESS );

        IDE_TEST( freePage( aStatistics,
                            aIndexStat,
                            aIndex,
                            &sMtx,
                            sLeafNode ) != IDE_SUCCESS );

        aIndexStat->mNodeDeleteCount++;
        // aIndex->mSmoNo++;
        increaseSmoNo( aIndex );
        (*aFreePageCount)++;

        // min stat�� �����Ѵ�.
        if ( sNextPID != SD_NULL_PID )
        {
            sNextNode = (sdpPhyPageHdr*)
                sdrMiniTrans::getPagePtrFromPageID( &sMtx,
                                                    aIndex->mIndexTSID,
                                                    sNextPID );
            if ( sNextNode == NULL )
            {
                ideLog::log( IDE_ERR_0,
                             "index TSID : %"ID_UINT32_FMT""
                             ", get page ID : %"ID_UINT32_FMT""
                             "\n",
                             aIndex->mIndexTSID, sNextPID );

                dumpHeadersAndIteratorToSMTrc( NULL,
                                               aIndex,
                                               NULL );

                dumpIndexNode( (sdpPhyPageHdr *)sLeafNode );
                IDE_ASSERT( 0 );
            }
            else
            {
                /* nothing to do */
            }
        }
        else
        {
            /* BUG-30605: Deleted/Dead�� Ű�� Min/Max�� ���Խ�ŵ�ϴ� */
            sNextNode = NULL;
        }

        if ( needToUpdateStat() == ID_TRUE )
        {
            IDE_TEST( adjustMinStat( aStatistics,
                                     aIndexStat,
                                     aPersIndex,
                                     aIndex,
                                     &sMtx,
                                     (sdpPhyPageHdr*)sLeafNode,
                                     sNextNode,
                                     ID_TRUE, /* aIsLoggingMeta */
                                     SMI_INDEX_BUILD_RT_STAT_UPDATE )
                      != IDE_SUCCESS );
        }
        else
        {
            /* nothing to do ... */
        }

        // max stat�� �����Ѵ�.
        if ( sPrevPID != SD_NULL_PID )
        {
            sPrevNode = (sdpPhyPageHdr*)
                sdrMiniTrans::getPagePtrFromPageID( &sMtx,
                                                    aIndex->mIndexTSID,
                                                    sPrevPID );
            if ( sPrevNode == NULL )
            {
                ideLog::log( IDE_ERR_0,
                             "index TSID : %"ID_UINT32_FMT""
                             ", get page ID : %"ID_UINT32_FMT""
                             "\n",
                             aIndex->mIndexTSID, sPrevPID );

                dumpHeadersAndIteratorToSMTrc( NULL,
                                               aIndex,
                                               NULL );

                dumpIndexNode( (sdpPhyPageHdr *)sLeafNode );
                IDE_ASSERT( 0 );
            }
            else
            {
                /* nothing to do */
            }
        }
        else
        {
            /* BUG-30605: Deleted/Dead�� Ű�� Min/Max�� ���Խ�ŵ�ϴ� */
            sPrevNode = NULL;
        }

        if ( needToUpdateStat() == ID_TRUE )
        {
            IDE_TEST( adjustMaxStat( aStatistics,
                                     aIndexStat,
                                     aPersIndex,
                                     aIndex,
                                     &sMtx,
                                     (sdpPhyPageHdr*)sLeafNode,
                                     sPrevNode,
                                     ID_TRUE, /* aIsLoggingMeta */
                                     SMI_INDEX_BUILD_RT_STAT_UPDATE )
                      != IDE_SUCCESS );
        }
        else
        {
            /* nothing to do ... */
        }
    }
    else
    {
        /*
         * EMPTY LIST�� �ִ� �ð��߿� �ٸ� Ʈ����ǿ� ���ؼ� Ű�� ���Ե�
         * ��쿡�� EMPTY LIST���� unlink�Ѵ�.
         */
        IDE_TEST( unlinkEmptyNode( aStatistics,
                                   aIndexStat,
                                   aIndex,
                                   &sMtx,
                                   (sdpPhyPageHdr*)sLeafNode,
                                   SDNB_IN_USED )
                  != IDE_SUCCESS );
    }

    IDE_EXCEPTION_CONT( SKIP_UNLINK_NODE );

    sMtxStart = 0;
    IDE_TEST( sdrMiniTrans::commit(&sMtx) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if ( sMtxStart == 1 )
    {
        IDE_ASSERT( sdrMiniTrans::rollback(&sMtx) == IDE_SUCCESS );
    }

    return IDE_FAILURE;
}


/*********************************************************************
 * FUNCTION DESCRIPTION : sdnbBTree::linkEmptyNode                   *
 * ------------------------------------------------------------------*
 * EMPTY LIST�� �ش� ��带 �����Ѵ�.                                *
 * �̹� EMPTY LIST�� FREE LIST�� ����Ǿ� �ִ� ����� SKIP�ϰ�,    *
 * �׷��� ���� ���� link�� �����Ѵ�.                               *
 *********************************************************************/
IDE_RC sdnbBTree::linkEmptyNode( idvSQL        * aStatistics,
                                 sdnbStatistic * aIndexStat,
                                 sdnbHeader    * aIndex,
                                 sdrMtx        * aMtx,
                                 sdpPhyPageHdr * aNode,
                                 idBool        * aIsSuccess )
{
    sdpPhyPageHdr * sTailPage;
    scPageID        sPageID;
    sdnbNodeHdr   * sCurNodeHdr;
    sdnbNodeHdr   * sTailNodeHdr;
    idBool          sIsSuccess;
    UChar           sNodeState;
    scPageID        sNullPID = SD_NULL_PID;
    UChar         * sMetaPage;

    *aIsSuccess = ID_TRUE;

    sPageID     = sdpPhyPage::getPageID((UChar *)aNode);
    sCurNodeHdr = (sdnbNodeHdr*)
                           sdpPhyPage::getLogicalHdrStartPtr((UChar *)aNode);

    IDE_TEST_CONT( ( sCurNodeHdr->mState == SDNB_IN_EMPTY_LIST ) ||
                   ( sCurNodeHdr->mState == SDNB_IN_FREE_LIST ),
                   SKIP_LINKING );

    /*
     * Index runtime Header�� empty node list������
     * Meta Page�� latch�� ���ؼ� ��ȣ�ȴ�.
     */
    sMetaPage = sdrMiniTrans::getPagePtrFromPageID(
                                     aMtx,
                                     aIndex->mIndexTSID,
                                     SD_MAKE_PID( aIndex->mMetaRID ));

    if ( sMetaPage == NULL )
    {
        IDE_TEST( sdnbBTree::getPage( aStatistics,
                                      &(aIndexStat->mMetaPage),
                                      aIndex->mIndexTSID,
                                      SD_MAKE_PID( aIndex->mMetaRID ),
                                      SDB_X_LATCH,
                                      SDB_WAIT_NORMAL,
                                      aMtx,
                                      (UChar **)&sMetaPage,
                                      &sIsSuccess )
                  != IDE_SUCCESS );
    }

    if ( aIndex->mEmptyNodeHead == SD_NULL_PID )
    {
        IDE_DASSERT( aIndex->mEmptyNodeTail == SD_NULL_PID );
        aIndex->mEmptyNodeHead =
        aIndex->mEmptyNodeTail = sPageID;

        IDE_TEST( setIndexEmptyNodeInfo( aStatistics,
                                         aIndex,
                                         aIndexStat,
                                         aMtx,
                                         &aIndex->mEmptyNodeHead,
                                         &aIndex->mEmptyNodeTail )
                  != IDE_SUCCESS );
    }
    else
    {
        IDE_DASSERT( aIndex->mEmptyNodeTail != SD_NULL_PID );

        /*
         * Deadlock�� ���ϱ� ���ؼ� tail page�� getPage�� try�Ѵ�.
         * ���� �����Ѵٸ� ��� ������ �ٽ� �����ؾ� �Ѵ�.
         */
        sTailPage = (sdpPhyPageHdr*)
            sdrMiniTrans::getPagePtrFromPageID( aMtx,
                                                aIndex->mIndexTSID,
                                                aIndex->mEmptyNodeTail);
        if ( sTailPage == NULL )
        {
            IDE_TEST( sdnbBTree::getPage( aStatistics,
                                          &(aIndexStat->mIndexPage),
                                          aIndex->mIndexTSID,
                                          aIndex->mEmptyNodeTail,
                                          SDB_X_LATCH,
                                          SDB_WAIT_NO,
                                          aMtx,
                                          (UChar **)&sTailPage,
                                          aIsSuccess) != IDE_SUCCESS );

            IDE_TEST_CONT( *aIsSuccess == ID_FALSE,
                            SKIP_LINKING );
        }
        else
        {
            /* nothing to do */
        }

        sTailNodeHdr = (sdnbNodeHdr*)
                          sdpPhyPage::getLogicalHdrStartPtr((UChar *)sTailPage);

        IDE_TEST( sdrMiniTrans::writeNBytes(
                                      aMtx,
                                      (UChar *)&sTailNodeHdr->mNextEmptyNode,
                                      (void *)&sPageID,
                                      ID_SIZEOF(sPageID) ) != IDE_SUCCESS );
        aIndex->mEmptyNodeTail = sPageID;

        IDE_TEST( setIndexEmptyNodeInfo( aStatistics,
                                         aIndex,
                                         aIndexStat,
                                         aMtx,
                                         NULL,
                                         &aIndex->mEmptyNodeTail )
                  != IDE_SUCCESS );
    }

    sNodeState = SDNB_IN_EMPTY_LIST;
    IDE_TEST( sdrMiniTrans::writeNBytes(
                                  aMtx,
                                  (UChar *)&(sCurNodeHdr->mState),
                                  (void *)&sNodeState,
                                  ID_SIZEOF(sNodeState) ) != IDE_SUCCESS );
    IDE_TEST( sdrMiniTrans::writeNBytes(
                                  aMtx,
                                  (UChar *)&(sCurNodeHdr->mNextEmptyNode),
                                  (void *)&sNullPID,
                                  ID_SIZEOF(sNullPID) ) != IDE_SUCCESS );


    IDE_EXCEPTION_CONT( SKIP_LINKING );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : sdnbBTree::unlinkEmptyNode                 *
 * ------------------------------------------------------------------*
 * EMPTY LIST���� �ش� ��带 �����Ѵ�.                              *
 *********************************************************************/
IDE_RC sdnbBTree::unlinkEmptyNode( idvSQL        * aStatistics,
                                   sdnbStatistic * aIndexStat,
                                   sdnbHeader    * aIndex,
                                   sdrMtx        * aMtx,
                                   sdpPhyPageHdr * aNode,
                                   UChar           aNodeState )
{
    sdnbNodeHdr   * sNodeHdr;
    UChar         * sMetaPage;
    idBool          sIsSuccess = ID_TRUE;

    sNodeHdr = (sdnbNodeHdr*)sdpPhyPage::getLogicalHdrStartPtr((UChar *)aNode);

    /*
     * FREE LIST�� �����ؾ� �ϴ� ��쿡�� �ݵ�� ���� ���°�
     * EMPTY LIST�� ����Ǿ� �ִ� ���¿��� �Ѵ�.
     */
    if ( ( aNodeState != SDNB_IN_USED ) &&
         ( sNodeHdr->mState != SDNB_IN_EMPTY_LIST ) )
    {
        ideLog::log( IDE_ERR_0,
                     "Node state : %"ID_UINT32_FMT""
                     "\nNode header state : %"ID_UINT32_FMT"\n",
                     aNodeState, sNodeHdr->mState );
        dumpIndexNode( aNode );
        IDE_ASSERT_MSG( 0, "Error Ouccurs in DRDB Index ID : %"ID_UINT32_FMT, aIndex->mIndexID );
    }
    else
    {
        /* nothing to do */
    }

    IDE_TEST_CONT( sNodeHdr->mState == SDNB_IN_USED,
                   SKIP_UNLINKING );

    if ( aIndex->mEmptyNodeHead == SD_NULL_PID )
    {
        dumpHeadersAndIteratorToSMTrc( NULL,
                                       aIndex,
                                       NULL );

        dumpIndexNode( aNode );
        IDE_ASSERT( 0 );
    }
    else
    {
        /* nothing to do */
    }

    /*
     * Index runtime Header�� empty node list������
     * Meta Page�� latch�� ���ؼ� ��ȣ�ȴ�.
     */
    sMetaPage = sdrMiniTrans::getPagePtrFromPageID(
                                             aMtx,
                                             aIndex->mIndexTSID,
                                             SD_MAKE_PID( aIndex->mMetaRID ));

    if ( sMetaPage == NULL )
    {
        IDE_TEST( sdnbBTree::getPage( aStatistics,
                                      &(aIndexStat->mMetaPage),
                                      aIndex->mIndexTSID,
                                      SD_MAKE_PID( aIndex->mMetaRID ),
                                      SDB_X_LATCH,
                                      SDB_WAIT_NORMAL,
                                      aMtx,
                                      (UChar **)&sMetaPage,
                                      &sIsSuccess )
                  != IDE_SUCCESS );
    }
    else
    {
        /* nothing to do */
    }

    aIndex->mEmptyNodeHead = sNodeHdr->mNextEmptyNode;

    if ( sNodeHdr->mNextEmptyNode == SD_NULL_PID )
    {
        aIndex->mEmptyNodeHead = SD_NULL_PID;
        aIndex->mEmptyNodeTail = SD_NULL_PID;

        IDE_TEST( setIndexEmptyNodeInfo( aStatistics,
                                         aIndex,
                                         aIndexStat,
                                         aMtx,
                                         &aIndex->mEmptyNodeHead,
                                         &aIndex->mEmptyNodeTail )
                  != IDE_SUCCESS );
    }
    else
    {
        aIndex->mEmptyNodeHead = sNodeHdr->mNextEmptyNode;

        IDE_TEST( setIndexEmptyNodeInfo( aStatistics,
                                         aIndex,
                                         aIndexStat,
                                         aMtx,
                                         &aIndex->mEmptyNodeHead,
                                         NULL )
                  != IDE_SUCCESS );
    }

    IDE_TEST( sdrMiniTrans::writeNBytes( aMtx,
                                         (UChar *)&(sNodeHdr->mState),
                                         (void *)&aNodeState,
                                         ID_SIZEOF(aNodeState) )
              != IDE_SUCCESS );

    IDE_EXCEPTION_CONT( SKIP_UNLINKING );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : sdnbBTree::agingInternal                   *
 * ------------------------------------------------------------------*
 * ����ڰ� ���������� aging��ų�� ȣ��Ǵ� �Լ��̴�.
 * ���� �α׸� ������ �ʱ� ���ؼ� Compaction�� ���� �ʴ´�.
 * 
 * BUG-31372: ���׸�Ʈ �ǻ��� ������ ��ȸ�� ����� �ʿ��մϴ�.
 * 1. MPR�� �������� ��ĵ�ϸ� Free Size�� ���ϰ�,
 *    Leaf Node�� ���ؼ��� DelayedStamping�� �Ѵ�.
 * 2. Node Aging�� �����Ѵ�.
 *********************************************************************/
IDE_RC sdnbBTree::aging( idvSQL         * aStatistics,
                         void           * aTrans,
                         smcTableHeader * /* aHeader */,
                         smnIndexHeader * aIndex )
{
    sdnbHeader      * sIdxHdr = NULL;
    sdpPhyPageHdr   * sPage = NULL;
    idBool            sIsSuccess;
    sdnbNodeHdr     * sNodeHdr = NULL;
    sdnCTS          * sCTS;
    sdnCTL          * sCTL;
    UInt              i;
    scPageID          sCurPID;
    smSCN             sCommitSCN;
    sdpSegCCache    * sSegCache;
    sdpSegHandle    * sSegHandle;
    sdpSegMgmtOp    * sSegMgmtOp;
    sdpSegInfo        sSegInfo;
    ULong             sUsedSegSizeByBytes = 0;
    UInt              sMetaSize = 0;
    UInt              sFreeSize = 0;
    UInt              sUsedSize = 0;
    sdbMPRMgr         sMPRMgr;
    UInt              sState   = 0;

    IDE_ASSERT( aIndex != NULL );

    sIdxHdr = (sdnbHeader*)(aIndex->mHeader);

    IDE_TEST( smLayerCallback::rebuildMinViewSCN( aStatistics )
              != IDE_SUCCESS );

    sSegHandle = sdpSegDescMgr::getSegHandle( &(sIdxHdr->mSegmentDesc) );
    sSegCache  = (sdpSegCCache*)sSegHandle->mCache;
    sSegMgmtOp = sdpSegDescMgr::getSegMgmtOp( &(sIdxHdr->mSegmentDesc) );
    // codesonar::Null Pointer Dereference
    IDE_ERROR( sSegMgmtOp != NULL );
 
    IDE_TEST( sSegMgmtOp->mGetSegInfo(
                             NULL,
                             sIdxHdr->mIndexTSID,
                             sdpSegDescMgr::getSegPID( &sIdxHdr->mSegmentDesc ),
                             NULL, /* aTableHeader */
                             &sSegInfo )
               != IDE_SUCCESS );

    IDE_TEST( sMPRMgr.initialize(
                          aStatistics,
                          sIdxHdr->mIndexTSID,
                          sdpSegDescMgr::getSegHandle( &(sIdxHdr->mSegmentDesc) ),
                          NULL ) /* Filter */
              != IDE_SUCCESS );
    sState = 1;

    sCurPID = SM_NULL_PID;

    IDE_TEST( sMPRMgr.beforeFst() != IDE_SUCCESS );

    while ( 1 )
    {
        IDE_TEST( sMPRMgr.getNxtPageID( NULL, /*FilterData*/
                                        &sCurPID )
                  != IDE_SUCCESS );

        if ( sCurPID == SD_NULL_PID )
        {
            break;
        }
        else
        {
            /* nothing to do */
        }

        IDE_TEST( sdbBufferMgr::getPage( aStatistics,
                                         sIdxHdr->mIndexTSID,
                                         sCurPID,
                                         SDB_S_LATCH,
                                         SDB_WAIT_NORMAL,
                                         SDB_MULTI_PAGE_READ,
                                         (UChar **)&sPage,
                                         &sIsSuccess )
                  != IDE_SUCCESS );
        sState = 2;

        IDE_DASSERT( ( sPage->mPageType == SDP_PAGE_FORMAT )      ||
                     ( sPage->mPageType == SDP_PAGE_INDEX_BTREE ) ||
                     ( sPage->mPageType == SDP_PAGE_INDEX_META_BTREE ) );

        /*
         * BUG-32942 When executing rebuild Index stat, abnormally shutdown
         * 
         * Free pages should be skiped. Otherwise index can read
         * garbage value, and server is shutdown abnormally.
         */
        if ( ( sPage->mPageType != SDP_PAGE_INDEX_BTREE ) ||
             ( sSegMgmtOp->mIsFreePage((UChar *)sPage) == ID_TRUE ) )
        {
            sState = 1;
            IDE_TEST( sdbBufferMgr::releasePage( aStatistics,
                                                 (UChar *)sPage )
                      != IDE_SUCCESS );
            continue;
        }
        else 
        {   
            /* nothing to do */
        }

        sNodeHdr = (sdnbNodeHdr*)sdpPhyPage::getLogicalHdrStartPtr( (UChar *)sPage );
        
        if ( sNodeHdr->mState != SDNB_IN_FREE_LIST )
        {
            /*
             * 1. Delayed CTL Stamping
             */
            if ( SDNB_IS_LEAF_NODE( sNodeHdr ) == ID_TRUE )
            {
                sCTL = sdnIndexCTL::getCTL( sPage );

                for ( i = 0; i < sdnIndexCTL::getCount( sCTL ) ; i++ )
                {
                    sCTS = sdnIndexCTL::getCTS( sCTL, i );

                    if ( sCTS->mState == SDN_CTS_UNCOMMITTED )
                    {
                        IDE_TEST( sdnIndexCTL::delayedStamping(
                                                        aStatistics,
                                                        NULL,        /* aTrans */
                                                        sCTS,
                                                        SDB_MULTI_PAGE_READ,
                                                        SM_SCN_INIT, /* aStmtViewSCN */
                                                        &sCommitSCN,
                                                        &sIsSuccess ) 
                                  != IDE_SUCCESS );
                    }
                    else
                    {
                        /* nothing to do */
                    }
                }//for
            }
            else
            {
                /* nothing to do */
            }

            sMetaSize = ID_SIZEOF( sdpPhyPageHdr )
                        + idlOS::align8(ID_SIZEOF(sdnbNodeHdr))
                        + sdpPhyPage::getSizeOfCTL( sPage )
                        + ID_SIZEOF( sdpPageFooter )
                        + ID_SIZEOF( sdpSlotDirHdr );

            sFreeSize = sNodeHdr->mTotalDeadKeySize + sPage->mTotalFreeSize;
            sUsedSize = SD_PAGE_SIZE - ( sMetaSize + sFreeSize );
            sUsedSegSizeByBytes += sUsedSize;
            IDE_DASSERT ( sMetaSize + sFreeSize + sUsedSize == SD_PAGE_SIZE );

        }
        else
        {
            /* nothing to do */
        }
        
        sState = 1;
        IDE_TEST( sdbBufferMgr::releasePage( aStatistics,
                                             (UChar *)sPage )
                  != IDE_SUCCESS );
    }
    
    /* BUG-31372: ���׸�Ʈ �ǻ��� ������ ��ȸ�� ����� �ʿ��մϴ�. */
    sSegCache->mFreeSegSizeByBytes = (sSegInfo.mFmtPageCnt * SD_PAGE_SIZE) - sUsedSegSizeByBytes;

    sState = 0;
    IDE_TEST( sMPRMgr.destroy() != IDE_SUCCESS );

    /*
     * 3. Node Aging
     */
    IDE_TEST( nodeAging( aStatistics,
                         aTrans,
                         &(sIdxHdr->mDMLStat),
                         aIndex,
                         sIdxHdr,
                         ID_UINT_MAX ) // aFreePageCount
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    switch( sState )
    {
        case 2:
            IDE_ASSERT( sdbBufferMgr::releasePage( aStatistics,
                                                   (UChar *)sPage )
                        == IDE_SUCCESS );
        case 1:
            IDE_ASSERT( sMPRMgr.destroy() == IDE_SUCCESS );
        default:
            break;
    }

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : sdnbBTree::gatherStat                      *
 * ------------------------------------------------------------------*
 * index�� ��������� �����Ѵ�.
 * 
 * BUG-32468 There is the need for rebuilding index statistics
 *
 * Statistics    - [IN]  IDLayer �������
 * Trans         - [IN]  �� �۾��� ��û�� Transaction
 * Percentage    - [IN]  Sampling Percentage
 * Degree        - [IN]  Parallel Degree
 * Header        - [IN]  ��� TableHeader
 * Index         - [IN]  ��� index
 *********************************************************************/
IDE_RC sdnbBTree::gatherStat( idvSQL         * aStatistics,
                              void           * aTrans,
                              SFloat           aPercentage,
                              SInt             aDegree,
                              smcTableHeader * aHeader,
                              void           * aTotalTableArg,
                              smnIndexHeader * aIndex,
                              void           * aStats,
                              idBool           aDynamicMode )
{
    sdnbHeader         * sIdxHdr  = NULL;
    sdpSegMgmtOp       * sSegMgmtOp;
    sdpSegInfo           sSegInfo;
    smuWorkerThreadMgr   sThreadMgr;
    sdnbStatArgument   * sStatArgument      = NULL;
    SFloat               sPercentage;
    smiIndexStat         sIndexStat;
    SLong                sClusteringFactor  = 0;
    SLong                sNumDist           = 0;
    SLong                sKeyCount          = 0;
    SLong                sMetaSpace         = 0;
    SLong                sUsedSpace         = 0;
    SLong                sAgableSpace       = 0;
    SLong                sFreeSpace         = 0;
    SLong                sAvgSlotCnt        = 0;
    UInt                 sAnalyzedPageCount = 0;
    UInt                 sSamplePageCount   = 0;
    UInt                 sState = 0;
    UInt                 i;
    smxTrans           * sSmxTrans = (smxTrans *)aTrans;
    UInt                 sStatFlag = SMI_INDEX_BUILD_RT_STAT_UPDATE;

    IDE_ASSERT( aIndex != NULL );
    IDE_ASSERT( aTotalTableArg == NULL );

    /* BUG-44794 �ε��� ����� �ε��� ��� ������ �������� �ʴ� ���� ������Ƽ �߰� */
    SMI_INDEX_BUILD_NEED_RT_STAT( sStatFlag, sSmxTrans );

    sIdxHdr = (sdnbHeader*)(aIndex->mHeader);
    idlOS::memset( &sIndexStat, 0, ID_SIZEOF( smiIndexStat ) );


    IDE_TEST( smLayerCallback::beginIndexStat( aHeader,
                                               aIndex,
                                               aDynamicMode )
              != IDE_SUCCESS );

    /******************************************************************
     * ������� �籸�� ����
     ******************************************************************/
    sSegMgmtOp = sdpSegDescMgr::getSegMgmtOp( &sIdxHdr->mSegmentDesc );
    // codesonar::Null Pointer Dereference
    IDE_ERROR( sSegMgmtOp != NULL );

    IDE_TEST( sSegMgmtOp->mGetSegInfo(
                            NULL,
                            sIdxHdr->mIndexTSID,
                            sdpSegDescMgr::getSegPID( &sIdxHdr->mSegmentDesc ),
                            NULL, /* aTableHeader */
                            &sSegInfo )
        != IDE_SUCCESS );

    /* Degree ��ŭ ���ķ� ������ */
    /* sdnbBTree_gatherStat_calloc_StatArgument.tc */
    IDU_FIT_POINT("sdnbBTree::gatherStat::calloc::StatArgument");
    IDE_TEST( iduMemMgr::calloc( IDU_MEM_SM_SDN, 
                                 1,
                                 (ULong)ID_SIZEOF( sdnbStatArgument ) * aDegree,
                                 (void **)&sStatArgument )
              != IDE_SUCCESS );
    sState = 1;

    IDE_TEST( smuWorkerThread::initialize( sdnbBTree::gatherStatParallel,
                                           aDegree,   /* Thread Count */
                                           aDegree,   /* Queue Size */
                                           &sThreadMgr )
              != IDE_SUCCESS );
    sState = 2;

    /* ���� �������ְ� �� �Ѱ��� */
    for ( i = 0 ; i < (UInt)aDegree ; i ++ )
    {
        sStatArgument[ i ].mStatistics              = aStatistics;
        sStatArgument[ i ].mFilter4Scan.mThreadCnt  = aDegree;
        sStatArgument[ i ].mFilter4Scan.mThreadID   = i;
        sStatArgument[ i ].mFilter4Scan.mPercentage = aPercentage;
        sStatArgument[ i ].mRuntimeIndexHeader      = sIdxHdr;

        IDE_TEST( smuWorkerThread::addJob( &sThreadMgr, 
                                           (void *)&sStatArgument[ i ] ) 
                  != IDE_SUCCESS );
    }

    sState = 1;
    IDE_TEST( smuWorkerThread::finalize( &sThreadMgr ) != IDE_SUCCESS );

    /* ������. */
    for ( i = 0 ; i < (UInt)aDegree ; i ++ )
    {
        sClusteringFactor  += sStatArgument[ i ].mClusteringFactor;
        sNumDist           += sStatArgument[ i ].mNumDist;
        sKeyCount          += sStatArgument[ i ].mKeyCount;
        sAnalyzedPageCount += sStatArgument[ i ].mAnalyzedPageCount;
        sSamplePageCount   += sStatArgument[ i ].mSampledPageCount;
        sMetaSpace         += sStatArgument[ i ].mMetaSpace;
        sUsedSpace         += sStatArgument[ i ].mUsedSpace;
        sAgableSpace       += sStatArgument[ i ].mAgableSpace;
        sFreeSpace         += sStatArgument[ i ].mFreeSpace;
    }

    if ( sAnalyzedPageCount == 0 )
    {
        /* ��ȸ����� ������ ���� �ʿ� ���� */
    }
    else
    {
        /* ������ �ʹ� ������ Extent��� ū ������ �������� Sampling�ϱ�
         * ������, Sampling ���� ũ�� �ٸ� �� ���� */
        if ( aPercentage < 0.2f )
        {
            sPercentage = sSamplePageCount / ((SFloat)sSegInfo.mFmtPageCnt);
        }
        else
        {
            sPercentage = aPercentage;
        }

        if ( sKeyCount <= sAnalyzedPageCount )
        {
            /* DeleteKey�� ���� ���, Page �ϳ��� Key �ϳ��� �ȵ� �� ����.
             * �� ��� �ܼ� ������ ��. */
            sClusteringFactor = (SLong)(sClusteringFactor / sPercentage);
            sNumDist          = (SLong)(sNumDist / sPercentage);
        }
        else
        {
            /* NumDist�� �����Ѵ�. ���� ���� �������� ���������� ��ģ
             * Ű ����� ������ �ʱ� ������, �� ���� �����Ѵ�.
             *
             * C     = ����Cardinaliy
             * K     = �м��� Key����
             * P     = �м��� Page����
             * (K-P) = �м��� Key���� ��
             * (K-1) = ���� Key���� ��
             *
             * C / ( K - P ) * ( K - 1 ) */
            sNumDist = (ULong)( ( ( (SFloat) sNumDist )
                                  / ( sKeyCount - sAnalyzedPageCount ) )
                                * ( sKeyCount - 1 ) / sPercentage ) + 1;
            sClusteringFactor = (ULong)
                                    ( ( ( (SFloat) sClusteringFactor )
                                        / ( sKeyCount - sAnalyzedPageCount ) )
                                      * ( sKeyCount - 1 ) / sPercentage );
        }
        sAvgSlotCnt  = sKeyCount / sAnalyzedPageCount;
        sMetaSpace   = (SLong)(sMetaSpace   / sPercentage);
        sUsedSpace   = (SLong)(sUsedSpace   / sPercentage);
        sAgableSpace = (SLong)(sAgableSpace / sPercentage);
        sFreeSpace   = (SLong)(sFreeSpace   / sPercentage);
        sKeyCount    = (SLong)(sKeyCount    / sPercentage);
    }
    sState = 0;
    IDE_TEST( iduMemMgr::free( sStatArgument ) != IDE_SUCCESS );

    IDE_TEST( gatherIndexHeight( aStatistics,
                                 aTrans,
                                 sIdxHdr,
                                 &sIndexStat.mIndexHeight )
              != IDE_SUCCESS );

    sIndexStat.mSampleSize        = aPercentage;
    sIndexStat.mNumPage           = sSegInfo.mFmtPageCnt;
    sIndexStat.mAvgSlotCnt        = sAvgSlotCnt;
    sIndexStat.mClusteringFactor  = sClusteringFactor;
    sIndexStat.mNumDist           = sNumDist; 
    sIndexStat.mKeyCount          = sKeyCount;
    sIndexStat.mMetaSpace         = sMetaSpace;
    sIndexStat.mUsedSpace         = sUsedSpace;
    sIndexStat.mAgableSpace       = sAgableSpace;
    sIndexStat.mFreeSpace         = sFreeSpace;

    IDE_TEST( smLayerCallback::setIndexStatWithoutMinMax( aIndex,
                                                          aTrans,
                                                          (smiIndexStat*)aStats,
                                                          &sIndexStat,
                                                          aDynamicMode,
                                                          sStatFlag )
              != IDE_SUCCESS );

    /* PROJ-2492 Dynamic sample selection */
    // ���̳��� ����϶� index �� min,max �� �������� �ʴ´�.
    if ( aDynamicMode == ID_FALSE )
    {
        IDE_TEST( rebuildMinStat( aStatistics,
                                  aTrans,
                                  aIndex,
                                  sIdxHdr )
                  != IDE_SUCCESS );

        IDE_TEST( rebuildMaxStat( aStatistics,
                                  aTrans,
                                  aIndex,
                                  sIdxHdr )
                  != IDE_SUCCESS );
    }
    else
    {
        // Nothing To Do
    }

    IDE_TEST( smLayerCallback::endIndexStat( aHeader,
                                             aIndex,
                                             aDynamicMode )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    switch( sState )
    {
    case 2:
        (void) smuWorkerThread::finalize( &sThreadMgr );
    case 1:
        (void) iduMemMgr::free( sStatArgument );
        break;
    default:
        break;
    }
    ideLog::log( IDE_SM_0,
                 "========================================\n"
                 " REBUILD INDEX STATISTICS: END(FAIL)    \n"
                 "========================================\n"
                 " Index Name: %s\n"
                 " Index ID:   %"ID_UINT32_FMT"\n"
                 "========================================\n",
                 aIndex->mName,
                 aIndex->mId );

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : sdnbBTree::gatherIndexHeight               *
 * ------------------------------------------------------------------*
 * Index�� ���̸� �����´�.                                          *
 *********************************************************************/
IDE_RC sdnbBTree::gatherIndexHeight( idvSQL        * aStatistics,
                                     void          * aTrans,
                                     sdnbHeader    * aIndex,
                                     SLong         * aIndexHeight )
{
    SLong                  sIndexHeight = 0;
    scPageID               sCurPID;
    UChar                * sPage;
    sdnbNodeHdr          * sNodeHdr;
    sdnbStatistic          sDummyStat;
    sdrMtx                 sMtx;
    UInt                   sMtxDLogType;
    idBool                 sTrySuccess = ID_FALSE;
    UInt                   sState = 0;

    IDE_DASSERT( aIndex != NULL );

    // BUG-27328 CodeSonar::Uninitialized Variable
    idlOS::memset( &sDummyStat, 0, ID_SIZEOF(sDummyStat) );

    if ( aTrans == NULL )
    {
        sMtxDLogType = SM_DLOG_ATTR_DUMMY;
    }
    else
    {
        sMtxDLogType = gMtxDLogType;
    }

    IDE_TEST( sdrMiniTrans::begin( aStatistics,
                                   &sMtx,
                                   aTrans,
                                   SDR_MTX_LOGGING,
                                   ID_FALSE,/*Undoable(PROJ-2162)*/
                                   sMtxDLogType )
              != IDE_SUCCESS );
    sState = 1;

    sCurPID = aIndex->mRootNode;

    while ( sCurPID != SC_NULL_PID )
    {
        sIndexHeight ++;

        /* BUG-30812 : Rebuild �� Stack Overflow�� �� �� �ֽ��ϴ�. */
        IDE_TEST( sdbBufferMgr::getPageByPID( aStatistics,
                                              aIndex->mIndexTSID,
                                              sCurPID,
                                              SDB_S_LATCH,
                                              SDB_WAIT_NORMAL,
                                              SDB_SINGLE_PAGE_READ,
                                              NULL, /* aMtx */
                                              (UChar **)&sPage,
                                              &sTrySuccess,
                                              NULL /* IsCorruptPage */ )
                  != IDE_SUCCESS );
        sState = 2;

        IDE_ASSERT_MSG( sTrySuccess == ID_TRUE,
                        "Error Ouccurs in DRDB Index ID : %"ID_UINT32_FMT, aIndex->mIndexID );

        sNodeHdr = (sdnbNodeHdr*)
                             sdpPhyPage::getLogicalHdrStartPtr((UChar *)sPage);

        if ( SDNB_IS_LEAF_NODE( sNodeHdr ) == ID_TRUE )
        {
            sState = 1;
            IDE_TEST( sdbBufferMgr::releasePage( aStatistics, sPage ) != IDE_SUCCESS );
            break;
        }
        else
        {
            /* Internal�̸�, LeftMost�� ��������. */
            sCurPID = sNodeHdr->mLeftmostChild;
        }
        sState = 1;
        IDE_TEST( sdbBufferMgr::releasePage( aStatistics, sPage ) != IDE_SUCCESS );
    }

    (*aIndexHeight) = sIndexHeight;

    sState = 0;
    IDE_TEST( sdrMiniTrans::commit( &sMtx ) != IDE_SUCCESS );
   
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    switch( sState )
    {
        case 2:
            (void)sdbBufferMgr::releasePage( aStatistics, sPage );

        case 1:
            (void)sdrMiniTrans::rollback( &sMtx );

        default:
            break;
    }

    return IDE_FAILURE;
}


/*********************************************************************
 * FUNCTION DESCRIPTION : sdnbBTree::gatherStatParallel              *
 * ------------------------------------------------------------------*
 * ���ķ� ��������� �����Ѵ�.
 * aJob      - [IN] ������ ��� ���� ������ ���� ����ü
 * *******************************************************************/
void sdnbBTree::gatherStatParallel( void   * aJob )
{
    sdnbStatArgument              * sStatArgument;
    sdnbHeader                    * sIdxHdr  = NULL;
    sdbMPRMgr                       sMPRMgr;
    idBool                          sMPRStart = ID_FALSE;
    idBool                          sGetState = ID_FALSE;
    sdbMPRFilter4SamplingAndPScan * sFilter4Scan;
    idBool                          sIsSuccess;
    sdpPhyPageHdr                 * sPage    = NULL;
    sdnbNodeHdr                   * sNodeHdr = NULL;
    scPageID                        sCurPID;
    sdpSegMgmtOp                  * sSegMgmtOp;
    UInt                            sMetaSpace   = 0;
    UInt                            sUsedSpace   = 0;
    UInt                            sFreeSpace   = 0;
#if defined(DEBUG)
    SChar                         * sTempBuf     = NULL; 
#endif
    sStatArgument = (sdnbStatArgument*)aJob;

    sIdxHdr      = sStatArgument->mRuntimeIndexHeader;
    sFilter4Scan = &sStatArgument->mFilter4Scan;

    sStatArgument->mClusteringFactor  = 0;
    sStatArgument->mNumDist           = 0;
    sStatArgument->mKeyCount          = 0;
    sStatArgument->mAnalyzedPageCount = 0;
    sStatArgument->mSampledPageCount  = 0;

    IDE_TEST( sMPRMgr.initialize(
                      sStatArgument->mStatistics,
                      sIdxHdr->mIndexTSID,
                      sdpSegDescMgr::getSegHandle( &(sIdxHdr->mSegmentDesc) ),
                      sdbMPRMgr::filter4SamplingAndPScan ) /* Filter */
              != IDE_SUCCESS );
    sMPRStart = ID_TRUE;

    sCurPID = SM_NULL_PID;

    IDE_TEST( sMPRMgr.beforeFst() != IDE_SUCCESS );

    while ( 1 )
    {
        IDE_TEST( sMPRMgr.getNxtPageID( (void *)sFilter4Scan,
                                        &sCurPID )
                  != IDE_SUCCESS );

        if ( sCurPID == SD_NULL_PID )
        {
            break;
        }
        else
        {
            /* nothing to do */
        }

        IDE_TEST( sdbBufferMgr::getPage( sStatArgument->mStatistics,
                                         sIdxHdr->mIndexTSID,
                                         sCurPID,
                                         SDB_S_LATCH,
                                         SDB_WAIT_NORMAL,
                                         SDB_MULTI_PAGE_READ,
                                         (UChar **)&sPage,
                                         &sIsSuccess )
                  != IDE_SUCCESS );
        /* Filter�Ǿ� Sample������� ������ ������ ���� */
        sStatArgument->mSampledPageCount ++;
        sGetState = ID_TRUE;

        IDE_DASSERT( ( sPage->mPageType == SDP_PAGE_FORMAT )      ||
                     ( sPage->mPageType == SDP_PAGE_INDEX_BTREE ) ||
                     ( sPage->mPageType == SDP_PAGE_INDEX_META_BTREE ) );

        if ( sPage->mPageType == SDP_PAGE_INDEX_BTREE )
        {
            sNodeHdr = (sdnbNodeHdr*) sdpPhyPage::getLogicalHdrStartPtr((UChar *)sPage);

            if ( SDNB_IS_LEAF_NODE( sNodeHdr ) == ID_TRUE )
            {
                sSegMgmtOp = sdpSegDescMgr::getSegMgmtOp( &(sIdxHdr->mSegmentDesc) );
                IDE_ERROR( sSegMgmtOp != NULL );
                /* Leaf Node */
                /* BUG-32942, BUG-42505  When executing rebuild Index stat, abnormally shutdown
                 * garbage value, and server is shutdown abnormally.*/
                if ( ( sSegMgmtOp->mIsFreePage((UChar *)sPage) != ID_TRUE ) &&
                     ( sNodeHdr->mState != SDNB_IN_INIT ) )
                {
                    IDE_TEST( analyzeNode4GatherStat( sStatArgument,
                                                      sIdxHdr,
                                                      sPage )
                              != IDE_SUCCESS );
                }
                else
                {
                    /* Index Build�� ���Ǿ��ٰ� Free�� Page.
                     * FormatPage�� ������ */
                    /* alloc �� �ǰ� logical hdr ���� �ʱ�ȭ �ȵ� page.
                       FormatPage�� ������ */
                    sStatArgument->mFreeSpace += 
                                sdpPhyPage::getTotalFreeSize( sPage );
                    sStatArgument->mMetaSpace += 
                                SD_PAGE_SIZE - sdpPhyPage::getTotalFreeSize( sPage );
#if defined(DEBUG)
                    if ( sNodeHdr->mState == SDNB_IN_INIT )
                    {

                        IDE_TEST( iduMemMgr::calloc( IDU_MEM_ID, 
                                                     1,
                                                     ID_SIZEOF( SChar ) * IDE_DUMP_DEST_LIMIT,
                                                     (void **)&sTempBuf )
                                  != IDE_SUCCESS );

                        if ( dumpNodeHdr( (UChar *)sPage, sTempBuf, IDE_DUMP_DEST_LIMIT )
                             == IDE_SUCCESS )
                        {
                            ideLog::log( IDE_SM_0, "IndexNodeHdr dump:\n" );
                            ideLog::log( IDE_SM_0, "%s\n", sTempBuf );
                        }
                        else
                        {
                            /* nothing to do */
                        }

                        IDE_TEST( iduMemMgr::free( sTempBuf ) != IDE_SUCCESS );
                    }
                    else
                    {
                        /* nothing to do */
                    }
#endif
                }
            }
            else
            {
                /* Internal Node */
                sMetaSpace = ID_SIZEOF( sdpPhyPageHdr )
                             + idlOS::align8(ID_SIZEOF(sdnbNodeHdr))
                             + sdpPhyPage::getSizeOfCTL( sPage )
                             + ID_SIZEOF( sdpSlotDirHdr )
                             + ID_SIZEOF( sdpPageFooter );
 
                sFreeSpace = sNodeHdr->mTotalDeadKeySize 
                             + sdpPhyPage::getTotalFreeSize( sPage );

                IDE_DASSERT( SD_PAGE_SIZE >= sMetaSpace + sFreeSpace );
                sUsedSpace = SD_PAGE_SIZE - (sMetaSpace + sFreeSpace) ;

                sStatArgument->mMetaSpace += sMetaSpace;
                sStatArgument->mUsedSpace += sUsedSpace;
                sStatArgument->mFreeSpace += sFreeSpace;

                IDE_DASSERT( sMetaSpace + sUsedSpace + sFreeSpace 
                             == SD_PAGE_SIZE );
         
            }
        }
        else
        {
            if ( sPage->mPageType == SDP_PAGE_FORMAT )
            {
                sStatArgument->mFreeSpace += 
                            sdpPhyPage::getTotalFreeSize( sPage );
                sStatArgument->mMetaSpace += 
                            SD_PAGE_SIZE - sdpPhyPage::getTotalFreeSize( sPage );
            }
            else
            {
                IDE_DASSERT( sPage->mPageType == SDP_PAGE_INDEX_META_BTREE )

                sStatArgument->mMetaSpace += SD_PAGE_SIZE;
            }
        }
        
        sGetState = ID_FALSE;
        IDE_TEST( sdbBufferMgr::releasePage( sStatArgument->mStatistics,
                                             (UChar *)sPage )
                  != IDE_SUCCESS );
    }
    
    sMPRStart = ID_FALSE;
    IDE_TEST( sMPRMgr.destroy() != IDE_SUCCESS );

    sStatArgument->mResult = IDE_SUCCESS;

    return;

    IDE_EXCEPTION_END;

    if ( sGetState == ID_TRUE )
    {
        IDE_ASSERT( sdbBufferMgr::releasePage( sStatArgument->mStatistics,
                                               (UChar *)sPage )
                    == IDE_SUCCESS );
    }

    if ( sMPRStart == ID_TRUE )
    {
        IDE_ASSERT( sMPRMgr.destroy() == IDE_SUCCESS );
    }

    sStatArgument->mResult = IDE_FAILURE;
    
    return;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : sdnbBTree::analyzeNode4GatherStat      
 * ------------------------------------------------------------------*
 * ������� ������ ���� ��带 �м��Ѵ�.
 * *******************************************************************/
IDE_RC sdnbBTree::analyzeNode4GatherStat( sdnbStatArgument * aStatArgument,
                                          sdnbHeader       * aIdxHdr,
                                          sdpPhyPageHdr    * aNode )
{
    UInt          i;
    UInt          sKeyCount    = 0;
    UInt          sNumDist     = 0;
    UInt          sClusteringFactor = 0;
    UShort        sSlotCount;
    UChar       * sSlotDirPtr = NULL;
    idBool        sIsSameValue;
    sdnbKeyInfo   sPrevKeyInfo;
    sdnbKeyInfo   sCurKeyInfo;
    UInt          sKeyValueLen;
    UInt          sMetaSpace   = 0;
    UInt          sUsedSpace   = 0;
    UInt          sAgableSpace = 0;
    UInt          sFreeSpace   = 0;
    sdnbLKey    * sCurKey = NULL;
    sdnbNodeHdr * sNodeHdr = NULL;

    sNodeHdr = (sdnbNodeHdr*)sdpPhyPage::getLogicalHdrStartPtr((UChar *)aNode);

    sMetaSpace = ID_SIZEOF( sdpPhyPageHdr )
                 + idlOS::align8(ID_SIZEOF(sdnbNodeHdr))
                 + sdpPhyPage::getSizeOfCTL( aNode )
                 + ID_SIZEOF( sdpPageFooter )
                 + ID_SIZEOF( sdpSlotDirHdr );
    sFreeSpace = sNodeHdr->mTotalDeadKeySize
                 + sdpPhyPage::getTotalFreeSize( aNode );

    sSlotDirPtr = sdpPhyPage::getSlotDirStartPtr( (UChar *)aNode );
    sSlotCount  = sdpSlotDirectory::getCount( sSlotDirPtr );

    sPrevKeyInfo.mKeyValue = NULL;

    for ( i = 0 ; i < sSlotCount ; i++ )
    {
        IDE_TEST( sdpSlotDirectory::getPagePtrFromSlotNum( sSlotDirPtr, 
                                                           i,
                                                           (UChar **)&sCurKey )
                  != IDE_SUCCESS );

        SDNB_LKEY_TO_KEYINFO( sCurKey, sCurKeyInfo );
        sKeyValueLen = ID_SIZEOF( sdpSlotEntry )
                        + getKeyLength( &(aIdxHdr->mColLenInfoList),
                                        (UChar *)sCurKey,
                                        ID_TRUE ); /* aIsLeaf */

        if ( ( SDNB_GET_STATE(sCurKey) == SDNB_KEY_UNSTABLE ) ||
             ( SDNB_GET_STATE(sCurKey) == SDNB_KEY_STABLE ) )
        {
            sUsedSpace += sKeyValueLen;
            sKeyCount ++;
        }
        else
        {
            if ( SDNB_GET_STATE( sCurKey ) == SDNB_KEY_DELETED )
            {
                sAgableSpace += sKeyValueLen;
            }
            else
            {
                IDE_DASSERT( SDNB_GET_STATE( sCurKey ) == SDNB_KEY_DEAD );
                /* already cumulatted */
            }
        }

        if ( isIgnoredKey4NumDistStat( aIdxHdr, sCurKey ) == ID_TRUE )
        {
            continue;
        }
        else
        {
            /* nothing to do */
        }

        if ( sPrevKeyInfo.mKeyValue == NULL )
        {
            sNumDist = 0;
        }
        else
        {
            (void)compareKeyAndKey( &aIdxHdr->mQueryStat,
                                    aIdxHdr->mColumns,
                                    aIdxHdr->mColumnFence,
                                    &sPrevKeyInfo,
                                    &sCurKeyInfo,
                                    SDNB_COMPARE_VALUE,
                                    &sIsSameValue );

            if ( sIsSameValue == ID_FALSE )
            {
                sNumDist++;
            }
            else
            {
                /* nothing to do */
            }
            if ( sPrevKeyInfo.mRowPID != sCurKeyInfo.mRowPID )
            {
                sClusteringFactor ++;
            }
            else
            {
                /* nothing to do */
            }
        }

        sPrevKeyInfo = sCurKeyInfo;
    }

    IDE_DASSERT( sMetaSpace + sUsedSpace + sAgableSpace + sFreeSpace ==
                 SD_PAGE_SIZE );

    aStatArgument->mMetaSpace   += sMetaSpace;
    aStatArgument->mUsedSpace   += sUsedSpace;
    aStatArgument->mAgableSpace += sAgableSpace;
    aStatArgument->mFreeSpace   += sFreeSpace;

    aStatArgument->mAnalyzedPageCount ++;
    aStatArgument->mKeyCount          += sKeyCount;
    aStatArgument->mNumDist           += sNumDist;
    aStatArgument->mClusteringFactor  += sClusteringFactor;

    return IDE_SUCCESS;
    
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : sdnbBTree::NA                              *
 * ------------------------------------------------------------------*
 * default function for invalid traversing protocol                  *
 *********************************************************************/
IDE_RC sdnbBTree::NA( void )
{

    IDE_SET( ideSetErrorCode( smERR_ABORT_smiTraverseNotApplicable ) );

    return IDE_FAILURE;

}

/*********************************************************************
 * FUNCTION DESCRIPTION : sdnbBTree::beforeFirst                     *
 * ------------------------------------------------------------------*
 * �־��� callback�� �ǰ��Ͽ� keyrange�� �ش��ϴ� leaf slot�� �ٷ�   *
 * ������ Ŀ���� �̵���Ų��.                                         *
 * �ַ� read lock���� traversing�Ҷ� ȣ��ȴ�.
 *********************************************************************/
IDE_RC sdnbBTree::beforeFirst( sdnbIterator*       aIterator,
                               const smSeekFunc** /**/)
{

    for ( aIterator->mKeyRange      = aIterator->mKeyRange ;
          aIterator->mKeyRange->prev != NULL ;
          aIterator->mKeyRange        = aIterator->mKeyRange->prev ) ;

    IDE_TEST( beforeFirstInternal( aIterator ) != IDE_SUCCESS );

    aIterator->mFlag  = aIterator->mFlag & (~SMI_RETRAVERSE_MASK);
    aIterator->mFlag |= SMI_RETRAVERSE_BEFORE;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

/*********************************************************************
 * FUNCTION DESCRIPTION : sdnbBTree::beforeFirstInternal             *
 * ------------------------------------------------------------------*
 * �־��� callback�� �ǰ��Ͽ� keyrange�� �ش��ϴ� ��� leaf slot��   *
 * �ٷ� ������ Ŀ���� �̵���Ų��.                                    *
 * key Range�� ����Ʈ�� ������ �� �ִµ�, �ش��ϴ� Key�� ��������    *
 * �ʴ� key range�� skip�Ѵ�.                                        *
 *********************************************************************/
IDE_RC sdnbBTree::beforeFirstInternal( sdnbIterator* aIterator )
{

    if ( aIterator->mProperties->mReadRecordCount > 0 )
    {
        IDE_TEST( sdnbBTree::findFirst( aIterator ) != IDE_SUCCESS );

        while( ( aIterator->mCacheFence == aIterator->mRowCache ) &&
               ( aIterator->mIsLastNodeInRange == ID_TRUE ) &&
               ( aIterator->mKeyRange->next != NULL ) )
        {
            aIterator->mKeyRange = aIterator->mKeyRange->next;
            IDE_TEST( sdnbBTree::findFirst( aIterator )
                      != IDE_SUCCESS );
        }
    }
    else
    {
        /* nothing to do */
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

/*********************************************************************
 * FUNCTION DESCRIPTION : sdnbBTree::findFirst                       *
 * ------------------------------------------------------------------*
 * �־��� minimum callback�� �ǰ��Ͽ� �ش� callback�� �����ϴ� slot  *
 * �� �ٷ� �� slot�� ã�´�.                                         *
 *********************************************************************/
IDE_RC sdnbBTree::findFirst( sdnbIterator * aIterator )
{
    idBool          sIsSuccess;
    UShort          sSlotCount;
    UChar         * sSlotDirPtr;
    sdnbStack       sStack;
    ULong           sIdxSmoNo;
    ULong           sNodeSmoNo;
    scPageID        sPID;
    scPageID        sChildPID;
    sdpPhyPageHdr * sNode;
    sdnbNodeHdr   * sNodeHdr;
    SShort          sSlotPos;
    idBool          sRetry = ID_FALSE;
    UInt            sFixState = 0;
    idBool          sTrySuccess;
    sdnbHeader    * sIndex;
    idvWeArgs       sWeArgs;
    idvSQL        * sStatistics;

    sStatistics = aIterator->mProperties->mStatistics;
    sIndex = (sdnbHeader*)((smnIndexHeader*)(aIterator->mIndex))->mHeader;

    initStack( &sStack );

  retry:
    /* BUG-31559 [sm-disk-index] invalid snapshot reading 
     *           about Index Runtime-header in DRDB
     * sIndex, �� RuntimeHeader�� �������� Latch�� ���� ���� ���·� �б⿡
     * �����ؾ� �մϴ�. �� RootNode�� NullPID�� �ƴ��� üũ������, �� ����
     * ������ �����صΰ�, ����� ���� Ȯ���Ͽ� ����ؾ� �մϴ�.
     * ���� �̶� SmoNO�� -�ݵ��- ���� Ȯ���ؾ� �Ѵٴ� �͵� ������ �ȵ˴ϴ�.*/
    getSmoNo( (void *)sIndex, &sIdxSmoNo );

    IDL_MEM_BARRIER;

    sPID = sIndex->mRootNode;

    if ( sPID != SD_NULL_PID )
    {
        IDE_TEST( sdnbBTree::getPage( sStatistics,
                                      &(sIndex->mQueryStat.mIndexPage),
                                      sIndex->mIndexTSID,
                                      sPID,
                                      SDB_S_LATCH,
                                      SDB_WAIT_NORMAL,
                                      NULL,
                                      (UChar **)&sNode,
                                      &sTrySuccess )
                  != IDE_SUCCESS );
        sFixState = 1;

    go_ahead:
        sNodeHdr = (sdnbNodeHdr*)sdpPhyPage::getLogicalHdrStartPtr((UChar *)sNode);
        while ( sNodeHdr->mHeight > 0 ) // internal nodes
        {
            IDE_TEST( iduCheckSessionEvent( sStatistics ) != IDE_SUCCESS );
            IDE_TEST( findFirstInternalKey( &aIterator->mKeyRange->minimum,
                                            sIndex,
                                            sNode,
                                            &(sIndex->mQueryStat),
                                            &sChildPID,
                                            sIdxSmoNo,
                                            &sNodeSmoNo,
                                            &sRetry )
                      != IDE_SUCCESS );

            if ( sRetry == ID_TRUE )
            {
                sFixState = 0;
                IDE_TEST( sdbBufferMgr::releasePage( sStatistics,
                                                     (UChar *)sNode )
                          != IDE_SUCCESS );

                IDV_WEARGS_SET(
                    &sWeArgs,
                    IDV_WAIT_INDEX_LATCH_DRDB_BTREE_INDEX_SMO_BY_OTHER_SESSION,
                    0,   // WaitParam1
                    0,   // WaitParam2
                    0 ); // WaitParam3

                // unlatch tree latch
                IDE_TEST( sIndex->mLatch.lockRead( sStatistics,
                                                   &sWeArgs )
                          != IDE_SUCCESS );

                // blocked...

                // unlatch tree latch
                IDE_TEST( sIndex->mLatch.unlock() != IDE_SUCCESS );
                IDE_TEST( findValidStackDepth(sStatistics,
                                              &(sIndex->mQueryStat),
                                              sIndex,
                                              &sStack,
                                              &sIdxSmoNo )
                          != IDE_SUCCESS );
                if ( sStack.mIndexDepth < 0 ) // SMO�� Root���� �Ͼ.
                {
                    initStack( &sStack );
                    goto retry;     // ó���� ���� �ٽ� ����
                }
                else
                {
                    // mIndexDepth - 1���� ����...�� �������� ����
                    sPID = sStack.mStack[sStack.mIndexDepth+1].mNode;
                    IDE_TEST( sdnbBTree::getPage( sStatistics,
                                                  &(sIndex->mQueryStat.mIndexPage),
                                                  sIndex->mIndexTSID,
                                                  sPID,
                                                  SDB_S_LATCH,
                                                  SDB_WAIT_NORMAL,
                                                  NULL,
                                                  (UChar **)&sNode,
                                                  &sTrySuccess )
                              != IDE_SUCCESS );
                    sFixState = 1;

                    goto go_ahead;
                }
            }

            sStack.mIndexDepth++;
            sStack.mStack[sStack.mIndexDepth].mNode = sPID;
            sStack.mStack[sStack.mIndexDepth].mKeyMapSeq = 0;
            sStack.mStack[sStack.mIndexDepth].mSmoNo = sNodeSmoNo;
            sPID = sChildPID;

            if ( sNodeHdr->mHeight == 1 ) // leaf �ٷ� ���� node
            {
                break;
            }
            else
            {
                sFixState = 0;
                IDE_TEST( sdbBufferMgr::releasePage( sStatistics,
                                                     (UChar *)sNode )
                          != IDE_SUCCESS );

                IDE_TEST( sdnbBTree::getPage( sStatistics,
                                              &(sIndex->mQueryStat.mIndexPage),
                                              sIndex->mIndexTSID,
                                              sPID,
                                              SDB_S_LATCH,
                                              SDB_WAIT_NORMAL,
                                              NULL,
                                              (UChar **)&sNode,
                                              &sTrySuccess )
                          != IDE_SUCCESS );
                sFixState = 1;

                sNodeHdr = (sdnbNodeHdr*)sdpPhyPage::getLogicalHdrStartPtr(
                                                               (UChar *)sNode);
            }

        } // while

        sFixState = 0;
        IDE_TEST( sdbBufferMgr::releasePage( sStatistics,
                                             (UChar *)sNode )
                  != IDE_SUCCESS );

        IDE_TEST( sdnbBTree::getPage( sStatistics,
                                      &(sIndex->mQueryStat.mIndexPage),
                                      sIndex->mIndexTSID,
                                      sPID,
                                      SDB_S_LATCH,
                                      SDB_WAIT_NORMAL,
                                      NULL,
                                      (UChar **)&sNode,
                                      &sIsSuccess )
                  != IDE_SUCCESS );
        sFixState = 1;

        sSlotDirPtr = sdpPhyPage::getSlotDirStartPtr( (UChar *)sNode );
        sSlotCount  = sdpSlotDirectory::getCount( sSlotDirPtr );

        IDL_MEM_BARRIER;

        if ( checkSMO( &(sIndex->mQueryStat),
                       sNode,
                       sIdxSmoNo,
                       &sNodeSmoNo) > 0 )
        {
            sFixState = 0;
            IDE_TEST( sdbBufferMgr::releasePage( sStatistics,
                                                 (UChar *)sNode )
                      != IDE_SUCCESS );


            IDV_WEARGS_SET(
                &sWeArgs,
                IDV_WAIT_INDEX_LATCH_DRDB_BTREE_INDEX_SMO_BY_OTHER_SESSION,
                0,   // WaitParam1
                0,   // WaitParam2
                0 ); // WaitParam3

            // unlatch tree latch
            IDE_TEST( sIndex->mLatch.lockRead( sStatistics,
                                               &sWeArgs )
                      != IDE_SUCCESS );
            // blocked...

            // unlatch tree latch
            IDE_TEST( sIndex->mLatch.unlock() != IDE_SUCCESS );
            IDE_TEST( findValidStackDepth( sStatistics,
                                           &(sIndex->mQueryStat),
                                           sIndex,
                                           &sStack,
                                           &sIdxSmoNo )
                      != IDE_SUCCESS );
            if ( sStack.mIndexDepth < 0 ) // SMO�� Root���� �Ͼ.
            {
                initStack( &sStack );
                goto retry;     // ó���� ���� �ٽ� ����
            }
            else
            {
                // mIndexDepth - 1���� ����...�� �������� ����
                sPID = sStack.mStack[sStack.mIndexDepth+1].mNode;

                IDE_TEST( sdnbBTree::getPage( sStatistics,
                                              &(sIndex->mQueryStat.mIndexPage),
                                              sIndex->mIndexTSID,
                                              sPID,
                                              SDB_S_LATCH,
                                              SDB_WAIT_NORMAL,
                                              NULL,
                                              (UChar **)&sNode,
                                              &sTrySuccess )
                          != IDE_SUCCESS );
                sFixState = 1;

                goto go_ahead;
            }
        }
        else
        {
            IDE_ERROR( findFirstLeafKey( &aIterator->mKeyRange->minimum,
                                         sIndex,
                                         (UChar *)sNode,
                                         0,
                                         (SShort)(sSlotCount - 1),
                                         &sSlotPos)
                       == IDE_SUCCESS );
        }

        sStack.mIndexDepth++;
        sStack.mStack[sStack.mIndexDepth].mNode = sPID;
        sStack.mStack[sStack.mIndexDepth].mKeyMapSeq = 0;
        sStack.mStack[sStack.mIndexDepth].mSmoNo = sNodeSmoNo;

        aIterator->mCurLeafNode  = sPID;
        aIterator->mNextLeafNode = sNode->mListNode.mNext;
        aIterator->mCurNodeSmoNo = sNodeSmoNo;
        aIterator->mIndexSmoNo   = sIdxSmoNo;
        aIterator->mScanBackNode = SD_NULL_PID;
        aIterator->mCurNodeLSN   = sdpPhyPage::getPageLSN(
                                       sdpPhyPage::getPageStartPtr((UChar *)sNode));

        // sSlotPos�� beforeFirst�� ��ġ�̹Ƿ� sSlotPos + 1 ����...
        IDE_TEST( makeRowCacheForward( aIterator,
                                       (UChar *)sNode,
                                       sSlotPos + 1 )
                  != IDE_SUCCESS );

        sFixState = 0;
        IDE_TEST( sdbBufferMgr::releasePage( sStatistics,
                                             (UChar *)sNode )
                  != IDE_SUCCESS );

        /* FIT/ART/sm/Bugs/BUG-27163/BUG-27163.tc */
        IDU_FIT_POINT( "1.BUG-27163@sdnbBTree::findFirst" );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if ( sFixState == 1 )
    {
        (void)sdbBufferMgr::releasePage( sStatistics,
                                         (UChar *)sNode );
    }

    return IDE_FAILURE;

}


/*********************************************************************
 * FUNCTION DESCRIPTION : sdnbBTree::findFirstInternalKey            *
 * ------------------------------------------------------------------*
 * �־��� minimum callback�� �ǰ��Ͽ� Internal node���� �ش�         *
 * callback�� �����ϴ� slot�� �ٷ� �� slot�� ã�´�.                 *
 * ���ο��� SMO �� �Ͼ���� üũ�� �ϸ鼭 �����Ѵ�.                *
 *********************************************************************/
IDE_RC sdnbBTree::findFirstInternalKey ( const smiCallBack  * aCallBack,
                                         sdnbHeader         * aIndex,
                                         sdpPhyPageHdr      * aNode,
                                         sdnbStatistic      * aIndexStat,
                                         scPageID           * aChildNode,
                                         ULong                aIndexSmoNo,
                                         ULong              * aNodeSmoNo,
                                         idBool             * aRetry )
{ 
    UChar         *sSlotDirPtr;
    UChar         *sKey;
    idBool         sResult;
    sdnbIKey      *sIKey;
    sdnbNodeHdr   *sNodeHdr;
    SShort         sMedium = 0;
    SShort         sMinimum = 0;
    UShort         sKeySlotCount;
    SShort         sMaximum;
    smiValue       sSmiValueList[ SMI_MAX_IDX_COLUMNS ];

    *aRetry = ID_FALSE;

    sSlotDirPtr   = sdpPhyPage::getSlotDirStartPtr( (UChar *)aNode );
    sKeySlotCount = sdpSlotDirectory::getCount( sSlotDirPtr );
    sMaximum      = sKeySlotCount - 1;

    IDL_MEM_BARRIER;

    if ( checkSMO( aIndexStat, 
                   aNode, 
                   aIndexSmoNo, 
                   aNodeSmoNo ) > 0 )
    {
        *aRetry = ID_TRUE;
        return IDE_SUCCESS;
    }
    else
    {
        /* nothing to do */
    }

    IDL_MEM_BARRIER;

    while ( sMinimum <= sMaximum )
    {
        sMedium = (sMinimum + sMaximum) >> 1;
        IDE_TEST( sdpSlotDirectory::getPagePtrFromSlotNum( sSlotDirPtr,
                                                           sMedium,
                                                           (UChar **)&sIKey )
                  != IDE_SUCCESS );
        sKey = SDNB_IKEY_KEY_PTR(sIKey);

        //PROJ-1872 Disk index ���� ���� ����ȭ
        //Compare(KeyFilter ����)�� ���� ���� �۾�.
        makeSmiValueListFromKeyValue( &(aIndex->mColLenInfoList),
                                      sKey,
                                      sSmiValueList );

        (void) aCallBack->callback( &sResult,
                                    sSmiValueList,
                                    NULL,
                                    0,
                                    SC_NULL_GRID,
                                    aCallBack->data );

        if ( sResult == ID_TRUE )
        {
            sMaximum = sMedium - 1;
        }
        else
        {
            sMinimum = sMedium + 1;
        }
    }

    sMedium = (sMinimum + sMaximum) >> 1;
    if ( sMedium == -1 )
    {
        sNodeHdr = (sdnbNodeHdr*)sdpPhyPage::getLogicalHdrStartPtr((UChar *)aNode);
        *aChildNode = sNodeHdr->mLeftmostChild;
    }
    else
    {
        IDE_TEST( sdpSlotDirectory::getPagePtrFromSlotNum( sSlotDirPtr,
                                                           sMedium,
                                                           (UChar **)&sIKey )
                  != IDE_SUCCESS );
        SDNB_GET_RIGHT_CHILD_PID( aChildNode, sIKey );
    }

    IDL_MEM_BARRIER;

    if ( checkSMO(aIndexStat, aNode, aIndexSmoNo, aNodeSmoNo) > 0 )
    {
        *aRetry = ID_TRUE;
        return IDE_SUCCESS;
    }

    return IDE_SUCCESS;
    
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : sdnbBTree::findFirstLeafKey                *
 * ------------------------------------------------------------------*
 * �־��� minimum callback�� �ǰ��Ͽ� Internal node���� �ش�         *
 * callback�� �����ϴ� slot�� �ٷ� �� slot�� ã�´�.                 *
 * smo �߽߰� aRetry�� ID_TRUE�� return�Ѵ�.                         *
 *********************************************************************/
IDE_RC sdnbBTree::findFirstLeafKey ( const smiCallBack *aCallBack,
                                     sdnbHeader        *aIndex,
                                     UChar             *aPagePtr,
                                     SShort             aMinimum,
                                     SShort             aMaximum,
                                     SShort            *aSlot )
{

    SShort       sMedium;
    idBool       sResult;
    UChar *      sKey;
    UChar *      sSlot;
    UChar       *sSlotDirPtr;
    smiValue     sSmiValueList[ SMI_MAX_IDX_COLUMNS ];

    sSlotDirPtr = sdpPhyPage::getSlotDirStartPtr( aPagePtr );

    while(aMinimum <= aMaximum)
    {
        sMedium = (aMinimum + aMaximum) >> 1;
        IDE_TEST( sdpSlotDirectory::getPagePtrFromSlotNum( sSlotDirPtr, 
                                                           sMedium,
                                                           &sSlot )
                  != IDE_SUCCESS );
        sKey    = SDNB_LKEY_KEY_PTR(sSlot);

        //PROJ-1872 Disk index ���� ���� ����ȭ
        //Compare(KeyFilter ����)�� ���� ���� �۾�.
        makeSmiValueListFromKeyValue( &(aIndex->mColLenInfoList),
                                      sKey,
                                      sSmiValueList );

        (void) aCallBack->callback( &sResult,
                                    sSmiValueList,
                                    NULL,
                                    0,
                                    SC_NULL_GRID,
                                    aCallBack->data);

        if ( sResult == ID_TRUE )
        {
            aMaximum = sMedium - 1;
        }
        else
        {
            aMinimum = sMedium + 1;
        }
    }

    sMedium = (aMinimum + aMaximum) >> 1;
    *aSlot = sMedium;

    return IDE_SUCCESS;
    
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/*********************************************************************
 * FUNCTION DESCRIPTION : sdnbBTree::makeRowCacheForward             *
 * ------------------------------------------------------------------*
 * aNode�� aStartSlotSeq���� ���������� �����ϸ� aCallBack�� �����ϴ�*
 * ������ Slot���� mRowRID�� Key Value�� Row Cache�� �����Ѵ�.       *
 * Row Cache�� ����� maximum KeyRange�� ����� slot�� �߿���        *
 * Transaction Level Visibility�� ����ϴ� Slot���̴�.               *
 *********************************************************************/
IDE_RC sdnbBTree::makeRowCacheForward( sdnbIterator   * aIterator,
                                       UChar          * aLeafNode,
                                       SShort           aStartSlotSeq )
{

    idBool              sResult;
    sdnbLKey          * sLKey;
    sdnbKeyInfo         sKeyInfo;
    SShort              sToSeq;
    UChar             * sKey;
    SInt                i;
    const smiRange    * sKeyFilter;
    sdnbVisibility      sVisibility = SDNB_VISIBILITY_NO;
    UChar             * sSlotDirPtr;
    smiValue            sSmiValueList[ SMI_MAX_IDX_COLUMNS ];
    UShort              sSlotCount;
    const smiCallBack * sMaximum = &aIterator->mKeyRange->maximum;
    sdnbHeader        * sIndex;

    /* FIT/ART/sm/Bugs/BUG-30911/BUG-30911.tc */
    IDU_FIT_POINT( "1.BUG-30911@sdnbBTree::makeRowCacheForward" );

    sIndex = (sdnbHeader*)((smnIndexHeader*)(aIterator->mIndex))->mHeader;

    sSlotDirPtr = sdpPhyPage::getSlotDirStartPtr( aLeafNode );
    sSlotCount  = sdpSlotDirectory::getCount( sSlotDirPtr );

    // aIterator�� Row Cache�� �ʱ�ȭ �Ѵ�.
    aIterator->mCurRowPtr = aIterator->mRowCache - 1;
    aIterator->mCacheFence = &aIterator->mRowCache[0];

    IDE_TEST( sdpSlotDirectory::getPagePtrFromSlotNum( sSlotDirPtr, 
                                                       sSlotCount - 1,
                                                       (UChar **)&sLKey )
              != IDE_SUCCESS );
    sKey = SDNB_LKEY_KEY_PTR( sLKey );

    //PROJ-1872 Disk index ���� ���� ����ȭ
    //Compare(KeyFilter ����)�� ���� ���� �۾�.
    makeSmiValueListFromKeyValue( &(sIndex->mColLenInfoList),
                                  sKey,
                                  sSmiValueList );

    (void)sMaximum->callback( &sResult,
                              sSmiValueList,
                              NULL,
                              0,
                              SC_NULL_GRID,
                              sMaximum->data);

    if ( sResult == ID_TRUE )
    {
        if ( sdpPhyPage::getNxtPIDOfDblList( sdpPhyPage::getHdr(aLeafNode) )
             != SD_NULL_PID )
        {
            aIterator->mIsLastNodeInRange = ID_FALSE;
            sToSeq = sSlotCount - 1;
        }
        else // next PID == NULL PID
        {
            aIterator->mIsLastNodeInRange = ID_TRUE;
            sToSeq = sSlotCount - 1;
        }
    }
    else // sResult == ID_FALSE
    {
        aIterator->mIsLastNodeInRange = ID_TRUE;
        IDE_ERROR( findLastLeafKey( sMaximum, 
                                    sIndex, 
                                    aLeafNode, 
                                    0, 
                                    (SShort)(sSlotCount - 1), 
                                    &sToSeq )
                   == IDE_SUCCESS );
        sToSeq--; // afterLast ��ġ�̹Ƿ� 1�� ���� ������ slot��ȣ�� ���Ѵ�.
    }

    for ( i = aStartSlotSeq ; i <= sToSeq ; i++ )
    {
        IDE_TEST( sdpSlotDirectory::getPagePtrFromSlotNum( sSlotDirPtr, 
                                                           i,
                                                           (UChar **)&sLKey )
                  != IDE_SUCCESS );

        sKey = SDNB_LKEY_KEY_PTR( sLKey );

        //PROJ-1872 Disk index ���� ���� ����ȭ
        //Compare(KeyFilter ����)�� ���� ���� �۾�.
        makeSmiValueListFromKeyValue( &(sIndex->mColLenInfoList),
                                      sKey,
                                      sSmiValueList );

        sKeyFilter = aIterator->mKeyFilter;
        while ( sKeyFilter != NULL )
        {
            sKeyFilter->minimum.callback( &sResult,
                                          sSmiValueList,
                                          NULL,
                                          0,
                                          SC_NULL_GRID,
                                          sKeyFilter->minimum.data);
            if ( sResult == ID_TRUE )
            {
                sKeyFilter->maximum.callback( &sResult,
                                              sSmiValueList,
                                              NULL,
                                              0,
                                              SC_NULL_GRID,
                                              sKeyFilter->maximum.data);
                if ( sResult == ID_TRUE )
                {
                    // minimum, maximum ��� ���.
                    break;
                }
                else
                {
                    /* nothing to do */
                }
            }
            else
            {
                /* nothing to do */
            }
            sKeyFilter = sKeyFilter->next;
        }//while

        if ( sResult == ID_FALSE )
        {
            continue;
        }
        else
        {
            /* nothing to do */
        }

        /* BUG-48353
           transLevelVisibility()���� Pending Wait ���� �ʴ´�. 
           (���� Pending Wait ������ Visibie ���θ� �˼��ִ� KEY�� �ִٸ�
            UNKNOWN ���� �Ǵ��� �ϴ� ĳ���� ����ȴ�.) */
        sVisibility = SDNB_VISIBILITY_NO;
        IDE_TEST( transLevelVisibility( aIterator->mProperties->mStatistics,
                                        aIterator->mTrans,
                                        sIndex,
                                        aLeafNode,
                                        (UChar *)sLKey,
                                        &aIterator->mSCN,
                                        &sVisibility )
                  != IDE_SUCCESS );

        if ( sVisibility == SDNB_VISIBILITY_NO )
        {
            continue;
        }

        /* BUG-48353 : VISIBILIY ����� LEAF NODE�� KEY�� ĳ���� �����Ѵ�. */

        // save slot info
        SDNB_LKEY_TO_KEYINFO( sLKey, sKeyInfo );
        aIterator->mCacheFence->mRowPID = sKeyInfo.mRowPID;
        aIterator->mCacheFence->mRowSlotNum = sKeyInfo.mRowSlotNum;
        aIterator->mCacheFence->mOffset =
                              sdpPhyPage::getOffsetFromPagePtr( (UChar *)sLKey );

        idlOS::memcpy( &(aIterator->mPage[aIterator->mCacheFence->mOffset]),
                       (UChar *)sLKey,
                       getKeyLength( &(sIndex->mColLenInfoList),
                                     (UChar *)sLKey,
                                     ID_TRUE ) );

        /* ���� �������� ĳ���� ��ϵ� KEY(sdnbLKey+KEY��) */
        aIterator->mPrevKey =
                 (sdnbLKey*)&(aIterator->mPage[aIterator->mCacheFence->mOffset]);

        if ( sVisibility == SDNB_VISIBILITY_UNKNOWN )
        {
            SDNB_SET_STATE( aIterator->mPrevKey, SDNB_CACHE_VISIBLE_UNKNOWN );
        }
        else /* SDNB_VISIBILITY_YES */
        {
            SDNB_SET_STATE( aIterator->mPrevKey, SDNB_CACHE_VISIBLE_YES );
        }

        // increase cache fence
        aIterator->mCacheFence++;
    }
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/*********************************************************************
 * FUNCTION DESCRIPTION : sdnbBTree::afterLast                       *
 * ------------------------------------------------------------------*
 * �־��� callback�� �ǰ��Ͽ� keyrange�� �ش��ϴ� ��� leaf slot��   *
 * �ٷ� �ڷ� Ŀ���� �̵���Ų��.                                      *
 *********************************************************************/
IDE_RC sdnbBTree::afterLast( sdnbIterator*       aIterator,
                             const smSeekFunc** /**/)
{

    for ( aIterator->mKeyRange        = aIterator->mKeyRange ;
          aIterator->mKeyRange->next != NULL ;
          aIterator->mKeyRange        = aIterator->mKeyRange->next ) ;

    IDE_TEST( afterLastInternal( aIterator ) != IDE_SUCCESS );

    aIterator->mFlag  = aIterator->mFlag & (~SMI_RETRAVERSE_MASK);
    aIterator->mFlag |= SMI_RETRAVERSE_AFTER;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

/*********************************************************************
 * FUNCTION DESCRIPTION : sdnbBTree::afterLastInternal               *
 * ------------------------------------------------------------------*
 * �־��� callback�� �ǰ��Ͽ� keyrange�� �ش��ϴ� ��� leaf slot��   *
 * �ٷ� �ڷ� Ŀ���� �̵���Ų��.                                      *
 *********************************************************************/
IDE_RC sdnbBTree::afterLastInternal( sdnbIterator* aIterator )
{

    if ( aIterator->mProperties->mReadRecordCount > 0 )
    {
        IDE_TEST( sdnbBTree::findLast(aIterator) != IDE_SUCCESS );

        while( ( aIterator->mCacheFence == aIterator->mRowCache ) &&
               ( aIterator->mIsLastNodeInRange == ID_TRUE ) &&
               ( aIterator->mKeyRange->prev != NULL ) )
        {
            aIterator->mKeyRange = aIterator->mKeyRange->prev;
            IDE_TEST( sdnbBTree::findLast(aIterator ) != IDE_SUCCESS );
        }
    }
    else
    {
        /* nothing to do */
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}


/*********************************************************************
 * FUNCTION DESCRIPTION : sdnbBTree::findLast                        *
 * ------------------------------------------------------------------*
 * �־��� callback�� �ǰ��Ͽ� keyrange�� �ش��ϴ� ��� leaf slot��   *
 * �ٷ� �ڷ� Ŀ���� �̵���Ų��.                                      *
 *********************************************************************/
IDE_RC sdnbBTree::findLast( sdnbIterator * aIterator )
{
    sdnbStack       sStack;
    ULong           sIdxSmoNo;
    ULong           sNodeSmoNo;
    scPageID        sPID;
    scPageID        sChildPID;
    sdpPhyPageHdr * sNode;
    sdnbNodeHdr *   sNodeHdr;
    UShort          sSlotCount;
    UChar         * sSlotDirPtr;
    SShort          sSlotPos;
    idBool          sRetry = ID_FALSE;
    UInt            sFixState = 0;
    idBool          sTrySuccess;
    sdnbHeader    * sIndex;
    idvWeArgs       sWeArgs;
    idvSQL        * sStatistics;

    sStatistics = aIterator->mProperties->mStatistics;
    sIndex      = (sdnbHeader*)((smnIndexHeader*)(aIterator->mIndex))->mHeader;

    initStack( &sStack );

  retry:
    /* BUG-31559 [sm-disk-index] invalid snapshot reading 
     *           about Index Runtime-header in DRDB
     * sIndex, �� RuntimeHeader�� �������� Latch�� ���� ���� ���·� �б⿡
     * �����ؾ� �մϴ�. �� RootNode�� NullPID�� �ƴ��� üũ������, �� ����
     * ������ �����صΰ�, ����� ���� Ȯ���Ͽ� ����ؾ� �մϴ�.
     * ���� �̶� SmoNO�� -�ݵ��- ���� Ȯ���ؾ� �Ѵٴ� �͵� ������ �ȵ˴ϴ�.*/
    getSmoNo( (void *)sIndex, &sIdxSmoNo );

    IDL_MEM_BARRIER;

    sPID = sIndex->mRootNode;

    if ( sPID != SD_NULL_PID )
    {
        IDE_TEST( sdnbBTree::getPage( sStatistics,
                                      &(sIndex->mQueryStat.mIndexPage),
                                      sIndex->mIndexTSID,
                                      sPID,
                                      SDB_S_LATCH,
                                      SDB_WAIT_NORMAL,
                                      NULL,
                                      (UChar **)&sNode,
                                      &sTrySuccess) != IDE_SUCCESS );
        sFixState = 1;

      go_ahead:
        sNodeHdr = (sdnbNodeHdr*)sdpPhyPage::getLogicalHdrStartPtr((UChar *)sNode);
        while ( sNodeHdr->mHeight > 0 ) // internal nodes
        {
            IDE_TEST(iduCheckSessionEvent(sStatistics) != IDE_SUCCESS );
            IDE_TEST( findLastInternalKey( &aIterator->mKeyRange->maximum,
                                           sIndex,
                                           sNode,
                                           &(sIndex->mQueryStat),
                                           &sChildPID,
                                           sIdxSmoNo,
                                           &sNodeSmoNo,
                                           &sRetry )
                     != IDE_SUCCESS );
            if ( sRetry == ID_TRUE )
            {
                sFixState = 0;
                IDE_TEST( sdbBufferMgr::releasePage( sStatistics,
                                                     (UChar *)sNode )
                          != IDE_SUCCESS );

                IDV_WEARGS_SET(
                    &sWeArgs,
                    IDV_WAIT_INDEX_LATCH_DRDB_BTREE_INDEX_SMO_BY_OTHER_SESSION,
                    0,   // WaitParam1
                    0,   // WaitParam2
                    0 ); // WaitParam3

                // unlatch tree latch
                IDE_TEST( sIndex->mLatch.lockRead( sStatistics,
                                                  &sWeArgs )
                          != IDE_SUCCESS );

                // blocked...

                // unlatch tree latch
                IDE_TEST( sIndex->mLatch.unlock() != IDE_SUCCESS );
                IDE_TEST( findValidStackDepth( sStatistics,
                                               &(sIndex->mQueryStat),
                                               sIndex,
                                               &sStack,
                                               &sIdxSmoNo )
                          != IDE_SUCCESS );
                if ( sStack.mIndexDepth < 0 ) // SMO�� Root���� �Ͼ.
                {
                    initStack( &sStack );
                    goto retry;     // ó���� ���� �ٽ� ����
                }
                else
                {
                    // mIndexDepth - 1���� ����...�� �������� ����
                    sPID = sStack.mStack[sStack.mIndexDepth+1].mNode;

                    IDE_TEST( sdnbBTree::getPage( sStatistics,
                                                  &(sIndex->mQueryStat.mIndexPage),
                                                  sIndex->mIndexTSID,
                                                  sPID,
                                                  SDB_S_LATCH,
                                                  SDB_WAIT_NORMAL,
                                                  NULL,
                                                  (UChar **)&sNode,
                                                  &sTrySuccess) != IDE_SUCCESS );
                    sFixState = 1;

                    goto go_ahead;
                }
            }
            else
            {
                /* nothing to do */
            }

            sStack.mIndexDepth++;
            sStack.mStack[sStack.mIndexDepth].mNode = sPID;
            sStack.mStack[sStack.mIndexDepth].mKeyMapSeq = 0;
            sStack.mStack[sStack.mIndexDepth].mSmoNo = sNodeSmoNo;
            sPID = sChildPID;

            if ( sNodeHdr->mHeight == 1 ) // leaf �ٷ� ���� node
            {
                break;
            }
            else
            {
                sFixState = 0;
                IDE_TEST( sdbBufferMgr::releasePage( sStatistics,
                                                     (UChar *)sNode )
                          != IDE_SUCCESS );

                IDE_TEST( sdnbBTree::getPage( sStatistics,
                                              &(sIndex->mQueryStat.mIndexPage),
                                              sIndex->mIndexTSID,
                                              sPID,
                                              SDB_S_LATCH,
                                              SDB_WAIT_NORMAL,
                                              NULL,
                                              (UChar **)&sNode,
                                              &sTrySuccess) != IDE_SUCCESS );
                sFixState = 1;

                sNodeHdr = (sdnbNodeHdr*)sdpPhyPage::getLogicalHdrStartPtr((UChar *)sNode);
            }
        } // while

        sFixState = 0;
        IDE_TEST( sdbBufferMgr::releasePage( sStatistics,
                                             (UChar *)sNode )
                  != IDE_SUCCESS );

        IDE_TEST( sdnbBTree::getPage( sStatistics,
                                      &(sIndex->mQueryStat.mIndexPage),
                                      sIndex->mIndexTSID,
                                      sPID,
                                      SDB_S_LATCH,
                                      SDB_WAIT_NORMAL,
                                      NULL,
                                      (UChar **)&sNode,
                                      &sTrySuccess) != IDE_SUCCESS );
        sFixState = 1;

        sSlotDirPtr = sdpPhyPage::getSlotDirStartPtr( (UChar *)sNode );
        sSlotCount  = sdpSlotDirectory::getCount( sSlotDirPtr );

        IDL_MEM_BARRIER;

        if ( checkSMO( &(sIndex->mQueryStat),
                       sNode,
                       sIdxSmoNo,
                       &sNodeSmoNo) > 0 )
        {
            sFixState = 0;
            IDE_TEST( sdbBufferMgr::releasePage( sStatistics,
                                                 (UChar *)sNode )
                      != IDE_SUCCESS );


            IDV_WEARGS_SET(
                    &sWeArgs,
                    IDV_WAIT_INDEX_LATCH_DRDB_BTREE_INDEX_SMO_BY_OTHER_SESSION,
                    0,   // WaitParam1
                    0,   // WaitParam2
                    0 ); // WaitParam3

            // unlatch tree latch
            IDE_TEST( sIndex->mLatch.lockRead( sStatistics,
                                              &sWeArgs )
                      != IDE_SUCCESS );

            // blocked...

            // unlatch tree latch
            IDE_TEST( sIndex->mLatch.unlock() != IDE_SUCCESS );
            IDE_TEST( findValidStackDepth( sStatistics,
                                           &(sIndex->mQueryStat),
                                           sIndex,
                                           &sStack,
                                           &sIdxSmoNo )
                      != IDE_SUCCESS );
            if ( sStack.mIndexDepth < 0 ) // SMO�� Root���� �Ͼ.
            {
                initStack( &sStack );
                goto retry;     // ó���� ���� �ٽ� ����
            }
            else
            {
                // mIndexDepth - 1���� ����...�� �������� ����
                sPID = sStack.mStack[sStack.mIndexDepth+1].mNode;

                IDE_TEST( sdnbBTree::getPage( sStatistics,
                                              &(sIndex->mQueryStat.mIndexPage),
                                              sIndex->mIndexTSID,
                                              sPID,
                                              SDB_S_LATCH,
                                              SDB_WAIT_NORMAL,
                                              NULL,
                                              (UChar **)&sNode,
                                              &sTrySuccess) != IDE_SUCCESS );
                sFixState = 1;

                goto go_ahead;
            }
        }
        else
        {
            IDE_ERROR( findLastLeafKey( &aIterator->mKeyRange->maximum,
                                        sIndex,
                                        (UChar *)sNode,
                                        0,
                                        (SShort)(sSlotCount - (SShort)1),
                                        &sSlotPos )
                       == IDE_SUCCESS );
        }
        sStack.mIndexDepth++;
        sStack.mStack[sStack.mIndexDepth].mNode = sPID;
        sStack.mStack[sStack.mIndexDepth].mKeyMapSeq = 0;
        sStack.mStack[sStack.mIndexDepth].mSmoNo = sNodeSmoNo;

        aIterator->mCurLeafNode  = sPID;
        aIterator->mNextLeafNode = sNode->mListNode.mPrev;
        aIterator->mCurNodeSmoNo = sNodeSmoNo;
        aIterator->mIndexSmoNo   = sIdxSmoNo;
        aIterator->mCurNodeLSN   = sdpPhyPage::getPageLSN(
                                       sdpPhyPage::getPageStartPtr((UChar *)sNode));
        // sSlotPos�� afterLast ��ġ�̹Ƿ� sSlotPos - 1����..
        IDE_TEST( makeRowCacheBackward( aIterator,
                                        (UChar *)sNode,
                                        sSlotPos - 1 )
                  != IDE_SUCCESS );

        sFixState = 0;
        IDE_TEST( sdbBufferMgr::releasePage( sStatistics,
                                             (UChar *)sNode )
                  != IDE_SUCCESS );

    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if ( sFixState ==  1 )
    {
        (void)sdbBufferMgr::releasePage( sStatistics,
                                         (UChar *)sNode );
    }

    return IDE_FAILURE;

}


/*********************************************************************
 * FUNCTION DESCRIPTION : sdnbBTree::findLastInternalKey             *
 * ------------------------------------------------------------------*
 * �־��� callback�� �ǰ��Ͽ� keyrange�� �ش��ϴ� ��� leaf slot��   *
 * �ٷ� �ڷ� Ŀ���� �̵���Ų��.                                      *
 * ���������� SMO üũ�� �ϸ鼭 �����Ѵ�.                            *
 *********************************************************************/
IDE_RC sdnbBTree::findLastInternalKey( const smiCallBack * aCallBack,
                                       sdnbHeader *        aIndex,
                                       sdpPhyPageHdr *     aNode,
                                       sdnbStatistic *     aIndexStat,
                                       scPageID *          aChildNode,
                                       ULong               aIndexSmoNo,
                                       ULong *             aNodeSmoNo,
                                       idBool *            aRetry )
{
    UChar         *sSlotDirPtr;
    UChar         *sKey;
    idBool         sResult;
    sdnbIKey      *sIKey;
    sdnbNodeHdr   *sNodeHdr;
    SShort         sMedium;
    SShort         sMinimum = 0;
    UShort         sKeySlotCount;
    SShort         sMaximum;
    smiValue       sSmiValueList[ SMI_MAX_IDX_COLUMNS ];

    *aRetry = ID_FALSE;

    sSlotDirPtr   = sdpPhyPage::getSlotDirStartPtr( (UChar *)aNode );
    sKeySlotCount = sdpSlotDirectory::getCount( sSlotDirPtr );
    sMaximum      = sKeySlotCount - 1;

    IDL_MEM_BARRIER;

    if ( checkSMO( aIndexStat, 
                   aNode, 
                   aIndexSmoNo, 
                   aNodeSmoNo ) > 0 )
    {
        *aRetry = ID_TRUE;
        return IDE_SUCCESS;
    }
    else
    {
        /* nothing to do */
    }

    IDL_MEM_BARRIER;

    if ( checkSMO( aIndexStat, 
                   aNode, 
                   aIndexSmoNo, 
                   aNodeSmoNo ) > 0 )
    {
        *aRetry = ID_TRUE;
        return IDE_SUCCESS;
    }
    else
    {
        /* nothing to do */
    }

    while ( sMinimum <= sMaximum )
    {
        sMedium = (sMinimum + sMaximum) >> 1;
        IDE_TEST( sdpSlotDirectory::getPagePtrFromSlotNum( sSlotDirPtr,
                                                           sMedium,
                                                           (UChar **)&sIKey )
                  != IDE_SUCCESS );
        sKey = SDNB_IKEY_KEY_PTR( sIKey );

        //PROJ-1872 Disk index ���� ���� ����ȭ
        //Compare(KeyFilter ����)�� ���� ���� �۾�.
        makeSmiValueListFromKeyValue( &(aIndex->mColLenInfoList),
                                      sKey,
                                      sSmiValueList );

        (void) aCallBack->callback( &sResult,
                                    sSmiValueList,
                                    NULL,
                                    0,
                                    SC_NULL_GRID,
                                    aCallBack->data );

        if ( sResult == ID_TRUE )
        {
            sMinimum = sMedium + 1;
        }
        else
        {
            sMaximum = sMedium - 1;
        }
    }

    sMedium = (sMinimum + sMaximum) >> 1;
    // ���� ����� ���� ���� callback�� �����ϴ� ������ slotNo
    if ( sMedium == -1 )
    {
        sNodeHdr    = (sdnbNodeHdr*)sdpPhyPage::getLogicalHdrStartPtr( (UChar *)aNode );
        *aChildNode = sNodeHdr->mLeftmostChild;
    }
    else
    {
        IDE_TEST( sdpSlotDirectory::getPagePtrFromSlotNum( sSlotDirPtr,
                                                           sMedium,
                                                           (UChar **)&sIKey )
                  != IDE_SUCCESS );
        SDNB_GET_RIGHT_CHILD_PID( aChildNode, sIKey );
    }

    IDL_MEM_BARRIER;

    if ( checkSMO( aIndexStat, 
                   aNode, 
                   aIndexSmoNo, 
                   aNodeSmoNo ) > 0 )
    {
        *aRetry = ID_TRUE;
        return IDE_SUCCESS;
    }
    else
    {
        /* nothing to do */
    }

    return IDE_SUCCESS;
    
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/*********************************************************************
 * FUNCTION DESCRIPTION : sdnbBTree::findLastLeafKey                 *
 * ------------------------------------------------------------------*
 * �־��� callback�� �ǰ��Ͽ� keyrange�� �ش��ϴ� ��� leaf slot��   *
 * �ٷ� �ڷ� Ŀ���� �̵���Ų��.                                      *
 * ���������� SMO üũ�� ���� �ʴ´�.
 *********************************************************************/
IDE_RC sdnbBTree::findLastLeafKey( const smiCallBack * aCallBack,
                                   sdnbHeader        * aIndex,
                                   UChar             * aPagePtr,
                                   SShort              aMinimum,
                                   SShort              aMaximum,
                                   SShort            * aSlot )
{

    SShort     sMedium;
    idBool     sResult;
    UChar    * sKey;
    UChar    * sSlot;
    UChar    * sSlotDirPtr;
    smiValue   sSmiValueList[ SMI_MAX_IDX_COLUMNS ];

    sSlotDirPtr = sdpPhyPage::getSlotDirStartPtr( aPagePtr );

    while ( aMinimum <= aMaximum )
    {
        sMedium = (aMinimum + aMaximum) >> 1;
        IDE_TEST( sdpSlotDirectory::getPagePtrFromSlotNum( sSlotDirPtr, 
                                                           sMedium,
                                                           &sSlot )
                    != IDE_SUCCESS );
        sKey = SDNB_LKEY_KEY_PTR(sSlot);

        //PROJ-1872 Disk index ���� ���� ����ȭ
        //Compare(KeyFilter ����)�� ���� ���� �۾�.
        makeSmiValueListFromKeyValue( &(aIndex->mColLenInfoList),
                                      sKey,
                                      sSmiValueList );

        (void) aCallBack->callback( &sResult,
                                    sSmiValueList,
                                    NULL,
                                    0,
                                    SC_NULL_GRID,
                                    aCallBack->data );

        if ( sResult == ID_TRUE )
        {
            aMinimum = sMedium + 1;
        }
        else
        {
            aMaximum = sMedium - 1;
        }
    }

    sMedium = ( aMinimum + aMaximum ) >> 1;
    // ���� ����� ���� ���� callback�� �����ϴ� ������ slotNo
    // �׷��Ƿ�, 1�� �����־�� ã������ afterLast��ġ�� �ȴ�.
    sMedium++;

    *aSlot = sMedium;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;


}

/*********************************************************************
 * FUNCTION DESCRIPTION : sdnbBTree::findSlotAtALeafNode             *
 * ------------------------------------------------------------------*
 *********************************************************************/
IDE_RC sdnbBTree::findSlotAtALeafNode( sdpPhyPageHdr * aNode,
                                       sdnbHeader    * aIndex,
                                       sdnbKeyInfo   * aKeyInfo,
                                       SShort        * aKeyMapSeq,
                                       idBool        * aFound )
{
    SShort                 sHigh;
    SShort                 sLow;
    SShort                 sMid;
    SInt                   sRet;
    sdnbLKey             * sLKey;
    sdnbKeyInfo            sKeyInfo;
    idBool                 sIsSameValue = ID_FALSE;
    UChar                * sSlotDirPtr;
    sdnbConvertedKeyInfo   sConvertedKeyInfo;
    smiValue               sSmiValueList[SMI_MAX_IDX_COLUMNS];

    *aFound = ID_FALSE;
    sConvertedKeyInfo.mSmiValueList = (smiValue*)sSmiValueList;
    SDNB_KEYINFO_TO_CONVERTED_KEYINFO( *aKeyInfo,
                                       sConvertedKeyInfo,
                                       &aIndex->mColLenInfoList );

    sLow        = 0;
    sSlotDirPtr = sdpPhyPage::getSlotDirStartPtr( (UChar *)aNode );
    sHigh       = sdpSlotDirectory::getCount( sSlotDirPtr ) - 1;

    while ( sLow <= sHigh )
    {
        sMid = (sLow + sHigh) >> 1;
        IDE_TEST( sdpSlotDirectory::getPagePtrFromSlotNum( sSlotDirPtr, 
                                                           sMid,
                                                           (UChar **)&sLKey )
                  != IDE_SUCCESS );

        SDNB_LKEY_TO_KEYINFO( sLKey, sKeyInfo );

        sRet = compareConvertedKeyAndKey( &aIndex->mQueryStat,
                                          aIndex->mColumns,
                                          aIndex->mColumnFence,
                                          &sConvertedKeyInfo,
                                          &sKeyInfo,
                                          ( SDNB_COMPARE_VALUE  |
                                            SDNB_COMPARE_PID    |
                                            SDNB_COMPARE_OFFSET ),
                                          &sIsSameValue );

        if ( sRet > 0 )
        {
            sLow = sMid + 1;
        }
        else if ( sRet == 0 )
        {
            *aFound = ID_TRUE;
            *aKeyMapSeq = sMid;
            break;
        }
        else
        {
            sHigh = sMid - 1;
        }
    } //while

    if ( *aFound == ID_FALSE )
    {
        sMid = (sLow + sHigh) >> 1;
        *aKeyMapSeq = sMid + 1;
    }
    else
    {
        /* nothing to do */
    }

    return IDE_SUCCESS;
    
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : sdnbBTree::makeRowCacheBackward            *
 * ------------------------------------------------------------------*
 * aNode�� aStartSlotSeq���� ���������� �����ϸ� aCallBack�� �����ϴ�*
 * ������ Slot���� mRowRID�� Key Value�� Row Cache�� �����Ѵ�.       *
 * Row Cache�� ����� minimum KeyRange�� ����� slot�� �߿���        *
 * Transaction Level Visibility�� ����ϴ� Slot���̴�.               *
 *********************************************************************/
IDE_RC sdnbBTree::makeRowCacheBackward( sdnbIterator *  aIterator,
                                        UChar *         aLeafNode,
                                        SShort          aStartSlotSeq )
{

    SInt                i;
    idBool              sResult;
    sdnbLKey          * sLKey;
    sdnbKeyInfo         sKeyInfo;
    SShort              sToSeq;
    UChar             * sKey;
    const smiCallBack * sMinimum = &aIterator->mKeyRange->minimum;
    const smiRange    * sKeyFilter;
    sdnbHeader        * sIndex;
    sdnbVisibility      sVisibility = SDNB_VISIBILITY_NO;
    UChar             * sSlotDirPtr;
    smiValue            sSmiValueList[ SMI_MAX_IDX_COLUMNS ];

    sIndex = (sdnbHeader*)((smnIndexHeader*)(aIterator->mIndex))->mHeader;

    // aIterator�� Row Cache�� �ʱ�ȭ �Ѵ�.
    aIterator->mCurRowPtr  = aIterator->mRowCache - 1;
    aIterator->mCacheFence = &aIterator->mRowCache[0];

    sSlotDirPtr = sdpPhyPage::getSlotDirStartPtr( aLeafNode );
    IDE_TEST( sdpSlotDirectory::getPagePtrFromSlotNum( sSlotDirPtr, 
                                                       0,
                                                       (UChar **)&sLKey )
              != IDE_SUCCESS );
    sKey = SDNB_LKEY_KEY_PTR( sLKey );

    //PROJ-1872 Disk index ���� ���� ����ȭ
    //Compare(KeyFilter ����)�� ���� ���� �۾�.
    makeSmiValueListFromKeyValue( &(sIndex->mColLenInfoList),
                                  sKey,
                                  sSmiValueList );

    (void)sMinimum->callback( &sResult,
                              sSmiValueList,
                              NULL,
                              0,
                              SC_NULL_GRID,
                              sMinimum->data );
    if ( sResult == ID_TRUE )
    {
        // ��峻�� ��� key�� minimumCallback�� �����Ѵ�.
        if ( sdpPhyPage::getPrvPIDOfDblList( sdpPhyPage::getHdr(aLeafNode) )
             != SD_NULL_PID )
        {
            aIterator->mIsLastNodeInRange = ID_FALSE;
            sToSeq = 0;
        }
        else
        {
            aIterator->mIsLastNodeInRange = ID_TRUE;
            sToSeq = 0;
        }
    }
    else
    {
        aIterator->mIsLastNodeInRange = ID_TRUE;
        IDE_ERROR( findFirstLeafKey ( sMinimum, 
                                      sIndex, 
                                      aLeafNode, 
                                      0, 
                                      aStartSlotSeq, 
                                      &sToSeq )
                   == IDE_SUCCESS );
        sToSeq++; // beforeFirst��ġ �̹Ƿ� 1�� ���� ó�� slot���� �̵��Ѵ�.
    }

    for ( i = aStartSlotSeq ; i >= sToSeq ; i-- )
    {
        IDE_TEST( sdpSlotDirectory::getPagePtrFromSlotNum( sSlotDirPtr, 
                                                           i, 
                                                           (UChar **)&sLKey )
                  != IDE_SUCCESS );

        sKey = SDNB_LKEY_KEY_PTR( sLKey );
        //PROJ-1872 Disk index ���� ���� ����ȭ
        //Compare(KeyFilter ����)�� ���� ���� �۾�.
        makeSmiValueListFromKeyValue( &(sIndex->mColLenInfoList),
                                      sKey,
                                      sSmiValueList );

        sKeyFilter = aIterator->mKeyFilter;
        while ( sKeyFilter != NULL )
        {
            sKeyFilter->minimum.callback( &sResult,
                                          sSmiValueList,
                                          NULL,
                                          0,
                                          SC_NULL_GRID,
                                          sKeyFilter->minimum.data);
            if ( sResult == ID_TRUE )
            {
                sKeyFilter->maximum.callback( &sResult,
                                              sSmiValueList,
                                              NULL,
                                              0,
                                              SC_NULL_GRID,
                                              sKeyFilter->maximum.data);
                if ( sResult == ID_TRUE )
                {
                    // minimum, maximum ��� ���.
                    break;
                }
                else
                {
                    /* nothing to do */
                }
            }
            else
            {
                /* nothing to do */
            }
            sKeyFilter = sKeyFilter->next;
        }// while

        if ( sResult == ID_FALSE )
        {
            continue;
        }
        else
        {
            /* nothing to do */
        }

        /* BUG-48353
           transLevelVisibility()���� Pending Wait ���� �ʴ´�. 
           ���� Pending Wait ������ Visibie ���θ� �˼��ִ� KEY�� �ִٸ�
           UNKNOWN ���� �Ǵ��� �ϴ� ĳ���� ����ȴ�. */
        sVisibility = SDNB_VISIBILITY_NO;
        IDE_TEST( transLevelVisibility( aIterator->mProperties->mStatistics,
                                        aIterator->mTrans,
                                        sIndex,
                                        aLeafNode,
                                        (UChar *)sLKey,
                                        &aIterator->mSCN,
                                        &sVisibility )
                  != IDE_SUCCESS );

        if ( sVisibility == SDNB_VISIBILITY_NO )
        {
            continue;
        }

        // save slot info
        SDNB_LKEY_TO_KEYINFO( sLKey, sKeyInfo );
        aIterator->mCacheFence->mRowPID = sKeyInfo.mRowPID;
        aIterator->mCacheFence->mRowSlotNum = sKeyInfo.mRowSlotNum;
        aIterator->mCacheFence->mOffset =
                              sdpPhyPage::getOffsetFromPagePtr( (UChar *)sLKey );
        idlOS::memcpy( &(aIterator->mPage[aIterator->mCacheFence->mOffset]),
                       (UChar *)sLKey,
                       getKeyLength( &(sIndex->mColLenInfoList),
                                     (UChar *)sLKey,
                                     ID_TRUE ) );

        aIterator->mPrevKey =
                 (sdnbLKey*)&(aIterator->mPage[aIterator->mCacheFence->mOffset]);

        if ( sVisibility == SDNB_VISIBILITY_UNKNOWN )
        {
            SDNB_SET_STATE( aIterator->mPrevKey, SDNB_CACHE_VISIBLE_UNKNOWN );
        }
        else /* SDNB_VISIBILITY_YES */
        {
            SDNB_SET_STATE( aIterator->mPrevKey, SDNB_CACHE_VISIBLE_YES );
        }

        // increase cache fence
        aIterator->mCacheFence++;
    }
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/*********************************************************************
 * FUNCTION DESCRIPTION : sdnbBTree::makeNextRowCacheForward         *
 * ------------------------------------------------------------------*
 * ���������� ��带 �̵��ϸ鼭, Cache�ϱ⿡ ������ ��������� �˻�  *
 * �Ѵ�. ���� mNextLeafNode�� ������ ���Ŀ� �������� ���ؼ� �ش�     *
 * ��尡 FREE�� ��쿡�� �ٽ� next node�� ���� ������ �ʿ��ϴ�.     *
 * mScanBackNode�� ������ �����߾��� ����� �������� �ʴ� ��带 �ǹ�*
 * �Ѵ�. ���� Node�� Free�� ��쿡�� mScanBackNode�� �̵��ؼ� next *
 * node�� ã�ƾ� �Ѵ�.                                               *
 * ���� mNextLeafNode�� ������ ���Ŀ� �ش� ��忡 SMO�� �߻�����     *
 * �ʾҴٸ� �ش� ��带 �����ϰ� ĳ���Ҽ� �ִ�.                      *
 *********************************************************************/
IDE_RC sdnbBTree::makeNextRowCacheForward( sdnbIterator * aIterator,
                                           sdnbHeader   * aIndex )
{
    sdpPhyPageHdr * sCurNode;
    idBool          sIsSameValue;
    SInt            sRet;
    sdnbKeyInfo     sCurKeyInfo;
    sdnbKeyInfo     sPrevKeyInfo;
    SShort          sSlotSeq = 0;
    sdnbLKey      * sLKey;
    UChar         * sSlotDirPtr;
    scPageID        sCurPageID;
    UInt            sState = 0;
    idBool          sIsSuccess;
    idBool          sFound;
    idvSQL        * sStatistics;
    sdpPhyPageHdr * sPrevNode;
    ULong           sPrevNodeSmoNo;

    sStatistics = aIterator->mProperties->mStatistics;

    if ( aIterator->mPrevKey != NULL )
    {
        SDNB_LKEY_TO_KEYINFO( aIterator->mPrevKey, sPrevKeyInfo );
    }
    else
    {
        /* nothing to do */
    }

    sCurPageID = aIterator->mNextLeafNode;

    while ( 1 )
    {
        if ( sCurPageID == SD_NULL_PID )
        {
            aIterator->mIsLastNodeInRange = ID_TRUE;
            IDE_CONT( RETURN_SUCCESS );
        }

        IDE_TEST( getPage( sStatistics,
                           &(aIndex->mQueryStat.mIndexPage),
                           aIndex->mIndexTSID,
                           sCurPageID,
                           SDB_S_LATCH,
                           SDB_WAIT_NORMAL,
                           NULL, /* aMtx */
                           (UChar **)&sCurNode,
                           &sIsSuccess )
                  != IDE_SUCCESS );
        sState = 1;

        // BUG-29132
        // when Previous Node and Next Node have been free and reused
        // the link between them can be same (not changed)
        // So, if SMO No of Prev Node is changed, iterator should
        // retraverse this index with previous fetched key
        if ( aIterator->mIndexSmoNo < sdpPhyPage::getIndexSMONo( sCurNode ) )
        {
            // try s-latch to previous node
            // if it fails, start retraversing
            IDE_TEST( getPage( sStatistics,
                               &(aIndex->mQueryStat.mIndexPage),
                               aIndex->mIndexTSID,
                               aIterator->mCurLeafNode,
                               SDB_S_LATCH,
                               SDB_WAIT_NO,
                               NULL, /* aMtx */
                               (UChar **)&sPrevNode,
                               &sIsSuccess )
                      != IDE_SUCCESS );

            if ( sIsSuccess == ID_TRUE )
            {
                sState = 2;
                sPrevNodeSmoNo = sdpPhyPage::getIndexSMONo(sPrevNode);
                sState = 1;
                IDE_TEST( sdbBufferMgr::releasePage( sStatistics,
                                                     (UChar *)sPrevNode )
                          != IDE_SUCCESS );
            }
            else
            {
                sPrevNodeSmoNo = ID_ULONG_MAX;
            }

            if ( ( aIterator->mIndexSmoNo < sPrevNodeSmoNo ) ||
                 ( sPrevNodeSmoNo == ID_ULONG_MAX )  )
            {
                sState = 0;
                IDE_TEST( sdbBufferMgr::releasePage( sStatistics,
                                                     (UChar *)sCurNode )
                          != IDE_SUCCESS );

                if ( aIterator->mScanBackNode != SD_NULL_PID )
                {
                    IDE_TEST( getPage( sStatistics,
                                       &(aIndex->mQueryStat.mIndexPage),
                                       aIndex->mIndexTSID,
                                       aIterator->mScanBackNode,
                                       SDB_S_LATCH,
                                       SDB_WAIT_NORMAL,
                                       NULL, /* aMtx */
                                       (UChar **)&sCurNode,
                                       &sIsSuccess )
                              != IDE_SUCCESS );
                    sState = 1;

                    aIterator->mCurLeafNode  = aIterator->mScanBackNode;
                    aIterator->mNextLeafNode = sCurNode->mListNode.mNext;
                    aIterator->mCurNodeSmoNo = sdpPhyPage::getIndexSMONo(sCurNode);
                    getSmoNo( (void *)aIndex, &aIterator->mIndexSmoNo );
                    IDL_MEM_BARRIER;
                    aIterator->mCurNodeLSN   = sdpPhyPage::getPageLSN(
                                   sdpPhyPage::getPageStartPtr((UChar *)sCurNode));
                }
                else
                {
                    IDE_TEST(findFirst(aIterator) != IDE_SUCCESS );
                    return IDE_SUCCESS;
                }
            }
            else
            {
                /* nothing to do */
            }
        }

        sSlotDirPtr = sdpPhyPage::getSlotDirStartPtr( (UChar *)sCurNode );
        if ( sdpSlotDirectory::getCount( sSlotDirPtr ) == 0 )
        {
            sCurPageID = sdpPhyPage::getNxtPIDOfDblList(sCurNode);

            sState = 0;
            IDE_TEST( sdbBufferMgr::releasePage( sStatistics,
                                                 (UChar *)sCurNode )
                      != IDE_SUCCESS );
        
            /* BUG-38216 detect hang on index module in abnormal state */
            IDE_TEST( iduCheckSessionEvent( sStatistics ) != IDE_SUCCESS );

            continue;
        }
        else
        {
            /* nothing to do */
        }

        if ( aIterator->mPrevKey != NULL )
        {
            IDE_TEST( sdpSlotDirectory::getPagePtrFromSlotNum( sSlotDirPtr, 
                                                               0, 
                                                               (UChar **)&sLKey )
                      != IDE_SUCCESS );
            SDNB_LKEY_TO_KEYINFO( sLKey, sCurKeyInfo );

            sRet = compareKeyAndKey( &aIndex->mQueryStat,
                                     aIndex->mColumns,
                                     aIndex->mColumnFence,
                                     &sPrevKeyInfo,
                                     &sCurKeyInfo,
                                     SDNB_COMPARE_VALUE |
                                     SDNB_COMPARE_PID   |
                                     SDNB_COMPARE_OFFSET,
                                     &sIsSameValue );

            if ( sRet >= 0 )
            {
                IDE_TEST( findSlotAtALeafNode( sCurNode,
                                               aIndex,
                                               &sPrevKeyInfo,
                                               &sSlotSeq,
                                               &sFound )
                          != IDE_SUCCESS );

                if ( sFound == ID_TRUE )
                {
                    sSlotSeq++;
                }
                else
                {
                    /* nothing to do */
                }

                if ( sSlotSeq >= sdpSlotDirectory::getCount( sSlotDirPtr ) )
                {
                    sCurPageID = sdpPhyPage::getNxtPIDOfDblList( sCurNode );

                    sState = 0;
                    IDE_TEST( sdbBufferMgr::releasePage( sStatistics,
                                                         (UChar *)sCurNode )
                              != IDE_SUCCESS );
                }
                else
                {
                    // ��峻�� cache�ؾ��� key���� �ִٸ�
                    break;
                }
            }
            else
            {
                // SMO�� ���Ͼ �����
                sSlotSeq = 0;
                break;
            }
        }
        else
        {
            // PrevSlot�� NULL�� ���� ���� ��带 cache
            sSlotSeq = 0;
            break;
        }
    }

    aIterator->mCurLeafNode  = sCurPageID;
    aIterator->mNextLeafNode = sCurNode->mListNode.mNext;
    aIterator->mCurNodeSmoNo = sdpPhyPage::getIndexSMONo(sCurNode);
    getSmoNo( (void *)aIndex, &aIterator->mIndexSmoNo );
    IDL_MEM_BARRIER;
    aIterator->mCurNodeLSN   = sdpPhyPage::getPageLSN(
                                sdpPhyPage::getPageStartPtr((UChar *)sCurNode));

    IDE_TEST( makeRowCacheForward( aIterator,
                                   (UChar *)sCurNode,
                                   sSlotSeq )
               != IDE_SUCCESS );

    sState = 0;
    IDE_TEST( sdbBufferMgr::releasePage( sStatistics,
                                         (UChar *)sCurNode )
              != IDE_SUCCESS );

    IDE_EXCEPTION_CONT( RETURN_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    switch ( sState )
    {
        case 2 :
            {
                if ( sdbBufferMgr::releasePage( sStatistics, (UChar *)sPrevNode )
                     != IDE_SUCCESS )
                {
                    dumpIndexNode( sPrevNode );
                    dumpIndexNode( sCurNode );
                    IDE_ASSERT( 0 );
                }
            }
        case 1 :
            {
                if ( sdbBufferMgr::releasePage( sStatistics, (UChar *)sCurNode )
                     != IDE_SUCCESS )
                {
                    dumpIndexNode( sCurNode );
                    IDE_ASSERT( 0 );
                }
            }
    }

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : sdnbBTree::beforeFirstW                    *
 * ------------------------------------------------------------------*
 * �־��� callback�� �ǰ��Ͽ� keyrange�� �ش��ϴ� leaf slot�� �ٷ�   *
 * ������ Ŀ���� �̵���Ų��.                                         *
 * �ַ� write lock���� traversing�Ҷ� ȣ��ȴ�.                      *
 * �ѹ� ȣ��� �Ŀ��� lock�� �ٽ� ���� �ʱ� ���� seekFunc�� �ٲ۴�.  *
 *********************************************************************/
IDE_RC sdnbBTree::beforeFirstW( sdnbIterator*       aIterator,
                                const smSeekFunc** aSeekFunc )
{
    IDE_TEST( sdnbBTree::beforeFirst( aIterator, aSeekFunc ) != IDE_SUCCESS );

    // Seek funstion set ����
    *aSeekFunc += 6;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : sdnbBTree::afterLastW                      *
 * ------------------------------------------------------------------*
 * �־��� callback�� �ǰ��Ͽ� keyrange�� �ش��ϴ� ��� leaf slot��   *
 * �ٷ� �ڷ� Ŀ���� �̵���Ų��.                                      *
 * �ַ� write lock���� traversing�Ҷ� ȣ��ȴ�.                      *
 * �ѹ� ȣ��� �Ŀ��� lock�� �ٽ� ���� �ʱ� ���� seekFunc�� �ٲ۴�.  *
 *********************************************************************/
IDE_RC sdnbBTree::afterLastW( sdnbIterator*       aIterator,
                              const smSeekFunc** aSeekFunc )
{
    IDE_TEST( sdnbBTree::afterLast( aIterator, aSeekFunc ) != IDE_SUCCESS );

    // Seek funstion set ����
    *aSeekFunc += 6;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : sdnbBTree::beforeFirstRR                   *
 * ------------------------------------------------------------------*
 * �־��� callback�� �ǰ��Ͽ� keyrange�� �ش�Ǵ� ��� Row�� lock��  *
 * �ɰ� ù slot�� �ٷ� ������ Ŀ���� �ٽ� �̵���Ų��.                *
 * �ַ� Repeatable read�� traversing�Ҷ� ȣ��ȴ�.                   *
 * �ѹ� ȣ��� ���Ŀ� �ٽ� ȣ����� �ʵ���  SeekFunc�� �ٲ۴�.       *
 *********************************************************************/
IDE_RC sdnbBTree::beforeFirstRR( sdnbIterator *       aIterator,
                                 const smSeekFunc ** aSeekFunc )
{

    sdnbIterator          sIterator;
    smiCursorProperties   sProp;
    smSCN                 sTempSCN;

    IDE_TEST( sdnbBTree::beforeFirst( aIterator, aSeekFunc ) != IDE_SUCCESS );

    idlOS::memcpy( &sIterator, 
                   aIterator, 
                   ID_SIZEOF(sdnbIterator) );
    idlOS::memcpy( &sProp, 
                   aIterator->mProperties, 
                   ID_SIZEOF(smiCursorProperties) );

    IDE_TEST( sdnbBTree::lockAllRows4RR( aIterator ) != IDE_SUCCESS );
    // beforefirst ���·� �ǵ��� ����.
    idlOS::memcpy( aIterator, 
                   &sIterator, 
                   ID_SIZEOF(sdnbIterator) );
    idlOS::memcpy( aIterator->mProperties, 
                   &sProp, 
                   ID_SIZEOF(smiCursorProperties) );

    /*
     * �ڽ��� ���� Lock Row�� ���� ���ؼ��� Cursor Infinite SCN��
     * 2���� ���Ѿ� �Ѵ�.
     */

    sTempSCN = aIterator->mInfinite;

    SM_ADD_INF_SCN( &sTempSCN );

    IDE_TEST_RAISE( SM_SCN_IS_LT( &sTempSCN, &aIterator->mInfinite) == ID_TRUE,
                    ERR_OVERFLOW );

    SM_ADD_INF_SCN( &aIterator->mInfinite );

    *aSeekFunc += 6;

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_OVERFLOW );
    {
        IDE_SET( ideSetErrorCode( smERR_ABORT_smiUpdateOverflow ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

/*********************************************************************
 * FUNCTION DESCRIPTION : sdnbBTree::afterLastRR                     *
 * ------------------------------------------------------------------*
 * �־��� callback�� �ǰ��Ͽ� keyrange�� �ش�Ǵ� ��� Row�� lock��  *
 * �ɰ� ������ slot�� �ٷ� �ڷ� Ŀ���� �ٽ� �̵���Ų��.              *
 * �ַ� Repeatable read�� traversing�Ҷ� ȣ��ȴ�.                   *
 *********************************************************************/
IDE_RC sdnbBTree::afterLastRR( sdnbIterator*       aIterator,
                               const smSeekFunc** aSeekFunc )
{

    sdnbIterator          sIterator;
    smiCursorProperties   sProp;
    smSCN                 sTempSCN;

    IDE_TEST( sdnbBTree::afterLast( aIterator, aSeekFunc ) != IDE_SUCCESS );

    idlOS::memcpy( &sIterator, 
                   aIterator, 
                   ID_SIZEOF(sdnbIterator) );
    idlOS::memcpy( &sProp, 
                   aIterator->mProperties, 
                   ID_SIZEOF(smiCursorProperties) );

    IDE_TEST( sdnbBTree::lockAllRows4RR( aIterator ) != IDE_SUCCESS );

    idlOS::memcpy( aIterator, 
                   &sIterator, 
                   ID_SIZEOF(sdnbIterator) );
    idlOS::memcpy( aIterator->mProperties, 
                   &sProp, 
                   ID_SIZEOF(smiCursorProperties) );

    /*
     * �ڽ��� ���� Lock Row�� ���� ���ؼ��� Cursor Infinite SCN��
     * 2���� ���Ѿ� �Ѵ�.
     */
    sTempSCN = aIterator->mInfinite;

    SM_ADD_INF_SCN( &sTempSCN );

    IDE_TEST_RAISE( SM_SCN_IS_LT( &sTempSCN, &aIterator->mInfinite) == ID_TRUE,
                    ERR_OVERFLOW );

    SM_ADD_INF_SCN( &aIterator->mInfinite );

    *aSeekFunc += 6;

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_OVERFLOW );
    {
        IDE_SET( ideSetErrorCode( smERR_ABORT_smiUpdateOverflow ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

/*********************************************************************
 * FUNCTION DESCRIPTION : sdnbBTree::restoreCursorPosition           *
 * ------------------------------------------------------------------*
 * ������ ����� Iterator�� ��ġ�� ���� index leaf node�� slot��ȣ�� *
 * ���Ѵ�. leaf node���� x latch�� ���� ���·� ��ȯ�Ѵ�.             *
 *********************************************************************/
IDE_RC sdnbBTree::restoreCursorPosition( const smnIndexHeader   *aIndexHeader,
                                         sdrMtx                 *aMtx,
                                         sdnbIterator           *aIterator,
                                         idBool                  aSmoOccurred,
                                         sdpPhyPageHdr         **aNode,
                                         SShort                 *aSlotSeq )
{
    ULong              sTempKeyValueBuf[SDNB_MAX_KEY_BUFFER_SIZE/ID_SIZEOF(ULong)];
    sdnbStack          sStack;
    UChar            * sSlot;
    idBool             sIsSuccess;
    sdnbKeyInfo        sKeyInfo;
    idBool             sIsPageLatchReleased = ID_TRUE;
    idBool             sIsRowDeleted = ID_FALSE;
    sdrSavePoint       sSP;
    sdnbHeader        *sIndex     = (sdnbHeader*)aIndexHeader->mHeader;
    sdSID              sTSSlotSID = smLayerCallback::getTSSlotSID( sdrMiniTrans::getTrans( aMtx ) );
    sdSID              sRowSID    = SD_MAKE_SID( aIterator->mCurRowPtr->mRowPID,
                                                 aIterator->mCurRowPtr->mRowSlotNum );
    SLong              sTotalTraverseLength = 0;

    sdrMiniTrans::setSavePoint( aMtx, &sSP );

    IDE_TEST( sdbBufferMgr::getPageBySID( aIterator->mProperties->mStatistics,
                                          sIndex->mTableTSID,
                                          sRowSID,
                                          SDB_S_LATCH,
                                          SDB_WAIT_NORMAL,
                                          aMtx,
                                          (UChar **)&sSlot,
                                          &sIsSuccess )
              != IDE_SUCCESS );
    sIsPageLatchReleased = ID_FALSE;

    IDE_TEST( makeKeyValueFromRow( aIterator->mProperties->mStatistics,
                                   aMtx,
                                   &sSP,
                                   aIterator->mTrans,
                                   aIterator->mTable,
                                   aIndexHeader,
                                   sSlot,
                                   SDB_SINGLE_PAGE_READ,
                                   sIndex->mTableTSID,
                                   SMI_FETCH_VERSION_CONSISTENT,
                                   sTSSlotSID,
                                   &(aIterator->mSCN),
                                   &(aIterator->mInfinite),
                                   (UChar *)sTempKeyValueBuf,
                                   &sIsRowDeleted,
                                   &sIsPageLatchReleased )
              != IDE_SUCCESS );

    if ( sIsRowDeleted != ID_FALSE )
    {
        dumpHeadersAndIteratorToSMTrc( (smnIndexHeader*)aIndexHeader,
                                        sIndex,
                                        aIterator );

        IDE_ASSERT( 0 );
    }
    else
    {
        /* nothing to do */
    }

    /* BUG-23319
     * [SD] �ε��� Scan�� sdcRow::fetch �Լ����� Deadlock �߻����ɼ��� ����. */
    /* row fetch�� �ϴ��߿� next rowpiece�� �̵��ؾ� �ϴ� ���,
     * ���� page�� latch�� Ǯ�� ������ deadlock �߻����ɼ��� �ִ�.
     * �׷��� page latch�� Ǭ ���� next rowpiece�� �̵��ϴµ�,
     * ���� �Լ������� page latch�� Ǯ������ ���θ� output parameter�� Ȯ���ϰ�
     * ��Ȳ�� ���� ������ ó���� �ؾ� �Ѵ�. */
    if ( sIsPageLatchReleased == ID_FALSE )
    {
        sIsPageLatchReleased = ID_TRUE;
        IDE_TEST( sdrMiniTrans::commit( aMtx ) != IDE_SUCCESS );


        IDE_TEST( sdrMiniTrans::begin( aIterator->mProperties->mStatistics,
                                       aMtx,
                                       aIterator->mTrans,
                                       SDR_MTX_LOGGING,
                                       ID_FALSE,/*Undoable(PROJ-2162)*/
                                       gMtxDLogType ) != IDE_SUCCESS );
    }

    if ( aSmoOccurred == ID_TRUE )
    {
        initStack( &sStack );
    }
    else
    {
        // trick -,.-;;
        // traverse()�Լ��� stack�� depth�� -1�� �ƴѰ��,
        // �� �ٷ� �Ʒ�(stack+1)�� node���� �ٽ� �Ѵ�.
        sStack.mIndexDepth = sStack.mCurrentDepth = 0;
        sStack.mStack[1].mNode = aIterator->mCurLeafNode;
    }

    sKeyInfo.mKeyValue   = (UChar *)sTempKeyValueBuf;
    sKeyInfo.mRowPID     = aIterator->mCurRowPtr->mRowPID;
    sKeyInfo.mRowSlotNum = aIterator->mCurRowPtr->mRowSlotNum;

    IDE_TEST( traverse( aIterator->mProperties->mStatistics,
                        sIndex,
                        aMtx,
                        &sKeyInfo,
                        &sStack,
                        &(sIndex->mQueryStat),
                        ID_FALSE, /* aPessimistic */
                        aNode,
                        aSlotSeq,
                        NULL,     /* aLeafSP */
                        NULL,     /* aIsSameKey */
                        &sTotalTraverseLength )
              != IDE_SUCCESS );

    if ( (*aNode) == NULL )
    {
        dumpHeadersAndIteratorToSMTrc( (smnIndexHeader*)aIndexHeader,
                                        sIndex,
                                        aIterator );

        ideLog::log( IDE_ERR_0,
                     "key sequence : %"ID_INT32_FMT"\n",
                     aSlotSeq );
        ideLog::log( IDE_ERR_0, "Index stack dump:\n" );
        ideLog::logMem( IDE_DUMP_0, (UChar *)&sStack, ID_SIZEOF(sdnbStack) );

        ideLog::log( IDE_ERR_0, "Key info dump:\n" );
        ideLog::logMem( IDE_DUMP_0, (UChar *)&sKeyInfo, ID_SIZEOF(sdnbKeyInfo) );
        IDE_ASSERT( 0 );
    }
    else
    {
        /* nothing to do */
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

/*********************************************************************
 * FUNCTION DESCRIPTION : sdnbBTree::fetchNext                       *
 * ------------------------------------------------------------------*
 * ĳ�õǾ� �ִ� Row�� �ִٸ� ĳ�ÿ��� �ְ�, ���� ���ٸ� Next Node�� *
 * ĳ���Ѵ�.                                                         *
 *********************************************************************/
IDE_RC sdnbBTree::fetchNext( sdnbIterator   * aIterator,
                             const void    ** aRow )
{
    sdnbHeader  * sIndex;
    idvSQL      * sStatistics;
    idBool        sNeedMoreCache;

    sIndex      = (sdnbHeader*)((smnIndexHeader*)(aIterator->mIndex))->mHeader;
    sStatistics = aIterator->mProperties->mStatistics;

  read_from_cache:
    IDE_TEST( fetchRowCache( aIterator,
                             aRow,
                             &sNeedMoreCache ) != IDE_SUCCESS );

    IDE_TEST_CONT( sNeedMoreCache == ID_FALSE, SKIP_CACHE );

    if ( aIterator->mIsLastNodeInRange == ID_TRUE )  // Range�� ���� ������.
    {
        if ( aIterator->mKeyRange->next != NULL ) // next key range�� �����ϸ�
        {
            aIterator->mKeyRange = aIterator->mKeyRange->next;

            IDE_TEST( iduCheckSessionEvent(sStatistics) != IDE_SUCCESS );

            IDE_TEST( beforeFirstInternal( aIterator ) != IDE_SUCCESS );

            goto read_from_cache;
        }
        else
        {
            // Ŀ���� ���¸� after last���·� �Ѵ�.
            aIterator->mCurRowPtr = aIterator->mCacheFence;

            *aRow = NULL;
            SC_MAKE_NULL_GRID( aIterator->mRowGRID );

            return IDE_SUCCESS;
        }
    }
    else
    {
        IDE_TEST( iduCheckSessionEvent( sStatistics ) != IDE_SUCCESS );

        /* FIT/ART/sm/Bugs/BUG-29132/BUG-29132.tc */
        IDU_FIT_POINT( "1.BUG-29132@sdnbBTree::fetchNext" );

        IDE_TEST( makeNextRowCacheForward( aIterator, sIndex )
                  != IDE_SUCCESS );

        goto read_from_cache;
    }

    IDE_EXCEPTION_CONT( SKIP_CACHE );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/*********************************************************************
 * FUNCTION DESCRIPTION : sdnbBTree::fetchPrev                       *
 * ------------------------------------------------------------------*
 * ĳ�õǾ� �ִ� Row�� �ִٸ� ĳ�ÿ��� �ְ�, ���� ���ٸ� Prev Node�� *
 * ĳ���Ѵ�.                                                         *
 *********************************************************************/
IDE_RC sdnbBTree::fetchPrev( sdnbIterator   * aIterator,
                             const void    ** aRow )
{
    sdnbHeader  * sIndex;
    idvSQL      * sStatistics;
    idBool        sNeedMoreCache;

    sIndex       = (sdnbHeader*)((smnIndexHeader*)(aIterator->mIndex))->mHeader;
    sStatistics  = aIterator->mProperties->mStatistics;

 read_from_cache:
    IDE_TEST( fetchRowCache( aIterator,
                             aRow,
                             &sNeedMoreCache ) != IDE_SUCCESS );

    IDE_TEST_CONT( sNeedMoreCache == ID_FALSE, SKIP_CACHE );

    if ( aIterator->mIsLastNodeInRange == ID_TRUE )  // Range�� ���� ������.
    {
        if ( aIterator->mKeyRange->prev != NULL ) // next key range�� �����ϸ�
        {
            aIterator->mKeyRange = aIterator->mKeyRange->prev;

            IDE_TEST( iduCheckSessionEvent( sStatistics ) != IDE_SUCCESS );

            IDE_TEST( afterLastInternal(aIterator) != IDE_SUCCESS );

            goto read_from_cache;
        }
        else
        {
            // Ŀ���� ���¸� before first ���·� �Ѵ�.
            aIterator->mCurRowPtr = aIterator->mCacheFence;

            *aRow = NULL;
            SC_MAKE_NULL_GRID( aIterator->mRowGRID );

            return IDE_SUCCESS;
        }
    }
    else
    {
        IDE_TEST( iduCheckSessionEvent(sStatistics) != IDE_SUCCESS );

        IDE_TEST( makeNextRowCacheBackward( aIterator, sIndex )
                  != IDE_SUCCESS );

        goto read_from_cache;
    }

    IDE_EXCEPTION_CONT( SKIP_CACHE );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : sdnbBTree::fetchRowCache                   *
 * ------------------------------------------------------------------*
 * ���̺�� ���� �ڽſ��� �´� Stable Version�� ����               *
 * 1. cursor level visibility �˻�                                   *
 * 2. Row Filter ����                                                *
 *********************************************************************/
IDE_RC sdnbBTree::fetchRowCache( sdnbIterator  * aIterator,
                                 const void   ** aRow,
                                 idBool        * aNeedMoreCache )
{
    UChar           * sDataSlot;
    UChar           * sDataPage;
    UChar           * sLeafPage;
    idBool            sIsSuccess;
    idBool            sResult;
    sdSID             sRowSID;
    smSCN             sMyFstDskViewSCN;
    smSCN             sFstDskViewSCN;
    idBool            sIsVisible;
    sdnbHeader      * sIndex;
    scSpaceID         sTableTSID;
    sdcRowHdrInfo     sRowHdrInfo;
    idBool            sIsRowDeleted;
    sdnbLKey        * sLeafKey;
    UChar           * sKey;
    idvSQL          * sStatistics;
    sdSID             sTSSlotSID;
    idBool            sIsMyTrans;
    UChar           * sDataSlotDir;
    idBool            sIsPageLatchReleased = ID_TRUE;
    smiValue          sSmiValueList[SMI_MAX_IDX_COLUMNS];
    sdnbColumn      * sColumn;
    UInt              i;

    sIndex           = (sdnbHeader*)
                       ((smnIndexHeader*)(aIterator->mIndex))->mHeader;
    sTableTSID       = sIndex->mTableTSID;
    sStatistics      = aIterator->mProperties->mStatistics;
    sTSSlotSID       = smLayerCallback::getTSSlotSID( aIterator->mTrans );
    sMyFstDskViewSCN = smLayerCallback::getFstDskViewSCN( aIterator->mTrans );

    *aNeedMoreCache = ID_TRUE;

    /* ĳ�� ����� KEY���� ��ȸ�Ѵ�. */
    while ( ( aIterator->mCurRowPtr + 1 ) < aIterator->mCacheFence )
    {
        IDE_TEST( iduCheckSessionEvent( sStatistics ) != IDE_SUCCESS );
        aIterator->mCurRowPtr++;

        sRowSID = SD_MAKE_SID( aIterator->mCurRowPtr->mRowPID,
                               aIterator->mCurRowPtr->mRowSlotNum );
        sLeafKey = (sdnbLKey*)&(aIterator->mPage[aIterator->mCurRowPtr->mOffset]);

        /* BUG-32017 [sm-disk-index] Full index scan in DRDB
         * Visible Yes�̸� FullIndexScan�� ������ ��Ȳ�̸�, Disk Table��
         * �������� �ʾƵ� �ȴ�. */
        if ( ( aIterator->mFullIndexScan == ID_TRUE ) &&
             ( SDNB_GET_STATE( sLeafKey ) == SDNB_CACHE_VISIBLE_YES ) )
        {
            sKey = SDNB_LKEY_KEY_PTR( sLeafKey );
            makeSmiValueListFromKeyValue( &(sIndex->mColLenInfoList),
                                          sKey,
                                          sSmiValueList );

            for ( sColumn = sIndex->mColumns, i = 0 ;
                  sColumn != sIndex->mColumnFence ;
                  sColumn++ )
            {
                sColumn->mCopyDiskColumnFunc(
                                sColumn->mVRowColumn.size,
                                ((UChar *)*aRow) + sColumn->mVRowColumn.offset,
                                0, //aCopyOffset
                                sSmiValueList[ i ].length,
                                sSmiValueList[ i ].value  );
                i ++;
            }
        }
        else
        {
            /* KEY�� ������ ������ �������� getPage �Ѵ�. */
            IDE_TEST_RAISE( getPage( sStatistics,
                                     &(sIndex->mQueryStat.mIndexPage),
                                     sTableTSID,
                                     SD_MAKE_PID( sRowSID ),
                                     SDB_S_LATCH,
                                     SDB_WAIT_NORMAL,
                                     NULL, /* aMtx */
                                     (UChar **)&sDataPage,
                                     &sIsSuccess )
                            != IDE_SUCCESS, ERR_CHECK_AND_DUMP );
            sIsPageLatchReleased = ID_FALSE;

            sDataSlotDir = sdpPhyPage::getSlotDirStartPtr( sDataPage );

            /*
            * BUG-27163 [SM] IDE_ASSERT(SDP_IS_UNUSED_SLOT_ENTRY(sSlotEntry) 
            * != ID_TRUE)�� ������ ������ �����մϴ�.
            * INSERT �ѹ��� Ʈ����� ��ü���� ���ڵ带 UNUSED���·� �����ϱ�
            * ������ VISIBLE_UNKOWN�� ��쿡 ���ؼ� �ش� SLOT�� UNUSED��������
            * �˻��ؾ� �Ѵ�.
             */
            if ( sdpSlotDirectory::isUnusedSlotEntry( sDataSlotDir,
                                                      SD_MAKE_SLOTNUM( sRowSID ) )
                 == ID_TRUE )
            {
                if ( SDNB_GET_STATE( sLeafKey ) == SDNB_CACHE_VISIBLE_YES ) 
                {
                    /* BUG-31845 [sm-disk-index] Debugging information is needed 
                     * for PBT when fail to check visibility using DRDB Index.
                     * ���� �߻��� ������ ����մϴ�. */
                    dumpHeadersAndIteratorToSMTrc( 
                                            (smnIndexHeader*)aIterator->mIndex,
                                            sIndex,
                                            aIterator );

                    if ( sIsPageLatchReleased == ID_FALSE )
                    {
                        ideLog::logMem( IDE_DUMP_0, 
                                        (UChar *)sDataPage,
                                        SD_PAGE_SIZE,
                                        "Data page dump :\n" );

                        sIsPageLatchReleased = ID_TRUE;
                        IDE_ASSERT( sdbBufferMgr::releasePage( sStatistics,
                                                               (UChar *)sDataPage )
                                    == IDE_SUCCESS );
                    }
                    else
                    {
                        /* nothing to do */
                    }

                    /* Index cache�����δ� page�� ���¸� ��Ȯ�� �� �� �����ϴ�.
                     * ���� LeafNode�� getpage�Ͽ� �ش� Page�� �о� ����ϴ�.
                     * (���� ���� latch�� Ǭ �����̱� ������, Cache�� ������
                     *  �ٸ� �� �ִٴ� ���� �����ؾ� �մϴ�. */
                    IDE_ASSERT_MSG( sdbBufferMgr::getPageByPID( sStatistics,
                                                                sIndex->mIndexTSID,
                                                                aIterator->mCurLeafNode,
                                                                SDB_S_LATCH,
                                                                SDB_WAIT_NORMAL,
                                                                SDB_SINGLE_PAGE_READ,
                                                                NULL, /* aMtx */
                                                                (UChar **)&sLeafPage,
                                                                &sIsSuccess,
                                                                NULL /*IsCorruptPage*/ )
                                    == IDE_SUCCESS,
                                    "Error Ouccurs in DRDB Index ID : %"ID_UINT32_FMT, sIndex->mIndexID );

                    ideLog::logMem( IDE_DUMP_0, 
                                    (UChar *)sLeafPage,
                                    SD_PAGE_SIZE,
                                    "Leaf Node dump :\n" );

                    IDE_ASSERT( sdbBufferMgr::releasePage( sStatistics,
                                                           (UChar *)sLeafPage )
                                == IDE_SUCCESS );

                    IDE_ERROR( 0 );
                }
                else
                {
                    /* nothing to do */
                }

                sIsPageLatchReleased = ID_TRUE;
                IDE_TEST( sdbBufferMgr::releasePage( sStatistics,
                                                     (UChar *)sDataPage )
                          != IDE_SUCCESS );
                continue;
            }
            else
            {
                /* nothing to do */
            }

            IDU_FIT_POINT_RAISE( "BUG-45979@sdnbBTree::fetchRowCache::getPagePtrFromSlotNum",
                                   ERR_CHECK_AND_DUMP );
            IDE_TEST_RAISE( sdpSlotDirectory::getPagePtrFromSlotNum( 
                                                    sDataSlotDir,
                                                    SD_MAKE_SLOTNUM( sRowSID ),
                                                    &sDataSlot )
                            != IDE_SUCCESS, ERR_CHECK_AND_DUMP );

            /* BUG-46068 : AGING�� DATA slot�� ����ǰ�, ù��° record piece�� �ƴϸ� SKIP�Ѵ�.
                           (ù��° record piece�� ���� �����ڵ忡�� Ȯ���� SKIP�Ѵ�.) */
            if ( sdcRow::isHeadRowPiece( sDataSlot ) == ID_FALSE )
            {
                if( sIsPageLatchReleased == ID_FALSE )
                {
                    sIsPageLatchReleased = ID_TRUE;
                    IDE_TEST( sdbBufferMgr::releasePage( sStatistics,
                                                         (UChar*)sDataPage )
                              != IDE_SUCCESS );
                }
                continue;
            }

            IDE_TEST_RAISE( makeVRowFromRow( sStatistics,
                                             NULL, /* aMtx */
                                             NULL, /* aSP */
                                             aIterator->mTrans,
                                             aIterator->mTable,
                                             sTableTSID,
                                             sDataSlot,
                                             SDB_SINGLE_PAGE_READ,
                                             aIterator->mProperties->mFetchColumnList,
                                             SMI_FETCH_VERSION_CONSISTENT,
                                             sTSSlotSID,
                                             &(aIterator->mSCN),
                                             &(aIterator->mInfinite),
                                             (UChar *)*aRow, /* [out] buffer */
                                             &sIsRowDeleted,
                                             &sIsPageLatchReleased )
                            != IDE_SUCCESS, ERR_CHECK_AND_DUMP );

            if ( sIsRowDeleted == ID_TRUE )
            {
                if ( sIsPageLatchReleased == ID_FALSE )
                {
                    sIsPageLatchReleased = ID_TRUE;
                    IDE_TEST( sdbBufferMgr::releasePage( sStatistics,
                                                         (UChar *)sDataPage )
                              != IDE_SUCCESS );
                }
                else
                {
                    /* nothing to do */
                }
                continue;
            }
            else
            {
                /* nothing to do */
            }

            if ( ( aIterator->mFlag & SMI_ITERATOR_TYPE_MASK )
                 == SMI_ITERATOR_WRITE )
            {
                /* BUG-23319
                 * [SD] �ε��� Scan�� sdcRow::fetch �Լ����� Deadlock �߻����ɼ��� ����. */
                /* row fetch�� �ϴ��߿� next rowpiece�� �̵��ؾ� �ϴ� ���,
                 * ���� page�� latch�� Ǯ�� ������ deadlock �߻����ɼ��� �ִ�.
                 * �׷��� page latch�� Ǭ ���� next rowpiece�� �̵��ϴµ�,
                 * ���� �Լ������� page latch�� Ǯ������ ���θ� output parameter�� Ȯ���ϰ�
                 * ��Ȳ�� ���� ������ ó���� �ؾ� �Ѵ�. */
                if ( sIsPageLatchReleased == ID_TRUE )
                {
                    IDE_TEST_RAISE( getPage( sStatistics,
                                             &(sIndex->mQueryStat.mIndexPage),
                                             sTableTSID,
                                             SD_MAKE_PID( sRowSID ),
                                             SDB_S_LATCH,
                                             SDB_WAIT_NORMAL,
                                             NULL, /* aMtx */
                                             (UChar **)&sDataPage,
                                             &sIsSuccess )
                              != IDE_SUCCESS, ERR_CHECK_AND_DUMP );
                    sIsPageLatchReleased = ID_FALSE;

                    sDataSlotDir = sdpPhyPage::getSlotDirStartPtr( sDataPage );
                    IDE_TEST_RAISE( sdpSlotDirectory::getPagePtrFromSlotNum( 
                                                          sDataSlotDir,
                                                          SD_MAKE_SLOTNUM( sRowSID ),
                                                          &sDataSlot )
                                    != IDE_SUCCESS, ERR_CHECK_AND_DUMP );
                }
                else
                {
                    /* nothing to do */
                }

                sIsMyTrans = sdcRow::isMyTransUpdating( sDataPage,
                                                        sDataSlot,
                                                        sMyFstDskViewSCN,
                                                        sTSSlotSID,
                                                        &sFstDskViewSCN );

                if ( sIsMyTrans == ID_TRUE )
                {
                    sdcRow::getRowHdrInfo( sDataSlot, &sRowHdrInfo );

                    if ( SM_SCN_IS_GE( &sRowHdrInfo.mInfiniteSCN,
                                       &aIterator->mInfinite ) )
                    {
                        sIsPageLatchReleased = ID_TRUE;
                        IDE_TEST( sdbBufferMgr::releasePage( sStatistics,
                                                             (UChar *)sDataPage )
                                  != IDE_SUCCESS );

                        continue;
                    }
                    else
                    {
                        /* nothing to do */
                    }
                }
                else
                {
                    /* nothing to do */
                }
            }
            else
            {
                /* nothing to do */
            }

            if ( sIsPageLatchReleased == ID_FALSE )
            {
                sIsPageLatchReleased = ID_TRUE;
                IDE_TEST( sdbBufferMgr::releasePage( sStatistics,
                                                     (UChar *)sDataPage )
                          != IDE_SUCCESS );
            }
            else
            {
                /* nothing to do */
            }

            sIsVisible = ID_TRUE;

            if ( SDNB_GET_STATE( sLeafKey ) == SDNB_CACHE_VISIBLE_UNKNOWN )
            {
                IDE_DASSERT( sIsRowDeleted == ID_FALSE );
                IDE_TEST_RAISE( cursorLevelVisibility( aIterator->mIndex,
                                                       &(sIndex->mQueryStat),
                                                       (UChar *)*aRow,
                                                       (UChar *)sLeafKey,
                                                       &sIsVisible )
                                != IDE_SUCCESS, ERR_CHECK_AND_DUMP );
            }
#if defined(DEBUG)
            /* BUG-31845 [sm-disk-index] Debugging information is needed for 
             * PBT when fail to check visibility using DRDB Index.
             * Unknown�� �ƴϸ�, ������ �� �� �־�� �մϴ�. */
            else
            {
                IDE_DASSERT( SDNB_GET_STATE( sLeafKey ) 
                             == SDNB_CACHE_VISIBLE_YES );
                IDE_DASSERT( sIsRowDeleted == ID_FALSE );

                IDE_TEST_RAISE( cursorLevelVisibility( aIterator->mIndex,
                                                       &(sIndex->mQueryStat),
                                                       (UChar *)*aRow,
                                                       (UChar *)sLeafKey,
                                                       &sIsVisible )
                                != IDE_SUCCESS, ERR_CHECK_AND_DUMP );
                IDE_DASSERT( sIsVisible == ID_TRUE );
            }
#endif


            if ( sIsVisible == ID_FALSE )
            {
                continue;
            }
            else
            {
                /* nothing to do */
            }
        }

        SC_MAKE_GRID_WITH_SLOTNUM( aIterator->mRowGRID,
                                   sTableTSID,
                                   SD_MAKE_PID( sRowSID ),
                                   SD_MAKE_SLOTNUM( sRowSID ) );

        aIterator->mScanBackNode = aIterator->mCurLeafNode;

        IDE_TEST( aIterator->mRowFilter->callback(
                                    &sResult,
                                    *aRow,
                                    NULL,
                                    0,
                                    aIterator->mRowGRID,
                                    aIterator->mRowFilter->data ) != IDE_SUCCESS );

        if ( sResult == ID_FALSE )
        {
            continue;
        }
        else
        {
            /* nothing to do */
        }

        // skip count ��ŭ row �ǳʶ�
        if ( aIterator->mProperties->mFirstReadRecordPos > 0 )
        {
            aIterator->mProperties->mFirstReadRecordPos--;
        }
        else if ( aIterator->mProperties->mReadRecordCount > 0 )
        {
            aIterator->mProperties->mReadRecordCount--;
            *aNeedMoreCache = ID_FALSE;

            break;
        }
        else
        {
            // Ŀ���� ���¸� after last���·� �Ѵ�.
            aIterator->mCurRowPtr = aIterator->mCacheFence;

            *aRow = NULL;
            SC_MAKE_NULL_GRID( aIterator->mRowGRID );

            *aNeedMoreCache = ID_FALSE;
            break;
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_CHECK_AND_DUMP )
    {
        /* �ܼ� ���� timeout ���� ���ܰ� �߻��Ҽ� �־ (iduCheckSessionEvent)
         * Consistent ������ ���� ����. */
        switch( ideGetErrorCode() )
        {
        case idERR_ABORT_Session_Disconnected:  /* SessionTimeOut�̴� �������� */
        case idERR_ABORT_Session_Closed:        /* ���� ���� */
        case idERR_ABORT_Query_Timeout:         /* �ð� �ʰ� */
        case idERR_ABORT_Query_Canceled:        /* ��� */
        case idERR_ABORT_IDU_MEMORY_ALLOCATION: /* �޸��Ѱ��Ȳ�� ���� */
        case idERR_ABORT_InsufficientMemory:    /* �޸��Ѱ��Ȳ�� ���� */
            break;
        default:
         /* �׿� ���� ������ Ȯ���غ��� */
            ideLog::log( IDE_ERR_0,
                         "Error : Cannot create Row cache because of ERR-%05X (error=%"ID_UINT32_FMT") %s\n",
                          E_ERROR_CODE( ideGetErrorCode() ) ,
                          ideGetSystemErrno(),
                          ideGetErrorMsg( ideGetErrorCode() )  );

            dumpHeadersAndIteratorToSMTrc( (smnIndexHeader*)aIterator->mIndex,
                                           sIndex,
                                           aIterator );
            break;
        }
    }
    IDE_EXCEPTION_END;

    if ( sIsPageLatchReleased == ID_FALSE )
    {
        (void)sdbBufferMgr::releasePage( sStatistics, (UChar*)sDataPage );
    }

    return IDE_FAILURE;

}


/*********************************************************************
 * FUNCTION DESCRIPTION : sdnbBTree::makeNextRowCacheBackward        *
 * ------------------------------------------------------------------*
 * ���������� ��带 �̵��ϸ鼭, Cache�ϱ⿡ ������ ��������� �˻�  *
 * �Ѵ�. ���� mNextLeafNode�� ������ ���Ŀ� �������� ���ؼ� �ش�     *
 * ��尡 SPLIT/KEY_REDISTRIBUTE/FREE�� ��쿡�� �ٽ� next node��    *
 * ���� ������ �ʿ��ϴ�.                                             *
 * ��ġ�� ȹ���ϸ鼭 ���������� �̵��Ҽ� ���� ������ traverse�� ���� *
 * ���� ĳ���� ��带 ã��, ã�� ���� ���� �ٽ� mNextLeafNode��    *
 * ���Ѵ�.
 *********************************************************************/
IDE_RC sdnbBTree::makeNextRowCacheBackward(sdnbIterator *aIterator,
                                           sdnbHeader   *aIndex)
{
    sdrMtx            sMtx;
    idBool            sIsSuccess;
    sdpPhyPageHdr   * sCurrentNode;
    sdpPhyPageHdr   * sTraverseNode = NULL;
    scPageID          sCurrentPID;
    ULong             sCurrentSmoNo;
    sdpPhyPageHdr   * sPrevNode;
    scPageID          sPrevPID;
    sdnbKeyInfo       sKeyInfo;
    SShort            sSlotSeq;
    sdnbStack         sStack;
    idBool            sMtxStart = ID_FALSE;
    UChar           * sSlotDirPtr;
    idvSQL          * sStatistics;
    UInt              sState = 0;
    idBool            sIsSameKey = ID_FALSE;
    SChar           * sOutBuffer4Dump;
    SLong             sTotalTraverseLength = 0;

    sStatistics = aIterator->mProperties->mStatistics;
    sCurrentPID = aIterator->mCurLeafNode;
    sCurrentSmoNo = aIterator->mCurNodeSmoNo;
    sPrevPID    = aIterator->mNextLeafNode; // ����� prevNode PID��.

    while (1)
    {
        if ( sPrevPID == SD_NULL_PID )
        {
            // ������ �����.
            aIterator->mIsLastNodeInRange = ID_TRUE;
            break;
        }
        else
        {
            /* nothing to do */
        }


        IDE_TEST( getPage( sStatistics,
                           &(aIndex->mQueryStat.mIndexPage),
                           aIndex->mIndexTSID,
                           sPrevPID,
                           SDB_S_LATCH,
                           SDB_WAIT_NORMAL,
                           NULL, /* aMtx */
                           (UChar **)&sPrevNode,
                           &sIsSuccess )
                  != IDE_SUCCESS );
        sState = 1;

        if ( sdpPhyPage::getNxtPIDOfDblList( sPrevNode ) == sCurrentPID ) 
        {
            // SMO�� �߻����� �ʾ���

            // BUG-21388
            // page link�� valid������, ���� �������� SMO number�� �ٲ������
            // �˻��ؾ� �Ѵ�.

            IDE_TEST( getPage( sStatistics,
                               &(aIndex->mQueryStat.mIndexPage),
                               aIndex->mIndexTSID,
                               sCurrentPID,
                               SDB_S_LATCH,
                               SDB_WAIT_NORMAL,
                               NULL, /* aMtx */
                               (UChar **)&sCurrentNode,
                               &sIsSuccess )
                      != IDE_SUCCESS );
            sState = 2;

            if ( sdpPhyPage::getIndexSMONo(sCurrentNode) == sCurrentSmoNo )
            {
                aIterator->mCurLeafNode  = sdpPhyPage::getPageID((UChar *)sPrevNode);
                aIterator->mNextLeafNode = sPrevNode->mListNode.mPrev;
                aIterator->mCurNodeSmoNo = sdpPhyPage::getIndexSMONo(sPrevNode);
                getSmoNo( (void *)aIndex, &aIterator->mIndexSmoNo );
                IDL_MEM_BARRIER;
                aIterator->mCurNodeLSN   = sdpPhyPage::getPageLSN(
                                             sdpPhyPage::getPageStartPtr((UChar *)sPrevNode));

                sSlotDirPtr = sdpPhyPage::getSlotDirStartPtr( (UChar *)sPrevNode );
                sSlotSeq    = sdpSlotDirectory::getCount( sSlotDirPtr ) - 1;

                IDE_TEST( makeRowCacheBackward( aIterator,
                                                (UChar *)sPrevNode,
                                                sSlotSeq )
                          != IDE_SUCCESS );

                sState = 1;
                IDE_TEST( sdbBufferMgr::releasePage( sStatistics,
                                                     (UChar *)sCurrentNode )
                          != IDE_SUCCESS );

                sState = 0;
                IDE_TEST( sdbBufferMgr::releasePage( sStatistics,
                                                     (UChar *)sPrevNode )
                          != IDE_SUCCESS );
                break;
            }
            else
            {
                /* nothing to do */
            }

            sState = 1;
            IDE_TEST( sdbBufferMgr::releasePage( sStatistics,
                                                 (UChar *)sCurrentNode )
                      != IDE_SUCCESS );
        }
        else
        {
            /* nothing to do */
        }

        // ������ link�� �ٲ���ų�, current page�� SMO�� �߻��� ���
        // �ٽ� �ε����� traverse�Ͽ� ������ row RID�� ã�� ��
        // �� �������� caching�Ѵ�.
        sState = 0;
        IDE_TEST( sdbBufferMgr::releasePage( sStatistics,
                                             (UChar *)sPrevNode )
                  != IDE_SUCCESS );

        if ( aIterator->mPrevKey == NULL )
        {
            IDE_TEST( findLast( aIterator ) != IDE_SUCCESS );

            break;
        }
        else
        {
            /* nothing to do */
        }

        SDNB_LKEY_TO_KEYINFO( aIterator->mPrevKey, sKeyInfo );

        /* BUG-24703 �ε��� �������� NOLOGGING���� X-LATCH�� ȹ��
         * �ϴ� ��찡 �ֽ��ϴ�.
         * MTX_NOLOGGING ���¿��� �α��� �ȵǴµ� X-Latch�� ����
         * �������� ���� ���, TempPage�� ���ֵ� �� �ֽ��ϴ�.
         * ���� X-Latch�� ��� ������ ���, �ݵ�� MTX_LOGGING
         * ���� ���� �մϴ�.*/
        IDE_TEST( sdrMiniTrans::begin( sStatistics,
                                       &sMtx,
                                       (void *)aIterator->mTrans,
                                       SDR_MTX_LOGGING,
                                       ID_FALSE,/*Undoable(PROJ-2162)*/
                                       gMtxDLogType )
                 != IDE_SUCCESS );
        sMtxStart = ID_TRUE;

        //fix BUG-16292 missing ,initialize stack
        initStack( &sStack );
        IDE_TEST( traverse( sStatistics,
                            aIndex,
                            &sMtx,
                            &sKeyInfo,
                            &sStack,
                            &(aIndex->mQueryStat),
                            ID_FALSE,  /* aPessimistic */
                            &sTraverseNode,
                            &sSlotSeq,
                            NULL,      /* aLeafSP */
                            &sIsSameKey,
                            &sTotalTraverseLength )
                  != IDE_SUCCESS );

        if ( ( sTraverseNode == NULL ) || ( sIsSameKey != ID_TRUE ) )
        {
            dumpHeadersAndIteratorToSMTrc( NULL,
                                           aIndex,
                                           aIterator );

            if ( iduMemMgr::calloc( IDU_MEM_SM_SDN, 
                                    1,
                                    ID_SIZEOF( SChar ) * IDE_DUMP_DEST_LIMIT,
                                    (void **)&sOutBuffer4Dump )
                 == IDE_SUCCESS )
            {
                /* PROJ-2162 Restart Recovery Reduction
                 * sKeyInfo ���Ŀ��� ����ó���Ǵ� ������ ����� */

                (void) dumpKeyInfo( &sKeyInfo, 
                                    &(aIndex->mColLenInfoList),
                                    sOutBuffer4Dump,
                                    IDE_DUMP_DEST_LIMIT );
                (void) dumpStack( &sStack, 
                                  sOutBuffer4Dump,
                                  IDE_DUMP_DEST_LIMIT );
                ideLog::log( IDE_ERR_0, "%s\n", sOutBuffer4Dump );
                (void) iduMemMgr::free( sOutBuffer4Dump ) ;
            }
            else
            {
                /* nothing to do */
            }

            if ( sTraverseNode != NULL )
            {
                dumpIndexNode( sTraverseNode );
            }
            else
            {
                /* nothing to do */
            }

            IDE_ERROR( 0 );
        }
        else
        {
            /* nothing to do */
        }

        if ( sSlotSeq > 0 )
        {
            // BUG-21388
            // traverse�� ã�� Ű�� 0��°�� �ƴ϶��
            // �� ������ Ű�鿡 ���� rowCache �����ؾ� �Ѵ�.
            aIterator->mCurLeafNode  = sdpPhyPage::getPageID((UChar *)sTraverseNode);
            aIterator->mNextLeafNode = sTraverseNode->mListNode.mPrev;
            aIterator->mCurNodeSmoNo = sdpPhyPage::getIndexSMONo(sTraverseNode);
            getSmoNo( (void *)aIndex, &aIterator->mIndexSmoNo );
            IDL_MEM_BARRIER;
            aIterator->mCurNodeLSN   = sdpPhyPage::getPageLSN(
                                         sdpPhyPage::getPageStartPtr((UChar *)sTraverseNode));

            IDE_TEST( makeRowCacheBackward( aIterator,
                                            (UChar *)sTraverseNode,
                                            sSlotSeq - 1 )
                      != IDE_SUCCESS );              

            sMtxStart = ID_TRUE;
            IDE_TEST( sdrMiniTrans::commit( &sMtx ) != IDE_SUCCESS );

            break;
        }
        else
        {
            /* nothing to do */
        }

        // traverse�� ã�� Ű�� node���� �� ó���� �ִ�.
        // �� ��� prev node�� �����ؾ� �Ѵ�.
        sPrevPID      = sTraverseNode->mListNode.mPrev;
        sCurrentPID   = sdpPhyPage::getPageID((UChar *)sTraverseNode);
        sCurrentSmoNo = sdpPhyPage::getIndexSMONo(sTraverseNode);

        sMtxStart = ID_TRUE;
        IDE_TEST( sdrMiniTrans::commit( &sMtx ) != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if (sMtxStart == ID_TRUE)
    {
        (void)sdrMiniTrans::rollback(&sMtx);
    }
    switch( sState )
    {
        case 2:
            if ( sdbBufferMgr::releasePage( sStatistics, (UChar *)sCurrentNode )
                 != IDE_SUCCESS )
            {
                dumpIndexNode( sCurrentNode );
                IDE_ASSERT( 0 );
            }
        case 1:
            if ( sdbBufferMgr::releasePage( sStatistics, (UChar *)sPrevNode )
                 != IDE_SUCCESS )
            {
                dumpIndexNode( sPrevNode );
                IDE_ASSERT( 0 );
            }

        default:
            break;
    }

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : sdnbBTree::lockAllRows4RR                  *
 * ------------------------------------------------------------------*
 * �־��� key range�� �´� ��� key�� ����Ű�� Row���� ã�� lock��   *
 * �Ǵ�.                                                             *
 * ���� key�� key range�� ���� ������ �ش� key range�� ������ ����   *
 * ���̹Ƿ� ���� key range�� ����Ͽ� �ٽ� �����Ѵ�.                 *
 * key range�� ����� key�� �߿���                                   *
 *     1. Transaction level visibility�� ����ϰ�,                   *
 *     2. Cursor level visibility�� ����ϰ�,                        *
 *     3. Filter������ ����ϴ�                                      *
 *     4. Update������                                               *
 * Row�鸸 ��ȯ�Ѵ�.                                                 *
 * �� �Լ��� Key Range�� Filter�� �մ��ϴ� ��� Row�� lock�� �ɱ�    *
 * ������, �����Ŀ��� After Last ���°� �ȴ�.                        *
 *********************************************************************/
IDE_RC sdnbBTree::lockAllRows4RR( sdnbIterator* aIterator )
{

    idBool          sIsRowDeleted;
    sdcUpdateState  sRetFlag;
    smTID           sWaitTxID;
    sdrMtx          sMtx;
    UChar         * sSlot;
    idBool          sIsSuccess;
    idBool          sResult;
    sdSID           sRowSID;
    scGRID          sRowGRID;
    idBool          sIsVisible;
    idBool          sSkipLockRec = ID_FALSE;
    sdSID           sTSSlotSID   = smLayerCallback::getTSSlotSID( aIterator->mTrans );
    sdnbHeader    * sIndex;
    scSpaceID       sTableTSID;
    UChar         * sLeafPage;
    sdnbLKey      * sLeafKey;
    UChar           sCTSlotIdx;
    sdrMtxStartInfo sStartInfo;
    sdpPhyPageHdr * sPageHdr;
    idBool          sIsPageLatchReleased = ID_TRUE;
    sdrSavePoint    sSP;
    UChar         * sDataPage;
    UChar         * sDataSlotDir;
    UChar         * sSlotDirPtr;
    UInt            sState = 0;

    IDE_ERROR( aIterator->mProperties->mLockRowBuffer != NULL ); // BUG-47758

    sIndex = (sdnbHeader*)((smnIndexHeader*)(aIterator->mIndex))->mHeader;
    sTableTSID = sIndex->mTableTSID;

    sStartInfo.mTrans   = aIterator->mTrans;
    sStartInfo.mLogMode = SDR_MTX_LOGGING;

  read_from_cache:
    while ( ( aIterator->mCurRowPtr + 1 ) < aIterator->mCacheFence )
    {
        IDE_TEST( iduCheckSessionEvent( aIterator->mProperties->mStatistics ) != IDE_SUCCESS );
        aIterator->mCurRowPtr++;
    revisit_row:
        /* BUG-45401 : undoable ID_FALSE -> ID_TRUE�� ���� */
        IDE_TEST( sdrMiniTrans::begin( aIterator->mProperties->mStatistics,
                                       &sMtx,
                                       aIterator->mTrans,
                                       SDR_MTX_LOGGING,
                                       ID_TRUE,/*Undoable(PROJ-2162)*/
                                       gMtxDLogType | SM_DLOG_ATTR_UNDOABLE )
                  != IDE_SUCCESS );
        sState = 1;

        sRowSID = SD_MAKE_SID( aIterator->mCurRowPtr->mRowPID,
                               aIterator->mCurRowPtr->mRowSlotNum);

        sdrMiniTrans::setSavePoint( &sMtx, &sSP );

        IDE_TEST( getPage( aIterator->mProperties->mStatistics,
                           &(sIndex->mQueryStat.mIndexPage),
                           sTableTSID,
                           SD_MAKE_PID( sRowSID ),
                           SDB_X_LATCH,
                           SDB_WAIT_NORMAL,
                           (void *)&sMtx,
                           (UChar **)&sDataPage,
                           &sIsSuccess )
                  != IDE_SUCCESS );

        sDataSlotDir = sdpPhyPage::getSlotDirStartPtr( sDataPage );
        sLeafKey     = (sdnbLKey*)&(aIterator->mPage[aIterator->mCurRowPtr->mOffset]);

        /*
         * BUG-27163 [SM] IDE_ASSERT(SDP_IS_UNUSED_SLOT_ENTRY(sSlotEntry) != ID_TRUE)��
         *           ������ ������ �����մϴ�.
         * : INSERT �ѹ��� Ʈ����� ��ü���� ���ڵ带 UNUSED���·� �����ϱ� ������
         *   VISIBLE_UNKOWN�� ��쿡 ���ؼ� �ش� SLOT�� UNUSED�������� �˻��ؾ� �Ѵ�.
         */
        if ( sdpSlotDirectory::isUnusedSlotEntry( sDataSlotDir,
                                                 SD_MAKE_SLOTNUM( sRowSID ) )
            == ID_TRUE )
        {
            if ( SDNB_GET_STATE( sLeafKey ) == SDNB_CACHE_VISIBLE_YES )
            {
                /* BUG-31845 [sm-disk-index] Debugging information is needed 
                 * for PBT when fail to check visibility using DRDB Index.
                 * ���� �߻��� ������ ����մϴ�. */
                dumpHeadersAndIteratorToSMTrc( 
                                         (smnIndexHeader*)aIterator->mIndex,
                                         sIndex,
                                         aIterator );

                if ( sIsPageLatchReleased == ID_FALSE )
                {
                    ideLog::logMem( IDE_DUMP_0, 
                                    (UChar *)sDataPage,
                                    SD_PAGE_SIZE,
                                    "Data page dump :\n" );

                    sIsPageLatchReleased = ID_TRUE;
                    IDE_ASSERT( sdbBufferMgr::releasePage( 
                                          aIterator->mProperties->mStatistics,
                                          (UChar *)sDataPage )
                                == IDE_SUCCESS );
                }
                else
                {
                    /* nothing to do */
                }

                /* Index cache�����δ� page�� ���¸� ��Ȯ�� �� �� �����ϴ�.
                 * ���� LeafNode�� getpage�Ͽ� �ش� Page�� �о� ����ϴ�.
                 * (���� ���� latch�� Ǭ �����̱� ������, Cache�� ������
                 *  �ٸ� �� �ִٴ� ���� �����ؾ� �մϴ�. */
                IDE_ASSERT_MSG( sdbBufferMgr::getPageByPID(
                                            aIterator->mProperties->mStatistics,
                                            sIndex->mIndexTSID,
                                            aIterator->mCurLeafNode,
                                            SDB_S_LATCH,
                                            SDB_WAIT_NORMAL,
                                            SDB_SINGLE_PAGE_READ,
                                            NULL, /* aMtx */
                                            (UChar **)&sLeafPage,
                                            &sIsSuccess,
                                            NULL /*IsCorruptPage*/ )
                                            == IDE_SUCCESS,
                                            "Error Ouccurs in DRDB Index ID : %"ID_UINT32_FMT, sIndex->mIndexID );

                ideLog::logMem( IDE_DUMP_0, 
                                (UChar *)sLeafPage,
                                SD_PAGE_SIZE,
                                "Leaf Node dump :\n" );

                IDE_ASSERT( sdbBufferMgr::releasePage( 
                                            aIterator->mProperties->mStatistics,
                                            (UChar *)sLeafPage )
                            == IDE_SUCCESS );

                IDE_ERROR( 0 );
            }

            sState = 0;
            IDE_TEST( sdrMiniTrans::commit(&sMtx) != IDE_SUCCESS );

            continue;
        }

        IDE_TEST( sdpSlotDirectory::getPagePtrFromSlotNum( 
                                                     sDataSlotDir,
                                                     SD_MAKE_SLOTNUM( sRowSID ),
                                                     &sSlot )
                  != IDE_SUCCESS );

        /* BUG-46068 : AGING�� DATA slot�� ����ǰ�, ù��° record piece�� �ƴϸ� SKIP�Ѵ�.
                       (ù��° record piece�� ���� �����ڵ忡�� Ȯ���� SKIP�Ѵ�.) */
        if ( sdcRow::isHeadRowPiece( sSlot ) == ID_FALSE )
        {
            sState = 0;
            IDE_TEST( sdrMiniTrans::commit(&sMtx) != IDE_SUCCESS );

            continue;
        }

        sIsPageLatchReleased = ID_FALSE;
        IDE_TEST( makeVRowFromRow( aIterator->mProperties->mStatistics,
                                   &sMtx,
                                   &sSP,
                                   aIterator->mTrans,
                                   aIterator->mTable,
                                   sTableTSID,
                                   sSlot,
                                   SDB_SINGLE_PAGE_READ,
                                   aIterator->mProperties->mFetchColumnList,
                                   SMI_FETCH_VERSION_CONSISTENT,
                                   sTSSlotSID,
                                   &(aIterator->mSCN),
                                   &(aIterator->mInfinite),
                                   aIterator->mProperties->mLockRowBuffer,
                                   &sIsRowDeleted,
                                   &sIsPageLatchReleased )
                  != IDE_SUCCESS );

        if ( sIsRowDeleted == ID_TRUE )
        {
            sState = 0;
            IDE_TEST( sdrMiniTrans::commit(&sMtx) != IDE_SUCCESS );
            continue;
        }
        else
        {
            /* nothing to do */
        }

        /* BUG-23319
         * [SD] �ε��� Scan�� sdcRow::fetch �Լ����� Deadlock �߻����ɼ��� ����. */
        /* row fetch�� �ϴ��߿� next rowpiece�� �̵��ؾ� �ϴ� ���,
         * ���� page�� latch�� Ǯ�� ������ deadlock �߻����ɼ��� �ִ�.
         * �׷��� page latch�� Ǭ ���� next rowpiece�� �̵��ϴµ�,
         * ���� �Լ������� page latch�� Ǯ������ ���θ� output parameter�� Ȯ���ϰ�
         * ��Ȳ�� ���� ������ ó���� �ؾ� �Ѵ�. */
        if ( sIsPageLatchReleased == ID_TRUE )
        {
            IDE_TEST( sdbBufferMgr::getPageBySID(
                                      aIterator->mProperties->mStatistics,
                                      sTableTSID,
                                      sRowSID,
                                      SDB_X_LATCH,
                                      SDB_WAIT_NORMAL,
                                      &sMtx,
                                      (UChar **)&sSlot,
                                      &sIsSuccess )
                      != IDE_SUCCESS );
        }

        sIsVisible = ID_TRUE;

        if ( SDNB_GET_STATE( sLeafKey ) == SDNB_CACHE_VISIBLE_UNKNOWN )
        {
            IDE_DASSERT( sIsRowDeleted == ID_FALSE );
            IDE_TEST( cursorLevelVisibility( aIterator->mIndex,
                                             &(sIndex->mQueryStat),
                                             aIterator->mProperties->mLockRowBuffer,
                                             (UChar *)sLeafKey,
                                             &sIsVisible )
                      != IDE_SUCCESS );
        }
#if defined(DEBUG)
        /* BUG-31845 [sm-disk-index] Debugging information is needed for 
         * PBT when fail to check visibility using DRDB Index.
         * Unknown�� �ƴϸ�, ������ �� �� �־�� �մϴ�. */
        else
        {
            IDE_DASSERT( SDNB_GET_STATE( sLeafKey ) 
                         == SDNB_CACHE_VISIBLE_YES );

            IDE_DASSERT( sIsRowDeleted == ID_FALSE );
            IDE_TEST( cursorLevelVisibility( aIterator->mIndex,
                                             &(sIndex->mQueryStat),
                                             aIterator->mProperties->mLockRowBuffer,
                                             (UChar *)sLeafKey,
                                             &sIsVisible )
                      != IDE_SUCCESS );
            IDE_DASSERT( sIsVisible == ID_TRUE );
        }
#endif

        if ( sIsVisible == ID_FALSE )
        {
            sState = 0;
            IDE_TEST( sdrMiniTrans::commit( &sMtx ) != IDE_SUCCESS );


            continue;
        }
        else
        {
            /* nothing to do */
        }

        SC_MAKE_GRID_WITH_SLOTNUM( sRowGRID,
                                   sTableTSID,
                                   SD_MAKE_PID( sRowSID ),
                                   SD_MAKE_SLOTNUM( sRowSID ) );

        aIterator->mScanBackNode = aIterator->mCurLeafNode;

        IDE_TEST( aIterator->mRowFilter->callback(
                                        &sResult,
                                        aIterator->mProperties->mLockRowBuffer,
                                        NULL,
                                        0,
                                        sRowGRID,
                                        aIterator->mRowFilter->data ) 
                  != IDE_SUCCESS );

        if ( sResult == ID_FALSE )
        {
            sState = 0;
            IDE_TEST( sdrMiniTrans::commit( &sMtx ) != IDE_SUCCESS );


            continue;
        }
        else
        {
            /* nothing to do */
        }

        IDE_TEST( sdcRow::canUpdateRowPieceInternal(
                                  aIterator->mProperties->mStatistics,
                                  &sStartInfo,
                                  sSlot,
                                  sTSSlotSID,
                                  SDB_SINGLE_PAGE_READ,
                                  &(aIterator->mSCN),
                                  &(aIterator->mInfinite),
                                  ID_FALSE, /* aIsUptLobByAPI */
                                  &sRetFlag,
                                  &sWaitTxID ) != IDE_SUCCESS );

        /* PROJ-2162 RestartRiskRedcution
         * ���� ����, lock �õ� �� Rollback�ϴ� ���, ����ó�� */
        if ( sRetFlag == SDC_UPTSTATE_REBUILD_ALREADY_MODIFIED )
        {
            /* ���� ���� ����ϰ�
               commit -> releaseLatch + rollback */
            IDE_RAISE( ERR_ALREADY_MODIFIED );
        }
        else
        {
            /* nothing to do */
        }

        if ( sRetFlag == SDC_UPTSTATE_UPDATE_BYOTHER )
        {
            sState = 0;
            IDE_TEST( sdrMiniTrans::commit(&sMtx) != IDE_SUCCESS );

            IDE_TEST( smLayerCallback::waitForTrans( aIterator->mTrans,
                                                     sWaitTxID,
                                                     sTableTSID,
                                                     aIterator->mProperties->mLockWaitMicroSec ) // aLockTimeOut
                      != IDE_SUCCESS );
            goto revisit_row;
        }
        else
        {
            if ( sRetFlag == SDC_UPTSTATE_INVISIBLE_MYUPTVERSION )
            {
                sState = 0;
                IDE_TEST( sdrMiniTrans::commit(&sMtx) != IDE_SUCCESS );
                continue;
            }
        }

        // skip count ��ŭ row �ǳʶ�
        if ( aIterator->mProperties->mFirstReadRecordPos > 0 )
        {
            aIterator->mProperties->mFirstReadRecordPos--;
        }
        else if ( aIterator->mProperties->mReadRecordCount > 0 )
        {
            sPageHdr = (sdpPhyPageHdr*)sdpPhyPage::getPageStartPtr( sSlot );

            IDE_TEST( sdcTableCTL::allocCTSAndSetDirty(
                                       aIterator->mProperties->mStatistics,
                                       &sMtx,       /* aFixMtx */
                                       &sStartInfo, /* for Logging */
                                       sPageHdr,
                                       &sCTSlotIdx ) != IDE_SUCCESS );

            /*  BUG-24406 [5.3.1 SD] index scan���� lock row �ϴٰ� �������. */
            /* allocCTS()�ÿ� CTL Ȯ���� �߻��ϴ� ���,
             * CTL Ȯ���߿� compact page ������ �߻��� �� �ִ�.
             * compact page ������ �߻��ϸ�
             * ������������ slot���� ��ġ(offset)�� ����� �� �ִ�.
             * �׷��Ƿ� allocCTS() �Ŀ��� slot pointer�� �ٽ� ���ؿ;� �Ѵ�. */
            sSlotDirPtr = sdpPhyPage::getSlotDirStartPtr( sdpPhyPage::getPageStartPtr( sSlot ) ); 
            IDE_TEST( sdpSlotDirectory::getPagePtrFromSlotNum(
                                              sSlotDirPtr,
                                              aIterator->mCurRowPtr->mRowSlotNum,
                                              &sSlot )
                      != IDE_SUCCESS );

            aIterator->mProperties->mReadRecordCount--;

            IDE_TEST( sdcRow::lock( aIterator->mProperties->mStatistics,
                                    sSlot,
                                    sRowSID,
                                    &(aIterator->mInfinite),
                                    &sMtx,
                                    sCTSlotIdx,
                                    &sSkipLockRec ) != IDE_SUCCESS );

            SC_MAKE_GRID_WITH_SLOTNUM( aIterator->mRowGRID,
                                       sTableTSID,
                                       aIterator->mCurRowPtr->mRowPID,
                                       aIterator->mCurRowPtr->mRowSlotNum);
        }
        else
        {
            // �ʿ��� Row�� ���� ��� lock�� ȹ���Ͽ���...
            sState = 0;
            IDE_TEST( sdrMiniTrans::commit( &sMtx ) != IDE_SUCCESS );


            return IDE_SUCCESS;
        }

        sState = 0;
        IDE_TEST( sdrMiniTrans::commit( &sMtx ) != IDE_SUCCESS );

    }

    if ( aIterator->mIsLastNodeInRange == ID_TRUE )  // Range�� ���� ������.
    {
        if ( ( aIterator->mFlag & SMI_RETRAVERSE_MASK ) == SMI_RETRAVERSE_BEFORE )
        {
            if ( aIterator->mKeyRange->next != NULL ) // next key range�� �����ϸ�
            {
                aIterator->mKeyRange = aIterator->mKeyRange->next;
                IDE_TEST( iduCheckSessionEvent(aIterator->mProperties->mStatistics) != IDE_SUCCESS );
                IDE_TEST( beforeFirstInternal(aIterator) != IDE_SUCCESS );
                goto read_from_cache;
            }
            else
            {
                return IDE_SUCCESS;
            }
        }
        else
        {
            if ( aIterator->mKeyRange->prev != NULL ) // next key range�� �����ϸ�
            {
                aIterator->mKeyRange = aIterator->mKeyRange->prev;
                IDE_TEST( iduCheckSessionEvent(aIterator->mProperties->mStatistics) != IDE_SUCCESS );
                IDE_TEST( afterLastInternal(aIterator) != IDE_SUCCESS );
                goto read_from_cache;
            }
            else
            {
                return IDE_SUCCESS;
            }
        }

    }
    else
    {
        // Key Range ������ ������ ���� ����
        // ���� Leaf Node�κ��� Index Cache ������ �����Ѵ�.
        // ���� ������ Index Cache �� ���� ������ ���� ����
        // read_from_cache �� �б��ϸ� �̸� �״�� ������.
        if ( ( aIterator->mFlag & SMI_RETRAVERSE_MASK ) == SMI_RETRAVERSE_BEFORE )
        {
            IDE_TEST( makeNextRowCacheForward( aIterator,
                                               sIndex )
                      != IDE_SUCCESS );
        }
        else
        {
            IDE_TEST( makeNextRowCacheBackward( aIterator,
                                                sIndex )
                      != IDE_SUCCESS );
        }

        goto read_from_cache;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_ALREADY_MODIFIED );
    {
        if( aIterator->mStatement->isForbiddenToRetry() == ID_TRUE )
        {
            IDE_DASSERT( ((smxTrans*)aIterator->mTrans)->mIsGCTx == ID_TRUE );

            SChar    sMsgBuf[SMI_MAX_ERR_MSG_LEN];
            sdpCTS * sCTS;
            smSCN    sFSCNOrCSCN;
            UChar    sCTSlotIdx;
            sdcRowHdrInfo   sRowHdrInfo;
            sdcRowHdrExInfo sRowHdrExInfo;

            sdcRow::getRowHdrInfo( sSlot, &sRowHdrInfo );
            sCTSlotIdx = sRowHdrInfo.mCTSlotIdx;

            if ( SDC_HAS_BOUND_CTS(sCTSlotIdx) )
            {
                sCTS = sdcTableCTL::getCTS( sdpPhyPage::getHdr(sSlot),sCTSlotIdx );
                SM_SET_SCN( &sFSCNOrCSCN, &sCTS->mFSCNOrCSCN );
            }
            else
            {
                sdcRow::getRowHdrExInfo( sSlot, &sRowHdrExInfo );
                SM_SET_SCN( &sFSCNOrCSCN, &sRowHdrExInfo.mFSCNOrCSCN );
            }

            idlOS::snprintf( sMsgBuf,
                             SMI_MAX_ERR_MSG_LEN,
                             "[INDEX VALIDATION(RR)] "
                             "SpaceID:%"ID_UINT32_FMT", "
                             "TableOID:%"ID_vULONG_FMT", "
                             "ViewSCN:%"ID_UINT64_FMT", "
                             "CSInfiniteSCN:%"ID_UINT64_FMT", "
                             "CTSlotIdx:%"ID_UINT32_FMT", "
                             "FSCNOrCSCN:%"ID_UINT64_FMT", "
                             "InfiniteSCN:%"ID_UINT64_FMT", "
                             "Deleted:%s ",
                              ((smcTableHeader*)aIterator->mTable)->mSpaceID,
                              ((smcTableHeader*)aIterator->mTable)->mSelfOID,
                             SM_SCN_TO_LONG( aIterator->mSCN ),
                             SM_SCN_TO_LONG( aIterator->mInfinite ),
                             sCTSlotIdx,
                             SM_SCN_TO_LONG( sFSCNOrCSCN ),
                             SM_SCN_TO_LONG( sRowHdrInfo.mInfiniteSCN ),
                             SM_SCN_IS_DELETED( sRowHdrInfo.mInfiniteSCN)?"Y":"N" );

            IDE_SET( ideSetErrorCode(smERR_ABORT_StatementTooOld, sMsgBuf) );

            IDE_ERRLOG( IDE_SD_19 );
        }
        else
        {
            IDE_SET( ideSetErrorCode( smERR_RETRY_Already_Modified ) );
        }

        IDE_ASSERT( sdcRow::releaseLatchForAlreadyModify( &sMtx, &sSP )
                    == IDE_SUCCESS );
    }
    IDE_EXCEPTION_END;

    IDE_PUSH();
    /* BUG-24151: [SC] Update Retry, Delete Retry, Statement Rebuild Count��
     *            AWI�� �߰��ؾ� �մϴ�.*/
    if ( ideGetErrorCode() == smERR_RETRY_Already_Modified )
    {
        SMX_INC_SESSION_STATISTIC( sStartInfo.mTrans,
                                   IDV_STAT_INDEX_LOCKROW_RETRY_COUNT,
                                   1 /* Increase Value */ );

        smcTable::incStatAtABort( (smcTableHeader*)aIterator->mTable,
                                  SMC_INC_RETRY_CNT_OF_LOCKROW );
    }

    if ( sState == 1 )
    {
        (void)sdrMiniTrans::rollback(&sMtx);
    }
    IDE_POP();

    return IDE_FAILURE;

}

/*********************************************************************
 * FUNCTION DESCRIPTION : sdnbBTree::retraverse                      *
 * ------------------------------------------------------------------*
 * ������ fetch�� Row�� ��ġ�� �ε��� Ŀ���� �ٽ� ���ͽ�Ų��.        *
 *********************************************************************/
IDE_RC sdnbBTree::retraverse(sdnbIterator *  /* aIterator */,
                             const void  **  /*aRow*/)
{

    /* For A4 :  TimeStamp�� �ʹ� ������ ���̶� Ager�� �� Ŀ��������
       ������ �ִ� ���, ������ �ٽ� TimeStamp�� �ٽ� ���η� �ϰ�
       Ŀ���� retraverse�Ͽ� �ٽ� cache�� �ۼ���
       DRDB������ TimeStamp�� ���� �ʱ⶧���� ������ ����.

       assert(0)
    */
    IDE_ASSERT(0);

    return IDE_SUCCESS;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : sdnbBTree::getPosition                     *
 * ------------------------------------------------------------------*
 * ���� Iterator�� ��ġ�� �����Ѵ�.                                  *
 *********************************************************************/
IDE_RC sdnbBTree::getPosition( sdnbIterator *     aIterator,
                               smiCursorPosInfo * aPosInfo )
{
    sdnbHeader * sIndex;

    sIndex = (sdnbHeader*)((smnIndexHeader*)(aIterator->mIndex))->mHeader;

    IDE_DASSERT( (aIterator->mCurRowPtr >= &aIterator->mRowCache[0]) &&
                 (aIterator->mCurRowPtr < aIterator->mCacheFence) );

    SC_MAKE_GRID( aPosInfo->mCursor.mDRPos.mLeafNode,
                  sIndex->mIndexTSID,
                  aIterator->mCurLeafNode,
                  aIterator->mCurRowPtr->mOffset );
    aPosInfo->mCursor.mDRPos.mSmoNo = aIterator->mCurNodeSmoNo;
    aPosInfo->mCursor.mDRPos.mLSN   = aIterator->mCurNodeLSN;
    SC_MAKE_GRID_WITH_SLOTNUM( aPosInfo->mCursor.mDRPos.mRowGRID,
                               sIndex->mTableTSID,
                               aIterator->mCurRowPtr->mRowPID,
                               aIterator->mCurRowPtr->mRowSlotNum );

    aPosInfo->mCursor.mDRPos.mKeyRange = (smiRange *)aIterator->mKeyRange; /* BUG-43913 */

    return IDE_SUCCESS;

}

/*********************************************************************
 * FUNCTION DESCRIPTION : sdnbBTree::setPosition                     *
 * ------------------------------------------------------------------*
 * ������ ����� Iterator�� ��ġ�� �ٽ� ���ͽ�Ų��.                  *
 *********************************************************************/
IDE_RC sdnbBTree::setPosition( sdnbIterator     * aIterator,
                               smiCursorPosInfo * aPosInfo )
{

    sdrMtx          sMtx;
    sdpPhyPageHdr * sNode;
    SShort          sSlotSeq;
    idBool          sIsSuccess;
    smLSN           sPageLSN;
    smLSN           sCursorLSN;
    idBool          sSmoOcurred;
    scSpaceID       sTBSID = SC_MAKE_SPACE(aPosInfo->mCursor.mDRPos.mLeafNode);
    scPageID        sPID   = SC_MAKE_PID(aPosInfo->mCursor.mDRPos.mLeafNode);
    idBool          sMtxStart = ID_FALSE;
    sdnbHeader    * sIndex;
    UChar          *sSlotDirPtr;

    IDE_DASSERT( aIterator != NULL );
    IDE_DASSERT( aPosInfo != NULL );

    aIterator->mKeyRange = aPosInfo->mCursor.mDRPos.mKeyRange; /* BUG-43913 */

    sIndex = (sdnbHeader*)((smnIndexHeader*)(aIterator->mIndex))->mHeader;

    IDE_TEST( sdrMiniTrans::begin( aIterator->mProperties->mStatistics,
                                   &sMtx,
                                   (void *)NULL,
                                   SDR_MTX_NOLOGGING,
                                   ID_FALSE,/*Undoable(PROJ-2162)*/
                                   gMtxDLogType ) != IDE_SUCCESS );
    sMtxStart = ID_TRUE;

    IDE_TEST( sdnbBTree::getPage( aIterator->mProperties->mStatistics,
                                  &(sIndex->mQueryStat.mIndexPage),
                                  sTBSID,
                                  sPID,
                                  SDB_S_LATCH,
                                  SDB_WAIT_NORMAL,
                                  &sMtx,
                                  (UChar **)&sNode,
                                  &sIsSuccess ) != IDE_SUCCESS );
    sPageLSN = sdpPhyPage::getPageLSN(sdpPhyPage::getPageStartPtr( (UChar *)sNode) );

    sCursorLSN = aPosInfo->mCursor.mDRPos.mLSN;

    if ( ( sPageLSN.mFileNo != sCursorLSN.mFileNo ) ||
         ( sPageLSN.mOffset != sCursorLSN.mOffset ) )
    {
        sSmoOcurred =
            (sdpPhyPage::getIndexSMONo(sNode)
                 == aPosInfo->mCursor.mDRPos.mSmoNo) ? ID_FALSE : ID_TRUE;
        sMtxStart = ID_FALSE;
        IDE_TEST( sdrMiniTrans::commit( &sMtx ) != IDE_SUCCESS );


        // traverse() �Լ��� x-latch�� ��� �Ǵµ�, �̶� nologging���� ������
        // flush list�� ������ ��ϵȴ�.
        IDE_TEST( sdrMiniTrans::begin( aIterator->mProperties->mStatistics,
                                       &sMtx,
                                       aIterator->mTrans,
                                       SDR_MTX_LOGGING,
                                       ID_FALSE,/*Undoable(PROJ-2162)*/
                                       gMtxDLogType ) != IDE_SUCCESS );
        sMtxStart = ID_TRUE;
        // aPosInfo�� RowRID�� �̿��Ͽ� Row�� ã�´�.
        aIterator->mCurLeafNode = sPID;
        aIterator->mCurRowPtr   = &aIterator->mRowCache[0];
        aIterator->mCacheFence  = &aIterator->mRowCache[1];
        aIterator->mCurRowPtr->mRowPID =
                          SC_MAKE_PID(aPosInfo->mCursor.mDRPos.mRowGRID);
        aIterator->mCurRowPtr->mRowSlotNum =
                        SC_MAKE_SLOTNUM(aPosInfo->mCursor.mDRPos.mRowGRID);
        aIterator->mPrevKey = NULL;

        IDE_TEST( restoreCursorPosition( (smnIndexHeader*)(aIterator->mIndex),
                                         &sMtx,
                                         aIterator,
                                         sSmoOcurred,
                                         &sNode,
                                         &sSlotSeq)
                  != IDE_SUCCESS );
    }
    else
    {
        aIterator->mPrevKey = NULL;

        sSlotDirPtr = sdpPhyPage::getSlotDirStartPtr( (UChar *)sNode );
        IDE_TEST( sdpSlotDirectory::find(
                            sSlotDirPtr,
                            SC_MAKE_OFFSET( aPosInfo->mCursor.mDRPos.mLeafNode ),
                            &sSlotSeq )
                  != IDE_SUCCESS );
        IDE_DASSERT( ( sSlotSeq >= 0 ) &&
                     ( sSlotSeq < sdpSlotDirectory::getCount( sSlotDirPtr ) ) );
    }

    aIterator->mCurNodeSmoNo = sdpPhyPage::getIndexSMONo( sNode );
    getSmoNo( (void *)sIndex, &aIterator->mIndexSmoNo );
    IDL_MEM_BARRIER;

    // Iterator�� fetch ���⿡ ���� row cache�� �ٽ� �����Ѵ�.
    if ( ( aIterator->mFlag & SMI_RETRAVERSE_MASK ) == SMI_RETRAVERSE_BEFORE )
    {
        aIterator->mNextLeafNode = sNode->mListNode.mNext;
        IDE_TEST( makeRowCacheForward( aIterator,
                                       (UChar *)sNode,
                                       sSlotSeq )
                  != IDE_SUCCESS );
    }
    else
    {
        aIterator->mNextLeafNode = sNode->mListNode.mPrev;
        IDE_TEST( makeRowCacheBackward( aIterator,
                                        (UChar *)sNode,
                                        sSlotSeq )
                  != IDE_SUCCESS );
    }
    // Leaf Node�� unlatch �Ѵ�.
    sMtxStart = ID_FALSE;
    IDE_TEST( sdrMiniTrans::commit(&sMtx) != IDE_SUCCESS );


    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if (sMtxStart == ID_TRUE)
    {
        (void)sdrMiniTrans::rollback(&sMtx);
    }

    return IDE_FAILURE;

}

/*********************************************************************
 * FUNCTION DESCRIPTION : sdnbBTree::allocPage                       *
 * ------------------------------------------------------------------*
 * Free Node List�� Free�������� �����Ѵٸ� Free Node List�� ���� �� *
 * �������� �Ҵ� �ް�, ���� �׷��� �ʴٸ� Physical Layer�� ���� �Ҵ� *
 * �޴´�.                                                           *
 *********************************************************************/
IDE_RC sdnbBTree::allocPage( idvSQL          * aStatistics,
                             sdnbStatistic   * aIndexStat,
                             sdnbHeader      * aIndex,
                             sdrMtx          * aMtx,
                             UChar          ** aNewPage )
{
    idBool         sIsSuccess;
    UChar        * sMetaPage;
    sdnbMeta     * sMeta;
    sdnbNodeHdr  * sNodeHdr;
    sdpSegMgmtOp * sSegMgmtOp;

    if ( aIndex->mFreeNodeCnt > 0 )
    {
        sMetaPage = sdrMiniTrans::getPagePtrFromPageID(
                                        aMtx,
                                        aIndex->mIndexTSID,
                                        SD_MAKE_PID( aIndex->mMetaRID ) );

        if ( sMetaPage == NULL )
        {
            IDE_TEST( sdnbBTree::getPage( aStatistics,
                                          &(aIndexStat->mMetaPage),
                                          aIndex->mIndexTSID,
                                          SD_MAKE_PID( aIndex->mMetaRID ),
                                          SDB_X_LATCH,
                                          SDB_WAIT_NORMAL,
                                          aMtx,
                                          (UChar **)&sMetaPage,
                                          &sIsSuccess )
                      != IDE_SUCCESS );
        }
        else
        {
            /* nothing to do */
        }

        sMeta = (sdnbMeta*)( sMetaPage + SMN_INDEX_META_OFFSET );

        IDE_DASSERT( sMeta->mFreeNodeHead != SD_NULL_PID );
        IDE_DASSERT( sMeta->mFreeNodeCnt == aIndex->mFreeNodeCnt );

        IDE_TEST( sdnbBTree::getPage( aStatistics,
                                      &(aIndexStat->mIndexPage),
                                      aIndex->mIndexTSID,
                                      sMeta->mFreeNodeHead,
                                      SDB_X_LATCH,
                                      SDB_WAIT_NORMAL,
                                      aMtx,
                                      (UChar **)aNewPage,
                                      &sIsSuccess )
                  != IDE_SUCCESS );

        sNodeHdr = (sdnbNodeHdr*)
                     sdpPhyPage::getLogicalHdrStartPtr( (UChar *)*aNewPage );

        /*
         * Leaf Node�� ���� �ݵ�� FREE LIST�� ����Ǿ� �ִ�
         * ���¿��� �Ѵ�.
         */
        IDE_DASSERT( ( SDNB_IS_LEAF_NODE( sNodeHdr ) == ID_FALSE ) ||
                     ( sNodeHdr->mState == SDNB_IN_FREE_LIST ) );

        aIndex->mFreeNodeCnt--;
        aIndex->mFreeNodeHead = sNodeHdr->mNextEmptyNode;

        IDE_TEST( setFreeNodeInfo( aStatistics,
                                   aIndex,
                                   aIndexStat,
                                   aMtx,
                                   sNodeHdr->mNextEmptyNode,
                                   aIndex->mFreeNodeCnt )
                  != IDE_SUCCESS );

        IDE_TEST( sdpPhyPage::reset( (sdpPhyPageHdr*)*aNewPage,
                                     ID_SIZEOF( sdnbNodeHdr ),
                                     aMtx )
                  != IDE_SUCCESS );

        IDE_TEST( sdpSlotDirectory::logAndInit( (sdpPhyPageHdr*)*aNewPage,
                                                aMtx )
                  != IDE_SUCCESS );
    }
    else
    {
        sSegMgmtOp = sdpSegDescMgr::getSegMgmtOp( &(aIndex->mSegmentDesc) );
        IDE_ERROR( sSegMgmtOp != NULL );
        IDE_ASSERT( sSegMgmtOp->mAllocNewPage( aStatistics,
                                               aMtx,
                                               aIndex->mIndexTSID,
                                               &(aIndex->mSegmentDesc.mSegHandle),
                                               SDP_PAGE_INDEX_BTREE,
                                               (UChar **)aNewPage )
                    == IDE_SUCCESS );
        IDU_FIT_POINT("1.BUG-42505@sdnbBTree::allocPage::sdpPhyPage::logAndInitLogicalHdr");

        IDE_TEST( sdpPhyPage::logAndInitLogicalHdr(
                                      (sdpPhyPageHdr*)*aNewPage,
                                      ID_SIZEOF(sdnbNodeHdr),
                                      aMtx,
                                      (UChar **)&sNodeHdr ) != IDE_SUCCESS );

        IDE_TEST( sdpSlotDirectory::logAndInit( (sdpPhyPageHdr*)*aNewPage,
                                                aMtx )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : sdnbBTree::freePage                        *
 * ------------------------------------------------------------------*
 * Free�� ��带 Free Node List�� �����Ѵ�.                          *
 *********************************************************************/
IDE_RC sdnbBTree::freePage( idvSQL          * aStatistics,
                            sdnbStatistic   * aIndexStat,
                            sdnbHeader      * aIndex,
                            sdrMtx          * aMtx,
                            UChar           * aFreePage )
{
    idBool        sIsSuccess;
    UChar       * sMetaPage;
    sdnbMeta    * sMeta;
    sdnbNodeHdr * sNodeHdr;

    sMetaPage = sdrMiniTrans::getPagePtrFromPageID(
                                        aMtx,
                                        aIndex->mIndexTSID,
                                        SD_MAKE_PID( aIndex->mMetaRID ));

    if ( sMetaPage == NULL )
    {
        IDE_TEST( sdnbBTree::getPage( aStatistics,
                                      &(aIndexStat->mMetaPage),
                                      aIndex->mIndexTSID,
                                      SD_MAKE_PID( aIndex->mMetaRID ),
                                      SDB_X_LATCH,
                                      SDB_WAIT_NORMAL,
                                      aMtx,
                                      (UChar **)&sMetaPage,
                                      &sIsSuccess )
                  != IDE_SUCCESS );
    }

    sMeta = (sdnbMeta*)( sMetaPage + SMN_INDEX_META_OFFSET );

    IDE_DASSERT( sMeta->mFreeNodeCnt == aIndex->mFreeNodeCnt );

    aIndex->mFreeNodeCnt++;
    aIndex->mFreeNodeHead = sdpPhyPage::getPageID( aFreePage );

    sNodeHdr = (sdnbNodeHdr*)sdpPhyPage::getLogicalHdrStartPtr((UChar *)aFreePage);

    IDE_TEST( sdrMiniTrans::writeNBytes( aMtx,
                                         (UChar *)&sNodeHdr->mNextEmptyNode,
                                         (void *)&sMeta->mFreeNodeHead,
                                         ID_SIZEOF(sMeta->mFreeNodeHead) )
              != IDE_SUCCESS );

    IDE_TEST( setFreeNodeInfo( aStatistics,
                               aIndex,
                               aIndexStat,
                               aMtx,
                               aIndex->mFreeNodeHead,
                               aIndex->mFreeNodeCnt )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : sdnbBTree::preparePages                    *
 * ------------------------------------------------------------------*
 * �־��� ������ŭ�� �������� �Ҵ������ ������ �˻��Ѵ�.            *
 *********************************************************************/
IDE_RC sdnbBTree::preparePages( idvSQL      * aStatistics,
                                sdnbHeader  * aIndex,
                                sdrMtx      * aMtx,
                                idBool      * aMtxStart,
                                UInt          aNeedPageCnt )
{
    sdcUndoSegment * sUDSegPtr;
    sdpSegMgmtOp   * sSegMgmtOp;

    if ( aIndex->mFreeNodeCnt < aNeedPageCnt )
    {
        sSegMgmtOp = sdpSegDescMgr::getSegMgmtOp( &(aIndex->mSegmentDesc) );
        IDE_ERROR( sSegMgmtOp != NULL );
        if ( sSegMgmtOp->mPrepareNewPages( aStatistics,
                                           aMtx,
                                           aIndex->mIndexTSID,
                                           &(aIndex->mSegmentDesc.mSegHandle),
                                           aNeedPageCnt - aIndex->mFreeNodeCnt )
             != IDE_SUCCESS )
        {
            /* BUG-32579 The MiniTransaction commit should not be used
             * in exception handling area.
             * 
             * compact page, self-aging�� �������� �� �����Ƿ� commit��
             * �����Ѵ�.*/
            *aMtxStart = ID_FALSE;
            IDE_TEST( sdrMiniTrans::commit( aMtx ) != IDE_SUCCESS );
            IDE_TEST( 1 );
        }
        else
        {
            /* nothing to do */
        }
    }
    else
    {
        /* nothing to do */
    }

    /* BUG-24400 ��ũ �ε��� SMO�߿� Undo ������������ Rollback �ؼ��� �ȵ˴ϴ�.
     *           SMO ���� �����ϱ� ���� Undo ���׸�Ʈ�� Undo ������ �ϳ��� Ȯ���� �Ŀ�
     *           �����Ͽ��� �Ѵ�. Ȯ������ ���ϸ�, SpaceNotEnough ������ ��ȯ�Ѵ�. */
    if ( ((smxTrans*)aMtx->mTrans)->getTXSegEntry() != NULL )
    {
        sUDSegPtr = smxTrans::getUDSegPtr( (smxTrans*)aMtx->mTrans );
        if ( sUDSegPtr == NULL )
        {
            ideLog::log( IDE_ERR_0, "Transaction TX Segment entry info:\n" );
            ideLog::logMem( IDE_DUMP_0,
                            (UChar *)(((smxTrans *)aMtx->mTrans)->getTXSegEntry()),
                            ID_SIZEOF(*((smxTrans *)aMtx->mTrans)->getTXSegEntry()) );
            IDE_ASSERT_MSG( 0, "Error Ouccurs in DRDB Index ID : %"ID_UINT32_FMT, aIndex->mIndexID );
        }
        else
        {
            /* nothing to do */
        }

        if ( sUDSegPtr->prepareNewPage( aStatistics, aMtx )
             != IDE_SUCCESS )
        {
            /* BUG-32579 The MiniTransaction commit should not be used
             * in exception handling area.
             * 
             * compact page, self-aging�� �������� �� �����Ƿ� commit��
             * �����Ѵ�.*/
            *aMtxStart = ID_FALSE;
            IDE_TEST( sdrMiniTrans::commit( aMtx ) != IDE_SUCCESS );
            IDE_TEST( 1 );
        }
        else
        {
            /* nothing to do */
        }
    }
    else
    {
        // Top-Down Build Thread�ÿ��� TXSegEntry�� �Ҵ����� ������,
        // UndoPage�� Prepare�� �ʿ䵵 ����.
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : sdnbBTree::fixPage                         *
 * ------------------------------------------------------------------*
 * To fix BUG-18252                                                  *
 * �ε��� �������� ��Ÿ�������� ���� �󵵿� ���� ������� ����       *
 *********************************************************************/
IDE_RC sdnbBTree::fixPage( idvSQL             *aStatistics,
                           sdnbPageStat       *aPageStat,
                           scSpaceID           aSpaceID,
                           scPageID            aPageID,
                           UChar             **aRetPagePtr,
                           idBool             *aTrySuccess )
{
    idvSQL      * sStatistics;
    idvSession    sDummySession;
    idvSQL        sDummySQL;
    ULong         sGetPageCount;
    ULong         sReadPageCount;
    UInt          sState = 0;

    sDummySQL.mGetPageCount = 0;
    sDummySQL.mReadPageCount = 0;

    if ( aStatistics == NULL )
    {
        //fix for UMR
        idvManager::initSession( &sDummySession, 0, NULL );

        idvManager::initSQL( &sDummySQL,
                             &sDummySession,
                             NULL,
                             NULL,
                             NULL,
                             NULL,
                             IDV_OWNER_UNKNOWN );
        sStatistics = &sDummySQL;
    }
    else
    {
        sStatistics = aStatistics;
    }

    sGetPageCount  = sStatistics->mGetPageCount;
    sReadPageCount = sStatistics->mReadPageCount;

    IDE_TEST( sdbBufferMgr::fixPageByPID( sStatistics,
                                          aSpaceID,
                                          aPageID,
                                          aRetPagePtr,
                                          aTrySuccess )
              != IDE_SUCCESS );
    sState = 1;

    /* PROJ-2162 RestartRiskReduction
     * Inconsistent page �߰��Ͽ���, �����ؾ� �ϴ� ����̸�. */
    IDE_TEST_RAISE( 
        ( sdpPhyPage::isConsistentPage( *aRetPagePtr ) == ID_FALSE ) &&
        ( smuProperty::getCrashTolerance() != 2 ) ,
        ERR_INCONSISTENT_PAGE );

    if ( aPageStat != NULL )
    {
        aPageStat->mGetPageCount += 
                             sStatistics->mGetPageCount - sGetPageCount;
        aPageStat->mReadPageCount += 
                             sStatistics->mReadPageCount - sReadPageCount;
    }
    else
    {
        /* nothing to do */
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_INCONSISTENT_PAGE )
    {
        IDE_SET( ideSetErrorCode( smERR_ABORT_INCONSISTENT_PAGE,
                                  aSpaceID,
                                  aPageID ) );
    }
    IDE_EXCEPTION_END;

    if ( sState == 1 )
    {
        (void)sdnbBTree::unfixPage( aStatistics, *aRetPagePtr );
        sState = 0;
    }

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : sdnbBTree::fixPageByRID                    *
 * ------------------------------------------------------------------*
 * To fix BUG-18252                                                  *
 * �ε��� �������� ��Ÿ�������� ���� �󵵿� ���� ������� ����       *
 *********************************************************************/
IDE_RC sdnbBTree::fixPageByRID( idvSQL             *aStatistics,
                                sdnbPageStat       *aPageStat,
                                scSpaceID           aSpaceID,
                                sdRID               aRID,
                                UChar             **aRetPagePtr,
                                idBool             *aTrySuccess )
{
    IDE_TEST( sdnbBTree::fixPage( aStatistics,
                                  aPageStat,
                                  aSpaceID,
                                  SD_MAKE_PID( aRID ),
                                  aRetPagePtr,
                                  aTrySuccess )
              != IDE_SUCCESS );

    *aRetPagePtr += SD_MAKE_OFFSET( aRID );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : sdnbBTree::unfixPage                       *
 * ------------------------------------------------------------------*
 * To fix BUG-18252                                                  *
 * �ε��� �������� ��Ÿ�������� ���� �󵵿� ���� ������� ����       *
 *********************************************************************/
IDE_RC sdnbBTree::unfixPage( idvSQL *aStatistics,
                             UChar  *aPagePtr )
{

    IDE_TEST( sdbBufferMgr::unfixPage( aStatistics, aPagePtr )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : sdnbBTree::getPage                         *
 * ------------------------------------------------------------------*
 * To fix BUG-18252                                                  *
 * �ε��� �������� ��Ÿ�������� ���� �󵵿� ���� ������� ����       *
 *********************************************************************/
IDE_RC sdnbBTree::getPage( idvSQL             *aStatistics,
                           sdnbPageStat       *aPageStat,
                           scSpaceID           aSpaceID,
                           scPageID            aPageID,
                           sdbLatchMode        aLatchMode,
                           sdbWaitMode         aWaitMode,
                           void               *aMtx,
                           UChar             **aRetPagePtr,
                           idBool             *aTrySuccess )
{
    idvSQL      * sStatistics;
    idvSession    sDummySession;
    idvSQL        sDummySQL;
    ULong         sGetPageCount;
    ULong         sReadPageCount;

    sDummySQL.mGetPageCount = 0;
    sDummySQL.mReadPageCount = 0;

    if ( aStatistics == NULL )
    {
        //fix for UMR
        idvManager::initSession( &sDummySession, 0, NULL );

        idvManager::initSQL( &sDummySQL,
                             &sDummySession,
                             NULL,
                             NULL,
                             NULL,
                             NULL,
                             IDV_OWNER_UNKNOWN );
        sStatistics = &sDummySQL;
    }
    else
    {
        sStatistics = aStatistics;
    }

    sGetPageCount  = sStatistics->mGetPageCount;
    sReadPageCount = sStatistics->mReadPageCount;

    IDE_TEST( sdbBufferMgr::getPageByPID( sStatistics,
                                          aSpaceID,
                                          aPageID,
                                          aLatchMode,
                                          aWaitMode,
                                          SDB_SINGLE_PAGE_READ,
                                          aMtx,
                                          aRetPagePtr,
                                          aTrySuccess,
                                          NULL /*IsCorruptPage*/ )
              != IDE_SUCCESS );

    /* PROJ-2162 RestartRiskReduction
     * Inconsistent page �߰��Ͽ���, �����ؾ� �ϴ� ����̸�. */
    IDE_TEST_RAISE( 
        ( sdpPhyPage::isConsistentPage( *aRetPagePtr ) == ID_FALSE ) &&
        ( smuProperty::getCrashTolerance() != 2 ) ,
        ERR_INCONSISTENT_PAGE );

    aPageStat->mGetPageCount += sStatistics->mGetPageCount - sGetPageCount;
    aPageStat->mReadPageCount += sStatistics->mReadPageCount - sReadPageCount;

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_INCONSISTENT_PAGE )
    {
        IDE_SET( ideSetErrorCode( smERR_ABORT_INCONSISTENT_PAGE,
                                  aSpaceID,
                                  aPageID ) );

        /* BUG-47970 aMtx�� null�� ���� ���� release page �� ���� ���� �ʴ´�.
         * ���� page �� release �� �ش�.*/
        if ( aMtx == NULL )
        {
            (void)sdbBufferMgr::releasePage( aStatistics,
                                             *aRetPagePtr );
        }
    }
    IDE_EXCEPTION_END;

    /* ����ó���� �Ǿ, releasePage�� �ʿ�� ����. MTX�� �����ֱ� ������
     * �˾Ƽ� release���� ���̱� �����̴�. */

    return IDE_FAILURE;
}

SInt sdnbBTree::checkSMO( sdnbStatistic * aIndexStat,
                          sdpPhyPageHdr * aNode,
                          ULong           aIndexSmoNo,
                          ULong *         aNodeSmoNo )
{
    SInt sResult;

    *aNodeSmoNo = sdpPhyPage::getIndexSMONo( aNode );
    sResult     = ( (*aNodeSmoNo) > aIndexSmoNo ) ? 1 : -1;

    if ( sResult > 0 )
    {
        aIndexStat->mNodeRetryCount++;
    }
    else
    {
        /* nothing to do */
    }

    return sResult;
}


IDE_RC sdnbBTree::allocIterator( void ** /* aIteratorMem */ )
{
    return IDE_SUCCESS;
}


IDE_RC sdnbBTree::freeIterator( void * /* aIteratorMem */ )
{
    return IDE_SUCCESS;
}

/*
 * ( ( "PAGE SIZE"(8192) - ( "PHYSICAL HEADER"(80) + "PAGE FOOTER"(16) +
 *                           "LOGICAL HEADER"(16) + "CTLayerHdr"(8) + "MAX CTL SIZE"(30*40) ) ) / 2 )
 * - ( "LEAF EXTENTION SLOT HEADER"(40) + "SLOT ENTRY SIZE"(2) )
 * �������� Ű�� ���Եɼ� �ִ� ������ 2�� ����������, Key Header�� ������
 * ������ ������ �ִ� Ű ũ���̴�.
 */
UShort sdnbBTree::getMaxKeySize()
{
    return ( ( sdpPhyPage::getEmptyPageFreeSize()
               - ID_SIZEOF(sdnbNodeHdr)
               - (ID_SIZEOF(sdnCTLayerHdr) + (ID_SIZEOF(sdnCTS) * (SMI_MAXIMUM_INDEX_CTL_SIZE))) )
             / 2 )
            - IDL_MAX( SDNB_IKEY_HEADER_LEN, ID_SIZEOF(sdnbLKeyEx))
            - ID_SIZEOF(sdpSlotEntry);
}

UInt sdnbBTree::getMinimumKeyValueSize( smnIndexHeader *aIndexHeader )
{
    const sdnbHeader    *sHeader;
    smiColumn           *sKeyColumn;
    UInt                 i;
    UInt                 sTotalSize = 0;

    IDE_DASSERT( aIndexHeader != NULL );

    sHeader = (sdnbHeader*)aIndexHeader->mHeader;

    for ( i = 0 ; i < aIndexHeader->mColumnCount ; i++ )
    {
        sKeyColumn = &sHeader->mColumns[i].mKeyColumn;

        if ( ( sKeyColumn->flag & SMI_COLUMN_LENGTH_TYPE_MASK ) 
             == SMI_COLUMN_LENGTH_TYPE_KNOWN )
        {
            sTotalSize += sKeyColumn->size ;
        }
        else
        {
            sTotalSize += SDNB_SMALL_COLUMN_HEADER_LENGTH;
        }
    }

    return sTotalSize;
}

UInt sdnbBTree::getInsertableMaxKeyCnt( UInt aKeyValueSize )
{
    UInt sKeySize;
    UInt sFreeSize;
    UInt sInsertableKeyCnt;

    /*Bottom-up Build�� ������ �ϳ��� ���� ������ �ִ� Ű ������ ����ϱ� ����
     *ȣ��Ǹ�, ���� �׻� TB_CTS�� */
    sKeySize   = SDNB_LKEY_LEN( aKeyValueSize, SDNB_KEY_TB_CTS );
    sFreeSize  = sdpPhyPage::getEmptyPageFreeSize();
    sFreeSize -= idlOS::align8((UInt)ID_SIZEOF(sdnbNodeHdr));
    sFreeSize -= ID_SIZEOF( sdpSlotDirHdr );

    sInsertableKeyCnt = sFreeSize / ( sKeySize + ID_SIZEOF(sdpSlotEntry) );

    return sInsertableKeyCnt;
}

/*
 * BUG-24403 : Disk Tablespace online���� update �߻��� index ���ѷ���
 *
 * [IN]  aIndex - sdnbHeader* �� Index Header
 * [OUT] aSmoNo - ������ SMO No.
 */
void sdnbBTree::getSmoNo( void  * aIndex,
                          ULong * aSmoNo )
{
    sdnbHeader *sIndex = (sdnbHeader*)aIndex;
#ifndef COMPILE_64BIT
    UInt        sAtomicA;
    UInt        sAtomicB;
#endif
    
    IDE_ASSERT( aIndex != NULL );

#ifndef COMPILE_64BIT
    while ( 1 )
    {
        ID_SERIAL_BEGIN( sAtomicA = sIndex->mSmoNoAtomicA );
        
        ID_SERIAL_EXEC( *aSmoNo = sIndex->mSmoNo, 1 );

        ID_SERIAL_END( sAtomicB = sIndex->mSmoNoAtomicB );

        if ( sAtomicA == sAtomicB )
        {
            break;
        }
        else
        {
            /* nothing to do */
        }

        idlOS::thr_yield();
    }
#else
    *aSmoNo = sIndex->mSmoNo;
#endif
}

void sdnbBTree::increaseSmoNo( sdnbHeader*    aIndex )
{
    // REVIEW: �ڵ� �����ϱ�
    //         Latch�� ����ص� �ɰ� ������...
    UInt sSerialValue = 0;
    
    ID_SERIAL_BEGIN( aIndex->mSmoNoAtomicB++ );

    ID_SERIAL_EXEC( aIndex->mSmoNo++, ++sSerialValue );
    
    ID_SERIAL_END( aIndex->mSmoNoAtomicA++ );
}

IDE_RC sdnbBTree::initMeta( UChar * aMetaPtr,
                            UInt    aBuildFlag,
                            void  * aMtx )
{
    sdrMtx      * sMtx;
    sdnbMeta    * sMeta;
    scPageID      sPID = SD_NULL_PID;
    smiIndexStat  sStat;
    ULong         sFreeNodeCnt  = 0;
    idBool        sIsConsistent = ID_FALSE;
    idBool        sIsCreatedWithLogging;
    idBool        sIsCreatedWithForce = ID_FALSE;
    smLSN         sNullLSN;
    
    sMtx = (sdrMtx*)aMtx;

    /* BUG-40082 [PROJ-2506] Insure++ Warning
     * - �ʱ�ȭ�� �ʿ��մϴ�.
     * - smrRecoveryMgr::initialize()�� �����Ͽ����ϴ�.(smrRecoveryMgr.cpp:243)
     */
    SM_SET_LSN( sNullLSN, 0, 0 );

    sIsCreatedWithLogging = ( ( aBuildFlag & SMI_INDEX_BUILD_LOGGING_MASK ) ==
                              SMI_INDEX_BUILD_LOGGING) ? ID_TRUE : ID_FALSE ;

    sIsCreatedWithForce = ( ( aBuildFlag & SMI_INDEX_BUILD_FORCE_MASK ) ==
                            SMI_INDEX_BUILD_FORCE) ? ID_TRUE : ID_FALSE ;

    idlOS::memset( &sStat, 0, ID_SIZEOF( smiIndexStat ) );
    /* Index Specific Data �ʱ�ȭ */
    sMeta = (sdnbMeta*)( aMetaPtr + SMN_INDEX_META_OFFSET );

    IDE_TEST( sdrMiniTrans::writeNBytes( sMtx,
                                         (UChar *)&sMeta->mRootNode,
                                         (void *)&sPID,
                                         ID_SIZEOF(sPID) ) != IDE_SUCCESS );
    
    IDE_TEST( sdrMiniTrans::writeNBytes( sMtx,
                                         (UChar *)&sMeta->mEmptyNodeHead,
                                         (void *)&sPID,
                                         ID_SIZEOF(sPID) ) != IDE_SUCCESS );
    
    IDE_TEST( sdrMiniTrans::writeNBytes( sMtx,
                                         (UChar *)&sMeta->mEmptyNodeTail,
                                         (void *)&sPID,
                                         ID_SIZEOF(sPID) ) != IDE_SUCCESS );
    
    IDE_TEST( sdrMiniTrans::writeNBytes( sMtx,
                                         (UChar *)&sMeta->mMinNode,
                                         (void *)&sPID,
                                         ID_SIZEOF(sPID) ) != IDE_SUCCESS );
    
    IDE_TEST( sdrMiniTrans::writeNBytes( sMtx,
                                         (UChar *)&sMeta->mMaxNode,
                                         (void *)&sPID,
                                         ID_SIZEOF(sPID) ) != IDE_SUCCESS );
    
    IDE_TEST( sdrMiniTrans::writeNBytes( sMtx,
                                         (UChar *)&sMeta->mFreeNodeHead,
                                         (void *)&sPID,
                                         ID_SIZEOF(sPID) ) != IDE_SUCCESS );
    
    IDE_TEST( sdrMiniTrans::writeNBytes( sMtx,
                                         (UChar *)&sMeta->mFreeNodeCnt,
                                         (void *)&sFreeNodeCnt,
                                         ID_SIZEOF(sFreeNodeCnt) )
              != IDE_SUCCESS );
    
    IDE_TEST( sdrMiniTrans::writeNBytes( sMtx,
                                         (UChar *)&sMeta->mIsConsistent,
                                         (void *)&sIsConsistent,
                                         ID_SIZEOF(sIsConsistent) )
              != IDE_SUCCESS );
    
    IDE_TEST( sdrMiniTrans::writeNBytes( sMtx,
                                         (UChar *)&sMeta->mIsCreatedWithLogging,
                                         (void *)&sIsCreatedWithLogging,
                                         ID_SIZEOF(sIsCreatedWithLogging) )
              != IDE_SUCCESS );
    
    IDE_TEST( sdrMiniTrans::writeNBytes( sMtx,
                                         (UChar *)&sMeta->mIsCreatedWithForce,
                                         (void *)&sIsCreatedWithForce,
                                         ID_SIZEOF(sIsCreatedWithForce) )
              != IDE_SUCCESS );

    IDE_TEST( sdrMiniTrans::writeNBytes(
                              sMtx,
                              (UChar *)&(sMeta->mNologgingCompletionLSN.mFileNo),
                              (void *)&(sNullLSN.mFileNo),
                              ID_SIZEOF(sNullLSN.mFileNo) )
              != IDE_SUCCESS );
    
    IDE_TEST( sdrMiniTrans::writeNBytes(
                              sMtx,
                              (UChar *)&(sMeta->mNologgingCompletionLSN.mOffset),
                              (void *)&(sNullLSN.mOffset),
                              ID_SIZEOF(sNullLSN.mOffset) )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC sdnbBTree::buildMeta( idvSQL     * aStatistics,
                             void       * aTrans,
                             void       * aIndex )
{
    sdnbHeader  * sIndex = (sdnbHeader*)aIndex;
    sdrMtx        sMtx;
    scPageID    * sRootNode;
    scPageID    * sMinNode;
    scPageID    * sMaxNode;
    scPageID    * sFreeNodeHead;
    ULong       * sFreeNodeCnt;
    idBool      * sIsConsistent;
    smLSN       * sCompletionLSN;
    idBool      * sIsCreatedWithLogging;
    idBool      * sIsCreatedWithForce;
    sdnbStatistic sDummyStat;

    idBool        sMtxStart = ID_FALSE;
    sdrMtxLogMode sLogMode;

    // BUG-27328 CodeSonar::Uninitialized Variable
    idlOS::memset( &sDummyStat, 0, ID_SIZEOF(sDummyStat) );

    // index runtime header�� mLogging�� DML���� ���Ǵ� ���̹Ƿ�
    // index build �� �׻� ID_TRUE�� �ʱ�ȭ��Ŵ
    sIndex->mLogging = ID_TRUE;

    sRootNode             = &(sIndex->mRootNode);
    sMinNode              = &(sIndex->mMinNode);
    sMaxNode              = &(sIndex->mMaxNode);
    sCompletionLSN        = &(sIndex->mCompletionLSN);
    sIsCreatedWithLogging = &(sIndex->mIsCreatedWithLogging);
    sIsCreatedWithForce   = &(sIndex->mIsCreatedWithForce);
    sFreeNodeHead         = &(sIndex->mFreeNodeHead);
    sFreeNodeCnt          = &(sIndex->mFreeNodeCnt);
    sIsConsistent         = &(sIndex->mIsConsistent);

    sLogMode = (*sIsCreatedWithLogging == ID_TRUE)?
                                   SDR_MTX_LOGGING : SDR_MTX_NOLOGGING;

    IDE_TEST( sdrMiniTrans::begin( aStatistics,
                                   &sMtx,
                                   aTrans,
                                   sLogMode,
                                   ID_TRUE,/*Undoable(PROJ-2162)*/
                                   gMtxDLogType ) != IDE_SUCCESS );
    sMtxStart = ID_TRUE;

    sdrMiniTrans::setNologgingPersistent( &sMtx );

    IDE_TEST( setIndexMetaInfo( aStatistics,
                                sIndex,
                                &sDummyStat,
                                &sMtx,
                                sRootNode,
                                sMinNode,
                                sMaxNode,
                                sFreeNodeHead,
                                sFreeNodeCnt,
                                sIsCreatedWithLogging,
                                sIsConsistent,
                                sIsCreatedWithForce,
                                sCompletionLSN ) != IDE_SUCCESS );

    sMtxStart = ID_FALSE;
    IDE_TEST( sdrMiniTrans::commit(&sMtx) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if (sMtxStart == ID_TRUE)
    {
        (void)sdrMiniTrans::rollback(&sMtx);
    }

    return IDE_FAILURE;

}

IDE_RC sdnbBTree::verifyLeafKeyOrder( sdnbHeader    * aIndex,
                                      sdpPhyPageHdr * aNode,
                                      sdnbIKey      * aParentKey,
                                      sdnbIKey      * aRightParentKey,
                                      idBool        * aIsOK )
{

    SInt          i;
    idBool        sIsSameValue;
    SInt          sRet;
    sdnbKeyInfo   sCurKeyInfo;
    sdnbKeyInfo   sPrevKeyInfo;
    void         *sPrevKey = NULL;
    void         *sCurKey = NULL;
    idBool        sIsOK = ID_TRUE;
    sdnbStatistic sDummyStat;
    UChar        *sSlotDirPtr;
    UShort        sSlotCnt;

    // BUG-27499 CodeSonar::Uninitialized Variable
    idlOS::memset( &sDummyStat, 0, ID_SIZEOF(sDummyStat) );

    sSlotDirPtr = sdpPhyPage::getSlotDirStartPtr( (UChar *)aNode );
    sSlotCnt    = sdpSlotDirectory::getCount( sSlotDirPtr );

    for ( i = 0 ; i <= sSlotCnt ; i++ )
    {
        // BUG-27118 BTree�� ������ Node�� �Ŵ޸�ü �ִ� ��Ȳ�� ����
        // Disk �ε��� Integrity �˻縦 �����ؾ� �մϴ�.
        // Leaf���� �θ� Node���� Ű ���� ���Ͽ�, Ʈ�� ������
        // �����մϴ�.

        if ( i == 0 ) // compare LeftMostLeafKey and ParentsKey
        {
            sPrevKey = (void *)aParentKey; //Ȯ�ο����� Pointer�� ����

            if ( aParentKey != NULL )
            {
                SDNB_IKEY_TO_KEYINFO( aParentKey, sPrevKeyInfo );
            }
            else
            {
                /* nothing to do */
            }
        }
        else
        {
            SDNB_LKEY_TO_KEYINFO( ((sdnbLKey*)sPrevKey), sPrevKeyInfo );
        }

        if ( i == sSlotCnt ) // compare rightMostLeafKey and rightParentsKey
        {
            sCurKey = (void *)aRightParentKey; //Ȯ�ο����� Pointer�� ����

            if ( aRightParentKey != NULL )
            {
                SDNB_IKEY_TO_KEYINFO( aRightParentKey, sCurKeyInfo );
            }
            else
            {
                /* nothing to do */
            }
        }
        else
        {
            IDE_TEST( sdpSlotDirectory::getPagePtrFromSlotNum(
                                                            sSlotDirPtr, 
                                                            i,
                                                            (UChar **)&sCurKey )
                      != IDE_SUCCESS );

            SDNB_LKEY_TO_KEYINFO( ((sdnbLKey*)sCurKey), sCurKeyInfo );
        }

        if ( ( sPrevKey != NULL ) &&
             ( sCurKey != NULL ) )
        {

            sRet = compareKeyAndKey( &sDummyStat,
                                     aIndex->mColumns,
                                     aIndex->mColumnFence,
                                     &sCurKeyInfo,
                                     &sPrevKeyInfo,
                                     ( SDNB_COMPARE_VALUE   |
                                       SDNB_COMPARE_PID     |
                                       SDNB_COMPARE_OFFSET ),
                                     &sIsSameValue );

            if ( sRet <= 0 )
            {
                /*
                 * [BUG-24370] �ε��� ���Ἲ �˻�� DEAD ������ Ű�� ���ܽ��Ѿ� �մϴ�.
                 * DEAD�̰ų� DELETED�� Ű�� ���� [SID, VALUE]�� ���� Ű�� �����Ҽ� �ִ�.
                 * ��� Ʈ����ǿ� ���ؼ� ���ڵ�� ���������� Ű�� ���� �ִ� ���°� ����
                 * �Ҽ� ������, �̷��� ������ ���ڵ� ������ ������ ��� �ε����� ���� Ű��
                 * �����Ҽ� �ִ�. �׷��� �ε��� ���忡���� �������� Ű�� �ϳ��� �����ϱ�
                 * ������ ������ ���� �ʴ´�.
                 * ���� �̷��� ���� SKIP�ؾ� �Ѵ�.
                 */
                if ( sRet == 0 )
                {
                    // BUG-27118 BTree�� ������ Node�� �Ŵ޸�ü �ִ� ��Ȳ�� ����
                    // Disk �ε��� Integrity �˻縦 �����ؾ� �մϴ�.
                    // �θ���(IKey)���� ���� ���, �θ����� Ű�� �ڽĳ����
                    // Ű�� ���� �� �ֽ��ϴ�.
                    if ( ( i == 0 ) || ( i == sSlotCnt ) )
                    {
                        // nothing to do
                    }
                    else
                    {
                        if ( ( SDNB_GET_STATE( (sdnbLKey*)sCurKey )  == SDNB_KEY_DEAD )    ||
                             ( SDNB_GET_STATE( (sdnbLKey*)sCurKey )  == SDNB_KEY_DELETED ) ||
                             ( SDNB_GET_STATE( (sdnbLKey*)sPrevKey ) == SDNB_KEY_DEAD )    ||
                             ( SDNB_GET_STATE( (sdnbLKey*)sPrevKey ) == SDNB_KEY_DELETED ) )
                        {
                            // nothing to do
                        }
                        else
                        {
                            sIsOK = ID_FALSE;
                            ideLog::log( IDE_SM_0,
                                         SM_TRC_INDEX_INTEGRITY_CHECK_KEY_ORDER,
                                         'T',
                                         sRet,
                                         aNode->mPageID, i,
                                         (UInt)sCurKeyInfo.mRowPID,
                                         (UInt)sCurKeyInfo.mRowSlotNum );
                            break;
                        }
                    }
                }
                else
                {
                    sIsOK = ID_FALSE;
                    ideLog::log( IDE_SM_0,
                                 SM_TRC_INDEX_INTEGRITY_CHECK_KEY_ORDER,
                                 'T',
                                 sRet,
                                 aNode->mPageID, i,
                                 (UInt)sCurKeyInfo.mRowPID,
                                 (UInt)sCurKeyInfo.mRowSlotNum );
                    break;
                }
            }
            else
            {
                // nothing to do
            }
        }
        sPrevKey = sCurKey;
    }

    *aIsOK = sIsOK;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC sdnbBTree::verifyInternalKeyOrder( sdnbHeader    * aIndex,
                                          sdpPhyPageHdr * aNode,
                                          sdnbIKey      * aParentKey,
                                          sdnbIKey      * aRightParentKey,
                                          idBool        * aIsOK )
{

    SInt           i;
    UShort         sSlotCnt;
    sdnbIKey      *sCurIKey;
    idBool         sIsSameValue;
    SInt           sRet;
    sdnbKeyInfo    sCurKeyInfo;
    sdnbKeyInfo    sPrevKeyInfo;
    sdnbIKey      *sPrevIKey = NULL;
    idBool         sIsOK = ID_TRUE;
    sdnbStatistic  sDummyStat;
    UChar         *sSlotDirPtr;

    // BUG-27499 CodeSonar::Uninitialized Variable
    idlOS::memset( &sDummyStat, 0, ID_SIZEOF(sDummyStat) );

    sSlotDirPtr = sdpPhyPage::getSlotDirStartPtr( (UChar *)aNode );
    sSlotCnt    = sdpSlotDirectory::getCount( sSlotDirPtr );

    sPrevIKey = aParentKey;

    for ( i = 0 ; i <= sSlotCnt ; i++ )
    {
        if ( i == sSlotCnt )
        {
            sCurIKey  = aRightParentKey;
        }
        else
        {
            IDE_TEST( sdpSlotDirectory::getPagePtrFromSlotNum(
                                                        sSlotDirPtr, 
                                                        i,
                                                        (UChar **)&sCurIKey )
                      != IDE_SUCCESS );
        }

        if ( ( sPrevIKey != NULL ) && ( sCurIKey  != NULL ) )
        {
            SDNB_IKEY_TO_KEYINFO( sCurIKey, sCurKeyInfo );
            SDNB_IKEY_TO_KEYINFO( sPrevIKey, sPrevKeyInfo );

            sRet = compareKeyAndKey( &sDummyStat,
                                     aIndex->mColumns,
                                     aIndex->mColumnFence,
                                     &sCurKeyInfo,
                                     &sPrevKeyInfo,
                                     ( SDNB_COMPARE_VALUE   |
                                       SDNB_COMPARE_PID     |
                                       SDNB_COMPARE_OFFSET ),
                                     &sIsSameValue );

            if ( sRet <= 0 )
            {
                sIsOK = ID_FALSE;
                ideLog::log( IDE_SM_0,
                             SM_TRC_INDEX_INTEGRITY_CHECK_KEY_ORDER,
                             'F',
                             sRet,
                             aNode->mPageID, i,
                             (UInt)sCurKeyInfo.mRowPID,
                             (UInt)sCurKeyInfo.mRowSlotNum );
                break;
            }
            else
            {
                /* nothing to do */
            }
        }
        else
        {
            /* nothing to do */
        }
        sPrevIKey = sCurIKey;
    }

    *aIsOK = sIsOK;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*******************************************************************
 * Definition
 *
 *    To Fix BUG-15670
 *
 *    KeySequnce No�� �̿��Ͽ� Key Pointer�� ȹ���Ѵ�.
 *
 * Implementation
 *
 *    Null �� �ƴ��� ����Ǵ� ��Ȳ���� ȣ��Ǿ�� �Ѵ�.
 *
 *******************************************************************/

IDE_RC sdnbBTree::getKeyPtr( UChar         * aIndexPageNode,
                             SShort          aKeyMapSeq,
                             UChar        ** aKeyPtr )
{
    sdnbNodeHdr   * sIdxNodeHdr;
    sdnbLKey      * sLKey;
    sdnbIKey      * sIKey;
    UChar         * sKeyPtr;
    UChar         * sSlotDirPtr;

    //---------------------------------------
    // Parameter Validation
    //---------------------------------------

    IDE_DASSERT( aIndexPageNode != NULL );
    IDE_DASSERT( aKeyMapSeq >= 0 );

    //---------------------------------------
    // Key Slot ȹ��
    //---------------------------------------

    sIdxNodeHdr = (sdnbNodeHdr*)
                   sdpPhyPage::getLogicalHdrStartPtr( (UChar *) aIndexPageNode );

    sSlotDirPtr = sdpPhyPage::getSlotDirStartPtr( aIndexPageNode );

    // PROJ-1618 D$DISK_BTREE_KEY ���̺��� ����
    // �ش� �Լ��� Leaf Node ���뿡�� Internal Node�� �����ϵ��� ������
    if ( sIdxNodeHdr->mHeight == 0 )
    {
        IDE_ERROR( sdpSlotDirectory::getPagePtrFromSlotNum( sSlotDirPtr,
                                                            aKeyMapSeq,
                                                            (UChar **)&sLKey )
                   == IDE_SUCCESS );
        if ( sLKey == NULL )
        {
            ideLog::log( IDE_ERR_0,
                         "KeyMap sequence : %"ID_INT32_FMT"\n",
                         aKeyMapSeq );
            dumpIndexNode( (sdpPhyPageHdr *)aIndexPageNode );
            IDE_ASSERT( 0 );
        }

        sKeyPtr = SDNB_LKEY_KEY_PTR( sLKey );
    }
    else
    {
        IDE_ERROR( sdpSlotDirectory::getPagePtrFromSlotNum( sSlotDirPtr,
                                                            aKeyMapSeq,
                                                            (UChar **)&sIKey )
                   == IDE_SUCCESS );
        if ( sIKey == NULL )
        {
            ideLog::log( IDE_ERR_0,
                         "KeyMap sequence : %"ID_INT32_FMT"\n",
                         aKeyMapSeq );
            dumpIndexNode( (sdpPhyPageHdr *)aIndexPageNode );
            IDE_ASSERT( 0 );
        }
        else
        {
            /* nothing to do */
        }

        sKeyPtr = SDNB_IKEY_KEY_PTR(sIKey);
    }

    *aKeyPtr = sKeyPtr; 
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*******************************************************************
 * Definition
 *
 *    To Fix BUG-15670
 *
 *    Index Leaf Slot �����κ���
 *    Key Value�� Key Size�� ȹ���Ѵ�.
 *
 * Implementation
 *
 *    Null �� �ƴ��� ����Ǵ� ��Ȳ���� ȣ��Ǿ�� �Ѵ�.
 *    �� �Լ��� ù��° Į������ ������� �Ѵ�.
 *    keyColumn�� offset�� �������ְ� ȣ��Ǿ�� �Ѵ�.
 *
 *******************************************************************/
IDE_RC sdnbBTree::setMinMaxValue( sdnbHeader     * aIndex,
                                  UChar          * aIndexPageNode,
                                  SShort           aKeyMapSeq,
                                  UChar          * aTargetPtr )
{
    UChar          * sKeyPtr;
    smiColumn      * sKeyColumn;
    sdnbColumn     * sIndexColumn;
    UInt             sColumnHeaderLength;
    UInt             sColumnValueLength;

    //---------------------------------------
    // Parameter Validation
    //---------------------------------------

    IDE_DASSERT( aIndex          != NULL );
    IDE_DASSERT( aIndexPageNode  != NULL );
    IDE_DASSERT( aKeyMapSeq      >= 0 );

    IDE_ERROR( getKeyPtr( aIndexPageNode, 
                          aKeyMapSeq, 
                          &sKeyPtr ) 
               == IDE_SUCCESS ); // Key Pointer ȹ��

    // ��� �� Į�� ���� ����
    sIndexColumn = aIndex->mColumns;  //MinMax�� 0�� Į���� �̿��Ѵ�
    sKeyColumn = &(sIndexColumn->mKeyColumn);

    // Null�� �ƴ��� ����Ǿ�� ��.
    if ( isNullColumn( sIndexColumn, sKeyPtr ) != ID_FALSE )
    {
        ideLog::log( IDE_ERR_0, "KeyMapSeq : %"ID_INT32_FMT"\n", aKeyMapSeq );
        dumpHeadersAndIteratorToSMTrc( NULL,
                                       aIndex,
                                       NULL );
        dumpIndexNode( (sdpPhyPageHdr *)aIndexPageNode );
        IDE_ASSERT( 0 );
    }
    else
    {
        /* nothing to do */
    }

    getColumnLength( (aIndex->mColLenInfoList).mColLenInfo[ 0 ],
                     sKeyPtr,
                     &sColumnHeaderLength,
                     &sColumnValueLength,
                     NULL );

    if ( (sColumnValueLength + sIndexColumn->mMtdHeaderLength) > MAX_MINMAX_VALUE_SIZE )
    {
        //Length-Unknown Ÿ�Ը� 40����Ʈ��
        if ( (aIndex->mColLenInfoList).mColLenInfo[0] != SDNB_COLLENINFO_LENGTH_UNKNOWN )
        {
            dumpHeadersAndIteratorToSMTrc( NULL,
                                           aIndex,
                                           NULL );
            ideLog::log( IDE_ERR_0, "Column Value length : %"ID_UINT32_FMT"\n",
                         sColumnValueLength );

            IDE_ASSERT( 0 );
        }
        else
        {
            /* nothing to do */
        }
        sColumnValueLength = MAX_MINMAX_VALUE_SIZE - sIndexColumn->mMtdHeaderLength;
    }
    else
    {
        /* nothing to do */
    }

    sIndexColumn->mCopyDiskColumnFunc( sKeyColumn->size,
                                       aTargetPtr,
                                       0, //aCopyOffset
                                       sColumnValueLength,
                                       ((UChar *)sKeyPtr) + sColumnHeaderLength );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

/*******************************************************************
 * Definition :
 *
 *    To Fix BUG-21925
 *    Meta Page�� mIsConsistent�� �����Ѵ�.
 *
 *    ���� : Transaction�� ������� �ʱ� ������ online ���¿�����
 *           ���Ǿ�� �ȵȴ�.
 *******************************************************************/
IDE_RC sdnbBTree::setConsistent( smnIndexHeader * aIndex,
                                 idBool           aIsConsistent )
{
    idvSQL      * sStatistics;
    idvSession    sDummySession;
    idvSQL        sDummySQL;
    sdrMtx        sMtx;
    sdnbHeader  * sHeader;
    sdnbMeta    * sMeta;
    idBool        sTrySuccess;
    UInt          sState = 0;
    scSpaceID     sSpaceID;
    sdRID         sMetaRID;

    sHeader  = (sdnbHeader*)aIndex->mHeader;
    sSpaceID = sHeader->mIndexTSID;
    sMetaRID = sHeader->mMetaRID;

    sHeader->mIsConsistent = aIsConsistent;

    // BUG-27328 CodeSonar::Uninitialized Variable
    idlOS::memset( &sDummySession, 0, ID_SIZEOF(sDummySession) );

    idvManager::initSQL( &sDummySQL,
                         &sDummySession,
                         NULL,
                         NULL,
                         NULL,
                         NULL,
                         IDV_OWNER_UNKNOWN );
    sStatistics = &sDummySQL;

    IDE_TEST( sdrMiniTrans::begin( sStatistics,
                                   &sMtx,
                                   (void *)NULL,
                                   SDR_MTX_LOGGING,
                                   ID_TRUE,/*Undoable(PROJ-2162)*/
                                   SM_DLOG_ATTR_DEFAULT | SM_DLOG_ATTR_DUMMY )
              != IDE_SUCCESS );
    sState = 1;

    IDE_TEST( sdbBufferMgr::getPageByRID( sStatistics,
                                          sSpaceID,
                                          sMetaRID,
                                          SDB_X_LATCH,
                                          SDB_WAIT_NORMAL,
                                          &sMtx,
                                          (UChar **)&sMeta,
                                          &sTrySuccess )
              != IDE_SUCCESS );


    IDE_TEST( sdrMiniTrans::writeNBytes( &sMtx,
                                         (UChar *)&sMeta->mIsConsistent,
                                         (void *)&aIsConsistent,
                                         ID_SIZEOF(aIsConsistent) )
              != IDE_SUCCESS );

    sState = 0;
    IDE_TEST( sdrMiniTrans::commit( &sMtx ) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if ( sState == 1 )
    {
        (void)sdrMiniTrans::rollback( &sMtx );
    }

    return IDE_FAILURE;
}

/* BUG-27118 BTree�� ������ Node�� �Ŵ޸�ü �ִ� ��Ȳ�� ����
 * Disk �ε��� Integrity �˻縦 �����ؾ� �մϴ�.
 *
 * ExpectedPrevPID => ������ Ž���� LeafNode��
 *                    Ž���� ���� ����� PreviousPID�� �����ؾ� ��
 * ExpectedCurrentPID => ������ Ž���� LeafNode�� NextPID.
 *                       �� Ž���� ���� ����� ���� PID�� �����ؾ��� */

IDE_RC sdnbBTree::verifyIndexIntegrity( idvSQL*  aStatistics,
                                        void  *  aIndex )
{
    sdnbHeader    * sIndex              = (sdnbHeader *)aIndex;
    scPageID        sExpectedPrevPID    = SC_NULL_PID;
    scPageID        sExpectedCurrentPID = SC_NULL_PID;

    // To fix BUG-17996
    IDE_TEST_RAISE( sIndex->mIsConsistent != ID_TRUE,
                    ERR_INCONSISTENT_INDEX );

    if ( sIndex->mRootNode != SD_NULL_PID )
    {
        IDE_TEST( traverse4VerifyIndexIntegrity( aStatistics,
                                                 sIndex,
                                                 sIndex->mRootNode,
                                                 NULL, // aParentSlot  (Root�� Parents�� ����)
                                                 NULL, // aRightParentSlot (Root�� Parents�� ����)
                                                 &sExpectedPrevPID,
                                                 &sExpectedCurrentPID )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_INCONSISTENT_INDEX )
    {
        ideLog::log( IDE_SM_0, SM_TRC_INDEX_INTEGRITY_CHECK_THE_INDEX_IS_NOT_CONSISTENT );
    }
    IDE_EXCEPTION_END;

    /*
     * BUG-23699  Index integrity check �������� integrity�� ���� index��
     *            undo���� ���ܽ��Ѿ� �մϴ�.
     */
    sIndex->mIsConsistent = ID_FALSE;

    return IDE_FAILURE;
}


/* BUG-27118 BTree�� ������ Node�� �Ŵ޸�ü �ִ� ��Ȳ�� ����
 * Disk �ε��� Integrity �˻縦 �����ؾ� �մϴ�.
 *
 *                +-----+
 * Root           |  1  |
 *                +-----+
 *               /       \
 *            +---+     +---+
 * SemiLeaf   | 2 |     | 4 |
 *            +---+     +---+
 *           /   \       /  \
 * Leaf   +---+ +---+ +---+ +---+
 *        | 3 | | 4 | | 5 | | 6 |
 *        +---+ +---+ +---+ +---+
 *
 * Integrity �˻� �˰���
 *
 * a. 1�� ���(Root)�� �˻�
 *    Root�� ���� ����(InteralKeyOrder) �� SMOȮ���մϴ�.
 *
 * b. 2�� ���(Internal - SemiLeaf)�� �˻�
 *    Root�� LeftMostChildNode�� 2�� Node�� �˻��մϴ�.
 *    ���� ����(InternalKeyOrder) �� SMO ��ȣ�� Ȯ���մϴ�.
 *    SemiLeaf( Leaf �ٷ� ���� Height�� 1�� Interal node)�̱� ������,
 *  �ڽ��� �ڽ� Node�� ���� LeafIntegrityCheck�� �õ��մϴ�.
 *
 * c. 3�� ���(Leaf)�� �˻�
 *    Leaf�� �´���, IndexPage�� �´���, SMO ��ȣ�� �´���, ���Ļ���
 *  (LeafKeyOrder)�� �˻��մϴ�.
 *    ���� LeafNode(4�����)���� ��ũ�� �˻��ϱ� ����, �� �����
 * PID(ExpectedPrevPID)�� �� ����� NextPID(ExpectedCurrentPID)�� ���ϴ�.
 *
 * d. 4�� ���(Leaf)�� �˻�
 *    3���� ����, LeafNode�� ���� Integrity�� üũ�մϴ�.
 *    �����ص� 3�� ����� PID �� 3���� Next Link�� �������� 4���� �����
 *  ����Ǿ� �ִ��� Ȯ�� ��, ExpectedPrevPID�� ExpectedCurrentPID�� 4��
 *  ����� ������ �ٲߴϴ�.
 *
 * e. 4�� ���(Internal - SemiLeaf)�� �˻� (b�� ����. ���� �ݺ�) */

IDE_RC sdnbBTree::traverse4VerifyIndexIntegrity( idvSQL*      aStatistics,
                                                 sdnbHeader * aIndex,
                                                 scPageID     aPageID,
                                                 sdnbIKey   * aParentKey,
                                                 sdnbIKey   * aRightParentKey,
                                                 scPageID   * aExpectedPrevPID,
                                                 scPageID   * aExpectedCurrentPID)
{

    sdnbHeader     *sIndex   = (sdnbHeader *)aIndex;
    sdnbNodeHdr    *sNodeHdr = NULL;
    idBool          sIsFixed = ID_FALSE;
    sdpPhyPageHdr  *sPage    = NULL;
    idBool          sIsCorrupted = ID_FALSE;
    idBool          sIsOK    = ID_FALSE;
    idBool          sTrySuccess;
    sdnbIKey      * sIKey    = NULL;
    UShort          i;
    UShort          sSlotCount;
    UChar         * sSlotDirPtr;
    scPageID        sChildPID;
    ULong           sNodeSmoNo;
    ULong           sIndexSmoNo;
    sdnbIKey      * sPreviousIKey = NULL;

    IDE_TEST( sdbBufferMgr::fixPageByPID( aStatistics,
                                          sIndex->mIndexTSID,
                                          aPageID,
                                          (UChar **)&sPage,
                                          &sTrySuccess ) != IDE_SUCCESS );
    sIsFixed = ID_TRUE;

    if ( sPage->mIsConsistent == SDP_PAGE_INCONSISTENT )
    {
        ideLog::log( IDE_SM_0,
                     " Index Page is inconsistent. "
                     "( TBSID: %"ID_UINT32_FMT", PID: %"ID_UINT32_FMT" ) ",
                     sIndex->mIndexTSID, 
                     sPage->mPageID );

        IDE_TEST( sPage->mIsConsistent == SDP_PAGE_INCONSISTENT );
    }
    else
    {
        /* nothing to do */
    }

    sNodeHdr = (sdnbNodeHdr*)sdpPhyPage::getLogicalHdrStartPtr( (UChar *)sPage );

    // Leaf Node ���, Root �ϳ��ۿ� ���� ���
    if ( sNodeHdr->mHeight == 0 )
    {
        IDE_TEST( sIndex->mRootNode    != aPageID );
        IDE_TEST( aParentKey           != NULL    );
        IDE_TEST( aRightParentKey      != NULL    );
        IDE_TEST( *aExpectedPrevPID    != SC_NULL_PID );
        IDE_TEST( *aExpectedCurrentPID != SC_NULL_PID );

        // verifyLeafIntegrity ������ �ٽ� Fix�ϱ� ������, Unfix����
        sIsFixed = ID_FALSE;
        IDE_TEST( sdbBufferMgr::unfixPage( aStatistics,
                                           (UChar *)sPage ) != IDE_SUCCESS );

        IDE_TEST( verifyLeafIntegrity( aStatistics,
                                       sIndex,
                                       aPageID,
                                       sPreviousIKey,
                                       sIKey,
                                       aExpectedPrevPID,
                                       aExpectedCurrentPID )
                      != IDE_SUCCESS );
        IDE_CONT( SKIP_CHECK );
    }
    else
    {
        /* nothing to do */
    }

    // BUG-16798 ���ͳγ���� ���Ļ��� �˻�
    IDE_TEST( verifyInternalKeyOrder( aIndex,
                                      sPage,
                                      aParentKey,
                                      aRightParentKey,
                                      &sIsOK ) 
              != IDE_SUCCESS );
    if ( sIsOK == ID_FALSE )
    {
        sIsCorrupted = ID_TRUE;
    }
    else
    {
        /* nothing to do */
    }

    // BUG-25438 SMO# �������� �ε��� integrity �˻�. ���ͳγ�忡 ���ؼ�..
    sNodeSmoNo = sdpPhyPage::getIndexSMONo(sPage);
    getSmoNo( (void *)sIndex, &sIndexSmoNo );
    if ( sIndexSmoNo < sNodeSmoNo )
    {
        ideLog::log( IDE_SM_0,
                     SM_TRC_INDEX_INTEGRITY_CHECK_SMO_NUMBER,
                     sPage->mPageID, 
                     sIndexSmoNo, 
                     sNodeSmoNo );
        sIsCorrupted = ID_TRUE;
    }
    else
    {
        /* nothing to do */
    }

    sSlotDirPtr = sdpPhyPage::getSlotDirStartPtr( (UChar *)sNodeHdr );
    sSlotCount  = sdpSlotDirectory::getCount( sSlotDirPtr );
    sChildPID   = sNodeHdr->mLeftmostChild;

    for ( i = 0 ; i <= sSlotCount ; i++ )
    {
        if ( i == sSlotCount )
        {
            sIKey = NULL;
        }
        else
        {
            IDE_TEST( sdpSlotDirectory::getPagePtrFromSlotNum( sSlotDirPtr, 
                                                               i,
                                                               (UChar **)&sIKey )
                    != IDE_SUCCESS );
        }

        // ���� ��尡 Grandchild node�� ������ �ִٸ�
        if ( sNodeHdr->mHeight > 1 )
        {
            IDE_TEST( traverse4VerifyIndexIntegrity( aStatistics,
                                                     sIndex,
                                                     sChildPID,
                                                     sPreviousIKey,
                                                     sIKey,
                                                     aExpectedPrevPID,
                                                     aExpectedCurrentPID )
                      != IDE_SUCCESS );
        }
        else
        {
            //Height=1, �� Semileaf�� ��� LeafNode�� Integrity Check�� ȣ��
            IDE_TEST( verifyLeafIntegrity( aStatistics,
                                           sIndex,
                                           sChildPID,
                                           sPreviousIKey,
                                           sIKey,
                                           aExpectedPrevPID,
                                           aExpectedCurrentPID )
                      != IDE_SUCCESS );
        }

        if ( sIKey != NULL )
        {
            SDNB_GET_RIGHT_CHILD_PID( &sChildPID, sIKey );
            sPreviousIKey = sIKey;
        }
        else
        {
            /* nothing to do */
        }
    }// for

    sIsFixed = ID_FALSE;
    IDE_TEST( sdbBufferMgr::unfixPage( aStatistics,
                                       (UChar *)sPage ) != IDE_SUCCESS );

    IDE_TEST( sIsCorrupted == ID_TRUE );

    IDE_EXCEPTION_CONT( SKIP_CHECK );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if ( sIsFixed == ID_TRUE )
    {
        (void)sdbBufferMgr::unfixPage( aStatistics,
                                       (UChar *)sPage );
    }

    return IDE_FAILURE;
}

IDE_RC sdnbBTree::verifyLeafIntegrity( idvSQL     * aStatistics,
                                       sdnbHeader * aIndex,
                                       scPageID     aPageID,
                                       sdnbIKey   * aParentKey,
                                       sdnbIKey   * aRightParentKey,
                                       scPageID   * aExpectedPrevPID,
                                       scPageID   * aExpectedCurrentPID )
{
    sdnbHeader    * sIndex   = (sdnbHeader *)aIndex;
    idBool          sIsFixed = ID_FALSE;
    sdpPhyPageHdr * sPage    = NULL;
    sdnbNodeHdr   * sNodeHdr = NULL;
    idBool          sIsCorrupted = ID_FALSE;
    idBool          sTrySuccess;
    idBool          sIsOK    = ID_FALSE;
    ULong           sNodeSmoNo;
    ULong           sIndexSmoNo;
    UChar         * sSlotDirPtr;

    IDE_TEST( sdbBufferMgr::fixPageByPID( aStatistics,
                                          aIndex->mIndexTSID,
                                          aPageID,
                                          (UChar **)&sPage,
                                          &sTrySuccess ) != IDE_SUCCESS );
    sIsFixed = ID_TRUE;

    if ( sPage->mIsConsistent == SDP_PAGE_INCONSISTENT )
    {
        ideLog::log( IDE_SM_0,
                     " Index Page is inconsistent. "
                     "( TBSID: %"ID_UINT32_FMT", PID: %"ID_UINT32_FMT" ) ",
                     sIndex->mIndexTSID, 
                     sPage->mPageID );

        IDE_TEST( sPage->mIsConsistent == SDP_PAGE_INCONSISTENT );
    }

    sNodeHdr = (sdnbNodeHdr*)sdpPhyPage::getLogicalHdrStartPtr( (UChar *)sPage );

    // 1.1 ���� ����� ������� �˻�
    if ( sNodeHdr->mHeight != 0 )
    {
        ideLog::log( IDE_SM_0,
                      SM_TRC_INDEX_INTEGRITY_CHECK_IS_NOT_LEAF_NODE,
                      sPage->mPageID,
                      sNodeHdr->mHeight );
        sIsCorrupted = ID_TRUE;
    }

    if ( *aExpectedPrevPID != SC_NULL_PID )
    {
        if ( ( *aExpectedCurrentPID != sPage->mPageID ) ||
             ( *aExpectedPrevPID != sPage->mListNode.mPrev ) )
        {
            ideLog::log( IDE_SM_0,
                         SM_TRC_INDEX_INTEGRITY_CHECK_BROKEN_LEAF_PAGE_LINK,
                         *aExpectedPrevPID,
                         *aExpectedCurrentPID,
                         sPage->mPageID,
                         sPage->mListNode.mPrev );
            sIsCorrupted = ID_TRUE;
        }
    }
    if ( sPage->mPageType != SDP_PAGE_INDEX_BTREE )
    {
        ideLog::log( IDE_SM_0,
                      SM_TRC_INDEX_INTEGRITY_CHECK_INVALID_PAGE_TYPE,
                      sPage->mPageID,
                      sNodeHdr->mHeight);
        sIsCorrupted = ID_TRUE;
    }

    // BUG-25438 SMO# �������� �ε��� integrity �˻�. ������忡 ���ؼ�...
    sNodeSmoNo = sdpPhyPage::getIndexSMONo( sPage );
    getSmoNo( (void *)sIndex, &sIndexSmoNo );
    if ( sIndexSmoNo < sNodeSmoNo )
    {
        ideLog::log( IDE_SM_0,
                     SM_TRC_INDEX_INTEGRITY_CHECK_SMO_NUMBER,
                     sPage->mPageID, 
                     sIndexSmoNo, 
                     sNodeSmoNo);
        sIsCorrupted = ID_TRUE;
    }

    sSlotDirPtr = sdpPhyPage::getSlotDirStartPtr( (UChar *)sPage );
    if ( sdpSlotDirectory::getCount( sSlotDirPtr ) == 0 )
    {
        ideLog::log( IDE_SM_0,
                     SM_TRC_INDEX_INTEGRITY_CHECK_INVALID_SLOT_COUNT,
                     sPage->mPageID );
        sIsCorrupted = ID_TRUE;
    }

    // 1.2 BUG-16798 ��������� ���Ļ��� �˻�
    IDE_TEST( verifyLeafKeyOrder( aIndex,
                                  sPage,
                                  aParentKey,
                                  aRightParentKey,
                                  &sIsOK ) 
              != IDE_SUCCESS );
    if ( sIsOK == ID_FALSE )
    {
        sIsCorrupted = ID_TRUE;
    }

    *aExpectedCurrentPID = sdpPhyPage::getNxtPIDOfDblList( sPage );
    *aExpectedPrevPID    = aPageID;

    sIsFixed = ID_FALSE;
    IDE_TEST( sdbBufferMgr::unfixPage( aStatistics,
                                       (UChar *)sPage ) != IDE_SUCCESS );

    IDE_TEST( sIsCorrupted == ID_TRUE );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if ( sIsFixed == ID_TRUE )
    {
        (void)sdbBufferMgr::unfixPage( aStatistics,
                                       (UChar *)sPage );
    }

    return IDE_FAILURE;
}

/* TASK-4007 [SM] PBT�� ���� ��� �߰�
 * �ε��� �������� Key�� Dump�Ͽ� ������. */

/* BUG-28379 [SD] sdnbBTree::dumpNodeHdr( UChar *aPage ) ������
 * local Array�� ptr�� ��ȯ�ϰ� �ֽ��ϴ�.
 * Local Array��� OutBuf�� �޾� �����ϵ��� �����մϴ�. */
IDE_RC sdnbBTree::dump( UChar * aPage ,
                        SChar * aOutBuf,
                        UInt    aOutSize )

{
    sdnbNodeHdr         * sNodeHdr;
    UChar               * sSlotDirPtr;
    sdpSlotDirHdr       * sSlotDirHdr;
    sdpSlotEntry        * sSlotEntry;
    UShort                sSlotCount;
    UShort                sSlotNum;
    UShort                sOffset;
    SChar                 sValueBuf[ SM_DUMP_VALUE_BUFFER_SIZE ]={0};
    sdnbLKey            * sLKey;
    sdnbIKey            * sIKey;
    sdnbKeyInfo           sKeyInfo;
    scPageID              sRightChildPID;
    smSCN                 sCreateCSCN;
    sdSID                 sCreateTSS;
    smSCN                 sLimitCSCN;
    sdSID                 sLimitTSS;
    sdnbLKeyEx          * sLKeyEx;

    IDE_DASSERT( aPage != NULL );
    IDE_DASSERT( sdpPhyPage::getPageStartPtr(aPage) == aPage );

    sNodeHdr    = (sdnbNodeHdr*)sdpPhyPage::getLogicalHdrStartPtr( aPage );
    sSlotDirPtr = sdpPhyPage::getSlotDirStartPtr( (UChar *)aPage );
    sSlotDirHdr = (sdpSlotDirHdr*)sSlotDirPtr;
    sSlotCount  = sSlotDirHdr->mSlotEntryCount;

    idlOS::snprintf( aOutBuf,
                     aOutSize,
"--------------------- Index Key Begin ---------------------------------------------\n"
" No.|Offs|ChildPID mRowPID Slot USdD CC D LL S T TBKCSCN TBKCTSS TBKLSCN TBKLTSS KeyValue\n"
"----+-------+----------------------------------------------------------------------\n" );

    for ( sSlotNum=0 ; sSlotNum < sSlotCount ; sSlotNum++ )
    {
        sSlotEntry = SDP_GET_SLOT_ENTRY( sSlotDirPtr, sSlotNum );

        sOffset = SDP_GET_OFFSET( sSlotEntry );
        if ( SD_PAGE_SIZE < sOffset )
        {
            idlVA::appendFormat( aOutBuf,
                                 aOutSize,
                                 "Invalid slot Offset (%"ID_UINT32_FMT")\n",
                                 sSlotEntry );
            continue;
        }
        else
        {
            /* nothing to do */
        }

        if ( sNodeHdr->mHeight == 0 ) // Leaf node
        {
            sLKey = (sdnbLKey*)sdpPhyPage::getPagePtrFromOffset( aPage, sOffset );

            SDNB_LKEY_TO_KEYINFO( sLKey, sKeyInfo );

            ideLog::ideMemToHexStr( sKeyInfo.mKeyValue,
                                    SM_DUMP_VALUE_LENGTH,
                                    IDE_DUMP_FORMAT_VALUEONLY,
                                    sValueBuf,
                                    SM_DUMP_VALUE_BUFFER_SIZE );

            if ( SDNB_GET_TB_TYPE( sLKey ) == SDN_CTS_IN_KEY )
            {
                sLKeyEx = (sdnbLKeyEx*)sLKey;

                SDNB_GET_TBK_CSCN( sLKeyEx, &sCreateCSCN );
                SDNB_GET_TBK_CTSS( sLKeyEx, &sCreateTSS );
                SDNB_GET_TBK_LSCN( sLKeyEx, &sLimitCSCN );
                SDNB_GET_TBK_LTSS( sLKeyEx, &sLimitTSS );
            }
            else
            {
                SM_INIT_SCN( &sCreateCSCN );
                sCreateTSS = SD_NULL_SID;
                SM_INIT_SCN( &sLimitCSCN );
                sLimitTSS  = SD_NULL_SID;
            }

            idlVA::appendFormat( aOutBuf,
                                 aOutSize,
                                 "%4"ID_UINT32_FMT"|"
                                 "%4"ID_UINT32_FMT"|"
                                 "leafnode"
                                 "%8"ID_UINT32_FMT
                                 "%5"ID_UINT32_FMT
                                 "%5"ID_UINT32_FMT
                                 " %2"ID_UINT32_FMT
                                 " %1"ID_UINT32_FMT
                                 " %2"ID_UINT32_FMT
                                 " %1"ID_UINT32_FMT
                                 " %1"ID_UINT32_FMT
                                 "%8"ID_xINT32_FMT
                                 "%8"ID_UINT64_FMT
                                 "%8"ID_xINT32_FMT
                                 "%8"ID_UINT64_FMT
                                 " %s\n",
                                 sSlotNum,
                                 *sSlotEntry,
                                 sKeyInfo.mRowPID,
                                 sKeyInfo.mRowSlotNum,
                                 sKeyInfo.mKeyState,
                                 SDNB_GET_CCTS_NO( sLKey ),
                                 SDNB_GET_DUPLICATED ( sLKey ),
                                 SDNB_GET_LCTS_NO ( sLKey ),
                                 SDNB_GET_STATE ( sLKey ),
                                 SDNB_GET_TB_TYPE ( sLKey ),
#ifdef COMPILE_64BIT
                                 sCreateCSCN,
                                 sCreateTSS,
                                 sLimitCSCN,
                                 sLimitTSS,
#else
                                 ( ((ULong)sCreateCSCN.mHigh) << 32 ) |
                                     (ULong)sCreateCSCN.mLow,
                                 sCreateTSS,
                                 ( ((ULong)sLimitCSCN.mHigh) << 32 ) |
                                     (ULong)sLimitCSCN.mLow,
                                 sLimitTSS,
#endif
                                 sValueBuf );
        }
        else
        {   // InternalNode
            sIKey = (sdnbIKey*)sdpPhyPage::getPagePtrFromOffset( aPage, sOffset );

            SDNB_IKEY_TO_KEYINFO( sIKey, sKeyInfo );
            SDNB_GET_RIGHT_CHILD_PID( &sRightChildPID, sIKey );

            ideLog::ideMemToHexStr( sKeyInfo.mKeyValue,
                                    SM_DUMP_VALUE_LENGTH,
                                    IDE_DUMP_FORMAT_VALUEONLY,
                                    sValueBuf,
                                    SM_DUMP_VALUE_BUFFER_SIZE );

            idlVA::appendFormat( aOutBuf,
                                 aOutSize,
                                 "%4"ID_UINT32_FMT"|"
                                 "%4"ID_UINT32_FMT"|"
                                 "%8"ID_UINT32_FMT
                                 "%8"ID_UINT32_FMT
                                 "%5"ID_UINT32_FMT
                                 "%5"ID_UINT32_FMT
                                 " %51c"
                                 " %s\n",
                                 sSlotNum,
                                 *sSlotEntry,
                                 sRightChildPID,
                                 sKeyInfo.mRowPID,
                                 sKeyInfo.mRowSlotNum,
                                 0,   /* �Ⱦ��� ���̶� 0���� ǥ�� */
                                 ' ',
                                 sValueBuf );
        }
    }
    idlVA::appendFormat( aOutBuf,
                         aOutSize,
"--------------------- Index Key End   -----------------------------------------\n" );
    return IDE_SUCCESS;
}

/* TASK-4007 [SM] PBT�� ���� ��� �߰�
 * �ε��� �������� NodeHdr�� Dump�Ͽ� �ش�. �̶� ���� ��������
 * Leaf�������� ���, CTS�������� Dump�Ͽ� �����ش�. */

/* BUG-28379 [SD] sdnbBTree::dumpNodeHdr( UChar *aPage ) ������
 * local Array�� ptr�� ��ȯ�ϰ� �ֽ��ϴ�.
 * Local Array��� OutBuf�� �޾� �����ϵ��� �����մϴ�. */

IDE_RC sdnbBTree::dumpNodeHdr( UChar * aPage ,
                               SChar * aOutBuf ,
                               UInt    aOutSize )
{
    sdnbNodeHdr *sNodeHdr;
    UInt         sCurrentOutStrSize;

    IDE_DASSERT( aPage != NULL );
    IDE_DASSERT( sdpPhyPage::getPageStartPtr(aPage) == aPage );

    sNodeHdr = (sdnbNodeHdr*)sdpPhyPage::getLogicalHdrStartPtr( aPage );

    idlOS::snprintf( aOutBuf,
                     aOutSize,
                     "----------- Index Page Logical Header Begin ----------\n"
                     "LeftmostChild   : %"ID_UINT32_FMT"\n"
                     "NextEmptyNode   : %"ID_UINT32_FMT"\n"
                     "Height          : %"ID_UINT32_FMT"\n"
                     "UnlimitedKeyCnt : %"ID_UINT32_FMT"\n"
                     "TotalDeadKeySize: %"ID_UINT32_FMT"\n"
                     "TBKCount        : %"ID_UINT32_FMT"\n"
                     "State(I,U,E,F)  : %"ID_UINT32_FMT"\n"
                     "----------- Index Page Logical Header End ------------\n",
                     sNodeHdr->mLeftmostChild,
                     sNodeHdr->mNextEmptyNode,
                     sNodeHdr->mHeight,
                     sNodeHdr->mUnlimitedKeyCount,
                     sNodeHdr->mTotalDeadKeySize,
                     sNodeHdr->mTBKCount,
                     sNodeHdr->mState );


    // Leaf���, CTL�� Dump
    if ( sNodeHdr->mHeight == 0 )
    {
        sCurrentOutStrSize = idlOS::strlen( aOutBuf );

        // sdnIndexCTL�� Dump�� ������ �����ؾ��մϴ�.
        IDE_ASSERT( sdnIndexCTL::dump( aPage,
                                       aOutBuf + sCurrentOutStrSize,
                                       aOutSize - sCurrentOutStrSize )
                    == IDE_SUCCESS );
    }

    return IDE_SUCCESS;
}

/* TASK-4007 [SM] PBT�� ���� ��� �߰�
 * �ε��� ��Ÿ �������� Dump�Ѵ�. �ε��� ��Ÿ �������� �ƴϴ���,
 * ������ Meta�������� Dump�Ͽ� �����ش�. */

/* BUG-28379 [SD] sdnbBTree::dumpNodeHdr( UChar *aPage ) ������
 * local Array�� ptr�� ��ȯ�ϰ� �ֽ��ϴ�.
 * Local Array��� OutBuf�� �޾� �����ϵ��� �����մϴ�. */

IDE_RC sdnbBTree::dumpMeta( UChar * aPage ,
                            SChar * aOutBuf ,
                            UInt    aOutSize )
{
    sdnbMeta    *sMeta;

    IDE_DASSERT( aPage != NULL );
    IDE_DASSERT( sdpPhyPage::getPageStartPtr(aPage) == aPage );

    sMeta = (sdnbMeta*)( aPage + SMN_INDEX_META_OFFSET );

    idlOS::snprintf( aOutBuf,
                     aOutSize,
                     "----------- Index Meta Page Begin ----------\n"
                     "mRootNode               : %"ID_UINT32_FMT"\n"
                     "mEmptyNodeHead          : %"ID_UINT32_FMT"\n"
                     "mEmptyNodeTail          : %"ID_UINT32_FMT"\n"
                     "mMinNode                : %"ID_UINT32_FMT"\n"
                     "mMaxNode                : %"ID_UINT32_FMT"\n"
                     "mIsCreatedWithLogging   : %"ID_UINT32_FMT"\n"
                     "mNologgingCompletionLSN \n"
                     "                mFileNo : %"ID_UINT32_FMT"\n"
                     "                mOffset : %"ID_UINT32_FMT"\n"
                     "mIsConsistent           : %"ID_UINT32_FMT"\n"
                     "mIsCreatedWithForce     : %"ID_UINT32_FMT"\n"
                     "mFreeNodeCnt            : %"ID_UINT64_FMT"\n"
                     "mFreeNodeHead           : %"ID_UINT32_FMT"\n"
                     "----------- Index Meta Page End   ----------\n",
                     sMeta->mRootNode,
                     sMeta->mEmptyNodeHead,
                     sMeta->mEmptyNodeTail,
                     sMeta->mMinNode,
                     sMeta->mMaxNode,
                     sMeta->mIsCreatedWithLogging,
                     sMeta->mNologgingCompletionLSN.mFileNo,
                     sMeta->mNologgingCompletionLSN.mOffset,
                     sMeta->mIsConsistent,
                     sMeta->mIsCreatedWithForce,
                     sMeta->mFreeNodeCnt,
                     sMeta->mFreeNodeHead );

    return IDE_SUCCESS;
}

#ifdef DEBUG
idBool sdnbBTree::verifyPrefixPos( sdnbHeader    * aIndex,
                                   sdpPhyPageHdr * aNode,
                                   scPageID        aNewChildPID,
                                   SShort          aKeyMapSeq,
                                   sdnbKeyInfo   * aKeyInfo )
{
    SShort                 i;
    SShort                 sKeyCnt;
    sdnbIKey              *sIKey;
    sdnbKeyInfo            sKeyInfo;
    sdnbConvertedKeyInfo   sConvertedKeyInfo;
    SInt                   sRet;
    idBool                 sIsSameValue;
    idBool                 sIsOK = ID_TRUE;
    sdnbStatistic          sDummyStat;
    UChar                 *sSlotDirPtr;
    scPageID               sChildPID;
    smiValue               sSmiValueList[SMI_MAX_IDX_COLUMNS];

    // BUG-27328 CodeSonar::Uninitialized Variable
    idlOS::memset( &sDummyStat, 0, ID_SIZEOF(sDummyStat) );

    sSlotDirPtr = sdpPhyPage::getSlotDirStartPtr( (UChar *)aNode );
    sKeyCnt     = sdpSlotDirectory::getCount( sSlotDirPtr );
    sConvertedKeyInfo.mSmiValueList = (smiValue*)sSmiValueList;
    SDNB_KEYINFO_TO_CONVERTED_KEYINFO( *aKeyInfo,
                                       sConvertedKeyInfo,
                                       &aIndex->mColLenInfoList );

    for ( i = 0 ; i < aKeyMapSeq ; i++ )
    {
        IDE_ASSERT( sdpSlotDirectory::getPagePtrFromSlotNum( sSlotDirPtr, 
                                                             i,
                                                             (UChar **)&sIKey )
                    == IDE_SUCCESS );

        SDNB_IKEY_TO_KEYINFO( sIKey, sKeyInfo );
        SDNB_GET_RIGHT_CHILD_PID( &sChildPID, sIKey);

        sRet = compareConvertedKeyAndKey( &sDummyStat,
                                          aIndex->mColumns,
                                          aIndex->mColumnFence,
                                          &sConvertedKeyInfo,
                                          &sKeyInfo,
                                          ( SDNB_COMPARE_VALUE |
                                            SDNB_COMPARE_PID   |
                                            SDNB_COMPARE_OFFSET ),
                                          &sIsSameValue );
        if ( sRet <= 0 )
        {
            sIsOK = ID_FALSE;
            ideLog::log( IDE_SM_0,
                         "PID(%"ID_UINT32_FMT"), "
                         "SLOT(%"ID_UINT32_FMT"), "
                         "ROW_PID(%"ID_UINT32_FMT"), "
                         "ROW_SLOTNUM(%"ID_UINT32_FMT")\n ",
                         aNode->mPageID, 
                         i,
                         (UInt)sKeyInfo.mRowPID,
                         (UInt)sKeyInfo.mRowSlotNum );
            IDE_RAISE( PAGE_CORRUPTED );
        }
        else
        {
            /* nothing to do */
        }

        if ( ( aNewChildPID != SD_NULL_PID ) &&
             ( aNewChildPID == sChildPID ) )
        {
            sIsOK = ID_FALSE;
            ideLog::log( IDE_SM_0,
                         "RIGHT CHILD PID(%"ID_UINT32_FMT"), "
                         "SLOT(%"ID_UINT32_FMT"), "
                         "ROW_PID(%"ID_UINT32_FMT"), "
                         "ROW_SLOTNUM(%"ID_UINT32_FMT")\n ",
                         sChildPID, 
                         i,
                         (UInt)sKeyInfo.mRowPID,
                         (UInt)sKeyInfo.mRowSlotNum );
            IDE_RAISE( PAGE_CORRUPTED );
        }
        else
        {
            /* nothing to do */
        }

    }//for

    for ( i =  aKeyMapSeq ; i < sKeyCnt ; i++ )
    {
        IDE_ASSERT( sdpSlotDirectory::getPagePtrFromSlotNum( sSlotDirPtr, 
                                                             i,
                                                             (UChar **)&sIKey )
                    == IDE_SUCCESS );
        SDNB_IKEY_TO_KEYINFO( sIKey, sKeyInfo );
        SDNB_GET_RIGHT_CHILD_PID( &sChildPID, sIKey );

        sRet = compareConvertedKeyAndKey( &sDummyStat,
                                          aIndex->mColumns,
                                          aIndex->mColumnFence,
                                          &sConvertedKeyInfo,
                                          &sKeyInfo,
                                          ( SDNB_COMPARE_VALUE  |
                                            SDNB_COMPARE_PID    |
                                            SDNB_COMPARE_OFFSET ),
                                          &sIsSameValue );
        if ( sRet >= 0 )
        {
            sIsOK = ID_FALSE;
            ideLog::log( IDE_SM_0,
                         "PID(%"ID_UINT32_FMT"), "
                         "SLOT(%"ID_UINT32_FMT"), "
                         "ROW_PID(%"ID_UINT32_FMT"), "
                         "ROW_SLOTNUM(%"ID_UINT32_FMT")\n ",
                         aNode->mPageID, 
                         i,
                         (UInt)sKeyInfo.mRowPID,
                         (UInt)sKeyInfo.mRowSlotNum );
            IDE_RAISE( PAGE_CORRUPTED );
        }
        else
        {
            /* nothing to do */
        }

        if ( ( aNewChildPID != SD_NULL_PID ) &&
             ( aNewChildPID == sChildPID ) )
        {
            sIsOK = ID_FALSE;
            ideLog::log( IDE_SM_0,
                         "RIGHT CHILD PID(%"ID_UINT32_FMT"), "
                         "SLOT(%"ID_UINT32_FMT"), "
                         "ROW_PID(%"ID_UINT32_FMT"), "
                         "ROW_SLOTNUM(%"ID_UINT32_FMT")\n ",
                         sChildPID, i,
                         (UInt)sKeyInfo.mRowPID,
                         (UInt)sKeyInfo.mRowSlotNum );
            IDE_RAISE( PAGE_CORRUPTED );
        }
        else
        {
            /* nothing to do */
        }
    }//for

    /* BUG-40385 sIsOK ���� ���� Failure ������ �� �����Ƿ�,
     * ���� IDE_TEST_RAISE -> IDE_TEST_CONT �� ��ȯ���� �ʴ´�. */
    IDE_EXCEPTION_CONT( PAGE_CORRUPTED );

    return sIsOK;
}
#endif

/**********************************************************************
 * Description: aIterator�� ���� ����Ű�� �ִ� Row�� ���ؼ� XLock��
 *              ȹ���մϴ�.
 *
 * aProperties - [IN] Index Iterator
 *
 * Related Issue:
 *   BUG-19068: smiTableCursor�� ���簡��Ű�� �ִ� Row�� ���ؼ�
 *              Lock�� ������ �մ� Interface�� �ʿ��մϴ�.
 *
 *********************************************************************/
IDE_RC sdnbBTree::lockRow( sdnbIterator* aIterator )
{
    scSpaceID sTableTSID;

    sTableTSID = ((smcTableHeader*)aIterator->mTable)->mSpaceID;

    if ( aIterator->mCurRowPtr == aIterator->mCacheFence )
    {
        dumpHeadersAndIteratorToSMTrc( (smnIndexHeader*)aIterator->mIndex,
                                       NULL,
                                       aIterator );
        IDE_ASSERT( 0 );
    }
    else
    {
        /* nothing to do */
    }

    return sdnManager::lockRow( aIterator->mProperties,
                                aIterator->mTrans,
                                &(aIterator->mSCN),
                                &(aIterator->mInfinite),
                                sTableTSID,
                                SD_MAKE_SID_FROM_GRID( aIterator->mRowGRID ),
                                aIterator->mStatement->isForbiddenToRetry() );
}

/**********************************************************************
 * Description: aKeyInfo�� ���� ���� ���� Ű�� aLeafNode ������ ã�´�.
 *              aSlotSeq�� ���� aDirect ���⿡ ���� Ž���Ѵ�.
 *
 * Related Issue:
 *   BUG-32313: The values of DRDB index Cardinality converge on 0
 *********************************************************************/
IDE_RC sdnbBTree::findSameValueKey( sdnbHeader      * aIndex,
                                    sdpPhyPageHdr   * aLeafNode,
                                    SInt              aSlotSeq,
                                    sdnbKeyInfo     * aKeyInfo,
                                    UInt              aDirect,
                                    sdnbLKey       ** aFindSameKey )
{
    sdnbLKey    * sFindKey  = NULL;
    sdnbLKey    * sKey      = NULL;
    sdnbKeyInfo   sFindKeyInfo;
    idBool        sIsSameValue = ID_FALSE;
    UChar       * sSlotDirPtr;
    SInt          sSlotCount = 0;
    SInt          i;

    sSlotDirPtr = sdpPhyPage::getSlotDirStartPtr( (UChar *)aLeafNode );
    sSlotCount  = sdpSlotDirectory::getCount( sSlotDirPtr );

    switch ( aDirect )
    {
        case SDNB_FIND_PREV_SAME_VALUE_KEY:
            {
                for ( i = aSlotSeq ; i >= 0 ; i-- )
                {
                    IDE_TEST(sdpSlotDirectory::getPagePtrFromSlotNum(
                                                              sSlotDirPtr, 
                                                              i,
                                                              (UChar **)&sKey )
                             != IDE_SUCCESS );

                    if ( isIgnoredKey4NumDistStat( aIndex, sKey ) != ID_TRUE )
                    {
                        sFindKey = sKey;
                        break;
                    }
                }
            }
            break;
        case SDNB_FIND_NEXT_SAME_VALUE_KEY:
            {
                for ( i = aSlotSeq ; i < sSlotCount ; i++ )
                {
                    IDE_TEST( sdpSlotDirectory::getPagePtrFromSlotNum(
                                                              sSlotDirPtr, 
                                                              i,
                                                              (UChar **)&sKey )
                              != IDE_SUCCESS );

                    if ( isIgnoredKey4NumDistStat( aIndex, sKey ) != ID_TRUE )
                    {
                        sFindKey = sKey;
                        break;
                    }
                    else
                    {
                        /* nothing to do */
                    }
                }
            }
            break;
        default:
            IDE_ASSERT( 0 );
    }

    if ( sFindKey != NULL )
    {
        sFindKeyInfo.mKeyValue = SDNB_LKEY_KEY_PTR(sFindKey);

        (void)compareKeyAndKey( &aIndex->mQueryStat,
                                aIndex->mColumns,
                                aIndex->mColumnFence,
                                &sFindKeyInfo,
                                aKeyInfo,
                                SDNB_COMPARE_VALUE,
                                &sIsSameValue );

        if ( sIsSameValue == ID_FALSE )
        {
            sFindKey = NULL;
        }
        else
        {
            /* nothing to do */
        }
    }
    else
    {
        /* nothing to do */
    }

    *aFindSameKey = sFindKey;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/* 
 * BUG-35163 - [sm_index] [SM] add some exception properties for __DBMS_STAT_METHOD
 *
 * __DBMS_STAT_METHOD ������Ƽ�� MANUAL �� ��� DRDB�� ���ܷ� AUTO�� �����ϰ� ������
 * __DBMS_STAT_METHOD_FOR_DRDB ������Ƽ�� ����Ѵ�.
 */
inline idBool sdnbBTree::needToUpdateStat()
{
    idBool sRet;

    if ( smuProperty::getDBMSStatMethod() == SMU_DBMS_STAT_METHOD_AUTO )
    {
        sRet = ID_TRUE;
    }
    else
    {
        if ( smuProperty::getDBMSStatMethod4DRDB() == SMU_DBMS_STAT_METHOD_AUTO )
        {
            sRet = ID_TRUE;
        }
        else
        {
            sRet = ID_FALSE;
        }
    }

    return sRet;
}

void sdnbBTree::dumpHeadersAndIteratorToSMTrc( 
                                            smnIndexHeader * aCommonHeader,
                                            sdnbHeader     * aRuntimeHeader,
                                            sdnbIterator   * aIterator )
{
    SChar    * sOutBuffer4Dump;

    if ( ( aCommonHeader == NULL ) && ( aIterator != NULL ) )
    {
        aCommonHeader = (smnIndexHeader*)aIterator->mIndex;
    }
    else
    {
        /* nothing to do */
    }

    if ( ( aRuntimeHeader == NULL ) && ( aCommonHeader != NULL ) )
    {
        aRuntimeHeader = (sdnbHeader*) aCommonHeader->mHeader;
    }
    else
    {
        /* nothing to do */
    }

    if ( iduMemMgr::calloc( IDU_MEM_SM_SDN, 
                            1,
                            ID_SIZEOF( SChar ) * IDE_DUMP_DEST_LIMIT,
                            (void **)&sOutBuffer4Dump )
        == IDE_SUCCESS )
    {
        if ( aCommonHeader != NULL )
        {
            if ( smnManager::dumpCommonHeader( aCommonHeader,
                                               sOutBuffer4Dump,
                                               IDE_DUMP_DEST_LIMIT )
                == IDE_SUCCESS )
            {
                ideLog::log( IDE_ERR_0, "%s\n", sOutBuffer4Dump );
            }
            else
            {
                /* nothing to do */
            }
        }
        else
        {
            /* nothing to do */
        }

        if ( aRuntimeHeader != NULL )
        {
            if ( sdnbBTree::dumpRuntimeHeader( aRuntimeHeader,
                                               sOutBuffer4Dump,
                                               IDE_DUMP_DEST_LIMIT )
                == IDE_SUCCESS )
            {
                ideLog::log( IDE_ERR_0, "%s\n", sOutBuffer4Dump );
            }
            else
            {
                /* nothing to do */
            }
        }
        else
        {
            /* nothing to do */
        }

        if ( aIterator != NULL )
        {
            if ( sdnbBTree::dumpIterator( aIterator, 
                                          sOutBuffer4Dump,
                                          IDE_DUMP_DEST_LIMIT )
                == IDE_SUCCESS )
            {
                ideLog::log( IDE_ERR_0, "%s\n", sOutBuffer4Dump );
            }
            else
            {
                /* nothing to do */
            }
        }
        else
        {
            /* nothing to do */
        }

        IDE_ASSERT( iduMemMgr::free( sOutBuffer4Dump ) == IDE_SUCCESS );
    }

    if ( aCommonHeader != NULL )
    {
        ideLog::logMem( IDE_DUMP_0, 
                        (UChar *)aCommonHeader, 
                        ID_SIZEOF( smnIndexHeader ),
                        "Index Persistent Header(hex):" );
    }
    else
    {
        /* nothing to do */
    }

    if ( aRuntimeHeader != NULL )
    {
        ideLog::logMem( IDE_DUMP_0, 
                        (UChar *)aRuntimeHeader, 
                        ID_SIZEOF( sdnbHeader ),
                        "Index Runtime Header(hex):" );
    }
    else
    {
        /* nothing to do */
    }

    if ( aIterator != NULL )
    {
        ideLog::logMem( IDE_DUMP_0, 
                        (UChar *)aIterator, 
                        ID_SIZEOF( sdnbIterator ),
                        "Index Iterator(hex):" );
    }
    else
    {
        /* nothing to do */
    }
}

/* BUG-31845 [sm-disk-index] Debugging information is needed for 
 * PBT when fail to check visibility using DRDB Index.
 * ������ Dump �ڵ� �߰� */
IDE_RC sdnbBTree::dumpRuntimeHeader( sdnbHeader * aHeader,
                                     SChar      * aOutBuf,
                                     UInt         aOutSize )
{
    SChar         sValueBuf[ SM_DUMP_VALUE_BUFFER_SIZE ]={0};
    sdnbColumn  * sColumn;

    idlOS::snprintf( aOutBuf,
                     aOutSize,
                     "DRDB Index Runtime Header:\n"
                     "-smnRuntimeHeader (common)\n"
                     "mIsCreated                : %"ID_UINT32_FMT"\n"
                     "mIsConsistent             : %"ID_UINT32_FMT"\n"
                     "\n-sdnRuntimeHeader (disk common)\n"
                     "mTableTSID                : %"ID_UINT32_FMT"\n"
                     "mIndexTSID                : %"ID_UINT32_FMT"\n"
                     "mMetaRID                  : %"ID_UINT64_FMT"\n"
                     "mTableOID                 : %"ID_vULONG_FMT"\n"
                     "mIndexID                  : %"ID_UINT32_FMT"\n"
                     "mSmoNo                    : %"ID_UINT32_FMT"\n"
                     "mIsUnique                 : %"ID_UINT32_FMT"\n"
                     "mIsNotNull                : %"ID_UINT32_FMT"\n"
                     "mLogging                  : %"ID_UINT32_FMT"\n"
                     "mIsCreatedWithLogging     : %"ID_UINT32_FMT"\n"
                     "mIsCreatedWithForce       : %"ID_UINT32_FMT"\n"
                     "mCompletionLSN.mFileNo    : %"ID_UINT32_FMT"\n"
                     "mCompletionLSN.mOffset    : %"ID_UINT32_FMT"\n"
                     "\n-BTree\n"
                     "mRootNode                 : %"ID_UINT32_FMT"\n"
                     "mEmptyNodeHead            : %"ID_UINT32_FMT"\n"
                     "mEmptyNodeTail            : %"ID_UINT32_FMT"\n"
                     "mMinNode                  : %"ID_UINT32_FMT"\n"
                     "mMaxNode                  : %"ID_UINT32_FMT"\n"
                     "mFreeNodeCnt              : %"ID_UINT32_FMT"\n"
                     "mFreeNodeHead             : %"ID_UINT32_FMT"\n",
                     aHeader->mIsCreated,
                     aHeader->mIsConsistent,
                     aHeader->mTableTSID,
                     aHeader->mIndexTSID,
                     aHeader->mMetaRID,
                     aHeader->mTableOID,
                     aHeader->mIndexID,
                     aHeader->mSmoNo,
                     aHeader->mIsUnique,
                     aHeader->mIsNotNull,
                     aHeader->mLogging,
                     aHeader->mIsCreatedWithLogging,
                     aHeader->mIsCreatedWithForce,
                     aHeader->mCompletionLSN.mFileNo,
                     aHeader->mCompletionLSN.mOffset,
                     aHeader->mRootNode,
                     aHeader->mEmptyNodeHead,
                     aHeader->mEmptyNodeTail,
                     aHeader->mMinNode,
                     aHeader->mMaxNode,
                     aHeader->mFreeNodeCnt,
                     aHeader->mFreeNodeHead );

    for ( sColumn  = aHeader->mColumns ; 
          sColumn != aHeader->mColumnFence ; 
          sColumn ++ )
    {
        idlVA::appendFormat( aOutBuf,
                             aOutSize,
                             "Column\n"
                             "sColumn->mtdHeaderLen : %"ID_UINT32_FMT"\n"
                             "sColumn->Key.id       : %"ID_UINT32_FMT"\n"
                             "sColumn->Key.flag     : %"ID_UINT32_FMT"\n"
                             "sColumn->Key.offset   : %"ID_UINT32_FMT"\n"
                             "sColumn->Key.size     : %"ID_UINT32_FMT"\n"
                             "sColumn->VRow.id      : %"ID_UINT32_FMT"\n"
                             "sColumn->VRow.flag    : %"ID_UINT32_FMT"\n"
                             "sColumn->VRow.offset  : %"ID_UINT32_FMT"\n"
                             "sColumn->VRow.size    : %"ID_UINT32_FMT"\n",
                             sColumn->mMtdHeaderLength,
                             sColumn->mKeyColumn.id,
                             sColumn->mKeyColumn.flag,
                             sColumn->mKeyColumn.offset,
                             sColumn->mKeyColumn.size,
                             sColumn->mVRowColumn.id,
                             sColumn->mVRowColumn.flag,
                             sColumn->mVRowColumn.offset,
                             sColumn->mVRowColumn.size );
    }

    (void)ideLog::ideMemToHexStr( (UChar *)&(aHeader->mColLenInfoList),
                                  ID_SIZEOF( sdnbColLenInfoList ),
                                  IDE_DUMP_FORMAT_VALUEONLY,
                                  sValueBuf,
                                  SM_DUMP_VALUE_BUFFER_SIZE );
    idlVA::appendFormat( aOutBuf,
                         aOutSize,
                         "mColLenInfo : %s",
                         sValueBuf );

    return IDE_SUCCESS;
}

IDE_RC sdnbBTree::dumpIterator( sdnbIterator * aIterator,
                                SChar        * aOutBuf,
                                UInt           aOutSize )
{
    sdnbRowCache * sRowCache;
    SChar        * sDumpBuf;
    sdSID          sTSSlotSID;
    smSCN          sMyFstDskViewSCN;
    smSCN          sSysMinDskViewSCN;
    smSCN          sMyMinDskViewSCN;
    UInt           i;

    // Transaction Infomation
    sTSSlotSID       = smLayerCallback::getTSSlotSID( aIterator->mTrans );
    sMyFstDskViewSCN = smLayerCallback::getFstDskViewSCN( aIterator->mTrans );
    SMX_GET_MIN_DISK_VIEW( &sSysMinDskViewSCN );
    sMyMinDskViewSCN = ( (smxTrans*) aIterator->mTrans )->getMinDskViewSCN();

    idlOS::snprintf( aOutBuf,
                     aOutSize,
                     "DRDB Index Iterator:\n"
                     "\n-common\n"
                     "mSCN                : 0x%"ID_xINT64_FMT"\n"
                     "mInfinite           : 0x%"ID_xINT64_FMT"\n"
                     "mRowGRID.mSpaceID   : %"ID_UINT32_FMT"\n"
                     "mRowGRID.mOffset    : %"ID_UINT32_FMT"\n"
                     "mRowGRID.mPageID    : %"ID_UINT32_FMT"\n"
                     "mTID                : %"ID_UINT32_FMT"\n"
                     "mFlag               : %"ID_UINT32_FMT"\n"
                     "\n-for Transaction\n"
                     "sTSSlotSID          : 0x%"ID_xINT64_FMT"\n"
                     "sMyFstDskViewSCN    : 0x%"ID_xINT64_FMT"\n"
                     "sSysMinDskViewSCN   : 0x%"ID_xINT64_FMT"\n"
                     "sMyMinDskViewSCN    : 0x%"ID_xINT64_FMT"\n"
                     "\n-for BTree\n"
                     "mCurLeafNode        : %"ID_UINT32_FMT"\n"
                     "mNextLeafNode       : %"ID_UINT32_FMT"\n"
                     "mScanBackNode       : %"ID_UINT32_FMT"\n"
                     "mCurNodeSmoNo       : %"ID_UINT32_FMT"\n"
                     "mIndexSmoNo         : %"ID_UINT32_FMT"\n"
                     "mIsLastNodeInRange  : %"ID_UINT32_FMT"\n"
                     "mCurNodeLSN.mFileNo : %"ID_UINT32_FMT"\n"
                     "mCurNodeLSN.mOffset : %"ID_UINT32_FMT"\n",
                     SM_SCN_TO_LONG( aIterator->mSCN ),
                     SM_SCN_TO_LONG( aIterator->mInfinite ),
                     aIterator->mRowGRID.mSpaceID,
                     aIterator->mRowGRID.mOffset,
                     aIterator->mRowGRID.mPageID,
                     aIterator->mTID,
                     aIterator->mFlag,
                     sTSSlotSID,
                     SM_SCN_TO_LONG( sMyFstDskViewSCN ),
                     SM_SCN_TO_LONG( sSysMinDskViewSCN ),
                     SM_SCN_TO_LONG( sMyMinDskViewSCN ),
                     aIterator->mCurLeafNode,
                     aIterator->mNextLeafNode,
                     aIterator->mScanBackNode,
                     aIterator->mCurNodeSmoNo,
                     aIterator->mIndexSmoNo,
                     aIterator->mIsLastNodeInRange,
                     aIterator->mCurNodeLSN.mFileNo,
                     aIterator->mCurNodeLSN.mOffset );

    for ( i = 0 ;
          ( i < SDNB_MAX_ROW_CACHE ) &&
          ( &(aIterator->mRowCache[ i ]) != aIterator->mCacheFence );
          i++ )
    {
        sRowCache = &(aIterator->mRowCache[ i ]);

        idlVA::appendFormat( aOutBuf,
                             aOutSize,
                             "[%2"ID_UINT32_FMT" %c]"
                             " PID:%-8"ID_UINT32_FMT
                             " SID:%-8"ID_UINT32_FMT
                             " Off:%-8"ID_UINT32_FMT"\n",
                             i,
                             ( sRowCache == aIterator->mCurRowPtr ) 
                             ? 'C' : ' ',
                             sRowCache->mRowPID,
                             sRowCache->mRowSlotNum,
                             sRowCache->mOffset );
    }


    if ( iduMemMgr::calloc( IDU_MEM_SM_SDN, 
                            1,
                            ID_SIZEOF( SChar ) * IDE_DUMP_DEST_LIMIT,
                            (void **)&sDumpBuf )
         == IDE_SUCCESS )
    {
        ideLog::ideMemToHexStr( (UChar *)aIterator->mPage,
                                SD_PAGE_SIZE,
                                IDE_DUMP_FORMAT_NORMAL,
                                sDumpBuf,
                                IDE_DUMP_DEST_LIMIT );
        idlVA::appendFormat( aOutBuf,
                             aOutSize,
                             "CacheDump:\n%s\n",
                             sDumpBuf );

        IDE_ASSERT( iduMemMgr::free( sDumpBuf ) == IDE_SUCCESS );
    }  
    else
    {
        /* nothing to do */
    }

    return IDE_SUCCESS;
}

IDE_RC sdnbBTree::dumpStack( sdnbStack * aStack,
                             SChar     * aOutBuf,
                             UInt        aOutSize )
{
    SInt i;

    idlOS::snprintf( aOutBuf,
                     aOutSize,
                     "DRDB Index Stack:\n"
                     "sStack.mIndexDepth                : %"ID_INT32_FMT"\n"
                     "sStack.mCurrentDepth              : %"ID_INT32_FMT"\n"
                     "sStack.mIsSameValueInSiblingNodes : %"ID_UINT32_FMT"\n"
                     "      mNode      mNextNode  mSmoNo     KMSeq NSSiz\n",
                     aStack->mIndexDepth,
                     aStack->mCurrentDepth,
                     aStack->mIsSameValueInSiblingNodes );

    for ( i = 0 ; i <= aStack->mIndexDepth ; i ++ )
    {
        idlVA::appendFormat( aOutBuf,
                             aOutSize,
                             "[%3"ID_UINT32_FMT"] "
                             "%10"ID_UINT32_FMT" "
                             "%10"ID_UINT32_FMT" "
                             "%10"ID_UINT64_FMT" "
                             "%5"ID_UINT32_FMT" "
                             "%5"ID_UINT32_FMT"\n",
                             i,
                             aStack->mStack[ i ].mNode,
                             aStack->mStack[ i ].mNextNode,
                             aStack->mStack[ i ].mSmoNo,
                             aStack->mStack[ i ].mKeyMapSeq,
                             aStack->mStack[ i ].mNextSlotSize );
    }

    return IDE_SUCCESS;
}


IDE_RC sdnbBTree::dumpKeyInfo( sdnbKeyInfo        * aKeyInfo,
                               sdnbColLenInfoList * aColLenInfoList,
                               SChar              * aOutBuf,
                               UInt                 aOutSize )
{
    SChar sValueBuf[ SM_DUMP_VALUE_BUFFER_SIZE ]={0};
    UInt  sKeyValueLength;

    IDE_TEST_CONT( aKeyInfo == NULL, SKIP );

    idlOS::snprintf( aOutBuf,
                     aOutSize,
                     "DRDB Index KeyInfo:\n"
                     "sKeyInfo.mRowPID      : %"ID_UINT32_FMT"\n"
                     "sKeyInfo.mRowSlotNum  : %"ID_UINT32_FMT"\n"
                     "sKeyInfo.mKeyState    : %"ID_UINT32_FMT"\n",
                     aKeyInfo->mRowPID,
                     aKeyInfo->mRowSlotNum,
                     aKeyInfo->mKeyState );

    if ( ( aColLenInfoList != NULL ) && ( aKeyInfo->mKeyValue != NULL ) )
    {
        sKeyValueLength = getKeyValueLength( aColLenInfoList,
                                             aKeyInfo->mKeyValue );

        ideLog::ideMemToHexStr( aKeyInfo->mKeyValue,
                                sKeyValueLength,
                                IDE_DUMP_FORMAT_VALUEONLY,
                                sValueBuf,
                                SM_DUMP_VALUE_BUFFER_SIZE );

        idlVA::appendFormat( aOutBuf,
                             aOutSize,
                             "%s\n",
                             sValueBuf );
    }
    else
    {
        /* nothing to do */
    }

    IDE_EXCEPTION_CONT( SKIP );

    return IDE_SUCCESS;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : sdnbBTree::allTBKStamping                  *
 * ------------------------------------------------------------------*
 * ��忡 ���Ե� ��� TBK�� stamping�Ѵ�. (BUG-44973)                *
 *                                                                   *
 * - ���� ������� �����Ѵ�.                                         * 
 *   1. ��忡�� stamping ������ TBK key ����Ʈ�� �����.            *
 *   2. TBK key ����Ʈ�� �α��Ѵ�.                                   *
 *   3. TBK key ����Ʈ�� ��ϵ� key���� stamping�Ѵ�.                *
 *********************************************************************/
IDE_RC sdnbBTree::allTBKStamping( idvSQL        * aStatistics,
                                  sdnbHeader    * aIndex,
                                  sdrMtx        * aMtx,
                                  sdpPhyPageHdr * aPage,
                                  UShort        * aStampingCount )
{
    UInt            i               = 0;
    UChar         * sSlotDirPtr     = 0;
    UShort          sSlotCount      = 0;
    sdnbLKey      * sLeafKey        = NULL;
    UShort          sDeadKeySize    = 0;
    UShort          sTBKCount       = 0; /* mTBKCount �������ؼ� */
    UShort          sDeadTBKCount   = 0;
    UShort          sStampingCount  = 0;
    sdnbNodeHdr   * sNodeHdr        = NULL;
    UChar           sCTSInKey       = SDN_CTS_IN_KEY; /* recovery�� �̰����� TBK STAMPING�� Ȯ���Ѵ�.(BUG-44973) */
    ULong           sTempBuf[SD_PAGE_SIZE / ID_SIZEOF(ULong)];
    UChar         * sKeyList        = (UChar *)sTempBuf;
    UShort          sKeyListSize    = 0;
    scOffset        sSlotOffset     = 0;
    idBool          sIsCSCN         = ID_FALSE;
    idBool          sIsLSCN         = ID_FALSE;
    sdnbTBKStamping sKeyEntry;

    sSlotDirPtr = sdpPhyPage::getSlotDirStartPtr( (UChar *)aPage );
    sSlotCount  = sdpSlotDirectory::getCount( sSlotDirPtr );

    /* 1. stamping ������ TBK key LIST�� �����. */ 
    for ( i = 0 ; i < sSlotCount ; i++ )
    {
        IDE_TEST( sdpSlotDirectory::getPagePtrFromSlotNum( sSlotDirPtr,
                                                           i,
                                                           (UChar **)&sLeafKey )
                  != IDE_SUCCESS );

        if ( SDNB_GET_TB_TYPE( sLeafKey ) == SDNB_KEY_TB_CTS )
        {
            continue;
        }
        else
        {
            sTBKCount++;
        }

        IDE_TEST( checkTBKStamping( aStatistics,
                                    aPage,
                                    sLeafKey,
                                    &sIsCSCN,
                                    &sIsLSCN ) != IDE_SUCCESS );

        if ( ( sIsCSCN == ID_TRUE ) ||
             ( sIsLSCN == ID_TRUE ) )
        {
            sStampingCount++;

            IDE_TEST( sdpSlotDirectory::getValue( sSlotDirPtr, 
                                                  i,
                                                  &sSlotOffset )
                      != IDE_SUCCESS );

            SDNB_SET_TBK_STAMPING( &sKeyEntry,
                                   sSlotOffset,
                                   sIsCSCN,
                                   sIsLSCN );

            idlOS::memcpy( sKeyList,
                           &sKeyEntry,
                           ID_SIZEOF( sdnbTBKStamping ) );

            sKeyList     += ID_SIZEOF( sdnbTBKStamping );
            sKeyListSize += ID_SIZEOF( sdnbTBKStamping );

            if ( sIsLSCN == ID_TRUE )
            {
                sDeadTBKCount++;

                sDeadKeySize += getKeyLength( &(aIndex->mColLenInfoList),
                        (UChar *)sLeafKey,
                        ID_TRUE );
                sDeadKeySize += ID_SIZEOF( sdpSlotEntry );
            }
            else
            {
                /* nothing to do */
            }
        }
        else
        {
            /* nothing to do */
        }
    }

    sKeyList = (UChar *)sTempBuf; /* sKeyList�� ���� ���� offset���� �̵��Ѵ�. */

    /* 2. TBK key ����Ʈ�� �α��Ѵ�. */
    if ( sKeyListSize > 0 )
    {

        IDE_TEST( sdrMiniTrans::writeLogRec( aMtx,
                                            (UChar *)aPage,
                                            NULL,
                                            ID_SIZEOF(UChar) + SDNB_COLLENINFO_LIST_SIZE( aIndex->mColLenInfoList )
                                                + ID_SIZEOF(UShort) +  sKeyListSize,
                                            SDR_SDN_KEY_STAMPING )
                  != IDE_SUCCESS );

        IDE_TEST( sdrMiniTrans::write( aMtx,
                                       (void *)&sCTSInKey,
                                       ID_SIZEOF(UChar) )
                  != IDE_SUCCESS );

        IDE_TEST( sdrMiniTrans::write( aMtx,
                                       (void *)&(aIndex->mColLenInfoList),
                                       SDNB_COLLENINFO_LIST_SIZE( aIndex->mColLenInfoList ) )
                  != IDE_SUCCESS );

        IDE_TEST( sdrMiniTrans::write( aMtx,
                                       &sKeyListSize,
                                       ID_SIZEOF(UShort) )
                  != IDE_SUCCESS );

        IDE_TEST( sdrMiniTrans::write( aMtx,
                                       (void *)sKeyList,
                                       sKeyListSize )
                  != IDE_SUCCESS );
    }
    else
    {
        /* nothing to do */
    }

    /* 3. TBK STAMPING �Ѵ�. */
    for ( i = 0; i < sKeyListSize; i += ID_SIZEOF( sdnbTBKStamping ) )
    {
        sKeyEntry = *(sdnbTBKStamping *)(sKeyList + i);

        sLeafKey = (sdnbLKey *)( ((UChar *)aPage) + sKeyEntry.mSlotOffset );

        if ( SDNB_IS_TBK_STAMPING_WITH_CSCN( &sKeyEntry ) == ID_TRUE )
        {
            SDNB_SET_CCTS_NO( sLeafKey, SDN_CTS_INFINITE );

            if ( SDNB_GET_STATE( sLeafKey ) == SDNB_KEY_UNSTABLE )
            {
                SDNB_SET_STATE( sLeafKey, SDNB_KEY_STABLE );
            }
            else
            {
                /* nothing to do */
            }
        }
        else
        {
            /* nothing to do */
        }

        if ( SDNB_IS_TBK_STAMPING_WITH_LSCN( &sKeyEntry ) == ID_TRUE )
        {
            SDNB_SET_STATE( sLeafKey, SDNB_KEY_DEAD );
            SDNB_SET_CCTS_NO( sLeafKey, SDN_CTS_INFINITE );
            SDNB_SET_LCTS_NO( sLeafKey, SDN_CTS_INFINITE );
        }
        else
        {
            /* nothing to do */
        }
    }

    sNodeHdr = (sdnbNodeHdr *)sdpPhyPage::getLogicalHdrStartPtr((UChar *)aPage);

    if ( sDeadKeySize > 0 )
    {
        sDeadKeySize += sNodeHdr->mTotalDeadKeySize;

        IDE_TEST( sdrMiniTrans::writeNBytes( aMtx,
                                             (UChar *)&sNodeHdr->mTotalDeadKeySize,
                                             (void *)&sDeadKeySize,
                                             ID_SIZEOF(UShort) )
                  != IDE_SUCCESS );
    }
    else
    {
        /* nothing to do */
    }

    if ( ( sNodeHdr->mTBKCount != sTBKCount ) || /* mTBKCount���� �߸��Ǿ��ִٸ� �����Ѵ�. (BUG-44973) */
         ( sDeadTBKCount > 0 ) ) /* Dead TBK�� ������ �׼���ŭ mTBKCount�� ���δ�. (BUG-44973) */
    {
        sTBKCount -= sDeadTBKCount;

        IDE_TEST( sdrMiniTrans::writeNBytes( aMtx,
                                             (UChar *)&sNodeHdr->mTBKCount,
                                             (void *)&sTBKCount,
                                             ID_SIZEOF(UShort) )
                  != IDE_SUCCESS );
    }
    else
    {
        /* nothing to do */
    }

    *aStampingCount = sStampingCount;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : sdnbBTree::checkTBKStamping                *
 * ------------------------------------------------------------------*
 * LeafKey�� TBK stamping �������� ���θ� üũ�Ѵ�. (BUG-44973)      *
 * (���� stamping�� �̷������ ������ ����)                          *
 *                                                                   *
 * stamping ���ɿ��δ� �Ʒ� �ΰ��� �Ű����� ������ Ȯ�ΰ����ϴ�.     *
 *  aIsCreateCSCN : TBK�� Create SCN�� stamping �����ϴ�.            * 
 *  aIsLimitCSCN  : TBK�� Limit SCN�� stamping �����ϴ�.             *
 *********************************************************************/
IDE_RC sdnbBTree::checkTBKStamping( idvSQL        * aStatistics,
                                    sdpPhyPageHdr * aPage,
                                    sdnbLKey      * aLeafKey,
                                    idBool        * aIsCreateCSCN,
                                    idBool        * aIsLimitCSCN )
{
    sdnbLKeyEx  * sLeafKeyEx    = NULL;
    smSCN         sCreateSCN;
    smSCN         sLimitSCN;
    idBool        sIsCreateCSCN = ID_FALSE;
    idBool        sIsLimitCSCN  = ID_FALSE;

    IDE_DASSERT( SDNB_GET_TB_TYPE( aLeafKey ) == SDNB_KEY_TB_KEY );

    sLeafKeyEx = (sdnbLKeyEx *)aLeafKey;

    if ( SDNB_GET_CCTS_NO( aLeafKey ) == SDN_CTS_IN_KEY )
    {
        IDE_TEST( getCommitSCNFromTBK( aStatistics,
                                       NULL,        /* aTrans */
                                       aPage,
                                       sLeafKeyEx,
                                       ID_FALSE,    /* aIsLimitSCN */
                                       SM_SCN_INIT, /* aStmtViewSCN */
                                       &sCreateSCN )
                  != IDE_SUCCESS );

        if ( isAgableTBK( sCreateSCN ) == ID_TRUE )
        {
            sIsCreateCSCN = ID_TRUE;
        }
        else
        {
            /* nothing to do */
        }
    }
    else
    {
        /* nothing to do */
    }

    if ( SDNB_GET_LCTS_NO( aLeafKey ) == SDN_CTS_IN_KEY )
    {
        IDE_TEST( getCommitSCNFromTBK( aStatistics,
                                       NULL,        /* aTrans */
                                       aPage,
                                       sLeafKeyEx,
                                       ID_TRUE,     /* aIsLimitSCN */
                                       SM_SCN_INIT, /* aStmtViewSCN */
                                       &sLimitSCN )
                  != IDE_SUCCESS );

        if ( isAgableTBK( sLimitSCN ) == ID_TRUE )
        {
            sIsLimitCSCN = ID_TRUE;
        }
        else
        {
            /* nothing to do */
        }
    }
    else
    {
        /* nothing to do */
    }
    
    *aIsCreateCSCN = sIsCreateCSCN;
    *aIsLimitCSCN  = sIsLimitCSCN;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}
