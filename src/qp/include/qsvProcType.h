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
 * $Id: qsvProcType.h 83112 2018-05-29 02:04:03Z ahra.cho $
 **********************************************************************/

#ifndef _Q_QSV_PROC_TYPE_H_
#define _Q_QSV_PROC_TYPE_H_ 1

#include <qc.h>
#include <qsParseTree.h>

class qsvProcType
{
public:
    // PROJ-1075
    // type ������ validation
    static IDE_RC validateTypeDeclare( qcStatement    * aStatement,
                                       qsTypes        * aType,
                                       qcNamePosition * aTypeName,
                                       idBool           aIsTriggerVariable );

    // PROJ-1075 local type�˻�, typeset type �˻�.
    // ã�� ���ϸ� ������ ������.
    static IDE_RC checkTypes(
        qcStatement     * aStatement,
        qsVariableItems * aVariableItem,
        qcNamePosition  * aUserName,
        qcNamePosition  * aLabelName,
        qcNamePosition  * aTypeName,
        qsTypes        ** aType );

    // PROJ-1075 Local Type �˻�
    static IDE_RC searchLocalTypes(
        qcStatement     * aStatement,
        qsVariableItems * aLocalVariableItems,
        qsVariableItems * aVariableItem,
        qcNamePosition  * aTypeName,
        idBool          * aIsFound,
        qsTypes        ** aType );

    // PROJ-1075 typeset���� type �˻�
    static IDE_RC searchTypesFromTypeSet(
        qcStatement     * aStatement,
        qcNamePosition  * aUserName,
        qcNamePosition  * aLabelName,
        qcNamePosition  * aTypeName,
        idBool          * aIsFound,
        qsTypes        ** aType );

    // PROJ-1075 rowtype ����
    // alloc�� �Լ� �ȿ��� ����.
    static IDE_RC makeRowType( qcStatement     * aStatement,
                               qcmTableInfo    * aTableInfo,
                               qcNamePosition  * aTypeName,
                               idBool            aTriggerVariable,
                               qsTypes        ** aType );
    
    static IDE_RC copyQsType( qcStatement     * aStatement,
                               qsTypes        * aOriType,
                               qsTypes       ** aType );

    static IDE_RC copyColumn( qcStatement * aStatement,
                              mtcColumn   * aOriColumn,
                              mtcColumn   * aColumn,
                              mtdModule  ** aModule );

    // PROJ-1073 Package
    static IDE_RC checkPkgTypes( qcStatement     * aStatement,
                                 qcNamePosition  * aUserName,
                                 qcNamePosition  * aTableName,
                                 qcNamePosition  * aTypeName,
                                 idBool          * aIsFound,
                                 qsTypes        ** aType );

    static IDE_RC searchTypesFromPkg( qsxPkgInfo      * aPkgInfo,
                                      qcNamePosition  * aTypeName,
                                      idBool          * aIsFound,
                                      qsTypes        ** aType );

    /* BUG-41720
       ���� package�� ����� array type�� return value�� ��� �����ؾ� �Ѵ�. */
    static IDE_RC searchPkgLocalTypes( qcStatement     * aStatement,
                                       qsVariableItems * aVariableItem,
                                       qcNamePosition  * aTypeName,
                                       idBool          * aIsFound,
                                       qsTypes        ** aType );

    // BUG- 46032
    static IDE_RC makeArrayTypeColumnByModule( iduVarMemList   * aMemory,
                                               qtcModule       * aRowModule,
                                               qsTypes        ** aType );
private:
    static IDE_RC validateRecordTypeDeclare(
        qcStatement    * aStatement,
        qsTypes        * aType,
        qcNamePosition * aTypeName,
        idBool           aIsTriggerVariable );

    static IDE_RC validateArrTypeDeclare(
        qcStatement    * aStatement,
        qsTypes        * aType,
        qcNamePosition * aTypeName );

    // PROJ-1386 Dynamic-SQL
    static IDE_RC validateRefCurTypeDeclare(
        qcStatement    * aStatement,
        qsTypes        * aType,
        qcNamePosition * aTypeName );

    // BUG-36772
    static IDE_RC makeRowTypeColumn( qsTypes     * aType,
                                     idBool        aIsTriggerVariable );

    static IDE_RC makeRecordTypeColumn( qcStatement     * aStatement,
                                        qsTypes         * aType );
};

#endif  // _Q_QSV_PROC_TYPE_H_
