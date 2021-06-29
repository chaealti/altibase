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
 * $Id: qmnMergeJoin.cpp 82075 2018-01-17 06:39:52Z jina.kim $
 *
 * Description :
 *     MGJN(MerGe JoiN) Node
 *
 *     ������ �𵨿��� Merge Join ������ �����ϴ� Plan Node �̴�.
 *
 *     Join Method�� Merge Join���� ����ϸ�, Left�� Right Child�δ�
 *     ������ ���� Plan Node���� ������ �� �ִ�.
 *
 *         - SCAN Node
 *         - SORT Node
 *         - MGJN Node
 *
 * ��� ���� :
 *
 * ��� :
 *
 **********************************************************************/

#include <ide.h>
#include <qcuProperty.h>
#include <qmoUtil.h>
#include <qmnScan.h>
#include <qmnPartitionCoord.h>
#include <qmnSort.h>
#include <qmnMergeJoin.h>
#include <qcg.h>

extern mtfModule mtfLessThan;
extern mtfModule mtfLessEqual;

IDE_RC
qmnMGJN::init( qcTemplate * aTemplate,
               qmnPlan    * aPlan )
{
/***********************************************************************
 *
 * Description :
 *    MGJN ����� �ʱ�ȭ
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qmnMGJN::init"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qmncMGJN * sCodePlan = (qmncMGJN *) aPlan;
    qmndMGJN * sDataPlan =
        (qmndMGJN *) (aTemplate->tmplate.data + aPlan->offset);
    sDataPlan->flag = & aTemplate->planFlag[sCodePlan->planID];

    sDataPlan->doIt = qmnMGJN::doItDefault;

    // first initialization
    if ( (*sDataPlan->flag & QMND_MGJN_INIT_DONE_MASK)
         == QMND_MGJN_INIT_DONE_FALSE )
    {
        IDE_TEST( firstInit(aTemplate, sCodePlan, sDataPlan) != IDE_SUCCESS );
    }
    else
    {
        // Nothing To Do
    }

    //------------------------------------------------
    // Child Plan�� �ʱ�ȭ
    //------------------------------------------------

    IDE_TEST( aPlan->left->init( aTemplate,
                                 aPlan->left ) != IDE_SUCCESS);

    IDE_TEST( aPlan->right->init( aTemplate,
                                  aPlan->right ) != IDE_SUCCESS);

    //------------------------------------------------
    // ���� Cursor�� ������ ǥ��
    //------------------------------------------------

    *sDataPlan->flag &= ~QMND_MGJN_CURSOR_STORED_MASK;
    *sDataPlan->flag |= QMND_MGJN_CURSOR_STORED_FALSE;

    //------------------------------------------------
    // ���� �Լ� ����
    //------------------------------------------------

    sDataPlan->doIt = qmnMGJN::doItFirst;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qmnMGJN::doIt( qcTemplate * aTemplate,
               qmnPlan    * aPlan,
               qmcRowFlag * aFlag )
{
    qmncMGJN * sCodePlan = (qmncMGJN *) aPlan;

    switch( sCodePlan->flag & QMNC_MGJN_TYPE_MASK )
    {
        case QMNC_MGJN_TYPE_INNER:
            IDE_TEST( doItInner( aTemplate, aPlan, aFlag ) != IDE_SUCCESS );
            break;
        case QMNC_MGJN_TYPE_SEMI:
            IDE_TEST( doItSemi( aTemplate, aPlan, aFlag ) != IDE_SUCCESS );
            break;
        case QMNC_MGJN_TYPE_ANTI:
            IDE_TEST( doItAnti( aTemplate, aPlan, aFlag ) != IDE_SUCCESS );
            break;
        default:
            IDE_ERROR( 0 );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmnMGJN::doItInner( qcTemplate * aTemplate,
                    qmnPlan    * aPlan,
                    qmcRowFlag * aFlag )
{
/***********************************************************************
 *
 * Description :
 *    MGJN�� ���� ����� �����Ѵ�.
 *
 * Implementation :
 *    1.������ �Լ� �����͸� �����Ѵ�.
 *    2.row�� �����ϴ°�� filter������ �˻��Ѵ�.
 *    2.1.row�� �������� �ʴ� ��� �Ϸ�.
 *    3.filter������ �˻��Ѵ�.
 *    3.1 filter������ �´� ��� �Ϸ�.
 *    3.2 filter������ ���� �ʴ� ��� 1��
 ***********************************************************************/

    qmncMGJN * sCodePlan = (qmncMGJN *) aPlan;
    qmndMGJN * sDataPlan =
        (qmndMGJN *) (aTemplate->tmplate.data + aPlan->offset);

    idBool     sJudge = ID_FALSE;

    while( sJudge == ID_FALSE )
    {
        // Merge Join Predicate ���� �˻��� �´� ���ڵ带 �ø�
        IDE_TEST( sDataPlan->doIt( aTemplate, aPlan, aFlag ) != IDE_SUCCESS );

        // ���� �´� ���ڵ尡 �ö����
        if ( (*aFlag & QMC_ROW_DATA_MASK) == QMC_ROW_DATA_EXIST )
        {
            // 
            // Join Filter ���� �˻�
            IDE_TEST( checkJoinFilter( aTemplate,
                                       sCodePlan,
                                       & sJudge )
                      != IDE_SUCCESS );

            // Join Filter���ǿ� ������ sJudge�� TRUE�� �ǰ� ������ ��������
        }
        else
        {
            // �´� ����� ������ break
            break;
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmnMGJN::padNull( qcTemplate * aTemplate,
                  qmnPlan    * aPlan )
{
/***********************************************************************
 *
 * Description :
 *    Null Padding�� �����Ѵ�.
 *
 * Implementation :
 *    ������ Null Row�� ������ ������,
 *    Child�� ���� Null Padding�� �����Ѵ�.
 *
 ***********************************************************************/

#define IDE_FN "qmnMGJN::padNull"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qmncMGJN * sCodePlan = (qmncMGJN *) aPlan;
    // qmndMGJN * sDataPlan =
    //     (qmndMGJN *) (aTemplate->tmplate.data + aPlan->offset);

    // first initialization
    if ( (aTemplate->planFlag[sCodePlan->planID] & QMND_MGJN_INIT_DONE_MASK)
         == QMND_MGJN_INIT_DONE_FALSE )
    {
        IDE_TEST( aPlan->init( aTemplate, aPlan ) != IDE_SUCCESS );
    }
    else
    {
        // Nothing To Do
    }

    //------------------------------------------------
    // Child �� ���� Null Padding
    //------------------------------------------------

    IDE_TEST( aPlan->left->padNull( aTemplate, aPlan->left )
              != IDE_SUCCESS );

    IDE_TEST( aPlan->right->padNull( aTemplate, aPlan->right )
              != IDE_SUCCESS );


    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}


IDE_RC
qmnMGJN::printPlan( qcTemplate   * aTemplate,
                    qmnPlan      * aPlan,
                    ULong          aDepth,
                    iduVarString * aString,
                    qmnDisplay     aMode )
{
/***********************************************************************
 *
 * Description :
 *    ���� ������ ����Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qmnMGJN::printPlan"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qmncMGJN * sCodePlan = (qmncMGJN *) aPlan;
    qmndMGJN * sDataPlan =
        (qmndMGJN *) (aTemplate->tmplate.data + aPlan->offset);
    sDataPlan->flag = & aTemplate->planFlag[sCodePlan->planID];

    ULong i;

    //----------------------------
    // Display ��ġ ����
    //----------------------------

    for ( i = 0; i < aDepth; i++ )
    {
        iduVarStringAppend( aString,
                            " " );
    }

    //----------------------------
    // MGJN ��� ǥ��
    //----------------------------

    switch( sCodePlan->flag & QMNC_MGJN_TYPE_MASK )
    {
        case QMNC_MGJN_TYPE_INNER:
            (void) iduVarStringAppend( aString, "MERGE-JOIN ( " );
            break;
        case QMNC_MGJN_TYPE_SEMI:
            (void) iduVarStringAppend( aString, "SEMI-MERGE-JOIN ( " );
            break;
        case QMNC_MGJN_TYPE_ANTI:
            (void) iduVarStringAppend( aString, "ANTI-MERGE-JOIN ( " );
            break;
        default:
            IDE_DASSERT( 0 );
    }

    //----------------------------
    // Join Method ���
    //----------------------------
    qmn::printJoinMethod( aString, sCodePlan->plan.flag );

    //----------------------------
    // Cost ���
    //----------------------------
    qmn::printCost( aString,
                    sCodePlan->plan.qmgAllCost );

    //----------------------------
    // Predicate ������ �� ���
    //----------------------------

    if (QCG_GET_SESSION_TRCLOG_DETAIL_PREDICATE(aTemplate->stmt) == 1)
    {
        // Key Range ���� ���
        for ( i = 0; i < aDepth; i++ )
        {
            iduVarStringAppend( aString,
                                " " );
        }
        iduVarStringAppend( aString,
                            " [ VARIABLE KEY ]\n" );
        IDE_TEST(qmoUtil::printPredInPlan(aTemplate,
                                          aString,
                                          aDepth + 1,
                                          sCodePlan->mergeJoinPred )
                 != IDE_SUCCESS);

        // Filter ���� ���
        if ( sCodePlan->joinFilter != NULL)
        {
            for ( i = 0; i < aDepth; i++ )
            {
                iduVarStringAppend( aString,
                                    " " );
            }
            iduVarStringAppend( aString,
                                " [ FILTER ]\n" );
            IDE_TEST(qmoUtil::printPredInPlan(aTemplate,
                                              aString,
                                              aDepth + 1,
                                              sCodePlan->joinFilter)
                     != IDE_SUCCESS);
        }
        else
        {
            // Nothing To Do
        }
    }
    else
    {
        // Nothing To Do
    }

    //----------------------------
    // Subquery ������ ���
    // Subquery�� ������ ���� predicate���� ������ �� �ִ�.
    //     1. Merge Join Predicate
    //     2. Join Filter
    //----------------------------

    IDE_TEST( qmn::printSubqueryPlan( aTemplate,
                                      sCodePlan->mergeJoinPred,
                                      aDepth,
                                      aString,
                                      aMode ) != IDE_SUCCESS );

    if ( sCodePlan->joinFilter != NULL )
    {
        IDE_TEST( qmn::printSubqueryPlan( aTemplate,
                                          sCodePlan->joinFilter,
                                          aDepth,
                                          aString,
                                          aMode ) != IDE_SUCCESS );
    }
    else
    {
        // Nothing To Do
    }

    //----------------------------
    // PROJ-1473 mtrNode info 
    //----------------------------

    if ( QCU_TRCLOG_DETAIL_MTRNODE == 1 )
    {
        qmn::printMTRinfo( aString,
                           aDepth,
                           sCodePlan->myNode,
                           "myNode",
                           ID_USHORT_MAX,
                           ID_USHORT_MAX,
                           ID_USHORT_MAX );
    }

    //----------------------------
    // Operator�� ��� ���� ���
    //----------------------------
    if ( QCU_TRCLOG_RESULT_DESC == 1 )
    {
        IDE_TEST( qmn::printResult( aTemplate,
                                    aDepth,
                                    aString,
                                    sCodePlan->plan.resultDesc )
                  != IDE_SUCCESS );
    }
    else
    {
        // Nothing to do.
    }

    //----------------------------
    // Child Plan�� ���� ���
    //----------------------------

    IDE_TEST( aPlan->left->printPlan( aTemplate,
                                      aPlan->left,
                                      aDepth + 1,
                                      aString,
                                      aMode ) != IDE_SUCCESS );

    IDE_TEST( aPlan->right->printPlan( aTemplate,
                                       aPlan->right,
                                       aDepth + 1,
                                       aString,
                                       aMode ) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qmnMGJN::doItDefault( qcTemplate * /* aTemplate */,
                      qmnPlan    * /* aPlan */,
                      qmcRowFlag * /* aFlag */)
{
/***********************************************************************
 *
 * Description :
 *    �� �Լ��� ����Ǹ� �ȵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qmnMGJN::doItDefault"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    IDE_DASSERT( 0 );

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qmnMGJN::doItFirst( qcTemplate * aTemplate,
                    qmnPlan    * aPlan,
                    qmcRowFlag * aFlag )
{
/***********************************************************************
 *
 * Description :
 *    ���� ���� �Լ�
 *
 * Implementation :
 *     Left Child �Ǵ� Right Child�� ����� ��� �ִ� ��쿡�� ����
 *
 ***********************************************************************/

#define IDE_FN "qmnMGJN::doItFirst"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qmncMGJN * sCodePlan = (qmncMGJN *) aPlan;
    qmndMGJN * sDataPlan =
        (qmndMGJN *) (aTemplate->tmplate.data + aPlan->offset);

    qmcRowFlag sFlag     = QMC_ROW_INITIALIZE;

    //----------------------------
    // Left Child�� ����
    //----------------------------

    IDE_TEST( aPlan->left->doIt( aTemplate, aPlan->left, & sFlag )
              != IDE_SUCCESS );
    if ( (sFlag & QMC_ROW_DATA_MASK) == QMC_ROW_DATA_NONE )
    {
        *aFlag = QMC_ROW_DATA_NONE;
    }
    else
    {
        //----------------------------
        // Right Child�� ����
        //----------------------------

        IDE_TEST( aPlan->right->doIt( aTemplate, aPlan->right, & sFlag )
                  != IDE_SUCCESS );

        if ( (sFlag & QMC_ROW_DATA_MASK) == QMC_ROW_DATA_NONE )
        {
            *aFlag = QMC_ROW_DATA_NONE;
        }
        else
        {
            // Left�� Right ��� �����ϴ� ���
            // Merge Join ����

            IDE_TEST( mergeJoin( aTemplate,
                                 sCodePlan,
                                 sDataPlan,
                                 ID_TRUE,  // Right Data�� ������
                                 aFlag )
                      != IDE_SUCCESS );
        }
    }

    //----------------------------
    // ����� ���� ������ ���� ó��
    //----------------------------

    if ( (*aFlag & QMC_ROW_DATA_MASK) == QMC_ROW_DATA_NONE )
    {
        *sDataPlan->flag &= ~QMND_MGJN_CURSOR_STORED_MASK;
        *sDataPlan->flag |= QMND_MGJN_CURSOR_STORED_FALSE;
    }
    else
    {
        sDataPlan->doIt = qmnMGJN::doItNext;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qmnMGJN::doItNext( qcTemplate * aTemplate,
                   qmnPlan    * aPlan,
                   qmcRowFlag * aFlag )
{
/***********************************************************************
 *
 * Description :
 *    ���� ���� �Լ�
 *
 * Implementation :
 *    Right�� �����ϰ� Data ���� ������ ���� Merge Join�� �����Ѵ�.
 *
 ***********************************************************************/

#define IDE_FN "qmnMGJN::doItNext"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qmncMGJN * sCodePlan = (qmncMGJN *) aPlan;
    qmndMGJN * sDataPlan =
        (qmndMGJN *) (aTemplate->tmplate.data + aPlan->offset);

    qmcRowFlag sFlag     = QMC_ROW_INITIALIZE;

    //----------------------------
    // Right Child�� ����
    //----------------------------

    IDE_TEST( aPlan->right->doIt( aTemplate, aPlan->right, & sFlag )
              != IDE_SUCCESS );

    if ( (sFlag & QMC_ROW_DATA_MASK) == QMC_ROW_DATA_NONE )
    {
        IDE_TEST( mergeJoin( aTemplate,
                             sCodePlan,
                             sDataPlan,
                             ID_FALSE,  // Right Data�� ����
                             aFlag )
                  != IDE_SUCCESS );
    }
    else
    {
        IDE_TEST( mergeJoin( aTemplate,
                             sCodePlan,
                             sDataPlan,
                             ID_TRUE,  // Right Data�� ������
                             aFlag )
                  != IDE_SUCCESS );
    }

    //----------------------------
    // ����� ���� ������ ���� ó��
    //----------------------------

    if ( (*aFlag & QMC_ROW_DATA_MASK) == QMC_ROW_DATA_NONE )
    {
        *sDataPlan->flag &= ~QMND_MGJN_CURSOR_STORED_MASK;
        *sDataPlan->flag |= QMND_MGJN_CURSOR_STORED_FALSE;

        sDataPlan->doIt = qmnMGJN::doItFirst;
    }
    else
    {
        // Nothing To Do
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qmnMGJN::doItSemi( qcTemplate * aTemplate,
                   qmnPlan    * aPlan,
                   qmcRowFlag * aFlag )
{
/***********************************************************************
 *
 * Description :
 *    ���� ���� �Լ�
 *
 * Implementation :
 *    Semi merge join�� �����Ѵ�.
 *
 ***********************************************************************/

    qmncMGJN * sCodePlan = (qmncMGJN *) aPlan;
    qmndMGJN * sDataPlan =
        (qmndMGJN *) (aTemplate->tmplate.data + aPlan->offset);

    qmcRowFlag sLeftFlag  = QMC_ROW_INITIALIZE;
    qmcRowFlag sRightFlag = QMC_ROW_INITIALIZE;
    idBool     sJudge;
    idBool     sFetchRight;

    if( ( *sDataPlan->flag & QMND_MGJN_CURSOR_STORED_MASK )
            == QMND_MGJN_CURSOR_STORED_TRUE )
    {
        sFetchRight = ID_FALSE;
    }
    else
    {
        sFetchRight = ID_TRUE;
    }

    //----------------------------
    // Left Child�� ����
    //----------------------------

    IDE_TEST( aPlan->left->doIt( aTemplate, aPlan->left, & sLeftFlag )
              != IDE_SUCCESS );

    while ( (sLeftFlag & QMC_ROW_DATA_MASK) == QMC_ROW_DATA_EXIST )
    {
        if( ( ( *sDataPlan->flag & QMND_MGJN_CURSOR_STORED_MASK )
                  == QMND_MGJN_CURSOR_STORED_TRUE ) &&
              ( sFetchRight == ID_FALSE ) )
        {
            IDE_TEST( qtc::judge( &sJudge,
                                  sCodePlan->storedMergeJoinPred,
                                  aTemplate )
                      != IDE_SUCCESS );

            if( sJudge == ID_TRUE )
            {
                IDE_TEST( restoreRightCursor( aTemplate, sCodePlan, sDataPlan )
                          != IDE_SUCCESS );

                sRightFlag = QMC_ROW_DATA_EXIST;
                sFetchRight = ID_FALSE;
            }
            else
            {
                *sDataPlan->flag &= ~QMND_MGJN_CURSOR_STORED_MASK;
                *sDataPlan->flag |= QMND_MGJN_CURSOR_STORED_FALSE;
            }
        }
        else
        {
            // Nothing to do.
        }

        if( sFetchRight == ID_TRUE )
        {
            sRightFlag = QMC_ROW_DATA_NONE;
            IDE_TEST( aPlan->right->doIt( aTemplate, aPlan->right, & sRightFlag )
                      != IDE_SUCCESS );

            if ( (sRightFlag & QMC_ROW_DATA_MASK) == QMC_ROW_DATA_NONE )
            {
                sFetchRight = ID_FALSE;
                IDE_TEST( aPlan->left->doIt( aTemplate, aPlan->left, & sLeftFlag )
                          != IDE_SUCCESS );

                continue;
            }
            else
            {
                // Nothing to do.
            }
        }
        else
        {
            if( ( sRightFlag & QMC_ROW_DATA_MASK ) == QMC_ROW_DATA_NONE )
            {
                *aFlag = QMC_ROW_DATA_NONE;
                break;
            }
            else
            {
                // Nothing to do.
            }
        }

        IDE_TEST( qtc::judge( &sJudge,
                              sCodePlan->mergeJoinPred,
                              aTemplate )
                  != IDE_SUCCESS );

        if( sJudge == ID_TRUE )
        {
            // mergeJoinPred�� �����Ǵ� ù ���� right�� ��� cursor ����
            IDE_TEST( manageCursor( aTemplate, sCodePlan, sDataPlan )
                      != IDE_SUCCESS );

            // joinFilter�� �����Ǵ��� Ȯ��
            IDE_TEST( checkJoinFilter( aTemplate, sCodePlan, &sJudge )
                      != IDE_SUCCESS );

            if( sJudge == ID_TRUE )
            {
                *aFlag = QMC_ROW_DATA_EXIST;
                break;
            }
            else
            {
                // joinFilter�� �����Ͽ����Ƿ� ���� right�� fetch
                sFetchRight = ID_TRUE;
            }
        }
        else
        {
            if( sCodePlan->compareLeftRight != NULL )
            {
                // Equi-joni�� ���
                IDE_TEST( qtc::judge( &sJudge,
                                      sCodePlan->compareLeftRight,
                                      aTemplate )
                          != IDE_SUCCESS );

                if( sJudge == ID_TRUE )
                {
                    // L > R
                    sFetchRight = ID_TRUE;
                    *sDataPlan->flag &= ~QMND_MGJN_CURSOR_STORED_MASK;
                    *sDataPlan->flag |= QMND_MGJN_CURSOR_STORED_FALSE;
                }
                else
                {
                    // L < R
                    sFetchRight = ID_FALSE;
                    IDE_TEST( aPlan->left->doIt( aTemplate, aPlan->left, & sLeftFlag )
                              != IDE_SUCCESS );
                }
            }
            else
            {
                // Non equi-join�� ���
                // BUG-41632 Semi-Merge-Join gives different result
                // ������Ŷ�� ���¿� ���� �������� �ٲ��� �Ѵ�.
                if ( sCodePlan->mergeJoinPred->indexArgument == 0 )
                {
                    // T2.i1 < T1.i1
                    //      MGJN
                    //     |    |
                    //    T1    T2 �� �����

                    if( ( sCodePlan->mergeJoinPred->node.module == &mtfLessThan ) ||
                        ( sCodePlan->mergeJoinPred->node.module == &mtfLessEqual ) )
                    {
                        // <, <= �� ���
                        // Left�� �о���δ�.

                        sFetchRight = ID_FALSE;
                        IDE_TEST( aPlan->left->doIt( aTemplate, aPlan->left, & sLeftFlag )
                                  != IDE_SUCCESS );



                    }
                    else
                    {
                        // >, >= �� ���
                        // Right�� �о���δ�.

                        *sDataPlan->flag &= ~QMND_MGJN_CURSOR_STORED_MASK;
                        *sDataPlan->flag |= QMND_MGJN_CURSOR_STORED_FALSE;

                        sFetchRight = ID_TRUE;
                    }
                }
                else
                {
                    // T1.i1 < T2.i1
                    //      MGJN
                    //     |    |
                    //    T1    T2 �� �����

                    if( ( sCodePlan->mergeJoinPred->node.module == &mtfLessThan ) ||
                        ( sCodePlan->mergeJoinPred->node.module == &mtfLessEqual ) )
                    {
                        *sDataPlan->flag &= ~QMND_MGJN_CURSOR_STORED_MASK;
                        *sDataPlan->flag |= QMND_MGJN_CURSOR_STORED_FALSE;

                        // <, <= �� ���
                        // Right�� �о���δ�.
                        sFetchRight = ID_TRUE;
                    }
                    else
                    {
                        // >, >= �� ���
                        // Left�� �о���δ�.
                        sFetchRight = ID_FALSE;
                        IDE_TEST( aPlan->left->doIt( aTemplate, aPlan->left, & sLeftFlag )
                                  != IDE_SUCCESS );
                    }
                }
            }
        }
    }

    if( (sLeftFlag & QMC_ROW_DATA_MASK) == QMC_ROW_DATA_NONE )
    {
        *aFlag = QMC_ROW_DATA_NONE;
    }
    else
    {
        // Nothing to do.
    }

    if ( (*aFlag & QMC_ROW_DATA_MASK) == QMC_ROW_DATA_NONE )
    {
        *sDataPlan->flag &= ~QMND_MGJN_CURSOR_STORED_MASK;
        *sDataPlan->flag |= QMND_MGJN_CURSOR_STORED_FALSE;
    }
    else
    {
        // Nothing To Do
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmnMGJN::doItAnti( qcTemplate * aTemplate,
                   qmnPlan    * aPlan,
                   qmcRowFlag * aFlag )
{
/***********************************************************************
 *
 * Description :
 *    ���� ���� �Լ�
 *
 * Implementation :
 *    Anti merge join�� �����Ѵ�.
 *
 ***********************************************************************/

    qmncMGJN * sCodePlan = (qmncMGJN *) aPlan;
    qmndMGJN * sDataPlan =
        (qmndMGJN *) (aTemplate->tmplate.data + aPlan->offset);

    qmcRowFlag sLeftFlag  = QMC_ROW_INITIALIZE;
    qmcRowFlag sRightFlag = QMC_ROW_INITIALIZE;
    idBool     sJudge;
    idBool     sFetchRight;

    if( ( *sDataPlan->flag & QMND_MGJN_CURSOR_STORED_MASK )
            == QMND_MGJN_CURSOR_STORED_TRUE )
    {
        sFetchRight = ID_FALSE;
    }
    else
    {
        sFetchRight = ID_TRUE;
    }

    //----------------------------
    // Left Child�� ����
    //----------------------------

    IDE_TEST( aPlan->left->doIt( aTemplate, aPlan->left, & sLeftFlag )
              != IDE_SUCCESS );

    while ( (sLeftFlag & QMC_ROW_DATA_MASK) == QMC_ROW_DATA_EXIST )
    {
        if( ( ( *sDataPlan->flag & QMND_MGJN_CURSOR_STORED_MASK )
                  == QMND_MGJN_CURSOR_STORED_TRUE ) &&
              ( sFetchRight == ID_FALSE ) )
        {
            IDE_TEST( qtc::judge( &sJudge,
                                  sCodePlan->storedMergeJoinPred,
                                  aTemplate )
                      != IDE_SUCCESS );

            if( sJudge == ID_TRUE )
            {
                IDE_TEST( restoreRightCursor( aTemplate, sCodePlan, sDataPlan )
                          != IDE_SUCCESS );

                sRightFlag = QMC_ROW_DATA_EXIST;
                sFetchRight = ID_FALSE;
            }
            else
            {
                *sDataPlan->flag &= ~QMND_MGJN_CURSOR_STORED_MASK;
                *sDataPlan->flag |= QMND_MGJN_CURSOR_STORED_FALSE;
            }
        }
        else
        {
            // Nothing to do.
        }

        if( sFetchRight == ID_TRUE )
        {
            IDE_TEST( aPlan->right->doIt( aTemplate, aPlan->right, & sRightFlag )
                      != IDE_SUCCESS );
        }
        else
        {
            // Nothing to do.
        }

        if( ( sRightFlag & QMC_ROW_DATA_MASK ) == QMC_ROW_DATA_NONE )
        {
            *aFlag = QMC_ROW_DATA_EXIST;
            break;
        }
        else
        {
            // Nothing to do.
        }

        IDE_TEST( qtc::judge( &sJudge,
                              sCodePlan->mergeJoinPred,
                              aTemplate )
                  != IDE_SUCCESS );

        if( sJudge == ID_TRUE )
        {
            // mergeJoinPred�� �����Ǵ� ù ���� right�� ��� cursor ����
            IDE_TEST( manageCursor( aTemplate, sCodePlan, sDataPlan )
                      != IDE_SUCCESS );

            // joinFilter�� �����Ǵ��� Ȯ��
            IDE_TEST( checkJoinFilter( aTemplate, sCodePlan, &sJudge )
                      != IDE_SUCCESS );

            if( sJudge == ID_TRUE )
            {
                // Anti join ����
                // ���� left�� �о���δ�.
                IDE_TEST( aPlan->left->doIt( aTemplate, aPlan->left, & sLeftFlag )
                          != IDE_SUCCESS );
                sFetchRight = ID_FALSE;
            }
            else
            {
                // joinFilter�� �����Ͽ����Ƿ� ���� right�� fetch
                sFetchRight = ID_TRUE;
            }
        }
        else
        {
            if( sCodePlan->compareLeftRight != NULL )
            {
                *sDataPlan->flag &= ~QMND_MGJN_CURSOR_STORED_MASK;
                *sDataPlan->flag |= QMND_MGJN_CURSOR_STORED_FALSE;

                // Equi-joni�� ���
                IDE_TEST( qtc::judge( &sJudge,
                                      sCodePlan->compareLeftRight,
                                      aTemplate )
                          != IDE_SUCCESS );

                if( sJudge == ID_TRUE )
                {
                    // L > R
                    sFetchRight = ID_TRUE;
                }
                else
                {
                    // L < R
                    // Anti join ����
                    IDE_TEST( manageCursor( aTemplate, sCodePlan, sDataPlan )
                              != IDE_SUCCESS );

                    *aFlag = QMC_ROW_DATA_EXIST;
                    break;
                }
            }
            else
            {
                *sDataPlan->flag &= ~QMND_MGJN_CURSOR_STORED_MASK;
                *sDataPlan->flag |= QMND_MGJN_CURSOR_STORED_FALSE;

                // Non equi-join�� ���

                if( ( sCodePlan->mergeJoinPred->node.module == &mtfLessThan ) ||
                    ( sCodePlan->mergeJoinPred->node.module == &mtfLessEqual ) )
                {
                    // <, <= �� ���
                    // Right�� �о���δ�.
                    sFetchRight = ID_TRUE;
                }
                else
                {
                    // >, >= �� ���
                    // Anti join ����
                    IDE_TEST( manageCursor( aTemplate, sCodePlan, sDataPlan )
                              != IDE_SUCCESS );

                    *aFlag = QMC_ROW_DATA_EXIST;
                    break;
                }
            }
        }
    }

    if( (sLeftFlag & QMC_ROW_DATA_MASK) == QMC_ROW_DATA_NONE )
    {
        *aFlag = QMC_ROW_DATA_NONE;
    }
    else
    {
        // Nothing to do.
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmnMGJN::firstInit( qcTemplate * aTemplate,
                    qmncMGJN   * aCodePlan,
                    qmndMGJN   * aDataPlan )
{
/***********************************************************************
 *
 * Description :
 *    MGJN node�� Data ������ ����� ���� �ʱ�ȭ�� ����
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qmnMGJN::firstInit"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    iduMemory * sMemory;

    //---------------------------------
    // ���ռ� �˻�
    //---------------------------------

    switch ( aCodePlan->flag & QMNC_MGJN_LEFT_CHILD_MASK )
    {
        case QMNC_MGJN_LEFT_CHILD_SCAN:
            IDE_DASSERT( aCodePlan->plan.left->type == QMN_SCAN );
            break;
        case QMNC_MGJN_LEFT_CHILD_PCRD:
            IDE_DASSERT( aCodePlan->plan.left->type == QMN_PCRD );
            break;
        case QMNC_MGJN_LEFT_CHILD_SORT:
            IDE_DASSERT( aCodePlan->plan.left->type == QMN_SORT );
            break;
        case QMNC_MGJN_LEFT_CHILD_MGJN:
            IDE_DASSERT( aCodePlan->plan.left->type == QMN_MGJN );
            break;
        default:
            IDE_DASSERT( 0 );
            break;
    }

    switch ( aCodePlan->flag & QMNC_MGJN_RIGHT_CHILD_MASK )
    {
        case QMNC_MGJN_RIGHT_CHILD_SCAN:
            IDE_DASSERT( aCodePlan->plan.right->type == QMN_SCAN );
            break;
        case QMNC_MGJN_RIGHT_CHILD_PCRD:
            IDE_DASSERT( aCodePlan->plan.right->type == QMN_PCRD );
            break;
        case QMNC_MGJN_RIGHT_CHILD_SORT:
            IDE_DASSERT( aCodePlan->plan.right->type == QMN_SORT );
            break;
        default:
            IDE_DASSERT( 0 );
            break;
    }

    IDE_DASSERT( aCodePlan->myNode != NULL );
    IDE_DASSERT( aCodePlan->mergeJoinPred != NULL );
    IDE_DASSERT( aCodePlan->storedMergeJoinPred != NULL );

    //---------------------------------
    // MGJN ���� ������ �ʱ�ȭ
    //---------------------------------

    IDE_TEST( initMtrNode( aTemplate, aCodePlan, aDataPlan ) != IDE_SUCCESS );
    aDataPlan->mtrRowSize = qmc::getMtrRowSize( aDataPlan->mtrNode );

    // Right Value ������ ���� ���� Ȯ��
    sMemory = aTemplate->stmt->qmxMem;
    IDE_TEST( sMemory->alloc( aDataPlan->mtrRowSize,
                              (void**) & aDataPlan->mtrNode->dstTuple->row )
              != IDE_SUCCESS);
    IDE_TEST_RAISE( aDataPlan->mtrNode->dstTuple->row == NULL, err_mem_alloc );

    //---------------------------------
    // �ʱ�ȭ �ϷḦ ǥ��
    //---------------------------------

    *aDataPlan->flag &= ~QMND_MGJN_INIT_DONE_MASK;
    *aDataPlan->flag |= QMND_MGJN_INIT_DONE_TRUE;

    return IDE_SUCCESS;

    IDE_EXCEPTION( err_mem_alloc );
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_MEMORY_ALLOCATION));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}


IDE_RC
qmnMGJN::initMtrNode( qcTemplate * aTemplate,
                      qmncMGJN   * aCodePlan,
                      qmndMGJN   * aDataPlan )
{
/***********************************************************************
 *
 * Description :
 *    ���� Column�� �ʱ�ȭ
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qmnMGJN::initMtrNode"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    //---------------------------------
    // ���ռ� �˻�
    //---------------------------------

    IDE_DASSERT( aCodePlan->myNode->next == NULL );
    IDE_DASSERT( aCodePlan->mtrNodeOffset > 0 );
    IDE_DASSERT(
        ( aTemplate->tmplate.rows[aCodePlan->myNode->dstNode->node.table].lflag
          & MTC_TUPLE_STORAGE_MASK ) == MTC_TUPLE_STORAGE_MEMORY );

    //---------------------------------
    // ���� Column�� �ʱ�ȭ
    //---------------------------------

    // 1.  ���� Column�� ���� ���� ����
    // 2.  ���� Column�� �ʱ�ȭ
    // 3.  ���� Column�� offset�� ������
    // 4.  Row Size�� ���

    aDataPlan->mtrNode =
        (qmdMtrNode*) (aTemplate->tmplate.data + aCodePlan->mtrNodeOffset);

    IDE_TEST( qmc::linkMtrNode( aCodePlan->myNode,
                                aDataPlan->mtrNode ) != IDE_SUCCESS );

    IDE_TEST( qmc::initMtrNode( aTemplate,
                                aDataPlan->mtrNode,
                                0 ) // Base Table�� �������� ����
              != IDE_SUCCESS );

    IDE_TEST( qmc::refineOffsets( aDataPlan->mtrNode,
                                  0 ) // Temp Table�� ������� ����
              != IDE_SUCCESS );

    IDE_TEST( qmc::setRowSize( aTemplate->stmt->qmxMem,
                               & aTemplate->tmplate,
                               aDataPlan->mtrNode->dstNode->node.table )
              != IDE_SUCCESS );

    //---------------------------------
    // ���ռ� �˻�
    //---------------------------------

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}


IDE_RC
qmnMGJN::mergeJoin( qcTemplate * aTemplate,
                    qmncMGJN   * aCodePlan,
                    qmndMGJN   * aDataPlan,
                    idBool       aRightExist,
                    qmcRowFlag * aFlag )
{
/***********************************************************************
 *
 * Description :
 *     Merge Join Algorithm�� ���� Row�� �����Ͽ� �����Ѵ�.
 *
 * Implementation :
 *     Merge Join Algorithm�� ������ �뷫 ������ ����.
 *
 *     - Right Row�� �������� �ʴ� ���
 *         A.  ���� Stored Merge ���� �˻�
 *             - Left Row ȹ��
 *             - ���� ���ǿ� ���� ó��
 *                 : Left Row�� ���� ���, [��� ����]
 *                 : Stored Merge ������ �������� �ʴ� ���, [��� ����]
 *                 : Stored Merge ������ �����ϴ� ���[��� ����]
 *
 *     - Right Row�� �����ϴ� ���
 *         B.  Merge ���� �˻�
 *             - ���� ���ǿ� ���� ó��
 *                 : Merge ������ �������� �ʴ� ���, [C ���� ����]
 *                 : Merge ������ �����ϴ� ��� [��� ����]
 *
 *         C. Stored Merge ���� �˻�
 *             - ������ Left �Ǵ� Right Row ȹ��
 *             - ���� ���ǿ� ���� ó��
 *                 - Left�� �а� row�� ���� ���, [��� ����]
 *                 - ���� Cursor�� ���ų� Stored Merge������ �������� �ʴ� ���
 *                     - Right Row�� ���� ���, [��� ����]
 *                     - Right Row�� �ִ� ���, [B ���� ����]
 *                 - Stored Merge ������ �����ϴ� ��� [��� ����]
 *
 *    ���� ���� ������ �߻�ȭ�ϸ� ������ ���� �������� ����ȴ�.
 *        - Right Row�� ���� ���
 *            A --> [C --> B --> C --> B]
 *        - Right Row�� �ִ� ���
 *            B --> [C --> B --> C --> B]
 *
 ***********************************************************************/

#define IDE_FN "qmnMGJN::mergeJoin"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    idBool sIsLoopMerge;

    if ( aRightExist == ID_FALSE )
    {
        //----------------------------------
        // Right Row�� �������� �ʴ� ���
        //----------------------------------

        // A.  ���� Stored Merge ������ �˻�

        IDE_TEST( checkFirstStoredMerge( aTemplate,
                                         aCodePlan,
                                         aDataPlan,
                                         aFlag )
                  != IDE_SUCCESS );
    }
    else
    {
        //-----------------------------------
        // Right Row�� �����ϴ� ���
        //-----------------------------------

        // B. Merge Join ���� �˻�

        IDE_TEST( checkMerge( aTemplate,
                              aCodePlan,
                              aDataPlan,
                              & sIsLoopMerge,
                              aFlag )
                  != IDE_SUCCESS );

        if ( sIsLoopMerge == ID_TRUE )
        {
            IDE_TEST( loopMerge( aTemplate, aCodePlan, aDataPlan, aFlag )
                      != IDE_SUCCESS );
        }
        else
        {
            // Nothing To Do
        }
    }


    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}


IDE_RC
qmnMGJN::loopMerge( qcTemplate * aTemplate,
                    qmncMGJN   * aCodePlan,
                    qmndMGJN   * aDataPlan,
                    qmcRowFlag * aFlag )
{
/***********************************************************************
 *
 * Description :
 *     �����ϴ� ����� �ְų�, �ƿ� ���� ������ �ݺ� ����
 *     cf) mergeJoin()�� C-->B �� �ݺ� ����
 *
 * Implementation :
 *     ����� ���� ������ ������ ����
 *         C : Stored Merge Join ���� �˻�
 *         B : Merge Join ���� �˻�
 *
 ***********************************************************************/

#define IDE_FN "qmnMGJN::loopMerge"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    idBool     sContinue;

    //-----------------------------------
    // ���� ����� �ְų�, ������ ����� ������ �ݺ� ����
    //-----------------------------------

    sContinue = ID_TRUE;

    while ( sContinue == ID_TRUE )
    {
        //-----------------------------------
        // C. Stored Merge ���� �˻�
        //-----------------------------------

        IDE_TEST( checkStoredMerge( aTemplate,
                                    aCodePlan,
                                    aDataPlan,
                                    & sContinue,
                                    aFlag )
                  != IDE_SUCCESS );

        if ( sContinue == ID_FALSE )
        {
            break;
        }
        else
        {
            // Nothing To Do
        }

        //-----------------------------------
        // B. Merge Join ���� �˻�
        //-----------------------------------

        IDE_TEST( checkMerge( aTemplate,
                              aCodePlan,
                              aDataPlan,
                              & sContinue,
                              aFlag )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}


IDE_RC
qmnMGJN::checkMerge( qcTemplate * aTemplate,
                     qmncMGJN   * aCodePlan,
                     qmndMGJN   * aDataPlan,
                     idBool     * aContinueNeed,
                     qmcRowFlag * aFlag )
{
/***********************************************************************
 *
 * Description :
 *    Merge ������ �����ϴ� ���� �Ǵ��ϰ�,
 *    �̿� ���� ��� ������ �������� �����Ѵ�.
 *
 * Implementation :
 *    ��� �����ϴ� ��� :
 *        Merge ������ �������� �ʴ� ���
 *    �������� �ʴ� ��� :
 *        ��� ������ �����ϴ� ��� : ��� ����
 *
 ***********************************************************************/

#define IDE_FN "qmnMGJN::checkMerge"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    idBool sJudge;

    // Merge Join ���� �˻�
    IDE_TEST( qtc::judge( & sJudge, aCodePlan->mergeJoinPred, aTemplate )
              != IDE_SUCCESS );

    if ( sJudge == ID_TRUE )
    {
        // Ŀ���� ���� ���� ����
        IDE_TEST( manageCursor( aTemplate, aCodePlan, aDataPlan )
                  != IDE_SUCCESS );

        *aFlag = QMC_ROW_DATA_EXIST;
        *aContinueNeed = ID_FALSE;
    }
    else
    {
        *aContinueNeed = ID_TRUE;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qmnMGJN::checkFirstStoredMerge( qcTemplate * aTemplate,
                                qmncMGJN   * aCodePlan,
                                qmndMGJN   * aDataPlan,
                                qmcRowFlag * aFlag )
{
/***********************************************************************
 *
 * Description :
 *    ���� Right Row �� ���� ��쿡 ȣ��Ǹ�,
 *    Stored Merge ������ �˻��Ͽ�
 *    ����� ������ ������ Loop Merge�� �ʿ��� ���� �Ǵ�
 *    cf) mergeJoin()���� A�� �ش��ϴ� ���� ����
 *
 * Implementation :
 *    Left Row�� �о� Stored Merge ������ �˻��Ѵ�.
 *
 ***********************************************************************/

#define IDE_FN "qmnMGJN::checkFirstStoredMerge"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    idBool sJudge;

    //-----------------------------------
    // ���ռ� �˻�
    //-----------------------------------

    // �ݵ�� ���� Cursor�� �����Ѵ�.
    IDE_DASSERT( (*aDataPlan->flag & QMND_MGJN_CURSOR_STORED_MASK)
                 == QMND_MGJN_CURSOR_STORED_TRUE );

    //-----------------------------------
    // Left Row ȹ��
    //-----------------------------------

    IDE_TEST( aCodePlan->plan.left->doIt( aTemplate,
                                          aCodePlan->plan.left,
                                          aFlag )
              != IDE_SUCCESS );

    if ( (*aFlag & QMC_ROW_DATA_MASK) == QMC_ROW_DATA_NONE )
    {
        // ����� �� �̻� ���� ����̴�.
        // Nothing to do.
    }
    else
    {
        //-----------------------------------
        // Stored Merge Join ���� �˻�
        //-----------------------------------

        IDE_TEST( qtc::judge( &sJudge,
                              aCodePlan->storedMergeJoinPred,
                              aTemplate )
                  != IDE_SUCCESS );

        if ( sJudge == ID_TRUE )
        {
            //-----------------------------------
            // Stored Merge Join ������ �����ϴ� ���
            //-----------------------------------

            // Cursor�� �����Ͽ� Right Row�� �����Ѵ�.
            IDE_TEST( restoreRightCursor( aTemplate, aCodePlan, aDataPlan )
                      != IDE_SUCCESS );
            
            *aFlag = QMC_ROW_DATA_EXIST;
        }
        else
        {
            //-----------------------------------
            // Stored Merge Join ������ �������� �ʴ� ���
            //-----------------------------------

            // ���̻� Cursor�� ��ȿ���� ����
            *aDataPlan->flag &= ~QMND_MGJN_CURSOR_STORED_MASK;
            *aDataPlan->flag |= QMND_MGJN_CURSOR_STORED_FALSE;

            // ���̻� ����� �������� ����
            *aFlag = QMC_ROW_DATA_NONE;
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}


IDE_RC
qmnMGJN::checkStoredMerge( qcTemplate * aTemplate,
                           qmncMGJN   * aCodePlan,
                           qmndMGJN   * aDataPlan,
                           idBool     * aContinueNeed,
                           qmcRowFlag * aFlag )
{
/***********************************************************************
 *
 * Description :
 *    Stored Merge ���ǰ� Filter ������ �����ϴ� ����
 *    �Ǵ��ϰ�, �̿� ���� ��� ������ �������� �����Ѵ�.
 *
 * Implementation :
 *
 *    ������ ���� ������ �ݺ��Ѵ�.
 *       - ���ο� Row�� ȹ��
 *           - Left Row�� �о��µ� ���� ���, [��� ����]
 *       - ���� Cursor�� ���� ���
 *           - ���ο� Row�� �ִٸ�, [B(Merge ����)���� ����]
 *           - ���ο� Row�� ���ٸ�, [��� ����]
 *       - ���� Cursor�� �ִ� ���
 *           - Stored Merge������ �������� �ʴ� ���
 *               - ���ο� Row�� ���ٸ�, [��� ����]
 *               - ���ο� Row�� �ִٸ�, [B���� ����]
 *           - Stored Merge������ �����ϳ�, Filter�� �������� �ʴ� ���
 *               - [��� �ݺ�]
 *           - Stored Merge���ǰ� Filter ������ �����ϴ� ���, [��� ����]
 *
 ***********************************************************************/

#define IDE_FN "qmnMGJN::checkStoredMerge"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    idBool sJudge;
    idBool sReadLeft;

    while ( 1 )
    {
        //----------------------------------------
        // ���ο� Row�� ȹ���� ���� ���� ���� �˻�
        //----------------------------------------

        IDE_TEST(
            readNewRow( aTemplate, aCodePlan, & sReadLeft, aFlag )
            != IDE_SUCCESS );

        //---------------------------------------
        // To Fix PR-8260
        // ������ ���� ����� ��������.
        // Left Row�� ���� ����� Store Cursor�� �˻��ؾ� ������,
        // Right Row�� ���� ����� Store Cursor�� �˻����� ���ƾ� �Ѵ�.
        // ����, ������ ���� �����Ѵ�.
        //
        // Left Row�� ���� ���
        //    - Data�� �ִ� ���
        //        - Store ������ ������ �˻�
        //        - Store ������ ������ Return �� ��� ����
        //    - Data�� ���� ���
        //        - ���� ����
        // Right Row�� ���� ���
        //    - Data�� �ִ� ���
        //        - Return �� ��� ����
        //    - Data�� ���� ���
        //        - Store ������ ������ Left�� ���� �� �����ϸ� �˻�
        //        - Store ������ ������ ���� ����
        //---------------------------------------

        if ( sReadLeft == ID_TRUE )
        {
            if ( (*aFlag & QMC_ROW_DATA_MASK) == QMC_ROW_DATA_EXIST )
            {
                if ( (*aDataPlan->flag & QMND_MGJN_CURSOR_STORED_MASK)
                     == QMND_MGJN_CURSOR_STORED_TRUE )
                {
                    // Store Condition�� �����ϸ� �˻��Ѵ�.
                    // Nothing To Do
                }
                else
                {
                    *aContinueNeed = ID_TRUE;
                    break;
                }
            }
            else
            {
                *aContinueNeed = ID_FALSE;
                break;
            }
        }
        else
        {
            if ( (*aFlag & QMC_ROW_DATA_MASK) == QMC_ROW_DATA_EXIST )
            {
                *aContinueNeed = ID_TRUE;
                break;
            }
            else
            {
                if ( (*aDataPlan->flag & QMND_MGJN_CURSOR_STORED_MASK)
                     == QMND_MGJN_CURSOR_STORED_TRUE )
                {
                    // Store Condition�� �����ϸ� Left�� ���� �� �˻��Ѵ�.
                    IDE_TEST( aCodePlan->plan.left->doIt( aTemplate,
                                                          aCodePlan->plan.left,
                                                          aFlag )
                              != IDE_SUCCESS );
                    if ( (*aFlag & QMC_ROW_DATA_MASK) == QMC_ROW_DATA_EXIST )
                    {
                        // ���� Cursor�κ��� �ٽ� �˻��Ѵ�.
                        // Nothing To Do
                    }
                    else
                    {
                        *aContinueNeed = ID_FALSE;
                        break;
                    }
                }
                else
                {
                    *aContinueNeed = ID_FALSE;
                    break;
                }
            }
        }

        //----------------------------------------
        // Stored Merge ������ �̿��� �˻�
        //----------------------------------------

        // ���� Row�� left�� ��쿡�� �˻��ϰ� �ȴ�.
        IDE_TEST( qtc::judge( &sJudge,
                              aCodePlan->storedMergeJoinPred,
                              aTemplate )
                  != IDE_SUCCESS );

        if ( sJudge == ID_FALSE )
        {
            // Stored Merge Join ������ �������� �ʴ� ���

            // ���̻� Cursor�� ��ȿ���� ����
            *aDataPlan->flag &= ~QMND_MGJN_CURSOR_STORED_MASK;
            *aDataPlan->flag |= QMND_MGJN_CURSOR_STORED_FALSE;

            if ( sReadLeft == ID_TRUE )
            {
                // Stored Condition�� ���� ������
                // ���� Row�� ������ �°� �����ϰ� �ȴ�.
                *aContinueNeed = ID_TRUE;
            }
            else
            {
                // Right�� ���� ����� Right�� Data�� ����
                // Left�� �ٽ� ���� �Ŀ� �˻��ϴ� ����̴�.
                // �� �̻� ������ �� ����.
                *aContinueNeed = ID_FALSE;
                // To Fix BUG-8747
                *aFlag = QMC_ROW_DATA_NONE;
            }
            break;
        }
        else
        {
            // Stored Merge Join ������ �����ϴ� ���

            // Cursor�� �����Ͽ� Right Row�� Tuple Set�� ����
            IDE_TEST( restoreRightCursor( aTemplate,
                                          aCodePlan,
                                          aDataPlan )
                      != IDE_SUCCESS );

            // ������ ��� ��� ����
            *aFlag = QMC_ROW_DATA_EXIST;
            *aContinueNeed = ID_FALSE;
            break;
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qmnMGJN::manageCursor( qcTemplate * aTemplate,
                       qmncMGJN   * aCodePlan,
                       qmndMGJN   * aDataPlan )
{
/***********************************************************************
 *
 * Description :
 *     Merge Join Predicate ���� �� Cursor ����
 *
 * Implementation :
 *     �̹� ���� Cursor�� �����Ѵٸ�, ���ٸ� �۾��� ���� �ʴ´�.
 *     ���� Cursor�� �������� ���� ���,
 *         - Cursor ����
 *         - Right Value ����
 *
 ***********************************************************************/

#define IDE_FN "qmnMGJN::manageCursor"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qmdMtrNode * sNode;

    if ( (*aDataPlan->flag & QMND_MGJN_CURSOR_STORED_MASK )
         == QMND_MGJN_CURSOR_STORED_FALSE )
    {
        //------------------------------------
        // Cursor�� ����
        //------------------------------------

        IDE_TEST( storeRightCursor( aTemplate, aCodePlan )
                  != IDE_SUCCESS );

        // To Fix PR-8062
        // Mask �ʱ�ȭ �߸���.
        *aDataPlan->flag &= ~QMND_MGJN_CURSOR_STORED_MASK;
        *aDataPlan->flag |= QMND_MGJN_CURSOR_STORED_TRUE;

        //------------------------------------
        // Right Value ����
        //------------------------------------

        for ( sNode = aDataPlan->mtrNode;
              sNode != NULL;
              sNode = sNode->next )
        {
            IDE_TEST( sNode->func.setMtr( aTemplate,
                                          sNode,
                                          sNode->dstTuple->row )
                      != IDE_SUCCESS );
        }
    }
    else
    {
        // Nothing To Do
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qmnMGJN::readNewRow( qcTemplate * aTemplate,
                     qmncMGJN   * aCodePlan,
                     idBool     * aReadLeft,
                     qmcRowFlag * aFlag )
{
/***********************************************************************
 *
 * Description :
 *     ������ Left �Ǵ� Right Row�� ȹ���Ѵ�.
 *
 * Implementation :
 *     ��ȣ �������� ���(Compare ������ ����)
 *         - ������ �������� ������, Left Read
 *         - ������ �������� ������, Right Read
 *     ��ȣ �����ڰ� �ƴ� ���(Compare ������ ����)
 *         - Left Read
 *
 ***********************************************************************/

#define IDE_FN "qmnMGJN::readNewRow"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    idBool sJudge;

    //------------------------------------
    // ���� Row�� ����
    //------------------------------------

    if ( aCodePlan->compareLeftRight != NULL )
    {
        IDE_TEST( qtc::judge( &sJudge,
                              aCodePlan->compareLeftRight,
                              aTemplate )
                  != IDE_SUCCESS );
    }
    else
    {
        sJudge = ID_FALSE;
    }

    //------------------------------------
    // ���ο� Row�� ȹ��
    //------------------------------------

    if ( sJudge == ID_TRUE )
    {
        // Right Row�� �д´�.
        IDE_TEST( aCodePlan->plan.right->doIt( aTemplate,
                                               aCodePlan->plan.right,
                                               aFlag )
                  != IDE_SUCCESS );
        *aReadLeft = ID_FALSE;
    }
    else
    {
        // To Fix PR-8062
        // Left Row�� �д´�.
        IDE_TEST( aCodePlan->plan.left->doIt( aTemplate,
                                              aCodePlan->plan.left,
                                              aFlag )
                  != IDE_SUCCESS );

        *aReadLeft = ID_TRUE;
    }


    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}



IDE_RC
qmnMGJN::checkJoinFilter( qcTemplate * aTemplate,
                          qmncMGJN   * aCodePlan,
                          idBool     * aResult )
{
/***********************************************************************
 *
 * Description :
 *     Join Filter�� ���� ���� �˻�
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qmnMGJN::checkJoinFilter"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    if ( aCodePlan->joinFilter == NULL )
    {
        *aResult = ID_TRUE;
    }
    else
    {
        IDE_TEST( qtc::judge( aResult, aCodePlan->joinFilter, aTemplate )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qmnMGJN::storeRightCursor( qcTemplate * aTemplate,
                           qmncMGJN   * aCodePlan )
{
/***********************************************************************
 *
 * Description :
 *     Right Child�� Cursor�� �����Ѵ�.
 *
 * Implementation :
 *     Child�� ������ ���� Cursor ���� �Լ��� ȣ���Ѵ�.
 *
 ***********************************************************************/

#define IDE_FN "qmnMGJN::storeRightCursor"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    //------------------------------------
    // Right Child�� Cursor ����
    //------------------------------------

    switch ( aCodePlan->flag & QMNC_MGJN_RIGHT_CHILD_MASK )
    {
        case QMNC_MGJN_RIGHT_CHILD_SCAN:
            IDE_TEST( qmnSCAN::storeCursor( aTemplate, aCodePlan->plan.right )
                      != IDE_SUCCESS );
            break;
        case QMNC_MGJN_RIGHT_CHILD_PCRD:
            IDE_TEST( qmnPCRD::storeCursor( aTemplate, aCodePlan->plan.right )
                      != IDE_SUCCESS );
            break;
        case QMNC_MGJN_RIGHT_CHILD_SORT:
            IDE_TEST( qmnSORT::storeCursor( aTemplate, aCodePlan->plan.right )
                      != IDE_SUCCESS );
            break;
        default:
            IDE_DASSERT( 0 );
            break;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qmnMGJN::restoreRightCursor( qcTemplate * aTemplate,
                             qmncMGJN   * aCodePlan,
                             qmndMGJN   * aDataPlan )
{
/***********************************************************************
 *
 * Description :
 *     Right Child�� Cursor�� �����Ѵ�.
 *
 * Implementation :
 *     Child�� ������ ���� Cursor ���� �Լ��� ȣ���Ѵ�.
 *
 ***********************************************************************/

#define IDE_FN "qmnMGJN::restoreRightCursor"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    // To Check PR-11733
    IDE_ASSERT( ( *aDataPlan->flag & QMND_MGJN_CURSOR_STORED_MASK )
                == QMND_MGJN_CURSOR_STORED_TRUE );

    //------------------------------------
    // Right Child�� Cursor ����
    //------------------------------------

    switch ( aCodePlan->flag & QMNC_MGJN_RIGHT_CHILD_MASK )
    {
        case QMNC_MGJN_RIGHT_CHILD_SCAN:
            IDE_TEST( qmnSCAN::restoreCursor( aTemplate,
                                              aCodePlan->plan.right )
                      != IDE_SUCCESS );
            break;
        case QMNC_MGJN_RIGHT_CHILD_PCRD:
            IDE_TEST( qmnPCRD::restoreCursor( aTemplate,
                                              aCodePlan->plan.right )
                      != IDE_SUCCESS );
            break;
        case QMNC_MGJN_RIGHT_CHILD_SORT:
            IDE_TEST( qmnSORT::restoreCursor( aTemplate,
                                              aCodePlan->plan.right )
                      != IDE_SUCCESS );
            break;
        default:
            IDE_DASSERT( 0 );
            break;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}
