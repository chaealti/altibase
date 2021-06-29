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
 * $Id: qmnInsert.h 55241 2012-08-27 09:13:19Z linkedlist $
 *
 * Description :
 *     INST(INSerT) Node
 *
 *     ������ �𵨿��� insert�� �����ϴ� Plan Node �̴�.
 *
 * ��� ���� :
 *
 * ��� :
 *
 **********************************************************************/

#ifndef _O_QMN_INST_H_
#define _O_QMN_INST_H_ 1

#include <mt.h>
#include <qmc.h>
#include <qmnDef.h>
#include <qcmTableInfo.h>
#include <qmmParseTree.h>
#include <qmcInsertCursor.h>
#include <qmsIndexTable.h>

//-----------------
// Code Node Flags
//-----------------
#define QMNC_INST_PARTITIONED_MASK  (0x00000001)
#define QMNC_INST_PARTITIONED_FALSE (0x00000000)
#define QMNC_INST_PARTITIONED_TRUE  (0x00000001)

/* BUG-47625 Insert execution ������� */
#define QMNC_INST_COMPLEX_MASK  (0x00000002)
#define QMNC_INST_COMPLEX_FALSE (0x00000000)
#define QMNC_INST_COMPLEX_TRUE  (0x00000002)

#define QMNC_INST_EXIST_NOTNULL_COLUMN_MASK  (0x00000004)
#define QMNC_INST_EXIST_NOTNULL_COLUMN_FALSE (0x00000000)
#define QMNC_INST_EXIST_NOTNULL_COLUMN_TRUE  (0x00000004)

#define QMNC_INST_EXIST_ENCRYPT_COLUMN_MASK  (0x00000008)
#define QMNC_INST_EXIST_ENCRYPT_COLUMN_FALSE (0x00000000)
#define QMNC_INST_EXIST_ENCRYPT_COLUMN_TRUE  (0x00000008)

#define QMNC_INST_EXIST_TIMESTAMP_MASK  (0x00000010)
#define QMNC_INST_EXIST_TIMESTAMP_FALSE (0x00000000)
#define QMNC_INST_EXIST_TIMESTAMP_TRUE  (0x00000010)

#define QMNC_INST_EXIST_QUEUE_MASK  (0x00000020)
#define QMNC_INST_EXIST_QUEUE_FALSE (0x00000000)
#define QMNC_INST_EXIST_QUEUE_TRUE  (0x00000020)

//-----------------
// Data Node Flags
//-----------------

// qmndINST.flag
# define QMND_INST_INIT_DONE_MASK           (0x00000001)
# define QMND_INST_INIT_DONE_FALSE          (0x00000000)
# define QMND_INST_INIT_DONE_TRUE           (0x00000001)

// qmndINST.flag
# define QMND_INST_CURSOR_MASK              (0x00000002)
# define QMND_INST_CURSOR_CLOSED            (0x00000000)
# define QMND_INST_CURSOR_OPEN              (0x00000002)

// qmndINST.flag
# define QMND_INST_INSERT_MASK              (0x00000004)
# define QMND_INST_INSERT_FALSE             (0x00000000)
# define QMND_INST_INSERT_TRUE              (0x00000004)

// qmndINST.flag
# define QMND_INST_INDEX_CURSOR_MASK        (0x00000008)
# define QMND_INST_INDEX_CURSOR_NONE        (0x00000000)
# define QMND_INST_INDEX_CURSOR_INITED      (0x00000008)

// qmndINST.flag
# define QMND_INST_ATOMIC_MASK              (0x00000010)
# define QMND_INST_ATOMIC_FALSE             (0x00000000)
# define QMND_INST_ATOMIC_TRUE              (0x00000010)

typedef struct qmncINST  
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
    // insert ���� ����
    //---------------------------------

    idBool                isInsertSelect;
    idBool                isMultiInsertSelect;

    // update columns
    qcmColumn           * columns;
    qcmColumn           * columnsForValues;

    // update values
    qmmMultiRows        * rows;       /* BUG-42764 Multi Row */
    UInt                  valueIdx;   // update smiValues index
    UInt                  canonizedTuple;
    // PROJ-2264 Dictionary table
    UInt                  compressedTuple;
    void                * queueMsgIDSeq;

    // direct-path insert
    qmsHints            * hints;
    idBool                isAppend;
    idBool                disableTrigger;

    // sequence ����
    qcParseSeqCaches    * nextValSeqs;
    
    // instead of trigger�� ���
    idBool                insteadOfTrigger;

    // PROJ-2551 simple query ����ȭ
    idBool                isSimple;    // simple insert
    UInt                  simpleValueCount;
    qmnValueInfo        * simpleValues;
    UInt                  simpleValueBufSize;  // c-type value�� mt-type value�� ��ȯ

    // BUG-43063 insert nowait
    ULong                 lockWaitMicroSec;
    
    //---------------------------------
    // constraint ó���� ���� ����
    //---------------------------------

    qcmParentInfo       * parentConstraints;
    qdConstraintSpec    * checkConstrList;

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
    // PROJ-1090 Function-based Index
    //---------------------------------
    
    qmsTableRef         * defaultExprTableRef;
    qcmColumn           * defaultExprColumns;
    
} qmncINST;

typedef struct qmndINST
{
    //---------------------------------
    // Data ���� ���� ����
    //---------------------------------

    qmndPlan              plan;
    doItFunc              doIt;
    UInt                * flag;        

    //---------------------------------
    // INST ���� ����
    //---------------------------------

    qmcInsertCursor       insertCursorMgr;

    // PROJ-1566
    UInt                  parallelDegree;
    
    // BUG-38129
    scGRID                rowGRID;

    /* BUG-42764 Multi Row */
    qmmMultiRows        * rows;   // Current row
    //---------------------------------
    // lob ó���� ���� ����
    //---------------------------------
    
    struct qmxLobInfo   * lobInfo;

    //---------------------------------
    // Trigger ó���� ���� ����
    //---------------------------------

    qcmColumn           * columnsForRow;
    
    // PROJ-1705
    // trigger row�� �ʿ俩�� ����
    idBool                needTriggerRow;
    idBool                existTrigger;
    
    idBool                isAppend;
    //---------------------------------
    // return into ó���� ���� ����
    //---------------------------------

    // instead of trigger�� newRow�� smiValue�̹Ƿ�
    // return into�� ���� tableRow�� �ʿ��ϴ�.
    mtcTuple            * viewTuple;    
    void                * returnRow;
    
    //---------------------------------
    // index table ó���� ���� ����
    //---------------------------------

    qmsIndexTableCursors  indexTableCursorInfo;

    //---------------------------------
    // PROJ-1090 Function-based Index
    //---------------------------------
    
    void               * defaultExprRowBuffer;

} qmndINST;

class qmnINST
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
    
    // check insert reference
    static IDE_RC checkInsertRef( qcTemplate * aTemplate,
                                  qmnPlan    * aPlan );
    
    // Cursor�� Close
    static IDE_RC closeCursor( qcTemplate * aTemplate,
                               qmnPlan    * aPlan );
    
    // BUG-38129
    static IDE_RC getLastInsertedRowGRID( qcTemplate * aTemplate,
                                          qmnPlan    * aPlan,
                                          scGRID     * aRowGRID );

private:    

    //------------------------
    // �ʱ�ȭ ���� �Լ�
    //------------------------

    // ���� �ʱ�ȭ
    static IDE_RC firstInit( qcTemplate * aTemplate,
                             qmncINST   * aCodePlan,
                             qmndINST   * aDataPlan );

    // alloc trigger row
    static IDE_RC allocTriggerRow( qmncINST   * aCodePlan,
                                   qmndINST   * aDataPlan );
    
    // alloc return row
    static IDE_RC allocReturnRow( qcTemplate * aTemplate,
                                  qmncINST   * aCodePlan,
                                  qmndINST   * aDataPlan );
    
    // alloc index table cursor
    static IDE_RC allocIndexTableCursor( qcTemplate * aTemplate,
                                         qmncINST   * aCodePlan,
                                         qmndINST   * aDataPlan );
    
    // ȣ��Ǿ�� �ȵ�.
    static IDE_RC doItDefault( qcTemplate * aTemplate,
                               qmnPlan    * aPlan,
                               qmcRowFlag * aFlag );

    // ���� INST�� ����
    static IDE_RC doItFirst( qcTemplate * aTemplate,
                             qmnPlan    * aPlan,
                             qmcRowFlag * aFlag );

    // ���� INST�� ����
    static IDE_RC doItNext( qcTemplate * aTemplate,
                            qmnPlan    * aPlan,
                            qmcRowFlag * aFlag );

    // ���� INST�� ����
    static IDE_RC doItFirstMultiRows( qcTemplate * aTemplate,
                                      qmnPlan    * aPlan,
                                      qmcRowFlag * aFlag );

    // ���� INST�� ����
    static IDE_RC doItNextMultiRows( qcTemplate * aTemplate,
                                     qmnPlan    * aPlan,
                                     qmcRowFlag * aFlag );

    // ���� INST�� ����
    /* BUG-47625 Insert execution ������� */
    static IDE_RC doItOneRow( qcTemplate * aTemplate,
                              qmnPlan    * aPlan,
                              qmcRowFlag * aFlag );

    // check trigger 
    static IDE_RC checkTrigger( qcTemplate * aTemplate,
                                qmnPlan    * aPlan );
    
    // Cursor�� Open
    static IDE_RC openCursor( qcTemplate * aTemplate,
                              qmnPlan    * aPlan );

    // Insert One Record
    static IDE_RC insertOneRow( qcTemplate * aTemplate,
                                qmnPlan    * aPlan );
    
    // Insert One Record
    static IDE_RC insertOnce( qcTemplate * aTemplate,
                              qmnPlan    * aPlan );
    
    // instead of trigger
    static IDE_RC fireInsteadOfTrigger( qcTemplate * aTemplate,
                                        qmnPlan    * aPlan );

    // check parent references
    static IDE_RC checkInsertChildRefOnScan( qcTemplate           * aTemplate,
                                             qmncINST             * aCodePlan,
                                             qcmTableInfo         * aTableInfo,
                                             UShort                 aTable,
                                             qmcInsertPartCursor  * aCursorIter,
                                             void                ** aRow );
    
    // insert index table
    static IDE_RC insertIndexTableCursor( qcTemplate     * aTemplate,
                                          qmndINST       * aDataPlan,
                                          smiValue       * aInsertValue,
                                          scGRID           aRowGRID );
};

#endif /* _O_QMN_INST_H_ */
