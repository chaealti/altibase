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
 * $Id: stndrModule.cpp 89495 2020-12-14 05:19:22Z emlee $
 **********************************************************************/

/*********************************************************************
 * FILE DESCRIPTION : stndrModule.cpp
 * ------------------------------------------------------------------*
 *
 *********************************************************************/

#include <idl.h>
#include <ide.h>
#include <mtd.h>

#include <smDef.h>
#include <smnDef.h>
#include <smnManager.h>

#include <sdp.h>
#include <sdc.h>
#include <sdnReq.h>

#include <smxTrans.h>
#include <sdrMiniTrans.h>
#include <sdnManager.h>
#include <sdnIndexCTL.h>
#include <sdbMPRMgr.h>

#include <stdUtils.h>

#include <stndrDef.h>
#include <stndrStackMgr.h>
#include <stndrModule.h>
#include <stndrTDBuild.h>
#include <stndrBUBuild.h>

extern smiGlobalCallBackList gSmiGlobalCallBackList;

extern mtdModule stdGeometry;

static UInt gMtxDLogType = SM_DLOG_ATTR_DEFAULT;


static sdnCallbackFuncs gCallbackFuncs4CTL =
{
    (sdnSoftKeyStamping)stndrRTree::softKeyStamping,
    (sdnHardKeyStamping)stndrRTree::hardKeyStamping
};


smnIndexModule stndrModule =
{
    SMN_MAKE_INDEX_MODULE_ID( SMI_TABLE_DISK,
                              SMI_ADDITIONAL_RTREE_INDEXTYPE_ID ),
    SMN_RANGE_DISABLE | SMN_DIMENSION_ENABLE | SMN_DEFAULT_DISABLE |
    SMN_BOTTOMUP_BUILD_ENABLE,
    ID_UINT_MAX,                // BUG-23113: RTree Key Size�� ������ ���� �ʴ´�.
    (smnMemoryFunc)             stndrRTree::prepareIteratorMem,
    (smnMemoryFunc)             stndrRTree::releaseIteratorMem,
    (smnMemoryFunc)             NULL, // prepareFreeNodeMem
    (smnMemoryFunc)             NULL, // releaseFreeNodeMem
    (smnCreateFunc)             stndrRTree::create,
    (smnDropFunc)               stndrRTree::drop,

    (smTableCursorLockRowFunc)  stndrRTree::lockRow,
    (smnDeleteFunc)             stndrRTree::deleteKey,
    (smnFreeFunc)               NULL,
    (smnInsertRollbackFunc)     stndrRTree::insertKeyRollback,
    (smnDeleteRollbackFunc)     stndrRTree::deleteKeyRollback,
    (smnAgingFunc)              stndrRTree::aging,

    (smInitFunc)                stndrRTree::init,
    (smDestFunc)                stndrRTree::dest,
    (smnFreeNodeListFunc)       stndrRTree::freeAllNodeList,
    (smnGetPositionFunc)        stndrRTree::getPositionNA,
    (smnSetPositionFunc)        stndrRTree::setPositionNA,

    (smnAllocIteratorFunc)      stndrRTree::allocIterator,
    (smnFreeIteratorFunc)       stndrRTree::freeIterator,
    (smnReInitFunc)             NULL,
    (smnInitMetaFunc)           stndrRTree::initMeta,

    (smnMakeDiskImageFunc)      NULL,
    (smnBuildIndexFunc)         stndrRTree::buildIndex,
    (smnGetSmoNoFunc)           stndrRTree::getSmoNo,
    (smnMakeKeyFromRow)         stndrRTree::makeKeyValueFromRow,
    (smnMakeKeyFromSmiValue)    stndrRTree::makeKeyValueFromSmiValueList,
    (smnRebuildIndexColumn)     stndrRTree::rebuildIndexColumn,
    (smnSetIndexConsistency)    stndrRTree::setConsistent,
    (smnGatherStat)             NULL
};


static const  smSeekFunc stndrSeekFunctions[32][12] =
{
    { /* SMI_TRAVERSE_FORWARD  SMI_PREVIOUS_DISABLE SMI_LOCK_READ            */
        (smSeekFunc)stndrRTree::fetchNext,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::beforeFirst,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::beforeFirst,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::beforeFirst
    },
    { /* SMI_TRAVERSE_FORWARD  SMI_PREVIOUS_DISABLE SMI_LOCK_WRITE           */
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::beforeFirstW,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::beforeFirst,
        (smSeekFunc)stndrRTree::fetchNext,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::beforeFirst
    },
    { /* SMI_TRAVERSE_FORWARD  SMI_PREVIOUS_DISABLE SMI_LOCK_REPEATABLE      */
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::beforeFirstRR,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::beforeFirst,
        (smSeekFunc)stndrRTree::fetchNext,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::beforeFirst,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::beforeFirst
    },
    { /* SMI_TRAVERSE_FORWARD  SMI_PREVIOUS_DISABLE SMI_LOCK_TABLE_SHARED    */
        (smSeekFunc)stndrRTree::fetchNext,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::beforeFirst,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::beforeFirst,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::beforeFirst
    },
    { /* SMI_TRAVERSE_FORWARD  SMI_PREVIOUS_DISABLE SMI_LOCK_TABLE_EXCLUSIVE */
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::beforeFirstW,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::beforeFirst,
        (smSeekFunc)stndrRTree::fetchNext,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::beforeFirst
    },
    { /* UNUSED                                                              */
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
    },
    { /* UNUSED                                                              */
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
    },
    { /* UNUSED                                                              */
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
    },
    { /* UNUSED                                                              */
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
    },
    { /* UNUSED                                                              */
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
    },
    { /* UNUSED                                                              */
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
    },
    { /* UNUSED                                                              */
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
    },
    { /* UNUSED                                                              */
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
    },
    { /* UNUSED                                                              */
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
    },
    { /* UNUSED                                                              */
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA
    },
    { /* UNUSED                                                              */
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA
    },
    { /* UNUSED                                                              */
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
    },
    { /* UNUSED                                                              */
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
    },
    { /* UNUSED                                                              */
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
    },
    { /* UNUSED                                                              */
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
    },
    { /* UNUSED                                                              */
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
    },
    { /* UNUSED                                                              */
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
    },
    { /* UNUSED                                                              */
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
    },
    { /* UNUSED                                                              */
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
    },
    { /* UNUSED                                                              */
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
    },
    { /* UNUSED                                                              */
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
    },
    { /* UNUSED                                                              */
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
    },
    { /* UNUSED                                                              */
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
    },
    { /* UNUSED                                                              */
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
    },
    { /* UNUSED                                                              */
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
    },
    { /* UNUSED                                                              */
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA
    },
    { /* UNUSED                                                              */
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
        (smSeekFunc)stndrRTree::NA,
    }
};

/*********************************************************************
 * FUNCTION DESCRIPTION :
 * ------------------------------------------------------------------*
 * MBR�� MinX ������ �����ϱ� ���� compare �Լ�
 *********************************************************************/
extern "C" SInt gCompareKeyArrayByAxisX( const void * aLhs,
                                         const void * aRhs )
{
    const stndrKeyArray * sLhs = (const stndrKeyArray*)aLhs;
    const stndrKeyArray * sRhs = (const stndrKeyArray*)aRhs;

    IDE_ASSERT( aLhs != NULL );
    IDE_ASSERT( aRhs != NULL );

    if( sLhs->mMBR.mMinX == sRhs->mMBR.mMinX )
    {
        return 0;
    }
    else
    {
        if( sLhs->mMBR.mMinX > sRhs->mMBR.mMinX )
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
 * FUNCTION DESCRIPTION :
 * ------------------------------------------------------------------*
 * MBR�� MinY ������ �����ϱ� ���� compare �Լ�
 *********************************************************************/
extern "C" SInt gCompareKeyArrayByAxisY( const void * aLhs,
                                         const void * aRhs )
{
    const stndrKeyArray * sLhs = (const stndrKeyArray*)aLhs;
    const stndrKeyArray * sRhs = (const stndrKeyArray*)aRhs;

    IDE_ASSERT( aLhs != NULL );
    IDE_ASSERT( aRhs != NULL );

    if( sLhs->mMBR.mMinY == sRhs->mMBR.mMinY )
    {
        return 0;
    }
    else
    {
        if( sLhs->mMBR.mMinY > sRhs->mMBR.mMinY )
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
 * FUNCTION DESCRIPTION :
 * ------------------------------------------------------------------*
 * quick sort�� ���� swap �Լ�
 *********************************************************************/
void stndrRTree::swap( stndrKeyArray * aArray,
                       SInt            i,
                       SInt            j )
{
    stndrKeyArray   sTmp;

    sTmp      = aArray[i];
    aArray[i] = aArray[j];
    aArray[j] = sTmp;
}

/*********************************************************************
 * FUNCTION DESCRIPTION :
 * ------------------------------------------------------------------*
 * KeyArray�� quick sort�� �����ϴ� �Լ��̴�. compare �Լ��� ���� MBR��
 * MinX �Ǵ� MinY ������ �����Ѵ�.
 *********************************************************************/
void stndrRTree::quickSort( stndrKeyArray * aArray,
                            SInt            aArraySize,
                            SInt         (* compare)( const void* aLhs,
                                                      const void* aRhs ) )
{
    stndrSortStackSlot  sStack[STNDR_MAX_SORT_STACK_SIZE];
    stndrSortStackSlot  sSlot;
    SInt                sDepth = -1;
    SInt                sLast;
    SInt                i;

    
    sDepth++;
    sStack[sDepth].mArray     = aArray;
    sStack[sDepth].mArraySize = aArraySize;

    while( sDepth >= 0 )
    {
        IDE_ASSERT( sDepth < (SInt)STNDR_MAX_SORT_STACK_SIZE );
        
        sSlot = sStack[sDepth];
        sDepth--;
        
        if( sSlot.mArraySize <= 1 )
            continue;

        swap( sSlot.mArray, 0, sSlot.mArraySize/2 );
        sLast = 0;

        for( i = 1; i < sSlot.mArraySize; i++ )
        {
            if( compare( (const void *)&sSlot.mArray[i],
                         (const void *)&sSlot.mArray[0] ) < 0 )
            {
                swap( sSlot.mArray, ++sLast, i );
            }
        }

        swap( sSlot.mArray, 0, sLast );

        sDepth++;
        sStack[sDepth].mArray = sSlot.mArray;
        sStack[sDepth].mArraySize = sLast;

        sDepth++;
        sStack[sDepth].mArray = sSlot.mArray + sLast + 1;
        sStack[sDepth].mArraySize = sSlot.mArraySize - sLast - 1;
    }
}

/*********************************************************************
 * FUNCTION DESCRIPTION :
 * ------------------------------------------------------------------*
 * �� �Լ��� Iterator�� �Ҵ��� �� memory pool�� �ʱ�ȭ
 *********************************************************************/
IDE_RC stndrRTree::prepareIteratorMem( smnIndexModule * /* aIndexModule */ )
{
    return IDE_SUCCESS;
}

/*********************************************************************
 * FUNCTION DESCRIPTION :
 * ------------------------------------------------------------------*
 * �� �Լ��� Iterator memory pool�� ����
 *********************************************************************/
IDE_RC stndrRTree::releaseIteratorMem( const smnIndexModule * )
{
    return IDE_SUCCESS;
}

/*********************************************************************
 * FUNCTION DESCRIPTION :
 * ------------------------------------------------------------------*
 * �ε����� ���� build�ϴٰ� ������ �߻��ϸ� ���ݱ��� ������ �ε���
 * ������ �����ؾ� �Ѵ�. �� �Լ��� �̷��� ȣ��ȴ�. ���׸�Ʈ����
 * �Ҵ�� ��� Page�� �����Ѵ�.
 *********************************************************************/
IDE_RC stndrRTree::freeAllNodeList( idvSQL          * aStatistics,
                                    smnIndexHeader  * aIndex,
                                    void            * aTrans )
{
    sdrMtx            sMtx;
    idBool            sMtxStart = ID_FALSE;
    stndrHeader     * sIndex;
    sdpSegmentDesc  * sSegmentDesc;
    

    sIndex = (stndrHeader*)((smnIndexHeader*)aIndex)->mHeader;

    // FOR A4 : index build�ÿ� ���� �߻��ϸ� ȣ���
    // �ε����� ������ ��� ������ ������

    // Index Header���� Segment Descriptor�� ������ ��� page�� �����Ѵ�.
    IDE_TEST( sdrMiniTrans::begin( aStatistics,
                                   &sMtx,
                                   aTrans,
                                   SDR_MTX_LOGGING,
                                   ID_FALSE,/*MtxUndoable(PROJ-2162)*/
                                   gMtxDLogType )
              != IDE_SUCCESS );
    
    sMtxStart = ID_TRUE;
    
    sSegmentDesc = &(sIndex->mSdnHeader.mSegmentDesc);
    
    IDE_TEST( sdpSegDescMgr::getSegMgmtOp(
                  &(sIndex->mSdnHeader.mSegmentDesc))->mResetSegment(
                      aStatistics,
                      &sMtx,
                      SC_MAKE_SPACE(aIndex->mIndexSegDesc),
                      &(sSegmentDesc->mSegHandle),
                      SDP_SEG_TYPE_INDEX )
              != IDE_SUCCESS );

    sMtxStart = ID_FALSE;
    
    IDE_TEST( sdrMiniTrans::commit( &sMtx ) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if( sMtxStart == ID_TRUE )
    {
        (void)sdrMiniTrans::rollback( &sMtx );
    }

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION :
 * ------------------------------------------------------------------*
 * Ű�� �����ϱ� ���ؼ��� Row�� Fetch�ؾ� �Ѵ�. �̶� ��� Į����
 * �ƴ�, Index�� �ɸ� Į���� Fetch�ؾ� �ϱ� ������ ���� FetchColumn-
 * List4Key�� �����Ѵ�.
 *********************************************************************/
IDE_RC stndrRTree::makeFetchColumnList4Index( void          * aTableHeader,
                                              stndrHeader   * aIndexHeader )
{
    const smiColumn     * sTableColumn;
    smiColumn           * sSmiColumnInFetchColumn;
    smiFetchColumnList  * sFetchColumnList;
    stndrColumn         * sIndexColumn;
    UShort                sColumnSeq;

    
    IDE_DASSERT( aTableHeader != NULL );
    IDE_DASSERT( aIndexHeader != NULL );

    sIndexColumn = &(aIndexHeader->mColumn);
    sColumnSeq   = sIndexColumn->mKeyColumn.id % SMI_COLUMN_ID_MAXIMUM;
    sTableColumn = smcTable::getColumn(aTableHeader, sColumnSeq);

    sFetchColumnList        = &(aIndexHeader->mFetchColumnListToMakeKey);
    sSmiColumnInFetchColumn = (smiColumn *)sFetchColumnList->column;

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
    sFetchColumnList->copyDiskColumn =
        (void *)sIndexColumn->mCopyDiskColumnFunc;

    sFetchColumnList->next = NULL;
    
    return IDE_SUCCESS;
}

/*********************************************************************
 * FUNCTION DESCRIPTION :
 * ------------------------------------------------------------------*
 * create�Լ��� ���� �ε��� Į�� ������ �����ϱ� ���� ȣ��ȴ�. ����
 * �߰������� �ǽð� Alter DDL�� ���� Į�������� ������ �ʿ䰡 ��������
 * ȣ��ǰ� �ȴ�.
 *********************************************************************/
IDE_RC stndrRTree::rebuildIndexColumn( smnIndexHeader   * aIndex,
                                       smcTableHeader   * aTable,
                                       void             * aHeader )
{
    const smiColumn * sTableColumn = NULL;
    stndrColumn     * sIndexColumn = NULL;
    stndrHeader     * sHeader = NULL;
    UInt              sColID;
    SInt              i;
    UInt              sNonStoringSize = 0;

    sHeader = (stndrHeader *)aHeader;

    // R-Tree�� �÷� ������ 1���̴�.
    IDE_ASSERT( aIndex->mColumnCount == 1 );

    for( i = 0; i < 1/* aIndex->mColumnCount */; i++)
    {
        sIndexColumn = &sHeader->mColumn;

        // �÷� ����(KeyColumn, mt callback functions,...) �ʱ�ȭ
        sColID = aIndex->mColumns[i] & SMI_COLUMN_ID_MASK;
        IDE_TEST_RAISE( sColID >= aTable->mColumnCount, ERR_COLUMN_NOT_FOUND );

        sTableColumn = smcTable::getColumn( aTable,sColID );

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

        sIndexColumn->mKeyColumn.flag = aIndex->mColumnFlags[i];
        sIndexColumn->mKeyColumn.flag &= ~SMI_COLUMN_USAGE_MASK;
        sIndexColumn->mKeyColumn.flag |= SMI_COLUMN_USAGE_INDEX;

        // PROJ-1872 Disk Index ���屸�� ����ȭ
        IDE_TEST( gSmiGlobalCallBackList.findCopyDiskColumnValue( 
                      sTableColumn,
                      &sIndexColumn->mCopyDiskColumnFunc )
                  != IDE_SUCCESS );
        
        IDE_TEST( gSmiGlobalCallBackList.findKey2String(
                      sTableColumn,
                      aIndex->mColumnFlags[i],
                      &sIndexColumn->mKey2String )
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
    
    // BUG-22946 
    IDE_TEST( makeFetchColumnList4Index(aTable, sHeader) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_COLUMN_NOT_FOUND );

    IDE_SET( ideSetErrorCode(smERR_FATAL_smnColumnNotFound) );

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION :
 * ------------------------------------------------------------------*
 * To Fix BUG-21925                     
 * Meta Page�� mIsConsistent�� �����Ѵ�.
 *
 * ���� : Transaction�� ������� �ʱ� ������ online ���¿�����
 *       ���Ǿ�� �ȵȴ�.
 *********************************************************************/
IDE_RC stndrRTree::setConsistent( smnIndexHeader * aIndex,
                                  idBool           aIsConsistent )
{
    idvSQL      * sStatistics;
    idvSession    sDummySession;
    idvSQL        sDummySQL;
    sdrMtx        sMtx;
    stndrMeta   * sMeta;
    idBool        sTrySuccess;
    UInt          sState = 0;
    stndrHeader * sHeader;
    scSpaceID     sSpaceID;
    sdRID         sMetaRID;

    sHeader  = (stndrHeader*)aIndex->mHeader;
    sSpaceID = sHeader->mSdnHeader.mIndexTSID;
    sMetaRID = sHeader->mSdnHeader.mMetaRID;

    
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
                                   (void*)NULL,
                                   SDR_MTX_LOGGING,
                                   ID_FALSE,/*MtxUndoable(PROJ-2162)*/
                                   SM_DLOG_ATTR_DEFAULT | SM_DLOG_ATTR_DUMMY )
              != IDE_SUCCESS );
    sState = 1;

    IDE_TEST( sdbBufferMgr::getPageByRID( sStatistics,
                                          sSpaceID,
                                          sMetaRID,
                                          SDB_X_LATCH,
                                          SDB_WAIT_NORMAL,
                                          &sMtx,
                                          (UChar**)&sMeta,
                                          &sTrySuccess )
              != IDE_SUCCESS );

    
    IDE_TEST( sdrMiniTrans::writeNBytes( &sMtx,
                                         (UChar*)&sMeta->mIsConsistent,
                                         (void*)&aIsConsistent,
                                         ID_SIZEOF(aIsConsistent) )
              != IDE_SUCCESS );

    sState = 0;
    IDE_TEST( sdrMiniTrans::commit( &sMtx ) != IDE_SUCCESS );
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if( sState == 1 )
    {
        (void)sdrMiniTrans::rollback( &sMtx );
    }

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION :
 * ------------------------------------------------------------------*
 * �� �Լ��� �ý��� ��ŸƮ���� Ȥ�� �ε����� ���� create�� �� run-time
 * index header�� �����Ͽ� �ٴ� ������ �Ѵ�. �޸� �ε����� �����ϰ�
 * �ϱ� ���� smmManager�� ���� temp Page�� �Ҵ�޾� ����Ѵ�. ���߿�
 * memmgr���� �޵��� ���� ���.(��� �ε����� ���ؼ�)
 *********************************************************************/
IDE_RC stndrRTree::create( idvSQL               * aStatistics,
                           smcTableHeader       * aTable,
                           smnIndexHeader       * aIndex,
                           smiSegAttr           * aSegAttr,
                           smiSegStorageAttr    * aSegStorageAttr,
                           smnInsertFunc        * aInsert,
                           smnIndexHeader      ** /*aRebuildIndexHeader*/,
                           ULong                  aSmoNo )
{
    UChar         * sPage;
    UChar         * sMetaPagePtr;
    stndrMeta     * sMeta;
    stndrHeader   * sHeader = NULL;
    idBool          sTrySuccess;
    sdpSegState     sIndexSegState;
    smLSN           sRecRedoLSN;
    sdpSegMgmtOp  * sSegMgmtOp;
    scPageID        sSegPID;
    scPageID        sMetaPID;
    UInt            sState = 0;
    SChar           sBuffer[IDU_MUTEX_NAME_LEN];
    stndrNodeHdr  * sNodeHdr = NULL;


    // Disk R-Tree�� �÷� ���� 1���̴�.
    IDE_ASSERT( aIndex->mColumnCount == 1 );

    // ��ũ R-Tree�� Run Time Header �� ��� ���� �Ҵ�
    IDE_TEST( iduMemMgr::malloc(IDU_MEM_ST_STN,
                                ID_SIZEOF(stndrHeader),
                                (void**)&sHeader)
              != IDE_SUCCESS );

    /* BUG-40964 runtime index header ���� ��ġ�� �޸� �Ҵ� ���ķ� ���� */  
    aIndex->mHeader = (smnRuntimeHeader*) sHeader;
    
    sState = 1;

    IDE_TEST( iduMemMgr::malloc(
                  IDU_MEM_ST_STN,
                  ID_SIZEOF(smiColumn),
                  (void**) &(sHeader->mFetchColumnListToMakeKey.column) )
              != IDE_SUCCESS);
    sState = 2;

    // Run Time Header ��� ���� �ʱ�ȭ
    idlOS::snprintf( sBuffer, 
                     ID_SIZEOF(sBuffer), 
                     "INDEX_HEADER_LATCH_%"ID_UINT32_FMT, 
                     aIndex->mId );

    IDE_TEST( sHeader->mSdnHeader.mLatch.initialize(sBuffer) != IDE_SUCCESS );

    /* statistics */
    IDE_TEST( smnManager::initIndexStatistics( aIndex,
                                               (smnRuntimeHeader*)sHeader,
                                               aIndex->mId )
              != IDE_SUCCESS );

    idlOS::snprintf( sBuffer,
                     ID_SIZEOF(sBuffer),
                     "%s""%"ID_UINT32_FMT,
                     "INDEX_SMONO_MUTEX_",
                     aIndex->mId );

    IDE_TEST( sHeader->mSmoNoMutex.initialize(
                  sBuffer,
                  IDU_MUTEX_KIND_POSIX,
                  IDV_WAIT_INDEX_NULL) != IDE_SUCCESS );

    sHeader->mSdnHeader.mIsCreated = ID_FALSE;
    sHeader->mSdnHeader.mLogging   = ID_TRUE;

    sHeader->mSdnHeader.mTableTSID = ((smcTableHeader*)aTable)->mSpaceID;
    sHeader->mSdnHeader.mIndexTSID = SC_MAKE_SPACE(aIndex->mIndexSegDesc);
    sHeader->mSdnHeader.mSmoNo     = aSmoNo;

    sHeader->mSdnHeader.mTableOID  = aIndex->mTableOID;
    sHeader->mSdnHeader.mIndexID   = aIndex->mId;

    sHeader->mSmoNoAtomicA = 0;
    sHeader->mSmoNoAtomicB = 0;

    sHeader->mVirtualRootNodeAtomicA = 0;
    sHeader->mVirtualRootNodeAtomicB = 0;

    sHeader->mKeyCount = ((smcTableHeader *)aTable)->mFixed.mDRDB.mRecCnt;

    idlOS::memset( &(sHeader->mDMLStat), 0x00, ID_SIZEOF(stndrStatistic) );
    idlOS::memset( &(sHeader->mQueryStat), 0x00, ID_SIZEOF(stndrStatistic) );


    // Segment ���� (PROJ-1671)
    // insert high limit�� insert low limit�� ������� ������ �����Ѵ�.
    sdpSegDescMgr::setDefaultSegAttr(
        &(sHeader->mSdnHeader.mSegmentDesc.mSegHandle.mSegAttr),
        SDP_SEG_TYPE_INDEX );

    sdpSegDescMgr::setSegAttr(
        &sHeader->mSdnHeader.mSegmentDesc, aSegAttr );

    IDE_DASSERT( aSegAttr->mInitTrans <= aSegAttr->mMaxTrans );
    IDE_DASSERT( aSegAttr->mInitTrans <= SMI_MAXIMUM_INDEX_CTL_SIZE );
    IDE_DASSERT( aSegAttr->mMaxTrans <= SMI_MAXIMUM_INDEX_CTL_SIZE );

    // Storage �Ӽ��� �����Ѵ�.
    sdpSegDescMgr::setSegStoAttr( &sHeader->mSdnHeader.mSegmentDesc,
                                  aSegStorageAttr );

    /* BUG-37955 index segment�� table OID�� Index ID�� ����ϵ��� ���� */
    IDE_TEST( sdpSegDescMgr::initSegDesc(
                  &sHeader->mSdnHeader.mSegmentDesc,
                  SC_MAKE_SPACE(aIndex->mIndexSegDesc),
                  SC_MAKE_PID(aIndex->mIndexSegDesc),
                  SDP_SEG_TYPE_INDEX,
                  aIndex->mTableOID,
                  aIndex->mId ) != IDE_SUCCESS );

    if( sHeader->mSdnHeader.mSegmentDesc.mSegMgmtType !=
        sdpTableSpace::getSegMgmtType(SC_MAKE_SPACE(aIndex->mIndexSegDesc)) )
    {
        ideLog::log( IDE_SERVER_0, "\
===================================================\n\
               IDX Name : %s                       \n\
               ID       : %u                       \n\
                                                   \n\
sHeader->mSegmentDesc.mSegMgmtType : %u\n\
sdpTableSpace::getSegMgmtType(SC_MAKE_SPACE(aIndex->mIndexSegDesc)) : %u\n",
                     aIndex->mName,
                     aIndex->mId,
                     sHeader->mSdnHeader.mSegmentDesc.mSegMgmtType,
                     sdpTableSpace::getSegMgmtType(SC_MAKE_SPACE(aIndex->mIndexSegDesc)) );

        ideLog::log( IDE_SERVER_0, "\
-<stndrHeader>--------------------------------------\n" );
        ideLog::logMem( IDE_SERVER_0, (UChar *)sHeader, ID_SIZEOF(stndrHeader) );
        
        ideLog::log( IDE_SERVER_0, "\
-<smnIndexHeader>----------------------------------\n" );
        ideLog::logMem( IDE_SERVER_0, (UChar *)aIndex, ID_SIZEOF(smnIndexHeader) );
        IDE_ASSERT( 0 );
    }

    sSegMgmtOp = sdpSegDescMgr::getSegMgmtOp( &sHeader->mSdnHeader.mSegmentDesc );

    IDE_TEST( sSegMgmtOp->mGetSegState(
                  aStatistics,
                  SC_MAKE_SPACE(aIndex->mIndexSegDesc),
                  sHeader->mSdnHeader.mSegmentDesc.mSegHandle.mSegPID,
                  &sIndexSegState)
              != IDE_SUCCESS );

    if( sIndexSegState == SDP_SEG_FREE )
    {
        // for PBT
        // restart disk index rebuild�� ������ �־ ������ ����.
        ideLog::log( SM_TRC_LOG_LEVEL_DINDEX,
                     SM_TRC_DINDEX_INDEX_SEG_FREE,
                     aTable->mFixed.mDRDB.mSegDesc.mSegHandle.mSegPID,
                     aIndex->mIndexSegDesc );
        
        return IDE_SUCCESS;
    }
    
    // set meta page to runtime header
    sSegPID  = sdpSegDescMgr::getSegPID(
        &(sHeader->mSdnHeader.mSegmentDesc) );

    IDE_TEST( sSegMgmtOp->mGetMetaPID(aStatistics,
                                      sHeader->mSdnHeader.mIndexTSID,
                                      sSegPID,
                                      0, /* Seg Meta PID Array Index */
                                      &sMetaPID)
              != IDE_SUCCESS );

    sHeader->mSdnHeader.mMetaRID =
        SD_MAKE_RID( sMetaPID, SMN_INDEX_META_OFFSET );

    IDE_TEST( sdbBufferMgr::fixPageByRID(
                  aStatistics,
                  sHeader->mSdnHeader.mIndexTSID,
                  sHeader->mSdnHeader.mMetaRID,
                  (UChar**)&sMeta,
                  &sTrySuccess)
              != IDE_SUCCESS );

    sMetaPagePtr = sdpPhyPage::getPageStartPtr( sMeta );
    
    sHeader->mRootNode              = sMeta->mRootNode;
    sHeader->mEmptyNodeHead         = sMeta->mEmptyNodeHead;
    sHeader->mEmptyNodeTail         = sMeta->mEmptyNodeTail;
    sHeader->mFreeNodeCnt           = sMeta->mFreeNodeCnt;
    sHeader->mFreeNodeHead          = sMeta->mFreeNodeHead;
    sHeader->mFreeNodeSCN           = sMeta->mFreeNodeSCN;
    sHeader->mConvexhullPointNum    = sMeta->mConvexhullPointNum;

    /* RTree�� NumDist�� �ǹ� ���� */
    sHeader->mSdnHeader.mIsConsistent  = sMeta->mIsConsistent;

    sHeader->mMaxKeyCount = smuProperty::getRTreeMaxKeyCount();
    
    // �α� �ּ�ȭ (PROJ-1469)
    // mIsConsistent = ID_FALSE : index access �Ұ�
    sHeader->mSdnHeader.mIsCreatedWithLogging
        = sMeta->mIsCreatedWithLogging;
    
    sHeader->mSdnHeader.mIsCreatedWithForce
        = sMeta->mIsCreatedWithForce;
    
    sHeader->mSdnHeader.mCompletionLSN
        = sMeta->mNologgingCompletionLSN;

    IDE_TEST( sdbBufferMgr::unfixPage(aStatistics, sMetaPagePtr)
              != IDE_SUCCESS );

    // mIsConsistent = ID_TRUE �̰� NOLOGGING/NOFORCE�� �����Ǿ��� ���
    // index build�� index page���� disk�� force�Ǿ����� check
    if( (sHeader->mSdnHeader.mIsConsistent         == ID_TRUE ) &&
        (sHeader->mSdnHeader.mIsCreatedWithLogging == ID_FALSE) &&
        (sHeader->mSdnHeader.mIsCreatedWithForce   == ID_FALSE) )
    {
        // index build�� index page���� disk�� force���� �ʾ�����
        // sHeader->mCompletionLSN�� sRecRedoLSN�� ���ؼ�
        // sHeader->mCompletionLSN�� sRecRedoLSN���� ũ��
        // sHeader->mIsConsistent = FALSE
        (void)smrRecoveryMgr::getDiskRedoLSNFromLogAnchor( &sRecRedoLSN );

        if( smrCompareLSN::isGTE( &(sHeader->mSdnHeader.mCompletionLSN),
                                  &sRecRedoLSN ) == ID_TRUE )
        {
            sHeader->mSdnHeader.mIsConsistent = ID_FALSE;

            // To fix BUG-21925
            IDE_TEST( setConsistent( aIndex,
                                     ID_FALSE) /* isConsistent */
                      != IDE_SUCCESS );
        }
    }

    // Tree MBR ����
    if( sMeta->mRootNode != SD_NULL_PID )
    {
        IDE_TEST( sdbBufferMgr::fixPageByPID(
                      aStatistics,
                      sHeader->mSdnHeader.mIndexTSID,
                      sMeta->mRootNode,
                      (UChar**)&sPage,
                      &sTrySuccess)
                  != IDE_SUCCESS );
        
        sNodeHdr = (stndrNodeHdr *)sdpPhyPage::getLogicalHdrStartPtr( sPage );
        sHeader->mTreeMBR = sNodeHdr->mMBR;
        sHeader->mInitTreeMBR = ID_TRUE;
        
        IDE_TEST( sdbBufferMgr::unfixPage(aStatistics, sPage)
                  != IDE_SUCCESS );
    }
    else
    {
        STNDR_INIT_MBR( sHeader->mTreeMBR );
        sHeader->mInitTreeMBR = ID_FALSE;
    }
    
    // column ����
    IDE_TEST( rebuildIndexColumn( aIndex, aTable, sHeader ) != IDE_SUCCESS );

    // Insert, Delete �Լ� ����
    *aInsert = stndrRTree::insertKey;

    // Virtual Root Node ����
    setVirtualRootNode( sHeader,
                        sHeader->mRootNode,
                        sHeader->mSdnHeader.mSmoNo );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    switch( sState )
    {
        case 2:
            (void)iduMemMgr::free( (void*)sHeader->mFetchColumnListToMakeKey.column );
            // no break
        case 1:
            (void)iduMemMgr::free( sHeader );
            break;
        default:
            break;
    }

    aIndex->mHeader = NULL;

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION :
 * ------------------------------------------------------------------*
 * �ε����� �����Ѵ�.
 *********************************************************************/
IDE_RC stndrRTree::buildIndex( idvSQL           * aStatistics,
                               void             * aTrans,
                               smcTableHeader   * aTable,
                               smnIndexHeader   * aIndex,
                               smnGetPageFunc     /*aGetPageFunc */,
                               smnGetRowFunc      /*aGetRowFunc  */,
                               SChar            * /*aNullRow     */,
                               idBool             aIsNeedValidation,
                               idBool             /* aIsEnableTable */,
                               UInt               aParallelDegree,
                               UInt               aBuildFlag,
                               UInt               aTotalRecCnt )

{
    stndrHeader * sHeader;

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
                                      aParallelDegree)
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
                                     aParallelDegree)
                      != IDE_SUCCESS );
        }
    }
    else
    {
        sHeader = (stndrHeader*)aIndex->mHeader;
        sHeader->mSdnHeader.mIsCreated = ID_TRUE;

        IDE_TEST( stndrBUBuild::updateStatistics(
                      aStatistics,
                      NULL,
                      aIndex,
                      0,
                      0 )
                  != IDE_SUCCESS );

        IDE_TEST( buildMeta(aStatistics,
                            aTrans,
                            sHeader)
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION :
 * ------------------------------------------------------------------*
 * Top-Down ����� �ε��� ���带 �����Ѵ�.
 *********************************************************************/
IDE_RC stndrRTree::buildDRTopDown(idvSQL            * aStatistics,
                                  void              * aTrans,
                                  smcTableHeader    * aTable,
                                  smnIndexHeader    * aIndex,
                                  idBool              aIsNeedValidation,
                                  UInt                aBuildFlag,
                                  UInt                aParallelDegree )
{
    SInt              i;
    SInt              sThreadCnt     = 0;
    idBool            sCreateSuccess = ID_TRUE;
    ULong             sAllocPageCnt;
    stndrTDBuild    * sThreads       = NULL;
    stndrHeader     * sHeader;
    SInt              sInitThreadCnt = 0;
    UInt              sState         = 0;

    
    // disk temp table�� cluster index�̱⶧����
    // build index�� ���� �ʴ´�.
    // �� create cluster index�� , key�� insert�ϴ� ������.
    IDE_DASSERT( (aTable->mFlag & SMI_TABLE_TYPE_MASK) == SMI_TABLE_DISK );
    IDE_DASSERT( aIndex->mType == SMI_ADDITIONAL_RTREE_INDEXTYPE_ID );

    sHeader = (stndrHeader*)((smnIndexHeader*)aIndex)->mHeader;

    // create index �ÿ��� meta page�� ���� �ʱ����� ID_FALSE�� �ؾ��Ѵ�.
    // No-logging �ÿ��� index runtime header�� �����Ѵ�.
    sHeader->mSdnHeader.mIsCreated = ID_FALSE;

    IDE_TEST( buildMeta( aStatistics,
                         aTrans,
                         sHeader ) != IDE_SUCCESS );

    if( aParallelDegree == 0 )
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

    if( sAllocPageCnt  < (UInt)sThreadCnt )
    {
        sThreadCnt = 1;
    }

    IDE_TEST( iduMemMgr::malloc( IDU_MEM_ST_STN,
                                 ID_SIZEOF(stndrTDBuild)*sThreadCnt,
                                 (void**)&sThreads )
              != IDE_SUCCESS );
    sState = 1;

    for( i = 0; i < sThreadCnt; i++)
    {
        new (sThreads + i) stndrTDBuild;
        IDE_TEST(sThreads[i].initialize(
                              sThreadCnt,
                              i,
                              aTable,
                              aIndex,
                              smLayerCallback::getFstDskViewSCN( aTrans ),
                              aIsNeedValidation,
                              aBuildFlag,
                              aStatistics,
                              &sCreateSuccess ) != IDE_SUCCESS);
        sInitThreadCnt++;
    }

    for( i = 0; i < sThreadCnt; i++)
    {
        IDE_TEST(sThreads[i].start( ) != IDE_SUCCESS);
        IDE_TEST(sThreads[i].waitToStart(0) != IDE_SUCCESS);
    }

    for( i = 0; i < sThreadCnt; i++)
    {
        IDE_TEST(sThreads[i].join() != IDE_SUCCESS);
    }

    if( sCreateSuccess != ID_TRUE )
    {
        IDE_TEST(iduCheckSessionEvent(aStatistics) != IDE_SUCCESS);
        for( i = 0; i < sThreadCnt; i++)
        {
            if( sThreads[i].getErrorCode() != 0 )
            {
                ideCopyErrorInfo( ideGetErrorMgr(),
                                  sThreads[i].getErrorMgr() );
                break;
            }
        }
        IDE_TEST( 1 );
    }

    for( i = ( sThreadCnt - 1 ) ; i >= 0 ; i-- )
    {
        sInitThreadCnt--;
        IDE_TEST( sThreads[i].destroy( ) != IDE_SUCCESS );
    }

    sState = 0;
    IDE_TEST( iduMemMgr::free(sThreads) != IDE_SUCCESS );

    sHeader->mSdnHeader.mIsCreated = ID_TRUE;
    sHeader->mSdnHeader.mIsConsistent = ID_TRUE;
    smrLogMgr::getLstLSN( &(sHeader->mSdnHeader.mCompletionLSN) );

    IDE_TEST( buildMeta( aStatistics,
                         aTrans,
                         sHeader )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    IDE_PUSH();

    // BUG-27612 CodeSonar::Use After Free (4)
    if( sState == 1 )
    {
        for( i = 0 ; i < sInitThreadCnt; i++ )
        {
            (void)sThreads[i].destroy();
        }
        (void)iduMemMgr::free(sThreads);
    }
    IDE_POP();

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION :
 * ------------------------------------------------------------------*
 * Bottom-UP ����� �ε��� ���带 �����Ѵ�.
 *********************************************************************/
IDE_RC stndrRTree::buildDRBottomUp( idvSQL          * aStatistics,
                                    void            * aTrans,
                                    smcTableHeader  * aTable,
                                    smnIndexHeader  * aIndex,
                                    idBool            aIsNeedValidation,
                                    UInt              aBuildFlag,
                                    UInt              aParallelDegree )
{
    SInt          sThreadCnt;
    ULong         sTotalExtentCnt;
    ULong         sTotalSortAreaSize;
    UInt          sTotalMergePageCnt;
    sdpSegInfo    sSegInfo;
    stndrHeader * sHeader;

    // disk temp table�� cluster index�̱⶧����
    // build index�� ���� �ʴ´�.
    // �� create cluster index�� , key�� insert�ϴ� ������.
    IDE_DASSERT ((aTable->mFlag & SMI_TABLE_TYPE_MASK) == SMI_TABLE_DISK);
    IDE_DASSERT( aIndex->mType == SMI_ADDITIONAL_RTREE_INDEXTYPE_ID );

    sHeader = (stndrHeader*)((smnIndexHeader*)aIndex)->mHeader;

    // create index �ÿ��� meta page�� ���� �ʱ����� ID_FALSE�� �ؾ��Ѵ�.
    // No-logging �ÿ��� index runtime header�� �����Ѵ�.
    sHeader->mSdnHeader.mIsCreated = ID_FALSE;

    if( aParallelDegree == 0 )
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

    if( sTotalExtentCnt < (UInt)sThreadCnt )
    {
        sThreadCnt = 1;
    }

    sTotalSortAreaSize = smuProperty::getSortAreaSize();

    // ������� SORT_AREA_SIZE�� 4���������� Ŀ�� �Ѵ�.
    while( (sTotalSortAreaSize / sThreadCnt) < SD_PAGE_SIZE*4 )
    {
        sThreadCnt = sThreadCnt / 2;
        if( sThreadCnt == 0 )
        {
            sThreadCnt = 1;
            break;
        }
    }

    sTotalMergePageCnt = smuProperty::getMergePageCount();

    // ������� MERGE_PAGE_COUNT�� 2���������� Ŀ�� �Ѵ�.
    while( (sTotalMergePageCnt / sThreadCnt) < 2 )
    {
        sThreadCnt = sThreadCnt / 2;
        if( sThreadCnt == 0 )
        {
            sThreadCnt = 1;
            break;
        }
    }

    // Parallel Index Build
    IDE_TEST( stndrBUBuild::main( aStatistics,
                                  aTrans,
                                  aTable,
                                  aIndex,
                                  sThreadCnt,
                                  sTotalSortAreaSize,
                                  sTotalMergePageCnt,
                                  aIsNeedValidation,
                                  aBuildFlag )
              != IDE_SUCCESS );

    setVirtualRootNode( sHeader,
                        sHeader->mRootNode,
                        sHeader->mSdnHeader.mSmoNo );

    sHeader->mSdnHeader.mIsCreated = ID_TRUE;

    IDE_TEST( buildMeta( aStatistics,
                         aTrans,
                         sHeader )
              != IDE_SUCCESS );

    // nologging & force�� ��� modify�� ���������� ������ flush�Ѵ�.
    if( (aBuildFlag & SMI_INDEX_BUILD_FORCE_MASK) == SMI_INDEX_BUILD_FORCE )
    {
        IDE_DASSERT( (aBuildFlag & SMI_INDEX_BUILD_LOGGING_MASK) ==
                     SMI_INDEX_BUILD_NOLOGGING );

        ideLog::log( IDE_SM_0, "\
========================================\n\
 [IDX_CRE] Buffer Flush                 \n\
========================================\n" );

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
 * FUNCTION DESCRIPTION : stndrRTree::drop
 * ------------------------------------------------------------------*
 * �� �Լ��� index�� drop�ϰų� system�� shutdown�� �� ȣ��ȴ�.
 * drop�ÿ��� run-time header�� �����Ѵ�.
 * �� �Լ��� commit �α׸� ���� ��, Ȥ�� shutdown�ÿ��� ������,
 * index segment�� TSS�� �̹� RID�� �޸� �����̴�.
 *********************************************************************/
IDE_RC stndrRTree::drop( smnIndexHeader * aIndex )
{
    stndrHeader * sHeader;

    
    sHeader = (stndrHeader*)(aIndex->mHeader);

    if( sHeader != NULL )
    {
        sHeader->mSdnHeader.mSegmentDesc.mSegHandle.mSegPID = SD_NULL_PID;

        IDE_TEST(
            sdpSegDescMgr::getSegMgmtOp(
                &(sHeader->mSdnHeader.mSegmentDesc))->mDestroy(
                &(sHeader->mSdnHeader.mSegmentDesc.mSegHandle) )
            != IDE_SUCCESS );

        sHeader->mSdnHeader.mSegmentDesc.mSegHandle.mCache = NULL;

        IDE_TEST( sHeader->mSdnHeader.mLatch.destroy()
                  != IDE_SUCCESS );

        IDE_TEST( smnManager::destIndexStatistics( aIndex,
                                                   (smnRuntimeHeader*)sHeader )
                  != IDE_SUCCESS );
        
        IDE_TEST( sHeader->mSmoNoMutex.destroy()
                  != IDE_SUCCESS );

        IDE_TEST( iduMemMgr::free( 
                      (void*)sHeader->mFetchColumnListToMakeKey.column)
                  != IDE_SUCCESS );

        IDE_TEST( iduMemMgr::free(sHeader) != IDE_SUCCESS );
        
        aIndex->mHeader = NULL;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : stndrRTree::init
 * ------------------------------------------------------------------*
 * �� �Լ��� �ε����� traverse�ϱ� ���� iterator�� �ʱ�ȭ�Ѵ�.
 *********************************************************************/
IDE_RC stndrRTree::init( stndrIterator          * aIterator,
                         void                   * aTrans,
                         smcTableHeader         * aTable,
                         smnIndexHeader         * aIndex,
                         void                   * /*aDumpObject*/,
                         const smiRange         * aKeyRange,
                         const smiRange         * /*aKeyFilter*/,
                         const smiCallBack      * aRowFilter,
                         UInt                     aFlag,
                         smSCN                    aSCN,
                         smSCN                    aInfinite,
                         idBool                   aUntouchable,
                         smiCursorProperties    * aProperties,
                         const smSeekFunc      ** aSeekFunc,
                         smiStatement           * aStatement )
{
    idvSQL  * sSQLStat = NULL;
    idBool    sStackInit = ID_FALSE;

    
    aIterator->mSCN             = aSCN;
    aIterator->mInfinite        = aInfinite;
    aIterator->mTrans           = aTrans;
    aIterator->mTable           = aTable;
    aIterator->mCurRecPtr       = NULL;
    aIterator->mLstFetchRecPtr  = NULL;
    SC_MAKE_NULL_GRID( aIterator->mRowGRID );
    aIterator->mTID             = smLayerCallback::getTransID( aTrans );
    aIterator->mFlag            = aUntouchable == ID_TRUE ? SMI_ITERATOR_READ : SMI_ITERATOR_WRITE;
    aIterator->mProperties      = aProperties;
    aIterator->mStatement       = aStatement;

    aIterator->mIndex               = aIndex;
    aIterator->mKeyRange            = aKeyRange;
    aIterator->mRowFilter           = aRowFilter;
    aIterator->mIsLastNodeInRange   = ID_TRUE;
    aIterator->mCurRowPtr           = &aIterator->mRowCache[0];
    aIterator->mCacheFence          = &aIterator->mRowCache[0];
    aIterator->mPage                = (UChar*)aIterator->mAlignedPage;

    IDE_TEST( stndrStackMgr::initialize( &aIterator->mStack ) );
    sStackInit = ID_TRUE;

    idlOS::memset( aIterator->mPage, 0x00, SD_PAGE_SIZE );
    
    *aSeekFunc = stndrSeekFunctions[ aFlag & (SMI_TRAVERSE_MASK |
                                              SMI_PREVIOUS_MASK |
                                              SMI_LOCK_MASK) ];

    sSQLStat = aIterator->mProperties->mStatistics;
    IDV_SQL_ADD( sSQLStat, mDiskCursorIndexScan, 1 );

    if( sSQLStat != NULL )
    {
        IDV_SESS_ADD( sSQLStat->mSess,
                      IDV_STAT_INDEX_DISK_CURSOR_IDX_SCAN, 1 );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if( sStackInit == ID_TRUE )
    {
        (void)stndrStackMgr::destroy( &aIterator->mStack );
    }

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION :
 * ------------------------------------------------------------------*
 * �ε����� traverse�ϱ� ���� iterator�� �����Ѵ�. stack traverse�� ����
 * �������� ������ �޸𸮸� ��ȯ�Ѵ�.
 *********************************************************************/
IDE_RC stndrRTree::dest( stndrIterator * aIterator )
{
    stndrStackMgr::destroy( &aIterator->mStack );
    
    return IDE_SUCCESS;
}
#if 0
/*********************************************************************
 * FUNCTION DESCRIPTION :                                            *
 * ------------------------------------------------------------------*
 * Unchained Key(Normal Key)�� Chained Key�� �����Ѵ�.               
 * Chained Key��� ���� Key�� ����Ű�� Ʈ����� ������ CTS Chain��
 * ������ �ǹ��Ѵ�.                                                 
 * �̹� Chained Key�� �� �ٽ� Chained Key�� �ɼ� ������, Chained    
 * Key�� ���� ������ UNDO�� ��ϵǰ�, ���� Visibility �˻�ÿ�      
 * �̿�ȴ�.
 *
 * !!CAUTION!! : Disk R-Tree�� ��� Chained Key�� ���� �� ��� CTS��
 * CommitSCN�� Ű�� mCreateSCN, mLimitSCN�� �����Ѵ�. findChainedKey �ÿ�
 * Ű�� mCreateSCN �Ǵ� mLimitSCN�� Undo ���ڵ忡 ����� CTS�� CommitSCN��
 * ���Ͽ� �ش� CTS�� Chained Key ���θ� �Ǵ��Ѵ�.
 *********************************************************************/
IDE_RC stndrRTree::logAndMakeChainedKeys( sdrMtx          * aMtx,
                                          sdpPhyPageHdr   * aNode,
                                          UChar             aCTSlotNum,
                                          UChar           * aContext,
                                          UChar           * aKeyList,
                                          UShort          * aKeyListSize,
                                          UShort          * aChainedKeyCount )
{
    IDE_TEST( writeChainedKeysLog( aMtx,
                                   aNode,
                                   aCTSlotNum )
              != IDE_SUCCESS );

    IDE_TEST( makeChainedKeys( aNode,
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
 * Unchained Key(Normal Key)�� Chained Key�� �����Ѵ�.               
 * Chained Key��� ���� Key�� ����Ű�� Ʈ����� ������ CTS Chain��
 * ������ �ǹ��Ѵ�.                                                 
 * �̹� Chained Key�� �� �ٽ� Chained Key�� �ɼ� ������, Chained    
 * Key�� ���� ������ UNDO�� ��ϵǰ�, ���� Visibility �˻�ÿ�      
 * �̿�ȴ�.
 *
 * !!CAUTION!! : Disk R-Tree�� ��� Chained Key�� ���� �� ��� CTS��
 * CommitSCN�� Ű�� mCreateSCN, mLimitSCN�� �����Ѵ�. findChainedKey �ÿ�
 * Ű�� mCreateSCN �Ǵ� mLimitSCN�� Undo ���ڵ忡 ����� CTS�� CommitSCN��
 * ���Ͽ� �ش� CTS�� Chained Key ���θ� �Ǵ��Ѵ�.
 *********************************************************************/
IDE_RC stndrRTree::writeChainedKeysLog( sdrMtx          * aMtx,
                                        sdpPhyPageHdr   * aNode,
                                        UChar             aCTSlotNum )
{
    smSCN                     sCommitSCN;
    sdnCTL                  * sCTL;
    sdnCTS                  * sCTS;

    sCTL = sdnIndexCTL::getCTL( aNode );
    sCTS = sdnIndexCTL::getCTS( sCTL, aCTSlotNum );
    SM_SET_SCN( &sCommitSCN, &sCTS->mCommitSCN );

    IDE_TEST( sdrMiniTrans::writeLogRec( aMtx,
                                         (UChar*)aNode,
                                         NULL,
                                         ID_SIZEOF(UChar) + ID_SIZEOF(smSCN),
                                         SDR_STNDR_MAKE_CHAINED_KEYS )
              != IDE_SUCCESS );

    IDE_TEST( sdrMiniTrans::write( aMtx,
                                   (void*)&aCTSlotNum,
                                   ID_SIZEOF(UChar))
              != IDE_SUCCESS );

    IDE_TEST( sdrMiniTrans::write( aMtx,
                                   (void*)&sCommitSCN,
                                   ID_SIZEOF(smSCN))
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
 * Unchained Key(Normal Key)�� Chained Key�� �����Ѵ�.               
 * Chained Key��� ���� Key�� ����Ű�� Ʈ����� ������ CTS Chain��
 * ������ �ǹ��Ѵ�.                                                 
 * �̹� Chained Key�� �� �ٽ� Chained Key�� �ɼ� ������, Chained    
 * Key�� ���� ������ UNDO�� ��ϵǰ�, ���� Visibility �˻�ÿ�      
 * �̿�ȴ�.
 *
 * !!CAUTION!! : Disk R-Tree�� ��� Chained Key�� ���� �� ��� CTS��
 * CommitSCN�� Ű�� mCreateSCN, mLimitSCN�� �����Ѵ�. findChainedKey �ÿ�
 * Ű�� mCreateSCN �Ǵ� mLimitSCN�� Undo ���ڵ忡 ����� CTS�� CommitSCN��
 * ���Ͽ� �ش� CTS�� Chained Key ���θ� �Ǵ��Ѵ�.
 *********************************************************************/
IDE_RC stndrRTree::makeChainedKeys( sdpPhyPageHdr   * aNode,
                                    UChar             aCTSlotNum,
                                    UChar           * /* aContext */,
                                    UChar           * /* aKeyList */,
                                    UShort          * aKeyListSize,
                                    UShort          * aChainedKeyCount )
{
    UInt                      i;
    UShort                    sKeyCount;
    UChar                   * sSlotDirPtr;
    smSCN                     sCommitSCN;
    sdnCTL                  * sCTL;
    sdnCTS                  * sCTS;
    stndrLKey               * sLeafKey;

    sCTL = sdnIndexCTL::getCTL( aNode );
    sCTS = sdnIndexCTL::getCTS( sCTL, aCTSlotNum );
    SM_SET_SCN( &sCommitSCN, &sCTS->mCommitSCN );

    *aKeyListSize = 0;

    sSlotDirPtr = sdpPhyPage::getSlotDirStartPtr((UChar*)aNode);
    sKeyCount   = sdpSlotDirectory::getCount(sSlotDirPtr);

    for( i = 0; i < sKeyCount; i++ )
    {
        IDE_TEST ( sdpSlotDirectory::getPagePtrFromSlotNum( sSlotDirPtr,
                                                            i,
                                                            (UChar**)&sLeafKey )
                   != IDE_SUCCESS );
        /*
         * DEAD KEY�� STABLE KEY�� Chained Key�� �ɼ� ����.
         */
        if( (STNDR_GET_STATE( sLeafKey  ) == STNDR_KEY_DEAD) ||
            (STNDR_GET_STATE( sLeafKey  ) == STNDR_KEY_STABLE) )
        {
            continue;
        }

        /*
         * �̹� Chained Key�� �ٽ� Chained Key�� �ɼ� ����.
         */
        if( (STNDR_GET_CCTS_NO( sLeafKey ) == aCTSlotNum) &&
            (STNDR_GET_CHAINED_CCTS( sLeafKey ) == SDN_CHAINED_NO) )
        {
            STNDR_SET_CHAINED_CCTS( sLeafKey , SDN_CHAINED_YES );
            STNDR_SET_CSCN( sLeafKey, &sCommitSCN );
            (*aChainedKeyCount)++;
        }
        if( (STNDR_GET_LCTS_NO( sLeafKey ) == aCTSlotNum) &&
            (STNDR_GET_CHAINED_LCTS( sLeafKey ) == SDN_CHAINED_NO) )
        {
            STNDR_SET_CHAINED_LCTS( sLeafKey , SDN_CHAINED_YES );
            STNDR_SET_LSCN( sLeafKey, &sCommitSCN );
            (*aChainedKeyCount)++;
        }
    }

    /*
     * Key�� ���°� DEAD�� ���( LimitCTS�� Stamping�� �� ���) ���
     * CTS.mRefCnt���� ������ �ִ�
     */
    if( (*aChainedKeyCount > sdnIndexCTL::getRefKeyCount(aNode, aCTSlotNum))
        ||
        (ID_SIZEOF(sdnCTS) + ID_SIZEOF(UShort) + *aKeyListSize >= SD_PAGE_SIZE) )
    {
        ideLog::log( IDE_SERVER_0,
                     "CTS slot number : %u"
                     ", Chained key count : %u"
                     ", Ref Key count : %u"
                     ", Key List size : %u\n",
                     aCTSlotNum, *aChainedKeyCount,
                     sdnIndexCTL::getRefKeyCount( aNode, aCTSlotNum ),
                     *aKeyListSize );
        dumpIndexNode( aNode );
        IDE_ASSERT( 0 );
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
 * Chained Key�� Unchained Key(Normal Key)�� �����Ѵ�.               
 * Unchained Key��� ���� Key�� ����Ű�� Ʈ����� ������ Key.CTS#��
 * ����Ű�� CTS�� ������ �ǹ��Ѵ�.
 *********************************************************************/
IDE_RC stndrRTree::logAndMakeUnchainedKeys( idvSQL        * aStatistics,
                                            sdrMtx        * aMtx,
                                            sdpPhyPageHdr * aNode,
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

    sSlotDirPtr = sdpPhyPage::getSlotDirStartPtr((UChar*)aNode);
    sSlotCount  = sdpSlotDirectory::getCount(sSlotDirPtr);

    /* SlotNum(UShort) + CreateCTS(UChar) + LimitCTS(UChar) */
    IDE_TEST( iduMemMgr::malloc(IDU_MEM_SM_SDN,
                ID_SIZEOF(UChar) * 4 * sSlotCount,
                (void **)&sUnchainedKey,
                IDU_MEM_IMMEDIATE )
            != IDE_SUCCESS);
    sState = 1;

    IDE_TEST( makeUnchainedKeys( aStatistics,
                                 aNode,
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
                                     aNode,
                                     *aUnchainedKeyCount,
                                     sUnchainedKey,
                                     sUnchainedKeySize )
              != IDE_SUCCESS );

    sState = 0;
    IDE_TEST( iduMemMgr::free(sUnchainedKey) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if( sState == 1 )
    {
        (void)iduMemMgr::free( sUnchainedKey );
    }

    return IDE_FAILURE;
}
#endif
#if 0
/*********************************************************************
 * FUNCTION DESCRIPTION :                                            *
 * ------------------------------------------------------------------*
 * Chained Key�� Unchained Key(Normal Key)�� �����Ѵ�.               
 * Unchained Key��� ���� Key�� ����Ű�� Ʈ����� ������ Key.CTS#��
 * ����Ű�� CTS�� ������ �ǹ��Ѵ�.
 *********************************************************************/
IDE_RC stndrRTree::writeUnchainedKeysLog( sdrMtx        * aMtx,
                                          sdpPhyPageHdr * aNode,
                                          UShort          aUnchainedKeyCount,
                                          UChar         * aUnchainedKey,
                                          UInt            aUnchainedKeySize )
{
    IDE_TEST( sdrMiniTrans::writeLogRec( aMtx,
                                         (UChar*)aNode,
                                         NULL,
                                         ID_SIZEOF(UShort) + aUnchainedKeySize,
                                         SDR_STNDR_MAKE_UNCHAINED_KEYS )
              != IDE_SUCCESS );

    IDE_TEST( sdrMiniTrans::write( aMtx,
                                   (void*)&aUnchainedKeyCount,
                                   ID_SIZEOF(UShort) )
              != IDE_SUCCESS );

    IDE_TEST( sdrMiniTrans::write( aMtx,
                                   (void*)aUnchainedKey,
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
 * Chained Key�� Unchained Key(Normal Key)�� �����Ѵ�.               
 * Unchained Key��� ���� Key�� ����Ű�� Ʈ����� ������ Key.CTS#��
 * ����Ű�� CTS�� ������ �ǹ��Ѵ�.
 *********************************************************************/
IDE_RC stndrRTree::makeUnchainedKeys( idvSQL        * aStatistics,
                                      sdpPhyPageHdr * aNode,
                                      sdnCTS        * aCTS,
                                      UChar           aCTSlotNum,
                                      UChar         * aChainedKeyList,
                                      UShort          aChainedKeySize,
                                      UChar         * aContext,
                                      UChar         * aUnchainedKey,
                                      UInt          * aUnchainedKeySize,
                                      UShort        * aUnchainedKeyCount )

{
    UShort                    sKeyCount;
    UShort                    i;
    UChar                     sChainedCCTS;
    UChar                     sChainedLCTS;
    UChar                   * sSlotDirPtr;
    UChar                   * sUnchainedKey;
    UShort                    sUnchainedKeyCount = 0;
    UInt                      sCursor = 0;
    stndrLKey               * sLeafKey;
    stndrCallbackContext    * sContext;
    scOffset                  sSlotOffset;

    IDE_ASSERT( aContext      != NULL );
    IDE_ASSERT( aUnchainedKey != NULL );

    *aUnchainedKeyCount = 0;
    *aUnchainedKeySize  = 0;

    sSlotDirPtr   = sdpPhyPage::getSlotDirStartPtr((UChar*)aNode);
    sKeyCount     = sdpSlotDirectory::getCount(sSlotDirPtr);
    sUnchainedKey = aUnchainedKey;

    for( i = 0; i < sKeyCount; i++ )
    {
        IDE_TEST( sdpSlotDirectory::getPagePtrFromSlotNum( sSlotDirPtr,
                                                           i, 
                                                           (UChar**)&sLeafKey )
                  != IDE_SUCCESS );

        if( (STNDR_GET_STATE( sLeafKey ) == STNDR_KEY_DEAD) ||
            (STNDR_GET_STATE( sLeafKey ) == STNDR_KEY_STABLE) )
        {
            continue;
        }
        
        sChainedCCTS = SDN_CHAINED_NO;
        sChainedLCTS = SDN_CHAINED_NO;

        sContext = (stndrCallbackContext*)aContext;
        sContext->mLeafKey  = sLeafKey;

        /*
         * Chained Key���
         */
        if( (STNDR_GET_CCTS_NO( sLeafKey ) == aCTSlotNum) &&
            (STNDR_GET_CHAINED_CCTS( sLeafKey ) == SDN_CHAINED_YES) )
        {
            /*
             * Chain�� ��ÿ��� �־�����, Chaind Key�� �ش� ����������
             * �������� �������� �ִ�.
             * 1. SMO�� ���ؼ� Chaind Key�� �̵��� ���
             * 2. Chaind Key�� DEAD���� �϶�
             *    (LIMIT CTS�� Soft Key Stamping�� �� ���)
             */
            if( findChainedKey( aStatistics,
                                aCTS,
                                aChainedKeyList,
                                aChainedKeySize,
                                &sChainedCCTS,
                                &sChainedLCTS,
                                (UChar*)sContext ) == ID_TRUE )
            {
                idlOS::memcpy( sUnchainedKey + sCursor, &i, ID_SIZEOF(UShort) );
                sCursor += 2;
                *(sUnchainedKey + sCursor) = sChainedCCTS;
                sCursor += 1;
                *(sUnchainedKey + sCursor) = sChainedLCTS;
                sCursor += 1;

                if( sChainedCCTS == SDN_CHAINED_YES )
                {
                    sUnchainedKeyCount++;
                    STNDR_SET_CHAINED_CCTS( sLeafKey , SDN_CHAINED_NO );
                    IDE_TEST( sdpSlotDirectory::getValue( sSlotDirPtr,
                                                          i,
                                                          &sSlotOffset )
                              != IDE_SUCCESS );
                    sdnIndexCTL::addRefKey( aNode,
                                            aCTSlotNum,
                                            sSlotOffset);
                }
                if( sChainedLCTS == SDN_CHAINED_YES )
                {
                    sUnchainedKeyCount++;
                    STNDR_SET_CHAINED_LCTS( sLeafKey , SDN_CHAINED_NO );
                    IDE_TEST( sdpSlotDirectory::getValue( sSlotDirPtr,
                                                          i,
                                                          &sSlotOffset )
                              != IDE_SUCCESS );
                    sdnIndexCTL::addRefKey( aNode,
                                            aCTSlotNum,
                                            sSlotOffset );
                }
            }
            else
            {
                if( sdnIndexCTL::hasChainedCTS( aNode, aCTSlotNum ) != ID_TRUE )
                {
                    ideLog::log( IDE_SERVER_0,
                                 "CTS slot number : %u"
                                 ", Key slot number : %u\n",
                                 aCTSlotNum, i );
                    dumpIndexNode( aNode );
                    IDE_ASSERT( 0 );
                }
            }
        }
        else
        {
            if( (STNDR_GET_LCTS_NO( sLeafKey ) == aCTSlotNum) &&
                (STNDR_GET_CHAINED_LCTS( sLeafKey ) == SDN_CHAINED_YES) )
            {
                if( findChainedKey( aStatistics,
                                    aCTS,
                                    aChainedKeyList,
                                    aChainedKeySize,
                                    &sChainedCCTS,
                                    &sChainedLCTS,
                                    (UChar*)sContext ) == ID_TRUE )
                {
                    idlOS::memcpy( sUnchainedKey + sCursor, &i, ID_SIZEOF(UShort) );
                    sCursor += 2;
                    *(sUnchainedKey + sCursor) = SDN_CHAINED_NO;
                    sCursor += 1;
                    *(sUnchainedKey + sCursor) = sChainedLCTS;
                    sCursor += 1;

                    if( sChainedLCTS == SDN_CHAINED_YES )
                    {
                        sUnchainedKeyCount++;
                        STNDR_SET_CHAINED_LCTS( sLeafKey , SDN_CHAINED_NO );
                        IDE_TEST( sdpSlotDirectory::getValue( sSlotDirPtr,
                                                              i,
                                                              &sSlotOffset )
                                 != IDE_SUCCESS );
                        sdnIndexCTL::addRefKey( aNode,
                                                aCTSlotNum,
                                                sSlotOffset );
                    }
                }
                else
                {
                    if( sdnIndexCTL::hasChainedCTS( aNode, aCTSlotNum ) != ID_TRUE )
                    {
                        ideLog::log( IDE_SERVER_0,
                                     "CTS slot number : %u"
                                     ", Key slot number : %u\n",
                                     aCTSlotNum, i );
                        dumpIndexNode( aNode );
                        IDE_ASSERT( 0 );
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
 * �־��� Leaf Key�� sCTS�� Chained Key ������ Ȯ���Ѵ�.
 * sCTS�� mCommitSCN�� Leaf Key�� mCreateSCN �Ǵ� mLimitSCN �� �����ϸ�
 * sCTS�� Chained Key �̴�.
 *********************************************************************/
idBool stndrRTree::findChainedKey( idvSQL   * /* aStatistics */,
                                   sdnCTS   * sCTS,
                                   UChar    * /* aChainedKeyList */,
                                   UShort     /* aChainedKeySize */,
                                   UChar    * aChainedCCTS,
                                   UChar    * aChainedLCTS,
                                   UChar    * aContext )
{
    stndrCallbackContext    * sContext;
    stndrLKey               * sLeafKey;
    smSCN                     sCommitSCN;
    smSCN                     sKeyCSCN;
    smSCN                     sKeyLSCN;
    idBool                    sFound = ID_FALSE;
    
    
    IDE_DASSERT( aContext != NULL );
    
    sContext = (stndrCallbackContext*)aContext;

    IDE_DASSERT( sContext->mIndex != NULL );
    IDE_DASSERT( sContext->mLeafKey  != NULL );
    IDE_DASSERT( sContext->mStatistics != NULL );
    
    sLeafKey = sContext->mLeafKey;

    sCommitSCN = sCTS->mCommitSCN;
    STNDR_GET_CSCN( sLeafKey, &sKeyCSCN );
    STNDR_GET_LSCN( sLeafKey, &sKeyLSCN );

    if( SM_SCN_IS_EQ( &sKeyCSCN, &sCommitSCN ) )
    {
        if( aChainedCCTS != NULL )
        {
            *aChainedCCTS = SDN_CHAINED_YES;
            sFound = ID_TRUE;
        }
    }
    
    if( SM_SCN_IS_EQ( &sKeyLSCN, &sCommitSCN ) )
    {
        if( aChainedLCTS != NULL )
        {
            *aChainedLCTS = SDN_CHAINED_YES;
            sFound = ID_TRUE;
        }
    }

    return sFound;
}
#endif
/*********************************************************************
 * FUNCTION DESCRIPTION :
 * ------------------------------------------------------------------*
 * aIterator�� ���� ����Ű�� �ִ� Row�� ���ؼ� XLock�� ȹ���մϴ�.
 *
 * aProperties - [IN] Index Iterator
 *
 * Related Issue:
 *   BUG-19068: smiTableCursor�� ���簡��Ű�� �ִ� Row�� ���ؼ�
 *              Lock�� ������ �մ� Interface�� �ʿ��մϴ�.
 *********************************************************************/
IDE_RC stndrRTree::lockRow( stndrIterator * aIterator )
{
    scSpaceID sTableTSID;

    sTableTSID = ((smcTableHeader*)aIterator->mTable)->mSpaceID;

    if( aIterator->mCurRowPtr == aIterator->mCacheFence )
    {
        ideLog::log( IDE_SERVER_0, "Index iterator info:\n" );
        ideLog::logMem( IDE_SERVER_0,
                        (UChar *)aIterator,
                        ID_SIZEOF(stndrIterator) );
        IDE_ASSERT( 0 );
    }

    return sdnManager::lockRow( aIterator->mProperties,
                                aIterator->mTrans,
                                &(aIterator->mSCN),
                                &(aIterator->mInfinite),
                                sTableTSID,
                                SD_MAKE_SID_FROM_GRID(aIterator->mRowGRID),
                                aIterator->mStatement->isForbiddenToRetry() );
}

/*********************************************************************
 * FUNCTION DESCRIPTION :                                            *
 * ------------------------------------------------------------------*
 * ������ Ű�� ã��, ã�� ��忡 Ű�� �ϳ��� �����Ѵٸ� Empty Node��
 * �����Ѵ�. ����� Node�� ������ ���� �����Ҷ� Insert Transaction�� 
 * ���ؼ� ����ȴ�. 
 * �������굵 Ʈ����������� ����� ����(CTS)�� �ʿ��ϸ�, �̸� �Ҵ�
 * ������ ���� ��쿡�� �Ҵ������ ���������� ��ٷ��� �Ѵ�.
 *********************************************************************/
IDE_RC stndrRTree::deleteKey( idvSQL        * aStatistics,
                              void          * aTrans,
                              void          * aIndex,
                              SChar         * aKeyValue,
                              smiIterator   * aIterator,
                              sdSID           aRowSID )
{
    sdrMtx            sMtx;
    stndrPathStack    sStack;
    sdpPhyPageHdr   * sLeafNode;
    SShort            sLeafKeySeq;
    sdpPhyPageHdr   * sTargetNode;
    SShort            sTargetKeySeq;
    scPageID          sEmptyNodePID[2] = { SD_NULL_PID, SD_NULL_PID };
    UInt              sEmptyNodeCount = 0;
    stndrKeyInfo      sKeyInfo;
    idBool            sMtxStart = ID_FALSE;
    smnIndexHeader  * sIndex;
    stndrHeader     * sHeader;
    stndrNodeHdr    * sNodeHdr;
    smLSN             sNTA;
    idvWeArgs         sWeArgs;
    idBool            sIsSuccess;
    idBool            sIsRetry = ID_FALSE;
    UInt              sAllocPageCount = 0;
    stndrStatistic    sIndexStat;
    UShort            sKeyValueLength;
    idBool            sIsIndexable;
    idBool            sIsPessimistic = ID_FALSE;


    isIndexableRow( aIndex, aKeyValue, &sIsIndexable );
    
    IDE_TEST_RAISE( sIsIndexable == ID_FALSE, RETURN_NO_DELETE );
    
    sIndex  = (smnIndexHeader*)aIndex;
    sHeader = (stndrHeader*)sIndex->mHeader;

    // To fix BUG-17939
    IDE_DASSERT( sHeader->mSdnHeader.mIsConsistent == ID_TRUE );

    sKeyInfo.mKeyValue   = (UChar*)&(((stdGeometryHeader*)aKeyValue)->mMbr);
    sKeyInfo.mRowPID     = SD_MAKE_PID(aRowSID);
    sKeyInfo.mRowSlotNum = SD_MAKE_SLOTNUM(aRowSID);

    IDL_MEM_BARRIER;
    
    sNTA = smxTrans::getTransLstUndoNxtLSN( aTrans );

    idlOS::memset( &sIndexStat, 0x00, sizeof(sIndexStat) );

  retry:

    IDE_TEST( sdrMiniTrans::begin( aStatistics,
                                   &sMtx,
                                   aTrans,
                                   SDR_MTX_LOGGING,
                                   ID_TRUE,/*MtxUndoable(PROJ-2162)*/
                                   gMtxDLogType ) != IDE_SUCCESS );
    sMtxStart = ID_TRUE;

    sIsRetry = ID_FALSE;

    // init stack
    // BUG-29039 codesonar ( Uninitialized Variable )
    sStack.mDepth = -1;

    sIsPessimistic = ID_FALSE;

  retraverse:
    
    // traverse
    sIsSuccess = ID_TRUE;
    
    IDE_TEST( traverse( aStatistics,
                        sHeader,
                        &sMtx,
                        &aIterator->infinite,
                        NULL, /* aFstDiskViewSCN */
                        &sKeyInfo,
                        SD_NULL_PID,
                        STNDR_TRAVERSE_DELETE_KEY,
                        &sStack,
                        &sIndexStat,
                        &sLeafNode,
                        &sLeafKeySeq ) != IDE_SUCCESS );

    if( sLeafNode == NULL )
    {
        ideLog::log( IDE_SERVER_0, "\
=============================================\n\
              NAME : %s                      \n\
              ID   : %u                      \n\
                                             \n\
stndrRTree::deleteKey, Traverse error        \n\
ROW_PID:%u, ROW_SLOTNUM: %u                  \n",
                 sIndex->mName,
                 sIndex->mId,
                 sKeyInfo.mRowPID,
                 sKeyInfo.mRowSlotNum );
        
        ideLog::log( IDE_SERVER_0,
                     "Leaf key sequence : %d\n",
                     sLeafKeySeq );
        
        ideLog::log( IDE_SERVER_0, "Index stack dump:\n" );
        ideLog::logMem( IDE_SERVER_0, (UChar *)&sStack, ID_SIZEOF(stndrPathStack) );
        
        ideLog::log( IDE_SERVER_0, "Index header dump:\n" );
        ideLog::logMem( IDE_SERVER_0, (UChar *)&sHeader, ID_SIZEOF(stndrHeader) );
        
        ideLog::log( IDE_SERVER_0, "Index run-time header dump:\n" );
        ideLog::logMem( IDE_SERVER_0, (UChar *)sIndex, ID_SIZEOF(smnIndexHeader) );
        
        ideLog::log( IDE_SERVER_0, "Key info dump:\n" );
        ideLog::logMem( IDE_SERVER_0, (UChar *)&sKeyInfo, ID_SIZEOF(stndrKeyInfo) );
        
        IDE_ASSERT( 0 );
    }

    IDE_TEST( deleteKeyFromLeafNode( aStatistics,
                                     &sIndexStat,
                                     &sMtx,
                                     sHeader,
                                     &aIterator->infinite,
                                     sLeafNode,
                                     &sLeafKeySeq,
                                     &sIsSuccess )
              != IDE_SUCCESS );
    
    if( sIsSuccess == ID_FALSE )
    {
        if( sIsPessimistic == ID_FALSE )
        {
            sIsPessimistic = ID_TRUE;

            sMtxStart = ID_FALSE;
            IDE_TEST( sdrMiniTrans::commit(&sMtx) != IDE_SUCCESS );

            IDE_TEST( sdrMiniTrans::begin( aStatistics,
                                           &sMtx,
                                           aTrans,
                                           SDR_MTX_LOGGING,
                                           ID_TRUE,/*MtxUndoable(PROJ-2162)*/
                                           gMtxDLogType )
                      != IDE_SUCCESS );
            sMtxStart = ID_TRUE;
            
            IDV_WEARGS_SET(
                &sWeArgs,
                IDV_WAIT_INDEX_LATCH_DRDB_RTREE_INDEX_SMO,
                0,   // WaitParam1
                0,   // WaitParam2
                0 ); // WaitParam3

            IDE_TEST( sHeader->mSdnHeader.mLatch.lockWrite( aStatistics,
                                                            &sWeArgs )
                      != IDE_SUCCESS);

            IDE_TEST( sdrMiniTrans::push(
                          &sMtx,
                          (void*)&sHeader->mSdnHeader.mLatch,
                          SDR_MTX_LATCH_X)
                      != IDE_SUCCESS );

            sStack.mDepth = -1;
            goto retraverse;
        }
        else
        {
            sKeyValueLength = getKeyValueLength();

            IDE_TEST( splitLeafNode( aStatistics,
                                     &sIndexStat,
                                     &sMtx,
                                     &sMtxStart,
                                     sHeader,
                                     &aIterator->infinite,
                                     &sStack,
                                     sLeafNode,
                                     &sKeyInfo,
                                     sKeyValueLength,
                                     sLeafKeySeq,
                                     ID_FALSE,
                                     smuProperty::getRTreeSplitMode(),
                                     &sTargetNode,
                                     &sTargetKeySeq,
                                     &sAllocPageCount,
                                     &sIsRetry )
                      != IDE_SUCCESS );

            if( sIsRetry == ID_TRUE )
            {
                // key propagation�� ���� latch�� ��� �߿� root node�� ����� ���
                sMtxStart = ID_FALSE;
                IDE_TEST( sdrMiniTrans::commit(&sMtx) != IDE_SUCCESS );
                sIndexStat.mOpRetryCount++;
                goto retry;
            }

            IDE_TEST( deleteKeyFromLeafNode( aStatistics,
                                             &sIndexStat,
                                             &sMtx,
                                             sHeader,
                                             &aIterator->infinite,
                                             sTargetNode,
                                             &sTargetKeySeq,
                                             &sIsSuccess )
                      != IDE_SUCCESS );

            if( sIsSuccess != ID_TRUE )
            {
                ideLog::log( IDE_SERVER_0,
                             "Target Key sequence : %d\n",
                             sTargetKeySeq );

                ideLog::log( IDE_SERVER_0, "Index header dump:\n" );
                ideLog::logMem( IDE_SERVER_0, (UChar *)sHeader, ID_SIZEOF(stndrHeader) );
                
                dumpIndexNode( sTargetNode );
                IDE_ASSERT( 0 );
            }

            findEmptyNodes( &sMtx,
                            sHeader,
                            sLeafNode,
                            sEmptyNodePID,
                            &sEmptyNodeCount );
                
            sIndexStat.mNodeSplitCount++;
        }
    }
    else
    {
        sNodeHdr = (stndrNodeHdr*)
            sdpPhyPage::getLogicalHdrStartPtr( (UChar*)sLeafNode );

        if( sNodeHdr->mUnlimitedKeyCount == 0 )
        {
            sEmptyNodePID[0] = sLeafNode->mPageID;
            sEmptyNodeCount = 1;
        }
    }

    sdrMiniTrans::setRefNTA( &sMtx,
                             sHeader->mSdnHeader.mIndexTSID,
                             SDR_OP_STNDR_DELETE_KEY_WITH_NTA,
                             &sNTA );

    sMtxStart = ID_FALSE;
    IDE_TEST( sdrMiniTrans::commit(&sMtx) != IDE_SUCCESS );
    
    if( sAllocPageCount > 0 )
    {
        IDE_TEST( nodeAging( aStatistics,
                             aTrans,
                             &sIndexStat,
                             sHeader,
                             sAllocPageCount )
                  != IDE_SUCCESS );
    }
    
    if( sEmptyNodeCount > 0 )
    {
        IDE_TEST( linkEmptyNodes( aStatistics,
                                  aTrans,
                                  sHeader,
                                  &sIndexStat,
                                  sEmptyNodePID )
                  != IDE_SUCCESS );
    }

    STNDR_ADD_STATISTIC( &sHeader->mDMLStat, &sIndexStat );

    IDE_EXCEPTION_CONT( RETURN_NO_DELETE );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if( sMtxStart == ID_TRUE )
    {
        sMtxStart = ID_FALSE;
        (void)sdrMiniTrans::rollback( &sMtx );
    }

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION :                                            *
 * ------------------------------------------------------------------*
 * InsertKey�� Rollback�Ǵ� ��쿡 �α׿� ����Ͽ� ȣ��ȴ�.         
 * �ѹ��� Ű�� ã��, ã�� ��忡 Ű�� �ϳ��� �����Ѵٸ� Empty Node�� 
 * �����Ѵ�. ����� Node�� ������ ���������Ҷ� Insert Transaction��  
 * ���ؼ� ����ȴ�.                                                
 * �ش翬���� Ʈ����������� �Ҵ��� �ʿ䰡 ����. ��, Rollback����   
 * �̹� Ʈ������� �Ҵ� ���� ������ ����Ѵ�.                        
 *********************************************************************/
IDE_RC stndrRTree::insertKeyRollback( idvSQL    * aStatistics,
                                      void      * aMtx,
                                      void      * aIndex,
                                      SChar     * aKeyValue,
                                      sdSID       aRowSID,
                                      UChar     * /*aRollbackContext*/,
                                      idBool      /*aIsDupKey*/ )
{
    sdrMtx                  * sMtx;
    stndrPathStack            sStack;
    stndrHeader             * sHeader;
    stndrKeyInfo              sKeyInfo;
    stndrLKey               * sLeafKey;
    stndrStatistic            sIndexStat;
    sdpPhyPageHdr           * sLeafNode;
    stndrNodeHdr            * sNodeHdr;
    UShort                    sUnlimitedKeyCount;
    SShort                    sLeafKeySeq;
    UShort                    sTotalDeadKeySize = 0;
    UShort                    sKeyOffset;
    UChar                   * sSlotDirPtr;
    UChar                     sCurCreateCTS;
    idBool                    sIsSuccess = ID_TRUE;
    UShort                    sTotalTBKCount = 0;

    
    idlOS::memset( &sIndexStat, 0x00, sizeof(sIndexStat) );

    sMtx = (sdrMtx*)aMtx;
    sHeader = (stndrHeader*)((smnIndexHeader*)aIndex)->mHeader;

    IDE_TEST_RAISE( sHeader->mSdnHeader.mIsConsistent == ID_FALSE,
                    SKIP_UNDO );
    
    sKeyInfo.mKeyValue = (UChar*)aKeyValue;
    sKeyInfo.mRowPID = SD_MAKE_PID( aRowSID );
    sKeyInfo.mRowSlotNum = SD_MAKE_SLOTNUM( aRowSID );

  retraverse:

    // init stack
    sStack.mDepth = -1;

    IDE_TEST( traverse( aStatistics,
                        sHeader,
                        sMtx,
                        NULL, /* aCursorSCN */
                        NULL, /* aFstDiskViewSCN */
                        &sKeyInfo,
                        SD_NULL_PID,
                        STNDR_TRAVERSE_INSERTKEY_ROLLBACK,
                        &sStack,
                        &sIndexStat,
                        &sLeafNode,
                        &sLeafKeySeq )
              != IDE_SUCCESS );

    if( sLeafNode == NULL )
    {
        ideLog::log( IDE_SERVER_0, "\
============================================= \n\
              NAME : %s                       \n\
              ID   : %u                       \n\
                                              \n\
The root node is not exist.                   \n\
stndrRTree::insertKeyRollback, Traverse error \n\
ROW_PID:%u, ROW_SLOTNUM: %u                   \n",
                 ((smnIndexHeader*)aIndex)->mName,
                 ((smnIndexHeader*)aIndex)->mId,
                 sKeyInfo.mRowPID,
                 sKeyInfo.mRowSlotNum );
        
        ideLog::log( IDE_SERVER_0,
                     "Leaf Key sequence : %d\n",
                     sLeafKeySeq );
        
        ideLog::log( IDE_SERVER_0, "Index stack dump:\n" );
        ideLog::logMem( IDE_SERVER_0, (UChar *)&sStack, ID_SIZEOF(stndrPathStack) );
        
        ideLog::log( IDE_SERVER_0, "Index header dump:\n" );
        ideLog::logMem( IDE_SERVER_0, (UChar *)&sHeader, ID_SIZEOF(stndrHeader) );
        
        ideLog::log( IDE_SERVER_0, "Index run-time header dump:\n" );
        ideLog::logMem( IDE_SERVER_0, (UChar *)aIndex, ID_SIZEOF(smnIndexHeader) );
        
        ideLog::log( IDE_SERVER_0, "Key info dump:\n" );
        ideLog::logMem( IDE_SERVER_0, (UChar *)&sKeyInfo, ID_SIZEOF(stndrKeyInfo) );
        IDE_ASSERT( 0 );
    }

    sNodeHdr    = (stndrNodeHdr*)sdpPhyPage::getLogicalHdrStartPtr((UChar*)sLeafNode);
    sSlotDirPtr = sdpPhyPage::getSlotDirStartPtr((UChar*)sLeafNode);
    IDE_TEST ( sdpSlotDirectory::getPagePtrFromSlotNum( sSlotDirPtr,
                                                        sLeafKeySeq,
                                                        (UChar**)&sLeafKey )
               != IDE_SUCCESS );
    IDE_TEST ( sdpSlotDirectory::getValue( sSlotDirPtr, 
                                           sLeafKeySeq,
                                           &sKeyOffset )
               != IDE_SUCCESS );

    if( sNodeHdr->mUnlimitedKeyCount == 1 )
    {
        IDE_TEST( linkEmptyNode( aStatistics,
                                 &sIndexStat,
                                 sHeader,
                                 sMtx,
                                 sLeafNode,
                                 &sIsSuccess )
                  != IDE_SUCCESS );
        
        if( sIsSuccess == ID_FALSE )
        {
            IDE_TEST( sdrMiniTrans::commit( sMtx ) != IDE_SUCCESS );
            
            IDE_TEST( sdrMiniTrans::begin( aStatistics,
                                           sMtx,
                                           sdrMiniTrans::getTrans(sMtx),
                                           SDR_MTX_LOGGING,
                                           ID_TRUE,/*MtxUndoable(PROJ-2162)*/
                                           gMtxDLogType ) != IDE_SUCCESS );
            
            goto retraverse;
        }
    }
    
    sCurCreateCTS = STNDR_GET_CCTS_NO( sLeafKey );
    
    sTotalDeadKeySize = sNodeHdr->mTotalDeadKeySize;
    sTotalDeadKeySize += getKeyLength( (UChar*)sLeafKey ,
                                       ID_TRUE /* aIsLeaf */ );
    sTotalDeadKeySize += ID_SIZEOF( sdpSlotEntry );

    STNDR_SET_CCTS_NO( sLeafKey , SDN_CTS_INFINITE );
    STNDR_SET_STATE( sLeafKey , STNDR_KEY_DEAD );

    IDE_TEST(sdrMiniTrans::writeNBytes( sMtx,
                                        (UChar*)&sNodeHdr->mTotalDeadKeySize,
                                        (void*)&sTotalDeadKeySize,
                                        ID_SIZEOF(sNodeHdr->mTotalDeadKeySize) )
             != IDE_SUCCESS );

    IDE_TEST( sdrMiniTrans::writeNBytes( sMtx,
                                         (UChar*)sLeafKey->mTxInfo,
                                         (void*)sLeafKey->mTxInfo,
                                         ID_SIZEOF(UChar)*2 )
              != IDE_SUCCESS );

    sUnlimitedKeyCount = sNodeHdr->mUnlimitedKeyCount - 1;
    IDE_DASSERT( sUnlimitedKeyCount < sdpSlotDirectory::getCount( sSlotDirPtr ) );
    IDE_TEST( sdrMiniTrans::writeNBytes( sMtx,
                                         (UChar*)&(sNodeHdr->mUnlimitedKeyCount),
                                         (void*)&sUnlimitedKeyCount,
                                         ID_SIZEOF(UShort) )
              != IDE_SUCCESS );
    
    if( (STNDR_GET_STATE( sLeafKey ) == STNDR_KEY_DEAD) &&
        (STNDR_GET_TB_TYPE( sLeafKey ) == STNDR_KEY_TB_KEY) )
    {
        sTotalTBKCount = sNodeHdr->mTBKCount - 1;
        IDE_TEST( sdrMiniTrans::writeNBytes( sMtx,
                                             (UChar*)&(sNodeHdr->mTBKCount),
                                             (void*)&sTotalTBKCount,
                                             ID_SIZEOF(UShort) )
                  != IDE_SUCCESS );
    }
    
    if( SDN_IS_VALID_CTS( sCurCreateCTS ) )
    {
        IDE_TEST( sdnIndexCTL::unbindCTS( sMtx,
                                          sLeafNode,
                                          sCurCreateCTS,
                                          sKeyOffset )
                  != IDE_SUCCESS );
    }
    
    STNDR_ADD_STATISTIC( &sHeader->mDMLStat, &sIndexStat );
   
    IDE_EXCEPTION_CONT( SKIP_UNDO );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION :                                            *
 * ------------------------------------------------------------------*
 * DeleteKey�� Rollback�Ǵ� ��쿡 �α׿� ����Ͽ� ȣ��ȴ�.         
 * �ѹ��� Ű�� ã��, ã�� Ű�� ���¸� STABLE�̳� UNSTABLE�� �����Ѵ�.
 * �ش翬���� Ʈ����������� �Ҵ��� �ʿ䰡 ����. ��, Rollback����    
 * �̹� Ʈ������� �Ҵ� ���� ������ ����Ѵ�. 
 *********************************************************************/
IDE_RC stndrRTree::deleteKeyRollback( idvSQL    * aStatistics,
                                      void      * aMtx,
                                      void      * aIndex,
                                      SChar     * aKeyValue,
                                      sdSID       aRowSID,
                                      UChar     * aRollbackContext )
{
    stndrPathStack            sStack;
    stndrHeader             * sHeader;
    stndrKeyInfo              sKeyInfo;
    stndrLKey               * sLeafKey;
    stndrStatistic            sIndexStat;
    sdpPhyPageHdr           * sLeafNode;
    SShort                    sLeafKeySeq;
    stndrNodeHdr            * sNodeHdr;
    UShort                    sUnlimitedKeyCount;
    UChar                     sLimitCTS;
    smSCN                     sCommitSCN;
    smSCN                     sCursorSCN;
    smSCN                     sFstDiskViewSCN;
    UShort                    sKeyOffset;
    UChar                   * sSlotDirPtr;
    sdrMtx                  * sMtx;

    idlOS::memset( &sIndexStat, 0x00, sizeof(sIndexStat) );

    sMtx = (sdrMtx*)aMtx;
    sHeader = (stndrHeader*)((smnIndexHeader*)aIndex)->mHeader;

    IDE_TEST_RAISE( sHeader->mSdnHeader.mIsConsistent == ID_FALSE,
                    SKIP_UNDO );

    SM_SET_SCN( &sCursorSCN,
                &(((stndrRollbackContext*)aRollbackContext)->mLimitSCN) );

    SM_SET_SCN( &sFstDiskViewSCN,
                &(((stndrRollbackContext*)aRollbackContext)->mFstDiskViewSCN) );
    
    sKeyInfo.mKeyValue   = (UChar*)aKeyValue;
    sKeyInfo.mRowPID     = SD_MAKE_PID( aRowSID );
    sKeyInfo.mRowSlotNum = SD_MAKE_SLOTNUM( aRowSID );

    // init stack
    sStack.mDepth = -1;

    IDE_TEST( traverse( aStatistics,
                        sHeader,
                        sMtx,
                        &sCursorSCN,
                        &sFstDiskViewSCN,
                        &sKeyInfo,
                        SD_NULL_PID,
                        STNDR_TRAVERSE_DELETEKEY_ROLLBACK,
                        &sStack,
                        &sIndexStat,
                        &sLeafNode,
                        &sLeafKeySeq ) != IDE_SUCCESS );

    if( sLeafNode == NULL )
    {
        ideLog::log( IDE_SERVER_0, "\
============================================= \n\
              NAME : %s                       \n\
              ID   : %u                       \n\
                                              \n\
stndrRTree::deleteKeyRollback, Traverse error \n\
ROW_PID:%u, ROW_SLOTNUM: %u                   \n",
                 ((smnIndexHeader*)aIndex)->mName,
                 ((smnIndexHeader*)aIndex)->mId,
                 sKeyInfo.mRowPID,
                 sKeyInfo.mRowSlotNum );
        
        ideLog::log( IDE_SERVER_0,
                     "Leaf Key sequence : %d\n",
                     sLeafKeySeq );
        
        ideLog::log( IDE_SERVER_0, "Index stack dump:\n" );
        ideLog::logMem( IDE_SERVER_0, (UChar *)&sStack, ID_SIZEOF(stndrStack) );
        
        ideLog::log( IDE_SERVER_0, "Index header dump:\n" );
        ideLog::logMem( IDE_SERVER_0, (UChar *)&sHeader, ID_SIZEOF(stndrHeader) );
        
        ideLog::log( IDE_SERVER_0, "Index run-time header dump:\n" );
        ideLog::logMem( IDE_SERVER_0, (UChar *)aIndex, ID_SIZEOF(smnIndexHeader) );
        
        ideLog::log( IDE_SERVER_0, "Key info dump:\n" );
        ideLog::logMem( IDE_SERVER_0, (UChar *)&sKeyInfo, ID_SIZEOF(stndrKeyInfo) );
        
        dumpIndexNode( sLeafNode );
        IDE_ASSERT( 0 );
    }

    sNodeHdr = (stndrNodeHdr*)sdpPhyPage::getLogicalHdrStartPtr( (UChar*)sLeafNode );
    sSlotDirPtr = sdpPhyPage::getSlotDirStartPtr((UChar*)sLeafNode);
    IDE_TEST ( sdpSlotDirectory::getPagePtrFromSlotNum( sSlotDirPtr,
                                                        sLeafKeySeq,
                                                        (UChar**)&sLeafKey )
               != IDE_SUCCESS );
    IDE_TEST ( sdpSlotDirectory::getValue( sSlotDirPtr, 
                                           sLeafKeySeq,
                                           &sKeyOffset )
               != IDE_SUCCESS );
    sLimitCTS = STNDR_GET_LCTS_NO( sLeafKey  );

    sUnlimitedKeyCount = sNodeHdr->mUnlimitedKeyCount + 1;
    IDE_DASSERT( sUnlimitedKeyCount <= sdpSlotDirectory::getCount( sSlotDirPtr ) );
    
    IDE_TEST( sdrMiniTrans::writeNBytes( sMtx,
                                         (UChar*)&(sNodeHdr->mUnlimitedKeyCount),
                                         (void*)&sUnlimitedKeyCount,
                                         ID_SIZEOF(UShort) )
              != IDE_SUCCESS );

    if( SDN_IS_VALID_CTS( sLimitCTS ) )
    {
        IDE_TEST( sdnIndexCTL::unbindCTS( sMtx,
                                          sLeafNode,
                                          sLimitCTS,
                                          sKeyOffset )
                  != IDE_SUCCESS );
    }

    STNDR_SET_LCTS_NO( sLeafKey , SDN_CTS_INFINITE );
    
    if( STNDR_GET_CCTS_NO( sLeafKey ) == SDN_CTS_INFINITE )
    {
        STNDR_SET_STATE( sLeafKey , STNDR_KEY_STABLE );
    }
    else
    {
        if( STNDR_GET_CCTS_NO( sLeafKey ) == SDN_CTS_IN_KEY )
        {
            IDE_TEST( getCommitSCN( aStatistics,
                                    NULL,        /* aTrans */
                                    sLeafNode,
                                    (stndrLKeyEx*)sLeafKey ,
                                    ID_FALSE,    /* aIsLimitSCN */
                                    SM_SCN_INIT, /* aStmtViewSCN */
                                    &sCommitSCN )
                      != IDE_SUCCESS );

            if( isAgableTBK( sCommitSCN ) == ID_TRUE )
            {
                STNDR_SET_STATE( sLeafKey , STNDR_KEY_STABLE );
            }
            else
            {
                STNDR_SET_STATE( sLeafKey , STNDR_KEY_UNSTABLE );
            }
        }
        else
        {
            STNDR_SET_STATE( sLeafKey , STNDR_KEY_UNSTABLE );
        }
    }

    IDE_TEST( sdrMiniTrans::writeNBytes( sMtx,
                                         (UChar*)sLeafKey->mTxInfo,
                                         (void*)sLeafKey->mTxInfo,
                                         ID_SIZEOF(UChar)*2 )
              != IDE_SUCCESS );

    STNDR_ADD_STATISTIC( &sHeader->mDMLStat, &sIndexStat );

    IDE_EXCEPTION_CONT( SKIP_UNDO );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION :                                            *
 * ------------------------------------------------------------------*
 * ����ڰ� ���������� aging��ų�� ȣ��Ǵ� �Լ��̴�.                
 * ���� �α׸� ������ �ʱ� ���ؼ� Compaction�� ���� �ʴ´�.          
 * 1. Leaf Node�� ã�´�.
 * 2. ��� Leaf Node�� Ž���ϸ鼭 DelayedStamping�� �����Ѵ�.
 * 3. Node Aging�� �����Ѵ�.                                         
 *********************************************************************/
IDE_RC stndrRTree::aging( idvSQL            * aStatistics,
                          void              * aTrans,
                          smcTableHeader    * /* aHeader */,
                          smnIndexHeader    * aIndex )
{
    stndrHeader          * sIdxHdr = NULL;
    idBool                 sIsSuccess;
    stndrNodeHdr         * sNodeHdr = NULL;
    sdnCTS               * sCTS;
    sdnCTL               * sCTL;
    UInt                   i;
    smSCN                  sCommitSCN;
    UInt                   sState = 0;
    sdpSegCCache         * sSegCache;
    sdpSegHandle         * sSegHandle;
    sdpSegMgmtOp         * sSegMgmtOp;
    sdbMPRMgr              sMPRMgr;
    scPageID               sCurPID;
    sdpPhyPageHdr        * sPage;
    ULong                  sUsedSegSizeByBytes = 0;
    UInt                   sMetaSize = 0;
    UInt                   sFreeSize = 0;
    UInt                   sUsedSize = 0;
    sdpSegInfo             sSegInfo;

    
    IDE_DASSERT( aIndex  != NULL );
    
    sIdxHdr = (stndrHeader*)(aIndex->mHeader);

    IDE_TEST( smLayerCallback::rebuildMinViewSCN( aStatistics ) 
              != IDE_SUCCESS );

    sSegHandle = sdpSegDescMgr::getSegHandle(&(sIdxHdr->mSdnHeader.mSegmentDesc));
    sSegCache  = (sdpSegCCache*)sSegHandle->mCache;
    sSegMgmtOp = sdpSegDescMgr::getSegMgmtOp(&(sIdxHdr->mSdnHeader.mSegmentDesc));

    IDE_TEST( sSegMgmtOp->mGetSegInfo(
                             NULL,
                             sIdxHdr->mSdnHeader.mIndexTSID,
                             sdpSegDescMgr::getSegPID( &(sIdxHdr->mSdnHeader.mSegmentDesc) ),
                             NULL, /* aTableHeader */
                             &sSegInfo )
               != IDE_SUCCESS );

    IDE_TEST( sMPRMgr.initialize(
                  aStatistics,
                  sIdxHdr->mSdnHeader.mIndexTSID,
                  sdpSegDescMgr::getSegHandle(&(sIdxHdr->mSdnHeader.mSegmentDesc)),
                  NULL ) /*aFilter*/
              != IDE_SUCCESS );
    sState = 1;

    sCurPID = SM_NULL_PID;

    IDE_TEST( sMPRMgr.beforeFst() != IDE_SUCCESS );

    while( 1 )
    {
        IDE_TEST( sMPRMgr.getNxtPageID(NULL, /*aData*/
                                       &sCurPID)
                  != IDE_SUCCESS );

        if( sCurPID == SD_NULL_PID )
        {
            break;
        }

        IDE_TEST( sdbBufferMgr::getPage( aStatistics,
                                         sIdxHdr->mSdnHeader.mIndexTSID,
                                         sCurPID,
                                         SDB_S_LATCH,
                                         SDB_WAIT_NORMAL,
                                         SDB_MULTI_PAGE_READ,
                                         (UChar**)&sPage,
                                         &sIsSuccess )
                  != IDE_SUCCESS );
        sState = 2;

        IDE_DASSERT( (sPage->mPageType == SDP_PAGE_FORMAT) ||
                     (sPage->mPageType == SDP_PAGE_INDEX_RTREE) ||
                     (sPage->mPageType == SDP_PAGE_INDEX_META_RTREE) );

        if( (sPage->mPageType != SDP_PAGE_INDEX_RTREE) ||
            (sSegMgmtOp->mIsFreePage((UChar*)sPage) == ID_TRUE) )
        {
            sState = 1;
            IDE_TEST( sdbBufferMgr::releasePage( aStatistics,
                                                (UChar*)sPage )
                      != IDE_SUCCESS );
            continue;
        } 
        else 
        {   
            /* nothing to do */
        }

        sNodeHdr = (stndrNodeHdr*)sdpPhyPage::getLogicalHdrStartPtr(
                                                                (UChar*)sPage);
        
        if( sNodeHdr->mState != STNDR_IN_FREE_LIST )
        {
            /*
             * 1. Delayed CTL Stamping
             */
            if( STNDR_IS_LEAF_NODE(sNodeHdr) == ID_TRUE )
            {
                sCTL = sdnIndexCTL::getCTL( sPage );

                for( i = 0; i < sdnIndexCTL::getCount( sCTL ); i++ )
                {
                    sCTS = sdnIndexCTL::getCTS( sCTL, i );

                    if( sCTS->mState == SDN_CTS_UNCOMMITTED )
                    {
                        IDE_TEST( sdnIndexCTL::delayedStamping( aStatistics,
                                                                aTrans, 
                                                                sCTS,
                                                                SDB_MULTI_PAGE_READ,
                                                                SM_SCN_INIT, /* aStmtViewSCN */ 
                                                                &sCommitSCN,
                                                                &sIsSuccess )
                            
                                  != IDE_SUCCESS );
                    }
                }
            }
            else
            {
                /* nothing to do */
            }
            sMetaSize = ID_SIZEOF( sdpPhyPageHdr )
                        + idlOS::align8(ID_SIZEOF(stndrNodeHdr))
                        + sdpPhyPage::getSizeOfCTL( sPage )
                        + ID_SIZEOF( sdpPageFooter )
                        + ID_SIZEOF( sdpSlotDirHdr );

            sFreeSize = sNodeHdr->mTotalDeadKeySize + sPage->mTotalFreeSize;
            sUsedSize = SD_PAGE_SIZE - (sMetaSize + sFreeSize );
            sUsedSegSizeByBytes += sUsedSize;
            IDE_DASSERT ( sMetaSize + sFreeSize + sUsedSize == SD_PAGE_SIZE );
        }
        else
        {
            /* nothing to do */
        }

        sState = 1;
        IDE_TEST( sdbBufferMgr::releasePage( aStatistics,
                                             (UChar*)sPage )
                  != IDE_SUCCESS );
    }

    /* BUG-31372: ���׸�Ʈ �ǻ��� ������ ��ȸ�� ����� �ʿ��մϴ�. */
    sSegCache->mFreeSegSizeByBytes = (sSegInfo.mFmtPageCnt * SD_PAGE_SIZE) - sUsedSegSizeByBytes;

    sState = 0;
    IDE_TEST( sMPRMgr.destroy() != IDE_SUCCESS );

    // 2. Node Aging
    IDE_TEST( nodeAging( aStatistics,
                         aTrans,
                         &(sIdxHdr->mDMLStat),
                         sIdxHdr,
                         ID_UINT_MAX )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    switch( sState )
    {
        case 2:
            IDE_ASSERT( sdbBufferMgr::releasePage( aStatistics,
                                                   (UChar*)sPage )
                        == IDE_SUCCESS );
        case 1:
            IDE_ASSERT( sMPRMgr.destroy() == IDE_SUCCESS );
        default:
            break;
    }
    
    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : stndrRTree::NA
 * ------------------------------------------------------------------*
 * default function for invalid traversing protocol
 *********************************************************************/
IDE_RC stndrRTree::NA( void )
{
    IDE_SET( ideSetErrorCode( smERR_ABORT_smiTraverseNotApplicable ) );

    return IDE_FAILURE;
}
/*********************************************************************
 * FUNCTION DESCRIPTION : stndrRTree::getPosition
 * ------------------------------------------------------------------*
 * ���� Iterator�� ��ġ�� �����Ѵ�.
 *********************************************************************/
IDE_RC stndrRTree::getPositionNA( stndrIterator     * /*aIterator*/,
                                  smiCursorPosInfo  * /*aPosInfo*/ )
{
    /* Do nothing */
    return IDE_SUCCESS;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : stndrRTree::setPosition
 * ------------------------------------------------------------------*
 * ������ ����� Iterator�� ��ġ�� �ٽ� ���ͽ�Ų��.
 *********************************************************************/
IDE_RC stndrRTree::setPositionNA( stndrIterator     * /*aIterator*/,
                                  smiCursorPosInfo  * /*aPosInfo*/ )
{
    /* Do nothing */
    return IDE_SUCCESS;
}

IDE_RC stndrRTree::allocIterator( void ** /* aIteratorMem */ )
{
    return IDE_SUCCESS;
}

IDE_RC stndrRTree::freeIterator( void * /* aIteratorMem */ )
{
    return IDE_SUCCESS;
}

/*********************************************************************
 * FUNCTION DESCRIPTION :
 * ------------------------------------------------------------------*
 * �ε��� Runtime Header�� SmoNo�� ��´�. SmoNo�� ULong Ÿ���̱� ������
 * 32��Ʈ �ӽ��� ��� partial read�� ���ɼ��� �ִ�. ���� peterson �˰���
 * ���� SmoNo�� ��� �������Ѵ�.
 *
 * !!CAUTION!! : Runtime Header�� ���� �����Ͽ� SmoNo�� ȹ���ؼ��� �ȵȴ�.
 * �ݵ�� �� �Լ��� �̿��Ͽ� peterson �˰��� ������� SmoNo�� ȹ���ϵ���
 * �Ѵ�.
 *********************************************************************/
void stndrRTree::getSmoNo( void * aIndex, ULong * aSmoNo )
{
    stndrHeader * sIndex = (stndrHeader*)aIndex;
#ifndef COMPILE_64BIT
    UInt          sAtomicA;
    UInt          sAtomicB;
#endif

    IDE_ASSERT( aIndex != NULL );
    
#ifndef COMPILE_64BIT
    while( 1 )
    {
        ID_SERIAL_BEGIN( sAtomicA = sIndex->mSmoNoAtomicA );
        
        ID_SERIAL_EXEC( *aSmoNo = sIndex->mSdnHeader.mSmoNo, 1 );

        ID_SERIAL_END( sAtomicB = sIndex->mSmoNoAtomicB );

        if( sAtomicA == sAtomicB )
        {
            break;
        }

        idlOS::thr_yield();
    }
#else
    *aSmoNo = sIndex->mSdnHeader.mSmoNo;
#endif

    return;
}

/*********************************************************************
 * FUNCTION DESCRIPTION :
 * ------------------------------------------------------------------*
 * �ε��� Runtime Header�� SmoNo�� 1 ���� ��Ų��. �� �Լ��� ���ÿ� ȣ��� ��
 * �����Ƿ� Mutex�� �̿��Ͽ� ����ȭ�ϸ�, peterson �˰��� �̿��Ͽ� 32��Ʈ
 * �ӽſ��� partial read ���� �ʵ��� ���´�.
 *********************************************************************/
IDE_RC stndrRTree::increaseSmoNo( idvSQL        * aStatistics,
                                  stndrHeader   * aIndex,
                                  ULong         * aSmoNo )
{
    UInt sSerialValue = 0;
    
    IDE_TEST( aIndex->mSmoNoMutex.lock(aStatistics) != IDE_SUCCESS );
    ID_SERIAL_BEGIN( aIndex->mSmoNoAtomicB++ );

    ID_SERIAL_EXEC( aIndex->mSdnHeader.mSmoNo++, ++sSerialValue );
    ID_SERIAL_EXEC( *aSmoNo = aIndex->mSdnHeader.mSmoNo, ++sSerialValue );
    
    ID_SERIAL_END( aIndex->mSmoNoAtomicA++ );
    IDE_TEST( aIndex->mSmoNoMutex.unlock() != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/*********************************************************************
 * FUNCTION DESCRIPTION :
 * ------------------------------------------------------------------*
 * Virtual Root Node�� ��´�. Virtual Root Node�� ChildPID, ChildSmoNo
 * �� ������ ������, Virtual Root Node�� Child�� ���� Root Node�� ����Ų��.
 *********************************************************************/
void stndrRTree::getVirtualRootNode( stndrHeader            * aIndex,
                                     stndrVirtualRootNode   * aVRootNode )
{
    UInt    sSerialValue = 0;
    UInt    sAtomicA = 0;
    UInt    sAtomicB = 0;

    IDE_ASSERT( aIndex != NULL );
    IDE_ASSERT( aVRootNode != NULL );
    
    while( 1 )
    {
        sSerialValue = 0;
        
        ID_SERIAL_BEGIN( sAtomicA = aIndex->mVirtualRootNodeAtomicA );
        
        ID_SERIAL_EXEC( aVRootNode->mChildSmoNo = aIndex->mVirtualRootNode.mChildSmoNo,
                        ++sSerialValue );


        ID_SERIAL_EXEC( aVRootNode->mChildPID = aIndex->mVirtualRootNode.mChildPID,
                        ++sSerialValue );

        ID_SERIAL_END( sAtomicB = aIndex->mVirtualRootNodeAtomicB );

        if( sAtomicA == sAtomicB )
        {
            break;
        }

        idlOS::thr_yield();
    }

    return;
}

/*********************************************************************
 * FUNCTION DESCRIPTION :
 * ------------------------------------------------------------------*
 * Virtual Root Node�� �����Ѵ�.
 * !!!: �� �Լ��� ���ÿ� �� �����常�� ȣ���ؾ� �Ѵ�. ���� �׷��� �ʴٸ�
 *      Mutex�� ��ȣ�Ǿ�� �Ѵ�.
 *********************************************************************/
void stndrRTree::setVirtualRootNode( stndrHeader  * aIndex,
                                     scPageID       aRootNode,
                                     ULong          aSmoNo )
{
    UInt    sSerialValue = 0;

    ID_SERIAL_BEGIN( aIndex->mVirtualRootNodeAtomicB++ );

    ID_SERIAL_EXEC( aIndex->mVirtualRootNode.mChildPID = aRootNode,
                    ++sSerialValue );
    
    ID_SERIAL_EXEC( aIndex->mVirtualRootNode.mChildSmoNo = aSmoNo,
                    ++sSerialValue );

    ID_SERIAL_END( aIndex->mVirtualRootNodeAtomicA++ );

    return;
}

/*********************************************************************
 * FUNCTION DESCRIPTION :
 * ------------------------------------------------------------------*
 * row�� fetch �ϸ鼭 Index�� KeyValue�� �����Ѵ�.
 * Disk R-Tree�� ��� row�� stdGeometryHeader ���� �о �����Ѵ�.
 * insertKey, deleteKey���� stdGeometryHeader�� mMbr ����� KeyValue��
 * ����Ѵ�.
 *********************************************************************/
IDE_RC stndrRTree::makeKeyValueFromRow(
                            idvSQL                  * aStatistics,
                            sdrMtx                  * aMtx,
                            sdrSavePoint            * aSP,
                            void                    * aTrans,
                            void                    * aTableHeader,
                            const smnIndexHeader    * aIndex,
                            const UChar             * aRow,
                            sdbPageReadMode           aPageReadMode,
                            scSpaceID                 aTableSpaceID,
                            smFetchVersion            aFetchVersion,
                            sdSID                     aTSSlotSID,
                            const smSCN             * aSCN,
                            const smSCN             * aInfiniteSCN,
                            UChar                   * aDestBuf,
                            idBool                  * aIsRowDeleted,
                            idBool                  * aIsPageLatchReleased)
{
    stndrHeader         * sIndexHeader;
    sdcIndexInfo4Fetch    sIndexInfo4Fetch;
    ULong                 sValueBuffer[STNDR_MAX_KEY_BUFFER_SIZE/ID_SIZEOF(ULong)];

    IDE_DASSERT( aTrans           != NULL );
    IDE_DASSERT( aTableHeader     != NULL );
    IDE_DASSERT( aIndex           != NULL );
    IDE_DASSERT( aRow             != NULL );
    IDE_DASSERT( aDestBuf         != NULL );

    if( aIndex->mType != SMI_ADDITIONAL_RTREE_INDEXTYPE_ID )
    {
        ideLog::log( IDE_SERVER_0, "Index Header :\n" );
        ideLog::logMem( IDE_SERVER_0, (UChar *)aIndex, ID_SIZEOF(smnIndexHeader) );
        IDE_ASSERT( 0 );
    }

    sIndexHeader = ((stndrHeader*)aIndex->mHeader);

    /* sdcRow::fetch() �Լ��� �Ѱ��� row info �� callback �Լ� ���� */
    sIndexInfo4Fetch.mTableHeader           = aTableHeader;
    sIndexInfo4Fetch.mCallbackFunc4Index    = makeSmiValueListInFetch;
    sIndexInfo4Fetch.mBuffer                = (UChar*)sValueBuffer;
    sIndexInfo4Fetch.mBufferCursor          = (UChar*)sValueBuffer;
    sIndexInfo4Fetch.mFetchSize             = ID_SIZEOF(stdGeometryHeader);

    IDE_TEST( sdcRow::fetch(
                          aStatistics,
                          aMtx,
                          aSP,
                          aTrans,
                          aTableSpaceID,
                          (UChar*)aRow,
                          ID_TRUE, /* aIsPersSlot */
                          aPageReadMode,
                          &sIndexHeader->mFetchColumnListToMakeKey,
                          aFetchVersion,
                          aTSSlotSID,
                          aSCN,
                          aInfiniteSCN,
                          &sIndexInfo4Fetch,
                          NULL, /* aLobInfo4Fetch */
                          ((smcTableHeader*)aTableHeader)->mRowTemplate,
                          (UChar*)sValueBuffer,
                          aIsRowDeleted,
                          aIsPageLatchReleased)
                  != IDE_SUCCESS );

    if( *aIsRowDeleted == ID_FALSE )
    {
        makeKeyValueFromSmiValueList( aIndex, 
                                      sIndexInfo4Fetch.mValueList, 
                                      aDestBuf );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION :
 * ------------------------------------------------------------------*
 * sdcRow�� �Ѱ��� ������ �������� smiValue���¸� �ٽ� �����Ѵ�.
 * �ε��� Ű�� �ٽ� ����� smiValue�� �������� Ű�� �����ϰ� �ȴ�.
 * Disk R-Tree�� ��� mFetchSize( = sizeof(stdGeometryHeader) ) ����
 * �о �����Ѵ�.
 *********************************************************************/
IDE_RC stndrRTree::makeSmiValueListInFetch(
                       const smiColumn              * aIndexColumn,
                       UInt                           aCopyOffset,
                       const smiValue               * aColumnValue,
                       void                         * aIndexInfo )
{
    smiValue            * sValue;
    UShort                sColumnSeq;
    UInt                  sLength;
    sdcIndexInfo4Fetch  * sIndexInfo;

    IDE_DASSERT( aIndexColumn     != NULL );
    IDE_DASSERT( aColumnValue     != NULL );
    IDE_DASSERT( aIndexInfo       != NULL );

    sIndexInfo = (sdcIndexInfo4Fetch*)aIndexInfo;
    sColumnSeq = aIndexColumn->id & SMI_COLUMN_ID_MASK;
    sValue     = &sIndexInfo->mValueList[ sColumnSeq ];

    /* Proj-1872 Disk Index ���屸�� ����ȭ 
     * �� �Լ��� Ű ������ �Ҹ���, Ű ������ mFetchColumnListToMakeKey��
     * �̿��Ѵ�. mFetchColumnListToMakeKey�� column(aIndexColumn)�� VRow��
     * �����Ҷ��� �̿���� �ʱ� ������, �׻� Offset�� 0�̴�. */
    IDE_DASSERT( aIndexColumn->offset == 0 );
    if( sIndexInfo->mFetchSize <= 0 ) //FetchSize�� Rtree���� ����
    {
        ideLog::log( IDE_SERVER_0, "Index info:\n" );
        ideLog::logMem( IDE_SERVER_0,
                        (UChar *)sIndexInfo,
                        ID_SIZEOF(sdcIndexInfo4Fetch) );
        IDE_ASSERT( 0 );
    }

    if( aCopyOffset == 0 ) //first col-piece
    {
        sValue->length = aColumnValue->length;
        if( sValue->length > sIndexInfo->mFetchSize )
        {
            sValue->length = sIndexInfo->mFetchSize;
        }
        
        sValue->value = sIndexInfo->mBufferCursor;
        
        sLength = sValue->length;
    }
    else //first col-piece�� �ƴ� ���
    {
        if( (sValue->length + aColumnValue->length) > sIndexInfo->mFetchSize )
        {
            sLength = sIndexInfo->mFetchSize - sValue->length;
        }
        else
        {
            sLength = aColumnValue->length;
        }
        
        sValue->length += sLength;
    }

    if( 0 < sLength ) //NULL�� ��� length�� 0
    {
        ID_WRITE_AND_MOVE_DEST( sIndexInfo->mBufferCursor, 
                                aColumnValue->value, 
                                sLength );
    }

    return IDE_SUCCESS;
}

/*********************************************************************
 * FUNCTION DESCRIPTION :
 * ------------------------------------------------------------------*
 * smiValue list�� ������ Key Value(index���� ����ϴ� ��)�� �����.
 * insert DML�ÿ� �� �Լ��� ȣ���Ѵ�.
 * Disk R-Tree�� ��� stdGeometryHeader�� KeyValue�� �����Ѵ�.
 *********************************************************************/
IDE_RC stndrRTree::makeKeyValueFromSmiValueList( const smnIndexHeader   * aIndex,
                                                 const smiValue         * aValueList,
                                                 UChar                  * aDestBuf )
{
    smiValue            * sIndexValue;
    UShort                sColumnSeq;
    UShort                sColCount = 0;
    UChar               * sBufferCursor;
    smiColumn           * sKeyColumn;
    stndrColumn         * sIndexColumn;
    stdGeometryHeader   * sGeometryHeader;


    IDE_DASSERT( aIndex     != NULL );
    IDE_DASSERT( aValueList != NULL );
    IDE_DASSERT( aDestBuf   != NULL );

    sBufferCursor = aDestBuf;
    sColCount     = aIndex->mColumnCount;

    IDE_ASSERT( sColCount == 1 );

    sColumnSeq   = aIndex->mColumns[0] & SMI_COLUMN_ID_MASK;
    sIndexValue  = (smiValue*)(aValueList + sColumnSeq);
        
    sIndexColumn = &((stndrHeader*)aIndex->mHeader)->mColumn;
    sKeyColumn   = &sIndexColumn->mKeyColumn;

    if( sColumnSeq != (sKeyColumn->id % SMI_COLUMN_ID_MAXIMUM) )
    {
        ideLog::log( IDE_SERVER_0,
                     "Column sequence : %u"
                     ", Column id : %u"
                     "\nColumn information :\n",
                     sColumnSeq, sKeyColumn->id );
        ideLog::logMem( IDE_SERVER_0,
                        (UChar *)sKeyColumn,
                        ID_SIZEOF(smiColumn) );
        IDE_ASSERT( 0 );
    }

    if( sIndexValue->length == 0 ) // NULL
    {
        (void)stdUtils::nullify( (stdGeometryHeader*)sBufferCursor );
    }
    else
    {
        IDE_ASSERT( sIndexValue->length >= ID_SIZEOF(stdGeometryHeader) );

        sGeometryHeader = (stdGeometryHeader*)sIndexValue->value;

        ID_WRITE_AND_MOVE_DEST( sBufferCursor,
                                sGeometryHeader,
                                ID_SIZEOF(stdGeometryHeader) );
    }
    
    return IDE_SUCCESS;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : stndrRTree::allocCTS
 * ------------------------------------------------------------------*
 * CTS�� �Ҵ��Ѵ�. 
 * ������ ������ ��쿡�� Compaction���Ŀ� Ȯ�嵵 ����Ѵ�. 
 *********************************************************************/
IDE_RC stndrRTree::allocCTS( idvSQL             * aStatistics,
                             stndrHeader        * aIndex,
                             sdrMtx             * aMtx,
                             sdpPhyPageHdr      * aNode,
                             UChar              * aCTSlotNum,
                             sdnCallbackFuncs   * aCallbackFunc,
                             UChar              * aContext,
                             SShort             * aKeySeq )
{
    UShort            sNeedSize;
    UShort            sFreeSize;
    stndrNodeHdr    * sNodeHdr;
    idBool            sSuccess;

    
    IDE_TEST( sdnIndexCTL::allocCTS(aStatistics,
                                    aMtx,
                                    &aIndex->mSdnHeader.mSegmentDesc.mSegHandle,
                                    aNode,
                                    aCTSlotNum,
                                    aCallbackFunc,
                                    aContext)
              != IDE_SUCCESS );

    if( *aCTSlotNum == SDN_CTS_INFINITE )
    {
        sNodeHdr  = (stndrNodeHdr*)
            sdpPhyPage::getLogicalHdrStartPtr( (UChar*)aNode );

        sNeedSize = ID_SIZEOF( sdnCTS );
        
        sFreeSize = getTotalFreeSize( aIndex, aNode );
        sFreeSize += sNodeHdr->mTotalDeadKeySize;

        if( sNeedSize <= sFreeSize )
        {
            // Non-Fragment Free Space�� �ʿ��� ������� ���� ����
            // Compaction���� �ʴ´�.
            if( sNeedSize > getNonFragFreeSize(aIndex, aNode) )
            {
                if( aKeySeq != NULL )
                {
                    adjustKeyPosition( aNode, aKeySeq );
                }
            
                IDE_TEST( compactPage( aMtx,
                                       aNode,
                                       ID_TRUE ) != IDE_SUCCESS );
            }

            *aCTSlotNum = sdnIndexCTL::getCount( aNode );

            IDE_TEST( sdnIndexCTL::extend(
                          aMtx,
                          &aIndex->mSdnHeader.mSegmentDesc.mSegHandle,
                          aNode,
                          ID_TRUE, //aLogging
                          &sSuccess)
                      != IDE_SUCCESS );

            if( sSuccess == ID_FALSE )
            {
                *aCTSlotNum = SDN_CTS_INFINITE;
            }
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : stndrRTree::allocPage
 * ------------------------------------------------------------------*
 * Free Node List�� Free�������� �����Ѵٸ� Free Node List�� ���� ��
 * �������� �Ҵ� �ް�, ���� �׷��� �ʴٸ� Physical Layer�� ���� �Ҵ�
 * �޴´�.
 *********************************************************************/
IDE_RC stndrRTree::allocPage( idvSQL            * aStatistics,
                              stndrStatistic    * aIndexStat,
                              stndrHeader       * aIndex,
                              sdrMtx            * aMtx,
                              UChar            ** aNewNode )
{
    idBool            sIsSuccess;
    UChar           * sMetaPage;
    stndrMeta       * sMeta;
    stndrNodeHdr    * sNodeHdr;
    smSCN             sSysMinDskViewSCN;
    sdrSavePoint      sSP;

    if( aIndex->mFreeNodeCnt > 0 )
    {
        sMetaPage = sdrMiniTrans::getPagePtrFromPageID(
            aMtx,
            aIndex->mSdnHeader.mIndexTSID,
            SD_MAKE_PID( aIndex->mSdnHeader.mMetaRID ));
    
        if( sMetaPage == NULL )
        {
            IDE_TEST( stndrRTree::getPage( aStatistics,
                                           &(aIndexStat->mMetaPage),
                                           aIndex->mSdnHeader.mIndexTSID,
                                           SD_MAKE_PID( aIndex->mSdnHeader.mMetaRID ),
                                           SDB_X_LATCH,
                                           SDB_WAIT_NORMAL,
                                           aMtx,
                                           (UChar**)&sMetaPage,
                                           &sIsSuccess )
                      != IDE_SUCCESS );
        }

        sMeta = (stndrMeta*)( sMetaPage + SMN_INDEX_META_OFFSET );

        IDE_ASSERT( sMeta->mFreeNodeHead != SD_NULL_PID );
        IDE_ASSERT( sMeta->mFreeNodeCnt == aIndex->mFreeNodeCnt );
        
        sdrMiniTrans::setSavePoint( aMtx, &sSP );
        
        IDE_TEST( stndrRTree::getPage( aStatistics,
                                       &(aIndexStat->mIndexPage),
                                       aIndex->mSdnHeader.mIndexTSID,
                                       sMeta->mFreeNodeHead,
                                       SDB_X_LATCH,
                                       SDB_WAIT_NORMAL,
                                       aMtx,
                                       (UChar**)aNewNode,
                                       &sIsSuccess )
                  != IDE_SUCCESS );
        
        sNodeHdr = (stndrNodeHdr*)
            sdpPhyPage::getLogicalHdrStartPtr((UChar*)*aNewNode);

        /*
         * Leaf Node�� ���� �ݵ�� FREE LIST�� ����Ǿ� �ִ�
         * ���¿��� �Ѵ�.
         */
        if( ( STNDR_IS_LEAF_NODE(sNodeHdr) == ID_TRUE ) &&
            ( sNodeHdr->mState != STNDR_IN_FREE_LIST ) )
        {
            ideLog::log( IDE_SERVER_0,
                         "Index TSID : %u"
                         ", Get page ID : %u\n",
                         aIndex->mSdnHeader.mIndexTSID,
                         sMeta->mFreeNodeHead );
            dumpIndexNode( (sdpPhyPageHdr *)*aNewNode );
            IDE_ASSERT( 0 );
        }
        
        // SCN ��
        SMX_GET_MIN_DISK_VIEW( &sSysMinDskViewSCN );
        
        if( SM_SCN_IS_LT(&sNodeHdr->mFreeNodeSCN, &sSysMinDskViewSCN) )
        {
            aIndex->mFreeNodeCnt--;
            aIndex->mFreeNodeHead = sNodeHdr->mNextFreeNode;
            aIndex->mFreeNodeSCN  = sNodeHdr->mNextFreeNodeSCN;
        
            IDE_TEST( setFreeNodeInfo(aStatistics,
                                      aIndex,
                                      aIndexStat,
                                      aMtx,
                                      sNodeHdr->mNextFreeNode,
                                      aIndex->mFreeNodeCnt,
                                      &aIndex->mFreeNodeSCN)
                      != IDE_SUCCESS );

            IDE_TEST( sdpPhyPage::reset((sdpPhyPageHdr*)*aNewNode,
                                        ID_SIZEOF( stndrNodeHdr ),
                                        aMtx)
                      != IDE_SUCCESS );

            IDE_TEST( sdpSlotDirectory::logAndInit((sdpPhyPageHdr*)*aNewNode,
                                                   aMtx)
                      != IDE_SUCCESS );

            IDE_RAISE( SKIP_ALLOC_NEW_PAGE );
        }
        else
        {
            sdrMiniTrans::releaseLatchToSP( aMtx, &sSP );
        }
    }

    // alloc page
    IDE_ASSERT( sdpSegDescMgr::getSegMgmtOp(
                    &(aIndex->mSdnHeader.mSegmentDesc) )->mAllocNewPage(
                        aStatistics,
                        aMtx,
                        aIndex->mSdnHeader.mIndexTSID,
                        &(aIndex->mSdnHeader.mSegmentDesc.mSegHandle),
                        SDP_PAGE_INDEX_RTREE,
                        (UChar**)aNewNode)
                == IDE_SUCCESS );

    IDE_TEST( sdpPhyPage::logAndInitLogicalHdr(
                  (sdpPhyPageHdr*)*aNewNode,
                  ID_SIZEOF(stndrNodeHdr),
                  aMtx,
                  (UChar**)&sNodeHdr ) != IDE_SUCCESS );

    IDE_TEST( sdpSlotDirectory::logAndInit((sdpPhyPageHdr*)*aNewNode,
                                           aMtx)
              != IDE_SUCCESS );

    IDE_EXCEPTION_CONT( SKIP_ALLOC_NEW_PAGE );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : stndrRTree::getPage
 * ------------------------------------------------------------------*
 * To fix BUG-18252
 * �ε��� �������� ��Ÿ�������� ���� �󵵿� ���� ������� ����
 *********************************************************************/
IDE_RC stndrRTree::getPage( idvSQL          * aStatistics,
                            stndrPageStat   * aPageStat,
                            scSpaceID         aSpaceID,
                            scPageID          aPageID,
                            sdbLatchMode      aLatchMode,
                            sdbWaitMode       aWaitMode,
                            void            * aMtx,
                            UChar          ** aRetPagePtr,
                            idBool          * aTrySuccess )
{
    idvSQL      * sStatistics;
    idvSession    sDummySession;
    idvSQL        sDummySQL;
    ULong         sGetPageCount;
    ULong         sReadPageCount;
    

    sDummySQL.mGetPageCount = 0;
    sDummySQL.mReadPageCount = 0;

    if( aStatistics == NULL )
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

    sGetPageCount = sStatistics->mGetPageCount;
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

    aPageStat->mGetPageCount += sStatistics->mGetPageCount - sGetPageCount;
    aPageStat->mReadPageCount += sStatistics->mReadPageCount - sReadPageCount;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : stndrRTree::freePage
 * ------------------------------------------------------------------*
 * Free�� ��带 Free Node List�� �����Ѵ�.
 *********************************************************************/
IDE_RC stndrRTree::freePage( idvSQL         * aStatistics,
                             stndrStatistic * aIndexStat,
                             stndrHeader    * aIndex,
                             sdrMtx         * aMtx,
                             UChar          * aFreePage )
{
    idBool            sIsSuccess;
    UChar           * sMetaPage;
    stndrMeta       * sMeta;
    stndrNodeHdr    * sNodeHdr;
    smSCN             sNextFreeNodeSCN;
    UChar             sNodeState = STNDR_IN_FREE_LIST;
    
    sMetaPage = sdrMiniTrans::getPagePtrFromPageID(
                                aMtx,
                                aIndex->mSdnHeader.mIndexTSID,
                                SD_MAKE_PID( aIndex->mSdnHeader.mMetaRID ));
                            
    if( sMetaPage == NULL )
    {
        IDE_TEST( stndrRTree::getPage( aStatistics,
                                       &(aIndexStat->mMetaPage),
                                       aIndex->mSdnHeader.mIndexTSID,
                                       SD_MAKE_PID(aIndex->mSdnHeader.mMetaRID),
                                       SDB_X_LATCH,
                                       SDB_WAIT_NORMAL,
                                       aMtx,
                                       (UChar**)&sMetaPage,
                                       &sIsSuccess )
                  != IDE_SUCCESS );
    }

    sMeta = (stndrMeta*)( sMetaPage + SMN_INDEX_META_OFFSET );

    IDE_ASSERT( sMeta->mFreeNodeCnt == aIndex->mFreeNodeCnt );

    sNextFreeNodeSCN = aIndex->mFreeNodeSCN;
    
    aIndex->mFreeNodeCnt++;
    aIndex->mFreeNodeHead = sdpPhyPage::getPageID( aFreePage );
    SMX_GET_SYSTEM_VIEW_SCN( &aIndex->mFreeNodeSCN );
        
    sNodeHdr = (stndrNodeHdr*)
                    sdpPhyPage::getLogicalHdrStartPtr( (UChar*)aFreePage );

    IDE_TEST( sdrMiniTrans::writeNBytes( aMtx,
                                         (UChar*)&sNodeHdr->mFreeNodeSCN,
                                         (void*)&aIndex->mFreeNodeSCN,
                                         ID_SIZEOF(aIndex->mFreeNodeSCN) )
              != IDE_SUCCESS );

    IDE_TEST( sdrMiniTrans::writeNBytes( aMtx,
                                         (UChar*)&sNodeHdr->mNextFreeNodeSCN,
                                         (void*)&sNextFreeNodeSCN,
                                         ID_SIZEOF(sNextFreeNodeSCN) )
              != IDE_SUCCESS );

    IDE_TEST( sdrMiniTrans::writeNBytes( aMtx,
                                         (UChar*)&sNodeHdr->mNextFreeNode,
                                         (void*)&sMeta->mFreeNodeHead,
                                         ID_SIZEOF(sMeta->mFreeNodeHead) )
              != IDE_SUCCESS );

    // Internal Node�� Free List�� �޸� ���� �����Ƿ� freePage �ÿ�
    // ���¸� STNDR_FREE_LIST�� �����Ѵ�.
    IDE_TEST( sdrMiniTrans::writeNBytes( aMtx,
                                         (UChar*)&sNodeHdr->mState,
                                         (void*)&sNodeState,
                                         ID_SIZEOF(sNodeState) )
              != IDE_SUCCESS );

    IDE_TEST( setFreeNodeInfo( aStatistics,
                               aIndex,
                               aIndexStat,
                               aMtx,
                               aIndex->mFreeNodeHead,
                               aIndex->mFreeNodeCnt,
                               &aIndex->mFreeNodeSCN )
              != IDE_SUCCESS );
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION :                                            *
 * ------------------------------------------------------------------*
 * Index node�� Header(stndrNodeHdr)�� �ʱ�ȭ�Ѵ�. �� Index node�� 
 * �Ҵ�ް� �Ǹ�, �ݵ�� �� �Լ��� ���Ͽ� Node header�� �ʱ�ȭ�ϰ�
 * �Ǹ�, Logging option�� ����, logging/no-logging�� �����Ѵ�.
 *********************************************************************/
IDE_RC stndrRTree::initializeNodeHdr( sdrMtx        * aMtx,
                                      sdpPhyPageHdr * aNode,
                                      UShort          aHeight,
                                      idBool          aIsLogging )
{
    stndrNodeHdr    * sNodeHdr;
    UShort            sKeyCount  = 0;
    UChar             sNodeState = STNDR_IN_USED;
    UShort            sTotalDeadKeySize = 0;
    scPageID          sEmptyPID = SD_NULL_PID;
    UShort            sTBKCount = 0;
    UShort            sRefTBK[STNDR_TBK_MAX_CACHE];
    UInt              i;
    smSCN             sSCN;

    
    SM_INIT_SCN( &sSCN );

    sNodeHdr = (stndrNodeHdr*)sdpPhyPage::getLogicalHdrStartPtr((UChar*)aNode);

    for( i = 0; i < STNDR_TBK_MAX_CACHE; i++ )
    {
        sRefTBK[i] = STNDR_TBK_CACHE_NULL;
    }
    
    if(aIsLogging == ID_TRUE)
    {

        IDE_TEST( sdrMiniTrans::writeNBytes( aMtx,
                                             (UChar*)&sNodeHdr->mNextEmptyNode,
                                             (void*)&sEmptyPID,
                                             ID_SIZEOF(sNodeHdr->mNextEmptyNode) )
                  != IDE_SUCCESS );

        IDE_TEST( sdrMiniTrans::writeNBytes( aMtx,
                                             (UChar*)&sNodeHdr->mNextFreeNode,
                                             (void*)&sEmptyPID,
                                             ID_SIZEOF(sNodeHdr->mNextFreeNode) )
                  != IDE_SUCCESS );
        
        IDE_TEST( sdrMiniTrans::writeNBytes( aMtx,
                                             (UChar*)&sNodeHdr->mFreeNodeSCN,
                                             (void*)&sSCN,
                                             ID_SIZEOF(sNodeHdr->mFreeNodeSCN) )
                  != IDE_SUCCESS );
        
        IDE_TEST( sdrMiniTrans::writeNBytes( aMtx,
                                             (UChar*)&sNodeHdr->mNextFreeNodeSCN,
                                             (void*)&sSCN,
                                             ID_SIZEOF(sNodeHdr->mNextFreeNodeSCN) )
                  != IDE_SUCCESS );
        
        IDE_TEST( sdrMiniTrans::writeNBytes(aMtx,
                                            (UChar*)&sNodeHdr->mHeight,
                                            (void*)&aHeight,
                                            ID_SIZEOF(aHeight))
                  != IDE_SUCCESS );

        IDE_TEST( sdrMiniTrans::writeNBytes(aMtx,
                                            (UChar*)&sNodeHdr->mUnlimitedKeyCount,
                                            (void*)&sKeyCount,
                                            ID_SIZEOF(sKeyCount))
                  != IDE_SUCCESS );
        
        IDE_TEST( sdrMiniTrans::writeNBytes(aMtx,
                                            (UChar*)&sNodeHdr->mTotalDeadKeySize,
                                            (void*)&sTotalDeadKeySize,
                                            ID_SIZEOF(sTotalDeadKeySize))
                  != IDE_SUCCESS );

        IDE_TEST( sdrMiniTrans::writeNBytes(aMtx,
                                            (UChar*)&sNodeHdr->mTBKCount,
                                            (void*)&sTBKCount,
                                            ID_SIZEOF(sTBKCount))
                  != IDE_SUCCESS );

        IDE_TEST( sdrMiniTrans::writeNBytes(aMtx,
                                            (UChar*)&sNodeHdr->mRefTBK,
                                            (void*)&sRefTBK,
                                            ID_SIZEOF(UShort)*STNDR_TBK_MAX_CACHE)
                  != IDE_SUCCESS );

        IDE_TEST( sdrMiniTrans::writeNBytes(aMtx,
                                            (UChar*)&sNodeHdr->mState,
                                            (void*)&sNodeState,
                                            ID_SIZEOF(sNodeState))
                  != IDE_SUCCESS );
    }
    else
    {
        sNodeHdr->mNextEmptyNode     = sEmptyPID;
        sNodeHdr->mNextEmptyNode     = sEmptyPID;
        sNodeHdr->mFreeNodeSCN       = sSCN;
        sNodeHdr->mNextFreeNodeSCN   = sSCN;
        sNodeHdr->mHeight            = aHeight;
        sNodeHdr->mUnlimitedKeyCount = sKeyCount;
        sNodeHdr->mTotalDeadKeySize  = sTotalDeadKeySize;
        sNodeHdr->mTBKCount          = sTBKCount;
        sNodeHdr->mState             = sNodeState;
        
        idlOS::memcpy( sNodeHdr->mRefTBK,
                       sRefTBK,
                       ID_SIZEOF(UShort)*STNDR_TBK_MAX_CACHE );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : stndrRTree::makeNewootNode
 * ------------------------------------------------------------------*
 * ���ο� node�� �Ҵ��Ͽ� root ���� �����.
 * ���� ������ Root�� Leaf Node�� �ƴ� ��� Split�� �� ��带 ����Ű��
 * �� ���� Ű�� �ö�´�.
 * !!CAUTION!!: Runtime Header�� Root Node ��ȣ�� ���� �Ŀ�
 * SmoNo�� ���� ��Ų��. Traverse ���� SmoNo�� ���� ��� Root Node�� ����
 * �����̴�.
 *********************************************************************/
IDE_RC stndrRTree::makeNewRootNode( idvSQL          * aStatistics,
                                    stndrStatistic  * aIndexStat,
                                    sdrMtx          * aMtx,
                                    idBool          * aMtxStart,
                                    stndrHeader     * aIndex,
                                    smSCN           * aInfiniteSCN,
                                    stndrKeyInfo    * aLeftKeyInfo,
                                    sdpPhyPageHdr   * aLeftNode,
                                    stndrKeyInfo    * aRightKeyInfo,
                                    UShort            aKeyValueLen,
                                    sdpPhyPageHdr  ** aNewChildPage,
                                    UInt            * aAllocPageCount )
{
    sdpPhyPageHdr         * sNewRootPage;
    stndrNodeHdr          * sNodeHdr;
    UShort                  sHeight;
    scPageID                sNewRootPID;
    scPageID                sChildPID;
    UShort                  sKeyLen;
    UShort                  sAllowedSize;
    scOffset                sSlotOffset;
    stndrIKey             * sIKey;
    SShort                  sKeySeq = 0;
    idBool                  sIsSuccess = ID_FALSE;
    ULong                   sSmoNo;
    stdMBR                  sNodeMBR;
    IDE_RC                  sRc;
    UChar                   sCTSlotNum = SDN_CTS_INFINITE;
    stndrCallbackContext    sContext;

    
    // set height
    if( aLeftNode == NULL )
    {
        sHeight = 0;
    }
    else
    {
        sNodeHdr = (stndrNodeHdr*)
            sdpPhyPage::getLogicalHdrStartPtr( (UChar*)aLeftNode );
        sHeight = sNodeHdr->mHeight + 1;
    }
    
    // SMO�� �ֻ�� -- allocPages
    // allocate new pages, stack tmp depth + 2 ��ŭ
    IDE_TEST( preparePages( aStatistics,
                            aIndex,
                            aMtx,
                            aMtxStart,
                            sHeight + 1 )
              != IDE_SUCCESS );

    // init root node
    IDE_ASSERT( allocPage( aStatistics,
                           aIndexStat,
                           aIndex,
                           aMtx,
                           (UChar**)&sNewRootPage )
                == IDE_SUCCESS );
    (*aAllocPageCount)++;

    IDE_ASSERT( ((vULong)sNewRootPage % SD_PAGE_SIZE) == 0 );

    sNewRootPID = sdpPhyPage::getPageID( (UChar*)sNewRootPage );

    IDE_TEST( initializeNodeHdr(aMtx,
                                sNewRootPage,
                                sHeight,
                                ID_TRUE)
              != IDE_SUCCESS );
                                 
    if( sHeight == 0 ) // �� root�� leaf node
    {
        IDE_ASSERT( aLeftKeyInfo != NULL && aRightKeyInfo == NULL );

        if( aIndex->mRootNode != SD_NULL_PID )
        {
            ideLog::log( IDE_SERVER_0, "Index header:\n" );
            ideLog::logMem( IDE_SERVER_0,
                            (UChar *)aIndex,
                            ID_SIZEOF(stndrHeader) );
            IDE_ASSERT( 0 );
        }
        
        IDE_TEST( sdnIndexCTL::init(
                      aMtx,
                      &aIndex->mSdnHeader.mSegmentDesc.mSegHandle,
                      sNewRootPage,
                      0)
                  != IDE_SUCCESS );
        
        *aNewChildPage = sNewRootPage;
        sKeySeq = 0;

        IDE_TEST( canInsertKey( aStatistics,
                                aMtx,
                                aIndex,
                                aIndexStat,
                                aLeftKeyInfo,
                                sNewRootPage,
                                &sKeySeq,
                                &sCTSlotNum,
                                &sContext )
                  != IDE_SUCCESS );

        IDE_TEST( insertKeyIntoLeafNode( aMtx,
                                         aIndex,
                                         aInfiniteSCN,
                                         sNewRootPage,
                                         &sKeySeq,
                                         aLeftKeyInfo,
                                         sCTSlotNum,
                                         &sIsSuccess )
                  != IDE_SUCCESS );
        
        IDE_ASSERT( sIsSuccess == ID_TRUE );
    }
    else
    {
        IDE_ASSERT( aLeftKeyInfo != NULL && aRightKeyInfo != NULL );
        
        // �� child node�� PID�� �� root�� �� slot�� child PID
        IDE_ASSERT( allocPage(aStatistics,
                              aIndexStat,
                              aIndex,
                              aMtx,
                              (UChar**)aNewChildPage) == IDE_SUCCESS );
        (*aAllocPageCount)++;

        // insert left key
        sChildPID = sdpPhyPage::getPageID( (UChar*)aLeftNode );
        sKeyLen   = STNDR_IKEY_LEN( aKeyValueLen );
        sKeySeq   = 0;

        IDE_TEST( sdpPhyPage::allocSlot( (sdpPhyPageHdr*)sNewRootPage,
                                         sKeySeq, // slot number
                                         sKeyLen,
                                         ID_TRUE,
                                         &sAllowedSize,
                                         (UChar**)&sIKey,
                                         &sSlotOffset,
                                         1 )
                  != IDE_SUCCESS );

        STNDR_KEYINFO_TO_IKEY( (*aLeftKeyInfo),
                               sChildPID,
                               aKeyValueLen,
                               sIKey );

        if( aIndex->mSdnHeader.mLogging == ID_TRUE )
        {
            IDE_TEST( sdrMiniTrans::writeLogRec( aMtx,
                                                 (UChar*)sIKey,
                                                 (void*)NULL,
                                                 ID_SIZEOF(sKeySeq)+sKeyLen,
                                                 SDR_STNDR_INSERT_INDEX_KEY )
                      != IDE_SUCCESS );

            IDE_TEST( sdrMiniTrans::write( aMtx,
                                           (void*)&sKeySeq,
                                           ID_SIZEOF(sKeySeq) )
                      != IDE_SUCCESS );

            IDE_TEST( sdrMiniTrans::write( aMtx,
                                           (void*)sIKey,
                                           sKeyLen )
                      != IDE_SUCCESS );
        }

        // insert right key
        sChildPID = sdpPhyPage::getPageID( (UChar*)*aNewChildPage );
        sKeyLen   = STNDR_IKEY_LEN( aKeyValueLen );
        sKeySeq   = 1;

        IDE_TEST( sdpPhyPage::allocSlot( (sdpPhyPageHdr*)sNewRootPage,
                                         sKeySeq, // slot number
                                         sKeyLen,
                                         ID_TRUE,
                                         &sAllowedSize,
                                         (UChar**)&sIKey,
                                         &sSlotOffset,
                                         1 )
                  != IDE_SUCCESS );

        STNDR_KEYINFO_TO_IKEY( (*aRightKeyInfo),
                               sChildPID,
                               aKeyValueLen,
                               sIKey );

        if( aIndex->mSdnHeader.mLogging == ID_TRUE )
        {
            IDE_TEST( sdrMiniTrans::writeLogRec(aMtx,
                                                (UChar*)sIKey,
                                                (void*)NULL,
                                                ID_SIZEOF(sKeySeq)+sKeyLen,
                                                SDR_STNDR_INSERT_INDEX_KEY )
                      != IDE_SUCCESS );

            IDE_TEST( sdrMiniTrans::write(aMtx,
                                          (void*)&sKeySeq,
                                          ID_SIZEOF(sKeySeq) )
                      != IDE_SUCCESS );

            IDE_TEST( sdrMiniTrans::write(aMtx,
                                          (void*)sIKey,
                                          sKeyLen)
                      != IDE_SUCCESS );
        }
        
    } // if( sHeight == 0 )

    // update node MBR
    IDE_ASSERT( adjustNodeMBR( aIndex,
                               sNewRootPage,
                               NULL,                  /* aInsertMBR */
                               STNDR_INVALID_KEY_SEQ, /* aUpdateKeySeq */
                               NULL,                  /* aUpdateMBR */
                               STNDR_INVALID_KEY_SEQ, /* aDeleteKeySeq */
                               &sNodeMBR )
                == IDE_SUCCESS );
        
    sRc = setNodeMBR( aMtx,
                      sNewRootPage,
                      &sNodeMBR );

    if( sRc != IDE_SUCCESS )
    {
        ideLog::log( IDE_SERVER_0, "update MBR : \n" );
        ideLog::logMem( IDE_SERVER_0,
                        (UChar*)&sNodeMBR,
                        ID_SIZEOF(stdMBR) );
        dumpIndexNode( sNewRootPage );
        IDE_ASSERT( 0 );
    }

    IDE_TEST( backupRuntimeHeader( aMtx, aIndex ) != IDE_SUCCESS );

    // update Tree MBR
    // : Tree MBR�� ���ü� ������,Root Node�� Latch��
    // ���� ���¿��� ������Ʈ �ϵ��� �Ѵ�.
    if( aIndex->mInitTreeMBR == ID_TRUE )
    {
        aIndex->mTreeMBR = sNodeMBR;
    }
    else
    {
        aIndex->mTreeMBR = sNodeMBR;
        aIndex->mInitTreeMBR = ID_TRUE;
    }

    // RootNode�� RootNode�� SmoNo�� �����Ѵ�.
    aIndex->mRootNode = sNewRootPID;
    
    IDE_TEST( increaseSmoNo( aStatistics,
                             aIndex,
                             &sSmoNo )
              != IDE_SUCCESS );

    sdpPhyPage::setIndexSMONo( (UChar*)sNewRootPage, sSmoNo );

    // Virtual Root Node�� �����Ѵ�.
    setVirtualRootNode( aIndex, sNewRootPID, sSmoNo );

    IDE_TEST( setIndexMetaInfo( aStatistics,
                                aIndex,
                                aIndexStat,
                                aMtx,
                                &sNewRootPID,
                                NULL,
                                NULL,
                                NULL,
                                NULL,
                                NULL,
                                NULL,
                                NULL ) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION :                                            *
 * ------------------------------------------------------------------*
 * index�� Key�� insert �Ѵ�.
 *
 * ���� Ű�� �����ϱ� ���� Leaf Node�� Ž���Ѵ�. Ž�� ����� Key ��������
 * ���� ���� Ȯ���� �ּ�ȭ �Ǵ� ��带 �����ϴ� ���̴�. Leaf Node�� ã�� ����
 * Ž���� �ϳ��� ��忡 ���ؼ��� S-Latch�� ��� Optimistic �ϰ� ����Ǹ�,
 * Leaf Node�� ���ؼ��� X-Latch�� ��´�.
 *
 * Leaf Node�� Key�� ������ ������ ������ Ű�� �����Ѵ�. Ű �������� ���� ���
 * MBR�� ����� ��� �̸� ���� �θ��忡 Propagation�Ѵ�. �̶� Leaf Node��
 * ���� ����� ���� �θ� ��忡���� X-Latch�� ���Ѵ�.
 *
 * Leaf Node�� Key�� ������ ������ ������ Split�� �����Ѵ�. Split ���� ��
 * ��ȯ�� ��忡 Key�� �����Ѵ�. Split �� Empty Node�� ������ ��� �̸�
 * Empty List�� �����Ѵ�.
 *
 * insertKey�� ���ο� ��带 �Ҵ� �޾��� ��쿡 ���ؼ��� Node Aging�� ����
 * �Ѵ�.
 *********************************************************************/
IDE_RC stndrRTree::insertKey( idvSQL    * aStatistics,
                              void      * aTrans,
                              void      * /* aTable */,
                              void      * aIndex,
                              smSCN       aInfiniteSCN,
                              SChar     * aKeyValue,
                              SChar     * /* aNullRow */,
                              idBool      /* aUniqueCheck */,
                              smSCN       aStmtViewSCN,
                              void      * aRowSID,
                              SChar    ** /* aExistUniqueRow */,
                              ULong       /* aInsertWaitTime */,
                              idBool      /* aForbiddenToRetry */ )
{
    sdrMtx                  sMtx;
    stndrPathStack          sStack;
    sdpPhyPageHdr         * sNewChildPage;
    UShort                  sKeyValueLength;
    sdpPhyPageHdr         * sLeafNode;
    SShort                  sLeafKeySeq;
    stndrNodeHdr          * sNodeHdr;
    idBool                  sIsSuccess;
    stndrKeyInfo            sKeyInfo;
    idBool                  sMtxStart = ID_FALSE;
    idBool                  sIsRetry = ID_FALSE;
    stndrHeader           * sHeader = (stndrHeader*)((smnIndexHeader*)aIndex)->mHeader;
    smLSN                   sNTA;
    idvWeArgs               sWeArgs;
    UInt                    sAllocPageCount = 0;
    sdSID                   sRowSID;
    smSCN                   sStmtSCN;
    sdpPhyPageHdr         * sTargetNode;
    SShort                  sTargetKeySeq;
    scPageID                sEmptyNodePID[2] = { SD_NULL_PID, SD_NULL_PID };
    UInt                    sEmptyNodeCount = 0;
    stndrStatistic          sIndexStat;
    stdMBR                  sKeyInfoMBR;
    stdMBR                  sLeafNodeMBR;
    idBool                  sIsIndexable;
    idBool                  sIsPessimistic = ID_FALSE;
    UChar                   sCTSlotNum = SDN_CTS_INFINITE;
    stndrCallbackContext    sContext;

    
    isIndexableRow( aIndex, aKeyValue, &sIsIndexable );
    
    IDE_TEST_RAISE(sIsIndexable == ID_FALSE, RETURN_NO_INSERT);

    sRowSID = *((sdSID*)aRowSID);

    SM_SET_SCN( &sStmtSCN, &aStmtViewSCN );
    SM_CLEAR_SCN_VIEW_BIT( &sStmtSCN );

    sKeyInfo.mKeyValue   = (UChar*)&(((stdGeometryHeader*)aKeyValue)->mMbr);
    sKeyInfo.mRowPID     = SD_MAKE_PID( sRowSID );
    sKeyInfo.mRowSlotNum = SD_MAKE_SLOTNUM( sRowSID );
    sKeyInfo.mKeyState   = SM_SCN_IS_MAX( sStmtSCN )
        ? STNDR_KEY_STABLE : STNDR_KEY_UNSTABLE;

    STNDR_GET_MBR_FROM_KEYINFO( sKeyInfoMBR, &sKeyInfo );

    sNTA = smxTrans::getTransLstUndoNxtLSN( aTrans );

    idlOS::memset( &sIndexStat, 0x00, ID_SIZEOF(sIndexStat) );

  retry:
        
    IDE_TEST( sdrMiniTrans::begin( aStatistics,
                                   &sMtx,
                                   aTrans,
                                   SDR_MTX_LOGGING,
                                   ID_TRUE,/*MtxUndoable(PROJ-2162)*/
                                   gMtxDLogType ) != IDE_SUCCESS );

    sMtxStart = ID_TRUE;

    sIsRetry = ID_FALSE;
        
    sStack.mDepth = -1;
    
    if( sHeader->mRootNode == SD_NULL_PID )
    {
        IDV_WEARGS_SET(
            &sWeArgs,
            IDV_WAIT_INDEX_LATCH_DRDB_RTREE_INDEX_SMO,
            0,   // WaitParam1
            0,   // WaitParam2
            0 ); // WaitParam3

        IDE_TEST( sHeader->mSdnHeader.mLatch.lockWrite(aStatistics,
                                                       &sWeArgs )
                  != IDE_SUCCESS);

        IDE_TEST( sdrMiniTrans::push(
                      &sMtx,
                      (void*)&sHeader->mSdnHeader.mLatch,
                      SDR_MTX_LATCH_X)
                  != IDE_SUCCESS );

        if( sHeader->mRootNode != SD_NULL_PID )
        {
            // latch ��� ���̿� Root Node�� ���� ���
            sMtxStart = ID_FALSE;
            IDE_TEST( sdrMiniTrans::commit( &sMtx ) != IDE_SUCCESS );
            sIndexStat.mOpRetryCount++;
            goto retry;
        }

        sKeyValueLength = getKeyValueLength();

        IDE_TEST( makeNewRootNode( aStatistics,
                                   &sIndexStat,
                                   &sMtx,
                                   &sMtxStart,
                                   sHeader,
                                   &aInfiniteSCN,
                                   &sKeyInfo,   // aLeftKeyInfo
                                   NULL,        // aLeftNode
                                   NULL,        // aRightKeyInfo
                                   sKeyValueLength,
                                   &sNewChildPage,
                                   &sAllocPageCount )
                  != IDE_SUCCESS );

        sLeafNode = sNewChildPage;
    }
    else
    {
        sIsPessimistic = ID_FALSE;
        
      retraverse:
        
        IDE_TEST( chooseLeafNode( aStatistics,
                                  &sIndexStat,
                                  &sMtx,
                                  &sStack,
                                  sHeader,
                                  &sKeyInfo,
                                  &sLeafNode,
                                  &sLeafKeySeq ) != IDE_SUCCESS );
        
        // Ž���� Root Node�� ����� ��� retry�Ѵ�.
        if( sLeafNode == NULL )
        {
            sMtxStart = ID_FALSE;
            IDE_TEST( sdrMiniTrans::commit(&sMtx) != IDE_SUCCESS );
            sIndexStat.mOpRetryCount++;
            goto retry;
        }


        // BUG-29596: LeafNode�� ���� Ű ���� ���� üũ�� CTL�� Ȯ���� ������� �ʾ� FATAL �߻��մϴ�.
        // Ű ���Խ� CTL�� Ȯ���� ������� �ʾƼ� Free ������ �߸� ����ϴ� ����
        if( canInsertKey( aStatistics,
                          &sMtx,
                          sHeader,
                          &sIndexStat,
                          &sKeyInfo,
                          sLeafNode,
                          &sLeafKeySeq,
                          &sCTSlotNum,
                          &sContext ) == IDE_SUCCESS )
        {
            sNodeHdr = (stndrNodeHdr*)
                sdpPhyPage::getLogicalHdrStartPtr( (UChar*)sLeafNode );

            sLeafNodeMBR = sNodeHdr->mMBR;

            if( stdUtils::isMBRContains( &sLeafNodeMBR, &sKeyInfoMBR )
                != ID_TRUE )
            {
                stdUtils::getMBRExtent( &sLeafNodeMBR, &sKeyInfoMBR );
                
                sStack.mDepth--;
                IDE_TEST( propagateKeyValue( aStatistics,
                                             &sIndexStat,
                                             sHeader,
                                             &sMtx,
                                             &sStack,
                                             sLeafNode,
                                             &sLeafNodeMBR,
                                             &sIsRetry ) != IDE_SUCCESS );
            
                if( sIsRetry == ID_TRUE )
                {
                    // propagation�߿� root node�� ����� ���
                    sMtxStart = ID_FALSE;
                    IDE_TEST( sdrMiniTrans::commit(&sMtx) != IDE_SUCCESS );
                    sIndexStat.mOpRetryCount++;
                    goto retry;
                }
            }

            IDE_TEST( insertKeyIntoLeafNode( &sMtx,
                                             sHeader,
                                             &aInfiniteSCN,
                                             sLeafNode,
                                             &sLeafKeySeq,
                                             &sKeyInfo,
                                             sCTSlotNum,
                                             &sIsSuccess )
                      != IDE_SUCCESS );
        }
        else
        {
            if( sIsPessimistic == ID_FALSE )
            {
                sIsPessimistic = ID_TRUE;
                
                sMtxStart = ID_FALSE;
                IDE_TEST( sdrMiniTrans::commit(&sMtx) != IDE_SUCCESS );

                
                IDE_TEST( sdrMiniTrans::begin( aStatistics,
                                               &sMtx,
                                               aTrans,
                                               SDR_MTX_LOGGING,
                                               ID_TRUE,/*MtxUndoable(PROJ-2162)*/
                                               gMtxDLogType )
                          != IDE_SUCCESS );
                sMtxStart = ID_TRUE;

                IDV_WEARGS_SET(
                    &sWeArgs,
                    IDV_WAIT_INDEX_LATCH_DRDB_RTREE_INDEX_SMO,
                    0,   // WaitParam1
                    0,   // WaitParam2
                    0 ); // WaitParam3

                IDE_TEST( sHeader->mSdnHeader.mLatch.lockWrite( aStatistics,
                                                                &sWeArgs )
                          != IDE_SUCCESS );

                IDE_TEST( sdrMiniTrans::push(
                              &sMtx,
                              (void*)&sHeader->mSdnHeader.mLatch,
                              SDR_MTX_LATCH_X )
                          != IDE_SUCCESS );

                IDE_TEST( findValidStackDepth( aStatistics,
                                               &sIndexStat,
                                               sHeader,
                                               &sStack,
                                               NULL /* aSmoNo */ )
                          != IDE_SUCCESS );

                if( sStack.mDepth < 0 )
                {
                    // BUG-29596: LeafNode�� ���� Ű ���� ���� üũ�� CTL�� Ȯ���� ������� �ʾ� FATAL �߻��մϴ�.
                    // Stress Test(InsertDelete) �� Hang �ɸ��� ����
                    sMtxStart = ID_FALSE;
                    IDE_TEST( sdrMiniTrans::commit(&sMtx) != IDE_SUCCESS );
                    sIndexStat.mOpRetryCount++;
                    goto retry;
                }
                else
                {
                    sStack.mDepth--;
                }
                
                goto retraverse;
            }
            else
            {
                sKeyValueLength = getKeyValueLength();

                IDE_TEST( splitLeafNode( aStatistics,
                                         &sIndexStat,
                                         &sMtx,
                                         &sMtxStart,
                                         sHeader,
                                         &aInfiniteSCN,
                                         &sStack,
                                         sLeafNode,
                                         &sKeyInfo,
                                         sKeyValueLength,
                                         sLeafKeySeq,
                                         ID_TRUE,
                                         smuProperty::getRTreeSplitMode(),
                                         &sTargetNode,
                                         &sTargetKeySeq,
                                         &sAllocPageCount,
                                         &sIsRetry )
                          != IDE_SUCCESS );

                if( sIsRetry == ID_TRUE )
                {
                    // key propagation�� ���� latch�� ��� �߿� root node�� ����� ���
                    sMtxStart = ID_FALSE;
                    IDE_TEST( sdrMiniTrans::commit(&sMtx) != IDE_SUCCESS );
                    sIndexStat.mOpRetryCount++;
                    goto retry;
                }

                IDE_TEST( canInsertKey( aStatistics,
                                        &sMtx,
                                        sHeader,
                                        &sIndexStat,
                                        &sKeyInfo,
                                        sTargetNode,
                                        &sTargetKeySeq,
                                        &sCTSlotNum,
                                        &sContext )
                          != IDE_SUCCESS );
            
                IDE_TEST( insertKeyIntoLeafNode( &sMtx,
                                                 sHeader,
                                                 &aInfiniteSCN,
                                                 sTargetNode,
                                                 &sTargetKeySeq,
                                                 &sKeyInfo,
                                                 sCTSlotNum,
                                                 &sIsSuccess )
                          != IDE_SUCCESS );

                findEmptyNodes( &sMtx,
                                sHeader,
                                sLeafNode,
                                sEmptyNodePID,
                                &sEmptyNodeCount );
                
                sIndexStat.mNodeSplitCount++;
            }
        }
    } // if( sHeader->mRootNode == SD_NULL_PID )

    sHeader->mKeyCount++;

    sdrMiniTrans::setRefNTA( &sMtx,
                             sHeader->mSdnHeader.mIndexTSID,
                             SDR_OP_STNDR_INSERT_KEY_WITH_NTA,
                             &sNTA );

    sMtxStart = ID_FALSE;
    IDE_TEST( sdrMiniTrans::commit(&sMtx) != IDE_SUCCESS );

    if( sAllocPageCount > 0 )
    {
        IDE_TEST( nodeAging( aStatistics,
                             aTrans,
                             &sIndexStat,
                             sHeader,
                             sAllocPageCount )
                  != IDE_SUCCESS );
    }

    if( sEmptyNodeCount > 0 )
    {
        IDE_TEST( linkEmptyNodes( aStatistics,
                                  aTrans,
                                  sHeader,
                                  &sIndexStat,
                                  sEmptyNodePID )
                  != IDE_SUCCESS );
    }

    STNDR_ADD_STATISTIC( &sHeader->mDMLStat, &sIndexStat );

    IDE_EXCEPTION_CONT( RETURN_NO_INSERT );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    
    if( sMtxStart == ID_TRUE )
    {
        sMtxStart = ID_FALSE;
        (void)sdrMiniTrans::rollback( &sMtx );
    }

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION :                                            *
 * ------------------------------------------------------------------*
 * ���� EMPTY LIST�� link�� ���Ŀ�, �ٸ� Ʈ����ǿ� ���ؼ� Ű�� ����
 * �� ��쿡�� EMPTY LIST���� �ش� ��带 �����Ѵ�.                  
 * Node������ ���̻� �ش� ��忡 Ű�� ������ �����ؾ� �Ѵ�. ��, ��� 
 * CTS�� DEAD���¸� �����ؾ� �Ѵ�. ����, Hard Key Stamping�� ����  
 * ��� CTS�� DEAD�� �ɼ� �ֵ��� �õ��Ѵ�.
 *********************************************************************/
IDE_RC stndrRTree::nodeAging( idvSQL            * aStatistics,
                              void              * aTrans,
                              stndrStatistic    * aIndexStat,
                              stndrHeader       * aIndex,
                              UInt                aFreePageCount )
{
    sdnCTL          * sCTL;
    sdnCTS          * sCTS;
    UInt              i;
    UChar             sAgedCount = 0;
    scPageID          sFreeNode;
    sdpPhyPageHdr   * sPage;
    UChar             sDeadCTSlotCount = 0;
    sdrMtx            sMtx;
    stndrKeyInfo      sKeyInfo;
    ULong             sTempKeyBuf[STNDR_MAX_KEY_BUFFER_SIZE/ID_SIZEOF(ULong)];
    stndrLKey       * sLeafKey;
    UShort            sKeyLength;
    UInt              sMtxStart = 0;
    UInt              sFreePageCount = 0;
    idBool            sSuccess = ID_FALSE;
    stndrNodeHdr    * sNodeHdr;
    idvWeArgs         sWeArgs;
    UChar           * sSlotDirPtr;
    
    /*
     * Link�� ȹ���� ���Ŀ� �ٸ� Ʈ����ǿ� ���ؼ� ������ ����
     * NodeAging�� ���� �ʴ´�.
     */
    IDE_TEST_RAISE( aIndex->mEmptyNodeHead == SD_NULL_PID, SKIP_AGING );

    while( 1 )
    {
        sDeadCTSlotCount = 0;
        
        sFreeNode = aIndex->mEmptyNodeHead;

        if( sFreeNode == SD_NULL_PID )
        {
            break;
        }
        
        IDE_TEST( sdrMiniTrans::begin( aStatistics,
                                       &sMtx,
                                       aTrans,
                                       SDR_MTX_LOGGING,
                                       ID_FALSE,/*MtxUndoable(PROJ-2162)*/
                                       gMtxDLogType )
                  != IDE_SUCCESS );
        sMtxStart = 1;

        IDV_WEARGS_SET( &sWeArgs,
                        IDV_WAIT_INDEX_LATCH_DRDB_RTREE_INDEX_SMO,
                        0,   // WaitParam1
                        0,   // WaitParam2
                        0 ); // WaitParam3
        
        IDE_TEST( aIndex->mSdnHeader.mLatch.lockWrite( aStatistics,
                                                       &sWeArgs )
                  != IDE_SUCCESS );

        IDE_TEST( sdrMiniTrans::push( &sMtx,
                                      (void*)&aIndex->mSdnHeader.mLatch,
                                      SDR_MTX_LATCH_X )
                  != IDE_SUCCESS );

        /*
         * ������ ������ sFreeNode�� �״�� ����ϸ� �ȵȴ�.
         * X Latch�� ȹ���� ���Ŀ� FreeNode�� �ٽ� Ȯ���ؾ� ��.
         */
        sFreeNode = aIndex->mEmptyNodeHead;

        if( sFreeNode == SD_NULL_PID )
        {
            sMtxStart = 0;
            IDE_TEST( sdrMiniTrans::commit( &sMtx ) != IDE_SUCCESS );
            break;
        }
        
        IDE_TEST( stndrRTree::getPage( aStatistics,
                                       &(aIndexStat->mIndexPage),
                                       aIndex->mSdnHeader.mIndexTSID,
                                       sFreeNode,
                                       SDB_X_LATCH,
                                       SDB_WAIT_NORMAL,
                                       &sMtx,
                                       (UChar**)&sPage,
                                       &sSuccess )
                  != IDE_SUCCESS );

        sNodeHdr = (stndrNodeHdr*)
            sdpPhyPage::getLogicalHdrStartPtr( (UChar*)sPage );

        /*
         * EMPTY LIST�� �ִ� �ð��߿� �ٸ� Ʈ����ǿ� ���ؼ� Ű�� ���Ե� ��쿡��
         * EMPTY LIST���� unlink�Ѵ�.
         */
        if( sNodeHdr->mUnlimitedKeyCount > 0 )
        {
            IDE_TEST( unlinkEmptyNode( aStatistics,
                                       aIndexStat,
                                       aIndex,
                                       &sMtx,
                                       sPage,
                                       STNDR_IN_USED )
                      != IDE_SUCCESS );

            sMtxStart = 0;
            IDE_TEST( sdrMiniTrans::commit( &sMtx ) != IDE_SUCCESS );

            continue;
        }

        /*
         * TBK KEY�鿡 ���� Aging�� �����Ѵ�.
         */
        if( sNodeHdr->mTBKCount > 0 )
        {
            IDE_TEST( agingAllTBK( aStatistics,
                                   &sMtx,
                                   sPage,
                                   &sSuccess )
                      != IDE_SUCCESS );
        }

        if( sSuccess == ID_FALSE )
        {
            sMtxStart = 0;
            IDE_TEST( sdrMiniTrans::commit( &sMtx ) != IDE_SUCCESS );

            IDE_RAISE( SKIP_AGING );
        }
        
        /*
         * HardKeyStamping�� �ؼ� ��� CTS�� DEAD���¸� �����.
         */
        sCTL = sdnIndexCTL::getCTL( sPage );

        for( i = 0; i < sdnIndexCTL::getCount(sCTL); i++ )
        {
            sCTS = sdnIndexCTL::getCTS( sCTL, i );

            if( sCTS->mState != SDN_CTS_DEAD )
            {
                IDE_TEST( hardKeyStamping( aStatistics,
                                           aIndex,
                                           &sMtx,
                                           sPage,
                                           i,
                                           &sSuccess )
                          != IDE_SUCCESS );

                if( sSuccess == ID_FALSE )
                {
                    sMtxStart = 0;
                    IDE_TEST( sdrMiniTrans::commit( &sMtx ) != IDE_SUCCESS );

                    IDE_RAISE( SKIP_AGING );
                }
            }

            sDeadCTSlotCount++;
        }

        /*
         * ��� CTS�� DEAD������ ���� ��带 �����Ҽ� �ִ� �����̴�.
         */
        if( sDeadCTSlotCount == sdnIndexCTL::getCount(sCTL) )
        {
            sSlotDirPtr = sdpPhyPage::getSlotDirStartPtr( (UChar*)sPage );
            IDE_TEST( sdpSlotDirectory::getPagePtrFromSlotNum(
                                                        sSlotDirPtr,
                                                        0,
                                                        (UChar**)&sLeafKey)
                      != IDE_SUCCESS );
            sKeyLength = getKeyLength( (UChar *)sLeafKey,
                                       ID_TRUE /* aIsLeaf */ );

            idlOS::memcpy( (UChar*)sTempKeyBuf, sLeafKey, sKeyLength );
            
            STNDR_LKEY_TO_KEYINFO( sLeafKey , sKeyInfo );
            
            sKeyInfo.mKeyValue = STNDR_LKEY_KEYVALUE_PTR( (UChar*)sTempKeyBuf );

            sMtxStart = 0;
            IDE_TEST( sdrMiniTrans::commit( &sMtx ) != IDE_SUCCESS );

            /*
             * ���������� Node�� �����Ѵ�.
             */
            IDE_TEST( freeNode( aStatistics,
                                aTrans,
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

        if( sAgedCount >= aFreePageCount )
        {
            break;
        }
    }

    IDE_EXCEPTION_CONT( SKIP_AGING );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if( sMtxStart == 1 )
    {
        sMtxStart = 0;
        (void)sdrMiniTrans::rollback( &sMtx );
    }

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION :                                            *
 * ------------------------------------------------------------------*
 * �ش� ��忡 �ִ� ��� TBK Ű���� Aging �Ѵ�.                      
 * �ش� �Լ��� Node Aging������ ����Ǳ� ������ Create�� Ʈ����ǿ� 
 * ���ؼ� Agable ���θ� �˻����� �ʴ´�.                      
 *********************************************************************/
IDE_RC stndrRTree::agingAllTBK( idvSQL          * aStatistics,
                                sdrMtx          * aMtx,
                                sdpPhyPageHdr   * aNode,
                                idBool          * aIsSuccess )
{
    UChar           * sSlotDirPtr;
    UShort            sKeyCount;
    stndrLKey       * sLeafKey;
    stndrLKeyEx     * sLeafKeyEx;
    smSCN             sLimitSCN;
    idBool            sIsSuccess = ID_TRUE;
    UInt              i;
    stndrNodeHdr    * sNodeHdr;
    UShort            sDeadKeySize = 0;
    UShort            sDeadTBKCount = 0;
    UShort            sTotalTBKCount = 0;
    
    sSlotDirPtr = sdpPhyPage::getSlotDirStartPtr( (UChar*)aNode );
    
    sKeyCount = sdpSlotDirectory::getCount( sSlotDirPtr );

    for( i = 0; i < sKeyCount; i++ )
    {
        IDE_TEST( sdpSlotDirectory::getPagePtrFromSlotNum( sSlotDirPtr,
                                                           i,
                                                           (UChar**)&sLeafKey )
                  != IDE_SUCCESS );
        sLeafKeyEx = (stndrLKeyEx*)sLeafKey;
        
        if( STNDR_GET_STATE( sLeafKey ) == STNDR_KEY_DEAD )
        {
            continue;
        }
        
        if( STNDR_GET_TB_TYPE( sLeafKey ) == STNDR_KEY_TB_CTS )
        {
            continue;
        }
        
        IDE_DASSERT( STNDR_GET_STATE( sLeafKey ) == STNDR_KEY_DELETED );

        /*
         * Create�� Ʈ������� Agable�˻�� ���� �ʴ´�.
         * Limit Ʈ������� Agable�ϴٰ� �ǴܵǸ� Create�� Agable�ϴٰ�
         * �Ǵ��Ҽ� �ִ�.
         */
        if( STNDR_GET_LCTS_NO( sLeafKey  ) == SDN_CTS_IN_KEY )
        {
            IDE_TEST( getCommitSCN( aStatistics,
                                    NULL,        /* aTrans */
                                    aNode,
                                    sLeafKeyEx,
                                    ID_TRUE,     /* aIsLimitSCN */
                                    SM_SCN_INIT, /* aStmtViewSCN */
                                    &sLimitSCN )
                      != IDE_SUCCESS );

            if( isAgableTBK( sLimitSCN ) == ID_FALSE )
            {
                sIsSuccess = ID_FALSE;
                break;
            }
            else
            {
                STNDR_SET_STATE( sLeafKey , STNDR_KEY_DEAD );
                STNDR_SET_CCTS_NO( sLeafKey , SDN_CTS_INFINITE );
                STNDR_SET_LCTS_NO( sLeafKey , SDN_CTS_INFINITE );

                sDeadTBKCount++;

                IDE_TEST( sdrMiniTrans::writeNBytes(
                              aMtx,
                              (UChar*)sLeafKey ->mTxInfo,
                              (void*)sLeafKey ->mTxInfo,
                              ID_SIZEOF(UChar)*2 )
                          != IDE_SUCCESS );
                
                sDeadKeySize += getKeyLength( (UChar*)sLeafKey ,
                                              ID_TRUE );
                
                sDeadKeySize += ID_SIZEOF( sdpSlotEntry );
            }
        }
    }

    if( sDeadKeySize > 0 )
    {
        sNodeHdr = (stndrNodeHdr*)
            sdpPhyPage::getLogicalHdrStartPtr( (UChar*)aNode );
        
        sDeadKeySize += sNodeHdr->mTotalDeadKeySize;
        
        IDE_TEST(sdrMiniTrans::writeNBytes(
                     aMtx,
                     (UChar*)&sNodeHdr->mTotalDeadKeySize,
                     (void*)&sDeadKeySize,
                     ID_SIZEOF(UShort) )
                 != IDE_SUCCESS );
    }

    if( sDeadTBKCount > 0 )
    {
        sNodeHdr = (stndrNodeHdr*)
            sdpPhyPage::getLogicalHdrStartPtr( (UChar*)aNode );
        
        sTotalTBKCount = sNodeHdr->mTBKCount - sDeadTBKCount;
        
        IDE_TEST(sdrMiniTrans::writeNBytes(
                     aMtx,
                     (UChar*)&sNodeHdr->mTBKCount,
                     (void*)&sTotalTBKCount,
                     ID_SIZEOF(UShort) )
                 != IDE_SUCCESS );
    }

    *aIsSuccess = sIsSuccess;
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION :                                            *
 * ------------------------------------------------------------------*
 * TBK Ű�� ���ؼ� �־��� CommitSCN�� Agable���� �˻��Ѵ�.               
 *********************************************************************/
idBool stndrRTree::isAgableTBK( smSCN   aCommitSCN )
{
    smSCN  sSysMinDskViewSCN;
    idBool sIsSuccess = ID_TRUE;

    if( SM_SCN_IS_INFINITE(aCommitSCN) == ID_TRUE )
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
        if( SM_SCN_IS_INIT( sSysMinDskViewSCN ) ||
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
 * FUNCTION DESCRIPTION :                                            *
 * ------------------------------------------------------------------*
 * DEAD Key�߿� �ϳ��� �����ؼ� ������ ��带 ã�´�.               
 * ã�� ��忡 Ű�� �ϳ��� �����Ѵٸ� EMPTY LIST�� �������Ŀ� �ٸ� 
 * Ʈ����ǿ� ���ؼ� Ű�� ���Ե� ����̱� ������, �̷��� ��쿡��    
 * �ش� ��带 EMTPY LIST���� �����Ѵ�.                              
 * �׷��� ���� ��쿡�� ��� ������ �ϸ� ���� �θ� ��忡 �ش� ��忡 ����
 * Ű�� �����Ѵ�.
 * Free Node ���� EMPTY NODE�� FREE LIST�� �̵��Ǿ� ����ȴ�.      
 *********************************************************************/
IDE_RC stndrRTree::freeNode( idvSQL         * aStatistics,
                             void           * aTrans,
                             stndrHeader    * aIndex,
                             scPageID         aFreeNodeID,
                             stndrKeyInfo   * aKeyInfo,
                             stndrStatistic * aIndexStat,
                             UInt           * aFreePageCount )
{
    UChar           * sLeafNode;
    SShort            sLeafKeySeq;
    sdrMtx            sMtx;
    idBool            sMtxStart = ID_FALSE;
    idvWeArgs         sWeArgs;
    scPageID          sSegPID;
    stndrPathStack    sStack;
    stndrNodeHdr    * sNodeHdr;
    scPageID          sPID;
    idBool            sIsRetry = ID_FALSE;


    sSegPID = aIndex->mSdnHeader.mSegmentDesc.mSegHandle.mSegPID;

    // init stack
    sStack.mDepth = -1;

    IDE_TEST( sdrMiniTrans::begin( aStatistics,
                                   &sMtx,
                                   aTrans,
                                   SDR_MTX_LOGGING,
                                   ID_FALSE,/*MtxUndoable(PROJ-2162)*/
                                   gMtxDLogType )
              != IDE_SUCCESS );
    
    sMtxStart = ID_TRUE;

    IDV_WEARGS_SET( &sWeArgs,
                    IDV_WAIT_INDEX_LATCH_DRDB_RTREE_INDEX_SMO,
                    0,   // WaitParam1
                    0,   // WaitParam2
                    0 ); // WaitParam3

    IDE_TEST( aIndex->mSdnHeader.mLatch.lockWrite( aStatistics,
                                                   &sWeArgs )
              != IDE_SUCCESS );

    IDE_TEST( sdrMiniTrans::push( &sMtx,
                                  (void*)&aIndex->mSdnHeader.mLatch,
                                  SDR_MTX_LATCH_X )
              != IDE_SUCCESS );

    /*
     * TREE LATCH�� ȹ���ϱ� ������ ������ aFreeNode�� ��ȿ���� �˻��Ѵ�.
     */
    IDE_TEST_RAISE( aIndex->mEmptyNodeHead != aFreeNodeID, SKIP_UNLINK_NODE );
        
    IDE_TEST( traverse( aStatistics,
                        aIndex,
                        &sMtx,
                        NULL, /* aCursorSCN */
                        NULL, /* aFstDiskViewSCN */
                        aKeyInfo,
                        aFreeNodeID,
                        STNDR_TRAVERSE_FREE_NODE,
                        &sStack,
                        aIndexStat,
                        (sdpPhyPageHdr**)&sLeafNode,
                        &sLeafKeySeq )
              != IDE_SUCCESS );

    IDE_TEST_RAISE( sLeafNode == NULL, SKIP_UNLINK_NODE );

    sNodeHdr = (stndrNodeHdr*)
        sdpPhyPage::getLogicalHdrStartPtr((UChar*)sLeafNode);

    if( sNodeHdr->mUnlimitedKeyCount == 0 )
    {
        if( sStack.mDepth > 0 )
        {
            sPID = sdpPhyPage::getPageID( (UChar*)sLeafNode );
            sStack.mDepth--;
            IDE_TEST( deleteInternalKey( aStatistics,
                                         aIndex,
                                         aIndexStat,
                                         sSegPID,
                                         sPID,
                                         &sMtx,
                                         &sStack,
                                         aFreePageCount,
                                         &sIsRetry )
                      != IDE_SUCCESS );

            IDE_TEST_RAISE( sIsRetry == ID_TRUE, SKIP_UNLINK_NODE );
        }
        else
        {
            IDE_TEST( unsetRootNode( aStatistics,
                                     &sMtx,
                                     aIndex,
                                     aIndexStat )
                      != IDE_SUCCESS );
        }

        IDE_TEST( unlinkEmptyNode( aStatistics,
                                   aIndexStat,
                                   aIndex,
                                   &sMtx,
                                   (sdpPhyPageHdr*)sLeafNode,
                                   STNDR_IN_FREE_LIST )
                  != IDE_SUCCESS );

        IDE_TEST( freePage( aStatistics,
                            aIndexStat,
                            aIndex,
                            &sMtx,
                            sLeafNode )
                  != IDE_SUCCESS );
        
        aIndexStat->mNodeDeleteCount++;
        (*aFreePageCount)++;
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
                                   STNDR_IN_USED )
                  != IDE_SUCCESS );
    }

    IDE_EXCEPTION_CONT( SKIP_UNLINK_NODE );
    
    sMtxStart = ID_FALSE;
    IDE_TEST( sdrMiniTrans::commit(&sMtx) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if( sMtxStart == ID_TRUE )
    {
        (void)sdrMiniTrans::rollback( &sMtx );
    }

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION :
 * ------------------------------------------------------------------*
 * Index���� Row�� RID�� Key Value�� �̿��Ͽ� Ư�� Ű�� ��ġ�� ã�� �Լ��̴�.
 * Traverse�� ȣ��Ǵ� ���� ������ ����.
 *
 * traverse�� �ʿ��� ���:
 *      1. freeNode
 *      2. deleteKey
 *      3. insertKeyRollback
 *      4. deleteKeyRollback
 *********************************************************************/
IDE_RC stndrRTree::traverse( idvSQL         * aStatistics,
                             stndrHeader    * aIndex,
                             sdrMtx         * aMtx,
                             smSCN          * aCursorSCN,
                             smSCN          * aFstDiskViewSCN,
                             stndrKeyInfo   * aKeyInfo,
                             scPageID         aFreeNodePID,
                             UInt             aTraverseMode,
                             stndrPathStack * aStack,
                             stndrStatistic * aIndexStat,
                             sdpPhyPageHdr ** aLeafNode,
                             SShort         * aLeafKeySeq )
{
    stndrNodeHdr        * sNodeHdr;
    stndrStack            sTraverseStack;
    stndrStackSlot        sStackSlot;
    stndrIKey           * sIKey;
    stndrVirtualRootNode  sVRootNode;
    sdpPhyPageHdr       * sNode;
    scPageID              sChildPID;
    UChar               * sSlotDirPtr;
    UShort                sKeyCount;
    SShort                sKeySeq;
    ULong                 sIndexSmoNo;
    ULong                 sNodeSmoNo;
    idBool                sIsSuccess;
    idBool                sTraverseStackInit = ID_FALSE;
    stdMBR                sMBR;
    stdMBR                sKeyMBR;
    sdrSavePoint          sSP;
    SInt                  i;
    UInt                  sState = 0;


    IDE_TEST( stndrStackMgr::initialize( &sTraverseStack )
              != IDE_SUCCESS );
    sTraverseStackInit = ID_TRUE;
    
    *aLeafNode = NULL;
    *aLeafKeySeq = STNDR_INVALID_KEY_SEQ; // BUG-29515: Codesonar (Uninitialized Value)

    stndrStackMgr::clear( &sTraverseStack );

    // init stack
    aStack->mDepth = -1;

    getVirtualRootNode( aIndex, &sVRootNode );

    if( sVRootNode.mChildPID != SD_NULL_PID )
    {
        IDE_TEST( stndrStackMgr::push( &sTraverseStack,
                                       sVRootNode.mChildPID,
                                       sVRootNode.mChildSmoNo,
                                       (SShort)STNDR_INVALID_KEY_SEQ )
                  != IDE_SUCCESS );
    }

    IDU_FIT_POINT( "1.PROJ-1591@stndrRTree::traverse" );

    while( stndrStackMgr::getDepth(&sTraverseStack) >= 0 )
    {
        sStackSlot = stndrStackMgr::pop( &sTraverseStack );

        if( sStackSlot.mNodePID == SD_NULL_PID )
        {
            ideLog::log( IDE_SERVER_0,
                         "Index Runtime Header:" );
            ideLog::logMem( IDE_SERVER_0,
                            (UChar*)aIndex,
                            ID_SIZEOF(stndrHeader) );

            ideLog::log( IDE_SERVER_0,
                         "Stack Slot:" );
            ideLog::logMem( IDE_SERVER_0,
                            (UChar*)&sStackSlot,
                            ID_SIZEOF(stndrStackSlot) );

            ideLog::log( IDE_SERVER_0,
                         "Traverse Stack Depth %d"
                         ", Traverse Stack Size %d"
                         ", Traverse Stack:",
                         sTraverseStack.mDepth,
                         sTraverseStack.mStackSize );
            ideLog::logMem( IDE_SERVER_0,
                            (UChar*)sTraverseStack.mStack,
                            ID_SIZEOF(stndrStackSlot) * sTraverseStack.mStackSize );
            IDE_ASSERT( 0 );
        }

        sState = 1;
        IDE_TEST( stndrRTree::getPage( aStatistics,
                                       &(aIndex->mQueryStat.mIndexPage),
                                       aIndex->mSdnHeader.mIndexTSID,
                                       sStackSlot.mNodePID,
                                       SDB_S_LATCH,
                                       SDB_WAIT_NORMAL,
                                       NULL,
                                       (UChar**)&sNode,
                                       &sIsSuccess )
                  != IDE_SUCCESS );

        sNodeHdr =
            (stndrNodeHdr*)sdpPhyPage::getLogicalHdrStartPtr( (UChar*)sNode );

        if( sdpPhyPage::getIndexSMONo(sNode) > sStackSlot.mSmoNo )
        {
            if( sdpPhyPage::getNxtPIDOfDblList(sNode) == SD_NULL_PID )
            {
                ideLog::log( IDE_SERVER_0,
                             "Index Runtime Header:" );
                ideLog::logMem( IDE_SERVER_0,
                                (UChar*)aIndex,
                                ID_SIZEOF(stndrHeader) );

                ideLog::log( IDE_SERVER_0,
                             "Stack Slot:" );
                ideLog::logMem( IDE_SERVER_0,
                                (UChar*)&sStackSlot,
                                ID_SIZEOF(stndrStackSlot) );

                ideLog::log( IDE_SERVER_0,
                             "Traverse Stack Depth %d"
                             ", Traverse Stack Size %d"
                             ", Traverse Stack:",
                             sTraverseStack.mDepth,
                             sTraverseStack.mStackSize );
                ideLog::logMem( IDE_SERVER_0,
                                (UChar*)sTraverseStack.mStack,
                                ID_SIZEOF(stndrStackSlot) * sTraverseStack.mStackSize );

                dumpIndexNode( sNode );
                IDE_ASSERT( 0 );
            }
            
            IDE_TEST( stndrStackMgr::push( &sTraverseStack,
                                           sdpPhyPage::getNxtPIDOfDblList(sNode),
                                           sStackSlot.mSmoNo,
                                           (SShort)STNDR_INVALID_KEY_SEQ )
                      != IDE_SUCCESS );
            
            aIndexStat->mFollowRightLinkCount++;
        }

        // push path
        while( aStack->mDepth >= 0 )
        {
            if( aStack->mStack[aStack->mDepth].mHeight >
                sNodeHdr->mHeight )
            {
                break;
            }

            aStack->mDepth--;
        }

        aStack->mDepth++;
        aStack->mStack[aStack->mDepth].mNodePID = sStackSlot.mNodePID;
        aStack->mStack[aStack->mDepth].mHeight  = sNodeHdr->mHeight;
        aStack->mStack[aStack->mDepth].mSmoNo   = sdpPhyPage::getIndexSMONo(sNode);
        aStack->mStack[aStack->mDepth].mKeySeq  = sStackSlot.mKeySeq;
        if( aStack->mDepth >= 1 )
        {
            aStack->mStack[aStack->mDepth - 1].mKeySeq = sStackSlot.mKeySeq;
        }

        if( sNodeHdr->mHeight > 0 ) // internal
        {
            sSlotDirPtr = sdpPhyPage::getSlotDirStartPtr( (UChar*)sNode );
            sKeyCount  = sdpSlotDirectory::getCount( sSlotDirPtr );

            getSmoNo( aIndex, &sIndexSmoNo );
            IDL_MEM_BARRIER;
            
            for( i = 0; i < sKeyCount; i++ )
            {
                aIndexStat->mKeyCompareCount++;
                
                IDE_TEST ( sdpSlotDirectory::getPagePtrFromSlotNum( 
                                                               sSlotDirPtr,
                                                               i,
                                                               (UChar**)&sIKey )
                           != IDE_SUCCESS );

                STNDR_GET_MBR_FROM_IKEY( sMBR, sIKey );
                STNDR_GET_CHILD_PID( &sChildPID, sIKey );
                STNDR_GET_MBR_FROM_KEYINFO( sKeyMBR, aKeyInfo );

                if( stdUtils::isMBRContains(&sMBR, &sKeyMBR) == ID_TRUE )
                {
                    IDE_TEST( stndrStackMgr::push( &sTraverseStack,
                                                   sChildPID,
                                                   sIndexSmoNo,
                                                   i ) != IDE_SUCCESS );
                }
            }

            sState = 0;
            IDE_TEST( sdbBufferMgr::releasePage( aStatistics, (UChar*)sNode )
                      != IDE_SUCCESS );
        }
        else // leaf
        {
            sNodeSmoNo = sdpPhyPage::getIndexSMONo( sNode );

            sState = 0;
            IDE_TEST( sdbBufferMgr::releasePage( aStatistics, (UChar*)sNode )
                      != IDE_SUCCESS );
            
            
            sdrMiniTrans::setSavePoint( aMtx, &sSP );

            IDE_TEST( stndrRTree::getPage( aStatistics,
                                           &(aIndexStat->mIndexPage),
                                           aIndex->mSdnHeader.mIndexTSID,
                                           sStackSlot.mNodePID,
                                           SDB_X_LATCH,
                                           SDB_WAIT_NORMAL,
                                           aMtx,
                                           (UChar**)&sNode,
                                           &sIsSuccess )
                      != IDE_SUCCESS );
            
            if( sdpPhyPage::getIndexSMONo(sNode) > sNodeSmoNo )
            {
                if( sdpPhyPage::getNxtPIDOfDblList(sNode) == SD_NULL_PID )
                {
                    ideLog::log( IDE_SERVER_0,
                                 "Index Runtime Header:" );
                    ideLog::logMem( IDE_SERVER_0,
                                    (UChar*)aIndex,
                                    ID_SIZEOF(stndrHeader) );

                    ideLog::log( IDE_SERVER_0,
                                 "Stack Slot:" );
                    ideLog::logMem( IDE_SERVER_0,
                                    (UChar*)&sStackSlot,
                                    ID_SIZEOF(stndrStackSlot) );

                    ideLog::log( IDE_SERVER_0,
                                 "Traverse Stack Depth %d"
                                 ", Traverse Stack Size %d"
                                 ", Traverse Stack:",
                                 sTraverseStack.mDepth,
                                 sTraverseStack.mStackSize );
                    ideLog::logMem( IDE_SERVER_0,
                                    (UChar*)sTraverseStack.mStack,
                                    ID_SIZEOF(stndrStackSlot) * sTraverseStack.mStackSize );

                    dumpIndexNode( sNode );
                    IDE_ASSERT( 0 );
                }
            
                IDE_TEST( stndrStackMgr::push( &sTraverseStack,
                                               sdpPhyPage::getNxtPIDOfDblList(sNode),
                                               sNodeSmoNo,
                                               sStackSlot.mKeySeq )
                          != IDE_SUCCESS );
                
                aIndexStat->mFollowRightLinkCount++;
            }

            if( aTraverseMode == STNDR_TRAVERSE_FREE_NODE )
            {
                IDE_ASSERT(aFreeNodePID != SD_NULL_PID);
                
                if( sdpPhyPage::getPageID((UChar*)sNode) == aFreeNodePID )
                {
                    *aLeafNode   = sNode;
                    *aLeafKeySeq = STNDR_INVALID_KEY_SEQ;

                    aStack->mStack[aStack->mDepth].mKeySeq = STNDR_INVALID_KEY_SEQ;
                    break;
                }
            }
            else
            {
                IDE_TEST( findLeafKey( aIndex,
                                       aMtx,
                                       aCursorSCN,
                                       aFstDiskViewSCN,
                                       aTraverseMode,
                                       sNode,
                                       aIndexStat,
                                       aKeyInfo,
                                       &sKeySeq,
                                       &sIsSuccess ) != IDE_SUCCESS );
            
                if( sIsSuccess == ID_TRUE )
                {
                    *aLeafNode   = sNode;
                    *aLeafKeySeq = sKeySeq;

                    aStack->mStack[aStack->mDepth].mKeySeq = sKeySeq;
                    break;
                }
            } // if( aTraverseMode == STNDR_TRAVERSE_FREE_NODE )

            sdrMiniTrans::releaseLatchToSP( aMtx, &sSP );
        }
    } // while

    sTraverseStackInit = ID_FALSE;
    IDE_TEST( stndrStackMgr::destroy( &sTraverseStack )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if( sState == 1 )
    {
        if( sdbBufferMgr::releasePage( aStatistics, (UChar*)sNode )
            != IDE_SUCCESS )
        {
            dumpIndexNode( sNode );
            IDE_ASSERT( 0 );
        }
    }

    if( sTraverseStackInit == ID_TRUE )
    {
        sTraverseStackInit = ID_FALSE;
        if( stndrStackMgr::destroy( &sTraverseStack )
            != IDE_SUCCESS )
        {
            ideLog::log( IDE_SERVER_0,
                         "Index Runtime Header:" );
            ideLog::logMem( IDE_SERVER_0,
                            (UChar*)aIndex,
                            ID_SIZEOF(stndrHeader) );

            ideLog::log( IDE_SERVER_0,
                         "Stack Slot:" );
            ideLog::logMem( IDE_SERVER_0,
                            (UChar*)&sStackSlot,
                            ID_SIZEOF(stndrStackSlot) );

            ideLog::log( IDE_SERVER_0,
                         "Traverse Stack Depth %d"
                         ", Traverse Stack Size %d"
                         ", Traverse Stack:",
                         sTraverseStack.mDepth,
                         sTraverseStack.mStackSize );
            ideLog::logMem( IDE_SERVER_0,
                            (UChar*)sTraverseStack.mStack,
                            ID_SIZEOF(stndrStackSlot) * sTraverseStack.mStackSize );
            IDE_ASSERT( 0 );
        }
    }

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION :
 * ------------------------------------------------------------------*
 * �־��� Leaf Node���� Row�� RID�� Key Value�� �̿��Ͽ� ��� Ű�� ã�´�.
 * �⺻������ RID�� Key Value�� ���ϸ�, Ž�� ��� ���� ������ ���� �񱳰�
 * �߰��� ����ȴ�.
 *  1. delete key
 *     Ű ���°� STABLE �Ǵ� UNSTABLE ����
 *  2. insert key rollback
 *     Ű ���°� UNSTABLE
 *  3. delete key rollback
 *     Ű ���°� DELETED
 *     Ű�� LimitSCN�� CursorSCN�� ����
 *     MyTransaction
 *********************************************************************/
IDE_RC stndrRTree::findLeafKey( stndrHeader     * /*aIndex*/,
                                sdrMtx          * aMtx,
                                smSCN           * aCursorSCN,
                                smSCN           * aFstDiskViewSCN,
                                UInt              aTraverseMode,
                                sdpPhyPageHdr   * aNode,
                                stndrStatistic  * aIndexStat,
                                stndrKeyInfo    * aKeyInfo,
                                SShort          * aKeySeq,
                                idBool          * aIsSuccess )
{
    UChar           * sSlotDirPtr;
    UShort            sKeyCount;
    stndrLKey       * sLKey;
    stndrKeyInfo      sLKeyInfo;
    smSCN             sCreateSCN;
    smSCN             sLimitSCN;
    stdMBR            sMBR;
    stdMBR            sKeyMBR;
    UInt              i;
    smSCN             sBeginSCN;
    sdSID             sTSSlotSID;

    
    *aIsSuccess = ID_FALSE;
    
    sSlotDirPtr = sdpPhyPage::getSlotDirStartPtr( (UChar*)aNode );
    sKeyCount   = sdpSlotDirectory::getCount( sSlotDirPtr );

    for( i = 0; i < sKeyCount; i++ )
    {
        aIndexStat->mKeyCompareCount++;
        
        IDE_TEST( sdpSlotDirectory::getPagePtrFromSlotNum( sSlotDirPtr, 
                                                           i,
                                                           (UChar**)&sLKey )
                  != IDE_SUCCESS );

        STNDR_LKEY_TO_KEYINFO( sLKey, sLKeyInfo );

        if( (sLKeyInfo.mRowPID     == aKeyInfo->mRowPID) &&
            (sLKeyInfo.mRowSlotNum == aKeyInfo->mRowSlotNum) )
        {
            STNDR_GET_MBR_FROM_LKEY( sMBR, sLKey );
            STNDR_GET_MBR_FROM_KEYINFO( sKeyMBR, aKeyInfo );

            STNDR_GET_CSCN( sLKey, &sCreateSCN );
            STNDR_GET_LSCN( sLKey, &sLimitSCN );

            aIndexStat->mKeyCompareCount++;
            
            if( stdUtils::isMBRContains(&sMBR, &sKeyMBR) != ID_TRUE )
            {
                continue;
            }

            switch( aTraverseMode )
            {
                case STNDR_TRAVERSE_DELETE_KEY:
                {
                    if( STNDR_GET_STATE(sLKey) == STNDR_KEY_STABLE ||
                        STNDR_GET_STATE(sLKey) == STNDR_KEY_UNSTABLE )
                    {
                        *aKeySeq = i;
                        *aIsSuccess = ID_TRUE;
                    }
                }
                break;
                
                case STNDR_TRAVERSE_INSERTKEY_ROLLBACK:
                {
                    if( STNDR_GET_STATE(sLKey) == STNDR_KEY_UNSTABLE )
                    {
                        *aKeySeq = i;
                        *aIsSuccess = ID_TRUE;
                    }
                }
                break;
                
                case STNDR_TRAVERSE_DELETEKEY_ROLLBACK:
                {
                    IDE_ASSERT( aCursorSCN != NULL );
                
                    if( STNDR_GET_STATE(sLKey) == STNDR_KEY_DELETED &&
                        SM_SCN_IS_EQ(&sLimitSCN, aCursorSCN) )
                    {
                        if( STNDR_GET_LCTS_NO( sLKey ) == SDN_CTS_IN_KEY )
                        {
                            STNDR_GET_TBK_LSCN( ((stndrLKeyEx*)sLKey ), &sBeginSCN );
                            STNDR_GET_TBK_LTSS( ((stndrLKeyEx*)sLKey ), &sTSSlotSID );
                            
                            if( isMyTransaction( aMtx->mTrans,
                                                 sBeginSCN,
                                                 sTSSlotSID,
                                                 aFstDiskViewSCN ) )
                            {
                                *aKeySeq = i;
                                *aIsSuccess = ID_TRUE;
                            }
                        }
                        else
                        {
                            if( sdnIndexCTL::isMyTransaction( aMtx->mTrans,
                                                              aNode,
                                                              STNDR_GET_LCTS_NO(sLKey),
                                                              aFstDiskViewSCN ) )
                            {
                                *aKeySeq = i;
                                *aIsSuccess = ID_TRUE;
                            }
                        }
                    }
                }
                break;
                
                default:
                    ideLog::log( IDE_SERVER_0,
                                 "Traverse Mode: %u\n",
                                 aTraverseMode );
                    IDE_ASSERT( 0 );
                    break;
            }

            if( *aIsSuccess == ID_TRUE )
            {
                break;
            }
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION :                                            *
 * ------------------------------------------------------------------*
 * Transaction Level���� Transaction�� Snapshot�� ���� Ű����        
 * Ȯ���Ѵ�. �ش� �Լ��� Key�� �ִ� Ʈ����� ���������� Visibility��
 * Ȯ���Ҽ� �ִ� ��������� �˻��Ѵ�. ���� �Ǵ��� ���� �ʴ´ٸ�      
 * Cursor Level visibility�� �˻��ؾ� �Ѵ�.                         
 *                                                                   
 * �Ʒ��� ��찡 Visibility�� Ȯ���Ҽ� ���� ����̴�.               
 *                                                                   
 * 1. CreateSCN�� Shnapshot.SCN���� ���� Duplicate Key�� ������ ��� 
 * 2. �ڽ��� ����/������ Ű�� ������ ���                            
 *                                                                   
 * ���� ��츦 ������ ��� Ű�� �Ʒ��� ���� 4���� ���� �з��ȴ�.   
 *                                                                   
 * 1. LimitSCN < CreateSCN < StmtSCN                                 
 *    : LimitSCN�� Upper Bound SCN�� ��찡 CreateSCN���� LimitSCN�� 
 *      �� ���� �� �ִ�. Upper Bound SCN�� "0"�̴�.                  
 *      �ش� ���� "Visible = TRUE"�̴�.                            
 *                                                                   
 * 2. CreateSCN < StmtSCN < LimitSCN                                 
 *    : ���� �������� �ʾұ� ������ "Visible = TRUE"�̴�.            
 *                                                                   
 * 3. CreateSCN < LimitSCN < StmtSCN                                
 *    : �̹� ������ Ű�̱� ������ "Visible = FALSE"�̴�.             
 *                                                                   
 * 4. StmtSCN < CreateSCN < LimitSCN                                 
 *    : Select�� ������ ��� ������ ������ �ʾҾ��� Ű�̱� ������    
 *      "Visible = FALSE"�̴�.                                       
 *********************************************************************/
IDE_RC stndrRTree::tranLevelVisibility( idvSQL          * aStatistics,
                                        void            * aTrans,
                                        UChar           * aNode,
                                        UChar           * aLeafKey ,
                                        smSCN           * aStmtViewSCN,
                                        idBool          * aIsVisible,
                                        idBool          * aIsUnknown )
{
    stndrLKey               * sLeafKey;
    stndrKeyInfo              sKeyInfo;
    smSCN                     sCreateSCN;
    smSCN                     sLimitSCN;
    smSCN                     sBeginSCN;
    sdSID                     sTSSlotSID;
    
    sLeafKey  = (stndrLKey*)aLeafKey;
    STNDR_LKEY_TO_KEYINFO( sLeafKey, sKeyInfo );

    if( STNDR_GET_STATE( sLeafKey ) == STNDR_KEY_DEAD )
    {
        *aIsVisible = ID_FALSE;
        *aIsUnknown = ID_FALSE;
        IDE_RAISE( RETURN_SUCCESS );
    }

    if( STNDR_GET_STATE( sLeafKey ) == STNDR_KEY_STABLE )
    {
        *aIsVisible = ID_TRUE;
        *aIsUnknown = ID_FALSE;
        IDE_RAISE( RETURN_SUCCESS );
    }
    
    if( STNDR_GET_CCTS_NO( sLeafKey ) == SDN_CTS_INFINITE )
    {
        SM_INIT_SCN( &sCreateSCN );
        if( STNDR_GET_LCTS_NO( sLeafKey ) == SDN_CTS_INFINITE )
        {
            ideLog::log( IDE_SERVER_0, "Leaf Key:\n" );
            ideLog::logMem( IDE_SERVER_0,
                            (UChar *)sLeafKey,
                            ID_SIZEOF( stndrLKey ) );
            dumpIndexNode( (sdpPhyPageHdr *)aNode );
            IDE_ASSERT( 0 );
        }
    }
    else
    {
        if( STNDR_GET_CCTS_NO( sLeafKey ) == SDN_CTS_IN_KEY )
        {
            IDE_TEST( getCommitSCN( aStatistics,
                                    aTrans,
                                    (sdpPhyPageHdr*)aNode,
                                    (stndrLKeyEx*)sLeafKey ,
                                    ID_FALSE, /* aIsLimitSCN */
                                    *aStmtViewSCN,
                                    &sCreateSCN )
                      != IDE_SUCCESS );
        }
        else
        {
            IDE_TEST( sdnIndexCTL::getCommitSCN( aStatistics,
                                                 aTrans,
                                                 (sdpPhyPageHdr*)aNode,
                                                 SDB_SINGLE_PAGE_READ,
                                                 STNDR_GET_CCTS_NO( sLeafKey ),
                                                 *aStmtViewSCN,
                                                 &sCreateSCN )
                      != IDE_SUCCESS );
        }
    }

    if( STNDR_GET_LCTS_NO( sLeafKey ) == SDN_CTS_INFINITE )
    {
        SM_MAX_SCN( &sLimitSCN );
    }
    else
    {
        if( STNDR_GET_LCTS_NO( sLeafKey ) == SDN_CTS_IN_KEY )
        {
            IDE_TEST( getCommitSCN( aStatistics,
                                    aTrans,
                                    (sdpPhyPageHdr*)aNode,
                                    (stndrLKeyEx*)sLeafKey ,
                                    ID_TRUE, /* aIsLimitSCN */
                                    *aStmtViewSCN,
                                    &sLimitSCN )
                      != IDE_SUCCESS );
        }
        else
        {
            IDE_TEST( sdnIndexCTL::getCommitSCN( aStatistics,
                                                 aTrans,
                                                 (sdpPhyPageHdr*)aNode,
                                                 SDB_SINGLE_PAGE_READ,
                                                 STNDR_GET_LCTS_NO( sLeafKey ),
                                                 *aStmtViewSCN,
                                                 &sLimitSCN )
                      != IDE_SUCCESS );
        }
    }

    if( SM_SCN_IS_INFINITE( sCreateSCN ) == ID_TRUE )
    {
        if( STNDR_GET_CCTS_NO( sLeafKey ) == SDN_CTS_IN_KEY )
        {
            STNDR_GET_TBK_CSCN( ((stndrLKeyEx*)sLeafKey ), &sBeginSCN );
            STNDR_GET_TBK_CTSS( ((stndrLKeyEx*)sLeafKey ), &sTSSlotSID );
            
            if( isMyTransaction( aTrans, sBeginSCN, sTSSlotSID ) )
            {
                *aIsVisible = ID_FALSE;
                *aIsUnknown = ID_TRUE;
                IDE_RAISE( RETURN_SUCCESS );
            }
        }
        else
        {
            if( sdnIndexCTL::isMyTransaction( aTrans,
                                              (sdpPhyPageHdr*)aNode,
                                              STNDR_GET_CCTS_NO( sLeafKey ) )
                == ID_TRUE )
            {
                *aIsVisible = ID_FALSE;
                *aIsUnknown = ID_TRUE;
                IDE_RAISE( RETURN_SUCCESS );
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
        if( SDC_CTS_SCN_IS_LEGACY(sCreateSCN) == ID_TRUE )
        {
            *aIsVisible = ID_FALSE;
            *aIsUnknown = ID_TRUE;
            IDE_RAISE( RETURN_SUCCESS );
        }

        if( SM_SCN_IS_INFINITE( sLimitSCN ) == ID_TRUE )
        {
            if( STNDR_GET_LCTS_NO( sLeafKey ) == SDN_CTS_IN_KEY )
            {
                STNDR_GET_TBK_LSCN( ((stndrLKeyEx*)sLeafKey ), &sBeginSCN );
                STNDR_GET_TBK_LTSS( ((stndrLKeyEx*)sLeafKey ), &sTSSlotSID );
                
                if( isMyTransaction( aTrans, sBeginSCN, sTSSlotSID ) )
                {
                    *aIsVisible = ID_FALSE;
                    *aIsUnknown = ID_TRUE;
                    IDE_RAISE( RETURN_SUCCESS );
                }
            }
            else
            {
                if( sdnIndexCTL::isMyTransaction( aTrans,
                                                  (sdpPhyPageHdr*)aNode,
                                                  STNDR_GET_LCTS_NO( sLeafKey ) )
                    == ID_TRUE )
                {
                    *aIsVisible = ID_FALSE;
                    *aIsUnknown = ID_TRUE;
                    IDE_RAISE( RETURN_SUCCESS );
                }
            }

            if( SM_SCN_IS_LT( &sCreateSCN, aStmtViewSCN ) )
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
                *aIsVisible = ID_TRUE;
                *aIsUnknown = ID_FALSE;
                IDE_RAISE( RETURN_SUCCESS );
            }
        }
        else
        {
            /* PROJ-1381 - Fetch Across Commits */
            if( SDC_CTS_SCN_IS_LEGACY(sLimitSCN) == ID_TRUE )
            {
                *aIsVisible = ID_FALSE;
                *aIsUnknown = ID_TRUE;
                IDE_RAISE( RETURN_SUCCESS );
            }

            if( SM_SCN_IS_LT( &sCreateSCN, aStmtViewSCN ) )
            {
                if( SM_SCN_IS_LT( aStmtViewSCN, &sLimitSCN ) )
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
                    *aIsVisible = ID_TRUE;
                    *aIsUnknown = ID_FALSE;
                    IDE_RAISE( RETURN_SUCCESS );
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
                    *aIsVisible = ID_FALSE;
                    *aIsUnknown = ID_FALSE;
                    IDE_RAISE( RETURN_SUCCESS );
                }
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
    *aIsVisible = ID_FALSE;
    *aIsUnknown = ID_FALSE;

    
    IDE_EXCEPTION_CONT( RETURN_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION :                                            *
 * ------------------------------------------------------------------*
 * Ŀ�� �������� Transaction�� Snapshot�� ���� Ű���� Ȯ���Ѵ�.
 * ������ ��쿡 �� �� �ִ� Ű�̴�.
 *
 * sCreateSCN < aInfiniteSCN < sLimitSCN
 *********************************************************************/
IDE_RC stndrRTree::cursorLevelVisibility( stndrLKey * aLeafKey,
                                          smSCN     * aInfiniteSCN,
                                          idBool    * aIsVisible )
{
    smSCN   sCreateSCN;
    smSCN   sLimitSCN;


    STNDR_GET_CSCN( aLeafKey, &sCreateSCN );

    if( SM_SCN_IS_CI_INFINITE( sCreateSCN ) == ID_TRUE )
    {
        SM_INIT_CI_SCN( &sCreateSCN );
    }

    STNDR_GET_LSCN( aLeafKey, &sLimitSCN );

    if( SM_SCN_IS_CI_INFINITE( sLimitSCN ) == ID_TRUE )
    {
        SM_MAX_CI_SCN( &sLimitSCN );
    }

    if( SM_SCN_IS_LT( &sCreateSCN, aInfiniteSCN ) )
    {
        if( SM_SCN_IS_LT( aInfiniteSCN, &sLimitSCN ) )
        {
            *aIsVisible = ID_TRUE;
            IDE_RAISE( RETURN_SUCCESS );
        }
    }

    *aIsVisible = ID_FALSE;

    IDE_EXCEPTION_CONT( RETURN_SUCCESS );
    
    return IDE_SUCCESS;
}

/*********************************************************************
 * FUNCTION DESCRIPTION :
 * ------------------------------------------------------------------*
 * Internal Node���� �ش� Child�� ����Ű�� Ű�� �����Ѵ�.
 * �� �Լ��� Child�κ��� Ű�� ������ Internal Node���� X-Latch�� ���� ����
 * ���� ����ȴ�.
 * �� �Լ��� ���� Internal Node�� ��� Ű�� ������ ��� ���� �θ� ����� Ű
 * ������ ��������� ��û�Ѵ�.
 *********************************************************************/
IDE_RC stndrRTree::deleteInternalKey( idvSQL            * aStatistics,
                                      stndrHeader       * aIndex,
                                      stndrStatistic    * aIndexStat,
                                      scPageID            aSegPID,
                                      scPageID            aChildPID,
                                      sdrMtx            * aMtx,
                                      stndrPathStack    * aStack,
                                      UInt              * aFreePageCount,
                                      idBool            * aIsRetry )
{
    UShort            sKeyCount;
    sdpPhyPageHdr   * sNode;
    SShort            sSeq;
    UChar           * sSlotDirPtr;
    UChar           * sFreeKey;
    UShort            sKeyLen;
    scPageID          sPID;
    stdMBR            sDummy;
    stdMBR            sNodeMBR;
    IDE_RC            sRc;

    
    // x latch current internal node
    IDE_TEST( getParentNode( aStatistics,
                             aIndexStat,
                             aMtx,
                             aIndex,
                             aStack,
                             aChildPID,
                             &sPID,
                             &sNode,
                             &sSeq,
                             &sDummy,
                             aIsRetry )
              != IDE_SUCCESS );

    IDE_TEST_RAISE( *aIsRetry == ID_TRUE, RETURN_SUCCESS );

    sSlotDirPtr = sdpPhyPage::getSlotDirStartPtr( (UChar*)sNode );
    sKeyCount   = sdpSlotDirectory::getCount( sSlotDirPtr );

    if( sKeyCount == 1 )
    {
        if( aStack->mDepth == 0 )
        {
            IDE_TEST( unsetRootNode( aStatistics,
                                     aMtx,
                                     aIndex,
                                     aIndexStat )
                      != IDE_SUCCESS );
        }
        else
        {
            aStack->mDepth--;
            IDE_TEST( deleteInternalKey( aStatistics,
                                         aIndex,
                                         aIndexStat,
                                         aSegPID,
                                         sPID,
                                         aMtx,
                                         aStack,
                                         aFreePageCount,
                                         aIsRetry )
                      != IDE_SUCCESS );

            IDE_TEST_RAISE( *aIsRetry == ID_TRUE, RETURN_SUCCESS );
        }

        IDE_TEST( freePage( aStatistics,
                            aIndexStat,
                            aIndex,
                            aMtx,
                            (UChar*)sNode )
                  != IDE_SUCCESS );
        
        aIndexStat->mNodeDeleteCount++;

        (*aFreePageCount)++;
    }
    else
    {
        // propagate key value
        IDE_ASSERT( adjustNodeMBR( aIndex,
                                   sNode,
                                   NULL,                   /* aInsertMBR */
                                   STNDR_INVALID_KEY_SEQ,  /* aUpdateKeySeq */
                                   NULL,                   /* aUpdateMBR */
                                   sSeq,                   /* aDeleteKeySeq */
                                   &sNodeMBR )
                    == IDE_SUCCESS );

        aStack->mDepth--;
        IDE_TEST( propagateKeyValue( aStatistics,
                                     aIndexStat,
                                     aIndex,
                                     aMtx,
                                     aStack,
                                     sNode,
                                     &sNodeMBR,
                                     aIsRetry )
                  != IDE_SUCCESS );

        IDE_TEST_RAISE( *aIsRetry == ID_TRUE, RETURN_SUCCESS );

        sRc = setNodeMBR( aMtx, sNode, &sNodeMBR );

        if( sRc != IDE_SUCCESS )
        {
            ideLog::log( IDE_SERVER_0, "update MBR : \n" );
            ideLog::logMem( IDE_SERVER_0,
                            (UChar*)&sNodeMBR,
                            ID_SIZEOF(stdMBR) );
            dumpIndexNode( sNode );
            IDE_ASSERT( 0 );
        }

        // free slot
        IDE_TEST( sdpSlotDirectory::getPagePtrFromSlotNum( sSlotDirPtr, 
                                                           sSeq,
                                                           &sFreeKey )
                  != IDE_SUCCESS );
        
        sKeyLen = getKeyLength( (UChar*)sFreeKey,
                                ID_FALSE /* aIsLeaf */);

        IDE_TEST( sdrMiniTrans::writeLogRec( aMtx,
                                             (UChar*)sFreeKey,
                                             (void*)&sKeyLen,
                                             ID_SIZEOF(UShort),
                                             SDR_STNDR_FREE_INDEX_KEY )
                  != IDE_SUCCESS );

        sdpPhyPage::freeSlot( sNode,
                              sSeq,
                              sKeyLen,
                              1  /* aSlotAlignValue */ );
    }

    IDE_EXCEPTION_CONT( RETURN_SUCCESS );
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION :                                            *
 * ------------------------------------------------------------------*
 * ��Ÿ���������� Root Node���� link�� �����Ѵ�.
 * Runtime Header�� Root Node�� SD_NULL_PID�� �����Ѵ�.
 *********************************************************************/
IDE_RC stndrRTree::unsetRootNode( idvSQL            * aStatistics,
                                  sdrMtx            * aMtx,
                                  stndrHeader       * aIndex,
                                  stndrStatistic    * aIndexStat )
{
    IDE_TEST( backupRuntimeHeader( aMtx, aIndex ) != IDE_SUCCESS );

    aIndex->mRootNode = SD_NULL_PID;

    IDE_TEST( setIndexMetaInfo( aStatistics,
                                aIndex,
                                aIndexStat,
                                aMtx,
                                &aIndex->mRootNode,
                                NULL,
                                NULL,
                                NULL,
                                NULL,
                                NULL,
                                NULL,
                                NULL )
              != IDE_SUCCESS );

    aIndex->mInitTreeMBR = ID_FALSE;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION :                                            *
 * ------------------------------------------------------------------*
 * �־��� Empty Node���� List�� �����Ѵ�.                            
 * ���ü� ������ ������ ��ġ ȹ���� �����ߴٸ� ���� ��ٷȴٰ�      
 * �ٽ� �õ��Ѵ�. 
 *********************************************************************/
IDE_RC stndrRTree::linkEmptyNodes( idvSQL           * aStatistics,
                                   void             * aTrans,
                                   stndrHeader      * aIndex,
                                   stndrStatistic   * aIndexStat,
                                   scPageID         * aEmptyNodePID )
{
    sdrMtx            sMtx;
    idBool            sMtxStart = ID_FALSE;
    stndrNodeHdr    * sNodeHdr;
    UInt              sEmptyNodeSeq;
    sdpPhyPageHdr   * sPage;
    idBool            sIsSuccess;

    for( sEmptyNodeSeq = 0; sEmptyNodeSeq < 2; )
    {
        if( aEmptyNodePID[sEmptyNodeSeq] == SD_NULL_PID )
        {
            sEmptyNodeSeq++;
            continue;
        }

        IDE_TEST( sdrMiniTrans::begin( aStatistics,
                                       &sMtx,
                                       aTrans,
                                       SDR_MTX_LOGGING,
                                       ID_FALSE,/*MtxUndoable(PROJ-2162)*/
                                       gMtxDLogType ) != IDE_SUCCESS );
        sMtxStart = ID_TRUE;
    
        IDE_TEST( stndrRTree::getPage( aStatistics,
                                       &(aIndexStat->mIndexPage),
                                       aIndex->mSdnHeader.mIndexTSID,
                                       aEmptyNodePID[sEmptyNodeSeq],
                                       SDB_X_LATCH,
                                       SDB_WAIT_NORMAL,
                                       &sMtx,
                                       (UChar**)&sPage,
                                       &sIsSuccess )
                  != IDE_SUCCESS );
        
        sNodeHdr = (stndrNodeHdr*)
            sdpPhyPage::getLogicalHdrStartPtr( (UChar*)sPage );
        
        if( sNodeHdr->mUnlimitedKeyCount != 0 )
        {
            sMtxStart = ID_FALSE;
            IDE_TEST( sdrMiniTrans::commit( &sMtx ) != IDE_SUCCESS );
            
            sEmptyNodeSeq++;
            continue;
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

        if( sIsSuccess == ID_TRUE )
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

    if( sMtxStart == ID_TRUE )
    {
        sMtxStart = ID_FALSE;
        (void)sdrMiniTrans::rollback( &sMtx );
    }

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION :                                            *
 * ------------------------------------------------------------------*
 * EMPTY LIST�� �ش� ��带 �����Ѵ�.                                
 * �̹� EMPTY LIST�� FREE LIST�� ����Ǿ� �ִ� ����� SKIP�ϰ�,    
 * �׷��� ���� ���� link�� �����Ѵ�.
 *********************************************************************/
IDE_RC stndrRTree::linkEmptyNode( idvSQL            * aStatistics,
                                  stndrStatistic    * aIndexStat,
                                  stndrHeader       * aIndex,
                                  sdrMtx            * aMtx,
                                  sdpPhyPageHdr     * aNode,
                                  idBool            * aIsSuccess )
{
    sdpPhyPageHdr   * sTailPage;
    scPageID          sPageID;
    stndrNodeHdr    * sCurNodeHdr;
    stndrNodeHdr    * sTailNodeHdr;
    idBool            sIsSuccess;
    UChar             sNodeState;
    scPageID          sNullPID = SD_NULL_PID;
    UChar           * sMetaPage;
    

    *aIsSuccess = ID_TRUE;

    sPageID  = sdpPhyPage::getPageID( (UChar*)aNode );
    
    sCurNodeHdr = (stndrNodeHdr*)
        sdpPhyPage::getLogicalHdrStartPtr( (UChar*)aNode );

    IDE_TEST_RAISE( (sCurNodeHdr->mState == STNDR_IN_EMPTY_LIST) ||
                    (sCurNodeHdr->mState == STNDR_IN_FREE_LIST),
                    SKIP_LINKING );

    /*
     * Index runtime Header�� empty node list������
     * Meta Page�� latch�� ���ؼ� ��ȣ�ȴ�.
     */
    sMetaPage = sdrMiniTrans::getPagePtrFromPageID(
        aMtx,
        aIndex->mSdnHeader.mIndexTSID,
        SD_MAKE_PID( aIndex->mSdnHeader.mMetaRID ));
    
    if( sMetaPage == NULL )
    {
        IDE_TEST( stndrRTree::getPage( aStatistics,
                                       &(aIndexStat->mMetaPage),
                                       aIndex->mSdnHeader.mIndexTSID,
                                       SD_MAKE_PID( aIndex->mSdnHeader.mMetaRID ),
                                       SDB_X_LATCH,
                                       SDB_WAIT_NORMAL,
                                       aMtx,
                                       (UChar**)&sMetaPage,
                                       &sIsSuccess )
                  != IDE_SUCCESS );
    }
    
    if( aIndex->mEmptyNodeHead == SD_NULL_PID )
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
                                                aIndex->mSdnHeader.mIndexTSID,
                                                aIndex->mEmptyNodeTail);
        if( sTailPage == NULL )
        {
            IDE_TEST( stndrRTree::getPage( aStatistics,
                                           &(aIndexStat->mIndexPage),
                                           aIndex->mSdnHeader.mIndexTSID,
                                           aIndex->mEmptyNodeTail,
                                           SDB_X_LATCH,
                                           SDB_WAIT_NO,
                                           aMtx,
                                           (UChar**)&sTailPage,
                                           aIsSuccess) != IDE_SUCCESS );

            IDE_TEST_RAISE( *aIsSuccess == ID_FALSE,
                            SKIP_LINKING );
        }
        
        sTailNodeHdr = (stndrNodeHdr*)
            sdpPhyPage::getLogicalHdrStartPtr((UChar*)sTailPage);

        IDE_TEST( sdrMiniTrans::writeNBytes(
                      aMtx,
                      (UChar*)&sTailNodeHdr->mNextEmptyNode,
                      (void*)&sPageID,
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

    sNodeState = STNDR_IN_EMPTY_LIST;
    
    IDE_TEST( sdrMiniTrans::writeNBytes(
                  aMtx,
                  (UChar*)&(sCurNodeHdr->mState),
                  (void*)&sNodeState,
                  ID_SIZEOF(sNodeState) ) != IDE_SUCCESS );
    
    IDE_TEST( sdrMiniTrans::writeNBytes(
                  aMtx,
                  (UChar*)&(sCurNodeHdr->mNextEmptyNode),
                  (void*)&sNullPID,
                  ID_SIZEOF(sNullPID) ) != IDE_SUCCESS );

    IDE_EXCEPTION_CONT( SKIP_LINKING );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION :                                            *
 * ------------------------------------------------------------------*
 * EMPTY LIST���� �ش� ��带 �����Ѵ�. 
 *********************************************************************/
IDE_RC stndrRTree::unlinkEmptyNode( idvSQL          * aStatistics,
                                    stndrStatistic  * aIndexStat,
                                    stndrHeader     * aIndex,
                                    sdrMtx          * aMtx,
                                    sdpPhyPageHdr   * aNode,
                                    UChar             aNodeState )
{
    stndrNodeHdr    * sNodeHdr;
    UChar           * sMetaPage;
    idBool            sIsSuccess = ID_TRUE;
    

    sNodeHdr = (stndrNodeHdr*)
        sdpPhyPage::getLogicalHdrStartPtr((UChar*)aNode);

    /*
     * FREE LIST�� �����ؾ� �ϴ� ��쿡�� �ݵ�� ���� ���°�
     * EMPTY LIST�� ����Ǿ� �ִ� ���¿��� �Ѵ�.
     */
    if( (aNodeState != STNDR_IN_USED) &&
        (sNodeHdr->mState != STNDR_IN_EMPTY_LIST) )
    {
        ideLog::log( IDE_SERVER_0,
                     "Node state : %u"
                     "\nNode header state : %u\n",
                     aNodeState, sNodeHdr->mState );
        dumpIndexNode( aNode );
        IDE_ASSERT( 0 );
    }
    
    IDE_TEST_RAISE( sNodeHdr->mState == STNDR_IN_USED,
                    SKIP_UNLINKING );
    
    if( aIndex->mEmptyNodeHead == SD_NULL_PID )
    {
        ideLog::log( IDE_SERVER_0, "Index header dump:\n" );
        ideLog::logMem( IDE_SERVER_0, (UChar *)aIndex, ID_SIZEOF(stndrHeader) );
        dumpIndexNode( aNode );
        IDE_ASSERT( 0 );
    }

    /*
     * Index runtime Header�� empty node list������
     * Meta Page�� latch�� ���ؼ� ��ȣ�ȴ�.
     */
    sMetaPage = sdrMiniTrans::getPagePtrFromPageID(
        aMtx,
        aIndex->mSdnHeader.mIndexTSID,
        SD_MAKE_PID(aIndex->mSdnHeader.mMetaRID) );
    
    if( sMetaPage == NULL )
    {
        IDE_TEST( stndrRTree::getPage( aStatistics,
                                       &(aIndexStat->mMetaPage),
                                       aIndex->mSdnHeader.mIndexTSID,
                                       SD_MAKE_PID( aIndex->mSdnHeader.mMetaRID ),
                                       SDB_X_LATCH,
                                       SDB_WAIT_NORMAL,
                                       aMtx,
                                       (UChar**)&sMetaPage,
                                       &sIsSuccess )
                  != IDE_SUCCESS );
    }
    
    aIndex->mEmptyNodeHead = sNodeHdr->mNextEmptyNode;

    if( sNodeHdr->mNextEmptyNode == SD_NULL_PID )
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
                                         (UChar*)&(sNodeHdr->mState),
                                         (void*)&aNodeState,
                                         ID_SIZEOF(aNodeState) )
              != IDE_SUCCESS );

    IDE_EXCEPTION_CONT( SKIP_UNLINKING );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION :                                            *
 * ------------------------------------------------------------------*
 * index segment�� ù��° ������(META PAGE)�� ����� Empty Node List��
 * ������ �α��Ѵ�. 
 *********************************************************************/
IDE_RC stndrRTree::setIndexEmptyNodeInfo( idvSQL            * aStatistics,
                                          stndrHeader       * aIndex,
                                          stndrStatistic    * aIndexStat,
                                          sdrMtx            * aMtx,
                                          scPageID          * aEmptyNodeHead,
                                          scPageID          * aEmptyNodeTail )
{
    UChar       * sPage;
    stndrMeta   * sMeta;
    idBool        sIsSuccess;
    

    sPage = sdrMiniTrans::getPagePtrFromPageID(
        aMtx,
        aIndex->mSdnHeader.mIndexTSID,
        SD_MAKE_PID( aIndex->mSdnHeader.mMetaRID ));
    
    if( sPage == NULL )
    {
        // SegHdr ������ �����͸� ����
        IDE_TEST( stndrRTree::getPage(
                      aStatistics,
                      &(aIndexStat->mMetaPage),
                      aIndex->mSdnHeader.mIndexTSID,
                      SD_MAKE_PID( aIndex->mSdnHeader.mMetaRID ),
                      SDB_X_LATCH,
                      SDB_WAIT_NORMAL,
                      aMtx,
                      (UChar**)&sPage,
                      &sIsSuccess ) != IDE_SUCCESS );
    }

    sMeta = (stndrMeta*)( sPage + SMN_INDEX_META_OFFSET );

    if( aEmptyNodeHead != NULL )
    {
        IDE_TEST( sdrMiniTrans::writeNBytes(
                      aMtx,
                      (UChar*)&sMeta->mEmptyNodeHead,
                      (void*)aEmptyNodeHead,
                      ID_SIZEOF(*aEmptyNodeHead) )
                  != IDE_SUCCESS );
    }
    
    if( aEmptyNodeTail != NULL )
    {
        IDE_TEST( sdrMiniTrans::writeNBytes(
                      aMtx,
                      (UChar*)&sMeta->mEmptyNodeTail,
                      (void*)aEmptyNodeTail,
                      ID_SIZEOF(*aEmptyNodeTail) )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : 
 * ------------------------------------------------------------------*
 * Unlimited Key�� 0�� ������ ã�´�. 
 * Split ���Ŀ� ȣ��Ǳ� ������ ����� ���� ��常�� �˻��Ѵ�. 
 *********************************************************************/
void stndrRTree::findEmptyNodes( sdrMtx         * aMtx,
                                 stndrHeader    * aIndex,
                                 sdpPhyPageHdr  * aStartPage,
                                 scPageID       * aEmptyNodePID,
                                 UInt           * aEmptyNodeCount )
{
    sdpPhyPageHdr   * sCurPage;
    sdpPhyPageHdr   * sPrevPage;
    stndrNodeHdr    * sNodeHdr;
    UInt              sEmptyNodeSeq = 0;
    UInt              i;
    scPageID          sNextPID;
    

    aEmptyNodePID[0] = SD_NULL_PID;
    aEmptyNodePID[1] = SD_NULL_PID;

    sCurPage = aStartPage;

    for( i = 0; i < 2; i++ )
    {
        if( sCurPage == NULL )
        {
            if( i == 1 )
            {
                ideLog::log( IDE_SERVER_0,
                             "Index TSID : %u"
                             ", Get page ID : %u\n",
                             aIndex->mSdnHeader.mIndexTSID, sNextPID );
                ideLog::logMem( IDE_SERVER_0,
                                (UChar *)aIndex, ID_SIZEOF(stndrHeader) );
                dumpIndexNode( sPrevPage );
            }
            IDE_ASSERT( 0 );
        }
        
        sNodeHdr = (stndrNodeHdr*)
            sdpPhyPage::getLogicalHdrStartPtr((UChar*)sCurPage);

        if( sNodeHdr->mUnlimitedKeyCount == 0 )
        {
            aEmptyNodePID[sEmptyNodeSeq] = sCurPage->mPageID;
            sEmptyNodeSeq++;
        }
        
        sNextPID = sdpPhyPage::getNxtPIDOfDblList( sCurPage );
        
        if( sNextPID != SD_NULL_PID )
        {
            sPrevPage = sCurPage;
            sCurPage = (sdpPhyPageHdr*)
                sdrMiniTrans::getPagePtrFromPageID( aMtx,
                                                    aIndex->mSdnHeader.mIndexTSID,
                                                    sNextPID );
        }
    }

    *aEmptyNodeCount = sEmptyNodeSeq;
}

/*********************************************************************
 * FUNCTION DESCRIPTION :
 * ------------------------------------------------------------------*
 * ���ο� Key�� Insert�ϱ� ���� Leaf Node�� Split�ϰ� Ű�� Insert�� Node��
 * ��ȯ�Ѵ�.
 *
 * Split �� ����Ǵ� ���� ������ ����.
 *  o insertKey �ÿ� Leaf Node�� ���ο� Ű�� ������ ������ ���� ���
 *  o deleteKey �ÿ� Leaf Node�� TBK Ÿ������ delete�� �����Ҷ� ������
 *    ���� ���
 *
 * Split ���(aSplitMode)�� �Ʒ� �ΰ����� �����ϸ� ����Ʈ�δ� RStart �����
 * Split�� ���� �Ѵ�.
 *********************************************************************/
IDE_RC stndrRTree::splitLeafNode( idvSQL            * aStatistics,
                                  stndrStatistic    * aIndexStat,
                                  sdrMtx            * aMtx,
                                  idBool            * aMtxStart,
                                  stndrHeader       * aIndex,
                                  smSCN             * aInfiniteSCN,
                                  stndrPathStack    * aStack,
                                  sdpPhyPageHdr     * aNode,
                                  stndrKeyInfo      * aKeyInfo,
                                  UShort              aKeyValueLen,
                                  SShort              aKeySeq,
                                  idBool              aIsInsert,
                                  UInt                aSplitMode,
                                  sdpPhyPageHdr    ** aTargetNode,
                                  SShort            * aTargetKeySeq,
                                  UInt              * aAllocPageCount,
                                  idBool            * aIsRetry )
{
    stndrKeyInfo      sOldNodeKeyInfo;
    stndrKeyInfo      sNewNodeKeyInfo;
    stndrKeyInfo    * sInsertKeyInfo;
    stndrKeyArray   * sKeyArray = NULL;
    UShort            sKeyArrayCnt;
    sdpPhyPageHdr   * sNewNode;
    idBool            sInsertKeyOnNewPage;
    idBool            sDeleteKeyOnNewPage;
    SShort            sInsertKeySeq;
    SShort            sDeleteKeySeq;
    SShort            sInsertKeyIdx;
    UShort            sSplitPoint;
    stdMBR            sOldNodeMBR;
    stdMBR            sNewNodeMBR;
    stdMBR            sNodeMBR;
    ULong             sPrevSmoNo;
    ULong             sNewSmoNo;
    IDE_RC            sRc;


    if( aIsInsert == ID_TRUE )
    {
        sInsertKeyInfo = aKeyInfo;
        sInsertKeySeq  = sdpSlotDirectory::getCount(
            sdpPhyPage::getSlotDirStartPtr((UChar*)aNode) );
        sDeleteKeySeq  = STNDR_INVALID_KEY_SEQ;
    }
    else
    {
        sInsertKeyInfo = NULL;
        sInsertKeySeq  = STNDR_INVALID_KEY_SEQ;
        sDeleteKeySeq  = aKeySeq;
    }
    
    // make split group
    IDE_TEST( makeKeyArray( NULL,                  /* aUpdateKeyInfo */
                            STNDR_INVALID_KEY_SEQ  /* aUpdateKeySeq */,
                            sInsertKeyInfo,
                            sInsertKeySeq,
                            aNode,
                            &sKeyArray,
                            &sKeyArrayCnt )
              != IDE_SUCCESS );

    makeSplitGroup( aIndex,
                    aSplitMode,
                    sKeyArray,
                    sKeyArrayCnt,
                    &sSplitPoint,
                    &sOldNodeMBR,
                    &sNewNodeMBR );

    adjustKeySeq( sKeyArray,
                  sKeyArrayCnt,
                  sSplitPoint,
                  &sInsertKeySeq,
                  NULL, /* aUpdateKeySeq */
                  &sDeleteKeySeq,
                  &sInsertKeyOnNewPage,
                  NULL, /* aUpdateKeyOnNewPage */
                  &sDeleteKeyOnNewPage );

    // move slot �ÿ� ����
    if( sInsertKeySeq != STNDR_INVALID_KEY_SEQ )
    {
        if( sInsertKeyOnNewPage == ID_TRUE )
        {
            sInsertKeyIdx = sInsertKeySeq + (sSplitPoint + 1);
        }
        else
        {
            sInsertKeyIdx = sInsertKeySeq;
        }

        sKeyArray[sInsertKeyIdx].mKeySeq = STNDR_INVALID_KEY_SEQ;
    }

    //
    sOldNodeKeyInfo.mKeyValue = (UChar *)&sOldNodeMBR;
    sNewNodeKeyInfo.mKeyValue = (UChar *)&sNewNodeMBR;

    // propagation
    aStack->mDepth--;
    IDE_TEST( propagateKeyInternalNode( aStatistics,
                                        aIndexStat,
                                        aMtx,
                                        aMtxStart,
                                        aIndex,
                                        aInfiniteSCN,
                                        aStack,
                                        aKeyValueLen,
                                        &sOldNodeKeyInfo,
                                        aNode,
                                        &sNewNodeKeyInfo,
                                        aSplitMode,
                                        &sNewNode,
                                        aAllocPageCount,
                                        aIsRetry )
              != IDE_SUCCESS );

    IDE_TEST_RAISE( *aIsRetry == ID_TRUE, SKIP_SPLIT_LEAF );

    // initialize new node
    sPrevSmoNo = sdpPhyPage::getIndexSMONo( aNode );

    IDE_TEST( increaseSmoNo( aStatistics, aIndex, &sNewSmoNo )
              != IDE_SUCCESS );

    sdpPhyPage::setIndexSMONo( (UChar*)aNode, sNewSmoNo );
    sdpPhyPage::setIndexSMONo( (UChar*)sNewNode, sPrevSmoNo );

    IDE_TEST( initializeNodeHdr( aMtx,
                                 sNewNode,
                                 0,
                                 aIndex->mSdnHeader.mLogging )
              != IDE_SUCCESS );

    IDE_TEST( sdnIndexCTL::init( aMtx,
                                 &aIndex->mSdnHeader.mSegmentDesc.mSegHandle,
                                 sNewNode,
                                 sdnIndexCTL::getUsedCount(aNode) )
              != IDE_SUCCESS );

    // distribute keys between old node and new node
    IDE_ASSERT( moveSlots( aMtx,
                           aIndex,
                           aNode,
                           sKeyArray,
                           sSplitPoint + 1,
                           sKeyArrayCnt - 1,
                           sNewNode ) == IDE_SUCCESS );

    // adjust node mbr
    if( adjustNodeMBR( aIndex,
                       aNode,
                       NULL,                   /* aInsertMBR */
                       STNDR_INVALID_KEY_SEQ,  /* aUpdateKeySeq */
                       NULL,                   /* aUpdateMBR */
                       STNDR_INVALID_KEY_SEQ,  /* aDeleteKeySeq */
                       &sNodeMBR ) == IDE_SUCCESS )
    {
        sRc = setNodeMBR( aMtx, aNode, &sNodeMBR );
        if( sRc != IDE_SUCCESS )
        {
            ideLog::log( IDE_SERVER_0, "update MBR : \n" );
            ideLog::logMem( IDE_SERVER_0,
                            (UChar*)&sNodeMBR,
                            ID_SIZEOF(stdMBR) );
            dumpIndexNode( aNode );
            IDE_ASSERT( 0 );
        }
    }

    if( adjustNodeMBR( aIndex,
                       sNewNode,
                       NULL,                   /* aInsertMBR */
                       STNDR_INVALID_KEY_SEQ,  /* aUpdateKeySeq */
                       NULL,                   /* aUpdateMBR */
                       STNDR_INVALID_KEY_SEQ,  /* aDeleteKeySeq */
                       &sNodeMBR ) == IDE_SUCCESS )
    {
        sRc = setNodeMBR( aMtx, sNewNode, &sNodeMBR );
        if( sRc != IDE_SUCCESS )
        {
            ideLog::log( IDE_SERVER_0, "update MBR : \n" );
            ideLog::logMem( IDE_SERVER_0,
                            (UChar*)&sNodeMBR,
                            ID_SIZEOF(stdMBR) );
            dumpIndexNode( sNewNode );
            IDE_ASSERT( 0 );
        }
    }

    // BUG-29560: �� ��忡 ���� �ι� Split �߻��� ���� ��ũ�� �������� �ʴ� ����
    sdpDblPIDList::setNxtOfNode( &sNewNode->mListNode,
                                 aNode->mListNode.mNext,
                                 aMtx );
    
    sdpDblPIDList::setNxtOfNode( &aNode->mListNode,
                                 sNewNode->mPageID,
                                 aMtx );

    // set target node
    if( aIsInsert == ID_TRUE )
    {
        *aTargetNode =
            ( sInsertKeyOnNewPage == ID_TRUE ) ? sNewNode : aNode;
        
        *aTargetKeySeq = sInsertKeySeq;
    }
    else
    {
        *aTargetNode =
            ( sDeleteKeyOnNewPage == ID_TRUE ) ? sNewNode : aNode;
        
        *aTargetKeySeq = sDeleteKeySeq;
    }

    IDE_EXCEPTION_CONT( SKIP_SPLIT_LEAF );

    if(sKeyArray != NULL)
    {
        (void)iduMemMgr::free( sKeyArray );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if(sKeyArray != NULL)
    {
        (void)iduMemMgr::free( sKeyArray );
    }

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION :
 * ------------------------------------------------------------------*
 * Node MBR�� �����Ѵ�.
 *********************************************************************/
IDE_RC stndrRTree::setNodeMBR( sdrMtx        * aMtx,
                               sdpPhyPageHdr * aNode,
                               stdMBR        * aMBR )
{
    stndrNodeHdr    * sNodeHdr;

    sNodeHdr = (stndrNodeHdr*)sdpPhyPage::getLogicalHdrStartPtr( (UChar*)aNode );
    
    IDE_TEST( sdrMiniTrans::writeNBytes(
                  aMtx,
                  (UChar*)&(sNodeHdr->mMBR.mMinX),
                  (void*)&aMBR->mMinX,
                  ID_SIZEOF(aMBR->mMinX))
              != IDE_SUCCESS );

    IDE_TEST( sdrMiniTrans::writeNBytes(
                  aMtx,
                  (UChar*)&(sNodeHdr->mMBR.mMinY),
                  (void*)&aMBR->mMinY,
                  ID_SIZEOF(aMBR->mMinY))
              != IDE_SUCCESS );

    IDE_TEST( sdrMiniTrans::writeNBytes(
                  aMtx,
                  (UChar*)&(sNodeHdr->mMBR.mMaxX),
                  (void*)&aMBR->mMaxX,
                  ID_SIZEOF(aMBR->mMaxX))
              != IDE_SUCCESS );

    IDE_TEST( sdrMiniTrans::writeNBytes(
                  aMtx,
                  (UChar*)&(sNodeHdr->mMBR.mMaxY),
                  (void*)&aMBR->mMaxY,
                  ID_SIZEOF(aMBR->mMaxY))
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION :
 * ------------------------------------------------------------------*
 * ����� Split���� ���� ���� ������ ���� ��忡 �����Ѵ�. ��尡 Split �Ǹ�
 * �� ���� ��尡 �����ǰ� �̷� ���� ���� ������ KeyInfo�� ���޵Ǿ� ȣ��ȴ�.
 * aUpdateKeyInfo ���� ���Ҵ�� ��� ������, aInsertKeyInfo ���� ���ҵǾ� 
 * ���� ������ ��� ������ ���޵ȴ�.
 *
 * �ֻ��� ��忡���� propgation �ȴٸ� Stack�� Depth�� -1�̴�. �� �� Root
 * Node�� ����Ǿ����� Ȯ���ϰ�, ����Ǿ��ٸ�, �ٽ� Insert�� �����ϵ��� �Ѵ�.
 * Ű ������ ���Ͽ� ��� Ž�� ��, Root Node�� ���� �����Ǿ��ٸ�, Stack��
 * �ش� Root ������ �����Ƿ� Ű ������ �ݿ����� ������ �ִ� ������ �ֱ�
 * �����̴�.
 *
 * ���� Stack Depth�� -1�� �ƴ϶�� ���� ��� ��带 ����Ű�� �θ� ��带
 * ã�� X-Latch�� ���, �θ� ��忡 ���� ��带 ����Ű�� Ű�� ������Ʈ�ϰ�
 * ���� ������ ��忡 ���� Ű�� �����Ѵ�.
 * 
 * �θ� ��忡 Ű�� ������ ������ ������ Split�� �����Ѵ�.
 * 
 * propagation�� ���� ������ �ֻ��� �θ� ������ ������ �ö󰡸� X-Latch��
 * ���� ���¿��� �ֻ��� ���κ��� ���� ��� �������� ���� �۾��� ����ȴ�.
 * Split�� ��� �������� ���ο� �ڽ� ��带 �Ҵ��Ͽ� �����ְ� �������� ������ 
 * �ڽ� ��带 ����Ѵ�.
 *
 * �θ� ��忡 Ű�� ������Ʈ �ǰų�, ���ο� Ű�� ���ԵǸ� ����� MBR�� �����
 * �� �����Ƿ�, ��� MBR�� ���� ������ �ݿ��Ѵ�.
 *********************************************************************/
IDE_RC stndrRTree::propagateKeyInternalNode( idvSQL         * aStatistics,
                                             stndrStatistic * aIndexStat,
                                             sdrMtx         * aMtx,
                                             idBool         * aMtxStart,
                                             stndrHeader    * aIndex,
                                             smSCN          * aInfiniteSCN,
                                             stndrPathStack * aStack,
                                             UShort           aKeyValueLen,
                                             stndrKeyInfo   * aUpdateKeyInfo,
                                             sdpPhyPageHdr  * aUpdateChildNode,
                                             stndrKeyInfo   * aInsertKeyInfo,
                                             UInt             aSplitMode,
                                             sdpPhyPageHdr ** aNewChildNode,
                                             UInt           * aAllocPageCount,
                                             idBool         * aIsRetry )
{
    sdpPhyPageHdr   * sNode = NULL;
    scPageID          sPID;
    scPageID          sNewChildPID;
    stndrNodeHdr    * sNodeHdr = NULL;
    SShort            sKeySeq;
    UShort            sKeyLength;
    idBool            sInsertable = ID_TRUE;
    stdMBR            sUpdateMBR;
    stdMBR            sInsertMBR;
    stdMBR            sNodeMBR;
    stdMBR            sDummy;
    IDE_RC            sRc;
    
    
    if( aStack->mDepth == -1 )
    {
        if( aStack->mStack[0].mNodePID != aIndex->mRootNode )
        {
            *aIsRetry = ID_TRUE;
            IDE_RAISE( RETURN_SUCCESS );
        }
        
        IDE_TEST( makeNewRootNode( aStatistics,
                                   aIndexStat,
                                   aMtx,
                                   aMtxStart,
                                   aIndex,
                                   aInfiniteSCN,
                                   aUpdateKeyInfo,
                                   aUpdateChildNode,
                                   aInsertKeyInfo,
                                   aKeyValueLen,
                                   aNewChildNode,
                                   aAllocPageCount )
                  != IDE_SUCCESS );
    }
    else
    {
        sKeyLength = STNDR_IKEY_LEN( aKeyValueLen );
                
        IDE_TEST( getParentNode( aStatistics,
                                 aIndexStat,
                                 aMtx,
                                 aIndex,
                                 aStack,
                                 sdpPhyPage::getPageID((UChar*)aUpdateChildNode),
                                 &sPID,
                                 &sNode,
                                 &sKeySeq,
                                 &sDummy,
                                 aIsRetry )
                  != IDE_SUCCESS );

        IDE_TEST_RAISE( *aIsRetry == ID_TRUE, RETURN_SUCCESS );

        // node�� ���� slot ������ �Ҵ�޴´�(sSlot)
        if( canAllocInternalKey( aMtx,
                                 aIndex,
                                 sNode,
                                 sKeyLength,
                                 ID_FALSE,
                                 ID_FALSE )
            != IDE_SUCCESS )
        {
            sInsertable = ID_FALSE;
        }

        if( sInsertable != ID_TRUE )
        {
            IDE_TEST( splitInternalNode( aStatistics,
                                         aIndexStat,
                                         aMtx,
                                         aMtxStart,
                                         aIndex,
                                         aInfiniteSCN,
                                         aStack,
                                         aKeyValueLen,
                                         aUpdateKeyInfo,
                                         sKeySeq,
                                         aInsertKeyInfo,
                                         aSplitMode,
                                         sNode,
                                         aNewChildNode,
                                         aAllocPageCount,
                                         aIsRetry )
                      != IDE_SUCCESS );
        }
        else
        {
            sNodeHdr = (stndrNodeHdr*)
                sdpPhyPage::getLogicalHdrStartPtr( (UChar*)sNode );
            
            IDE_TEST( preparePages( aStatistics,
                                    aIndex,
                                    aMtx,
                                    aMtxStart,
                                    sNodeHdr->mHeight )
                      != IDE_SUCCESS );
            
            IDE_ASSERT( allocPage( aStatistics,
                                   aIndexStat,
                                   aIndex,
                                   aMtx,
                                   (UChar**)aNewChildNode )
                        == IDE_SUCCESS );
            
            (*aAllocPageCount)++;

            sNewChildPID = sdpPhyPage::getPageID((UChar*)*aNewChildNode);

            STNDR_GET_MBR_FROM_KEYINFO( sUpdateMBR, aUpdateKeyInfo );
            STNDR_GET_MBR_FROM_KEYINFO( sInsertMBR, aInsertKeyInfo );

            IDE_ASSERT( adjustNodeMBR( aIndex,
                                       sNode,
                                       &sInsertMBR,
                                       sKeySeq,               /* aUpdateKeySeq */
                                       &sUpdateMBR,
                                       STNDR_INVALID_KEY_SEQ, /* aDeleteKeySeq */
                                       &sNodeMBR )
                        == IDE_SUCCESS );

            aStack->mDepth--;
            IDE_TEST( propagateKeyValue( aStatistics,
                                         aIndexStat,
                                         aIndex,
                                         aMtx,
                                         aStack,
                                         sNode,
                                         &sNodeMBR,
                                         aIsRetry )
                      != IDE_SUCCESS );

            IDE_TEST_RAISE( *aIsRetry == ID_TRUE, RETURN_SUCCESS );

            sRc = updateIKey( aMtx,
                              sNode,
                              sKeySeq,
                              &sUpdateMBR,
                              aIndex->mSdnHeader.mLogging );

            if( sRc != IDE_SUCCESS )
            {
                ideLog::log( IDE_SERVER_0,
                             "Key sequence : %d"
                             ", Key value length : %u"
                             ", Update MBR: \n",
                             sKeySeq, aKeyValueLen );
                ideLog::logMem( IDE_SERVER_0,
                                (UChar*)&sUpdateMBR,
                                ID_SIZEOF(sUpdateMBR) );
                dumpIndexNode( sNode );
                IDE_ASSERT( 0 );
            }

            sKeySeq = sdpSlotDirectory::getCount(
                sdpPhyPage::getSlotDirStartPtr((UChar*)sNode) );

            sRc = insertIKey( aMtx,
                              aIndex,
                              sNode,
                              sKeySeq,
                              aInsertKeyInfo,
                              aKeyValueLen,
                              sNewChildPID,
                              aIndex->mSdnHeader.mLogging );
            
            if( sRc != IDE_SUCCESS )
            {
                ideLog::log( IDE_SERVER_0,
                             "Key sequence : %d"
                             ", Key value length : %u"
                             ", New child PID : %u"
                             ", Key Info : \n",
                             sKeySeq, aKeyValueLen, sNewChildPID );
                ideLog::logMem( IDE_SERVER_0,
                                (UChar*)aInsertKeyInfo,
                                ID_SIZEOF(stndrKeyInfo) );
                
                dumpIndexNode( sNode );
                
                IDE_ASSERT( 0 );
            }

            sRc = setNodeMBR( aMtx, sNode, &sNodeMBR );
            if( sRc != IDE_SUCCESS )
            {
                ideLog::log( IDE_SERVER_0, "update MBR : \n" );
                ideLog::logMem( IDE_SERVER_0,
                                (UChar*)&sNodeMBR,
                                ID_SIZEOF(stdMBR) );
                dumpIndexNode( sNode );
                IDE_ASSERT( 0 );
            }
        }        
    }
    
    IDE_EXCEPTION_CONT( RETURN_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION :
 * ------------------------------------------------------------------*
 * Internal Node�� ���� Split�� �����Ѵ�. Leaf Node�� ���������� Split
 * Mode�� ���� RTree, RStar Tree ������� ���ҵȴ�.
 *
 * Split �ÿ� ������Ʈ �Ǵ� Ű�� ���ԵǴ� Ű ������ aUpdateKeyInfo,
 * aInsertKeyInfo�� ���� ���޵ȴ�. �� Ű���� ��� ����Ͽ� ��带 2����
 * Split �׷����� �����Ѵ�. ���ҵ� ��忡 ���� ������ KeyInfo�� �����Ͽ�
 * ���� ��忡 �����Ѵ�. propagateKeyInternalNode���� ���ҵ� �ڽ� ��带
 * ������ ���� �ְ�, ���� ��忡�� Split �׷쿡 ���� Ű�� ���ο� �ڽ� ����
 * �̵� ��Ų��.
 *
 * ��尡 ���ҵǾ� 2���� ��尡 �Ǵµ�, splitInternalNode ȣ��ÿ� ���޵�
 * Ű�� Split �׷쿡 ���� ����� ��ġ�� ���� �ݿ��Ѵ�. �̶� ��� MBR�� ���浵
 * ���� �ݿ��Ѵ�.
 *
 * R-Link�� ���� SmoNo�� ���� ��Ű��, ���ο� �ڽ� ��忡�� ��Ȱ��� �����
 * SmoNo�� �����ϰ�, ���� ��� ��忡�� ������Ų SmoNo�� ������ �� ����
 * ��� ����� ������ ��ũ�� ���� ������ �ڽ� ��带 ����Ű���� �Ѵ�.
 *********************************************************************/
IDE_RC stndrRTree::splitInternalNode( idvSQL            * aStatistics,
                                      stndrStatistic    * aIndexStat,
                                      sdrMtx            * aMtx,
                                      idBool            * aMtxStart,
                                      stndrHeader       * aIndex,
                                      smSCN             * aInfiniteSCN,
                                      stndrPathStack    * aStack,
                                      UShort              aKeyValueLen,
                                      stndrKeyInfo      * aUpdateKeyInfo,
                                      SShort              aUpdateKeySeq,
                                      stndrKeyInfo      * aInsertKeyInfo,
                                      UInt                aSplitMode,
                                      sdpPhyPageHdr     * aNode,
                                      sdpPhyPageHdr    ** aNewChildNode,
                                      UInt              * aAllocPageCount,
                                      idBool            * aIsRetry )
{
    stndrKeyArray   * sKeyArray = NULL;
    sdpPhyPageHdr   * sNewNode;
    sdpPhyPageHdr   * sTargetNode;
    UShort            sKeyArrayCnt;
    stndrKeyInfo      sOldNodeKeyInfo;
    stndrKeyInfo      sNewNodeKeyInfo;
    idBool            sInsertKeyOnNewPage;
    idBool            sUpdateKeyOnNewPage;
    SShort            sUpdateKeySeq = STNDR_INVALID_KEY_SEQ;
    SShort            sInsertKeySeq = STNDR_INVALID_KEY_SEQ;
    SShort            sInsertKeyIdx;
    UShort            sSplitPoint;
    stdMBR            sOldNodeMBR;
    stdMBR            sNewNodeMBR;
    stdMBR            sUpdateMBR;
    stdMBR            sNodeMBR;
    ULong             sPrevSmoNo;
    ULong             sNewSmoNo;
    stndrNodeHdr*     sNodeHdr;
    IDE_RC            sRc;


    sInsertKeySeq = sdpSlotDirectory::getCount(
        sdpPhyPage::getSlotDirStartPtr((UChar*)aNode) );
    
    sUpdateKeySeq = aUpdateKeySeq;

    // make split group
    IDE_TEST( makeKeyArray( aUpdateKeyInfo,
                            sUpdateKeySeq,
                            aInsertKeyInfo,
                            sInsertKeySeq,
                            aNode,
                            &sKeyArray,
                            &sKeyArrayCnt )
              != IDE_SUCCESS );

    makeSplitGroup( aIndex,
                    aSplitMode,
                    sKeyArray,
                    sKeyArrayCnt,
                    &sSplitPoint,
                    &sOldNodeMBR,
                    &sNewNodeMBR );
    
    adjustKeySeq( sKeyArray,
                  sKeyArrayCnt,
                  sSplitPoint,
                  &sInsertKeySeq,
                  &sUpdateKeySeq,
                  NULL, /* aDeleteKeySeq */
                  &sInsertKeyOnNewPage,
                  &sUpdateKeyOnNewPage,
                  NULL /* aDeteteKeyOnNewPage */ );

    // move slot �ÿ� ����
    if( sInsertKeyOnNewPage == ID_TRUE )
    {
        sInsertKeyIdx = sInsertKeySeq + (sSplitPoint + 1);
    }
    else
    {
        sInsertKeyIdx = sInsertKeySeq;
    }
    
    sKeyArray[sInsertKeyIdx].mKeySeq = STNDR_INVALID_KEY_SEQ;

    //
    sOldNodeKeyInfo.mKeyValue = (UChar *)&sOldNodeMBR;
    sNewNodeKeyInfo.mKeyValue = (UChar *)&sNewNodeMBR;
    
    // propagation
    aStack->mDepth--;
    IDE_TEST( propagateKeyInternalNode( aStatistics,
                                        aIndexStat,
                                        aMtx,
                                        aMtxStart,
                                        aIndex,
                                        aInfiniteSCN,
                                        aStack,
                                        aKeyValueLen,
                                        &sOldNodeKeyInfo,
                                        aNode,
                                        &sNewNodeKeyInfo,
                                        aSplitMode,
                                        &sNewNode,
                                        aAllocPageCount,
                                        aIsRetry )
              != IDE_SUCCESS );

    IDE_TEST_RAISE( *aIsRetry == ID_TRUE, SKIP_SPLIT_INTERNAL );

    // �� Child ��带 �Ҵ�޴´�.
    IDE_ASSERT( allocPage( aStatistics,
                           aIndexStat,
                           aIndex,
                           aMtx,
                           (UChar**)aNewChildNode )
                == IDE_SUCCESS );
    
    (*aAllocPageCount)++;

    // initialize new node
    sPrevSmoNo = sdpPhyPage::getIndexSMONo( aNode );

    IDE_TEST( increaseSmoNo( aStatistics, aIndex, &sNewSmoNo )
              != IDE_SUCCESS );

    sdpPhyPage::setIndexSMONo( (UChar*)aNode, sNewSmoNo );
    sdpPhyPage::setIndexSMONo( (UChar*)sNewNode, sPrevSmoNo );

    sNodeHdr = (stndrNodeHdr*)
        sdpPhyPage::getLogicalHdrStartPtr( (UChar*)aNode );

    IDE_TEST( initializeNodeHdr( aMtx,
                                 sNewNode,
                                 sNodeHdr->mHeight,
                                 aIndex->mSdnHeader.mLogging )
              != IDE_SUCCESS );
    
    // distribute keys between old node and new node
    sRc = moveSlots( aMtx,
                     aIndex,
                     aNode,
                     sKeyArray,
                     sSplitPoint + 1,
                     sKeyArrayCnt - 1,
                     sNewNode );
    
    if( sRc != IDE_SUCCESS )
    {
        ideLog::log( IDE_SERVER_0,
                     "move from <%d> to <%u>"
                     "\nIndex depth : %d"
                     ", key array count : %d"
                     ", key array : \n",
                     sSplitPoint + 1, sKeyArrayCnt - 1,
                     aStack->mDepth, sKeyArrayCnt );
        ideLog::logMem( IDE_SERVER_0,
                        (UChar*)sKeyArray,
                        ID_SIZEOF(stndrKeyArray) * sKeyArrayCnt );
        dumpIndexNode( aNode );
        dumpIndexNode( sNewNode );
        IDE_ASSERT( 0 );
    }

    // insert key
    sTargetNode = ( sInsertKeyOnNewPage == ID_TRUE ) ? sNewNode : aNode;
        
    sRc = insertIKey( aMtx,
                      aIndex,
                      sTargetNode,
                      sInsertKeySeq,
                      aInsertKeyInfo,
                      aKeyValueLen,
                      sdpPhyPage::getPageID((UChar*)*aNewChildNode),
                      aIndex->mSdnHeader.mLogging );

    if( sRc != IDE_SUCCESS )
    {
        ideLog::log( IDE_SERVER_0,
                     "Key sequence number : %d"
                     ", Key value length : %u"
                     ", New child PID : %u"
                     ", Key Info: \n",
                     sInsertKeySeq,
                     aKeyValueLen,
                     sdpPhyPage::getPageID((UChar*)*aNewChildNode) );
        ideLog::logMem( IDE_SERVER_0,
                        (UChar*)aInsertKeyInfo,
                        ID_SIZEOF(stndrKeyInfo) );
        dumpIndexNode( sTargetNode );
        IDE_ASSERT( 0 );
    }

    // Update Internal Key
    // �ݵ�� Ű ���� �� ������Ʈ�� �ϵ��� �Ѵ�.
    // �׷��� ������ �߸��� Key Seq�� ������Ʈ �� �� �ִ�.
    sTargetNode = ( sUpdateKeyOnNewPage == ID_TRUE ) ? sNewNode : aNode;
    STNDR_GET_MBR_FROM_KEYINFO( sUpdateMBR, aUpdateKeyInfo );
    sRc = updateIKey( aMtx,
                      sTargetNode,
                      sUpdateKeySeq,
                      &sUpdateMBR,
                      aIndex->mSdnHeader.mLogging );
    
    if( sRc != IDE_SUCCESS )
    {
        ideLog::log( IDE_SERVER_0,
                     "Key sequence number: %d"
                     ", Key value length : %u"
                     ", update MBR : \n",
                     sUpdateKeySeq, aKeyValueLen );
        ideLog::logMem( IDE_SERVER_0,
                        (UChar*)&sUpdateMBR,
                        ID_SIZEOF(sUpdateMBR) );
                
        dumpIndexNode( sTargetNode );
        IDE_ASSERT( 0 );
    }
    
    // adjust node mbr
    IDE_ASSERT( adjustNodeMBR( aIndex,
                               aNode,
                               NULL,                   /* aInsertMBR */
                               STNDR_INVALID_KEY_SEQ,  /* aUpdateKeySeq */
                               NULL,                   /* aUpdateMBR */
                               STNDR_INVALID_KEY_SEQ,  /* aDeleteKeySeq */
                               &sNodeMBR ) == IDE_SUCCESS );
    
    sRc = setNodeMBR( aMtx, aNode, &sNodeMBR );
    if( sRc != IDE_SUCCESS )
    {
        ideLog::log( IDE_SERVER_0, "update MBR : \n" );
        ideLog::logMem( IDE_SERVER_0,
                        (UChar*)&sNodeMBR,
                        ID_SIZEOF(stdMBR) );
        dumpIndexNode( aNode );
        IDE_ASSERT( 0 );
    }
    
    IDE_ASSERT( adjustNodeMBR( aIndex,
                               sNewNode,
                               NULL,                   /* aInsertMBR */
                               STNDR_INVALID_KEY_SEQ,  /* aUpdateKeySeq */
                               NULL,                   /* aUpdateMBR */
                               STNDR_INVALID_KEY_SEQ,  /* aDeleteKeySeq */
                               &sNodeMBR ) == IDE_SUCCESS );
    
    sRc = setNodeMBR( aMtx, sNewNode, &sNodeMBR );
    if( sRc != IDE_SUCCESS )
    {
        ideLog::log( IDE_SERVER_0, "update MBR : \n" );
        ideLog::logMem( IDE_SERVER_0,
                        (UChar*)&sNodeMBR,
                        ID_SIZEOF(stdMBR) );
        dumpIndexNode( sNewNode );
        IDE_ASSERT( 0 );
    }

    // BUG-29560: �� ��忡 ���� �ι� Split �߻��� ���� ��ũ�� �������� �ʴ� ����
    sdpDblPIDList::setNxtOfNode( &sNewNode->mListNode,
                                 aNode->mListNode.mNext,
                                 aMtx );

    sdpDblPIDList::setNxtOfNode( &aNode->mListNode,
                                 sNewNode->mPageID,
                                 aMtx );

    IDE_EXCEPTION_CONT( SKIP_SPLIT_INTERNAL );
    
    if( sKeyArray != NULL )
    {
        IDE_TEST( iduMemMgr::free( sKeyArray ) != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if( sKeyArray != NULL )
    {
        (void)iduMemMgr::free( sKeyArray );
    }

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION :
 * ------------------------------------------------------------------*
 * ���� �Ǵ� ��忡�� ���ϵ� Ű, ������ Ű, ������Ʈ �� Ű�� ������ �� �ִ�.
 * Split�� ����Ǹ� �̵��� KeySeq�� ����� �� ������, Split ���� Key Seq��
 * �������ش�.
 *********************************************************************/
void stndrRTree::adjustKeySeq( stndrKeyArray    * aKeyArray,
                               UShort             aKeyArrayCnt,
                               UShort             aSplitPoint,
                               SShort           * aInsertKeySeq,
                               SShort           * aUpdateKeySeq,
                               SShort           * aDeleteKeySeq,
                               idBool           * aInsertKeyOnNewPage,
                               idBool           * aUpdateKeyOnNewPage,
                               idBool           * aDeleteKeyOnNewPage )
{
    SShort  sNewInsertKeySeq = STNDR_INVALID_KEY_SEQ;
    SShort  sNewUpdateKeySeq = STNDR_INVALID_KEY_SEQ;
    SShort  sNewDeleteKeySeq = STNDR_INVALID_KEY_SEQ;
    SShort  sInsertKeySeq = STNDR_INVALID_KEY_SEQ;
    SShort  sUpdateKeySeq = STNDR_INVALID_KEY_SEQ;
    SShort  sDeleteKeySeq = STNDR_INVALID_KEY_SEQ;
    idBool  sInsertKeyOnNewPage = ID_TRUE;
    idBool  sUpdateKeyOnNewPage = ID_TRUE;
    idBool  sDeleteKeyOnNewPage = ID_TRUE;
    UInt    i;

    if( aInsertKeySeq != NULL)
    {
        sInsertKeySeq = *aInsertKeySeq;
    }

    if( aUpdateKeySeq != NULL)
    {
        sUpdateKeySeq = *aUpdateKeySeq;
    }

    if( aDeleteKeySeq != NULL)
    {
        sDeleteKeySeq = *aDeleteKeySeq;
    }
    
    sUpdateKeyOnNewPage = ID_TRUE;
    sInsertKeyOnNewPage = ID_TRUE;
    sDeleteKeyOnNewPage = ID_TRUE;
    
    for( i = 0; i <= aSplitPoint; i++ )
    {
        if( aKeyArray[i].mKeySeq == sInsertKeySeq )
        {
            sNewInsertKeySeq = i;
            sInsertKeyOnNewPage = ID_FALSE;
        }

        if( aKeyArray[i].mKeySeq == sUpdateKeySeq )
        {
            sNewUpdateKeySeq = i;
            sUpdateKeyOnNewPage = ID_FALSE;
        }

        if( aKeyArray[i].mKeySeq == sDeleteKeySeq )
        {
            sNewDeleteKeySeq = i;
            sDeleteKeyOnNewPage = ID_FALSE;
        }
    }

    for( i = aSplitPoint + 1; i < aKeyArrayCnt; i++ )
    {
        if( aKeyArray[i].mKeySeq == sInsertKeySeq )
        {
            sNewInsertKeySeq = i - (aSplitPoint + 1);
        }

        if( aKeyArray[i].mKeySeq == sUpdateKeySeq )
        {
            sNewUpdateKeySeq = i - (aSplitPoint + 1);
        }

        if( aKeyArray[i].mKeySeq == sDeleteKeySeq )
        {
            sNewDeleteKeySeq = i - (aSplitPoint + 1 );
        }
    }

    // set value
    if( aInsertKeySeq != NULL )
    {
        *aInsertKeySeq = sNewInsertKeySeq;
    }

    if( aUpdateKeySeq != NULL )
    {
        *aUpdateKeySeq = sNewUpdateKeySeq;
    }

    if( aDeleteKeySeq != NULL )
    {
        *aDeleteKeySeq = sNewDeleteKeySeq;
    }

    if( aInsertKeyOnNewPage != NULL )
    {
        *aInsertKeyOnNewPage = sInsertKeyOnNewPage;
    }

    if( aUpdateKeyOnNewPage != NULL )
    {
        *aUpdateKeyOnNewPage = sUpdateKeyOnNewPage;
    }
    
    if( aDeleteKeyOnNewPage != NULL )
    {
        *aDeleteKeyOnNewPage = sDeleteKeyOnNewPage;
    }
}

/*********************************************************************
 * FUNCTION DESCRIPTION :
 * ------------------------------------------------------------------*
 * ��带 �����ϱ� ���� Key Array�� �Ҵ��Ѵ�. �Ҵ�� Key Array�� ���Ͽ�
 * Split �׷��� �����ȴ�. 
 *********************************************************************/
IDE_RC stndrRTree::makeKeyArray( stndrKeyInfo   * aUpdateKeyInfo,
                                 SShort           aUpdateKeySeq,
                                 stndrKeyInfo   * aInsertKeyInfo,
                                 SShort           aInsertKeySeq,
                                 sdpPhyPageHdr  * aNode,
                                 stndrKeyArray ** aKeyArray,
                                 UShort         * aKeyArrayCnt )
{
    UChar           * sSlotDirPtr;
    UShort            sKeyCount;
    UShort            sIdx;
    UShort            sKeyArrayCnt;
    stndrKeyArray   * sKeyArray = NULL;
    stndrNodeHdr    * sNodeHdr;
    stndrIKey       * sIKey;
    stndrLKey       * sLKey;
    stdMBR            sMBR;
    SInt              i;

    
    *aKeyArray  = NULL;

    sNodeHdr    = (stndrNodeHdr*)sdpPhyPage::getLogicalHdrStartPtr( (UChar*)aNode );
    sSlotDirPtr = sdpPhyPage::getSlotDirStartPtr( (UChar*)aNode );
    sKeyCount   = sdpSlotDirectory::getCount( sSlotDirPtr );

    if( aInsertKeyInfo != NULL )
    {
        sKeyArrayCnt = sKeyCount + 1;
    }
    else
    {
        sKeyArrayCnt = sKeyCount;
    }
    
    IDE_TEST( iduMemMgr::malloc( IDU_MEM_ST_STN,
                                 ID_SIZEOF(stndrKeyArray) * sKeyArrayCnt,
                                 (void **)&sKeyArray,
                                 IDU_MEM_IMMEDIATE ) != IDE_SUCCESS );

    sIdx = 0;
    if( STNDR_IS_LEAF_NODE(sNodeHdr) == ID_TRUE )
    {
        for( i = 0; i < sKeyCount; i++ )
        {
            if( (aUpdateKeySeq != STNDR_INVALID_KEY_SEQ) && (aUpdateKeySeq == i) )
            {
                IDE_ASSERT( aUpdateKeyInfo != NULL );
                STNDR_GET_MBR_FROM_KEYINFO( sMBR, aUpdateKeyInfo );
            }
            else
            {
                IDE_TEST( sdpSlotDirectory::getPagePtrFromSlotNum( 
                                                                sSlotDirPtr, 
                                                                i,
                                                                (UChar**)&sLKey)
                          != IDE_SUCCESS );
                STNDR_GET_MBR_FROM_LKEY( sMBR, sLKey );
            }
            
            sKeyArray[sIdx].mKeySeq = i;
            sKeyArray[sIdx].mMBR = sMBR;
            sIdx++;
        }
    }
    else
    {
        for( i = 0; i < sKeyCount; i++ )
        {
            if( (aUpdateKeySeq != STNDR_INVALID_KEY_SEQ) && (aUpdateKeySeq == i) )
            {
                IDE_ASSERT( aUpdateKeyInfo != NULL );
                STNDR_GET_MBR_FROM_KEYINFO( sMBR, aUpdateKeyInfo );
            }
            else
            {
                IDE_TEST( sdpSlotDirectory::getPagePtrFromSlotNum( 
                                                            sSlotDirPtr, 
                                                            i,
                                                            (UChar**)&sIKey )
                          != IDE_SUCCESS );
                STNDR_GET_MBR_FROM_IKEY( sMBR, sIKey );
            }
            
            sKeyArray[sIdx].mKeySeq = i;
            sKeyArray[sIdx].mMBR = sMBR;
            sIdx++;
        }
    }

    if( aInsertKeyInfo != NULL )
    {
        sKeyArray[sIdx].mKeySeq = aInsertKeySeq;
        STNDR_GET_MBR_FROM_KEYINFO( sKeyArray[sIdx].mMBR, aInsertKeyInfo );
    }

    *aKeyArrayCnt = sKeyArrayCnt;
    *aKeyArray = sKeyArray;
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if( sKeyArray != NULL )
    {
        (void)iduMemMgr::free( (void*)sKeyArray );
    }

    *aKeyArrayCnt = 0;
    *aKeyArray = NULL;

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION :
 * ------------------------------------------------------------------*
 * Split Mode�� ���� Split �׷��� �����Ѵ�.
 *********************************************************************/
void stndrRTree::makeSplitGroup( stndrHeader    * aIndex,
                                 UInt             aSplitMode,
                                 stndrKeyArray  * aKeyArray,
                                 UShort           aKeyArrayCnt,
                                 UShort         * aSplitPoint,
                                 stdMBR         * aOldGroupMBR,
                                 stdMBR         * aNewGroupMBR )
{
    stdMBR  sMBR;
    UInt    i;


    IDE_ASSERT( aKeyArrayCnt > 2 );
    
    if( aSplitMode == STNDR_SPLIT_MODE_RTREE )
    {
        splitByRTreeWay( aIndex,
                         aKeyArray,
                         aKeyArrayCnt,
                         aSplitPoint );
    }
    else
    {
        splitByRStarWay( aIndex,
                         aKeyArray,
                         aKeyArrayCnt,
                         aSplitPoint );
    }

    // calculate group mbr
    stdUtils::copyMBR( &sMBR, &aKeyArray[0].mMBR );
    
    for( i = 0; i <= *aSplitPoint; i++ )
    {
        stdUtils::getMBRExtent(
            &sMBR,
            &aKeyArray[i].mMBR );
    }

    *aOldGroupMBR = sMBR;

    stdUtils::copyMBR( &sMBR, &aKeyArray[*aSplitPoint +1 ].mMBR );
    
    for( i = *aSplitPoint + 1; i < aKeyArrayCnt; i++ )
    {
        stdUtils::getMBRExtent(
            &sMBR,
            &aKeyArray[i].mMBR );
    }

    *aNewGroupMBR = sMBR;

    return;
}

/*********************************************************************
 * FUNCTION DESCRIPTION :
 * ------------------------------------------------------------------*
 * RStar ������� Split �׷��� �����Ѵ�.
 *********************************************************************/
void stndrRTree::splitByRStarWay( stndrHeader   * aIndex,
                                  stndrKeyArray * aKeyArray,
                                  UShort          aKeyArrayCnt,
                                  UShort        * aSplitPoint )
{
    stndrKeyArray   sKeyArrayX[STNDR_MAX_KEY_COUNT];
    stndrKeyArray   sKeyArrayY[STNDR_MAX_KEY_COUNT];
    SDouble         sSumPerimeterX;
    SDouble         sSumPerimeterY;
    UShort          sSplitPointX;
    UShort          sSplitPointY;


    IDE_ASSERT( aKeyArrayCnt <= STNDR_MAX_KEY_COUNT );

    idlOS::memcpy( sKeyArrayX, aKeyArray,
                   ID_SIZEOF(stndrKeyArray) * aKeyArrayCnt );
    
    idlOS::memcpy( sKeyArrayY, aKeyArray,
                   ID_SIZEOF(stndrKeyArray) * aKeyArrayCnt );

    // sort
    quickSort( sKeyArrayX,
               aKeyArrayCnt,
               gCompareKeyArrayByAxisX );

    quickSort( sKeyArrayY,
               aKeyArrayCnt,
               gCompareKeyArrayByAxisY );

    // get split infomation from each axis
    getSplitInfo( aIndex,
                  sKeyArrayX,
                  aKeyArrayCnt,
                  &sSplitPointX,
                  &sSumPerimeterX );
    
    getSplitInfo( aIndex,
                  sKeyArrayY,
                  aKeyArrayCnt,
                  &sSplitPointY,
                  &sSumPerimeterY );

    if( sSumPerimeterX < sSumPerimeterY )
    {
        idlOS::memcpy( aKeyArray, sKeyArrayX,
                       ID_SIZEOF(stndrKeyArray) * aKeyArrayCnt );

        *aSplitPoint = sSplitPointX;
    }
    else
    {
        idlOS::memcpy( aKeyArray, sKeyArrayY,
                       ID_SIZEOF(stndrKeyArray) * aKeyArrayCnt );

        *aSplitPoint = sSplitPointY;
    }

    return;
}

/*********************************************************************
 * FUNCTION DESCRIPTION :
 * ------------------------------------------------------------------*
 * �⺻���� RTree ������� Split �׷��� �����Ѵ�.
 *********************************************************************/
void stndrRTree::splitByRTreeWay( stndrHeader   * aIndex,
                                  stndrKeyArray * aKeyArray,
                                  UShort          aKeyArrayCnt,
                                  UShort        * aSplitPoint )
{
    stndrKeyArray   sTmpKeyArray[STNDR_MAX_KEY_COUNT];
    stndrKeyArray   sKeyArraySeed0[STNDR_MAX_KEY_COUNT];
    stndrKeyArray   sKeyArraySeed1[STNDR_MAX_KEY_COUNT];
    stndrKeyArray   sKey;
    stndrKeyArray   sKeySeed0;
    stndrKeyArray   sKeySeed1;
    UShort          sKeyCntSeed0 = 0;
    UShort          sKeyCntSeed1 = 0;
    stdMBR          sMBRSeed0;
    stdMBR          sMBRSeed1;
    UShort          sMinCnt;
    UShort          sSeed;
    SShort          sPos;
    SInt            i;
    UInt            j;

    IDE_ASSERT( aKeyArrayCnt <= STNDR_MAX_KEY_COUNT );

    idlOS::memcpy( sTmpKeyArray, aKeyArray,
                   ID_SIZEOF(stndrKeyArray) * aKeyArrayCnt );

    pickSeed( aIndex,
              sTmpKeyArray,
              aKeyArrayCnt,
              &sKeySeed0,
              &sKeySeed1,
              &sPos );

    sKeyArraySeed0[sKeyCntSeed0++] = sKeySeed0;
    sKeyArraySeed1[sKeyCntSeed1++] = sKeySeed1;

    sMBRSeed0 = sKeySeed0.mMBR;
    sMBRSeed1 = sKeySeed1.mMBR;

    sMinCnt = aKeyArrayCnt / 2;

    while( sPos >= 0 )
    {
        pickNext( aIndex,
                  sTmpKeyArray,
                  &sMBRSeed0,
                  &sMBRSeed1,
                  &sPos,
                  &sKey,
                  &sSeed );

        if( sKeyCntSeed0 > sMinCnt )
        {
            sSeed = 1;
        }
        
        if( sSeed == 0 )
        {
            sKeyArraySeed0[sKeyCntSeed0++] = sKey;
            
            stdUtils::getMBRExtent(
                &sMBRSeed0,
                &sKey.mMBR );
        }
        else
        {
            sKeyArraySeed1[sKeyCntSeed1++] = sKey;
            
            stdUtils::getMBRExtent(
                &sMBRSeed1,
                &sKey.mMBR);
        }
    }

    IDE_ASSERT( aKeyArrayCnt == (sKeyCntSeed0 + sKeyCntSeed1) );

    for( i = 0, j = 0; i < sKeyCntSeed0; i++ )
    {
        aKeyArray[j++] = sKeyArraySeed0[i];
    }
    
    *aSplitPoint = j - 1;
    
    for( i = 0; i < sKeyCntSeed1; i++ )
    {
        aKeyArray[j++] = sKeyArraySeed1[i];
    }

    return;
}

/*********************************************************************
 * FUNCTION DESCRIPTION :
 * ------------------------------------------------------------------*
 * RTree ����� Split���� ���ȴ�. Dead Space�� ���� ū Ű ���� �����Ѵ�.
 * ���õ� 2���� Ű�� Seed�� �����ϰ� Key Array�� �� ������ �̵���Ų��.
 *********************************************************************/
void stndrRTree::pickSeed( stndrHeader      * /*aIndex*/,
                           stndrKeyArray    * aArray,
                           UShort             aArrayCount,
                           stndrKeyArray    * aKeySeed0,
                           stndrKeyArray    * aKeySeed1,
                           SShort           * aPos )
{
    stndrKeyArray   sTmpKey;
    SDouble         sMaxArea;
    SDouble         sTmpArea;
    stdMBR          sTmpMBR;
    UShort          sSeed0;
    UShort          sSeed1;
    UShort          i;
    UShort          j;


    IDE_ASSERT( aArrayCount >= 2 );

    // init seed
    sSeed0 = 0;
    sSeed1 = 1;
    
    sTmpMBR = aArray[sSeed0].mMBR;
            
    sTmpArea = 
        stdUtils::getMBRArea(
            stdUtils::getMBRExtent(
                &sTmpMBR,
                &aArray[sSeed1].mMBR ) );

    sTmpArea -= (stdUtils::getMBRArea(&aArray[sSeed0].mMBR) +
                 stdUtils::getMBRArea(&aArray[sSeed1].mMBR));

    sMaxArea = sTmpArea;

    // find best seed
    for( i = 0; i < aArrayCount - 1 ; i++ )
    {
        for( j = i + 1; j < aArrayCount; j++ )
        {
            sTmpMBR = aArray[i].mMBR;
            
            sTmpArea = 
                stdUtils::getMBRArea(
                    stdUtils::getMBRExtent( &sTmpMBR,
                                            &aArray[j].mMBR ) );

            sTmpArea -= (stdUtils::getMBRArea(&aArray[i].mMBR) +
                         stdUtils::getMBRArea(&aArray[j].mMBR));
            
            if( sTmpArea > sMaxArea )
            {
                sMaxArea = sTmpArea;
                sSeed0 = i;
                sSeed1 = j;
            }
        }
    }

    sTmpKey                 = aArray[sSeed0];
    aArray[sSeed0]          = aArray[aArrayCount - 2];
    aArray[aArrayCount - 2] = sTmpKey;

    sTmpKey                 = aArray[sSeed1];
    aArray[sSeed1]          = aArray[aArrayCount - 1];
    aArray[aArrayCount - 1] = sTmpKey;

    *aKeySeed0 = aArray[aArrayCount - 1];
    *aKeySeed1 = aArray[aArrayCount - 2];

    *aPos = aArrayCount - 3;
}

/*********************************************************************
 * FUNCTION DESCRIPTION :
 * ------------------------------------------------------------------*
 * RTree ����� Split���� ���ȴ�. Key Array���� �� Seed�� ���Եɶ� ����
 * ���� Ȯ���� ���� Ű�� �����Ѵ�.
 *********************************************************************/
void stndrRTree::pickNext( stndrHeader      * /*aIndex*/,
                           stndrKeyArray    * aArray,
                           stdMBR           * aMBRSeed0,
                           stdMBR           * aMBRSeed1,
                           SShort           * aPos,
                           stndrKeyArray    * aKey,
                           UShort           * aSeed )
{
    SDouble         sDeltaA;
    SDouble         sDeltaB;
    SDouble         sMinDelta;
    stndrKeyArray   sTmpKey;
    SInt            sMin;
    SInt            i;

    
    sDeltaA = stdUtils::getMBRDelta( &aArray[0].mMBR, aMBRSeed0 );
    sDeltaB = stdUtils::getMBRDelta( &aArray[0].mMBR, aMBRSeed1 );

    if( sDeltaA >  sDeltaB )
    {
        sMinDelta = sDeltaB;
        *aSeed = 1;
        sMin = 0;        
    }
    else
    {
        sMinDelta = sDeltaA;
        *aSeed = 0;
        sMin = 0;
    }
    
    for( i = 0; i < (*aPos); i++ )
    {
        sDeltaA = stdUtils::getMBRDelta( &aArray[i].mMBR, aMBRSeed0 );
        sDeltaB = stdUtils::getMBRDelta( &aArray[i].mMBR, aMBRSeed1 );
        
        if( STNDR_MIN(sDeltaA, sDeltaB) < sMinDelta )
        {
            sMin = i;
            
            if( sDeltaA < sDeltaB )
            {
                sMinDelta = sDeltaA;
                *aSeed = 0;
            }
            else
            {
                sMinDelta = sDeltaB;
                *aSeed = 1;
            }
        }
    }

    *aKey        = aArray[sMin];
    
    sTmpKey      = aArray[*aPos];
    aArray[sMin] = sTmpKey;

    (*aPos)--;
}

/*********************************************************************
 * FUNCTION DESCRIPTION :
 * ------------------------------------------------------------------*
 * aSrcNode�κ��� aDstNode�� slot���� �ű�� �α��Ѵ�.
 *  1. Copy Source Key to Destination Node
 *  2. Unbind Source Key
 *  3. Free Source Key and Adjust UnlimitedKeyCount
 *********************************************************************/
IDE_RC stndrRTree::moveSlots( sdrMtx            * aMtx,
                              stndrHeader       * aIndex,
                              sdpPhyPageHdr     * aSrcNode,
                              stndrKeyArray     * aKeyArray,
                              UShort              aFromIdx,
                              UShort              aToIdx,
                              sdpPhyPageHdr     * aDstNode )
{
    SInt                  i;
    SShort                sSeq;
    SShort                sDstSeq;
    UShort                sAllowedSize;
    UChar               * sSlotDirPtr;
    stndrLKey           * sSrcKey;
    stndrLKey           * sDstKey;
    UChar                 sSrcCreateCTS;
    UChar                 sSrcLimitCTS;
    UChar                 sDstCreateCTS;
    UChar                 sDstLimitCTS;
    UInt                  sKeyLen  = 0;
    UShort                sUnlimitedKeyCount = 0;
    UShort                sKeyOffset;
    stndrNodeHdr        * sSrcNodeHdr;
    stndrNodeHdr        * sDstNodeHdr;
    UShort                sDstTBKCount = 0;
    IDE_RC                sRc;

    sSrcNodeHdr = (stndrNodeHdr*)
        sdpPhyPage::getLogicalHdrStartPtr( (UChar*)aSrcNode );
    
    sDstNodeHdr = (stndrNodeHdr*)
        sdpPhyPage::getLogicalHdrStartPtr( (UChar*)aDstNode );

    sSlotDirPtr = sdpPhyPage::getSlotDirStartPtr( (UChar*)aSrcNode );

    // Leaf Node�� �׻� Compaction�� �Ϸ�� ���¿��� �Ѵ�.
    IDE_DASSERT( ( STNDR_IS_LEAF_NODE(sSrcNodeHdr) == ID_FALSE )  ||
                 ( getNonFragFreeSize(aIndex, aSrcNode) ==
                   getTotalFreeSize(aIndex, aSrcNode) ) );

    // Copy Source Key to Destination Node
    sUnlimitedKeyCount = sDstNodeHdr->mUnlimitedKeyCount;
    sDstSeq = -1;

    for( i = aFromIdx; i <= aToIdx; i++ )
    {
        sSeq = aKeyArray[i].mKeySeq;

        // ���� ���ԵǴ� Ű�� �Ѿ
        if( sSeq == STNDR_INVALID_KEY_SEQ )
        {
            continue;
        }
        
        IDE_TEST(sdpSlotDirectory::getPagePtrFromSlotNum( sSlotDirPtr, 
                                                          sSeq,
                                                          (UChar**)&sSrcKey )
                 != IDE_SUCCESS );

        sDstSeq++;
        
        if( STNDR_IS_LEAF_NODE(sSrcNodeHdr) == ID_TRUE )
        {
            sKeyLen = getKeyLength( (UChar*)sSrcKey,  ID_TRUE );

            sRc = canAllocLeafKey( aMtx,
                                   aIndex,
                                   aDstNode,
                                   sKeyLen,
                                   NULL );
            
            if( sRc != IDE_SUCCESS )
            {
                ideLog::log( IDE_SERVER_0,
                             "From <%d> to <%d> : "
                             "Current sequence number : %d"
                             ", KeyLen %u\n",
                             aFromIdx, aToIdx, i, sKeyLen );
                ideLog::logMem( IDE_SERVER_0,
                                (UChar *)&aKeyArray[aFromIdx],
                                ID_SIZEOF(stndrKeyArray) * (aToIdx - aFromIdx + 1) );
                dumpIndexNode( aSrcNode );
                dumpIndexNode( aDstNode );
                IDE_ASSERT( 0 );
            }
        }
        else
        {
            sKeyLen = getKeyLength( (UChar*)sSrcKey,  ID_FALSE );
                        
            sRc = canAllocInternalKey( aMtx,
                                       aIndex,
                                       aDstNode,
                                       sKeyLen,
                                       ID_TRUE,  /* aExecCompact */
                                       ID_TRUE ); /* aIsLogging */

            if( sRc != IDE_SUCCESS )
            {
                ideLog::log( IDE_SERVER_0,
                             "From <%d> to <%d> : "
                             "Current sequence number : %d"
                             ", KeyLen %u\n",
                             aFromIdx, aToIdx, i, sKeyLen );
                ideLog::logMem( IDE_SERVER_0,
                                (UChar *)&aKeyArray[aFromIdx],
                                ID_SIZEOF(stndrKeyArray) * (aToIdx - aFromIdx + 1) );
                dumpIndexNode( aSrcNode );
                dumpIndexNode( aDstNode );
                IDE_ASSERT( 0 );
            }
        }

        IDE_TEST( sdpPhyPage::allocSlot( (sdpPhyPageHdr*)aDstNode,
                                         sDstSeq,
                                         sKeyLen,
                                         ID_TRUE,
                                         &sAllowedSize,
                                         (UChar**)&sDstKey,
                                         &sKeyOffset,
                                         1 )
                  != IDE_SUCCESS );
        
        IDE_ASSERT( sAllowedSize >= sKeyLen );

        if( STNDR_IS_LEAF_NODE(sSrcNodeHdr) == ID_TRUE )
        {
            sSrcCreateCTS = STNDR_GET_CCTS_NO( sSrcKey );
            sSrcLimitCTS  = STNDR_GET_LCTS_NO( sSrcKey );

            sDstCreateCTS = sSrcCreateCTS;
            sDstLimitCTS  = sSrcLimitCTS;

            // Copy Source CTS to Destination Node
            if( SDN_IS_VALID_CTS(sSrcCreateCTS) )
            {
                if( STNDR_GET_STATE(sSrcKey) == STNDR_KEY_DEAD )
                {
                    sDstCreateCTS = SDN_CTS_INFINITE;
                }
                else
                {
                    sRc = sdnIndexCTL::allocCTS( aSrcNode,
                                                 sSrcCreateCTS,
                                                 aDstNode,
                                                 &sDstCreateCTS );
                    
                    if( sRc != IDE_SUCCESS )
                    {
                        ideLog::log( IDE_SERVER_0,
                                     "From <%d> to <%d> : "
                                     "Current sequence number : %d"
                                     "\nSource create CTS : %u"
                                     ", Dest create CTS : %u\n",
                                     aFromIdx, aToIdx, i,
                                     sSrcCreateCTS, sDstCreateCTS );
                        ideLog::logMem( IDE_SERVER_0,
                                        (UChar *)&aKeyArray[aFromIdx],
                                        ID_SIZEOF(stndrKeyArray) * (aToIdx - aFromIdx + 1) );
                        dumpIndexNode( aSrcNode );
                        dumpIndexNode( aDstNode );
                        IDE_ASSERT( 0 );
                    }

                    sRc = sdnIndexCTL::bindCTS( aMtx,
                                                aIndex->mSdnHeader.mIndexTSID,
                                                sKeyOffset,
                                                aSrcNode,
                                                sSrcCreateCTS,
                                                aDstNode,
                                                sDstCreateCTS );
                    if ( sRc != IDE_SUCCESS )
                    {
                        ideLog::log( IDE_SERVER_0,
                                     "From <%d> to <%d> : "
                                     "Current sequence number : %d"
                                     "\nSource create CTS : %u"
                                     ", Dest create CTS : %u"
                                     "\nKey Offset : %u\n",
                                     aFromIdx, aToIdx, i,
                                     sSrcCreateCTS, sDstCreateCTS, sKeyOffset );
                        ideLog::logMem( IDE_SERVER_0,
                                        (UChar *)&aKeyArray[aFromIdx],
                                        ID_SIZEOF(stndrKeyArray) * (aToIdx - aFromIdx + 1) );
                        dumpIndexNode( aSrcNode );
                        dumpIndexNode( aDstNode );
                        IDE_ASSERT( 0 );
                    }
                }
            }

            if( SDN_IS_VALID_CTS(sSrcLimitCTS) )
            {
                sRc = sdnIndexCTL::allocCTS( aSrcNode,
                                             sSrcLimitCTS,
                                             aDstNode,
                                             &sDstLimitCTS );
                
                if( sRc != IDE_SUCCESS )
                {
                    ideLog::log( IDE_SERVER_0,
                                 "From <%d> to <%d> : "
                                 "Current sequence number : %d"
                                 "\nSource create CTS : %u"
                                 ", Dest create CTS : %u"
                                 "\nKey Offset : %u\n",
                                 aFromIdx, aToIdx, i,
                                 sSrcCreateCTS, sDstCreateCTS, sKeyOffset );
                    dumpIndexNode( aSrcNode );
                    dumpIndexNode( aDstNode );
                    IDE_ASSERT( 0 );
                }

                sRc = sdnIndexCTL::bindCTS( aMtx,
                                            aIndex->mSdnHeader.mIndexTSID,
                                            sKeyOffset,
                                            aSrcNode,
                                            sSrcLimitCTS,
                                            aDstNode,
                                            sDstLimitCTS );
                if ( sRc != IDE_SUCCESS )
                {
                    ideLog::log( IDE_SERVER_0,
                                 "From <%d> to <%d> : "
                                 "Current sequence number : %d"
                                 "\nSource create CTS : %u"
                                 ", Dest create CTS : %u"
                                 "\nKey Offset : %u\n",
                                 aFromIdx, aToIdx, i,
                                 sSrcCreateCTS, sDstCreateCTS, sKeyOffset );
                    dumpIndexNode( aSrcNode );
                    dumpIndexNode( aDstNode );
                    IDE_ASSERT( 0 );
                }
            }

            idlOS::memcpy( (UChar*)sDstKey, (UChar*)sSrcKey, sKeyLen );

            STNDR_SET_CCTS_NO( sDstKey, sDstCreateCTS );
            STNDR_SET_LCTS_NO( sDstKey, sDstLimitCTS );

            if( ( STNDR_GET_STATE(sDstKey) == STNDR_KEY_UNSTABLE ) ||
                ( STNDR_GET_STATE(sDstKey) == STNDR_KEY_STABLE ) )
            {
                sUnlimitedKeyCount++;
            }

            // BUG-29538 split�� TBK count�� �������� �ʰ� �ֽ��ϴ�.
            if( STNDR_GET_TB_TYPE( sSrcKey ) == STNDR_KEY_TB_KEY )
            {
                sDstTBKCount++;
                IDE_ASSERT( sDstTBKCount <= sSrcNodeHdr->mTBKCount );
            }
        }
        else
        {
            idlOS::memcpy( (UChar*)sDstKey, (UChar*)sSrcKey, sKeyLen );
        }
        
        // write log
        IDE_TEST( sdrMiniTrans::writeLogRec( aMtx,
                                             (UChar*)sDstKey,
                                             (void*)NULL,
                                             ID_SIZEOF(SShort)+sKeyLen,
                                             SDR_STNDR_INSERT_INDEX_KEY )
                  != IDE_SUCCESS );
        
        IDE_TEST( sdrMiniTrans::write( aMtx,
                                       (void*)&sDstSeq,
                                       ID_SIZEOF(SShort) )
                  != IDE_SUCCESS );
        
        IDE_TEST( sdrMiniTrans::write( aMtx,
                                       (void*)sDstKey,
                                       sKeyLen)
                  != IDE_SUCCESS );
    }

    IDE_TEST( sdrMiniTrans::writeNBytes(
                  aMtx,
                  (UChar*)&sDstNodeHdr->mUnlimitedKeyCount,
                  (void*)&sUnlimitedKeyCount,
                  ID_SIZEOF(sUnlimitedKeyCount) )
              != IDE_SUCCESS);

    // BUG-29538 split�� TBK count�� �������� �ʰ� �ֽ��ϴ�.
    // Destination leaf node�� header�� TBK count�� �����ϰ� �α�
    IDE_ASSERT( sSrcNodeHdr->mTBKCount >= sDstTBKCount );

    IDE_TEST( sdrMiniTrans::writeNBytes(
                  aMtx,
                  (UChar*)&sDstNodeHdr->mTBKCount,
                  (void*)&sDstTBKCount,
                  ID_SIZEOF(sDstTBKCount))
              != IDE_SUCCESS);

    // Unbind Source Key
    for( i = aToIdx; i >= aFromIdx; i-- )
    {
        sSeq = aKeyArray[i].mKeySeq;

        if( (sSeq == STNDR_INVALID_KEY_SEQ) )
        {
            continue;
        }

        IDE_TEST( sdpSlotDirectory::getPagePtrFromSlotNum( sSlotDirPtr, 
                                                           sSeq,
                                                           (UChar**)&sSrcKey )
                  != IDE_SUCCESS );
        
        IDE_TEST( sdpSlotDirectory::getValue( sSlotDirPtr, 
                                              sSeq,
                                              &sKeyOffset )
                  != IDE_SUCCESS );
        
        if( STNDR_IS_LEAF_NODE(sSrcNodeHdr) == ID_TRUE )
        {
            if ( SDN_IS_VALID_CTS( STNDR_GET_CCTS_NO(sSrcKey) ) )
            {
                IDE_TEST( sdnIndexCTL::unbindCTS( aMtx,
                                                  aSrcNode,
                                                  STNDR_GET_CCTS_NO( sSrcKey ),
                                                  sKeyOffset )
                          != IDE_SUCCESS );
            }

            if ( SDN_IS_VALID_CTS( STNDR_GET_LCTS_NO( sSrcKey ) ) )
            {
                IDE_TEST( sdnIndexCTL::unbindCTS( aMtx,
                                                  aSrcNode,
                                                  STNDR_GET_LCTS_NO( sSrcKey ),
                                                  sKeyOffset )
                          != IDE_SUCCESS );
            }
        }
    }

    // Free Source Key and Adjust UnlimitedKeyCount
    IDE_TEST( freeKeys( aMtx,
                        aSrcNode,
                        aKeyArray,
                        0,
                        aFromIdx - 1 )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION :
 * ------------------------------------------------------------------*
 * aFromIdx�������� aToIdx�� ������ ��� Ű�� Free ��Ų��.
 *********************************************************************/
IDE_RC stndrRTree::freeKeys( sdrMtx         * aMtx,
                             sdpPhyPageHdr  * aNode,
                             stndrKeyArray  * aKeyArray,
                             UShort           aFromIdx,
                             UShort           aToIdx )
{
    stndrNodeHdr    * sNodeHdr;
    UShort            sUnlimitedKeyCount = 0;
    UShort            sTBKCount          = 0;

    sNodeHdr = (stndrNodeHdr*)
        sdpPhyPage::getLogicalHdrStartPtr( (UChar*)aNode );

    IDE_TEST( writeLogFreeKeys(
                  aMtx,
                  (UChar*)aNode,
                  aKeyArray,
                  aFromIdx,
                  aToIdx ) != IDE_SUCCESS );

    if( STNDR_IS_LEAF_NODE(sNodeHdr) == ID_TRUE )
    {
        freeKeysLeaf( aNode,
                      aKeyArray,
                      aFromIdx,
                      aToIdx,
                      &sUnlimitedKeyCount,
                      &sTBKCount );
    }
    else
    {
        freeKeysInternal( aNode,
                          aKeyArray,
                          aFromIdx,
                          aToIdx );
    }

    IDE_TEST( sdrMiniTrans::writeNBytes(
                  aMtx,
                  (UChar*)&sNodeHdr->mUnlimitedKeyCount,
                  (void*)&sUnlimitedKeyCount,
                  ID_SIZEOF(sUnlimitedKeyCount) )
              != IDE_SUCCESS );

    // BUG-29538 split�� TBK count�� �������� �ʰ� �ֽ��ϴ�.
    // Source leaf node�� header�� TBK count�� �����ϰ� �α�
    IDE_TEST( sdrMiniTrans::writeNBytes(
                  aMtx,
                  (UChar*)&sNodeHdr->mTBKCount,
                  (void*)&sTBKCount,
                  ID_SIZEOF(sTBKCount) )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

/*********************************************************************
 * FUNCTION DESCRIPTION :
 * ------------------------------------------------------------------*
 * Internal Key�� Free ��Ų��.
 *********************************************************************/
IDE_RC stndrRTree::freeKeysInternal( sdpPhyPageHdr    * aNode,
                                   stndrKeyArray    * aKeyArray,
                                   UShort             aFromIdx,
                                   UShort             aToIdx )
{

    SInt          i;
    UShort        sTmpBuf[SD_PAGE_SIZE]; // 2 * Page size -> align ����...
    UChar       * sTmpPage;
    UChar       * sSlotDirPtr;
    UShort        sKeyLength;
    UShort        sAllowedSize;
    scOffset      sSlotOffset;
    UChar       * sSrcKey;
    UChar       * sDstKey;
    SShort        sSeq;
    SShort        sKeySeq = 0;
    

    sSlotDirPtr = sdpPhyPage::getSlotDirStartPtr( (UChar*)aNode );

    sTmpPage = ((vULong)sTmpBuf) % SD_PAGE_SIZE == 0
        ? (UChar*)sTmpBuf
        : (UChar*)sTmpBuf + SD_PAGE_SIZE - (((vULong)sTmpBuf) % SD_PAGE_SIZE);

    idlOS::memcpy( sTmpPage, aNode, SD_PAGE_SIZE );
    
    IDE_ASSERT( aNode->mPageID == ((sdpPhyPageHdr*)sTmpPage)->mPageID );

    (void)sdpPhyPage::reset( aNode,
                             ID_SIZEOF(stndrNodeHdr),
                             NULL );

    sdpSlotDirectory::init( aNode );
    sSlotDirPtr = sdpPhyPage::getSlotDirStartPtr(sTmpPage);

    for( i = aFromIdx; i <= aToIdx; i++ )
    {
        sSeq = aKeyArray[i].mKeySeq;

        if( sSeq == STNDR_INVALID_KEY_SEQ )
        {
            continue;
        }
        
        IDE_TEST( sdpSlotDirectory::getPagePtrFromSlotNum( sSlotDirPtr, 
                                                           sSeq,
                                                           &sSrcKey )
                  != IDE_SUCCESS );


        sKeyLength = getKeyLength( sSrcKey, ID_FALSE ); //aIsLeaf

        IDE_ASSERT( sdpPhyPage::allocSlot( (sdpPhyPageHdr*)aNode,
                                           sKeySeq,
                                           sKeyLength,
                                           ID_TRUE,
                                           &sAllowedSize,
                                           &sDstKey,
                                           &sSlotOffset,
                                           1 )
                    == IDE_SUCCESS );
        
        idlOS::memcpy( sDstKey, sSrcKey, sKeyLength );
        // Insert Logging�� �ʿ� ����.

        sKeySeq++;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

/*********************************************************************
 * FUNCTION DESCRIPTION :
 * ------------------------------------------------------------------*
 * Leaf Key�� Free ��Ų��.
 *********************************************************************/
IDE_RC stndrRTree::freeKeysLeaf( sdpPhyPageHdr    * aNode,
                                 stndrKeyArray    * aKeyArray,
                                 UShort             aFromIdx,
                                 UShort             aToIdx,
                                 UShort           * aUnlimitedKeyCount,
                                 UShort           * aTBKCount )
{
    SInt              i;
    UShort            sTmpBuf[SD_PAGE_SIZE]; // 2 * Page size -> align ����...
    UChar           * sTmpPage;
    UChar           * sSlotDirPtr;
    UShort            sKeyLength;
    UShort            sAllowedSize;
    scOffset          sSlotOffset;
    UChar           * sSrcKey;
    UChar           * sDstKey;
    stndrLKey       * sLeafKey;
    UChar             sCreateCTS;
    UChar             sLimitCTS;
    stndrNodeHdr    * sNodeHdr;
    SShort            sKeySeq;
    UChar           * sDummy;
    UShort            sUnlimitedKeyCount = 0;
    SShort            sSeq;
    UShort            sTBKCount = 0;

    sSlotDirPtr = sdpPhyPage::getSlotDirStartPtr((UChar*)aNode);
    sNodeHdr    = (stndrNodeHdr*)sdpPhyPage::getLogicalHdrStartPtr((UChar*)aNode);

    sTmpPage = ((vULong)sTmpBuf) % SD_PAGE_SIZE == 0
        ? (UChar*)sTmpBuf
        : (UChar*)sTmpBuf + SD_PAGE_SIZE - (((vULong)sTmpBuf) % SD_PAGE_SIZE);

    idlOS::memcpy(sTmpPage, aNode, SD_PAGE_SIZE);
    IDE_ASSERT( aNode->mPageID == ((sdpPhyPageHdr*)sTmpPage)->mPageID );

    (void)sdpPhyPage::reset( aNode,
                             ID_SIZEOF(stndrNodeHdr),
                             NULL );

    // sdpPhyPage::reset������ CTL�� �ʱ�ȭ �������� �ʴ´�.
    sdpPhyPage::initCTL( aNode,
                         (UInt)sdnIndexCTL::getCTLayerSize(sTmpPage),
                         &sDummy );

    sdpSlotDirectory::init( aNode );

    sdnIndexCTL::cleanAllRefInfo( aNode );

    sNodeHdr->mTotalDeadKeySize = 0;

    sSlotDirPtr = sdpPhyPage::getSlotDirStartPtr(sTmpPage);
    
    for( i = aFromIdx, sKeySeq = 0; i <= aToIdx; i++ )
    {
        sSeq = aKeyArray[i].mKeySeq;

        if( sSeq == STNDR_INVALID_KEY_SEQ )
        {
            continue;
        }

        
        IDE_TEST( sdpSlotDirectory::getPagePtrFromSlotNum(sSlotDirPtr, 
                                                          sSeq, 
                                                          &sSrcKey)
                  != IDE_SUCCESS );
        
        sLeafKey = (stndrLKey*)sSrcKey;
            
        sKeyLength = getKeyLength( sSrcKey, ID_TRUE ); //aIsLeaf

        IDE_ASSERT( sdpPhyPage::allocSlot( (sdpPhyPageHdr*)aNode,
                                           sKeySeq,
                                           sKeyLength,
                                           ID_TRUE,
                                           &sAllowedSize,
                                           &sDstKey,
                                           &sSlotOffset,
                                           1 )
                    == IDE_SUCCESS );

        sKeySeq++;
       
        idlOS::memcpy( sDstKey, sSrcKey, sKeyLength );

        sCreateCTS = STNDR_GET_CCTS_NO( sLeafKey );
        sLimitCTS  = STNDR_GET_LCTS_NO( sLeafKey );

        if ( SDN_IS_VALID_CTS(sCreateCTS) )
        {
            sdnIndexCTL::addRefKey( aNode, sCreateCTS, sSlotOffset );
        }

        if ( SDN_IS_VALID_CTS(sLimitCTS) )
        {
            sdnIndexCTL::addRefKey( aNode, sLimitCTS, sSlotOffset );
        }

        if( (STNDR_GET_STATE( sLeafKey ) == STNDR_KEY_UNSTABLE) ||
            (STNDR_GET_STATE( sLeafKey ) == STNDR_KEY_STABLE) )
        {
            sUnlimitedKeyCount++;
        }

        // BUG-29538 split�� TBK count�� �������� �ʰ� �ֽ��ϴ�.
        if( STNDR_GET_TB_TYPE( sLeafKey ) == STNDR_KEY_TB_KEY )
        {
            sTBKCount++;
            IDE_ASSERT( sTBKCount <= sNodeHdr->mTBKCount );
        }
    }

    *aUnlimitedKeyCount = sUnlimitedKeyCount;
    *aTBKCount          = sTBKCount;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

/*********************************************************************
 * FUNCTION DESCRIPTION :
 * ------------------------------------------------------------------*
 * aKeyArray���� ������ Split Point�� ���� ������ ��� ����� ����
 * Perimeter�� ���Ѵ�.
 *********************************************************************/
void stndrRTree::getSplitInfo( stndrHeader      * aIndex,
                               stndrKeyArray    * aKeyArray,
                               SShort             aKeyArrayCnt,
                               UShort           * aSplitPoint,
                               SDouble          * aSumPerimeter )
{
    SDouble sSumPerimeter;
    SDouble sMinPerimeter;
    SDouble sMinOverlap;
    SDouble sMinArea;
    SDouble sPerimeter1;
    SDouble sPerimeter2;
    SDouble sOverlap;
    SDouble sArea;
    stdMBR  sMBR1;
    stdMBR  sMBR2;
    UInt    sSplitRate = 40;
    SShort  sInitSplitPoint = 0;
    SShort  sMaxSplitPoint = 0;
    UShort  i; // BUG-30950 ������ ���׷� ���Ͽ� SShort -> UShort�� ����

    IDE_ASSERT( aKeyArrayCnt >= 3 );

    sSplitRate = smuProperty::getRTreeSplitRate();
    if( sSplitRate > 50 )
    {
        sSplitRate = 50;
    }

    sInitSplitPoint = ( aKeyArrayCnt * sSplitRate ) / 100 ;
    if( sInitSplitPoint <  0)
    {
        sInitSplitPoint = 0;
    }
    
    sMaxSplitPoint  = sInitSplitPoint +
        ( aKeyArrayCnt - (sInitSplitPoint * 2) );
    if( sMaxSplitPoint >= (aKeyArrayCnt - 1) )
    {
        // zero base �̱� ������ -2�� �����Ѵ�.
        sMaxSplitPoint = (aKeyArrayCnt - 2);
    }
    
    getArrayPerimeter( aIndex,
                       aKeyArray,
                       0,
                       aKeyArrayCnt - 1,
                       &sMinPerimeter,
                       &sMBR1 );
    
    sMinOverlap = stdUtils::getMBRArea( &sMBR1 );
    sMinArea    = sMinOverlap;

    sSumPerimeter = 0;
    *aSplitPoint = sInitSplitPoint;
    
    for( i = sInitSplitPoint; i <= sMaxSplitPoint; i++ )
    {
        getArrayPerimeter( aIndex,
                           aKeyArray,
                           0,
                           i,
                           &sPerimeter1,
                           &sMBR1 );
        
        getArrayPerimeter( aIndex,
                           aKeyArray,
                           i + 1,
                           (aKeyArrayCnt - 1),
                           &sPerimeter2,
                           &sMBR2 );

        sOverlap = stdUtils::getMBROverlap( &sMBR1, &sMBR2 );

        sArea = stdUtils::getMBRArea( &sMBR1 ) + stdUtils::getMBRArea( &sMBR2 );

        if( sOverlap < sMinOverlap )
        {
            *aSplitPoint = i;
            sMinOverlap = sOverlap;
        }
        else
        {
            if( (sOverlap == sMinOverlap) && (sArea < sMinArea) )
            {
                *aSplitPoint = i;
                sMinArea = sArea;
            }
        }

        sSumPerimeter += (sPerimeter1 + sPerimeter2);
    }

    *aSumPerimeter  = sSumPerimeter;

    return;
}

/*********************************************************************
 * FUNCTION DESCRIPTION :
 * ------------------------------------------------------------------*
 * Kery Array�� aStartPos���� aEndPos�� ������ Key�� �����ϴ� MBR��
 * Perimeter�� ���Ѵ�.
 *********************************************************************/
void stndrRTree::getArrayPerimeter( stndrHeader     * /*aIndex*/,
                                    stndrKeyArray   * aKeyArray,
                                    UShort            aStartPos,
                                    UShort            aEndPos,
                                    SDouble         * aPerimeter,
                                    stdMBR          * aMBR )
{
    UInt i;

    *aMBR = aKeyArray[aStartPos].mMBR;

    for( i = aStartPos + 1; i <= aEndPos; i++ )
    {
        stdUtils::getMBRExtent( aMBR, &aKeyArray[i].mMBR );
    }

    *aPerimeter =
        (aMBR->mMaxX - aMBR->mMinX)*2 +
        (aMBR->mMaxY - aMBR->mMinY)*2;
    
    return;
}

/*********************************************************************
 * FUNCTION DESCRIPTION :
 * ------------------------------------------------------------------*
 * �ڽĳ���� KeyValue(MBR) ������ ���� ��忡 �����Ѵ�. ������������
 * ������ ���� ������ ��� ���� ��忡 ���� X-Latch�� ���� �� �ֻ������� 
 * �Ʒ��� ���� ���鼭 KeyValue �����۾��� �����Ѵ�.
 * �� �� Stack�� �ֻ��� ��尡 Root Node�� �ƴϸ� Root�� ����� ���
 * �̹Ƿ� Retry �Ѵ�.
 *********************************************************************/
IDE_RC stndrRTree::propagateKeyValue( idvSQL            * aStatistics,
                                      stndrStatistic    * aIndexStat,
                                      stndrHeader       * aIndex,
                                      sdrMtx            * aMtx,
                                      stndrPathStack    * aStack,
                                      sdpPhyPageHdr     * aChildNode,
                                      stdMBR            * aChildNodeMBR,
                                      idBool            * aIsRetry )
{
    scPageID          sChildPID;
    scPageID          sParentPID;
    sdpPhyPageHdr   * sParentNode;
    SShort            sParentKeySeq;
    stdMBR            sParentKeyMBR;
    stdMBR            sParentNodeMBR;
    IDE_RC            sRc;
    

    if( aStack->mDepth < 0 )
    {
        IDE_RAISE( RETURN_SUCCESS );
    }

    sChildPID = sdpPhyPage::getPageID( (UChar*)aChildNode );

    IDE_TEST( getParentNode( aStatistics,
                             aIndexStat,
                             aMtx,
                             aIndex,
                             aStack,
                             sChildPID,
                             &sParentPID,
                             &sParentNode,
                             &sParentKeySeq,
                             &sParentKeyMBR,
                             aIsRetry )
              != IDE_SUCCESS );

    // root node ���� üũ�ϱ�
    IDE_TEST_RAISE( *aIsRetry == ID_TRUE, RETURN_SUCCESS );

    if( stdUtils::isMBREquals( &sParentKeyMBR,
                               aChildNodeMBR ) == ID_TRUE )
    {
        IDE_RAISE( RETURN_SUCCESS );
    }

    IDE_ASSERT( adjustNodeMBR( aIndex,
                               sParentNode,
                               NULL,                   /* aInsertKeySeq, */
                               sParentKeySeq,
                               aChildNodeMBR,
                               STNDR_INVALID_KEY_SEQ,  /* aDeleteKeySeq */
                               &sParentNodeMBR )
                == IDE_SUCCESS );
    
    aStack->mDepth--;
    IDE_TEST( propagateKeyValue( aStatistics,
                                 aIndexStat,
                                 aIndex,
                                 aMtx,
                                 aStack,
                                 sParentNode,
                                 &sParentNodeMBR,
                                 aIsRetry )
              != IDE_SUCCESS );

    if( *aIsRetry == ID_TRUE )
    {
        IDE_RAISE( RETURN_SUCCESS );
    }

    sRc = updateIKey( aMtx,
                      sParentNode,
                      sParentKeySeq,
                      aChildNodeMBR,
                      aIndex->mSdnHeader.mLogging );

    if( sRc != IDE_SUCCESS )
    {
        ideLog::log( IDE_SERVER_0,
                     "Key sequence : %d"
                     ", update MBR : \n",
                     sParentKeySeq  );
        ideLog::logMem( IDE_SERVER_0,
                        (UChar*)aChildNodeMBR,
                        ID_SIZEOF(stdMBR) );
        dumpIndexNode( sParentNode );
        IDE_ASSERT( 0 );
    }

    sRc = setNodeMBR( aMtx,
                      sParentNode,
                      &sParentNodeMBR );

    if( sRc != IDE_SUCCESS )
    {
        ideLog::log( IDE_SERVER_0, "update MBR : \n" );
        ideLog::logMem( IDE_SERVER_0,
                        (UChar*)&sParentNodeMBR,
                        ID_SIZEOF(stdMBR) );
        dumpIndexNode( sParentNode );
        IDE_ASSERT( 0 );
    }

    // update Tree MBR
    if( sParentPID == aIndex->mRootNode )
    {
        if( aIndex->mInitTreeMBR == ID_TRUE )
        {
            aIndex->mTreeMBR = sParentNodeMBR;
        }
        else
        {
            aIndex->mTreeMBR = sParentNodeMBR;
            aIndex->mInitTreeMBR = ID_TRUE;
        }
    }
    
    aIndexStat->mKeyPropagateCount++;

    IDE_EXCEPTION_CONT( RETURN_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : 
 * ------------------------------------------------------------------*
 * Internal node�� aNode�� �� Ű�� insert�Ѵ�. 
 *********************************************************************/
IDE_RC stndrRTree::insertIKey( sdrMtx           * aMtx,
                               stndrHeader      * aIndex,
                               sdpPhyPageHdr    * aNode,
                               SShort             aKeySeq,
                               stndrKeyInfo     * aKeyInfo,
                               UShort             aKeyValueLen,
                               scPageID           aRightChildPID,
                               idBool             aIsNeedLogging )
{
    UShort        sAllowedSize;
    scOffset      sSlotOffset;
    stndrIKey   * sIKey;
    UShort        sKeyLength = STNDR_IKEY_LEN( aKeyValueLen );


    IDE_TEST( canAllocInternalKey( aMtx,
                                   aIndex,
                                   aNode,
                                   sKeyLength,
                                   ID_TRUE, //aExecCompact
                                   aIsNeedLogging )
              != IDE_SUCCESS );

    IDE_TEST( sdpPhyPage::allocSlot( (sdpPhyPageHdr*)aNode,
                                     aKeySeq,
                                     sKeyLength,
                                     ID_TRUE,
                                     &sAllowedSize,
                                     (UChar**)&sIKey,
                                     &sSlotOffset,
                                     1 )
              != IDE_SUCCESS );
    
    IDE_DASSERT( sAllowedSize >= sKeyLength );

    STNDR_KEYINFO_TO_IKEY( *aKeyInfo,
                           aRightChildPID,
                           aKeyValueLen,
                           sIKey );

    if( aIsNeedLogging == ID_TRUE )
    {
        IDE_TEST( sdrMiniTrans::writeLogRec( aMtx,
                                             (UChar*)sIKey,
                                             (void*)NULL,
                                             ID_SIZEOF(SShort)+sKeyLength,
                                             SDR_STNDR_INSERT_INDEX_KEY )
                  != IDE_SUCCESS );

        IDE_TEST( sdrMiniTrans::write( aMtx,
                                       (void*)&aKeySeq,
                                       ID_SIZEOF(SShort) )
                  != IDE_SUCCESS );

        IDE_TEST( sdrMiniTrans::write( aMtx,
                                       (void*)sIKey,
                                       sKeyLength )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : 
 * ------------------------------------------------------------------*
 * Internal node�� aNode�� ���� Ű�� ������Ʈ �Ѵ�.
 *********************************************************************/
IDE_RC stndrRTree::updateIKey( sdrMtx           * aMtx,
                               sdpPhyPageHdr    * aNode,
                               SShort             aKeySeq,
                               stdMBR           * aKeyValue,
                               idBool             aLogging )
{
    UChar       * sSlotDirPtr;
    stndrIKey   * sIKey;


    sSlotDirPtr = sdpPhyPage::getSlotDirStartPtr( (UChar*)aNode );
    IDE_TEST( sdpSlotDirectory::getPagePtrFromSlotNum( sSlotDirPtr, 
                                                       aKeySeq,
                                                       (UChar**)&sIKey )
              != IDE_SUCCESS );

    STNDR_SET_KEYVALUE_TO_IKEY( *aKeyValue, ID_SIZEOF(*aKeyValue), sIKey );

    if( aLogging == ID_TRUE )
    {
        IDE_TEST( sdrMiniTrans::writeLogRec( aMtx,
                                             (UChar*)sIKey,
                                             (void*)aKeyValue,
                                             ID_SIZEOF(*aKeyValue),
                                             SDR_STNDR_UPDATE_INDEX_KEY )
                  != IDE_SUCCESS );
    }
    
    return IDE_SUCCESS;
    
    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

/*********************************************************************
 * FUNCTION DESCRIPTION : 
 * ------------------------------------------------------------------*
 * aChildPID�� ����Ű�� �θ� ��带 ã�Ƽ� X-Latch�� ��´�.
 * �θ� ��尡 Split�� �߻��� ��� ������ ��ũ�� ���󰡸鼭 aChildPID��
 * ����Ű�� �θ� ��带 ã�Ƽ� X-Latch�� ��´�.
 *********************************************************************/
IDE_RC stndrRTree::getParentNode( idvSQL            * aStatistics,
                                  stndrStatistic    * aIndexStat,
                                  sdrMtx            * aMtx,
                                  stndrHeader       * aIndex,
                                  stndrPathStack    * aStack,
                                  scPageID            aChildPID,
                                  scPageID          * aParentPID,
                                  sdpPhyPageHdr    ** aParentNode,
                                  SShort            * aParentKeySeq,
                                  stdMBR            * aParentKeyMBR,
                                  idBool            * aIsRetry )
{
    stndrStackSlot    sStackSlot;
    sdpPhyPageHdr   * sNode;
    scPageID          sPID;
    scPageID          sIKeyChildPID;
    UShort            sKeyCount;
    stndrIKey       * sIKey;
    ULong             sNodeSmoNo;
    ULong             sIndexSmoNo;
    SShort            sKeySeq;
    UChar           * sSlotDirPtr;
    idBool            sIsSuccess;
    UShort            i;
    
    
    *aParentPID    = SD_NULL_PID;
    *aParentNode   = NULL;
    *aParentKeySeq = STNDR_INVALID_KEY_SEQ;

    sStackSlot = aStack->mStack[aStack->mDepth];

    sPID       = sStackSlot.mNodePID;
    sIndexSmoNo= sStackSlot.mSmoNo;
    sKeySeq    = sStackSlot.mKeySeq;

    IDE_TEST( stndrRTree::getPage( aStatistics,
                                   &(aIndexStat->mIndexPage),
                                   aIndex->mSdnHeader.mIndexTSID,
                                   sPID,
                                   SDB_X_LATCH,
                                   SDB_WAIT_NORMAL,
                                   aMtx,
                                   (UChar**)&sNode,
                                   &sIsSuccess ) != IDE_SUCCESS );

    sSlotDirPtr = sdpPhyPage::getSlotDirStartPtr( (UChar*)sNode );
    sKeyCount   = sdpSlotDirectory::getCount( sSlotDirPtr );

    if( (sIndexSmoNo == sdpPhyPage::getIndexSMONo(sNode)) &&
        (sKeySeq != STNDR_INVALID_KEY_SEQ ) &&
        (sKeyCount > sKeySeq) )
    {
        IDE_TEST( sdpSlotDirectory::getPagePtrFromSlotNum( sSlotDirPtr, 
                                                           sKeySeq,
                                                           (UChar**)&sIKey )
                  != IDE_SUCCESS );

        if( aChildPID == sIKey->mChildPID )
        {
            *aParentPID    = sPID;
            *aParentNode   = sNode;
            *aParentKeySeq = sKeySeq;

            STNDR_GET_MBR_FROM_IKEY( *aParentKeyMBR, sIKey );

            IDE_RAISE( RETURN_SUCCESS );
        }
    }

    while( (sNodeSmoNo = sdpPhyPage::getIndexSMONo(sNode))
           >= sIndexSmoNo )
    {
        sSlotDirPtr = sdpPhyPage::getSlotDirStartPtr( (UChar*)sNode );
        sKeyCount   = sdpSlotDirectory::getCount( sSlotDirPtr );

        for( i = 0; i < sKeyCount; i++)
        {
            IDE_TEST( sdpSlotDirectory::getPagePtrFromSlotNum( sSlotDirPtr, 
                                                               i,
                                                               (UChar**)&sIKey )
                      != IDE_SUCCESS );

            STNDR_GET_CHILD_PID( &sIKeyChildPID, sIKey );

            if( aChildPID == sIKeyChildPID )
            {
                *aParentPID     = sPID;
                *aParentNode    = sNode;
                *aParentKeySeq  = i;

                STNDR_GET_MBR_FROM_IKEY( *aParentKeyMBR, sIKey );
                
                break;
            }
        }

        if( i != sKeyCount )
        {
            break;
        }

        if( sNodeSmoNo == sIndexSmoNo )
        {
            break;
        }

        sPID = sdpPhyPage::getNxtPIDOfDblList(sNode);
        if( sPID == SD_NULL_PID )
        {
            break;
        }

        IDE_TEST( stndrRTree::getPage( aStatistics,
                                       &(aIndexStat->mIndexPage),
                                       aIndex->mSdnHeader.mIndexTSID,
                                       sPID,
                                       SDB_X_LATCH,
                                       SDB_WAIT_NORMAL,
                                       aMtx,
                                       (UChar**)&sNode,
                                       &sIsSuccess ) != IDE_SUCCESS );

        aIndexStat->mFollowRightLinkCount++;
    }

    if( (aStack->mDepth == 0) && (*aParentPID != aIndex->mRootNode) )
    {
        *aIsRetry = ID_TRUE;
    }

    if( (*aParentPID    == SD_NULL_PID) ||
        (*aParentNode   == NULL) ||
        (*aParentKeySeq == STNDR_INVALID_KEY_SEQ) )
    {
        ideLog::log( IDE_SERVER_0,
                     "Child PID : %u"
                     ", sPID : %u"
                     ", Node Smo NO: %llu\n",
                     aChildPID, sPID, sNodeSmoNo );

        ideLog::log( IDE_SERVER_0,
                     "Stack Slot:\n" );
        ideLog::logMem( IDE_SERVER_0,
                        (UChar*)&sStackSlot,
                        ID_SIZEOF(stndrStackSlot) );

        ideLog::log( IDE_SERVER_0,
                     "Path Stack:\n" );
        ideLog::logMem( IDE_SERVER_0,
                        (UChar*)aStack,
                        ID_SIZEOF(stndrPathStack) );

        IDE_ASSERT( 0 );
    }

    IDE_EXCEPTION_CONT( RETURN_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : 
 * ------------------------------------------------------------------*
 * Leaf Node Ű�� ������ ������ �ִ��� Ȯ���Ѵ�.
 *********************************************************************/
IDE_RC stndrRTree::canInsertKey( idvSQL               * aStatistics,
                                 sdrMtx               * aMtx,
                                 stndrHeader          * aIndex,
                                 stndrStatistic       * aIndexStat,
                                 stndrKeyInfo         * aKeyInfo,
                                 sdpPhyPageHdr        * aLeafNode,
                                 SShort               * aLeafKeySeq,
                                 UChar                * aCTSlotNum,
                                 stndrCallbackContext * aContext )
{
    UChar   sAgedCount = 0;
    smSCN   sSysMinDskViewSCN;
    UShort  sKeyValueLen;
    UShort  sKeyLen;

    aContext->mIndex      = aIndex;
    aContext->mStatistics = aIndexStat;

    sKeyValueLen = getKeyValueLength();

    // BUG-30020: Top-Down ����ÿ� Stable�� Ű�� allocCTS�� ��ŵ�ؾ� �մϴ�.
    if( aKeyInfo->mKeyState == STNDR_KEY_STABLE )
    {
        sKeyLen = STNDR_LKEY_LEN( sKeyValueLen, STNDR_KEY_TB_CTS );
        IDE_RAISE( SKIP_ALLOC_CTS );
    }

    IDE_TEST( stndrRTree::allocCTS( aStatistics,
                                    aIndex,
                                    aMtx,
                                    aLeafNode,
                                    aCTSlotNum,
                                    &gCallbackFuncs4CTL,
                                    (UChar*)aContext,
                                    aLeafKeySeq )
              != IDE_SUCCESS );

    if( *aCTSlotNum == SDN_CTS_INFINITE )
    {
        sKeyLen = STNDR_LKEY_LEN( sKeyValueLen, STNDR_KEY_TB_KEY );
    }
    else
    {
        sKeyLen = STNDR_LKEY_LEN( sKeyValueLen, STNDR_KEY_TB_CTS );
    }

    IDE_EXCEPTION_CONT( SKIP_ALLOC_CTS );

    if( canAllocLeafKey( aMtx,
                         aIndex,
                         aLeafNode,
                         sKeyLen,
                         aLeafKeySeq ) != IDE_SUCCESS )
    {
        SMX_GET_MIN_DISK_VIEW( &sSysMinDskViewSCN );
            
        // ���������� ���� �Ҵ��� ���ؼ� Self Aging�� �Ѵ�.
        IDE_TEST( selfAging( aIndex,
                             aMtx,
                             aLeafNode,
                             &sSysMinDskViewSCN,
                             &sAgedCount ) != IDE_SUCCESS );

        if( sAgedCount > 0 )
        {
            IDE_TEST( canAllocLeafKey( aMtx,
                                       aIndex,
                                       aLeafNode,
                                       (UInt)sKeyLen,
                                       aLeafKeySeq )
                      != IDE_SUCCESS );
        }
        else
        {
            IDE_TEST( 1 ); // return IDE_FAILURE;
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : 
 * ------------------------------------------------------------------*
 * Ű�� ������ ������ Leaf Node�� ã�´�. Ű�� ���Ե� �� MBR�� Ȯ����
 * �ּ�ȭ�Ǵ� �������� Ž���Ѵ�.
 * Ű ���� �� Split�� �߻��ϸ� Split�� �߻����� ���� ���� ��带 ã�Ƽ�
 * �ش� ��ġ�κ��� ��Ž���Ѵ�.
 *********************************************************************/
IDE_RC stndrRTree::chooseLeafNode( idvSQL           * aStatistics,
                                   stndrStatistic   * aIndexStat,
                                   sdrMtx           * aMtx,
                                   stndrPathStack   * aStack,
                                   stndrHeader      * aIndex,
                                   stndrKeyInfo     * aKeyInfo,
                                   sdpPhyPageHdr   ** aLeafNode,
                                   SShort           * aLeafKeySeq )
{
    ULong                  sIndexSmoNo;
    SShort                 sKeySeq = 0;
    scPageID               sPID = SD_NULL_PID;
    scPageID               sChildPID = SD_NULL_PID;
    SDouble                sDelta;
    sdpPhyPageHdr        * sPage;
    stndrNodeHdr         * sNodeHdr;
    UInt                   sHeight;
    stndrStackSlot         sSlot;
    idBool                 sIsSuccess;
    idBool                 sIsRetry;
    idBool                 sFixState = ID_FALSE;
    sdrSavePoint           sSP;
    stndrVirtualRootNode   sVRootNode;

    
  retry:
    
    if( aStack->mDepth == -1 )
    {
        getVirtualRootNode( aIndex, &sVRootNode );

        sPID        = sVRootNode.mChildPID;
        sIndexSmoNo = sVRootNode.mChildSmoNo;
        
        if( sPID == SD_NULL_PID )
        {
            *aLeafNode = NULL;
            *aLeafKeySeq = 0;

            IDE_RAISE( RETURN_SUCCESS );
        }
    }
    else
    {
        sPID        = aStack->mStack[aStack->mDepth + 1].mNodePID;
        sIndexSmoNo = aStack->mStack[aStack->mDepth + 1].mSmoNo;
    }

    IDU_FIT_POINT( "1.PROJ-1591@stndrRTree::chooseLeafNode" );
    
    IDE_TEST( stndrRTree::getPage( aStatistics,
                                   &(aIndexStat->mIndexPage),
                                   aIndex->mSdnHeader.mIndexTSID,
                                   sPID,
                                   SDB_S_LATCH,
                                   SDB_WAIT_NORMAL,
                                   NULL,
                                   (UChar**)&sPage,
                                   &sIsSuccess ) != IDE_SUCCESS );
    sFixState = ID_TRUE;

    while( 1 )
    {
        sNodeHdr = (stndrNodeHdr*)
            sdpPhyPage::getLogicalHdrStartPtr( (UChar*)sPage );
        
        sHeight = sNodeHdr->mHeight;
        
        if( sHeight > 0 ) // Internal Node
        {
            findBestInternalKey( aKeyInfo,
                                 aIndex,
                                 sPage,
                                 &sKeySeq,
                                 &sChildPID,
                                 &sDelta,
                                 sIndexSmoNo,
                                 &sIsRetry );

            if( sIsRetry == ID_TRUE )
            {
                sFixState = ID_FALSE;
                IDE_TEST( sdbBufferMgr::releasePage( aStatistics,
                                                     (UChar*)sPage )
                          != IDE_SUCCESS );

                IDE_TEST( findValidStackDepth( aStatistics,
                                               aIndexStat,
                                               aIndex,
                                               aStack,
                                               &sIndexSmoNo )
                          != IDE_SUCCESS );

                // if( stndrStackMgr::getDepth(aStack) < 0 ) // SMO�� Root���� �Ͼ.
                if( aStack->mDepth < 0 ) // SMO�� Root���� �Ͼ.
                {
                    // init stack
                    aStack->mDepth = -1;
                    goto retry;
                }
                else
                {
                    sSlot = aStack->mStack[aStack->mDepth];
                    aStack->mDepth--;
                    
                    sPID = sSlot.mNodePID;
                    
                    IDE_TEST( stndrRTree::getPage( aStatistics,
                                                   &(aIndexStat->mIndexPage),
                                                   aIndex->mSdnHeader.mIndexTSID,
                                                   sPID,
                                                   SDB_S_LATCH,
                                                   SDB_WAIT_NORMAL,
                                                   NULL,
                                                   (UChar**)&sPage,
                                                   &sIsSuccess)
                              != IDE_SUCCESS );
                    sFixState = ID_TRUE;
                    
                    continue;
                }
            }
            else
            {
                aStack->mDepth++;
                aStack->mStack[aStack->mDepth].mNodePID = sPID;
                aStack->mStack[aStack->mDepth].mSmoNo  = sdpPhyPage::getIndexSMONo(sPage);
                aStack->mStack[aStack->mDepth].mKeySeq = sKeySeq;

                sPID = sChildPID;

                // !!! �ݵ�� Latch�� Ǯ�� ���� IndexNSN�� ����. �׷��� ������
                // �ڽ� ����� Split�� ������ �� ����.
                getSmoNo( aIndex, &sIndexSmoNo );
                IDL_MEM_BARRIER;

                sFixState = ID_FALSE;
                IDE_TEST( sdbBufferMgr::releasePage( aStatistics, (UChar*)sPage )
                          != IDE_SUCCESS );
            
                // fix sChildPID to sPage
                IDE_TEST( stndrRTree::getPage( aStatistics,
                                               &(aIndexStat->mIndexPage),
                                               aIndex->mSdnHeader.mIndexTSID,
                                               sPID,
                                               SDB_S_LATCH,
                                               SDB_WAIT_NORMAL,
                                               NULL,
                                               (UChar**)&sPage,
                                               &sIsSuccess )
                          != IDE_SUCCESS );
                
                sFixState = ID_TRUE;
            }
        }
        else // Leaf Node
        {
            // sNode�� unfix�Ѵ�.
            sFixState = ID_FALSE;
            IDE_TEST( sdbBufferMgr::releasePage( aStatistics,
                                                 (UChar*)sPage )
                      != IDE_SUCCESS );

            sdrMiniTrans::setSavePoint( aMtx, &sSP );
            
            IDE_TEST( stndrRTree::getPage( aStatistics,
                                           &(aIndexStat->mIndexPage),
                                           aIndex->mSdnHeader.mIndexTSID,
                                           sPID,
                                           SDB_X_LATCH,
                                           SDB_WAIT_NORMAL,
                                           aMtx,
                                           (UChar**)&sPage,
                                           &sIsSuccess )
                      != IDE_SUCCESS );

            findBestLeafKey( sPage, &sKeySeq, sIndexSmoNo, &sIsRetry );

            if( sIsRetry == ID_TRUE )
            {
                sdrMiniTrans::releaseLatchToSP( aMtx, &sSP );

                IDE_TEST( findValidStackDepth( aStatistics,
                                               aIndexStat,
                                               aIndex,
                                               aStack,
                                               &sIndexSmoNo )
                          != IDE_SUCCESS );

                if( aStack->mDepth < 0 ) // SMO�� Root���� �Ͼ.
                {
                    // init Stack
                    aStack->mDepth = -1;
                    goto retry;
                }
                else
                {
                    sSlot = aStack->mStack[aStack->mDepth];
                    aStack->mDepth--;
                    
                    sPID = sSlot.mNodePID;
                    
                    IDE_TEST( stndrRTree::getPage( aStatistics,
                                                   &(aIndexStat->mIndexPage),
                                                   aIndex->mSdnHeader.mIndexTSID,
                                                   sPID,
                                                   SDB_S_LATCH,
                                                   SDB_WAIT_NORMAL,
                                                   NULL,
                                                   (UChar**)&sPage,
                                                   &sIsSuccess)
                              != IDE_SUCCESS );
                    sFixState = ID_TRUE;
                    
                    continue;
                }
            }
            else
            {
                aStack->mDepth++;
                aStack->mStack[aStack->mDepth].mNodePID = sPID;
                aStack->mStack[aStack->mDepth].mSmoNo  = sdpPhyPage::getIndexSMONo(sPage);
                aStack->mStack[aStack->mDepth].mKeySeq = sKeySeq;

                *aLeafNode   = sPage;
                *aLeafKeySeq = sKeySeq;
            }
            
            break;
        }
    }

    IDE_EXCEPTION_CONT( RETURN_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if( sFixState == ID_TRUE )
    {
        if( sdbBufferMgr::releasePage( aStatistics, (UChar*)sPage )
            != IDE_SUCCESS )
        {
            dumpIndexNode( sPage );
            IDE_ASSERT( 0 );
        }
    }

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : 
 * ------------------------------------------------------------------*
 * Ű ������ ���� Tree Ž�� �߿� Split �� ��带 ������ ��� ȣ��ȴ�.
 * Stack�� ���� �ð����� Split�� �߻����� ���� ��带 ã�´�.
 *********************************************************************/
IDE_RC stndrRTree::findValidStackDepth( idvSQL          * aStatistics,
                                        stndrStatistic  * aIndexStat,
                                        stndrHeader     * aIndex,
                                        stndrPathStack  * aStack,
                                        ULong           * aSmoNo )
{
    sdpPhyPageHdr   * sNode;
    stndrStackSlot    sSlot;
    ULong             sNodeSmoNo;
    idBool            sTrySuccess;

    while( 1 ) // ������ ���� �ö� ����
    {
        if( aStack->mDepth < 0 ) // root���� SMO �߻�
        {
            break;
        }
        else
        {
            sSlot = aStack->mStack[aStack->mDepth];
            
            IDE_TEST( stndrRTree::getPage( aStatistics,
                                           &(aIndexStat->mIndexPage),
                                           aIndex->mSdnHeader.mIndexTSID,
                                           sSlot.mNodePID,
                                           SDB_S_LATCH,
                                           SDB_WAIT_NORMAL,
                                           NULL,
                                           (UChar**)&sNode,
                                           &sTrySuccess )
                      != IDE_SUCCESS );
            
            sNodeSmoNo = sdpPhyPage::getIndexSMONo( sNode );

            IDE_TEST( sdbBufferMgr::releasePage( aStatistics, (UChar*)sNode )
                      != IDE_SUCCESS );

            if( sNodeSmoNo <= sSlot.mSmoNo )
            {
                if( aSmoNo != NULL )
                {
                    *aSmoNo = sNodeSmoNo;
                }
                break; // �� ��� �������� �ٽ� traverse
            }

            aStack->mDepth--;
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : 
 * ------------------------------------------------------------------*
 * Internal Node���� Ű�� �����ϱ� ���� ������ Key�� �����Ѵ�.
 *********************************************************************/
IDE_RC stndrRTree::findBestInternalKey( stndrKeyInfo  * aKeyInfo,
                                        stndrHeader   * /*aIndex*/,
                                        sdpPhyPageHdr * aNode,
                                        SShort        * aKeySeq,
                                        scPageID      * aChildPID,
                                        SDouble       * aDelta,
                                        ULong           aIndexSmoNo,
                                        idBool        * aIsRetry )
{
    stndrNodeHdr    * sNodeHdr;
    SDouble           sMinArea;
    SDouble           sMinDelta;
    SDouble           sDelta;
    UChar           * sSlotDirPtr;
    stndrIKey       * sIKey;
    UShort            sKeyCount;
    stdMBR            sKeyInfoMBR;
    stdMBR            sKeyMBR;
    scPageID          sMinChildPID;
    UInt              sMinSeq;
    UInt              i;

    
    *aIsRetry = ID_FALSE;

    sNodeHdr = (stndrNodeHdr*)
        sdpPhyPage::getLogicalHdrStartPtr( (UChar*)aNode );
    
    if( (sdpPhyPage::getIndexSMONo(aNode) > aIndexSmoNo) ||
        (sNodeHdr->mState == STNDR_IN_FREE_LIST) )
    {
        *aIsRetry = ID_TRUE;
        return IDE_SUCCESS; 
    }

    STNDR_GET_MBR_FROM_KEYINFO( sKeyInfoMBR, aKeyInfo );
    
    sSlotDirPtr = sdpPhyPage::getSlotDirStartPtr( (UChar*)aNode );
    sKeyCount   = sdpSlotDirectory::getCount( sSlotDirPtr );

    sMinSeq = 0;
    IDE_TEST( sdpSlotDirectory::getPagePtrFromSlotNum( sSlotDirPtr, 
                                                       sMinSeq,
                                                       (UChar**)&sIKey )
              != IDE_SUCCESS );

    STNDR_GET_MBR_FROM_IKEY( sKeyMBR, sIKey );

    sMinArea  = stdUtils::getMBRArea( &sKeyMBR );
    sMinDelta = stdUtils::getMBRDelta( &sKeyMBR, &sKeyInfoMBR );
    
    STNDR_GET_CHILD_PID( &sMinChildPID, sIKey );
    STNDR_GET_MBR_FROM_IKEY( sKeyMBR, sIKey );

    for( i = 1; i < sKeyCount; i++ )
    {
        IDE_TEST( sdpSlotDirectory::getPagePtrFromSlotNum( sSlotDirPtr, 
                                                           i,
                                                           (UChar**)&sIKey )
                  != IDE_SUCCESS );
        
        STNDR_GET_MBR_FROM_IKEY( sKeyMBR, sIKey );

        sDelta = stdUtils::getMBRDelta( &sKeyMBR, &sKeyInfoMBR );

        if( (sDelta < sMinDelta) ||
            ((sDelta == sMinDelta) &&
             (sMinArea > stdUtils::getMBRArea(&sKeyMBR))) )
        {
            sMinDelta = sDelta;
            sMinArea  = stdUtils::getMBRArea( &sKeyMBR );
            sMinSeq   = i;
            
            STNDR_GET_CHILD_PID( &sMinChildPID, sIKey );
        }
    }

    *aKeySeq    = sMinSeq;
    *aChildPID  = sMinChildPID;
    *aDelta     = sMinDelta;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : 
 * ------------------------------------------------------------------*
 * Leaf Node���� Ű�� �����ϱ� ���� ������ Key�� �����Ѵ�.
 *********************************************************************/
void stndrRTree::findBestLeafKey( sdpPhyPageHdr * aNode,
                                  SShort        * aKeySeq,
                                  ULong           aIndexSmoNo,
                                  idBool        * aIsRetry )
{
    stndrNodeHdr    * sNodeHdr;

    
    *aIsRetry = ID_FALSE;

    sNodeHdr = (stndrNodeHdr*)
        sdpPhyPage::getLogicalHdrStartPtr( (UChar*)aNode );

    if( (sdpPhyPage::getIndexSMONo(aNode) > aIndexSmoNo) ||
        (sNodeHdr->mState == STNDR_IN_FREE_LIST) )
    {
        *aIsRetry = ID_TRUE;
        return;
    }

    *aKeySeq = sdpSlotDirectory::getCount(
        sdpPhyPage::getSlotDirStartPtr((UChar*)aNode) );

    return;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : 
 * ------------------------------------------------------------------*
 * Node MBR�� ���Ѵ�.
 *********************************************************************/
IDE_RC stndrRTree::adjustNodeMBR( stndrHeader   * aIndex,
                                  sdpPhyPageHdr * aNode,
                                  stdMBR        * aInsertMBR,
                                  SShort          aUpdateKeySeq,
                                  stdMBR        * aUpdateMBR,
                                  SShort          aDeleteKeySeq,
                                  stdMBR        * aNodeMBR )
{
    stndrNodeHdr    * sNodeHdr;

    sNodeHdr = (stndrNodeHdr*)
        sdpPhyPage::getLogicalHdrStartPtr( (UChar*)aNode );

    if( STNDR_IS_LEAF_NODE(sNodeHdr) == ID_TRUE )
    {
        return adjustLNodeMBR( aIndex,
                               aNode,
                               aInsertMBR,
                               aUpdateKeySeq,
                               aUpdateMBR,
                               aDeleteKeySeq,
                               aNodeMBR );
    }
    else
    {
        return adjustINodeMBR( aIndex,
                               aNode,
                               aInsertMBR,
                               aUpdateKeySeq,
                               aUpdateMBR,
                               aDeleteKeySeq,
                               aNodeMBR );
    }
}

/*********************************************************************
 * FUNCTION DESCRIPTION : 
 * ------------------------------------------------------------------*
 * Internal Node�� MBR�� ���Ѵ�.
 *********************************************************************/
IDE_RC stndrRTree::adjustINodeMBR( stndrHeader   * /*aIndex*/,
                                   sdpPhyPageHdr * aNode,
                                   stdMBR        * aInsertMBR,
                                   SShort          aUpdateKeySeq,
                                   stdMBR        * aUpdateMBR,
                                   SShort          aDeleteKeySeq,
                                   stdMBR        * aNodeMBR )
{
    stndrIKey   * sIKey;
    stdMBR        sNodeMBR;
    stdMBR        sMBR;
    UChar       * sSlotDirPtr;
    UShort        sKeyCount;
    idBool        sIsFirst;
    SInt          i;


    sIsFirst = ID_TRUE;

    sSlotDirPtr = sdpPhyPage::getSlotDirStartPtr( (UChar*)aNode );
    sKeyCount   = sdpSlotDirectory::getCount( sSlotDirPtr );

    IDE_TEST( sKeyCount <= 0 );

    for( i = 0; i < sKeyCount; i++ )
    {
        IDE_TEST( sdpSlotDirectory::getPagePtrFromSlotNum( sSlotDirPtr, 
                                                           i,
                                                           (UChar**)&sIKey )
                  != IDE_SUCCESS );

        if( i == aDeleteKeySeq )
        {
            continue;
        }

        if( i == aUpdateKeySeq )
        {
            IDE_ASSERT( aUpdateMBR != NULL );
            sMBR = *aUpdateMBR;
        }
        else
        {
            STNDR_GET_MBR_FROM_IKEY( sMBR, sIKey );
        }

        if( sIsFirst == ID_TRUE )
        {
            sNodeMBR = sMBR;
            sIsFirst = ID_FALSE;
        }
        else
        {
            stdUtils::getMBRExtent( &sNodeMBR, &sMBR );
        }
    }

    // BUG-29039 codesonar ( Uninitialized Variable )
    // �߰����� ���Ѱ��
    IDE_TEST( sIsFirst == ID_TRUE );

    if( aInsertMBR != NULL )
    {
        stdUtils::getMBRExtent( &sNodeMBR, aInsertMBR );
    }

    *aNodeMBR = sNodeMBR;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : 
 * ------------------------------------------------------------------*
 * Leaf Node�� MBR�� ���Ѵ�.
 *********************************************************************/
IDE_RC stndrRTree::adjustLNodeMBR( stndrHeader   * /*aIndex*/,
                                   sdpPhyPageHdr * aNode,
                                   stdMBR        * aInsertMBR,
                                   SShort          aUpdateKeySeq,
                                   stdMBR        * aUpdateMBR,
                                   SShort          aDeleteKeySeq,
                                   stdMBR        * aNodeMBR )
{
    stndrLKey   * sLKey;
    stdMBR        sNodeMBR;
    stdMBR        sMBR;
    UChar       * sSlotDirPtr;
    UShort        sKeyCount;
    idBool        sIsFirst;
    SInt          i;

    
    sIsFirst = ID_TRUE;

    sSlotDirPtr = sdpPhyPage::getSlotDirStartPtr( (UChar*)aNode );
    sKeyCount   = sdpSlotDirectory::getCount( sSlotDirPtr );

    IDE_TEST( sKeyCount <= 0 );

    for( i = 0; i < sKeyCount; i++ )
    {
        IDE_TEST( sdpSlotDirectory::getPagePtrFromSlotNum( sSlotDirPtr, 
                                                           i,
                                                           (UChar**)&sLKey )
                  != IDE_SUCCESS );

        if( i == aDeleteKeySeq )
        {
            continue;
        }

        if( i == aUpdateKeySeq )
        {
            IDE_ASSERT( aUpdateMBR != NULL );
            sMBR = *aUpdateMBR;
        }
        else
        {
            STNDR_GET_MBR_FROM_LKEY( sMBR, sLKey );
        }

        if( sIsFirst == ID_TRUE )
        {
            sNodeMBR = sMBR;
            sIsFirst = ID_FALSE;
        }
        else
        {
            stdUtils::getMBRExtent( &sNodeMBR, &sMBR );
        }
    }

    // BUG-29039 codesonar ( Uninitialized Variable )
    // �߰����� ���Ѱ��
    IDE_TEST( sIsFirst == ID_TRUE );

    if( aInsertMBR != NULL )
    {
        stdUtils::getMBRExtent( &sNodeMBR, aInsertMBR );
    }
    
    *aNodeMBR = sNodeMBR;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : stndrRTree::softKeyStamping 
 * ------------------------------------------------------------------*
 * Internal SoftKeyStamping�� ���� Wrapper Function 
 *********************************************************************/
IDE_RC stndrRTree::softKeyStamping( sdrMtx          * aMtx,
                                    sdpPhyPageHdr   * aNode,
                                    UChar             aCTSlotNum,
                                    UChar           * aContext )
{
    stndrCallbackContext * sContext;

    sContext = (stndrCallbackContext*)aContext;
    return softKeyStamping( sContext->mIndex,
                            aMtx,
                            aNode,
                            aCTSlotNum );
}

/*********************************************************************
 * FUNCTION DESCRIPTION : stndrRTree::softKeyStamping 
 * ------------------------------------------------------------------*
 * Soft Key Stamping�� CTS�� STAMPED���¿��� ����ȴ�. 
 * CTS#�� ���� ��� KEY�鿡 ���ؼ� CTS#�� ���Ѵ�� �����Ѵ�. 
 * ���� CreateCTS�� ���Ѵ�� ����Ǹ� Key�� ���´� STABLE���·� 
 * ����ǰ�, LimitCTS�� ���Ѵ��� ���� DEAD���·� �����Ų��. 
 *********************************************************************/
IDE_RC stndrRTree::softKeyStamping( stndrHeader     * /* aIndex */,
                                    sdrMtx          * aMtx,
                                    sdpPhyPageHdr   * aNode,
                                    UChar             aCTSlotNum )
{
    UShort            sKeyCount;
    stndrLKey       * sLeafKey;
    UInt              i;
    stndrNodeHdr    * sNodeHdr;
    UShort            sKeyLen;
    UShort            sTotalDeadKeySize = 0;
    UShort          * sArrRefKey;
    UShort            sRefKeyCount;
    UShort            sAffectedKeyCount = 0;
    UChar           * sSlotDirPtr;
    smSCN             sCSSCNInfinite;

    
    SM_SET_SCN_CI_INFINITE( &sCSSCNInfinite );

    sNodeHdr = (stndrNodeHdr*)sdpPhyPage::getLogicalHdrStartPtr( (UChar*)aNode );
    
    sTotalDeadKeySize = sNodeHdr->mTotalDeadKeySize;
    
    IDE_TEST( sdrMiniTrans::writeLogRec(
                  aMtx,
                  (UChar*)aNode,
                  NULL,
                  ID_SIZEOF(aCTSlotNum),
                  SDR_STNDR_KEY_STAMPING)
              != IDE_SUCCESS );

    IDE_TEST( sdrMiniTrans::write(aMtx,
                                  (void*)&aCTSlotNum,
                                  ID_SIZEOF(aCTSlotNum))
              != IDE_SUCCESS );

    sSlotDirPtr = sdpPhyPage::getSlotDirStartPtr( (UChar*)aNode );
    sKeyCount  = sdpSlotDirectory::getCount( sSlotDirPtr );

    sdnIndexCTL::getRefKey( aNode,
                            aCTSlotNum,
                            &sRefKeyCount,
                            &sArrRefKey );

    for ( i = 0; i < SDN_CTS_MAX_KEY_CACHE; i++ )
    {
        if( sArrRefKey[i] == SDN_CTS_KEY_CACHE_NULL )
        {
            continue;
        }

        sAffectedKeyCount++;
        sLeafKey = (stndrLKey*)(((UChar*)aNode) + sArrRefKey[i]);

        if( STNDR_GET_CCTS_NO(sLeafKey) == aCTSlotNum )
        {
            STNDR_SET_CCTS_NO( sLeafKey , SDN_CTS_INFINITE );

            // Create CTS�� Stamping�� ���� �ʰ� Limit CTS�� Stamping��
            // ���� DEAD�����ϼ� �ֱ� ������ SKIP�Ѵ�. ���� 
            // STNDR_KEY_DELETED ���´� �������� �ʴ´�.
            if( STNDR_GET_STATE(sLeafKey) == STNDR_KEY_UNSTABLE )
            {
                STNDR_SET_STATE( sLeafKey , STNDR_KEY_STABLE );

                STNDR_SET_CSCN( sLeafKey , &sCSSCNInfinite );
            }
        }

        if( STNDR_GET_LCTS_NO(sLeafKey) == aCTSlotNum )
        {
            if( STNDR_GET_STATE( sLeafKey ) != STNDR_KEY_DELETED )
            {
                ideLog::log( IDE_SERVER_0,
                             "CTS slot number : %u"
                             "\nCTS key cache idx : %u\n",
                             aCTSlotNum, i );
                dumpIndexNode( aNode );
                IDE_ASSERT( 0 );
            }

            sKeyLen = getKeyLength( (UChar*)sLeafKey, ID_TRUE /* aIsLeaf */);
            sTotalDeadKeySize += sKeyLen + ID_SIZEOF( sdpSlotEntry );

            STNDR_SET_CCTS_NO( sLeafKey, SDN_CTS_INFINITE );
            STNDR_SET_LCTS_NO( sLeafKey , SDN_CTS_INFINITE );
            STNDR_SET_STATE( sLeafKey , STNDR_KEY_DEAD );

            STNDR_SET_LSCN( sLeafKey, &sCSSCNInfinite );
        }
    }

    if( sAffectedKeyCount < sRefKeyCount )
    {
        // full scan�ؼ� Key Stamping�� �Ѵ�.
        for( i = 0; i < sKeyCount; i++ )
        {
            IDE_TEST( sdpSlotDirectory::getPagePtrFromSlotNum(
                                                        (UChar*)sSlotDirPtr, 
                                                        i,
                                                        (UChar**)&sLeafKey )
                      != IDE_SUCCESS );

            if( STNDR_GET_CCTS_NO(sLeafKey) == aCTSlotNum )
            {
                STNDR_SET_CCTS_NO( sLeafKey , SDN_CTS_INFINITE );

                // Create CTS�� Stamping�� ���� �ʰ� Limit CTS�� Stamping��
                // ���� DEAD�����ϼ� �ֱ� ������ SKIP�Ѵ�. ���� 
                // STNDR_KEY_DELETED ���´� �������� �ʴ´�.
                if( STNDR_GET_STATE(sLeafKey) == STNDR_KEY_UNSTABLE )
                {
                    STNDR_SET_STATE( sLeafKey , STNDR_KEY_STABLE );
                    
                    STNDR_SET_CSCN( sLeafKey , &sCSSCNInfinite );
                    
                }
            }

            if( STNDR_GET_LCTS_NO(sLeafKey) == aCTSlotNum )
            {
                if( STNDR_GET_STATE( sLeafKey ) != STNDR_KEY_DELETED )
                {
                    ideLog::log( IDE_SERVER_0,
                                 "CTS slot number : %u"
                                 "\nKey sequeunce : %u\n",
                                 aCTSlotNum, i );
                    dumpIndexNode( aNode );
                    IDE_ASSERT( 0 );
                }
                
                sKeyLen = getKeyLength( (UChar*)sLeafKey , ID_TRUE /*aIsLeaf*/);
                sTotalDeadKeySize += sKeyLen + ID_SIZEOF( sdpSlotEntry );

                STNDR_SET_CCTS_NO( sLeafKey, SDN_CTS_INFINITE );
                STNDR_SET_LCTS_NO( sLeafKey , SDN_CTS_INFINITE );
                STNDR_SET_STATE( sLeafKey , STNDR_KEY_DEAD );

                STNDR_SET_LSCN( sLeafKey, &sCSSCNInfinite );
            }
        }
    }

    IDE_TEST( sdrMiniTrans::writeNBytes(
                  aMtx,
                  (UChar*)&sNodeHdr->mTotalDeadKeySize,
                  (void*)&sTotalDeadKeySize,
                  ID_SIZEOF(sNodeHdr->mTotalDeadKeySize))
              != IDE_SUCCESS );

    IDE_TEST( sdnIndexCTL::freeCTS(aMtx,
                                   aNode,
                                   aCTSlotNum,
                                   ID_TRUE)
              != IDE_SUCCESS );
    
#if DEBUG
    for( i = 0; i < sKeyCount; i++ )
    {
        IDE_TEST( sdpSlotDirectory::getPagePtrFromSlotNum(
                                                        (UChar*)sSlotDirPtr, 
                                                        i,
                                                        (UChar**)&sLeafKey )
                  != IDE_SUCCESS );

        if( STNDR_GET_STATE( sLeafKey ) == STNDR_KEY_DEAD )
        {
            continue;
        }

        // SoftKeyStamping�� �ߴµ��� CTS#�� ������� ���� Key�� ������
        // ����.
        IDE_ASSERT( STNDR_GET_CCTS_NO(sLeafKey) != aCTSlotNum );
        IDE_ASSERT( STNDR_GET_LCTS_NO(sLeafKey) != aCTSlotNum );
        if( (STNDR_GET_CCTS_NO( sLeafKey  ) == aCTSlotNum)
            ||
            (STNDR_GET_LCTS_NO( sLeafKey  ) == aCTSlotNum) )
        {
            ideLog::log( IDE_SERVER_0,
                         "CTS slot number : %u"
                         "\nKey sequence : %u\n",
                         aCTSlotNum, i );
            dumpIndexNode( aNode );
            IDE_ASSERT( 0 );
        }
    }
#endif
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

/*********************************************************************
 * FUNCTION DESCRIPTION : stndrRTree::hardKeyStamping 
 * ------------------------------------------------------------------*
 * Internal HardKeyStamping�� ���� Wrapper Function 
 *********************************************************************/
IDE_RC stndrRTree::hardKeyStamping( idvSQL        * aStatistics,
                                    sdrMtx        * aMtx,
                                    sdpPhyPageHdr * aNode,
                                    UChar           aCTSlotNum,
                                    UChar         * aContext,
                                    idBool        * aSuccess )
{
    stndrCallbackContext * sContext = (stndrCallbackContext*)aContext;

    IDE_TEST( hardKeyStamping( aStatistics,
                               sContext->mIndex,
                               aMtx,
                               aNode,
                               aCTSlotNum,
                               aSuccess )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

/*********************************************************************
 * FUNCTION DESCRIPTION : stndrRTree::hardKeyStamping 
 * ------------------------------------------------------------------*
 * �ʿ��ϴٸ� TSS�� ���� CommitSCN�� ���ؿͼ�, SoftKeyStamping�� 
 * �õ� �Ѵ�. 
 *********************************************************************/
IDE_RC stndrRTree::hardKeyStamping( idvSQL          * aStatistics,
                                    stndrHeader     * aIndex,
                                    sdrMtx          * aMtx,
                                    sdpPhyPageHdr   * aNode,
                                    UChar             aCTSlotNum,
                                    idBool          * aIsSuccess )
{
    sdnCTL  * sCTL;
    sdnCTS  * sCTS;
    idBool    sSuccess = ID_TRUE;
    smSCN     sSysMinDskViewSCN;
    smSCN     sCommitSCN;
    
    sCTL = sdnIndexCTL::getCTL( aNode );
    sCTS = sdnIndexCTL::getCTS( sCTL, aCTSlotNum );

    if( sCTS->mState == SDN_CTS_UNCOMMITTED )
    {
        IDE_TEST( sdnIndexCTL::delayedStamping( aStatistics,
                                                NULL,        /* aTrans */
                                                sCTS,
                                                SDB_SINGLE_PAGE_READ,
                                                SM_SCN_INIT, /* aStmtViewSCN */ 
                                                &sCommitSCN,
                                                &sSuccess )
                  != IDE_SUCCESS );
    }

    if( sSuccess == ID_TRUE )
    {
        IDE_DASSERT( sCTS->mState == SDN_CTS_STAMPED );

        SMX_GET_MIN_DISK_VIEW( &sSysMinDskViewSCN );

        sCommitSCN = sdnIndexCTL::getCommitSCN( sCTS );

        if( SM_SCN_IS_LT( &sCommitSCN, &sSysMinDskViewSCN ) )
        {
            IDE_TEST( softKeyStamping( aIndex,
                                       aMtx,
                                       aNode,
                                       aCTSlotNum )
                      != IDE_SUCCESS );
            *aIsSuccess = ID_TRUE;
        }
        else
        {
            *aIsSuccess = ID_FALSE;
        }
    }
    else
    {
        *aIsSuccess = ID_FALSE;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : 
 * ------------------------------------------------------------------*
 * Meta Page�� ������ �����Ѵ�.
 *********************************************************************/
IDE_RC stndrRTree::buildMeta( idvSQL    * aStatistics,
                              void      * aTrans,
                              void      * aIndex )
{
    stndrHeader     * sIndex = (stndrHeader*)aIndex;
    sdrMtx            sMtx;
    scPageID        * sRootNode;
    scPageID        * sFreeNodeHead;
    ULong           * sFreeNodeCnt;
    idBool          * sIsConsistent;
    smLSN           * sCompletionLSN;
    idBool          * sIsCreatedWithLogging;
    idBool          * sIsCreatedWithForce;
    UShort          * sConvexhullPointNum;
    stndrStatistic    sDummyStat;
    idBool            sMtxStart = ID_FALSE;
    sdrMtxLogMode     sLogMode;
    

    // BUG-27328 CodeSonar::Uninitialized Variable
    idlOS::memset( &sDummyStat, 0, ID_SIZEOF(sDummyStat) );

    // index runtime header�� mLogging�� DML���� ���Ǵ� ���̹Ƿ�
    // index build �� �׻� ID_TRUE�� �ʱ�ȭ��Ŵ
    sIndex->mSdnHeader.mLogging = ID_TRUE;

    sIsConsistent         = &(sIndex->mSdnHeader.mIsConsistent);
    sCompletionLSN        = &(sIndex->mSdnHeader.mCompletionLSN);
    sIsCreatedWithLogging = &(sIndex->mSdnHeader.mIsCreatedWithLogging);
    sIsCreatedWithForce   = &(sIndex->mSdnHeader.mIsCreatedWithForce);
    
    sRootNode             = &(sIndex->mRootNode);
    sFreeNodeHead         = &(sIndex->mFreeNodeHead);
    sFreeNodeCnt          = &(sIndex->mFreeNodeCnt);
    sConvexhullPointNum   = &(sIndex->mConvexhullPointNum);

    sLogMode  = (*sIsCreatedWithLogging == ID_TRUE) ?
        SDR_MTX_LOGGING : SDR_MTX_NOLOGGING;

    IDE_TEST( sdrMiniTrans::begin(aStatistics,
                                  &sMtx,
                                  aTrans,
                                  sLogMode,
                                  ID_TRUE,/*MtxUndoable(PROJ-2162)*/
                                  gMtxDLogType ) != IDE_SUCCESS );
    sMtxStart = ID_TRUE;

    sdrMiniTrans::setNologgingPersistent( &sMtx );

    IDE_TEST( setIndexMetaInfo( aStatistics,
                                sIndex,
                                &sDummyStat,
                                &sMtx,
                                sRootNode,
                                sFreeNodeHead,
                                sFreeNodeCnt,
                                sIsCreatedWithLogging,
                                sIsConsistent,
                                sIsCreatedWithForce,
                                sCompletionLSN,
                                sConvexhullPointNum ) != IDE_SUCCESS );

    sMtxStart = ID_FALSE;
    IDE_TEST( sdrMiniTrans::commit(&sMtx) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if(sMtxStart == ID_TRUE)
    {
        (void)sdrMiniTrans::rollback( &sMtx );
    }

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : stndrBTree::backupRuntimeHeader             *
 * ------------------------------------------------------------------*
 * MtxRollback���� ���� RuntimeHeader ������ ����, ������ ����صд�.
 *
 * aMtx      - [In] ��� Mtx
 * aIndex    - [In] ����� RuntimeHeader
 *********************************************************************/
IDE_RC stndrRTree::backupRuntimeHeader( sdrMtx      * aMtx,
                                        stndrHeader * aIndex )
{
    /* Mtx�� Abort�Ǹ�, PageImage�� Rollback���� RuntimeValud��
     * �������� �ʽ��ϴ�. 
     * ���� Rollback�� ���� ������ �����ϵ��� �մϴ�.
     * ������ ��� Page�� XLatch�� ��� ������ ���ÿ� �� Mtx��
     * �����մϴ�. ���� ������� �ϳ��� ������ �˴ϴ�.*/
    sdrMiniTrans::addPendingJob( aMtx,
                                 ID_FALSE, // isCommitJob
                                 stndrRTree::restoreRuntimeHeader,
                                 (void*)aIndex );

    aIndex->mRootNode4MtxRollback        = aIndex->mRootNode;
    aIndex->mEmptyNodeHead4MtxRollback   = aIndex->mEmptyNodeHead;
    aIndex->mEmptyNodeTail4MtxRollback   = aIndex->mEmptyNodeTail;
    aIndex->mFreeNodeCnt4MtxRollback     = aIndex->mFreeNodeCnt;
    aIndex->mFreeNodeHead4MtxRollback    = aIndex->mFreeNodeHead;
    aIndex->mFreeNodeSCN4MtxRollback     = aIndex->mFreeNodeSCN;
    aIndex->mKeyCount4MtxRollback        = aIndex->mKeyCount;
    aIndex->mTreeMBR4MtxRollback         = aIndex->mTreeMBR;
    aIndex->mInitTreeMBR4MtxRollback     = aIndex->mInitTreeMBR;
    aIndex->mVirtualRootNode4MtxRollback = aIndex->mVirtualRootNode;

    return IDE_SUCCESS;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : stndrRTree::restoreRuntimeHeader            *
 * ------------------------------------------------------------------*
 * MtxRollback���� ���� RuntimeHeader�� Meta������ ������
 *
 * aMtx      - [In] ��� Mtx
 * aIndex    - [In] ����� RuntimeHeader
 *********************************************************************/
IDE_RC stndrRTree::restoreRuntimeHeader( void      * aIndex )
{
    stndrHeader  * sIndex;

    IDE_ASSERT( aIndex != NULL );

    sIndex = (stndrHeader*) aIndex;

    sIndex->mRootNode        = sIndex->mRootNode4MtxRollback;
    sIndex->mEmptyNodeHead   = sIndex->mEmptyNodeHead4MtxRollback;
    sIndex->mEmptyNodeTail   = sIndex->mEmptyNodeTail4MtxRollback;
    sIndex->mFreeNodeCnt     = sIndex->mFreeNodeCnt4MtxRollback;
    sIndex->mFreeNodeHead    = sIndex->mFreeNodeHead4MtxRollback;
    sIndex->mFreeNodeSCN     = sIndex->mFreeNodeSCN4MtxRollback;
    sIndex->mKeyCount        = sIndex->mKeyCount4MtxRollback;
    sIndex->mTreeMBR         = sIndex->mTreeMBR4MtxRollback;
    sIndex->mInitTreeMBR     = sIndex->mInitTreeMBR4MtxRollback;
    sIndex->mVirtualRootNode = sIndex->mVirtualRootNode4MtxRollback;

    return IDE_SUCCESS;
}


/*********************************************************************
 * FUNCTION DESCRIPTION : stndrRTree::setIndexMetaInfo
 * ------------------------------------------------------------------*
 * Meta Page�� ����� ��������� ������ �α��Ѵ�.
 *********************************************************************/
IDE_RC stndrRTree::setIndexMetaInfo( idvSQL         * aStatistics,
                                     stndrHeader    * aIndex,
                                     stndrStatistic * aIndexStat,
                                     sdrMtx         * aMtx,
                                     scPageID       * aRootPID,
                                     scPageID       * aFreeNodeHead,
                                     ULong          * aFreeNodeCnt,
                                     idBool         * aIsCreatedWithLogging,
                                     idBool         * aIsConsistent,
                                     idBool         * aIsCreatedWithForce,
                                     smLSN          * aNologgingCompletionLSN,
                                     UShort         * aConvexhullPointNum )
{
    UChar       * sPage = NULL;
    stndrMeta   * sMeta = NULL;
    idBool        sIsSuccess = ID_FALSE;
    

    IDE_ASSERT( (aRootPID                != NULL) ||
                (aFreeNodeHead           != NULL) ||
                (aFreeNodeCnt            != NULL) ||
                (aIsConsistent           != NULL) ||
                (aIsCreatedWithLogging   != NULL) ||
                (aIsCreatedWithForce     != NULL) ||
                (aNologgingCompletionLSN != NULL) ||
                (aConvexhullPointNum     != NULL) );

    sPage = sdrMiniTrans::getPagePtrFromPageID(
        aMtx,
        aIndex->mSdnHeader.mIndexTSID,
        SD_MAKE_PID( aIndex->mSdnHeader.mMetaRID ));
    
    if( sPage == NULL )
    {
        IDE_TEST( stndrRTree::getPage(
                         aStatistics,
                         &(aIndexStat->mMetaPage),
                         aIndex->mSdnHeader.mIndexTSID,
                         SD_MAKE_PID( aIndex->mSdnHeader.mMetaRID ),
                         SDB_X_LATCH,
                         SDB_WAIT_NORMAL,
                         aMtx,
                         (UChar**)&sPage,
                         &sIsSuccess ) != IDE_SUCCESS );
    }

    sMeta = (stndrMeta*)( sPage + SMN_INDEX_META_OFFSET );

    if( aRootPID != NULL )
    {
        IDE_TEST( sdrMiniTrans::writeNBytes( aMtx,
                                             (UChar*)&sMeta->mRootNode,
                                             (void*)aRootPID,
                                             ID_SIZEOF(*aRootPID) )
                  != IDE_SUCCESS );
    }

    if( aFreeNodeHead != NULL )
    {
        IDE_TEST( sdrMiniTrans::writeNBytes( aMtx,
                                             (UChar*)&sMeta->mFreeNodeHead,
                                             (void*)aFreeNodeHead,
                                             ID_SIZEOF(*aFreeNodeHead) )
                  != IDE_SUCCESS );
    }
    
    if( aFreeNodeCnt != NULL )
    {
        IDE_TEST( sdrMiniTrans::writeNBytes( aMtx,
                                             (UChar*)&sMeta->mFreeNodeCnt,
                                             (void*)aFreeNodeCnt,
                                             ID_SIZEOF(*aFreeNodeCnt) )
                  != IDE_SUCCESS );
    }
    
    if( aIsConsistent != NULL )
    {
        IDE_TEST( sdrMiniTrans::writeNBytes( aMtx,
                                             (UChar*)&sMeta->mIsConsistent,
                                             (void*)aIsConsistent,
                                             ID_SIZEOF(*aIsConsistent) )
                  != IDE_SUCCESS );
    }
    
    if( aIsCreatedWithLogging != NULL )
    {
        IDE_TEST( sdrMiniTrans::writeNBytes( aMtx,
                                             (UChar*)&sMeta->mIsCreatedWithLogging,
                                             (void*)aIsCreatedWithLogging,
                                             ID_SIZEOF(*aIsCreatedWithLogging) )
                  != IDE_SUCCESS );
    }
    
    if( aIsCreatedWithForce != NULL )
    {
        IDE_TEST( sdrMiniTrans::writeNBytes( aMtx,
                                             (UChar*)&sMeta->mIsCreatedWithForce,
                                             (void*)aIsCreatedWithForce,
                                             ID_SIZEOF(*aIsCreatedWithForce) )
                  != IDE_SUCCESS );
    }
    
    if( aNologgingCompletionLSN != NULL )
    {
        IDE_TEST( sdrMiniTrans::writeNBytes(
                      aMtx,
                      (UChar*)&(sMeta->mNologgingCompletionLSN.mFileNo),
                      (void*)&(aNologgingCompletionLSN->mFileNo),
                      ID_SIZEOF(aNologgingCompletionLSN->mFileNo) )
                  != IDE_SUCCESS );
        IDE_TEST( sdrMiniTrans::writeNBytes(
                      aMtx,
                      (UChar*)&(sMeta->mNologgingCompletionLSN.mOffset),
                      (void*)&(aNologgingCompletionLSN->mOffset),
                      ID_SIZEOF(aNologgingCompletionLSN->mOffset) )
                  != IDE_SUCCESS );
    }

    if( aConvexhullPointNum != NULL )
    {
        IDE_TEST( sdrMiniTrans::writeNBytes( aMtx,
                                             (UChar*)&sMeta->mConvexhullPointNum,
                                             (void*)aConvexhullPointNum,
                                             ID_SIZEOF(*aConvexhullPointNum) )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : stndrRTree::preparePages
 * ------------------------------------------------------------------*
 * �־��� ������ŭ�� �������� �Ҵ������ ������ �˻��Ѵ�.
 *********************************************************************/
IDE_RC stndrRTree::preparePages( idvSQL         * aStatistics,
                                 stndrHeader    * aIndex,
                                 sdrMtx         * aMtx,
                                 idBool         * aMtxStart,
                                 UInt             aNeedPageCnt )
{
    sdcUndoSegment  * sUDSegPtr;
    smSCN             sSysMinDskViewSCN;
    

    SMX_GET_MIN_DISK_VIEW( &sSysMinDskViewSCN );

    if( (aIndex->mFreeNodeCnt < aNeedPageCnt) ||
        SM_SCN_IS_GE(&aIndex->mFreeNodeSCN, &sSysMinDskViewSCN) )
    {
        if( sdpSegDescMgr::getSegMgmtOp(
                      &(aIndex->mSdnHeader.mSegmentDesc))->mPrepareNewPages(
                          aStatistics,
                          aMtx,
                          aIndex->mSdnHeader.mIndexTSID,
                          &(aIndex->mSdnHeader.mSegmentDesc.mSegHandle),
                          aNeedPageCnt)
                  != IDE_SUCCESS )
        {
            *aMtxStart = ID_FALSE;
            IDE_TEST( sdrMiniTrans::commit(aMtx) != IDE_SUCCESS );
            IDE_TEST( 1 );
        }
    }

    /* BUG-24400 ��ũ �ε��� SMO�߿� Undo ������������ Rollback �ؼ��� �ȵ˴ϴ�.
     *           SMO ���� �����ϱ� ���� Undo ���׸�Ʈ�� Undo ������ �ϳ��� Ȯ���� �Ŀ�
     *           �����Ͽ��� �Ѵ�. Ȯ������ ���ϸ�, SpaceNotEnough ������ ��ȯ�Ѵ�. */
    if ( ((smxTrans*)aMtx->mTrans)->getTXSegEntry() != NULL )
    {
        sUDSegPtr = smxTrans::getUDSegPtr( (smxTrans*)aMtx->mTrans );
        if( sUDSegPtr == NULL )
        {
            ideLog::log( IDE_SERVER_0, "Transaction TX Segment entry info:\n" );
            ideLog::logMem( IDE_SERVER_0,
                            (UChar *)(((smxTrans *)aMtx->mTrans)->getTXSegEntry()),
                            ID_SIZEOF(*((smxTrans *)aMtx->mTrans)->getTXSegEntry()) );
            IDE_ASSERT( 0 );
        }

        if( sUDSegPtr->prepareNewPage( aStatistics, aMtx )
            != IDE_SUCCESS )
        {
            *aMtxStart = ID_FALSE;
            IDE_TEST( sdrMiniTrans::commit(aMtx) != IDE_SUCCESS );
            IDE_TEST( 1 );
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
 * FUNCTION DESCRIPTION : stndrRTree::fixPage
 * ------------------------------------------------------------------*
 * To fix BUG-18252
 * �ε��� �������� ��Ÿ�������� ���� �󵵿� ���� ������� ����
 *********************************************************************/
IDE_RC stndrRTree::fixPage( idvSQL          * aStatistics,
                            stndrPageStat   * aPageStat,
                            scSpaceID         aSpaceID,
                            scPageID          aPageID,
                            UChar          ** aRetPagePtr,
                            idBool          * aTrySuccess )
{
    idvSQL      * sStatistics;
    idvSession    sDummySession;
    idvSQL        sDummySQL;
    ULong         sGetPageCount;
    ULong         sReadPageCount;

    
    sDummySQL.mGetPageCount = 0;
    sDummySQL.mReadPageCount = 0;

    if( aStatistics == NULL )
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

    sGetPageCount = sStatistics->mGetPageCount;
    sReadPageCount = sStatistics->mReadPageCount;

    IDE_TEST( sdbBufferMgr::fixPageByPID( sStatistics,
                                          aSpaceID,
                                          aPageID,
                                          aRetPagePtr,
                                          aTrySuccess )
              != IDE_SUCCESS );

    aPageStat->mGetPageCount += sStatistics->mGetPageCount - sGetPageCount;
    aPageStat->mReadPageCount += sStatistics->mReadPageCount - sReadPageCount;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : stndrRTree::unfixPage
 * ------------------------------------------------------------------*
 * To fix BUG-18252
 * �ε��� �������� ��Ÿ�������� ���� �󵵿� ���� ������� ����
 *********************************************************************/
IDE_RC stndrRTree::unfixPage( idvSQL * aStatistics, UChar * aPagePtr )
{
    IDE_TEST( sdbBufferMgr::unfixPage( aStatistics, aPagePtr ) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : stndrRTree::setFreeNodeInfo
 * ------------------------------------------------------------------*
 * To fix BUG-23287
 * Free Node ������ Meta �������� �����Ѵ�.
 * 1. Free Node Head�� ����
 * 2. Free Node Count�� ����
 * 3. Free Node SCN�� ����
 *********************************************************************/
IDE_RC stndrRTree::setFreeNodeInfo( idvSQL          * aStatistics,
                                    stndrHeader     * aIndex,
                                    stndrStatistic  * aIndexStat,
                                    sdrMtx          * aMtx,
                                    scPageID          aFreeNodeHead,
                                    ULong             aFreeNodeCnt,
                                    smSCN           * aFreeNodeSCN )
{
    UChar       * sPage;
    stndrMeta   * sMeta;
    idBool        sIsSuccess;

    IDE_DASSERT( aMtx != NULL );

    sPage = sdrMiniTrans::getPagePtrFromPageID(
        aMtx,
        aIndex->mSdnHeader.mIndexTSID,
        SD_MAKE_PID(aIndex->mSdnHeader.mMetaRID) );
    
    if( sPage == NULL )
    {
        // SegHdr ������ �����͸� ����
        IDE_TEST( stndrRTree::getPage(
                      aStatistics,
                      &(aIndexStat->mMetaPage),
                      aIndex->mSdnHeader.mIndexTSID,
                      SD_MAKE_PID( aIndex->mSdnHeader.mMetaRID ),
                      SDB_X_LATCH,
                      SDB_WAIT_NORMAL,
                      aMtx,
                      (UChar**)&sPage,
                      &sIsSuccess ) != IDE_SUCCESS );
    }

    sMeta = (stndrMeta*)( sPage + SMN_INDEX_META_OFFSET );

    IDE_TEST( sdrMiniTrans::writeNBytes( aMtx,
                                         (UChar*)&sMeta->mFreeNodeHead,
                                         (void*)&aFreeNodeHead,
                                         ID_SIZEOF(aFreeNodeHead) )
              != IDE_SUCCESS );

    IDE_TEST( sdrMiniTrans::writeNBytes( aMtx,
                                         (UChar*)&sMeta->mFreeNodeCnt,
                                         (void*)&aFreeNodeCnt,
                                         ID_SIZEOF(aFreeNodeCnt) )
              != IDE_SUCCESS );

    /* BUG-32764 [st-disk-index] The RTree module writes the invalid log of
     * FreeNodeSCN */
    IDE_TEST( sdrMiniTrans::writeNBytes( aMtx,
                                         (UChar*)&sMeta->mFreeNodeSCN,
                                         (void*)aFreeNodeSCN,
                                         ID_SIZEOF(*aFreeNodeSCN) )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : 
 * ------------------------------------------------------------------*
 * Key Value�� Length�� ���Ѵ�.
 *********************************************************************/
UShort stndrRTree::getKeyValueLength()
{
    UShort sTotalKeySize;

    sTotalKeySize = ID_SIZEOF( stdMBR );

    return sTotalKeySize;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : 
 * ------------------------------------------------------------------*
 * Leaf Node�� Key�� �����Ѵ�. �� �Լ��� Leaf Node�� X-Latch�� ����
 * ���¿��� ȣ��ȴ�.
 *********************************************************************/
IDE_RC stndrRTree::insertKeyIntoLeafNode( sdrMtx                * aMtx,
                                          stndrHeader           * aIndex,
                                          smSCN                 * aInfiniteSCN,
                                          sdpPhyPageHdr         * aLeafNode,
                                          SShort                * aLeafKeySeq,
                                          stndrKeyInfo          * aKeyInfo,
                                          UChar                   aCTSlotNum,
                                          idBool                * aIsSuccess )
{
    UShort sKeyValueLen;
    UShort sKeyLength;
    

    *aIsSuccess = ID_TRUE;

    sKeyValueLen = getKeyValueLength();

    // BUG-26060 [SN] BTree Top-Down Build�� �߸��� CTS#��
    // �����ǰ� �ֽ��ϴ�.
    if( aKeyInfo->mKeyState == STNDR_KEY_STABLE )
    {
        sKeyLength = STNDR_LKEY_LEN( sKeyValueLen, STNDR_KEY_TB_CTS );
    }
    else
    {
        if( aCTSlotNum == SDN_CTS_INFINITE )
        {
            sKeyLength = STNDR_LKEY_LEN( sKeyValueLen, STNDR_KEY_TB_KEY );
        }
        else
        {
            sKeyLength = STNDR_LKEY_LEN( sKeyValueLen, STNDR_KEY_TB_CTS );
        }
    }
    
    if( canAllocLeafKey( aMtx,
                         aIndex,
                         aLeafNode,
                         (UInt)sKeyLength,
                         aLeafKeySeq ) != IDE_SUCCESS )
    {
        ideLog::log( IDE_SERVER_0,
                     "Leaf Key sequence : %u"
                     ", Leaf Key length : %u"
                     ", CTS Slot Num : %u\n",
                     *aLeafKeySeq, sKeyLength, aCTSlotNum );

        ideLog::log( IDE_SERVER_0, "Key info dump:\n" );
        ideLog::logMem( IDE_SERVER_0,
                        (UChar *)aKeyInfo,
                        ID_SIZEOF(stndrKeyInfo) );

        dumpIndexNode( aLeafNode );
        IDE_ASSERT(0);
    }
    
    // Top-Down Build�� �ƴ� ����߿� CTS �Ҵ��� ������ ����
    // TBK�� Ű�� �����Ѵ�.
    if( (aKeyInfo->mKeyState != STNDR_KEY_STABLE) &&
        (aCTSlotNum == SDN_CTS_INFINITE) )
    {
        IDE_TEST( insertLeafKeyWithTBK( aMtx,
                                        aIndex,
                                        aInfiniteSCN,
                                        aLeafNode,
                                        aKeyInfo,
                                        sKeyValueLen,
                                        *aLeafKeySeq )
                  != IDE_SUCCESS );
    }
    else
    {
        IDE_TEST( insertLeafKeyWithTBT( aMtx,
                                        aIndex,
                                        aCTSlotNum,
                                        aInfiniteSCN,
                                        aLeafNode,
                                        aKeyInfo,
                                        sKeyValueLen,
                                        *aLeafKeySeq )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : 
 * ------------------------------------------------------------------*
 * sdpPhyPage::getNonFragFreeSize�� ���� wrapper �Լ��̴�.
 * ����� Key Count�� aIndex->mMaxKeyCount �̻��� ��� 0�� ��ȯ�Ѵ�.
 *********************************************************************/
UShort stndrRTree::getNonFragFreeSize( stndrHeader   * aIndex,
                                       sdpPhyPageHdr * aNode )
{
    UChar  * sSlotDirPtr;
    UShort   sKeyCount;
    
    sSlotDirPtr = sdpPhyPage::getSlotDirStartPtr( (UChar*)aNode );
    sKeyCount = sdpSlotDirectory::getCount( sSlotDirPtr );

    if( sKeyCount >= aIndex->mMaxKeyCount )
    {
        return 0;
    }

    return sdpPhyPage::getNonFragFreeSize(aNode);
}

/*********************************************************************
 * FUNCTION DESCRIPTION : 
 * ------------------------------------------------------------------*
 * sdpPhyPage::getTotalFreeSize�� ���� wrapper �Լ��̴�.
 * ����� Key Count�� aIndex->mMaxKeyCount �̻��� ��� 0�� ��ȯ�Ѵ�.
 *********************************************************************/
UShort stndrRTree::getTotalFreeSize( stndrHeader   * aIndex,
                                     sdpPhyPageHdr * aNode )
{
    UChar  * sSlotDirPtr;
    UShort   sKeyCount;
    
    sSlotDirPtr = sdpPhyPage::getSlotDirStartPtr( (UChar*)aNode );
    sKeyCount = sdpSlotDirectory::getCount( sSlotDirPtr );

    if( sKeyCount >= aIndex->mMaxKeyCount )
    {
        return 0;
    }

    return sdpPhyPage::getTotalFreeSize(aNode);
}

/*********************************************************************
 * FUNCTION DESCRIPTION : 
 * ------------------------------------------------------------------*
 * Page�� �־��� ũ���� slot�� �Ҵ��� �� �ִ��� �˻��ϰ�, �ʿ��ϸ�
 * compactPage���� �����Ѵ�. 
 *********************************************************************/
IDE_RC stndrRTree::canAllocInternalKey( sdrMtx          * aMtx,
                                        stndrHeader     * aIndex,
                                        sdpPhyPageHdr   * aNode,
                                        UInt              aSaveSize,
                                        idBool            aExecCompact,
                                        idBool            aIsLogging )
{
    UShort            sNeededFreeSize;
    UShort            sBeforeFreeSize;
    stndrNodeHdr    * sNodeHdr;

    sNeededFreeSize = aSaveSize + ID_SIZEOF(sdpSlotEntry);
    
    if( getNonFragFreeSize(aIndex, aNode) < sNeededFreeSize )
    {
        sNodeHdr = (stndrNodeHdr*)
            sdpPhyPage::getLogicalHdrStartPtr( (UChar*)aNode );

        sBeforeFreeSize = getTotalFreeSize( aIndex, aNode );

        // compact page�� �ص� slot�� �Ҵ���� ���ϴ� ���
        IDE_TEST( (UInt)(sBeforeFreeSize + sNodeHdr->mTotalDeadKeySize) <
                  (UInt)sNeededFreeSize );

        if( aExecCompact == ID_TRUE )
        {
            IDE_TEST( compactPage( aMtx,
                                   aNode,
                                   aIsLogging ) != IDE_SUCCESS);
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : 
 * ------------------------------------------------------------------*
 * Page�� �־��� ũ���� slot�� �Ҵ��� �� �ִ��� �˻��ϰ�, �ʿ��ϸ�
 * compactPage���� �����Ѵ�. 
 *********************************************************************/
IDE_RC stndrRTree::canAllocLeafKey( sdrMtx          * aMtx,
                                    stndrHeader     * aIndex,
                                    sdpPhyPageHdr   * aNode,
                                    UInt              aSaveSize,
                                    SShort          * aKeySeq )
{
    UShort            sNeedFreeSize;
    UShort            sBeforeFreeSize;
    stndrNodeHdr    * sNodeHdr;

    
    sNeedFreeSize = aSaveSize + ID_SIZEOF(sdpSlotEntry);
    
    if( getNonFragFreeSize( aIndex, aNode ) < sNeedFreeSize )
    {
        sNodeHdr = (stndrNodeHdr*)
            sdpPhyPage::getLogicalHdrStartPtr( (UChar*)aNode );

        sBeforeFreeSize = getTotalFreeSize( aIndex, aNode );;

        if( sBeforeFreeSize + sNodeHdr->mTotalDeadKeySize < sNeedFreeSize )
        {
            if( aKeySeq != NULL )
            {
                adjustKeyPosition( aNode, aKeySeq );
            }
                
            IDE_TEST( compactPage( aMtx,
                                   aNode,
                                   ID_TRUE ) != IDE_SUCCESS );

            // �� ���� �Ҵ��� �� ���� ��� �̹Ƿ� FAILURE ó���Ѵ�.
            IDE_TEST( 1 );
        }
        else
        {
            if( aKeySeq != NULL )
            {
                adjustKeyPosition( aNode, aKeySeq );
            }
            
            IDE_TEST( compactPage( aMtx,
                                   aNode,
                                   ID_TRUE ) != IDE_SUCCESS);
            
            IDE_ASSERT( getNonFragFreeSize(aIndex, aNode) >= sNeedFreeSize );
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : 
 * ------------------------------------------------------------------*
 * Ű ���� �� �߻��� page split�� ���� ���� ��带 compact�Ѵ�.
 * Logging�� �� �� ���� �����Լ��� ȣ���Ѵ�.
 *********************************************************************/
IDE_RC stndrRTree::compactPage( sdrMtx          * aMtx,
                                sdpPhyPageHdr   * aPage,
                                idBool            aIsLogging )
{
    stndrNodeHdr    * sNodeHdr;
    

    sNodeHdr = (stndrNodeHdr*)
        sdpPhyPage::getLogicalHdrStartPtr( (UChar*)aPage );

    if( aIsLogging == ID_TRUE )
    {
        IDE_TEST( sdrMiniTrans::writeLogRec(
                      aMtx,
                      (UChar*)aPage,
                      NULL, // value
                      0,    // valueSize
                      SDR_STNDR_COMPACT_INDEX_PAGE )
                  != IDE_SUCCESS );
    }

    if( STNDR_IS_LEAF_NODE(sNodeHdr) == ID_TRUE )
    {
        compactPageLeaf( aPage );
    }
    else
    {
        compactPageInternal( aPage );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

/*********************************************************************
 * FUNCTION DESCRIPTION :
 * ------------------------------------------------------------------*
 * Ű ���� �� �߻��� page split�� ���� ���� ��带 compact�Ѵ�.
 *********************************************************************/
IDE_RC stndrRTree::compactPageInternal( sdpPhyPageHdr * aPage )
{

    SInt          i;
    UShort        sTmpBuf[SD_PAGE_SIZE]; // 2 * Page size -> align ����...
    UChar       * sTmpPage;
    UChar       * sSlotDirPtr;
    UShort        sKeyLength;
    UShort        sAllowedSize;
    scOffset      sSlotOffset;
    UChar       * sSrcKey;
    UChar       * sDstKey;
    UShort        sKeyCount;
    

    sSlotDirPtr = sdpPhyPage::getSlotDirStartPtr( (UChar*)aPage );
    sKeyCount  = sdpSlotDirectory::getCount( sSlotDirPtr );

    sTmpPage = ((vULong)sTmpBuf) % SD_PAGE_SIZE == 0
        ? (UChar*)sTmpBuf
        : (UChar*)sTmpBuf + SD_PAGE_SIZE - (((vULong)sTmpBuf) % SD_PAGE_SIZE);

    idlOS::memcpy( sTmpPage, aPage, SD_PAGE_SIZE );
    
    if( aPage->mPageID != ((sdpPhyPageHdr*)sTmpPage)->mPageID )
    {
        dumpIndexNode( aPage );
        dumpIndexNode( (sdpPhyPageHdr *)sTmpPage );
        IDE_ASSERT( 0 );
    }

    (void)sdpPhyPage::reset( aPage,
                             ID_SIZEOF(stndrNodeHdr),
                             NULL );

    sdpSlotDirectory::init( aPage );
    sSlotDirPtr = sdpPhyPage::getSlotDirStartPtr(sTmpPage);

    for( i = 0; i < sKeyCount; i++ )
    {
        IDE_TEST( sdpSlotDirectory::getPagePtrFromSlotNum( sSlotDirPtr, 
                                                           i,
                                                           &sSrcKey )
                  != IDE_SUCCESS );

        sKeyLength = getKeyLength( sSrcKey, ID_FALSE ); //aIsLeaf

        IDE_ASSERT( sdpPhyPage::allocSlot( (sdpPhyPageHdr*)aPage,
                                           i,
                                           sKeyLength,
                                           ID_TRUE,
                                           &sAllowedSize,
                                           &sDstKey,
                                           &sSlotOffset,
                                           1 )
                    == IDE_SUCCESS );
        
        idlOS::memcpy( sDstKey, sSrcKey, sKeyLength );
        // Insert Logging�� �ʿ� ����.
    }
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : 
 * ------------------------------------------------------------------*
 * Ű ���� �� �߻��� page split�� ���� ���� ��带 compact�Ѵ�.
 * Compaction���Ŀ� ���� CTS.refKey ������ invalid�ϱ� ������ �̸�
 * �������� �ʿ䰡 �ִ�. 
 *********************************************************************/
IDE_RC stndrRTree::compactPageLeaf( sdpPhyPageHdr * aPage )
{
    SInt              i;
    UShort            sTmpBuf[SD_PAGE_SIZE]; // 2 * Page size -> align ����...
    UChar           * sTmpPage;
    UChar           * sSlotDirPtr;
    UShort            sKeyLength;
    UShort            sAllowedSize;
    scOffset          sSlotOffset;
    UChar           * sSrcKey;
    UChar           * sDstKey;
    UShort            sKeyCount;
    stndrLKey       * sLeafKey;
    UChar             sCreateCTS;
    UChar             sLimitCTS;
    stndrNodeHdr    * sNodeHdr;
    SShort            sKeySeq;
    UChar           * sDummy;
    

    sSlotDirPtr = sdpPhyPage::getSlotDirStartPtr((UChar*)aPage);
    sKeyCount  = sdpSlotDirectory::getCount(sSlotDirPtr);
    sNodeHdr    = (stndrNodeHdr*)sdpPhyPage::getLogicalHdrStartPtr((UChar*)aPage);

    sTmpPage = ((vULong)sTmpBuf) % SD_PAGE_SIZE == 0
        ? (UChar*)sTmpBuf
        : (UChar*)sTmpBuf + SD_PAGE_SIZE - (((vULong)sTmpBuf) % SD_PAGE_SIZE);

    idlOS::memcpy(sTmpPage, aPage, SD_PAGE_SIZE);
    if( aPage->mPageID != ((sdpPhyPageHdr*)sTmpPage)->mPageID )
    {
        dumpIndexNode( aPage );
        dumpIndexNode( (sdpPhyPageHdr *)sTmpPage );
        IDE_ASSERT( 0 );
    }

    (void)sdpPhyPage::reset( aPage,
                             ID_SIZEOF(stndrNodeHdr),
                             NULL );

    // sdpPhyPage::reset������ CTL�� �ʱ�ȭ �������� �ʴ´�.
    sdpPhyPage::initCTL( aPage,
                         (UInt)sdnIndexCTL::getCTLayerSize(sTmpPage),
                         &sDummy );

    sdpSlotDirectory::init( aPage );

    sdnIndexCTL::cleanAllRefInfo( aPage );

    sNodeHdr->mTotalDeadKeySize = 0;

    sSlotDirPtr = sdpPhyPage::getSlotDirStartPtr(sTmpPage);
    
    for( i = 0, sKeySeq = 0; i < sKeyCount; i++ )
    {
        IDE_TEST( sdpSlotDirectory::getPagePtrFromSlotNum( sSlotDirPtr,
                                                           i,
                                                           &sSrcKey)
                  != IDE_SUCCESS );
        
        sLeafKey = (stndrLKey*)sSrcKey;
            
        if( STNDR_GET_STATE( sLeafKey ) == STNDR_KEY_DEAD )
        {
            continue;
        }
        
        sKeyLength = getKeyLength( sSrcKey, ID_TRUE ); //aIsLeaf

        IDE_ASSERT( sdpPhyPage::allocSlot( (sdpPhyPageHdr*)aPage,
                                           sKeySeq,
                                           sKeyLength,
                                           ID_TRUE,
                                           &sAllowedSize,
                                           &sDstKey,
                                           &sSlotOffset,
                                           1 )
                    == IDE_SUCCESS );

        sKeySeq++;
       
        idlOS::memcpy( sDstKey, sSrcKey, sKeyLength );

        sCreateCTS = STNDR_GET_CCTS_NO( sLeafKey );
        sLimitCTS  = STNDR_GET_LCTS_NO( sLeafKey );

        if ( SDN_IS_VALID_CTS(sCreateCTS) )
        {
            sdnIndexCTL::addRefKey( aPage,
                                    sCreateCTS,
                                    sSlotOffset );
        }

        if ( SDN_IS_VALID_CTS(sLimitCTS) )
        {
            sdnIndexCTL::addRefKey( aPage,
                                    sLimitCTS,
                                    sSlotOffset );
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : 
 * ------------------------------------------------------------------*
 * Key�� Length�� ���Ѵ�.
 *********************************************************************/
UShort stndrRTree::getKeyLength( UChar * aKey, idBool aIsLeaf )
{
    UShort sTotalKeySize ;

    
    IDE_ASSERT( aKey != NULL );

    if( aIsLeaf == ID_TRUE )
    {
        sTotalKeySize = getKeyValueLength();
        
        sTotalKeySize =
            STNDR_LKEY_LEN( sTotalKeySize,
                            STNDR_GET_TB_TYPE( (stndrLKey*)aKey ) );
    }
    else
    {
        sTotalKeySize = getKeyValueLength();
        
        sTotalKeySize = STNDR_IKEY_LEN( sTotalKeySize );
    }

    IDE_ASSERT( sTotalKeySize < STNDR_MAX_KEY_BUFFER_SIZE );

    return sTotalKeySize;
}

/*********************************************************************
 * FUNCTION DESCRIPTION :
 * ------------------------------------------------------------------*
 * Transaction�� OldestSCN���� ���� CommitSCN�� ���� CTS�� ���ؼ�
 * Soft Key Stamping(Aging)�� �����Ѵ�.
 *********************************************************************/
IDE_RC stndrRTree::selfAging( stndrHeader   * aIndex,
                              sdrMtx        * aMtx,
                              sdpPhyPageHdr * aNode,
                              smSCN         * aOldestSCN,
                              UChar         * aAgedCount )
{
    sdnCTL  * sCTL;
    sdnCTS  * sCTS;
    UInt      i;
    smSCN     sCommitSCN;
    UChar     sAgedCount = 0;
    

    sCTL = sdnIndexCTL::getCTL( aNode );

    for( i = 0; i < sdnIndexCTL::getCount(sCTL); i++ )
    {
        sCTS = sdnIndexCTL::getCTS( sCTL, i );

        if( sCTS->mState == SDN_CTS_STAMPED )
        {
            sCommitSCN = sdnIndexCTL::getCommitSCN( sCTS );
            if( SM_SCN_IS_LT(&sCommitSCN, aOldestSCN) )
            {
                IDE_TEST( softKeyStamping(aIndex,
                                          aMtx,
                                          aNode,
                                          i)
                          != IDE_SUCCESS );
                
                sAgedCount++;
            }
        }
    }

    *aAgedCount = sAgedCount;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : 
 * ------------------------------------------------------------------*
 * TBT ������ Ű�� �����Ѵ�. 
 * Ʈ������� ������ CTS�� Binding �Ѵ�. 
 *********************************************************************/
IDE_RC stndrRTree::insertLeafKeyWithTBT( sdrMtx                 * aMtx,
                                         stndrHeader            * aIndex,
                                         UChar                    aCTSlotNum,
                                         smSCN                  * aInfiniteSCN,
                                         sdpPhyPageHdr          * aLeafNode,
                                         stndrKeyInfo           * aKeyInfo,
                                         UShort                   aKeyValueLen,
                                         SShort                   aKeySeq )
{
    UShort  sKeyOffset;
    UShort  sAllocatedKeyOffset;
    UShort  sKeyLength;

    
    sKeyLength = STNDR_LKEY_LEN( aKeyValueLen, STNDR_KEY_TB_CTS );
    sKeyOffset = sdpPhyPage::getAllocSlotOffset( aLeafNode, sKeyLength );
        
    if( aKeyInfo->mKeyState != STNDR_KEY_STABLE )
    {
        IDE_ASSERT( aCTSlotNum != SDN_CTS_INFINITE );
            
        IDE_TEST( sdnIndexCTL::bindCTS( aMtx,
                                        aIndex->mSdnHeader.mIndexTSID,
                                        aLeafNode,
                                        aCTSlotNum,
                                        sKeyOffset)
                  != IDE_SUCCESS );
    }

    IDE_TEST( insertLKey(aMtx,
                         aIndex,
                         (sdpPhyPageHdr*)aLeafNode,
                         aKeySeq,
                         aCTSlotNum,
                         aInfiniteSCN,
                         STNDR_KEY_TB_CTS,
                         aKeyInfo,
                         aKeyValueLen,
                         ID_TRUE, //aIsLogging
                         &sAllocatedKeyOffset)
              != IDE_SUCCESS );

    if( sKeyOffset != sAllocatedKeyOffset )
    {
        ideLog::log( IDE_SERVER_0,
                     "Key Offset : %u"
                     ", Allocated Key Offset : %u"
                     "\nKey sequence : %d"
                     ", CT slot number : %u"
                     ", Key Value size : %u"
                     "\nKey Info :\n",
                     sKeyOffset, sAllocatedKeyOffset,
                     aKeySeq, aCTSlotNum, aKeyValueLen );
        ideLog::logMem( IDE_SERVER_0,
                        (UChar *)aKeyInfo, ID_SIZEOF(stndrKeyInfo) );
        dumpIndexNode( (sdpPhyPageHdr *)aLeafNode );
        IDE_ASSERT( 0 );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : 
 * ------------------------------------------------------------------*
 * TBK ������ Ű�� �����Ѵ�. 
 * �ش� �Լ��� CTS�� �Ҵ��Ҽ� ���� ��쿡 ȣ��Ǹ�, Ʈ������� 
 * ������ KEY ��ü�� Binding �Ѵ�. 
 *********************************************************************/
IDE_RC stndrRTree::insertLeafKeyWithTBK( sdrMtx         * aMtx,
                                         stndrHeader    * aIndex,
                                         smSCN*           aInfiniteSCN,
                                         sdpPhyPageHdr  * aLeafNode,
                                         stndrKeyInfo   * aKeyInfo,
                                         UShort           aKeyValueLen,
                                         SShort           aKeySeq )
{
    UShort sKeyLength;
    UShort sKeyOffset;
    UShort sAllocatedKeyOffset = 0;

    
    sKeyLength = STNDR_LKEY_LEN( aKeyValueLen, STNDR_KEY_TB_KEY );
    sKeyOffset = sdpPhyPage::getAllocSlotOffset( aLeafNode, sKeyLength );
        
    IDE_TEST( insertLKey( aMtx,
                          aIndex,
                          (sdpPhyPageHdr*)aLeafNode,
                          aKeySeq,
                          SDN_CTS_IN_KEY,
                          aInfiniteSCN,
                          STNDR_KEY_TB_KEY,
                          aKeyInfo,
                          aKeyValueLen,
                          ID_TRUE, //aIsLogging
                          &sAllocatedKeyOffset )
              != IDE_SUCCESS );
    
    if( sKeyOffset != sAllocatedKeyOffset )
    {
        ideLog::log( IDE_SERVER_0,
                     "Key Offset : %u"
                     ", Allocated Key Offset : %u"
                     "\nKey sequence : %d"
                     ", CTS slot number : %u"
                     ", Key Value size : %u"
                     "\nKey Info :\n",
                     sKeyOffset, sAllocatedKeyOffset,
                     aKeySeq, SDN_CTS_IN_KEY, aKeyValueLen );
        ideLog::logMem( IDE_SERVER_0,
                        (UChar *)aKeyInfo, ID_SIZEOF(stndrKeyInfo) );
        dumpIndexNode( (sdpPhyPageHdr *)aLeafNode );
        IDE_ASSERT( 0 );
    }
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : 
 * ------------------------------------------------------------------*
 * Insert Position�� ȹ���� ����, Compaction���� ���Ͽ� Insertable   
 * Position�� ����ɼ� ������, �ش� �Լ��� �̸� �������ִ� ������
 * �Ѵ�. 
 *********************************************************************/
IDE_RC stndrRTree::adjustKeyPosition( sdpPhyPageHdr   * aNode,
                                    SShort          * aKeyPosition )
{
    UChar       * sSlotDirPtr;
    SInt          i;
    UChar       * sSlot;
    stndrLKey   * sLeafKey;
    SShort        sOrgKeyPosition;
    SShort        sAdjustedKeyPosition;
    
    
    sSlotDirPtr = sdpPhyPage::getSlotDirStartPtr( (UChar*)aNode );

    sOrgKeyPosition = *aKeyPosition;
    sAdjustedKeyPosition = *aKeyPosition;
    
    for( i = 0; i < sOrgKeyPosition; i++ )
    {
        IDE_TEST( sdpSlotDirectory::getPagePtrFromSlotNum( sSlotDirPtr, 
                                                           i,
                                                           &sSlot )
                  != IDE_SUCCESS );
        sLeafKey  = (stndrLKey*)sSlot;
            
        if( STNDR_GET_STATE(sLeafKey) == STNDR_KEY_DEAD )
        {
            sAdjustedKeyPosition--;
        }
    }

    *aKeyPosition = sAdjustedKeyPosition;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

/*********************************************************************
 * FUNCTION DESCRIPTION : 
 * ------------------------------------------------------------------*
 * Leaf Node�� aNode �� Ű�� insert�Ѵ�. 
 *********************************************************************/
IDE_RC stndrRTree::insertLKey( sdrMtx           * aMtx,
                               stndrHeader      * aIndex,
                               sdpPhyPageHdr    * aNode,
                               SShort             aKeySeq,
                               UChar              aCTSlotNum,
                               smSCN            * aInfiniteSCN,
                               UChar              aTxBoundType,
                               stndrKeyInfo     * aKeyInfo,
                               UShort             aKeyValueLen,
                               idBool             aIsLoggableSlot,
                               UShort           * aKeyOffset )
{
    UShort                    sAllowedSize;
    stndrLKey               * sLKey;
    UShort                    sKeyLength;
    UShort                    sKeyCount;
    stndrRollbackContext      sRollbackContext;
    stndrNodeHdr            * sNodeHdr;
    scOffset                  sKeyOffset;
    UShort                    sTotalTBKCount = 0;
    smSCN                     sFstDskViewSCN;
    smSCN                     sCreateSCN;
    smSCN                     sLimitSCN;
    sdSID                     sTSSlotSID;
    stdMBR                    sKeyInfoMBR;
    stdMBR                    sNodeMBR;
    idBool                    sIsChangedMBR = ID_FALSE;
    IDE_RC                    sRc;

    
    sFstDskViewSCN = smLayerCallback::getFstDskViewSCN( aMtx->mTrans );
    sTSSlotSID     = smLayerCallback::getTSSlotSID( aMtx->mTrans );
    sKeyLength     = STNDR_LKEY_LEN( aKeyValueLen, aTxBoundType );
    
    SM_SET_SCN( &sCreateSCN, aInfiniteSCN );
    SM_SET_SCN_CI_INFINITE( &sLimitSCN );
    
    sNodeHdr = (stndrNodeHdr*)
        sdpPhyPage::getLogicalHdrStartPtr( (UChar*)aNode );

    sKeyCount = sdpSlotDirectory::getCount(
        sdpPhyPage::getSlotDirStartPtr((UChar*)aNode) );

    IDE_TEST( sdpPhyPage::allocSlot( (sdpPhyPageHdr*)aNode,
                                     aKeySeq,
                                     sKeyLength,
                                     ID_TRUE,
                                     &sAllowedSize,
                                     (UChar**)&sLKey,
                                     &sKeyOffset,
                                     1 )
              != IDE_SUCCESS );

    // aKeyOffset�� NULL�� �ƴ� ���, Return �ش޶�� ��
    if( aKeyOffset != NULL )
    {
        *aKeyOffset = sKeyOffset;
    }
    
    IDE_ASSERT( aKeyInfo->mKeyState == STNDR_KEY_UNSTABLE ||
                aKeyInfo->mKeyState == STNDR_KEY_STABLE );
    
    IDE_ASSERT( sAllowedSize >= sKeyLength );
    
    idlOS::memset(sLKey, 0x00, sKeyLength );

    STNDR_GET_MBR_FROM_KEYINFO( sKeyInfoMBR, aKeyInfo );
    
    STNDR_KEYINFO_TO_LKEY( (*aKeyInfo), aKeyValueLen, sLKey,
                           aCTSlotNum,           //CCTS_NO
                           sCreateSCN,
                           SDN_CTS_INFINITE,     //LCTS_NO
                           sLimitSCN,
                           aKeyInfo->mKeyState,  //STATE
                           aTxBoundType );       //TB_TYPE

    IDE_ASSERT( (STNDR_GET_STATE(sLKey) == STNDR_KEY_UNSTABLE) ||
                (STNDR_GET_STATE(sLKey) == STNDR_KEY_STABLE) );
    
    if( aTxBoundType == STNDR_KEY_TB_KEY )
    {
        STNDR_SET_TBK_CSCN( ((stndrLKeyEx*)sLKey), &sFstDskViewSCN );
        STNDR_SET_TBK_CTSS( ((stndrLKeyEx*)sLKey), &sTSSlotSID );
    }
    
    sRollbackContext.mTableOID = aIndex->mSdnHeader.mTableOID;
    sRollbackContext.mIndexID  = aIndex->mSdnHeader.mIndexID;

    sNodeHdr->mUnlimitedKeyCount++;

    if( sKeyCount == 0 )
    {
        sIsChangedMBR = ID_TRUE;
        sNodeMBR = sKeyInfoMBR;
    }
    else
    {
        if( stdUtils::isMBRContains( &sNodeHdr->mMBR, &sKeyInfoMBR )
            != ID_TRUE )
        {
            sIsChangedMBR = ID_TRUE;
            sNodeMBR = sNodeHdr->mMBR;
            stdUtils::getMBRExtent( &sNodeMBR, &sKeyInfoMBR );
        }
    }

    if( (aIsLoggableSlot == ID_TRUE) &&
        (aIndex->mSdnHeader.mLogging == ID_TRUE) )
    {
        if( sIsChangedMBR == ID_TRUE )
        {
            sRc = setNodeMBR( aMtx, aNode, &sNodeMBR );
            if( sRc != IDE_SUCCESS )
            {
                ideLog::log( IDE_SERVER_0, "update MBR : \n" );
                ideLog::logMem( IDE_SERVER_0,
                                (UChar*)&sNodeMBR,
                                ID_SIZEOF(stdMBR) );
                dumpIndexNode( aNode );
                IDE_ASSERT( 0 );
            }
        }
        
        if( aTxBoundType == STNDR_KEY_TB_KEY )
        {
            sTotalTBKCount = sNodeHdr->mTBKCount + 1;
            IDE_TEST( sdrMiniTrans::writeNBytes(
                          aMtx,
                          (UChar*)&(sNodeHdr->mTBKCount),
                          (void*)&sTotalTBKCount,
                          ID_SIZEOF(UShort))
                      != IDE_SUCCESS );
        }
        
        IDE_TEST( sdrMiniTrans::writeNBytes(
                      aMtx,
                      (UChar*)&(sNodeHdr->mUnlimitedKeyCount),
                      (void*)&sNodeHdr->mUnlimitedKeyCount,
                      ID_SIZEOF(UShort))
                  != IDE_SUCCESS );

        sdrMiniTrans::setRefOffset( aMtx, SM_OID_NULL );
        
        IDE_TEST( sdrMiniTrans::writeLogRec(aMtx,
                                            (UChar*)sLKey,
                                            (void*)NULL,
                                            ID_SIZEOF(aKeySeq) + sKeyLength +
                                            ID_SIZEOF(sRollbackContext),
                                            SDR_STNDR_INSERT_KEY)
                  != IDE_SUCCESS );

        IDE_TEST( sdrMiniTrans::write(aMtx,
                                      (void*)&aKeySeq,
                                      ID_SIZEOF(SShort))
                  != IDE_SUCCESS );

        IDE_TEST( sdrMiniTrans::write(aMtx,
                                      (void*)&sRollbackContext,
                                      ID_SIZEOF(stndrRollbackContext))
                  != IDE_SUCCESS );

        IDE_TEST( sdrMiniTrans::write(aMtx,
                                      (void*)sLKey,
                                      sKeyLength)
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

UInt stndrRTree::getMinimumKeyValueLength( smnIndexHeader * aIndexHeader )
{
    UInt sTotalSize = 0;

    IDE_DASSERT( aIndexHeader != NULL );

    sTotalSize = ID_SIZEOF( stdMBR );

    
    return sTotalSize;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : 
 * ------------------------------------------------------------------*
 * TBK ������ Ű���� Commit SCN�� ���� �Լ� 
 * �ش� Ʈ������� Commit�Ǿ��ٸ� Delayed Stamping�� �õ��� ����. 
 *********************************************************************/
IDE_RC stndrRTree::getCommitSCN( idvSQL         * aStatistics,
                                 void           * aTrans,
                                 sdpPhyPageHdr  * aNode,
                                 stndrLKeyEx    * aLeafKeyEx,
                                 idBool           aIsLimit,
                                 smSCN            aStmtViewSCN,
                                 smSCN          * aCommitSCN )
{
    smSCN       sBeginSCN;
    smSCN       sCommitSCN;
    sdSID       sTSSlotSID;
    idBool      sTrySuccess = ID_FALSE;
    smTID       sTransID;

    if( aIsLimit == ID_FALSE )
    {
        STNDR_GET_TBK_CSCN( aLeafKeyEx, &sBeginSCN );
        STNDR_GET_TBK_CTSS( aLeafKeyEx, &sTSSlotSID );
    }
    else
    {
        STNDR_GET_TBK_LSCN( aLeafKeyEx, &sBeginSCN );
        STNDR_GET_TBK_LTSS( aLeafKeyEx, &sTSSlotSID );
    }
    
    if( SM_SCN_IS_VIEWSCN( sBeginSCN ) )
    {
        IDE_TEST( sdcTSSegment::getCommitSCN( aStatistics,
                                              aTrans,
                                              sTSSlotSID,
                                              &sBeginSCN,
                                              aStmtViewSCN,
                                              &sTransID,
                                              &sCommitSCN )
                  != IDE_SUCCESS );

        /*
         * �ش� Ʈ������� Commit�Ǿ��ٸ� Delayed Stamping�� �õ��� ����.
         */
        if( SM_SCN_IS_INFINITE( sCommitSCN ) == ID_FALSE )
        {
            sdbBufferMgr::tryEscalateLatchPage( aStatistics,
                                                (UChar*)aNode,
                                                SDB_SINGLE_PAGE_READ,
                                                &sTrySuccess );

            if( sTrySuccess == ID_TRUE )
            {
                sdbBufferMgr::setDirtyPageToBCB( aStatistics,
                                                 (UChar*)aNode );
                if( aIsLimit == ID_FALSE )
                {
                    STNDR_SET_TBK_CSCN( aLeafKeyEx, &sCommitSCN );
                }
                else
                {
                    STNDR_SET_TBK_LSCN( aLeafKeyEx, &sCommitSCN );
                }
            }
        }
    }
    else
    {
        sCommitSCN = sBeginSCN;
    }

    SM_SET_SCN(aCommitSCN, &sCommitSCN);
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION :
 * ------------------------------------------------------------------*
 * TBK Key�� �ڽ��� Ʈ������� ������ Ű���� �˻��Ѵ�.
 *********************************************************************/
idBool stndrRTree::isMyTransaction( void*   aTrans,
                                    smSCN   aBeginSCN,
                                    sdSID   aTSSlotSID )
{
    smSCN sSCN;
    
    if ( aTSSlotSID == smLayerCallback::getTSSlotSID( aTrans ) )
    {
        sSCN = smLayerCallback::getFstDskViewSCN( aTrans );

        if( SM_SCN_IS_EQ( &aBeginSCN, &sSCN ) )
        {
            return ID_TRUE;
        }
    }
    
    return ID_FALSE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION :
 * ------------------------------------------------------------------*
 * Rollback�� �ڽ��� Ʈ������� ������ Ű���� �˻��Ѵ�. aSCN����
 * FstDskViewSCN ���� �޴´�.
 *********************************************************************/
idBool stndrRTree::isMyTransaction( void   * aTrans,
                                    smSCN    aBeginSCN,
                                    sdSID    aTSSlotSID,
                                    smSCN  * aSCN )
{
    if ( aTSSlotSID == smLayerCallback::getTSSlotSID( aTrans ) )
    {
        if( SM_SCN_IS_EQ( &aBeginSCN, aSCN ) )
        {
            return ID_TRUE;
        }
    }
    
    return ID_FALSE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : 
 * ------------------------------------------------------------------*
 * FREE KEY���� logical log�� ����Ѵ�.
 *********************************************************************/
IDE_RC stndrRTree::writeLogFreeKeys( sdrMtx         * aMtx,
                                     UChar          * aNode,
                                     stndrKeyArray  * aKeyArray,
                                     UShort           aFromSeq,
                                     UShort           aToSeq )
{
    SShort  sCount;
    
    if(aFromSeq <= aToSeq)
    {
        sCount = aToSeq - aFromSeq + 1;
        IDE_TEST( sdrMiniTrans::writeLogRec(
                      aMtx,
                      (UChar*)aNode,
                      NULL,
                      (ID_SIZEOF(sCount) + (ID_SIZEOF(stndrKeyArray) * sCount)),
                      SDR_STNDR_FREE_KEYS )
                  != IDE_SUCCESS);

        IDE_TEST( sdrMiniTrans::write( aMtx,
                                       (void*)&sCount,
                                       ID_SIZEOF(sCount) )
                  != IDE_SUCCESS );

        IDE_TEST( sdrMiniTrans::write( aMtx,
                                       (void*)&aKeyArray[aFromSeq],
                                       ID_SIZEOF(stndrKeyArray) * sCount )
                  != IDE_SUCCESS );

    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : 
 * ------------------------------------------------------------------*
 * Leaf Node���� �־��� Ű�� �����Ѵ�.
 * CTS�� �Ҵ� �����ϴٸ� TBT(Transactin info Bound in CTS)�� Ű��
 * ������ �ϰ�, �ݴ��� TBK(Transaction info Bound in Key)��
 * ������ �Ѵ�.  
 *********************************************************************/
IDE_RC stndrRTree::deleteKeyFromLeafNode( idvSQL            * aStatistics,
                                          stndrStatistic    * aIndexStat,
                                          sdrMtx            * aMtx,
                                          stndrHeader       * aIndex,
                                          smSCN             * aInfiniteSCN,
                                          sdpPhyPageHdr     * aLeafNode,
                                          SShort            * aLeafKeySeq ,
                                          idBool            * aIsSuccess )
{
    stndrCallbackContext      sCallbackContext;
    UChar                     sCTSlotNum;
    UShort                    sKeyOffset;
    UChar                   * sSlotDirPtr;
    stndrLKey               * sLeafKey;

    
    *aIsSuccess = ID_TRUE;
    
    sCallbackContext.mIndex = (stndrHeader*)aIndex;
    sCallbackContext.mStatistics = aIndexStat;
    
    sSlotDirPtr = sdpPhyPage::getSlotDirStartPtr( (UChar*)aLeafNode );
    IDE_TEST( sdpSlotDirectory::getPagePtrFromSlotNum( sSlotDirPtr,
                                                       *aLeafKeySeq,
                                                       (UChar**)&sLeafKey )
              != IDE_SUCCESS );
    IDE_TEST ( sdpSlotDirectory::getValue( sSlotDirPtr, 
                                           *aLeafKeySeq,
                                           &sKeyOffset )
               != IDE_SUCCESS );

    if( (STNDR_GET_STATE( sLeafKey ) != STNDR_KEY_UNSTABLE) &&
        (STNDR_GET_STATE( sLeafKey ) != STNDR_KEY_STABLE) )
    {
        ideLog::log( IDE_SERVER_0,
                     "Leaf key sequence number : %d\n",
                     *aLeafKeySeq );
        dumpIndexNode( aLeafNode );
        IDE_ASSERT( 0 );
    }
    
    IDE_TEST( sdnIndexCTL::allocCTS( aStatistics,
                                     aMtx,
                                     &aIndex->mSdnHeader.mSegmentDesc.mSegHandle,
                                     aLeafNode,
                                     &sCTSlotNum,
                                     &gCallbackFuncs4CTL,
                                     (UChar*)&sCallbackContext )
              != IDE_SUCCESS );

    /*
     * CTS �Ҵ��� ������ ���� TBK�� Ű�� �����Ѵ�.
     */
    if( sCTSlotNum == SDN_CTS_INFINITE )
    {
        IDE_TEST( deleteLeafKeyWithTBK( aMtx,
                                        aIndex,
                                        aInfiniteSCN,
                                        aLeafNode,
                                        aLeafKeySeq ,
                                        aIsSuccess )
                  != IDE_SUCCESS );

        IDE_TEST_RAISE( *aIsSuccess == ID_FALSE, ALLOC_FAILURE );
    }
    else
    {
        IDE_TEST( deleteLeafKeyWithTBT( aMtx,
                                        aIndex,
                                        aInfiniteSCN,
                                        sCTSlotNum,
                                        aLeafNode,
                                        *aLeafKeySeq  )
                  != IDE_SUCCESS );
    }
    
    IDE_EXCEPTION_CONT( ALLOC_FAILURE );
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : 
 * ------------------------------------------------------------------*
 * TBT ������ Ű�� �����Ѵ�. 
 * CTS�� �����ϴ� Ʈ������� ������ Binding�Ѵ�. 
 *********************************************************************/
IDE_RC stndrRTree::deleteLeafKeyWithTBT( sdrMtx         * aMtx,
                                         stndrHeader    * aIndex,
                                         smSCN          * aInfiniteSCN,
                                         UChar            aCTSlotNum,
                                         sdpPhyPageHdr  * aLeafNode,
                                         SShort           aLeafKeySeq )
{
    stndrRollbackContext      sRollbackContext;
    UChar                   * sSlotDirPtr;
    stndrLKey               * sLeafKey;
    UShort                    sKeyOffset;
    UShort                    sKeyLength;
    stndrNodeHdr            * sNodeHdr;
    SShort                    sDummyKeySeq;
    idBool                    sRemoveInsert;
    UShort                    sUnlimitedKeyCount;
    smSCN                     sFstDiskViewSCN;

    
    sFstDiskViewSCN = smLayerCallback::getFstDskViewSCN( aMtx->mTrans );
    
    sSlotDirPtr = sdpPhyPage::getSlotDirStartPtr( (UChar*)aLeafNode );
    IDE_TEST( sdpSlotDirectory::getPagePtrFromSlotNum( sSlotDirPtr,
                                                       aLeafKeySeq,
                                                       (UChar**)&sLeafKey )
              != IDE_SUCCESS );
    IDE_TEST( sdpSlotDirectory::getValue( sSlotDirPtr, 
                                          aLeafKeySeq,
                                          &sKeyOffset )
              != IDE_SUCCESS );
    sKeyLength = getKeyLength( (UChar*)sLeafKey ,
                               ID_TRUE /* aIsLeaf */ );

    IDE_TEST( sdnIndexCTL::bindCTS( aMtx,
                                    aIndex->mSdnHeader.mIndexTSID,
                                    aLeafNode,
                                    aCTSlotNum,
                                    sKeyOffset )
              != IDE_SUCCESS );
    
    sRollbackContext.mTableOID = aIndex->mSdnHeader.mTableOID;
    sRollbackContext.mIndexID  = aIndex->mSdnHeader.mIndexID;
    sRollbackContext.mFstDiskViewSCN = sFstDiskViewSCN;

    SM_SET_SCN( &sRollbackContext.mLimitSCN, aInfiniteSCN );

    STNDR_SET_LCTS_NO( sLeafKey , aCTSlotNum );
    STNDR_SET_LSCN( sLeafKey, aInfiniteSCN );
    STNDR_SET_STATE( sLeafKey , STNDR_KEY_DELETED );

    sNodeHdr = (stndrNodeHdr*)
        sdpPhyPage::getLogicalHdrStartPtr((UChar*)aLeafNode);
    sUnlimitedKeyCount = sNodeHdr->mUnlimitedKeyCount - 1;
    IDE_DASSERT( sUnlimitedKeyCount < sdpSlotDirectory::getCount(sSlotDirPtr) );
                 
    IDE_TEST( sdrMiniTrans::writeNBytes( aMtx,
                                         (UChar*)&(sNodeHdr->mUnlimitedKeyCount),
                                         (void*)&sUnlimitedKeyCount,
                                         ID_SIZEOF(UShort) )
              != IDE_SUCCESS );

    sdrMiniTrans::setRefOffset( aMtx, SM_OID_NULL );
    IDE_TEST( sdrMiniTrans::writeLogRec( aMtx,
                                         (UChar*)sLeafKey ,
                                         (void*)NULL,
                                         sKeyLength +
                                         ID_SIZEOF(stndrRollbackContext) +
                                         ID_SIZEOF(SShort) +
                                         ID_SIZEOF(UChar),
                                         SDR_STNDR_DELETE_KEY_WITH_NTA )
              != IDE_SUCCESS );

    IDE_TEST( sdrMiniTrans::write( aMtx,
                                   (void*)&sRollbackContext,
                                   ID_SIZEOF(stndrRollbackContext) )
              != IDE_SUCCESS );

    sRemoveInsert = ID_FALSE;
    IDE_TEST( sdrMiniTrans::write( aMtx,
                                   (void*)&sRemoveInsert,
                                   ID_SIZEOF(UChar) )
              != IDE_SUCCESS );

    sDummyKeySeq = 0; /* �ǹ̾��� �� */
    IDE_TEST( sdrMiniTrans::write( aMtx,
                                   (void*)&sDummyKeySeq,
                                   ID_SIZEOF(SShort) )
              != IDE_SUCCESS );

    IDE_TEST( sdrMiniTrans::write( aMtx,
                                   (void*)sLeafKey ,
                                   sKeyLength )
              != IDE_SUCCESS );
        
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : 
 * ------------------------------------------------------------------*
 * TBK ������ Ű�� �����Ѵ�. 
 * Key ��ü�� �����ϴ� Ʈ������� ������ Binding �Ѵ�. 
 * ���� Ű�� TBK��� ���ο� ������ �Ҵ��� �ʿ䰡 ������ �ݴ��� 
 * ���� Ű�� ���� ������ �Ҵ��ؾ� �Ѵ�. 
 *********************************************************************/
IDE_RC stndrRTree::deleteLeafKeyWithTBK( sdrMtx         * aMtx,
                                         stndrHeader    * aIndex,
                                         smSCN          * aInfiniteSCN,
                                         sdpPhyPageHdr  * aLeafNode,
                                         SShort         * aLeafKeySeq ,
                                         idBool         * aIsSuccess )
{
    smSCN                     sSysMinDskViewSCN;
    UChar                     sAgedCount = 0;
    UShort                    sKeyLength;
    UShort                    sAllowedSize;
    UChar                   * sSlotDirPtr;
    stndrLKey               * sLeafKey;
    UChar                     sTxBoundType;
    stndrLKey               * sRemoveKey;
    stndrRollbackContext      sRollbackContext;
    UShort                    sUnlimitedKeyCount;
    stndrNodeHdr            * sNodeHdr;
    UShort                    sTotalTBKCount = 0;
    UChar                     sRemoveInsert = ID_FALSE;
    smSCN                     sFstDiskViewSCN;
    sdSID                     sTSSlotSID;
    scOffset                  sOldKeyOffset;
    scOffset                  sNewKeyOffset;

    *aIsSuccess = ID_TRUE;
    
    sFstDiskViewSCN = smLayerCallback::getFstDskViewSCN( aMtx->mTrans );
    sTSSlotSID      = smLayerCallback::getTSSlotSID( aMtx->mTrans );
    
    sSlotDirPtr = sdpPhyPage::getSlotDirStartPtr( (UChar*)aLeafNode );
    IDE_TEST( sdpSlotDirectory::getPagePtrFromSlotNum( sSlotDirPtr,
                                                       *aLeafKeySeq,
                                                       (UChar**)&sRemoveKey )
              != IDE_SUCCESS );

    sTxBoundType = STNDR_GET_TB_TYPE( sRemoveKey );

    sKeyLength = STNDR_LKEY_LEN( getKeyValueLength(),
                                 STNDR_KEY_TB_KEY );
    
    if( sTxBoundType == STNDR_KEY_TB_KEY )
    {
        sRemoveInsert = ID_FALSE;
        sLeafKey  = sRemoveKey;
        IDE_RAISE( SKIP_ALLOC_SLOT );
    }
    
    /*
     * canAllocLeafKey ������ Compaction���� ���Ͽ�
     * KeySeq�� ����ɼ� �ִ�.
     */
    if( canAllocLeafKey ( aMtx,
                          aIndex,
                          aLeafNode,
                          (UInt)sKeyLength,
                          aLeafKeySeq  ) != IDE_SUCCESS )
    {
        SMX_GET_MIN_DISK_VIEW( &sSysMinDskViewSCN );

        /*
         * ���������� ���� �Ҵ��� ���ؼ� Self Aging�� �Ѵ�.
         */
        IDE_TEST( selfAging( aIndex,
                             aMtx,
                             aLeafNode,
                             &sSysMinDskViewSCN,
                             &sAgedCount )
                  != IDE_SUCCESS );

        if( sAgedCount > 0 )
        {
            if( canAllocLeafKey ( aMtx,
                                  aIndex,
                                  aLeafNode,
                                  (UInt)sKeyLength,
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
    
    IDE_TEST_RAISE( *aIsSuccess == ID_FALSE, ALLOC_FAILURE );

    sRemoveInsert = ID_TRUE;
    IDE_TEST( sdpPhyPage::allocSlot( (sdpPhyPageHdr*)aLeafNode,
                                     *aLeafKeySeq,
                                     sKeyLength,
                                     ID_TRUE,
                                     &sAllowedSize,
                                     (UChar**)&sLeafKey,
                                     &sNewKeyOffset,
                                     1 )
              != IDE_SUCCESS );
    
    idlOS::memset( sLeafKey , 0x00, sKeyLength );

    IDE_TEST( sdpSlotDirectory::getPagePtrFromSlotNum( sSlotDirPtr,
                                                       *aLeafKeySeq  + 1,
                                                       (UChar**)&sRemoveKey )
              != IDE_SUCCESS );

    idlOS::memcpy( sLeafKey , sRemoveKey, ID_SIZEOF( stndrLKey ) );
    STNDR_SET_TB_TYPE( sLeafKey , STNDR_KEY_TB_KEY );
    idlOS::memcpy( STNDR_LKEY_KEYVALUE_PTR( sLeafKey  ),
                   STNDR_LKEY_KEYVALUE_PTR( sRemoveKey ),
                   getKeyValueLength() );

    // BUG-29506 TBT�� TBK�� ��ȯ�� offset�� CTS�� �ݿ����� �ʽ��ϴ�.
    // ���� offset���� TBK�� �����Ǵ� key�� offset���� ����
    IDE_TEST(sdpSlotDirectory::getValue( sSlotDirPtr,
                                         *aLeafKeySeq + 1,   
                                         &sOldKeyOffset )
             != IDE_SUCCESS );

    if( SDN_IS_VALID_CTS(STNDR_GET_CCTS_NO(sLeafKey)) )
    {
        IDE_TEST( sdnIndexCTL::updateRefKey( aMtx,
                                             aLeafNode,
                                             STNDR_GET_CCTS_NO( sLeafKey ),
                                             sOldKeyOffset,
                                             sNewKeyOffset )
                  != IDE_SUCCESS );
    }

    IDE_EXCEPTION_CONT( SKIP_ALLOC_SLOT );

    STNDR_SET_TBK_LSCN( ((stndrLKeyEx*)sLeafKey ), &sFstDiskViewSCN );
    STNDR_SET_TBK_LTSS( ((stndrLKeyEx*)sLeafKey ), &sTSSlotSID );

    STNDR_SET_LCTS_NO( sLeafKey , SDN_CTS_IN_KEY );
    STNDR_SET_LSCN( sLeafKey, aInfiniteSCN );;
    
    STNDR_SET_STATE( sLeafKey , STNDR_KEY_DELETED );

    sRollbackContext.mTableOID = aIndex->mSdnHeader.mTableOID;
    sRollbackContext.mIndexID  = aIndex->mSdnHeader.mIndexID;
    sRollbackContext.mFstDiskViewSCN = sFstDiskViewSCN;
    SM_SET_SCN( &sRollbackContext.mLimitSCN, aInfiniteSCN );
   
    sNodeHdr = (stndrNodeHdr*)sdpPhyPage::getLogicalHdrStartPtr( (UChar*)aLeafNode );
    sUnlimitedKeyCount = sNodeHdr->mUnlimitedKeyCount - 1;
   
    IDE_TEST( sdrMiniTrans::writeNBytes( aMtx,
                                         (UChar*)&(sNodeHdr->mUnlimitedKeyCount),
                                         (void*)&sUnlimitedKeyCount,
                                         ID_SIZEOF(UShort) )
              != IDE_SUCCESS );

    if( sRemoveInsert == ID_TRUE )
    {
        sTotalTBKCount = sNodeHdr->mTBKCount + 1;
        IDE_TEST( sdrMiniTrans::writeNBytes( aMtx,
                                             (UChar*)&(sNodeHdr->mTBKCount),
                                             (void*)&sTotalTBKCount,
                                             ID_SIZEOF(UShort) )
                  != IDE_SUCCESS );
    }
   
    sdrMiniTrans::setRefOffset( aMtx, SM_OID_NULL );
    IDE_TEST( sdrMiniTrans::writeLogRec( aMtx,
                                         (UChar*)sLeafKey ,
                                         (void*)NULL,
                                         sKeyLength +
                                         ID_SIZEOF(stndrRollbackContext) +
                                         ID_SIZEOF(SShort) +
                                         ID_SIZEOF(UChar),
                                         SDR_STNDR_DELETE_KEY_WITH_NTA )
              != IDE_SUCCESS );

    IDE_TEST( sdrMiniTrans::write( aMtx,
                                   (void*)&sRollbackContext,
                                   ID_SIZEOF(stndrRollbackContext) )
              != IDE_SUCCESS );

    IDE_TEST( sdrMiniTrans::write( aMtx,
                                   (void*)&sRemoveInsert,
                                   ID_SIZEOF(UChar) )
              != IDE_SUCCESS );
    
    IDE_TEST( sdrMiniTrans::write( aMtx,
                                   (void*)aLeafKeySeq ,
                                   ID_SIZEOF(SShort) )
              != IDE_SUCCESS );

    IDE_TEST( sdrMiniTrans::write( aMtx,
                                   (void*)sLeafKey ,
                                   sKeyLength )
              != IDE_SUCCESS );

    /*
     * ���ο� KEY�� �Ҵ�Ǿ��ٸ� ���� KEY�� �����Ѵ�.
     */
    if( sRemoveInsert == ID_TRUE )
    {
        IDE_DASSERT( sTxBoundType != STNDR_KEY_TB_KEY );

        sKeyLength = getKeyLength( (UChar *)sRemoveKey,
                                   ID_TRUE /* aIsLeaf */ );
            
        IDE_TEST(sdrMiniTrans::writeLogRec(aMtx,
                                           (UChar*)sRemoveKey,
                                           (void*)&sKeyLength,
                                           ID_SIZEOF(UShort),
                                           SDR_STNDR_FREE_INDEX_KEY )
                 != IDE_SUCCESS );
            
        sdpPhyPage::freeSlot(aLeafNode,
                             *aLeafKeySeq  + 1,
                             sKeyLength,
                             1 );
    }
    
    IDE_EXCEPTION_CONT( ALLOC_FAILURE );
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : 
 * ------------------------------------------------------------------*
 * �־��� callback�� �ǰ��Ͽ� keyrange�� �ش��ϴ� leaf slot�� �ٷ�
 * ������ Ŀ���� �̵���Ų��. 
 * �ַ� read lock���� traversing�Ҷ� ȣ��ȴ�.
 *********************************************************************/
IDE_RC stndrRTree::beforeFirst( stndrIterator       *  aIterator,
                                const smSeekFunc   **  /**/)
{
    for( aIterator->mKeyRange       = aIterator->mKeyRange;
         aIterator->mKeyRange->prev != NULL;
         aIterator->mKeyRange       = aIterator->mKeyRange->prev ) ;

    IDE_TEST( beforeFirstInternal( aIterator ) != IDE_SUCCESS );

    aIterator->mFlag  = aIterator->mFlag & (~SMI_RETRAVERSE_MASK);
    aIterator->mFlag |= SMI_RETRAVERSE_BEFORE;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : 
 * ------------------------------------------------------------------*
 * �־��� callback�� �ǰ��Ͽ� keyrange�� �ش��ϴ� ��� leaf slot��
 * �ٷ� ������ Ŀ���� �̵���Ų��. 
 * key Range�� ����Ʈ�� ������ �� �ִµ�, �ش��ϴ� Key�� ��������
 * �ʴ� key range�� skip�Ѵ�. 
 *********************************************************************/
IDE_RC stndrRTree::beforeFirstInternal( stndrIterator * aIterator )
{
    if(aIterator->mProperties->mReadRecordCount > 0)
    {
        IDE_TEST( stndrRTree::findFirst(aIterator) != IDE_SUCCESS );

        while( (aIterator->mCacheFence        == aIterator->mRowCache) &&
               (aIterator->mIsLastNodeInRange == ID_TRUE) &&
               (aIterator->mKeyRange->next    != NULL) )
        {
            aIterator->mKeyRange = aIterator->mKeyRange->next;
            
            IDE_TEST( stndrRTree::findFirst(aIterator) != IDE_SUCCESS );
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : 
 * ------------------------------------------------------------------*
 * �־��� maximum callback�� �ǰ��Ͽ� �ش� callback�� �����ϴ� Key��
 * ã�� row cache�� �����Ѵ�.
 *********************************************************************/
IDE_RC stndrRTree::findFirst( stndrIterator * aIterator )
{
    idvSQL              * sStatistics = NULL;
    sdpPhyPageHdr       * sLeafNode = NULL;
    const smiRange      * sRange = NULL;
    stndrStack          * sStack = NULL;
    stndrHeader         * sIndex = NULL;
    idBool                sIsRetry;
    UInt                  sState = 0;
    stndrVirtualRootNode  sVRootNode;


    sIndex      = (stndrHeader*)((smnIndexHeader*)(aIterator->mIndex))->mHeader;
    sStatistics = aIterator->mProperties->mStatistics;
    sStack      = &aIterator->mStack;
    sRange      = aIterator->mKeyRange;

  retry:
    
    stndrStackMgr::clear(sStack);

    getVirtualRootNode( sIndex, &sVRootNode );
    
    if( sVRootNode.mChildPID != SD_NULL_PID )
    {
        IDE_TEST( stndrStackMgr::push( sStack,
                                       sVRootNode.mChildPID,
                                       sVRootNode.mChildSmoNo,
                                       STNDR_INVALID_KEY_SEQ )
                  != IDE_SUCCESS );

        IDE_TEST( findNextLeaf( sStatistics,
                                &(sIndex->mQueryStat),
                                sIndex,
                                &(sRange->maximum),
                                sStack,
                                &sLeafNode,
                                &sIsRetry )
                  != IDE_SUCCESS );

        if( sIsRetry == ID_TRUE )
        {
            goto retry;
        }

        if( sLeafNode != NULL )
        {
            sState = 1;
        }

        IDE_TEST( makeRowCache( aIterator, (UChar*)sLeafNode )
                  != IDE_SUCCESS );

        if( sLeafNode != NULL )
        {
            sState = 0;
            
            IDE_TEST( sdbBufferMgr::releasePage( sStatistics,
                                                 (UChar*)sLeafNode )
                      != IDE_SUCCESS );
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if( sState == 1 && sLeafNode != NULL )
    {
        (void)sdbBufferMgr::releasePage( sStatistics, (UChar*)sLeafNode );
    }

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : 
 * ------------------------------------------------------------------*
 * �־��� maximum callback�� �����ϴ� Leaf Node�� Ž���Ѵ�. aCallBack��
 * NULL�� ��� ��� Leaf Node�� Ž���Ѵ�.
 *********************************************************************/
IDE_RC stndrRTree::findNextLeaf( idvSQL             * aStatistics,
                                 stndrStatistic     * aIndexStat,
                                 stndrHeader        * aIndex,
                                 const smiCallBack  * aCallBack,
                                 stndrStack         * aStack,
                                 sdpPhyPageHdr     ** aLeafNode,
                                 idBool             * aIsRetry )
{
    sdpPhyPageHdr   * sNode;
    stndrNodeHdr    * sNodeHdr;
    stndrStackSlot    sSlot;
    stndrIKey       * sIKey;
    stdMBR            sMBR;
    scPageID          sChildPID;
    UChar           * sSlotDirPtr;
    UShort            sKeyCount;
    ULong             sNodeSmoNo;
    ULong             sIndexSmoNo;
    idBool            sResult;
    idBool            sIsSuccess;
    UInt              sState = 0;
    UInt              i;

    
    *aIsRetry = ID_FALSE;
    *aLeafNode = NULL;

    while( stndrStackMgr::getDepth(aStack) >= 0 )
    {
        sSlot = stndrStackMgr::pop( aStack );

        IDU_FIT_POINT( "1.PROJ-1591@stndrRTree::findNextLeaf" );

        IDE_TEST( stndrRTree::getPage( aStatistics,
                                       &(aIndex->mQueryStat.mIndexPage),
                                       aIndex->mSdnHeader.mIndexTSID,
                                       sSlot.mNodePID,
                                       SDB_S_LATCH,
                                       SDB_WAIT_NORMAL,
                                       NULL,
                                       (UChar**)&sNode,
                                       &sIsSuccess )
                  != IDE_SUCCESS );
        sState = 1;

        sNodeHdr = (stndrNodeHdr*)sdpPhyPage::getLogicalHdrStartPtr( (UChar*)sNode );
        sNodeSmoNo = sdpPhyPage::getIndexSMONo( sNode );

        if( sNodeHdr->mState == STNDR_IN_FREE_LIST )
        {
            // BUG-29629: Disk R-Tree�� Scan�� FREE LIST �������� ���� S-Latch��
            //            �ʾƼ� Hang�� �߻��մϴ�.
            sState = 0;
            IDE_TEST( sdbBufferMgr::releasePage( aStatistics,
                                                 (UChar*)sNode )
                      != IDE_SUCCESS );
            continue;
        }

        if( sNodeSmoNo > sSlot.mSmoNo )
        {
            if( sdpPhyPage::getNxtPIDOfDblList(sNode) == SD_NULL_PID )
            {
                ideLog::log( IDE_SERVER_0,
                             "Index Header:\n" );
                ideLog::logMem( IDE_SERVER_0,
                                (UChar *)aIndex,
                                ID_SIZEOF(stndrHeader) );

                ideLog::log( IDE_SERVER_0,
                             "Stack Slot:\n" );
                ideLog::logMem( IDE_SERVER_0,
                                (UChar *)&sSlot,
                                ID_SIZEOF(stndrStackSlot) );

                dumpIndexNode( sNode );
                IDE_ASSERT( 0 );
            }
            
            IDE_TEST( stndrStackMgr::push(
                          aStack,
                          sdpPhyPage::getNxtPIDOfDblList(sNode),
                          sSlot.mSmoNo,
                          0 )
                      != IDE_SUCCESS );

            aIndexStat->mFollowRightLinkCount++;
        }

        if( STNDR_IS_LEAF_NODE( sNodeHdr ) == ID_TRUE )
        {
            *aLeafNode = sNode;
            break;
        }

        sSlotDirPtr = sdpPhyPage::getSlotDirStartPtr( (UChar*)sNode );
        sKeyCount = sdpSlotDirectory::getCount( sSlotDirPtr );

        getSmoNo( aIndex, &sIndexSmoNo );
        IDL_MEM_BARRIER;

        IDE_TEST( iduCheckSessionEvent(aStatistics) != IDE_SUCCESS );
        for( i = 0; i < sKeyCount; i++ )
        {
            sResult = ID_TRUE;
            
            IDE_TEST ( sdpSlotDirectory::getPagePtrFromSlotNum( sSlotDirPtr, 
                                                                i,
                                                                (UChar**)&sIKey )
                       != IDE_SUCCESS );

            STNDR_GET_MBR_FROM_IKEY( sMBR, sIKey );
            STNDR_GET_CHILD_PID( &sChildPID, sIKey );

            if( aCallBack != NULL )
            {
                (void)aCallBack->callback( &sResult,
                                           &sMBR,
                                           NULL,
                                           0,
                                           SC_NULL_GRID,
                                           aCallBack->data );
                
                aIndexStat->mKeyRangeCount++;
            }

            if( sResult == ID_TRUE )
            {
                IDE_TEST( stndrStackMgr::push( aStack,
                                               sChildPID,
                                               sIndexSmoNo,
                                               0 )
                          != IDE_SUCCESS );
            }
        }

        sState = 0;
        IDE_TEST( sdbBufferMgr::releasePage( aStatistics, (UChar*)sNode )
                  != IDE_SUCCESS );

    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if( sState == 1 )
    {
        if( sdbBufferMgr::releasePage( aStatistics, (UChar*)sNode )
            != IDE_SUCCESS )
        {
            dumpIndexNode( sNode );
            IDE_ASSERT( 0 );
        }
    }

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : 
 * ------------------------------------------------------------------*
 * aNode���� aCallBack�� �����ϴ� ������ Key ���� mRowRID�� Key Value��
 * Row Cache�� �����Ѵ�. Row Cache�� ����� maximum KeyRange�� �����
 * Key �� �߿��� Transaction Level Visibility�� Cursor Level Visibility
 * �� ����ϴ� Key ���̴�.               
 *********************************************************************/
IDE_RC stndrRTree::makeRowCache( stndrIterator * aIterator, UChar * aNode )
{
    stndrStack      * sStack;
    stndrLKey       * sLKey;
    stndrKeyInfo      sKeyInfo;
    stdMBR            sMBR;
    const smiRange  * sRange;
    UChar           * sSlotDirPtr;
    UShort            sKeyCount;
    idBool            sIsVisible;
    idBool            sIsUnknown;
    idBool            sResult;
    UInt              i;

    sStack = &aIterator->mStack;

    // aIterator�� Row Cache�� �ʱ�ȭ �Ѵ�.
    aIterator->mCurRowPtr = aIterator->mRowCache - 1;
    aIterator->mCacheFence = &aIterator->mRowCache[0];

    if( stndrStackMgr::getDepth( sStack ) == -1 )
    {
        aIterator->mIsLastNodeInRange = ID_TRUE;
    }
    else
    {
        aIterator->mIsLastNodeInRange = ID_FALSE;
    }

    sRange = aIterator->mKeyRange;

    // �о���� Ű���� ĳ���Ѵ�.

    // TEST CASE: makeRowCacheForward���� Leaf Node�� ���� ��츦 �׽�Ʈ �ϱ�
    if( aNode == NULL )
    {
        IDE_RAISE( RETURN_SUCCESS );
    }
    
    sSlotDirPtr = sdpPhyPage::getSlotDirStartPtr( aNode );
    sKeyCount  = sdpSlotDirectory::getCount( sSlotDirPtr );

    for( i = 0; i < sKeyCount; i++ )
    {
        sIsVisible = ID_FALSE;
        sIsUnknown = ID_FALSE;

        IDE_TEST( sdpSlotDirectory::getPagePtrFromSlotNum( sSlotDirPtr, 
                                                           i,
                                                           (UChar**)&sLKey )
                  != IDE_SUCCESS );

        STNDR_GET_MBR_FROM_LKEY( sMBR, sLKey );
        
        sRange->maximum.callback( &sResult,
                                  &sMBR,
                                  NULL,
                                  0,
                                  SC_NULL_GRID,
                                  sRange->maximum.data ); 
        if( sResult == ID_FALSE )
        {
            continue;
        }

        IDE_TEST( tranLevelVisibility( aIterator->mProperties->mStatistics,
                                       aIterator->mTrans,
                                       aNode,
                                       (UChar*)sLKey,
                                       &aIterator->mSCN,
                                       &sIsVisible,
                                       &sIsUnknown ) != IDE_SUCCESS );

        if( (sIsVisible == ID_FALSE) && (sIsUnknown == ID_FALSE) )
        {
            continue;
        }

        if( sIsUnknown == ID_TRUE )
        {
            IDE_TEST( cursorLevelVisibility( sLKey,
                                             &aIterator->mInfinite,
                                             &sIsVisible ) != IDE_SUCCESS );

            if( sIsVisible == ID_FALSE )
            {
                continue;
            }
        }

        // save slot info
        STNDR_LKEY_TO_KEYINFO( sLKey, sKeyInfo );
        
        aIterator->mCacheFence->mRowPID     = sKeyInfo.mRowPID;
        aIterator->mCacheFence->mRowSlotNum = sKeyInfo.mRowSlotNum;
        aIterator->mCacheFence->mOffset     =
            sdpPhyPage::getOffsetFromPagePtr( (UChar*)sLKey );

        idlOS::memcpy( &(aIterator->mPage[aIterator->mCacheFence->mOffset]),
                       (UChar*)sLKey,
                       getKeyLength( (UChar*)sLKey, ID_TRUE ) );

        // increase cache fence
        aIterator->mCacheFence++;
    }

    IDE_EXCEPTION_CONT( RETURN_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : 
 * ------------------------------------------------------------------*
 * ĳ�õǾ� �ִ� Row�� �ִٸ� ĳ�ÿ��� �ְ�, ���� ���ٸ� Next Node��
 * ĳ���Ѵ�. 
 *********************************************************************/
IDE_RC stndrRTree::fetchNext( stndrIterator * aIterator,
                              const void   ** aRow )
{
    stndrHeader * sIndex;
    idvSQL      * sStatistics;
    idBool        sNeedMoreCache;

    
    sIndex      = (stndrHeader*)((smnIndexHeader*)(aIterator->mIndex))->mHeader;
    sStatistics = aIterator->mProperties->mStatistics;

  read_from_cache:
    IDE_TEST( fetchRowCache( aIterator,
                             aRow,
                             &sNeedMoreCache ) != IDE_SUCCESS );

    IDE_TEST_RAISE( sNeedMoreCache == ID_FALSE, SKIP_CACHE );

    if( aIterator->mIsLastNodeInRange == ID_TRUE )  // Range�� ���� ������.
    {
        if( aIterator->mKeyRange->next != NULL ) // next key range�� �����ϸ�
        {
            aIterator->mKeyRange = aIterator->mKeyRange->next;

            IDE_TEST( iduCheckSessionEvent(aIterator->mProperties->mStatistics)
                      != IDE_SUCCESS);
            
            (void)beforeFirstInternal( aIterator );

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
        IDE_TEST( iduCheckSessionEvent( sStatistics )
                  != IDE_SUCCESS);

        IDE_TEST( makeNextRowCache( aIterator, sIndex )
                  != IDE_SUCCESS);

        goto read_from_cache;
    }

    IDE_EXCEPTION_CONT( SKIP_CACHE );
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : 
 * ------------------------------------------------------------------*
 * ���̺�� ���� �ڽſ��� �´� Stable Version�� ���� Row Filter ����
 * �Ѵ�.
 *********************************************************************/
IDE_RC stndrRTree::fetchRowCache( stndrIterator * aIterator,
                                  const void   ** aRow,
                                  idBool        * aNeedMoreCache )
{
    UChar           * sDataSlot;
    UChar           * sDataPage;
    idBool            sIsSuccess;
    idBool            sResult;
    sdSID             sRowSID;
    smSCN             sMyFstDskViewSCN;
    stndrHeader     * sIndex;
    scSpaceID         sTableTSID;
    sdcRowHdrInfo     sRowHdrInfo;
    idBool            sIsRowDeleted;
    idvSQL          * sStatistics;
    sdSID             sTSSlotSID;
    idBool            sIsMyTrans;
    UChar           * sDataSlotDir;
    idBool            sIsPageLatchReleased = ID_TRUE;
    smSCN             sFstDskViewSCN;
    

    sIndex           = (stndrHeader*)((smnIndexHeader*)(aIterator->mIndex))->mHeader;
    sTableTSID       = sIndex->mSdnHeader.mTableTSID;
    sStatistics      = aIterator->mProperties->mStatistics;
    sTSSlotSID       = smLayerCallback::getTSSlotSID(aIterator->mTrans);
    sMyFstDskViewSCN = smLayerCallback::getFstDskViewSCN(aIterator->mTrans);

    *aNeedMoreCache = ID_TRUE;
    
    while( aIterator->mCurRowPtr + 1 < aIterator->mCacheFence )
    {
        IDE_TEST(iduCheckSessionEvent(sStatistics) != IDE_SUCCESS);
        aIterator->mCurRowPtr++;

        sRowSID = SD_MAKE_SID( aIterator->mCurRowPtr->mRowPID,
                               aIterator->mCurRowPtr->mRowSlotNum);

        IDE_TEST( getPage( sStatistics,
                           &(sIndex->mQueryStat.mIndexPage),
                           sTableTSID,
                           SD_MAKE_PID( sRowSID ),
                           SDB_S_LATCH,
                           SDB_WAIT_NORMAL,
                           NULL, /* aMtx */
                           (UChar**)&sDataPage,
                           &sIsSuccess )
                  != IDE_SUCCESS );
        
        sIsPageLatchReleased = ID_FALSE;
        
        sDataSlotDir = sdpPhyPage::getSlotDirStartPtr(sDataPage);
        
        IDE_TEST( sdpSlotDirectory::getPagePtrFromSlotNum( 
                                                    sDataSlotDir,
                                                    SD_MAKE_SLOTNUM( sRowSID ),
                                                    &sDataSlot )
                  != IDE_SUCCESS );

        IDE_TEST( makeVRowFromRow( sIndex,
                                   sStatistics,
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
                                   (UChar*)*aRow,
                                   &sIsRowDeleted,
                                   &sIsPageLatchReleased )
                  != IDE_SUCCESS );

        if ( sIsRowDeleted == ID_TRUE )
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

        if( (aIterator->mFlag & SMI_ITERATOR_TYPE_MASK) 
            == SMI_ITERATOR_WRITE )
        {
            /* BUG-23319
             * [SD] �ε��� Scan�� sdcRow::fetch �Լ����� Deadlock �߻����ɼ��� ����. */
            /* row fetch�� �ϴ��߿� next rowpiece�� �̵��ؾ� �ϴ� ���,
             * ���� page�� latch�� Ǯ�� ������ deadlock �߻����ɼ��� �ִ�.
             * �׷��� page latch�� Ǭ ���� next rowpiece�� �̵��ϴµ�,
             * ���� �Լ������� page latch�� Ǯ������ ���θ� output parameter�� Ȯ���ϰ�
             * ��Ȳ�� ���� ������ ó���� �ؾ� �Ѵ�. */
            if( sIsPageLatchReleased == ID_TRUE )
            {
                IDE_TEST( getPage( sStatistics,
                                   &(sIndex->mQueryStat.mIndexPage),
                                   sTableTSID,
                                   SD_MAKE_PID( sRowSID ),
                                   SDB_S_LATCH,
                                   SDB_WAIT_NORMAL,
                                   NULL, /* aMtx */
                                   (UChar**)&sDataPage,
                                   &sIsSuccess )
                          != IDE_SUCCESS );
                
                sIsPageLatchReleased = ID_FALSE;

                sDataSlotDir = sdpPhyPage::getSlotDirStartPtr(sDataPage);
                IDE_TEST( sdpSlotDirectory::getPagePtrFromSlotNum(
                                                    sDataSlotDir,
                                                    SD_MAKE_SLOTNUM(sRowSID),
                                                    &sDataSlot )
                          != IDE_SUCCESS );
            }

            sIsMyTrans = sdcRow::isMyTransUpdating( sDataPage,
                                                    sDataSlot,
                                                    sMyFstDskViewSCN,
                                                    sTSSlotSID,
                                                    &sFstDskViewSCN );

            if ( sIsMyTrans == ID_TRUE )
            {
                sdcRow::getRowHdrInfo( sDataSlot, &sRowHdrInfo );

                if( SM_SCN_IS_GE( &sRowHdrInfo.mInfiniteSCN,
                                  &aIterator->mInfinite ) )
                {
                    IDE_ASSERT( 0 );
                }
            }
        }

        if( sIsPageLatchReleased == ID_FALSE )
        {
            sIsPageLatchReleased = ID_TRUE;
            IDE_TEST( sdbBufferMgr::releasePage( sStatistics,
                                                 (UChar*)sDataPage )
                      != IDE_SUCCESS );
        }

        SC_MAKE_GRID_WITH_SLOTNUM( aIterator->mRowGRID,
                                   sTableTSID,
                                   SD_MAKE_PID(sRowSID),
                                   SD_MAKE_SLOTNUM(sRowSID) );

        sIndex->mQueryStat.mRowFilterCount++;

        IDE_TEST( aIterator->mRowFilter->callback( &sResult,
                                                   *aRow,
                                                   NULL,
                                                   0,
                                                   aIterator->mRowGRID,
                                                   aIterator->mRowFilter->data) != IDE_SUCCESS );

        if( sResult == ID_FALSE )
        {
            continue;
        }

        // skip count ��ŭ row �ǳʶ�
        if( aIterator->mProperties->mFirstReadRecordPos > 0 )
        {
            aIterator->mProperties->mFirstReadRecordPos--;
        }
        else if( aIterator->mProperties->mReadRecordCount > 0 )
        {
            aIterator->mProperties->mReadRecordCount--;
            *aNeedMoreCache = ID_FALSE;

            break;
        }
        else
        {
            // Ŀ���� ���¸� after last���·� �Ѵ�.
            aIterator->mCurRowPtr = aIterator->mCacheFence;
            SC_MAKE_NULL_GRID( aIterator->mRowGRID );
            *aRow = NULL;

            *aNeedMoreCache = ID_FALSE;
            break;
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if( sIsPageLatchReleased == ID_FALSE )
    {
        IDE_PUSH();
        if( sdbBufferMgr::releasePage( sStatistics, (UChar*)sDataPage )
            != IDE_SUCCESS )
        {
            dumpIndexNode( (sdpPhyPageHdr *)sDataPage );
            IDE_ASSERT( 0 );
        }
        IDE_POP();
    }

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : 
 * ------------------------------------------------------------------*
 * row�κ��� VRow�� �����Ѵ�. ������ Fetch�������� ȣ��Ǹ�, RowFilter,
 * Qp�� �÷��ִ� �뵵�θ� ����Ѵ�.
 *********************************************************************/
IDE_RC stndrRTree::makeVRowFromRow( stndrHeader         * aIndex,
                                    idvSQL              * aStatistics,
                                    sdrMtx              * aMtx,
                                    sdrSavePoint        * aSP,
                                    void                * aTrans,
                                    void                * aTable,
                                    scSpaceID             aTableSpaceID,
                                    const UChar         * aRow,
                                    sdbPageReadMode       aPageReadMode,
                                    smiFetchColumnList  * aFetchColumnList,
                                    smFetchVersion        aFetchVersion,
                                    sdSID                 aTSSlotSID,
                                    const smSCN         * aSCN,
                                    const smSCN         * aInfiniteSCN,
                                    UChar               * aDestBuf,
                                    idBool              * aIsRowDeleted,
                                    idBool              * aIsPageLatchReleased )
{
    IDE_DASSERT( aTrans     != NULL );
    IDE_DASSERT( aRow       != NULL );
    IDE_DASSERT( aDestBuf   != NULL );

    IDE_DASSERT( checkFetchColumnList( aIndex, aFetchColumnList ) 
                 == IDE_SUCCESS );

    IDE_TEST( sdcRow::fetch( aStatistics,
                             aMtx,
                             aSP,
                             aTrans,
                             aTableSpaceID,
                             (UChar*)aRow,
                             ID_TRUE, /* aIsPersSlot */
                             aPageReadMode,
                             aFetchColumnList,
                             aFetchVersion,
                             aTSSlotSID,
                             aSCN,
                             aInfiniteSCN,
                             NULL,
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

/*********************************************************************
 * FUNCTION DESCRIPTION : 
 * ------------------------------------------------------------------*
 * Row�� �������� VRow������ fetchcolumnlist�� ���ȴ�. �ε��� �÷���
 * FetchColumnList�� �ݵ�� �����ؾ� �Ѵ�.
 *********************************************************************/
IDE_RC stndrRTree::checkFetchColumnList( stndrHeader        * aIndex,
                                         smiFetchColumnList * aFetchColumnList )
{
    stndrColumn         * sColumn;
    smiFetchColumnList  * sFetchColumnList;

    IDE_DASSERT( aIndex           != NULL );
    IDE_DASSERT( aFetchColumnList != NULL );

    sColumn = &aIndex->mColumn;
    
    for( sFetchColumnList = aFetchColumnList;
         sFetchColumnList != NULL;
         sFetchColumnList = sFetchColumnList->next )
    {
        if( sColumn->mVRowColumn.id == sFetchColumnList->column->id )
        {
            break;
        }
    }

    IDE_TEST( sFetchColumnList == NULL );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : 
 * ------------------------------------------------------------------*
 * ���� Leaf Node�� Ž���ϰ�, �ش� ���κ��� row cache�� �����Ѵ�.
 *********************************************************************/
IDE_RC stndrRTree::makeNextRowCache( stndrIterator * aIterator, stndrHeader * aIndex )
{
    idvSQL          * sStatistics = NULL;
    sdpPhyPageHdr   * sLeafNode = NULL;
    const smiRange  * sRange = NULL;
    stndrStack      * sStack = NULL;
    idBool            sIsRetry;
    UInt              sState = 0;


    sStatistics = aIterator->mProperties->mStatistics;
    sStack      = &aIterator->mStack;
    sRange      = aIterator->mKeyRange;

    IDE_TEST( findNextLeaf( sStatistics,
                            &(aIndex->mQueryStat),
                            aIndex,
                            &(sRange->maximum),
                            sStack,
                            &sLeafNode,
                            &sIsRetry )
              != IDE_SUCCESS );

    sState = 1;

    IDE_TEST( makeRowCache( aIterator, (UChar*)sLeafNode )
              != IDE_SUCCESS );

    if( sLeafNode != NULL )
    {
        IDE_TEST( sdbBufferMgr::releasePage( sStatistics, (UChar*)sLeafNode )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if( sState == 1 && sLeafNode != NULL )
    {
        if( sdbBufferMgr::releasePage( sStatistics, (UChar*)sLeafNode )
            != IDE_SUCCESS )
        {
            dumpIndexNode( sLeafNode );
            IDE_ASSERT( 0 );
        }
    }

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : 
 * ------------------------------------------------------------------*
 * �־��� callback�� �ǰ��Ͽ� keyrange�� �ش��ϴ� leaf key�� �ٷ� 
 * ������ Ŀ���� �̵���Ų��. 
 * �ַ� write lock���� traversing�Ҷ� ȣ��ȴ�. 
 * �ѹ� ȣ��� �Ŀ��� lock�� �ٽ� ���� �ʱ� ���� seekFunc�� �ٲ۴�. 
 *********************************************************************/
IDE_RC stndrRTree::beforeFirstW( stndrIterator      * aIterator,
                                 const smSeekFunc  ** aSeekFunc )
{
    (void) stndrRTree::beforeFirst( aIterator, aSeekFunc );

    // Seek funstion set ����
    *aSeekFunc += 6;

    return IDE_SUCCESS;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : 
 * ------------------------------------------------------------------*
 * �־��� callback�� �ǰ��Ͽ� keyrange�� �ش�Ǵ� ��� Row�� lock��  
 * �ɰ� ù Ű�� �ٷ� ������ Ŀ���� �ٽ� �̵���Ų��.                
 * �ַ� Repeatable read�� traversing�Ҷ� ȣ��ȴ�.                   
 * �ѹ� ȣ��� ���Ŀ� �ٽ� ȣ����� �ʵ���  SeekFunc�� �ٲ۴�.      
 *********************************************************************/
IDE_RC stndrRTree::beforeFirstRR( stndrIterator     * aIterator,
                                  const smSeekFunc ** aSeekFunc )
{
    stndrIterator       sIterator;
    stndrStack          sStack;
    idBool              sStackInit = ID_FALSE;
    smiCursorProperties sProp;

    IDE_TEST( stndrRTree::beforeFirst( aIterator, aSeekFunc ) != IDE_SUCCESS );

    IDE_TEST( stndrStackMgr::initialize(
                  &sStack,
                  stndrStackMgr::getStackSize( &aIterator->mStack ) )
              != IDE_SUCCESS );
    sStackInit = ID_TRUE;

    idlOS::memcpy( &sIterator, aIterator, ID_SIZEOF(stndrIterator) );
    idlOS::memcpy( &sProp, aIterator->mProperties, ID_SIZEOF(smiCursorProperties) );

    stndrStackMgr::copy( &sStack, &aIterator->mStack );
    
    IDE_TEST( stndrRTree::lockAllRows4RR( aIterator ) != IDE_SUCCESS );
    
    // beforefirst ���·� �ǵ��� ����.
    idlOS::memcpy( aIterator, &sIterator, ID_SIZEOF(stndrIterator) );
    idlOS::memcpy( aIterator->mProperties, &sProp, ID_SIZEOF(smiCursorProperties) );

    stndrStackMgr::copy( &aIterator->mStack, &sStack );

    /*
     * �ڽ��� ���� Lock Row�� ���� ���ؼ��� Cursor Infinite SCN��
     * 2���� ���Ѿ� �Ѵ�.
     */
    SM_ADD_SCN( &aIterator->mInfinite, 2 );
    
    *aSeekFunc += 6;

    sStackInit = ID_FALSE;
    IDE_TEST( stndrStackMgr::destroy( &sStack ) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if( sStackInit == ID_TRUE )
    {
        (void)stndrStackMgr::destroy( &sStack );
    }

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : 
 * ------------------------------------------------------------------*
 * �־��� key range�� �´� ��� key�� ����Ű�� Row���� ã�� lock��
 * �Ǵ�.
 * ���� key�� key range�� ���� ������ �ش� key range�� ������ ����
 * ���̹Ƿ� ���� key range�� ����Ͽ� �ٽ� �����Ѵ�.
 * key range�� ����� key�� �߿���
 *     1. Transaction level visibility�� ����ϰ�,
 *     2. Cursor level visibility�� ����ϰ�,
 *     3. Filter������ ����ϴ�
 *     4. Update������
 * Row�鸸 ��ȯ�Ѵ�. 
 * �� �Լ��� Key Range�� Filter�� �մ��ϴ� ��� Row�� lock�� �ɱ�
 * ������, �����Ŀ��� After Last ���°� �ȴ�.
 *********************************************************************/
IDE_RC stndrRTree::lockAllRows4RR( stndrIterator * aIterator )
{

    idBool            sIsRowDeleted;
    sdcUpdateState    sRetFlag;
    smTID             sWaitTxID;
    sdrMtx            sMtx;
    UChar           * sSlot;
    idBool            sIsSuccess;
    idBool            sResult;
    sdSID             sRowSID;
    scGRID            sRowGRID;
    idBool            sSkipLockRec = ID_FALSE;
    sdSID             sTSSlotSID   = smLayerCallback::getTSSlotSID(aIterator->mTrans);
    stndrHeader     * sIndex;
    scSpaceID         sTableTSID;
    UChar             sCTSlotIdx;
    sdrMtxStartInfo   sStartInfo;
    sdpPhyPageHdr   * sPageHdr;
    idBool            sIsPageLatchReleased = ID_TRUE;
    sdrSavePoint      sSP;
    UChar           * sDataPage;
    UChar           * sDataSlotDir;
    UInt              sState = 0;
    

    IDE_DASSERT( aIterator->mProperties->mLockRowBuffer != NULL );

    sIndex = (stndrHeader*)((smnIndexHeader*)(aIterator->mIndex))->mHeader;
    sTableTSID = sIndex->mSdnHeader.mTableTSID;

    sStartInfo.mTrans   = aIterator->mTrans;
    sStartInfo.mLogMode = SDR_MTX_LOGGING;

  read_from_cache:
    
    while( aIterator->mCurRowPtr + 1 < aIterator->mCacheFence )
    {
        IDE_TEST( iduCheckSessionEvent(aIterator->mProperties->mStatistics) != IDE_SUCCESS );
        aIterator->mCurRowPtr++;
        
      revisit_row:
        
        IDE_TEST( sdrMiniTrans::begin(aIterator->mProperties->mStatistics,
                                      &sMtx,
                                      aIterator->mTrans,
                                      SDR_MTX_LOGGING,
                                      ID_FALSE,/*MtxUndoable(PROJ-2162)*/
                                      gMtxDLogType | SM_DLOG_ATTR_UNDOABLE )
                  != IDE_SUCCESS );
        sState = 1;

        sRowSID = SD_MAKE_SID( aIterator->mCurRowPtr->mRowPID,
                               aIterator->mCurRowPtr->mRowSlotNum);

        sdrMiniTrans::setSavePoint( &sMtx, &sSP );

        IDE_TEST( stndrRTree::getPage( aIterator->mProperties->mStatistics,
                                       &(sIndex->mQueryStat.mIndexPage),
                                       sTableTSID,
                                       SD_MAKE_PID( sRowSID ),
                                       SDB_X_LATCH,
                                       SDB_WAIT_NORMAL,
                                       (void*)&sMtx,
                                       (UChar**)&sDataPage,
                                       &sIsSuccess )
                  != IDE_SUCCESS );
        
        sDataSlotDir = sdpPhyPage::getSlotDirStartPtr(sDataPage);
        IDE_TEST( sdpSlotDirectory::getPagePtrFromSlotNum( 
                                                    sDataSlotDir,
                                                    SD_MAKE_SLOTNUM( sRowSID ),
                                                    &sSlot )
                  != IDE_SUCCESS );
        
        sIsPageLatchReleased = ID_FALSE;
        IDE_TEST( makeVRowFromRow( sIndex,
                                   aIterator->mProperties->mStatistics,
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
        
        /* BUG-23319
         * [SD] �ε��� Scan�� sdcRow::fetch �Լ����� Deadlock �߻����ɼ��� ����. */
        /* row fetch�� �ϴ��߿� next rowpiece�� �̵��ؾ� �ϴ� ���,
         * ���� page�� latch�� Ǯ�� ������ deadlock �߻����ɼ��� �ִ�.
         * �׷��� page latch�� Ǭ ���� next rowpiece�� �̵��ϴµ�,
         * ���� �Լ������� page latch�� Ǯ������ ���θ� output parameter�� Ȯ���ϰ�
         * ��Ȳ�� ���� ������ ó���� �ؾ� �Ѵ�. */
        if( sIsPageLatchReleased == ID_TRUE )
        {
            IDE_TEST( sdbBufferMgr::getPageBySID(
                          aIterator->mProperties->mStatistics,
                          sTableTSID,
                          sRowSID,
                          SDB_X_LATCH,
                          SDB_WAIT_NORMAL,
                          &sMtx,
                          (UChar**)&sSlot,
                          &sIsSuccess )
                      != IDE_SUCCESS );
        }

        SC_MAKE_GRID_WITH_SLOTNUM( sRowGRID,
                                   sTableTSID,
                                   SD_MAKE_PID( sRowSID ),
                                   SD_MAKE_SLOTNUM( sRowSID ) );

        IDE_TEST( aIterator->mRowFilter->callback( &sResult,
                                                   aIterator->mProperties->mLockRowBuffer,
                                                   NULL,
                                                   0,
                                                   sRowGRID,
                                                   aIterator->mRowFilter->data) != IDE_SUCCESS );

        if( sResult == ID_FALSE )
        {
            sState = 0;
            IDE_TEST( sdrMiniTrans::commit(&sMtx) != IDE_SUCCESS );
            continue;
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
         * ���� ����, lock �õ� �� Rollback�ϴ� ���, ����ó��*/
        if( sRetFlag == SDC_UPTSTATE_REBUILD_ALREADY_MODIFIED )
        {
            /* ������ ���� ����ϰ�
               commit -> releaseLatch + rollback */
            IDE_RAISE( ERR_ALREADY_MODIFIED );
        }

        if( sRetFlag == SDC_UPTSTATE_UPDATE_BYOTHER )
        {
            sState = 0;
            IDE_TEST( sdrMiniTrans::commit(&sMtx) != IDE_SUCCESS );
            IDE_TEST( smLayerCallback::waitForTrans( 
                                     aIterator->mTrans,
                                     sWaitTxID,
                                     sTableTSID,
                                     aIterator->mProperties->mLockWaitMicroSec ) // aLockTimeOut
                      != IDE_SUCCESS );
            goto revisit_row;
        }
        else
        {
            if( sRetFlag == SDC_UPTSTATE_INVISIBLE_MYUPTVERSION )
            {
                sState = 0;
                IDE_TEST( sdrMiniTrans::commit(&sMtx) != IDE_SUCCESS );
                continue;
            }
        }

        // skip count ��ŭ row �ǳʶ�
        if( aIterator->mProperties->mFirstReadRecordPos > 0 )
        {
            aIterator->mProperties->mFirstReadRecordPos--;
        }
        else if( aIterator->mProperties->mReadRecordCount > 0 )
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

            sDataSlotDir =
              sdpPhyPage::getSlotDirStartPtr(sdpPhyPage::getPageStartPtr(sSlot));
            IDE_TEST( sdpSlotDirectory::getPagePtrFromSlotNum(
                                            sDataSlotDir,
                                            aIterator->mCurRowPtr->mRowSlotNum,
                                            &sSlot)
                      != IDE_SUCCESS );

            aIterator->mProperties->mReadRecordCount--;

            IDE_TEST(sdcRow::lock(aIterator->mProperties->mStatistics,
                                  sSlot,
                                  sRowSID,
                                  &(aIterator->mInfinite),
                                  &sMtx,
                                  sCTSlotIdx,
                                  &sSkipLockRec) != IDE_SUCCESS);

            SC_MAKE_GRID_WITH_SLOTNUM( aIterator->mRowGRID,
                                       sTableTSID,
                                       aIterator->mCurRowPtr->mRowPID,
                                       aIterator->mCurRowPtr->mRowSlotNum);
        }
        else
        {
            // �ʿ��� Row�� ���� ��� lock�� ȹ���Ͽ���...
            sState = 0;
            IDE_TEST( sdrMiniTrans::commit(&sMtx) != IDE_SUCCESS );
            return IDE_SUCCESS;
        }
        
        sState = 0;
        IDE_TEST( sdrMiniTrans::commit(&sMtx) != IDE_SUCCESS );
    }

    if( aIterator->mIsLastNodeInRange == ID_TRUE )  // Range�� ���� ������.
    {
        if( (aIterator->mFlag & SMI_RETRAVERSE_MASK) == SMI_RETRAVERSE_BEFORE )
        {
            if( aIterator->mKeyRange->next != NULL ) // next key range�� �����ϸ�
            {
                aIterator->mKeyRange = aIterator->mKeyRange->next;
                IDE_TEST(iduCheckSessionEvent(aIterator->mProperties->mStatistics) != IDE_SUCCESS);
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
            // Disk R-Tree������ SMI_RETRAVERSE_BEFORE�ܿ��� ����.
            IDE_ASSERT(0);
        }
    }
    else
    {
        // Key Range ������ ������ ���� ����
        // ���� Leaf Node�κ��� Index Cache ������ �����Ѵ�.
        // ���� ������ Index Cache �� ���� ������ ���� ����
        // read_from_cache �� �б��ϸ� �̸� �״�� ������.
        if( (aIterator->mFlag & SMI_RETRAVERSE_MASK) == SMI_RETRAVERSE_BEFORE )
        {
            IDE_TEST( makeNextRowCache( aIterator, sIndex ) != IDE_SUCCESS );
        }
        else
        {
            // Disk R-Tree������ SMI_RETRAVERSE_BEFORE�ܿ��� ����.
            IDE_ASSERT(0);
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
                             "[GEOMETRY INDEX VALIDATION(RR)] "
                             "SpaceID:%"ID_UINT32_FMT", "
                             "TableOID:%"ID_vULONG_FMT", "
                             "ViewSCN:%"ID_UINT64_FMT", "
                             "CSInfiniteSCN:%"ID_UINT64_FMT", "
                             "FSCNOrCSCN:%"ID_UINT64_FMT", "
                             "InfiniteSCN:%"ID_UINT64_FMT,
                              ((smcTableHeader*)aIterator->mTable)->mSpaceID,
                              ((smcTableHeader*)aIterator->mTable)->mSelfOID,
                             SM_SCN_TO_LONG( aIterator->mSCN ),
                             SM_SCN_TO_LONG( aIterator->mInfinite ),
                             SM_SCN_TO_LONG( sFSCNOrCSCN ),
                             SM_SCN_TO_LONG( sRowHdrInfo.mInfiniteSCN ) );
            IDE_SET( ideSetErrorCode(smERR_ABORT_StatementTooOld, sMsgBuf) );

            IDE_ERRLOG( IDE_SD_3 );
        }
        else
        {
            IDE_SET( ideSetErrorCode(smERR_RETRY_Already_Modified) );
        }

        IDE_ASSERT( sdcRow::releaseLatchForAlreadyModify( &sMtx, &sSP )
                    == IDE_SUCCESS );
    }
    IDE_EXCEPTION_END;

    IDE_PUSH();
    /* BUG-24151: [SC] Update Retry, Delete Retry, Statement Rebuild Count��
     *            AWI�� �߰��ؾ� �մϴ�.*/
    if( ideGetErrorCode() == smERR_RETRY_Already_Modified )
    {
        SMX_INC_SESSION_STATISTIC( sStartInfo.mTrans,
                                   IDV_STAT_INDEX_LOCKROW_RETRY_COUNT,
                                   1 /* Increase Value */ );

        smcTable::incStatAtABort( (smcTableHeader*)aIterator->mTable,
                                  SMC_INC_RETRY_CNT_OF_LOCKROW );
    }

    if( sState == 1 )
    {
        IDE_ASSERT( sdrMiniTrans::rollback(&sMtx) == IDE_SUCCESS );
    }
    IDE_POP();

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : 
 * ------------------------------------------------------------------*
 * Disk R-Tree�� Meta Page�� �ʱ�ȭ �Ѵ�.
 *********************************************************************/
IDE_RC stndrRTree::initMeta( UChar * aMetaPtr,
                             UInt    aBuildFlag,
                             void  * aMtx )
{
    sdrMtx      * sMtx;
    stndrMeta   * sMeta;
    scPageID      sPID = SD_NULL_PID;
    ULong         sFreeNodeCnt = 0;
    idBool        sIsConsistent = ID_FALSE;
    idBool        sIsCreatedWithLogging;
    idBool        sIsCreatedWithForce = ID_FALSE;
    smLSN         sNullLSN;
    smSCN         sFreeNodeSCN;
    UShort        sConvexhullPointNum = 0;

    sMtx = (sdrMtx*)aMtx;
    
    sIsCreatedWithLogging = ((aBuildFlag & SMI_INDEX_BUILD_LOGGING_MASK) ==
                             SMI_INDEX_BUILD_LOGGING) ? ID_TRUE : ID_FALSE ;

    sIsCreatedWithForce = ((aBuildFlag & SMI_INDEX_BUILD_FORCE_MASK) ==
                           SMI_INDEX_BUILD_FORCE) ? ID_TRUE : ID_FALSE ;

    SM_MAX_SCN( &sFreeNodeSCN );

    /* Index Specific Data �ʱ�ȭ */
    sMeta = (stndrMeta*)( aMetaPtr + SMN_INDEX_META_OFFSET );

    IDE_TEST( sdrMiniTrans::writeNBytes( sMtx,
                                         (UChar*)&sMeta->mRootNode,
                                         (void*)&sPID,
                                         ID_SIZEOF(sPID) ) != IDE_SUCCESS );
    
    IDE_TEST( sdrMiniTrans::writeNBytes( sMtx,
                                         (UChar*)&sMeta->mEmptyNodeHead,
                                         (void*)&sPID,
                                         ID_SIZEOF(sPID) ) != IDE_SUCCESS );
    
    IDE_TEST( sdrMiniTrans::writeNBytes( sMtx,
                                         (UChar*)&sMeta->mEmptyNodeTail,
                                         (void*)&sPID,
                                         ID_SIZEOF(sPID) ) != IDE_SUCCESS );

    IDE_TEST( sdrMiniTrans::writeNBytes( sMtx,
                                         (UChar*)&sMeta->mFreeNodeHead,
                                         (void*)&sPID,
                                         ID_SIZEOF(sPID) ) != IDE_SUCCESS );
    
    IDE_TEST( sdrMiniTrans::writeNBytes( sMtx,
                                         (UChar*)&sMeta->mFreeNodeCnt,
                                         (void*)&sFreeNodeCnt,
                                         ID_SIZEOF(sFreeNodeCnt) )
              != IDE_SUCCESS );
    
    IDE_TEST( sdrMiniTrans::writeNBytes( sMtx,
                                         (UChar*)&sMeta->mFreeNodeSCN,
                                         (void*)&sFreeNodeSCN,
                                         ID_SIZEOF(sFreeNodeSCN) )
              != IDE_SUCCESS );

    IDE_TEST( sdrMiniTrans::writeNBytes( sMtx,
                                         (UChar*)&sMeta->mConvexhullPointNum,
                                         (void*)&sConvexhullPointNum,
                                         ID_SIZEOF(sConvexhullPointNum) )
              != IDE_SUCCESS );
    
    
    IDE_TEST( sdrMiniTrans::writeNBytes( sMtx,
                                         (UChar*)&sMeta->mIsConsistent,
                                         (void*)&sIsConsistent,
                                         ID_SIZEOF(sIsConsistent) )
              != IDE_SUCCESS );
    
    IDE_TEST( sdrMiniTrans::writeNBytes( sMtx,
                                         (UChar*)&sMeta->mIsCreatedWithLogging,
                                         (void*)&sIsCreatedWithLogging,
                                         ID_SIZEOF(sIsCreatedWithLogging) )
              != IDE_SUCCESS );
    
    IDE_TEST( sdrMiniTrans::writeNBytes( sMtx,
                                         (UChar*)&sMeta->mIsCreatedWithForce,
                                         (void*)&sIsCreatedWithForce,
                                         ID_SIZEOF(sIsCreatedWithForce) )
              != IDE_SUCCESS );

    IDE_TEST( sdrMiniTrans::writeNBytes(
                  sMtx,
                  (UChar*)&(sMeta->mNologgingCompletionLSN.mFileNo),
                  (void*)&(sNullLSN.mFileNo),
                  ID_SIZEOF(sNullLSN.mFileNo) )
              != IDE_SUCCESS );
    
    IDE_TEST( sdrMiniTrans::writeNBytes(
                  sMtx,
                  (UChar*)&(sMeta->mNologgingCompletionLSN.mOffset),
                  (void*)&(sNullLSN.mOffset),
                  ID_SIZEOF(sNullLSN.mOffset) )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : 
 * ------------------------------------------------------------------*
 * Ű ���� ���� ������ ������ Ȯ���Ѵ�. �Ʒ��� ���̸� ���ε��� �ʴ´�.
 *   1. NULL
 *   2. Empty
 *   3. MBR�� MinX, MinY, MaxX, MaxY�� �ϳ��� NULL
 *********************************************************************/
void stndrRTree::isIndexableRow( void   * /* aIndex */,
                                 SChar  * aKeyValue,
                                 idBool * aIsIndexable )
{
    stdGeometryHeader * sValue;
    
    sValue = (stdGeometryHeader*)aKeyValue;
    
    stdUtils::isIndexableObject( sValue, aIsIndexable );
}

/*********************************************************************
 * FUNCTION DESCRIPTION :                                            
 * ------------------------------------------------------------------*
 * Key Column ���� ���ڿ� ���·� �����Ͽ� �ش�.
 *********************************************************************/
IDE_RC stndrRTree::columnValue2String( UChar * aColumnPtr,
                                       UChar * aText,
                                       UInt  * aTextLen )
{
    stdMBR  sMBR;
    UInt    sStringLen;
    
    idlOS::memcpy( &sMBR, aColumnPtr, sizeof(sMBR) );

    aText[0] = '\0';
    sStringLen = 0;

    sStringLen = idlVA::appendFormat(
        (SChar*)aText, 
        *aTextLen,
        "%"ID_FLOAT_G_FMT",%"ID_FLOAT_G_FMT",%"ID_FLOAT_G_FMT",%"ID_FLOAT_G_FMT,
        sMBR.mMinX, sMBR.mMinY, sMBR.mMaxX, sMBR.mMaxY );

    aText[sStringLen] = '\0';
    *aTextLen = sStringLen;

    return IDE_SUCCESS;
}

/*********************************************************************
 * FUNCTION DESCRIPTION :                                            
 * ------------------------------------------------------------------*
 * Column�� �м��Ͽ� ColumnValueLength�� ColumnValuePtr�� ���ϰ�,
 * �� �÷��� ���̸� ��ȯ�Ѵ�.
 *********************************************************************/
UShort stndrRTree::getColumnLength( UChar       * aColumnPtr,
                                    UInt        * aColumnHeaderLen,
                                    UInt        * aColumnValueLen,
                                    const void ** aColumnValuePtr )
{
    IDE_DASSERT( aColumnPtr       != NULL );
    IDE_DASSERT( aColumnHeaderLen != NULL );
    IDE_DASSERT( aColumnValueLen  != NULL );

    *aColumnHeaderLen = 0;
    *aColumnValueLen = ID_SIZEOF(stdMBR);

    if( aColumnValuePtr != NULL )
    {
        *aColumnValuePtr = aColumnPtr;
    }
    
    return (*aColumnHeaderLen) + (*aColumnValueLen);
}

/*********************************************************************
 * FUNCTION DESCRIPTION :
 * ------------------------------------------------------------------*
 * Disk R-Tree Meta Page Dump
 *
 * BUG-29039 codesonar ( Return Pointer to Local )
 *  - BUG-28379 �� �����ϰ� �Ѱܹ��� ���ۿ� ��� ��ȯ�ϵ��� ����
 *********************************************************************/
IDE_RC stndrRTree::dumpMeta( UChar * aPage ,
                             SChar * aOutBuf ,
                             UInt    aOutSize )
{
    stndrMeta * sMeta;

    IDE_TEST_RAISE( ( aPage == NULL ) ||
                    ( sdpPhyPage::getPageStartPtr(aPage) != aPage ) , SKIP );

    sMeta = (stndrMeta*)( aPage + SMN_INDEX_META_OFFSET );

    idlOS::snprintf( aOutBuf,
                     aOutSize,
                     "----------- Index Meta Page Begin ----------\n"
                     "mRootNode               : %"ID_UINT32_FMT"\n"
                     "mEmptyNodeHead          : %"ID_UINT32_FMT"\n"
                     "mEmptyNodeTail          : %"ID_UINT32_FMT"\n"
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
                     sMeta->mIsCreatedWithLogging,
                     sMeta->mNologgingCompletionLSN.mFileNo,
                     sMeta->mNologgingCompletionLSN.mOffset,
                     sMeta->mIsConsistent,
                     sMeta->mIsCreatedWithForce,
                     sMeta->mFreeNodeCnt,
                     sMeta->mFreeNodeHead );

    IDE_EXCEPTION_CONT( SKIP );

    return IDE_SUCCESS;
}

/*********************************************************************
 * FUNCTION DESCRIPTION :
 * ------------------------------------------------------------------*
 * TASK-4007 [SM] PBT�� ���� ��� �߰�
 * �ε��� �������� NodeHdr�� Dump�Ͽ� �ش�. �̶� ���� ��������
 * Leaf�������� ���, CTS�������� Dump�Ͽ� �����ش�.
 *********************************************************************/
IDE_RC stndrRTree::dumpNodeHdr( UChar * aPage,
                                SChar * aOutBuf,
                                UInt    aOutSize )
{
    stndrNodeHdr    * sNodeHdr;
    UInt              sCurrentOutStrSize;
    SChar             sStrFreeNodeSCN[ SM_SCN_STRING_LENGTH + 1];
    SChar             sStrNextFreeNodeSCN[ SM_SCN_STRING_LENGTH + 1];
#ifndef COMPILE_64BIT
    ULong               sSCN;
#endif

    IDE_DASSERT( aPage != NULL )
    IDE_DASSERT( sdpPhyPage::getPageStartPtr(aPage) == aPage );

    sNodeHdr = (stndrNodeHdr*)sdpPhyPage::getLogicalHdrStartPtr( aPage );

    // make scn string
    idlOS::memset( sStrFreeNodeSCN,     0x00, SM_SCN_STRING_LENGTH + 1 );
    idlOS::memset( sStrNextFreeNodeSCN, 0x00, SM_SCN_STRING_LENGTH + 1 );
#ifdef COMPILE_64BIT
    idlOS::sprintf( (SChar*)sStrFreeNodeSCN,
                    "%"ID_XINT64_FMT, sNodeHdr->mFreeNodeSCN );
    
    idlOS::sprintf( (SChar*)sStrNextFreeNodeSCN,
                    "%"ID_XINT64_FMT, sNodeHdr->mNextFreeNodeSCN );
#else
    sSCN  = (ULong)sNodeHdr->mFreeNodeSCN.mHigh << 32;
    sSCN |= (ULong)sNodeHdr->mFreeNodeSCN.mLow;
    idlOS::snprintf( (SChar*)sStrFreeNodeSCN, SM_SCN_STRING_LENGTH,
                     "%"ID_XINT64_FMT, sSCN );

    sSCN  = (ULong)sNodeHdr->mNextFreeNodeSCN.mHigh << 32;
    sSCN |= (ULong)sNodeHdr->mNextFreeNodeSCN.mLow;
    idlOS::snprintf( (SChar*)sStrNextFreeNodeSCN, SM_SCN_STRING_LENGTH,
                     "%"ID_XINT64_FMT, sSCN );
#endif

    // make output
    idlOS::snprintf( aOutBuf,
                     aOutSize,
                     "----------- Index Page Logical Header Begin ----------\n"
                     "MinX            : %"ID_FLOAT_G_FMT"\n"
                     "MinY            : %"ID_FLOAT_G_FMT"\n"
                     "MaxX            : %"ID_FLOAT_G_FMT"\n"
                     "MaxY            : %"ID_FLOAT_G_FMT"\n"
                     "NextEmptyNode   : %"ID_UINT32_FMT"\n"
                     "NextFreeNode    : %"ID_UINT32_FMT"\n"
                     "FreeNodeSCN     : %s\n",
                     "NextFreeNodeSCN : %s\n",
                     "Height          : %"ID_UINT32_FMT"\n"
                     "UnlimitedKeyCnt : %"ID_UINT32_FMT"\n"
                     "TotalDeadKeySize: %"ID_UINT32_FMT"\n"
                     "TBKCount        : %"ID_UINT32_FMT"\n"
                     "RefTBK1         : %"ID_UINT32_FMT"\n"
                     "RefTBK2         : %"ID_UINT32_FMT"\n"
                     "State(U,E,F)    : %"ID_UINT32_FMT"\n"
                     "----------- Index Page Logical Header End ------------\n",
                     sNodeHdr->mMBR.mMinX,
                     sNodeHdr->mMBR.mMinY,
                     sNodeHdr->mMBR.mMaxX,
                     sNodeHdr->mMBR.mMaxY,
                     sNodeHdr->mNextEmptyNode,
                     sNodeHdr->mNextFreeNode,
                     sStrFreeNodeSCN,
                     sStrNextFreeNodeSCN,
                     sNodeHdr->mHeight,
                     sNodeHdr->mUnlimitedKeyCount,
                     sNodeHdr->mTotalDeadKeySize,
                     sNodeHdr->mTBKCount,
                     sNodeHdr->mRefTBK[0],
                     sNodeHdr->mRefTBK[1],
                     sNodeHdr->mState );


    // Leaf���, CTL�� Dump
    if( sNodeHdr->mHeight == 0 )
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

/*********************************************************************
 * FUNCTION DESCRIPTION :
 * ------------------------------------------------------------------*
 * Disk R-Tree Index Page Dump
 *
 * BUG-29039 codesonar ( Return Pointer to Local )
 *  - BUG-28379 �� �����ϰ� �Ѱܹ��� ���ۿ� ��� ��ȯ�ϵ��� ����
 *********************************************************************/
IDE_RC stndrRTree::dump( UChar * aPage ,
                         SChar * aOutBuf ,
                         UInt    aOutSize )
{
    stndrNodeHdr    * sNodeHdr;
    UChar           * sSlotDirPtr;
    sdpSlotDirHdr   * sSlotDirHdr;
    sdpSlotEntry    * sSlotEntry;
    UShort            sSlotCount;
    UShort            sSlotNum;
    UShort            sOffset;
    SChar             sValueBuf[ SM_DUMP_VALUE_BUFFER_SIZE ]={0};
    stndrLKey       * sLKey;
    stndrIKey       * sIKey;
    stndrKeyInfo      sKeyInfo;
    scPageID          sChildPID;

    IDE_TEST_RAISE( ( aPage == NULL ) ||
                    ( sdpPhyPage::getPageStartPtr(aPage) != aPage ) , SKIP );

    sNodeHdr = (stndrNodeHdr*)sdpPhyPage::getLogicalHdrStartPtr( aPage );
    sSlotDirPtr = sdpPhyPage::getSlotDirStartPtr( (UChar*)aPage );
    sSlotDirHdr = (sdpSlotDirHdr*)sSlotDirPtr;
    sSlotCount  = sSlotDirHdr->mSlotEntryCount;

    sKeyInfo.mKeyState = 0;

    idlOS::snprintf( aOutBuf,
                     aOutSize,
"--------------------- Index Key Begin -----------------------------------------\n"
" No.|mOffset|mChildPID mRowPID mRowSlotNum mKeyState mKeyValue \n"
"----+-------+------------------------------------------------------------------\n" );

    for( sSlotNum=0; sSlotNum < sSlotCount; sSlotNum++ )
    {
        sSlotEntry = SDP_GET_SLOT_ENTRY(sSlotDirPtr, sSlotNum);

        sOffset = SDP_GET_OFFSET( sSlotEntry );
        if( SD_PAGE_SIZE < sOffset )
        {
            idlVA::appendFormat( aOutBuf,
                                 aOutSize,
                                 "Invalid slot Offset (%d)\n",
                                 sSlotEntry );
            continue;
        }

        if( sNodeHdr->mHeight == 0 ) // Leaf node
        {
            sLKey = (stndrLKey*)sdpPhyPage::getPagePtrFromOffset( aPage, sOffset );

            STNDR_LKEY_TO_KEYINFO( sLKey, sKeyInfo );

            ideLog::ideMemToHexStr( sKeyInfo.mKeyValue,
                                    SM_DUMP_VALUE_LENGTH,
                                    IDE_DUMP_FORMAT_VALUEONLY,
                                    sValueBuf,
                                    SM_DUMP_VALUE_BUFFER_SIZE );

            idlVA::appendFormat( aOutBuf,
                                 aOutSize,
                                 "%4"ID_UINT32_FMT"|"
                                 "%7"ID_UINT32_FMT"|"
                                 " <leaf node>  "
                                 "%8"ID_UINT32_FMT
                                 "%12"ID_UINT32_FMT
                                 "%10"ID_UINT32_FMT
                                 " %s\n",
                                 sSlotNum,
                                 *sSlotEntry,
                                 sKeyInfo.mRowPID,
                                 sKeyInfo.mRowSlotNum,
                                 sKeyInfo.mKeyState,
                                 sValueBuf );
        }
        else
        {   // InternalNode
            sIKey = (stndrIKey*)sdpPhyPage::getPagePtrFromOffset( aPage, sOffset );

            STNDR_IKEY_TO_KEYINFO( sIKey, sKeyInfo );
            STNDR_GET_CHILD_PID( &sChildPID, sIKey );

            ideLog::ideMemToHexStr( sKeyInfo.mKeyValue,
                                    SM_DUMP_VALUE_LENGTH,
                                    IDE_DUMP_FORMAT_VALUEONLY,
                                    sValueBuf,
                                    SM_DUMP_VALUE_BUFFER_SIZE );

            idlVA::appendFormat( aOutBuf,
                                 aOutSize,
                                 "%4"ID_UINT32_FMT"|"
                                 "%7"ID_UINT32_FMT"|"
                                 "%14"ID_UINT32_FMT
                                 "%8"ID_UINT32_FMT
                                 "%12"ID_UINT32_FMT
                                 "%10"ID_UINT32_FMT"\n",
                                 sSlotNum,
                                 *sSlotEntry,
                                 sChildPID,
                                 sKeyInfo.mRowPID,
                                 sKeyInfo.mRowSlotNum,
                                 sKeyInfo.mKeyState,
                                 sValueBuf );
        }
    }
    idlVA::appendFormat( aOutBuf,
                         aOutSize,
"--------------------- Index Key End   -----------------------------------------\n" );

    IDE_EXCEPTION_CONT( SKIP );

    return IDE_SUCCESS;
}
