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
 * $Id: qmoNonCrtPathMgr.h 82075 2018-01-17 06:39:52Z jina.kim $
 *
 * Description :
 *     Non Critical Path Manager
 *
 *     Critical Path �̿��� �κп� ���� ����ȭ �� �׷����� �����Ѵ�.
 *     ��, ������ ���� �κп� ���� �׷��� ����ȭ�� �����Ѵ�.
 *         - Projection Graph
 *         - Set Graph
 *         - Sorting Graph
 *         - Distinction Graph
 *         - Grouping Graph
 *
 * ��� ���� :
 *
 * ��� :
 *
 **********************************************************************/

#ifndef _O_QMO_NON_CRT_PATH_MGR_H_
#define _O_QMO_NON_CRT_PATH_MGR_H_ 1

#include <qc.h>
#include <qmgDef.h>
#include <qmsParseTree.h>
#include <qmoCrtPathMgr.h>

//---------------------------------------------------
// Non Critical Path�� �����ϱ� ���� �ڷ� ����
//---------------------------------------------------

// qmoNonCrtPath.flag
#define QMO_NONCRT_PATH_INITIALIZE      (0x00000000)

// TOP�� ��� Sorting, Projection Graph���� ����
#define QMO_NONCRT_PATH_TOP_MASK        (0x00000001)
#define QMO_NONCRT_PATH_TOP_FALSE       (0x00000000)
#define QMO_NONCRT_PATH_TOP_TRUE        (0x00000001)

typedef struct qmoNonCrtPath
{
    UInt                flag;
    
    qmgGraph          * myGraph;    // non critical path���� ó���� ���graph
    qmsQuerySet       * myQuerySet; // �ڽ��� qmsParseTree::querySet�� ����Ŵ

    qmoCrtPath        * crtPath;    // critical path ����
    
    qmoNonCrtPath     * left;
    qmoNonCrtPath     * right;
    
} qmoNonCrtPath;

//---------------------------------------------------
// Non Critical Path�� ���� �Լ�
//---------------------------------------------------

class qmoNonCrtPathMgr 
{
public:

    // Non Critical Path ���� �� �ʱ�ȭ
    static IDE_RC    init( qcStatement    * aStatement,
                           qmsQuerySet    * aQuerySet,
                           idBool           aIsTop,
                           qmoNonCrtPath ** aNonCrtPath );
    
    // Non Critical Path�� ���� ����ȭ �� Graph ����
    static IDE_RC    optimize( qcStatement    * aStatement,
                               qmoNonCrtPath  * aNonCrtPath);
    
private:
    // Leaf Non Critical Path�� ����ȭ
    static IDE_RC    optimizeLeaf( qcStatement   * aStatement,
                                   qmoNonCrtPath * aNonCrtPath );
    

    // Non-Leaf Non Critical Path�� ����ȭ
    static IDE_RC    optimizeNonLeaf( qcStatement   * aStatement,
                                      qmoNonCrtPath * aNonCrtPath );

    // PROJ-2582 recursive with
    static IDE_RC optimizeSet( qcStatement   * aStatement,
                               qmoNonCrtPath * aNonCrtPath );

    static IDE_RC  optimizeSetRecursive( qcStatement   * aStatement,
                                         qmoNonCrtPath * aNonCrtPath );
};


#endif /* _O_QMO_NON_CRT_PATH_MGR_H_ */

