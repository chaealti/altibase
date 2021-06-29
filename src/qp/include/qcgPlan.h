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
 * $Id: qcgPlan.h 82691 2018-04-03 06:25:10Z andrew.shin $
 *
 * Description :
 *     PROJ-1436 SQL Plan Cache�� ���� �ڷ� ���� ����
 *
 * ��� ���� :
 *
 * ��� :
 *
 **********************************************************************/

#ifndef _O_QCG_PLAN_H_
#define _O_QCG_PLAN_H_ 1

#include <qci.h>
#include <qc.h>
#include <qs.h>

//////////////////////////////////////////////////////
// BUG-38148 ������Ƽ �߰��۾��� �ܼ�ȭ ���Ѿ� �մϴ�.
//////////////////////////////////////////////////////
// qcgPlan::isMatchedPlanProperty only
#define QCG_MATCHED_PLAN_PROPERTY( aRef, aValue, aSetValue )            \
{                                                                       \
    ++sMatchedCount;                                                    \
    if ( (aPlanProperty->aRef) == ID_TRUE )                             \
    {                                                                   \
        if ( (sProperty->aRef) == ID_FALSE )                            \
        {                                                               \
            (sProperty->aRef)   = ID_TRUE;                              \
            (sProperty->aValue) = (aSetValue);                          \
        }                                                               \
        else                                                            \
        {                                                               \
        }                                                               \
                                                                        \
        if ( (aPlanProperty->aValue) != (sProperty->aValue) )           \
        {                                                               \
            sIsMatched = ID_FALSE;                                      \
            IDE_CONT( NOT_MATCHED );                                    \
        }                                                               \
        else                                                            \
        {                                                               \
        }                                                               \
    }                                                                   \
    else                                                                \
    {                                                                   \
    }                                                                   \
}
// qcgPlan::rebuildPlanProperty only
#define QCG_REBUILD_PLAN_PROPERTY( aRef, aValue, aSetValue )            \
{                                                                       \
    ++sRebuildCount;                                                    \
    if ( (aPlanProperty->aRef) == ID_TRUE )                             \
    {                                                                   \
        (aPlanProperty->aValue) = (aSetValue);                          \
    }                                                                   \
    else                                                                \
    {                                                                   \
    }                                                                   \
}
// qcgPlan::registerPlanProperty only
#define QCG_REGISTER_PLAN_PROPERTY( aRef, aValue, aSetValue )           \
{                                                                       \
    if ( (sProperty->aRef) == ID_FALSE )                                \
    {                                                                   \
        (sProperty->aRef)   = ID_TRUE;                                  \
        (sProperty->aValue) = (aSetValue);                              \
    }                                                                   \
    else                                                                \
    {                                                                   \
        IDE_DASSERT( (sProperty->aValue) == (aSetValue) );              \
    }                                                                   \
}

//---------------------------------------------------
// [qcgPlanObject]
// plan ������ ������ ��� database ��ü
//---------------------------------------------------

// table, view, queue, dblink
typedef struct qcgEnvTableList
{
    void            * tableHandle;
    smSCN             tableSCN;

    /* BUG-45893 */
    idBool            mIsDBAUser;

    qcgEnvTableList * next;
} qcgEnvTableList;

typedef struct qcgEnvSequenceList
{
    void               * sequenceHandle;
    smSCN                sequenceSCN; /* Slot ��Ȱ�� ���θ� Ȯ���ϱ� ���� SCN */
    qcgEnvSequenceList * next;
} qcgEnvSequenceList;

// procedure, function, typeset
typedef struct qcgEnvProcList
{
    void            * procHandle;
    smSCN             procSCN; /* Slot ��Ȱ�� ���θ� Ȯ���ϱ� ���� SCN */
    qsOID             procID;
    qsProcParseTree * planTree;
    UInt              modifyCount;
    qcgEnvProcList  * next;
} qcgEnvProcList;

typedef struct qcgEnvSynonymList
{
    // synonym ����
    SChar         userName[QC_MAX_OBJECT_NAME_LEN + 1];
    SChar         objectName[QC_MAX_OBJECT_NAME_LEN + 1];
    idBool        isPublicSynonym;

    // synonym�� ����Ű�� ���� ��ü�� ����
    // (table, view, queue, dblink, sequence, proc �� �ϳ���)
    void        * tableHandle;
    void        * sequenceHandle;
    void        * objectHandle;
    qsOID         objectID;
    smSCN         objectSCN; /* Slot ��Ȱ�� ���θ� Ȯ���ϱ� ���� SCN */
    
    qcgEnvSynonymList  * next;
    
} qcgEnvSynonymList;

typedef struct qcgEnvRemoteTableColumnList
{
    SChar  columnName[QC_MAX_OBJECT_NAME_LEN + 1];
    UInt   dataTypeId;
    SInt   precision;
    SInt   scale;

    qcgEnvRemoteTableColumnList * next;
    
} qcgEnvRemoteTableColumnList;

// PROJ-1073 Package
typedef struct qcgEnvPkgList
{
    void           * pkgHandle;
    smSCN            pkgSCN; /* Slot ��Ȱ�� ���θ� Ȯ���ϱ� ���� SCN */
    qsOID            pkgID;
    qsPkgParseTree * planTree;           // BUG-37250
    UInt             modifyCount;
    qcgEnvPkgList  * next;
} qcgEnvPkgList;

typedef struct qcgPlanObject
{
    qcgEnvTableList       * tableList;
    qcgEnvSequenceList    * sequenceList;
    qcgEnvProcList        * procList;
    qcgEnvSynonymList     * synonymList;
    qcgEnvPkgList         * pkgList;             // PROJ-1073 Package
} qcgPlanObject;

//---------------------------------------------------
// [qcgPlanPrivilege]
// plan ������ ������ ��ü�鿡 ���� �ʿ��� ����
//---------------------------------------------------

typedef struct qcgEnvTablePrivList
{
    // ��� ��ü
    void                 * tableHandle;
    UInt                   tableOwnerID;
    SChar                  tableName[ QC_MAX_OBJECT_NAME_LEN + 1 ];
    UInt                   privilegeCount;
    struct qcmPrivilege  * privilegeInfo;  // cached meta�� ���纻
    
    // �ʿ��� ���� (reference, insert, update, delete)
    UInt                   privilegeID;

    qcgEnvTablePrivList  * next;
} qcgEnvTablePrivList;

typedef struct qcgEnvSequencePrivList
{
    // ��� ��ü
    UInt   sequenceOwnerID;
    UInt   sequenceID;  // meta table ID
    SChar  name[ QC_MAX_OBJECT_NAME_LEN + 1 ];
    // �ʿ��� ���� (�������� ���� ������ ���� select ���̴�.)
    //UInt  privilegeID; 
    
    qcgEnvSequencePrivList * next;
} qcgEnvSequencePrivList;

typedef struct qcgEnvProcPrivList
{
    // ��� ��ü
    qsOID       procID;
    
    // �ʿ��� ���� (proc�� ���� ������ ���� execute ���̴�.)
    //UInt        privilegeID;
    
    qcgEnvProcPrivList * next;
} qcgEnvProcPrivList;

typedef struct qcgPlanPrivilege
{
    qcgEnvTablePrivList    * tableList;
    qcgEnvSequencePrivList * sequenceList;
    
    // procPrivList�� procList�� �����ϰ� �߰��� ����� ������
    // �����Ƿ� ������� �ʴ´�.
    //qcgEnvProcPrivList     * procList;
} qcgPlanPrivilege;

//---------------------------------------------------
// [qcgPlanEnv]
// plan ������ ������ ��� ��� (Plan Environment)
//---------------------------------------------------

typedef struct qcgPlanEnv
{
    // plan ������ ������ ��� property
    qcPlanProperty         planProperty;

    // plan ������ ������ ��� ��ü
    qcgPlanObject          planObject;

    // plan ������ ������ ��� ����
    qcgPlanPrivilege       planPrivilege;

    // plan�� ������ user
    UInt                   planUserID;

    // PROJ-2163 : plan ������ ������ ���ε� ����
    qcPlanBindInfo         planBindInfo;

    // PROJ-2223 audit
    qciAuditInfo           auditInfo;
    
} qcgPlanEnv;


class qcgPlan
{
public:

    //-------------------------------------------------
    // plan environment ���� �Լ�
    //-------------------------------------------------

    // plan environment�� �����ϰ� �ʱ�ȭ�Ѵ�.
    static IDE_RC allocAndInitPlanEnv( qcStatement * aStatement );

    // plan property�� �ʱ�ȭ�Ѵ�.
    static void   initPlanProperty( qcPlanProperty * aProperty );

    // ���� ���� ������ ����Ѵ�.
    static void   startIndirectRefFlag( qcStatement * aStatement,
                                        idBool      * aIndirectRef );

    // ���� ���� ���Ḧ ����Ѵ�.
    static void   endIndirectRefFlag( qcStatement * aStatement,
                                      idBool      * aIndirectRef );

    // plan�� property�� �������� ����Ѵ�.
    static void   registerPlanProperty( qcStatement        * aStatement,
                                        qcPlanPropertyKind   aPropertyID );

    // plan�� table ��ü�� �������� ����Ѵ�.
    static IDE_RC registerPlanTable( qcStatement  * aStatement,
                                     void         * aTableHandle,
                                     smSCN          aTableSCN,
                                     UInt           aTableOwnerID, /* BUG-45893 */
                                     SChar        * aTableName );  /* BUG-45893 */

    // plan�� sequence ��ü�� �������� ����Ѵ�.
    static IDE_RC registerPlanSequence( qcStatement * aStatement,
                                        void        * aSequenceHandle );

    // plan�� proc ��ü �������� ����Ѵ�.
    static IDE_RC registerPlanProc( qcStatement     * aStatement,
                                    qsxProcPlanList * aProcPlanList );

    // plan�� table�� sequence ��ü�� synonym ��ü�� �������� ����Ѵ�.
    static IDE_RC registerPlanSynonym( qcStatement           * aStatement,
                                       struct qcmSynonymInfo * aSynonymInfo,
                                       qcNamePosition          aUserName,
                                       qcNamePosition          aObjectName,
                                       void                  * aTableHandle,
                                       void                  * aSequenceHandle );

    // plan�� proc ��ü�� synonymn ��ü�� �������� ����Ѵ�.
    static IDE_RC registerPlanProcSynonym( qcStatement          * aStatement,
                                           struct qsSynonymList * aObjectSynonymList );

    // plan�� table�� ���� �ʿ��� ������ ����Ѵ�.
    static IDE_RC registerPlanPrivTable( qcStatement         * aStatement,
                                         UInt                  aPrivilegeID,
                                         struct qcmTableInfo * aTableInfo );

    // plan�� sequence ��ü�� ���� �ʿ��� ������ ����Ѵ�.
    static IDE_RC registerPlanPrivSequence( qcStatement            * aStatement,
                                            struct qcmSequenceInfo * aSequenceInfo );

    // Plan �� �����ϴ� ���ε� ������ ����Ѵ�.
    static void registerPlanBindInfo( qcStatement  * aStatement );

    // PROJ-1073 Package
    // plan�� pkg ��ü �������� ����Ѵ�.
    static IDE_RC registerPlanPkg( qcStatement     * aStatement,
                                   qsxProcPlanList * aPlanList );

    //-------------------------------------------------
    // plan validation �Լ�
    //-------------------------------------------------

    // soft-prepare �������� �ùٸ� plan�� �˻��ϱ� ���� ����Ѵ�.
    static IDE_RC isMatchedPlanProperty( qcStatement    * aStatement,
                                         qcPlanProperty * aPlanProperty,
                                         idBool         * aIsMatched );

    // PROJ-2163 : soft-prepare �������� �ùٸ� plan�� �˻��ϱ� ���� ����Ѵ�.
    static IDE_RC isMatchedPlanBindInfo( qcStatement    * aStatement,
                                         qcPlanBindInfo * aBindInfo,
                                         idBool         * aIsMatched );

    // rebuild�� �ݿ��Ǵ� property environment�� �����Ѵ�.
    static IDE_RC rebuildPlanProperty( qcStatement    * aStatement,
                                       qcPlanProperty * aPlanProperty );

    // plan �������� table ��ü�� ���� validation�� �����Ѵ�.
    static IDE_RC validatePlanTable( qcgEnvTableList * aTableList,
                                     UInt              aUserID,    /* BUG-45893 */
                                     idBool          * aIsValid );

    // plan �������� sequence ��ü�� ���� validation�� �����Ѵ�.
    static IDE_RC validatePlanSequence( qcgEnvSequenceList * aSequenceList,
                                        idBool             * aIsValid );

    // plan �������� proc ��ü�� ���� validation�� �����Ѵ�.
    // proc�� �˻��ϸ鼭 latch�� ȹ���Ѵ�.
    static IDE_RC validatePlanProc( qcgEnvProcList * aProcList,
                                    idBool         * aIsValid );

    // plan �������� synonym ��ü�� ���� validation�� �����Ѵ�.
    static IDE_RC validatePlanSynonym( qcStatement       * aStatement,
                                       qcgEnvSynonymList * aSynonymList,
                                       idBool            * aIsValid );

    // PROJ-1073 Package
    // plan �������� package ��ü�� ���� validation�� �����Ѵ�.
    // package�� �˻��ϸ鼭 latch�� ȹ���Ѵ�.
    static IDE_RC validatePlanPkg( qcgEnvPkgList  * aPkgList,
                                   idBool         * aIsValid );

    //-------------------------------------------------
    // privilege �˻� �Լ�
    //-------------------------------------------------

    // plan �������� table ��ü�� ���� �ʿ��� ������ �˻��Ѵ�.
    static IDE_RC checkPlanPrivTable(
        qcStatement                         * aStatement,
        qcgEnvTablePrivList                 * aTableList,
        qciGetSmiStatement4PrepareCallback    aGetSmiStmt4PrepareCallback,
        void                                * aGetSmiStmt4PrepareContext );

    // plan �������� sequence ��ü�� ���� �ʿ��� ������ �˻��Ѵ�.
    static IDE_RC checkPlanPrivSequence(
        qcStatement                        * aStatement,
        qcgEnvSequencePrivList             * aSequenceList,
        qciGetSmiStatement4PrepareCallback   aGetSmiStmt4PrepareCallback,
        void                               * aGetSmiStmt4PrepareContext );

    // plan �������� proc ��ü�� ���� �ʿ��� ������ �˻��Ѵ�.
    static IDE_RC checkPlanPrivProc(
        qcStatement                         * aStatement,
        qcgEnvProcList                      * aProcList,
        qciGetSmiStatement4PrepareCallback    aGetSmiStmt4PrepareCallback,
        void                                * aGetSmiStmt4PrepareContext );

    // BUG-37251 check privilege for package
    // plan �������� package ��ü�� ���� �ʿ��� ������ �˻��Ѵ�.
    static IDE_RC checkPlanPrivPkg(
        qcStatement                        * aStatement,
        qcgEnvPkgList                      * aPkgList,
        qciGetSmiStatement4PrepareCallback   aGetSmiStmt4PrepareCallback,
        void                               * aGetSmiStmt4PrepareContext );
    
    // qci::checkPrivilege���� �ʿ��� ��쿡�� smiStmt�� �����Ѵ�.
    static IDE_RC setSmiStmtCallback(
        qcStatement                        * aStatement,
        qciGetSmiStatement4PrepareCallback   aGetSmiStmt4PrepareCallback,
        void                               * aGetSmiStmt4PrepareContext );
    
    //-------------------------------------------------
    // plan cache ���������� ����ϱ� ���� �Լ�
    //-------------------------------------------------

    // template�� �����ϱ� ���� �ʱ�ȭ�� �����Ѵ�.
    static void   initPrepTemplate4Reuse( qcTemplate * aTemplate );

    // ���� template���� free �ߴ� tuple row�� ������Ѵ�.
    static IDE_RC allocUnusedTupleRow( qcSharedPlan * aSharedPlan );

    // ���� template���� free ������ tuple row�� �����Ѵ�.
    static IDE_RC freeUnusedTupleRow( qcSharedPlan * aSharedPlan );

private:

    //-------------------------------------------------
    // plan validation �Լ�
    //-------------------------------------------------
    
    // plan �������� table ��ü�� synonym ��ü�� ���� validation�� �����Ѵ�.
    static IDE_RC validatePlanSynonymTable( qcStatement       * aStatement,
                                            qcgEnvSynonymList * aSynonym,
                                            idBool            * aIsValid );
    
    // plan �������� sequence ��ü�� synonym ��ü�� ���� validation�� �����Ѵ�.
    static IDE_RC validatePlanSynonymSequence( qcStatement       * aStatement,
                                               qcgEnvSynonymList * aSynonym,
                                               idBool            * aIsValid );
    
    // plan �������� proc ��ü�� synonym ��ü�� ���� validation�� �����Ѵ�.
    static IDE_RC validatePlanSynonymProc( qcStatement       * aStatement,
                                           qcgEnvSynonymList * aSynonym,
                                           idBool            * aIsValid );
};

#endif /* _O_QCG_PLAN_H_ */
