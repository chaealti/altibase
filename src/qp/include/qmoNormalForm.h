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
 * $Id: qmoNormalForm.h 89925 2021-02-03 04:40:48Z ahra.cho $
 *
 * Description :
 *     Normal Form Manager
 *
 *     ������ȭ�� Predicate���� ����ȭ�� ���·� �����Ű�� ��Ȱ�� �Ѵ�.
 *     ������ ���� ����ȭ�� �����Ѵ�.
 *         - CNF (Conjunctive Normal Form)
 *         - DNF (Disjunctive Normal Form)
 *
 * ��� ���� :
 *
 * ��� :
 *
 **********************************************************************/

#ifndef _Q_QMO_NORMAL_FORM_H_
#define _Q_QMO_NORMAL_FORM_H_ 1

#include <ide.h>
#include <qtcDef.h>
#include <qmsParseTree.h>

//-----------------------------------------------------------
// To Fix PR-8379
// Normal Form �� ������ ��� �ִ� ����
// Normal Form ����� �Ʒ��� ������ �Ѵ� �����,
// �ƹ��� ��� ����� ���� �������� ����ġ�� ���� Filter��
// ���� ������ ������ ���� �ȴ�.
// CNF�� DNF ��� �ش� ���� �Ѵ´ٸ� ��� �񱳰� �̷������
// ������ ���� ���ٰ� ū ������ ���� �ʴ´�.
//-----------------------------------------------------------
// #define QMO_NORMALFORM_MAXIMUM                         (128)

//-----------------------------------------------------------
// Normal Form ���� �Լ�
//-----------------------------------------------------------

class qmoNormalForm
{
public:

    // CNF�θ� ó���� �� �ִ� ��츦 �Ǵ�
    static IDE_RC normalizeCheckCNFOnly( qtcNode  * aNode,
                                         idBool   * aCNFonly );

    // DNF �� ����ȭ
    static IDE_RC normalizeDNF( qcStatement   * aStatement,
                                qtcNode       * aNode,
                                qtcNode      ** aDNF        );

    // CNF �� ����ȭ
    static IDE_RC normalizeCNF( qcStatement   * aStatement,
                                qtcNode       * aNode,
                                qtcNode      ** aCNF,
                                idBool          aIsWhere = ID_FALSE,
                                qmsHints      * aHints = NULL );
           
    // DNF�� ����ȭ�Ǿ����� ��������� �񱳿����� ����� ���� ����
    static IDE_RC estimateDNF( qtcNode  * aNode,
                               UInt     * aCount );

    // CNF�� ����ȭ�Ǿ����� ��������� �񱳿����� ����� ���� ����
    static IDE_RC estimateCNF( qtcNode  * aNode,
                               UInt     * aCount );

    // ����ȭ���·� ��ȯ�ϸ鼭 ���� ���ο� �������� ��忡 ����
    // flag�� dependency ����
    // qmoPredicate::nodeTransform()������ ȣ��
    static IDE_RC setFlagAndDependencies(qtcNode * aNode);    


    // ����ȭ�� ���¿��� ���ʿ��� AND, OR ��带 ����
    static IDE_RC optimizeForm( qtcNode  * aInputNode,
                                qtcNode ** aOutputNode );

    // BUG-34295 Join ordering ANSI style query
    // ����ȭ ���·� ��ȯ�ϴ� �������� predicate ����
    static IDE_RC addToMerge( qtcNode     * aPrevNF,
                              qtcNode     * aCurrNF,
                              qtcNode    ** aNFNode );

    // BUG-35155 Partial CNF
    // ������ ���������� CNF ��� ���� �� ����ȭ�Ǿ������� �񱳿����� ��� ���� ����
    static void estimatePartialCNF( qtcNode  * aNode,
                                    UInt     * aCount,
                                    qtcNode  * aRoot,
                                    UInt       aNFMaximum );

    // CNF ��󿡼� ���ܵ� qtcNode �� NNF ���� ����
    static IDE_RC extractNNFFilter4CNF( qcStatement  * aStatement,
                                        qtcNode      * aNode,
                                        qtcNode     ** aNNF );

private:

    // Subquery�� ������ ���, DNF�� ����ϸ� �ȵ�
    // DNF ���·� predicate ��ȯ
    static IDE_RC makeDNF( qcStatement  * aStatement,
                           qtcNode      * aNode,
                           qtcNode     ** aDNF );

    // CNF ���·� predicate ��ȯ
    static IDE_RC makeCNF( qcStatement  * aStatement,
                           qtcNode      * aNode,
                           qtcNode     ** aCNF );
    
    // ����ȭ ���·� ��ȯ�ϴ� �������� predicate�� ���� ��й�Ģ ����
    static IDE_RC productToMerge( qcStatement * aStatement,
                                  qtcNode     * aPrevNF,
                                  qtcNode     * aCurrNF,
                                  qtcNode    ** aNFNode );

    // NNF ���� ����
    static IDE_RC makeNNF4CNFByCopyNodeTree( qcStatement  * aStatement,
                                             qtcNode      * aNode,
                                             qtcNode     ** aNNF );

    // qtcNode Ʈ�� ����
    static IDE_RC copyNodeTree( qcStatement  * aStatement,
                                qtcNode      * aNode,
                                qtcNode     ** aCopy );

    // NULL �� �����ϴ� addToMerge
    static IDE_RC addToMerge2( qtcNode     * aPrevNF,
                               qtcNode     * aCurrNF,
                               qtcNode    ** aNFNode);

};

#endif  /* _Q_QMO_NORMAL_FORM_H_ */ 
