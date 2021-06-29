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
 * $Id: qcmDump.h 13836 2007-02-05 01:52:33Z leekmo $
 *
 * Description :
 *
 *    D$XXXX �迭�� DUMP OBJECT�� ���� ���� ȹ�� ���� �����Ѵ�.
 *
 *
 * ��� ���� :
 *
 * ��� :
 *
 *
 **********************************************************************/

#ifndef _O_QCM_DUMP_H_
#define _O_QCM_DUMP_H_ 1

#include    <iduFixedTable.h>
#include    <smiDef.h>
#include    <qc.h>
#include    <qtc.h>

class qcmDump
{
public:
    static IDE_RC getDumpObjectInfo( qcStatement * aStatement,
                                     qmsTableRef * aTableRef );
private:

    // �ε��� �̸��� ���ڷ� ������, �ε��� ���(smnIndexHeader)
    // �� dumpObj�� �־��ش�.
    // for D$*_INDEX_*
    static IDE_RC getIndexInfo( qcStatement    * aStatement,
                                qmsDumpObjList * aDumpObjList,
                                idBool           aEnableDiskIdx,
                                idBool           aEnableMemIdx,
                                idBool           aEnableVolIdx );


    // for D$*_TABLE_*
    // ���̺� �̸��� ���ڷ� ������, ���̺� ���(smcTableHeader)
    // �� dumpObj�� �־��ش�.
    static IDE_RC getTableInfo( qcStatement    * aStatement,
                                qmsDumpObjList * aDumpObjList,
                                idBool           aEnableDiskTable,
                                idBool           aEnableMemTable,
                                idBool           aEnableVolTable );
    
    // for D$*_TBS_*
    // ���̺� �����̽� �̸��� ���ڷ� ������, ���̺����̽�ID
    // �� dumpObj�� �־��ش�.
    static IDE_RC getTBSID( qcStatement    * aStatement,
                            qmsDumpObjList * aDumpObjList,
                            idBool           aEnableDiskTBS,
                            idBool           aEnableMemTBS,
                            idBool           aEnableVolTBS );

    // for D$*_DB_*
    /* TASK-4007 [SM] PBT�� ���� ��� �߰�
     * Disk, Memory, volatile Tablespace�� �������� dump�ϱ� ����
     * SID�� PID�� ���ڷ� �޾Ƶ��� */
    static IDE_RC getGRID( qcStatement    * aStatement,
                           qmsDumpObjList * aDumpObjList,
                           idBool           aEnableDiskTBS,
                           idBool           aEnableMemTBS,
                           idBool           aEnableVolTBS );
};


#endif /* _O_QCM_DUMP_H_ */
