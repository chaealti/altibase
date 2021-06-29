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
 *
 * $Id: scpfDef.h $
 *
 **********************************************************************/

#ifndef _O_SCPF_DEF_H_
#define _O_SCPF_DEF_H_ 1

#include <smDef.h>
#include <scpDef.h>
#include <iduFile.h>

#define SCPF_VARINT_1BYTE_THRESHOLD        (64)
#define SCPF_VARINT_2BYTE_THRESHOLD        (16384)
#define SCPF_VARINT_3BYTE_THRESHOLD        (4194304)
#define SCPF_VARINT_4BYTE_THRESHOLD        (1073741824)

#define SCPF_WRITE_UINT              SMI_WRITE_UINT
#define SCPF_WRITE_USHORT            SMI_WRITE_USHORT
#define SCPF_WRITE_ULONG             SMI_WRITE_ULONG
#define SCPF_READ_UINT               SMI_READ_UINT
#define SCPF_READ_USHORT             SMI_READ_USHORT
#define SCPF_READ_ULONG              SMI_READ_ULONG

#define SCPF_GET_VARINT_MAXSIZE      (4)
#define SCPF_GET_VARINT_SIZE( src )                                \
     ( ( (*(src)) < SCPF_VARINT_1BYTE_THRESHOLD ) ? 1 :     \
       ( (*(src)) < SCPF_VARINT_2BYTE_THRESHOLD ) ? 2 :     \
       ( (*(src)) < SCPF_VARINT_3BYTE_THRESHOLD ) ? 3 : 4 )

/* UInt�� BigEndian���� Write�Ѵ�. */
#define SCPF_WRITE_VARINT(src, dst){                                 \
     if( (*(UInt*)(src)) < SCPF_VARINT_1BYTE_THRESHOLD )             \
     {                                                               \
        ((UChar*)(dst))[0] = *(UInt*)(src);                          \
     } else                                                          \
     if( (*(UInt*)(src)) < SCPF_VARINT_2BYTE_THRESHOLD )             \
     {                                                               \
        ((UChar*)(dst))[0] = (((*(UInt*)(src))>>8) & 63) | ( 1<<6 ); \
        ((UChar*)(dst))[1] = (*(UInt*)(src)) & 255;                  \
     } else                                                          \
     if( (*(UInt*)(src)) < SCPF_VARINT_3BYTE_THRESHOLD )             \
     {                                                               \
        ((UChar*)(dst))[0] = (((*(UInt*)(src))>>16) & 63) | ( 2<<6 );\
        ((UChar*)(dst))[1] = (((*(UInt*)(src))>> 8) & 255);          \
        ((UChar*)(dst))[2] = (*(UInt*)(src)) & 255;                  \
     } else                                                          \
     if( (*(UInt*)(src)) < SCPF_VARINT_4BYTE_THRESHOLD )             \
     {                                                               \
        ((UChar*)(dst))[0] = (((*(UInt*)(src))>>24) & 63) | ( 3<<6 );\
        ((UChar*)(dst))[1] = (((*(UInt*)(src))>>16) & 255);          \
        ((UChar*)(dst))[2] = (((*(UInt*)(src))>> 8) & 255);          \
        ((UChar*)(dst))[3] = (*(UInt*)(src)) & 255;                  \
     } else                                                          \
     {                                                               \
         IDE_ASSERT( 0 );                                            \
     }                                                               \
}

/* UInt�� BigEndian���� Write�Ѵ�. */
#define SCPF_READ_VARINT(src, dst){                    \
    switch( (((UChar*)(src))[0]) >> 6 )                \
    {                                                  \
    case 0:                                            \
        *(dst) = (((UChar*)(src))[0] & 63);            \
        break;                                         \
    case 1:                                            \
        *(dst) = (((((UChar*)(src))[0]) & 63)<<8)      \
               | (((UChar*)(src))[1]);                 \
        break;                                         \
    case 2:                                            \
        *(dst) = (((((UChar*)(src))[0]) & 63)<<16)     \
               | ((((UChar*)(src))[1])<<8)             \
               | ((((UChar*)(src))[2]));               \
        break;                                         \
    case 3:                                            \
        *(dst) = (((((UChar*)(src))[0]) & 63)<<24)     \
               | ((((UChar*)(src))[1])<<16)            \
               | ((((UChar*)(src))[2])<<8)             \
               | ((((UChar*)(src))[3]));               \
        break;                                         \
    default:                                           \
        break;                                         \
    }                                                  \
}

#define SCPF_GET_SLOT_VAL_LEN( src )                       \
    ( *(src) - SCPF_GET_VARINT_SIZE(src) )

#define SCPF_LOB_LEN_SIZE     ID_SIZEOF( UInt )


//File�� Version
#define SCPF_VERSION_1        (1)
#define SCPF_VERSION_LATEST   SCPF_VERSION_1

#define SCPF_DPFILE_EXT       ((SChar*)".dpf")
#define SCPF_DPFILE_SEPARATOR '_'

// BlockInfo�� File�κ��� ���� Block�� �÷����� �޸𸮻���
// ��ü��, Export�� 1��, Import�� �ּ� 2���� ����Ѵ�.
#define SCPF_MIN_BLOCKINFO_COUNT   (2)

#define SCPF_OFFSET_NULL           (0)

// ���� �������� ���� BlockBuffer�� Position
typedef struct scpfBlockBufferPosition
{
    UInt  mSeq;         // �а��ִ� Block Info�� Block Sequence.
    UInt  mID;          // ���� �а��ִ� Block�� File�� ��ȣ.
    UInt  mOffset;      // Block�� Offset 
    UInt  mSlotSeq;     // Block�� Slot��ȣ
} scpfBlockBufferPosition;

// ���� �������� ���� File�� ��ġ
typedef struct scpfFilePosition
{
    UInt       mIdx;      // ���� �ٷ�� �ִ� ������ ��ȣ
    ULong      mOffset;   // ������� �аų� �� FileOffset
} scpfFilePosition;

// �ϳ��� Block�� ���� �������� ����
typedef struct scpfBlockInfo
{
    //Store
    UInt       mCheckSum;             // Checksum
    UInt       mBlockID;              // �� Block�� File �� Seq
    SLong      mRowSeqOfFirstSlot;    // Block�� ù��° Slot�� RowSeq
    SLong      mRowSeqOfLastSlot;     // Block�� ������ Slot RowSeq

    UInt       mFirstRowSlotSeq;      // Block�� ù��° Row�� SlotSeq 
    UInt       mFirstRowOffset;       // Block�� ù��° Row�� Offset
    idBool     mHasLastChainedSlot;   // Block�� ������ Slot�� ���� Block��
                                      // Chained�ž� �ִ��� ����
    UInt       mSlotCount ;           // Block�� �� Slot�� ����
 
    //Runtime
    UChar    * mBlockPtr;             // �� Block�� ���� ���� ������
} scpfBlockInfo;

extern smiDataPortHeaderDesc  gSCPfBlockHeaderEncDesc[];

//File�� Header�� ��ϵȴ�.
typedef struct scpfHeader
{
    UInt  mFileHeaderSize;     // File�� ��� �κ��� ũ��
    UInt  mBlockHeaderSize;    // Block�� Header�� ũ��
    UInt  mBlockSize;          // Block �ϳ��� ũ��
    UInt  mBlockInfoCount;     // Import�ϴµ� �ʿ��� BlockInfo�� ����
    UInt  mMaxRowCountInBlock; // �ϳ��� Block�� ���� Row�� �ִ� ����. 
                               // smiRow4DP�� ũ�� ������ ����.
                               // Block�� Row�� UInt���� ���� �� ����
                               //     (Block�� ũ�Ⱑ UInt�� �Ѱ��̱� )
    UInt  mBlockCount;         // �� File�� �� Block ����.
                               // Export�ÿ��� ������� ������ Block�� ����
                               // Import�ÿ��� File�� ������ �ִ� Block�� ����
    SLong mRowCount;           // �� File�� ���� Row�� ����
    SLong mFirstRowSeqInFile;  // File�� ù��° Row
    SLong mLastRowSeqInFile;   // File�� ���ڹ� Row
} scpfHeader;

extern smiDataPortHeaderDesc  gSCPfFileHeaderEncDesc[];

// Data�� ������ File�� Handle
typedef struct scpfHandle
{
    scpHandle               mCommonHandle;
    scpfHeader              mFileHeader;       // FileHeader

    scpfBlockBufferPosition mBlockBufferPosition;    // �������� Block ��ġ
    scpfFilePosition        mFilePosition;           // �������� ���� ��ġ


    /* ��)    �ʱ����
     *        BlockMap 0,1,2,3     
     *
     *     => 0,1�� Block�� Row�� ��� ó���Ͽ���
     *        (BlockBufferPosition.mSeq�� 2�� ����Ŵ)
     *
     *     => 2�� ��ŭ Shift��Ŵ
     *        BlockMap  2,3,0,1    
     *
     *     => ���� 0,1���� 4,5�� Block�� �ø�
     *        BlockMap  2,3,4,5    
     */    
    scpfBlockInfo     ** mBlockMap;         // �а��� �޸𸮻��� Buffer.
                                            // Circular�ϰ� ��ȯ�ȴ�.
                  
    smiRow4DP          * mRowList;          // Import�� ���� Row��(Imp)

    /*  Block�� �а� ���µ� �ʿ��� ���� �޸� ������ Block���� �ϳ��ϳ�
     * �Ҵ����� �ʰ�, �ѹ��� N���� �Ҵ��� �� BlockMap�� �й��Ѵ�.
     *  ���� �Ҵ��� �޸𸮸� �����Ҷ��� ���� �Ҵ���� ����� Pointer
     * �� ������ �־�� �Ѵ�. ���� �� ������ �� Pointer�̴�. */
    scpfBlockInfo      * mBlockBuffer;      // BlockInfo�� Buffer
    smiValue           * mValueBuffer;      // RowList�� ValueList(Imp)
    UChar              * mAllocatedBlock;   // �Ҵ��� Block
    UInt               * mColumnMap;        // columnHeader�� ColumnMap���
                                            // �� �ӽ÷� �����صд�

    UInt       mColumnChainingThreshold;// Column�� Chainning�Ǵ� ũ��

    SLong      mRowSeq;                 // Import/Export�������� Row
    SLong      mLimitRowSeq;            // Import�Ϸ��� ������ Row(Imp)
    SLong      mSplit;                  // ������ �ɰ����� �б���

    UChar    * mChainedSlotBuffer;      // Chained�� Row�� ����ϴ� ����(Imp)
    UInt       mUsedChainedSlotBufSize; // ���� ChainedRowBuf ũ�� (Imp)

    idBool     mIsFirstSlotInRow;       // Row�� ù Slot�ΰ� (Exp)

    UInt       mAllocatedLobSlotSize;   // ��� �����ص� Lob������ ũ��
    UInt       mRemainLobLength;        // ���� ����ϴ� Lob�� ���� ����
    UInt       mRemainLobColumnCount;   // ����ؾ� �ϴ� ���� LobColumn����

    idBool     mOpenFile;
    iduFile    mFile;      
    SChar      mDirectory[SM_MAX_FILE_NAME];  // Directory�̸�
    SChar      mObjName[SM_MAX_FILE_NAME];    // Object�̸�
    SChar      mFileName[SM_MAX_FILE_NAME];   // ������ �̸�

} scpfHandle;

#endif // _O_SCPF_DEF_H_
