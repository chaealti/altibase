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
 *
 * - Backup Info Manager
 *
 **********************************************************************/

#ifndef _O_SMRI_BI_MANAGER_H_
#define _O_SMRI_BI_MANAGER_H_ 1

#include <smiDef.h>
#include <smriDef.h>

class smriBackupInfoMgr
{
private:

    /*BI���� ���*/
    static smriBIFileHdr      mBIFileHdr;

    /*BI slot�޸� ������ ���� ������*/
    static smriBISlot       * mBISlotArea;

    static idBool             mIsBISlotAreaLoaded;

    /*BI����*/
    static iduFile            mFile;

    static iduMutex           mMutex;

private:

    /*BI���� ��� �ʱ�ȭ*/
    static void initBIFileHdr();

    static IDE_RC checkBIFileHdrCheckSum();
    static void setBIFileHdrCheckSum();

    static IDE_RC checkBISlotCheckSum( smriBISlot *aBISlot );
    static void setBISlotCheckSum( smriBISlot * aBISlot );

    static IDE_RC processRestBackupFile( SChar    * aBackupPath,
                                         SChar    * aDescPath,
                                         iduFile  * aFile,
                                         idBool     aIsRemove );

    inline static UInt convertDayToSec( UInt aBackupInfoRetentionPeriod )
    {   return aBackupInfoRetentionPeriod * 60 * 60 * 24; };

    static  IDE_RC moveFile( SChar   * aSrcFilePath,        
                             SChar   * aDestFilePath,       
                             SChar   * aFileName,           
                             iduFile * aFile );
public:
    
    static IDE_RC initializeStatic();
    static IDE_RC destroyStatic();

    static IDE_RC createBIFile();
    static IDE_RC removeBIFile();

    static IDE_RC begin();
    static IDE_RC end();    

    static IDE_RC loadBISlotArea();
    static IDE_RC unloadBISlotArea();

    /*backup file����*/
    static IDE_RC removeBackupFile( SChar * aBackupFileName );

    /*BI���� flush*/
    static IDE_RC flushBIFile( UInt aValidSlotIdx, smLSN * aLastBackupLSN );
    
    /*time��� �ҿ��� �����϶� �ش� �ð��� �ش��ϴ� BISlotIdx�� ���Ѵ�.*/
    static IDE_RC findBISlotIdxUsingTime( UInt    UntilaTime, 
                                          UInt  * aTargetBackupTagSlotIdx );

    /*�����ؾ��� BISlotIdx�� ���Ѵ�.*/
    static IDE_RC getRestoreTargetSlotIdx( SChar            * aUntilBackupTag,
                                           UInt               aStartScanBISlotIdx,
                                           idBool             aSearchUntilBackupTag,
                                           smriBIBackupTarget aBackupTarget,
                                           scSpaceID          aSpaceID,
                                           UInt             * aRestoreSlotIdx );

    /*Level1 backup file ������ ������ BISlot�� ������ ���Ѵ�.*/
    static IDE_RC calcRestoreBISlotRange4Level1( 
                                         UInt            aScanStartSlotIdx,
                                         smiRestoreType  aRestoreType,
                                         UInt            aUltilTime,
                                         SChar         * aUntilBackupTag,
                                         UInt          * aRestoreStartSlotIdx,
                                         UInt          * aRestoreEndSlotIdx );

    /*BI������ ����Ѵ�.*/
    static IDE_RC backup( SChar * aBackupPath );

    /*�� BIslot�� BI���Ͽ� �߰��Ѵ�.*/
    static IDE_RC appendBISlot( smriBISlot * aBISlot );

    /*�Ⱓ�� ���� BIslot�� ����*/
    static IDE_RC removeObsoleteBISlots();

    /*�Ⱓ�� ���������� ù��° BISlot�� Idx�� �����´�.*/
    static IDE_RC getValidBISlotIdx( UInt * aValidSlotIdx );

    /* incremental backup ������ ������ ��ġ�� �̵���Ų��. */
    static IDE_RC moveIncrementalBackupFiles( SChar * aMovePath, 
                                              idBool  aWithFile );

    /*BISlotIdx�� �ش��ϴ� BISlot�� �����´�.*/
    static IDE_RC getBISlot( UInt aBISlotIdx, smriBISlot ** aBISlot );

    /*BI�� backup���� ����� �����Ѵ�.*/
    static void setBI2BackupFileHdr( smriBISlot * aBackupFileHdrBISlot, 
                                     smriBISlot * aBISlot );

    /*������ ���������� ����� �����ϴ� BI������ �����Ѵ�.*/
    static void clearDataFileHdrBI( smriBISlot * aDataFileHdrBI );

    /*BI������ ����� �����´�.*/
    static IDE_RC getBIFileHdr( smriBIFileHdr ** aBIFileHdr );

    /* pathName�� ��ȿ�� ���������� Ȯ���Ѵ�. */
    static IDE_RC isValidABSPath( idBool aCheckPerm, SChar  * aPathName );

    /* incremental backup�� �����Ұ�� backupinfo ������ �������  ���·�
     * rollbackup�Ѵ�. 
     */ 
    static IDE_RC rollbackBIFile( UInt aBISlotCnt );
    
    /* incremental backup�� ������ backupDir�� �����Ѵ�. */
    static IDE_RC removeBackupDir( SChar * aIncrementalBackupPath );

    static IDE_RC checkDBName( SChar * aDBName );

    static IDE_RC lock()
    { return mMutex.lock(NULL); }

    static IDE_RC unlock()
    { return mMutex.unlock(); }

};

#endif //_O_SMRI_BI_MANAGER_H_
