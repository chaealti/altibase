/**
 *  Copyright (c) 1999~2017, Altibase Corp. and/or its affiliates. All rights reserved.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License, version 3,
 *  as published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _O_ULN_ANALYZE_STMT_H_
#define _O_ULN_ANALYZE_STMT_H_ 1

#include <ulnPrivate.h>

/**
 *  ulnAnalyzeStmtCreate
 *
 *  @aAnalyzeStmt : ulnAnalyzeStmt�� ��ü
 *  @aStmtStr     : Statement ��Ʈ��
 *  @aStmtStrLen  : Statement ����
 *
 *  = Statement�� �м��� ��ū List�� �����Ѵ�.
 *  = �����ϸ� ACI_SUCCESS, �����ϸ� ACI_FAILURE
 */
ACI_RC ulnAnalyzeStmtCreate(ulnAnalyzeStmt **aAnalyzeStmt,
                            acp_char_t      *aStmtStr,
                            acp_sint32_t     aStmtStrLen);

/**
 *  ulnAnalyzeStmtReInit
 *
 *  = ������ �Ҵ�� �޸𸮸� �̿��� Statement�� �м��ϰ�
 *    ��ū List�� �����Ѵ�.
 */
ACI_RC ulnAnalyzeStmtReInit(ulnAnalyzeStmt **aAnalyzeStmt,
                            acp_char_t      *aStmtStr,
                            acp_sint32_t     aStmtStrLen);

/**
 *  ulnAnalyzeStmtDestroy
 *
 *  @aAnalyzeStmt : ulnAnalyzeStmt�� Object
 *
 *  = ulnAnalyzeStmt ��ü�� �Ҹ��Ѵ�.
 */
void ulnAnalyzeStmtDestroy(ulnAnalyzeStmt **aAnalyzeStmt);

/**
 *  ulnAnalyzeStmtGetPosArr
 *
 *  @aAnalyzeStmt : ulnAnalyzeStmt�� ��ü
 *  @aToken  : ã�� ��ū ��Ʈ��
 *  @aPosArr : Statement �� aToken�� �������� ������ �迭
 *  @aPosCnt : ������ ����
 *
 *  = aToken�� PosArr�� ������ �� �迭�� �������� ��ȯ�� �ش�.
 *  = aToken�� PosArr�� ���ų� Statement�� :name, ?�� ȥ��Ǿ� ������
 *    NULL�� 0�� ��ȯ�� �ش�. �� Position ���ε��� �Ѵ�.
 *  = �����ϸ� ACI_SUCCESS, �����ϸ� ACI_FAILURE
 */
ACI_RC ulnAnalyzeStmtGetPosArr(ulnAnalyzeStmt  *aAnalyzeStmt,
                               acp_char_t      *aToken,
                               acp_uint16_t   **aPosArr,
                               acp_uint16_t    *aPosCnt);

#endif /* _O_ULN_ANALYZE_STMT_H_ */
