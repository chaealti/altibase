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
 * $Id: qcmPkg.cpp 88191 2020-07-27 03:08:54Z mason.lee $
 **********************************************************************/

#include <idl.h>
#include <ide.h>
#include <idu.h>
#include <qcuProperty.h>
#include <qcm.h>
#include <qcg.h>
#include <qsvEnv.h>
#include <qcmPkg.h>
#include <qcmProc.h>
#include <qcmView.h>
#include <qsxPkg.h>
#include <qdParseTree.h>
#include <qcmUser.h>
#include <qdpDrop.h>
#include <mtuProperty.h>
#include <mtf.h>

// PROJ-1073 Package
const void * gQcmPkgs;
const void * gQcmPkgParas;
const void * gQcmPkgParse;
const void * gQcmPkgRelated;
const void * gQcmPkgsIndex      [ QCM_MAX_META_INDICES ];
const void * gQcmPkgParasIndex  [ QCM_MAX_META_INDICES ];
const void * gQcmPkgParseIndex  [ QCM_MAX_META_INDICES ];
const void * gQcmPkgRelatedIndex[ QCM_MAX_META_INDICES ];

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

IDE_RC qcmPkg::getTypeFieldValues (
    qcStatement * aStatement,
    qtcNode     * aVarNode,
    SChar       * aVarType,
    SChar       * aVarLang,
    SChar       * aVarSize ,
    SChar       * aVarPrecision,
    SChar       * aVarScale,
    UInt          aMaxBufferLength
    )
{
    mtcColumn * sVarNodeColumn = NULL;

    sVarNodeColumn = QTC_TMPL_COLUMN( QC_PRIVATE_TMPLATE(aStatement),
                                             aVarNode );

    idlOS::snprintf( aVarType, aMaxBufferLength,
                     QCM_SQL_INT32_FMT,
                     (SInt) sVarNodeColumn-> type. dataTypeId );

    idlOS::snprintf( aVarLang, aMaxBufferLength,
                     QCM_SQL_INT32_FMT,
                     (SInt) sVarNodeColumn-> type. languageId );

    idlOS::snprintf( aVarSize , aMaxBufferLength,
                     QCM_SQL_UINT32_FMT,
                     sVarNodeColumn-> column. size);

    idlOS::snprintf( aVarPrecision , aMaxBufferLength,
                     QCM_SQL_INT32_FMT,
                     sVarNodeColumn->precision );

    idlOS::snprintf( aVarScale , aMaxBufferLength,
                     QCM_SQL_INT32_FMT,
                     sVarNodeColumn->scale );

    return IDE_SUCCESS;
}

/*************************************************************
                      main functions
**************************************************************/
IDE_RC qcmPkg::insert (
    qcStatement      * aStatement,
    qsPkgParseTree   * aPkgParse )
{
    qsRelatedObjects  * sRelObjs;
    qsRelatedObjects  * sTmpRelObjs;
    qsRelatedObjects  * sNewRelObj;
    idBool              sExist = ID_FALSE;
    qsOID               sPkgBodyOID;

    IDE_TEST( qcmPkg::pkgInsert ( aStatement,
                                  aPkgParse )
              != IDE_SUCCESS );

    if ( aPkgParse->objType == QS_PKG )
    {
        IDE_TEST( qcmPkg::paraInsert( aStatement,
                                      aPkgParse )
                  != IDE_SUCCESS );
    }
    else
    {
        /* PKG Body�� ������ ���� paraInsert�� ȣ������ �ʴ´�.
         * SYS_PACKAGE_PARAS_�� spec�� �ִ� procedure/function��
         * ���ؼ��� parameter ������ �����Ѵ�.*/
    }

    IDE_TEST( qcmPkg::prsInsert( aStatement,
                                 aPkgParse )
              != IDE_SUCCESS );

    /* PROJ-2197 PSM Renewal
       aStatement->spvEnv->relatedObjects ���
       aPkgParse->PkgInfo->relatedObjects�� ����Ѵ�. */
    for( sRelObjs = aPkgParse->pkgInfo->relatedObjects ;
         sRelObjs != NULL ;
         sRelObjs = sRelObjs->next )
    {
        sExist = ID_FALSE;

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
            IDE_TEST( qcmPkg::relInsert( aStatement,
                                         aPkgParse,
                                         sRelObjs )
                      != IDE_SUCCESS );

            // BUG-36975
            if( sRelObjs->objectType == QS_PKG )
            {
                if ( ( sRelObjs->userID != aPkgParse->userID ) || 
                     ( ( idlOS::strMatch( aPkgParse->pkgNamePos.stmtText + aPkgParse->pkgNamePos.offset,
                                          aPkgParse->pkgNamePos.size,
                                          sRelObjs->objectName.name,
                                          idlOS::strlen( sRelObjs->objectName.name ) ) ) != 0 ) )
                {
                    if( getPkgExistWithEmptyByNamePtr( aStatement,
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
                            IDE_TEST( makeRelatedObjectNodeForInsertMeta(
                                    aStatement,
                                    sRelObjs->userID,
                                    sPkgBodyOID,
                                    sRelObjs->objectNamePos,
                                    (SInt)QS_PKG_BODY,
                                    &sNewRelObj )
                                != IDE_SUCCESS );

                            IDE_TEST( qcmPkg::relInsert( aStatement,
                                                         aPkgParse,
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
                }
            }
            else
            {
                // Nothing to do.
                // package�� spec�� body�� ���еȴ�.
            }
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

IDE_RC qcmPkg::remove (
    qcStatement      * aStatement,
    qsOID              aPkgOID,
    qcNamePosition     aUserNamePos,
    qcNamePosition     aPkgNamePos,
    qsObjectType       aPkgType,
    idBool             aPreservePrivInfo )
{
    UInt sUserID;

    IDE_TEST( qcmUser::getUserID( aStatement,
                                  aUserNamePos,
                                  &sUserID ) != IDE_SUCCESS );

    IDE_TEST( remove( aStatement,
                      aPkgOID,
                      sUserID,
                      aPkgNamePos.stmtText + aPkgNamePos.offset,
                      aPkgNamePos.size,
                      aPkgType,
                      aPreservePrivInfo )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qcmPkg::remove (
    qcStatement      * aStatement,
    qsOID              aPkgOID,
    UInt               aUserID,
    SChar            * aPkgName,
    UInt               aPkgNameSize,
    qsObjectType       aPkgType,
    idBool             aPreservePrivInfo )
{
    if ( aPkgType == QS_PKG )
    {
        //-----------------------------------------------
        // related Pkg
        //-----------------------------------------------
        IDE_TEST( qcmPkg::relSetInvalidPkgOfRelated (
                      aStatement,
                      aUserID,
                      aPkgName,
                      aPkgNameSize,
                      QS_PKG )
                  != IDE_SUCCESS );

        IDE_TEST( qcmProc::relSetInvalidProcOfRelated (
                      aStatement,
                      aUserID,
                      aPkgName,
                      aPkgNameSize,
                      QS_PKG )
                  != IDE_SUCCESS );

        //-----------------------------------------------
        // related view
        //-----------------------------------------------
        IDE_TEST( qcmView::setInvalidViewOfRelated(
                      aStatement,
                      aUserID,
                      aPkgName,
                      aPkgNameSize,
                      QS_PKG )
                  != IDE_SUCCESS );

        //-----------------------------------------------
        // remove
        //-----------------------------------------------
        IDE_TEST( qcmPkg::paraRemoveAll( aStatement,
                                         aPkgOID )
                  != IDE_SUCCESS );
    }
    else
    {
        // Nothing to do.
    }

    IDE_TEST( qcmPkg::pkgRemove( aStatement,
                                 aPkgOID )
              != IDE_SUCCESS );

    IDE_TEST( qcmPkg::prsRemoveAll( aStatement,
                                    aPkgOID )
              != IDE_SUCCESS );

    IDE_TEST( qcmPkg::relRemoveAll( aStatement,
                                    aPkgOID )
              != IDE_SUCCESS );

    if( aPreservePrivInfo != ID_TRUE )
    {
        IDE_TEST( qdpDrop::removePriv4DropPkg( aStatement,
                                               aPkgOID )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qcmPkg::pkgInsert(
    qcStatement      * aStatement,
    qsPkgParseTree   * aPkgParse )
{
/***********************************************************************
 *
 * Description :
 *    createPkgOrFunc �ÿ� ��Ÿ ���̺� �Է�
 *
 * Implementation :
 *    ��õ� ParseTree �κ��� SYS_PACKAGES_ ��Ÿ ���̺� �Է��� �����͸�
 *    ������ �Ŀ� �Է� ������ ���� ����
 *
 ***********************************************************************/
    vSLong      sRowCnt;
    SChar       sPkgName[ QC_MAX_OBJECT_NAME_LEN + 1 ];
    SInt        sAuthidType = 0;
    SChar     * sBuffer;

    QC_STR_COPY( sPkgName, aPkgParse->pkgNamePos );

    /* BUG-45306 PSM AUTHID */
    if ( aPkgParse->isDefiner == ID_TRUE )
    {
        sAuthidType = (SInt)QCM_PKG_DEFINER;
    }
    else
    {
        sAuthidType = (SInt)QCM_PKG_CURRENT_USER;
    }

    IDE_TEST( STRUCT_ALLOC_WITH_SIZE( aStatement->qmxMem,
                                      SChar,
                                      QD_MAX_SQL_LENGTH,
                                      &sBuffer )
              != IDE_SUCCESS);

    idlOS::snprintf( sBuffer, QD_MAX_SQL_LENGTH,
                     "INSERT INTO SYS_PACKAGES_ VALUES ( "
                     QCM_SQL_UINT32_FMT", "     // 1 USER_ID
                     QCM_SQL_BIGINT_FMT", "     // 2 PACKAGE_OID
                     QCM_SQL_CHAR_FMT", "       // 3 PACKAGE_NAME
                     QCM_SQL_INT32_FMT", "      // 4 PACKAGE_TYPE
                     QCM_SQL_INT32_FMT", "      // 5 STATUS
                     "SYSDATE, SYSDATE, "       // 6, 7 CREATED LAST_DDL_TIME
                     QCM_SQL_INT32_FMT") ",     // 8 AUTHID
                     aPkgParse->userID,                            // 1 USER_ID
                     QCM_OID_TO_BIGINT( aPkgParse->pkgOID ),       // 2 PACKAGE_OID
                     sPkgName,                                     // 3 PACKAGE_NAME
                     (SInt) aPkgParse->objType,                    // 4 PACKAGE_TYPE
                     (SInt)
                         ((aPkgParse->pkgInfo->isValid == ID_TRUE)
                         ? QCM_PKG_VALID : QCM_PKG_INVALID ),      // 5 STATUS
                     sAuthidType );                                // 8 AUTHID

    IDE_TEST( qcg::runDMLforDDL( QC_SMI_STMT( aStatement ),
                                 sBuffer,
                                 & sRowCnt )
              != IDE_SUCCESS );

    IDE_TEST_RAISE( sRowCnt != 1, err_inserted_count_is_not_1 );

    return IDE_SUCCESS;

    IDE_EXCEPTION( err_inserted_count_is_not_1 );
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QCM_INTERNAL_ARG,
                                "[qcmPkg::pkgInsert] err_inserted_count_is_not_1" ));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qcmSetPkgOIDOfQcmPkgs(
    idvSQL              * /* aStatistics */,
    const void          * aRow,
    qsOID               * aPkgID )
{
    SLong       sSLongID;
    mtcColumn * sPkgOIDMtcColumn;

    IDE_TEST( smiGetTableColumns( gQcmPkgs,
                                  QCM_PKGS_PKGOID_COL_ORDER,
                                  (const smiColumn**)&sPkgOIDMtcColumn )
              != IDE_SUCCESS );

    qcm::getBigintFieldValue (
        aRow,
        sPkgOIDMtcColumn,
        & sSLongID );
    // BUGBUG 32bit machine���� ���� �� SLong(64bit)������ uVLong(32bit)������
    // ��ȯ�ϹǷ� ������ �ս� ���ɼ� ����
    *aPkgID = (qsOID)sSLongID;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qcmPkg::getPkgExistWithEmptyByName(
    qcStatement     * aStatement,
    UInt              aUserID,
    qcNamePosition    aPkgName,
    qsObjectType      aObjType,
    qsOID           * aPkgOID )
{
    IDE_TEST(
        getPkgExistWithEmptyByNamePtr(
            aStatement,
            aUserID,
            (SChar*)(aPkgName.stmtText + aPkgName.offset),
            aPkgName.size,
            aObjType,
            aPkgOID)
        != IDE_SUCCESS);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qcmPkg::getPkgExistWithEmptyByNamePtr(
    qcStatement     * aStatement,
    UInt              aUserID,
    SChar           * aPkgName,
    SInt              aPkgNameSize,
    qsObjectType      aObjType,
    qsOID           * aPkgOID )
{
    smiRange              sRange;
    qtcMetaRangeColumn    sFirstMetaRange;
    qtcMetaRangeColumn    sSecondMetaRange;
    qtcMetaRangeColumn    sThirdMetaRange;
    mtdIntegerType        sUserIdData;
    mtdIntegerType        sObjTypeData;
    vSLong                sRowCount;
    qsOID                 sSelectedPkgOID;
    qcNameCharBuffer      sPkgNameBuffer;
    mtdCharType         * sPkgName = ( mtdCharType * ) &sPkgNameBuffer;
    mtcColumn           * sFirsttMtcColumn;
    mtcColumn           * sSecondMtcColumn;
    mtcColumn           * sThirdMtcColumn;

    if ( aPkgNameSize > QC_MAX_OBJECT_NAME_LEN )
    {
        *aPkgOID = QS_EMPTY_OID;
        IDE_CONT( NORMAL_EXIT );
    }
    else
    {
        // Nothing to do.
    }

    qtc::setVarcharValue( sPkgName,
                          NULL,
                          aPkgName,
                          aPkgNameSize );

    sUserIdData = ( mtdIntegerType ) aUserID;

    sObjTypeData = ( mtdIntegerType ) aObjType;

    IDE_TEST( smiGetTableColumns( gQcmPkgs,
                                  QCM_PKGS_PKGNAME_COL_ORDER,
                                  (const smiColumn**)&sFirsttMtcColumn )
              != IDE_SUCCESS );

    IDE_TEST( smiGetTableColumns( gQcmPkgs,
                                  QCM_PKGS_PKGTYPE_COL_ORDER,
                                  (const smiColumn**)&sSecondMtcColumn )
              != IDE_SUCCESS );

    IDE_TEST( smiGetTableColumns( gQcmPkgs,
                                  QCM_PKGS_USERID_COL_ORDER,
                                  (const smiColumn**)&sThirdMtcColumn )
              != IDE_SUCCESS );

    qcm::makeMetaRangeTripleColumn( & sFirstMetaRange,
                                    & sSecondMetaRange,
                                    & sThirdMetaRange,
                                    sFirsttMtcColumn,
                                    ( const void * ) sPkgName,
                                    sSecondMtcColumn,
                                    & sObjTypeData,
                                    sThirdMtcColumn,
                                    & sUserIdData,
                                    & sRange );

    IDE_TEST( qcm::selectRow(
                  QC_SMI_STMT( aStatement ),
                  gQcmPkgs,
                  smiGetDefaultFilter(),
                  & sRange,
                  gQcmPkgsIndex
                  [ QCM_PKGS_PKGNAME_PKGTYPE_USERID_IDX_ORDER ],
                  (qcmSetStructMemberFunc ) qcmSetPkgOIDOfQcmPkgs,
                  & sSelectedPkgOID,
                  0,
                  1,
                  & sRowCount )
              != IDE_SUCCESS );

    if (sRowCount == 1)
    {
        ( *aPkgOID ) = sSelectedPkgOID;
    }
    else
    {
        if (sRowCount == 0)
        {
            ( *aPkgOID ) = QS_EMPTY_OID;
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
                                "[qcmPkg::getPkgExistWithEmptyByName]"
                                "err_selected_count_is_not_1" ));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qcmPkg::pkgUpdateStatus(
    qcStatement             * aStatement,
    qsOID                     aPkgOID,
    qcmPkgStatusType          aStatus)
{
/***********************************************************************
 *
 * Description :
 *    alterPkgOrFunc, recompile, rebuild �ÿ� ��Ÿ ���̺� ����
 *
 * Implementation :
 *    ��õ� pkgOID, status ������ SYS_PACKAGES_ ��Ÿ ���̺���
 *    STATUS ���� �����Ѵ�.
 *
 ***********************************************************************/
    SChar  sBuffer[QD_MAX_SQL_LENGTH];
    vSLong sRowCnt;

    idlOS::snprintf( sBuffer, QD_MAX_SQL_LENGTH,
                     "UPDATE SYS_PACKAGES_ "
                     "SET    STATUS   = "QCM_SQL_INT32_FMT", "
                     "       LAST_DDL_TIME = SYSDATE "
                     "WHERE  PACKAGE_OID = "QCM_SQL_BIGINT_FMT,
                     (SInt) aStatus,
                     QCM_OID_TO_BIGINT( aPkgOID ) );

    IDE_TEST( qcg::runDMLforDDL( QC_SMI_STMT( aStatement ),
                                 sBuffer,
                                 & sRowCnt )
              != IDE_SUCCESS );

    IDE_TEST_RAISE( sRowCnt != 1, err_updated_count_is_not_1 );

    return IDE_SUCCESS;

    IDE_EXCEPTION( err_updated_count_is_not_1 );
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QCM_INTERNAL_ARG,
                                "[qcmPkg::pkgUpdateStatus]"
                                "err_updated_count_is_not_1" ));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qcmPkg::pkgUpdateStatusTx(
    qcStatement             * aQcStmt,
    qsOID                     aPkgOID,
    qcmPkgStatusType          aStatus)
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

    if ( qcmPkg::pkgUpdateStatus( aQcStmt,
                                  aPkgOID,
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

IDE_RC qcmPkg::pkgRemove(
    qcStatement     * aStatement,
    qsOID             aPkgOID )
{
/***********************************************************************
 *
 * Description :
 *    replace, drop �ÿ� ��Ÿ ���̺��� ����
 *
 * Implementation :
 *    ��õ� PkgOID �� �ش��ϴ� �����͸� SYS_PACKAGES_ ��Ÿ ���̺���
 *    �����Ѵ�.
 *
 ***********************************************************************/
    SChar    * sBuffer;
    vSLong     sRowCnt;

    IDE_TEST(STRUCT_ALLOC_WITH_SIZE(aStatement->qmxMem,
                                    SChar,
                                    QD_MAX_SQL_LENGTH,
                                    &sBuffer)
             != IDE_SUCCESS);

    idlOS::snprintf( sBuffer, QD_MAX_SQL_LENGTH,
                     "DELETE FROM SYS_PACKAGES_ "
                     "WHERE  PACKAGE_OID = "QCM_SQL_BIGINT_FMT,
                     QCM_OID_TO_BIGINT( aPkgOID ) );

    IDE_TEST( qcg::runDMLforDDL( QC_SMI_STMT( aStatement ),
                                 sBuffer,
                                 & sRowCnt )
              != IDE_SUCCESS );

    IDE_TEST_RAISE( sRowCnt != 1, err_removed_count_is_not_1 );

    return IDE_SUCCESS;

    IDE_EXCEPTION( err_removed_count_is_not_1 );
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QCM_INTERNAL_ARG,
                                "[qcmPkg::pkgRemove]"
                                "err_removed_count_is_not_1" ));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qcmPkg::pkgSelectCountWithObjectType (
    smiStatement         * aSmiStmt,
    UInt                   aObjType,
    vSLong               * aRowCount )
{
    vSLong                selectedRowCount;
    qtcMetaRangeColumn    sFirstMetaRange;
    smiRange              sRange;
    mtdIntegerType        sIntData;
    mtcColumn            * sPkgTypeMtcColumn;

    sIntData = (mtdIntegerType) aObjType;

    IDE_TEST( smiGetTableColumns( gQcmPkgs,
                                  QCM_PKGS_PKGTYPE_COL_ORDER,
                                  (const smiColumn**)&sPkgTypeMtcColumn )
              != IDE_SUCCESS );

    qcm::makeMetaRangeSingleColumn( & sFirstMetaRange,
                                    sPkgTypeMtcColumn,
                                    & sIntData,
                                    & sRange );

    IDE_TEST( qcm::selectCount(
                  aSmiStmt,
                  gQcmPkgs,
                  &selectedRowCount,
                  smiGetDefaultFilter(),
                  & sRange,
                  gQcmPkgsIndex
                  [ QCM_PKGS_PKGTYPE_IDX_ORDER ] )
              != IDE_SUCCESS );

    *aRowCount = (vSLong)selectedRowCount ;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qcmPkg::pkgSelectAllWithObjectType (
    smiStatement         * aSmiStmt,
    UInt                   aObjType,
    vSLong                 aMaxPkgCount,
    vSLong               * aSelectedPkgCount,
    qcmPkgs              * aPkgArray )
{
    vSLong                selectedRowCount;
    qtcMetaRangeColumn    sFirstMetaRange;
    smiRange              sRange;
    mtdIntegerType        sIntData;
    mtcColumn           * sPkgTypeMtcColumn;

    sIntData = (mtdIntegerType) aObjType;

    IDE_TEST( smiGetTableColumns( gQcmPkgs,
                                  QCM_PKGS_PKGTYPE_COL_ORDER,
                                  (const smiColumn**)&sPkgTypeMtcColumn )
              != IDE_SUCCESS );

    qcm::makeMetaRangeSingleColumn( & sFirstMetaRange,
                                    sPkgTypeMtcColumn,
                                    & sIntData,
                                    & sRange );

    IDE_TEST( qcm::selectRow(
            aSmiStmt,
            gQcmPkgs,
            smiGetDefaultFilter(), /* a_callback */
            &sRange,
            gQcmPkgsIndex[ QCM_PKGS_PKGTYPE_IDX_ORDER ],
            (qcmSetStructMemberFunc ) qcmPkg::pkgSetMember,
            aPkgArray,
            ID_SIZEOF(qcmPkgs),    /* distance */
            aMaxPkgCount,
            &selectedRowCount)
        != IDE_SUCCESS );

    *aSelectedPkgCount = (vSLong) selectedRowCount;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


IDE_RC qcmPkg::pkgSelectCountWithUserId (
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

    IDE_TEST( smiGetTableColumns( gQcmPkgs,
                                  QCM_PKGS_USERID_COL_ORDER,
                                  (const smiColumn**)&sUserIDMtcColumn )
              != IDE_SUCCESS );

    qcm::makeMetaRangeSingleColumn( & sFirstMetaRange,
                                    sUserIDMtcColumn,
                                    & sIntData,
                                    & sRange );

    IDE_TEST( qcm::selectCount
              (
                  QC_SMI_STMT( aStatement ),
                  gQcmPkgs,
                  &selectedRowCount,
                  smiGetDefaultFilter(),
                  & sRange,
                  gQcmPkgsIndex
                  [ QCM_PKGS_USERID_IDX_ORDER ]
              ) != IDE_SUCCESS );

    *aRowCount = (vSLong)selectedRowCount ;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qcmPkg::pkgSelectAllWithUserId (
    qcStatement          * aStatement,
    UInt                   aUserId,
    vSLong                 aMaxPkgCount,
    vSLong               * aSelectedPkgCount,
    qcmPkgs              * aPkgArray )
{
    vSLong                selectedRowCount;
    qtcMetaRangeColumn    sFirstMetaRange;
    smiRange              sRange;
    mtdIntegerType        sIntData;
    mtcColumn           * sUserIDMtcColumn;

    sIntData = (mtdIntegerType) aUserId;

    IDE_TEST( smiGetTableColumns( gQcmPkgs,
                                  QCM_PKGS_USERID_COL_ORDER,
                                  (const smiColumn**)&sUserIDMtcColumn )
              != IDE_SUCCESS );

    qcm::makeMetaRangeSingleColumn( & sFirstMetaRange,
                                    sUserIDMtcColumn,
                                    & sIntData,
                                    & sRange );

    IDE_TEST( qcm::selectRow(
            QC_SMI_STMT( aStatement ),
            gQcmPkgs,
            smiGetDefaultFilter(), /* a_callback */
            &sRange,
            gQcmPkgsIndex[ QCM_PKGS_USERID_IDX_ORDER ],
            (qcmSetStructMemberFunc ) qcmPkg::pkgSetMember,
            aPkgArray,
            ID_SIZEOF(qcmPkgs),    /* distance */
            aMaxPkgCount,
            &selectedRowCount)
        != IDE_SUCCESS );

    *aSelectedPkgCount = (vSLong) selectedRowCount;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*************************************************************
  qcmPkgParas
 **************************************************************/
IDE_RC qcmPkg::paraInsert(
    qcStatement     * aStatement,
    qsPkgParseTree  * aPkgParseTree )
{
/***********************************************************************
 *
 * Description :
 *    create �ÿ� ��Ÿ ���̺� ���ν����� ���� ���� �Է�
 *
 * Implementation :
 *    ��õ� ParseTree �κ��� ���������� �����Ͽ� SYS_PACKAGE_PARAS_
 *    ��Ÿ ���̺� �Է��ϴ� �������� ���� �� ����
 *
 ***********************************************************************/

    vSLong               sRowCnt;
    SInt                 sParaOrder;
    SChar                sPkgName             [ QC_MAX_OBJECT_NAME_LEN + 1    ];
    SChar                sSubProgramName      [ QC_MAX_OBJECT_NAME_LEN + 1    ];
    SChar                sParaName            [ QC_MAX_OBJECT_NAME_LEN + 1    ];
    SChar                sDefaultValueString  [ QCM_MAX_DEFAULT_VAL + 2];
    SChar              * sBuffer;
    SChar                sVarType      [ MAX_COLDATA_SQL_LENGTH ];
    SChar                sVarLang      [ MAX_COLDATA_SQL_LENGTH ];
    SChar                sVarSize      [ MAX_COLDATA_SQL_LENGTH ];
    SChar                sVarPrecision [ MAX_COLDATA_SQL_LENGTH ];
    SChar                sVarScale     [ MAX_COLDATA_SQL_LENGTH ];
    qsVariables        * sParaVar;
    qsVariableItems    * sParaDecls     = NULL;
    qsPkgSubprograms   * sPkgSubprogram = NULL;
    qsPkgStmts         * sPkgStmt = NULL;
    qsProcParseTree    * sProcParseTree = NULL;

    /* PROJ-1973 Package
     * qcmPkg::paraInsert �Լ��� package spec�� ������ ����
     * ȣ���� �� �ִ�. */
    IDE_DASSERT( aPkgParseTree->objType == QS_PKG );

    sPkgStmt = aPkgParseTree->block->subprograms;

    QC_STR_COPY( sPkgName, aPkgParseTree->pkgNamePos );

    while( sPkgStmt != NULL )
    {
        if( sPkgStmt->stmtType != QS_OBJECT_MAX )
        {
            sPkgSubprogram = ( qsPkgSubprograms * )sPkgStmt;

            if( sPkgSubprogram->parseTree != NULL )
            {
                sProcParseTree = sPkgSubprogram->parseTree;
                sParaDecls     = sProcParseTree->paraDecls;
                sParaOrder     = 0;

                QC_STR_COPY( sSubProgramName, sProcParseTree->procNamePos );

                IDE_TEST(STRUCT_ALLOC_WITH_SIZE(aStatement->qmxMem,
                                                SChar,
                                                QD_MAX_SQL_LENGTH,
                                                &sBuffer)
                         != IDE_SUCCESS);

                if ( sProcParseTree->returnTypeVar != NULL )  // stored function
                {
                    sParaOrder++;

                    IDE_TEST( qcmPkg::getTypeFieldValues( aStatement,
                                                          sProcParseTree->returnTypeVar->variableTypeNode,
                                                          sVarType,
                                                          sVarLang,
                                                          sVarSize,
                                                          sVarPrecision,
                                                          sVarScale,
                                                          MAX_COLDATA_SQL_LENGTH )
                              != IDE_SUCCESS );

                    idlOS::snprintf( sBuffer, QD_MAX_SQL_LENGTH,
                                     "INSERT INTO SYS_PACKAGE_PARAS_ VALUES ( "
                                     QCM_SQL_UINT32_FMT", "                     // 1 USER_ID
                                     QCM_SQL_CHAR_FMT", "                       // 2 OBJ_NAME
                                     QCM_SQL_CHAR_FMT", "                       // 3 PKG_NAME
                                     QCM_SQL_BIGINT_FMT", "                     // 4 OBJ_OID
                                     QCM_SQL_INT32_FMT", "                      // 5 SUB_ID
                                     QCM_SQL_INT32_FMT", "                      // 6 SUB_TYPE
                                     "NULL, "                                   // 7 PARA_NAME
                                     QCM_SQL_INT32_FMT", "                      // 8 PARA_ORDER
                                     "1, "                                      // 9 INOUT_TYPE (QS_OUT)
                                     "%s, "                                     // 10 DATA_TYPE
                                     "%s, "                                     // 11 LANG_ID
                                     "%s, "                                     // 12 SIZE
                                     "%s, "                                     // 13 PRECISION
                                     "%s, "                                     // 14 SCALE
                                     "NULL )",                                  // 15 DEFAULT_VAL
                                     aPkgParseTree->userID,                     // 1 USER_ID
                                     sSubProgramName,                           // 2 OBJ_NAME
                                     sPkgName                       ,           // 3 PKG_NAME
                                     QCM_OID_TO_BIGINT( aPkgParseTree->pkgOID ),// 4 OBJ_OID
                                     sPkgSubprogram->subprogramID,              // 5 SUB_ID
                                     sProcParseTree->objType,                   // 6 SUB_TYPE
                                     // 7 PARA_NAME (N/A)
                                     sParaOrder,                                // 8 PARA_ORDER
                                     // 9 INOUT_TYPE (N/A)
                                     sVarType,                                  // 10 DATA_TYPE
                                     sVarLang,                                  // 11 LANG_ID
                                     sVarSize,                                  // 12 SIZE
                                     sVarPrecision,                             // 13 PRECISION
                                     sVarScale );                               // 14 SCALE
                    // 15 DEFAULT_VAL (N/A)

                    IDE_TEST( qcg::runDMLforDDL( QC_SMI_STMT( aStatement ),
                                                 sBuffer,
                                                 & sRowCnt )
                              != IDE_SUCCESS );

                    IDE_TEST_RAISE( sRowCnt != 1, err_inserted_count_is_not_1 );
                }
                else // stored procedure
                {
                    if( sParaDecls == NULL )
                    {
                        idlOS::snprintf( sVarType,      MAX_COLDATA_SQL_LENGTH, "NULL");
                        idlOS::snprintf( sVarLang,      MAX_COLDATA_SQL_LENGTH, "NULL");
                        idlOS::snprintf( sVarSize,      MAX_COLDATA_SQL_LENGTH, "NULL");
                        idlOS::snprintf( sVarPrecision, MAX_COLDATA_SQL_LENGTH, "NULL");
                        idlOS::snprintf( sVarScale,     MAX_COLDATA_SQL_LENGTH, "NULL");

                        idlOS::snprintf( sBuffer, QD_MAX_SQL_LENGTH,
                                         "INSERT INTO SYS_PACKAGE_PARAS_ VALUES ( "
                                         QCM_SQL_UINT32_FMT", "                     // 1 USER_ID
                                         QCM_SQL_CHAR_FMT", "                       // 2 OBJ_NAME
                                         QCM_SQL_CHAR_FMT", "                       // 3 PKG_NAME
                                         QCM_SQL_BIGINT_FMT", "                     // 4 OBJ_OID
                                         QCM_SQL_INT32_FMT", "                      // 5 SUB_ID
                                         QCM_SQL_INT32_FMT", "                      // 6 SUB_TYPE
                                         "NULL, "                                   // 7 PARA_NAME
                                         QCM_SQL_INT32_FMT", "                      // 8 PARA_ORDER
                                         "0, "                                      // 9 INOUT_TYPE (QS_IN)
                                         "%s, "                                     // 10 DATA_TYPE
                                         "%s, "                                     // 11 LANG_ID
                                         "%s, "                                     // 12 SIZE
                                         "%s, "                                     // 13 PRECISION
                                         "%s, "                                     // 14 SCALE
                                         "NULL )",                                  // 15 DEFAULT_VAL
                                         aPkgParseTree->userID,                     // 1 USER_ID
                                         sSubProgramName,                           // 2 OBJ_NAME
                                         sPkgName                       ,           // 3 PKG_NAME
                                         QCM_OID_TO_BIGINT( aPkgParseTree->pkgOID ),// 4 OBJ_OID
                                         sPkgSubprogram->subprogramID,              // 5 SUB_ID
                                         sProcParseTree->objType,                   // 6 SUB_TYPE
                                         // 7 PARA_NAME (N/A)
                                         sParaOrder,                                // 8 PARA_ORDER
                                         // 9 INOUT_TYPE (N/A)
                                         sVarType,                                  // 10 DATA_TYPE
                                         sVarLang,                                  // 11 LANG_ID
                                         sVarSize,                                  // 12 SIZE
                                         sVarPrecision,                             // 13 PRECISION
                                         sVarScale );                               // 14 SCALE
                        // 15 DEFAULT_VAL (N/A)

                        IDE_TEST( qcg::runDMLforDDL( QC_SMI_STMT( aStatement ),
                                                     sBuffer,
                                                     & sRowCnt )
                                  != IDE_SUCCESS );

                        IDE_TEST_RAISE( sRowCnt != 1, err_inserted_count_is_not_1 );
                    }
                    else
                    {
                        /* Do Nothing */
                    }
                }

                if( sParaDecls != NULL )
                {
                    sParaVar = (qsVariables*)sParaDecls;
                }
                else
                {
                    sParaVar = NULL;
                }

                while( sParaVar != NULL )
                {
                    sParaOrder++ ;

                    QC_STR_COPY( sParaName, sParaVar->common.name );

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

                    IDE_TEST( qcmPkg::getTypeFieldValues( aStatement,
                                                          sParaVar-> variableTypeNode,
                                                          sVarType,
                                                          sVarLang,
                                                          sVarSize,
                                                          sVarPrecision,
                                                          sVarScale,
                                                          MAX_COLDATA_SQL_LENGTH )
                              != IDE_SUCCESS );

                    IDE_TEST(STRUCT_ALLOC_WITH_SIZE(aStatement->qmxMem,
                                                    SChar,
                                                    QD_MAX_SQL_LENGTH,
                                                    &sBuffer)
                             != IDE_SUCCESS);

                    idlOS::snprintf( sBuffer, QD_MAX_SQL_LENGTH,
                                     "INSERT INTO SYS_PACKAGE_PARAS_ VALUES ( "
                                     QCM_SQL_UINT32_FMT", "                     // 1 USER_ID
                                     QCM_SQL_CHAR_FMT", "                       // 2 OBJ_NAME
                                     QCM_SQL_CHAR_FMT", "                       // 3 PKG_NAME
                                     QCM_SQL_BIGINT_FMT", "                     // 4 OBJ_OID
                                     QCM_SQL_INT32_FMT", "                      // 5 SUB_ID
                                     QCM_SQL_INT32_FMT", "                      // 6 SUB_TYPE
                                     QCM_SQL_CHAR_FMT", "                       // 7 PARA_NAME
                                     QCM_SQL_INT32_FMT", "                      // 8 PARA_ORDER
                                     QCM_SQL_INT32_FMT", "                      // 9 INOUT_TYPE
                                     "%s, "                                     // 10 DATA_TYPE
                                     "%s, "                                     // 11 LANG_ID
                                     "%s, "                                     // 12 SIZE
                                     "%s, "                                     // 13 PRECISION
                                     "%s, "                                     // 14 SCALE
                                     QCM_SQL_CHAR_FMT" )",                      // 15 DEFAULT_VAL
                                     aPkgParseTree->userID,                     // 1 USER_ID
                                     sSubProgramName,                           // 2 OBJ_NAME
                                     sPkgName                       ,           // 3 PKG_NAME
                                     QCM_OID_TO_BIGINT( aPkgParseTree->pkgOID ),// 4 OBJ_OID
                                     sPkgSubprogram->subprogramID,              // 5 SUB_ID
                                     sProcParseTree->objType,                   // 6 SUB_TYPE
                                     sParaName,                                 // 7 PARA_NAME
                                     sParaOrder,                                // 8 PARA_ORDER
                                     (SInt) sParaVar->inOutType,                // 9 INOUT_TYPE
                                     sVarType,                                  // 10 DATA_TYPE
                                     sVarLang,                                  // 11 LANG_ID
                                     sVarSize,                                  // 12 SIZE
                                     sVarPrecision,                             // 13 PRECISION
                                     sVarScale,                                 // 14 SCALE
                                     sDefaultValueString );                     // 15 DEFAULT_VAL

                    IDE_TEST( qcg::runDMLforDDL( QC_SMI_STMT( aStatement ),
                                                 sBuffer,
                                                 & sRowCnt )
                              != IDE_SUCCESS );

                    IDE_TEST_RAISE( sRowCnt != 1, err_inserted_count_is_not_1 );

                    sParaVar = (qsVariables*)(sParaVar->common.next) ;
                }
            }
            else
            {
                /* Do Nothing */
            }
        }
        else
        {
            // Nothing to do.
        }

        sPkgStmt = sPkgStmt->next;
    } /* end of while */

    return IDE_SUCCESS;

    IDE_EXCEPTION( err_inserted_count_is_not_1 );
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QCM_INTERNAL_ARG,
                                "[qcmPkg::paraInsert] err_inserted_count_is_not_1" ));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


IDE_RC qcmPkg::paraRemoveAll(
    qcStatement     * aStatement,
    qsOID             aPkgOID)
{
/***********************************************************************
 *
 * Description :
 *    �����ÿ� ��Ÿ ���̺� ���ν����� ���� ���� ����
 *
 * Implementation :
 *    ��õ� PkgOID �� �ش��Ѵ� �����͸� SYS_PACKAGE_PARAS_ ��Ÿ ���̺���
 *    �����ϴ� �������� ���� �� ����
 *
 ***********************************************************************/

    SChar    * sBuffer;
    vSLong     sRowCnt;

    IDE_TEST(STRUCT_ALLOC_WITH_SIZE(aStatement->qmxMem,
                                    SChar,
                                    QD_MAX_SQL_LENGTH,
                                    &sBuffer)
             != IDE_SUCCESS);

    idlOS::snprintf( sBuffer, QD_MAX_SQL_LENGTH,
                     "DELETE FROM SYS_PACKAGE_PARAS_ "
                     "WHERE  PACKAGE_OID = "QCM_SQL_BIGINT_FMT,
                     QCM_OID_TO_BIGINT( aPkgOID ) );

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
  qcmPkgParse
 **************************************************************/
IDE_RC qcmPkg::prsInsert(
    qcStatement     * aStatement,
    qsPkgParseTree * aPkgParse )
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

    sNcharList = aPkgParse->ncharList;

    /* PROJ-2550 PSM Encryption
       system_.sys_package_parse_�� ��Ÿ���̺�����
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
            sStmtBuffer    = aStatement->myPlan->stmtText;
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

            // insert v_QCM_PKG_parse
            IDE_TEST( qcmPkg::prsInsertFragment(
                    aStatement,
                    sStmtBuffer,
                    aPkgParse,
                    sSeqNo,
                    sCurrPos,
                    sCurrLen )
                != IDE_SUCCESS );
            break;
        }
        else
        {
            if( sIndex - sStartIndex >= QCM_MAX_PKG_LEN )
            {
                // ���� ������ �� ����, �дٺ��� 100����Ʈ �Ǵ� �ʰ��� ����
                // �Ǿ��� �� �߶� ���
                sCurrPos = sStartIndex - sStmtBuffer;

                if( sIndex - sStartIndex == QCM_MAX_PKG_LEN )
                {
                    // �� �������� ���
                    sCurrLen = QCM_MAX_PKG_LEN;
                    sStartIndex = sIndex;
                }
                else
                {
                    // �������� ��� �� ���� ĳ���� ��ġ���� ���
                    sCurrLen = sPrevIndex - sStartIndex;
                    sStartIndex = sPrevIndex;
                }

                sSeqNo++;

                // insert v_QCM_PKG_parse
                IDE_TEST( qcmPkg::prsInsertFragment(
                        aStatement,
                        sStmtBuffer,
                        aPkgParse,
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

IDE_RC qcmPkg::prsCopyStrDupAppo (
    SChar         * aDest,
    SChar         * aSrc,
    UInt            aLength )
{
    IDU_FIT_POINT_FATAL( "qcmPkg::prsCopyStrDupAppo::__FT__" );

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

IDE_RC qcmPkg::prsInsertFragment(
    qcStatement     * aStatement,
    SChar           * aStmtBuffer,
    qsPkgParseTree  * aPkgParse,
    UInt              aSeqNo,
    UInt              aOffset,
    UInt              aLength )
{
/***********************************************************************
 *
 * Description :
 *    �����ÿ� ���� ���������� SYS_PACKAGE_PARSE_ �� ����
 *
 * Implementation :
 *    �����ÿ� ���� ���������� ������ ������� �������� �Բ� ���޵Ǹ�,
 *    SYS_PACKAGE_PARSE_ ��Ÿ ���̺� �Է��ϴ� ������ ���� ����
 *
 ***********************************************************************/

    vSLong sRowCnt;
    SChar  sParseStr [ QCM_MAX_PKG_LEN * 2 + 2 ];
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

    IDE_TEST(STRUCT_ALLOC_WITH_SIZE(aStatement->qmxMem,
                                    SChar,
                                    QD_MAX_SQL_LENGTH,
                                    &sBuffer)
             != IDE_SUCCESS);

    idlOS::snprintf( sBuffer, QD_MAX_SQL_LENGTH,
                     "INSERT INTO SYS_PACKAGE_PARSE_ VALUES ( "
                     QCM_SQL_UINT32_FMT", "                     // 1 USER_ID
                     QCM_SQL_BIGINT_FMT", "                     // 2 PACKAGE_OID
                     QCM_SQL_UINT32_FMT", "                     // 3 PACKAGE_TYPE
                     QCM_SQL_UINT32_FMT", "                     // 4 SEQ_NO
                     QCM_SQL_CHAR_FMT") ",                      // 5 PARSE
                     aPkgParse->userID,                         // 1 USER_ID
                     QCM_OID_TO_BIGINT( aPkgParse->pkgOID ),    // 2 PACKAGE_OID
                     (SInt) aPkgParse->objType,                 // 3 PACKAGE_TYPE
                     aSeqNo,                                    // 4 SEQ_NO
                     sParseStr );                               // 5 PARSE

    IDE_TEST( qcg::runDMLforDDL( QC_SMI_STMT( aStatement ),
                                 sBuffer,
                                 & sRowCnt )
              != IDE_SUCCESS );

    IDE_TEST_RAISE( sRowCnt != 1, err_inserted_count_is_not_1 );

    return IDE_SUCCESS;

    IDE_EXCEPTION( err_inserted_count_is_not_1 );
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QCM_INTERNAL_ARG,
                                "[qcmPkg::prsInsertFragment]"
                                "err_inserted_count_is_not_1" ));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


IDE_RC qcmPkg::prsRemoveAll(
    qcStatement     * aStatement,
    qsOID             aPkgOID)
{
/***********************************************************************
 *
 * Description :
 *    drop �ÿ� SYS_PACKAGE_PARSE_ ���̺�κ��� ����
 *
 * Implementation :
 *    SYS_PACKAGE_PARSE_ ��Ÿ ���̺��� ��õ� PkgID �� �����͸� �����Ѵ�.
 *
 ***********************************************************************/

    SChar  * sBuffer;
    vSLong   sRowCnt;

    IDE_TEST(STRUCT_ALLOC_WITH_SIZE(aStatement->qmxMem,
                                    SChar,
                                    QD_MAX_SQL_LENGTH,
                                    &sBuffer)
             != IDE_SUCCESS);

    idlOS::snprintf( sBuffer, QD_MAX_SQL_LENGTH,
                     "DELETE FROM SYS_PACKAGE_PARSE_ "
                     "WHERE  PACKAGE_OID = "QCM_SQL_BIGINT_FMT,
                     QCM_OID_TO_BIGINT( aPkgOID ) );

    IDE_TEST( qcg::runDMLforDDL( QC_SMI_STMT( aStatement ),
                                 sBuffer,
                                 & sRowCnt )
              != IDE_SUCCESS );

    // [original code] no error is raised even if no row is deleted.

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

IDE_RC qcmPkg::convertToUTypeString( qcStatement   * aStatement,
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
  qcmPkgRelated
 **************************************************************/
IDE_RC qcmPkg::relInsert(
    qcStatement         * aStatement,
    qsPkgParseTree     * aPkgParse,
    qsRelatedObjects    * aRelatedObjList )
{
/***********************************************************************
 *
 * Description :
 *    ���ν��� ������ ���õ� ������Ʈ�� ���� ������ �Է��Ѵ�.
 *
 * Implementation :
 *    SYS_PACKAGE_RELATED_ ��Ÿ ���̺� ����� ������Ʈ���� �Է��Ѵ�.
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

    IDE_TEST(STRUCT_ALLOC_WITH_SIZE(aStatement->qmxMem,
                                    SChar,
                                    QD_MAX_SQL_LENGTH,
                                    &sBuffer)
             != IDE_SUCCESS);

    if ( sRelUserID != QC_PUBLIC_USER_ID )
    {
        idlOS::snprintf( sBuffer, QD_MAX_SQL_LENGTH,
                         "INSERT INTO SYS_PACKAGE_RELATED_ VALUES ( "
                         QCM_SQL_UINT32_FMT", "                     // 1 USER_ID
                         QCM_SQL_BIGINT_FMT", "                     // 2 PACKAGE_OID
                         QCM_SQL_INT32_FMT", "                      // 3 RELATED_USER_ID
                         QCM_SQL_CHAR_FMT", "                       // 4 RELATED_OBJECT_NAME
                         QCM_SQL_INT32_FMT") ",                     // 5 RELATED_OBJECT_TYPE
                         aPkgParse-> userID,                        // 1 USER_ID
                         QCM_OID_TO_BIGINT( aPkgParse->pkgOID ),    // 2 PACKAGE_OID
                         sRelUserID,                                // 3 RELATED_USER_ID
                         sRelObjectName,                            // 4 RELATED_OBJECT_NAME
                         aRelatedObjList-> objectType );            // 5 RELATED_OBJECT_TYPE
    }
    else
    {
        idlOS::snprintf( sBuffer, QD_MAX_SQL_LENGTH,
                         "INSERT INTO SYS_PACKAGE_RELATED_ VALUES ( "
                         QCM_SQL_UINT32_FMT", "                     // 1 USER_ID
                         QCM_SQL_BIGINT_FMT", "                     // 2 PACKAGE_OID
                         "NULL, "                                   // (3) NULL value
                         QCM_SQL_CHAR_FMT", "                       // 4 RELATED_OBJECT_NAME
                         QCM_SQL_INT32_FMT") ",                     // 5 RELATED_OBJECT_TYPE
                         aPkgParse->userID,                         // 1 USER_ID
                         QCM_OID_TO_BIGINT( aPkgParse->pkgOID ),    // 2 PACKAGE_OID
                         sRelObjectName,                            // 4 RELATED_OBJECT_NAME
                         aRelatedObjList-> objectType );            // 5 RELATED_OBJECT_TYPE
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
                                "[qcmPkg::relInsert] err_inserted_count_is_not_1" ));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}


// for each row in SYS_PACKAGE_RELATED_ concered,
// update SYS_PACKAGES_ to QCM_PKG_INVALID
IDE_RC qcmModifyStatusOfRelatedPkgToInvalid (
    idvSQL              * /* aStatistics */,
    const void          * aRow,
    qcStatement         * aStatement )
{
    qsOID           sPkgOID;
    SLong           sSLongOID;
    mtcColumn     * sPkgOIDMtcColumn;

    IDE_TEST( smiGetTableColumns( gQcmPkgRelated,
                                  QCM_PKG_RELATED_PKGOID_COL_ORDER,
                                  (const smiColumn**)&sPkgOIDMtcColumn )
              != IDE_SUCCESS );

    qcm::getBigintFieldValue (
        aRow,
        sPkgOIDMtcColumn,
        & sSLongOID );
    // BUGBUG 32bit machine���� ���� �� SLong(64bit)������ uVLong(32bit)������
    // ��ȯ�ϹǷ� ������ �ս� ���ɼ� ����
    sPkgOID = (qsOID)sSLongOID;

    IDE_TEST( qsxPkg::makeStatusInvalid( aStatement,
                                         sPkgOID )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

// BUG-46416
IDE_RC qcmModifyStatusOfRelatedPkgToInvalidTx (
    idvSQL              * /* aStatistics */,
    const void          * aRow,
    qcStatement         * aStatement )
{
    qsOID           sPkgOID;
    SLong           sSLongOID;
    mtcColumn     * sPkgOIDMtcColumn;

    IDE_TEST( smiGetTableColumns( gQcmPkgRelated,
                                  QCM_PKG_RELATED_PKGOID_COL_ORDER,
                                  (const smiColumn**)&sPkgOIDMtcColumn )
              != IDE_SUCCESS );

    qcm::getBigintFieldValue (
        aRow,
        sPkgOIDMtcColumn,
        & sSLongOID );
    // BUGBUG 32bit machine���� ���� �� SLong(64bit)������ uVLong(32bit)������
    // ��ȯ�ϹǷ� ������ �ս� ���ɼ� ����
    sPkgOID = (qsOID)sSLongOID;

    IDE_TEST( qsxPkg::makeStatusInvalidTx( aStatement,
                                           sPkgOID )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

IDE_RC qcmPkg::relSetInvalidPkgOfRelated(
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

    IDE_TEST( smiGetTableColumns( gQcmPkgRelated,
                                  QCM_PKG_RELATED_RELATEDUSERID_COL_ORDER,
                                  (const smiColumn**)&sFirstMtcColumn )
              != IDE_SUCCESS );

    IDE_TEST( smiGetTableColumns( gQcmPkgRelated,
                                  QCM_PKG_RELATED_RELATEDOBJECTNAME_COL_ORDER,
                                  (const smiColumn**)&sSecondMtcColumn )
              != IDE_SUCCESS );

    IDE_TEST( smiGetTableColumns( gQcmPkgRelated,
                                  QCM_PKG_RELATED_RELATEDOBJECTTYPE_COL_ORDER,
                                  (const smiColumn**)&sThirdMtcColumn )
              != IDE_SUCCESS );

    qcm::makeMetaRangeTripleColumn(
        & sFirstMetaRange,
        & sSecondMetaRange,
        & sThirdMetaRange,
        sFirstMtcColumn,
        & sUserIdData,
        sSecondMtcColumn,
        (const void*) sObjectName,
        sThirdMtcColumn,
        & sObjectTypeIntData,
        & sRange );

    IDE_TEST( qcm::selectRow(
            QC_SMI_STMT( aStatement ),
            gQcmPkgRelated,
            smiGetDefaultFilter(), /* a_callback */
            & sRange,
            gQcmPkgRelatedIndex
            [ QCM_PKG_RELATED_RELUSERID_RELOBJECTNAME_RELOBJECTTYPE ],
            (aIsUseTx == ID_FALSE)?(qcmSetStructMemberFunc ) qcmModifyStatusOfRelatedPkgToInvalid:
                                   (qcmSetStructMemberFunc ) qcmModifyStatusOfRelatedPkgToInvalidTx,
            aStatement, /* argument to qcmSetStructMemberFunc */
            0,          /* aMetaStructDistance.
                           0 means do not change pointer to aStatement */
            ID_UINT_MAX,/* no limit on selected row count */
            & sRowCount )
        != IDE_SUCCESS );

    // no row of qcmPkgRelated is updated
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    // no row of qcmPkgRelated is updated
    return IDE_FAILURE;
}

/* BUG-39340 */
IDE_RC qcmModifyStatusOfRelatedPkgBodyToInvalid (
    idvSQL              * /* aStatistics */,
    const void          * aRow,
    qcStatement         * aStatement )
{
    qsOID           sPkgOID;
    SLong           sSLongOID;
    mtcColumn     * sPkgOIDMtcColumn;

    IDE_TEST( smiGetTableColumns( gQcmPkgs,
                                  QCM_PKGS_PKGOID_COL_ORDER,
                                  (const smiColumn**)&sPkgOIDMtcColumn )
              != IDE_SUCCESS );

    qcm::getBigintFieldValue (
        aRow,
        sPkgOIDMtcColumn,
        & sSLongOID );

    // BUGBUG 32bit machine���� ���� �� SLong(64bit)������ uVLong(32bit)������
    // ��ȯ�ϹǷ� ������ �ս� ���ɼ� ����
    sPkgOID = (qsOID)sSLongOID;

    IDE_TEST( qsxPkg::makeStatusInvalid( aStatement,
                                         sPkgOID )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qcmPkg::relSetInvalidPkgBody(
    qcStatement    * aStatement,
    UInt             aPkgBodyUserID,
    SChar          * aPkgBodyObjectName,
    UInt             aPkgBodyObjectNameSize,
    qsObjectType     aPkgBodyObjectType)
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

    sUserIdData = (mtdIntegerType) aPkgBodyUserID;

    qtc::setVarcharValue( sObjectName,
                          NULL,
                          aPkgBodyObjectName,
                          aPkgBodyObjectNameSize );

    sObjectTypeIntData = (mtdIntegerType) aPkgBodyObjectType;

    IDE_TEST( smiGetTableColumns( gQcmPkgs,
                                  QCM_PKGS_PKGNAME_COL_ORDER,
                                  (const smiColumn**)&sFirstMtcColumn )
              != IDE_SUCCESS );

    IDE_TEST( smiGetTableColumns( gQcmPkgs,
                                  QCM_PKGS_PKGTYPE_COL_ORDER,
                                  (const smiColumn**)&sSecondMtcColumn )
              != IDE_SUCCESS );

    IDE_TEST( smiGetTableColumns( gQcmPkgs,
                                  QCM_PKGS_USERID_COL_ORDER,
                                  (const smiColumn**)&sThirdMtcColumn )
              != IDE_SUCCESS );

    qcm::makeMetaRangeTripleColumn(
        & sFirstMetaRange,
        & sSecondMetaRange,
        & sThirdMetaRange,
        sFirstMtcColumn,
        (const void*) sObjectName,
        sSecondMtcColumn,
        & sObjectTypeIntData,
        sThirdMtcColumn,
        & sUserIdData,
        & sRange );

    IDE_TEST( qcm::selectRow(
                  QC_SMI_STMT( aStatement ),
                  gQcmPkgs,
                  smiGetDefaultFilter(), /* a_callback */
                  & sRange,
                  gQcmPkgsIndex
                  [ QCM_PKGS_PKGNAME_PKGTYPE_USERID_IDX_ORDER ],
                  (qcmSetStructMemberFunc )
                  qcmModifyStatusOfRelatedPkgBodyToInvalid,
                  aStatement, /* argument to qcmSetStructMemberFunc */
                  0,          /* aMetaStructDistance.
                                 0 means do not change pointer to aStatement */
                  ID_UINT_MAX,/* no limit on selected row count */
                  & sRowCount )
              != IDE_SUCCESS );

    // no row of qcmPkgRelated is updated
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    // no row of qcmPkgRelated is updated
    return IDE_FAILURE;
}

IDE_RC qcmPkg::relRemoveAll(
    qcStatement     * aStatement,
    qsOID          aPkgOID)
{
/***********************************************************************
 *
 * Description :
 *    ���ν����� ���õ� ������Ʈ�� ���� ������ �����Ѵ�.
 *
 * Implementation :
 *    SYS_PACKAGE_RELATED_ ��Ÿ ���̺��� ����� PkgOID �� �ش��ϴ�
 *    �����͸� �����Ѵ�.
 *
 ***********************************************************************/

    SChar   * sBuffer;
    vSLong    sRowCnt;

    IDE_TEST(STRUCT_ALLOC_WITH_SIZE(aStatement->qmxMem,
                                    SChar,
                                    QD_MAX_SQL_LENGTH,
                                    &sBuffer)
             != IDE_SUCCESS);

    idlOS::snprintf( sBuffer, QD_MAX_SQL_LENGTH,
                     "DELETE FROM SYS_PACKAGE_RELATED_ "
                     "WHERE  PACKAGE_OID = "QCM_SQL_BIGINT_FMT,
                     QCM_OID_TO_BIGINT( aPkgOID ) );

    IDE_TEST( qcg::runDMLforDDL( QC_SMI_STMT( aStatement ),
                                 sBuffer,
                                 & sRowCnt )
              != IDE_SUCCESS );

    // [original code] no error is raised even if no row is deleted.

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}


IDE_RC qcmSetPkgUserIDOfQcmPkgs(
    idvSQL     * /* aStatistics */,
    const void * aRow,
    SInt       * aUserID )
{
/*******************************************************************
 * Description : To Fix BUG-19839
 *               Pkg oid �� ����� �������� UserID�� �˻�
 *
 * Implementation :
 ********************************************************************/

    mtcColumn * sUserIDMtcColumn;

    IDE_TEST( smiGetTableColumns( gQcmPkgs,
                                  QCM_PKGS_USERID_COL_ORDER,
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

IDE_RC qcmPkg::getPkgUserID ( qcStatement * aStatement,
                              qsOID         aPkgOID,
                              UInt        * aPkgUserID )
{
/*******************************************************************
 * Description : To Fix BUG-19839
 *               Pkg oid �� ����� �������� UserID�� �˻�
 *
 * Implementation :
 ********************************************************************/

    smiRange           sRange;
    qtcMetaRangeColumn sMetaRange;
    vSLong             sRowCount;
    SLong              sPkgOID;
    mtcColumn        * sPkgOIDMtcColumn;

    sPkgOID = QCM_OID_TO_BIGINT( aPkgOID );

    IDE_TEST( smiGetTableColumns( gQcmPkgs,
                                  QCM_PKGS_PKGOID_COL_ORDER,
                                  (const smiColumn**)&sPkgOIDMtcColumn )
              != IDE_SUCCESS );

    (void)qcm::makeMetaRangeSingleColumn( & sMetaRange,
                                          sPkgOIDMtcColumn,
                                          & sPkgOID,
                                          & sRange );

    IDE_TEST( qcm::selectRow( QC_SMI_STMT( aStatement ),
                              gQcmPkgs,
                              smiGetDefaultFilter(),
                              & sRange,
                              gQcmPkgsIndex
                              [ QCM_PKGS_PKGOID_IDX_ORDER ],
                              (qcmSetStructMemberFunc ) qcmSetPkgUserIDOfQcmPkgs,
                              aPkgUserID,
                              0,
                              1,
                              & sRowCount )
              != IDE_SUCCESS );

    IDE_TEST_RAISE( sRowCount != 1, err_selected_count_is_not_1 );

    return IDE_SUCCESS;

    IDE_EXCEPTION( err_selected_count_is_not_1 );
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QCM_INTERNAL_ARG,
                                "[qcmPkg::getPkgUserID]"
                                "err_selected_count_is_not_1" ));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

// BUG-36975
IDE_RC qcmPkg::makeRelatedObjectNodeForInsertMeta(
           qcStatement       * aStatement,
           UInt                aUserID,
           qsOID               aObjectID,
           qcNamePosition      aObjectName,
           SInt                aObjectType,
           qsRelatedObjects ** aRelObj )
{
    qsRelatedObjects * sRelObj = NULL;
    iduMemory        * sMemory = NULL;

    sMemory = QC_QMX_MEM( aStatement );
    
    IDE_TEST( STRUCT_ALLOC( sMemory,
                            qsRelatedObjects,
                            &sRelObj ) != IDE_SUCCESS );

    // user name setting
    IDE_TEST( STRUCT_ALLOC_WITH_SIZE( sMemory,
                                      SChar,
                                      QC_MAX_OBJECT_NAME_LEN + 1,
                                      &( sRelObj->userName.name ) )
              != IDE_SUCCESS )

    // connect user name
    IDE_TEST( qcmUser::getUserName(
                   aStatement,
                   aUserID,
                   sRelObj->userName.name )
              != IDE_SUCCESS);

    sRelObj->userName.size
        = idlOS::strlen(sRelObj->userName.name);
    sRelObj->userID = aUserID;

    // set object name
    sRelObj->objectName.size = aObjectName.size;

    IDE_TEST( STRUCT_ALLOC_WITH_SIZE( sMemory,
                                      SChar,
                                      (sRelObj->objectName.size+1),
                                      &(sRelObj->objectName.name ) )
              != IDE_SUCCESS);

    QC_STR_COPY( sRelObj->objectName.name, aObjectName );

    // objectNamePos
    SET_POSITION( sRelObj->objectNamePos, aObjectName );

    sRelObj->userID = aUserID;
    sRelObj->objectID = aObjectID;
    sRelObj->objectType = aObjectType;
    sRelObj->tableID = 0;

    *aRelObj = sRelObj;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/*************************************************************
  SetMember functions
  for more members, see qcmPkg.cpp rev.1.11
 **************************************************************/

IDE_RC qcmPkg::pkgSetMember(
    idvSQL        * /* aStatistics */,
    const void    * aRow,
    qcmPkgs       * aPkgs )
{
    SLong       sSLongOID;
    mtcColumn * sUserIDMtcColumn;
    mtcColumn * sPkgOIDMtcColumn;
    mtcColumn * sPkgNameMtcColumn;
    mtcColumn * sPkgTypeMtcColumn;

    IDE_TEST( smiGetTableColumns( gQcmPkgs,
                                  QCM_PKGS_USERID_COL_ORDER,
                                  (const smiColumn**)&sUserIDMtcColumn )
              != IDE_SUCCESS );

    qcm::getIntegerFieldValue(
        aRow,
        sUserIDMtcColumn,
        & aPkgs->userID);

    IDE_TEST( smiGetTableColumns( gQcmPkgs,
                                  QCM_PKGS_PKGOID_COL_ORDER,
                                  (const smiColumn**)&sPkgOIDMtcColumn )
              != IDE_SUCCESS );

    qcm::getBigintFieldValue (
        aRow,
        sPkgOIDMtcColumn,
        & sSLongOID);
    // BUGBUG 32bit machine���� ���� �� SLong(64bit)������ uVLong(32bit)������
    // ��ȯ�ϹǷ� ������ �ս� ���ɼ� ����
    aPkgs->pkgOID = (qsOID)sSLongOID;

    IDE_TEST( smiGetTableColumns( gQcmPkgs,
                                  QCM_PKGS_PKGNAME_COL_ORDER,
                                  (const smiColumn**)&sPkgNameMtcColumn )
              != IDE_SUCCESS );

    qcm::getCharFieldValue (
        aRow,
        sPkgNameMtcColumn,
        aPkgs->pkgName);

    IDE_TEST( smiGetTableColumns( gQcmPkgs,
                                  QCM_PKGS_PKGTYPE_COL_ORDER,
                                  (const smiColumn**)&sPkgTypeMtcColumn )
              != IDE_SUCCESS );

    qcm::getIntegerFieldValue(
        aRow,
        sPkgTypeMtcColumn,
        & aPkgs->pkgType);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/* ------------------------------------------------
 *  X$PkgTEXT
 * ----------------------------------------------*/

#define QCM_PKG_TEXT_LEN  64

typedef struct qcmPkgText
{
    ULong  mPkgOID;
    UInt   mPiece;
    SChar *mText;

}qcmPkgText;


static iduFixedTableColDesc gPkgTEXTColDesc[] =
{
    {
        (SChar *)"PACKAGE_OID",
        offsetof(qcmPkgText, mPkgOID),
        IDU_FT_SIZEOF(qcmPkgText, mPkgOID),
        IDU_FT_TYPE_UBIGINT,
        NULL,
        0, 0,NULL // for internal use
    },

    {
        (SChar *)"PIECE",
        offsetof(qcmPkgText, mPiece),
        IDU_FT_SIZEOF(qcmPkgText, mPiece),
        IDU_FT_TYPE_UINTEGER,
        NULL,
        0, 0,NULL // for internal use
    },

    {
        (SChar *)"TEXT",
        offsetof(qcmPkgText, mText),
        QCM_PKG_TEXT_LEN,
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


IDE_RC qcmPkg::pkgSelectAllForFixedTable (
    smiStatement         * aSmiStmt,
    vSLong                 aMaxPkgCount,
    vSLong               * aSelectedPkgCount,
    void                 * aFixedTableInfo )
{
    vSLong selectedRowCount;

    IDE_TEST( qcm::selectRow
              (
                  aSmiStmt,
                  gQcmPkgs,
                  smiGetDefaultFilter(), /* a_callback */
                  smiGetDefaultKeyRange(), /* a_range */
                  NULL, /* a_index */
                  (qcmSetStructMemberFunc ) qcmPkg::buildPkgText,
                  aFixedTableInfo,
                  0, /* distance */
                  aMaxPkgCount,
                  &selectedRowCount
              ) != IDE_SUCCESS );

    *aSelectedPkgCount = (vSLong) selectedRowCount;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


IDE_RC qcmPkg::buildPkgText( idvSQL        * aStatistics,
                             const void    * aRow,
                             void          * aFixedTableInfo )
{
    UInt            sState = 0;
    qsOID           sOID;
    SLong           sSLongOID;
    SInt            sPkgLen = 0;
    qcmPkgText      sPkgText;
    SChar         * sPkgBuf = NULL;
    qcStatement     sStatement;
    mtcColumn     * sPkgOIDMtcColumn;
    // PROJ-2446
    smiTrans       sMetaTx;
    smiStatement * sDummySmiStmt;
    smiStatement   sSmiStmt;
    UInt           sPkgState = 0;
    SChar        * sIndex;
    SChar        * sStartIndex;
    SChar        * sPrevIndex;
    SInt           sCurrPos = 0;
    SInt           sCurrLen = 0;
    SInt           sSeqNo   = 0;
    SChar          sParseStr [ QCM_PKG_TEXT_LEN * 2 + 2 ] = { 0, };

    const mtlModule* sModule;

    IDE_TEST(sMetaTx.initialize() != IDE_SUCCESS);
    sPkgState = 1;

    IDE_TEST(sMetaTx.begin(&sDummySmiStmt,
                           aStatistics,
                           (SMI_TRANSACTION_UNTOUCHABLE |
                            SMI_ISOLATION_CONSISTENT    |
                            SMI_COMMIT_WRITE_NOWAIT))
             != IDE_SUCCESS);
    sPkgState = 2;

    IDE_TEST(sSmiStmt.begin( aStatistics,
                             sDummySmiStmt,
                             SMI_STATEMENT_UNTOUCHABLE |
                             SMI_STATEMENT_MEMORY_CURSOR)
             != IDE_SUCCESS);
    sPkgState = 3;

    /* ------------------------------------------------
     *  Get Pkg OID
     * ----------------------------------------------*/

    IDE_TEST( smiGetTableColumns( gQcmPkgs,
                                  QCM_PKGS_PKGOID_COL_ORDER,
                                  (const smiColumn**)&sPkgOIDMtcColumn )
              != IDE_SUCCESS );

    qcm::getBigintFieldValue (
        aRow,
        sPkgOIDMtcColumn,
        & sSLongOID);
    // BUGBUG 32bit machine���� ���� �� SLong(64bit)������ uVLong(32bit)������
    // ��ȯ�ϹǷ� ������ �ս� ���ɼ� ����
    sOID = (qsOID)sSLongOID;

    /* ------------------------------------------------
     *  Generate Pkg Text
     * ----------------------------------------------*/

    if (qsxPkg::latchS( sOID ) == IDE_SUCCESS)
    {
        sState = 1;

        IDE_TEST( qcg::allocStatement( & sStatement,
                                       NULL,
                                       NULL,
                                       NULL )
                  != IDE_SUCCESS);

        sState = 2;

        sPkgBuf = NULL; // allocate inside
        IDE_TEST(qsxPkg::getPkgText(&sStatement,
                                    sOID,
                                    &sPkgBuf,
                                    &sPkgLen)
                 != IDE_SUCCESS);

        sPkgText.mPkgOID = sOID;

        // BUG-44978
        sModule     = mtl::mDBCharSet;
        sStartIndex = sPkgBuf;
        sIndex      = sStartIndex;

        while(1)
        {
            sPrevIndex = sIndex;

            (void)sModule->nextCharPtr( (UChar**) &sIndex,
                                        (UChar*) ( sPkgBuf + sPkgLen ));

            if (( sPkgBuf + sPkgLen ) <= sIndex )
            {
                // ������ �� ���.
                // ����� �� �� break.
                sSeqNo++;

                sCurrPos = sStartIndex - sPkgBuf;
                sCurrLen = sIndex - sStartIndex;

                qcg::prsCopyStrDupAppo( sParseStr,
                                        sPkgBuf + sCurrPos,
                                        sCurrLen );
                
                sPkgText.mPiece = sSeqNo;
                sPkgText.mText  = sParseStr;

                IDE_TEST(iduFixedTable::buildRecord(((qcmTempFixedTableInfo *)aFixedTableInfo)->mHeader,
                                                    ((qcmTempFixedTableInfo *)aFixedTableInfo)->mMemory,
                                                    (void *) &sPkgText)
                         != IDE_SUCCESS);
                break;
            }
            else
            {
                if( sIndex - sStartIndex >= QCM_PKG_TEXT_LEN )
                {
                    // ���� ������ �� ����, �дٺ��� 64����Ʈ �Ǵ� �ʰ��� ����
                    // �Ǿ��� �� �߶� ���
                    sCurrPos = sStartIndex - sPkgBuf;
                
                    if( sIndex - sStartIndex == QCM_PKG_TEXT_LEN )
                    {
                        // �� �������� ���
                        sCurrLen = QCM_PKG_TEXT_LEN;
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
                                            sPkgBuf + sCurrPos,
                                            sCurrLen );

                    sPkgText.mPiece  = sSeqNo;
                    sPkgText.mText   = sParseStr;

                    IDE_TEST(iduFixedTable::buildRecord(((qcmTempFixedTableInfo *)aFixedTableInfo)->mHeader,
                                                        ((qcmTempFixedTableInfo *)aFixedTableInfo)->mMemory,
                                                        (void *) &sPkgText)
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

        IDE_TEST(qsxPkg::unlatch( sOID )
                 != IDE_SUCCESS);
    }
    else
    {
#if defined(DEBUG)
        ideLog::log(IDE_QP_0, "Latch Error \n");
#endif
    }

    sPkgState = 2;
    IDE_TEST(sSmiStmt.end(SMI_STATEMENT_RESULT_SUCCESS) != IDE_SUCCESS);

    sPkgState = 1;
    IDE_TEST(sMetaTx.commit() != IDE_SUCCESS);

    sPkgState = 0;
    IDE_TEST(sMetaTx.destroy( aStatistics ) != IDE_SUCCESS);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    switch(sState)
    {
        case 2:
            IDE_ASSERT(qcg::freeStatement(&sStatement) == IDE_SUCCESS);
        case 1:
            IDE_ASSERT(qsxPkg::unlatch( sOID )
                       == IDE_SUCCESS);
        case 0:
            //nothing
            break;
        default:
            IDE_CALLBACK_FATAL("Can't be here");
    }

    switch(sPkgState)
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

IDE_RC qcmPkg::buildRecordForPkgTEXT(idvSQL              * aStatistics,
                                     void                * aHeader,
                                     void                * /* aDumpObj */,
                                     iduFixedTableMemory * aMemory)
{
    smiTrans               sMetaTx;
    smiStatement          *sDummySmiStmt;
    smiStatement           sStatement;
    vSLong                 sRecCount;
    qcmTempFixedTableInfo  sInfo;
    UInt                   sState = 0;

    IDU_FIT_POINT_FATAL( "qcmPkg::buildRecordForPkgTEXT::__NOFT__" );

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


    IDE_TEST(pkgSelectAllForFixedTable (&sStatement,
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


iduFixedTableDesc gPkgTEXTTableDesc =
{
    (SChar *)"X$PKGTEXT",
    qcmPkg::buildRecordForPkgTEXT,
    gPkgTEXTColDesc,
    IDU_STARTUP_META,
    0,
    0,
    IDU_FT_DESC_TRANS_NOT_USE,
    NULL
};
