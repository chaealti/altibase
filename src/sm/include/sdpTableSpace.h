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
 * $Id: sdpTableSpace.h 86110 2019-09-02 04:52:04Z et16 $
 *
 * ���̺����̽� ������
 *
 * # ����
 *
 * �ϳ� �̻��� �������� ����Ÿ ���Ϸ� ������ ���̺����̽���
 * ������ level�� ������ �ڷᱸ���� ������
 *
 **********************************************************************/

#ifndef _O_SDP_TABLE_SPACE_H_
#define _O_SDP_TABLE_SPACE_H_ 1

#include <sdpDef.h>
#include <sddDef.h>
#include <sdptbSpaceDDL.h>
#include <sdptbGroup.h>
#include <sdptbVerifyAndDump.h>

class sdpTableSpace
{
public:

    /* ��� ���̺����̽� ��� �ʱ�ȭ */
    static IDE_RC initialize();
    /* ��� ���̺����̽� ��� ���� */
    static IDE_RC destroy();

    static IDE_RC createTBS( idvSQL            * aStatistics,
                             smiTableSpaceAttr * aTableSpaceAttr,
                             smiDataFileAttr  ** aDataFileAttr,
                             UInt                aDataFileAttrCount,
                             void*               aTrans )
    {
        sdrMtxStartInfo   sStartInfo;

        IDE_DASSERT( aTableSpaceAttr    != NULL );
        IDE_DASSERT( aDataFileAttr      != NULL );
        IDE_DASSERT( aDataFileAttrCount != 0 );
        IDE_DASSERT( aTrans             != NULL );

        sStartInfo.mTrans = aTrans;
        sStartInfo.mLogMode = SDR_MTX_LOGGING;

        return sdptbSpaceDDL::createTBS( aStatistics,
                                         &sStartInfo,
                                         aTableSpaceAttr,
                                         aDataFileAttr,
                                         aDataFileAttrCount );
    };

    /* Tablespace ���� */
    static IDE_RC dropTBS( idvSQL       *aStatistics,
                           void*         aTrans,
                           scSpaceID     aSpaceID,
                           smiTouchMode  aTouchMode )
    {
        return sdptbSpaceDDL::dropTBS( aStatistics,
                                       aTrans,
                                       aSpaceID,
                                       aTouchMode );
    };

    /* Tablespace ���� */
    static IDE_RC resetTBS( idvSQL           *aStatistics,
                            scSpaceID         aSpaceID,
                            void             *aTrans );

    /* Tablespace ��ȿȭ */
    static IDE_RC alterTBSdiscard( sddTableSpaceNode * aTBSNode )
    {
        return sdptbSpaceDDL::alterTBSdiscard( aTBSNode );
    };


    // PRJ-1548 User Memory TableSpace
    /* Disk Tablespace�� ���� Alter Tablespace Online/Offline�� ���� */
    static IDE_RC alterTBSStatus( idvSQL*             aStatistics,
                                  void              * aTrans,
                                  sddTableSpaceNode * aSpaceNode,
                                  UInt                aState )
    {
        return sdptbSpaceDDL::alterTBSStatus( aStatistics,
                                              aTrans,
                                              aSpaceNode,
                                              aState );
    };

    /* ����Ÿ���� ���� */
    static IDE_RC createDataFiles( idvSQL            *aStatistics,
                                   void*              aTrans,
                                   scSpaceID          aSpaceID,
                                   smiDataFileAttr  **aDataFileAttr,
                                   UInt               aDataFileAttrCount )
    {
        return sdptbSpaceDDL::createDataFilesFEBT( aStatistics,
                                                   aTrans,
                                                   aSpaceID,
                                                   aDataFileAttr,
                                                   aDataFileAttrCount ) ;
    };

    /* ����Ÿ���� ���� */
    static IDE_RC removeDataFile( idvSQL         *aStatistics,
                                  void*           aTrans,
                                  scSpaceID       aSpaceID,
                                  SChar          *aFileName,
                                  SChar          *aValidDataFileName )
    {
        return sdptbSpaceDDL::removeDataFile( aStatistics,
                                              aTrans,
                                              aSpaceID,
                                              aFileName,
                                              aValidDataFileName );
    };

    /* ����Ÿ���� �ڵ�Ȯ�� ��� ���� */
    static IDE_RC alterDataFileAutoExtend( idvSQL     *aStatistics,
                                           void*       aTrans,
                                           scSpaceID   aSpaceID,
                                           SChar      *aFileName,
                                           idBool      aAutoExtend,
                                           ULong       aNextSize,
                                           ULong       aMaxSize,
                                           SChar      *aValidDataFileName )
    {
        return sdptbSpaceDDL::alterDataFileAutoExtendFEBT( aStatistics,
                                                           aTrans,
                                                           aSpaceID,
                                                           aFileName,
                                                           aAutoExtend,
                                                           aNextSize,
                                                           aMaxSize,
                                                           aValidDataFileName );
    };

    /* ����Ÿ���� ��� ���� */
    static IDE_RC alterDataFileName( idvSQL*      aStatistics,
                                     scSpaceID    aSpaceID,
                                     SChar       *aOldName,
                                     SChar       *aNewName )
    {
        return sdptbSpaceDDL::alterDataFileName( aStatistics,
                                                 aSpaceID,
                                                 aOldName,
                                                 aNewName );
    };

    /* ����Ÿ���� ũ�� ���� */
    static IDE_RC alterDataFileReSize( idvSQL       *aStatistics,
                                       void         *aTrans,
                                       scSpaceID     aSpaceID,
                                       SChar        *aFileName,
                                       ULong         aSizeWanted,
                                       ULong        *aSizeChanged,
                                       SChar        *aValidDataFileName )
    {
        return sdptbSpaceDDL::alterDataFileReSizeFEBT( aStatistics,
                                                       aTrans,
                                                       aSpaceID,
                                                       aFileName,
                                                       aSizeWanted,
                                                       aSizeChanged,
                                                       aValidDataFileName );
    };


    /*
     * ================================================================
     * Request Function
     * ================================================================
     */
    // callback ���� ����ϱ� ���� public�� ����
    // Tablespace�� OFFLINE��Ų Tx�� Commit�Ǿ��� �� �Ҹ��� Pending�Լ�
    static IDE_RC alterOfflineCommitPending(
                      idvSQL            * aStatistics,
                      sctTableSpaceNode * aTBSNode,
                      sctPendingOp      * aPendingOp )
    {
        return sdptbSpaceDDL::alterOfflineCommitPending( aStatistics,
                                                         aTBSNode,
                                                         aPendingOp ) ;
    };

    // Tablespace�� ONLINE��Ų Tx�� Commit�Ǿ��� �� �Ҹ��� Pending�Լ�
    static IDE_RC alterOnlineCommitPending(
                      idvSQL            * aStatistics,
                      sctTableSpaceNode * aTBSNode,
                      sctPendingOp      * aPendingOp )
    {
        return sdptbSpaceDDL::alterOnlineCommitPending( aStatistics,
                                                        aTBSNode,
                                                        aPendingOp ) ;
    };

    /* BUG-15564 ���̺����̽��� �� �������� ������ ���� ��ȯ */
    static IDE_RC getTotalPageCount( idvSQL*      aStatistics,
                                     scSpaceID    aSpaceID,
                                     ULong       *aTotalPageCount )
    {
        return  sdptbGroup::getTotalPageCount( aStatistics,
                                               aSpaceID,
                                               aTotalPageCount );
    };

    static UInt  getExtPageCount( UChar * aPagePtr );

    static IDE_RC verify( idvSQL*    aStatistics,
                          scSpaceID  aSpaceID,
                          UInt       aFlag )
    {
        return sdptbVerifyAndDump::verify( aStatistics,
                                           aSpaceID,
                                           aFlag );
    };

    static IDE_RC dump( scSpaceID  aSpaceID,
                        UInt       aDumpFlag )
    {
        return sdptbVerifyAndDump::dump( aSpaceID, aDumpFlag );
    };

    /* Space Cache �Ҵ� �� �ʱ�ȭ */
    static IDE_RC  doActAllocSpaceCache( idvSQL            * aStatistics,
                                         sctTableSpaceNode * aSpaceNode,
                                         void              * /*aActionArg*/ );
    /* Space Cache ���� */
    static IDE_RC  doActFreeSpaceCache( idvSQL            * aStatistics,
                                        sctTableSpaceNode * aSpaceNode,
                                        void              * /*aActionArg*/ );

    static IDE_RC  freeSpaceCacheCommitPending(
                                        idvSQL               * /*aStatistics*/,
                                        sctTableSpaceNode    * aSpaceNode,
                                        sctPendingOp         * /*aPendingOp*/ );

    static IDE_RC refineDRDBSpaceCache(void);
    static IDE_RC doRefineSpaceCache( idvSQL            * /* aStatistics*/,
                                      sctTableSpaceNode * aSpaceNode,
                                      void              * /*aActionArg*/ );

    //  Tablespace���� ����ϴ� extent ���� ���� ��� ��ȯ
    static smiExtMgmtType getExtMgmtType( scSpaceID aSpaceID );

    /* Tablespace �������� ��Ŀ� ���� Segment �������� ��� ��ȯ */
    static smiSegMgmtType getSegMgmtType( scSpaceID aSpaceID );

    // Tablespace�� extent�� page���� ��ȯ�Ѵ�.
    static UInt getPagesPerExt( scSpaceID     aSpaceID );

    static IDE_RC checkPureFileSize(
                           smiDataFileAttr   ** aDataFileAttr,
                           UInt                 aDataFileAttrCount,
                           UInt                 aValidSmallSize );
};


#endif // _O_SDP_TABLE_SPACE_H_
