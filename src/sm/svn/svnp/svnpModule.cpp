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
 * $Id: svnpModule.cpp 90083 2021-02-26 00:58:48Z et16 $
 ******************************************************************************/

#include <ide.h>
#include <idu.h>
#include <smi.h>
#include <smc.h>
#include <svm.h>
#include <svp.h>

#include <smErrorCode.h>
#include <smnModules.h>
#include <svnReq.h>
#include <svnpDef.h>
#include <svnpModule.h>
#include <smnManager.h>
#include <svmExpandChunk.h>

static IDE_RC svnpPrepareIteratorMem( const smnIndexModule* );

static IDE_RC svnpReleaseIteratorMem(const smnIndexModule* );

static IDE_RC svnpInit( svnpIterator         * aIterator,
                        void                 * aTrans,
                        smcTableHeader       * aTable,
                        smnIndexHeader       * /* aIndex */,
                        void                 * /* aDumpObject */,
                        const smiRange       * aKeyRange,
                        const smiRange       * /* aKeyFilter */,
                        const smiCallBack    * aRowFilter,
                        UInt                   aFlag,
                        smSCN                  aSCN,
                        smSCN                  aInfinite,
                        idBool                 /* aUntouchable */,
                        smiCursorProperties  * aProperties,
                        const smSeekFunc    ** aSeekFunc,
                        smiStatement         * aStatement );

static IDE_RC svnpDest( svnpIterator* aIterator );

static IDE_RC svnpNA( void );
static IDE_RC svnpAA( svnpIterator  * aIterator,
                                 const void   ** aRow );
static IDE_RC svnpBeforeFirst( svnpIterator      * aIterator,
                               const smSeekFunc ** aSeekFunc );
static IDE_RC svnpAfterLast( svnpIterator      * aIterator,
                             const smSeekFunc ** aSeekFunc );
static IDE_RC svnpBeforeFirstU( svnpIterator      * aIterator,
                                const smSeekFunc ** aSeekFunc );
static IDE_RC svnpAfterLastU( svnpIterator      * aIterator,
                              const smSeekFunc ** aSeekFunc );
static IDE_RC svnpBeforeFirstR( svnpIterator      * aIterator,
                                const smSeekFunc ** aSeekFunc );
static IDE_RC svnpAfterLastR( svnpIterator      * aIterator,
                              const smSeekFunc ** aSeekFunc );
static IDE_RC svnpFetchNext( svnpIterator  * aIterator,
                             const void   ** aRow );
static IDE_RC svnpFetchPrev( svnpIterator  * aIterator,
                             const void   ** aRow );
static IDE_RC svnpFetchNextU( svnpIterator  * aIterator,
                              const void   ** aRow );
static IDE_RC svnpFetchPrevU( svnpIterator  * aIterator,
                              const void   ** aRow );
static IDE_RC svnpFetchNextR( svnpIterator  * aIterator );

static IDE_RC svnpAllocIterator( void  ** aIteratorMem );

static IDE_RC svnpFreeIterator( void  * aIteratorMem );

static IDE_RC svnpGetValidVersion( svnpIterator  * aIterator,
                                   smOID           aRowOID,
                                   void         ** aRow,
                                   idBool        * aIsVisibleRow );

static IDE_RC svnpValidateGRID( smcTableHeader     * aTableHdr,
                                scGRID               aGRID,
                                idBool             * aIsValidGRID );

smnIndexModule svnpModule = {
    SMN_MAKE_INDEX_MODULE_ID( SMI_TABLE_VOLATILE,
                              SMI_BUILTIN_GRID_INDEXTYPE_ID ),
    SMN_RANGE_ENABLE|SMN_DIMENSION_DISABLE,
    ID_SIZEOF( svnpIterator ),
    (smnMemoryFunc) svnpPrepareIteratorMem,
    (smnMemoryFunc) svnpReleaseIteratorMem,
    (smnMemoryFunc) NULL,
    (smnMemoryFunc) NULL,
    (smnCreateFunc) NULL,
    (smnDropFunc) NULL,
    (smTableCursorLockRowFunc) smnManager::lockRow,
    (smnDeleteFunc) NULL,
    (smnFreeFunc) NULL,
    (smnInsertRollbackFunc) NULL,
    (smnDeleteRollbackFunc) NULL,
    (smnAgingFunc) NULL,
    (smInitFunc) svnpInit,
    (smDestFunc) svnpDest,
    (smnFreeNodeListFunc) NULL,
    (smnGetPositionFunc) NULL,
    (smnSetPositionFunc) NULL,

    (smnAllocIteratorFunc) svnpAllocIterator,
    (smnFreeIteratorFunc) svnpFreeIterator,
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

static const smSeekFunc svnpSeekFunctions[32][12] =
{ { /* SMI_TRAVERSE_FORWARD  SMI_PREVIOUS_DISABLE SMI_LOCK_READ            */
        (smSeekFunc)svnpFetchNext,
        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpBeforeFirst,
        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpAA,
        (smSeekFunc)svnpAA,

        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpAA,
        (smSeekFunc)svnpAA
    },
    { /* SMI_TRAVERSE_FORWARD  SMI_PREVIOUS_DISABLE SMI_LOCK_WRITE           */
        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpBeforeFirstU,
        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpAA,
        (smSeekFunc)svnpAA,

        (smSeekFunc)svnpFetchNext,
        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpAA,
        (smSeekFunc)svnpAA
    },
    { /* SMI_TRAVERSE_FORWARD  SMI_PREVIOUS_DISABLE SMI_LOCK_REPEATALBE      */
        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpBeforeFirstR,
        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpAA,
        (smSeekFunc)svnpAA,

        (smSeekFunc)svnpFetchNext,
        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpBeforeFirst,
        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpAA,
        (smSeekFunc)svnpAA
    },
    { /* SMI_TRAVERSE_FORWARD  SMI_PREVIOUS_DISABLE SMI_LOCK_TABLE_SHARED    */
        (smSeekFunc)svnpFetchNext,
        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpBeforeFirst,
        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpAA,
        (smSeekFunc)svnpAA,

        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpAA,
        (smSeekFunc)svnpAA
    },
    { /* SMI_TRAVERSE_FORWARD  SMI_PREVIOUS_DISABLE SMI_LOCK_TABLE_EXCLUSIVE */
        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpBeforeFirstU,
        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpAA,
        (smSeekFunc)svnpAA,

        (smSeekFunc)svnpFetchNext,
        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpAA,
        (smSeekFunc)svnpAA
    },
    { /* UNUSED                                                              */
        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpNA,

        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpNA
    },
    { /* UNUSED                                                              */
        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpNA,

        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpNA
    },
    { /* UNUSED                                                              */
        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpNA,

        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpNA
    },
    { /* SMI_TRAVERSE_FORWARD  SMI_PREVIOUS_ENABLE  SMI_LOCK_READ            */
        (smSeekFunc)svnpFetchNext,
        (smSeekFunc)svnpFetchPrev,
        (smSeekFunc)svnpBeforeFirst,
        (smSeekFunc)svnpAfterLast,
        (smSeekFunc)svnpAA,
        (smSeekFunc)svnpAA,

        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpAA,
        (smSeekFunc)svnpAA
    },
    { /* SMI_TRAVERSE_FORWARD  SMI_PREVIOUS_ENABLE  SMI_LOCK_WRITE           */
        (smSeekFunc)svnpFetchNextU,
        (smSeekFunc)svnpFetchPrevU,
        (smSeekFunc)svnpBeforeFirst,
        (smSeekFunc)svnpAfterLast,
        (smSeekFunc)svnpAA,
        (smSeekFunc)svnpAA,

        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpAA,
        (smSeekFunc)svnpAA
    },
    { /* SMI_TRAVERSE_FORWARD  SMI_PREVIOUS_ENABLE  SMI_LOCK_REPEATALBE      */
        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpBeforeFirstR,
        (smSeekFunc)svnpAfterLastR,
        (smSeekFunc)svnpAA,
        (smSeekFunc)svnpAA,

        (smSeekFunc)svnpFetchNext,
        (smSeekFunc)svnpFetchPrev,
        (smSeekFunc)svnpBeforeFirst,
        (smSeekFunc)svnpAfterLast,
        (smSeekFunc)svnpAA,
        (smSeekFunc)svnpAA
    },
    { /* SMI_TRAVERSE_FORWARD  SMI_PREVIOUS_ENABLE  SMI_LOCK_TABLE_SHARED    */
        (smSeekFunc)svnpFetchNext,
        (smSeekFunc)svnpFetchPrev,
        (smSeekFunc)svnpBeforeFirst,
        (smSeekFunc)svnpAfterLast,
        (smSeekFunc)svnpAA,
        (smSeekFunc)svnpAA,

        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpAA,
        (smSeekFunc)svnpAA
    },
    { /* SMI_TRAVERSE_FORWARD  SMI_PREVIOUS_ENABLE  SMI_LOCK_TABLE_EXCLUSIVE */
        (smSeekFunc)svnpFetchNextU,
        (smSeekFunc)svnpFetchPrevU,
        (smSeekFunc)svnpBeforeFirst,
        (smSeekFunc)svnpAfterLast,
        (smSeekFunc)svnpAA,
        (smSeekFunc)svnpAA,

        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpAA,
        (smSeekFunc)svnpAA
    },
    { /* UNUSED                                                              */
        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpNA,

        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpNA
    },
    { /* UNUSED                                                              */
        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpNA,

        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpNA
    },
    { /* UNUSED                                                              */
        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpNA,

        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpNA
    },
    { /* SMI_TRAVERSE_BACKWARD SMI_PREVIOUS_DISABLE SMI_LOCK_READ            */
        (smSeekFunc)svnpFetchPrev,
        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpAfterLast,
        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpAA,
        (smSeekFunc)svnpAA,

        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpAA,
        (smSeekFunc)svnpAA
    },
    { /* SMI_TRAVERSE_BACKWARD SMI_PREVIOUS_DISABLE SMI_LOCK_WRITE           */
        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpAfterLastU,
        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpAA,
        (smSeekFunc)svnpAA,

        (smSeekFunc)svnpFetchPrev,
        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpAA,
        (smSeekFunc)svnpAA
    },
    { /* SMI_TRAVERSE_BACKWARD SMI_PREVIOUS_DISABLE SMI_LOCK_REPEATABLE      */
        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpAfterLastR,
        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpAA,
        (smSeekFunc)svnpAA,

        (smSeekFunc)svnpFetchPrev,
        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpAfterLast,
        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpAA,
        (smSeekFunc)svnpAA
    },
    { /* SMI_TRAVERSE_BACKWARD SMI_PREVIOUS_DISABLE SMI_LOCK_TABLE_SHARED    */
        (smSeekFunc)svnpFetchPrev,
        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpAfterLast,
        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpAA,
        (smSeekFunc)svnpAA,

        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpAA,
        (smSeekFunc)svnpAA
    },
    { /* SMI_TRAVERSE_BACKWARD SMI_PREVIOUS_DISABLE SMI_LOCK_TABLE_EXCLUSIVE */
        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpAfterLastU,
        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpAA,
        (smSeekFunc)svnpAA,

        (smSeekFunc)svnpFetchPrev,
        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpAA,
        (smSeekFunc)svnpAA
    },
    { /* UNUSED                                                              */
        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpNA,

        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpNA
    },
    { /* UNUSED                                                              */
        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpNA,

        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpNA
    },
    { /* UNUSED                                                              */
        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpNA,

        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpNA
    },
    { /* SMI_TRAVERSE_BACKWARD SMI_PREVIOUS_ENABLE  SMI_LOCK_READ            */
        (smSeekFunc)svnpFetchPrev,
        (smSeekFunc)svnpFetchNext,
        (smSeekFunc)svnpAfterLast,
        (smSeekFunc)svnpBeforeFirst,
        (smSeekFunc)svnpAA,
        (smSeekFunc)svnpAA,

        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpAA,
        (smSeekFunc)svnpAA      // BUGBUG
    },
    { /* SMI_TRAVERSE_BACKWARD SMI_PREVIOUS_ENABLE  SMI_LOCK_WRITE           */
        (smSeekFunc)svnpFetchPrevU,
        (smSeekFunc)svnpFetchNextU,
        (smSeekFunc)svnpAfterLast,
        (smSeekFunc)svnpBeforeFirst,
        (smSeekFunc)svnpAA,
        (smSeekFunc)svnpAA,

        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpAA,
        (smSeekFunc)svnpAA
    },
    { /* SMI_TRAVERSE_BACKWARD SMI_PREVIOUS_ENABLE  SMI_LOCK_REPEATABLE      */
        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpAfterLastR,
        (smSeekFunc)svnpBeforeFirstR,
        (smSeekFunc)svnpAA,
        (smSeekFunc)svnpAA,

        (smSeekFunc)svnpFetchPrev,
        (smSeekFunc)svnpFetchNext,
        (smSeekFunc)svnpAfterLast,
        (smSeekFunc)svnpBeforeFirst,
        (smSeekFunc)svnpAA,
        (smSeekFunc)svnpAA
    },
    { /* SMI_TRAVERSE_BACKWARD SMI_PREVIOUS_ENABLE  SMI_LOCK_TABLE_SHARED    */
        (smSeekFunc)svnpFetchPrev,
        (smSeekFunc)svnpFetchNext,
        (smSeekFunc)svnpAfterLast,
        (smSeekFunc)svnpBeforeFirst,
        (smSeekFunc)svnpAA,
        (smSeekFunc)svnpAA,

        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpAA,
        (smSeekFunc)svnpAA
    },
    { /* SMI_TRAVERSE_BACKWARD SMI_PREVIOUS_ENABLE  SMI_LOCK_TABLE_EXCLUSIVE */
        (smSeekFunc)svnpFetchPrevU,
        (smSeekFunc)svnpFetchNextU,
        (smSeekFunc)svnpAfterLast,
        (smSeekFunc)svnpBeforeFirst,
        (smSeekFunc)svnpAA,
        (smSeekFunc)svnpAA,

        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpAA,
        (smSeekFunc)svnpAA
    },
    { /* UNUSED                                                              */
        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpNA,

        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpNA
    },
    { /* UNUSED                                                              */
        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpNA,

        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpNA
    },
    { /* UNUSED                                                              */
        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpNA,

        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpNA,
        (smSeekFunc)svnpNA
    }
};


static IDE_RC svnpPrepareIteratorMem( const smnIndexModule* )
{
    return IDE_SUCCESS;
}

static IDE_RC svnpReleaseIteratorMem(const smnIndexModule* )
{
    return IDE_SUCCESS;
}

static IDE_RC svnpInit( svnpIterator          * aIterator,
                        void                  * aTrans,
                        smcTableHeader        * aTable,
                        smnIndexHeader        * ,
                        void                  * ,
                        const smiRange        * aRange,
                        const smiRange        * ,
                        const smiCallBack     * aFilter,
                        UInt                    aFlag,
                        smSCN                   aSCN,
                        smSCN                   aInfinite,
                        idBool                  ,
                        smiCursorProperties   * aProperties,
                        const smSeekFunc     ** aSeekFunc,
                        smiStatement          * aStatement )
{
    idvSQL                        *sSQLStat;

    aIterator->SCN                = aSCN;
    aIterator->infinite           = aInfinite;
    aIterator->trans              = aTrans;
    aIterator->table              = aTable;
    aIterator->curRecPtr          = NULL;
    aIterator->lstFetchRecPtr     = NULL;
    SC_MAKE_NULL_GRID( aIterator->mRowGRID );
    aIterator->tid                = smLayerCallback::getTransID(aTrans);
    aIterator->flag               = aFlag;
    aIterator->mProperties        = aProperties;

    aIterator->mRange             = aRange;
    aIterator->mNxtRange          = NULL;
    aIterator->mFilter            = aFilter;
    aIterator->mStatement         = aStatement; 


    *aSeekFunc = svnpSeekFunctions[ aFlag&(SMI_TRAVERSE_MASK |
                                           SMI_PREVIOUS_MASK |
                                           SMI_LOCK_MASK) ];

    sSQLStat = aIterator->mProperties->mStatistics;
    IDV_SQL_ADD(sSQLStat, mMemCursorGRIDScan, 1);
    
    if (sSQLStat != NULL)
    {
        IDV_SESS_ADD(sSQLStat->mSess, IDV_STAT_INDEX_MEM_CURSOR_GRID_SCAN, 1);
    }
    
    return IDE_SUCCESS;
}

static IDE_RC svnpDest( svnpIterator* /*aIterator*/ )
{
    return IDE_SUCCESS;
}

static IDE_RC svnpAA( svnpIterator*,
                      const void** )
{
    return IDE_SUCCESS;
}

IDE_RC svnpNA( void )
{
    IDE_SET( ideSetErrorCode( smERR_ABORT_smiTraverseNotApplicable ) );

    return IDE_FAILURE;
}

static IDE_RC svnpBeforeFirst( svnpIterator       * aIterator,
                               const smSeekFunc  ** )
{
    aIterator->curRecPtr         = NULL;
    aIterator->lstFetchRecPtr    = NULL;

    aIterator->mNxtRange = aIterator->mRange;
   
    return IDE_SUCCESS;
}                                                                             

static IDE_RC svnpAfterLast( svnpIterator       * aIterator,
                             const smSeekFunc  ** )
{
    aIterator->curRecPtr         = NULL;
    aIterator->lstFetchRecPtr    = NULL;

    for( aIterator->mNxtRange = aIterator->mRange;
         aIterator->mNxtRange->next != NULL;
         aIterator->mNxtRange = aIterator->mNxtRange->next ) ;

    return IDE_SUCCESS;
}                                                                             

static IDE_RC svnpBeforeFirstU( svnpIterator       * aIterator,
                                const smSeekFunc  ** aSeekFunc )
{
    IDE_TEST( svnpBeforeFirst( aIterator, aSeekFunc ) != IDE_SUCCESS );
    
    *aSeekFunc += 6;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}                                                                             

static IDE_RC svnpAfterLastU( svnpIterator       * aIterator,
                              const smSeekFunc  ** aSeekFunc )
{
    IDE_TEST( svnpAfterLast( aIterator, aSeekFunc ) != IDE_SUCCESS );
    
    *aSeekFunc += 6;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}                                                                             

static IDE_RC svnpBeforeFirstR( svnpIterator       * aIterator,
                                const smSeekFunc  ** aSeekFunc )
{
    IDE_TEST( svnpBeforeFirst( aIterator, aSeekFunc ) != IDE_SUCCESS );
    
    IDE_TEST( svnpFetchNextR( aIterator ) != IDE_SUCCESS );
    
    aIterator->curRecPtr          = NULL;
    aIterator->lstFetchRecPtr     = NULL;

    *aSeekFunc += 6;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}                                                                             

static IDE_RC svnpAfterLastR( svnpIterator       * aIterator,
                              const smSeekFunc  ** aSeekFunc )
{
    IDE_TEST( svnpBeforeFirst( aIterator, aSeekFunc ) != IDE_SUCCESS );
    
    IDE_TEST( svnpFetchNextR( aIterator ) != IDE_SUCCESS );

    IDE_TEST( svnpAfterLast( aIterator, aSeekFunc ) != IDE_SUCCESS );

    *aSeekFunc += 6;

    return IDE_SUCCESS;
    
    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}                                                                             

static IDE_RC svnpFetchNext( svnpIterator   * aIterator,
                             const void    ** aRow )
{
    smOID       sRowOID;
    idBool      sResult;
    idBool      sIsVisibleRow;
    scGRID      sGRID;
    idBool      sIsValidGRID;
    idBool      sIsFetchSuccess = ID_FALSE;

    if( (aIterator->mProperties->mReadRecordCount == 0) ||
        (aIterator->mNxtRange == NULL) )
    {
        IDE_CONT( skip_no_more_row );
    }
    else
    {
        /* do nothing */
    }

    IDE_TEST( iduCheckSessionEvent(aIterator->mProperties->mStatistics)
              != IDE_SUCCESS);

    SC_MAKE_NULL_GRID( sGRID );

    while( aIterator->mNxtRange != NULL )
    {
        sGRID = *((scGRID*)aIterator->mNxtRange->minimum.data);
        aIterator->mNxtRange = aIterator->mNxtRange->next;

        IDE_TEST( svnpValidateGRID( aIterator->table, sGRID, &sIsValidGRID )
                  != IDE_SUCCESS );

        if( sIsValidGRID == ID_FALSE )
        {
            continue;
        }
        else
        {
            /* do nothing */
        }

        sRowOID = SM_MAKE_OID( SC_MAKE_PID(sGRID), SC_MAKE_OFFSET(sGRID) );

        IDE_TEST( svnpGetValidVersion( aIterator,
                                       sRowOID,
                                       (void**)&aIterator->curRecPtr,
                                       &sIsVisibleRow )
                  != IDE_SUCCESS );

        if( sIsVisibleRow == ID_FALSE)
        {
            continue;
        }
        else
        {
            /* do nothing */
        }

        IDE_TEST( aIterator->mFilter->callback( &sResult,
                                                aIterator->curRecPtr,
                                                NULL,
                                                0,
                                                sGRID,
                                                aIterator->mFilter->data )
                  != IDE_SUCCESS );

        if( sResult == ID_TRUE )
        {
            if( aIterator->mProperties->mFirstReadRecordPos == 0 )
            {
                if( aIterator->mProperties->mReadRecordCount != 0 )
                {
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

    IDE_EXCEPTION_CONT( skip_no_more_row );

    if( sIsFetchSuccess == ID_TRUE )
    {
        aIterator->lstFetchRecPtr = aIterator->curRecPtr;

        *aRow = aIterator->curRecPtr;
        SC_COPY_GRID( sGRID, aIterator->mRowGRID );
    }
    else
    {
        aIterator->curRecPtr = NULL;
        aIterator->lstFetchRecPtr = NULL;
        SC_MAKE_NULL_GRID( aIterator->mRowGRID );
        *aRow = NULL;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

static IDE_RC svnpFetchPrev( svnpIterator   * aIterator,
                             const void    ** aRow )
{
    smOID       sRowOID;
    idBool      sResult;
    idBool      sIsVisibleRow;
    scGRID      sGRID;
    idBool      sIsValidGRID;
    idBool      sIsFetchSuccess = ID_FALSE;

    if( (aIterator->mProperties->mReadRecordCount == 0) ||
        (aIterator->mNxtRange == NULL) )
    {
        IDE_CONT( skip_no_more_row );
    }
    else
    {
        /* do nothing */
    }

    IDE_TEST( iduCheckSessionEvent(aIterator->mProperties->mStatistics)
              != IDE_SUCCESS);

    SC_MAKE_NULL_GRID( sGRID );

    while( aIterator->mNxtRange != NULL )
    {
        sGRID = *((scGRID*)aIterator->mNxtRange->minimum.data);
        aIterator->mNxtRange = aIterator->mNxtRange->prev;

        IDE_TEST( svnpValidateGRID( aIterator->table, sGRID, &sIsValidGRID )
                  != IDE_SUCCESS );

        if( sIsValidGRID == ID_FALSE )
        {
            continue;
        }
        else
        {
            /* do nothing */
        }

        sRowOID = SM_MAKE_OID( SC_MAKE_PID(sGRID), SC_MAKE_OFFSET(sGRID) );

        IDE_TEST( svnpGetValidVersion( aIterator,
                                       sRowOID,
                                       (void**)&aIterator->curRecPtr,
                                       &sIsVisibleRow )
                  != IDE_SUCCESS );

        if( sIsVisibleRow == ID_FALSE)
        {
            continue;
        }
        else
        {
            /* do nothing */
        }

        IDE_TEST( aIterator->mFilter->callback( &sResult,
                                                aIterator->curRecPtr,
                                                NULL,
                                                0,
                                                sGRID,
                                                aIterator->mFilter->data )
                  != IDE_SUCCESS );

        if( sResult == ID_TRUE )
        {
            if( aIterator->mProperties->mFirstReadRecordPos == 0 )
            {
                if( aIterator->mProperties->mReadRecordCount != 0 )
                {
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

    IDE_EXCEPTION_CONT( skip_no_more_row );

    if( sIsFetchSuccess == ID_TRUE )
    {
        aIterator->lstFetchRecPtr = aIterator->curRecPtr;

        *aRow = aIterator->curRecPtr;
        SC_COPY_GRID( sGRID, aIterator->mRowGRID );
    }
    else
    {
        aIterator->curRecPtr = NULL;
        aIterator->lstFetchRecPtr = NULL;
        SC_MAKE_NULL_GRID( aIterator->mRowGRID );
        *aRow = NULL;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}                                                                             

static IDE_RC svnpFetchNextU( svnpIterator   * aIterator,
                              const void    ** aRow )
{
    smOID       sRowOID;
    idBool      sResult;
    idBool      sIsVisibleRow;
    scGRID      sGRID;
    idBool      sIsValidGRID;
    idBool      sIsFetchSuccess = ID_FALSE;


    if( (aIterator->mProperties->mReadRecordCount == 0) ||
        (aIterator->mNxtRange == NULL) )
    {
        IDE_CONT( skip_no_more_row );
    }
    else
    {
        /* do nothing */
    }

    IDE_TEST( iduCheckSessionEvent(aIterator->mProperties->mStatistics)
              != IDE_SUCCESS);

    SC_MAKE_NULL_GRID( sGRID );

    while( aIterator->mNxtRange != NULL )
    {
        sGRID = *((scGRID*)aIterator->mNxtRange->minimum.data);
        aIterator->mNxtRange = aIterator->mNxtRange->next;

        IDE_TEST( svnpValidateGRID( aIterator->table, sGRID, &sIsValidGRID )
                  != IDE_SUCCESS );

        if( sIsValidGRID == ID_FALSE )
        {
            continue;
        }
        else
        {
            /* do nothing */
        }

        sRowOID = SM_MAKE_OID( SC_MAKE_PID(sGRID), SC_MAKE_OFFSET(sGRID) );

        IDE_TEST( svnpGetValidVersion( aIterator,
                                       sRowOID,
                                       (void**)&aIterator->curRecPtr,
                                       &sIsVisibleRow )
                  != IDE_SUCCESS );

        if( sIsVisibleRow == ID_FALSE)
        {
            continue;
        }
        else
        {
            /* do nothing */
        }

        IDE_TEST( aIterator->mFilter->callback( &sResult,
                                                aIterator->curRecPtr,
                                                NULL,
                                                0,
                                                sGRID,
                                                aIterator->mFilter->data )
                  != IDE_SUCCESS );

        if( sResult == ID_TRUE )
        {
            if( SM_SCN_IS_DELETED(
                    ((smpSlotHeader*)(aIterator->curRecPtr))->mCreateSCN) )
            {
                continue;
            }

            if( aIterator->mProperties->mFirstReadRecordPos == 0 )
            {
                if( aIterator->mProperties->mReadRecordCount != 0 )
                {
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

    IDE_EXCEPTION_CONT( skip_no_more_row );

    if( sIsFetchSuccess == ID_TRUE )
    {
        aIterator->lstFetchRecPtr = aIterator->curRecPtr;

        smnManager::updatedRow( (smiIterator*)aIterator );

        *aRow = aIterator->curRecPtr;
        SC_COPY_GRID( sGRID, aIterator->mRowGRID );
    }
    else
    {
        aIterator->curRecPtr = NULL;
        aIterator->lstFetchRecPtr = NULL;
        SC_MAKE_NULL_GRID( aIterator->mRowGRID );
        *aRow = NULL;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

static IDE_RC svnpFetchPrevU( svnpIterator   * aIterator,
                              const void    ** aRow )
{
    smOID       sRowOID;
    idBool      sResult;
    idBool      sIsVisibleRow;
    scGRID      sGRID;
    idBool      sIsValidGRID;
    idBool      sIsFetchSuccess = ID_FALSE;

    if( (aIterator->mProperties->mReadRecordCount == 0) ||
        (aIterator->mNxtRange == NULL) )
    {
        IDE_CONT( skip_no_more_row );
    }
    else
    {
        /* do nothing */
    }

    IDE_TEST( iduCheckSessionEvent(aIterator->mProperties->mStatistics)
              != IDE_SUCCESS);

    SC_MAKE_NULL_GRID( sGRID );

    while( aIterator->mNxtRange != NULL )
    {
        sGRID = *((scGRID*)aIterator->mNxtRange->minimum.data);
        aIterator->mNxtRange = aIterator->mNxtRange->prev;

        IDE_TEST( svnpValidateGRID( aIterator->table, sGRID, &sIsValidGRID )
                  != IDE_SUCCESS );

        if( sIsValidGRID == ID_FALSE )
        {
            continue;
        }
        else
        {
            /* do nothing */
        }

        sRowOID = SM_MAKE_OID( SC_MAKE_PID(sGRID), SC_MAKE_OFFSET(sGRID) );

        IDE_TEST( svnpGetValidVersion( aIterator,
                                       sRowOID,
                                       (void**)&aIterator->curRecPtr,
                                       &sIsVisibleRow )
                  != IDE_SUCCESS );

        if( sIsVisibleRow == ID_FALSE)
        {
            continue;
        }
        else
        {
            /* do nothing */
        }

        IDE_TEST( aIterator->mFilter->callback( &sResult,
                                                aIterator->curRecPtr,
                                                NULL,
                                                0,
                                                sGRID,
                                                aIterator->mFilter->data )
                  != IDE_SUCCESS );

        if( sResult == ID_TRUE )
        {
            if( SM_SCN_IS_DELETED(
                    ((smpSlotHeader*)(aIterator->curRecPtr))->mCreateSCN) )
            {
                continue;
            }

            if( aIterator->mProperties->mFirstReadRecordPos == 0 )
            {
                if( aIterator->mProperties->mReadRecordCount != 0 )
                {
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

    IDE_EXCEPTION_CONT( skip_no_more_row );

    if( sIsFetchSuccess == ID_TRUE )
    {
        aIterator->lstFetchRecPtr = aIterator->curRecPtr;

        smnManager::updatedRow( (smiIterator*)aIterator );

        *aRow = aIterator->curRecPtr;
        SC_COPY_GRID( sGRID, aIterator->mRowGRID );
    }
    else
    {
        aIterator->curRecPtr = NULL;
        aIterator->lstFetchRecPtr = NULL;
        SC_MAKE_NULL_GRID( aIterator->mRowGRID );
        *aRow = NULL;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}                                                                             

static IDE_RC svnpFetchNextR( svnpIterator  * aIterator)
{
    smOID       sRowOID;
    idBool      sResult;
    idBool      sIsVisibleRow;
    scGRID      sGRID;
    idBool      sIsValidGRID;
    /* BUG-39836 : ���ʰ� ���� ���� �߰� */ 
    ULong           sReadRecordCountOrigin;
    ULong           sFirstReadRecordPosOrigin; 
    const smiRange *sNxtRangeOrigin;

    /* BUG-39836 : repeatable read��忡�� smnp/svnpFetchNextR�Լ��� fetch�� 
     * row�� ��ȸ�ϰ� lock�� ��´�. ����� �Ǵ� row�� ��ȸ�ϱ� ���� aIterator->mProperties��
     * mReadRecordCount�� mFirstReadRecordPos, mNxtRange���� �̿��Ѵ�. �� �Լ�������
     * lock�� ��� ���� fetch�� smnp/svnpFetchNext���� �����Ѵ�. (smnp/svnpSeekFunctions ����)
     * ���� mReadRecordCount�� mFirstReadRecordPos, mNxtRange�� ���� ���� �����ϰ�
     * �� �Լ��� ���� �Ǵ� ������ �� �������� ���� ���ʰ����� ���� ���� �־�� �Ѵ�.
     */
    sReadRecordCountOrigin    = aIterator->mProperties->mReadRecordCount;
    sFirstReadRecordPosOrigin = aIterator->mProperties->mFirstReadRecordPos; 
    sNxtRangeOrigin           = aIterator->mNxtRange;

    if( (aIterator->mProperties->mReadRecordCount == 0) ||
        (aIterator->mNxtRange == NULL) )
    {
        IDE_CONT( skip_no_more_row );
    }
    else
    {
        /* do nothing */
    }

    while( aIterator->mNxtRange != NULL )
    {
        IDE_TEST( iduCheckSessionEvent(aIterator->mProperties->mStatistics)
                  != IDE_SUCCESS);

        sGRID = *((scGRID*)aIterator->mNxtRange->minimum.data);
        aIterator->mNxtRange = aIterator->mNxtRange->next;

        IDE_TEST( svnpValidateGRID( aIterator->table, sGRID, &sIsValidGRID )
                  != IDE_SUCCESS );

        if( sIsValidGRID == ID_FALSE )
        {
            continue;
        }
        else
        {
            /* do nothing */
        }

        sRowOID = SM_MAKE_OID( SC_MAKE_PID(sGRID), SC_MAKE_OFFSET(sGRID) );

        IDE_TEST( svnpGetValidVersion( aIterator,
                                       sRowOID,
                                       (void**)&aIterator->curRecPtr,
                                       &sIsVisibleRow )
                  != IDE_SUCCESS );

        if( sIsVisibleRow == ID_FALSE)
        {
            continue;
        }
        else
        {
            /* do nothing */
        }

        IDE_TEST( aIterator->mFilter->callback( &sResult,
                                                aIterator->curRecPtr,
                                                NULL,
                                                0,
                                                sGRID,
                                                aIterator->mFilter->data )
                  != IDE_SUCCESS );

        if( sResult == ID_TRUE )
        {
            if( aIterator->mProperties->mFirstReadRecordPos == 0 )
            {
                if( aIterator->mProperties->mReadRecordCount != 0 )
                {
                    aIterator->mProperties->mReadRecordCount--;

                    aIterator->lstFetchRecPtr = aIterator->curRecPtr;
                    IDE_TEST( smnManager::lockRow( (smiIterator*)aIterator )
                              != IDE_SUCCESS );
                }
            }
            else
            {
                aIterator->mProperties->mFirstReadRecordPos--;
            }
        }
    }

    IDE_EXCEPTION_CONT( skip_no_more_row );

    aIterator->curRecPtr = NULL;
    aIterator->lstFetchRecPtr = NULL;
    SC_MAKE_NULL_GRID( aIterator->mRowGRID );

    /* BUG-39836 : mReadRecordCount�� mFirstReadRecordPos, mNxtRange�� ���� ������ ���� */
    aIterator->mProperties->mReadRecordCount    = sReadRecordCountOrigin;
    aIterator->mProperties->mFirstReadRecordPos = sFirstReadRecordPosOrigin;
    aIterator->mNxtRange                        = sNxtRangeOrigin;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


static IDE_RC svnpAllocIterator( void  ** /* aIteratorMem */ )
{
    return IDE_SUCCESS;
}


static IDE_RC svnpFreeIterator( void * /* aIteratorMem */ )
{
    return IDE_SUCCESS;
}

/*******************************************************************************
 * Description: smpSlotHeader�� next�� ���󰡼� �ڽ��� ���� �� �ִ� visible��
 *              version�� ���� ���, ���� �� �ִ� version�� row pointer�� ��ȯ.
 *              ���� �� �ִ� version�� ���� ���, aIsVisibleRow�� ID_FALSE��
 *              �����Ͽ� ��ȯ.
 *
 * Parameters:
 *  - aIterator     [IN] Iterator
 *  - aFstOID       [IN] ���� ������ record�� OID
 *  - aRow          [OUT] �о�� record�� pointer
 *  - aIsVisibleRow [OUT] aRow�� pointer�� visible�� record�� ����Ű�� �ִ���
 ******************************************************************************/
static IDE_RC svnpGetValidVersion( svnpIterator  * aIterator,
                                   smOID           aFstOID,
                                   void         ** aRow,
                                   idBool        * aIsVisibleRow )
{
    scSpaceID        sSpaceID  = aIterator->table->mSpaceID;
    smOID            sCurOID   = aFstOID;
    smOID            sNxtOID;
    smpSlotHeader  * sRow;
    idBool           sLocked   = ID_FALSE;
    idBool           sIsVisible;

    IDE_ERROR( aRow != NULL );
    IDE_ERROR( aIsVisibleRow != NULL );


    *aIsVisibleRow = ID_FALSE;

    IDE_TEST( svmManager::holdPageSLatch( sSpaceID,
                                          SM_MAKE_PID(aFstOID) )
              != IDE_SUCCESS );
    sLocked = ID_TRUE;

    while( sCurOID != SM_NULL_OID )
    {
        IDE_ASSERT( smmManager::getOIDPtr( sSpaceID,
                                           sCurOID,
                                           (void**)&sRow )
                    == IDE_SUCCESS );

        IDE_TEST( smnManager::checkSCN( (smiIterator*)aIterator, sRow, NULL, &sIsVisible )
                  != IDE_SUCCESS );
        if( sIsVisible == ID_TRUE )
        {
            *aRow = sRow;
            *aIsVisibleRow = ID_TRUE;
            break;
        }
        else
        {
            sNxtOID = SMP_SLOT_GET_NEXT_OID( sRow );

            if( SM_IS_VALID_OID( sNxtOID ) == ID_TRUE )
            {
                sCurOID = sNxtOID;
                continue;
            }
            else
            {
                *aRow = NULL;
                *aIsVisibleRow = ID_FALSE;
                break;
            }
        }
    }

    sLocked = ID_FALSE;
    IDE_TEST( svmManager::releasePageLatch( sSpaceID,
                                            SM_MAKE_PID(aFstOID) )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    IDE_PUSH();
    if( sLocked == ID_TRUE )
    {
        IDE_ASSERT( svmManager::releasePageLatch( sSpaceID,
                                                  SM_MAKE_PID(aFstOID) )
                    == IDE_SUCCESS );
    }
    IDE_POP();

    return IDE_FAILURE;
}

/*******************************************************************************
 * Description: MRDB�� ���� fetch by GRID�� ������ �� ��� GRID�� ��ȿ����
 *              Ȯ���ϴ� �Լ�
 *
 * Parameters:
 *  - aTableHdr     [IN] Fetch ��� table�� table header
 *  - aGRID         [IN] Fetch ��� record�� GRID
 *  - aIsValidGRID  [OUT] GRID�� ��ȿ���� ����
 ******************************************************************************/
static IDE_RC svnpValidateGRID( smcTableHeader     * aTableHdr,
                                scGRID               aGRID,
                                idBool             * aIsValidGRID )
{
    scSpaceID            sSpaceID;
    scPageID             sPageID;
    scOffset             sOffset;
    svmTBSNode         * sTBSNode;
    vULong               sMaxPageCnt;
    smpPersPageHeader  * sPageHdrPtr;
    vULong               sSlotSize;
    svmPageState         sPageState;

    *aIsValidGRID = ID_FALSE;

    /* ���� �� �ִ� GRID���� �˻� */
    IDE_TEST_CONT( SC_GRID_IS_NULL(aGRID), error_invalid_grid );
    IDE_TEST_CONT( SC_GRID_IS_WITH_SLOTNUM(aGRID), error_invalid_grid );

    sSpaceID = SC_MAKE_SPACE(aGRID);
    sPageID  = SC_MAKE_PID(aGRID);
    sOffset  = SC_MAKE_OFFSET(aGRID);

    /* GRID�� table header�� SpaceID ��ġ �˻� */
    IDE_TEST_CONT( sSpaceID != aTableHdr->mSpaceID,
                    error_invalid_grid );

    IDE_TEST( sctTableSpaceMgr::findSpaceNodeBySpaceID( sSpaceID,
                                                        (void**)&sTBSNode )
              != IDE_SUCCESS );

    sMaxPageCnt = svmDatabase::getAllocPersPageCount( &sTBSNode->mMemBase );

    /* GRID�� PageID�� ��ȿ�� PageID ���� ������ Ȯ�� */
    IDE_TEST_CONT( !(sPageID < sMaxPageCnt), error_invalid_grid );

    /* ��� page�� table�� �Ҵ� �Ǿ� �ִ� data page�� �´��� Ȯ�� */
    IDE_TEST( svmExpandChunk::getPageState( sTBSNode,
                                            sPageID,
                                            &sPageState )
              != IDE_SUCCESS );

    IDE_TEST_CONT( sPageState != SVM_PAGE_STATE_ALLOC, error_invalid_grid );

    IDE_TEST( smmManager::getPersPagePtr( sSpaceID,
                                          sPageID,
                                          (void**)&sPageHdrPtr )
              != IDE_SUCCESS );

    /* GRID�� page�� TableOID�� ��ġ�ϴ���, fixed page�� �´��� Ȯ�� */
    IDE_TEST_CONT( ( sPageHdrPtr->mTableOID != aTableHdr->mSelfOID ) &&
                    ( SMP_GET_PERS_PAGE_TYPE(sPageHdrPtr) ==
                            SMP_PAGETYPE_FIX ),
                    error_invalid_grid );

    /* offset�� slot size�� ���� align�� ���� ��ȿ�� offset ������ Ȯ�� */
    sSlotSize = aTableHdr->mFixed.mVRDB.mSlotSize;

    IDE_TEST_CONT( ((sOffset - ID_SIZEOF(smpPersPageHeader)) % sSlotSize)
                        != 0,
                    error_invalid_grid )


    /* ��� �˻縦 ����Ͽ���. */
    *aIsValidGRID = ID_TRUE;

    IDE_EXCEPTION_CONT( error_invalid_grid );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

