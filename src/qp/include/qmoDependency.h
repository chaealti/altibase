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
 * $Id: qmoDependency.h 82075 2018-01-17 06:39:52Z jina.kim $
 *
 * Description :
 *     Plan Dependency Manager
 *
 *     �� Plan Node ���� �� Plan ���� ���踦 ����Ͽ�,
 *     �ش� Plan�� Dependency�� �����ϴ� ������ �����Ѵ�.
 *     �Ʒ��� ���� �� 6�ܰ��� ������ ���� Plan�� Depenendency�� �����ȴ�.
 *
 *     - 1�ܰ� : Set Tuple ID
 *     - 2�ܰ� : Dependencies ����
 *     - 3�ܰ� : Table Map ����
 *     - 4�ܰ� : Dependency ����
 *     - 5�ܰ� : Dependency ����
 *     - 6�ܰ� : Dependencies ����
 *
 *    ����)  �� Plan ��忡�� Subquery�� ������ ���,
 *           Subquery�� ���� Plan Tree ������ 2�ܰ�� 3�ܰ� ���̿���
 *           ó���Ǿ�� �Ѵ�.
 *
 * ��� ���� :
 *
 * ��� :
 *
 **********************************************************************/

#ifndef _O_QMO_PLAN_DEPENDENCY_H_
#define _O_QMO_PLAN_DEPENDENCY_H_ 1

#include <qc.h>
#include <qmoCnfMgr.h>
#include <qmoCrtPathMgr.h>

//---------------------------------------------------
// Plan Dependency �� �����ϱ� ���� �ڷ� ����
//---------------------------------------------------

// Plan Node �� ó�� �ؾ� �ϴ� dependency ����
#define QMO_DEPENDENCY_STEP1_SET_TABLE_MAP_MASK        (0x00000010) 
#define QMO_DEPENDENCY_STEP1_SET_TABLE_MAP_TRUE        (0x00000000) 
#define QMO_DEPENDENCY_STEP1_SET_TABLE_MAP_FALSE       (0x00000010) 

#define QMO_DEPENDENCY_STEP2_BASE_TABLE_MASK           (0x00000020) 
#define QMO_DEPENDENCY_STEP2_BASE_TABLE_TRUE           (0x00000000) 
#define QMO_DEPENDENCY_STEP2_BASE_TABLE_FALSE          (0x00000020) 

#define QMO_DEPENDENCY_STEP2_DEP_MASK                  (0x00000040) 
#define QMO_DEPENDENCY_STEP2_DEP_WITH_PREDICATE        (0x00000000) 
#define QMO_DEPENDENCY_STEP2_DEP_WITH_MATERIALIZATION  (0x00000040) 

#define QMO_DEPENDENCY_STEP3_TABLEMAP_REFINE_MASK      (0x00000080) 
#define QMO_DEPENDENCY_STEP3_TABLEMAP_REFINE_TRUE      (0x00000000) 
#define QMO_DEPENDENCY_STEP3_TABLEMAP_REFINE_FALSE     (0x00000080) 

#define QMO_DEPENDENCY_STEP6_DEPENDENCIES_REFINE_MASK  (0x00000100) 
#define QMO_DEPENDENCY_STEP6_DEPENDENCIES_REFINE_TRUE  (0x00000000) 
#define QMO_DEPENDENCY_STEP6_DEPENDENCIES_REFINE_FALSE (0x00000100) 

#define QMO_DEPENDENCY_STEP2_SETNODE_MASK              (0x00000200)
#define QMO_DEPENDENCY_STEP2_SETNODE_FALSE             (0x00000000)
#define QMO_DEPENDENCY_STEP2_SETNODE_TRUE              (0x00000200)

//---------------------------------------------------
// Plan Dependency �� �����ϱ� ���� �Լ�
//---------------------------------------------------

class qmoDependency
{
public:
    
    // �ش� Plan�� Dependency, Dependencies, Subquery�� Plan�� ����
    static IDE_RC    setDependency( qcStatement * aStatement,
                                    qmsQuerySet * aQuerySet,
                                    qmnPlan     * aPlan,
                                    UInt          aDependencyFlag,
                                    UShort        aTupleID,
                                    qmsTarget   * aTarget,
                                    UInt          aPredCount,
                                    qtcNode    ** aPredExpr,
                                    UInt          aMtrCount,
                                    qmcMtrNode ** aMtrNode );
    
    // Dependency ���� 1�ܰ�
    static IDE_RC    step1setTupleID( qcStatement * aStatement ,
                                      UShort        aTupleID );
    
    // Dependency ���� 2�ܰ�
    static IDE_RC    step2makeDependencies( qmnPlan     * aPlan ,
                                            UInt          aDependencyFlag,
                                            UShort        aTupleID ,
                                            qcDepInfo   * aDependencies );
    
    // Dependency ���� 3�ܰ�
    static IDE_RC    step3refineTableMap( qcStatement * aStatement ,
                                          qmnPlan     * aPlan ,
                                          qmsQuerySet * aQuerySet ,
                                          UShort        aTupleID );

    // Dependency ���� 4-5�ܰ�
    static IDE_RC    step4decideDependency( qcStatement * aStatement ,
                                            qmnPlan     * aPlan ,
                                            qmsQuerySet * aQuerySet );

    // Dependency ���� 6�ܰ�
    static IDE_RC    step6refineDependencies( qmnPlan     * aPlan ,
                                              qcDepInfo   * aDependencies );
    
private:

    // Join Order�� Dependencies�� ���� ���� ������ order�� ã��
    static IDE_RC    findRightJoinOrder( qmsSFWGH   * aSFWGH ,
                                         qcDepInfo  * aInputDependencies ,
                                         qcDepInfo  * aOutputDependencies );
    
    
};

#endif /* _O_QMO_PLAN_DEPENDENCY_H_ */

