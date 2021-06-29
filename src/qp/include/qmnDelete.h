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
 * $Id: qmnDelete.h 55241 2012-08-27 09:13:19Z linkedlist $
 *
 * Description :
 *     DETE(DEleTE) Node
 *
 *     ������ �𵨿��� delete�� �����ϴ� Plan Node �̴�.
 *
 * ��� ���� :
 *
 * ��� :
 *
 **********************************************************************/

#ifndef _O_QMN_DETE_H_
#define _O_QMN_DETE_H_ 1

#include <mt.h>
#include <qmc.h>
#include <qmnDef.h>
#include <qcmTableInfo.h>
#include <qmmParseTree.h>
#include <qmsIndexTable.h>

//-----------------
// Code Node Flags
//-----------------

// qmncDETE.flag
// Limit�� ���������� ���� ����
# define QMNC_DETE_LIMIT_MASK               (0x00000002)
# define QMNC_DETE_LIMIT_FALSE              (0x00000000)
# define QMNC_DETE_LIMIT_TRUE               (0x00000002)

// qmncDETE.flag
// VIEW�� ���� delete���� ����
# define QMNC_DETE_VIEW_MASK                (0x00000004)
# define QMNC_DETE_VIEW_FALSE               (0x00000000)
# define QMNC_DETE_VIEW_TRUE                (0x00000004)

// qmncDETE.flag
// VIEW�� ���� update key preserved property
# define QMNC_DETE_VIEW_KEY_PRESERVED_MASK  (0x00000008)
# define QMNC_DETE_VIEW_KEY_PRESERVED_FALSE (0x00000000)
# define QMNC_DETE_VIEW_KEY_PRESERVED_TRUE  (0x00000008)

# define QMNC_DETE_PARTITIONED_MASK         (0x00000010)
# define QMNC_DETE_PARTITIONED_FALSE        (0x00000000)
# define QMNC_DETE_PARTITIONED_TRUE         (0x00000010)

# define QMNC_DETE_MULTIPLE_TABLE_MASK      (0x00000020)
# define QMNC_DETE_MULTIPLE_TABLE_FALSE     (0x00000000)
# define QMNC_DETE_MULTIPLE_TABLE_TRUE      (0x00000020)

//-----------------
// Data Node Flags
//-----------------

// qmndDETE.flag
# define QMND_DETE_INIT_DONE_MASK           (0x00000001)
# define QMND_DETE_INIT_DONE_FALSE          (0x00000000)
# define QMND_DETE_INIT_DONE_TRUE           (0x00000001)

// qmndDETE.flag
# define QMND_DETE_CURSOR_MASK              (0x00000002)
# define QMND_DETE_CURSOR_CLOSED            (0x00000000)
# define QMND_DETE_CURSOR_OPEN              (0x00000002)

// qmndDETE.flag
# define QMND_DETE_REMOVE_MASK              (0x00000004)
# define QMND_DETE_REMOVE_FALSE             (0x00000000)
# define QMND_DETE_REMOVE_TRUE              (0x00000004)

// qmndDETE.flag
# define QMND_DETE_INDEX_CURSOR_MASK        (0x00000008)
# define QMND_DETE_INDEX_CURSOR_NONE        (0x00000000)
# define QMND_DETE_INDEX_CURSOR_INITED      (0x00000008)

typedef struct qmncDETE  
{
    //---------------------------------
    // Code ���� ���� ����
    //---------------------------------

    qmnPlan               plan;
    UInt                  flag;
    UInt                  planID;

    //---------------------------------
    // querySet ���� ����
    //---------------------------------
    
    qmsTableRef         * tableRef;

    //---------------------------------
    // delete ���� ����
    //---------------------------------
    
    // instead of trigger�� ���
    idBool                insteadOfTrigger;

    // PROJ-2551 simple query ����ȭ
    idBool                isSimple;    // simple delete
    
    //---------------------------------
    // Limitation ���� ����
    //---------------------------------
    
    qmsLimit            * limit;

    //---------------------------------
    // child constraint ó���� ���� ����
    //---------------------------------

    qcmRefChildInfo     * childConstraints;

    //---------------------------------
    // return into ó���� ���� ����
    //---------------------------------
    
    /* PROJ-1584 DML Return Clause */
    qmmReturnInto       * returnInto;
    
    //---------------------------------
    // Display ���� ����
    //---------------------------------

    qmsNamePosition       tableOwnerName;     // Table Owner Name
    qmsNamePosition       tableName;          // Table Name
    qmsNamePosition       aliasName;          // Alias Name

    //---------------------------------
    // PROJ-1784 DML Without Retry
    //---------------------------------
    
    smiColumnList        * whereColumnList;
    smiColumnList       ** wherePartColumnList;
    idBool                 withoutRetry;

    /* PROJ-2714 Multiple Update Delete support */
    UInt                   mMultiTableCount;
    qmmDelMultiTables    * mTableList;
} qmncDETE;

typedef struct qmndDelMultiTables
{
    UInt                   mFlag;
    qmsIndexTableCursors   mIndexTableCursorInfo;
    qcmColumn            * mColumnsForRow;
    idBool                 mNeedTriggerRow;
    idBool                 mExistTrigger;
    smiTableCursor       * mIndexDeleteCursor;
    void                 * mOldRow;    // OLD ROW Reference�� ���� ����
} qmndDelMultiTables;

typedef struct qmndDETE
{
    //---------------------------------
    // Data ���� ���� ����
    //---------------------------------
    qmndPlan              plan;
    doItFunc              doIt;
    UInt                * flag;        

    //---------------------------------
    // DETE ���� ����
    //---------------------------------

    mtcTuple            * deleteTuple;
    UShort                deleteTupleID;
    smiTableCursor      * deleteCursor;

    /* PROJ-2464 hybrid partitioned table ���� */
    qcmTableInfo        * deletePartInfo;

    //---------------------------------
    // Limitation ���� ����
    //---------------------------------
    
    ULong                 limitCurrent;    // ���� Limit ��
    ULong                 limitStart;      // ���� Limit ��
    ULong                 limitEnd;        // ���� Limit ��

    //---------------------------------
    // Trigger ó���� ���� ����
    //---------------------------------

    qcmColumn           * columnsForRow;
    void                * oldRow;    // OLD ROW Reference�� ���� ����

    // PROJ-1705
    // trigger row�� �ʿ俩�� ����
    idBool                needTriggerRow;
    idBool                existTrigger;
    
    //---------------------------------
    // return into ó���� ���� ����
    //---------------------------------

    // instead of trigger�� oldRow�� smiValue�̹Ƿ�
    // return into�� ���� tableRow�� �ʿ��ϴ�.
    void                * returnRow;
    
    //---------------------------------
    // index table ó���� ���� ����
    //---------------------------------

    qmsIndexTableCursors  indexTableCursorInfo;

    // selection�� ���� index table cursor�� ����
    smiTableCursor      * indexDeleteCursor;
    mtcTuple            * indexDeleteTuple;

    //---------------------------------
    // PROJ-1784 DML Without Retry
    //---------------------------------

    smiDMLRetryInfo       retryInfo;

    //---------------------------------
    // PROJ-2359 Table/Partition Access Option
    //---------------------------------

    qcmAccessOption       accessOption;

    qmndDelMultiTables  * mTableArray;
} qmndDETE;

#define QMND_DETE_MULTI_TABLES( _dst_ )         \
{                                               \
    (_dst_)->mFlag                  = 0;        \
    (_dst_)->mColumnsForRow         = NULL;     \
    (_dst_)->mNeedTriggerRow        = ID_FALSE; \
    (_dst_)->mExistTrigger          = ID_FALSE; \
    (_dst_)->mIndexDeleteCursor     = NULL;     \
    (_dst_)->mOldRow                = NULL;     \
}

class qmnDETE
{
public:

    //------------------------
    // Base Function Pointer
    //------------------------

    // �ʱ�ȭ
    static IDE_RC init( qcTemplate * aTemplate,
                        qmnPlan    * aPlan );

    // ���� �Լ�
    static IDE_RC doIt( qcTemplate * aTemplate,
                        qmnPlan    * aPlan,
                        qmcRowFlag * aFlag );

    // Null Padding
    static IDE_RC padNull( qcTemplate * aTemplate,
                           qmnPlan    * aPlan );
    
    // Plan ���� ���
    static IDE_RC printPlan( qcTemplate   * aTemplate,
                             qmnPlan      * aPlan,
                             ULong          aDepth,
                             iduVarString * aString,
                             qmnDisplay     aMode );
    
    // check delete reference
    static IDE_RC checkDeleteRef( qcTemplate * aTemplate,
                                  qmnPlan    * aPlan );
    
    // Cursor�� Close
    static IDE_RC closeCursor( qcTemplate * aTemplate,
                               qmnPlan    * aPlan );

    /* BUG-39399 remove search key preserved table */
    static IDE_RC checkDuplicateDelete( qmncDETE   * aCodePlan,
                                        qmndDETE   * aDataPlan );

    // Cursor�� Close
    static IDE_RC closeCursorMultiTable( qcTemplate * aTemplate,
                                         qmnPlan    * aPlan );

    static IDE_RC checkDeleteRefMultiTable( qcTemplate        * aTemplate,
                                            qmnPlan           * aPlan,
                                            qmmDelMultiTables * aTable,
                                            UInt                aIndex );
private:

    //------------------------
    // �ʱ�ȭ ���� �Լ�
    //------------------------

    // ���� �ʱ�ȭ
    static IDE_RC firstInit( qcTemplate * aTemplate,
                             qmncDETE   * aCodePlan,
                             qmndDETE   * aDataPlan );

    // cursor info ����
    static IDE_RC allocCursorInfo( qcTemplate * aTemplate,
                                   qmncDETE   * aCodePlan,
                                   qmndDETE   * aDataPlan );
    
    // alloc trigger row
    static IDE_RC allocTriggerRow( qcTemplate * aTemplate,
                                   qmncDETE   * aCodePlan,
                                   qmndDETE   * aDataPlan );
    
    // alloc return row
    static IDE_RC allocReturnRow( qcTemplate * aTemplate,
                                  qmncDETE   * aCodePlan,
                                  qmndDETE   * aDataPlan );
    
    // alloc index table cursor
    static IDE_RC allocIndexTableCursor( qcTemplate * aTemplate,
                                         qmncDETE   * aCodePlan,
                                         qmndDETE   * aDataPlan );
    
    // ȣ��Ǿ�� �ȵ�.
    static IDE_RC doItDefault( qcTemplate * aTemplate,
                               qmnPlan    * aPlan,
                               qmcRowFlag * aFlag );

    // ���� DETE�� ����
    static IDE_RC doItFirst( qcTemplate * aTemplate,
                             qmnPlan    * aPlan,
                             qmcRowFlag * aFlag );

    // ���� DETE�� ����
    static IDE_RC doItNext( qcTemplate * aTemplate,
                            qmnPlan    * aPlan,
                            qmcRowFlag * aFlag );

    // check trigger 
    static IDE_RC checkTrigger( qcTemplate * aTemplate,
                                qmnPlan    * aPlan );
    
    // Cursor�� Get
    static IDE_RC getCursor( qcTemplate * aTemplate,
                             qmnPlan    * aPlan,
                             idBool     * aIsTableCursorChanged );

    // Delete One Record
    static IDE_RC deleteOneRow( qcTemplate * aTemplate,
                                qmnPlan    * aPlan );
    
    // instead of trigger
    static IDE_RC fireInsteadOfTrigger( qcTemplate * aTemplate,
                                        qmnPlan    * aPlan );

    // check child references
    static IDE_RC checkDeleteChildRefOnScan( qcTemplate     * aTemplate,
                                             qmncDETE       * aCodePlan,
                                             qcmTableInfo   * aTableInfo,
                                             mtcTuple       * aDeleteTuple );
    
    // update index table
    static IDE_RC deleteIndexTableCursor( qcTemplate     * aTemplate,
                                          qmncDETE       * aCodePlan,
                                          qmndDETE       * aDataPlan );

    static IDE_RC firstInitMultiTable( qcTemplate * aTemplate,
                                       qmncDETE   * aCodePlan,
                                       qmndDETE   * aDataPlan );

    static IDE_RC doItFirstMultiTable( qcTemplate * aTemplate,
                                       qmnPlan    * aPlan,
                                       qmcRowFlag * aFlag );

    // ���� DETE�� ����
    static IDE_RC doItNextMultiTable( qcTemplate * aTemplate,
                                      qmnPlan    * aPlan,
                                      qmcRowFlag * aFlag );

    static IDE_RC getCursorMultiTable( qcTemplate        * aTemplate,
                                       qmncDETE          * aCodePlan,
                                       qmndDETE          * aDataPlan,
                                       qmmDelMultiTables * aTable,
                                       idBool            * aIsSkip );

    static IDE_RC deleteOneRowMultiTable( qcTemplate        * aTemplate,
                                          qmndDETE          * aDataPlan,
                                          qmmDelMultiTables * aTable,
                                          UInt                aIndex );

    static IDE_RC checkSkipMultiTable( qcTemplate        * aTemplate,
                                       qmndDETE          * aDataPlan,
                                       qmmDelMultiTables * aTable,
                                       idBool            * aIsSkip );

    static IDE_RC deleteIndexTableCursorMultiTable( qcTemplate        * aTemplate,
                                                    qmndDETE          * aDataPlan,
                                                    qmmDelMultiTables * aTable,
                                                    UInt                aIndex );

    static IDE_RC allocIndexTableCursorMultiTable( qcTemplate * aTemplate,
                                                   qmncDETE   * aCodePlan,
                                                   qmndDETE   * aDataPlan );

    static IDE_RC allocTriggerRowMultiTable( qcTemplate * aTemplate,
                                             qmncDETE   * aCodePlan,
                                             qmndDETE   * aDataPlan );

    static IDE_RC checkTriggerMultiTable( qcTemplate        * aTemplate,
                                          qmndDETE          * aDataPlan,
                                          qmmDelMultiTables * aTable,
                                          UInt                aIndex );

    static IDE_RC fireInsteadOfTriggerMultiTable( qcTemplate        * aTemplate,
                                                  qmndDETE          * aDataPlan,
                                                  qmmDelMultiTables * aTable,
                                                  UInt                aIndex );

    static IDE_RC checkDeleteChildRefOnScanMultiTable( qcTemplate        * aTemplate,
                                                       qmmDelMultiTables * aTable,
                                                       qcmTableInfo      * aTableInfo,
                                                       mtcTuple          * aDeleteTuple );
};

#endif /* _O_QMN_DETE_H_ */
