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
 * $Id: qmoJoinMethod.h 82075 2018-01-17 06:39:52Z jina.kim $
 *
 * Description :
 *    Join Cost�� ���Ͽ� �پ��� Join Method�� �߿��� ���� cost �� ����
 *    Join Method�� �����Ѵ�.
 *
 * ��� ���� :
 *
 * ��� :
 *
 **********************************************************************/

#ifndef _O_QMO_JOIN_METHOD_H_
#define _O_QMO_JOIN_METHOD_H_ 1

#include <qc.h>
#include <qcm.h>
#include <qmgDef.h>
#include <qmoDef.h>
#include <qmoSelectivity.h>

/***********************************************************************
 * [qmoJoinMethodCost�� flag�� ���� ���]
 ***********************************************************************/

// qmoJoinMethodCost.flag
#define QMO_JOIN_METHOD_FLAG_CLEAR             (0x00000000)

// Join Method Type : qmoHint�� QMO_JOIN_METHOD_MASK �� ����

// qmoJoinMethodCost.flag
// Feasibility : �ش� Join Method ���� ���ɼ�
#define QMO_JOIN_METHOD_FEASIBILITY_MASK       (0x00010000)
#define QMO_JOIN_METHOD_FEASIBILITY_FALSE      (0x00010000)
#define QMO_JOIN_METHOD_FEASIBILITY_TRUE       (0x00000000)

// qmoJoinMethodCost.flag
// Join Direction : Left->Right �Ǵ� Right->Left
#define QMO_JOIN_METHOD_DIRECTION_MASK         (0x00020000)
#define QMO_JOIN_METHOD_DIRECTION_LEFTRIGHT    (0x00000000)
#define QMO_JOIN_METHOD_DIRECTION_RIGHTLEFT    (0x00020000)

// qmoJoinMethodCost.flag
// DISK/MEMORY : ���� ��� ������ disk����, memory������ ���� ����
// Two-Pass Sort/Hash Join�� ���� Temp Table�� ����
#define QMO_JOIN_METHOD_LEFT_STORAGE_MASK         (0x00040000)
#define QMO_JOIN_METHOD_LEFT_STORAGE_DISK         (0x00000000)
#define QMO_JOIN_METHOD_LEFT_STORAGE_MEMORY       (0x00040000)

// qmoJoinMethodCost.flag
#define QMO_JOIN_METHOD_RIGHT_STORAGE_MASK        (0x00080000)
#define QMO_JOIN_METHOD_RIGHT_STORAGE_DISK        (0x00000000)
#define QMO_JOIN_METHOD_RIGHT_STORAGE_MEMORY      (0x00080000)

// qmoJoinMethodCost.flag
// left preserved order�� ���
#define QMO_JOIN_METHOD_USE_LEFT_PRES_ORDER_MASK        (0x00100000)
#define QMO_JOIN_METHOD_USE_LEFT_PRES_ORDER_FALSE       (0x00000000)
#define QMO_JOIN_METHOD_USE_LEFT_PRES_ORDER_TRUE        (0x00100000)

// qmoJoinMethodCost.flag
// right preserved order�� ���
#define QMO_JOIN_METHOD_USE_RIGHT_PRES_ORDER_MASK        (0x00200000)
#define QMO_JOIN_METHOD_USE_RIGHT_PRES_ORDER_FALSE       (0x00000000)
#define QMO_JOIN_METHOD_USE_RIGHT_PRES_ORDER_TRUE        (0x00200000)

// qmoJoinMethodCost.flag
// SORT �Ǵ� MERGE JOIN ����� Left Node�� ���� ����
// NONE       : Left Node�� �������� ����.
// STORE      : Left Node�� SORT��带 �����ϰ� Sorting�� ���� ����
// SORTING    : Left Node�� SORT��带 �����ϰ� Sorting Option
#define QMO_JOIN_LEFT_NODE_MASK           (0x00C00000)
#define QMO_JOIN_LEFT_NODE_NONE           (0x00000000)
#define QMO_JOIN_LEFT_NODE_STORE          (0x00400000)
#define QMO_JOIN_LEFT_NODE_SORTING        (0x00800000)

// qmoJoinMethodCost.flag
// SORT �Ǵ� MERGE JOIN ����� Right Node�� ���� ����
#define QMO_JOIN_RIGHT_NODE_MASK           (0x03000000)
#define QMO_JOIN_RIGHT_NODE_NONE           (0x00000000)
#define QMO_JOIN_RIGHT_NODE_STORE          (0x01000000)
#define QMO_JOIN_RIGHT_NODE_SORTING        (0x02000000)

// qmoJoinMethodCost.flag
// BUG-37407 semi, anti join cost
#define QMO_JOIN_SEMI_MASK                 (0x04000000)
#define QMO_JOIN_SEMI_FALSE                (0x00000000)
#define QMO_JOIN_SEMI_TRUE                 (0x04000000)

// qmoJoinMethodCost.flag
#define QMO_JOIN_ANTI_MASK                 (0x08000000)
#define QMO_JOIN_ANTI_FALSE                (0x00000000)
#define QMO_JOIN_ANTI_TRUE                 (0x08000000)

// To Fix PR-11212
// Index Merge Join ������ �Ǵ��� ���� MASK
#define QMO_JOIN_INDEX_MERGE_MASK       ( QMO_JOIN_METHOD_MASK |        \
                                          QMO_JOIN_LEFT_NODE_MASK |     \
                                          QMO_JOIN_RIGHT_NODE_MASK )
#define QMO_JOIN_INDEX_MERGE_TRUE       ( QMO_JOIN_METHOD_MERGE |       \
                                          QMO_JOIN_LEFT_NODE_NONE |     \
                                          QMO_JOIN_RIGHT_NODE_NONE )

// PROJ-2242
#define QMO_FIRST_ROWS_SET( aLeft , aJoinMethodCost ) \
    { (aLeft)->costInfo.outputRecordCnt *= (aJoinMethodCost)->firstRowsFactor; \
      (aLeft)->costInfo.totalAccessCost *= (aJoinMethodCost)->firstRowsFactor; \
      (aLeft)->costInfo.totalDiskCost   *= (aJoinMethodCost)->firstRowsFactor; \
      (aLeft)->costInfo.totalAllCost    *= (aJoinMethodCost)->firstRowsFactor; \
    }

#define QMO_FIRST_ROWS_UNSET( aLeft , aJoinMethodCost ) \
    { (aLeft)->costInfo.outputRecordCnt /= (aJoinMethodCost)->firstRowsFactor; \
      (aLeft)->costInfo.totalAccessCost /= (aJoinMethodCost)->firstRowsFactor; \
      (aLeft)->costInfo.totalDiskCost   /= (aJoinMethodCost)->firstRowsFactor; \
      (aLeft)->costInfo.totalAllCost    /= (aJoinMethodCost)->firstRowsFactor; \
    }

// BUG-40022
#define QMO_JOIN_DISABLE_NL       (0x00000001)
#define QMO_JOIN_DISABLE_HASH     (0x00000002)
#define QMO_JOIN_DISABLE_SORT     (0x00000004)
#define QMO_JOIN_DISABLE_MERGE    (0x00000008)

//---------------------------------------------------
// join ���� ���⿡ ���� ����
//---------------------------------------------------
typedef enum
{
    QMO_JOIN_DIRECTION_LEFT_RIGHT = 0,
    QMO_JOIN_DIRECTION_RIGHT_LEFT

} qmoJoinDirection;

//---------------------------------------------------------------------
// PROJ-2418 
// Join����, �� Child Graph�� �ٸ� Child�� �ܺ� �����ϴ��� ����
//---------------------------------------------------------------------
typedef enum
{
    QMO_JOIN_LATERAL_NONE = 0,   // �� Graph�� �ٸ� ���� �������� ���� 
    QMO_JOIN_LATERAL_LEFT,       // ���� Graph�� Lateral View, �������� ����
    QMO_JOIN_LATERAL_RIGHT       // ������ Graph�� Lateral View, ������ ����
} qmoJoinLateralDirection;

//---------------------------------------
// Index Nested Loop Join ����, �Է����ڷ� ���� �ڷᱸ�� ����.
//---------------------------------------
typedef struct qmoJoinIndexNL
{
    // selectivity�� ����ϱ����� index����
    qcmIndex         * index;

    //-----------------------------------------------
    // [predicate ����]
    // aPredicate      : join predicate�� ���Ḯ��Ʈ
    // aRightChildPred : join index Ȱ���� ����ȭ ������ ����
    //                   right child graph�� predicate ����
    //-----------------------------------------------
    qmoPredicate     * predicate;
    qmoPredicate     * rightChildPred;

    // right child graph�� dependencies ����
    qcDepInfo        * rightDepInfo;
    // ������� �ڷᱸ��
    qmoStatistics    * rightStatiscalData;

    // left->right or right->left�� join �������.
    qmoJoinDirection   direction;

} qmoJoinIndexNL;

//---------------------------------------
// Anti Outer Join ����, �Է����ڷ� ���� �ڷᱸ�� ����.
//---------------------------------------
typedef struct qmoJoinAntiOuter
{
    // selectivity�� ����ϱ����� index���� (right graph index)
    qcmIndex         * index;

    // join predicate�� ���Ḯ��Ʈ
    qmoPredicate     * predicate;

    // right child graph�� dependencies ����
    qcDepInfo        * rightDepInfo;

    //-----------------------------------------------
    // left graph�� �ε��� ����
    // predicate ������ ����÷��� �ε����� �����ϴ����� �˻��ϱ� ���� ����
    //-----------------------------------------------
    UInt               leftIndexCnt;
    qmoIdxCardInfo   * leftIndexInfo;

} qmoJoinAntiOuter;

//----------------------------------------------------
// Join Graph�� Left���� Right������ ����
//----------------------------------------------------
typedef enum
{
    QMO_JOIN_CHILD_LEFT = 0,
    QMO_JOIN_CHILD_RIGHT
} qmoJoinChild;

/***********************************************************************
 * [Join Method Cost �� ���ϱ� ���� �ڷ� ����]
 ***********************************************************************/

//----------------------------------------------------
// qmoJoinMethodCost �ڷ� ����
//----------------------------------------------------
typedef struct qmoJoinMethodCost
{
    // Join Method Type, Feasibility, Join Direction, ���� DISK/MEMORY ����
    UInt            flag;
    SDouble         selectivity;
    SDouble         firstRowsFactor;
    qmoPredInfo   * joinPredicate;    // joinable Predicate ����

    //---------------------------------------------
    // ��� ����
    //---------------------------------------------

    SDouble         accessCost;
    SDouble         diskCost;
    SDouble         totalCost;


    //---------------------------------------------
    // Index ���� : selectivity ��� �ÿ� �ʿ���
    //    - Index Nested Loop Join, Anti Outer Join�� ��쿡�� ���
    //    - �� ���� ��쿡�� NULL ���� ������.
    //---------------------------------------------

    qmoIdxCardInfo  * rightIdxInfo;
    qmoIdxCardInfo  * leftIdxInfo;

    // two pass hash join hint�� temp table ������ �����ϴ� ��쿡�� ����
    UInt            hashTmpTblCntHint;

} qmoJoinMethodCost;

//---------------------------------------------------
// Join Method �ڷ� ����
//
//    - flag : QMO_JOIN_METHOD_NL
//             QMO_JOIN_METHOD_HASH
//             QMO_JOIN_METHOD_SORT
//             QMO_JOIN_METHOD_MERGE
//    - selectedJoinMethod : �ش� joinMethodCost ��
//             ���� cost�� ���� JoinMethodCost
//    - joinMethodCnt : �ش� Join Method ����
//    - joinMethodCost : joinMethod�� ���� cost
//
//---------------------------------------------------
typedef struct qmoJoinMethod
{
    UInt                flag;

    //---------------------------------------------
    // ���õ� Join Method
    //    - selectedJoinMethod : ���� cost�� ���� join method
    //    - hintJoinMethod : Join Method Hint���� table order���� �����ϴ�
    //                       Join Method
    //    - hintJoinMethod2 : Join Method Hint���� Method�� �����ϴ�
    //                        Join Method
    //---------------------------------------------

    qmoJoinMethodCost * selectedJoinMethod;
    qmoJoinMethodCost * hintJoinMethod;
    qmoJoinMethodCost * hintJoinMethod2;

    UInt                joinMethodCnt;
    qmoJoinMethodCost * joinMethodCost;
} qmoJoinMethod;


//---------------------------------------------------
// Join Method�� �����ϴ� �Լ�
//---------------------------------------------------

class qmoJoinMethodMgr
{
public:

    // Join Method �ڷ� ������ �ʱ�ȭ
    static IDE_RC init( qcStatement    * aStatement,
                        qmgGraph       * aGraph,
                        SDouble          aFirstRowsFactor,
                        qmoPredicate   * aJoinPredicate,
                        UInt             aJoinMethodType,
                        qmoJoinMethod  * aJoinMethod );

    // Join Method ����
    static IDE_RC getBestJoinMethodCost( qcStatement   * aStatement,
                                         qmgGraph      * aGraph,
                                         qmoPredicate  * aJoinPredicate,
                                         qmoJoinMethod * aJoinMethod );

    // Graph�� ���� ������ �����.
    static IDE_RC     printJoinMethod( qmoJoinMethod * aMethod,
                                       ULong           aDepth,
                                       iduVarString  * aString );

private:
    // Join Method�� Cost ���
    static IDE_RC getJoinCost( qcStatement       * aStatement,
                               qmoJoinMethodCost * aMethod,
                               qmoPredicate      * aJoinPredicate,
                               qmgGraph          * aLeft,
                               qmgGraph          * aRight);


    // Full Nested Loop Join�� Cost ���
    static IDE_RC getFullNestedLoop( qcStatement       * aStatement,
                                     qmoJoinMethodCost * aMethod,
                                     qmoPredicate      * aJoinPredicate,
                                     qmgGraph          * aLeft,
                                     qmgGraph          * aRight);

    // Full Store Nested Loop Join�� Cost ���
    static IDE_RC getFullStoreNestedLoop( qcStatement       * aStatement,
                                          qmoJoinMethodCost * aMethod,
                                          qmoPredicate      * aJoinPredicate,
                                          qmgGraph          * aLeft,
                                          qmgGraph          * aRight);

    // Index Nested Loop Join�� Cost ���
    static IDE_RC getIndexNestedLoop( qcStatement       * aStatement,
                                      qmoJoinMethodCost * aMethod,
                                      qmoPredicate      * aJoinPredicate,
                                      qmgGraph          * aLeft,
                                      qmgGraph          * aRight);

    // Anti Outer Join�� Cost ���
    static IDE_RC getAntiOuter( qcStatement       * aStatement,
                                qmoJoinMethodCost * aMethod,
                                qmoPredicate      * aJoinPredicate,
                                qmgGraph          * aLeft,
                                qmgGraph          * aRight);

    // One Pass Sort Join�� Cost ���
    static IDE_RC getOnePassSort( qcStatement       * aStatement,
                                  qmoJoinMethodCost * aMethod,
                                  qmoPredicate      * aJoinPredicate,
                                  qmgGraph          * aLeft,
                                  qmgGraph          * aRight);

    // Two Pass Sort Join�� Cost ���
    static IDE_RC getTwoPassSort( qcStatement       * aStatement,
                                  qmoJoinMethodCost * aMethod,
                                  qmoPredicate      * aJoinPredicate,
                                  qmgGraph          * aLeft,
                                  qmgGraph          * aRight);

    // Inverse Sort Join�� Cost ���
    static IDE_RC getInverseSort( qcStatement       * aStatement,
                                  qmoJoinMethodCost * aMethod,
                                  qmoPredicate      * aJoinPredicate,
                                  qmgGraph          * aLeft,
                                  qmgGraph          * aRight);

    // One Pass Hash Join�� Cost ���
    static IDE_RC getOnePassHash( qcStatement       * aStatement,
                                  qmoJoinMethodCost * aMethod,
                                  qmoPredicate      * aJoinPredicate,
                                  qmgGraph          * aLeft,
                                  qmgGraph          * aRight);

    // Two Pass Hash Join�� Cost ���
    static IDE_RC getTwoPassHash( qcStatement       * aStatement,
                                  qmoJoinMethodCost * aMethod,
                                  qmoPredicate      * aJoinPredicate,
                                  qmgGraph          * aLeft,
                                  qmgGraph          * aRight);

    // Inverse Hash Join�� Cost ���
    static IDE_RC getInverseHash( qcStatement       * aStatement,
                                  qmoJoinMethodCost * aMethod,
                                  qmoPredicate      * aJoinPredicate,
                                  qmgGraph          * aLeft,
                                  qmgGraph          * aRight);

    // Merge Join�� Cost ���
    static IDE_RC getMerge( qcStatement       * aStatement,
                            qmoJoinMethodCost * aMethod,
                            qmoPredicate      * aJoinPredicate,
                            qmgGraph          * aLeft,
                            qmgGraph          * aRight);

    // Nested Loop Join Method Cost�� �ʱ�ȭ
    static IDE_RC initJoinMethodCost4NL( qcStatement             * aStatement,
                                         qmgGraph                * aGraph,
                                         qmoPredicate            * aJoinPredicate,
                                         qmoJoinLateralDirection   aLateralDirection,
                                         qmoJoinMethodCost      ** aMethod,
                                         UInt                    * aMethodCnt );

    // Hash Join Method Cost�� �ʱ�ȭ
    static IDE_RC initJoinMethodCost4Hash( qcStatement             * aStatement,
                                           qmgGraph                * aGraph,
                                           qmoPredicate            * aJoinPredicate,
                                           qmoJoinLateralDirection   aLateralDirection,
                                           qmoJoinMethodCost      ** aMethod,
                                           UInt                    * aMethodCnt );

    // Sort Join Method Cost�� �ʱ�ȭ
    static IDE_RC initJoinMethodCost4Sort( qcStatement             * aStatement,
                                           qmgGraph                * aGraph,
                                           qmoPredicate            * aJoinPredicate,
                                           qmoJoinLateralDirection   aLateralDirection,
                                           qmoJoinMethodCost      ** aMethod,
                                           UInt                    * aMethodCnt );


    // Merget Join�� Join Method Cost�� �ʱ�ȭ
    static IDE_RC initJoinMethodCost4Merge(qcStatement             * aStatement,
                                           qmgGraph                * aGraph,
                                           qmoPredicate            * aJoinPredicate,
                                           qmoJoinLateralDirection   aLateralDirection,
                                           qmoJoinMethodCost      ** aMethod,
                                           UInt                    * aMethodCnt );

    // Join Method Cost��  Join Method Type�� direction ������ �����ϴ� �Լ�
    static IDE_RC setFlagInfo( qmoJoinMethodCost * aJoinMethodCost,
                               UInt                aFirstTypeFlag,
                               UInt                aJoinMethodCnt,
                               idBool              aIsLeftOuter );

    // merge join���� preserved order ��� ���� ���θ� �˾Ƴ�
    static IDE_RC usablePreservedOrder4Merge( qcStatement    * aStatement,
                                              qmoPredInfo    * aJoinPred,
                                              qmgGraph       * aGraph,
                                              qmoAccessMethod ** aAccessMethod,
                                              idBool         * aUsable );

    // Join Method Hint�� �´� qmoJoinMethodCost�� ã��
    static IDE_RC setJoinMethodHint( qmoJoinMethod      * aJoinMethod,
                                     qmsJoinMethodHints * aJoinMethodHints,
                                     qmgGraph           * aGraph,
                                     UInt                 aNoUseHintMask,
                                     qmoJoinMethodCost ** aSameTableOrder,
                                     qmoJoinMethodCost ** aDiffTableOrder );

    // Sort Join���� ���Ͽ� Preserved Order�� ��� �������� �˻�
    static IDE_RC canUsePreservedOrder( qcStatement       * aStatement,
                                        qmoJoinMethodCost * aJoinMethodCost,
                                        qmgGraph          * aGraph,
                                        qmoJoinChild        aChildPosition,
                                        idBool            * aUsable );

    // full nested loop, full store nested loop join�� ��뿩�� �˻�
    static IDE_RC usableJoinMethodFullNL( qmgGraph     * aGraph,
                                          qmoPredicate * aJoinPredicate,
                                          idBool       * aIsUsable );

    // push_pred hints�� ���� join order ����
    static IDE_RC forcePushPredHint( qmgGraph      * aGraph,
                                     qmoJoinMethod * aJoinMethod );


    // PROJ-2582 recursive with
    // recursive view�� ���� join order ����
    static IDE_RC forceJoinOrder4RecursiveView( qmgGraph      * aGraph,
                                                qmoJoinMethod * aJoinMethod );

    /* PROJ-2242 */

    // Nested loop join ������ qmoPredInfo list ȹ��
    static IDE_RC setJoinPredInfo4NL( qcStatement         * aStatement,
                                      qmoPredicate        * aJoinPredicate,
                                      qmoIdxCardInfo      * aRightIndexInfo,
                                      qmgGraph            * aRightGraph,
                                      qmoJoinMethodCost   * aMethodCost );

    // Anti outer ������ qmoPredInfo list ȹ��
    static IDE_RC setJoinPredInfo4AntiOuter( qcStatement       * aStatement,
                                             qmoPredicate      * aJoinPredicate,
                                             qmoIdxCardInfo    * aRightIndexInfo,
                                             qmgGraph          * aRightGraph,
                                             qmgGraph          * aLeftGraph,
                                             qmoJoinMethodCost * aMethodCost );

    // Hash join ������ qmoPredInfo list ȹ��
    static IDE_RC setJoinPredInfo4Hash( qcStatement       * aStatement,
                                        qmoPredicate      * aJoinPredicate,
                                        qmoJoinMethodCost * aMethodCost );

    // Sort join ������ qmoPredInfo list ȹ��
    static IDE_RC setJoinPredInfo4Sort( qcStatement       * aStatement,
                                        qmoPredicate      * aJoinPredicate,
                                        qcDepInfo         * aRightDepInfo,
                                        qmoJoinMethodCost * aMethodCost );

    // Merge join ������ qmoPredInfo ȹ�� (one predicate)
    static IDE_RC setJoinPredInfo4Merge( qcStatement         * aStatement,
                                         qmoPredicate        * aJoinPredicate,
                                         qmoJoinMethodCost   * aMethodCost );

    // Index nested loop join ������ qmoPredInfo list ��ȯ
    static IDE_RC getIndexNLPredInfo( qcStatement      * aStatement,
                                      qmoJoinIndexNL   * aIndexNLInfo,
                                      qmoPredInfo     ** aJoinPredInfo );

    // Anti outer ������ qmoPredInfo list ��ȯ
    static IDE_RC getAntiOuterPredInfo( qcStatement      * aStatement,
                                        UInt               aMethodCount,
                                        qmoAccessMethod  * aAccessMethod,
                                        qmoJoinAntiOuter * aAntiOuterInfo,
                                        qmoPredInfo     ** aJoinPredInfo,
                                        qmoIdxCardInfo  ** aLeftIdxInfo );

    // Anti outer ������ qmoPredicate ���� �� ��ȯ
    // cost �� ���� ���� left qmoAccessMethod[].method ��ȯ
    static IDE_RC getAntiOuterPred( qcStatement      * aStatement,
                                    UInt               aKeyColCnt,
                                    UInt               aMethodCount,
                                    qmoAccessMethod  * aAccessMethod,
                                    qmoPredicate     * aPredicate,
                                    qmoPredicate    ** aAntiOuterPred,
                                    qmoIdxCardInfo  ** aSelectedLeftIdxInfo );

    // getIndexNLPredInfo, getAntiOuterPredInfo �����
    // �ʿ��� �ӽ����� ���� ( columnID, indexArgument )
    static IDE_RC setTempInfo( qcStatement  * aStatement,
                               qmoPredicate * aPredicate,
                               qcDepInfo    * aRightDepInfo,
                               UInt           aDirection );

    // setJoinPredInfo4Sort �����,
    // ���� sort column�� ������ �÷��� �����ϴ��� �˻�
    static IDE_RC findEqualSortColumn( qcStatement  * aStatement,
                                       qmoPredicate * aPredicate,
                                       qcDepInfo    * aRightDepInfo,
                                       qtcNode      * aSortColumn,
                                       qmoPredInfo  * aPredInfo );
    // BUG-42429
    // index_NL ���ν� index cost �� �ٽ� ��� �ϴ� �Լ�
    static void adjustIndexCost( qcStatement     * aStatement,
                                 qmgGraph        * aRight,
                                 qmoIdxCardInfo  * aIndexCardInfo,
                                 qmoPredInfo     * aJoinPredInfo,
                                 SDouble         * aMemCost,
                                 SDouble         * aDiskCost );

    // PROJ-2418 
    // Join Graph�� Lateral Position ��ȯ
    static IDE_RC getJoinLateralDirection( qmgGraph                * aJoinGraph,
                                           qmoJoinLateralDirection * aLateralDirection );
};


#endif /* _O_QMO_JOIN_METHOD_H_ */
