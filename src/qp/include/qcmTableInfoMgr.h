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
 * $Id: qcmTableInfoMgr.h 19310 2006-12-06 01:56:51Z sungminee $
 **********************************************************************/

#ifndef _O_QCM_TABLE_INFO_MGR_H_
#define _O_QCM_TABLE_INFO_MGR_H_ 1

#include <smiDef.h>
#include <qc.h>
#include <qcmTableInfo.h>

class qcmTableInfoMgr
{
public:

    // tableInfo�� ���� �����Ѵ�.
    static IDE_RC makeTableInfoFirst( qcStatement   * aStatement,
                                      UInt            aTableID,
                                      smOID           aTableOID,
                                      qcmTableInfo ** aNewTableInfo );

    // ������ tableInfo�� ������ ���ο� tableInfo�� �����Ѵ�.
    static IDE_RC makeTableInfo( qcStatement   * aStatement,
                                 qcmTableInfo  * aOldTableInfo,
                                 qcmTableInfo ** aNewTableInfo );

    // tableInfo�� �����Ѵ�.
    // ���ο� tableInfo�� �����Ǿ��ִٸ�, ���ο� tableInfo�� �����Ѵ�.
    static IDE_RC destroyTableInfo( qcStatement  * aStatement,
                                    qcmTableInfo * aTableInfo );

    // execute�� ������ old tableInfo�� �����Ѵ�.
    static void   destroyAllOldTableInfo( qcStatement  * aStatement );

    // execute�� ������ new tableInfo�� �����ϰ� �����Ѵ�.
    static void   revokeAllNewTableInfo( qcStatement  * aStatement );
};

#endif /* _O_QCM_TABLE_INFO_H_ */
