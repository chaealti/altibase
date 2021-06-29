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
 * $Id: qsvPkgStmts.h 82075 2018-01-17 06:39:52Z jina.kim $
 **********************************************************************/

#ifndef _Q_QSV_PKG_STMTS_H_
#define _Q_QSV_PKG_STMTS_H_ 1

#include <qc.h>
#include <qsParseTree.h>
#include <qcmSynonym.h>

// BUG-41262 PSM overloading
#define QSV_TYPE_GROUP_PENALTY      (0x10000)

class qsvPkgStmts
{
public:
    static IDE_RC parseSpecBlock( qcStatement * aStatement,
                                  qsProcStmts * aProcStmts );

    static IDE_RC parseBodyBlock( qcStatement * aStatement,
                                  qsProcStmts * aProcStmts );

    static IDE_RC validateSpecBlock( qcStatement       * aStatement,
                                     qsProcStmts       * aProcStmts,
                                     qsProcStmts       * aParentStmt,
                                     qsValidatePurpose   aPurpose );


    static IDE_RC validateBodyBlock( qcStatement       * aStatement,
                                     qsProcStmts       * aProcStmts,
                                     qsProcStmts       * aParentStmt,
                                     qsValidatePurpose   aPurpose );

    static IDE_RC parseCreateProc( qcStatement * aStatement,
                                   qsPkgStmts  * aPkgStmt );

    static IDE_RC validateCreateProcHeader( qcStatement       * aStatement,
                                            qsPkgStmts        * aPkgStmt );

    static IDE_RC validateCreateProcBody( qcStatement       * aStatement,
                                          qsPkgStmts        * aPkgStmt,
                                          qsValidatePurpose   aPurpose );

    static IDE_RC validateCreateFuncHeader( qcStatement       * aStatement,
                                            qsPkgStmts        * aPkgStmt );

    static IDE_RC getPkgSubprogramID( qcStatement    * aStatement,
                                      qtcNode        * aNode,
                                      qcNamePosition   aUserName,
                                      qcNamePosition   aTableName,
                                      qcNamePosition   aColumnName,
                                      qsxPkgInfo     * aPkgInfo,
                                      UInt           * aSubprogramID );

    static IDE_RC getMyPkgSubprogramID( qcStatement         * aStatement,
                                        qtcNode             * aNode,
                                        qsPkgSubprogramType * aSubprogramType,
                                        UInt                * aSubprogramID );

    static IDE_RC makeRelatedObjects( qcStatement      * aStatement,
                                      qcNamePosition   * aUserName,
                                      qcNamePosition   * aTableName,
                                      qcmSynonymInfo   * aSynonymInfo,
                                      UInt               aTableID,
                                      SInt               aObjectType );

    static IDE_RC makeNewRelatedObject( qcStatement       * aStatement,
                                        qcNamePosition    * aUserName,
                                        qcNamePosition    * aTableName,
                                        qcmSynonymInfo    * aSynonymInfo,
                                        UInt                aTableID,
                                        SInt                aObjectType,
                                        qsRelatedObjects ** aObject );

    static IDE_RC makePkgSynonymList( qcStatement           * aStatement,
                                      struct qcmSynonymInfo * aSynonymInfo,
                                      qcNamePosition          aUserName,
                                      qcNamePosition          aTableName,
                                      smOID                   aPkgID );

    static IDE_RC getExceptionFromPkg( qcStatement   * aStatement,
                                       qsExceptions  * aException,
                                       mtcColumn    ** aColumn,
                                       idBool        * aFind );

    // BUG-37333
    static IDE_RC makeUsingSubprogramList( qcStatement         * aStatement,
                                           qtcNode             * aNode );

    // BUG-37655
    static IDE_RC checkPragmaRestrictReferencesOption(
                       qcStatement * aStatement,
                       qsPkgStmts  * aPkgStmt,
                       qsProcStmts * aProcStmt,
                       idBool        aIsPkgInitSection /* BUG-39003 */ );

    static IDE_RC getPkgSubprogramPlanTree(
                       qcStatement          * aStatement,
                       qsPkgParseTree       * aPkgPlanTree,
                       qsObjectType           aPkgType,
                       UInt                   aSubprogramID,
                       qsProcParseTree     ** aSubprogramPlanTree );

    /* BUG-41847
       package�� local exception variable�� ã�´�. */
    static IDE_RC getPkgLocalException( qcStatement  * aStatement,
                                        qsExceptions * aException,
                                        idBool       * aFind );

    /* BUG-38844 , BUG-41847
       package spec�� spvEnv->procPlanList�� �޾Ƴ��´�. */
    static IDE_RC makeRelatedPkgSpecToPkgBody( qcStatement * aStatement,
                                               qsxPkgInfo  * aPkgSpecInfo );

    /* BUG-41847
       package spec�� related object�� package body�� related object�� ��� */
    static IDE_RC inheritRelatedObjectFromPkgSpec( qcStatement * aStatement,
                                                   qsxPkgInfo * aPkgSpecInfo );

    /* BUG-41847
       subprogram�� ���ȣ���ϴ��� check�ϴ� �Լ� */
    static idBool checkIsRecursiveSubprogramCall( qsPkgStmts * aCurrPkgStmt,
                                                  UInt         aTargetSubprogramID );
private:

    static IDE_RC checkDupSubprograms( qcStatement * aStatement,
                                       qsPkgStmts  * aCurrSub,
                                       qsPkgStmts  * sNextSub );

    static IDE_RC checkSubprogramDef( qcStatement * aStatement,
                                      qsPkgStmts  * aBodySub,
                                      qsPkgStmts  * aSpecSub );

    static IDE_RC checkSubprogramsInBody( qcStatement * aStatement,
                                          qsPkgStmts  * aCurrSub );

    // BUG-37655
    static IDE_RC validatePragmaRestrictReferences( 
                      qcStatement    * aStatement,
                      qsPkgStmtBlock * aBLOCK,
                      qsPkgStmts     * aPkgStmt );

    /* BUG-39220
       subprogram �̸��� parameter ������ ���� �� �����Ƿ�,
       parameter �� return ���� ���� �񱳰� �ʿ��ϴ�. */
    static IDE_RC compareParametersAndReturnVar(
                      qcStatement      * aStatement,
                      idBool             aIsPrivate,
                      idBool             aCheckParameters,
                      qsPkgSubprograms * aCurrSubprogram );

    // BUG-41262 PSM overloading
    // create �Ҷ� ������ �������α׷����� ���θ� ��ȯ�Ѵ�.
    static idBool isSubprogramEqual( qsPkgSubprograms * aCurrent,
                                     qsPkgSubprograms * aNext );

    // exec �Ҷ� �˸´� �������α׷��� ã�´�.
    static IDE_RC matchingSubprogram( qcStatement           * aStatement,
                                      qsPkgParseTree        * aParseTree,
                                      qtcNode               * aNode,
                                      UInt                    aArgCount,
                                      qcNamePosition          aColumnName,
                                      idBool                  aExactly,
                                      idBool                * aExist,
                                      qsPkgSubprogramType   * aSubprogramType,
                                      UInt                  * aSubprogramID );

    // matchingSubprogram ���ο��� ����ϴ� �Լ�
    // �������� ����Ͽ� ���� Ÿ���� ���������� �˷��ش�.
    static IDE_RC calTypeFriendly( qcStatement  * aStatement,
                                   mtcNode      * aArgNode,
                                   mtcColumn    * aArgColumn,
                                   mtcColumn    * aParamColumn,
                                   UInt         * aFriendly );
};
#endif  // _Q_QSV_PKG_STMTS_H_
