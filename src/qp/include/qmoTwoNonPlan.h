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
 * $Id: qmoTwoNonPlan.h 86088 2019-08-30 00:33:15Z ahra.cho $
 *
 * Description :
 *     Plan Generator
 *
 *     Two-child Non-materialized Plan�� �����ϱ� ���� �������̴�.
 *
 *     ������ ���� Plan Node�� ������ �����Ѵ�.
 *         - JOIN ���
 *         - MGJN ���
 *         - LOJN ���
 *         - FOJN ���
 *         - AOJN ���
 *         - CONC ���
 *         - BUNI ���
 * ��� ���� :
 *
 * ��� :
 *
 **********************************************************************/

#ifndef _O_QMO_TWO_CHILD_NON_MATER_H_
#define _O_QMO_TWO_CHILD_NON_MATER_H_ 1

#include <qc.h>
#include <qmoDef.h>
#include <qmn.h>
#include <qmgDef.h>
#include <qmoDependency.h>
#include <qmoPredicate.h>
#include <qmoNormalForm.h>
#include <qmoSubquery.h>

//------------------------------
//JOIN����� dependency�� ȣ���� ���� flag
//------------------------------
#define QMO_JOIN_DEPENDENCY ( QMO_DEPENDENCY_STEP1_SET_TABLE_MAP_FALSE     |    \
                              QMO_DEPENDENCY_STEP2_BASE_TABLE_FALSE        |    \
                              QMO_DEPENDENCY_STEP2_DEP_WITH_PREDICATE      |    \
                              QMO_DEPENDENCY_STEP2_SETNODE_FALSE           |    \
                              QMO_DEPENDENCY_STEP3_TABLEMAP_REFINE_FALSE   |    \
                              QMO_DEPENDENCY_STEP6_DEPENDENCIES_REFINE_FALSE )

//------------------------------
//MGJN����� dependency�� ȣ���� ���� flag
//------------------------------
#define QMO_MGJN_DEPENDENCY ( QMO_DEPENDENCY_STEP1_SET_TABLE_MAP_TRUE     |     \
                              QMO_DEPENDENCY_STEP2_BASE_TABLE_FALSE        |    \
                              QMO_DEPENDENCY_STEP2_DEP_WITH_PREDICATE      |    \
                              QMO_DEPENDENCY_STEP2_SETNODE_FALSE           |    \
                              QMO_DEPENDENCY_STEP3_TABLEMAP_REFINE_FALSE   |    \
                              QMO_DEPENDENCY_STEP6_DEPENDENCIES_REFINE_FALSE )

//------------------------------
//LOJN����� dependency�� ȣ���� ���� flag
//------------------------------
#define QMO_LOJN_DEPENDENCY ( QMO_DEPENDENCY_STEP1_SET_TABLE_MAP_FALSE     |    \
                              QMO_DEPENDENCY_STEP2_BASE_TABLE_FALSE        |    \
                              QMO_DEPENDENCY_STEP2_DEP_WITH_PREDICATE      |    \
                              QMO_DEPENDENCY_STEP2_SETNODE_FALSE           |    \
                              QMO_DEPENDENCY_STEP3_TABLEMAP_REFINE_FALSE   |    \
                              QMO_DEPENDENCY_STEP6_DEPENDENCIES_REFINE_FALSE )

//------------------------------
//FOJN����� dependency�� ȣ���� ���� flag
//------------------------------
#define QMO_FOJN_DEPENDENCY ( QMO_DEPENDENCY_STEP1_SET_TABLE_MAP_FALSE     |    \
                              QMO_DEPENDENCY_STEP2_BASE_TABLE_FALSE        |    \
                              QMO_DEPENDENCY_STEP2_DEP_WITH_PREDICATE      |    \
                              QMO_DEPENDENCY_STEP2_SETNODE_FALSE           |    \
                              QMO_DEPENDENCY_STEP3_TABLEMAP_REFINE_FALSE   |    \
                              QMO_DEPENDENCY_STEP6_DEPENDENCIES_REFINE_FALSE )

//------------------------------
//AOJN����� dependency�� ȣ���� ���� flag
//------------------------------
#define QMO_AOJN_DEPENDENCY ( QMO_DEPENDENCY_STEP1_SET_TABLE_MAP_FALSE     |    \
                              QMO_DEPENDENCY_STEP2_BASE_TABLE_FALSE        |    \
                              QMO_DEPENDENCY_STEP2_DEP_WITH_PREDICATE      |    \
                              QMO_DEPENDENCY_STEP2_SETNODE_FALSE           |    \
                              QMO_DEPENDENCY_STEP3_TABLEMAP_REFINE_FALSE   |    \
                              QMO_DEPENDENCY_STEP6_DEPENDENCIES_REFINE_FALSE )

//------------------------------
//CONC����� dependency�� ȣ���� ���� flag
//------------------------------
#define QMO_CONC_DEPENDENCY ( QMO_DEPENDENCY_STEP1_SET_TABLE_MAP_FALSE     |    \
                              QMO_DEPENDENCY_STEP2_BASE_TABLE_FALSE        |    \
                              QMO_DEPENDENCY_STEP2_DEP_WITH_PREDICATE      |    \
                              QMO_DEPENDENCY_STEP2_SETNODE_FALSE           |    \
                              QMO_DEPENDENCY_STEP3_TABLEMAP_REFINE_FALSE   |    \
                              QMO_DEPENDENCY_STEP6_DEPENDENCIES_REFINE_FALSE )

//------------------------------
//BUNI����� dependency�� ȣ���� ���� flag
//------------------------------
#define QMO_BUNI_DEPENDENCY ( QMO_DEPENDENCY_STEP1_SET_TABLE_MAP_FALSE     |    \
                              QMO_DEPENDENCY_STEP2_BASE_TABLE_FALSE        |    \
                              QMO_DEPENDENCY_STEP2_DEP_WITH_PREDICATE      |    \
                              QMO_DEPENDENCY_STEP2_SETNODE_TRUE            |    \
                              QMO_DEPENDENCY_STEP3_TABLEMAP_REFINE_FALSE   |    \
                              QMO_DEPENDENCY_STEP6_DEPENDENCIES_REFINE_FALSE )

// PROJ-2582 recursive with
//------------------------------
// SREC����� dependency�� ȣ���� ���� flag
//------------------------------
#define QMO_SREC_DEPENDENCY ( QMO_DEPENDENCY_STEP1_SET_TABLE_MAP_FALSE     |    \
                              QMO_DEPENDENCY_STEP2_BASE_TABLE_FALSE        |    \
                              QMO_DEPENDENCY_STEP2_DEP_WITH_PREDICATE      |    \
                              QMO_DEPENDENCY_STEP2_SETNODE_TRUE            |    \
                              QMO_DEPENDENCY_STEP3_TABLEMAP_REFINE_FALSE   |    \
                              QMO_DEPENDENCY_STEP6_DEPENDENCIES_REFINE_FALSE )

//---------------------------------------------------
// Two-Child Non-Meterialized Plan�� �����ϱ� ���� �ڷ� ����
//---------------------------------------------------


//---------------------------------------------------
// Two-Child Non-Materialized Plan�� �����ϱ� ���� �Լ�
//---------------------------------------------------

class qmoTwoNonPlan
{
public:

    // JOIN ����� ����
    static IDE_RC    initJOIN( qcStatement   * aStatement,
                               qmsQuerySet   * aQuerySet,
                               qmsQuerySet   * aViewQuerySet,
                               qtcNode       * aJoinPredicate,
                               qtcNode       * aFilterPredicate,
                               qmoPredicate  * aPredicate,
                               qmnPlan       * aParent,
                               qmnPlan      ** aPlan );

    static IDE_RC    makeJOIN( qcStatement  * aStatement ,
                               qmsQuerySet  * aQuerySet ,
                               UInt           aGraphType ,
                               qmnPlan      * aLeftChild ,
                               qmnPlan      * aRightChild ,
                               qmnPlan      * aPlan );

    // MGJN ����� ����
    static IDE_RC    initMGJN( qcStatement   * aStatement ,
                               qmsQuerySet   * aQuerySet ,
                               qmoPredicate  * aJoinPredicate ,
                               qmoPredicate  * aFilterPredicate ,
                               qmnPlan       * aParent,
                               qmnPlan      ** aPlan );

    static IDE_RC    makeMGJN( qcStatement  * aStatement ,
                               qmsQuerySet  * aQuerySet ,
                               UInt           aGraphType ,
                               qmoPredicate * aJoinPredicate ,
                               qmoPredicate * aFilterPredicate ,
                               qmnPlan      * aLeftChild ,
                               qmnPlan      * aRightChild ,
                               qmnPlan      * aPlan );

    // LOJN ����� ����
    static IDE_RC    initLOJN( qcStatement   * aStatement ,
                               qmsQuerySet   * aQuerySet ,
                               qmsQuerySet   * aViewQuerySet,
                               qtcNode       * aJoinPredicate,
                               qtcNode       * aFilter,
                               qmoPredicate  * aPredicate,
                               qmnPlan       * aParent ,
                               qmnPlan      ** aPlan );

    static IDE_RC    makeLOJN( qcStatement  * aStatement ,
                                qmsQuerySet  * aQuerySet ,
                                qtcNode      * aFilter ,
                                qmnPlan      * aLeftChild ,
                                qmnPlan      * aRightChild ,
                                qmnPlan      * aPlan );

    // FOJN ����� ����
    static IDE_RC    initFOJN( qcStatement   * aStatement ,
                               qmsQuerySet   * aQuerySet ,
                               qtcNode       * aJoinPredicate,
                               qtcNode       * aFilter ,
                               qmoPredicate  * aPredicate,
                               qmnPlan       * aParent,
                               qmnPlan      ** aPlan );

    static IDE_RC    makeFOJN( qcStatement  * aStatement ,
                               qmsQuerySet  * aQuerySet ,
                               qtcNode      * aFilter ,
                               qmnPlan      * aLeftChild ,
                               qmnPlan      * aRightChild ,
                               qmnPlan      * aPlan );

    // AOJN ����� ����
    static IDE_RC    initAOJN( qcStatement   * aStatement ,
                               qmsQuerySet   * aQuerySet ,
                               qtcNode       * aFilter ,
                               qmoPredicate  * aPredicate,
                               qmnPlan       * aParent,
                               qmnPlan      ** aPlan );

    static IDE_RC    makeAOJN( qcStatement  * aStatement ,
                               qmsQuerySet  * aQuerySet ,
                               qtcNode      * aFilter ,
                               qmnPlan      * aLeftChild ,
                               qmnPlan      * aRightChild ,
                               qmnPlan      * aPlan );

    // CONC ����� ����
    static IDE_RC    initCONC( qcStatement  * aStatement ,
                               qmsQuerySet  * aQuerySet ,
                               qmnPlan      * aParent,
                               qcDepInfo    * aDepInfo,
                               qmnPlan     ** aPlan );

    static IDE_RC    makeCONC( qcStatement  * aStatement ,
                               qmsQuerySet  * aQuerySet ,
                               qmnPlan      * aLeftChild ,
                               qmnPlan      * aRightChild ,
                               qmnPlan      * aPlan );

    // BUNI ����� ����
    static IDE_RC    initBUNI( qcStatement  * aStatement ,
                               qmsQuerySet  * aQuerySet ,
                               qmnPlan      * aParent,
                               qmnPlan     ** aPlan );

    static IDE_RC    makeBUNI( qcStatement  * aStatement ,
                               qmsQuerySet  * aQuerySet ,
                               qmnPlan      * aLeftChild ,
                               qmnPlan      * aRightChild ,
                               qmnPlan      * aPlan );
    
    // PROJ-2582 recursive with
    // SREC ����� ����
    static IDE_RC    initSREC( qcStatement  * aStatement ,
                               qmsQuerySet  * aQuerySet ,
                               qmnPlan      * aParent,
                               qmnPlan     ** aPlan );
    
    static IDE_RC    makeSREC( qcStatement  * aStatement ,
                               qmsQuerySet  * aQuerySet ,
                               qmnPlan      * aLeftChild ,
                               qmnPlan      * aRightChild ,
                               qmnPlan      * aRecursiveChild ,
                               qmnPlan      * aPlan );
    
private:

    // PROJ-2551 simple query ����ȭ
    static IDE_RC checkSimpleJOIN( qcStatement  * aStatement,
                                   qmncJOIN     * aJOIN );
};

#endif /* _O_QMO_TWO_CHILD_NON_MATER_H_ */
