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

#ifndef _O_SDS_META_H_
#define _O_SDS_META_H_ 1

#include <sdbBCBHash.h>

/* sdsMetaData(24byte) ����Ǹ� �ϳ��� Group�� �ִ�  64*32 ���� meta�� ���ԵǴ� ����� ���� 
 *  seq  + chechsum + sizeof(sdsMetaData)*2048 + checksum 
 = 4byte + 12 byte  + 49152byte +              + 12byte 
 = 49180 byte (7page = 57,344 byte)
 */

#define SDS_META_FRAME_MAX_COUNT_IN_GROUP   SDS_META_MAX_CHUNK_PAGE_COUNT
#define SDS_META_DATA_MAX_COUNT_IN_GROUP    SDS_META_MAX_CHUNK_PAGE_COUNT
#define SDS_META_TABLE_PAGE_COUNT           (7)
#define SDS_META_TABLE_SIZE                 (SDS_META_TABLE_PAGE_COUNT * SD_PAGE_SIZE)

typedef struct sdsMetaData
{
    scSpaceID       mSpaceID; /* �ش� page�� spaceID */
    scPageID        mPageID;  /* �ش� page�� pageID  */
    sdsSBCBState    mState;   /* �ش� page�� ����. �� dirty / clean ����*/
    smLSN           mPageLSN; /* LSN ����            */
} sdsMetaData;

class sdsMeta
{
public:
    IDE_RC initializeStatic( UInt             aPageCount,
                             UInt             aExtNum,
                             sdsFile        * aFile,
                             sdsBufferArea  * aBufferArea,
                             sdbBCBHash     * aHashTable );

    IDE_RC destroyStatic( void );

    IDE_RC finalize( idvSQL * aStatistics );

    IDE_RC makeMetaTable( idvSQL * aStatistics,  UInt aExtentIndex );

    IDE_RC copyToMetaTable( UInt aMetaTableSeqNum );

    IDE_RC writeMetaTable( idvSQL * aStatistics, UInt aMetaTableSeqNum );

    IDE_RC readMetaTable( idvSQL * aStatistics, UInt aMetaTableSeqNum );

    IDE_RC readMetaInfo( idvSQL * aStatistics, 
                         UInt   * aLstMetaTableSeqNum,
                         smLSN  * aRecoveryLSN );

    IDE_RC buildBCB( idvSQL * aStatistics, idBool aNeedRecovery );
    
    inline ULong getFrameOffset ( UInt aIndex );

    inline ULong getTableOffset ( UInt aSeq ); 

    /* for Util */ 
    IDE_RC dumpMetaTable( const UChar    * aPage,
                          SChar          * aOutBuf, 
                          UInt             aOutSize );

    IDE_RC dumpHex( void );

private:

    idBool needToWriteMetaTable( idvSQL * aStatistics, UInt aExtentIndex );

    IDE_RC copyToMetaTable( idvSQL  * aStatistics,  
                            UInt      aMetaTableSeqNum,
                            UInt    * aBaseIndex,
                            smLSN   * aRecoveryLSN );

    IDE_RC readAndCheckUpMetaTable( idvSQL         * aStatistics, 
                                    UInt             aMetaTableSeqNum,
                                    sdsMetaData    * aMeta,
                                    idBool         * aIsValidate ); 
    
    IDE_RC copyToSBCB( idvSQL      * aStatistics,
                       sdsMetaData * aMetaData,
                       UInt          aMetaTableNum,
                       smLSN       * aRecoveryLSN );

    IDE_RC buildBCBByMetaTable( idvSQL * aStatistics, 
                                UInt     aLstMetaTableSeqNum,
                                smLSN  * aRecoveryLSN );

    IDE_RC buildBCBByMetaTable( idvSQL * aStatistics,  
                                UInt     aMetaTableSeqNum,
                                UInt     aTotalMetaTableNum,
                                smLSN  * aRecoveryLSN );

    IDE_RC buildBCBByScan( idvSQL * aStatistics, 
                           UInt     aMetaTableSeqNum,
                           smLSN  * aRecoveryLSN );
public :



private:
    sdsMetaData  ** mMetaData;  // Meta data array  
    /* Secondary Buffer �� �����ϱ� ���� �ʿ��� �ִ� meta talble ���� */
    UInt            mMaxMetaTableCnt;
    /* Group�ȿ� ���ԵǴ� Ext�� ���� */
    UInt            mExtCntInGroup;
    /* �ϳ��� Extent�� ���ԵǴ� meta data ���� */
    UInt            mFrameCntInExt; 
     /* MetaTable ������ Ȯ���ϴ� ���� */    
    UChar         * mBase;
    UChar         * mBasePtr;
    /*  */
    sdsFile       * mSBufferFile;
    /*  */
    sdsBufferArea * mSBufferArea; 
    /*  */
    sdbBCBHash    * mHashTable;

};

 /* get frame(F) offset 
 * |F|F|F|F|F|M |F|F|F|F|F|M | 
                ^ here
 */
ULong sdsMeta::getFrameOffset( UInt aExtIndex )
{ 
    IDE_ASSERT( mFrameCntInExt > 0 );
    IDE_ASSERT( mExtCntInGroup > 0 );

    return ( ( aExtIndex*SD_PAGE_SIZE ) +
             ( (aExtIndex / (mFrameCntInExt * mExtCntInGroup)) * SDS_META_TABLE_SIZE ) ); 
}

/* get MetaTable(M) offset
 * |F|F|F|F|F|M |F|F|F|F|F|M | 
                          ^ here
 */
ULong sdsMeta::getTableOffset( UInt aMetaTblSeq )  
{
    IDE_ASSERT( mFrameCntInExt > 0 );
    IDE_ASSERT( mExtCntInGroup > 0 );

    return ( ( aMetaTblSeq * SDS_META_TABLE_SIZE ) +
             ( (aMetaTblSeq+1) * mFrameCntInExt * mExtCntInGroup * SD_PAGE_SIZE ) );
}
#endif //_O_SDS_META_H_
