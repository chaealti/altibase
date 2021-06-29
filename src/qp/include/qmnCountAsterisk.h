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
 * $Id: qmnCountAsterisk.h 82075 2018-01-17 06:39:52Z jina.kim $
 *
 * Description :
 *     CoUNT (*) Plan Node
 *
 *     ������ �𵨿��� COUNT(*)���� ó���ϱ� ���� Ư���� Plan Node �̴�.
 *
 * ��� ���� :
 *
 * ��� :
 *
 **********************************************************************/

#ifndef _O_QMN_COUNT_ASTERISK_H_
#define _O_QMN_COUNT_ASTERISK_H_ 1

#include <mt.h>
#include <qmc.h>
#include <qmnDef.h>
#include <qmnScan.h>

//-----------------
// Code Node Flags
//-----------------

/* qmncCUNT.flag                                     */
// Count(*) ó�� ���
// Table Handle�� �̿��� ����� Cursor�� �̿��� ���

#define QMNC_CUNT_METHOD_MASK              (0x00000001)
#define QMNC_CUNT_METHOD_HANDLE            (0x00000000)
#define QMNC_CUNT_METHOD_CURSOR            (0x00000001)

/* qmncCUNT.flag                                     */
// �����Ǵ� table�� fixed table �Ǵ� performance view������ ������ ��Ÿ��.
// fixed table �Ǵ� performance view�� ���ؼ���
// table�� ���� IS LOCK�� ���� �ʵ��� �ϱ� ����.
#define QMNC_CUNT_TABLE_FV_MASK            (0x00000002)
#define QMNC_CUNT_TABLE_FV_FALSE           (0x00000000)
#define QMNC_CUNT_TABLE_FV_TRUE            (0x00000002)

/* qmncCUNT.flag                                     */
// To fix BUG-12742
// index�� �ݵ�� ����ؾ� �ϴ��� ���θ� ��Ÿ����.
#define QMNC_CUNT_FORCE_INDEX_SCAN_MASK    (0x00000004)
#define QMNC_CUNT_FORCE_INDEX_SCAN_FALSE   (0x00000000)
#define QMNC_CUNT_FORCE_INDEX_SCAN_TRUE    (0x00000004)

//-----------------
// Data Node Flags
//-----------------

/* qmndCUNT.flag                                     */
// First Initialization Done
#define QMND_CUNT_INIT_DONE_MASK           (0x00000001)
#define QMND_CUNT_INIT_DONE_FALSE          (0x00000000)
#define QMND_CUNT_INIT_DONE_TRUE           (0x00000001)

/* qmndCUNT.flag                                     */
// Cursor Status
#define QMND_CUNT_CURSOR_MASK              (0x00000002)
#define QMND_CUNT_CURSOR_CLOSED            (0x00000000)
#define QMND_CUNT_CURSOR_OPENED            (0x00000002)

/* qmndCUNT.flag                                     */
// Isolation Level�� ���Ͽ� ���� ����� �ٲ� �� �ִ�.
// Table Handle�� �̿��� ����� Cursor�� �̿��� ���
#define QMND_CUNT_METHOD_MASK              (0x00000004)
#define QMND_CUNT_METHOD_HANDLE            (0x00000000)
#define QMND_CUNT_METHOD_CURSOR            (0x00000004)

/* qmndCUNT.flag                                     */
// plan�� selected method�� ���õǾ����� ���θ� ���Ѵ�.
#define QMND_CUNT_SELECTED_METHOD_SET_MASK   (0x00000008)
#define QMND_CUNT_SELECTED_METHOD_SET_TRUE   (0x00000008)
#define QMND_CUNT_SELECTED_METHOD_SET_FALSE  (0x00000000)

typedef struct qmncCUNT
{
    //---------------------------------
    // Code ���� ���� ����
    //---------------------------------

    qmnPlan        plan;
    UInt           flag;
    UInt           planID;

    //---------------------------------
    // CUNT ���� ����
    //---------------------------------

    qtcNode      * countNode;           // Count Asterisk Node

    UShort         tupleRowID;          // Tuple ID
    UShort         depTupleRowID;       // Dependent Tuple Row ID

    qmsTableRef  * tableRef;
    smSCN          tableSCN;            // Table SCN

    void         * dumpObject;          // BUG-16651 Dump Object

    UInt                lockMode;       // Lock Mode
    smiCursorProperties cursorProperty; // Cursor Property

    //---------------------------------
    // Predicate ����
    //---------------------------------

    qmncScanMethod       method;
    //---------------------------------
    // Display ���� ����
    //---------------------------------

    qmsNamePosition      tableOwnerName;     // Table Owner Name
    qmsNamePosition      tableName;          // Table Name
    qmsNamePosition      aliasName;          // Alias Name

    qmoScanDecisionFactor * sdf;
} qmncCUNT;

typedef struct qmndCUNT
{
    //---------------------------------
    // Data ���� ���� ����
    //---------------------------------
    qmndPlan            plan;
    doItFunc            doIt;
    UInt              * flag;

    //---------------------------------
    // CUNT ���� ����
    //---------------------------------

    SLong                       countValue;      // COUNT(*)�� ��
    mtcTuple                  * depTuple;        // dependent tuple
    UInt                        depValue;        // dependency value

    smiTableCursor              cursor;          // Cursor
    smiCursorProperties         cursorProperty;  // Cursor ���� ����

    //---------------------------------
    // Predicate ����
    //---------------------------------

    smiRange                  * fixKeyRangeArea; // Fixed Key Range ����
    smiRange                  * fixKeyRange;     // Fixed Key Range
    UInt                        fixKeyRangeSize; // Fixed Key Range ũ��

    smiRange                  * fixKeyFilterArea; //Fixed Key Filter ����
    smiRange                  * fixKeyFilter;    // Fixed Key Filter
    UInt                        fixKeyFilterSize;// Fixed Key Filter ũ��
    
    smiRange                  * varKeyRangeArea; // Variable Key Range ����
    smiRange                  * varKeyRange;     // Variable Key Range
    UInt                        varKeyRangeSize; // Variable Key Range ũ��

    smiRange                  * varKeyFilterArea; // Variable Key Filter ����
    smiRange                  * varKeyFilter;    // Variable Key Filter
    UInt                        varKeyFilterSize;// Variable Key Filter ũ��

    smiRange                  * keyRange;       // ���� Key Range
    smiRange                  * keyFilter;      // ���� Key Filter
    smiRange                  * ridRange;

    // Filter ���� CallBack ����
    smiCallBack                 callBack;        // Filter CallBack
    qtcSmiCallBackDataAnd       callBackDataAnd; //
    qtcSmiCallBackData          callBackData[3]; // �� ������ Filter�� ������.

    qmndScanFixedTable          fixedTable;         // BUG-42639 Monitoring Query
    smiFixedTableProperties     fixedTableProperty; // BUG-42639 Mornitoring Query
} qmndCUNT;

class qmnCUNT
{
public:
    //------------------------
    // Base Function Pointer
    //------------------------

    // �ʱ�ȭ
    static IDE_RC init( qcTemplate * aTemplate,
                        qmnPlan    * aPlan );

    // ����
    static IDE_RC fini( qcTemplate * aTemplate,
                        qmnPlan    * aPlan );

    // ���� �Լ�
    static IDE_RC doIt( qcTemplate * aTemplate,
                        qmnPlan    * aPlan,
                        qmcRowFlag * aFlag );

    // Null Padding
    static IDE_RC padNull( qcTemplate * aTemplate,
                           qmnPlan    * aPlan );

    // Plan ���� ���
    static IDE_RC printPlan( qcTemplate   * aTemplate,
                             qmnPlan      * aPlan,
                             ULong          aDepth,
                             iduVarString * aString,
                             qmnDisplay     aMode );

    //------------------------
    // mapping by doIt() function pointer
    //------------------------

    // ȣ��Ǿ�� �ȵ�.
    static IDE_RC doItDefault( qcTemplate * aTemplate,
                               qmnPlan    * aPlan,
                               qmcRowFlag * aFlag );

    // COUNT(*)�� ����
    static IDE_RC doItFirst( qcTemplate * aTemplate,
                             qmnPlan    * aPlan,
                             qmcRowFlag * aFlag );

    // ��� ������ ����
    static IDE_RC doItLast( qcTemplate * aTemplate,
                            qmnPlan    * aPlan,
                            qmcRowFlag * aFlag );


    // PROJ-1446 Host variable�� ������ ���� ����ȭ
    // plan���� data ������ selected method�� ���õǾ����� �˷��ش�.
    static IDE_RC notifyOfSelectedMethodSet( qcTemplate * aTemplate,
                                             qmncCUNT   * aCodePlan );
private:

    //------------------------
    // �ʱ�ȭ ���� �Լ�
    //------------------------

    // ���� �ʱ�ȭ
    static IDE_RC firstInit( qcTemplate * aTemplate,
                             qmncCUNT   * aCodePlan,
                             qmndCUNT   * aDataPlan );

    // Fixed Key Range�� ���� Range ���� �Ҵ�
    static IDE_RC allocFixKeyRange( qcTemplate * aTemplate,
                                    qmncCUNT   * aCodePlan,
                                    qmndCUNT   * aDataPlan );

    // Variable Key Filter�� ���� Range ���� �Ҵ�
    static IDE_RC allocFixKeyFilter( qcTemplate * aTemplate,
                                     qmncCUNT   * aCodePlan,
                                     qmndCUNT   * aDataPlan );

    // Variable Key Range�� ���� Range ���� �Ҵ�
    static IDE_RC allocVarKeyRange( qcTemplate * aTemplate,
                                    qmncCUNT   * aCodePlan,
                                    qmndCUNT   * aDataPlan );

    // Variable Key Filter�� ���� Range ���� �Ҵ�
    static IDE_RC allocVarKeyFilter( qcTemplate * aTemplate,
                                     qmncCUNT   * aCodePlan,
                                     qmndCUNT   * aDataPlan );

    // Rid Filter �� ���� Range ���� �Ҵ�
    static IDE_RC allocRidRange( qcTemplate * aTemplate,
                                 qmncCUNT   * aCodePlan,
                                 qmndCUNT   * aDataPlan );

    // Dependent Tuple�� ���� ���� �˻�
    static IDE_RC checkDependency( qmndCUNT   * aDataPlan,
                                   idBool     * aDependent );

    // COUNT(*) �� ȹ��
    static IDE_RC getCountValue( qcTemplate * aTemplate,
                                 qmncCUNT   * aCodePlan,
                                 qmndCUNT   * aDataPlan );

    // Handle�� �̿��� COUNT(*)�� ȹ��
    static IDE_RC getCountByHandle( qcTemplate * aTemplate,
                                    qmncCUNT   * aCodePlan,
                                    qmndCUNT   * aDataPlan );

    // Cursor�� �̿��� COUNT(*)�� ȹ��
    static IDE_RC getCountByCursor( qcTemplate * aTemplate,
                                    qmncCUNT   * aCodePlan,
                                    qmndCUNT   * aDataPlan );

    /* BUG-42639 Monitoring query */
    static IDE_RC getCountByFixedTable( qcTemplate * aTemplate,
                                        qmncCUNT   * aCodePlan,
                                        qmndCUNT   * aDataPlan );
    //------------------------
    // Cursor ���� �Լ�
    //------------------------

    static IDE_RC makeKeyRangeAndFilter( qcTemplate * aTemplate,
                                         qmncCUNT   * aCodePlan,
                                         qmndCUNT   * aDataPlan );

    static IDE_RC makeRidRange( qcTemplate * aTemplate,
                                qmncCUNT   * aCodePlan,
                                qmndCUNT   * aDataPlan );

    // Cursor�� Open
    static IDE_RC openCursor( qcTemplate * aTemplate,
                              qmncCUNT   * aCodePlan,
                              qmndCUNT   * aDataPlan );

    //------------------------
    // Plan Display ���� �Լ�
    //------------------------

    // Access������ ����Ѵ�.
    static IDE_RC printAccessInfo( qmncCUNT     * aCodePlan,
                                   qmndCUNT     * aDataPlan,
                                   iduVarString * aString,
                                   qmnDisplay     aMode );

    // Predicate�� �� ������ ����Ѵ�.
    static IDE_RC printPredicateInfo( qcTemplate   * aTemplate,
                                      qmncCUNT     * aCodePlan,
                                      ULong          aDepth,
                                      iduVarString * aString );

    // Key Range�� �� ������ ���
    static IDE_RC printKeyRangeInfo( qcTemplate   * aTemplate,
                                     qmncCUNT     * aCodePlan,
                                     ULong          aDepth,
                                     iduVarString * aString );

    // Key Filter�� �� ������ ���
    static IDE_RC printKeyFilterInfo( qcTemplate   * aTemplate,
                                      qmncCUNT     * aCodePlan,
                                      ULong          aDepth,
                                      iduVarString * aString );

    // Filter�� �� ������ ���
    static IDE_RC printFilterInfo( qcTemplate   * aTemplate,
                                   qmncCUNT     * aCodePlan,
                                   ULong          aDepth,
                                   iduVarString * aString );


    // code plan�� qmncScanMethod�� ����� ��,
    // sdf�� candidate�� ���� qmncScanMethod�� ����� �� �Ǵ��Ѵ�.
    static qmncScanMethod * getScanMethod( qcTemplate * aTemplate,
                                           qmncCUNT   * aCodePlan );
};

#endif /* _O_QMN_COUNT_ASTERISK_H_ */
