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
 * Description : SDSL(SharD SElect) Node
 *
 *     Shard data node �� ���� scan�� �����ϴ� Plan Node �̴�.
 *
 * ��� ���� :
 *
 * ��� :
 *
 **********************************************************************/

#ifndef _O_QMN_SDSE_H_
#define _O_QMN_SDSE_H_ 1

#include <mt.h>
#include <qmc.h>
#include <qmnDef.h>
#include <qmnShardDML.h>
#include <sdi.h>

//-----------------
// Code Node Flags
//-----------------

//-----------------
// Data Node Flags
//-----------------

// qmndSDSE.flag
// First Initialization Done
#define QMND_SDSE_INIT_DONE_MASK           (0x00000001)
#define QMND_SDSE_INIT_DONE_FALSE          (0x00000000)
#define QMND_SDSE_INIT_DONE_TRUE           (0x00000001)

#define QMND_SDSE_ALL_FALSE_MASK           (0x00000002)
#define QMND_SDSE_ALL_FALSE_FALSE          (0x00000000)
#define QMND_SDSE_ALL_FALSE_TRUE           (0x00000002)

typedef struct qmncSDSE
{
    //---------------------------------
    // ���� ����
    //---------------------------------

    qmnPlan          plan;
    UInt             flag;
    UInt             planID;

    //---------------------------------
    // ���� ����
    //---------------------------------

    qmsTableRef    * tableRef;
    UShort           tupleRowID;

    qtcNode        * constantFilter;
    qtcNode        * subqueryFilter;
    qtcNode        * nnfFilter;
    qtcNode        * filter;
    qtcNode        * lobFilter;       // Lob Filter ( BUG-25916 ) 

    UInt             shardDataIndex;
    UInt             shardDataOffset;
    UInt             shardDataSize;

    UInt             mBuffer[SDI_NODE_MAX_COUNT];  // offset
    UInt             mOffset;       // offset
    UInt             mMaxByteSize;  // offset
    UInt             mBindParam;    // offset
    UInt             mOutBindParam; // offset

    /* TASK-7219 Non-shard DML */
    UInt             mOutRefBindData; // offset

    qcNamePosition * mQueryPos;
    sdiAnalyzeInfo * mShardAnalysis;
    qcShardParamInfo * mShardParamInfo; /* TASK-7219 Non-shard DML */
    UShort           mShardParamCount;

} qmncSDSE;

typedef struct qmndSDSE
{
    //---------------------------------
    // ���� ����
    //---------------------------------

    qmndPlan       plan;
    doItFunc       doIt;
    UInt         * flag;

    UInt           lobBindCount; // PROJ-2728

    //---------------------------------
    // Disk Table ���� ����
    //---------------------------------

    void         * nullRow;  // Disk Table�� ���� null row
    scGRID         nullRID;

    //---------------------------------
    // ���� ����
    //---------------------------------

    UShort         mCurrScanNode;
    UShort         mScanDoneCount;

    sdiDataNodes * mDataInfo;

} qmndSDSE;

class qmnSDSE
{
public:

    //------------------------
    // Base Function Pointer
    //------------------------

    static IDE_RC init( qcTemplate * aTemplate,
                        qmnPlan    * aPlan );

    //------------------------
    // ���� �Լ�
    // mapping by doIt() function pointer
    //------------------------

    static IDE_RC doIt( qcTemplate * aTemplate,
                        qmnPlan    * aPlan,
                        qmcRowFlag * aFlag );

    static IDE_RC doItAllFalse( qcTemplate * aTemplate,
                                qmnPlan    * aPlan,
                                qmcRowFlag * aFlag );

    static IDE_RC doItFirst( qcTemplate * aTemplate,
                             qmnPlan    * aPlan,
                             qmcRowFlag * aFlag );

    static IDE_RC doItNext( qcTemplate * aTemplate,
                            qmnPlan    * aPlan,
                            qmcRowFlag * aFlag );

    //------------------------
    // Null Padding
    //------------------------

    static IDE_RC padNull( qcTemplate * aTemplate,
                           qmnPlan    * aPlan );

    //------------------------
    // Plan ���� ���
    //------------------------

    static IDE_RC printPlan( qcTemplate   * aTemplate,
                             qmnPlan      * aPlan,
                             ULong          aDepth,
                             iduVarString * aString,
                             qmnDisplay     aMode );

    //------------------------
    // TASK-7219 Non-shard DML
    // Transformed out ref column bind
    //------------------------

    static IDE_RC setTransformedOutRefBindValue( qcTemplate * aTemplate,
                                                 qmncSDSE   * aCodePlan );

private:

    //------------------------
    // ���� �ʱ�ȭ
    //------------------------

    static IDE_RC firstInit( qcTemplate * aTemplate,
                             qmncSDSE   * aCodePlan,
                             qmndSDSE   * aDataPlan );
    
    static IDE_RC setParamInfo( qcTemplate   * aTemplate,
                                qmncSDSE     * aCodePlan,
                                sdiBindParam * aBindParams,
                                void         * aOutRefBindData,
                                UInt         * aLobBindCount );

    static IDE_RC setLobInfo( qcTemplate   * aTemplate,
                              qmncSDSE     * aCodePlan );
};

#endif /* _O_QMN_SDSE_H_ */
