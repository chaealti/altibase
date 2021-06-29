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
 *     SDEX(Shard DML EXecutor) Node
 *
 * ��� ���� :
 *
 * ��� :
 *
 **********************************************************************/

#ifndef _O_QMN_SDEX_H_
#define _O_QMN_SDEX_H_ 1

#include <mt.h>
#include <qmc.h>
#include <qmnDef.h>
#include <sdi.h>

//-----------------
// Code Node Flags
//-----------------


//-----------------
// Data Node Flags
//-----------------

/* qmndSDEX.flag                                     */
// First Initialization Done
#define QMND_SDEX_INIT_DONE_MASK           (0x00000001)
#define QMND_SDEX_INIT_DONE_FALSE          (0x00000000)
#define QMND_SDEX_INIT_DONE_TRUE           (0x00000001)

#define QMND_SDEX_ALL_FALSE_MASK           (0x00000002)
#define QMND_SDEX_ALL_FALSE_FALSE          (0x00000000)
#define QMND_SDEX_ALL_FALSE_TRUE           (0x00000002)

typedef struct qmncSDEX
{
    //---------------------------------
    // Code ���� ���� ����
    //---------------------------------

    qmnPlan            plan;
    UInt               flag;
    UInt               planID;

    //---------------------------------
    // ���� ����
    //---------------------------------

    UInt               shardDataIndex;
    UInt               shardDataOffset;
    UInt               shardDataSize;

    UInt               bindParam;    // offset
    UInt               outBindParam; // offset

    qcNamePosition     shardQuery;
    sdiAnalyzeInfo   * shardAnalysis;
    qcShardParamInfo * shardParamInfo; /* TASK-7219 */
    UShort             shardParamCount;

} qmncSDEX;

typedef struct qmndSDEX
{
    //---------------------------------
    // Data ���� ���� ����
    //---------------------------------

    qmndPlan       plan;
    doItFunc       doIt;
    UInt         * flag;

    //---------------------------------
    // lob ó���� ���� ����
    //---------------------------------
    
    struct qmxLobInfo   * lobInfo;
    UInt                  lobBindCount;

    //---------------------------------
    // ���� ����
    //---------------------------------

    sdiDataNodes * mDataInfo;

} qmndSDEX;

class qmnSDEX
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

    // �������� ���
    static IDE_RC printDataInfo( qcTemplate    * aTemplate,
                                 sdiClientInfo * aClientInfo,
                                 sdiDataNodes  * aDataInfo,
                                 ULong           aDepth,
                                 iduVarString  * aString,
                                 qmnDisplay      aMode,
                                 UInt          * aInitFlag );
    
    static void shardStmtPartialRollbackUsingSavepoint( qcTemplate  * aTemplate,
                                                        qmnPlan     * aPlan );
private:

    // ���� �ʱ�ȭ
    static IDE_RC firstInit( qcTemplate * aTemplate,
                             qmncSDEX   * aCodePlan,
                             qmndSDEX   * aDataPlan );

    static IDE_RC setParamInfo( qcTemplate      * aTemplate,
                                qmncSDEX        * aCodePlan,
                                sdiBindParam    * aBindParams,
                                sdiOutBindParam * aOutBindParams,
                                UInt            * aLobBindCount );

    /* PROJ-2728 Sharding LOB */
    static IDE_RC setLobInfo( qcTemplate   * aTemplate,
                              qmncSDEX     * aCodePlan,
                              qmxLobInfo   * aLobInfo );
};

#endif /* _O_QMN_SDEX_H_ */
