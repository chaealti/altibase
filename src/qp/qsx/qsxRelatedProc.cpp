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
 * $Id: qsxRelatedProc.cpp 89989 2021-02-15 04:54:24Z donovan.seo $
 **********************************************************************/

#include <idl.h>
#include <ide.h>
#include <idu.h>
#include <idx.h>
#include <qcuProperty.h>
#include <qcg.h>
#include <qsx.h>
#include <qsParseTree.h>
#include <qsxProc.h>
#include <qsxRelatedProc.h>
#include <qcmProc.h>
#include <qcmPkg.h>
#include <qsvEnv.h>
#include <qcuSqlSourceInfo.h>

IDE_RC qsxRelatedProc::latchObjects( qsxProcPlanList * aObjectPlanList )
{
    qsxProcPlanList  * sObjectPlanList;

    // BUG-41227 source select statement of merge query containing function
    //           reference already freed session information
    UInt               sLatchCount = 0;

    smiTrans           sMetaTx;
    smiStatement     * sDummySmiStmt;
    smiStatement       sSmiStmt;
    UInt               sState = 0;

    /* PROJ-2268 Reuse Catalog Table Slot
     * Object�� Latch ��� ���� Ager�� �������� ���ϵ��� MinViewSCN�� ������ �ش�.
     * MinViewSCN�� ������� �̹� Drop ó���Ǿ��ٸ� 
     * Validate���� Object_Changed Error �� ��ȯ �� ���̰�
     * MinViewSCN�� ���� ���Ŀ� Drop �ȴٸ� Ager���� ó������ ���ϱ� ������
     * ���������� Latch�� ȹ���� �� �ִ�.
     * Validation ������ Latch ȹ�� ���� �� Drop �� ���� Latch �Լ� ���ο��� ó���ȴ�. */
    if ( smuProperty::getCatalogSlotReusable()  == 1 )
    {
        IDE_TEST( sMetaTx.initialize() != IDE_SUCCESS );
        sState = 1;

        IDE_TEST( sMetaTx.begin( &sDummySmiStmt,
                                 NULL,
                                 (SMI_TRANSACTION_UNTOUCHABLE |
                                  SMI_ISOLATION_CONSISTENT    |
                                  SMI_COMMIT_WRITE_NOWAIT) )
                  != IDE_SUCCESS);
        sState = 2;

        IDE_TEST( sSmiStmt.begin( NULL,
                                  sDummySmiStmt,
                                  SMI_STATEMENT_UNTOUCHABLE |
                                  SMI_STATEMENT_MEMORY_CURSOR )
                  != IDE_SUCCESS);
        sState = 3;
    }
    else
    {
        /* nothing to do ... */
    }

    for( sObjectPlanList = aObjectPlanList;
         sObjectPlanList != NULL;
         sObjectPlanList = sObjectPlanList->next )
    {
        if ( (sObjectPlanList->objectType == QS_PROC) ||
             (sObjectPlanList->objectType == QS_FUNC) ||
             (sObjectPlanList->objectType == QS_TYPESET) )
        {
            /* PROJ-2268 Reuse Catalog Table Slot */
            IDE_TEST_RAISE( smiValidatePlanHandle( smiGetTable( sObjectPlanList->objectID ),
                                                   sObjectPlanList->objectSCN ) 
                            != IDE_SUCCESS, err_object_changed );
           
            IDE_TEST( qsxProc::latchS ( sObjectPlanList->objectID )
                != IDE_SUCCESS );
            sLatchCount++;
        }
        else
        {
            if ( sObjectPlanList->objectType == QS_PKG )
            {
                /* PROJ-2268 Reuse Catalog Table Slot */
                IDE_TEST_RAISE( smiValidatePlanHandle( smiGetTable( sObjectPlanList->objectID ),
                                                       sObjectPlanList->objectSCN ) 
                                != IDE_SUCCESS, err_object_changed );

                IDE_TEST( qsxPkg::latchS ( sObjectPlanList->objectID )
                          != IDE_SUCCESS );
                sLatchCount++;

                /* BUG-38720 unlatch for package body */
                if ( sObjectPlanList->pkgBodyOID != QS_EMPTY_OID )
                {
                    /* PROJ-2268 Reuse Catalog Table Slot */
                    IDE_TEST_RAISE( smiValidatePlanHandle( smiGetTable( sObjectPlanList->pkgBodyOID ),
                                                           sObjectPlanList->pkgBodySCN ) 
                                    != IDE_SUCCESS, err_object_changed );

                    IDE_TEST( qsxPkg::latchS( sObjectPlanList->pkgBodyOID )
                              != IDE_SUCCESS );
                    sLatchCount++;
                }
                else
                {
                    // Nothing to do.
                }
            }
            else
            {
                IDE_DASSERT( sObjectPlanList->objectType != QS_PKG_BODY );
                // Nothing to do.
            }
        }
    }

    if ( smuProperty::getCatalogSlotReusable()  == 1 )
    {
        sState = 2;
        IDE_TEST( sSmiStmt.end( SMI_STATEMENT_RESULT_SUCCESS ) != IDE_SUCCESS );

        sState = 1;
        IDE_TEST( sMetaTx.commit() != IDE_SUCCESS );

        sState = 0;
        IDE_TEST( sMetaTx.destroy( NULL ) != IDE_SUCCESS );
    }
    else
    {
        /* nothing to do ... */
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION(err_object_changed);
    {
        IDE_SET(ideSetErrorCode( qpERR_ABORT_QSX_PLAN_CHANGED ));
    }
    IDE_EXCEPTION_END;

    for( sObjectPlanList = aObjectPlanList;
         (sObjectPlanList != NULL) && (sLatchCount > 0);
         sObjectPlanList = sObjectPlanList->next )
    {
        if ( (sObjectPlanList->objectType == QS_PROC) ||
             (sObjectPlanList->objectType == QS_FUNC) ||
             (sObjectPlanList->objectType == QS_TYPESET) )
        {
            if ( qsxProc::unlatch( sObjectPlanList->objectID )
                 != IDE_SUCCESS )
            {
                IDE_ERRLOG(IDE_QP_1);
            }
            sLatchCount--;
        }
        else
        {
            if ( sObjectPlanList->objectType == QS_PKG )
            {
                if ( qsxPkg::unlatch( sObjectPlanList->objectID )
                     != IDE_SUCCESS )
                {
                    IDE_ERRLOG(IDE_QP_1);
                }
                sLatchCount--;

                if ( (sObjectPlanList->pkgBodyOID != QS_EMPTY_OID) &&
                     (sLatchCount > 0) )
                {
                    if ( qsxPkg::unlatch( sObjectPlanList->pkgBodyOID )
                         != IDE_SUCCESS )
                    {
                        IDE_ERRLOG(IDE_QP_1);
                    }
                    sLatchCount--;
                }
                else
                {
                    // Nothing to do.
                }
            }
            else
            {
                // Nothing to do.
            }
        }
    }

    switch( sState )
    {
        case 3:
            IDE_ASSERT( sSmiStmt.end( SMI_STATEMENT_RESULT_FAILURE ) == IDE_SUCCESS );

        case 2:
            IDE_ASSERT( sMetaTx.commit() == IDE_SUCCESS );

        case 1:
            IDE_ASSERT( sMetaTx.destroy( NULL ) == IDE_SUCCESS );

        case 0:
            //nothing
            break;
        default:
            IDE_CALLBACK_FATAL("Can't be here");
    }

    return IDE_FAILURE;
}

IDE_RC qsxRelatedProc::checkObjects( qsxProcPlanList * aObjectPlanList )
{
    qsxProcInfo      * sProcInfo     = NULL;
    qsxPkgInfo       * sPkgSpecInfo  = NULL;
    qsxPkgInfo       * sPkgBodyInfo  = NULL;

    for( ;
         aObjectPlanList != NULL;
         aObjectPlanList = aObjectPlanList->next )
    {
        if ( (aObjectPlanList->objectType == QS_PROC) ||
             (aObjectPlanList->objectType == QS_FUNC) ||
             (aObjectPlanList->objectType == QS_TYPESET) )
        {
            IDE_TEST( qsxProc::getProcInfo ( aObjectPlanList->objectID,
                                             & sProcInfo )
                      != IDE_SUCCESS );

            IDE_TEST_RAISE( sProcInfo == NULL, err_object_dropped );

            IDE_TEST_RAISE( sProcInfo->isValid != ID_TRUE, err_object_invalid );

            IDE_TEST_RAISE( aObjectPlanList->modifyCount !=
                            sProcInfo->modifyCount, err_object_changed );
        }
        else
        {
            if ( aObjectPlanList->objectType == QS_PKG )
            {
                IDE_TEST( qsxPkg::getPkgInfo( aObjectPlanList->objectID,
                                              & sPkgSpecInfo )
                          != IDE_SUCCESS );

                IDE_TEST_RAISE( sPkgSpecInfo == NULL, err_object_dropped );

                IDE_TEST_RAISE( sPkgSpecInfo->isValid != ID_TRUE, err_object_invalid );

                IDE_TEST_RAISE( aObjectPlanList->modifyCount !=
                                sPkgSpecInfo->modifyCount, err_object_changed );

                if ( aObjectPlanList->pkgBodyOID != QS_EMPTY_OID )
                {
                    IDE_TEST( qsxPkg::getPkgInfo( aObjectPlanList->pkgBodyOID,
                                                  & sPkgBodyInfo )
                              != IDE_SUCCESS );

                    IDE_TEST_RAISE( sPkgBodyInfo == NULL, err_object_dropped );

                    IDE_TEST_RAISE( sPkgBodyInfo->isValid != ID_TRUE, err_object_invalid );

                    IDE_TEST_RAISE( aObjectPlanList->pkgBodyModifyCount !=
                                    sPkgBodyInfo->modifyCount, err_object_changed );
                }
                else
                {
                    // Nothing to do.
                }
            }
            else
            {
                // Nothing to do.
            }
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION(err_object_dropped);
    {
        IDE_SET(ideSetErrorCode( qpERR_ABORT_QSX_PLAN_DROPPED ));
    }
    IDE_EXCEPTION(err_object_invalid);
    {
        IDE_SET(ideSetErrorCode( qpERR_ABORT_QSX_PLAN_INVALID ));
    }
    IDE_EXCEPTION(err_object_changed);
    {
        IDE_SET(ideSetErrorCode( qpERR_ABORT_QSX_PLAN_CHANGED ));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


IDE_RC qsxRelatedProc::unlatchObjects( qsxProcPlanList * aObjectPlanList )
{
    for( ;
         aObjectPlanList != NULL;
         aObjectPlanList = aObjectPlanList->next )
    {
        if ( (aObjectPlanList->objectType == QS_PROC) ||
             (aObjectPlanList->objectType == QS_FUNC) ||
             (aObjectPlanList->objectType == QS_TYPESET) )
        {
            IDE_TEST( qsxProc::unlatch ( aObjectPlanList->objectID )
                      != IDE_SUCCESS );
        }
        else
        {
            if ( aObjectPlanList->objectType == QS_PKG )
            {
                IDE_TEST( qsxPkg::unlatch ( aObjectPlanList->objectID )
                          != IDE_SUCCESS );

                /* BUG-38720 unlatch for package body */
                if ( aObjectPlanList->pkgBodyOID != QS_EMPTY_OID )
                {
                    IDE_TEST( qsxPkg::unlatch( aObjectPlanList->pkgBodyOID )
                        != IDE_SUCCESS );
                }
                else
                {
                    // Nothing to do.
                }
            }
            else
            {
                // Nothing to do.
            }
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qsxRelatedProc::findPlanTree(
    qcStatement        * aQcStmt,
    qsOID                aObjectID,
    qsxProcPlanList   ** aObjectPlan       /* out */
    )
{
    qsxProcPlanList * sObjectPlanList;
    qsxProcPlanList * sFoundObjectPlan = NULL;

    for (sObjectPlanList = aQcStmt->spvEnv->procPlanList;
         sObjectPlanList != NULL;
         sObjectPlanList = sObjectPlanList->next)
    {
        if ( sObjectPlanList->objectID == aObjectID )
        {
            sFoundObjectPlan = sObjectPlanList;
            break;
        }
    }

    IDE_TEST_RAISE( sFoundObjectPlan == NULL,
                    err_plan_tree_not_found );

    *aObjectPlan = sFoundObjectPlan;

    return IDE_SUCCESS;

    IDE_EXCEPTION(err_plan_tree_not_found);
    {
        IDE_SET(ideSetErrorCode(
                qpERR_ABORT_QSX_INTERNAL_SERVER_ERROR_ARG,
                "[qsxRelatedProc::findPlanTree] planTree is not found" ));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*
 * aIduMem :  for prepared execution, qcpMemMgr
 *                for direct execution, qcxMemMgr
 */
IDE_RC qsxRelatedProc::prepareRelatedPlanTree (
    qcStatement         * aQcStmt,
    qsOID                 aObjectID,
    SInt                  aObjType,
    qsxProcPlanList    ** aObjectPlanList
    )
{
    qsxProcPlanList * sObjectPlanListTail;

    // doPrepareRelatedPlanTree uses only procOID memober of qsRelatedObjects.
    qsRelatedObjects  sObjectIdList;

    sObjectIdList.objectID     = aObjectID;
    sObjectIdList.objectType   = aObjType;
    sObjectIdList.next         = NULL;

    // TASK-3876 codesonar
    IDE_TEST_RAISE( aObjectPlanList == NULL,
                    ERR_INVALID_ARGUMENT );

    sObjectPlanListTail = *aObjectPlanList;

    while (1)
    {
        if ( sObjectPlanListTail == NULL )
        {
            break;
        }
        if ( sObjectPlanListTail->next == NULL )
        {
            break;
        }

        sObjectPlanListTail = sObjectPlanListTail->next;
    }

    IDE_TEST( qsxRelatedProc::doPrepareRelatedPlanTree (
            aQcStmt,
            &sObjectIdList,
            aObjectPlanList /* ALL */ )
        != IDE_SUCCESS);

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_INVALID_ARGUMENT )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qsxRelatedProc::prepareRelatedPlanTree",
                                  "aObjectPlanList is NULL" ));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*
 * aIduMem :  for prepared execution, qcpMemMgr
 *                for direct execution, qcxMemMgr
 */
IDE_RC qsxRelatedProc::doPrepareRelatedPlanTree (
    qcStatement         * aQcStmt,
    qsRelatedObjects    * aObjectIdList,
    qsxProcPlanList    ** aAllObjectPlanListHead /* out */ )
{
    qsxProcPlanList    * sNewObjectPlanList         = NULL;
    qsRelatedObjects   * sCurObjectIdList           = NULL;
    qsRelatedObjects   * sRelatedObjectIdList       = NULL;

    qsxProcPlanList    * sObjectPlanNode            = NULL;
    idBool               sSamePlanFound             = ID_FALSE;
    iduVarMemList      * sMemory = NULL;

    if (aObjectIdList != NULL)
    {
        for (sCurObjectIdList = aObjectIdList;
             sCurObjectIdList != NULL;
             sCurObjectIdList = sCurObjectIdList->next)
        {
            if ( ( sCurObjectIdList->objectType != QS_PROC ) &&
                 ( sCurObjectIdList->objectType != QS_FUNC ) &&
                 ( sCurObjectIdList->objectType != QS_TYPESET ) &&
                 ( sCurObjectIdList->objectType != QS_PKG ) &&
                 ( sCurObjectIdList->objectType != QS_PKG_BODY ) )
            {
                continue;
            }

            // check if the plan already found
            if (*aAllObjectPlanListHead != NULL)
            {
                sSamePlanFound = ID_FALSE;
                for (sObjectPlanNode = *aAllObjectPlanListHead;
                     sObjectPlanNode != NULL;
                     sObjectPlanNode = sObjectPlanNode->next )
                {
                    // same procedure found.
                    if ( sObjectPlanNode->objectID == sCurObjectIdList->objectID )
                    {
                        sSamePlanFound = ID_TRUE;
                        break;
                    }
                }
                // continue to next proc plan
                if (sSamePlanFound == ID_TRUE)
                {
                    continue;
                }
            }

            if( ( sCurObjectIdList->objectType == QS_PROC ) || 
                ( sCurObjectIdList->objectType == QS_FUNC ) ||
                ( sCurObjectIdList->objectType == QS_TYPESET ) )
            {
                // BUG-31450
                // 1. exec proc
                if( aQcStmt->spvEnv->createProc == NULL )
                {
                    sMemory = aQcStmt->myPlan->qmpMem;
                }
                else
                {
                    // 2. ���ν��� �����ÿ� ���� PVO �ϴ� ���
                    if( aQcStmt->spvEnv->createProc->procInfo == NULL )
                    {
                        sMemory = aQcStmt->myPlan->qmpMem;
                    }
                    else
                    {
                        // 3. ���ν��� �����ÿ� �ι�°�� PVO �ϴ� ���
                        // qsxProcInfo�� relatedObjects�� �Ҵ�Ǵ� �޸𸮴� qmsMem �� ���
                        // �Ǿ�� �Ѵ�.
                        // �� statement�� qmpMem�� ���Ǵ� ��� �ش� statement�� �����
                        // �Ǹ� ������ �Ҵ��� �޸� ������ ��� �����ϱ� �����̴�.
                        sMemory = aQcStmt->spvEnv->createProc->procInfo->qmsMem;
                    }
                }

                // make new node
                IDE_TEST(sMemory->cralloc(
                        idlOS::align8((UInt) ID_SIZEOF(qsxProcPlanList) ),
                        (void**)&sNewObjectPlanList)
                    != IDE_SUCCESS);

                sNewObjectPlanList->objectID           = sCurObjectIdList->objectID;
                sNewObjectPlanList->objectType         = sCurObjectIdList->objectType; /* BUG-38844 */
                sNewObjectPlanList->objectSCN          = smiGetRowSCN( smiGetTable( sCurObjectIdList->objectID ) ); /* PROJ-2268 */
                sNewObjectPlanList->pkgBodyOID         = QS_EMPTY_OID;
                sNewObjectPlanList->pkgBodyModifyCount = 0;
                sNewObjectPlanList->pkgBodySCN         = SM_SCN_INIT;
                sNewObjectPlanList->next               = NULL;

                IDE_TEST( preparePlanTree(
                        aQcStmt,
                        sNewObjectPlanList,
                        & sRelatedObjectIdList )
                    != IDE_SUCCESS );

            }
            else if( ( sCurObjectIdList->objectType == QS_PKG ) ||
                     ( sCurObjectIdList->objectType == QS_PKG_BODY ) )
            {
                // BUG-31450
                // 1. exec package.variable
                if( aQcStmt->spvEnv->createPkg == NULL )
                {
                    sMemory = aQcStmt->myPlan->qmpMem;
                }
                else
                {
                    // 2. ���ν��� �����ÿ� ���� PVO �ϴ� ���
                    if( aQcStmt->spvEnv->createPkg->pkgInfo == NULL )
                    {
                        sMemory = aQcStmt->myPlan->qmpMem;
                    }
                    else
                    {
                        // 3. ���ν��� �����ÿ� �ι�°�� PVO �ϴ� ���
                        // qsxProcInfo�� relatedObjects�� �Ҵ�Ǵ� �޸𸮴� qmsMem �� ���
                        // �Ǿ�� �Ѵ�.
                        // �� statement�� qmpMem�� ���Ǵ� ��� �ش� statement�� �����
                        // �Ǹ� ������ �Ҵ��� �޸� ������ ��� �����ϱ� �����̴�.
                        sMemory = aQcStmt->spvEnv->createPkg->pkgInfo->qmsMem;
                    }
                }

                // make new node
                IDE_TEST(sMemory->cralloc(
                        idlOS::align8((UInt) ID_SIZEOF(qsxProcPlanList) ),
                        (void**)&sNewObjectPlanList)
                    != IDE_SUCCESS);

                sNewObjectPlanList->objectID           = sCurObjectIdList->objectID;
                sNewObjectPlanList->objectType         = sCurObjectIdList->objectType; /* BUG-38844 */
                sNewObjectPlanList->objectSCN          = smiGetRowSCN( smiGetTable( sCurObjectIdList->objectID ) ); /* PROJ-2268 */
                sNewObjectPlanList->pkgBodyOID         = QS_EMPTY_OID;
                sNewObjectPlanList->pkgBodyModifyCount = 0;
                sNewObjectPlanList->pkgBodySCN         = SM_SCN_INIT;
                sNewObjectPlanList->next               = NULL;

                IDE_TEST( preparePkgPlanTree( aQcStmt,
                                              sCurObjectIdList->objectType,
                                              sNewObjectPlanList,
                                              & sRelatedObjectIdList )
                          != IDE_SUCCESS );

                /* BUG-39770
                   package�� �����ϰų�, package�� �����ϴ� ��ü�� ������ ���
                   parallel�ϰ� �������� �� �Ѵ�. */
                if ( (aQcStmt->spvEnv->createProc != NULL) &&
                     (aQcStmt->spvEnv->createPkg == NULL) )
                {
                    aQcStmt->spvEnv->createProc->referToPkg = ID_TRUE;
                }
                else
                {
                    // Nothing to do.
                }
            }
            else
            {
                // Nothing to do.
            }

            // connect procPlanList
            // proc-1535
            // preparePlanTree�� ������ proc�� �߰��ȴ�.
            if (*aAllObjectPlanListHead == NULL)
            {
                *aAllObjectPlanListHead = sNewObjectPlanList;
            }
            else
            {
                for (sObjectPlanNode = *aAllObjectPlanListHead;
                     sObjectPlanNode->next != NULL;
                     sObjectPlanNode = sObjectPlanNode->next );

                sObjectPlanNode->next = sNewObjectPlanList;
            }

            // when NULL, base condition.
            if (sRelatedObjectIdList != NULL)
            {
                if ( doPrepareRelatedPlanTree(
                        aQcStmt,
                        sRelatedObjectIdList,
                        aAllObjectPlanListHead )
                    != IDE_SUCCESS )
                {
                    IDE_TEST( 1 != 0 );
                }
            }
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qsxRelatedProc::preparePlanTree (
    qcStatement        * aQcStmt,
    qsxProcPlanList    * aObjectPlan,         /* member out */
    qsRelatedObjects  ** aRelatedObjects    /* out */
    )
{
    //PROJ-1677 DEQ
    qsxProcObjectInfo * sProcObjectInfo    = NULL;
    qsxProcInfo       * sProcInfo          = NULL;
    qsOID               sProcOID           = QS_EMPTY_OID;
    SInt                sLatchState   = 0;
    SInt                sProcState    = 0;
    qcuSqlSourceInfo    sSqlInfo;
    qsCallSpec        * sCallSpec;

    sProcOID    = aObjectPlan->objectID;

    IDE_TEST( qsxProc::getProcObjectInfo( sProcOID,
                                          & sProcObjectInfo )
              != IDE_SUCCESS );

    // on boot up case.
    if ( sProcObjectInfo == NULL )
    {
        IDE_TEST( qsxProc::createProcObjectInfo( sProcOID, & sProcObjectInfo )
                  != IDE_SUCCESS );
        sProcState = 1;

        IDE_TEST( qsxProc::setProcObjectInfo( sProcOID,
                                              sProcObjectInfo )
                  != IDE_SUCCESS );

        IDE_TEST( qsxProc::createProcInfo( sProcOID, & sProcInfo ) // To Fix BUG-19839 
                  != IDE_SUCCESS );
        sProcState = 2;

        IDE_TEST( qsxProc::setProcInfo( sProcOID, sProcInfo )
                  != IDE_SUCCESS );
        sProcState = 0;
    }

    /* PROJ-2268 Reuse Catalog Table Slot */
    IDE_TEST_RAISE( smiValidatePlanHandle( smiGetTable( sProcOID ),
                                           aObjectPlan->objectSCN ) 
                    != IDE_SUCCESS, err_object_changed );

    IDE_TEST( qsxProc::latchS ( sProcOID )
              != IDE_SUCCESS );
    sLatchState = 1;

    IDE_TEST( qsxProc::getProcInfo ( sProcOID, &sProcInfo )
              != IDE_SUCCESS );

    if (sProcInfo->isValid != ID_TRUE)
    {
        // proj-1535
        // upgrade to exclusive latch
        sLatchState = 0;
        IDE_TEST( qsxProc::unlatch ( sProcOID )
                  != IDE_SUCCESS );

        IDE_TEST( qsxProc::latchXForRecompile( sProcOID ) != IDE_SUCCESS );
        sLatchState = 1;

        IDE_TEST( qsxProc::getProcInfo ( sProcOID, &sProcInfo )
                  != IDE_SUCCESS );

        if ( sProcInfo->isValid != ID_TRUE )
        {

            IDE_TEST_RAISE( qsx::doRecompile( aQcStmt,
                                              sProcOID,
                                              sProcInfo,
                                              ID_TRUE )
                            != IDE_SUCCESS,
                            RECOMPILE_ERR );
        }
        else
        {
            // Nothing to do.
        }

        // proj-1535
        // downgrade to shared latch
        sLatchState = 0;
        IDE_TEST( qsxProc::unlatch ( sProcOID )
                  != IDE_SUCCESS );

        IDE_TEST( qsxProc::latchS ( sProcOID )
                  != IDE_SUCCESS );
        sLatchState = 1;

        IDE_TEST( qsxProc::getProcInfo ( sProcOID, &sProcInfo )
                  != IDE_SUCCESS );

        // PROJ-2717 Internal procedure
        //   Library�� load���� ���߰ų�, function�� ����� ã�� ���Ѱ�� ����
        if ( sProcInfo->planTree->procType == QS_INTERNAL_C )
        {
            sCallSpec = sProcInfo->planTree->expCallSpec;

            IDE_TEST_RAISE( sCallSpec->libraryNode == NULL,
                            ERR_LIB_NOT_FOUND );

            IDE_TEST_RAISE( sCallSpec->libraryNode->mHandle == PDL_SHLIB_INVALID_HANDLE,
                            ERR_LIB_NOT_FOUND );

            IDE_TEST_RAISE( sCallSpec->libraryNode->mFunctionPtr == NULL,
                            ERR_ENTRY_FUNCTION_NOT_FOUND );

            IDE_TEST_RAISE( sCallSpec->functionPtr == NULL,
                            ERR_FUNCTION_NOT_FOUND );
        }

        IDE_TEST_RAISE( sProcInfo->isValid != ID_TRUE, err_object_invalid );
    }
    else
    {
        // Nothing to do.
    }

    aObjectPlan->planTree    = sProcInfo->planTree;
    aObjectPlan->modifyCount = sProcInfo->modifyCount;

    *aRelatedObjects         = sProcInfo->relatedObjects;

    return IDE_SUCCESS;

    IDE_EXCEPTION(err_object_changed);
    {
        IDE_SET(ideSetErrorCode( qpERR_ABORT_QSX_PLAN_CHANGED ));
    }
    // BUG-43979
    // PSM ������ ���� recompile �߿� ������ �߻��� ���, �̸� ����ڿ��� ��������� �˸��ϴ�.
    IDE_EXCEPTION( RECOMPILE_ERR )
    {
        if ( ( sProcInfo->planTree != NULL ) &&
             ( ideGetErrorCode() != qpERR_ABORT_QSX_INVALID_OBJ_RECOMPILE_FAILED ) )
        {
            (void)sSqlInfo.initWithBeforeMessage( QC_QME_MEM(aQcStmt) );
                IDE_SET(
                    ideSetErrorCode(qpERR_ABORT_QSX_INVALID_OBJ_RECOMPILE_FAILED,
                                    "\n\n", sSqlInfo.getBeforeErrMessage() ) );
            (void)sSqlInfo.fini();
        }
    }
    IDE_EXCEPTION( err_object_invalid );
    {
        IDE_SET( ideSetErrorCode(qpERR_ABORT_QSX_PLAN_INVALID) );
    }
    IDE_EXCEPTION( ERR_LIB_NOT_FOUND )
    {
        ideSetErrorCode( idERR_ABORT_IDX_LIBRARY_NOT_FOUND,
                         idxLocalSock::mHomePath,
                         IDX_LIB_DEFAULT_DIR,
                         sCallSpec->libraryNode->mLibPath );
    }
    IDE_EXCEPTION( ERR_ENTRY_FUNCTION_NOT_FOUND )
    {
        ideSetErrorCode( idERR_ABORT_IDX_ENTRY_FUNCTION_NOT_FOUND );
    }
    IDE_EXCEPTION( ERR_FUNCTION_NOT_FOUND )
    {
        ideSetErrorCode( idERR_ABORT_IDX_FUNCTION_NOT_FOUND );
    }
    IDE_EXCEPTION_END;

    switch( sProcState )
    {
        case 2:
            if ( qsxProc::destroyProcInfo ( & sProcInfo ) != IDE_SUCCESS )
            {
                IDE_ERRLOG(IDE_QP_1);
            }
            /* fall through */
        case 1:
            if ( qsxProc::destroyProcObjectInfo ( & sProcObjectInfo )
                 != IDE_SUCCESS )
            {
                IDE_ERRLOG(IDE_QP_1);
            }
            break;
        default:
            break;
    }

    switch( sLatchState )
    {
        case 1:
            if ( qsxProc::unlatch ( sProcOID ) != IDE_SUCCESS)
            {
                IDE_ERRLOG(IDE_QP_1);
            }
            break;
        default:
            break;
    }

    return IDE_FAILURE;
}

IDE_RC qsxRelatedProc::preparePkgPlanTree(
    qcStatement        * aQcStmt,
    SInt                 aObjectType,
    qsxProcPlanList    * aObjectPlan,        /* member out */
    qsRelatedObjects  ** aRelatedObjects )          /* out */
{
    qsxPkgObjectInfo * sPkgObjectInfo     = NULL;
    qsxPkgObjectInfo * sPkgBodyObjectInfo = NULL;
    qsxPkgInfo       * sPkgInfo           = NULL;
    qsxPkgInfo       * sPkgBodyInfo       = NULL;
    qsOID              sPkgOID            = QS_EMPTY_OID;
    qsOID              sPkgBodyOID        = QS_EMPTY_OID;
    SInt               sPkgLatchState     = 0;
    SInt               sPkgBodyLatchState = 0;
    SInt               sPkgState          = 0;
    SInt               sPkgBodyState      = 0;
    qciStmtType        sTopStmtKind;
    qcuSqlSourceInfo   sSqlInfo;

    IDE_DASSERT( QC_SHARED_TMPLATE(aQcStmt) != NULL );
    IDE_DASSERT( QC_SHARED_TMPLATE(aQcStmt)->stmt != NULL );
    IDE_DASSERT( QC_SHARED_TMPLATE(aQcStmt)->stmt->myPlan != NULL );
    IDE_DASSERT( QC_SHARED_TMPLATE(aQcStmt)->stmt->myPlan->parseTree != NULL );

    sTopStmtKind = QC_SHARED_TMPLATE(aQcStmt)->stmt->myPlan->parseTree->stmtKind;
    sPkgOID      = aObjectPlan->objectID;

    IDE_TEST( qsxPkg::getPkgObjectInfo( sPkgOID, & sPkgObjectInfo )
              != IDE_SUCCESS );

    // on boot up case.
    if ( sPkgObjectInfo == NULL )
    {
        IDE_TEST( qsxPkg::createPkgObjectInfo( sPkgOID, & sPkgObjectInfo )
                  != IDE_SUCCESS );
        sPkgState = 1;

        IDE_TEST( qsxPkg::setPkgObjectInfo( sPkgOID, sPkgObjectInfo )
                  != IDE_SUCCESS );

        IDE_TEST( qsxPkg::createPkgInfo( sPkgOID, & sPkgInfo ) // To Fix BUG-19839
                  != IDE_SUCCESS );
        sPkgState = 2;

        IDE_TEST( qsxPkg::setPkgInfo( sPkgOID, sPkgInfo )
                  != IDE_SUCCESS );
        sPkgState = 0;
    }

    /* PROJ-2268 Reuse Catalog Table Slot */
    IDE_TEST_RAISE( smiValidatePlanHandle( smiGetTable( sPkgOID ),
                                           aObjectPlan->objectSCN ) 
                    != IDE_SUCCESS, err_object_changed );

    IDE_TEST( qsxPkg::latchS( sPkgOID )
              != IDE_SUCCESS );
    sPkgLatchState = 1;

    IDE_TEST( qsxPkg::getPkgInfo( sPkgOID,
                                  &sPkgInfo )
              != IDE_SUCCESS );

    if ( sPkgInfo->isValid != ID_TRUE )
    {
        // proj-1535
        // upgrade to exclusive latch
        sPkgLatchState = 0;
        IDE_TEST( qsxPkg::unlatch ( sPkgOID )
                  != IDE_SUCCESS );

        IDE_TEST( qsxPkg::latchXForRecompile( sPkgOID )
                  != IDE_SUCCESS );
        sPkgLatchState = 1;

        IDE_TEST( qsxPkg::getPkgInfo( sPkgOID,
                                      &sPkgInfo )
                  != IDE_SUCCESS );

        if ( sPkgInfo->isValid != ID_TRUE )
        {
            IDE_TEST_RAISE( qsx::doPkgRecompile( aQcStmt,
                                                 sPkgOID,
                                                 sPkgInfo,
                                                 ID_TRUE )
                            != IDE_SUCCESS,
                            RECOMPILE_ERR );

            /* BUG-48443 */
            if ( aObjectType == QS_PKG )
            {
                aQcStmt->mFlag &= ~QC_STMT_PACKAGE_RECOMPILE_MASK;
                aQcStmt->mFlag |= QC_STMT_PACKAGE_RECOMPILE_TRUE;
            }
        }
        else
        {
            // Nothing to do.
        }

        // proj-1535
        // downgrade to shared latch
        sPkgLatchState = 0;
        IDE_TEST( qsxPkg::unlatch ( sPkgOID )
                  != IDE_SUCCESS );

        IDE_TEST( qsxPkg::latchS ( sPkgOID )
                  != IDE_SUCCESS );
        sPkgLatchState = 1;

        IDE_TEST( qsxPkg::getPkgInfo( sPkgOID,
                                      &sPkgInfo )
                  != IDE_SUCCESS );

        IDE_TEST_RAISE( sPkgInfo->isValid != ID_TRUE, err_object_invalid );
    }

    if( aObjectType == QS_PKG )
    {
        /* BUG-39340
           package body�� ���� ������ recompile �ϵ��� �Ѵ�. */
        if ( (sTopStmtKind & QCI_STMT_MASK_MASK) != QCI_STMT_MASK_DDL )
        {
            /* BUG-38720
               package body ���� ���� Ȯ�� �� body�� qsxPkgInfo�� �����´�. */
            IDE_DASSERT( sPkgInfo->planTree != NULL );

            IDE_TEST( qcmPkg::getPkgExistWithEmptyByName( aQcStmt,
                                                          sPkgInfo->planTree->userID,
                                                          sPkgInfo->planTree->pkgNamePos,
                                                          QS_PKG_BODY,
                                                          &sPkgBodyOID )
                      != IDE_SUCCESS );

            if( sPkgBodyOID != QS_EMPTY_OID )
            {
                IDE_TEST( qsxPkg::getPkgObjectInfo( sPkgBodyOID, & sPkgBodyObjectInfo )
                          != IDE_SUCCESS );

                // on boot up case.
                if ( sPkgBodyObjectInfo == NULL )
                {
                    IDE_TEST( qsxPkg::createPkgObjectInfo( sPkgBodyOID, & sPkgBodyObjectInfo )
                              != IDE_SUCCESS );
                    sPkgBodyState = 1;

                    IDE_TEST( qsxPkg::setPkgObjectInfo( sPkgBodyOID,
                                                        sPkgBodyObjectInfo )
                              != IDE_SUCCESS );

                    IDE_TEST( qsxPkg::createPkgInfo( sPkgBodyOID, & sPkgBodyInfo ) // To Fix BUG-19839
                              != IDE_SUCCESS );
                    sPkgBodyState = 2;

                    IDE_TEST( qsxPkg::setPkgInfo( sPkgBodyOID, sPkgBodyInfo )
                              != IDE_SUCCESS );
                    sPkgBodyState = 0;
                }

                IDE_TEST( qsxPkg::latchS ( sPkgBodyOID )
                          != IDE_SUCCESS );
                sPkgBodyLatchState = 1;

                IDE_TEST( qsxPkg::getPkgInfo( sPkgBodyOID,
                                              &sPkgBodyInfo )
                          != IDE_SUCCESS );

                if ( sPkgBodyInfo->isValid != ID_TRUE )
                {
                    sPkgBodyLatchState = 0;
                    IDE_TEST( qsxPkg::unlatch ( sPkgBodyOID )
                              != IDE_SUCCESS );

                    IDE_TEST( qsxPkg::latchXForRecompile( sPkgBodyOID )
                              != IDE_SUCCESS );
                    sPkgBodyLatchState = 1;

                    IDE_TEST( qsxPkg::getPkgInfo( sPkgBodyOID,
                                                  &sPkgBodyInfo )
                              != IDE_SUCCESS );

                    if ( sPkgBodyInfo->isValid != ID_TRUE )
                    {
                        IDE_TEST_RAISE( qsx::doPkgRecompile( aQcStmt,
                                                             sPkgBodyOID,
                                                             sPkgBodyInfo,
                                                             ID_TRUE )
                                        != IDE_SUCCESS,
                                        RECOMPILE_ERR );
                    }
                    else
                    {
                        // Nothing to do.
                    }

                    sPkgBodyLatchState = 0;
                    IDE_TEST( qsxPkg::unlatch ( sPkgBodyOID )
                              != IDE_SUCCESS );

                    IDE_TEST( qsxPkg::latchS ( sPkgBodyOID )
                              != IDE_SUCCESS );
                    sPkgBodyLatchState = 1;

                    IDE_TEST( qsxPkg::getPkgInfo( sPkgBodyOID,
                                                  &sPkgBodyInfo )
                              != IDE_SUCCESS );

                    IDE_TEST_RAISE( sPkgBodyInfo->isValid != ID_TRUE, err_object_invalid );
                }
            }
        }
        else
        {
            // Nothing to do.
        }
    }
    else
    {
        sPkgBodyInfo = sPkgInfo;
    }

    aObjectPlan->modifyCount = sPkgInfo->modifyCount;
    aObjectPlan->planTree    = (void *)( sPkgInfo->planTree );

    if( sPkgBodyInfo != NULL )
    {
        aObjectPlan->pkgBodyOID         = sPkgBodyInfo->pkgOID;
        aObjectPlan->pkgBodyModifyCount = sPkgBodyInfo->modifyCount;
        aObjectPlan->pkgBodySCN         = smiGetRowSCN( smiGetTable( sPkgBodyInfo->pkgOID ) ); /* PROJ-2268 */
        *aRelatedObjects = sPkgBodyInfo->relatedObjects;
    }
    else
    {
        *aRelatedObjects = sPkgInfo->relatedObjects;
    }

    return IDE_SUCCESS;

    // BUG-43979
    // PSM ������ ���� recompile �߿� ������ �߻��� ���, �̸� ����ڿ��� ��������� �˸��ϴ�.
    IDE_EXCEPTION( RECOMPILE_ERR )
    {
        if ( ( sPkgInfo->planTree != NULL ) &&
             ( ideGetErrorCode() != qpERR_ABORT_QSX_INVALID_OBJ_RECOMPILE_FAILED ) )
        {
            (void)sSqlInfo.initWithBeforeMessage( QC_QME_MEM(aQcStmt) );
                IDE_SET(
                    ideSetErrorCode(qpERR_ABORT_QSX_INVALID_OBJ_RECOMPILE_FAILED,
                                    "\n\n", sSqlInfo.getBeforeErrMessage() ) );
            (void)sSqlInfo.fini();
        }
    }
    IDE_EXCEPTION( err_object_invalid );
    {
        IDE_SET( ideSetErrorCode(qpERR_ABORT_QSX_PLAN_INVALID) );
    }
    IDE_EXCEPTION(err_object_changed);
    {
        IDE_SET(ideSetErrorCode( qpERR_ABORT_QSX_PLAN_CHANGED ));
    }

    IDE_EXCEPTION_END;

    switch( sPkgState )
    {
        case 2:
            if ( qsxPkg::destroyPkgInfo ( & sPkgInfo ) != IDE_SUCCESS )
            {
                IDE_ERRLOG(IDE_QP_1);
            }
            /* fall through */
        case 1:
            if ( qsxPkg::destroyPkgObjectInfo ( sPkgOID, & sPkgObjectInfo )
                 != IDE_SUCCESS )
            {
                IDE_ERRLOG(IDE_QP_1);
            }
            /* fall through */
        case 0:
            // Nothing to do.
            break;
    }

    switch( sPkgBodyState )
    {
        case 2:
            if ( qsxPkg::destroyPkgInfo ( & sPkgBodyInfo ) != IDE_SUCCESS )
            {
                IDE_ERRLOG(IDE_QP_1);
            }
            /* fall through */
        case 1:
            if ( qsxPkg::destroyPkgObjectInfo ( sPkgBodyOID, & sPkgBodyObjectInfo )
                 != IDE_SUCCESS )
            {
                IDE_ERRLOG(IDE_QP_1);
            }
            /* fall through */
        case 0:
            // Nothing to do.
            break;
    }

    switch( sPkgLatchState )
    {
        case 1:
            if ( qsxPkg::unlatch ( sPkgOID ) != IDE_SUCCESS)
            {
                IDE_ERRLOG(IDE_QP_1);
            }
            /* fall through */
        case 0:
            // Nothing to do.
            break;

    }

    switch( sPkgBodyLatchState )
    {
        case 1:
            if ( qsxPkg::unlatch ( sPkgBodyOID ) != IDE_SUCCESS)
            {
                IDE_ERRLOG(IDE_QP_1);
            }
            /* fall through */
        case 0:
            // Nothing to do.
            break;
    }

    return IDE_FAILURE;
}
