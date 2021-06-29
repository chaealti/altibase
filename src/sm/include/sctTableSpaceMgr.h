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
 * $Id: sdpTableSpace.h 15787 2006-05-19 02:26:17Z bskim $
 *
 * Description :
 *
 * ���̺����̽� ������
 *
 *
 **********************************************************************/

#ifndef _O_SCT_TABLE_SPACE_MGR_H_
#define _O_SCT_TABLE_SPACE_MGR_H_ 1

#include <sctDef.h>
// BUGBUG-1548 SDD�� SCT���� �� LAyer�̹Ƿ� �̿Ͱ��� Include�ؼ��� �ȵ�
#include <sdd.h>

struct smmTBSNode;
struct svmTBSNode;
class smxTrans;

class sctTableSpaceMgr
{
public:

    static IDE_RC initialize();

    static IDE_RC destroy();

    static inline idBool isBackupingTBS( scSpaceID  aSpaceID );

    // smiTBSLockValidType�� sctTBSLockValidOpt�� �����Ͽ� ��ȯ�Ѵ�.
    static sctTBSLockValidOpt
              getTBSLvType2Opt( smiTBSLockValidType aTBSLvType )
    {
        IDE_ASSERT( aTBSLvType < SMI_TBSLV_OPER_MAXMAX );

        return mTBSLockValidOpt[ aTBSLvType ];
    }

    // ( Disk/Memory ���� ) Tablespace Node�� �ʱ�ȭ�Ѵ�.
    static IDE_RC initializeTBSNode(sctTableSpaceNode * aSpaceNode,
                                    smiTableSpaceAttr * aSpaceAttr );

    // ( Disk/Memory ���� ) Tablespace Node�� �ı��Ѵ�.
    static IDE_RC destroyTBSNode(sctTableSpaceNode * aSpaceNode );


    // Tablespace�� Sync Mutex�� ȹ���Ѵ�.
    static IDE_RC latchSyncMutex( sctTableSpaceNode * aSpaceNode );

    // Tablespace�� Sync Mutex�� Ǯ���ش�.
    static IDE_RC unlatchSyncMutex( sctTableSpaceNode * aSpaceNode );

    // NEW ���̺����̽� ID�� �Ҵ��Ѵ�.
    static IDE_RC allocNewTableSpaceID( scSpaceID*   aNewID );

    // ���� NEW ���̺����̽� ID�� �����Ѵ�.
    static void   setNewTableSpaceID( scSpaceID aNewID )
                  { mNewTableSpaceID = aNewID; }

    // ���� NEW ���̺����̽� ID�� ��ȯ�Ѵ�.
    static scSpaceID getNewTableSpaceID() { return mNewTableSpaceID; }

    // ù ���̺����̽� ��带 ��ȯ�Ѵ�.
    static inline sctTableSpaceNode * getFirstSpaceNode();

    // ���� ���̺����̽��� �������� ���� ���̺����̽� ��带 ��ȯ�Ѵ�.
    static inline sctTableSpaceNode * getNextSpaceNode( scSpaceID aSpaceID );

    static inline sctTableSpaceNode *  getNextSpaceNodeIncludingDropped( scSpaceID aSpaceID );

    /* TASK-4007 [SM] PBT�� ���� ��� �߰� *
     * Drop���̰ų� Drop�� ���̰ų� �������� ���̺� �����̽� ��
     * ������ ���̺� �����̽��� �����´�*/
    static inline sctTableSpaceNode * getNextSpaceNodeWithoutDropped( scSpaceID aSpaceID );

    // �޸� ���̺����̽��� ���θ� ��ȯ�Ѵ�.
    static inline idBool isMemTableSpace( scSpaceID aSpaceID );
    static inline idBool isMemTableSpace( void * aSpaceNode );
    static inline idBool isMemTableSpaceType( smiTableSpaceType aSpaceType );

    // Volatile ���̺����̽��� ���θ� ��ȯ�Ѵ�.
    static inline idBool isVolatileTableSpace( scSpaceID aSpaceID );
    static inline idBool isVolatileTableSpace( void * aSpaceNode );
    static inline idBool isVolatileTableSpaceType( smiTableSpaceType aSpaceType );

    // ��ũ ���̺����̽��� ���θ� ��ȯ�Ѵ�.
    static inline idBool isDiskTableSpace( scSpaceID aSpaceID );
    static inline idBool isDiskTableSpace( void * aSpaceNode );
    static inline idBool isDiskTableSpaceType( smiTableSpaceType aSpaceType );

    // ��� ���̺����̽��� ���θ� ��ȯ�Ѵ�.
    static inline idBool isUndoTableSpace( scSpaceID aSpaceID );

    // �ӽ� ���̺����̽��� ���θ� ��ȯ�Ѵ�.
    static inline idBool isTempTableSpace( scSpaceID aSpaceID );
    static inline idBool isTempTableSpace( void * aSpaceNode );
    static inline idBool isTempTableSpaceType( smiTableSpaceType aSpaceType );

    static inline idBool isDataTableSpaceType( smiTableSpaceType  aType );

    // �ý��� �޸� ���̺����̽��� ���θ� ��ȯ�Ѵ�.
    static inline idBool isSystemMemTableSpace( scSpaceID aSpaceID );

    // TBS�� ���� ������ ��ȯ�Ѵ�. (Disk, Memory, Volatile)
    static inline smiTBSLocation getTBSLocation( scSpaceID aSpaceID );
    static inline smiTBSLocation getTBSLocation( sctTableSpaceNode * aSpaceNode );

    // PRJ-1548 User Memory Tablespace
    // �ý��� ���̺����̽��� ���θ� ��ȯ�Ѵ�.
    static inline idBool isSystemTableSpace( scSpaceID aSpaceID );

    static IDE_RC dumpTableSpaceList();

    // TBSID�� �Է¹޾� ���̺����̽��� �Ӽ��� ��ȯ�Ѵ�.
    static IDE_RC getTBSAttrByID( idvSQL   * aStatistics,
                                  scSpaceID          aID,
                                  smiTableSpaceAttr* aSpaceAttr );

    // Tablespace Attribute Flag�� Pointer�� ��ȯ�Ѵ�.
    static IDE_RC getTBSAttrFlagByID(scSpaceID       aID,
                                     UInt          * aAttrFlagPtr);
    // Tablespace Attribute Flag�� ��ȯ�Ѵ�.
    static UInt getTBSAttrFlag( sctTableSpaceNode * aSpaceNode );
    // Tablespace Attribute Flag�� �����Ѵ�.
    static void setTBSAttrFlag( sctTableSpaceNode * aSpaceNode,
                                UInt                aAttrFlag );
    // Tablespace�� Attribute Flag�κ��� �α� ���࿩�θ� ���´�
    static IDE_RC getSpaceLogCompFlag( scSpaceID aSpaceID, idBool *aDoComp );


    // TBS���� �Է¹޾� ���̺����̽��� �Ӽ��� ��ȯ�Ѵ�.
    static IDE_RC getTBSAttrByName(SChar*              aName,
                                   smiTableSpaceAttr*  aSpaceAttr);


    // TBS���� �Է¹޾� ���̺����̽��� �Ӽ��� ��ȯ�Ѵ�.
    static IDE_RC findSpaceNodeByName(SChar* aName,
                                      void** aSpaceNode ,
                                      idBool aLockSpace );


    // TBS���� �Է¹޾� ���̺����̽��� �����ϴ��� Ȯ���Ѵ�.
    static idBool checkExistSpaceNodeByName( SChar* aTableSpaceName );

    // Tablespace ID�� SpaceNode�� ã�´�.
    // �ش� Tablespace�� DROP�� ��� ������ �߻���Ų��.
    static IDE_RC findSpaceNodeBySpaceID( scSpaceID  aSpaceID,
                                          void**     aSpaceNode )
    {
        return findSpaceNodeBySpaceID( NULL,
                                       aSpaceID,
                                       aSpaceNode,
                                       ID_FALSE );
    };

    static IDE_RC findAndLockSpaceNodeBySpaceID( idvSQL   * aStatistics,
                                                 scSpaceID  aSpaceID,
                                                 void**     aSpaceNode )
    {
        return findSpaceNodeBySpaceID( aStatistics,
                                       aSpaceID,
                                       aSpaceNode,
                                       ID_TRUE );
    };

private:
    static IDE_RC findSpaceNodeBySpaceID( idvSQL   * aStatistics,
                                          scSpaceID  aSpaceID,
                                          void**     aSpaceNode,
                                          idBool     aLockSpace );
public:
    // Tablespace ID�� SpaceNode�� ã�´�.
    // ������ TBS Node �����͸� �ش�.
    // BUG-18743: �޸� �������� ������ ��� PBT�� �� �ִ�
    // ������ �ʿ��մϴ�.
    //   smmManager::isValidPageID���� ����ϱ� ���ؼ� �߰�.
    // �ٸ� ���������� �� �Լ� ��뺸�ٴ� findSpaceNodeBySpaceID ��
    // �����մϴ�. �ֳ��ϸ� ���Լ��� Validation�۾��� ���� �����ϴ�.
    static inline sctTableSpaceNode * getSpaceNodeBySpaceID( scSpaceID  aSpaceID )
    {
        return mSpaceNodeArray[aSpaceID];
    }

    // Tablespace ID�� SpaceNode�� ã�´�.
    // �ش� Tablespace�� DROP�� ��� SpaceNode�� NULL�� �����Ѵ�.
    static sctTableSpaceNode* findSpaceNodeWithoutException( scSpaceID  aSpaceID,
                                                             idBool     aUsingTBSAttr = ID_FALSE );


    // Tablespace ID�� SpaceNode�� ã�´�.
    // �ش� Tablespace�� DROP�� ��쿡�� aSpaceNode�� �ش� TBS�� ����.
    static sctTableSpaceNode* findSpaceNodeIncludingDropped( scSpaceID  aSpaceID )
    {  return mSpaceNodeArray[aSpaceID]; };

    // Tablespace�� �����ϴ��� üũ�Ѵ�.
    static idBool isExistingTBS( scSpaceID aSpaceID );

    // Tablespace�� ONLINE�������� üũ�Ѵ�.
    static idBool isOnlineTBS( scSpaceID aSpaceID );

    // Tablespace�� ���� State�� �ϳ��� State�� ���ϴ��� üũ�Ѵ�.
    static idBool hasState( scSpaceID   aSpaceID,
                            sctStateSet aStateSet,
                            idBool      aUsingTBSAttr = ID_FALSE );

    static idBool hasState( sctTableSpaceNode * aSpaceNode,
                            sctStateSet         aStateSet );

    // Tablespace�� State�� aStateSet���� State�� �ϳ��� �ִ��� üũ�Ѵ�.
    static idBool isStateInSet( UInt         aTBSState,
                                sctStateSet  aStateSet );

    // Tablespace���� Table/Index�� Open�ϱ� ���� Tablespace��
    // ��� �������� üũ�Ѵ�.
    static IDE_RC validateTBSNode( sctTableSpaceNode * aSpaceNode,
                                   sctTBSLockValidOpt  aTBSLvOpt );

    // ���̺����̽� ������ ����Ÿ���� ������ ���� ���ü� ����
    static IDE_RC lockForCrtTBS( ) { return mMtxCrtTBS.lock( NULL ); }
    static IDE_RC unlockForCrtTBS( ) { return mMtxCrtTBS.unlock(); }

    // PRJ-1548 User Memory Tablespace
    // ��ݾ��� ���̺����̽��� ���õ� Sync����� Drop ���갣�� ���ü�
    // �����ϱ� ���� Sync Mutex�� ����Ѵ�.

    static void addTableSpaceNode( sctTableSpaceNode *aSpaceNode );
    static void removeTableSpaceNode( sctTableSpaceNode *aSpaceNode );

    // ���������ÿ� ��� �ӽ� ���̺����̽��� Reset �Ѵ�.
    static IDE_RC resetAllTempTBS( void *aTrans );

    //for PRJ-1149 added functions
    static void getDataFileNodeByName(SChar*            aFileName,
                                      sddDataFileNode** aFileNode,
                                      scSpaceID*        aTbsID,
                                      scPageID*         aFstPageID = NULL,
                                      scPageID*         aLstPageID = NULL,
                                      SChar**           aSpaceName = NULL);


    // Ʈ����� Ŀ�� ������ �����ϱ� ���� ������ ���
    static IDE_RC addPendingOperation( void               *aTrans,
                                       scSpaceID           aSpaceID,
                                       idBool              aIsCommit,
                                       sctPendingOpType    aPendingOpType,
                                       sctPendingOp      **aPendingOp = NULL );


    // ���̺����̽� ���� Ʈ����� Commit-Pending ������ �����Ѵ�.
    static IDE_RC executePendingOperation( idvSQL *aStatistics,
                                           void   *aPendingOp,
                                           idBool  aIsCommit);

    // PRJ-1548 User Memory Tablespace
    //
    // # ��ݰ��������� ����
    //
    // �������� table�� tablespace ���� ������ lock hierachy�� ������ ��ݰ����� �ϰ� �ִ�.
    // �̷��� ���������� case by case�� ���� �κ��� ó���ؾ��ϴ� ��찡 ���� �߻��� ���̴�.
    // ������� offline ������ �� ��, ���� table�� ���� X ����� ȹ���ϱ�� �Ͽ��� ��쿡
    // �������� table�̳� drop ���� table�� ���ؼ��� ���ó���� ��� �ϴ��Ķ�� ������ �����.
    // �Ƹ� catalog page�� ���ؼ� ����� �����ؾ� ������ �𸥴�.
    // �̷��� ��츦 ������� ���� ������ tablespace ����� ����� ȹ���ϵ��� �����Ѵٸ�
    // �� �� ����ϰ� �ذ��� �� ���� ���̰�,
    // ���� ������ ��å�� �ƴҷ���..
    //
    // A. TBS NODE -> CATALOG TABLE -> TABLE -> DBF NODE ������
    //    lock hierachy�� �����Ѵ�.
    //
    // B. DML ����� TBS Node�� ���ؼ��� INTENTION LOCK�� ��û�ϵ��� ó���Ѵ�.
    // STATEMENT�� CURSOR ���½� ó���ϰ�, SAVEPOINT�� ó���ؾ��Ѵ�.
    // ���̺����̽� ���� DDL���� ���ü� ��� �����ϴ�.
    //
    // C. SYSTEM TABLESPACE�� drop�� �Ұ����ϹǷ� ����� ȹ������ �ʾƵ� �ȴ�.
    //
    // D. DDL ����� TBS Node �Ǵ� DBF Node�� ���ؼ� ����� ��û�ϵ��� ó���Ѵ�.
    // TABLE/INDEX� ���� DDL�� ���ؼ��� ���� TBS�� ����� ȹ���ϵ��� �Ѵ�.
    // ���̺����̽� ���� DDL���� ���ü� ��� �����ϴ�.
    // ��, PSM/VIEW/SEQUENCE���� SYSTEM_DICTIONARY_TABLESPACE�� �����ǹǷ� �����
    // ȹ���� �ʿ�� ����.
    //
    // E. LOCK ��û ����� ���̱� ���ؼ� TABLE_LOCK_ENABLE�� ����� �ƶ�����
    // TABLESPACE_LOCK_ENABLE ����� �����Ͽ�, TABLESPACE DDL�� ����ϴ� �ʴ� ����������
    // TBS LIST, TBS NODE, DBF NODE �� ���� LOCK ��û�� ���� ���ϵ��� ó���Ѵ�.


    // ���̺��� ���̺����̽��鿡 ���Ͽ� INTENTION ����� ȹ���Ѵ�.
    // smiValidateAndLockTable(), smiTable::lockTable, Ŀ�� open�� ȣ��
    static IDE_RC lockAndValidateTBS(
                      void                * aTrans,      /* [IN]  */
                      scSpaceID             aSpaceID,    /* [IN]  */
                      sctTBSLockValidOpt    aTBSLvOpt,   /* [IN]  */
                      idBool                aIsIntent,   /* [IN]  */
                      idBool                aIsExclusive,/* [IN]  */
                      ULong                 aLockWaitMicroSec ); /* [IN] */

    // ���̺�� ���õ� ���̺����̽��鿡 ���Ͽ� INTENTION ����� ȹ���Ѵ�.
    // smiValidateAndLockTable(), smiTable::lockTable, Ŀ�� open�� ȣ��
    static IDE_RC lockAndValidateRelTBSs(
                    void                 * aTrans,           /* [IN] */
                    void                 * aTable,           /* [IN] */
                    sctTBSLockValidOpt     aTBSLvOpt,        /* [IN] */
                    idBool                 aIsIntent,        /* [IN] */
                    idBool                 aIsExclusive,     /* [IN] */
                    ULong                  aLockWaitMicroSec ); /* [IN] */

    // Tablespace Node�� ��� ȹ�� ( ���� : Tablespace ID )
    static IDE_RC lockTBSNodeByID( void              * aTrans,
                                   scSpaceID           aSpaceID,
                                   idBool              aIsIntent,
                                   idBool              aIsExclusive,
                                   ULong               aLockWaitMicroSec,
                                   sctTBSLockValidOpt  aTBSLvOpt,
                                   idBool            * aLocked,
                                   sctLockHier       * aLockHier );

    // Tablespace Node�� ��� ȹ�� ( ���� : Tablespace Node )
    static IDE_RC lockTBSNode( void              * aTrans,
                               sctTableSpaceNode * aSpaceNode,
                               idBool              aIsIntent,
                               idBool              aIsExclusive,
                               ULong               aLockWaitMicroSec,
                               sctTBSLockValidOpt  aTBSLvOpt,
                               idBool           *  aLocked,
                               sctLockHier      *  aLockHier );



    // Tablespace Node�� ��� ȹ�� ( ���� : Tablespace ID )
    static IDE_RC lockTBSNodeByID( void              * aTrans,
                                   scSpaceID           aSpaceID,
                                   idBool              aIsIntent,
                                   idBool              aIsExclusive,
                                   sctTBSLockValidOpt  aTBSLvOpt );

    // Tablespace Node�� ��� ȹ�� ( ���� : Tablespace Node )
    static IDE_RC lockTBSNode( void              * aTrans,
                               sctTableSpaceNode * aSpaceNode,
                               idBool              aIsIntent,
                               idBool              aIsExclusive,
                               sctTBSLockValidOpt  aTBSLvOpt );

    // ������ Tablespace�� ���� Ư�� Action�� �����Ѵ�.
    static IDE_RC doAction4EachTBS( idvSQL           * aStatistics,
                                    sctAction4TBS      aAction,
                                    void             * aActionArg,
                                    sctActionExecMode  aActionExecMode );

    // DDL_LOCK_TIMEOUT ������Ƽ�� ���� ���ð��� ��ȯ�Ѵ�.
    static ULong getDDLLockTimeOut(smxTrans * aTrans);

    /* BUG-34187 ������ ȯ�濡�� �����ÿ� �������ø� ȥ���ؼ� ��� �Ұ��� �մϴ�. */
#if defined(VC_WIN32)
    static void adjustFileSeparator( SChar * aPath );
#endif

    /* ����� ��ȿ�� �˻� �� ����θ� ������ ��ȯ */
    static IDE_RC makeValidABSPath( idBool         aCheckPerm,
                                    SChar*         aValidName,
                                    UInt*          aNameLength,
                                    smiTBSLocation aTBSLocation );

    /* BUG-38621 */
    static IDE_RC makeRELPath( SChar        * aName,
                               UInt         * aNameLength,
                               smiTBSLocation aTBSLocation );

    // PRJ-1548 User Memory TableSpace ���䵵��

    // ��ũ/�޸� ���̺����̽��� ������¸� �����Ѵ�.
    static IDE_RC startTableSpaceBackup( idvSQL            * aStatistics,
                                         sctTableSpaceNode * aSpaceNode );

    // ��ũ/�޸� ���̺����̽��� ������¸� �����Ѵ�.
    static IDE_RC endTableSpaceBackup( idvSQL            * aStatistics,
                                       sctTableSpaceNode * aSpaceNode );

    // PRJ-1149 checkpoint ������ DBF ��忡 �����ϱ� ����
    // �����ϴ� �Լ�.
    static void setRedoLSN4DBFileMetaHdr( smLSN* aDiskRedoLSN,
                                          smLSN* aMemRedoLSN );

    // �ֱ� üũ����Ʈ���� ������ ��ũ Redo LSN�� ��ȯ�Ѵ�.
    static smLSN* getDiskRedoLSN()  { return &mDiskRedoLSN; };
    // �ֱ� üũ����Ʈ���� ������ �޸� Redo LSN �迭�� ��ȯ�Ѵ�.
    static smLSN* getMemRedoLSN() { return &mMemRedoLSN; };

    ////////////////////////////////////////////////////////////////////
    // Alter Tablespace Online/Offline �� ���������� ����ϴ� �Լ�
    ////////////////////////////////////////////////////////////////////

    // Alter Tablespace Online/Offline�� ���� ����ó���� �����Ѵ�.
    static IDE_RC checkError4AlterStatus( sctTableSpaceNode    * aTBSNode,
                                          smiTableSpaceState     aNewTBSState );


    // Tablespace�� ���� �������� Backup�� �Ϸ�Ǳ⸦ ��ٸ� ��,
    // Tablespace�� ��� �Ұ����� ���·� �����Ѵ�.
    static IDE_RC wait4BackupAndBlockBackup( idvSQL           * aStatistics,
                                             sctTableSpaceNode* aTBSNode,
                                             smiTableSpaceState aTBSSwitchingState );

    static inline void wait4Backup( void * aSpaceNode );
    static inline void wakeup4Backup( void * aSpaceNode );

    // Tablespace�� DISCARDED���·� �ٲٰ�, Loganchor�� Flush�Ѵ�.
    static IDE_RC alterTBSdiscard( sctTableSpaceNode  * aTBSNode );

    // Tablespace�� mMaxTblDDLCommitSCN�� aCommitSCN���� �����Ѵ�.
    static void updateTblDDLCommitSCN( scSpaceID aSpaceID,
                                       smSCN     aCommitSCN);

    // Tablespace�� ���ؼ� aViewSCN���� Drop Tablespace�� �Ҽ��ִ����� �˻��Ѵ�.
    static IDE_RC canDropByViewSCN( scSpaceID aSpaceID,
                                    smSCN     aViewSCN);

    static IDE_RC setMaxFDCntAllDFileOfAllDiskTBS( UInt aMaxFDCnt4File );

    static void lockSpaceNode( idvSQL   * aStatistics,
                               void     * aSpaceNode )
    { (void)((sctTableSpaceNode*)aSpaceNode)->mMutex.lock( aStatistics ); };

    static void unlockSpaceNode( void   * aSpaceNode )
    { (void)((sctTableSpaceNode*)aSpaceNode)->mMutex.unlock( ); };

private:
    // BUG-23953
    static inline smiTableSpaceType getTableSpaceType( scSpaceID           aSpaceID );

    static inline sctTableSpaceNode * findNextSpaceNode( scSpaceID aSpaceID,
                                                         UInt      aSkipStateSet );

    static sctTableSpaceNode * mSpaceNodeArray[SC_MAX_SPACE_ARRAY_SIZE]; // BUG-48513
    static scSpaceID           mNewTableSpaceID;


    // ���̺����̽� ������ ����Ÿ���� ������ ���� ���ü� ����
    static iduMutex            mMtxCrtTBS;

    // Drop DBF�� AllocPageCount ���갣�� ���ü� ����
    static iduMutex            mGlobalPageCountCheckMutex;

    // PRJ-1548 ��ũ/�޸� ��������� ������ ���� �ֱٿ�
    // �߻��� üũ����Ʈ ����
    static smLSN               mDiskRedoLSN;
    static smLSN               mMemRedoLSN;

    // BUG-17285 Disk Tablespace �� OFFLINE/DISCARD �� DROP�� �����߻�
    static sctTBSLockValidOpt mTBSLockValidOpt[ SMI_TBSLV_OPER_MAXMAX ];
};

/***********************************************************************
 *
 * Description : ���̺����̽��� Backup �������� üũ�Ѵ�.
 *
 * aSpaceID  - [IN] ���̺����̽� ID
 *
 **********************************************************************/
inline idBool sctTableSpaceMgr::isBackupingTBS( scSpaceID  aSpaceID )
{
    return (((mSpaceNodeArray[aSpaceID]->mState & SMI_TBS_BACKUP)
             == SMI_TBS_BACKUP) ? ID_TRUE : ID_FALSE );
}

/***********************************************************************
 * Description : ���̺����̽� type�� ���� 3���� Ư���� ��ȯ�Ѵ�.
 * [IN]  aSpaceID : �м��Ϸ��� ���̺� �����̽��� ID
 * [OUT] aTableSpaceType : aSpaceID�� �ش��ϴ� ���̺����̽���
 *              1) �ý��� ���̺����̽� �ΰ�?
 *              2) ���� ���̺����̽� �ΰ�?
 *              3) ����Ǵ� ��ġ(MEM, DISK, VOL)
 *                          �� ���� ������ ��ȯ�Ѵ�.
 **********************************************************************/
inline smiTableSpaceType sctTableSpaceMgr::getTableSpaceType( scSpaceID   aSpaceID )
{
    sctTableSpaceNode * sSpaceNode      = NULL;

    IDU_FIT_POINT("BUG-46450@sctTableSpaceMgr::getTableSpaceType::TablespaceType");

    if ( aSpaceID >= mNewTableSpaceID )
    {
        return SMI_TABLESPACE_TYPE_MAX;
    }
    
    sSpaceNode = mSpaceNodeArray[ aSpaceID ];

    if ( sSpaceNode == NULL )
    {
        return SMI_TABLESPACE_TYPE_MAX;
    }

    return sSpaceNode->mType ;

#ifdef ALTIBASE_FIT_CHECK
    IDE_EXCEPTION_END;
    return SMI_TABLESPACE_TYPE_MAX;
#endif
}

/**********************************************************************
 * Description : TBS�� ���� ����(�޸�, ��ũ, �ӽ�)�� ��ȯ�Ѵ�.
 *
 *   aSpaceID - [IN] ���� ������ Ȯ���� TBS�� ID
 **********************************************************************/
smiTBSLocation sctTableSpaceMgr::getTBSLocation( sctTableSpaceNode * aSpaceNode )
{
    switch ( aSpaceNode->mType )
    {
        case SMI_MEMORY_SYSTEM_DICTIONARY:
        case SMI_MEMORY_SYSTEM_DATA:
        case SMI_MEMORY_USER_DATA:
            return SMI_TBS_MEMORY;
            break;
        case SMI_DISK_SYSTEM_DATA:
        case SMI_DISK_USER_DATA:
        case SMI_DISK_SYSTEM_TEMP:
        case SMI_DISK_USER_TEMP:
        case SMI_DISK_SYSTEM_UNDO:
            return SMI_TBS_DISK;
            break;
        case SMI_VOLATILE_USER_DATA:
            return SMI_TBS_VOLATILE;
            break;

        default:
            break;
    }
    return SMI_TBS_NONE;
}

smiTBSLocation sctTableSpaceMgr::getTBSLocation( scSpaceID aSpaceID )
{
    if ( aSpaceID >= mNewTableSpaceID )
    {
        return SMI_TBS_NONE;
    }

    if ( NULL == mSpaceNodeArray[ aSpaceID ] )
    {
        return SMI_TBS_NONE;
    }
   
    return getTBSLocation( mSpaceNodeArray[ aSpaceID ] );
}


/**********************************************************************
 * Description : �ý��� ���̺����̽� ���� ��ȯ
 **********************************************************************/
idBool sctTableSpaceMgr::isSystemTableSpace( scSpaceID aSpaceID )
{
    if (( aSpaceID == SMI_ID_TABLESPACE_SYSTEM_MEMORY_DIC  ) ||
        ( aSpaceID == SMI_ID_TABLESPACE_SYSTEM_MEMORY_DATA ) ||
        ( aSpaceID == SMI_ID_TABLESPACE_SYSTEM_DISK_DATA   ) ||
        ( aSpaceID == SMI_ID_TABLESPACE_SYSTEM_DISK_UNDO   ) ||
        ( aSpaceID == SMI_ID_TABLESPACE_SYSTEM_DISK_TEMP   ))
    {
        return ID_TRUE;
    }
    return ID_FALSE;
}


/**********************************************************************
 * Description :
 **********************************************************************/
idBool sctTableSpaceMgr::isSystemMemTableSpace( scSpaceID aSpaceID )
{
    if (( aSpaceID == SMI_ID_TABLESPACE_SYSTEM_MEMORY_DIC ) ||
        ( aSpaceID == SMI_ID_TABLESPACE_SYSTEM_MEMORY_DATA ))
    {
        return ID_TRUE;
    }
    return ID_FALSE;
}

inline idBool sctTableSpaceMgr::isMemTableSpace( scSpaceID aSpaceID )
{
    smiTableSpaceType sType = getTableSpaceType( aSpaceID );

    return isMemTableSpaceType( sType ) ;
}

inline idBool sctTableSpaceMgr::isMemTableSpace( void * aSpaceNode )
{
    return isMemTableSpaceType( ((sctTableSpaceNode*)aSpaceNode)->mType ) ;
}

inline idBool sctTableSpaceMgr::isMemTableSpaceType( smiTableSpaceType aSpaceType )
{
    if (( aSpaceType == SMI_MEMORY_SYSTEM_DATA       ) ||
        ( aSpaceType == SMI_MEMORY_SYSTEM_DICTIONARY ) ||
        ( aSpaceType == SMI_MEMORY_USER_DATA         ))
    {
        return ID_TRUE;
    }
    return ID_FALSE ;
}

inline idBool sctTableSpaceMgr::isVolatileTableSpace( scSpaceID aSpaceID )
{
    smiTableSpaceType  sType = getTableSpaceType( aSpaceID );
    
    return isVolatileTableSpaceType( sType ) ;
}

inline idBool sctTableSpaceMgr::isVolatileTableSpace( void * aSpaceNode )
{   
    return isVolatileTableSpaceType( ((sctTableSpaceNode*)aSpaceNode)->mType ) ;
}

inline idBool sctTableSpaceMgr::isVolatileTableSpaceType( smiTableSpaceType aSpaceType )
{
    return ( SMI_VOLATILE_USER_DATA == aSpaceType ) ? ID_TRUE : ID_FALSE ;
}

inline idBool sctTableSpaceMgr::isDiskTableSpace( scSpaceID aSpaceID )
{
    smiTableSpaceType sType = getTableSpaceType( aSpaceID );

    return isDiskTableSpaceType( sType ) ;
}

inline idBool sctTableSpaceMgr::isDiskTableSpace( void * aSpaceNode )
{
    return isDiskTableSpaceType( ((sctTableSpaceNode*)aSpaceNode)->mType ) ;
}

inline idBool sctTableSpaceMgr::isDiskTableSpaceType( smiTableSpaceType aSpaceType )
{
    if (( aSpaceType == SMI_DISK_SYSTEM_DATA ) ||
        ( aSpaceType == SMI_DISK_USER_DATA   ) ||
        ( aSpaceType == SMI_DISK_SYSTEM_TEMP ) ||
        ( aSpaceType == SMI_DISK_USER_TEMP   ) ||
        ( aSpaceType == SMI_DISK_SYSTEM_UNDO ))
    {
        return ID_TRUE;
    }
    return ID_FALSE ;
}

inline idBool sctTableSpaceMgr::isTempTableSpace( scSpaceID aSpaceID )
{
    smiTableSpaceType  sType = getTableSpaceType( aSpaceID );

    return isTempTableSpaceType( sType ) ;
}

inline idBool sctTableSpaceMgr::isTempTableSpace( void * aSpaceNode )
{
    return isTempTableSpaceType( ((sctTableSpaceNode*)aSpaceNode)->mType ) ;
}

inline idBool sctTableSpaceMgr::isTempTableSpaceType( smiTableSpaceType aSpaceType )
{
    if (( aSpaceType == SMI_DISK_SYSTEM_TEMP ) ||
        ( aSpaceType == SMI_DISK_USER_TEMP   ))
    {
        return ID_TRUE;
    }
    return ID_FALSE;
}

inline idBool sctTableSpaceMgr::isDataTableSpaceType( smiTableSpaceType  aSpaceType )
{
    if (( aSpaceType == SMI_MEMORY_SYSTEM_DATA ) ||
        ( aSpaceType == SMI_MEMORY_USER_DATA   ) ||
        ( aSpaceType == SMI_VOLATILE_USER_DATA ) ||
        ( aSpaceType == SMI_DISK_SYSTEM_DATA   ) ||
        ( aSpaceType == SMI_DISK_USER_DATA     ))
    {
        return ID_TRUE;
    }
    return ID_FALSE;
}

inline idBool sctTableSpaceMgr::isUndoTableSpace( scSpaceID aSpaceID )
{
   return ( aSpaceID == SMI_ID_TABLESPACE_SYSTEM_DISK_UNDO ) ? ID_TRUE : ID_FALSE ;
}

inline sctTableSpaceNode * sctTableSpaceMgr::getFirstSpaceNode()
{
    return mSpaceNodeArray[0];
}

inline sctTableSpaceNode * sctTableSpaceMgr::getNextSpaceNode( scSpaceID aSpaceID )
{
    return findNextSpaceNode( aSpaceID,
                              SMI_TBS_DROPPED );
}

inline sctTableSpaceNode * sctTableSpaceMgr::getNextSpaceNodeIncludingDropped( scSpaceID aSpaceID )
{
    return findNextSpaceNode( aSpaceID,
                              0/* Do Not Skip Any TBS
                                  (Including Dropped) */ );
}

inline sctTableSpaceNode * sctTableSpaceMgr::getNextSpaceNodeWithoutDropped( scSpaceID  aSpaceID )
{
    return findNextSpaceNode( aSpaceID,
                              ( SMI_TBS_DROPPED | 
                                SMI_TBS_CREATING |
                                SMI_TBS_DROP_PENDING )  );
}

/**********************************************************************
 * Description : �Է¹��� TableSpace ID �� ���� ��밡���� TableSpace Node�� ��ȯ�Ѵ�.
 *
 *   aSpaceID - [IN] ���� TBS�� ID
 **********************************************************************/
inline sctTableSpaceNode * sctTableSpaceMgr::findNextSpaceNode( scSpaceID aSpaceID,
                                                                UInt      aSkipStateSet )
{
    sctTableSpaceNode* sSpaceNode ;

    while( ++aSpaceID < mNewTableSpaceID )
    {
        sSpaceNode = mSpaceNodeArray[aSpaceID];

        if ( sSpaceNode != NULL )
        {
            if ( (sSpaceNode->mState & aSkipStateSet) == 0 )
            {
                IDE_DASSERT( aSpaceID == sSpaceNode->mID );
                return sSpaceNode;
            }
        }
    }

    return NULL;
}

/***********************************************************************
 * Description : TableSpace������ waiting�� wakeup ȣ�� �Լ�
 *
 * FATAL ������ ���ڸ����� �ٷ� ����ǹǷ� ���ܷ� ó������ �ʾƵ� �ȴ�.
 **********************************************************************/
inline void sctTableSpaceMgr::wait4Backup( void * aSpaceNode )
{
    PDL_Time_Value sTimeValue;

    sTimeValue.set( idlOS::time(NULL) +
                    smuProperty::getDataFileBackupEndWaitInterval() );

    if ( ((sctTableSpaceNode*)aSpaceNode)->mBackupCV.timedwait(
             &(((sctTableSpaceNode*)aSpaceNode)->mMutex),
             &sTimeValue,
             IDU_IGNORE_TIMEDOUT ) != IDE_SUCCESS )
    {
        IDE_SET( ideSetErrorCode( smERR_FATAL_ThrCondWait ));
    }
    return;
}

inline void sctTableSpaceMgr::wakeup4Backup( void * aSpaceNode )
{
    if ( ((sctTableSpaceNode*)aSpaceNode)->mBackupCV.broadcast() != IDE_SUCCESS )
    {
        IDE_SET( ideSetErrorCode( smERR_FATAL_ThrCondSignal ));
    }
    return;
}

#endif // _O_SCT_TABLE_SPACE_H_


