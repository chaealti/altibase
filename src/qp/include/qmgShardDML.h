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
 * Description : ShardDML graph�� ���� ����
 *
 * ��� ���� :
 *
 * ��� :
 *
 **********************************************************************/

#ifndef _O_QMG_SHARDDML_H_
#define _O_QMG_SHARDDML_H_ 1

#include <qc.h>
#include <qmgDef.h>
#include <qmoOneNonPlan.h>
#include <sdi.h>

//---------------------------------------------------
// ShardDML graph�� Define ���
//---------------------------------------------------

#define QMG_SHARDDML_FLAG_CLEAR                  (0x00000000)

//---------------------------------------------------
// ShardDML graph �� �����ϱ� ���� �ڷ� ����
//---------------------------------------------------

typedef struct qmgSHARDDML
{
    // ���� ����
    qmgGraph           graph;

    // ���� ����
    qcNamePosition     shardQuery;
    sdiAnalyzeInfo   * shardAnalysis;
    qcShardParamInfo * shardParamInfo; /* TASK-7219 Non-shard DML */
    UShort             shardParamCount;

    UInt               flag;

} qmgSHARDDML;

//---------------------------------------------------
// ShardDML graph �� �����ϱ� ���� �Լ�
//---------------------------------------------------

class qmgShardDML
{
public:
    // Graph �� �ʱ�ȭ
    static IDE_RC init( qcStatement      * aStatement,
                        qcNamePosition   * aShardQuery,
                        sdiAnalyzeInfo   * aShardAnalysis,
                        qcShardParamInfo * aShardParamInfo, /* TASK-7219 Non-shard DML */
                        UShort             aShardParamCount,
                        qmgGraph        ** aGraph );

    // Graph�� ����ȭ ����
    static IDE_RC optimize( qcStatement * aStatement,
                            qmgGraph    * aGraph );

    // Graph�� Plan Tree ����
    static IDE_RC makePlan( qcStatement    * aStatement,
                            const qmgGraph * aParent,
                            qmgGraph       * aGraph );

    // Graph�� ���� ������ �����.
    static IDE_RC printGraph( qcStatement  * aStatement,
                              qmgGraph     * aGraph,
                              ULong          aDepth,
                              iduVarString * aString );

    // shard ������ ���
    static IDE_RC printShardInfo( qcStatement    * aStatement,
                                  sdiAnalyzeInfo * aAnalyzeInfo,
                                  qcNamePosition * aQuery,
                                  ULong            aDepth,
                                  iduVarString   * aString );

private:

    static IDE_RC printSplitInfo(  qcStatement    * aStatement,
                                   sdiAnalyzeInfo * aAnalyzeInfo,
                                   ULong            aDepth,
                                   iduVarString   * aString );

    static IDE_RC printRangeInfo( qcStatement    * aStatement,
                                  sdiAnalyzeInfo * aAnalyzeInfo,
                                  sdiClientInfo  * aClientInfo,
                                  ULong            aDepth,
                                  iduVarString   * aString );
};

#endif /* _O_QMG_SHARDDML_H_ */
