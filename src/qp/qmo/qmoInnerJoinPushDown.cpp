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
 *     BUG-43039 inner join push down
 *
 * ��� ���� :
 *
 * ��� :
 *
 **********************************************************************/

#include <qmoInnerJoinPushDown.h>

IDE_RC qmoInnerJoinPushDown::doTransform( qcStatement * aStatement,
                                          qmsQuerySet * aQuerySet )
{
    qmsFrom     * sFrom;
    UInt          sFromCount = 0;

    IDE_TEST_CONT( aQuerySet->SFWGH->where == NULL, skip );

    /* PROJ-2509 Hierarchy Query Join
     * Hierarchy Query�� Innerjoin�� PushDown�� �ϰԵǸ� Hierarchy Query��
     * Filter �� ó���Ǿ���� Where���� predicate�� Join ���� ��������
     * ����� �ٸ��� ������ �ǹǷ� InnerJoin PushDown�� ���� �ʵ��� �Ѵ�.
     */
    IDE_TEST_CONT( aQuerySet->SFWGH->hierarchy != NULL, skip );

    for( sFrom = aQuerySet->SFWGH->from;
         sFrom != NULL;
         sFrom = sFrom->next )
    {
        if ( sFrom->joinType == QMS_NO_JOIN )
        {
            ++sFromCount;
        }
        else
        {
            // Nothing to do.
        }
    }

    for ( ; sFromCount > 0; --sFromCount )
    {
        IDE_TEST( pushInnerJoin( aStatement, aQuerySet ) != IDE_SUCCESS );
    }

    IDE_EXCEPTION_CONT( skip );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmoInnerJoinPushDown::pushInnerJoin( qcStatement * aStatement,
                                            qmsQuerySet * aQuerySet )
{
    idBool        sFind     = ID_FALSE;
    qmsFrom     * sFindFrom;
    qmsFrom     * sPushFrom;
    qmsFrom    ** sPrevFrom;
    qmsFrom     * sParent   = NULL;
    qmsFrom     * sTemp;
    qtcNode     * sNode     = aQuerySet->SFWGH->where;
    qtcNode    ** sPrevNode;
    qcDepInfo     sPushDep;

    //-------------------------------------------------
    // pushInnerJoin �� ��ġ�� ã��
    //-------------------------------------------------

    for( sTemp = aQuerySet->SFWGH->from;
         sTemp != NULL;
         sTemp = sTemp->next )
    {
        if ( sTemp->joinType == QMS_LEFT_OUTER_JOIN )
        {
            sFindFrom = sTemp;

            while ( sFindFrom != NULL )
            {
                if ( sFindFrom->joinType == QMS_NO_JOIN )
                {
                    sFind = ID_TRUE;
                    break;
                }
                else if ( sFindFrom->joinType == QMS_INNER_JOIN )
                {
                    sFindFrom = sFindFrom->left;
                }
                else if ( sFindFrom->joinType == QMS_LEFT_OUTER_JOIN )
                {
                    sFindFrom = sFindFrom->left;
                }
                else
                {
                    break;
                }
            }

            if ( sFind == ID_TRUE )
            {
                break;
            }
            else
            {
                // Nothing to do.
            }
        }
        else
        {
            // Nothing to do.
        }
    }

    IDE_TEST_CONT( sFind == ID_FALSE, skip );


    //-------------------------------------------------
    // pushInnerJoin ���� ������ ���� ������Ŷ�� ã��
    // �ٸ� ���� ���̺��� ������ø� ����
    //-------------------------------------------------

    // BUG-43920 or�� �������� ������� ����
    IDE_TEST_CONT( (sNode->node.lflag & MTC_NODE_OPERATOR_MASK) != MTC_NODE_OPERATOR_AND, skip );

    qtc::dependencyClear( &sPushDep );

    for ( sPrevNode = (qtcNode**)&(sNode->node.arguments),
          sNode = (qtcNode*)sNode->node.arguments;
          sNode != NULL;
          sPrevNode = (qtcNode**)&(sNode->node.next),
          sNode = (qtcNode*)(sNode->node.next) )
    {
        // semi, anti ������ �����Ѵ�.
        if ( (sNode->lflag & QTC_NODE_JOIN_TYPE_MASK) != QTC_NODE_JOIN_TYPE_INNER )
        {
            continue;
        }
        else
        {
            // Nothing to do.
        }

        // BUG-43079 ���� ������Ŷ�� �ƴ� ��쿡�� skip �ؾ� �մϴ�.
        if ( qtc::getCountBitSet( &(sNode->depInfo) ) != 2 )
        {
            continue;
        }
        else
        {
            // Nothing to do.
        }

        if ( (sNode->node.lflag & MTC_NODE_LOGICAL_CONDITION_MASK)
             == MTC_NODE_LOGICAL_CONDITION_TRUE )
        {
            continue;
        }
        else
        {
            // Nothing to do.
        }

        if ( qtc::dependencyContains( &(sNode->depInfo),
                                      &(sFindFrom->depInfo) ) == ID_TRUE )
        {
            qtc::dependencyMinus( &(sNode->depInfo),
                                  &(sFindFrom->depInfo),
                                  &sPushDep );
            break;
        }
        else
        {
            // Nothing to do.
        }
    }

    IDE_TEST_CONT( qtc::haveDependencies( &sPushDep ) == ID_FALSE, skip );


    //-------------------------------------------------
    // push�� ���̺��� ã�Ƴ�
    //-------------------------------------------------
    for( sPrevFrom = &(aQuerySet->SFWGH->from),
         sPushFrom = aQuerySet->SFWGH->from;
         sPushFrom != NULL;
         sPrevFrom = &(sPushFrom->next),
         sPushFrom = sPushFrom->next )
    {
        // semi, anti ������ �����Ѵ�.
        if( qtc::haveDependencies( &(sPushFrom->semiAntiJoinDepInfo) ) == ID_TRUE )
        {
            continue;
        }
        else
        {
            // Nothing to do.
        }

        if( sPushFrom->joinType != QMS_NO_JOIN )
        {
            continue;
        }
        else
        {
            // Nothing to do.
        }

        if ( qtc::dependencyEqual( &sPushDep, &(sPushFrom->depInfo) ) == ID_TRUE )
        {
            break;
        }
        else
        {
            // Nothing to do.
        }
    }

    IDE_TEST_CONT( sPushFrom == NULL, skip );


    //-------------------------------------------------
    // ���� ������ �߶󳽴�.
    //-------------------------------------------------
    *sPrevNode       = (qtcNode*)sNode->node.next;
    sNode->node.next = NULL;

    *sPrevFrom       = sPushFrom->next;
    sPushFrom->next  = NULL;

    //-------------------------------------------------
    // INNER ���� ����
    //-------------------------------------------------
    sFind = ID_FALSE;

    for( sTemp = aQuerySet->SFWGH->from;
         sTemp != NULL;
         sTemp = sTemp->next )
    {
        if ( sTemp->joinType == QMS_LEFT_OUTER_JOIN )
        {
            sFindFrom = sTemp;

            while ( sFindFrom != NULL )
            {
                if ( sFindFrom->joinType == QMS_NO_JOIN )
                {
                    sFind = ID_TRUE;
                    break;
                }
                else if ( sFindFrom->joinType == QMS_INNER_JOIN )
                {
                    IDE_TEST( qtc::dependencyOr( &(sPushFrom->depInfo),
                                                 &(sFindFrom->depInfo),
                                                 &(sFindFrom->depInfo) ) != IDE_SUCCESS );

                    sParent   = sFindFrom;
                    sFindFrom = sFindFrom->left;
                }
                else if ( sFindFrom->joinType == QMS_LEFT_OUTER_JOIN )
                {
                    IDE_TEST( qtc::dependencyOr( &(sPushFrom->depInfo),
                                                 &(sFindFrom->depInfo),
                                                 &(sFindFrom->depInfo) ) != IDE_SUCCESS );

                    sParent   = sFindFrom;
                    sFindFrom = sFindFrom->left;
                }
                else
                {
                    break;
                }
            }

            if ( sFind == ID_TRUE )
            {
                break;
            }
            else
            {
                // Nothing to do.
            }
        }
        else
        {
            // Nothing to do.
        }
    }

    IDE_TEST(QC_QMP_MEM(aStatement)->cralloc( ID_SIZEOF(qmsFrom), (void**)&sTemp ) != IDE_SUCCESS);

    sTemp->joinType     = QMS_INNER_JOIN;
    sTemp->onCondition  = sNode;

    IDE_TEST( qtc::dependencyOr( &(sPushFrom->depInfo),
                                 &(sFindFrom->depInfo),
                                 &(sTemp->depInfo) ) != IDE_SUCCESS );

    // sPushFrom�� ������ ���ʿ� �־�� �Ѵ�.
    // Ž���Ҷ� ������ ���ʸ� Ž���ϱ� �����̴�.
    sTemp->left         = sPushFrom;
    sTemp->right        = sFindFrom;
    sParent->left       = sTemp;

    IDE_EXCEPTION_CONT( skip );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}
