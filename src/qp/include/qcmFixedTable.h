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
 * $Id: qcmFixedTable.h 82075 2018-01-17 06:39:52Z jina.kim $
 *
 * Description :
 *
 *     ID, SM�� ���� ���ǵ� fixedTable�� ���ؼ�
 *     QP�� ������� selection �� �ϱ� ���ؼ��� meta cache�� �ʿ��ϸ�,
 *     �̿� ���� ���� ����, ���� ȹ����� �����Ѵ�.
 *
 * ��� ���� :
 *
 * ��� :
 *
 *
 **********************************************************************/

#ifndef _O_QCM_FIXEDTABLE_H_
#define _O_QCM_FIXEDTABLE_H_ 1

#include    <iduFixedTable.h>
#include    <smiDef.h>
#include    <qc.h>
#include    <qtc.h>

class qcmFixedTable
{

private:
    // meta cache���� ����ÿ� ���.
    static IDE_RC getQcmColumn( qcmTableInfo  * aTableInfo );

    static IDE_RC createColumn( qcmTableInfo * aTableInfo,
                                void        ** aMtcColumn,
                                UInt           aIndex );

public:

    static IDE_RC validateTableName(
        qcStatement    *aStatement,
        qcNamePosition  aTableName );


    static IDE_RC getTableInfo(
        qcStatement    *aStatement,
        qcNamePosition  aTableName,
        qcmTableInfo  **aTableInfo,         // out
        smSCN          *aSCN,
        void          **aTableHandle );
    
    static IDE_RC getTableInfo(
        qcStatement     *aStatement,
        UChar           *aTableName,
        SInt             aTableNameSize,
        qcmTableInfo   **aTableInfo,        // out
        smSCN           *aSCN,
        void           **aTableHandle );   
        

    static IDE_RC getTableHandleByName(
        UChar        * aTableName,
        SInt           aTableNameSize,
        void        ** aTableHandle,        // out
        smSCN        * aSCN );              // out

    static IDE_RC makeAndSetQcmTableInfo( idvSQL * aStatistics,
                                          SChar  * aNameType );

    static IDE_RC makeNullRow4FT( qcmColumn  * aColumns,
                                  void      ** aNullRow );

    static IDE_RC makeTrimmedName( SChar * aDst,
                                   SChar * aSrc );

    static IDE_RC checkDuplicatedTable( qcStatement      * aStatement,
                                        qcNamePosition     aTableName );

    /* BUG-43006 FixedTable Indexing Filter */
    static IDE_RC getQcmIndices( qcmTableInfo * aTableInfo );
};

#endif /* _O_QCM_FIXEDTABLE_H_ */
