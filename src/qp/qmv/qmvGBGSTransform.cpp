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
 * $Id: qmvGBGSTransform.cpp 89835 2021-01-22 10:10:02Z andrew.shin $
 *
 * PROJ-2415 Grouping Sets Clause
 * 
 * Grouping Sets�� ���� �� Sub Group�� ������ Query Set���� �����
 * �̸� UNION ALL �� �����Ͽ� �������� ������ ParseTree�� �����Ѵ�.
 * 
 * SELECT /+Hint+/ TOP 3 i1, i2, i3 x, SUM( i4 )
 *   INTO :a, :b, :c, :d
 *   FROM t1
 *  WHERE i1 >= 0
 * GROUP BY GROUPING SETS( i1, i2, i3, () )
 * HAVING GROUPING_ID( i1, i2, i3 ) >= 0;
 * 
 * ========================= The Above Query Would Be Transformed As Below ==========================
 *        
 * SELECT /+Hint+/ TOP 3 *  -> INTO Clause �� TOP, ORDER BY�� ó���ϱ� ���� ���� Query Set�� Simple View ���·� �����Ѵ�.
 *   INTO :a, :b, :c, :d
 *   FROM (                 -> GROUPING SETS�� ������ QuerySet�� �� GROUP ���� ����&������ Query Sets�� ����
 *                             UNION ALL Set Query�� �����Ͽ� Simple View ���·� ������ ���� Query Set�� FROM�� Inline View�� ����
 *                             
 *            SELECT i1, NULL, NULL x, SUM( i4 )         
 *              FROM ( SELECT * FROM t1 WHERE i1 >= 0 ) -> SCAN COUNT�� ���̱� ���� SELECT, FROM, WHERE, HIERARCHY��
 *            GROUP BY i1                                  InlineView�� ����
 *            HAVING GROUPING_ID( i1, NULL, NULL ) >= 0;
 *                       UNION ALL
 *            SELECT NULL, i2, NULL x, SUM( i4 )             
 *              FROM ( SELECT * FROM t1 WHERE i1 >= 0 ) -> SameViewRef�� ���� ������ View�� �ٶ󺸵��� ����
 *            GROUP BY i2                              
 *            HAVING GROUPING_ID( NULL, i2, NULL ) >= 0;
 *                       UNION ALL
 *            SELECT NULL, NULL, i3 x, SUM( i4 )
 *              FROM ( SELECT * FROM t1 WHERE i1 >= 0 ) -> SameViewRef�� ���� ������ View�� �ٶ󺸵��� ����
 *            GROUP BY i3                               
 *            HAVING GROUPING_ID( NULL, NULL, i3 ) >= 0;
 *            
 *        );
 *
 * * GROUPING SETS�� ���ڰ� �ϳ� �� ��� GROUP BY �� �����ϱ� ������ GROUPING SETS �� �����Ѵ�.
 *
 ***********************************************************************/

#include <idl.h>
#include <qcg.h>
#include <qtc.h>
#include <qmvQTC.h>
#include <qmvOrderBy.h>
#include <qmvGBGSTransform.h>
#include <qmvQuerySet.h>
#include <qmv.h>
#include <qmo.h>
#include <qmx.h>
#include <qcpManager.h>
#include <qmoViewMerging.h>

IDE_RC qmvGBGSTransform::searchGroupingSets( qmsQuerySet       * aQuerySet,
                                             qmsConcatElement ** aGroupingSets,
                                             qmsConcatElement ** aPrevGroup )
{
/***********************************************************************
 *
 * Description :
 *    GROUP BY ������ GROUPING SETS Type�� Group��
 *    �� Group�� Next�� ������ Previous Group�� �����Ѵ�. 
 *
 * Implementation :
 *
 ***********************************************************************/
    qmsConcatElement * sGroup;
    qmsConcatElement * sPrevGroup = NULL;
    idBool             sIsFound   = ID_FALSE;

    IDU_FIT_POINT_FATAL( "qmvGBGSTransform::searchGroupingSets::__FT__" );

    *aGroupingSets = NULL;
    *aPrevGroup    = NULL;

    sGroup = aQuerySet->SFWGH->group;

    for ( sGroup  = aQuerySet->SFWGH->group;
          sGroup != NULL;
          sGroup  = sGroup->next )
    {
        switch ( sGroup->type )
        {
            case QMS_GROUPBY_GROUPING_SETS :
                *aGroupingSets = sGroup;
                *aPrevGroup = sPrevGroup;
                /* fall through */ 
            case QMS_GROUPBY_ROLLUP :
            case QMS_GROUPBY_CUBE :
                if ( sIsFound == ID_FALSE )
                {
                    sIsFound = ID_TRUE;
                }
                else
                {
                    IDE_RAISE( ERR_NOT_SUPPORT_COMPLICATE_GROUP_EXT );
                }
                break;
            case QMS_GROUPBY_NORMAL :
            case QMS_GROUPBY_NULL:
                if ( sIsFound == ID_FALSE )
                {
                    sPrevGroup = sGroup;
                }
                else
                {
                    /* Nothing to do */
                }
                break;
            default :
                IDE_RAISE( ERR_UNEXPECTED_GROUP_BY_TYPE );
                break;
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_NOT_SUPPORT_COMPLICATE_GROUP_EXT )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMV_NOT_SUPPORT_COMPLICATE_GROUP_EXT ) );
    }
    IDE_EXCEPTION( ERR_UNEXPECTED_GROUP_BY_TYPE )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmvGBGSTransform::searchGroupingSets",
                                  "Unsupported groupby type." ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmvGBGSTransform::copyPartialQuerySet( qcStatement          * aStatement,
                                              qmsConcatElement     * aGroupingSets,
                                              qmsQuerySet          * aSrcQuerySet,
                                              qmvGBGSQuerySetList ** aDstQuerySetList )
{
/***********************************************************************
 *
 * Description :
 *     Grouping Sets�� Arguments �� ��ŭ QuerySet�� �����Ͽ�
 *     List( qmvGBGSQuerySetList )���·� �����Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/
    qmsConcatElement    * sGroupingSetsArgs;
    qmvGBGSQuerySetList * sQuerySetList = NULL;
    qmvGBGSQuerySetList * sPrevQuerySetList = NULL;
    qmsSFWGH            * sSFWGH;
    qmsFrom             * sFrom;    
    SInt                  sStartOffset = 0;
    SInt                  sSize = 0;
    qcStatement           sStatement;

    IDU_FIT_POINT_FATAL( "qmvGBGSTransform::copyPartialQuerySet::__FT__" );

    QC_SET_STATEMENT( ( &sStatement ), aStatement, NULL );

    // SrcQuerySet�� Query Statement�� �� SELECT~GROUP~(HAVING)������
    // Partial Statement�� Offset�� Size�� ���Ѵ�.    
    IDE_TEST( getPosForPartialQuerySet( aSrcQuerySet,
                                        & sStartOffset,
                                        & sSize )
              != IDE_SUCCESS );

    for ( sGroupingSetsArgs  = aGroupingSets->arguments;
          sGroupingSetsArgs != NULL;
          sGroupingSetsArgs  = sGroupingSetsArgs->next )
    {
        // FIT TEST
        IDU_FIT_POINT( "qmvGBGSTransform::copyPartialQuerySet::STRUCT_ALLOC::ERR_MEM",
                       idERR_ABORT_InsufficientMemory);
        
        IDE_TEST( STRUCT_ALLOC( QC_QMP_MEM( aStatement ),
                                qmvGBGSQuerySetList,
                                &sQuerySetList ) != IDE_SUCCESS);

        if ( sGroupingSetsArgs == aGroupingSets->arguments )
        {
            // First Loop
            sPrevQuerySetList  = sQuerySetList;
            *aDstQuerySetList  = sPrevQuerySetList;
        }
        else
        {
            if ( sPrevQuerySetList != NULL )
            {
                sPrevQuerySetList->next = sQuerySetList;
                sPrevQuerySetList = sPrevQuerySetList->next;
            }
            else
            {
                // Compile Warning ������ �߰�
                IDE_RAISE( ERR_INVALID_POINTER );
            }            
        }

        sStatement.myPlan->stmtText = aSrcQuerySet->startPos.stmtText;
        sStatement.myPlan->stmtTextLen = idlOS::strlen( aSrcQuerySet->startPos.stmtText );
        
        // SrcQuerySet�� Statement Text�� PartialParsing�ؼ� parseTree�� �����Ѵ�.    
        IDE_TEST( qcpManager::parsePartialForQuerySet( &sStatement,
                                                       aSrcQuerySet->startPos.stmtText,
                                                       sStartOffset,
                                                       sSize )
                  != IDE_SUCCESS );

        sQuerySetList->querySet = ( ( qmsParseTree* )sStatement.myPlan->parseTree )->querySet;
        
        sSFWGH = sQuerySetList->querySet->SFWGH;
        // Grouping Sets Transformed MASK 
        sSFWGH->lflag &= ~QMV_SFWGH_GBGS_TRANSFORM_MASK;
        sSFWGH->lflag |= QMV_SFWGH_GBGS_TRANSFORM_BOTTOM;

        // Bottom View�� Simple View Merging���� �ʴ´�.
        for ( sFrom  = sQuerySetList->querySet->SFWGH->from;
              sFrom != NULL;
              sFrom  = sFrom->next )
        {        
            IDE_TEST( setNoMerge( sFrom ) 
                      != IDE_SUCCESS );
        }           

        // DISTINCT TOP INTO Initialize
        sSFWGH->selectType = QMS_ALL;
        sSFWGH->top = NULL;
        sSFWGH->intoVariables = NULL;
    }
   
    if ( sQuerySetList != NULL )
    { 
        sQuerySetList->next = NULL;    
    }
    else
    {
        IDE_RAISE( ERR_UNEXPECTED_ERROR );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_UNEXPECTED_ERROR )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmvGBGSTransform::copyPartialQuerySet",
                                  "Unexpected Error" ) );
    }
    IDE_EXCEPTION( ERR_INVALID_POINTER )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmvGBGSTransform::copyPartialQuerySet",
                                  "Invalid Pointer" ) );
    }    
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmvGBGSTransform::getPosForPartialQuerySet( qmsQuerySet * aQuerySet,
                                                   SInt        * aStartOffset,
                                                   SInt        * aSize )
{
/***********************************************************************
 *
 * Description :
 *     Query Set���� SELECT - FROM - WHERE - HIERARCHY - GROUPBY - (HAVING)��
 *     �κ� Statement�� Parsing�ϱ� ���� SELECT�� ���� Offset��
 *     GROUPBY( HAVING�� ������� HAVING )�� LastNode Offset+Size�� ���Ѵ�.
 *
 * Implementation :
 * 
 ***********************************************************************/
    SInt               sStartOffset = QC_POS_EMPTY_OFFSET; // -1
    SInt               sEndOffset   = QC_POS_EMPTY_OFFSET; // -1
    qmsConcatElement * sGroup;

    IDU_FIT_POINT_FATAL( "qmvGBGSTransform::getPosForPartialQuerySet::__FT__" );

    // Get Start Offset of SELECT    
    sStartOffset = aQuerySet->startPos.offset;

    // Get End Offset of HAVING or GROUP BY 
    if ( aQuerySet->SFWGH->having != NULL )
    {
        sEndOffset = aQuerySet->SFWGH->having->position.offset
            + aQuerySet->SFWGH->having->position.size;
    }
    else
    {        
        for ( sGroup = aQuerySet->SFWGH->group;
              sGroup->next != NULL;
              sGroup = sGroup->next )
        {
            // GROUP BY�� ������ Element�� ���Ѵ�.
        };

        sEndOffset = sGroup->position.offset + sGroup->position.size;
    }

    // ���� 1 Byte�� Ȯ���Ͽ� Double Quotation�� ������ �����Ѵ�.
    if ( *( aQuerySet->startPos.stmtText + sEndOffset ) == '"' )
    {
        sEndOffset++;
    }
    else
    {
        /* Nothing to do */
    }    
    
    // Invalid Offset
    IDE_TEST_RAISE( ( sStartOffset == QC_POS_EMPTY_OFFSET ) ||
                    ( sEndOffset   == QC_POS_EMPTY_OFFSET ),
                    ERR_INVALID_OFFSET );
    
    //Set Return Variables    
    *aStartOffset = sStartOffset;
    *aSize        = sEndOffset - *aStartOffset;

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_INVALID_OFFSET )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmvGBGSTransform::getPosForPartialQuerySet",
                                  "Invalid Offset" ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmvGBGSTransform::getPosForOrderBy( qmsSortColumns * aOrderBy,
                                           SInt           * aStartOffset,
                                           SInt           * aSize )
{
/***********************************************************************
 *
 * Description :
 *     ORDER BY �� sortColumn�� Partial Parsing �ϱ� ����
 *     First Node�� Offset�� Last Node Offset + Size�� ���Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/
    SInt               sStartOffset = QC_POS_EMPTY_OFFSET; // -1
    SInt               sEndOffset   = QC_POS_EMPTY_OFFSET; // -1
    qmsSortColumns   * sOrderBy     = aOrderBy;

    IDU_FIT_POINT_FATAL( "qmvGBGSTransform::getPosForOrderBy::__FT__" );

    // Get Start Offset of The First Node Of ORDER BY    
    sStartOffset = sOrderBy->sortColumn->position.offset;

    // ���� 1 Byte�� Ȯ���Ͽ� Double Quotation�� ������ �����Ѵ�.
    if ( sStartOffset > 0 )
    {
        if ( *( sOrderBy->sortColumn->position.stmtText + sStartOffset - 1 ) == '"' )
        {
            sStartOffset--;
        }
        else
        {
            /* Nothing to do */
        }
    }
    else
    {
        /* Nothing to do */
    }

    for ( ; sOrderBy->next != NULL; sOrderBy  = sOrderBy->next )
    {
        // OrderBy�� ������ Node�� ���Ѵ�.
    };

    // Get End Offset of The Last Node Of ORDER BY
    sEndOffset = sOrderBy->sortColumn->position.offset +
        sOrderBy->sortColumn->position.size;
    
    // ���� 1 Byte�� Ȯ���Ͽ� Double Quotation�� ������ �����Ѵ�.
    if ( *( sOrderBy->sortColumn->position.stmtText + sEndOffset ) == '"' )
    {
        sEndOffset++;
    }
    else
    {
        /* Nothing to do */
    }

    // Invalid Offset
    IDE_TEST_RAISE( ( sStartOffset == QC_POS_EMPTY_OFFSET ) ||
                    ( sEndOffset   == QC_POS_EMPTY_OFFSET ),
                    ERR_INVALID_OFFSET );

    // Set Return Variables
    *aStartOffset = sStartOffset;
    *aSize = sEndOffset - sStartOffset;

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_INVALID_OFFSET )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmvGBGSTransform::getPosForOrderBy",
                                  "Invalid Offset" ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmvGBGSTransform::modifyGroupBy( qcStatement         * aStatement,
                                        qmvGBGSQuerySetList * aQuerySetList )
{
/***********************************************************************
 *
 * Description :
 *     GROUP BY ������ QMS_GROUPBY_GROUPING_SETS Type�� Group Element ��
 *     �� Grouping Sets Type Element �� Next�� ������ Prev Element�� ã��
 *     Grouping Sets Clause�� Group By Clause�� �����Ѵ�.
 *     
 * Implementation :
 *
 ***********************************************************************/
    qmvGBGSQuerySetList * sQuerySetList;
    qmsConcatElement    * sGroup = NULL;
    qmsConcatElement    * sPrevGroup = NULL;    
    SInt                  sNullCheck = 0;

    IDU_FIT_POINT_FATAL( "qmvGBGSTransform::modifyGroupBy::__FT__" );

    for ( sQuerySetList  = aQuerySetList;
          sQuerySetList != NULL;
          sQuerySetList  = sQuerySetList->next, sNullCheck++ )
    {
        // Grouping Sets Type�� Group Element�� Previous Group Element�� �����͸� ��´�.   
        IDE_TEST( searchGroupingSets( sQuerySetList->querySet, &sGroup, &sPrevGroup )
                  != IDE_SUCCESS );

        // GROUP BY ���� �����Ѵ�.
        if ( sGroup != NULL )
        {
            IDE_TEST( modifyGroupingSets( aStatement,
                                          sQuerySetList->querySet->SFWGH,
                                          sPrevGroup,
                                          sGroup,
                                          sGroup->next,
                                          sNullCheck )
                      != IDE_SUCCESS );
        }
        else
        {
            IDE_RAISE( ERR_UNEXPECTED_ERROR ); 
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_UNEXPECTED_ERROR )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmvGBGSTransform::modifyGroupBy",
                                  "Unexpected Error" ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmvGBGSTransform::modifyGroupingSets( qcStatement      * aStatement,
                                             qmsSFWGH         * aSFWGH,
                                             qmsConcatElement * aPrev,
                                             qmsConcatElement * aElement,
                                             qmsConcatElement * aNext,
                                             SInt               aNullCheck )
{
/***********************************************************************
 *
 * Description :
 *     GROUP BY������ Grouping sets�� �����ϰ�, Grouping Sets �� Argument����
 *     GroupingSets�� �ִ� �ڸ��� �����Ѵ�.
 *     
 *     Grouping sets�� arguments�� �ڽ��� Query Set�� �ش��ϴ� Group�� �ƴϸ�,
 *     - QMS_GROUPBY_NORMAL Type�� ��� QMS_GROUPBY_NULL�� �����Ѵ�.
 *     - QMS_GROUPBY_ROLLUP �Ǵ� QMS_GROUPBY_CUBE �� ��� ROLLUP/CUBE Group�� �����ϰ�,
 *       ROLLUP/CUBE�� Argument���� ROLLUP/CUBE�� �־��� �ڸ��� �����Ѵ�.  
 *
 *     Grouping Sets�� ���ڷ� �� �� �ִ� Group Type�� ������ ����.
 *
 *     1) QMS_GROUPBY_NORMAL
 *                           1-1) Normal Group ( Normal Expression )
 *                           1-2) Concatenated Group( LIST )
 *     2) QMS_GROUPBY_ROLLUP 
 *     3) QMS_GROUPBY_CUBE
 *
 *     ROLLUP�� CUBE�� ���ڷ� �� �� �ִ� Group Type�� ������ ����.
 *
 *     1) QMS_GROUPBY_NORMAL
 *                           1-1) Normal Group ( Normal Expression )
 *                           1-2) Concatenated Group( LIST )
 *
 *     Example)
 *     
 *     GROUP BY GROUPING SETS( i1, ROLLUP( i2, i3 ), ( i4, i5 ) ), i6
 *     
 *     ���� Grouping Sets�� ������ Query Set�� �Ʒ��� ���� 3���� Query Set����
 *     Transform �ȴ�.
 *       
 *     QUERY SET 1 : GROUP BY i1, i2(NULL), i3(NULL), i4(NULL), i5(NULL), i6
 *     UNION ALL
 *     QUERY SET 2 : GROUP BY i1(NULL), ROLLUP( i2, i3 ), i4(NULL), i5(NULL), i6
 *     UNION ALL
 *     QUERY SET 3 : GROUP BY i1(NULL), i2(NULL), i3(NULL), i4, i5, i6
 *
 *     * (NULL) �� QMS_GROUPBY_NULL Type�� Group Element�� �ǹ��Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/
    qmsConcatElement * sElement;
    qmsConcatElement * sPrev;
    SInt               sNullIndex;

    IDU_FIT_POINT_FATAL( "qmvGBGSTransform::modifyGroupingSets::__FT__" );

    if ( aPrev == NULL )
    {
        aSFWGH->group = aElement->arguments;
        sPrev = NULL;
    }
    else
    {
        aPrev->next = aElement->arguments;
        sPrev = aPrev;
    }

    for ( sElement  = aElement->arguments, sNullIndex = 0;
          sElement != NULL; 
          sElement  = sElement->next, sNullIndex++ )
    {
        // ������ querySetList���� �ڽ��� querySet����( aNullCheck )��
        // Grouping Sets Argument�� ����(sNullIndex)�� ���� Group�� ��ȿ�ϴ�.
        if ( aNullCheck == sNullIndex )
        {   // ��ȿ�� Group�� ���
            
            if ( sElement->type == QMS_GROUPBY_NORMAL )
            {
                if ( ( sElement->arithmeticOrList->node.lflag & MTC_NODE_OPERATOR_MASK ) ==
                     MTC_NODE_OPERATOR_LIST )
                {
                    // LIST �� ��� ó��
                    IDE_TEST( modifyList( aStatement,
                                          aSFWGH,
                                          &sPrev,
                                          sElement,
                                          ( sElement->next == NULL ) ? aNext : sElement->next,
                                          QMS_GROUPBY_NORMAL )
                              != IDE_SUCCESS );
                }
                else
                {
                    sPrev = sElement;
                }
            }
            else
            {
                sPrev = sElement; 
            }
        }
        else
        {   // ��ȿ���� ���� Group�� ���            
            switch ( sElement->type )
            {
                case QMS_GROUPBY_ROLLUP :
                case QMS_GROUPBY_CUBE :
                    // ROLLUP, CUBE �� ��� ó��
                    IDE_TEST( modifyRollupCube( aStatement,
                                                aSFWGH,
                                                &sPrev,
                                                sElement,
                                                ( sElement->next == NULL ) ? aNext : sElement->next )
                              != IDE_SUCCESS );
                    break;  
                case QMS_GROUPBY_NORMAL :
                    if ( ( sElement->arithmeticOrList->node.lflag & MTC_NODE_OPERATOR_MASK ) ==
                         MTC_NODE_OPERATOR_LIST )
                    {
                        // LIST �� ��� ó��
                        IDE_TEST( modifyList( aStatement,
                                              aSFWGH,
                                              &sPrev,
                                              sElement,
                                              ( sElement->next == NULL ) ? aNext : sElement->next,
                                              QMS_GROUPBY_NULL )
                                  != IDE_SUCCESS );
                    }
                    else
                    {
                        sElement->type = QMS_GROUPBY_NULL;
                        sPrev = sElement;
                    }
                    break;
                default :
                    IDE_RAISE( ERR_NOR_SUPPORT_COMPLICATE_GROUP_EXT );
                    break;   
            }
        }
    }

    if ( sPrev != NULL )
    {
        if ( sPrev->next == NULL )
        {
            sPrev->next = aNext;
        }
        else
        {
            /* Nothing to do */
        }
    }
    else
    {
        /* Nothing to do */

    }
    
    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_NOR_SUPPORT_COMPLICATE_GROUP_EXT );
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QMV_NOT_SUPPORT_COMPLICATE_GROUP_EXT));
    }
    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}
IDE_RC qmvGBGSTransform::modifyList( qcStatement        * aStatement,
                                     qmsSFWGH           * aSFWGH,
                                     qmsConcatElement  ** aPrev,
                                     qmsConcatElement   * aElement,
                                     qmsConcatElement   * aNext,
                                     qmsGroupByType       aType )
{
/***********************************************************************
 *
 * Description :
 *     Grouping Sets�� arguments�� List ����( Concatenated Group )�� ���ų�
 *     Grouping Sets�� arguments�� Rollup �Ǵ� Cube �ȿ� List ������ Group Element�� ���� ���
 *     LIST �� �����ϰ� ������ qmsConcatElement�� �����ؼ� LIST�� �ִ� �ڸ��� �����Ѵ�.
 *
 *     GROUP BY GROUPING SETS( i1, ( i2, i3 ), ROLLUP( i4, ( i5, i6 ) ) )
 *
 *     GROUP 1 :  i1, i2(NULL), i3(NULL), i4(NULL), i5(NULL), i6(NULL)
 *     GROUP 2 :  i1(NULL), i2, i3, i4(NULL), i5(NULL), i6(NULL)
 *     GROUP 3 :  i1(NULL), i2(NULL), i3(NULL), ROLLUP( i4, ( i5, i6 ) )
 *
 *     * NULL �� QMS_GROUPBY_NULL Type�� Group Element�� �ǹ��Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/
    qtcNode          * sNode;
    qtcNode          * sPrevNode = NULL;
    qmsConcatElement * sElement;

    IDU_FIT_POINT_FATAL( "qmvGBGSTransform::modifyList::__FT__" );

    for ( sNode  = ( qtcNode* )aElement->arithmeticOrList->node.arguments;
          sNode != NULL;
          sNode  = ( qtcNode* )sNode->node.next )
    {
        // LIST�� �����ϰ�, LIST�� arguments�� Group Element �� �����Ѵ�.

        // FIT TEST CODE
        IDU_FIT_POINT( "qmvGBGSTransform::modifyList::STRUCT_ALLOC::ERR_MEM",
                       idERR_ABORT_InsufficientMemory);

        IDE_TEST( STRUCT_ALLOC( QC_QMP_MEM( aStatement ),
                                qmsConcatElement,
                                &sElement ) != IDE_SUCCESS);

        sElement->type             = aType;
        sElement->arithmeticOrList = sNode;
        sElement->arguments        = NULL;
        sElement->next             = NULL;

        if ( *aPrev == NULL )
        {
            aSFWGH->group = sElement;
            *aPrev = sElement;
        }
        else
        {
            ( *aPrev )->next = sElement;
            *aPrev = ( *aPrev )->next;
        }

        /* BUG-45143 partitioned table�� ���� grouping set ���� ����� ����
         * group by groupings sets ( ( a, b ) ) --> group by a, b
         * ���� ���� ��ȯ�� List�� group by �������� ��ȯ��  List�� a Node�� Next�� b
         * �� �����Ǿ� �ִµ� group by �� ������� Next�� ���� �־���Ѵ�.
         */
        if ( sPrevNode != NULL )
        {
            sPrevNode->node.next = NULL;
        }
        else
        {
            /* Nothing to do */
        }
        sPrevNode = sNode;
    }

    if ( *aPrev != NULL )
    {
        if ( ( *aPrev )->next == NULL )
        {
            ( *aPrev )->next = aNext;
        }
        else
        {
            /* Nothing to do */
        }
    }
    else
    {
        /* Nothing to do */
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}
    
IDE_RC qmvGBGSTransform::modifyRollupCube( qcStatement       * aStatement,
                                           qmsSFWGH          * aSFWGH,
                                           qmsConcatElement ** aPrev,
                                           qmsConcatElement  * aElement,
                                           qmsConcatElement  * aNext )
{
/***********************************************************************
 *
 * Description :
 *     Grouping Sets�� arguments�� �� Rollup / Cube �� ������ ��
 *     Rollup / Cube�� ���� argument���� Rollup / Cube �� �־��� �ڸ��� �����ϰ�,
 *     Type�� QMS_GROUPBY_NULL �� �����Ѵ�.
 *
 *     GROUP BY GROUPING SETS( i1, ROLLUP( i2, i3 ), i4 )
 *
 *     GROUP 1 : i1, i2(NULL), i3(NULL), i4(NULL)
 *     GROUP 2 : i1(NULL), ROLLUP( i2, i3 ), i4(NULL)
 *     GROUP 3 : i1(NULL), i2(NULL), i3(NULL), i4
 *
 *     * NULL �� QMS_GROUPBY_NULL Type�� Group Element�� �ǹ��Ѵ�.
 *     
 * Implementation :
 *
 ***********************************************************************/
    qmsConcatElement * sElement;

    IDU_FIT_POINT_FATAL( "qmvGBGSTransform::modifyRollupCube::__FT__" );

    if ( *aPrev == NULL )
    {
        aSFWGH->group = aElement->arguments;
    } 
    else
    {
        ( *aPrev )->next = aElement->arguments;
    }

    for ( sElement  = aElement->arguments;
          sElement != NULL;
          sElement  = sElement->next )
    {
        // ROLLUP, CUBE�� Argument�� GROUPING SETS, ROLLUP, CUBE�� �� ���
        IDE_TEST_RAISE( sElement->type != QMS_GROUPBY_NORMAL, 
                        ERR_NOR_SUPPORT_COMPLICATE_GROUP_EXT );

        if ( ( sElement->arithmeticOrList->node.lflag & MTC_NODE_OPERATOR_MASK ) ==
             MTC_NODE_OPERATOR_LIST )
        {
            // LIST �� ��� ó��
            IDE_TEST( modifyList( aStatement,
                                  aSFWGH,
                                  aPrev,
                                  sElement,
                                  ( sElement->next == NULL ) ? aNext : sElement->next,
                                  QMS_GROUPBY_NULL )
                      != IDE_SUCCESS );
        }
        else
        {
            sElement->type = QMS_GROUPBY_NULL;
            *aPrev         = sElement;
        }
    }

    if ( *aPrev != NULL )
    {
        if ( ( *aPrev )->next == NULL )
        {
            ( *aPrev )->next = aNext;
        }
        else
        {
            /* Nothing to do */
        }
    }
    else
    {
        /* Nothing to do */
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_NOR_SUPPORT_COMPLICATE_GROUP_EXT );
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMV_NOT_SUPPORT_COMPLICATE_GROUP_EXT ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmvGBGSTransform::modifyFrom( qcStatement         * aStatement,
                                     qmsQuerySet         * aQuerySet,
                                     qmvGBGSQuerySetList * aQuerySetList )
{
/***********************************************************************
 *
 * Description :
 *     Grouping Sets Transform�ÿ� Group ���� ���� ��
 *     Query Set���� ������ ����κ�( SELECT * - FROM - WHERE - HIERARCHY )��
 *     SameViewReferencing ó���ؼ� �ߺ� ������ ���Ѵ�.
 *
 *     < BEFORE TRANSFORMATION >
 *
 *     SELECT i1, i2, i3
 *       FROM t1
 *      WHERE i1 >= 0 
 *     GROUP BY GROUPING SETS( i1, i2, i3 );
 *
 *     < AFTER TRANSFORMATION >
 *
 *     SELECT *
 *       FROM (
 *                SELECT i1, NULL, NULL            *Same View Referencing*
 *                  FROM ( SELECT * FROM t1 WHERE i1 >= 0 )<------
 *                GROUP BY i1                                  | |
 *                UNION ALL                                    | |
 *                SELECT NULL, i2, NULL                        | |
 *                  FROM ( SELECT * FROM t1 WHERE i1 >= 0 )----- |
 *                GROUP BY i2                                    |
 *                UNION ALL                                      |
 *                SELECT NULL, NULL, i3                          |
 *                  FROM ( SELECT * FROM t1 WHERE i1 >= 0 )-------
 *                GROUP BY i3
 *            );
 *
 * Implementation :
 * 
 ***********************************************************************/
    qmvGBGSQuerySetList * sQuerySetList;
    qcStatement           sStatement;    
    qmsParseTree        * sParseTree;
    qmsTarget           * sTarget;
    qmsFrom             * sFrom;
    SInt                  sStartOffset = 0;
    SInt                  sSize        = 0;

    IDU_FIT_POINT_FATAL( "qmvGBGSTransform::modifyFrom::__FT__" );

    // Partial Statement Parsing
    IDE_TEST( getPosForPartialQuerySet( aQuerySet,
                                        & sStartOffset,
                                        & sSize )
              != IDE_SUCCESS );
        
    for ( sQuerySetList  = aQuerySetList;
          sQuerySetList != NULL;
          sQuerySetList  = sQuerySetList->next )
    {
        // ���� inLineView�� �Ű��� ���� �ʱ�ȭ
        sQuerySetList->querySet->SFWGH->where         = NULL;
        sQuerySetList->querySet->SFWGH->hierarchy     = NULL;
        
        // ��Ÿ���� �ʱ�ȭ
        sQuerySetList->querySet->SFWGH->intoVariables = NULL;
        sQuerySetList->querySet->SFWGH->top           = NULL;

        QC_SET_STATEMENT( ( &sStatement ), aStatement, NULL );

        sStatement.myPlan->stmtText    = aQuerySet->startPos.stmtText;
        sStatement.myPlan->stmtTextLen = idlOS::strlen( aQuerySet->startPos.stmtText );
        
        IDE_TEST( qcpManager::parsePartialForQuerySet( &sStatement,
                                                       aQuerySet->startPos.stmtText,
                                                       sStartOffset,
                                                       sSize )
                  != IDE_SUCCESS );

        // FIT TEST
        IDU_FIT_POINT( "qmvGBGSTransform::modifyFrom::STRUCT_ALLOC1::ERR_MEM",
                       idERR_ABORT_InsufficientMemory);
        
        // Asterisk Target Node ����
        IDE_TEST( STRUCT_ALLOC( QC_QMP_MEM( aStatement ),
                                qmsTarget,
                                &sTarget ) != IDE_SUCCESS);

        QMS_TARGET_INIT( sTarget );

        // FIT TEST
        IDU_FIT_POINT( "qmvGBGSTransform::modifyFrom::STRUCT_ALLOC2::ERR_MEM",
                       idERR_ABORT_InsufficientMemory);

        IDE_TEST( STRUCT_ALLOC( QC_QMP_MEM( aStatement ),
                                qtcNode,
                                &sTarget->targetColumn ) != IDE_SUCCESS);

        QTC_NODE_INIT( sTarget->targetColumn );

        // make expression        
        IDE_TEST( qtc::makeTargetColumn( sTarget->targetColumn, 0, 0 ) != IDE_SUCCESS );
        
        sTarget->flag &= ~QMS_TARGET_ASTERISK_MASK;
        sTarget->flag |=  QMS_TARGET_ASTERISK_MASK;

        // ������ QuerySet�� �ʱ�ȭ
        sParseTree = ( qmsParseTree* )sStatement.myPlan->parseTree;
        
        sParseTree->querySet->SFWGH->target = sTarget;        
        sParseTree->querySet->SFWGH->group  = NULL;
        sParseTree->querySet->SFWGH->having = NULL;
        sParseTree->querySet->SFWGH->top    = NULL;
        sParseTree->querySet->SFWGH->intoVariables = NULL;
        
        // Grouping Sets Transformed MASK For Bottom Mtr View
        sParseTree->querySet->SFWGH->lflag &= ~QMV_SFWGH_GBGS_TRANSFORM_MASK;
        sParseTree->querySet->SFWGH->lflag |= QMV_SFWGH_GBGS_TRANSFORM_BOTTOM_MTR;

        // Partial Statement Parsing���� ������ QuerySet�� inLineView�� ���� 
        IDE_TEST( makeInlineView( aStatement,
                                  sQuerySetList->querySet->SFWGH,
                                  sParseTree->querySet ) 
                  != IDE_SUCCESS );
       
        // Middle View�� Outer SFWGH�� Simple View Merging���� �ʴ´�.
        sFrom = sQuerySetList->querySet->SFWGH->from;        
        IDE_TEST( setNoMerge( sFrom ) != IDE_SUCCESS );
        
        // Change Grouping Sets Transformed MASK From Bottom View To Middle View
        sQuerySetList->querySet->SFWGH->lflag &= ~QMV_SFWGH_GBGS_TRANSFORM_MASK;
        sQuerySetList->querySet->SFWGH->lflag |= QMV_SFWGH_GBGS_TRANSFORM_MIDDLE;        

        // Bottom View �� Simple View Merging ���� �ʴ´�.
        for ( sFrom  = sParseTree->querySet->SFWGH->from;
              sFrom != NULL;
              sFrom  = sFrom->next )
        {
            IDE_TEST( setNoMerge( sFrom ) 
                      != IDE_SUCCESS );        
        }

        // Same View Referencing
        if ( sQuerySetList->querySet != aQuerySetList->querySet )
        {
            sQuerySetList->querySet->SFWGH->from->tableRef->sameViewRef = 
                aQuerySetList->querySet->SFWGH->from->tableRef;
        }
        else
        {
            /* Nothing to do */
        } 
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmvGBGSTransform::unionQuerySets( qcStatement          * aStatement,
                                         qmvGBGSQuerySetList  * aQuerySetList,
                                         qmsQuerySet         ** aRet )
{
/*********************************************************************** *
 * Description :
 *     Query Set List�� Query Set ����
 *     �������� UNION ALL�� �����Ѵ�.
 *
 *     GROUP BY GROUPING SETS( i1, i2, i3, i4 )
 *
 *     GROUP BY i1                         UNION ALL
 *     UNION ALL                            /      \
 *     GROUP BY i2                 GROUP BY i1   UNION ALL
 *     UNION ALL                                  /      \
 *     GROUP BY i3                        GROUP BY i2   UNION ALL
 *     UNION ALL                                         /      \
 *     GROUP BY i4                               GROUP BY i3  GROUP BY i4
 *     
 * Implementation :
 *
 ***********************************************************************/

    qmvGBGSQuerySetList * sQuerySetList;
    qmsQuerySet         * sUnionAllQuerySet;

    IDU_FIT_POINT_FATAL( "qmvGBGSTransform::unionQuerySets::__FT__" );

    sQuerySetList = aQuerySetList;

    if ( sQuerySetList->next != NULL )
    {
        // FIT TEST
        IDU_FIT_POINT( "qmvGBGSTransform::unionQuerySets::STRUCT_ALLOC::ERR_MEM",
                       idERR_ABORT_InsufficientMemory);
        
        IDE_TEST( STRUCT_ALLOC( QC_QMP_MEM( aStatement ),
                                qmsQuerySet,
                                & sUnionAllQuerySet ) != IDE_SUCCESS);

        QCP_SET_INIT_QMS_QUERY_SET( sUnionAllQuerySet );

        sUnionAllQuerySet->setOp = QMS_UNION_ALL;
        sUnionAllQuerySet->left  = sQuerySetList->querySet;

        // Recursive Call For right of UNION ALL
        IDE_TEST( unionQuerySets( aStatement,
                                  sQuerySetList->next,
                                  & sUnionAllQuerySet->right ) 
                  != IDE_SUCCESS );
        
        *aRet = sUnionAllQuerySet;        
    }
    else
    {
        // UNION ALL �� ������ querySet
        *aRet = sQuerySetList->querySet;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
};

IDE_RC qmvGBGSTransform::makeInlineView( qcStatement * aStatement,
                                         qmsSFWGH    * aParentSFWGH,
                                         qmsQuerySet * aChildQuerySet )
{
/***********************************************************************
 *
 * Description :
 *     ParentSFWGH�� ChildQuerySet�� inLineView�� �����Ѵ�.
 *        
 * Implementation :        
 *
 ***********************************************************************/
    qcStatement    * sStatement;
    qmsParseTree   * sParseTree;
    qmsFrom        * sFrom;
    qmsTableRef    * sTableRef;
    qcNamePosition   sNullPosition;

    IDU_FIT_POINT_FATAL( "qmvGBGSTransform::makeInlineView::__FT__" );

    IDE_TEST_RAISE( aChildQuerySet == NULL, ERR_INVALID_ARGS );

    SET_EMPTY_POSITION( sNullPosition );

    // FIT TEST
    IDU_FIT_POINT( "qmvGBGSTransform::makeInlineView::STRUCT_ALLOC1::ERR_MEM",
                   idERR_ABORT_InsufficientMemory);
    
    IDE_TEST( STRUCT_ALLOC( QC_QMP_MEM( aStatement ),
                            qcStatement,
                            &sStatement ) != IDE_SUCCESS);

    // FIT TEST
    IDU_FIT_POINT( "qmvGBGSTransform::makeInlineView::STRUCT_ALLOC2::ERR_MEM",
                   idERR_ABORT_InsufficientMemory);    

    IDE_TEST( STRUCT_ALLOC( QC_QMP_MEM( aStatement ),
                            qmsParseTree,
                            &sParseTree ) != IDE_SUCCESS);

    QC_SET_INIT_PARSE_TREE( sParseTree, sNullPosition );
    
    // FIT TEST
    IDU_FIT_POINT( "qmvGBGSTransform::makeInlineView::STRUCT_ALLOC3::ERR_MEM",
                   idERR_ABORT_InsufficientMemory);    

    IDE_TEST( STRUCT_ALLOC( QC_QMP_MEM( aStatement ),
                            qmsFrom,
                            &sFrom ) != IDE_SUCCESS);

    QCP_SET_INIT_QMS_FROM( sFrom );

    SET_POSITION( sFrom->fromPosition, aParentSFWGH->from->fromPosition );

    // FIT TEST
    IDU_FIT_POINT( "qmvGBGSTransform::makeInlineView::STRUCT_ALLOC4::ERR_MEM", 
                   idERR_ABORT_InsufficientMemory);    

    IDE_TEST( STRUCT_ALLOC( QC_QMP_MEM( aStatement ),
                            qmsTableRef,
                            &sTableRef ) != IDE_SUCCESS);

    QCP_SET_INIT_QMS_TABLE_REF( sTableRef );

    sParseTree->withClause         = NULL;
    sParseTree->querySet           = aChildQuerySet;
    sParseTree->orderBy            = NULL;
    sParseTree->limit              = NULL;
    sParseTree->loopNode           = NULL;
    sParseTree->forUpdate          = NULL;
    sParseTree->queue              = NULL;
    sParseTree->isTransformed      = ID_FALSE;
    sParseTree->isSiblings         = ID_FALSE;
    sParseTree->isView             = ID_TRUE;
    sParseTree->isShardView        = ID_FALSE;
    sParseTree->common.currValSeqs = NULL;
    sParseTree->common.nextValSeqs = NULL;
    sParseTree->common.parse       = qmv::parseSelect;
    sParseTree->common.validate    = qmv::validateSelect;
    sParseTree->common.optimize    = qmo::optimizeSelect;
    sParseTree->common.execute     = qmx::executeSelect;
    
    QC_SET_STATEMENT( sStatement, aStatement, sParseTree );
    sStatement->myPlan->parseTree->stmtKind = QCI_STMT_SELECT;    

    aParentSFWGH->from                  = sFrom;
    aParentSFWGH->from->tableRef        = sTableRef;
    aParentSFWGH->from->tableRef->view  = sStatement;
    aParentSFWGH->from->tableRef->pivot = NULL;
    aParentSFWGH->from->next            = NULL;

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_INVALID_ARGS )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmvGBGSTransform::makeInlineView",
                                  "Invalid Arguments" ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmvGBGSTransform::modifyOrgSFWGH( qcStatement * aStatement,
                                         qmsSFWGH    * aSFWGH )
{
/***********************************************************************
 *
 * Description :
 *     Query Set �� Target�� Asterisk Node�� ���� �ϰ�
 *     HINT, TOP, INTO �� ������ ���� View�� �Ű��� SFWGH�� ������ �ʱ�ȭ �Ѵ�.

 * Implementation :
 *
 ***********************************************************************/
    qmsTarget * sTarget;

    IDU_FIT_POINT_FATAL( "qmvGBGSTransform::modifyOrgSFWGH::__FT__" );

    // FIT TEST
    IDU_FIT_POINT( "qmvGBGSTransform::modifyOrgSFWGH::STRUCT_ALLOC1::ERR_MEM",
                   idERR_ABORT_InsufficientMemory);
    
    IDE_TEST( STRUCT_ALLOC( QC_QMP_MEM( aStatement ),
                            qmsTarget,
                            &sTarget ) != IDE_SUCCESS);

    QMS_TARGET_INIT( sTarget );

    // FIT TEST
    IDU_FIT_POINT( "qmvGBGSTransform::modifyOrgSFWGH::STRUCT_ALLOC2::ERR_MEM",
                   idERR_ABORT_InsufficientMemory);    

    IDE_TEST( STRUCT_ALLOC( QC_QMP_MEM( aStatement ),
                            qtcNode,
                            &sTarget->targetColumn ) != IDE_SUCCESS);

    QTC_NODE_INIT( sTarget->targetColumn );

    IDE_TEST( qtc::makeTargetColumn( sTarget->targetColumn,
                                     0,
                                     0 ) != IDE_SUCCESS );
    
    sTarget->flag &= ~QMS_TARGET_ASTERISK_MASK;
    sTarget->flag |=  QMS_TARGET_ASTERISK_MASK;
    
    aSFWGH->target    = sTarget;
    aSFWGH->where     = NULL;
    aSFWGH->hierarchy = NULL;
    aSFWGH->group     = NULL;
    aSFWGH->having    = NULL;
    
    // Grouping Sets Transformed MASK 
    aSFWGH->lflag &= ~QMV_SFWGH_GBGS_TRANSFORM_MASK;
    aSFWGH->lflag |= QMV_SFWGH_GBGS_TRANSFORM_TOP;

    // Org SFWGH�� Simple View Merging ���� �ʴ´�.
    IDE_TEST( setNoMerge( aSFWGH->from ) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmvGBGSTransform::modifyOrderBy( qcStatement         * aStatement,
                                        qmvGBGSQuerySetList * aQuerySetList )
{
/***********************************************************************
 *
 * Description :
 *     OrderBy ���� ó���� ���� Transform�� �����Ѵ�.
 *
 *               << Before Transformation >>     
 *                  SELECT a.i1, a.i2 X
 *                    FROM t1 a
 *                GROUP BY GROUPING SETS( a.i1, a.i2 )
 *                ORDER BY a.i1 DESC, X ASC
 *                LIMIT 3;
 *
 *     ���� Query�� GBGSTransform ������ ���� �Ʒ��� ���� Validation�Ǿ��� �� �̴�.
 *
 *               << After Transformation >>
 *                  SELECT i1, X
 *                    FROM (
 *                          SELECT a.i1, NULL X
 *                            FROM ( SELECT * FROM t1 a )
 *                        GROUP BY a.i1              
 *                          SELECT NULL, a.i2 X         
 *                            FROM ( SELECT * FROM t1 a )
 *                        GROUP BY a.i2              
 *                         ) 
 *                ORDER BY a.i1 DESC, X ASC
 *                LIMIT 3;
 *                         ^ ���⼭ GroupingSets Transform���� ���� �� inLineView ������
 *                           TableAliasName�� a�� ã�� �� ����.
 *
 *     �̿����� �ذ�å���� ������ ���� Transform �Ѵ�.
 * 
 *     SELECT i1, X // 3. expandAllTarget�� �߰� �� ORDER BY�� Node�� ������ ������ TargetList �� �����´�.
 *       FROM (
 *                SELECT a.i1, NULL X, a.i1, X // 1. ORDER BY���� Node���� Target List�� �߰��Ѵ�.
 *                  FROM ( SELECT * FROM t1 a )
 *                GROUP BY a.i1                  
 *                SELECT NULL, a.i2 X, a.i1, X    
 *                  FROM ( SELECT * FROM t1 a )
 *              ORDER BY 3 DESC, 4 ASC, // 2. ORDER BY�� Limit�� inLineView ������ ����ְ� 
 *              LIMIT 3                       ORDER BY�� Node�� �߰��� Targe Node�� Position���� �����Ѵ�.     
 *            ); 
 *
 *            
 *    * Org Query�� Set Operator�� �����Ѵٸ� OrderBy�� Transform�� �������� �ʴ´�.
 *    
 * Implementation :  
 *
 ***********************************************************************/
    extern mtdModule      mtdSmallint;

    qmsParseTree        * sOrgParseTree;
    qmsParseTree        * sTransformedParseTree;

    qmvGBGSQuerySetList * sQuerySetList;
    qmsSortColumns      * sSortList;
    qmsTarget           * sLastTarget;
    qmsTarget           * sNewTarget;
    qmsTarget           * sTarget;
    qtcNode             * sLastNode = NULL;
    qtcNode             * sNewNode[2];    
    qtcNode             * sNode;
    UInt                  sTargetPosition = 1;
    SInt                  sStartOffset = 0;
    SInt                  sSize = 0;
    
    /* Target�� �߰��Ǵ� OrderBy Node�� ������ ���� Target Node�� ������ ���� 9999999999 ���� ���� ���Ѵ�. */
    SChar                 sValue[ QMV_GBGS_TRANSFORM_MAX_TARGET_COUNT + 1 ];

    const mtcColumn*      sColumn;
    qcStatement           sStatement; 
    qcNamePosition        sPosition;
    idBool                sIsIndicator;

    IDU_FIT_POINT_FATAL( "qmvGBGSTransform::modifyOrderBy::__FT__" );

    SET_EMPTY_POSITION( sPosition );
    QC_SET_STATEMENT( ( &sStatement ), aStatement, NULL );

    /* sOrgParseTree -> SELECT TOP 3 *
     *                    FROM (
     * sTransformedParseTree->     SELECT t1.i1, NULL, NULL
     *                               FROM (
     *                                        SELECT * FROM t1, t2...
     *                                    )
     *                           GROUP BY t1.i1
     *                           UNION ALL
     *                           ...
     *                         )
     */                         
    sOrgParseTree = ( qmsParseTree * )aStatement->myPlan->parseTree;

    // Org Statement�� SetOerator�� ���� ��� OrderBy�� Transfrom�� �������� �ʴ´�. 
    if ( ( sOrgParseTree->orderBy != NULL ) && 
         ( sOrgParseTree->querySet->setOp == QMS_NONE ) )
    {
        // OrderBy�� Node�� Partial Parsing �ؼ� Target�� �߰��Ѵ�.
        IDE_TEST( getPosForOrderBy( sOrgParseTree->orderBy,
                                    &sStartOffset,
                                    &sSize ) != IDE_SUCCESS );
        
        // QuerySetList�� QuerySet�鿡 Order By�� Node�� Target���� �߰��Ѵ�.
        for ( sQuerySetList  = aQuerySetList;
              sQuerySetList != NULL;
              sQuerySetList  = sQuerySetList->next )
        {
            // Target�� ������ Node�� Count�� ���Ѵ�.
            for ( sLastTarget        = sQuerySetList->querySet->SFWGH->target, sTargetPosition = 1;
                  sLastTarget->next != NULL;
                  sLastTarget        = sLastTarget->next )
            {
                sTargetPosition++;
            }
            
            sStatement.myPlan->stmtText = sQuerySetList->querySet->startPos.stmtText;
            sStatement.myPlan->stmtTextLen = idlOS::strlen( sQuerySetList->querySet->startPos.stmtText );
            
            IDE_TEST( qcpManager::parsePartialForOrderBy( &sStatement,
                                                          sQuerySetList->querySet->startPos.stmtText,
                                                          sStartOffset,
                                                          sSize )
                      != IDE_SUCCESS );

            sNewTarget = ( ( qmsParseTree* )sStatement.myPlan->parseTree )->querySet->target;
            
            sLastTarget->next = sNewTarget;
            sLastTarget->targetColumn->node.next = &sNewTarget->targetColumn->node;

            // �߰��� Target Node�� valueMode�̸鼭 SmallInt���� Node( ORDER BY�� Position Node )�� �����Ѵ�.
            for ( sTarget  = sNewTarget;
                  sTarget != NULL;
                  sTarget  = sTarget->next )
            {
                sNode = sTarget->targetColumn;
                
                // Indicator���� Ȯ���Ѵ�.
                if ( qtc::isConstValue( QC_SHARED_TMPLATE(aStatement), sNode ) == ID_TRUE )
                {
                    sColumn = QTC_STMT_COLUMN( aStatement, sNode );

                    if ( sColumn->module == &mtdSmallint )
                    {
                        sIsIndicator = ID_TRUE;
                    }
                    else
                    {
                        sIsIndicator = ID_FALSE;
                    }
                }
                else
                {
                    sIsIndicator = ID_FALSE;
                }

                if ( sIsIndicator == ID_TRUE )
                {
                    // Indicator�� ��� Target���� ����                     
                    sLastTarget->next = sTarget->next; 
                }
                else
                {
                    sLastTarget = sTarget;
                    sNode->lflag &= ~QTC_NODE_GBGS_ORDER_BY_NODE_MASK;
                    sNode->lflag |= QTC_NODE_GBGS_ORDER_BY_NODE_TRUE;
                }
            }
        }

        // ORDER BY �� Node�� Target�� �߰��� Node�� ����Ű�� Position���� �����Ѵ�.
        for ( sSortList  = sOrgParseTree->orderBy;  
              sSortList != NULL;
              sSortList  = sSortList->next )
        {
            sNode = sSortList->sortColumn;
            
            // Indicator���� Ȯ���Ѵ�.
            if ( qtc::isConstValue( QC_SHARED_TMPLATE(aStatement), sNode ) == ID_TRUE )
            {
                sColumn = QTC_STMT_COLUMN( aStatement, sNode );

                if ( sColumn->module == &mtdSmallint )
                {
                    sIsIndicator = ID_TRUE;
                }
                else
                {
                    sIsIndicator = ID_FALSE;
                }
            }
            else
            {
                sIsIndicator = ID_FALSE;
            }

            if ( sIsIndicator == ID_FALSE ) 
            {
                sTargetPosition++;

                // OrderBy Node�� Indicator�� �����Ѵ�.
                idlOS::snprintf( sValue,
                                 QMV_GBGS_TRANSFORM_MAX_TARGET_COUNT + 1,
                                 "%"ID_UINT32_FMT,
                                 sTargetPosition );            

                IDE_TEST( qtc::makeValue( aStatement,
                                          sNewNode,
                                          ( const UChar* )"SMALLINT",
                                          8,
                                          &sPosition,
                                          ( const UChar* )sValue,
                                          idlOS::strlen( sValue ),
                                          MTC_COLUMN_NORMAL_LITERAL ) 
                          != IDE_SUCCESS );
                
                sNewNode[0]->lflag &= ~QTC_NODE_GBGS_ORDER_BY_NODE_MASK;
                sNewNode[0]->lflag |= QTC_NODE_GBGS_ORDER_BY_NODE_TRUE;
                    
                sSortList->sortColumn = sNewNode[0];
            }
            else
            {
                /* Nothing to do */
            }
            
            // mtcNode�� ����            
            if ( sSortList == sOrgParseTree->orderBy )
            {
                // First Loop
                sLastNode = sSortList->sortColumn;
            }
            else
            {
                if ( sLastNode != NULL )
                {
                    sLastNode->node.next = &( sSortList->sortColumn->node );
                    sLastNode = ( qtcNode * )sLastNode->node.next;
                }
                else
                {
                    // Compile Warning ������ �߰�
                    IDE_RAISE( ERR_INVALID_POINTER );
                }                
            }
        }        

        sTransformedParseTree = ( qmsParseTree * )
            sOrgParseTree->querySet->SFWGH->from->tableRef->view->myPlan->parseTree;
    
        /* Order By �� Limit�� View������ �о� �ִ´�. */
        sTransformedParseTree->orderBy = sOrgParseTree->orderBy;
        sTransformedParseTree->limit   = sOrgParseTree->limit;
        sOrgParseTree->orderBy   = NULL;
        sOrgParseTree->limit     = NULL;                        
    }
    else
    {
        /* Nothing to do */
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_INVALID_POINTER )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmvGBGSTransform::modifyOrderBy",
                                  "Invalid Pointer" ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmvGBGSTransform::removeNullGroupElements( qmsSFWGH * aSFWGH )
{
/***********************************************************************
 *
 * Description :
 *     Group By�� Group Elements �� QMS_GROUPBY_NULL Type�� 
 *     Element�� �����Ѵ�.
 *     
 * Implementation :
 *
 ***********************************************************************/

    qmsConcatElement * sGroup     = NULL;
    qmsConcatElement * sPrevGroup = NULL;

    IDU_FIT_POINT_FATAL( "qmvGBGSTransform::removeNullGroupElements::__FT__" );

    for ( sGroup  = aSFWGH->group;
          sGroup != NULL;
          sGroup  = sGroup->next )
    {
        if ( sGroup->type == QMS_GROUPBY_NULL )
        {
            if ( sPrevGroup == NULL )
            {
                aSFWGH->group = sGroup->next;
            }
            else
            {
                sPrevGroup->next = sGroup->next;
            }
        }
        else
        {
            sPrevGroup = sGroup;
        }
    }

    IDE_TEST_RAISE( aSFWGH->group == NULL,
                    ERR_GROUP_IS_NULL );

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_GROUP_IS_NULL )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmvGBGSTransform::removeNullGroupElements",
                                  "Group is null." ) );
    }
    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

IDE_RC qmvGBGSTransform::setNoMerge( qmsFrom * aFrom )
{

    IDU_FIT_POINT_FATAL( "qmvGBGSTransform::setNoMerge::__FT__" );

    if ( aFrom->joinType != QMS_NO_JOIN )
    {
        IDE_TEST( setNoMerge( aFrom->left  ) != IDE_SUCCESS );
        IDE_TEST( setNoMerge( aFrom->right ) != IDE_SUCCESS );        
    }
    else
    {
        aFrom->tableRef->noMergeHint = ID_TRUE;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


IDE_RC qmvGBGSTransform::makeNullNode( qcStatement * aStatement,
                                       qtcNode     * aExpression )
{
/***********************************************************************
 *
 * Description :
 *     Grouping Sets Transform ���� QMS_GROUPBY_NULL Type���� ������
 *     Group Expression �� Equivalent �� Target �Ǵ� Having�� Expression��
 *     ���ڷ� �޾� �ش� Type�� Null Value�� ���� ��Ų��.
 *     
 * Implementation :
 *
 ***********************************************************************/    
    mtcColumn       * sColumn;
    qtcNode         * sNullNode[2];    
    qcNamePosition    sPosition;

    IDU_FIT_POINT_FATAL( "qmvGBGSTransform::makeNullNode::__FT__" );

    sColumn = QTC_STMT_COLUMN( aStatement, aExpression );
    SET_EMPTY_POSITION( sPosition );    

    // Null Value Node ����
    IDE_TEST( qtc::makeValue( aStatement,
                              sNullNode,
                              ( const UChar* )sColumn->module->names->string,
                              sColumn->module->names->length,
                              &sPosition,
                              ( const UChar* )"NULL",
                              0, // ��� Type�� Size�� 0�� �� NULL ������ �����ȴ�.
                              MTC_COLUMN_NORMAL_LITERAL )
              != IDE_SUCCESS );
    
    IDE_TEST( qtc::estimate( sNullNode[0],
                             QC_SHARED_TMPLATE( aStatement ),
                             aStatement,
                             NULL,
                             NULL,
                             NULL )
              != IDE_SUCCESS );

    aExpression->lflag           = sNullNode[0]->lflag;
    aExpression->node.table      = sNullNode[0]->node.table;
    aExpression->node.column     = sNullNode[0]->node.column;    
    aExpression->node.baseTable  = sNullNode[0]->node.table;
    aExpression->node.baseColumn = sNullNode[0]->node.column;
    aExpression->node.module     = &qtc::valueModule;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmvGBGSTransform::makeNullOrPassNode( qcStatement      * aStatement,
                                             qtcNode          * aExpression,
                                             qmsConcatElement * aGroup,
                                             idBool             aMakePassNode )
{
/***********************************************************************
 *
 * Description :
 *    Grouping Sets Transform���� ������ QMS_GROUPBY_NULL Type�� Group Element�� �����Ǵ�
 *    Target �� Having�� Expression�� NullNode�� ���� ��Ű�ų� �ٸ�PassNode ���ٶ� ���Ѵ�.
 *
 *        SELECT i1, i2, i3
 *          FROM t1
 *      GROUP BY GROUPING SETS ( i1, i2, i3 ), i1;
 *
 *      ���� Query���� i1 ó�� GROUPING SETS�� ���ڷ� �� Group Expression��
 *      GROUPING SETS �ۿ��� Partial Group���� ��� �� ��� �����Ǵ� GROUPING�� ������ ����.
 *
 *          GROUP 1 - i1(QMS_GROUPBY_NORMAL), i2(QMS_GROUPBY_NULL), i3(QMS_GROUPBY_NULL), i1(QMS_GROUPBY_NORMAL)
 *          GROUP 2 - i1(QMS_GROUPBY_NULL), i2(QMS_GROUPBY_NORMAL), i3(QMS_GROUPBY_NULL), i1(QMS_GROUPBY_NORMAL)
 *          GROUP 3 - i1(QMS_GROUPBY_NULL), i2(QMS_GROUPBY_NULL), i3(QMS_GROUPBY_NORMAL), i1(QMS_GROUPBY_NORMAL)
 *
 *      ���� ����ó�� Partial Group�� GROUPING SETS�� �й�Ǿ� ��� GROUPING �� ���� �Ǵµ�,
 *
 *      1. Target �Ǵ� Having �� Group Key�� �Ǵ� Expression�� Equivalent��
 *         QMS_GROUPBY_NORMAL Type�� Group�� �ִٸ� �ش� Group�� PassNode �� �ٶ󺸰� �����ϰ�,
 *      
 *      2. Equivalent�� QMS_GROUPBY_NORMAL Type�� Expression�� ���ٸ�,
 *         QMS_GROUPBY_NULL Type�� Group�� Equivalent �ϸ� �ش� Expression�� NullNode�� �����Ѵ�.
 *         
 * Implementation :
 *
 ***********************************************************************/    
    qmsConcatElement * sGroup;
    qmsConcatElement * sSubGroup;
    qtcNode          * sListElement;
    qtcNode          * sAlterPassNode = NULL;
    qtcNode          * sPassNode = NULL;
    idBool             sIsNormalGroup = ID_FALSE;
    idBool             sIsNormalGroupTmp = ID_FALSE;

    IDU_FIT_POINT_FATAL( "qmvGBGSTransform::makeNullOrPassNode::__FT__" );

    for ( sGroup  = aGroup;
          sGroup != NULL;
          sGroup  = sGroup->next )
    {
        // ��� Group Element�� �˻�
        if ( sGroup->type == QMS_GROUPBY_NORMAL )
        {
            IDE_TEST( qtc::isEquivalentExpression( aStatement,
                                                   sGroup->arithmeticOrList,
                                                   aExpression,
                                                   &sIsNormalGroupTmp )
                      != IDE_SUCCESS );

            if ( sIsNormalGroupTmp == ID_TRUE )
            {
                // QMS_GROUPBY_NORMAL Type�� Group�� ã�����
                // PassNode�� �����ϰ� break
                sIsNormalGroup = ID_TRUE;                
                sAlterPassNode = sGroup->arithmeticOrList;
                break;
            }
            else
            {
                /* Nothing to do */
            }
        }
        else if ( ( sGroup->type == QMS_GROUPBY_ROLLUP ) ||
                  ( sGroup->type == QMS_GROUPBY_CUBE   ) )
        {
            for ( sSubGroup  = sGroup->arguments;
                  sSubGroup != NULL ;
                  sSubGroup = sSubGroup->next )
            {
                IDE_DASSERT( sSubGroup->arithmeticOrList != NULL );

                /* *********************************
                 * Concatenated Expression
                 * *********************************/
                if ( ( sSubGroup->arithmeticOrList->node.lflag & MTC_NODE_OPERATOR_MASK )
                     == MTC_NODE_OPERATOR_LIST )
                {
                    
                    for ( sListElement  = ( qtcNode* )sSubGroup->arithmeticOrList->node.arguments;
                          sListElement != NULL;  
                          sListElement  = ( qtcNode* )sListElement->node.next )
                    {
                        IDE_TEST( qtc::isEquivalentExpression( aStatement,
                                                               sListElement,
                                                               aExpression,
                                                               &sIsNormalGroupTmp )
                                  != IDE_SUCCESS );
                        
                        if ( sIsNormalGroupTmp == ID_TRUE )
                        {
                            sIsNormalGroup = ID_TRUE;
                            sAlterPassNode = sListElement;
                            break;
                        }
                        else
                        {
                            /* Nothing to do */
                        }
                    }
                }
                else
                {
                    IDE_TEST( qtc::isEquivalentExpression( aStatement,
                                                           sSubGroup->arithmeticOrList,
                                                           aExpression,
                                                           &sIsNormalGroupTmp )
                              != IDE_SUCCESS );
                    
                    if ( sIsNormalGroupTmp == ID_TRUE )
                    {
                        sIsNormalGroup = ID_TRUE;
                        sAlterPassNode  = sSubGroup->arithmeticOrList;
                        break;
                    }
                    else
                    {
                        /* Nothing to do */
                    }
                }

                if ( sIsNormalGroupTmp == ID_TRUE )
                {
                    break;
                }
                else
                {
                    /* Nothing to do */
                }
            }
        }
        else
        {
            /* Nothing to do */
        }
    }

    if ( sIsNormalGroup != ID_TRUE )
    {
        // Make Null Node        
        IDE_TEST( makeNullNode( aStatement, aExpression ) != IDE_SUCCESS );
    }
    else
    {
        IDE_DASSERT( sAlterPassNode != NULL );

        if ( aMakePassNode == ID_TRUE )
        {
            IDE_TEST( qtc::makePassNode( aStatement,
                                         aExpression,
                                         sAlterPassNode,
                                         &sPassNode )
                      != IDE_SUCCESS );

            IDE_DASSERT( aExpression == sPassNode );
        }
        else
        {
            /* Nothing to do */
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmvGBGSTransform::doTransform( qcStatement * aStatement,
                                      qmsQuerySet * aQuerySet )
{
    qmsConcatElement    * sGroupingSets = NULL;
    qmsConcatElement    * sPrevGroup = NULL;
    qmvGBGSQuerySetList * sQuerySetList = NULL;
    qmsQuerySet         * sUnionQuerySet = NULL;

    IDU_FIT_POINT_FATAL( "qmvGBGSTransform::doTransform::__FT__" );

    // GroupBy ������ Grouping Sets�� ã�´�.    
    IDE_TEST( searchGroupingSets( aQuerySet, &sGroupingSets, &sPrevGroup ) 
              != IDE_SUCCESS );

    if ( sGroupingSets != NULL )
    {
        // Grouping Sets�� Argument�� �ϳ� ���
        // Normal GroupBy�� ���� ������, Grouping Sets�� �����Ѵ�.         
        if ( sGroupingSets->arguments->next == NULL )
        {
            // FIT TEST
            IDU_FIT_POINT( "qmvGBGSTransform::doTransform::STRUCT_ALLOC::ERR_MEM",
                           idERR_ABORT_InsufficientMemory);
            
            IDE_TEST( STRUCT_ALLOC( QC_QMP_MEM( aStatement ),
                                    qmvGBGSQuerySetList,
                                    &sQuerySetList ) != IDE_SUCCESS);

            sQuerySetList->querySet = aQuerySet;
            sQuerySetList->next     = NULL;

            IDE_TEST( modifyGroupBy( aStatement,
                                     sQuerySetList )
                      != IDE_SUCCESS );
        }
        else
        {
            // 1. Query Statement�� Partial Parsing�� ����
            //    ���� �� Group�� �� ��ŭ QuerySet�� �����Ѵ�.
            IDE_TEST( copyPartialQuerySet( aStatement,
                                           sGroupingSets,
                                           aQuerySet,
                                           &sQuerySetList )
                      != IDE_SUCCESS );
            
            // 2. ���� �� �� QuerySet�� GroupBy �ڷᱸ���� �����Ѵ�.
            IDE_TEST( modifyGroupBy( aStatement, 
                                     sQuerySetList ) 
                      != IDE_SUCCESS );
            
            // ( 3. Same View Refrencing ó���� ����
            //      ���� �� �� QuerySet���� InLineView�� �����Ѵ�. )
            //
            // "GROUPING_SETS_MTR" Hint �� ���� ��쿡�� ����
            if ( aQuerySet->SFWGH->hints != NULL )
            {   
                if ( aQuerySet->SFWGH->hints->GBGSOptViewMtr == ID_TRUE )
                {
                    IDE_TEST( modifyFrom( aStatement, 
                                          aQuerySet, 
                                          sQuerySetList ) 
                              != IDE_SUCCESS );
                }
                else
                {
                    /* Nothing to do */
                }
            }
            else
            {
                /* Nothing to do */
            }            
            
            // 4. ���� �� QuerySet���� UNION ALL�� �����Ѵ�.
            IDE_TEST( unionQuerySets( aStatement,
                                      sQuerySetList,
                                      &sUnionQuerySet )
                      != IDE_SUCCESS );

            // 5. UNION ALL�� ���� �� QuerySet���� ���� QuerySet��
            //    InlineView�� �����.
            IDE_TEST( makeInlineView( aStatement,
                                      aQuerySet->SFWGH,
                                      sUnionQuerySet )
                      != IDE_SUCCESS );
            
            // 6. ���� QuerySet�� Target�� Asterisk Node�� �����ϰ�,
            //    HINT, DISTINCT, TOP, INTO, FROM �� ������
            //    ������ ������ �ʱ�ȭ �Ѵ�.
            IDE_TEST( modifyOrgSFWGH( aStatement,
                                      aQuerySet->SFWGH )
                      != IDE_SUCCESS );

            // 7. OrderBy�� Node�� Target���� �����ϰ�
            //    OrderBy�� Node���� ������ Target Node��
            //    Position ���� �����Ѵ�.
            IDE_TEST( modifyOrderBy( aStatement,
                                     sQuerySetList )
                      != IDE_SUCCESS );
        }
    }
    else
    {
        /* Nothing to do */
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
};
