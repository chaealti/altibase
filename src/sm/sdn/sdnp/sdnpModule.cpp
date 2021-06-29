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
 

/*******************************************************************************
 * $Id: sdnpModule.cpp 89495 2020-12-14 05:19:22Z emlee $
 ******************************************************************************/

#include <ide.h>
#include <idu.h>
#include <smi.h>
#include <sdr.h>
#include <sdp.h>
#include <smc.h>
#include <sdc.h>
#include <smm.h>
#include <smp.h>
#include <smx.h>

#include <smErrorCode.h>
#include <smnModules.h>
#include <smnManager.h>
#include <sdbMPRMgr.h>
#include <sdnpDef.h>
#include <sdn.h>
#include <sdnReq.h>

static IDE_RC sdnpPrepareIteratorMem( const smnIndexModule* );

static IDE_RC sdnpReleaseIteratorMem( const smnIndexModule* );

static IDE_RC sdnpInit( sdnpIterator          * aIterator,
                        void                  * aTrans,
                        smcTableHeader        * aTable,
                        void                  * /* aIndexHeader */,
                        void                  * /* aDumpObject */,
                        const smiRange        * aKeyRange,
                        const smiRange        * /* aKeyFilter */,
                        const smiCallBack     * aFilter,
                        UInt                    aFlag,
                        smSCN                   aSCN,
                        smSCN                   aInfinite,
                        idBool                  /* aUntouchable */,
                        smiCursorProperties   * aProperties,
                        const smSeekFunc     ** aSeekFunc,
                        smiStatement          * aStatement );

static IDE_RC sdnpDest( sdnpIterator  * aIterator );

static IDE_RC sdnpNA( void );
static IDE_RC sdnpAA( sdnpIterator   * aIterator,
                      const void    ** aRow );

static IDE_RC sdnpBeforeFirst( sdnpIterator       * aIterator,
                               const smSeekFunc  ** aSeekFunc );

static IDE_RC sdnpBeforeFirstW( sdnpIterator       * aIterator,
                                const smSeekFunc  ** aSeekFunc );

static IDE_RC sdnpBeforeFirstRR( sdnpIterator       * aIterator,
                                 const smSeekFunc  ** aSeekFunc );

static IDE_RC sdnpAfterLast( sdnpIterator       * aIterator,
                             const smSeekFunc  ** aSeekFunc );

static IDE_RC sdnpAfterLastW( sdnpIterator       * aIterator,
                              const smSeekFunc  ** aSeekFunc );

static IDE_RC sdnpAfterLastRR( sdnpIterator       * aIterator,
                               const smSeekFunc  ** aSeekFunc );

static IDE_RC sdnpFetchNext( sdnpIterator   * aIterator,
                             const void    ** aRow );

static IDE_RC sdnpLockAllRows4RR( sdnpIterator  * aIterator );

static IDE_RC sdnpLockRow4RR( sdnpIterator  * aIterator,
                              sdrMtx        * aMtx,
                              sdrSavePoint  * aSvp,
                              scGRID          aGRID,
                              UChar         * aRowPtr,
                              UChar         * aPagePtr );

static IDE_RC sdnpAllocIterator( void  ** aIteratorMem );

static IDE_RC sdnpFreeIterator( void  * aIteratorMem );

static IDE_RC sdnpLockRow( sdnpIterator  * aIterator );

static IDE_RC sdnpFetchAndCheckFilter( sdnpIterator * aIterator,
                                       UChar        * aSlot,
                                       scGRID         aRowGRID,
                                       UChar        * aDestBuf,
                                       idBool       * aResult,
                                       idBool       * aIsPageLatchReleased );

static IDE_RC sdnpValidateAndGetPageByGRID(
                                        idvSQL              * aStatistics,
                                        sdrMtx              * aMtx,
                                        sddTableSpaceNode   * aTBSNode,
                                        smcTableHeader      * aTableHdr,
                                        scGRID                aGRID,
                                        sdbLatchMode          aLatchMode,
                                        UChar              ** aRowPtr,
                                        UChar              ** aPagePtr,
                                        idBool              * aIsValidGRID );

smnIndexModule sdnpModule = {
    SMN_MAKE_INDEX_MODULE_ID( SMI_TABLE_DISK,
                              SMI_BUILTIN_GRID_INDEXTYPE_ID ),
    SMN_RANGE_ENABLE|SMN_DIMENSION_DISABLE,
    ID_SIZEOF( sdnpIterator ),
    (smnMemoryFunc) sdnpPrepareIteratorMem,
    (smnMemoryFunc) sdnpReleaseIteratorMem,
    (smnMemoryFunc) NULL,
    (smnMemoryFunc) NULL,
    (smnCreateFunc) NULL,
    (smnDropFunc) NULL,

    (smTableCursorLockRowFunc) sdnpLockRow,
    (smnDeleteFunc) NULL,
    (smnFreeFunc) NULL,
    (smnInsertRollbackFunc) NULL,
    (smnDeleteRollbackFunc) NULL,
    (smnAgingFunc) NULL,

    (smInitFunc) sdnpInit,
    (smDestFunc) sdnpDest,
    (smnFreeNodeListFunc) NULL,
    (smnGetPositionFunc) NULL,
    (smnSetPositionFunc) NULL,

    (smnAllocIteratorFunc) sdnpAllocIterator,
    (smnFreeIteratorFunc) sdnpFreeIterator,
    (smnReInitFunc) NULL,
    (smnInitMetaFunc) NULL,
    (smnMakeDiskImageFunc) NULL,
    (smnBuildIndexFunc) NULL,
    (smnGetSmoNoFunc) NULL,
    (smnMakeKeyFromRow) NULL,
    (smnMakeKeyFromSmiValue) NULL,
    (smnRebuildIndexColumn) NULL,
    (smnSetIndexConsistency) NULL,
    (smnGatherStat) NULL
};

static const smSeekFunc sdnpSeekFunctions[32][12] =
{ { /* SMI_TRAVERSE_FORWARD  SMI_PREVIOUS_DISABLE SMI_LOCK_READ            */
        (smSeekFunc)sdnpFetchNext,
        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpBeforeFirst,
        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpAA,
        (smSeekFunc)sdnpAA,

        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpAA,
        (smSeekFunc)sdnpAA
    },
    { /* SMI_TRAVERSE_FORWARD  SMI_PREVIOUS_DISABLE SMI_LOCK_WRITE           */
        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpBeforeFirstW,
        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpAA,
        (smSeekFunc)sdnpAA,

        (smSeekFunc)sdnpFetchNext,
        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpAA,
        (smSeekFunc)sdnpAA
    },
    { /* SMI_TRAVERSE_FORWARD  SMI_PREVIOUS_DISABLE SMI_LOCK_REPEATALBE      */
        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpBeforeFirstRR,
        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpAA,
        (smSeekFunc)sdnpAA,

        (smSeekFunc)sdnpFetchNext,
        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpBeforeFirst,
        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpAA,
        (smSeekFunc)sdnpAA
    },
    { /* SMI_TRAVERSE_FORWARD  SMI_PREVIOUS_DISABLE SMI_LOCK_TABLE_SHARED    */
        (smSeekFunc)sdnpFetchNext,
        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpBeforeFirst,
        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpAA,
        (smSeekFunc)sdnpAA,

        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpAA,
        (smSeekFunc)sdnpAA
    },
    { /* SMI_TRAVERSE_FORWARD  SMI_PREVIOUS_DISABLE SMI_LOCK_TABLE_EXCLUSIVE */
        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpBeforeFirstW,
        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpAA,
        (smSeekFunc)sdnpAA,

        (smSeekFunc)sdnpFetchNext,
        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpAA,
        (smSeekFunc)sdnpAA
    },
    { /* UNUSED                                                              */
        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpNA,

        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpNA
    },
    { /* UNUSED                                                              */
        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpNA,

        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpNA
    },
    { /* UNUSED                                                              */
        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpNA,

        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpNA
    },
    { /* SMI_TRAVERSE_FORWARD  SMI_PREVIOUS_ENABLE  SMI_LOCK_READ            */
        (smSeekFunc)sdnpFetchNext,
        (smSeekFunc)sdnpNA, // sdnpFetchPrev
        (smSeekFunc)sdnpBeforeFirst,
        (smSeekFunc)sdnpAfterLast,
        (smSeekFunc)sdnpAA,
        (smSeekFunc)sdnpAA,

        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpAA,
        (smSeekFunc)sdnpAA
    },
    { /* SMI_TRAVERSE_FORWARD  SMI_PREVIOUS_ENABLE  SMI_LOCK_WRITE           */
        (smSeekFunc)sdnpFetchNext,
        (smSeekFunc)sdnpNA, // sdnpFetchPrevW,
        (smSeekFunc)sdnpBeforeFirst,
        (smSeekFunc)sdnpAfterLast,
        (smSeekFunc)sdnpAA,
        (smSeekFunc)sdnpAA,

        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpAA,
        (smSeekFunc)sdnpAA
    },
    { /* SMI_TRAVERSE_FORWARD  SMI_PREVIOUS_ENABLE  SMI_LOCK_REPEATALBE      */
        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpBeforeFirstRR,
        (smSeekFunc)sdnpAfterLastRR,
        (smSeekFunc)sdnpAA,
        (smSeekFunc)sdnpAA,

        (smSeekFunc)sdnpFetchNext,
        (smSeekFunc)sdnpNA, // sdnpFetchPrev,
        (smSeekFunc)sdnpBeforeFirst,
        (smSeekFunc)sdnpAfterLast,
        (smSeekFunc)sdnpAA,
        (smSeekFunc)sdnpAA
    },
    { /* SMI_TRAVERSE_FORWARD  SMI_PREVIOUS_ENABLE  SMI_LOCK_TABLE_SHARED    */
        (smSeekFunc)sdnpFetchNext,
        (smSeekFunc)sdnpNA, // sdnpFetchPrev,
        (smSeekFunc)sdnpBeforeFirst,
        (smSeekFunc)sdnpAfterLast,
        (smSeekFunc)sdnpAA,
        (smSeekFunc)sdnpAA,

        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpAA,
        (smSeekFunc)sdnpAA
    },
    { /* SMI_TRAVERSE_FORWARD  SMI_PREVIOUS_ENABLE  SMI_LOCK_TABLE_EXCLUSIVE */
        (smSeekFunc)sdnpFetchNext,
        (smSeekFunc)sdnpNA, // sdnpFetchPrevU,
        (smSeekFunc)sdnpBeforeFirst,
        (smSeekFunc)sdnpAfterLast,
        (smSeekFunc)sdnpAA,
        (smSeekFunc)sdnpAA,

        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpAA,
        (smSeekFunc)sdnpAA
    },
    { /* UNUSED                                                              */
        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpNA,

        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpNA
    },
    { /* UNUSED                                                              */
        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpNA,

        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpNA
    },
    { /* UNUSED                                                              */
        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpNA,

        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpNA
    },
    { /* SMI_TRAVERSE_BACKWARD SMI_PREVIOUS_DISABLE SMI_LOCK_READ            */
        (smSeekFunc)sdnpNA, // sdnpFetchPrev,
        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpAfterLast,
        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpAA,
        (smSeekFunc)sdnpAA,

        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpAA,
        (smSeekFunc)sdnpAA
    },
    { /* SMI_TRAVERSE_BACKWARD SMI_PREVIOUS_DISABLE SMI_LOCK_WRITE           */
        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpAfterLastW,
        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpAA,
        (smSeekFunc)sdnpAA,

        (smSeekFunc)sdnpNA, // sdnpFetchPrev,
        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpAA,
        (smSeekFunc)sdnpAA
    },
    { /* SMI_TRAVERSE_BACKWARD SMI_PREVIOUS_DISABLE SMI_LOCK_REPEATABLE      */
        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpAfterLastRR,
        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpAA,
        (smSeekFunc)sdnpAA,

        (smSeekFunc)sdnpNA, // sdnpFetchPrev,
        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpAfterLast,
        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpAA,
        (smSeekFunc)sdnpAA
    },
    { /* SMI_TRAVERSE_BACKWARD SMI_PREVIOUS_DISABLE SMI_LOCK_TABLE_SHARED    */
        (smSeekFunc)sdnpNA, // sdnpFetchPrev,
        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpAfterLast,
        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpAA,
        (smSeekFunc)sdnpAA,

        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpAA,
        (smSeekFunc)sdnpAA
    },
    { /* SMI_TRAVERSE_BACKWARD SMI_PREVIOUS_DISABLE SMI_LOCK_TABLE_EXCLUSIVE */
        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpAfterLastW,
        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpAA,
        (smSeekFunc)sdnpAA,

        (smSeekFunc)sdnpNA, // sdnpFetchPrev,
        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpAA,
        (smSeekFunc)sdnpAA
    },
    { /* UNUSED                                                              */
        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpNA,

        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpNA
    },
    { /* UNUSED                                                              */
        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpNA,

        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpNA
    },
    { /* UNUSED                                                              */
        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpNA,

        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpNA
    },
    { /* SMI_TRAVERSE_BACKWARD SMI_PREVIOUS_ENABLE  SMI_LOCK_READ            */
        (smSeekFunc)sdnpNA, // sdnpFetchPrev,
        (smSeekFunc)sdnpFetchNext,
        (smSeekFunc)sdnpAfterLast,
        (smSeekFunc)sdnpBeforeFirst,
        (smSeekFunc)sdnpAA,
        (smSeekFunc)sdnpAA,

        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpAA,
        (smSeekFunc)sdnpAA      // BUGBUG
    },
    { /* SMI_TRAVERSE_BACKWARD SMI_PREVIOUS_ENABLE  SMI_LOCK_WRITE           */
        (smSeekFunc)sdnpNA, // sdnpFetchPrevU,
        (smSeekFunc)sdnpFetchNext,
        (smSeekFunc)sdnpAfterLast,
        (smSeekFunc)sdnpBeforeFirst,
        (smSeekFunc)sdnpAA,
        (smSeekFunc)sdnpAA,

        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpAA,
        (smSeekFunc)sdnpAA
    },
    { /* SMI_TRAVERSE_BACKWARD SMI_PREVIOUS_ENABLE  SMI_LOCK_REPEATABLE      */
        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpAfterLastRR,
        (smSeekFunc)sdnpBeforeFirstRR,
        (smSeekFunc)sdnpAA,
        (smSeekFunc)sdnpAA,

        (smSeekFunc)sdnpNA, // sdnpFetchPrev,
        (smSeekFunc)sdnpFetchNext,
        (smSeekFunc)sdnpAfterLast,
        (smSeekFunc)sdnpBeforeFirst,
        (smSeekFunc)sdnpAA,
        (smSeekFunc)sdnpAA
    },
    { /* SMI_TRAVERSE_BACKWARD SMI_PREVIOUS_ENABLE  SMI_LOCK_TABLE_SHARED    */
        (smSeekFunc)sdnpNA, // sdnpFetchPrev,
        (smSeekFunc)sdnpFetchNext,
        (smSeekFunc)sdnpAfterLast,
        (smSeekFunc)sdnpBeforeFirst,
        (smSeekFunc)sdnpAA,
        (smSeekFunc)sdnpAA,

        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpAA,
        (smSeekFunc)sdnpAA
    },
    { /* SMI_TRAVERSE_BACKWARD SMI_PREVIOUS_ENABLE  SMI_LOCK_TABLE_EXCLUSIVE */
        (smSeekFunc)sdnpNA, // sdnpFetchPrevU,
        (smSeekFunc)sdnpFetchNext,
        (smSeekFunc)sdnpAfterLast,
        (smSeekFunc)sdnpBeforeFirst,
        (smSeekFunc)sdnpAA,
        (smSeekFunc)sdnpAA,

        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpAA,
        (smSeekFunc)sdnpAA
    },
    { /* UNUSED                                                              */
        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpNA,

        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpNA
    },
    { /* UNUSED                                                              */
        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpNA,

        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpNA
    },
    { /* UNUSED                                                              */
        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpNA,

        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpNA,
        (smSeekFunc)sdnpNA
    }
};

static IDE_RC sdnpPrepareIteratorMem( const smnIndexModule* )
{
    return IDE_SUCCESS;
}

static IDE_RC sdnpReleaseIteratorMem(const smnIndexModule* )
{
    return IDE_SUCCESS;
}

static IDE_RC sdnpInit( sdnpIterator          * aIterator,
                        void                  * aTrans,
                        smcTableHeader        * aTable,
                        void                  * /* aIndexHeader */,
                        void                  * /* aDumpObject */,
                        const smiRange        * aRange,
                        const smiRange        * /* aKeyFilter */,
                        const smiCallBack     * aFilter,
                        UInt                    aFlag,
                        smSCN                   aSCN,
                        smSCN                   aInfinite,
                        idBool                  /* aUntouchable */,
                        smiCursorProperties   * aProperties,
                        const smSeekFunc     ** aSeekFunc, 
                        smiStatement          * aStatement )
{
    idvSQL            *sSQLStat;

    SM_SET_SCN( &aIterator->mSCN, &aSCN );
    SM_SET_SCN( &(aIterator->mInfinite), &aInfinite );
    aIterator->mTrans           = aTrans;
    aIterator->mTable           = aTable;
    aIterator->mCurRecPtr       = NULL;
    aIterator->mLstFetchRecPtr  = NULL;
    SC_MAKE_NULL_GRID( aIterator->mRowGRID );
    aIterator->mTid             = smLayerCallback::getTransID( aTrans );
    aIterator->mFlag            = aFlag;
    aIterator->mProperties      = aProperties;

    aIterator->mRange           = aRange;
    aIterator->mNxtRange        = NULL;
    aIterator->mFilter          = aFilter;
    aIterator->mStatement       = aStatement;

    IDE_TEST( sctTableSpaceMgr::findSpaceNodeBySpaceID(
                                        aTable->mSpaceID,
                                        (void**)&aIterator->mTBSNode )
              != IDE_SUCCESS );

    *aSeekFunc = sdnpSeekFunctions[ aFlag&( SMI_TRAVERSE_MASK |
                                            SMI_PREVIOUS_MASK |
                                            SMI_LOCK_MASK ) ];

    sSQLStat = aIterator->mProperties->mStatistics;
    IDV_SQL_ADD(sSQLStat, mDiskCursorGRIDScan, 1);

    if (sSQLStat != NULL)
    {
        IDV_SESS_ADD(sSQLStat->mSess,
                     IDV_STAT_INDEX_DISK_CURSOR_GRID_SCAN, 1);
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

static IDE_RC sdnpDest( sdnpIterator  * /*aIterator*/ )
{
    return IDE_SUCCESS;
}

static IDE_RC sdnpAA( sdnpIterator   * /*aIterator*/,
                      const void    ** /* */)
{
    return IDE_SUCCESS;
}

IDE_RC sdnpNA( void )
{
    IDE_SET( ideSetErrorCode( smERR_ABORT_smiTraverseNotApplicable ) );

    return IDE_FAILURE;
}

static IDE_RC sdnpBeforeFirst( sdnpIterator       * aIterator,
                               const smSeekFunc  ** /*aSeekFunc*/)
{
    SC_MAKE_NULL_GRID( aIterator->mRowGRID );

    aIterator->mNxtRange = aIterator->mRange;
    
    return IDE_SUCCESS;
}

/*********************************************************
  function description: sdnpBeforeFirstW
  sdnpBeforeFirst�� �����ϰ� smSeekFunction�� ��������
  �������� �ٸ� callback�� �Ҹ����� �Ѵ�.
***********************************************************/
static IDE_RC sdnpBeforeFirstW( sdnpIterator*       aIterator,
                                const smSeekFunc** aSeekFunc )
{
    IDE_TEST( sdnpBeforeFirst( aIterator, aSeekFunc ) != IDE_SUCCESS );

    *aSeekFunc += 6;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*********************************************************
  function description: sdnpBeforeFirstR

  - sdnpBeforeFirst�� �����ϰ�  lockAllRows4RR�� �ҷ���,
  filter�� ������Ű��  row�� ���Ͽ� lock�� ǥ���ϴ� undo record
  �� �����ϰ� rollptr�� �����Ų��.

  - sdnpBeforeFirst�� �ٽ� �����Ѵ�.

  �������� ȣ��� ��쿡��  sdnpBeforeFirstR�� �ƴ�
  �ٸ� �Լ��� �Ҹ����� smSeekFunction�� offset�� 6������Ų��.
***********************************************************/
static IDE_RC sdnpBeforeFirstRR( sdnpIterator *       aIterator,
                                 const smSeekFunc ** aSeekFunc )
{
    IDE_TEST( sdnpBeforeFirst( aIterator, aSeekFunc ) != IDE_SUCCESS );

    // select for update�� ���Ͽ� lock�� ��Ÿ���� undo record�� ����.
    IDE_TEST( sdnpLockAllRows4RR( aIterator ) != IDE_SUCCESS );

    IDE_TEST( sdnpBeforeFirst( aIterator, aSeekFunc ) != IDE_SUCCESS );

    *aSeekFunc += 6;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

static IDE_RC sdnpAfterLast( sdnpIterator *      /* aIterator */,
                             const smSeekFunc ** /* aSeekFunc */)
{
    return IDE_FAILURE;
}

static IDE_RC sdnpAfterLastW( sdnpIterator *       /* aIterator */,
                              const smSeekFunc **  /* aSeekFunc */ )
{
    return IDE_FAILURE;
}

static IDE_RC sdnpAfterLastRR( sdnpIterator *       /* aIterator */,
                               const smSeekFunc **  /* aSeekFunc */ )
{
    return IDE_FAILURE;
}

/*******************************************************************************
 * Description: ��� row�� fetch �� �� ��, filter�� �����Ѵ�.
 *
 * Parameters:
 *  aIterator       - [IN]  Iterator�� ������
 *  aSlot           - [IN]  fetch�� ��� row�� slot ������
 *  aRowGRID        - [IN]  fetch�� ��� row�� GRID
 *  aDestBuf        - [OUT] fetch �� �� row�� ����� buffer
 *  aResult         - [OUT] fetch ���
 ******************************************************************************/
static IDE_RC sdnpFetchAndCheckFilter( sdnpIterator * aIterator,
                                       UChar        * aSlot,
                                       scGRID         aRowGRID,
                                       UChar        * aDestBuf,
                                       idBool       * aResult,
                                       idBool       * aIsPageLatchReleased )
{
    idBool      sIsRowDeleted;

    IDE_DASSERT( aIterator != NULL );
    IDE_DASSERT( aSlot     != NULL );
    IDE_DASSERT( aDestBuf  != NULL );
    IDE_DASSERT( aResult   != NULL );
    IDE_DASSERT( sdcRow::isHeadRowPiece(aSlot) == ID_TRUE );

    *aResult = ID_FALSE;

    /* MVCC scheme���� �� ������ �´� row�� ������. */
    IDE_TEST( sdcRow::fetch(
                  aIterator->mProperties->mStatistics,
                  NULL, /* aMtx */
                  NULL, /* aSP */
                  aIterator->mTrans,
                  ((smcTableHeader*)aIterator->mTable)->mSpaceID,
                  aSlot,
                  ID_FALSE, /* aIsPersSlot */
                  SDB_MULTI_PAGE_READ,
                  aIterator->mProperties->mFetchColumnList,
                  SMI_FETCH_VERSION_CONSISTENT,
                  smLayerCallback::getTSSlotSID( aIterator->mTrans ),
                  &(aIterator->mSCN ),
                  &(aIterator->mInfinite),
                  NULL, /* aIndexInfo4Fetch */
                  NULL, /* aLobInfo4Fetch */
                  ((smcTableHeader*)aIterator->mTable)->mRowTemplate,
                  aDestBuf,
                  &sIsRowDeleted,
                  aIsPageLatchReleased )
              != IDE_SUCCESS );

    // delete�� row�̰ų� insert ���� �����̸� skip
    IDE_TEST_CONT( sIsRowDeleted == ID_TRUE, skip_deleted_row );

    // filter����;
    IDE_TEST( aIterator->mFilter->callback( aResult,
                                            aDestBuf,
                                            NULL,
                                            0,
                                            aRowGRID,
                                            aIterator->mFilter->data )
              != IDE_SUCCESS );

    IDE_EXCEPTION_CONT( skip_deleted_row );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*********************************************************
 * function description: sdnpFetchNext.
 * -  beforeFirst�� ���� fetchNext�� ������ keymap sequence
 *    �������Ѽ� �Ʒ��� �۾��� �Ѵ�.
 *    row�� getValidVersion�� ���ϰ�  filter��������, true�̸�
 *    �ش� row�� ������ aRow��  copy�ϰ� iterator�� ��ġ�� �����Ѵ�.
 *
 * - PR-14121
 *   lock coupling ������� ���� �������� latch�� �ɰ�,
 *   ���� �������� unlatch�Ѵ�. �ֳ��ϸ�, page list ������� deadlock
 *   �� ȸ���ϱ� �����̴�.
 ***********************************************************/
static IDE_RC sdnpFetchNext( sdnpIterator   * aIterator,
                             const void    ** aRow )
{
    scGRID      sGRID;
    UChar     * sRowPtr;
    idBool      sIsValidGRID;
    idBool      sResult;
    idBool      sIsPageLatchReleased;
    idBool      sIsFetchSuccess = ID_FALSE;

    // table�� ������ �������� next�� �����߰ų�,
    // selet .. from limit 100������ �о�� �� ���������� �����.
    if( ( aIterator->mProperties->mReadRecordCount == 0 ) ||
        ( aIterator->mNxtRange == NULL) )
    {
        IDE_CONT( skip_no_more_row );
    }

    IDE_TEST( iduCheckSessionEvent( aIterator->mProperties->mStatistics )
              != IDE_SUCCESS );

    while( aIterator->mNxtRange != NULL )
    {
        sGRID = *((scGRID*)aIterator->mNxtRange->minimum.data);
        aIterator->mNxtRange = aIterator->mNxtRange->next;

        IDE_TEST( sdnpValidateAndGetPageByGRID(
                                    aIterator->mProperties->mStatistics,
                                    NULL,
                                    aIterator->mTBSNode,
                                    (smcTableHeader*)aIterator->mTable,
                                    sGRID,
                                    SDB_S_LATCH,
                                    &sRowPtr,
                                    NULL,   /* aPagePtr */           
                                    &sIsValidGRID )
                  != IDE_SUCCESS );

        if( sIsValidGRID == ID_TRUE )
        {
            IDE_TEST( sdnpFetchAndCheckFilter( aIterator,
                                               sRowPtr,
                                               sGRID,
                                               (UChar*)*aRow,
                                               &sResult,
                                               &sIsPageLatchReleased )
                      != IDE_SUCCESS );

            if( sIsPageLatchReleased == ID_FALSE )
            {
                IDE_TEST( sdbBufferMgr::releasePage(
                                        aIterator->mProperties->mStatistics,
                                        sRowPtr )
                          != IDE_SUCCESS );
            }

            if( sResult == ID_TRUE )
            {
                //skip�� ��ġ�� �ִ� ���, select .. from ..limit 3,10
                if( aIterator->mProperties->mFirstReadRecordPos == 0 )
                {
                    if( aIterator->mProperties->mReadRecordCount != 0 )
                    {
                        //�о�� �� row�� ���� ����
                        // ex. select .. from limit 100;
                        // replication���� parallel sync.
                        aIterator->mProperties->mReadRecordCount--;
                        sIsFetchSuccess = ID_TRUE;
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

    IDE_EXCEPTION_CONT( skip_no_more_row );

    if( sIsFetchSuccess == ID_TRUE )
    {
        SC_COPY_GRID( sGRID, aIterator->mRowGRID );
    }
    else
    {
        *aRow = NULL;
        SC_MAKE_NULL_GRID( aIterator->mRowGRID );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*********************************************************
  function description: sdnpLockAllRows4RR
  - select for update, repeatable read�� ���Ͽ� ���̺��� 
  ����Ÿ ù��° ����������, ������ ���������� filter�� 
  ������Ű��  row�鿡 ���Ͽ� row-level lock�� ������ ���� �Ǵ�.

   1. row�� ���Ͽ� update�������� �Ǵ�(sdcRecord::isUpdatable).
        >  skip flag�� �ö���� skip;
        >  retry�� �ö���� �ٽ� ����Ÿ ������ latch�� ���,
           update�������� �Ǵ�.
        >  delete bit�� setting�� row�̸� skip
   2.  filter�����Ͽ� true�̸� lock record( lock�� ǥ���ϴ�
       undo record������ rollptr ����.
***********************************************************/
static IDE_RC sdnpLockAllRows4RR( sdnpIterator* aIterator)
{
    scGRID          sGRID;
    sdrMtx          sMtx;
    sdrSavePoint    sSvp;
    UChar         * sRowPtr;
    UChar         * sPagePtr;   
    idBool          sIsValidGRID;
    SInt            sState = 0;
    UInt            sFirstRead = aIterator->mProperties->mFirstReadRecordPos;
    UInt            sReadRecordCnt = aIterator->mProperties->mReadRecordCount;

    if( (aIterator->mProperties->mReadRecordCount == 0) ||
        (aIterator->mNxtRange == NULL) )
    {
        IDE_CONT( skip_no_more_row );
    }

    while( aIterator->mNxtRange != NULL )
    {
        sGRID = *((scGRID*)aIterator->mNxtRange->minimum.data);
        aIterator->mNxtRange = aIterator->mNxtRange->next;

        IDE_TEST( sdrMiniTrans::begin(
                                aIterator->mProperties->mStatistics,
                                &sMtx,
                                aIterator->mTrans,
                                SDR_MTX_LOGGING,
                                ID_FALSE,/*MtxUndoable(PROJ-2162)*/
                                SM_DLOG_ATTR_DEFAULT | SM_DLOG_ATTR_UNDOABLE )
                != IDE_SUCCESS );
        sState = 1;

        sdrMiniTrans::setSavePoint( &sMtx, &sSvp );

        /* BUG-39674 : sdnpLockRow4RR���� ����ϱ� ���Ͽ� out parameter�� 
         * page�����͸� �߰��Ѵ�. */
        IDE_TEST( sdnpValidateAndGetPageByGRID(
                                    aIterator->mProperties->mStatistics,
                                    &sMtx,
                                    aIterator->mTBSNode,
                                    (smcTableHeader*)aIterator->mTable,
                                    sGRID,
                                    SDB_X_LATCH,
                                    &sRowPtr,
                                    &sPagePtr,
                                    &sIsValidGRID )
                  != IDE_SUCCESS );

        if( sIsValidGRID == ID_TRUE )
        {
            /* BUG-39674 : �Լ��� ���ڷ� sdnpValidateAndGetPageByGRID ���� ����
             * page�����͸� �߰��Ѵ�. */
            IDE_TEST( sdnpLockRow4RR( aIterator,
                                      &sMtx,
                                      &sSvp,
                                      sGRID,
                                      sRowPtr,
                                      sPagePtr)
                      != IDE_SUCCESS );
        }

        sState = 0;
        IDE_TEST( sdrMiniTrans::commit( &sMtx ) != IDE_SUCCESS );

        IDE_TEST( iduCheckSessionEvent( aIterator->mProperties->mStatistics )
                != IDE_SUCCESS );

        if( aIterator->mProperties->mReadRecordCount == 0 )
        {
            goto skip_no_more_row;
        }
    }

    IDE_EXCEPTION_CONT( skip_no_more_row );

    IDE_ASSERT( sState == 0 );

    aIterator->mProperties->mFirstReadRecordPos = sFirstRead;
    aIterator->mProperties->mReadRecordCount    = sReadRecordCnt;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if ( sState != 0 )
    {
        IDE_ASSERT( sdrMiniTrans::commit( &sMtx ) == IDE_SUCCESS );
    }

    return IDE_FAILURE;
}

static IDE_RC sdnpLockRow4RR( sdnpIterator  * aIterator,
                              sdrMtx        * aMtx,
                              sdrSavePoint  * aSvp,
                              scGRID          aGRID,
                              UChar         * aRowPtr, 
                              UChar         * aPagePtr )
{
    sdrMtxStartInfo     sStartInfo;
    idBool              sIsRowDeleted;
    idBool              sIsPageLatchReleased = ID_FALSE;
    UChar             * sPagePtr;
    UChar             * sSlotDirPtr;
    SInt                sSlotSeq;
    UChar             * sSlotPtr;
    idBool              sResult;
    sdcUpdateState      sRetFlag;
    sdrMtx              sLogMtx;
    UChar               sCTSlotIdx = SDP_CTS_IDX_NULL;
    idBool              sSkipLockRec;
    idBool              sDummy;
    UInt                sState = 0;

    /* BUG-39674 : [aRow/aPage]Ptr �� [sSlot/sPage]Ptr�� �ʱ�ȭ �ϰ� sSlotSeq��
     * sSlotSeq�� SC_MAKE_SLOTNUM(aGRID)�� �ʱ�ȭ�Ѵ�. [aRow/aPage]Ptr�� ������
     * ȣ��� sdnpValidateAndGetPageByGRID�Լ����� aGrid�� �̿��Ͽ� ���� slot��
     * page�� pointer �̴�.
     */
    sSlotPtr = aRowPtr;
    sPagePtr = aPagePtr;
    sSlotSeq = SC_MAKE_SLOTNUM(aGRID);

    IDE_ERROR( aIterator->mProperties->mLockRowBuffer != NULL ); // BUG-47758

    sdrMiniTrans::makeStartInfo( aMtx, &sStartInfo );

    /* MVCC scheme���� �� ������ �´� row�� ������. */
    IDE_TEST( sdcRow::fetch(
                    aIterator->mProperties->mStatistics,
                    aMtx,
                    aSvp,
                    aIterator->mTrans,
                    ((smcTableHeader*)aIterator->mTable)->mSpaceID,
                    aRowPtr,
                    ID_TRUE, /* aIsPersSlot */
                    SDB_SINGLE_PAGE_READ,
                    aIterator->mProperties->mFetchColumnList,
                    SMI_FETCH_VERSION_LAST,
                    SD_NULL_SID,/* smLayerCallback::getTSSlotSID( aIterator->mTrans ) */
                    NULL,       /* &(aIterator->mSCN ) */
                    NULL,       /* &(aIterator->mInfinite) */
                    NULL,       /* aIndexInfo4Fetch */
                    NULL,       /* aLobInfo4Fetch */
                    ((smcTableHeader*)aIterator->mTable)->mRowTemplate,
                    aIterator->mProperties->mLockRowBuffer,
                    &sIsRowDeleted,
                    &sIsPageLatchReleased ) != IDE_SUCCESS );

    /* BUG-23319
     * [SD] �ε��� Scan�� sdcRow::fetch �Լ����� Deadlock �߻����ɼ��� ����. */
    /* row fetch�� �ϴ��߿� next rowpiece�� �̵��ؾ� �ϴ� ���,
     * ���� page�� latch�� Ǯ�� ������ deadlock �߻����ɼ��� �ִ�.
     * �׷��� page latch�� Ǭ ���� next rowpiece�� �̵��ϴµ�,
     * ���� �Լ������� page latch�� Ǯ������ ���θ� output parameter�� Ȯ���ϰ�
     * ��Ȳ�� ���� ������ ó���� �ؾ� �Ѵ�. */
    if( sIsPageLatchReleased == ID_TRUE )
    {
        IDE_TEST( sdbBufferMgr::getPageByPID(
                                        aIterator->mProperties->mStatistics,
                                        SC_MAKE_SPACE(aGRID),
                                        SC_MAKE_PID(aGRID),
                                        SDB_X_LATCH,
                                        SDB_WAIT_NORMAL,
                                        SDB_SINGLE_PAGE_READ,
                                        aMtx,
                                        &sPagePtr,
                                        &sDummy,
                                        NULL ) /* aIsCorruptPage */
                  != IDE_SUCCESS );

        sIsPageLatchReleased = ID_FALSE;

        /* BUG-32010 [sm-disk-collection] 'select for update' DRDB module
         * does not consider that the slot directory shift down caused 
         * by ExtendCTS.
         * PageLatch�� Ǯ���� ���� CTL Ȯ������ SlotDirectory�� ������ ��
         * �ִ�. */
        sSlotDirPtr  = sdpPhyPage::getSlotDirStartPtr(sPagePtr);
       
        if( sdpSlotDirectory::isUnusedSlotEntry(sSlotDirPtr, sSlotSeq)
            == ID_TRUE )
        {
            IDE_CONT( skip_lock_row );
        }

        IDE_TEST( sdpSlotDirectory::getPagePtrFromSlotNum( sSlotDirPtr,
                                                           sSlotSeq,
                                                           &sSlotPtr )
                  != IDE_SUCCESS );
    }

    /* BUG-45188
       �� ���۷� ST��� �Ǵ� MT����� ȣ���ϴ� ���� ���´�.
       sResult = ID_TRUE �� �����ؼ�, sdcRow::isDeleted() == ID_TRUE �� skip �ϵ��� �Ѵ�.
       ( ���⼭ �ٷ� skip �ϸ�, row stamping�� ���ϴ� ��찡 �־ ���Ͱ��� ó���Ͽ���.) */
    if ( sIsRowDeleted == ID_TRUE )
    {
        sResult = ID_TRUE;
    }
    else
    {
        IDE_TEST( aIterator->mFilter->callback( &sResult,
                                                aIterator->mProperties->mLockRowBuffer,
                                                NULL,
                                                0,
                                                aGRID,
                                                aIterator->mFilter->data )
                  != IDE_SUCCESS );
    }

    if( sResult == ID_TRUE )
    {
        /* BUG-32010 [sm-disk-collection] 'select for update' DRDB module
            * does not consider that the slot directory shift down caused 
            * by ExtendCTS.
            * canUpdateRowPiece�� Filtering ���Ŀ� �ؾ� �Ѵ�. �׷���������
            * ���� ��� ���� Row�� ���� ���� ���θ� �Ǵ��ϴٰ� Wait�ϰ�
            * �ȴ�. */

        IDE_TEST( sdcRow::canUpdateRowPiece(
                                aIterator->mProperties->mStatistics,
                                aMtx,
                                aSvp,
                                SC_MAKE_SPACE(aGRID),
                                SD_MAKE_SID_FROM_GRID(aGRID),
                                SDB_SINGLE_PAGE_READ,
                                &(aIterator->mSCN),
                                &(aIterator->mInfinite),
                                ID_FALSE, /* aIsUptLobByAPI */
                                (UChar**)&sSlotPtr,
                                &sRetFlag,
                                NULL, /* aCTSlotIdx */
                                aIterator->mProperties->mLockWaitMicroSec)
                  != IDE_SUCCESS );

        if( sRetFlag == SDC_UPTSTATE_REBUILD_ALREADY_MODIFIED )
        {
            /* ���� ���� ����ϰ� releaseLatch */
            IDE_RAISE( rebuild_already_modified );
        }

        if( sRetFlag == SDC_UPTSTATE_INVISIBLE_MYUPTVERSION )
        {
            IDE_CONT( skip_lock_row );
        }

        // delete�� row�̰ų� insert ���� �����̸� skip
        if( sdcRow::isDeleted(sSlotPtr) == ID_TRUE )
        {
            IDE_CONT( skip_lock_row );
        }

        /*
         * BUG-25385 disk table�� ���, sm���� for update���� ���� '
         *           scan limit�� ������� ����. 
         */
        //skip�� ��ġ�� �ִ� ��� ex)select .. from ..limit 3,10
        if( aIterator->mProperties->mFirstReadRecordPos == 0 )
        {
            if( aIterator->mProperties->mReadRecordCount != 0 )
            {
                //�о�� �� row�� ���� ����
                // ex) select .. from limit 100;
                aIterator->mProperties->mReadRecordCount--;
            }
        }
        else
        {
            aIterator->mProperties->mFirstReadRecordPos--;
        }

        /* BUG-45401 : undoable ID_FALSE -> ID_TRUE�� ���� */
        IDE_TEST( sdrMiniTrans::begin(
                                aIterator->mProperties->mStatistics,
                                &sLogMtx,
                                &sStartInfo,
                                ID_TRUE,/*MtxUndoable(PROJ-2162)*/
                                SM_DLOG_ATTR_DEFAULT | SM_DLOG_ATTR_UNDOABLE )
                  != IDE_SUCCESS );
        sState = 1;

        if( sCTSlotIdx == SDP_CTS_IDX_NULL )
        {
            IDE_TEST( sdcTableCTL::allocCTS(
                                    aIterator->mProperties->mStatistics,
                                    aMtx,
                                    &sLogMtx,
                                    SDB_SINGLE_PAGE_READ,
                                    (sdpPhyPageHdr*)sPagePtr,
                                    &sCTSlotIdx ) != IDE_SUCCESS );
        }

        /* allocCTS()�ÿ� CTL Ȯ���� �߻��ϴ� ���,
         * CTL Ȯ���߿� compact page ������ �߻��� �� �ִ�.
         * compact page ������ �߻��ϸ�
         * ������������ slot���� ��ġ(offset)�� ����� �� �ִ�.
         * �׷��Ƿ� allocCTS() �Ŀ��� slot pointer�� �ٽ� ���ؿ;� �Ѵ�. */
        /* BUG-32010 [sm-disk-collection] 'select for update' DRDB module
         * does not consider that the slot directory shift down caused 
         * by ExtendCTS. 
         * AllocCTS�� �ƴϴ���, canUpdateRowPiece���꿡 ���� LockWait
         * �� ���� ��쵵 PageLatch�� Ǯ �� �ְ�, �̵��� CTL Ȯ����
         * �Ͼ �� �ִ�.*/
        sSlotDirPtr  = sdpPhyPage::getSlotDirStartPtr(sPagePtr);
        IDE_TEST( sdpSlotDirectory::getPagePtrFromSlotNum( sSlotDirPtr,
                                                           sSlotSeq,
                                                           &sSlotPtr )
                  != IDE_SUCCESS );

        // lock undo record�� �����Ѵ�.
        IDE_TEST( sdcRow::lock( aIterator->mProperties->mStatistics,
                                sSlotPtr,
                                SD_MAKE_SID_FROM_GRID(aGRID),
                                &(aIterator->mInfinite),
                                &sLogMtx,
                                sCTSlotIdx,
                                &sSkipLockRec )
                  != IDE_SUCCESS );

        if ( sSkipLockRec == ID_FALSE )
        {
            IDE_TEST( sdrMiniTrans::setDirtyPage( &sLogMtx, sPagePtr )
                      != IDE_SUCCESS );
        }

        sState = 0;
        IDE_TEST( sdrMiniTrans::commit( &sLogMtx ) != IDE_SUCCESS );
    }

    IDE_EXCEPTION_CONT( skip_lock_row );

    return IDE_SUCCESS;

    IDE_EXCEPTION( rebuild_already_modified );
    {
        if( aIterator->mStatement->isForbiddenToRetry() == ID_TRUE )
        {
            IDE_DASSERT( ((smxTrans*)aIterator->mTrans)->mIsGCTx == ID_TRUE );

            SChar   sMsgBuf[SMI_MAX_ERR_MSG_LEN];
            sdpCTS *sCTS;
            smSCN   sFSCNOrCSCN;
            UChar   sCTSlotIdx;
            sdcRowHdrInfo   sRowHdrInfo;
            sdcRowHdrExInfo sRowHdrExInfo;

            sdcRow::getRowHdrInfo( sSlotPtr, &sRowHdrInfo );
            sCTSlotIdx = sRowHdrInfo.mCTSlotIdx;

            if ( SDC_HAS_BOUND_CTS(sCTSlotIdx) )
            {
                sCTS = sdcTableCTL::getCTS( sdpPhyPage::getHdr(sSlotPtr),sCTSlotIdx );
                SM_SET_SCN( &sFSCNOrCSCN, &sCTS->mFSCNOrCSCN );
            }
            else
            {
                sdcRow::getRowHdrExInfo( sSlotPtr, &sRowHdrExInfo );
                SM_SET_SCN( &sFSCNOrCSCN, &sRowHdrExInfo.mFSCNOrCSCN );
            }

            idlOS::snprintf( sMsgBuf,
                             SMI_MAX_ERR_MSG_LEN,
                             "[GRID SCAN VALIDATION(RR)] "
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

        IDE_ASSERT( sdcRow::releaseLatchForAlreadyModify( aMtx, aSvp )
                     == IDE_SUCCESS );
    }
    IDE_EXCEPTION_END;

    IDE_PUSH();

    if ( sState != 0 )
    {
        IDE_ASSERT( sdrMiniTrans::rollback( &sLogMtx ) == IDE_SUCCESS );
    }

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
    IDE_POP();

    return IDE_FAILURE;
}

static IDE_RC sdnpAllocIterator( void ** /* aIteratorMem */ )
{
    return IDE_SUCCESS;
}

static IDE_RC sdnpFreeIterator( void * /* aIteratorMem */ )
{
    return IDE_SUCCESS;
}

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
static IDE_RC sdnpLockRow( sdnpIterator* aIterator )
{
    return sdnManager::lockRow( aIterator->mProperties,
                                aIterator->mTrans,
                                &(aIterator->mSCN),
                                &(aIterator->mInfinite),
                                ((smcTableHeader*)aIterator->mTable)->mSpaceID,
                                SD_MAKE_RID_FROM_GRID(aIterator->mRowGRID),
                                aIterator->mStatement->isForbiddenToRetry() );
}

/*******************************************************************************
 * Description: DRDB�� ���� GRID�� ��ȿ���� Ȯ���ϴ� �Լ�
 *  
 * Parameters:        
 *  - aStatistics   [IN] idvSQL
 *  - aTableHdr     [IN] Fetch ��� table�� header
 *  - aMtx          [IN] Fetch ��� page�� getPage�Ͽ� �����ϱ� ���� mtx
 *  - aGRID         [IN] ���� ��� GRID
 *  - aIsValidGRID  [OUT] ��ȿ�� GRID���� ���θ� ��ȯ
 ******************************************************************************/
static IDE_RC sdnpValidateAndGetPageByGRID(
                                        idvSQL              * aStatistics,
                                        sdrMtx              * aMtx,
                                        sddTableSpaceNode   * aTBSNode,
                                        smcTableHeader      * aTableHdr,
                                        scGRID                aGRID,
                                        sdbLatchMode          aLatchMode,
                                        UChar              ** aRowPtr,
                                        UChar              ** aPagePtr,
                                        idBool              * aIsValidGRID )
{
    scSpaceID           sSpaceID;
    scPageID            sPageID;
    scSlotNum           sSlotNum;
    sddDataFileNode   * sFileNode;
    sdrSavePoint        sSvp;
    UChar             * sPagePtr;
    sdpPhyPageHdr     * sPageHdr;
    UChar             * sSlotDir;
    UInt                sSlotCount;
    UChar             * sSlotPtr = NULL;
    SInt                sState = 0;

    *aIsValidGRID = ID_FALSE;

    /* ���� �� �ִ� GRID���� �˻� */
    IDE_TEST_CONT( SC_GRID_IS_NULL(aGRID), error_invalid_grid );
    IDE_TEST_CONT( SC_GRID_IS_NOT_WITH_SLOTNUM(aGRID) , error_invalid_grid );

    sSpaceID = SC_MAKE_SPACE(aGRID);
    sPageID  = SC_MAKE_PID(aGRID);
    sSlotNum = SC_MAKE_SLOTNUM(aGRID);

    /* GRID�� iterator�� SpaceID ��ġ �˻� */
    IDE_TEST_CONT( sSpaceID != aTableHdr->mSpaceID,
                    error_invalid_grid );

    /* GRID�� PageID�� ��ȿ�� PageID ���� ������ Ȯ�� */
    IDE_TEST( sddTableSpace::getDataFileNodeByPageID( aTBSNode,
                                                      sPageID,
                                                      &sFileNode,
                                                      ID_FALSE ) /* aFatal */
              != IDE_SUCCESS );

    if( aMtx != NULL )
    {
        sdrMiniTrans::setSavePoint( aMtx, &sSvp );
    }

    /* Page�� �о TableOID�� ��ġ�ϴ��� Ȯ�� */
    IDE_TEST( sdbBufferMgr::getPageByPID( aStatistics,
                                          sSpaceID,
                                          sPageID,
                                          aLatchMode,
                                          SDB_WAIT_NORMAL,
                                          SDB_SINGLE_PAGE_READ,
                                          aMtx,
                                          (UChar**)&sPagePtr,
                                          NULL, /*TrySuccess*/
                                          NULL  /*IsCorruptPage*/ )
              != IDE_SUCCESS );
    sState = 1;

    sPageHdr = (sdpPhyPageHdr*)sPagePtr;

    IDE_TEST_CONT( sPageHdr->mTableOID != aTableHdr->mSelfOID,
                    error_invalid_grid );

    /* Data page�� �´��� Ȯ�� */
    IDE_TEST_CONT( sdpPhyPage::getPageType(sPageHdr) != SDP_PAGE_DATA,
                    error_invalid_grid );

    /* GRID�� SlotNum�� page�� slot count �̸����� Ȯ�� */ 
    sSlotDir   = sdpPhyPage::getSlotDirStartPtr(sPagePtr);
    sSlotCount = sdpSlotDirectory::getCount(sSlotDir);

    IDE_TEST_CONT( sSlotCount <= sSlotNum, error_invalid_grid );

    IDE_TEST_CONT( sdpSlotDirectory::isUnusedSlotEntry(sSlotDir, sSlotNum)
                    == ID_TRUE, error_invalid_grid )

    /* slot pointer�� �� out parameter�� ������ �ش�. */
    IDE_TEST( sdpSlotDirectory::getPagePtrFromSlotNum( sSlotDir,
                                                       SC_MAKE_SLOTNUM(aGRID),
                                                       &sSlotPtr)
              != IDE_SUCCESS );

    IDE_TEST_CONT( sdcRow::isHeadRowPiece(sSlotPtr) != ID_TRUE,
                    error_invalid_grid );

    /* ��� �˻縦 ����Ͽ����Ƿ� out parameter�� slot pointer �Ҵ��ؼ� �ѱ�. */
    *aRowPtr = sSlotPtr;
    *aIsValidGRID = ID_TRUE;
    
    /* BUG-39674 : out parameter�� Page pointer �Ҵ��ؼ� �ѱ�. 
     * �� �Լ� ( sdnpValidateAndGetPageByGRID ) �� ȣ���ϴ� �Լ��� smnpFetchNext
     * ������ page pointer�� �ѱ� �ʿ䰡 ���� ������ �ش� ���ڸ� NULL�� ���� 
     */  
    if( aPagePtr != NULL )
    {
         *aPagePtr = sPagePtr;
    }
    else 
    {
        /* do nothing */
    }

    IDE_EXCEPTION_CONT( error_invalid_grid );

    if( *aIsValidGRID == ID_FALSE )
    {
        if( sState == 1 )
        {
            sState = 0;

            if( aMtx == NULL )
            {
                IDE_TEST( sdbBufferMgr::releasePage( aStatistics,
                                                    sPagePtr )
                          != IDE_SUCCESS );
            }
            else
            {
                IDE_TEST( sdrMiniTrans::releaseLatchToSP( aMtx, &sSvp )
                          != IDE_SUCCESS );
            }
        }

        *aRowPtr = NULL;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if( sState == 1 )
    {
        if( aMtx == NULL )
        {
            IDE_ASSERT( sdbBufferMgr::releasePage( NULL, /* idvSQL */
                                                   sPagePtr )
                        == IDE_SUCCESS );
        }
        else
        {
            IDE_ASSERT( sdrMiniTrans::releaseLatchToSP( aMtx, &sSvp )
                        == IDE_SUCCESS );
        }
    }

    return IDE_FAILURE;
}

