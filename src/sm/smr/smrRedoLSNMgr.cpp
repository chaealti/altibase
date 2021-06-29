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
 * $Id: smrRedoLSNMgr.cpp 82936 2018-04-30 04:22:11Z emlee $
 **********************************************************************/

#include <smErrorCode.h>
#include <smDef.h>
#include <smr.h>
#include <smrDef.h>

smrRedoInfo    smrRedoLSNMgr::mRedoInfo;
smrRedoInfo*   smrRedoLSNMgr::mCurRedoInfoPtr;
smLSN          smrRedoLSNMgr::mLstLSN;

smrRedoLSNMgr::smrRedoLSNMgr()
{
}

smrRedoLSNMgr::~smrRedoLSNMgr()
{
}

/***********************************************************************
 * Description : smrRedoLSNMgr�� �ʱ�ȭ�Ѵ�.

  smrRedoInfo�� �Ҵ��ϰ� ������ smrRedoInfo��
  mRedoLSN�� ����Ű�� Log�� Head�� mSN���� ���� Sorting�ϱ� ����
  mRedoLSN�� ����Ű�� Log�� �о mSortRedoInfo�� �����Ѵ�.

  aRedoLSN : [IN] Redo LSN 
*/
IDE_RC smrRedoLSNMgr::initialize( smLSN *aRedoLSN )
{

    mCurRedoInfoPtr = NULL;

    IDE_TEST( initializeRedoInfo( &mRedoInfo )
              != IDE_SUCCESS );

    IDE_TEST( pushRedoInfo( &mRedoInfo,
                            aRedoLSN )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/* Redo Info�� �ʱ�ȭ�Ѵ�

   [IN] aRedoInfo - �ʱ�ȭ�� Redo Info
 */
IDE_RC smrRedoLSNMgr::initializeRedoInfo( smrRedoInfo * aRedoInfo )
{
    IDE_DASSERT( aRedoInfo != NULL );

    idlOS::memset( aRedoInfo, 0, ID_SIZEOF( *aRedoInfo ) );

    // Log Record�� ���� ������ ��,
    // �ٷ� Redo���� �ʰ� Hash Table�� �Ŵ޸�ä�� �����ְ� �ȴ�.
    // �̸� ���� Log�� Decompress Buffer�� �������� �ʵ���,
    // iduGrowingMemoryHandle�� ����Ѵ�.

    /* smrRedoLSNMgr_initializeRedoInfo_malloc_DecompBufferHandle1.tc */
    IDU_FIT_POINT("smrRedoLSNMgr::initializeRedoInfo::malloc::DecompBufferHandle1");
    IDE_TEST( iduMemMgr::malloc( IDU_MEM_SM_SMR,
                                 ID_SIZEOF( iduGrowingMemoryHandle ),
                                 (void**)&aRedoInfo->mDecompBufferHandle,
                                 IDU_MEM_FORCE )
              != IDE_SUCCESS );

    // �α� ������� �ڵ��� �ʱ�ȭ
    IDE_TEST( ((iduGrowingMemoryHandle*)aRedoInfo->mDecompBufferHandle)->
                    initialize(
                        IDU_MEM_SM_SMR,
                        // Chunk ũ��
                        // ( �ִ� �Ҵ簡���� �������ũ��
                        //   => Log Record�� �ִ� ũ�� == �α�����ũ��)
                        smuProperty::getLogFileSize() )
              != IDE_SUCCESS );

    aRedoInfo->mLogFilePtr = NULL;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/* Redo Info�� �ı��Ѵ�

   [IN] aRedoInfo - �ı��� Redo Info
 */
IDE_RC smrRedoLSNMgr::destroyRedoInfo( smrRedoInfo * aRedoInfo )
{
    IDE_DASSERT( aRedoInfo != NULL );

    // �α� ������� �ڵ��� �ı�
    IDE_TEST( ((iduGrowingMemoryHandle*)aRedoInfo->mDecompBufferHandle)->
                destroy() != IDE_SUCCESS );

    IDE_TEST( iduMemMgr::free( aRedoInfo->mDecompBufferHandle )
              != IDE_SUCCESS );
    aRedoInfo->mDecompBufferHandle = NULL;

    // BUGBUG Log File�� Close�� ��� �ϴ���?

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/* Decompress Log Buffer ũ�⸦ ���´�

   return - Decompress Log Bufferũ��
 */

ULong smrRedoLSNMgr::getDecompBufferSize()
{
    return ((iduGrowingMemoryHandle*)
            mRedoInfo.mDecompBufferHandle)->getSize();
}

// Decompress Log Buffer�� �Ҵ��� ��� �޸𸮸� �����Ѵ�.
IDE_RC smrRedoLSNMgr::clearDecompBuffer()
{

    IDE_TEST( ((iduGrowingMemoryHandle*)mRedoInfo.mDecompBufferHandle)->clear()
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/* Redo Info�� Sort Array�� Push�Ѵ�.

   [IN] aRedoInfo -  Sort Array�� Push�� Redo Info
   [IN] aRedoLSN  -  Redo ���� ��ġ�� ����Ű�� LSN

 */
IDE_RC smrRedoLSNMgr::pushRedoInfo( smrRedoInfo * aRedoInfo,
                                    smLSN       * aRedoLSN )
{
    iduMemoryHandle * sDecompBufferHandle;
    smrLogHead      * sLogHeadPtr;
    ULong             sOrgDecompBufferSize;

    IDE_DASSERT( aRedoInfo  != NULL);
    IDE_DASSERT( aRedoLSN   != NULL );

    aRedoInfo->mRedoLSN  = *aRedoLSN;
    sDecompBufferHandle  = aRedoInfo->mDecompBufferHandle;
    sOrgDecompBufferSize = sDecompBufferHandle->getSize();

    /*mRedoLSN�� ����Ű�� Log�� �о mRedoInfo�� �����Ѵ�.*/
    IDE_TEST(smrLogMgr::readLog( aRedoInfo->mDecompBufferHandle,
                                 &(aRedoInfo->mRedoLSN),
                                 ID_FALSE, /* don't Close Log File When aLogFile doesn't include aLSN */
                                 &(aRedoInfo->mLogFilePtr),
                                 &(aRedoInfo->mLogHead),
                                 &(aRedoInfo->mLogPtr),
                                 &(aRedoInfo->mIsValid),
                                 &(aRedoInfo->mLogSizeAtDisk))
             != IDE_SUCCESS);

    if(aRedoInfo->mIsValid == ID_TRUE)
    {
        sLogHeadPtr = &aRedoInfo->mLogHead;

        // ��ũ �α׸� ���� ���
        if ( smrLogMgr::isDiskLogType( smrLogHeadI::getType( sLogHeadPtr ) )
             == ID_TRUE )
        {
            IDE_TEST( makeCopyOfDiskLogIfNonComp(
                          aRedoInfo,
                          sDecompBufferHandle,
                          sOrgDecompBufferSize ) != IDE_SUCCESS );
        }
    }
    else
    {
        /* Active�� Transaction�� �ϳ��� ���� ��� */
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}



/***********************************************************************
 * Description : smrRedoLSNMgr�� �����Ѵ�.
 *
 * �Ҵ�� Resource�� �����Ѵ�.
 */

IDE_RC smrRedoLSNMgr::destroy()
{
    IDE_TEST( smrLogMgr::getLogFileMgr().closeAllLogFile() != IDE_SUCCESS );

    IDE_TEST( destroyRedoInfo( &mRedoInfo )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : iduPriorityQueue���� Item���� Compare�� �� ����ϴ�
 *               Callback Function��. 
 *               iduPriorityQueue�� initialize�Ҷ� �Ѱ���
 *
 * arg1  - [IN] compare�� smrRedoInfo 1
 * arg2  - [IN] compare�� smrRedoInfo 2
*/
SInt smrRedoLSNMgr::compare( const void *arg1,const void *arg2 )
{
    smLSN sLSN1;
    smLSN sLSN2;

    SM_GET_LSN( sLSN1, ((*(smrRedoInfo**)arg1))->mRedoLSN );
    SM_GET_LSN( sLSN2, ((*(smrRedoInfo**)arg2))->mRedoLSN );

    if ( smrCompareLSN::isGT(&sLSN1, &sLSN2 ) )
    {
        return 1;
    }
    else
    {
        if ( smrCompareLSN::isLT(&sLSN1, &sLSN2 ) )
        {
            return -1;
        }
        else
        {
            return 0;
        }
    }
}

/***********************************************************************
 * Description : Redo�� Log�� �о �ش�.
 *
 * mSortRedoInfo�� ����ִ� smrRedoInfo�߿��� ���� ���� mSN����
 * ���� Log�� �о���δ�.
 *
 * [OUT] aLSN       - Log�� LSN
 * [OUT] aLogHead   - aLSN�� ����Ű�� log�� LogHead
 * [OUT] aLogPtr    - aLSN�� ����Ű�� log�� Log Buffr Ptr
 * [OUT] aLogSizeAtDisk - disk �󿡼��� log�� ����
 * [OUT] aIsValid   - aLSN�� ����Ű�� log��  Valid�ϸ� ID_TRUE�ƴϸ� ID_FALSE
 ***********************************************************************/
IDE_RC smrRedoLSNMgr::readLog( smLSN      ** aLSN,
                               smrLogHead ** aLogHead,
                               SChar      ** aLogPtr,
                               UInt        * aLogSizeAtDisk,
                               idBool      * aIsValid )
{
    smrLogFile      * sOrgLogFile ;
    iduMemoryHandle * sDecompBufferHandle;
    ULong             sOrgDecompBufferSize;
    smrRedoInfo     * sRedoInfoPtr      = NULL;
    smrLogHead      * sLogHeadPtr;
    idBool            sUnderflow        = ID_FALSE;
#ifdef DEBUG
    smLSN             sDebugLSN;
#endif  

    IDE_DASSERT( aLSN           != NULL );
    IDE_DASSERT( aLogHead       != NULL );
    IDE_DASSERT( aLogPtr        != NULL );
    IDE_DASSERT( aLogSizeAtDisk != NULL );
    IDE_DASSERT( aIsValid       != NULL );

    *aIsValid = ID_TRUE;

    if ( mCurRedoInfoPtr != NULL )
    {
        if ( mCurRedoInfoPtr->mIsValid == ID_TRUE )
        {
            /* BUG-35392
             * Dummy Log �� �ǳʶٸ鼭 ���� �α׸� ������ ���� ���� �ݺ� */
            while(1)
            {
                sDecompBufferHandle = mCurRedoInfoPtr->mDecompBufferHandle;

                sOrgLogFile = mCurRedoInfoPtr->mLogFilePtr;
                sOrgDecompBufferSize = sDecompBufferHandle->getSize();


                /* ���������� Redo������ �� smrRedoInfo�� mRedoLSN��
                 * smrRecoveryMgr�� Redo���� update�� �ȴ�. ���� �ٽ�
                 * mRedoLSN�� ����Ű�� �α׸� �о smrRedoInfo�� �����ϰ�
                 * �ٽ� mSortRedoInfo�� �־�� �Ѵ�.*/
                IDE_TEST( smrLogMgr::readLog( mCurRedoInfoPtr->mDecompBufferHandle,
                                              &(mCurRedoInfoPtr->mRedoLSN),
                                              ID_FALSE, /* don't Close Log File When aLogFile doesn't include aLSN */
                                              &(mCurRedoInfoPtr->mLogFilePtr),
                                              &(mCurRedoInfoPtr->mLogHead),
                                              &(mCurRedoInfoPtr->mLogPtr),
                                              &(mCurRedoInfoPtr->mIsValid),
                                              &(mCurRedoInfoPtr->mLogSizeAtDisk) )
                          != IDE_SUCCESS);

                if ( mCurRedoInfoPtr->mIsValid == ID_TRUE )
                {

                    // ���ο� �α������� ���� ���
                    if ( sOrgLogFile != mCurRedoInfoPtr->mLogFilePtr )
                    {
                        // ���� �α������� Close�Ѵ�.
                        // ���� : makeCopyOfDiskLog�� �ּ� ����
                        IDE_TEST( smrLogMgr::closeLogFile( sOrgLogFile )
                                  != IDE_SUCCESS );
                    }

                    sLogHeadPtr = &mCurRedoInfoPtr->mLogHead;

                    /* BUG-35392 */
                    if ( smrLogHeadI::isDummyLog( sLogHeadPtr ) == ID_TRUE )
                    {
                        mCurRedoInfoPtr->mRedoLSN.mOffset += mCurRedoInfoPtr->mLogSizeAtDisk;
                        continue;
                    }

                    // ��ũ �α׸� ���� ���
                    if ( smrLogMgr::isDiskLogType( smrLogHeadI::getType( sLogHeadPtr ))
                         == ID_TRUE )
                    {
                        IDE_TEST( makeCopyOfDiskLogIfNonComp( 
                                                    mCurRedoInfoPtr,
                                                    sDecompBufferHandle,
                                                    sOrgDecompBufferSize )
                                  != IDE_SUCCESS );
                    }
                }
                else
                {
                    /* nothing to do */
                }
                break;
            } // end of while
        }
    }

    sRedoInfoPtr = &mRedoInfo;

    if ( sUnderflow == ID_FALSE )
    {
        mCurRedoInfoPtr = sRedoInfoPtr;
    }

    // fix BUG-26934 : [codeSonar] Null Pointer Dereference
    if ( mCurRedoInfoPtr != NULL )
    {
        if ( mCurRedoInfoPtr->mIsValid == ID_TRUE )
        {
#ifdef DEBUG
            sDebugLSN = smrLogHeadI::getLSN( &(mCurRedoInfoPtr->mLogHead) ); 
            IDE_ASSERT( smrCompareLSN::isEQ( &(mCurRedoInfoPtr->mRedoLSN), &sDebugLSN ) );
#endif
            SM_GET_LSN( mLstLSN, mCurRedoInfoPtr->mRedoLSN );
            *aLSN     = &(mCurRedoInfoPtr->mRedoLSN);
            *aLogHead = &(mCurRedoInfoPtr->mLogHead);
            *aLogPtr  = mCurRedoInfoPtr->mLogPtr;
            *aLogSizeAtDisk = mCurRedoInfoPtr->mLogSizeAtDisk;
        }
        else
        {
            *aLSN     = &(mCurRedoInfoPtr->mRedoLSN);
            *aIsValid = ID_FALSE;
        }
    }
    else
    {
        *aIsValid = ID_FALSE;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*
 * Description : ������۸� ������� ���� Disk Log memcpy
 *
 * To Fix BUG-21078
 * ������� ���� DiskLog�� ������ Memory ������ memcpy �Ѵ�.
 */
IDE_RC smrRedoLSNMgr::makeCopyOfDiskLogIfNonComp(
                          smrRedoInfo     * aCurRedoInfoPtr,
                          iduMemoryHandle * aDecompBufferHandle,
                          ULong             aOrgDecompBufferSize )
{
    smrLogHead      * sLogHeadPtr;

    sLogHeadPtr = &aCurRedoInfoPtr->mLogHead;

    // ��ũ �α׸� ���� ���
    if ( smrLogMgr::isDiskLogType( smrLogHeadI::getType( sLogHeadPtr ))
         == ID_TRUE )
    {
        // ������۸� ������� ���� ���
        if ( aOrgDecompBufferSize ==
             aDecompBufferHandle->getSize() )
        {
            // ������� ���� �α��̴�.
            // �����α�ũ��� Disk���� Logũ�Ⱑ ���ƾ� ��
            IDE_DASSERT( aCurRedoInfoPtr->mLogSizeAtDisk ==
                         smrLogHeadI::getSize(sLogHeadPtr) );

            // To Fix BUG-18686
            //   Disk Log�� Redo�� Decompression Buffer�� ��������
            //   Hash �� Log Record���� Apply
            IDE_TEST( makeCopyOfDiskLog(
                                  aDecompBufferHandle,
                                  aCurRedoInfoPtr->mLogPtr,
                                  smrLogHeadI::getSize(sLogHeadPtr),
                                  & aCurRedoInfoPtr->mLogPtr )
                      != IDE_SUCCESS );

            // �α� Head�� (aCurRedoInfoPtr->mLogHead)
            // �̹� �޸� ����� �����̴�.
            // ���� ó�� ���� �ʾƵ� �ȴ�.
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

// �޸� �ڵ�κ��� �Ҵ��� �޸𸮿� �α׸� �����Ѵ�.
//
// To Fix BUG-18686
//   Disk Log�� Redo�� Decompression Buffer�� ��������
//   Hash �� Log Record���� Apply
//
// Disk Log�� ��� Page ID�������� Hash Table��
// �޾Ƶξ��ٰ� �Ѳ����� Redo�Ѵ�.
//
// �α� ���ڵ�� �α� ���� ��ü�� �α׹���
// �޸𸮸� ����Ű�� �Ǵµ�, �̷��� �޸𸮸�
// �����ϱ� ���ؼ��� �α� ������
// open�صξ�� �Ѵ�.
//
// �� ���, Disk Log�� �ƴ϶� Memory Log����
// Open�� �α����� �ȿ� �����Ͽ�
// �޸� ���� �߻��Ѵ�.

// ���������� �α��� ��� Decompress Buffer��
// �αװ� ������, �� ���۰� Ư�� ũ�� �̻�����
// Ŀ���� Disk Log�� Hash�κ��� Buffer�� �ݿ��Ѵ�.
//
// Disk�α��� ��� ������� ���� �α׵�
// Decompress Buffer�κ��� �Ҵ�� �޸𸮿�
// �α׸� �����ϰ� �̸� PID-Log Hash Table����
// ����Ű���� �Ѵ�.
//
// �̷��� �ϸ� �α������� Open��ä�� ������
// �ʿ䰡 �����Ƿ�,
// �ϳ��� �α������� �� �аԵǸ� close�� �Ѵ�.
/*
   [IN] aMemoryHandle - �α׸� ������ �޸𸮸� �Ҵ���� �ڵ�
   [IN] aOrgLogPtr - �����α� �ּ�
   [IN] aOrgLogSize - �����α� ũ��
   [OUT] aCopiedLogPtr -����� �α��� �ּ�
 */
IDE_RC smrRedoLSNMgr::makeCopyOfDiskLog( iduMemoryHandle * aMemoryHandle,
                                         SChar           * aOrgLogPtr,
                                         UInt              aOrgLogSize,
                                         SChar          ** aCopiedLogPtr )
{
    IDE_DASSERT( aMemoryHandle != NULL );
    IDE_DASSERT( aOrgLogSize > 0 );
    IDE_DASSERT( aOrgLogPtr != NULL );
    IDE_DASSERT( aCopiedLogPtr != NULL );

    SChar * sLogMemory;

    IDE_TEST( aMemoryHandle->prepareMemory( aOrgLogSize,
                                            (void**) & sLogMemory )
              != IDE_SUCCESS );

    idlOS::memcpy( sLogMemory, aOrgLogPtr, aOrgLogSize );

    *aCopiedLogPtr = sLogMemory;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}
