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
 * $Id: qmoStatMgr.h 90850 2021-05-17 05:09:54Z donovan.seo $
 *
 * Description :
 *     Statistical Information Manager
 *
 *     ������ ���� ���� �ǽð� ��� ������ ������ ����Ѵ�.
 *          - Table�� Record ����
 *          - Table�� Disk Page ����
 *          - Index�� Cardinality
 *          - Column�� Cardinality
 *          - Column�� MIN Value
 *          - Column�� MAX Value
 *
 * ��� ���� :
 *
 * ��� :
 *
 **********************************************************************/

#ifndef _O_QMO_STAT_MGR_H_
#define _O_QMO_STAT_MGR_H_ 1

#include <mtdTypes.h>
#include <qc.h>
#include <qmsParseTree.h>
#include <qmoDef.h>
#include <qmoHint.h>
#include <smiDef.h>

//-------------------------------------------------------
// default �������
//-------------------------------------------------------
// TASK-6699 TPC-H ���� ����
# define QMO_STAT_SYSTEM_SREAD_TIME                  (87.70000000)
# define QMO_STAT_SYSTEM_MREAD_TIME                  (17.25000000)
# define QMO_STAT_SYSTEM_MTCALL_TIME                 (0.01130000)
# define QMO_STAT_SYSTEM_HASH_TIME                   (0.01500000)
# define QMO_STAT_SYSTEM_COMPARE_TIME                (0.01130000)
# define QMO_STAT_SYSTEM_STORE_TIME                  (0.05000000)

// BUG-40913 v$table ���� ���ǰ� ������ ����
# define QMO_STAT_PFVIEW_RECORD_COUNT                (1024)

# define QMO_STAT_TABLE_RECORD_COUNT                 (10240)
# define QMO_STAT_TABLE_DISK_PAGE_COUNT              (10)
# define QMO_STAT_TABLE_AVG_ROW_LEN                  (100)
# define QMO_STAT_TABLE_MEM_ROW_READ_TIME            (1)
# define QMO_STAT_TABLE_DISK_ROW_READ_TIME           (10)

# define QMO_STAT_INDEX_KEY_NDV                      (100)
# define QMO_STAT_INDEX_AVG_SLOT_COUNT               (100)
# define QMO_STAT_INDEX_HEIGHT                       (1)
# define QMO_STAT_INDEX_DISK_PAGE_COUNT              (1)
# define QMO_STAT_INDEX_CLUSTER_FACTOR               (1)

# define QMO_STAT_COLUMN_NDV                         (100)
# define QMO_STAT_COLUMN_NULL_VALUE_COUNT            (0)
# define QMO_STAT_COLUMN_AVG_LEN                     (20)

// bug-37125
// �������� 1���� �۰� ������ �ǰ�
// 0�� ���ô� ��쵵 �ֱ� ������ �ּ����� ������ �����Ѵ�.
# define QMO_STAT_TIME_MIN                           (QMO_COST_EPS)

// TASK-6699 TPC-H ���� ����
# define QMO_STAT_READROW_TIME_MIN                   (0.05290000)

//-------------------------------------------------------
// qmoIdxCardInfo�� qmoColCardInfo�� flag �ʱ�ȭ
//-------------------------------------------------------
/* qmoColCardInfo�� qmoIdxCardInfo�� flag�� �ʱ�ȭ��Ŵ */
# define QMO_STAT_CLEAR                       (0x00000000)

//-------------------------------------------------------
// qmoIdxCardInfo�� flag ����
//-------------------------------------------------------

/* qmoIdxCardInfo.flag                                 */
// NO INDEX Hint�� �־��� ���
# define QMO_STAT_CARD_IDX_HINT_MASK          (0x00000007)
# define QMO_STAT_CARD_IDX_HINT_NONE          (0x00000000)
# define QMO_STAT_CARD_IDX_NO_INDEX           (0x00000001)
# define QMO_STAT_CARD_IDX_INDEX              (0x00000002)
# define QMO_STAT_CARD_IDX_INDEX_ASC          (0x00000003)
# define QMO_STAT_CARD_IDX_INDEX_DESC         (0x00000004)

// To Fix PR-11412
# define QMO_STAT_INDEX_HAS_ACCESS_HINT( aFlag )                                \
    (((aFlag & QMO_STAT_CARD_IDX_HINT_MASK) == QMO_STAT_CARD_IDX_INDEX)     ||  \
     ((aFlag & QMO_STAT_CARD_IDX_HINT_MASK) == QMO_STAT_CARD_IDX_INDEX_ASC) ||  \
     ((aFlag & QMO_STAT_CARD_IDX_HINT_MASK) == QMO_STAT_CARD_IDX_INDEX_DESC))

/* qmoIdxCardInfo.flag                                 */
// �ش� index�� �����ϴ� predicate�� ���� ����
# define QMO_STAT_CARD_IDX_EXIST_PRED_MASK    (0x00000010)
# define QMO_STAT_CARD_IDX_EXIST_PRED_FALSE   (0x00000000)
# define QMO_STAT_CARD_IDX_EXIST_PRED_TRUE    (0x00000010)

// To Fix PR-9181
/* qmoIdxCardInfo.flag                                 */
// IN SUBQUERY KEYRANGE�� ���Ǵ� ���� ����
# define QMO_STAT_CARD_IDX_IN_SUBQUERY_MASK   (0x00000020)
# define QMO_STAT_CARD_IDX_IN_SUBQUERY_FALSE  (0x00000000)
# define QMO_STAT_CARD_IDX_IN_SUBQUERY_TRUE   (0x00000020)


/* qmoIdxCardInfo.flag                                 */
// PRIMARY KEY�� ����
# define QMO_STAT_CARD_IDX_PRIMARY_MASK       (0x00000040)
# define QMO_STAT_CARD_IDX_PRIMARY_FALSE      (0x00000000)
# define QMO_STAT_CARD_IDX_PRIMARY_TRUE       (0x00000040)

/* qmoIdxCardInfo.flag                                 */
// indexable ������Ŷ���θ� index key column�� ���� ����
# define QMO_STAT_CARD_ALL_EQUAL_IDX_MASK     (0x00000080)
# define QMO_STAT_CARD_ALL_EQUAL_IDX_FALSE    (0x00000000)
# define QMO_STAT_CARD_ALL_EQUAL_IDX_TRUE     (0x00000080)

//-------------------------------------------------------
// qmoColCardInfo�� flag ����
//-------------------------------------------------------

/* qmoColCardInfo.flag                                 */

/* qmoColCardInfo.flag                                 */
// column�� MIN, MAX ���� �����Ǿ������� ����
# define QMO_STAT_MINMAX_COLUMN_SET_MASK     (0x00000002)
# define QMO_STAT_MINMAX_COLUMN_SET_FALSE    (0x00000000)
# define QMO_STAT_MINMAX_COLUMN_SET_TRUE     (0x00000002)

/* qmoColCardInfo.flag                                 */
// fix BUG-11214 default cardinality�� �����Ǿ������� ����
# define QMO_STAT_DEFAULT_CARD_COLUMN_SET_MASK   (0x00000004)
# define QMO_STAT_DEFAULT_CARD_COLUMN_SET_FALSE  (0x00000000)
# define QMO_STAT_DEFAULT_CARD_COLUMN_SET_TRUE   (0x00000004)


//-------------------------------------------------------
// ��� ������ �����ϱ� ���� �ڷ� ����
//-------------------------------------------------------

//--------------------------------------
// SYSTEM Statistics
//--------------------------------------

typedef struct qmoSystemStatistics
{
    idBool       isValidStat;
    SDouble      singleIoScanTime;
    SDouble      multiIoScanTime;
    SDouble      mMTCallTime;
    SDouble      mHashTime;
    SDouble      mCompareTime;
    SDouble      mStoreTime;
    SDouble      mIndexNlJoinPenalty;
} qmoSystemStatistics;


//--------------------------------------
// Index Cardinality ����
//--------------------------------------

typedef struct qmoKeyRangeColumn
{
    UInt    mColumnCount;
    UShort* mColumnArray;
} qmoKeyRangeColummn;

typedef struct qmoIdxCardInfo
{
    // index ordering�������� �÷����� diff ������ ����
    // qsort ���� compare�Լ������� ����.
    idBool              isValidStat;

    UInt                indexId;
    UInt                flag;
    qcmIndex          * index;

    SDouble             KeyNDV;
    SDouble             avgSlotCount;
    SDouble             pageCnt;
    SDouble             indexLevel;
    SDouble             clusteringFactor;

    qmoKeyRangeColumn   mKeyRangeColumn;
} qmoIdxCardInfo;

//--------------------------------------
// Column Cardinality ����
//--------------------------------------

typedef struct qmoColCardInfo
{
    //---------------------------------------------------
    // flag : cardinality�� MIN,MAX������ ����Ǿ������� ����
    //---------------------------------------------------
    idBool              isValidStat;

    UInt                flag;
    mtcColumn         * column;        // column pointer

    SDouble             columnNDV;     // �ش� column�� cardinality ��
    SDouble             nullValueCount;
    SDouble             avgColumnLen;

    //---------------------------------------------------
    // MIN, MAX ��
    //    - MIN, MAX ���� ���� Data Type�� ��������
    //      ��¥���̴�.
    //    - �� �� ���� ū ũ�⸦ ���� Numeric�� Maximum
    //      ��ŭ ������ �Ҵ��Ͽ�, Data Type�� ����
    //      ������ ���⵵�� ���δ�.
    //---------------------------------------------------
    ULong               minValue[SMI_MAX_MINMAX_VALUE_SIZE/ID_SIZEOF(ULong)];
    ULong               maxValue[SMI_MAX_MINMAX_VALUE_SIZE/ID_SIZEOF(ULong)];
} qmoColCardInfo;

//-------------------------------------------------------
// ��� ����
//-------------------------------------------------------

typedef struct qmoStatistics
{
    qmoOptGoalType      optGoleType;
    idBool              isValidStat;

    SDouble             totalRecordCnt;
    SDouble             pageCnt;
    SDouble             avgRowLen;
    SDouble             readRowTime;

    SDouble             firstRowsFactor;
    UInt                firstRowsN;

    UInt                indexCnt;          // Index�� ����
    qmoIdxCardInfo    * idxCardInfo;       // �� index�� cardinality ����

    UInt                columnCnt;         // Column�� ����
    qmoColCardInfo    * colCardInfo;       // �� column�� cardinality ����
} qmoStatistics;

/***********************************************************************
 * [Access Method�� �����ϱ� ���� �ڷ� ����]
 *     method : Index�� ���� ��� ����
 *            : Full Scan�� ��� NULL ����.
 ***********************************************************************/

typedef struct qmoAccessMethod
{
    qmoIdxCardInfo        * method;          // index�� cardinality ����
    SDouble                 keyRangeSelectivity;
    SDouble                 keyFilterSelectivity;
    SDouble                 filterSelectivity;
    SDouble                 methodSelectivity;
    SDouble                 keyRangeCost;
    SDouble                 keyFilterCost;
    SDouble                 filterCost;
    SDouble                 accessCost;
    SDouble                 diskCost;
    SDouble                 totalCost;
} qmoAccessMethod;

//---------------------------------------------------
// ��� ������ �����ϱ� ���� �Լ�
//---------------------------------------------------

class qmoStat
{
public:

    //--------------------------------------------
    // View�� ���� ��� ���� ������ ���� Interface
    //--------------------------------------------

    static IDE_RC    getStatInfo4View( qcStatement     * aStatement,
                                       qmgGraph        * aGraph,
                                       qmoStatistics  ** aStatInfo);

    //--------------------------------------------
    // �Ϲ� ���̺� ���� ��� ������ �����Ѵ�.
    // qmsSFWGH->from�� �޷��ִ� ��� Base Table�� ���� ��������� �����Ѵ�.
    // Validation �����߿� ȣ���.
    //--------------------------------------------
    static IDE_RC  getStatInfo4AllBaseTables( qcStatement    * aStatement,
                                              qmsSFWGH       * aSFWGH );

    // ��� ������ �����.
    static IDE_RC  printStat( qmsFrom       * aFrom,
                              ULong           aDepth,
                              iduVarString  * aString );

    // PROJ-1502 PARTITIONED DISK TABLE
    // partition�� ���� ��� ������ �����.
    static IDE_RC  printStat4Partition( qmsTableRef     * aTableRef,
                                        qmsPartitionRef * aPartitionRef,
                                        SChar           * aPartitionName,
                                        ULong             aDepth,
                                        iduVarString    * aString );

    // qmoAccessMethod�� ���� index�� ��� ���ؼ�
    // �׻� method�� NULL���� �˻縦 �ؾ� �ϴµ�,
    // �� ������ ���̱� ���ؼ� �Ѵܰ� �߻�ȭ�Ѵ�.
    inline static qcmIndex* getMethodIndex( qmoAccessMethod * aMethod )
    {
        IDE_DASSERT( aMethod != NULL );

        if( aMethod->method == NULL )
        {
            return NULL;
        }
        else
        {
            return aMethod->method->index;
        }
    }

    // PROJ-1502 PARTITIONED DISK TABLE
    // private->public
    // �ϳ��� Base Table�� ���� ��������� �����Ѵ�.
    static IDE_RC    getStatInfo4BaseTable( qcStatement    * aStatement,
                                            qmoOptGoalType   aOptimizerMode,
                                            qcmTableInfo   * aTableInfo,
                                            qmoStatistics ** aStatInfo );

    /* PROJ-1832 New database link */
    static IDE_RC getStatInfo4RemoteTable( qcStatement      * aStatement,
                                           qcmTableInfo     * aTableInfo,
                                           qmoStatistics   ** aStatInfo );

    // PROJ-1502 PARTITIONED DISK TABLE
    // tableRef�� �޷��ִ� partitionRef�� ��������� �̿��Ͽ�
    // partitioned table�� ���� ��������� �����Ѵ�.
    // aStatInfo�� �̹� �Ҵ�Ǿ� �����Ƿ� pointer���� �ѱ��.
    static IDE_RC    getStatInfo4PartitionedTable(
        qcStatement    * aStatement,
        qmoOptGoalType   aOptimizerMode,
        qmsTableRef    * aTableRef,
        qmoStatistics  * aStatInfo );

    static IDE_RC    getSystemStatistics( qmoSystemStatistics * aStatistics );
    static void      getSystemStatistics4Rule( qmoSystemStatistics * aStatistics );

private:

    // ���� ���̺��� left, right ���̺��� ��ȸ�ϸ鼭,
    // base table�� ã�Ƽ� ��������� �����Ѵ�.
    static IDE_RC    findBaseTableNGetStatInfo( qcStatement * aStatement,
                                                qmsSFWGH    * aSFWGH,
                                                qmsFrom     * aFrom );

    static IDE_RC    getTableStatistics( qcStatement   * aStatement,
                                         qcmTableInfo  * aTableInfo,
                                         qmoStatistics * aStatInfo,
                                         smiTableStat  * aData );

    static void      getTableStatistics4Rule( qmoStatistics * aStatistics,
                                              qcmTableInfo  * aTableInfo,
                                              idBool          aIsDiskTable );

    // ���̺��� �����ϰ� �ִ� �� disk page ���� ����.
    static IDE_RC    setTablePageCount( qcStatement    * aStatement,
                                        qcmTableInfo   * aTableInfo,
                                        qmoStatistics  * aStatInfo,
                                        smiTableStat   * aData );

    // �ε��� �������
    static IDE_RC    getIndexStatistics( qcStatement   * aStatement,
                                         qmoStatistics * aStatistics,
                                         idBool          aIsAutoDBMStat,
                                         smiIndexStat  * aData );

    static void      getIndexStatistics4Rule( qmoStatistics * aStatistics,
                                              idBool          aIsDiskTable );

    static IDE_RC    setIndexPageCnt( qcStatement    * aStatement,
                                      qcmIndex       * aIndex,
                                      qmoIdxCardInfo * aIdxInfo,
                                      smiIndexStat   * aData );

    // �� column�� cardinality�� MIX,MAX ���� ���ؼ�,
    // �ش� ������� �ڷᱸ���� �����Ѵ�.
    static IDE_RC    getColumnStatistics( qmoStatistics * aStatistics,
                                          smiColumnStat * aData );

    static void      getColumnStatistics4Rule( qmoStatistics * aStatistics );

    static IDE_RC    setColumnNDV( qcmTableInfo   * aTableInfo,
                                   qmoColCardInfo * aColInfo );

    static IDE_RC    setColumnNullCount( qcmTableInfo   * aTableInfo,
                                         qmoColCardInfo * aColInfo );

    static IDE_RC    setColumnAvgLen( qcmTableInfo   * aTableInfo,
                                      qmoColCardInfo * aColInfo );

    // ��������� idxCardInfo�� �����Ѵ�.
    static IDE_RC    sortIndexInfo( qmoStatistics   * aStatInfo,
                                    idBool            aIsAscending,
                                    qmoIdxCardInfo ** aSortedIdxInfoArry );

    // PROJ-2492 Dynamic sample selection
    static IDE_RC    calculateSamplePercentage( qcmTableInfo   * aTableInfo,
                                                UInt             aAutoStatsLevel,
                                                SFloat         * aPercentage );

    //-----------------------------------------------------
    // TPC-H Data ���� ������ ��� ���� ������ ���� Interface
    //-----------------------------------------------------

    // TPC-H �� ���� ������ ��� ���� ����
    static IDE_RC    getFakeStatInfo( qcStatement    * aStatement,
                                      qcmTableInfo   * aTableInfo,
                                      qmoStatistics  * aStatInfo );

    // REGION ���̺��� ���� ���� ��� ���� ����
    static IDE_RC    getFakeStat4Region( idBool          aIsDisk,
                                         qmoStatistics * aStatInfo );

    // NATION ���̺��� ���� ���� ��� ���� ����
    static IDE_RC    getFakeStat4Nation( idBool          aIsDisk,
                                         qmoStatistics * aStatInfo );

    // SUPPLIER ���̺��� ���� ���� ��� ���� ����
    static IDE_RC    getFakeStat4Supplier( idBool          aIsDisk,
                                           qmoStatistics * aStatInfo );

    // CUSTOMER ���̺��� ���� ���� ��� ���� ����
    static IDE_RC    getFakeStat4Customer( idBool          aIsDisk,
                                           qmoStatistics * aStatInfo );

    // PART ���̺��� ���� ���� ��� ���� ����
    static IDE_RC    getFakeStat4Part( idBool          aIsDisk,
                                       qmoStatistics * aStatInfo );

    // PARTSUPP ���̺��� ���� ���� ��� ���� ����
    static IDE_RC    getFakeStat4PartSupp( idBool          aIsDisk,
                                           qmoStatistics * aStatInfo );

    // ORDERS ���̺��� ���� ���� ��� ���� ����
    static IDE_RC    getFakeStat4Orders( qcStatement   * aStatement,
                                         idBool          aIsDisk,
                                         qmoStatistics * aStatInfo );

    // LINEITEM ���̺��� ���� ���� ��� ���� ����
    static IDE_RC    getFakeStat4LineItem( qcStatement   * aStatement,
                                           idBool          aIsDisk,
                                           qmoStatistics * aStatInfo );

};

#endif /* _O_QMO_STAT_MGR_H_ */
