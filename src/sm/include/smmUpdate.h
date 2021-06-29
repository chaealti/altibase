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
 * $Id: smmUpdate.h 82075 2018-01-17 06:39:52Z jina.kim $
 **********************************************************************/

#ifndef _O_SMM_UPDATE_H_
#define _O_SMM_UPDATE_H_ 1

#include <smm.h>

class smmUpdate
{
//For Operation
public:
    /* Update type:  SMR_PHYSICAL                                   */
    static IDE_RC redo_undo_PHYSICAL(smTID      /*a_tid*/,
                                     scSpaceID  a_spaceid,
                                     scPageID   a_pid,
                                     scOffset   a_offset,
                                     vULong     a_data,
                                     SChar     *a_pImage,
                                     SInt       a_nSize,
                                     UInt      /*aFlag*/);

    /* Update type:  SMR_SMM_MEMBASE_INFO                      */
    static IDE_RC redo_SMM_MEMBASE_INFO(smTID       /*a_tid*/,
                                        scSpaceID   a_spaceid,
                                        scPageID    a_pid,
                                        scOffset    a_offset,
                                        vULong      /*a_data*/,
                                        SChar     * a_pImage,
                                        SInt        a_nSize,
                                        UInt        /*aFlag*/);
    
    /* Update type:  SMR_SMM_MEMBASE_SET_SYSTEM_SCN                      */
    static IDE_RC redo_SMM_MEMBASE_SET_SYSTEM_SCN(smTID      /*a_tid*/,
                                                  scSpaceID  a_spaceid,
                                                  scPageID   a_pid,
                                                  scOffset   a_offset,
                                                  vULong     /*a_data*/,
                                                  SChar     *a_pImage,
                                                  SInt       a_nSize,
                                                  UInt      /*aFlag*/);
    
    /* Update type:  SMR_SMM_MEMBASE_ALLOC_PERS
     *
     *  Membase�� ����ȭ�� Free Page List�� ������ ���뿡 ���� REDO/UNDO �ǽ�
     *
     */
    static IDE_RC redo_undo_SMM_MEMBASE_ALLOC_PERS_LIST(
                      smTID      /*a_tid*/,
                      scSpaceID  a_spaceid,
                      scPageID   a_pid,
                      scOffset   a_offset,
                      vULong     /*a_data*/,
                      SChar     *a_pImage,
                      SInt       a_nSize,
                      UInt      /*aFlag*/);

    /* Update type:  SMR_SMM_MEMBASE_ALLOC_PAGECHUNK
     *
     *  Expand Chunk�� �Ҵ翡 ���� logical redo �ǽ� 
     */
    static IDE_RC redo_SMM_MEMBASE_ALLOC_EXPAND_CHUNK(
                      smTID      /*a_tid*/,
                      scSpaceID  a_spaceid,
                      scPageID   a_pid,
                      scOffset   a_offset,
                      vULong     /*a_data*/,
                      SChar     *a_pImage,
                      SInt       a_nSize,
                      UInt      /*aFlag*/);


    /* Update type : SMR_SMM_PERS_UPDATE_NEXT_FREE_PAGE_LINK
     *
     * Free Page List Info Page �� Free Page�� Next Free Page����� �Ϳ� ����
     * REDO / UNDO �ǽ�
     */
    static IDE_RC redo_undo_SMM_PERS_UPDATE_NEXT_FREE_PAGE_LINK(
                      smTID          /*a_tid*/,
                      scSpaceID      a_spaceid,
                      scPageID       a_pid,
                      scOffset       a_offset,
                      vULong         /*a_data*/,
                      SChar        * a_pImage,
                      SInt           /*a_nSize*/,
                      UInt           /*aFlag*/);

    // BUGBUG-1548 �ּ� �ް�
    static IDE_RC redo_SMM_UPDATE_MRDB_CREATE_TBS(
                      idvSQL*            aStatistics, 
                      void*              aTrans,
                      smLSN              aCurLSN,
                      scSpaceID          aSpaceID,
                      UInt               aFileID,
                      UInt               aValueSize ,
                      SChar*             aValuePtr ,
                      idBool             aIsRestart );
    
    static IDE_RC undo_SMM_UPDATE_MRDB_CREATE_TBS(
                      idvSQL*            aStatistics, 
                      void*              aTrans,
                      smLSN              aCurLSN,
                      scSpaceID          aSpaceID,
                      UInt               aFileID,
                      UInt               aValueSize ,
                      SChar*             aValuePtr ,
                      idBool             aIsRestart );
    
    static IDE_RC redo_SMM_UPDATE_MRDB_DROP_TBS(
                      idvSQL*            aStatistics, 
                      void*              aTrans,
                      smLSN              aCurLSN,
                      scSpaceID          aSpaceID,
                      UInt               aFileID,
                      UInt               aValueSize ,
                      SChar*             aValuePtr ,
                      idBool             aIsRestart );
    
    static IDE_RC undo_SMM_UPDATE_MRDB_DROP_TBS(
                      idvSQL*            aStatistics, 
                      void*              aTrans,
                      smLSN              aCurLSN,
                      scSpaceID          aSpaceID,
                      UInt               aFileID,
                      UInt               aValueSize ,
                      SChar*             aValuePtr ,
                      idBool             aIsRestart );


    // ALTER TABLESPACE TBS1 AUTOEXTEND .... �� ���� REDO ����
    static IDE_RC redo_SCT_UPDATE_MRDB_ALTER_AUTOEXTEND(
                       idvSQL*              aStatistics, 
                       void*                aTrans,
                       smLSN                aCurLSN,
                       scSpaceID            aSpaceID,
                       UInt                 /*aFileID*/,
                       UInt                 aValueSize,
                       SChar*               aValuePtr,
                       idBool               /* aIsRestart */ );


    // ALTER TABLESPACE TBS1 AUTOEXTEND .... �� ���� REDO ����
    static IDE_RC undo_SCT_UPDATE_MRDB_ALTER_AUTOEXTEND(
                       idvSQL*              aStatistics, 
                       void*                aTrans,
                       smLSN                aCurLSN,
                       scSpaceID            aSpaceID,
                       UInt                 /*aFileID*/,
                       UInt                 aValueSize,
                       SChar*               aValuePtr,
                       idBool               aIsRestart );


    // Create Tablespace���� Checkpoint Image File�� ������ ���� Redo
    static IDE_RC redo_SMM_UPDATE_MRDB_CREATE_CIMAGE_FILE(
                      idvSQL*              aStatistics, 
                      void*              /* aTrans */,
                      smLSN                aCurLSN,
                      scSpaceID          /* aSpaceID */,
                      UInt               /* aFileID */,
                      UInt               /* aValueSize */,
                      SChar*             /* aValuePtr */,
                      idBool             /* aIsRestart */  );
    

    // Create Tablespace���� Checkpoint Image File�� ������ ���� Undo
    static IDE_RC undo_SMM_UPDATE_MRDB_CREATE_CIMAGE_FILE(
                      idvSQL*            aStatistics, 
                      void*              aTrans,
                      smLSN              aCurLSN,
                      scSpaceID          aSpaceID,
                      UInt               /* aFileID */,
                      UInt               aValueSize,
                      SChar*             aValuePtr,
                      idBool             /* aIsRestart */  );
    
    static IDE_RC redo4MR_SMM_MEMBASE_ALLOC_EXPAND_CHUNK(
                       scSpaceID     a_spaceid,
                       scPageID      a_pid,
                       scOffset      a_offset,
                       SChar        *a_pImage );

    
private:
    // ALTER TABLESPACE TBS1 AUTOEXTEND .... �� ���� Log Image�� �м��Ѵ�.
    static IDE_RC getAlterAutoExtendImage( UInt       aValueSize,
                                           SChar    * aValuePtr,
                                           idBool   * aAutoExtMode,
                                           scPageID * aNextPageCount,
                                           scPageID * aMaxPageCount );
    
    
};

#endif
