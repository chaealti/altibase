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
 * $Id: smcRecordUpdate.h 86509 2020-01-08 07:56:53Z et16 $
 **********************************************************************/

#ifndef _O_SMC_RECORD_UPDATE_H_
#define _O_SMC_RECORD_UPDATE_H_ 1

#include <smDef.h>

class smcRecordUpdate
{
public:
    /* DML Logging API */

    /* Insert(DML)�� ���� Log�� ����Ѵ�. */
    static IDE_RC writeInsertLog( void*             aTrans,
                                  smcTableHeader*   aHeader,
                                  SChar*            aFixedRow,
                                  UInt              aAfterImageSize,
                                  UShort            aVarColumnCnt,
                                  const smiColumn** aVarColumns,
                                  UInt              aLargeVarCnt,
                                  const smiColumn** aLargeVarColumn );
    
    /* Update Versioning�� ���� Log�� ����Ѵ�.*/
    static IDE_RC writeUpdateVersionLog( void                   * aTrans,
                                         smcTableHeader         * aHeader,
                                         const smiColumnList    * aColumnList,
                                         idBool                   aIsReplSenderSend,
                                         smOID                    aOldRowOID,
                                         SChar                  * aBFixedRow,
                                         SChar                  * aAFixedRow,
                                         idBool                   aIsLockRow,
                                         UInt                     aBImageSize,
                                         UInt                     aAImageSize,
                                         UShort                   aUnitedVarColCnt );
    
    /* Update Inplace�� ���� Log�� ����Ѵ�.*/
    static IDE_RC writeUpdateInplaceLog(void*                 aTrans,
                                        smcTableHeader*       aHeader,
                                        const SChar*          aRowPtr,
                                        const smiColumnList * aColumnList,
                                        const smiValue      * aValueList,
                                        SChar*                aRowPtrBuffer,
                                        ULong                 aModifyIdxBit );

    /* Remove Row�� ���� Log�� ����Ѵ�.*/
    static IDE_RC writeRemoveVersionLog( void            * aTrans,
                                         smcTableHeader  * aHeader,
                                         SChar           * aRow,
                                         ULong             aBfrNxt,
                                         ULong             aAftNxt,
                                         smcMakeLogFlagOpt aOpt,
                                         idBool          * aIsSetImpSvp);

    /* Variable Column Piece �÷��� ���� log�� ����Ѵ�. */
    static IDE_RC writeVCPieceFlagLog( void            * aTrans,
                                       smcTableHeader  * aHeader,
                                       UInt            * aOffset,
                                       UInt            * aLogSize,
                                       smOID             aVCPieceOID );

    /* get primary key size */
    static UInt getPrimaryKeySize( const smcTableHeader*    aHeader,
                                   const SChar*             aFixRow);

    /* DML(insert, update(mvcc, inplace), delete)�� Log�� Flag�� ����*/
    static IDE_RC  makeLogFlag(void                 *aTrans,
                               const smcTableHeader *aHeader,
                               smrUpdateType         aType,
                               smcMakeLogFlagOpt     aOpt,
                               UInt                 *aFlag);

    /* Update type:  SMR_SMC_PERS_INIT_FIXED_ROW                        */
    static IDE_RC redo_SMC_PERS_INIT_FIXED_ROW(smTID      /*aTID*/,
                                               scSpaceID  aSpaceID,
                                               scPageID   aPID,
                                               scOffset   aOffset,
                                               vULong     aData,
                                               SChar*     aAfterImage,
                                               SInt       aSize,
                                               UInt       aFlag);

    static IDE_RC undo_SMC_PERS_INIT_FIXED_ROW(smTID      aTID,
                                               scSpaceID  aSpaceID,
                                               scPageID   aPID,
                                               scOffset   aOffset,
                                               vULong     aData,
                                               SChar*     aBeforeImage,
                                               SInt       aSize,
                                               UInt       aFlag);

    /* Update type:  SMR_SMC_PERS_UPDATE_FIXED_ROW                     */
    static IDE_RC redo_undo_SMC_PERS_UPDATE_FIXED_ROW(smTID      /*aTID*/,
                                                      scSpaceID  aSpaceID,
                                                      scPageID   aPID,
                                                      scOffset   aOffset,
                                                      vULong     aData,
                                                      SChar     *aImage,
                                                      SInt       aSize,
                                                      UInt       aFlag);

    /* Update type:  SMR_SMC_PERS_UPDATE_FIXED_ROW_NEXT_FREE              */
    static IDE_RC redo_undo_SMC_PERS_UPDATE_FIXED_ROW_NEXT_FREE(smTID     /*aTID*/,
                                                                scSpaceID  aSpaceID,
                                                                scPageID   aPID,
                                                                scOffset   aOffset,
                                                                vULong     aData,
                                                                SChar  * /*aImage*/,
                                                                SInt     /*aSize*/,
                                                                UInt       aFlag);

    /* Update type:  SMR_SMC_PERS_UPDATE_FIXED_ROW_NEXT_VERSION                */
    /* redo - To Fix BUG-14639 */
    static IDE_RC redo_SMC_PERS_UPDATE_FIXED_ROW_NEXT_VERSION(smTID      aTID,
                                                              scSpaceID  aSpaceID,
                                                              scPageID   aPID,
                                                              scOffset   aOffset,
                                                              vULong     aData,
                                                              SChar     *aImage,
                                                              SInt       aSize,
                                                              UInt       aFlag);

    /* Update type:  SMR_SMC_PERS_UPDATE_FIXED_ROW_NEXT_VERSION                */
    /* undo - To Fix BUG-14639 */
    static IDE_RC undo_SMC_PERS_UPDATE_FIXED_ROW_NEXT_VERSION(smTID      aTID,
                                                              scSpaceID  aSpaceID,
                                                              scPageID   aPID,
                                                              scOffset   aOffset,
                                                              vULong     aData,
                                                              SChar     *aImage,
                                                              SInt       aSize,
                                                              UInt       aFlag);
    /* Update Type: SMR_SMC_PERS_SET_FIX_ROW_DROP_FLAG                */
    static IDE_RC redo_SMC_PERS_SET_FIX_ROW_DROP_FLAG(smTID       /*aTID*/,
                                                      scSpaceID  aSpaceID,
                                                      scPageID    aPID,
                                                      scOffset    aOffset,
                                                      vULong      aData,
                                                      SChar   * /*aImage*/,
                                                      SInt      /*aSize*/,
                                                      UInt       aFlag);
    
    static IDE_RC undo_SMC_PERS_SET_FIX_ROW_DROP_FLAG(smTID       /*aTID*/,
                                                      scSpaceID  aSpaceID,
                                                      scPageID    aPID,
                                                      scOffset    aOffset,
                                                      vULong      aData,
                                                      SChar   * /*aImage*/,
                                                      SInt      /*aSize*/,
                                                      UInt       aFlag);

    /* Update Type: SMR_SMC_PERS_SET_FIX_ROW_DELETE_BIT                */
    static IDE_RC redo_SMC_PERS_SET_FIX_ROW_DELETE_BIT(smTID      /*aTID*/,
                                                       scSpaceID  aSpaceID,
                                                       scPageID   aPID,
                                                       scOffset   aOffset,
                                                       vULong     aData,
                                                       SChar     *aImage,
                                                       SInt       aSize,
                                                       UInt       aFlag);

    static IDE_RC undo_SMC_PERS_SET_FIX_ROW_DELETE_BIT(smTID      /*aTID*/,
                                                       scSpaceID  aSpaceID,
                                                       scPageID   aPID,
                                                       scOffset   aOffset,
                                                       vULong     aData,
                                                       SChar     *aImage,
                                                       SInt       aSize,
                                                       UInt       aFlag);

   /* Update type:  SMR_SMC_PERS_UPDATE_VAR_ROW                      */
    static IDE_RC redo_undo_SMC_PERS_UPDATE_VAR_ROW_HEAD(smTID   /*aTID*/,
                                                         scSpaceID  aSpaceID,
                                                         scPageID   aPID,
                                                         scOffset   aOffset,
                                                         vULong     aData,
                                                         SChar     *aImage,
                                                         SInt       aSize,
                                                         UInt       aFlag);

    /* Update type:  SMR_SMC_PERS_UPDATE_VAR_ROW                 */
    static IDE_RC redo_undo_SMC_PERS_UPDATE_VAR_ROW(smTID      /*aTID*/,
                                                    scSpaceID  aSpaceID,
                                                    scPageID   aPID,
                                                    scOffset   aOffset,
                                                    vULong     /*aData*/,
                                                    SChar     *aImage,
                                                    SInt       aSize,
                                                    UInt       aFlag);

    /* Update type:  SMR_SMC_PERS_SET_VAR_ROW_FLAG              */
    static IDE_RC redo_SMC_PERS_SET_VAR_ROW_FLAG(smTID      aTID,
                                                 scSpaceID  aSpaceID,
                                                 scPageID   aPID,
                                                 scOffset   aOffset,
                                                 vULong     aData,
                                                 SChar     *aImage,
                                                 SInt       aSize,
                                                 UInt       aFlag);

    static IDE_RC undo_SMC_PERS_SET_VAR_ROW_FLAG(smTID      /*aTID*/,
                                                 scSpaceID  aSpaceID,
                                                 scPageID   aPID,
                                                 scOffset   aOffset,
                                                 vULong     aData,
                                                 SChar     *aImage,
                                                 SInt       aSize,
                                                 UInt       aFlag);

    /* Update type:  SMR_SMC_INDEX_SET_DROP_FLAG              */
    static IDE_RC redo_undo_SMC_INDEX_SET_DROP_FLAG(smTID     aTID,
                                                    scSpaceID aSpaceID,
                                                    scPageID  aPID,
                                                    scOffset  aOffset,
                                                    vULong   ,/* aData */
                                                    SChar   * aImage,
                                                    SInt      aSize,
                                                    UInt      /* aFlag */ );
    
    /* Update type:  SMR_SMC_PERS_SET_VAR_ROW_NXT_OID           */
    static IDE_RC redo_undo_SMC_PERS_SET_VAR_ROW_NXT_OID(smTID      aTID,
                                                         scSpaceID  aSpaceID,
                                                         scPageID   aPID,
                                                         scOffset   aOffset,
                                                         vULong     aData,
                                                         SChar     *aImage,
                                                         SInt       aSize,
                                                         UInt       aFlag);

    /* Update type:  SMR_SMC_PERS_INSERT_ROW */
    static IDE_RC redo_SMC_PERS_INSERT_ROW(smTID      aTID,
                                           scSpaceID  aSpaceID,
                                           scPageID   aPID,
                                           scOffset   aOffset,
                                           vULong     aData,
                                           SChar     *aImage,
                                           SInt       aSize,
                                           UInt       aFlag);
    
    static IDE_RC undo_SMC_PERS_INSERT_ROW(smTID       aTID,
                                           scSpaceID  aSpaceID,
                                           scPageID    aPID,
                                           scOffset    aOffset,
                                           vULong      aData,
                                           SChar   * /*aImage*/,
                                           SInt      /*aSize*/,
                                           UInt       aFlag);
        
    /* Update type:  SMR_SMC_PERS_UPDATE_INPLACE_ROW */
    static IDE_RC redo_SMC_PERS_UPDATE_INPLACE_ROW(smTID      aTID,
                                                   scSpaceID  aSpaceID,
                                                   scPageID   aPID,
                                                   scOffset   aOffset,
                                                   vULong     aData,
                                                   SChar     *aImage,
                                                   SInt       aSize,
                                                   UInt       aFlag);
        
    static IDE_RC undo_SMC_PERS_UPDATE_INPLACE_ROW(smTID      aTID,
                                                   scSpaceID  aSpaceID,
                                                   scPageID   aPID,
                                                   scOffset   aOffset,
                                                   vULong     aData,
                                                   SChar     *aImage,
                                                   SInt       aSize,
                                                   UInt       aFlag);

    /* Update type:  SMR_SMC_PERS_UPDATE_VERSION_ROW */
    static IDE_RC redo_SMC_PERS_UPDATE_VERSION_ROW(smTID      aTID,
                                                   scSpaceID  aSpaceID,
                                                   scPageID   aPID,
                                                   scOffset   aOffset,
                                                   vULong     aData,
                                                   SChar     *aImage,
                                                   SInt       aSize,
                                                   UInt       aFlag);

    static IDE_RC undo_SMC_PERS_UPDATE_VERSION_ROW(smTID      aTID,
                                                   scSpaceID  aSpaceID,
                                                   scPageID   aPID,
                                                   scOffset   aOffset,
                                                   vULong     aData,
                                                   SChar     *aImage,
                                                   SInt       aSize,
                                                   UInt       aFlag);

    /* Update type:  SMR_SMC_PERS_DELETE_VERSION_ROW */
    static IDE_RC redo_SMC_PERS_DELETE_VERSION_ROW(smTID      aTID,
                                                   scSpaceID  aSpaceID,
                                                   scPageID   aPID,
                                                   scOffset   aOffset,
                                                   vULong     aData,
                                                   SChar     *aImage,
                                                   SInt     /*aSize*/,
                                                   UInt       aFlag);

    static IDE_RC undo_SMC_PERS_DELETE_VERSION_ROW(smTID      aTID,
                                                   scSpaceID  aSpaceID,
                                                   scPageID   aPID,
                                                   scOffset   aOffset,
                                                   vULong     aData,
                                                   SChar     *aImage,
                                                   SInt     /*aSize*/,
                                                   UInt       aFlag);
    
    /* Primary Key Log��Ͻ� ���. */
    static IDE_RC writePrimaryKeyLog( void*                    aTrans,
                                      const smcTableHeader*    aHeader,
                                      const SChar*             aFixRow,
                                      const UInt               aPKSize,
                                      UInt                     aOffset);
    
    /* SVC ��⿡�� ����� �� �ֵ��� public���� �ٲ۴�. */
    /* aTable�� ��� Index�� ���� aRowID�� �ش��ϴ� Row�� ���� */
    static IDE_RC deleteRowFromTBIdx( scSpaceID aSpaceID,
                                      smOID     aTableOID,
                                      smOID     aRowID,
                                      ULong     aModifyIdxBit );

    /* aTable�� ��� Index�� aRowID�� �ش��ϴ� Row Insert */
    static IDE_RC insertRow2TBIdx(void*     aTrans,
                                  scSpaceID aSpaceID,
                                  smOID     aTableOID,
                                  smOID     aRowID,
                                  ULong     aModifyIdxBit);

    /*PROJ-2429 Dictionary based data compress for on-disk DB*/
    static IDE_RC writeSetSCNLog( void           * aTrans,
                                  smcTableHeader * aHeader,
                                  SChar          * aRow );

    static IDE_RC redo_SMC_SET_CREATE_SCN(smTID      /*aTID*/,
                                          scSpaceID    aSpaceID,
                                          scPageID     aPID,
                                          scOffset     aOffset,
                                          vULong       aData,
                                          SChar*     /*aAfterImage*/,
                                          SInt       /*aSize*/,
                                          UInt       /*aFlag*/);

    static inline UShort getUnitedVCSize( smVCPieceHeader   * aPiece);

private:

    /* MVCC�� Fixed Column�� ���� Update�� Fixe Column�� ����
       Log��Ͻ� ����Ѵ�. */
    static IDE_RC writeFCLog4MVCC( void              *aTrans,
                                   const smiColumn   *aColumn,
                                   UInt              *aLogOffset,
                                   void              *aValue,
                                   UInt               aLength);

    /* MVCC�� Variable Column�� ���� Update�� Log��Ͻ� ����Ѵ�. */
    static IDE_RC writeVCLog4MVCC( void              *aTrans,
                                   const smiColumn   *aColumn,
                                   smVCDesc          *aVCDesc,
                                   UInt              *aOffset,
                                   smcVCLogWrtOpt     aOption);

    /* Inplace Update�� �� Column�� Update�� ���� Log ��� */
    static IDE_RC writeUInplaceColumnLog( void              *aTrans,
                                          smcLogReplOpt      aIsReplSenderRead,
                                          const smiColumn   *aColumn,
                                          UInt              *aLogOffset,
                                          const SChar       *aValue,
                                          UInt               aLength,
                                          smcUILogWrtOpt     aOpt);

    /* Priamry Key Log��Ͻ� ���. */
    static IDE_RC writePrimaryKeyLog( void*                    aTrans,
                                      const smcTableHeader*    aHeader,
                                      const SChar*             aFixRow,
                                      UInt                     aOffset );
    
    /* Update Inplace�� ���� Log�� Header�� ����Ѵ�.*/
    static IDE_RC writeUIPLHdr2TxLBf(void                 * aTrans,
                                     const smcTableHeader * aHeader,
                                     smcLogReplOpt          aIsReplSenderSend,
                                     const SChar          * aFixedRow,
                                     SChar                * aRowBuffer,
                                     const smiColumnList  * aColumnList,
                                     const smiValue       * aValueList,
                                     UInt                 * aPrimaryKeySize);

    /* Update Inplace�� ���� Log�� Before Image�� ����Ѵ�.*/
    static IDE_RC writeUIPBfrLg2TxLBf(void                 * aTrans,
                                      smcLogReplOpt          aIsReplSenderRead,
                                      const SChar          * aFixedRow,
                                      smOID                  aAfterOID,
                                      const smiColumnList  * aColumnList,
                                      UInt                 * aLogOffset,
                                      UInt                   aUtdVarColCnt,
                                      ULong                  aModifyIdxBit);

    /* Update Inplace�� ���� Log�� After Image�� ����Ѵ�.*/
    static IDE_RC writeUIPAftLg2TxLBf(void                 * aTrans,
                                      smcLogReplOpt          aIsReplSenderRead,
                                      const SChar          * aLogBuffer,
                                      smOID                  aBeforeOID,
                                      const smiColumnList  * aColumnList,
                                      const smiValue       * aValueList,
                                      UInt                 * aLogOffset,
                                      UInt                   aUtdVarColCnt);

    /* LOB Column�� Update�� Dummy Before Image�� ����Ѵ�.*/
    static IDE_RC writeDummyBVCLog4Lob(void *aTrans,
                                       UInt aColumnID,
                                       UInt *aOffset);

    static IDE_RC writeVCValue4OutMode(void              *aTrans,
                                       const smiColumn   *aColumn,
                                       smVCDesc          *aVCDesc,
                                       UInt              *aOffset);    
};

/***********************************************************************
 * Description : United Variable Piece �� ���̸� ���Ѵ�
 *
 *              offset array �� end offset ���� ���� ��� ���̸� �����Ͽ� ���Ѵ�
 *
 * aPiece   - [in] ���̸� ���� piece header 
 *
 ***********************************************************************/
inline UShort smcRecordUpdate::getUnitedVCSize( smVCPieceHeader   * aPiece)
{
    return (*((UShort*)(aPiece + 1) + aPiece->colCount)) - ID_SIZEOF(smVCPieceHeader);
}

#endif
