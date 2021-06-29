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
 *
 * $Id: sdpscSH.h 82075 2018-01-17 06:39:52Z jina.kim $
 *
 * �� ������ Circular-List Managed Segment�� Segment Header�� ��������̴�.
 *
 ***********************************************************************/

# ifndef _O_SDPSC_SH_H_
# define _O_SDPSC_SH_H_ 1

# include <sdpDef.h>
# include <sdpPhyPage.h>

# include <sdpscDef.h>
# include <sdpscED.h>

class sdpscSegHdr
{

public:

    /* Segment Header �������� �ʱ�ȭ�Ѵ�. */
    static void initSegHdr ( sdpscSegMetaHdr * aSegHdreader,
                             scPageID          aSegHdrPID,
                             sdpSegType        aSegType,
                             UInt              aPageCntInExt,
                             UShort            aMaxExtCntInExtDir );

    /* [ INTERFACE ] segment ���¸� ��ȯ�Ѵ�. */
    static IDE_RC getSegState( idvSQL        *aStatistics,
                               scSpaceID      aSpaceID,
                               scPageID       aSegPID,
                               sdpSegState   *aSegState );

    /* Segment Header ������ ���� */
    static IDE_RC createAndInitPage( idvSQL                * aStatistics,
                                     sdrMtxStartInfo       * aStartInfo,
                                     scSpaceID               aSpaceID,
                                     sdpscExtDesc          * aFstExtDesc,
                                     sdpSegType              aSegType,
                                     UShort                  aMaxExtCntInExtDir );

    static IDE_RC addNewExtDir( idvSQL             * aStatistics,
                                sdrMtx             * aMtx,
                                scSpaceID            aSpaceID,
                                scPageID             aSegHdrPID,
                                scPageID             aCurExtDirPID,
                                scPageID             aNewExtDirPID,
                                UInt               * aTotExtCntOfSeg );

    static IDE_RC removeExtDir( idvSQL             * aStatistics,
                                sdrMtx             * aMtx,
                                scSpaceID            aSpaceID,
                                scPageID             aSegHdrPID,
                                scPageID             aPrvPIDOfExtDir,
                                scPageID             aRemExtDirPID,
                                scPageID             aNxtPIDOfNewExtDir,
                                UShort               aTotExtCntOfRemExtDir,
                                UInt               * aTotExtCntOfSeg );


    /* Segment Header �������� ���ο� ExtDir ���� */
    static IDE_RC addNewExtDir( sdrMtx             * aMtx,
                                sdpscSegMetaHdr    * aSegHdr,
                                sdpscExtDirCntlHdr * aCurExtDirCntlHdr,
                                sdpscExtDirCntlHdr * aNewExtDirCntlHdr );

    /* Segment Header �������� ExtDir ���� */
    static IDE_RC removeExtDir( sdrMtx             * aMtx,
                                sdpscSegMetaHdr    * aSegHdr,
                                sdpscExtDirCntlHdr * aPrvExtDirCntlHdr,
                                scPageID             aRemExtDir,
                                scPageID             aNewNxtExtDir );

    /* [ INTERFACE ] Sequential Scan Before First���� Segment ���� ��ȯ */
    static IDE_RC getSegInfo( idvSQL        * aStatistics,
                              scSpaceID       aSpaceID,
                              scPageID        aSegPID,
                              void          * aTableHeader,
                              sdpSegInfo    * aSegInfo );

    static IDE_RC getFmtPageCnt( idvSQL        *aStatistics,
                                 scSpaceID      aSpaceID,
                                 sdpSegHandle  *aSegHandle,
                                 ULong         *aFmtPageCnt );

    /* Segment Header ���������� ������ Lst ExtDir PID�� ��ȯ�Ѵ�. */
    static inline scPageID getLstExtDir( sdpscSegMetaHdr * aSegHdr );

    /* Segment Header ���������� ù��° ExtDir PID�� ��ȯ�Ѵ�. */
    static inline scPageID getFstExtDir( sdpscSegMetaHdr * aSegHdr );

    /* ExtDir�� ������ ��ȯ�Ѵ�. */
    static inline UInt     getExtDirCnt( sdpscSegMetaHdr * aSegHdr );

    /* ExtDir Control Header ��ȯ */
    static inline sdpscExtDirCntlHdr * getExtDirCntlHdr( sdpscSegMetaHdr * aSegHdr );

    /* Extent Control Header ��ȯ */
    static inline sdpscExtCntlHdr * getExtCntlHdr( sdpscSegMetaHdr * aSegHdr );

    /* Segment Control Header ��ȯ */
    static inline sdpscSegCntlHdr * getSegCntlHdr( sdpscSegMetaHdr * aSegHdr );

    /* �������� ������ �����Ϳ��� Logical Header Ptr�� ��ȯ */
    static inline sdpscSegMetaHdr* getHdrPtr( UChar * aPagePtr );

    static inline void  calcExtRID2ExtInfo( scPageID             aSegPID,
                                            sdpscSegMetaHdr    * aSegHdr,
                                            sdRID                aExtRID,
                                            scPageID           * aExtDir,
                                            SShort             * aDescIdx );

    static inline void  calcExtInfo2ExtRID( scPageID             aSegPID,
                                            sdpscSegMetaHdr    * aSegHdr,
                                            scPageID             aExtDir,
                                            SShort               aDescIdx,
                                            sdRID              * aExtRID );
    // �ִ� ������ �� �ִ� Ext Desc�� ����
    static inline UShort getMaxExtDescCnt( sdpSegType aSegType );

    /* ExtDir�� ����� �� �ִ� ExtDesc ���� ��ȯ */
    static inline UShort getFreeDescCntOfExtDir(
                            sdpscExtDirCntlHdr * aCntlHdr,
                            sdpSegType           aSegType );

    static IDE_RC fixAndGetHdr4Write( idvSQL           * aStatistics,
                                      sdrMtx           * aMtx,
                                      scSpaceID          aSpaceID,
                                      scPageID           aSegHdrPID,
                                      sdpscSegMetaHdr ** aSegHdr );

    static IDE_RC fixAndGetHdr4Read( idvSQL            * aStatistics,
                                     sdrMtx            * aMtx,
                                     scSpaceID           aSpaceID,
                                     scPageID            aSegHdrPID,
                                     sdpscSegMetaHdr  ** aSegHdr );

    static IDE_RC setTotExtCnt( sdrMtx          * aMtx,
                                sdpscExtCntlHdr * aExtCntlHdr,
                                UInt              aTotExtDescCnt );

private :

    static IDE_RC setTotExtDirCnt( sdrMtx          * aMtx,
                                   sdpscExtCntlHdr * aExtCntlHdr,
                                   UInt              aTotExtDirCnt );

    static IDE_RC setLstExtDir( sdrMtx          * aMtx,
                                sdpscExtCntlHdr * aExtCntlHdr,
                                scPageID          aLstExtDirPID );
};

/*
 * �������� ������ �����Ϳ��� Logical Header Ptr�� ��ȯ�Ѵ�.
 */
inline sdpscSegMetaHdr* sdpscSegHdr::getHdrPtr( UChar * aPagePtr )
{
    IDE_ASSERT( aPagePtr != NULL );
    return (sdpscSegMetaHdr*)sdpPhyPage::getLogicalHdrStartPtr( aPagePtr );
}

/* Segment Header���� Segment Control Header�� Ptr�� ��ȯ�Ѵ�. */
inline sdpscSegCntlHdr * sdpscSegHdr::getSegCntlHdr( sdpscSegMetaHdr * aSegHdr )
{
    return (sdpscSegCntlHdr*)&(aSegHdr->mSegCntlHdr);
}

/* Segment Header���� Segment Control Header�� Ptr�� ��ȯ�Ѵ�. */
inline sdpscExtCntlHdr * sdpscSegHdr::getExtCntlHdr( sdpscSegMetaHdr * aSegHdr )
{
    return (sdpscExtCntlHdr*)&(aSegHdr->mExtCntlHdr);
}

/* ExtDir Control Header ��ȯ */
inline sdpscExtDirCntlHdr * sdpscSegHdr::getExtDirCntlHdr( sdpscSegMetaHdr * aSegHdr )
{
    return (sdpscExtDirCntlHdr*)&(aSegHdr->mExtDirCntlHdr);
}

/* Segment Header�� ExtDir ������ �ִ� ExtDesc ���� ��ȯ */
inline UShort sdpscSegHdr::getMaxExtDescCnt( sdpSegType  aSegType )
{
    UShort sPropDescCnt;
    UShort sMaxDescCnt = (UShort)
        (((sdpPhyPage::getEmptyPageFreeSize()
         - ID_SIZEOF( sdpscSegMetaHdr ) ))
         / ID_SIZEOF( sdpscExtDesc ));

    switch( aSegType )
    {
        case SDP_SEG_TYPE_TSS:
            sPropDescCnt = smuProperty::getTSSegExtDescCntPerExtDir();
            IDE_ASSERT( sMaxDescCnt >= sPropDescCnt );
            break;

        case SDP_SEG_TYPE_UNDO:
            sPropDescCnt = smuProperty::getUDSegExtDescCntPerExtDir();
            IDE_ASSERT( sMaxDescCnt >= sPropDescCnt );
            break;

        default:
            IDE_ASSERT( 0 );
            break;
    }

    return sPropDescCnt;
}

/* ExtDir�� ����� �� �ִ� ExtDesc ���� ��ȯ */
inline UShort sdpscSegHdr::getFreeDescCntOfExtDir( sdpscExtDirCntlHdr * aCntlHdr,
                                                   sdpSegType           aSegType )
{
    return (getMaxExtDescCnt( aSegType ) - aCntlHdr->mExtCnt);
}

/* Segment Header ���������� ������ Lst ExtDir PID�� ��ȯ�Ѵ�. */
inline scPageID sdpscSegHdr::getLstExtDir( sdpscSegMetaHdr * aSegHdr )
{
    return getExtCntlHdr( aSegHdr )->mLstExtDir;
}

/* Segment Header ���������� ù��° ExtDir PID�� ��ȯ�Ѵ�. */
inline scPageID sdpscSegHdr::getFstExtDir( sdpscSegMetaHdr * aSegHdr )
{
    return getExtDirCntlHdr( aSegHdr )->mNxtExtDir;
}

/* ExtDir�� ������ ��ȯ�Ѵ�. */
inline UInt  sdpscSegHdr::getExtDirCnt( sdpscSegMetaHdr  * aSegHdr )
{
    return getExtCntlHdr( aSegHdr )->mTotExtDirCnt;
}

/***********************************************************************
 * Description : ExtDir PID�� ExtDesc �������� ExtDesc�� RID ��ȯ
 ***********************************************************************/
inline void  sdpscSegHdr::calcExtInfo2ExtRID( scPageID             aSegPID,
                                              sdpscSegMetaHdr    * aSegHdr,
                                              scPageID             aExtDir,
                                              SShort               aDescIdx,
                                              sdRID              * aExtRID )
{
    IDE_ASSERT( aSegHdr != NULL );
    IDE_ASSERT( aExtRID != NULL );

    if ( aSegPID != aExtDir )
    {
        *aExtRID = SD_MAKE_RID( aExtDir,
                                sdpscExtDir::calcDescIdx2Offset(
                                    NULL,
                                    aDescIdx ) );
    }
    else
    {
        *aExtRID = SD_MAKE_RID( aExtDir,
                                sdpscExtDir::calcDescIdx2Offset(
                                    getExtDirCntlHdr(aSegHdr),
                                    aDescIdx ) );
    }
    return;
}

/***********************************************************************
 * Description : ExtDesc�� RID�� ExtDir PID�� ExtDesc ���� ��ȯ
 ***********************************************************************/
inline void  sdpscSegHdr::calcExtRID2ExtInfo( scPageID             aSegPID,
                                              sdpscSegMetaHdr    * aSegHdr,
                                              sdRID                aExtRID,
                                              scPageID           * aExtDir,
                                              SShort             * aDescIdx )
{
    IDE_ASSERT( aSegHdr  != NULL );
    IDE_ASSERT( aExtRID  != SD_NULL_RID );
    IDE_ASSERT( aExtDir  != NULL );
    IDE_ASSERT( aDescIdx != NULL );

    if ( aSegPID != SD_MAKE_PID( aExtRID ) )
    {
        *aDescIdx = sdpscExtDir::calcOffset2DescIdx(
            NULL,
            SD_MAKE_OFFSET(aExtRID) );
    }
    else
    {
        *aDescIdx = sdpscExtDir::calcOffset2DescIdx(
            getExtDirCntlHdr(aSegHdr),
            SD_MAKE_OFFSET(aExtRID) );
    }
    *aExtDir = SD_MAKE_PID(aExtRID);
    return;
}

#endif // _O_SDPSC_SH_H_
