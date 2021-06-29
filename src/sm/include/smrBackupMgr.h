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
 * $Id: smrBackupMgr.h 86110 2019-09-02 04:52:04Z et16 $
 *
 * Description :
 *
 * �� ������ ��� �����ڿ� ���� ��� �����̴�.
 *
 * # ���
 *   1.
 **********************************************************************/

#ifndef _O_SMR_BACKUP_MGR_H_
#define _O_SMR_BACKUP_MGR_H_ 1

#include <idu.h>
#include <idp.h>
#include <smrDef.h>
#include <sctTableSpaceMgr.h>
#include <smriDef.h>

class smrBackupMgr
{

public:

    static IDE_RC initialize();
    static IDE_RC destroy();

    static inline idBool isBackupingTBS( scSpaceID   aSpaceID );

    // ALTER DATABASE BACKUP LOGANCHOR .. ������ ���ؼ�
    // Loganchor���� ����� �����Ѵ�.
    static IDE_RC backupLogAnchor( idvSQL*  aStatistics,
                                   SChar  * aDestFilePath );

    // ALTER TABLESPACE BACKUP .. ������ ���ؼ� TABLESPACE LEVEL��
    // ����� �����Ѵ�.
    static IDE_RC backupTableSpace(idvSQL*   aStatistics,
                                   void *    aTrans,
                                   scSpaceID aTbsID,
                                   SChar*    aBackupDir);

    // ALTER DATABASE BACKUP .. ������ ���ؼ� DB LEVEL��
    // ��ü����� �����Ѵ�.
    static IDE_RC backupDatabase(idvSQL* aStatistics,
                                 void  * aTrans,
                                 SChar * aBackupDir);

    // .. BACKUP BEGIN.. ������ ���ؼ� ���̺����̽� ������
    // ������·� �����Ѵ�.
    static IDE_RC beginBackupTBS(scSpaceID aSpaceID);

    // .. BACKUP END.. ������ ���ؼ� ���̺����̽� ������
    // ������¸� �����Ѵ�.
    static IDE_RC endBackupTBS(scSpaceID aSpaceID);

    // �޸� ���̺����̽��� ���� ���� ����� �����Ѵ�.
    static IDE_RC backupMemoryTBS( idvSQL*      aStatistics,
                                   smmTBSNode * aSpaceNode,
                                   SChar     *  aBackupDir );

    // ��ũ ���̺����̽��� ���� ���� ����� �����Ѵ�.
    static IDE_RC backupDiskTBS( idvSQL            * aStatistics,
                                 sddTableSpaceNode * aSpaceNode,
                                 SChar             * aBackupDir );

    // ����ϷḦ ���ؼ� �α������� ������ ��ī�̺��Ų��.
    static IDE_RC swithLogFileByForces();

    //�ش� path�� ��� memory db ���� file���� ������
    static IDE_RC unlinkChkptImages( SChar* aPathName,
                                     SChar* aTBSName );

    // �ش� path�� disk db ���� file�� ������
    static IDE_RC unlinkDataFile( SChar*  aDataFileName );

    // �ش� path�� ��� disk db ���� log file���� ������
    static IDE_RC unlinkAllLogFiles( SChar* aPathName );

    /************************************
    //PROJ-2133 incremental backup begin
    ************************************/
    static IDE_RC unlinkChangeTrackingFile( SChar * aChangeTrackingFileName ); 

    static IDE_RC unlinkBackupInfoFile( SChar * aBackupInfoFileName );

    static smrBackupState getBackupState();

    static IDE_RC incrementalBackupDatabase( idvSQL            * aStatistics,
                                             void              * aTrans,    
                                             SChar             * aBackupDir,
                                             smiBackuplevel      aBackupLevel,
                                             UShort              aBackupType,
                                             SChar             * aBackupTag );

    static IDE_RC incrementalBackupTableSpace( idvSQL            * aStatistics,
                                               void              * aTrans,    
                                               scSpaceID           aSpaceID,
                                               SChar             * aBackupDir,
                                               smiBackuplevel      aBackupLevel,
                                               UShort              aBackupType,
                                               SChar             * aBackupTag,
                                               idBool              aCheckTagName );

    static IDE_RC incrementalBackupMemoryTBS( idvSQL     * aStatistics,
                                              smmTBSNode * aSpaceNode,
                                              SChar      * aBackupDir,
                                              smriBISlot * aCommonBackupInfo );

    static IDE_RC incrementalBackupDiskTBS( idvSQL            * aStatistics,
                                            sddTableSpaceNode * aSpaceNode,
                                            SChar             * aBackupDir,
                                            smriBISlot        * aCommonBackupInfo );

    static IDE_RC setBackupInfoAndPath(
                                    SChar            * aBackupDir, 
                                    smiBackupLevel     aBackupLevel,
                                    UShort             aBackupType,
                                    smriBIBackupTarget aBackupTarget,
                                    SChar            * aBackupTag,
                                    smriBISlot       * aCommonBackupInfo,
                                    SChar            * aIncrementalBackupPath,
                                    idBool             aCheckTagName );
    /************************************
    //PROJ-2133 incremental backup end
    ************************************/
                                        

private:


    // ONLINE BACKUP �÷��׿� �����ϰ��� �ϴ� ���°��� �����Ѵ�.
    static void setOnlineBackupStatusOR( UInt  aOR );

    // ONLINE BACKUP �÷��׿� �����ϰ��� �ϴ� ���°��� �����Ѵ�.
    static void setOnlineBackupStatusNOT( UInt aNOT );

    /* �¶��� ��� ������� ���� */
    static inline idBool isRemainSpace(SInt  sSystemErrno);

    static IDE_RC beginBackupDiskTBS( idvSQL            * aStatistics,
                                      sddTableSpaceNode * aSpaceNode );

    static inline IDE_RC lock( idvSQL* aStatistics )
    { return mMtxOnlineBackupStatus.lock( aStatistics ); }
    static inline IDE_RC unlock() { return mMtxOnlineBackupStatus.unlock(); }

    //For Read Online Backup Status
    static iduMutex         mMtxOnlineBackupStatus;
    static SChar            mOnlineBackupStatus[256];
    static smrBackupState   mOnlineBackupState;

    // PRJ-1548 User Memory Tablespace
    static UInt             mBeginBackupDiskTBSCount;
    static UInt             mBeginBackupMemTBSCount;

    // PROJ-2133 Incremental Backup
    static UInt             mBackupBISlotCnt;
    static SChar            mLastBackupTagName[ SMI_MAX_BACKUP_TAG_NAME_LEN ];
};


/***********************************************************************
 * Description : �ý��� ������ȣ �м� (Is enough space?)
 **********************************************************************/
inline idBool smrBackupMgr::isRemainSpace(SInt sSystemErrno)
{

    if ( sSystemErrno == ENOSPC)
    {
        return ID_FALSE;
    }
    else
    {
        return ID_TRUE;
    }

}

/***********************************************************************
 *
 * Description : ������ ������¸� ��ȯ�Ѵ�.
 *
 **********************************************************************/
inline smrBackupState smrBackupMgr::getBackupState()
{
    return mOnlineBackupState;
}

/***********************************************************************
 *
 * Description : ���̺����̽��� Backup �������� üũ�Ѵ�.
 *
 * aSpaceID  - [IN] ���̺����̽� ID
 *
 **********************************************************************/
inline idBool smrBackupMgr::isBackupingTBS( scSpaceID   aSpaceID )
{
    // ����������� ���¸� ���� üũ�Ѵ�.
    if ( (getBackupState() & SMR_BACKUP_DISKTBS)
         != SMR_BACKUP_DISKTBS )
    {
        return ID_FALSE;
    }

    // ���̺����̽��� ���¸� üũ�Ѵ�.
    return sctTableSpaceMgr::isBackupingTBS( aSpaceID );
}

#endif /* _O_SMR_BACKUP_MGR_H_ */

