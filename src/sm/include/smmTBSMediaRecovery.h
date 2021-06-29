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
 * $Id: smmTBSMediaRecovery.h 19201 2006-11-30 00:54:40Z kmkim $
 **********************************************************************/

#ifndef _O_SMM_TBS_MEDIA_RECOVERY_H_
#define _O_SMM_TBS_MEDIA_RECOVERY_H_ 1

#include <idu.h>
#include <iduMemPool.h>
#include <idp.h>
#include <smDef.h>
#include <smmDef.h>
#include <smu.h>
#include <smriDef.h>

/*
  [����] SMM�ȿ����� File���� Layer�� ������ ������ ����.
         ���� Layer�� �ڵ忡���� ���� Layer�� �ڵ带 ����� �� ����.

  ----------------------------------------------------------------------------
  smmTBSCreate          ; Create Tablespace ����
  smmTBSDrop            ; Drop Tablespace ����
  smmTBSAlterAutoExtend ; Alter Tablespace Auto Extend ����
  smmTBSAlterChkptPath  ; Alter Tablespace Add/Rename/Drop Checkpoint Path����
  smmTBSAlterDiscard    ; Alter Tablespace Discard ����
  smmTBSStartupShutdown ; Startup, Shutdown���� Tablespace���� ó���� ����
  ----------------------------------------------------------------------------
  smmTBSChkptPath  ; Tablespace�� Checkpoint Path ����
  smmTBSMultiPhase ; Tablespace�� �ٴܰ� �ʱ�ȭ
  ----------------------------------------------------------------------------
  smmManager       ; Tablespace�� ���� ���� 
  smmFPLManager    ; Tablespace Free Page List�� ���� ����
  smmExpandChunk   ; Chunk�� ���α��� ����
  ----------------------------------------------------------------------------
  
  c.f> Memory Tablespace�� Alter Online/Offline�� smp layer�� �����Ǿ� �ִ�.
*/


/*
   Memory DB�� Tablespace���� ������� ������ class
   - Create Tablespace 
   - Drop Tablespace
   - Alter Tablespace Add/Rename/Drop Checkpoint Path
   - Alter Tablespace AutoExtend ...

   ����� Alter Tablespace Online/Offline��
   smpTBSAlterOnOff class�� �����Ǿ� �ִ�.
 */
class smmTBSMediaRecovery
{
public :
    // ������ (�ƹ��͵� ����)
    smmTBSMediaRecovery();

public :
    // ��� Chkpt Images���� ��Ÿ����� üũ���������� �����Ѵ�. 
    static IDE_RC updateDBFileHdr4AllTBS();

    // ���̺����̽��� ������ ������ ���Ϲ�ȣ�� ��ȯ�Ѵ�. 
    static UInt   getLstCreatedDBFile( smmTBSNode    * aTBSNode )
                  { return aTBSNode->mLstCreatedDBFile; }

//    // ���̺����̽��� N��° ����Ÿ������ ������ ���� ��ȯ
//    static void getPageRangeOfNthFile( smmTBSNode * aTBSNode, 
//                                       UInt         aFileNum,
//                                       scPageID   * aFstPageID, 
//                                       scPageID   * aLstPageID );

    ////////////////////////////////////////////////////////////////////
    // Backup ���� �Լ�
    ////////////////////////////////////////////////////////////////////

    // PRJ-1548 User Memory Tablespace 
    // ��� �޸� ���̺����̽��� ����Ѵ�. 
    static IDE_RC backupAllMemoryTBS(
                        idvSQL * aStatistics,
                        void   * aTrans,
                        SChar  * aBackupDir );

    
    // PROJ-2133 incremental backup
    // ��� �޸� ���̺����̽��� incremental backup�Ѵ�. 
    static IDE_RC incrementalBackupAllMemoryTBS(
                        idvSQL     * aStatistics,
                        void       * aTrans,
                        smriBISlot * aCommonBackupInfo,
                        SChar      * aBackupDir );

    ////////////////////////////////////////////////////////////////////
    // Media Recovery ���� �Լ�
    ////////////////////////////////////////////////////////////////////

    // ���̺����̽��� �̵������� �ִ� ����Ÿ���� ����� �����.
    static IDE_RC makeMediaRecoveryDBFList( sctTableSpaceNode * sSpaceNode, 
                                            smiRecoverType      aRecoveryType,
                                            UInt              * aFailureChkptImgCount, 
                                            smLSN             * aFromRedoLSN,
                                            smLSN             * aToRedoLSN );

    // ��� �޸� ���̺����̽��� ��� �޸� DBFile���� 
    // �̵�� ���� �ʿ� ���θ� üũ�Ѵ�.
    static IDE_RC identifyDBFilesOfAllTBS( idBool aIsOnCheckPoint );

    // ��� ����Ÿ������ ��Ÿ����� �ǵ��Ͽ� 
    // �̵�� �������θ� Ȯ���Ѵ�. 
    static IDE_RC doActIdentifyAllDBFiles( 
                               idvSQL            * aStatistics, 
                               sctTableSpaceNode  * aSpaceNode,
                               void               * aActionArg );

    // ���� ������ ����Ÿ������ ��Ÿ�� ����� CreateLSN�� 
    static IDE_RC setCreateLSN4NewDBFiles( smmTBSNode * aSpaceNode,
                                           smLSN      * aCreateLSN );

    // �̵������� ���� �̵����� ������ �޸� ����Ÿ���ϵ��� 
    // ã�Ƽ� ��������� �����Ѵ�. 
    static IDE_RC doActRepairDBFHdr( 
                               idvSQL             * aStatistics, 
                               sctTableSpaceNode  * aSpaceNode,
                               void               * aActionArg );

    // ��� ���̺����̽��� ����Ÿ���Ͽ��� �Էµ� ������ ID�� ������ 
    // Failure ����Ÿ������ ���翩�θ� ��ȯ�Ѵ�. 
    static IDE_RC findMatchFailureDBF( scSpaceID   aTBSID,
                                       scPageID    aPageID, 
                                       idBool    * aIsExistTBS,
                                       idBool    * aIsFailureDBF );

    // �̵����� ���̺����̽��鿡 �Ҵ��ߴ� �ڿ����� �����Ѵ�.. 
    static IDE_RC resetMediaFailureMemTBSNodes();
    
    // online backup�� �����Ѵ�. 
    static IDE_RC doActOnlineBackup( 
                             idvSQL            * aStatistics, 
                             sctTableSpaceNode * aSpaceNode,
                             void              * aActionArg );


    // ����Ÿ������ ��Ÿ����� �����Ѵ�. 
    static IDE_RC doActUpdateAllDBFileHdr(
                             idvSQL            * aStatistics, 
                             sctTableSpaceNode * aSpaceNode,
                             void              * aActionArg );


    // �̵����� ���̺����̽��� �Ҵ��ߴ�
    // �ڿ����� �����Ѵ�. ( Action �Լ� )
    static IDE_RC doActResetMediaFailureTBSNode( 
                            idvSQL            * aStatistics, 
                            sctTableSpaceNode * aTBSNode,
                            void              * /* aActionArg */ );

    // Tablespace�� State �ܰ�� ���ȴٰ� �ٽ� Page�ܰ�� �ø���. 
    static IDE_RC resetTBSNode(smmTBSNode * aTBSNode);

private:
    // �ϳ��� Tablespace�� ���� ��� DB file�� Header�� Redo LSN�� ���
    static IDE_RC flushRedoLSN4AllDBF( smmTBSNode * aSpaceNode );
};

#endif /* _O_SMM_TBS_MEDIA_RECOVERY_H_ */
