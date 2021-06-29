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
 * $Id: qmoTwoMtrPlan.cpp 82075 2018-01-17 06:39:52Z jina.kim $
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

#include <ide.h>
#include <qmo.h>
#include <qmoTwoMtrPlan.h>

IDE_RC
qmoTwoMtrPlan::initSITS( qcStatement  * aStatement ,
                         qmsQuerySet  * aQuerySet ,
                         qmnPlan      * aParent,
                         qmnPlan     ** aPlan )
{
/***********************************************************************
 *
 * Description : SITS ��带 �����Ѵ�
 *
 * Implementation :
 *     + �ʱ�ȭ �۾�
 *         - qmncSITS�� �Ҵ� �� �ʱ�ȭ
 *     + ���� �۾�
 *         - leftDepTupleRowID , rightDepTupleID ����
 *         - SITS ����� ����
 *     + ������ �۾�
 *         - data ������ ũ�� ���
 *         - dependency�� ó��
 *         - subquery�� ó��
 *
 * TO DO
 *
 ***********************************************************************/

    qmncSITS          * sSITS;
    UInt                sDataNodeOffset;

    IDU_FIT_POINT_FATAL( "qmoTwoMtrPlan::initSITS::__FT__" );

    //----------------------------------
    // ���ռ� �˻�
    //----------------------------------

    IDE_FT_ASSERT( aStatement != NULL );
    IDE_FT_ASSERT( aQuerySet != NULL );

    //-------------------------------------------------------------
    // �ʱ�ȭ �۾�
    //-------------------------------------------------------------

    //qmncSITS�� �Ҵ� �� �ʱ�ȭ
    IDE_TEST( QC_QMP_MEM(aStatement)->alloc( ID_SIZEOF(qmncSITS) , (void **)&sSITS )
              != IDE_SUCCESS );

    QMO_INIT_PLAN_NODE( sSITS ,
                        QC_SHARED_TMPLATE(aStatement) ,
                        QMN_SITS ,
                        qmnSITS ,
                        qmndSITS,
                        sDataNodeOffset );

    *aPlan = (qmnPlan *)sSITS;

    IDE_TEST( qmc::pushResultDesc( aStatement,
                                   aQuerySet,
                                   ID_FALSE,
                                   aParent->resultDesc,
                                   &sSITS->plan.resultDesc )
              != IDE_SUCCESS );

    /* PROJ-2462 Result Cache */
    IDE_TEST( qmo::initResultCacheStack( aStatement,
                                         aQuerySet,
                                         sSITS->planID,
                                         ID_FALSE,
                                         ID_FALSE )
              != IDE_SUCCESS );
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

IDE_RC
qmoTwoMtrPlan::makeSITS( qcStatement  * aStatement ,
                         qmsQuerySet  * aQuerySet ,
                         UInt           aFlag,
                         UInt           aBucketCount ,
                         qmnPlan      * aLeftChild ,
                         qmnPlan      * aRightChild ,
                         qmnPlan      * aPlan )
{
/***********************************************************************
 *
 * Description : SITS ��带 �����Ѵ�
 *
 * Implementation :
 *     + �ʱ�ȭ �۾�
 *         - qmncSITS�� �Ҵ� �� �ʱ�ȭ
 *     + ���� �۾�
 *         - leftDepTupleRowID , rightDepTupleID ����
 *         - SITS ����� ����
 *     + ������ �۾�
 *         - data ������ ũ�� ���
 *         - dependency�� ó��
 *         - subquery�� ó��
 *
 * TO DO
 *
 ***********************************************************************/

    qmncSITS          * sSITS = (qmncSITS *)aPlan;

    UInt                sDataNodeOffset;
    qmcMtrNode        * sMtrNode[2];

    UShort              sTupleID;

    UShort              sColumnCount = 0;

    qmcMtrNode        * sNewMtrNode;
    qmcMtrNode        * sLastMtrNode = NULL;

    mtcTemplate       * sMtcTemplate;
    qtcNode             sCopiedNode;
    qmcAttrDesc       * sItrAttr;

    IDU_FIT_POINT_FATAL( "qmoTwoMtrPlan::makeSITS::__FT__" );

    //----------------------------------
    // ���ռ� �˻�
    //----------------------------------

    IDE_FT_ASSERT( aStatement != NULL );
    IDE_FT_ASSERT( aQuerySet != NULL );
    IDE_FT_ASSERT( aLeftChild != NULL );
    IDE_FT_ASSERT( aRightChild != NULL );

    //-------------------------------------------------------------
    // �ʱ�ȭ �۾�
    //-------------------------------------------------------------

    sMtcTemplate = & QC_SHARED_TMPLATE(aStatement)->tmplate;

    sSITS->plan.left     = aLeftChild;
    sSITS->plan.right    = aRightChild;

    aPlan->offset = aStatement->myPlan->sTmplate->tmplate.dataSize;
    sDataNodeOffset  = idlOS::align8(aPlan->offset +
                                     ID_SIZEOF(qmndSITS));

    sSITS->mtrNodeOffset = sDataNodeOffset;

    //----------------------------------
    // Flag ����
    //----------------------------------

    sSITS->flag = QMN_PLAN_FLAG_CLEAR;
    sSITS->plan.flag = QMN_PLAN_FLAG_CLEAR;

    //-------------------------------------------------------------
    // ���� �۾�
    //-------------------------------------------------------------

    sSITS->bucketCnt       = aBucketCount;

    //----------------------------------
    // Ʃ���� �Ҵ�
    //----------------------------------

    // To Fix PR-8060
    // Target���κ��� Tuple ID�� ȹ���ϸ� �ȵ�.
    // Target�� ������ VIEW�� ���� �������� �����Ǿ� �ִ�.
    // ����, �Ϲ� Mtr NODE�� ���������� ���ο� ID�� �Ҵ�޾�
    // ó���Ͽ��� �Ѵ�.
    // Tuple is alloced in qmvQuerySet::validate
    // sTupleID         = aQuerySet->target->targetColumn->node.table;
    // sTuple           = aStatement->tmplate->tmplate.rows + sTupleID;

    IDE_TEST( qtc::nextTable( &sTupleID , aStatement , NULL, ID_TRUE, MTC_COLUMN_NOTNULL_TRUE ) // PR-13597
              != IDE_SUCCESS );

    // To Fix PR-8493
    // �÷��� ��ü ���θ� �����ϱ� ���ؼ���
    // Tuple�� ���� ��ü ������ �̸� ����ϰ� �־�� �Ѵ�.
    if( (aFlag & QMO_MAKESITS_TEMP_TABLE_MASK) ==
        QMO_MAKESITS_MEMORY_TEMP_TABLE )
    {
        sMtcTemplate->rows[sTupleID].lflag      &= ~MTC_TUPLE_STORAGE_MASK;
        sMtcTemplate->rows[sTupleID].lflag      |= MTC_TUPLE_STORAGE_MEMORY;
    }
    else
    {
        sMtcTemplate->rows[sTupleID].lflag      &= ~MTC_TUPLE_STORAGE_MASK;
        sMtcTemplate->rows[sTupleID].lflag      |= MTC_TUPLE_STORAGE_DISK;
    }

    //----------------------------------
    // flag�� ����
    //----------------------------------

    if( QC_SHARED_TMPLATE(aStatement)->stmt == aStatement )
    {
        // �ֻ��� Query���� ���Ǵ� ���
        sSITS->flag &= ~QMNC_SITS_IN_TOP_MASK;
        sSITS->flag |= QMNC_SITS_IN_TOP_TRUE;

        // BUG-31997: When using temporary tables by RID, RID refers
        // to the invalid row.
        sSITS->plan.flag  &= ~QMN_PLAN_TEMP_FIXED_RID_MASK;
        sSITS->plan.flag  |= QMN_PLAN_TEMP_FIXED_RID_TRUE;
    }
    else
    {
        // �ֻ��� Query�� �ƴѰ��
        sSITS->flag &= ~QMNC_SITS_IN_TOP_MASK;
        sSITS->flag |= QMNC_SITS_IN_TOP_FALSE;

        // BUG-31997: When using temporary tables by RID, RID refers
        // to the invalid row.
        sSITS->plan.flag  &= ~QMN_PLAN_TEMP_FIXED_RID_MASK;
        sSITS->plan.flag  |= QMN_PLAN_TEMP_FIXED_RID_FALSE;
    }

    //----------------------------------
    // myNode�� ����
    //----------------------------------
    sSITS->myNode = NULL;

    for( sItrAttr = aPlan->left->resultDesc;
         sItrAttr != NULL;
         sItrAttr = sItrAttr->next )
    {
        sCopiedNode = *sItrAttr->expr;
        // To Fix PR-8060
        // ���� �Ҵ���� Tuple ID�� ��������
        // ���� Column�� �����Ͽ��� �Ѵ�.
        IDE_TEST( qmg::makeColumnMtrNode( aStatement ,
                                          aQuerySet ,
                                          &sCopiedNode,
                                          ID_TRUE,
                                          sTupleID ,
                                          0,
                                          &sColumnCount ,
                                          &sNewMtrNode )
                  != IDE_SUCCESS );

        sNewMtrNode->flag = QMC_MTR_INITIALIZE;
        sNewMtrNode->flag &= ~QMC_MTR_HASH_NEED_MASK;
        sNewMtrNode->flag |= QMC_MTR_HASH_NEED_TRUE;
        sNewMtrNode->flag &= ~QMC_MTR_TYPE_MASK;
        sNewMtrNode->flag |= QMC_MTR_TYPE_COPY_VALUE;

        sNewMtrNode->myDist = NULL;

        // connect
        if( sSITS->myNode == NULL )
        {
            sSITS->myNode = sNewMtrNode;
            sLastMtrNode  = sNewMtrNode;
        }
        else
        {
            sLastMtrNode->next = sNewMtrNode;
            sLastMtrNode       = sNewMtrNode;
        }
    }

    // To Fix PR-8060
    //----------------------------------
    // Tuple column�� �Ҵ�
    // mtcColumn , mtcExecute ������ ����
    //----------------------------------

    IDE_TEST( qtc::allocIntermediateTuple( aStatement,
                                           & QC_SHARED_TMPLATE(aStatement)->tmplate,
                                           sTupleID ,
                                           sColumnCount )
              != IDE_SUCCESS);

    sMtcTemplate->rows[sTupleID].lflag &= ~MTC_TUPLE_PLAN_MASK;
    sMtcTemplate->rows[sTupleID].lflag |= MTC_TUPLE_PLAN_TRUE;

    sMtcTemplate->rows[sTupleID].lflag &= ~MTC_TUPLE_PLAN_MTR_MASK;
    sMtcTemplate->rows[sTupleID].lflag |= MTC_TUPLE_PLAN_MTR_TRUE;

    //GRAPH���� ������ �����ü�� ����Ѵ�.
    if( (aFlag & QMO_MAKESITS_TEMP_TABLE_MASK) ==
        QMO_MAKESITS_MEMORY_TEMP_TABLE )
    {
        sSITS->plan.flag  &= ~QMN_PLAN_STORAGE_MASK;
        sSITS->plan.flag  |= QMN_PLAN_STORAGE_MEMORY;
        sMtcTemplate->rows[sTupleID].lflag      &= ~MTC_TUPLE_STORAGE_MASK;
        sMtcTemplate->rows[sTupleID].lflag      |= MTC_TUPLE_STORAGE_MEMORY;
    }
    else
    {
        sSITS->plan.flag  &= ~QMN_PLAN_STORAGE_MASK;
        sSITS->plan.flag  |= QMN_PLAN_STORAGE_DISK;
        sMtcTemplate->rows[sTupleID].lflag      &= ~MTC_TUPLE_STORAGE_MASK;
        sMtcTemplate->rows[sTupleID].lflag      |= MTC_TUPLE_STORAGE_DISK;
    }

    IDE_TEST( qmg::copyMtcColumnExecute( aStatement ,
                                         sSITS->myNode )
              != IDE_SUCCESS);

    //----------------------------------
    // PROJ-1473 column locate ����.
    //----------------------------------

    IDE_TEST( qmg::setColumnLocate( aStatement,
                                    sSITS->myNode )
              != IDE_SUCCESS );

    //-------------------------------------------------------------
    // ������ �۾�
    //-------------------------------------------------------------

    //data ������ ũ�� ���
    QC_SHARED_TMPLATE(aStatement)->tmplate.dataSize = sDataNodeOffset +
        sColumnCount * idlOS::align8( ID_SIZEOF(qmdMtrNode) );

    //----------------------------------
    //dependency ó�� �� subquery ó��
    //----------------------------------

    // PROJ-1358
    // Set�� ���� dependency�� �������� �ʴ´�.
    sMtrNode[0] = NULL;

    // To Fix PR-12791
    // PROJ-1358 �� ���� dependency ���� �˰����� �����Ǿ�����
    // Query Set���� ������ dependency ������ �����ؾ� ��.
    IDE_TEST( qmoDependency::setDependency( aStatement ,
                                            aQuerySet ,
                                            &sSITS->plan ,
                                            QMO_SITS_DEPENDENCY,
                                            (UShort)qtc::getPosFirstBitSet( & aQuerySet->depInfo ),
                                            NULL ,
                                            0 ,
                                            NULL ,
                                            1 ,
                                            sMtrNode )
              != IDE_SUCCESS );

    // BUG-27526
    IDE_TEST_RAISE( ( sSITS->plan.left->dependency == ID_UINT_MAX ) ||
                    ( sSITS->plan.right->dependency == ID_UINT_MAX ),
                    ERR_INVALID_DEPENDENCY );
    
    sSITS->leftDepTupleRowID  = (UShort)sSITS->plan.left->dependency;
    sSITS->rightDepTupleRowID = (UShort)sSITS->plan.right->dependency;

    /*
     * PROJ-1071 Parallel Query
     * parallel degree
     */
    if ( aLeftChild->mParallelDegree > aRightChild->mParallelDegree )
    {
        sSITS->plan.mParallelDegree = aLeftChild->mParallelDegree;
    }
    else
    {
        sSITS->plan.mParallelDegree = aRightChild->mParallelDegree;
    }
    sSITS->plan.flag &= ~QMN_PLAN_NODE_EXIST_MASK;
    sSITS->plan.flag |= ((aRightChild->flag | aLeftChild->flag) &
                         QMN_PLAN_NODE_EXIST_MASK);

    /* PROJ-2462 Result Cache */
    qmo::makeResultCacheStack( aStatement,
                               aQuerySet,
                               sSITS->planID,
                               sSITS->plan.flag,
                               sMtcTemplate->rows[sTupleID].lflag,
                               sSITS->myNode,
                               &sSITS->componentInfo,
                               ID_FALSE );

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_INVALID_DEPENDENCY )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmoOneMtrPlan::makeSITS",
                                  "Invalid dependency" ));
    }
    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

IDE_RC
qmoTwoMtrPlan::initSDIF( qcStatement  * aStatement,
                         qmsQuerySet  * aQuerySet,
                         qmnPlan      * aParent,
                         qmnPlan     ** aPlan )
{

/***********************************************************************
 *
 * Description : SDIF ��带 �����Ѵ�
 *
 * Implementation :
 *     + �ʱ�ȭ �۾�
 *         - qmncSDIF�� �Ҵ� �� �ʱ�ȭ
 *     + ���� �۾�
 *         - leftDepTupleRowID , rightDepTupleID ����
 *         - SDIF ����� ����
 *     + ������ �۾�
 *         - data ������ ũ�� ���
 *         - dependency�� ó��
 *         - subquery�� ó��
 *
 * TO DO
 *
 ***********************************************************************/

    qmncSDIF          * sSDIF;
    UInt                sDataNodeOffset;

    IDU_FIT_POINT_FATAL( "qmoTwoMtrPlan::initSDIF::__FT__" );

    //----------------------------------
    // ���ռ� �˻�
    //----------------------------------

    IDE_DASSERT( aStatement != NULL );

    //-------------------------------------------------------------
    // �ʱ�ȭ �۾�
    //-------------------------------------------------------------

    //qmncSITS�� �Ҵ� �� �ʱ�ȭ
    IDE_TEST( QC_QMP_MEM(aStatement)->alloc( ID_SIZEOF(qmncSDIF) , (void **)&sSDIF )
              != IDE_SUCCESS );

    QMO_INIT_PLAN_NODE( sSDIF ,
                        QC_SHARED_TMPLATE(aStatement) ,
                        QMN_SDIF ,
                        qmnSDIF ,
                        qmndSDIF,
                        sDataNodeOffset );

    *aPlan = (qmnPlan *)sSDIF;

    IDE_TEST( qmc::pushResultDesc( aStatement,
                                   aQuerySet,
                                   ID_FALSE,
                                   aParent->resultDesc,
                                   &sSDIF->plan.resultDesc )
              != IDE_SUCCESS );

    /* PROJ-2462 Result Cache */
    IDE_TEST( qmo::initResultCacheStack( aStatement,
                                         aQuerySet,
                                         sSDIF->planID,
                                         ID_FALSE,
                                         ID_FALSE )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

IDE_RC
qmoTwoMtrPlan::makeSDIF( qcStatement  * aStatement,
                         qmsQuerySet  * aQuerySet ,
                         UInt           aFlag,
                         UInt           aBucketCount ,
                         qmnPlan      * aLeftChild ,
                         qmnPlan      * aRightChild ,
                         qmnPlan      * aPlan )
{

/***********************************************************************
 *
 * Description : SDIF ��带 �����Ѵ�
 *
 * Implementation :
 *     + �ʱ�ȭ �۾�
 *         - qmncSDIF�� �Ҵ� �� �ʱ�ȭ
 *     + ���� �۾�
 *         - leftDepTupleRowID , rightDepTupleID ����
 *         - SDIF ����� ����
 *     + ������ �۾�
 *         - data ������ ũ�� ���
 *         - dependency�� ó��
 *         - subquery�� ó��
 *
 * TO DO
 *
 ***********************************************************************/

    qmncSDIF          * sSDIF = (qmncSDIF *)aPlan;

    UInt                sDataNodeOffset;
    qmcMtrNode        * sMtrNode[2];

    UShort              sTupleID;

    UShort              sColumnCount = 0;

    qmcMtrNode        * sNewMtrNode;
    qmcMtrNode        * sLastMtrNode = NULL;

    mtcTemplate       * sMtcTemplate;
    qtcNode             sCopiedNode;
    qmcAttrDesc       * sItrAttr;

    IDU_FIT_POINT_FATAL( "qmoTwoMtrPlan::makeSDIF::__FT__" );

    //----------------------------------
    // ���ռ� �˻�
    //----------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aQuerySet != NULL);
    IDE_DASSERT( aLeftChild != NULL );
    IDE_DASSERT( aRightChild != NULL );

    //-------------------------------------------------------------
    // �ʱ�ȭ �۾�
    //-------------------------------------------------------------

    sMtcTemplate = & QC_SHARED_TMPLATE(aStatement)->tmplate;

    sSDIF->plan.left     = aLeftChild;
    sSDIF->plan.right    = aRightChild;

    aPlan->offset = aStatement->myPlan->sTmplate->tmplate.dataSize;
    sDataNodeOffset  = idlOS::align8(aPlan->offset +
                                     ID_SIZEOF(qmndSDIF));

    sSDIF->mtrNodeOffset = sDataNodeOffset;

    //----------------------------------
    // Flag ����
    //----------------------------------

    sSDIF->flag = QMN_PLAN_FLAG_CLEAR;
    sSDIF->plan.flag = QMN_PLAN_FLAG_CLEAR;

    //-------------------------------------------------------------
    // ���� �۾�
    //-------------------------------------------------------------

    sSDIF->bucketCnt       = aBucketCount;

    //----------------------------------
    // Ʃ���� �Ҵ�
    //----------------------------------

    //----------------------------------
    // Ʃ���� �Ҵ�
    //----------------------------------

    // To Fix PR-8060
    // Target���κ��� Tuple ID�� ȹ���ϸ� �ȵ�.
    // Target�� ������ VIEW�� ���� �������� �����Ǿ� �ִ�.
    // ����, �Ϲ� Mtr NODE�� ���������� ���ο� ID�� �Ҵ�޾�
    // ó���Ͽ��� �Ѵ�.
    // Tuple is alloced in qmvQuerySet::validate
    // sTupleID         = aQuerySet->target->targetColumn->node.table;
    // sTuple           = aStatement->tmplate->tmplate.rows + sTupleID;

    IDE_TEST( qtc::nextTable( &sTupleID , aStatement , NULL, ID_TRUE, MTC_COLUMN_NOTNULL_TRUE ) // PR-13597
              != IDE_SUCCESS );

    // To Fix PR-8493
    // �÷��� ��ü ���θ� �����ϱ� ���ؼ���
    // Tuple�� ���� ��ü ������ �̸� ����ϰ� �־�� �Ѵ�.
    if( (aFlag & QMO_MAKESDIF_TEMP_TABLE_MASK) ==
        QMO_MAKESDIF_MEMORY_TEMP_TABLE )
    {
        sMtcTemplate->rows[sTupleID].lflag      &= ~MTC_TUPLE_STORAGE_MASK;
        sMtcTemplate->rows[sTupleID].lflag      |= MTC_TUPLE_STORAGE_MEMORY;
    }
    else
    {
        sMtcTemplate->rows[sTupleID].lflag      &= ~MTC_TUPLE_STORAGE_MASK;
        sMtcTemplate->rows[sTupleID].lflag      |= MTC_TUPLE_STORAGE_DISK;
    }

    //----------------------------------
    // myNode�� ����
    //----------------------------------

    sSDIF->myNode = NULL;

    for( sItrAttr = aPlan->left->resultDesc;
         sItrAttr != NULL;
         sItrAttr = sItrAttr->next )
    {
        sCopiedNode = *sItrAttr->expr;

        // To Fix PR-8060
        // ���� �Ҵ���� Tuple ID�� ��������
        // ���� Column�� �����Ͽ��� �Ѵ�.
        IDE_TEST( qmg::makeColumnMtrNode( aStatement ,
                                          aQuerySet ,
                                          &sCopiedNode,
                                          ID_TRUE,
                                          sTupleID ,
                                          0,
                                          &sColumnCount ,
                                          &sNewMtrNode )
                  != IDE_SUCCESS );

        sNewMtrNode->flag = QMC_MTR_INITIALIZE;
        sNewMtrNode->flag &= ~QMC_MTR_HASH_NEED_MASK;
        sNewMtrNode->flag |= QMC_MTR_HASH_NEED_TRUE;
        sNewMtrNode->flag &= ~QMC_MTR_TYPE_MASK;
        sNewMtrNode->flag |= QMC_MTR_TYPE_COPY_VALUE;

        sNewMtrNode->myDist = NULL;

        // connect
        if( sSDIF->myNode == NULL )
        {
            sSDIF->myNode = sNewMtrNode;
            sLastMtrNode  = sNewMtrNode;
        }
        else
        {
            sLastMtrNode->next = sNewMtrNode;
            sLastMtrNode       = sNewMtrNode;
        }
    }

    // To Fix PR-8060
    //----------------------------------
    // Tuple column�� �Ҵ�
    // mtcColumn , mtcExecute ������ ����
    //----------------------------------

    IDE_TEST( qtc::allocIntermediateTuple( aStatement,
                                           & QC_SHARED_TMPLATE(aStatement)->tmplate,
                                           sTupleID ,
                                           sColumnCount )
              != IDE_SUCCESS);

    sMtcTemplate->rows[sTupleID].lflag &= ~MTC_TUPLE_PLAN_MASK;
    sMtcTemplate->rows[sTupleID].lflag |= MTC_TUPLE_PLAN_TRUE;

    sMtcTemplate->rows[sTupleID].lflag &= ~MTC_TUPLE_PLAN_MTR_MASK;
    sMtcTemplate->rows[sTupleID].lflag |= MTC_TUPLE_PLAN_MTR_TRUE;

    //GRAPH���� ������ �����ü�� ����Ѵ�.
    if( (aFlag & QMO_MAKESDIF_TEMP_TABLE_MASK) ==
        QMO_MAKESDIF_MEMORY_TEMP_TABLE )
    {
        sSDIF->plan.flag  &= ~QMN_PLAN_STORAGE_MASK;
        sSDIF->plan.flag  |= QMN_PLAN_STORAGE_MEMORY;
        sMtcTemplate->rows[sTupleID].lflag      &= ~MTC_TUPLE_STORAGE_MASK;
        sMtcTemplate->rows[sTupleID].lflag      |= MTC_TUPLE_STORAGE_MEMORY;
    }
    else
    {
        sSDIF->plan.flag  &= ~QMN_PLAN_STORAGE_MASK;
        sSDIF->plan.flag  |= QMN_PLAN_STORAGE_DISK;
        sMtcTemplate->rows[sTupleID].lflag      &= ~MTC_TUPLE_STORAGE_MASK;
        sMtcTemplate->rows[sTupleID].lflag      |= MTC_TUPLE_STORAGE_DISK;
    }

    IDE_TEST( qmg::copyMtcColumnExecute( aStatement ,
                                         sSDIF->myNode )
              != IDE_SUCCESS);

    //----------------------------------
    // PROJ-1473 column locate ����.
    //----------------------------------

    IDE_TEST( qmg::setColumnLocate( aStatement,
                                    sSDIF->myNode )
              != IDE_SUCCESS );

    //-------------------------------------------------------------
    // ������ �۾�
    //-------------------------------------------------------------

    //data ������ ũ�� ���
    QC_SHARED_TMPLATE(aStatement)->tmplate.dataSize = sDataNodeOffset +
        sColumnCount * idlOS::align8( ID_SIZEOF(qmdMtrNode) );

    //----------------------------------
    //dependency ó�� �� subquery ó��
    //----------------------------------

    // PROJ-1358
    // SET�� ���� depedency�� �������� �ʴ´�.
    sMtrNode[0] = NULL;

    // To Fix PR-12791
    // PROJ-1358 �� ���� dependency ���� �˰����� �����Ǿ�����
    // Query Set���� ������ dependency ������ �����ؾ� ��.
    IDE_TEST( qmoDependency::setDependency( aStatement ,
                                            aQuerySet ,
                                            &sSDIF->plan ,
                                            QMO_SDIF_DEPENDENCY,
                                            (UShort)qtc::getPosFirstBitSet( & aQuerySet->depInfo ),
                                            NULL ,
                                            0 ,
                                            NULL ,
                                            1 ,
                                            sMtrNode )
              != IDE_SUCCESS );

    // BUG-27526
    IDE_TEST_RAISE( ( sSDIF->plan.left->dependency == ID_UINT_MAX ) ||
                    ( sSDIF->plan.right->dependency == ID_UINT_MAX ),
                    ERR_INVALID_DEPENDENCY );

    sSDIF->leftDepTupleRowID  = (UShort)sSDIF->plan.left->dependency;
    sSDIF->rightDepTupleRowID = (UShort)sSDIF->plan.right->dependency;

    /*
     * PROJ-1071 Parallel Query
     * parallel degree
     */
    if ( aLeftChild->mParallelDegree > aRightChild->mParallelDegree )
    {
        sSDIF->plan.mParallelDegree = aLeftChild->mParallelDegree;
    }
    else
    {
        sSDIF->plan.mParallelDegree = aRightChild->mParallelDegree;
    }
    sSDIF->plan.flag &= ~QMN_PLAN_NODE_EXIST_MASK;
    sSDIF->plan.flag |= ((aRightChild->flag | aLeftChild->flag) &
                         QMN_PLAN_NODE_EXIST_MASK);

    /* PROJ-2462 Result Cache */
    qmo::makeResultCacheStack( aStatement,
                               aQuerySet,
                               sSDIF->planID,
                               sSDIF->plan.flag,
                               sMtcTemplate->rows[sTupleID].lflag,
                               sSDIF->myNode,
                               &sSDIF->componentInfo,
                               ID_FALSE );

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_INVALID_DEPENDENCY )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmoOneMtrPlan::makeSDIF",
                                  "Invalid dependency" ));
    }
    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}
