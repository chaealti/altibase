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
 * $Id$
 *
 * Description :
 *     Shard Insert Graph�� ���� ����
 *
 * ��� ���� :
 *
 * ��� :
 *
 **********************************************************************/

#ifndef _O_QMG_SHARD_INSERT_H_
#define _O_QMG_SHARD_INSERT_H_ 1

#include <qc.h>
#include <qmgDef.h>

//---------------------------------------------------
// Shard Insert Graph�� Define ���
//---------------------------------------------------


//---------------------------------------------------
// Shard Insert Graph �� �����ϱ� ���� �ڷ� ����
//---------------------------------------------------

typedef struct qmgShardINST
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
    // Proj - 1360 Queue
    void               * queueMsgIDSeq;

    // sequence ����
    qcParseSeqCaches   * nextValSeqs;

    //---------------------------------
    // ���� ����
    //---------------------------------

    qcNamePosition       insertPos;
    qcNamePosition       shardQuery;
    SChar                shardQueryBuf[1024];
    sdiAnalyzeInfo       shardAnalysis;

} qmgShardINST;

//---------------------------------------------------
// Shard Insert Graph �� �����ϱ� ���� �Լ�
//---------------------------------------------------

class qmgShardInsert
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
};

#endif /* _O_QMG_SHARD_INSERT_H_ */

