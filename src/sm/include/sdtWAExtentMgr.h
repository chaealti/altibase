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
 * $Id
 *
 * Sort �� Hash�� �ϴ� ��������, �ִ��� ���ü� ó�� ���� �����ϵ��� �����ȴ�.
 * ���� SortArea, HashArea�� �ƿ츣�� �ǹ̷� ���ȴ�. ��
 * SortArea, HashArea��WorkArea�� ���Ѵ�.
 *
 * ���� PrivateWorkArea�� ����������, ù��° ������ P�� ���������� ���Ǳ�
 * ������(��-Page); WorkArea�� �����Ͽ���.
 * �Ϲ������� ���� �� Prefix�� WA�� ���������, ��� ���ܰ� �ִ�.
 * WCB - BCB�� ������ ���� ������, �ǹ̻� WACB���� WCB�� �����Ͽ���.
 * WPID - WAPID�� �� �� WPID�� �ٿ���.
 *
 **********************************************************************/

#ifndef _O_SDT_WA_EXTENT_MGR_H_
#define _O_SDT_WA_EXTENT_MGR_H_ 1

#include <idl.h>
#include <smDef.h>
#include <smiDef.h>
#include <sdtDef.h>
#include <iduLatch.h>
#include <smuMemStack.h>

class sdtWAExtentMgr
{
public:
    /******************************************************************
     * WorkArea
     ******************************************************************/
    static IDE_RC initializeStatic();

    static IDE_RC prepareCachedFreeNExts( idvSQL  * aStatistics );
    static IDE_RC resizeWAExtentPool( ULong aNewSize );
    static IDE_RC destroyStatic();

    static UInt getFreeWAExtentCount()
    {
        return mFreeExtentPool.getTotItemCnt();
    };
    static UInt getWAExtentCount() { return mWAExtentCount; }
    static UInt getMaxWAExtentCount() { return mMaxWAExtentCount; }

    static void updateMaxWAExtentCount()
    {
        SInt sCurUsedExtentCount = (SInt)getWAExtentCount() - (SInt)getFreeWAExtentCount();

        if ( sCurUsedExtentCount > (SInt)getMaxWAExtentCount() )
        {
            mMaxWAExtentCount = (UInt)sCurUsedExtentCount;
        }
    };
    static ULong getInitTotalWASize()
    {
        ULong sInitTotalWASize = smuProperty::getInitTotalWASize();
        ULong sMaxTotalWASize  = smuProperty::getMaxTotalWASize();

        /* �� �� ���������� ��ȯ�Ѵ�.
         * ���� �� �� �����Ƿ� ������ �޾Ƽ� ����Ѵ�.
         * �񱳿� ��ȯ �� ������ �ȴ�. (�񱳽ÿ� �۾Ҵ� ���� return���� ũ�� ���� �Ǿ� ��ȯ�ϸ� �ȵȴ�.)
         * ������ ���� �Ŀ� return �ϱ� ���� property���� ���� �Ǵ� ���� ��� ����. */
        return ( sInitTotalWASize < sMaxTotalWASize) ? sInitTotalWASize : sMaxTotalWASize;
    }
    static void lock()
    {
        (void)mMutex.lock( NULL );
    }
    static void unlock()
    {
        (void) mMutex.unlock();
    }

    static UInt calcWAExtentCount( ULong aWorkAreaSize )
    {
        return (UInt)(( aWorkAreaSize + SDT_WAEXTENT_SIZE -1 ) / SDT_WAEXTENT_SIZE );
    }

    /******************************************************************
     * Segment
     ******************************************************************/
    static IDE_RC initWAExtents( idvSQL            * aStatistics,
                                 smiTempTableStats * aStatsPtr,
                                 sdtWAExtentInfo   * aWAExtentInfo,
                                 UInt                aInitWAExtentCount );

    static IDE_RC allocWAExtents( sdtWAExtentInfo  * aWAExtentInfo,
                                  UInt               aGetWAExtentCount );

    static IDE_RC memAllocWAExtent( sdtWAExtentInfo * aWAExtentInfo );

    static IDE_RC freeWAExtents( sdtWAExtentInfo * aWAExtentInfo );
    static IDE_RC freeAllNExtents( scSpaceID           aSpaceID,
                                   sdtNExtFstPIDList * aNExtFstPIDList );
    static IDE_RC allocFreeNExtent( idvSQL            * aStatistics,
                                    smiTempTableStats * aStatsPtr,
                                    scSpaceID           aSpaceID,
                                    sdtNExtFstPIDList * aNExtFstPIDList );

public:
    /***********************************************************
     * FixedTable�� �Լ��� ( for X$TEMPINFO )
     ***********************************************************/
    static IDE_RC buildTempInfoRecord( void                * aHeader,
                                       iduFixedTableMemory * aMemory );

    static void dumpNExtentList( sdtNExtFstPIDList & aNExtentList,
                                 SChar             * aOutBuf,
                                 UInt                aOutSize );

private:
    static UInt                  mWAExtentCount;
    static UInt                  mMaxWAExtentCount;
    static iduMutex              mMutex;
    static iduStackMgr           mFreeExtentPool;    /*�� Extent�� �����ϴ� �� */
    static iduMemPool            mNExtentArrPool;    /*�� NExtMgr�� �����ϴ� �� */
};

#endif //_O_SDT_WA_EXTENT_MGR
