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

#ifndef _O_SDB_MPR_MGR_H_
#define _O_SDB_MPR_MGR_H_ 1

#include <sdp.h>

class sdbMPRMgr
{
public:
    /* BUG-33762 The MPR should uses the mempool instead allocBuff4DirectIO
     * function. */
    static IDE_RC initializeStatic();

    static IDE_RC destroyStatic();

    IDE_RC initialize( idvSQL           * aStatistics,
                       scSpaceID          aSpaceID,
                       sdpSegHandle     * aSegHandle,
                       sdbMPRFilterFunc   aFilter );

    IDE_RC destroy();

    IDE_RC beforeFst();

    IDE_RC getNxtPageID( void     * aFilterData,
                         scPageID * aPageID );
                         
    UChar* getCurPagePtr();

    /* TASK-4990 changing the method of collecting index statistics
     * MPR�� ParallelScan����� Sampling���� �̿��ϱ� ����,
     * ���� ParallalScan�� ���� Filter�� �⺻���� �����Ѵ�. */
    static idBool filter4PScan( ULong   aExtSeq,
                                void  * aFilterData );
    /* Sampling �� ParallelScan */
    static idBool filter4SamplingAndPScan( ULong   aExtSeq,
                                           void  * aFilterData );

private:
    IDE_RC initMPRKey();
    IDE_RC destMPRKey();

    IDE_RC fixPage( sdRID            aExtRID,
                    scPageID         aPageID );

    UInt getMPRCnt( sdRID    aCurReadExtRID,
                    scPageID aFstReadPID );

private:
    // ��� ����
    idvSQL            *mStatistics;

    // SCAN�� ������ Thread Count
    sdbMPRFilterFunc   mFilter;

    // Segment������ ���̺�
    sdpSegMgmtOp      *mSegMgmtOP;

    // Buffer Pool Pointer
    sdbBufferPool     *mBufferPool;

    // TableSpace ID
    scSpaceID          mSpaceID;

    // Segment Handle
    sdpSegHandle      *mSegHandle;

    // Segment Info�μ� Segment Scan�� �ʿ��� ������ �ִ�.
    sdpSegInfo         mSegInfo;

    // Segment Cache Info�μ� Segment Scan�� �ʿ��� ������ �ִ�.
    sdpSegCacheInfo    mSegCacheInfo;

    // ���� �а� �ִ� �������� �ִ� Extent Info
    sdpExtInfo         mCurExtInfo;

    // Multi Page Read Key
    sdbMPRKey          mMPRKey;

    // Multi Page Read Count�� �� ������ DataFile�� Read�Ѵ�.
    UInt               mMPRCnt;

    // ���� �а� �ִ� Extent RID
    sdRID              mCurExtRID;

    // ���� Extent Sequence Number
    ULong              mCurExtSeq;

    // ���� �а� �ִ� ������ ID
    scPageID           mCurPageID;

    // Segment���� ���������� �Ҵ�� ������ ID�� SeqNo
    scPageID           mLstAllocPID;
    ULong              mLstAllocSeqNo;

    // Segment�� ������ PID, SeqNo�� �����ؾ� �ϴ°�?
    idBool             mFoundLstAllocPage;

    // MPR�� ���ؼ� Buffer Pool�� Page�� Pin�Ǿ��ִ°�?
    idBool             mIsFetchPage;

    // MPR�� ���� ���̵��� Caching�� ���ΰ�?
    idBool             mIsCachePage;

    /* BUG-33762 [sm-disk-resource] The MPR should uses the mempool instead
     * allocBuff4DirectIO function. */
    static iduMemPool mReadIOBPool;
};

#endif // _O_SDB_MPR_MGR_H_
