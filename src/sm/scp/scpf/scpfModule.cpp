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
 * $Id: scpfModule.cpp 
 **********************************************************************/

/* << SM Common DataPort File Module >>
 *  < Proj-2059 DB Upgrade ��� >
 *
 *
 *
 *
 *  - ����
 *   �� Module�� Table�� �ִ� �����͸� �����ϰ� �ִ� DataPort ��� ��
 *  �Ű�ü�� File�� ����ϴ� Module�Դϴ�.
 *   �� Module�� scpModule���� ���ǵ� Interface�� �����մϴ�. ����ϴ� 
 * �ʿ����� smiDataPort �������̽��� ���� scpModule�� �����Ͽ� ����ϸ�
 * �˴ϴ�.
 *
 *
 *
 *
 *
 *
 *  - File����
 *      Header
 *          Version(4Byte)
 *          CommonHeader( gSmiDataPortHeaderDesc )
 *          FileHeader( gScpfFileHeaderDesc ) 
 *          TableHeader( gQsfTableHeaderDesc )
 *          ColumnHeader( gQsfColumnHeaderDesc ) * ColumnCount
 *          PartitionHeader( gQsfPartitionHeaderDesc ) * PartitionCount
 *      Block
 *          BlockHeader( gScpfBlockHeaderDesc )
 *          BlockBody
 *
 *
 *
 *
 *
 *
 *  - Block Body�� ����
 *      - BasicColumn
 *          - �⺻
 *              <Slot_Len(1~4Byte)><SlotVal(..Byte)>
 *
 *          - Block�� ��ĥ ��� (Chained)
 *              <Slot_Len(1~4Byte)><SlotVal(..)> |  <VarLen(1~4Byte)><SlotVal(....)> 
 *              ��) <Slot_Len:20><....20Byte>
 *                      =>  <Slot_Len:15><....15Byte> | <VarLen:5><..5Byte)
 *
 *      - LobColumn
 *          - �⺻
 *              <LobLen(4Byte)><VarInt(1~4Byte)><SlotVal(..Byte)>
 *
 *          - Block�� ��ĥ ��� (Chained)
 *              <LobLen(4Byte)><Slot_Len(1~4Byte)><SlotVal(..Byte)>
 *                      |  <Slot_Len(1~4Byte)><SlotVal(..Byte)> 
 *              ��) <LobLen:20><Slot_Len:20><....20Byte>
 *                      =>  <LobLen:20><Slot_Len:15><....15Byte>
 *                              | <Slot_Len:5><..5Byte)
 *
 *
 *
 *
 *
 * - ���
 *  Block        : 1~N���� Block�� �� File�� �����Ѵ�. 
 *                 Block�� 0~N���� Row�� �����ȴ�.
 *  Row          : Row�� 1~N���� Column���� �����ȴ�.
 *  Column       : Column�� BasicColumn�� LobColumn�� �ִ�.
 *  BasicColumn  : 1~N���� Slot�� �� �ϳ��� BasicColumn�� �ȴ�.
 *  LobColumn    : LobLen�ϳ��� 1~N���� Slot�� �� �ϳ��� BasicColumn�� �ȴ�.
 *
 *  isLobColmnInfo : �� Column���� Lob�̳� �ƴϳĿ� ���� ����. 
 *                      �̹��� ���� Column�� Lob���� ��ϵǾ� �ִ�.
 *
 *  ColumnMap    : BasicColumn�� LobColumn�� ������ Map.
 *                 ex) Column�� NLNNL ������ ���, NNNLL�� ��ġ�Ǿ�� ��.
 *                     ColumnMap -> 0,2,3,1,4
 *
 *  SlotValLen   : Slot�� Length. ũ�⿡ ���� Variable�ϰ� 1~4Byte ���̷�
 *                 ����Ǿ� ��ϵȴ�.
 *      0~63        [0 | len ]
 *     64~16383     [1 | len>>8  & 63][len ]
 *  16384~4194303   [2 | len>>16 & 63][len>>8 & 255][len & 255]
 *4194304~1073741824[3 | len>>24 & 63][len>>16 & 255][len>>8 & 255][len & 255]
 *
 *  LobLen       : Lob Column�ϳ��� ��ü ����. 4Byte UInt�� ��ϵȴ�.
 *
 *  Overflow     : Row�� Block�� ���� ��ϵɶ�, �Ҽӵ� Slot�� ���� Block�� ���
 *                 �Ǿ�� �ϴµ� �Ѿ���Ƿ�, OverflowBlock�̴�
 *
 *  Chaining     : Row�� �����ϴ� Column�� Block�� ���� ��ϵɶ�,
 *                 Chaining�Ǿ��ٰ� �Ѵ�.
 *
 *  Overflow��Chaining�� ���� : Chaining�� Column�� Slot�� �ɰ��� ���̰�
 *                              OverFlow�� Row�� �����ϴ� Column��
 *                              ������ ��ϵ� ���̴�.
 *      ex) Table( Integer, Char(10) )
 *           Overflow N Chaining N : <Integer><Char(10)>
 *           Overflow Y Chaining N : <Integer> | <Char(10)>
 *           Overflow N Chaining Y :  - ������ ���� -
 *           Overflow Y Chaining Y : <Integer><Char | (10)>
 *           
 *  BuildRow     : Block�� �ؼ��Ͽ� Value�� Pointer�� �����ϴ� ���̴�.
 *                 ���� Import��, memcpy���� Block�� �ִ� ������
 *                 �ٷ� insert�ϴµ� ����� �� �ִ�.
 *
 *
 *
 *
 * - �Լ� �з�
 *    - interface
 *    scpfBeginExport
 *    scpfWrite
 *    scpfPrepareLob
 *    scpfWriteLob
 *    scpfEndExport
 * 
 *    scpfBeginImport
 *    scpfRead
 *    scpfReadLobLength
 *    scpfReadLob
 *    scpfEndImport
 * 
 *
 *
 * - Export��
 *    - Handle Alloc/Dealloc
 *    scpfInit4Export
 *    scpfDestroy4Export
 * 
 *    - ���� Open/Close
 *    scpfPrepareFile
 *    scpfCloseFile4Export
 * 
 *    - ���� �а� ����
 *    scpfWriteHeader
 *    scpfAllocSlot
 *    scpfWriteBasicColumn
 *    scpfWriteLobColumn
 *    scpfWriteBlock
 *    scpfPrepareBlock
 * 
 * 
 * 
 * 
 * - Import��
 *    - Handle ����
 *    scpfInit4Import
 *    scpfReleaseHeader
 *    scpfDestroy4Import
 * 
 *    - ���� Open/Close
 *    scpfOpenFile
 *    scpfFindFile
 *    scpfCloseFile4Import
 * 
 *    - ���� �а� ����
 *    scpfFindFirstBlock
 *    scpfShiftBlock
 *    scpfPreFetchBlock
 *    scpfReadHeader
 *    scpfReadBlock
 * 
 *    scpfBuildRow
 *    scpfReadBasicColumn
 *    scpfReadLobColumn
 *    scpfReadSlot
 *
 */

#include <ide.h>
#include <scpReq.h>
#include <scpModule.h>
#include <scpfModule.h>
#include <smuProperty.h>
#include <smiMain.h>
#include <smiMisc.h>
#include <smiDataPort.h>
#include <smErrorCode.h>

extern smiGlobalCallBackList gSmiGlobalCallBackList;

/**********************************************************************
 * Interface
 **********************************************************************/

//export�� �����Ѵ�.
static IDE_RC scpfBeginExport( idvSQL               * aStatistics,
                               void                ** aHandle,
                               smiDataPortHeader    * aCommonHeader,
                               SChar                * aObjName,
                               SChar                * aDirectory,
                               SLong                  aSplit );

//Row�� �ϳ� Write�Ѵ�.
static IDE_RC scpfWrite( idvSQL         * aStatistics,
                         void           * aHandle,
                         smiValue       * aValueList );

// Lob�� �� �غ� �Ѵ�.
static IDE_RC scpfPrepareLob( idvSQL         * aStatistics,
                              void           * aHandle,
                              UInt             aLobLength );


// Lob�� ����Ѵ�.
static IDE_RC scpfWriteLob( idvSQL         * aStatistics,
                            void           * aHandle,
                            UInt             aLobPieceLength,
                            UChar          * aLobPieceValue );

// Lob�� ����� �Ϸ��Ѵ�.
static IDE_RC scpfFinishLobWriting( idvSQL         * aStatistics,
                                    void           * aHandle );

// Export�� �����Ѵ�.
static IDE_RC scpfEndExport( idvSQL         * aStatistics,
                             void          ** aHandle );


//import�� �����Ѵ�. ����� �д´�.
static IDE_RC scpfBeginImport( idvSQL              * aStatistics,
                               void               ** aHandle,
                               smiDataPortHeader   * aCommonHeader,
                               SLong                 aFirstRowSeq,
                               SLong                 aLastRowSeq,
                               SChar               * aObjName,
                               SChar               * aDirectory );

//row���� �д´�.
static IDE_RC scpfRead( idvSQL         * aStatistics,
                        void           * aHandle,
                        smiRow4DP     ** aRows,
                        UInt           * aRowCount );

//Lob�� �� ���̸� ��ȯ�Ѵ�.
static IDE_RC scpfReadLobLength( idvSQL         * aStatistics,
                                 void           * aHandle,
                                 UInt           * aLength );

//Lob�� �д´�.
static IDE_RC scpfReadLob( idvSQL         * aStatistics,
                           void           * aHandle,
                           UInt           * aLobPieceLength,
                           UChar         ** aLobPieceValue );

// Lob�� �б⸦ �Ϸ��Ѵ�.
static IDE_RC scpfFinishLobReading( idvSQL         * aStatistics,
                                    void           * aHandle );
//import�� �����Ѵ�.
static IDE_RC scpfEndImport( idvSQL         * aStatistics,
                             void          ** aHandle );

scpModule scpfModule = {
    SMI_DATAPORT_TYPE_FILE,
    (scpBeginExport)      scpfBeginExport,
    (scpWrite)            scpfWrite,
    (scpPrepareLob)       scpfPrepareLob,
    (scpWriteLob)         scpfWriteLob,
    (scpFinishLobWriting) scpfFinishLobWriting,
    (scpEndExport)        scpfEndExport,
    (scpBeginImport)      scpfBeginImport,
    (scpRead)             scpfRead,
    (scpReadLobLength)    scpfReadLobLength,
    (scpReadLob)          scpfReadLob,
    (scpFinishLobReading) scpfFinishLobReading,
    (scpEndImport)        scpfEndImport
};



/**********************************************************************
 * Internal Functions for Exporting
 **********************************************************************/

// Export�ϱ� ���� Handle�� �⺻ ���� �Ѵ�.
static IDE_RC scpfInit4Export( idvSQL                * aStatistics,
                               scpfHandle           ** aHandle,
                               smiDataPortHeader     * aCommonHeader,
                               SChar                 * aObjName,
                               SChar                 * aDirectory,
                               SLong                   aSplit );

// Handle�� �����Ѵ�.
static IDE_RC scpfDestroy4Export( idvSQL         * aStatistics,
                                  scpfHandle    ** aHandle );

// ����� ������ �غ��Ѵ�.
static IDE_RC scpfPrepareFile( idvSQL         * aStatistics,
                               scpfHandle     * aHandle );

// File�� �ݴ´�.
static IDE_RC scpfCloseFile4Export( idvSQL         * aStatistics,
                                    scpfHandle     * aHandle );

// ��� ��Slot�� Ȯ���Ѵ�
static IDE_RC scpfAllocSlot( 
                idvSQL                  * aStatistics,
                iduFile                 * aFile,
                smiDataPortHeader       * aCommonHeader,
                scpfHeader              * aFileHeader,
                SLong                     aRowSeq,
                UInt                      aColumnChainingThreshold,
                idBool                  * aIsFirstSlotInRow,
                scpfBlockInfo           * aBlockInfo,
                idBool                    aWriteLobLen,
                UInt                      aNeedSize,
                scpfBlockBufferPosition * aBlockBufferPosition,
                scpfFilePosition        * aFilePosition,
                UInt                    * aAllocatedSlotValLen );

// BasicColumn�� write�Ѵ�.
static IDE_RC scpfWriteBasicColumn( 
            idvSQL                  * aStatistics,
            iduFile                 * aFile,
            smiDataPortHeader       * aCommonHeader,
            scpfHeader              * aFileHeader,
            scpfBlockInfo           * aBlockInfo,
            UInt                      aColumnChainingThreshold,
            SLong                     aRowSeq,
            smiValue                * aValue,
            idBool                  * aIsFirstSlotInRow,
            scpfBlockBufferPosition * aBlockBufferPosition,
            scpfFilePosition        * aFilePosition );

// LobColumn( LobLen + slot...)�� Write�Ѵ�.
static IDE_RC scpfWriteLobColumn( idvSQL         * aStatistics,
                                  scpfHandle     * aHandle,
                                  UInt             aLobPieceLength,
                                  UChar          * aLobPieceValue,
                                  UInt           * aRemainLobLength,
                                  UInt           * aAllocatedLobSlotSize );

// ���� Block�� �غ��Ѵ�.
static IDE_RC scpfPrepareBlock( idvSQL                  * aStatistics,
                                scpfHeader              * aFileHeader,
                                SLong                     aRowSeq,
                                scpfBlockInfo           * aBlockInfo,
                                scpfBlockBufferPosition * aBlockBufferPosition );

// ���� ������� Block�� ���Ͽ� ����Ѵ�.
static IDE_RC scpfWriteBlock( idvSQL                  * aStatistics, 
                              iduFile                 * aFile,
                              smiDataPortHeader       * aCommonHeader,
                              scpfHeader              * aFileHeader,
                              SLong                     aRowSeq,
                              scpfBlockInfo           * aBlockInfo,
                              scpfBlockBufferPosition * aBlockBufferPosition,
                              scpfFilePosition        * aFilePosition );

// Header�� File�� ����.
static IDE_RC scpfWriteHeader( idvSQL                     * aStatistics,
                               iduFile                    * aFile,
                               smiDataPortHeader          * aCommonHeader,
                               scpfHeader                 * aFileHeader,
                               scpfBlockBufferPosition    * aBlockBufferPosition,
                               scpfFilePosition           * aFilePosition );

/**********************************************************************
 * Internal Functions for Importing
 **********************************************************************/

// import�ϱ� ���� Handle�� �غ��Ѵ�.
static IDE_RC scpfInit4Import( idvSQL                * aStatistics,
                               scpfHandle           ** aHandle,
                               smiDataPortHeader     * aCommonHeader,
                               SLong                   aFirstRowSeq,
                               SLong                   aLastRowSeq,
                               SChar                 * aObjName,
                               SChar                 * aDirectory );

// Handle�� �����Ѵ�.
static IDE_RC scpfDestroy4Import( idvSQL         * aStatistics,
                                  scpfHandle    ** aHandle );
// �ش� File�� ����.
static IDE_RC scpfOpenFile( idBool         * aOpenFile,
                            iduFile        * aFile,
                            SChar          * aObjName, 
                            SChar          * aDirectory,
                            UInt             aIdx,
                            SChar          * aFileName );

// ��ǥ File�� ã�´�
static IDE_RC scpfFindFile( SChar            * aObjName,
                            SChar            * aDirectory,
                            UInt               aFileIdx,
                            SChar            * aFileName );

// File�� �ݴ´�.
static IDE_RC scpfCloseFile4Import( idvSQL         * aStatistics,
                                    scpfHandle     * aHandle );

// Header�� �д´�.
static IDE_RC scpfReadHeader( idvSQL                    * aStatistics,
                              iduFile                   * aFile,
                              smiDataPortHeader         * aCommonHeader,
                              scpfHeader                * aFileHeader,
                              scpfBlockBufferPosition   * aBlockBufferPosition,
                              scpfFilePosition          * aFilePosition );

// Header�� �дµ� ����ߴ� �޸𸮸� ����
static IDE_RC scpfReleaseHeader( idvSQL         * aStatistics,
                                 scpfHandle     * aHandle );

// File�κ��� Block�� �д´�.
static IDE_RC scpfReadBlock( idvSQL             * aStatistics,
                             iduFile            * aFile,
                             smiDataPortHeader  * aCommonHeader,
                             UInt                 aFileHeaderSize,
                             UInt                 aBlockHeaderSize,
                             UInt                 aBlockSize,
                             UInt                 aBlockID,
                             ULong              * aFileOffset,
                             scpfBlockInfo      * aBlockInfo );

// BinarySearch�� FirstRow�� ���� Block�� ã�´�.
static IDE_RC scpfFindFirstBlock( idvSQL                   * aStatistics,
                                  iduFile                  * aFile,
                                  smiDataPortHeader        * aCommonHeader,
                                  SLong                      aFirstRowSeqInFile,
                                  SLong                      aTargetRow,
                                  UInt                       aBlockCount,
                                  UInt                       aFileHeaderSize,
                                  UInt                       aBlockHeaderSize,
                                  UInt                       aBlockSize,
                                  scpfBlockInfo           ** aBlockMap,
                                  scpfBlockBufferPosition  * aBlockBufferPosition );

// BlockMap�� Shift���Ѽ� ���� ���� ���� Block�� BlockMap�� ���ʿ�
// ������ �̵���Ų��
static void scpfShiftBlock( UInt                aShiftBlockCount,
                            UInt                aBlockInfoCount,
                            scpfBlockInfo    ** aBlockMap );

// ������ Block�� �о Handle�� �ø���.
static IDE_RC scpfPreFetchBlock( idvSQL             * aStatistics,
                                 iduFile            * aFile,
                                 smiDataPortHeader  * aCommonHeader,
                                 SChar              * aFileName,
                                 SLong                aTargetRow,
                                 UInt                 aBlockCount,
                                 UInt                 aFileHeaderSize,
                                 UInt                 aBlockHeaderSize,
                                 UInt                 aBlockSize,
                                 UInt                 aNextBlockID,
                                 scpfFilePosition   * aFilePosition,
                                 UInt                 aBlockInfoCount,
                                 UInt                 aReadBlockCount,
                                 scpfBlockInfo     ** aBlockMap );

// Block�� Parsing�Ͽ� Row�� �����
static IDE_RC scpfBuildRow( idvSQL                    * aStatistics,
                            scpfHandle                * aHandle,
                            smiDataPortHeader         * aCommonHeader,
                            scpfHeader                * aFileHeader,
                            scpfBlockBufferPosition   * aBlockBufferPosition,
                            scpfBlockInfo            ** aBlockMap,
                            UChar                     * aChainedSlotBuffer,
                            UInt                      * aUsedChainedSlotBufSize,
                            UInt                      * aBuiltRowCount,
                            idBool                    * aHasSupplementLob,
                            smiRow4DP                 * aRowList );

// BasicColumn�� �д´�
static IDE_RC scpfReadBasicColumn( 
                    scpfBlockBufferPosition * aBlockBufferPosition,
                    scpfBlockInfo          ** aBlockMap,
                    UInt                      aBlockHeaderSize,
                    UChar                   * aChainedSlotBuffer,
                    UInt                    * aUsedChainedSlotBufferSize,
                    smiValue                * aValue );

// LobColumn�� �д´�.
static IDE_RC scpfReadLobColumn( scpfBlockBufferPosition  * aBlockBufferPosition,
                                 scpfBlockInfo           ** aBlockMap,
                                 UInt                       aBlockHeaderSize,
                                 smiValue                 * aValue );

// Slot�ϳ��� �д´�.
static IDE_RC scpfReadSlot( scpfBlockBufferPosition * aBlockBufferPosition,
                            scpfBlockInfo          ** aBlockMap,
                            UInt                      aBlockHeaderSize,
                            UInt                    * aSlotValLen,
                            void                   ** aSlotVal );

/**********************************************************************
 * Internal Functions for Utility
 **********************************************************************/

IDE_RC scpfDumpBlockBody( scpfBlockInfo  * aBlockInfo,
                          UInt             aBeginOffset,
                          UInt             aEndOffset,
                          SChar          * aOutBuf,
                          UInt             aOutSize );




smiDataPortHeaderColDesc  gScpfBlockHeaderEncColDesc[]=
{
    {
        (SChar*)"CHECK_SUM",
        SMI_DATAPORT_HEADER_OFFSETOF( scpfBlockInfo, mCheckSum ),
        SMI_DATAPORT_HEADER_SIZEOF( scpfBlockInfo, mCheckSum ),
        0, NULL,
        SMI_DATAPORT_HEADER_TYPE_INTEGER
    },
    {
        (SChar*)"BLOCK_ID",
        SMI_DATAPORT_HEADER_OFFSETOF( scpfBlockInfo, mBlockID ),
        SMI_DATAPORT_HEADER_SIZEOF( scpfBlockInfo, mBlockID ),
        0, NULL,
        SMI_DATAPORT_HEADER_TYPE_INTEGER
    },
    {
        (SChar*)"ROW_SEQ_OF_FIRST_SLOT",
        SMI_DATAPORT_HEADER_OFFSETOF( scpfBlockInfo, mRowSeqOfFirstSlot ),
        SMI_DATAPORT_HEADER_SIZEOF( scpfBlockInfo, mRowSeqOfFirstSlot ),
        0, NULL,
        SMI_DATAPORT_HEADER_TYPE_LONG
    },
    {
        (SChar*)"ROW_SEQ_OF_LAST_SLOT",
        SMI_DATAPORT_HEADER_OFFSETOF( scpfBlockInfo, mRowSeqOfLastSlot ),
        SMI_DATAPORT_HEADER_SIZEOF( scpfBlockInfo, mRowSeqOfLastSlot ),
        0, NULL,
        SMI_DATAPORT_HEADER_TYPE_LONG
    },
    {
        (SChar*)"FIRST_ROW_SLOTSEQ",
        SMI_DATAPORT_HEADER_OFFSETOF( scpfBlockInfo, mFirstRowSlotSeq ),
        SMI_DATAPORT_HEADER_SIZEOF( scpfBlockInfo, mFirstRowSlotSeq ),
        0, NULL,
        SMI_DATAPORT_HEADER_TYPE_INTEGER
    },
    {
        (SChar*)"FIRST_ROW_OFFSET",
        SMI_DATAPORT_HEADER_OFFSETOF( scpfBlockInfo, mFirstRowOffset ),
        SMI_DATAPORT_HEADER_SIZEOF( scpfBlockInfo, mFirstRowOffset ),
        0, NULL,
        SMI_DATAPORT_HEADER_TYPE_INTEGER
    },
    {
        (SChar*)"HAS_LASTCHAINED_SLOT",
        SMI_DATAPORT_HEADER_OFFSETOF( scpfBlockInfo, mHasLastChainedSlot ),
        SMI_DATAPORT_HEADER_SIZEOF( scpfBlockInfo, mHasLastChainedSlot ),
        0, NULL,
        SMI_DATAPORT_HEADER_TYPE_INTEGER
    },
    {
        (SChar*)"SLOT_COUNT",
        SMI_DATAPORT_HEADER_OFFSETOF( scpfBlockInfo, mSlotCount ),
        SMI_DATAPORT_HEADER_SIZEOF( scpfBlockInfo, mSlotCount ),
        0, NULL,
        SMI_DATAPORT_HEADER_TYPE_INTEGER
    },
    {
        NULL,
        0,
        0,
        0,0,
        0
    }
};

IDE_RC gScpfBlockHeaderValidation( void * aDesc, 
                                   UInt   aVersion,
                                   void * aHeader )
{
    smiDataPortHeaderDesc * sDesc;
    scpfBlockInfo         * sHeader;
    SChar                 * sTempBuf;
    UInt                    sState  = 0;

    sDesc   = (smiDataPortHeaderDesc*)aDesc;
    sHeader = (scpfBlockInfo*)aHeader;

    IDE_TEST_RAISE( 1 < sHeader->mHasLastChainedSlot,
                    ERR_ABORT_DATA_PORT_INTERNAL_ERROR );

    IDE_TEST_RAISE( sHeader->mFirstRowSlotSeq > sHeader->mSlotCount,
                    ERR_ABORT_DATA_PORT_INTERNAL_ERROR );

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_ABORT_DATA_PORT_INTERNAL_ERROR );
    {
        // Memory �Ҵ� ��, �ش� �޸𸮿� �� Header�� Dump�صӴϴ�
        // �׸��� Dump�� Header�� TRC�α׿� ����մϴ�.
        if( iduMemMgr::calloc( IDU_MEM_SM_SCP, 
                               1, 
                               IDE_DUMP_DEST_LIMIT, 
                               (void**)&sTempBuf )
            == IDE_SUCCESS )
        {
            sState = 1;
            (void) smiDataPort::dumpHeader( sDesc,
                                            aVersion,
                                            aHeader,
                                            SMI_DATAPORT_HEADER_FLAG_FULL,
                                            sTempBuf,
                                            IDE_DUMP_DEST_LIMIT );
            ideLog::log( IDE_SERVER_0, "%s", sTempBuf );
            sState = 0;
            (void)iduMemMgr::free( sTempBuf );
        }

        IDE_SET( ideSetErrorCode( 
                smERR_ABORT_DATA_PORT_INTERNAL_ERROR ) );
    }
    IDE_EXCEPTION_END;

    switch( sState )
    {
        case 1:
            IDE_ASSERT( iduMemMgr::free( sTempBuf ) == IDE_SUCCESS );
            sTempBuf = NULL;
        default:
            break;
    }

    return IDE_FAILURE;
}

smiDataPortHeaderDesc  gScpfBlockHeaderDesc[ SMI_DATAPORT_VERSION_COUNT ]=
{
    {
        (SChar*)"VERSION_NONE",
        0,
        NULL,
        NULL
    },
    {
        (SChar*)"BLOCK_HEADER",
        ID_SIZEOF( scpfBlockInfo),
        (smiDataPortHeaderColDesc*)gScpfBlockHeaderEncColDesc,
        gScpfBlockHeaderValidation
    }
};

smiDataPortHeaderColDesc  gScpfFileHeaderEncColDesc[]=
{
    {
        (SChar*)"FILE_HEADER_SIZE",
        SMI_DATAPORT_HEADER_OFFSETOF( scpfHeader, mFileHeaderSize ),
        SMI_DATAPORT_HEADER_SIZEOF( scpfHeader, mFileHeaderSize ),
        0, NULL,
        SMI_DATAPORT_HEADER_TYPE_INTEGER
    },
    {
        (SChar*)"BLOCK_HEADER_SIZE",
        SMI_DATAPORT_HEADER_OFFSETOF( scpfHeader, mBlockHeaderSize ),
        SMI_DATAPORT_HEADER_SIZEOF( scpfHeader, mBlockHeaderSize ),
        0, NULL,
        SMI_DATAPORT_HEADER_TYPE_INTEGER
    },
    {
        (SChar*)"BLOCK_SIZE",
        SMI_DATAPORT_HEADER_OFFSETOF( scpfHeader, mBlockSize ),
        SMI_DATAPORT_HEADER_SIZEOF( scpfHeader, mBlockSize ),
        0, NULL,
        SMI_DATAPORT_HEADER_TYPE_INTEGER
    },
    {
        (SChar*)"MAX_BLOCK_COUNT_PER_ROW",
        SMI_DATAPORT_HEADER_OFFSETOF( scpfHeader, mBlockInfoCount ),
        SMI_DATAPORT_HEADER_SIZEOF( scpfHeader, mBlockInfoCount ),
        0, NULL,
        SMI_DATAPORT_HEADER_TYPE_INTEGER
    },
    {
        (SChar*)"MAX_ROW_COUNT_IN_BLOCK",
        SMI_DATAPORT_HEADER_OFFSETOF( scpfHeader, mMaxRowCountInBlock ),
        SMI_DATAPORT_HEADER_SIZEOF( scpfHeader, mMaxRowCountInBlock ),
        0, NULL,
        SMI_DATAPORT_HEADER_TYPE_INTEGER
    },
    {
        (SChar*)"BLOCK_COUNT",
        SMI_DATAPORT_HEADER_OFFSETOF( scpfHeader, mBlockCount ),
        SMI_DATAPORT_HEADER_SIZEOF( scpfHeader, mBlockCount ),
        0, NULL,
        SMI_DATAPORT_HEADER_TYPE_INTEGER
    },
    {
        (SChar*)"ROW_COUNT",
        SMI_DATAPORT_HEADER_OFFSETOF( scpfHeader, mRowCount ),
        SMI_DATAPORT_HEADER_SIZEOF( scpfHeader, mRowCount ),
        0, NULL,
        SMI_DATAPORT_HEADER_TYPE_LONG
    },
    {
        (SChar*)"FIRST_ROW_SEQ",
        SMI_DATAPORT_HEADER_OFFSETOF( scpfHeader, mFirstRowSeqInFile ),
        SMI_DATAPORT_HEADER_SIZEOF( scpfHeader, mFirstRowSeqInFile ),
        0, NULL,
        SMI_DATAPORT_HEADER_TYPE_LONG
    },
    {
        (SChar*)"LAST_ROW_SEQ",
        SMI_DATAPORT_HEADER_OFFSETOF( scpfHeader, mLastRowSeqInFile ),
        SMI_DATAPORT_HEADER_SIZEOF( scpfHeader, mLastRowSeqInFile ),
        0, NULL,
        SMI_DATAPORT_HEADER_TYPE_LONG
    },
    {
        NULL,
        0,
        0,
        0,0,
        0
    }
};

IDE_RC gScpfFileHeaderValidation( void * aDesc, 
                                  UInt   aVersion,
                                  void * aHeader )
{
    smiDataPortHeaderDesc * sDesc;
    scpfHeader            * sHeader;
    SChar                 * sTempBuf;
    UInt                    sState  = 0;

    sDesc   = (smiDataPortHeaderDesc*)aDesc;
    sHeader = (scpfHeader*)aHeader;

    IDE_TEST_RAISE( sHeader->mBlockInfoCount == 0,
                    ERR_ABORT_DATA_PORT_INTERNAL_ERROR );

    IDE_TEST_RAISE( (sHeader->mMaxRowCountInBlock == 0) ||
                    (sHeader->mMaxRowCountInBlock > 1073741824 ),
                    ERR_ABORT_DATA_PORT_INTERNAL_ERROR );

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_ABORT_DATA_PORT_INTERNAL_ERROR );
    {
        // Memory �Ҵ� ��, �ش� �޸𸮿� �� Header�� Dump�صӴϴ�
        // �׸��� Dump�� Header�� TRC�α׿� ����մϴ�.
        if( iduMemMgr::calloc( IDU_MEM_SM_SCP, 
                               1, 
                               IDE_DUMP_DEST_LIMIT, 
                               (void**)&sTempBuf )
            == IDE_SUCCESS )
        {
            sState = 1;
            (void) smiDataPort::dumpHeader( sDesc,
                                            aVersion,
                                            aHeader,
                                            SMI_DATAPORT_HEADER_FLAG_FULL,
                                            sTempBuf,
                                            IDE_DUMP_DEST_LIMIT );
            ideLog::log( IDE_SERVER_0, "%s", sTempBuf );
            sState = 0;
            (void)iduMemMgr::free( sTempBuf );
        }

        IDE_SET( ideSetErrorCode( 
                smERR_ABORT_DATA_PORT_INTERNAL_ERROR ) );
    }
    IDE_EXCEPTION_END;

    switch( sState )
    {
        case 1:
            IDE_ASSERT( iduMemMgr::free( sTempBuf ) == IDE_SUCCESS );
            sTempBuf = NULL;
        default:
            break;
    }

    return IDE_FAILURE;
}
 
smiDataPortHeaderDesc  gScpfFileHeaderDesc[ SMI_DATAPORT_VERSION_COUNT ]=
{
    {
        (SChar*)"VERSION_NONE",
        0,
        NULL,
        NULL
    },
    {
        (SChar*)"FILE_HEADER",
        ID_SIZEOF( scpfHeader ),
        (smiDataPortHeaderColDesc*)gScpfFileHeaderEncColDesc,
        gScpfFileHeaderValidation
    }
};




/***********************************************************************
 *
 * Description :
 *  Export�� ���� �غ� �Ѵ�.
 *
 *  1) Object Handle�� �ʱ�ȭ�Ѵ�.
 *  2) ������ �����մϴ�.
 *  3) Header�� ����Ѵ�.
 *
 *  aStatistics         - [IN] �������
 *  aHandle             - [IN/OUT] Object Handle
 *  aCommonHeader       - [IN] File�� ���� ���� Header(Column,Table�� ����)
 *  aObjName            - [IN] Object �̸�
 *  aSplit              - [IN] Split�Ǵ� ���� Row ����
 *
 **********************************************************************/

static IDE_RC scpfBeginExport( idvSQL               * aStatistics,
                               void                ** aHandle,
                               smiDataPortHeader    * aCommonHeader,
                               SChar                * aObjName,
                               SChar                * aDirectory,
                               SLong                  aSplit )
{
    scpfHandle **sHandle;

    IDE_DASSERT( aHandle       != NULL );
    IDE_DASSERT( aCommonHeader != NULL );
    IDE_DASSERT( aObjName      != NULL );

    sHandle = (scpfHandle**)aHandle;

    // Handle�� �ʱ�ȭ�Ѵ�
    IDE_TEST( scpfInit4Export( aStatistics, 
                               sHandle, 
                               aCommonHeader,
                               aObjName,
                               aDirectory,
                               aSplit )
              != IDE_SUCCESS);

    // File�� �����Ѵ�.
    IDE_TEST( scpfPrepareFile( aStatistics, 
                               *sHandle )
              != IDE_SUCCESS);

    IDU_FIT_POINT( "1.PROJ-2059@scpfModule::scpfBeginExport" );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

//row�� �ϳ� write�Ѵ�.
static IDE_RC scpfWrite( idvSQL         * aStatistics,
                         void           * aHandle,
                         smiValue       * aValueList )
{
    scpfHandle               * sHandle;
    scpfBlockInfo            * sBlockInfo;
    scpfBlockBufferPosition  * sBlockBufferPosition;
    smiDataPortHeader        * sCommonHeader;
    UInt                     * sColumnMap;
    UInt                       sBeginBlockID = 0;
    UInt                       i = 0;

    IDE_DASSERT( aHandle    != NULL );
    IDE_DASSERT( aValueList != NULL );

    sHandle             = (scpfHandle*)aHandle;
    sCommonHeader       = sHandle->mCommonHandle.mHeader;
    sColumnMap          = sHandle->mColumnMap;
    sBlockInfo          = sHandle->mBlockMap[ 0 ]; // Export�ÿ��� 0���� �̿�
    sBlockBufferPosition= &(sHandle->mBlockBufferPosition);

    // File Split ���� üũ
    if( sHandle->mSplit != 0 )
    {
        if( sHandle->mSplit == sHandle->mFileHeader.mRowCount  )
        {
            IDE_TEST( scpfCloseFile4Export( aStatistics, sHandle )
                      != IDE_SUCCESS);
            IDE_TEST( scpfPrepareFile( aStatistics, sHandle ) 
                      != IDE_SUCCESS );
        }
    }

    sBeginBlockID = sBlockBufferPosition->mID;
    sHandle->mIsFirstSlotInRow = ID_TRUE;

    // BasicColumn�� ������ ���, Skip
    IDE_TEST_CONT( sCommonHeader->mBasicColumnCount == 0,
                    SKIP_WRITE_BASICCOLUMN );

    for( i=0; i < sCommonHeader->mBasicColumnCount; i++)
    {
        // N L N N L �� ���, ColumnMap(0,2,3,1,4)�� ���� 
        // 0,  2,3  ��°�� �ִ� BasicColumn�� ã�� Write�Ѵ�.
        IDE_TEST( scpfWriteBasicColumn( aStatistics,
                                         &sHandle->mFile,
                                         sHandle->mCommonHandle.mHeader,
                                         &sHandle->mFileHeader,
                                         sBlockInfo,
                                         sHandle->mColumnChainingThreshold,
                                         sHandle->mRowSeq,
                                         aValueList + sColumnMap[ i ],
                                         &sHandle->mIsFirstSlotInRow,
                                         &sHandle->mBlockBufferPosition,
                                         &sHandle->mFilePosition )
                  != IDE_SUCCESS );
    }

    // Row�� Block�� ���� ��ϵǸ�, 
    if( sBeginBlockID != sBlockBufferPosition->mID )
    {
        // firstBlock���� LastBlock���� Row�� ���� ��ϵǱ� ������, 
        // import�� ���� ��ϵ� ��ŭ�� BlockInfo�� �ʿ��ϴ�.
        sHandle->mFileHeader.mBlockInfoCount = 
            IDL_MAX( sHandle->mFileHeader.mBlockInfoCount,
                     sBlockBufferPosition->mID - sBeginBlockID + 1 );
    }

    // LobColumn�� ���ٸ�, BasicColumn�� ���� ��ϸ����ε� Row�� ����
    // ����� �Ϸ�� ���̴�.
    if( sCommonHeader->mLobColumnCount == 0 )
    {
        sHandle->mFileHeader.mRowCount ++;
        sHandle->mRowSeq               ++;
    }

    IDE_EXCEPTION_CONT( SKIP_WRITE_BASICCOLUMN );

    sHandle->mRemainLobColumnCount = sCommonHeader->mLobColumnCount;

    IDU_FIT_POINT( "1.PROJ-2059@scpfModule::scpfWrite" );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/***********************************************************************
 *
 * Description :
 *  Lob�� ����ϱ� ��, �� Length¥�� Lob�� ��ϵǴ��� ���� �˸���
 * �Լ��̴�.
 *  Block�� LobLength�� SlotValLen�� ����Ѵ�.
 *
 *  aStatistics         - [IN] �������
 *  aHandle             - [IN] Object Handle
 *  aLength             - [IN] ����� Lob�� ����
 *
 * - Lob Column
 *   [LobLen (4byte)][SlotValLen(1~4Byte)][SlotVal]
 *   Slot_Len�� ���� ���� SlotVal�� ũ��. �� LobLen�� 4Byte�� ���Ե��� �ʴ´�.
 *   ���⼭ LobLen�� Slot�� Chaining�� �Ͱ� ��� ����, �ϳ��� LobColumn
 * ��ü�� �����̴�. ���� Lob�� Chaining�Ǹ�, ù��° Slot���� ����Ѵ�.
 *
 **********************************************************************/
static IDE_RC scpfPrepareLob( idvSQL         * aStatistics,
                              void           * aHandle,
                              UInt             aLength )
{
    scpfHandle               * sHandle;
    UInt                       sAllocatedSlotValLen;

    IDE_DASSERT( aHandle != NULL );

    sHandle               = (scpfHandle*) aHandle;

    // Slot Ȯ��
    IDE_TEST( scpfAllocSlot( aStatistics, 
                             &sHandle->mFile,
                             sHandle->mCommonHandle.mHeader,
                             &sHandle->mFileHeader,
                             sHandle->mRowSeq,
                             sHandle->mColumnChainingThreshold,
                             &sHandle->mIsFirstSlotInRow,
                             sHandle->mBlockMap[ 0 ],
                             ID_TRUE, // write lob len
                             aLength, // need size
                             &sHandle->mBlockBufferPosition,
                             &sHandle->mFilePosition,
                             &sAllocatedSlotValLen )
              != IDE_SUCCESS );

    sHandle->mRemainLobLength      = aLength;
    sHandle->mAllocatedLobSlotSize = sAllocatedSlotValLen;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 *
 * Description :
 *  prepareLob ��, lob�� ���� write�� �����Ѵ�.
 *
 *  aStatistics         - [IN] �������
 *  aHandle             - [IN] Object Handle
 *  aLobPieceLength     - [IN] ����� Lob ������ ����
 *  aLobPieceValue      - [IN] ����� Lob ���� ��
 *
 **********************************************************************/
static IDE_RC scpfWriteLob( idvSQL         * aStatistics,
                            void           * aHandle,
                            UInt             aLobPieceLength,
                            UChar          * aLobPieceValue )
{
    scpfHandle    * sHandle;

    IDE_DASSERT( aHandle        != NULL );
    IDE_DASSERT( aLobPieceValue != NULL );

    sHandle = (scpfHandle*)aHandle;

    // ���� Lob�� ���̶� �����ߴ� �� ���� �� ���� LobPiece�� ���� �� ����.
    // �̴� �� ����� �߸� ��������� ��Ÿ���� ����̴�.
    IDE_TEST_RAISE( sHandle->mRemainLobLength < aLobPieceLength,
                    ERR_ABORT_PREPARE_WRITE_PROCOTOL );

    IDE_TEST( scpfWriteLobColumn( aStatistics,
                                  sHandle,
                                  aLobPieceLength,
                                  aLobPieceValue,
                                  &sHandle->mRemainLobLength,
                                  &sHandle->mAllocatedLobSlotSize )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_ABORT_PREPARE_WRITE_PROCOTOL);
    {
        IDE_SET( ideSetErrorCode( smERR_ABORT_PrePareWriteProtocol ) );
    }

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 *
 * Description :
 *  prepareLob, Writelob �� ���������� Writing�� �Ϸ��Ѵ�.
 *
 *  aStatistics         - [IN] �������
 *  aHandle             - [IN] Object Handle
 *  aLobPieceLength     - [IN] ����� Lob ������ ����
 *  aLobPieceValue      - [IN] ����� Lob ���� ��
 *
 **********************************************************************/
static IDE_RC scpfFinishLobWriting( idvSQL         * /*aStatistics*/,
                                    void           * aHandle )
{
    scpfHandle    * sHandle;

    IDE_DASSERT( aHandle        != NULL );

    sHandle = (scpfHandle*)aHandle;

    // Lob����� �Ϸ�Ǿ����. �׷��� ������ ����� �߸� ����� ���.
    IDE_TEST_RAISE( sHandle->mRemainLobLength > 0,
                    ERR_ABORT_FINISH_WRITE_PROCOTOL );

    sHandle->mRemainLobColumnCount --;

    // Row ��� �Ϸ�
    if( sHandle->mRemainLobColumnCount == 0 )
    {
        sHandle->mFileHeader.mRowCount ++;
        sHandle->mRowSeq               ++;
    }


    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_ABORT_FINISH_WRITE_PROCOTOL);
    {
        IDE_SET( ideSetErrorCode( smERR_ABORT_FinishWriteProtocol ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

/***********************************************************************
 *
 * Description :
 *  export�� �����Ѵ�.
 *
 *  aStatistics         - [IN] �������
 *  aHandle             - [IN] Object Handle
 *
 **********************************************************************/

static IDE_RC scpfEndExport( idvSQL         * aStatistics,
                             void          ** aHandle )
{
    scpfHandle    * sHandle;

    IDE_DASSERT( aHandle        != NULL );

    sHandle                                = (scpfHandle*)(*aHandle);
    sHandle->mFileHeader.mLastRowSeqInFile = sHandle->mRowSeq;

    IDE_TEST( scpfCloseFile4Export( aStatistics, 
                                    sHandle )
              != IDE_SUCCESS);

    IDE_TEST( scpfDestroy4Export( aStatistics,
                                  (scpfHandle**) aHandle )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 *
 * Description :
 *  import�� �����Ѵ�.
 *
 *  aStatistics         - [IN]  �������
 *  aHandle             - [IN]  Object Handle
 *  aCommonHeader       - [IN/OUT] File�κ��� ���� Header ����
 *  aFirstRowSeq        - [IN]  Import�� ù��° Row
 *  aFirstRowSeq        - [IN]  Export�� ù��° Row
 *  aObjName            - [IN]  Object �̸�
 *
 **********************************************************************/
static IDE_RC scpfBeginImport( idvSQL              * aStatistics,
                               void               ** aHandle,
                               smiDataPortHeader   * aCommonHeader,
                               SLong                 aFirstRowSeq,
                               SLong                 aLastRowSeq,
                               SChar               * aObjName,
                               SChar               * aDirectory )
{
    scpfHandle         * sHandle;

    IDE_DASSERT( aHandle                   != NULL );
    IDE_DASSERT( aCommonHeader             != NULL );
    IDE_DASSERT( aObjName                  != NULL );

    // Handle�� �ʱ�ȭ�ϰ� ������ Header�� �д´�.
    IDE_TEST( scpfInit4Import( aStatistics, 
                               (scpfHandle**)aHandle, 
                               aCommonHeader,
                               aFirstRowSeq,
                               aLastRowSeq,
                               aObjName,
                               aDirectory )
              != IDE_SUCCESS);

    sHandle = (scpfHandle*)*aHandle;

    IDE_TEST( scpfFindFirstBlock( aStatistics,
                                  &sHandle->mFile,
                                  sHandle->mCommonHandle.mHeader,
                                  sHandle->mFileHeader.mFirstRowSeqInFile,
                                  sHandle->mRowSeq,
                                  sHandle->mFileHeader.mBlockCount,
                                  sHandle->mFileHeader.mFileHeaderSize,
                                  sHandle->mFileHeader.mBlockHeaderSize,
                                  sHandle->mFileHeader.mBlockSize,
                                  sHandle->mBlockMap,
                                  &sHandle->mBlockBufferPosition )
              != IDE_SUCCESS );

    // Block�� ���� �д´�
    IDE_TEST( scpfPreFetchBlock( aStatistics,
                                 &sHandle->mFile,
                                 sHandle->mCommonHandle.mHeader,
                                 sHandle->mFileName,
                                 sHandle->mRowSeq,
                                 sHandle->mFileHeader.mBlockCount,
                                 sHandle->mFileHeader.mFileHeaderSize,
                                 sHandle->mFileHeader.mBlockHeaderSize,
                                 sHandle->mFileHeader.mBlockSize,
                                 sHandle->mBlockBufferPosition.mID,
                                 &sHandle->mFilePosition,
                                 sHandle->mFileHeader.mBlockInfoCount, //max
                                 sHandle->mFileHeader.mBlockInfoCount, //read
                                 sHandle->mBlockMap )
              != IDE_SUCCESS );


    IDU_FIT_POINT( "1.PROJ-2059@scpfModule::scpfBeginImport" );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 *
 * Description :
 *  row���� �д´�.
 *
 *  1) Block�� �д´�.
 *  1-1) ���� Build�� Row�� ���ٸ�, ó�� ������ �д°��̹Ƿ� ù��°
 *       Row�� ���� Block�� BinarySearch�� ã�� �ű⼭���� �����Ѵ�.
 *  1-2) ó���� �ƴ϶��, ���� Block�� �д´�.
 *  2) Build�� Block���� �Ѱ��ش�.
 *
 *  aStatistics         - [IN]  �������
 *  aHandle             - [IN]  Object Handle
 *  aRows               - [OUT] ���� Row
 *  aRowCount           - [OUT] ���� Row�� ����
 *
 **********************************************************************/
static IDE_RC scpfRead( idvSQL         * aStatistics,
                        void           * aHandle,
                        smiRow4DP     ** aRows,
                        UInt           * aRowCount )
{
    scpfHandle         * sHandle;
    smiDataPortHeader  * sCommonHeader;
    scpfHeader         * sFileHeader;
    scpfBlockInfo      * sBlockInfo;
    UInt                 sSkipRowCount;
    UInt                 sBuiltRowCount;
    idBool               sHasSupplementLob;
    SLong                sFirstRowSeqInBlock;

    IDE_DASSERT( aHandle   != NULL );
    IDE_DASSERT( aRows     != NULL );
    IDE_DASSERT( aRowCount != NULL );

    sHandle       = (scpfHandle*)aHandle;
    sCommonHeader = sHandle->mCommonHandle.mHeader;
    sFileHeader   = &(sHandle->mFileHeader);

    *aRowCount                     = 0;
    sHandle->mRemainLobColumnCount = 0;
    sHasSupplementLob              = ID_FALSE;

    // Row������ Limit�� �Ѿ ���
    IDE_TEST_CONT( sHandle->mRowSeq >= sHandle->mLimitRowSeq,
                    NO_MORE_ROW);

    scpfShiftBlock( sHandle->mBlockBufferPosition.mSeq,
                    sFileHeader->mBlockInfoCount,
                    sHandle->mBlockMap );

    IDE_TEST( scpfPreFetchBlock( aStatistics,
                                 &sHandle->mFile,
                                 sHandle->mCommonHandle.mHeader,
                                 sHandle->mFileName,
                                 sHandle->mRowSeq,
                                 sFileHeader->mBlockCount,
                                 sFileHeader->mFileHeaderSize,
                                 sFileHeader->mBlockHeaderSize,
                                 sFileHeader->mBlockSize,
                                 sHandle->mBlockBufferPosition.mID,
                                 &sHandle->mFilePosition,
                                 sHandle->mFileHeader.mBlockInfoCount,
                                 sHandle->mBlockBufferPosition.mSeq,
                                 sHandle->mBlockMap )
              != IDE_SUCCESS );

    sHandle->mBlockBufferPosition.mSeq = 0;

    // mRowSeqOfFirstSlot => Block�� ù��° Slot�� � RowSeq�� ���ϴ°�
    // sFirstRowSeqInBlock => �� Block�������� ���۵Ǵ� Row�� Seq
    //
    // ex)
    //    Block 0{ <Row0><Row1><Row2| }
    //    Blcok 1{ |Row2><Row3><Row4> }
    //      �� �������� Row2�� Block 0~1 ���̿� ���� ���(Overflow)�Ǿ� �ִ�.
    //
    //      Block1�� mRowSeqOfFirstSlot�� 2�̴�.
    //          �ֳ��ϸ�, Block1�� ù �κ��� Row2�� Slot�̱� �����̴�.
    //      Block1�� sFirstRowSeqInBlock�� 3�̴�.
    //          �ֳ��ϸ�, Block1���� ������ �����ϴ� Row�� Row3�̱� �����̴�.
    //
    sBlockInfo              = sHandle->mBlockMap[0];

    // Read�� �� Block�� FirstValue���� �о�� �ϱ� ������
    // Overflowed�� Slot��ŭ �ǳʶڴ�.
    sHandle->mBlockBufferPosition.mSlotSeq  = sBlockInfo->mFirstRowSlotSeq;
    sHandle->mBlockBufferPosition.mOffset   = sBlockInfo->mFirstRowOffset;

    if( 0 < sBlockInfo->mFirstRowSlotSeq )
    {
        // ���� Block���� Overflow�� Slot�� ������, 
        // FirstRow�� �� ���� Row�̴�.
        sFirstRowSeqInBlock = sBlockInfo->mRowSeqOfFirstSlot + 1;
    }
    else
    {
        sFirstRowSeqInBlock = sBlockInfo->mRowSeqOfFirstSlot ;
    }

    // Block �߰��� Row���� ���ϰ� �ִٸ�
    if( sFirstRowSeqInBlock < sHandle->mRowSeq )
    {
        //Skip�ؾ� �ϴ� Row�� ������ ����Ѵ�.
        sSkipRowCount = sHandle->mRowSeq - sFirstRowSeqInBlock;
    }
    else
    {
        sSkipRowCount = 0;
    }

    // Block�� Row�� Build�Ѵ�.
    IDE_TEST( scpfBuildRow( aStatistics, 
                            sHandle,
                            sCommonHeader,
                            sFileHeader,
                            &sHandle->mBlockBufferPosition,
                            sHandle->mBlockMap,
                            sHandle->mChainedSlotBuffer,
                            &sHandle->mUsedChainedSlotBufSize,
                            &sBuiltRowCount,
                            &sHasSupplementLob,
                            sHandle->mRowList )
              !=IDE_SUCCESS );


    //Built�� row�� ������, Block �ϳ��� ���� �ִ� Row�������� Ŭ �� ����.
    IDE_TEST_RAISE( sBuiltRowCount > sFileHeader->mMaxRowCountInBlock,
                    ERR_ABORT_CORRUPTED_BLOCK );

    // BuiltRowCount�� �ݵ�� SkipRowCount�� ���ų� Ŀ�� �Ѵ�.
    // SkipRowCount�� <Block�� Row��ȣ> - <Block�� ùRow��ȣ>��
    // <Block�� Row��ȣ>�� mRowSeq�� Block���� Row�� ����� ��
    // ����Ű��, <Block�� �� Row����>�� mBuiltRowCount���� ����
    // �� ���� �����̴�.
    IDE_TEST_RAISE( sBuiltRowCount < sSkipRowCount,
                    ERR_ABORT_CORRUPTED_BLOCK );

    // BuildRow�� �ϸ�, Block���� ��� Row�� �������ش�.
    // ���� �ʿ��� �κи� ����� ������ Skip�Ѵ�.
    *aRows            = &(sHandle->mRowList[ sSkipRowCount ]);
    *aRowCount        = sBuiltRowCount - sSkipRowCount;

    // Block �߰��� Row������ ���ϰ� �ִٸ�,
    // RowCount�� �ٿ��� �÷������� Row�� ������ ���δ�.
    if( (sHandle->mRowSeq + *aRowCount) > sHandle->mLimitRowSeq )
    {
        // First�� Last�� �������� �ʴ� �̻�, ������ ���� ������ ���� �� ����.
        IDE_TEST_RAISE( sHandle->mLimitRowSeq < sHandle->mRowSeq,
                        ERR_ABORT_CORRUPTED_BLOCK );

        *aRowCount = sHandle->mLimitRowSeq - sHandle->mRowSeq;
    }

    // ó���� Row������ �ݿ��Ѵ�. 
    sHandle->mRowSeq += sBuiltRowCount - sSkipRowCount;

    // �߰� Lobó���� ����� �Ѵٸ�
    if( sHasSupplementLob == ID_TRUE )
    {
        // ���� Row �ϳ��� ó�� �Ϸ� �ȵǾ���.
        sHandle->mRowSeq --;

        sHandle->mRemainLobColumnCount = sCommonHeader->mLobColumnCount;
        sHandle->mRemainLobLength      = 0;
    }

    // Row�� ���� ���
    IDE_EXCEPTION_CONT( NO_MORE_ROW );

    sHandle->mRemainLobLength = 0;

    IDU_FIT_POINT( "1.PROJ-2059@scpfModule::scpfRead" );

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_ABORT_CORRUPTED_BLOCK );
    {
        IDE_SET( ideSetErrorCode( 
                smERR_ABORT_CORRUPTED_BLOCK,
                sBlockInfo->mBlockID ) );
    }

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 *
 * Description :
 *  Lob�� �� ���̸� ��ȯ�Ѵ�.
 *
 *  aStatistics         - [IN]  �������
 *  aHandle             - [IN]  Object Handle
 *  aLength             - [OUT] Lob�� �� ����
 *
 **********************************************************************/
static IDE_RC scpfReadLobLength( idvSQL         * aStatistics,
                                 void           * aHandle,
                                 UInt           * aLength )
{
    scpfBlockInfo           * sBlockInfo;
    scpfHandle              * sHandle;
    scpfHeader              * sFileHeader;
    scpfBlockBufferPosition * sBlockBufferPosition;

    IDE_DASSERT( aHandle                   != NULL );
    IDE_DASSERT( aLength                   != NULL );

    sHandle              = (scpfHandle*)aHandle;
    sBlockBufferPosition = &(sHandle->mBlockBufferPosition);
    sFileHeader          = &(sHandle->mFileHeader);

    // ���Ӱ� LobColumn�� ���� �غ� �Ǿ���� ��.
    // �׷��� ������ ����� �߸� ����� ���.
    IDE_TEST_RAISE( ( sHandle->mRemainLobLength != 0 ) ||
                    ( sHandle->mRemainLobColumnCount == 0),
                    ERR_ABORT_PREPARE_WRITE_PROCOTOL );

    scpfShiftBlock( sHandle->mBlockBufferPosition.mSeq,
                    sFileHeader->mBlockInfoCount,
                    sHandle->mBlockMap );

    IDE_TEST( scpfPreFetchBlock( aStatistics,
                                 &sHandle->mFile,
                                 sHandle->mCommonHandle.mHeader,
                                 sHandle->mFileName,
                                 sHandle->mRowSeq,
                                 sFileHeader->mBlockCount,
                                 sFileHeader->mFileHeaderSize,
                                 sFileHeader->mBlockHeaderSize,
                                 sFileHeader->mBlockSize,
                                 sHandle->mBlockBufferPosition.mID,
                                 &sHandle->mFilePosition,
                                 sHandle->mFileHeader.mBlockInfoCount,
                                 sHandle->mBlockBufferPosition.mSeq,
                                 sHandle->mBlockMap )
              != IDE_SUCCESS );

    sHandle->mBlockBufferPosition.mSeq = 0;

    sBlockInfo = sHandle->mBlockMap[ sBlockBufferPosition->mSeq ];

    // LobLen�� ����
    SCPF_READ_UINT( sBlockInfo->mBlockPtr + sBlockBufferPosition->mOffset, 
                    &(sHandle->mRemainLobLength) );
    sBlockBufferPosition->mOffset += ID_SIZEOF(UInt);
    *aLength = sHandle->mRemainLobLength;

    IDU_FIT_POINT( "1.PROJ-2059@scpfModule::scpfReadLobLength" );

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_ABORT_PREPARE_WRITE_PROCOTOL);
    {
        IDE_SET( ideSetErrorCode( smERR_ABORT_PrePareWriteProtocol ) );
    }

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 *
 * Description :
 *  LobPiece�� �д´�.
 *
 *  aStatistics         - [IN]  �������
 *  aHandle             - [IN]  Object Handle
 *  aLobPieceLength     - [OUT] ���� Lob ������ ����
 *  aLobPieceValue      - [OUT] ���� Lob ������ Value
 *
 **********************************************************************/
static IDE_RC scpfReadLob( idvSQL         * aStatistics,
                           void           * aHandle,
                           UInt           * aLobPieceLength,
                           UChar         ** aLobPieceValue )
{
    scpfHandle              * sHandle;
    scpfHeader              * sFileHeader;
    scpfBlockBufferPosition * sBlockBufferPosition;

    IDE_DASSERT( aHandle                   != NULL );
    IDE_DASSERT( aLobPieceLength           != NULL );
    IDE_DASSERT( aLobPieceValue            != NULL );

    sHandle              = (scpfHandle*)aHandle;
    sFileHeader          = &(sHandle->mFileHeader);
    sBlockBufferPosition = &(sHandle->mBlockBufferPosition);

    //RemainLobLength�� 0�� ���� �� Module�� �߸� ����������̴�.
    IDE_TEST_RAISE( (sHandle->mRemainLobLength == 0) ||
                    (sHandle->mRemainLobColumnCount == 0),
                    ERR_ABORT_FINISH_WRITE_PROCOTOL );

    scpfShiftBlock( sHandle->mBlockBufferPosition.mSeq,
                    sFileHeader->mBlockInfoCount,
                    sHandle->mBlockMap );

    IDE_TEST( scpfPreFetchBlock( aStatistics,
                                 &sHandle->mFile,
                                 sHandle->mCommonHandle.mHeader,
                                 sHandle->mFileName,
                                 sHandle->mRowSeq,
                                 sFileHeader->mBlockCount,
                                 sFileHeader->mFileHeaderSize,
                                 sFileHeader->mBlockHeaderSize,
                                 sFileHeader->mBlockSize,
                                 sHandle->mBlockBufferPosition.mID,
                                 &sHandle->mFilePosition,
                                 sHandle->mFileHeader.mBlockInfoCount,
                                 sHandle->mBlockBufferPosition.mSeq,
                                 sHandle->mBlockMap )
              != IDE_SUCCESS );

    sHandle->mBlockBufferPosition.mSeq = 0;

    //Slot�ϳ� �д´�.
    IDE_TEST( scpfReadSlot( &sHandle->mBlockBufferPosition,
                            sHandle->mBlockMap,
                            sFileHeader->mBlockHeaderSize, 
                            aLobPieceLength,
                            (void**)aLobPieceValue )
              != IDE_SUCCESS);

    IDE_TEST_RAISE( sHandle->mRemainLobLength < *aLobPieceLength,
                    ERR_ABORT_CORRUPTED_BLOCK  );

    sHandle->mRemainLobLength -= *aLobPieceLength;



    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_ABORT_FINISH_WRITE_PROCOTOL);
    {
        IDE_SET( ideSetErrorCode( smERR_ABORT_FinishWriteProtocol ) );
    }
    IDE_EXCEPTION( ERR_ABORT_CORRUPTED_BLOCK );
    {
        IDE_SET( ideSetErrorCode( 
            smERR_ABORT_CORRUPTED_BLOCK,
            sHandle->mBlockMap[ sBlockBufferPosition->mSeq ]->mBlockID ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 *
 * Description :
 *  readLob �� ���������� Reading�� �Ϸ��Ѵ�.
 *
 *  aStatistics         - [IN] �������
 *  aHandle             - [IN] Object Handle
 *  aLobPieceLength     - [IN] ����� Lob ������ ����
 *  aLobPieceValue      - [IN] ����� Lob ���� ��
 *
 **********************************************************************/
static IDE_RC scpfFinishLobReading( idvSQL         * /*aStatistics*/,
                                    void           * aHandle )
{
    scpfHandle              * sHandle;

    IDE_DASSERT( aHandle != NULL );

    sHandle = (scpfHandle*)aHandle;

    // Lob Reading�� �Ϸ�Ǿ����. �׷��� ������ ����� �߸� ����� ���.
    IDE_TEST_RAISE( (sHandle->mRemainLobLength > 0) ||
                    (sHandle->mRemainLobColumnCount == 0),
                    ERR_ABORT_FINISH_WRITE_PROCOTOL );

    sHandle->mRemainLobColumnCount --;

    // �� Row�� ���� �бⰡ �Ϸ�Ǹ�
    if( sHandle->mRemainLobColumnCount == 0 )
    {
        sHandle->mRowSeq ++;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_ABORT_FINISH_WRITE_PROCOTOL);
    {
        IDE_SET( ideSetErrorCode( smERR_ABORT_FinishWriteProtocol ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}


/***********************************************************************
 *
 * Description :
 *  Import�� �����Ѵ�.
 *
 *  aStatistics         - [IN] �������
 *  aHandle             - [IN] Object Handle
 *
 **********************************************************************/

static IDE_RC scpfEndImport( idvSQL         * aStatistics,
                             void          ** aHandle )
{
    scpfHandle    * sHandle;

    IDE_DASSERT( aHandle        != NULL );

    sHandle                 = *((scpfHandle**)aHandle);

    IDE_TEST( scpfCloseFile4Import( aStatistics, 
                                    sHandle )
              != IDE_SUCCESS);

    IDE_TEST( scpfDestroy4Import( aStatistics,
                                  (scpfHandle**) aHandle )
              != IDE_SUCCESS );


    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}






/**********************************************************************
 * Internal Functions for Exporting
 **********************************************************************/

/***********************************************************************
 *
 * Description :
 *  Object Handle�� �ʱ�ȭ�Ѵ�.
 *
 *  aStatistics         - [IN] �������
 *  aHandle             - [IN/OUT] Object Handle
 *  aCommonHeader       - [IN] File�� ���� Header����(Column,Table�� ����)
 *  aObjName            - [IN] Object �̸�
 *  aSplit              - [IN] Split�Ǵ� ���� Row ����
 *
 **********************************************************************/
static IDE_RC scpfInit4Export( idvSQL                * /*aStatistics*/,
                               scpfHandle           ** aHandle,
                               smiDataPortHeader     * aCommonHeader,
                               SChar                 * aObjName,
                               SChar                 * aDirectory,
                               SLong                   aSplit )
{
    scpfHandle        * sHandle;
    scpfHeader        * sFileHeader;
    scpfBlockInfo     * sBlockInfo;
    UChar             * sAlignedBlockPtr;
    UInt                sBlockSize;
    UInt                sState = 0;
    UInt                i;

    IDE_DASSERT( aHandle       != NULL );
    IDE_DASSERT( aCommonHeader != NULL );
    IDE_DASSERT( aObjName      != NULL );


    // Writable property �̱⿡ �̸� Snapshot�� ���
    sBlockSize               = smuProperty::getDataPortFileBlockSize(); 

    // -----------------------------------------
    // Memory Allocation
    // -----------------------------------------
    IDE_TEST( iduMemMgr::calloc( IDU_MEM_SM_SCP, 
                                 1,
                                 ID_SIZEOF(scpfHandle),
                                 (void**)&(sHandle) )
              != IDE_SUCCESS );
    *aHandle    = sHandle;
    sFileHeader = &sHandle->mFileHeader;
    sState = 1;

    IDE_TEST( iduMemMgr::calloc( IDU_MEM_SM_SCP, 
                                 1,
                                 ID_SIZEOF(scpfBlockInfo),
                                 (void**)&(sHandle->mBlockBuffer) )
              != IDE_SUCCESS );
    sState = 2;

    IDE_TEST( iduMemMgr::calloc( IDU_MEM_SM_SCP, 
                                 1,
                                 ID_SIZEOF(scpfBlockInfo*),
                                 (void**)&(sHandle->mBlockMap) ) 
              != IDE_SUCCESS );
    sState = 3;

    IDE_TEST( iduMemMgr::calloc( IDU_MEM_SM_SCP, 
                                 1,
                                 sBlockSize + ID_MAX_DIO_PAGE_SIZE,
                                 (void**)&(sHandle->mAllocatedBlock) )
              != IDE_SUCCESS );
    sState = 4;

    IDE_TEST( iduMemMgr::calloc( IDU_MEM_SM_SCP, 
                                 ID_SIZEOF( UInt  ),
                                 aCommonHeader->mColumnCount,
                                 (void**)&(sHandle->mColumnMap ) )
              != IDE_SUCCESS );
    sState = 5;

    // DirectIO�� ���� Align�� ������� �մϴ�.
    sAlignedBlockPtr = (UChar*)idlOS::align( sHandle->mAllocatedBlock,
                                             ID_MAX_DIO_PAGE_SIZE );


    // --------------------------------------
    // Initialize attribute 
    // ------------------------------------- 
    // common header
    sHandle->mCommonHandle.mHeader   = aCommonHeader;

    // fileHeader
    sFileHeader->mBlockHeaderSize    = 
        smiDataPort::getEncodedHeaderSize(
            gScpfBlockHeaderDesc,
            aCommonHeader->mVersion );
    sFileHeader->mBlockSize          = sBlockSize;
    sFileHeader->mMaxRowCountInBlock = 1; //Block���� �ּ� 1���� Row�� ����
    sFileHeader->mBlockInfoCount     = SCPF_MIN_BLOCKINFO_COUNT;

    // runtime
    sHandle->mSplit                      = aSplit;
    sHandle->mColumnChainingThreshold    = 
        smuProperty::getExportColumnChainingThreshold();

    idlOS::strncpy( (SChar*)sHandle->mDirectory, 
                    (SChar*)aDirectory,
                    SM_MAX_FILE_NAME );

    idlOS::strncpy( (SChar*)sHandle->mObjName, 
                    (SChar*)aObjName,
                    SM_MAX_FILE_NAME );

    // Export�ÿ��� Block 1���� �����
    sBlockInfo            = &sHandle->mBlockBuffer[ 0 ];
    sBlockInfo->mBlockPtr = sAlignedBlockPtr;
    sHandle->mBlockMap[0] = sBlockInfo;

    // Column map
    for( i=0; i<aCommonHeader->mColumnCount; i++)
    {
        sHandle->mColumnMap[i] = 
            gSmiGlobalCallBackList.getColumnMapFromColumnHeaderFunc(
                aCommonHeader->mColumnHeader,
                i );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    switch( sState )
    {
        case 5:
            (void) iduMemMgr::free( sHandle->mColumnMap );
        case 4:
            (void) iduMemMgr::free( sHandle->mAllocatedBlock );
        case 3:
            (void) iduMemMgr::free( sHandle->mBlockMap ); 
        case 2:
            (void) iduMemMgr::free( sHandle->mBlockBuffer );
        case 1:
            (void) iduMemMgr::free( sHandle ); 
            *aHandle = NULL;
            break;
        default:
            break;
    }

    return IDE_FAILURE;
}

/***********************************************************************
 *
 * Description :
 *  ����� ObjectHandle�� �����Ѵ�.
 *
 *  aStatistics         - [IN] �������
 *  aHandle             - [IN] Object Handle
 *
 **********************************************************************/
static IDE_RC scpfDestroy4Export( idvSQL         * /*aStatistics*/,
                                  scpfHandle    ** aHandle )
{
    UInt sState = 5;

    sState = 4;
    IDE_TEST( iduMemMgr::free( (*aHandle)->mAllocatedBlock ) 
              != IDE_SUCCESS );

    sState = 3;
    IDE_TEST( iduMemMgr::free( (*aHandle)->mColumnMap )
              != IDE_SUCCESS );

    sState = 2;
    IDE_TEST( iduMemMgr::free( (*aHandle)->mBlockMap  ) 
              != IDE_SUCCESS );

    sState = 1;
    IDE_TEST( iduMemMgr::free( (*aHandle)->mBlockBuffer ) 
              != IDE_SUCCESS );

    sState = 0;
    IDE_TEST( iduMemMgr::free( *aHandle ) 
              != IDE_SUCCESS );
    *aHandle = NULL;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    switch( sState )
    {
        case 5:
            (void) iduMemMgr::free( (*aHandle)->mAllocatedBlock );
        case 4:
            (void) iduMemMgr::free( (*aHandle)->mColumnMap );
        case 3:
            (void) iduMemMgr::free( (*aHandle)->mBlockMap );
        case 2:
            (void) iduMemMgr::free( (*aHandle)->mBlockBuffer );
        case 1:
            (void) iduMemMgr::free( *aHandle );
            *aHandle = NULL;
            break;
        default:
            break;
    }

    return IDE_FAILURE;
}

/***********************************************************************
 *
 * Description :
 *  ����� ������ �غ��Ѵ� (Split)
 *
 * 1) �� ���Ͽ� �°� Header�� �����Ѵ�.
 * 2) �� ���Ͽ�createFile, writeHeader�� �����Ѵ�.
 *
 *  aStatistics         - [IN] �������
 *  aHandle             - [IN] Object Handle
 *
 **********************************************************************/
static IDE_RC scpfPrepareFile( idvSQL         * aStatistics,
                               scpfHandle     * aHandle )
{
    scpfBlockInfo           * sBlockInfo;
    scpfHeader              * sFileHeader;
    scpfBlockBufferPosition * sBlockBufferPosition;
    UInt                      sLen;
    UInt                      sFileState = 0;
    iduFile                 * sFile;


    IDE_DASSERT( aHandle                        != NULL );

    IDE_TEST_RAISE( (aHandle->mRemainLobLength > 0) ||
                    (aHandle->mRemainLobColumnCount > 0),
                    ERR_ABORT_FINISH_WRITE_PROCOTOL );

    // Export�ÿ��� 0�� Block�� ����Ѵ�
    sFileHeader          = &aHandle->mFileHeader;
    sBlockInfo           = aHandle->mBlockMap[0];
    sFile                = &(aHandle->mFile);
    sBlockBufferPosition = &(aHandle->mBlockBufferPosition);

    // Header reinitialize
    // persistent
    sFileHeader->mMaxRowCountInBlock  = 1;
    sFileHeader->mBlockCount          = 0;
    sFileHeader->mRowCount            = 0;
    sFileHeader->mFirstRowSeqInFile   = aHandle->mRowSeq;
    sFileHeader->mLastRowSeqInFile    = aHandle->mRowSeq;

    // runtime
    aHandle->mFilePosition.mOffset         = 0;
    aHandle->mRemainLobLength              = 0;
    aHandle->mRemainLobColumnCount         = 0;

    // Block
    sBlockBufferPosition->mID         = 0;
    sBlockBufferPosition->mOffset     = 0;
    sBlockBufferPosition->mSlotSeq    = 0;

    // �� Block�� ���� �ʱ�ȭ
    sBlockInfo->mRowSeqOfFirstSlot   = aHandle->mRowSeq;
    sBlockInfo->mRowSeqOfLastSlot    = aHandle->mRowSeq;
    sBlockInfo->mFirstRowSlotSeq     = 0;
    sBlockInfo->mFirstRowOffset      = SCPF_OFFSET_NULL;
    sBlockInfo->mHasLastChainedSlot  = ID_FALSE;
    sBlockInfo->mSlotCount           = 0;
    sBlockInfo->mCheckSum            = 0;
    sBlockInfo->mBlockID             = 0;

    // --------------------------------------
    // Create file 
    // ------------------------------------- 
    // Directory�� �����Ͱ� �ִµ�, /�� ������ ���� ��� �ٿ���
    sLen = idlOS::strnlen( aHandle->mDirectory, SM_MAX_FILE_NAME );
    if( 0 < sLen )
    {
        if( aHandle->mDirectory[ sLen - 1 ] != IDL_FILE_SEPARATOR )
        {
            idlVA::appendFormat( aHandle->mDirectory,
                                 SM_MAX_FILE_NAME,
                                 "%c",
                                 IDL_FILE_SEPARATOR );
        }
    }

    idlOS::snprintf((SChar*)aHandle->mFileName, 
                    SM_MAX_FILE_NAME,
                    "%s%s%c%"ID_UINT32_FMT"%s",
                    aHandle->mDirectory, 
                    aHandle->mObjName, 
                    SCPF_DPFILE_SEPARATOR,
                    aHandle->mFilePosition.mIdx,
                    SCPF_DPFILE_EXT);

    IDE_TEST( sFile->initialize( IDU_MEM_SM_SCP,
                                 1, /* Max Open FD Count */
                                 IDU_FIO_STAT_OFF,
                                 IDV_WAIT_INDEX_NULL )
              != IDE_SUCCESS);
    sFileState = 1;

    IDE_TEST( sFile->setFileName( aHandle->mFileName )
              != IDE_SUCCESS);
    IDE_TEST( sFile->createUntilSuccess( smLayerCallback::setEmergency )
              != IDE_SUCCESS );
    if( smuProperty::getDataPortDirectIOEnable() == 1 )
    {
        IDE_TEST( sFile->open( ID_TRUE )  //DIRECT_IO
                  != IDE_SUCCESS );
    }
    else
    {
        IDE_TEST( sFile->open( ID_FALSE ) //DIRECT_IO
                  != IDE_SUCCESS );
    }
    sFileState = 2;
    aHandle->mOpenFile = ID_TRUE;

    // Header�� ����Ѵ�.
    IDE_TEST( scpfWriteHeader( aStatistics,
                               &aHandle->mFile,
                               aHandle->mCommonHandle.mHeader,
                               &aHandle->mFileHeader,
                               &aHandle->mBlockBufferPosition,
                               &aHandle->mFilePosition )
              != IDE_SUCCESS);

    IDE_TEST( scpfPrepareBlock( aStatistics,
                                &aHandle->mFileHeader,
                                aHandle->mRowSeq,
                                sBlockInfo,
                                &aHandle->mBlockBufferPosition )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_ABORT_FINISH_WRITE_PROCOTOL);
    {
        IDE_SET( ideSetErrorCode( smERR_ABORT_FinishWriteProtocol ) );
    }
    IDE_EXCEPTION_END;

    switch( sFileState )
    {
        case 2:
            (void) sFile->close() ;
            aHandle->mOpenFile = ID_FALSE;
        case 1:
            (void) sFile->destroy();
            break;
        default:
            break;
    }

    return IDE_FAILURE;
}

/***********************************************************************
 *
 * Description :
 *  ���ݱ��� ����� ������ �ݴ´�.
 *
 *  aStatistics         - [IN] �������
 *  aHandle             - [IN] Object Handle
 *
 **********************************************************************/
static IDE_RC scpfCloseFile4Export( idvSQL         * aStatistics,
                                    scpfHandle     * aHandle )
{
    scpfBlockInfo   * sBlockInfo;
    scpfHeader      * sFileHeader;
    UInt              sFileState = 2;

    IDE_DASSERT( aHandle                        != NULL );

    // Export�ÿ��� 0�� Block�� ����Ѵ�
    sFileHeader  = &aHandle->mFileHeader;
    sBlockInfo   = aHandle->mBlockMap[0];

    sFileHeader->mLastRowSeqInFile = aHandle->mRowSeq;

    IDE_TEST( scpfWriteBlock( aStatistics, 
                              &aHandle->mFile,
                              aHandle->mCommonHandle.mHeader,
                              &aHandle->mFileHeader,
                              aHandle->mRowSeq,
                              sBlockInfo,
                              &aHandle->mBlockBufferPosition,
                              &aHandle->mFilePosition )
              != IDE_SUCCESS);

    IDE_TEST( scpfWriteHeader( aStatistics,
                               &aHandle->mFile,
                               aHandle->mCommonHandle.mHeader,
                               &aHandle->mFileHeader,
                               &aHandle->mBlockBufferPosition,
                               &aHandle->mFilePosition )
              != IDE_SUCCESS);

    IDE_TEST( aHandle->mFile.sync() != IDE_SUCCESS);

    sFileState = 1;
    IDE_TEST( aHandle->mFile.close() != IDE_SUCCESS);

    aHandle->mOpenFile = ID_FALSE;
    sFileState = 0;
    IDE_TEST( aHandle->mFile.destroy() != IDE_SUCCESS);

    aHandle->mFilePosition.mIdx ++;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    switch( sFileState )
    {
        case 2:
            (void) aHandle->mFile.close() ;
            aHandle->mOpenFile = ID_FALSE;
        case 1:
            (void) aHandle->mFile.destroy() ;
            break;
        default:
            break;
    }

    return IDE_FAILURE;
}


/***********************************************************************
 *
 * Description :
 *  Slot�� �� ������ Ȯ���Ѵ�.
 *
 *  aStatistics              - [IN] �������
 *  aFile                    - [IN] ����� file
 *  aFileHeader              - [IN] ����� File�� ���� ���� ���
 *  aRowSeq                  - [IN] ���� ������� Row ��ȣ
 *  aColumnChainingThreshold - [IN] Column�� �ڸ��� ���� ũ��
 *  aIsFirstSlotInRow        - [IN/OUT] Block�� ù��° Slot�� ����ϴ� ���ΰ�?
 *  aBlockInfo               - [IN] ����ϰ� �ִ� BlockInfo
 *  aWriteLobLen             - [IN] LobLength�� ����� ���ΰ�?
 *  aNeedSize                - [IN] Ȯ���Ϸ��� Į���� ũ��
 *  aBlockBufferPosition     - [OUT]    ������ Block���� Ŀ�� �ű�
 *  aFilePosition            - [IN/OUT] ����� ���� Block�� ��ġ�� ��������
 *                                      ����� Blockũ�⸸ŭ Offset�� �ű�
 *  aAllocatedSlotValLen     - [OUT]    Ȯ���� SlotVal ��Ͽ� ������ ũ��
 *
 **********************************************************************/
static IDE_RC scpfAllocSlot( 
                idvSQL                  * aStatistics,
                iduFile                 * aFile,
                smiDataPortHeader       * aCommonHeader,
                scpfHeader              * aFileHeader,
                SLong                     aRowSeq,
                UInt                      aColumnChainingThreshold,
                idBool                  * aIsFirstSlotInRow,
                scpfBlockInfo           * aBlockInfo,
                idBool                    aWriteLobLen,
                UInt                      aNeedSlotValLen,
                scpfBlockBufferPosition * aBlockBufferPosition,
                scpfFilePosition        * aFilePosition,
                UInt                    * aAllocatedSlotValLen )
{
    UInt     sBlockRemainSize;
    UInt     sNeedSize;

    IDE_DASSERT( aFile               != NULL );
    IDE_DASSERT( aFileHeader         != NULL );
    IDE_DASSERT( aFilePosition       != NULL );
    IDE_DASSERT( aBlockInfo          != NULL );
    IDE_DASSERT( aFilePosition       != NULL );
    IDE_DASSERT( aBlockBufferPosition!= NULL );

    sBlockRemainSize = aFileHeader->mBlockSize - aBlockBufferPosition->mOffset;

    sNeedSize = SCPF_GET_VARINT_SIZE(&aNeedSlotValLen) + aNeedSlotValLen;
    if( aWriteLobLen == ID_TRUE )
    {
        sNeedSize += SCPF_LOB_LEN_SIZE;
    }

    // ����� ������ �����ϸ�,
    // ���� ������ Threshold������ ���,
    // ���� Block�� ���� �� Block�� �غ��Ͽ� Threshold�̻��� ������ Ȯ���Ѵ�.
    if( (sNeedSize > sBlockRemainSize) &&
        (aColumnChainingThreshold > sBlockRemainSize ) )
    {
        IDE_TEST( scpfWriteBlock( aStatistics, 
                                  aFile,
                                  aCommonHeader,
                                  aFileHeader,
                                  aRowSeq,
                                  aBlockInfo,
                                  aBlockBufferPosition,
                                  aFilePosition )
                  != IDE_SUCCESS );

        IDE_TEST( scpfPrepareBlock( aStatistics, 
                                    aFileHeader,
                                    aRowSeq,
                                    aBlockInfo,
                                    aBlockBufferPosition )
                  != IDE_SUCCESS );

        sBlockRemainSize = 
            aFileHeader->mBlockSize - aBlockBufferPosition->mOffset;

        //�� Block�� ���������� �ּ����� ������ Ȯ���Ǿ�� ��
        IDE_TEST_RAISE( aColumnChainingThreshold > sBlockRemainSize,
                        ERR_ABORT_CORRUPTED_BLOCK  );
    }

    // �̹��� Row�� ���� ù ����̸�
    if( *aIsFirstSlotInRow == ID_TRUE )
    {
        *aIsFirstSlotInRow = ID_FALSE;

        // �� Block������ ù��° Row�̱⵵ �ϴٸ�
        if( aBlockInfo->mFirstRowOffset == SCPF_OFFSET_NULL )
        {
            aBlockInfo->mFirstRowOffset  = aBlockBufferPosition->mOffset;
            aBlockInfo->mFirstRowSlotSeq = aBlockBufferPosition->mSlotSeq ;
        }
    }

    // Slot �ϳ��� Ȯ����
    aBlockInfo->mSlotCount ++;
    aBlockBufferPosition->mSlotSeq ++;



    // LobLen ���
    if( aWriteLobLen == ID_TRUE )
    {
        SCPF_WRITE_UINT( &aNeedSlotValLen, 
                         aBlockInfo->mBlockPtr 
                             + aBlockBufferPosition->mOffset );

        aBlockBufferPosition->mOffset += SCPF_LOB_LEN_SIZE;
        sNeedSize                     -= SCPF_LOB_LEN_SIZE;

        sBlockRemainSize = 
            aFileHeader->mBlockSize - aBlockBufferPosition->mOffset;
    }



    //SlotValLen ���
    // ������ ������ ���
    if( sBlockRemainSize < sNeedSize )
    {
        *aAllocatedSlotValLen = SCPF_GET_SLOT_VAL_LEN( &sBlockRemainSize );
    }
    else
    {
        //�˳��� ���
        *aAllocatedSlotValLen = aNeedSlotValLen;
    }

    SCPF_WRITE_VARINT( aAllocatedSlotValLen,
                       aBlockInfo->mBlockPtr 
                           + aBlockBufferPosition->mOffset );
    aBlockBufferPosition->mOffset += 
                       SCPF_GET_VARINT_SIZE( aAllocatedSlotValLen );

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_ABORT_CORRUPTED_BLOCK );
    {
        IDE_SET( ideSetErrorCode( 
                smERR_ABORT_CORRUPTED_BLOCK,
                aBlockInfo->mBlockID ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 *
 * Description :
 *  Slot �ϳ��� Block�� ����Ѵ�.
 *  Chaining, overflow, Block�� ���� File�� ���� ��ϵ� �����Ѵ�.
 *
 *  aStatistics             - [IN]  �������
 *  aFile                   - [IN]  ����� ����
 *  aFileHeader             - [IN]  ���� ��� ����
 *  aBlockInfo              - [IN]  ����� Block�� ���� Info
 *  aBlockSize              - [IN]  �� �ϳ��� ũ��
 *  aColumnChainingThreshold- [IN]  Slot Chaining�� �����ϴ� Threshold
 *  aRowSeq                 - [IN]  ���� ����ϴ� Row�� ��ȣ
 *  aValue                  - [IN]  ����� Slot 
 *  aIsFirstSlotInRow       - [IN/OUT] Row�� ù��° Slot �� ����ϴ� ���ΰ�?
 *  aBlockBufferPosition    - [IN/OUT] ����ϰ� Position�� �ű��.
 *  aFilePosition           - [IN/OUT] ����ϰ� Position�� �ű��.
 * 
 * Slot �� ����
 * - VarInt (Length ��Ͽ� �̿��)
 *      0~63        [0 | len ]
 *     64~16383     [1 | len>>8  & 63][len ]
 *  16384~4194303   [2 | len>>16 & 63][len>>8 & 255][len & 255]
 *4194304~1073741824[3 | len>>24 & 63][len>>16 & 255][len>>8 & 255][len & 255]
 *
 * - BasicColumn
 *   [Slot_Len:VarInt(1~4Byte)][Body]
 * 
 *   Slot_Len�� Length��, ���� Body�� ũ���̴�. ���� Chaining�ž��� ���, 
 * �� Block�� ��ϵ� Length�� ��ϵȴ�.
 *   BasicColumn�� LobLength�� 0���� ����, ��ϵ��� �ʴ´�.
 **********************************************************************/

static IDE_RC scpfWriteBasicColumn( 
            idvSQL                  * aStatistics,
            iduFile                 * aFile,
            smiDataPortHeader       * aCommonHeader,
            scpfHeader              * aFileHeader,
            scpfBlockInfo           * aBlockInfo,
            UInt                      aColumnChainingThreshold,
            SLong                     aRowSeq,
            smiValue                * aValue,
            idBool                  * aIsFirstSlotInRow,
            scpfBlockBufferPosition * aBlockBufferPosition,
            scpfFilePosition        * aFilePosition )
{
    UInt              sRemainLength;
    UChar           * sRemainValue;
    UInt              sAllocatedSlotValLen;
    idBool            sDone = ID_FALSE;

    IDE_DASSERT( aFile                    != NULL );
    IDE_DASSERT( aFileHeader              != NULL );
    IDE_DASSERT( aBlockInfo               != NULL );
    IDE_DASSERT( aIsFirstSlotInRow        != NULL );
    IDE_DASSERT( aValue                   != NULL );
    IDE_DASSERT( aBlockBufferPosition     != NULL );
    IDE_DASSERT( aFilePosition            != NULL );

    // Export�ÿ��� 0�� Block�� ����Ѵ�
    sRemainLength = aValue->length;
    sRemainValue  = (UChar*)aValue->value;

    while( sDone == ID_FALSE )
    {
        // Slot Ȯ��
        IDE_TEST( scpfAllocSlot( aStatistics, 
                                 aFile,
                                 aCommonHeader,
                                 aFileHeader,
                                 aRowSeq,
                                 aColumnChainingThreshold,
                                 aIsFirstSlotInRow,
                                 aBlockInfo,
                                 ID_FALSE,       // write lob len
                                 sRemainLength,  // need size
                                 aBlockBufferPosition,
                                 aFilePosition,
                                 &sAllocatedSlotValLen )
                  != IDE_SUCCESS );

        // Write Slot Val
        idlOS::memcpy( aBlockInfo->mBlockPtr + aBlockBufferPosition->mOffset,
                       sRemainValue,
                       sAllocatedSlotValLen );
        aBlockBufferPosition->mOffset   += sAllocatedSlotValLen;
        sRemainValue                    += sAllocatedSlotValLen;
        sRemainLength                   -= sAllocatedSlotValLen;

        // ���� ����� �����Ͱ� ���Ҵٸ� Chaining
        if( 0 < sRemainLength )
        {
            // Block�� ���� ������ ���� ���¿��� �Ѵ�.
            IDE_TEST_RAISE( aFileHeader->mBlockSize 
                                - aBlockBufferPosition->mOffset 
                                >= SCPF_GET_VARINT_MAXSIZE,
                            ERR_ABORT_CORRUPTED_BLOCK  );
            aBlockInfo->mHasLastChainedSlot = ID_TRUE;
        }
        else
        {
            sDone = ID_TRUE;
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_ABORT_CORRUPTED_BLOCK );
    {
        IDE_SET( ideSetErrorCode( 
                    smERR_ABORT_CORRUPTED_BLOCK,
                    aBlockInfo->mBlockID ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 *
 * Description :
 *  LobValue�� Block�� ����Ѵ�.
 *
 *  aStatistics          - [IN]     �������
 *  aHandle              - [IN]     Object Handle
 *  aLobPieceLength      - [IN]     ����� Lob���� ����
 *  aLobPieceValue       - [IN]     ����� Lob����
 *  aRemainLobLength     - [IN/OUT] ���� Lob ��ü�� ũ��
 *  aAllocatedLobSlotSize- [IN/OUT] �� Block���� ����� Lob ��� ������ ũ��
 *
 * - Lob Column
 *   [LobLen (4byte)][Slot_Len(1~4Byte)][Body]
 *   Slot_Len�� ���� ���� Body�� ũ��. �� LobLen�� 4Byte�� ���Ե��� �ʴ´�.
 *   ���⼭ LobLen�� Slot �� Chaining�� �Ͱ� ��� ����, �ϳ��� LobColumn
 * ��ü�� �����̴�. ���� Lob�� Chaining�Ǹ�, ù��° Slot ���� ����Ѵ�.
 *
 ************************************************************************/

static IDE_RC scpfWriteLobColumn( idvSQL         * aStatistics,
                                  scpfHandle     * aHandle,
                                  UInt             aLobPieceLength,
                                  UChar          * aLobPieceValue,
                                  UInt           * aRemainLobLength,
                                  UInt           * aAllocatedLobSlotValLen )
{
    scpfBlockBufferPosition * sBlockBufferPosition;
    scpfBlockInfo           * sBlockInfo;
    UInt                      sBodySize;
    UInt                      sRemainLobPieceLength;
    UChar                   * sRemainLobPieceValue;

    IDE_DASSERT( aHandle         != NULL );

    // Export�ÿ��� 0�� Block�� ����Ѵ�
    sBlockInfo              = aHandle->mBlockMap[0];
    sBlockBufferPosition    = &(aHandle->mBlockBufferPosition);

    sRemainLobPieceLength = aLobPieceLength;
    sRemainLobPieceValue  = aLobPieceValue;

    // �� LobPiece�� ��� ����Ҷ����� �ݺ��Ѵ�.
    while( 0 < sRemainLobPieceLength )
    {
        //Write Lob piece
        sBodySize = IDL_MIN( sRemainLobPieceLength,*aAllocatedLobSlotValLen );
        idlOS::memcpy( sBlockInfo->mBlockPtr + sBlockBufferPosition->mOffset,
                       sRemainLobPieceValue,
                       sBodySize );
        sBlockBufferPosition->mOffset   += sBodySize;
        sRemainLobPieceValue            += sBodySize;
        sRemainLobPieceLength           -= sBodySize;
        *aRemainLobLength               -= sBodySize;
        *aAllocatedLobSlotValLen        -= sBodySize;

        // Slot�ϳ� �ٽ�µ�, ���� ����� Lob�� ������
        // �� Slot Ȯ��
        if( ( 0 == *aAllocatedLobSlotValLen ) &&
            ( 0 <  *aRemainLobLength  ) )
        {
            sBlockInfo->mHasLastChainedSlot = ID_TRUE;

            // Slot Ȯ��
            IDE_TEST( scpfAllocSlot( 
                    aStatistics, 
                    &aHandle->mFile,
                    aHandle->mCommonHandle.mHeader,
                    &aHandle->mFileHeader,
                    aHandle->mRowSeq,
                    aHandle->mColumnChainingThreshold,
                    &aHandle->mIsFirstSlotInRow,
                    sBlockInfo,
                    ID_FALSE,          // write lob len
                    *aRemainLobLength, // need size
                    &aHandle->mBlockBufferPosition,
                    &aHandle->mFilePosition,
                    aAllocatedLobSlotValLen )
                != IDE_SUCCESS );
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/***********************************************************************
 *
 * Description :
 *  Block�� File�� Write�Ѵ�.
 *
 *  aStatistics         - [IN] �������
 *  aFile               - [IN] ����� file
 *  aCommonHeader       - [IN] �������
 *  aFileHeader         - [IN] ����� File�� ���� ���� ���
 *  aRowSeq             - [IN] ���� ������� Row ��ȣ
 *  aBlockInfo          - [IN] ����� Block
 *  aBlockBufferPosition- [OUT]    ������ Block���� Ŀ�� �ű�
 *  aFilePosition       - [IN/OUT] ����� ���� Block�� ��ġ�� ��������
 *                                 ����� Blockũ�⸸ŭ Offset�� �ű�
 *
 **********************************************************************/
static IDE_RC scpfWriteBlock( idvSQL                  * aStatistics,
                              iduFile                 * aFile,
                              smiDataPortHeader       * aCommonHeader,
                              scpfHeader              * aFileHeader,
                              SLong                     aRowSeq,
                              scpfBlockInfo           * aBlockInfo,
                              scpfBlockBufferPosition * aBlockBufferPosition,
                              scpfFilePosition        * aFilePosition )
{
    UInt            sBlockOffset;
#if defined(DEBUG)
    SChar         * sTempBuf;
    UInt i;
#endif

    IDE_DASSERT( aFile         != NULL );
    IDE_DASSERT( aFileHeader   != NULL );
    IDE_DASSERT( aFilePosition != NULL );
    IDE_DASSERT( aBlockInfo    != NULL );

    // �� Block�������� ���۵� Row�� ���� ���, 
    // FirstRowOffset�� �������� �ʾ� NULL�̴�.
    if( aBlockInfo->mFirstRowOffset == SCPF_OFFSET_NULL )
    {
        // ���� Block���� ���� �Ѿ�� Slot �� �ִ�
        aBlockInfo->mFirstRowSlotSeq = aBlockInfo->mSlotCount ;
    }

    aBlockInfo->mRowSeqOfLastSlot    = aRowSeq;
    aFileHeader->mMaxRowCountInBlock =
        IDL_MAX( aFileHeader->mMaxRowCountInBlock ,
                 (aBlockInfo->mRowSeqOfLastSlot
                  - aBlockInfo->mRowSeqOfFirstSlot) + 1 );
    aFileHeader->mBlockCount ++;

#if defined(DEBUG)
    for( i=0; i< aFileHeader->mBlockHeaderSize; i++)
    {
        // Block�� Header �κ��� ���⼭ ����ϱ� ������ 0���� �ʱ�ȭ �Ǿ�
        // �־�� �Ѵ�.

        if( aBlockInfo->mBlockPtr[i] != 0 )
        {
            if( iduMemMgr::calloc( IDU_MEM_SM_SCP, 
                                   1, 
                                   IDE_DUMP_DEST_LIMIT, 
                                   (void**)&sTempBuf )
                == IDE_SUCCESS )
            {
                idlOS::snprintf( sTempBuf,
                                 IDE_DUMP_DEST_LIMIT,
                                 "----------- DumpBlock  ----------\n" );
                IDE_TEST( smiDataPort::dumpHeader( 
                        gScpfBlockHeaderDesc,
                        aCommonHeader->mVersion,
                        (void*)aBlockInfo,
                        SMI_DATAPORT_HEADER_FLAG_FULL,
                        sTempBuf,
                        IDE_DUMP_DEST_LIMIT )
                    != IDE_SUCCESS );
                ideLog::log( IDE_SERVER_0, "%s", sTempBuf );

                sTempBuf[0] = '\0';
                if( scpfDumpBlockBody( 
                        aBlockInfo, 
                        0,                             // begin offset
                        aFileHeader->mBlockHeaderSize, // end offset
                        sTempBuf, 
                        IDE_DUMP_DEST_LIMIT )
                    == IDE_SUCCESS )
                {
                    ideLog::log( IDE_SERVER_0, "%s", sTempBuf );
                }

                (void)iduMemMgr::free( sTempBuf );
            }
            IDE_ASSERT(0);
        }
    }
#endif

    // Body�� ���� Checksum ���
    aBlockInfo->mCheckSum = smuUtility::foldBinary( 
        aBlockInfo->mBlockPtr + aFileHeader->mBlockHeaderSize,
        aFileHeader->mBlockSize - aFileHeader->mBlockHeaderSize );

    // Header�� Block�� ����.
    sBlockOffset = 0; // Block �� Offset
    IDE_TEST( smiDataPort::writeHeader(
            gScpfBlockHeaderDesc,
            aCommonHeader->mVersion,
            aBlockInfo,
            aBlockInfo->mBlockPtr,
            &sBlockOffset,
            aFileHeader->mBlockSize )
        != IDE_SUCCESS );

    IDE_TEST( aFile->write( aStatistics,
                            aFilePosition->mOffset,
                            aBlockInfo->mBlockPtr,
                            aFileHeader->mBlockSize )
              != IDE_SUCCESS);

    aBlockBufferPosition->mID  ++;
    aFilePosition->mOffset += aFileHeader->mBlockSize ;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/***********************************************************************
 *
 * Description :
 *  ���� Block�� �غ��Ѵ�.
 *
 *  aStatistics         - [IN] �������
 *  aFileHeader         - [IN] File������ ���� Header
 *  aRowSeq             - [IN] ���� ������� Row ��ȣ
 *  aBlockInfo          - [IN] ����ϴ� BlockBuffer 
 *  aBlockBufferPosition- [IN/OUT] Block�� ù Slot ��ġ�� Ŀ�� �̵�
 *
 **********************************************************************/
static IDE_RC scpfPrepareBlock( idvSQL                  * /*aStatistics*/,
                                scpfHeader              * aFileHeader,
                                SLong                     aRowSeq,
                                scpfBlockInfo           * aBlockInfo,
                                scpfBlockBufferPosition * aBlockBufferPosition )
{
    IDE_DASSERT( aFileHeader              != NULL );
    IDE_DASSERT( aBlockBufferPosition     != NULL );
    IDE_DASSERT( aBlockInfo               != NULL );

    IDE_TEST_RAISE( aBlockInfo->mSlotCount != aBlockBufferPosition->mSlotSeq ,
                    ERR_ABORT_CORRUPTED_BLOCK  );

    // Ŀ�� �̵�
    aBlockBufferPosition->mOffset   = aFileHeader->mBlockHeaderSize;
    aBlockBufferPosition->mSlotSeq  = 0;

    // �� Block�� ���� �ʱ�ȭ
    aBlockInfo->mRowSeqOfFirstSlot   = aRowSeq;
    aBlockInfo->mRowSeqOfLastSlot    = aRowSeq;
    aBlockInfo->mFirstRowSlotSeq     = 0;
    aBlockInfo->mFirstRowOffset      = SCPF_OFFSET_NULL;
    aBlockInfo->mHasLastChainedSlot  = ID_FALSE;
    aBlockInfo->mSlotCount           = 0;
    aBlockInfo->mCheckSum            = 0;
    aBlockInfo->mBlockID             = aBlockBufferPosition->mID;

    // PBT�� ���� �ϱ� ���� �ʱ�ȭ 
    idlOS::memset( aBlockInfo->mBlockPtr, 0, aFileHeader->mBlockSize );

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_ABORT_CORRUPTED_BLOCK );
    {
        IDE_SET( ideSetErrorCode( 
                smERR_ABORT_CORRUPTED_BLOCK,
                aBlockInfo->mBlockID ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 *
 * Description :
 *  Header�� File�� Write�Ѵ�.(BigEndian����)
 *
 *  1) Version�� �������� DATA_PORT_HEADER����
 *  2) Size Estimation
 *  3) Buffer�� ���
 *  4) File�� ���
 *
 *  aStatistics         - [IN] �������
 *  aFile               - [IN] ����� ����
 *  aCommonHeader       - [IN] ���� ���
 *  aFileHeader         - [IN] ���� ���
 *  aBlockBufferPosition- [OUT] Ŀ���� Header �ٷ� �������� �ű�
 *  aFilePosition       - [OUT] Ŀ���� Header �ٷ� �������� �ű�
 *
 **********************************************************************/
static IDE_RC scpfWriteHeader( idvSQL                     * aStatistics,
                               iduFile                    * aFile,
                               smiDataPortHeader          * aCommonHeader,
                               scpfHeader                 * aFileHeader,
                               scpfBlockBufferPosition    * aBlockBufferPosition,
                               scpfFilePosition           * aFilePosition )
{
    UInt                      sVersion;
    UInt                      sColumnCount;
    UInt                      sPartitionCount;
    smiDataPortHeaderDesc   * sCommonHeaderDesc;
    smiDataPortHeaderDesc   * sObjectHeaderDesc;
    smiDataPortHeaderDesc   * sColumnHeaderDesc;
    smiDataPortHeaderDesc   * sTableHeaderDesc;
    smiDataPortHeaderDesc   * sPartitionHeaderDesc;
    void                    * sObjectHeader;
    void                    * sTableHeader;
    void                    * sColumnHeader;
    void                    * sPartitionHeader;
    UInt                      sHeaderSize;
    UChar                   * sBuffer;
    UChar                   * sAlignedBufferPtr;
    UInt                      sBufferSize;
    UInt                      sBufferOffset = 0;
    UInt                      sState  = 0;
    UInt                      i;

    IDE_DASSERT( aFile                 != NULL );
    IDE_DASSERT( aCommonHeader         != NULL );
    IDE_DASSERT( aFileHeader           != NULL );
    IDE_DASSERT( aBlockBufferPosition  != NULL );
    IDE_DASSERT( aFilePosition         != NULL );

    sVersion    = aCommonHeader->mVersion;

    IDE_TEST_RAISE( SMI_DATAPORT_VERSION_LATEST < sVersion,
                    ERR_ABORT_DATA_PORT_VERSION );

    /********************************************************************
     * Select DATA_PORT_HEADER Desc
     *
     * Version�� �������� DATA_PORT_HEADER Desc ����
     ********************************************************************/
    sCommonHeaderDesc    = gSmiDataPortHeaderDesc;
    sObjectHeaderDesc    = gScpfFileHeaderDesc;
    sTableHeaderDesc     = (smiDataPortHeaderDesc*)
        gSmiGlobalCallBackList.getTableHeaderDescFunc();
    sColumnHeaderDesc    = (smiDataPortHeaderDesc*)
        gSmiGlobalCallBackList.getColumnHeaderDescFunc();
    sPartitionHeaderDesc = (smiDataPortHeaderDesc*)
        gSmiGlobalCallBackList.getPartitionHeaderDescFunc();

    sColumnCount         = aCommonHeader->mColumnCount;
    sPartitionCount      = aCommonHeader->mPartitionCount;

    sObjectHeader        = aFileHeader;
    sColumnHeader        = aCommonHeader->mColumnHeader;
    sTableHeader         = aCommonHeader->mTableHeader;
    sPartitionHeader     = aCommonHeader->mPartitionHeader;

    /*********************************************************************
     * Size Estimation
     *
     * Header�� Version +
     *          CommonHeader +
     *          scpfHeader +
     *          qsfTableInfo +
     *          qsfColumnInfo*ColCount + 
     *          qsfPartitionInfo*PartitionCount +
     * �� ���·� �����Ǹ�, ���� Header�� ������ ������ ũ�⵵ �̿�
     * ����.
     **********************************************************************/
    sBufferSize = 
        ID_SIZEOF(UInt) +
        smiDataPort::getEncodedHeaderSize( sCommonHeaderDesc, sVersion ) +
        smiDataPort::getEncodedHeaderSize( sObjectHeaderDesc, sVersion ) +
        smiDataPort::getEncodedHeaderSize( sTableHeaderDesc, sVersion ) +
        smiDataPort::getEncodedHeaderSize( sColumnHeaderDesc, sVersion ) * sColumnCount +
        smiDataPort::getEncodedHeaderSize( sPartitionHeaderDesc, sVersion ) * sPartitionCount;

    // DirectIO�� ���� Align�� ������� �մϴ�.
    sBufferSize = idlOS::align( sBufferSize, ID_MAX_DIO_PAGE_SIZE );

    aFileHeader->mFileHeaderSize = sBufferSize;

    IDE_TEST( iduMemMgr::calloc( IDU_MEM_SM_SCP, 
                                 1,
                                 sBufferSize + ID_MAX_DIO_PAGE_SIZE,
                                 (void**)&(sBuffer) )
              != IDE_SUCCESS );
    sAlignedBufferPtr = (UChar*)idlOS::align( sBuffer, ID_MAX_DIO_PAGE_SIZE );
    sState = 1;



    // Write
    sBufferOffset = 0;

    SCPF_WRITE_UINT( &sVersion, sAlignedBufferPtr + sBufferOffset );
    sBufferOffset += ID_SIZEOF( UInt );


    IDE_TEST( smiDataPort::writeHeader( sCommonHeaderDesc,
                                        sVersion,
                                        aCommonHeader,
                                        sAlignedBufferPtr,
                                        &sBufferOffset,
                                        sBufferSize )
              != IDE_SUCCESS );

    IDE_TEST( smiDataPort::writeHeader( sObjectHeaderDesc,
                                        sVersion,
                                        sObjectHeader,
                                        sAlignedBufferPtr,
                                        &sBufferOffset,
                                        sBufferSize )
              != IDE_SUCCESS );

    IDE_TEST( smiDataPort::writeHeader( sTableHeaderDesc,
                                        sVersion,
                                        sTableHeader,
                                        sAlignedBufferPtr,
                                        &sBufferOffset,
                                        sBufferSize )
              != IDE_SUCCESS );

    sHeaderSize = smiDataPort::getHeaderSize( sColumnHeaderDesc,
                                              sVersion );
    for( i=0; i<sColumnCount; i++ )
    {
        IDE_TEST( smiDataPort::writeHeader( sColumnHeaderDesc,
                                            sVersion,
                                            ((UChar*)sColumnHeader)
                                            + sHeaderSize * i,
                                            sAlignedBufferPtr,
                                            &sBufferOffset,
                                            sBufferSize )
                  != IDE_SUCCESS );
    }

    sHeaderSize = smiDataPort::getHeaderSize( sPartitionHeaderDesc,
                                              sVersion );
    for( i=0; i<sPartitionCount; i++ )
    {
        IDE_TEST( smiDataPort::writeHeader( sPartitionHeaderDesc,
                                            sVersion,
                                            ((UChar*)sPartitionHeader)
                                            + sHeaderSize * i,
                                            sAlignedBufferPtr,
                                            &sBufferOffset,
                                            sBufferSize )
                  != IDE_SUCCESS );
    }

    IDE_TEST( aFile->write( aStatistics,
                            0, // header offset
                            sAlignedBufferPtr,
                            sBufferSize )
              != IDE_SUCCESS);


    sState = 0;
    IDE_TEST( iduMemMgr::free( sBuffer ) != IDE_SUCCESS );

    aBlockBufferPosition->mSeq      = 0;
    aBlockBufferPosition->mID       = 0;
    aBlockBufferPosition->mOffset   = aFileHeader->mBlockHeaderSize;
    aBlockBufferPosition->mSlotSeq  = 0;
    aFilePosition->mOffset          = aFileHeader->mFileHeaderSize;

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_ABORT_DATA_PORT_VERSION );
    {
        IDE_SET( ideSetErrorCode( smERR_ABORT_VERSION_NOT_SUPPORTED,
                                  sVersion,
                                  SMI_DATAPORT_VERSION_LATEST ) );
    }
    IDE_EXCEPTION_END;

    switch( sState )
    {
        case 1:
            (void)iduMemMgr::free( sBuffer );
            break;
        default:
            break;
    }

    return IDE_FAILURE;
}








/**********************************************************************
 * Internal Functions for Importing
 **********************************************************************/

/***********************************************************************
 *
 * Description :
 *  Object Handle�� �ʱ�ȭ�Ѵ�.
 *
 *  aStatistics         - [IN] �������
 *  aCommonHeader       - [IN/OUT] File�κ��� ���� Header ����
 *  aFirstRowSeq        - [IN]  Import�� ù��° Row
 *  aLastRowSeq         - [IN]  Import�� ������ Row
 *  aObjName            - [IN]  Object �̸�
 *
 **********************************************************************/
static IDE_RC scpfInit4Import( idvSQL                * aStatistics,
                               scpfHandle           ** aHandle,
                               smiDataPortHeader     * aCommonHeader,
                               SLong                   aFirstRowSeq,
                               SLong                   aLastRowSeq,
                               SChar                 * aObjName,
                               SChar                 * aDirectory )
{
    scpfHandle        * sHandle;
    scpfBlockInfo     * sBlockInfo;
    UInt                sBlockSize;
    UInt                sAllocatedBlockInfoCount;
    UChar             * sAlignedBlockPtr;
    UInt                sState = 0;
    UInt                i;

    IDE_DASSERT( aHandle       != NULL );
    IDE_DASSERT( aCommonHeader != NULL );
    IDE_DASSERT( aObjName      != NULL );

    // -----------------------------------------
    // Handle Allocation
    // -----------------------------------------
    IDE_TEST( iduMemMgr::calloc( IDU_MEM_SM_SCP, 
                                 1,
                                 ID_SIZEOF(scpfHandle),
                                 (void**)&(sHandle) )
              != IDE_SUCCESS );
    *aHandle = sHandle;
    sState = 1;



    // --------------------------------------
    // Initialize basic attribute 
    // ------------------------------------- 
    sHandle->mBlockBufferPosition.mID        = 0;
    sHandle->mBlockBufferPosition.mOffset    = 0;
    sHandle->mBlockBufferPosition.mSlotSeq   = 0;
    sHandle->mFilePosition.mIdx              = 0;
    sHandle->mFilePosition.mOffset           = 0;

    sHandle->mSplit                    = 0;  // Import�� ��� ����
    sHandle->mColumnChainingThreshold  = 0;  // Import�� ��� ����
    sHandle->mRemainLobLength          = 0; 
    sHandle->mRemainLobColumnCount     = 0;

    sHandle->mCommonHandle.mHeader   = aCommonHeader;

    idlOS::strncpy( (SChar*)sHandle->mDirectory, 
                    (SChar*)aDirectory,
                    SM_MAX_FILE_NAME );

    idlOS::strncpy( (SChar*)sHandle->mObjName,
                    (SChar*)aObjName,
                    SM_MAX_FILE_NAME );

    // --------------------------------------
    // Open file and Load header
    // ------------------------------------- 
    // File�� ����.
    IDE_TEST( scpfOpenFile( &(sHandle->mOpenFile),
                            &(sHandle->mFile),
                            sHandle->mObjName, 
                            sHandle->mDirectory, 
                            sHandle->mFilePosition.mIdx,
                            sHandle->mFileName )
              != IDE_SUCCESS);


    // Header�� �ҷ��´�
    IDE_TEST( scpfReadHeader( aStatistics, 
                              &(sHandle->mFile),
                              sHandle->mCommonHandle.mHeader,
                              &sHandle->mFileHeader,
                              &sHandle->mBlockBufferPosition,
                              &sHandle->mFilePosition )
              != IDE_SUCCESS);

    // Row������ ������
    if( sHandle->mFileHeader.mLastRowSeqInFile < aLastRowSeq)
    {
        sHandle->mLimitRowSeq = sHandle->mFileHeader.mLastRowSeqInFile;
    }
    else
    {
        sHandle->mLimitRowSeq = aLastRowSeq;
    }

    // FirstRowSeq�� �Ѱ躸�� ũ�� �ȵȴ�.
    if( sHandle->mLimitRowSeq < aFirstRowSeq )
    {
        aFirstRowSeq = sHandle->mLimitRowSeq;
    }

    if( aFirstRowSeq < sHandle->mFileHeader.mFirstRowSeqInFile )
    {
        sHandle->mRowSeq = sHandle->mFileHeader.mFirstRowSeqInFile;
    }
    else
    {
        sHandle->mRowSeq = aFirstRowSeq;
    }

    // -----------------------------------------
    // Memory Allocation
    // -----------------------------------------
    sAllocatedBlockInfoCount = sHandle->mFileHeader.mBlockInfoCount;
    sBlockSize               = sHandle->mFileHeader.mBlockSize;

    IDE_TEST( iduMemMgr::calloc( IDU_MEM_SM_SCP, 
                                 sAllocatedBlockInfoCount,
                                 ID_SIZEOF(scpfBlockInfo),
                                 (void**)&(sHandle->mBlockBuffer) )
              != IDE_SUCCESS );
    sState = 2;

    IDE_TEST( iduMemMgr::calloc( IDU_MEM_SM_SCP, 
                                 sAllocatedBlockInfoCount,
                                 ID_SIZEOF(scpfBlockInfo*),
                                 (void**)&(sHandle->mBlockMap) )
              != IDE_SUCCESS );
    sState = 3;

    IDE_TEST( iduMemMgr::calloc( IDU_MEM_SM_SCP, 1,
                                 (ULong)sAllocatedBlockInfoCount * sBlockSize,
                                 (void**)&(sHandle->mChainedSlotBuffer) )
              != IDE_SUCCESS );
    sState = 4;

    IDE_TEST( iduMemMgr::calloc( IDU_MEM_SM_SCP, 
                                 ID_SIZEOF( smiRow4DP ),
                                 sHandle->mFileHeader.mMaxRowCountInBlock,
                                 (void**)&(sHandle->mRowList) )
              != IDE_SUCCESS );
    sState = 5;

    IDE_TEST( iduMemMgr::calloc( IDU_MEM_SM_SCP, 
                                 ID_SIZEOF( smiValue ),
                                 aCommonHeader->mColumnCount *
                                 sHandle->mFileHeader.mMaxRowCountInBlock,
                                 (void**)&(sHandle->mValueBuffer ) )
              != IDE_SUCCESS );
    sState = 6;

    IDE_TEST( iduMemMgr::calloc( IDU_MEM_SM_SCP, 
                                 1,
                                 sAllocatedBlockInfoCount * sBlockSize 
                                    + ID_MAX_DIO_PAGE_SIZE,
                                 (void**)&(sHandle->mAllocatedBlock) )
              != IDE_SUCCESS );
    sState = 7;

    // DirectIO�� ���� Align�� ������� �մϴ�.
    sAlignedBlockPtr = (UChar*)idlOS::align( sHandle->mAllocatedBlock,
                                             ID_MAX_DIO_PAGE_SIZE );


    IDE_TEST( iduMemMgr::calloc( IDU_MEM_SM_SCP, 
                                 ID_SIZEOF( UInt  ),
                                 aCommonHeader->mColumnCount,
                                 (void**)&(sHandle->mColumnMap ) )
              != IDE_SUCCESS );
    sState = 8;

    // Column map
    for( i=0; i<aCommonHeader->mColumnCount; i++)
    {
        sHandle->mColumnMap[i] = 
            gSmiGlobalCallBackList.getColumnMapFromColumnHeaderFunc(
                aCommonHeader->mColumnHeader,
                i );
    }

    // block
    for( i=0; i<sAllocatedBlockInfoCount; i++)
    {
        sBlockInfo = &sHandle->mBlockBuffer[ i ];
        sBlockInfo->mBlockID             = 0;
        sBlockInfo->mRowSeqOfFirstSlot   = 0;
        sBlockInfo->mRowSeqOfLastSlot    = 0;
        sBlockInfo->mFirstRowSlotSeq     = 0;
        sBlockInfo->mFirstRowOffset      = SCPF_OFFSET_NULL;
        sBlockInfo->mHasLastChainedSlot  = ID_FALSE;
        sBlockInfo->mSlotCount           = 0;
        sBlockInfo->mCheckSum            = 0;

        sBlockInfo->mBlockPtr = sAlignedBlockPtr + i * sBlockSize;
        sHandle->mBlockMap[i] = sBlockInfo;
    }

    // ValueList
    for( i=0; i<sHandle->mFileHeader.mMaxRowCountInBlock; i++)
    {
        sHandle->mRowList[ i ].mValueList = 
            &(sHandle->mValueBuffer[ 
              i * aCommonHeader->mColumnCount ]);
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    switch( sState )
    {
        case 8:
            (void) iduMemMgr::free( sHandle->mColumnMap );
        case 7:
            (void) iduMemMgr::free( sHandle->mAllocatedBlock );
        case 6:
            (void) iduMemMgr::free( sHandle->mValueBuffer );
        case 5:
            (void) iduMemMgr::free( sHandle->mRowList );
        case 4:
            (void) iduMemMgr::free( sHandle->mChainedSlotBuffer );
        case 3:
            (void) iduMemMgr::free( sHandle->mBlockMap );
        case 2:
            (void) iduMemMgr::free( sHandle->mBlockBuffer );
        case 1:
            (void) iduMemMgr::free( sHandle    );
            *aHandle = NULL;
            break;
        default:
            break;
    }

    return IDE_FAILURE;
}

/***********************************************************************
 *
 * Description :
 *  ����� ObjectHandle�� �����Ѵ�.
 *
 *  aStatistics         - [IN] �������
 *  aHandle             - [IN] Object Handle
 *
 **********************************************************************/
static IDE_RC scpfDestroy4Import( idvSQL         * aStatistics,
                                  scpfHandle    ** aHandle )
{
    UInt        sState = 10;

    IDE_DASSERT( aHandle       != NULL );


    sState = 9;
    IDE_TEST( scpfReleaseHeader( aStatistics,
                                 (*aHandle) )
              != IDE_SUCCESS );

    sState = 8;
    IDE_TEST( iduMemMgr::free( (*aHandle)->mColumnMap )
              != IDE_SUCCESS );

    sState = 7;
    IDE_TEST( iduMemMgr::free( (*aHandle)->mAllocatedBlock )
              != IDE_SUCCESS );

    sState = 5;
    IDE_TEST( iduMemMgr::free( (*aHandle)->mValueBuffer )
              != IDE_SUCCESS );

    sState = 4;
    IDE_TEST( iduMemMgr::free( (*aHandle)->mRowList )
              != IDE_SUCCESS );

    sState = 3;
    IDE_TEST( iduMemMgr::free( (*aHandle)->mChainedSlotBuffer ) 
              != IDE_SUCCESS );

    sState = 2;
    IDE_TEST( iduMemMgr::free( (*aHandle)->mBlockMap ) 
              != IDE_SUCCESS );

    sState = 1;
    IDE_TEST( iduMemMgr::free( (*aHandle)->mBlockBuffer ) 
              != IDE_SUCCESS );

    sState = 0;
    IDE_TEST( iduMemMgr::free( *aHandle ) 
              != IDE_SUCCESS );
    *aHandle = NULL;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    switch( sState )
    {
        case 9:
            (void) scpfReleaseHeader( aStatistics,
                                      (*aHandle) );
        case 8:
            (void) iduMemMgr::free( (*aHandle)->mColumnMap );
        case 7:
            (void) iduMemMgr::free( (*aHandle)->mAllocatedBlock );
        case 6:
            (void) iduMemMgr::free( (*aHandle)->mValueBuffer );
        case 5:
            (void) iduMemMgr::free( (*aHandle)->mRowList );
        case 4:
            (void) iduMemMgr::free( (*aHandle)->mChainedSlotBuffer );
        case 3:
            (void) iduMemMgr::free( (*aHandle)->mBlockMap );
        case 2:
            (void) iduMemMgr::free( (*aHandle)->mBlockBuffer );
        case 1:
            (void) iduMemMgr::free( *aHandle );
            *aHandle = NULL;
            break;
        default:
            break;
    }

    return IDE_FAILURE;
}

/***********************************************************************
 *
 * Description :
 *  �ش� File�� ����.
 *
 *  aOpenFile           - [IN]  File�� Open�Ǿ� �ִ°�
 *  aFile               - [IN]  Open�� File �ڵ�
 *  aObjName            - [IN]  File�̸��� �����ϱ� ���� Obj�̸�
 *  aIdx                - [IN]  File�� ��ȣ
 *  aFileName           - [OUT] ������ ���� �̸�
 *
 **********************************************************************/
static IDE_RC scpfOpenFile( idBool         * aOpenFile,
                            iduFile        * aFile,
                            SChar          * aObjName, 
                            SChar          * aDirectory,
                            UInt             aIdx,
                            SChar          * aFileName )
{
    UInt         sFileState = 0;

    IDE_DASSERT( aFile        != NULL );
    IDE_DASSERT( aObjName     != NULL );
    IDE_DASSERT( aFileName    != NULL );

    // --------------------------------------
    // Create file 
    // ------------------------------------- 
    IDE_TEST( scpfFindFile( aObjName, 
                            aDirectory,
                            aIdx,
                            aFileName )
              != IDE_SUCCESS );

    IDE_TEST( aFile->initialize( IDU_MEM_SM_SCP,
                                 1, /* Max Open FD Count */
                                 IDU_FIO_STAT_OFF,
                                 IDV_WAIT_INDEX_NULL )
              != IDE_SUCCESS);
    sFileState = 1;

    IDE_TEST( aFile->setFileName( aFileName )
              != IDE_SUCCESS ) ;

    IDE_TEST( aFile->open()
              != IDE_SUCCESS );
    sFileState = 2;
    *aOpenFile = ID_TRUE;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    switch( sFileState )
    {
        case 2:
            (void) aFile->close() ;
            *aOpenFile = ID_FALSE;
        case 1:
            (void) aFile->destroy();
            break;
        default:
            break;
    }


    return IDE_FAILURE;

}
/***********************************************************************
 *
 * Description :
 *  ObjectName�� �´� File�� ã�´� 
 *  <Directory><DirectorySperator><FileName>_<Idx>.<EXT>  
 *
 *  aObjName            - [IN]  Object �̸�
 *  aDirectory          - [IN]  ������ �ִ� directory
 *  aFileIdx            - [IN]  File ��ȣ
 *  aFileName           - [OUT] ������ ���� �̸�
 *
 **********************************************************************/

static IDE_RC scpfFindFile( SChar            * aObjName,
                            SChar            * aDirectory,
                            UInt               aFileIdx,
                            SChar            * aFileName )
{
    UInt    sLen;

    IDE_DASSERT( aObjName   != NULL );
    IDE_DASSERT( aDirectory != NULL );
    IDE_DASSERT( aFileName  != NULL );


    // Directory�� �����Ͱ� �ִµ�, /�� ������ ���� ��� �ٿ���
    sLen = idlOS::strnlen( aDirectory, SM_MAX_FILE_NAME );
    if( 0 < sLen )
    {
        if( aDirectory[ sLen - 1 ] != IDL_FILE_SEPARATOR )
        {
            idlVA::appendFormat( aDirectory,
                                 SM_MAX_FILE_NAME,
                                 "%c",
                                 IDL_FILE_SEPARATOR );
        }
    }

    // 1. Original
    idlOS::snprintf( aFileName, 
                     SM_MAX_FILE_NAME,
                     "%s%s",
                     aDirectory, 
                     aObjName );
    IDE_TEST_CONT( idf::access(aFileName, F_OK) == 0, DONE );


    // 2. Extension
    // Ȯ���ڸ� ����
    idlOS::snprintf( aFileName, 
                     SM_MAX_FILE_NAME,
                     "%s%s%s",
                     aDirectory, 
                     aObjName, 
                     SCPF_DPFILE_EXT);
    IDE_TEST_CONT( idf::access(aFileName, F_OK) == 0, DONE );


    // 3. FileIdx
    // FileIndex�� Ȯ���ڸ� ����
    idlOS::snprintf( aFileName, 
                     SM_MAX_FILE_NAME,
                     "%s%s%c%"ID_UINT32_FMT"%s",
                     aDirectory, 
                     aObjName, 
                     SCPF_DPFILE_SEPARATOR,
                     aFileIdx,
                     SCPF_DPFILE_EXT);
    IDE_TEST_CONT( idf::access(aFileName, F_OK) == 0, DONE );

    // Ž�� ����
    IDE_RAISE( ERR_ABORT_CANT_OPEN_FILE );

    IDE_EXCEPTION_CONT( DONE );

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_ABORT_CANT_OPEN_FILE );
    {
        IDE_SET( ideSetErrorCode( smERR_ABORT_CANT_OPEN_FILE,
                                  aFileName ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}


/***********************************************************************
 *
 * Description :
 *  ������ ���� Header�� �а� �̸� �������� Handle�� �ʱ�ȭ �Ѵ�.
 *
 * 1) Header�� �д´�.
 * 2) Header�� Version�� �������� DATA_PORT_HEADERDesc�� �����Ѵ�.
 * 3) Size Estimation
 * 4) ������ DATA_PORT_HEADER�� �������� Header�� ���� �����͸� ����
 * 5) Size�� �������� Validation
 *
 *  aStatistics         - [IN] �������
 *  aFile               - [IN] ����� ����
 *  aCommonHeader       - [IN] ���� ���
 *  aFileHeader         - [IN] ���� ���
 *  aBlockBufferPosition- [OUT] Ŀ���� Header �ٷ� �������� �ű�
 *  aFilePosition       - [OUT] Ŀ���� Header �ٷ� �������� �ű�
 *
 **********************************************************************/

static IDE_RC scpfReadHeader( idvSQL                    * aStatistics,
                              iduFile                   * aFile,
                              smiDataPortHeader         * aCommonHeader,
                              scpfHeader                * aFileHeader,
                              scpfBlockBufferPosition   * aBlockBufferPosition,
                              scpfFilePosition          * aFilePosition )
{
    UInt                      sVersionBuffer;
    UInt                      sVersion;
    UInt                      sColumnCount;
    UInt                      sPartitionCount;
    smiDataPortHeaderDesc   * sCommonHeaderDesc;
    smiDataPortHeaderDesc   * sObjectHeaderDesc;
    smiDataPortHeaderDesc   * sTableHeaderDesc;
    smiDataPortHeaderDesc   * sColumnHeaderDesc;
    smiDataPortHeaderDesc   * sPartitionHeaderDesc;
    void                    * sObjectHeader;
    void                    * sTableHeader;
    void                    * sColumnHeader;
    void                    * sPartitionHeader; 
    UInt                      sTotalHeaderSize;
    UInt                      sHeaderSize;
    UChar                   * sBuffer;
    UInt                      sBufferSize;
    UInt                      sBufferOffset = 0;
    UInt                      sBufferState  = 0;
    UInt                      sOffset       = 0;
    UInt                      sState        = 0;
    UInt                      i;

    IDE_DASSERT( aStatistics   != NULL );
    IDE_DASSERT( aFile         != NULL );
    IDE_DASSERT( aCommonHeader != NULL );
    IDE_DASSERT( aFileHeader   != NULL );

    /********************************************************************
     * Read Version
     * Version�� �о�� ��������� ���¸� �� �� �ִ�.
     ********************************************************************/

    // Version ���� Endian ������� �о�� �ϱ� ������, ���ۿ� ���� ��
    // �ٽ� �д´�.
    IDE_TEST( aFile->read( aStatistics,
                           0, // header offset
                           &sVersionBuffer,
                           ID_SIZEOF(UInt) )
              != IDE_SUCCESS);
    sOffset = ID_SIZEOF(UInt);

    SCPF_READ_UINT( &sVersionBuffer,
                    &sVersion );

    IDE_TEST_RAISE( SMI_DATAPORT_VERSION_LATEST < sVersion,
                    ERR_ABORT_DATA_PORT_VERSION );
    aCommonHeader->mVersion = sVersion;

    /********************************************************************
     * Read CommonHeader
     ********************************************************************/

    sCommonHeaderDesc = gSmiDataPortHeaderDesc;
    sBufferSize = smiDataPort::getEncodedHeaderSize( sCommonHeaderDesc,
                                                     sVersion );

    IDE_TEST( iduMemMgr::calloc( IDU_MEM_SM_SCP, 
                                 1,
                                 sBufferSize,
                                 (void**)&(sBuffer) )
              != IDE_SUCCESS );
    sBufferState = 1;

    IDE_TEST( aFile->read( aStatistics,
                           sOffset,
                           sBuffer,
                           sBufferSize )
              != IDE_SUCCESS);
    sOffset += sBufferSize;

    sBufferOffset = 0;
    IDE_TEST( smiDataPort::readHeader( sCommonHeaderDesc,
                                       sVersion,
                                       sBuffer,
                                       &sBufferOffset,
                                       sBufferSize,
                                       aCommonHeader )
              != IDE_SUCCESS);

    sBufferState = 0;
    IDE_TEST( iduMemMgr::free( sBuffer ) != IDE_SUCCESS );

    /********************************************************************
     * Select DATA_PORT_HEADER Desc
     *
     * Version�� �������� DATA_PORT_HEADER Desc ����
     ********************************************************************/
    sObjectHeaderDesc     = gScpfFileHeaderDesc;
    sTableHeaderDesc      = (smiDataPortHeaderDesc*)
        gSmiGlobalCallBackList.getTableHeaderDescFunc();
    sColumnHeaderDesc     = (smiDataPortHeaderDesc*)
        gSmiGlobalCallBackList.getColumnHeaderDescFunc();
    sPartitionHeaderDesc  = (smiDataPortHeaderDesc*)
        gSmiGlobalCallBackList.getPartitionHeaderDescFunc();

    sColumnCount    = aCommonHeader->mColumnCount;
    sPartitionCount = aCommonHeader->mPartitionCount;

    /********************************************************************
     * Size Estimation
     *
     * Header�� Version +
     *          commonHeader + 
     *          scpfHeader +
     *          qsfTableInfo +
     *          qsfColumnInfo*ColCount + 
     *          qsfPartitionInfo*PartitionCount +
     * �� ���·� �����ȴ�. ���� ���� ���� �̹� �о����Ƿ� �����Ѵ�.
     ********************************************************************/
    sBufferSize = 
        smiDataPort::getEncodedHeaderSize( sObjectHeaderDesc, sVersion ) +
        smiDataPort::getEncodedHeaderSize( sTableHeaderDesc, sVersion ) +
        smiDataPort::getEncodedHeaderSize( sColumnHeaderDesc, sVersion ) 
            * sColumnCount +
        smiDataPort::getEncodedHeaderSize( sPartitionHeaderDesc, sVersion ) 
            * sPartitionCount;

    IDE_TEST( iduMemMgr::calloc( IDU_MEM_SM_SCP, 
                                 1,
                                 sBufferSize,
                                 (void**)&(sBuffer) )
              != IDE_SUCCESS );
    sBufferState = 1;

    // Size�� �������� Structure�� ������ ���� �Ҵ�

    // ObjectHeader(fileHeader)�� �̹� ����Ǿ� �ֱ⿡ �װ��� �̿��Ѵ�.
    sObjectHeader                = aFileHeader;
    aCommonHeader->mObjectHeader = aFileHeader;

    IDE_TEST( iduMemMgr::calloc( IDU_MEM_SM_SCP, 
                                 1,
                                 smiDataPort::getHeaderSize( 
                                     sTableHeaderDesc,
                                     sVersion ),
                                 (void**)&(aCommonHeader->mTableHeader ) )
              != IDE_SUCCESS );
    sTableHeader =  aCommonHeader->mTableHeader;
    sState = 1;

    IDE_TEST( iduMemMgr::calloc( IDU_MEM_SM_SCP, 
                                 sColumnCount,
                                 smiDataPort::getHeaderSize( 
                                     sColumnHeaderDesc,
                                     sVersion ),
                                 (void**)&(aCommonHeader->mColumnHeader ) )
              != IDE_SUCCESS );
    sColumnHeader = aCommonHeader->mColumnHeader;
    sState = 2;

    IDE_TEST( iduMemMgr::calloc( IDU_MEM_SM_SCP, 
                                 sPartitionCount,
                                 smiDataPort::getHeaderSize( 
                                     sPartitionHeaderDesc,
                                     sVersion ),
                                 (void**)&(aCommonHeader->mPartitionHeader ) )
              != IDE_SUCCESS );
    sPartitionHeader = aCommonHeader->mPartitionHeader;
    sState = 3;

    /********************************************************************
     * Load File & Table & Column & Partition Headers
     ********************************************************************/

    // Read
    sBufferOffset = 0;
    IDE_TEST( aFile->read( aStatistics,
                           sOffset,
                           sBuffer,
                           sBufferSize )
              != IDE_SUCCESS);
    sOffset += sBufferSize;

    IDE_TEST( smiDataPort::readHeader( sObjectHeaderDesc,
                                       sVersion,
                                       sBuffer,
                                       &sBufferOffset,
                                       sBufferSize,
                                       sObjectHeader )
              != IDE_SUCCESS );

    IDE_TEST( smiDataPort::readHeader( sTableHeaderDesc,
                                       sVersion,
                                       sBuffer,
                                       &sBufferOffset,
                                       sBufferSize,
                                       sTableHeader )
              != IDE_SUCCESS );

    sHeaderSize = smiDataPort::getHeaderSize( sColumnHeaderDesc,
                                              sVersion );
    for( i=0; i<sColumnCount; i++ )
    {
        IDE_TEST( smiDataPort::readHeader( sColumnHeaderDesc,
                                           sVersion,
                                           sBuffer,
                                           &sBufferOffset,
                                           sBufferSize,
                                           ((UChar*)sColumnHeader) 
                                           + sHeaderSize * i )
                  != IDE_SUCCESS );
    }

    sHeaderSize = smiDataPort::getHeaderSize( sPartitionHeaderDesc,
                                              sVersion );
    for( i=0; i<sPartitionCount; i++ )
    {
        IDE_TEST( smiDataPort::readHeader( sPartitionHeaderDesc,
                                           sVersion,
                                           sBuffer,
                                           &sBufferOffset,
                                           sBufferSize,
                                           ((UChar*)sPartitionHeader)
                                           + sHeaderSize * i )
                  != IDE_SUCCESS );
    }

    /********************************************************************
     * Size validation
     *******************************************************************/

    if( aFileHeader->mBlockHeaderSize 
        != smiDataPort::getEncodedHeaderSize( 
            gScpfBlockHeaderDesc,
            sVersion ) )
    {
        ideLog::log( IDE_SERVER_0,
                     "INTERNAL_ERROR[%s:%u]\n"
                     "BlockHeaderSize  :%u\n"
                     "EstimatedSize    :%u\n",
                     (SChar *)idlVA::basename(__FILE__),
                     __LINE__,
                     aFileHeader->mBlockHeaderSize,
                     smiDataPort::getEncodedHeaderSize( 
                         gScpfBlockHeaderDesc,
                         sVersion ) );

        IDE_RAISE( ERR_ABORT_DATA_PORT_INTERNAL_ERROR );
    }

    // ����� Headerũ��� ���� Header ũ�Ⱑ �����ؾ� �Ѵ�.
    sTotalHeaderSize = 
        ID_SIZEOF(UInt) +
        smiDataPort::getEncodedHeaderSize( sCommonHeaderDesc, sVersion ) +
        smiDataPort::getEncodedHeaderSize( sObjectHeaderDesc, sVersion ) +
        smiDataPort::getEncodedHeaderSize( sTableHeaderDesc, sVersion ) +
        smiDataPort::getEncodedHeaderSize( sColumnHeaderDesc, sVersion ) 
            * sColumnCount +
        smiDataPort::getEncodedHeaderSize( sPartitionHeaderDesc, sVersion ) 
            * sPartitionCount;

    // DirectIO�� ���� Align�� ������� �մϴ�.
    sTotalHeaderSize = 
        idlOS::align( sTotalHeaderSize, ID_MAX_DIO_PAGE_SIZE );

    if( aFileHeader->mFileHeaderSize != sTotalHeaderSize )
    {
        ideLog::log( IDE_SERVER_0,
                     "INTERNAL_ERROR[%s:%u]\n"
                     "FileHeaderSize  :%u\n"
                     "EstimatedSize   :%u\n",
                     (SChar *)idlVA::basename(__FILE__),
                     __LINE__,
                     aFileHeader->mBlockHeaderSize,
                     sTotalHeaderSize );

        IDE_RAISE( ERR_ABORT_DATA_PORT_INTERNAL_ERROR );
    }

    sBufferState = 0;
    IDE_TEST( iduMemMgr::free( sBuffer ) != IDE_SUCCESS );

    aBlockBufferPosition->mSeq      = 0;
    aBlockBufferPosition->mID       = 0;
    aBlockBufferPosition->mOffset   = aFileHeader->mBlockHeaderSize;
    aBlockBufferPosition->mSlotSeq  = 0;
    aFilePosition->mOffset          = aFileHeader->mFileHeaderSize;

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_ABORT_DATA_PORT_VERSION );
    {
        IDE_SET( ideSetErrorCode( smERR_ABORT_VERSION_NOT_SUPPORTED,
                                  sVersion,
                                  SMI_DATAPORT_VERSION_LATEST ) );
    }
    IDE_EXCEPTION( ERR_ABORT_DATA_PORT_INTERNAL_ERROR );
    {
        IDE_SET( ideSetErrorCode( 
                smERR_ABORT_DATA_PORT_INTERNAL_ERROR ) );
    }   
    IDE_EXCEPTION_END;

    switch( sState )
    {
    case 3:
        (void)iduMemMgr::free( sPartitionHeader );
        break;
    case 2:
        (void)iduMemMgr::free( sColumnHeader );
        break;
    case 1:
        (void)iduMemMgr::free( sTableHeader );
        break;
    default:
        break;
    }

    switch( sBufferState )
    {
        case 1:
            (void)iduMemMgr::free( sBuffer );
            break;
        default:
            break;
    }

    return IDE_FAILURE;

}

// Header�� ������.
static IDE_RC scpfReleaseHeader( idvSQL         * /*aStatistics*/,
                                 scpfHandle     * aHandle )
{
    UInt                sState = 3;
    smiDataPortHeader  *sCommonHeader;

    sCommonHeader = aHandle->mCommonHandle.mHeader;

    sState = 2;
    IDE_TEST( iduMemMgr::free( sCommonHeader->mPartitionHeader ) 
              != IDE_SUCCESS );

    sState = 1;
    IDE_TEST( iduMemMgr::free( sCommonHeader->mColumnHeader ) 
              != IDE_SUCCESS );

    sState = 0;
    IDE_TEST( iduMemMgr::free( sCommonHeader->mTableHeader ) 
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    switch( sState )
    {
    case 3:
        (void)iduMemMgr::free( sCommonHeader->mPartitionHeader );
        break;
    case 2:
        (void)iduMemMgr::free( sCommonHeader->mColumnHeader );
        break;
    case 1:
        (void)iduMemMgr::free( sCommonHeader->mTableHeader );
        break;
    default:
        break;
    }

    return IDE_FAILURE;
}

/***********************************************************************
 *
 * Description :
 *  Block�� File�� ���� �д´�.
 *  �����ϰ� Block ��ü�� BlockHeader Parsing�� �����Ѵ�.
 *
 *  aStatistics         - [IN] �������
 *  aFile               - [IN] FileHandle
 *  aCommonHeader       - [IN] �������
 *  aFileHeaderSize     - [IN] File�� Header ũ��
 *  aBlockHeaderSize    - [IN] Block�� Header ũ��
 *  aBlockSize          - [IN] Block�� ũ��
 *  aBlockID            - [IN] ���� Block ID
 *  aFileOffset         - [OUT] ���� Block�� File ��ġ
 *  aBlockInfo          - [OUT] ���� Block
 *
 **********************************************************************/
static IDE_RC scpfReadBlock( idvSQL             * aStatistics,
                             iduFile            * aFile,
                             smiDataPortHeader  * aCommonHeader,
                             UInt                 aFileHeaderSize,
                             UInt                 aBlockHeaderSize,
                             UInt                 aBlockSize,
                             UInt                 aBlockID,
                             ULong              * aFileOffset,
                             scpfBlockInfo      * aBlockInfo )
{
    UInt            sBufferOffset;
    UInt            sCheckSum;

    IDE_DASSERT( aFile       != NULL );
    IDE_DASSERT( aFileOffset != NULL );
    IDE_DASSERT( aBlockInfo  != NULL );

    /*BUG-32115  [sm-util] The Import operation in DataPort 
     * calculates file offset uses UINT type. */
    *aFileOffset  =  aFileHeaderSize +  ((ULong)aBlockSize) * aBlockID;

    IDE_TEST( aFile->read( aStatistics,
                           *aFileOffset,
                           aBlockInfo->mBlockPtr,
                           aBlockSize )
              != IDE_SUCCESS);

    /******************************************************************
     * Read Block Header
     *****************************************************************/
    sBufferOffset = 0;
    IDE_TEST( smiDataPort::readHeader( 
            gScpfBlockHeaderDesc,
            aCommonHeader->mVersion,
            aBlockInfo->mBlockPtr,
            &sBufferOffset,
            aBlockSize,
            aBlockInfo )
        != IDE_SUCCESS );

    // �䱸�� Block�� �о������
    IDE_TEST_RAISE( aBlockInfo->mBlockID != aBlockID,
                    ERR_ABORT_CORRUPTED_BLOCK );

    // Body�� ���� Checksum Ȯ�� (Checksum�� BlockHeader ����)
    sCheckSum = smuUtility::foldBinary( 
        aBlockInfo->mBlockPtr + aBlockHeaderSize,
        aBlockSize- aBlockHeaderSize );
    IDE_TEST_RAISE( aBlockInfo->mCheckSum != sCheckSum, 
                    ERR_ABORT_CORRUPTED_BLOCK );

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_ABORT_CORRUPTED_BLOCK );
    {
        IDE_SET( ideSetErrorCode( smERR_ABORT_CORRUPTED_BLOCK, 
                                  aBlockID ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 *
 * Description :
 *  BinarySearch�� FirstRow�� ���� Block�� ã�Ƽ� Handle�� �ø���.
 *
 *  aStatistics         - [IN] �������
 *  aFile               - [IN] ���� File
 *  aCommonHeader       - [IN] �������
 *  aFirstRowSeqInFile  - [IN] ���� �� ù��° row�� ��ȣ
 *  aTargetRow          - [IN] ã���� �ϴ� FirstRow
 *  aBlockCount         - [IN] Block�� ����
 *  aFileHeaderSize     - [IN] FileHeader�� ũ��
 *  aBlockHeaderSize    - [IN] BlockHeader�� ũ��
 *  aBlockSize          - [IN] Block�� ũ��
 *  aBlockMap           - [OUT] ���� Block�� ������ BlockInfo
 *  aBlockCorsur        - [IN/OUT] BlockBufferPosition
 *
 **********************************************************************/
static IDE_RC scpfFindFirstBlock( idvSQL                  * aStatistics,
                                  iduFile                 * aFile,
                                  smiDataPortHeader       * aCommonHeader,
                                  SLong                     aFirstRowSeqInFile,
                                  SLong                     aTargetRow,
                                  UInt                      aBlockCount,
                                  UInt                      aFileHeaderSize,
                                  UInt                      aBlockHeaderSize,
                                  UInt                      aBlockSize,
                                  scpfBlockInfo          ** aBlockMap,
                                  scpfBlockBufferPosition * aBlockBufferPosition )

{
    scpfBlockInfo *sBlockInfo;
    SLong          sFirstRowSeq;
    UInt           sMin;
    UInt           sMed;
    UInt           sMax;
    ULong          sFileOffset;
    UInt           sBlockID;

    IDE_DASSERT( aBlockMap     != NULL );

    sBlockID = 0;
    // ���� �� ù��° Row�� ��������� �ǹ̿�����, 
    // Binary search�� Skip�ϰ� ù��° Block���� ����
    IDE_TEST_CONT( (aTargetRow == aFirstRowSeqInFile), SKIP_FIND_ROW );

    sMin       = 0;
    sMax       = aBlockCount - 1;
    sBlockInfo = aBlockMap[ 0 ]; // 0�� Info�� Buffer�� ����Ѵ�.

    while( sMin <= sMax )
    {
        sMed = (sMin + sMax) >> 1;

        IDE_TEST( scpfReadBlock( aStatistics,
                                 aFile,
                                 aCommonHeader,
                                 aFileHeaderSize,
                                 aBlockHeaderSize,
                                 aBlockSize,
                                 sMed,
                                 &sFileOffset,
                                 sBlockInfo )
                  != IDE_SUCCESS );

        if( 0 < sBlockInfo->mFirstRowSlotSeq )
        {
            // ���� Block���� Overflow�� Slot �� ������, 
            // FirstRow�� �� ���� Row�̴�.
            sFirstRowSeq = sBlockInfo->mRowSeqOfFirstSlot + 1;
        }
        else
        {
            sFirstRowSeq = sBlockInfo->mRowSeqOfFirstSlot ;
        }

        if( aTargetRow < sFirstRowSeq )
        {
            sMax = sMed - 1;
        }
        else
        {
            sMin = sMed + 1;
        }
    }
    sBlockID = (sMin + sMax) >> 1;

    IDE_EXCEPTION_CONT( SKIP_FIND_ROW );

    aBlockBufferPosition->mID = sBlockID;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

/***********************************************************************
 *
 * Description :
 *  BlockMap�� Shift���Ѽ� ���� ���� ���� Block�� BlockMap��
 *  ���ʿ� ������ �̵���Ų��
 * 
 *  ��) BlockMap 0,1,2,3
 *     => 0,1�� Block�� Row�� ��� ó���Ͽ���
 *        (BlockBufferPosition.mSeq�� 2�� ����Ŵ)
 *     => 2�� ��ŭ Shift��Ŵ
 *        BlockMap  2,3,0,1
 *     => ���� 0,1���� 4,5�� Block�� �ø�
 *        BlockMap  2,3,4,5
 *     
 *
 *  aShiftBlockCount    - [IN] Shift�� Block ����
 *  aBlockInfoCount     - [IN] �ѹ��� ���� �� �ִ� BlockInfo ����
 *  aBlockMap           - [IN/OUT] ���� Block�� ������ BlockInfo 
 *
 **********************************************************************/
static void scpfShiftBlock( UInt                aShiftBlockCount,
                            UInt                aBlockInfoCount,
                            scpfBlockInfo    ** aBlockMap )
{
    scpfBlockInfo    * sBlockInfoPtr;
    UInt               i;
    UInt               j;

    IDE_DASSERT( aBlockMap    != NULL );

    // ���� Block��ŭ Shift
    for( j=0; j<aShiftBlockCount; j++ )
    {
        sBlockInfoPtr = aBlockMap[ 0 ];
        for( i=0; i<aBlockInfoCount-1; i++ )
        {
            aBlockMap[ i ] = aBlockMap[ i+1 ];
        }
        aBlockMap[ aBlockInfoCount-1 ] = sBlockInfoPtr;
    }
}

/***********************************************************************
 *
 * Description :
 *  ������ Block�� �о Handle�� �ø���.
 *
 *  aStatistics         - [IN] �������
 *  aFile               - [IN] ���� File
 *  aCommonHeader       - [IN]  �������
 *  aFileName           - [IN] ���� File�̸�
 *  aTargetRow          - [IN] Block�� ������, �� Row�� �־�� �Ѵ�.
 *  aBlockCount         - [IN] �� Block�� ����
 *  aFileHeaderSize     - [IN] FileHeader�� ũ��
 *  aBlockHeaderSize    - [IN] BlockHeader�� ũ��
 *  aBlockSize          - [IN] Block�� ũ��
 *  aNextBlockID        - [IN] �̹��� �غ�Ǵ� Block�� ù��° Block
 *  aFilePosition       - [OUT] ������ŭ FilePosition�̵�
 *  aBlockInfoCount     - [IN] �޸𸮻��� BlockInfo�� ����
 *  aReadBlockCount     - [IN] �̹��� ���� Block ����
 *  aBlockMap           - [OUT] ���� Block�� ������ BlockInfo 
 *
 **********************************************************************/
static IDE_RC scpfPreFetchBlock( idvSQL             * aStatistics,
                                 iduFile            * aFile,
                                 smiDataPortHeader  * aCommonHeader,
                                 SChar              * aFileName,
                                 SLong                aTargetRow,
                                 UInt                 aBlockCount,
                                 UInt                 aFileHeaderSize,
                                 UInt                 aBlockHeaderSize,
                                 UInt                 aBlockSize,
                                 UInt                 aNextBlockID,
                                 scpfFilePosition   * aFilePosition,
                                 UInt                 aBlockInfoCount,
                                 UInt                 aReadBlockCount,
                                 scpfBlockInfo     ** aBlockMap )
{
    ULong              sFileOffset;
    scpfBlockInfo    * sFirstBlock;
    UInt               sCurBlockID;
    UInt               sRemainBlockCount;
    UInt               i;

    IDE_DASSERT( aFilePosition!= NULL );
    IDE_DASSERT( aBlockMap    != NULL );

    // Block�� ���� �о��ų�, ���� Block�� ���°�
    IDE_TEST_CONT( ( aNextBlockID >= aBlockCount ) ||
                    ( aReadBlockCount == 0) ,
                    SKIP_READBLOCK );

    sFileOffset       = 0;
    sRemainBlockCount = aBlockInfoCount - aReadBlockCount;

    for( i=sRemainBlockCount; i<sRemainBlockCount + aReadBlockCount; i++ )
    {
        sCurBlockID = aNextBlockID + i;
        if( aBlockCount <= sCurBlockID )
        {
            break;
        }

        IDE_TEST( scpfReadBlock( aStatistics,
                                 aFile,
                                 aCommonHeader,
                                 aFileHeaderSize,
                                 aBlockHeaderSize,
                                 aBlockSize,
                                 sCurBlockID,
                                 &sFileOffset,
                                 aBlockMap[ i ] )
                  != IDE_SUCCESS );
    }


    // �ٸ��� Shift�ž�����, BlockMap�� ù��° Block��
    // NextBlockID��° Block�̸�, TargetRow�� ������.
    sFirstBlock   = aBlockMap[ 0 ];
    if( ( aNextBlockID !=  sFirstBlock->mBlockID ) ||
        ( aTargetRow < sFirstBlock->mRowSeqOfFirstSlot ) ||
        ( sFirstBlock->mRowSeqOfLastSlot < aTargetRow ) )
    {
        ideLog::log( IDE_SERVER_0,
                     "INTERNAL_ERROR[%s:%u]\n"
                     "FileName          :%s\n"
                     "NextBlockID       :%u\n"
                     "ReadBlockID       :%u\n"
                     "TargetRowID       :%llu\n"
                     "FirstRowSeqInBlock:%llu\n"
                     "LastRowSeqInBlock :%llu\n",
                     (SChar *)idlVA::basename(__FILE__),
                     __LINE__,
                     aFileName,
                     aNextBlockID,
                     sFirstBlock->mBlockID,
                     aTargetRow,
                     sFirstBlock->mRowSeqOfFirstSlot,
                     sFirstBlock->mRowSeqOfLastSlot );

        IDE_RAISE( ERR_ABORT_DATA_PORT_INTERNAL_ERROR );
    }

    aFilePosition->mOffset    = sFileOffset;

    IDE_EXCEPTION_CONT( SKIP_READBLOCK );

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_ABORT_DATA_PORT_INTERNAL_ERROR );
    {
        IDE_SET( ideSetErrorCode( 
                smERR_ABORT_DATA_PORT_INTERNAL_ERROR ) );
    }   
    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

/***********************************************************************
 *
 * Description :
 *  Block�� Parsing�Ͽ� Row���� Build�Ѵ�.
 *  Block�� �������� Slot �� �����̴�. ������ Slot �� 1�� �Ǵ� N���� ��
 * �ϳ��� Column, �� Column�� �� �ϳ��� Row�� �ȴ�.
 *  ���� �� �Լ�������, Block�� arsing�Ͽ� Handle�� smiRow4DP��
 * �� Slot ���� Row�� �����.
 *
 *  aStatistics            - [IN]  �������
 *  aCommonHeader          - [IN]  �������
 *  aFileHeader            - [IN]  �������
 *  aBlockBufferPosition   - [IN]  BlockBufferPosition
 *  aBlockMap              - [IN]  Row�� ���� BlockInfo�� map
 *  aChainedSlotBuffer     - [OUT] ChainedRow�� �����ϴ� �ӽ� ����
 *  aUsedChainedSlotBufSize- [OUT] �ӽ� ������ ��뷮
 *  aBuiltRowCount         - [OUT] Build�� Row�� ����
 *  aHasSupplementLob      - [OUT] �߰��� �ؾ��� Lobó���� �ִ°�?
 *  aRowList               - [OUT] Row���
 *
 **********************************************************************/

static IDE_RC scpfBuildRow( idvSQL                    * /*aStatistics*/,
                            scpfHandle                * aHandle,
                            smiDataPortHeader         * aCommonHeader,
                            scpfHeader                * aFileHeader,
                            scpfBlockBufferPosition   * aBlockBufferPosition,
                            scpfBlockInfo            ** aBlockMap,
                            UChar                     * aChainedSlotBuffer,
                            UInt                      * aUsedChainedSlotBufSize,
                            UInt                      * aBuiltRowCount,
                            idBool                    * aHasSupplementLob,
                            smiRow4DP                 * aRowList )
{
    smiRow4DP     * sRow;
    scpfBlockInfo * sBlockInfo;
    UInt            sBeginBlockID;
    UInt            sUnchainedSlotCount;
    UInt            sBasicColumnCount;
    UInt            sLobColumnCount;
    UInt          * sColumnMap;
    UInt            sMaxRowCountInBlock;
    UInt            sBlockHeaderSize;
    idBool          sDone;
    UInt            i;

    IDE_DASSERT( aCommonHeader          != NULL );
    IDE_DASSERT( aFileHeader            != NULL );
    IDE_DASSERT( aBlockBufferPosition   != NULL );
    IDE_DASSERT( aBlockMap              != NULL );
    IDE_DASSERT( aChainedSlotBuffer     != NULL );
    IDE_DASSERT( aUsedChainedSlotBufSize!= NULL );
    IDE_DASSERT( aBuiltRowCount         != NULL );
    IDE_DASSERT( aHasSupplementLob      != NULL );
    IDE_DASSERT( aRowList               != NULL );

    sBeginBlockID           = aBlockBufferPosition->mID;
    sRow                    = aRowList;
    sBasicColumnCount       = aCommonHeader->mBasicColumnCount;
    sLobColumnCount         = aCommonHeader->mLobColumnCount;
    sColumnMap              = aHandle->mColumnMap;
    sMaxRowCountInBlock     = aFileHeader->mMaxRowCountInBlock;
    sBlockHeaderSize        = aFileHeader->mBlockHeaderSize;

    *aUsedChainedSlotBufSize   = 0;
    IDE_DASSERT( aBlockBufferPosition->mSeq == 0);
    sDone                      = ID_FALSE;

    *aHasSupplementLob         = ID_FALSE;
    *aBuiltRowCount            = 0;

    while( sDone == ID_FALSE )
    {
        // BuildRowCount�� �̹� ����� Row�� �����̴�.
        // MaxRowCountInBlock�� Block���� �ִ� Row�����̸�, 
        // �̸� �������� row4DP�� �Ҵ��Ѵ�.
        // ���� �� ���� ���ٴ� ����, �̹� �غ��� row4DP�� ����
        // ����ߴٴ� ���̴�.
        IDE_TEST_RAISE( (*aBuiltRowCount) >= sMaxRowCountInBlock,
                        ERR_ABORT_CORRUPTED_BLOCK );

        for( i=0; i<sBasicColumnCount; i++)
        {
            IDE_TEST( scpfReadBasicColumn(
                        aBlockBufferPosition,
                        aBlockMap,
                        sBlockHeaderSize,
                        aChainedSlotBuffer,
                        aUsedChainedSlotBufSize,
                        &(sRow->mValueList[ sColumnMap[ i ] ]) )
                != IDE_SUCCESS );
        }

        // Lob�� ���� ���
        if( sLobColumnCount > 0)
        {
            // ���� Row�� Lob�� Block�� �Ѿ���� Ȯ���Ѵ�.
            sBlockInfo = aBlockMap[ aBlockBufferPosition->mSeq ];

            // Chained���� ���� Slot �� ������ ���Ѵ�
            sUnchainedSlotCount = sBlockInfo->mSlotCount ;
            if( sBlockInfo->mHasLastChainedSlot == ID_TRUE )
            {
                sUnchainedSlotCount = sBlockInfo->mSlotCount - 1;
            }
    
            if( sUnchainedSlotCount
                < ( aBlockBufferPosition->mSlotSeq  + sLobColumnCount ) )
            {
                // ���� Row�� Lob�� Block�� �Ѿ�� ���
                sRow->mHasSupplementLob = ID_TRUE;
                *aHasSupplementLob      = ID_TRUE;

                for( i = sBasicColumnCount; 
                     i < (sLobColumnCount + sBasicColumnCount);
                     i ++)
                {
                    // �ϴ� Null�� ���Ե� ��, ���߿� Append�ȴ�.
                    sRow->mValueList[ sColumnMap[ i ] ].length = 0;
                    sRow->mValueList[ sColumnMap[ i ] ].value  = 0;
                }

                sDone = ID_TRUE;
            }
            else
            {
                // �Ϲ� Column�� ������ ������� ó���Ѵ�.
                sRow->mHasSupplementLob = ID_FALSE;

                for( i = sBasicColumnCount; 
                     i < (sLobColumnCount + sBasicColumnCount);
                     i ++)
                {
                    IDE_TEST( scpfReadLobColumn( 
                                aBlockBufferPosition,
                                aBlockMap,
                                sBlockHeaderSize,
                                &(sRow->mValueList[ sColumnMap[ i ] ]) )
                        != IDE_SUCCESS );
                }
            }
        }

        // BeginBlockID�� BuildRow ���� ������ ������ ID��.
        // ���� �̰��� �ٸ��ٴ� �Ǵϴ� ReadColumn�Ѽ��鿡 ����
        // ���� Block���� Position�� �Ѿ�ٴ� ���̴�.
        // ��, �� Block�� ó���Ͽ��� ������ Build�۾��� �Ϸ�ȴ�.
        if( sBeginBlockID  != aBlockBufferPosition->mID )
        {
            sDone = ID_TRUE;
        }

        sRow ++;
        (*aBuiltRowCount) ++;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_ABORT_CORRUPTED_BLOCK );
    {
        IDE_SET( ideSetErrorCode( 
                smERR_ABORT_CORRUPTED_BLOCK,
                aBlockBufferPosition->mID ) );
    }

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 *
 * Description :
 *  Block���κ��� BasicColumn�� �д´�.
 *  ���� Column�� Chaine�Ǹ�, ChainedSlotBuffer�� �̿��Ѵ�.
 *
 *  aStatistics                - [IN]  �������
 *  aBlockBufferPosition       - [IN]  BlockBufferPosition
 *  aBlockInfoCount            - [IN]  Row�� �о�帱 Block�� �� ����
 *  aBlockInfo                 - [IN]  Row�� �о�帱 Block
 *  aBlockHeaderSize           - [IN]  BlockHeader�� ũ��
 *  aChainedSlotBuffer         - [IN]  Chained�� Row�� �����ص� �ӽ� ����
 *  aUsedChainedSlotBufferSize - [OUT] ����� �ӽ� ������ ũ��
 *  aValue                     - [OUT] ���� Value
 *
 * Slot �� ����
 * - VarInt (Length ��Ͽ� �̿��)
 *      0~63        [0|len ]
 *     64~16383     [1|len>>8  & 63][len ]
 *  16384~4194303   [2|len>>16 & 63][len>>8 & 255][len & 255]
 *4194304~1073741824[3|len>>24 & 63][len>>16 & 255][len>>8 & 255][len & 255]
 *
 * - BasicColumn
 *   [Header:VarInt(1~4Byte)][Body]
 **********************************************************************/

static IDE_RC scpfReadBasicColumn( 
                    scpfBlockBufferPosition * aBlockBufferPosition,
                    scpfBlockInfo          ** aBlockMap,
                    UInt                      aBlockHeaderSize,
                    UChar                   * aChainedSlotBuffer,
                    UInt                    * aUsedChainedSlotBufSize,
                    smiValue                * aValue )
{
    scpfBlockInfo   * sBlockInfo;
    idBool            sReadNext;
    UInt              sSlotValLen;
    void            * sSlotVal;

    IDE_DASSERT( aBlockBufferPosition        != NULL );
    IDE_DASSERT( aBlockMap                   != NULL );
    IDE_DASSERT( aChainedSlotBuffer          != NULL );
    IDE_DASSERT( aUsedChainedSlotBufSize     != NULL );
    IDE_DASSERT( aValue                      != NULL );

    sSlotValLen = 0;
    sBlockInfo  = aBlockMap[ aBlockBufferPosition->mSeq ];

    // ������ Slot���� �̷���� ���
    if( (aBlockBufferPosition->mSlotSeq  == sBlockInfo->mSlotCount - 1) &&
        (sBlockInfo->mHasLastChainedSlot == ID_TRUE) )
    {
        aValue->length = 0;
        aValue->value  = aChainedSlotBuffer + *aUsedChainedSlotBufSize;
        sReadNext      = ID_TRUE;

        while( sReadNext == ID_TRUE )
        {
            sBlockInfo = aBlockMap[ aBlockBufferPosition->mSeq ];

            sReadNext      = ID_FALSE;
            // ���� �д� Slot �� ChainedSlot �ΰ�
            if( (aBlockBufferPosition->mSlotSeq + 1 
                    == sBlockInfo->mSlotCount ) &&
                (sBlockInfo->mHasLastChainedSlot 
                    == ID_TRUE) )
            {
                sReadNext      = ID_TRUE;
            }

            // Slot�� �ϳ� �о
            IDE_TEST( scpfReadSlot( aBlockBufferPosition,
                                    aBlockMap,
                                    aBlockHeaderSize,
                                    &sSlotValLen,
                                    &sSlotVal )
                      != IDE_SUCCESS);

            // ChainedBuffer�� �����Ѵ�.
            aValue->length += sSlotValLen;
            idlOS::memcpy( aChainedSlotBuffer + *aUsedChainedSlotBufSize,
                           sSlotVal,
                           sSlotValLen );
            (*aUsedChainedSlotBufSize) += sSlotValLen;
        }
    }
    else
    {
        // Chaining �ȵ� ���
        IDE_TEST( scpfReadSlot( aBlockBufferPosition,
                                aBlockMap,
                                aBlockHeaderSize,
                                &aValue->length,
                                (void**)&aValue->value )
                  != IDE_SUCCESS);
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

/***********************************************************************
 *
 * Description :
 *  Block���κ��� Lob Column�� �д´�.
 *  Slot�� �ϳ� �ִ� ��츸 �������� �´�.
 *
 *  aStatistics                - [IN]  �������
 *  aBlockMap                  - [IN]  Slot�� ���� Block
 *  aBlockHeaderSize           - [IN]  BlockHeader�� ũ��
 *  aValue                     - [OUT] ���� Value
 *
 * - Lob Column
 *   [LobLen (4byte)][Slot_Len(1~4Byte)][Body]
 ***********************************************************************/

static IDE_RC scpfReadLobColumn( scpfBlockBufferPosition * aBlockBufferPosition,
                                 scpfBlockInfo          ** aBlockMap,
                                 UInt                      aBlockHeaderSize,
                                 smiValue                * aValue )
{
    scpfBlockInfo   * sBlockInfo;
    UInt              sLobLength;

    IDE_DASSERT( aBlockBufferPosition!= NULL );
    IDE_DASSERT( aBlockMap           != NULL );
    IDE_DASSERT( aValue              != NULL );

    sBlockInfo = aBlockMap[ aBlockBufferPosition->mSeq ];

    // Block���� Chained �ȵ� Slot�� �־�� �Ѵ�
    IDE_TEST_RAISE( 
        (aBlockBufferPosition->mSlotSeq  == sBlockInfo->mSlotCount - 1) &&
        (sBlockInfo->mHasLastChainedSlot == ID_TRUE),
        ERR_ABORT_CORRUPTED_BLOCK  );

    // Lob Len
    SCPF_READ_UINT( sBlockInfo->mBlockPtr + aBlockBufferPosition->mOffset, 
                    &sLobLength );
    aBlockBufferPosition->mOffset += ID_SIZEOF(UInt);

    // Slot
    IDE_TEST( scpfReadSlot( aBlockBufferPosition,
                            aBlockMap,
                            aBlockHeaderSize,
                            &aValue->length,
                            (void**)&aValue->value )
              != IDE_SUCCESS);

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_ABORT_CORRUPTED_BLOCK );
    {
        IDE_SET( ideSetErrorCode( 
                smERR_ABORT_CORRUPTED_BLOCK,
                sBlockInfo->mBlockID ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

/***********************************************************************
 *
 * Description :
 *  Block���κ��� Slot�� �ϳ� �д´�.
 *
 *  aBlockBufferPosition       - [IN]  BlockBufferPosition
 *  aBlockMap                  - [IN]  Slot�� ���� Block
 *  aBlockHeaderSize           - [IN]  BlockHeader�� ũ��
 *  aSlotValLen                - [OUT] ���� SlotVal�� length
 *  aSlotVal                   - [OUT] ���� Slotvalue
 *
 **********************************************************************/

static IDE_RC scpfReadSlot( scpfBlockBufferPosition * aBlockBufferPosition,
                            scpfBlockInfo          ** aBlockMap,
                            UInt                      aBlockHeaderSize,
                            UInt                    * aSlotValLen,
                            void                   ** aSlotVal )
{
    scpfBlockInfo   * sBlockInfo;

    IDE_DASSERT( aBlockBufferPosition        != NULL );
    IDE_DASSERT( aBlockMap                   != NULL );
    IDE_DASSERT( aSlotValLen                 != NULL );
    IDE_DASSERT( aSlotVal                    != NULL );

    sBlockInfo = aBlockMap[ aBlockBufferPosition->mSeq ];

    // SlotValLen ����
    SCPF_READ_VARINT( sBlockInfo->mBlockPtr + aBlockBufferPosition->mOffset,
                      aSlotValLen );
    aBlockBufferPosition->mOffset += SCPF_GET_VARINT_SIZE( aSlotValLen );
    aBlockBufferPosition->mSlotSeq  ++;

    // SlotVal ����
    if( *aSlotValLen == 0 )
    {
        *aSlotVal = NULL;
    }
    else
    {
        *aSlotVal = sBlockInfo->mBlockPtr + aBlockBufferPosition->mOffset;
        aBlockBufferPosition->mOffset += *aSlotValLen;
    }

    // Block���� ��� Slot �� �о��� ���
    if( aBlockBufferPosition->mSlotSeq  == sBlockInfo->mSlotCount )
    {
        aBlockBufferPosition->mSeq        ++;
        aBlockBufferPosition->mID         ++;
        aBlockBufferPosition->mOffset     = aBlockHeaderSize;
        aBlockBufferPosition->mSlotSeq    = 0;
    }

    return IDE_SUCCESS;

}


/***********************************************************************
 *
 * Description :
 *  ���ݱ��� ���� ������ �ݴ´�.
 *
 *  aStatistics         - [IN] �������
 *  aHandle             - [IN] Object Handle
 *
 **********************************************************************/
static IDE_RC scpfCloseFile4Import( idvSQL         * /*aStatistics*/,
                                    scpfHandle     * aHandle )
{
    UInt                      sFileState = 2;

    sFileState = 1;
    IDE_TEST( aHandle->mFile.close() != IDE_SUCCESS);

    aHandle->mOpenFile = ID_FALSE;
    sFileState = 0;
    IDE_TEST( aHandle->mFile.destroy() != IDE_SUCCESS);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    switch( sFileState )
    {
        case 2:
            (void) aHandle->mFile.close() ;
            aHandle->mOpenFile = ID_FALSE;
        case 1:
            (void) aHandle->mFile.destroy() ;
            break;
        default:
            break;
    }


    return IDE_FAILURE;
}

/***********************************************************************
 * Dump - mm/main/dumpte�� ���� �Լ���
 **********************************************************************/

/***********************************************************************
 *
 * Description :
 *  Block�� Dump�Ѵ�
 *
 *  aStatistics         - [IN] �������
 *  aHandle             - [IN] Handle 
 *  aFirstBlock         - [IN] Dump�� Block�� ���� ��ȣ
 *  aLastBlock          - [IN] Dump�� Block�� �� ��ȣ
 *  aDetail             - [IN] Body���� �ڼ��� ����� ���ΰ�?
 *
 **********************************************************************/
IDE_RC scpfDumpBlocks( idvSQL         * aStatistics,
                       scpfHandle     * aHandle,
                       SLong            aFirstBlock,
                       SLong            aLastBlock,
                       idBool           aDetail )
{
    scpfBlockInfo * sBlockInfo;
    SChar         * sTempBuf;
    UInt            sState = 0;
    ULong           sFileOffset;
    UInt            sFlag;
    UInt            sDumpSize;
    UInt            i;
    UInt            j;

    IDE_DASSERT( aHandle    != NULL );


    if( aHandle->mFileHeader.mBlockCount < aLastBlock )
    {
        aLastBlock = aHandle->mFileHeader.mBlockCount;
    }

    if( aLastBlock < aFirstBlock )
    {
        aFirstBlock = aLastBlock-1;
    }



    /* Block ���θ� Dump�� ������� ������ ���۸� Ȯ���մϴ�. */
    IDE_TEST( iduMemMgr::calloc( IDU_MEM_SM_SCP, 1,
                                 ID_SIZEOF( SChar ) * IDE_DUMP_DEST_LIMIT,
                                 (void**)&sTempBuf )
              != IDE_SUCCESS );
    sState = 1;


    /* Block Dump */
    sBlockInfo = aHandle->mBlockMap[0];
    for( i = aFirstBlock; i < aLastBlock; i ++ )
    {
        IDE_TEST( scpfReadBlock( aStatistics,
                                 &aHandle->mFile,
                                 aHandle->mCommonHandle.mHeader,
                                 aHandle->mFileHeader.mFileHeaderSize,
                                 aHandle->mFileHeader.mBlockHeaderSize,
                                 aHandle->mFileHeader.mBlockSize,
                                 i,
                                 &sFileOffset,
                                 sBlockInfo )
                  != IDE_SUCCESS );

        idlOS::printf( "[BlockID:%"ID_UINT32_FMT" - FileOffset:%"ID_UINT64_FMT"]\n", 
                       i,
                       sFileOffset );

        if( aDetail == ID_TRUE )
        {
            sFlag = SMI_DATAPORT_HEADER_FLAG_FULL;
        }
        else
        {
            sFlag = SMI_DATAPORT_HEADER_FLAG_DESCNAME_YES |
                SMI_DATAPORT_HEADER_FLAG_COLNAME_YES ;
        }

        sTempBuf[0] = '\0';
        IDE_TEST( smiDataPort::dumpHeader( 
                gScpfBlockHeaderDesc,
                aHandle->mCommonHandle.mHeader->mVersion,
                (void*)sBlockInfo,
                sFlag,
                sTempBuf,
                IDE_DUMP_DEST_LIMIT )
                  != IDE_SUCCESS );

        idlOS::printf( "%s\n",sTempBuf );

        if( aDetail == ID_TRUE )
        {
            for( j=0; j<aHandle->mFileHeader.mBlockSize; j+=IDE_DUMP_SRC_LIMIT )
            {
                sTempBuf[0] = '\0';
                sDumpSize   = aHandle->mFileHeader.mBlockSize - j;
                if( IDE_DUMP_SRC_LIMIT < sDumpSize )
                {
                    sDumpSize = IDE_DUMP_SRC_LIMIT;
                }
                IDE_TEST( scpfDumpBlockBody( sBlockInfo, 
                                             j,
                                             j + sDumpSize,
                                             sTempBuf, 
                                             IDE_DUMP_DEST_LIMIT )
                          != IDE_SUCCESS );
                idlOS::printf( "%s\n",sTempBuf );
            }
        }
    }

    sState = 0;
    IDE_TEST( iduMemMgr::free( sTempBuf ) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    switch(sState)
    {
        case 1:
            (void) iduMemMgr::free( sTempBuf ) ;
        default:
            break;
    }


    return IDE_FAILURE;
}

/***********************************************************************
 *
 * Description :
 *  Header�� Dump�Ѵ�.
 *
 *  sHandle         - [IN] Dump�� ���
 *
 **********************************************************************/
IDE_RC scpfDumpHeader( scpfHandle     * aHandle,
                       idBool           aDetail )
{
    smiDataPortHeaderDesc        * sCommonHeaderDesc;
    smiDataPortHeaderDesc        * sObjectHeaderDesc;
    smiDataPortHeaderDesc        * sTableHeaderDesc;
    smiDataPortHeaderDesc        * sColumnHeaderDesc;
    smiDataPortHeaderDesc        * sPartitionHeaderDesc;
    UInt                           sHeaderSize;
    smiDataPortHeader            * sHeader;
    SChar                        * sTempBuf;
    UInt                           sState = 0;
    UInt                           sFlag;
    UInt                           i;

    IDE_DASSERT( aHandle != NULL     );

    /* Dump�� Buffer Ȯ�� */
    IDE_TEST( iduMemMgr::calloc( IDU_MEM_SM_SCP, 1,
                                 ID_SIZEOF( SChar ) * IDE_DUMP_DEST_LIMIT,
                                 (void**)&sTempBuf )
              != IDE_SUCCESS );
    sState = 1;

    sHeader = aHandle->mCommonHandle.mHeader;

    sCommonHeaderDesc     = gSmiDataPortHeaderDesc;
    sObjectHeaderDesc     = gScpfFileHeaderDesc;
    sTableHeaderDesc      = (smiDataPortHeaderDesc*)
        gSmiGlobalCallBackList.getTableHeaderDescFunc();
    sColumnHeaderDesc     = (smiDataPortHeaderDesc*)
        gSmiGlobalCallBackList.getColumnHeaderDescFunc();
    sPartitionHeaderDesc  = (smiDataPortHeaderDesc*)
        gSmiGlobalCallBackList.getPartitionHeaderDescFunc();

    if( aDetail == ID_TRUE )
    {
        sFlag = SMI_DATAPORT_HEADER_FLAG_FULL;
    }
    else
    {
        sFlag = SMI_DATAPORT_HEADER_FLAG_DESCNAME_YES |
            SMI_DATAPORT_HEADER_FLAG_COLNAME_YES ;
    }

    idlOS::snprintf( sTempBuf,
                     IDE_DUMP_DEST_LIMIT,
                     "----------- Header ----------\n" );
    IDE_TEST( smiDataPort::dumpHeader( sCommonHeaderDesc,
                                       sHeader->mVersion,
                                       sHeader,
                                       sFlag,
                                       sTempBuf,
                                       IDE_DUMP_DEST_LIMIT )
              != IDE_SUCCESS );

    if( aDetail == ID_TRUE )
    {
        IDE_TEST( smiDataPort::dumpHeader( sObjectHeaderDesc,
                                           sHeader->mVersion,
                                           sHeader->mObjectHeader,
                                           sFlag,
                                           sTempBuf,
                                           IDE_DUMP_DEST_LIMIT )
                  != IDE_SUCCESS );

        IDE_TEST( smiDataPort::dumpHeader( sTableHeaderDesc,
                                           sHeader->mVersion,
                                           sHeader->mTableHeader,
                                           sFlag,
                                           sTempBuf,
                                           IDE_DUMP_DEST_LIMIT )
                  != IDE_SUCCESS );
    }

    sHeaderSize = smiDataPort::getHeaderSize( sColumnHeaderDesc,
                                              sHeader->mVersion );
    for( i=0; i<sHeader->mColumnCount; i++ )
    {
        IDE_TEST( smiDataPort::dumpHeader( sColumnHeaderDesc,
                                           sHeader->mVersion,
                                           ((UChar*)sHeader->mColumnHeader)
                                           + sHeaderSize * i,
                                           sFlag,
                                           sTempBuf,
                                           IDE_DUMP_DEST_LIMIT )
                  != IDE_SUCCESS );
    }

    if( aDetail == ID_TRUE )
    {
        sHeaderSize = smiDataPort::getHeaderSize( sPartitionHeaderDesc,
                                                  sHeader->mVersion );
        for( i=0; i<sHeader->mPartitionCount; i++ )
        {
            IDE_TEST( smiDataPort::dumpHeader( 
                            sPartitionHeaderDesc,
                            sHeader->mVersion,
                            ((UChar*)sHeader->mPartitionHeader)
                            + sHeaderSize * i,
                            sFlag,
                            sTempBuf,
                            IDE_DUMP_DEST_LIMIT )
                        != IDE_SUCCESS );
        }
    }

    idlOS::printf("%s\n",sTempBuf);

    sState = 0;
    IDE_TEST( iduMemMgr::free( sTempBuf ) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    switch(sState)
    {
        case 1:
            (void) iduMemMgr::free( sTempBuf ) ;
        default:
            break;
    }

    return IDE_FAILURE;
}

/***********************************************************************
 *
 * Description :
 *  Block�� Body�� Dump�Ѵ�.
 *
 *  aBlockInfo      - [IN]  Dump�� Block
 *  aBeginOffset    - [IN]  Block�� Dump�� ���� ����
 *  aEndOffset      - [IN]  Block�� Dump������ ����
 *  aOutBuf         - [OUT] ������� ������ ����
 *  aOutSize        - [OUT] ��� ������ ũ��
 *
 **********************************************************************/
IDE_RC scpfDumpBlockBody( scpfBlockInfo  * aBlockInfo,
                          UInt             aBeginOffset,
                          UInt             aEndOffset,
                          SChar          * aOutBuf,
                          UInt             aOutSize )
{
    SChar     * sTempBuf;
    UInt        sState = 0;

    IDE_DASSERT( aBlockInfo != NULL );
    IDE_DASSERT( aOutBuf    != NULL );

    /* Block ���θ� Dump�� ������� ������ ���۸� Ȯ���մϴ�. */
    IDE_TEST( iduMemMgr::calloc( IDU_MEM_SM_SCP, 1,
                                 ID_SIZEOF( SChar ) * IDE_DUMP_DEST_LIMIT,
                                 (void**)&sTempBuf )
              != IDE_SUCCESS );
    sState = 1;

    IDE_TEST( ideLog::ideMemToHexStr( 
            aBlockInfo->mBlockPtr + aBeginOffset,
            aEndOffset - aBeginOffset,
            IDE_DUMP_FORMAT_NORMAL,
            sTempBuf,
            IDE_DUMP_DEST_LIMIT )
        != IDE_SUCCESS );


    idlVA::appendFormat( aOutBuf, aOutSize, "%s\n", sTempBuf );

    sState = 0;
    IDE_TEST( iduMemMgr::free( sTempBuf ) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    switch(sState)
    {
        case 1:
            (void) iduMemMgr::free( sTempBuf ) ;
        default:
            break;
    }

    return IDE_FAILURE;
}


/***********************************************************************
 *
 * Description :
 *  Row�� Slot ���� ����Ѵ�.
 *
 *  aStatistics         - [IN] �������
 *  aHandle             - [IN] Handle 
 *
 **********************************************************************/
IDE_RC scpfDumpRows( idvSQL         * aStatistics,
                     scpfHandle     * aHandle )

{
    UInt        sRowCount;
    smiRow4DP * sRows;
    SChar     * sTempBuf;
    UInt        sDumpOffset;
    UInt        sDumpSize;
    UInt        sState = 0;
    UInt        sBasicColumnCount;
    UInt        sLobColumnCount;
    SLong       sRowSeq;
    UInt        sLobLength;
    UInt        sLobPieceLength;
    UChar     * sLobPieceValue;
    UInt        i;
    UInt        j;

    IDE_DASSERT( aStatistics != NULL );
    IDE_DASSERT( aHandle     != NULL );

    sBasicColumnCount  = aHandle->mCommonHandle.mHeader->mBasicColumnCount;
    sLobColumnCount    = aHandle->mCommonHandle.mHeader->mLobColumnCount;

    /* Row�� Dump�� Buffer Ȯ�� */
    IDE_TEST( iduMemMgr::calloc( IDU_MEM_SM_SCP, 1,
                                 ID_SIZEOF( SChar ) * IDE_DUMP_DEST_LIMIT,
                                 (void**)&sTempBuf )
              != IDE_SUCCESS );
    sState = 1;

    sRowSeq = aHandle->mRowSeq;

    IDE_TEST( scpfRead( aStatistics,
                        aHandle,
                        &sRows,
                        &sRowCount )
              != IDE_SUCCESS );

    while( 0 < sRowCount )
    {
        for( i=0; i<sRowCount; i ++)
        {
            // [ <RowSeq ]
            idlOS::printf(
                "[%8"ID_UINT64_FMT"]",
                sRowSeq );

            for( j=0; j<sBasicColumnCount; j++)
            {
                // [ <RowSeq ][ <ValueLength> |
                idlOS::printf(
                    "[%4"ID_UINT32_FMT"|",
                    sRows[i].mValueList[j].length );

                if( sRows[i].mValueList[j].value != NULL )
                {
                    for( sDumpOffset = 0;
                         sDumpOffset < sRows[i].mValueList[j].length;
                         sDumpOffset += IDE_DUMP_SRC_LIMIT )
                    {
                        sDumpSize = 
                            sRows[i].mValueList[j].length - sDumpOffset;

                        if( IDE_DUMP_SRC_LIMIT < sDumpSize )
                        {
                            sDumpSize = IDE_DUMP_SRC_LIMIT;
                        }

                        IDE_TEST( ideLog::ideMemToHexStr( 
                                (UChar*)sRows[i].mValueList[j].value
                                + sDumpOffset,
                                sDumpSize,
                                IDE_DUMP_FORMAT_VALUEONLY,
                                sTempBuf,
                                IDE_DUMP_DEST_LIMIT )
                            != IDE_SUCCESS );

                        // [ <RowSeq ][ <ValueLength> | <Value>
                        idlOS::printf( "%s", sTempBuf );

                        //���� �� ����� ���� ������, ���� �� �߰� ���
                        if( ( sDumpOffset + IDE_DUMP_SRC_LIMIT ) 
                            < sRows[i].mValueList[j].length )
                        {
                            idlOS::printf( "\n" );
                        }
                    }
                    // [ <RowSeq ][ <ValueLength> | <Value> ]
                    idlOS::printf( "] " );
                }
                else
                {
                    // [ <RowSeq ][ <NULL> ]
                    idlOS::printf( "<NULL>] " );
                }

            }

            if( sRows[i].mHasSupplementLob == ID_FALSE )
            {
                for( j = sBasicColumnCount; 
                     j < sBasicColumnCount + sLobColumnCount;
                     j++)
                {
                    // [ <RowSeq ][ <ValueLength> |
                    idlOS::printf(
                        "[%4"ID_UINT32_FMT"|",
                        sRows[i].mValueList[j].length );

                    if( sRows[i].mValueList[j].value != NULL )
                    {
                        for( sDumpOffset = 0;
                             sDumpOffset < sRows[i].mValueList[j].length;
                             sDumpOffset += IDE_DUMP_SRC_LIMIT )
                        {
                            sDumpSize = 
                                sRows[i].mValueList[j].length - sDumpOffset;

                            if( IDE_DUMP_SRC_LIMIT < sDumpSize )
                            {
                                sDumpSize = IDE_DUMP_SRC_LIMIT;
                            }

                            IDE_TEST( ideLog::ideMemToHexStr( 
                                    (UChar*)sRows[i].mValueList[j].value
                                    + sDumpOffset,
                                    sDumpSize,
                                    IDE_DUMP_FORMAT_VALUEONLY,
                                    sTempBuf,
                                    IDE_DUMP_DEST_LIMIT )
                                != IDE_SUCCESS );

                            // [ <RowSeq ][ <ValueLength> | <Value>
                            idlOS::printf( "%s", sTempBuf );

                            //���� �� ����� ���� ������, ���� �� �߰� ���
                            if( ( sDumpOffset + IDE_DUMP_SRC_LIMIT ) 
                                < sRows[i].mValueList[j].length )
                            {
                                idlOS::printf( "\n" );
                            }
                        }
                        // [ <RowSeq ][ <ValueLength> | <Value> ]
                        idlOS::printf( "] " );
                    }
                    else
                    {
                        // [ <RowSeq ][ <NULL> ]
                        idlOS::printf( "<NULL>] " );
                    }
                }
            }
            else
            {
                for( j = sBasicColumnCount; 
                     j < sBasicColumnCount + sLobColumnCount;
                     j++)
                {
                    IDE_TEST( scpfReadLobLength( aStatistics,
                                                 aHandle,
                                                 &sLobLength )
                              != IDE_SUCCESS );

                    idlOS::printf( "[%"ID_UINT32_FMT"|\n", sLobLength );

                    while( 0 < sLobLength )
                    {
                        IDE_TEST( scpfReadLob( aStatistics,
                                               aHandle,
                                               &sLobPieceLength,
                                               &sLobPieceValue )
                                  != IDE_SUCCESS );

                        for( sDumpOffset = 0;
                             sDumpOffset < sLobPieceLength;
                             sDumpOffset += IDE_DUMP_SRC_LIMIT )
                        {
                            sDumpSize = 
                                sLobPieceLength - sDumpOffset;

                            if( IDE_DUMP_SRC_LIMIT < sDumpSize )
                            {
                                sDumpSize = IDE_DUMP_SRC_LIMIT;
                            }

                            IDE_TEST( ideLog::ideMemToHexStr( 
                                    (UChar*)sLobPieceValue
                                    + sDumpOffset,
                                    sDumpSize,
                                    IDE_DUMP_FORMAT_VALUEONLY,
                                    sTempBuf,
                                    IDE_DUMP_DEST_LIMIT )
                                != IDE_SUCCESS );

                            idlOS::printf( "%s", sTempBuf );
                        }
                        sLobLength -= sLobPieceLength;
                    }
                    IDE_TEST( scpfFinishLobReading( aStatistics,
                                                    aHandle )
                              != IDE_SUCCESS );
                    idlOS::printf( "]" );
                }
            }
            idlOS::printf("\n");

            sRowSeq++;
        }

        IDE_TEST( scpfRead( aStatistics,
                            aHandle,
                            &sRows,
                            &sRowCount )
                  != IDE_SUCCESS );
    }

    sState = 0;
    IDE_TEST( iduMemMgr::free( sTempBuf ) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    switch(sState)
    {
        case 1:
            (void) iduMemMgr::free( sTempBuf ) ;
        default:
            break;
    }

    return IDE_FAILURE;
}

