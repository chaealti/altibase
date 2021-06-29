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
 * $Id: qdnForeignKey.h 82075 2018-01-17 06:39:52Z jina.kim $
 *
 * Description :
 *
 *     Foreign Key Reference Constraint ó���� ���� ���
 *
 * ��� ���� :
 *
 * ��� :
 *
 **********************************************************************/

#ifndef _O_QDN_FOREIGN_KEY_H_
#define _O_QDN_FOREIGN_KEY_H_  1

#include <qdn.h>
#include <qmsIndexTable.h>

// PROJ-1624 non-partitioned index
typedef enum qdnFKScanMethod
{
    QDN_FK_TABLE_FULL_SCAN = 0,   // table full scan�� ���
    QDN_FK_TABLE_INDEX_SCAN,      // table index scan�� ���
    QDN_FK_PARTITION_FULL_SCAN,   // table partition full scan�� ���
    QDN_FK_PARTITION_INDEX_SCAN,  // table partition local index scan�� ���
    QDN_FK_PARTITION_ONLY_INDEX_TABLE_SCAN,  // index table scan�� ���
    QDN_FK_PARTITION_INDEX_TABLE_SCAN        // index table scan + partition scan�� ���
} qdnFKScanMethod;

// PROJ-1624 non-partitioned index
#define QDN_PART_CURSOR_ALLOCED      ((UChar)'A')  // alloc�� ����
#define QDN_PART_CURSOR_INITIALIZED  ((UChar)'I')  // init�� ����, open�Ǿ��� closed�� ����
#define QDN_PART_CURSOR_OPENED       ((UChar)'O')  // open�� ����

class qdnForeignKey
{
public:

    //----------------------------------------------------------
    // Validation for Add Foreign Key Reference
    //----------------------------------------------------------

    // Foreign Key�� ���� Vaildation
    static IDE_RC validateForeignKeySpec( qcStatement      * aStatement,
                                          UInt               aTableOwnerID,
                                          qcmTableInfo     * aTableInfo,
                                          qdConstraintSpec * aConstr );

    // To Fix PR-10385
    // Foreign Key Constraint �߰��� Data�� ���� Reference �˻�
    static IDE_RC checkRef4AddConst( qcStatement      * aStatement,
                                     qcmTableInfo     * aTableInfo,
                                     qdConstraintSpec * aForeignSpec );

    // Foreign Key Constraint ���� ����� Data�� ���� Reference �˻�
    static IDE_RC checkRef4ModConst( qcStatement      * aStatement,
                                     qcmTableInfo     * aTableInfo,
                                     qcmForeignKey    * aForeignKey );

    //----------------------------------------------------------
    // DML �߻� �� Foreign Key Reference�� ó���ϱ� ���� �Լ�
    //----------------------------------------------------------

    // BUG-28049���� FK Ž�� ��� ����

    // Parent Table�� �ش� Reference���� �����ϴ� �� �˻�.
    static IDE_RC checkParentRef( qcStatement   * aStatement,
                                  UInt          * aChangedColumns,
                                  qcmParentInfo * aParentInfo,
                                  mtcTuple      * aCheckedTuple,
                                  const void    * aCheckedRow,
                                  UInt            aUpdatedColCount );

    // Parent Table�� �ش� Reference���� �����ϴ� �� �˻�.
    static IDE_RC checkParentRef4Const( qcStatement   * aStatement,
                                        UInt          * aChangedColumns,
                                        qcmParentInfo * aParentInfo,
                                        const void    * aCheckedRow,
                                        qcmColumn     * aColumnsForCheckedRow,
                                        UInt            aUpdatedColCount );

    // Delete �� Child Table�� ���� Foreign Key Reference �˻�
    static IDE_RC checkChildRefOnDelete( qcStatement      * aStatement,
                                         qcmRefChildInfo  * aChildConstraints,
                                         UInt               aParentTableID,
                                         mtcTuple         * aCheckedTuple,
                                         const void       * aCheckedRow,
                                         idBool             aIsOnDelete );

    // Update�� Child Table�� ���� Foreign Key Reference �˻�
    static IDE_RC checkChildRefOnUpdate( qcStatement      * aStatement,
                                         qmsTableRef      * aTableRef,
                                         qcmTableInfo     * aTableInfo,
                                         UInt             * aChangedColumns,
                                         qcmRefChildInfo  * aChildConstraints,
                                         UInt               aParentTableID,
                                         mtcTuple         * aCheckedTuple,
                                         const void       * aCheckedRow,
                                         UInt               aChangedColCount);

    // Delete ���� �� Foreign Key Reference �˻縦 ���� Cursor Open ������ �Ǵ�
    static idBool haveToOpenBeforeCursor( qcmRefChildInfo  * aChildInfo,
                                          UInt             * aChangedColumns,
                                          UInt               aChangedColCount );
    
    // Parent�� ���� Foreign Key �˻簡 �ʿ��� ���� ����
    static idBool haveToCheckParent( qcmTableInfo * aTableInfo,
                                     UInt         * aChangedColumns,
                                     UInt           aChangedColCount );

private:

    //----------------------------------------------------------
    // Validation for Add Foreign Key Reference
    //----------------------------------------------------------

    // Foreign Key�� ���� ������, Unique Key�� ������ �µ��� ������
    static IDE_RC reorderForeignKeySpec( qdConstraintSpec * aUniqueKeyConstr,
                                         qdConstraintSpec * aForeignKeyConstr );

    static IDE_RC reorderForeignKeySpec( qcmIndex         * aUniqueIndex,
                                         qdConstraintSpec * aForeignKeyConstr );

    // Primary Key ���� ����
    static IDE_RC getPrimaryKeyFromDefinition(
        qdTableParseTree * aParseTree,
        qdReferenceSpec  * aReferenceSpec );

    // Foreign Key Validation�� ���� Primary Key ȹ��
    static IDE_RC getPrimaryKeyFromCache( iduVarMemList * aMem, //fix PROJ-1596
                                          qcmTableInfo  * aTableInfo,
                                          qcmColumn    ** aColumns,
                                          UInt          * aColCount);

    // To Fix PR-10385
    // Parent Table�� Index �� Meta Cache ���� ����
    static IDE_RC getParentTableAndIndex( qcStatement     * aStatement,
                                          qdReferenceSpec * aRefSpec,
                                          qcmTableInfo    * aChildTableInfo,
                                          qcmTableInfo   ** aParentTableInfo,
                                          qcmIndex       ** aParentIndexInfo );

    //----------------------------------------------------------
    // Parent Table�� ���� Foreign Key Reference �˻� �Լ�
    //----------------------------------------------------------

    // Parent Table�� �ش� Key�� �����ϴ� �� �˻�
    static IDE_RC searchParentKey( qcStatement   * aStatement,
                                   qcmParentInfo * aParentInfo,
                                   mtcTuple      * aCheckedTuple,
                                   const void    * aCheckedRow,
                                   qcmColumn     * aColumnsForCheckedRow );

    //----------------------------------------------------------
    // Child Table�� ���� Foreign Key Reference �˻� �Լ�
    //----------------------------------------------------------

    // BUG-28049���� FK Ž�� ��� ����

    // NULL���ο� Child���� ����ϰ� �ִ� ������ �˻�
    static IDE_RC searchForeignKey(qcStatement       * aStatement,
                                   qcmRefChildInfo   * aChildConstraints,
                                   qcmRefChildInfo   * aRefChildInfo,
                                   mtcTuple          * aCheckedTuple,
                                   const void        * aCheckedRow,
                                   idBool              aIsOnDelete );

    // Self Foreign Key Reference�� ���� �˻�
    static IDE_RC searchSelf( qcStatement  * aStatement,
                              qmsTableRef  * aTableRef,
                              qcmTableInfo * aTableInfo,
                              qcmIndex     * aSelfIndex,
                              mtcTuple     * aCheckedTuple,
                              const void   * aCheckedRow );

    // Child���� ����ϰ� �ִ� ������ �˻�
    static IDE_RC getChildRowWithRow( qcStatement      * aStatement,
                                      qcmRefChildInfo  * aRefChildInfo,
                                      mtcTuple         * aCheckedTuple,
                                      const void       * aCheckedRow );

    // Child���� ����ϰ� �ִ� ������ ���� ( on delete cascade���� ��� )
    static IDE_RC deleteChildRowWithRow(
        qcStatement       * aStatement,
        qcmRefChildInfo   * aChildConstraints,
        qcmRefChildInfo   * aRefChildInfo,
        mtcTuple          * aCheckedTuple,
        const void        * aCheckedRow );

    // PROJ-2212 foreign key on delete set null
    static IDE_RC updateNullChildRowWithRow(
        qcStatement       * aStatement,
        qcmRefChildInfo   * aChildConstraints,
        qcmRefChildInfo   * aRefChildInfo,
        mtcTuple          * aCheckedTuple,
        const void        * aCheckedRow );

    // To Fix PR-10592
    // Child Table�� ���� �˻縦 ���Ͽ� Key Range������ ���� ������ ȹ��
    static IDE_RC getChildKeyRangeInfo( qcmTableInfo  * aChildTableInfo,
                                        qcmForeignKey * aForeignKey,
                                        qcmIndex     ** aSelectedIndex,
                                        UInt          * aKeyColumnCnt );

    // PROJ-1624 non-partitioned index
    static IDE_RC validateScanMethodAndLockTable( qcStatement       * aStatement,
                                                  qcmRefChildInfo   * aRefChildInfo,
                                                  qcmIndex         ** aSelectedIndex,
                                                  UInt              * aKeyRangeColumnCnt,
                                                  qmsIndexTableRef ** aSelectedIndexTable,
                                                  qdnFKScanMethod   * aScanMethod );

    // PROJ-1624 non-partitioned index
    static IDE_RC validateScanMethodAndLockTableForDelete( qcStatement       * aStatement,
                                                           qcmRefChildInfo   * aRefChildInfo,
                                                           qcmIndex         ** aSelectedIndex,
                                                           UInt              * aKeyRangeColumnCnt,
                                                           qmsIndexTableRef ** aSelectedIndexTable,
                                                           qdnFKScanMethod   * aScanMethod );

    // PROJ-1624 non-partitioned index
    static IDE_RC makeKeyRangeAndIndex( UInt                  aTableType,
                                        UInt                  aKeyRangeColumnCnt,
                                        qcmIndex            * aSelectedIndex,
                                        qcmIndex            * aUniqueConstraint,
                                        mtcTuple            * aCheckedTuple,
                                        const void          * aCheckedRow,
                                        qtcMetaRangeColumn  * aRangeColumn,
                                        smiRange            * aRange,
                                        qcmIndex           ** aIndex,
                                        smiRange           ** aKeyRange );

    // PROJ-1624 non-partitioned index
    static IDE_RC changePartIndex( qcmTableInfo   * aPartitionInfo,
                                   UInt             aKeyRangeColumnCnt,
                                   qcmIndex       * aSelectedIndex,
                                   qcmIndex      ** aIndex );

    // PROJ-1624 non-partitioned index
    static IDE_RC getChildRowWithRow4Table( qcStatement    * aStatement,
                                            qmsTableRef    * aChildTableRef,
                                            qcmTableInfo   * aChildTableInfo,
                                            idBool           aIsIndexTable,
                                            qcmForeignKey  * aForeignKey,
                                            UInt             aKeyRangeColumnCnt,
                                            qcmIndex       * aSelectedIndex,
                                            qcmIndex       * aUniqueConstraint,
                                            mtcTuple       * aCheckedTuple,
                                            const void     * aCheckedRow );
    
    // PROJ-1624 non-partitioned index
    static IDE_RC getChildRowWithRow4Partition( qcStatement    * aStatement,
                                                qmsTableRef    * aChildTableRef,
                                                qcmForeignKey  * aForeignKey,
                                                UInt             aKeyRangeColumnCnt,
                                                qcmIndex       * aSelectedIndex,
                                                qcmIndex       * aUniqueConstraint,
                                                mtcTuple       * aCheckedTuple,
                                                const void     * aCheckedRow );

    // PROJ-1624 non-partitioned index
    static IDE_RC getChildRowWithRow4IndexTable( qcStatement      * aStatement,
                                                 qmsIndexTableRef * aChildIndexTable,
                                                 qmsTableRef      * aChildTableRef,
                                                 qcmTableInfo     * aChildTableInfo,
                                                 qcmForeignKey    * aForeignKey,
                                                 UInt               aKeyRangeColumnCnt,
                                                 qcmIndex         * aSelectedIndex,
                                                 qcmIndex         * aUniqueConstraint,
                                                 mtcTuple         * aCheckedTuple,
                                                 const void       * aCheckedRow );
    
    // PROJ-1624 non-partitioned index
    static IDE_RC checkChildRow( qcmForeignKey    * aForeignKey,
                                 UInt               aKeyRangeColumnCnt,
                                 qcmIndex         * aUniqueConstraint,
                                 mtcTuple         * aCheckedTuple,
                                 mtcTuple         * aChildTuple,
                                 const void       * aCheckedRow,
                                 const void       * aChildRow );

    // PROJ-1624 non-partitioned index
    static IDE_RC deleteChildRowWithRow4Table( qcStatement      * aStatement,
                                               qcmRefChildInfo  * aChildConstraints,
                                               qmsTableRef      * aChildTableRef,
                                               qcmTableInfo     * aChildTableInfo,
                                               qcmForeignKey    * aForeignKey,
                                               UInt               aKeyRangeColumnCnt,
                                               qcmIndex         * aSelectedIndex,
                                               qcmIndex         * aUniqueConstraint,
                                               mtcTuple         * aCheckedTuple,
                                               const void       * aCheckedRow );

    // PROJ-1624 non-partitioned index
    static IDE_RC deleteChildRowWithRow4Partition( qcStatement      * aStatement,
                                                   qcmRefChildInfo  * aChildConstraints,
                                                   qmsTableRef      * aChildTableRef,
                                                   qcmTableInfo     * aChildTableInfo,
                                                   qcmForeignKey    * aForeignKey,
                                                   UInt               aKeyRangeColumnCnt,
                                                   qcmIndex         * aSelectedIndex,
                                                   qcmIndex         * aUniqueConstraint,
                                                   mtcTuple         * aCheckedTuple,
                                                   const void       * aCheckedRow );

    // PROJ-1624 non-partitioned index
    static IDE_RC deleteChildRowWithRow4IndexTable( qcStatement      * aStatement,
                                                    qcmRefChildInfo  * aChildConstraints,
                                                    qmsIndexTableRef * aChildIndexTable,
                                                    qmsTableRef      * aChildTableRef,
                                                    qcmTableInfo     * aChildTableInfo,
                                                    qcmForeignKey    * aForeignKey,
                                                    UInt               aKeyRangeColumnCnt,
                                                    qcmIndex         * aUniqueConstraint,
                                                    mtcTuple         * aCheckedTuple,
                                                    const void       * aCheckedRow );

    // PROJ-1624 non-partitioned index
    static IDE_RC deleteChildRow( qcStatement          * aStatement,
                                  qmsTableRef          * aChildTableRef,
                                  qcmTableInfo         * aChildTableInfo,
                                  qmsPartitionRef      * aChildPartitionRef,
                                  qcmTableInfo         * aChildPartitionInfo,
                                  qcmForeignKey        * aForeignKey,
                                  UInt                   aKeyRangeColumnCnt,
                                  qcmIndex             * aUniqueConstraint,
                                  smiTableCursor       * aChildCursor,
                                  mtcTuple             * aCheckedTuple,
                                  const void           * aCheckedRow,
                                  const void           * aChildRow,
                                  void                 * aChildTriggerRow,
                                  UInt                   aChildRowSize,
                                  qcmColumn            * aColumnsForChildRow,
                                  smiTableCursor       * aSelectedIndexTableCursor,
                                  qmsIndexTableCursors * aIndexTableCursor,
                                  scGRID                 aIndexGRID );
    
    // PROJ-1624 non-partitioned index
    static IDE_RC checkChild4DeleteChildRow( qcStatement      * aStatement,
                                             qcmRefChildInfo  * aChildConstraints,
                                             qcmTableInfo     * aChildTableInfo,
                                             mtcTuple         * aCheckedTuple,
                                             const void       * aOldRow );

    // PROJ-1624 non-partitioned index
    static IDE_RC updateNullChildRowWithRow4Table( qcStatement           * aStatement,
                                                   qcmRefChildInfo       * aChildConstraints,
                                                   qmsTableRef           * aChildTableRef,
                                                   qcmTableInfo          * aChildTableInfo,
                                                   qdConstraintSpec      * aChildCheckConstrList,
                                                   qmsTableRef           * aDefaultTableRef,
                                                   qcmColumn             * aDefaultExprColumns,
                                                   qcmColumn             * aDefaultExprBaseColumns,
                                                   UInt                    aUpdateColCount,
                                                   UInt                  * aUpdateColumnID,
                                                   qcmColumn             * aUpdateColumn,
                                                   smiColumnList         * aUpdateColumnList,
                                                   qcmRefChildUpdateType   aUpdateRowMovement,
                                                   qcmForeignKey         * aForeignKey,
                                                   UInt                    aKeyRangeColumnCnt,
                                                   qcmIndex              * aSelectedIndex,
                                                   qcmIndex              * aUniqueConstraint,
                                                   mtcTuple              * aCheckedTuple,
                                                   const void            * aCheckedRow );

    // PROJ-1624 non-partitioned index
    static IDE_RC updateNullChildRowWithRow4Partition( qcStatement           * aStatement,
                                                       qcmRefChildInfo       * aChildConstraints,
                                                       qmsTableRef           * aChildTableRef,
                                                       qcmTableInfo          * aChildTableInfo,
                                                       qdConstraintSpec      * aChildCheckConstrList,
                                                       qmsTableRef           * aDefaultTableRef,
                                                       qcmColumn             * aDefaultExprColumns,
                                                       qcmColumn             * aDefaultExprBaseColumns,
                                                       UInt                    aUpdateColCount,
                                                       UInt                  * aUpdateColumnID,
                                                       qcmColumn             * aUpdateColumn,
                                                       qcmRefChildUpdateType   aUpdateRowMovement,
                                                       qcmForeignKey         * aForeignKey,
                                                       UInt                    aKeyRangeColumnCnt,
                                                       qcmIndex              * aSelectedIndex,
                                                       qcmIndex              * aUniqueConstraint,
                                                       mtcTuple              * aCheckedTuple,
                                                       const void            * aCheckedRow );

    // PROJ-1624 non-partitioned index
    static IDE_RC updateNullChildRowWithRow4IndexTable( qcStatement           * aStatement,
                                                        qcmRefChildInfo       * aChildConstraints,
                                                        qmsIndexTableRef      * aChildIndexTable,
                                                        qmsTableRef           * aChildTableRef,
                                                        qcmTableInfo          * aChildTableInfo,
                                                        qdConstraintSpec      * aChildCheckConstrList,
                                                        qmsTableRef           * aDefaultTableRef,
                                                        qcmColumn             * aDefaultExprColumns,
                                                        qcmColumn             * aDefaultExprBaseColumns,
                                                        UInt                    aUpdateColCount,
                                                        UInt                  * aUpdateColumnID,
                                                        qcmColumn             * aUpdateColumn,
                                                        qcmRefChildUpdateType   aUpdateRowMovement,
                                                        qcmForeignKey         * aForeignKey,
                                                        UInt                    aKeyRangeColumnCnt,
                                                        qcmIndex              * aUniqueConstraint,
                                                        mtcTuple              * aCheckedTuple,
                                                        const void            * aCheckedRow );
    
    // PROJ-1624 non-partitioned index
    static IDE_RC updateNullChildRow( qcStatement           * aStatement,
                                      qmsTableRef           * aChildTableRef,
                                      qmsPartitionRef       * aChildPartitionRef,
                                      qcmTableInfo          * aChildTableInfo,
                                      qcmForeignKey         * aForeignKey,
                                      qdConstraintSpec      * aChildCheckConstrList,
                                      UInt                    aKeyRangeColumnCnt,
                                      qcmIndex              * aUniqueConstraint,
                                      smiTableCursor        * aChildCursor,
                                      smiFetchColumnList    * aFetchColumnList,
                                      UInt                    aUpdateColumnCount,
                                      UInt                  * aUpdateColumnID,
                                      smiColumnList         * aUpdateColumnList,
                                      qcmColumn             * aUpdateColumns,
                                      qcmRefChildUpdateType   aUpdateRowMovement,
                                      smiValue              * aNullValues,
                                      smiValue              * aCheckValues,
                                      smiValue              * aInsertValues,
                                      struct qmxLobInfo     * aInsertLobInfo,
                                      mtcTuple              * aCheckedTuple,
                                      const void            * aCheckedRow,
                                      const void            * aChildRow,
                                      void                  * aChildDiskTriggerOldRow,
                                      void                  * aChildDiskTriggerNewRow,
                                      UInt                    aChildDiskRowSize,
                                      void                  * aChildMemoryTriggerOldRow,
                                      void                  * aChildMemoryTriggerNewRow,
                                      UInt                    aChildMemoryRowSize,
                                      qcmColumn             * aColumnsForChildRow,
                                      qmsIndexTableRef      * aChildIndexTable,
                                      smiTableCursor        * aSelectedIndexTableCursor,
                                      smiValue              * aIndexNullValues,
                                      qmsIndexTableCursors  * aIndexTableCursor,
                                      qmsPartRefIndexInfo   * aPartIndexInfo,
                                      smiTableCursor        * aPartCursor,
                                      scGRID                  aIndexGRID );
    
    // PROJ-1624 non-partitioned index
    static IDE_RC checkChild4UpdateNullChildRow( qcStatement      * aStatement,
                                                 qcmRefChildInfo  * aChildConstraints,
                                                 qcmTableInfo     * aChildTableInfo,
                                                 qcmForeignKey    * aForeignKey,
                                                 mtcTuple         * aCheckedTuple,
                                                 const void       * aOldRow );
};

#endif // _O_QDN_FOREIGN_KEY_H_
