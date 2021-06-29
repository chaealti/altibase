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
 * $Id: qmgDistinction.h 82075 2018-01-17 06:39:52Z jina.kim $
 *
 * Description :
 *     Distinction Graph�� ���� ����
 *
 * ��� ���� :
 *
 * ��� :
 *
 **********************************************************************/

#ifndef _O_QMG_DISTINCTION_H_
#define _O_QMG_DISTINCTION_H_ 1

#include <qc.h>
#include <qmgDef.h>

//---------------------------------------------------
// Distinction Graph�� Define ���
//---------------------------------------------------

// qmgDIST.graph.flag
#define QMG_DIST_OPT_TIP_MASK                   (0x0F000000)
#define QMG_DIST_OPT_TIP_NONE                   (0x00000000)
#define QMG_DIST_OPT_TIP_INDEXABLE_DISINCT      (0x01000000)


//---------------------------------------------------
// Distinction Graph �� �����ϱ� ���� �ڷ� ����
//---------------------------------------------------

typedef struct qmgDIST
{
    qmgGraph    graph;         // ���� Graph ����
    
    // hash based Distinction���� ������ ��쿡 ����
    UInt        hashBucketCnt; 
    
} qmgDIST;

//---------------------------------------------------
// Distinction Graph �� �����ϱ� ���� �Լ�
//---------------------------------------------------

class qmgDistinction
{
public:
    // Graph �� �ʱ�ȭ
    static IDE_RC  init( qcStatement * aStatement,
                         qmsQuerySet * aQuerySet,
                         qmgGraph    * aChildGraph, 
                         qmgGraph   ** aGraph );

    // Graph�� ����ȭ ����
    static IDE_RC  optimize( qcStatement * aStatement, qmgGraph * aGraph );

    // Graph�� Plan Tree ����
    static IDE_RC  makePlan( qcStatement * aStatement, const qmgGraph * aParent, qmgGraph * aGraph );

    // Graph�� ���� ������ �����.
    static IDE_RC  printGraph( qcStatement  * aStatement,
                               qmgGraph     * aGraph,
                               ULong          aDepth,
                               iduVarString * aString );

    static IDE_RC finalizePreservedOrder( qmgGraph * aGraph );

private:
    static IDE_RC  makeSortDistinction( qcStatement * aStatement,
                                        qmgDIST     * aMyGraph );

    static IDE_RC  makeHashDistinction( qcStatement * aStatement,
                                        qmgDIST     * aMyGraph );

    // DISTINCT Target  �÷� ������ �̿��Ͽ�Order �ڷ� ������ �����Ѵ�.
    static IDE_RC makeTargetOrder( qcStatement        * aStatement,
                                   qmsTarget          * aDistTarget,
                                   qmgPreservedOrder ** aNewTargetOrder );
                                  
    // indexable group by ����ȭ
    static IDE_RC indexableDistinct( qcStatement      * aStatement,
                                     qmgGraph         * aGraph,
                                     qmsTarget        * aDistTarget,
                                     idBool           * aIndexableDistinct );

    // Preserved Order ����� ����Ͽ��� ����� ��� ���
    static IDE_RC getCostByPrevOrder( qcStatement      * aStatement,
                                      qmgDIST          * aDistGraph,
                                      SDouble          * aAccessCost,
                                      SDouble          * aDiskCost,
                                      SDouble          * aTotalCost );
};

#endif /* _O_QMG_DISTINCTION_H_ */

