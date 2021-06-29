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
 * $Id: qcpManager.h 88025 2020-07-13 05:35:50Z andrew.shin $
 **********************************************************************/

#ifndef _O_QCP_MANAGER_H_
#define _O_QCP_MANAGER_H_  1

#define QCP_TEST(test) if( test ) YYABORT;

#define QCP_STRUCT_ALLOC(_node_, _structType_)                                  \
    QCP_TEST(STRUCT_ALLOC(MEMORY, _structType_, &_node_) != IDE_SUCCESS)

#define QCP_STRUCT_CRALLOC(_node_, _structType_)                                \
    QCP_TEST(STRUCT_CRALLOC(MEMORY, _structType_, &_node_) != IDE_SUCCESS)

#include <iduMemory.h>
#include <qc.h>

class qcplLexer;

class qcpManager
{
public:
    static IDE_RC parseIt( qcStatement * aStatement );

    // PROJ-1988 merge query
    // stmtText���� �Ϻκ��� parsing �Ѵ�. subquery�� parse tree��
    // �����ϴµ� ���Ǹ�, lexer���� �׻� ù��° token���� TR_RETURN��
    // ��ȯ�Ͽ� get_condition_statement rule�� parsing�ȴ�.
    static IDE_RC parsePartialForSubquery( qcStatement * aStatement,
                                           SChar       * aText,
                                           SInt          aStart,
                                           SInt          aSize );
    
    // PROJ-2415 Grouping Sets Clause
    // stmtText���� �Ϻκ��� parsing �Ѵ�.
    // Grouping Sets�� Transform �� querySet �� ������ ���� ���Ǹ�,
    // lexer���� �׻� ù��° token���� TR_MODIFY�� ��ȯ�Ͽ�
    // get_queryset_statement rule�� parsing�ȴ�. 
    static IDE_RC parsePartialForQuerySet( qcStatement * aStatement,
                                           SChar       * aText,
                                           SInt          aStart,
                                           SInt          aSize );

    // PROJ-2415 Grouping Sets Clause
    // stmtText���� �Ϻκ��� parsing �Ѵ�.
    // Grouping Sets�� Transform�� OrderBy�� Node�� Target Node�� ��ȯ �ϴµ� ���Ǹ�,
    // lexer���� �׻� ù��° token���� TR_BACKUP�� ��ȯ�Ͽ�
    // get_target_list_statement rule�� parsing�ȴ�.     
    static IDE_RC parsePartialForOrderBy( qcStatement * aStatement,
                                          SChar       * aText,
                                          SInt          aStart,
                                          SInt          aSize );

    // PROJ-2638 shard table
    static IDE_RC parsePartialForAnalyze( qcStatement * aStatement,
                                          SChar       * aText,
                                          SInt          aStart,
                                          SInt          aSize );

    /* TASK-7219 */
    static IDE_RC parsePartialForWhere( qcStatement * aStatement,
                                        SChar       * aText,
                                        SInt          aStart,
                                        SInt          aSize );

private:
    static IDE_RC parseInternal( qcStatement * aStatement,
                                 qcplLexer   * aLexer );
};

#endif  /* _O_QCP_MANAGER_H_ */
