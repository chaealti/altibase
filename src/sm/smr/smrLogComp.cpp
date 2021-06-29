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
 * $Id:$
 **********************************************************************/

#include <iduLZ4.h>

#include <smErrorCode.h>
#include <smrLogComp.h>
#include <smrLogHeadI.h>
#include <sctTableSpaceMgr.h>

/* �α������� Ư�� Offset���� �α� ���ڵ带 �о�´�.
   ����� �α��� ���, �α� ���������� �����Ѵ�.

   [IN] aDecompBufferHandle   - ���� ���� ������ �ڵ�
   [IN] aLogFile            - �α׸� �о�� �α�����
   [IN] aLogOffset          - �α׸� �о�� ������
   [OUT] aRawLogHead        - �α��� Head
   [OUT] aRawLogPtr         - �о �α� (���������� �α�)
   [OUT] aLogSizeAtDisk     - ���Ͽ��� �о �α� �������� ��
*/
IDE_RC smrLogComp::readLog( iduMemoryHandle    * aDecompBufferHandle,
                            smrLogFile         * aLogFile,
                            UInt                 aLogOffset,
                            smrLogHead         * aRawLogHead,
                            SChar             ** aRawLogPtr,
                            UInt               * aLogSizeAtDisk )
{
    // ����� �α׸� �д� ��� aDecompBufferHandle�� NULL�� ���´�
    IDE_DASSERT( aLogFile       != NULL );
    IDE_DASSERT( aRawLogHead    != NULL );
    IDE_DASSERT( aRawLogPtr     != NULL );
    IDE_DASSERT( aLogOffset < smuProperty::getLogFileSize() );
    IDE_DASSERT( aLogSizeAtDisk != NULL );

    smMagic     sValidLogMagic;
    SChar     * sRawOrCompLog;

    aLogFile->read(aLogOffset, &sRawOrCompLog);

    // File No�� Offset���κ��� Magic�� ���
    sValidLogMagic = smrLogFile::makeMagicNumber( aLogFile->getFileNo(),
                                                  aLogOffset );

    IDE_TEST( getRawLog( aDecompBufferHandle,
                         aLogOffset,
                         sRawOrCompLog,
                         sValidLogMagic,
                         aRawLogHead,
                         aRawLogPtr,
                         aLogSizeAtDisk ) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    // BUG-26695 log decompress size ����ġ�� Recovery �����մϴ�.
    IDE_PUSH();
    IDE_SET( ideSetErrorCode( smERR_ABORT_INVALID_LOGFILE,
                              aLogFile->getFileName() ) );
    IDE_POP();

    return IDE_FAILURE;
}

/* �����, Ȥ�� ������� ���� �α� ���ڵ�κ���
   ������� ���� ������ Log Head�� Log Ptr�� �����´�.

   ( ����� �α��� ���, �α� ���������� �����Ѵ�. )

   [IN] aDecompBufferHandle - ���� ���� ������ �ڵ�
   [IN] aRawLogOffset       - �α� Offset
   [IN] aRawOrCompLog       - �α׸� �о�� ������
   [IN] aValidLogMagic      - �α��� �������� Magic��
   [OUT] aRawLogHead        - �α��� Head
   [OUT] aRawLogPtr         - �о �α� (���������� �α�)
   [OUT] aLogSizeAtDisk     - ���Ͽ��� �о �α� �������� ��
*/
IDE_RC smrLogComp::getRawLog( iduMemoryHandle    * aDecompBufferHandle,
                              UInt                 aRawLogOffset,
                              SChar              * aRawOrCompLog,
                              smMagic              aValidLogMagic,
                              smrLogHead         * aRawLogHead,
                              SChar             ** aRawLogPtr,
                              UInt               * aLogSizeAtDisk )
{
    // ����� �α׸� �д� ��� aDecompBufferHandle�� NULL�� ���´�
    IDE_DASSERT( aRawOrCompLog  != NULL );
    IDE_DASSERT( aRawLogHead    != NULL );
    IDE_DASSERT( aRawLogPtr     != NULL );
    IDE_DASSERT( aLogSizeAtDisk != NULL );    

    smMagic      sMagicValue;
    smLSN        sLogLSN;
    smrLogHead   sInvalidLogHead;
    idBool       sIsValid = ID_TRUE;

    /* BUG-38962
     * Magic Number�� ���� �˻��ؼ� invalid ���θ� �Ǵ��Ѵ�.
     * invalid log�� ������ ����� �α׷� ����� ��, ���� ��⿡�� ���� ó���Ѵ�. 
     */
    idlOS::memcpy( &sMagicValue,
                   aRawOrCompLog + SMR_COMP_LOG_MAGIC_OFFSET,
                   SMR_COMP_LOG_MAGIC_SIZE );

    if ( aValidLogMagic == sMagicValue )
    {
        sIsValid = ID_TRUE;
    }
    else
    {
        sIsValid = ID_FALSE;
    }

    if( ( isCompressedLog( aRawOrCompLog ) == ID_TRUE ) &&
        ( sIsValid   == ID_TRUE ) /* BUG-39462 */ )
    {
        /* BUG-35392 */
        if( smrLogHeadI::isDummyLog( aRawOrCompLog ) == ID_FALSE )
        {
            IDE_TEST( decompressCompLog( aDecompBufferHandle,
                                        aRawLogOffset,
                                        aRawOrCompLog,
                                        aValidLogMagic,
                                        aRawLogHead,
                                        aRawLogPtr,
                                        aLogSizeAtDisk )
                      != IDE_SUCCESS );
        }
        else
        {
            /* Dummy log�̸� size�� return */
            *aRawLogPtr = aRawOrCompLog;

            /* Log Header�� �����Ѵ�.*/
            idlOS::memcpy(aRawLogHead, aRawOrCompLog, ID_SIZEOF(smrLogHead));

            idlOS::memcpy( (void*)&sLogLSN,
                           aRawOrCompLog + SMR_COMP_LOG_LSN_OFFSET,
                           SMR_COMP_LOG_LSN_SIZE );

            smrLogHeadI::setLSN(aRawLogHead, sLogLSN );

            *aLogSizeAtDisk = getCompressedLogSize( aRawOrCompLog );
            IDE_TEST_RAISE( *aLogSizeAtDisk > smuProperty::getLogFileSize(),
                            err_invalid_log );
        }
    }
    else
    {
        /* ����� �αװ� �ƴϴ�. ��� ����. */
        *aRawLogPtr = aRawOrCompLog;

        /* Log Header�� �����Ѵ�.*/
        idlOS::memcpy(aRawLogHead, aRawOrCompLog, ID_SIZEOF(smrLogHead));

        /* sIsValid�� ID_FALSE�� �ֱ� ������ �˻�� ���� �ʴ´�. */
        *aLogSizeAtDisk = smrLogHeadI::getSize( aRawLogHead );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( err_invalid_log );
    {
        idlOS::memcpy( &sInvalidLogHead,
                       aRawOrCompLog,
                       ID_SIZEOF(smrLogHead) );

        ideLog::log( SM_TRC_LOG_LEVEL_MRECOV,
                     SM_TRC_MRECOVER_INVALID_DECOMP_LOG_HEAD,
                     sInvalidLogHead.mFlag,
                     sInvalidLogHead.mType,
                     sInvalidLogHead.mMagic,
                     sInvalidLogHead.mSize,
                     sInvalidLogHead.mPrevUndoLSN.mFileNo,
                     sInvalidLogHead.mPrevUndoLSN.mOffset,
                     sInvalidLogHead.mTransID,
                     sInvalidLogHead.mReplSvPNumber );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/* ������ۿ� ����α׸� ����Ѵ�.

   �����α��� ����� ũ�⸦ �˾Ƴ��� ����
   �����α��� ������ ���� �ǽ��Ѵ�.

   [IN] aCompBufferHandle - ���� ������ �ڵ�
   [IN] aRawLog           - ����Ǳ� ���� ���� �α�
   [IN] aRawLogSize       - ����Ǳ� ���� ���� �α��� ũ��
   [IN] aCompLog          - ����� �α�
   [IN] aCompLogSize      - ����� �α��� ũ��
 */
IDE_RC smrLogComp::createCompLog( iduMemoryHandle    * aCompBufferHandle,
                                  SChar              * aRawLog,
                                  UInt                 aRawLogSize,
                                  SChar             ** aCompLog,
                                  UInt               * aCompLogSize,
                                  smOID                aTableOID )
{
    IDE_DASSERT( aCompBufferHandle != NULL );
    IDE_DASSERT( aRawLog       != NULL );
    IDE_DASSERT( aRawLogSize    > 0  );
    IDE_DASSERT( aCompLog      != NULL );
    IDE_DASSERT( aCompLogSize  != NULL );

    SChar * sCompBuffer     = NULL;
    UInt    sCompBufferSize = 0;
    UInt    sCompressedRawLogSize;

    /* FIT/ART/sm/Bugs/BUG-31009/BUG-31009.tc */
    IDU_FIT_POINT( "1.BUG-31009@smrLogComp::createCompLog" );

    // ******************************************************
    // ������ ���� ���� ���� �غ�
    IDE_TEST( prepareCompBuffer( aCompBufferHandle,
                                 aRawLogSize,
                                 & sCompBuffer,
                                 & sCompBufferSize )
              != IDE_SUCCESS );

    // ******************************************************
    // ����α��� Body��� ( ���� �α׸� ������ ������ )
    IDE_TEST( writeCompBody( sCompBuffer + SMR_COMP_LOG_HEAD_SIZE,
                             sCompBufferSize - SMR_COMP_LOG_OVERHEAD,
                             aRawLog,
                             aRawLogSize,
                             & sCompressedRawLogSize )
              != IDE_SUCCESS );

    // ******************************************************
    // ����α��� Head���
    (void)writeCompHead( sCompBuffer,
                         aRawLog,
                         aRawLogSize,
                         sCompressedRawLogSize,
                         aTableOID );

    // ******************************************************
    // ����α��� Tail���
    (void)writeCompTail( sCompBuffer +
                         SMR_COMP_LOG_HEAD_SIZE +
                         sCompressedRawLogSize,
                         aRawLogSize );

    *aCompLog     = sCompBuffer;
    *aCompLogSize = SMR_COMP_LOG_HEAD_SIZE +
                        sCompressedRawLogSize +
                    SMR_COMP_LOG_TAIL_SIZE;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*
    �����α׸� ������ �����͸� ����α� Body�� ����Ѵ�.
    [IN] aCompDestPtr  - ����α� Body�� ��ϵ� �޸� �ּ�
    [IN] aCompDestSize - ����α� Body�� ��ϵ� �޸��� ũ��
    [IN] aRawLog       - �����α�
    [IN] aRawLogSize   - �����α� ũ��
    [OUT] aCompressedRawLogSize - �����αװ� ����� �������� ũ��
 */
IDE_RC smrLogComp::writeCompBody( SChar  * aCompDestPtr,
                                  UInt     aCompDestSize,
                                  SChar  * aRawLog,
                                  UInt     aRawLogSize,
                                  UInt   * aCompressedRawLogSize )
{
    IDE_DASSERT( aCompDestPtr != NULL );
    IDE_DASSERT( aCompDestSize > 0 );
    IDE_DASSERT( aRawLog      != NULL );
    IDE_DASSERT( aRawLogSize  > 0  );
    IDE_DASSERT( aCompressedRawLogSize != NULL );

    *aCompressedRawLogSize = iduLZ4_compress( aRawLog,       /* source */
                                              aCompDestPtr,  /* dest */
                                              aRawLogSize,   /* sourceSize */
                                              aCompDestSize, /* maxOutputSize */
                                              smuProperty::getLogCompAcceleration() );/* acceleration */

    IDE_ERROR_RAISE( *aCompressedRawLogSize > 0, err_fail_log_compress );
                     
    // ���� �������� ũ��� ��������� ũ�⸦ �Ѿ �� ����.
    IDE_ERROR_RAISE( *aCompressedRawLogSize <= aCompDestSize,
                     err_fail_log_compress );

    return IDE_SUCCESS;

    IDE_EXCEPTION( err_fail_log_compress )
    {
        ideLog::log( SM_TRC_LOG_LEVEL_MRECOV, 
                     "Log compress failed - invalid log size: %"ID_UINT32_FMT"", 
                     *aCompressedRawLogSize );

    }
    IDE_EXCEPTION_END;
    /* �α� ���࿡ ������ ��� ���ܰ� �ƴ϶� �׳� ����� �α׸� ��� �Ѵ�. */
    return IDE_FAILURE;
}

/*
    ����α��� Head�� ����Ѵ�.

    [IN] aHeadDestPtr - ����α��� Head�� ��ϵ� �޸� �ּ�
    [IN] aRawLog      - ���� ������ ���� �α�
    [IN] aRawLogSize  - ���� ������ ���� �α� ũ��
    [IN] aCompressedRawLogSize - ����� �α��� ũ��

 */
void smrLogComp::writeCompHead( SChar  * aHeadDestPtr,
                                SChar  * aRawLog,
                                UInt     aRawLogSize,
                                UInt     aCompressedRawLogSize,
                                smOID    aTableOID )
{
    IDE_DASSERT( aHeadDestPtr          != NULL );
    IDE_DASSERT( aRawLog               != NULL );
    IDE_DASSERT( aRawLogSize            > 0 );
    IDE_DASSERT( aCompressedRawLogSize  > 0 );

    UInt          sCompLogFlag  = 0;
#ifdef DEBUG
    UInt          sBufferOffset = 0;
#endif
    smrLogHead  * sRawLogHead = (smrLogHead *) aRawLog ;

    /* 4 byte Flag */
    {
        IDE_DASSERT( ID_SIZEOF( sCompLogFlag ) == SMR_COMP_LOG_FLAG_SIZE );

        sCompLogFlag = smrLogHeadI::getFlag( sRawLogHead );

        IDE_ASSERT( ( sCompLogFlag & SMR_LOG_COMPRESSED_MASK )
                    == SMR_LOG_COMPRESSED_NO );

        sCompLogFlag |= SMR_LOG_COMPRESSED_OK;

        /* smrCompResPool�� 8Byte align �� �´� ���� : memcpy ���� �����Ѵ� */
        smrLogHeadI::setFlag( (smrLogHead *)aHeadDestPtr, sCompLogFlag );
#ifdef DEBUG
        sBufferOffset += SMR_COMP_LOG_FLAG_SIZE;
#endif
    }

    /* 4 bytes  ����� �α�ũ�� */
    {
        IDE_DASSERT( ID_SIZEOF( aCompressedRawLogSize ) == SMR_COMP_LOG_COMP_SIZE );

        /* BUG-35392
         * ����� �α� ũ�� + head,tail�� ũ�⸦ ���� ��ü ����α� ũ�� ���� */
        aCompressedRawLogSize += SMR_COMP_LOG_OVERHEAD;
        /* smrCompResPool�� 8Byte align �� �´� ���� : memcpy ���� �����Ѵ� */
        smrLogHeadI::setSize( (smrLogHead *)aHeadDestPtr, aCompressedRawLogSize );
#ifdef DEBUG
        sBufferOffset += SMR_COMP_LOG_COMP_SIZE;
#endif
    }

    /* 8 bytes LSN */
    {
        IDE_DASSERT( sBufferOffset == SMR_COMP_LOG_LSN_OFFSET );
        /* ���⼭�� 0���� �����ϰ� ���� �α� ��Ͻÿ�
         * �α� ������ Mutex�� ���� ���·� LSN�� ���� ����Ѵ�. */
        /* smrCompResPool�� 8Byte align �� �´� ���� : memcpy ���� �����Ѵ� */
        SM_LSN_INIT( ((smrLogHead *)aHeadDestPtr)->mLSN );
#ifdef DEBUG
        sBufferOffset += SMR_COMP_LOG_LSN_SIZE;
#endif
    }

    /* 2 bytes Magic Number */
    {
        IDE_DASSERT( sBufferOffset == SMR_COMP_LOG_MAGIC_OFFSET );
        /* ���⼭�� 0���� �����ϰ� ���� �α� ��Ͻÿ�
         * �α� ������ Mutex�� ���� ���·�
         * �αװ� ��ϵ� LSN�� ���� Magic�� ����ϰ� ����Ѵ�. */
        /* smrCompResPool�� 8Byte align �� �´� ���� : memcpy ���� �����Ѵ� */
         smrLogHeadI::setMagic( (smrLogHead *)aHeadDestPtr, 0 );
#ifdef DEBUG
        sBufferOffset += SMR_COMP_LOG_MAGIC_SIZE;
#endif
    }

    /* 4 bytes  �����α�ũ�� */
    {
        IDE_DASSERT( ID_SIZEOF( aRawLogSize ) == SMR_COMP_LOG_DECOMP_SIZE );

        /* 4Byte align �ȸ���.memcpy �ʿ�  */
        idlOS::memcpy( aHeadDestPtr + SMR_COMP_LOG_DECOMP_OFFSET,
                       & aRawLogSize,
                       SMR_COMP_LOG_DECOMP_SIZE );
#ifdef DEBUG
        sBufferOffset += SMR_COMP_LOG_DECOMP_SIZE;
#endif
    }

    /* 8 bytes  TableOID */
    {
        idlOS::memcpy( aHeadDestPtr + SMR_COMP_LOG_TABLEOID_OFFSET,
                       &aTableOID,
                       SMR_COMP_LOG_TABLEOID_SIZE );
#ifdef DEBUG
        sBufferOffset += SMR_COMP_LOG_TABLEOID_SIZE;
#endif
    }

   IDE_DASSERT( sBufferOffset == SMR_COMP_LOG_HEAD_SIZE );
}


/*
   ����α��� Tail�� ����Ѵ�.

   ����α��� Tail�� ������ ������ ����

       [ 4 bytes ] ���� �α��� ũ��

   [IN] aTailDestPtr      - ����α��� Tail�� ��ϵ� ��ġ
   [IN] aRawLogSize       - ���� �α��� ũ��
 */
void smrLogComp::writeCompTail( SChar * aTailDestPtr,
                                  UInt    aRawLogSize )
{
    IDE_DASSERT( aTailDestPtr  != NULL );
    IDE_DASSERT( aRawLogSize  > 0 );

    idlOS::memcpy( aTailDestPtr,
                   & aRawLogSize,
                   ID_SIZEOF( aRawLogSize ) );

    IDE_ASSERT( ID_SIZEOF( aRawLogSize ) == SMR_COMP_LOG_TAIL_SIZE );
}

/*
   ���� ����� ����� �α׸� ���� �޸� ���� �غ�

   [IN] aCompBufferHandle - ���� ������ �ڵ�
   [IN] aRawLogSize       - ����Ǳ� ���� ���� �α��� ũ��
   [OUT] aCompBuffer      - �غ�� ���� ���� �޸�
   [OUT] aCompBufferSize  - �غ�� ���� ������ ũ��
 */
IDE_RC smrLogComp::prepareCompBuffer( iduMemoryHandle    * aCompBufferHandle,
                                      UInt                 aRawLogSize,
                                      SChar             ** aCompBuffer,
                                      UInt               * aCompBufferSize)
{
    IDE_DASSERT( aCompBufferHandle != NULL );
    IDE_DASSERT( aRawLogSize > 0 );
    IDE_DASSERT( aCompBuffer != NULL );
    IDE_DASSERT( aCompBufferSize != NULL );

    UInt sMaxCompLogSize =
             SMR_COMP_LOG_OVERHEAD + IDU_LZ4_COMPRESSBOUND(aRawLogSize);

    IDE_TEST( aCompBufferHandle->prepareMemory( sMaxCompLogSize,
                                                (void**)aCompBuffer )
              != IDE_SUCCESS );

    * aCompBufferSize = sMaxCompLogSize;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/*
   ���������� ���� �޸� ���� �غ�

   [IN] aDecompBufferHandle - ���� ���� ������ �ڵ�
   [IN] aRawLogSize       - ����Ǳ� ���� ���� �α��� ũ��
   [OUT] aDecompBuffer      - �غ�� ���� ���� ���� �޸�
   [OUT] aDecompBufferSize  - �غ�� ���� ���� ������ ũ��
 */
IDE_RC smrLogComp::prepareDecompBuffer(
                      iduMemoryHandle    * aDecompBufferHandle,
                      UInt                 aRawLogSize,
                      SChar             ** aDecompBuffer,
                      UInt               * aDecompBufferSize )
{
    IDE_DASSERT( aDecompBufferHandle != NULL );
    IDE_DASSERT( aRawLogSize > 0 );
    IDE_DASSERT( aDecompBuffer != NULL );
    IDE_DASSERT( aDecompBufferSize != NULL );

    IDE_TEST( aDecompBufferHandle->prepareMemory( aRawLogSize,
                                                  (void**)aDecompBuffer )
              != IDE_SUCCESS );

    * aDecompBufferSize = aRawLogSize;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/* ����� �α��� ���������� �����Ѵ�

   [IN] aDecompBufferHandle - �������� ������ �ڵ�
   [IN] aCompLogOffset      - Compress�� �α��� Offset
   [IN] aValidLogMagic      - �α��� �������� Magic��
   [IN] aCompLog            - ����� �α�
   [OUT] aRawLog            - ���������� �α�
   [OUT] aMagicValue        - �α��� Magic��
   [OUT] aLogLSN             - �α��� �Ϸù�ȣ
   [OUT] aCompLogSize       - ����α��� ��üũ�� (Head+Body+Tail)
 */
IDE_RC smrLogComp::decompressLog( iduMemoryHandle    * aDecompBufferHandle,
                                  UInt                 aCompLogOffset,
                                  SChar              * aCompLog,
                                  smMagic              aValidLogMagic,
                                  SChar             ** aRawLog,
                                  smMagic            * aMagicValue,
                                  smLSN              * aLogLSN,
                                  UInt               * aCompLogSize )
{
    IDE_DASSERT( aDecompBufferHandle    != NULL );
    IDE_DASSERT( aCompLog               != NULL );
    IDE_DASSERT( aRawLog                != NULL );
    IDE_DASSERT( aCompLogSize           != NULL );

    SChar  * sDecompLogBuffer;
    UInt     sDecompLogBufferSize = 0;
    idBool   sIsValidLog;
    UInt     sRawLogSize;
    UInt     sCompressedRawLogSize;
    SInt     sDecompressedLogSize;

    // ����α� �м�
    //   1. ���� Head�� �ʵ� �б�
    //   2. ���� Tail�� ���� �Ϻθ� ��ϵ� ������ �α����� �Ǻ�
    IDE_TEST( analizeCompLog( aCompLog,
                              aCompLogOffset,
                              aValidLogMagic,
                              &sIsValidLog,
                              aMagicValue,
                              aLogLSN,
                              &sRawLogSize,
                              &sCompressedRawLogSize )
              != IDE_SUCCESS );

    if ( sIsValidLog == ID_TRUE )
    {
        // ���� ������ ���� ���� ���� �غ�
        IDE_TEST( prepareDecompBuffer( aDecompBufferHandle,
                                       sRawLogSize,
                                       & sDecompLogBuffer,
                                       & sDecompLogBufferSize )
                  != IDE_SUCCESS );
        // �������� �ǽ�
        sDecompressedLogSize = iduLZ4_decompress(
                       aCompLog + SMR_COMP_LOG_HEAD_SIZE, /* const char * source */
                       sDecompLogBuffer,                          /* char * dest */
                       sCompressedRawLogSize,              /* int compressedSize */
                       sDecompLogBufferSize );        /* int maxDecompressedSize */

        IDE_ERROR_RAISE( sDecompressedLogSize > 0, err_fail_log_decompress );
        // ���� �α��� ũ��� ���� ������ �α��� ũ�Ⱑ ���ƾ� ��
        IDE_ERROR_RAISE( sRawLogSize == (UInt)sDecompressedLogSize,
                          err_fail_log_decompress );

        *aRawLog      = sDecompLogBuffer;
        *aCompLogSize = SMR_COMP_LOG_HEAD_SIZE +
                        sCompressedRawLogSize +
                        SMR_COMP_LOG_TAIL_SIZE;
    }
    else
    {
        /* ���� ������ ���·� Invalid Log�� �Ϻη� ����� ���´� */
        /* �� �Լ��� ȣ���ڴ� ���� ������ �α��� Head�� Tail�� ����
           �α��� ��� ������ ������ ��ϵ� Valid�� �α�����
           �Ǻ��ؾ� �Ѵ� */
        IDE_TEST( createInvalidLog( aDecompBufferHandle,
                                    aRawLog )
                  != IDE_SUCCESS );

        // Invalid�� �α��̹Ƿ�, ���� ����α��� ũ��� ū �ǹ̰� ����.
        // ����α��� HEAD + TAIL�� ũ�⸦ ����
        *aCompLogSize = SMR_COMP_LOG_HEAD_SIZE + SMR_COMP_LOG_TAIL_SIZE ;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( err_fail_log_decompress )
    {
        ideLog::log( SM_TRC_LOG_LEVEL_MRECOV, 
                     "Log decompress failed - invalid log size: %"ID_UINT32_FMT"", 
                     sDecompressedLogSize );

        // BUG-26695 log decompress size ����ġ�� Recovery �����մϴ�.
        // Decompressed Log Size�� Log Head Size���� �� Ŭ ��쿡��
        // �߸��� Log Head ������ ����ֱ� ���� Decompressed��
        // Log �� Buffer Pointer�� �Ѱ��ݴϴ�.
        if( ID_SIZEOF(smrLogHead) <= (UInt)sDecompressedLogSize )
        {
            *aRawLog = sDecompLogBuffer;
        }
    }

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/* ����α׸� �ؼ��Ѵ�.

   [IN] aDeompBufferHandle     - ���� ���� ������ �ڵ�
   [IN] aCompLogOffset         - ���� �α� Offset
   [IN] aValidLogMagic         - �α��� �������� Magic��

   [OUT] aIsValid              - ����αװ� VALID���� ����
   [OUT] aFlag                 - ����α��� Flag
   [OUT] aMagicValue           - �α��� LSN���� ���� Magic Value
   [OUT] aLogLSN               - �α��� �Ϸù�ȣ
   [OUT] aRawLogSize           - ����Ǳ��� ���� �α��� ũ��
   [OUT] aCompressedRawLogSize - �����α׸� ������ �������� ũ��
*/
IDE_RC smrLogComp::analizeCompLog( SChar    * aCompLog,
                                   UInt       aCompLogOffset,
                                   smMagic    aValidLogMagic,
                                   idBool   * aIsValid,
                                   smMagic  * aMagicValue,
                                   smLSN    * aLogLSN,
                                   UInt     * aRawLogSize,
                                   UInt     * aCompressedRawLogSize )
{
    static UInt sLogFileSize    = smuProperty::getLogFileSize();
    UInt        sMaxLogSize;
    UInt        sCompLogTail;
    UInt        sLogFlag        = 0;

    IDE_ASSERT( aLogLSN     != NULL );
    IDE_ASSERT( aCompLog    != NULL );
    IDE_ASSERT( aIsValid    != NULL );
    IDE_ASSERT( aRawLogSize != NULL );
    IDE_ASSERT( aCompressedRawLogSize != NULL );

    *aIsValid    = ID_TRUE;
    // BUG-27331 CodeSonar::Uninitialized Variable
    SM_LSN_INIT( *aLogLSN );
    *aRawLogSize = 0;
    *aCompressedRawLogSize = 0;

    /* 4 byte Flag */
    {
        /* BUG-35392
         * ���� �α׿��� �÷��׸� �������� align�� ����ؾ� �Ѵ�.
         * memcpy�� �о�� �� */
        idlOS::memcpy( &sLogFlag, aCompLog, SMR_COMP_LOG_FLAG_SIZE );

        IDE_ASSERT( ( sLogFlag & SMR_LOG_COMPRESSED_MASK ) ==
                    SMR_LOG_COMPRESSED_OK );
        aCompLog += SMR_COMP_LOG_FLAG_SIZE;
    }

    /* 4 bytes ���� �� �α�ũ�� */
    {
        idlOS::memcpy( aCompressedRawLogSize, aCompLog, SMR_COMP_LOG_COMP_SIZE );
        aCompLog += SMR_COMP_LOG_COMP_SIZE;

        /* BUG-35392
         * head,tail ũ�⸦ �� ���� ����α� ũ�⸦ ���� */
        *aCompressedRawLogSize -= SMR_COMP_LOG_OVERHEAD;
    }

    /* 8 bytes Log�� LSN */
    {
        idlOS::memcpy( aLogLSN, aCompLog, SMR_COMP_LOG_LSN_SIZE );
        aCompLog += SMR_COMP_LOG_LSN_SIZE;
    }

    /* 2 bytes Log�� Magic Value */
    {
        idlOS::memcpy( aMagicValue, aCompLog, SMR_COMP_LOG_MAGIC_SIZE );
        aCompLog += SMR_COMP_LOG_MAGIC_SIZE;
    }

    /* Magic���� Valid���� ���� ��� Invalid�α׷� ����*/
    IDE_TEST_CONT( aValidLogMagic != *aMagicValue, skip_invalid_log );

    /* 4 bytes �����α�ũ�� */
    {
        idlOS::memcpy( aRawLogSize, aCompLog, SMR_COMP_LOG_DECOMP_SIZE );
        aCompLog += SMR_COMP_LOG_DECOMP_SIZE;
    }

    /* BUG-46944  CompLogHeader�� TableOID �߰� */
    aCompLog += SMR_COMP_LOG_TABLEOID_SIZE;

    IDE_ASSERT( sLogFileSize > aCompLogOffset );

    sMaxLogSize = sLogFileSize - aCompLogOffset;

    /* BUG-24162: [SC] LogSize�� �����ϰ� ū������ �Ǿ� �־� LogTail�� ã������
     *            �� ���� �̿��ϴٰ� invalid�� memory������ �����Ͽ� ���� ���.
     *
     * LogHeader�� LogSize�� Valid���� �˻��Ѵ�. */
    IDE_TEST_CONT( *aCompressedRawLogSize > sMaxLogSize, skip_invalid_log );
    
    /* 4 bytes [Tail] �����α�ũ�� */
    {
        idlOS::memcpy( &sCompLogTail,
                       aCompLog + *aCompressedRawLogSize,
                       SMR_COMP_LOG_TAIL_SIZE);
    }

    IDE_TEST_CONT( *aRawLogSize != sCompLogTail, skip_invalid_log );

    // Size�� 0�� ��� Invalid�α׷� ����
    //  ���� : smrLogComp.h�� ����� ���� �α� Head����
    //         Magic���� ����� �ǰ� ����/���� �α�ũ�� ���ķ�
    //         ���� ����� ���� ���� ���,
    //         ���� �α� ũ��, ����α��� Tail�� ��� 0�̰�
    //         Magic ���� �¾ƶ������� ������ Valid�� �α׷� �νĵ�.
    //         => Size�� 0�̸� Invalid Log�� ����
    IDE_TEST_CONT( *aRawLogSize == 0, skip_invalid_log );

    return IDE_SUCCESS;

    IDE_EXCEPTION_CONT( skip_invalid_log );

    *aIsValid = ID_FALSE;

    return IDE_SUCCESS;
}


/*
   ����� �αװ� Invalid�� ���
   ���� ���� ���ۿ� �Ϻη� �������� �� �������� Invalid�α� ��� */
typedef struct smrInvalidLog
{
    smrLogHead    mHead;
    smrLogTail    mTail;
    UChar         mLogRecFence; /* log record�� ũ�⸦ ���ϱ� ���� ���� */
} smrInvalidLog;

/* VALID���� ���� �α׸� ������� ���� ���·� �����Ѵ�.

   ����αװ� ������ ��ϵ��� ���ϰ� �Ϻθ� ��ϵ� ���,
   ���� ���� ���ۻ� INVALID�� �α׸� �Ϻη� ����Ѵ�.

   => Compression Transparency�� �����ϱ� ���� �̿� ����
      ������� ���� ������ INVALID�� �α׸� ����صд�.
      ���� �α׸� �д� ��⿡�� ������� ���� �α���
      VALID���θ� �Ǵ��ϴ� ��İ� ������ �������
      �ش� �αװ� VALID���� �˻��Ѵ�.

   [IN] aDeompBufferHandle - ���� ���� ������ �ڵ�
   [OUT] aInvalidRawLog    - INVALID �α��� �ּ�
 */
IDE_RC smrLogComp::createInvalidLog( iduMemoryHandle    * aDecompBufferHandle,
                                     SChar             ** aInvalidRawLog )
{
    IDE_DASSERT( aDecompBufferHandle != NULL );
    IDE_DASSERT( aInvalidRawLog != NULL );

    smrInvalidLog sInvalidLog;

    void * sLogBuffer;

    // Invalid Log�� ����� �޸� ���� �غ�
    IDE_TEST( aDecompBufferHandle->prepareMemory( ID_SIZEOF( sInvalidLog ),
                                                  & sLogBuffer )
              != IDE_SUCCESS );

    // sInvalidLog�� ������ Invalid�� �α׷� ����
    {
        idlOS::memset( & sInvalidLog, 0, ID_SIZEOF( sInvalidLog ) );

        // Invalid�ϰ� �νĵǵ��� Head�� Type(1)�� Tail(0)�� �ٸ��� ���
        smrLogHeadI::setType( &sInvalidLog.mHead, 1 );
        sInvalidLog.mTail = 0;

        smrLogHeadI::setTransID( &sInvalidLog.mHead, SM_NULL_TID );
        smrLogHeadI::setSize( &sInvalidLog.mHead,
                              SMR_LOGREC_SIZE(smrInvalidLog) );

        smrLogHeadI::setFlag( &sInvalidLog.mHead,
                              SMR_LOG_TYPE_NORMAL );

        smrLogHeadI::setPrevLSN( &sInvalidLog.mHead,
                                 ID_UINT_MAX,  // FILENO
                                 ID_UINT_MAX ); // OFFSET

        smrLogHeadI::setReplStmtDepth( &sInvalidLog.mHead,
                                       SMI_STATEMENT_DEPTH_NULL );
    }

    idlOS::memcpy( sLogBuffer, & sInvalidLog, ID_SIZEOF( sInvalidLog ) );

    *aInvalidRawLog = (SChar *) sLogBuffer;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/* ���� ��� �α����� �Ǻ��Ѵ�

   [IN] aRawLog     - ������ �α������� �Ǻ��� �α� ���ڵ�
   [IN] aRawLogSize - �α� ���ڵ��� ũ�� ( Head + Body + Tail )
   [OUT] aDoCompLog - �α� ���� ����
 */
IDE_RC smrLogComp::shouldLogBeCompressed( SChar  * aRawLog,
                                          UInt     aRawLogSize,
                                          idBool * aDoCompLog )
{
    IDE_DASSERT( aRawLog != NULL );
    IDE_DASSERT( aRawLogSize > 0 );
    IDE_DASSERT( aDoCompLog != NULL );


    scSpaceID    sSpaceID;
    smrLogHead * sLogHead;
    idBool       sDoComp; /* Log compress ���� */


    sLogHead = (smrLogHead*)aRawLog;

    if ( smuProperty::getMinLogRecordSizeForCompress() == 0 )
    {
        // Log Compression�� Disable�� ����
        // Log Compression�� �������� �ʴ´�.
        sDoComp = ID_FALSE;
    }
    else
    {
        if ( aRawLogSize >= smuProperty::getMinLogRecordSizeForCompress() )
        {
            // Tablespace�� �α� ���࿩�� �Ǵ� �ʿ�
            getSpaceIDOfLog( sLogHead, aRawLog, & sSpaceID );

            if ( sSpaceID == SC_NULL_SPACEID )
            {
                // Ư�� Tablespace���� �αװ� �ƴ� ���
                sDoComp = ID_TRUE; // �α� ���� �ǽ�
            }
            else
            {
                IDE_TEST( sctTableSpaceMgr::getSpaceLogCompFlag( sSpaceID,
                                                                 &sDoComp )
                          != IDE_SUCCESS );
            }
        }
        else
        {
            sDoComp = ID_FALSE;
        }

        switch( smrLogHeadI::getType( sLogHead ) )
        {
            // �α������� �� �տ� ��ϵǴ�
            //  File Begin Log�� ��� �������� �ʴ´�.
            // ���� :
            //     File�� ù��° Log�� LSN�� �д� �۾���
            //     ������ �����ϱ� ����
            case SMR_LT_FILE_BEGIN :
            // �α������� Offset���� üũ�� ���� �� �� �ֵ��� �ϱ� ����
            //    => Offset < (�α�����ũ�� - smrLogHead - smrLogTail)
            case SMR_LT_FILE_END :

            // ������ �ܼ�ȭ�� ���� �������� �ʴ� �α׵�
            case SMR_LT_CHKPT_BEGIN :
            case SMR_LT_CHKPT_END :

                sDoComp = ID_FALSE;
                break;

            default:
                break;

        }
    }

    // �α� ������ ���� �ʵ��� Log Head�� ������ ���
    // �α׸� �������� �ʴ´�.
    if ( ( smrLogHeadI::getFlag(sLogHead) & SMR_LOG_FORBID_COMPRESS_MASK )
         == SMR_LOG_FORBID_COMPRESS_OK )
    {
        sDoComp = ID_FALSE;
    }

    *aDoCompLog = sDoComp;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*
    Log Record�� ��ϵ� Tablespace ID�� �����Ѵ�.

    [IN] aLogHead - Log�� Head
    [IN] aRawLog  - Log�� �ּ�
    [OUT] aSpaceID - Tablespace�� ID
 */
void smrLogComp::getSpaceIDOfLog( smrLogHead * aLogHead,
                                  SChar      * aRawLog,
                                  scSpaceID  * aSpaceID )
{
    IDE_DASSERT( aLogHead != NULL );
    IDE_DASSERT( aRawLog != NULL );
    IDE_DASSERT( aSpaceID != NULL );

    scSpaceID sSpaceID;

    switch( smrLogHeadI::getType( aLogHead ) )
    {
        case SMR_LT_UPDATE :
            sSpaceID = SC_MAKE_SPACE(((smrUpdateLog*)aRawLog)->mGRID);
            break;

        case SMR_LT_COMPENSATION :
            sSpaceID = SC_MAKE_SPACE(((smrCMPSLog*)aRawLog)->mGRID);
            break;

        case SMR_LT_TBS_UPDATE :
            sSpaceID = ((smrTBSUptLog*)aRawLog)->mSpaceID;
            break;

        case SMR_LT_NTA :
            sSpaceID = ((smrNTALog*)aRawLog)->mSpaceID;
            break;

        case SMR_DLT_NTA :
        case SMR_DLT_REF_NTA :
        case SMR_DLT_REDOONLY :
        case SMR_DLT_UNDOABLE :
        case SMR_DLT_COMPENSATION :
            /*
               Disk Log�� ��� �� �� �̻��� Tablespace�� ����
               �αװ� ��ϵ� �� �ִ�. (Ex> LOB Column )

               Space ID�� NULL�� �ѱ��.
            */
        default :
            /* �⺻������ NULL SPACE ID�� �ѱ��
             */
            sSpaceID = SC_NULL_SPACEID;
            break;

    }

    *aSpaceID = sSpaceID ;
}


/* �α������� Ư�� Offset���� �α� ���ڵ带 �о�´�.
   ����� �α׶�� ����� ���� �״�� ��ȯ�Ѵ�.

   [IN] aLogFile            - �α׸� �о�� �α�����
   [IN] aLogOffset          - �α׸� �о�� ������
   [OUT] aRawLogHead        - �α��� Head
   [OUT] aRawLogPtr         - �о �α� (������������ ���� �α�)
   [OUT] aLogSizeAtDisk     - ���Ͽ��� �о �α� �������� ��
*/
IDE_RC smrLogComp::readLog4RP( smrLogFile         * aLogFile,
                               UInt                 aLogOffset,
                               smrLogHead         * aRawLogHead,
                               SChar             ** aRawLogPtr,
                               UInt               * aLogSizeAtDisk )
{
    IDE_DASSERT( aLogFile       != NULL );
    IDE_DASSERT( aRawLogHead    != NULL );
    IDE_DASSERT( aRawLogPtr     != NULL );
    IDE_DASSERT( aLogOffset < smuProperty::getLogFileSize() );
    IDE_DASSERT( aLogSizeAtDisk != NULL );

    smrLogHead  sInvalidLogHead;
    SChar     * sRawOrCompLog;

    aLogFile->read(aLogOffset, &sRawOrCompLog);

    /* valid log �˻� ���� �ʴ´�. ���� ��⿡�� ���� ó���Ѵ�. */
    *aRawLogPtr = sRawOrCompLog;

    // ������ Ǯ�� ���� ���̱� ������ ����α��� ��� Header ���ʿ� ������ ���� �� ������ �ִ�.
    // Magic Number ������ ��ȿ�ϴ�.
    /* Log Header�� �����Ѵ�.*/
    idlOS::memcpy(aRawLogHead, sRawOrCompLog, ID_SIZEOF(smrLogHead));

    // ������ Ǯ�� ���� ���̱� ������ ���� disk ���� Size�� ��ȯ�ϸ� �ȴ�
    // ���� �α׿� ����� �α� ��� Disk���� Size�� ���� ��ġ�� �����ϱ� ������
    // �׳� ��ȯ�ϸ� �ȴ�.
    *aLogSizeAtDisk = smrLogHeadI::getSize( aRawLogHead );
    IDE_TEST_RAISE( *aLogSizeAtDisk > smuProperty::getLogFileSize(),
                    err_invalid_log );

    return IDE_SUCCESS;

    IDE_EXCEPTION( err_invalid_log );
    {
        idlOS::memcpy( &sInvalidLogHead,
                       sRawOrCompLog,
                       ID_SIZEOF(smrLogHead) );

        ideLog::log( SM_TRC_LOG_LEVEL_MRECOV,
                     SM_TRC_MRECOVER_INVALID_DECOMP_LOG_HEAD,
                     sInvalidLogHead.mFlag,
                     sInvalidLogHead.mType,
                     sInvalidLogHead.mMagic,
                     sInvalidLogHead.mSize,
                     sInvalidLogHead.mPrevUndoLSN.mFileNo,
                     sInvalidLogHead.mPrevUndoLSN.mOffset,
                     sInvalidLogHead.mTransID,
                     sInvalidLogHead.mReplSvPNumber );
    }
    IDE_EXCEPTION_END;

    // BUG-26695 log decompress size ����ġ�� Recovery �����մϴ�.
    IDE_PUSH();
    IDE_SET( ideSetErrorCode( smERR_ABORT_INVALID_LOGFILE,
                              aLogFile->getFileName() ) );
    IDE_POP();

    return IDE_FAILURE;
}

IDE_RC smrLogComp::decompressCompLog( iduMemoryHandle    * aDecompBufferHandle,
                                     UInt                 aCompLogOffset,
                                     SChar              * aCompLog,
                                     smMagic              aValidLogMagic,
                                     smrLogHead         * aRawLogHead,
                                     SChar             ** aRawLog,
                                     UInt               * aLogSizeAtDisk )
{
    UInt         sCompLogSize = 0;
    SChar      * sDecompLog   = NULL;
    smMagic      sMagicValue;
    smLSN        sLogLSN;
    smrLogHead   sInvalidLogHead;

    if( smrLogHeadI::isDummyLog( aCompLog ) == ID_FALSE )
    {

        /* ����� �α׸� �б� ���ؼ���
         * ����α� ���� �ڵ��� ���ڷ� �Ѱܾ� �� */
        IDE_ASSERT( aDecompBufferHandle != NULL );

        /* ����� �α��̴�. ���� ������ ����. */
        IDE_TEST_RAISE( decompressLog( aDecompBufferHandle,
                                       aCompLogOffset,
                                       aCompLog,
                                       aValidLogMagic,
                                       &sDecompLog,
                                       &sMagicValue,
                                       &sLogLSN,
                                       &sCompLogSize )
                        != IDE_SUCCESS, err_fail_log_decompress );

        *aRawLog = sDecompLog;

        /* Log Header�� �����Ѵ�.
           Log�� ��ϵɶ� Log�� ũ�Ⱑ align���� �ʾұ�
           ������ �����ؼ� �����Ѵ�.*/
        idlOS::memcpy(aRawLogHead, sDecompLog, ID_SIZEOF(smrLogHead));

        /* ���� ������ Log�� Head�� LSN�� Magic�� ����
         * - ���� : ����� compHead�� �ۼ��Ҷ� LSN�� magicNo�� 0���� ������
         * smrLogComp.h�� <����α��� �ǵ� ����> ���� */
        smrLogHeadI::setMagic( aRawLogHead, sMagicValue );
        smrLogHeadI::setLSN( aRawLogHead, sLogLSN );

        /* ����� Log�� Head�� ���������� �α׿� ���� */
        idlOS::memcpy( sDecompLog, aRawLogHead, ID_SIZEOF(smrLogHead) );

        *aLogSizeAtDisk = sCompLogSize;
        IDE_TEST_RAISE( *aLogSizeAtDisk > smuProperty::getLogFileSize(),
                        err_invalid_log );
    }
    else
    {
        /* Dummy log�̸� size�� return */
        *aRawLog = aCompLog;

        /* Log Header�� �����Ѵ�.*/
        idlOS::memcpy(aRawLogHead, aCompLog, ID_SIZEOF(smrLogHead));

        idlOS::memcpy( (void*)&sLogLSN,
                       aCompLog + SMR_COMP_LOG_LSN_OFFSET,
                       SMR_COMP_LOG_LSN_SIZE );

        smrLogHeadI::setLSN(aRawLogHead, sLogLSN );

        *aLogSizeAtDisk = getCompressedLogSize( aCompLog );
        IDE_TEST_RAISE( *aLogSizeAtDisk > smuProperty::getLogFileSize(),
                        err_invalid_log );

    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( err_fail_log_decompress );
    {
        // BUG-26695 log decompress size ����ġ�� Recovery �����մϴ�.
        ideLog::log( SM_TRC_LOG_LEVEL_MRECOV,
                     SM_TRC_MRECOVER_INVALID_DECOMP_LOG_LSN,
                     sLogLSN.mFileNo,
                     sLogLSN.mOffset );

        if( sDecompLog != NULL )
        {
            // Decompressed Log Size�� Log Head Size���� �� Ŭ ��쿡��
            // ��� �߸��Ǿ����� �˱����� �߸��� Log Head ������ ���
            idlOS::memcpy( &sInvalidLogHead,
                           sDecompLog,
                           ID_SIZEOF(smrLogHead));

            ideLog::log( SM_TRC_LOG_LEVEL_MRECOV,
                         SM_TRC_MRECOVER_INVALID_DECOMP_LOG_HEAD,
                         sInvalidLogHead.mFlag,
                         sInvalidLogHead.mType,
                         sInvalidLogHead.mMagic,
                         sInvalidLogHead.mSize,
                         sInvalidLogHead.mPrevUndoLSN.mFileNo,
                         sInvalidLogHead.mPrevUndoLSN.mOffset,
                         sInvalidLogHead.mTransID,
                         sInvalidLogHead.mReplSvPNumber );
        }
    }

    IDE_EXCEPTION( err_invalid_log );
    {
        idlOS::memcpy( &sInvalidLogHead,
                       aCompLog,
                       ID_SIZEOF(smrLogHead) );

        ideLog::log( SM_TRC_LOG_LEVEL_MRECOV,
                     SM_TRC_MRECOVER_INVALID_DECOMP_LOG_HEAD,
                     sInvalidLogHead.mFlag,
                     sInvalidLogHead.mType,
                     sInvalidLogHead.mMagic,
                     sInvalidLogHead.mSize,
                     sInvalidLogHead.mPrevUndoLSN.mFileNo,
                     sInvalidLogHead.mPrevUndoLSN.mOffset,
                     sInvalidLogHead.mTransID,
                     sInvalidLogHead.mReplSvPNumber );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}
