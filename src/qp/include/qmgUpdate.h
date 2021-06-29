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
 * $Id: qmgUpdate.h 53265 2012-05-18 00:05:06Z seo0jun $
 *
 * Description :
 *     Update Graph�� ���� ����
 *
 * ��� ���� :
 *
 * ��� :
 *
 **********************************************************************/

#ifndef _O_QMG_UPDATE_H_
#define _O_QMG_UPDATE_H_ 1

#include <qc.h>
#include <qmgDef.h>

//---------------------------------------------------
// Update Graph�� Define ���
//---------------------------------------------------


//---------------------------------------------------
// Update Graph �� �����ϱ� ���� �ڷ� ����
//---------------------------------------------------

typedef struct qmgUPTE
{
    qmgGraph             graph;    // ���� Graph ����

    //---------------------------------
    // update ���� ����
    //---------------------------------

    /* PROJ-2204 JOIN UPDATE, DELETE */
    qmsTableRef        * updateTableRef;
    
    // update columns
    qcmColumn          * columns;
    smiColumnList      * updateColumnList;
    UInt               * updateColumnIDs;
    UInt                 updateColumnCount;
    
    // update values
    qmmValueNode       * values;
    qmmSubqueries      * subqueries; // subqueries in set clause
    qmmValueNode       * lists;      // lists in set clause
    UInt                 valueIdx;   // update smiValues index
    UInt                 canonizedTuple;
    // PROJ-2264 Dictionary table
    UInt                 compressedTuple;
    mtdIsNullFunc      * isNull;
    
    // sequence ����
    qcParseSeqCaches   * nextValSeqs;

    // instead of trigger
    idBool               insteadOfTrigger;

    qmoUpdateType        updateType;

    //---------------------------------
    // cursor ���� ����
    //---------------------------------
    
    smiCursorType        cursorType;
    idBool               inplaceUpdate;
    
    //---------------------------------
    // partition ���� ����
    //---------------------------------
    
    qmsTableRef        * insertTableRef;
    idBool               isRowMovementUpdate;
    
    //---------------------------------
    // Limitation ���� ����
    //---------------------------------
    
    qmsLimit           * limit;   // limit ����

    //---------------------------------
    // constraint ó���� ���� ����
    //---------------------------------
    
    qcmParentInfo      * parentConstraints;
    qcmRefChildInfo    * childConstraints;
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
    qcmColumn          * defaultExprBaseColumns;
   
    /* PROJ-2714 Multiple Update Delete support */
    qmmMultiTables     * mTableList;
} qmgUPTE;

//---------------------------------------------------
// Update Graph �� �����ϱ� ���� �Լ�
//---------------------------------------------------

class qmgUpdate
{
public:
    // Graph �� �ʱ�ȭ
    static IDE_RC  init( qcStatement * aStatement,
                         qmsQuerySet * aQuerySet,
                         qmgGraph    * aChildGraph,
                         qmgGraph   ** aGraph );

    // Graph�� ����ȭ ����
    static IDE_RC  optimize( qcStatement * aStatement, qmgGraph * aGraph );

    // Graph�� ����ȭ ����
    static IDE_RC  optimizeMultiUpdate( qcStatement * aStatement, qmgGraph * aGraph );

    // Graph�� Plan Tree ����
    static IDE_RC  makePlan( qcStatement    * aStatement,
                             const qmgGraph * aParent,
                             qmgGraph       * aGraph );

    // Graph�� ���� ������ �����.
    static IDE_RC  printGraph( qcStatement  * aStatement,
                               qmgGraph     * aGraph,
                               ULong          aDepth,
                               iduVarString * aString );
};

#endif /* _O_QMG_UPDATE_H_ */

