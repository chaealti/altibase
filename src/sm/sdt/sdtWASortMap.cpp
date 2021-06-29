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
 * $Id $
 **********************************************************************/

#include <sdtWASortMap.h>
#include <sdtSortDef.h>
#include <smErrorCode.h>


/**************************************************************************
 * Description :
 * WAMap�� �����. ������ Group�� Count��ŭ �����.
 * ���� Count�� 0�� ���, ���� Ȯ���Ѵ�.
 *
 * <IN>
 * aWASegment     - ��� WASegment
 * aWAGroupID     - ��� Group ID
 * aWMType        - WAMap�� Type
 * aSlotCount     - WAMap�� ��� Slot�� ����
 * aVersionCount  - Versioning�ϱ� ���� ����
 * <OUT>
 * aRet           - ������� WAMap
 ***************************************************************************/
IDE_RC sdtWASortMap::create( sdtSortSegHdr* aWASegment,
                             sdtGroupID     aWAGID,
                             sdtWMType      aWMType,
                             UInt           aSlotCount,
                             UInt           aVersionCount,
                             sdtWASortMapHdr  * aWAMapHdr )
{
    sdtSortGroup       * sWAGrpInfo = sdtSortSegment::getWAGroupInfo( aWASegment, aWAGID );
    UInt                 sSlotSize;

    aWAMapHdr->mWASegment    = aWASegment;
    aWAMapHdr->mWAGID        = aWAGID;
    aWAMapHdr->mBeginWPID    = sWAGrpInfo->mBeginWPID;
    aWAMapHdr->mSlotCount    = aSlotCount;
    aWAMapHdr->mVersionCount = aVersionCount;
    aWAMapHdr->mVersionIdx   = 0;
    sSlotSize  = ( aWMType ==  SDT_WM_TYPE_RUNINFO ) ? ID_SIZEOF(sdtTempMergeRunInfo) : ID_SIZEOF(vULong);
    aWAMapHdr->mSlotSize     = sSlotSize;

    IDE_DASSERT( sSlotSize < SD_PAGE_SIZE );
    IDE_DASSERT( sSlotSize <= SDT_WAMAP_SLOT_MAX_SIZE );

    IDE_ERROR( sWAGrpInfo->mPolicy == SDT_WA_REUSE_INMEMORY );

    IDE_TEST_RAISE( (ULong)sSlotSize * aSlotCount * aVersionCount >
                    (ULong)sdtSortSegment::getWAGroupPageCount( sWAGrpInfo )
                    * SD_PAGE_SIZE,
                    ERROR_MANY_SLOT );

    sWAGrpInfo->mSortMapHdr = aWAMapHdr;

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERROR_MANY_SLOT );
    {
        IDE_SET( ideSetErrorCode( smERR_ABORT_TooManySlotIndiskTempTable ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**************************************************************************
 * Description :
 * �ش� WAmap�� ��ȿȭ��Ų��.
 *
 * <IN>
 * aWAMap         - ��� WAMap
 ***************************************************************************/
void sdtWASortMap::disable( sdtSortSegHdr * aWASegment )
{
    aWASegment->mSortMapHdr.mWAGID = SDT_WAGROUPID_NONE;
}


/**************************************************************************
 * Description :
 * WAMap�� Pointer�� ��� �缳���Ѵ�. Dump���̴�.
 *
 * <IN>
 * aWAMap         - ��� WAMap
 * aWASegment     - ��� WASegment
 ***************************************************************************/
IDE_RC sdtWASortMap::resetPtrAddr( sdtSortSegHdr* aWASegment )
{
    sdtWASortMapHdr    * sWAMapHdr = &aWASegment->mSortMapHdr;
    sdtSortGroup       * sGrpInfo;

    sWAMapHdr->mWASegment = aWASegment;

    if ( sWAMapHdr->mWAGID != SDT_WAGROUPID_NONE )
    {
        sGrpInfo = sdtSortSegment::getWAGroupInfo( aWASegment,
                                                   sWAMapHdr->mWAGID );
        sGrpInfo->mSortMapHdr = sWAMapHdr;
    }

    return IDE_SUCCESS;
}

/**************************************************************************
 * Description :
 * WAMap�� ���� ������ �� Slot�� �߰��Ͽ� Ȯ���Ѵ�.
 * ���� �� Ȯ���� ���������� ���� Ȯ���ϴµ�,  aLimitPID ��������
 * Ȯ�� �����ϴ�. Ȯ���� �����ϸ�, idx�� UINT_MAX�� ��ȯ�Ѵ�.
 *
 * <IN>
 * aWAMap         - ��� WAMap
 * aLimitPID      - Ȯ���� �� �ִ� �Ѱ� PID
 * <OUT>
 * aIdx           - Ȯ���� Slot�� ��ġ
 ***************************************************************************/
IDE_RC sdtWASortMap::expand( sdtWASortMapHdr * aWAMapHdr,
                             scPageID          aLimitPID,
                             UInt            * aIdx )
{
    scPageID          sPID;
    UInt              sSlotSize;

    IDE_ERROR( aWAMapHdr->mWAGID != SDT_WAGROUPID_NONE );

    sSlotSize = aWAMapHdr->mSlotSize;
    /* SlotCount�� �����Ͽ����� �߰� �������� �ʿ�� �ϴ� ��� */
    if ( ( aWAMapHdr->mSlotCount * sSlotSize * aWAMapHdr->mVersionCount )
         / SD_PAGE_SIZE !=
         ( ( aWAMapHdr->mSlotCount + 1 ) * sSlotSize * aWAMapHdr->mVersionCount
           / SD_PAGE_SIZE ) )
    {
        sPID = aWAMapHdr->mBeginWPID
            + ( ( aWAMapHdr->mSlotCount + 1 )
                * sSlotSize * aWAMapHdr->mVersionCount
                / SD_PAGE_SIZE );
        if ( sPID >= aLimitPID )
        {
            (*aIdx) = SDT_WASLOT_UNUSED;
        }
        else
        {
            (*aIdx) = aWAMapHdr->mSlotCount;
            aWAMapHdr->mSlotCount++;
        }
    }
    else
    {
        (*aIdx) = aWAMapHdr->mSlotCount;
        aWAMapHdr->mSlotCount++;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

void sdtWASortMap::dumpWASortMap( sdtWASortMapHdr * aMapHdr,
                                  SChar           * aOutBuf,
                                  UInt              aOutSize )
{
    sdtTempMergeRunInfo *sRunInfo;
    UChar       * sSlotPtr;
    UInt          i;
    UInt        sOffset;
    scPageID    sPageID;
    UChar     * sPagePtr;
    sdtWCB    * sWCBPtr;

    if ( aMapHdr != NULL )
    {
        (void)idlVA::appendFormat( aOutBuf,
                                   aOutSize,
                                   "DUMP WAMAP:%s\n"
                                   "       SegPtr   :0x%"ID_xINT64_FMT"\n"
                                   "       GroupID  :%"ID_UINT32_FMT"\n"
                                   "       BeginWPID:%"ID_UINT32_FMT"\n"
                                   "       SlotCount:%"ID_UINT32_FMT"\n",
                                   ( aMapHdr->mWMType == SDT_WM_TYPE_RUNINFO ) ? "RUNINFO" : "POINTER",
                                   aMapHdr->mWASegment,
                                   aMapHdr->mWAGID,
                                   aMapHdr->mBeginWPID,
                                   aMapHdr->mSlotCount );

        if ( aMapHdr->mWAGID != SDT_WAGROUPID_NONE )
        {
            for( i = 0 ; i< aMapHdr->mSlotCount ; i++ )
            {
                {
                    sOffset = getOffset( i,
                                         getSlotSize( aMapHdr ),
                                         aMapHdr->mVersionIdx,
                                         aMapHdr->mVersionCount );

                    sPageID = aMapHdr->mBeginWPID + ( sOffset / SD_PAGE_SIZE ) ;

                    sWCBPtr = sdtSortSegment::getWCBWithLnk( aMapHdr->mWASegment,
                                                             sPageID );

                    sPagePtr = sdtSortSegment::getWAPagePtr( sWCBPtr );

                    IDE_ASSERT( sPagePtr != NULL );

                    sSlotPtr = sPagePtr + ( sOffset % SD_PAGE_SIZE );
                }

                if ( aMapHdr->mWMType == SDT_WM_TYPE_RUNINFO )
                {
                    sRunInfo = (sdtTempMergeRunInfo*)sSlotPtr;

                    if (( sRunInfo->mRunNo  != 0 ) ||
                        ( sRunInfo->mPIDSeq != 0 ) ||
                        ( sRunInfo->mSlotNo != 0 ))
                    {
                        (void)idlVA::appendFormat( aOutBuf,
                                                   aOutSize,
                                                   "%6"ID_UINT32_FMT" : "
                                                   "[%12"ID_UINT32_FMT
                                                   " %12"ID_UINT32_FMT
                                                   " %12"ID_UINT32_FMT"]\n",
                                                   i,
                                                   sRunInfo->mRunNo,
                                                   sRunInfo->mPIDSeq,
                                                   sRunInfo->mSlotNo );
                    }
                }
                else
                {
                    if ( *(vULong*)sSlotPtr != 0 )
                    {
                        (void)idlVA::appendFormat( aOutBuf,
                                                   aOutSize,
                                                   "%6"ID_UINT32_FMT" : "
                                                   "0x%\n"ID_xINT64_FMT,
                                                   i,
                                                   (vULong)sSlotPtr );
                    }
                }
            }
        }
        (void)idlVA::appendFormat( aOutBuf, aOutSize, "\n" );
    }

    return;
}
