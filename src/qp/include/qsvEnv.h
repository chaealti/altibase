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
 * $Id: qsvEnv.h 87967 2020-07-07 05:02:47Z khkwak $
 **********************************************************************/

#ifndef _O_QSV_ENV_H_
#define _O_QSV_ENV_H_ 1

#include <qc.h>
#include <qtcDef.h>
#include <qsParseTree.h>
#include<qsxExecutor.h>

// qsvEnvInfo.flag
#define QSV_ENV_RETURN_STMT_MASK              (0x00000001)
#define QSV_ENV_RETURN_STMT_ABSENT            (0x00000000)
#define QSV_ENV_RETURN_STMT_EXIST             (0x00000001)

// qsvEnvInfo.flag
#define QSV_ENV_ESTIMATE_ARGU_MASK            (0x00000002)
#define QSV_ENV_ESTIMATE_ARGU_FALSE           (0x00000000)
#define QSV_ENV_ESTIMATE_ARGU_TRUE            (0x00000002)

// qsvEnvInfo.flag
#define QSV_ENV_SUBQUERY_ON_ARGU_ALLOW_MASK   (0x00000004)
#define QSV_ENV_SUBQUERY_ON_ARGU_ALLOW_TRUE   (0x00000000)
#define QSV_ENV_SUBQUERY_ON_ARGU_ALLOW_FALSE  (0x00000004)

// qsvEnvInfo.flag
// To Fix PR-10735
// TRIGGER�� ���� Procedure������ �Ǵ���.
#define QSV_ENV_TRIGGER_MASK                  (0x00000008)
#define QSV_ENV_TRIGGER_FALSE                 (0x00000000)
#define QSV_ENV_TRIGGER_TRUE                  (0x00000008)

// qsvEnvInfo.flag
// BUG-47971 Package global variable�� �л� ������ �����մϴ�.
// Package global variable ���� ����
#define QSV_ENV_PKG_VAR_EXIST_MASK            (0x00000010)
#define QSV_ENV_PKG_VAR_EXIST_FALSE           (0x00000000)
#define QSV_ENV_PKG_VAR_EXIST_TRUE            (0x00000010)

#define QSV_ENV_SET_SQL( a, b ) { \
          a->spvEnv->sql = a->myPlan->stmtText; \
          a->spvEnv->sqlSize = a->myPlan->stmtTextLen; } 


#define QSV_ENV_INIT_SQL( a ) { \
          a->spvEnv->sql = NULL; \
          a->spvEnv->sqlSize = 0; }


typedef struct qsvEnvInfo
{
    struct qsAllVariables     * allVariables;

    SInt                        loopCount;      // for CONTINUE
    SInt                        exceptionCount; // for RAISE
    SInt                        nextId;         // for label or exception ID
    UInt                        flag;

    struct qsProcStmts        * currStmt;       // current validating statement
    struct qsRelatedObjects   * relatedObjects;
    struct qsProcParseTree    * createProc;
    struct qsSynonymList      * objectSynonymList;
    struct qsxProcPlanList    * procPlanList;

    SChar                     * sql;
    SInt                        sqlSize;

    // PROJ-1535
    // procPlanList�� latch���¸� ������
    idBool                      latched;

    // PROJ-1359 Trigger
    // Trigger�� Cycle Detection�� ���� ������
    struct qsModifiedTable    * modifiedTableList;

    // To fix BUG-14129
    struct qsVariableItems    * currDeclItem;

    // fix BUG-32837
    struct qsVariableItems    * allParaDecls;

    // BUG-36203 PSM Optimize
    qsxStmtList               * mStmtList;
    qsxStmtList2              * mStmtList2;

    // PROJ-1073 Package
    qsPkgParseTree            * createPkg;
    // create package body �� �ش� package body�� ����
    // package spec�� parse tree�� ����
    qsPkgParseTree            * pkgPlanTree;
    qsPkgStmts                * currSubprogram;

    /* BUG-39004
       package intialize section�� ���ؼ�
       validation ���� ���� ǥ�� */
    idBool                      isPkgInitializeSection;
} qsvEnvInfo;

class qsvEnv
{
public:
    static IDE_RC allocEnv( qcStatement * aStatement );

    static void clearEnv(qsvEnvInfo * aEnv);

    static SInt getNextId (qsvEnvInfo * aEnvInfo);
};

#endif /* _O_QSV_ENV_H_ */
