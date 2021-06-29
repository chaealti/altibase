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
 * $Id: qmoRidPred.cpp 47368 2011-05-11 01:52:04Z sbjang $
 **********************************************************************/

#include <ide.h>
#include <qtc.h>
#include <qmoRidPred.h>

extern mtfModule mtfEqual;
extern mtfModule mtfEqualAny;

static idBool qmoRidPredCheckRidScan(qmoPredicate* aRidPred)
{
    idBool   sIsOk;
    mtcNode* sNode;
    mtcNode* sRidNode = NULL;
    mtcNode* sValueNode = NULL;

    IDE_DASSERT(aRidPred != NULL);

    sIsOk = ID_TRUE;

    sNode = aRidPred->node->node.arguments; /* sNode = '�񱳿�����' */

    while( sNode != NULL )
    {
        /*
         * 1) = �� ���� value �ʿ� COLUMN �� ���� _prowid �� conversion �� ���� ���¿� ���� rid scan ����
         *    - _prowid = 1 + 1
         *    - _prowid = cast (:a as bigint) + 1
         * 2) =ANY �� ���� value �ʿ� conversion �� ���� ���¿� ���� rid scan ����
         *    - _prowid =ANY ( 1, 2 )
         */

        // BUG-41215 _prowid predicate fails to create ridRange
        if ( sNode->module == &mtfEqual )
        {
            // BUG-43043
            if ( ( sNode->arguments->module == &gQtcRidModule ) &&
                 ( qtc::haveDependencies( & ((qtcNode *)sNode->arguments->next)->depInfo ) == ID_FALSE ) )
            {
                sRidNode = sNode->arguments;
            }
            else if ( ( sNode->arguments->next->module == &gQtcRidModule ) &&
                      ( qtc::haveDependencies( & ((qtcNode *)sNode->arguments)->depInfo ) == ID_FALSE ) )
            {
                sRidNode = sNode->arguments->next;
            }
            else
            {
                sIsOk = ID_FALSE;
                break;
            }

            if ( sRidNode->conversion != NULL )
            {
                sIsOk = ID_FALSE;
                break;
            }
            else
            {
                // Nothing to do.
            }
        }
        else if ( sNode->module == &mtfEqualAny )
        {
            if ( sNode->arguments->module == &gQtcRidModule )
            {
                sRidNode = sNode->arguments;
                sValueNode = sNode->arguments->next->arguments;
            }
            else
            {
                sIsOk = ID_FALSE;
                break;
            }

            // BUG-41591
            while( sValueNode != NULL )
            {
                // LIST ������ ��� ( ex: i1 =ANY ( 1, '2' ) )
                // value ���� conversion �� �ٸ� �� �����Ƿ�
                // valueNode �ʿ� conversion ��带 ������
                if ( ( sValueNode->module == &qtc::valueModule ) &&
                     ( sValueNode->leftConversion == NULL ) )
                {
                    sValueNode = sValueNode->next;
                }
                else
                {
                    sIsOk = ID_FALSE;
                    break;
                }
            }

            if ( sIsOk == ID_FALSE )
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
            sIsOk = ID_FALSE;
            break;
        }

        if ( sRidNode->column != MTC_RID_COLUMN_ID )
        {
            sIsOk = ID_FALSE;
            break;
        }
        else
        {
            // Nothing to do.
        }

        sNode = sNode->next;
    }
    
    return sIsOk;
}

/*
 * RID predicate list ���� RID scan �� �� predicate �� �����
 * ������ RID predicate �� �Ϲ� predicate �� ���δ�
 */
IDE_RC qmoRidPredExtractRangePred(qmoPredicate*  aInRidPred,
                                  qmoPredicate*  aInOtherPred,
                                  qmoPredicate** aOutRidPred,
                                  qmoPredicate** aOutOtherPred)
{
    idBool        sIsRidScan;
    qmoPredicate* sRidPred;
    qmoPredicate* sFirstPred;
    qmoPredicate* sPrevPred;
    qmoPredicate* sOtherPred;
    qmoPredicate* sNextPred;

    IDU_FIT_POINT_FATAL( "qmoRidPredExtractRangePred::__FT__" );

    IDE_DASSERT(aInRidPred != NULL);

    sFirstPred = aInRidPred;
    sRidPred   = aInRidPred;
    sPrevPred  = NULL;
    sOtherPred = aInOtherPred;
    sIsRidScan = ID_FALSE;

    while (sRidPred != NULL)
    {
        sIsRidScan = qmoRidPredCheckRidScan(sRidPred);
        if (sIsRidScan == ID_TRUE)
        {
            break;
        }
        else
        {
        }

        sPrevPred = sRidPred;
        sRidPred  = sRidPred->next;
    }

    if (sIsRidScan == ID_TRUE)
    {
        /*
         * rid predicate �� �������� ���
         * rid scan ������ �ϳ��� predicate �� �����
         * �������� �Ϲ� predicate ���� �ű��
         */

        if (sRidPred->next != NULL)
        {
            sNextPred       = sRidPred->next;
            sNextPred->next = sOtherPred;

            if (sPrevPred != NULL)
            {
                /*
                 *  ---      ---      ---
                 * |   | -> | V | -> |   |
                 *  ---      ---      ---
                 */
                sPrevPred->next = sNextPred;
                sOtherPred      = sFirstPred;
            }
            else
            {
                /*
                 *   ---      ---
                 *  | V | -> |   |
                 *   ---      ---
                 */
                sOtherPred = sNextPred;
            }
            sRidPred->next = NULL;
        }
        else
        {
            if (sPrevPred != NULL)
            {
                /*
                 *   ---      ---
                 *  |   | -> | V |
                 *   ---      ---
                 */
                sPrevPred->next = sOtherPred;
                sOtherPred      = sFirstPred;
            }
            else
            {
                /*
                 *   ---
                 *  | V |
                 *   ---
                 */
            }
        }
    }
    else
    {
        /*
         * rid scan �� ������ predicate �� ����
         * => ��� rid predicate �� �Ϲ� predicate ���� �ű�
         */
        sPrevPred->next = sOtherPred;
        sOtherPred      = sFirstPred;
        sRidPred        = NULL;
    }

    *aOutRidPred   = sRidPred;
    *aOutOtherPred = sOtherPred;

    return IDE_SUCCESS;
}

