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
 * $Id: qmgPartition.h 20233 2007-02-01 01:58:21Z shsuh $
 *
 * Description :
 *     Partition Graph�� ���� ����
 *
 * ��� ���� :
 *
 * ��� :
 *
 **********************************************************************/

#ifndef _O_QMG_PARTITION_H_
#define _O_QMG_PARTITION_H_ 1

#include <qc.h>
#include <qmgDef.h>

//---------------------------------------------------
// Partition Graph�� Define ���
//---------------------------------------------------

// qmgSELT.graph.flag�� �����Ͽ� ����.
// QMG_SELT_FULL_SCAN_HINT_MASK
// QMG_SELT_NOTNULL_KEYRANGE_MASK

//---------------------------------------------------
// Partition Graph �� �����ϱ� ���� �ڷ� ����
//---------------------------------------------------

typedef struct qmgPARTITION
{
    qmgGraph          graph;    // ���� Graph ����

    qmsLimit        * limit;    // SCAN Limit ����ȭ �����, limit ���� ����

    //----------------------------------------------
    // Partition Filter Predicate ����:
    //     - partKeyRange        : partition key range ��¿�
    //     - partFilterPredicate : partition filter �����
    //----------------------------------------------

    qtcNode         * partKeyRange;
    qmoPredicate    * partFilterPredicate;

    //------------------------------------------------
    // Access Method�� ���� ����
    //     - selectedIndex : ���õ� AccessMethod�� FULL SCAN�� �ƴ� ���,
    //                       ���õ� AccessMethod Index
    //     - accessMethodCnt : �ش� Table�� index ���� + 1
    //     - accessMethod : �� accessMethod ������ Cost ����
    //------------------------------------------------
    qcmIndex        * selectedIndex;
    qmoAccessMethod * selectedMethod;  // ���õ� Access Method
    UInt              accessMethodCnt; // join�� ���� accessMethodCnt
    qmoAccessMethod * accessMethod;    // join�� ���� accessMethod

    idBool            forceIndexScan;

    qmsPartitionRef * mPrePruningPartRef; // BUG-48800 prepruning tableRef
} qmgPARTITION;

//---------------------------------------------------
// Partition Graph �� �����ϱ� ���� �Լ�
//---------------------------------------------------

class qmgPartition
{
public:
    // Graph �� �ʱ�ȭ
    static IDE_RC  init( qcStatement * aStatement,
                         qmsQuerySet * aQuerySet,
                         qmsFrom     * aFrom,
                         qmgGraph   ** aGraph);

    // Graph�� ����ȭ ����
    static IDE_RC  optimize( qcStatement * aStatement, qmgGraph * aGraph );

    // Graph�� Plan Tree ����
    static IDE_RC  makePlan( qcStatement * aStatement, const qmgGraph * aParent, qmgGraph * aGraph );

    // Graph�� ���� ������ �����.
    static IDE_RC  printGraph( qcStatement  * aStatement,
                               qmgGraph     * aGraph,
                               ULong          aDepth,
                               iduVarString * aString );

    // ���� graph�� ���� access method�� �ٲ� ���
    static IDE_RC alterSelectedIndex( qcStatement  * aStatement,
                                      qmgPARTITION * aGraph,
                                      qcmIndex     * aNewIndex );

    // PROJ-1446 Host variable�� ������ ���� ����ȭ
    // ���� JOIN graph���� ANTI�� ó���� ��
    // ���� SELT graph�� �����ϴµ� �̶� �� �Լ���
    // ���ؼ� �����ϵ��� �ؾ� �����ϴ�.
    static IDE_RC copyPARTITIONAndAlterSelectedIndex( qcStatement   * aStatement,
                                                      qmgPARTITION  * aSource,
                                                      qmgPARTITION ** aTarget,
                                                      qcmIndex      * aNewSelectedIndex,
                                                      UInt            aWhichOne );

    // push-down join predicate�� �޾Ƽ� �ڽ��� �׷����� ����.
    static IDE_RC setJoinPushDownPredicate( qcStatement   * aStatement,
                                            qmgPARTITION  * aGraph,
                                            qmoPredicate ** aPredicate );

    // push-down non-join predicate�� �޾Ƽ� �ڽ��� �׷����� ����.
    static IDE_RC setNonJoinPushDownPredicate( qcStatement   * aStatement,
                                               qmgPARTITION  * aGraph,
                                               qmoPredicate ** aPredicate );

    // Preserved Order�� direction�� �����Ѵ�.
    static IDE_RC finalizePreservedOrder( qmgGraph * aGraph );
    
private:

    // partition keyrange�� ����
    static IDE_RC extractPartKeyRange(
        qcStatement          * aStatement,
        qmsQuerySet          * aQuerySet,
        qmoPredicate        ** aPartKeyPred,
        UInt                   aPartKeyCount,
        mtcColumn            * aPartKeyColumns,
        UInt                 * aPartKeyColsFlag,
        qtcNode             ** aPartKeyRangeOrg,
        smiRange            ** aPartKeyRange );

    // PROJ-2242 �ڽ� ��Ƽ���� AccessMethodsCost �� �̿��Ͽ� cost �� ������
    static IDE_RC reviseAccessMethodsCost( qmgPARTITION   * aPartitionGraph,
                                           UInt             aPartitionCount );

    // children�� ������ rid scan���� ����
    static IDE_RC alterForceRidScan( qcStatement  * aStatement,
                                     qmgPARTITION * aGraph );
};

#endif /* _O_QMG_PARTITION_H_ */

