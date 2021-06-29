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
 
/******************************************************************************
 * $Id$
 *****************************************************************************/

#ifndef _Q_QMO_DISTINCT_ELIMINATION_H_
#define _Q_QMO_DISTINCT_ELIMINATION_H_ 1

#include <ide.h>
#include <qmsParseTree.h>

class qmoDistinctElimination
{
public:

    static IDE_RC doTransform( qcStatement * aStatement,
                               qmsQuerySet * aQuerySet );

private:

    static IDE_RC doTransformSFWGH( qcStatement * aStatement,
                                    qmsSFWGH    * aSFWGH,
                                    idBool      * aChanged );

    // Distinct Elimination�� �ʿ����� Ȯ��
    static idBool canTransform( qmsSFWGH * aSFWGH );

    // BUG-39665 GROUP BY �ִ� SELECT TARGET�� DISTINCT 
    static IDE_RC isDistTargetByGroup( qcStatement  * aStatement,
                                       qmsSFWGH     * aSFWGH,
                                       idBool       * sIsDistTarget );

    // BUG-39522 GROUP BY ���� SELECT TARGET�� DISTINCT
    static IDE_RC isDistTargetByUniqueIdx( qcStatement  * aStatement,
                                           qmsSFWGH     * aSFWGH,
                                           qmsFrom      * aFrom,
                                           idBool       * aIsDistTarget );
};

#endif /* _Q_QMO_DISTINCT_ELIMINATION_H_ */
