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
 * $Id: qcm.cpp 90270 2021-03-21 23:20:18Z bethy $
 **********************************************************************/

#include <idl.h>
#include <ide.h>
#include <smDef.h>
#include <smcDef.h>
#include <qcm.h>
#include <qcmUser.h>
#include <qcmPriv.h>
#include <qcmCreate.h>
#include <qcmDatabase.h>
#include <qcmCache.h>
#include <qcmSynonym.h>
#include <qcmTableInfo.h>
#include <qcmTableSpace.h>
#include <qcmTrigger.h>
#include <qcmView.h>
#include <qcg.h>
#include <qtc.h>
#include <qcpManager.h>
#include <qcuProperty.h>
#include <qdParseTree.h>
#include <qsxProc.h>
#include <mtcDef.h>
#include <mtdTypes.h>
#include <qcmProc.h>
#include <qcmPkg.h>
#include <qcmPartition.h>
#include <qcmDatabaseLinks.h>
#include <qmoPartition.h>
#include <qds.h>
#include <qcgPlan.h>
#include <qdnCheck.h>
#include <qmsIndexTable.h>
#include <qmsDefaultExpr.h>
#include <qmvQuerySet.h>
#include <mtlTerritory.h>
#include <mtz.h>
#include <qcmLibrary.h> // PROJ-1685

//----------------------------------------------
// External Module
//----------------------------------------------
#include <smiTableSpace.h>

qcmAllExternModule gExternModule = {0, {NULL,} };

/* this variable must initialized during qcm::init */
qcmTableInfo *gQcmIndicesTableInfo;

const void * gQcmTables;
const void * gQcmColumns;
const void * gQcmIndices;
const void * gQcmConstraints;
const void * gQcmConstraintColumns;
const void * gQcmIndexColumns;
const void * gQcmIsamKeyDesc;
const void * gQcmIsamKeyPart;

// PROJ-1362
const void * gQcmLobs;

// PROJ-2002 Column Security
const void * gQcmSecurity;
const void * gQcmEncryptedColumns;

const void * gQcmTablesIndex[QCM_MAX_META_INDICES];
const void * gQcmColumnsIndex[QCM_MAX_META_INDICES];
const void * gQcmConstraintsIndex[QCM_MAX_META_INDICES];
const void * gQcmConstraintColumnsIndex[QCM_MAX_META_INDICES];
const void * gQcmIndexColumnsIndex[QCM_MAX_META_INDICES];
const void * gQcmIndicesIndex[QCM_MAX_META_INDICES];

// PROJ-1362
const void * gQcmLobsIndex[QCM_MAX_META_INDICES];

// PROJ-2002 Column Security
const void * gQcmEncryptedColumnsIndex[QCM_MAX_META_INDICES];

/* PROJ-2240 Replication module */
const void * gQcmReplications;
const void * gQcmReplicationsIndex [ QCM_MAX_META_INDICES ];
const void * gQcmReplReceiver;
const void * gQcmReplReceiverIndex [ QCM_MAX_META_INDICES ];
const void * gQcmReplHosts;
const void * gQcmReplHostsIndex [ QCM_MAX_META_INDICES ];
const void * gQcmReplItems;
const void * gQcmReplItemsIndex [ QCM_MAX_META_INDICES ];
const void * gQcmReplItemsHistory;
const void * gQcmReplRecoveryInfos;

/* PROJ-1442 Replication Online �� DDL ��� */
const void * gQcmReplOldItems;
const void * gQcmReplOldItemsIndex  [QCM_MAX_META_INDICES];
const void * gQcmReplOldCols;
const void * gQcmReplOldColsIndex   [QCM_MAX_META_INDICES];
const void * gQcmReplOldIdxs;
const void * gQcmReplOldIdxsIndex   [QCM_MAX_META_INDICES];
const void * gQcmReplOldIdxCols;
const void * gQcmReplOldIdxColsIndex[QCM_MAX_META_INDICES];

/* PROJ-2642 */
const void * gQcmReplOldChecks = NULL;
const void * gQcmReplOldChecksIndex [QCM_MAX_META_INDICES] = { NULL, };
const void * gQcmReplOldCheckColumns = NULL;
const void * gQcmReplOldCheckColumnsIndex[QCM_MAX_META_INDICES] = { NULL, };

/* PROJ-1915 Off-line replicator */
const void * gQcmReplOfflineDirs;
const void * gQcmReplOfflineDirsIndex [QCM_MAX_META_INDICES];


const void * gQcmReplItemReplaceHistory;
const void * gQcmReplItemReplaceHistoryIndex [ QCM_MAX_META_INDICES ];

/* PROJ-2422 srid */
const void * gQcmGeometries;
const void * gQcmGeometriesIndex[QCM_MAX_META_INDICES];

/* TASK-7217 Sharded sequence */
SLong gShardNodeID[QCM_MAX_SHARDED_SEQUENCE_SCALE] = {0, };

/* PROJ-2240 Untill here */

SChar      * gDBSequenceName[] =
{
    (SChar*) "NEXT_TABLE_ID"
    , (SChar*) "NEXT_USER_ID"
    , (SChar*) "NEXT_INDEX_ID"
    , (SChar*) "NEXT_CONSTRAINT_ID"
    , (SChar*) "NEXT_DIRECTORY_ID"
    , (SChar*) "NEXT_TABLE_PARTITION_ID"
    , (SChar*) "NEXT_INDEX_PARTITION_ID"
    , (SChar*) "NEXT_DATABASE_LINK_ID"
    , (SChar*) "NEXT_MATERIALIZED_VIEW_ID"
    , (SChar*) "NEXT_LIBRARY_ID" // PROJ-1685
    , (SChar*) "NEXT_DICTIONARY_ID" // PROJ-2264 Dictionary table
    , (SChar*) "NEXT_JOB_ID"     /* PROJ-1438 Job Scheduler */
};

smiCursorProperties gMetaDefaultCursorProperty = {
    ID_ULONG_MAX,
    0,
    ID_ULONG_MAX,
    ID_TRUE,
    NULL,
    NULL,
    { NULL, NULL },
    0,
    NULL,
    NULL,
    0,
    SMI_BUILTIN_SEQUENTIAL_INDEXTYPE_ID,
    {1, 1, 0}
};

extern "C" SInt
compareQcmColumn( const void* aElem1, const void* aElem2 )
{
/***********************************************************************
 *
 * Description :
 *    compare qcmColumn
 *
 * Implementation :
 *
 ***********************************************************************/

    IDE_DASSERT( aElem1 != NULL );
    IDE_DASSERT( aElem2 != NULL );

    //--------------------------------
    // compare columnID
    //--------------------------------

    if( ((qcmColumn*)aElem1)->basicInfo->column.id >
        ((qcmColumn*)aElem2)->basicInfo->column.id )
    {
        return 1;
    }
    else if( ((qcmColumn*)aElem1)->basicInfo->column.id <
             ((qcmColumn*)aElem2)->basicInfo->column.id )
    {
        return -1;
    }
    else
    {
        return 0;
    }
}

IDE_RC qcm::check(smiStatement * aSmiStmt)
{
    //----------------------------------------------------
    // 1. Check that META tables exist.
    // 2. Check Disk Edition
    // 3. Check META Version
    // 4. If necessary, upgrade META tables.
    //----------------------------------------------------

    const void            * sTable;
    ULong                 * sTableInfo;

    //----------------------------------------------------
    // 1. Check that META tables exist.
    //----------------------------------------------------

    sTableInfo = (ULong *)smiGetTableInfo( smiGetCatalogTable() );

    IDE_TEST_RAISE( sTableInfo == NULL, ERR_QCM_INIT_DB);

    sTable = smiGetTable( (smOID)(*sTableInfo) );

    // initalize global variable
    (void)initializeGlobalVariables( sTable );

    //----------------------------------------------------
    // 2. Check Disk Edition
    //----------------------------------------------------
    /* PROJ-2639 Altibase Disk Edition */
#ifdef ALTI_CFG_EDITION_DISK
    IDE_TEST( checkDiskEdition( aSmiStmt ) != IDE_SUCCESS );

    // initalize global variable
    (void)initializeGlobalVariables( sTable );
#endif

    //----------------------------------------------------
    // 3. Check META Version
    // 4. If necessary, upgrade META tables.
    //----------------------------------------------------
    IDE_TEST( checkMetaVersionAndUpgrade( aSmiStmt ) != IDE_SUCCESS);

    // initalize global variable
    (void)initializeGlobalVariables( sTable );

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_QCM_INIT_DB);
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QCM_INITDB));
    }
    IDE_EXCEPTION_END;

    // To Fix PR-11656
    // Global ������ �ݵ�� �ʱ�ȭ�Ǿ�� ��.
    // Create DB => Drop DB => Create DB (��⼭ ���� �߻���)
    initializeGlobalVariables( NULL );

    return IDE_FAILURE;
}

void qcm::initializeGlobalVariables( const void * aTable )
{
    SInt              i;

    gQcmUsersTempInfo    = NULL;
    gQcmDNUsersTempInfo  = NULL;
    gQcmIndicesTableInfo = NULL;

    gQcmUsers                 = NULL;
    gQcmDNUsers               = NULL;
    gQcmTables                = aTable;
    gQcmDatabase              = NULL;
    gQcmColumns               = NULL;
    gQcmIndices               = NULL;
    gQcmConstraints           = NULL;
    gQcmConstraintColumns     = NULL;
    gQcmIndexColumns          = NULL;
    gQcmProcedures            = NULL;
    gQcmProcParas             = NULL;
    gQcmProcParse             = NULL;
    gQcmProcRelated           = NULL;

    // PROJ-1073 Package
    gQcmPkgs                  = NULL;
    gQcmPkgParas              = NULL;
    gQcmPkgParse              = NULL;
    gQcmPkgRelated            = NULL;

    // PROJ-1359 Trigger
    gQcmTriggers              = NULL;
    gQcmTriggerStrings        = NULL;
    gQcmTriggerUpdateColumns  = NULL;
    gQcmTriggerDmlTables      = NULL;

    // PROJ-1076 Synonym
    gQcmSynonyms              = NULL;

    // PROJ-1362
    gQcmLobs                  = NULL;

    // PROJ-2002 Column Security
    gQcmSecurity              = NULL;
    gQcmEncryptedColumns      = NULL;

#if !defined(SMALL_FOOTPRINT)
    gQcmReplications          = NULL;
    gQcmReplReceiver          = NULL;
    gQcmReplHosts             = NULL;
    gQcmReplItems             = NULL;
    gQcmReplItemsHistory      = NULL;
    // PROJ-1442 Replication Online �� DDL ���
    gQcmReplOldItems          = NULL;
    gQcmReplOldCols           = NULL;
    gQcmReplOldIdxs           = NULL;
    gQcmReplOldIdxCols        = NULL;

    gQcmReplOldChecks         = NULL;
    gQcmReplOldCheckColumns   = NULL;

    /* PROJ-1915 */
    gQcmReplOfflineDirs       = NULL;

    gQcmReplRecoveryInfos     = NULL;
    
    gQcmReplItemReplaceHistory       = NULL;
#endif

    gQcmPrivileges            = NULL;
    gQcmGrantSystem           = NULL;
    gQcmGrantObject           = NULL;
    gQcmIsamKeyDesc           = NULL;
    gQcmIsamKeyPart           = NULL;
    gQcmXaHeuristicTrans      = NULL;
    gQcmViews                 = NULL;
    gQcmViewParse             = NULL;
    gQcmViewRelated           = NULL;

    // PROJ-1502 PARTITIONED DISK TABLE
    gQcmPartTables            = NULL;
    gQcmPartIndices           = NULL;
    gQcmTablePartitions       = NULL;
    gQcmIndexPartitions       = NULL;
    gQcmPartKeyColumns        = NULL;
    gQcmPartLobs              = NULL;

    // BUG-21387 COMMENT
    gQcmComments              = NULL;

    /* PROJ-2207 Password policy support */
    gQcmPasswordHistory       = NULL;

    /* PROJ-2211 Materialized View */
    gQcmMaterializedViews     = NULL;

    // PROJ-2223
    gQcmAudit                 = NULL;
    gQcmAuditAllOpts          = NULL;

    /* PROJ-2264 */
    gQcmCompressionTables     = NULL;

    /* BUG-35445 Check Constraint, Function-Based Index���� ��� ���� Function�� ����/���� ���� */
    gQcmConstraintRelated     = NULL;
    gQcmIndexRelated          = NULL;

    /* PROJ-1438 Job Scheduler */
    gQcmJobs                  = NULL;

    /* PROJ-1812 ROLE */
    gQcmUserRoles             = NULL;

    /* TASK-7217 Sharded sequence */
    for (i = 0; i < QCM_MAX_SHARDED_SEQUENCE_SCALE; i++)
    {
        gShardNodeID[i] = 0;
    }

    for (i = 0; i < QCM_MAX_META_INDICES; i ++)
    {
        gQcmTablesIndex[i]                = NULL;
        gQcmColumnsIndex[i]               = NULL;
        gQcmConstraintsIndex[i]           = NULL;
        gQcmConstraintColumnsIndex[i]     = NULL;
        gQcmIndexColumnsIndex[i]          = NULL;
        gQcmUsersIndex[i]                 = NULL;
        gQcmDNUsersIndex[i]               = NULL;
        gQcmTBSUsersIndex[i]              = NULL;
        gQcmIndicesIndex[i]               = NULL;

#if !defined(SMALL_FOOTPRINT)
        gQcmReplicationsIndex[i]          = NULL;
        gQcmReplHostsIndex[i]             = NULL;
        gQcmReplItemsIndex[i]             = NULL;

        // PROJ-1442 Replication Online �� DDL ���
        gQcmReplOldItemsIndex[i]          = NULL;
        gQcmReplOldColsIndex[i]           = NULL;
        gQcmReplOldIdxsIndex[i]           = NULL;
        gQcmReplOldIdxColsIndex[i]        = NULL;

        gQcmReplOldChecksIndex[i]         = NULL;
        gQcmReplOldCheckColumnsIndex[i]   = NULL;

        /* PROJ-1915 */
        gQcmReplOfflineDirsIndex[i]       = NULL;
        
        gQcmReplItemReplaceHistoryIndex[i]       = NULL;
#endif

        gQcmProceduresIndex[i]            = NULL;
        gQcmProcParasIndex[i]             = NULL;
        gQcmProcParseIndex[i]             = NULL;
        gQcmProcRelatedIndex[i]           = NULL;

        // PROJ-1073 Package
        gQcmPkgsIndex[i]                  = NULL;
        gQcmPkgParasIndex[i]              = NULL;
        gQcmPkgParseIndex[i]              = NULL;
        gQcmPkgRelatedIndex[i]            = NULL;

        // PROJ-1359 Trigger
        gQcmTriggersIndex[i]              = NULL;
        gQcmTriggerStringsIndex[i]        = NULL;
        gQcmTriggerUpdateColumnsIndex[i]  = NULL;
        gQcmTriggerDmlTablesIndex[i]      = NULL;

        gQcmPrivilegesIndex[i]            = NULL;
        gQcmGrantSystemIndex[i]           = NULL;
        gQcmGrantObjectIndex[i]           = NULL;
        gQcmXaHeuristicTransIndex[i]      = NULL;
        gQcmViewsIndex[i]                 = NULL;
        gQcmViewParseIndex[i]             = NULL;
        gQcmViewRelatedIndex[i]           = NULL;

        // PROJ-1076 Synonym
        gQcmSynonymsIndex[i]              = NULL;

        // PROJ-1362
        gQcmLobsIndex[i]                  = NULL;

        // PROJ-2002 Column Security
        gQcmEncryptedColumnsIndex[i]      = NULL;

        // PROJ-1502 PARTITIONED DISK TABLE
        gQcmPartTablesIndex[i]            = NULL;
        gQcmPartIndicesIndex[i]           = NULL;
        gQcmTablePartitionsIndex[i]       = NULL;
        gQcmIndexPartitionsIndex[i]       = NULL;
        gQcmPartKeyColumnsIndex[i]        = NULL;
        gQcmPartLobsIndex[i]              = NULL;

        // BUG-21387 COMMENT
        gQcmCommentsIndex[i]              = NULL;

        // PROJ-2207 Password policy support
        gQcmPasswordHistoryIndex[i]       = NULL;

        /* PROJ-2211 Materialized View */
        gQcmMaterializedViewsIndex[i]     = NULL;

        // PROJ-2223 audit
        gQcmAuditAllOptsIndex[i]          = NULL;

        /* PROJ-2264 */
        gQcmCompressionTablesIndex[i]     = NULL;

        /* BUG-35445 Check Constraint, Function-Based Index���� ��� ���� Function�� ����/���� ���� */
        gQcmConstraintRelatedIndex[i]     = NULL;
        gQcmIndexRelatedIndex[i]          = NULL;

        /* PROJ-1438 Job Scheduler */
        gQcmJobsIndex[i]                  = NULL;

        /* PROJ-1812 ROLE */
        gQcmUserRolesIndex[i]             = NULL;
    }

    // PROJ-1488 Altibase Spatio-Temporal DBMS
    for ( i = 0; i < gExternModule.mCnt; i++ )
    {
        (void) gExternModule.mExternModule[i]->mInitGlobalHandle();
    }
}

IDE_RC qcm::checkMetaVersionAndUpgrade( smiStatement * aSmiStmt )
{
    UInt        sCurrMajorVersion = 0;
    UInt        sCurrMinorVersion = 0;
    UInt        sCurrPatchVersion = 0;

    // get SYS_TABLES_ index handle
    IDE_TEST( getMetaIndex( gQcmTablesIndex,
                            gQcmTables ) != IDE_SUCCESS );

    // get table handle
    IDE_TEST( qcm::getMetaTable( QCM_DATABASE,
                                 & gQcmDatabase,
                                 aSmiStmt ) != IDE_SUCCESS );

    // check META VERSION of DATABASE
    IDE_TEST( qcmDatabase::getMetaVersion( aSmiStmt,
                                           &sCurrMajorVersion,
                                           &sCurrMinorVersion,
                                           &sCurrPatchVersion )
              != IDE_SUCCESS );

    // A META MAJOR VERSION of DATABASE differs from altibase binary.
    if ( sCurrMajorVersion != QCM_META_MAJOR_VER )
    {
        IDE_RAISE(ERR_QCM_META_MAJOR_VERSION_DIFFER);
    }

    // A META MAJOR VERSION of DATABASE differs from altibase binary.
    if ( sCurrMinorVersion != QCM_META_MINOR_VER )
    {

        ideLog::log(IDE_QP_0,"[QCM_META_UPGRADE] DATABASE VERSION = "
                    "%d.%d.%d\n",
                    sCurrMajorVersion, sCurrMinorVersion, sCurrPatchVersion );

        ideLog::log(IDE_QP_0,"[QCM_META_UPGRADE] BINARY VERSION = "
                    "%d.%d.%d\n",
                    QCM_META_MAJOR_VER, QCM_META_MINOR_VER, QCM_META_PATCH_VER );

        if ( sCurrMinorVersion < QCM_META_MINOR_VER )
        {
            // initialize global meta handles ( table, index )
            IDE_TEST( initMetaHandles( aSmiStmt, &sCurrMinorVersion  )
                      != IDE_SUCCESS );

            // make temporary qcmTableInfo of current tables
            IDE_TEST( initMetaCaches( aSmiStmt ) != IDE_SUCCESS );

            // upgrade META : A new minor version is set
            //          in qcmCreate::upgradeMeta
            IDE_TEST( qcmCreate::upgradeMeta( aSmiStmt->mStatistics, sCurrMinorVersion )
                      != IDE_SUCCESS );


            ideLog::log(IDE_QP_0,"[QCM_META_UPGRADE] SUCCESS\n");
        }
        else
        {
            IDE_RAISE(ERR_QCM_META_MINOR_VERSION_LOW);
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_QCM_META_MAJOR_VERSION_DIFFER);
    {

        ideLog::log(IDE_QP_0,"[QCM_META_UPGRADE] DATABASE VERSION = "
                    "%d.%d.%d\n",
                    sCurrMajorVersion, sCurrMinorVersion, sCurrPatchVersion );

        ideLog::log(IDE_QP_0,"[QCM_META_UPGRADE] BINARY VERSION = "
                    "%d.%d.%d\n",
                    QCM_META_MAJOR_VER, QCM_META_MINOR_VER, QCM_META_PATCH_VER );
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QCM_INITDB));
    }
    IDE_EXCEPTION(ERR_QCM_META_MINOR_VERSION_LOW);
    {

        ideLog::log(IDE_QP_0,"[QCM_META_UPGRADE] Binary Minor Version is Low.\n");

        ideLog::log(IDE_QP_0,"[QCM_META_UPGRADE] DATABASE VERSION = "
                    "%d.%d.%d\n",
                    sCurrMajorVersion, sCurrMinorVersion, sCurrPatchVersion );

        ideLog::log(IDE_QP_0,"[QCM_META_UPGRADE] BINARY VERSION = "
                    "%d.%d.%d\n",
                    QCM_META_MAJOR_VER, QCM_META_MINOR_VER, QCM_META_PATCH_VER );
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QCM_INITDB));
    }
    IDE_EXCEPTION_END;


    ideLog::log(IDE_QP_0,"[QCM_META_UPGRADE] FAILURE!!!\n");

    return IDE_FAILURE;
}

IDE_RC qcm::checkDiskEdition( smiStatement * aSmiStmt )
{
    SDouble       sCharBuffer[ (ID_SIZEOF(UShort) + QC_MAX_NAME_LEN + 7) / 8 ] = { (SDouble)0.0, };
    mtdCharType * sResult                         = (mtdCharType *)&sCharBuffer;
    SChar         sSqlBuffer[QD_MAX_SQL_LENGTH]   = { 0, };
    SChar         sMsgBuffer[QC_MAX_NAME_LEN + 1] = { 0, };
    idBool        sRecordExist                    = ID_FALSE;

    // initialize global meta handles ( table, index )
    IDE_TEST( initMetaHandles( aSmiStmt, NULL ) != IDE_SUCCESS );

    // make temporary qcmTableInfo of current tables
    IDE_TEST( initMetaCaches( aSmiStmt ) != IDE_SUCCESS );

    /* User Memory Tablespace */
    idlOS::snprintf( sSqlBuffer,
                     QD_MAX_SQL_LENGTH,
                     "SELECT NAME FROM X$TABLESPACES_HEADER WHERE TYPE = 2 AND STATE_BITSET NOT IN ( 128, 1024 )" );

    IDE_TEST( qcg::runSelectOneRowforDDL( aSmiStmt,
                                          sSqlBuffer,
                                          sResult,
                                          &sRecordExist,
                                          ID_FALSE )
              != IDE_SUCCESS );

    if ( sRecordExist == ID_TRUE )
    {
        IDE_TEST_RAISE( ( sResult->length == 0 ) || ( sResult->length > QC_MAX_NAME_LEN ),
                        ERR_MSG_TOO_LONG );

        idlOS::memcpy( sMsgBuffer, sResult->value, sResult->length );
        sMsgBuffer[sResult->length] = '\0';

        IDE_RAISE( ERR_NOT_SUPPORT_MEMORY_TABLESPACE_IN_DISK_EDITION );
    }
    else
    {
        /* Nothing to do */
    }

    /* User Memory/Volatile Table (Temporary Table�� ����) */
    idlOS::snprintf( sSqlBuffer,
                     QD_MAX_SQL_LENGTH,
                     "SELECT C.NAME"
                     "  FROM SYSTEM_.SYS_TABLES_ A, SYSTEM_.SYS_USERS_ B, X$TABLESPACES_HEADER C"
                     " WHERE A.USER_ID = B.USER_ID AND A.USER_ID <> 1 AND A.TABLE_TYPE IN ( 'T', 'M', 'R' ) AND"
                     "       A.TEMPORARY = 'N' AND A.TBS_ID = C.ID AND C.TYPE IN ( 1, 2, 8 ) AND"
                     "       C.STATE_BITSET NOT IN ( 128, 1024 )" );

    IDE_TEST( qcg::runSelectOneRowforDDL( aSmiStmt,
                                          sSqlBuffer,
                                          sResult,
                                          &sRecordExist,
                                          ID_FALSE )
              != IDE_SUCCESS );

    if ( sRecordExist == ID_TRUE )
    {
        IDE_TEST_RAISE( ( sResult->length == 0 ) || ( sResult->length > QC_MAX_NAME_LEN ),
                        ERR_MSG_TOO_LONG );

        idlOS::memcpy( sMsgBuffer, sResult->value, sResult->length );
        sMsgBuffer[sResult->length] = '\0';

        IDE_RAISE( ERR_NOT_SUPPORT_MEMORY_VOLATILE_TABLESPACE_IN_DISK_EDITION );
    }
    else
    {
        /* Nothing to do */
    }

    /* User Memory/Volatile Partition */
    idlOS::snprintf( sSqlBuffer,
                     QD_MAX_SQL_LENGTH,
                     "SELECT C.NAME"
                     "  FROM SYSTEM_.SYS_TABLES_ A, SYSTEM_.SYS_USERS_ B, X$TABLESPACES_HEADER C,"
                     "       SYSTEM_.SYS_TABLE_PARTITIONS_ D"
                     " WHERE A.USER_ID = B.USER_ID AND A.USER_ID <> 1 AND A.TABLE_TYPE IN ( 'T', 'M', 'R' ) AND"
                     "       A.TEMPORARY = 'N' AND D.TBS_ID = C.ID AND C.TYPE IN ( 1, 2, 8 ) AND"
                     "       C.STATE_BITSET NOT IN ( 128, 1024 ) AND A.TABLE_ID = D.TABLE_ID" );

    IDE_TEST( qcg::runSelectOneRowforDDL( aSmiStmt,
                                          sSqlBuffer,
                                          sResult,
                                          &sRecordExist,
                                          ID_FALSE )
              != IDE_SUCCESS );

    if ( sRecordExist == ID_TRUE )
    {
        IDE_TEST_RAISE( ( sResult->length == 0 ) || ( sResult->length > QC_MAX_NAME_LEN ),
                        ERR_MSG_TOO_LONG );

        idlOS::memcpy( sMsgBuffer, sResult->value, sResult->length );
        sMsgBuffer[sResult->length] = '\0';

        IDE_RAISE( ERR_NOT_SUPPORT_MEMORY_VOLATILE_TABLESPACE_IN_DISK_EDITION );
    }
    else
    {
        /* Nothing to do */
    }

    ideLog::log( IDE_SERVER_0, "[SUCCESS] Disk Edition Validation\n" );

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_MSG_TOO_LONG );
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QCM_MESSAGE_TOO_LONG ) );
    }
    IDE_EXCEPTION( ERR_NOT_SUPPORT_MEMORY_TABLESPACE_IN_DISK_EDITION );
    {
        ideLog::log( IDE_SERVER_0,
                     "[FAILURE] %s is a user memory tablespace.\n",
                     sMsgBuffer );

        IDE_SET( ideSetErrorCode( qpERR_ABORT_QDT_CANNOT_USE_USER_MEMORY_TABLESPACE_IN_DISK_EDITION,
                                  sMsgBuffer ) );
    }
    IDE_EXCEPTION( ERR_NOT_SUPPORT_MEMORY_VOLATILE_TABLESPACE_IN_DISK_EDITION );
    {
        ideLog::log( IDE_SERVER_0,
                     "[FAILURE] %s is used by a user table or partition.\n",
                     sMsgBuffer );

        IDE_SET( ideSetErrorCode( qpERR_ABORT_QDB_USE_ONLY_DISK_TABLE_PARTITION_IN_DISK_EDITION ) );
    }
    IDE_EXCEPTION_END;

    ideLog::log( IDE_SERVER_0, "[FAILURE] Disk Edition Validation!!!\n" );

    return IDE_FAILURE;
}

IDE_RC qcm::initMetaHandles(  smiStatement * aSmiStmt,
                              UInt         * aCurrMinorVersion )
{
    UInt i;
    UInt sColumnCnt;
    mtcColumn * sColumn;

    // �Ź� Zero Dependencies�� �����ϴ� ���� �����ϱ� ���Ͽ�
    // Global�� Zero Dependencies�� ������.
    qtc::dependencyClear( & qtc::zeroDependencies );
    sColumnCnt = smiGetTableColumnCount( gQcmTables);
    //-------------------------------------------
    // get SYS_TABLES_ index handle
    //-------------------------------------------

    IDE_TEST( getMetaIndex( gQcmTablesIndex,
                            gQcmTables ) != IDE_SUCCESS);

    // PROJ-1359 Trigger
    // ���� ���� �� �ѹ��� Module Pointer�� �����ϸ� �ȴ�.

    for ( i = 0; i < sColumnCnt; i++ )
    {
        //PROJ-1362 QP-Large Record & Internal LOB support,
        //table column���� Ǯ��.
        IDE_TEST( smiGetTableColumns( gQcmTables,
                                      i,
                                      (const smiColumn**)&sColumn  )
                  != IDE_SUCCESS );

        // mtdModule ����
        IDE_TEST( mtd::moduleById( &sColumn->module,
                                   sColumn->type.dataTypeId )
                  != IDE_SUCCESS );

        // mtlModule ����
        IDE_TEST( mtl::moduleById( &sColumn->language,
                                   sColumn->type.languageId )
                  != IDE_SUCCESS );
    }

    //-------------------------------------------
    // get table handle
    //-------------------------------------------
    IDE_TEST( qcm::getMetaTable( QCM_DATABASE,
                                 & gQcmDatabase,
                                 aSmiStmt ) != IDE_SUCCESS );

    IDE_TEST( qcm::getMetaTable( QCM_USERS,
                                 & gQcmUsers,
                                 aSmiStmt ) != IDE_SUCCESS );

    IDE_TEST( qcm::getMetaTable( QCM_DN_USERS,
                                 & gQcmDNUsers,
                                 aSmiStmt ) != IDE_SUCCESS );

    IDE_TEST( qcm::getMetaTable( QCM_TBS_USERS,
                                 & gQcmTBSUsers,
                                 aSmiStmt ) != IDE_SUCCESS );

    IDE_TEST( qcm::getMetaTable( QCM_COLUMNS,
                                 & gQcmColumns,
                                 aSmiStmt ) != IDE_SUCCESS);

    IDE_TEST( qcm::getMetaTable( QCM_INDICES,
                                 & gQcmIndices,
                                 aSmiStmt ) != IDE_SUCCESS);

    IDE_TEST( qcm::getMetaTable( QCM_INDEX_COLUMNS,
                                 & gQcmIndexColumns,
                                 aSmiStmt) != IDE_SUCCESS);

    IDE_TEST( qcm::getMetaTable( QCM_CONSTRAINTS,
                                 & gQcmConstraints,
                                 aSmiStmt ) != IDE_SUCCESS);

    IDE_TEST( qcm::getMetaTable( QCM_CONSTRAINT_COLUMNS,
                                 & gQcmConstraintColumns,
                                 aSmiStmt) != IDE_SUCCESS);
    IDE_TEST( qcm::getMetaTable( QCM_PROCEDURES,
                                 & gQcmProcedures,
                                 aSmiStmt ) != IDE_SUCCESS );

    IDE_TEST( qcm::getMetaTable( QCM_PROC_PARAS,
                                 & gQcmProcParas,
                                 aSmiStmt ) != IDE_SUCCESS );

    IDE_TEST( qcm::getMetaTable( QCM_PROC_PARSE,
                                 & gQcmProcParse,
                                 aSmiStmt ) != IDE_SUCCESS );

    IDE_TEST( qcm::getMetaTable( QCM_PROC_RELATED,
                                 & gQcmProcRelated,
                                 aSmiStmt ) != IDE_SUCCESS );

    // PROJ-1073 Package
    IDE_TEST( qcm::getMetaTable( QCM_PKGS,
                                 & gQcmPkgs,
                                 aSmiStmt ) != IDE_SUCCESS );
    IDE_TEST( qcm::getMetaTable( QCM_PKG_PARAS,
                                 & gQcmPkgParas,
                                 aSmiStmt ) != IDE_SUCCESS );
    IDE_TEST( qcm::getMetaTable( QCM_PKG_PARSE,
                                 & gQcmPkgParse,
                                 aSmiStmt ) != IDE_SUCCESS );
    IDE_TEST( qcm::getMetaTable( QCM_PKG_RELATED,
                                 & gQcmPkgRelated,
                                 aSmiStmt ) != IDE_SUCCESS );

    // PROJ-1359 Trigger
    IDE_TEST( qcm::getMetaTable( QCM_TRIGGERS,
                                 & gQcmTriggers,
                                 aSmiStmt ) != IDE_SUCCESS );

    IDE_TEST( qcm::getMetaTable( QCM_TRIGGER_STRINGS,
                                 & gQcmTriggerStrings,
                                 aSmiStmt ) != IDE_SUCCESS );

    IDE_TEST( qcm::getMetaTable( QCM_TRIGGER_UPDATE_COLUMNS,
                                 & gQcmTriggerUpdateColumns,
                                 aSmiStmt ) != IDE_SUCCESS );

    IDE_TEST( qcm::getMetaTable( QCM_TRIGGER_DML_TABLES,
                                 & gQcmTriggerDmlTables,
                                 aSmiStmt ) != IDE_SUCCESS );

    // PROJ-1362 LOB
    IDE_TEST( qcm::getMetaTable( QCM_LOBS,
                                 & gQcmLobs,
                                 aSmiStmt ) != IDE_SUCCESS );

    // PROJ-2002 Column Security
    IDE_TEST( qcm::getMetaTable( QCM_SECURITY,
                                 & gQcmSecurity,
                                 aSmiStmt ) != IDE_SUCCESS );

    IDE_TEST( qcm::getMetaTable( QCM_ENCRYPTED_COLUMNS,
                                 & gQcmEncryptedColumns,
                                 aSmiStmt ) != IDE_SUCCESS );

#if !defined(SMALL_FOOTPRINT)
    IDE_TEST( qcm::getMetaTable( QCM_REPLICATIONS,
                                 & gQcmReplications,
                                 aSmiStmt ) != IDE_SUCCESS );

    IDE_TEST( qcm::getMetaTable( QCM_REPL_RECEIVER,
                                 & gQcmReplReceiver,
                                 aSmiStmt ) != IDE_SUCCESS );

    IDE_TEST( qcm::getMetaTable( QCM_REPL_HOSTS,
                                 & gQcmReplHosts,
                                 aSmiStmt ) != IDE_SUCCESS );

    IDE_TEST( qcm::getMetaTable( QCM_REPL_ITEMS,
                                 & gQcmReplItems,
                                 aSmiStmt ) != IDE_SUCCESS );

    IDE_TEST( qcm::getMetaTable( QCM_REPL_ITEMS_HISTORY,
                                 & gQcmReplItemsHistory,
                                 aSmiStmt ) != IDE_SUCCESS );

    // PROJ-1442 Replication Online �� DDL ���
    IDE_TEST( qcm::getMetaTable( QCM_REPL_OLD_ITEMS,
                                 & gQcmReplOldItems,
                                 aSmiStmt ) != IDE_SUCCESS );

    IDE_TEST( qcm::getMetaTable( QCM_REPL_OLD_COLUMNS,
                                 & gQcmReplOldCols,
                                 aSmiStmt ) != IDE_SUCCESS );

    IDE_TEST( qcm::getMetaTable( QCM_REPL_OLD_INDICES,
                                 & gQcmReplOldIdxs,
                                 aSmiStmt ) != IDE_SUCCESS );

    IDE_TEST( qcm::getMetaTable( QCM_REPL_OLD_INDEX_COLUMNS,
                                 & gQcmReplOldIdxCols,
                                 aSmiStmt ) != IDE_SUCCESS );

    IDE_TEST( qcm::getMetaTable( QCM_REPL_RECOVERY_INFOS,
                                 & gQcmReplRecoveryInfos,
                                 aSmiStmt ) != IDE_SUCCESS );

   /* PROJ-1915 */
    IDE_TEST( qcm::getMetaTable( QCM_REPL_OFFLINE_DIR,
                                 & gQcmReplOfflineDirs,
                                 aSmiStmt ) != IDE_SUCCESS );

    IDE_TEST( qcm::getMetaTable( QCM_REPL_ITEM_REPLACE_HISTORY,
                                 & gQcmReplItemReplaceHistory,
                                 aSmiStmt ) != IDE_SUCCESS );
#endif

    IDE_TEST( qcm::getMetaTable( QCM_PRIVILEGES,
                                 & gQcmPrivileges,
                                 aSmiStmt ) != IDE_SUCCESS );

    IDE_TEST( qcm::getMetaTable( QCM_GRANT_SYSTEM,
                                 & gQcmGrantSystem,
                                 aSmiStmt ) != IDE_SUCCESS );

    IDE_TEST( qcm::getMetaTable( QCM_GRANT_OBJECT,
                                 & gQcmGrantObject,
                                 aSmiStmt ) != IDE_SUCCESS );
    IDE_TEST( qcm::getMetaTable( QCM_XA_HEURISTIC_TRANS,
                                 & gQcmXaHeuristicTrans,
                                 aSmiStmt ) != IDE_SUCCESS );

    IDE_TEST( qcm::getMetaTable( QCM_VIEWS,
                                 & gQcmViews,
                                 aSmiStmt ) != IDE_SUCCESS );

    IDE_TEST( qcm::getMetaTable( QCM_VIEW_PARSE,
                                 & gQcmViewParse,
                                 aSmiStmt ) != IDE_SUCCESS );

    IDE_TEST( qcm::getMetaTable( QCM_VIEW_RELATED,
                                 & gQcmViewRelated,
                                 aSmiStmt ) != IDE_SUCCESS );

    // PROJ-1502 PARTITIONED DISK TABLE - BEGIN -
    IDE_TEST( qcm::getMetaTable( QCM_PART_TABLES,
                                 & gQcmPartTables,
                                 aSmiStmt ) != IDE_SUCCESS );
    IDE_TEST( qcm::getMetaTable( QCM_PART_INDICES,
                                 & gQcmPartIndices,
                                 aSmiStmt ) != IDE_SUCCESS );
    IDE_TEST( qcm::getMetaTable( QCM_TABLE_PARTITIONS,
                                 & gQcmTablePartitions,
                                 aSmiStmt ) != IDE_SUCCESS );
    IDE_TEST( qcm::getMetaTable( QCM_INDEX_PARTITIONS,
                                 & gQcmIndexPartitions,
                                 aSmiStmt ) != IDE_SUCCESS );
    IDE_TEST( qcm::getMetaTable( QCM_PART_KEY_COLUMNS,
                                 & gQcmPartKeyColumns,
                                 aSmiStmt ) != IDE_SUCCESS );
    IDE_TEST( qcm::getMetaTable( QCM_PART_LOBS,
                                 & gQcmPartLobs,
                                 aSmiStmt ) != IDE_SUCCESS );
    // PROJ-1502 PARTITIONED DISK TABLE - END -

    // BUG-21387 COMMENT
    IDE_TEST( qcm::getMetaTable( QCM_COMMENTS,
                                 & gQcmComments,
                                 aSmiStmt ) != IDE_SUCCESS );

    /* PROJ-2207 Password policy support */
    IDE_TEST( qcm::getMetaTable( QCM_PASSWORD_HISTORY,
                                 & gQcmPasswordHistory,
                                 aSmiStmt ) != IDE_SUCCESS );

    /* PROJ-2211 Materialized View */
    IDE_TEST( qcm::getMetaTable( QCM_MATERIALIZED_VIEWS,
                                 & gQcmMaterializedViews,
                                 aSmiStmt ) != IDE_SUCCESS );

    // PROJ-2223 audit
    IDE_TEST( qcm::getMetaTable( QCM_AUDIT,
                                 & gQcmAudit,
                                 aSmiStmt ) != IDE_SUCCESS );
    IDE_TEST( qcm::getMetaTable( QCM_AUDIT_ALL_OPTS,
                                 & gQcmAuditAllOpts,
                                 aSmiStmt ) != IDE_SUCCESS );

    /* PROJ-2264 */
    IDE_TEST( qcm::getMetaTable( QCM_COMPRESSION_TABLES,
                                 & gQcmCompressionTables,
                                 aSmiStmt ) != IDE_SUCCESS );

    /* BUG-35445 Check Constraint, Function-Based Index���� ��� ���� Function�� ����/���� ���� */
    IDE_TEST( qcm::getMetaTable( QCM_CONSTRAINT_RELATED,
                                 & gQcmConstraintRelated,
                                 aSmiStmt ) != IDE_SUCCESS );

    IDE_TEST( qcm::getMetaTable( QCM_INDEX_RELATED,
                                 & gQcmIndexRelated,
                                 aSmiStmt ) != IDE_SUCCESS );

    IDE_TEST( qcm::getMetaTable( QCM_JOBS,
                                 & gQcmJobs,
                                 aSmiStmt ) != IDE_SUCCESS );
    
    // Proj - 1076 Synonym
    IDE_TEST( qcm::getMetaTable( QCM_SYNONYM,
                                 & gQcmSynonyms,
                                 aSmiStmt ) != IDE_SUCCESS );

    // PROJ-1371 Directories
    IDE_TEST( qcm::getMetaTable( QCM_DIRECTORIES,
                                 & gQcmDirectories,
                                 aSmiStmt ) != IDE_SUCCESS );

    // PROJ-1685
    IDE_TEST( qcm::getMetaTable( QCM_LIBRARIES,
                                 &gQcmLibraries,
                                 aSmiStmt ) != IDE_SUCCESS );
    /* PROJ-1812 ROLE */
    IDE_TEST( qcm::getMetaTable( QCM_USER_ROLES,
                                 & gQcmUserRoles,
                                 aSmiStmt ) != IDE_SUCCESS );

    /* PROJ-2422 srid */
    IDE_TEST( qcm::getMetaTable( QCM_GEOMETRIES,
                                 &gQcmGeometries,
                                 aSmiStmt ) != IDE_SUCCESS );

    //-------------------------------------------
    // get index handle
    //-------------------------------------------
    IDE_TEST( getMetaIndex( gQcmColumnsIndex,
                            gQcmColumns ) != IDE_SUCCESS);

    IDE_TEST( getMetaIndex( gQcmConstraintsIndex,
                            gQcmConstraints ) != IDE_SUCCESS);

    IDE_TEST( getMetaIndex( gQcmConstraintColumnsIndex,
                            gQcmConstraintColumns ) != IDE_SUCCESS);

    IDE_TEST( getMetaIndex( gQcmIndexColumnsIndex,
                            gQcmIndexColumns ) != IDE_SUCCESS);

    IDE_TEST( getMetaIndex( gQcmUsersIndex,
                            gQcmUsers ) != IDE_SUCCESS);

    IDE_TEST( getMetaIndex( gQcmDNUsersIndex,
                            gQcmDNUsers ) != IDE_SUCCESS);

    IDE_TEST( getMetaIndex( gQcmTBSUsersIndex,
                            gQcmTBSUsers ) != IDE_SUCCESS);

    IDE_TEST( getMetaIndex( gQcmIndicesIndex,
                            gQcmIndices ) != IDE_SUCCESS);

#if !defined(SMALL_FOOTPRINT)
    IDE_TEST( getMetaIndex( gQcmReplicationsIndex,
                            gQcmReplications ) != IDE_SUCCESS);

    IDE_TEST( getMetaIndex( gQcmReplReceiverIndex,
                            gQcmReplReceiver) != IDE_SUCCESS);

    IDE_TEST( getMetaIndex( gQcmReplHostsIndex,
                            gQcmReplHosts ) != IDE_SUCCESS);

    IDE_TEST( getMetaIndex( gQcmReplItemsIndex,
                            gQcmReplItems ) != IDE_SUCCESS);

    // PROJ-1442 Replication Online �� DDL ���
    IDE_TEST( getMetaIndex( gQcmReplOldItemsIndex,
                            gQcmReplOldItems ) != IDE_SUCCESS);

    IDE_TEST( getMetaIndex( gQcmReplOldColsIndex,
                            gQcmReplOldCols ) != IDE_SUCCESS);

    IDE_TEST( getMetaIndex( gQcmReplOldIdxsIndex,
                            gQcmReplOldIdxs ) != IDE_SUCCESS);

    IDE_TEST( getMetaIndex( gQcmReplOldIdxColsIndex,
                            gQcmReplOldIdxCols ) != IDE_SUCCESS);

    /* PROJ-1915 */
    IDE_TEST( getMetaIndex( gQcmReplOfflineDirsIndex,
                            gQcmReplOfflineDirs ) != IDE_SUCCESS);

    IDE_TEST( getMetaIndex( gQcmReplItemReplaceHistoryIndex,
                            gQcmReplItemReplaceHistory ) != IDE_SUCCESS);

#endif

    IDE_TEST( getMetaIndex( gQcmProceduresIndex,
                            gQcmProcedures ) != IDE_SUCCESS);

    IDE_TEST( getMetaIndex( gQcmProcParasIndex,
                            gQcmProcParas ) != IDE_SUCCESS);

    IDE_TEST( getMetaIndex( gQcmProcParseIndex,
                            gQcmProcParse ) != IDE_SUCCESS);

    IDE_TEST( getMetaIndex( gQcmProcRelatedIndex,
                            gQcmProcRelated ) != IDE_SUCCESS);

    //PROJ-1073 Package
    IDE_TEST( getMetaIndex( gQcmPkgsIndex,
                            gQcmPkgs ) != IDE_SUCCESS);

    IDE_TEST( getMetaIndex( gQcmPkgParasIndex,
                            gQcmPkgParas ) != IDE_SUCCESS);

    IDE_TEST( getMetaIndex( gQcmPkgParseIndex,
                            gQcmPkgParse ) != IDE_SUCCESS);

    IDE_TEST( getMetaIndex( gQcmPkgRelatedIndex,
                            gQcmPkgRelated ) != IDE_SUCCESS);

    // PROJ-1359 Trigger
    IDE_TEST( getMetaIndex( gQcmTriggersIndex,
                            gQcmTriggers ) != IDE_SUCCESS);

    IDE_TEST( getMetaIndex( gQcmTriggerStringsIndex,
                            gQcmTriggerStrings ) != IDE_SUCCESS);

    IDE_TEST( getMetaIndex( gQcmTriggerUpdateColumnsIndex,
                            gQcmTriggerUpdateColumns ) != IDE_SUCCESS);

    IDE_TEST( getMetaIndex( gQcmTriggerDmlTablesIndex,
                            gQcmTriggerDmlTables ) != IDE_SUCCESS);

    // PROJ-1362 LOB
    IDE_TEST( getMetaIndex( gQcmLobsIndex,
                            gQcmLobs ) != IDE_SUCCESS);

    // PROJ-2002 Column Security
    IDE_TEST( getMetaIndex( gQcmEncryptedColumnsIndex,
                            gQcmEncryptedColumns ) != IDE_SUCCESS);

    IDE_TEST( getMetaIndex( gQcmPrivilegesIndex,
                            gQcmPrivileges ) != IDE_SUCCESS);

    IDE_TEST( getMetaIndex( gQcmGrantSystemIndex,
                            gQcmGrantSystem ) != IDE_SUCCESS);

    IDE_TEST( getMetaIndex( gQcmGrantObjectIndex,
                            gQcmGrantObject ) != IDE_SUCCESS);

    IDE_TEST( getMetaIndex( gQcmXaHeuristicTransIndex,
                            gQcmXaHeuristicTrans ) != IDE_SUCCESS);

    IDE_TEST( getMetaIndex( gQcmViewsIndex,
                            gQcmViews ) != IDE_SUCCESS);

    IDE_TEST( getMetaIndex( gQcmViewParseIndex,
                            gQcmViewParse ) != IDE_SUCCESS);

    IDE_TEST( getMetaIndex( gQcmViewRelatedIndex,
                            gQcmViewRelated ) != IDE_SUCCESS);

    // PROJ-1502 PARTITIONED DISK TABLE - BEGIN -
    IDE_TEST( getMetaIndex( gQcmPartTablesIndex,
                            gQcmPartTables ) != IDE_SUCCESS);
    IDE_TEST( getMetaIndex( gQcmPartIndicesIndex,
                            gQcmPartIndices ) != IDE_SUCCESS);
    IDE_TEST( getMetaIndex( gQcmTablePartitionsIndex,
                            gQcmTablePartitions ) != IDE_SUCCESS);
    IDE_TEST( getMetaIndex( gQcmIndexPartitionsIndex,
                            gQcmIndexPartitions ) != IDE_SUCCESS);
    IDE_TEST( getMetaIndex( gQcmPartKeyColumnsIndex,
                            gQcmPartKeyColumns ) != IDE_SUCCESS);
    IDE_TEST( getMetaIndex( gQcmPartLobsIndex,
                            gQcmPartLobs ) != IDE_SUCCESS);
    // PROJ-1502 PARTITIONED DISK TABLE - END -

    // BUG-21387 COMMENT
    IDE_TEST( getMetaIndex( gQcmCommentsIndex,
                            gQcmComments ) != IDE_SUCCESS);

    // PROJ-2207 Password policy support
    IDE_TEST( getMetaIndex( gQcmPasswordHistoryIndex,
                            gQcmPasswordHistory ) != IDE_SUCCESS);

    /* PROJ-2211 Materialized View */
    IDE_TEST( getMetaIndex( gQcmMaterializedViewsIndex,
                            gQcmMaterializedViews ) != IDE_SUCCESS );

    // PROJ-2223 audit
    IDE_TEST( getMetaIndex( gQcmAuditAllOptsIndex,
                            gQcmAuditAllOpts ) != IDE_SUCCESS );

    /* BUG-35445 Check Constraint, Function-Based Index���� ��� ���� Function�� ����/���� ���� */
    IDE_TEST( getMetaIndex( gQcmConstraintRelatedIndex,
                            gQcmConstraintRelated ) != IDE_SUCCESS);

    IDE_TEST( getMetaIndex( gQcmIndexRelatedIndex,
                            gQcmIndexRelated ) != IDE_SUCCESS);

    /* PROJ-1438 Jobs Scheduler */
    IDE_TEST( getMetaIndex( gQcmJobsIndex,
                            gQcmJobs ) != IDE_SUCCESS);

    /* PROJ-1832 New database link */
    IDE_TEST( qcmDatabaseLinksInitializeMetaHandle( aSmiStmt )
              != IDE_SUCCESS );
    /* PROJ-2264 */
    IDE_TEST( getMetaIndex( gQcmCompressionTablesIndex,
                            gQcmCompressionTables ) != IDE_SUCCESS);

    // Proj-1076 Synonym
    IDE_TEST( getMetaIndex( gQcmSynonymsIndex,
                            gQcmSynonyms ) != IDE_SUCCESS);

    // PROJ-1371 Directories
    IDE_TEST( getMetaIndex( gQcmDirectoriesIndex,
                            gQcmDirectories ) != IDE_SUCCESS);

    // PROJ-1685
    IDE_TEST( getMetaIndex( gQcmLibrariesIndex,
                            gQcmLibraries ) != IDE_SUCCESS );
    /* PROJ-1812 ROLE */
    IDE_TEST( getMetaIndex( gQcmUserRolesIndex,
                            gQcmUserRoles ) != IDE_SUCCESS );

    /* PROJ-2422 srid */
    IDE_TEST( getMetaIndex( gQcmGeometriesIndex,
                            gQcmGeometries ) != IDE_SUCCESS );
    
    // PROJ-1488 Altibase Spatio-Temporal DBMS
    for ( i = 0; i < gExternModule.mCnt; i++ )
    {
        IDE_TEST( gExternModule.mExternModule[i]->mInitMetaHandle( aSmiStmt )
                  != IDE_SUCCESS );
    }

    if (aCurrMinorVersion == NULL) // during startup service
    {
        IDE_TEST( qcm::getMetaTable( QCM_REPL_OLD_CHECKS,
                                     & gQcmReplOldChecks,
                                     aSmiStmt ) != IDE_SUCCESS );

        IDE_TEST( qcm::getMetaTable( QCM_REPL_OLD_CHECK_COLUMNS,
                                     & gQcmReplOldCheckColumns,
                                     aSmiStmt ) != IDE_SUCCESS );

        IDE_TEST( getMetaIndex( gQcmReplOldChecksIndex,
                                gQcmReplOldChecks ) != IDE_SUCCESS );

        IDE_TEST( getMetaIndex( gQcmReplOldCheckColumnsIndex,
                                gQcmReplOldCheckColumns ) != IDE_SUCCESS );

    }
    else // during upgrademeta
    {
        switch ( *aCurrMinorVersion )
        {
            case 2:
                IDE_TEST( qcm::getMetaTable( QCM_REPL_OLD_CHECKS,
                                             & gQcmReplOldChecks,
                                             aSmiStmt ) != IDE_SUCCESS );

                IDE_TEST( qcm::getMetaTable( QCM_REPL_OLD_CHECK_COLUMNS,
                                             & gQcmReplOldCheckColumns,
                                             aSmiStmt ) != IDE_SUCCESS );

                IDE_TEST( getMetaIndex( gQcmReplOldChecksIndex,
                                        gQcmReplOldChecks ) != IDE_SUCCESS );

                IDE_TEST( getMetaIndex( gQcmReplOldCheckColumnsIndex,
                                        gQcmReplOldCheckColumns ) != IDE_SUCCESS );

                // fall through
            case 1:
                // fall through
            default:
                break;
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qcm::initMetaCaches(smiStatement * aSmiStmt)
{
/***********************************************************************
 *
 * Description :
 *    ���̺���� ��Ÿ ĳ�� qcmTableInfo �� �����
 *
 * Implementation :
 *    1. read all tuple in QCM_TABLES_
 *    2. table OID, table id ���ϱ�
 *    3. makeAndSetQcmTableInfo �� ���ؼ� ��Ÿ ĳ���� �����
 *    4. gQcmUsersTempInfo, gQcmIndicesTableInfo �� ���� �д�
 *
 ***********************************************************************/

    UInt              sStage = 0;
    smiTableCursor    sCursor;
    const void      * sQcmTablesRow;
    UInt              sTableID;
    smOID             sTableOID;
    ULong             sTableOIDULong;
    mtcColumn       * sQcmTablesColumnInfo;
    mtcColumn       * sQcmTableIDColumn;
    mtcColumn       * sQcmTablesTableTypeColumn;

    scGRID            sRid; // Disk Table�� ���� Record IDentifier

    UChar           * sTableType;
    mtdCharType     * sTableTypeStr;

    sCursor.initialize();

    // read all tuple in QCM_TABLES_
    IDE_TEST(sCursor.open(aSmiStmt,
                          gQcmTables,
                          NULL,
                          smiGetRowSCN(gQcmTables),
                          NULL,
                          smiGetDefaultKeyRange(),
                          smiGetDefaultKeyRange(),
                          smiGetDefaultFilter(),
                          QCM_META_CURSOR_FLAG,
                          SMI_SELECT_CURSOR,
                          &gMetaDefaultCursorProperty)
             != IDE_SUCCESS);

    sStage = 1;

    IDE_TEST(sCursor.beforeFirst() != IDE_SUCCESS);

    IDE_TEST(sCursor.readRow(&sQcmTablesRow, &sRid, SMI_FIND_NEXT)
             != IDE_SUCCESS);

    // table OID
    IDE_TEST( smiGetTableColumns( gQcmTables,
                                  QCM_TABLES_TABLE_OID_COL_ORDER,
                                  (const smiColumn**)&sQcmTablesColumnInfo )
              != IDE_SUCCESS );

    // table id
    IDE_TEST( smiGetTableColumns( gQcmTables,
                                  QCM_TABLES_TABLE_ID_COL_ORDER,
                                  (const smiColumn**)&sQcmTableIDColumn)
              != IDE_SUCCESS );
    // table type
    IDE_TEST( smiGetTableColumns( gQcmTables,
                                  QCM_TABLES_TABLE_TYPE_COL_ORDER,
                                  (const smiColumn**)&sQcmTablesTableTypeColumn)
              != IDE_SUCCESS );

    while (sQcmTablesRow != NULL)
    {
        sTableOIDULong = *(ULong*) ((UChar*)sQcmTablesRow +
                                    sQcmTablesColumnInfo->column.offset);
        sTableOID = (smOID) sTableOIDULong;
        sTableID = *(UInt *) ((UChar*)sQcmTablesRow +
                              sQcmTableIDColumn->column.offset);

        sTableTypeStr = (mtdCharType*) ((UChar*)sQcmTablesRow +
                                        sQcmTablesTableTypeColumn->column.offset);
        sTableType = sTableTypeStr->value;

        // if we want to get the table is sequence or not
        /* BUG-35460 Add TABLE_TYPE G in SYS_TABLES_ */
        if ( ( idlOS::strMatch( (SChar*)sTableType, sTableTypeStr->length, "T", 1 ) == 0 ) ||
             ( idlOS::strMatch( (SChar*)sTableType, sTableTypeStr->length, "V", 1 ) == 0 ) ||
             ( idlOS::strMatch( (SChar*)sTableType, sTableTypeStr->length, "M", 1 ) == 0 ) ||
             ( idlOS::strMatch( (SChar*)sTableType, sTableTypeStr->length, "A", 1 ) == 0 ) ||
             ( idlOS::strMatch( (SChar*)sTableType, sTableTypeStr->length, "Q", 1 ) == 0 ) ||
             ( idlOS::strMatch( (SChar*)sTableType, sTableTypeStr->length, "G", 1 ) == 0 ) ||
             ( idlOS::strMatch( (SChar*)sTableType, sTableTypeStr->length, "D", 1 ) == 0 ) ||
             ( idlOS::strMatch( (SChar*)sTableType, sTableTypeStr->length, "E", 1 ) == 0 ) ||
             ( idlOS::strMatch( (SChar*)sTableType, sTableTypeStr->length, "R", 1 ) == 0 ) ) /* PROJ-2441 flashback */
        {
            IDE_TEST( makeAndSetQcmTableInfo( aSmiStmt,
                                              sTableID,
                                              sTableOID,
                                              sQcmTablesRow ) != IDE_SUCCESS);
        
            // PROJ-1502 PARTITIONED DISK TABLE
            IDE_TEST( qcmPartition::makeQcmPartitionInfoByTableID(
                          aSmiStmt,
                          sTableID )
                      != IDE_SUCCESS );
        }
        else
        {
            // Nothing to do.
        }

        // read Next Tuple
        IDE_TEST(sCursor.readRow(&sQcmTablesRow, &sRid, SMI_FIND_NEXT)
                 != IDE_SUCCESS);
    }

    IDE_TEST( smiGetTableTempInfo( gQcmUsers,
                                   (void**)&gQcmUsersTempInfo )
              != IDE_SUCCESS );
    IDE_TEST( smiGetTableTempInfo( gQcmDNUsers,
                                   (void**)&gQcmDNUsersTempInfo )
              != IDE_SUCCESS );
    IDE_TEST( smiGetTableTempInfo( gQcmIndices,
                                   (void**)&gQcmIndicesTableInfo )
              != IDE_SUCCESS );

    sStage = 0;
    IDE_TEST(sCursor.close() != IDE_SUCCESS);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if ( sStage == 1 )
    {
        (void)sCursor.close();
    }

    return IDE_FAILURE;
}

IDE_RC qcm::getMetaIndex( const void ** v_indexHandle,
                          const void  * v_tableHandle)
{
    UInt         i;
    UInt         v_idxCount;

    v_idxCount = smiGetTableIndexCount(v_tableHandle);
    IDE_TEST(v_idxCount == 0);


    // BUGBUG : re-consider this function!!
    for (i = 0; i < v_idxCount; i++)
    {
        // BUG-28454 smiGetTableIndexes �Լ��� ����� �̰������� ���
        // �� �� ��� ��쿡�� smiGetTableIndexById�� ����ؾ� ��
        v_indexHandle[i] = smiGetTableIndexes( v_tableHandle, i );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/* Meta Table�� Tablespace ID�ʵ�κ��� �����͸� �о�´� */
scSpaceID qcm::getSpaceID(const void * aTableRow,
                          UInt         aFieldOffset )
{
    return (scSpaceID) *(mtdIntegerType*) ((UChar *)aTableRow + aFieldOffset);
}


IDE_RC qcm::makeAndSetQcmTableInfo( smiStatement * aSmiStmt,
                                    UInt           aTableID,
                                    smOID          aTableOID,
                                    const void *   aTableRow /* = NULL */)
{
    void            * sTableHandle;
    qcmTableInfo    * sTableInfo = NULL;
    smSCN             sSCN;
    mtcColumn       * sTableTypeCol;
    mtcColumn       * sReplicationCountCol;
    mtcColumn       * sReplicationRecoveryCountCol;
    mtcColumn       * sTableNameCol;
    mtdCharType     * sTableTypeStr;
    mtdCharType     * sTableNameStr;
    mtdCharType     * sTBSNameStr;
    mtcColumn       * sTableOwnerIDCol;
    mtcColumn       * sMaxRowsCol;
    mtcColumn       * sTBSNameCol;
    mtcColumn       * sTBSIDCol;
    mtcColumn       * sPctFreeCol;
    mtcColumn       * sPctUsedCol;
    mtcColumn       * sInitTransCol;
    mtcColumn       * sMaxTransCol;
    mtcColumn       * sInitExtentsCol;
    mtcColumn       * sNextExtentsCol;
    mtcColumn       * sMinExtentsCol;
    mtcColumn       * sMaxExtentsCol;
    const void      * sTableRow;

    // PROJ-1502 PARTITIONED DISK TABLE
    mtcColumn       * sIsPartitionedCol;
    mtdCharType     * sIsPartitionedStr;
    idBool            sIsPartitioned = ID_FALSE;
    UInt              i;

    // PROJ-1407 Temporary Table
    mtcColumn       * sTemporaryCol;
    mtdCharType     * sTemporaryStr;

    // PROJ-1624 Global Non-partitioned Index
    mtcColumn       * sHiddenCol;
    mtdCharType     * sHiddenStr;

    // PROJ-1071 Parallel query
    mtcColumn       * sParallelDegreeCol;

    /* PROJ-2359 Table/Partition Access Option */
    mtcColumn       * sAccessOptionCol;
    mtdCharType     * sAccessOptionStr;

    /* TASK-7307 DML Data Consistency in Shard */
    mtcColumn       * sUsableCol;
    mtdCharType     * sUsableStr;
    mtcColumn       * sShardFlagCol;

    smiTableSpaceAttr  sTBSAttr;

    qciArgCreateQueue sArgCreateQueue;

    IDU_FIT_POINT( "qcm::makeAndSetQcmTableInfo::malloc::sTableInfo", 
                    idERR_ABORT_InsufficientMemory );

    IDE_TEST(iduMemMgr::malloc(IDU_MEM_QCM,
                               ID_SIZEOF(qcmTableInfo),
                               (void**)&sTableInfo)
             != IDE_SUCCESS);

    idlOS::memset( sTableInfo, 0x00, ID_SIZEOF(qcmTableInfo) );

    // PROJ-1502 PARTITIONED DISK TABLE
    sTableInfo->tablePartitionType = QCM_NONE_PARTITIONED_TABLE;
    sTableInfo->partitionMethod  = QCM_PARTITION_METHOD_NONE;
    sTableInfo->partKeyColumns = NULL;
    sTableInfo->partKeyColBasicInfo = NULL;
    sTableInfo->partKeyColsFlag = NULL;
    sTableInfo->partKeyColCount  = 0;
    sTableInfo->rowMovement      = ID_FALSE;

    // PROJ-1407 Temporary Type
    sTableInfo->temporaryInfo.type = QCM_TEMPORARY_ON_COMMIT_NONE;

    // PROJ-1624 Global Non-partitioned Index
    sTableInfo->hiddenType = QCM_HIDDEN_NONE;

    /* PROJ-2359 Table/Partition Access Option */
    sTableInfo->accessOption = QCM_ACCESS_OPTION_READ_WRITE;

    // TABLE_ID
    sTableInfo->tableID = aTableID;

    /* BUG-45646 */
    sTableInfo->mPVType = QCM_PV_TYPE_NONE;

    /* TASK-7307 DML Data Consistency in Shard */
    sTableInfo->mIsUsable  = ID_TRUE;
    sTableInfo->mShardFlag = QCM_SHARD_FLAG_TABLE_NONE;

    if (gQcmTablesIndex[QCM_TABLES_TABLEID_IDX_ORDER] == NULL)
    {
        // on create db.
        sTableInfo->tableOwnerID     = QC_SYSTEM_USER_ID;
        idlOS::snprintf( sTableInfo->tableOwnerName,
                         QC_MAX_OBJECT_NAME_LEN + 1,
                         "%s",
                         QC_SYSTEM_USER_NAME );
        sTableInfo->tableType        = QCM_META_TABLE;
        sTableInfo->replicationCount = 0;
        sTableInfo->maxrows          = 0;
        sTableInfo->status           = QCM_VIEW_VALID;
        sTableInfo->TBSID            = SMI_ID_TABLESPACE_SYSTEM_MEMORY_DIC;
        sTableInfo->TBSType          = SMI_MEMORY_SYSTEM_DICTIONARY;
        sTableInfo->replicationRecoveryCount = 0; //proj-1608

        /* PROJ-1071 Parallel Query */
        sTableInfo->parallelDegree = 1;
    }
    else
    {
        IDE_TEST( smiGetTableColumns( gQcmTables,
                                      QCM_TABLES_USER_ID_COL_ORDER,
                                      (const smiColumn**)&sTableOwnerIDCol )
                  != IDE_SUCCESS );
        IDE_TEST( smiGetTableColumns( gQcmTables,
                                      QCM_TABLES_TABLE_TYPE_COL_ORDER,
                                      (const smiColumn**)& sTableTypeCol )
                  != IDE_SUCCESS );
        IDE_TEST( smiGetTableColumns( gQcmTables,
                                      QCM_TABLES_REPLICATION_COUNT_COL_ORDER,
                                      (const smiColumn**)& sReplicationCountCol )
                  != IDE_SUCCESS );
        //proj-1608
        IDE_TEST( smiGetTableColumns( gQcmTables,
                                      QCM_TABLES_REPL_RECOERY_COUNT_COL_ORDER,
                                      (const smiColumn**)&sReplicationRecoveryCountCol )
                  != IDE_SUCCESS );

        IDE_TEST( smiGetTableColumns( gQcmTables,
                                      QCM_TABLES_TABLE_NAME_COL_ORDER,
                                      (const smiColumn**)&sTableNameCol )
                  != IDE_SUCCESS );
        IDE_TEST( smiGetTableColumns( gQcmTables,
                                      QCM_TABLES_MAXROW_COL_ORDER,
                                      (const smiColumn**)&sMaxRowsCol )
                  != IDE_SUCCESS );
        IDE_TEST( smiGetTableColumns( gQcmTables,
                                      QCM_TABLES_TBS_NAME_COL_ORDER,
                                      (const smiColumn**)&sTBSNameCol )
                  != IDE_SUCCESS );
        IDE_TEST( smiGetTableColumns( gQcmTables,
                                      QCM_TABLES_TBS_ID_COL_ORDER,
                                      (const smiColumn**)&sTBSIDCol )
                  != IDE_SUCCESS );
        IDE_TEST( smiGetTableColumns( gQcmTables,
                                      QCM_TABLES_PCTFREE_COL_ORDER,
                                      (const smiColumn**)&sPctFreeCol )
                  != IDE_SUCCESS );
        IDE_TEST( smiGetTableColumns( gQcmTables,
                                      QCM_TABLES_PCTUSED_COL_ORDER,
                                      (const smiColumn**)&sPctUsedCol )
                  != IDE_SUCCESS );
        IDE_TEST( smiGetTableColumns( gQcmTables,
                                      QCM_TABLES_INIT_TRANS_COL_ORDER,
                                      (const smiColumn**)&sInitTransCol )
                  != IDE_SUCCESS );
        IDE_TEST( smiGetTableColumns( gQcmTables,
                                      QCM_TABLES_MAX_TRANS_COL_ORDER,
                                      (const smiColumn**)&sMaxTransCol )
                  != IDE_SUCCESS );

        // PROJ-1671 BITMAP TABLESPACE AND SEGMENT MANAGMENT
        IDE_TEST( smiGetTableColumns( gQcmTables,
                                      QCM_TABLES_INITEXTENTS_COL_ORDER,
                                      (const smiColumn**)&sInitExtentsCol )
                  != IDE_SUCCESS );
        IDE_TEST( smiGetTableColumns( gQcmTables,
                                      QCM_TABLES_NEXTEXTENTS_COL_ORDER,
                                      (const smiColumn**)&sNextExtentsCol )
                  != IDE_SUCCESS );
        IDE_TEST( smiGetTableColumns( gQcmTables,
                                      QCM_TABLES_MINEXTENTS_COL_ORDER,
                                      (const smiColumn**)&sMinExtentsCol )
                  != IDE_SUCCESS );
        IDE_TEST( smiGetTableColumns( gQcmTables,
                                      QCM_TABLES_MAXEXTENTS_COL_ORDER,
                                      (const smiColumn**)&sMaxExtentsCol )
                  != IDE_SUCCESS );
        // PROJ-1502 PARTITIONED DISK TABLE
        IDE_TEST( smiGetTableColumns( gQcmTables,
                                      QCM_TABLES_IS_PARTITIONED_COL_ORDER,
                                      (const smiColumn**)&sIsPartitionedCol )
                  != IDE_SUCCESS );

        // PROJ-1407 Temporary Table
        IDE_TEST( smiGetTableColumns( gQcmTables,
                                      QCM_TABLES_TEMPORARY_COL_ORDER,
                                      (const smiColumn**)&sTemporaryCol )
                  != IDE_SUCCESS );

        // PROJ-1624 Global Non-partitioned Index
        IDE_TEST( smiGetTableColumns( gQcmTables,
                                      QCM_TABLES_HIDDEN_COL_ORDER,
                                      (const smiColumn**)&sHiddenCol )
                  != IDE_SUCCESS );
        /* PROJ-2359 Table/Partition Access Option */
        IDE_TEST( smiGetTableColumns( gQcmTables,
                                      QCM_TABLES_ACCESS_COL_ORDER,
                                      (const smiColumn**)&sAccessOptionCol )
                  != IDE_SUCCESS );

        /* PROJ-1071 Parallel Query */
        IDE_TEST( smiGetTableColumns( gQcmTables,
                                      QCM_TABLES_PARALLEL_DEGREE_COL_ORDER,
                                      (const smiColumn**)&sParallelDegreeCol )
                  != IDE_SUCCESS );

        /* TASK-7307 DML Data Consistency in Shard */
        IDE_TEST( smiGetTableColumns( gQcmTables,
                                      QCM_TABLES_USABLE_COL_ORDER,
                                      (const smiColumn**)&sUsableCol )
                  != IDE_SUCCESS );
        IDE_TEST( smiGetTableColumns( gQcmTables,
                                      QCM_TABLES_SHARD_FLAG_COL_ORDER,
                                      (const smiColumn**)&sShardFlagCol )
                  != IDE_SUCCESS );

        if (aTableRow == NULL)
        {
            IDE_TEST(getTableHandleByID(aSmiStmt,
                                        aTableID,
                                        &sTableHandle,
                                        &sSCN,
                                        &sTableRow) != IDE_SUCCESS);

            aTableRow = sTableRow;
        }

        // table owner ID
        sTableInfo->tableOwnerID = *(UInt *)
            ((UChar *)aTableRow + sTableOwnerIDCol->column.offset);

        // table owner name
        IDE_TEST( qcmUser::getMetaUserName( aSmiStmt,
                                            sTableInfo->tableOwnerID,
                                            sTableInfo->tableOwnerName )
                  != IDE_SUCCESS );

        // table replication count
        sTableInfo->replicationCount = *(UInt *)
            ((UChar *)aTableRow +
             sReplicationCountCol->column.offset);

        //proj-1608: table replication recovery count
        sTableInfo->replicationRecoveryCount = *(UInt *)
            ((UChar *)aTableRow +
             sReplicationRecoveryCountCol->column.offset);

        // table maxrows
        sTableInfo->maxrows = *(ULong *)
            ((UChar *)aTableRow + sMaxRowsCol->column.offset);

        // table tablespace name
        sTBSNameStr = (mtdCharType*)
            ((UChar*)aTableRow + sTBSNameCol->column.offset);

        idlOS::memcpy(sTableInfo->TBSName,
                      sTBSNameStr->value,
                      sTBSNameStr->length);
        if (sTBSNameStr->length > 0)
        {
            sTableInfo->TBSName[sTBSNameStr->length] = '\0';
        }
        else
        {
            /* Nothing to do */
        }

        // table tablespace_id
        sTableInfo->TBSID = qcm::getSpaceID(aTableRow,
                                            sTBSIDCol->column.offset);

        // set TablespaceType
        IDE_TEST( qcmTablespace::getTBSAttrByID( sTableInfo->TBSID,
                                                 & sTBSAttr )
                  != IDE_SUCCESS);
        sTableInfo->TBSType = sTBSAttr.mType;

        // table PCTFREE
        sTableInfo->segAttr.mPctFree = *(UInt *)
            ((UChar *)aTableRow + sPctFreeCol->column.offset);

        // table PCTUSED
        sTableInfo->segAttr.mPctUsed = *(UInt *)
            ((UChar *)aTableRow + sPctUsedCol->column.offset);

        // table initTrans
        sTableInfo->segAttr.mInitTrans = *(UInt *)
            ((UChar *)aTableRow + sInitTransCol->column.offset);

        // table MAXTRANS
        sTableInfo->segAttr.mMaxTrans = *(UInt *)
            ((UChar *)aTableRow + sMaxTransCol->column.offset);

        // table initextents
        sTableInfo->segStoAttr.mInitExtCnt = *(ULong *)
            ((UChar *)aTableRow + sInitExtentsCol->column.offset);

        // table nextextents
        sTableInfo->segStoAttr.mNextExtCnt = *(ULong *)
            ((UChar *)aTableRow + sNextExtentsCol->column.offset);

        // table minextents
        sTableInfo->segStoAttr.mMinExtCnt = *(ULong *)
            ((UChar *)aTableRow + sMinExtentsCol->column.offset);

        // table maxextents
        sTableInfo->segStoAttr.mMaxExtCnt = *(ULong *)
            ((UChar *)aTableRow + sMaxExtentsCol->column.offset);

        // table type
        sTableTypeStr = (mtdCharType *)
            ((UChar *)aTableRow + sTableTypeCol->column.offset);
        if ( idlOS::strMatch( (SChar *)&(sTableTypeStr->value),
                                sTableTypeStr->length,
                                "T",
                                1 ) == 0 )
        {
            sTableInfo->tableType = QCM_USER_TABLE;
            sTableInfo->status    = QCM_VIEW_VALID;

            // PROJ-1502 PARTITIONED DISK TABLE
            sIsPartitionedStr = (mtdCharType *)
                ((UChar *)aTableRow + sIsPartitionedCol->column.offset);

            if ( idlOS::strMatch( (SChar *)&(sIsPartitionedStr->value),
                                  sIsPartitionedStr->length,
                                  "T",
                                  1 ) == 0 )
            {
                sIsPartitioned = ID_TRUE;
            }
            else
            {
                // Nothing to do.
            }
        }
        else if ( idlOS::strMatch( (SChar *)&(sTableTypeStr->value),
                                   sTableTypeStr->length,
                                   "D",
                                   1 ) == 0 )
        {
            sTableInfo->tableType = QCM_DICTIONARY_TABLE;
            sTableInfo->status    = QCM_VIEW_VALID;

            // PROJ-1502 PARTITIONED DISK TABLE
            sIsPartitionedStr = (mtdCharType *)
                ((UChar *)aTableRow + sIsPartitionedCol->column.offset);

            if ( idlOS::strMatch( (SChar *)&(sIsPartitionedStr->value),
                                  sIsPartitionedStr->length,
                                  "T",
                                  1 ) == 0 )
            {
                sIsPartitioned = ID_TRUE;
            }
            else
            {
                // Nothing to do.
            }
        }
        // Proj-1360 Queue
        else if ( idlOS::strMatch( (SChar *)&(sTableTypeStr->value),
                                   sTableTypeStr->length,
                                   "Q",
                                   1 ) == 0 )
        {
            sTableInfo->tableType = QCM_QUEUE_TABLE;
            sTableInfo->status    = QCM_VIEW_VALID;

            sArgCreateQueue.mTableID = aTableID;

            IDE_TEST(qcg::mCreateQueueFuncPtr( (void *)&sArgCreateQueue )
                     != IDE_SUCCESS);
        }
        else if ( idlOS::strMatch( (SChar *)&(sTableTypeStr->value),
                                   sTableTypeStr->length,
                                   "V",
                                   1 ) == 0 )
        {
            sTableInfo->tableType = QCM_VIEW;

            // view status
            IDE_TEST(qcmView::getStatusOfViews( aSmiStmt,
                                                sTableInfo->tableID,
                                                &(sTableInfo->status) )
                     != IDE_SUCCESS);
        }
        else if ( idlOS::strMatch( (SChar *)&(sTableTypeStr->value),
                                   sTableTypeStr->length,
                                   "M",
                                   1 ) == 0 )
        {
            sTableInfo->tableType = QCM_MVIEW_TABLE;
            sTableInfo->status    = QCM_VIEW_VALID;

            /* PROJ-1502 PARTITIONED DISK TABLE */
            sIsPartitionedStr = (mtdCharType *)
                ((UChar *)aTableRow + sIsPartitionedCol->column.offset);

            if ( idlOS::strMatch( (SChar *)&(sIsPartitionedStr->value),
                                  sIsPartitionedStr->length,
                                  "T",
                                  1 ) == 0 )
            {
                sIsPartitioned = ID_TRUE;
            }
            else
            {
                /* Nothing to do */
            }
        }
        else if ( idlOS::strMatch( (SChar *)&(sTableTypeStr->value),
                                   sTableTypeStr->length,
                                   "A",
                                   1 ) == 0 )
        {
            sTableInfo->tableType = QCM_MVIEW_VIEW;

            /* view status */
            IDE_TEST( qcmView::getStatusOfViews( aSmiStmt,
                                                 sTableInfo->tableID,
                                                 &(sTableInfo->status) )
                      != IDE_SUCCESS );
        }
        /* BUG-35460 Add TABLE_TYPE G in SYS_TABLES_ */
        else if ( idlOS::strMatch( (SChar *)&(sTableTypeStr->value),
                                   sTableTypeStr->length,
                                   "G",
                                   1 ) == 0 )
        {
            sTableInfo->tableType = QCM_INDEX_TABLE;
            sTableInfo->status    = QCM_VIEW_VALID;

            // PROJ-1502 PARTITIONED DISK TABLE
            sIsPartitionedStr = (mtdCharType *)
                ((UChar *)aTableRow + sIsPartitionedCol->column.offset);

            if ( idlOS::strMatch( (SChar *)&(sIsPartitionedStr->value),
                                  sIsPartitionedStr->length,
                                  "T",
                                  1 ) == 0 )
            {
                IDE_RAISE( ERR_NOT_SUPPORTED_PARTITION );
            }
            else
            {
                // Nothing to do.
            }
        }
        /* PROJ-2365 sequence table */
        else if ( idlOS::strMatch( (SChar *)&(sTableTypeStr->value),
                                   sTableTypeStr->length,
                                   "E",
                                   1 ) == 0 )
        {
            sTableInfo->tableType = QCM_SEQUENCE_TABLE;
            sTableInfo->status    = QCM_VIEW_VALID;

            // PROJ-1502 PARTITIONED DISK TABLE
            sIsPartitionedStr = (mtdCharType *)
                ((UChar *)aTableRow + sIsPartitionedCol->column.offset);

            if ( idlOS::strMatch( (SChar *)&(sIsPartitionedStr->value),
                                  sIsPartitionedStr->length,
                                  "T",
                                  1 ) == 0 )
            {
                IDE_RAISE( ERR_NOT_SUPPORTED_PARTITION );
            }
            else
            {
                // Nothing to do.
            }
        }
        /* PROJ-2441 flashback */
        else if ( idlOS::strMatch( (SChar *)&(sTableTypeStr->value),
                                   sTableTypeStr->length,
                                   "R",
                                   1 ) == 0 )
        {
            sTableInfo->tableType = QCM_RECYCLEBIN_TABLE;
            sTableInfo->status    = QCM_VIEW_VALID;
            
            // PROJ-1502 PARTITIONED DISK TABLE
            sIsPartitionedStr = (mtdCharType *)
            ((UChar *)aTableRow + sIsPartitionedCol->column.offset);
            
            if ( idlOS::strMatch( (SChar *)&(sIsPartitionedStr->value),
                                  sIsPartitionedStr->length,
                                  "T",
                                  1 ) == 0 )
            {
                sIsPartitioned = ID_TRUE;
            }
            else
            {
                // Nothing to do.
            }
        }
        else
        {
            sTableInfo->tableType = QCM_META_TABLE;
            sTableInfo->status    = QCM_VIEW_VALID;

            ideLog::log(
                IDE_QP_0,
                "[FAILURE] make qcmTableInfo : unknown TABLE_TYPE !!!\n");
            IDE_RAISE(ERR_META_CRASH);
        }

        // PROJ-1407 Temporary Table
        sTemporaryStr = (mtdCharType *)
            ((UChar *)aTableRow + sTemporaryCol->column.offset);

        if ( idlOS::strMatch( (SChar *)&(sTemporaryStr->value),
                              sTemporaryStr->length,
                              "N",
                              1 ) == 0 )
        {
            // Nothing to do.
        }
        else if ( idlOS::strMatch( (SChar *)&(sTemporaryStr->value),
                                   sTemporaryStr->length,
                                   "D",
                                   1 ) == 0 )
        {
            sTableInfo->temporaryInfo.type = QCM_TEMPORARY_ON_COMMIT_DELETE_ROWS;
        }
        else if ( idlOS::strMatch( (SChar *)&(sTemporaryStr->value),
                                   sTemporaryStr->length,
                                   "P",
                                   1 ) == 0 )
        {
            sTableInfo->temporaryInfo.type = QCM_TEMPORARY_ON_COMMIT_PRESERVE_ROWS;
        }
        else
        {
            ideLog::log(
                IDE_QP_0,
                "[FAILURE] make qcmTableInfo : unknown TEMPORARY !!!\n");
            IDE_RAISE(ERR_META_CRASH);
        }

        // PROJ-1624 global non-partitioned index
        sHiddenStr = (mtdCharType *)
            ((UChar *)aTableRow + sHiddenCol->column.offset);

        if ( idlOS::strMatch( (SChar *)&(sHiddenStr->value),
                              sHiddenStr->length,
                              "N",
                              1 ) == 0 )
        {
            // Nothing to do.
        }
        /* BUG-35460 Add TABLE_TYPE G in SYS_TABLES_ */
        else if ( idlOS::strMatch( (SChar *)&(sHiddenStr->value),
                                   sHiddenStr->length,
                                   "Y",
                                   1 ) == 0 )
        {
            sTableInfo->hiddenType = QCM_HIDDEN_INDEX_TABLE;
        }
        else
        {
            ideLog::log(
                IDE_QP_0,
                "[FAILURE] make qcmTableInfo : unknown HIDDEN !!!\n");
            IDE_RAISE(ERR_META_CRASH);
        }

        /* PROJ-2359 Table/Partition Access Option */
        sAccessOptionStr = (mtdCharType*)
            ((UChar*)aTableRow + sAccessOptionCol->column.offset);

        switch ( sAccessOptionStr->value[0] )
        {
            case 'R' :
                sTableInfo->accessOption = QCM_ACCESS_OPTION_READ_ONLY;
                break;

            case 'W' :
                sTableInfo->accessOption = QCM_ACCESS_OPTION_READ_WRITE;
                break;

            case 'A' :
                sTableInfo->accessOption = QCM_ACCESS_OPTION_READ_APPEND;
                break;

            default :
                ideLog::log( IDE_QP_0, "[FAILURE] make qcmTableInfo : unknown ACCESS !!!\n" );
                IDE_RAISE( ERR_META_CRASH );
        }

        /* PROJ-1071 Parallel Query */
        sTableInfo->parallelDegree = *(UInt *)
            ((UChar *)aTableRow + sParallelDegreeCol->column.offset);

        // TASK-7307 DML Data Consistency in Shard
        // usable
        sUsableStr = (mtdCharType *)
            ((UChar *)aTableRow + sUsableCol->column.offset);

        if ( sUsableStr->value[0] == 'Y' )
        {
            sTableInfo->mIsUsable = ID_TRUE;
        }
        else if ( sUsableStr->value[0] == 'N' )
        {
            sTableInfo->mIsUsable = ID_FALSE;
        }
        else
        {
            ideLog::log(
                IDE_QP_0,
                "[FAILURE] make qcmTableInfo : unknown USABLE !!!\n");
            IDE_RAISE(ERR_META_CRASH);
        }

        // shard_flag
        sTableInfo->mShardFlag = *(UInt *)
            ((UChar *)aTableRow + sShardFlagCol->column.offset);

        // table name
        sTableNameStr = (mtdCharType*)
            ((UChar*)aTableRow + sTableNameCol->column.offset);

        idlOS::memcpy(sTableInfo->name,
                      sTableNameStr->value,
                      sTableNameStr->length);
        if (sTableNameStr->length > 0)
        {
            sTableInfo->name[sTableNameStr->length] = '\0';
        }
    }

    // BUG-13725
    qcm::setOperatableFlag( sTableInfo->tableType,
                            &sTableInfo->operatableFlag );

    // table handle
    sTableInfo->tableHandle = (void*) smiGetTable( aTableOID );

    // ��Ÿ ���� ����ϴ� ����
    sTableInfo->tableOID     = aTableOID;
    sTableInfo->tableFlag    = smiGetTableFlag( sTableInfo->tableHandle );
    sTableInfo->isDictionary = smiIsDictionaryTable( sTableInfo->tableHandle );
    sTableInfo->viewReadOnly = QCM_VIEW_NON_READ_ONLY;

    if (sTableInfo->status == QCM_VIEW_VALID)
    {
        // in case of TABLE, SEQUENCE, valid VIEW

        // column count
        sTableInfo->columnCount =
            smiGetTableColumnCount(sTableInfo->tableHandle);

        // get column info
        IDE_TEST(getQcmColumn(aSmiStmt, sTableInfo) != IDE_SUCCESS);

        // BUG-42877
        // lob column�� ������ lob ������ sTableInfo->columns�� flag�� �߰��Ѵ�.
        if ( sTableInfo->lobColumnCount > 0 )
        {
            IDE_TEST(getQcmLobColumn(aSmiStmt, sTableInfo ) != IDE_SUCCESS);
        }

        // index count
        sTableInfo->indexCount = smiGetTableIndexCount(sTableInfo->tableHandle);

        // get index info
        IDE_TEST(getQcmIndices(aSmiStmt, sTableInfo) != IDE_SUCCESS);

        // get constraints info
        IDE_TEST(getQcmConstraints(aSmiStmt, aTableID, sTableInfo)
                 != IDE_SUCCESS);

        // get privileges info
        IDE_TEST(qcmPriv::getQcmPrivileges(aSmiStmt, aTableID, sTableInfo)
                 != IDE_SUCCESS);

        // PROJ-1359 Trigger, get trigger info
        IDE_TEST( qcmTrigger::getTriggerMetaCache( aSmiStmt,
                                                   aTableID,
                                                   sTableInfo )
                  != IDE_SUCCESS );

        // PROJ-1502 PARTITIONED DISK TABLE
        if( sIsPartitioned == ID_TRUE )
        {
            IDE_TEST( qcmPartition::getQcmPartitionedTableInfo(
                          aSmiStmt,
                          sTableInfo )
                      != IDE_SUCCESS );

            for ( i = 0; i < sTableInfo->indexCount; i++ )
            {
                if ( sTableInfo->indices[i].indexPartitionType ==
                     QCM_NONE_PARTITIONED_INDEX )
                {
                    // partitioned table�� index�� non-partitioned index���
                    // �� index�� global index�̰� indexTableID�� �ݵ�� �����ؾ��Ѵ�.
                    IDE_TEST_RAISE( sTableInfo->indices[i].indexTableID == 0,
                                    ERR_META_CRASH );
                }
                else
                {
                    // partitioned table�� index�� partitioned index���
                    // indexTableID�� ������ �� ����.
                    IDE_TEST_RAISE( sTableInfo->indices[i].indexTableID != 0,
                                    ERR_META_CRASH );
                }
            }
        }
        else
        {
            for ( i = 0; i < sTableInfo->indexCount; i++ )
            {
                // non-partitioned table�� index�� non-partitioned index�̾�� �Ѵ�.
                IDE_TEST_RAISE( sTableInfo->indices[i].indexPartitionType !=
                                QCM_NONE_PARTITIONED_INDEX,
                                ERR_META_CRASH );

                // non-partitioned table�� non-partitioned index��
                // indexTableID�� ������ �� ����.
                IDE_TEST_RAISE( sTableInfo->indices[i].indexTableID != 0,
                                ERR_META_CRASH );
            }
        }
    }
    else
    {
        // in case of invalid VIEW

        // column info
        sTableInfo->columnCount = 0;
        sTableInfo->columns     = NULL;

        // index info
        sTableInfo->indexCount     = 0;
        sTableInfo->indices        = NULL;

        // primary key
        sTableInfo->primaryKey = NULL;

        // unique key
        sTableInfo->uniqueKeyCount = 0;
        sTableInfo->uniqueKeys     = NULL;

        // foreign key
        sTableInfo->foreignKeyCount = 0;
        sTableInfo->foreignKeys     = NULL;

        // not null constraint
        sTableInfo->notNullCount = 0;
        sTableInfo->notNulls     = NULL;

        /* PROJ-1107 Check Constraint ���� */
        sTableInfo->checkCount = 0;
        sTableInfo->checks     = NULL;

        //timestamp
        sTableInfo->timestamp = NULL;

        // privilege info
        sTableInfo->privilegeCount = 0;
        sTableInfo->privilegeInfo  = NULL;

        /* PROJ-1888 INSTEAD OF TRIGGER */
        IDE_TEST( qcmTrigger::getTriggerMetaCache( aSmiStmt,
                                       aTableID,
                                       sTableInfo )
                  != IDE_SUCCESS );

        // PROJ-1362
        sTableInfo->lobColumnCount = 0;
    }

    // set table info.
    smiSetTableTempInfo( sTableInfo->tableHandle, (void*)sTableInfo );

    /* BUG-42928 No Partition Lock */
    if ( QCU_TABLE_LOCK_MODE != 0 )
    {
        smiInitTableReplacementLock( sTableInfo->tableHandle );
    }
    else
    {
        /* Nothing to do */
    }

    IDU_FIT_POINT("qcm::makeAndSetQcmTableInfo::_FT_");

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_META_CRASH);
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QCM_META_CRASH));
    }
    IDE_EXCEPTION( ERR_NOT_SUPPORTED_PARTITION )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qcm::makeAndSetQcmTableInfo",
                                  "This object type cannot be used with the partition feature" ) );
    }
    IDE_EXCEPTION_END;

    if (sTableInfo != NULL)
    {
        (void)destroyQcmTableInfo(sTableInfo);
    }

    return IDE_FAILURE;
}

void qcm::destroyQcmTableInfo(qcmTableInfo *aInfo)
{
    UInt i;

    if (aInfo != NULL)
    {
        // PROJ-1502 PARTITIONED DISK TABLE
        IDE_DASSERT( aInfo->tablePartitionType != QCM_TABLE_PARTITION );

        if (aInfo->columns != NULL)
        {
            for (i = 0; i < aInfo->columnCount; i++)
            {
                if (aInfo->columns[i].defaultValueStr != NULL)
                {
                    IDE_TEST(iduMemMgr::free(aInfo->columns[i].defaultValueStr)
                             != IDE_SUCCESS);
                    aInfo->columns[i].defaultValueStr = NULL;
                }

                if (aInfo->columns[i].basicInfo != NULL)
                {
                    IDE_TEST(iduMemMgr::free(aInfo->columns[i].basicInfo)
                             != IDE_SUCCESS);
                    aInfo->columns[i].basicInfo = NULL;
                }
            }

            IDE_TEST(iduMemMgr::free(aInfo->columns) != IDE_SUCCESS);
            aInfo->columns = NULL;
        }

        if (aInfo->indices != NULL)
        {
            for (i = 0; i< aInfo->indexCount; i++)
            {
                if ( aInfo->indices[i].keyColumns != NULL )
                {
                    IDE_TEST(iduMemMgr::free(aInfo->indices[i].keyColumns)
                             != IDE_SUCCESS);
                    aInfo->indices[i].keyColumns = NULL;
                }
            }
            IDE_TEST(iduMemMgr::free(aInfo->indices) != IDE_SUCCESS);
            aInfo->indices = NULL;
        }

        if (aInfo->uniqueKeys != NULL)
        {
            IDE_TEST(iduMemMgr::free(aInfo->uniqueKeys) != IDE_SUCCESS);
            aInfo->uniqueKeys = NULL;
        }

        if (aInfo->foreignKeys != NULL)
        {
            IDE_TEST(iduMemMgr::free(aInfo->foreignKeys) != IDE_SUCCESS);
            aInfo->foreignKeys = NULL;
        }

        if (aInfo->notNulls != NULL)
        {
            IDE_TEST(iduMemMgr::free(aInfo->notNulls) != IDE_SUCCESS);
            aInfo->notNulls = NULL;
        }

        /* PROJ-1107 Check Constraint ���� */
        if ( aInfo->checks != NULL )
        {
            if ( aInfo->checks->checkCondition != NULL )
            {
                IDE_TEST( iduMemMgr::free( aInfo->checks->checkCondition ) != IDE_SUCCESS );
                aInfo->checks->checkCondition = NULL;
            }
            else
            {
                /* Nothing to do */
            }

            IDE_TEST( iduMemMgr::free( aInfo->checks ) != IDE_SUCCESS );
            aInfo->checks = NULL;
        }
        else
        {
            /* Nothing to do */
        }

        if (aInfo->timestamp != NULL)
        {
            IDE_TEST(iduMemMgr::free(aInfo->timestamp) != IDE_SUCCESS);
            aInfo->timestamp = NULL;
        }

        if (aInfo->privilegeInfo != NULL)
        {
            IDE_TEST(iduMemMgr::free(aInfo->privilegeInfo) != IDE_SUCCESS);
            aInfo->privilegeInfo = NULL;
        }

        // PROJ-1359
        if ( aInfo->triggerInfo != NULL )
        {
            IDE_TEST(iduMemMgr::free( aInfo->triggerInfo ) != IDE_SUCCESS);
            aInfo->triggerInfo = NULL;
            aInfo->triggerCount = 0;
        }

        // PROJ-1502 PARTITIONED DISK TABLE
        if( aInfo->tablePartitionType == QCM_PARTITIONED_TABLE )
        {
            if( aInfo->partKeyColumns != NULL )
            {
                IDE_TEST(iduMemMgr::free( aInfo->partKeyColumns )
                         != IDE_SUCCESS);
                aInfo->partKeyColumns = NULL;
            }

            if( aInfo->partKeyColBasicInfo != NULL )
            {
                IDE_TEST(iduMemMgr::free( aInfo->partKeyColBasicInfo )
                         != IDE_SUCCESS);
                aInfo->partKeyColBasicInfo = NULL;
            }

            if( aInfo->partKeyColsFlag != NULL )
            {
                IDE_TEST(iduMemMgr::free( aInfo->partKeyColsFlag )
                         != IDE_SUCCESS);
                aInfo->partKeyColsFlag = NULL;
            }
        }
        else
        {
            // Nothing to do.
        }


        IDE_TEST(iduMemMgr::free(aInfo) != IDE_SUCCESS);
        aInfo = NULL;
    }

    return;

    IDE_EXCEPTION_END;

    IDE_DASSERT( 0 );
}

IDE_RC qcm::getQcmColumn( smiStatement * aSmiStmt,
                          qcmTableInfo * aTableInfo)
{
/***********************************************************************
 *
 * Description :
 *    makeAndSetQcmTableInfo �κ��� ȣ��Ǹ� �÷����� ĳ���� �����
 *
 * Implementation :
 *    1. SYS_COLUMNS_ ���̺��� TABLE_ID, COLUMN_NAME, IS_NULLABLE, COLUMN_ORDER
 *       �÷��� ���Ѵ�.
 *    2. QCM_COLUMNS_TABLEID_COLID_IDX_ORDER �� �ε����� ������ ��õ�
 *       table ID �� keyRange �� �����.
 *    3. �� �Ǿ� �����鼭 ��Ÿ ĳ���� �����Ѵ�.( �� �� 2���� �ε����� ��������
 *       ��õ� table ID �� ������ �ǿ� ���ؼ��� �����Ѵ�.
 *
 ***********************************************************************/

    UInt                    sStage  = 0;
    UInt                    i       = 0;
    smiRange                sRange;
    const void            * sRow;
    mtcColumn             * sColumn;
    mtcColumn             * sQcmColumnsIndexColumn;
    mtdCharType           * sDefaultValueStr;
    mtcColumn             * sColNameColInfo;
    mtcColumn             * sColIsNullColInfo;
    mtcColumn             * sColOrderColInfo;
    mtcColumn             * sColDefaultColInfo;
    mtcColumn             * sColHiddenColInfo;
    mtcColumn             * sColKeyPrevColInfo;
    mtdCharType           * sCharStr; // BUGBUG : default value.
    UInt                    sTableID;
    idBool                  sFoundRow = ID_FALSE;
    qtcMetaRangeColumn      sRangeColumn;
    smiTableCursor          sCursor;
    smiCursorProperties     sCursorProperty;

    scGRID                  sRid; // Disk Table�� ���� Record IDentifier

    sCursor.initialize();

    //----------------------------
    // sQcmColumnsIndexColumn
    //----------------------------

    IDE_TEST( smiGetTableColumns( gQcmColumns,
                                  QCM_COLUMNS_TABLE_ID_COL_ORDER,
                                  (const smiColumn**)&sQcmColumnsIndexColumn )
              != IDE_SUCCESS );

    // mtdModule ����
    IDE_TEST(mtd::moduleById( &(sQcmColumnsIndexColumn->module),
                              sQcmColumnsIndexColumn->type.dataTypeId )
             != IDE_SUCCESS);

    // mtlModule ����
    IDE_TEST(mtl::moduleById( &(sQcmColumnsIndexColumn->language),
                              sQcmColumnsIndexColumn->type.languageId )
             != IDE_SUCCESS);

    //----------------------------
    // sColNameColInfo
    //----------------------------

    IDE_TEST( smiGetTableColumns( gQcmColumns,
                                  QCM_COLUMNS_COLUMN_NAME_COL_ORDER, // COLUMN_NAME
                                  (const smiColumn**)&sColNameColInfo )
              != IDE_SUCCESS );

    // mtdModule ����
    IDE_TEST(mtd::moduleById( &(sColNameColInfo->module),
                              sColNameColInfo->type.dataTypeId )
             != IDE_SUCCESS);

    // mtdModule ����
    IDE_TEST(mtl::moduleById( &(sColNameColInfo->language),
                              sColNameColInfo->type.languageId )
             != IDE_SUCCESS);

    //----------------------------
    // sColIsNullColInfo
    //----------------------------

    IDE_TEST( smiGetTableColumns( gQcmColumns,
                                  QCM_COLUMNS_IS_NULLABLE_COL_ORDER, // IS_NULLABLE;
                                  (const smiColumn**)&sColIsNullColInfo )
              != IDE_SUCCESS );

    // mtdModule ����
    IDE_TEST(mtd::moduleById( &(sColIsNullColInfo->module),
                              sColIsNullColInfo->type.dataTypeId )
             != IDE_SUCCESS);

    IDE_TEST(mtl::moduleById( &(sColIsNullColInfo->language),
                              sColIsNullColInfo->type.languageId )
             != IDE_SUCCESS);

    //----------------------------
    // sColOrderColInfo
    //----------------------------

    IDE_TEST( smiGetTableColumns( gQcmColumns,
                                  QCM_COLUMNS_COLUMN_ORDER_COL_ORDER, // COLUMN_ORDER
                                  (const smiColumn**)&sColOrderColInfo )
              != IDE_SUCCESS );

    // mtdModule ����
    IDE_TEST(mtd::moduleById( &(sColOrderColInfo->module),
                              sColOrderColInfo->type.dataTypeId )
             != IDE_SUCCESS);

    // mtlModule
    IDE_TEST(mtl::moduleById( &(sColOrderColInfo->language),
                              sColOrderColInfo->type.languageId )
             != IDE_SUCCESS);

    //----------------------------
    // sColDefaultColInfo
    //----------------------------

    IDE_TEST( smiGetTableColumns( gQcmColumns,
                                  QCM_COLUMNS_DEFAULT_VAL_COL_ORDER,
                                  (const smiColumn**)&sColDefaultColInfo )
              != IDE_SUCCESS );
    // mtdModule ����
    // To Fix PR-5795
    IDE_TEST(mtd::moduleById( &(sColDefaultColInfo->module),
                              sColDefaultColInfo->type.dataTypeId )
             != IDE_SUCCESS);

    IDE_TEST(mtl::moduleById( &(sColDefaultColInfo->language),
                              sColDefaultColInfo->type.languageId )
             != IDE_SUCCESS);

    //----------------------------
    // sColHiddenColInfo
    //----------------------------
    /* PROJ-1090 Function-based Index */
    IDE_TEST( smiGetTableColumns( gQcmColumns,
                                  QCM_COLUMNS_IS_HIDDEN_COL_ORDER,
                                  (const smiColumn**)&sColHiddenColInfo )
              != IDE_SUCCESS );

    // mtdModule ����
    IDE_TEST( mtd::moduleById( &(sColHiddenColInfo->module),
                               sColHiddenColInfo->type.dataTypeId )
              != IDE_SUCCESS);

    IDE_TEST( mtl::moduleById( &(sColHiddenColInfo->language),
                               sColHiddenColInfo->type.languageId )
              != IDE_SUCCESS );

    //----------------------------
    // sColKeyPrevColInfo
    //----------------------------

    IDE_TEST( smiGetTableColumns( gQcmColumns,
                                  QCM_COLUMNS_IS_KEY_PRESERVED_COL_ORDER,
                                  (const smiColumn**)&sColKeyPrevColInfo )
              != IDE_SUCCESS );

    // mtdModule ����
    IDE_TEST( mtd::moduleById( &(sColKeyPrevColInfo->module),
                               sColKeyPrevColInfo->type.dataTypeId )
              != IDE_SUCCESS);

    IDE_TEST( mtl::moduleById( &(sColKeyPrevColInfo->language),
                               sColKeyPrevColInfo->type.languageId )
              != IDE_SUCCESS );

    //----------------------------
    // Meta Cache ����
    //----------------------------

    if (aTableInfo->columnCount == 0)
    {
        aTableInfo->columns       = NULL;
    }
    else
    {
        IDU_FIT_POINT( "qcm::getQcmColumn::malloc::columns",
                        idERR_ABORT_InsufficientMemory );

        IDE_TEST( iduMemMgr::malloc(IDU_MEM_QCM,
                                    ID_SIZEOF(qcmColumn) * aTableInfo->columnCount,
                                    (void**)&(aTableInfo->columns))
                  != IDE_SUCCESS);

        idlOS::memset( aTableInfo->columns,
                       0x00,
                       ID_SIZEOF(qcmColumn) * aTableInfo->columnCount );

        if (gQcmColumnsIndex[QCM_COLUMNS_TABLEID_COLID_IDX_ORDER] == NULL)
        {
            SMI_CURSOR_PROP_INIT_FOR_META_FULL_SCAN(&sCursorProperty, NULL);

            // perform a sequential scan.
            IDE_TEST(sCursor.open(aSmiStmt,
                                  gQcmColumns,
                                  NULL,
                                  smiGetRowSCN(gQcmColumns),
                                  NULL,
                                  smiGetDefaultKeyRange(),
                                  smiGetDefaultKeyRange(),
                                  smiGetDefaultFilter(),
                                  QCM_META_CURSOR_FLAG,
                                  SMI_SELECT_CURSOR,
                                  &sCursorProperty)
                     != IDE_SUCCESS);
            sStage = 1;
        }
        else
        {
            makeMetaRangeSingleColumn(
                &sRangeColumn,
                (const mtcColumn*) sQcmColumnsIndexColumn,
                (const void *) &aTableInfo->tableID,
                &sRange);

            SMI_CURSOR_PROP_INIT_FOR_META_INDEX_SCAN(&sCursorProperty, NULL);

            IDE_TEST(sCursor.open(
                         aSmiStmt,
                         gQcmColumns,
                         gQcmColumnsIndex[QCM_COLUMNS_TABLEID_COLID_IDX_ORDER],
                         smiGetRowSCN(gQcmColumns),
                         NULL,
                         &sRange,
                         smiGetDefaultKeyRange(),
                         smiGetDefaultFilter(),
                         QCM_META_CURSOR_FLAG,
                         SMI_SELECT_CURSOR,
                         &sCursorProperty) != IDE_SUCCESS);
            sStage = 1;
        }
        IDE_TEST(sCursor.beforeFirst() != IDE_SUCCESS);
        IDE_TEST(sCursor.readRow(&sRow, &sRid, SMI_FIND_NEXT) != IDE_SUCCESS);

        while (sRow != NULL)
        {
            if (gQcmColumnsIndex[QCM_COLUMNS_TABLEID_COLID_IDX_ORDER] == NULL)
            {
                // we have to filter this row.
                sTableID = *(UInt*)((UChar*)sRow +
                                    sQcmColumnsIndexColumn->column.offset);
                if (sTableID == aTableInfo->tableID)
                {
                    sFoundRow = ID_TRUE;
                }
                else
                {
                    sFoundRow = ID_FALSE;
                }
            }
            else
            {
                aTableInfo->columns[i].next = &(aTableInfo->columns[i+1]);
                sFoundRow = ID_TRUE;
            }

            if (sFoundRow == ID_TRUE)
            {
                i = * (UInt*) ((UChar*)sRow+sColOrderColInfo->column.offset);

                aTableInfo->columns[i].flag = 0;

                // BUG-25886
                // table header�� �÷��������� module�� language�� �����Ѵ�.
                IDE_TEST( smiGetTableColumns( aTableInfo->tableHandle,
                                              i,
                                              (const smiColumn**)&sColumn )
                          != IDE_SUCCESS );

                // mtdModule ����
                IDE_TEST( mtd::moduleById( &(sColumn->module),
                                           sColumn->type.dataTypeId )
                          != IDE_SUCCESS );

                // mtlModule ����
                IDE_TEST( mtl::moduleById( &(sColumn->language),
                                           sColumn->type.languageId )
                          != IDE_SUCCESS );

                // PROJ-1877 basicInfo�� ���� �����Ѵ�.
                IDU_FIT_POINT( "qcm::getQcmColumn::malloc::basicInfo",
                                idERR_ABORT_InsufficientMemory );

                IDE_TEST( iduMemMgr::malloc(IDU_MEM_QCM,
                                            ID_SIZEOF(mtcColumn),
                                            (void**)&(aTableInfo->columns[i].basicInfo))
                          != IDE_SUCCESS);

                idlOS::memcpy( aTableInfo->columns[i].basicInfo,
                               sColumn,
                               ID_SIZEOF(mtcColumn) );

                sCharStr = (mtdCharType*)
                    ((UChar*)sRow + sColNameColInfo->column.offset);
                idlOS::memcpy(aTableInfo->columns[i].name,
                              (SChar*)&(sCharStr->value),
                              sCharStr->length);
                aTableInfo->columns[i].name[sCharStr->length] = '\0';
                aTableInfo->columns[i].defaultValue = NULL;
                aTableInfo->columns[i].namePos.stmtText = NULL;
                aTableInfo->columns[i].namePos.offset = 0;
                aTableInfo->columns[i].namePos.size = 0;

                sCharStr = (mtdCharType*)
                    ((UChar*)sRow + sColIsNullColInfo->column.offset);
                if ( idlOS::strMatch( (char*)sCharStr->value,
                                      sCharStr->length,
                                      "F",
                                      1 ) == 0 )
                {
                    aTableInfo->columns[i].basicInfo->flag |=
                        MTC_COLUMN_NOTNULL_TRUE;
                }
                else
                {
                    aTableInfo->columns[i].basicInfo->flag &=
                        ~MTC_COLUMN_NOTNULL_TRUE;
                }

                if (i == aTableInfo->columnCount-1)
                {
                    aTableInfo->columns[i].next = NULL;
                }
                else
                {
                    aTableInfo->columns[i].next = &(aTableInfo->columns[i+1]);
                }

                // To Fix PR-5795
                // sDefaultValueStr = (mtdCharType *)
                //     ((SChar *)sRow + sColDefaultColInfo->column.offset);

                sDefaultValueStr = (mtdCharType *)
                    mtc::value( sColDefaultColInfo, sRow, MTD_OFFSET_USE );

                if (sDefaultValueStr->length > 0)
                {
                    IDU_FIT_POINT( "qcm::getQcmColumn::malloc::defaultValueStr",
                                    idERR_ABORT_InsufficientMemory );

                    IDE_TEST(iduMemMgr::malloc(
                            IDU_MEM_QCM,
                            (sDefaultValueStr->length) + 1,
                            (void**)&(aTableInfo->columns[i].defaultValueStr))
                        != IDE_SUCCESS);

                    idlOS::memcpy(aTableInfo->columns[i].defaultValueStr,
                                  sDefaultValueStr->value,
                                  sDefaultValueStr->length);
                    aTableInfo->columns[i].
                        defaultValueStr[sDefaultValueStr->length] = '\0';
                }
                else
                {
                    aTableInfo->columns[i].defaultValueStr = NULL;
                }

                /* PROJ-1090 Function-based Index */
                aTableInfo->columns[i].flag &= ~QCM_COLUMN_HIDDEN_COLUMN_MASK;
                sCharStr = (mtdCharType *)((UChar*)sRow + sColHiddenColInfo->column.offset);
                if ( idlOS::strMatch( (char*)sCharStr->value,
                                      sCharStr->length,
                                      "T",
                                      1 ) == 0 )
                {
                    aTableInfo->columns[i].flag |= QCM_COLUMN_HIDDEN_COLUMN_TRUE;
                }
                else
                {
                    /* Nothing to do */
                }

                // PROJ-2204 Join Update, Delete
                sCharStr = (mtdCharType *)((UChar*)sRow + sColKeyPrevColInfo->column.offset);
                if ( idlOS::strMatch( (char*)sCharStr->value,
                                      sCharStr->length,
                                      "T",
                                      1 ) == 0 )
                {
                    aTableInfo->columns[i].basicInfo->flag |=
                        MTC_COLUMN_KEY_PRESERVED_TRUE;
                }
                else
                {
                    aTableInfo->columns[i].basicInfo->flag &=
                        ~MTC_COLUMN_KEY_PRESERVED_TRUE;
                }

                // PROJ-1488 AST
                // External Module�� Data Type�� ���� ��Ÿ���̺� ����
                IDE_TEST( aTableInfo->columns[i].basicInfo->module
                          ->makeColumnInfo( aSmiStmt, aTableInfo, i )
                          != IDE_SUCCESS );

                // PROJ-1362
                // lobColumnCount�� �̹� �ʱ�ȭ �Ǿ� �ִ�.
                if ( (aTableInfo->columns[i].basicInfo->module->flag &
                      MTD_COLUMN_TYPE_MASK)
                     == MTD_COLUMN_TYPE_LOB )
                {
                    aTableInfo->lobColumnCount++;
                }
                else
                {
                    // Nothing to do.
                }

                i++;
            }
            else
            {
                // nothing to do
            }

            IDE_TEST(sCursor.readRow(&sRow, &sRid, SMI_FIND_NEXT)
                     != IDE_SUCCESS);
            sFoundRow = ID_FALSE;
        }

        sStage = 0;
        IDE_TEST(sCursor.close() != IDE_SUCCESS);
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if ( sStage == 1 )
    {
        (void)sCursor.close();
    }

    return IDE_FAILURE;
}

IDE_RC qcm::getQcmIndices( smiStatement * aSmiStmt,
                           qcmTableInfo * aTableInfo)
{
/***********************************************************************
 *
 * Description :
 *    makeAndSetQcmTableInfo �κ��� ȣ��Ǹ� ���̺��� �ε��� ĳ���� �����
 *
 * Implementation :
 *    1. SYS_INDICES_ ���̺��� TABLE_ID, INDEX_ID �÷��� ���Ѵ�.
 *    2. SYS_INDICES_ ���̺� �ε����� ������ ��õ� table ID ��
 *       keyRange �� �����.
 *    3. �� �Ǿ� �����鼭 ��Ÿ ĳ���� �����Ѵ�.( �� �� 2���� �ε�����
 *       �������� ��õ� table ID �� ������ �ǿ� ���ؼ��� �����Ѵ�.
 *
 ***********************************************************************/

    mtcColumn             * sQcmIndexTableIDCol;
    mtcColumn             * sQcmIndexIndexIDCol;
    mtcColumn             * sMtcColumnInfo;
    mtdCharType           * sCharTypeStr;
    idBool                  sFoundRow = ID_FALSE;
    UInt                    sTableID;
    qtcMetaRangeColumn      sRangeColumn;
    const void            * sRow;
    const void            * sIndexHandle;
    SInt                    sStage  = 0;
    SInt                    i       = 0;

    smiRange                sRange;
    smiTableCursor          sCursor;
    smiCursorProperties     sCursorProperty;

    smiTableSpaceAttr       sTBSAttr;
    scGRID                  sRid; // Disk Table�� ���� Record IDentifier
    UInt                  * sKeyCols;
    UInt                    k = 0;
    UInt                    c = 0;
    UInt                    sOffset = 0;
    UInt                    sTableType;

    mtcColumn       * sIsPartitionedCol; // PROJ-1502 PARTITIONED DISK TABLE
    mtdCharType     * sIsPartitionedStr;

    sCursor.initialize();

    // on createdb.
    if (gQcmIndices == NULL)
    {
        // not yet created QCM_INDICES
        aTableInfo->indices = NULL;

        return IDE_SUCCESS;
    }

    if (aTableInfo->indexCount == 0)
    {
        aTableInfo->indices = NULL;

        return IDE_SUCCESS;
    }

    sTableType = aTableInfo->tableFlag & SMI_TABLE_TYPE_MASK;

    //----------------------------
    // sQcmIndexTableIDCol
    //----------------------------

    // in createdb.
    IDE_TEST( smiGetTableColumns( gQcmIndices,
                                  QCM_INDICES_TABLE_ID_COL_ORDER,
                                  (const smiColumn**)&sQcmIndexTableIDCol )
              != IDE_SUCCESS );

    // mtdModule ����
    IDE_TEST(mtd::moduleById( &(sQcmIndexTableIDCol->module),
                              sQcmIndexTableIDCol->type.dataTypeId )
             != IDE_SUCCESS);

    //----------------------------
    // sQcmIndexIndexIDCol
    //----------------------------
    IDE_TEST( smiGetTableColumns( gQcmIndices,
                                  QCM_INDICES_INDEX_ID_COL_ORDER,
                                  (const smiColumn**)&sQcmIndexIndexIDCol )
              != IDE_SUCCESS );

    // mtdModule ����
    IDE_TEST(mtd::moduleById( &(sQcmIndexIndexIDCol->module),
                              sQcmIndexIndexIDCol->type.dataTypeId )
             != IDE_SUCCESS);

    IDU_FIT_POINT( "qcm::getQcmIndices::malloc::indices",
                    idERR_ABORT_InsufficientMemory );

    IDE_TEST(iduMemMgr::malloc(IDU_MEM_QCM,
                               ID_SIZEOF(qcmIndex) * aTableInfo->indexCount,
                               (void**)&(aTableInfo->indices))
             != IDE_SUCCESS);

    // BUG-29814
    idlOS::memset( aTableInfo->indices,
                   0,
                   ID_SIZEOF(qcmIndex) * aTableInfo->indexCount );

    if (gQcmIndicesIndex[QCM_INDICES_TABLEID_INDEXID_IDX_ORDER] == NULL)
    {
        SMI_CURSOR_PROP_INIT_FOR_META_FULL_SCAN(&sCursorProperty, NULL);

        IDE_TEST(sCursor.open(
                     aSmiStmt,
                     gQcmIndices,
                     NULL,
                     smiGetRowSCN(gQcmIndices),
                     NULL,
                     smiGetDefaultKeyRange(),
                     smiGetDefaultKeyRange(),
                     smiGetDefaultFilter(),
                     QCM_META_CURSOR_FLAG,
                     SMI_SELECT_CURSOR,
                     &sCursorProperty) != IDE_SUCCESS);
        sStage = 1;
    }
    else
    {
        makeMetaRangeSingleColumn(
            &sRangeColumn,
            (const mtcColumn*) sQcmIndexTableIDCol,
            (const void *) & (aTableInfo->tableID),
            &sRange);

        SMI_CURSOR_PROP_INIT_FOR_META_INDEX_SCAN(&sCursorProperty, NULL);

        IDE_TEST(sCursor.open(
                     aSmiStmt,
                     gQcmIndices,
                     gQcmIndicesIndex[QCM_INDICES_TABLEID_INDEXID_IDX_ORDER],
                     smiGetRowSCN(gQcmIndices),
                     NULL,
                     &sRange,
                     smiGetDefaultKeyRange(),
                     smiGetDefaultFilter(),
                     QCM_META_CURSOR_FLAG,
                     SMI_SELECT_CURSOR,
                     &sCursorProperty) != IDE_SUCCESS);
        sStage = 1;
    }

    IDE_TEST(sCursor.beforeFirst() != IDE_SUCCESS);
    IDE_TEST(sCursor.readRow(&sRow, &sRid, SMI_FIND_NEXT) != IDE_SUCCESS);

    while (sRow != NULL)
    {
        if (gQcmIndicesIndex[QCM_INDICES_TABLEID_INDEXID_IDX_ORDER] == NULL)
        {
            sTableID = *(UInt*)((UChar*)sRow +
                                sQcmIndexTableIDCol->column.offset);

            if (sTableID == aTableInfo->tableID)
            {
                sFoundRow = ID_TRUE;
            }
            else
            {
                sFoundRow = ID_FALSE;
            }
        }
        else
        {
            sFoundRow = ID_TRUE;
        }

        if (sFoundRow == ID_TRUE)
        {
            aTableInfo->indices[i].indexId = *(UInt*)
                ((UChar*)sRow + sQcmIndexIndexIDCol-> column.offset);

            //----------------------------
            // COLUMN_CNT
            //----------------------------
            IDE_TEST( smiGetTableColumns( gQcmIndices,
                                          QCM_INDICES_COLUMN_CNT_COL_ORDER,
                                          (const smiColumn**)&sMtcColumnInfo )
                      != IDE_SUCCESS );

            // mtdModule ����
            IDE_TEST(mtd::moduleById( &(sMtcColumnInfo->module),
                                      sMtcColumnInfo->type.dataTypeId )
                     != IDE_SUCCESS);

            // mtlModule ����
            IDE_TEST(mtl::moduleById( &(sMtcColumnInfo->language),
                                      sMtcColumnInfo->type.languageId )
                     != IDE_SUCCESS);

            aTableInfo->indices[i].keyColCount = *(SInt*)
                ((UChar *)sRow + sMtcColumnInfo->column.offset);

            sIndexHandle = smiGetTableIndexByID( aTableInfo->tableHandle,
                                                 aTableInfo->indices[i].indexId );

            IDU_FIT_POINT( "qcm::getQcmIndices::malloc::keyColumns",
                            idERR_ABORT_InsufficientMemory );

            IDE_TEST( iduMemMgr::malloc(IDU_MEM_QCM,
                                        ID_SIZEOF(mtcColumn) *
                                        aTableInfo->indices[i].keyColCount,
                                        (void**)&(aTableInfo->indices[i].  keyColumns))
                      != IDE_SUCCESS);

            sKeyCols = (UInt*)smiGetIndexColumns(sIndexHandle);
            sOffset = 0;
            for (k = 0; k < aTableInfo->indices[i].keyColCount; k++)
            {
                for (c = 0; c < aTableInfo->columnCount; c++)
                {
                    if (sKeyCols[k] ==
                        aTableInfo->columns[c].basicInfo->column.id)
                    {
                        break;
                    }
                }
                IDE_TEST_RAISE(c == aTableInfo->columnCount,
                               ERR_META_CRASH);

                idlOS::memcpy(&(aTableInfo->indices[i].keyColumns[k]),
                              aTableInfo->columns[c].basicInfo,
                              ID_SIZEOF(mtcColumn));

                // fix BUG-9462
                aTableInfo->indices[i].keyColumns[k].column.flag &=
                    ~SMI_COLUMN_USAGE_MASK;
                aTableInfo->indices[i].keyColumns[k].column.flag |=
                    SMI_COLUMN_USAGE_INDEX;

                if ( sTableType == SMI_TABLE_DISK )
                {
                    // PROJ-1705
                    IDE_TEST(
                        qdbCommon::setIndexKeyColumnTypeFlag(
                            &(aTableInfo->indices[i].keyColumns[k]) )
                        != IDE_SUCCESS );

                    // To Fix PR-8111
                    if ( ( aTableInfo->indices[i].keyColumns[k].column.flag &
                           SMI_COLUMN_TYPE_MASK )
                         == SMI_COLUMN_TYPE_FIXED )
                    {
                        sOffset =
                            idlOS::align( sOffset,
                                          aTableInfo->columns[c].
                                          basicInfo->module->align );
                        aTableInfo->indices[i].keyColumns[k].
                            column.offset = sOffset;
                        sOffset +=
                            aTableInfo->columns[c].basicInfo->
                            column.size;
                    }
                    else
                    {
                        sOffset = idlOS::align( sOffset, 8 );
                        aTableInfo->indices[i].keyColumns[k].
                            column.offset = sOffset;
                        sOffset +=
                            smiGetVariableColumnSize4DiskIndex();
                    }
                }
            }
            aTableInfo->indices[i].keyColsFlag = (UInt*)
                smiGetIndexColumnFlags(sIndexHandle);
            aTableInfo->indices[i].indexHandle = (void*)
                sIndexHandle;

            //----------------------------
            // BUG-16967
            // USER_ID
            //----------------------------

            IDE_TEST( smiGetTableColumns( gQcmIndices,
                                          QCM_INDICES_USER_ID_COL_ORDER,
                                          (const smiColumn**)&sMtcColumnInfo )
                      != IDE_SUCCESS );

            // mtdModule ����
            IDE_TEST(mtd::moduleById( &(sMtcColumnInfo->module),
                                      sMtcColumnInfo->type.dataTypeId )
                     != IDE_SUCCESS);

            // mtlModule ����
            IDE_TEST(mtl::moduleById( &(sMtcColumnInfo->language),
                                      sMtcColumnInfo->type.languageId )
                     != IDE_SUCCESS);

            aTableInfo->indices[i].userID =
                *(UInt*)((UChar*)sRow + sMtcColumnInfo->column.offset);

            //----------------------------
            // USER_NAME
            //----------------------------

            if ( aTableInfo->indices[i].userID == aTableInfo->tableOwnerID )
            {
                idlOS::strncpy( aTableInfo->indices[i].userName,
                                aTableInfo->tableOwnerName,
                                QC_MAX_OBJECT_NAME_LEN + 1 );
            }
            else
            {
                IDE_TEST( qcmUser::getMetaUserName( aSmiStmt,
                                                    aTableInfo->indices[i].userID,
                                                    aTableInfo->indices[i].userName )
                          != IDE_SUCCESS );
            }

            //----------------------------
            // INDEX_TYPE
            //----------------------------

            IDE_TEST( smiGetTableColumns( gQcmIndices,
                                          QCM_INDICES_INDEXTYPE_ID_COL_ORDER,
                                          (const smiColumn**)&sMtcColumnInfo )
                      != IDE_SUCCESS );

            // mtdModule ����
            IDE_TEST(mtd::moduleById( &(sMtcColumnInfo->module),
                                      sMtcColumnInfo->type.dataTypeId )
                     != IDE_SUCCESS);

            // mtlModule ����
            IDE_TEST(mtl::moduleById( &(sMtcColumnInfo->language),
                                      sMtcColumnInfo->type.languageId )
                     != IDE_SUCCESS);

            aTableInfo->indices[i].indexTypeId =
                *(UInt*)((UChar*)sRow + sMtcColumnInfo->column.offset);

            //----------------------------
            // INDEX_NAME
            //----------------------------

            IDE_TEST( smiGetTableColumns( gQcmIndices,
                                          QCM_INDICES_INDEXNAME_COL_ORDER,
                                          (const smiColumn**)&sMtcColumnInfo )
                      != IDE_SUCCESS );

            // mtdModule ����
            IDE_TEST(mtd::moduleById( &(sMtcColumnInfo->module),
                                      sMtcColumnInfo->type.dataTypeId )
                     != IDE_SUCCESS);

            // mtlModule ����
            IDE_TEST(mtl::moduleById( &(sMtcColumnInfo->language),
                                      sMtcColumnInfo->type.languageId )
                     != IDE_SUCCESS);

            sCharTypeStr = (mtdCharType*)((UChar*)sRow +
                                          sMtcColumnInfo->column.offset);

            idlOS::strncpy(aTableInfo->indices[i].name,
                           (const char*)&(sCharTypeStr->value),
                           sCharTypeStr->length);
            *(aTableInfo->indices[i].name + sCharTypeStr->length) = '\0';

            //----------------------------
            // IS_UNIQUE
            //----------------------------

            IDE_TEST( smiGetTableColumns( gQcmIndices,
                                          QCM_INDICES_ISUNIQUE_COL_ORDER,
                                          (const smiColumn**)&sMtcColumnInfo )
                      != IDE_SUCCESS );

            // mtdModule ����
            IDE_TEST(mtd::moduleById( &(sMtcColumnInfo->module),
                                      sMtcColumnInfo->type.dataTypeId )
                     != IDE_SUCCESS);

            // mtlModule ����
            IDE_TEST(mtl::moduleById( &(sMtcColumnInfo->language),
                                      sMtcColumnInfo->type.languageId )
                     != IDE_SUCCESS);

            sCharTypeStr = (mtdCharType*)((UChar*)sRow +
                                          sMtcColumnInfo->column.offset);
            if ( idlOS::strMatch( (SChar*)&(sCharTypeStr->value),
                                  sCharTypeStr->length,
                                  "T",
                                  1 ) == 0 )
            {
                aTableInfo->indices[i].isUnique = ID_TRUE;
            }
            else
            {
                aTableInfo->indices[i].isUnique = ID_FALSE;
            }

            aTableInfo->indices[i].isLocalUnique = ID_FALSE;

            //----------------------------
            // IS_RANGE
            //----------------------------

            IDE_TEST( smiGetTableColumns( gQcmIndices,
                                          QCM_INDICES_ISRANGE_COL_ORDER,
                                          (const smiColumn**)&sMtcColumnInfo )
                      != IDE_SUCCESS );

            // mtdModule ����
            IDE_TEST(mtd::moduleById( &(sMtcColumnInfo->module),
                                      sMtcColumnInfo->type.dataTypeId )
                     != IDE_SUCCESS);

            // mtlModule ����
            IDE_TEST(mtl::moduleById( &(sMtcColumnInfo->language),
                                      sMtcColumnInfo->type.languageId )
                     != IDE_SUCCESS);

            sCharTypeStr = (mtdCharType*)((UChar*)sRow +
                                          sMtcColumnInfo->column.offset);
            if ( idlOS::strMatch( (SChar*)&(sCharTypeStr->value),
                                  sCharTypeStr->length,
                                  "T",
                                  1 ) == 0 )
            {
                aTableInfo->indices[i].isRange = ID_TRUE;
            }
            else
            {
                aTableInfo->indices[i].isRange = ID_FALSE;
            }

            //----------------------------
            // TBS_ID
            //----------------------------
            IDE_TEST( smiGetTableColumns( gQcmIndices,
                                          QCM_INDICES_TBSID_COL_ORDER,
                                          (const smiColumn**)&sMtcColumnInfo )
                      != IDE_SUCCESS );
            // mtdModule ����
            IDE_TEST(mtd::moduleById( &(sMtcColumnInfo->module),
                                      sMtcColumnInfo->type.dataTypeId )
                     != IDE_SUCCESS);

            // mtlModule ����
            IDE_TEST(mtl::moduleById( &(sMtcColumnInfo->language),
                                      sMtcColumnInfo->type.languageId )
                     != IDE_SUCCESS);

            aTableInfo->indices[i].TBSID =
                qcm::getSpaceID(sRow,
                                sMtcColumnInfo->column.offset);

            // is_online
            IDE_TEST(
                qcmTablespace::getTBSAttrByID( aTableInfo->indices[i].TBSID,
                                               &sTBSAttr )
                != IDE_SUCCESS);
            if( sTBSAttr.mTBSStateOnLA != SMI_TBS_OFFLINE )
            {
                aTableInfo->indices[i].isOnlineTBS = ID_TRUE;
            }
            else
            {
                aTableInfo->indices[i].isOnlineTBS = ID_FALSE;
            }

            //----------------------------
            // PROJ-1502 PARTITIONED DISK TABLE
            // IS PARTITIONED
            //----------------------------

            IDE_TEST( smiGetTableColumns( gQcmIndices,
                                          QCM_INDICES_IS_PARTITIONED_COL_ORDER,
                                          (const smiColumn**)&sIsPartitionedCol )
                      != IDE_SUCCESS );

            sIsPartitionedStr = (mtdCharType *)
                ((UChar *)sRow + sIsPartitionedCol->column.offset);

            if ( idlOS::strMatch( (SChar*)&(sIsPartitionedStr->value),
                                  sIsPartitionedStr->length,
                                  "T",
                                  1 ) == 0 )
            {
                IDE_TEST( qcmPartition::getQcmPartitionedIndexInfo(
                              aSmiStmt,
                              &aTableInfo->indices[i] )
                          != IDE_SUCCESS );
            }
            else
            {
                aTableInfo->indices[i].indexPartitionType =
                    QCM_NONE_PARTITIONED_INDEX;
            }

            //----------------------------
            // PROJ-1624 Global Non-partitioned Index
            // INDEX_TABLE_ID
            //----------------------------

            IDE_TEST( smiGetTableColumns( gQcmIndices,
                                          QCM_INDICES_INDEX_TABLE_ID_COL_ORDER,
                                          (const smiColumn**)&sMtcColumnInfo )
                      != IDE_SUCCESS );

            // mtdModule ����
            IDE_TEST(mtd::moduleById( &(sMtcColumnInfo->module),
                                      sMtcColumnInfo->type.dataTypeId )
                     != IDE_SUCCESS);

            // mtlModule ����
            IDE_TEST(mtl::moduleById( &(sMtcColumnInfo->language),
                                      sMtcColumnInfo->type.languageId )
                     != IDE_SUCCESS);

            aTableInfo->indices[i].indexTableID =
                *(UInt*)((UChar*)sRow + sMtcColumnInfo->column.offset);

            i++;
        }

        IDE_TEST(sCursor.readRow(&sRow, &sRid, SMI_FIND_NEXT) != IDE_SUCCESS);
    }

    sStage = 0;
    IDE_TEST(sCursor.close() != IDE_SUCCESS);

    // indexCount�� i�� ���ƾ� �Ѵ�.
    IDE_DASSERT( aTableInfo->indexCount == (UInt)i );

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_META_CRASH);
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QCM_META_CRASH));
    }
    IDE_EXCEPTION_END;

    switch (sStage)
    {
        case 1:
            sCursor.close();
    }

    return IDE_FAILURE;
}

IDE_RC qcm::getQcmConstraints( smiStatement  * aSmiStmt,
                               UInt            aTableID,
                               qcmTableInfo  * aTableInfo)
{
/***********************************************************************
 *
 * Description :
 *    makeAndSetQcmTableInfo �κ��� ȣ��Ǹ� ���̺��� constraint ĳ����
 *    �����
 *
 * Implementation :
 *    1. SYS_CONSTRAINTS_ ���̺��� TABLE_ID, CONSTRAINT_NAME, CONSTRAINT_ID,
 *       INDEX_ID, COLUMN_CNT, CONSTRAINT_TYPE, REFERENCED_TABLE_ID, REFERENCED_CONSTRAINT_ID,
 *       DELETE_RULE, CHECK_CONDITION �÷��� ���Ѵ�.
 *    2. ��õ� table ID �� keyRange �� �����.
 *    3. �� �Ǿ� �����鼭 ��Ÿ ĳ��(constraint ������ ���� qcmForeignKey,
 *       qcmUnique, qcmNotNull, qcmCheck�� �� �� �ִ�) �� �����Ѵ�.
 *
 ***********************************************************************/

    smiTableCursor sCursor;
    smiRange       sRange;
    const void*    sRow;
    qtcMetaRangeColumn sRangeColumn;
    const void*    sMetaTableIndex;

    SInt           sStage = 0;
    UInt           sConstraintType;
    // column informations for sys_constraints_;
    mtcColumn    * sTableIDCol;
    mtcColumn    * sConstraintNameCol;
    mtcColumn    * sConstraintIDCol;
    mtcColumn    * sTableIndexIDCol;
    mtcColumn    * sColumnCountCol;
    mtcColumn    * sConstraintTypeCol;
    mtcColumn    * sReferencedTableIDCol;
    mtcColumn    * sReferencedConstraintIDCol;
    mtcColumn    * sDeleteRuleIDCol;
    mtcColumn    * sCheckConditionCol;  /* PROJ-1107 Check Constraint ���� */
    mtcColumn    * sValidatedCol = NULL;
    UInt           sIndexID;
    mtdCharType  * sCharStr; /* for constraint name */
    UInt           i;
    UInt           sIdxForeignKey;
    UInt           sIdxUniqueKey;
    UInt           sIdxNotNull;
    UInt           sIdxCheck;           /* PROJ-1107 Check Constraint ���� */
    UInt           sTimeStampCount = 0;

    scGRID         sRid; // Disk Table�� ���� Record IDentifier
    UInt           sQcmConstraintsColCnt;
    smiCursorProperties sCursorProperty;

    aTableInfo->uniqueKeyCount  = 0;
    aTableInfo->foreignKeyCount = 0;
    aTableInfo->notNullCount    = 0;
    aTableInfo->checkCount      = 0;    /* PROJ-1107 Check Constraint ���� */
    aTableInfo->primaryKey      = NULL;
    aTableInfo->timestamp       = NULL;

    sCursor.initialize();

    if (gQcmConstraints == NULL)
    {
        // on createdb : not yet created CONSTRAINTS META
        aTableInfo->primaryKey  = NULL;
        aTableInfo->uniqueKeys  = NULL;
        aTableInfo->foreignKeys = NULL;
        aTableInfo->notNulls    = NULL;
        aTableInfo->checks      = NULL; /* PROJ-1107 Check Constraint ���� */
        aTableInfo->timestamp   = NULL;

        return IDE_SUCCESS;
    }

    // PROJ-1874
    sQcmConstraintsColCnt = smiGetTableColumnCount( gQcmConstraints );

    IDE_TEST( smiGetTableColumns( gQcmConstraints,
                                  QCM_CONSTRAINTS_TABLE_ID_COL_ORDER,
                                  (const smiColumn**)&sTableIDCol )
              != IDE_SUCCESS );

    // mtdModule ����
    IDE_TEST(mtd::moduleById( &sTableIDCol->module,
                              sTableIDCol->type.dataTypeId )
             != IDE_SUCCESS);

    IDE_TEST( smiGetTableColumns( gQcmConstraints,
                                  QCM_CONSTRAINTS_CONSTRAINT_NAME_COL_ORDER,
                                  (const smiColumn**)&sConstraintNameCol )
              != IDE_SUCCESS );

    // mtdModule ����
    IDE_TEST(mtd::moduleById( &sConstraintNameCol->module,
                              sConstraintNameCol->type.dataTypeId )
             != IDE_SUCCESS);

    IDE_TEST( smiGetTableColumns( gQcmConstraints,
                                  QCM_CONSTRAINTS_CONSTRAINT_ID_COL_ORDER,
                                  (const smiColumn**)&sConstraintIDCol )
              != IDE_SUCCESS );
    // mtdModule ����
    IDE_TEST(mtd::moduleById( &sConstraintIDCol->module,
                              sConstraintIDCol->type.dataTypeId )
             != IDE_SUCCESS);

    IDE_TEST( smiGetTableColumns( gQcmConstraints,
                                  QCM_CONSTRAINTS_INDEX_ID_COL_ORDER,
                                  (const smiColumn**)&sTableIndexIDCol )
              != IDE_SUCCESS );
    // mtdModule ����
    IDE_TEST(mtd::moduleById( &sTableIndexIDCol->module,
                              sTableIndexIDCol->type.dataTypeId )
             != IDE_SUCCESS);

    IDE_TEST( smiGetTableColumns( gQcmConstraints,
                                  QCM_CONSTRAINTS_COLUMN_CNT_COL_ORDER,
                                  (const smiColumn**)&sColumnCountCol )
              != IDE_SUCCESS );
    // mtdModule ����
    IDE_TEST(mtd::moduleById( &sColumnCountCol->module,
                              sColumnCountCol->type.dataTypeId )
             != IDE_SUCCESS);

    IDE_TEST( smiGetTableColumns( gQcmConstraints,
                                  QCM_CONSTRAINTS_CONSTRAINT_TYPE_COL_ORDER,
                                  (const smiColumn**)&sConstraintTypeCol )
              != IDE_SUCCESS );
    // mtdModule ����
    IDE_TEST(mtd::moduleById( &sConstraintTypeCol->module,
                              sConstraintTypeCol->type.dataTypeId )
             != IDE_SUCCESS);

    IDE_TEST( smiGetTableColumns( gQcmConstraints,
                                  QCM_CONSTRAINTS_REFERENCED_TABLE_ID_COL_ORDER,
                                  (const smiColumn**)&sReferencedTableIDCol )
              != IDE_SUCCESS );
    // mtdModule ����
    IDE_TEST(mtd::moduleById( &sReferencedTableIDCol->module,
                              sReferencedTableIDCol->type.dataTypeId )
             != IDE_SUCCESS);

    IDE_TEST( smiGetTableColumns( gQcmConstraints,
                                  QCM_CONSTRAINTS_REFERENCED_CONSTRAINT_ID_COL_ORDER,
                                  (const smiColumn**)&sReferencedConstraintIDCol )
              != IDE_SUCCESS );
    // mtdModule ����
    IDE_TEST(mtd::moduleById( &sReferencedConstraintIDCol->module,
                              sReferencedConstraintIDCol->type.dataTypeId )
             != IDE_SUCCESS);

    IDE_TEST( smiGetTableColumns( gQcmConstraints,
                                  QCM_CONSTRAINTS_DELETE_RULE_COL_ORDER,
                                  (const smiColumn**)&sDeleteRuleIDCol )
              != IDE_SUCCESS );
    // mtdModule ����
    IDE_TEST(mtd::moduleById( &sDeleteRuleIDCol->module,
                              sDeleteRuleIDCol->type.dataTypeId )
             != IDE_SUCCESS );

    /* PROJ-1107 Check Constraint ���� */
    IDE_TEST( smiGetTableColumns( gQcmConstraints,
                                  QCM_CONSTRAINTS_CHECK_CONDITION_COL_ORDER,
                                  (const smiColumn**)&sCheckConditionCol )
              != IDE_SUCCESS );
    /* mtdModule ���� */
    IDE_TEST( mtd::moduleById( &sCheckConditionCol->module,
                               sCheckConditionCol->type.dataTypeId )
              != IDE_SUCCESS );

    // PROJ-1874 FK Novalidate
    // SYS_CONSTRAINTS_ Table�� ���濡 ���� ���� �������� ȣȯ�� ����
    // SYS_CONSTRAINTS_ Table�� VALIDATED �÷��� �����ϴ��� �˻��ϰ� �о������ �����Ѵ�.
    if( sQcmConstraintsColCnt >= QCM_CONSTRAINTS_VALIDATED_ORDER+1 )
    {
        IDE_TEST( smiGetTableColumns( gQcmConstraints,
                                      QCM_CONSTRAINTS_VALIDATED_ORDER,
                                      (const smiColumn**)&sValidatedCol )
                  != IDE_SUCCESS );
        // mtdModule ����
        IDE_TEST(mtd::moduleById( &sValidatedCol->module,
                                  sValidatedCol->type.dataTypeId )
                 != IDE_SUCCESS );
    }
    else
    {
        // Nothing to do.
    }

    // open table
    makeMetaRangeSingleColumn(
        &sRangeColumn,
        (const mtcColumn*) sTableIDCol,
        (const void *) & aTableID,
        &sRange);

    // find index.
    if (smiGetTableIndexCount(gQcmConstraints) == 0)
    {
        // still on cretedb
        aTableInfo->primaryKey  = NULL;
        aTableInfo->uniqueKeys  = NULL;
        aTableInfo->foreignKeys = NULL;
        aTableInfo->notNulls    = NULL;
        aTableInfo->checks      = NULL; /* PROJ-1107 Check Constraint ���� */

        return IDE_SUCCESS;
    }

    aTableInfo->primaryKey = NULL;

    // instead of using QCM_CONSTRAINTS_IDX_ON_TABLE_ID_INDEX_ID
    sMetaTableIndex =
        gQcmConstraintsIndex [ QCM_CONSTRAINTS_TABLEID_INDEXID_IDX_ORDER ] ;

    if ( sMetaTableIndex != NULL )
    {
        SMI_CURSOR_PROP_INIT_FOR_META_INDEX_SCAN(&sCursorProperty, NULL);
    }
    else
    {
        SMI_CURSOR_PROP_INIT_FOR_META_FULL_SCAN(&sCursorProperty, NULL);
    }

    IDE_TEST(sCursor.open(
                 aSmiStmt,
                 gQcmConstraints,
                 sMetaTableIndex,
                 smiGetRowSCN(gQcmConstraints),
                 NULL,
                 &sRange,
                 smiGetDefaultKeyRange(),
                 smiGetDefaultFilter(),
                 QCM_META_CURSOR_FLAG,
                 SMI_SELECT_CURSOR,
                 &sCursorProperty) != IDE_SUCCESS);
    sStage = 1;

    IDE_TEST(sCursor.beforeFirst() != IDE_SUCCESS);

    IDE_TEST(sCursor.readRow(&sRow, &sRid, SMI_FIND_NEXT) != IDE_SUCCESS);

    // count constraints
    while (sRow != NULL)
    {
        sConstraintType = *((UInt *)(
                                (UChar *)sRow + sConstraintTypeCol->column.offset));

        if (sConstraintType == QD_FOREIGN)
        {
            aTableInfo->foreignKeyCount ++;
        }
        else if( (sConstraintType == QD_PRIMARYKEY) ||
                 (sConstraintType == QD_UNIQUE) ||
                 (sConstraintType == QD_LOCAL_UNIQUE) ) // fix BUG-19187, BUG-19190
        {
            aTableInfo->uniqueKeyCount ++;
        }
        else if ( sConstraintType == QD_NOT_NULL )
        {
            aTableInfo->notNullCount ++;
        }
        else if ( sConstraintType == QD_CHECK ) /* PROJ-1107 Check Constraint ���� */
        {
            aTableInfo->checkCount++;
        }
        else if ( sConstraintType == QD_TIMESTAMP )
        {
            sTimeStampCount ++;
        }

        IDE_TEST(sCursor.readRow(&sRow, &sRid, SMI_FIND_NEXT) != IDE_SUCCESS);
    }
    // alloc.
    if (aTableInfo->foreignKeyCount != 0)
    {
        IDU_FIT_POINT( "qcm::getQcmConstraints::malloc::foreignKeys",
                        idERR_ABORT_InsufficientMemory );

        IDE_TEST(iduMemMgr::malloc(
                IDU_MEM_QCM,
                ID_SIZEOF(qcmForeignKey) * aTableInfo->foreignKeyCount,
                (void**)&(aTableInfo->foreignKeys))
            != IDE_SUCCESS);
    }
    else
    {
        aTableInfo->foreignKeys = NULL;
    }
    if (aTableInfo->uniqueKeyCount != 0)
    {
        IDU_FIT_POINT( "qcm::getQcmConstraints::malloc::uniqueKeys",
                        idERR_ABORT_InsufficientMemory );

        IDE_TEST(iduMemMgr::malloc(
                IDU_MEM_QCM,
                ID_SIZEOF(qcmUnique) * aTableInfo->uniqueKeyCount,
                (void**)&(aTableInfo->uniqueKeys))
            != IDE_SUCCESS);
    }
    else
    {
        aTableInfo->uniqueKeys = NULL;
    }
    if (aTableInfo->notNullCount != 0)
    {
        IDU_FIT_POINT( "qcm::getQcmConstraints::malloc::notNulls",
                        idERR_ABORT_InsufficientMemory );

        IDE_TEST(iduMemMgr::malloc(
                IDU_MEM_QCM,
                ID_SIZEOF(qcmNotNull) * aTableInfo->notNullCount,
                (void**)&(aTableInfo->notNulls))
            != IDE_SUCCESS);
    }
    else
    {
        aTableInfo->notNulls = NULL;
    }

    /* PROJ-1107 Check Constraint ���� */
    if ( aTableInfo->checkCount != 0 )
    {
        IDE_TEST( iduMemMgr::calloc( IDU_MEM_QCM,
                                     aTableInfo->checkCount,
                                     ID_SIZEOF(qcmCheck),
                                     (void**)&(aTableInfo->checks) )
                  != IDE_SUCCESS);
    }
    else
    {
        aTableInfo->checks = NULL;
    }

    if (sTimeStampCount != 0)
    {
        IDU_FIT_POINT( "qcm::getQcmConstraints::malloc::timestamp",
                        idERR_ABORT_InsufficientMemory );

        IDE_TEST(iduMemMgr::malloc( IDU_MEM_QCM,
                                    ID_SIZEOF(qcmTimeStamp) * sTimeStampCount,
                                    (void**)&(aTableInfo->timestamp))
                 != IDE_SUCCESS);
    }
    else
    {
        aTableInfo->timestamp = NULL;
    }

    IDE_TEST(sCursor.beforeFirst() != IDE_SUCCESS);

    IDE_TEST(sCursor.readRow(&sRow, &sRid, SMI_FIND_NEXT) != IDE_SUCCESS);
    sIdxForeignKey = 0;
    sIdxUniqueKey  = 0;
    sIdxNotNull    = 0;
    sIdxCheck      = 0;
    while (sRow != NULL)
    {
        sConstraintType = *((UInt *)(
                                (UChar *)sRow + sConstraintTypeCol->column.offset));

        if (sConstraintType == QD_FOREIGN)
        {
            IDE_TEST_RAISE( (sIdxForeignKey >= aTableInfo->foreignKeyCount) ||
                            (aTableInfo->foreignKeys == NULL),
                            ERR_META_CRASH );

            aTableInfo->foreignKeys[sIdxForeignKey].constraintID = *((UInt *)(
                                                            (UChar *)sRow + sConstraintIDCol->column.offset));

            aTableInfo->foreignKeys[sIdxForeignKey].constraintColumnCount = *((UInt *)(
                                                                     (UChar *)sRow + sColumnCountCol->column.offset));

            sCharStr = (mtdCharType *)(
                (UChar *)sRow + sConstraintNameCol->column.offset);

            idlOS::memcpy(aTableInfo->foreignKeys[sIdxForeignKey].name,
                          (SChar*)&(sCharStr->value),
                          sCharStr->length);

            aTableInfo->foreignKeys[sIdxForeignKey].name[sCharStr->length] = '\0';
            // BUGBUG scn check.
            aTableInfo->foreignKeys[sIdxForeignKey].referencedTableID =
                *(UInt *)((UChar *)sRow +
                          sReferencedTableIDCol->column.offset);

            aTableInfo->foreignKeys[sIdxForeignKey].referencedIndexID =  *((UInt *)(
                                                                  (UChar *)sRow + sReferencedConstraintIDCol->column.offset));

            aTableInfo->foreignKeys[sIdxForeignKey].referenceRule = *((UInt *)(
                                                          (UChar *)sRow + sDeleteRuleIDCol->column.offset));

            // PROJ-1874 FK Novalidate
            // SYS_CONSTRAINTS_ Table�� ���濡 ���� ���� �������� ȣȯ�� ����
            // SYS_CONSTRAINTS_ Table�� VALIDATED �÷��� �����ϴ��� �˻��ϰ� �о������ �����Ѵ�.
            if( sQcmConstraintsColCnt >= QCM_CONSTRAINTS_VALIDATED_ORDER+1 )
            {
                sCharStr = (mtdCharType *)(
                    (UChar *)sRow + sValidatedCol->column.offset);

                if ( idlOS::strMatch( (char*)sCharStr->value,
                                      sCharStr->length,
                                      "T",
                                      1 ) == 0 )
                {
                    aTableInfo->foreignKeys[sIdxForeignKey].validated = ID_TRUE;
                }
                else
                {
                    aTableInfo->foreignKeys[sIdxForeignKey].validated = ID_FALSE;
                }
            }
            else
            {
                // ���� ���� ȣȯ���� ���� �÷� ������ ���� ����Ʈ ���� �ִ´�.
                aTableInfo->foreignKeys[sIdxForeignKey].validated = ID_TRUE;
            }

            IDE_TEST( getQcmConstraintColumn(
                          aSmiStmt,
                          aTableInfo->foreignKeys[sIdxForeignKey].constraintID,
                          (aTableInfo->foreignKeys[sIdxForeignKey].referencingColumn))
                      != IDE_SUCCESS);
            sIdxForeignKey++;
        }
        else if( (sConstraintType == QD_PRIMARYKEY) ||
                 (sConstraintType == QD_UNIQUE) ||
                 (sConstraintType == QD_LOCAL_UNIQUE) ) // fix BUG-19187, BUG-19190
        {
            IDE_TEST_RAISE( (sIdxUniqueKey >= aTableInfo->uniqueKeyCount) ||
                            (aTableInfo->uniqueKeys == NULL),
                            ERR_META_CRASH );

            aTableInfo->uniqueKeys[sIdxUniqueKey].constraintType = sConstraintType;

            aTableInfo->uniqueKeys[sIdxUniqueKey].constraintID = *((UInt *)(
                                                           (UChar *)sRow + sConstraintIDCol->column.offset));

            aTableInfo->uniqueKeys[sIdxUniqueKey].constraintColumnCount = *((UInt *)(
                                                                    (UChar *)sRow + sColumnCountCol->column.offset));

            sIndexID =  *((UInt *)(
                              (UChar *)sRow + sTableIndexIDCol->column.offset));

            sCharStr = (mtdCharType *)(
                (UChar *)sRow + sConstraintNameCol->column.offset);

            idlOS::memcpy(aTableInfo->uniqueKeys[sIdxUniqueKey].name,
                          (SChar*)&(sCharStr->value),
                          sCharStr->length);

            aTableInfo->uniqueKeys[sIdxUniqueKey].name[sCharStr->length] = '\0';

            IDE_TEST(
                getQcmConstraintColumn(
                    aSmiStmt,
                    aTableInfo->uniqueKeys[sIdxUniqueKey].constraintID,
                    aTableInfo->uniqueKeys[sIdxUniqueKey].constraintColumn)
                != IDE_SUCCESS);

            for ( i = 0; i < aTableInfo->indexCount; i++)
            {
                if (aTableInfo->indices[i].indexId == sIndexID)
                {
                    aTableInfo->uniqueKeys[sIdxUniqueKey].constraintIndex =
                        &aTableInfo->indices[i];
                    if (sConstraintType == QD_PRIMARYKEY)
                    {
                        aTableInfo->primaryKey =
                            &aTableInfo->indices[i];
                    }
                    break;
                }
            }
            sIdxUniqueKey++;
        }
        else if ( sConstraintType == QD_NOT_NULL )
        {
            IDE_TEST_RAISE( (sIdxNotNull >= aTableInfo->notNullCount) ||
                            (aTableInfo->notNulls == NULL),
                            ERR_META_CRASH );

            aTableInfo->notNulls[sIdxNotNull].constraintID = *((UInt *)(
                                                                   (UChar *)sRow + sConstraintIDCol->column.offset));

            aTableInfo->notNulls[sIdxNotNull].constraintType = QD_NOT_NULL;

            aTableInfo->notNulls[sIdxNotNull].constraintColumnCount =
                *((UInt *)((UChar *)sRow + sColumnCountCol->column.offset));

            sCharStr = (mtdCharType *)(
                (UChar *)sRow + sConstraintNameCol->column.offset);

            idlOS::memcpy(aTableInfo->notNulls[sIdxNotNull].name,
                          (SChar*)&(sCharStr->value),
                          sCharStr->length);

            aTableInfo->notNulls[sIdxNotNull].name[sCharStr->length] = '\0';

            IDE_TEST(
                getQcmConstraintColumn(
                    aSmiStmt,
                    aTableInfo->notNulls[sIdxNotNull].constraintID,
                    aTableInfo->notNulls[sIdxNotNull].constraintColumn)
                != IDE_SUCCESS);
            sIdxNotNull++;
        }
        /* PROJ-1107 Check Constraint ���� */
        else if ( sConstraintType == QD_CHECK )
        {
            IDE_TEST_RAISE( (sIdxCheck >= aTableInfo->checkCount) ||
                            (aTableInfo->checks == NULL),
                            ERR_META_CRASH );

            sCharStr = (mtdCharType *)(
                (UChar *)sRow + sConstraintNameCol->column.offset);

            idlOS::memcpy( aTableInfo->checks[sIdxCheck].name,
                           (SChar*)&(sCharStr->value),
                           sCharStr->length );
            aTableInfo->checks[sIdxCheck].name[sCharStr->length] = '\0';

            aTableInfo->checks[sIdxCheck].constraintID =
                *((UInt *)((UChar *)sRow + sConstraintIDCol->column.offset));

            IDE_TEST( getQcmConstraintColumn(
                            aSmiStmt,
                            aTableInfo->checks[sIdxCheck].constraintID,
                            aTableInfo->checks[sIdxCheck].constraintColumn )
                      != IDE_SUCCESS );

            sCharStr = (mtdCharType *) mtc::value( sCheckConditionCol,
                                                   sRow,
                                                   MTD_OFFSET_USE );
            IDE_TEST( iduMemMgr::malloc(
                    IDU_MEM_QCM,
                    sCharStr->length + 1,
                    (void**)&(aTableInfo->checks[sIdxCheck].checkCondition) )
                != IDE_SUCCESS);
            idlOS::memcpy( aTableInfo->checks[sIdxCheck].checkCondition,
                           (SChar*)&(sCharStr->value),
                           sCharStr->length );
            aTableInfo->checks[sIdxCheck].checkCondition[sCharStr->length] = '\0';

            aTableInfo->checks[sIdxCheck].constraintColumnCount =
                *((UInt *)((UChar *)sRow + sColumnCountCol->column.offset));

            sIdxCheck++;
        }
        else if ( sConstraintType == QD_TIMESTAMP )
        {
            IDE_TEST_RAISE( aTableInfo->timestamp == NULL,
                            ERR_META_CRASH );

            aTableInfo->timestamp->constraintID = *((UInt *)(
                                                        (UChar *)sRow + sConstraintIDCol->column.offset));

            aTableInfo->timestamp->constraintColumnCount =
                *((UInt *)((UChar *)sRow + sColumnCountCol->column.offset));

            sCharStr = (mtdCharType *)(
                (UChar *)sRow + sConstraintNameCol->column.offset);

            idlOS::memcpy(aTableInfo->timestamp->name,
                          (SChar*)&(sCharStr->value),
                          sCharStr->length);

            aTableInfo->timestamp->name[sCharStr->length] = '\0';

            IDE_TEST(
                getQcmConstraintColumn(
                    aSmiStmt,
                    aTableInfo->timestamp->constraintID,
                    aTableInfo->timestamp->constraintColumn)
                != IDE_SUCCESS);
        }

        IDE_TEST(sCursor.readRow(&sRow, &sRid, SMI_FIND_NEXT) != IDE_SUCCESS);
    }
    sStage = 0;
    IDE_TEST(sCursor.close() != IDE_SUCCESS);

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_META_CRASH);
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QCM_META_CRASH));
    }
    IDE_EXCEPTION_END;

    switch (sStage)
    {
        case 1 :
            sCursor.close();
    }

    return IDE_FAILURE;
}

IDE_RC qcm::getQcmLocalUniqueByCols( smiStatement  * aSmiStmt,
                                     qcmTableInfo  * aTableInfo,
                                     UInt            aKeyColCount,
                                     UInt          * aKeyCols,
                                     UInt          * aKeyColsFlag,
                                     qcmUnique     * aLocalUnique )
{
/***********************************************************************
 *
 * Description :
 *    makeAndSetQcmTableInfo �κ��� ȣ��Ǹ� ���̺��� constraint ĳ����
 *    �����
 *
 * Implementation :
 *    1. SYS_CONSTRAINTS_ ���̺��� TABLE_ID, CONSTRAINT_NAME, CONSTRAINT_ID,
 *       INDEX_ID, COLUMN_CNT,REFERENCED_TABLE_ID,REFERENCED_CONSTRAINT_ID
 *       �÷��� ���Ѵ�.
 *    2. ��õ� table ID �� keyRange �� �����.
 *    3. �� �Ǿ� �����鼭 ��Ÿ ĳ��(constraint ������ ���� qcmForeignKey,
 *       qcmUnique,qcmNotNull �� �� �� �ִ�) �� �����Ѵ�.
 *
 ***********************************************************************/

    smiTableCursor sCursor;
    smiRange       sRange;
    const void*    sRow;
    qtcMetaRangeColumn sRangeColumn;
    const void*    sMetaTableIndex;

    SInt           sStage = 0;
    UInt           sConstraintType;
    // column informations for sys_constraints_;
    mtcColumn    * sTableIDCol;
    mtcColumn    * sConstraintNameCol;
    mtcColumn    * sConstraintIDCol;
    mtcColumn    * sTableIndexIDCol;
    mtcColumn    * sColumnCountCol;
    mtcColumn    * sConstraintTypeCol;
    mtcColumn    * sReferencedTableIDCol;
    mtcColumn    * sReferencedConstraintIDCol;
    mtcColumn    * sDeleteRuleIDCol;
    UInt           sIndexID;
    mtdCharType  * sCharStr; /* for constraint name */
    UInt           j;
    UInt           k;
    UInt           sTableID;
    idBool         sMatchFound;
    qcmIndex     * sIndex;
    qcmUnique      sLocalUnique;
    scGRID         sRid; // Disk Table�� ���� Record IDentifier
    idBool         sFound;
    smiCursorProperties sCursorProperty;

    sCursor.initialize();
    sTableID = aTableInfo->tableID;

    IDE_TEST( smiGetTableColumns( gQcmConstraints,
                                  QCM_CONSTRAINTS_TABLE_ID_COL_ORDER,
                                  (const smiColumn**)&sTableIDCol )
              != IDE_SUCCESS );

    // mtdModule ����
    IDE_TEST(mtd::moduleById( &sTableIDCol->module,
                              sTableIDCol->type.dataTypeId )
             != IDE_SUCCESS);

    IDE_TEST( smiGetTableColumns( gQcmConstraints,
                                  QCM_CONSTRAINTS_CONSTRAINT_NAME_COL_ORDER,
                                  (const smiColumn**)&sConstraintNameCol )
              != IDE_SUCCESS );
    // mtdModule ����
    IDE_TEST(mtd::moduleById( &sConstraintNameCol->module,
                              sConstraintNameCol->type.dataTypeId )
             != IDE_SUCCESS);

    IDE_TEST( smiGetTableColumns( gQcmConstraints,
                                  QCM_CONSTRAINTS_CONSTRAINT_ID_COL_ORDER,
                                  (const smiColumn**)&sConstraintIDCol )
              != IDE_SUCCESS );
    // mtdModule ����
    IDE_TEST(mtd::moduleById( &sConstraintIDCol->module,
                              sConstraintIDCol->type.dataTypeId )
             != IDE_SUCCESS);

    IDE_TEST( smiGetTableColumns( gQcmConstraints,
                                  QCM_CONSTRAINTS_INDEX_ID_COL_ORDER,
                                  (const smiColumn**)&sTableIndexIDCol )
              != IDE_SUCCESS );
    // mtdModule ����
    IDE_TEST(mtd::moduleById( &sTableIndexIDCol->module,
                              sTableIndexIDCol->type.dataTypeId )
             != IDE_SUCCESS);

    IDE_TEST( smiGetTableColumns( gQcmConstraints,
                                  QCM_CONSTRAINTS_COLUMN_CNT_COL_ORDER,
                                  (const smiColumn**)&sColumnCountCol )
              != IDE_SUCCESS );
    // mtdModule ����
    IDE_TEST(mtd::moduleById( &sColumnCountCol->module,
                              sColumnCountCol->type.dataTypeId )
             != IDE_SUCCESS);

    IDE_TEST( smiGetTableColumns( gQcmConstraints,
                                  QCM_CONSTRAINTS_CONSTRAINT_TYPE_COL_ORDER,
                                  (const smiColumn**)&sConstraintTypeCol )
              != IDE_SUCCESS );
    // mtdModule ����
    IDE_TEST(mtd::moduleById( &sConstraintTypeCol->module,
                              sConstraintTypeCol->type.dataTypeId )
             != IDE_SUCCESS);

    IDE_TEST( smiGetTableColumns( gQcmConstraints,
                                  QCM_CONSTRAINTS_REFERENCED_TABLE_ID_COL_ORDER,
                                  (const smiColumn**)&sReferencedTableIDCol )
              != IDE_SUCCESS );
    // mtdModule ����
    IDE_TEST(mtd::moduleById( &sReferencedTableIDCol->module,
                              sReferencedTableIDCol->type.dataTypeId )
             != IDE_SUCCESS);

    IDE_TEST( smiGetTableColumns( gQcmConstraints,
                                  QCM_CONSTRAINTS_REFERENCED_CONSTRAINT_ID_COL_ORDER,
                                  (const smiColumn**)&sReferencedConstraintIDCol )
              != IDE_SUCCESS );
    // mtdModule ����
    IDE_TEST(mtd::moduleById( &sReferencedConstraintIDCol->module,
                              sReferencedConstraintIDCol->type.dataTypeId )
             != IDE_SUCCESS);

    IDE_TEST( smiGetTableColumns( gQcmConstraints,
                                  QCM_CONSTRAINTS_DELETE_RULE_COL_ORDER,
                                  (const smiColumn**)&sDeleteRuleIDCol )
              != IDE_SUCCESS );
    // mtdModule ����
    IDE_TEST(mtd::moduleById( &sDeleteRuleIDCol->module,
                              sDeleteRuleIDCol->type.dataTypeId )
             != IDE_SUCCESS );

    // open table
    makeMetaRangeSingleColumn(
        &sRangeColumn,
        (const mtcColumn*) sTableIDCol,
        (const void *) & sTableID,
        &sRange);
    // instead of using QCM_CONSTRAINTS_IDX_ON_TABLE_ID_INDEX_ID
    sMetaTableIndex =
        gQcmConstraintsIndex [ QCM_CONSTRAINTS_TABLEID_INDEXID_IDX_ORDER ] ;

    if ( sMetaTableIndex != NULL )
    {
        SMI_CURSOR_PROP_INIT_FOR_META_INDEX_SCAN(&sCursorProperty, NULL);
    }
    else
    {
        SMI_CURSOR_PROP_INIT_FOR_META_FULL_SCAN(&sCursorProperty, NULL);
    }

    IDE_TEST(sCursor.open(
                 aSmiStmt,
                 gQcmConstraints,
                 sMetaTableIndex,
                 smiGetRowSCN(gQcmConstraints),
                 NULL,
                 &sRange,
                 smiGetDefaultKeyRange(),
                 smiGetDefaultFilter(),
                 QCM_META_CURSOR_FLAG,
                 SMI_SELECT_CURSOR,
                 &sCursorProperty) != IDE_SUCCESS);
    sStage = 1;

    IDE_TEST(sCursor.beforeFirst() != IDE_SUCCESS);

    IDE_TEST(sCursor.readRow(&sRow, &sRid, SMI_FIND_NEXT) != IDE_SUCCESS);

    // count constraints
    while (sRow != NULL)
    {
        sConstraintType = *((UInt *)(
                                (UChar *)sRow + sConstraintTypeCol->column.offset));

        if (sConstraintType == QD_LOCAL_UNIQUE)
        {
            sLocalUnique.constraintType = sConstraintType;
            sLocalUnique.constraintID = *((UInt *)(
                                              (UChar *)sRow + sConstraintIDCol->column.offset));

            sLocalUnique.constraintColumnCount = *((UInt *)(
                                                       (UChar *)sRow + sColumnCountCol->column.offset));

            sIndexID =  *((UInt *)(
                              (UChar *)sRow + sTableIndexIDCol->column.offset));

            sCharStr = (mtdCharType *)(
                (UChar *)sRow + sConstraintNameCol->column.offset);

            idlOS::memcpy(sLocalUnique.name,
                          (SChar*)&(sCharStr->value),
                          sCharStr->length);

            sLocalUnique.name[sCharStr->length] = '\0';

            IDE_TEST( getQcmConstraintColumn( aSmiStmt,
                                              sLocalUnique.constraintID,
                                              sLocalUnique.constraintColumn )
                      != IDE_SUCCESS);

            sFound = ID_FALSE;
            for ( k = 0; k < aTableInfo->indexCount; k++)
            {
                if (aTableInfo->indices[k].indexId == sIndexID)
                {
                    sLocalUnique.constraintIndex = &aTableInfo->indices[k];
                    sFound = ID_TRUE;
                    break;
                }
            }

            IDE_TEST_RAISE( sFound == ID_FALSE,
                            ERR_NOT_EXIST_MATCHED_LOCAL_UNIQUE_KEY );

            sIndex = sLocalUnique.constraintIndex;

            if( sIndex->keyColCount == aKeyColCount )
            {
                sMatchFound = ID_TRUE;
                for( j = 0; j < aKeyColCount; j++ )
                {
                    if( (sIndex->keyColumns[j].column.id != aKeyCols[j]) ||
                        (sIndex->keyColsFlag[j] & SMI_COLUMN_ORDER_MASK)
                        != (aKeyColsFlag[j] & SMI_COLUMN_ORDER_MASK) )
                    {
                        sMatchFound = ID_FALSE;
                    }
                }
                if( sMatchFound == ID_TRUE )
                {
                    *aLocalUnique = sLocalUnique;
                    IDE_CONT( MATCH_FOUND );
                }
            }
        }

        IDE_TEST(sCursor.readRow(&sRow, &sRid, SMI_FIND_NEXT) != IDE_SUCCESS);
    }

    IDE_RAISE( ERR_NOT_EXIST_LOCAL_UNIQUE_KEY );

    IDE_EXCEPTION_CONT( MATCH_FOUND );

    sStage = 0;
    sCursor.close();

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_NOT_EXIST_LOCAL_UNIQUE_KEY );
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QDN_NOT_EXISTS_LOCAL_UNIQUE_KEY));
    }
    IDE_EXCEPTION( ERR_NOT_EXIST_MATCHED_LOCAL_UNIQUE_KEY );
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qcm::getQcmLocalUniqueByCols",
                                  "Not exist matched local unique key" ));
    }

    IDE_EXCEPTION_END;

    switch (sStage)
    {
        case 1 :
            sCursor.close();
    }

    return IDE_FAILURE;
}

IDE_RC qcm::getQcmConstraintColumn( smiStatement  * aSmiStmt,
                                    UInt            aConstrID,
                                    UInt          * aColumns)
{
/***********************************************************************
 *
 * Description :
 *    constraint �� �����ϴ� �÷��� ���� ������ ĳ���Ѵ�.
 *
 * Implementation :
 *    1. SYS_CONSTRAINT_COLUMNS_ ���̺��� CONSTRAINT_ID, COLUMN_ID,
 *       CONSTRAINT_COL_ORDER �÷��� ���Ѵ�.
 *    2. ��õ� aConstrID �� keyRange �� �����.
 *    3. �� �Ǿ� �����鼭 ��Ÿ ĳ��( �÷� ID �� �迭, aColumns ) �� �����Ѵ�.
 *
 ***********************************************************************/

    smSCN          sSCN;

    smiTableCursor sCursor;
    smiRange       sRange;
    void*    sRow;
    qtcMetaRangeColumn sRangeColumn;
    UInt           i;
    SInt           sStage = 0;
    const void*    sMetaTableIndex;
    mtcColumn    * sConstraintColumnConstraintID;
    mtcColumn    * sColumnIDCol;
    mtcColumn    * sColumnOrderCol;

    scGRID         sRid; // Disk Table�� ���� Record IDentifier
    smiCursorProperties sCursorProperty;

    sCursor.initialize();

    if (gQcmConstraintColumns == NULL)
    {
        IDE_TEST(getTableHandleByName(aSmiStmt,
                                      QC_SYSTEM_USER_ID,
                                      (UChar *) QCM_CONSTRAINT_COLUMNS,
                                      idlOS::strlen( (SChar*)
                                                     QCM_CONSTRAINT_COLUMNS),
                                      (void**)&gQcmConstraintColumns,
                                      &sSCN)
                 != IDE_SUCCESS);
    }


    IDE_TEST( smiGetTableColumns( gQcmConstraintColumns,
                                  QCM_CONSTRAINT_COLUMNS_CONSTRAINT_ID_COL_ORDER,
                                  (const smiColumn**)&sConstraintColumnConstraintID )
              != IDE_SUCCESS );
    // mtdModule ����
    IDE_TEST(mtd::moduleById( &sConstraintColumnConstraintID->module,
                              sConstraintColumnConstraintID->type.dataTypeId )
             != IDE_SUCCESS);

    IDE_TEST( smiGetTableColumns( gQcmConstraintColumns,
                                  QCM_CONSTRAINT_COLUMNS_COLUMN_ID_COL_ORDER,
                                  (const smiColumn**)&sColumnIDCol )
              != IDE_SUCCESS );
    // mtdModule ����
    IDE_TEST(mtd::moduleById( &sColumnIDCol->module,
                              sColumnIDCol->type.dataTypeId )
             != IDE_SUCCESS);


    IDE_TEST( smiGetTableColumns( gQcmConstraintColumns,
                                  QCM_CONSTRAINT_COLUMNS_CONSTRAINT_COL_ORDER,
                                  (const smiColumn**)&sColumnOrderCol )
              != IDE_SUCCESS );
    // mtdModule ����
    IDE_TEST(mtd::moduleById( &sColumnOrderCol->module,
                              sColumnOrderCol->type.dataTypeId )
             != IDE_SUCCESS);


    makeMetaRangeSingleColumn(
        &sRangeColumn,
        (const mtcColumn *) sConstraintColumnConstraintID,
        (const void *) &aConstrID,
        &sRange);

    // instead of using QCM_CONSTRAINT_COLUMNS_IDX
    sMetaTableIndex =
        gQcmConstraintColumnsIndex
        [QCM_CONSTRAINT_COLUMNS_CONSTRAINT_ID_CONSTRAINT_COL_ORDER_IDX_ORDER];

    if ( sMetaTableIndex != NULL )
    {
        SMI_CURSOR_PROP_INIT_FOR_META_INDEX_SCAN(&sCursorProperty, NULL);
    }
    else
    {
        SMI_CURSOR_PROP_INIT_FOR_META_FULL_SCAN(&sCursorProperty, NULL);
    }

    IDE_TEST(sCursor.open(aSmiStmt,
                          gQcmConstraintColumns,
                          sMetaTableIndex,
                          smiGetRowSCN(gQcmConstraintColumns),
                          NULL,
                          &sRange,
                          smiGetDefaultKeyRange(),
                          smiGetDefaultFilter(),
                          QCM_META_CURSOR_FLAG,
                          SMI_SELECT_CURSOR,
                          &sCursorProperty)
             != IDE_SUCCESS);
    sStage = 1;
    IDE_TEST(sCursor.beforeFirst() != IDE_SUCCESS);

    IDE_TEST(sCursor.readRow((const void**)&sRow, &sRid, SMI_FIND_NEXT)
             != IDE_SUCCESS);

    for (i = 0; i < QC_MAX_KEY_COLUMN_COUNT; i ++)
    {
        aColumns[i] = 0;
    }

    while (sRow != NULL)
    {
        i = *(UInt *) ((UChar *) sRow + sColumnOrderCol->column.offset);
        aColumns[i] = * ((UInt *) ((UChar *) sRow +
                                   sColumnIDCol->column.offset));
        IDE_TEST(sCursor.readRow((const void **)&sRow, &sRid, SMI_FIND_NEXT)
                 != IDE_SUCCESS);
    }
    sStage = 0;

    IDE_TEST(sCursor.close() != IDE_SUCCESS);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    switch (sStage)
    {
        case 1 :
            sCursor.close();
    }

    return IDE_FAILURE;
}

IDE_RC qcm::finiMetaCaches(smiStatement * aSmiStmt)
{
/***********************************************************************
 *
 * Description :
 *    constraint �� �����ϴ� �÷��� ���� ������ ĳ���Ѵ�.
 *
 * Implementation :
 *    1. SYS_TABLES_ �� ��� tuple �� �д´�.
 *    2. �� �Ǿ� �����鼭 �� ���̺��� ��Ÿ ĳ���� destroy �Ѵ�.
 *
 ***********************************************************************/

    UInt              sStage = 0;
    smiTableCursor    sCursor;
    const void      * sRow;
    smOID             sTableOID;
    ULong             sTableOIDULong;
    const mtcColumn * sQcmTablesColumnInfo;
    qcmTableInfo    * sTableInfo;
    const void      * sTableHandle;

    scGRID            sRid; // Disk Table�� ���� Record IDentifier

    sCursor.initialize();

    // read all tuple in SYS_TABLES_
    IDE_TEST(sCursor.open(aSmiStmt,
                          gQcmTables,
                          NULL,
                          smiGetRowSCN(gQcmTables),
                          NULL,
                          smiGetDefaultKeyRange(),
                          smiGetDefaultKeyRange(),
                          smiGetDefaultFilter(),
                          QCM_META_CURSOR_FLAG,
                          SMI_SELECT_CURSOR,
                          &gMetaDefaultCursorProperty)
             != IDE_SUCCESS);
    sStage = 1;

    IDE_TEST(sCursor.beforeFirst() != IDE_SUCCESS);

    IDE_TEST(sCursor.readRow(&sRow, &sRid, SMI_FIND_NEXT) != IDE_SUCCESS);

    // table OID
    IDE_TEST( smiGetTableColumns( gQcmTables,
                                  QCM_TABLES_TABLE_OID_COL_ORDER,
                                  (const smiColumn**)&sQcmTablesColumnInfo )
              != IDE_SUCCESS );
    while (sRow != NULL)
    {
        sTableOIDULong = *(ULong*)((UChar*)sRow +
                                   sQcmTablesColumnInfo->column.offset);
        sTableOID = (smOID)sTableOIDULong;

        sTableHandle = smiGetTable( sTableOID );
        IDE_TEST( smiGetTableTempInfo( sTableHandle,
                                       (void**)&sTableInfo)
                  != IDE_SUCCESS );
 
        if( sTableInfo != NULL )
        {
            if( sTableInfo->tablePartitionType == QCM_PARTITIONED_TABLE )
            {
                IDE_TEST( qcmPartition::destroyQcmPartitionInfoByTableID(
                              aSmiStmt,
                              sTableInfo->tableID )
                          != IDE_SUCCESS );
            }
            else
            {
                // Nothing to do.
            }
 
            (void)destroyQcmTableInfo(sTableInfo);
        }
        else
        {
            // Nothing to do.
        }

        IDE_TEST(sCursor.readRow(&sRow, &sRid, SMI_FIND_NEXT) != IDE_SUCCESS);
    }

    sStage = 0;
    IDE_TEST(sCursor.close() != IDE_SUCCESS);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if ( sStage == 1 )
    {
        (void)sCursor.close();
    }

    return IDE_FAILURE;
}

// BUG-13725
void qcm::setOperatableFlag( qcmTableType   aTableType,
                             UInt         * aOperatableFlag )
{
/***********************************************************************
 *
 * Description :
 *
 * Implementation :
 *
 ***********************************************************************/

    UInt sOperatableFlag = 0;   
 
    if( aTableType == QCM_USER_TABLE )
    {
       sOperatableFlag |= (QCM_OPERATABLE_QP_REPL_ENABLE);
       sOperatableFlag |= (QCM_OPERATABLE_QP_LOCK_TABLE_ENABLE);
       sOperatableFlag |= (QCM_OPERATABLE_QP_CREATE_TABLE_ENABLE);
       sOperatableFlag |= (QCM_OPERATABLE_QP_DROP_TABLE_ENABLE);
       sOperatableFlag |= (QCM_OPERATABLE_QP_TRUNC_TABLE_ENABLE);
       sOperatableFlag |= (QCM_OPERATABLE_QP_ALTER_TABLE_ENABLE);
       sOperatableFlag |= (QCM_OPERATABLE_QP_CREATE_VIEW_DISABLE);
       sOperatableFlag |= (QCM_OPERATABLE_QP_ALTER_VIEW_DISABLE);
       sOperatableFlag |= (QCM_OPERATABLE_QP_DROP_VIEW_DISABLE);
       sOperatableFlag |= (QCM_OPERATABLE_QP_INSERT_ENABLE);
       sOperatableFlag |= (QCM_OPERATABLE_QP_UPDATE_ENABLE);
       sOperatableFlag |= (QCM_OPERATABLE_QP_DELETE_ENABLE);
       sOperatableFlag |= (QCM_OPERATABLE_QP_MOVE_ENABLE);
       sOperatableFlag |= (QCM_OPERATABLE_QP_SELECT_ENABLE);
       sOperatableFlag |= (QCM_OPERATABLE_QP_COMMENT_ENABLE);
       sOperatableFlag |= (QCM_OPERATABLE_QP_PURGE_DISABLE);
       sOperatableFlag |= (QCM_OPERATABLE_QP_FLASHBACK_DISABLE);
       sOperatableFlag |= (QCM_OPERATABLE_QP_FT_TRANS_DISABLE);
    }
    else if( aTableType == QCM_META_TABLE )
    {
       sOperatableFlag |= (QCM_OPERATABLE_QP_REPL_DISABLE);
       sOperatableFlag |= (QCM_OPERATABLE_QP_LOCK_TABLE_DISABLE);
       sOperatableFlag |= (QCM_OPERATABLE_QP_CREATE_TABLE_ENABLE);
       sOperatableFlag |= (QCM_OPERATABLE_QP_DROP_TABLE_ENABLE);
       sOperatableFlag |= (QCM_OPERATABLE_QP_TRUNC_TABLE_ENABLE);
       sOperatableFlag |= (QCM_OPERATABLE_QP_ALTER_TABLE_ENABLE);
       sOperatableFlag |= (QCM_OPERATABLE_QP_CREATE_VIEW_DISABLE);
       sOperatableFlag |= (QCM_OPERATABLE_QP_ALTER_VIEW_DISABLE);
       sOperatableFlag |= (QCM_OPERATABLE_QP_DROP_VIEW_DISABLE);
       sOperatableFlag |= (QCM_OPERATABLE_QP_INSERT_ENABLE);
       sOperatableFlag |= (QCM_OPERATABLE_QP_UPDATE_ENABLE);
       sOperatableFlag |= (QCM_OPERATABLE_QP_DELETE_ENABLE);
       sOperatableFlag |= (QCM_OPERATABLE_QP_MOVE_DISABLE);
       sOperatableFlag |= (QCM_OPERATABLE_QP_SELECT_ENABLE);
       sOperatableFlag |= (QCM_OPERATABLE_QP_COMMENT_DISABLE);
       sOperatableFlag |= (QCM_OPERATABLE_QP_PURGE_DISABLE);
       sOperatableFlag |= (QCM_OPERATABLE_QP_FLASHBACK_DISABLE);
       sOperatableFlag |= (QCM_OPERATABLE_QP_FT_TRANS_DISABLE);
    }
    else if( aTableType == QCM_VIEW )
    {
       sOperatableFlag |= (QCM_OPERATABLE_QP_REPL_DISABLE);
       sOperatableFlag |= (QCM_OPERATABLE_QP_LOCK_TABLE_ENABLE);
       sOperatableFlag |= (QCM_OPERATABLE_QP_CREATE_TABLE_DISABLE);
       sOperatableFlag |= (QCM_OPERATABLE_QP_DROP_TABLE_DISABLE);
       sOperatableFlag |= (QCM_OPERATABLE_QP_TRUNC_TABLE_DISABLE);
       sOperatableFlag |= (QCM_OPERATABLE_QP_ALTER_TABLE_DISABLE);
       sOperatableFlag |= (QCM_OPERATABLE_QP_CREATE_VIEW_ENABLE);
       sOperatableFlag |= (QCM_OPERATABLE_QP_ALTER_VIEW_ENABLE);
       sOperatableFlag |= (QCM_OPERATABLE_QP_DROP_VIEW_ENABLE);
       sOperatableFlag |= (QCM_OPERATABLE_QP_INSERT_DISABLE);
       sOperatableFlag |= (QCM_OPERATABLE_QP_UPDATE_DISABLE);
       sOperatableFlag |= (QCM_OPERATABLE_QP_DELETE_DISABLE);
       sOperatableFlag |= (QCM_OPERATABLE_QP_MOVE_DISABLE);
       sOperatableFlag |= (QCM_OPERATABLE_QP_SELECT_ENABLE);
       sOperatableFlag |= (QCM_OPERATABLE_QP_COMMENT_ENABLE);
       sOperatableFlag |= (QCM_OPERATABLE_QP_PURGE_DISABLE);
       sOperatableFlag |= (QCM_OPERATABLE_QP_FLASHBACK_DISABLE);
       sOperatableFlag |= (QCM_OPERATABLE_QP_FT_TRANS_DISABLE);
    }
    else if( (aTableType == QCM_FIXED_TABLE) ||
             (aTableType == QCM_DUMP_TABLE) || // BUG-16651
             (aTableType == QCM_PERFORMANCE_VIEW) )
    {
       sOperatableFlag |= (QCM_OPERATABLE_QP_REPL_DISABLE);
       sOperatableFlag |= (QCM_OPERATABLE_QP_LOCK_TABLE_DISABLE);
       sOperatableFlag |= (QCM_OPERATABLE_QP_CREATE_TABLE_DISABLE);
       sOperatableFlag |= (QCM_OPERATABLE_QP_DROP_TABLE_DISABLE);
       sOperatableFlag |= (QCM_OPERATABLE_QP_TRUNC_TABLE_DISABLE);
       sOperatableFlag |= (QCM_OPERATABLE_QP_ALTER_TABLE_DISABLE);
       sOperatableFlag |= (QCM_OPERATABLE_QP_CREATE_VIEW_DISABLE);
       sOperatableFlag |= (QCM_OPERATABLE_QP_ALTER_VIEW_DISABLE);
       sOperatableFlag |= (QCM_OPERATABLE_QP_DROP_VIEW_DISABLE);
       sOperatableFlag |= (QCM_OPERATABLE_QP_INSERT_DISABLE);
       sOperatableFlag |= (QCM_OPERATABLE_QP_UPDATE_DISABLE);
       sOperatableFlag |= (QCM_OPERATABLE_QP_DELETE_DISABLE);
       sOperatableFlag |= (QCM_OPERATABLE_QP_MOVE_DISABLE);
       sOperatableFlag |= (QCM_OPERATABLE_QP_SELECT_ENABLE);
       sOperatableFlag |= (QCM_OPERATABLE_QP_COMMENT_DISABLE);
       sOperatableFlag |= (QCM_OPERATABLE_QP_PURGE_DISABLE);
       sOperatableFlag |= (QCM_OPERATABLE_QP_FLASHBACK_DISABLE);
       sOperatableFlag |= (QCM_OPERATABLE_QP_FT_TRANS_DISABLE);
    }
    else if( aTableType == QCM_QUEUE_TABLE )
    {
       sOperatableFlag |= (QCM_OPERATABLE_QP_REPL_DISABLE);
       sOperatableFlag |= (QCM_OPERATABLE_QP_LOCK_TABLE_ENABLE);
       sOperatableFlag |= (QCM_OPERATABLE_QP_CREATE_TABLE_ENABLE);
       sOperatableFlag |= (QCM_OPERATABLE_QP_DROP_TABLE_ENABLE);
       sOperatableFlag |= (QCM_OPERATABLE_QP_TRUNC_TABLE_ENABLE);
       sOperatableFlag |= (QCM_OPERATABLE_QP_ALTER_TABLE_DISABLE);
       sOperatableFlag |= (QCM_OPERATABLE_QP_CREATE_VIEW_DISABLE);
       sOperatableFlag |= (QCM_OPERATABLE_QP_ALTER_VIEW_DISABLE);
       sOperatableFlag |= (QCM_OPERATABLE_QP_DROP_VIEW_DISABLE);
       sOperatableFlag |= (QCM_OPERATABLE_QP_INSERT_ENABLE);
       sOperatableFlag |= (QCM_OPERATABLE_QP_UPDATE_DISABLE);
       sOperatableFlag |= (QCM_OPERATABLE_QP_DELETE_ENABLE);
       sOperatableFlag |= (QCM_OPERATABLE_QP_MOVE_DISABLE);
       sOperatableFlag |= (QCM_OPERATABLE_QP_SELECT_ENABLE);
       sOperatableFlag |= (QCM_OPERATABLE_QP_COMMENT_DISABLE);
       sOperatableFlag |= (QCM_OPERATABLE_QP_PURGE_DISABLE);
       sOperatableFlag |= (QCM_OPERATABLE_QP_FLASHBACK_DISABLE);
       sOperatableFlag |= (QCM_OPERATABLE_QP_FT_TRANS_DISABLE);
    }
    else if( aTableType == QCM_MVIEW_TABLE )
    {
       sOperatableFlag |= (QCM_OPERATABLE_QP_REPL_DISABLE);
       sOperatableFlag |= (QCM_OPERATABLE_QP_LOCK_TABLE_ENABLE);
       sOperatableFlag |= (QCM_OPERATABLE_QP_CREATE_TABLE_ENABLE);
       sOperatableFlag |= (QCM_OPERATABLE_QP_DROP_TABLE_DISABLE);
       sOperatableFlag |= (QCM_OPERATABLE_QP_TRUNC_TABLE_ENABLE);
       sOperatableFlag |= (QCM_OPERATABLE_QP_ALTER_TABLE_ENABLE);
       sOperatableFlag |= (QCM_OPERATABLE_QP_CREATE_VIEW_DISABLE);
       sOperatableFlag |= (QCM_OPERATABLE_QP_ALTER_VIEW_DISABLE);
       sOperatableFlag |= (QCM_OPERATABLE_QP_DROP_VIEW_DISABLE);
       sOperatableFlag |= (QCM_OPERATABLE_QP_INSERT_ENABLE);
       sOperatableFlag |= (QCM_OPERATABLE_QP_UPDATE_DISABLE);
       sOperatableFlag |= (QCM_OPERATABLE_QP_DELETE_ENABLE);
       sOperatableFlag |= (QCM_OPERATABLE_QP_MOVE_DISABLE);
       sOperatableFlag |= (QCM_OPERATABLE_QP_SELECT_ENABLE);
       sOperatableFlag |= (QCM_OPERATABLE_QP_COMMENT_DISABLE);
       sOperatableFlag |= (QCM_OPERATABLE_QP_PURGE_DISABLE);
       sOperatableFlag |= (QCM_OPERATABLE_QP_FLASHBACK_DISABLE);
       sOperatableFlag |= (QCM_OPERATABLE_QP_FT_TRANS_DISABLE);
    }
    else if( aTableType == QCM_MVIEW_VIEW )
    {
       sOperatableFlag |= (QCM_OPERATABLE_QP_REPL_DISABLE);
       sOperatableFlag |= (QCM_OPERATABLE_QP_LOCK_TABLE_ENABLE);
       sOperatableFlag |= (QCM_OPERATABLE_QP_CREATE_TABLE_DISABLE);
       sOperatableFlag |= (QCM_OPERATABLE_QP_DROP_TABLE_DISABLE);
       sOperatableFlag |= (QCM_OPERATABLE_QP_TRUNC_TABLE_DISABLE);
       sOperatableFlag |= (QCM_OPERATABLE_QP_ALTER_TABLE_DISABLE);
       sOperatableFlag |= (QCM_OPERATABLE_QP_CREATE_VIEW_ENABLE);
       sOperatableFlag |= (QCM_OPERATABLE_QP_ALTER_VIEW_ENABLE);
       sOperatableFlag |= (QCM_OPERATABLE_QP_DROP_VIEW_DISABLE);
       sOperatableFlag |= (QCM_OPERATABLE_QP_INSERT_DISABLE);
       sOperatableFlag |= (QCM_OPERATABLE_QP_UPDATE_DISABLE);
       sOperatableFlag |= (QCM_OPERATABLE_QP_DELETE_DISABLE);
       sOperatableFlag |= (QCM_OPERATABLE_QP_MOVE_DISABLE);
       sOperatableFlag |= (QCM_OPERATABLE_QP_SELECT_ENABLE);
       sOperatableFlag |= (QCM_OPERATABLE_QP_COMMENT_DISABLE);
       sOperatableFlag |= (QCM_OPERATABLE_QP_PURGE_DISABLE);
       sOperatableFlag |= (QCM_OPERATABLE_QP_FLASHBACK_DISABLE);
       sOperatableFlag |= (QCM_OPERATABLE_QP_FT_TRANS_DISABLE);
    }
    else if( aTableType == QCM_INDEX_TABLE )
    {
        // PROJ-1624 non-partitioned index
        // hidden table(index table)���� ��� DDL, DML�� �����Ѵ�.
       sOperatableFlag |= (QCM_OPERATABLE_QP_REPL_DISABLE);
       sOperatableFlag |= (QCM_OPERATABLE_QP_LOCK_TABLE_DISABLE);
       sOperatableFlag |= (QCM_OPERATABLE_QP_CREATE_TABLE_DISABLE);
       sOperatableFlag |= (QCM_OPERATABLE_QP_DROP_TABLE_DISABLE);
       sOperatableFlag |= (QCM_OPERATABLE_QP_TRUNC_TABLE_DISABLE);
       sOperatableFlag |= (QCM_OPERATABLE_QP_ALTER_TABLE_DISABLE);
       sOperatableFlag |= (QCM_OPERATABLE_QP_CREATE_VIEW_DISABLE);
       sOperatableFlag |= (QCM_OPERATABLE_QP_ALTER_VIEW_DISABLE);
       sOperatableFlag |= (QCM_OPERATABLE_QP_DROP_VIEW_DISABLE);
       sOperatableFlag |= (QCM_OPERATABLE_QP_INSERT_DISABLE);
       sOperatableFlag |= (QCM_OPERATABLE_QP_UPDATE_DISABLE);
       sOperatableFlag |= (QCM_OPERATABLE_QP_DELETE_DISABLE);
       sOperatableFlag |= (QCM_OPERATABLE_QP_MOVE_DISABLE);
       sOperatableFlag |= (QCM_OPERATABLE_QP_SELECT_ENABLE);
       sOperatableFlag |= (QCM_OPERATABLE_QP_COMMENT_DISABLE);
       sOperatableFlag |= (QCM_OPERATABLE_QP_PURGE_DISABLE);
       sOperatableFlag |= (QCM_OPERATABLE_QP_FLASHBACK_DISABLE);
       sOperatableFlag |= (QCM_OPERATABLE_QP_FT_TRANS_DISABLE);
    }
    else if( aTableType == QCM_SEQUENCE_TABLE )
    {
        // PROJ-2365 sequence table
        // sequence table���� select�� replication�� ������ ��� DDL, DML�� �����Ѵ�.
        // test�� ���޻�Ȳ�� ����� update�� �߰� ����Ѵ�.
       sOperatableFlag |= (QCM_OPERATABLE_QP_REPL_ENABLE);
       sOperatableFlag |= (QCM_OPERATABLE_QP_LOCK_TABLE_DISABLE);
       sOperatableFlag |= (QCM_OPERATABLE_QP_CREATE_TABLE_DISABLE);
       sOperatableFlag |= (QCM_OPERATABLE_QP_DROP_TABLE_DISABLE);
       sOperatableFlag |= (QCM_OPERATABLE_QP_TRUNC_TABLE_DISABLE);
       sOperatableFlag |= (QCM_OPERATABLE_QP_ALTER_TABLE_DISABLE);
       sOperatableFlag |= (QCM_OPERATABLE_QP_CREATE_VIEW_DISABLE);
       sOperatableFlag |= (QCM_OPERATABLE_QP_ALTER_VIEW_DISABLE);
       sOperatableFlag |= (QCM_OPERATABLE_QP_DROP_VIEW_DISABLE);
       sOperatableFlag |= (QCM_OPERATABLE_QP_INSERT_DISABLE);
       sOperatableFlag |= (QCM_OPERATABLE_QP_UPDATE_ENABLE);
       sOperatableFlag |= (QCM_OPERATABLE_QP_DELETE_DISABLE);
       sOperatableFlag |= (QCM_OPERATABLE_QP_MOVE_DISABLE);
       sOperatableFlag |= (QCM_OPERATABLE_QP_SELECT_ENABLE);
       sOperatableFlag |= (QCM_OPERATABLE_QP_COMMENT_DISABLE);
       sOperatableFlag |= (QCM_OPERATABLE_QP_PURGE_DISABLE);
       sOperatableFlag |= (QCM_OPERATABLE_QP_FLASHBACK_DISABLE);
       sOperatableFlag |= (QCM_OPERATABLE_QP_FT_TRANS_DISABLE);
    }
    /* PROJ-2441 flashback */
    else if( aTableType == QCM_RECYCLEBIN_TABLE )
    {
        /*
         SELECT, PURGE, FLASHBACK �� ���
        */
       sOperatableFlag |= (QCM_OPERATABLE_QP_REPL_DISABLE);
       sOperatableFlag |= (QCM_OPERATABLE_QP_LOCK_TABLE_DISABLE);
       sOperatableFlag |= (QCM_OPERATABLE_QP_CREATE_TABLE_DISABLE);
       sOperatableFlag |= (QCM_OPERATABLE_QP_DROP_TABLE_DISABLE);
       sOperatableFlag |= (QCM_OPERATABLE_QP_TRUNC_TABLE_DISABLE);
       sOperatableFlag |= (QCM_OPERATABLE_QP_ALTER_TABLE_DISABLE);
       sOperatableFlag |= (QCM_OPERATABLE_QP_CREATE_VIEW_DISABLE);
       sOperatableFlag |= (QCM_OPERATABLE_QP_ALTER_VIEW_DISABLE);
       sOperatableFlag |= (QCM_OPERATABLE_QP_DROP_VIEW_DISABLE);
       sOperatableFlag |= (QCM_OPERATABLE_QP_INSERT_DISABLE);
       sOperatableFlag |= (QCM_OPERATABLE_QP_UPDATE_DISABLE);
       sOperatableFlag |= (QCM_OPERATABLE_QP_DELETE_DISABLE);
       sOperatableFlag |= (QCM_OPERATABLE_QP_MOVE_DISABLE);
       sOperatableFlag |= (QCM_OPERATABLE_QP_SELECT_ENABLE);
       sOperatableFlag |= (QCM_OPERATABLE_QP_COMMENT_DISABLE);
       sOperatableFlag |= (QCM_OPERATABLE_QP_PURGE_ENABLE);
       sOperatableFlag |= (QCM_OPERATABLE_QP_FLASHBACK_ENABLE);
       sOperatableFlag |= (QCM_OPERATABLE_QP_FT_TRANS_DISABLE);
    }
    else if( aTableType == QCM_DICTIONARY_TABLE )
    {
        // BUG-45366
       sOperatableFlag |= (QCM_OPERATABLE_QP_REPL_DISABLE);
       sOperatableFlag |= (QCM_OPERATABLE_QP_LOCK_TABLE_DISABLE);
       sOperatableFlag |= (QCM_OPERATABLE_QP_CREATE_TABLE_DISABLE);
       sOperatableFlag |= (QCM_OPERATABLE_QP_DROP_TABLE_DISABLE);
       sOperatableFlag |= (QCM_OPERATABLE_QP_TRUNC_TABLE_DISABLE);
       sOperatableFlag |= (QCM_OPERATABLE_QP_ALTER_TABLE_DISABLE);
       sOperatableFlag |= (QCM_OPERATABLE_QP_CREATE_VIEW_DISABLE);
       sOperatableFlag |= (QCM_OPERATABLE_QP_ALTER_VIEW_DISABLE);
       sOperatableFlag |= (QCM_OPERATABLE_QP_DROP_VIEW_DISABLE);
       sOperatableFlag |= (QCM_OPERATABLE_QP_INSERT_DISABLE);
       sOperatableFlag |= (QCM_OPERATABLE_QP_UPDATE_DISABLE);
       sOperatableFlag |= (QCM_OPERATABLE_QP_DELETE_DISABLE);
       sOperatableFlag |= (QCM_OPERATABLE_QP_MOVE_DISABLE);
       sOperatableFlag |= (QCM_OPERATABLE_QP_SELECT_ENABLE);
       sOperatableFlag |= (QCM_OPERATABLE_QP_COMMENT_DISABLE);
       sOperatableFlag |= (QCM_OPERATABLE_QP_PURGE_DISABLE);
       sOperatableFlag |= (QCM_OPERATABLE_QP_FLASHBACK_DISABLE);
       sOperatableFlag |= (QCM_OPERATABLE_QP_FT_TRANS_DISABLE);
    }
    else
    {
        // BUG-16980
        // QCM_SEQUENCE�� QCM_QUEUE_SEQUENCE������ �ȵȴ�.
        IDE_DASSERT( 0 );
    }

    *aOperatableFlag = sOperatableFlag;
}

IDE_RC qcm::existObject(
    qcStatement    * aStatement,
    idBool           aIsPublicObject,
    qcNamePosition   aUserName,
    qcNamePosition   aObjectName,
    qsObjectType     aObjectType,        // BUG-37293
    UInt           * aUserID,
    qsOID          * aObjectID,
    idBool         * aExist )
{
/***********************************************************************
 *
 * Description :
 *
 * Implementation :
 *
 ***********************************************************************/

    qcmSynonymInfo    sSynonymInfo;
    void            * sTableHandle;
    smSCN             sSCN;
    void            * sSequenceHandle;
    qcmLibraryInfo    sLibraryInfo;               // PROJ-1685
    qsOID             sObjectID = QS_EMPTY_OID;   // BUG-37293

    *aObjectID = QS_EMPTY_OID;
    *aExist    = ID_FALSE;

    /*
     * Synonym ��Ű�� ��ü���� PUBLIC�̶�� ������ �����Ѵ�.
     * PUBLIC ��ü�� Ư�� ������ ���� ��ü�� �ƴϰ�, ��� ����ڰ� ����� ��
     * �ִ� ��ü�̴�.
     */

    // Public Object
    if(ID_TRUE == aIsPublicObject)
    {
        *aUserID = QC_PUBLIC_USER_ID;
    }
    // Private Object
    else
    {
        if(QC_IS_NULL_NAME(aUserName) == ID_TRUE)
        {
            // UserName�� �������� ������ Session User�� ��ü�� �����ڰ� ��
            *aUserID = QCG_GET_SESSION_USER_ID(aStatement);
        }
        else
        {
            // UserName���� UserID�� ã��
            IDE_TEST(qcmUser::getUserID( aStatement, aUserName, aUserID )
                     != IDE_SUCCESS);
        }
    }

    // ��ü �̸��� NULL�� ���
    if (QC_IS_NULL_NAME(aObjectName) == ID_TRUE)
    {
        IDE_RAISE(OBJECT_NOT_EXIST);
    }

    /* BUG-39066 */
    if ( (gQcmTables == NULL) ||
         (gQcmProcedures == NULL) ||
         (gQcmPkgs == NULL) ||
         (gQcmLibraries == NULL) ||
         (gQcmSynonyms == NULL) )
    {
        IDE_RAISE(OBJECT_NOT_EXIST);
    }
    else
    {
        // Nothing to do.
    }

    // table, sequence, procedure�� ����
    // public object�� �������� �ʴ´�.
    if(ID_TRUE != aIsPublicObject)
    {
        // get table
        if(qcm::getTableHandleByName(QC_SMI_STMT(aStatement),
                                     *aUserID,
                                     (UChar*) (aObjectName.stmtText +
                                               aObjectName.offset),
                                     aObjectName.size,
                                     &sTableHandle,
                                     &sSCN)
           == IDE_SUCCESS)
        {
            IDE_RAISE(OBJECT_EXIST);
        }
        else
        {
            /* Nothing to do. */
        }

        // get sequence
        if(qcm::getSequenceHandleByName(QC_SMI_STMT(aStatement),
                                        *aUserID,
                                        (UChar*) (aObjectName.stmtText +
                                                  aObjectName.offset),
                                        aObjectName.size,
                                        &sSequenceHandle)
           == IDE_SUCCESS)
        {
            IDE_RAISE(OBJECT_EXIST);
        }
        else
        {
            /* Nothing to do. */
        }

        // get procedure
        if(qcmProc::getProcExistWithEmptyByNamePtr(aStatement,
                                                   *aUserID,
                                                   (SChar*) (aObjectName.stmtText +
                                                             aObjectName.offset),
                                                   aObjectName.size,
                                                   &sObjectID)
           == IDE_SUCCESS)
        {
            if(QS_EMPTY_OID != sObjectID)
            {
                // BUG-37293
                // psm ��ü �Ǵ� package ���� ��,
                // psm ��ü���� package���� �����Ͽ�
                // ������ �̸��� ���� ��ü�� ���� ���θ� �Ǵ��ؾ� �Ѵ�.
                if( (aObjectType == QS_PROC) ||
                    (aObjectType == QS_FUNC) ||
                    (aObjectType == QS_TYPESET) )
                {
                    *aObjectID = sObjectID;
                }
                else
                {
                    // Nothing to do.
                    // *aObjectID�� QS_EMPTY_OID��
                }

                IDE_RAISE(OBJECT_EXIST);
            }
        }

        // PROJ-1685
        // get library
        if( qcmLibrary::getLibrary( aStatement,
                                    *aUserID,
                                    (SChar*)(aObjectName.stmtText +
                                                          aObjectName.offset),
                                    aObjectName.size,
                                    &sLibraryInfo,
                                    aExist ) == IDE_SUCCESS )
        {
            if( ID_TRUE == *aExist )
            {
                IDE_RAISE( OBJECT_EXIST );
            }
        }

        // PROJ-1073 Package
        // get package
        if(qcmPkg::getPkgExistWithEmptyByNamePtr( aStatement,
                                                  *aUserID,
                                                  (SChar*)(aObjectName.stmtText +
                                                           aObjectName.offset),
                                                  aObjectName.size,
                                                  QS_PKG,
                                                  &sObjectID )
           == IDE_SUCCESS)
        {
            if(QS_EMPTY_OID != sObjectID)
            {
                // BUG-37293
                // psm ��ü �Ǵ� package ���� ��,
                // psm ��ü���� package���� �����Ͽ�
                // ������ �̸��� ���� ��ü�� ���� ���θ� �Ǵ��ؾ� �Ѵ�.
                if( aObjectType == QS_PKG )

                {
                    *aObjectID = sObjectID;
                }
                else
                {
                    // Nothing to do.
                    // *aObjectID�� QS_EMPTY_OID
                }

                IDE_RAISE(OBJECT_EXIST);
            }
        }
    }

    // get synonym
    if(qcmSynonym::getSynonym(aStatement,
                              *aUserID,
                              aObjectName.stmtText + aObjectName.offset,
                              aObjectName.size,
                              &sSynonymInfo,
                              aExist)
       == IDE_SUCCESS)
    {
        if(ID_TRUE == *aExist)
        {
            IDE_RAISE(OBJECT_EXIST);
        }
    }

    /*
     * ������� �Դٸ� ��ü�� ã�� ���� ���̴�.
     */


// ��ü�� ã�� ���ϸ� ����� �̵��Ѵ�.
OBJECT_NOT_EXIST :
    *aExist = ID_FALSE;

    return IDE_SUCCESS;

// ��ü�� ã���� ����� �̵��Ѵ�.
OBJECT_EXIST :
    *aExist = ID_TRUE;

    return IDE_SUCCESS;


    IDE_EXCEPTION_END;

    *aExist = ID_FALSE;

    return IDE_FAILURE;

}

IDE_RC qcm::getTableInfo( qcStatement     *aStatement,
                          UInt             aUserID,
                          qcNamePosition   aTableName,
                          qcmTableInfo   **aTableInfo,
                          smSCN           *aSCN,
                          void           **aTableHandle)
{
/***********************************************************************
 *
 * Description :
 *    ���̺� �̸����� ĳ���� ��Ÿ���� qcmTableInfo ����ü�� ���Ѵ�
 *
 * Implementation :
 *    1. ���̺� �̸����� ���̺��� �ڵ��� ���Ѵ�.
 *    2. 1�� �ڵ�� ���̺� info �� ���Ѵ� => smiGetTableTempInfo
 *
 ***********************************************************************/

    if ( aTableName.size > 0 )
    {
        IDE_TEST(getTableInfo( aStatement,
                               aUserID,
                               (UChar*) (aTableName.stmtText +
                                         aTableName.offset),
                               aTableName.size,
                               aTableInfo,
                               aSCN,
                               aTableHandle)
                 != IDE_SUCCESS );
    }
    else
    {
        // To Fix UMR
        // In-line VIEW�� ��� Table Name�� �������� �ʾ�
        // UMR�� �߻��ϰ� �ȴ�.
        IDE_RAISE( ERR_NOT_EXIST_TABLE );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_NOT_EXIST_TABLE);
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QCM_NOT_EXIST_TABLE));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qcm::getTableInfo(
    qcStatement     *aStatement,
    UInt             aUserID,
    UChar           *aTableName,
    SInt             aTableNameSize,
    qcmTableInfo   **aTableInfo,
    smSCN           *aSCN,
    void           **aTableHandle)
{
/***********************************************************************
 *
 * Description :
 *    ���̺� �̸����� ĳ���� ��Ÿ���� qcmTableInfo ����ü�� ���Ѵ�
 *
 * Implementation :
 *    1. ���̺� �̸����� ���̺��� �ڵ��� ���Ѵ�.
 *    2. 1�� �ڵ�� ���̺� info �� ���Ѵ� => smiGetTableTempInfo
 *
 ***********************************************************************/

    UInt    sFlag;

    IDE_TEST( getTableHandleByName( QC_SMI_STMT( aStatement ),
                                    aUserID,
                                    aTableName,
                                    aTableNameSize,
                                    aTableHandle,
                                    aSCN )
              != IDE_SUCCESS );

    IDE_TEST(  smiGetTableTempInfo( *aTableHandle,
                                    (void**)aTableInfo )
               != IDE_SUCCESS );

    if ( QC_SHARED_TMPLATE(aStatement) != NULL )
    {
        sFlag = smiGetTableFlag(*aTableHandle);

        // To fix BUG-14826
        // tableinfo�� ���� lock�� ���� �Ⱦ����Ƿ� ��ȿ���� �ʴ�.
        // tablehandle�� ���ؾ� ��.
        if( ( (sFlag & SMI_TABLE_TYPE_MASK) == SMI_TABLE_MEMORY) ||
            ( (sFlag & SMI_TABLE_TYPE_MASK) == SMI_TABLE_META) ||
            ( (sFlag & SMI_TABLE_TYPE_MASK) == SMI_TABLE_VOLATILE) )
        {
            QC_SHARED_TMPLATE(aStatement)->smiStatementFlag |= SMI_STATEMENT_MEMORY_CURSOR;
        }
        else
        {
            QC_SHARED_TMPLATE(aStatement)->smiStatementFlag |= SMI_STATEMENT_DISK_CURSOR;
        }
    }
    else
    {
        // Nothing to do.
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qcm::getTableInfo(qcStatement     *aStatement,
                         SChar           *aUserName,
                         SChar           *aTableName,
                         qcmTableInfo   **aOutTableInfo,
                         smSCN           *aOutSCN,
                         void           **aOutTableHandle)
{
    UInt   sUserID = 0;

    IDE_TEST( qcmUser::getUserID( aStatement,
                                  aUserName,
                                  idlOS::strlen(aUserName),
                                  & sUserID ) != IDE_SUCCESS );

    IDE_TEST( qcm::getTableInfo( aStatement,
                                 sUserID,
                                 (UChar *)aTableName,
                                 idlOS::strlen(aTableName),
                                 aOutTableInfo,
                                 aOutSCN,
                                 aOutTableHandle ) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qcm::getTableInfoAndLock(qcStatement     *aStatement,
                                SChar           *aUserName,
                                SChar           *aTableName,
                                smiTableLockMode aLockMode,
                                ULong            aTimeout,
                                qcmTableInfo   **aOutTableInfo,
                                void           **aOutTableHandle)
{
/***********************************************************************
 *
 * Description :
 *    ����� �̸��� ���̺� �̸����� ĳ���� ��Ÿ���� qcmTableInfo ����ü�� ���Ѵ�
 *    ����ڰ� ��û�� lock�� ȹ��� ���·� table info�� ��ȯ�ϹǷ� �ٷ� ��� �����ϴ�.
 *
 * Implementation :
 *    1. ���̺� �̸����� ���̺��� �ڵ��� ���Ѵ�.
 *    2. ���̺� �ڵ��� �̿��ؼ� lock�� ��´�.
 *    3. 1�� �ڵ�� ���̺� info �� ���Ѵ� => smiGetTableTempInfo
 *
 ***********************************************************************/
    UInt   sUserID = 0;
    smSCN  sTableSCN;
    void * sTableHandle = NULL;
    qcmTableInfo * sTableInfo = NULL;

    IDE_TEST( qcmUser::getUserID( NULL,
                                  QC_SMI_STMT(aStatement),
                                  aUserName,
                                  idlOS::strlen(aUserName),
                                  & sUserID ) != IDE_SUCCESS );

    IDE_TEST( qcm::getTableHandleByName( QC_SMI_STMT(aStatement),
                                         sUserID,
                                         (UChar *)aTableName,
                                         idlOS::strlen(aTableName),
                                         (void**)&sTableHandle,
                                         &sTableSCN ) != IDE_SUCCESS );

    IDE_TEST(smiValidateAndLockObjects( QC_SMI_STMT(aStatement)->getTrans(),
                                        sTableHandle,
                                        sTableSCN,
                                        SMI_TBSLV_DDL_DML,
                                        aLockMode,
                                        aTimeout,
                                        ID_FALSE)
             != IDE_SUCCESS);

    IDE_TEST( smiGetTableTempInfo( sTableHandle,
                                   (void**)&sTableInfo )
              != IDE_SUCCESS );

    *aOutTableHandle = sTableHandle;
    *aOutTableInfo = sTableInfo;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qcm::getTableHandleByName( smiStatement     * aSmiStmt,
                                  UInt               aUserID,
                                  UChar            * aTableName,
                                  SInt               aTableNameSize,
                                  void            ** aTableHandle,
                                  smSCN            * aSCN )
{
/***********************************************************************
 *
 * Description :
 *    userID, ���̺� �̸����� ���̺� �ڵ��� ���Ѵ�.
 *
 * Implementation :
 *    1. SYS_TABLES_ ���̺��� TABLE_NAME, TABLE_OID �÷��� ���Ѵ�.
 *    2. SYS_TABLES_ ���̺� �ε����� ������, ��õ� userID, tableName ����
 *       keyRange �� �����
 *    3. table OID �� ���� ����, ( 2���� �ε����� ������ read �ϸ鼭 ��õ�
 *       userID, tableName �� ���ؼ� ã�´� ) smiGetTable �� ���ؼ�
 *       ���̺� �ڵ��� ���Ѵ�. ���ǿ� �´� �Ǽ��� ������ ������ ��ȯ�Ѵ�.
 *    4. TABLE_TYPE �� 'S'(������) Ȥ�� 'W'(ť������)�̸� ��ã�Ҵ�.
 *
 ***********************************************************************/

    UInt                  sStage = 0;
    smiRange              sRange;
    smiTableCursor        sCursor;
    const void          * sRow                      = NULL;
    mtcColumn           * sQcmTablesUserIDColumn    = NULL;
    mtcColumn           * sQcmTablesTableNameColumn = NULL;
    mtcColumn           * sQcmTablesTableOIDColumn  = NULL;
    mtcColumn           * sQcmTablesTableTypeColumn = NULL;
    UInt                  sUserID;
    smOID                 sTableOID;
    ULong                 sTableOIDULong;
    mtdCharType         * sTableName                = NULL;
    idBool                sFoundTable               = ID_FALSE;
    qtcMetaRangeColumn    sFirstRangeColumn;
    qtcMetaRangeColumn    sSecondRangeColumn;

    qcNameCharBuffer      sTableValueBuffer;
    mtdCharType         * sTableValue = ( mtdCharType * ) & sTableValueBuffer;

    UChar               * sTableType                = NULL;
    mtdCharType         * sTableTypeStr             = NULL;

    scGRID                sRid; // Disk Table�� ���� Record IDentifier
    smiCursorProperties   sCursorProperty;

    IDE_TEST_RAISE( aTableNameSize > QC_MAX_OBJECT_NAME_LEN,
                    ERR_NOT_EXIST_TABLE );
        
    sCursor.initialize();

    // USER_ID
    IDE_TEST( smiGetTableColumns( gQcmTables,
                                  QCM_TABLES_USER_ID_COL_ORDER,
                                  (const smiColumn**)&sQcmTablesUserIDColumn )
              != IDE_SUCCESS );

    // TABLE_NAME
    IDE_TEST( smiGetTableColumns( gQcmTables,
                                  QCM_TABLES_TABLE_NAME_COL_ORDER,
                                  (const smiColumn**)&sQcmTablesTableNameColumn )
              != IDE_SUCCESS );

    // TABLE_OID
    IDE_TEST( smiGetTableColumns( gQcmTables,
                                  QCM_TABLES_TABLE_OID_COL_ORDER,
                                  (const smiColumn**)&sQcmTablesTableOIDColumn )
              != IDE_SUCCESS );

    // TABLE_TYPE
    IDE_TEST( smiGetTableColumns( gQcmTables,
                                  QCM_TABLES_TABLE_TYPE_COL_ORDER,
                                  (const smiColumn**)&sQcmTablesTableTypeColumn )
              != IDE_SUCCESS );

    if (gQcmTablesIndex[QCM_TABLES_TABLENAME_USERID_IDX_ORDER]  == NULL)
    {
        SMI_CURSOR_PROP_INIT_FOR_META_FULL_SCAN(&sCursorProperty, NULL);

        // this situtation occurs only from createdb.
        // sequencial access to metatable.
        IDE_TEST(sCursor.open(
                     aSmiStmt,
                     gQcmTables,
                     NULL,
                     smiGetRowSCN(gQcmTables),
                     NULL,
                     smiGetDefaultKeyRange(),
                     smiGetDefaultKeyRange(),
                     smiGetDefaultFilter(),
                     QCM_META_CURSOR_FLAG,
                     SMI_SELECT_CURSOR,
                     &sCursorProperty) != IDE_SUCCESS);
        sStage = 1;

        IDE_TEST(sCursor.beforeFirst() != IDE_SUCCESS);

        IDE_TEST(sCursor.readRow(&sRow, &sRid, SMI_FIND_NEXT) != IDE_SUCCESS);

        while( (sRow != NULL) && (sFoundTable != ID_TRUE) )
        {
            sUserID = *(UInt *)((UChar *) sRow +
                                sQcmTablesUserIDColumn->column.offset);

            sTableName = (mtdCharType*)((UChar *) sRow +
                                        sQcmTablesTableNameColumn->column.offset);

            // BUG-37032
            if( ( sUserID == aUserID ) &&
                ( idlOS::strMatch( (SChar*)&(sTableName->value),
                                   sTableName->length,
                                   (SChar*)aTableName,
                                   aTableNameSize ) == 0 ) )
            {
                sFoundTable = ID_TRUE;
                break;
            }
            IDE_TEST(sCursor.readRow(&sRow, &sRid, SMI_FIND_NEXT) != IDE_SUCCESS);
        }
        IDE_TEST_RAISE(sRow == NULL, ERR_NOT_EXIST_TABLE);
    }
    else
    {
        // mtdModule ����
        IDE_TEST(mtd::moduleById( &(sQcmTablesUserIDColumn->module),
                                  sQcmTablesUserIDColumn->type.dataTypeId )
                 != IDE_SUCCESS);

        // mtdModule ����
        IDE_TEST(mtd::moduleById( &(sQcmTablesTableNameColumn->module),
                                  sQcmTablesTableNameColumn->type.dataTypeId )
                 != IDE_SUCCESS);

        qtc::setVarcharValue( sTableValue,
                              NULL,
                              (SChar*)aTableName,
                              aTableNameSize );

        makeMetaRangeDoubleColumn(
            &sFirstRangeColumn,
            &sSecondRangeColumn,
            (const mtcColumn *) sQcmTablesTableNameColumn,
            (const void *) sTableValue,
            (const mtcColumn *) sQcmTablesUserIDColumn,
            (const void *) &aUserID,
            &sRange);

        SMI_CURSOR_PROP_INIT_FOR_META_INDEX_SCAN(&sCursorProperty, NULL);

        IDE_TEST(sCursor.open(
                     aSmiStmt,
                     gQcmTables,
                     gQcmTablesIndex[QCM_TABLES_TABLENAME_USERID_IDX_ORDER],
                     smiGetRowSCN(gQcmTables),
                     NULL,
                     &sRange,
                     smiGetDefaultKeyRange(),
                     smiGetDefaultFilter(),
                     QCM_META_CURSOR_FLAG,
                     SMI_SELECT_CURSOR,
                     &sCursorProperty) != IDE_SUCCESS);
        sStage = 1;

        IDE_TEST(sCursor.beforeFirst() != IDE_SUCCESS);

        IDE_TEST(sCursor.readRow(&sRow, &sRid, SMI_FIND_NEXT) != IDE_SUCCESS);

        IDE_TEST_RAISE(sRow == NULL, ERR_NOT_EXIST_TABLE);
    }

    sTableOIDULong = *(ULong *) ((UChar *) sRow +
                                 sQcmTablesTableOIDColumn->column.offset);
    sTableOID = (smOID) sTableOIDULong;

    *aTableHandle = (void*)smiGetTable( sTableOID );
    *aSCN = smiGetRowSCN(*aTableHandle);

    sTableTypeStr = (mtdCharType*)
        ((UChar*) sRow + sQcmTablesTableTypeColumn->column.offset);
    sTableType = sTableTypeStr->value;

    // if we want to get the table is sequence or not
    /* BUG-35460 Add TABLE_TYPE G in SYS_TABLES_ */
    if ( ( idlOS::strMatch( (SChar*)sTableType, sTableTypeStr->length, "T", 1 ) == 0 ) ||
         ( idlOS::strMatch( (SChar*)sTableType, sTableTypeStr->length, "V", 1 ) == 0 ) ||
         ( idlOS::strMatch( (SChar*)sTableType, sTableTypeStr->length, "M", 1 ) == 0 ) ||
         ( idlOS::strMatch( (SChar*)sTableType, sTableTypeStr->length, "A", 1 ) == 0 ) ||
         ( idlOS::strMatch( (SChar*)sTableType, sTableTypeStr->length, "Q", 1 ) == 0 ) ||
         ( idlOS::strMatch( (SChar*)sTableType, sTableTypeStr->length, "G", 1 ) == 0 ) ||
         ( idlOS::strMatch( (SChar*)sTableType, sTableTypeStr->length, "D", 1 ) == 0 ) ||
         ( idlOS::strMatch( (SChar*)sTableType, sTableTypeStr->length, "E", 1 ) == 0 ) ||
         ( idlOS::strMatch( (SChar*)sTableType, sTableTypeStr->length, "R", 1 ) == 0 ) )    /* PROJ-2441 flashback */
    {
        // Nothing to do.
    }
    else
    {
        IDE_RAISE(ERR_NOT_EXIST_TABLE);
    }

    sStage = 0;
    IDE_TEST(sCursor.close() != IDE_SUCCESS);

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_NOT_EXIST_TABLE);
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QCM_NOT_EXIST_TABLE));
    }
    IDE_EXCEPTION_END;

    if ( sStage == 1 )
    {
        (void)sCursor.close();
    }

    return IDE_FAILURE;
}

IDE_RC qcm::getTableHandleByID( smiStatement  * aSmiStmt,
                                UInt            aTableID,
                                void         ** aTableHandle,
                                smSCN         * aSCN,
                                const void   ** aTableRow,
                                const idBool    aTouchTable)
{
/***********************************************************************
 *
 * Description :
 *    ���̺� ID �� ���̺� �ڵ��� ���Ѵ�.
 *
 * Implementation :
 *    1. SYS_TABLES_ ���̺��� TABLE_ID, TABLE_OID �÷��� ���Ѵ�.
 *    2. ��õ� aTableID �� keyRange �� �����
 *    3. table OID �� ���� ����, smiGetTable �� ���ؼ� ���̺� �ڵ��� ���Ѵ�.
 *       ���ǿ� �´� �Ǽ��� ������ ������ ��ȯ�Ѵ�.
 *    4. aTouchTable ���� ID_TRUE �̸� �ش� tuple �� updateRow(NULL) �Ѵ�.
 *    5. TableHandle, SCN, TableRow �� ��ȯ�Ѵ�.
 *
 ***********************************************************************/

    UInt                  sStage = 0;
    smiRange              sRange;
    smiTableCursor        sCursor;
    const void          * sRow;
    smOID                 sTableOID;
    ULong                 sTableOIDULong;
    UInt                  sFlag = QCM_META_CURSOR_FLAG;
    mtcColumn           * sQcmTablesIndexColumn;
    mtcColumn           * sTableOIDColInfo;
    mtcColumn           * sTableTypeColInfo;
    qtcMetaRangeColumn    sRangeColumn;
    idBool                sFoundTable   = ID_FALSE;

    UInt                  sTableID;
    UChar               * sTableType    = NULL;
    mtdCharType         * sTableTypeStr = NULL;

    scGRID                sRid; // Disk Table�� ���� Record IDentifier
    smiCursorType         sCursorType;
    smiCursorProperties   sCursorProperty;

    sCursor.initialize();

    IDE_TEST( smiGetTableColumns( gQcmTables,
                                  QCM_TABLES_TABLE_ID_COL_ORDER,
                                  (const smiColumn**)&sQcmTablesIndexColumn )
              != IDE_SUCCESS );

    IDE_TEST( smiGetTableColumns( gQcmTables,
                                  QCM_TABLES_TABLE_OID_COL_ORDER,
                                  (const smiColumn**)&sTableOIDColInfo )
              != IDE_SUCCESS );

    IDE_TEST( smiGetTableColumns( gQcmTables,
                                  QCM_TABLES_TABLE_TYPE_COL_ORDER,
                                  (const smiColumn**)&sTableTypeColInfo )
              != IDE_SUCCESS );

    // To fix BUG-17593
    // touchTable�� �� ���� update cursor type
    // touchTable�� �� �� ���� select cursor type
    if (aTouchTable == ID_TRUE)
    {
        sFlag = (SMI_LOCK_WRITE|SMI_TRAVERSE_FORWARD|SMI_PREVIOUS_DISABLE);
        sCursorType = SMI_UPDATE_CURSOR;
    }
    else
    {
        sCursorType = SMI_SELECT_CURSOR;
    }

    // BUG-17239
    if (gQcmTablesIndex[QCM_TABLES_TABLENAME_USERID_IDX_ORDER] == NULL)
    {
        SMI_CURSOR_PROP_INIT_FOR_META_FULL_SCAN(&sCursorProperty, NULL);

        // this situtation occurs only from createdb.
        // sequencial access to metatable.
        IDE_TEST(sCursor.open(
                     aSmiStmt,
                     gQcmTables,
                     NULL,
                     smiGetRowSCN(gQcmTables),
                     NULL,
                     smiGetDefaultKeyRange(),
                     smiGetDefaultKeyRange(),
                     smiGetDefaultFilter(),
                     sFlag,
                     sCursorType,
                     &sCursorProperty) != IDE_SUCCESS);
        sStage = 1;

        IDE_TEST(sCursor.beforeFirst() != IDE_SUCCESS);

        IDE_TEST(sCursor.readRow(&sRow, &sRid, SMI_FIND_NEXT) != IDE_SUCCESS);

        while ((sRow != NULL) && (sFoundTable != ID_TRUE))
        {
            sTableID = *(UInt *)((UChar *) sRow +
                                 sQcmTablesIndexColumn->column.offset);

            if (aTableID == sTableID)
            {
                sTableTypeStr = (mtdCharType*)
                    ((UChar*) sRow + sTableTypeColInfo->column.offset);
                sTableType = sTableTypeStr->value;

                // if we want to get the table is sequence or not
                /* BUG-35460 Add TABLE_TYPE G in SYS_TABLES_ */
                if ( ( idlOS::strMatch( (SChar*)sTableType, sTableTypeStr->length, "T", 1 ) == 0 ) ||
                     ( idlOS::strMatch( (SChar*)sTableType, sTableTypeStr->length, "V", 1 ) == 0 ) ||
                     ( idlOS::strMatch( (SChar*)sTableType, sTableTypeStr->length, "M", 1 ) == 0 ) ||
                     ( idlOS::strMatch( (SChar*)sTableType, sTableTypeStr->length, "A", 1 ) == 0 ) ||
                     ( idlOS::strMatch( (SChar*)sTableType, sTableTypeStr->length, "Q", 1 ) == 0 ) ||
                     ( idlOS::strMatch( (SChar*)sTableType, sTableTypeStr->length, "G", 1 ) == 0 ) ||
                     ( idlOS::strMatch( (SChar*)sTableType, sTableTypeStr->length, "D", 1 ) == 0 ) ||
                     ( idlOS::strMatch( (SChar*)sTableType, sTableTypeStr->length, "E", 1 ) == 0 ) ||
                     ( idlOS::strMatch( (SChar*)sTableType, sTableTypeStr->length, "R", 1 ) == 0 ) )    /* PROJ-2441 flashback */
                {
                    // Nothing to do.
                }
                else
                {
                    IDE_RAISE(ERR_NOT_EXIST_TABLE);
                }

                sFoundTable = ID_TRUE;
                break;
            }
            else
            {
                // Nothing to do.
            }

            IDE_TEST(sCursor.readRow(&sRow, &sRid, SMI_FIND_NEXT) != IDE_SUCCESS);
        }

        IDE_TEST_RAISE(sRow == NULL, ERR_NOT_EXIST_TABLE);
    }
    else
    {
        makeMetaRangeSingleColumn(
            &sRangeColumn,
            (const mtcColumn *) sQcmTablesIndexColumn,
            (const void *) &aTableID,
            &sRange);

        SMI_CURSOR_PROP_INIT_FOR_META_INDEX_SCAN(&sCursorProperty, NULL);

        IDE_TEST(sCursor.open(
                     aSmiStmt,
                     gQcmTables,
                     gQcmTablesIndex[QCM_TABLES_TABLEID_IDX_ORDER],
                     smiGetRowSCN(gQcmTables),
                     NULL,
                     &sRange,
                     smiGetDefaultKeyRange(),
                     smiGetDefaultFilter(),
                     sFlag,
                     sCursorType,
                     &sCursorProperty) != IDE_SUCCESS);
        sStage = 1;

        IDE_TEST(sCursor.beforeFirst() != IDE_SUCCESS);

        IDE_TEST(sCursor.readRow(&sRow, &sRid, SMI_FIND_NEXT) != IDE_SUCCESS);

        IDE_TEST_RAISE(sRow == NULL, ERR_NOT_EXIST_TABLE);

        sTableTypeStr = (mtdCharType*)
            ((UChar*) sRow + sTableTypeColInfo->column.offset);
        sTableType = sTableTypeStr->value;

        // if we want to get the table is sequence or not
        /* BUG-35460 Add TABLE_TYPE G in SYS_TABLES_ */
        /* PROJ-2441 flashback Add TABLE_TYPE R in SYS_TABLES_ */
        if ( ( idlOS::strMatch( (SChar*)sTableType, sTableTypeStr->length, "T", 1 ) == 0 ) ||
             ( idlOS::strMatch( (SChar*)sTableType, sTableTypeStr->length, "V", 1 ) == 0 ) ||
             ( idlOS::strMatch( (SChar*)sTableType, sTableTypeStr->length, "M", 1 ) == 0 ) ||
             ( idlOS::strMatch( (SChar*)sTableType, sTableTypeStr->length, "A", 1 ) == 0 ) ||
             ( idlOS::strMatch( (SChar*)sTableType, sTableTypeStr->length, "Q", 1 ) == 0 ) ||
             ( idlOS::strMatch( (SChar*)sTableType, sTableTypeStr->length, "G", 1 ) == 0 ) ||
             ( idlOS::strMatch( (SChar*)sTableType, sTableTypeStr->length, "D", 1 ) == 0 ) ||
             ( idlOS::strMatch( (SChar*)sTableType, sTableTypeStr->length, "E", 1 ) == 0 ) ||
             ( idlOS::strMatch( (SChar*)sTableType, sTableTypeStr->length, "R", 1 ) == 0 ) )    /* PROJ-2441 flashback */
        {
            // Nothing to do.
        }
        else
        {
            IDE_RAISE(ERR_NOT_EXIST_TABLE);
        }
    }

    if (aTouchTable == ID_TRUE)
    {
        IDE_TEST(sCursor.updateRow(NULL) != IDE_SUCCESS);
    }

    sStage = 0;
    IDE_TEST(sCursor.close() != IDE_SUCCESS);

    sTableOIDULong = *(ULong *)((UChar*) sRow +
                                sTableOIDColInfo->column.offset);
    sTableOID = (smOID)sTableOIDULong;

    *aTableHandle = (void*)smiGetTable( sTableOID );

    *aSCN = smiGetRowSCN(*aTableHandle);

    if (aTableRow != NULL)
    {
        *aTableRow = sRow;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_NOT_EXIST_TABLE);
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QCM_NOT_EXIST_TABLE));
    }
    IDE_EXCEPTION_END;

    if ( sStage == 1 )
    {
        (void)sCursor.close();
    }

    return IDE_FAILURE;
}

IDE_RC qcm::getTableInfoByID( qcStatement    *aStatement,
                              UInt            aTableID,
                              qcmTableInfo  **aTableInfo,
                              smSCN          *aSCN,
                              void          **aTableHandle )
{
    const void *sTableRow;

    IDE_TEST(getTableHandleByID(
                 QC_SMI_STMT( aStatement ),
                 aTableID,
                 aTableHandle,
                 aSCN,
                 &sTableRow) != IDE_SUCCESS);
    IDE_TEST( smiGetTableTempInfo( *aTableHandle,
                                   (void**)aTableInfo )
              != IDE_SUCCESS );

    if ( QC_SHARED_TMPLATE(aStatement) != NULL )
    {
        // BUG-16235
        if( ((smiGetTableFlag(*aTableHandle) & SMI_TABLE_TYPE_MASK) == SMI_TABLE_MEMORY) ||
            ((smiGetTableFlag(*aTableHandle) & SMI_TABLE_TYPE_MASK) == SMI_TABLE_META) ||
            ((smiGetTableFlag(*aTableHandle) & SMI_TABLE_TYPE_MASK) == SMI_TABLE_VOLATILE) )
        {
            // Memory Table Space�� ����ϴ� DDL�� ���
            // Cursor Flag�� ����
            QC_SHARED_TMPLATE(aStatement)->smiStatementFlag |= SMI_STATEMENT_MEMORY_CURSOR;
        }
        else
        {
            // Disk Table Space�� ����ϴ� DDL�� ���
            // Cursor Flag�� ����
            QC_SHARED_TMPLATE(aStatement)->smiStatementFlag |= SMI_STATEMENT_DISK_CURSOR;
        }
    }
    else
    {
        // Nothing to do.
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qcm::getSequenceHandleByName( smiStatement   * aSmiStmt,
                                     UInt             aUserID,
                                     UChar          * aSequenceName,
                                     SInt             aSequenceNameSize,
                                     void          ** aSequenceHandle )
{
/***********************************************************************
 *
 * Description :
 *    BUG-16980
 *    userID, ������ �̸����� ������ �ڵ��� ���Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qcm::getSequenceHandleByName"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY("qcm::getSequenceHandleByName"));

    return getSequenceInfoByName( aSmiStmt,
                                  aUserID,
                                  aSequenceName,
                                  aSequenceNameSize,
                                  NULL,
                                  aSequenceHandle );

#undef IDE_FN
}

IDE_RC qcm::getSequenceInfoByName( smiStatement     * aSmiStmt,
                                   UInt               aUserID,
                                   UChar            * aSequenceName,
                                   SInt               aSequenceNameSize,
                                   qcmSequenceInfo  * aSequenceInfo,
                                   void            ** aSequenceHandle )
{
/***********************************************************************
 *
 * Description :
 *    BUG-16980
 *    userID, ������ �̸����� ������ ����(handle,id) �� ���Ѵ�.
 *
 * Implementation :
 *    1. SYS_TABLES_ ���̺��� TABLE_NAME, TABLE_OID �÷��� ���Ѵ�.
 *    2. SYS_TABLES_ ���̺� �ε����� ������, ��õ� userID, tableName ����
 *       keyRange �� �����
 *    3. table OID �� ���� ����, ( 2���� �ε����� ������ read �ϸ鼭 ��õ�
 *       userID, tableName �� ���ؼ� ã�´� ) smiGetTable �� ���ؼ�
 *       ���̺� �ڵ��� ���Ѵ�. ���ǿ� �´� �Ǽ��� ������ ������ ��ȯ�Ѵ�.
 *    4. TABLE_TYPE �� 'S'(������) Ȥ�� 'W'(ť������)�̸� ã�Ҵ�.
 *
 ***********************************************************************/

    UInt                  sStage = 0;
    smiRange              sRange;
    smiTableCursor        sCursor;
    const void          * sRow                      = NULL;
    mtcColumn           * sQcmTablesUserIDColumn    = NULL;
    mtcColumn           * sQcmTablesTableIDColumn   = NULL;
    mtcColumn           * sQcmTablesTableNameColumn = NULL;
    mtcColumn           * sQcmTablesTableOIDColumn  = NULL;
    mtcColumn           * sQcmTablesTableTypeColumn = NULL;
    UInt                  sUserID;
    UInt                  sTableID;
    smOID                 sTableOID;
    ULong                 sTableOIDULong;
    mtdCharType         * sTableName                = NULL;
    idBool                sFoundTable               = ID_FALSE;
    qtcMetaRangeColumn    sFirstRangeColumn;
    qtcMetaRangeColumn    sSecondRangeColumn;

    qcSeqNameCharBuffer   sTableValueBuffer;
    mtdCharType         * sTableValue = ( mtdCharType * ) & sTableValueBuffer;

    UChar               * sTableType                = NULL;
    mtdCharType         * sTableTypeStr             = NULL;

    scGRID                sRid; // Disk Table�� ���� Record IDentifier
    smiCursorProperties   sCursorProperty;

    IDE_TEST_RAISE( aSequenceNameSize > QC_MAX_OBJECT_NAME_LEN,
                    ERR_NOT_EXIST_SEQUENCE );
    
    sCursor.initialize();

    // USER_ID
    IDE_TEST( smiGetTableColumns( gQcmTables,
                                  QCM_TABLES_USER_ID_COL_ORDER,
                                  (const smiColumn**)&sQcmTablesUserIDColumn )
              != IDE_SUCCESS );

    // TABLE_ID
    IDE_TEST( smiGetTableColumns( gQcmTables,
                                  QCM_TABLES_TABLE_ID_COL_ORDER,
                                  (const smiColumn**)&sQcmTablesTableIDColumn )
              != IDE_SUCCESS );

    // TABLE_NAME
    IDE_TEST( smiGetTableColumns( gQcmTables,
                                  QCM_TABLES_TABLE_NAME_COL_ORDER,
                                  (const smiColumn**)&sQcmTablesTableNameColumn )
              != IDE_SUCCESS );

    // TABLE_OID
    IDE_TEST( smiGetTableColumns( gQcmTables,
                                  QCM_TABLES_TABLE_OID_COL_ORDER,
                                  (const smiColumn**)&sQcmTablesTableOIDColumn )
              != IDE_SUCCESS );

    // TABLE_TYPE
    IDE_TEST( smiGetTableColumns( gQcmTables,
                                  QCM_TABLES_TABLE_TYPE_COL_ORDER,
                                  (const smiColumn**)&sQcmTablesTableTypeColumn )
              != IDE_SUCCESS );

    if (gQcmTablesIndex[QCM_TABLES_TABLENAME_USERID_IDX_ORDER]  == NULL)
    {
        // this situtation occurs only from createdb.
        // sequencial access to metatable.
        IDE_TEST(sCursor.open(
                     aSmiStmt,
                     gQcmTables,
                     NULL,
                     smiGetRowSCN(gQcmTables),
                     NULL,
                     smiGetDefaultKeyRange(),
                     smiGetDefaultKeyRange(),
                     smiGetDefaultFilter(),
                     QCM_META_CURSOR_FLAG,
                     SMI_SELECT_CURSOR,
                     &gMetaDefaultCursorProperty) != IDE_SUCCESS);
        sStage = 1;

        IDE_TEST(sCursor.beforeFirst() != IDE_SUCCESS);

        IDE_TEST(sCursor.readRow(&sRow, &sRid, SMI_FIND_NEXT) != IDE_SUCCESS);

        while (sRow != NULL && sFoundTable != ID_TRUE)
        {
            sUserID = *(UInt *)((UChar *) sRow +
                                sQcmTablesUserIDColumn->column.offset);

            sTableName = (mtdCharType*)((UChar *) sRow +
                                        sQcmTablesTableNameColumn->column.offset);

            // BUG-37032
            if( ( sUserID == aUserID ) &&
                ( idlOS::strMatch( (SChar*)&(sTableName->value),
                                   sTableName->length,
                                   (SChar*)aSequenceName,
                                   aSequenceNameSize ) == 0 ) )
            {
                sFoundTable = ID_TRUE;
                break;
            }
            IDE_TEST(sCursor.readRow(&sRow, &sRid, SMI_FIND_NEXT) != IDE_SUCCESS);
        }
        IDE_TEST_RAISE(sRow == NULL, ERR_NOT_EXIST_SEQUENCE);
    }
    else
    {
        // mtdModule ����
        IDE_TEST(mtd::moduleById( &(sQcmTablesUserIDColumn->module),
                                  sQcmTablesUserIDColumn->type.dataTypeId )
                 != IDE_SUCCESS);

        // mtdModule ����
        IDE_TEST(mtd::moduleById( &(sQcmTablesTableNameColumn->module),
                                  sQcmTablesTableNameColumn->type.dataTypeId )
                 != IDE_SUCCESS);

        qtc::setVarcharValue( sTableValue,
                              NULL,
                              (SChar*)aSequenceName,
                              aSequenceNameSize );

        makeMetaRangeDoubleColumn(
            &sFirstRangeColumn,
            &sSecondRangeColumn,
            (const mtcColumn *) sQcmTablesTableNameColumn,
            (const void *) sTableValue,
            (const mtcColumn *) sQcmTablesUserIDColumn,
            (const void *) &aUserID,
            &sRange);

        SMI_CURSOR_PROP_INIT_FOR_META_INDEX_SCAN(&sCursorProperty, NULL);

        IDE_TEST(sCursor.open(
                     aSmiStmt,
                     gQcmTables,
                     gQcmTablesIndex[QCM_TABLES_TABLENAME_USERID_IDX_ORDER],
                     smiGetRowSCN(gQcmTables),
                     NULL,
                     &sRange,
                     smiGetDefaultKeyRange(),
                     smiGetDefaultFilter(),
                     QCM_META_CURSOR_FLAG,
                     SMI_SELECT_CURSOR,
                     &sCursorProperty) != IDE_SUCCESS);
        sStage = 1;

        IDE_TEST(sCursor.beforeFirst() != IDE_SUCCESS);

        IDE_TEST(sCursor.readRow(&sRow, &sRid, SMI_FIND_NEXT) != IDE_SUCCESS);

        IDE_TEST_RAISE(sRow == NULL, ERR_NOT_EXIST_SEQUENCE);
    }

    sTableID = *(UInt *) ((UChar *) sRow +
                          sQcmTablesTableIDColumn->column.offset);

    sTableOIDULong = *(ULong *) ((UChar *) sRow +
                                 sQcmTablesTableOIDColumn->column.offset);
    sTableOID = (smOID) sTableOIDULong;

    *aSequenceHandle = (void*)smiGetTable( sTableOID );

    sTableTypeStr = (mtdCharType*)
        ((UChar*) sRow + sQcmTablesTableTypeColumn->column.offset);
    sTableType = sTableTypeStr->value;

    // make sequenceInfo
    if ( aSequenceInfo != NULL )
    {
        aSequenceInfo->sequenceOwnerID = aUserID;
        aSequenceInfo->sequenceID      = sTableID;
        aSequenceInfo->sequenceHandle  = *aSequenceHandle;
        aSequenceInfo->sequenceOID     = sTableOID;

        if (aSequenceNameSize > 0)
        {
            idlOS::memcpy(aSequenceInfo->name,
                          aSequenceName,
                          aSequenceNameSize);
            aSequenceInfo->name[aSequenceNameSize] = '\0';
        }
        else
        {
            aSequenceInfo->name[0] = '\0';
        }
    }
    else
    {
        // Nothing to do.
    }

    if ( idlOS::strMatch( (SChar*)sTableType,
                          sTableTypeStr->length,
                          "S",
                          1 ) == 0 )
    {
        if ( aSequenceInfo != NULL )
        {
            aSequenceInfo->sequenceType = QCM_SEQUENCE;
        }
        else
        {
            // Nothing to do.
        }
    }
    else if ( idlOS::strMatch( (SChar*)sTableType,
                               sTableTypeStr->length,
                               "W",
                               1 ) == 0 )
    {
        if ( aSequenceInfo != NULL )
        {
            aSequenceInfo->sequenceType = QCM_QUEUE_SEQUENCE;
        }
        else
        {
            // Nothing to do.
        }
    }
    else
    {
        IDE_RAISE(ERR_NOT_EXIST_SEQUENCE);
    }

    sStage = 0;
    IDE_TEST(sCursor.close() != IDE_SUCCESS);

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_NOT_EXIST_SEQUENCE);
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QCM_NOT_EXIST_SEQUENCE));
    }
    IDE_EXCEPTION_END;

    if ( sStage == 1 )
    {
        (void)sCursor.close();
    }

    return IDE_FAILURE;
}

IDE_RC qcm::getSequenceInfo( qcStatement      * aStatement,
                             UInt               aUserID,
                             UChar            * aSequenceName,
                             SInt               aSequenceNameSize,
                             qcmSequenceInfo  * aSequenceInfo,
                             void            ** aSequenceHandle )
{
/***********************************************************************
 *
 * Description :
 *    ������ �̸����� �������� sequenceInfo�� ���Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

    return getSequenceInfoByName( QC_SMI_STMT( aStatement ),
                                  aUserID,
                                  aSequenceName,
                                  aSequenceNameSize,
                                  aSequenceInfo,
                                  aSequenceHandle );
}

IDE_RC qcm::checkSequenceInfo( qcStatement      * aStatement,
                               UInt               aUserID,
                               qcNamePosition     aNamePos,
                               qcmSequenceInfo  * aSequenceInfo,
                               void            ** aSequenceHandle )
{
    if ( QC_IS_NULL_NAME(aNamePos) == ID_TRUE )
    {
        IDE_RAISE( ERR_NOT_EXIST_SEQUENCE );
    }
    else
    {
        IDE_TEST( qcm::getSequenceInfoByName(
                      QC_SMI_STMT(aStatement),
                      aUserID,
                      (UChar*) aNamePos.stmtText + aNamePos.offset,
                      aNamePos.size,
                      aSequenceInfo,
                      aSequenceHandle )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_NOT_EXIST_SEQUENCE );
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QCM_NOT_EXIST_SEQUENCE));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qcm::checkIndexByUser( qcStatement       *aStatement,
                              qcNamePosition     aUserName,
                              qcNamePosition     aIndexName,
                              UInt              *aUserID,
                              UInt              *aTableID,
                              UInt              *aIndexID)
{
/***********************************************************************
 *
 * Description :
 *    userName.indexName �� üũ�Ѵ�.
 *
 * Implementation :
 *    1. user �� �ִ��� üũ�ؼ� userID �� ���Ѵ�.
 *    2. SYS_INDICES_ ĳ���κ��� USER_ID, INDEX_NAME �÷��� ���� �д�.
 *    3. 2�� ������ META_CRASH ���� ��ȯ
 *    4. SYS_INDICES_ ��Ÿ ���̺� USERID_INDEXNAME �� �ε����� ������,
 *       userID, indexName ���� keyRange �� �����.
 *    5. �� Row �� �����鼭(4���� �ε����� ������, userID, indexName��
 *       ���ؼ� ��ġ�ϴ� �͸�) tableID, indexID �� ���Ѵ�.
 *    6. ��ġ�ϴ� �ε����� ������ ������ ��ȯ�Ѵ�.
 *
 ***********************************************************************/

    UInt                    sStage = 0;
    UInt                    i;
    UInt                    sUserID;
    qcmColumn             * sUserIDCol = NULL;
    qcmColumn             * sIndexNameCol = NULL;
    qcmTableInfo          * sQcmIndicesTableInfo;
    idBool                  sIdxFound = ID_FALSE;
    smSCN                   sSCN;
    smiRange                sRange;
    smiTableCursor          sCursor;

    qcNameCharBuffer        sIdxNameBuffer;
    mtdCharType           * sIdxName = (mtdCharType *) & sIdxNameBuffer;

    mtdCharType           * sIdxNameStr;
    const void            * sRow;
    qtcMetaRangeColumn      sFirstRangeColumn;
    qtcMetaRangeColumn      sSecondRangeColumn;
    smiCursorProperties     sCursorProperty;
    scGRID                  sRid; // Disk Table�� ���� Record IDentifier

    *aIndexID = 0;

    // check user existence
    if (QC_IS_NULL_NAME(aUserName) == ID_TRUE)
    {
        sUserID = QCG_GET_SESSION_USER_ID(aStatement);
    }
    else
    {
        IDE_TEST(qcmUser::getUserID( aStatement, aUserName, &sUserID )
                 != IDE_SUCCESS);
    }

    *aUserID = sUserID;

    sCursor.initialize();

    if (gQcmIndices == NULL)
    {
        // on create db.
        IDE_TEST(getTableHandleByName(
                     QC_SMI_STMT( aStatement ),
                     QC_SYSTEM_USER_ID,
                     (UChar *)QCM_INDICES,
                     idlOS::strlen(QCM_INDICES),
                     (void**) &gQcmIndices,
                     &sSCN) != IDE_SUCCESS);
    }

    IDE_TEST( smiGetTableTempInfo( gQcmIndices,
                                   (void**)&sQcmIndicesTableInfo )
              != IDE_SUCCESS );

    // find index node from qcm_indices
    for (i = 0; i < sQcmIndicesTableInfo->columnCount; i++)
    {
        if ( idlOS::strMatch( sQcmIndicesTableInfo->columns[i].name,
                              idlOS::strlen( sQcmIndicesTableInfo->columns[i].name ),
                              "USER_ID",
                              7 ) == 0 )
        {
            sUserIDCol = &sQcmIndicesTableInfo->columns[i];
            // BUGBUG : column module loading...
//            IDE_TEST(mtd::moduleByID(
//                &(sUserIDCol->basicInfo->module),
//                sUserIDCol->basicInfo->type) != IDE_SUCCESS);
        }

        if ( idlOS::strMatch( sQcmIndicesTableInfo->columns[i].name,
                              idlOS::strlen( sQcmIndicesTableInfo->columns[i].name ),
                              "INDEX_NAME",
                              10 ) == 0 )
        {
            sIndexNameCol = &sQcmIndicesTableInfo->columns[i];
        }

        if (sUserIDCol != NULL && sIndexNameCol != NULL)
        {
            break;
        }
    }

    IDE_TEST_RAISE(sUserIDCol == NULL, ERR_META_CRASH);
    IDE_TEST_RAISE(sIndexNameCol == NULL, ERR_META_CRASH);
    IDE_TEST_RAISE(i == sQcmIndicesTableInfo->columnCount, ERR_META_CRASH);

    if (gQcmIndicesIndex[QCM_INDICES_USERID_INDEXNAME_IDX_ORDER] == NULL)
    {
        SMI_CURSOR_PROP_INIT_FOR_META_FULL_SCAN(&sCursorProperty, aStatement->mStatistics);

        // we have to find index by filter.
        IDE_TEST(sCursor.open(
                     QC_SMI_STMT( aStatement ),
                     sQcmIndicesTableInfo->tableHandle,
                     NULL,
                     smiGetRowSCN(sQcmIndicesTableInfo->tableHandle),
                     NULL,
                     smiGetDefaultKeyRange(),
                     smiGetDefaultKeyRange(),
                     smiGetDefaultFilter(),
                     QCM_META_CURSOR_FLAG,
                     SMI_SELECT_CURSOR,
                     & sCursorProperty) != IDE_SUCCESS);
    }
    else
    {
        qtc::setVarcharValue( sIdxName,
                              NULL,
                              aIndexName.stmtText + aIndexName.offset,
                              aIndexName.size);
        makeMetaRangeDoubleColumn(
            &sFirstRangeColumn,
            &sSecondRangeColumn,
            (const mtcColumn*)(sUserIDCol->basicInfo),
            (const void *) & sUserID,
            (const mtcColumn*)(sIndexNameCol->basicInfo),
            (const void *) sIdxName,
            &sRange);

        SMI_CURSOR_PROP_INIT_FOR_META_INDEX_SCAN(&sCursorProperty, aStatement->mStatistics);

        IDE_TEST(sCursor.open(
                     QC_SMI_STMT( aStatement ),
                     gQcmIndicesTableInfo->tableHandle,
                     gQcmIndicesIndex[QCM_INDICES_USERID_INDEXNAME_IDX_ORDER],
                     smiGetRowSCN(sQcmIndicesTableInfo->tableHandle),
                     NULL,
                     &sRange,
                     smiGetDefaultKeyRange(),
                     smiGetDefaultFilter(),
                     QCM_META_CURSOR_FLAG,
                     SMI_SELECT_CURSOR,
                     & sCursorProperty) != IDE_SUCCESS);
    }
    sStage = 1;

    IDE_TEST(sCursor.beforeFirst() != IDE_SUCCESS);

    IDE_TEST(sCursor.readRow(&sRow, &sRid, SMI_FIND_NEXT) != IDE_SUCCESS);

    while (sRow != NULL && sIdxFound == ID_FALSE)
    {
        if (gQcmIndicesIndex[QCM_INDICES_USERID_INDEXNAME_IDX_ORDER] == NULL)
        { // the scan is sequential full scan.
            sIdxNameStr = (mtdCharType*)((UChar*)sRow +
                                         sIndexNameCol->basicInfo->column.offset);

            // BUG-37032
            if( ( *(UInt*)((UChar*)sRow + sUserIDCol->basicInfo->column.offset) == sUserID ) &&
                ( idlOS::strMatch( aIndexName.stmtText + aIndexName.offset,
                                   aIndexName.size,
                                   (SChar*)&(sIdxNameStr->value),
                                   sIdxNameStr->length ) == 0 ) )
            {
                *aTableID = *(UInt*)((UChar *)sRow +
                                     sQcmIndicesTableInfo->
                                     columns[QCM_INDICES_TABLE_ID_COL_ORDER].
                                     basicInfo->column.offset);

                *aIndexID = *(UInt*)((UChar *)sRow +
                                     sQcmIndicesTableInfo->
                                     columns[QCM_INDICES_INDEX_ID_COL_ORDER].
                                     basicInfo->column.offset);
                sIdxFound = ID_TRUE;
            }
            else
            {
                sIdxFound = ID_FALSE;
            }
        }
        else
        {
            sIdxFound = ID_TRUE;
            *aTableID = *(UInt*)((UChar *)sRow +
                                 sQcmIndicesTableInfo->
                                 columns[QCM_INDICES_TABLE_ID_COL_ORDER].
                                 basicInfo->column.offset);

            *aIndexID = *(UInt*)((UChar *)sRow +
                                 sQcmIndicesTableInfo->
                                 columns[QCM_INDICES_INDEX_ID_COL_ORDER].
                                 basicInfo->column.offset);
        }
        IDE_TEST(sCursor.readRow(&sRow, &sRid, SMI_FIND_NEXT) != IDE_SUCCESS);
    }

    sStage = 0;

    IDE_TEST(sCursor.close() != IDE_SUCCESS);

    IDE_TEST_RAISE(sIdxFound != ID_TRUE, ERR_NOT_EXIST_INDEX);

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_META_CRASH);
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QCM_META_CRASH));
    }
    IDE_EXCEPTION(ERR_NOT_EXIST_INDEX);
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QCM_NOT_EXISTS_INDEX));
    }
    IDE_EXCEPTION_END;

    if ( sStage == 1 )
    {
        (void)sCursor.close();
    }

    return IDE_FAILURE;
}

// for search from meta.
void qcm::makeMetaRangeSingleColumn( qtcMetaRangeColumn  * aRangeColumn,
                                     const mtcColumn     * aColumnDesc,
                                     const void          * aValue,
                                     smiRange            * aRange)
{
    UInt sCompareType;

    // PROJ-1872
    // index�� �ִ� Į���� meta range�� ���� �Ǹ�
    // disk index column�� compare�� stored type�� mt type ���� ���̴�.
    if ( ( aColumnDesc->column.flag & SMI_COLUMN_STORAGE_MASK )
         == SMI_COLUMN_STORAGE_DISK )
    {
        sCompareType = MTD_COMPARE_STOREDVAL_MTDVAL;
    }
    else
    {
        sCompareType = MTD_COMPARE_MTDVAL_MTDVAL;
    }

    qtc::initializeMetaRange(aRange, sCompareType);

    qtc::setMetaRangeColumn(aRangeColumn,
                            aColumnDesc,
                            aValue,
                            SMI_COLUMN_ORDER_ASCENDING,
                            0 ); //First columnIdx

    qtc::addMetaRangeColumn(aRange,
                            aRangeColumn);

    qtc::fixMetaRange(aRange);
}

void qcm::makeMetaRangeDoubleColumn(
    qtcMetaRangeColumn  * aFirstRangeColumn,
    qtcMetaRangeColumn  * aSecondRangeColumn,
    const mtcColumn     * aFirstColumnDesc,
    const void          * aFirstColValue,
    const mtcColumn     * aSecondColumnDesc,
    const void          * aSecondColValue,
    smiRange            * aRange)
{
    UInt sCompareType;

    // PROJ-1872
    // index�� �ִ� Į���� meta range�� ���� �Ǹ�
    // disk index column�� compare�� stored type�� mt type ���� ���̴�.
    // First Column�� Second Column ��� ���� table type�̹Ƿ�
    // ù��° type�� ���� Ÿ�Ը����� �� ����� compare type��
    // ���Ҽ� �ִ�.
    if ( ( aFirstColumnDesc->column.flag & SMI_COLUMN_STORAGE_MASK )
         == SMI_COLUMN_STORAGE_DISK )
    {
        sCompareType = MTD_COMPARE_STOREDVAL_MTDVAL;
    }
    else
    {
        sCompareType = MTD_COMPARE_MTDVAL_MTDVAL;
    }

    qtc::initializeMetaRange(aRange, sCompareType);

    qtc::setMetaRangeColumn(aFirstRangeColumn,
                            aFirstColumnDesc,
                            aFirstColValue,
                            SMI_COLUMN_ORDER_ASCENDING,
                            0 ); //First columnIdx

    qtc::addMetaRangeColumn(aRange,
                            aFirstRangeColumn);

    qtc::setMetaRangeColumn(aSecondRangeColumn,
                            aSecondColumnDesc,
                            aSecondColValue,
                            SMI_COLUMN_ORDER_ASCENDING,
                            1 ); //Second columnIdx

    qtc::addMetaRangeColumn(aRange,
                            aSecondRangeColumn);

    qtc::fixMetaRange(aRange);
}

void qcm::makeMetaRangeTripleColumn( qtcMetaRangeColumn  * aFirstRangeColumn,
                                     qtcMetaRangeColumn  * aSecondRangeColumn,
                                     qtcMetaRangeColumn  * aThirdRangeColumn,
                                     const mtcColumn     * aFirstColumnDesc,
                                     const void          * aFirstColValue,
                                     const mtcColumn     * aSecondColumnDesc,
                                     const void          * aSecondColValue,
                                     const mtcColumn     * aThirdColumnDesc,
                                     const void          * aThirdColValue,
                                     smiRange            * aRange)
{
    UInt sCompareType;

    // PROJ-1872
    // index�� �ִ� Į���� meta range�� ���� �Ǹ�
    // disk index column�� compare�� stored type�� mt type ���� ���̴�.
    // First, Second, Third Column ��� ���� table type�̹Ƿ�
    // ù��° type�� ���� Ÿ�Ը����� �� ����� compare type��
    // ���Ҽ� �ִ�.
    if ( ( aFirstColumnDesc->column.flag & SMI_COLUMN_STORAGE_MASK )
         == SMI_COLUMN_STORAGE_DISK )
    {
        sCompareType = MTD_COMPARE_STOREDVAL_MTDVAL;
    }
    else
    {
        sCompareType = MTD_COMPARE_MTDVAL_MTDVAL;
    }

    qtc::initializeMetaRange(aRange, sCompareType);

    qtc::setMetaRangeColumn(aFirstRangeColumn,
                            aFirstColumnDesc,
                            aFirstColValue,
                            SMI_COLUMN_ORDER_ASCENDING,
                            0 ); //First columnIdx

    qtc::addMetaRangeColumn(aRange,
                            aFirstRangeColumn);

    qtc::setMetaRangeColumn(aSecondRangeColumn,
                            aSecondColumnDesc,
                            aSecondColValue,
                            SMI_COLUMN_ORDER_ASCENDING,
                            1 ); //Second columnIdx

    qtc::addMetaRangeColumn(aRange,
                            aSecondRangeColumn);

    qtc::setMetaRangeColumn(aThirdRangeColumn,
                            aThirdColumnDesc,
                            aThirdColValue,
                            SMI_COLUMN_ORDER_ASCENDING,
                            2 ); //Third columnIdx

    qtc::addMetaRangeColumn(aRange,
                            aThirdRangeColumn);

    qtc::fixMetaRange(aRange);
}


void qcm::makeMetaRangeFourColumn( qtcMetaRangeColumn  * aFirstRangeColumn,
                                   qtcMetaRangeColumn  * aSecondRangeColumn,
                                   qtcMetaRangeColumn  * aThirdRangeColumn,
                                   qtcMetaRangeColumn  * aFourthRangeColumn,
                                   const mtcColumn     * aFirstColumnDesc,
                                   const void          * aFirstColValue,
                                   const mtcColumn     * aSecondColumnDesc,
                                   const void          * aSecondColValue,
                                   const mtcColumn     * aThirdColumnDesc,
                                   const void          * aThirdColValue,
                                   const mtcColumn     * aFourthColumnDesc,
                                   const void          * aFourthColValue,
                                   smiRange            * aRange)
{
    UInt sCompareType;

    // PROJ-1872
    // index�� �ִ� Į���� meta range�� ���� �Ǹ�
    // disk index column�� compare�� stored type�� mt type ���� ���̴�.
    // First, Second, Third, Fourth Column ��� ���� table type�̹Ƿ�
    // ù��° type�� ���� Ÿ�Ը����� �� ����� compare type��
    // ���Ҽ� �ִ�.
    if ( ( aFirstColumnDesc->column.flag & SMI_COLUMN_STORAGE_MASK )
         == SMI_COLUMN_STORAGE_DISK )
    {
        sCompareType = MTD_COMPARE_STOREDVAL_MTDVAL;
    }
    else
    {
        sCompareType = MTD_COMPARE_MTDVAL_MTDVAL;
    }

    qtc::initializeMetaRange(aRange, sCompareType);

    qtc::setMetaRangeColumn(aFirstRangeColumn,
                            aFirstColumnDesc,
                            aFirstColValue,
                            SMI_COLUMN_ORDER_ASCENDING,
                            0 ); //First columnIdx

    qtc::addMetaRangeColumn(aRange,
                            aFirstRangeColumn);

    qtc::setMetaRangeColumn(aSecondRangeColumn,
                            aSecondColumnDesc,
                            aSecondColValue,
                            SMI_COLUMN_ORDER_ASCENDING,
                            1 ); //Second columnIdx

    qtc::addMetaRangeColumn(aRange,
                            aSecondRangeColumn);

    qtc::setMetaRangeColumn(aThirdRangeColumn,
                            aThirdColumnDesc,
                            aThirdColValue,
                            SMI_COLUMN_ORDER_ASCENDING,
                            2 ); //Third columnIdx

    qtc::addMetaRangeColumn(aRange,
                            aThirdRangeColumn);

    qtc::setMetaRangeColumn(aFourthRangeColumn,
                            aFourthColumnDesc,
                            aFourthColValue,
                            SMI_COLUMN_ORDER_ASCENDING,
                            3 ); //Fourth columnIdx

    qtc::addMetaRangeColumn(aRange,
                            aFourthRangeColumn);

    qtc::fixMetaRange(aRange);
}


void qcm::makeMetaRangeFiveColumn( qtcMetaRangeColumn  * aFirstRangeColumn,
                                   qtcMetaRangeColumn  * aSecondRangeColumn,
                                   qtcMetaRangeColumn  * aThirdRangeColumn,
                                   qtcMetaRangeColumn  * aFourthRangeColumn,
                                   qtcMetaRangeColumn  * aFifthRangeColumn,
                                   const mtcColumn     * aFirstColumnDesc,
                                   const void          * aFirstColValue,
                                   const mtcColumn     * aSecondColumnDesc,
                                   const void          * aSecondColValue,
                                   const mtcColumn     * aThirdColumnDesc,
                                   const void          * aThirdColValue,
                                   const mtcColumn     * aFourthColumnDesc,
                                   const void          * aFourthColValue,
                                   const mtcColumn     * aFifthColumnDesc,
                                   const void          * aFifthColValue,
                                   smiRange            * aRange)
{
    UInt sCompareType;

    // PROJ-1872
    // index�� �ִ� Į���� meta range�� ���� �Ǹ�
    // disk index column�� compare�� stored type�� mt type ���� ���̴�.
    // First, Second, Third, Fourth, Fifth Column ��� ���� table type�̹Ƿ�
    // ù��° type�� ���� Ÿ�Ը����� �� ����� compare type��
    // ���Ҽ� �ִ�.
    if ( ( aFirstColumnDesc->column.flag & SMI_COLUMN_STORAGE_MASK )
         == SMI_COLUMN_STORAGE_DISK )
    {
        sCompareType = MTD_COMPARE_STOREDVAL_MTDVAL;
    }
    else
    {
        sCompareType = MTD_COMPARE_MTDVAL_MTDVAL;
    }

    qtc::initializeMetaRange(aRange, sCompareType);

    qtc::setMetaRangeColumn(aFirstRangeColumn,
                            aFirstColumnDesc,
                            aFirstColValue,
                            SMI_COLUMN_ORDER_ASCENDING,
                            0 ); //First columnIdx

    qtc::addMetaRangeColumn(aRange,
                            aFirstRangeColumn);

    qtc::setMetaRangeColumn(aSecondRangeColumn,
                            aSecondColumnDesc,
                            aSecondColValue,
                            SMI_COLUMN_ORDER_ASCENDING,
                            1 ); //Second columnIdx

    qtc::addMetaRangeColumn(aRange,
                            aSecondRangeColumn);

    qtc::setMetaRangeColumn(aThirdRangeColumn,
                            aThirdColumnDesc,
                            aThirdColValue,
                            SMI_COLUMN_ORDER_ASCENDING,
                            2 ); //Third columnIdx

    qtc::addMetaRangeColumn(aRange,
                            aThirdRangeColumn);

    qtc::setMetaRangeColumn(aFourthRangeColumn,
                            aFourthColumnDesc,
                            aFourthColValue,
                            SMI_COLUMN_ORDER_ASCENDING,
                            3 ); //Fourth columnIdx

    qtc::addMetaRangeColumn(aRange,
                            aFourthRangeColumn);

    qtc::setMetaRangeColumn(aFifthRangeColumn,
                            aFifthColumnDesc,
                            aFifthColValue,
                            SMI_COLUMN_ORDER_ASCENDING,
                            4 ); //Fifth columnIdx

    qtc::addMetaRangeColumn(aRange,
                            aFifthRangeColumn);

    qtc::fixMetaRange(aRange);
}


IDE_RC qcm::getNextTableID(qcStatement *aStatement,
                           UInt        *aTableID)
{
    void   * sSequenceHandle;
    idBool   sExist;
    idBool   sFirst;
    SLong    sSeqVal;
    SLong    sSeqValFirst = 0;

    sExist = ID_TRUE;
    sFirst = ID_TRUE;

    IDE_TEST(getSequenceHandleByName(
                 QC_SMI_STMT( aStatement ),
                 QC_SYSTEM_USER_ID,
                 (UChar*)gDBSequenceName[QCM_DB_SEQUENCE_TABLEID],
                 idlOS::strlen(gDBSequenceName[QCM_DB_SEQUENCE_TABLEID]),
                 &sSequenceHandle)
             != IDE_SUCCESS);

    while( sExist == ID_TRUE )
    {
        IDE_TEST(smiTable::readSequence( QC_SMI_STMT( aStatement ),
                                         sSequenceHandle,
                                         SMI_SEQUENCE_NEXT,
                                         &sSeqVal,
                                         NULL )
                 != IDE_SUCCESS );

        // sSeqVal�� ��� SLong������, sequence�� ������ ��
        // max�� integer max�� �ȳѵ��� �Ͽ��� ������
        // ���⼭ overflowüũ�� ���� �ʴ´�.
        IDE_TEST( searchTableID( QC_SMI_STMT( aStatement ),
                                 (SInt)sSeqVal,
                                 &sExist )
                  != IDE_SUCCESS );

        if( sFirst == ID_TRUE )
        {
            sSeqValFirst = sSeqVal;
            sFirst = ID_FALSE;
        }
        else
        {
            // ã��ã�� �ѹ��� �� ���.
            // �̴� object�� �� �� ���� �ǹ���.
            IDE_TEST_RAISE( sSeqVal == sSeqValFirst, ERR_OBJECTS_OVERFLOW );
        }
    }

    *aTableID = sSeqVal;

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_OBJECTS_OVERFLOW);
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QCM_MAXIMUM_OBJECT_NUMBER_EXCEED,
                                "objects(table, view, queue or sequence)",
                                QCM_TABLES_SEQ_MAXVALUE) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qcm::getNextIndexID( qcStatement *aStatement,
                            UInt        *aIndexID)
{
    void   * sSequenceHandle = NULL;
    idBool   sExist;
    idBool   sFirst;
    SLong    sSeqVal      = 0;
    SLong    sSeqValFirst = 0;

    sExist = ID_TRUE;
    sFirst = ID_TRUE;

    IDE_TEST(getSequenceHandleByName(
                 QC_SMI_STMT( aStatement ),
                 QC_SYSTEM_USER_ID,
                 (UChar*)gDBSequenceName[QCM_DB_SEQUENCE_INDEXID],
                 idlOS::strlen(gDBSequenceName[QCM_DB_SEQUENCE_INDEXID]),
                 &sSequenceHandle)
             != IDE_SUCCESS);

    while( sExist == ID_TRUE )
    {
        IDE_TEST(smiTable::readSequence( QC_SMI_STMT( aStatement ),
                                         sSequenceHandle,
                                         SMI_SEQUENCE_NEXT,
                                         &sSeqVal,
                                         NULL )
                 != IDE_SUCCESS );

        // sSeqVal�� ��� SLong������, sequence�� ������ ��
        // max�� integer max�� �ȳѵ��� �Ͽ��� ������
        // ���⼭ overflowüũ�� ���� �ʴ´�.
        IDE_TEST( searchIndexID( QC_SMI_STMT( aStatement ),
                                 (SInt)sSeqVal,
                                 &sExist )
                  != IDE_SUCCESS );

        if( sFirst == ID_TRUE )
        {
            sSeqValFirst = sSeqVal;
            sFirst = ID_FALSE;
        }
        else
        {
            // ã��ã�� �ѹ��� �� ���.
            // �̴� object�� �� �� ���� �ǹ���.
            IDE_TEST_RAISE( sSeqVal == sSeqValFirst, ERR_OBJECTS_OVERFLOW );
        }
    }

    *aIndexID = sSeqVal;

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_OBJECTS_OVERFLOW);
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QCM_MAXIMUM_OBJECT_NUMBER_EXCEED,
                                "indices",
                                QCM_META_SEQ_MAXVALUE) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qcm::getNextConstrID( qcStatement *aStatement,
                             UInt        *aConstrID)
{
    void   * sSequenceHandle;
    idBool   sExist;
    idBool   sFirst;
    SLong    sSeqVal;
    SLong    sSeqValFirst = 0;

    sExist = ID_TRUE;
    sFirst = ID_TRUE;

    IDE_TEST(getSequenceHandleByName(
                 QC_SMI_STMT( aStatement ),
                 QC_SYSTEM_USER_ID,
                 (UChar*)gDBSequenceName[QCM_DB_SEQUENCE_CONSTRID],
                 idlOS::strlen(gDBSequenceName[QCM_DB_SEQUENCE_CONSTRID]),
                 &sSequenceHandle)
             != IDE_SUCCESS);

    while( sExist == ID_TRUE )
    {
        IDE_TEST(smiTable::readSequence( QC_SMI_STMT( aStatement ),
                                         sSequenceHandle,
                                         SMI_SEQUENCE_NEXT,
                                         &sSeqVal,
                                         NULL )
                 != IDE_SUCCESS );

        // sSeqVal�� ��� SLong������, sequence�� ������ ��
        // max�� integer max�� �ȳѵ��� �Ͽ��� ������
        // ���⼭ overflowüũ�� ���� �ʴ´�.
        IDE_TEST( searchConstrID( QC_SMI_STMT( aStatement ),
                                  (SInt)sSeqVal,
                                  &sExist )
                  != IDE_SUCCESS );

        if( sFirst == ID_TRUE )
        {
            sSeqValFirst = sSeqVal;
            sFirst = ID_FALSE;
        }
        else
        {
            // ã��ã�� �ѹ��� �� ���.
            // �̴� object�� �� �� ���� �ǹ���.
            IDE_TEST_RAISE( sSeqVal == sSeqValFirst, ERR_OBJECTS_OVERFLOW );
        }
    }

    *aConstrID = sSeqVal;

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_OBJECTS_OVERFLOW);
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QCM_MAXIMUM_OBJECT_NUMBER_EXCEED,
                                "constraints",
                                QCM_META_SEQ_MAXVALUE) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qcm::getNextDirectoryID(qcStatement *aStatement,
                               SLong       *aDirectoryID)
{
    void   * sSequenceHandle;
    SLong    sSeqVal;

    IDE_TEST(getSequenceHandleByName(
                 QC_SMI_STMT( aStatement ),
                 QC_SYSTEM_USER_ID,
                 (UChar*)gDBSequenceName[QCM_DB_SEQUENCE_DIRECTORYID],
                 idlOS::strlen(gDBSequenceName[QCM_DB_SEQUENCE_DIRECTORYID]),
                 &sSequenceHandle)
             != IDE_SUCCESS);

    IDE_TEST(smiTable::readSequence( QC_SMI_STMT( aStatement ),
                                     sSequenceHandle,
                                     SMI_SEQUENCE_NEXT,
                                     &sSeqVal,
                                     NULL )
             != IDE_SUCCESS );

    *aDirectoryID = sSeqVal;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

// PROJ-1685
IDE_RC qcm::getNextLibraryID(qcStatement *aStatement,
                             SLong       *aLibraryID)
{
    void   * sSequenceHandle;
    SLong    sSeqVal;

    IDE_TEST(getSequenceHandleByName(
                 QC_SMI_STMT( aStatement ),
                 QC_SYSTEM_USER_ID,
                 (UChar*)gDBSequenceName[QCM_DB_SEQUENCE_LIBRARYID],
                 idlOS::strlen(gDBSequenceName[QCM_DB_SEQUENCE_LIBRARYID]),
                 &sSequenceHandle)
             != IDE_SUCCESS);

    IDE_TEST(smiTable::readSequence( QC_SMI_STMT( aStatement ),
                                     sSequenceHandle,
                                     SMI_SEQUENCE_NEXT,
                                     &sSeqVal,
                                     NULL )
             != IDE_SUCCESS );

    *aLibraryID = sSeqVal;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qcm::getNextDictionaryID(qcStatement *aStatement,
                                UInt        *aDictionaryID)
{
    void   * sSequenceHandle;
    SLong    sSeqVal;

    IDE_TEST(getSequenceHandleByName(
                 QC_SMI_STMT( aStatement ),
                 QC_SYSTEM_USER_ID,
                 (UChar*)gDBSequenceName[QCM_DB_SEQUENCE_DICTIONARY_ID],
                 idlOS::strlen(gDBSequenceName[QCM_DB_SEQUENCE_DICTIONARY_ID]),
                 &sSequenceHandle)
             != IDE_SUCCESS);

    IDE_TEST(smiTable::readSequence( QC_SMI_STMT( aStatement ),
                                     sSequenceHandle,
                                     SMI_SEQUENCE_NEXT,
                                     &sSeqVal,
                                     NULL )
             != IDE_SUCCESS );

    *aDictionaryID = sSeqVal;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/* PROJ-1438 Job Scheduler */
IDE_RC qcm::getNextJobID( qcStatement * aStatement, UInt * aJobID )
{
    void   * sSequenceHandle = NULL;
    idBool   sExist;
    idBool   sFirst;
    SLong    sSeqVal = 0;
    SLong    sSeqValFirst = 0;

    sExist = ID_TRUE;
    sFirst = ID_TRUE;

    IDE_TEST(getSequenceHandleByName(
                 QC_SMI_STMT( aStatement ),
                 QC_SYSTEM_USER_ID,
                 (UChar*)gDBSequenceName[QCM_DB_SEQUENCE_JOB_ID],
                 idlOS::strlen(gDBSequenceName[QCM_DB_SEQUENCE_JOB_ID]),
                 &sSequenceHandle)
             != IDE_SUCCESS);

    while( sExist == ID_TRUE )
    {
        IDE_TEST(smiTable::readSequence( QC_SMI_STMT( aStatement ),
                                         sSequenceHandle,
                                         SMI_SEQUENCE_NEXT,
                                         &sSeqVal,
                                         NULL )
                 != IDE_SUCCESS );

        // sSeqVal�� ��� SLong������, sequence�� ������ ��
        // max�� integer max�� �ȳѵ��� �Ͽ��� ������
        // ���⼭ overflowüũ�� ���� �ʴ´�.
        IDE_TEST( searchJobID( QC_SMI_STMT( aStatement ),
                                 (SInt)sSeqVal,
                                 &sExist )
                  != IDE_SUCCESS );

        if( sFirst == ID_TRUE )
        {
            sSeqValFirst = sSeqVal;
            sFirst = ID_FALSE;
        }
        else
        {
            // ã��ã�� �ѹ��� �� ���.
            // �̴� object�� �� �� ���� �ǹ���.
            IDE_TEST_RAISE( sSeqVal == sSeqValFirst, ERR_OBJECTS_OVERFLOW );
        }
    }

    *aJobID = sSeqVal;

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_OBJECTS_OVERFLOW);
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QCM_MAXIMUM_OBJECT_NUMBER_EXCEED,
                                "jobs",
                                QCM_META_SEQ_MAXVALUE) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

//call by qcm::getNextTableID()
IDE_RC qcm::searchTableID( smiStatement * aSmiStmt,
                           SInt           aTableID,
                           idBool       * aExist )
{
/***********************************************************************
 *
 * Description : table id�� �����ϴ��� �˻�.
 *
 * Implementation :
 *
 ***********************************************************************/

    UInt                sStage = 0;
    smiRange            sRange;
    smiTableCursor      sCursor;
    const void          *sRow;
    UInt                sFlag = QCM_META_CURSOR_FLAG;
    mtcColumn           *sQcmTablesIndexColumn;
    qtcMetaRangeColumn  sRangeColumn;

    scGRID              sRid; // Disk Table�� ���� Record IDentifier
    smiCursorProperties sCursorProperty;

    *aExist = ID_FALSE;

    if( gQcmTablesIndex[QCM_TABLES_TABLEID_IDX_ORDER] == NULL )
    {
        // createdb�ϴ� �����.
        // �̶��� �˻� �� �ʿ䰡 ����
    }
    else
    {
        sCursor.initialize();

        IDE_TEST( smiGetTableColumns( gQcmTables,
                                      QCM_TABLES_TABLE_ID_COL_ORDER,
                                      (const smiColumn**)&sQcmTablesIndexColumn )
                  != IDE_SUCCESS );

        makeMetaRangeSingleColumn(
            &sRangeColumn,
            (const mtcColumn *) sQcmTablesIndexColumn,
            (const void *) &aTableID,
            &sRange);

        SMI_CURSOR_PROP_INIT_FOR_META_INDEX_SCAN(&sCursorProperty, NULL);

        IDE_TEST(sCursor.open(
                     aSmiStmt,
                     gQcmTables,
                     gQcmTablesIndex[QCM_TABLES_TABLEID_IDX_ORDER],
                     smiGetRowSCN(gQcmTables),
                     NULL,
                     &sRange,
                     smiGetDefaultKeyRange(),
                     smiGetDefaultFilter(),
                     sFlag,
                     SMI_SELECT_CURSOR,
                     &sCursorProperty) != IDE_SUCCESS);
        sStage = 1;

        IDE_TEST(sCursor.beforeFirst() != IDE_SUCCESS);

        IDE_TEST(sCursor.readRow(&sRow, &sRid, SMI_FIND_NEXT) != IDE_SUCCESS);

        if( sRow != NULL )
        {
            *aExist = ID_TRUE;
        }
        else
        {
            /* Nothing to do */
        }
        
        sStage = 0;
        IDE_TEST( sCursor.close() != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if ( sStage == 1 )
    {
        (void)sCursor.close();
    }

    return IDE_FAILURE;
}

IDE_RC qcm::searchIndexID( smiStatement * aSmiStmt,
                           SInt           aIndexID,
                           idBool       * aExist )
{
/***********************************************************************
 *
 * Description : index id�� �����ϴ��� �˻�.
 *
 * Implementation :
 *
 ***********************************************************************/

    UInt                sStage = 0;
    smiRange            sRange;
    smiTableCursor      sCursor;
    const void          *sRow;
    UInt                sFlag = QCM_META_CURSOR_FLAG;
    mtcColumn           *sQcmIndicesIndexColumn;
    qtcMetaRangeColumn  sRangeColumn;

    scGRID              sRid; // Disk Table�� ���� Record IDentifier
    smiCursorProperties sCursorProperty;

    if( gQcmIndicesIndex[QCM_INDICES_INDEXID_INDEX_TYPE_IDX_ORDER] == NULL )
    {
        // createdb�ϴ� �����.
        // �̶��� �˻� �� �ʿ䰡 ����
        *aExist = ID_FALSE;
    }
    else
    {
        sCursor.initialize();

        IDE_TEST( smiGetTableColumns( gQcmIndices,
                                      QCM_INDICES_INDEX_ID_COL_ORDER,
                                      (const smiColumn**)&sQcmIndicesIndexColumn )
                  != IDE_SUCCESS );

        makeMetaRangeSingleColumn(
            &sRangeColumn,
            (const mtcColumn *) sQcmIndicesIndexColumn,
            (const void *) &aIndexID,
            &sRange);

        SMI_CURSOR_PROP_INIT_FOR_META_INDEX_SCAN(&sCursorProperty, NULL);

        IDE_TEST(sCursor.open(
                     aSmiStmt,
                     gQcmIndices,
                     gQcmIndicesIndex[QCM_INDICES_INDEXID_INDEX_TYPE_IDX_ORDER],
                     smiGetRowSCN(gQcmIndices),
                     NULL,
                     &sRange,
                     smiGetDefaultKeyRange(),
                     smiGetDefaultFilter(),
                     sFlag,
                     SMI_SELECT_CURSOR,
                     &sCursorProperty) != IDE_SUCCESS);
        sStage = 1;

        IDE_TEST(sCursor.beforeFirst() != IDE_SUCCESS);

        IDE_TEST(sCursor.readRow(&sRow, &sRid, SMI_FIND_NEXT) != IDE_SUCCESS);

        if( sRow == NULL )
        {
            *aExist = ID_FALSE;
        }
        else
        {
            *aExist = ID_TRUE;
        }

        sStage = 0;
        IDE_TEST(sCursor.close() != IDE_SUCCESS);
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if ( sStage == 1 )
    {
        (void)sCursor.close();
    }

    return IDE_FAILURE;
}

IDE_RC qcm::searchConstrID( smiStatement * aSmiStmt,
                            SInt           aConstrID,
                            idBool       * aExist )
{
/***********************************************************************
 *
 * Description : constraint id�� �����ϴ��� �˻�.
 *
 * Implementation :
 *
 ***********************************************************************/

    UInt                sStage = 0;
    smiRange            sRange;
    smiTableCursor      sCursor;
    const void          *sRow;
    UInt                sFlag = QCM_META_CURSOR_FLAG;
    mtcColumn           *sQcmConstraintsIndexColumn;
    qtcMetaRangeColumn  sRangeColumn;

    scGRID              sRid; // Disk Table�� ���� Record IDentifier
    smiCursorProperties sCursorProperty;

    if( gQcmConstraintsIndex[QCM_CONSTRAINTS_CONSTID_IDX_ORDER] == NULL )
    {
        // createdb�ϴ� �����.
        // �̶��� �˻� �� �ʿ䰡 ����
        *aExist = ID_FALSE;
    }
    else
    {
        sCursor.initialize();

        IDE_TEST( smiGetTableColumns( gQcmConstraints,
                                      QCM_CONSTRAINTS_CONSTRAINT_ID_COL_ORDER,
                                      (const smiColumn**)&sQcmConstraintsIndexColumn )
                  != IDE_SUCCESS );

        makeMetaRangeSingleColumn(
            &sRangeColumn,
            (const mtcColumn *) sQcmConstraintsIndexColumn,
            (const void *) &aConstrID,
            &sRange);

        SMI_CURSOR_PROP_INIT_FOR_META_INDEX_SCAN(&sCursorProperty, NULL);

        IDE_TEST(sCursor.open(
                     aSmiStmt,
                     gQcmConstraints,
                     gQcmConstraintsIndex[QCM_CONSTRAINTS_CONSTID_IDX_ORDER],
                     smiGetRowSCN(gQcmConstraints),
                     NULL,
                     &sRange,
                     smiGetDefaultKeyRange(),
                     smiGetDefaultFilter(),
                     sFlag,
                     SMI_SELECT_CURSOR,
                     &sCursorProperty) != IDE_SUCCESS);
        sStage = 1;

        IDE_TEST(sCursor.beforeFirst() != IDE_SUCCESS);

        IDE_TEST(sCursor.readRow(&sRow, &sRid, SMI_FIND_NEXT) != IDE_SUCCESS);

        if( sRow == NULL )
        {
            *aExist = ID_FALSE;
        }
        else
        {
            *aExist = ID_TRUE;
        }

        sStage = 0;
        IDE_TEST(sCursor.close() != IDE_SUCCESS);
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if ( sStage == 1 )
    {
        (void)sCursor.close();
    }

    return IDE_FAILURE;
}

IDE_RC qcm::searchJobID( smiStatement * aSmiStmt,
                         SInt           aJobID,
                         idBool       * aExist )
{
    UInt                sStage = 0;
    smiRange            sRange;
    smiTableCursor      sCursor;
    const void          *sRow;
    UInt                sFlag = QCM_META_CURSOR_FLAG;
    mtcColumn           *sQcmJobIDColumn;
    qtcMetaRangeColumn  sRangeColumn;

    scGRID              sRid; // Disk Table�� ���� Record IDentifier
    smiCursorProperties sCursorProperty;

    if( gQcmJobsIndex[QCM_JOBS_ID_IDX_ORDER] == NULL )
    {
        // createdb�ϴ� �����.
        // �̶��� �˻� �� �ʿ䰡 ����
        *aExist = ID_FALSE;
    }
    else
    {
        sCursor.initialize();

        IDE_TEST( smiGetTableColumns( gQcmJobs,
                                      QCM_JOBS_ID_COL_ORDER,
                                      (const smiColumn**)&sQcmJobIDColumn )
                  != IDE_SUCCESS );

        makeMetaRangeSingleColumn(
                &sRangeColumn,
                (const mtcColumn *) sQcmJobIDColumn,
                (const void *) &aJobID,
                &sRange);

        SMI_CURSOR_PROP_INIT_FOR_META_INDEX_SCAN(&sCursorProperty, NULL);

        IDE_TEST(sCursor.open(
                        aSmiStmt,
                        gQcmJobs,
                        gQcmJobsIndex[QCM_JOBS_ID_IDX_ORDER],
                        smiGetRowSCN(gQcmJobs),
                        NULL,
                        &sRange,
                        smiGetDefaultKeyRange(),
                        smiGetDefaultFilter(),
                        sFlag,
                        SMI_SELECT_CURSOR,
                        &sCursorProperty) != IDE_SUCCESS);
        sStage = 1;

        IDE_TEST(sCursor.beforeFirst() != IDE_SUCCESS);

        IDE_TEST(sCursor.readRow(&sRow, &sRid, SMI_FIND_NEXT) != IDE_SUCCESS);

        if( sRow == NULL )
        {
            *aExist = ID_FALSE;
        }
        else
        {
            *aExist = ID_TRUE;
        }

        sStage = 0;
        IDE_TEST(sCursor.close() != IDE_SUCCESS);
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if ( sStage == 1 )
    {
        (void)sCursor.close();
    }
    else
    {
        /* Nothing to do */
    }

    return IDE_FAILURE;
}

IDE_RC qcm::getMetaTable( const SChar   * aMetaTableName,
                          const void   ** aTableHandle,
                          smiStatement  * aSmiStmt  )
{
    smSCN sTableSCN;
    UInt i;
    mtcColumn * sColumn;
    UInt        sColumnCnt;

    IDE_TEST(getTableHandleByName( aSmiStmt,
                                   QC_SYSTEM_USER_ID,
                                   (UChar *)aMetaTableName,
                                   idlOS::strlen(aMetaTableName),
                                   (void**)aTableHandle,
                                   &sTableSCN)
             != IDE_SUCCESS );

    // PROJ-1359 Trigger
    // ���� ���� �� �ѹ��� Module Pointer�� �����ϸ� �ȴ�.
    sColumnCnt = smiGetTableColumnCount( *aTableHandle );
    for ( i = 0; i < sColumnCnt; i++ )
    {
        IDE_TEST( smiGetTableColumns( *aTableHandle,
                                      i,
                                      (const smiColumn**)&sColumn )
                  != IDE_SUCCESS );

        // mtdModule ����
        IDE_TEST( mtd::moduleById( &sColumn->module,
                                   sColumn->type.dataTypeId )
                  != IDE_SUCCESS );

        // mtlModule ����
        IDE_TEST( mtl::moduleById( &sColumn->language,
                                   sColumn->type.languageId )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

// called by qdd::executeDropUser
IDE_RC qcm::checkObjectByUserID( qcStatement * aStatement,
                                 UInt          aUserID,
                                 idBool      * aIsTrue )
{
/***********************************************************************
 *
 * Description :
 *    ��õ� userID ������ ������Ʈ�� �ִ��� üũ�Ѵ�.
 *
 * Implementation :
 *    1. SYS_TABLES_ ���̺��� USER_ID �÷��� ���� �д�.
 *    2. ��õ� userID �� keyRange �� �����.
 *    3. ��ġ�ϴ� object �� ������ aIsTrue �� ID_TRUE �� �����Ѵ�.
 *    4. 1,2,3 �� ������ sys_indices_, sys_constraints_, sys_procedures_
 *       �� ���ؼ� �ݺ��Ѵ�.
 *    5. �����ϰ� �ִ� object �� ������ aIsTrue ���� ID_FALSE �� ��ȯ�ȴ�.
 *
 ***********************************************************************/

    SInt                    sStage =0;
    smiRange                sRange;
    mtcColumn             * sColumn;
    qtcMetaRangeColumn      sRangeColumn;
    const void            * sRow;
    smiTableCursor          sCursor;
    smiCursorProperties     sCursorProperty;
    scGRID                  sRid; // Disk Table�� ���� Record IDentifier

    *aIsTrue = ID_FALSE;

    sCursor.initialize();

    // search sys_tables_
    IDE_TEST( smiGetTableColumns( gQcmTables,
                                  QCM_TABLES_USER_ID_COL_ORDER,
                                  (const smiColumn**)&sColumn )
              != IDE_SUCCESS );

    makeMetaRangeSingleColumn(&sRangeColumn,
                              (const mtcColumn*)sColumn,
                              (const void *) &(aUserID),
                              &sRange);

    SMI_CURSOR_PROP_INIT_FOR_META_INDEX_SCAN(&sCursorProperty, aStatement->mStatistics);

    IDE_TEST(sCursor.open(
                 QC_SMI_STMT( aStatement ),
                 gQcmTables,
                 gQcmTablesIndex[QCM_TABLES_USERID_IDX_ORDER],
                 smiGetRowSCN(gQcmTables),
                 NULL,
                 &sRange,
                 smiGetDefaultKeyRange(),
                 smiGetDefaultFilter(),
                 QCM_META_CURSOR_FLAG,
                 SMI_SELECT_CURSOR,
                 & sCursorProperty) != IDE_SUCCESS);

    sStage = 1;
    IDE_TEST(sCursor.beforeFirst() != IDE_SUCCESS);

    IDE_TEST(sCursor.readRow(&sRow, &sRid, SMI_FIND_NEXT) != IDE_SUCCESS);
    if (sRow != NULL)
    {
        *aIsTrue = ID_TRUE;
        sStage = 0;
        IDE_TEST(sCursor.close() != IDE_SUCCESS);
        IDE_CONT(OBJECT_EXIST);
    }
    sStage = 0;

    IDE_TEST(sCursor.close() != IDE_SUCCESS);

    //-----------------------------------
    // PROJ-1502 PARTITIONED DISK TABLE
    // search SYS_TABLE_PARTITIONS_
    //-----------------------------------
    IDE_TEST( smiGetTableColumns( gQcmTablePartitions,
                                  QCM_TABLE_PARTITIONS_USER_ID_COL_ORDER,
                                  (const smiColumn**)&sColumn )
              != IDE_SUCCESS );

    qcm::makeMetaRangeSingleColumn( &sRangeColumn,
                                    (const mtcColumn*)sColumn,
                                    (const void *) &(aUserID),
                                    &sRange );

    IDE_TEST(sCursor.open(
                 QC_SMI_STMT(aStatement),
                 gQcmTablePartitions,
                 gQcmTablePartitionsIndex[QCM_TABLE_PARTITIONS_IDX3_ORDER],
                 smiGetRowSCN(gQcmTablePartitions),
                 NULL,
                 &sRange,
                 smiGetDefaultKeyRange(),
                 smiGetDefaultFilter(),
                 QCM_META_CURSOR_FLAG,
                 SMI_SELECT_CURSOR,
                 & sCursorProperty) != IDE_SUCCESS);

    sStage = 1;
    IDE_TEST(sCursor.beforeFirst() != IDE_SUCCESS);

    IDE_TEST(sCursor.readRow(&sRow, &sRid, SMI_FIND_NEXT) != IDE_SUCCESS);
    if (sRow != NULL)
    {
        *aIsTrue = ID_TRUE;
        sStage = 0;
        IDE_TEST(sCursor.close() != IDE_SUCCESS);
        IDE_CONT(OBJECT_EXIST);
    }
    else
    {
        /* Nothing to do */
    }

    sStage = 0;

    IDE_TEST(sCursor.close() != IDE_SUCCESS);


    // search sys_indices_

    IDE_TEST( smiGetTableColumns( gQcmIndices,
                                  QCM_INDICES_USER_ID_COL_ORDER,
                                  (const smiColumn**)&sColumn )
              != IDE_SUCCESS );
    makeMetaRangeSingleColumn(&sRangeColumn,
                              (const mtcColumn*)sColumn,
                              (const void *) &(aUserID),
                              &sRange);
    IDE_TEST(sCursor.open(
                 QC_SMI_STMT( aStatement ),
                 gQcmIndices,
                 gQcmIndicesIndex[QCM_INDICES_USERID_INDEXNAME_IDX_ORDER],
                 smiGetRowSCN(gQcmIndices),
                 NULL,
                 &sRange,
                 smiGetDefaultKeyRange(),
                 smiGetDefaultFilter(),
                 QCM_META_CURSOR_FLAG,
                 SMI_SELECT_CURSOR,
                 & sCursorProperty) != IDE_SUCCESS);
    sStage = 1;
    IDE_TEST(sCursor.beforeFirst() != IDE_SUCCESS);

    IDE_TEST(sCursor.readRow(&sRow, &sRid, SMI_FIND_NEXT) != IDE_SUCCESS);
    if (sRow != NULL)
    {
        *aIsTrue = ID_TRUE;
        sStage = 0;
        IDE_TEST(sCursor.close() != IDE_SUCCESS);
        IDE_CONT(OBJECT_EXIST);
    }
    else
    {
        /* Nothing to do */
    }

    sStage = 0;

    IDE_TEST(sCursor.close() != IDE_SUCCESS);


    //-----------------------------------
    // PROJ-1502 PARTITIONED DISK TABLE
    // search SYS_INDEX_PARTITIONS_
    //-----------------------------------
    IDE_TEST( smiGetTableColumns( gQcmIndexPartitions,
                                  QCM_INDEX_PARTITIONS_USER_ID_COL_ORDER,
                                  (const smiColumn**)&sColumn )
              != IDE_SUCCESS );

    qcm::makeMetaRangeSingleColumn( &sRangeColumn,
                                    (const mtcColumn*)sColumn,
                                    (const void *) &(aUserID),
                                    &sRange );

    IDE_TEST(sCursor.open(
                 QC_SMI_STMT( aStatement ),
                 gQcmIndexPartitions,
                 gQcmIndexPartitionsIndex[QCM_INDEX_PARTITIONS_IDX4_ORDER],
                 smiGetRowSCN(gQcmIndexPartitions),
                 NULL,
                 &sRange,
                 smiGetDefaultKeyRange(),
                 smiGetDefaultFilter(),
                 QCM_META_CURSOR_FLAG,
                 SMI_SELECT_CURSOR,
                 & sCursorProperty) != IDE_SUCCESS);

    sStage = 1;
    IDE_TEST(sCursor.beforeFirst() != IDE_SUCCESS);

    IDE_TEST(sCursor.readRow(&sRow, &sRid, SMI_FIND_NEXT) != IDE_SUCCESS);

    if (sRow != NULL)
    {
        *aIsTrue = ID_TRUE;
        sStage = 0;
        IDE_TEST(sCursor.close() != IDE_SUCCESS);
        IDE_CONT(OBJECT_EXIST);
    }
    else
    {
        /* Nothing to do */
    }

    sStage = 0;

    IDE_TEST(sCursor.close() != IDE_SUCCESS);


    // search sys_constraints_
    IDE_TEST( smiGetTableColumns( gQcmConstraints,
                                  QCM_CONSTRAINTS_USER_ID_COL_ORDER,
                                  (const smiColumn**)&sColumn )
              != IDE_SUCCESS );

    makeMetaRangeSingleColumn(&sRangeColumn,
                              (const mtcColumn*)sColumn,
                              (const void *) &(aUserID),
                              &sRange);

    IDE_TEST(sCursor.open(
                 QC_SMI_STMT( aStatement ),
                 gQcmConstraints,
                 gQcmConstraintsIndex
                 [QCM_CONSTRAINTS_USERID_CONSTID_IDX_ORDER],
                 smiGetRowSCN(gQcmConstraints),
                 NULL,
                 &sRange,
                 smiGetDefaultKeyRange(),
                 smiGetDefaultFilter(),
                 QCM_META_CURSOR_FLAG,
                 SMI_SELECT_CURSOR,
                 & sCursorProperty) != IDE_SUCCESS);
    sStage = 1;
    IDE_TEST(sCursor.beforeFirst() != IDE_SUCCESS);
    IDE_TEST(sCursor.readRow(&sRow, &sRid, SMI_FIND_NEXT) != IDE_SUCCESS);
    if (sRow != NULL)
    {
        *aIsTrue = ID_TRUE;
        sStage = 0;
        IDE_TEST(sCursor.close() != IDE_SUCCESS);
        IDE_CONT(OBJECT_EXIST);
    }
    else
    {
        /* Nothing to do */
    }

    sStage = 0;

    IDE_TEST(sCursor.close() != IDE_SUCCESS);

    // search sys_procedures_
    IDE_TEST( smiGetTableColumns( gQcmProcedures,
                                  QCM_PROCEDURES_USERID_COL_ORDER,
                                  (const smiColumn**)&sColumn )
              != IDE_SUCCESS );

    makeMetaRangeSingleColumn(&sRangeColumn,
                              (const mtcColumn*)sColumn,
                              (const void *) &(aUserID),
                              &sRange);

    IDE_TEST(sCursor.open(
                 QC_SMI_STMT( aStatement ),
                 gQcmProcedures,
                 gQcmProceduresIndex[QCM_PROCEDURES_USERID_IDX_ORDER],
                 smiGetRowSCN(gQcmProcedures),
                 NULL,
                 &sRange,
                 smiGetDefaultKeyRange(),
                 smiGetDefaultFilter(),
                 QCM_META_CURSOR_FLAG,
                 SMI_SELECT_CURSOR,
                 & sCursorProperty) != IDE_SUCCESS);
    sStage = 1;
    IDE_TEST(sCursor.beforeFirst() != IDE_SUCCESS);
    IDE_TEST(sCursor.readRow(&sRow, &sRid, SMI_FIND_NEXT) != IDE_SUCCESS);
    if (sRow != NULL)
    {
        *aIsTrue = ID_TRUE;
        sStage = 0;
        IDE_TEST(sCursor.close() != IDE_SUCCESS);
        IDE_CONT(OBJECT_EXIST);
    }
    else
    {
        /* Nothing to do */
    }

    sStage = 0;

    IDE_TEST(sCursor.close() != IDE_SUCCESS);

    // PROJ-1073 Package
    // search sys_packages
    IDE_TEST( smiGetTableColumns( gQcmPkgs,
                                  QCM_PKGS_USERID_COL_ORDER,
                                  (const smiColumn**)&sColumn )
              != IDE_SUCCESS );

    makeMetaRangeSingleColumn(&sRangeColumn,
                              (const mtcColumn*)sColumn,
                              (const void *) &(aUserID),
                              &sRange);

    IDE_TEST(sCursor.open(
                 QC_SMI_STMT( aStatement ),
                 gQcmPkgs,
                 gQcmPkgsIndex[QCM_PKGS_USERID_IDX_ORDER],
                 smiGetRowSCN(gQcmPkgs),
                 NULL,
                 &sRange,
                 smiGetDefaultKeyRange(),
                 smiGetDefaultFilter(),
                 QCM_META_CURSOR_FLAG,
                 SMI_SELECT_CURSOR,
                 & sCursorProperty) != IDE_SUCCESS);
    sStage = 1;
    IDE_TEST(sCursor.beforeFirst() != IDE_SUCCESS);
    IDE_TEST(sCursor.readRow(&sRow, &sRid, SMI_FIND_NEXT) != IDE_SUCCESS);
    if (sRow != NULL)
    {
        *aIsTrue = ID_TRUE;
        sStage = 0;
        IDE_TEST(sCursor.close() != IDE_SUCCESS);
        IDE_CONT(OBJECT_EXIST);
    }
    else
    {
        /* Nothing to do */
    }

    sStage = 0;

    IDE_TEST(sCursor.close() != IDE_SUCCESS);


    // PROJ-1359 Trigger
    // search sys_triggers_
    IDE_TEST( smiGetTableColumns( gQcmTriggers,
                                  QCM_TRIGGERS_USER_ID,
                                  (const smiColumn**)&sColumn )
              != IDE_SUCCESS );

    makeMetaRangeSingleColumn(&sRangeColumn,
                              (const mtcColumn*)sColumn,
                              (const void *) &(aUserID),
                              &sRange);

    IDE_TEST(sCursor.open(
                 QC_SMI_STMT( aStatement ),
                 gQcmTriggers,
                 gQcmTriggersIndex[QCM_TRIGGERS_USERID_TRIGGERNAME_INDEX],
                 smiGetRowSCN(gQcmTriggers),
                 NULL,
                 &sRange,
                 smiGetDefaultKeyRange(),
                 smiGetDefaultFilter(),
                 QCM_META_CURSOR_FLAG,
                 SMI_SELECT_CURSOR,
                 & sCursorProperty) != IDE_SUCCESS);
    sStage = 1;
    IDE_TEST(sCursor.beforeFirst() != IDE_SUCCESS);
    IDE_TEST(sCursor.readRow(&sRow, &sRid, SMI_FIND_NEXT) != IDE_SUCCESS);
    if (sRow != NULL)
    {
        *aIsTrue = ID_TRUE;
        sStage = 0;
        IDE_TEST(sCursor.close() != IDE_SUCCESS);
        IDE_CONT(OBJECT_EXIST);
    }
    else
    {
        /* Nothing to do */
    }

    sStage = 0;

    IDE_TEST(sCursor.close() != IDE_SUCCESS);

    // PROJ-1076 synonym
    IDE_TEST( smiGetTableColumns( gQcmSynonyms,
                                  QCM_SYNONYMS_SYNONYM_OWNER_ID_COL_ORDER,
                                  (const smiColumn**)&sColumn )
              != IDE_SUCCESS );

    makeMetaRangeSingleColumn(&sRangeColumn,
                              (const mtcColumn*)sColumn,
                              (const void *) &(aUserID),
                              &sRange);

    IDE_TEST(sCursor.open(
                 QC_SMI_STMT( aStatement ),
                 gQcmSynonyms,
                 gQcmSynonymsIndex
                 [QCM_SYNONYMS_USERID_SYNONYMNAME_IDX_ORDER],
                 smiGetRowSCN(gQcmSynonyms),
                 NULL,
                 &sRange,
                 smiGetDefaultKeyRange(),
                 smiGetDefaultFilter(),
                 QCM_META_CURSOR_FLAG,
                 SMI_SELECT_CURSOR,
                 & sCursorProperty) != IDE_SUCCESS);
    sStage = 1;
    IDE_TEST(sCursor.beforeFirst() != IDE_SUCCESS);
    IDE_TEST(sCursor.readRow(&sRow, &sRid, SMI_FIND_NEXT) != IDE_SUCCESS);
    if (sRow != NULL)
    {
        *aIsTrue = ID_TRUE;
        sStage = 0;
        IDE_TEST(sCursor.close() != IDE_SUCCESS);
        IDE_CONT(OBJECT_EXIST);
    }
    else
    {
        /* Nothing to do */
    }

    sStage = 0;

    IDE_TEST(sCursor.close() != IDE_SUCCESS);


    /* PROJ-1832 New database link */
    IDE_TEST( qcmDatabaseLinksIsUserExisted( aStatement,
                                             aUserID,
                                             aIsTrue )
              != IDE_SUCCESS );
    if  ( *aIsTrue == ID_TRUE )
    {
        *aIsTrue = ID_TRUE;
        IDE_CONT(OBJECT_EXIST);
    }
    else
    {
        /* Nothing to do */
    }

    // PROJ-1371 Directories
    // search SYS_DIRECTORIES_

    IDE_TEST( smiGetTableColumns( gQcmDirectories,
                                  QCM_DIRECTORIES_USER_ID,
                                  (const smiColumn**)&sColumn )
              != IDE_SUCCESS );

    makeMetaRangeSingleColumn(&sRangeColumn,
                              (const mtcColumn*)sColumn,
                              (const void *) &(aUserID),
                              &sRange);

    IDE_TEST(sCursor.open(
                 QC_SMI_STMT( aStatement ),
                 gQcmDirectories,
                 gQcmDirectoriesIndex[QCM_DIRECTORIES_USERID_IDX_ORDER],
                 smiGetRowSCN(gQcmDirectories),
                 NULL,
                 &sRange,
                 smiGetDefaultKeyRange(),
                 smiGetDefaultFilter(),
                 QCM_META_CURSOR_FLAG,
                 SMI_SELECT_CURSOR,
                 & sCursorProperty) != IDE_SUCCESS);
    sStage = 1;
    IDE_TEST(sCursor.beforeFirst() != IDE_SUCCESS);
    IDE_TEST(sCursor.readRow(&sRow, &sRid, SMI_FIND_NEXT) != IDE_SUCCESS);
    if (sRow != NULL)
    {
        *aIsTrue = ID_TRUE;
        sStage = 0;
        IDE_TEST(sCursor.close() != IDE_SUCCESS);
        IDE_CONT(OBJECT_EXIST);
    }
    else
    {
        /* Nothing to do */
    }

    sStage = 0;

    IDE_TEST(sCursor.close() != IDE_SUCCESS);

    // PROJ-1685
    IDE_TEST( smiGetTableColumns( gQcmLibraries,
                                  QCM_LIBRARIES_USER_ID_ORDER,
                                  (const smiColumn**)&sColumn )
              != IDE_SUCCESS );

    makeMetaRangeSingleColumn(&sRangeColumn,
                              (const mtcColumn*)sColumn,
                              (const void *) &(aUserID),
                              &sRange);

    IDE_TEST(sCursor.open(
                 QC_SMI_STMT( aStatement ),
                 gQcmLibraries,
                 gQcmLibrariesIndex[QCM_LIBRARIES_IDX_USERID],
                 smiGetRowSCN(gQcmLibraries),
                 NULL,
                 &sRange,
                 smiGetDefaultKeyRange(),
                 smiGetDefaultFilter(),
                 QCM_META_CURSOR_FLAG,
                 SMI_SELECT_CURSOR,
                 & sCursorProperty) != IDE_SUCCESS);
    sStage = 1;
    IDE_TEST(sCursor.beforeFirst() != IDE_SUCCESS);
    IDE_TEST(sCursor.readRow(&sRow, &sRid, SMI_FIND_NEXT) != IDE_SUCCESS);
    if (sRow != NULL)
    {
        *aIsTrue = ID_TRUE;
        sStage = 0;
        IDE_TEST(sCursor.close() != IDE_SUCCESS);
        IDE_CONT(OBJECT_EXIST);
    }
    else
    {
        /* Nothing to do */
    }

    // ���� ���� ������ ��ü�� ���� ����
    *aIsTrue = ID_FALSE;
    sStage = 0;

    IDE_TEST(sCursor.close() != IDE_SUCCESS);

    // ��ü�� ã���� ����� �̵��Ѵ�.
    IDE_EXCEPTION_CONT(OBJECT_EXIST);
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    switch (sStage)
    {
        case 1:
            sCursor.close();
            break;
    }

    return IDE_FAILURE;
}

IDE_RC
qcm::touchTable( smiStatement     *  aSmiStmt ,
                 UInt                aTableID ,
                 smiTBSLockValidType aTBSLvType )
{
    const void * sTouchedTableHandle;
    smSCN        sSCN;

    IDE_TEST(getTableHandleByID(aSmiStmt,
                                aTableID,
                                (void **) &sTouchedTableHandle,
                                &sSCN,
                                NULL,
                                ID_TRUE)
             != IDE_SUCCESS);

    IDE_TEST(smiTable::touchTable( aSmiStmt,
                                   sTouchedTableHandle,
                                   aTBSLvType )
             != IDE_SUCCESS);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qcm::getParentKey( qcStatement   * aStatement,
                          qcmForeignKey * aForeignKey,
                          qcmParentInfo * aReferenceInfo)
{
    qcmIndex         * sParentIndex = NULL;
    qmsTableRef      * sTableRef;
    qmsIndexTableRef * sIndexTableRef;
    qcmIndex         * sIndexTableIndex;

    IDU_LIMITPOINT("qcm::getParentKey::malloc");
    IDE_TEST( QC_QMP_MEM( aStatement )->cralloc( ID_SIZEOF( qmsTableRef ),
                                                 (void**) &sTableRef )
              != IDE_SUCCESS );

    IDE_TEST( getTableInfoByID( aStatement,
                                aForeignKey->referencedTableID,
                                &sTableRef->tableInfo,
                                &sTableRef->tableSCN,
                                &sTableRef->tableHandle )
              != IDE_SUCCESS );
    
    // BUG-17119, 34492
    // �������ϹǷ� validation lock�̸� ����ϴ�.
    IDE_TEST( lockTableForDMLValidation( aStatement,
                                         sTableRef->tableHandle,
                                         sTableRef->tableSCN )
              != IDE_SUCCESS );

    // environment�� ���
    IDE_TEST( qcgPlan::registerPlanTable( aStatement,
                                          sTableRef->tableHandle,
                                          sTableRef->tableSCN,
                                          sTableRef->tableInfo->tableOwnerID, /* BUG-45893 */
                                          sTableRef->tableInfo->name )        /* BUG-45893 */
              != IDE_SUCCESS );

    IDE_TEST( qcmCache::getIndexByID( sTableRef->tableInfo,
                                      aForeignKey->referencedIndexID,
                                      &sParentIndex )
              != IDE_SUCCESS );

    aReferenceInfo->foreignKey     = aForeignKey;
    aReferenceInfo->parentIndex    = sParentIndex;
    aReferenceInfo->parentTableRef = sTableRef;

    // PROJ-1502 PARTITIONED DISK TABLE
    if( sTableRef->tableInfo->tablePartitionType == QCM_PARTITIONED_TABLE )
    {
        // PROJ-1624 non-partitioned index
        if ( sParentIndex->indexPartitionType == QCM_NONE_PARTITIONED_INDEX )
        {
            IDE_TEST( QC_QMP_MEM(aStatement)->alloc( ID_SIZEOF(qmsIndexTableRef),
                                                     (void **)&sIndexTableRef )
                      != IDE_SUCCESS );

            IDE_TEST( qmsIndexTable::makeOneIndexTableRef( aStatement,
                                                           sParentIndex,
                                                           sIndexTableRef )
                      != IDE_SUCCESS );

            IDE_TEST( qmsIndexTable::validateAndLockOneIndexTableRef( aStatement,
                                                                      sIndexTableRef,
                                                                      SMI_TABLE_LOCK_IS )
                      != IDE_SUCCESS );

            // environment�� ���
            IDE_TEST( qcgPlan::registerPlanTable( aStatement,
                                                  sIndexTableRef->tableHandle,
                                                  sIndexTableRef->tableSCN,
                                                  sIndexTableRef->tableInfo->tableOwnerID, /* BUG-45893 */
                                                  sIndexTableRef->tableInfo->name )        /* BUG-45893 */
                      != IDE_SUCCESS );

            // key index�� ã�´�.
            IDE_TEST( qmsIndexTable::findKeyIndex( sIndexTableRef->tableInfo,
                                                   & sIndexTableIndex )
                      != IDE_SUCCESS );

            aReferenceInfo->parentIndexTable      = sIndexTableRef;
            aReferenceInfo->parentIndexTableIndex = sIndexTableIndex;
        }
        else
        {
            sTableRef->flag &= ~QMS_TABLE_REF_PRE_PRUNING_MASK;
            sTableRef->flag |= QMS_TABLE_REF_PRE_PRUNING_FALSE;

            IDE_TEST( qmoPartition::makePartitions( aStatement,
                                                    sTableRef )
                      != IDE_SUCCESS );

            IDE_TEST( qcmPartition::validateAndLockPartitions(
                          aStatement,
                          sTableRef,
                          SMI_TABLE_LOCK_IS )
                      != IDE_SUCCESS );

            /* PROJ-2464 hybrid partitioned table ���� */
            IDE_TEST( qcmPartition::makePartitionSummary( aStatement, sTableRef )
                      != IDE_SUCCESS );

            aReferenceInfo->parentIndexTable      = NULL;
            aReferenceInfo->parentIndexTableIndex = NULL;
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qcm::getChildKeys( qcStatement      * aStatement,
                          qcmIndex         * aUniqueIndex,
                          qcmTableInfo     * aTableInfo,
                          qcmRefChildInfo ** aChildInfo )
{
/***********************************************************************
 *
 * Description :
 *    childKey �� ���Ѵ�.
 *
 * Implementation :
 *    1. SYS_CONSTRAINTS_ ���̺���
 *       REFERENCED_TABLE_ID, REFERENCED_CONSTRAINT_ID,
 *       TABLE_ID �÷��� ���� �д�.
 *    2. ��õ� tableID, indexID �� keyRange �� �����
 *       (��õ� ���̺�,�ε�����
 *        �����ϴ� child ���̺��� ���ϰ��� �Ѵ�)
 *    3. �� row �� �����鼭 TABLE_ID �κ��� ���̺� �ڵ��� ���Ѵ�.
 *    4. 3���� ���� child ���̺��� foreignkey ���� �߿��� ��õ� indexID ��
 *       �� �����ϴ� key �� �ִ��� üũ�Ѵ�.
 *    5. 4���� ���� foreignkey �� aChildInfo ����Ʈ�� ��ȯ�Ѵ�.
 *
 ***********************************************************************/

    qtcMetaRangeColumn      sFirstRangeColumn;
    qtcMetaRangeColumn      sSecondRangeColumn;
    qcmTableInfo          * sChildTableInfo;
    mtcColumn             * sTableIDCol;
    mtcColumn             * sReferencedTableIDCol;
    mtcColumn             * sReferencingTableIDCol;
    mtcColumn             * sReferencedIndexIDCol;
    mtcColumn             * sConstraintIDCol;
    smiRange                sRange;
    smiTableCursor          sCursor;

    // BUG-28049
    qcmForeignKey         * sForeignKey;
    qcmRefChildInfo       * sForeignKeyInfo;

    qcmRefChildInfo       * sPrevForeignKeyInfo;
    UInt                    sStage = 0;
    UInt                    sTableID;
    UInt                    sReferencingTableID;
    UInt                    sConstraintID;
    UInt                    i;
    const void            * sRow;
    smiCursorProperties     sCursorProperty;
    scGRID                  sRid; // Disk Table�� ���� Record IDentifier

    qmsTableRef           * sChildTableRef;
    qdConstraintSpec      * sConstr;
    UInt                    sTableType;
    qmsFrom                 sFrom;
    qmsTableRef           * sDefaultTableRef;
    UInt                    sDefaultColCount;
    UInt                    sUpdateColCount;
    UInt                  * sUpdateColumnID;
    qcmColumn             * sUpdateColumn;
    smiColumnList         * sUpdateColumnList;
    qcmColumn             * sQcmColumn;
    idBool                  sIsIntersect;
    qcmRefChildUpdateType   sUpdateRowMovement;
    qmsPartitionRef   *     sPartitionRef;

    *aChildInfo = NULL;

    sCursor.initialize();

    IDE_TEST( smiGetTableColumns( gQcmConstraints,
                                  QCM_CONSTRAINTS_TABLE_ID_COL_ORDER,
                                  (const smiColumn**)&sTableIDCol )
              != IDE_SUCCESS );
    IDE_TEST( smiGetTableColumns( gQcmConstraints,
                                  QCM_CONSTRAINTS_REFERENCED_TABLE_ID_COL_ORDER,
                                  (const smiColumn**)&sReferencedTableIDCol )
              != IDE_SUCCESS );
    IDE_TEST( smiGetTableColumns( gQcmConstraints,
                                  QCM_CONSTRAINTS_REFERENCED_CONSTRAINT_ID_COL_ORDER,
                                  (const smiColumn**)&sReferencedIndexIDCol )
              != IDE_SUCCESS );
    IDE_TEST( smiGetTableColumns( gQcmConstraints,
                                  QCM_CONSTRAINTS_TABLE_ID_COL_ORDER,
                                  (const smiColumn**)&sReferencingTableIDCol )
              != IDE_SUCCESS );
    IDE_TEST( smiGetTableColumns( gQcmConstraints,
                                  QCM_CONSTRAINTS_CONSTRAINT_ID_COL_ORDER,
                                  (const smiColumn**)&sConstraintIDCol )
              != IDE_SUCCESS );

    makeMetaRangeDoubleColumn(
        &sFirstRangeColumn,
        &sSecondRangeColumn,
        (const mtcColumn*) sReferencedTableIDCol,
        (const void *) & (aTableInfo->tableID),
        (const mtcColumn*) sReferencedIndexIDCol,
        (const void *) & (aUniqueIndex->indexId),
        &sRange);

    SMI_CURSOR_PROP_INIT_FOR_META_INDEX_SCAN(&sCursorProperty, aStatement->mStatistics);

    IDE_TEST(sCursor.open(
                 QC_SMI_STMT( aStatement ),
                 gQcmConstraints,
                 gQcmConstraintsIndex
                 [QCM_CONSTRAINTS_REFTABLEID_REFINDEXID_IDX_ORDER],
                 smiGetRowSCN(gQcmConstraints),
                 NULL,
                 &sRange,
                 smiGetDefaultKeyRange(),
                 smiGetDefaultFilter(),
                 QCM_META_CURSOR_FLAG,
                 SMI_SELECT_CURSOR,
                 & sCursorProperty) != IDE_SUCCESS);

    sStage = 1;
    IDE_TEST(sCursor.beforeFirst() != IDE_SUCCESS);

    IDE_TEST(sCursor.readRow(&sRow, &sRid, SMI_FIND_NEXT) != IDE_SUCCESS);

    if (sRow == NULL)
    {
        // Nothing to do.
    }
    else
    {
        // BUG-28049
        IDU_LIMITPOINT("qcm::getChildKeys::malloc1");
        IDE_TEST(QC_QMP_MEM(aStatement)->alloc(ID_SIZEOF(qcmRefChildInfo),
                                               (void**)aChildInfo)
                 != IDE_SUCCESS);

        // BUG-28049
        (*aChildInfo)->next = NULL;
        sPrevForeignKeyInfo = NULL;

        while (sRow != NULL)
        {
            sTableID =  *(UInt *)(
                (SChar *) sRow + sTableIDCol->column.offset);

            sReferencingTableID =  *(UInt *)(
                (SChar *) sRow + sReferencingTableIDCol->column.offset);

            // BUG-23145
            sConstraintID = *(UInt *)(
                (SChar *) sRow + sConstraintIDCol->column.offset);

            IDU_LIMITPOINT("qcm::getChildKeys::malloc2");
            IDE_TEST(QC_QMP_MEM(aStatement)->cralloc(ID_SIZEOF(qmsTableRef),
                                                     (void**)&sChildTableRef)
                     != IDE_SUCCESS);

            IDE_TEST(getTableHandleByID(
                         QC_SMI_STMT( aStatement ),
                         sReferencingTableID,
                         (void **)&sChildTableRef->tableHandle,
                         &sChildTableRef->tableSCN)
                     != IDE_SUCCESS);

            // sChildTableInfo�� �����ϱ� ���� IS LOCK�� ��´�.
            // DML�� DDL�� Validation���� ȣ���Ѵ�.
            IDE_TEST( qcm::validateAndLockTable( aStatement,
                                                 sChildTableRef->tableHandle,
                                                 sChildTableRef->tableSCN,
                                                 SMI_TABLE_LOCK_IS )
                      != IDE_SUCCESS );

            IDE_TEST (smiGetTableTempInfo( sChildTableRef->tableHandle,
                                           (void**)&sChildTableInfo )
                      != IDE_SUCCESS );

            sChildTableRef->tableInfo = sChildTableInfo;

            // BUG-21816
            // environment�� ���
            IDE_TEST( qcgPlan::registerPlanTable( aStatement,
                                                  sChildTableRef->tableHandle,
                                                  sChildTableRef->tableSCN,
                                                  sChildTableRef->tableInfo->tableOwnerID, /* BUG-45893 */
                                                  sChildTableRef->tableInfo->name )        /* BUG-45893 */
                      != IDE_SUCCESS );

            sTableType = sChildTableInfo->tableFlag & SMI_TABLE_TYPE_MASK;

            // add tuple set
            IDE_TEST( qtc::nextTable( &(sChildTableRef->table),
                                      aStatement,
                                      sChildTableInfo,
                                      QCM_TABLE_TYPE_IS_DISK( sChildTableInfo->tableFlag ),
                                      MTC_COLUMN_NOTNULL_TRUE ) // PR-13597
                      != IDE_SUCCESS );

            // PROJ-1502 PARTITIONED DISK TABLE
            if( sChildTableInfo->tablePartitionType == QCM_PARTITIONED_TABLE )
            {
                QC_SHARED_TMPLATE(aStatement)->tmplate.rows[sChildTableRef->table].lflag
                    &= ~MTC_TUPLE_PARTITIONED_TABLE_MASK;
                QC_SHARED_TMPLATE(aStatement)->tmplate.rows[sChildTableRef->table].lflag
                    |= MTC_TUPLE_PARTITIONED_TABLE_TRUE;
            }
            else
            {
                // Nothing to do.
            }

            // PROJ-1502 PARTITIONED DISK TABLE
            if( sChildTableInfo->tablePartitionType == QCM_PARTITIONED_TABLE )
            {
                sChildTableRef->flag &= ~QMS_TABLE_REF_PRE_PRUNING_MASK;
                sChildTableRef->flag |= QMS_TABLE_REF_PRE_PRUNING_FALSE;

                IDE_TEST( qmoPartition::makePartitions( aStatement,
                                                        sChildTableRef )
                          != IDE_SUCCESS );

                IDE_TEST( qcmPartition::validateAndLockPartitions( aStatement,
                                                                   sChildTableRef,
                                                                   SMI_TABLE_LOCK_IS )
                          != IDE_SUCCESS );

                /* PROJ-2464 hybrid partitioned table ���� */
                IDE_TEST( qcmPartition::makePartitionSummary( aStatement, sChildTableRef )
                          != IDE_SUCCESS );

                /* PROJ-2464 hybrid partitioned table ���� */
                if ( sChildTableRef->partitionSummary->isHybridPartitionedTable == ID_TRUE )
                {
                    QC_SHARED_TMPLATE(aStatement)->tmplate.rows[sChildTableRef->table].lflag
                            &= ~MTC_TUPLE_HYBRID_PARTITIONED_TABLE_MASK;
                    QC_SHARED_TMPLATE(aStatement)->tmplate.rows[sChildTableRef->table].lflag
                            |= MTC_TUPLE_HYBRID_PARTITIONED_TABLE_TRUE;
                }
                else
                {
                    /* Nothing to do */
                }

                for ( sPartitionRef = sChildTableRef->partitionRef;
                      sPartitionRef != NULL;
                      sPartitionRef = sPartitionRef->next )
                {
                    IDE_TEST( qtc::nextTable( &(sPartitionRef->table),
                                              aStatement,
                                              sPartitionRef->partitionInfo,
                                              QCM_TABLE_TYPE_IS_DISK(
                                                  sPartitionRef->partitionInfo->tableFlag ),
                                  MTC_COLUMN_NOTNULL_TRUE ) // PR-13597
                              != IDE_SUCCESS );
                }

                // PROJ-1624 non-partitioned index
                IDE_TEST( qmsIndexTable::makeIndexTableRef(
                              aStatement,
                              sChildTableInfo,
                              & sChildTableRef->indexTableRef,
                              & sChildTableRef->indexTableCount )
                          != IDE_SUCCESS );

                IDE_TEST( qmsIndexTable::validateAndLockIndexTableRefList(
                              aStatement,
                              sChildTableRef->indexTableRef,
                              SMI_TABLE_LOCK_IS )
                          != IDE_SUCCESS );
            }
            else
            {
                // Nothing to do.
            }

            sForeignKey = NULL;

            // Find foreign Key node.
            for ( i = 0; i < sChildTableInfo->foreignKeyCount; i++)
            {
                // BUG-23145
                // foreign key �߿� ���� sRow�� ����Ű�� foreign key�� ���;��ϱ� ������
                // sConstraintID�� ���Ѵ�.
                // (constraintID�� unique�ϹǷ� indexID�� ������ �ʾƵ� ��)
                if ( sChildTableInfo->foreignKeys[i].constraintID == sConstraintID )
                {
                    sForeignKey = &(sChildTableInfo->foreignKeys[i]);
                    break;
                }
            }

            // if not found raise error.
            IDE_TEST_RAISE(sForeignKey == NULL, ERR_META_CRASH);

            // make linked list.
            // BUG-28049
            if (sPrevForeignKeyInfo == NULL)
            {
                sForeignKeyInfo       = *aChildInfo;
                sForeignKeyInfo->next = NULL;
                sPrevForeignKeyInfo   = sForeignKeyInfo;
            }
            else
            {
                IDU_LIMITPOINT("qcm::getChildKeys::malloc3");
                IDE_TEST(QC_QMP_MEM(aStatement)->alloc(
                             ID_SIZEOF(qcmRefChildInfo),
                             (void**)&sForeignKeyInfo)
                         != IDE_SUCCESS);

                sForeignKeyInfo->next     = NULL;
                sPrevForeignKeyInfo->next = sForeignKeyInfo;
                sPrevForeignKeyInfo       = sForeignKeyInfo;
            }

            // PROJ-2212 foreign key on delete set null
            if ( sTableID == aTableInfo->tableID )
            {
                sForeignKeyInfo->isSelfRef = ID_TRUE;
            }
            else
            {
                sForeignKeyInfo->isSelfRef = ID_FALSE;
            }

            sForeignKeyInfo->childCheckConstrList   = NULL;
            sForeignKeyInfo->defaultTableRef        = NULL;
            sForeignKeyInfo->defaultExprColumns     = NULL;
            sForeignKeyInfo->defaultExprBaseColumns = NULL;

            sForeignKeyInfo->updateColCount   = 0;
            sForeignKeyInfo->updateColumn     = NULL;
            sForeignKeyInfo->updateColumnID   = NULL;
            sForeignKeyInfo->updateColumnList = NULL;

            if ( (sForeignKey->referenceRule & QD_FOREIGN_OPTION_MASK)
                 == (QD_FOREIGN_DELETE_SET_NULL & QD_FOREIGN_OPTION_MASK) )
            {
                // BUG-34440
                // on delete set null�̸� child table�� check constraint�� �ִ� ���
                // child table�� check condition �˻縦 ����
                // 1. tuple�� �Ҵ��ϰ�
                // 2. check condition�� validation �Ѵ�.
                if ( sChildTableInfo->checkCount > 0 )
                {
                    for ( i = 0; i < sForeignKey->constraintColumnCount; i++ )
                    {
                        IDE_TEST( qdnCheck::addCheckConstrSpecRelatedToColumn(
                                      aStatement,
                                      &(sForeignKeyInfo->childCheckConstrList),
                                      sChildTableInfo->checks,
                                      sChildTableInfo->checkCount,
                                      sForeignKey->referencingColumn[i] )
                                  != IDE_SUCCESS );
                    }

                    /* PROJ-1107 Check Constraint ���� */
                    IDE_TEST( qdnCheck::setMtcColumnToCheckConstrList(
                                  aStatement,
                                  sChildTableInfo,
                                  sForeignKeyInfo->childCheckConstrList )
                              != IDE_SUCCESS );

                    QCP_SET_INIT_QMS_FROM( (&sFrom) );
                    sFrom.tableRef = sChildTableRef;

                    // validate check condition
                    for ( sConstr = sForeignKeyInfo->childCheckConstrList;
                          sConstr != NULL;
                          sConstr = sConstr->next )
                    {
                        IDE_TEST( qdbCommon::validateCheckConstrDefinition(
                                      aStatement,
                                      sConstr,
                                      NULL,
                                      &sFrom )
                                  != IDE_SUCCESS );
                    }
                }
                else
                {
                    // Nothing to do.
                }

                /* PROJ-1090 Function-based Index */
                /* default expression column list ���� */
                for ( i = 0; i < sForeignKey->constraintColumnCount; i++ )
                {
                    IDE_TEST( qmsDefaultExpr::addDefaultExpressionColumnsRelatedToColumn(
                                  aStatement,
                                  &(sForeignKeyInfo->defaultExprColumns),
                                  sChildTableInfo,
                                  sForeignKey->referencingColumn[i] )
                              != IDE_SUCCESS );
                }

                sDefaultColCount = 0;

                if ( sForeignKeyInfo->defaultExprColumns != NULL )
                {
                    IDE_TEST( QC_QMP_MEM(aStatement)->alloc(
                                  ID_SIZEOF(qmsTableRef),
                                  (void**)&sDefaultTableRef )
                              != IDE_SUCCESS );

                    idlOS::memcpy( (void*) sDefaultTableRef,
                                   (void*) sChildTableRef,
                                   ID_SIZEOF(qmsTableRef) );

                    // add tuple set
                    IDE_TEST( qtc::nextTable( &(sDefaultTableRef->table),
                                              aStatement,
                                              sChildTableInfo,
                                              QCM_TABLE_TYPE_IS_DISK( sChildTableInfo->tableFlag ),
                                              MTC_COLUMN_NOTNULL_TRUE ) // PR-13597
                              != IDE_SUCCESS );

                    // PROJ-1502 PARTITIONED DISK TABLE
                    if( sChildTableInfo->tablePartitionType == QCM_PARTITIONED_TABLE )
                    {
                        QC_SHARED_TMPLATE(aStatement)->tmplate.rows[sDefaultTableRef->table].lflag
                            &= ~MTC_TUPLE_PARTITIONED_TABLE_MASK;
                        QC_SHARED_TMPLATE(aStatement)->tmplate.rows[sDefaultTableRef->table].lflag
                            |= MTC_TUPLE_PARTITIONED_TABLE_TRUE;
                    }
                    else
                    {
                        // Nothing to do.
                    }

                    /* Memory Table�̸�, Variable Column�� Fixed Column���� ��ȯ�� TableRef�� �����. */
                    if ( ( sTableType == SMI_TABLE_MEMORY ) ||
                         ( sTableType == SMI_TABLE_VOLATILE ) )
                    {
                        IDE_TEST( qmvQuerySet::makeTupleForInlineView(
                                      aStatement,
                                      sDefaultTableRef,
                                      sDefaultTableRef->table,
                                      MTC_COLUMN_NOTNULL_TRUE )
                                  != IDE_SUCCESS );
                    }
                    else
                    {
                        /* Disk Table�� Row Buffer���� Variable Column�� ����. */
                    }

                    QCP_SET_INIT_QMS_FROM( (&sFrom) );
                    sFrom.tableRef = sDefaultTableRef;

                    for ( sQcmColumn = sForeignKeyInfo->defaultExprColumns;
                          sQcmColumn != NULL;
                          sQcmColumn = sQcmColumn->next )
                    {
                        IDE_TEST( qdbCommon::validateDefaultExprDefinition(
                                      aStatement,
                                      sQcmColumn->defaultValue,
                                      NULL,
                                      &sFrom )
                                  != IDE_SUCCESS );

                        sDefaultColCount++;
                    }

                    // make base columns
                    IDE_TEST( qmsDefaultExpr::makeBaseColumn(
                                  aStatement,
                                  sChildTableInfo,
                                  sForeignKeyInfo->defaultExprColumns,
                                  &(sForeignKeyInfo->defaultExprBaseColumns) )
                              != IDE_SUCCESS );

                    sForeignKeyInfo->defaultTableRef = sDefaultTableRef;
                }
                else
                {
                    // Nothing to do.
                }

                sUpdateColCount = sForeignKey->constraintColumnCount + sDefaultColCount;

                IDE_TEST( QC_QMP_MEM(aStatement)->alloc(
                              ID_SIZEOF(qcmColumn) * sUpdateColCount,
                              (void**)& sUpdateColumn )
                          != IDE_SUCCESS );

                IDE_TEST( QC_QMP_MEM(aStatement)->alloc(
                              ID_SIZEOF(UInt) * sUpdateColCount,
                              (void**)& sUpdateColumnID )
                          != IDE_SUCCESS );

                IDE_TEST( QC_QMP_MEM(aStatement)->alloc(
                              ID_SIZEOF(smiColumnList) * sUpdateColCount,
                              (void**)& sUpdateColumnList )
                          != IDE_SUCCESS );

                // make update column list
                for ( i = 0; i < sForeignKey->constraintColumnCount; i++ )
                {
                    IDE_TEST( qcmCache::getColumnByID( sChildTableInfo,
                                                       sForeignKey->referencingColumn[i],
                                                       &sQcmColumn )
                              != IDE_SUCCESS );

                    idlOS::memcpy( &(sUpdateColumn[i]),
                                   sQcmColumn,
                                   ID_SIZEOF(qcmColumn) );
                }

                for ( sQcmColumn = sForeignKeyInfo->defaultExprColumns;
                      sQcmColumn != NULL;
                      sQcmColumn = sQcmColumn->next, i++ )
                {
                    idlOS::memcpy( &(sUpdateColumn[i]),
                                   sQcmColumn,
                                   ID_SIZEOF(qcmColumn) );
                }

                /* Column ID ������� �����ϸ�, DISK�� ��쿡 ���� ����� �ִ�. */
                if ( sUpdateColCount > 1 )
                {
                    idlOS::qsort( sUpdateColumn,
                                  sUpdateColCount,
                                  ID_SIZEOF(qcmColumn),
                                  compareQcmColumn );
                }
                else
                {
                    // Nothing To Do
                }

                // make update column list
                for ( i = 0; i < sUpdateColCount; i++ )
                {
                    sUpdateColumnID[i] = sUpdateColumn[i].basicInfo->column.id;
                    sUpdateColumnList[i].column = (smiColumn*) sUpdateColumn[i].basicInfo;
                }

                // link
                for ( i = 0; i < sUpdateColCount; i++ )
                {
                    sUpdateColumn[i].next = &(sUpdateColumn[i + 1]);
                    sUpdateColumnList[i].next = &(sUpdateColumnList[i + 1]);
                }
                sUpdateColumn[sUpdateColCount - 1].next = NULL;
                sUpdateColumnList[sUpdateColCount - 1].next = NULL;

                // BUG-35379 on delete set null�� row movement�� ����ؾ���
                sUpdateRowMovement = QCM_CHILD_UPDATE_NORMAL;

                if( sChildTableInfo->tablePartitionType == QCM_PARTITIONED_TABLE )
                {
                    IDE_TEST( qmoPartition::isIntersectPartKeyColumn(
                                  sChildTableInfo->partKeyColumns,
                                  sUpdateColumn,
                                  & sIsIntersect )
                              != IDE_SUCCESS );

                    if ( sIsIntersect == ID_TRUE )
                    {
                        if ( sChildTableInfo->rowMovement == ID_TRUE )
                        {
                            // row movement update ���ɼ� ����
                            sUpdateRowMovement = QCM_CHILD_UPDATE_ROWMOVEMENT;
                        }
                        else
                        {
                            // row movement�� �߻��ϸ� ����
                            sUpdateRowMovement = QCM_CHILD_UPDATE_CHECK_ROWMOVEMENT;
                        }
                    }
                    else
                    {
                        sUpdateRowMovement = QCM_CHILD_UPDATE_NO_ROWMOVEMENT;
                    }
                }
                else
                {
                    // Nothing to do.
                }

                // set
                sForeignKeyInfo->updateColCount    = sUpdateColCount;
                sForeignKeyInfo->updateColumnID    = sUpdateColumnID;
                sForeignKeyInfo->updateColumn      = sUpdateColumn;
                sForeignKeyInfo->updateColumnList  = sUpdateColumnList;
                sForeignKeyInfo->updateRowMovement = sUpdateRowMovement;
            }
            else
            {
                // Nothing to do.
            }

            sForeignKeyInfo->parentTableID    = aTableInfo->tableID;
            sForeignKeyInfo->parentIndex      = aUniqueIndex;
            sForeignKeyInfo->isCycle          = ID_FALSE;
            sForeignKeyInfo->foreignKey       = sForeignKey;
            sForeignKeyInfo->referenceRule    = sForeignKey->referenceRule;
            sForeignKeyInfo->childTableRef    = sChildTableRef;

            IDE_TEST( sCursor.readRow(&sRow, &sRid, SMI_FIND_NEXT)
                      != IDE_SUCCESS );
        }
    }

    sStage = 0;
    IDE_TEST(sCursor.close() != IDE_SUCCESS);

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_META_CRASH);
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QCM_META_CRASH));
    }
    IDE_EXCEPTION_END;

    switch(sStage)
    {
        case 1:
            sCursor.close();
    }

    return IDE_FAILURE;
}

IDE_RC qcm::getChildKeysForDelete( qcStatement       * aStatement,
                                   UInt                aReferencingUserID,
                                   qcmIndex          * aUniqueIndex,
                                   qcmTableInfo      * aTableInfo,
                                   idBool              aDropTablespace,
                                   qcmRefChildInfo  ** aChildInfo )
{
/***********************************************************************
 *
 * Description :
 *    childKey �� ���Ѵ�.
 *
 *    TASK-2176
 *    validation�� ȣ��Ǵ� �Լ��� execution�� ȣ��Ǵ� �Լ� �и�
 *    getChildKeys�� DML,DDL�� validation�� ȣ��ǰ�,
 *    getChildKeysForDelete�� DDL�� execution�� ȣ��ȴ�.
 *
 * Implementation :
 *    1. SYS_CONSTRAINTS_ ���̺���
 *       REFERENCED_TABLE_ID, REFERENCED_CONSTRAINT_ID,
 *       TABLE_ID �÷��� ���� �д�.
 *    2. ��õ� tableID, indexID �� keyRange �� �����
 *       (��õ� ���̺�,�ε�����
 *        �����ϴ� child ���̺��� ���ϰ��� �Ѵ�)
 *    3. �� row �� �����鼭 TABLE_ID �κ��� ���̺� �ڵ��� ���Ѵ�.
 *    4. 3���� ���� child ���̺��� foreignkey ���� �߿��� ��õ� indexID ��
 *       �� �����ϴ� key �� �ִ��� üũ�Ѵ�.
 *    5. 4���� ���� foreignkey �� aChildInfo ����Ʈ�� ��ȯ�Ѵ�.
 *
 ***********************************************************************/

    qtcMetaRangeColumn   sFirstRangeColumn;
    qtcMetaRangeColumn   sSecondRangeColumn;
    qcmTableInfo       * sChildTableInfo;
    mtcColumn          * sReferencedTableIDCol;
    mtcColumn          * sReferencingTableIDCol;
    mtcColumn          * sReferencingUserIDCol;
    mtcColumn          * sReferencedIndexIDCol;
    mtcColumn          * sConstraintIDCol;
    smiRange             sRange;
    smiTableCursor       sCursor;
    qcmForeignKey      * sForeignKey;

    // BUG-28049
    qcmRefChildInfo    * sForeignKeyInfo;
    qcmRefChildInfo    * sPrevForeignKeyInfo;

    qcmIndex           * sIndexInfo;
    UInt                 sStage = 0;
    UInt                 sReferencingTableID;
    UInt                 sReferencingUserID;
    UInt                 sReferencedIndexID;
    UInt                 sConstraintID;
    UInt                 i;
    const void         * sRow;
    smiCursorProperties  sCursorProperty;
    scGRID               sRid; // Disk Table�� ���� Record IDentifier

    qmsTableRef        * sChildTableRef;
    qmsPartitionRef    * sPartitionRef = NULL;

    smiTBSLockValidType  sTBSLockValidType = SMI_TBSLV_DDL_DML;

    *aChildInfo = NULL;

    sCursor.initialize();

    /* BUG-43299 Drop Tablespace�� ���, SMI_TBSLV_DROP_TBS�� ����ؾ� �Ѵ�. */
    if ( aDropTablespace == ID_TRUE )
    {
        sTBSLockValidType = SMI_TBSLV_DROP_TBS;
    }
    else
    {
        /* Nothingn to do */
    }

    IDE_TEST( smiGetTableColumns( gQcmConstraints,
                                  QCM_CONSTRAINTS_REFERENCED_TABLE_ID_COL_ORDER,
                                  (const smiColumn**)&sReferencedTableIDCol )
              != IDE_SUCCESS );
    IDE_TEST( smiGetTableColumns( gQcmConstraints,
                                  QCM_CONSTRAINTS_REFERENCED_CONSTRAINT_ID_COL_ORDER,
                                  (const smiColumn**)&sReferencedIndexIDCol )
              != IDE_SUCCESS );
    IDE_TEST( smiGetTableColumns( gQcmConstraints,
                                  QCM_CONSTRAINTS_TABLE_ID_COL_ORDER,
                                  (const smiColumn**)&sReferencingTableIDCol )
              != IDE_SUCCESS );
    IDE_TEST( smiGetTableColumns( gQcmConstraints,
                                  QCM_CONSTRAINTS_USER_ID_COL_ORDER,
                                  (const smiColumn**)&sReferencingUserIDCol )
              != IDE_SUCCESS );
    IDE_TEST( smiGetTableColumns( gQcmConstraints,
                                  QCM_CONSTRAINTS_CONSTRAINT_ID_COL_ORDER,
                                  (const smiColumn**)&sConstraintIDCol )
              != IDE_SUCCESS );

    if( aUniqueIndex != NULL )
    {
        makeMetaRangeDoubleColumn(
            &sFirstRangeColumn,
            &sSecondRangeColumn,
            (const mtcColumn*) sReferencedTableIDCol,
            (const void *) & (aTableInfo->tableID),
            (const mtcColumn*) sReferencedIndexIDCol,
            (const void *) & (aUniqueIndex->indexId),
            &sRange);
    }
    else
    {
        makeMetaRangeSingleColumn(
            &sFirstRangeColumn,
            (const mtcColumn*) sReferencedTableIDCol,
            (const void *) & (aTableInfo->tableID),
            &sRange);
    }

    SMI_CURSOR_PROP_INIT_FOR_META_INDEX_SCAN(&sCursorProperty, aStatement->mStatistics);

    IDE_TEST(sCursor.open(
                 QC_SMI_STMT( aStatement ),
                 gQcmConstraints,
                 gQcmConstraintsIndex
                 [QCM_CONSTRAINTS_REFTABLEID_REFINDEXID_IDX_ORDER],
                 smiGetRowSCN(gQcmConstraints),
                 NULL,
                 &sRange,
                 smiGetDefaultKeyRange(),
                 smiGetDefaultFilter(),
                 QCM_META_CURSOR_FLAG,
                 SMI_SELECT_CURSOR,
                 & sCursorProperty) != IDE_SUCCESS);

    sStage = 1;
    IDE_TEST(sCursor.beforeFirst() != IDE_SUCCESS);

    IDE_TEST(sCursor.readRow(&sRow, &sRid, SMI_FIND_NEXT) != IDE_SUCCESS);

    if (sRow == NULL)
    {
        // Nothing to do.
    }
    else
    {
        sPrevForeignKeyInfo = NULL;

        while (sRow != NULL)
        {
            sReferencingUserID =  *(UInt *)(
                (SChar *) sRow + sReferencingUserIDCol->column.offset);

            sReferencingTableID =  *(UInt *)(
                (SChar *) sRow + sReferencingTableIDCol->column.offset);

            sReferencedIndexID =  *(UInt *)(
                (SChar *) sRow + sReferencedIndexIDCol->column.offset);

            sConstraintID =  *(UInt *)(
                (SChar *) sRow + sConstraintIDCol->column.offset);

            if( ( aReferencingUserID == QC_EMPTY_USER_ID ) ||
                ( sReferencingUserID == aReferencingUserID ) )
            {
                IDU_LIMITPOINT("qcm::getChildKeysForDelete::malloc2");
                IDE_TEST(aStatement->qmxMem->cralloc(ID_SIZEOF(qmsTableRef),
                                                     (void**)&sChildTableRef)
                         != IDE_SUCCESS);

                IDE_TEST( getTableInfoByID( aStatement,
                                            sReferencingTableID,
                                            &sChildTableInfo,
                                            &sChildTableRef->tableSCN,
                                            &sChildTableRef->tableHandle)
                          != IDE_SUCCESS);

                // sChildTableInfo�� �����ϱ� ���� LOCK�� ��´�.
                IDE_TEST( smiValidateAndLockObjects( (QC_SMI_STMT( aStatement ))->getTrans(),
                                                     sChildTableRef->tableHandle,
                                                     sChildTableRef->tableSCN,
                                                     sTBSLockValidType, // TBS Validation �ɼ�
                                                     SMI_TABLE_LOCK_X,
                                                     smiGetDDLLockTimeOut((QC_SMI_STMT(aStatement))->getTrans()),
                                                     ID_FALSE ) // BUG-28752 ����� Lock�� ������ Lock�� �����մϴ�.
                          != IDE_SUCCESS );

                sChildTableRef->tableInfo = sChildTableInfo;


                // PROJ-1502 PARTITIONED DISK TABLE
                if( sChildTableInfo->tablePartitionType == QCM_PARTITIONED_TABLE )
                {
                    sChildTableRef->flag &= ~QMS_TABLE_REF_PRE_PRUNING_MASK;
                    sChildTableRef->flag |= QMS_TABLE_REF_PRE_PRUNING_FALSE;

                    IDE_TEST( qmoPartition::makePartitions( aStatement,
                                                            sChildTableRef )
                              != IDE_SUCCESS );

                    for ( sPartitionRef = sChildTableRef->partitionRef;
                          sPartitionRef != NULL;
                          sPartitionRef = sPartitionRef->next )
                    {
                        IDE_TEST( qcmPartition::validateAndLockOnePartition( aStatement,
                                                                             sPartitionRef->partitionHandle,
                                                                             sPartitionRef->partitionSCN,
                                                                             sTBSLockValidType,
                                                                             SMI_TABLE_LOCK_X,
                                                                             smiGetDDLLockTimeOut((QC_SMI_STMT(aStatement))->getTrans()))
                                  != IDE_SUCCESS );
                    }

                    /* PROJ-2464 hybrid partitioned table ���� */
                    IDE_TEST( qcmPartition::makePartitionSummary( aStatement, sChildTableRef )
                              != IDE_SUCCESS );
                }

                sForeignKey = NULL;

                // Find foreign Key node.
                for ( i = 0; i < sChildTableInfo->foreignKeyCount; i++)
                {
                    // BUG-23145
                    // referenced index id������ �˻��� �� �� ����.
                    // T1(i1) -> T2(i1) / T1(i1) -> T2(i2)�� ���ÿ� ������ �� ����
                    // ���� constraintID�� �˻��ؾ� ��
                    if (sChildTableInfo->foreignKeys[i].constraintID
                        == sConstraintID)
                    {
                        sForeignKey = &(sChildTableInfo->foreignKeys[i]);
                        break;
                    }
                }

                // if not found raise error.
                IDE_TEST_RAISE(sForeignKey == NULL, ERR_META_CRASH);

                // make linked list.
                // BUG-28049
                if (sPrevForeignKeyInfo == NULL)
                {
                    for ( i = 0, sIndexInfo = NULL;
                          i < aTableInfo->indexCount;
                          i++ )
                    {
                        if ( aTableInfo->indices[i].indexId ==
                             sReferencedIndexID )
                        {
                            sIndexInfo = &aTableInfo->indices[i];
                            break;
                        }
                        else
                        {
                            // Nothing To Do!!
                        }
                    }

                    IDE_TEST_RAISE( sIndexInfo == NULL, ERR_META_CRASH );
                }

                // fix BUG-32876
                IDU_LIMITPOINT("qcm::getChildKeysForDelete::malloc3");
                IDE_TEST(QC_QMP_MEM(aStatement)->alloc(
                             ID_SIZEOF(qcmRefChildInfo),
                             (void**)&sForeignKeyInfo)
                         != IDE_SUCCESS);

                sForeignKeyInfo->next = NULL;

                if( *aChildInfo == NULL )
                {
                    *aChildInfo = sForeignKeyInfo;
                }
                else
                {
                    sPrevForeignKeyInfo->next = sForeignKeyInfo;
                }

                sPrevForeignKeyInfo = sForeignKeyInfo;

                sForeignKeyInfo->parentTableID        = aTableInfo->tableID;
                sForeignKeyInfo->parentIndex          = aUniqueIndex;
                sForeignKeyInfo->isSelfRef            = ID_FALSE;
                sForeignKeyInfo->isCycle              = ID_FALSE;
                sForeignKeyInfo->foreignKey           = sForeignKey;
                sForeignKeyInfo->referenceRule        = sForeignKey->referenceRule;
                sForeignKeyInfo->childTableRef        = sChildTableRef;
                sForeignKeyInfo->childCheckConstrList = NULL;
            }
            else
            {
                // do nothing
            }

            IDE_TEST(sCursor.readRow(&sRow, &sRid, SMI_FIND_NEXT) !=
                     IDE_SUCCESS);
        }
    }
    sStage = 0;
    IDE_TEST(sCursor.close() != IDE_SUCCESS);

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_META_CRASH);
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QCM_META_CRASH));
    }
    IDE_EXCEPTION_END;

    switch(sStage)
    {
        case 1:
            sCursor.close();
    }

    return IDE_FAILURE;
}

IDE_RC qcm::selectRow
(
    smiStatement            * aSmiStmt,
    const void              * aTable,
    smiCallBack             * aCallback,
    smiRange                * aRange,
    const void              * aIndex,
    /* function that sets members in aMetaStruct from aRow */
    qcmSetStructMemberFunc    aSetStructMemberFunc,
    /* output address space.
     * if NULL, only count tuples
     */
    void                    * aMetaStruct,        /* OUT */
    /* distance between structures in address space*/
    UInt                      aMetaStructDistance,
    /* maximum count of structures capable for aMetaStruct */
    /* o_mataStruct should have been allocated
     * aMetaStructDistance * aMetaStructMaxCount */
    UInt                      aMetaStructMaxCount,
    vSLong                  * aSelectedRowCount   /* OUT */
    )
{
/***********************************************************************
 *
 * Description :
 *    childKey �� ���Ѵ�.
 *
 * Implementation :
 *    1. ���̺� �÷� ����, �÷��� ���� ���� smiColumnList ���·� ����
 *    2. Ŀ�� ���� �� �� ���ڵ徿 �����鼭 count ���� => count ��ȯ
 *    3. ���ڿ� ��õ� meta ����ü ���� function �� ������ ����, ����ü ����
 *       �� �̿��ؼ� meta ����ü �� ������ ���� �����ϰ� �ȴ�.
 *
 *    PROJ-1705
 *    �� �Լ��� ��Ÿ���̺� ���� ó���̹Ƿ�
 *    fetch column list�� �������� �ʴ´�.
 *
 ***********************************************************************/

    smiTableCursor  sCursor;
    UInt            sStep = 0;
    const void *    sRow;

    UInt            i;
    UInt            sColumnCount;
    mtcColumn      *sColumn;
    smiColumnList   sColumnList[QCM_MAX_META_COLUMNS];
    scGRID          sRid; // Disk Table�� ���� Record IDentifier
    smiCursorProperties sCursorProperty;

    sCursor.initialize();

    sColumnCount = smiGetTableColumnCount(aTable);
    IDE_TEST( smiGetTableColumns( aTable,
                                  0,
                                  (const smiColumn**)&sColumn )
              != IDE_SUCCESS );


    IDE_TEST_RAISE( sColumnCount > QCM_MAX_META_COLUMNS,
                    err_too_many_meta_columns );


    for (i = 0; i < sColumnCount -1; i++)
    {

        IDE_TEST( smiGetTableColumns( aTable,
                                      i,
                                      (const smiColumn**)&sColumnList[i].column )
                  != IDE_SUCCESS );
        sColumnList[i].next   = &sColumnList[i+1];
    }

    IDE_TEST( smiGetTableColumns( aTable,
                                  sColumnCount-1,
                                  (const smiColumn**)&sColumnList[sColumnCount-1].column )
              != IDE_SUCCESS );
    sColumnList[sColumnCount-1].next = NULL;

    if ( aIndex != NULL )
    {
        SMI_CURSOR_PROP_INIT_FOR_META_INDEX_SCAN(&sCursorProperty, NULL);
    }
    else
    {
        SMI_CURSOR_PROP_INIT_FOR_META_FULL_SCAN(&sCursorProperty, NULL);
    }

    IDE_TEST( sCursor.open( aSmiStmt,
                            aTable,
                            aIndex, // index
                            smiGetRowSCN( aTable ),
                            sColumnList, // columns
                            aRange, // range
                            smiGetDefaultKeyRange(),
                            aCallback, // callback
                            QCM_META_CURSOR_FLAG,
                            SMI_SELECT_CURSOR,
                            &sCursorProperty)
              != IDE_SUCCESS );
    sStep = 1;

    *aSelectedRowCount = 0;

    IDE_TEST( sCursor.beforeFirst() != IDE_SUCCESS);

    IDE_TEST( sCursor.readRow( &sRow, &sRid, SMI_FIND_NEXT )
              != IDE_SUCCESS );

    while( sRow != NULL )
    {
        (*aSelectedRowCount) ++;

        if (aMetaStruct != NULL && aSetStructMemberFunc != NULL)
        {
            if ( (UInt) *aSelectedRowCount <= aMetaStructMaxCount )
            {
                // fill aMetaStruct with data using sRow and sColumn
                IDE_TEST( (*aSetStructMemberFunc) ( aSmiStmt->mStatistics,
                                                    sRow,
                                                    aMetaStruct )
                          != IDE_SUCCESS );
            }

            aMetaStruct = (void*) ( ((SChar*)aMetaStruct) +
                                    aMetaStructDistance );
        }

        IDE_TEST( sCursor.readRow( &sRow, &sRid, SMI_FIND_NEXT )
                  != IDE_SUCCESS );
    }

    sStep = 0;
    IDE_TEST( sCursor.close( ) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION(err_too_many_meta_columns);
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QCM_INTERNAL_ARG,
                                "[qcm::selectRow] err_too_many_meta_columns"));
    }
    IDE_EXCEPTION_END;

    switch (sStep)
    {
        case 1:
            (void) sCursor.close();
            /* fall through */
        default:
            break;
    }

    return IDE_FAILURE;
}

IDE_RC qcm::selectCount
(
    smiStatement        * aSmiStmt,
    const void          * aTable,
    vSLong              * aSelectedRowCount,  /* OUT */
    smiCallBack         * aCallback , // = NULL
    smiRange            * aRange    , // = NULL
    const void          * aIndex      // = NULL
    )
{
    return selectRow
        (
            aSmiStmt,
            aTable,
            aCallback,
            aRange,
            aIndex,
            NULL, /* aSetStructMemberFunc */
            NULL, /* aMetaStruct */
            0,    /* aMetaStructDistance */
            0,    /* aMetaStructMaxCount */
            aSelectedRowCount
            );
}

void qcm::getIntegerFieldValue( const void * aRow,
                                mtcColumn  * aColumn,
                                UInt       * aValue )
{
    mtdIntegerType sIntData;

    sIntData = *(mtdIntegerType*) ((UChar*) aRow + aColumn->column.offset );
    (*aValue) = (UInt) sIntData;
}


void qcm::getIntegerFieldValue( const void * aRow,
                                mtcColumn  * aColumn,
                                SInt       * aValue )
{
    mtdIntegerType sIntData;

    sIntData = *(mtdIntegerType*) ((UChar*) aRow + aColumn->column.offset );
    (*aValue) = (SInt) sIntData;
}

void qcm::getCharFieldValue( const void * aRow,
                             mtcColumn  * aColumn,
                             SChar      * aValue )
{
    mtdCharType  * sCharData;

    sCharData = (mtdCharType*)
        ((UChar*) aRow + aColumn->column.offset);
    idlOS::memcpy( aValue,
                   sCharData->value,
                   sCharData->length );
    aValue[ sCharData->length ] = '\0';
}

void qcm::getBigintFieldValue ( const void * aRow,
                                mtcColumn  * aColumn,
                                SLong      * aValue )
{
    mtdBigintType  sBigIntData;

    sBigIntData = *(mtdBigintType*)
        ((UChar*) aRow + aColumn->column.offset);
    *aValue =  (SLong) sBigIntData;
}

/*-----------------------------------------------------------------------------
 * Name :
 *    qcm::checkCascadeOption()
 *
 * Argument :
 *    aStatement - which have parse tree, iduMemory, session information.
 *    aExist     - existence of result set ( OUT )
 *
 * Description :
 *
 *     SELECT *
 *       FROM SYS_CONSTRAINTS_
 *      WHERE USER_ID = aUserId
 *        AND REFERENCED_TABLE_ID = aReferencedTableID;
 *
 *    If result set exists, then aExist = ID_TRUE
 *                          else aExist = ID_FALSE
 *
 *    ����� REFERENCED_TABLE_ID�� ������ CONSTRAINT�� FOREIGN KEY�ۿ� ����.
 * ---------------------------------------------------------------------------*/

IDE_RC qcm::checkCascadeOption( qcStatement * aStatement,
                                UInt          aUserId,
                                UInt          aReferencedTableID,
                                idBool      * aExist)
{
    smiRange               sRange;
    qtcMetaRangeColumn     sFirstMetaRange;
    qtcMetaRangeColumn     sSecondMetaRange;
    mtdIntegerType         sIntDataOfUserID;
    mtdIntegerType         sIntDataOfReferencedTableID;
    smiTableCursor         sCursor;
    SInt                   sStage = 0;
    const void           * sRow = NULL;
    smiCursorProperties    sCursorProperty;
    scGRID                 sRid; // Disk Table�� ���� Record IDentifier
    mtcColumn            * sFstMtcColumn;
    mtcColumn            * sSndMtcColumn;

    sCursor.initialize();

    // set data of user ID
    sIntDataOfUserID = (mtdIntegerType) aUserId;
    // set data of referenced table ID
    sIntDataOfReferencedTableID = (mtdIntegerType) aReferencedTableID;

    // make key range for index ( USER_ID, REFERENCED_TABLE_ID )
    IDE_TEST( smiGetTableColumns( gQcmConstraints
                                  ,QCM_CONSTRAINTS_USER_ID_COL_ORDER,
                                  (const smiColumn**)&sFstMtcColumn )
              != IDE_SUCCESS );

    IDE_TEST( smiGetTableColumns( gQcmConstraints,
                                  QCM_CONSTRAINTS_REFERENCED_TABLE_ID_COL_ORDER,
                                  (const smiColumn**)&sSndMtcColumn )
              != IDE_SUCCESS );
        
    qcm::makeMetaRangeDoubleColumn(
        & sFirstMetaRange,
        & sSecondMetaRange,
        sFstMtcColumn,
        & sIntDataOfUserID,
        sSndMtcColumn,
        & sIntDataOfReferencedTableID,
        & sRange );

    SMI_CURSOR_PROP_INIT_FOR_META_INDEX_SCAN(&sCursorProperty, aStatement->mStatistics);

    IDE_TEST( sCursor.open(
                  QC_SMI_STMT( aStatement ),
                  gQcmConstraints,
                  gQcmConstraintsIndex
                  [QCM_CONSTRAINTS_USERID_REFTABLEID_IDX_ORDER],
                  smiGetRowSCN( gQcmConstraints ),
                  NULL,
                  & sRange,
                  smiGetDefaultKeyRange(),
                  smiGetDefaultFilter(),
                  QCM_META_CURSOR_FLAG,
                  SMI_SELECT_CURSOR,
                  & sCursorProperty )
              != IDE_SUCCESS );
    sStage = 1;

    IDE_TEST( sCursor.beforeFirst() != IDE_SUCCESS );

    IDE_TEST( sCursor.readRow( & sRow, &sRid, SMI_FIND_NEXT ) != IDE_SUCCESS );

    if ( sRow != NULL )
    {
        *aExist = ID_TRUE;
    }
    else
    {
        *aExist = ID_FALSE;
    }

    sStage = 0;
    IDE_TEST( sCursor.close() != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if (sStage != 0)
    {
        sCursor.close();
    }

    return IDE_FAILURE;
}


/*-----------------------------------------------------------------------------
 * Name :
 *    qcm::getIsDefaultTBS()
 *
 * Argument :
 *
 * Description :
 *
 * ---------------------------------------------------------------------------*/
IDE_RC qcm::getIsDefaultTBS( qcStatement   * aStatement,
                             UInt            aTableID,
                             UInt            aColumnID,
                             idBool        * aIsDefaultTBS )
{

    qtcMetaRangeColumn   sFirstRangeColumn;
    qtcMetaRangeColumn   sSecondRangeColumn;
    mtcColumn          * sTableIdCol;
    mtcColumn          * sColumnIdCol;
    mtcColumn          * sIsDefaultTBSCol;
    smiRange             sRange;
    smiTableCursor       sCursor;
    const void         * sRow;
    smiCursorProperties  sCursorProperty;
    scGRID               sGRID;
    mtdCharType        * sIsDefaultTBSStr;
    UInt                 sStage = 0;

    sCursor.initialize();

    IDE_TEST( smiGetTableColumns( gQcmLobs,
                                  QCM_LOBS_TABLE_ID_COL_ORDER,
                                  (const smiColumn**)&sTableIdCol )
              != IDE_SUCCESS );
    IDE_TEST( smiGetTableColumns( gQcmLobs,
                                  QCM_LOBS_COLUMN_ID_COL_ORDER,
                                  (const smiColumn**)&sColumnIdCol )
              != IDE_SUCCESS );
    IDE_TEST( smiGetTableColumns( gQcmLobs,
                                  QCM_LOBS_IS_DEFAULT_TBS_COL_ORDER,
                                  (const smiColumn**)&sIsDefaultTBSCol )
              != IDE_SUCCESS );

    makeMetaRangeDoubleColumn( &sFirstRangeColumn,
                               &sSecondRangeColumn,
                               sTableIdCol,
                               &aTableID,
                               sColumnIdCol,
                               &aColumnID,
                               &sRange );

    SMI_CURSOR_PROP_INIT_FOR_META_INDEX_SCAN(&sCursorProperty, aStatement->mStatistics);

    IDE_TEST(sCursor.open(
                 QC_SMI_STMT(aStatement),
                 gQcmLobs,
                 gQcmLobsIndex[QCM_LOBS_TABLEID_COLUMNID_IDX_ORDER],
                 smiGetRowSCN(gQcmLobs),
                 NULL,
                 &sRange,
                 smiGetDefaultKeyRange(),
                 smiGetDefaultFilter(),
                 QCM_META_CURSOR_FLAG,
                 SMI_SELECT_CURSOR,
                 & sCursorProperty) != IDE_SUCCESS);

    sStage = 1;
    IDE_TEST(sCursor.beforeFirst() != IDE_SUCCESS);

    IDE_TEST(sCursor.readRow(&sRow, &sGRID, SMI_FIND_NEXT) != IDE_SUCCESS);

    IDE_TEST_RAISE( sRow == NULL, ERR_META_CRASH );

    sIsDefaultTBSStr = (mtdCharType *)
        ((UChar *)sRow + sIsDefaultTBSCol->column.offset);

    if ( idlOS::strMatch( (SChar *)&(sIsDefaultTBSStr->value),
                          sIsDefaultTBSStr->length,
                          "T",
                          1 ) == 0 )
    {
        *aIsDefaultTBS = ID_TRUE;
    }
    else
    {
        *aIsDefaultTBS = ID_FALSE;
    }

    sStage = 0;
    IDE_TEST(sCursor.close() != IDE_SUCCESS);

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_META_CRASH);
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QCM_META_CRASH));
    }
    IDE_EXCEPTION_END;

    if ( sStage == 1 )
    {
        (void)sCursor.close();
    }
    else
    {
        // Nothing to do.
    }

    return IDE_FAILURE;

}


/*-----------------------------------------------------------------------------
 * Name :
 *    qcm::copyQcmColumns()
 *
 * Argument :
 *    aMemory   - that allocates the memory for aTarget
 *    aSource   - source qcmColumn list
 *    aColCount - source qcmColumn list count
 *    aTarget   - target qcmColumn list
 *
 * Description :
 *
 *    Meta cache�� column���� ������ �����͸� �ٲ� ���� ����.
 *    qmx ��� meta cache�� column�� �����ϸ鼭
 *    basicInfo->column.value ���� ������ �ٲٱ� ���ؼ���
 *    �ݵ�� columns�� ������ �� ����ؾ� �Ѵ�.
 *    �׸��� qcmColumn ���� ������ ��쿡�� �ݵ�� mtcColumn ������(basicInfo)��
 *    ī���ؾ� �Ѵ�. (shallow copy�� �ƴ� deep copy���� �Ѵ�.)
 *
 * ---------------------------------------------------------------------------*/

IDE_RC qcm::copyQcmColumns( iduMemory  *  aMemory,
                            qcmColumn  *  aSource,
                            qcmColumn  ** aTarget,
                            UInt          aColCount )
{
    UInt       i;  // fix BUG-12105
    mtcColumn *sMtcColumns;
    qcmColumn *sTargetPosition;
    qcmColumn *sColumn;

    IDU_LIMITPOINT("qcm::copyQcmColumns::malloc1");
    IDE_TEST( aMemory->alloc( ID_SIZEOF(qcmColumn) * aColCount,
                              (void**)aTarget )
              != IDE_SUCCESS );

    IDU_LIMITPOINT("qcm::copyQcmColumns::malloc2");
    IDE_TEST( aMemory->alloc( ID_SIZEOF(mtcColumn) * aColCount,
                              (void**)&sMtcColumns )
              != IDE_SUCCESS );

    for( i = 0, sColumn = aSource;
         (i < aColCount) && (sColumn != NULL);
         i++, sColumn = sColumn->next )
    {
        idlOS::memcpy( *aTarget + i,
                       sColumn,
                       ID_SIZEOF(qcmColumn) );
    }

    for( i = 0, sColumn = aSource, sTargetPosition = *aTarget;
         (i < aColCount) && (sColumn != NULL);  // TASK-3876 Code Sonar
         i++, sMtcColumns++, sColumn = sColumn->next, sTargetPosition++ )
    {
        idlOS::memcpy( sMtcColumns, sColumn->basicInfo, ID_SIZEOF(mtcColumn) );

        sTargetPosition->basicInfo = sMtcColumns;
        sTargetPosition->next = sTargetPosition+1;
    }
    (sTargetPosition-1)->next = NULL;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qcm::copyQcmColumns( iduVarMemList  * aMemory,
                            qcmColumn      * aSource,
                            qcmColumn     ** aTarget,
                            UInt             aColCount )
{
    UInt       i;  // fix BUG-12105
    mtcColumn *sMtcColumns;
    qcmColumn *sTargetPosition;
    qcmColumn *sColumn;

    IDU_LIMITPOINT("qcm::copyQcmColumns::malloc3");
    IDE_TEST( aMemory->alloc( ID_SIZEOF(qcmColumn) * aColCount,
                              (void**)aTarget )
              != IDE_SUCCESS );

    IDU_LIMITPOINT("qcm::copyQcmColumns::malloc4");
    IDE_TEST( aMemory->alloc( ID_SIZEOF(mtcColumn) * aColCount,
                              (void**)&sMtcColumns )
              != IDE_SUCCESS );

    for( i = 0, sColumn = aSource;
         (i < aColCount) && (sColumn != NULL);
         i++, sColumn = sColumn->next )
    {
        idlOS::memcpy( *aTarget + i,
                       sColumn,
                       ID_SIZEOF(qcmColumn) );
    }

    for( i = 0, sColumn = aSource, sTargetPosition = *aTarget;
         (i < aColCount) && (sColumn != NULL);  // TASK-3876 Code Sonar
         i++, sMtcColumns++, sColumn = sColumn->next, sTargetPosition++ )
    {
        idlOS::memcpy( sMtcColumns, sColumn->basicInfo, ID_SIZEOF(mtcColumn) );

        sTargetPosition->basicInfo = sMtcColumns;
        sTargetPosition->next = sTargetPosition+1;
    }
    (sTargetPosition-1)->next = NULL;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qcm::validateAndLockTable( qcStatement      * aStatement,
                                  void             * aTableHandle,
                                  smSCN              aSCN,
                                  smiTableLockMode   aLockMode )
{
/***********************************************************************
 *
 * Description :
 *    TASK-2176
 *    stmtKind�� ���� ������ ���� �����Ѵ�.
 *    1. DML�� ��� tableHandle�� NULL�� �ƴϾ�� �Ѵ�.
 *    2. DDL�� ��� tableHandle�� NULL�� �� �ִ�. (CREATE TABLE ��)
 *       (QCI_STMT_NON_SCHEMA_DDL�̶�� �ؼ� tableHandle�� �ݵ�� NULL�� �ƴϴ�.)
 *    3. DCL/EAB/DB�� ��� tableHandle�� ���̺��� handle�� �ƴϰ�,
 *       NULL�̾�� �Ѵ�.
 *    4. SP�� ��� tableHandle�� ���̺��� handle�� �ƴϰ�, NULL�̾�� �Ѵ�.
 *       (PSM�� handle������ ���� �˾Ƴ� ����� ����.)
 *
 * Implementation :
 *
 *
 ***********************************************************************/

    UInt              sTableFlag;
    switch ( aStatement->myPlan->parseTree->stmtKind & QCI_STMT_MASK_MASK )
    {
        case QCI_STMT_MASK_DML:

            if ( aTableHandle == NULL )
            {
                // Nothing to do.

                ideLog::log(IDE_QP_0,"[LOCK_TABLE_FOR_DML] TableHanle is NULL\n%s\n",
                            aStatement->myPlan->stmtText );

                // tableHandle�� NULL�� DML�� ���� �� ����.
                IDE_DASSERT(0);
            }
            else
            {
                sTableFlag = smiGetTableFlag( aTableHandle );

                if ( (sTableFlag & SMI_TABLE_TYPE_MASK) == SMI_TABLE_FIXED )
                {
                    // skip Table Lock

                    ideLog::log(IDE_QP_0,"[LOCK_TABLE_FOR_DML] TableHandle is fixed type\n%s\n",
                                aStatement->myPlan->stmtText );

                    // Fixed Type�� validateLockTable�� ȣ���ؼ��� �ȵȴ�.
                    IDE_DASSERT(0);
                }
                else
                {
                        IDE_TEST(smiValidateAndLockObjects( (QC_SMI_STMT( aStatement ))->getTrans(),
                                                            aTableHandle,
                                                            aSCN,
                                                            SMI_TBSLV_DDL_DML, // TBS Validation �ɼ�
                                                            aLockMode,
                                                            ID_ULONG_MAX,
                                                            ID_FALSE) // BUG-28752 ����� Lock�� ������ Lock�� �����մϴ�.
                                 != IDE_SUCCESS);
                }
            }
            break;

        case QCI_STMT_MASK_DDL:
            //R2HA dcl lock timeout is not exist. use ddl lock timeout temporary
        case QCI_STMT_MASK_DCL:
            if ( aTableHandle == NULL )
            {
                // Nothing to do.
                // create table or create view
            }
            else
            {
                sTableFlag = smiGetTableFlag( aTableHandle );

                if ( (sTableFlag & SMI_TABLE_TYPE_MASK) == SMI_TABLE_FIXED )
                {
                    // skip Table Lock
                }
                else
                {
                    // Table Lock
                    IDE_TEST(smiValidateAndLockObjects( (QC_SMI_STMT( aStatement ))->getTrans(),
                                                        aTableHandle,
                                                        aSCN,
                                                        SMI_TBSLV_DDL_DML, // TBS Validation �ɼ�
                                                        aLockMode,
                                                        smiGetDDLLockTimeOut((QC_SMI_STMT(aStatement))->getTrans()),
                                                        ID_FALSE) // BUG-28752 ����� Lock�� ������ Lock�� �����մϴ�.
                             != IDE_SUCCESS);
                }
            }
            break;

        case QCI_STMT_MASK_DB:

            if ( aTableHandle == NULL )
            {
                // Nothing to do.
            }
            else
            {
                ideLog::log(IDE_QP_0,"[LOCK_TABLE_FOR_DB] TableHanle is not NULL\n%s\n",
                            aStatement->myPlan->stmtText );

                // tableHandle�� NULL�� �ƴ� DB�� ���� �� ����.
                IDE_DASSERT(0);
            }
            break;

        case QCI_STMT_MASK_SP:

            if ( aTableHandle == NULL )
            {
                // Nothing to do.
            }
            else
            {
                ideLog::log(IDE_QP_0,"[LOCK_TABLE_FOR_SP] TableHanle is not NULL\n%s\n",
                            aStatement->myPlan->stmtText );

                // tableHandle�� NULL�� �ƴ� SP�� ���� �� ����.
                IDE_DASSERT(0);
            }
            break;

        default:
            IDE_ASSERT(0);
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qcm::validateAndLockAllObjects( qcStatement *aStatement )
{
/***********************************************************************
 *
 * Description :
 *    BUG-17455
 *    Statement���� ����ϴ� Object���� ��ȿ���� �˻��ϰ�
 *    Object�� ������ ���� �˸��� ���� ȹ���Ѵ�.
 *
 * Implementation :
 *    1. Table/Queue
 *       smiValidateAndLockObjects���� ������ �˻��ϰ� ���� ȹ���Ѵ�.
 *    2. Sequence
 *       checkSequence�� ������ �˻��Ѵ�. ���� ����
 *
 ***********************************************************************/

    UShort                i;
    qcParseSeqCaches    * sParseSeqCache;
    qcTemplate          * sTemplate;
    qcTableMap          * sTableMap;
    qmsTableRef         * sTableRef;
    qmsPartitionRef     * sPartitionRef = NULL;
    UShort                sRowCount;
    mtcTuple            * sRows;

    IDE_DASSERT( aStatement != NULL );

    //------------------------------------------
    // TABLE or VIEW ��ü�� ����˻�
    //------------------------------------------

    sTemplate = QC_PRIVATE_TMPLATE(aStatement);

    // TASK-3876 Code Sonar
    IDE_ASSERT( aStatement->myPlan->parseTree != NULL );

    if ( sTemplate->planCount > 0 )
    {
        sTableMap = sTemplate->tableMap;
        sRowCount = sTemplate->tmplate.rowCount;
        sRows     = sTemplate->tmplate.rows;
        // BUG-17409
        for ( i = sRowCount; i != 0; i--, sRows++, sTableMap++ )
        {
            if ( sTableMap->from != NULL )
            {
                // FROM�� �ش��ϴ� ��ü�� ���

                if( ( sRows->lflag & MTC_TUPLE_PARTITION_MASK ) ==
                    MTC_TUPLE_PARTITION_FALSE )
                {
                    // PROJ-2582 recursive CTE
                    // recursive member�� right subquery�� with query_name��
                    //valdiate �ܰ迡�� table�� ó�� �ϱ� ���� �ӽ� tableInfo�� �����Ͽ���.
                    // execute �ܰ迡���� view ó�� ó�� �Ͽ��� �Ѵ�.
                    if ( ( sTableMap->from->tableRef->view == NULL ) &&
                         ( ( sTableMap->from->tableRef->flag &
                             QMS_TABLE_REF_RECURSIVE_VIEW_MASK )
                           != QMS_TABLE_REF_RECURSIVE_VIEW_TRUE ) )
                    {
                        // Table�� ���
                        sTableRef = sTableMap->from->tableRef;

                        if ( (sTableRef->flag & QMS_TABLE_REF_SCAN_FOR_NON_SELECT_MASK) ==
                             QMS_TABLE_REF_SCAN_FOR_NON_SELECT_TRUE )
                        {
                            // Nothing to do.

                            // BUG-17409
                            // Lock Escalation�� DeadLock�� ������ �� �����Ƿ�
                            // Insert/Update/Delete/Move�� Target Table�� ���ؼ���
                            // �ش� execute�Լ����� ���� ������ Lock Mode�� ȹ���Ѵ�.
                        }
                        else
                        {
                            if ( (sTableRef->tableFlag & SMI_TABLE_TYPE_MASK) == SMI_TABLE_FIXED )
                            {
                                // skip Table Lock
                            }
                            else
                            {
                                // Table Lock

                                // BUG-21361
                                // queue table�� ��� IX_LOCK�� ȹ���Ѵ�. dequeue�� queue table��
                                // ���ؼ��� ������ �����ϸ� queue table�̿��� table�� ���ؼ���
                                // ������ �� ����. (dequeue������ subquery�� �Ұ��ϴ�.)
                                if( aStatement->myPlan->parseTree->stmtKind == QCI_STMT_DEQUEUE )
                                {
                                    IDE_TEST( smiValidateAndLockObjects( (QC_SMI_STMT( aStatement ))->getTrans(),
                                                  sTableRef->tableHandle,
                                                  sTableRef->tableSCN,
                                                  SMI_TBSLV_DDL_DML, // TBS Validation �ɼ�
                                                  SMI_TABLE_LOCK_IX,
                                                  ID_ULONG_MAX,
                                                  ID_FALSE ) // BUG-28752 ����� Lock�� ������ Lock�� �����մϴ�.
                                              != IDE_SUCCESS );

                                    // queue table�� partitioned table�� �ƴϴ�.
                                    IDE_DASSERT( ( sRows->lflag & MTC_TUPLE_PARTITIONED_TABLE_MASK ) ==
                                                 MTC_TUPLE_PARTITIONED_TABLE_FALSE );
                                }
                                else
                                {
                                    // BUG-16978
                                    IDE_TEST( smiValidateAndLockObjects( (QC_SMI_STMT( aStatement ))->getTrans(),
                                                                         sTableRef->tableHandle,
                                                                         sTableRef->tableSCN,
                                                                         SMI_TBSLV_DDL_DML, // TBS Validation �ɼ�
                                                                         SMI_TABLE_LOCK_IS,
                                                                         ID_ULONG_MAX,
                                                                         ID_FALSE ) // BUG-28752 ����� Lock�� ������ Lock�� �����մϴ�.
                                              != IDE_SUCCESS );
                                    // PROJ-2462 ResultCache
                                    // ������ Parition�� ��� smiValidationAndLock�� �����
                                    // ���õ� Partition������ Execution������ Lock�� ��´�.
                                    // ResultCache������ Cache�� Partitio�� invalid ���θ�
                                    // �˾ƾ߸� Cache�� Row Pointer�� ��ȿ���� �˼� �ִ�.
                                    // ���� �� Partition�� Scn�� ��ȿ������ �˻��Ѵ�.
                                    if ( ( sTemplate->resultCache.count > 0 ) &&
                                         ( ( sTemplate->resultCache.flag & QC_RESULT_CACHE_MAX_EXCEED_MASK )
                                           == QC_RESULT_CACHE_MAX_EXCEED_FALSE ) )
                                    {
                                        for ( sPartitionRef = sTableRef->partitionRef;
                                              sPartitionRef != NULL;
                                              sPartitionRef = sPartitionRef->next )
                                        {
                                            IDE_TEST( smiValidateObjects( sPartitionRef->partitionHandle,
                                                                          sPartitionRef->partitionSCN )
                                                      != IDE_SUCCESS );
                                        }
                                    }
                                    else
                                    {
                                        /* Nothing to do */
                                    }
                                }
                            }
                        }
                    }
                    else
                    {
                        // View�� ���

                        // BUG-37832, BUG-38264
                        sTableRef = sTableMap->from->tableRef;

                        if ( ( sTableRef->tableHandle != NULL ) &&
                             ( ( sTableRef->tableType == QCM_VIEW ) ||
                               ( sTableRef->tableType == QCM_MVIEW_VIEW ) ) )
                        {
                            // subquery ���°� �ƴ� VIEW
                            // ex) FROM t1, v1
                            IDE_TEST( smiValidateAndLockObjects( (QC_SMI_STMT( aStatement ) )->getTrans(),
                                          sTableRef->tableHandle,
                                          sTableRef->tableSCN,
                                          SMI_TBSLV_DDL_DML, // TBS Validation �ɼ�
                                          SMI_TABLE_LOCK_IS,
                                          ID_ULONG_MAX,
                                          ID_FALSE ) // BUG-28752 ����� Lock�� ������ Lock�� �����մϴ�.
                                      != IDE_SUCCESS );
                        }
                        else
                        {
                            // Nothing to do.
                        }
                    }
                }
                else
                {
                    // partition�� ���
                    // Nothing to do.
                }
            }
            else
            {
                // Nothing To Do
            }
        }
    }
    else
    {
        // Nothing to do.

        // planCount�� 0�� ��� �� execute�Լ����� ������ ���� ȹ���Ѵ�.
        // (planCount�� 0�� ��찡 ��� DDL�� �ƴ�)
    }

    //------------------------------------------
    // BUG-17455
    // SEQUENCE ��ü�� ����˻�
    //------------------------------------------

    // CurrVal
    if ( aStatement->myPlan->parseTree->currValSeqs != NULL )
    {
        for ( sParseSeqCache = aStatement->myPlan->parseTree->currValSeqs;
              sParseSeqCache != NULL;
              sParseSeqCache = sParseSeqCache->next )
        {
            IDE_TEST( qds::checkSequence( aStatement,
                                          sParseSeqCache,
                                          SMI_SEQUENCE_CURR )
                      != IDE_SUCCESS );
        }
    }
    else
    {
        // Nothing to do.
    }

    // NextVal
    if ( aStatement->myPlan->parseTree->nextValSeqs != NULL )
    {
        for ( sParseSeqCache = aStatement->myPlan->parseTree->nextValSeqs;
              sParseSeqCache != NULL;
              sParseSeqCache = sParseSeqCache->next )
        {
            IDE_TEST( qds::checkSequence( aStatement,
                                          sParseSeqCache,
                                          SMI_SEQUENCE_NEXT )
                      != IDE_SUCCESS );
        }
    }
    else
    {
        // Nothing to do.
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qcm::lockTableForDMLValidation(qcStatement  * aStatement,
                                      void         * aTableHandle,
                                      smSCN          aSCN)
{
/***********************************************************************
 *
 * Description : BUG-34492
 *
 *    DML�� Validation ��, ���̺� LOCK(IS)�� ��� �Լ��Դϴ�.
 *
 * Implementation :
 *
 *
 ***********************************************************************/

    // ���̺� LOCK(IS)
    IDE_TEST(smiValidateAndLockObjects( (QC_SMI_STMT( aStatement ))->getTrans(),
                                        aTableHandle,
                                        aSCN,
                                        SMI_TBSLV_DDL_DML, // TBS Validation �ɼ�
                                        SMI_TABLE_LOCK_IS,
                                        ID_ULONG_MAX,
                                        ID_FALSE ) // BUG-28752 ����� Lock�� ������ Lock�� �����մϴ�.
             != IDE_SUCCESS);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qcm::lockTableForDDLValidation(qcStatement * aStatement,
                                      void        * aTableHandle,
                                      smSCN         aSCN)
{
/***********************************************************************
 *
 * Description :
 *
 *    DDL�� Validation ��, ���̺� LOCK(IS)�� ��� �Լ��Դϴ�.
 *
 *    BUG-17547�� fix�� ���� �ӽ÷� ���� �Լ��Դϴ�.
 *    TASK-2176�� ���ؼ� �ٽ� �����Ǿ�� �մϴ�.
 *
 * Implementation :
 *
 *
 ***********************************************************************/

    // ���̺� LOCK(IS)
    IDE_TEST(smiValidateAndLockObjects( (QC_SMI_STMT( aStatement ))->getTrans(),
                                        aTableHandle,
                                        aSCN,
                                        SMI_TBSLV_DDL_DML, // TBS Validation �ɼ�
                                        SMI_TABLE_LOCK_IS,
                                        smiGetDDLLockTimeOut((QC_SMI_STMT(aStatement))->getTrans()),
                                        ID_FALSE ) // BUG-28752 ����� Lock�� ������ Lock�� �����մϴ�.
             != IDE_SUCCESS);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/* ------------------------------------------------
 *  X$DATATYPE
 * ----------------------------------------------*/

// BUG-17202
// �� �ʵ��� �Ӽ��� ODBC SPEC�� �µ��� ����

static iduFixedTableColDesc gDataTypeColDesc[] =
{
    {
        (SChar *)"TYPE_NAME",
        offsetof(mtdType, mTypeName),
        MTD_TYPE_NAME_LEN,
        IDU_FT_TYPE_VARCHAR | IDU_FT_TYPE_POINTER,
        NULL,
        0, 0, NULL // for internal use
    },

    {
        (SChar *)"DATA_TYPE",
        offsetof(mtdType, mDataType),
        IDU_FT_SIZEOF(mtdType, mDataType),
        IDU_FT_TYPE_SMALLINT,
        NULL,
        0, 0, NULL // for internal use
    },

    // BUG-17202�� ���� �ʵ� �߰�
    {
        (SChar *)"ODBC_DATA_TYPE",
        offsetof(mtdType, mODBCDataType),
        IDU_FT_SIZEOF(mtdType, mODBCDataType),
        IDU_FT_TYPE_SMALLINT,
        NULL,
        0, 0, NULL // for internal use
    },

    {
        (SChar *)"COLUMN_SIZE",
        offsetof(mtdType, mColumnSize),
        IDU_FT_SIZEOF(mtdType, mColumnSize),
        IDU_FT_TYPE_INTEGER,
        NULL,
        0, 0, NULL // for internal use
    },

    {
        (SChar *)"LITERAL_PREFIX",
        offsetof(mtdType, mLiteralPrefix),
        MTD_TYPE_LITERAL_LEN,
        IDU_FT_TYPE_VARCHAR | IDU_FT_TYPE_POINTER,
        NULL,
        0, 0, NULL // for internal use
    },

    {
        (SChar *)"LITERAL_SUFFIX",
        offsetof(mtdType, mLiteralSuffix),
        MTD_TYPE_LITERAL_LEN,
        IDU_FT_TYPE_VARCHAR | IDU_FT_TYPE_POINTER,
        NULL,
        0, 0, NULL // for internal use
    },

    {
        (SChar *)"CREATE_PARAM",
        offsetof(mtdType, mCreateParam),
        MTD_TYPE_CREATE_PARAM_LEN,
        IDU_FT_TYPE_VARCHAR | IDU_FT_TYPE_POINTER,
        NULL,
        0, 0, NULL // for internal use
    },

    {
        (SChar *)"NULLABLE",
        offsetof(mtdType, mNullable),
        IDU_FT_SIZEOF(mtdType, mNullable),
        IDU_FT_TYPE_SMALLINT,
        NULL,
        0, 0, NULL // for internal use
    },

    {
        (SChar *)"CASE_SENSITIVE",
        offsetof(mtdType, mCaseSens),
        IDU_FT_SIZEOF(mtdType, mCaseSens),
        IDU_FT_TYPE_SMALLINT,
        NULL,
        0, 0, NULL // for internal use
    },

    {
        (SChar *)"SEARCHABLE",
        offsetof(mtdType, mSearchable),
        IDU_FT_SIZEOF(mtdType, mSearchable),
        IDU_FT_TYPE_SMALLINT,
        NULL,
        0, 0, NULL // for internal use
    },

    {
        (SChar *)"UNSIGNED_ATTRIBUTE",
        offsetof(mtdType, mUnsignedAttr),
        IDU_FT_SIZEOF(mtdType, mUnsignedAttr),
        IDU_FT_TYPE_SMALLINT,
        NULL,
        0, 0, NULL // for internal use
    },

    {
        (SChar *)"FIXED_PREC_SCALE",
        offsetof(mtdType, mFixedPrecScale),
        IDU_FT_SIZEOF(mtdType, mFixedPrecScale),
        IDU_FT_TYPE_SMALLINT,
        NULL,
        0, 0, NULL // for internal use
    },

    {
        (SChar *)"AUTO_UNIQUE_VALUE",
        offsetof(mtdType, mAutoUniqueValue),
        IDU_FT_SIZEOF(mtdType, mAutoUniqueValue),
        IDU_FT_TYPE_SMALLINT,
        NULL,
        0, 0, NULL // for internal use
    },

    {
        (SChar *)"LOCAL_TYPE_NAME",
        offsetof(mtdType, mLocalTypeName),
        MTD_TYPE_NAME_LEN,
        IDU_FT_TYPE_VARCHAR | IDU_FT_TYPE_POINTER,
        NULL,
        0, 0, NULL // for internal use
    },

    {
        (SChar *)"MINIMUM_SCALE",
        offsetof(mtdType, mMinScale),
        IDU_FT_SIZEOF(mtdType, mMinScale),
        IDU_FT_TYPE_SMALLINT,
        NULL,
        0, 0, NULL // for internal use
    },

    {
        (SChar *)"MAXIMUM_SCALE",
        offsetof(mtdType, mMaxScale),
        IDU_FT_SIZEOF(mtdType, mMaxScale),
        IDU_FT_TYPE_SMALLINT,
        NULL,
        0, 0, NULL // for internal use
    },

    {
        (SChar *)"SQL_DATA_TYPE",
        offsetof(mtdType, mSQLDataType),
        IDU_FT_SIZEOF(mtdType, mSQLDataType),
        IDU_FT_TYPE_SMALLINT,
        NULL,
        0, 0, NULL // for internal use
    },

    // BUG-17202�� ���� �ʵ� �߰�
    {
        (SChar *)"ODBC_SQL_DATA_TYPE",
        offsetof(mtdType, mODBCSQLDataType),
        IDU_FT_SIZEOF(mtdType, mODBCSQLDataType),
        IDU_FT_TYPE_SMALLINT,
        NULL,
        0, 0, NULL // for internal use
    },

    {
        (SChar *)"SQL_DATETIME_SUB",
        offsetof(mtdType, mSQLDateTimeSub),
        IDU_FT_SIZEOF(mtdType, mSQLDateTimeSub),
        IDU_FT_TYPE_SMALLINT,
        NULL,
        0, 0, NULL // for internal use
    },

    {
        (SChar *)"NUM_PREC_RADIX",
        offsetof(mtdType, mNumPrecRadix),
        IDU_FT_SIZEOF(mtdType, mNumPrecRadix),
        IDU_FT_TYPE_INTEGER,
        NULL,
        0, 0, NULL // for internal use
    },

    {
        (SChar *)"INTERVAL_PRECISION",
        offsetof(mtdType, mIntervalPrecision),
        IDU_FT_SIZEOF(mtdType, mIntervalPrecision),
        IDU_FT_TYPE_SMALLINT,
        NULL,
        0, 0, NULL // for internal use
    },

    {
        NULL,
        0,
        0,
        IDU_FT_TYPE_CHAR,
        NULL,
        0, 0, NULL // for internal use
    }
};

IDE_RC buildRecordForDATATYPE( idvSQL              * /*aStatistics*/,
                               void                * aHeader,
                               void                * /* aDumpObj */,
                               iduFixedTableMemory * aMemory )
{
/***********************************************************************
 *
 * Description : X$DATATYPE�� �����ϱ� ���� ���ڵ� ���� ����
 *
 * Implementation : ( PROJ-1362 )
 *
 ***********************************************************************/

    const mtdModule * sModule;
    mtdType           sDataType;
    UInt              i;

    SChar * sLiteralPrefix[2] = { (SChar*)"'", (SChar*)"{ts'" };
    SChar * sLiteralSuffix[2] = { (SChar*)"'", (SChar*)"'}" };

    SChar * sCreateParamPrecision      = (SChar*)"precision";
    SChar * sCreateParamPrecisionScale = (SChar*)"precision,scale";

    SInt    sTrue  = 1;
    SInt    sFalse = 0;

    SInt    sPredNone   = 0;
    SInt    sPredChar   = 1;
    SInt    sPredBasic  = 2;
    SInt    sSearchable = 3;

    SInt    sSQLDateTimeSubValue = 3;    // BUG-17020

    SInt    sNumPrecRadixValue = 10;

    for ( i = 0; i < mtd::getNumberOfModules(); i++ )
    {
        IDE_TEST( mtd::moduleByNo( & sModule, i )
                  != IDE_SUCCESS );

        if ( ( (sModule->flag & MTD_CREATE_MASK) == MTD_CREATE_ENABLE ) ||
             ( sModule->id == MTD_BINARY_ID ) )
        {
            sDataType.mTypeName      = (SChar*) sModule->names->string;
            sDataType.mDataType      = sModule->id;

            //fix BUG-17202
            // �������� DECODE ������ ���� ���� ������
            // DECODE�� ������ ���� NUMERIC �Ӽ��� ����
            // ODBC SPEC�� SMALLINT Ÿ������ ��ȯ�ϱ� ����
            // DECODE ������� ODBC_DATA_TYPE�� ����
            switch ( sDataType.mDataType )
            {
                // PROJ-2002 Column Security
                // echar->char, evarchar->varchar
                case 60 :
                    sDataType.mODBCDataType = 1;
                    break;
                case 61 :
                    sDataType.mODBCDataType = 12;
                    break;
                case 9 :
                    sDataType.mODBCDataType = 93;
                    break;
                case 6:
                    sDataType.mODBCDataType = 2;
                    break;
                case 10002:
                    sDataType.mODBCDataType = 2;
                    break;
                default:
                    sDataType.mODBCDataType = sDataType.mDataType;
                    break;
            }

            sDataType.mColumnSize    = sModule->maxStorePrecision;

            if ( (sModule->flag & MTD_LITERAL_MASK)
                 == MTD_LITERAL_TRUE )
            {
                if ( (sModule->flag & MTD_SQL_DATETIME_SUB_MASK)
                     == MTD_SQL_DATETIME_SUB_TRUE )
                {
                    sDataType.mLiteralPrefix = sLiteralPrefix[1];
                    sDataType.mLiteralSuffix = sLiteralSuffix[1];
                }
                else
                {
                    sDataType.mLiteralPrefix = sLiteralPrefix[0];
                    sDataType.mLiteralSuffix = sLiteralSuffix[0];
                }
            }
            else
            {
                sDataType.mLiteralPrefix = NULL;
                sDataType.mLiteralSuffix = NULL;
            }

            if ( (sModule->flag & MTD_CREATE_PARAM_MASK)
                 == MTD_CREATE_PARAM_PRECISION )
            {
                sDataType.mCreateParam = sCreateParamPrecision;
            }
            else if ( (sModule->flag & MTD_CREATE_PARAM_MASK)
                      == MTD_CREATE_PARAM_PRECISIONSCALE )
            {
                sDataType.mCreateParam = sCreateParamPrecisionScale;
            }
            else
            {
                sDataType.mCreateParam = NULL;
            }

            sDataType.mNullable     = sTrue;

            if ( (sModule->flag & MTD_CASE_SENS_MASK)
                 == MTD_CASE_SENS_TRUE )
            {
                sDataType.mCaseSens = sTrue;
            }
            else
            {
                sDataType.mCaseSens = sFalse;
            }

            if ( (sModule->flag & MTD_SEARCHABLE_MASK)
                 == MTD_SEARCHABLE_PRED_CHAR )
            {
                sDataType.mSearchable = sPredChar;
            }
            else if ( (sModule->flag & MTD_SEARCHABLE_MASK)
                      == MTD_SEARCHABLE_PRED_BASIC )
            {
                sDataType.mSearchable = sPredBasic;
            }
            else if ( (sModule->flag & MTD_SEARCHABLE_MASK)
                      == MTD_SEARCHABLE_SEARCHABLE )
            {
                sDataType.mSearchable = sSearchable;
            }
            else
            {
                sDataType.mSearchable = sPredNone;
            }

            if ( (sModule->flag & MTD_UNSIGNED_ATTR_MASK)
                 == MTD_UNSIGNED_ATTR_TRUE )
            {
                sDataType.mUnsignedAttr = sFalse;
            }
            else
            {
                sDataType.mUnsignedAttr = MTD_SMALLINT_NULL;
            }

            sDataType.mFixedPrecScale  = sFalse;
            sDataType.mAutoUniqueValue = MTD_SMALLINT_NULL;
            sDataType.mLocalTypeName   = sDataType.mTypeName;

            if ( sModule->minScale != 0 )
            {
                sDataType.mMinScale = sModule->minScale;
            }
            else
            {
                sDataType.mMinScale = MTD_SMALLINT_NULL;
            }

            if ( sModule->maxScale != 0 )
            {
                sDataType.mMaxScale = sModule->maxScale;
            }
            else
            {
                sDataType.mMaxScale = MTD_SMALLINT_NULL;
            }

            //fix BUG-17202
            // �������� DECODE ������ ���� ���� ������
            // DECODE�� ������ ���� NUMERIC �Ӽ��� ����
            // ODBC SPEC�� SMALLINT Ÿ������ ��ȯ�ϱ� ����
            // DECODE ������� ODBC_SQL_DATA_TYPE�� ����

            sDataType.mSQLDataType = sDataType.mDataType;

            switch ( sDataType.mSQLDataType )
            {
                // PROJ-2002 Column Security
                // echar->char, evarchar->varchar
                case 60:
                    sDataType.mODBCSQLDataType = 1;
                    break;
                case 61:
                    sDataType.mODBCSQLDataType = 12;
                    break;
                // BUG-21574, 21594
                // float = 6, date = 11 �� �Ѵ�.
                case 9:
                    sDataType.mODBCSQLDataType = 11;
                    break;
                case 10002:
                    sDataType.mODBCSQLDataType = 2;
                    break;
                default:
                    sDataType.mODBCSQLDataType = sDataType.mSQLDataType;
                    break;
            }

            if ( (sModule->flag & MTD_SQL_DATETIME_SUB_MASK)
                 == MTD_SQL_DATETIME_SUB_TRUE )
            {
                sDataType.mSQLDateTimeSub = sSQLDateTimeSubValue;
            }
            else
            {
                sDataType.mSQLDateTimeSub = MTD_SMALLINT_NULL;
            }

            if ( (sModule->flag & MTD_NUM_PREC_RADIX_MASK)
                 == MTD_NUM_PREC_RADIX_TRUE )
            {
                sDataType.mNumPrecRadix = sNumPrecRadixValue;
            }
            else
            {
                sDataType.mNumPrecRadix = MTD_INTEGER_NULL;
            }

            sDataType.mIntervalPrecision = MTD_SMALLINT_NULL;

            IDE_TEST( iduFixedTable::buildRecord(
                          aHeader,
                          aMemory,
                          (void *) & sDataType )
                      != IDE_SUCCESS);
        }
        else
        {
            // Nothing to do.
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

iduFixedTableDesc gDataTypeTableDesc =
{
    (SChar *)"X$DATATYPE",
    buildRecordForDATATYPE,
    gDataTypeColDesc,
    IDU_STARTUP_META,
    0,
    0,
    IDU_FT_DESC_TRANS_NOT_USE,
    NULL
};


/* ------------------------------------------------
 *  X$DUAL
 * ----------------------------------------------*/

// PROJ-2083 DUAL TABLE

typedef struct qcmDualTable
{
    const SChar   * mDummyCol;

} qcmDualTable;

static iduFixedTableColDesc gDualColDesc[] =
{
    {
        (SChar *)"DUMMY",
        offsetof(qcmDualTable, mDummyCol),
        1,
        IDU_FT_TYPE_VARCHAR | IDU_FT_TYPE_POINTER,
        NULL,
        0, 0,NULL // for internal use
    },

    {
        NULL,
        0,
        0,
        IDU_FT_TYPE_CHAR,
        NULL,
        0, 0,NULL // for internal use
    }
};

IDE_RC buildRecordForDualTable( idvSQL              * /*aStatistics*/,
                                void        *aHeader,
                                void        * /* aDumpObj */,
                                iduFixedTableMemory *aMemory)
{
    qcmDualTable  sDualTable;

    sDualTable.mDummyCol = (SChar *)"X";

    IDE_TEST( iduFixedTable::buildRecord( aHeader,
                                          aMemory,
                                          (void *)& sDualTable )
              != IDE_SUCCESS);

    return IDE_SUCCESS;
    IDE_EXCEPTION_END;
    return IDE_FAILURE;
}

iduFixedTableDesc gDualTable =
{
    (SChar *)"X$DUAL",
    buildRecordForDualTable,
    gDualColDesc,
    IDU_STARTUP_INIT,
    0,
    0,
    IDU_FT_DESC_TRANS_NOT_USE,
    NULL
};

static iduFixedTableColDesc gNlsTerritoryColDesc[] =
{
    {
        ( SChar * )"NAME",
        offsetof( qcmDualTable, mDummyCol ),
        MTD_TYPE_NAME_LEN,
        IDU_FT_TYPE_VARCHAR | IDU_FT_TYPE_POINTER,
        NULL,
        0, 0, NULL // for internal use
    },

    {
        NULL,
        0,
        0,
        IDU_FT_TYPE_CHAR,
        NULL,
        0, 0, NULL
    }
};

IDE_RC buildRecordForNlsTerritoryTable( idvSQL              * /*aStatistics*/,
                                        void                * aHeader,
                                        void                * ,
                                        iduFixedTableMemory * aMemory )
{
    qcmDualTable  sDualTable;
    SInt          i;

    for ( i = 0 ; i < TERRITORY_MAX; i++ )
    {
        sDualTable.mDummyCol = mtlTerritory::getNlsTerritoryName( i );

        IDE_TEST( iduFixedTable::buildRecord( aHeader,
                                              aMemory,
                                              (void *)& sDualTable )
                  != IDE_SUCCESS);
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END

    return IDE_FAILURE;
}

iduFixedTableDesc gNlsTerritoryTable =
{
    ( SChar * )"X$NLS_TERRITORY",
    buildRecordForNlsTerritoryTable,
    gNlsTerritoryColDesc,
    IDU_STARTUP_INIT,
    0,
    0,
    IDU_FT_DESC_TRANS_NOT_USE,
    NULL
};

/* ------------------------------------------------
 *  X$TIME_ZONE_NAMES
 * ----------------------------------------------*/
static iduFixedTableColDesc gTimeZoneNamesColDesc[] =
{
    {
        (SChar *)"NAME",
        offsetof(mtzTimezoneNamesTable, mName),
        MTC_TIMEZONE_NAME_LEN,
        IDU_FT_TYPE_VARCHAR | IDU_FT_TYPE_POINTER,
        NULL,
        0, 0,NULL // for internal use
    },
    {
        (SChar *)"UTC_OFFSET",
        offsetof(mtzTimezoneNamesTable, mUTCOffset),
        MTC_TIMEZONE_VALUE_LEN,
        IDU_FT_TYPE_VARCHAR | IDU_FT_TYPE_POINTER,
        NULL,
        0, 0,NULL // for internal use
    },
    {
        (SChar *)"SECOND_OFFSET",
        offsetof(mtzTimezoneNamesTable, mSecond),
        IDU_FT_SIZEOF(mtzTimezoneNamesTable, mSecond),
        IDU_FT_TYPE_BIGINT,
        NULL,
        0, 0,NULL // for internal use
    },
    {
        NULL,
        0,
        0,
        IDU_FT_TYPE_CHAR,
        NULL,
        0, 0,NULL // for internal use
    }
};

IDE_RC buildRecordForTimezoneNamesTable( idvSQL              * /*aStatistics*/,
                                         void        *aHeader,
                                         void        * /* aDumpObj */,
                                         iduFixedTableMemory *aMemory )
{
    mtzTimezoneNamesTable sTimezoneNamesTable;
    UInt                  i;
    UInt                  sCount;

    sCount = mtz::getTimezoneElementCount();

    for ( i = 0; i < sCount; i++ )
    {
        sTimezoneNamesTable.mName      = mtz::getTimezoneName( i );
        sTimezoneNamesTable.mUTCOffset = mtz::getTimezoneUTCOffset( i );
        sTimezoneNamesTable.mSecond    = mtz::getTimezoneSecondWithIndex( i );

        IDE_TEST( iduFixedTable::buildRecord( aHeader,
                                              aMemory,
                                              (void *)& sTimezoneNamesTable )
                  != IDE_SUCCESS);
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

iduFixedTableDesc gTimezoneNamesTable =
{
    (SChar *)"X$TIME_ZONE_NAMES",
    buildRecordForTimezoneNamesTable,
    gTimeZoneNamesColDesc,
    IDU_STARTUP_INIT,
    0,
    0,
    IDU_FT_DESC_TRANS_NOT_USE,
    NULL
};

/* PROJ-2446 ONE SOURCE */
IDE_RC qcm::makeAndSetQcmTblInfoCallback( smiStatement * aSmiStmt,
                                          UInt           aTableID,
                                          smOID          aTableOID )
{
    qcmTableInfo * sTableInfo;
    const void   * sTableHandle;

    IDE_TEST( makeAndSetQcmTableInfo( aSmiStmt,
                                      aTableID,
                                      aTableOID ) != IDE_SUCCESS);

    sTableHandle = smiGetTable( aTableOID );
    IDE_TEST( smiGetTableTempInfo( sTableHandle, 
                                   (void**)&sTableInfo )
              != IDE_SUCCESS );

    if( sTableInfo->tablePartitionType == QCM_PARTITIONED_TABLE )
    {
        // PROJ-1502 PARTITIONED DISK TABLE
        IDE_TEST( qcmPartition::makeQcmPartitionInfoByTableID(
                      aSmiStmt,
                      aTableID )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qcm::getQcmLobColumn(smiStatement * aSmiStmt,
                            qcmTableInfo * aTableInfo)
{
/***********************************************************************
 *
 * Description :
 *    BUG-42877
 *    makeAndSetQcmTableInfo �κ��� ȣ��Ǹ�
 *    lob column�� ������ lob ������ sTableInfo->columns�� flag�� �߰��Ѵ�.
 *
 * Implementation :
 *    1. SYS_LOBS_ ���̺��� IS_DEFAULT_TBS �÷��� ���Ѵ�.
 *    2. �� �Ǿ� �����鼭 ��Ÿ ĳ���� �߰��Ѵ�.
 *
 ***********************************************************************/

    UInt                    sStage  = 0;
    UInt                    i       = 0;
    smiRange                sRange;
    const void            * sRow;
    mtcColumn             * sTableIdCol;
    mtcColumn             * sColumnIdCol;
    mtcColumn             * sIsDefaultTBSCol;
    mtdCharType           * sIsDefaultTBSStr;
    qtcMetaRangeColumn      sRangeColumn;
    smiTableCursor          sCursor;
    smiCursorProperties     sCursorProperty;

    scGRID                  sRid; // Disk Table�� ���� Record IDentifier

    sCursor.initialize();

    IDE_TEST( smiGetTableColumns( gQcmLobs,
                                  QCM_LOBS_TABLE_ID_COL_ORDER,
                                  (const smiColumn**)&sTableIdCol )
              != IDE_SUCCESS );
    IDE_TEST( smiGetTableColumns( gQcmLobs,
                                  QCM_LOBS_COLUMN_ID_COL_ORDER,
                                  (const smiColumn**)&sColumnIdCol )
              != IDE_SUCCESS );
    IDE_TEST( smiGetTableColumns( gQcmLobs,
                                  QCM_LOBS_IS_DEFAULT_TBS_COL_ORDER,
                                  (const smiColumn**)&sIsDefaultTBSCol )
              != IDE_SUCCESS );

    makeMetaRangeSingleColumn(
        &sRangeColumn,
        (const mtcColumn*) sTableIdCol,
        (const void *) &aTableInfo->tableID,
        &sRange);

    SMI_CURSOR_PROP_INIT_FOR_META_INDEX_SCAN(&sCursorProperty, aSmiStmt->mStatistics);

    IDE_TEST(sCursor.open(
                 aSmiStmt,
                 gQcmLobs,
                 gQcmLobsIndex[QCM_LOBS_TABLEID_COLUMNID_IDX_ORDER],
                 smiGetRowSCN(gQcmLobs),
                 NULL,
                 &sRange,
                 smiGetDefaultKeyRange(),
                 smiGetDefaultFilter(),
                 QCM_META_CURSOR_FLAG,
                 SMI_SELECT_CURSOR,
                 &sCursorProperty) != IDE_SUCCESS);
    sStage = 1;

    IDE_TEST(sCursor.beforeFirst() != IDE_SUCCESS);
    IDE_TEST(sCursor.readRow(&sRow, &sRid, SMI_FIND_NEXT) != IDE_SUCCESS);

    while ( sRow != NULL )
    {
        i = * (UInt*) ((UChar*)sRow+sColumnIdCol->column.offset);
        i -= (aTableInfo->tableID * SMI_COLUMN_ID_MAXIMUM);

        sIsDefaultTBSStr = (mtdCharType*)
            ((UChar*)sRow + sIsDefaultTBSCol->column.offset);

        if (idlOS::strncmp((char*)sIsDefaultTBSStr->value,
                           (char*)"T",
                           sIsDefaultTBSStr->length) == 0)
        {
            aTableInfo->columns[i].flag |=
                QCM_COLUMN_LOB_DEFAULT_TBS_TRUE;
        }
        else
        {
            aTableInfo->columns[i].flag &=
                ~QCM_COLUMN_LOB_DEFAULT_TBS_TRUE;
        }

        IDE_TEST(sCursor.readRow(&sRow, &sRid, SMI_FIND_NEXT)
                 != IDE_SUCCESS);
    }

    sStage = 0;
    IDE_TEST(sCursor.close() != IDE_SUCCESS);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if ( sStage == 1 )
    {
        (void)sCursor.close();
    }

    return IDE_FAILURE;
}

IDE_RC qcm::checkMetaVersionAndDowngrade( smiStatement * aSmiStmt )
{
/***********************************************************************
 *
 * Description :
 *    PROJ-2689 Downgrade meta
 *
 * Implementation :
 *   1. Check that META tables exist.
 *   2. Check META Version
 *   3. If necessary, downgrade META tables.
 *
 ***********************************************************************/
    UInt                    sCurrMinorVersion = QCM_META_MINOR_VER;
    UInt                    sPrevMajorVersion = 0;
    UInt                    sPrevMinorVersion = 0;
    UInt                    sPrevPatchVersion = 0;
    const void            * sTable;
    ULong                 * sTableInfo;

    //----------------------------------------------------
    // 1. Check that META tables exist.
    //----------------------------------------------------

    sTableInfo = (ULong *)smiGetTableInfo( smiGetCatalogTable() );

    IDE_TEST_RAISE( sTableInfo == NULL, ERR_QCM_INIT_DB);

    sTable = smiGetTable( (smOID)(*sTableInfo) );

    // initalize global variable
    (void)initializeGlobalVariables( sTable );

    //----------------------------------------------------
    // 2. Check META Version
    // 3. If necessary, downgrade META tables.
    //----------------------------------------------------
    // get SYS_TABLES_ index handle
    IDE_TEST( getMetaIndex( gQcmTablesIndex,
                            gQcmTables ) != IDE_SUCCESS );

    // get table handle
    IDE_TEST( qcm::getMetaTable( QCM_DATABASE,
                                 & gQcmDatabase,
                                 aSmiStmt ) != IDE_SUCCESS );

    // check PREV META VERSION of DATABASE
    IDE_TEST( qcmDatabase::getMetaPrevVersion( aSmiStmt,
                                               &sPrevMajorVersion,
                                               &sPrevMinorVersion,
                                               &sPrevPatchVersion )
              != IDE_SUCCESS );

    ideLog::log(IDE_QP_0, "[QCM_META_DOWNGRADE] PREVIOUS META VERSION = "
                "%d.%d.%d\n",
                sPrevMajorVersion, sPrevMinorVersion, sPrevPatchVersion );

    ideLog::log(IDE_QP_0, "[QCM_META_DOWNGRADE] BINARY META VERSION = "
                "%d.%d.%d\n",
                QCM_META_MAJOR_VER, QCM_META_MINOR_VER, QCM_META_PATCH_VER );

    // A PREV META MAJOR VERSION of DATABASE differs from altibase binary.
    if ( sPrevMajorVersion != QCM_META_MAJOR_VER )
    {
        IDE_RAISE(ERR_QCM_META_MAJOR_VERSION_DIFFER);
    }
    else
    {
        // Nothing to do.
    }

    // A PREV META MAJOR VERSION of DATABASE differs from altibase binary.
    if ( sPrevMinorVersion != QCM_META_MINOR_VER )
    {
        if ( sPrevMinorVersion < QCM_META_MINOR_VER )
        {
            // initialize global meta handles ( table, index )
            IDE_TEST( initMetaHandles( aSmiStmt, &sCurrMinorVersion )
                      != IDE_SUCCESS );

            // make temporary qcmTableInfo of current tables
            IDE_TEST( initMetaCaches( aSmiStmt ) != IDE_SUCCESS );

            // downgrade META : A new minor version is set
            //                  in qcmCreate::downgradeMeta
            IDE_TEST( qcmCreate::downgradeMeta( aSmiStmt->mStatistics, sPrevMinorVersion )
                      != IDE_SUCCESS );

            ideLog::log(IDE_QP_0, "[QCM_META_DOWNGRADE] SUCCESS\n");
        }
        else
        {
            IDE_RAISE(ERR_QCM_META_MINOR_VERSION_HIGH);
        }
    }
    else
    {
        // Nothing to do.
        // Previous meta version is same as Binary meta version.
        ideLog::log(IDE_QP_0, "[QCM_META_DOWNGRADE] SUCCESS\n");
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_QCM_INIT_DB);
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QCM_INITDB));
    }
    IDE_EXCEPTION(ERR_QCM_META_MAJOR_VERSION_DIFFER);
    {
        ideLog::log(IDE_QP_0, "[QCM_META_DOWNGRADE] Privious Major Version differs from Binary Major Version.\n");
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QCM_INITDB));
    }
    IDE_EXCEPTION(ERR_QCM_META_MINOR_VERSION_HIGH);
    {
        ideLog::log(IDE_QP_0, "[QCM_META_DOWNGRADE] Privious Minor Version is higher than Binary Minor Version.\n");
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QCM_INITDB));
    }
    IDE_EXCEPTION_END;

    ideLog::log(IDE_QP_0, "[QCM_META_DOWNGRADE] FAILURE!!!\n");

    return IDE_FAILURE;
}

IDE_RC qcm::getTableIDAndTypeByName( smiStatement     * aSmiStmt,
                                     UInt               aUserID,
                                     UChar            * aTableName,
                                     SInt               aTableNameSize,
                                     UInt             * aTableID,
                                     qcmTableType     * aTableType )
{
/***********************************************************************
 *
 * Description :
 *    BUG-46284
 *    IN USER ID, TABLE NAME, OUT TABLE ID, TABLE TYPE
 *
 * Implementation :
 *
 ***********************************************************************/

    UInt                  sStage = 0;
    smiRange              sRange;
    smiTableCursor        sCursor;
    const void          * sRow                      = NULL;
    mtcColumn           * sQcmTablesUserIDColumn    = NULL;
    mtcColumn           * sQcmTablesTableNameColumn = NULL;
    mtcColumn           * sQcmTablesTableTypeColumn = NULL;
    mtcColumn           * sQcmTablesTableIDColumn   = NULL;
    UChar               * sTableType                = NULL;
    mtdCharType         * sTableTypeStr             = NULL;
    qtcMetaRangeColumn    sFirstRangeColumn;
    qtcMetaRangeColumn    sSecondRangeColumn;
    qcNameCharBuffer      sTableValueBuffer;
    scGRID                sRid; // Disk Table�� ���� Record IDentifier
    smiCursorProperties   sCursorProperty;
    mtdCharType         * sTableValue = ( mtdCharType * ) & sTableValueBuffer;
    
    IDE_TEST_RAISE( aTableNameSize > QC_MAX_OBJECT_NAME_LEN,
                    ERR_NOT_EXIST_TABLE );
        
    sCursor.initialize();

    // USER_ID
    IDE_TEST( smiGetTableColumns( gQcmTables,
                                  QCM_TABLES_USER_ID_COL_ORDER,
                                  (const smiColumn**)&sQcmTablesUserIDColumn )
              != IDE_SUCCESS );

    // TABLE_NAME
    IDE_TEST( smiGetTableColumns( gQcmTables,
                                  QCM_TABLES_TABLE_NAME_COL_ORDER,
                                  (const smiColumn**)&sQcmTablesTableNameColumn )
              != IDE_SUCCESS );

    // TABLE_ID
    IDE_TEST( smiGetTableColumns( gQcmTables,
                                  QCM_TABLES_TABLE_ID_COL_ORDER,
                                  (const smiColumn**)&sQcmTablesTableIDColumn )
              != IDE_SUCCESS );

    // TABLE_TYPE
    IDE_TEST( smiGetTableColumns( gQcmTables,
                                  QCM_TABLES_TABLE_TYPE_COL_ORDER,
                                  (const smiColumn**)&sQcmTablesTableTypeColumn )
              != IDE_SUCCESS );
    
    // mtdModule ����
    IDE_TEST(mtd::moduleById( &(sQcmTablesUserIDColumn->module),
                              sQcmTablesUserIDColumn->type.dataTypeId )
             != IDE_SUCCESS);

    // mtdModule ����
    IDE_TEST(mtd::moduleById( &(sQcmTablesTableNameColumn->module),
                              sQcmTablesTableNameColumn->type.dataTypeId )
             != IDE_SUCCESS);

    qtc::setVarcharValue( sTableValue,
                          NULL,
                          (SChar*)aTableName,
                          aTableNameSize );

    makeMetaRangeDoubleColumn(
        &sFirstRangeColumn,
        &sSecondRangeColumn,
        (const mtcColumn *) sQcmTablesTableNameColumn,
        (const void *) sTableValue,
        (const mtcColumn *) sQcmTablesUserIDColumn,
        (const void *) &aUserID,
        &sRange);

    SMI_CURSOR_PROP_INIT_FOR_META_INDEX_SCAN(&sCursorProperty, NULL);

    IDE_TEST(sCursor.open(
                 aSmiStmt,
                 gQcmTables,
                 gQcmTablesIndex[QCM_TABLES_TABLENAME_USERID_IDX_ORDER],
                 smiGetRowSCN(gQcmTables),
                 NULL,
                 &sRange,
                 smiGetDefaultKeyRange(),
                 smiGetDefaultFilter(),
                 QCM_META_CURSOR_FLAG,
                 SMI_SELECT_CURSOR,
                 &sCursorProperty) != IDE_SUCCESS);
    sStage = 1;

    IDE_TEST(sCursor.beforeFirst() != IDE_SUCCESS);

    IDE_TEST(sCursor.readRow(&sRow, &sRid, SMI_FIND_NEXT) != IDE_SUCCESS);

    IDE_TEST_RAISE(sRow == NULL, ERR_NOT_EXIST_TABLE);

    // TABLE_ID
    *aTableID = *(UInt *) ((UChar *) sRow +
                          sQcmTablesTableIDColumn->column.offset);

    // TABLE_TYPE     
    sTableTypeStr = (mtdCharType*)
        ((UChar*) sRow + sQcmTablesTableTypeColumn->column.offset);
    sTableType = sTableTypeStr->value;
    
    if ( idlOS::strMatch( (SChar*)sTableType, sTableTypeStr->length, "T", 1 ) == 0 )
    {
        *aTableType = QCM_USER_TABLE;
    }
    else if ( idlOS::strMatch( (SChar*)sTableType, sTableTypeStr->length, "D", 1 ) == 0 )
    {
        *aTableType = QCM_DICTIONARY_TABLE;
    }
    else if ( idlOS::strMatch( (SChar*)sTableType, sTableTypeStr->length, "Q", 1 ) == 0 )
    {
        *aTableType = QCM_QUEUE_TABLE;
    }
    else if ( idlOS::strMatch( (SChar*)sTableType, sTableTypeStr->length, "V", 1 ) == 0 )
    {
        *aTableType = QCM_VIEW;
    }
    else if ( idlOS::strMatch( (SChar*)sTableType, sTableTypeStr->length, "M", 1 ) == 0 )
    {
        *aTableType = QCM_MVIEW_TABLE;
    }
    else if ( idlOS::strMatch( (SChar*)sTableType, sTableTypeStr->length, "A", 1 ) == 0 )
    {
        *aTableType = QCM_MVIEW_VIEW;
    }
    else if ( idlOS::strMatch( (SChar*)sTableType, sTableTypeStr->length, "G", 1 ) == 0 )
    {
        *aTableType = QCM_INDEX_TABLE;
    }
    else if ( idlOS::strMatch( (SChar*)sTableType, sTableTypeStr->length, "E", 1 ) == 0 )
    {
        *aTableType = QCM_SEQUENCE_TABLE;
    }
    else if ( idlOS::strMatch( (SChar*)sTableType, sTableTypeStr->length, "R", 1 ) == 0 )
    {
        *aTableType = QCM_RECYCLEBIN_TABLE;
    }
    else
    {
        IDE_RAISE(ERR_META_CRASH);
    }
    
    sStage = 0;
    IDE_TEST(sCursor.close() != IDE_SUCCESS);

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_NOT_EXIST_TABLE);
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QCM_NOT_EXIST_TABLE));
    }
    IDE_EXCEPTION( ERR_META_CRASH );
    {            
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qcm::getTableIDAndTypeByName",
                                  "meta crash" ));
    }        
    IDE_EXCEPTION_END;

    if ( sStage == 1 )
    {
        (void)sCursor.close();
    }

    return IDE_FAILURE;
}

idBool qcm::existGlobalNonPartitionedIndex( qcmTableInfo * aTableInfo, qcmIndex ** aIndex )
{
    UInt       i      = 0;
    idBool     sExist = ID_FALSE;
    qcmIndex * sIndex = NULL;

    if ( aTableInfo->tablePartitionType == QCM_PARTITIONED_TABLE )
    {
        for ( i = 0; i < aTableInfo->indexCount; i++ )
        {
            sIndex = &( aTableInfo->indices[i] );
            if ( sIndex->indexPartitionType == QCM_NONE_PARTITIONED_INDEX )
            {
                break;
            }
            else
            {
                sIndex = NULL;
            }
        }
    }

    if ( sIndex != NULL )
    {
        sExist = ID_TRUE;
    }
    else
    {
        sExist = ID_FALSE;
    }

    *aIndex = sIndex;

    return sExist;
}

/* TASK-7217 Sharded sequence */
void qcm::setShardNodeID( UInt aNodeID )
{
    SLong sScale = 1;
    UInt i;

    for ( i = 0; i < QCM_MAX_SHARDED_SEQUENCE_SCALE; i++)
    {
        gShardNodeID[i] = aNodeID * sScale;
        sScale = sScale * 10;
    }
}

void qcm::unsetShardNodeID( void )
{
    UInt i;

    for ( i = 0; i < QCM_MAX_SHARDED_SEQUENCE_SCALE; i++)
    {
        gShardNodeID[i] = 0;
    }
}

SLong qcm::getShardNodeID(UInt aScale)
{
    return gShardNodeID[aScale];
}
