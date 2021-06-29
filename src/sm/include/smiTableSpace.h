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
 * $Id: smiTableSpace.h 82075 2018-01-17 06:39:52Z jina.kim $
 **********************************************************************/

#ifndef _O_SMI_TABLE_SPACE_H_
# define _O_SMI_TABLE_SPACE_H_ 1

# include <smiDef.h>

class smiTrans;

class smiTableSpace
{
public:

    static IDE_RC createDiskTBS(idvSQL            * aStatistics,
                                smiTrans          * aTrans,
                                smiTableSpaceAttr * aTableSpaceAttr,
                                smiDataFileAttr  ** aDataFileAttrList,
                                UInt                aFileAttrCnt,
                                UInt                aExtPageCnt,
				                ULong               aExtSize);

    // �޸� Tablespace�� �����Ѵ�.
    static IDE_RC createMemoryTBS(smiTrans             * aTrans,
                                  SChar                * aName,
                                  UInt                   aAttrFlag,
                                  smiChkptPathAttrList * aChkptPathList,
                                  ULong                  aSplitFileSize,
                                  ULong                  aInitSize,
                                  idBool                 aIsAutoExtend,
                                  ULong                  aNextSize,
                                  ULong                  aMaxSize,
                                  idBool                 aIsOnline,
                                  scSpaceID            * aTBSID );

    /* PROJ-1594 Volatile TBS */
    static IDE_RC createVolatileTBS( smiTrans   * aTrans,
                                     SChar      * aName,
                                     UInt         aAttrFlag,
                                     ULong        aInitSize,
                                     idBool       aIsAutoExtend,
                                     ULong        aNextSize,
                                     ULong        aMaxSize,
                                     UInt         aState,
                                     scSpaceID  * aTBSID );

    // �޸�/��ũ Tablespace�� Drop�Ѵ�.
    static IDE_RC drop( idvSQL*      aStatistics,
                        smiTrans *   aTrans,
                        scSpaceID    aTblSpaceID,
                        smiTouchMode aTouchMode );



    // �޸�/��ũ TableSpace�� DISCARD�Ѵ�
    static IDE_RC alterDiscard( scSpaceID aTableSpaceID );


    // ALTER TABLESPACE ONLINE/OFFLINE�� ����
    static IDE_RC alterStatus(idvSQL     * aStatistics,
                              smiTrans   * aTrans,
                              scSpaceID    aID,
                              UInt         aStatus);

    // Tablespace�� Attribute Flag�� �����Ѵ�.
    static IDE_RC alterTBSAttrFlag(smiTrans  * aTrans,
                                   scSpaceID   aTableSpaceID,
                                   UInt        aAttrFlagMask,
                                   UInt        aAttrFlagValue );
    
    static IDE_RC addDataFile(idvSQL           * aStatistics,
                              smiTrans*          aTrans,
                              scSpaceID          aID,
                              smiDataFileAttr ** aDataFileAttr,
                              UInt               aDataFileAttrCnt);


    static IDE_RC alterDataFileAutoExtend( smiTrans * aTrans,
                                           scSpaceID  aSpaceID,
                                           SChar *    aFileName,
                                           idBool     aAutoExtend,
                                           ULong      aNextSize,
                                           ULong      aMaxSize,
                                           SChar *    aValidFileName);

    // alter tablespace tbs-name alter datafile datafile-name online/offline
    static IDE_RC alterDataFileOnLineMode(scSpaceID   aSpaceID,
                                          SChar *     aFileName,
                                          UInt        aOnline);

    static  IDE_RC resizeDataFile(idvSQL    * aStatistics,
                                  smiTrans  * aTrans,
                                  scSpaceID   aTblSpaceID,
                                  SChar     * aFileName,
                                  ULong       aSizeWanted,
                                  ULong     * aSizeChanged,
                                  SChar     * aValidFileName);

    // alter tablespace rename datafile
    static IDE_RC  renameDataFile(scSpaceID  aTblSpaceID,
                                  SChar *    aOldFileName,
                                  SChar *    aNewFileName);

    static IDE_RC  removeDataFile(smiTrans * aTrans,
                                  scSpaceID aTblSpaceID,
                                  SChar*    aDataFile,
                                  SChar*    aValidFileName);

    // BUG-29812
    // ������ getAbsPath�� Memory TBS������ ����ϵ��� ����
    // �̸� ���� TBS�� Memory�� Disk�� �����ϱ� ���ؼ� aTBSType�� �߰�
    static IDE_RC getAbsPath( SChar         * aRelName,
                              SChar         * aAbsName,
                              smiTBSLocation  aTBSLocation );

    static IDE_RC getAttrByID( scSpaceID           aTableSpaceID,
                               smiTableSpaceAttr * aTBSAttr );

    static IDE_RC getAttrByName( SChar             * aTableSpaceName,
                                 smiTableSpaceAttr * aTBSAttr );

    static IDE_RC existDataFile( scSpaceID         aTableSpaceID,
                                 SChar*            aDataFileName,
                                 idBool*           aExist);

    static IDE_RC existDataFile( SChar*            aDataFileName,
                                 idBool*           aExist);


    // fix BUG-13646
    static IDE_RC getExtentAnTotalPageCnt(scSpaceID  aTableSpaceID,
                                          UInt*      aExtentPageCount,
                                          ULong*     aTotalPageCount);

    // �ý��� ���̺����̽� ���� ��ȯ
    static idBool isSystemTableSpace( scSpaceID aSpaceID );


    static idBool isMemTableSpace( scSpaceID aTableSpaceID );
    static idBool isDiskTableSpace( scSpaceID  aTableSpaceID );
    static idBool isTempTableSpace( scSpaceID  aTableSpaceID );
    static idBool isVolatileTableSpace( scSpaceID aTableSpaceID );

    static idBool isMemTableSpaceType( smiTableSpaceType  aType );
    static idBool isVolatileTableSpaceType( smiTableSpaceType aType );
    static idBool isDiskTableSpaceType( smiTableSpaceType  aType );
    static idBool isTempTableSpaceType( smiTableSpaceType  aType );
    static idBool isDataTableSpaceType( smiTableSpaceType  aType );

    ////////////////////////////////////////////////////////////////////
    // PROJ-1548-M3 ALTER TABLESPACE
    ////////////////////////////////////////////////////////////////////
    //  ALTER TABLESPACE TBSNAME ADD CHECKPOINT PATH ... �� ����
    static IDE_RC alterMemoryTBSAddChkptPath( scSpaceID      aSpaceID,
                                              SChar        * aChkptPath );

    // ALTER TABLESPACE TBSNAME RENAME CHECKPOINT PATH ... �� ����
    static IDE_RC alterMemoryTBSRenameChkptPath( scSpaceID    aSpaceID,
                                                 SChar      * aOrgChkptPath,
                                                 SChar      * aNewChkptPath );


    //  ALTER TABLESPACE TBSNAME DROP CHECKPOINT PATH ... �� ����
    static IDE_RC alterMemoryTBSDropChkptPath( scSpaceID    aSpaceID,
                                               SChar      * aChkptPath );

    // ALTER TABLESPACE TBSNAME AUTOEXTEND ... �� �����Ѵ�
    static IDE_RC alterMemoryTBSAutoExtend(smiTrans*   aTrans,
                                           scSpaceID   aSpaceID,
                                           idBool      aAutoExtend,
                                           ULong       aNextSize,
                                           ULong       aMaxSize );

    /* PROJ-1594 Volatile TBS */
    static IDE_RC alterVolatileTBSAutoExtend(smiTrans*   aTrans,
                                             scSpaceID   aSpaceID,
                                             idBool      aAutoExtend,
                                             ULong       aNextSize,
                                             ULong       aMaxSize );

    ////////////////////////////////////////////////////////////////////////////
    // BUG-14897 sys_tbs_data�� ���� �߰��� user tablespace�� ������ �����մϴ�.
    ////////////////////////////////////////////////////////////////////////////
    // BUG-14897 - �Ӽ� SYS_DATA_TBS_EXTENT_SIZE �� ����.
    static ULong  getSysDataTBSExtentSize();

    // BUG-14897 - �Ӽ� SYS_DATA_FILE_INIT_SIZE �� ����.
    static ULong  getSysDataFileInitSize();

    // BUG-14897 - �Ӽ� SYS_DATA_FILE_MAX_SIZE �� ����.
    static ULong  getSysDataFileMaxSize();

    // BUG-14897 - �Ӽ� SYS_DATA_FILE_NEXT_SIZE �� ����.
    static ULong  getSysDataFileNextSize();

    // BUG-14897 - �Ӽ� SYS_UNDO_TBS_EXTENT_SIZE �� ����.
    static ULong  getSysUndoTBSExtentSize();

    // BUG-14897 - �Ӽ� SYS_UNDO_FILE_INIT_SIZE �� ����.
    static ULong  getSysUndoFileInitSize();

    // BUG-14897 - �Ӽ� SYS_UNDO_FILE_MAX_SIZE �� ����.
    static ULong  getSysUndoFileMaxSize();

    // BUG-14897 - �Ӽ� SYS_UNDO_FILE_NEXT_SIZE �� ����.
    static ULong  getSysUndoFileNextSize();

    // BUG-14897 - �Ӽ� SYS_TEMP_TBS_EXTENT_SIZE �� ����.
    static ULong  getSysTempTBSExtentSize();

    // BUG-14897 - �Ӽ� SYS_TEMP_FILE_INIT_SIZE �� ����.
    static ULong  getSysTempFileInitSize();

    // BUG-14897 - �Ӽ� SYS_TEMP_FILE_MAX_SIZE �� ����.
    static ULong  getSysTempFileMaxSize();

    // BUG-14897 - �Ӽ� SYS_TEMP_FILE_NEXT_SIZE �� ����.
    static ULong  getSysTempFileNextSize();

    // BUG-14897 - �Ӽ� USER_DATA_TBS_EXTENT_SIZE �� ����.
    static ULong  getUserDataTBSExtentSize();

    // BUG-14897 - �Ӽ� USER_DATA_FILE_INIT_SIZE �� ����.
    static ULong  getUserDataFileInitSize();

    // BUG-14897 - �Ӽ� USER_DATA_FILE_MAX_SIZE �� ����.
    static ULong  getUserDataFileMaxSize();

    // BUG-14897 - �Ӽ� USER_DATA_FILE_NEXT_SIZE �� ����.
    static ULong  getUserDataFileNextSize();

    // BUG-14897 - �Ӽ� USER_TEMP_TBS_EXTENT_SIZE �� ����.
    static ULong  getUserTempTBSExtentSize();

    // BUG-14897 - �Ӽ� USER_TEMP_FILE_INIT_SIZE �� ����.
    static ULong  getUserTempFileInitSize();

    // BUG-14897 - �Ӽ� USER_TEMP_FILE_MAX_SIZE �� ����.
    static ULong  getUserTempFileMaxSize();

    // BUG-14897 - �Ӽ� USER_TEMP_FILE_NEXT_SIZE �� ����.
    static ULong  getUserTempFileNextSize();

private:
    //OS limit,file header�� ����� ������ ũ�⸦ ������ ��.
    static ULong  getValidSize4Disk( ULong aSizeInBytes );

};

#endif /* _O_SMI_TABLE_SPACE_H_ */
