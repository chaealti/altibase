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
 * $Id: qmgWindowing.h 29304 2008-11-14 08:17:42Z jakim $
 *
 * Description :
 *     Windowing Graph�� ���� ����
 *
 * ��� ���� :
 *
 * ��� :
 *
 **********************************************************************/

#ifndef _O_QMG_WINDOWING_H_
#define _O_QMG_WINDOWING_H_ 1

#include <qc.h>
#include <qmgDef.h>

//---------------------------------------------------
// Windowing Graph�� Define ���
//---------------------------------------------------

//---------------------------------------------------
// Windowing Graph �� �����ϱ� ���� �ڷ� ����
//---------------------------------------------------

typedef struct qmgWINDOW
{
    qmgGraph              graph;      // ���� Graph ����

    qmoDistAggArg       * distAggArg;    
    qmsAnalyticFunc     * analyticFuncList;
    qmsAnalyticFunc    ** analyticFuncListPtrArr;
    UInt                  sortingKeyCount;
    qmgPreservedOrder  ** sortingKeyPtrArr;

} qmgWINDOW;

//---------------------------------------------------
// Windowing Graph �� �����ϱ� ���� �Լ�
//---------------------------------------------------

class qmgWindowing
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

    // sorting key�� �߿��� want order�� ������ key �� �����ϴ��� �˻� 
    static
    IDE_RC existWantOrderInSortingKey( qmgGraph          * aGraph,
                                       qmgPreservedOrder * aWantOrder,
                                       idBool            * aFind,
                                       UInt              * aSameKeyPosition );

    // want order�� sorting key ���� 
    static IDE_RC alterSortingOrder( qcStatement       * aStatement,
                                     qmgGraph          * aGraph,
                                     qmgPreservedOrder * aWantOrder,
                                     idBool              aAlterFirstOrder );

    // plan�� ����� sorting key ���� �缳�� 
    static IDE_RC resetSortingKey( qmgPreservedOrder ** aSortingKeyPtrArr,
                                   UInt                 aSortingKeyCount,
                                   qmsAnalyticFunc   ** aAnalyticFuncListPtrArr);

    static IDE_RC finalizePreservedOrder( qmgGraph * aGraph );

private:
    // sorting key�� analytic function �з� 
    static IDE_RC classifySortingKeyNAnalFunc( qcStatement     * aStatement,
                                               qmgWINDOW       * aMyGraph );

    // partition by, order by�� ������ sorting key, direction�� �����ϴ��� �˻� 
    static IDE_RC existSameSortingKeyAndDirection(
        UInt                 aSortingKeyCount,
        qmgPreservedOrder ** aSortingKeyPtrArr,
        qtcOverColumn      * aOverColumn,
        idBool             * aFoundSameSortingKey,
        idBool             * aReplaceSortingKey,
        UInt               * aSortingKeyPosition );
    
    // BUG-33663 Ranking Function
    // sorting key array���� ���Ű����� sorting key�� ����
    static IDE_RC compactSortingKeyArr( UInt                 aFlag,
                                        qmgPreservedOrder ** aSortingKeyPtrArr,
                                        UInt               * aSortingKeyCount,
                                        qmsAnalyticFunc   ** aAnalyticFuncListPtrArr);
    
    static idBool isSameSortingKey( qmgPreservedOrder * aSortingKey1,
                                    qmgPreservedOrder * aSortingKey2 );
    
    static void mergeSortingKey( qmgPreservedOrder * aSortingKey1,
                                 qmgPreservedOrder * aSortingKey2 );

    static IDE_RC getBucketCnt4Windowing( qcStatement  * aStatement,
                                          qmgWINDOW    * aMyGraph,
                                          qtcOverColumn* aGroupBy,
                                          UInt           aHintBucketCnt,
                                          UInt         * aBucketCnt );
};

#endif /* _O_QMG_WINDOWING_H_ */

