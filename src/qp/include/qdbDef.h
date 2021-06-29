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
 * $Id$
 *
 * Description :
 *     PROJ-1877 Alter Column Modify�� ���� �ڷ� ���� ����
 *
 * ��� ���� :
 *
 * ��� :
 *
 **********************************************************************/

#ifndef _O_QDB_DEF_H_
#define _O_QDB_DEF_H_ 1

#include <qc.h>

// BUG-42920 DDL display data move progress
#define QDB_PROGRESS_ROWS_COUNT   (100000)

#define QDB_CONVERT_MATRIX_SIZE   (19)

//----------------------------------------------------------------
// PROJ-1877 Alter Table Modify Column�� ���� ������ ������ �߰��մϴ�.
//
// [type ����]
//
// type ������ ũ�� �� ������ ���еȴ�.
//
// 1. type ���������� �ڷᱸ���� �����Ͽ� length ����� ������ ���
//    8���� case
//    nchar->nvarchar
//    nvarchar->nchar
//    char->varchar
//    varchar->char
//    bit->varbit
//    varbit->bit
//    float->numeric
//    numeric->float
//
//    �̸� �� �� �з��غ��� ũ�� �������� ������ �� �ִ�.
//    a. padding type���� non-padding type���� ��ȯ
//       3���� case
//       nchar->nvarchar
//       char->varchar
//       bit->varbit
//
//       length Ȯ��� �ǽð� ddl�� �����ϴ�.
//       length ��Ҵ� ��� null�� ��� �ǽð� ddl�� ������ �����ϸ�
//       null�� �ƴ϶�� ddl�� �����Ѵ�.
//
//    b. non-padding type���� padding type���� ��ȯ
//       3���� case
//       nvarchar->nchar
//       varchar->char
//       varbit->bit
//
//       length Ȯ��� pad ���ڸ� �پ�� �ϹǷ� recreate
//       length ��Ҵ� length ������ �����ϴ� ��� �ǽð� ddl�� �����ϸ�
//       length ������ �������� �ʴ� ��� ddl�� �����Ѵ�.
//
//    c. float type���� numeric type���� ��ȯ
//       1���� case
//       float->numeric
//
//       length Ȯ��� ��Ҹ� schema�� ������ �� ������
//       float�� ���� numeric�� length�� �����ϴ� ��� �ǽð� ddl�� �����ϴ�.
//       �׷��� ���� ��� ddl�� �����Ѵ�.
//       ��, data loss �ɼ��� ����� ��� length ������ �������� �ʴ���
//       float->numeric conversion�� ���� recreate�Ѵ�.
//       �̶� round-off�� �߻��Ͽ� data loss�� �߻��� �� �ִ�.
//
//    d. numeric type���� float type���� ��ȯ
//       1���� case
//       numeric->float
//
//       length Ȯ��� �ǽð� ddl�� �����ϴ�.
//       length ��Ҵ� length ������ �����ϴ� ��츸 �ǽð� ddl�� �����ϴ�.
//       ��, data loss �ɼ��� ����� ��� length ������ �������� �ʴ���
//       numeric->float conversion�� ���� recreate�Ѵ�.
//       �̶� round-off�� �߻��Ͽ� data loss�� �߻��� �� �ִ�.
//
// 2. type �����̰� �ڷᱸ���� �ٸ� ���
//    ������ ��� ���
//    ��)
//    char(4)->integer
//    varchar(8)->date
//
//    type ������ ��� recreate�̸�
//    data loss�� �߻��ϴ� type ������ ��� ��� null�̾�� �����ϴ�.
//    ��, data loss �ɼ��� ����� ��� null�� �ƴϾ �����ϸ�
//    type conversion�� ���� recreate�Ѵ�.
//    �̶� ������ type�� ��� round-off�� �߻��Ͽ� data loss�� �߻��� �� ������
//    date type�� ��� default_date_format property�� ���� data loss�� �߻��� �� �ִ�.    
//
// [length ����]
//
// type ������� precision�̳� scale�� �����Ѵ�.
//    10���� case
//    nchar
//    nvarchar
//    char
//    varchar
//    bit
//    varbit
//    float
//    numeric
//    byte
//    nibble
//
//    �̸� �� �� �з��غ��� ũ�� �װ����� ������ �� �ִ�.
//    a. padding type�� length ����
//       4���� case
//       nchar
//       char
//       bit
//       byte
//
//       length Ȯ��� ��� null�̶�� �ǽð� ddl�� �����ϳ�
//       �׷��� ���� ���� recreate
//       length ��Ҵ� ��� null�̶�� �ǽð� ddl�� ����������
//       �׷��� ���� ���� ddl�� �����Ѵ�.
//
//    b. non-padding type�� length ����
//       4���� case
//       nvarchar
//       varchar
//       varbit
//       nibble
//
//       length Ȯ��� �ǽð� ddl�� �����ϴ�.
//       length ��Ҵ� length ������ �����ϴ� ��쿡�� �ǽð� ddl��
//       �����ϰ�, length ������ �������� �ʴ� ���� ddl�� �����Ѵ�.
//
//    c. float type�� length ����
//       1���� case
//       float
//
//       length Ȯ��� �ǽð� ddl�� �����ϴ�.
//       length ��Ҵ� length ������ �����ϴ� ��쿡�� �ǽð� ddl��
//       �����ϰ�, length ������ �������� �ʴ� ���� ddl�� �����Ѵ�.
//       ��, data loss �ɼ��� ����� ��� length ������ �������� �ʴ���
//       float canonize�� ���� recreate�Ѵ�.
//       �̶� round-off�� �߻��Ͽ� data loss�� �߻��� �� �ִ�.
//
//    d. numeric type�� length ����
//       1���� case
//       numeric
//
//       length Ȯ��� �ǽð� ddl�� �����ϴ�.
//       length ��Ҵ� ��� null�� �ƴ϶�� ddl�� �����Ѵ�.
//       ��, data loss �ɼ��� ����� ��� length ������ �������� �ʴ���
//       numeric canonize�� ���� recreate�Ѵ�.
//       �̶� round-off�� �߻��Ͽ� data loss�� �߻��� �� �ִ�.
//
// [���ǻ���]
//
// 1. �ǽð� ddl�̶����� ddl�� execution�� �ǽð�(real-time)�� ���ϴ� ������
//    ddl�� validation�� �ǽð��� �ƴ� �� �� �ִ�.
//
//    ���� ��� i1 varchar(5)�� varchar(3)���� �����ϴ� ddl��
//    execution ��ü�� �ǽð�������, i1�� length�� ��� varchar(3)��
//    �����ϴ��� �˻��ϱ� ���� validation������ �ǽð��� �ƴϴ�.
//    �ᱹ ����ڴ� �ǽð��� �ƴ϶�� ���� �� ������,
//    ���������δ� �ǽð� ddl�̶�� �����Ѵ�. ���� ��ǽð� ddl�� ���ؼ�
//    �ſ� ������ ddl�� �����Ѵ�. (scan �ѹ��� �ð� ����)
//
//    �ǽð� ddl
//    - �ǽð� validation + �ǽð� execution  
//    - ��ǽð� validation + �ǽð� execution
//
//    ��ǽð� ddl
//    - �ǽð� validation + ��ǽð� execution  
//    - ��ǽð� validation + ��ǽð� execution
//
// 2. �ǽð� ddl�� ������ table�� record�� ���ؼ��� ������ ������
//    index�� key�� ���� �ǽð� ddl�� ���� �Ұ����ϴ�.
//    �׷��Ƿ� �÷��� index�� �ִ� ��� �ش� index�� ������Ǿ�� �ϸ�
//    index ��������� ���� ddl�� execution�� �ǽð�(real-time)����
//    ������� ���� �� �ִ�.
//
//    ���������� ���ٸ� index�� �ִ� ��� ddl�� execution�� ��ǽð�����
//    ����ǹǷ� ��ǽð� ddl�� �����Ѵ�.
//
// 3. data loss�� ����ϴ� �ɼ��� ����Ͽ� type�̳� length�� �����ϴ� ���
//    ��쿡 ���� modify�� �����ϴ� ��찡 �߻��� �� �ִ�.
//
//    ���� ��� i1 float(2)�� i1 integer�� modify�Ϸ��� �Ҷ� i1�� unique index��
//    �ִ� ��� i1�� ����� ���� ���� unique index ������ ������ �� �ִ�.
//    �̰��� ddl �������� validation �������� �˻�Ǹ� ��������,
//    ����� index ������������ �߰ߵǾ� ddl�� rollback�� �ȴ�.
//    (1.1, 1.2) -> (1, 1) unique violation error
//
//----------------------------------------------------------------

//----------------------------------------------------------------
// PROJ-1877
// Modify Column �����,
// Table Column�� modify method ���
//----------------------------------------------------------------

typedef enum
{
    // �ƹ��͵� ���� �ʾƵ� alter table modify column ��ɼ���
    QD_TBL_COL_MODIFY_METHOD_NONE = 0,

    // column ���� meta ���游���� alter table modify column ��ɼ���
    QD_TBL_COL_MODIFY_METHOD_ALTER_META,
    
    // table ��������� alter table modify column ��ɼ���
    QD_TBL_COL_MODIFY_METHOD_RECREATE_TABLE
    
} qdTblColModifyMethod;

//----------------------------------------------------------------
// PROJ-1911
// Modify Column ���� ��, 
// Index Column�� modify method ��� 
//----------------------------------------------------------------

typedef enum
{
    // index�� Į���� ���� meta ���游����
    // alter table modify column ��ɼ���
    QD_IDX_COL_MODIFY_METHOD_ALTER_META = 0,

    // index ��������� alter table modify column ��ɼ���
    QD_IDX_COL_MODIFY_METHOD_RECREATE_INDEX
} qdIdxColModifyMethod;

typedef struct qdbIdxColModify
{
    UInt                   indexId;
    qdIdxColModifyMethod   method;
}qdIdxColModify;

//----------------------------------------------------------------
// PROJ-1877
// verify column command
//----------------------------------------------------------------

typedef enum
{
    //----------------------------------------------------------------
    // function: �ʱⰪ
    // action  : �ƹ��͵� ���� �ʴ´�.
    //----------------------------------------------------------------
    QD_VERIFY_NONE = 0,

    //----------------------------------------------------------------
    // function: value�� ��� not null�ΰ�?
    //
    // action  : null to not null�� ���Ǹ� null�� �߰ߵǴ� ���
    //           DDL�� �����Ѵ�.
    //
    // example : create table t1(i1 integer null);
    //           alter table t1 modify (i1 not null);
    //           i1�� ��� not null�̾�� alter�� �����ϴ�. �׷���
    //           i1�� null�� ��� alter�� �Ұ����ϴ�.
    //----------------------------------------------------------------
    QD_VERIFY_NOT_NULL,
    
    //----------------------------------------------------------------
    // function: value�� ��� null �ΰ�?
    //
    // action  : disk table���� value�� ��� null�� ��� �ǽð�
    //           �ϼ����� ���డ���ϴ�.
    //           null�� �ƴ� ��� DDL�� �����Ѵ�.
    //
    // example : create table t1(i1 char(3));
    //           alter table t1 modify (i1 char(1));
    //           i1�� ��� null�̾�� alter�� �����ϴ�. �׷���
    //           i1�� null�� �ƴ� ��� alter�� �Ұ����ϴ�.
    //----------------------------------------------------------------
    QD_VERIFY_NULL,
    
    //----------------------------------------------------------------
    // function: value�� ��� null �ΰ�?
    //
    // action  : disk table���� value�� ��� null�� ��� �ǽð�
    //           �ϼ����� ���డ���ϴ�.
    //           null�� �ƴ� ��� ��ǽð����� �����Ѵ�.
    //
    // example : create table t1(i1 char(3));
    //           alter table t1 modify (i1 char(5));
    //           i1�� ��� null�� ��� �ǽð� alter�� �����ϴ�. �׷���
    //           i1�� null�� �ƴ� ��� recreate table�� �����Ѵ�.
    //----------------------------------------------------------------
    QD_VERIFY_NULL_ONLY,
    
    //----------------------------------------------------------------
    // function: value�� ��� null�̰ų� ������ size�����ΰ�?
    //
    // action  : value�� ��� null�� �ƴϰ� ������ size���ϰ� �ƴ�
    //           ��� DDL�� �����Ѵ�.
    //
    // example : create table t1(i1 varchar(5));
    //           alter table t1 modify (i1 varchar(3));
    //           i1�� ��� null�̰ų� ���̰� 3���� �۰ų� ���� ��츸
    //           alter�� �����ϴ�. �׷��� i1�� ���̰� 3���� ū ��� alter��
    //           �Ұ����ϴ�.
    //----------------------------------------------------------------
    QD_VERIFY_NULL_OR_UNDER_SIZE,
    
    //----------------------------------------------------------------
    // function: value�� ��� null�̰ų� ������ size�ΰ�?
    //
    // action  : disk table���� value�� ��� null�̰ų� Ư�� size��
    //           ��� �ǽð� �ϼ����� ���డ���ϴ�.
    //           �׸��� disk table���� value�� ��� null�̰ų�
    //           Ư�� size���� ���� ��� ��ǽð����� �����Ѵ�.
    //           �׷��� size���� ū ��찡 ������ DDL�� �����Ѵ�.
    //
    // example : create table t1(i1 varchar(5));
    //           alter table t1 modify (i1 char(3));
    //           i1�� ��� null�̰ų� ���̰� ��Ȯ�� 3�� ��� �ǽð� alter��
    //           �����ϰ�, i1�� ��� null�̰ų� ���̰� 3���� �۰ų� ����
    //           ��� recreate table�� �����Ѵ�. �׷��� i1�� ���̰� 3����
    //           ū ��� alter�� �Ұ����ϴ�.
    //----------------------------------------------------------------
    QD_VERIFY_NULL_OR_EXACT_OR_UNDER_SIZE,
    
    //----------------------------------------------------------------
    // function: value�� ������ srid�ΰ�?
    //
    // action  : disk table���� value�� ������ srid�̰ų� 0��
    //           ��� �ǽð� �ϼ����� ���డ���ϴ�.
    //           �׸��� disk table���� value�� ��� ������ srid Ȥ�� 0��
    //           �ƴ� ��� DDL�� �����Ѵ�.
    //
    // example : create table t1(i1 geometry);
    //           alter table t1 modify (i1 srid 100);
    //           i1�� ��� srid�� 0�̰ų� 100�� ��� �ǽð� alter��
    //           �����ϰ�, i1�� 0�̳� 100�� �ƴ� ��� alter�� �Ұ����ϴ�.
    //----------------------------------------------------------------
    QD_VERIFY_SRID
    
} qdVerifyCommand;

//----------------------------------------------------------------
// PROJ-1911
// modify column ���� ��, disk ���� Ÿ���� ����Ǵ��� ���� 
//----------------------------------------------------------------
typedef enum
{
    // Disk�� ������� ���� 
    QD_CHANGE_STORED_TYPE_NONE,

    // Disk ���� Ÿ���� ������� ����
    // ex ) char->varchar, char->char ��� 
    QD_CHANGE_STORED_TYPE_FALSE,

    // Disk ���� Ÿ���� �����
    // ex) char->integer, varchar->date ��� 
    QD_CHANGE_STORED_TYPE_TRUE
} qdChangeStoredType;

//----------------------------------------------------------------
// PROJ-1877
// verify column list�� �ѹ��� scan���� modify ��� �÷��� �Ѳ�����
// �˻��Ͽ� alter�� �Ұ������� �˻��ϰų� modify method�� ���� �����Ѵ�.
//
// example : create table t1(i1 varchar(5), i2 varchar(5), i3 char(3));
//           alter table t1 modify (i1 varchar(3) not null, i2 char(3), i3 char(5));
//
//                      +----------------------------+  
//                      |i1                          |  
// verifyColumnList --> |QD_VERIFY_NULL_OR_UNDER_SIZE|
//                      |3                           |
//                      +----------------------------+
//                                |
//                      +------------------+
//                      |i1                |
//                      |QD_VERIFY_NOT_NULL|
//                      |0                 |
//                      +------------------+
//                                |
//                      +-------------------------------------+
//                      |i2                                   |
//                      |QD_VERIFY_NULL_OR_EXACT_OR_UNDER_SIZE|
//                      |3                                    |
//                      +-------------------------------------+
//                                |
//                      +-------------------+
//                      |i3                 |
//                      |QD_VERIFY_NULL_ONLY|
//                      |0                  |
//                      +-------------------+
//
//----------------------------------------------------------------

typedef struct qdVerifyColumn
{
    qcmColumn          * column;
    qdVerifyCommand      command;
    UInt                 size;
    SInt                 precision;
    SInt                 scale;
    qdChangeStoredType   changeStoredType;
    qdVerifyColumn     * next;
} qdVerifyColumn;

//----------------------------------------------------------------
// PROJ-1877
// convert for smiValue list
//----------------------------------------------------------------

typedef struct qdbConvertContext
{
    idBool               needConvert;
    mtvConvert         * convert;

    idBool               needCanonize;
    void               * canonBuf;

    // PROJ-2002 Column Security
    idBool               needEncrypt;
    idBool               needDecrypt;
    void               * encBuf;

    qdbConvertContext  * next;
    
} qdbConvertContext;

typedef struct qdbCallBackInfo
{
    // record ������ convert�� ���� �޸� ����
    iduMemoryStatus    * qmxMemStatus;
    iduMemory          * qmxMem;

    // column value�� convert�� ���� �ڷᱸ��
    qcTemplate         * tmplate;
    qcmTableInfo       * tableInfo;
    qdbConvertContext  * convertContextList;

    // convert context�� ã�� ���� pointer
    qdbConvertContext  * convertContextPtr;

    // null smiValue list
    smiValue           * nullValues;

    /* PROJ-1090 Function-based Index */
    idBool               hasDefaultExpr;
    qmsTableRef        * srcTableRef;
    qcmColumn          * dstTblColumn;
    void               * rowBuffer;

    /* PROJ-2210 autoincrement column */
    qcStatement        * statement;
    qcmColumn          * srcColumns;
    qcmColumn          * dstColumns;

    // BUG-42920 DDL display data move progress
    qcmTableInfo       * partitionInfo; // partition name, partition ����.
    ULong                progressRows;  // insert�Ǿ��� ����Ÿ�� ����.
    
} qdbCallBackInfo;

#endif /* _O_QDB_DEF_H_ */
