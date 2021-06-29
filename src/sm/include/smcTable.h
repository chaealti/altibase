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
 * $Id: smcTable.h 90259 2021-03-19 01:22:22Z emlee $
 *
 * Description :
 *
 * �� ������ table�� ���� ��������Դϴ�.
 *
 * # ����
 *
 * memory/disk(temporary ����) table�� �����Ѵ�.
 * ����, table�� �����ϴ� index ����� ���� ������ ����
 * smcIndex�� �и���Ų��.
 *
 * # ����
 *
 * - smc layer ���ٰ��
 *
 *                       smi
 *         _______________|_________________
 *        /               |                 \
 *   ___________          |                  ___________ __________
 *   |smcRecord|  _________________       | |sdcRecord|-|sdcKeyMap|
 *                |smcTable       |       |
 *                |_______________|       | sdcTss.. sdcUndo..
 *                ____/\__________________|
 *               /                        \         [sdc]
 *      _______   ____                    |\ _______
 *      |fixed|  |var|                    |  |entry|
 *                                        |
 *            [smp]                       |     [sdp]
 *
 *
 *  1. ���̺� ���� �� ����
 *
 *     memory Ÿ���̵� disk Ÿ���� ���̺� ����� ��� MRDB�� catalog
 *     table���� ������ �ǹǷ� smcTable�� creatTable/dropTable �Լ���
 *     �����Ͽ� ����Ѵ�
 *     disk table�� page list entry������ tablespace�� segment�� �ϳ�
 *     �����Ͽ� �ʿ��� page�� �Ҵ�޾� tuple�� �����Ѵ�.disk table��
 *     memory table�� �ٸ��� fixed column ����Ÿ�� variable column ����
 *     Ÿ�� �����Ͽ� �������� �ʱ⶧���� page list entry�� �ϳ��� �����Ѵ�.
 *
 *     !!] pending ���� : disk G.C�� ���ؼ��� ó���ǵ��� ��
 *     table ���Ŵ� pending operation���� ó���Ǹ�, memory table�� ����
 *     ager�� ���� disk table�� disk G.C�� ���� smcTable::dropTablePending()��
 *     ȣ���Ͽ� ó���ȴ�. restart recovery�Ŀ� refine ����������� catalog table
 *     �� �˻��ϸ鼭 disk table Ÿ���� table header�� �ǵ��� ��쿡 ���Ͽ�
 *     refine�� ������� ������, ���Ŀ� disk G.C�� start�Ǹ� ���� �ϵ��� �ñ��.
 *
 *  2. disk temporary ���̺�
 *
 *     - temproary table for meta table
 *       createdb ��������߿� sys_* ���� catalog table�� �����ϴ� ��������
 *       smcTable::createTable()�Լ��� ȣ���Ͽ� �����Ѵ�.
 *
 *     !!] meta table�� ���Ͽ� page list entry���� page �Ҵ翡 ���ؼ���
 *         ��� logging�� �ϸ�, temporary table�� system ����ÿ� �Ҵ�Ǿ���
 *         ��� fixed page list entry�� page��� var page list entry�� page����
 *         ��� truncate ��Ų��.
 *
 *     - temporary table
 *
 *     temporary table�� ���� ���� �� ���Ŵ� �Ϲ����� table
 *     �� ó�������� �ٸ��Ƿ� createTempTable/dropTempTable�� ����Ͽ�
 *     no logging, no lock���� ó���Ѵ�.
 *     ��, slot �Ҵ�ÿ� page �Ҵ���� logging�� �����Ͽ��� �Ѵ�.
 *
 *                                                meta table�� fixed slot��
 *                                                �Ҵ��Ͽ� smcTableHeader��
 *                                                �����Ѵ�.
 *                                                        |
 *    _________________________________          _________v_______
 *   | meta table for temporaroy table |         |smcTableHeader |
 *   | : catalog table�κ��� fixed slot|  --->   |               |
 *   |___�� �Ҵ�޾� �����Ѵ�.  _______|         |_______________|
 *            |                 |                         |
 *            |                 ��               _________V_________
 *            o                 |                 | smnIndexHeader  |
 *            |                 �� var            |_________________|
 *            o fixed              page list
 *              page list entry    entry         meta table�� var slot��
 *                                               �Ҵ��Ͽ� smnIndexHeader��
 *                                               �����Ѵ�.
 *
 *     !!] �Ҵ�Ǿ��� temporary tablespace�� ���Ͽ� reset ó���� �Ѵ�.
 *
 *
 *  3. disk table�� ���� alter table add/drop column ...���� DDL�� ���� ó��
 *
 *     �޸� ���̺��� memory ���������� �ؼ��ϱ� ���� �Ʒ��� ���� �����
 *     ����Ͽ� ó���ϴ� ����� ���� ������, disk table�� �Ʒ��� ����
 *     ����� �״�� ����ص� ������ ����.
 *
 *     ���ο� table�� ������ ������ ���� table�� ���ο� table�� ���Ͽ�
 *     ���� select cursor�� insert cursor�� open�Ͽ� rows�� ���ο� ���̺�
 *     insert���� old table�� ���Ͽ� drop table�� �����Ѵ�.
 *     -> QP�ܿ��� ó�� => moveRows() .. ���
 *
 *
 *
 *  4.�ε��� ���� �� ����
 *
 *     �ε��� ������ ��� ������ ��κ� �����ϸ�,
 *     ��, index header�� �ʱ�ȭ�ϴ� �ڵ带 �ش� index manager�� ȣ���Ͽ�
 *     �ʱ�ȭ�ϵ��� �Ѵ�.
 *
 *     !!] pending ���� : disk G.C�� ���ؼ��� ó���ǵ��� ��
 *     index ���Ŵ� pending operation���� ó���Ǹ�, memory index�� ����
 *     transaction commit �������� ó���Ǹ� crash�ÿ��� runtime ������ index��
 *     �ڵ����� �Ҹ�ǹǷ� ������� �ʾƵ� �ȴ�. �ݸ鿡 disk index�� ����
 *     transaction commit ������ ó������ �ʰ� disk G.C�� ���ظ� ó����
 *     ���� �ϸ�, crash�ÿ��� persistent�ϱ� ������ disk G.C�� ���� ó���ɼ�
 *     �ֵ��� �Ѵ�. (���� �����۾��� �Ҷ��� smcTable::dropIndexPending() ȣ��)
 *
 *
 *  5. DDL�� ���� recovery
 *
 *  !!]���� ���� DDL ���࿡ �־ memory tablespace�� ���� �α��
 *     disk tablespace ������ ���� �α��� ȥ�յǴ� ��쿡 �־
 *     ��Ȯ�� transaction rollback�̳� recovery�� ����Ǿ�� �Ѵ�.
 *
 *  : ���� ���, disk table ���������� �뷫���� �α��� ������ ����.
 *
 *    1) catalog table���� fixed slot �Ҵ翡 ���� �α�
 *    2) disk data page�� ���� segment �Ҵ翡 ����
 *       operation NTA disk d-�α� (SMR_LT_DRDB_OP_NTA)
 *       segment �Ҵ��� �̷����
 *    3) SMR_SMC_TABLEHEADER_INIT ..
 *    4) table header �Ҵ� �� �ʱ�ȭ�� ����
 *       SMR_OP_CREATE_TABLE NTA m-�α�
 *    5) column, index, ...�� ���� ó��
 *       ...
 *
 *    �α��� �Ŀ� segment�Ҵ翡 ���� operation NTA disk �α׸� �α��� �ϰ�,
 *    crash�� �߻��Ͽ���.
 *    �׷��ٸ�, restart recovery�ÿ� �ش� DDL�� ������
 *    uncommited transaction�� ������ �α׸� �ֱ� �������� �ǵ��Ұ��̸�,
 *
 *    - SMR_OP_CREATE_TABLE�� �ǵ��ϸ�, logical undo�ν� dropTable��
 *      ó���ϸ�, nta ������ �ǵ����� �ʰ� �Ѿ��.
 *    - ����, segment �Ҵ翡 ���� operation NTA disk �α׸� �ǵ��ϰ� �Ǹ�,
 *      �ش� �α������ ����� sdRID�� ���� segment ������ �����ϸ� �ȴ�.
 *
 * 6. refine catalog table
 *
 *    disk G.C�� drop table�� ���� pending ������ ó���ϴ� �����
 *    ������ ����.
 *
 *    tss commit list���� drop table�� ���� pending ������ ó����
 *    smcTable::dropTablePending �Լ��� ���ؼ� �����
 *    1) �� �������� free�� segment rid�� ������ sdpPageListEntry��
 *       mDataSegDescRID�� SD_NULL_RID�� �����Ѵ�.(�α�)
 *    2) free data segment �Ѵ�. (1���� 2�������� NTA�����̴�)
 *    3) ���� 1�������� ó���� �� �׾��ٸ�, undoAll�������� ����������
 *       ������ �����Ұ��̰�, 2������ ó���ߴٸ� free segment�� undo
 *       ���� ���� ���̴�.
 *    4) �̷��� ��Ȳ�̶�� ������ ���� ����� ��´�.
 *       : refine DB�ÿ� disk table header�� entry->mDataSegDescRID��
 *         SD_NULL_RID��� --> free table header slot�� �����Ѵ�.
 *       : �׷��� �ʴٸ� --> dropTablePending�� ó���ϰ�,
 *         ���� refineDB�ÿ� free table header slot�� �����Ѵ�.
 *
 **********************************************************************/

#ifndef _O_SMC_TABLE_H_
#define _O_SMC_TABLE_H_ 1

#include <smDef.h>
#include <sdb.h>
#include <smu.h>
#include <smr.h>
#include <smm.h>
#include <smcDef.h>
#include <smcCatalogTable.h>
#include <smnDef.h>

class smcTable
{
public:
    static IDE_RC initialize();

    static IDE_RC destroy();

    // Table�� Lock Item�� �ʱ�ȭ�ϰ� Runtime Item���� NULL�� �����Ѵ�.
    static IDE_RC initLockAndSetRuntimeNull( smcTableHeader* aHeader );

    // Table Header�� Runtime Item���� NULL�� �ʱ�ȭ�Ѵ�.
    static IDE_RC setRuntimeNull( smcTableHeader* aHeader );

    /* FOR A4 : runtime������ �ʱ�ȭ */
    static IDE_RC initLockAndRuntimeItem( smcTableHeader * aHeader );

    /* table header�� lock item�� mSync�ʱ�ȭ */
    static IDE_RC initLockItem( smcTableHeader * aHeader );

    /* table header�� runtime item �ʱ�ȭ */
    static IDE_RC initRuntimeItem( smcTableHeader * aHeader );

    /* FOR A4 : table header�� lock item�� runtime������ ���� */
    static IDE_RC finLockAndRuntimeItem( smcTableHeader * aHeader );

    /* table header�� lock item ���� */
    static IDE_RC finLockItem( smcTableHeader * aHeader );


    /* table header�� runtime item ���� */
    static IDE_RC finRuntimeItem( smcTableHeader * aHeader );

    // table
    static IDE_RC initTableHeader( void                * aTrans,
                                   scSpaceID             aSpaceID,
                                   vULong                aMax,
                                   UInt                  aColumnSize,
                                   UInt                  aColumnCount,
                                   smOID                 aFixOID,
                                   UInt                  aFlag,
                                   smcSequenceInfo*      aSequence,
                                   smcTableType          aType,
                                   smiObjectType         aObjectType,
                                   ULong                 aMaxRow,
                                   smiSegAttr            aSegAttr,
                                   smiSegStorageAttr     aSegStorageAttr,
                                   UInt                  aParallelDegree,
                                   smcTableHeader     *  aHeader );

    /* Column List������ ���� Column������ Fixed Row�� ũ�⸦ ���Ѵ�.*/
    static IDE_RC validateAndGetColCntAndFixedRowSize(
                                   const UInt           aTableType,
                                   const smiColumnList *aColumnList,
                                   UInt                *aColumnCnt,
                                   UInt                *aFixedRowSize);

    /* memory/disk Ÿ�� table�� �����Ѵ�. */
    static IDE_RC createTable( idvSQL                *aStatistics,
                               void*                  aTrans,
                               scSpaceID              aSpaceID,
                               const smiColumnList*   aColumnList,
                               UInt                   aColumnSize,
                               const void*            aInfo,
                               UInt                   aInfoSize,
                               const smiValue*        aNullRow,
                               UInt                   aFlag,
                               ULong                  aMaxRow,
                               smiSegAttr             aSegAttr,
                               smiSegStorageAttr      aSegStorageAttr,
                               UInt                   aParallelDegree,
                               const void**           aHeader );

    /* Table Header�� Info�� �����Ѵ�. */
    static IDE_RC setInfoAtTableHeader( void*            aTrans,
                                        smcTableHeader*  aTable,
                                        const void*      aInfo,
                                        UInt             aInfoSize );

    /* Table�� Header�� ���ο� Column ����Ʈ�� �����Ѵ�. */
    static IDE_RC setColumnAtTableHeader( void*                aTrans,
                                          smcTableHeader*      aTableHeader,
                                          const smiColumnList* aColumnsList,
                                          UInt                 aColumnCnt,
                                          UInt                 aColumnSize );

    /* FOR A4 : memory/disk Ÿ�� table�� �����Ѵ�. */
    static IDE_RC dropTable  ( void                 * aTrans,
                               const void           * aHeader,
                               sctTBSLockValidOpt     aTBSLvOpt,
                               UInt                   aFlag = (SMC_LOCK_TABLE));

    static IDE_RC alterIndexInfo(void    *       aTrans,
                                 smcTableHeader* aHeader,
                                 void          * aIndex,
                                 UInt            aFlag);


    /* PROJ-2162 RestartRiskReduction
     * InconsistentFlag�� �����Ѵ�. */
    static IDE_RC setIndexInconsistency( smOID            aTableOID,
                                         smOID            aIndexID );

    static IDE_RC setAllIndexInconsistency( smcTableHeader  * aTableHeader );
                                        

    // PROJ-1671
    static IDE_RC alterTableSegAttr( void                  * aTrans,
                                     smcTableHeader        * aHeader,
                                     smiSegAttr              aSegAttr,
                                     sctTBSLockValidOpt      aTBSLvOpt );

    static IDE_RC alterTableSegStoAttr( void               * aTrans,
                                        smcTableHeader     * aHeader,
                                        smiSegStorageAttr    aSegStoAttr,
                                        sctTBSLockValidOpt   aTBSLvOpt );

    static IDE_RC alterTableAllocExts( idvSQL              * aStatistics,
                                       void                * aTrans,
                                       smcTableHeader      * aHeader,
                                       ULong                 aExtendSize,
                                       sctTBSLockValidOpt    aTBSLvOpt );

    static IDE_RC alterIndexSegAttr( void                * aTrans,
                                     smcTableHeader      * aHeader,
                                     void                * aIndex,
                                     smiSegAttr            aSegAttr,
                                     sctTBSLockValidOpt    aTBSLvOpt );
    
    static IDE_RC alterIndexSegStoAttr( void                * aTrans,
                                        smcTableHeader      * aHeader,
                                        void                * aIndex,
                                        smiSegStorageAttr     aSegStoAttr,
                                        sctTBSLockValidOpt    aTBSLvOpt );

    static IDE_RC alterIndexAllocExts( idvSQL              * aStatistics,
                                       void                * aTrans,
                                       smcTableHeader      * aHeader,
                                       void                * aIndex,
                                       ULong                 aExtendSize,
                                       sctTBSLockValidOpt    aTBSLvOpt );

    // fix BUG-22395
    static IDE_RC alterIndexName ( idvSQL              * aStatistics,
                                   void                * aTrans,
                                   smcTableHeader      * aHeader,
                                   void                * aIndex,
                                   SChar               * aName );

    static IDE_RC setUseFlagOfAllIndex(void            * aTrans,
                                       smcTableHeader  * aHeader,
                                       UInt              aFlag);
    // table�� Lockȹ��
    static IDE_RC lockTable( void*               aTrans,
                             smcTableHeader*     aHeader,
                             sctTBSLockValidOpt  aTBSLvOpt,
                             UInt                aFlag );

    static IDE_RC compact( void           * aTrans,
                           smcTableHeader * aTable,
                           UInt             aPages );
    
    static IDE_RC validateTable( void              * aTrans,
                                 smcTableHeader    * aHeader,
                                 sctTBSLockValidOpt  aTBSLvOpt,
                                 UInt                aFlag = (SMC_LOCK_TABLE));

    static IDE_RC modifyTableInfo( void                 * aTrans,
                                   smcTableHeader       * aHeader,
                                   const smiColumnList  * aColumns,
                                   UInt                   aColumnSize,
                                   const void           * aInfo,
                                   UInt                   aInfoSize,
                                   UInt                   aFlag,
                                   ULong                  aMaxRow,
                                   UInt                   aParallelDegree,
                                   sctTBSLockValidOpt     aTBSLvOpt,
                                   idBool                 aIsInitRowTemplate );

    /* FOR A4 : disk/memory index�� �����Ѵ�. */
    static IDE_RC createIndex( idvSQL                 *aStatistics,
                               void                  * aTrans,
                               scSpaceID               aSpaceID,
                               smcTableHeader        * aHeader,
                               SChar                 * aName,
                               UInt                    aID,
                               UInt                    aType,
                               const smiColumnList   * aColumns,
                               UInt                    aFlag,
                               UInt                    aBuildFlag,
                               smiSegAttr            * aSegAttr,
                               smiSegStorageAttr     * aSegStorageAttr,
                               ULong                   aDirectKeyMaxSize,
                               void                 ** aIndex );

    /* FOR A4 : disk/memory index�� �����Ѵ�. */
    static IDE_RC dropIndex( void              * aTrans,
                             smcTableHeader    * aHeader,
                             void              * aIndex,
                             sctTBSLockValidOpt  aTBSLvOpt );

    static IDE_RC dropIndexByAbortHeader( idvSQL         * aStatistics,
                                          void           * aTrans,
                                          smcTableHeader * aHeader,
                                          const  UInt      aIdx,
                                          void           * aIndexHeader,
                                          smOID            aOIDIndex );

    static IDE_RC dropIndexPending( void* aIndexHeader );

    static IDE_RC dropIndexList(smcTableHeader * aHeader);

    static IDE_RC dropTablePending( idvSQL           *aStatistics,
                                    void*             aTrans,
                                    smcTableHeader*   aHeader,
                                    idBool            aFlag = ID_TRUE );

    // for alter table PR-4360
    static IDE_RC dropTablePageListPending( void           * aTrans,
                                            smcTableHeader * aHeader,
                                            idBool           aUndo );

    inline static const smiColumn * getColumn( const void  * aHeader,
                                               const UInt    aIndex );

    /* SegmentGRID�� ������ Lob �÷��� ��ȯ�Ѵ�. */
    static const smiColumn* findLOBColumn( void  * aHeader,
                                           scGRID  aSegGRID );

    static UInt getColumnIndex( UInt aColumnID );
    static UInt getPrimaryKeySize(smcTableHeader*    aHeader,
                                  SChar*             aFixRow);

    static UInt getColumnCount(const void *aTableHeader);
    static UInt getLobColumnCount(const void *aTableHeader);

    static const smiColumn* getLobColumn( const void *aTableHeader,
                                          scSpaceID   aSpaceID,
                                          scPageID    aSegPID );

    static IDE_RC createLOBSegmentDesc( smcTableHeader * sHeader );
    static IDE_RC destroyLOBSegmentDesc( smcTableHeader * sHeader );

    static UInt getColumnSize(void *aTableHeader);

    static void* getDiskPageListEntry( void *aTableHeader );
    static void* getDiskPageListEntry( smOID aTableOID );

    static ULong getMaxRows(void *aTableHeader);
    static smOID getTableOID(const void *aTableHeader);

    /* BUG-32479 [sm-mem-resource] refactoring for handling exceptional case
     * about the SMM_OID_PTR and SMM_PID_PTR macro .
     * SMC_TABLE_HEADER�� ����ó���ϴ� �Լ��� ��ȯ��. */
    inline static IDE_RC getTableHeaderFromOID( smOID    aOID, 
                                                void  ** aHeader );

    static IDE_RC getIndexHeaderFromOID( smOID    aOID, 
                                         void  ** aHeader );

    // disk table�� ���Ͽ� ȣ��� (NTA undo��)
    static IDE_RC doNTADropDiskTable( idvSQL *aStatistics,
                                      void*   aTrans,
                                      SChar*  aRow );

    static IDE_RC getRecordCount(smcTableHeader * aHeader,
                                 ULong          * aRecordCount);

    static IDE_RC setRecordCount(smcTableHeader * aHeader,
                                 ULong            aRecordCount);

    static inline idBool isDropedTable( smcTableHeader * aHeader );

    /* BUG-47367 Delete Thread ���� Drop Check �Լ� */
    static inline idBool isDropedTable4Ager( smcTableHeader * aHeader, smSCN aSCN );


    //PROJ-1362 LOB���� index���� ����Ǯ�� part.
    inline static UInt getIndexCount( void * aHeader );

    inline static const void * getTableIndex( void     * aHeader,
                                              const UInt aIdx );
    static const void * getTableIndexByID( void         * aHeader,
                                           const UInt     aIdx );

    static IDE_RC waitForFileDelete( idvSQL *aStatistics, SChar* aStrFileName);

    inline static SChar* getBackupDir();

    static IDE_RC deleteAllTableBackup();

    static IDE_RC  copyAllTableBackup( SChar      * aSrcDir,
                                       SChar      * aTargetDir );


    static void  getTableHeaderFlag(void*,
                                    scPageID*,
                                    scOffset*,
                                    UInt*);

    static UInt getTableHeaderFlagOnly( void* );

    /* PROJ-2162 RestartRiskReduction */
    static void  getTableIsConsistent( void     * aTable,
                                       scPageID * aPageID,
                                       scOffset * aOffset,
                                       idBool   * aFlag );


    static IDE_RC  dropIndexByAbortOID( idvSQL *aStatistics,
                                        void *  aTrans,
                                        smOID   aOldIndexOID,
                                        smOID   aNewIndexOID );

    static IDE_RC  clearIndexList(smOID aTblHeaderOID);

    //BUG-15627 Alter Add Column�� Abort�� ��� Table�� SCN�� �����ؾ� ��.
    //logical undo, ����������Ž� �Ҹ��� �Լ�
    static void    changeTableSCNForRebuild( smOID aTableOID );

    // BUG-25611 
    // memory table�� ���� LogicalUndo��, ���� Refine���� ���� �����̱� ������
    // �ӽ÷� ���̺��� Refine���ش�.
    static IDE_RC prepare4LogicalUndo( smcTableHeader * aTable );

    // BUG-25611 
    // memory table�� ���� LogicalUndo��, �ӽ÷� Refine�� ���� �ٽ�
    // �������ش�.
    static IDE_RC finalizeTable4LogicalUndo(smcTableHeader * aTable,
                                            void           * aTrans );

    static void  getTableHeaderInfo( void*,
                                     scPageID*,
                                     scOffset*,
                                     smVCDesc*,
                                     smVCDesc*,
                                     UInt*,
                                     UInt*,
                                     ULong*,
                                     UInt*);


    static IDE_RC getTablePageCount( void *aHeader, ULong *aPageCnt );

    static inline scSpaceID getTableSpaceID( void * aHeader );

    static void   addToTotalIndexCount( UInt aCount );

    static IDE_RC freeColumns(idvSQL          *aStatistics,
                              void* aTrans,
                              smcTableHeader* aTableHeader);

    // PROJ-1362 QP Large Record & Internal LOB ����.
    // �ִ� �÷������� �����Ѵٰ� �����Ҷ� ���Ǵ�
    // vairable slot������ ���Ѵ�.
    static inline UInt  getMaxColumnVarSlotCnt(const UInt aColumnSize);
    static inline UInt  getColumnVarSlotCnt(smcTableHeader* aTableHeader);
    static void  buildOIDStack(smcTableHeader* aTableHeader,
                               smcOIDStack*    aOIDStack);

    static void adjustIndexSelfOID( UChar * aIndexHeaders,
                                    smOID   aStartIndexOID,
                                    UInt    aTotalIndexHeaderSize );
    
    static IDE_RC findIndexVarSlot2Insert(smcTableHeader *aHeader,
                                          UInt           aFlag,
                                          const UInt     aIndexHeaderSize,
                                          UInt           *aTargetIndexVarSlotIdx);
    
    // BUF-23218 : DROP INDEX �� ALTER INDEX���� �� �� �ֵ��� �̸� ����
    static IDE_RC findIndexVarSlot2AlterAndDrop(smcTableHeader *aHeader,
                                                void*                aIndex,
                                                const UInt           aIndexHeaderSize,
                                                UInt*                aTargetIndexVarSlotIdx,
                                                SInt*                aOffset );

    static IDE_RC freeIndexes( idvSQL          * aStatistics,
                               void            * aTrans,
                               smcTableHeader  * aTableHeader,
                               UInt              aTableType );

    /* FOR A4 :
       fixed page list entry�� variable page list entries �ʱ�ȭ */
    static void   initMRDBTablePageList( vULong            aMax,
                                         smcTableHeader*   aHeader );

    /* volatile table�� page list entry �ʱ�ȭ */
    static void   initVRDBTablePageList( vULong            aMax,
                                         smcTableHeader*   aHeader );

    /* disk page list entry�� �ʱ�ȭ �� segment �Ҵ� */
    static void   initDRDBTablePageList( vULong            aMax,
                                         smcTableHeader*   aHeader,
                                         smiSegAttr        aSegAttr,
                                         smiSegStorageAttr aSegStorageAttr );

    /* online disk tbs/restart recovery�ÿ� disk table�� runtime index header rebuild */
    static IDE_RC rebuildRuntimeIndexHeaders(
                                         idvSQL          * aStatistics,
                                         smcTableHeader  * aHeader,
                                         ULong             aMaxSmoNo );
    /* For Pararrel Loading */
    static IDE_RC loadParallel( SInt   aDbPingPong,
                                idBool aLoadMetaHeader );

    static inline idBool isReplicationTable(const smcTableHeader* aHeader);
    /* BUG-39143 */
    static inline idBool isTransWaitReplicationTable( const smcTableHeader* aHeader );

    static inline idBool isSupplementalTable(const smcTableHeader* aHeader);

    static idBool needReplicate(const smcTableHeader* aTableHeader,
                                void*  aTrans);

    // BUG-17371  [MMDB] Aging�� �и���� System�� ������
    //           �� Aging�� �и��� ������ �� ��ȭ��.
    static void incStatAtABort( smcTableHeader * aHeader,
                                smcTblStatType   aTblStatType );

    static IDE_RC setNullRow(void*             aTrans,
                             smcTableHeader*   aTable,
                             UInt              aTableType,
                             void*             aData);

    static IDE_RC makeLobColumnList(
                                void             * aTableHeader,
                                smiColumn     ***  aArrLobColumn,
                                UInt             * aLobColumnCnt );

    static IDE_RC destLobColumnList( void *aColumnList) ;

    /* PROJ-1594 Volatile TBS */
    /* initAllVolatileTables()���� ���Ǵ� �Լ���,
       callback �Լ��� �̿��� null row�� ���´�. */
    static IDE_RC makeNullRowByCallback(smcTableHeader *aTableHeader,
                                        smiColumnList  *aColList,
                                        smiValue       *aNullRow,
                                        SChar          *aValueBuffer);

    static scSpaceID getSpaceID(const void *aTableHeader);
    static scPageID  getSegPID( void *aTable );

    ///////////////////////////////////////////////////////////////////////
    // BUG-17371  [MMDB] Aging�� �и���� System�� ������
    //           �� Aging�� �и��� ������ �� ��ȭ��.
    //
    // => �ذ� : AGER�� ���ķ� ����
    // AGER Thread�� 2�� �̻��� �Ǹ� iduSync�� ���ü� ���� �Ұ���.
    // TableHeader�� iduSync��� iduLatch�� �̿��ϵ��� �ڵ带 ������

    // Table Header�� Index Latch�� �Ҵ��ϰ� �ʱ�ȭ
    static IDE_RC allocAndInitIndexLatch(smcTableHeader * aTableHeader );

    // Table Header�� Index Latch�� �ı��ϰ� ����
    static IDE_RC finiAndFreeIndexLatch(smcTableHeader * aTableHeader );

    // Table Header�� Index Latch�� Exclusive Latch�� ��´�.
    static IDE_RC latchExclusive(smcTableHeader * aTableHeader );

    // Table Header�� Index Latch�� Shared Latch�� ��´�.
    static IDE_RC latchShared(smcTableHeader * aTableHeader );

    //Table Header�� Index Latch�� Ǯ���ش�.
    static IDE_RC unlatch(smcTableHeader * aTableHeader );


    /* BUG-30378 ������������ Drop�Ǿ����� refine���� �ʴ� 
     * ���̺��� �����մϴ�.
     * dumpTableHeaderByBuffer�� �̿��ϴ� ���� �Լ� */
    static void dumpTableHeader( smcTableHeader * aTable );

    /* BUG-31206    improve usability of DUMPCI and DUMPDDF 
     * Util������ �� �� �ִ� ���ۿ� �Լ�*/
    static void dumpTableHeaderByBuffer( smcTableHeader * aTable,
                                         idBool           aDumpBinary,
                                         idBool           aDisplayTable,
                                         SChar          * aOutBuf,
                                         UInt             aOutSize );

    // PROJ-1665 : Table�� Parallel Degree�� �Ѱ���
    static UInt getParallelDegree( void * aTableHeader );

    // PROJ-1665
    static idBool isLoggingMode( void * aTableHeader );

    // PROJ-1665
    static idBool isTableConsistent( void * aTableHeader );

    // Table�� Flag�� �����Ѵ�.
    static IDE_RC alterTableFlag( void           * aTrans,
                                  smcTableHeader * aTableHeader,
                                  UInt             aFlagMask,
                                  UInt             aFlagValue );

    // Table�� Insert Limit�� �����Ѵ�.
    static IDE_RC alterTableSegAttr( void           * aTrans,
                                     smcTableHeader * aTableHeader,
                                     smiSegAttr       aSegAttr );

    /* PROJ-2162 RestartRiskReduction
     * InconsistentFlag�� �����Ѵ�. */
    static IDE_RC setTableHeaderInconsistency(smOID            aTableOID );

    static IDE_RC setTableHeaderDropFlag( scSpaceID    aSpaceID,
                                          scPageID     aPID,
                                          scOffset     aOffset,
                                          idBool       aIsDrop );

    // Table Meta Log Record�� ����Ѵ�.
    static IDE_RC writeTableMetaLog(void         * aTrans,
                                    smrTableMeta * aTableMeta,
                                    const void   * aLogBody,
                                    UInt           aLogBodyLen);

    static inline smcTableType getTableType( void* aTable );

    static idBool isSegPIDOfTbl( void* aTable, scSpaceID aSpaceID, scPageID aSegPID );
    static idBool isIdxSegPIDOfTbl( void* aTable, scSpaceID aSpaceID, scPageID aSegPID );
    static idBool isLOBSegPIDOfTbl( void* aTable, scSpaceID aSpaceID, scPageID aSegPID );

    static IDE_RC initRuntimeInfos(
                               idvSQL         * aStatistics,
                               smcTableHeader * aHeader,
                               void           * aActionArg );

    static IDE_RC verifyIndexIntegrity(
                               idvSQL         * aStatistics,
                               smcTableHeader * aHeader,
                               void           * aActionArg );

    static smiColumn* getColumnAndOID(const void* aTableHeader,
                                      const UInt  aIndex,
                                      smOID*      aOID);

    static IDE_RC initRowTemplate( idvSQL         * aStatistics,
                                   smcTableHeader * aHeader,
                                   void           * aActionArg );

    static IDE_RC destroyRowTemplate( smcTableHeader * aHeader );

    static void dumpRowTemplate( smcRowTemplate * aRowTemplate );

    /* PROJ-2462 Result Cache */
    static void inline addModifyCount( smcTableHeader * aTableHeader );

    static idBool isNullSegPID4DiskTable( void * aTableHeader );
        
    static IDE_RC setTableCreateSCN( smcTableHeader * aHeader, smSCN  aSCN );
private:

    // Create Table�� ���� ����ó��
    static IDE_RC checkError4CreateTable( scSpaceID aSpaceID,
                                          UInt      aTableFlag );


    // Table�� Flag���濡 ���� ����ó��
    static IDE_RC checkError4AlterTableFlag( smcTableHeader * aTableHeader,
                                             UInt             aFlagMask,
                                             UInt             aFlagValue );

    // PROJ-1362 QP Large Record & Internal LOB ����.
    // column , index ���� ���� Ǯ��.
    static IDE_RC storeTableColumns(void*   aTrans,
                                    UInt    aColumnSize,
                                    const smiColumnList* aColumns,
                                    UInt                aTableType,
                                    smOID*  aHeadVarOID);

    static IDE_RC  freeLobSegFunc(idvSQL*           aStatistics,
                                  void*             aTrans,
                                  scSpaceID         aColSlotSpaceID,
                                  smOID             aColSlotOID,
                                  UInt              aColumnSize,
                                  sdrMtxLogMode     aLoggingMode);

    static IDE_RC  freeLobSegNullFunc(idvSQL*           aStatistics,
                                      void*             aTrans,
                                      scSpaceID         aColSlotSpaceID,
                                      smOID             aColSlotOID,
                                      UInt              aColumnSize,
                                      sdrMtxLogMode     aLoggingMode);


    // Table�� �Ҵ�� ��� FreePageHeader�� �ʱ�ȭ
    static void   initAllFreePageHeader( smcTableHeader* aTableHeader );

    static IDE_RC removeIdxHdrAndCopyIndexHdrArr(
                                        void*     aTrans,
                                        void*     aDropIndexHeader,
                                        void*     aIndexHeaderArr,
                                        UInt      aIndexHeaderArrLen,
                                        smOID     aOIDIndexHeaderArr );

//For Member
public:
    // mDropIdxPagePool�� �̷� Drop�� IndexHeader�� ������ Memory Pool�̴�.
    static iduMemPool mDropIdxPagePool;

    static SChar      mBackupDir[SM_MAX_FILE_NAME];

    // ���� Table�� ������ Index�� ����
    static UInt       mTotalIndex;

private:
};

SChar* smcTable::getBackupDir()
{
    return mBackupDir;
}

/***********************************************************************
 * Description : aHeader�� ����Ű�� Table�� Replication�� �ɷ������� ID_TRUE,
 *               �ƴϸ� ID_FALSE.
 *
 * aHeader   - [IN] Table Header
 ***********************************************************************/
inline idBool smcTable::isReplicationTable(const smcTableHeader* aHeader)
{
    IDE_DASSERT(aHeader != NULL);

    return  ((aHeader->mFlag & SMI_TABLE_REPLICATION_MASK) == SMI_TABLE_REPLICATION_ENABLE ?
             ID_TRUE : ID_FALSE);
}
inline idBool smcTable::isTransWaitReplicationTable( const smcTableHeader* aHeader )
{
    IDE_DASSERT(aHeader != NULL);

    return  ( (aHeader->mFlag & SMI_TABLE_REPLICATION_TRANS_WAIT_MASK) == 
              SMI_TABLE_REPLICATION_TRANS_WAIT_ENABLE ? ID_TRUE : ID_FALSE );
}
inline idBool smcTable::isSupplementalTable(const smcTableHeader* aHeader)
{
    IDE_DASSERT(aHeader != NULL);

    return ((aHeader->mFlag & SMI_TABLE_SUPPLEMENTAL_LOGGING_MASK) == SMI_TABLE_SUPPLEMENTAL_LOGGING_TRUE ?
             ID_TRUE : ID_FALSE);
}

inline  UInt  smcTable::getMaxColumnVarSlotCnt(const UInt aColumnSize)
{
    UInt sColumnLen = aColumnSize * SMI_COLUMN_ID_MAXIMUM;

    return ( sColumnLen / SMP_VC_PIECE_MAX_SIZE)  + ((sColumnLen % SMP_VC_PIECE_MAX_SIZE) == 0 ? 0:1) ;
}

inline  UInt  smcTable::getColumnVarSlotCnt(smcTableHeader* aTableHeader)
{
    UInt sColumnLen = (aTableHeader->mColumnSize) * (aTableHeader->mColumnCount);

    return ( sColumnLen / SMP_VC_PIECE_MAX_SIZE)  + ((sColumnLen % SMP_VC_PIECE_MAX_SIZE) == 0 ? 0:1) ;
}

inline  UInt  smcTable::getParallelDegree(void * aTableHeader)
{
    IDE_ASSERT( aTableHeader != NULL );

    return ((smcTableHeader*)aTableHeader)->mParallelDegree;
}

inline idBool smcTable::isLoggingMode(void * aTableHeader)
{
    idBool sIsLogging;

    IDE_ASSERT( aTableHeader != NULL );

    if( (((smcTableHeader*)aTableHeader)->mFlag &
         SMI_TABLE_LOGGING_MASK)
        == SMI_TABLE_LOGGING )
    {
        sIsLogging = ID_TRUE;
    }
    else
    {
        sIsLogging = ID_FALSE;
    }

    return sIsLogging;
}

inline idBool smcTable::isTableConsistent(void * aTableHeader)
{
    IDE_ASSERT( aTableHeader != NULL );

    return  ((smcTableHeader*)aTableHeader)->mIsConsistent;
}

inline smcTableType smcTable::getTableType( void* aTable )
{
    smcTableHeader *sTblHdr = (smcTableHeader*)((smpSlotHeader*)aTable + 1);
    return sTblHdr->mType;
}

inline idBool smcTable::isNullSegPID4DiskTable( void * aTableHeader )
{
    idBool isNullSegPID = ID_FALSE;

    if ( sdpSegDescMgr::getSegPID( &(((smcTableHeader*)aTableHeader)->mFixed.mDRDB) ) 
                        == SD_NULL_RID )
    {
        isNullSegPID = ID_TRUE;
    }
    else
    {
        isNullSegPID = ID_FALSE;
    }

    return isNullSegPID;
}

/***********************************************************************
 * Description : get space id of a table
 ***********************************************************************/
inline scSpaceID smcTable::getTableSpaceID( void * aHeader )
{
    IDE_DASSERT( aHeader != NULL );

    return ((smcTableHeader *)aHeader)->mSpaceID;
}

/***********************************************************************
 * Description : table�� drop �Ǿ����� �˻��Ѵ�.
 ***********************************************************************/
inline idBool smcTable::isDropedTable( smcTableHeader * aHeader )
{
    smpSlotHeader * sSlotHeader = NULL;

    sSlotHeader = (smpSlotHeader *)((smpSlotHeader *)aHeader - 1);
    if ( SM_SCN_IS_DELETED( sSlotHeader->mCreateSCN ) == ID_TRUE )
    {
        return ID_TRUE;
    }

    return ID_FALSE;
}

/***********************************************************************
 * BUG-47367 Ager List ����ȭ
 * Description : table�� drop �Ǿ����� �˻��Ѵ�.
 * OID�� SCN ���� TableHeader.CreateSCN�� ũ�ٸ� (sSlotHeader->mCreateSCN �ƴ�!)
 * ��Ȱ��� TableSlot�̴�.
 ***********************************************************************/
inline idBool smcTable::isDropedTable4Ager( smcTableHeader * aHeader, smSCN aSCN )
{
    smpSlotHeader * sSlotHeader = (smpSlotHeader *)((smpSlotHeader *)aHeader - 1);

    /* 1.drop �ȵ� ���̺� �̸�
     *   smpSlotHeader.mCreateSCN
     *        => (create table commitSCN) or (touchTable ������ systemSCN/commitSCN)
     *    smcTableHeader.mTableCreateSCN��
     *        => create table�� commitSCN */

    /* 2.drop�� ���̺� commit �̸� 
     *   smpSlotHeader.mCreateSCN 
     *        => | SM_SCN_DELETE_BIT
     *     smcTableHeader.mTableCreateSCN��
     *        => SM_SCN_INFINITE */
    if ( SM_SCN_IS_DELETED( sSlotHeader->mCreateSCN ) == ID_TRUE )
    {
        return ID_TRUE;
    }

    /* 3.delete ager �� �����ؼ� drop table �� aging �ߴٸ� 
     *   smpSlotHeader.mCreateSCN 
     *        => SM_SCN_FREE_ROW
     *     smcTableHeader.mTableCreateSCN��
     *        => SM_SCN_INFINITE */

    /* 4.TableSlot ��Ȱ�� ��.
     *   smpSlotHeader.mCreateSCN 
     *        => SM_SCN_INFINITE
     *     smcTableHeader.mTableCreateSCN��
     *        => SM_SCN_INFINITE */

    /* 5.TableSlot ��Ȱ�� ��.
     *   smpSlotHeader.mCreateSCN 
     *        => create table�� commitSCN
     *     smcTableHeader.mTableCreateSCN��
     *        => create table�� commitSCN */
    if ( SM_SCN_IS_GT( &aHeader->mTableCreateSCN, &aSCN ) )
    {
        return ID_TRUE;
    }

    return ID_FALSE;
}


/**
 * PROJ-2462 Result Cache
 *
 * Description : Table Header��mSequenceInfo �� mCurSequence ���� ���� ��Ų��.
 * ������ Table Header�� mSequenceInfo�� ����ϰ� ���� ������ Reuslt Cache��
 * table�� insert/update/delete ���θ� �˱� ���� mCurSequence ���� Ȱ���Ѵ�.
 */
inline void smcTable::addModifyCount( smcTableHeader * aTableHeader )
{
    idCore::acpAtomicInc64( &aTableHeader->mSequence.mCurSequence );
}

/***********************************************************************
 * Description : table�� index ���� ��ȯ
 ***********************************************************************/
//PROJ-1362 LOB���� �ε��� ���� ���� Ǯ��.
inline UInt smcTable::getIndexCount( void * aHeader )
{
    smcTableHeader * sHeader;
    UInt  sTotalIndexLen ;
    UInt  i;

    IDE_DASSERT( aHeader != NULL );

    sHeader = (smcTableHeader *)aHeader;
    for ( i = 0, sTotalIndexLen = 0 ; i < SMC_MAX_INDEX_OID_CNT ; i++ )
    {
        sTotalIndexLen += sHeader->mIndexes[i].length;
    }

    return ( sTotalIndexLen / ID_SIZEOF(smnIndexHeader) );
}

/* PROJ-1362 LOB���� �ε��� ���� ���� Ǯ��. */
inline const void * smcTable::getTableIndex( void      * aHeader,
                                             const UInt  aIdx )
{
    smcTableHeader  * sHeader;
    smVCPieceHeader * sVCPieceHdr = NULL;

    UInt sOffset;
    UInt  i;
    UInt  sCurSize =0;

    IDE_DASSERT( aIdx < getIndexCount(aHeader) );

    sHeader = (smcTableHeader *) aHeader;
    sOffset = ID_SIZEOF(smnIndexHeader) * aIdx;
    for (i = 0 ; i < SMC_MAX_INDEX_OID_CNT; i++ )
    {
        if ( sHeader->mIndexes[i].fstPieceOID == SM_NULL_OID )
        {
            continue;
        }

        sCurSize = sHeader->mIndexes[i].length;

        if (sCurSize  <= sOffset)
        {
            sOffset -= sCurSize;
        }
        else
        {
            IDE_ASSERT( smmManager::getOIDPtr( 
                                    SMI_ID_TABLESPACE_SYSTEM_MEMORY_DIC, 
                                    sHeader->mIndexes[i].fstPieceOID,
                                    (void **)&sVCPieceHdr )
                        == IDE_SUCCESS );

            break;
        }
    }

    /* fix BUG-27232 [CodeSonar] Null Pointer Dereference */
    IDE_ASSERT( i != SMC_MAX_INDEX_OID_CNT );

    return ( (UChar *) sVCPieceHdr + ID_SIZEOF(smVCPieceHeader) + sOffset );
}

/***********************************************************************
 * Description : table oid�κ��� table header�� ��´�.
 * table�� OID�� slot�� header�� ����Ų��.
 ***********************************************************************/
inline IDE_RC smcTable::getTableHeaderFromOID( smOID     aOID, 
                                               void   ** aHeader )
{
    return smmManager::getOIDPtr( SMI_ID_TABLESPACE_SYSTEM_MEMORY_DIC, 
                                  aOID + SMP_SLOT_HEADER_SIZE, 
                                  (void **)aHeader );
}

/* PROJ-1362 QP-Large Record & Internal LOB support
   table�� �÷������� �ε��� ���� ���� Ǯ��. */
inline const smiColumn * smcTable::getColumn( const void * aTableHeader,
                                              const UInt   aIndex )
{
    smOID   sDummyOID;

    return getColumnAndOID( aTableHeader,
                            aIndex,
                            &sDummyOID );
}

#endif /* _O_SMC_TABLE_H_ */
