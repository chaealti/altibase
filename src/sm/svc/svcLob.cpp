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
 **********************************************************************/

#include <svcLob.h>
#include <svcReq.h>
#include <smr.h>
#include <smErrorCode.h>
#include <smcLobUpdate.h>
#include <svcRecord.h>

smLobModule svcLobModule =
{
    svcLob::open,
    svcLob::read,
    svcLob::write,
    svcLob::erase,
    svcLob::trim,
    svcLob::prepare4Write,
    svcLob::finishWrite,
    svcLob::getLobInfo,
    smcLobUpdate::writeLog4CursorOpen,
    svcLob::close
};

smcLobStatistics svcLob::mMemLobStatistics;

IDE_RC svcLob::open()
{
    mMemLobStatistics.mOpen++;

    return IDE_SUCCESS;
}

IDE_RC svcLob::close( idvSQL*        aStatistics,
                      void         * aTrans,
                      smLobViewEnv * aLobViewEnv )
{
    ACP_UNUSED( aStatistics );
    ACP_UNUSED( aTrans );
    ACP_UNUSED( aLobViewEnv );

    mMemLobStatistics.mClose++;

    return IDE_SUCCESS;
}

void svcLob::initializeFixedTableArea()
{
    mMemLobStatistics.mOpen            = 0;
    mMemLobStatistics.mRead            = 0;
    mMemLobStatistics.mWrite           = 0;
    mMemLobStatistics.mPrepareForWrite = 0;
    mMemLobStatistics.mFinishWrite     = 0;
    mMemLobStatistics.mGetLobInfo      = 0;
    mMemLobStatistics.mClose           = 0;
}

/**********************************************************************
 * lobCursor�� ����Ű�� �ִ� �޸� LOB ����Ÿ�� �о�´�.
 *
 * aTrans      [IN]  �۾��ϴ� Ʈ����� ��ü
 * aLobViewEnv [IN]  �۾��Ϸ��� LobViewEnv ��ü
 * aOffset     [IN]  �о������ Lob ����Ÿ�� ��ġ
 * aMount      [IN]  �о������ piece�� ũ��
 * aPiece      [OUT] ��ȯ�Ϸ��� Lob ����Ÿ piece ������
 **********************************************************************/
IDE_RC svcLob::read(idvSQL*        /*aStatistics */,
                    void*          aTrans,
                    smLobViewEnv*  aLobViewEnv,
                    UInt           aOffset,
                    UInt           aMount,
                    UChar*         aPiece,
                    UInt*          aReadLength)
{
    smcLobDesc*    sLobDesc;
    smcLPCH*       sTargetLPCH;
    UShort         sOffset;
    UShort         sReadableSize;
    UInt           sReadSize = 0;
    SChar*         sCurFixedRowPtr;

    IDE_DASSERT( aTrans != NULL );
    IDE_DASSERT( aLobViewEnv != NULL );
    IDE_DASSERT( aPiece != NULL );

    if(aMount > 0)
    {
        // fixed row�� ���� ������ �����Ѵ�.
        IDE_TEST(getViewRowPtr(aTrans, aLobViewEnv, &sCurFixedRowPtr)
                 != IDE_SUCCESS);

        // ��� lob data�� �д´�.
        sLobDesc = (smcLobDesc*)(sCurFixedRowPtr + aLobViewEnv->mLobCol.offset);

        IDE_TEST_RAISE( aOffset >= sLobDesc->length, range_error );

        if( (aOffset + aMount) > sLobDesc->length )
        {
            aMount = sLobDesc->length - aOffset;
        }
        
        *aReadLength = aMount;

        if ( SM_VCDESC_IS_MODE_IN( sLobDesc ) )
        {
            idlOS::memcpy(aPiece,
                          (UChar*)sLobDesc + ID_SIZEOF(smVCDescInMode) + aOffset,
                          aMount);
        }
        else
        {
            sTargetLPCH = findPosition(sLobDesc, aOffset);

            sOffset = aOffset % SMP_VC_PIECE_MAX_SIZE;

            while( sReadSize < aMount )
            {
                sReadableSize = SMP_VC_PIECE_MAX_SIZE - sOffset;
                if( sReadableSize > (aMount - sReadSize) )
                {
                    sReadableSize = (aMount - sReadSize);
                }

                IDE_ERROR( sTargetLPCH->mOID != 0 );
                
                readPiece( sTargetLPCH,
                           sOffset,
                           sReadableSize,
                           aPiece + sReadSize );

                sReadSize += sReadableSize;
                sTargetLPCH++;
                sOffset = 0;
            }
        }
    }

    mMemLobStatistics.mRead++;

    return IDE_SUCCESS;

    IDE_EXCEPTION(range_error);
    {
        // BUG-29212 ��ũ Lob�� read�� range check�� ���� �ʽ��ϴ�.
        // Read�ÿ��� Start Offset�� Ȯ���մϴ�.
        // End Offset�� LobLength�� �Ѿ����
        // ���� �� �ִ� �κи� �а� ���� Read Size�� ��ȯ�մϴ�.
        // �����޽����� Range�������� Offset������ �����մϴ�.
        IDE_SET(ideSetErrorCode( smERR_ABORT_InvalidLobStartOffset,
                                 aOffset,
                                 sLobDesc->length));
    }

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**********************************************************************
 * ���� �Ҵ�� ������ lob ����Ÿ�� ����Ѵ�.
 *
 * aTrans      [IN] �۾��ϴ� Ʈ����� ��ü
 * aLobViewEnv [IN] �۾��Ϸ��� LobViewEnv ��ü
 * aLobLocator [IN] �۾��Ϸ��� Lob Locator
 * aOffset     [IN] �۾��Ϸ��� Lob ����Ÿ�� ��ġ
 * aPieceLen   [IN] ���� �ԷµǴ� piece�� ũ��
 * aPiece      [IN] ���� �ԷµǴ� lob ����Ÿ piece
 **********************************************************************/
IDE_RC svcLob::write( idvSQL        * /* aStatistics */,
                      void          * aTrans,
                      smLobViewEnv  * aLobViewEnv,
                      smLobLocator    /* aLobLocator */,
                      UInt            aOffset,
                      UInt            aPieceLen,
                      UChar         * aPiece,
                      idBool          /* aIsFromAPI */,
                      UInt            /* aContType*/ )
{
    smSCN          sSCN;
    smTID          sTID;
    smpSlotHeader* sCurFixedSlotHeader;

    IDE_DASSERT( aTrans != NULL );
    IDE_DASSERT( aLobViewEnv != NULL );
    IDE_DASSERT( aPiece != NULL );

    /* ������ Row�� �����Ѵ�. ������ Row�� ���� ������ Row�̴�. */
    sCurFixedSlotHeader = (smpSlotHeader*)aLobViewEnv->mRow;

    while( SMP_SLOT_HAS_VALID_NEXT_OID( sCurFixedSlotHeader ) )
    {
        IDE_ASSERT( smmManager::getOIDPtr( 
                        aLobViewEnv->mLobCol.colSpace,
                        SMP_SLOT_GET_NEXT_OID(sCurFixedSlotHeader),
                        (void**)&sCurFixedSlotHeader )
                    == IDE_SUCCESS );
    }

    // aTrans Ʈ�������  LobCursor�� memory row ��  mRow�� ���Ͽ�
    // record lock�� �� Ʈ��������� Ȯ���Ѵ�.
    SMX_GET_SCN_AND_TID( sCurFixedSlotHeader->mCreateSCN, sSCN, sTID );

    if ( smLayerCallback::getLogTypeFlagOfTrans( aTrans )
         == SMR_LOG_TYPE_NORMAL )
    {
        /* BUG-16003:
         * Sender�� �ϳ��� Row�� ���ؼ� ���� Table Cursor��
         * LOB Cursor�� �ΰ� ���� �ΰ��� LOB Cursor�� ����
         * Infinite SCN�� ������. ������ Receiver�ܿ�����
         * ���� �ٸ� Table Cursor�� LOB Cursor�� �ΰ�������
         * �Ǿ �ٸ� Infinite SCN�� ������ �Ǿ� Sender����
         * ������ Prepare�� Receiver�ܿ����� Too Old������
         * �߻��Ѵ�. �� ������ �����ϱ� ���� Normal Transaction
         * �� ��쿡�� �Ʒ� üũ�� �����Ѵ�. Replication�� ������
         * ������ ��쿡�� Log�� ��ϵǱ� ������ �Ʒ� Validate��
         * �����ص� �ȴ�.*/

        IDE_ASSERT( SM_SCN_IS_EQ(&sSCN, &(aLobViewEnv->mInfinite)));
    }

    if( sTID != SM_NULL_TID )
    {
        IDE_ASSERT( sTID == smLayerCallback::getTransID( aTrans ) );
    }

    /*
     * reserve space
     */
    
    IDE_TEST( reserveSpace(aTrans,
                           aLobViewEnv,
                           (SChar*)sCurFixedSlotHeader,
                           aOffset,
                           aPieceLen)
              != IDE_SUCCESS );

    IDE_TEST( writeInternal( (UChar*)sCurFixedSlotHeader,
                             &aLobViewEnv->mLobCol,
                             aOffset,
                             aPieceLen,
                             aPiece )
              != IDE_SUCCESS );

    aLobViewEnv->mWriteOffset += aPieceLen;

    mMemLobStatistics.mWrite++;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC svcLob::erase( idvSQL       * aStatistics,
                      void         * aTrans,
                      smLobViewEnv * aLobViewEnv,
                      smLobLocator   /* aLobLocator */,
                      UInt           aOffset,
                      UInt           aPieceLen )
{
    SChar*      sFixedRowPtr = NULL;
    smcLobDesc* sCurLobDesc  = NULL;
    SChar*      sLobInRowPtr = NULL;
    UChar       sTmpPiece[SMP_VC_PIECE_MAX_SIZE];
    UInt        sTmpPieceLen;
    UInt        sTmpOffset;
    UInt        sEraseLen;
    
    IDE_DASSERT( aTrans != NULL );
    IDE_DASSERT( aLobViewEnv != NULL );

    /*
     * to update LOB column
     */

    IDE_TEST( getLastVersion(aStatistics,
                             aTrans,
                             aLobViewEnv,
                             &sFixedRowPtr)
              != IDE_SUCCESS );

    /*
     * to set new LOB version
     */
    
    sLobInRowPtr = sFixedRowPtr + aLobViewEnv->mLobCol.offset;
    sCurLobDesc  = (smcLobDesc*)sLobInRowPtr;

    IDE_TEST_RAISE( (aOffset > sCurLobDesc->length), range_error );

    if( (sCurLobDesc->flag & SM_VCDESC_MODE_MASK) == SM_VCDESC_MODE_OUT )
    {
        // LobVersion �� Out Mode ������ ��ȿ. 
        aLobViewEnv->mLobVersion = sCurLobDesc->mLobVersion + 1;
        IDE_TEST_RAISE( aLobViewEnv->mLobVersion == ID_ULONG_MAX,
                        error_version_overflow );    
    }
    else
    {
        aLobViewEnv->mLobVersion = 1;
    }

    /*
     * to erase
     */
    sEraseLen = sCurLobDesc->length > (aOffset + aPieceLen) ?
        aPieceLen : (sCurLobDesc->length - aOffset);

    IDE_TEST( reserveSpace(aTrans,
                           aLobViewEnv,
                           sFixedRowPtr,
                           aOffset,
                           sEraseLen)
              != IDE_SUCCESS );

    if( aLobViewEnv->mLobCol.colType == SMI_LOB_COLUMN_TYPE_BLOB )
    {
        idlOS::memset( sTmpPiece, 0x00, ID_SIZEOF(sTmpPiece) );
    }
    else
    {
        idlOS::memset( sTmpPiece, ' ', ID_SIZEOF(sTmpPiece) );
    }

    sTmpOffset = aOffset;

    while( sEraseLen > 0 )
    {
        if( sEraseLen > ID_SIZEOF(sTmpPiece) )
        {
            sTmpPieceLen = ID_SIZEOF(sTmpPiece);
        }
        else
        {
            sTmpPieceLen = sEraseLen;
        }
        
        IDE_TEST( writeInternal( (UChar*)sFixedRowPtr,
                                 &aLobViewEnv->mLobCol,
                                 sTmpOffset,
                                 sTmpPieceLen,
                                 sTmpPiece )
                  != IDE_SUCCESS );

        sEraseLen  -= sTmpPieceLen;
        sTmpOffset += sTmpPieceLen;
    }

    mMemLobStatistics.mErase++;

    return IDE_SUCCESS;

    IDE_EXCEPTION(range_error);
    {
        // BUG-29212 ��ũ Lob�� read�� range check�� ���� �ʽ��ϴ�.
        // range�� Offset���� Amount - 1 ���� �Դϴ�.
        IDE_SET( ideSetErrorCode(smERR_ABORT_RangeError,
                                 aOffset,
                                 (aOffset + aPieceLen - 1),
                                 0,
                                 (sCurLobDesc->length - 1)) );
    }
    IDE_EXCEPTION( error_version_overflow )
    {
        IDE_SET( ideSetErrorCode(smERR_ABORT_INTERNAL_ARG,
                                 "volatile erase:version overflow") );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**********************************************************************
 * ���� �Ҵ�� ������ lob ����Ÿ�� ����Ѵ�.
 *
 * aTrans            [IN] �۾��ϴ� Ʈ����� ��ü
 * aTable            [IN] �۾��ϴ� ���̺� ���
 * aFixedRowPtr      [IN] lob ����Ÿ�� ������ fixed row
 * aLobColumn        [IN] lob ����Ÿ�� ������ column ��ü
 * aOffset           [IN] lob ����Ÿ�� ���� ��ġ
 * aPieceLen         [IN] ���� �ԷµǴ� piece�� ũ��
 * aPiece            [IN] ���� �ԷµǴ� lob ����Ÿ piece
 * aIsWriteLog       [IN] �α� ����
 * aIsReplSenderSend [IN] replication �۵� ����
 * aLobLocator       [IN] lob locator ��ü
 **********************************************************************/
IDE_RC svcLob::writeInternal( UChar             * aFixedRowPtr,
                              const smiColumn   * aLobColumn,
                              UInt                aOffset,
                              UInt                aPieceLen,
                              UChar             * aPiece )
{
    smcLobDesc* sLobDesc;

    IDE_DASSERT( aFixedRowPtr != NULL );
    IDE_DASSERT( aLobColumn != NULL );
    IDE_DASSERT( aPiece != NULL );

    IDE_TEST_CONT( aPieceLen == 0, SKIP_WRITE );

    sLobDesc = (smcLobDesc*)(aFixedRowPtr + aLobColumn->offset);

    IDE_TEST_RAISE( (aOffset + aPieceLen) > sLobDesc->length,
                    range_error);

    if ( SM_VCDESC_IS_MODE_IN(sLobDesc) )
    {
        idlOS::memcpy( (aFixedRowPtr + aLobColumn->offset + ID_SIZEOF(smVCDescInMode) + aOffset),
                       aPiece,
                       aPieceLen );
    }
    else
    {
        IDE_TEST( write4OutMode( sLobDesc,
                                 aOffset,
                                 aPieceLen,
                                 aPiece )
                  != IDE_SUCCESS );
    }

    IDE_EXCEPTION_CONT( SKIP_WRITE );
    
    return IDE_SUCCESS;

    IDE_EXCEPTION(range_error);
    {
        // BUG-29212 ��ũ Lob�� read�� range check�� ���� �ʽ��ϴ�.
        // range�� Offset���� Amount - 1 ���� �Դϴ�.
        IDE_SET(ideSetErrorCode(smERR_ABORT_RangeError,
                                aOffset,
                                (aOffset + aPieceLen - 1),
                                0,
                                (sLobDesc->length - 1)));
    }

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**********************************************************************
 * OutMode�� aLobDesc�� Lob Piece�� lob ����Ÿ�� ����Ѵ�.
 *
 * aTrans            [IN] �۾��ϴ� Ʈ����� ��ü
 * aTable            [IN] �۾��ϴ� ���̺� ���
 * aLobDesc          [IN] lob ����Ÿ�� ������ LobDesc ��ü
 * aLobSpaceID       [IN] log ����Ÿ�� ������ SpaceID
 * aOffset           [IN] lob ����Ÿ�� ���� ��ġ
 * aPieceLen         [IN] ���� �ԷµǴ� piece�� ũ��
 * aPiece            [IN] ���� �ԷµǴ� lob ����Ÿ piece
 * aIsWriteLog       [IN] �α� ����
 * aIsReplSenderSend [IN] replication �۵� ����
 * aLobLocator       [IN] lob locator ��ü
 **********************************************************************/
IDE_RC svcLob::write4OutMode( smcLobDesc    * aLobDesc,
                              UInt            aOffset,
                              UInt            aPieceLen,
                              UChar         * aPiece )
{
    smcLPCH* sTargetLPCH;
    UShort   sOffset;
    UShort   sWritableSize;
    UInt     sWrittenSize = 0;

    IDE_DASSERT( aLobDesc != NULL );
    IDE_DASSERT( aPiece != NULL );
    IDE_DASSERT( aPieceLen > 0 );

    sTargetLPCH = findPosition(aLobDesc, aOffset);

    IDE_ASSERT( sTargetLPCH != NULL );

    sOffset = aOffset % SMP_VC_PIECE_MAX_SIZE;

    // write lob piece
    while(sWrittenSize < aPieceLen)
    {
        sWritableSize = SMP_VC_PIECE_MAX_SIZE - sOffset;
        if( sWritableSize > (aPieceLen - sWrittenSize) )
        {
            sWritableSize = aPieceLen - sWrittenSize;
        }

        // sOffset�� ���� Piece ����Ÿ�� ũ�⿡ ������ ��ġ��
        // Offset���� �����Ǿ��� ��� sWritableSize���� �ȴ�.
        if( sWritableSize != 0)
        {
            IDE_TEST( writePiece( sTargetLPCH,
                                  sOffset,
                                  sWritableSize,
                                  aPiece + sWrittenSize )
                      != IDE_SUCCESS );
        }

        sWrittenSize += sWritableSize;
        sTargetLPCH++;
        sOffset = 0;
    }

    IDE_ASSERT(sWrittenSize == aPieceLen);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**********************************************************************
 * lob write�ϱ� ���� new version�� ���� ���� Offset�� �����Ѵ�.
 *
 * aTrans      [IN] �۾��ϴ� Ʈ����� ��ü
 * aLobViewEnv [IN] �۾��Ϸ��� LobViewEnv ��ü
 * aLobLocator [IN] �۾��Ϸ��� Lob Locator
 * aOffset     [IN] �۾��Ϸ��� Lob ����Ÿ�� ��ġ
 * aNewSize    [IN] ���� �ԷµǴ� ����Ÿ�� ũ��
 **********************************************************************/
IDE_RC svcLob::prepare4Write(idvSQL*       aStatistics,
                             void*         aTrans,
                             smLobViewEnv* aLobViewEnv,
                             smLobLocator  /*aLobLocator*/,
                             UInt          aOffset,
                             UInt          aNewSize)
{
    SChar*         sNxtFixedRowPtr = NULL;
    smcLobDesc*    sCurLobDesc  = NULL;
    SChar*         sLobInRowPtr = NULL;
    
    IDE_DASSERT( aTrans != NULL );
    IDE_DASSERT( aLobViewEnv != NULL );

    /*
     * to update LOB column
     */

    IDE_TEST( getLastVersion(aStatistics,
                             aTrans,
                             aLobViewEnv,
                             &sNxtFixedRowPtr)
              != IDE_SUCCESS );

    /*
     * to set new LOB version
     */
    
    sLobInRowPtr = sNxtFixedRowPtr + aLobViewEnv->mLobCol.offset;
    sCurLobDesc  = (smcLobDesc*)sLobInRowPtr;

    IDE_TEST_RAISE( (aOffset > sCurLobDesc->length), range_error );

    /*
     * set offset
     */
    aLobViewEnv->mWriteOffset = aOffset;

    mMemLobStatistics.mPrepareForWrite++;

    return IDE_SUCCESS;

    IDE_EXCEPTION(range_error);
    {
        // BUG-29212 ��ũ Lob�� read�� range check�� ���� �ʽ��ϴ�.
        // range�� Offset���� Amount - 1 ���� �Դϴ�.
        IDE_SET(ideSetErrorCode(smERR_ABORT_RangeError,
                                aOffset,
                                (aOffset + aNewSize - 1),
                                0,
                                (sCurLobDesc->length - 1)));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : Write�� ����Ǿ���. Replication Log�� �����.
 *
 *    aStatistics - [IN] ��� ����
 *    aTrans      - [IN] Transaction
 *    aLobViewEnv - [IN] �ڽ��� ���� �� LOB������ ����
 *    aLobLocator - [IN] Lob Locator
 **********************************************************************/
IDE_RC svcLob::finishWrite( idvSQL       * /*aStatistics*/,
                            void         * /*aTrans*/,
                            smLobViewEnv * aLobViewEnv,
                            smLobLocator   /*aLobLocator*/ )
{
    IDE_ASSERT( aLobViewEnv->mOpenMode == SMI_LOB_READ_WRITE_MODE );

    mMemLobStatistics.mFinishWrite++;

    return IDE_SUCCESS;
}

/**********************************************************************
 * ���������� prepare4WriteInternal�� ȣ���ϰ� ���ŵ� LobDesc�� ����
 * �α׸� ����Ѵ�. �̴� prepare4Write������ prepare4WriteInternalȣ���Ŀ�
 * ���ŵ� ������ ���ؼ� �α׸� ��������� smiTableBackup::restore������
 * prepare4WriteInternal�� �ٷ� ȣ���ϱ⶧���� �α��� �������ϴ� �������̽���
 * �ʿ��ϴ�.
 *
 * aTrans      [IN] �۾��ϴ� Ʈ����� ��ü
 * aTable      [IN] �۾��Ϸ��� ���̺� ���
 * aLobColumn  [IN] Lob Column Desc����
 * aLobDesc    [IN] �۾��Ϸ��� Lob Description
 * aOffset     [IN] �۾��Ϸ��� Lob ����Ÿ�� ��ġ
 * aNewSize    [IN] ���� �ԷµǴ� ����Ÿ�� ũ��
 * aAddOIDFlag [IN] OID LIST�� �߰����� ����
 **********************************************************************/
IDE_RC svcLob::reserveSpaceInternalAndLogging(
                                            void*               aTrans,
                                            smcTableHeader*     aTable,
                                            SChar*              aRow,
                                            const smiColumn*    aLobColumn,
                                            UInt                aOffset,
                                            UInt                aNewSize,
                                            UInt                aAddOIDFlag )
{
    ULong       sLobColBuf[ SMC_LOB_MAX_IN_ROW_STORE_SIZE/ID_SIZEOF(ULong) ];
    /* BUG-30414  LobColumn ����� Stack�� Buffer��
       align ���� �ʾ� sigbus�� �Ͼ�ϴ�. */
    SChar     * sLobInRowPtr;
    smcLobDesc* sNewLobDesc;
    smcLobDesc* sCurLobDesc;

    sLobInRowPtr = aRow + aLobColumn->offset;
    sCurLobDesc  = (smcLobDesc*)sLobInRowPtr;

    // old image�� ����
    if ( SM_VCDESC_IS_MODE_IN(sCurLobDesc) )
    {
        // BUG-30101 ����Ÿ�� In-Mode�� ����Ǿ��� ��� ����Ÿ��
        // Fixed������ ����Ǿ��ִ�. Lob Column Desc �� �Բ� ���� �����صд�
        idlOS::memcpy( sLobColBuf,
                       sLobInRowPtr,
                       sCurLobDesc->length + ID_SIZEOF(smVCDescInMode));
    }
    else
    {
        // Out Mode�� ���� �Ǿ� ������� LOB Desc�� ����
        idlOS::memcpy( sLobColBuf, sLobInRowPtr, ID_SIZEOF(smcLobDesc) );
    }

    // BUG-30036 Memory LOB�� ODBC�� Insert �ϴ� �����Ͽ��� ��,
    // �����ϴ� LOB Desc�� ������ ä�� Rollback���� �ʰ� �ֽ��ϴ�. �� ���Ͽ�
    // ������ Dummy Lob Desc�� Prepare �ϰ� Log�� ���� ��� �� �Ŀ�
    // ����� LOB Desc�� Data Page�� �ݿ��մϴ�.
    IDE_TEST( reserveSpaceInternal(aTrans,
                                   aTable,
                                   aLobColumn,
                                   sCurLobDesc->mLobVersion + 1,
                                   (smcLobDesc*)sLobColBuf,
                                   aOffset,
                                   aNewSize,
                                   aAddOIDFlag,
                                   ID_TRUE) /* aIsFullWrite */
              != IDE_SUCCESS );

    sNewLobDesc = (smcLobDesc*)sLobColBuf;

    // BUG-30036 ����� Lob Desc�� Data Page�� �ݿ�
    if ( SM_VCDESC_IS_MODE_IN(sNewLobDesc) )
    {
        /* Lob Desc�� �����Ѵ�. */
        idlOS::memcpy( sLobInRowPtr,
                       sLobColBuf,
                       ID_SIZEOF(smVCDescInMode) );
    }
    else
    {
        // Out Mode�� ���� �� ��� Lob Desc�� �����Ѵ�.
        idlOS::memcpy( sLobInRowPtr,
                       sLobColBuf,
                       ID_SIZEOF(smcLobDesc) );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**********************************************************************
 * lob write�ϱ� ���� new version�� ���� ���� �Ҵ��ϴ� ���� �۾��� �Ѵ�.
 *
 * old [LobDesc]--[LPCH#1][LPCH#2][LPCH#3][LPCH#4][LPCH#5]
 *                  | |      |       |        |       | |
 *           [piece#1]| [piece#2] [piece#3] [piece#4] |[piece#5]
 *                  | |    |                   |      | |
 *                  | |    V                   V      | |
 *                  | |[piece#2'][piece#3'][piece#4'] | |
 *                  | V      |       |          |     V |
 * new [LobDesc]--[LPCH#1'][LPCH#2'][LPCH#3'][LPCH#4'][LPCH#5']
 *
 * aOffset�� piece#2���� �����ؼ� aOldSize�� piece#4���� �̰�,
 * aNewSize�� piece#4'���� �϶�, ������� �ʴ� piece#1,5�� LPCH��
 * ����(LPCH#1->#1', #5->#5')�Ͽ� piece#1,5�� �����ϰ� �ǰ�,
 * ����Ǵ� piece#2',#3',#4'�� ���� �Ҵ�޾� LPCH�� �����Ѵ�.
 * �̶�, ���� ���۰� �� piece#2,#4�� ���� ������ �ƴ� ���� ����
 * �Ҵ� ���� piece#2',#4'�� ������ �ش�.
 *
 * aTrans      [IN] �۾��ϴ� Ʈ����� ��ü
 * aTable      [IN] �۾��Ϸ��� ���̺� ���
 * aLobColumn  [IN]
 * aLobVersion
 * aLobDesc    [IN] �۾��Ϸ��� Lob Description
 * aOffset     [IN] �۾��Ϸ��� Lob ����Ÿ�� ��ġ
 * aNewSize    [IN] ���� �ԷµǴ� ����Ÿ�� ũ��
 * aAddOIDFlag [IN] OID LIST�� �߰����� ����
 * aIsFullWrite
 **********************************************************************/
IDE_RC svcLob::reserveSpaceInternal(void*               aTrans,
                                    smcTableHeader*     aTable,
                                    const smiColumn*    aLobColumn,
                                    ULong               aLobVersion,
                                    smcLobDesc*         aLobDesc,
                                    UInt                aOffset,
                                    UInt                aNewSize,
                                    UInt                aAddOIDFlag,
                                    idBool              aIsFullWrite)
{
    smcLobDesc* sOldLobDesc;
    UInt        sNewLobLen;
    UInt        sNewLPCHCnt;
    UInt        sBeginIdx;
    UInt        sEndIdx;
    ULong       sLobColumnBuffer[SM_PAGE_SIZE / ID_SIZEOF(ULong)];

    IDE_DASSERT( aTrans != NULL );
    IDE_DASSERT( aTable != NULL );
    IDE_DASSERT( aLobDesc != NULL );

    IDE_TEST_RAISE( (aOffset > aLobDesc->length), range_error );

    /*
     * to backup LOB Descriptor
     */
    
    sOldLobDesc = (smcLobDesc*)sLobColumnBuffer;

    if ( SM_VCDESC_IS_MODE_IN( aLobDesc ) )
    {
        /* ����Ÿ�� In-Mode�� ����Ǿ��� ��� ����Ÿ��
         * Fixed������ ����Ǿ��ִ�. Lob Column Desc �� �Բ� ���� �����صд� */
        idlOS::memcpy( (SChar*)sOldLobDesc,
                       (SChar*)aLobDesc,
                       ID_SIZEOF(smVCDescInMode) + aLobDesc->length );
    }
    else
    {
        idlOS::memcpy( (SChar*)sOldLobDesc,
                       (SChar*)aLobDesc,
                       ID_SIZEOF(smcLobDesc) );
    }
    
    /*
     * to set LOB mode & length
     */

    if( aIsFullWrite == ID_TRUE )
    {
        IDE_ERROR( aOffset == 0 );
        
        sNewLobLen = aOffset + aNewSize;

        if( sNewLobLen <= aLobColumn->vcInOutBaseSize )
        {
            if ( ( sOldLobDesc->flag & SM_VCDESC_MODE_MASK ) == SM_VCDESC_MODE_OUT )
            {
                /* BUG-46666 : FULL WRITE����,
                               out -> in �϶�, new��(aLobDesc)�� �ʱ�ȭ�ؾ� old���� aging�ȴ�. */
                SMC_LOB_DESC_INIT( aLobDesc );
            }

            aLobDesc->flag = SM_VCDESC_MODE_IN;
        }
        else
        {
            aLobDesc->flag = SM_VCDESC_MODE_OUT;
        }
    }
    else
    {
        sNewLobLen = (aOffset + aNewSize) > sOldLobDesc->length ?
                                    (aOffset + aNewSize) : sOldLobDesc->length;

        if( sNewLobLen <= aLobColumn->vcInOutBaseSize )
        {
            if ( SM_VCDESC_IS_MODE_IN( sOldLobDesc ) )
            {
                aLobDesc->flag = SM_VCDESC_MODE_IN;
            }
            else
            {
                aLobDesc->flag = SM_VCDESC_MODE_OUT;
            }
        }
        else
        {
            aLobDesc->flag = SM_VCDESC_MODE_OUT;
        }
    }

    aLobDesc->length = sNewLobLen;

    /*
     * to allocate new version space
     */
    
    if( (aLobDesc->flag & SM_VCDESC_MODE_MASK) == SM_VCDESC_MODE_OUT )
    {
        aLobDesc->mLobVersion = aLobVersion;

        sNewLPCHCnt = getLPCHCntFromLength(sNewLobLen);

        IDE_TEST( allocLPCH(aTrans,
                            aTable,
                            aLobDesc,
                            sOldLobDesc,
                            aLobColumn->colSpace,
                            sNewLPCHCnt,
                            aAddOIDFlag,
                            aIsFullWrite)
                  != IDE_SUCCESS );

        IDE_TEST( allocPiece(aTrans,
                             aTable,
                             aLobVersion,
                             aLobDesc,
                             sOldLobDesc,
                             aLobColumn->colSpace,
                             aOffset,
                             aNewSize,
                             sNewLobLen,
                             aAddOIDFlag,
                             aIsFullWrite)
                  != IDE_SUCCESS );

        /* If the LOB mode is converted from in to out, the previsous
         * value must be copied to new piece. */
        if ( ( aIsFullWrite != ID_TRUE ) &&
             ( SM_VCDESC_IS_MODE_IN(sOldLobDesc) ) )
        {
            if( sOldLobDesc->length > 0 )
            {
                IDE_TEST( copyPiece( sOldLobDesc,    /* aSrcLobDesc */
                                     0,              /* aSrcOffset */
                                     aLobDesc,       /* aDstLobDesc */
                                     0,              /* aDstOffset */
                                     sOldLobDesc->length )
                          != IDE_SUCCESS );
            }
            else
            {
                /* nothing to do */
            }
        }
        else
        {
            /* nothing to do */
        }
    }
    else
    {
        /* nothing to do */
    }

    /*
     * to remove old version (LPCH & LOB Piece)
     */

    if( ((sOldLobDesc->flag & SM_VCDESC_MODE_MASK) == SM_VCDESC_MODE_OUT) &&
        (sOldLobDesc->mLPCHCount > 0) )
    {
        if( aIsFullWrite == ID_TRUE )
        {
            sBeginIdx = 0;
            sEndIdx = sOldLobDesc->mLPCHCount - 1;
        }
        else
        {
            sBeginIdx = aOffset / SMP_VC_PIECE_MAX_SIZE;

            if( (aOffset + aNewSize) <= sOldLobDesc->length )
            {
                IDE_ERROR( (aOffset + aNewSize) > 0);
                sEndIdx = (aOffset + aNewSize - 1) / SMP_VC_PIECE_MAX_SIZE;
            }
            else
            {
                sEndIdx = sOldLobDesc->mLPCHCount - 1;
            }
        }

        IDE_TEST( removeOldLPCH(aTrans,
                                aTable,
                                aLobColumn,
                                sOldLobDesc,
                                aLobDesc,
                                sBeginIdx,
                                sEndIdx)
                  != IDE_SUCCESS );
    }
    else
    {
        /* nothing to do */
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION(range_error);
    {
        // BUG-29212 ��ũ Lob�� read�� range check�� ���� �ʽ��ϴ�.
        // range�� Offset���� Amount - 1 ���� �Դϴ�.
        IDE_SET(ideSetErrorCode(smERR_ABORT_RangeError,
                                aOffset,
                                (aOffset + aNewSize - 1),
                                0,
                                (aLobDesc->length - 1)));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**********************************************************************
 * ���ο� lob piece�� �Ҵ��Ѵ�.
 *
 * aTrans         [IN] �۾��ϴ� Ʈ����� ��ü
 * aTable         [IN] �۾��Ϸ��� ���̺� ���
 * aLobSpaceID [IN] Lob Column�����Ͱ� ��ϵǴ� Tablespace�� ID
 * aFirstLPCH     [IN] ��ü lob �������� ���� LPCH
 * aStartLPCH     [IN] ���� �����ϴ� LPCH
 * aNxtPieceOID   [IN] ���� �Ҵ� ���� lob piece�� ���� �������� next oid
 * aNewSlotSize   [IN] ���� �Ҵ� ���� slot�� �� ������ slot�� ũ��
 * aAddOIDFlag    [IN] OID LIST�� �߰����� ����
 **********************************************************************/
IDE_RC svcLob::allocPiece(void*           aTrans,
                          smcTableHeader* aTable,
                          ULong           aLobVersion,
                          smcLobDesc*     aLobDesc,
                          smcLobDesc*     aOldLobDesc,
                          scSpaceID       aLobSpaceID,
                          UInt            aOffset,
                          UInt            aNewSize,
                          UInt            aNewLobLen,
                          UInt            aAddOIDFlag,
                          idBool          aIsFullWrite)
{
    UInt             i;
    UInt             sBeginIdx;
    UInt             sEndIdx;
    UInt             sLPCHCnt;
    UInt             sOldLPCHCnt;
    UInt             sPieceSize;
    smOID            sNxtPieceOID;
    smOID            sNewPieceOID = SM_NULL_OID;
    SChar*           sNewPiecePtr = NULL;
    smcLPCH*         sTargetLPCH;
    smVCPieceHeader* sCurVCPieceHeader;
    smVCPieceHeader* sSrcVCPieceHeader;

    IDE_ASSERT( aTrans   != NULL );
    IDE_ASSERT( aTable   != NULL );
    IDE_ASSERT( aLobDesc != NULL );
    IDE_ASSERT( aLobDesc->mFirstLPCH != NULL );
    IDE_DASSERT( (aLobDesc->flag & SM_VCDESC_MODE_MASK) == SM_VCDESC_MODE_OUT );

    IDE_ERROR( (aOffset + aNewSize) > 0 );

    sBeginIdx = aOffset / SMP_VC_PIECE_MAX_SIZE;
    sEndIdx   = (aOffset + aNewSize - 1) / SMP_VC_PIECE_MAX_SIZE;
    
    sLPCHCnt  = aLobDesc->mLPCHCount;

    if( sLPCHCnt <= 1 )
    {
        IDE_ERROR( aNewLobLen <= SMP_VC_PIECE_MAX_SIZE );
        
        sPieceSize = aNewLobLen;
    }
    else
    {
        sPieceSize = SMP_VC_PIECE_MAX_SIZE;
    }    

    if( (aOldLobDesc->flag & SM_VCDESC_MODE_MASK) == SM_VCDESC_MODE_OUT )
    {
        sOldLPCHCnt = aOldLobDesc->mLPCHCount;
    }
    else
    {
        sOldLPCHCnt = 0;
    }

    IDE_ERROR( sBeginIdx < aLobDesc->mLPCHCount );
    IDE_ERROR( sEndIdx   < aLobDesc->mLPCHCount );

    /*
     * to allocate piece.
     */
    
    for( i = sEndIdx; i >= sBeginIdx; i-- )
    {
        if( (i + 1) < sLPCHCnt )
        {
            sNxtPieceOID = aLobDesc->mFirstLPCH[i+1].mOID;
        }
        else
        {
            sNxtPieceOID = SM_NULL_OID;
        }
        
        if( (i < sOldLPCHCnt) && (aIsFullWrite != ID_TRUE) )
        {
            sSrcVCPieceHeader = (smVCPieceHeader*)(aOldLobDesc->mFirstLPCH[i].mPtr);

            if( i > 0 )
            {
                IDE_ERROR( sSrcVCPieceHeader->length == SMP_VC_PIECE_MAX_SIZE );
            }
            else
            {
                IDE_ERROR( sSrcVCPieceHeader->length <= SMP_VC_PIECE_MAX_SIZE );
            }

            if( (aLobVersion != aOldLobDesc->mFirstLPCH[i].mLobVersion) ||
                (sSrcVCPieceHeader->length < SMP_VC_PIECE_MAX_SIZE) )            
            {
                IDE_TEST( svpVarPageList::allocSlot(aTrans,
                                                    aLobSpaceID,
                                                    aTable->mSelfOID,
                                                    aTable->mVar.mVRDB,
                                                    sPieceSize,
                                                    sNxtPieceOID,
                                                    &sNewPieceOID,
                                                    &sNewPiecePtr)
                          != IDE_SUCCESS );

                aLobDesc->mFirstLPCH[i].mLobVersion = aLobVersion;
                aLobDesc->mFirstLPCH[i].mOID        = sNewPieceOID;
                aLobDesc->mFirstLPCH[i].mPtr        = sNewPiecePtr;

                /* ���� �Ҵ��� lob piece�� ���Ͽ� version list �߰� */
                if ( SM_INSERT_ADD_OID_IS_OK(aAddOIDFlag) )
                {
                    IDE_TEST( smLayerCallback::addOID( aTrans,
                                                       aTable->mSelfOID,
                                                       sNewPieceOID,
                                                       aLobSpaceID,
                                                       SM_OID_NEW_VARIABLE_SLOT )
                              != IDE_SUCCESS );
                }
                else
                {
                    /* Nothing to do */
                }

                if( i == sBeginIdx )
                {
                    IDE_ERROR( sPieceSize >= sSrcVCPieceHeader->length );

                    IDE_TEST( copyPiece( aOldLobDesc,
                                         ( sBeginIdx * SMP_VC_PIECE_MAX_SIZE ),
                                         aLobDesc,
                                         ( sBeginIdx * SMP_VC_PIECE_MAX_SIZE ),
                                         sSrcVCPieceHeader->length )
                              != IDE_SUCCESS );
                }

                if( i == sEndIdx )
                {
                    IDE_ERROR( sPieceSize >= sSrcVCPieceHeader->length );

                    IDE_TEST( copyPiece( aOldLobDesc,
                                         ( sEndIdx * SMP_VC_PIECE_MAX_SIZE ),
                                         aLobDesc,
                                         ( sEndIdx * SMP_VC_PIECE_MAX_SIZE ),
                                         sSrcVCPieceHeader->length )
                              != IDE_SUCCESS );
                }
            }
            else
            {
                sTargetLPCH = aLobDesc->mFirstLPCH + i;
                
                sCurVCPieceHeader =
                    (smVCPieceHeader*)(sTargetLPCH)->mPtr;

                if( sCurVCPieceHeader->nxtPieceOID != sNxtPieceOID )
                {
                    sCurVCPieceHeader->nxtPieceOID = sNewPieceOID;
                }
            }
        }
        else
        {
            IDE_TEST( svpVarPageList::allocSlot(aTrans,
                                                aLobSpaceID,
                                                aTable->mSelfOID,
                                                aTable->mVar.mVRDB,
                                                sPieceSize,
                                                sNxtPieceOID,
                                                &sNewPieceOID,
                                                &sNewPiecePtr)
                      != IDE_SUCCESS );

            aLobDesc->mFirstLPCH[i].mLobVersion = aLobVersion;
            aLobDesc->mFirstLPCH[i].mOID        = sNewPieceOID;
            aLobDesc->mFirstLPCH[i].mPtr        = sNewPiecePtr;

            /* ���� �Ҵ��� lob piece�� ���Ͽ� version list �߰� */
            if ( SM_INSERT_ADD_OID_IS_OK(aAddOIDFlag) )
            {
                IDE_TEST( smLayerCallback::addOID( aTrans,
                                                   aTable->mSelfOID,
                                                   sNewPieceOID,
                                                   aLobSpaceID,
                                                   SM_OID_NEW_VARIABLE_SLOT )
                          != IDE_SUCCESS );
            }
            else
            {
                /* Nothing to do */
            }
        }

        if( i == 0 )
        {
            break;
        }
    }

    /*
     * to set fstPieceOID
     */
    
    if( aLobDesc->mLPCHCount > 0 )
    {
        aLobDesc->fstPieceOID = aLobDesc->mFirstLPCH[0].mOID;
    }

    /*
     * to set nxtPieceOID
     */ 

    if( sBeginIdx > 0 )
    {
        sNxtPieceOID = aLobDesc->mFirstLPCH[sBeginIdx].mOID;
        
        sTargetLPCH = aLobDesc->mFirstLPCH + (sBeginIdx - 1);

        sCurVCPieceHeader = (smVCPieceHeader*)(sTargetLPCH)->mPtr;

        if( sCurVCPieceHeader->nxtPieceOID != sNxtPieceOID )
        {
            sCurVCPieceHeader->nxtPieceOID = sNxtPieceOID;
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**********************************************************************
 * ���� �Ҵ���� lob piece�������� ������� �ʴ� ������ ������ �´�.
 *
 * aTrans                [IN] �۾��ϴ� Ʈ����� ��ü
 * aTable                [IN] �۾��Ϸ��� ���̺� ���
 * aSourceLobDesc        [IN] �۾��Ϸ��� Lob ����Ÿ�� ��ġ
 * aLobSpaceID           [IN] Lob Piece�� �����ϴ� SpaceID
 * aSourceOffset         [IN] ���� �۾��� �ϰ��� �ϴ� �κ��� ũ��
 * aDstLobDesc           [IN] ���� �ԷµǴ� ����Ÿ�� ũ��
 * aDstOffset            [IN] ���� �����ϴ� LPCH
 * aLength               [IN] aStartLPCH���� ������� �ʴ� ũ��
 **********************************************************************/
IDE_RC svcLob::copyPiece( smcLobDesc    * aSrcLobDesc,
                          UInt            aSrcOffset,
                          smcLobDesc    * aDstLobDesc,
                          UInt            aDstOffset,
                          UInt            aLength )
{
    UChar*   sSrcPiecePtr;
    smcLPCH* sSrcLPCH;

    IDE_DASSERT( aSrcLobDesc != NULL );
    IDE_DASSERT( aDstLobDesc != NULL );

    // source point ȹ��
    if ( SM_VCDESC_IS_MODE_IN(aSrcLobDesc) )
    {
        sSrcPiecePtr = (UChar*)aSrcLobDesc + ID_SIZEOF(smVCDescInMode) + aSrcOffset;
    }
    else
    {
        sSrcLPCH = findPosition( aSrcLobDesc, aSrcOffset );

        sSrcPiecePtr  = (UChar*)(sSrcLPCH->mPtr) + ID_SIZEOF(smVCPieceHeader)
                        + (aSrcOffset % SMP_VC_PIECE_MAX_SIZE);
    }

    // destination�� write
    if ( SM_VCDESC_IS_MODE_IN(aDstLobDesc) ) 
    {
        idlOS::memcpy( ((SChar*)aDstLobDesc + ID_SIZEOF(smVCDescInMode) + aDstOffset),
                       sSrcPiecePtr,
                       aLength);
    }
    else
    {
        IDE_TEST( write4OutMode( aDstLobDesc,
                                 aDstOffset,
                                 aLength,
                                 sSrcPiecePtr )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**********************************************************************
 * ���ο� LPCH�� �Ҵ��Ѵ�.
 *
 * aTrans         [IN]  �۾��ϴ� Ʈ����� ��ü
 * aTable         [IN]  �۾��Ϸ��� ���̺� ���
 * aLobDesc       [IN]  �۾��Ϸ��� Lob Description
 * aLobSpaceID    [IN] Lob Column�����Ͱ� ��ϵǴ� Tablespace�� ID
 * aAddOIDFlag    [IN] OID LIST�� �߰����� ����
 **********************************************************************/
IDE_RC svcLob::allocLPCH(void*              aTrans,
                         smcTableHeader*    aTable,
                         smcLobDesc*        aLobDesc,
                         smcLobDesc*        aOldLobDesc,
                         scSpaceID          aLobSpaceID,
                         UInt               aNewLPCHCnt,
                         UInt               aAddOIDFlag,
                         idBool             aIsFullWrite)

{
    smcLPCH   * sNewLPCH    = NULL;
    UInt        sCopyCnt    = 0;
    UInt        sState      = 0;

    if( aNewLPCHCnt > 0 )
    {
        /* svcLob_allocLPCH_malloc_NewLPCH.tc */
        IDU_FIT_POINT( "svcLob::allocLPCH::malloc::NewLPCH",
                        idERR_ABORT_InsufficientMemory );

        IDE_TEST( iduMemMgr::calloc(IDU_MEM_SM_SMC,
                                    1,
                                    (ULong)ID_SIZEOF(smcLPCH) * aNewLPCHCnt,
                                    (void**)&sNewLPCH)
                  != IDE_SUCCESS );
        sState = 1;

        /*
         * to copy LPCH from old LPCH if the LOB is out-mode.
         */

        if( aIsFullWrite != ID_TRUE )
        {
            if( (aOldLobDesc->flag & SM_VCDESC_MODE_MASK)
                == SM_VCDESC_MODE_OUT )
            {
                if( aOldLobDesc->mLPCHCount > aNewLPCHCnt )
                {
                    sCopyCnt = aNewLPCHCnt;
                }
                else
                {
                    sCopyCnt = aOldLobDesc->mLPCHCount;
                }

                if( sCopyCnt > 0 )
                {
                    idlOS::memcpy( sNewLPCH,
                                   aOldLobDesc->mFirstLPCH,
                                   (size_t)ID_SIZEOF(smcLPCH) * sCopyCnt );
                }
            }
        }

        /*
         * to add new LPCH OID (for rollback)
         */
        if ( SM_INSERT_ADD_LPCH_OID_IS_OK(aAddOIDFlag) )
        {
            IDE_TEST( smLayerCallback::addOID( aTrans,
                                               aTable->mSelfOID,
                                               (smOID)sNewLPCH,
                                               aLobSpaceID,
                                               SM_OID_NEW_LPCH )
                      != IDE_SUCCESS );
        }
        else
        {
            /* BUG-42411 add column�� ���з� ���� ���̺��� restore�Ҷ� �Ҵ��� LPCH��
               ager�� ���������� �ʵ��� OID�� add ���� �ʽ��ϴ�.
               (undo�� ȣ��Ǵ� restore ������ SM_INSERT_ADD_LPCH_OID_NO ����) */
        }
    }

    aLobDesc->mFirstLPCH = sNewLPCH;
    aLobDesc->mLPCHCount = aNewLPCHCnt;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    switch( sState )
    {
        case 1:
            IDE_ASSERT( iduMemMgr::free( sNewLPCH ) == IDE_SUCCESS );
            sNewLPCH = NULL;
        default:
            break;
    }

    return IDE_FAILURE;
}

/**********************************************************************
 * refine�ÿ� LPCH�� rebuild�Ѵ�.
 *
 * aTableOID     - [IN] Table�� OID
 * aArrLobColumn - [IN] Lob Column Array
 * aLobColumnCnt - [IN] Lob Column Count
 * aFixedRow     - [IN] �۾��Ϸ��� Fixed Row
 **********************************************************************/
IDE_RC svcLob::rebuildLPCH( smiColumn ** aArrLobColumn,
                            UInt         aLobColumnCnt,
                            SChar      * aFixedRow )
{
    UInt             i;
    smiColumn**      sCurColumn;
    smiColumn**      sFence;
    smcLobDesc*      sLobDesc;
    smcLPCH*         sNewLPCH;
    smOID            sCurOID;
    void*            sCurPtr;

    IDE_DASSERT(aFixedRow != NULL);

    sCurColumn = aArrLobColumn;
    sFence     = aArrLobColumn + aLobColumnCnt;

    for(; sCurColumn < sFence; sCurColumn++)
    {
        IDE_ERROR_MSG( ((*sCurColumn)->flag & SMI_COLUMN_TYPE_MASK)
                       == SMI_COLUMN_TYPE_LOB,
                       "Flag : %"ID_UINT32_FMT,
                       (*sCurColumn)->flag );

        sLobDesc = (smcLobDesc*)(aFixedRow + (*sCurColumn)->offset);

        if( (sLobDesc->flag & SM_VCDESC_MODE_MASK) == SM_VCDESC_MODE_OUT )
        {
            /* trim���� ���� mLPCHCount�� 0�� �� �ִ�. */
            if( sLobDesc->mLPCHCount > 0 )
            {
                IDE_TEST( iduMemMgr::malloc( IDU_MEM_SM_SMC,
                                             ID_SIZEOF(smcLPCH) * sLobDesc->mLPCHCount,
                                             (void**)&sNewLPCH)
                          != IDE_SUCCESS );

                sCurOID = sLobDesc->fstPieceOID;

                for(i = 0; i < sLobDesc->mLPCHCount; i++)
                {
                    IDE_ERROR_MSG( smmManager::getOIDPtr( (*sCurColumn)->colSpace,
                                                          sCurOID,
                                                          (void**)&sCurPtr )
                                   == IDE_SUCCESS,
                                   "OID : %"ID_UINT64_FMT,
                                   sCurOID );

                    sNewLPCH[i].mOID = sCurOID;
                    sNewLPCH[i].mPtr = sCurPtr;

                    sCurOID  = ((smVCPieceHeader*)sCurPtr)->nxtPieceOID;
                }

                IDE_ERROR_MSG( sCurOID == SM_NULL_OID,
                               "OID : %"ID_UINT64_FMT,
                               sCurOID );

                sLobDesc->mFirstLPCH = sNewLPCH;
            }
            else
            {
                sLobDesc->mFirstLPCH = NULL;
            }
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**********************************************************************
 * lobCursor�� ����Ű�� �ִ� �޸� LOB�� ���̸� return�Ѵ�.
 *
 * aTrans      [IN] �۾��ϴ� Ʈ����� ��ü
 * aLobViewEnv [IN] �۾��Ϸ��� LobViewEnv ��ü
 * aLobLen     [OUT] LOB ����Ÿ ����
 * aLobMode    [OUT] LOB ���� ��� ( In/Out )
 **********************************************************************/
IDE_RC svcLob::getLobInfo( idvSQL*        /*aStatistics*/,
                           void*          aTrans,
                           smLobViewEnv*  aLobViewEnv,
                           UInt*          aLobLen,
                           UInt*          aLobMode,
                           idBool*        aIsNullLob)
{
    smcLobDesc*    sLobDesc;
    SChar*         sCurFixedRowPtr;

    IDE_DASSERT(aLobViewEnv != NULL);
    IDE_DASSERT(aLobLen != NULL);

    IDE_TEST(getViewRowPtr(aTrans,
                           aLobViewEnv,
                           &sCurFixedRowPtr) != IDE_SUCCESS);

    sLobDesc = (smcLobDesc*)(sCurFixedRowPtr
                             + aLobViewEnv->mLobCol.offset);

    *aLobLen = sLobDesc->length;

    if( (sLobDesc->flag & SM_VCDESC_NULL_LOB_MASK) ==
        SM_VCDESC_NULL_LOB_TRUE )
    {
        *aIsNullLob = ID_TRUE;
    }
    else
    {
        *aIsNullLob = ID_FALSE;
    }

    if( aLobMode != NULL )
    {
        *aLobMode = sLobDesc->flag & SMI_COLUMN_MODE_MASK ;
    }

    mMemLobStatistics.mGetLobInfo++;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**********************************************************************
 * Lob Cursor�� ������ Row Pointer�� ã�´�.
 *
 * aTrans      [IN] �۾��ϴ� Ʈ����� ��ü
 * aLobViewEnv [IN] �۾��Ϸ��� LobViewEnv ��ü
 * aRowPtr     [OUT] �аų� Update�ؾ��� Row Pointer
 **********************************************************************/
IDE_RC svcLob::getViewRowPtr(void*         aTrans,
                             smLobViewEnv* aLobViewEnv,
                             SChar**       aRowPtr)
{
    SChar*         sReadFixedRowPtr;
    smpSlotHeader* sCurFixedSlotHeader;
    smpSlotHeader* sNxtFixedSlotHeader;
    smSCN          sSCN;
    smTID          sTID;

    sReadFixedRowPtr     = (SChar*)(aLobViewEnv->mRow);
    sCurFixedSlotHeader = (smpSlotHeader*)sReadFixedRowPtr;

    if(aLobViewEnv->mOpenMode == SMI_LOB_READ_WRITE_MODE)
    {
        // fixed row�� ���� ������ �����Ѵ�.
        while( SMP_SLOT_HAS_VALID_NEXT_OID( sCurFixedSlotHeader ) )
        {
            IDE_ASSERT( smmManager::getOIDPtr( 
                            aLobViewEnv->mLobCol.colSpace,
                            SMP_SLOT_GET_NEXT_OID(sCurFixedSlotHeader),
                            (void**)&sNxtFixedSlotHeader )
                        == IDE_SUCCESS );

            SMX_GET_SCN_AND_TID( sNxtFixedSlotHeader->mCreateSCN, sSCN, sTID );

            if ( ( SM_SCN_IS_INFINITE( sSCN ) ) &&
                 ( sTID == smLayerCallback::getTransID( aTrans ) ) )
            {
                if( SM_SCN_IS_EQ(&sSCN, &(aLobViewEnv->mInfinite)) )
                {
                    // ���� Lob Cursor�� update�� Next Version�̶�� �����ش�.
                    sReadFixedRowPtr = (SChar*)sNxtFixedSlotHeader;
                }
            }

            sCurFixedSlotHeader = sNxtFixedSlotHeader;
        }

        sCurFixedSlotHeader = (smpSlotHeader*)sReadFixedRowPtr;

        IDE_TEST_RAISE( SM_SCN_IS_DELETED(sCurFixedSlotHeader->mCreateSCN),
                        lob_cursor_too_old);
    }

    *aRowPtr = sReadFixedRowPtr;

    return IDE_SUCCESS;

    IDE_EXCEPTION(lob_cursor_too_old);
    {
        IDE_SET(ideSetErrorCode (smERR_ABORT_LobCursorTooOld));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**********************************************************************
 * aRowPtr�� aLobViewEnv�� ����Ű�� Table�� �����Ѵ�.
 *
 * aTrans      [IN] �۾��ϴ� Ʈ����� ��ü
 * aLobViewEnv [IN] �۾��Ϸ��� LobViewEnv ��ü
 * aRowPtr     [IN] Insert�ؾ��� Row Pointer
 **********************************************************************/
IDE_RC svcLob::insertIntoIdx(idvSQL*       aStatistics,
                             void*         aTrans,
                             smLobViewEnv* aLobViewEnv,
                             SChar*        aRowPtr)
{
    UInt             sIndexCnt;
    smnIndexHeader*  sIndexCursor;
    UInt             i;
    idBool           sUniqueCheck = ID_FALSE;
    smcTableHeader*  sTable;
    SChar*           sNullPtr;

    sTable = (smcTableHeader*)aLobViewEnv->mTable;
    sIndexCnt = smcTable::getIndexCount(aLobViewEnv->mTable);
    IDE_ASSERT( smmManager::getOIDPtr( sTable->mSpaceID, 
                                       sTable->mNullOID,
                                       (void**)&sNullPtr )
                == IDE_SUCCESS );

    for( i = 0 ; i < sIndexCnt; i++ )
    {
        sIndexCursor = (smnIndexHeader*)smcTable::getTableIndex( aLobViewEnv->mTable, i);

        if((sIndexCursor->mFlag & SMI_INDEX_USE_MASK) == SMI_INDEX_USE_ENABLE)
        {
            if( ((sIndexCursor->mFlag & SMI_INDEX_UNIQUE_MASK) ==
                 SMI_INDEX_UNIQUE_ENABLE) ||
                ((sIndexCursor->mFlag & SMI_INDEX_LOCAL_UNIQUE_MASK) ==
                 SMI_INDEX_LOCAL_UNIQUE_ENABLE) )
            {
                sUniqueCheck = ID_TRUE;
            }

            IDE_TEST( sIndexCursor->mInsert( aStatistics,
                                             aTrans,
                                             aLobViewEnv->mTable,
                                             sIndexCursor,
                                             aLobViewEnv->mInfinite,
                                             aRowPtr,
                                             sNullPtr,
                                             sUniqueCheck,
                                             aLobViewEnv->mSCN,
                                             NULL,
                                             NULL,
                                             ID_ULONG_MAX,/* aInsertWaitTime */
                                             ID_FALSE )   /* aForbiddenToRetry */
                      != IDE_SUCCESS );
        }//if
    }//for i

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**********************************************************************
 * Lob Piece�� ����Ѵ�.
 **********************************************************************/
IDE_RC svcLob::writePiece( smcLPCH    * aTargetLPCH,
                           UShort       aOffset,
                           UShort       aMount,
                           UChar      * aBuffer )
{
    idlOS::memcpy( (UChar*)(aTargetLPCH->mPtr)
                   + ID_SIZEOF(smVCPieceHeader) + aOffset,
                   aBuffer,
                   aMount );

    return IDE_SUCCESS;
}

/**********************************************************************
 * ������ ���̸� �����ϱ� ���� LPCH�� Count�� �����Ѵ�.
 **********************************************************************/
UInt svcLob::getLPCHCntFromLength( UInt aLength )
{
    UInt sLPCHCnt = 0;

    if( aLength > 0 )
    {
        sLPCHCnt = aLength / SMP_VC_PIECE_MAX_SIZE;
        
        if( (aLength % SMP_VC_PIECE_MAX_SIZE) > 0 )
        {
            sLPCHCnt++;
        }
    }

    return sLPCHCnt;
}

/**********************************************************************
 * LOB �÷��� update �Ѵ�.
 **********************************************************************/
IDE_RC svcLob::getLastVersion(idvSQL*          aStatistics,
                              void*            aTrans,
                              smLobViewEnv*    aLobViewEnv,
                              SChar**          aNxtFixedRowPtr)
{
    smSCN          sSCN;
    smTID          sTID;
    SChar*         sNxtFixedRowPtr = NULL;
    smpSlotHeader* sCurFixedSlotHeader = NULL;
    SChar*         sUpdateFixedRowPtr = NULL;
    smpSlotHeader* sNxtFixedSlotHeader = NULL ;
    
    IDE_DASSERT( aTrans != NULL );
    IDE_DASSERT( aLobViewEnv != NULL );

    sCurFixedSlotHeader = (smpSlotHeader*)aLobViewEnv->mRow;

    /*
     * to update LOB column
     */ 

    sUpdateFixedRowPtr = NULL;

    IDE_TEST_RAISE( SM_SCN_IS_DELETED(sCurFixedSlotHeader->mCreateSCN),
                    lob_cursor_too_old );
    while(1)
    {
        if( SM_SCN_IS_FREE_ROW( sCurFixedSlotHeader->mLimitSCN ) )
        {

            if ( /*���� Update�� Row�� Ʈ������� �ٸ� Lob Cursor�� �����ִ�.*/
                 ( smLayerCallback::getMemLobCursorCnt( aTrans,
                                                        aLobViewEnv->mLobCol.id,
                                                        aLobViewEnv->mRow ) <= 1 ) &&
                 /*���� Row�� �ٸ� Transaction�� Update�� Row�̴�.*/
                 SM_SCN_IS_EQ( &(sCurFixedSlotHeader->mCreateSCN), &(aLobViewEnv->mInfinite) ) )
            {
                // lob copy�Ҷ��� ���� insert�� row�� �ٷ� ����Ѵ�.
                sNxtFixedRowPtr = (SChar*)aLobViewEnv->mRow;
                break;
            }
            else
            {
                sUpdateFixedRowPtr = (SChar*)(aLobViewEnv->mRow);
            }
        }
        else
        {
            sNxtFixedSlotHeader = sCurFixedSlotHeader;

            while( SMP_SLOT_HAS_VALID_NEXT_OID( sNxtFixedSlotHeader ) )
            {
                /* update�� ���� �ֽ� ������ ���ؼ� ������ �ȴ�.*/
                IDE_ASSERT( smmManager::getOIDPtr(
                                aLobViewEnv->mLobCol.colSpace,
                                SMP_SLOT_GET_NEXT_OID(sNxtFixedSlotHeader),
                                (void**)&sNxtFixedSlotHeader )
                            == IDE_SUCCESS );
            }

            if( SM_SCN_IS_FREE_ROW( sNxtFixedSlotHeader->mLimitSCN ) )
            {
                SMX_GET_SCN_AND_TID( sNxtFixedSlotHeader->mCreateSCN, sSCN, sTID );

                /* check whether the record is already modified. */
                IDE_ASSERT( SM_SCN_IS_INFINITE( sSCN ) );

                // ���� Ʈ����ǿ� ���� �ٸ� �۾��� �־��ٸ�...
                IDE_ASSERT( sTID == smLayerCallback::getTransID( aTrans ) );

                // �� LOB Cursor�� ���� Update�Ǳ����� �ٸ� LOB
                // Cursor�� ���� Update�� �߻��Ͽ���.
                if ( smLayerCallback::getLogTypeFlagOfTrans( aTrans )
                     == SMR_LOG_TYPE_NORMAL )
                {
                    /* BUG-16003:
                     * Sender�� �ϳ��� Row�� ���ؼ� ���� Table Cursor��
                     * LOB Cursor�� �ΰ� ���� �ΰ��� LOB Cursor�� ����
                     * Infinite SCN�� ������. ������ Receiver�ܿ�����
                     * ���� �ٸ� Table Cursor�� LOB Cursor�� �ΰ�������
                     * �Ǿ �ٸ� Infinite SCN�� ������ �Ǿ� Sender����
                     * ������ Prepare�� Receiver�ܿ����� Too Old������
                     * �߻��Ѵ�. �� ������ �����ϱ� ���� Normal Transaction
                     * �� ��쿡�� �Ʒ� üũ�� �����Ѵ�. Replication�� ������
                     * ������ ��쿡�� Log�� ��ϵǱ� ������ �Ʒ� Validate��
                     * �����ص� �ȴ�.*/
                    IDE_TEST_RAISE( !SM_SCN_IS_EQ(
                                        &sSCN,
                                        &(aLobViewEnv->mInfinite)),
                                    lob_cursor_too_old);
                }

                if ( smLayerCallback::getMemLobCursorCnt( aTrans,
                                                          aLobViewEnv->mLobCol.id,
                                                          sNxtFixedSlotHeader )
                     != 0 )
                {
                    sUpdateFixedRowPtr = (SChar*)sNxtFixedSlotHeader;
                }
                else
                {
                    // ���� Lob Cursor�� update�� Next Version�� �����Ѵ�.
                    // BUGBUG - �̶� partial rollback ��������..
                    sNxtFixedRowPtr = (SChar*)sNxtFixedSlotHeader;
                    break;
                }
            }
            else
            {
                if( SM_SCN_IS_LOCK_ROW(sNxtFixedSlotHeader->mLimitSCN))
                {
                    IDE_ASSERT( sNxtFixedSlotHeader == sCurFixedSlotHeader);

                    sUpdateFixedRowPtr = (SChar*)(aLobViewEnv->mRow);
                }
                else
                {
                    /*
                     * delete row������ ����Ǿ���.
                     * �� ��쿡�� �̹� delete�� row�� mNext�� ���ο� ������ �޸��� �ȴ�.
                     * �̷��� ���� �߻��ؼ��� �ȵȴ�.
                     */
                    IDE_ASSERT(0);
                }
            }
        }

        IDE_ASSERT(sUpdateFixedRowPtr != NULL);

        // Global������ ����� SC_NULL_GRID�� NULL GRID�� �´��� ��Ȯ��
        // �޸� ���� ��� ���⼭ ASSERT�ɸ�
        IDE_ASSERT( SC_GRID_IS_NULL( SC_NULL_GRID ) );

        // fixed row�� ���� update version ����
        IDE_TEST( svcRecord::updateVersion(
                              aStatistics,  // aStatistics
                              aTrans,
                              aLobViewEnv->mSCN,
                              NULL, // aTableInfoPtr
                              (smcTableHeader*)(aLobViewEnv->mTable),
                              sUpdateFixedRowPtr,
                              SC_NULL_GRID, // aOldRID 
                              &sNxtFixedRowPtr,
                              NULL, // aRetSlotRID 
                              NULL, // aColumnList
                              NULL, // aValueList
                              NULL, // DNL retry info
                              aLobViewEnv->mInfinite,
                              NULL, // aLobInfo4Update
                              0,    // aModifyIdxBit 
                              ID_FALSE )    // aForbiddenToRetry
                  != IDE_SUCCESS );

        IDE_TEST(insertIntoIdx(aStatistics, /* idvSQL* */
                               aTrans,
                               aLobViewEnv,
                               sNxtFixedRowPtr)
                 != IDE_SUCCESS);
        break;
    }

    *aNxtFixedRowPtr = sNxtFixedRowPtr;

    return IDE_SUCCESS;

    IDE_EXCEPTION(lob_cursor_too_old);
    {
        IDE_SET(ideSetErrorCode (smERR_ABORT_LobCursorTooOld));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**********************************************************************
 * write�� ������ Ȯ���Ѵ�.
 **********************************************************************/
IDE_RC svcLob::reserveSpace(void*         aTrans,
                            smLobViewEnv* aLobViewEnv,
                            SChar*        aNxtFixedRowPtr,
                            UInt          aOffset,
                            UInt          aNewSize)
{
    smcLobDesc*    sCurLobDesc  = NULL;
    smcLobDesc*    sNewLobDesc  = NULL;
    SChar*         sLobInRowPtr = NULL;
    ULong          sLobColBuf[ SMC_LOB_MAX_IN_ROW_STORE_SIZE/ID_SIZEOF(ULong) ];

    sLobInRowPtr = aNxtFixedRowPtr + aLobViewEnv->mLobCol.offset;
    sCurLobDesc  = (smcLobDesc*)sLobInRowPtr;

    IDE_TEST_RAISE( aOffset > sCurLobDesc->length, range_error );

    // old image�� ����
    if ( SM_VCDESC_IS_MODE_IN(sCurLobDesc) )
    {
        // BUG-30101 ����Ÿ�� In-Mode�� ����Ǿ��� ��� ����Ÿ��
        // Fixed������ ����Ǿ��ִ�. Lob Column Desc �� �Բ� ���� �����صд�
        idlOS::memcpy( sLobColBuf,
                       sLobInRowPtr,
                       sCurLobDesc->length + ID_SIZEOF(smVCDescInMode));
    }
    else
    {
        // Out Mode�� ���� �Ǿ� �������
        idlOS::memcpy( sLobColBuf, sLobInRowPtr, ID_SIZEOF(smcLobDesc) );
    }

    // new version �Ҵ�
    // BUG-30036 Memory LOB�� ODBC�� Insert �ϴ� �����Ͽ��� ��,
    // �����ϴ� LOB Desc�� ������ ä�� Rollback���� �ʰ� �ֽ��ϴ�. �� ���Ͽ�
    // ������ Dummy Lob Desc�� Prepare �ϰ� Log�� ���� ��� �� �Ŀ�
    // ����� LOB Desc�� Data Page�� �ݿ��մϴ�.
    IDE_TEST( reserveSpaceInternal( aTrans,
                                    (smcTableHeader*)(aLobViewEnv->mTable),
                                    &aLobViewEnv->mLobCol,
                                    aLobViewEnv->mLobVersion,
                                    (smcLobDesc*)sLobColBuf,
                                    aOffset,
                                    aNewSize,
                                    SM_FLAG_INSERT_LOB,
                                    ID_FALSE /* aIsFullWrite */)
              != IDE_SUCCESS );

    sNewLobDesc = (smcLobDesc*)sLobColBuf;

    // BUG-30036 ����� Lob Desc�� Data Page�� �ݿ�
    if ( SM_VCDESC_IS_MODE_IN(sNewLobDesc) )
    {
        /* Lob Desc�� �����Ѵ�. */
        idlOS::memcpy( sLobInRowPtr,
                       sLobColBuf,
                       ID_SIZEOF(smVCDescInMode) );
    }
    else
    {
        // BUG-30101 Out Mode�� ���� �� ��� Lob Desc�� �����Ѵ�.
        idlOS::memcpy( sLobInRowPtr,
                       sLobColBuf,
                       ID_SIZEOF(smcLobDesc) );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION(range_error);
    {
        // BUG-29212 ��ũ Lob�� read�� range check�� ���� �ʽ��ϴ�.
        // range�� Offset���� Amount - 1 ���� �Դϴ�.
        IDE_SET(ideSetErrorCode(smERR_ABORT_RangeError,
                                aOffset,
                                (aOffset + aNewSize - 1),
                                0,
                                (sCurLobDesc->length - 1)));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**********************************************************************
 * ���� trim �Ѵ�.
 **********************************************************************/
IDE_RC svcLob::trimSpaceInternal(void*               aTrans,
                                 smcTableHeader*     aTable,
                                 const smiColumn*    aLobColumn,
                                 ULong               aLobVersion,
                                 smcLobDesc*         aLobDesc,
                                 UInt                aOffset,
                                 UInt                aAddOIDFlag)
{
    smcLobDesc* sOldLobDesc;
    UInt        sNewLobLen;
    UInt        sNewLPCHCnt;
    UInt        sBeginIdx;
    UInt        sEndIdx;
    ULong       sLobColumnBuffer[SM_PAGE_SIZE / ID_SIZEOF(ULong)];

    IDE_DASSERT( aTrans != NULL );
    IDE_DASSERT( aTable != NULL );
    IDE_DASSERT( aLobDesc != NULL );

    IDE_TEST_RAISE( (aOffset >= aLobDesc->length), range_error );

    /*
     * to backup LOB Descriptor
     */
    
    sOldLobDesc = (smcLobDesc*)sLobColumnBuffer;

    if ( SM_VCDESC_IS_MODE_IN( aLobDesc ) )
    {
        /* ����Ÿ�� In-Mode�� ����Ǿ��� ��� ����Ÿ��
         * Fixed������ ����Ǿ��ִ�. Lob Column Desc �� �Բ� ���� �����صд� */
        idlOS::memcpy( (SChar*)sOldLobDesc,
                       (SChar*)aLobDesc,
                       ID_SIZEOF(smVCDescInMode) + aLobDesc->length );
    }
    else
    {
        idlOS::memcpy( (SChar*)sOldLobDesc,
                       (SChar*)aLobDesc,
                       ID_SIZEOF(smcLobDesc) );
    }

    /*
     * to allocate new version space
     */

    sNewLobLen = aOffset;
    
    if( (sOldLobDesc->flag & SM_VCDESC_MODE_MASK) == SM_VCDESC_MODE_OUT )
    {
        aLobDesc->mLobVersion = aLobVersion;

        sNewLPCHCnt = getLPCHCntFromLength(sNewLobLen);

        IDE_TEST( allocLPCH(aTrans,
                            aTable,
                            aLobDesc,
                            sOldLobDesc,
                            aLobColumn->colSpace,
                            sNewLPCHCnt,
                            aAddOIDFlag,
                            ID_FALSE /* aIsFullWrite */)
                  != IDE_SUCCESS );

        IDE_TEST( trimPiece(aTrans,
                            aTable,
                            aLobVersion,
                            aLobDesc,
                            sOldLobDesc,
                            aLobColumn->colSpace,
                            aOffset,
                            aAddOIDFlag)
                  != IDE_SUCCESS );
    }
    else
    {
        /* nothing to do */
    }

    aLobDesc->length = sNewLobLen;

    /*
     * to remove old version (LPCH & LOB Piece)
     */

    if( ((sOldLobDesc->flag & SM_VCDESC_MODE_MASK) == SM_VCDESC_MODE_OUT) &&
        (sOldLobDesc->mLPCHCount > 0) )
    {

        sBeginIdx = aOffset / SMP_VC_PIECE_MAX_SIZE;
        sEndIdx = sOldLobDesc->mLPCHCount - 1;

        IDE_TEST( removeOldLPCH(aTrans,
                                aTable,
                                aLobColumn,
                                sOldLobDesc,
                                aLobDesc,
                                sBeginIdx,
                                sEndIdx)
                  != IDE_SUCCESS );
    }
    else
    {
        /* nothing to do */
    }
    
    return IDE_SUCCESS;

    IDE_EXCEPTION(range_error);
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_RangeError,
                                aOffset,
                                (aLobDesc->length - 1),
                                0,
                                (aLobDesc->length - 1)));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**********************************************************************
 * Old Version�� LPCH�� ����Ű�� Piece �� LPCH�� �����Ѵ�.
 **********************************************************************/
IDE_RC svcLob::removeOldLPCH(void*               aTrans,
                             smcTableHeader*     aTable,
                             const smiColumn*    aLobColumn,
                             smcLobDesc*         aOldLobDesc,
                             smcLobDesc*         aLobDesc,
                             UInt                aBeginIdx,
                             UInt                aEndIdx)
{
    smcLPCH*    sOldLPCH = NULL;
    smcLPCH*    sNewLPCH = NULL;
    UInt        i;

    IDE_ASSERT( aOldLobDesc->mFirstLPCH != NULL );
    IDE_ASSERT( aOldLobDesc->mLPCHCount > 0 );
    IDE_ASSERT( (aOldLobDesc->flag & SM_VCDESC_MODE_MASK) == SM_VCDESC_MODE_OUT );

    if( aLobDesc->mFirstLPCH != aOldLobDesc->mFirstLPCH )
    {
        IDE_TEST( smLayerCallback::addOID( aTrans,
                                           aTable->mSelfOID,
                                           (smOID)(aOldLobDesc->mFirstLPCH),
                                           aLobColumn->colSpace,
                                           SM_OID_OLD_LPCH )
                  != IDE_SUCCESS );
    }

    for( i = aBeginIdx; i <= aEndIdx; i++ )
    {
        sOldLPCH = aOldLobDesc->mFirstLPCH + i;

        if( i < aLobDesc->mLPCHCount )
        {
            sNewLPCH = aLobDesc->mFirstLPCH + i;
        }
        else
        {
            sNewLPCH = NULL;
        }

        if( sNewLPCH == NULL ) // for trim
        {
            IDE_TEST( svcRecord::setFreeFlagAtVCPieceHdr( sOldLPCH->mPtr,
                                                          SM_VCPIECE_FREE_OK )
                      != IDE_SUCCESS );
                            
            IDE_TEST( smLayerCallback::addOID( aTrans,
                                               aTable->mSelfOID,
                                               sOldLPCH->mOID,
                                               aLobColumn->colSpace,
                                               SM_OID_OLD_VARIABLE_SLOT )
                      != IDE_SUCCESS );
            
        }
        else
        {
            if( sOldLPCH->mOID != sNewLPCH->mOID )
            {
                IDE_TEST( svcRecord::setFreeFlagAtVCPieceHdr( sOldLPCH->mPtr,
                                                              SM_VCPIECE_FREE_OK )
                          != IDE_SUCCESS );
                            
                IDE_TEST( smLayerCallback::addOID( aTrans,
                                                   aTable->mSelfOID,
                                                   sOldLPCH->mOID,
                                                   aLobColumn->colSpace,
                                                   SM_OID_OLD_VARIABLE_SLOT )
                          != IDE_SUCCESS );
            }
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC svcLob::trimPiece(void*           aTrans,
                         smcTableHeader* aTable,
                         ULong           aLobVersion,
                         smcLobDesc*     aLobDesc,
                         smcLobDesc*     aOldLobDesc,
                         scSpaceID       aLobSpaceID,
                         UInt            aOffset,
                         UInt            aAddOIDFlag)
{
    UInt             sIdx;
    smOID            sNxtPieceOID = SM_NULL_OID;
    smOID            sNewPieceOID = SM_NULL_OID;
    SChar*           sNewPiecePtr = NULL;
    smcLPCH*         sTargetLPCH;
    smVCPieceHeader* sCurVCPieceHeader;
    UInt             sPieceSize;

    IDE_ASSERT( aTrans   != NULL );
    IDE_ASSERT( aTable   != NULL );
    IDE_ASSERT( aLobDesc != NULL );
    IDE_DASSERT( (aOldLobDesc->flag & SM_VCDESC_MODE_MASK) == SM_VCDESC_MODE_OUT )

    if( aLobDesc->mLPCHCount == 0 )
    {
        aLobDesc->fstPieceOID = SM_NULL_OID;
        IDE_CONT( SKIP_TRIM_PIECE );
    }

    sIdx = aOffset / SMP_VC_PIECE_MAX_SIZE;

    IDE_ERROR( (UInt)sIdx < aOldLobDesc->mLPCHCount );

    if( aLobDesc->mLPCHCount <= 1 )
    {
        IDE_ERROR( aOffset <= SMP_VC_PIECE_MAX_SIZE );
        
        sPieceSize = aOffset;
    }
    else
    {
        sPieceSize = SMP_VC_PIECE_MAX_SIZE;
    }    

    /*
     * to allocate piece.
     */

    if( aLobDesc->mLPCHCount > 0 )
    {
        IDE_ERROR( sIdx < aLobDesc->mLPCHCount );

        sNxtPieceOID = SM_NULL_OID;
        sNewPiecePtr = NULL;
        sNewPieceOID = SM_NULL_OID;

        if( aLobVersion != aOldLobDesc->mFirstLPCH[sIdx].mLobVersion )
        {
            IDE_TEST( svpVarPageList::allocSlot(aTrans,
                                                aLobSpaceID,
                                                aTable->mSelfOID,
                                                aTable->mVar.mMRDB,
                                                sPieceSize,
                                                sNxtPieceOID,
                                                &sNewPieceOID,
                                                &sNewPiecePtr)
                      != IDE_SUCCESS );

            aLobDesc->mFirstLPCH[sIdx].mLobVersion = aLobVersion;
            aLobDesc->mFirstLPCH[sIdx].mOID        = sNewPieceOID;
            aLobDesc->mFirstLPCH[sIdx].mPtr        = sNewPiecePtr;

            /* ���� �Ҵ��� lob piece�� ���Ͽ� version list �߰� */
            if ( SM_INSERT_ADD_OID_IS_OK(aAddOIDFlag) )
            {
                IDE_TEST( smLayerCallback::addOID( aTrans,
                                                   aTable->mSelfOID,
                                                   sNewPieceOID,
                                                   aLobSpaceID,
                                                   SM_OID_NEW_VARIABLE_SLOT )
                          != IDE_SUCCESS );
            }
            else
            {
                /* Nothing to do */
            }
            IDE_TEST( copyPiece( aOldLobDesc,
                                 ( sIdx * SMP_VC_PIECE_MAX_SIZE ),
                                 aLobDesc,
                                 ( sIdx * SMP_VC_PIECE_MAX_SIZE ),
                                 sPieceSize )
                      != IDE_SUCCESS );
        }
        else
        {
            sTargetLPCH = aLobDesc->mFirstLPCH + sIdx;
                
            sCurVCPieceHeader =
                (smVCPieceHeader*)(sTargetLPCH)->mPtr;

            sCurVCPieceHeader->nxtPieceOID = sNxtPieceOID;
        }

        /*
         * to set fstPieceOID
         */
        
        aLobDesc->fstPieceOID = aLobDesc->mFirstLPCH[0].mOID;
    }
    else
    {
        IDE_ERROR( aLobDesc->mFirstLPCH == NULL );
        
        /*
         * to set fstPieceOID
         */
        
        aLobDesc->fstPieceOID = SM_NULL_OID;
    }
    
    /*
     * to set nxtPieceOID
     */ 

    if( sIdx > 0 )
    {
        sNxtPieceOID = aLobDesc->mFirstLPCH[sIdx].mOID;

        sTargetLPCH = aLobDesc->mFirstLPCH + (sIdx - 1);

        sCurVCPieceHeader = (smVCPieceHeader*)(sTargetLPCH)->mPtr;

        sCurVCPieceHeader->nxtPieceOID = sNxtPieceOID;
    }

    IDE_EXCEPTION_CONT( SKIP_TRIM_PIECE );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC svcLob::trim( idvSQL       * aStatistics,
                     void         * aTrans,
                     smLobViewEnv * aLobViewEnv,
                     smLobLocator   /*aLobLocator*/,
                     UInt           aOffset )
{
    SChar*      sFixedRowPtr = NULL;
    smcLobDesc* sCurLobDesc  = NULL;
    SChar*      sLobInRowPtr = NULL;
    smcLobDesc* sNewLobDesc;
    ULong       sLobColBuf[ SMC_LOB_MAX_IN_ROW_STORE_SIZE/ID_SIZEOF(ULong) ];

    IDE_DASSERT( aTrans != NULL );
    IDE_DASSERT( aLobViewEnv != NULL );

    /*
     * to update LOB column
     */

    IDE_TEST( getLastVersion(aStatistics,
                             aTrans,
                             aLobViewEnv,
                             &sFixedRowPtr)
              != IDE_SUCCESS );

    /*
     * to set new LOB version
     */
    
    sLobInRowPtr = sFixedRowPtr + aLobViewEnv->mLobCol.offset;
    sCurLobDesc  = (smcLobDesc*)sLobInRowPtr;

    IDE_TEST_RAISE( aOffset >= sCurLobDesc->length, range_error );

    if ( SM_VCDESC_IS_MODE_IN(sCurLobDesc) )
    {
        aLobViewEnv->mLobVersion = 1;
        // BUG-30101 ����Ÿ�� In-Mode�� ����Ǿ��� ��� ����Ÿ��
        // Fixed������ ����Ǿ��ִ�. Lob Column Desc �� �Բ� ���� �����صд�
        idlOS::memcpy( sLobColBuf,
                       sLobInRowPtr,
                       sCurLobDesc->length + ID_SIZEOF(smVCDescInMode));
    }
    else
    {
        // LobVersion �� Out Mode ������ ��ȿ�ϴ�. 
        aLobViewEnv->mLobVersion = sCurLobDesc->mLobVersion + 1;
        IDE_TEST_RAISE( aLobViewEnv->mLobVersion == ID_ULONG_MAX,
                        error_version_overflow );    

        // Out Mode�� ���� �Ǿ� �������
        idlOS::memcpy( sLobColBuf, sLobInRowPtr, ID_SIZEOF(smcLobDesc) );
    }
    

    /*
     * to trim
     */

    IDE_TEST( trimSpaceInternal(aTrans,
                                (smcTableHeader*)(aLobViewEnv->mTable),
                                &aLobViewEnv->mLobCol,
                                aLobViewEnv->mLobVersion,
                                (smcLobDesc*)sLobColBuf,
                                aOffset,
                                SM_FLAG_INSERT_LOB)
              != IDE_SUCCESS );

     sNewLobDesc = (smcLobDesc*)sLobColBuf;

    if ( SM_VCDESC_IS_MODE_IN(sNewLobDesc) )
    {
        /* Lob Desc�� �����Ѵ�. */
        idlOS::memcpy( sLobInRowPtr,
                       sLobColBuf,
                       ID_SIZEOF(smVCDescInMode) );
    }
    else
    {
        // BUG-30101 Out Mode�� ���� �� ��� Lob Desc�� �����Ѵ�.
        idlOS::memcpy( sLobInRowPtr,
                       sLobColBuf,
                       ID_SIZEOF(smcLobDesc) );
    }

    mMemLobStatistics.mTrim++;

    return IDE_SUCCESS;

    IDE_EXCEPTION(range_error);
    {
        // BUG-29212 ��ũ Lob�� read�� range check�� ���� �ʽ��ϴ�.
        // range�� Offset���� Amount - 1 ���� �Դϴ�.
        IDE_SET(ideSetErrorCode(smERR_ABORT_RangeError,
                                aOffset,
                                (sCurLobDesc->length - 1),
                                0,
                                (sCurLobDesc->length - 1)));
    }
    IDE_EXCEPTION( error_version_overflow )
    {
        IDE_SET( ideSetErrorCode(smERR_ABORT_INTERNAL_ARG,
                                 "volatile trim:version overflow") );
    }

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}
