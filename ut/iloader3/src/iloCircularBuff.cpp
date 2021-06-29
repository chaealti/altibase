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
 
#include <ilo.h>

CCircularBuf::CCircularBuf()
{
    //Initialize���� �ʱ�ȭ ��
}

void CCircularBuf::Initialize( ALTIBASE_ILOADER_HANDLE aHandle )
{
    iloaderHandle *sHandle = (iloaderHandle *) aHandle;
    
    m_nBufSize      = MAX_CIRCULAR_BUF;
    m_nDataSize     = 0;
    m_pCircularBuf  = (SChar*)idlOS::malloc(MAX_CIRCULAR_BUF+1);
    m_pStartBuf     = m_pCurrBuf = m_pCircularBuf;
    m_pEndBuf       = &m_pCircularBuf[MAX_CIRCULAR_BUF];
    m_bEOF          = ILO_FALSE;
    // BUG-24473 Iloader�� CPU 100% ���� �м�
    // thr_yield ��� sleep �� ����Ѵ�.
    mSleepTime.initialize(0, 1000);
    idlOS::thread_mutex_init(&(sHandle->mParallel.mCirBufMutex));
}

CCircularBuf::~CCircularBuf()
{
    //Finalize���� ������
}

void CCircularBuf::Finalize( ALTIBASE_ILOADER_HANDLE aHandle )
{
    iloaderHandle *sHandle = (iloaderHandle *) aHandle;
    
    idlOS::thread_mutex_destroy(&(sHandle->mParallel.mCirBufMutex));
    //BUG-23188
    idlOS::free(m_pCircularBuf);
}

SInt CCircularBuf::WriteBuf( ALTIBASE_ILOADER_HANDLE aHandle, SChar* pBuf, SInt aSize)
{   
    SInt sRet    = 0;
    SInt sPos    = 0;
    SInt sSpare  = 0;

    iloaderHandle *sHandle = (iloaderHandle *) aHandle;
    
    while ( (GetDataSize(sHandle) + aSize) > MAX_CIRCULAR_BUF )
    {
        //Buffer������ ���� ������ ��ٸ�
        // BUG-24473 Iloader�� CPU 100% ���� �м�
        // thr_yield ��� sleep �� ����Ѵ�.
        idlOS::sleep( mSleepTime );

        /* BUG-24211
         * -L �ɼ����� ������ �Է��� ��������� ������ ���, FileRead Thread�� ��Ӵ�� ���� �ʵ��� 
         * Load�ϴ� Thread�� ������ 0�� �� ��� �����ϵ��� �Ѵ�.
         */
        IDE_TEST ( sHandle->mParallel.mLoadThrNum == 0 )
    }

    iloMutexLock( sHandle, &(sHandle->mParallel.mCirBufMutex));

    sPos = m_pCurrBuf - m_pCircularBuf;
    sSpare = m_pEndBuf - m_pCurrBuf;

    if( sSpare >= aSize) {
        idlOS::memcpy(m_pCircularBuf + sPos, pBuf, aSize);
        m_pCurrBuf = m_pCurrBuf + aSize;
    }
    else
    {
        idlOS::memcpy(m_pCircularBuf + sPos, pBuf, sSpare);
        idlOS::memcpy(m_pCircularBuf, (SChar*)pBuf + sSpare, aSize - sSpare);
        m_pCurrBuf = m_pCircularBuf + aSize - sSpare;
    }

    m_nDataSize += aSize;
    //BUG-22389
    //Buffer write�� Return ���� Writed Size
    sRet = aSize;
    iloMutexUnLock(sHandle, &(sHandle->mParallel.mCirBufMutex));

    return sRet;

    IDE_EXCEPTION_END;

    return 0;
}

SInt CCircularBuf::ReadBuf( ALTIBASE_ILOADER_HANDLE aHandle, SChar* pBuf, SInt aSize)
{
    /*
     * �Ʒ��� Code�� ���ü��� ������ �ټ�������, ������ ������ϱⰡ �ſ� ��ٷӴ�.. (����� ����Ʈ�� �������� �߻�)
     * �����ÿ� ���� ���.
     */

    //BUG-22434
    //Double Buffer�� ���� ������ ������.
    SInt sSize = aSize;

    iloaderHandle *sHandle = (iloaderHandle *) aHandle;

    while ( GetDataSize(sHandle) < aSize )
    {
        /* PROJ-1714
         * ���� �����Ͱ� ���� �� ���� ���...
         * pthread_cond_wait, pthread_cond_signal �Ǵ� pthread_cond_broadcast�� ����� ���,
         * signal ���� �ӵ��� ������ ���� ���ϰ� �߻��Ͽ� thr_yield()�� ��ü����..
         */
        // BUG-24473 Iloader�� CPU 100% ���� �м�
        // thr_yield ��� sleep �� ����Ѵ�.
        idlOS::sleep( mSleepTime );
        
        /* PROJ-1714
         * GetEOF() ������ GetDataSize()�� ȣ���ؾ��Ѵ�.
         * ������ �ٲ� ���, ���� �������� Datafile�� ������ ���������� �����Ͱ� �Էµ��� ���� �� �ִ�.
         */
        //��� �ϴ� ���ȿ� EOF�� �� �� ����. 
        if ( GetEOF(sHandle) == ILO_TRUE )
        {
            if ( GetDataSize(sHandle) == 0 )
            {
                return -1;
            }
            else
            {
                //BUG-22434
                // ���� �ִ� �����Ͱ� aSize ���� ���� ��� �������� �д´�.
                // �׷��� ���� ��쿡�� aSize��ŭ�� �д´�.
                // �̴� ���� While ������ GetDataSize() < aSize�� �����Ͽ� ������� ������
                // ���⿡���� �ݵ�� ���� ������ �������� �����Ƿ� �Ʒ��� ���� ó���� �ʿ��ϴ�.
                if ( GetDataSize(sHandle) < aSize )
                {
                    sSize = GetDataSize(sHandle);
                }
                break;
            }
        }
    }
     return ReadBufReal( sHandle, pBuf, sSize);
}

SInt CCircularBuf::GetDataSize( ALTIBASE_ILOADER_HANDLE aHandle )
{
    iloaderHandle *sHandle = (iloaderHandle *) aHandle;
    
    SInt sRet = 0;
    iloMutexLock( sHandle, &(sHandle->mParallel.mCirBufMutex) );
    sRet = m_nDataSize;
    iloMutexUnLock( sHandle, &(sHandle->mParallel.mCirBufMutex) );

    return sRet;
}

SInt CCircularBuf::GetBufSize( ALTIBASE_ILOADER_HANDLE aHandle )
{
    iloaderHandle *sHandle = (iloaderHandle *) aHandle;
    
    SInt sRet = 0;
    iloMutexLock( sHandle, &(sHandle->mParallel.mCirBufMutex) );
    sRet = m_nBufSize - m_nDataSize;
    iloMutexUnLock( sHandle, &(sHandle->mParallel.mCirBufMutex) );

    return sRet;
}


SInt CCircularBuf::ReadBufReal( ALTIBASE_ILOADER_HANDLE aHandle,
                                SChar* pBuf,
                                SInt aSize)
{
    SInt sRet    = 0;
    SInt sSpare  = 0;
    SInt sPos    = 0;
    
    iloaderHandle *sHandle = (iloaderHandle *) aHandle;
    
    iloMutexLock( sHandle, &(sHandle->mParallel.mCirBufMutex) );
    
    sSpare     = m_pEndBuf - m_pStartBuf;
    sPos       = m_pStartBuf - m_pCircularBuf;
    
    if( sSpare >= aSize )
    {
        idlOS::memcpy(pBuf, m_pCircularBuf + sPos, aSize);
        m_pStartBuf = m_pStartBuf + aSize;
    }
    else
    {
        idlOS::memcpy(pBuf, m_pCircularBuf + sPos, sSpare);
        idlOS::memcpy((SChar*)pBuf + sSpare, m_pCircularBuf, aSize - sSpare);
        m_pStartBuf = m_pCircularBuf + aSize - sSpare;
    }
    
    m_nDataSize -= aSize;
    //BUG-22389
    //Buffer Read�� Return ���� Readed Size
    sRet = aSize;
    iloMutexUnLock( sHandle, &(sHandle->mParallel.mCirBufMutex) );

    return sRet;
}

iloBool CCircularBuf::GetEOF( ALTIBASE_ILOADER_HANDLE aHandle )
{
    iloBool sRet;

    iloaderHandle *sHandle = (iloaderHandle *) aHandle;
    
    iloMutexLock( sHandle, &(sHandle->mParallel.mCirBufMutex) );
    sRet = m_bEOF;
    iloMutexUnLock( sHandle, &(sHandle->mParallel.mCirBufMutex) );
    return sRet;
}

void CCircularBuf::SetEOF( ALTIBASE_ILOADER_HANDLE aHandle, iloBool aValue )
{
    iloaderHandle *sHandle = (iloaderHandle *) aHandle;
    
    iloMutexLock( sHandle, &(sHandle->mParallel.mCirBufMutex) );
    m_bEOF = aValue;
    iloMutexUnLock( sHandle, &(sHandle->mParallel.mCirBufMutex) );
}
