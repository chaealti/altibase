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
 
#ifndef _O_ILO_CIRCULARBUFF_H
#define _O_ILO_CIRCULARBUFF_H

#include <iloApi.h>
/*
 * PROJ-1714
 * Data Uploading��, ����ϴ� ���� ���� Class �����
 * File���� ���� �����͸� ���� ���ۿ� �����ϰ�, ����� �����͸� ���� �� �ִ�.
 * Thread Safety~~
 */
 
class CCircularBuf  
{
public:
    CCircularBuf();
    virtual ~CCircularBuf();

private:
    SChar  *m_pCircularBuf;                         //����Ÿ ���� ����
    SChar  *m_pStartBuf, *m_pCurrBuf, *m_pEndBuf;   //����Ÿ ������ġ, ����Ÿ ����ġ, ���� ����ġ
    SInt    m_nBufSize, m_nDataSize;                //���� ������, ���ۿ� �����ϴ� ����Ÿ ������
    iloBool  m_bEOF;                                 //File�� �� ����
    PDL_Time_Value mSleepTime;

public:
    void    Initialize( ALTIBASE_ILOADER_HANDLE aHandle );                           //���� �ʱ�ȭ
    void    Finalize( ALTIBASE_ILOADER_HANDLE aHandle );                             //���� Free
    SInt    WriteBuf( ALTIBASE_ILOADER_HANDLE aHandle ,
                      SChar* pBuf,
                      SInt nSize);      //���ۿ� ����Ÿ �����Լ�
    SInt    ReadBuf( ALTIBASE_ILOADER_HANDLE aHandle, SChar* pBuf, SInt nSize);       //���� ����Ÿ �ε��Լ�(size��ŭ)
    void    SetEOF( ALTIBASE_ILOADER_HANDLE aHandle, iloBool aValue);
    iloBool  GetEOF( ALTIBASE_ILOADER_HANDLE aHandle );
    
private:
    SInt    ReadBufReal( ALTIBASE_ILOADER_HANDLE aHandle, SChar* pBuf, SInt nSize);
    SInt    GetDataSize( ALTIBASE_ILOADER_HANDLE aHandle );                          //���ۿ� �����ϴ� ũ�� ����
    SInt    GetBufSize( ALTIBASE_ILOADER_HANDLE aHandle );                           //���ۿ� ���尡���� ũ�� ����    
};

#endif  /* _O_ILO_CIRCULARBUFF_H */

