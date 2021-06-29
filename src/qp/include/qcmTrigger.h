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
 * $Id: qcmTrigger.h 82075 2018-01-17 06:39:52Z jina.kim $
 *
 * Description :
 *
 *     [PROJ-1359] Trigger
 *
 *     Trigger�� ���� Meta �� Cache ������ ���� �Լ�
 *
 * ��� ���� :
 *
 * ��� :
 *
 **********************************************************************/

#ifndef _O_QCM_TRIGGER_H_
#define _O_QCM_TRIGGER_H_  1

#include <qcm.h>

// 100���� ���ڿ��� ������ ��,
// ������ ���� ������ �����ϴ� ���
#define QCM_TRIGGER_SUBSTRING_LEN                  (100)
#define QCM_TRIGGER_SUBSTRING_LEN_STR              "100"

class qcmTrigger
{
public:

    // Trigger OID �� Table ID�� ȹ��
    static IDE_RC getTriggerOID( qcStatement    * aStatement,
                                 UInt             aUserID,
                                 qcNamePosition   aTriggerName,
                                 smOID          * aTriggerOID,
                                 UInt           * aTableID,
                                 idBool         * aIsExist );

    // Trigger Cycle�� ���� ���� �˻�
    // aTableID�� Trigger�� �����ϴ� Table�� aCycleTableID��
    // ������ ��� Cycle�� ������.
    static IDE_RC checkTriggerCycle( qcStatement * aStatement,
                                     UInt          aTableID,
                                     UInt          aCycleTableID );

    // Meta Table�� Trigger ������ ����Ѵ�.
    static IDE_RC addMetaInfo( qcStatement * aStatement,
                               void        * sTriggerHandle );

    // Meta Table���� Trigger ������ �����Ѵ�.
    static IDE_RC removeMetaInfo( qcStatement * aStatement,
                                  smOID         aTriggerOID );

    // Meta Table�κ��� Table Cache�� Trigger ������ �����Ѵ�.
    static IDE_RC getTriggerMetaCache( smiStatement * aSmiStmt,
                                       UInt           aTableID,
                                       qcmTableInfo * aTableInfo );

    /* PROJ-2600 Online DDL for Tablespace Alteration */
    static IDE_RC copyTriggerMetaInfo( qcStatement * aStatement,
                                       UInt          aToTableID,
                                       smOID         aToTriggerOID,
                                       SChar       * aToTriggerName,
                                       UInt          aFromTableID,
                                       smOID         aFromTriggerOID,
                                       SChar       * aDDLText,
                                       SInt          aDDLTextLength );

    /* PROJ-2600 Online DDL for Tablespace Alteration */
    static IDE_RC updateTriggerStringsMetaInfo( qcStatement * aStatement,
                                                UInt          aTableID,
                                                smOID         aTriggerOID,
                                                SInt          aDDLTextLength );

    /* PROJ-2600 Online DDL for Tablespace Alteration */
    static IDE_RC insertTriggerStringsMetaInfo( qcStatement * aStatement,
                                                UInt          aTableID,
                                                smOID         aTriggerOID,
                                                SChar       * aDDLText,
                                                SInt          aDDLTextLength );

    /* PROJ-2600 Online DDL for Tablespace Alteration */
    static IDE_RC removeTriggerStringsMetaInfo( qcStatement * aStatement,
                                                smOID         aTriggerOID );

    /* PROJ-2600 Online DDL for Tablespace Alteration */
    static IDE_RC renameTriggerMetaInfoForSwap( qcStatement    * aStatement,
                                                qcmTableInfo   * aSourceTableInfo,
                                                qcmTableInfo   * aTargetTableInfo,
                                                qcNamePosition   aNamesPrefix );

    /* PROJ-2600 Online DDL for Tablespace Alteration */
    static IDE_RC swapTriggerDMLTablesMetaInfo( qcStatement * aStatement,
                                                UInt          aTargetTableID,
                                                UInt          aSourceTableID );

private:

};
#endif // _O_QCM_TRIGGER_H_
