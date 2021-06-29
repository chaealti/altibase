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
 * $Id: qdnTrigger.h 82075 2018-01-17 06:39:52Z jina.kim $
 *
 * Description :
 *
 *     [PROJ-1359] Trigger
 *
 *     Trigger ó���� ���� �ڷ� ���� �� �Լ�
 *
 * ��� ���� :
 *
 * ��� :
 *
 **********************************************************************/

#ifndef _O_QDN_TRIGGER_H_
#define _O_QDN_TRIGGER_H_  1

#include <qtcDef.h>
#include <qsParseTree.h>
#include <qsxProc.h>

//=========================================================
// [PROJ-1359] Trigger�� ���� Parse Tree ����
//=========================================================

//-----------------------------------------------------
// [Trigger Event ����]
//-----------------------------------------------------

// [Trigger Event�� ����]
typedef struct qdnTriggerEventTypeList
{
    UInt                       eventType;
    qcmColumn *                updateColumns;
    qdnTriggerEventTypeList *  next;
} qcmTriggerEventTypeList;

typedef struct qdnTriggerEvent {
    qcmTriggerEventTime       eventTime;
    qcmTriggerEventTypeList  *eventTypeList;
} qdnTriggerEvent;

//-----------------------------------------------------
// [Trigger Referencing ����]
//-----------------------------------------------------

// RERENCING ������ ��ϵǴ� OLD ROW, NEW ROW����
// Action Body������ [old_row T1%ROWTYPE] �Ͻ������� ���Ǹ�,
// �ϳ��� Node�� ǥ���ȴ�.  �̸� �����ϱ� ���� �����̸�,
// Trigger Action ���� �� �ش� ������ �̿��Ͽ� Procedure��
// Declare Secion�� ���� ��ȭ��Ų��.
//
// Ex) CREATE TRIGGER trigger_1 AFTER UPDATE ON t1
//     REFERENCING OLD ROW AS old_row
//     AS BEGIN
//         INSERT INTO log_table VALUES ( old_row.i1 );
//     END;
//
//     ===>  Action Body�� ���� Procedure ����
//
//     CREATE PROCEDURE trigger_procedure
//     AS
//         old_row T1%ROWTYPE;   <========== �ٷ� ������ ������ ����
//     AS BEGIN
//         INSERT INTO log_table VALUES ( old_row.i1 );
//     END;
//-----------------------------------------------------

// [REFERENCING ����]
typedef struct qdnTriggerRef
{
    qcmTriggerRefType         refType;
    qcNamePosition            aliasNamePos;
    qsVariables             * rowVar;          // referencing row
    qdnTriggerRef           * next;
} qdnTriggerRef;

//-----------------------------------------------------
// [Trigger Action Condition ����]
//-----------------------------------------------------

// [ACTION CONDITION ����]
typedef struct qdnTriggerActionCond
{
    qcmTriggerGranularity actionGranularity;

    // BUG-42989 Create trigger with enable/disable option.
    qcmTriggerAble        isEnable;

    qtcNode *             whenCondition;
} qdnTriggerActionCond;

// [ACTION BODY�� �����ϴ� DML Table]
// Cycle Detection�� ���Ͽ� �����Ѵ�.

typedef struct qdnActionRelatedTable
{
    UInt                    tableID;
    qsProcStmtType          stmtType;
    qdnActionRelatedTable * next;
} qdnActionRelatedTable;

//-----------------------------------------------------
// [ALTER TRIGGER�� ���� ����]
//-----------------------------------------------------

typedef enum {
    QDN_TRIGGER_ALTER_NONE,
    QDN_TRIGGER_ALTER_ENABLE,
    QDN_TRIGGER_ALTER_DISABLE,
    QDN_TRIGGER_ALTER_COMPILE
} qdnTriggerAlterOption;

//===========================================================
// Trigger�� ���� Parse Tree
//===========================================================

// Example )
//     CREATE TRIGGER triggerUserName.triggerName
//     AFTER UPDATE OF updateColumns ON userName.tableName
//     REFERENCING OLD ROW AS aliasName
//     FOR EACH ROW WHEN tableName.i1 > 30
//     AS
//     BEGIN
//         INSERT INTO log_table VALUES ( aliasName.i1 );
//     END;

// [CREATE TRIGGER�� ���� Parse Tree]
typedef struct qdnCreateTriggerParseTree
{
    //---------------------------------------------
    // Parsing ����
    //---------------------------------------------

    qcParseTree                common;

    // Trigger Name ����
    qcNamePosition             triggerUserPos;
    qcNamePosition             triggerNamePos;

    // Trigger Table ����
    qcNamePosition             userNamePos;
    qcNamePosition             tableNamePos;

    // Trigger Event ����
    qdnTriggerEvent            triggerEvent;

    // Referencing ����
    qdnTriggerRef            * triggerReference;

    // Trigger Action ����
    qdnTriggerActionCond       actionCond;
    qsProcParseTree            actionBody;

    smOID                      triggerOID;
    smSCN                      triggerSCN;

    //---------------------------------------------
    // Validation ����
    //---------------------------------------------

    // recompile ����
    idBool                     isRecompile;

    // Trigger Name ����
    UInt                       triggerUserID;
    SChar                      triggerUserName[QC_MAX_OBJECT_NAME_LEN + 1];
    SChar                      triggerName[QC_MAX_OBJECT_NAME_LEN + 1];

    // Trigger Table ����
    UInt                       tableUserID;
    UInt                       tableID;
    smOID                      tableOID;
    qcmTableInfo             * tableInfo;

    // TASK-2176
    void                     * tableHandle;
    smSCN                      tableSCN;

    // BUG-20948
    // Trigger�� Target Table�� �ٲ���� ��� Original Table�� ������ �����ϱ� ���� ����
    UInt                       orgTableUserID;
    UInt                       orgTableID;
    smOID                      orgTableOID;
    qcmTableInfo             * orgTableInfo;

    void                     * orgTableHandle; 
    smSCN                      orgTableSCN;       

    // Ʈ���ŵ� ���ν����� �����ϰ� �κ� ����尡 �����ϴ�.
    // �κ� ����� ���� ������ qsxProcInfo�� �����ϱ� ������ ������
    // qsxProcInfo�� ������ �־�� �Ѵ�.
    qsxProcInfo                procInfo;

    // BUG-21761
    // NŸ���� UŸ������ ������ų �� ���
    qcNamePosList            * ncharList;

    // PROJ-2219 Row-level before update trigger
    // count and list of referenced columns by a trigger.
    UInt                       refColumnCount;
    UChar                    * refColumnList;

} qdnCreateTriggerParseTree;

// PROJ-2219 Row-level before update trigger
#define QDN_REF_COLUMN_FALSE ((UChar)0X00)
#define QDN_REF_COLUMN_TRUE  ((UChar)0X01)

#define QDN_CREATE_TRIGGER_PARSE_TREE_INIT( _dst_ ) \
    {                                               \
        SET_EMPTY_POSITION(_dst_->triggerUserPos);  \
        SET_EMPTY_POSITION(_dst_->triggerNamePos);  \
        SET_EMPTY_POSITION(_dst_->userNamePos);     \
        SET_EMPTY_POSITION(_dst_->tableNamePos);    \
        _dst_->ncharList = NULL;                    \
    }


// [ALTER TRIGGER�� ���� Parse Tree]
typedef struct qdnAlterTriggerParseTree
{
    //---------------------------------------------
    // Parsing ����
    //---------------------------------------------

    qcParseTree                common;

    // Trigger Name ����
    qcNamePosition             triggerUserPos;
    qcNamePosition             triggerNamePos;

    qdnTriggerAlterOption      option;

    //---------------------------------------------
    // Validation ����
    //---------------------------------------------

    // Trigger Name ����
    UInt                       triggerUserID;
    smOID                      triggerOID;

    // Trigger�� �����ϴ� Table ����
    UInt                       tableID;

} qdnAlterTriggerParseTree;

#define QDN_ALTER_TRIGGER_PARSE_TREE_INIT( _dst_ )  \
    {                                               \
        SET_EMPTY_POSITION(_dst_->triggerUserPos);  \
        SET_EMPTY_POSITION(_dst_->triggerNamePos);  \
    }


// [DROP TRIGGER�� ���� Parse Tree]
typedef struct qdnDropTriggerParseTree
{
    //---------------------------------------------
    // Parsing ����
    //---------------------------------------------

    qcParseTree                common;

    // Trigger Name ����
    qcNamePosition             triggerUserPos;
    qcNamePosition             triggerNamePos;

    //---------------------------------------------
    // Validation ����
    //---------------------------------------------

    // Trigger Name ����
    UInt                       triggerUserID;
    smOID                      triggerOID;

    // Trigger�� �����ϴ� Table ����
    UInt                       tableID;
    qcmTableInfo             * tableInfo;

    // TASK-2176
    void                     * tableHandle;
    smSCN                      tableSCN;

} qdnDropTriggerParseTree;

#define QDN_DROP_TRIGGER_PARSE_TREE_INIT( _dst_ )   \
    {                                               \
        SET_EMPTY_POSITION(_dst_->triggerUserPos);  \
        SET_EMPTY_POSITION(_dst_->triggerNamePos);  \
    }

//===========================================================
// [Trigger Object�� ���� Cache ����]
//
//     [Trigger Handle] -- info ----> CREATE TRIGGER String
//                      |
//                      -- tempInfo ----> Trigger Object Cache
//
// Trigger Object Cache�� CREATE TRIGGER �Ǵ� Server ���� ��
// �����Ǹ�,  Trigger�� �����ϱ� ���� �Ϻ� �������� �����Ѵ�.
// Trigger ������ ���ü� ���� �� Trigger Action ������ ����ȴ�.
//===========================================================

typedef struct qdnTriggerCache
{
    iduLatch                     latch;      // ���ü� ��� ���� latch
    idBool                       isValid;    // Cache ������ ��ȿ�� ����
    qcStatement                  triggerStatement; // PVO�� �Ϸ�� Statement ����
    
    UInt                         stmtTextLen;
    SChar                      * stmtText;
} qdnTriggerCache;

//=========================================================
// [PROJ-1359] Trigger�� ���� �Լ�
//=========================================================

class qdnTrigger
{
public:

    //----------------------------------------------
    // CREATE TRIGGER�� ���� �Լ�
    //----------------------------------------------

    static IDE_RC parseCreate( qcStatement * aStatement );
    static IDE_RC validateCreate( qcStatement * aStatement );
    static IDE_RC validateRecompile( qcStatement * aStatement );
    static IDE_RC optimizeCreate( qcStatement * aStatement );
    static IDE_RC executeCreate( qcStatement * aStatement );
    static IDE_RC validateReplace( qcStatement * aStatement );
    static IDE_RC executeReplace( qcStatement * aStatement );

    //----------------------------------------------
    // ALTER TRIGGER�� ���� �Լ�
    //----------------------------------------------

    static IDE_RC parseAlter( qcStatement * aStatement );
    static IDE_RC validateAlter( qcStatement * aStatement );
    static IDE_RC optimizeAlter( qcStatement * aStatement );
    static IDE_RC executeAlter( qcStatement * aStatement );

    //----------------------------------------------
    // DROP TRIGGER�� ���� �Լ�
    //----------------------------------------------

    static IDE_RC parseDrop( qcStatement * aStatement );
    static IDE_RC validateDrop( qcStatement * aStatement );
    static IDE_RC optimizeDrop( qcStatement * aStatement );
    static IDE_RC executeDrop( qcStatement * aStatement );

    //----------------------------------------------
    // TRIGGER ������ ���� �Լ�
    //----------------------------------------------

    // Trigger Referencing Row�� Build�� �ʿ������� �Ǵ�
    static IDE_RC needTriggerRow( qcStatement         * aStatement,
                                  qcmTableInfo        * aTableInfo,
                                  qcmTriggerEventTime   aEventTime,
                                  UInt                  aEventType,
                                  smiColumnList       * aUptColumn,
                                  idBool              * aIsNeed );

    // Trigger�� �����Ѵ�.
    static IDE_RC fireTrigger( qcStatement         * aStatement,
                               iduMemory           * aNewValueMem,
                               qcmTableInfo        * aTableInfo,
                               qcmTriggerGranularity aGranularity,
                               qcmTriggerEventTime   aEventTime,
                               UInt                  aEventType,
                               smiColumnList       * aUptColumn,
                               smiTableCursor      * aTableCursor,
                               scGRID                aGRID,
                               void                * aOldRow,
                               qcmColumn           * aOldRowColumns,
                               void                * aNewRow,
                               qcmColumn           * aNewRowColumns );

    //----------------------------------------------
    // Trigger�� �ϰ� ó�� �۾�
    //----------------------------------------------

    // Server ������ Trigger Object Cache������ �����Ѵ�.
    static IDE_RC loadAllTrigger( smiStatement * aSmiStmt );

    // Table �� ������ Trigger�� ��� �����Ѵ�.
    static IDE_RC dropTrigger4DropTable( qcStatement  * aStatement,
                                         qcmTableInfo * aTableInfo );

    // To Fix BUG-12034
    // qdd::executeDropTable ����
    // SM�� ���̻� ������ ������ ���� ����
    // �� �Լ��� ȣ���Ͽ� TriggerChache�� free�Ѵ�.
    static IDE_RC freeTriggerCaches4DropTable( qcmTableInfo * aTableInfo );

    // Trigger Cache �� �����Ѵ�.
    static IDE_RC freeTriggerCache( qdnTriggerCache * aCache );

    /* PROJ-2600 Online DDL for Tablespace Alteration */
    static void freeTriggerCacheArray( qdnTriggerCache ** aTriggerCaches,
                                       UInt               aTriggerCount );

    /* PROJ-2600 Online DDL for Tablespace Alteration */
    static void restoreTempInfo( qdnTriggerCache ** aTriggerCaches,
                                 qcmTableInfo     * aTableInfo );

    // BUG-16543
    static IDE_RC setInvalidTriggerCache4Table( qcmTableInfo * aTableInfo );

    // BUG-31406
    static IDE_RC executeRenameTable( qcStatement  * aStatement,
                                      qcmTableInfo * aTableInfo );

    /* PROJ-2600 Online DDL for Tablespace Alteration */
    static IDE_RC executeCopyTable( qcStatement       * aStatement,
                                    qcmTableInfo      * aSourceTableInfo,
                                    qcmTableInfo      * aTargetTableInfo,
                                    qcNamePosition      aNamesPrefix,
                                    qdnTriggerCache *** aTargetTriggerCache );

    /* PROJ-2600 Online DDL for Tablespace Alteration */
    static IDE_RC executeSwapTable( qcStatement       * aStatement,
                                    qcmTableInfo      * aSourceTableInfo,
                                    qcmTableInfo      * aTargetTableInfo,
                                    qcNamePosition      aNamesPrefix,
                                    qdnTriggerCache *** aOldSourceTriggerCache,
                                    qdnTriggerCache *** aOldTargetTriggerCache,
                                    qdnTriggerCache *** aNewSourceTriggerCache,
                                    qdnTriggerCache *** aNewTargetTriggerCache );

    // PROJ-2219 Row-level before update trigger
    static IDE_RC verifyTriggers( qcStatement   * aQcStmt,
                                  qcmTableInfo  * aTableInfo,
                                  smiColumnList * aUptColumn,
                                  idBool        * aIsNeedRebuild );

    //----------------------------------------------
    // CREATE TRIGGER�� Execution�� ���� �Լ�
    //----------------------------------------------
    
    // Trigger Handle�� ���� Cache ������ �Ҵ��Ѵ�.
    static IDE_RC allocTriggerCache( void             * aTriggerHandle,
                                     smOID              aTriggerOID,
                                     qdnTriggerCache ** aCache );
private:

    //----------------------------------------------
    // CREATE TRIGGER�� Parsing�� ���� �Լ�
    //----------------------------------------------

    // FOR EACH ROW�� Validation�� ���� �ΰ� ������ �߰���
    static IDE_RC addGranularityInfo( qdnCreateTriggerParseTree * aParseTree );

    // FOR EACH ROW�� Action Body�� ���� �ΰ� ������ �߰���.
    static IDE_RC addActionBodyInfo( qcStatement               * aStatement,
                                     qdnCreateTriggerParseTree * aParseTree );

    //----------------------------------------------
    // CREATE TRIGGER�� Validation�� ���� �Լ�
    //----------------------------------------------

    // BUG-24570
    // Trigger User�� ���� ���� ����
    static IDE_RC setTriggerUser( qcStatement               * aStatement,
                                  qdnCreateTriggerParseTree * aParseTree );

    // Trigger ������ ���� ���� �˻�
    static IDE_RC valPrivilege( qcStatement               * aStatement,
                                qdnCreateTriggerParseTree * aParseTree );

    // Trigger Name�� ���� Validation
    static IDE_RC valTriggerName( qcStatement               * aStatement,
                                  qdnCreateTriggerParseTree * aParseTree );

    // Recompile�� ���� Trigger Name�� ���� Validation
    static IDE_RC reValTriggerName( qcStatement               * aStatement,
                                    qdnCreateTriggerParseTree * aParseTree );

    // Trigger Table�� ���� Validation
    static IDE_RC valTableName( qcStatement               * aStatement,
                                qdnCreateTriggerParseTree * aParseTree );

    // To Fix BUG-20948
    static IDE_RC valOrgTableName( qcStatement               * aStatement,
                                   qdnCreateTriggerParseTree * aParseTree );

    // Trigger Event�� Referencing �� ���� Validation
    static IDE_RC valEventReference( qcStatement               * aStatement,
                                     qdnCreateTriggerParseTree * aParseTree );

    // Action Condition�� ���� Validation
    static IDE_RC valActionCondition( qcStatement               * aStatement,
                                      qdnCreateTriggerParseTree * aParseTree );

    // Action Body�� ���� Validation
    static IDE_RC valActionBody( qcStatement               * aStatement,
                                 qdnCreateTriggerParseTree * aParseTree );
    
    //PROJ-1888 INSTEAD OF TRIGGER ������� �˻�
    static IDE_RC valInsteadOfTrigger( qdnCreateTriggerParseTree * aParseTree );
    
    // Action Body�� �����ϴ� Cycle�� �˻�
    static IDE_RC checkCycle( qcStatement               * aStatement,
                              qdnCreateTriggerParseTree * aParseTree );

    // DML�� �����Ǵ� Table�� ������ Trigger�κ��� Cycle�� �˻�
    static IDE_RC checkCycleOtherTable( qcStatement               * aStatement,
                                        qdnCreateTriggerParseTree * aParseTree,
                                        UInt                        aTableID );

    //----------------------------------------------
    // CREATE TRIGGER�� Execution�� ���� �Լ�
    //----------------------------------------------

    // Trigger Object�� �����Ѵ�.
    static IDE_RC createTriggerObject( qcStatement     * aStatement,
                                       void           ** aTriggerHandle );

    //----------------------------------------------
    // DROP TRIGGER�� Execution�� ���� �Լ�
    //----------------------------------------------

    // Trigger Object�� �����Ѵ�.
    static IDE_RC dropTriggerObject( qcStatement     * aStatement,
                                     void            * aTriggerHandle );

    //----------------------------------------------
    // Trigger ������ ���� �Լ�
    //----------------------------------------------

    // ���ǿ� �����ϴ� Trigger���� �˻�
    static IDE_RC checkCondition( qcStatement         * aStatement,
                                  qcmTriggerInfo      * aTriggerInfo,
                                  qcmTriggerGranularity aGranularity,
                                  qcmTriggerEventTime   aEventTime,
                                  UInt                  aEventType,
                                  smiColumnList       * aUptColumn,
                                  idBool              * aNeedAction,
                                  idBool              * aIsRecompile );

    // Validation ���θ� ����Ͽ� Trigger Action�� �����Ѵ�.
    static IDE_RC fireTriggerAction( qcStatement    * aStatement,
                                     iduMemory      * aNewValueMem,
                                     qcmTableInfo   * aTableInfo,
                                     qcmTriggerInfo * aTriggerInfo,
                                     smiTableCursor * aTableCursor,
                                     scGRID           aGRID,
                                     void           * aOldRow,
                                     qcmColumn      * aOldRowColumns,
                                     void           * aNewRow,
                                     qcmColumn      * aNewRowColumns );

    // Validate�� ��� Trigger Action�� �����Ѵ�.
    static IDE_RC runTriggerAction( qcStatement           * aStatement,
                                    iduMemory             * aNewValueMem,
                                    qcmTableInfo          * aTableInfo,
                                    qcmTriggerInfo        * aTriggerInfo,
                                    smiTableCursor        * aTableCursor,
                                    scGRID                  aGRID,
                                    void                  * aOldRow,
                                    qcmColumn             * aOldRowColumns,
                                    void                  * aNewRow,
                                    qcmColumn             * aNewRowColumns );

    // WHEN Condition �� Action Body������ ó���� �� �ֵ���
    // REFERENCING ROW�� ������ �����Ѵ�.
    static IDE_RC setReferencingRow( qcTemplate                * aClonedTemplate,
                                     qcmTableInfo              * aTableInfo,
                                     qdnCreateTriggerParseTree * aParseTree,
                                     smiTableCursor            * aTableCursor,
                                     scGRID                      aGRID,
                                     void                      * aOldRow,
                                     qcmColumn                 * aOldRowColumns,
                                     void                      * aNewRow,
                                     qcmColumn                 * aNewRowColumns );

    // WHEN Condition �� Action Body������ ó���� �� �ֵ���
    // REFERENCING ROW�� ������ �����Ѵ�.
    static IDE_RC makeValueListFromReferencingRow(
        qcTemplate                * aClonedTemplate,
        iduMemory                 * aNewValueMem,
        qcmTableInfo              * aTableInfo,
        qdnCreateTriggerParseTree * aParseTree,
        void                      * aNewRow,
        qcmColumn                 * aNewRowColumn );
    
    // tableRow�� PSM rowtype���� ��ȯ�Ͽ� ����
    static IDE_RC copyRowFromTableRow( qcTemplate        * aTemplate,
                                       qcmTableInfo      * aTableInfo,
                                       qsVariables       * aVariable,
                                       smiTableCursor    * aTableCursor,
                                       void              * aTableRow,
                                       scGRID              aGRID,
                                       qcmColumn         * aTableRowColumns,
                                       qcmTriggerRefType   aRefType );

    // value list�� PSM rowtype���� ��ȯ�Ͽ� ����
    static IDE_RC copyRowFromValueList( qcTemplate   * aTemplate,
                                        qcmTableInfo * aTableInfo,
                                        qsVariables  * aVariable,
                                        void         * aValueList,
                                        qcmColumn    * aTableRowColumns );

    // row�� ���� value list�� ����
    static IDE_RC copyValueListFromRow( qcTemplate   * aTemplate,
                                        iduMemory    * aNewValueMem,
                                        qcmTableInfo * aTableInfo,
                                        qsVariables  * aVariable,
                                        void         * aValueList,
                                        qcmColumn    * aTableRowColumns );

    // BUG-20797
    // Trigger���� �����ϴ� PSM�� ������ �˻��Ѵ�.
    static IDE_RC checkObjects( qcStatement  * aStatement,
                                idBool       * aInvalidProc );

    // Trigger�� Recompile�Ѵ�.
    static IDE_RC recompileTrigger( qcStatement     * aStatement,
                                    qcmTriggerInfo  * aTriggerInfo );

    //fix PROJ-1596
    static IDE_RC allocProcInfo( qcStatement * aStatement,
                                 UInt          aTriggerUserID );

    static IDE_RC getTriggerSCN( smOID    aTriggerOID,
                                 smSCN  * aTriggerSCN );

    /* PROJ-1530 PSM/Trigger���� LOB ����Ÿ Ÿ�� ���� */
    static IDE_RC convertXlobToXlobValue( mtcTemplate        * aTemplate,
                                          smiTableCursor     * aTableCursor,
                                          void               * aTableRow,
                                          scGRID               aGRID,
                                          mtcColumn          * aSrcLobColumn,
                                          void               * aSrcValue,
                                          mtcColumn          * aDestLobColumn,
                                          void               * aDestValue,
                                          qcmTriggerRefType    aRefType );

    /* PROJ-2219 Row-level before update trigger */
    static IDE_RC makeRefColumnList( qcStatement * aQcStmt );

    // BUG-38137 Trigger�� when condition���� PSM�� ȣ���� �� ����.
    static IDE_RC checkNoSpFunctionCall( qtcNode * aNode );

    /* PROJ-2600 Online DDL for Tablespace Alteration */
    static IDE_RC makeNewTriggerStringForSwap( qcStatement      * aStatement,
                                               qdnTriggerCache  * aTriggerCache,
                                               SChar            * aTriggerStmtBuffer,
                                               void             * aTriggerHandle,
                                               smOID              aTriggerOID,
                                               UInt               aTableID,
                                               SChar            * aNewTriggerName,
                                               UInt               aNewTriggerNameLen,
                                               SChar            * aPeerTableNameStr,
                                               UInt               aPeerTableNameLen,
                                               qdnTriggerCache ** aNewTriggerCache );

};
#endif // _O_QDN_TRIGGER_H_
