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
 * $Id: sdnCTL.cpp 29513 2008-11-21 11:12:43Z upinel9 $
 **********************************************************************/

#include <smDef.h>
#include <smErrorCode.h>
#include <smu.h>
#include <sdp.h>
#include <sdc.h>
#include <sdnReq.h>


/*********************************************************************
 * FUNCTION DESCRIPTION : sdnIndexCTL::init                          *
 * ------------------------------------------------------------------*
 * Touched Transaction Layer�� �ʱ�ȭ �Ѵ�.                          *
 *********************************************************************/
IDE_RC sdnIndexCTL::init( sdrMtx         * aMtx,
                          sdpSegHandle   * aSegHandle,
                          sdpPhyPageHdr  * aPage,
                          UChar            aInitSize )
{
    sdnCTLayerHdr  * sCTLayerHdr;
    UChar            sInitSize;

    sInitSize = IDL_MAX( aSegHandle->mSegAttr.mInitTrans, aInitSize );

    IDE_ERROR( sdnIndexCTL::initLow( aPage, sInitSize )
               == IDE_SUCCESS );
    sCTLayerHdr = (sdnCTLayerHdr*)getCTL( aPage );

    IDE_TEST( sdrMiniTrans::writeLogRec( aMtx,
                                         (UChar*)aPage,
                                         NULL,
                                         ID_SIZEOF( UChar ),
                                         SDR_SDN_INIT_CTL )
              != IDE_SUCCESS );

    IDE_TEST( sdrMiniTrans::write( aMtx,
                                   (UChar*)&(sCTLayerHdr->mCount),
                                   ID_SIZEOF( UChar ) )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : sdnIndexCTL::initLow                       *
 * ------------------------------------------------------------------*
 * Physical Layer������ CTL������ �ʱ�ȭ �Ѵ�.                       *
 *********************************************************************/
IDE_RC sdnIndexCTL::initLow( sdpPhyPageHdr  * aPage,
                             UChar            aInitSize )
{
    sdnCTLayerHdr  * sCTLayerHdr;
    sdnCTS         * sArrCTS;
    UInt             i;
    idBool           sTrySuccess = ID_FALSE;

    if( sdpSlotDirectory::getSize(aPage) > 0 )
    {
        sdpPhyPage::shiftSlotDirToBottom( aPage,
                                          idlOS::align8(ID_SIZEOF(sdnCTLayerHdr)) );
    }

    sdpPhyPage::initCTL( aPage,
                         ID_SIZEOF(sdnCTLayerHdr),
                         (UChar**)&sCTLayerHdr );

    if( aInitSize > 0 )
    {
        IDE_ERROR( sdpPhyPage::extendCTL( aPage,
                               aInitSize,
                               ID_SIZEOF( sdnCTS ),
                               (UChar**)&sArrCTS,
                               &sTrySuccess )
                   == IDE_SUCCESS );
        if( sTrySuccess != ID_TRUE )
        {
            ideLog::log( IDE_SERVER_0,
                         "Init size : %u\n",
                         aInitSize );
            dumpIndexNode( aPage );
            IDE_ASSERT( 0 );
        }
    }

    sCTLayerHdr->mUsedCount = 0;
    sCTLayerHdr->mCount = aInitSize;

    for( i = 0; i < sCTLayerHdr->mCount; i++ )
    {
        idlOS::memset( &sArrCTS[i], 0x00, ID_SIZEOF(sdnCTS) );
        sArrCTS[i].mState  = SDN_CTS_DEAD;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/*********************************************************************
 * FUNCTION DESCRIPTION : sdnIndexCTL::allocCTS                      *
 * ------------------------------------------------------------------*
 * Key�� Ʈ����� ������ �����ϱ� ���� ������ Ȯ���ϴ� �Լ��̴�.     *
 * �ش� �Լ��� Key Insertion/Deletion�� ���ؼ� ȣ��ȴ�.             *
 * 5���� Step���� �����Ǿ� ������,                                   *
 *  1. �ڽ��� ������ Ʈ������� �ִٸ�                               *
 *  2. DEAD CTS�� �ִٸ�                                             *
 *  3. Agable CTS�� �ִٸ�                                           *
 *  4. Uncommitted CTS�߿� Commit�� CTS�� �ִٸ�                     *
 *  5. CTL�� Ȯ���Ҽ� �ִٸ�                                         *
 * AllocCTS�� �����Ѵ�. �׷��� ���� ��쿡�� �ݵ�� ��õ��� �ؾ�    *
 * �Ѵ�. ��õ��� �����ϰ� �Ϸ��� ����� rollback�� ������ ����ؼ�  *
 * �� �ȵȴ�.                                                        *
 *********************************************************************/
IDE_RC sdnIndexCTL::allocCTS( idvSQL             * aStatistics,
                              sdrMtx             * aMtx,
                              sdpSegHandle       * aSegHandle,
                              sdpPhyPageHdr      * aPage,
                              UChar              * aCTSlotNum,
                              sdnCallbackFuncs   * aCallbackFunc,
                              UChar              * aContext )
{
    UInt        i;
    UInt        sState  = 0;
    sdnCTL    * sCTL;
    sdnCTS    * sCTS;
    idBool      sSuccess;
    UChar       sDeadCTS;
    UChar       sAgableCTS;
    UChar     * sPageStartPtr;
    SChar     * sDumpBuf;
    smSCN       sSysMinDskViewSCN;

    *aCTSlotNum          = SDN_CTS_INFINITE;
    sDeadCTS             = SDN_CTS_INFINITE;
    sAgableCTS           = SDN_CTS_INFINITE;

    // BUG-29506 TBT�� TBK�� ��ȯ�� offset�� CTS�� �ݿ����� �ʽ��ϴ�.
    // �����ϱ� ���� CTS �Ҵ� ���θ� ���Ƿ� �����ϱ� ���� PROPERTY�� �߰�
    IDE_TEST_CONT( smuProperty::getDisableTransactionBoundInCTS() == 1,
                    RETURN_SUCCESS );

    sCTL = getCTL( aPage );

    SMX_GET_MIN_DISK_VIEW( &sSysMinDskViewSCN );

    /*
     * 1. �ڽ��� Ʈ������� ������ CTS�� �ִ� ���
     */
    for( i = 0; i < getCount(sCTL); i++ )
    {
        sCTS = getCTS( sCTL, i );

        switch( sCTS->mState )
        {
            case SDN_CTS_UNCOMMITTED:
            {
                if( isMyTransaction( aMtx->mTrans, aPage, i ) == ID_TRUE )
                {
                    *aCTSlotNum = i;
                    IDE_CONT( RETURN_SUCCESS );
                }
                break;
            }

            case SDN_CTS_DEAD:
            {
                if( (sDeadCTS == SDN_CTS_INFINITE) &&
                    (sAgableCTS == SDN_CTS_INFINITE) )
                {
                    sDeadCTS = i;
                }
                break;
            }

            case SDN_CTS_STAMPED:
            {
                if( (sDeadCTS == SDN_CTS_INFINITE) &&
                    (sAgableCTS == SDN_CTS_INFINITE) )
                {
                    if( SM_SCN_IS_LT( &sCTS->mCommitSCN,
                                      &sSysMinDskViewSCN ) )
                    {
                        IDE_TEST( aCallbackFunc->mSoftKeyStamping( aMtx,
                                                                   aPage,
                                                                   i,
                                                                   aContext )
                                  != IDE_SUCCESS );

                        sAgableCTS = i;
                    }
                }
                break;
            }

            default:
            {
                ideLog::log( IDE_SERVER_0,
                             "CTS number : %u"
                             ", CTS state : %u"
                             ", CT slot number : %u\n",
                             i, sCTS->mState, aCTSlotNum );
                dumpIndexNode( aPage );

                // BUG-28785 Case-23923�� Server ������ ���ῡ ���� ����� �ڵ� �߰�
                ideLog::log( IDE_SERVER_0,
                             "sCTL->CTS[%u] State(%u) is invalied\n",
                             i,
                             sCTS->mState );

                sPageStartPtr = sdpPhyPage::getPageStartPtr( aPage );

                // Dump Page Hex Data And Header
                (void)sdpPhyPage::tracePage( IDE_SERVER_0,
                                             sPageStartPtr,
                                             "Dump Page:" );

                IDE_ASSERT( iduMemMgr::calloc( IDU_MEM_SM_SDN, 1,
                                               ID_SIZEOF( SChar ) * IDE_DUMP_DEST_LIMIT,
                                               (void**)&sDumpBuf )
                            == IDE_SUCCESS );
                sState = 1;

                // Dump CTL, CTS
                (void)dump( sPageStartPtr, (SChar*)sDumpBuf, IDE_DUMP_DEST_LIMIT );

                ideLog::log( IDE_SERVER_0, "%s\n", sDumpBuf );

                sState = 0;
                IDE_ASSERT( iduMemMgr::free( sDumpBuf ) == IDE_SUCCESS );

                IDE_ASSERT( 0 );
                break;
            }
        }
    }

    /*
     * 2. DEAD���� CTS�� �����ϴ� ��� CTS�� �ٷ� �����Ѵ�.
     */
    if( sDeadCTS != SDN_CTS_INFINITE )
    {
        *aCTSlotNum = sDeadCTS;
    }

    IDE_TEST_CONT( *aCTSlotNum != SDN_CTS_INFINITE, RETURN_SUCCESS );

    /*
     * 3. AGING�� ������ CTS�� �����ϴ� ���
     *    CTS�� �ٷ� �����Ѵ�.
     */
    if( sAgableCTS != SDN_CTS_INFINITE )
    {
        *aCTSlotNum = sAgableCTS;
    }

    IDE_TEST_CONT( *aCTSlotNum != SDN_CTS_INFINITE, RETURN_SUCCESS );

    /*
     * 4. UNCOMMITTED��������, Ŀ�Ե� Ʈ������� �ִ� ���
     *    delayed tts timestamping�� �ϰ� �����Ѵ�.
     */
    for( i = 0; i < getCount(sCTL); i++ )
    {
        sCTS = getCTS( sCTL, i );

        if( sCTS->mState == SDN_CTS_UNCOMMITTED )
        {
            sSuccess = ID_FALSE;

            IDE_TEST( aCallbackFunc->mHardKeyStamping( aStatistics,
                                                       aMtx,
                                                       aPage,
                                                       i,
                                                       aContext,
                                                       &sSuccess )
                      != IDE_SUCCESS );

            if( sSuccess == ID_TRUE )
            {
                *aCTSlotNum = i;
                break;
            }
        }
    }

    IDE_TEST_CONT( *aCTSlotNum != SDN_CTS_INFINITE, RETURN_SUCCESS );

    /*
     * 5. CTL�� Ȯ�� �����ϴٸ� Ȯ��� CTS�� ����Ѵ�.
     */
    IDE_TEST( extend( aMtx,
                      aSegHandle,
                      aPage,
                      ID_TRUE,
                      &sSuccess ) != IDE_SUCCESS );

    if( sSuccess == ID_TRUE )
    {
        *aCTSlotNum = getCount( sCTL ) - 1;
    }

    IDE_EXCEPTION_CONT( RETURN_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    switch( sState )
    {
        case 1:
            IDE_ASSERT( iduMemMgr::free( sDumpBuf ) == IDE_SUCCESS );
            sDumpBuf = NULL;
        default:
            break;
    }

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : sdnIndexCTL::allocCTS                      *
 * ------------------------------------------------------------------*
 * Key�� Ʈ����� ������ �����ϱ� ���� ������ Ȯ���ϴ� �Լ��̴�.     *
 * �ش� �Լ��� SMO(KEY ��й�/SPLIT)�ÿ� ȣ��ȴ�.                   *
 * �̹� ���� Ʈ������� ������ �ִ� ��쿡�� �ش� CTS�� ����ϰ�     *
 * �׷��� ���� ��쿡�� DEAD CTS�� �����Ѵ�.                       *
 * SMO�ÿ� CTS�� ���� ������ �̸� �Ҵ�޾ұ� ������ �ش� �Լ��� ���� *
 * �ϴ� ���� ����� �Ѵ�.                                          *
 *********************************************************************/
IDE_RC sdnIndexCTL::allocCTS( sdpPhyPageHdr * aSrcPage,
                              UChar           aSrcCTSlotNum,
                              sdpPhyPageHdr * aDstPage,
                              UChar         * aDstCTSlotNum )
{
    sdnCTL  * sDstCTL;
    sdnCTL  * sSrcCTL;
    sdnCTS  * sDstCTS;
    sdnCTS  * sSrcCTS;
    UChar     sDeadCTS;
    UInt      i;

    sDstCTL = getCTL( aDstPage );
    sSrcCTL = getCTL( aSrcPage );

    sSrcCTS = getCTS( sSrcCTL, aSrcCTSlotNum );

    sDeadCTS = SDN_CTS_INFINITE;
    *aDstCTSlotNum = SDN_CTS_INFINITE;

    /*
     * 1. ���� Ʈ������� �ִ��� �˻�
     */
    for( i = 0; i < getCount(sDstCTL); i++ )
    {
        sDstCTS = getCTS( sDstCTL, i );

        if( sDstCTS->mState != SDN_CTS_DEAD )
        {
            if( isSameTransaction( sDstCTS, sSrcCTS ) == ID_TRUE )
            {
                *aDstCTSlotNum = i;
                break;
            }
        }
        else
        {
            if( sDeadCTS == SDN_CTS_INFINITE )
            {
                sDeadCTS = i;
            }
        }
    }

    IDE_TEST_CONT( *aDstCTSlotNum != SDN_CTS_INFINITE, RETURN_SUCCESS );

    /*
     * 2. DEAD���� CTS�� �����ϴ� ��� CTS�� �ٷ� �����Ѵ�.
     */
    if( sDeadCTS != SDN_CTS_INFINITE )
    {
        *aDstCTSlotNum = sDeadCTS;
    }

    IDE_EXCEPTION_CONT( RETURN_SUCCESS );

    return IDE_SUCCESS;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : sdnIndexCTL::bindCTS                       *
 * ------------------------------------------------------------------*
 * Key Insertion/Deletion�� ���ؼ� ȣ��Ǹ�,                         *
 * Ʈ����� ������ �ش� CTS�� �����Ѵ�.                              *
 * DEAD������ ���� �ʱ�ȭ�� �����Ѵ�.                            *
 * �ش� �Լ��� ������ ���Ŀ��� ����� ������ �ѹ�Ǿ�� �ȵȴ�.    *
 *********************************************************************/
IDE_RC sdnIndexCTL::bindCTS( sdrMtx           * aMtx,
                             scSpaceID          aSpaceID,
                             sdpPhyPageHdr    * aPage,
                             UChar              aCTSlotNum,
                             UShort             aKeyOffset )
{
    sdnCTL * sCTL;
    sdnCTS * sCTS;
    sdSID    sTSSlotSID;
    UChar    sUsedCount;
    UInt     i;

    sCTL       = getCTL( aPage );
    sCTS       = getCTS( aPage, aCTSlotNum );
    sTSSlotSID = smLayerCallback::getTSSlotSID( aMtx->mTrans );

    /* BUG-48064 : CTS CHAING ����� ���������Ƿ� STAMPED ������ CTS�� �ü�����. */
    IDE_DASSERT(sCTS->mState != SDN_CTS_STAMPED);

    if ( sCTS->mState == SDN_CTS_DEAD )
    {
        sUsedCount = sCTL->mUsedCount + 1;
        if ( sUsedCount >= SDN_CTS_INFINITE )
        {
            ideLog::log( IDE_SERVER_0,
                         "CT slot number : %u"
                         "\nUsed count : %u\n",
                         aCTSlotNum, sUsedCount );
            dumpIndexNode( aPage );
            IDE_ASSERT( 0 );
        }

        sCTS->mCommitSCN = smLayerCallback::getFstDskViewSCN( aMtx->mTrans );
        sCTS->mState     = SDN_CTS_UNCOMMITTED;
        sCTS->mRefCnt    = 1;

        idlOS::memset( sCTS->mRefKey,
                       0x00,
                       ID_SIZEOF(UShort) * SDN_CTS_MAX_KEY_CACHE );
        sCTS->mRefKey[0] = aKeyOffset;

        sCTS->mTSSlotPID = SD_MAKE_PID( sTSSlotSID );
        sCTS->mTSSlotNum = SD_MAKE_SLOTNUM( sTSSlotSID );

        IDE_TEST( sdrMiniTrans::writeNBytes(aMtx,
                                            (UChar*)&(sCTL->mUsedCount),
                                            (void*)&sUsedCount,
                                            ID_SIZEOF(UChar) )
                  != IDE_SUCCESS );
    }
    else
    {
        /*
         * UNCOMMITTED������ CTS�� Cache�������� �����Ѵ�.
         */
        for( i = 0; i < SDN_CTS_MAX_KEY_CACHE; i++ )
        {
            if( sCTS->mRefKey[i] == SDN_CTS_KEY_CACHE_NULL )
            {
                sCTS->mRefKey[i] = aKeyOffset;
                break;
            }
        }
        sCTS->mRefCnt++;
    }

    if( sCTS->mRefCnt == 1 )
    {
        IDE_TEST( smLayerCallback::addTouchedPage( aMtx->mTrans,
                                                   aSpaceID,
                                                   aPage->mPageID,
                                                   (SShort)aCTSlotNum )
                  != IDE_SUCCESS );
    }

    IDE_TEST( sdrMiniTrans::writeLogRec(aMtx,
                                        (UChar*)sCTS,
                                        (void*)sCTS,
                                        ID_SIZEOF(sdnCTS),
                                        SDR_SDP_BINARY )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : sdnIndexCTL::bindCTS                       *
 * ------------------------------------------------------------------*
 * SMO(Key ��й�/SPLIT)�� ���ؼ� ȣ��Ǹ�,                          *
 * Ʈ����� ������ �ش� CTS�� �����Ѵ�.                              *
 * DEAD������ ���� �ʱ�ȭ�� �����Ѵ�.                            *
 *********************************************************************/
IDE_RC sdnIndexCTL::bindCTS( sdrMtx        * aMtx,
                             scSpaceID       aSpaceID,
                             UShort          aKeyOffset,
                             sdpPhyPageHdr * aSrcPage,
                             UChar           aSrcCTSlotNum,
                             sdpPhyPageHdr * aDstPage,
                             UChar           aDstCTSlotNum )
{
    sdnCTL * sDstCTL;
    sdnCTS * sDstCTS;
    sdnCTS * sSrcCTS;
    UChar    sUsedCount;
    UInt     i;

    sDstCTL       = getCTL( aDstPage );
    sDstCTS       = getCTS( aDstPage, aDstCTSlotNum );
    sSrcCTS       = getCTS( aSrcPage, aSrcCTSlotNum );

    if( sDstCTS->mState == SDN_CTS_DEAD )
    {
        idlOS::memcpy( sDstCTS, sSrcCTS, ID_SIZEOF(sdnCTS) );

        sDstCTS->mRefCnt = 1;

        idlOS::memset( sDstCTS->mRefKey,
                       0x00,
                       ID_SIZEOF(UShort) * SDN_CTS_MAX_KEY_CACHE );
        sDstCTS->mRefKey[0] = aKeyOffset;

        sUsedCount = sDstCTL->mUsedCount + 1;
        if( sUsedCount >= SDN_CTS_INFINITE )
        {
            ideLog::log( IDE_SERVER_0,
                         "Source CT slot number : %u"
                         ", Destination CT slot number : %u"
                         "\nUsed count : %u\n",
                         aSrcCTSlotNum, aDstCTSlotNum, sUsedCount );
            dumpIndexNode( aSrcPage );
            dumpIndexNode( aDstPage );
            IDE_ASSERT( 0 );
        }

        IDE_TEST( sdrMiniTrans::writeNBytes(aMtx,
                                            (UChar*)&(sDstCTL->mUsedCount),
                                            (void*)&sUsedCount,
                                            ID_SIZEOF(UChar) )
                  != IDE_SUCCESS );
    }
    else
    {
        for( i = 0; i < SDN_CTS_MAX_KEY_CACHE; i++ )
        {
            if( sDstCTS->mRefKey[i] == SDN_CTS_KEY_CACHE_NULL )
            {
                sDstCTS->mRefKey[i] = aKeyOffset;
                break;
            }
        }
        sDstCTS->mRefCnt++;
    }

    if( isMyTransaction( aMtx->mTrans,
                         aSrcPage,
                         aSrcCTSlotNum ) == ID_TRUE )
    {
        if( sDstCTS->mRefCnt == 1 )
        {
            IDE_TEST( smLayerCallback::addTouchedPage( aMtx->mTrans,
                                                       aSpaceID,
                                                       aDstPage->mPageID,
                                                       (SShort)aDstCTSlotNum )
                      != IDE_SUCCESS );
        }
    }

    IDE_TEST( sdrMiniTrans::writeLogRec(aMtx,
                                        (UChar*)sDstCTS,
                                        (void*)sDstCTS,
                                        ID_SIZEOF(sdnCTS),
                                        SDR_SDP_BINARY )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}
#if 0
/*********************************************************************
 * FUNCTION DESCRIPTION : sdnIndexCTL::bindChainedCTS                *
 * ------------------------------------------------------------------*
 * Chain�� ���� Dummy CTS�� �����.                                  *
 * �̷��� ���� SourceCTS�� Chain�� �ִ� ��쿡 ȣ��ɼ� �ִ�.    *
 *********************************************************************/
IDE_RC sdnIndexCTL::bindChainedCTS( sdrMtx        * aMtx,
                                    scSpaceID       aSpaceID,
                                    sdpPhyPageHdr * aSrcPage,
                                    UChar           aSrcCTSlotNum,
                                    sdpPhyPageHdr * aDstPage,
                                    UChar           aDstCTSlotNum )
{
    sdnCTL * sDstCTL;
    sdnCTS * sDstCTS;
    sdnCTS * sSrcCTS;
    UChar    sUsedCount;

    sDstCTL       = getCTL( aDstPage );
    sDstCTS       = getCTS( aDstPage, aDstCTSlotNum );
    sSrcCTS       = getCTS( aSrcPage, aSrcCTSlotNum );

    if( sDstCTS->mState == SDN_CTS_DEAD )
    {
        /*
         * reference count�� 0�� Dummy CTS�� �����.
         */
        idlOS::memcpy( sDstCTS, sSrcCTS, ID_SIZEOF(sdnCTS) );

        sDstCTS->mRefCnt = 0;

        idlOS::memset( sDstCTS->mRefKey,
                       0x00,
                       ID_SIZEOF(UShort) * SDN_CTS_MAX_KEY_CACHE );

        sUsedCount = sDstCTL->mUsedCount + 1;
        if( sUsedCount >= SDN_CTS_INFINITE )
        {
            ideLog::log( IDE_SERVER_0,
                         "Source CT slot number : %u"
                         ", Destination CT slot number : %u"
                         "\nUsed count : %u\n",
                         aSrcCTSlotNum, aDstCTSlotNum, sUsedCount );
            dumpIndexNode( aSrcPage );
            dumpIndexNode( aDstPage );
            IDE_ASSERT( 0 );
        }

        IDE_TEST( sdrMiniTrans::writeNBytes(aMtx,
                                            (UChar*)&(sDstCTL->mUsedCount),
                                            (void*)&sUsedCount,
                                            ID_SIZEOF(UChar) )
                  != IDE_SUCCESS );

        if( isMyTransaction( aMtx->mTrans,
                             aSrcPage,
                             aSrcCTSlotNum ) == ID_TRUE )
        {
            IDE_TEST( smLayerCallback::addTouchedPage( aMtx->mTrans,
                                                       aSpaceID,
                                                       aDstPage->mPageID,
                                                       (SShort)aDstCTSlotNum )
                      != IDE_SUCCESS );
        }
    }
    else
    {
        /*
         * Chained CTS��� Source Chain�� ������ DstCTS�� �����Ѵ�.
         */
        if( hasChainedCTS( sSrcCTS ) == ID_TRUE )
        {
            SM_SET_SCN( &sDstCTS->mNxtCommitSCN, &sSrcCTS->mNxtCommitSCN );

            sDstCTS->mUndoPID = sSrcCTS->mUndoPID;
            sDstCTS->mUndoSlotNum = sSrcCTS->mUndoSlotNum;
        }
    }

    IDE_TEST( sdrMiniTrans::writeLogRec(aMtx,
                                        (UChar*)sDstCTS,
                                        (void*)sDstCTS,
                                        ID_SIZEOF(sdnCTS),
                                        SDR_SDP_BINARY )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}
#endif
/*********************************************************************
 * FUNCTION DESCRIPTION : sdnIndexCTL::freeCTS                       *
 * ------------------------------------------------------------------*
 * CTS�� DEAD���·� �����.                                          *
 *********************************************************************/
IDE_RC sdnIndexCTL::freeCTS( sdrMtx        * aMtx,
                             sdpPhyPageHdr * aPage,
                             UChar           aSlotNum,
                             idBool          aLogging )
{
    sdnCTL * sCTL;
    sdnCTS * sCTS;

    sCTL = getCTL( aPage );
    sCTS = getCTS( aPage, aSlotNum );

    /* BUG-38304 �̹� DEAD�� ó���� CTS�� �ٽ� freeCTS�� ȣ��� ��� 
     * CTS�� CTL�� ���� ����ġ�� �߻��� �� �ִ�. */
    IDE_ASSERT( sCTS->mState != SDN_CTS_DEAD );

    setCTSlotState( sCTS, SDN_CTS_DEAD );
    sCTL->mUsedCount--;

    if( sCTL->mUsedCount >= SDN_CTS_INFINITE )
    {
        ideLog::log( IDE_SERVER_0,
                     "CT slot number : %u"
                     "\nUsed count : %u\n",
                     aSlotNum, sCTL->mUsedCount );
        dumpIndexNode( aPage );
        IDE_ASSERT( 0 );
    }

    if( aLogging == ID_TRUE )
    {
        IDE_TEST( logFreeCTS( aMtx, aPage, aSlotNum ) != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : sdnIndexCTL::freeCTS                       *
 * ------------------------------------------------------------------*
 * CTS�� DEAD���°� �Ǿ����� �α��Ѵ�.                               *
 *********************************************************************/
IDE_RC sdnIndexCTL::logFreeCTS( sdrMtx        * aMtx,
                                sdpPhyPageHdr * aPage,
                                UChar           aSlotNum )
{
    IDE_TEST( sdrMiniTrans::writeLogRec(aMtx,
                                        (UChar*)aPage,
                                        NULL,
                                        ID_SIZEOF(UChar),
                                        SDR_SDN_FREE_CTS )
              != IDE_SUCCESS );

    IDE_TEST( sdrMiniTrans::write( aMtx,
                                   (void*)&aSlotNum,
                                   ID_SIZEOF(UChar))
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : sdnIndexCTL::extend                        *
 * ------------------------------------------------------------------*
 * CTL�� ũ�⸦ 1��ŭ Ȯ���Ѵ�..                                     *
 *********************************************************************/
IDE_RC sdnIndexCTL::extend( sdrMtx        * aMtx,
                            sdpSegHandle  * aSegHandle,
                            sdpPhyPageHdr * aPage,
                            idBool          aLogging,
                            idBool        * aSuccess )
{
    sdnCTL * sCTL;
    sdnCTS * sCTS;
    UChar    sCount;
    idBool   sTrySuccess = ID_FALSE;

    // BUG-29506 TBT�� TBK�� ��ȯ�� offset�� CTS�� �ݿ����� �ʽ��ϴ�.
    // �����ϱ� ���� CTS �Ҵ� ���θ� ���Ƿ� �����ϱ� ���� PROPERTY�� �߰�
    *aSuccess = ID_FALSE;
    IDE_TEST_CONT( smuProperty::getDisableTransactionBoundInCTS() == 1,
                    RETURN_SUCCESS );

    sCTL = getCTL( aPage );

    sCount = sCTL->mCount;

    if( (aSegHandle == NULL) ||
        (sCount < aSegHandle->mSegAttr.mMaxTrans) )
    {
        IDE_ERROR( sdpPhyPage::extendCTL( aPage,
                                          1,
                                          ID_SIZEOF(sdnCTS),
                                          (UChar**)&sCTS,
                                          &sTrySuccess )
                   == IDE_SUCCESS );

        if ( sTrySuccess == ID_TRUE )
        {
            idlOS::memset( sCTS, 0x00, ID_SIZEOF(sdnCTS) );
            sCTS->mState = SDN_CTS_DEAD;

            sCTL->mCount++;

            if( aLogging == ID_TRUE )
            {
                IDE_TEST( sdrMiniTrans::writeLogRec(aMtx,
                                                    (UChar*)aPage,
                                                    NULL,
                                                    ID_SIZEOF(UChar),
                                                    SDR_SDN_EXTEND_CTL )
                          != IDE_SUCCESS );

                IDE_TEST( sdrMiniTrans::write( aMtx,
                                               (void*)&sCount,
                                               ID_SIZEOF(UChar))
                          != IDE_SUCCESS );
            }

            *aSuccess = ID_TRUE;
        }
    }

    IDE_EXCEPTION_CONT( RETURN_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : sdnIndexCTL::fastStamping                  *
 * ------------------------------------------------------------------*
 * Transaction Commit�ÿ� ȣ��Ǹ�, CTS�� CommitSCN�� �����ϰ� ����  *
 * �� STAMPED�� �����Ѵ�.                                            *
 *********************************************************************/
IDE_RC sdnIndexCTL::fastStamping( void   * aTrans,
                                  UChar  * aPage,
                                  UChar    aSlotNum,
                                  smSCN  * aCommitSCN )
{
    sdnCTS    * sCTS;

    // BUG-29442: stamping ��� Page�� free�� �� ����� ���
    //            �߸��� Stamping�� ������ �� �ִ�.

    IDE_TEST_CONT( aSlotNum >= getCount( (sdpPhyPageHdr*)aPage ),
                    SKIP_STAMPING );

    IDE_TEST_CONT( ((sdpPhyPageHdr*)aPage)->mSizeOfCTL == 0,
                    SKIP_STAMPING );

    sCTS = getCTS( (sdpPhyPageHdr*)aPage, aSlotNum );

    if( getCTSlotState( sCTS ) == SDN_CTS_UNCOMMITTED )
    {
        if( isMyTransaction( aTrans,
                             (sdpPhyPageHdr*)aPage,
                             aSlotNum ) == ID_TRUE )
        {
            sCTS->mCommitSCN = *aCommitSCN;
            sCTS->mState = SDN_CTS_STAMPED;
        }
    }

    IDE_EXCEPTION_CONT( SKIP_STAMPING );

    return IDE_SUCCESS;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : sdnIndexCTL::fastStampingAll               
 * -------------------------------------------------------------------
 * Description : DRDBRedo Valdation�� ���� FastStamping�� �õ��մϴ�.
 *
 * 1) Stamping�� �ߴ��� ���� �ʾҴ��Ŀ� ���� SCN�� �ٸ� �� �ֱ� �����̴ϴ�.
 * 2) Stamping �Ǿ�� �ٽ� Stamping�� �õ��մϴ�. TSS�� ������ ��Ȱ��
 *    �Ǿ� CommitSCN�� 0�� ���� �ֱ� �����Դϴ�.
 * 3) �� �������� ���ÿ� Stamping�մϴ�. �ι��� ���� Stamping�ϸ�
 *    getCommitSCN�� Ÿ�ֿ̹� ����, ������ Stamping �Ǹ鼭 �ٸ� ������
 *    Stamping �ȵ� ���� �ֱ� �����Դϴ�.
 *
 * aStatistics    - [IN] Dummy�������
 * aPagePtr1      - [IN] ������ ��� ���� ������
 * aPagePtr2      - [IN] ������ ��� ���� ������
 *********************************************************************/
IDE_RC sdnIndexCTL::stampingAll4RedoValidation( idvSQL * aStatistics,
                                                UChar  * aPage1,
                                                UChar  * aPage2 )
{
    sdnCTL *  sCTL1;
    sdnCTS *  sCTS1;
    sdnCTL *  sCTL2;
    sdnCTS *  sCTS2;
    sdSID     sTSSlotSID;
    smTID     sDummyTID4Wait;
    smSCN     sCommitSCN;
    UInt      i;

    /* Internal Node���� CTS�� ������, Ÿ ��⿡���� �̸� ������ �� ����.
     * ���� Index ���ο��� �����ؼ�, CTS�� �ִ� ��쿡�� ó���� */
    IDE_TEST_CONT( sdpPhyPage::getSizeOfCTL( ((sdpPhyPageHdr*)aPage1) ) == 0,
                    SKIP );

    sCTL1 = sdnIndexCTL::getCTL( (sdpPhyPageHdr*) aPage1);
    sCTL2 = sdnIndexCTL::getCTL( (sdpPhyPageHdr*) aPage2);

    IDE_ERROR( sdnIndexCTL::getCount(sCTL1) == sdnIndexCTL::getCount(sCTL2) );

    for( i = 0; i < sdnIndexCTL::getCount(sCTL1); i++ )
    {
        sCTS1 = sdnIndexCTL::getCTS( sCTL1, i );
        sCTS2 = sdnIndexCTL::getCTS( sCTL2, i );

        /* Stamping �� ���¶�, �ٽ� Stampnig�� �մϴ�.
         * �ֳ��ϸ� TSS�� ������ ��Ȱ��Ǿ� CommtSCN�� 0�� ������ ����
         * �ֱ� �����Դϴ�. */
        if( ( sdnIndexCTL::getCTSlotState( sCTS1 ) == SDN_CTS_UNCOMMITTED ) ||
            ( sdnIndexCTL::getCTSlotState( sCTS1 ) == SDN_CTS_STAMPED ) )
        {
            IDE_ERROR( ( sdnIndexCTL::getCTSlotState( sCTS2 ) == SDN_CTS_UNCOMMITTED ) ||
                       ( sdnIndexCTL::getCTSlotState( sCTS2 ) == SDN_CTS_STAMPED ) );

            sTSSlotSID = SD_MAKE_SID( sCTS1->mTSSlotPID, sCTS1->mTSSlotNum );

            IDE_ERROR( sTSSlotSID == 
                       SD_MAKE_SID( sCTS2->mTSSlotPID, sCTS2->mTSSlotNum ) );

            IDE_TEST( sdcTSSegment::getCommitSCN( aStatistics,
                                                  NULL,        /* aTrans */
                                                  sTSSlotSID,
                                                  &sCTS1->mCommitSCN,
                                                  SM_SCN_INIT, /* aStmtViewSCN */
                                                  &sDummyTID4Wait, 
                                                  &sCommitSCN )
                      != IDE_SUCCESS );
            if( SM_SCN_IS_NOT_INFINITE( sCommitSCN ) )
            {
                sCTS1->mCommitSCN = sCommitSCN;
                sCTS1->mState     = SDN_CTS_STAMPED;
                sCTS2->mCommitSCN = sCommitSCN;
                sCTS2->mState     = SDN_CTS_STAMPED;
            }
        }
        else
        {
            /* ���ʿ��� Diff�� ������ �ʱ�ȭ�Ѵ�. */
            IDE_ERROR( ( sdnIndexCTL::getCTSlotState( sCTS1 ) == SDN_CTS_NONE ) || 
                       ( sdnIndexCTL::getCTSlotState( sCTS1 ) == SDN_CTS_DEAD ) );
            IDE_ERROR( ( sdnIndexCTL::getCTSlotState( sCTS2 ) == SDN_CTS_NONE ) || 
                       ( sdnIndexCTL::getCTSlotState( sCTS2 ) == SDN_CTS_DEAD ) );

            idlOS::memset( sCTS1, 0, ID_SIZEOF( sdnCTS ) );
            sCTS1->mState  = SDN_CTS_DEAD;
            idlOS::memset( sCTS2, 0, ID_SIZEOF( sdnCTS ) );
            sCTS2->mState  = SDN_CTS_DEAD;
        }
    }

    IDE_EXCEPTION_CONT( SKIP );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : sdnIndexCTL::delayedStamping               *
 * ------------------------------------------------------------------*
 * �ٸ� Transaction(SELECT/DML)�� ���ؼ� ����Ǹ�, CTS�� CommitSCN�� *
 * �����ϰ� ���� �� STAMPED�� �����Ѵ�.                              *
 *                                                                   *
 * BUG-31372: ���׸�Ʈ �ǻ��� ������ ��ȸ�� ����� �ʿ��մϴ�      *
 *            Aging �ÿ� MPR�� �̿��ϱ� ������ Page Read Mode�� �ʿ� * 
 *********************************************************************/
IDE_RC sdnIndexCTL::delayedStamping( idvSQL          * aStatistics,
                                     void            * aTrans,
                                     sdnCTS          * aCTS,
                                     sdbPageReadMode   aPageReadMode,
                                     smSCN             aStmtViewSCN,
                                     smSCN           * aCommitSCN,
                                     idBool          * aSuccess )
{
    sdSID         sTSSlotSID;
    smTID         sDummyTID4Wait;
    UChar       * sPageHdr;
    idBool        sTrySuccess = ID_FALSE;

    IDE_DASSERT( aCTS->mState == SDN_CTS_UNCOMMITTED );
     
    sTSSlotSID = SD_MAKE_SID( aCTS->mTSSlotPID, aCTS->mTSSlotNum );

    IDE_TEST( sdcTSSegment::getCommitSCN( aStatistics,
                                          (smxTrans*)aTrans,
                                          sTSSlotSID,
                                          &aCTS->mCommitSCN,
                                          aStmtViewSCN,
                                          &sDummyTID4Wait, 
                                          aCommitSCN )
              != IDE_SUCCESS );

    if( SM_SCN_IS_NOT_INFINITE( *aCommitSCN ) )
    {
        sPageHdr = sdpPhyPage::getPageStartPtr( aCTS );

        sdbBufferMgr::tryEscalateLatchPage( aStatistics,
                                            sPageHdr,
                                            aPageReadMode,
                                            &sTrySuccess );

        if( sTrySuccess == ID_TRUE )
        {
            sdbBufferMgr::setDirtyPageToBCB( aStatistics,
                                             sPageHdr );
            aCTS->mState = SDN_CTS_STAMPED;
            aCTS->mCommitSCN = *aCommitSCN;
            *aSuccess = ID_TRUE;
        }
        else
        {
            *aSuccess = ID_FALSE;
        }
    }
    else
    {
        *aSuccess = ID_FALSE;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : sdnIndexCTL::getCTL                        *
 * ------------------------------------------------------------------*
 * CTL Header�� ��´�.                                              *
 *********************************************************************/
sdnCTL* sdnIndexCTL::getCTL( sdpPhyPageHdr  * aPage )
{
    return (sdnCTL*)sdpPhyPage::getCTLStartPtr( (UChar*)aPage );
}

/*********************************************************************
 * FUNCTION DESCRIPTION : sdnIndexCTL::getCTS                        *
 * ------------------------------------------------------------------*
 * CTS Pointer�� ��´�.                                             *
 *********************************************************************/
sdnCTS* sdnIndexCTL::getCTS( sdnCTL  * aCTL,
                             UChar     aSlotNum )
{
    IDE_DASSERT( aSlotNum < aCTL->mCount );

    return &(aCTL->mArrCTS[ aSlotNum ]);
}

/*********************************************************************
 * FUNCTION DESCRIPTION : sdnIndexCTL::getCTS                        *
 * ------------------------------------------------------------------*
 * CTS Pointer�� ��´�.                                             *
 *********************************************************************/
sdnCTS* sdnIndexCTL::getCTS( sdpPhyPageHdr  * aPage,
                             UChar            aSlotNum )
{
    return getCTS( getCTL(aPage), aSlotNum );
}

/*********************************************************************
 * FUNCTION DESCRIPTION : sdnIndexCTL::getCount                      *
 * ------------------------------------------------------------------*
 * CTL�� ũ�⸦ �����Ѵ�.                                            *
 *********************************************************************/
UChar sdnIndexCTL::getCount( sdnCTL  * aCTL )
{
    return aCTL->mCount;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : sdnIndexCTL::getCount                      *
 * ------------------------------------------------------------------*
 * CTL�� ũ�⸦ �����Ѵ�.                                            *
 *********************************************************************/
UChar sdnIndexCTL::getCount( sdpPhyPageHdr  * aPage )
{
    return getCount( getCTL(aPage) );
}

/*********************************************************************
 * FUNCTION DESCRIPTION : sdnIndexCTL::getCount                      *
 * ------------------------------------------------------------------*
 * Active CTS�� ������ �����Ѵ�.                                     *
 *********************************************************************/
UChar sdnIndexCTL::getUsedCount( sdnCTL  * aCTL )
{
    return aCTL->mUsedCount;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : sdnIndexCTL::getCount                      *
 * ------------------------------------------------------------------*
 * Active CTS�� ������ �����Ѵ�.                                     *
 *********************************************************************/
UChar sdnIndexCTL::getUsedCount( sdpPhyPageHdr  * aPage )
{
    return getUsedCount( getCTL(aPage) );
}

/*********************************************************************
 * FUNCTION DESCRIPTION : sdnIndexCTL::getCTSlotState                *
 * ------------------------------------------------------------------*
 * CTS�� ���¸� �����Ѵ�.                                            *
 *********************************************************************/
UChar sdnIndexCTL::getCTSlotState( sdpPhyPageHdr * aPage,
                                   UChar           aSlotNum )
{
    sdnCTS * sCTS;

    sCTS = getCTS( aPage, aSlotNum );

    return getCTSlotState( sCTS );
}

/*********************************************************************
 * FUNCTION DESCRIPTION : sdnIndexCTL::getCTSlotState                *
 * ------------------------------------------------------------------*
 * CTS�� ���¸� �����Ѵ�.                                            *
 *********************************************************************/
UChar sdnIndexCTL::getCTSlotState( sdnCTS  * aCTS )
{
    return aCTS->mState;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : sdnIndexCTL::setCTSlotState                *
 * ------------------------------------------------------------------*
 * CTS�� ���¸� �����Ѵ�.                                            *
 *********************************************************************/
void sdnIndexCTL::setCTSlotState( sdnCTS * aCTS,
                                  UChar    aState )
{
    aCTS->mState = aState;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : sdnIndexCTL::getCommitSCN                  *
 * ------------------------------------------------------------------*
 * CTS.CommitSCN�� �����Ѵ�.                                         *
 *********************************************************************/
smSCN sdnIndexCTL::getCommitSCN( sdnCTS  * aCTS )
{
    return (aCTS->mCommitSCN);
}

/*********************************************************************
 * FUNCTION DESCRIPTION : sdnIndexCTL::getCommitSCN                  *
 * ------------------------------------------------------------------*
 * Transaction�� CommitSCN�� ���Ѵ�.                                 *
 * UNCOMMITTED ���¶�� Delayed Stamping�� ���ؼ� CommitSCN�� ���� *
 * �Ѵ�.                                                             *
 *********************************************************************/
IDE_RC sdnIndexCTL::getCommitSCN( idvSQL           * aStatistics,
                                  void             * aTrans,
                                  sdpPhyPageHdr    * aPage,
                                  sdbPageReadMode    aPageReadMode,
                                  UChar              aCTSlotNum,
                                  smSCN              aStmtViewSCN,
                                  smSCN            * aCommitSCN )
{
    sdnCTS    * sCTS;
    idBool      sSuccess;

    sCTS = getCTS( aPage, aCTSlotNum );

    if ( sCTS->mState == SDN_CTS_STAMPED )
    {
        SM_GET_SCN(aCommitSCN, &sCTS->mCommitSCN);
    }
    else
    {
        IDE_TEST( delayedStamping( aStatistics,
                                   aTrans,
                                   sCTS,
                                   aPageReadMode,
                                   aStmtViewSCN,
                                   aCommitSCN,
                                   &sSuccess )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}
#if 0
/*********************************************************************
 * FUNCTION DESCRIPTION : sdnIndexCTL::getCommitSCNfromUndo          *
 * ------------------------------------------------------------------*
 * Undo Chain�� ����Ǿ� �ִ� CommitSCN�� ���Ѵ�. Key�� ����Ǿ�   *
 * �ִ� CTS�� ã�ƾ� �ϰ�,                                           *
 * �׷��� CTS�� [ROW_PID, ROW_SLOTNUM, KEY_VALUE, WHICH_CTS]��       *
 * ������ ã�´�. ���� [ROW_PID, ROW_SLOTNUM, KEY_VALUE]�� ���� Ű�� *
 * Chain�� 2��(Create/Limit)�� �����Ҽ� �ֱ� ������ WHICH_CTS��    *
 * ������ Create���� Limit���� Ȯ���ؾ� �Ѵ�.                        *
 *********************************************************************/
IDE_RC sdnIndexCTL::getCommitSCNfromUndo( idvSQL           * aStatistics,
                                          sdpPhyPageHdr    * aPage,
                                          sdnCTS           * aCTS,
                                          UChar              aCTSlotNum,
                                          smSCN            * aStmtViewSCN,
                                          sdnCallbackFuncs * aCallbackFunc,
                                          UChar            * aContext,
                                          idBool             aIsCreateSCN,
                                          smSCN            * aCommitSCN )
{
    sdnCTS   sCTS;
    UChar    sChainedCCTS   = SDN_CHAINED_NO;
    UChar    sChainedLCTS   = SDN_CHAINED_NO;
    UShort   sKeyListSize;
    ULong    sKeyList[SD_PAGE_SIZE / ID_SIZEOF(ULong)];
    idBool   sIsReusedUndoRec;

    idlOS::memcpy( &sCTS, aCTS, ID_SIZEOF( sdnCTS ) );

    while( 1 )
    {
        if( hasChainedCTS( &sCTS ) != ID_TRUE )
        {
            SM_INIT_SCN( aCommitSCN );
            break;
        }

        /* FIT/ART/sm/Bugs/BUG-29839/BUG-29839.sql */
        IDU_FIT_POINT( "1.BUG-29839@sdnIndexCTL::getCommitSCNfromUndo" );

        IDE_TEST( getChainedCTS( aStatistics,
                                 NULL, //aMtx
                                 aPage,
                                 aCallbackFunc,
                                 aContext,
                                 &(sCTS.mNxtCommitSCN),
                                 aCTSlotNum,
                                 ID_FALSE, // Try Hard Stamping
                                 &sCTS,
                                 (UChar*)sKeyList,
                                 &sKeyListSize,
                                 &sIsReusedUndoRec ) != IDE_SUCCESS );

        /* BUG-29839 BTree���� ����� undo page�� ������ �� �ֽ��ϴ�.
         * chain�� CTS�� commit SCN�� ���ϴ� �������� �ٸ� tx�� ����
         * chain�� CTS�� undo page�� ����Ǿ��� �� �ֽ��ϴ�.
         * �̷� ���� ������ �����ϱ� ���� ������ ���� CTS�� mNxtCommitSCN
         * ���� ������ sysMinDskSCN���� ū ��쿡�� ������ INIT_SCN ����
         * �Ǵ��Ͽ� �������� ���� ������ ���� �ʵ��� �մϴ�.
         * �̴� �������� ���� S Latch�� ���� �Ŀ� ���Ͽ��� �մϴ�.*/

        if( sIsReusedUndoRec == ID_TRUE )
        {
            SM_INIT_SCN( aCommitSCN );
            break;
        }

        if( aCallbackFunc->mFindChainedKey( NULL,
                                            &sCTS,
                                            (UChar*)sKeyList,
                                            sKeyListSize,
                                            &sChainedCCTS,
                                            &sChainedLCTS,
                                            aContext ) == ID_TRUE )
        {
            /*
             * Chain�󿡴� ���� [ROW_PID, ROW_SLOTNUM, KEY_VALUE]�� ���� Key����
             * �����Ҽ� �ֱ� ������ WHICH_CHAIN������ �����ؾ� �Ѵ�.
             * ���� CreateCTS�� ���� Chain�̶�� �ݵ�� ChainedCCTS��
             * SDN_CHAINED_YES�� �����Ǿ� �־�߸� �Ѵ�.
             */
            if ( ((aIsCreateSCN == ID_TRUE) &&
                  (sChainedCCTS == SDN_CHAINED_YES)) ||
                 ((aIsCreateSCN == ID_FALSE) &&
                  (sChainedLCTS == SDN_CHAINED_YES)) )
            {
                *aCommitSCN = sCTS.mCommitSCN;
                break;
            }
        }
        else
        {
            if( SM_SCN_IS_LT( &sCTS.mNxtCommitSCN, aStmtViewSCN ) )
            {
                *aCommitSCN = sCTS.mNxtCommitSCN;
                break;
            }
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}
#endif
/*********************************************************************
 * FUNCTION DESCRIPTION : sdnIndexCTL::getCommitSCN                  *
 * ------------------------------------------------------------------*
 * CTS.CommitSCN�� �����Ѵ�.                                         *
 *********************************************************************/
smSCN sdnIndexCTL::getCommitSCN( sdpPhyPageHdr * aPage,
                                 UChar           aSlotNum )
{
    sdnCTS * sCTS;

    sCTS = getCTS( aPage, aSlotNum );

    return getCommitSCN( sCTS );
}

/*********************************************************************
 * FUNCTION DESCRIPTION : sdnIndexCTL::unbindCTS                     *
 * ------------------------------------------------------------------*
 * Key�� ����Ű�� Transaction������ ���������� ���ҽ�Ų��. ���� ���� *
 * ������ 0�� �Ǵ� ���� CTS�� DEAD���·� �����.
 *********************************************************************/
IDE_RC sdnIndexCTL::unbindCTS( sdrMtx           * aMtx,
                               sdpPhyPageHdr    * aPage,
                               UChar              aSlotNum,
                               UShort             aKeyOffset )
{
    sdnCTS  * sCTS;
    UShort    sRefCnt;
    SInt      i;

    sCTS = getCTS( aPage, aSlotNum );
    sRefCnt = sCTS->mRefCnt - 1;

    if ( sRefCnt == 0 )
    {
        IDE_TEST( freeCTS( aMtx,
                           aPage,
                           aSlotNum,
                           ID_TRUE )
                  != IDE_SUCCESS );

        IDE_CONT( SKIP_ADJUST_REFKEY );
    }

    for( i = SDN_CTS_MAX_KEY_CACHE - 1; i >= 0 ; i-- )
    {
        if( sCTS->mRefKey[i] == aKeyOffset )
        {
            sCTS->mRefKey[i] = SDN_CTS_KEY_CACHE_NULL;
            break;
        }
    }

    IDE_TEST( sdrMiniTrans::writeNBytes( aMtx,
                                         (UChar*)&(sCTS->mRefCnt),
                                         &sRefCnt,
                                         ID_SIZEOF(UShort) )
              != IDE_SUCCESS );

    /* BUG-48064
       writeNBytes() �Լ��� 1bytes,2bytes,4bytes,8bytes�� ���̷� �޴´�.
       UChar mRefKey[3]�� �����ϱ����� 4bytes, 2bytes ���̷� �ι� ȣ���Ѵ�.
       ����, SDN_CTS_MAX_KEY_CACHE �� �����ϰ� �Ǹ� �̰��� �����ؾ��Ѵ�.
     */
    IDE_TEST( sdrMiniTrans::writeNBytes( aMtx,
                                         (UChar*)sCTS->mRefKey,
                                         (UChar*)sCTS->mRefKey,
                                         ID_SIZEOF(UShort)*2 )
              != IDE_SUCCESS );
    IDE_TEST( sdrMiniTrans::writeNBytes( aMtx,
                                         (UChar*)((sCTS->mRefKey) + 2),
                                         (UChar*)((sCTS->mRefKey) + 2),
                                         ID_SIZEOF(UShort) )
              != IDE_SUCCESS );

    IDE_EXCEPTION_CONT( SKIP_ADJUST_REFKEY );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}
#if 0
/*********************************************************************
 * FUNCTION DESCRIPTION : sdnIndexCTL::makeChainedCTS                *
 * ------------------------------------------------------------------*
 * Unchained Key�� Chained Key�� �����, �ش� Ű ����Ʈ�� undo page  *
 * �� ����Ѵ�.                                                      *
 * Undo Format ==> | sdnCTS | KeyListSize | KeyList(Key1,Key2,...) | *
 *********************************************************************/
IDE_RC sdnIndexCTL::makeChainedCTS( idvSQL           * aStatistics,
                                    sdrMtx           * aMtx,
                                    sdpPhyPageHdr    * aPage,
                                    UChar              aCTSlotNum,
                                    sdnCallbackFuncs * aCallbackFunc,
                                    UChar            * aContext,
                                    sdSID            * aRollbackSID )
{
    ULong            sTempBuf[SD_PAGE_SIZE / ID_SIZEOF(ULong)];
    UChar          * sKeyList;
    sdcUndoSegment * sUndoSegment;
    sdrMtxStartInfo  sStartInfo;
    UShort         * sKeyListSize;
    sdnCTS         * sCTS;
    UShort           sChainedKeyCount = 0;
    idBool           sMadeChainedKeys = ID_FALSE;
    UShort           sUnchainedKeyCount = 0;
    UChar          * sUnchainedKey = NULL;
    UInt             sUnchainedKeySize = 0;
    UChar          * sSlotDirPtr;
    UShort           sSlotCount;

    sStartInfo.mTrans   = aMtx->mTrans;
    sStartInfo.mLogMode = SDR_MTX_LOGGING;

    sCTS = getCTS( aPage, aCTSlotNum );

    idlOS::memcpy( sTempBuf, sCTS, ID_SIZEOF( sdnCTS ) );
    sKeyListSize = (UShort*)(((UChar*)sTempBuf) + ID_SIZEOF(sdnCTS));
    sKeyList = ((UChar*)sTempBuf) + ID_SIZEOF(sdnCTS) + ID_SIZEOF(ULong);

    IDE_TEST( aCallbackFunc->mMakeChainedKeys( aPage,
                                               aCTSlotNum,
                                               aContext,
                                               sKeyList,
                                               sKeyListSize,
                                               &sChainedKeyCount )
              != IDE_SUCCESS );
    sMadeChainedKeys = ID_TRUE;

    sUndoSegment = smLayerCallback::getUDSegPtr( (smxTrans*)aMtx->mTrans );

    /*
     * aTableOID�� NULL_OID�� �����Ͽ� Cursor Close�� Index ó����
     * Skip �ϰ� �Ѵ�.
     */
    if ( sUndoSegment->addIndexCTSlotUndoRec(
                          aStatistics,
                          &sStartInfo,
                          SM_NULL_OID, /* aTableOID */
                          (UChar*)sTempBuf,
                          ID_SIZEOF(sdnCTS) + ID_SIZEOF(ULong) + *sKeyListSize,
                          aRollbackSID ) != IDE_SUCCESS )
    {
        //BUG-47715: setIgnoreMtxUndo ���� 
        IDE_TEST( 1 );
    }

    IDE_TEST( aCallbackFunc->mWriteChainedKeysLog( aMtx,
                                                   aPage,
                                                   aCTSlotNum )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    /*
     * Undo Page Full�� ���ؼ� ���̻� ������ ���� ���
     */
    if( sMadeChainedKeys == ID_TRUE )
    {
        cleanRefInfo( aPage, aCTSlotNum );

        sSlotDirPtr = sdpPhyPage::getSlotDirStartPtr( (UChar*)aPage );
        sSlotCount  = sdpSlotDirectory::getCount( sSlotDirPtr );

        /* SlotNum(UShort) + CreateCTS(UChar) + LimitCTS(UChar) */
        if ( iduMemMgr::malloc( IDU_MEM_SM_SDN,
                                ID_SIZEOF(UChar) * 4 * sSlotCount,
                                (void**)&sUnchainedKey,
                                IDU_MEM_IMMEDIATE ) == IDE_SUCCESS )
        {

            (void)aCallbackFunc->mMakeUnchainedKeys( NULL,
                                                     aPage,
                                                     sCTS,
                                                     aCTSlotNum,
                                                     sKeyList,
                                                     *sKeyListSize,
                                                     aContext,
                                                     sUnchainedKey,
                                                     &sUnchainedKeySize,
                                                     &sUnchainedKeyCount );

            (void)iduMemMgr::free( sUnchainedKey );
        }
    }

    return IDE_FAILURE;
}
#endif
/*********************************************************************
 * FUNCTION DESCRIPTION : sdnIndexCTL::canAllocCTS                   *
 * ------------------------------------------------------------------*
 * SMO�ÿ� ȣ��Ǹ�, CTS������ �Ҵ������ ������ �˻��ϴ� �Լ��̴�.  *
 *********************************************************************/
idBool sdnIndexCTL::canAllocCTS( sdpPhyPageHdr * aPage,
                                 UChar           aNeedCount )
{
    sdnCTL * sCTL;

    sCTL = getCTL( aPage );

    if( getCount( sCTL ) >= (sCTL->mUsedCount + aNeedCount) )
    {
        return ID_TRUE;
    }

    return ID_FALSE;
}

#if 0
/*********************************************************************
 * FUNCTION DESCRIPTION : sdnIndexCTL::unchaining                    *
 * ------------------------------------------------------------------*
 * Undo page�� ����Ǿ� �־��� Chained Key�� Unchained Key�� �����. *
 * �ش� �������� chaining��ÿ��� �־����� unchaining�� �Ǳ� ����    *
 * SMO�� ���ؼ� �ٸ� �������� �̵��� ��쵵 �ִ�. ����, Unchained  *
 * key count�� 0�� ��� ����� ������, ���� chain�� ������ ���� ���� *
 * CTS��� DEAD���·� �����.                                        *
 *********************************************************************/
IDE_RC sdnIndexCTL::unchainingCTS( idvSQL           * aStatistics,
                                   sdrMtx           * aMtx,
                                   sdpPhyPageHdr    * aPage,
                                   sdnCTS           * aCTS,
                                   UChar              aCTSlotNum,
                                   sdnCallbackFuncs * aCallbackFunc,
                                   UChar            * aContext )
{
    UShort   sUnchainedKeyCount = 0;
    UShort   sChainedKeySize;
    ULong    sKeyList[SD_PAGE_SIZE / ID_SIZEOF(ULong)];
    idBool   sIsReusedUndoRec;

    IDE_TEST( getChainedCTS( aStatistics,
                             aMtx,
                             aPage,
                             aCallbackFunc,
                             aContext,
                             &(aCTS->mCommitSCN),
                             aCTSlotNum,
                             ID_TRUE, // Try Hard Stamping
                             aCTS,
                             (UChar*)sKeyList,
                             &sChainedKeySize,
                             &sIsReusedUndoRec ) != IDE_SUCCESS );

    if( sIsReusedUndoRec == ID_FALSE )
    {
        /*
         * Chaining���� ���Ŀ� Compact�Ǿ������� �ֱ� ������
         * Reference Info�� reset�ϰ� MakeUnchainedKey���� �ٽ� �����Ѵ�.
         */
        cleanRefInfo( aPage, aCTSlotNum );

        IDE_TEST( aCallbackFunc->mLogAndMakeUnchainedKeys( NULL,
                                                           aMtx,
                                                           aPage,
                                                           aCTS,
                                                           aCTSlotNum,
                                                           (UChar*)sKeyList,
                                                           sChainedKeySize,
                                                           &sUnchainedKeyCount,
                                                           aContext )
                  != IDE_SUCCESS );

        /*
         * Unchaining�� Ű�� �������� �ʴ´ٸ� SMO�� ���Ͽ�
         * ��� Ű�� �̵��� ����̴�. �ٷ� �����Ѵ�.
         */
        if( (sUnchainedKeyCount == 0) &&
            (hasChainedCTS( aCTS ) == ID_FALSE) )
        {
            // BUG-28010 mUsedCount Mismatch
            // Do Logging just below this block
            /* BUG-32118 [sm-disk-index] Do not logging FreeCTS if do
             * makeUnchainedKey operation. */
            IDE_TEST( freeCTS( aMtx,
                               aPage,
                               aCTSlotNum,
                               ID_TRUE ) // logging
                      != IDE_SUCCESS );
        }

        IDE_TEST( sdrMiniTrans::writeLogRec( aMtx,
                                             (UChar*)aCTS,
                                             (void*)aCTS,
                                             ID_SIZEOF(sdnCTS),
                                             SDR_SDP_BINARY)
                  != IDE_SUCCESS);
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}
#endif
#if 0
/*********************************************************************
 * FUNCTION DESCRIPTION : sdnIndexCTL::getChainedCTS                 *
 * ------------------------------------------------------------------*
 * BUG-29839,34499 BTree���� ����� undo page�� ������ �� �ֽ��ϴ�.*
 * chain�� CTS�� commit SCN�� ���ϴ� �������� �ٸ� tx�� ����         *
 * chain�� CTS�� undo page�� ����Ǿ��� �� �ֽ��ϴ�.               *
 * �̷� ���� ������ �����ϱ� ���� ������ ���� CTS�� mNxtCommitSCN    *
 * ���� ������ sysMinDskSCN���� ū ��쿡�� ������ ��Ȱ������      *
 * �Ǵ��Ͽ� �������� ���� ������ ���� �ʵ��� �մϴ�.                 *
 * �̴� �������� ���� S Latch�� ���� �Ŀ� ���Ͽ��� �մϴ�.         *
 *
 *  aStatistics      - [IN] �������
 *  aMtx             - [IN] Mini transaction,
 *  aPage            - [IN] CTS�� ���Ե� page�� pointer
 *  aCallbackFunc    - [IN] callback func
 *  aContext         - [IN] context
 *  aCommitSCN       - [IN] undo page ��Ȱ���� Ȯ�� �� commit scn
 *  aCTSlotNum       - [IN] CTS�� slot number
 *  aCTS             - [IN/OUT] CTS�� �޾Ƽ� Chained CTS�� ��ȯ
 *  aKeyList         - [OUT] Key List
 *  aKeyListSize     - [OUT] Key List�� ũ��
 *  aIsReusedUndoRec - [OUT] Undo Page�� ���� ����
 *********************************************************************/
IDE_RC sdnIndexCTL::getChainedCTS( idvSQL           * aStatistics,
                                   sdrMtx           * aMtx,
                                   sdpPhyPageHdr    * aPage,
                                   sdnCallbackFuncs * aCallbackFunc,
                                   UChar            * aContext,
                                   smSCN            * aCommitSCN,
                                   UChar              aCTSlotNum,
                                   idBool             aTryHardKeyStamping,
                                   sdnCTS           * aCTS,
                                   UChar            * aKeyList,
                                   UShort           * aKeyListSize,
                                   idBool           * aIsReusedUndoRec )
{
    UChar         * sPagePtr;
    UChar         * sUNDOREC_HDR;
    UInt            sLatchState  = 0;
    UShort          sKeyListSize = 0;
    idBool          sIsReusedUndoRec = ID_TRUE;
    idBool          sIsSuccess;
    sdnCTS        * sCTS;
    sdnCTS          sChainedCTS;
    sdpPageType     sPageType;
    sdpSlotDirHdr * sSlotDirHdr = NULL;
    sdcUndoRecType  sType;
    smSCN           sSysMinDskSCN = SM_SCN_INIT;
    smSCN           sChainedCTSCommitSCN = SM_SCN_INIT;

    /* ���� ������ CTS�� mCommitSCN�� ���Ѵ��� ��찡 ���ٰ�
     * �Ǵ��Ͽ���. ���� ������ Ʋ������ �ٽ� �����ؾ� �Ѵ�.*/
    IDE_TEST_RAISE( SM_SCN_IS_INFINITE( *aCommitSCN ),
                    ERR_INFINITE_CTS_COMMIT_SCN );

    smLayerCallback::getSysMinDskViewSCN( &sSysMinDskSCN );

    /* BUG-38962
     * sSysMinDskSCN�� �ʱⰪ�̸�, restart recovery ���� ���� �ǹ��Ѵ�. */
    if( SM_SCN_IS_LT( aCommitSCN, &sSysMinDskSCN ) ||
        SM_SCN_IS_INIT( sSysMinDskSCN ) )
    {
        /* getCommitSCN��� ����ϴ� ���
         * hardkeystamping�� ���� �ʰ� �ٷ� ����������.*/
        IDE_TEST_CONT( aTryHardKeyStamping == ID_FALSE, CONT_REUSE_UNDO_PAGE );

        IDE_ASSERT( aMtx != NULL );

        /* ��Ȱ������ �Ǵ� �Ǹ� �ٷ� hard key stamping �մϴ�.*/
        IDE_TEST( aCallbackFunc->mHardKeyStamping( aStatistics,
                                                   aMtx,
                                                   aPage,
                                                   aCTSlotNum,
                                                   aContext,
                                                   &sIsSuccess )
                  != IDE_SUCCESS );

        /* hard key stamping�� ������ ��� ��Ȱ�� �� ����̴�.*/

        /* BUG-44919 ������ ��ġ�� escalate�� ������ ��쿡�� hardkeystamping�� �����մϴ�.
         *           �� ��쿡�� undo page�� ��Ȱ�� �Ǿ��� ���ɼ��� �����Ƿ�
         *           hardkeystamping�� ���� ���ο� ������� ������������ �մϴ�. */
    }
    else
    {
        IDE_TEST( sdbBufferMgr::getPage( aStatistics,
                                         SMI_ID_TABLESPACE_SYSTEM_DISK_UNDO,
                                         aCTS->mUndoPID,
                                         SDB_S_LATCH,
                                         SDB_WAIT_NORMAL,
                                         SDB_SINGLE_PAGE_READ,
                                         &sPagePtr,
                                         &sIsSuccess ) != IDE_SUCCESS );
        sLatchState = 1;

        sIsReusedUndoRec = ID_FALSE;

        /* ���ϴ� undo page�� ��Ȱ�� ���� ���� ����̴�
         * �ݵ�� ���� �� �־�� �Ѵ�.*/
        sPageType = sdpPhyPage::getPageType( (sdpPhyPageHdr*)sPagePtr );

        IDE_TEST_RAISE( sPageType != SDP_PAGE_UNDO, ERR_INVALID_PAGE_TYPE );

        sSlotDirHdr = (sdpSlotDirHdr*)sdpPhyPage::getSlotDirStartPtr( sPagePtr );

        IDE_TEST_RAISE( aCTS->mUndoSlotNum >= sSlotDirHdr->mSlotEntryCount,
                        ERR_INVALID_UNDO_SLOT_NUM );

        IDE_TEST( sdpSlotDirectory::getPagePtrFromSlotNum( (UChar*)sSlotDirHdr,
                                                           aCTS->mUndoSlotNum,  
                                                           &sUNDOREC_HDR )
                  != IDE_SUCCESS );

        SDC_GET_UNDOREC_HDR_1B_FIELD( sUNDOREC_HDR,
                                      SDC_UNDOREC_HDR_TYPE,
                                      &sType );

        IDE_TEST_RAISE( sType != SDC_UNDO_INDEX_CTS, ERR_INVALID_UNDO_REC_TYPE );

        sCTS = (sdnCTS*)(sUNDOREC_HDR + SDC_UNDOREC_HDR_SIZE);

        /* BUG-34542 Undo Record�� CTS�� Align�� ���� �ʽ��ϴ�.
         * Commit SCN�� ���ϱ� ���� memcpy�ؿ;� �մϴ�.*/
        idlOS::memcpy( (UChar*)&sChainedCTSCommitSCN,
                       (UChar*)sCTS,
                       ID_SIZEOF( smSCN ) );

        IDU_FIT_POINT_RAISE( "BUG-45303@sdnIndexCTL::getCommitSCN", ERR_INVALID_CTS_COMMIT_SCN );

        /* NxtCommitSCN�� chaining�� CTS�� Commit SCN�� ��Ÿ����. ���ƾ��Ѵ�. */
        IDE_TEST_RAISE( ! SM_SCN_IS_EQ( &(aCTS->mNxtCommitSCN), &(sChainedCTSCommitSCN) ),
                        ERR_INVALID_CTS_COMMIT_SCN );

        idlOS::memcpy( aCTS,
                       ((UChar*)sCTS),
                       ID_SIZEOF( sdnCTS ) );

        idlOS::memcpy( (UChar*)&sKeyListSize,
                       ((UChar*)sCTS) + ID_SIZEOF( sdnCTS ),
                       ID_SIZEOF(UShort) );

        IDE_TEST_RAISE( sKeyListSize >= SD_PAGE_SIZE, ERR_INVALID_KEY_LIST_SIZE );

        idlOS::memcpy( aKeyList,
                       ((UChar*)sCTS) + ID_SIZEOF(sdnCTS) + ID_SIZEOF(ULong),
                       sKeyListSize );
    }

    IDE_EXCEPTION_CONT( CONT_REUSE_UNDO_PAGE );

    if( sLatchState != 0 )
    {
        sLatchState = 0;
        IDE_TEST( sdbBufferMgr::releasePage( aStatistics, sPagePtr )
                  != IDE_SUCCESS );
    }

    *aIsReusedUndoRec = sIsReusedUndoRec;
    *aKeyListSize     = sKeyListSize;

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_INFINITE_CTS_COMMIT_SCN );
    {
        ideLog::log( IDE_SERVER_0,
                     "CTS Commit SCN             : %llu\n"
                     "CTS Next Commit SCN        : %llu\n"
                     "Min First Disk View SCN    : %llu\n",
#ifdef COMPILE_64BIT
                     aCTS->mCommitSCN,
                     aCTS->mNxtCommitSCN,
                     sSysMinDskSCN
#else
                     ((ULong)((((ULong)aCTS->mCommitSCN.mHigh) << 32) |
                             (ULong)aCTS->mCommitSCN.mLow)),
                     ((ULong)((((ULong)aCTS->mNxtCommitSCN.mHigh) << 32) |
                             (ULong)aCTS->mNxtCommitSCN.mLow)),
                     ((ULong)((((ULong)sSysMinDskSCN.mHigh) << 32) |
                             (ULong)sSysMinDskSCN.mLow))
#endif
            );
    }
    IDE_EXCEPTION( ERR_INVALID_PAGE_TYPE );
    {
        ideLog::log( IDE_SERVER_0,
                     "Page Type        : %u\n"
                     "CTS Commit SCN   : %llu\n"
                     "CTS NxtCommitSCN : %llu\n"
                     "Sys Min Disk SCN : %llu\n",
                     sPageType,
#ifdef COMPILE_64BIT
                     aCTS->mCommitSCN,
                     aCTS->mNxtCommitSCN,
                     sSysMinDskSCN
#else
                     ((ULong)((((ULong)aCTS->mCommitSCN.mHigh) << 32) |
                             (ULong)aCTS->mCommitSCN.mLow)),
                     ((ULong)((((ULong)aCTS->mNxtCommitSCN.mHigh) << 32) |
                             (ULong)aCTS->mNxtCommitSCN.mLow)),
                     ((ULong)((((ULong)sSysMinDskSCN.mHigh) << 32) |
                             (ULong)sSysMinDskSCN.mLow))
#endif
            );
    }
    IDE_EXCEPTION( ERR_INVALID_UNDO_SLOT_NUM );
    {
        ideLog::log( IDE_SERVER_0,
                     "CTS Undo Slot Num (%u) >="
                     "Undo Slot Entry Count (%u)\n"
                     "CTS Commit SCN   :%llu\n"
                     "CTS NxtCommitSCN :%llu\n"
                     "Sys Min Disk SCN :%llu\n",
                     aCTS->mUndoSlotNum,
                     sSlotDirHdr->mSlotEntryCount,
#ifdef COMPILE_64BIT
                     aCTS->mCommitSCN,
                     aCTS->mNxtCommitSCN,
                     sSysMinDskSCN
#else
                     ((ULong)((((ULong)aCTS->mCommitSCN.mHigh) << 32) |
                             (ULong)aCTS->mCommitSCN.mLow)),
                     ((ULong)((((ULong)aCTS->mNxtCommitSCN.mHigh) << 32) |
                             (ULong)aCTS->mNxtCommitSCN.mLow)),
                     ((ULong)((((ULong)sSysMinDskSCN.mHigh) << 32) |
                             (ULong)sSysMinDskSCN.mLow))
#endif
            );
    }
    IDE_EXCEPTION( ERR_INVALID_CTS_COMMIT_SCN );
    {
        idlOS::memcpy( (UChar*)&sChainedCTS,
                       (UChar*)sCTS,
                       ID_SIZEOF( sdnCTS ) );

       ideLog::log( IDE_SERVER_0,
                    "CTS Commit SCN             : %llu\n"
                    "CTS Next Commit SCN        : %llu\n"
                    "Chained CTS Commit SCN     : %llu\n"
                    "Chained CTS Next Commit SCN: %llu\n"
                    "Min First Disk View SCN    : %llu\n",
#ifdef COMPILE_64BIT
                    aCTS->mCommitSCN,
                    aCTS->mNxtCommitSCN,
                    sChainedCTS.mCommitSCN,
                    sChainedCTS.mNxtCommitSCN,
                    sSysMinDskSCN
#else
                    ((ULong)((((ULong)aCTS->mCommitSCN.mHigh) << 32) |
                             (ULong)aCTS->mCommitSCN.mLow)),
                    ((ULong)((((ULong)aCTS->mNxtCommitSCN.mHigh) << 32) |
                             (ULong)aCTS->mNxtCommitSCN.mLow)),
                    ((ULong)((((ULong)sChainedCTS.mCommitSCN.mHigh) << 32) |
                             (ULong)sChainedCTS.mCommitSCN.mLow)),
                    ((ULong)((((ULong)sChainedCTS.mNxtCommitSCN.mHigh) << 32) |
                             (ULong)sChainedCTS.mNxtCommitSCN.mLow)),
                    ((ULong)((((ULong)sSysMinDskSCN.mHigh) << 32) |
                             (ULong)sSysMinDskSCN.mLow))
#endif
            );
    }
    IDE_EXCEPTION( ERR_INVALID_UNDO_REC_TYPE );
    {
        ideLog::log( IDE_SERVER_0,
                     "Undo PageID               : %u\n"
                     "Undo Slot Number          : %u\n"
                     "UndoRec Hdr Type          : %u\n"
                     "Chained CTS\'s Commit SCN : %llu\n"
                     "Min DskView SCN           : %llu\n",
                     aCTS->mUndoPID,
                     aCTS->mUndoSlotNum,
                     sType,
#ifdef COMPILE_64BIT
                     aCTS->mNxtCommitSCN,
                     sSysMinDskSCN
#else
                     ((ULong)((((ULong)aCTS->mNxtCommitSCN.mHigh) << 32) |
                              (ULong)aCTS->mNxtCommitSCN.mLow)),
                     ((ULong)((((ULong)sSysMinDskSCN.mHigh) << 32) |
                              (ULong)sSysMinDskSCN.mLow))
#endif
            );
    }
    IDE_EXCEPTION( ERR_INVALID_KEY_LIST_SIZE );
    {
        ideLog::log( IDE_SERVER_0,
                     "Undo PageID      : %u\n"
                     "Undo Slot Number : %u\n"
                     "KeyList Size     : %u\n",
                     aCTS->mUndoPID,
                     aCTS->mUndoSlotNum,
                     sKeyListSize );
    }
    IDE_EXCEPTION_END;

    IDE_PUSH();
    {
        if ( sLatchState != 0 )
        {
            // Dump Page and page Header
            (void)sdpPhyPage::tracePage( IDE_SERVER_0,
                                         (UChar*)aPage,
                                         "Index Page" );

            // Dump Page and page Header
            (void)sdpPhyPage::tracePage( IDE_SERVER_0,
                                         sPagePtr,
                                         "Undo Page" );

            IDE_ASSERT( sdbBufferMgr::releasePage( aStatistics, sPagePtr )
                        == IDE_SUCCESS );
        }
    }
    IDE_POP();

    IDE_ASSERT( 0 );

    return IDE_FAILURE;
}
#endif


/*********************************************************************
 * FUNCTION DESCRIPTION : sdnIndexCTL::isMyTransaction               *
 * ------------------------------------------------------------------*
 * �ڽ��� Ʈ����������� �˻��Ѵ�.                                   *
 *********************************************************************/
idBool sdnIndexCTL::isMyTransaction( void          * aTrans,
                                     sdpPhyPageHdr * aPage,
                                     UChar           aSlotNum )
{
    smSCN    sSCN;

    sSCN = smLayerCallback::getFstDskViewSCN( aTrans );

    return isMyTransaction( aTrans, aPage, aSlotNum, &sSCN );
}

idBool sdnIndexCTL::isMyTransaction( void          * aTrans,
                                     sdpPhyPageHdr * aPage,
                                     UChar           aSlotNum,
                                     smSCN         * aSCN )
{
    sdSID    sTSSlotSID;
    sdnCTS * sCTS;

    sCTS = getCTS( aPage, aSlotNum );

    sTSSlotSID = smLayerCallback::getTSSlotSID( aTrans );

    if( (SD_MAKE_PID( sTSSlotSID ) == sCTS->mTSSlotPID) &&
        (SD_MAKE_SLOTNUM( sTSSlotSID ) == sCTS->mTSSlotNum) )
    {
        if( SM_SCN_IS_EQ( &sCTS->mCommitSCN, aSCN ) )
        {
            return ID_TRUE;
        }
    }
    return ID_FALSE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : sdnIndexCTL::isSameTransaction             *
 * ------------------------------------------------------------------*
 * �־��� 2���� CTS�� ���� Ʈ����������� �˻��Ѵ�.                  *
 *********************************************************************/
idBool sdnIndexCTL::isSameTransaction( sdnCTS * aCTS1,
                                       sdnCTS * aCTS2 )
{
    if( (aCTS1->mTSSlotPID == aCTS2->mTSSlotPID) &&
        (aCTS1->mTSSlotNum == aCTS2->mTSSlotNum) )
    {
        if( ( (aCTS1->mState == SDN_CTS_UNCOMMITTED) &&
              (aCTS2->mState == SDN_CTS_UNCOMMITTED) ) ||
            ( (aCTS1->mState == SDN_CTS_STAMPED) &&
              (aCTS2->mState == SDN_CTS_STAMPED) ) )
        {
            if( SM_SCN_IS_EQ( &aCTS1->mCommitSCN, &aCTS2->mCommitSCN ) )
            {
                return ID_TRUE;
            }
        }
    }

    return ID_FALSE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : sdnIndexCTL::getTSSlotSID                  *
 * ------------------------------------------------------------------*
 * CTSlotSID�� ��´�.                                               *
 *********************************************************************/
sdSID sdnIndexCTL::getTSSlotSID( sdpPhyPageHdr * aPage,
                                 UChar           aSlotNum )
{
    sdnCTS * sCTS;

    sCTS = getCTS( aPage, aSlotNum );

    return SD_MAKE_SID( sCTS->mTSSlotPID,
                        sCTS->mTSSlotNum );
}

/*********************************************************************
 * FUNCTION DESCRIPTION : sdnIndexCTL::getRefKey                     *
 * ------------------------------------------------------------------*
 * CTS reference infomation�� �����Ѵ�.                              *
 *********************************************************************/
void sdnIndexCTL::getRefKey( sdpPhyPageHdr * aPage,
                             UChar           aSlotNum,
                             UShort        * aRefKeyCount,
                             UShort       ** aArrRefKey )
{
    sdnCTS * sCTS;

    sCTS = getCTS( aPage, aSlotNum );

    *aRefKeyCount = sCTS->mRefCnt;
    *aArrRefKey = sCTS->mRefKey;
}
#if 0
/*********************************************************************
 * FUNCTION DESCRIPTION : sdnIndexCTL::getRefKeyCount                *
 * ------------------------------------------------------------------*
 * CTS reference count�� �����Ѵ�.                                   *
 *********************************************************************/
UShort sdnIndexCTL::getRefKeyCount( sdpPhyPageHdr * aPage,
                                    UChar           aSlotNum )
{
    sdnCTS * sCTS;

    sCTS = getCTS( aPage, aSlotNum );

    return (sCTS->mRefCnt);
}
#endif
/*********************************************************************
 * FUNCTION DESCRIPTION : sdnIndexCTL::addRefKey                     *
 * ------------------------------------------------------------------*
 * CTS reference key�� �����Ѵ�.                                     *
 *********************************************************************/
void sdnIndexCTL::addRefKey( sdpPhyPageHdr * aPage,
                             UChar           aSlotNum,
                             UShort          aKeyOffset )
{
    sdnCTS * sCTS;
    UInt     i;

    sCTS = getCTS( aPage, aSlotNum );

    for( i = 0; i < SDN_CTS_MAX_KEY_CACHE; i++ )
    {
        if( sCTS->mRefKey[i] == SDN_CTS_KEY_CACHE_NULL )
        {
            sCTS->mRefKey[i] = aKeyOffset;
            break;
        }
    }

    sCTS->mRefCnt++;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : sdnIndexCTL::updateRefKey                  *
 * ------------------------------------------------------------------*
 * BUG-29506 TBT�� TBK�� ��ȯ�� offset�� CTS�� �ݿ����� �ʽ��ϴ�.    *
 * CTS reference key�� �����Ѵ�.                                     *
 *********************************************************************/
IDE_RC sdnIndexCTL::updateRefKey( sdrMtx        * aMtx,
                                  sdpPhyPageHdr * aPage,
                                  UChar           aSlotNum,
                                  UShort          aOldKeyOffset,
                                  UShort          aNewKeyOffset )
{
    sdnCTS * sCTS;
    UInt     i;

    sCTS = getCTS( aPage, aSlotNum );

    for( i = 0; i < SDN_CTS_MAX_KEY_CACHE; i++ )
    {
        if( sCTS->mRefKey[i] == aOldKeyOffset )
        {
            IDE_TEST( sdrMiniTrans::writeNBytes( aMtx,
                                                 (UChar*)&(sCTS->mRefKey[i]),
                                                 (UChar*)&aNewKeyOffset,
                                                 ID_SIZEOF(UShort) )
                      != IDE_SUCCESS );
            break;
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : sdnIndexCTL::cleanRefInfo                  *
 * ------------------------------------------------------------------*
 * CTL�� reference ������ reset�Ѵ�.                                 *
 *********************************************************************/
void sdnIndexCTL::cleanAllRefInfo( sdpPhyPageHdr * aPage )
{
    sdnCTL  * sCTL;
    sdnCTS  * sCTS;
    UInt      i, j;

    sCTL = getCTL( aPage );

    for( i = 0; i < getCount(sCTL); i++ )
    {
        sCTS = getCTS( sCTL, i );
        sCTS->mRefCnt = 0;

        for( j = 0; j < SDN_CTS_MAX_KEY_CACHE; j++ )
        {
            sCTS->mRefKey[j] = SDN_CTS_KEY_CACHE_NULL;
        }
    }
}
#if 0
/*********************************************************************
 * FUNCTION DESCRIPTION : sdnIndexCTL::cleanRefInfo                  *
 * ------------------------------------------------------------------*
 * CTS�� reference ������ reset�Ѵ�.                                 *
 *********************************************************************/
void sdnIndexCTL::cleanRefInfo( sdpPhyPageHdr * aPage,
                                UChar           aCTSlotNum )
{
    sdnCTL  * sCTL;
    sdnCTS  * sCTS;
    UInt      i;

    sCTL = getCTL( aPage );

    sCTS = getCTS( sCTL, aCTSlotNum );
    sCTS->mRefCnt = 0;

    for( i = 0; i < SDN_CTS_MAX_KEY_CACHE; i++ )
    {
        sCTS->mRefKey[i] = SDN_CTS_KEY_CACHE_NULL;
    }
}
#endif
#if 0 // not used
/*********************************************************************
 * FUNCTION DESCRIPTION : sdnIndexCTL::getVictimTrans                *
 * ------------------------------------------------------------------*
 * Uncommitted CTS�� victim�� �����Ѵ�.                              *
 *********************************************************************/
IDE_RC sdnIndexCTL::getVictimTrans( idvSQL        * aStatistics,
                                    sdpPhyPageHdr * aPage,
                                    smTID         * aTransID )
{
    UInt      i;
    sdnCTL  * sCTL;
    sdnCTS  * sCTS;
    sdcTSS  * sTSSlot;
    UChar   * sPagePtr;
    smSCN     sTransCSCN;
    idBool    sIsFixed = ID_FALSE;

    sCTL = getCTL( aPage );

    *aTransID = SM_NULL_TID;

    for( i = 0; i < getCount(sCTL); i++ )
    {
        sCTS = getCTS( sCTL, i );

        if( sCTS->mState == SDN_CTS_UNCOMMITTED )
        {
            IDE_TEST( sdbBufferMgr::fixPageByPID(
                                            aStatistics,
                                            SMI_ID_TABLESPACE_SYSTEM_DISK_UNDO,
                                            sCTS->mTSSlotPID,
                                            SDB_WAIT_NORMAL,
                                            &sPagePtr ) 
                      != IDE_SUCCESS );
            sIsFixed = ID_TRUE;

            IDE_TEST( sdpSlotDirectory::getPagePtrFromSlotNum(
                                        sdpPhyPage::getSlotDirStartPtr(sPagePtr),
                                        sCTS->mTSSlotNum,
                                        (UChar**)&sTSSlot )
                      != IDE_SUCCESS );

            sdcTSSlot::getTransInfo( sTSSlot, aTransID, &sTransCSCN );

            IDE_TEST( sdbBufferMgr::unfixPage( aStatistics, sPagePtr )
                      != IDE_SUCCESS );

            break;
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    
    if ( sIsFixed == ID_TRUE ) 
    {
        sIsFixed = ID_FALSE;
        (void)sdbBufferMgr::unfixPage( aStatistics, sPagePtr );
    }
    return IDE_FAILURE;
}
#endif
#if 0
/*********************************************************************
 * FUNCTION DESCRIPTION : sdnIndexCTL::isChainedCTS                  *
 * ------------------------------------------------------------------*
 * chain�� ������ �ִ� CTS������ �˻��Ѵ�.                           *
 *********************************************************************/
idBool sdnIndexCTL::hasChainedCTS( sdpPhyPageHdr * aPage,
                                  UChar           aCTSlotNum )
{
    sdnCTL  * sCTL;
    sdnCTS  * sCTS;

    sCTL = getCTL( aPage );
    sCTS = getCTS( sCTL, aCTSlotNum );

    return hasChainedCTS( sCTS );
}
#endif
#if 0
/*********************************************************************
 * FUNCTION DESCRIPTION : sdnIndexCTL::isChainedCTS                  *
 * ------------------------------------------------------------------*
 * chain�� ������ �ִ� CTS������ �˻��Ѵ�.                           *
 *********************************************************************/
idBool sdnIndexCTL::hasChainedCTS( sdnCTS * aCTS )
{
    if( (aCTS->mUndoPID == 0) && (aCTS->mUndoSlotNum == 0) )
    {
        return ID_FALSE;
    }
    return ID_TRUE;
}
#endif
/*********************************************************************
 * FUNCTION DESCRIPTION : sdnIndexCTL::getCTLayerSize                *
 * ------------------------------------------------------------------*
 * CTL�� �� ũ�⸦ �����Ѵ�.                                         *
 *********************************************************************/
UShort sdnIndexCTL::getCTLayerSize( UChar * aPage )
{
    sdnCTL  * sCTL;

    sCTL = getCTL( (sdpPhyPageHdr*)aPage );

    return (getCount(sCTL) * ID_SIZEOF(sdnCTS)) +
           ID_SIZEOF(sdnCTLayerHdr);
}

#if 0
/*********************************************************************
 * FUNCTION DESCRIPTION : sdnIndexCTL::wait4Trans                    *
 * ------------------------------------------------------------------*
 * CTS�� �Ҵ��� Ʈ������� �Ϸ�Ǳ⸦ Ư���ð���ŭ�� ����Ѵ�.
 *
 * BUG-24140 Index/Data CTL �Ҵ�������� TRANS_WAIT_TIME_FOR_CTS
 *           �� �ʰ��ϴ� ��� Error�� �߻��ؼ��� �ȵ�.
 *
 * ���ð� �ʰ��� ���ؼ� Error �� �߻��� ��� IDE_SUCCESS�� ��ȯ�Ѵ�.
 * ��, �ϳ��� Ʈ������� ����������, �� Ʈ������� �Ϸ�ɶ����� ������� �ʰ�,
 * ������Ƽ�� �־��� �ð����� �Ϸ���� �ʾҴٸ� AllocCTS �������� ó������ �ٽ�
 * �����Ѵ�.
 *
 * aStatistics  - [IN] �������
 * aTrans       - [IN] Ʈ����� ������
 * aWait4TID    - [IN] ����ؾ��� Ʈ����� ID
 *
 ***********************************************************************/
IDE_RC sdnIndexCTL::wait4Trans( void   * aTrans,
                                smTID    aWait4TID )
{
    IDE_ASSERT( aTrans    != NULL );
    IDE_ASSERT( aWait4TID != SM_NULL_TID );

    if ( smLayerCallback::waitForTrans( aTrans,
                                        aWait4TID,
                                        smuProperty::getTransWaitTime4TTS() )
         != IDE_SUCCESS )
    {
        IDE_TEST( ideGetErrorCode() != smERR_ABORT_smcExceedLockTimeWait );
        IDE_CLEAR();
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}
#endif
/*********************************************************************
 * FUNCTION DESCRIPTION : sdnIndexCTL::dump                          *
 * ------------------------------------------------------------------*
 * TASK-4007 [SM] PBT�� ���� ��� �߰� - dumpddf����ȭ               *
 * IndexPage���� CTS�� Dump�Ѵ�.                                     *
 *                                                                   *
 * BUG-28379 [SD] sdnbBTree::dumpNodeHdr( UChar *aPage ) ������      *
 * local Array�� ptr�� ��ȯ�ϰ� �ֽ��ϴ�.                            *
 * Local Array��� OutBuf�� �޾� �����ϵ��� �����մϴ�.              *
 *********************************************************************/
IDE_RC sdnIndexCTL::dump( UChar *aPage  ,
                          SChar *aOutBuf ,
                          UInt   aOutSize )

{
    UInt            i;
    sdnCTL        * sCTL;
    sdnCTS        * sCTS;

    IDE_DASSERT( aPage != NULL );
    IDE_DASSERT( sdpPhyPage::getPageStartPtr(aPage) == aPage );

    sCTL = getCTL( (sdpPhyPageHdr*) aPage );

    idlOS::snprintf( aOutBuf,
                     aOutSize,
                     "----------- Index CTL Begin ----------\n"
                     "mCount     : %"ID_UINT32_FMT"\n"
                     "mUsedCount : %"ID_UINT32_FMT"\n\n",
                     sCTL->mCount,
                     sCTL->mUsedCount );

    for( i = 0; i < getCount(sCTL); i++ )
    {
        sCTS = getCTS( sCTL, i );

        idlVA::appendFormat( aOutBuf,
                             aOutSize,
                             "[%"ID_UINT32_FMT"] CTS ----------------\n"
                             "mCommitSCN    : 0x%"ID_xINT64_FMT"\n"
                             "mTSSlotPID    : %"ID_UINT32_FMT"\n"
                             "mTSSlotNum    : %"ID_UINT32_FMT"\n"
                             "mState        : %"ID_UINT32_FMT"\n"
                             "mRefCnt       : %"ID_UINT32_FMT"\n"
                             "mRefKey[ 0 ]  : %"ID_UINT32_FMT"\n"
                             "mRefKey[ 1 ]  : %"ID_UINT32_FMT"\n"
                             "mRefKey[ 2 ]  : %"ID_UINT32_FMT"\n",
                             i,
                             SM_SCN_TO_LONG( sCTS->mCommitSCN ),
                             sCTS->mTSSlotPID,
                             sCTS->mTSSlotNum,
                             sCTS->mState,
                             sCTS->mRefCnt,
                             sCTS->mRefKey[ 0 ],
                             sCTS->mRefKey[ 1 ],
                             sCTS->mRefKey[ 2 ] );
    }

    idlVA::appendFormat( aOutBuf,
                         aOutSize,
                         "----------- Index CTL End   ----------\n" );

    return IDE_SUCCESS;
}

