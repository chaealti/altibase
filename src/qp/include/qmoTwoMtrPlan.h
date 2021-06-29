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
 * $Id: qmoTwoMtrPlan.h 82075 2018-01-17 06:39:52Z jina.kim $
 *
 * Description :
 *     Plan Generator
 *
 *     Two-child Materialized Plan�� �����ϱ� ���� �������̴�.
 *
 *     ������ ���� Plan Node�� ������ �����Ѵ�.
 *         - SITS ���
 *         - SDIF ���
 *
 * ��� ���� :
 *
 * ��� :
 *
 **********************************************************************/

#ifndef _O_QMO_TWO_CHILD_MATER_H_
#define _O_QMO_TWO_CHILD_MATER_H_ 1

#include <qc.h>
#include <qmoDef.h>
#include <qmn.h>
#include <qmgDef.h>
#include <qmoDependency.h>
#include <qmoPredicate.h>
#include <qmoNormalForm.h>
#include <qmoSubquery.h>

//------------------------------
//SITS����� dependency�� ȣ���� ���� flag
//------------------------------
#define QMO_SITS_DEPENDENCY ( QMO_DEPENDENCY_STEP1_SET_TABLE_MAP_FALSE      | \
                              QMO_DEPENDENCY_STEP2_BASE_TABLE_FALSE         | \
                              QMO_DEPENDENCY_STEP2_DEP_WITH_MATERIALIZATION | \
                              QMO_DEPENDENCY_STEP2_SETNODE_TRUE             | \
                              QMO_DEPENDENCY_STEP3_TABLEMAP_REFINE_TRUE     | \
                              QMO_DEPENDENCY_STEP6_DEPENDENCIES_REFINE_FALSE  )


//------------------------------
//SDIF����� dependency�� ȣ���� ���� flag
//------------------------------
#define QMO_SDIF_DEPENDENCY ( QMO_DEPENDENCY_STEP1_SET_TABLE_MAP_FALSE      | \
                              QMO_DEPENDENCY_STEP2_BASE_TABLE_FALSE         | \
                              QMO_DEPENDENCY_STEP2_DEP_WITH_MATERIALIZATION | \
                              QMO_DEPENDENCY_STEP2_SETNODE_TRUE             | \
                              QMO_DEPENDENCY_STEP3_TABLEMAP_REFINE_TRUE     | \
                              QMO_DEPENDENCY_STEP6_DEPENDENCIES_REFINE_FALSE  )

//---------------------------------------------------
// Two-Child Meterialized Plan�� �����ϱ� ���� �ڷ� ����
//---------------------------------------------------

//------------------------------
//makeSITS()�Լ��� �ʿ��� flag
//------------------------------

#define QMO_MAKESITS_TEMP_TABLE_MASK             (0x00000001)
#define QMO_MAKESITS_MEMORY_TEMP_TABLE           (0x00000000)
#define QMO_MAKESITS_DISK_TEMP_TABLE             (0x00000001)

//------------------------------
//makeSDIF()�Լ��� �ʿ��� flag
//------------------------------

#define QMO_MAKESDIF_TEMP_TABLE_MASK             (0x00000001)
#define QMO_MAKESDIF_MEMORY_TEMP_TABLE           (0x00000000)
#define QMO_MAKESDIF_DISK_TEMP_TABLE             (0x00000001)

//---------------------------------------------------
// Two-Child Materialized Plan�� �����ϱ� ���� �Լ�
//---------------------------------------------------

class qmoTwoMtrPlan
{
public:

    // SITS ����� ����
    static IDE_RC    initSITS( qcStatement  * aStatement ,
                               qmsQuerySet  * aQuerySet ,
                               qmnPlan      * aParent,
                               qmnPlan     ** aPlan );

    static IDE_RC    makeSITS( qcStatement  * aStatement ,
                               qmsQuerySet  * aQuerySet ,
                               UInt           aFlag,
                               UInt           aBucketCount ,
                               qmnPlan      * aLeftChild ,
                               qmnPlan      * aRightChild ,
                               qmnPlan      * aPlan );

    // SDIF ����� ����
    static IDE_RC    initSDIF( qcStatement  * aStatement,
                               qmsQuerySet  * aQuerySet ,
                               qmnPlan      * aParent,
                               qmnPlan     ** aPlan );

    static IDE_RC    makeSDIF( qcStatement  * aStatement,
                               qmsQuerySet  * aQuerySet ,
                               UInt           aFlag,
                               UInt           aBucketCount ,
                               qmnPlan      * aLeftChild ,
                               qmnPlan      * aRightChild ,
                               qmnPlan      * aPlan );
  
private:

};

#endif /* _O_QMO_TWO_CHILD_MATER_H_ */

