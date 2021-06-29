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
 * $Id: qcmProc.cpp 90009 2021-02-17 06:54:43Z khkwak $
 **********************************************************************/

#include <idl.h>
#include <ide.h>
#include <idu.h>
#include <qcuProperty.h>
#include <qcm.h>
#include <qcg.h>
#include <qsvEnv.h>
#include <qcmProc.h>
#include <qcmView.h>
#include <qcmPkg.h>
#include <qsxProc.h>
#include <qdParseTree.h>
#include <qcmUser.h>
#include <qdpDrop.h>
#include <mtuProperty.h>
#include <mtf.h>

const void * gQcmProcedures;
const void * gQcmProcParas;
const void * gQcmProcRelated;
const void * gQcmProcParse;
const void * gQcmProceduresIndex [ QCM_MAX_META_INDICES ];
const void * gQcmProcParasIndex  [ QCM_MAX_META_INDICES ];
const void * gQcmProcParseIndex  [ QCM_MAX_META_INDICES ];
const void * gQcmProcRelatedIndex[ QCM_MAX_META_INDICES ];

/* BUG-35445 Check Constraint, Function-Based Index���� ��� ���� Function�� ����/���� ���� */
const void * gQcmConstraintRelated;
const void * gQcmIndexRelated;
const void * gQcmConstraintRelatedIndex[ QCM_MAX_META_INDICES ];
const void * gQcmIndexRelatedIndex     [ QCM_MAX_META_INDICES ];

extern mtlModule mtlUTF16;

/*************************************************************
                       common functions
**************************************************************/

#define TYPE_ARG( MtcCol )                                      \
    ( (MtcCol)-> flag & MTC_COLUMN_ARGUMENT_COUNT_MASK )

#define TYPE_PRECISION( MtcCol )                                \
    ( TYPE_ARG( MtcCol ) == 1 ||                                \
      TYPE_ARG( MtcCol ) == 2 ) ? (MtcCol)-> precision : -1

#define TYPE_SCALE( MtcCol )                            \
    ( TYPE_ARG( MtcCol ) == 2 ) ? (MtcCol)-> scale : -1

#define MAX_COLDATA_SQL_LENGTH (255)

IDE_RC qcmProc::getTypeFieldValues (
    qcStatement * aStatement,
    qtcNode     * aTypeNode,
    SChar       * aReturnTypeType,
    SChar       * aReturnTypeLang,
    SChar       * aReturnTypeSize ,
    SChar       * aReturnTypePrecision,
    SChar       * aReturnTypeScale,
    UInt          aMaxBufferLength
    )
{
    mtcColumn          * sReturnTypeNodeColumn       = NULL;

    sReturnTypeNodeColumn = QTC_TMPL_COLUMN( QC_PRIVATE_TMPLATE(aStatement),
                                             aTypeNode );

    idlOS::snprintf( aReturnTypeType, aMaxBufferLength,
                     QCM_SQL_INT32_FMT,
                     (SInt) sReturnTypeNodeColumn-> type. dataTypeId );

    idlOS::snprintf( aReturnTypeLang, aMaxBufferLength,
                     QCM_SQL_INT32_FMT,
                     (SInt) sReturnTypeNodeColumn-> type. languageId );

    idlOS::snprintf( aReturnTypeSize , aMaxBufferLength,
                     QCM_SQL_UINT32_FMT,
                     sReturnTypeNodeColumn-> column. size);

    idlOS::snprintf( aReturnTypePrecision , aMaxBufferLength,
                     QCM_SQL_INT32_FMT,
                     sReturnTypeNodeColumn->precision );

    idlOS::snprintf( aReturnTypeScale , aMaxBufferLength,
                     QCM_SQL_INT32_FMT,
                     sReturnTypeNodeColumn->scale );

    return IDE_SUCCESS;
}

/*************************************************************
                      main functions
**************************************************************/
IDE_RC qcmProc::insert (
    qcStatement      * aStatement,
    qsProcParseTree  * aProcParse )
{
    qsRelatedObjects  * sRelObjs;
    qsRelatedObjects  * sTmpRelObjs;
    qsRelatedObjects  * sNewRelObj;
    idBool              sExist = ID_FALSE;
    qsOID               sPkgBodyOID;

    IDE_TEST( qcmProc::procInsert ( aStatement,
                                    aProcParse )
              != IDE_SUCCESS );

    if ( aProcParse-> paraDecls != NULL )
    {
        IDE_TEST( qcmProc::paraInsert ( aStatement,
                                        aProcParse,
                                        aProcParse->paraDecls )
                  != IDE_SUCCESS );
    }


    IDE_TEST( qcmProc::prsInsert( aStatement,
                                  aProcParse )
              != IDE_SUCCESS );


    /* PROJ-2197 PSM Renewal
     * aStatement->spvEnv->relatedObjects ���
     * aProcParse->procInfo->relatedObjects�� ����Ѵ�. */
    for ( sRelObjs = aProcParse->procInfo->relatedObjects;
          sRelObjs != NULL;
          sRelObjs = sRelObjs->next )
    {
        // BUG-36975
        for( sTmpRelObjs = sRelObjs->next ;
             sTmpRelObjs != NULL ;
             sTmpRelObjs = sTmpRelObjs->next )
        {
            if( ( sRelObjs->objectID == sTmpRelObjs->objectID ) &&
                ( idlOS::strMatch( sRelObjs->objectName.name,
                                   sRelObjs->objectName.size,
                                   sTmpRelObjs->objectName.name,
                                   sTmpRelObjs->objectName.size ) == 0 ) )
            {
                sExist = ID_TRUE;
                break;
            }
        }

        if( sExist == ID_FALSE )
        {
            IDE_TEST( qcmProc::relInsert( aStatement,
                                          aProcParse,
                                          sRelObjs )
                      != IDE_SUCCESS );

            // BUG-36975
            if( sRelObjs->objectType == QS_PKG )
            {
                if( qcmPkg::getPkgExistWithEmptyByNamePtr( aStatement,
                                                           sRelObjs->userID,
                                                           (SChar*)(sRelObjs->objectNamePos.stmtText +
                                                                    sRelObjs->objectNamePos.offset),
                                                           sRelObjs->objectNamePos.size,
                                                           QS_PKG_BODY,
                                                           &sPkgBodyOID )
                    == IDE_SUCCESS )
                {
                    if( sPkgBodyOID != QS_EMPTY_OID )
                    {
                        IDE_TEST( qcmPkg::makeRelatedObjectNodeForInsertMeta(
                                           aStatement,
                                           sRelObjs->userID,
                                           sRelObjs->objectID,
                                           sRelObjs->objectNamePos,
                                           (SInt)QS_PKG_BODY,
                                           &sNewRelObj )
                                  != IDE_SUCCESS );

                        IDE_TEST( qcmProc::relInsert( aStatement,
                                                      aProcParse,
                                                      sNewRelObj )
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
            else
            {
                // Nothing to do.
                //  package�� spec�� body�� �з��ȴ�.
            }
        }
        else
        {
            sExist = ID_FALSE;
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qcmProc::remove (
    qcStatement      * aStatement,
    qsOID              aProcOID,
    qcNamePosition     aUserNamePos,
    qcNamePosition     aProcNamePos,
    idBool             aPreservePrivInfo )
{
    UInt sUserID;

    IDE_TEST( qcmUser::getUserID( aStatement,
                                  aUserNamePos,
                                  & sUserID ) != IDE_SUCCESS );

    IDE_TEST( remove( aStatement,
                      aProcOID,
                      sUserID,
                      aProcNamePos.stmtText + aProcNamePos.offset,
                      aProcNamePos.size,
                      aPreservePrivInfo )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qcmProc::remove (
    qcStatement      * aStatement,
    qsOID              aProcOID,
    UInt               aUserID,
    SChar            * aProcName,
    UInt               aProcNameSize,
    idBool             aPreservePrivInfo )
{
    //-----------------------------------------------
    // related procedure, function or typeset
    //-----------------------------------------------

    IDE_TEST( qcmProc::relSetInvalidProcOfRelated (
                  aStatement,
                  aUserID,
                  aProcName,
                  aProcNameSize,
                  QS_PROC )
              != IDE_SUCCESS );

    IDE_TEST( qcmProc::relSetInvalidProcOfRelated (
                  aStatement,
                  aUserID,
                  aProcName,
                  aProcNameSize,
                  QS_FUNC )
              != IDE_SUCCESS );

    // PROJ-1075 TYPESET �߰�.
    IDE_TEST( qcmProc::relSetInvalidProcOfRelated (
                  aStatement,
                  aUserID,
                  aProcName,
                  aProcNameSize,
                  QS_TYPESET )
              != IDE_SUCCESS );

    // PROJ-1073 Package
    IDE_TEST( qcmPkg::relSetInvalidPkgOfRelated (
                 aStatement,
                 aUserID,
                 aProcName,
                 aProcNameSize,
                 QS_PROC )
              != IDE_SUCCESS );

    IDE_TEST( qcmPkg::relSetInvalidPkgOfRelated (
                 aStatement,
                 aUserID,
                 aProcName,
                 aProcNameSize,
                 QS_FUNC )
              != IDE_SUCCESS );

    IDE_TEST( qcmPkg::relSetInvalidPkgOfRelated (
                 aStatement,
                 aUserID,
                 aProcName,
                 aProcNameSize,
                 QS_TYPESET )
              != IDE_SUCCESS );

    //-----------------------------------------------
    // related view
    // PROJ-1075 TYPESET�� view�� ������.
    //-----------------------------------------------
    IDE_TEST( qcmView::setInvalidViewOfRelated(
                  aStatement,
                  aUserID,
                  aProcName,
                  aProcNameSize,
                  QS_PROC )
              != IDE_SUCCESS );

    IDE_TEST( qcmView::setInvalidViewOfRelated(
                  aStatement,
                  aUserID,
                  aProcName,
                  aProcNameSize,
                  QS_FUNC )
              != IDE_SUCCESS );

    //-----------------------------------------------
    // remove
    //-----------------------------------------------
    IDE_TEST( qcmProc::procRemove( aStatement,
                                   aProcOID )
              != IDE_SUCCESS );

    IDE_TEST( qcmProc::paraRemoveAll( aStatement,
                                      aProcOID )
              != IDE_SUCCESS );

    IDE_TEST( qcmProc::prsRemoveAll( aStatement,
                                     aProcOID )
              != IDE_SUCCESS );

    IDE_TEST( qcmProc::relRemoveAll ( aStatement,
                                      aProcOID )
              != IDE_SUCCESS );

    if( aPreservePrivInfo != ID_TRUE )
    {
        IDE_TEST( qdpDrop::removePriv4DropProc( aStatement,
                                                aProcOID )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}



/*************************************************************
                       qcmProcedures
**************************************************************/

IDE_RC qcmProc::procInsert(
    qcStatement      * aStatement,
    qsProcParseTree  * aProcParse )
{
/***********************************************************************
 *
 * Description :
 *    createProcOrFunc �ÿ� ��Ÿ ���̺� �Է�
 *
 * Implementation :
 *    ��õ� ParseTree �κ��� SYS_PROCEDURES_ ��Ÿ ���̺� �Է��� �����͸�
 *    ������ �Ŀ� �Է� ������ ���� ����
 *
 ***********************************************************************/

    qsVariableItems    * sParaDecls;
    SInt                 sParaCount = 0;
    vSLong               sRowCnt;
    SChar                sProcName            [ QC_MAX_OBJECT_NAME_LEN + 1 ];
    SChar              * sBuffer;
    SChar                sReturnTypeType      [ MAX_COLDATA_SQL_LENGTH ];
    SChar                sReturnTypeLang      [ MAX_COLDATA_SQL_LENGTH ];
    SChar                sReturnTypeSize      [ MAX_COLDATA_SQL_LENGTH ];
    SChar                sReturnTypePrecision [ MAX_COLDATA_SQL_LENGTH ];
    SChar                sReturnTypeScale     [ MAX_COLDATA_SQL_LENGTH ];
    // SChar                sNatGroupOID         [ MAX_COLDATA_SQL_LENGTH ];
    SInt                 sAuthidType = 0;

    QC_STR_COPY( sProcName, aProcParse-> procNamePos );

    for ( sParaDecls = aProcParse-> paraDecls ;
          sParaDecls != NULL                  ;
          sParaDecls = sParaDecls-> next )
    {
        sParaCount ++;
    }

    if ( aProcParse->returnTypeVar != NULL )  // stored function
    {
        IDE_TEST( qcmProc::getTypeFieldValues( aStatement,
                                               aProcParse->returnTypeVar->variableTypeNode,
                                               sReturnTypeType,
                                               sReturnTypeLang,
                                               sReturnTypeSize,
                                               sReturnTypePrecision,
                                               sReturnTypeScale,
                                               MAX_COLDATA_SQL_LENGTH )
                  != IDE_SUCCESS );
    }
    else // stored procedure
    {
        idlOS::snprintf( sReturnTypeType,      MAX_COLDATA_SQL_LENGTH, "NULL");
        idlOS::snprintf( sReturnTypeLang,      MAX_COLDATA_SQL_LENGTH, "NULL");
        idlOS::snprintf( sReturnTypeSize,      MAX_COLDATA_SQL_LENGTH, "NULL");
        idlOS::snprintf( sReturnTypePrecision, MAX_COLDATA_SQL_LENGTH, "NULL");
        idlOS::snprintf( sReturnTypeScale,     MAX_COLDATA_SQL_LENGTH, "NULL");
    }

    /* BUG-45306 Authid */
    if ( aProcParse->isDefiner == ID_TRUE )
    {
        sAuthidType = (SInt)QCM_PROC_DEFINER;
    }
    else
    {
        sAuthidType = (SInt)QCM_PROC_CURRENT_USER;
    }

    IDU_LIMITPOINT("qcmProc::procInsert::malloc");
    IDE_TEST(STRUCT_ALLOC_WITH_SIZE(aStatement->qmxMem,
                                    SChar,
                                    QD_MAX_SQL_LENGTH,
                                    &sBuffer)
             != IDE_SUCCESS);

    idlOS::snprintf( sBuffer, QD_MAX_SQL_LENGTH,
                     "INSERT INTO SYS_PROCEDURES_ VALUES ( "
                     QCM_SQL_UINT32_FMT", "     // 1
                     QCM_SQL_BIGINT_FMT", "     // 2
                     QCM_SQL_CHAR_FMT", "       // 3
                     QCM_SQL_INT32_FMT", "      // 4
                     QCM_SQL_INT32_FMT", "      // 5 
                     QCM_SQL_INT32_FMT", "      // 6
                     "%s, "                     // 7
                     "%s, "                     // 8
                     "%s, "                     // 9
                     "%s, "                     // 10
                     "%s, "                     // 11
                     QCM_SQL_INT32_FMT", "      // 12
                     QCM_SQL_INT32_FMT", "      // 13
                     "SYSDATE, SYSDATE, "
                     QCM_SQL_INT32_FMT" )",     // 16
                     aProcParse->userID,                             // 1
                     QCM_OID_TO_BIGINT( aProcParse->procOID ),       // 2
                     sProcName,                                      // 3
                     (SInt) aProcParse->objType,                     // 4
                     (SInt)
                         (aProcParse->procInfo->isValid == ID_TRUE)
                         ? QCM_PROC_VALID : QCM_PROC_INVALID,        // 5
                     (SInt) sParaCount,                              // 6
                     sReturnTypeType,                                // 7
                     sReturnTypeLang,                                // 8
                     sReturnTypeSize,                                // 9
                     sReturnTypePrecision,                           // 10
                     sReturnTypeScale,                               // 11
                     (SInt )
                     ( (aStatement->myPlan->stmtTextLen / QCM_MAX_PROC_LEN)
                       + (((aStatement->myPlan->stmtTextLen % QCM_MAX_PROC_LEN) == 0)
                          ? 0 : 1) ),                                // 12
                     aStatement->myPlan->stmtTextLen,                // 13
                     sAuthidType );                                  // 16

    IDE_TEST( qcg::runDMLforDDL( QC_SMI_STMT( aStatement ),
                                 sBuffer,
                                 & sRowCnt )
              != IDE_SUCCESS );

    IDE_TEST_RAISE( sRowCnt != 1, err_inserted_count_is_not_1 );

    return IDE_SUCCESS;

    IDE_EXCEPTION( err_inserted_count_is_not_1 );
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QCM_INTERNAL_ARG,
                                "[qcmProc::procInsert] err_inserted_count_is_not_1" ));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qcmSetProcOIDOfQcmProcedures(
    idvSQL              * /* aStatistics */,
    const void          * aRow,
    qsOID               * aProcID )
{
    SLong   sSLongID;
    mtcColumn            * sProcIDMtcColumn;

    IDE_TEST( smiGetTableColumns( gQcmProcedures,
                                  QCM_PROCEDURES_PROCOID_COL_ORDER,
                                  (const smiColumn**)&sProcIDMtcColumn )
              != IDE_SUCCESS );

    qcm::getBigintFieldValue (
        aRow,
        sProcIDMtcColumn,
        & sSLongID );
    // BUGBUG 32bit machine���� ���� �� SLong(64bit)������ uVLong(32bit)������
    // ��ȯ�ϹǷ� ������ �ս� ���ɼ� ����
    *aProcID = (qsOID)sSLongID;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


IDE_RC qcmProc::getProcExistWithEmptyByName(
    qcStatement     * aStatement,
    UInt              aUserID,
    qcNamePosition    aProcName,
    qsOID           * aProcOID )
{
    IDE_TEST(
        getProcExistWithEmptyByNamePtr(
            aStatement,
            aUserID,
            (SChar*) (aProcName.stmtText +
                      aProcName.offset),
            aProcName.size,
            aProcOID)
        != IDE_SUCCESS);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qcmProc::getProcExistWithEmptyByNamePtr(
    qcStatement     * aStatement,
    UInt              aUserID,
    SChar           * aProcName,
    SInt              aProcNameSize,
    qsOID           * aProcOID )
{
    smiRange              sRange;
    qtcMetaRangeColumn    sFirstMetaRange;
    qtcMetaRangeColumn    sSecondMetaRange;

    mtdIntegerType        sIntData;
    vSLong                sRowCount;
    qsOID                 sSelectedProcOID;

    qcNameCharBuffer      sProcNameBuffer;
    mtdCharType         * sProcName = ( mtdCharType * ) & sProcNameBuffer;
    mtcColumn            * sFstMtcColumn;
    mtcColumn            * sSndMtcColumn;

    if ( aProcNameSize > QC_MAX_OBJECT_NAME_LEN )
    {
        *aProcOID = QS_EMPTY_OID;
        IDE_CONT( NORMAL_EXIT );
    }
    else
    {
        // Nothing to do.
    }

    qtc::setVarcharValue( sProcName,
                          NULL,
                          aProcName,
                          aProcNameSize );

    sIntData = (mtdIntegerType) aUserID;

    IDE_TEST( smiGetTableColumns( gQcmProcedures,
                                  QCM_PROCEDURES_PROCNAME_COL_ORDER,
                                  (const smiColumn**)&sFstMtcColumn )
              != IDE_SUCCESS );

    IDE_TEST( smiGetTableColumns( gQcmProcedures,
                                  QCM_PROCEDURES_USERID_COL_ORDER,
                                  (const smiColumn**)&sSndMtcColumn )
              != IDE_SUCCESS );

    qcm::makeMetaRangeDoubleColumn( & sFirstMetaRange,
                                    & sSecondMetaRange,
                                    sFstMtcColumn,
                                    (const void*) sProcName,
                                    sSndMtcColumn,
                                    & sIntData,
                                    & sRange );

    IDE_TEST( qcm::selectRow(
                  QC_SMI_STMT( aStatement ),
                  gQcmProcedures,
                  smiGetDefaultFilter(),
                  & sRange,
                  gQcmProceduresIndex
                  [ QCM_PROCEDURES_PROCNAME_USERID_IDX_ORDER ],
                  (qcmSetStructMemberFunc ) qcmSetProcOIDOfQcmProcedures,
                  & sSelectedProcOID,
                  0,
                  1,
                  & sRowCount )
              != IDE_SUCCESS );

    if (sRowCount == 1)
    {
        ( *aProcOID ) = sSelectedProcOID;
    }
    else
    {
        if (sRowCount == 0)
        {
            ( *aProcOID ) = QS_EMPTY_OID;
        }
        else
        {
            IDE_TEST_RAISE( sRowCount != 1, err_selected_count_is_not_1 );
        }
    }

    IDE_EXCEPTION_CONT( NORMAL_EXIT );
    
    return IDE_SUCCESS;

    IDE_EXCEPTION( err_selected_count_is_not_1 );
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QCM_INTERNAL_ARG,
                                "[qcmProc::getProcExistWithEmptyByName]"
                                "err_selected_count_is_not_1" ));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qcmProc::procUpdateStatus(
    qcStatement             * aStatement,
    qsOID                     aProcOID,
    qcmProcStatusType         aStatus)
{
/***********************************************************************
 *
 * Description :
 *    alterProcOrFunc, recompile, rebuild �ÿ� ��Ÿ ���̺� ����
 *
 * Implementation :
 *    ��õ� ProcOID, status ������ SYS_PROCEDURES_ ��Ÿ ���̺���
 *    STATUS ���� �����Ѵ�.
 *
 ***********************************************************************/

    SChar    sBuffer[QD_MAX_SQL_LENGTH];
    vSLong   sRowCnt;

    idlOS::snprintf( sBuffer, QD_MAX_SQL_LENGTH,
                     "UPDATE SYS_PROCEDURES_ "
                     "SET    STATUS   = "QCM_SQL_INT32_FMT", "
                     "       LAST_DDL_TIME = SYSDATE "
                     "WHERE  PROC_OID = "QCM_SQL_BIGINT_FMT,
                     (SInt) aStatus,
                     QCM_OID_TO_BIGINT( aProcOID ) );

    IDE_TEST( qcg::runDMLforDDL( QC_SMI_STMT( aStatement ),
                                 sBuffer,
                                 & sRowCnt )
              != IDE_SUCCESS );

    IDE_TEST_RAISE( sRowCnt != 1, err_updated_count_is_not_1 );

    return IDE_SUCCESS;

    IDE_EXCEPTION( err_updated_count_is_not_1 );
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QCM_INTERNAL_ARG,
                                "[qcmProc::procUpdateStatus]"
                                "err_updated_count_is_not_1" ));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qcmProc::procUpdateStatusTx(
    qcStatement             * aQcStmt,
    qsOID                     aProcOID,
    qcmProcStatusType         aStatus)
{
    smiTrans       sSmiTrans;
    smiStatement * sDummySmiStmt = NULL;
    smiStatement   sSmiStmt;
    smiStatement * sSmiStmtOrg   = NULL;
    SInt           sState        = 0;
    UInt           sSmiStmtFlag  = 0;

retry:
    // transaction initialze
    IDE_TEST( sSmiTrans.initialize() != IDE_SUCCESS );
    sState = 1;

    // transaction begin
    IDE_TEST( sSmiTrans.begin( &sDummySmiStmt,
                               aQcStmt->mStatistics)
              != IDE_SUCCESS );
    sState = 2;

    sSmiStmtFlag &= ~SMI_STATEMENT_MASK;
    sSmiStmtFlag |= SMI_STATEMENT_NORMAL;

    sSmiStmtFlag &= ~SMI_STATEMENT_CURSOR_MASK;
    sSmiStmtFlag |= SMI_STATEMENT_MEMORY_CURSOR;

    // statement begin
    IDE_TEST( sSmiStmt.begin( aQcStmt->mStatistics, sDummySmiStmt, sSmiStmtFlag )
              != IDE_SUCCESS );

    // swap
    qcg::getSmiStmt( aQcStmt, &sSmiStmtOrg );
    qcg::setSmiStmt( aQcStmt, &sSmiStmt );
    sState = 3;

    if ( qcmProc::procUpdateStatus( aQcStmt,
                                    aProcOID,
                                    aStatus ) != IDE_SUCCESS )
    {
        switch ( ideGetErrorCode() & E_ACTION_MASK )
        {
            case E_ACTION_RETRY:
                qcg::setSmiStmt( aQcStmt, sSmiStmtOrg );

                sSmiStmt.end( SMI_STATEMENT_RESULT_FAILURE );

                sSmiTrans.rollback();

                sSmiTrans.destroy( aQcStmt->mStatistics );

                goto retry;
                break;
            default:
                IDE_TEST( 1 );
                break;
        }
    }
    else
    {
        // Nothing to do.
    }

    // restore
    qcg::setSmiStmt( aQcStmt, sSmiStmtOrg );

    // statement close
    sState = 2;
    IDE_TEST( sSmiStmt.end( SMI_STATEMENT_RESULT_SUCCESS )!= IDE_SUCCESS );

    // transaction commit
    sState = 1;
    IDE_TEST( sSmiTrans.commit() != IDE_SUCCESS );

    // transaction destroy
    sState = 0;
    IDE_TEST( sSmiTrans.destroy( aQcStmt->mStatistics ) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    switch(sState)
    {
        case 3:
            qcg::setSmiStmt( aQcStmt, sSmiStmtOrg );
            sSmiStmt.end( SMI_STATEMENT_RESULT_FAILURE );
        case 2:
            sSmiTrans.rollback();
        case 1:
            sSmiTrans.destroy( aQcStmt->mStatistics );
            break;
    }

    return IDE_FAILURE;
}

IDE_RC qcmProc::procRemove(
    qcStatement     * aStatement,
    qsOID             aProcOID )
{
/***********************************************************************
 *
 * Description :
 *    replace, drop �ÿ� ��Ÿ ���̺��� ����
 *
 * Implementation :
 *    ��õ� ProcOID �� �ش��ϴ� �����͸� SYS_PROCEDURES_ ��Ÿ ���̺���
 *    �����Ѵ�.
 *
 ***********************************************************************/

    SChar    * sBuffer;
    vSLong     sRowCnt;

    IDU_LIMITPOINT("qcmProc::procRemove::malloc");
    IDE_TEST(STRUCT_ALLOC_WITH_SIZE(aStatement->qmxMem,
                                    SChar,
                                    QD_MAX_SQL_LENGTH,
                                    &sBuffer)
             != IDE_SUCCESS);

    idlOS::snprintf( sBuffer, QD_MAX_SQL_LENGTH,
                     "DELETE FROM SYS_PROCEDURES_ "
                     "WHERE  PROC_OID = "QCM_SQL_BIGINT_FMT,
                     QCM_OID_TO_BIGINT( aProcOID ) );

    IDE_TEST( qcg::runDMLforDDL( QC_SMI_STMT( aStatement ),
                                 sBuffer,
                                 & sRowCnt )
              != IDE_SUCCESS );

    IDE_TEST_RAISE( sRowCnt != 1, err_removed_count_is_not_1 );

    return IDE_SUCCESS;

    IDE_EXCEPTION( err_removed_count_is_not_1 );
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QCM_INTERNAL_ARG,
                                "[qcmProc::procRemove]"
                                "err_removed_count_is_not_1" ));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qcmProc::procSelectCount (
    smiStatement         * aSmiStmt,
    vSLong               * aRowCount )
{
    vSLong selectedRowCount;

    IDE_TEST( qcm::selectCount
              (
                  aSmiStmt,
                  gQcmProcedures,
                  &selectedRowCount,
                  smiGetDefaultFilter(),
                  smiGetDefaultKeyRange(),
                  NULL
              ) != IDE_SUCCESS );

    *aRowCount = (vSLong)selectedRowCount ;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


IDE_RC qcmProc::procSelectAll (
    smiStatement         * aSmiStmt,
    vSLong                 aMaxProcedureCount,
    vSLong               * aSelectedProcedureCount,
    qcmProcedures        * aProcedureArray )
{
    vSLong selectedRowCount;

    IDE_TEST( qcm::selectRow
              (
                  aSmiStmt,
                  gQcmProcedures,
                  smiGetDefaultFilter(), /* a_callback */
                  smiGetDefaultKeyRange(), /* a_range */
                  NULL, /* a_index */
                  (qcmSetStructMemberFunc ) qcmProc::procSetMember,
                  aProcedureArray,
                  ID_SIZEOF(qcmProcedures), /* distance */
                  aMaxProcedureCount,
                  &selectedRowCount
                  ) != IDE_SUCCESS );

    *aSelectedProcedureCount = (vSLong) selectedRowCount;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


IDE_RC qcmProc::procSelectCountWithUserId (
    qcStatement          * aStatement,
    UInt                   aUserId,
    vSLong               * aRowCount )
{
    vSLong                selectedRowCount;
    qtcMetaRangeColumn    sFirstMetaRange;
    smiRange              sRange;
    mtdIntegerType        sIntData;
    mtcColumn           * sUserIDMtcColumn;

    sIntData = (mtdIntegerType) aUserId;

    IDE_TEST( smiGetTableColumns( gQcmProcedures,
                                  QCM_PROCEDURES_USERID_COL_ORDER,
                                  (const smiColumn**)&sUserIDMtcColumn )
              != IDE_SUCCESS );

    qcm::makeMetaRangeSingleColumn( & sFirstMetaRange,
                                    sUserIDMtcColumn,
                                    & sIntData,
                                    & sRange );

    IDE_TEST( qcm::selectCount
              (
                  QC_SMI_STMT( aStatement ),
                  gQcmProcedures,
                  &selectedRowCount,
                  smiGetDefaultFilter(),
                  & sRange,
                  gQcmProceduresIndex
                  [ QCM_PROCEDURES_USERID_IDX_ORDER ]
              ) != IDE_SUCCESS );

    *aRowCount = (vSLong)selectedRowCount ;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qcmProc::procSelectAllWithUserId (
    qcStatement          * aStatement,
    UInt                   aUserId,
    vSLong                 aMaxProcedureCount,
    vSLong               * aSelectedProcedureCount,
    qcmProcedures        * aProcedureArray )
{
    vSLong                selectedRowCount;
    qtcMetaRangeColumn    sFirstMetaRange;
    smiRange              sRange;
    mtdIntegerType        sIntData;
    mtcColumn           * sUserIDMtcColumn;

    sIntData = (mtdIntegerType) aUserId;

    IDE_TEST( smiGetTableColumns( gQcmProcedures,
                                  QCM_PROCEDURES_USERID_COL_ORDER,
                                  (const smiColumn**)&sUserIDMtcColumn )
              != IDE_SUCCESS );

    qcm::makeMetaRangeSingleColumn( & sFirstMetaRange,
                                    sUserIDMtcColumn,
                                    & sIntData,
                                    & sRange );

    IDE_TEST( qcm::selectRow
              (
                  QC_SMI_STMT( aStatement ),
                  gQcmProcedures,
                  smiGetDefaultFilter(), /* a_callback */
                  & sRange,
                  gQcmProceduresIndex
                  [ QCM_PROCEDURES_USERID_IDX_ORDER ],
                  (qcmSetStructMemberFunc ) qcmProc::procSetMember,
                  aProcedureArray,
                  ID_SIZEOF(qcmProcedures), /* distance */
                  aMaxProcedureCount,
                  &selectedRowCount
                  ) != IDE_SUCCESS );

    *aSelectedProcedureCount = (vSLong) selectedRowCount;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/*************************************************************
                       qcmProcParas
**************************************************************/
IDE_RC qcmProc::paraInsert(
    qcStatement     * aStatement,
    qsProcParseTree * aProcParse,
    qsVariableItems * aParaDeclParse)
{
/***********************************************************************
 *
 * Description :
 *    create �ÿ� ��Ÿ ���̺� ���ν����� ���� ���� �Է�
 *
 * Implementation :
 *    ��õ� ParseTree �κ��� ���������� �����Ͽ� SYS_PROC_PARAS_
 *    ��Ÿ ���̺� �Է��ϴ� �������� ���� �� ����
 *
 ***********************************************************************/

    vSLong               sRowCnt;
    SInt                 sParaOrder = 0;
    SChar                sParaName            [ QC_MAX_OBJECT_NAME_LEN + 1    ];
    SChar                sDefaultValueString  [ QCM_MAX_DEFAULT_VAL + 2];
    SChar              * sBuffer;
    SChar                sReturnTypeType      [ MAX_COLDATA_SQL_LENGTH ];
    SChar                sReturnTypeLang      [ MAX_COLDATA_SQL_LENGTH ];
    SChar                sReturnTypeSize      [ MAX_COLDATA_SQL_LENGTH ];
    SChar                sReturnTypePrecision [ MAX_COLDATA_SQL_LENGTH ];
    SChar                sReturnTypeScale     [ MAX_COLDATA_SQL_LENGTH ];
    qsVariables        * sParaVar;

    while( aParaDeclParse != NULL )
    {
        sParaVar = (qsVariables*)aParaDeclParse;

        sParaOrder ++ ;

        QC_STR_COPY( sParaName, aParaDeclParse->name );

        if (sParaVar->defaultValueNode != NULL)
        {
            prsCopyStrDupAppo(
                sDefaultValueString,
                sParaVar->defaultValueNode->position.stmtText +
                sParaVar->defaultValueNode->position.offset,
                sParaVar->defaultValueNode->position.size );
        }
        else
        {
            sDefaultValueString[0] = '\0';
        }

        IDE_TEST( qcmProc::getTypeFieldValues( aStatement,
                                               sParaVar-> variableTypeNode,
                                               sReturnTypeType,
                                               sReturnTypeLang,
                                               sReturnTypeSize,
                                               sReturnTypePrecision,
                                               sReturnTypeScale,
                                               MAX_COLDATA_SQL_LENGTH )
                  != IDE_SUCCESS );

        IDU_LIMITPOINT("qcmProc::paraInsert::malloc");
        IDE_TEST(STRUCT_ALLOC_WITH_SIZE(aStatement->qmxMem,
                                        SChar,
                                        QD_MAX_SQL_LENGTH,
                                        &sBuffer)
                 != IDE_SUCCESS);

        idlOS::snprintf( sBuffer, QD_MAX_SQL_LENGTH,
                         "INSERT INTO SYS_PROC_PARAS_ VALUES ( "
                         QCM_SQL_UINT32_FMT", "                     // 1
                         QCM_SQL_BIGINT_FMT", "                     // 2
                         QCM_SQL_CHAR_FMT", "                       // 3
                         QCM_SQL_INT32_FMT", "                      // 4
                         QCM_SQL_INT32_FMT", "                      // 5
                         "%s ,"                                     // 6
                         "%s ,"                                     // 7
                         "%s ,"                                     // 8
                         "%s ,"                                     // 9
                         "%s ,"                                     // 10
                         QCM_SQL_CHAR_FMT" ) ",                     // 11
                         aProcParse-> userID,                       // 1
                         QCM_OID_TO_BIGINT( aProcParse-> procOID ), // 2
                         sParaName,                                 // 3
                         sParaOrder,                                // 4
                         (SInt) sParaVar->inOutType,                // 5
                         sReturnTypeType,                           // 6
                         sReturnTypeLang,                           // 7
                         sReturnTypeSize,                           // 8
                         sReturnTypePrecision,                      // 9
                         sReturnTypeScale,                          // 10
                         sDefaultValueString );                     // 11

        IDE_TEST( qcg::runDMLforDDL( QC_SMI_STMT( aStatement ),
                                     sBuffer,
                                     & sRowCnt )
                  != IDE_SUCCESS );

        IDE_TEST_RAISE( sRowCnt != 1, err_inserted_count_is_not_1 );

        aParaDeclParse = aParaDeclParse->next ;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( err_inserted_count_is_not_1 );
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QCM_INTERNAL_ARG,
                                "[qcmProc::paraInsert] err_inserted_count_is_not_1" ));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


IDE_RC qcmProc::paraRemoveAll(
    qcStatement     * aStatement,
    qsOID             aProcOID)
{
/***********************************************************************
 *
 * Description :
 *    �����ÿ� ��Ÿ ���̺� ���ν����� ���� ���� ����
 *
 * Implementation :
 *    ��õ� ProcOID �� �ش��Ѵ� �����͸� SYS_PROC_PARAS_ ��Ÿ ���̺���
 *    �����ϴ� �������� ���� �� ����
 *
 ***********************************************************************/

    SChar    * sBuffer;
    vSLong     sRowCnt;

    IDU_LIMITPOINT("qcmProc::paraRemoveAll::malloc");
    IDE_TEST(STRUCT_ALLOC_WITH_SIZE(aStatement->qmxMem,
                                    SChar,
                                    QD_MAX_SQL_LENGTH,
                                    &sBuffer)
             != IDE_SUCCESS);

    idlOS::snprintf( sBuffer, QD_MAX_SQL_LENGTH,
                     "DELETE FROM SYS_PROC_PARAS_ "
                     "WHERE  PROC_OID = "QCM_SQL_BIGINT_FMT,
                     QCM_OID_TO_BIGINT( aProcOID ) );

    IDE_TEST( qcg::runDMLforDDL( QC_SMI_STMT( aStatement ),
                                 sBuffer,
                                 & sRowCnt )
              != IDE_SUCCESS );

    // [original code] no error is raised even if no row is deleted.

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*************************************************************
                       qcmProcParse
**************************************************************/
IDE_RC qcmProc::prsInsert(
    qcStatement     * aStatement,
    qsProcParseTree * aProcParse )
{
    SInt            sCurrPos = 0;
    SInt            sCurrLen = 0;
    SInt            sSeqNo   = 0;
    UInt            sAddSize = 0;
    SChar         * sStmtBuffer = NULL;
    UInt            sStmtBufferLen = 0;
    qcNamePosList * sNcharList = NULL;
    qcNamePosList * sTempNamePosList = NULL;
    qcNamePosition  sNamePos;
    UInt            sBufferSize = 0;
    SChar* sIndex;
    SChar* sStartIndex;
    SChar* sPrevIndex;
    
    const mtlModule* sModule;

    sNcharList = aProcParse->ncharList;

    /* PROJ-2550 PSM Encryption
       system_.sys_proc_parse_�� ��Ÿ���̺�����
       �Է¹��� ������ insert�Ǿ�� �Ѵ�.
       ��, encrypted text�� �Է¹޾�����, encrypted text��
       �Ϲ� ������ �Է¹޾�����, �ش� ������ insert �ȴ�. */
    if ( aStatement->myPlan->encryptedText == NULL )
    {
        // PROJ-1579 NCHAR
        // ��Ÿ���̺� �����ϱ� ���� ��Ʈ���� �����ϱ� ����
        // N Ÿ���� �ִ� ��� U Ÿ������ ��ȯ�Ѵ�.
        if ( sNcharList != NULL )
        {
            for ( sTempNamePosList = sNcharList;
                  sTempNamePosList != NULL;
                  sTempNamePosList = sTempNamePosList->next )
            {
                sNamePos = sTempNamePosList->namePos;

                // U Ÿ������ ��ȯ�ϸ鼭 �þ�� ������ ���
                // N'��' => U'\C548' ���� ��ȯ�ȴٸ�
                // '��'�� ĳ���� ���� KSC5601�̶�� �������� ��,
                // single-quote���� ���ڴ� 2 byte -> 5byte�� ����ȴ�.
                // ��, 1.5�谡 �þ�� ���̴�.
                //(��ü ����� �ƴ϶� �����ϴ� ����� ����ϴ� ����)
                // ������, � �������� ĳ���� ���� ������ �𸣹Ƿ�
                // * 2�� ����� ��´�.
                sAddSize += (sNamePos.size - 3) * 2;
            }

            sBufferSize = aStatement->myPlan->stmtTextLen + sAddSize;

            IDU_LIMITPOINT("qcmProc::prsInsert::malloc");
            IDE_TEST(STRUCT_ALLOC_WITH_SIZE(aStatement->qmxMem,
                                            SChar,
                                            sBufferSize,
                                            & sStmtBuffer)
                     != IDE_SUCCESS);

            IDE_TEST( convertToUTypeString( aStatement,
                                            sNcharList,
                                            sStmtBuffer,
                                            sBufferSize )
                      != IDE_SUCCESS );

            sStmtBufferLen = idlOS::strlen( sStmtBuffer );
        }
        else
        {
            sStmtBufferLen = aStatement->myPlan->stmtTextLen;
            sStmtBuffer = aStatement->myPlan->stmtText;
        }
    }
    else
    {
        sStmtBufferLen = aStatement->myPlan->encryptedTextLen;
        sStmtBuffer    = aStatement->myPlan->encryptedText;
    }

    sModule = mtl::mDBCharSet;

    sStartIndex = sStmtBuffer;
    sIndex = sStartIndex;

    // To fix BUG-21299
    // 100bytes ������ �ڸ���, ĳ���ͼ¿� �°� ���ڸ� �ڸ���.
    // ��, ���� ĳ���͸� �о��� �� 100����Ʈ�� �Ѵ� ��찡 ����µ�,
    // �̶��� �� ���� ĳ���͸� �о��� ���� ���ư��� �ű������ �߶� ����� �ϰ�,
    // �� ������ �̾ ����� �Ѵ�.
    while (1)
    {
        sPrevIndex = sIndex;
        
        (void)sModule->nextCharPtr( (UChar**) &sIndex,
                                    (UChar*)(sStmtBuffer +
                                             sStmtBufferLen) );
        
        if( (sStmtBuffer +
             sStmtBufferLen) <= sIndex )
        {
            // ������ �� ���.
            // ����� �� �� break.
            sSeqNo++;

            sCurrPos = sStartIndex - sStmtBuffer;
            sCurrLen = sIndex - sStartIndex;

            // insert v_qcm_proc_parse
            IDE_TEST( qcmProc::prsInsertFragment(
                          aStatement,
                          sStmtBuffer,
                          aProcParse,
                          sSeqNo,
                          sCurrPos,
                          sCurrLen )
                      != IDE_SUCCESS );
            break;
        }
        else
        {
            if( sIndex - sStartIndex >= QCM_MAX_PROC_LEN )
            {
                // ���� ������ �� ����, �дٺ��� 100����Ʈ �Ǵ� �ʰ��� ����
                // �Ǿ��� �� �߶� ���
                sCurrPos = sStartIndex - sStmtBuffer;
                
                if( sIndex - sStartIndex == QCM_MAX_PROC_LEN )
                {
                    // �� �������� ���
                    sCurrLen = QCM_MAX_PROC_LEN;
                    sStartIndex = sIndex;
                }
                else
                {
                    // �������� ��� �� ���� ĳ���� ��ġ���� ���
                    sCurrLen = sPrevIndex - sStartIndex;
                    sStartIndex = sPrevIndex;
                }

                sSeqNo++;
                
                // insert v_qcm_proc_parse
                IDE_TEST( qcmProc::prsInsertFragment(
                              aStatement,
                              sStmtBuffer,
                              aProcParse,
                              sSeqNo,
                              sCurrPos,
                              sCurrLen )
                          != IDE_SUCCESS );
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

IDE_RC qcmProc::prsCopyStrDupAppo (
    SChar         * aDest,
    SChar         * aSrc,
    UInt            aLength )
{

    while( aLength-- > 0 )
    {
        if ( *aSrc == '\'' )
        {
            *aDest ++ = '\'';
            *aDest ++ = '\'';
        }
        else
        {
            *aDest ++ = *aSrc;
        }
        aSrc ++;
    }
    *aDest = '\0';

    return IDE_SUCCESS;
}

IDE_RC qcmProc::prsInsertFragment(
    qcStatement     * aStatement,
    SChar           * aStmtBuffer,
    qsProcParseTree * aProcParse,
    UInt              aSeqNo,
    UInt              aOffset,
    UInt              aLength )
{
/***********************************************************************
 *
 * Description :
 *    �����ÿ� ���� ���������� SYS_PROC_PARSE_ �� ����
 *
 * Implementation :
 *    �����ÿ� ���� ���������� ������ ������� �������� �Բ� ���޵Ǹ�,
 *    SYS_PROC_PARSE_ ��Ÿ ���̺� �Է��ϴ� ������ ���� ����
 *
 ***********************************************************************/

    vSLong sRowCnt;
    SChar  sParseStr [ QCM_MAX_PROC_LEN * 2 + 2 ];
    SChar *sBuffer;

    if( aStmtBuffer != NULL )
    {
        prsCopyStrDupAppo( sParseStr,
                           aStmtBuffer + aOffset,
                           aLength );
    }
    else
    {
        prsCopyStrDupAppo( sParseStr,
                           aStatement->myPlan->stmtText + aOffset,
                           aLength );
    }

    IDU_LIMITPOINT("qcmProc::prsInsertFragment::malloc");
    IDE_TEST(STRUCT_ALLOC_WITH_SIZE(aStatement->qmxMem,
                                    SChar,
                                    QD_MAX_SQL_LENGTH,
                                    &sBuffer)
             != IDE_SUCCESS);

    idlOS::snprintf( sBuffer, QD_MAX_SQL_LENGTH,
                     "INSERT INTO SYS_PROC_PARSE_ VALUES ( "
                     QCM_SQL_UINT32_FMT", "                     // 1
                     QCM_SQL_BIGINT_FMT", "                     // 2
                     QCM_SQL_UINT32_FMT", "                     // 3
                     QCM_SQL_CHAR_FMT") ",                      // 4
                     aProcParse-> userID,                       // 1
                     QCM_OID_TO_BIGINT( aProcParse-> procOID ), // 2
                     aSeqNo,                                    // 3
                     sParseStr );                               // 4

    IDE_TEST( qcg::runDMLforDDL( QC_SMI_STMT( aStatement ),
                                 sBuffer,
                                 & sRowCnt )
              != IDE_SUCCESS );

    IDE_TEST_RAISE( sRowCnt != 1, err_inserted_count_is_not_1 );

    return IDE_SUCCESS;

    IDE_EXCEPTION( err_inserted_count_is_not_1 );
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QCM_INTERNAL_ARG,
                                "[qcmProc::prsInsertFragment]"
                                "err_inserted_count_is_not_1" ));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qcmProc::prsRemoveAll(
    qcStatement     * aStatement,
    qsOID             aProcOID)
{
/***********************************************************************
 *
 * Description :
 *    drop �ÿ� SYS_PROC_PARSE_ ���̺�κ��� ����
 *
 * Implementation :
 *    SYS_PROC_PARSE_ ��Ÿ ���̺��� ��õ� ProcID �� �����͸� �����Ѵ�.
 *
 ***********************************************************************/

    SChar  * sBuffer;
    vSLong   sRowCnt;

    IDU_LIMITPOINT("qcmProc::prsRemoveAll::malloc");
    IDE_TEST(STRUCT_ALLOC_WITH_SIZE(aStatement->qmxMem,
                                    SChar,
                                    QD_MAX_SQL_LENGTH,
                                    &sBuffer)
             != IDE_SUCCESS);

    idlOS::snprintf( sBuffer, QD_MAX_SQL_LENGTH,
                     "DELETE FROM SYS_PROC_PARSE_ "
                     "WHERE  PROC_OID = "QCM_SQL_BIGINT_FMT,
                     QCM_OID_TO_BIGINT( aProcOID ) );

    IDE_TEST( qcg::runDMLforDDL( QC_SMI_STMT( aStatement ),
                                 sBuffer,
                                 & sRowCnt )
              != IDE_SUCCESS );

    // [original code] no error is raised even if no row is deleted.

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

IDE_RC qcmProc::convertToUTypeString( qcStatement   * aStatement,
                                      qcNamePosList * aNcharList,
                                      SChar         * aDest,
                                      UInt            aBufferSize )
{
/***********************************************************************
 *
 * Description :
 *      PROJ-1579 NCHAR
 *
 *      N'��'�� ���� ��Ʈ���� ��Ÿ���̺� ������ ���
 *      ALTIBASE_NLS_NCHAR_LITERAL_REPLACE = 1 �� ���
 *      U'\C548'�� ���� ����ȴ�.
 *
 * Implementation :
 *
 *      aStatement->namePosList�� stmt�� ���� ������� ���ĵǾ� �ִٰ�
 *      �����Ѵ�.
 *
 *      EX) create view v1 
 *          as select * from t1 where i1 = n'��' and i2 = n'��' and i3 = 'A';
 *
 *      1. loop(n type�� �ִ� ����)
 *          1-1. 'n'-1���� memcpy
 *          1-2. u' memcpy
 *
 *          1-3. loop(n type literal�� ĳ���� ������ �ݺ�)
 *              1) \ ����
 *              2) �� => C548�� ���� �����ڵ� ����Ʈ ���·� �����ؼ� ����
 *                 (ASCII�� �״�� �����Ѵ�.)
 *
 *      2. stmt�� �� �޺κ� ����
 *
 ***********************************************************************/

    qcNamePosList   * sNcharList;
    qcNamePosList   * sTempNamePosList;
    qcNamePosition    sNamePos;
    const mtlModule * sClientCharSet;

    SChar           * sSrcVal;
    UInt              sSrcLen = 0;

    SChar           * sNTypeVal;
    UInt              sNTypeLen = 0;
    SChar           * sNTypeFence;

    UInt              sSrcValOffset = 0;
    UInt              sDestOffset = 0;
    UInt              sCharLength = 0;

    sNcharList = aNcharList;
    
    sClientCharSet = QCG_GET_SESSION_LANGUAGE(aStatement);
    
    sSrcVal = aStatement->myPlan->stmtText;
    sSrcLen = aStatement->myPlan->stmtTextLen;
    
    if( sNcharList != NULL )
    {
        for( sTempNamePosList = sNcharList;
             sTempNamePosList != NULL;
             sTempNamePosList = sTempNamePosList->next )
        {
            sNamePos = sTempNamePosList->namePos;

            // -----------------------------------
            // N �ٷ� ������ ����
            // -----------------------------------
            idlOS::memcpy( aDest + sDestOffset,
                           sSrcVal + sSrcValOffset,
                           sNamePos.offset - sSrcValOffset );

            sDestOffset += (sNamePos.offset - sSrcValOffset );

            // -----------------------------------
            // U'\ ����
            // -----------------------------------
            idlOS::memcpy( aDest + sDestOffset,
                           "U\'",
                           2 );

            sDestOffset += 2;

            // -----------------------------------
            // N Ÿ�� ���ͷ��� ĳ���� �� ��ȯ
            // Ŭ���̾�Ʈ ĳ���� �� => ���ų� ĳ���� ��
            // -----------------------------------
            sNTypeVal = aStatement->myPlan->stmtText + sNamePos.offset + 2;
            sNTypeLen = sNamePos.size - 3;
            sNTypeFence = sNTypeVal + sNTypeLen;

            IDE_TEST( mtf::makeUFromChar( sClientCharSet,
                                          (UChar *)sNTypeVal,
                                          (UChar *)sNTypeFence,
                                          (UChar *)aDest + sDestOffset,
                                          (UChar *)aDest + aBufferSize,
                                          & sCharLength )
                      != IDE_SUCCESS );

            sDestOffset += sCharLength;

            sSrcValOffset = sNamePos.offset + sNamePos.size - 1;
        }

        // -----------------------------------
        // '���� ������ ����
        // -----------------------------------
        idlOS::memcpy( aDest + sDestOffset,
                       sSrcVal + sNamePos.offset + sNamePos.size - 1,
                       sSrcLen - (sNamePos.offset + sNamePos.size - 1) );

        sDestOffset += sSrcLen - (sNamePos.offset + sNamePos.size - 1);

        aDest[sDestOffset] = '\0';
    }
    else
    {
        // N���ͷ��� �����Ƿ� memcpy��.
        idlOS::memcpy( aDest, sSrcVal, sSrcLen );
        
        aDest[sSrcLen] = '\0';
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/*************************************************************
                       qcmProcRelated
**************************************************************/
IDE_RC qcmProc::relInsert(
    qcStatement         * aStatement,
    qsProcParseTree     * aProcParse,
    qsRelatedObjects    * aRelatedObjList )
{
/***********************************************************************
 *
 * Description :
 *    ���ν��� ������ ���õ� ������Ʈ�� ���� ������ �Է��Ѵ�.
 *
 * Implementation :
 *    SYS_PROC_RELATED_ ��Ÿ ���̺� ����� ������Ʈ���� �Է��Ѵ�.
 *
 ***********************************************************************/

    vSLong sRowCnt;
    UInt   sRelUserID ;
    SChar  sRelObjectName [ QC_MAX_OBJECT_NAME_LEN + 1 ];
    SChar *sBuffer;

    // BUG-46395 public synonym�� ����Ѵ�.
    if ( ( aRelatedObjList->objectType == QS_SYNONYM ) &&
         ( aRelatedObjList->userName.size == 0 ) )
    {
        sRelUserID = QC_PUBLIC_USER_ID;
    }
    else
    {
        IDE_TEST( qcmUser::getUserID( aStatement,
                                      aRelatedObjList->userName.name,
                                      aRelatedObjList->userName.size,
                                      & sRelUserID ) != IDE_SUCCESS );
    }

    idlOS::memcpy(sRelObjectName,
                  aRelatedObjList-> objectName.name,
                  aRelatedObjList-> objectName.size);
    sRelObjectName[ aRelatedObjList->objectName.size ] = '\0';

    IDU_LIMITPOINT("qcmProc::relInsert::malloc");
    IDE_TEST(STRUCT_ALLOC_WITH_SIZE(aStatement->qmxMem,
                                    SChar,
                                    QD_MAX_SQL_LENGTH,
                                    &sBuffer)
             != IDE_SUCCESS);

    if ( sRelUserID != QC_PUBLIC_USER_ID )
    {
        idlOS::snprintf( sBuffer, QD_MAX_SQL_LENGTH,
                         "INSERT INTO SYS_PROC_RELATED_ VALUES ( "
                         QCM_SQL_UINT32_FMT", "                     // 1
                         QCM_SQL_BIGINT_FMT", "                     // 2
                         QCM_SQL_INT32_FMT", "                      // 3
                         QCM_SQL_CHAR_FMT", "                       // 4
                         QCM_SQL_INT32_FMT") ",                     // 5
                         aProcParse-> userID,                       // 1
                         QCM_OID_TO_BIGINT( aProcParse-> procOID ), // 2
                         sRelUserID,                                // 3
                         sRelObjectName,                            // 4
                         aRelatedObjList-> objectType );            // 5
    }
    else
    {
        idlOS::snprintf( sBuffer, QD_MAX_SQL_LENGTH,
                         "INSERT INTO SYS_PROC_RELATED_ VALUES ( "
                         QCM_SQL_UINT32_FMT", "                     // 1
                         QCM_SQL_BIGINT_FMT", "                     // 2
                         "NULL, "                                   // NULL value
                         QCM_SQL_CHAR_FMT", "                       // 4
                         QCM_SQL_INT32_FMT") ",                     // 5
                         aProcParse-> userID,                       // 1
                         QCM_OID_TO_BIGINT( aProcParse-> procOID ), // 2
                         sRelObjectName,                            // 4
                         aRelatedObjList-> objectType );            // 5
    }

    IDE_TEST( qcg::runDMLforDDL( QC_SMI_STMT( aStatement ),
                                 sBuffer,
                                 & sRowCnt )
              != IDE_SUCCESS );

    IDE_TEST_RAISE( sRowCnt != 1, err_inserted_count_is_not_1 );

    return IDE_SUCCESS;

    IDE_EXCEPTION( err_inserted_count_is_not_1 );
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QCM_INTERNAL_ARG,
                                "[qcmProc::relInsert] err_inserted_count_is_not_1" ));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}


// for each row in SYS_PROC_RELATED_ concered,
// update SYS_PROCEDURES_ to QCM_PROC_INVALID
IDE_RC qcmModifyStatusOfRelatedProcToInvalid (
    idvSQL              * /* aStatistics */,
    const void          * aRow,
    qcStatement         * aStatement )
{
    qsOID           sProcOID;
    SLong           sSLongOID;
    mtcColumn     * sProcIDMtcColumn;

    IDE_TEST( smiGetTableColumns( gQcmProcRelated,
                                  QCM_PROC_RELATED_PROCOID_COL_ORDER,
                                  (const smiColumn**)&sProcIDMtcColumn )
              != IDE_SUCCESS );

    qcm::getBigintFieldValue (
        aRow,
        sProcIDMtcColumn,
        & sSLongOID );
    // BUGBUG 32bit machine���� ���� �� SLong(64bit)������ uVLong(32bit)������
    // ��ȯ�ϹǷ� ������ �ս� ���ɼ� ����
    sProcOID = (qsOID)sSLongOID;

    IDE_TEST( qsxProc::makeStatusInvalid( aStatement,
                                          sProcOID )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

// BUG-46416
IDE_RC qcmModifyStatusOfRelatedProcToInvalidTx (
    idvSQL              * /* aStatistics */,
    const void          * aRow,
    qcStatement         * aStatement )
{
    qsOID           sProcOID;
    SLong           sSLongOID;
    mtcColumn     * sProcIDMtcColumn;

    IDE_TEST( smiGetTableColumns( gQcmProcRelated,
                                  QCM_PROC_RELATED_PROCOID_COL_ORDER,
                                  (const smiColumn**)&sProcIDMtcColumn )
              != IDE_SUCCESS );

    qcm::getBigintFieldValue (
        aRow,
        sProcIDMtcColumn,
        & sSLongOID );
    // BUGBUG 32bit machine���� ���� �� SLong(64bit)������ uVLong(32bit)������
    // ��ȯ�ϹǷ� ������ �ս� ���ɼ� ����
    sProcOID = (qsOID)sSLongOID;

    IDE_TEST( qsxProc::makeStatusInvalidTx( aStatement,
                                            sProcOID )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qcmProc::relSetInvalidProcOfRelated(
    qcStatement    * aStatement,
    UInt             aRelatedUserID,
    SChar          * aRelatedObjectName,
    UInt             aRelatedObjectNameSize,
    qsObjectType     aRelatedObjectType,
    idBool           aIsUseTx )
{
    smiRange              sRange;
    qtcMetaRangeColumn    sFirstMetaRange;
    qtcMetaRangeColumn    sSecondMetaRange;
    qtcMetaRangeColumn    sThirdMetaRange;
    mtdIntegerType        sObjectTypeIntData;
    mtdIntegerType        sUserIdData;
    vSLong                sRowCount;

    qcNameCharBuffer      sObjectNameBuffer;
    mtdCharType         * sObjectName = ( mtdCharType * ) & sObjectNameBuffer ;
    mtcColumn           * sFirstMtcColumn;
    mtcColumn           * sSecondMtcColumn;
    mtcColumn           * sThirdMtcColumn;

    sUserIdData = (mtdIntegerType) aRelatedUserID;

    qtc::setVarcharValue( sObjectName,
                          NULL,
                          aRelatedObjectName,
                          aRelatedObjectNameSize );

    sObjectTypeIntData = (mtdIntegerType) aRelatedObjectType;

    IDE_TEST( smiGetTableColumns( gQcmProcRelated,
                                  QCM_PROC_RELATED_RELATEDUSERID_COL_ORDER,
                                  (const smiColumn**)&sFirstMtcColumn )
              != IDE_SUCCESS );

    IDE_TEST( smiGetTableColumns( gQcmProcRelated,
                                  QCM_PROC_RELATED_RELATEDOBJECTNAME_COL_ORDER,
                                  (const smiColumn**)&sSecondMtcColumn )
              != IDE_SUCCESS );

    IDE_TEST( smiGetTableColumns( gQcmProcRelated,
                                  QCM_PROC_RELATED_RELATEDOBJECTTYPE_COL_ORDER,
                                  (const smiColumn**)&sThirdMtcColumn )
              != IDE_SUCCESS );

    // BUG-46395 public synonym�� ����Ѵ�.
    // Public synonym�� user id�� NULL�̴�.
    // �˻� ����
    qtc::initializeMetaRange( &sRange,
                              MTD_COMPARE_MTDVAL_MTDVAL );  // Meta is memory table

    if ( aRelatedUserID != QC_PUBLIC_USER_ID )
    {
        qtc::setMetaRangeColumn( &sFirstMetaRange,
                                 sFirstMtcColumn,
                                 & sUserIdData,
                                 SMI_COLUMN_ORDER_ASCENDING,
                                 0 );  // First column of Index
    }
    else
    {
        qtc::setMetaRangeIsNullColumn(
            &sFirstMetaRange,
            sFirstMtcColumn,
            0 );  // First column of Index
    }

    qtc::addMetaRangeColumn( &sRange,
                             &sFirstMetaRange );

    qtc::setMetaRangeColumn( &sSecondMetaRange,
                             sSecondMtcColumn,
                             (const void*) sObjectName,
                             SMI_COLUMN_ORDER_ASCENDING,
                             1 );  // Second column of Index

    qtc::addMetaRangeColumn( &sRange,
                             &sSecondMetaRange );

    qtc::setMetaRangeColumn( &sThirdMetaRange,
                             sThirdMtcColumn,
                             & sObjectTypeIntData,
                             SMI_COLUMN_ORDER_ASCENDING,
                             2 );  // Third column of Index

    qtc::addMetaRangeColumn( &sRange,
                             &sThirdMetaRange );

    qtc::fixMetaRange( &sRange );

    IDE_TEST( qcm::selectRow(
                  QC_SMI_STMT( aStatement ),
                  gQcmProcRelated,
                  smiGetDefaultFilter(), /* a_callback */
                  & sRange,
                  gQcmProcRelatedIndex
                  [ QCM_PROC_RELATED_RELUSERID_RELOBJNAME_RELOBJTYPE ],
                  (aIsUseTx == ID_FALSE)?(qcmSetStructMemberFunc )qcmModifyStatusOfRelatedProcToInvalid:
                                         (qcmSetStructMemberFunc )qcmModifyStatusOfRelatedProcToInvalidTx,
                  aStatement, /* argument to qcmSetStructMemberFunc */
                  0,          /* aMetaStructDistance.
                                 0 means do not change pointer to aStatement */
                  ID_UINT_MAX,/* no limit on selected row count */
                  & sRowCount )
              != IDE_SUCCESS );

    // no row of qcmProcRelated is updated
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    // no row of qcmProcRelated is updated
    return IDE_FAILURE;
}


IDE_RC qcmProc::relRemoveAll(
    qcStatement     * aStatement,
    qsOID             aProcOID)
{
/***********************************************************************
 *
 * Description :
 *    ���ν����� ���õ� ������Ʈ�� ���� ������ �����Ѵ�.
 *
 * Implementation :
 *    SYS_PROC_RELATED_ ��Ÿ ���̺��� ����� ProcOID �� �ش��ϴ�
 *    �����͸� �����Ѵ�.
 *
 ***********************************************************************/

    SChar   * sBuffer;
    vSLong    sRowCnt;

    IDU_LIMITPOINT("qcmProc::relRemoveAll::malloc");
    IDE_TEST(STRUCT_ALLOC_WITH_SIZE(aStatement->qmxMem,
                                    SChar,
                                    QD_MAX_SQL_LENGTH,
                                    &sBuffer)
             != IDE_SUCCESS);

    idlOS::snprintf( sBuffer, QD_MAX_SQL_LENGTH,
                     "DELETE FROM SYS_PROC_RELATED_ "
                     "WHERE  PROC_OID = "QCM_SQL_BIGINT_FMT,
                     QCM_OID_TO_BIGINT( aProcOID ) );

    IDE_TEST( qcg::runDMLforDDL( QC_SMI_STMT( aStatement ),
                                 sBuffer,
                                 & sRowCnt )
              != IDE_SUCCESS );

    // [original code] no error is raised even if no row is deleted.

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*************************************************************
                       qcmConstraintRelated
**************************************************************/
IDE_RC qcmProc::relInsertRelatedToConstraint(
    qcStatement             * aStatement,
    qsConstraintRelatedProc * aRelatedProc )
{
/***********************************************************************
 *
 * Description :
 *    Constraint�� ���õ� Procedure�� ���� ������ �Է��Ѵ�.
 *
 * Implementation :
 *    SYS_CONSTRAINT_RELATED_ ��Ÿ ���̺� �����͸� �Է��Ѵ�.
 *
 ***********************************************************************/

    vSLong   sRowCnt;
    SChar    sRelatedProcName[QC_MAX_OBJECT_NAME_LEN + 1];
    SChar  * sBuffer;

    QC_STR_COPY( sRelatedProcName, aRelatedProc->relatedProcName );

    IDE_TEST( STRUCT_ALLOC_WITH_SIZE( aStatement->qmxMem,
                                      SChar,
                                      QD_MAX_SQL_LENGTH,
                                      &sBuffer )
              != IDE_SUCCESS );

    (void) idlOS::snprintf( sBuffer, QD_MAX_SQL_LENGTH,
                            "INSERT INTO SYS_CONSTRAINT_RELATED_ VALUES ( "
                            QCM_SQL_UINT32_FMT", "          // 1
                            QCM_SQL_UINT32_FMT", "          // 2
                            QCM_SQL_UINT32_FMT", "          // 3
                            QCM_SQL_UINT32_FMT", "          // 4
                            QCM_SQL_CHAR_FMT" ) ",          // 5
                            aRelatedProc->userID,           // 1
                            aRelatedProc->tableID,          // 2
                            aRelatedProc->constraintID,     // 3
                            aRelatedProc->relatedUserID,    // 4
                            sRelatedProcName );             // 5

    IDE_TEST( qcg::runDMLforDDL( QC_SMI_STMT( aStatement ),
                                 sBuffer,
                                 &sRowCnt )
              != IDE_SUCCESS );

    IDE_TEST_RAISE( sRowCnt != 1, ERR_INSERTED_COUNT_IS_NOT_1 );

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_INSERTED_COUNT_IS_NOT_1 );
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QCM_INTERNAL_ARG,
                                  "[qcmProc::relInsertRelatedToConstraint] ERR_INSERTED_COUNT_IS_NOT_1" ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qcmProc::relRemoveRelatedToConstraintByUserID( qcStatement * aStatement,
                                                      UInt          aUserID )
{
/***********************************************************************
 *
 * Description :
 *    Constraint�� ���õ� Procedure�� ���� ������ �����Ѵ�.
 *
 * Implementation :
 *    ����� User ID�� �ش��ϴ� �����͸� SYS_CONSTRAINT_RELATED_
 *    ��Ÿ ���̺��� �����Ѵ�.
 *
 ***********************************************************************/

    SChar  * sBuffer;
    vSLong   sRowCnt;

    IDE_TEST( STRUCT_ALLOC_WITH_SIZE( aStatement->qmxMem,
                                      SChar,
                                      QD_MAX_SQL_LENGTH,
                                      &sBuffer )
              != IDE_SUCCESS );

    (void) idlOS::snprintf( sBuffer, QD_MAX_SQL_LENGTH,
                            "DELETE FROM SYS_CONSTRAINT_RELATED_ "
                            "WHERE  USER_ID = "QCM_SQL_UINT32_FMT,
                            aUserID );

    IDE_TEST( qcg::runDMLforDDL( QC_SMI_STMT( aStatement ),
                                 sBuffer,
                                 &sRowCnt )
              != IDE_SUCCESS );

    /* [original code] no error is raised, even if no row is deleted. */

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qcmProc::relRemoveRelatedToConstraintByTableID( qcStatement * aStatement,
                                                       UInt          aTableID )
{
/***********************************************************************
 *
 * Description :
 *    Constraint�� ���õ� Procedure�� ���� ������ �����Ѵ�.
 *
 * Implementation :
 *    ����� Table ID�� �ش��ϴ� �����͸� SYS_CONSTRAINT_RELATED_
 *    ��Ÿ ���̺��� �����Ѵ�.
 *
 ***********************************************************************/

    SChar  * sBuffer;
    vSLong   sRowCnt;

    IDE_TEST( STRUCT_ALLOC_WITH_SIZE( aStatement->qmxMem,
                                      SChar,
                                      QD_MAX_SQL_LENGTH,
                                      &sBuffer )
              != IDE_SUCCESS );

    (void) idlOS::snprintf( sBuffer, QD_MAX_SQL_LENGTH,
                            "DELETE FROM SYS_CONSTRAINT_RELATED_ "
                            "WHERE  TABLE_ID = "QCM_SQL_UINT32_FMT,
                            aTableID );

    IDE_TEST( qcg::runDMLforDDL( QC_SMI_STMT( aStatement ),
                                 sBuffer,
                                 &sRowCnt )
              != IDE_SUCCESS );

    /* [original code] no error is raised, even if no row is deleted. */

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qcmProc::relRemoveRelatedToConstraintByConstraintID( qcStatement * aStatement,
                                                            UInt          aConstraintID )
{
/***********************************************************************
 *
 * Description :
 *    Constraint�� ���õ� Procedure�� ���� ������ �����Ѵ�.
 *
 * Implementation :
 *    ����� Constraint ID�� �ش��ϴ� �����͸� SYS_CONSTRAINT_RELATED_
 *    ��Ÿ ���̺��� �����Ѵ�.
 *
 ***********************************************************************/

    SChar  * sBuffer;
    vSLong   sRowCnt;

    IDE_TEST( STRUCT_ALLOC_WITH_SIZE( aStatement->qmxMem,
                                      SChar,
                                      QD_MAX_SQL_LENGTH,
                                      &sBuffer )
              != IDE_SUCCESS );

    (void) idlOS::snprintf( sBuffer, QD_MAX_SQL_LENGTH,
                            "DELETE FROM SYS_CONSTRAINT_RELATED_ "
                            "WHERE  CONSTRAINT_ID = "QCM_SQL_UINT32_FMT,
                            aConstraintID );

    IDE_TEST( qcg::runDMLforDDL( QC_SMI_STMT( aStatement ),
                                 sBuffer,
                                 &sRowCnt )
              != IDE_SUCCESS );

    /* [original code] no error is raised, even if no row is deleted. */

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qcmProc::relIsUsedProcByConstraint( qcStatement    * aStatement,
                                           qcNamePosition * aRelatedProcName,
                                           UInt             aRelatedUserID,
                                           idBool         * aIsUsed )
{
/***********************************************************************
 *
 * Description :
 *    Procedure�� Constraint���� ��� ������ Ȯ���Ѵ�.
 *
 * Implementation :
 *    SYS_CONSTRAINT_RELATED_ ��Ÿ ���̺��� Procedure�� �˻��Ѵ�.
 *
 ***********************************************************************/

    smiRange              sRange;
    qtcMetaRangeColumn    sFirstMetaRange;
    qtcMetaRangeColumn    sSecondMetaRange;

    qcNameCharBuffer      sRelatedProcNameBuffer;
    mtdCharType         * sRelatedProcName = (mtdCharType *)&sRelatedProcNameBuffer;
    mtdIntegerType        sRelatedUserID;

    smiTableCursor        sCursor;
    idBool                sCursorOpened = ID_FALSE;
    const void          * sRow;
    scGRID                sRid;
    smiCursorProperties   sProperty;
    mtcColumn           * sFstMtcColumn;
    mtcColumn           * sSndMtcColumn;

    qtc::setVarcharValue( sRelatedProcName,
                          NULL,
                          aRelatedProcName->stmtText + aRelatedProcName->offset,
                          aRelatedProcName->size );
    sRelatedUserID = (mtdIntegerType)aRelatedUserID;

    IDE_TEST( smiGetTableColumns( gQcmConstraintRelated,
                                  QCM_CONSTRAINT_RELATED_RELATEDPROCNAME_COL_ORDER,
                                  (const smiColumn**)&sFstMtcColumn )
              != IDE_SUCCESS );

    IDE_TEST( smiGetTableColumns( gQcmConstraintRelated,
                                  QCM_CONSTRAINT_RELATED_RELATEDUSERID_COL_ORDER,
                                  (const smiColumn**)&sSndMtcColumn )
              != IDE_SUCCESS );

    qciMisc::makeMetaRangeDoubleColumn(
        &sFirstMetaRange,
        &sSecondMetaRange,
        sFstMtcColumn,
        (const void *)sRelatedProcName,
        sSndMtcColumn,
        &sRelatedUserID,
        &sRange );

    sCursor.initialize();

    SMI_CURSOR_PROP_INIT_FOR_META_INDEX_SCAN( &sProperty, NULL );

    IDE_TEST( sCursor.open( QC_SMI_STMT( aStatement ),
                            gQcmConstraintRelated,
                            gQcmConstraintRelatedIndex[QCM_CONSTRAINT_RELATED_RELPROCNAME_RELUSERID],
                            smiGetRowSCN( gQcmConstraintRelated ),
                            NULL,
                            &sRange,
                            smiGetDefaultKeyRange(),
                            smiGetDefaultFilter(),
                            QCM_META_CURSOR_FLAG,
                            SMI_SELECT_CURSOR,
                            &sProperty )
              != IDE_SUCCESS );
    sCursorOpened = ID_TRUE;

    IDE_TEST( sCursor.beforeFirst() != IDE_SUCCESS );
    IDE_TEST( sCursor.readRow( &sRow, &sRid, SMI_FIND_NEXT ) != IDE_SUCCESS );

    if ( sRow != NULL )
    {
        *aIsUsed = ID_TRUE;
    }
    else
    {
        *aIsUsed = ID_FALSE;
    }

    sCursorOpened = ID_FALSE;
    IDE_TEST( sCursor.close() != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if ( sCursorOpened == ID_TRUE )
    {
        (void)sCursor.close();
    }
    else
    {
        /* Nothing to do */
    }

    return IDE_FAILURE;
}


/*************************************************************
                       qcmIndexRelated
**************************************************************/
IDE_RC qcmProc::relInsertRelatedToIndex(
    qcStatement        * aStatement,
    qsIndexRelatedProc * aRelatedProc )
{
/***********************************************************************
 *
 * Description :
 *    Index�� ���õ� Procedure�� ���� ������ �Է��Ѵ�.
 *
 * Implementation :
 *    SYS_INDEX_RELATED_ ��Ÿ ���̺� �����͸� �Է��Ѵ�.
 *
 ***********************************************************************/

    vSLong   sRowCnt;
    SChar    sRelatedProcName[QC_MAX_OBJECT_NAME_LEN + 1];
    SChar  * sBuffer;

    QC_STR_COPY( sRelatedProcName, aRelatedProc->relatedProcName );

    IDE_TEST( STRUCT_ALLOC_WITH_SIZE( aStatement->qmxMem,
                                      SChar,
                                      QD_MAX_SQL_LENGTH,
                                      &sBuffer )
              != IDE_SUCCESS );

    (void) idlOS::snprintf( sBuffer, QD_MAX_SQL_LENGTH,
                            "INSERT INTO SYS_INDEX_RELATED_ VALUES ( "
                            QCM_SQL_UINT32_FMT", "          // 1
                            QCM_SQL_UINT32_FMT", "          // 2
                            QCM_SQL_UINT32_FMT", "          // 3
                            QCM_SQL_UINT32_FMT", "          // 4
                            QCM_SQL_CHAR_FMT" ) ",          // 5
                            aRelatedProc->userID,           // 1
                            aRelatedProc->tableID,          // 2
                            aRelatedProc->indexID,          // 3
                            aRelatedProc->relatedUserID,    // 4
                            sRelatedProcName );             // 5

    IDE_TEST( qcg::runDMLforDDL( QC_SMI_STMT( aStatement ),
                                 sBuffer,
                                 &sRowCnt )
              != IDE_SUCCESS );

    IDE_TEST_RAISE( sRowCnt != 1, ERR_INSERTED_COUNT_IS_NOT_1 );

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_INSERTED_COUNT_IS_NOT_1 );
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QCM_INTERNAL_ARG,
                                  "[qcmProc::relInsertRelatedToIndex] ERR_INSERTED_COUNT_IS_NOT_1" ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qcmProc::relRemoveRelatedToIndexByUserID( qcStatement * aStatement,
                                                 UInt          aUserID )
{
/***********************************************************************
 *
 * Description :
 *    Index�� ���õ� Procedure�� ���� ������ �����Ѵ�.
 *
 * Implementation :
 *    ����� User ID�� �ش��ϴ� �����͸� SYS_INDEX_RELATED_
 *    ��Ÿ ���̺��� �����Ѵ�.
 *
 ***********************************************************************/

    SChar  * sBuffer;
    vSLong   sRowCnt;

    IDE_TEST( STRUCT_ALLOC_WITH_SIZE( aStatement->qmxMem,
                                      SChar,
                                      QD_MAX_SQL_LENGTH,
                                      &sBuffer )
              != IDE_SUCCESS );

    (void) idlOS::snprintf( sBuffer, QD_MAX_SQL_LENGTH,
                            "DELETE FROM SYS_INDEX_RELATED_ "
                            "WHERE  USER_ID = "QCM_SQL_UINT32_FMT,
                            aUserID );

    IDE_TEST( qcg::runDMLforDDL( QC_SMI_STMT( aStatement ),
                                 sBuffer,
                                 &sRowCnt )
              != IDE_SUCCESS );

    /* [original code] no error is raised, even if no row is deleted. */

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qcmProc::relRemoveRelatedToIndexByTableID( qcStatement * aStatement,
                                                  UInt          aTableID )
{
/***********************************************************************
 *
 * Description :
 *    Index�� ���õ� Procedure�� ���� ������ �����Ѵ�.
 *
 * Implementation :
 *    ����� Table ID�� �ش��ϴ� �����͸� SYS_INDEX_RELATED_
 *    ��Ÿ ���̺��� �����Ѵ�.
 *
 ***********************************************************************/

    SChar  * sBuffer;
    vSLong   sRowCnt;

    IDE_TEST( STRUCT_ALLOC_WITH_SIZE( aStatement->qmxMem,
                                      SChar,
                                      QD_MAX_SQL_LENGTH,
                                      &sBuffer )
              != IDE_SUCCESS );

    (void) idlOS::snprintf( sBuffer, QD_MAX_SQL_LENGTH,
                            "DELETE FROM SYS_INDEX_RELATED_ "
                            "WHERE  TABLE_ID = "QCM_SQL_UINT32_FMT,
                            aTableID );

    IDE_TEST( qcg::runDMLforDDL( QC_SMI_STMT( aStatement ),
                                 sBuffer,
                                 &sRowCnt )
              != IDE_SUCCESS );

    /* [original code] no error is raised, even if no row is deleted. */

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qcmProc::relRemoveRelatedToIndexByIndexID( qcStatement * aStatement,
                                                  UInt          aIndexID )
{
/***********************************************************************
 *
 * Description :
 *    Index�� ���õ� Procedure�� ���� ������ �����Ѵ�.
 *
 * Implementation :
 *    ����� Index ID�� �ش��ϴ� �����͸� SYS_INDEX_RELATED_
 *    ��Ÿ ���̺��� �����Ѵ�.
 *
 ***********************************************************************/

    SChar  * sBuffer;
    vSLong   sRowCnt;

    IDE_TEST( STRUCT_ALLOC_WITH_SIZE( aStatement->qmxMem,
                                      SChar,
                                      QD_MAX_SQL_LENGTH,
                                      &sBuffer )
              != IDE_SUCCESS );

    (void) idlOS::snprintf( sBuffer, QD_MAX_SQL_LENGTH,
                            "DELETE FROM SYS_INDEX_RELATED_ "
                            "WHERE  INDEX_ID = "QCM_SQL_UINT32_FMT,
                            aIndexID );

    IDE_TEST( qcg::runDMLforDDL( QC_SMI_STMT( aStatement ),
                                 sBuffer,
                                 &sRowCnt )
              != IDE_SUCCESS );

    /* [original code] no error is raised, even if no row is deleted. */

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qcmProc::relIsUsedProcByIndex( qcStatement    * aStatement,
                                      qcNamePosition * aRelatedProcName,
                                      UInt             aRelatedUserID,
                                      idBool         * aIsUsed )
{
/***********************************************************************
 *
 * Description :
 *    Procedure�� Index���� ��� ������ Ȯ���Ѵ�.
 *
 * Implementation :
 *    SYS_INDEX_RELATED_ ��Ÿ ���̺��� Procedure�� �˻��Ѵ�.
 *
 ***********************************************************************/

    smiRange              sRange;
    qtcMetaRangeColumn    sFirstMetaRange;
    qtcMetaRangeColumn    sSecondMetaRange;

    qcNameCharBuffer      sRelatedProcNameBuffer;
    mtdCharType         * sRelatedProcName = (mtdCharType *)&sRelatedProcNameBuffer;
    mtdIntegerType        sRelatedUserID;

    smiTableCursor        sCursor;
    idBool                sCursorOpened = ID_FALSE;
    const void          * sRow;
    scGRID                sRid;
    smiCursorProperties   sProperty;
    mtcColumn           * sFstMtcColumn;
    mtcColumn           * sSndMtcColumn;

    qtc::setVarcharValue( sRelatedProcName,
                          NULL,
                          aRelatedProcName->stmtText + aRelatedProcName->offset,
                          aRelatedProcName->size );
    sRelatedUserID = (mtdIntegerType)aRelatedUserID;

    IDE_TEST( smiGetTableColumns( gQcmIndexRelated,
                                  QCM_INDEX_RELATED_RELATEDPROCNAME_COL_ORDER,
                                  (const smiColumn**)&sFstMtcColumn )
              != IDE_SUCCESS );

    IDE_TEST( smiGetTableColumns( gQcmIndexRelated,
                                  QCM_INDEX_RELATED_RELATEDUSERID_COL_ORDER,
                                  (const smiColumn**)&sSndMtcColumn )
              != IDE_SUCCESS );

    qciMisc::makeMetaRangeDoubleColumn(
        &sFirstMetaRange,
        &sSecondMetaRange,
        sFstMtcColumn,
        (const void *)sRelatedProcName,
        sSndMtcColumn,
        &sRelatedUserID,
        &sRange );

    sCursor.initialize();

    SMI_CURSOR_PROP_INIT_FOR_META_INDEX_SCAN( &sProperty, NULL );

    IDE_TEST( sCursor.open( QC_SMI_STMT( aStatement ),
                            gQcmIndexRelated,
                            gQcmIndexRelatedIndex[QCM_INDEX_RELATED_RELPROCNAME_RELUSERID],
                            smiGetRowSCN( gQcmIndexRelated ),
                            NULL,
                            &sRange,
                            smiGetDefaultKeyRange(),
                            smiGetDefaultFilter(),
                            QCM_META_CURSOR_FLAG,
                            SMI_SELECT_CURSOR,
                            &sProperty )
              != IDE_SUCCESS );
    sCursorOpened = ID_TRUE;

    IDE_TEST( sCursor.beforeFirst() != IDE_SUCCESS );
    IDE_TEST( sCursor.readRow( &sRow, &sRid, SMI_FIND_NEXT ) != IDE_SUCCESS );

    if ( sRow != NULL )
    {
        *aIsUsed = ID_TRUE;
    }
    else
    {
        *aIsUsed = ID_FALSE;
    }

    sCursorOpened = ID_FALSE;
    IDE_TEST( sCursor.close() != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if ( sCursorOpened == ID_TRUE )
    {
        (void)sCursor.close();
    }
    else
    {
        /* Nothing to do */
    }

    return IDE_FAILURE;
}


IDE_RC qcmSetProcUserIDOfQcmProcedures(
    idvSQL     * /* aStatistics */,
    const void * aRow,
    SInt       * aUserID )
{
/*******************************************************************
 * Description : To Fix BUG-19839
 *               Procedure oid �� ����� �������� UserID�� �˻�
 *
 * Implementation : 
 ********************************************************************/

    mtcColumn * sUserIDMtcColumn;

    IDE_TEST( smiGetTableColumns( gQcmProcedures,
                                  QCM_PROCEDURES_USERID_COL_ORDER,
                                  (const smiColumn**)&sUserIDMtcColumn )
              != IDE_SUCCESS );

    qcm::getIntegerFieldValue(
        aRow,
        sUserIDMtcColumn,
        aUserID );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


IDE_RC qcmSetProcObjTypeOfQcmProcedures(
    idvSQL     * /* aStatistics */,
    const void * aRow,
    SInt       * aProcObjType )
{
    mtcColumn            * sObjectTypeMtcColumn;

    IDE_TEST( smiGetTableColumns( gQcmProcedures,
                                  QCM_PROCEDURES_OBJECTTYP_COL_ORDER,
                                  (const smiColumn**)&sObjectTypeMtcColumn )
              != IDE_SUCCESS );

    qcm::getIntegerFieldValue(
        aRow,
        sObjectTypeMtcColumn,
        aProcObjType );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


IDE_RC qcmProc::getProcUserID ( qcStatement * aStatement,
                                qsOID         aProcOID,
                                UInt        * aProcUserID )
{
/*******************************************************************
 * Description : To Fix BUG-19839
 *               Procedure oid �� ����� �������� UserID�� �˻�
 *
 * Implementation : 
 ********************************************************************/
    
    smiRange           sRange;
    qtcMetaRangeColumn sMetaRange;
    vSLong             sRowCount;
    SLong              sProcOID;
    mtcColumn        * sProcIDMtcColumn;

    sProcOID = QCM_OID_TO_BIGINT( aProcOID );

    IDE_TEST( smiGetTableColumns( gQcmProcedures,
                                  QCM_PROCEDURES_PROCOID_COL_ORDER,
                                  (const smiColumn**)&sProcIDMtcColumn )
              != IDE_SUCCESS );

    (void)qcm::makeMetaRangeSingleColumn( & sMetaRange,
                                          sProcIDMtcColumn,
                                          & sProcOID,
                                          & sRange );

    IDE_TEST( qcm::selectRow( QC_SMI_STMT( aStatement ),
                              gQcmProcedures,
                              smiGetDefaultFilter(),
                              & sRange,
                              gQcmProceduresIndex
                              [ QCM_PROCEDURES_PROCOID_IDX_ORDER ],
                              (qcmSetStructMemberFunc ) qcmSetProcUserIDOfQcmProcedures,
                              aProcUserID,
                              0,
                              1,
                              & sRowCount )
              != IDE_SUCCESS );

    IDE_TEST_RAISE( sRowCount != 1, err_selected_count_is_not_1 );

    return IDE_SUCCESS;

    IDE_EXCEPTION( err_selected_count_is_not_1 );
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QCM_INTERNAL_ARG,
                   "[qcmProc::getProcUserID]"
                   "err_selected_count_is_not_1" ));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


IDE_RC qcmProc::getProcObjType( qcStatement * aStatement,
                                qsOID         aProcOID,
                                SInt        * aProcObjType )
{
/***********************************************************************
 *
 * Description :  PROJ-1075 TYPESET
 *                procedure oid�� ���� object type�� �˻�.
 *                typeset �߰��� object type�� ����
 *                ddl ���࿩�θ� �����ϱ� ����.
 * Implementation :
 *
 ***********************************************************************/

    smiRange           sRange;
    qtcMetaRangeColumn sMetaRange;
    vSLong             sRowCount;
    SLong              sProcOID;
    mtcColumn        * sProcIDMtcColumn;

    // To fix BUG-14439
    // OID�� keyrange�� ����� ���� bigint Ÿ������ ����ؾ� ��.
    sProcOID = QCM_OID_TO_BIGINT( aProcOID );

    IDE_TEST( smiGetTableColumns( gQcmProcedures,
                                  QCM_PROCEDURES_PROCOID_COL_ORDER,
                                  (const smiColumn**)&sProcIDMtcColumn )
              != IDE_SUCCESS );
    (void)qcm::makeMetaRangeSingleColumn( & sMetaRange,
                                          sProcIDMtcColumn,
                                          & sProcOID,
                                          & sRange );

    IDE_TEST( qcm::selectRow( QC_SMI_STMT( aStatement ),
                              gQcmProcedures,
                              smiGetDefaultFilter(),
                              & sRange,
                              gQcmProceduresIndex
                              [ QCM_PROCEDURES_PROCOID_IDX_ORDER ],
                              (qcmSetStructMemberFunc ) qcmSetProcObjTypeOfQcmProcedures,
                              aProcObjType,
                              0,
                              1,
                              & sRowCount )
              != IDE_SUCCESS );

    IDE_TEST_RAISE( sRowCount != 1, err_selected_count_is_not_1 );

    return IDE_SUCCESS;

    IDE_EXCEPTION( err_selected_count_is_not_1 );
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QCM_INTERNAL_ARG,
                                "[qcmProc::getProcObjType]"
                                "err_selected_count_is_not_1" ));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*************************************************************
                     SetMember functions
          for more members, see qcmProc.cpp rev.1.11
**************************************************************/

IDE_RC qcmProc::procSetMember(
    idvSQL        * /* aStatistics */,
    const void    * aRow,
    qcmProcedures * aProcedures )
{
    SLong       sSLongOID;
    mtcColumn * sUserIDMtcColumn;
    mtcColumn * sProcIDMtcColumn;
    mtcColumn * sProcNameMtcColumn;

    IDE_TEST( smiGetTableColumns( gQcmProcedures,
                                  QCM_PROCEDURES_USERID_COL_ORDER,
                                  (const smiColumn**)&sUserIDMtcColumn )
              != IDE_SUCCESS );

    qcm::getIntegerFieldValue(
        aRow,
        sUserIDMtcColumn,
        & aProcedures->userID);

    IDE_TEST( smiGetTableColumns( gQcmProcedures,
                                  QCM_PROCEDURES_PROCOID_COL_ORDER,
                                  (const smiColumn**)&sProcIDMtcColumn )
              != IDE_SUCCESS );

    qcm::getBigintFieldValue (
        aRow,
        sProcIDMtcColumn,
        & sSLongOID);
    // BUGBUG 32bit machine���� ���� �� SLong(64bit)������ uVLong(32bit)������
    // ��ȯ�ϹǷ� ������ �ս� ���ɼ� ����
    aProcedures->procOID = (qsOID)sSLongOID;

    IDE_TEST( smiGetTableColumns( gQcmProcedures,
                                  QCM_PROCEDURES_PROCNAME_COL_ORDER,
                                  (const smiColumn**)&sProcNameMtcColumn )
              != IDE_SUCCESS );

    qcm::getCharFieldValue (
        aRow,
        sProcNameMtcColumn,
        aProcedures->procName);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}



/* ------------------------------------------------
 *  X$PROCTEXT
 * ----------------------------------------------*/

#define QCM_PROC_TEXT_LEN  64

typedef struct qcmProcText
{
    ULong  mProcOID;
    UInt   mPiece;
    SChar *mText;

}qcmProcText;


static iduFixedTableColDesc gPROCTEXTColDesc[] =
{
    {
        (SChar *)"PROC_OID",
        offsetof(qcmProcText, mProcOID),
        IDU_FT_SIZEOF(qcmProcText, mProcOID),
        IDU_FT_TYPE_UBIGINT | IDU_FT_COLUMN_INDEX,
        NULL,
        0, 0,NULL // for internal use
    },

    {
        (SChar *)"PIECE",
        offsetof(qcmProcText, mPiece),
        IDU_FT_SIZEOF(qcmProcText, mPiece),
        IDU_FT_TYPE_UINTEGER,
        NULL,
        0, 0,NULL // for internal use
    },

    {
        (SChar *)"TEXT",
        offsetof(qcmProcText, mText),
        QCM_PROC_TEXT_LEN,
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


/*************************************************************
            For Fixed Table
**************************************************************/

// ���ڸ� �ӽ÷� �ѱ�� ����.
typedef struct qcmTempFixedTableInfo
{
    void                  *mHeader;
    iduFixedTableMemory   *mMemory;
} qcmTempFixedTableInfo;


IDE_RC qcmProc::procSelectAllForFixedTable (
    smiStatement         * aSmiStmt,
    vSLong                 aMaxProcedureCount,
    vSLong               * aSelectedProcedureCount,
    void                 * aFixedTableInfo )
{
    vSLong selectedRowCount;

    IDE_TEST( qcm::selectRow
              (
                  aSmiStmt,
                  gQcmProcedures,
                  smiGetDefaultFilter(), /* a_callback */
                  smiGetDefaultKeyRange(), /* a_range */
                  NULL, /* a_index */
                  (qcmSetStructMemberFunc ) qcmProc::buildProcText,
                  aFixedTableInfo,
                  0, /* distance */
                  aMaxProcedureCount,
                  &selectedRowCount
                  ) != IDE_SUCCESS );

    *aSelectedProcedureCount = (vSLong) selectedRowCount;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


IDE_RC qcmProc::buildProcText( idvSQL        * aStatistics,
                               const void    * aRow,
                               void          * aFixedTableInfo )
{
    UInt           sState = 0;
    qsOID          sOID;
    SLong          sSLongOID;
    SInt           sProcLen = 0;
    qcmProcText    sProcText;
    SChar        * sProcBuf = NULL;
    qcStatement    sStatement;
    mtcColumn    * sProcIDMtcColumn;
    // PROJ-2446
    smiTrans       sMetaTx;
    smiStatement * sDummySmiStmt;
    smiStatement   sSmiStmt;
    UInt           sProcState = 0;
    qcmTempFixedTableInfo * sFixedTableInfo;

    void         * sIndexValues[1];
    SChar        * sIndex;
    SChar        * sStartIndex;
    SChar        * sPrevIndex;
    SInt           sCurrPos = 0;
    SInt           sCurrLen = 0;
    SInt           sSeqNo   = 0;
    SChar          sParseStr [ QCM_PROC_TEXT_LEN * 2 + 2 ] = { 0, };
         
    const mtlModule* sModule;

    IDE_TEST(sMetaTx.initialize() != IDE_SUCCESS);
    sProcState = 1;

    IDE_TEST(sMetaTx.begin(&sDummySmiStmt,
                           aStatistics,
                           (SMI_TRANSACTION_UNTOUCHABLE |
                            SMI_ISOLATION_CONSISTENT    |
                            SMI_COMMIT_WRITE_NOWAIT))
             != IDE_SUCCESS);
    sProcState = 2;

    IDE_TEST(sSmiStmt.begin( aStatistics,
                             sDummySmiStmt,
                             SMI_STATEMENT_UNTOUCHABLE |
                             SMI_STATEMENT_MEMORY_CURSOR)
             != IDE_SUCCESS);
    sProcState = 3;

    /* ------------------------------------------------
     *  Get Proc OID
     * ----------------------------------------------*/
    IDE_TEST( smiGetTableColumns( gQcmProcedures,
                                  QCM_PROCEDURES_PROCOID_COL_ORDER,
                                  (const smiColumn**)&sProcIDMtcColumn )
              != IDE_SUCCESS );

    qcm::getBigintFieldValue (
        aRow,
        sProcIDMtcColumn,
        & sSLongOID);
    // BUGBUG 32bit machine���� ���� �� SLong(64bit)������ uVLong(32bit)������
    // ��ȯ�ϹǷ� ������ �ս� ���ɼ� ����
    sOID = (qsOID)sSLongOID;
    sIndexValues[0] = &sOID;
    sFixedTableInfo = (qcmTempFixedTableInfo *)aFixedTableInfo;

    /* BUG-43006 FixedTable Indexing Filter
     * Column Index �� ����ؼ� ��ü Record�� ���������ʰ�
     * �κи� ������ Filtering �Ѵ�.
     * 1. void * �迭�� IDU_FT_COLUMN_INDEX �� ������ �÷���
     * �ش��ϴ� ���� ������� �־��־�� �Ѵ�.
     * 2. IDU_FT_COLUMN_INDEX�� �÷��� �ش��ϴ� ���� ��� ��
     * �� �־���Ѵ�.
     */
    if ( iduFixedTable::checkKeyRange( sFixedTableInfo->mMemory,
                                       gPROCTEXTColDesc,
                                       sIndexValues )
         == ID_TRUE )
    {
        /* ------------------------------------------------
         *  Generate Proc Text
         * ----------------------------------------------*/

        if (qsxProc::latchS( sOID ) == IDE_SUCCESS)
        {
            sState = 1;

            IDE_TEST( qcg::allocStatement( & sStatement,
                                           NULL,
                                           NULL,
                                           NULL )
                      != IDE_SUCCESS);

            sState = 2;

            sProcBuf = NULL; // allocate inside
            IDE_TEST(qsxProc::getProcText(&sStatement,
                                          sOID,
                                          &sProcBuf,
                                          &sProcLen)
                     != IDE_SUCCESS);

            sProcText.mProcOID = sOID;

            // BUG-44978
            sModule     = mtl::mDBCharSet;
            sStartIndex = sProcBuf;
            sIndex      = sStartIndex;

            while(1)
            {
                sPrevIndex = sIndex;

                (void)sModule->nextCharPtr( (UChar**) &sIndex,
                                            (UChar*) ( sProcBuf + sProcLen ));

                if (( sProcBuf + sProcLen ) <= sIndex )
                {
                    // ������ �� ���.
                    // ����� �� �� break.
                    sSeqNo++;

                    sCurrPos = sStartIndex - sProcBuf;
                    sCurrLen = sIndex - sStartIndex;

                    qcg::prsCopyStrDupAppo( sParseStr,
                                            sProcBuf + sCurrPos,
                                            sCurrLen );

                    sProcText.mPiece = sSeqNo;
                    sProcText.mText  = sParseStr;

                    IDE_TEST(iduFixedTable::buildRecord(((qcmTempFixedTableInfo *)aFixedTableInfo)->mHeader,
                                                        ((qcmTempFixedTableInfo *)aFixedTableInfo)->mMemory,
                                                        (void *) &sProcText)
                             != IDE_SUCCESS);
                    break;
                }
                else
                {
                    if( sIndex - sStartIndex >= QCM_PROC_TEXT_LEN )
                    {
                        // ���� ������ �� ����, �дٺ��� 64����Ʈ �Ǵ� �ʰ��� ����
                        // �Ǿ��� �� �߶� ���
                        sCurrPos = sStartIndex - sProcBuf;
                
                        if( sIndex - sStartIndex == QCM_PROC_TEXT_LEN )
                        {
                            // �� �������� ���
                            sCurrLen = QCM_PROC_TEXT_LEN;
                            sStartIndex = sIndex;
                        }
                        else
                        {
                            // �������� ��� �� ���� ĳ���� ��ġ���� ���
                            sCurrLen = sPrevIndex - sStartIndex;
                            sStartIndex = sPrevIndex;
                        }

                        sSeqNo++;

                        qcg::prsCopyStrDupAppo( sParseStr,
                                                sProcBuf + sCurrPos,
                                                sCurrLen );

                        sProcText.mPiece  = sSeqNo;
                        sProcText.mText   = sParseStr;

                        IDE_TEST(iduFixedTable::buildRecord(((qcmTempFixedTableInfo *)aFixedTableInfo)->mHeader,
                                                            ((qcmTempFixedTableInfo *)aFixedTableInfo)->mMemory,
                                                            (void *) &sProcText)
                                 != IDE_SUCCESS);
                    }
                    else
                    {
                        // Nothing to do.
                    }
                }
            }
        
            sState = 1;

            IDE_TEST(qcg::freeStatement(&sStatement) != IDE_SUCCESS);

            sState = 0;

            IDE_TEST(qsxProc::unlatch( sOID )
                     != IDE_SUCCESS);
        }
        else
        {
#if defined(DEBUG)
            ideLog::log(IDE_QP_0, "Latch Error \n");
#endif
        }
    }
    else
    {
        /* Nothing to do */
    }

    sProcState = 2;
    IDE_TEST(sSmiStmt.end(SMI_STATEMENT_RESULT_SUCCESS) != IDE_SUCCESS);

    sProcState = 1;
    IDE_TEST(sMetaTx.commit() != IDE_SUCCESS);

    sProcState = 0;
    IDE_TEST(sMetaTx.destroy( aStatistics ) != IDE_SUCCESS);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    switch(sState)
    {
        case 2:
            IDE_ASSERT(qcg::freeStatement(&sStatement) == IDE_SUCCESS);
        case 1:
            /* PROJ-2446 ONE SOURCE XDB BUGBUG statement pointer�� �Ѱܾ��Ѵ�. */
            IDE_ASSERT(qsxProc::unlatch( sOID )
                       == IDE_SUCCESS);
        case 0:
            //nothing
            break;
        default:
            IDE_CALLBACK_FATAL("Can't be here");
    }

    switch(sProcState)
    {
        case 3:
            IDE_ASSERT(sSmiStmt.end(SMI_STATEMENT_RESULT_FAILURE) == IDE_SUCCESS);

        case 2:
            IDE_ASSERT(sMetaTx.commit() == IDE_SUCCESS);

        case 1:
            IDE_ASSERT(sMetaTx.destroy( aStatistics ) == IDE_SUCCESS);

        case 0:
            //nothing
            break;
        default:
            IDE_CALLBACK_FATAL("Can't be here");
    }

    return IDE_FAILURE ;
}

IDE_RC qcmProc::buildRecordForPROCTEXT( idvSQL      * aStatistics,
                                        void        * aHeader,
                                        void        * /* aDumpObj */,
                                        iduFixedTableMemory   *aMemory)
{
    smiTrans               sMetaTx;
    smiStatement          *sDummySmiStmt;
    smiStatement           sStatement;
    vSLong                 sRecCount;
    qcmTempFixedTableInfo  sInfo;
    UInt                   sState = 0;

    sInfo.mHeader = aHeader;
    sInfo.mMemory = aMemory;

    /* ------------------------------------------------
     *  Build Text Record
     * ----------------------------------------------*/

    IDE_TEST(sMetaTx.initialize() != IDE_SUCCESS);
    sState = 1;

    IDE_TEST(sMetaTx.begin(&sDummySmiStmt,
                           aStatistics,
                           (SMI_TRANSACTION_UNTOUCHABLE |
                            SMI_ISOLATION_CONSISTENT    |
                            SMI_COMMIT_WRITE_NOWAIT))
             != IDE_SUCCESS);
    sState = 2;

    IDE_TEST(sStatement.begin( aStatistics,
                               sDummySmiStmt,
                               SMI_STATEMENT_UNTOUCHABLE |
                               SMI_STATEMENT_MEMORY_CURSOR)
             != IDE_SUCCESS);
    sState = 3;


    IDE_TEST(procSelectAllForFixedTable(&sStatement,
                                        ID_UINT_MAX,
                                        &sRecCount,
                                        &sInfo) != IDE_SUCCESS);

    sState = 2;
    IDE_TEST(sStatement.end(SMI_STATEMENT_RESULT_SUCCESS) != IDE_SUCCESS);

    sState = 1;
    IDE_TEST(sMetaTx.commit() != IDE_SUCCESS);

    sState = 0;
    IDE_TEST(sMetaTx.destroy( aStatistics ) != IDE_SUCCESS);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    {
        switch(sState)
        {
            case 3:
                IDE_ASSERT(sStatement.end(SMI_STATEMENT_RESULT_FAILURE) == IDE_SUCCESS);

            case 2:
                IDE_ASSERT(sMetaTx.commit() == IDE_SUCCESS);

            case 1:
                IDE_ASSERT(sMetaTx.destroy( aStatistics ) == IDE_SUCCESS);

            case 0:
                //nothing
                break;
            default:
                IDE_CALLBACK_FATAL("Can't be here");
        }
        return IDE_FAILURE;
    }
}


iduFixedTableDesc gPROCTEXTTableDesc =
{
    (SChar *)"X$PROCTEXT",
    qcmProc::buildRecordForPROCTEXT,
    gPROCTEXTColDesc,
    IDU_STARTUP_META,
    0,
    0,
    IDU_FT_DESC_TRANS_NOT_USE,
    NULL
};


typedef struct qcmProcInfo
{
    ULong mProcOID;
    UInt  mModifyCount;
    UInt  mStatus;
    UInt  mSessionID;
    UInt  mProcType;
    UInt  mShardSplitMethod; // TASK-7244 Set shard split method to PSM info
}qcmProcInfo;

// PROJ-2717 Internal procedure
//   X$PROCINFO
//     PROC_OID
//     MODIFY_COUNT
//     Status [VALID(0), INVALID(1)]
//     SESSION_ID
//     PROC_TYPE [NORMAL(0), EXTERNAL_C(1), INTERNAL_C(2)]
//     SHARD_SPLIT_METHOD [NONE(0), HASH(1), RANGE(2), LIST(3), CLONE(4), SOLO(5)]

static iduFixedTableColDesc gPROCInfoColDesc[] =
{
    {
        (SChar *)"PROC_OID",
        offsetof(qcmProcInfo, mProcOID),
        IDU_FT_SIZEOF(qcmProcInfo, mProcOID),
        IDU_FT_TYPE_UBIGINT,
        NULL,
        0, 0,NULL // for internal use
    },

    {
        (SChar *)"MODIFY_COUNT",
        offsetof(qcmProcInfo, mModifyCount),
        IDU_FT_SIZEOF(qcmProcInfo, mModifyCount),
        IDU_FT_TYPE_UINTEGER,
        NULL,
        0, 0,NULL // for internal use
    },

    {
        (SChar *)"STATUS",
        offsetof(qcmProcInfo, mStatus),
        IDU_FT_SIZEOF(qcmProcInfo, mStatus),
        IDU_FT_TYPE_UINTEGER,
        NULL,
        0, 0,NULL // for internal use
    },

    {
        (SChar *)"SESSION_ID",
        offsetof(qcmProcInfo, mSessionID),
        IDU_FT_SIZEOF(qcmProcInfo, mSessionID),
        IDU_FT_TYPE_UINTEGER,
        NULL,
        0, 0,NULL // for internal use
    },

    {
        (SChar *)"PROC_TYPE",
        offsetof(qcmProcInfo, mProcType),
        IDU_FT_SIZEOF(qcmProcInfo, mProcType),
        IDU_FT_TYPE_UINTEGER,
        NULL,
        0, 0,NULL // for internal use
    },

    // TASK-7244 Set shard split method to PSM info
    {
        (SChar *)"SHARD_SPLIT_METHOD",
        offsetof(qcmProcInfo, mShardSplitMethod),
        IDU_FT_SIZEOF(qcmProcInfo, mShardSplitMethod),
        IDU_FT_TYPE_UINTEGER,
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

iduFixedTableDesc gPROCInfoDesc =
{
    (SChar *)"X$PROCINFO",
    qcmProc::buildRecordForPROCInfo,
    gPROCInfoColDesc,
    IDU_STARTUP_META,
    0,
    0,
    IDU_FT_DESC_TRANS_NOT_USE,
    NULL
};



IDE_RC qcmProc::buildRecordForPROCInfo( idvSQL      * aStatistics,
                                        void        * aHeader,
                                        void        * /* aDumpObj */,
                                        iduFixedTableMemory   *aMemory)
{
    smiTrans               sMetaTx;
    smiStatement          *sDummySmiStmt;
    smiStatement           sStatement;
    vSLong                 sRecCount;
    qcmTempFixedTableInfo  sInfo;
    UInt                   sState = 0;

    sInfo.mHeader = aHeader;
    sInfo.mMemory = aMemory;

    /* ------------------------------------------------
     *  Build Info Record
     * ----------------------------------------------*/

    IDE_TEST(sMetaTx.initialize( ) != IDE_SUCCESS);
    sState = 1;

    IDE_TEST(sMetaTx.begin(&sDummySmiStmt,
                           aStatistics,
                           (SMI_TRANSACTION_UNTOUCHABLE |
                            SMI_ISOLATION_CONSISTENT    |
                            SMI_COMMIT_WRITE_NOWAIT))
             != IDE_SUCCESS);
    sState = 2;

    IDE_TEST(sStatement.begin( aStatistics,
                               sDummySmiStmt,
                               SMI_STATEMENT_UNTOUCHABLE |
                               SMI_STATEMENT_MEMORY_CURSOR)
             != IDE_SUCCESS);
    sState = 3;


    IDE_TEST(procSelectAllForBuildProcInfo(&sStatement,
                                           ID_UINT_MAX,
                                           &sRecCount,
                                           &sInfo) != IDE_SUCCESS);

    sState = 2;
    IDE_TEST(sStatement.end(SMI_STATEMENT_RESULT_SUCCESS) != IDE_SUCCESS);

    sState = 1;
    IDE_TEST(sMetaTx.commit() != IDE_SUCCESS);

    sState = 0;
    IDE_TEST(sMetaTx.destroy( aStatistics ) != IDE_SUCCESS);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    {
        switch(sState)
        {
            case 3:
                IDE_ASSERT(sStatement.end(SMI_STATEMENT_RESULT_FAILURE) == IDE_SUCCESS);

            case 2:
                IDE_ASSERT(sMetaTx.commit() == IDE_SUCCESS);

            case 1:
                IDE_ASSERT(sMetaTx.destroy( aStatistics ) == IDE_SUCCESS);

            case 0:
                //nothing
                break;
            default:
                IDE_CALLBACK_FATAL("Can't be here");
        }
        return IDE_FAILURE;
    }
}


IDE_RC qcmProc::procSelectAllForBuildProcInfo(
    smiStatement         * aSmiStmt,
    vSLong                 aMaxProcedureCount,
    vSLong               * aSelectedProcedureCount,
    void                 * aFixedTableInfo )
{
    vSLong selectedRowCount;

    IDE_TEST( qcm::selectRow
              (
                  aSmiStmt,
                  gQcmProcedures,
                  smiGetDefaultFilter(), /* a_callback */
                  smiGetDefaultKeyRange(), /* a_range */
                  NULL, /* a_index */
                  (qcmSetStructMemberFunc ) qcmProc::buildProcInfo,
                  aFixedTableInfo,
                  0, /* distance */
                  aMaxProcedureCount,
                  &selectedRowCount
                  ) != IDE_SUCCESS );

    *aSelectedProcedureCount = (vSLong) selectedRowCount;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qcmProc::buildProcInfo( idvSQL        * aStatistics,
                               const void    * aRow,
                               void          * aFixedTableInfo )
{
    UInt           sState = 0;
    qsOID          sOID;
    SLong          sSLongOID;
    mtcColumn    * sProcIDMtcColumn;
    // PROJ-2446
    smiTrans       sMetaTx;
    smiStatement * sDummySmiStmt;
    smiStatement   sSmiStmt;
    UInt           sProcState = 0;
    qsxProcInfo  * sProcInfo;
    qcmProcInfo    sQcmProcInfo;
         
    IDE_TEST(sMetaTx.initialize( ) != IDE_SUCCESS);
    sProcState = 1;

    IDE_TEST(sMetaTx.begin(&sDummySmiStmt,
                           aStatistics,
                           (SMI_TRANSACTION_UNTOUCHABLE |
                            SMI_ISOLATION_CONSISTENT    |
                            SMI_COMMIT_WRITE_NOWAIT))
             != IDE_SUCCESS);
    sProcState = 2;

    IDE_TEST(sSmiStmt.begin( aStatistics,
                             sDummySmiStmt,
                             SMI_STATEMENT_UNTOUCHABLE |
                             SMI_STATEMENT_MEMORY_CURSOR)
             != IDE_SUCCESS);
    sProcState = 3;

    /* ------------------------------------------------
     *  Get Proc OID
     * ----------------------------------------------*/
    IDE_TEST( smiGetTableColumns( gQcmProcedures,
                                  QCM_PROCEDURES_PROCOID_COL_ORDER,
                                  (const smiColumn**)&sProcIDMtcColumn )
              != IDE_SUCCESS );

    qcm::getBigintFieldValue (
        aRow,
        sProcIDMtcColumn,
        & sSLongOID);
    // BUGBUG 32bit machine���� ���� �� SLong(64bit)������ uVLong(32bit)������
    // ��ȯ�ϹǷ� ������ �ս� ���ɼ� ����
    sOID = (qsOID)sSLongOID;

    /* ------------------------------------------------
     *  Generate Proc Info 
     * ----------------------------------------------*/

    if (qsxProc::latchS( sOID ) == IDE_SUCCESS)
    {
        sState = 1;

        IDE_TEST(qsxProc::getProcInfo( sOID,
                                       &sProcInfo)
                 != IDE_SUCCESS);

        sQcmProcInfo.mProcOID     = sProcInfo->procOID;
        sQcmProcInfo.mModifyCount = sProcInfo->modifyCount;
        sQcmProcInfo.mStatus      = (sProcInfo->isValid == ID_TRUE)?0:1;
        sQcmProcInfo.mSessionID   = sProcInfo->sessionID;
        // TASK-7244 Set shard split method to PSM info
        sQcmProcInfo.mShardSplitMethod = sProcInfo->shardSplitMethod;

        if ( sProcInfo->planTree!= NULL )
        {
            sQcmProcInfo.mProcType = sProcInfo->planTree->procType;
        }
        else
        {
            sQcmProcInfo.mProcType = ID_UINT_MAX;
        }

        IDE_TEST(iduFixedTable::buildRecord(((qcmTempFixedTableInfo *)aFixedTableInfo)->mHeader,
                                            ((qcmTempFixedTableInfo *)aFixedTableInfo)->mMemory,
                                            (void *) &sQcmProcInfo)
                 != IDE_SUCCESS);

        sState = 0;
        IDE_TEST(qsxProc::unlatch( sOID )
                 != IDE_SUCCESS);
    }
    else
    {
#if defined(DEBUG)
        ideLog::log(IDE_QP_0, "Latch Error \n");
#endif
    }

    sProcState = 2;
    IDE_TEST(sSmiStmt.end(SMI_STATEMENT_RESULT_SUCCESS) != IDE_SUCCESS);

    sProcState = 1;
    IDE_TEST(sMetaTx.commit() != IDE_SUCCESS);

    sProcState = 0;
    IDE_TEST(sMetaTx.destroy( aStatistics ) != IDE_SUCCESS);

    return IDE_SUCCESS;
    IDE_EXCEPTION_END;

    switch(sState)
    {
        case 1:
            /* PROJ-2446 ONE SOURCE XDB BUGBUG statement pointer�� �Ѱܾ��Ѵ�. */
            IDE_ASSERT(qsxProc::unlatch( sOID )
                       == IDE_SUCCESS);
        case 0:
            //nothing
            break;
        default:
            IDE_CALLBACK_FATAL("Can't be here");
    }

    switch(sProcState)
    {
        case 3:
            IDE_ASSERT(sSmiStmt.end(SMI_STATEMENT_RESULT_FAILURE) == IDE_SUCCESS);

        case 2:
            IDE_ASSERT(sMetaTx.commit() == IDE_SUCCESS);

        case 1:
            IDE_ASSERT(sMetaTx.destroy( aStatistics ) == IDE_SUCCESS);

        case 0:
            //nothing
            break;
        default:
            IDE_CALLBACK_FATAL("Can't be here");
    }

    return IDE_FAILURE ;
}

