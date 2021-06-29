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

/***********************************************************************
 * PROJ-2102 The Fast Secondary Buffer
 **********************************************************************/

#ifndef _O_SDS_BCB_H_
#define _O_SDS_BCB_H_ 1

#include <iduLatch.h>
#include <iduMutex.h>
#include <idv.h>
#include <idl.h>

#include <smDef.h>
#include <sdbDef.h>
#include <sdbBCB.h> 

/* --------------------------------------------------------------------
 * Ư�� BCB�� ��� ������ �����ϴ��� ���θ�
 * �˻��Ҷ� ���
 * ----------------------------------------------------------------- */
typedef idBool (*sdsFiltFunc)( void *aBCB, void *aFiltAgr );

/* --------------------------------------------------------------------
 * BCB�� ���� ����
 * ----------------------------------------------------------------- */
typedef enum
{
    SDS_SBCB_FREE = 0,  /*���� ������ �ʴ� ����. hash���� ���ŵǾ� �ִ�.*/
    SDS_SBCB_CLEAN,     /* hash���� ���ٰ����ϰ� ����� ���� ���� 
                         * ��ũ IO���� �׳� replace�� �����ϴ�. 
                         * ���� ���°� CLEAN���� �ٽ� �������� ���� �ʴ´�. */
    SDS_SBCB_DIRTY,     /* hash���� ���ٰ����ϰ� ����� ���� ���� . 
                         * replace�Ϸ��� HDD�� flush �ؾ� �Ѵ�. */
    SDS_SBCB_INIOB,     /* flusher�� flush�� ���� �ڽ��� ���� ����(IOB)�� 
                         * �� BCB ������ ������ ���� */
    SDS_SBCB_OLD        /* IOB�� �ִµ� ���� �������� ���ͼ� Flusher�� 
                           �ش� �۾��� �������� ������ �ϴ� ���� */
} sdsSBCBState;

class sdsBCB
{

public:
    IDE_RC initialize( UInt aBCBID );

    IDE_RC destroy( void );

    IDE_RC setFree( void );

    IDE_RC dump( sdsBCB* aSBCB );

    /* BCB mutex */
    inline IDE_RC lockBCBMutex( idvSQL *aStatistics )
    {
        return mBCBMutex.lock( aStatistics);
    }

    inline IDE_RC unlockBCBMutex( void )
    {
        return mBCBMutex.unlock() ;
    }

    /* ReadIOMutex  mutex */
    inline IDE_RC lockReadIOMutex( idvSQL *aStatistics )
    {
        return mReadIOMutex.lock( aStatistics );
    }

    inline IDE_RC unlockReadIOMutex( void )
    {
        return mReadIOMutex.unlock();
    }

    inline IDE_RC tryLockReadIOMutex( idBool * aLocked )
    {
        return mReadIOMutex.trylock( * aLocked );
    }

private:

public:
    /* ���� �κ� */
    SD_BCB_PARAMETERS
    /* SBCB ���� ID */
    ULong          mSBCBID;            
    /* �ش� page�� ����. �� dirty / clean ���� */
    sdsSBCBState   mState;         
    /* state�� ����,�����Ҷ� ȹ���Ѵ� */
    iduMutex       mBCBMutex;     
    /* �б�� ���ſ����� ���� mutex */
    iduMutex       mReadIOMutex;     
    /* �����ϴ� BCB */
    sdbBCB       * mBCB;           
    /* Page�� Secondary�� ���� ���� page�� LSN */
    smLSN          mPageLSN;

private:

};

#endif //_O_SDS_BCB_H_

