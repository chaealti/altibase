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
 * $Id: qmnMerge.h 55241 2012-08-27 09:13:19Z linkedlist $
 *
 * Description :
 *     MRGE(MeRGE) Node
 *
 *     ������ �𵨿��� merge�� �����ϴ� Plan Node �̴�.
 *
 * ��� ���� :
 *
 * ��� :
 *
 **********************************************************************/

#ifndef _O_QMN_MRGE_H_
#define _O_QMN_MRGE_H_ 1

#include <mt.h>
#include <qmc.h>
#include <qmnDef.h>
#include <qcmTableInfo.h>
#include <qmmParseTree.h>

//-----------------
// Code Node Flags
//-----------------

//-----------------
// Data Node Flags
//-----------------

// qmndMRGE.flag
# define QMND_MRGE_INIT_DONE_MASK           (0x00000001)
# define QMND_MRGE_INIT_DONE_FALSE          (0x00000000)
# define QMND_MRGE_INIT_DONE_TRUE           (0x00000001)

// qmndMRGE.flag
# define QMND_MRGE_MERGE_MASK               (0x00000002)
# define QMND_MRGE_MERGE_FALSE              (0x00000000)
# define QMND_MRGE_MERGE_TRUE               (0x00000002)

typedef struct qmncMRGE  
{
    //---------------------------------
    // Code ���� ���� ����
    //---------------------------------

    qmnPlan               plan;
    UInt                  flag;
    UInt                  planID;

    //---------------------------------
    // target ���� ����
    //---------------------------------
    
    qmsTableRef         * tableRef;

    //---------------------------------
    // merge ���� ����
    //---------------------------------

    // child statements
    qcStatement         * selectSourceStatement;
    qcStatement         * selectTargetStatement;
    
    qcStatement         * updateStatement;
    qcStatement         * deleteStatement;    
    qcStatement         * insertStatement;
    qcStatement         * insertNoRowsStatement;

    // insert where clause
    qtcNode             * whereForInsert;

    // reset plan index
    UInt                  resetPlanFlagStartIndex;
    UInt                  resetPlanFlagEndIndex;
    UInt                  resetExecInfoStartIndex;
    UInt                  resetExecInfoEndIndex;
    
    //---------------------------------
    // Display ���� ����
    //---------------------------------

    qmsNamePosition       tableOwnerName;     // Table Owner Name
    qmsNamePosition       tableName;          // Table Name
    qmsNamePosition       aliasName;          // Alias Name
    
} qmncMRGE;

typedef struct qmndMRGE
{
    //---------------------------------
    // Data ���� ���� ����
    //---------------------------------

    qmndPlan              plan;
    doItFunc              doIt;
    UInt                * flag;

    //---------------------------------
    // merge ���� ����
    //---------------------------------

    qcStatement           selectSourceStatement;
    qcStatement           selectTargetStatement;

    qcStatement           updateStatement;
    qcStatement           deleteStatement;
    qcStatement           insertStatement;
    qcStatement           insertNoRowsStatement;
    
    qmcCursor             selectSourceCursorMgr;
    qmcdTempTableMgr      selectSourceTempTableMgr;

    qmcCursor           * originalCursorMgr;
    qmcdTempTableMgr    * originalTempTableMgr;
    
    qcStatement         * originalStatement;

    // 1ȸ doIt�� merge�� row��
    vSLong                mergedCount;
    
} qmndMRGE;

class qmnMRGE
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
    
    // merged row count ��ȯ�Լ�
    static IDE_RC getMergedRowCount( qcTemplate * aTemplate,
                                     qmnPlan    * aPlan,
                                     vSLong     * aCount );

    // Cursor�� Close
    static IDE_RC closeCursor( qcTemplate * aTemplate,
                               qmnPlan    * aPlan );
    
private:    

    //------------------------
    // �ʱ�ȭ ���� �Լ�
    //------------------------

    // ���� �ʱ�ȭ
    static IDE_RC firstInit( qcTemplate * aTemplate,
                             qmncMRGE   * aCodePlan,
                             qmndMRGE   * aDataPlan );

    // ȣ��Ǿ�� �ȵ�.
    static IDE_RC doItDefault( qcTemplate * aTemplate,
                               qmnPlan    * aPlan,
                               qmcRowFlag * aFlag );

    // ���� MRGE�� ����
    static IDE_RC doItFirst( qcTemplate * aTemplate,
                             qmnPlan    * aPlan,
                             qmcRowFlag * aFlag );

    // ���� MRGE�� ����
    static IDE_RC doItNext( qcTemplate * aTemplate,
                            qmnPlan    * aPlan,
                            qmcRowFlag * aFlag );

    static IDE_RC initSource( qcTemplate * aTemplate,
                              qmnPlan    * aPlan );

    static IDE_RC readSource( qcTemplate * aTemplate,
                              qmnPlan    * aPlan,
                              qmcRowFlag * aFlag );

    static IDE_RC matchTarget( qcTemplate * aTemplate,
                               qmnPlan    * aPlan,
                               qmcRowFlag * aFlag );
    
    static IDE_RC updateTarget( qcTemplate * aTemplate,
                                qmnPlan    * aPlan,
                                vSLong     * aNumRows );

    static IDE_RC deleteTarget( qcTemplate * aTemplate,
                                qmnPlan    * aPlan );

    static IDE_RC insertTarget( qcTemplate * aTemplate,
                                qmnPlan    * aPlan );

    static IDE_RC insertNoRowsTarget( qcTemplate * aTemplate,
                                      qmnPlan    * aPlan );

    static IDE_RC checkWhereForInsert( qcTemplate * aTemplate,
                                       qmnPlan    * aPlan,
                                       idBool     * aFlag );

    static void finalize( qcTemplate * aTemplate,
                          qmnPlan    * aPlan );
    
    static void setCursorMgr( qcTemplate       * aTemplate,
                              qmcCursor        * aCursorMgr,
                              qmcdTempTableMgr * aTempTableMgr );

    /* BUG-45763 MERGE �������� CLOB Column Update �� FATAL �߻��մϴ�. */
    static IDE_RC setBindParam( qcTemplate * aTemplate,
                                qmndMRGE   * aDataPlan );
};

#endif /* _O_QMN_MRGE_H_ */
