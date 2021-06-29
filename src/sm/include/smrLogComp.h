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
 * $Id: smrLogComp.h 15368 2006-03-23 01:14:43Z kmkim $
 **********************************************************************/

#ifndef _O_SMR_LOG_COMP_H_
#define _O_SMR_LOG_COMP_H_ 1

#include <idl.h>
#include <iduMemoryHandle.h>

#include <smDef.h>
#include <smrLogFile.h>

/*
   Log Compressor/Decompressor

   �� Ŭ������ �α��� ����� ������ ����Ѵ�.

   Log File Group������ �α׸� �а� ���� smrLogMgr��
   ���� �α׷��ڵ带 ���Ϸκ��� �а� ���� smrLogFile�� �߰�����
   �α��� ����� ������ ����Ѵ�.
   
   * �α��� ���� ����
     - smrLogMgr�� tryLogCompression���� ������ �α��� ������
       smrLogComp�� �����Ѵ�.
     - ����� �α׸� �̿��Ͽ� lockAndWriteLog ���� �α׸�
       �α����Ͽ� ����Ѵ�.
   |  ---------------------------------------------- 
   V          smrLogMgr::writeLog
   |  ----------------------------------------------
              smrLogMgr::tryLogCompression
   V          smrLogMgr::lockAndWriteLog
   |  ----------------------------------------------
   V          smrLogFile::writeLog
      ----------------------------------------------
   
   * �α��� �������� ����
     - ������ Layer�� smrLogFile�κ��� �ϳ��� �α׸� �д´�.
     - smrLogComp������ ����� �α��� ������ �����Ͽ�
       ���������� �α��� �����͸� �����Ѵ�.
     - smrLogMgr�� �̸� �޾Ƽ� �״�� �����Ѵ�.
      ---------------------------------------------- 
   ^         smrLogMgr::readLog
      ---------------------------------------------- 
   ^         smrLogMgr::readLog
   |  ----------------------------------------------
   ^         smrLogComp::readLog
   |  ----------------------------------------------
   ^         smrLogFile::readLog
   |  ----------------------------------------------

    * BUG-35392 ������ ���� Flag�� 1->4 byte�� ���� �Ǿ���.
   * ����� �α��� ����
      +---------+----------------+-----------------------------------------+
      | 4 byte  | ����α� Flag  | 0x80 ��Ʈ�� 1�� ������                  |
      |         |                | �����α��� Flag                         |
      +---------+----------------+-----------------------------------------+
      | 4 bytes | ����α�ũ��   | �����α��� ����� �α��� ũ��           |
      +---------+----------------+-----------------------------------------+
      | 8 bytes | LSN            | �α��� Serial Number(mFileNo + mOffset) |
      +---------+----------------+-----------------------------------------+
      | 2 bytes | Magic          | �α��� Magic Value                      |
      +---------+----------------+-----------------------------------------+
      | 4 bytes | �����α�ũ��   | ���� �α��� ũ��                        |
      +---------+----------------+-----------------------------------------+
      | N bytes | ����α�       | ����� �α� ������                      |
      +---------+----------------+-----------------------------------------+
      | 4 bytes | �α� Tail      | �Ϻθ� ��ϵ� �α� �Ǻ��� ���� Tail     |
      |         |                | ���� �α��� ũ�Ⱑ ��ϵ�               |
      +---------+----------------+-----------------------------------------+

   * ����α��� ��� ����
   
     - �α��� SN�� Magic�� ��ϵ��� ���� �����α׸� �����Ѵ�
       - ������ ������ �� �� �ֵ���, �α� ������ Mutex�� ����
         ���� ä�� ����� Log�� �����Ѵ�.
     
     - Log ������ Mutex�� ��´�
       - SN�� Magic���� ���´�.
       - ����α��� Head�� SN�� Magic���� ����Ѵ�.
       - ����α׸� �α����Ͽ� ����Ѵ�
     - Log ������ Mutex�� Ǭ��. 

   * ����α��� �ǵ� ����
     - ����α��� ������ �����Ѵ�.
     - ����α��� Head�� ��ϵ� SN�� Magic�� �����α�(�����α�)��
       Head�� �����Ѵ�.
 */

/* BUG-35392 */
// ����� �α��� Head ũ��
// smrLogComp.h�� ����� ����α� ��������

// Flag, Uchar -> UInt�� Ÿ���� ���� �Ǿ���
#define SMR_COMP_LOG_FLAG_SIZE      ( ID_SIZEOF( UInt) )

// ���� �� �α� ũ��
#define SMR_COMP_LOG_COMP_SIZE      ( ID_SIZEOF( UInt ) )
// LSN 
#define SMR_COMP_LOG_LSN_SIZE       ( ID_SIZEOF( smLSN ) )
// Magic
#define SMR_COMP_LOG_MAGIC_SIZE     ( ID_SIZEOF( smMagic ) )
// ���� �α� ũ��
#define SMR_COMP_LOG_DECOMP_SIZE    ( ID_SIZEOF( UInt ) )
// BUG-46944 Comp Log�� �߰��Ǵ� TableOID�� ũ��
#define SMR_COMP_LOG_TABLEOID_SIZE  ( ID_SIZEOF( smOID ) )

// ����� �α��� Tail ũ��
#define SMR_COMP_LOG_TAIL_SIZE      ( ID_SIZEOF( UInt ) )

#define SMR_COMP_LOG_HEAD_SIZE      ( SMR_COMP_LOG_FLAG_SIZE +      \
                                      SMR_COMP_LOG_COMP_SIZE +      \
                                      SMR_COMP_LOG_LSN_SIZE  +      \
                                      SMR_COMP_LOG_MAGIC_SIZE +     \
                                      SMR_COMP_LOG_DECOMP_SIZE +    \
                                      SMR_COMP_LOG_TABLEOID_SIZE )

// ����� �α��� �������
// N����Ʈ�� "����α�"�� ������ ������ �κ��� ũ��
#define SMR_COMP_LOG_OVERHEAD \
           ( SMR_COMP_LOG_HEAD_SIZE + SMR_COMP_LOG_TAIL_SIZE )

//���� �� �α��� Flag offset
#define SMR_COMP_LOG_FLAG_OFFSET    ( 0 ) // �׻� ���� �տ� �־�� ��

//����� �α��� ũ�Ⱑ ����� offset
#define SMR_COMP_LOG_SIZE_OFFSET    ( SMR_COMP_LOG_FLAG_SIZE )

// ����α� Head���� Log SN offset
#define SMR_COMP_LOG_LSN_OFFSET     ( SMR_COMP_LOG_FLAG_SIZE +      \
                                      SMR_COMP_LOG_COMP_SIZE )

// ����α� Head���� MAGIC Value offset
#define SMR_COMP_LOG_MAGIC_OFFSET   ( SMR_COMP_LOG_FLAG_SIZE +      \
                                      SMR_COMP_LOG_COMP_SIZE +      \
                                      SMR_COMP_LOG_LSN_SIZE )

// ����α� Head���� ���� �α� ũ�� offset
#define SMR_COMP_LOG_DECOMP_OFFSET  ( SMR_COMP_LOG_FLAG_SIZE +      \
                                      SMR_COMP_LOG_COMP_SIZE +      \
                                      SMR_COMP_LOG_LSN_SIZE  +      \
                                      SMR_COMP_LOG_MAGIC_SIZE )

// ����α� Head���� TableOID offset
#define SMR_COMP_LOG_TABLEOID_OFFSET   ( SMR_COMP_LOG_FLAG_SIZE +      \
                                         SMR_COMP_LOG_COMP_SIZE +      \
                                         SMR_COMP_LOG_LSN_SIZE  +      \
                                         SMR_COMP_LOG_MAGIC_SIZE +     \
                                         SMR_COMP_LOG_DECOMP_SIZE )



class smrLogComp
{
public :
    /* ������ۿ� ����α׸� ����Ѵ�. */
    static IDE_RC createCompLog( iduMemoryHandle    * aCompBufferHandle,
                                 SChar              * aRawLog,
                                 UInt                 aRawLogSize,
                                 SChar             ** aCompLog,
                                 UInt               * aCompLogSize,
                                 smOID                aTableOID );

    // ���� �α��� Head�� SN�� �����Ѵ�.
    static void setLogLSN( SChar * aCompLog,
                           smLSN   aLogLSN );

    // ���� �α��� Head�� MAGIC���� �����Ѵ�.
    static void setLogMagic( SChar * aCompLog,
                             UShort  aMagicValue );
    
    
    /* �α������� Ư�� Offset���� �α� ���ڵ带 �о�´�.
       ����� �α��� ���, �α� ���������� �����Ѵ�.
     */
    static IDE_RC readLog( iduMemoryHandle    * aDecompBufferHandle,
                           smrLogFile         * aLogFile,
                           UInt                 aLogOffset,
                           smrLogHead         * aRawLogHead,
                           SChar             ** aRawLogPtr,
                           UInt               * aLogSizeAtDisk );

    /* �α������� Ư�� Offset���� �α� ���ڵ带 �о�´�.
       ����� �α׶�� ����� ���� �״�� ��ȯ�Ѵ�.
     */
    static IDE_RC readLog4RP( smrLogFile         * aLogFile,
                              UInt                 aLogOffset,
                              smrLogHead         * aRawLogHead,
                              SChar             ** aRawLogPtr,
                              UInt               * aLogSizeAtDisk );

   /* �����, Ȥ�� ������� ���� �α� ���ڵ�κ���
      ������� ���� ������ Log Head�� Log Ptr�� �����´�.
   */
    static IDE_RC getRawLog( iduMemoryHandle    * aDecompBufferHandle,
                             UInt                 aRawLogOffset,
                             SChar              * aRawOrCompLog,
                             smMagic              aValidLogMagic,
                             smrLogHead         * aRawLogHead,
                             SChar             ** aRawLogPtr,
                             UInt               * aLogSizeAtDisk );

    /* ����� �α��� ���������� �����Ѵ� */
    static IDE_RC decompressCompLog( iduMemoryHandle    * aDecompBufferHandle,
                                     UInt                 aCompLogOffset,
                                     SChar              * aCompLog,
                                     smMagic              aValidLogMagic,
                                     smrLogHead         * aRawLogHead,
                                     SChar             ** aRawLog,
                                     UInt               * aLogSizeAtDisk );

    /* ���� ��� �α����� �Ǻ��Ѵ� */
    static IDE_RC shouldLogBeCompressed( SChar * aRawLog,
                                         UInt aRawLogSize,
                                         idBool * aDoCompLog );

    
    /* ����� �α����� ���θ� �Ǻ��Ѵ�. */
    static inline idBool isCompressedLog( SChar * aRawOrCompLog );
    /* ����α׸� Log�� �����ִ� TableOID�� �ƴϸ� NULL OID�� ��ȯ�Ѵ�. */
    static inline smOID getTableOID( SChar * aRawOrCompLog );

    static UInt getCompressedLogSize( SChar  * aCompLog );

    static void setCompressedLogSize( SChar  * aCompLog,
                                      UInt     aCompLogSize );

    /* PROJ-1923 ALTIBASE HDB Disaster Recovery
     * Ÿ ��⿡�� ����ϱ� ���� private -> public ��ȯ */
    /* ���������� ���� �޸� ���� �غ� */
    static IDE_RC prepareDecompBuffer( iduMemoryHandle   * aDecompBufferHandle,
                                        UInt                aRawLogSize,
                                        SChar            ** aDecompBuffer,
                                        UInt              * aDecompBufferSize);


private:
    /* ����� �α��� ���������� �����Ѵ� */
    static IDE_RC decompressLog( iduMemoryHandle    * aDecompBufferHandle,
                                 UInt                 aCompLogOffset,
                                 SChar              * aCompLog,
                                 smMagic              aValidLogMagic,
                                 SChar             ** aRawLog,
                                 smMagic            * aMagicValue,
                                 smLSN              * aLogLSN,
                                 UInt               * aCompLogSize );
    
    /* �����α׸� ������ �����͸� ����α� Body�� ����Ѵ�. */
    static IDE_RC writeCompBody( SChar  * aCompDestPtr,
                                 UInt     aCompDestSize,
                                 SChar  * aRawLog,
                                 UInt     aRawLogSize,
                                 UInt   * aCompressedRawLogSize );
        
    /* ����α��� Head�� ����Ѵ�. */
    static void writeCompHead( SChar  * aHeadDestPtr,
                               SChar  * aRawLog,
                               UInt     aRawLogSize,
                               UInt     aCompressedRawLogSize,
                               smOID    aTableOID );
    
    /*  ����α��� Tail�� ����Ѵ�. */
    static void writeCompTail( SChar * aTailDestPtr,
                               UInt    aRawLogSize );
    
    /* ���� ����� ����� �α׸� ���� �޸� ���� �غ� */
    static IDE_RC prepareCompBuffer( iduMemoryHandle    * aCompBufferHandle,
                                     UInt                 aRawLogSize,
                                     SChar             ** aCompBuffer,
                                     UInt               * aCompBufferSize);

    /* ����α׸� �ؼ��Ѵ�. */
    static IDE_RC analizeCompLog( SChar   * aCompLog,
                                  UInt      aCompLogOffset,
                                  smMagic   aValidLogMagic,
                                  idBool  * aIsValid,
                                  smMagic * aMagicValue,
                                  smLSN   * aLogLSN,
                                  UInt    * aRawLogSize,
                                  UInt    * aCompressedRawLogSize );
    
    /* VALID���� ���� �α׸� ������� ���� ���·� �����Ѵ�. */
    static IDE_RC createInvalidLog( iduMemoryHandle    * aDecompBufferHandle,
                                    SChar             ** aInvalidRawLog );
    
    // Log Record�� ��ϵ� Tablespace ID�� �����Ѵ�.
    static void getSpaceIDOfLog( smrLogHead * aLogHead,
                                 SChar      * aRawLog,
                                 scSpaceID  * aSpaceID );

};

/******************************************************************************
 * BUG-35392
 * ���� log�� head�� tail�� ũ�Ⱑ ���Ե� ���� �α�ũ�⸦ ��ȯ�Ѵ�.
 *
 * [IN]  aCompLog      - ����� �α��� ���� �ּ�
 * [OUT] aCompLogSize  - ����� �α��� ũ��
 *****************************************************************************/
inline UInt smrLogComp::getCompressedLogSize( SChar  * aCompLog )
{
    UInt    sCompLogSizeWithoutHdrNTail;
    UInt    sLogFlag    = 0;

    IDE_DASSERT( aCompLog   != NULL );

    idlOS::memcpy( &sLogFlag,
                   aCompLog,
                   ID_SIZEOF( UInt ) );    

    IDE_DASSERT( (sLogFlag & SMR_LOG_COMPRESSED_MASK) == SMR_LOG_COMPRESSED_OK );

    //BUG-34791 SIGBUS occurs when recovering multiplex logfiles
    idlOS::memcpy( &sCompLogSizeWithoutHdrNTail, 
                   (aCompLog + SMR_COMP_LOG_SIZE_OFFSET), SMR_COMP_LOG_COMP_SIZE);

    return sCompLogSizeWithoutHdrNTail;
}

/******************************************************************************
 * BUG-35392
 * ���� log�� head�� tail�� ũ�Ⱑ ���Ե� ���� �α�ũ�⸦ �����Ѵ�.
 *
 * [IN]  aCompLog      - ����� �α��� ���� �ּ�
 * [OUT] aCompLogSize  - ����� �α��� ũ��
 *****************************************************************************/
inline void smrLogComp::setCompressedLogSize( SChar  * aCompLog,
                                              UInt     aCompLogSize )
{
    UInt    sLogFlag    = 0;

    IDE_DASSERT( aCompLog   != NULL );

    idlOS::memcpy( &sLogFlag,
                   aCompLog,
                   ID_SIZEOF( UInt ) );    

    IDE_DASSERT( (sLogFlag & SMR_LOG_COMPRESSED_MASK) != SMR_LOG_COMPRESSED_OK );

    //BUG-34791 SIGBUS occurs when recovering multiplex logfiles
    idlOS::memcpy( (aCompLog + SMR_COMP_LOG_SIZE_OFFSET),
                   &aCompLogSize,
                   SMR_COMP_LOG_COMP_SIZE );
}


/******************************************************************************
 * BUG-35392
 * ���� �α��� Head�� LSN�� �����Ѵ�.
 *
 * - ���� ����α� ������ ���� �α��� Head�� Log�� SN�� 0���� ��ϵȴ�.
 * - ���� �α����Ͽ� �α� ����Ҷ� �α� ������ Mutex�� ���� ���·�
 *   �αװ� ��ϵ� SN�� ���� �� �Լ��� �̿��Ͽ� ����Ѵ�.
 *
 * [IN] aCompLog - ����α�
 * [IN] aLogSN   - �α��� SN
 *****************************************************************************/
inline void smrLogComp::setLogLSN( SChar * aCompLog,
                                   smLSN   aLogLSN )
{
    IDE_DASSERT( aCompLog != NULL );

    idlOS::memcpy( aCompLog + SMR_COMP_LOG_LSN_OFFSET,
                   &aLogLSN,
                   SMR_COMP_LOG_LSN_SIZE );
}


/******************************************************************************
 * BUG-35392
 * ���� �α��� Head�� MAGIC���� �����Ѵ�.
 *
 * - ���� ����α� ������ ���� �α��� Head�� Log�� Magic�� 0���� ��ϵȴ�.
 *
 * - ���� �α����Ͽ� �α� ����Ҷ� �α� ������ Mutex�� ���� ���·�
 *   �αװ� ��ϵ� LSN�� ���� Magic�� ����ϰ� �� �Լ��� �̿��Ͽ� ����Ѵ�.
 *
 * [IN] aCompLog - ����α�
 * [IN] aLogSN   - �α��� SN
 *****************************************************************************/
inline void smrLogComp::setLogMagic( SChar  * aCompLog,
                                     smMagic  aMagicValue )
{
    IDE_DASSERT( aCompLog != NULL );

    idlOS::memcpy( aCompLog + SMR_COMP_LOG_MAGIC_OFFSET,
                   & aMagicValue,
                   SMR_COMP_LOG_MAGIC_SIZE );
}

/******************************************************************************
 * ����� �α����� ���θ� �Ǻ��Ѵ�.
 *
 * ����α�, ����� �α� ��� ù��° ����Ʈ�� Flag�� �ΰ�,
 * ����Ǿ����� ���θ� SMR_LOG_COMPRESSED_OK ��Ʈ�� ǥ���Ѵ�.
 *
 *  [IN] aRawOrCompLog - ����α� Ȥ�� ������� ���� ������ �α�
 *****************************************************************************/
inline idBool smrLogComp::isCompressedLog( SChar * aRawOrCompLog )
{
    IDE_DASSERT( aRawOrCompLog != NULL );

    idBool  sIsCompressed;
    UInt    sLogFlag;

    /* BUG-35392 smrLogHead�� mFlag�� UInt �̹Ƿ� memcpy�� �о�� 
     * align ������ ȸ���� �� �ִ�. */
    idlOS::memcpy( &sLogFlag,
                   aRawOrCompLog,
                   SMR_COMP_LOG_FLAG_SIZE );

    if ( ( sLogFlag & SMR_LOG_COMPRESSED_MASK )
         ==  SMR_LOG_COMPRESSED_OK )
    {
        sIsCompressed = ID_TRUE;
    }
    else
    {
        sIsCompressed = ID_FALSE;
    }

    return sIsCompressed;
}

/******************************************************************************
 * BUG-46944 LogHeader�� �Ѱ��ָ� ����� �α����� Ȯ�� �ؼ�
 * ����α׸� Log�� �����ִ� TableOID��
 * ����αװ� �ƴϸ� NULL OID�� ��ȯ�Ѵ�.
 * ��������� ��ϵ� TableOID�� NULL OID�� ��� �ϴ� ���� �����Ѵ�.
 *****************************************************************************/
inline smOID smrLogComp::getTableOID( SChar * aRawOrCompLog )
{
    smOID sTableOID = SM_OID_NULL;

    if ( isCompressedLog( aRawOrCompLog ) == ID_TRUE )
    {
        idlOS::memcpy( &sTableOID,
                       aRawOrCompLog + SMR_COMP_LOG_TABLEOID_OFFSET,
                       SMR_COMP_LOG_TABLEOID_SIZE );
    }

    return sTableOID;
}

#endif /* _O_SMR_LOG_COMP_H_ */

