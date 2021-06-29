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
 * $Id: qmgInsert.h 53265 2012-05-18 00:05:06Z seo0jun $
 *
 * Description :
 *     Insert Graph�� ���� ����
 *
 * ��� ���� :
 *
 * ��� :
 *
 **********************************************************************/

#ifndef _O_QMG_INSERT_H_
#define _O_QMG_INSERT_H_ 1

#include <qc.h>
#include <qmgDef.h>

//---------------------------------------------------
// Insert Graph�� Define ���
//---------------------------------------------------


//---------------------------------------------------
// Insert Graph �� �����ϱ� ���� �ڷ� ����
//---------------------------------------------------

typedef struct qmgINST
{
    qmgGraph             graph;    // ���� Graph ����

    //---------------------------------
    // insert ���� ����
    //---------------------------------
    
    struct qmsTableRef * tableRef;
    
    // insert columns
    qcmColumn          * columns;    // for INSERT ... SELECT ...
    qcmColumn          * columnsForValues;
    
    // insert values
    qmmMultiRows       * rows;       /* BUG-42764 Multi Row */
    UInt                 valueIdx;   // insert smiValues index
    UInt                 canonizedTuple;
    // PROJ-2264 Dictionary table
    UInt                 compressedTuple;
    // Proj - 1360 Queue
    void               * queueMsgIDSeq;

    // direct-path insert
    qmsHints           * hints;

    // sequence ����
    qcParseSeqCaches   * nextValSeqs;
    
    // multi-table insert
    idBool               multiInsertSelect;
    
    // instead of trigger
    idBool               insteadOfTrigger;

    // BUG-43063 insert nowait
    ULong                lockWaitMicroSec;
    
    //---------------------------------
    // constraint ó���� ���� ����
    //---------------------------------
    
    qcmParentInfo      * parentConstraints;
    qdConstraintSpec   * checkConstrList;

    //---------------------------------
    // return into ó���� ���� ����
    //---------------------------------
    
    /* PROJ-1584 DML Return Clause */
    qmmReturnInto      * returnInto;
    
    //---------------------------------
    // PROJ-1090 Function-based Index
    //---------------------------------
    
    qmsTableRef        * defaultExprTableRef;
    qcmColumn          * defaultExprColumns;
    
} qmgINST;

//---------------------------------------------------
// Insert Graph �� �����ϱ� ���� �Լ�
//---------------------------------------------------

class qmgInsert
{
public:
    // Graph �� �ʱ�ȭ
    static IDE_RC  init( qcStatement      * aStatement,
                         qmmInsParseTree  * aParseTree,
                         qmgGraph         * aChildGraph,
                         qmgGraph        ** aGraph );

    // Graph�� ����ȭ ����
    static IDE_RC  optimize( qcStatement * aStatement, qmgGraph * aGraph );

    // Graph�� Plan Tree ����
    static IDE_RC  makePlan( qcStatement    * aStatement,
                             const qmgGraph * aParent,
                             qmgGraph       * aGraph );

    // Graph�� ���� ������ �����.
    static IDE_RC  printGraph( qcStatement  * aStatement,
                               qmgGraph     * aGraph,
                               ULong          aDepth,
                               iduVarString * aString );

private:
    /* PROJ-2464 hybrid partitioned table ���� */
    static void   fixHint( qmnPlan * aPlan );
};

#endif /* _O_QMG_INSERT_H_ */

