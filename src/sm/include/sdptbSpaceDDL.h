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
 * $Id: sdptbSpaceDDL.h 27220 2008-07-23 14:56:22Z newdaily $
 *
 * DDL�� ���õ� �Լ����̴�.
 ***********************************************************************/

# ifndef _O_SDPTB_SPACE_DDL_H_
# define _O_SDPTB_SPACE_DDL_H_ 1

#include <sdptbDef.h>   /* PROJ-1923 */

class sdptbSpaceDDL
{
public:
    static IDE_RC initialize(){ return IDE_SUCCESS; }
    static IDE_RC destroy(){ return IDE_SUCCESS; }

    /* ��� ������ tablespace ������������ ȣ��Ǵ� �Լ� */
    static IDE_RC createTBS(idvSQL             *aStatistics,
                            sdrMtxStartInfo    *aStartInfo,
                            smiTableSpaceAttr*  aTableSpaceAttr,
                            smiDataFileAttr**   aDataFileAttr,
                            UInt                aDataFileAttrCount );

    /* PROJ-1923
     * tablespace ���������� ���� redo�� ȣ��Ǵ� �Լ� */
    static IDE_RC createTBS4Redo( void                * aTrans,
                                  smiTableSpaceAttr   * aTableSpaceAttr );

    /* PROJ-1923
     * tablespace ���������� ���� redo�� ȣ��Ǵ� �Լ� */
    static IDE_RC createDBF4Redo( void            * aTrans,
                                  smLSN             aCurLSN,
                                  scSpaceID         aSpaceID,
                                  smiDataFileAttr * aTableSpaceAttr );

    /* [ INTERFACE ] User Tablespace ���� */
    static IDE_RC createUserTBS(idvSQL             *aStatistics,
                                void*               aTransForMtx,
                                smiTableSpaceAttr*  aTableSpaceAttr,
                                smiDataFileAttr**   aDataFileAttr,
                                UInt                aDataFileAttrCount );

    /* [ INTERFACE ] Temporary Tablespace ���� */
    static IDE_RC createTempTBS(idvSQL             *aStatistics,
                                void*               aTransForMtx,
                                smiTableSpaceAttr*  aTableSpaceAttr,
                                smiDataFileAttr**   aDataFileAttr,
                                UInt                aDataFileAttrCount );

    /* [ INTERFACE ] System Tablespace ���� */
    static IDE_RC createSystemTBS(idvSQL             *aStatistics,
                                  void*               aTransForMtx,
                                  smiTableSpaceAttr*  aTableSpaceAttr,
                                  smiDataFileAttr**   aDataFileAttr,
                                  UInt                aDataFileAttrCount );

    /* [ INTERFACE ] Undo Tablespace ���� */
    static IDE_RC createUndoTBS(idvSQL             *aStatistics,
                                void*               aTransForMtx,
                                smiTableSpaceAttr*  aTableSpaceAttr,
                                smiDataFileAttr**   aDataFileAttr,
                                UInt                aDataFileAttrCount );


    static IDE_RC resetTBSCore( idvSQL             *aStatistics,
                                void*               aTransForMtx,
                                scSpaceID           aSpaceID );


    static IDE_RC alterDataFileReSizeFEBT( idvSQL       *aStatistics,
                                           void         *aTrans,
                                           scSpaceID     aSpaceID,
                                           SChar        *aFileName,
                                           ULong         aSizeWanted,
                                           ULong        *aSizeChanged,
                                           SChar        *aValidDataFileName );

    static IDE_RC removeDataFile( idvSQL         *aStatistics,
                                  void*           aTrans,
                                  scSpaceID       aSpaceID,
                                  SChar          *aFileName,
                                  SChar          *aValidDataFileName );

    static IDE_RC createDataFilesFEBT(
                                    idvSQL           * aStatistics,
                                    void*              aTrans,
                                    scSpaceID          aSpaceID,
                                    smiDataFileAttr  **aDataFileAttr,
                                    UInt               aDataFileAttrCount );

    static IDE_RC alterDataFileAutoExtendFEBT(
                                        idvSQL             *aStatistics,
                                        void               *aTrans,
                                        scSpaceID           aSpaceID,
                                        SChar              *aFileName,
                                        idBool              aAutoExtend,
                                        ULong               aNextSize,
                                        ULong               aMaxSize,
                                        SChar              *aValidDataFileName);

    static void checkDataFileSize(  smiDataFileAttr**   aDataFileAttr,
                                    UInt                aDataFileAttrCount,
                                    UInt                aPagesPerExt );


    /* [INTERFACE] TSSRID�� ��Ʈ�Ѵ�. */
    static IDE_RC setTSSPID( idvSQL        * aStatistics,
                             sdrMtx        * aMtx,
                             scSpaceID       aSpaceID,
                             UInt            aIndex,
                             scPageID        aTSSPID )
    {
        return setPIDCore( aStatistics,
                           aMtx,
                           aSpaceID,
                           aIndex,
                           aTSSPID,
                           SDPTB_RID_TYPE_TSS );
    };

    /* [INTERFACE] TSSRID�� ���� */
    static IDE_RC getTSSPID( idvSQL        * aStatistics,
                             scSpaceID       aSpaceID,
                             UInt            aIndex,
                             scPageID      * aTSSPID )
    {
        return getPIDCore( aStatistics,
                           aSpaceID,
                           aIndex,
                           SDPTB_RID_TYPE_TSS,
                           aTSSPID);
    };

    /* [INTERFACE] UDSRID�� ��Ʈ�Ѵ�. */
    static IDE_RC setUDSPID( idvSQL        * aStatistics,
                             sdrMtx        * aMtx,
                             scSpaceID       aSpaceID,
                             UInt            aIndex,
                             scPageID        aUDSPID )
    {
        return setPIDCore( aStatistics,
                           aMtx,
                           aSpaceID,
                           aIndex,
                           aUDSPID,
                           SDPTB_RID_TYPE_UDS ) ;
    };

    /* [INTERFACE] UDSRID�� ���� */
    static IDE_RC getUDSPID( idvSQL        * aStatistics,
                             scSpaceID       aSpaceID,
                             UInt            aIndex,
                             scPageID      * aUDSPID )
    {
        return getPIDCore( aStatistics,
                           aSpaceID,
                           aIndex,
                           SDPTB_RID_TYPE_UDS,
                           aUDSPID);
    }


    static IDE_RC getSpaceNodeAndFileNode(
                          scSpaceID               aSpaceID,
                          SChar                 * aFileName,
                          sddTableSpaceNode    ** aSpaceNode,
                          sddDataFileNode      ** aFileNode,
                          SChar                 * aValidDataFileName );

    /* 
     * dropTBS, alterTBSStatus,alterTBSdiscard,alterDataFileName��
     * sdptl���� ������ �ڵ��̴�. ���Ŀ� ���ڵ���� ����ɼ��� �ִ�.
     */

    /* [ INTERFACE ] Tablespace ���� */
    static IDE_RC dropTBS( idvSQL       *aStatistics,
                           void*         aTrans,
                           scSpaceID     aSpaceID,
                           smiTouchMode  aTouchMode );

    // PRJ-1548 User Memory TableSpace - D1
    // Disk Tablespace�� ���� Alter Tablespace Online/Offline�� ����
    static IDE_RC alterTBSStatus( idvSQL*             aStatistics,
                                  void              * aTrans,
                                  sddTableSpaceNode * aSpaceNode,
                                  UInt                aState );

    /* [ INTERFACE ] Tablespace ��ȿȭ */
    static IDE_RC alterTBSdiscard( sddTableSpaceNode * aTBSNode );

    /* [ INTERFACE ] ����Ÿ���� ��� ���� */
    static IDE_RC alterDataFileName( idvSQL*      aStatistics,
                                     scSpaceID    aSpaceID,
                                     SChar       *aOldName,
                                     SChar       *aNewName );

    /* [ INTERFACE ] META/SERVICE�ܰ迡�� Tablespace�� Online���·� �����Ѵ�. */
    static IDE_RC alterTBSonline( idvSQL*              aStatistics,
                                  void               * aTrans,
                                  sddTableSpaceNode  * aSpaceNode );

    /* [ INTERFACE ] META/SERVICE�ܰ迡�� Tablespace�� Offline���·� �����Ѵ�.*/
    static IDE_RC alterTBSoffline( idvSQL*             aStatistics,
                                   void              * aTrans,
                                   sddTableSpaceNode * aSpaceNode );

    static IDE_RC alterOnlineCommitPending(
                          idvSQL            * aStatistics,
                          sctTableSpaceNode * aSpaceNode,
                          sctPendingOp      * aPendingOp );

    static IDE_RC alterOfflineCommitPending(
                          idvSQL            * aStatistics,
                          sctTableSpaceNode * aSpaceNode,
                          sctPendingOp      * aPendingOp );

    static IDE_RC alterAddFileCommitPending(
                          idvSQL            * aStatistics,
                          sctTableSpaceNode * aSpaceNode,
                          sctPendingOp      * aPendingOp );

    static IDE_RC alterDropFileCommitPending(
                          idvSQL            * aStatistics,
                          sctTableSpaceNode * aSpaceNode,
                          sctPendingOp      * aPendingOp );
private:


    static void alignSizeWithOSFileLimit( ULong *aAlignDest,
                                          UInt   aFileHdrPageCnt );

    /* RID�� ��Ʈ�ϴ� �ٽ��Լ� */
    static IDE_RC setPIDCore( idvSQL        * aStatistics,
                              sdrMtx        * aMtx,
                              scSpaceID       aSpaceID,
                              UInt            aIndex,
                              scPageID        aTSSPID,
                              sdptbRIDType    aRIDType);

    /* RID�� ���� �ٽ��Լ� */
    static IDE_RC getPIDCore( idvSQL        * aStatistics,
                              scSpaceID       aSpaceID,
                              UInt            aIndex,
                              sdptbRIDType    aRIDType,
                              scPageID      * aTSSPID );
};


#endif // _O_SDPTB_SPACE_DDL_H_
