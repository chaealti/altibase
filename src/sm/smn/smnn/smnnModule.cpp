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
 * $Id: smnnModule.cpp 90158 2021-03-10 00:30:31Z emlee $
 **********************************************************************/

#include <ide.h>
#include <idu.h>
#include <smi.h>
#include <smc.h>
#include <smm.h>
#include <smp.h>

#include <smErrorCode.h>
#include <smnModules.h>
#include <smnReq.h>
#include <smnnDef.h>
#include <smnnModule.h>
#include <smnManager.h>
#include <smmExpandChunk.h>

smnIndexModule smnnModule = {
    SMN_MAKE_INDEX_MODULE_ID( SMI_TABLE_MEMORY,
                              SMI_BUILTIN_SEQUENTIAL_INDEXTYPE_ID ),
    SMN_RANGE_DISABLE|SMN_DIMENSION_DISABLE,
    ID_SIZEOF( smnnIterator ),
    (smnMemoryFunc) smnnSeq::prepareIteratorMem,
    (smnMemoryFunc) smnnSeq::releaseIteratorMem,
    (smnMemoryFunc) NULL,
    (smnMemoryFunc) NULL,
    (smnCreateFunc) NULL,
    (smnDropFunc)   NULL,
    (smTableCursorLockRowFunc)  smnManager::lockRow,
    (smnDeleteFunc)       NULL,
    (smnFreeFunc)         NULL,
    (smnInsertRollbackFunc)    NULL,
    (smnDeleteRollbackFunc)    NULL,
    (smnAgingFunc)             NULL,
    (smInitFunc)          smnnSeq::init,
    (smDestFunc)          smnnSeq::dest,
    (smnFreeNodeListFunc) NULL,
    (smnGetPositionFunc)  NULL,
    (smnSetPositionFunc)  NULL,

    (smnAllocIteratorFunc) smnnSeq::allocIterator,
    (smnFreeIteratorFunc)  smnnSeq::freeIterator,
    (smnReInitFunc)        NULL,
    (smnInitMetaFunc)      NULL,
    
    (smnMakeDiskImageFunc)  NULL,

    (smnBuildIndexFunc)     NULL,
    (smnGetSmoNoFunc)       NULL,
    (smnMakeKeyFromRow)     NULL,
    (smnMakeKeyFromSmiValue)NULL,
    (smnRebuildIndexColumn) NULL,
    (smnSetIndexConsistency)NULL,
    (smnGatherStat)         smnnSeq::gatherStat
};

static const smSeekFunc smnnSeekFunctions[32][12] =
{ { /* SMI_TRAVERSE_FORWARD  SMI_PREVIOUS_DISABLE SMI_LOCK_READ            */
        (smSeekFunc)smnnSeq::fetchNext,
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::beforeFirst,
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::AA,
        (smSeekFunc)smnnSeq::AA,
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::AA,
        (smSeekFunc)smnnSeq::AA
    },
    { /* SMI_TRAVERSE_FORWARD  SMI_PREVIOUS_DISABLE SMI_LOCK_WRITE           */
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::beforeFirstU,
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::AA,
        (smSeekFunc)smnnSeq::AA,
        (smSeekFunc)smnnSeq::fetchNext,
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::AA,
        (smSeekFunc)smnnSeq::AA
    },
    { /* SMI_TRAVERSE_FORWARD  SMI_PREVIOUS_DISABLE SMI_LOCK_REPEATALBE      */
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::beforeFirstR,
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::AA,
        (smSeekFunc)smnnSeq::AA,
        (smSeekFunc)smnnSeq::fetchNext,
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::beforeFirst,
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::AA,
        (smSeekFunc)smnnSeq::AA
    },
    { /* SMI_TRAVERSE_FORWARD  SMI_PREVIOUS_DISABLE SMI_LOCK_TABLE_SHARED    */
        (smSeekFunc)smnnSeq::fetchNext,
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::beforeFirst,
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::AA,
        (smSeekFunc)smnnSeq::AA,
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::AA,
        (smSeekFunc)smnnSeq::AA
    },
    { /* SMI_TRAVERSE_FORWARD  SMI_PREVIOUS_DISABLE SMI_LOCK_TABLE_EXCLUSIVE */
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::beforeFirstU,
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::AA,
        (smSeekFunc)smnnSeq::AA,
        (smSeekFunc)smnnSeq::fetchNext,
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::AA,
        (smSeekFunc)smnnSeq::AA
    },
    { /* UNUSED                                                              */
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::NA
    },
    { /* UNUSED                                                              */
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::NA
    },
    { /* UNUSED                                                              */
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::NA
    },
    { /* SMI_TRAVERSE_FORWARD  SMI_PREVIOUS_ENABLE  SMI_LOCK_READ            */
        (smSeekFunc)smnnSeq::fetchNext,
        (smSeekFunc)smnnSeq::fetchPrev,
        (smSeekFunc)smnnSeq::beforeFirst,
        (smSeekFunc)smnnSeq::afterLast,
        (smSeekFunc)smnnSeq::AA,
        (smSeekFunc)smnnSeq::AA,
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::AA,
        (smSeekFunc)smnnSeq::AA
    },
    { /* SMI_TRAVERSE_FORWARD  SMI_PREVIOUS_ENABLE  SMI_LOCK_WRITE           */
        (smSeekFunc)smnnSeq::fetchNextU,
        (smSeekFunc)smnnSeq::fetchPrevU,
        (smSeekFunc)smnnSeq::beforeFirst,
        (smSeekFunc)smnnSeq::afterLast,
        (smSeekFunc)smnnSeq::AA,
        (smSeekFunc)smnnSeq::AA,
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::AA,
        (smSeekFunc)smnnSeq::AA
    },
    { /* SMI_TRAVERSE_FORWARD  SMI_PREVIOUS_ENABLE  SMI_LOCK_REPEATALBE      */
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::beforeFirstR,
        (smSeekFunc)smnnSeq::afterLastR,
        (smSeekFunc)smnnSeq::AA,
        (smSeekFunc)smnnSeq::AA,
        (smSeekFunc)smnnSeq::fetchNext,
        (smSeekFunc)smnnSeq::fetchPrev,
        (smSeekFunc)smnnSeq::beforeFirst,
        (smSeekFunc)smnnSeq::afterLast,
        (smSeekFunc)smnnSeq::AA,
        (smSeekFunc)smnnSeq::AA
    },
    { /* SMI_TRAVERSE_FORWARD  SMI_PREVIOUS_ENABLE  SMI_LOCK_TABLE_SHARED    */
        (smSeekFunc)smnnSeq::fetchNext,
        (smSeekFunc)smnnSeq::fetchPrev,
        (smSeekFunc)smnnSeq::beforeFirst,
        (smSeekFunc)smnnSeq::afterLast,
        (smSeekFunc)smnnSeq::AA,
        (smSeekFunc)smnnSeq::AA,
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::AA,
        (smSeekFunc)smnnSeq::AA
    },
    { /* SMI_TRAVERSE_FORWARD  SMI_PREVIOUS_ENABLE  SMI_LOCK_TABLE_EXCLUSIVE */
        (smSeekFunc)smnnSeq::fetchNextU,
        (smSeekFunc)smnnSeq::fetchPrevU,
        (smSeekFunc)smnnSeq::beforeFirst,
        (smSeekFunc)smnnSeq::afterLast,
        (smSeekFunc)smnnSeq::AA,
        (smSeekFunc)smnnSeq::AA,
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::AA,
        (smSeekFunc)smnnSeq::AA
    },
    { /* UNUSED                                                              */
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::NA
    },
    { /* UNUSED                                                              */
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::NA
    },
    { /* UNUSED                                                              */
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::NA
    },
    { /* SMI_TRAVERSE_BACKWARD SMI_PREVIOUS_DISABLE SMI_LOCK_READ            */
        (smSeekFunc)smnnSeq::fetchPrev,
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::afterLast,
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::AA,
        (smSeekFunc)smnnSeq::AA,
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::AA,
        (smSeekFunc)smnnSeq::AA
    },
    { /* SMI_TRAVERSE_BACKWARD SMI_PREVIOUS_DISABLE SMI_LOCK_WRITE           */
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::afterLastU,
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::AA,
        (smSeekFunc)smnnSeq::AA,
        (smSeekFunc)smnnSeq::fetchPrev,
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::AA,
        (smSeekFunc)smnnSeq::AA
    },
    { /* SMI_TRAVERSE_BACKWARD SMI_PREVIOUS_DISABLE SMI_LOCK_REPEATABLE      */
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::afterLastR,
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::AA,
        (smSeekFunc)smnnSeq::AA,
        (smSeekFunc)smnnSeq::fetchPrev,
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::afterLast,
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::AA,
        (smSeekFunc)smnnSeq::AA
    },
    { /* SMI_TRAVERSE_BACKWARD SMI_PREVIOUS_DISABLE SMI_LOCK_TABLE_SHARED    */
        (smSeekFunc)smnnSeq::fetchPrev,
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::afterLast,
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::AA,
        (smSeekFunc)smnnSeq::AA,
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::AA,
        (smSeekFunc)smnnSeq::AA
    },
    { /* SMI_TRAVERSE_BACKWARD SMI_PREVIOUS_DISABLE SMI_LOCK_TABLE_EXCLUSIVE */
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::afterLastU,
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::AA,
        (smSeekFunc)smnnSeq::AA,
        (smSeekFunc)smnnSeq::fetchPrev,
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::AA,
        (smSeekFunc)smnnSeq::AA
    },
    { /* UNUSED                                                              */
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::NA
    },
    { /* UNUSED                                                              */
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::NA
    },
    { /* UNUSED                                                              */
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::NA
    },
    { /* SMI_TRAVERSE_BACKWARD SMI_PREVIOUS_ENABLE  SMI_LOCK_READ            */
        (smSeekFunc)smnnSeq::fetchPrev,
        (smSeekFunc)smnnSeq::fetchNext,
        (smSeekFunc)smnnSeq::afterLast,
        (smSeekFunc)smnnSeq::beforeFirst,
        (smSeekFunc)smnnSeq::AA,
        (smSeekFunc)smnnSeq::AA,
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::AA,
        (smSeekFunc)smnnSeq::AA
    },
    { /* SMI_TRAVERSE_BACKWARD SMI_PREVIOUS_ENABLE  SMI_LOCK_WRITE           */
        (smSeekFunc)smnnSeq::fetchPrevU,
        (smSeekFunc)smnnSeq::fetchNextU,
        (smSeekFunc)smnnSeq::afterLast,
        (smSeekFunc)smnnSeq::beforeFirst,
        (smSeekFunc)smnnSeq::AA,
        (smSeekFunc)smnnSeq::AA,
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::AA,
        (smSeekFunc)smnnSeq::AA
    },
    { /* SMI_TRAVERSE_BACKWARD SMI_PREVIOUS_ENABLE  SMI_LOCK_REPEATABLE      */
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::afterLastR,
        (smSeekFunc)smnnSeq::beforeFirstR,
        (smSeekFunc)smnnSeq::AA,
        (smSeekFunc)smnnSeq::AA,
        (smSeekFunc)smnnSeq::fetchPrev,
        (smSeekFunc)smnnSeq::fetchNext,
        (smSeekFunc)smnnSeq::afterLast,
        (smSeekFunc)smnnSeq::beforeFirst,
        (smSeekFunc)smnnSeq::AA,
        (smSeekFunc)smnnSeq::AA
    },
    { /* SMI_TRAVERSE_BACKWARD SMI_PREVIOUS_ENABLE  SMI_LOCK_TABLE_SHARED    */
        (smSeekFunc)smnnSeq::fetchPrev,
        (smSeekFunc)smnnSeq::fetchNext,
        (smSeekFunc)smnnSeq::afterLast,
        (smSeekFunc)smnnSeq::beforeFirst,
        (smSeekFunc)smnnSeq::AA,
        (smSeekFunc)smnnSeq::AA,
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::AA,
        (smSeekFunc)smnnSeq::AA
    },
    { /* SMI_TRAVERSE_BACKWARD SMI_PREVIOUS_ENABLE  SMI_LOCK_TABLE_EXCLUSIVE */
        (smSeekFunc)smnnSeq::fetchPrevU,
        (smSeekFunc)smnnSeq::fetchNextU,
        (smSeekFunc)smnnSeq::afterLast,
        (smSeekFunc)smnnSeq::beforeFirst,
        (smSeekFunc)smnnSeq::AA,
        (smSeekFunc)smnnSeq::AA,
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::AA,
        (smSeekFunc)smnnSeq::AA
    },
    { /* UNUSED                                                              */
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::NA
    },
    { /* UNUSED                                                              */
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::NA
    },
    { /* UNUSED                                                              */
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::NA,
        (smSeekFunc)smnnSeq::NA
    }
};


IDE_RC smnnSeq::prepareIteratorMem( const smnIndexModule* )
{
    return IDE_SUCCESS;
}

IDE_RC smnnSeq::releaseIteratorMem(const smnIndexModule* )
{
    return IDE_SUCCESS;
}

IDE_RC smnnSeq::init( smnnIterator      * aIterator,
                      void              * aTrans,
                      smcTableHeader    * aTable,
                      smnIndexHeader    *,
                      void              *,
                      const smiRange    *,
                      const smiRange    *,
                      const smiCallBack * aFilter,
                      UInt                aFlag,
                      smSCN               aSCN,
                      smSCN               aInfinite,
                      idBool             ,
                      smiCursorProperties * aProperties,
                      const smSeekFunc   ** aSeekFunc,
                      smiStatement        * aStatement )
{
    idvSQL                    *sSQLStat;

    aIterator->SCN                = aSCN;
    aIterator->infinite           = aInfinite;
    aIterator->trans              = aTrans;
    aIterator->table              = aTable;
    aIterator->curRecPtr          = NULL;
    aIterator->lstFetchRecPtr     = NULL;
    SC_MAKE_NULL_GRID( aIterator->mRowGRID );
    aIterator->tid                = smLayerCallback::getTransID( aTrans );
    aIterator->filter             = aFilter;
    aIterator->mProperties        = aProperties;
    aIterator->mScanBackPID       = SM_NULL_PID;
    aIterator->mScanBackModifySeq = 0;
    aIterator->mModifySeqForScan  = 0;
    aIterator->page               = SM_NULL_PID;
    aIterator->mStatement         = aStatement;

    *aSeekFunc = smnnSeekFunctions[aFlag&(SMI_TRAVERSE_MASK|
                                          SMI_PREVIOUS_MASK|
                                          SMI_LOCK_MASK)];

    sSQLStat = aIterator->mProperties->mStatistics;
    IDV_SQL_ADD(sSQLStat, mMemCursorSeqScan, 1);
    
    if (sSQLStat != NULL)
    {
        IDV_SESS_ADD(sSQLStat->mSess, IDV_STAT_INDEX_MEM_CURSOR_SEQ_SCAN, 1);
    }
    
    return IDE_SUCCESS;
    
}

IDE_RC smnnSeq::dest( smnnIterator* /*aIterator*/ )
{
    return IDE_SUCCESS;
}

IDE_RC smnnSeq::AA( smnnIterator*,
                    const void** )
{

    return IDE_SUCCESS;

}

IDE_RC smnnSeq::NA( void )
{

    IDE_SET( ideSetErrorCode( smERR_ABORT_smiTraverseNotApplicable ) );

    return IDE_FAILURE;

}

IDE_RC smnnSeq::beforeFirst( smnnIterator* aIterator,
                             const smSeekFunc** )
{

    smpPageListEntry* sFixed;
    vULong            sSize;
#ifdef DEBUG
    smTID             sTransID;
#endif
    idBool            sLocked = ID_FALSE;
    SChar*            sRowPtr;
    SChar*            sPagePtr;
    SChar**           sSlot;
    SChar**           sHighFence;
    SChar*            sFence;
    smpSlotHeader*    sRow;
    scGRID            sRowGRID;
    idBool            sResult;
    smxTrans*         sTrans               = (smxTrans*)aIterator->trans;
    idBool            sCanReusableRollback = ID_TRUE;
    idBool            sReusableCheckAgain  = ID_FALSE;
    idBool            sIsVisible;

    sFixed                       = (smpPageListEntry*)&(aIterator->table->mFixed);
    aIterator->curRecPtr         = NULL;
    aIterator->lstFetchRecPtr    = NULL;
    aIterator->page              = SM_NULL_PID;
    aIterator->lowFence          = aIterator->rows;
    aIterator->highFence         = aIterator->lowFence - 1;
    aIterator->slot              = aIterator->highFence;
    aIterator->least             = ID_TRUE;
    aIterator->highest           = ID_TRUE;
    sSize                        = sFixed->mSlotSize;
#ifdef  DEBUG
    sTransID                     = aIterator->tid;
#endif
    /* __CRASH_TOLERANCE Property�� ���� ������, Tablespace�� FLI�� �̿���
     * ��ȸ�ϴ� ������� Page�� ������ */
    if( smuProperty::getCrashTolerance() == 0 )
    {
        if ( aIterator->mProperties->mParallelReadProperties.mThreadCnt == 1 )
        {
            IDE_TEST( moveNext( aIterator ) != IDE_SUCCESS );
        }
        else
        {
            IDE_TEST( moveNextParallel( aIterator ) != IDE_SUCCESS );
        }
    }
    else
    {
        IDE_TEST( moveNextUsingFLI( aIterator ) != IDE_SUCCESS );
    }
    
    if( ( aIterator->page != SM_NULL_PID ) && 
        ( aIterator->page != SM_SPECIAL_PID ) &&
        ( aIterator->mProperties->mReadRecordCount > 0) )
    {
        /* BUG-39423 : trace log ��� �� ���� ���� �� ���� �۵� ��� ��� ���� */
#ifdef DEBUG               
        if(   ( aIterator->page < smpManager::getFirstScanPageID(sFixed) )
            ||( aIterator->page > smpManager::getLastScanPageID(sFixed)  ) )
        { 
            /* Scan Page List�� �������� ���� page�� ��� */
            if( aIterator->mModifySeqForScan == 0 )
            {
                ideLog::log(IDE_SM_0,
                            SM_TRC_MINDEX_INDEX_INFO,
                            sTransID,
                            __FILE__,
                            __LINE__,
                            aIterator->page,
                            smpManager::getFirstScanPageID(sFixed),
                            smpManager::getLastScanPageID(sFixed));
            }
        }        
#endif       

        IDE_TEST( smmManager::holdPageSLatch(aIterator->table->mSpaceID,
                                             aIterator->page) != IDE_SUCCESS );
        sLocked = ID_TRUE;

        IDE_ASSERT( smmManager::getPersPagePtr( aIterator->table->mSpaceID, 
                                                aIterator->page,
                                                (void**)&sPagePtr )
                    == IDE_SUCCESS );

        IDE_ASSERT( sPagePtr != NULL );
            
        /* BUG-48353
           checkSCN() ���� Pending Wait �� �߻��Ҽ��ֱ� ������
           PAGE release ���ķ� checkSCN()�� �̷��.
           ���⼭�� row pointer ��θ� ĳ���� �����Ѵ�. */
        for( sRowPtr    = sPagePtr + SMP_PERS_PAGE_BODY_OFFSET,
             sHighFence = aIterator->highFence,
             sFence     = sRowPtr + sFixed->mSlotCount * sSize;
             sRowPtr    < sFence;
             sRowPtr   += sSize )
        {
            *(++sHighFence) = sRowPtr;
        }

        sLocked = ID_FALSE;
        IDE_TEST( smmManager::releasePageLatch(aIterator->table->mSpaceID,
                                               aIterator->page)
                  != IDE_SUCCESS );

        IDE_TEST(iduCheckSessionEvent(aIterator->mProperties->mStatistics)
                 != IDE_SUCCESS);
           
        for( sSlot  = aIterator->slot + 1;
             sSlot <= sHighFence;
             sSlot++ )
        {
            sRow = (smpSlotHeader *)(*sSlot);

            IDE_TEST( smnManager::checkSCN( (smiIterator *)aIterator,
                                            sRow,
                                            &sCanReusableRollback,
                                            &sIsVisible,
                                            ID_TRUE )
                      != IDE_SUCCESS );

            if ( sIsVisible == ID_FALSE )
            {
                continue;
            }

            aIterator->mScanBackPID = aIterator->page;
            aIterator->mScanBackModifySeq = aIterator->mModifySeqForScan;

            if( sCanReusableRollback == ID_FALSE )
            {
                sReusableCheckAgain = ID_TRUE;
            }

            SC_MAKE_GRID( sRowGRID,
                          aIterator->table->mSpaceID,
                          SMP_SLOT_GET_PID( sRow ),
                          SMP_SLOT_GET_OFFSET( sRow ) );

            IDE_TEST( aIterator->filter->callback( &sResult,
                                                   *sSlot,
                                                   NULL,
                                                   0,
                                                   sRowGRID,
                                                   aIterator->filter->data )
                      != IDE_SUCCESS );
            if( sResult == ID_TRUE )
            {
                /* SCN ������ ����� slot�� �ϳ��� cursor reusable üũ�� �ʿ��� ���
                 * filter ������ ����� slot�� ������� �ٽ� Ȯ���ؾ� �Ѵ�. */
                if( ( sReusableCheckAgain == ID_TRUE ) && ( sTrans->mIsReusableRollback == ID_TRUE ) )
                {
                    if( smnManager::checkCanReusableRollback( ( smiIterator* )aIterator, 
                                                              *sSlot ) == ID_FALSE )
                    {
                        sTrans->mIsReusableRollback = ID_FALSE;  
                    }
                }

                if(aIterator->mProperties->mFirstReadRecordPos == 0)
                {
                    if(aIterator->mProperties->mReadRecordCount != 1)
                    {
                        aIterator->mProperties->mReadRecordCount--;
                    
                        *(++aIterator->highFence) = *sSlot;
                    }
                    else 
                    {
                        IDE_ASSERT(aIterator->mProperties->mReadRecordCount != 0);
                        aIterator->mProperties->mReadRecordCount--;
                        
                        aIterator->highest  = ID_TRUE;
                        *(++aIterator->highFence) = *sSlot;
                        break;
                    }
                }
                else
                {
                    aIterator->mProperties->mFirstReadRecordPos--;
                }
            }
        }
    }
    
    return IDE_SUCCESS;
   
    IDE_EXCEPTION_END;
     
    if( sLocked == ID_TRUE )
    {
        IDE_ASSERT( smmManager::releasePageLatch(aIterator->table->mSpaceID,
                                                 aIterator->page)
                    == IDE_SUCCESS );
    }
    
    return IDE_FAILURE;
    
}                                                                             

IDE_RC smnnSeq::afterLast( smnnIterator* aIterator,
                           const smSeekFunc** )
{

    smpPageListEntry* sFixed;
    vULong            sSize;
#ifdef DEBUG
    smTID             sTransID;
#endif
    idBool            sLocked = ID_FALSE;
    SChar*            sRowPtr;
    SChar**           sSlot;
    SChar**           sLowFence;
    SChar*            sFence;
    smpSlotHeader*    sRow;
    scGRID            sRowGRID;
    idBool            sResult;
    smxTrans        * sTrans               = (smxTrans*)aIterator->trans;
    idBool            sCanReusableRollback = ID_TRUE;
    idBool            sReusableCheckAgain  = ID_FALSE;
    idBool            sIsVisible;

    sFixed                     = (smpPageListEntry*)&(aIterator->table->mFixed);
    aIterator->curRecPtr       = NULL;
    aIterator->lstFetchRecPtr  = NULL;
    aIterator->lowFence        = aIterator->rows + SMNN_ROWS_CACHE;
    aIterator->slot            = aIterator->lowFence;
    aIterator->highFence       = aIterator->lowFence - 1;
    aIterator->page            = SM_NULL_PID;
    aIterator->least           = ID_TRUE;
    aIterator->highest         = ID_TRUE;
    sSize                      = sFixed->mSlotSize;
#ifdef DEBUG
    sTransID                   = aIterator->tid;
#endif
    /* __CRASH_TOLERANCE Property�� ���� ������, Tablespace�� FLI�� �̿���
     * ��ȸ�ϴ� ������� Page�� ������ */
    if( smuProperty::getCrashTolerance() == 0 )
    {
        IDE_TEST( movePrev( aIterator ) != IDE_SUCCESS );
    }
    else
    {
        IDE_TEST( movePrevUsingFLI( aIterator )!= IDE_SUCCESS );
    }
    
    if( aIterator->page != SM_NULL_PID && aIterator->mProperties->mReadRecordCount > 0)
    {
        /* BUG-39423 : trace log ��� �� ���� ���� �� ���� �۵� ��� ��� ���� */
#ifdef DEBUG
        if(   ( aIterator->page < smpManager::getFirstScanPageID(sFixed) )
            ||( aIterator->page > smpManager::getLastScanPageID(sFixed)  ) )
        {
            /* Scan Page List�� �������� ���� page�� ��� */
            if( aIterator->mModifySeqForScan == 0 )
            {
                ideLog::log(IDE_SM_0,
                            SM_TRC_MINDEX_INDEX_INFO,
                            sTransID,
                            __FILE__,
                            __LINE__,
                            aIterator->page,
                            smpManager::getFirstScanPageID(sFixed),
                            smpManager::getLastScanPageID(sFixed));
            }
        }      
#endif

    
        IDE_TEST( smmManager::holdPageSLatch(aIterator->table->mSpaceID,
                                             aIterator->page) != IDE_SUCCESS );
        sLocked = ID_TRUE;
        
        IDE_ASSERT( smmManager::getOIDPtr( 
                          aIterator->table->mSpaceID, 
                          SM_MAKE_OID( aIterator->page, 
                                       SMP_PERS_PAGE_BODY_OFFSET ),
                          (void**)&sFence )
            == IDE_SUCCESS );
       
        /* BUG-48353
           checkSCN() ���� Pending Wait �� �߻��Ҽ��ֱ� ������
           PAGE release ���ķ� checkSCN()�� �̷��.
           ���⼭�� row pointer ��θ� ĳ���� �����Ѵ�. */
        for( sLowFence  = aIterator->lowFence,
             sRowPtr    = sFence + ( sFixed->mSlotCount - 1 ) * sSize;
             sRowPtr   >= sFence;
             sRowPtr   -= sSize )
        {
            *(--sLowFence) = sRowPtr;
        }

        sLocked = ID_FALSE;
        IDE_TEST( smmManager::releasePageLatch(aIterator->table->mSpaceID,
                                               aIterator->page)
                  != IDE_SUCCESS );

        IDE_TEST(iduCheckSessionEvent(aIterator->mProperties->mStatistics) != IDE_SUCCESS);

        for( sSlot  = aIterator->slot - 1;
             sSlot >= sLowFence;
             sSlot-- )
        {
            sRow = (smpSlotHeader *)(*sSlot);

            IDE_TEST( smnManager::checkSCN( (smiIterator *)aIterator,
                                            sRow,
                                            &sCanReusableRollback,
                                            &sIsVisible,
                                            ID_TRUE )
                      != IDE_SUCCESS );

            if ( sIsVisible == ID_FALSE )
            {
                continue;
            }

            aIterator->mScanBackPID = aIterator->page;
            aIterator->mScanBackModifySeq = aIterator->mModifySeqForScan;

            if ( sCanReusableRollback == ID_FALSE ) 
            {
                sReusableCheckAgain = ID_TRUE;
            }

            SC_MAKE_GRID( sRowGRID,
                          aIterator->table->mSpaceID,
                          SMP_SLOT_GET_PID( sRow ),
                          SMP_SLOT_GET_OFFSET( sRow ) );

            IDE_TEST( aIterator->filter->callback( &sResult,
                                                   *sSlot,
                                                   NULL,
                                                   0,
                                                   sRowGRID,
                                                   aIterator->filter->data )
                      != IDE_SUCCESS );
            
            if( sResult == ID_TRUE )
            {
                /* SCN ������ ����� slot�� �ϳ��� cursor reusable üũ�� �ʿ��� ���
                 * filter ������ ����� slot�� ������� �ٽ� Ȯ���ؾ� �Ѵ�. */
                if( ( sReusableCheckAgain == ID_TRUE ) && ( sTrans->mIsReusableRollback == ID_TRUE ) )
                {
                    if( smnManager::checkCanReusableRollback( ( smiIterator* )aIterator, 
                                                              *sSlot ) == ID_FALSE )
                    {
                        sTrans->mIsReusableRollback = ID_FALSE;  
                    }
                }

                if(aIterator->mProperties->mFirstReadRecordPos == 0)
                {
                    if(aIterator->mProperties->mReadRecordCount != 1)
                    {
                        aIterator->mProperties->mReadRecordCount--;
                        *(--aIterator->lowFence) = *sSlot;
                    }
                    else 
                    {
                        IDE_ASSERT(aIterator->mProperties->mReadRecordCount != 0);
                        aIterator->mProperties->mReadRecordCount--;
                        
                        aIterator->least  = ID_TRUE;
                        *(--aIterator->lowFence) = *sSlot;
                        break;
                    }
                }
                else
                {
                    aIterator->mProperties->mFirstReadRecordPos--;
                }
            }
        }
    }

    return IDE_SUCCESS;
    
    IDE_EXCEPTION_END;
    
    if( sLocked == ID_TRUE )
    {
        IDE_ASSERT( smmManager::releasePageLatch(aIterator->table->mSpaceID,
                                                 aIterator->page)
                    == IDE_SUCCESS );
    }
    
    return IDE_FAILURE;

}                                                                             

IDE_RC smnnSeq::beforeFirstU( smnnIterator*       aIterator,
                              const smSeekFunc** aSeekFunc )
{

    IDE_TEST( beforeFirst( aIterator, aSeekFunc ) != IDE_SUCCESS );
    
    *aSeekFunc += 6;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
    
}                                                                             

IDE_RC smnnSeq::afterLastU( smnnIterator*       aIterator,
                            const smSeekFunc** aSeekFunc )
{
    
    IDE_TEST( afterLast( aIterator, aSeekFunc ) != IDE_SUCCESS );
    
    *aSeekFunc += 6;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
    
}                                                                             

IDE_RC smnnSeq::beforeFirstR( smnnIterator*       aIterator,
                              const smSeekFunc** aSeekFunc )
{

    smnnIterator sIterator;
    UInt         sRowCount;
    
    IDE_TEST( beforeFirst( aIterator, aSeekFunc ) != IDE_SUCCESS );
    
    sIterator.curRecPtr           = aIterator->curRecPtr;
    sIterator.lstFetchRecPtr      = aIterator->lstFetchRecPtr;
    sIterator.least               = aIterator->least;
    sIterator.highest             = aIterator->highest;
    sIterator.page                = aIterator->page;
    sIterator.mScanBackPID        = aIterator->mScanBackPID;
    sIterator.mScanBackModifySeq  = aIterator->mScanBackModifySeq;
    sIterator.mModifySeqForScan   = aIterator->mModifySeqForScan;
    sIterator.slot                = aIterator->slot;
    sIterator.lowFence            = aIterator->lowFence;
    sIterator.highFence           = aIterator->highFence;
    sRowCount                     = aIterator->highFence - aIterator->rows + 1;

    /* BUG-27516 [5.3.3 release] Klocwork SM (5) 
     * sRowCount�� SMNN_ROWS_CACHE���� ū ���� �־�� �ȵ˴ϴ�.
     * SMNN_ROWS_CACHE�� SM_PAGE_SIZE / ID_SIZEOF( smpSlotHeader)�� �� ������ ����
     * �� �� �ִ� ������ �ִ� �����̱� �����Դϴ�. */
    IDE_ASSERT( sRowCount < SMNN_ROWS_CACHE ); 
    idlOS::memcpy( sIterator.rows, aIterator->rows, (size_t)ID_SIZEOF(SChar*) * sRowCount );
           
    IDE_TEST( fetchNextR( aIterator ) != IDE_SUCCESS );
    
    aIterator->curRecPtr          = sIterator.curRecPtr;
    aIterator->lstFetchRecPtr     = sIterator.lstFetchRecPtr;
    aIterator->least              = sIterator.least;
    aIterator->highest            = sIterator.highest;
    aIterator->page               = sIterator.page;
    aIterator->mScanBackPID       = sIterator.mScanBackPID;
    aIterator->mScanBackModifySeq = sIterator.mScanBackModifySeq;
    aIterator->mModifySeqForScan  = sIterator.mModifySeqForScan;
    aIterator->slot               = sIterator.slot;
    aIterator->lowFence           = sIterator.lowFence;
    aIterator->highFence          = sIterator.highFence;
    idlOS::memcpy( aIterator->rows, sIterator.rows, (size_t)ID_SIZEOF(SChar*) * sRowCount );

    IDE_ASSERT(SMNN_ROWS_CACHE >= sRowCount);
    
    *aSeekFunc += 6;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
    
}                                                                             

IDE_RC smnnSeq::afterLastR( smnnIterator*       aIterator,
                            const smSeekFunc** aSeekFunc )
{
    
    IDE_TEST( beforeFirst( aIterator, aSeekFunc ) != IDE_SUCCESS );
    
    IDE_TEST( fetchNextR( aIterator ) != IDE_SUCCESS );

    *aSeekFunc += 6;

    return IDE_SUCCESS;
    
    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
    
}

/****************************************************************************
 *
 * BUG-25179 [SMM] Full Scan�� ���� �������� Scan List�� �ʿ��մϴ�.
 *
 * ���� Scan Page�κ��� ���� ���� Scan Page�� ����.
 *
 * Iterator�� ����Ű�� Page�� Cache�� ���Ŀ� �����ɼ� �ֱ� ������ Next Link��
 * ��ȿ���� ������ �ִ�. �̿� ���� ������ �ʿ��ϸ� �̴� Modify Sequence�� �̿��Ѵ�.
 * Cache�� ���Ŀ� Scan List���� ���ŵǾ��ٸ� Modify Sequence�� �����Ǳ� ������
 * �̸� Ȯ������ ��ȿ�� Next Link�� Ȯ���Ѵ�.
 *
 * ���� ���� Page�� Scan List�� �������� ������ �ֱ� ������ Scan Back Page��
 * �̵��� �ٽ� ������Ѵ�.
 *
 * Scan Back Page�� �Ѱ��̶� ��ȿ�� ���ڵ尡 �ִ� �������� �ǹ��Ѵ�. ��,
 * ����� Scan List���� ���ŵ��� �ʴ� �������� �ǹ��Ѵ�.
 *
 ****************************************************************************/

IDE_RC smnnSeq::moveNextBlock( smnnIterator * aIterator )
{
    smpPageListEntry     * sFixed;
    ULong                  sCurScanModifySeq = 0;
    ULong                  sNxtScanModifySeq = 0;
    scSpaceID              sSpaceID;
    scPageID               sNextPID = SM_NULL_PID;
    smpScanPageListEntry * sScanPageList;

    IDE_DASSERT( aIterator != NULL );
    
    sFixed  = (smpPageListEntry*)&(aIterator->table->mFixed);
    sScanPageList = &(sFixed->mRuntimeEntry->mScanPageList);
    sSpaceID = aIterator->table->mSpaceID;
    
    while( 1 )
    {
        IDE_TEST( sScanPageList->mMutex->lock( NULL ) != IDE_SUCCESS );

        /*
         * ������ Iterator���� QP�� ����� �ø����� ���� ���
         * 1. beforeFirst�� ȣ��� ���
         * 2. page�� NULL�� ���� ScanBackPID�κ��� ������ ���
         */
        if( aIterator->page == SM_NULL_PID )
        {
            aIterator->page = smpManager::getFirstScanPageID( sFixed );

            if( aIterator->page == SM_NULL_PID )
            {
                IDE_TEST( sScanPageList->mMutex->unlock() != IDE_SUCCESS );
                IDE_CONT( RETURN_SUCCESS );
            }

            sCurScanModifySeq = smpManager::getModifySeqForScan( sSpaceID,
                                                                 aIterator->page );
            aIterator->mModifySeqForScan = sNxtScanModifySeq = sCurScanModifySeq;

            sNextPID = smpManager::getNextScanPageID( sSpaceID,
                                                      aIterator->page );
        }
        else
        {
            sCurScanModifySeq = smpManager::getModifySeqForScan( sSpaceID,
                                                                 aIterator->page );
            aIterator->page = smpManager::getNextScanPageID( sSpaceID,
                                                             aIterator->page );
           
            /*
             * ���� Page�� Scan List���� ������ Page��� ���̻� Next��ũ��
             * ���� �ʿ䰡 ����. ����, ���� Page�� Scan List���� ���ŵ� ���¶��
             * ScanBackPID�κ��� Next Page�� �����Ѵ�.
             */
            if( (aIterator->page != SM_SPECIAL_PID) &&
                (aIterator->page != SM_NULL_PID) )
            {
                sNextPID = smpManager::getNextScanPageID( sSpaceID,
                                                          aIterator->page );

                sNxtScanModifySeq = smpManager::getModifySeqForScan( sSpaceID,
                                                                     aIterator->page );
            }
        }
        
        IDE_TEST( sScanPageList->mMutex->unlock() != IDE_SUCCESS );

        /*
         * ���� Page�� Scan List���� ���ŵǾ��ų�, ���ŵ� ���Ŀ� �ٽ� Scan List��
         * ���Ե� ����� �� ���¶�� ScanBackPID�κ��� Next Page�� �����Ѵ�.
         */
        if( sCurScanModifySeq != aIterator->mModifySeqForScan )
        {
            aIterator->page = aIterator->mScanBackPID;
            aIterator->mModifySeqForScan = aIterator->mScanBackModifySeq;
        }
        else
        {
            IDE_DASSERT( aIterator->page != SM_NULL_PID );
            aIterator->mModifySeqForScan = sNxtScanModifySeq;
            break;
        }
    }

    if ( ( aIterator->page != SM_SPECIAL_PID ) &&
         ( aIterator->page != SM_NULL_PID) )
    {
        // BUG-43463 Next Page ID�� ��ȯ ���� �ʰ�, Iterator�� �ٷ� �����Ѵ�
        aIterator->highest = ( SM_SPECIAL_PID == sNextPID ) ? ID_TRUE : ID_FALSE;
    }

    IDE_EXCEPTION_CONT( RETURN_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/****************************************************************************
 * BUG-43463 memory full scan page list mutex ���� ����
 *
 *     Fullscan�ÿ� iterator�� Next Page�� �ű�� �Լ�,
 *     smnnSeq::moveNextBlock() �Լ��� ������ ������ �Ѵ�,
 *     sScanPageList->mMutex �� ���� �ʵ��� �����Ǿ���.
 *
 * aIterator    - [IN/OUT]  Iterator
 *
 ****************************************************************************/
IDE_RC smnnSeq::moveNextNonBlock( smnnIterator * aIterator )
{
    smpPageListEntry     * sFixed;
    ULong                  sNxtScanModifySeq = 0;
    ULong                  sCurScanModifySeq = 0;
    scSpaceID              sSpaceID;
    scPageID               sCurrPID;
    scPageID               sNextPID;
    scPageID               sNextNextPID = SM_NULL_PID;
    smpScanPageListEntry * sScanPageList;

    IDE_DASSERT( aIterator != NULL );

    sFixed  = (smpPageListEntry*)&(aIterator->table->mFixed);
    sScanPageList = &(sFixed->mRuntimeEntry->mScanPageList);
    sSpaceID = aIterator->table->mSpaceID;

    while( 1 )
    {
        /*
         * ������ Iterator���� QP�� ����� �ø����� ���� ���
         * 1. beforeFirst�� ȣ��� ���
         * 2. page�� NULL�� ���� ScanBackPID�κ��� ������ ���
         */
        if( aIterator->page == SM_NULL_PID )
        {
            // ù��° page�� lock�� ��� �����´�.
            IDE_TEST( sScanPageList->mMutex->lock( NULL ) != IDE_SUCCESS );

            sNextPID = smpManager::getFirstScanPageID( sFixed );

            if( sNextPID == SM_NULL_PID )
            {
                IDE_TEST( sScanPageList->mMutex->unlock() != IDE_SUCCESS );
                IDE_CONT( RETURN_SUCCESS );
            }

            sNxtScanModifySeq = smpManager::getModifySeqForScan( sSpaceID,
                                                                 sNextPID );

            sNextNextPID = smpManager::getNextScanPageID( sSpaceID,
                                                          sNextPID );

            IDE_TEST( sScanPageList->mMutex->unlock() != IDE_SUCCESS );

            aIterator->mModifySeqForScan = sNxtScanModifySeq;
            aIterator->page              = sNextPID;

            break;
        }
        else
        {
            // Curr PID�� ������� Next PID�� �����ϰ�,
            // Next�� Next PID�� Ȯ���ؼ� Next page�� ���������� Ȯ���Ѵ�.

            sCurrPID = aIterator->page;

            sCurScanModifySeq = smpManager::getModifySeqForScan( sSpaceID,
                                                                 sCurrPID );

            // Curr Page�� List���� ���ŵǰų� ��ġ�� �ٲ����.
            // ScanBackPID���� �ٽ� �����Ѵ�.
            if( sCurScanModifySeq != aIterator->mModifySeqForScan )
            {
                // Curr Page�� �ٲ���ٸ� current page�� scan back page�� ���� ����.
                // �ݴ�� scanback pid�� curr pid�� ������ hang�� �߻��ϹǷ� �����Ѵ�.
                IDE_DASSERT( sCurrPID != aIterator->mScanBackPID );

                aIterator->page              = aIterator->mScanBackPID;
                aIterator->mModifySeqForScan = aIterator->mScanBackModifySeq;
                // Modify Seq�� Page �� link, unlink������ ���ŵȴ�.
                // Scan Back Page�� ager�� ���� �� ���� slot�� �����ϹǷ�
                // List���� ���� �� �� ��� Modify Seq�� ���ŵ��� �ʴ´�.
                continue;
            }

            // Ȧ���� ���� ���� ���� ����̴�.
            IDE_ASSERT( SMM_PAGE_IS_MODIFYING( sCurScanModifySeq ) == ID_FALSE );

            // Iterator�� �����Ѿ� �� Next Page
            sNextPID = smpManager::getNextScanPageID( sSpaceID,
                                                      sCurrPID );

            /*
             * ���� Page�� Scan List���� ������ Page��� ���̻� Next��ũ��
             * ���� �ʿ䰡 ����. ����, ���� Page�� Scan List���� ���ŵ� ���¶��
             * ScanBackPID�κ��� Next Page�� �����Ѵ�.
             */
            if( ( sNextPID != SM_SPECIAL_PID) &&
                ( sNextPID != SM_NULL_PID) )
            {
                // Next Modify Seq�� Iterator�� �����ϱ� ���� �ʿ��ϴ�.
                // NextNext PID�� NextPID�� ������ Page������ �Ǵ��ϴ� �뵵�� ���ȴ�.
                // NextNext PID�� ��Ȯ���� ���� Modify Seq�� ���� �д´�.
                // Link,Unlink�ÿ� ���� ��/�ڷ� Modify Seq�� ���ŵȴ�.
                sNxtScanModifySeq = smpManager::getModifySeqForScan( sSpaceID,
                                                                     sNextPID );
                sNextNextPID = smpManager::getNextScanPageID( sSpaceID,
                                                              sNextPID );

                // - Modify Seq�� Ȧ���̸� Link, Unlink�۾� ���̴�.
                // - Modify Seq�� ���� �Ǿ����� Link, Unlink�� �Ϸ� �Ǿ���ȴ�.
                // - Next PID�� ���� �Ǿ����� Modify Seq�� �б� ���� Curr Page�� ���� �� ���̶�
                //   Next Page ID �� Next Page�� Modify Seq�� ���� �� ����.
                if(( SMM_PAGE_IS_MODIFYING( sNxtScanModifySeq ) == ID_TRUE ) ||
                   ( smpManager::getNextScanPageID( sSpaceID,
                                                    sCurrPID ) != sNextPID ) ||
                   ( smpManager::getModifySeqForScan( sSpaceID,
                                                      sNextPID ) != sNxtScanModifySeq ))
                {
                    // curr page ���� �ٽ� �����Ѵ�.
                    continue;
                }
            }

            // Curr Modify Seq�� ���� �Ǿ��ٸ�, Curr Page�� Unlink�Ȱ��̴�.
            // Next PID�� Unlink�Ŀ� ��� �� ���� �� �����Ƿ� ���� �� ����.
            // Scan back page���� �ٽ� �����Ѵ�.
            if( smpManager::getModifySeqForScan( sSpaceID,
                                                 sCurrPID ) != sCurScanModifySeq )
            {
                // Curr Page�� �ٲ���ٸ� current page�� scan back page�� ���� ����.
                // �ݴ�� scanback pid�� curr pid�� ������ hang�� �߻��ϹǷ� �����Ѵ�.
                IDE_DASSERT( sCurrPID != aIterator->mScanBackPID );

                /* ���� Page�� Scan List���� ���ŵǾ��ų�, ���ŵ� ���Ŀ� �ٽ� Scan List��
                * ���Ե� ����� �� ���¶�� ScanBackPID�κ��� Next Page�� �����Ѵ�.
                */
                aIterator->page              = aIterator->mScanBackPID;
                aIterator->mModifySeqForScan = aIterator->mScanBackModifySeq;
            }
            else
            {
                IDE_ASSERT( sNextPID != SM_NULL_PID );

                aIterator->page              = sNextPID;
                aIterator->mModifySeqForScan = sNxtScanModifySeq;

                break;
            }
        }
    }

    if ( ( aIterator->page != SM_SPECIAL_PID ) &&
         ( aIterator->page != SM_NULL_PID) )
    {
        // NextNext PageID�� ������� Next page�� ������ page���� Ȯ���Ѵ�.
        // BUG-43463 Next Page ID�� ��ȯ ���� �ʰ�, Iterator�� �ٷ� �����Ѵ�
        aIterator->highest = ( SM_SPECIAL_PID == sNextNextPID ) ? ID_TRUE : ID_FALSE;
    }

    IDE_EXCEPTION_CONT( RETURN_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

// PROJ-2402
IDE_RC smnnSeq::moveNextParallelBlock( smnnIterator * aIterator )
{
    smpPageListEntry     * sFixed;
    ULong                  sCurScanModifySeq = 0;
    ULong                  sNxtScanModifySeq = 0;
    scSpaceID              sSpaceID;
    smpScanPageListEntry * sScanPageList;
    ULong                  sThreadCnt;
    ULong                  sThreadID;
    scPageID               sNextPID = SM_NULL_PID;

    IDE_ASSERT( aIterator != NULL );

    sFixed        = (smpPageListEntry*)&(aIterator->table->mFixed);
    sScanPageList = &(sFixed->mRuntimeEntry->mScanPageList);
    sSpaceID      = aIterator->table->mSpaceID;
    sThreadCnt    = aIterator->mProperties->mParallelReadProperties.mThreadCnt;
    sThreadID     = aIterator->mProperties->mParallelReadProperties.mThreadID;

    while ( 1 )
    {
        IDE_TEST( sScanPageList->mMutex->lock( NULL ) != IDE_SUCCESS );

        /*
         * ������ Iterator���� QP�� ����� �ø����� ���� ���
         * 1. beforeFirst�� ȣ��� ���
         * 2. page�� NULL�� ���� ScanBackPID�κ��� ������ ���
         */
        if ( aIterator->page == SM_NULL_PID )
        {
            aIterator->page = smpManager::getFirstScanPageID( sFixed );

            if ( aIterator->page == SM_NULL_PID )
            {
                IDE_TEST( sScanPageList->mMutex->unlock() != IDE_SUCCESS );
                IDE_CONT( RETURN_SUCCESS );
            }
            else
            {
                // nothing to do
            }

            sCurScanModifySeq = smpManager::getModifySeqForScan( sSpaceID,
                                                                 aIterator->page );
            aIterator->mModifySeqForScan = sNxtScanModifySeq = sCurScanModifySeq;

            sNextPID = smpManager::getNextScanPageID( sSpaceID,
                                                      aIterator->page );
        }
        else
        {
            sCurScanModifySeq = smpManager::getModifySeqForScan( sSpaceID,
                                                                 aIterator->page );
            aIterator->page = smpManager::getNextScanPageID( sSpaceID,
                                                             aIterator->page );

            /*
             * ���� Page�� Scan List���� ������ Page��� ���̻� Next��ũ��
             * ���� �ʿ䰡 ����. ����, ���� Page�� Scan List���� ���ŵ� ���¶��
             * ScanBackPID�κ��� Next Page�� �����Ѵ�.
             */
            if ( ( aIterator->page != SM_SPECIAL_PID ) &&
                 ( aIterator->page != SM_NULL_PID) )
            {
                sNextPID = smpManager::getNextScanPageID( sSpaceID,
                                                          aIterator->page );

                sNxtScanModifySeq = smpManager::getModifySeqForScan( sSpaceID,
                                                                     aIterator->page );
            }
            else
            {
                // nothing to do
            }
        }

        IDE_TEST( sScanPageList->mMutex->unlock() != IDE_SUCCESS );

        /*
         * ���� Page�� Scan List���� ���ŵǾ��ų�, ���ŵ� ���Ŀ� �ٽ� Scan List��
         * ���Ե� ����� �� ���¶�� ScanBackPID�κ��� Next Page�� �����Ѵ�.
         */
        if ( sCurScanModifySeq != aIterator->mModifySeqForScan )
        {
            aIterator->page              = aIterator->mScanBackPID;
            aIterator->mModifySeqForScan = aIterator->mScanBackModifySeq;
        }
        else
        {
            IDE_ASSERT( aIterator->page != SM_NULL_PID );
            aIterator->mModifySeqForScan = sNxtScanModifySeq;

            if ( ( aIterator->page != SM_SPECIAL_PID ) &&
                 ( aIterator->page != SM_NULL_PID) )
            {
                if ( ( aIterator->page % sThreadCnt + 1 ) != sThreadID )
                {
                    continue;
                }
                else
                {
                    // nothing to do
                }
            }
            else
            {
                // nothing to do
            }

            break;
        }
    }

    if ( ( aIterator->page != SM_SPECIAL_PID ) &&
         ( aIterator->page != SM_NULL_PID) )
    {
        // BUG-43463 Next Page ID�� ��ȯ ���� �ʰ�, Iterator�� �ٷ� �����Ѵ�
        aIterator->highest = ( SM_SPECIAL_PID == sNextPID ) ? ID_TRUE : ID_FALSE;
    }

    IDE_EXCEPTION_CONT( RETURN_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/****************************************************************************
 * BUG-43463 memory full scan page list mutex ���� ����
 *
 *     Parallel Fullscan�ÿ� iterator�� Next Page�� �ű�� �Լ�,
 *     smnnSeq::moveNextParallelBlock() �Լ��� ������ ������ �Ѵ�,
 *     sScanPageList->mMutex �� ���� �ʵ��� �����Ǿ���.
 *     ���� ������ smnnSeq::moveNextNonBlock() �ּ��� �����϶�
 *
 * aIterator    - [IN/OUT]  Iterator
 *
 ****************************************************************************/
IDE_RC smnnSeq::moveNextParallelNonBlock( smnnIterator * aIterator )
{
    smpPageListEntry     * sFixed;
    ULong                  sNxtScanModifySeq = 0;
    ULong                  sCurScanModifySeq = 0;
    scSpaceID              sSpaceID;
    smpScanPageListEntry * sScanPageList;
    ULong                  sThreadCnt;
    ULong                  sThreadID;
    scPageID               sNextNextPID = SM_NULL_PID;
    scPageID               sCurrPID;
    scPageID               sNextPID;

    IDE_ASSERT( aIterator != NULL );

    sFixed        = (smpPageListEntry*)&(aIterator->table->mFixed);
    sScanPageList = &(sFixed->mRuntimeEntry->mScanPageList);
    sSpaceID      = aIterator->table->mSpaceID;
    sThreadCnt    = aIterator->mProperties->mParallelReadProperties.mThreadCnt;
    sThreadID     = aIterator->mProperties->mParallelReadProperties.mThreadID;

    while ( 1 )
    {
        /*
         * ������ Iterator���� QP�� ����� �ø����� ���� ���
         * 1. beforeFirst�� ȣ��� ���
         * 2. page�� NULL�� ���� ScanBackPID�κ��� ������ ���
         */
        if ( aIterator->page == SM_NULL_PID )
        {
            IDE_TEST( sScanPageList->mMutex->lock( NULL ) != IDE_SUCCESS );

            sNextPID = smpManager::getFirstScanPageID( sFixed );

            if ( sNextPID == SM_NULL_PID )
            {
                IDE_TEST( sScanPageList->mMutex->unlock() != IDE_SUCCESS );
                IDE_CONT( RETURN_SUCCESS );
            }
            else
            {
                // nothing to do
            }
            IDE_ASSERT ( sNextPID != SM_SPECIAL_PID );

            sNxtScanModifySeq = smpManager::getModifySeqForScan( sSpaceID,
                                                                 sNextPID );

            sNextNextPID = smpManager::getNextScanPageID( sSpaceID,
                                                          sNextPID );

            IDE_TEST( sScanPageList->mMutex->unlock() != IDE_SUCCESS );

            aIterator->page              = sNextPID;
            aIterator->mModifySeqForScan = sNxtScanModifySeq;

            if ( ( sNextPID % sThreadCnt + 1 ) != sThreadID )
            {
                continue;
            }

            break;
        }
        else
        {
            sCurrPID = aIterator->page;
            sCurScanModifySeq = smpManager::getModifySeqForScan( sSpaceID,
                                                                 sCurrPID );

            if( sCurScanModifySeq != aIterator->mModifySeqForScan )
            {
                IDE_DASSERT( sCurrPID != aIterator->mScanBackPID );

                aIterator->page              = aIterator->mScanBackPID;
                aIterator->mModifySeqForScan = aIterator->mScanBackModifySeq;

                continue;
            }

            IDE_ASSERT( SMM_PAGE_IS_MODIFYING( sCurScanModifySeq ) == ID_FALSE );

            sNextPID = smpManager::getNextScanPageID( sSpaceID,
                                                      sCurrPID );
            /*
             * ���� Page�� Scan List���� ������ Page��� ���̻� Next��ũ��
             * ���� �ʿ䰡 ����. ����, ���� Page�� Scan List���� ���ŵ� ���¶��
             * ScanBackPID�κ��� Next Page�� �����Ѵ�.
             */
            if ( ( sNextPID != SM_SPECIAL_PID ) &&
                 ( sNextPID != SM_NULL_PID) )
            {
                sNxtScanModifySeq = smpManager::getModifySeqForScan( sSpaceID,
                                                                     sNextPID );

                sNextNextPID = smpManager::getNextScanPageID( sSpaceID,
                                                              sNextPID );

                if(( SMM_PAGE_IS_MODIFYING( sNxtScanModifySeq ) == ID_TRUE ) ||
                   ( smpManager::getNextScanPageID( sSpaceID,
                                                    sCurrPID ) != sNextPID ) ||
                   ( smpManager::getModifySeqForScan( sSpaceID,
                                                      sNextPID ) != sNxtScanModifySeq ))
                {
                    continue;
                }
            }
            else
            {
                // nothing to do
            }

            /*
             * ���� Page�� Scan List���� ���ŵǾ��ų�, ���ŵ� ���Ŀ� �ٽ� Scan List��
             * ���Ե� ����� �� ���¶�� ScanBackPID�κ��� Next Page�� �����Ѵ�.
             */

            if( smpManager::getModifySeqForScan( sSpaceID,
                                                 sCurrPID ) != sCurScanModifySeq )
            {
                IDE_DASSERT( sCurrPID != aIterator->mScanBackPID );

                aIterator->page              = aIterator->mScanBackPID;
                aIterator->mModifySeqForScan = aIterator->mScanBackModifySeq;
            }
            else
            {
                IDE_ASSERT( sNextPID != SM_NULL_PID );

                aIterator->page              = sNextPID;
                aIterator->mModifySeqForScan = sNxtScanModifySeq;

                if ( ( sNextPID != SM_SPECIAL_PID ) &&
                     ( sNextPID != SM_NULL_PID) )
                {
                    if ( ( sNextPID % sThreadCnt + 1 ) != sThreadID )
                    {
                        continue;
                    }
                }
                else
                {
                    // nothing to do
                }

                break;
            }
        }
    }

    if ( ( aIterator->page != SM_SPECIAL_PID ) &&
         ( aIterator->page != SM_NULL_PID) )
    {
        // NextNext PageID�� ������� Next page�� ������ page���� Ȯ���Ѵ�.
        // BUG-43463 Next Page ID�� ��ȯ ���� �ʰ�, Iterator�� �ٷ� �����Ѵ�
        aIterator->highest = ( SM_SPECIAL_PID == sNextNextPID ) ? ID_TRUE : ID_FALSE;
    }

    IDE_EXCEPTION_CONT( RETURN_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/****************************************************************************
 *
 * BUG-25179 [SMM] Full Scan�� ���� �������� Scan List�� �ʿ��մϴ�.
 *
 * ���� Scan Page�κ��� ���� ���� Scan Page�� ����.
 *
 * Iterator�� ����Ű�� Page�� Cache�� ���Ŀ� �����ɼ� �ֱ� ������ Previous Link��
 * ��ȿ���� ������ �ִ�. �̿� ���� ������ �ʿ��ϸ� �̴� Modify Sequence�� �̿��Ѵ�.
 * Cache�� ���Ŀ� Scan List���� ���ŵǾ��ٸ� Modify Sequence�� �����Ǳ� ������
 * �̸� Ȯ������ ��ȿ�� Previous Link�� Ȯ���Ѵ�.
 *
 * ���� ���� Page�� Scan List�� �������� ������ �ֱ� ������ Scan Back Page��
 * �̵��� �ٽ� ������Ѵ�.
 *
 * Scan Back Page�� �Ѱ��̶� ��ȿ�� ���ڵ尡 �ִ� �������� �ǹ��Ѵ�. ��,
 * ����� Scan List���� ���ŵ��� �ʴ� �������� �ǹ��Ѵ�.
 *
 ****************************************************************************/

IDE_RC smnnSeq::movePrevBlock( smnnIterator * aIterator )
{
    smpPageListEntry     * sFixed;
    ULong                  sCurScanModifySeq = 0;
    ULong                  sNxtScanModifySeq = 0;
    smpScanPageListEntry * sScanPageList;
    scSpaceID              sSpaceID;
    scPageID               sPrevPID = 0;

    IDE_DASSERT( aIterator != NULL );
    
    sFixed  = (smpPageListEntry*)&(aIterator->table->mFixed);
    sScanPageList = &(sFixed->mRuntimeEntry->mScanPageList);
    sSpaceID = aIterator->table->mSpaceID;
    
    while( 1 )
    {
        IDE_TEST( sScanPageList->mMutex->lock( NULL ) != IDE_SUCCESS );
        
        /*
         * ������ Iterator���� QP�� ����� �ø����� ���� ���
         * 1. beforeFirst�� ȣ��� ���
         * 2. page�� NULL�� ���� ScanBackPID�κ��� ������ ���
         */
        if( aIterator->page == SM_NULL_PID )
        {
            aIterator->page = smpManager::getLastScanPageID( sFixed );

            if( aIterator->page == SM_NULL_PID )
            {
                IDE_TEST( sScanPageList->mMutex->unlock() != IDE_SUCCESS );
                IDE_CONT( RETURN_SUCCESS );
            }
            
            sCurScanModifySeq = smpManager::getModifySeqForScan( sSpaceID,
                                                                 aIterator->page );
            aIterator->mModifySeqForScan = sNxtScanModifySeq = sCurScanModifySeq;

            sPrevPID = smpManager::getPrevScanPageID( sSpaceID,
                                                      aIterator->page );
        }
        else
        {
            sCurScanModifySeq = smpManager::getModifySeqForScan( sSpaceID,
                                                                 aIterator->page );
            aIterator->page = smpManager::getPrevScanPageID( sSpaceID,
                                                             aIterator->page );

            /*
             * ���� Page�� Scan List���� ó�� Page��� ���̻� Prev��ũ��
             * ���� �ʿ䰡 ����. ����, ���� Page�� Scan List���� ���ŵ� ���¶��
             * ScanBackPID�κ��� Previous Page�� �����Ѵ�.
             */
            if( (aIterator->page != SM_SPECIAL_PID) &&
                (aIterator->page != SM_NULL_PID) )
            {
                sPrevPID = smpManager::getPrevScanPageID( sSpaceID,
                                                          aIterator->page );
                sNxtScanModifySeq = smpManager::getModifySeqForScan( sSpaceID,
                                                                     aIterator->page );
            }
        }
        
        IDE_TEST( sScanPageList->mMutex->unlock() != IDE_SUCCESS );
        
        /*
         * ���� Page�� Scan List���� ���ŵǾ��ų�, ���ŵ� ���Ŀ� �ٽ� Scan List��
         * ���Ե� ����� �� ���¶�� ScanBackPID�κ��� Previous Page�� �����Ѵ�.
         */
        if( sCurScanModifySeq != aIterator->mModifySeqForScan )
        {
            aIterator->page = aIterator->mScanBackPID;
            aIterator->mModifySeqForScan = aIterator->mScanBackModifySeq;
        }
        else
        {
            aIterator->mModifySeqForScan = sNxtScanModifySeq;
            break;
        }
    }

    if ( ( aIterator->page != SM_SPECIAL_PID ) &&
         ( aIterator->page != SM_NULL_PID) )
    {
        // BUG-43463 Prev Page ID�� ��ȯ ���� �ʰ�, Iterator�� �ٷ� �����Ѵ�
        aIterator->least = ( SM_SPECIAL_PID == sPrevPID ) ? ID_TRUE : ID_FALSE ;
    }

    IDE_EXCEPTION_CONT( RETURN_SUCCESS );
    
    return IDE_SUCCESS;
    
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/****************************************************************************
 * BUG-43463 memory full scan page list mutex ���� ����
 *
 *     Fullscan�ÿ� iterator�� Prev Page�� �ű�� �Լ�,
 *     smnnSeq::movePrevBlock() �Լ��� ������ ������ �Ѵ�,
 *     sScanPageList->mMutex �� ���� �ʵ��� �����Ǿ���.
 *     ���� ������ smnnSeq::moveNextNonBlock() �ּ��� �����϶�
 *
 * aIterator    - [IN/OUT]  Iterator
 *
 ****************************************************************************/
IDE_RC smnnSeq::movePrevNonBlock( smnnIterator * aIterator )
{
    smpPageListEntry     * sFixed;
    ULong                  sPrvScanModifySeq = 0;
    ULong                  sCurScanModifySeq = 0;
    smpScanPageListEntry * sScanPageList;
    scSpaceID              sSpaceID;
    scPageID               sPrevPrevPID = SM_NULL_PID;
    scPageID               sPrevPID;
    scPageID               sCurrPID;

    IDE_DASSERT( aIterator != NULL );

    sFixed  = (smpPageListEntry*)&(aIterator->table->mFixed);
    sScanPageList = &(sFixed->mRuntimeEntry->mScanPageList);
    sSpaceID = aIterator->table->mSpaceID;

    while( 1 )
    {
        /*
         * ������ Iterator���� QP�� ����� �ø����� ���� ���
         * 1. beforeFirst�� ȣ��� ���
         * 2. page�� NULL�� ���� ScanBackPID�κ��� ������ ���
         */
        if( aIterator->page == SM_NULL_PID )
        {
            IDE_TEST( sScanPageList->mMutex->lock( NULL ) != IDE_SUCCESS );

            sPrevPID = smpManager::getLastScanPageID( sFixed );

            if( sPrevPID == SM_NULL_PID )
            {
                IDE_TEST( sScanPageList->mMutex->unlock() != IDE_SUCCESS );
                IDE_CONT( RETURN_SUCCESS );
            }

            sPrvScanModifySeq = smpManager::getModifySeqForScan( sSpaceID,
                                                                 sPrevPID );

            sPrevPrevPID = smpManager::getPrevScanPageID( sSpaceID,
                                                          sPrevPID );

            IDE_TEST( sScanPageList->mMutex->unlock() != IDE_SUCCESS );

            aIterator->mModifySeqForScan = sPrvScanModifySeq;
            aIterator->page              = sPrevPID;

            break;
        }
        else
        {
            sCurrPID = aIterator->page;

            sCurScanModifySeq = smpManager::getModifySeqForScan( sSpaceID,
                                                                 sCurrPID );

            if( sCurScanModifySeq != aIterator->mModifySeqForScan )
            {
                IDE_DASSERT( sCurrPID != aIterator->mScanBackPID );

                aIterator->page              = aIterator->mScanBackPID;
                aIterator->mModifySeqForScan = aIterator->mScanBackModifySeq;

                continue;
            }

            IDE_ASSERT( SMM_PAGE_IS_MODIFYING( sCurScanModifySeq ) == ID_FALSE );

            sPrevPID = smpManager::getPrevScanPageID( sSpaceID,
                                                      sCurrPID );
            /*
             * ���� Page�� Scan List���� ó�� Page��� ���̻� Prev��ũ��
             * ���� �ʿ䰡 ����. ����, ���� Page�� Scan List���� ���ŵ� ���¶��
             * ScanBackPID�κ��� Previous Page�� �����Ѵ�.
             */
            if( ( sPrevPID != SM_SPECIAL_PID ) &&
                ( sPrevPID != SM_NULL_PID ) )
            {
                sPrvScanModifySeq = smpManager::getModifySeqForScan( sSpaceID,
                                                                     sPrevPID );
                sPrevPrevPID = smpManager::getPrevScanPageID( sSpaceID,
                                                              sPrevPID );

                if(( SMM_PAGE_IS_MODIFYING( sPrvScanModifySeq ) == ID_TRUE ) ||
                   ( smpManager::getPrevScanPageID( sSpaceID,
                                                    sCurrPID ) != sPrevPID ) ||
                   ( smpManager::getModifySeqForScan( sSpaceID,
                                                      sPrevPID ) != sPrvScanModifySeq ))
                {
                    continue;
                }
            }

            if( smpManager::getModifySeqForScan( sSpaceID,
                                                 sCurrPID ) != sCurScanModifySeq )
            {
                IDE_DASSERT( sCurrPID != aIterator->mScanBackPID );

                /* ���� Page�� Scan List���� ���ŵǾ��ų�, ���ŵ� ���Ŀ� �ٽ� Scan List��
                * ���Ե� ����� �� ���¶�� ScanBackPID�κ��� Next Page�� �����Ѵ�.
                */
                aIterator->page              = aIterator->mScanBackPID;
                aIterator->mModifySeqForScan = aIterator->mScanBackModifySeq;
            }
            else
            {
                aIterator->page              = sPrevPID;
                aIterator->mModifySeqForScan = sPrvScanModifySeq;

                break;
            }
        }
    }

    if ( ( aIterator->page != SM_SPECIAL_PID ) &&
         ( aIterator->page != SM_NULL_PID) )
    {
        // PrevPrev PageID�� ������� prev page�� ������ page���� Ȯ���Ѵ�.
        // BUG-43463 Prev Page ID�� ��ȯ ���� �ʰ�, Iterator�� �ٷ� �����Ѵ�
        aIterator->least = ( SM_SPECIAL_PID == sPrevPrevPID ) ? ID_TRUE : ID_FALSE ;
    }

    IDE_EXCEPTION_CONT( RETURN_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/****************************************************************************
 *
 * PROJ-2162 RestartRiskReduction
 *
 * ��� ������ Refine���� ������ ��� MRDB�� PageList�� ������ �Ǹ�, �� ���
 * ��ȸ�� �Ұ����ϴ�.
 *
 * ���� �̸� �غ��ϱ� ���� Tablespace ��ü�� ��ȸ�ϰ�, PageHeader��
 * ��ϵ� TableOID�� �������� �ڽ��� Page���� Ȯ���ϴ� ������ Page�� ���ϴ�
 * smnnMoveNextUsingFLI�Լ��� �����Ѵ�.
 *
 ****************************************************************************/
IDE_RC smnnSeq::movePageUsingFLI( smnnIterator * aIterator,
                                  scPageID     * aNextPID )
{
    smpPageListEntry     * sFixed;
    scSpaceID              sSpaceID;
    scPageID               sPageID;
    scPageID               sMaxPID;
    smmTBSNode           * sTBSNode;
    smmPageState           sPageState;
    smpPersPageHeader    * sPersPageHeaderPtr;
    smOID                  sTableOID;

    sSpaceID  = aIterator->table->mSpaceID;
    sPageID   = aIterator->page;
    sFixed    = (smpPageListEntry*)&(aIterator->table->mFixed);
    sTableOID = sFixed->mTableOID;

    IDE_TEST( sctTableSpaceMgr::findSpaceNodeBySpaceID( sSpaceID,
                                                        (void**)&sTBSNode)
              != IDE_SUCCESS );
 
    if( aIterator->page == SM_NULL_PID )
    {
        /* ������ Ž���� ��� */
        aIterator->page = 1;
    }
    else
    {
        sPageID ++;
    }

    (*aNextPID) = SM_SPECIAL_PID;
    sMaxPID = sTBSNode->mMemBase->mAllocPersPageCount;
    while( sPageID < sMaxPID )
    {
        /* alloc page�� ã�������� Ž�� */
        IDE_TEST( smmExpandChunk::getPageState( sTBSNode, 
                                                sPageID, 
                                                &sPageState )
                  != IDE_SUCCESS );

        if( sPageState == SMM_PAGE_STATE_ALLOC )
        {
            IDE_TEST( smmManager::getPersPagePtr( sSpaceID,
                                                  sPageID,
                                                  (void**)&sPersPageHeaderPtr )
                      != IDE_SUCCESS );
            /* 1) TableOID�� ��ġ�ϰ�
             * 2) FixPage�̰� (VarPage�� ��� ����)
             * 3) Consistency�� �ְų�, Property�� ��� ������ */
            if( ( sPersPageHeaderPtr->mTableOID == sTableOID ) &&
                ( SMP_GET_PERS_PAGE_TYPE( sPersPageHeaderPtr ) == 
                        SMP_PAGETYPE_FIX ) &&
                ( ( SMP_GET_PERS_PAGE_INCONSISTENCY( sPersPageHeaderPtr ) ==
                    SMP_PAGEINCONSISTENCY_TRUE ) ||
                  ( smuProperty::getCrashTolerance() == 2 ) ) )
            {
                *aNextPID = sPageID;
                /* Found! */
                break;
            }
        }
        sPageID ++;
    } 

    if( sPageID == sMaxPID )
    {
        /* �������� ������ */
        sPageID = SM_SPECIAL_PID;
    }
    aIterator->page = sPageID;

    return IDE_SUCCESS;
    
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/* ������ ����Ȳ���� ������ ȹ���� ���� ����ϴ� ����̱� ������
 * FetchPrev�� FetchNext�� �����ϰ� �����Ѵ�. */
IDE_RC smnnSeq::movePrevUsingFLI( smnnIterator * aIterator )
{
    scPageID   sPrevPrevPID = SM_NULL_PID;

    IDE_TEST( movePageUsingFLI( aIterator, &sPrevPrevPID ) != IDE_SUCCESS );

    if ( ( aIterator->page != SM_SPECIAL_PID ) &&
         ( aIterator->page != SM_NULL_PID) )
    {
        aIterator->least = ( SM_SPECIAL_PID == sPrevPrevPID ) ? ID_TRUE : ID_FALSE ;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC smnnSeq::moveNextUsingFLI( smnnIterator * aIterator )
{
    scPageID   sNextNextPID = SM_NULL_PID;

    IDE_TEST( movePageUsingFLI( aIterator, &sNextNextPID ) != IDE_SUCCESS );

    if ( ( aIterator->page != SM_SPECIAL_PID ) &&
         ( aIterator->page != SM_NULL_PID) )
    {
        aIterator->highest = ( SM_SPECIAL_PID == sNextNextPID ) ? ID_TRUE : ID_FALSE;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


IDE_RC smnnSeq::fetchNext( smnnIterator* aIterator,
                           const void**  aRow )
{
    idBool sFound = ID_FALSE;

    do
    {
        aIterator->slot++;
        if( aIterator->slot <= aIterator->highFence )
        {
            aIterator->curRecPtr      = *aIterator->slot;
            aIterator->lstFetchRecPtr = aIterator->curRecPtr;
            *aRow                     = aIterator->curRecPtr;
            SC_MAKE_GRID( aIterator->mRowGRID,
                          aIterator->table->mSpaceID,
                          SMP_SLOT_GET_PID( (smpSlotHeader*)*aRow ),
                          SMP_SLOT_GET_OFFSET( (smpSlotHeader*)*aRow ) );

            break;
        }

        IDE_TEST( makeRowCache( aIterator, aRow, &sFound ) != IDE_SUCCESS );
    }
    while( sFound == ID_TRUE );
    
    return IDE_SUCCESS;
    
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC smnnSeq::makeRowCache ( smnnIterator      * aIterator,
                               const void       ** aRow,
                               idBool            * aFound )
{
    smpPageListEntry* sFixed;
    vULong            sSize;
    idBool            sLocked;
    SChar*            sRowPtr;
    SChar**           sSlot;
    SChar**           sHighFence;
    SChar*            sFence;
    smpSlotHeader*    sRow;
    scGRID            sRowGRID;
    idBool            sResult;
#ifdef DEBUG
    smTID             sTransID; 
#endif
    smxTrans        * sTrans               = (smxTrans*)aIterator->trans;
    idBool            sCanReusableRollback = ID_TRUE;
    idBool            sReusableCheckAgain  = ID_FALSE;
    idBool            sIsVisible;
    
    sLocked = ID_FALSE;
    *aFound = ID_FALSE;
#ifdef DEBUG
    sTransID = aIterator->tid;    
#endif
    sFixed  = (smpPageListEntry*)&(aIterator->table->mFixed);
    sSize   = sFixed->mSlotSize;
   
    if( aIterator->highest == ID_FALSE )
    {
        /* __CRASH_TOLERANCE Property�� ���� ������, Tablespace�� FLI�� �̿���
         * ��ȸ�ϴ� ������� Page�� ������ */
        if( smuProperty::getCrashTolerance() == 0 )
        {
            if ( aIterator->mProperties->mParallelReadProperties.mThreadCnt == 1 )
            {
                IDE_TEST( moveNext( aIterator ) != IDE_SUCCESS );
            }
            else
            {
                IDE_TEST( moveNextParallel( aIterator ) != IDE_SUCCESS );
            }
        }
        else
        {
            IDE_TEST( moveNextUsingFLI( aIterator ) != IDE_SUCCESS );
        }

        /*
         * page�� NULL�� ���� beforeFirst���� ����� ������ �߻��Ҽ� ������,
         * page�� SPECIAL�� ���� next page�� ������ �ǹ��Ѵ�.
         * �̷��� 2���� ���� ���̻� ���� ���ڵ尡 ������ �ǹ��Ѵ�.
         */
        IDE_TEST_CONT( (aIterator->page == SM_NULL_PID) ||
                       (aIterator->page == SM_SPECIAL_PID), RETURN_SUCCESS );

        aIterator->slot      = ( aIterator->lowFence = aIterator->rows ) - 1;
        aIterator->highFence = aIterator->slot;
        aIterator->least     = ID_FALSE;
        
        /* BUG-39423 : trace log ��� �� ���� ���� �� ���� �۵� ��� ��� ���� */
#ifdef DEBUG
        if(   ( aIterator->page < smpManager::getFirstScanPageID(sFixed) )
            ||( aIterator->page > smpManager::getLastScanPageID(sFixed)  ) )
        {
            /* Scan Page List�� �������� ���� page�� ��� */
            if( aIterator->mModifySeqForScan == 0 )
            {
                ideLog::log(IDE_SM_0,
                            SM_TRC_MINDEX_INDEX_INFO,
                            sTransID,
                            __FILE__,
                            __LINE__,
                            aIterator->page,
                            smpManager::getFirstScanPageID(sFixed),
                            smpManager::getLastScanPageID(sFixed));
            }
        } 
#endif

        IDE_TEST( smmManager::holdPageSLatch(aIterator->table->mSpaceID,
                                             aIterator->page) 
                  != IDE_SUCCESS );
        sLocked = ID_TRUE;
        
        IDE_ERROR( smmManager::getOIDPtr( aIterator->table->mSpaceID,
                                          SM_MAKE_OID( aIterator->page,
                                                       SMP_PERS_PAGE_BODY_OFFSET ),
                                          (void**)&sRowPtr )
                    == IDE_SUCCESS );

        /* BUG-48353
           checkSCN() ���� Pending Wait �� �߻��Ҽ��ֱ� ������
           PAGE release ���ķ� checkSCN()�� �̷��.
           ���⼭�� row pointer ��θ� ĳ���� �����Ѵ�. */
        for( sHighFence = aIterator->highFence,
             sFence     = sRowPtr + sFixed->mSlotCount * sSize;
             sRowPtr    < sFence;
             sRowPtr   += sSize )
        {
            *(++sHighFence) = sRowPtr;
        }

        sLocked = ID_FALSE;
        IDE_TEST( smmManager::releasePageLatch(aIterator->table->mSpaceID,
                                               aIterator->page)
                  != IDE_SUCCESS );
        
        IDE_TEST(iduCheckSessionEvent(aIterator->mProperties->mStatistics) 
                 != IDE_SUCCESS);
         IDU_FIT_POINT_RAISE( "BUG-48258@smnnSeq::makeRowCache", ERR_ABORT_INTERNAL );

        for( sSlot  = aIterator->slot + 1;
             sSlot <= sHighFence;
             sSlot++ )
        {
            sRow = (smpSlotHeader *)(*sSlot);

            IDE_TEST( smnManager::checkSCN( (smiIterator *)aIterator,
                                            sRow,
                                            &sCanReusableRollback,
                                            &sIsVisible,
                                            ID_TRUE )
                      != IDE_SUCCESS );

            if ( sIsVisible == ID_FALSE )
            {
                continue;
            }

            aIterator->mScanBackPID = aIterator->page;
            aIterator->mScanBackModifySeq = aIterator->mModifySeqForScan;

            if( sCanReusableRollback == ID_FALSE ) 
            {
                sReusableCheckAgain = ID_TRUE;
            }

            SC_MAKE_GRID( sRowGRID,
                          aIterator->table->mSpaceID,
                          SMP_SLOT_GET_PID( sRow ),
                          SMP_SLOT_GET_OFFSET( sRow ) );
            
            IDE_TEST( aIterator->filter->callback( &sResult,
                                                   *sSlot,
                                                   NULL,
                                                   0,
                                                   sRowGRID,
                                                   aIterator->filter->data )
                      != IDE_SUCCESS );

            if( sResult == ID_TRUE )
            {
                /* SCN ������ ����� slot�� �ϳ��� cursor reusable üũ�� �ʿ��� ���
                 * filter ������ ����� slot�� ������� �ٽ� Ȯ���ؾ� �Ѵ�. */
                if( ( sReusableCheckAgain == ID_TRUE ) && ( sTrans->mIsReusableRollback == ID_TRUE ) )
                {
                    if( smnManager::checkCanReusableRollback( ( smiIterator* )aIterator, 
                                                              *sSlot ) == ID_FALSE )
                    {
                        sTrans->mIsReusableRollback = ID_FALSE;  
                    }
                }

                if(aIterator->mProperties->mFirstReadRecordPos == 0)
                {
                    if(aIterator->mProperties->mReadRecordCount != 1)
                    {
                        aIterator->mProperties->mReadRecordCount--;
                    
                        *(++aIterator->highFence) = *sSlot;
                    }
                    else 
                    {
                        IDE_ASSERT(aIterator->mProperties->mReadRecordCount != 0);
                        aIterator->mProperties->mReadRecordCount--;

                        aIterator->highest  = ID_TRUE;
                        *(++aIterator->highFence) = *sSlot;
                        break;
                    }
                }
                else
                {
                    aIterator->mProperties->mFirstReadRecordPos--;
                }
            }
        }

        *aFound = ID_TRUE;
    }

    IDE_EXCEPTION_CONT( RETURN_SUCCESS );

    if( *aFound == ID_FALSE )
    {
        aIterator->slot           = aIterator->highFence + 1;
        aIterator->curRecPtr      = NULL;
        aIterator->lstFetchRecPtr = NULL;
        SC_MAKE_NULL_GRID( aIterator->mRowGRID );
        *aRow                     = NULL;
    }

    return IDE_SUCCESS;
#ifdef ALTIBASE_FIT_CHECK
    IDE_EXCEPTION( ERR_ABORT_INTERNAL );
    {
        IDE_SET( ideSetErrorCode( smERR_ABORT_INTERNAL ) );
    }
#endif 
    IDE_EXCEPTION_END;

    if( sLocked == ID_TRUE )
    {
        IDE_ASSERT( smmManager::releasePageLatch(aIterator->table->mSpaceID,
                                                 aIterator->page)
                    == IDE_SUCCESS );
    }

    return IDE_FAILURE;
}

IDE_RC smnnSeq::fetchPrev( smnnIterator* aIterator,
                           const void**  aRow )
{
    
    smpPageListEntry* sFixed;
    vULong            sSize;
    idBool            sLocked = ID_FALSE;
    SChar*            sRowPtr;
    SChar**           sSlot;
    SChar**           sLowFence;
    SChar*            sFence;
    smpSlotHeader*    sRow;
    scGRID            sRowGRID;
    idBool            sResult;
#ifdef DEBUG
    smTID             sTransID = aIterator->tid;    
#endif
    smxTrans        * sTrans               = (smxTrans*)aIterator->trans;
    idBool            sCanReusableRollback = ID_TRUE;
    idBool            sReusableCheckAgain  = ID_FALSE;
    idBool            sIsVisible;

restart:
    
    for( aIterator->slot--;
         aIterator->slot >= aIterator->lowFence;
         aIterator->slot-- )
    {
        aIterator->curRecPtr      = *aIterator->slot;
        aIterator->lstFetchRecPtr = aIterator->curRecPtr;
        *aRow          = aIterator->curRecPtr;
        SC_MAKE_GRID( aIterator->mRowGRID,
                      aIterator->table->mSpaceID,
                      SMP_SLOT_GET_PID( (smpSlotHeader*)*aRow ),
                      SMP_SLOT_GET_OFFSET( (smpSlotHeader*)*aRow ) );

        return IDE_SUCCESS;
    }

    sFixed    = (smpPageListEntry*)&(aIterator->table->mFixed);
    sSize     = sFixed->mSlotSize;
    
    if( aIterator->least == ID_FALSE )
    {
        /* __CRASH_TOLERANCE Property�� ���� ������, Tablespace�� FLI�� �̿���
         * ��ȸ�ϴ� ������� Page�� ������ */
        if( smuProperty::getCrashTolerance() == 0 )
        {
            IDE_TEST( movePrev( aIterator ) != IDE_SUCCESS );
        }
        else
        {
            IDE_TEST( movePrevUsingFLI( aIterator ) != IDE_SUCCESS );
        }

        /*
         * page�� NULL�� ���� beforeFirst���� ����� ������ �߻��Ҽ� ������,
         * page�� SPECIAL�� ���� previous page�� ������ �ǹ��Ѵ�.
         * �̷��� 2���� ���� ���̻� ���� ���ڵ尡 ������ �ǹ��Ѵ�.
         */
        IDE_TEST_CONT( (aIterator->page == SM_NULL_PID) ||
                       (aIterator->page == SM_SPECIAL_PID), RETURN_SUCCESS );
        
        aIterator->highFence =
            ( aIterator->slot = aIterator->lowFence = aIterator->rows + SMNN_ROWS_CACHE ) - 1;
        aIterator->highest = ID_TRUE;

        /* BUG-39423 : trace log ��� �� ���� ���� �� ���� �۵� ��� ��� ���� */
#ifdef DEBUG
        if(   ( aIterator->page < smpManager::getFirstScanPageID(sFixed) )
            ||( aIterator->page > smpManager::getLastScanPageID(sFixed)  ) )
        {
            /* Scan Page List�� �������� ���� page�� ��� */
            if( aIterator->mModifySeqForScan == 0 )
            {
                ideLog::log(IDE_SM_0,
                            SM_TRC_MINDEX_INDEX_INFO,
                            sTransID,
                            __FILE__,
                            __LINE__,
                            aIterator->page,
                            smpManager::getFirstScanPageID(sFixed),
                            smpManager::getLastScanPageID(sFixed));
            }
        }
#endif

        IDE_TEST( smmManager::holdPageSLatch(aIterator->table->mSpaceID,
                                             aIterator->page) != IDE_SUCCESS );
        sLocked = ID_TRUE;

        IDE_ASSERT( smmManager::getOIDPtr( 
                          aIterator->table->mSpaceID, 
                          SM_MAKE_OID( aIterator->page, 
                                       SMP_PERS_PAGE_BODY_OFFSET ),
                          (void**)&sFence )
            == IDE_SUCCESS );
       
        /* BUG-48353
           checkSCN() ���� Pending Wait �� �߻��Ҽ��ֱ� ������
           PAGE release ���ķ� checkSCN()�� �̷��.
           ���⼭�� row pointer ��θ� ĳ���� �����Ѵ�. */
        for( sLowFence = aIterator->lowFence,
             sRowPtr   = sFence + ( sFixed->mSlotCount - 1 ) * sSize;
             sRowPtr  >= sFence;
             sRowPtr  -= sSize )
        {
            *(--sLowFence) = sRowPtr;
        }

        sLocked = ID_FALSE;
        IDE_TEST( smmManager::releasePageLatch(aIterator->table->mSpaceID,
                                               aIterator->page)
                  != IDE_SUCCESS );

        IDE_TEST(iduCheckSessionEvent(aIterator->mProperties->mStatistics) != IDE_SUCCESS);

        for( sSlot  = aIterator->slot - 1;
             sSlot >= sLowFence;
             sSlot-- )
        {
            sRow = (smpSlotHeader *)(*sSlot);

            IDE_TEST( smnManager::checkSCN( (smiIterator *)aIterator,
                                            sRow,
                                            &sCanReusableRollback,
                                            &sIsVisible,
                                            ID_TRUE )
                      != IDE_SUCCESS );

            if ( sIsVisible == ID_FALSE )
            {
                continue;
            }

            aIterator->mScanBackPID = aIterator->page;
            aIterator->mScanBackModifySeq = aIterator->mModifySeqForScan;

            if( sCanReusableRollback == ID_FALSE ) 
            {
                sReusableCheckAgain = ID_TRUE;
            }

            SC_MAKE_GRID( sRowGRID,
                          aIterator->table->mSpaceID,
                          SMP_SLOT_GET_PID( sRow ),
                          SMP_SLOT_GET_OFFSET( sRow ) );
            
            IDE_TEST( aIterator->filter->callback( &sResult,
                                                   *sSlot,
                                                   NULL,
                                                   0,
                                                   sRowGRID,
                                                   aIterator->filter->data )
                      != IDE_SUCCESS );
            if( sResult == ID_TRUE )
            {
                /* SCN ������ ����� slot�� �ϳ��� cursor reusable üũ�� �ʿ��� ���
                 * filter ������ ����� slot�� ������� �ٽ� Ȯ���ؾ� �Ѵ�. */
                if( ( sReusableCheckAgain == ID_TRUE ) && ( sTrans->mIsReusableRollback == ID_TRUE ) )
                {
                    if( smnManager::checkCanReusableRollback( ( smiIterator* )aIterator, *sSlot )
                        == ID_FALSE )
                    {
                        sTrans->mIsReusableRollback = ID_FALSE;  
                    }
                }

                if(aIterator->mProperties->mFirstReadRecordPos == 0)
                {
                    if(aIterator->mProperties->mReadRecordCount != 1)
                    {
                        aIterator->mProperties->mReadRecordCount--;
                        *(--aIterator->lowFence) = *sSlot;
                    }
                    else 
                    {
                        IDE_ASSERT(aIterator->mProperties->mReadRecordCount != 0);
                        aIterator->mProperties->mReadRecordCount--;
                        
                        aIterator->least  = ID_TRUE;
                        *(--aIterator->lowFence) = *sSlot;
                        break;
                    }
                }
                else
                {
                    aIterator->mProperties->mFirstReadRecordPos--;
                }
            }
        }

        goto restart;
    }

    IDE_EXCEPTION_CONT( RETURN_SUCCESS );
    
    aIterator->slot           = aIterator->lowFence - 1;
    aIterator->curRecPtr      = NULL;
    aIterator->lstFetchRecPtr = NULL;
    SC_MAKE_NULL_GRID( aIterator->mRowGRID );
    *aRow                     = NULL;

    return IDE_SUCCESS;
    
    IDE_EXCEPTION_END;
    
    if( sLocked == ID_TRUE )
    {
        IDE_ASSERT( smmManager::releasePageLatch(aIterator->table->mSpaceID,
                                                 aIterator->page)
                    == IDE_SUCCESS );
    }
    
    return IDE_FAILURE;
    
}                                                                             

IDE_RC smnnSeq::fetchNextU( smnnIterator* aIterator,
                            const void**  aRow )
{
    
    smpPageListEntry* sFixed;
    vULong            sSize;
    idBool            sLocked = ID_FALSE;
    SChar*            sRowPtr;
    SChar**           sSlot;
    SChar**           sHighFence;
    SChar*            sFence;
    smpSlotHeader*    sRow;
    scGRID            sRowGRID;
    idBool            sResult;
#ifdef DEBUG  
    smTID             sTransID = aIterator->tid; 
#endif
    smxTrans        * sTrans               = (smxTrans*)aIterator->trans;
    idBool            sCanReusableRollback = ID_TRUE;
    idBool            sReusableCheckAgain  = ID_FALSE;
    idBool            sIsVisible;

restart:
    
    for( aIterator->slot++;
         aIterator->slot <= aIterator->highFence;
         aIterator->slot++ )
    {
        aIterator->curRecPtr      = *aIterator->slot;
        aIterator->lstFetchRecPtr = aIterator->curRecPtr;
        
        smnManager::updatedRow( (smiIterator*)aIterator );
        *aRow = aIterator->curRecPtr;
        SC_MAKE_GRID( aIterator->mRowGRID,
                      aIterator->table->mSpaceID,
                      SMP_SLOT_GET_PID( (smpSlotHeader*)*aRow ),
                      SMP_SLOT_GET_OFFSET( (smpSlotHeader*)*aRow ) );


        if( SM_SCN_IS_NOT_DELETED(((smpSlotHeader*)(*aRow))->mCreateSCN) )
        {
            return IDE_SUCCESS;
        }
    }

    sFixed    = (smpPageListEntry*)&(aIterator->table->mFixed);
    sSize     = sFixed->mSlotSize;
    
    if( aIterator->highest == ID_FALSE )
    {
        /* __CRASH_TOLERANCE Property�� ���� ������, Tablespace�� FLI�� �̿���
         * ��ȸ�ϴ� ������� Page�� ������ */
        if( smuProperty::getCrashTolerance() == 0 )
        {
            IDE_TEST( moveNext( aIterator ) != IDE_SUCCESS );
        }
        else
        {
            IDE_TEST( moveNextUsingFLI( aIterator ) != IDE_SUCCESS );
        }

        /*
         * page�� NULL�� ���� beforeFirst���� ����� ������ �߻��Ҽ� ������,
         * page�� SPECIAL�� ���� next page�� ������ �ǹ��Ѵ�.
         * �̷��� 2���� ���� ���̻� ���� ���ڵ尡 ������ �ǹ��Ѵ�.
         */
        IDE_TEST_CONT( (aIterator->page == SM_NULL_PID) ||
                       (aIterator->page == SM_SPECIAL_PID), RETURN_SUCCESS );
        
        aIterator->slot = aIterator->highFence = ( aIterator->lowFence = aIterator->rows ) - 1;
        aIterator->least = ID_FALSE;
   
        /* BUG-39423 : trace log ��� �� ���� ���� �� ���� �۵� ��� ��� ���� */
#ifdef DEBUG  
        if(   ( aIterator->page < smpManager::getFirstScanPageID(sFixed) )
            ||( aIterator->page > smpManager::getLastScanPageID(sFixed)  ) )
        {
            /* Scan Page List�� �������� ���� page�� ��� */
            if( aIterator->mModifySeqForScan == 0 )
            {
                ideLog::log(IDE_SM_0,
                            SM_TRC_MINDEX_INDEX_INFO,
                            sTransID,
                            __FILE__,
                            __LINE__,
                            aIterator->page,
                            smpManager::getFirstScanPageID(sFixed),
                            smpManager::getLastScanPageID(sFixed));
            }
        }
#endif

        IDE_TEST( smmManager::holdPageSLatch(aIterator->table->mSpaceID,
                                             aIterator->page) != IDE_SUCCESS );
        sLocked = ID_TRUE;
        
        IDE_ASSERT( smmManager::getOIDPtr( 
                          aIterator->table->mSpaceID, 
                          SM_MAKE_OID( aIterator->page, 
                                       SMP_PERS_PAGE_BODY_OFFSET ),
                          (void**)&sRowPtr )
            == IDE_SUCCESS );

        /* BUG-48353
           checkSCN() ���� Pending Wait �� �߻��Ҽ��ֱ� ������
           PAGE release ���ķ� checkSCN()�� �̷��.
           ���⼭�� row pointer ��θ� ĳ���� �����Ѵ�. */
        for( sHighFence = aIterator->highFence,
             sFence     = sRowPtr + sFixed->mSlotCount * sSize;
             sRowPtr    < sFence;
             sRowPtr   += sSize )
        {
            *(++sHighFence) = sRowPtr;
        }

        sLocked = ID_FALSE;
        IDE_TEST( smmManager::releasePageLatch(aIterator->table->mSpaceID,
                                               aIterator->page)
                  != IDE_SUCCESS );

        IDE_TEST(iduCheckSessionEvent(aIterator->mProperties->mStatistics) != IDE_SUCCESS);
        
        for( sSlot  = aIterator->slot + 1;
             sSlot <= sHighFence;
             sSlot++ )
        {
            sRow = (smpSlotHeader *)(*sSlot);

            IDE_TEST( smnManager::checkSCN( (smiIterator *)aIterator,
                                            sRow,
                                            &sCanReusableRollback,
                                            &sIsVisible,
                                            ID_TRUE )
                      != IDE_SUCCESS );

            if ( sIsVisible == ID_FALSE )
            {
                continue;
            }

            aIterator->mScanBackPID = aIterator->page;
            aIterator->mScanBackModifySeq = aIterator->mModifySeqForScan;

            if ( sCanReusableRollback == ID_FALSE ) 
            {
                sReusableCheckAgain = ID_TRUE;
            }

            SC_MAKE_GRID( sRowGRID,
                          aIterator->table->mSpaceID,
                          SMP_SLOT_GET_PID( sRow ),
                          SMP_SLOT_GET_OFFSET( sRow ) );
            
            IDE_TEST( aIterator->filter->callback( &sResult,
                                                   *sSlot,
                                                   NULL,
                                                   0,
                                                   sRowGRID,
                                                   aIterator->filter->data )
                      != IDE_SUCCESS );
            if( sResult == ID_TRUE )
            {
                /* SCN ������ ����� slot�� �ϳ��� cursor reusable üũ�� �ʿ��� ���
                 * filter ������ ����� slot�� ������� �ٽ� Ȯ���ؾ� �Ѵ�. */
                if( ( sReusableCheckAgain == ID_TRUE ) && ( sTrans->mIsReusableRollback == ID_TRUE ) )
                {
                    if( smnManager::checkCanReusableRollback( ( smiIterator* )aIterator, *sSlot )
                        == ID_FALSE )
                    {
                        sTrans->mIsReusableRollback = ID_FALSE;  
                    }
                }

                if(aIterator->mProperties->mFirstReadRecordPos == 0)
                {
                    if(aIterator->mProperties->mReadRecordCount != 1)
                    {
                        aIterator->mProperties->mReadRecordCount--;
                        *(++aIterator->highFence) = *sSlot;
                    }
                    else 
                    {
                        IDE_ASSERT(aIterator->mProperties->mReadRecordCount != 0);
                        aIterator->mProperties->mReadRecordCount--;
                        
                        aIterator->highest  = ID_TRUE;
                        *(++aIterator->highFence) = *sSlot;
                        break;
                    }
                }
                else
                {
                    aIterator->mProperties->mFirstReadRecordPos--;
                }
            }
        }

        goto restart;
    }
    
    IDE_EXCEPTION_CONT( RETURN_SUCCESS );
    
    aIterator->slot           = aIterator->highFence + 1;
    aIterator->curRecPtr      = NULL;
    aIterator->lstFetchRecPtr = NULL;
    SC_MAKE_NULL_GRID( aIterator->mRowGRID );
    *aRow           = NULL;
    
    return IDE_SUCCESS;
    
    IDE_EXCEPTION_END;
    
    if( sLocked == ID_TRUE )
    {
        IDE_ASSERT( smmManager::releasePageLatch(aIterator->table->mSpaceID,
                                                 aIterator->page)
                    == IDE_SUCCESS );
    }
    
    return IDE_FAILURE;
    
}

IDE_RC smnnSeq::fetchPrevU( smnnIterator* aIterator,
                            const void**  aRow )
{
    
    smpPageListEntry* sFixed;
    vULong            sSize;
    idBool            sLocked = ID_FALSE;
    SChar*            sRowPtr;
    SChar**           sSlot;
    SChar**           sLowFence;
    SChar*            sFence;
    smpSlotHeader*    sRow;
    scGRID            sRowGRID;
    idBool            sResult;
#ifdef DEBUG      
    smTID             sTransID = aIterator->tid;
#endif
    smxTrans        * sTrans               = (smxTrans*)aIterator->trans;
    idBool            sCanReusableRollback = ID_TRUE;
    idBool            sReusableCheckAgain  = ID_FALSE;
    idBool            sIsVisible;

restart:
    
    for( aIterator->slot--;
         aIterator->slot >= aIterator->lowFence;
         aIterator->slot-- )
    {
        aIterator->curRecPtr      = *aIterator->slot;
        aIterator->lstFetchRecPtr = aIterator->curRecPtr;
        
        smnManager::updatedRow( (smiIterator*)aIterator );
        *aRow = aIterator->curRecPtr;
        SC_MAKE_GRID( aIterator->mRowGRID,
                      aIterator->table->mSpaceID,
                      SMP_SLOT_GET_PID( (smpSlotHeader*)*aRow ),
                      SMP_SLOT_GET_OFFSET( (smpSlotHeader*)*aRow ) );


        if( SM_SCN_IS_NOT_DELETED(((smpSlotHeader*)(*aRow))->mCreateSCN) )
        {
            return IDE_SUCCESS;
        }
    }

    sFixed    = (smpPageListEntry*)&(aIterator->table->mFixed);
    sSize     = sFixed->mSlotSize;
    
    if( aIterator->least == ID_FALSE )
    {
        /* __CRASH_TOLERANCE Property�� ���� ������, Tablespace�� FLI�� �̿���
         * ��ȸ�ϴ� ������� Page�� ������ */
        if( smuProperty::getCrashTolerance() == 0 )
        {
            IDE_TEST( movePrev( aIterator ) != IDE_SUCCESS );
        }
        else
        {
            IDE_TEST( movePrevUsingFLI( aIterator ) != IDE_SUCCESS );
        }

        /*
         * page�� NULL�� ���� beforeFirst���� ����� ������ �߻��Ҽ� ������,
         * page�� SPECIAL�� ���� previous page�� ������ �ǹ��Ѵ�.
         * �̷��� 2���� ���� ���̻� ���� ���ڵ尡 ������ �ǹ��Ѵ�.
         */
        IDE_TEST_CONT( (aIterator->page == SM_NULL_PID) ||
                       (aIterator->page == SM_SPECIAL_PID), RETURN_SUCCESS );
        
        aIterator->highFence =
            ( aIterator->slot = aIterator->lowFence = aIterator->rows + SMNN_ROWS_CACHE ) - 1;
        aIterator->highest = ID_TRUE;

        /* BUG-39423 : trace log ��� �� ���� ���� �� ���� �۵� ��� ��� ���� */
#ifdef DEBUG      
        if(   ( aIterator->page < smpManager::getFirstScanPageID(sFixed) )
            ||( aIterator->page > smpManager::getLastScanPageID(sFixed)  ) )
        {
            /* Scan Page List�� �������� ���� page�� ��� */
            if( aIterator->mModifySeqForScan == 0 )
            {
                ideLog::log(IDE_SM_0,
                            SM_TRC_MINDEX_INDEX_INFO,
                            sTransID,
                            __FILE__,
                            __LINE__,
                            aIterator->page,
                            smpManager::getFirstScanPageID(sFixed),
                            smpManager::getLastScanPageID(sFixed));
            }
        }
#endif

        IDE_TEST( smmManager::holdPageSLatch(aIterator->table->mSpaceID,
                                             aIterator->page) != IDE_SUCCESS );
        sLocked = ID_TRUE;
        
        IDE_ASSERT( smmManager::getOIDPtr( 
                          aIterator->table->mSpaceID, 
                          SM_MAKE_OID( aIterator->page, 
                                       SMP_PERS_PAGE_BODY_OFFSET ),
                          (void**)&sFence )
            == IDE_SUCCESS );

        /* BUG-48353
           checkSCN() ���� Pending Wait �� �߻��Ҽ��ֱ� ������
           PAGE release ���ķ� checkSCN()�� �̷��.
           ���⼭�� row pointer ��θ� ĳ���� �����Ѵ�. */
        for( sLowFence = aIterator->lowFence,
             sRowPtr   = sFence + ( sFixed->mSlotCount - 1 ) * sSize;
             sRowPtr  >= sFence;
             sRowPtr  -= sSize )
        {
            *(--sLowFence) = sRowPtr;
        }

        sLocked = ID_FALSE;
        IDE_TEST( smmManager::releasePageLatch(aIterator->table->mSpaceID,
                                               aIterator->page)
                  != IDE_SUCCESS );
        
        IDE_TEST(iduCheckSessionEvent(aIterator->mProperties->mStatistics) != IDE_SUCCESS);
       
        for( sSlot  = aIterator->slot - 1;
             sSlot >= sLowFence;
             sSlot-- )
        {
            sRow = (smpSlotHeader *)(*sSlot);

            IDE_TEST( smnManager::checkSCN( (smiIterator *)aIterator,
                                            sRow,
                                            &sCanReusableRollback,
                                            &sIsVisible,
                                            ID_TRUE )
                      != IDE_SUCCESS );

            if ( sIsVisible == ID_FALSE )
            {
                continue;
            }

            aIterator->mScanBackPID = aIterator->page;
            aIterator->mScanBackModifySeq = aIterator->mModifySeqForScan;

            if( sCanReusableRollback == ID_FALSE ) 
            {
                sReusableCheckAgain = ID_TRUE;
            }

            SC_MAKE_GRID( sRowGRID,
                          aIterator->table->mSpaceID,
                          SMP_SLOT_GET_PID( sRow ),
                          SMP_SLOT_GET_OFFSET( sRow ) );
            
            IDE_TEST( aIterator->filter->callback( &sResult,
                                                   *sSlot,
                                                   NULL,
                                                   0,
                                                   sRowGRID,
                                                   aIterator->filter->data )
                      != IDE_SUCCESS );
            if( sResult == ID_TRUE )
            {
                /* SCN ������ ����� slot�� �ϳ��� cursor reusable üũ�� �ʿ��� ���
                 * filter ������ ����� slot�� ������� �ٽ� Ȯ���ؾ� �Ѵ�. */
                if( ( sReusableCheckAgain == ID_TRUE ) && ( sTrans->mIsReusableRollback == ID_TRUE ) )
                {
                    if( smnManager::checkCanReusableRollback( ( smiIterator* )aIterator, *sSlot )
                        == ID_FALSE )
                    {
                        sTrans->mIsReusableRollback = ID_FALSE;  
                    }
                }

                if(aIterator->mProperties->mFirstReadRecordPos == 0)
                {
                    if(aIterator->mProperties->mReadRecordCount != 1)
                    {
                        aIterator->mProperties->mReadRecordCount--;
                        *(--aIterator->lowFence) = *sSlot;
                    }
                    else 
                    {
                        IDE_ASSERT(aIterator->mProperties->mReadRecordCount != 0);
                        aIterator->mProperties->mReadRecordCount--;
                        
                        aIterator->least  = ID_TRUE;
                        *(--aIterator->lowFence) = *sSlot;
                        break;
                    }
                }
                else
                {
                    aIterator->mProperties->mFirstReadRecordPos--;
                }
            }
        }

        goto restart;
    }

    IDE_EXCEPTION_CONT( RETURN_SUCCESS );
    
    aIterator->slot           = aIterator->lowFence - 1;
    aIterator->curRecPtr      = NULL;
    aIterator->lstFetchRecPtr = NULL;
    SC_MAKE_NULL_GRID( aIterator->mRowGRID );
    *aRow           = NULL;

    return IDE_SUCCESS;
    
    IDE_EXCEPTION_END;
    
    if( sLocked == ID_TRUE )
    {
        IDE_ASSERT( smmManager::releasePageLatch(aIterator->table->mSpaceID,
                                                 aIterator->page)
                    == IDE_SUCCESS );
    }
    
    return IDE_FAILURE;
    
}                                                                             

IDE_RC smnnSeq::fetchNextR( smnnIterator* aIterator)
{
    
    smpPageListEntry* sFixed;
    vULong            sSize;
    idBool            sLocked = ID_FALSE;
    SChar*            sRowPtr;
    SChar**           sSlot;
    SChar**           sHighFence;
    SChar*            sFence;
    smpSlotHeader*    sRow;
    scGRID            sRowGRID;
    idBool            sResult;
#ifdef DEBUG
    smTID             sTransID = aIterator->tid;
#endif
    smxTrans        * sTrans               = (smxTrans*)aIterator->trans;
    idBool            sCanReusableRollback = ID_TRUE;
    idBool            sReusableCheckAgain  = ID_FALSE;
    idBool            sIsVisible;

    sFixed    = (smpPageListEntry*)&(aIterator->table->mFixed);
    sSize     = sFixed->mSlotSize;
    
restart:
    
    for( aIterator->slot++;
         aIterator->slot <= aIterator->highFence;
         aIterator->slot++ )
    {
        aIterator->curRecPtr      = *aIterator->slot;
        aIterator->lstFetchRecPtr = aIterator->curRecPtr;

        IDE_TEST( smnManager::lockRow( (smiIterator*)aIterator)
                  != IDE_SUCCESS );
    }

    if( aIterator->highest == ID_FALSE )
    {
        IDU_FIT_POINT("smnnSeq::fetchNextR::wait1");

        /* __CRASH_TOLERANCE Property�� ���� ������, Tablespace�� FLI�� �̿���
         * ��ȸ�ϴ� ������� Page�� ������ */
        if( smuProperty::getCrashTolerance() == 0 )
        {
            IDE_TEST( moveNext( aIterator ) != IDE_SUCCESS );
        }
        else
        {
            IDE_TEST( moveNextUsingFLI( aIterator ) != IDE_SUCCESS );
        }

        /*
         * page�� NULL�� ���� beforeFirst���� ����� ������ �߻��Ҽ� ������,
         * page�� SPECIAL�� ���� next page�� ������ �ǹ��Ѵ�.
         * �̷��� 2���� ���� ���̻� ���� ���ڵ尡 ������ �ǹ��Ѵ�.
         */
        IDE_TEST_CONT( (aIterator->page == SM_NULL_PID) ||
                       (aIterator->page == SM_SPECIAL_PID), RETURN_SUCCESS );

        aIterator->slot = aIterator->highFence = ( aIterator->lowFence = aIterator->rows ) - 1;
        aIterator->least = ID_FALSE;

        /* BUG-39423 : trace log ��� �� ���� ���� �� ���� �۵� ��� ��� ���� */
#ifdef DEBUG
        if(   ( aIterator->page < smpManager::getFirstScanPageID(sFixed) )
            ||( aIterator->page > smpManager::getLastScanPageID(sFixed)  ) )
        {
            /* Scan Page List�� �������� ���� page�� ��� */
            if( aIterator->mModifySeqForScan == 0 )
            {
                ideLog::log(IDE_SM_0,
                            SM_TRC_MINDEX_INDEX_INFO,
                            sTransID,
                            __FILE__,
                            __LINE__,
                            aIterator->page,
                            smpManager::getFirstScanPageID(sFixed),
                            smpManager::getLastScanPageID(sFixed));
            }
        }
#endif

        IDE_TEST( smmManager::holdPageSLatch(aIterator->table->mSpaceID,
                                             aIterator->page) != IDE_SUCCESS );
        sLocked = ID_TRUE;
        
        IDE_ASSERT( smmManager::getOIDPtr( 
                          aIterator->table->mSpaceID, 
                          SM_MAKE_OID( aIterator->page, 
                                       SMP_PERS_PAGE_BODY_OFFSET ),
                          (void**)&sRowPtr )
                    == IDE_SUCCESS );

        /* BUG-48353
           checkSCN() ���� Pending Wait �� �߻��Ҽ��ֱ� ������
           PAGE release ���ķ� checkSCN()�� �̷��.
           ���⼭�� row pointer ��θ� ĳ���� �����Ѵ�. */
        for( sHighFence = aIterator->highFence,
             sFence     = sRowPtr + sFixed->mSlotCount * sSize;
             sRowPtr    < sFence;
             sRowPtr   += sSize )
        {
            *(++sHighFence) = sRowPtr;
        }

        sLocked = ID_FALSE;
        IDE_TEST( smmManager::releasePageLatch(aIterator->table->mSpaceID,
                                               aIterator->page)
                  != IDE_SUCCESS );

        IDE_TEST(iduCheckSessionEvent(aIterator->mProperties->mStatistics) != IDE_SUCCESS);
        
        for( sSlot  = aIterator->slot + 1;
             sSlot <= sHighFence;
             sSlot++ )
        {
            sRow = (smpSlotHeader *)(*sSlot);

            IDE_TEST( smnManager::checkSCN( (smiIterator *)aIterator,
                                            sRow,
                                            &sCanReusableRollback,
                                            &sIsVisible,
                                            ID_TRUE )
                      != IDE_SUCCESS );

            if ( sIsVisible == ID_FALSE )
            {
                continue;
            }

            aIterator->mScanBackPID = aIterator->page;
            // BUG-30538 ScanBackModifySeq �� �߸� ����ϰ� �ֽ��ϴ�.
            aIterator->mScanBackModifySeq = aIterator->mModifySeqForScan;

            if( sCanReusableRollback == ID_FALSE ) 
            {
                sReusableCheckAgain = ID_TRUE;
            }

            SC_MAKE_GRID( sRowGRID,
                          aIterator->table->mSpaceID,
                          SMP_SLOT_GET_PID( sRow ),
                          SMP_SLOT_GET_OFFSET( sRow ) );
            
            IDE_TEST( aIterator->filter->callback( &sResult,
                                                   *sSlot,
                                                   NULL,
                                                   0,
                                                   sRowGRID,
                                                   aIterator->filter->data )
                      != IDE_SUCCESS );
            if( sResult == ID_TRUE )
            {
                /* SCN ������ ����� slot�� �ϳ��� cursor reusable üũ�� �ʿ��� ���
                 * filter ������ ����� slot�� ������� �ٽ� Ȯ���ؾ� �Ѵ�. */
                if( ( sReusableCheckAgain == ID_TRUE ) && ( sTrans->mIsReusableRollback == ID_TRUE ) )
                {
                    if( smnManager::checkCanReusableRollback( ( smiIterator* )aIterator, *sSlot )
                        == ID_FALSE )
                    {
                        sTrans->mIsReusableRollback = ID_FALSE;  
                    }
                }

                if(aIterator->mProperties->mFirstReadRecordPos == 0)
                {
                    if(aIterator->mProperties->mReadRecordCount != 1)
                    {
                        aIterator->mProperties->mReadRecordCount--;
                        *(++aIterator->highFence) = *sSlot;
                    }
                    else 
                    {
                        IDE_ASSERT(aIterator->mProperties->mReadRecordCount != 0);
                        aIterator->mProperties->mReadRecordCount--;
                        
                        aIterator->highest  = ID_TRUE;
                        *(++aIterator->highFence) = *sSlot;
                        break;
                    }
                }
                else
                {
                    aIterator->mProperties->mFirstReadRecordPos--;
                }
            }
        }

        goto restart;
    }
    
    IDE_EXCEPTION_CONT( RETURN_SUCCESS );
    
    aIterator->slot           = aIterator->highFence + 1;
    aIterator->curRecPtr      = NULL;
    aIterator->lstFetchRecPtr = NULL;
    
    return IDE_SUCCESS;
    
    IDE_EXCEPTION_END;
    
    if( sLocked == ID_TRUE )
    {
        IDE_ASSERT( smmManager::releasePageLatch(aIterator->table->mSpaceID,
                                                 aIterator->page)
                    == IDE_SUCCESS );
    }
    
    return IDE_FAILURE;
    
}


IDE_RC smnnSeq::allocIterator( void ** /* aIteratorMem */ )
{
    return IDE_SUCCESS;
}


IDE_RC smnnSeq::freeIterator( void * /* aIteratorMem */ )
{
    return IDE_SUCCESS;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : smnnGatherStat                             *
 * ------------------------------------------------------------------*
 * ��������� �����Ѵ�.
 * 
 * ScanPageList��� AllocPageList�� ����Ѵ�.
 * 1) AllocList�� ALTER ... COMPACT ������ �ٱ� ������, DanglingReference
 *    ������ ������ �ʴ´�.
 * 2) AllocList�� Record�� �ϳ��� ��� ��ȸ�ϱ� ������ ���ȹ�濡
 *    �����ϴ�.
 *
 * Statistics    - [IN]  IDLayer �������
 * Trans         - [IN]  �� �۾��� ��û�� Transaction
 * Percentage    - [IN]  Sampling Percentage
 * Degree        - [IN]  Parallel Degree
 * Header        - [IN]  ��� TableHeader
 * Index         - [IN]  ��� index (FullScan�̱⿡ ���õ� )
 *********************************************************************/
IDE_RC smnnSeq::gatherStat( idvSQL         * aStatistics,
                            void           * aTrans,
                            SFloat           aPercentage,
                            SInt             /* aDegree */,
                            smcTableHeader * aHeader,
                            void           * aTotalTableArg,
                            smnIndexHeader * /*aIndex*/,
                            void           * aStats,
                            idBool           aDynamicMode )
{
    void          * sTableArgument;
    UInt            sState = 0;

    IDE_TEST( smLayerCallback::beginTableStat( aHeader,
                                               aPercentage,
                                               aDynamicMode,
                                               &sTableArgument )
              != IDE_SUCCESS );
    sState = 1;

    IDE_TEST( gatherStat4Fixed( aStatistics,
                                sTableArgument,
                                aPercentage,
                                aHeader,
                                aTotalTableArg )
              != IDE_SUCCESS );

    IDE_TEST( gatherStat4Var( aStatistics,
                              sTableArgument,
                              aPercentage,
                              aHeader )
              != IDE_SUCCESS );

    IDE_TEST( smLayerCallback::setTableStat( aHeader,
                                             aTrans,
                                             sTableArgument,
                                             (smiAllStat*)aStats,
                                             aDynamicMode )
              != IDE_SUCCESS );

    sState = 0;
    IDE_TEST( smLayerCallback::endTableStat( aHeader,
                                             sTableArgument,
                                             aDynamicMode )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if( sState == 1 )
    {
        (void) smLayerCallback::endTableStat( aHeader,
                                              sTableArgument,
                                              aDynamicMode );
    }

    return IDE_FAILURE;
}


/*********************************************************************
 * FUNCTION DESCRIPTION : smnnGatherStat4Fixed                       *
 * ------------------------------------------------------------------*
 * FixedPage�� ���� ��������� �����Ѵ�.
 * 
 * aStatistics    - [IN]  IDLayer �������
 * aTableArgument - [IN]  �м� ����� �����صδ°�
 * Percentage     - [IN]  Sampling Percentage
 * Header         - [IN]  ��� TableHeader
 *********************************************************************/
IDE_RC smnnSeq::gatherStat4Fixed( idvSQL         * aStatistics,
                             void           * aTableArgument,
                             SFloat           aPercentage,
                             smcTableHeader * aHeader,
                             void           * aTotalTableArg )
{
    scPageID        sPageID = SM_NULL_PID;
    SChar         * sRow    = NULL;
    smpSlotHeader * sSlotHeader;
    UInt            sSlotSize;
    UInt            sMetaSpace   = 0;
    UInt            sUsedSpace   = 0;
    UInt            sAgableSpace = 0;
    UInt            sFreeSpace   = 0;
    UInt            sState = 0;
    UInt            i;

    sSlotSize = ((smpPageListEntry*)&(aHeader->mFixed))->mSlotSize;
    sPageID   = smpManager::getFirstAllocPageID( &(aHeader->mFixed.mMRDB) );
    while( sPageID != SM_NULL_PID )
    {
        /*Sampling Percentage�� P�� ������, �����Ǵ� �� C�� �ΰ� C�� P��
         * �������� 1�� �Ѿ�� ��쿡 Sampling �ϴ� ������ �Ѵ�.
         *
         * ��)
         * P=1, C=0
         * ù��° ������ C+=P  C=>0.25
         * �ι�° ������ C+=P  C=>0.50
         * ����° ������ C+=P  C=>0.75
         * �׹�° ������ C+=P  C=>1(Sampling!)  C--; C=>0
         * �ټ���° ������ C+=P  C=>0.25
         * ������° ������ C+=P  C=>0.50
         * �ϰ���° ������ C+=P  C=>0.75
         * ������° ������ C+=P  C=>1(Sampling!)  C--; C=>0 */
        if(  ( (UInt)( aPercentage * sPageID
                       + SMI_STAT_SAMPLING_INITVAL ) ) !=
             ( (UInt)( aPercentage * (sPageID+1)
                       + SMI_STAT_SAMPLING_INITVAL ) ) )
        {
            IDE_TEST( smmManager::holdPageSLatch( aHeader->mSpaceID, sPageID )
                      != IDE_SUCCESS );
            sState = 1;

            sMetaSpace   = SM_PAGE_SIZE 
                - ( aHeader->mFixed.mMRDB.mSlotCount * sSlotSize );
            sUsedSpace   = 0;
            sAgableSpace = 0;
            sFreeSpace   = 0;

            IDE_TEST( smmManager::getOIDPtr( 
                                aHeader->mSpaceID,
                                SM_MAKE_OID( sPageID, SMP_PERS_PAGE_BODY_OFFSET ),
                                (void**)&sRow )
                      != IDE_SUCCESS );

            for( i = 0 ; i < aHeader->mFixed.mMRDB.mSlotCount ; i ++ )
            {
                sSlotHeader = (smpSlotHeader*)sRow;

                if( ( SM_SCN_IS_DELETED( sSlotHeader->mCreateSCN ) == ID_TRUE ) ||
                    ( SM_SCN_IS_NULL_ROW( sSlotHeader->mCreateSCN ) ) )
                {
                    sFreeSpace += sSlotSize;
                }
                else
                {
                    if( SM_SCN_IS_FREE_ROW( sSlotHeader->mLimitSCN ) ||
                        SM_SCN_IS_LOCK_ROW( sSlotHeader->mLimitSCN ) )
                    {
                        /* Update/Delete�ȵǰ� Lock������ �ɸ�
                         * (���� �� Update/���Ե� Row ����) */
                        IDE_TEST( smLayerCallback::analyzeRow4Stat( aHeader,
                                                                    aTableArgument,
                                                                    aTotalTableArg,
                                                                    (UChar*)sRow )
                                  != IDE_SUCCESS );
                        sUsedSpace += sSlotSize;
                    }
                    else
                    {
                        sAgableSpace += sSlotSize;
                    }
                }

                sRow += sSlotSize;
            }


            IDE_ERROR( sMetaSpace   <= SM_PAGE_SIZE );
            IDE_ERROR( sUsedSpace   <= SM_PAGE_SIZE );
            IDE_ERROR( sAgableSpace <= SM_PAGE_SIZE );
            IDE_ERROR( sFreeSpace   <= SM_PAGE_SIZE );
            IDE_ERROR( ( sMetaSpace + sUsedSpace + sAgableSpace + sFreeSpace ) ==
                       SM_PAGE_SIZE );

            IDE_TEST( smLayerCallback::updateSpaceUsage( aTableArgument,
                                                         sMetaSpace,
                                                         sUsedSpace,
                                                         sAgableSpace,
                                                         sFreeSpace )
                      != IDE_SUCCESS );

            sState = 0;
            IDE_TEST( smmManager::releasePageLatch( aHeader->mSpaceID, 
                                                    sPageID )
                      != IDE_SUCCESS );
        }
        else
        {
            /* Skip�� */
        }

        sPageID = smpManager::getNextAllocPageID( aHeader->mSpaceID,
                                                  &(aHeader->mFixed.mMRDB),
                                                  sPageID );

        IDE_TEST( iduCheckSessionEvent( aStatistics )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if( sState == 1 )
    {
        (void) smmManager::releasePageLatch( aHeader->mSpaceID, sPageID );
    }

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : smnnGatherStat4Var                         *
 * ------------------------------------------------------------------*
 * VariablePage�� ���� ��������� �����Ѵ�.
 * 
 * aStatistics    - [IN]  IDLayer �������
 * aTableArgument - [IN]  �м� ����� �����صδ°�
 * Percentage     - [IN]  Sampling Percentage
 * Header         - [IN]  ��� TableHeader
 *********************************************************************/
IDE_RC smnnSeq::gatherStat4Var( idvSQL         * aStatistics,
                                void           * aTableArgument,
                                SFloat           aPercentage,
                                smcTableHeader * aHeader )
{
    scPageID          sPageID = SM_NULL_PID;
    smpPersPage     * sPage;
    UInt              sSlotSize;
    UInt              sMetaSpace   = 0;
    UInt              sUsedSpace   = 0;
    UInt              sAgableSpace = 0;
    UInt              sFreeSpace   = 0;
    UInt              sIdx = 0;
    smVCPieceHeader * sCurVCPieceHeader;
    UInt              sState = 0;
    UInt              i;

    sPageID = smpManager::getFirstAllocPageID( aHeader->mVar.mMRDB );
    while( sPageID != SM_NULL_PID )
    {
        /*Sampling Percentage�� P�� ������, �����Ǵ� �� C�� �ΰ� C�� P��
         * �������� 1�� �Ѿ�� ��쿡 Sampling �ϴ� ������ �Ѵ�.
         *
         * ��)
         * P=1, C=0
         * ù��° ������ C+=P  C=>0.25
         * �ι�° ������ C+=P  C=>0.50
         * ����° ������ C+=P  C=>0.75
         * �׹�° ������ C+=P  C=>1(Sampling!)  C--; C=>0
         * �ټ���° ������ C+=P  C=>0.25
         * ������° ������ C+=P  C=>0.50
         * �ϰ���° ������ C+=P  C=>0.75
         * ������° ������ C+=P  C=>1(Sampling!)  C--; C=>0 */
        if(  ( (UInt)( aPercentage * sPageID
                       + SMI_STAT_SAMPLING_INITVAL ) ) !=
             ( (UInt)( aPercentage * (sPageID+1)
                       + SMI_STAT_SAMPLING_INITVAL ) ) )
        {
            IDE_TEST( smmManager::holdPageSLatch( aHeader->mSpaceID, sPageID )
                      != IDE_SUCCESS );
            sState = 1;

            IDE_TEST( smmManager::getPersPagePtr( aHeader->mSpaceID,
                                                  sPageID,
                                                  (void**)&sPage )
                      != IDE_SUCCESS );

            sIdx      = smpVarPageList::getVarIdx(sPage);
            sSlotSize = aHeader->mVar.mMRDB[sIdx].mSlotSize;

            sMetaSpace   = SM_PAGE_SIZE 
                - aHeader->mVar.mMRDB[sIdx].mSlotCount * sSlotSize;
            sUsedSpace   = 0;
            sAgableSpace = 0;
            sFreeSpace   = 0;

            for( i = 0 ; i < aHeader->mVar.mMRDB[sIdx].mSlotCount ; i ++ )
            {
                sCurVCPieceHeader = (smVCPieceHeader*)
                    &sPage->mBody[ ID_SIZEOF(smpVarPageHeader) 
                                      + i * sSlotSize ];

                if((sCurVCPieceHeader->flag & SM_VCPIECE_FREE_MASK)
                   == SM_VCPIECE_FREE_OK)
                {
                    sFreeSpace += sSlotSize;
                }
                else
                {
                    sUsedSpace += sSlotSize;
                }
            }

            IDE_ERROR( sMetaSpace   <= SM_PAGE_SIZE );
            IDE_ERROR( sUsedSpace   <= SM_PAGE_SIZE );
            IDE_ERROR( sAgableSpace <= SM_PAGE_SIZE );
            IDE_ERROR( sFreeSpace   <= SM_PAGE_SIZE );
            IDE_ERROR( ( sMetaSpace + sUsedSpace + sAgableSpace + sFreeSpace ) ==
                       SM_PAGE_SIZE );

            IDE_TEST( smLayerCallback::updateSpaceUsage( aTableArgument,
                                                         sMetaSpace,
                                                         sUsedSpace,
                                                         sAgableSpace,
                                                         sFreeSpace )
                      != IDE_SUCCESS );

            sState = 0;
            IDE_TEST( smmManager::releasePageLatch( aHeader->mSpaceID, 
                                                    sPageID )
                      != IDE_SUCCESS );
        }
        else
        {
            /* Skip�� */
        }

        sPageID = smpManager::getNextAllocPageID( aHeader->mSpaceID,
                                                  aHeader->mVar.mMRDB,
                                                  sPageID );

        IDE_TEST( iduCheckSessionEvent( aStatistics )
                  != IDE_SUCCESS );

    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if( sState == 1 )
    {
        (void) smmManager::releasePageLatch( aHeader->mSpaceID, sPageID );
    }

    return IDE_FAILURE;
}


