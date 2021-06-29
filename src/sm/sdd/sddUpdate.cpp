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
 * $Id: sddUpdate.cpp 88020 2020-07-10 09:34:13Z et16 $
 *
 * Description :
 *
 * �� ������ FILE ������� redo/undo �Լ��� ���� ���������̴�.
 *
 **********************************************************************/

#include <smDef.h>
#include <sddDef.h>
#include <smErrorCode.h>
#include <sddDiskMgr.h>
#include <sddTableSpace.h>
#include <sddDataFile.h>
#include <sddUpdate.h>
#include <sddReq.h>
#include <sctTableSpaceMgr.h>
#include <sdptbSpaceDDL.h>

/*
PRJ-1548 User Memory Tablespace

SCT_UPDATE_DRDB_CREATE_TBS �α� �����

Ʈ����� Commit Pending List: [POP_DBF]->[POP_TBS] // ���� ����

���� :   (1)           (2)            (3)      (4)        (5)       (6)       (7)        (8)
���� : [CREATE_TBS]->[CREATE_DBF]->[ANCHOR]->[COMMIT]->[POP_DBF]->[ANCHOR]->[POP_TBS]->[ANCHOR]
���� :  |CREATEING     |CREATING     ����      Ŀ��      ONLINE     ����     ONLINE     ����
        |ONLINE        |ONLINE                          ~CREATING           ~CREATING

�˰���

��. (3) ������ �����ϸ� Ŀ���� �ȵ� Ʈ������̰�, Loganchor���� ������� �ʾұ� ������
    TBS List���� �˻��� �ȵǸ�, ������� ���� ����.

��. (3)�� (4) ���̿� �����ϸ� Ŀ���� �ȵ� Ʈ�����������, �α׾�Ŀ�� ONLINE|CREATING ���·�
    ����Ǿ����Ƿ�, Rollback Pending ������ ���� DROPPED�� �������־�� �Ѵ�.

��. (7)�� (8) ���̿��� �����ϸ� Ŀ�Ե� Ʈ�����������, �α׾�Ŀ�� ONLINE|CREATING ���·�
    ����Ǿ����Ƿ�, ONLINE|CREATING ������ ��쿡�� Commit Pending ������ ����Ͽ�
    ONLINE ���·� �������־�� �Ѵ�.

��. (8) ���Ŀ� �����ϸ� Ŀ�Ե� Ʈ������̰�, �α׾�Ŀ�� ONLINE�� ���·� ����Ǿ����Ƿ�
    ����� �� ���� ����.

PROJ-1923 ALTIBASE HDB Disaster Recovery
�� ����� ���� ������ redo�� �����Ѵ�.

    => ���� ��
��. (3) ������ �����ϸ� Ŀ�� �ȵ� Ʈ������̰�, �α׾�Ŀ���� ����Ǿ� ���� �ʴ�.
    ���� ó������ Create TBS�� �����Ѵ�. (Ŀ���� �����Ƿ� ���� Rollback �ȴ�.)
*/
IDE_RC sddUpdate::redo_SCT_UPDATE_DRDB_CREATE_TBS(
                                            idvSQL    * /* aStatistics */,
                                            void      * aTrans,
                                            smLSN       /* aCurLSN */,
                                            scSpaceID   aSpaceID,
                                            UInt        /* aFileID */,
                                            UInt        aValueSize,
                                            SChar     * aValuePtr,
                                            idBool      /* aIsRestart */ )
{
    sddTableSpaceNode * sSpaceNode;
    smiTableSpaceAttr   sTableSpaceAttr;
    scSpaceID           sNewSpaceID = 0;

    IDE_ERROR( aTrans != NULL );
    IDE_ERROR_MSG( aValueSize == ID_SIZEOF(smiTableSpaceAttr),
                   "aValuesSize : %"ID_UINT32_FMT,
                   aValueSize );

    /* Loganchor�κ��� �ʱ�ȭ�� TBS List�� �˻��Ѵ�. */
    sSpaceNode = (sddTableSpaceNode*)sctTableSpaceMgr::findSpaceNodeWithoutException( aSpaceID );

    if( sSpaceNode != NULL )
    {
        IDE_ERROR( (sSpaceNode->mHeader.mState & SMI_TBS_DROPPED)
                     != SMI_TBS_DROPPED );

        if ( SMI_TBS_IS_CREATING(sSpaceNode->mHeader.mState) )
        {
            /* �˰��� (��)�� �ش��ϴ� CREATINIG ������ ��쿡�� �����Ƿ�
             * ���¸� ONLINE���� ������ �� �ְ� Commit Pending ������ ����Ѵ�. */
            if ( smLayerCallback::isBeginTrans( aTrans ) == ID_TRUE )
            {
                IDE_TEST( sctTableSpaceMgr::addPendingOperation(
                                                aTrans,
                                                aSpaceID,
                                                ID_TRUE, /* commit�ÿ� ���� */
                                                SCT_POP_CREATE_TBS )
                          != IDE_SUCCESS );
            }
            else
            {
                /* Active Tx�� �ƴѰ�� Pendig ������� �ʴ´�. */
            }

            /* �˰��� (��)�� �ش��ϴ� ���� Rollback Pending �����̱� ������
             * undo_SCT_UPDATE_DRDB_CREATE_TBS()���� POP_DROP_TBS ���� ����Ѵ�. */
        }
        else
        {
            /* �˰��� (��) �� �ش��ϹǷ� ��������� �ʴ´�. */
        }
    }
    else
    {
        sNewSpaceID = sctTableSpaceMgr::getNewTableSpaceID();

        /* �α׿��� �о�� spaceID�� ��Ÿ���� �о�� newSpaceID��
         * ������ (��)�� �ش��ϴ� ��� �� redo�� �����ؾ� �Ѵ�. */
        if( aSpaceID == sNewSpaceID )
        {
            /* PROJ-1923 ALTIBASE HDB Disaster Recovery
             * �� ����� �����ϱ� ���� �˰��� (��) �� �ش��ϴ� ��쿡��
             * ������� �����Ѵ�. */
            idlOS::memcpy( (void *)&sTableSpaceAttr,
                           aValuePtr,
                           ID_SIZEOF(smiTableSpaceAttr) );

            // sdptbSpaceDDL::createTBS() �� �����Ͽ� redo �Ѵ�.
            IDE_TEST( sdptbSpaceDDL::createTBS4Redo( aTrans,
                                                     &sTableSpaceAttr )
                      != IDE_SUCCESS );
        }
        else
        {
            // do nothing
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*
PRJ-1548 User Memory Tablespace

SCT_UPDATE_DRDB_CREATE_TBS �α� UNDO

Ʈ����� Rollback Pending List: [POP_DBF]->[POP_TBS] // ��������


���� :   (1)           (2)            (3)      (4)        (5)        (6)
���� : [CREATE_TBS]->[CREATE_DBF]->[ANCHOR]->[CLR_DBF]->[CLR_TBS]->[ROLLBACK]->
���� :  |CREATING     |CREATING      ����      |DROPPING |DROPPING
        |ONLINE       |ONLINE                  |ONLINE   |ONLINE
                                               |CREATING |CREATING

���� :  (7)          (8)       (9)      (10)
���� : [POP_DBF]->[ANCHOR]->[POP_TBS]->[ANCHOR]
���� :  DROPPED    ����     DROPPED     ����
        ~ONLINE  (DBF����)  ~ONLINE
        ~CREATING           ~CREATING
        ~DROPPING           ~DROPPING

���� �˰���

RESTART��

��. (3)undo�� �����ϸ� �Ϸᰡ �ȵ� Ʈ�����������, �α׾�Ŀ�� ONLINE|CREATING �����̱�
    ������, ONLINE|CREATING|DROPPING ���·� �����ϰ�, Rollback Pending ������ ����Ͽ� DROPPED��
    �������־�� �Ѵ�.

��. (9)�� (10) ���̿��� �����ϸ� Ŀ�Ե� Ʈ�����������, �α׾�Ŀ�� ONLINE|CREATING ���·�
    ����Ǿ����Ƿ�, Rollback Pending ������ ����Ͽ� DROPPED ���·� �������־�� �Ѵ�.

��. (10) ���Ŀ� �����ϸ� Ŀ�Ե� Ʈ������̰�, �α׾�Ŀ�� DROPPED�� ���·� ����Ǿ����Ƿ�
    �˻��� ���� ������, undo�� �͵� ����.

RUNTIME��

��. (1)�� (2) ���̿��� �����ϸ� TBS List���� �˻��� �ȵǴ� ��쿡�� ������� ���� ����.

��. (3)���� �����ϸ� TBS List���� �˻��� �ǹǷ�, ONLINE|CREATING|DROPPING �����ϰ�
    Rollback Pending ������ ����Ͽ� DROPPED�� �����Ѵ�.

*/
IDE_RC sddUpdate::undo_SCT_UPDATE_DRDB_CREATE_TBS(
                    idvSQL    * /* aStatistics */,
                    void      * aTrans,
                    smLSN       /* aCurLSN */,
                    scSpaceID   aSpaceID,
                    UInt        /* aFileID */,
                    UInt        aValueSize,
                    SChar     * /* aValuePtr */,
                    idBool      aIsRestart )
{
    sddTableSpaceNode * sSpaceNode;

    IDE_ERROR_MSG( aValueSize == 0,
                   "aValuesSize : %"ID_UINT32_FMT,
                   aValueSize );
    IDE_ERROR( (aTrans      != NULL) ||
               (aIsRestart  == ID_TRUE) );

    /* TBS List�� �˻��Ѵ�. */
    sSpaceNode = (sddTableSpaceNode*)sctTableSpaceMgr::findSpaceNodeWithoutException( aSpaceID );

    /* RUNTIME�ÿ��� sSpaceNode ��ü�� ���ؼ� (X) ����� �����ֱ� ������
     * sctTableSpaceMgr::lock�� ȹ���� �ʿ䰡 ����. */
    if( sSpaceNode != NULL )
    {
        IDE_ERROR( (sSpaceNode->mHeader.mState & SMI_TBS_DROPPED)
                   != SMI_TBS_DROPPED );

        if ( SMI_TBS_IS_CREATING(sSpaceNode->mHeader.mState) )
        {
            /* CREATE TBS ���꿡���� ��� �������� �����ϴ���
             * Loganchor�� DROPPING���°� ����� �� �����Ƿ�
             * RESTART�ÿ��� DROPPING ���°� ���� �� ����. */
            IDE_ERROR( (sSpaceNode->mHeader.mState & SMI_TBS_DROPPING)
                       != SMI_TBS_DROPPING );

            /* RESTART �˰��� (��),(��)�� �ش��Ѵ�.
             * RUNTIME �˰��� (��)�� �ش��Ѵ�. */
            IDE_TEST( sctTableSpaceMgr::addPendingOperation(
                                          aTrans,
                                          sSpaceNode->mHeader.mID,
                                          ID_FALSE, /* abort �� ���� */
                                          SCT_POP_DROP_TBS) != IDE_SUCCESS );

            sSpaceNode->mHeader.mState |= SMI_TBS_DROPPING;
        }
        else
        {
            /* �˰��� RESTART (��)�� ����ȴ�.
             * nothing to do ... */
            IDE_ERROR( 0 );
        }

    }
    else
    {
        /* RESTART �˰��� (��) �ش�
         * RUNTIME �˰��� (��) �ش�
         * nothing to do ... */
    }

    /* RUNTIME�ÿ� ������ �߻��ߴٸ� Rollback Pending�� ��ϵǾ��� ���̰�
     * Rollback Pending�� Loganchor�� �����Ѵ�. */
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*
PRJ-1548 User Memory Tablespace

SCT_UPDATE_DRDB_DROP_TBS �α� �����

Ʈ����� Pending List: [POP_DBF]->[POP_TBS] // ��������

���� :   (1)           (2)      (3)        (4)       (5)       (6)        (7)
���� : [DROP_DBF]->[DROP_TBS]->[COMMIT]->[POP_DBF]->[ANCHOR]->[POP_TBS]->[ANCHOR]
���� :  |DROPPING  |DROPPING   Ŀ��     |DROPPED     ����      DROPPED     ����
        |ONLINE    |ONLINE              |ONLINE     (dbf����)

���� �˰���

��. (3) ������ �����ϸ� Ŀ���� �ȵ� Ʈ������̰�, Loganchor���� ������� �ʾұ� ������
    TBS List���� �˻��� �Ǹ� ONLINE|DROPPING ���·� �����ϰ�, Commit Pending
    (DROPPING�� �ƴϸ� ����) ������ ����Ͽ� DROPPED ���·� �������־�� �Ѵ�.

��. (6)�� (7) ���̿��� �����ϸ� Ŀ�Ե� Ʈ�����������, �α׾�Ŀ�� ONLINE ���·�
    ����Ǿ� �ֱ� ������ ONLINE|DROPPING���·� �����ϰ�, Commit Pending ������ ����Ͽ�
    DROPPED ���·� �������־�� �Ѵ�.

��. (7) ���Ŀ� �����ϸ� Ŀ�Ե� Ʈ������̰�, ���°� DROPPED�� �Ǿ� Loganchor��
    ������� �����Ƿ�, TBS List���� �˻����� �ʴ´�.

*/
IDE_RC sddUpdate::redo_SCT_UPDATE_DRDB_DROP_TBS( idvSQL       * /* aStatistics */,
                                                 void         * aTrans,
                                                 smLSN          /* aCurLSN */,
                                                 scSpaceID      aSpaceID,
                                                 UInt           /* aFileID */,
                                                 UInt           aValueSize,
                                                 SChar        * /* aValuePtr */,
                                                 idBool         /* aIsRestart */ )
{
    sddTableSpaceNode     * sSpaceNode;

    IDE_ERROR( aTrans       != NULL );
    IDE_ERROR( aValueSize   == 0 );

    // TBS List���� �˻��Ѵ�.
    sSpaceNode = (sddTableSpaceNode*)sctTableSpaceMgr::findSpaceNodeWithoutException( aSpaceID );

    if( sSpaceNode != NULL )
    {
        // PRJ-1548 User Memory Tablespace
        // DROP TBS ������ commit�� �ƴϱ� ������ DROPPED��
        // �����ϸ� ���õ� �α׷��ڵ带 ������� �� ����.
        // RESTART RECOVERY�� Commit Pending Operation�� �����Ͽ�
        // �� ���׸� �����Ѵ�.
        // SCT_UPDATE_DRDB_DROP_TBS ������� �� ��쿡�� DROPPING
        // ���·� �����ϰ�, �ش� Ʈ������� COMMIT �α׸� ������� ��
        // Commit Pending Operation���� DROPPED ���·� �����Ѵ�.

        if ( (sSpaceNode->mHeader.mState & SMI_TBS_DROPPING)
             != SMI_TBS_DROPPING )
        {
            if ( smLayerCallback::isBeginTrans( aTrans ) == ID_TRUE )
            {
                sSpaceNode->mHeader.mState |= SMI_TBS_DROPPING;

                // �˰��� (��), (��)�� �ش��ϴ� ��� Commit Pending ���� ���
                IDE_TEST( sctTableSpaceMgr::addPendingOperation(
                                            aTrans,
                                            aSpaceID,
                                            ID_TRUE, /* commit�ÿ� ���� */
                                            SCT_POP_DROP_TBS )
                          != IDE_SUCCESS );
            }
            else
            {
                // Active Tx�� �ƴ� ��쿡�� Pending ������ �߰����� �ʴ´�.
            }
        }
        else
        {
            // nothing to do ..
        }
    }
    else
    {
        // �˰��� (��)�� �ش��ϴ� ��� TBS List���� �˻��� ���� ������
        // ��������� �ʴ´�.
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*
PRJ-1548 User Memory Tablespace

SCT_UPDATE_DRDB_DROP_TBS �α� UNDO

���� :   (1)           (2)     (3)         (4)        (5)
���� : [DROP_DBF]->[DROP_TBS]->[CLR_TBS]->[CLR_DBF]->[ROLLBACK]
���� :  |ONLINE    |ONLINE     ~DROPPING   ~DROPPING
        |DROPPING  |DROPPING

���� �˰���

RESTART��

��. (2)���Ŀ� ������ ���, TBS List���� �˻��ȴٸ� (2)�� ������Ͽ�
    ONLINE|DROPPING �����̱� ������, ~DROPPING ������ �Ͽ� ONLINE ���·� �����Ѵ�.

RUNTIME��

��. (2)���Ŀ� �����ϸ� TBS List���� �˻��Ͽ� ~DROPPING ������ �����Ͽ� ONLINE ���·�
    �����Ѵ�.

*/
IDE_RC sddUpdate::undo_SCT_UPDATE_DRDB_DROP_TBS(
                       idvSQL *             /*aStatistics*/,
                       void*                aTrans,
                       smLSN                /* aCurLSN */,
                       scSpaceID            aSpaceID,
                       UInt               /*aFileID */,
                       UInt                 aValueSize,
                       SChar*             /*aValuePtr */,
                       idBool               aIsRestart )
{
    sddTableSpaceNode*  sSpaceNode;

    IDE_ERROR( (aTrans != NULL) || (aIsRestart == ID_TRUE) );
    IDE_ERROR( aValueSize == 0 );

    sSpaceNode = (sddTableSpaceNode*)sctTableSpaceMgr::findSpaceNodeWithoutException( aSpaceID );

    // RUNTIME�ÿ��� sSpaceNode ��ü�� ���ؼ� (X) ����� �����ֱ� ������
    // sctTableSpaceMgr::lock�� ȹ���� �ʿ䰡 ����.
    if( sSpaceNode != NULL )
    {
        if( SMI_TBS_IS_DROPPING(sSpaceNode->mHeader.mState) )
        {
            // �˰��� RESTART (��), RUNTIME (��) �� �ش��ϴ� ����̴�.
            // DROPPING�� ����, ONLINE ���·� �����Ѵ�.
            sSpaceNode->mHeader.mState &= ~SMI_TBS_DROPPING;
        }

        IDE_ERROR( (sSpaceNode->mHeader.mState & SMI_TBS_DROPPED )
                   != SMI_TBS_DROPPED );
        IDE_ERROR( (sSpaceNode->mHeader.mState & SMI_TBS_CREATING)
                   != SMI_TBS_CREATING );
    }
    else
    {
        // TBS List���� �˻��� ���� ������ �ƹ��͵� ���� �ʴ´�.
        // nothing to do...
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*
PRJ-1548 User Memory Tablespace

SCT_UPDATE_DRDB_CREATE_DBF �α� �����

Ʈ����� Commit Pending List: [POP_DBF]->[POP_TBS]

���� :   (1)           (2)            (3)      (4)        (5)       (6)       (7)        (8)
���� : [CREATE_TBS]->[CREATE_DBF]->[ANCHOR]->[COMMIT]->[POP_DBF]->[ANCHOR]->[POP_TBS]->[ANCHOR]
���� :  |CREATEING    |CREATING     ����      Ŀ��      ONLINE     ����     ONLINE     ����
        |ONLINE       |ONLINE                          ~CREATING           ~CREATING

�˰���

��. (3) ������ �����ϸ� Ŀ���� �ȵ� Ʈ������̰�, Loganchor���� ������� �ʾұ� ������
    TBS List���� �˻��� �ȵǸ�, ������� ���� ����.

��. (3)�� (4) ���̿� �����ϸ� Ŀ���� �ȵ� Ʈ�����������, �α׾�Ŀ�� ONLINE|CREATING ���·�
    ����Ǿ����Ƿ�, Rollback Pending ������ ���� DROPPED�� �������־�� �Ѵ�.

��. (5)�� (6) ���̿��� �����ϸ� Ŀ�Ե� Ʈ�����������, �α׾�Ŀ�� ONLINE|CREATING ���·�
    ����Ǿ����Ƿ�, ONLINE|CREATING ������ ��쿡�� Commit Pending ������ ����Ͽ�
    ONLINE ���·� �������־�� �Ѵ�.

��. (6) ���Ŀ� �����ϸ� Ŀ�Ե� Ʈ������̰�, �α׾�Ŀ�� ONLINE�� ���·� ����Ǿ����Ƿ�
    ����� �� ���� ����.

# CREATE_TBS�� �����˰���� ���� �����ϴ�.

*/
IDE_RC sddUpdate::redo_SCT_UPDATE_DRDB_CREATE_DBF( idvSQL     * /* aStatistics */,
                                                   void       * aTrans,
                                                   smLSN        aCurLSN,
                                                   scSpaceID    aSpaceID,
                                                   UInt         aFileID,
                                                   UInt         aValueSize,
                                                   SChar      * aValuePtr,
                                                   idBool       /* aIsRestart */)
{
    sctPendingOp      * sPendingOp;
    smiTouchMode        sTouchMode;
    sddTableSpaceNode * sSpaceNode;
    sddDataFileNode   * sFileNode;
    smiDataFileAttr     sDataFileAttr;

    IDE_ERROR( aTrans       != NULL );
    IDE_ERROR( aValuePtr    != NULL );
    IDE_ERROR_MSG( aValueSize == ( ID_SIZEOF(smiTouchMode) +
                                   ID_SIZEOF(smiDataFileAttr) ),
                   "aValueSize : %"ID_UINT32_FMT,
                   aValueSize );

    /* Loganchor�κ��� �ʱ�ȭ�� TBS List�� �˻��Ѵ�. */
    IDE_TEST( getTBSDBF( aSpaceID, aFileID, &sSpaceNode, &sFileNode )
              != IDE_SUCCESS );

    /* PROJ-1923 ALTIBASE HDB Disaster Recovery
     * �˰��� (��)�� �ش��ϴ� ����̳�,
     * SCT_UPDATE_DRDB_CREATE_TBS�� �׻� Redo �ϹǷ�,
     * SCT_UPDATE_DRDB_CREATE_DBF�� Redo �Ҷ� �ش� TBS�� ��ã���� ���� ��Ȳ */
    IDE_TEST_RAISE( sSpaceNode == NULL, err_tablespace_does_not_exist );

    IDE_ERROR( (sSpaceNode->mHeader.mState & SMI_TBS_DROPPED)
               != SMI_TBS_DROPPED );

    if ( sFileNode != NULL )
    {
        if ( SMI_FILE_STATE_IS_CREATING( sFileNode->mState ) )
        {
            if ( smLayerCallback::isBeginTrans( aTrans ) == ID_TRUE )
            {
                idlOS::memcpy( &sTouchMode,
                               aValuePtr,
                               ID_SIZEOF(smiTouchMode) );

                /* �˰��� (��)�� �ش��ϴ� CREATINIG ������ ��쿡��
                 * �����Ƿ� ���¸� ONLINE���� ������ �� �ְ�
                 * Commit Pending ������ ����Ѵ�. */
                IDE_TEST( sddDataFile::addPendingOperation(
                        aTrans,
                        sFileNode,
                        ID_TRUE, /* commit�ÿ� ���� */
                        SCT_POP_CREATE_DBF,
                        &sPendingOp ) != IDE_SUCCESS );

                sPendingOp->mTouchMode = sTouchMode;
            }
            else
            {
                /* Active Tx�� �ƴ� ��쿡�� Pending ������
                 * �߰����� �ʴ´�. */
            }

            /* �˰��� (��)�� �ش��ϴ� ���� Rollback Pending�����̱� ������
             * undo_SCT_UPDATE_DRDB_CREATE_DBF()���� POP_DROP_DBF���� ����Ѵ�. */
        }
        else
        {
            /* �˰��� (��) �� �ش��ϹǷ� ��������� �ʴ´�. */
        }
    }
    else
    {
        /* PROJ-1923 ALTIBASE HDB Disaster Recovery
         * �� ����� �����ϱ� ���� �˰��� (��) �� �ش��ϴ� ��� ������
         * ������� �����Ѵ�.
         * ������ DBF ������ �ִٸ� �����ϰ� ���� ���� �ȴ�.
         * �ֳ��ϸ� �α׾�Ŀ�� ��ϵǾ� ���� �����Ƿ� ���� ���ϰ� ����. */
        idlOS::memcpy( (void *)&sDataFileAttr,
                       aValuePtr + ID_SIZEOF(smiTouchMode) ,
                       ID_SIZEOF(smiDataFileAttr) );
        
        // sdptbSpaceDDL::createTBS() �� �����Ͽ� redo �Ѵ�.
        IDE_TEST( sdptbSpaceDDL::createDBF4Redo( aTrans,
                                                 aCurLSN,
                                                 aSpaceID,
                                                 &sDataFileAttr )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( err_tablespace_does_not_exist );
    {
        IDE_SET( ideSetErrorCode( smERR_ABORT_TablespaceDoesNotExist, aSpaceID ) );
    }

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*
PRJ-1548 User Memory Tablespace

SCT_UPDATE_DRDB_CREATE_DBF �α� UNDO

Ʈ����� Pending List: [POP_TBS]->[POP_DBF]


���� :   (1)           (2)            (3)      (4)        (5)        (6)
���� : [CREATE_TBS]->[CREATE_DBF]->[ANCHOR]->[CLR_DBF]->[CLR_TBS]->[ROLLBACK]->
���� :  CREATING      |CREATING     ����      |DROPPING  |DROPPING
                      |ONLINE                 |ONLINE    |CREATING
                                              |CREATING
���� :  (7)          (8)       (9)      (10)
���� : [POP_DBF]->[ANCHOR]->[POP_TBS]->[ANCHOR]
���� :  |DROPPED    DBF ����  DROPPED   TBS ����
        |ONLINE    (���ϻ���)

���� �˰���

RESTART��

��. (3)���� undo�� �����ϸ� �Ϸᰡ �ȵ� Ʈ�����������, �α׾�Ŀ�� ONLINE|CREATING �����̱�
    ������, ONLINE|CREATING|DROPPING ���·� �����ϰ�, Rollback Pending ������ ����Ͽ� DROPPED��
    �������־�� �Ѵ�.

��. (7)�� (8) ���̿��� �����ϸ� Ŀ�Ե� Ʈ�����������, �α׾�Ŀ�� ONLINE|CREATING ���·�
    ����Ǿ����Ƿ�, Rollback Pending ������ ����Ͽ� DROPPED ���·� �������־�� �Ѵ�.

��. (8) ���Ŀ� �����ϸ� Ŀ�Ե� Ʈ������̰�, �α׾�Ŀ�� DROPPED�� ���·� ����Ǿ����Ƿ�
    �˻��� ���� ������, undo�� �͵� ����.

RUNTIME��

��. (2)�� (3) ���̿��� �����ϸ� TBS List���� �˻��� �ȵǴ� ��쿡�� ������� ���� ����.

��. (3)���� �����ϸ� TBS List���� �˻��� �ǹǷ�, ONLINE|CREATING|DROPPING �����ϰ�
    Rollback Pending ������ ����Ͽ� DROPPED�� �����Ѵ�.

# CREATE_TBS�� �����˰���� ���� �����ϴ�.
*/
IDE_RC sddUpdate::undo_SCT_UPDATE_DRDB_CREATE_DBF( idvSQL     * aStatistics,
                                                   void       * aTrans,
                                                   smLSN        /* aCurLSN */,
                                                   scSpaceID    aSpaceID,
                                                   UInt         aFileID,
                                                   UInt         aValueSize,
                                                   SChar      * aValuePtr,
                                                   idBool       aIsRestart )
{
    UInt                sState = 0;
    smiTouchMode        sTouchMode;
    sddTableSpaceNode * sSpaceNode;
    sddDataFileNode   * sFileNode;
    sctPendingOp      * sPendingOp;

    IDE_ERROR( aValuePtr    != NULL );
    IDE_ERROR( (aTrans      != NULL) ||
               (aIsRestart  == ID_TRUE) );
    IDE_ERROR_MSG( aValueSize == ID_SIZEOF(smiTouchMode),
                   "aValueSize : %"ID_UINT32_FMT,
                   aValueSize );

    IDE_TEST( getTBSDBF( aSpaceID, aFileID, &sSpaceNode, &sFileNode )
              != IDE_SUCCESS );

    /* TBS Node�� (X) ����� ȹ���߰ų�, DBF Node�� (X)����� ȹ���� ���Ķ�
     * sctTableSpaceMgr::lock()�� ȹ������ �ʴ´�. */
    if (( sSpaceNode != NULL ) &&
        ( sFileNode  != NULL ))
    {
        /* creating �߿� �� ���� �ٸ��̰� �������� ����
         * �ݴ�� �׷��� ������ ���ü� ���� ����.
         * �����ϰ� ��´�.*/
        sctTableSpaceMgr::lockSpaceNode( aStatistics,
                                         sSpaceNode );
        sState = 1;

        /* �˰��� RESTART (��)�� ����ȴ�. nothing to do ... */
        IDE_ERROR( SMI_FILE_STATE_IS_CREATING( sFileNode->mState ) );

        idlOS::memcpy( &sTouchMode,
                       aValuePtr,
                       ID_SIZEOF(smiTouchMode) );

        /* CREATE TBS ���꿡���� ��� �������� �����ϴ���
         * Loganchor�� DROPPING���°� ����� �� �����Ƿ�
         * RESTART�ÿ��� DROPPING ���°� ���� �� ����. */
        IDE_ERROR( SMI_FILE_STATE_IS_NOT_DROPPING( sFileNode->mState ) );

        /* RESTART �˰��� (��),(��)�� �ش��Ѵ�.
         * RUNTIME �˰��� (��)�� �ش��Ѵ�. */
        IDE_TEST( sddDataFile::addPendingOperation(
                      aTrans,
                      sFileNode,
                      ID_FALSE, /* abort �� ���� */
                      SCT_POP_DROP_DBF,
                      &sPendingOp ) != IDE_SUCCESS );

        sPendingOp->mPendingOpFunc  = sddDiskMgr::removeFilePending;
        sPendingOp->mPendingOpParam = (void*)sFileNode;
        sPendingOp->mTouchMode      = sTouchMode;

        sFileNode->mState |= SMI_FILE_DROPPING;

        sState = 0;
        sctTableSpaceMgr::unlockSpaceNode( sSpaceNode );
    }
    else
    {
        /* RESTART �˰��� (��) �ش�
         * RUNTIME �˰��� (��) �ش�
         * nothing to do ... */
    }

    /* RUNTIME�ÿ� ������ �߻��ߴٸ� Rollback Pending�� ��ϵǾ��� ���̰�
     * Rollback Pending�� Loganchor�� �����Ѵ�. */
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if ( sState != 0 )
    {
        /* BUG-47982 ���� ó������ ������ mutex�� unlock �մϴ�. */
        sctTableSpaceMgr::unlockSpaceNode( sSpaceNode );
    }

    return IDE_FAILURE;
}

/*
PRJ-1548 User Memory Tablespace

SCT_UPDATE_DRDB_DROP_DBF �α� �����

Ʈ����� Pending List: [POP_DBF]->[POP_TBS]

���� :   (1)           (2)      (3)        (4)       (5)       (6)        (7)
���� : [DROP_DBF]->[DROP_TBS]->[COMMIT]->[POP_DBF]->[ANCHOR]->[POP_TBS]->[ANCHOR]
���� :  |DROPPING  |DROPPING   Ŀ��     |DROPPED     ����      DROPPED     ����
        |ONLINE    |ONLINE              |ONLINE

���� �˰���

��. (3) ������ �����ϸ� Ŀ���� �ȵ� Ʈ������̰�, Loganchor���� ������� �ʾұ� ������
    TBS List���� �˻��� �Ǹ� ONLINE|DROPPING ���·� �����ϰ�, Commit Pending ������ ����Ͽ�
    DROPPED ���·� �������־�� �Ѵ�.

��. (4)�� (5) ���̿��� �����ϸ� Ŀ�Ե� Ʈ�����������, �α׾�Ŀ�� ONLINE ���·�
    ����Ǿ� �ֱ� ������ ONLINE|DROPPING���·� �����ϰ�, Commit Pending ������ ����Ͽ�
    DROPPED ���·� �������־�� �Ѵ�.

��. (7) ���Ŀ� �����ϸ� Ŀ�Ե� Ʈ������̰�, ���°� DROPPED�� �Ǿ� Loganchor��
    ������� �����Ƿ�, TBS List���� �˻����� �ʴ´�.

# DROP_TBS�� �����˰���� ���� �����ϴ�.
*/
IDE_RC sddUpdate::redo_SCT_UPDATE_DRDB_DROP_DBF( idvSQL       * /* aStatistics */,
                                                 void         * aTrans,
                                                 smLSN          /* aCurLSN */,
                                                 scSpaceID      aSpaceID,
                                                 UInt           aFileID,
                                                 UInt           aValueSize,
                                                 SChar        * aValuePtr,
                                                 idBool         /* aIsRestart */ )
{
    sctPendingOp      * sPendingOp;
    SChar             * sValuePtr;
    sddTableSpaceNode * sSpaceNode;
    sddDataFileNode   * sFileNode;
    smiTouchMode        sTouchMode;

    IDE_ERROR( aTrans       != NULL );
    IDE_ERROR( aValuePtr    != NULL );
    IDE_ERROR_MSG( aValueSize == ID_SIZEOF(smiTouchMode),
                   "aValueSize : %"ID_UINT32_FMT,
                   aValueSize );

    sValuePtr = aValuePtr;
    idlOS::memcpy(&sTouchMode, sValuePtr, ID_SIZEOF(smiTouchMode));

    IDE_TEST( getTBSDBF( aSpaceID, aFileID, &sSpaceNode, &sFileNode )
              != IDE_SUCCESS );

    if (( sSpaceNode != NULL ) &&
        ( sFileNode  != NULL ))
    {
        // PRJ-1548 User Memory Tablespace
        // DROP DBF ������ commit�� �ƴϱ� ������ DROPPED��
        // �����ϸ� ���� �α׷��ڵ带 ������� �� ����.
        // RESTART RECOVERY�� Commit Pending Operation�� �����Ͽ�
        // �� ���׸� �����Ѵ�.
        // SCT_UPDATE_DRDB_DROP_DBF ������� �� ��쿡�� DROPPING
        // ���·� �����ϰ�, �ش� Ʈ������� COMMIT �α׸� ������� ��
        // Commit Pending Operation���� DROPPED ���·� �����Ѵ�.
        // for fix BUG-14978

        // Commit Pending Operation ����Ѵ�.
        if ( smLayerCallback::isBeginTrans( aTrans ) == ID_TRUE )
        {
            IDE_TEST( sddDataFile::addPendingOperation(
                          aTrans,
                          sFileNode,
                          ID_TRUE, /* commit�� ���� */
                          SCT_POP_DROP_DBF,
                          &sPendingOp )
                      != IDE_SUCCESS );

            sPendingOp->mPendingOpFunc  = sddDiskMgr::removeFilePending;
            sPendingOp->mPendingOpParam = (void*)sFileNode;
            sPendingOp->mTouchMode      = sTouchMode;

            // DBF Node�� ���¸� DROPPING���� �����Ѵ�.
            sFileNode->mState |= SMI_FILE_DROPPING;
        }
        else
        {
            // Active Tx�� �ƴ� ��쿡�� Pending ������
            // �߰����� �ʴ´�.
        }
    }
    else
    {
        // noting to do..
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*
PRJ-1548 User Memory Tablespace

SCT_UPDATE_DRDB_DROP_DBF �α� UNDO

���� :   (1)           (2)     (3)         (4)        (5)
���� : [DROP_DBF]->[DROP_TBS]->[CLR_TBS]->[CLR_DBF]->[ROLLBACK]
���� :  |ONLINE    |ONLINE     ~DROPPING   ~DROPPING
        |DROPPING  |DROPPING

���� �˰���

RESTART��

��. (1)���Ŀ� ������ ���, TBS List���� �˻��ȴٸ� (1)�� ������Ͽ�
    ONLINE|DROPPING �����̱� ������, ~DROPPING ������ �Ͽ� ONLINE ���·� �����Ѵ�.

RUNTIME��

��. (1)���Ŀ� �����ϸ� TBS List���� �˻��Ͽ� ~DROPPING ������ �����Ͽ� ONLINE ���·�
    �����Ѵ�.

# DROP_TBS�� �����˰���� ���� �����ϴ�.

*/
IDE_RC sddUpdate::undo_SCT_UPDATE_DRDB_DROP_DBF(
                       idvSQL *             aStatistics,
                       void*                aTrans,
                       smLSN                /* aCurLSN */,
                       scSpaceID            aSpaceID,
                       UInt                 aFileID,
                       UInt                 aValueSize,
                       SChar*               /* aValuePtr */,
                       idBool               aIsRestart )
{
    UInt                sState = 0;
    sddDataFileNode*    sFileNode = NULL;
    sddTableSpaceNode*  sSpaceNode = NULL;

    IDE_ERROR( aValueSize == 0 );
    IDE_ERROR( (aTrans != NULL) || (aIsRestart == ID_TRUE) );

    IDE_TEST( getTBSDBF( aSpaceID, aFileID, &sSpaceNode, &sFileNode )
              != IDE_SUCCESS );

    // RUNTIME�ÿ��� sSpaceNode ��ü�� ���ؼ� (X) ����� �����ֱ� ������
    // sctTableSpaceMgr::lock�� ȹ���� �ʿ䰡 ����.
    if (( sSpaceNode != NULL ) &&
        ( sFileNode  != NULL ))
    {
        // FileNode���� �� lock�� ��ƾ� �Ѵ�.
        sctTableSpaceMgr::lockSpaceNode( aStatistics,
                                         sSpaceNode );
        sState = 1;

        if( SMI_FILE_STATE_IS_DROPPING( sFileNode->mState ) )
        {
            // �˰��� RESTART (��), RUNTIME (��) �� �ش��ϴ� ����̴�.
            // DROPPING�� ����, ONLINE ���·� �����Ѵ�.
            sFileNode->mState &= ~SMI_FILE_DROPPING;
        }

        IDE_ERROR( SMI_FILE_STATE_IS_NOT_DROPPED( sFileNode->mState ) );
        IDE_ERROR( SMI_FILE_STATE_IS_NOT_CREATING( sFileNode->mState ) );

        sState = 0;
        sctTableSpaceMgr::unlockSpaceNode( sSpaceNode );
    }
    else
    {
        // TBS List���� �˻��� ���� ������ �ƹ��͵� ���� �ʴ´�.
        // nothing to do...
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if ( sState != 0 )
    {
        sctTableSpaceMgr::unlockSpaceNode( sSpaceNode );
    }

    return IDE_FAILURE;
}

/***********************************************************************
 * DESCRIPTION : tbs ���� dbf ��带 ��ȯ�Ѵ�.
 ***********************************************************************/
IDE_RC sddUpdate::getTBSDBF( scSpaceID            aSpaceID,
                             UInt                 aFileID,
                             sddTableSpaceNode**  aSpaceNode,
                             sddDataFileNode**    aFileNode )
{

    sddTableSpaceNode*  sSpaceNode = NULL;
    sddDataFileNode*    sFileNode = NULL;

    IDE_ERROR( aSpaceNode != NULL );
    IDE_ERROR( aFileNode  != NULL );

    sSpaceNode = (sddTableSpaceNode*)sctTableSpaceMgr::findSpaceNodeWithoutException( aSpaceID );

    if (sSpaceNode != NULL)
    {
        IDE_ERROR( sSpaceNode->mHeader.mID == aSpaceID );

        sddTableSpace::getDataFileNodeByIDWithoutException(
                              sSpaceNode,
                              aFileID,
                              &sFileNode );

        if (sFileNode != NULL)
        {
            IDE_ERROR_MSG( aFileID == sFileNode->mID,
                           "aFileID       : %"ID_UINT32_FMT"\n"
                           "sFileNode->mID : %"ID_UINT32_FMT,
                           aFileID,
                           sFileNode->mID );
            IDE_ERROR_MSG( sSpaceNode->mNewFileID > sFileNode->mID,
                           "sSpaceNode->mNewFileID : %"ID_UINT32_FMT"\n"
                           "sFileNode->mID         : %"ID_UINT32_FMT,
                           sSpaceNode->mNewFileID,
                           sFileNode->mID );
            IDE_ERROR_MSG( SMI_FILE_STATE_IS_NOT_DROPPED( sFileNode->mState ), 
                           "sFileNode->mState        : %"ID_UINT32_FMT,
                           sFileNode->mState );
        }
    }
    else
    {
         // nothing to do..
    }

    *aSpaceNode = sSpaceNode;
    *aFileNode  = sFileNode;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*
PRJ-1548 User Memory Tablespace

SCT_UPDATE_DRDB_EXTEND_DBF �α� �����

Ʈ����� Commit Pending List: [POP_DBF]

���� :   (1)                 (2)       (3)      (4)        (5)
���� : [RESIZE_DBF]-------->[ANCHOR]->[COMMIT]->[POP_DBF]->[ANCHOR]
���� :  |ONLINE     (Ȯ��)    ����      Ŀ��      ~RESIZING  ����
        |RESIZING

�˰���

��. (1) ������ �����ϸ� Ŀ���� �ȵ� Ʈ������̰�, Loganchor���� ������� �ʾұ� ������
    ������� ���� ����.

��. (2)�� (3) ���̿� �����ϸ� Ŀ���� �ȵ� Ʈ�����������, �α׾�Ŀ�� ONLINE|RESIZING ���·�
    ����Ǿ����Ƿ�, Rollback ����� RESIZING�ΰ�쿡�� ������ ���淮 ��ҿ� ~RESIZING�� ���־�� �Ѵ�.
    ��, RESIZING�� ���� �̹� Ȯ���� �Ϸ�� �����̱� �����̴�.

��. (3)�� (5) ���̿��� �����ϸ� Ŀ�Ե� Ʈ�����������, �α׾�Ŀ�� ONLINE|RESIZING ���·�
    ����Ǿ����Ƿ�, ONLINE|RESIZING ������ ��쿡�� Commit Pending ������ ����Ͽ�
    ONLINE ���·� �������־�� �Ѵ�.
    �̵����� ����ؼ� �������� ũ��� AfterSize�� ����Ͽ� Ȯ���� �Ѵ�

��. (5) ���Ŀ� �����ϸ� Ŀ�Ե� Ʈ������̰�, �α׾�Ŀ�� ONLINE�� ���·� ����Ǿ����Ƿ�
    ����� �� ���� ����.

*/
IDE_RC sddUpdate::redo_SCT_UPDATE_DRDB_EXTEND_DBF( idvSQL     * aStatistics,
                                                   void       * aTrans,
                                                   smLSN        /* aCurLSN */,
                                                   scSpaceID    aSpaceID,
                                                   UInt         aFileID,
                                                   UInt         aValueSize,
                                                   SChar      * aValuePtr,
                                                   idBool       /* aIsRestart */ )
{
    sddTableSpaceNode * sSpaceNode = NULL;
    sddDataFileNode   * sFileNode  = NULL;
    ULong               sFileSize  = 0;
    ULong               sCurrSize  = 0;
    ULong               sDiffSize  = 0;
    ULong               sAfterSize = 0;
    idBool              sIsPreparedIO = ID_FALSE;
    idBool              sIsNeedLogAnchorFlush = ID_FALSE;

    IDE_ERROR( aTrans != NULL );
    IDE_ERROR( aValuePtr != NULL );
    IDE_ERROR_MSG( aValueSize == (ID_SIZEOF(ULong)*2),
                   "aValueSize : %"ID_UINT32_FMT,
                   aValueSize );

    // AfterSize : Ȯ��� CURRSIZE
    idlOS::memcpy( &sAfterSize, aValuePtr, ID_SIZEOF(ULong) );

    // AfterSize : Ȯ��� ������ ����
    idlOS::memcpy( &sDiffSize,
                   aValuePtr+ID_SIZEOF(ULong),
                   ID_SIZEOF(ULong) );

    IDE_TEST( getTBSDBF( aSpaceID, aFileID, &sSpaceNode, &sFileNode )
              != IDE_SUCCESS );

    if (sFileNode != NULL)
    {
        // [�߿�]
        // ��������ũ��� Afterũ�⸦ ���Ͽ� AFTER ũ�Ⱑ ũ��
        // ������ CURRSIZE���� �����Ѵ�.
        // SHRINK������ ������ ������ ���������� ����������,
        // CURRSIZE ���� �����Ѵ�.

        // ���� ���� ������ page ������ ���Ѵ�.
        IDE_TEST(sddDiskMgr::prepareIO( aStatistics,
                                        sFileNode ) != IDE_SUCCESS);
        sIsPreparedIO = ID_TRUE;

        IDE_TEST(sFileNode->mFile.getFileSize(&sFileSize) != IDE_SUCCESS);

        sCurrSize = (sFileSize-SM_DBFILE_METAHDR_PAGE_SIZE) /
                    SD_PAGE_SIZE;

        /* PROJ-1923 �Ʒ��� ���� ��ø if �� ������ �����Ѵ�.
         * ������, �ڵ����ظ� ���� �ּ����� �����ϵ��� �Ѵ�.
         *
         * PRJ-1548 RESIZE ���� ���
         * BUGBUG - Media Recovery �ÿ��� sFileNode�� ���°� Resize�� �� �ִ�.
         *
         */
        /*
        * if ( SMI_FILE_STATE_IS_RESIZING( sFileNode->mState ) )
        * {
        *     // �˰��� (��)�� �ش��ϴ� RESIZING ������ ��쿡�� �����Ƿ�
        *     // ���¸� ONLINE���� ������ �� �ְ� Commit Pending ������ ����Ѵ�.
        *     if ( smLayerCallback::isBeginTrans( aTrans ) == ID_TRUE )
        *     {
        *         IDE_TEST( sddDataFile::addPendingOperation(
        *                 aTrans,
        *                 sFileNode,
        *                 ID_TRUE, // commit�ÿ� ���� 
        *                 SCT_POP_ALTER_DBF_RESIZE )
        *             != IDE_SUCCESS );
        *     }
        *     else
        *     {
        *         // ActiveTx�� �ƴ� ��� Pending ������� �ʴ´�.
        *     }
        *     // �˰��� (��) Rollback ����� ������ ���淮 ���(RESIZING�� ��쿡��)��
        *     // ~RESIZING�� ���־�� �Ѵ�.
        * }
        * else
        * {
        *     // Pending ����� �� �ʿ䰡 ����.
        * }
        */

        if ( smLayerCallback::isBeginTrans( aTrans ) == ID_TRUE )
        {
            if( SMI_FILE_STATE_IS_RESIZING( sFileNode->mState ) )
            {
                IDE_TEST( sddDataFile::addPendingOperation(
                        aTrans,
                        sFileNode,
                        ID_TRUE, // commit�ÿ� ���� 
                        SCT_POP_ALTER_DBF_RESIZE )
                    != IDE_SUCCESS );
            }
            else
            {
                // PROJ-1923 ������ redo�� �����Ѵ�.
                // �α׾�Ŀ�� size < log�� ��ϵ� size�� ũ�ٸ�,
                // �α׸� ����ϰ� extend�� ������� �������̹Ƿ� redo�Ѵ�.
                // �� �ܿ��� ���� �ʴ´�.
                if( sCurrSize < sAfterSize )
                {
                    sFileNode->mState |= SMI_FILE_RESIZING;

                    IDE_TEST( sddDataFile::addPendingOperation(
                            aTrans,
                            sFileNode,
                            ID_TRUE, // commit�ÿ� ���� 
                            SCT_POP_ALTER_DBF_RESIZE )
                        != IDE_SUCCESS );

                    sIsNeedLogAnchorFlush   = ID_TRUE;
                }
                else
                {
                    // do nothing
                }
            }
        }
        else
        {
            // do nothing
        }

        // ��Ŀ�� aftersize���� �۰�, ����ũ�Ⱑ aftersize���� ���� ����
        // ������ extend �Ѵ�.
        if (sCurrSize < sAfterSize)
        {
            sddDataFile::setCurrSize(sFileNode, (sAfterSize - sDiffSize));
            IDE_TEST( sddDataFile::extend(NULL, sFileNode, sDiffSize)
                      != IDE_SUCCESS );
        }

        sIsPreparedIO = ID_FALSE;
        IDE_TEST(sddDiskMgr::completeIO( aStatistics,
                                         sFileNode,
                                         SDD_IO_WRITE )
                 != IDE_SUCCESS);

        sddDataFile::setCurrSize(sFileNode, sAfterSize);

        if( sIsNeedLogAnchorFlush == ID_TRUE )
        {
            // loganchor flush
            IDE_ASSERT( smLayerCallback::updateDBFNodeAndFlush( sFileNode )
                        == IDE_SUCCESS );
        }
        else
        {
            // do nothing
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if ( sIsPreparedIO == ID_TRUE )
    {
        IDE_ASSERT( sddDiskMgr::completeIO( aStatistics,
                                            sFileNode,
                                            SDD_IO_WRITE ) == IDE_SUCCESS );
    }

    return IDE_FAILURE;
}

/*
PRJ-1548 User Memory Tablespace

SCT_UPDATE_DRDB_EXTEND_DBF �α� UNDO

Ʈ����� Commit Pending List: [POP_DBF]

���� :   (1)                 (2)         (3)          (4)        (5)
���� : [EXTEND_DBF]-------->[ANCHOR]->[CLR_EXTEND]->[ANCHOR]->[ROLLBACK]
���� :  |ONLINE     (Ȯ��)    ����     ~RESIZING      ����     �Ϸ�
        |RESIZING                      (���淮 ���)

���� �˰���

RESTART��

��. (1)�� (2) ���̿��� �����ϸ� �α׾�Ŀ�� ONLINE �����̰�, RESTART ��Ȳ�̹Ƿ� Ȯ�差�� ����
    ������� �ʴ´�. (��������)

��. (2)���� undo�� �����ϸ� �Ϸᰡ �ȵ� Ʈ�����������, �α׾�Ŀ�� ONLINE|RESIZING �����̱�
    ������, ONLINE(~RESIZING) ���·� �����ϰ�, ���淮�� ���(���)�Ѵ�.

RUNTIME��

��. (1)�� (2)���� �����ϸ� ONLINE|RESIZING ���� ~RESIZING �ϰ� ���淮 ����Ѵ�.

*/
IDE_RC sddUpdate::undo_SCT_UPDATE_DRDB_EXTEND_DBF(
                       idvSQL *             aStatistics,
                       void*                aTrans,
                       smLSN                /* aCurLSN */,
                       scSpaceID            aSpaceID,
                       UInt                 aFileID,
                       UInt                 aValueSize,
                       SChar*               aValuePtr,
                       idBool               aIsRestart )
{

    UInt                sState    = 0;
    UInt                sPrepared = 0;
    sddTableSpaceNode*  sSpaceNode;
    sddDataFileNode*    sFileNode = NULL;
    ULong               sBeforeSize;

    IDE_ERROR( aTrans != NULL );
    IDE_ERROR( aValuePtr != NULL );
    IDE_ERROR_MSG( aValueSize == ID_SIZEOF(ULong),
                   "aValueSize : %"ID_UINT32_FMT,
                   aValueSize );

    sSpaceNode = NULL;
    sFileNode  = NULL;

    idlOS::memcpy(&sBeforeSize, aValuePtr, ID_SIZEOF(ULong));

    IDE_TEST( getTBSDBF( aSpaceID, aFileID, &sSpaceNode, &sFileNode )
              != IDE_SUCCESS );

    if (( sSpaceNode != NULL ) &&
        ( sFileNode  != NULL ))
    {
        sctTableSpaceMgr::lockSpaceNode( aStatistics,
                                         sSpaceNode );
        sState = 1;

        while ( SMI_FILE_STATE_IS_BACKUP( sFileNode->mState ) )
        {
            IDE_ERROR( aIsRestart != ID_TRUE );

            // fix BUG-11337.
            // ����Ÿ ������ ������̸� �Ϸ��Ҷ����� ��� �Ѵ�.
            sctTableSpaceMgr::wait4Backup( sSpaceNode );
        }

        // ��߿� Rollback�� �߻��ϰų�  RESIZING ���°� Loganchor�� ����� ���
        // ���� ���� ���淮�� ������ش�.
        if ( SMI_FILE_STATE_IS_RESIZING( sFileNode->mState ) )
        {
            // ���� ���� ������ page ������ ���Ѵ�.
            IDE_TEST(sddDiskMgr::prepareIO( aStatistics,
                                            sFileNode ) != IDE_SUCCESS);
            sPrepared = 1;

            IDE_TEST( sddDataFile::truncate(sFileNode, sBeforeSize) != IDE_SUCCESS );

            sPrepared = 0;
            IDE_TEST( sddDiskMgr::completeIO( aStatistics,
                                              sFileNode,
                                              SDD_IO_WRITE ) != IDE_SUCCESS );

            sFileNode->mState &= ~SMI_FILE_RESIZING;
        }
        else
        {
            // RESTART�� (3)������ ������ ��� �̹Ƿ� �˰��� (��)�� �ش��Ѵ�.
            // NOTHING TO DO ...
        }

        sddDataFile::setCurrSize(sFileNode, sBeforeSize);

        sState = 0;
        sctTableSpaceMgr::unlockSpaceNode( sSpaceNode );
    }
    else
    {
        // NOTHING TO DO ...
    }

    if ( sFileNode != NULL )
    {
        /* BUG-24086: [SD] Restart�ÿ��� File�̳� TBS�� ���� ���°� �ٲ���� ���
         * LogAnchor�� ���¸� �ݿ��ؾ� �Ѵ�.
         *
         * Restart Recovery�ÿ��� updateDBFNodeAndFlush���� �ʴ����� �ϵ��� ����.
         * */

        IDE_ASSERT( smLayerCallback::updateDBFNodeAndFlush( sFileNode )
                    == IDE_SUCCESS );

    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    IDE_PUSH();
    {
        if ( sPrepared != 0 )
        {
            IDE_ASSERT( sddDiskMgr::completeIO( aStatistics,
                                                sFileNode,
                                                SDD_IO_WRITE )
                        == IDE_SUCCESS );
        }

        if ( sState != 0 )
        {
            sctTableSpaceMgr::unlockSpaceNode( sSpaceNode );
        }
    }
    IDE_POP();

    return IDE_FAILURE;
}

/*
PRJ-1548 User Memory Tablespace

SCT_UPDATE_DRDB_SHRINK_DBF �α� �����

Ʈ����� Commit Pending List: [POP_DBF]

���� :   (1)                 (2)       (3)      (4)        (5)
���� : [SHRINK_DBF]-------->[ANCHOR]->[COMMIT]->[POP_DBF]->[ANCHOR]
���� :  |ONLINE     (���)    ����      Ŀ��    ~RESIZING  ����
        |RESIZING

�˰���

��. (1) ������ �����ϸ� Ŀ���� �ȵ� Ʈ������̰�, Loganchor���� ������� �ʾұ� ������
    ������� ���� ����.

��. (2)�� (3) ���̿� �����ϸ� Ŀ���� �ȵ� Ʈ�����������, �α׾�Ŀ�� ONLINE|RESIZING ���·�
    ����Ǿ����Ƿ�, Rollback ����� RESIZING�ΰ�쿡�� ������ ���淮 ��ҿ� ~RESIZING�� ���־�� �Ѵ�.
    ��, RESIZING�� ���� �̹� ��Ұ� �Ϸ�� �����̱� �����̴�.

��. (3)�� (5) ���̿��� �����ϸ� Ŀ�Ե� Ʈ�����������, �α׾�Ŀ�� ONLINE|RESIZING ���·�
    ����Ǿ����Ƿ�, ONLINE|RESIZING ������ ��쿡�� Commit Pending ������ ����Ͽ�
    ONLINE ���·� �������־�� �Ѵ�.
    �̵����� ����ؼ� �������� ũ��� AfterSize�� ����Ͽ� Ȯ���� �Ѵ�

��. (5) ���Ŀ� �����ϸ� Ŀ�Ե� Ʈ������̰�, �α׾�Ŀ�� ONLINE�� ���·� ����Ǿ����Ƿ�
    ����� �� ���� ����.

*/
IDE_RC sddUpdate::redo_SCT_UPDATE_DRDB_SHRINK_DBF(
                                            idvSQL    * aStatistics,
                                            void      * aTrans,
                                            smLSN       /* aCurLSN */,
                                            scSpaceID   aSpaceID,
                                            UInt        aFileID,
                                            UInt        aValueSize,
                                            SChar     * aValuePtr,
                                            idBool      /* aIsRestart */ )
{
    sddTableSpaceNode * sSpaceNode;
    sddDataFileNode   * sFileNode;
    ULong               sAfterInitSize;
    ULong               sAfterCurrSize;
    ULong               sDiffSize;
    ULong               sFileSize   = 0;
    ULong               sCurrSize   = 0;
    SLong               sResizePageSize = 0;
    idBool              sIsNeedLogAnchorFlush   = ID_FALSE;
    sctPendingOp      * sPendingOp;
    UInt                sState = 0;

    IDE_DASSERT( aTrans != NULL );
    IDE_DASSERT( aValuePtr != NULL );
    IDE_DASSERT( aValueSize == (ID_SIZEOF(ULong)*3));
    ACP_UNUSED( aValueSize );

    idlOS::memcpy( &sAfterInitSize, aValuePtr, ID_SIZEOF(ULong) );

    idlOS::memcpy( &sAfterCurrSize, aValuePtr + ID_SIZEOF(ULong),
                   ID_SIZEOF(ULong) );

    idlOS::memcpy( &sDiffSize, aValuePtr + (ID_SIZEOF(ULong)*2),
                   ID_SIZEOF(ULong) );

    IDE_TEST( getTBSDBF( aSpaceID, aFileID, &sSpaceNode, &sFileNode )
              != IDE_SUCCESS );

    if (( sSpaceNode != NULL ) &&
        ( sFileNode  != NULL ))
    {
        // ���� ���� ������ page ������ ���Ѵ�.
        IDE_TEST( sddDiskMgr::prepareIO( aStatistics,
                                         sFileNode ) != IDE_SUCCESS );
        sState = 1;

        IDE_TEST(sFileNode->mFile.getFileSize(&sFileSize) != IDE_SUCCESS);

        // BUG-47364 prepareIO �Ŀ� completeIO �� ���� �ʴ� ���� �ذ�
        sState = 0;
        IDE_TEST( sddDiskMgr::completeIO( aStatistics,
                                          sFileNode,
                                          SDD_IO_READ ) != IDE_SUCCESS );

        sCurrSize = (sFileSize-SM_DBFILE_METAHDR_PAGE_SIZE) /
            SD_PAGE_SIZE;

        /* PROJ-1923 �Ʒ��� ���� ��ø if �� ������ �����Ѵ�.
         * ������, �ڵ����ظ� ���� �ּ����� �����ϵ��� �Ѵ�. */
        /*
         * // PRJ-1548 RESIZE ���� ���
         * if ( SMI_FILE_STATE_IS_RESIZING( sFileNode->mState ) )
         * {
         *     // �˰��� (��)�� �ش��ϴ� RESIZING ������ ��쿡�� �����Ƿ�
         *     // ���¸� ONLINE���� ������ �� �ְ� Commit Pending ������
         *     // ����Ѵ�.
         *     if ( smLayerCallback::isBeginTrans( aTrans ) == ID_TRUE )
         *     {
         *         IDE_TEST( sddDataFile::addPendingOperation(
         *                                   aTrans,
         *                                   sFileNode,
         *                                   ID_TRUE, // commit�ÿ� ����
         *                                   SCT_POP_ALTER_DBF_RESIZE )
         *                   != IDE_SUCCESS );
         *     }
         *     else
         *     {
         *         // ActiveTx�� �ƴ� ��� Pending ������� �ʴ´�.
         *     }
         * 
         *     // ���� Commit���� ���� Ʈ������̶�� Rollback Pending
         *     // �����̱� ������
         *     // undo_SCT_UPDATE_DRDB_EXTEND_DBF()���� POP_DROP_DBF ����
         *     // ����Ѵ�.
         * }
         * else
         * {
         *     // Pending ����� �� �ʿ䰡 ����.
         * }
         */

        if ( smLayerCallback::isBeginTrans( aTrans ) == ID_TRUE )
        {
            if ( SMI_FILE_STATE_IS_RESIZING( sFileNode->mState ) )
            {
                IDE_TEST( sddDataFile::addPendingOperation(
                              aTrans,
                              sFileNode,
                              ID_TRUE, // commit�ÿ� ����
                              SCT_POP_ALTER_DBF_RESIZE )
                          != IDE_SUCCESS );
            }
            else
            {
                // PROJ-1923 ������ redo�� �����Ѵ�.
                // �α׾�Ŀ�� size > log �� ��ϵ� size�� �۴ٸ�,
                // �α׸� ����ϰ� shrink �� ������� �������̹Ƿ�
                // redo �Ѵ�. �� �ܿ��� ���� �ʴ´�.
                if( sCurrSize > sAfterCurrSize )
                {
                    sFileNode->mState |= SMI_FILE_RESIZING;

                    IDE_TEST( sddDataFile::addPendingOperation(
                                  aTrans,
                                  sFileNode,
                                  ID_TRUE, // commit�ÿ� ����
                                  SCT_POP_ALTER_DBF_RESIZE,
                                  &sPendingOp )
                              != IDE_SUCCESS );

                    sResizePageSize = sDiffSize * -1;

                    sPendingOp->mPendingOpFunc    = sddDiskMgr::shrinkFilePending;
                    sPendingOp->mPendingOpParam   = (void *)sFileNode;
                    sPendingOp->mResizePageSize   = sResizePageSize;
                    sPendingOp->mResizeSizeWanted = sAfterInitSize; // aSizeWanted;

                    sIsNeedLogAnchorFlush   = ID_TRUE;
                }
            }
        }
        else
        {
            // do nothing
        }

        // [�߿�]
        // ������ CURRSIZE���� �����Ѵ�.
        // EXTEND������ ������ ������ ���������� ����������,
        // CURRSIZE ���� �����Ѵ�.

        sddDataFile::setInitSize(sFileNode, sAfterInitSize);
        sddDataFile::setCurrSize(sFileNode, sAfterCurrSize);

        if( sIsNeedLogAnchorFlush == ID_TRUE )
        {
            // loganchor flush
            IDE_ASSERT( smLayerCallback::updateDBFNodeAndFlush( sFileNode )
                        == IDE_SUCCESS );
        }
        else
        {
            // do nothing
        }
    }
    else
    {
        // nothing to do..
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if ( sState == 1 )
    {
        IDE_ASSERT( sddDiskMgr::completeIO( aStatistics,
                                            sFileNode,
                                            SDD_IO_READ ) == IDE_SUCCESS );
    }

    return IDE_FAILURE;
}


/*
PRJ-1548 User Memory Tablespace

SCT_UPDATE_DRDB_SHRINK_DBF �α� UNDO

Ʈ����� Commit Pending List: [POP_DBF]

���� :   (1)                 (2)         (3)          (4)        (5)
���� : [SHRINK_DBF]-------->[ANCHOR]->[CLR_SHRINK]->[ANCHOR]->[ROLLBACK]
���� :  |ONLINE               ����     ~RESIZING (���)      ����     �Ϸ�
        |RESIZING                      (���淮 Ȯ��)

���� �˰���

RESTART��

��. (1)�� (2) ���̿��� �����ϸ� �α׾�Ŀ�� ONLINE �����̰�, RESTART ��Ȳ�̹Ƿ� ��ҷ��� ����
    ������� �ʴ´�. (��������)

��. (2)���� undo�� �����ϸ� �Ϸᰡ �ȵ� Ʈ�����������, �α׾�Ŀ�� ONLINE|RESIZING �����̱�
    ������, ONLINE(~RESIZING) ���·� �����ϰ�, ���淮�� ����Ѵ�.

RUNTIME��

��. (1)�� (2) ���̿����� before �̹����� �������ش�.

��. (3)���� ONLINE|RESIZING ���� ~RESIZING �ϰ� ���淮 ����Ѵ�.

*/
IDE_RC sddUpdate::undo_SCT_UPDATE_DRDB_SHRINK_DBF(
                                       idvSQL *             aStatistics,
                                       void*                aTrans,
                                       smLSN                /* aCurLSN */,
                                       scSpaceID            aSpaceID,
                                       UInt                 aFileID,
                                       UInt                 aValueSize,
                                       SChar*               aValuePtr,
                                       idBool               /* aIsRestart */)
{
    UInt                sState = 0;
    ULong               sBeforeInitSize;
    ULong               sBeforeCurrSize;
    ULong               sDiffSize;
    sddTableSpaceNode*  sSpaceNode;
    sddDataFileNode*    sFileNode;

    IDE_DASSERT( aTrans != NULL );
    IDE_DASSERT( aValueSize == (ID_SIZEOF(ULong)*3) );
    IDE_DASSERT( aValuePtr != NULL );
    
    ACP_UNUSED( aTrans );
    ACP_UNUSED( aValueSize );

    idlOS::memcpy( &sBeforeInitSize, aValuePtr, ID_SIZEOF(ULong) );

    idlOS::memcpy( &sBeforeCurrSize,
                   aValuePtr + ID_SIZEOF(ULong),
                   ID_SIZEOF(ULong) );

    idlOS::memcpy( &sDiffSize,
                   aValuePtr + (ID_SIZEOF(ULong)*2),
                   ID_SIZEOF(ULong) );

    IDE_TEST( getTBSDBF( aSpaceID, aFileID, &sSpaceNode, &sFileNode )
              != IDE_SUCCESS );

    if (( sSpaceNode != NULL ) &&
        ( sFileNode  != NULL ))
    {
        sctTableSpaceMgr::lockSpaceNode( aStatistics,
                                         sSpaceNode );
        sState = 1;
        // fix BUG-11337.
        // ����Ÿ ������ ������̸� �Ϸ��Ҷ����� ��� �Ѵ�.
        while ( SMI_FILE_STATE_IS_BACKUP( sFileNode->mState ) )
        {
            // �Ʒ��Լ����� SpaceNode::lock �� �����ߴٰ�
            // �ٽ� ȹ���ؼ� return ��.

            sctTableSpaceMgr::wait4Backup( sSpaceNode );
        }

        // ��߿� Rollback�� �߻��ϰų�  RESIZING ���°� Loganchor�� ����� ���
        // ���� ���Ͽ� ���ؼ� ����Ȱ��� ���� ������ ���°��� ����.
        if ( SMI_FILE_STATE_IS_RESIZING( sFileNode->mState ) )
        {
            // RESTART�� (3)������ ������ ��� �̹Ƿ� �˰��� (��)�� �ش��Ѵ�.
            // NOTHING TO DO ...
        }

        // TBS Node�� X ����� ȹ���� �����̹Ƿ� lock�� ȹ���� �ʿ����.
        sddDataFile::setInitSize(sFileNode, sBeforeInitSize);
        sddDataFile::setCurrSize(sFileNode, sBeforeCurrSize);

        sFileNode->mState &= ~SMI_FILE_RESIZING;

        sState = 0;
        sctTableSpaceMgr::unlockSpaceNode( sSpaceNode );
    }
    else
    {
        // ���������ÿ��� Nothing To do...
    }

    if ( sFileNode != NULL )
    {
        /* BUG-24086: [SD] Restart�ÿ��� File�̳� TBS�� ���� ���°� �ٲ���� ���
         * LogAnchor�� ���¸� �ݿ��ؾ� �Ѵ�.
         *
         * Restart Recovery�ÿ��� updateDBFNodeAndFlush���� �ʴ����� �ϵ��� ����.
         * */

        IDE_ASSERT( smLayerCallback::updateDBFNodeAndFlush( sFileNode )
                    == IDE_SUCCESS );

    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if ( sState != 0 )
    {
        sctTableSpaceMgr::unlockSpaceNode( sSpaceNode );
    }

    return IDE_FAILURE;
}

/***********************************************************************
 * DESCRIPTION : datafile autoextend mode�� ���� redo ����
 * SMR_LT_TBS_UPDATE : SCT_UPDATE_DRDB_AUTOEXTEND_DBF
 * After  image : datafile attribute
 **********************************************************************/
IDE_RC sddUpdate::redo_SCT_UPDATE_DRDB_AUTOEXTEND_DBF(
                                   idvSQL             * /* aStatistics */,
                                   void               * aTrans,
                                   smLSN                /* aCurLSN */,
                                   scSpaceID            aSpaceID,
                                   UInt                 aFileID,
                                   UInt                 aValueSize,
                                   SChar              * aValuePtr,
                                   idBool               /* aIsRestart */ )
{

    idBool              sAutoExtMode;
    ULong               sMaxSize;
    ULong               sNextSize;
    sddTableSpaceNode * sSpaceNode;
    sddDataFileNode   * sFileNode;

    IDE_DASSERT( aTrans     != NULL );
    IDE_DASSERT( aValuePtr  != NULL );
    IDE_DASSERT( aValueSize == ID_SIZEOF(idBool) + ID_SIZEOF(ULong) * 2);

    ACP_UNUSED( aTrans );
    ACP_UNUSED( aValueSize );

    idlOS::memcpy(&sAutoExtMode, aValuePtr, ID_SIZEOF(idBool));
    aValuePtr += ID_SIZEOF(idBool);

    idlOS::memcpy(&sNextSize, aValuePtr, ID_SIZEOF(ULong));
    aValuePtr += ID_SIZEOF(ULong);

    idlOS::memcpy(&sMaxSize, aValuePtr, ID_SIZEOF(ULong));

    IDE_TEST( getTBSDBF( aSpaceID, aFileID, &sSpaceNode, &sFileNode )
              != IDE_SUCCESS );

    if (( sSpaceNode != NULL ) &&
        ( sFileNode  != NULL ))
    {
        sddDataFile::setAutoExtendProp(sFileNode, sAutoExtMode, sNextSize, sMaxSize);

        // loganchor flush
        IDE_ASSERT( smLayerCallback::updateDBFNodeAndFlush( sFileNode )
                    == IDE_SUCCESS );
    }
    else
    {
        // nothing to do ...
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * DESCRIPTION : datafile autoextend mode�� ���� undo ����
 * SMR_LT_TBS_UPDATE : SCT_UPDATE_DRDB_AUTOEXTEND_DBF
 * before image : datafile attribute
 **********************************************************************/
IDE_RC sddUpdate::undo_SCT_UPDATE_DRDB_AUTOEXTEND_DBF(
                       idvSQL *             aStatistics,
                       void*                aTrans,
                       smLSN                /* aCurLSN */,
                       scSpaceID            aSpaceID,
                       UInt                 aFileID,
                       UInt                 aValueSize,
                       SChar*               aValuePtr,
                       idBool               aIsRestart )
{

    UInt               sState = 0;
    idBool             sAutoExtMode;
    ULong              sNextSize;
    ULong              sMaxSize;
    sddTableSpaceNode* sSpaceNode;
    sddDataFileNode*   sFileNode;

    IDE_ERROR( (aTrans != NULL) || (aIsRestart == ID_TRUE) );
    IDE_DASSERT( aValuePtr != NULL );
    IDE_DASSERT( aValueSize == ID_SIZEOF(idBool) + ID_SIZEOF(ULong) * 2);

    ACP_UNUSED( aValueSize );

    idlOS::memcpy(&sAutoExtMode, aValuePtr, ID_SIZEOF(idBool));
    aValuePtr += ID_SIZEOF(idBool);

    idlOS::memcpy(&sNextSize, aValuePtr, ID_SIZEOF(ULong));
    aValuePtr += ID_SIZEOF(ULong);

    idlOS::memcpy(&sMaxSize, aValuePtr, ID_SIZEOF(ULong));

    IDE_TEST( getTBSDBF( aSpaceID, aFileID, &sSpaceNode, &sFileNode )
              != IDE_SUCCESS );

    if (( sSpaceNode != NULL ) &&
        ( sFileNode  != NULL ))
    {
        sctTableSpaceMgr::lockSpaceNode( aStatistics,
                                         sSpaceNode );
        sState = 1;

        sddDataFile::setAutoExtendProp(sFileNode, sAutoExtMode, sNextSize, sMaxSize);

        /* BUG-24086: [SD] Restart�ÿ��� File�̳� TBS�� ���� ���°� �ٲ���� ���
         * LogAnchor�� ���¸� �ݿ��ؾ� �Ѵ�.
         *
         * Restart Recovery�ÿ��� updateDBFNodeAndFlush���� �ʴ����� �ϵ��� ����.
         * */

        IDE_ASSERT( smLayerCallback::updateDBFNodeAndFlush( sFileNode ) == IDE_SUCCESS );

        sState = 0;
        sctTableSpaceMgr::unlockSpaceNode( sSpaceNode );
    }
    else
    {
        // nothing to do ...
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if ( sState != 0 )
    {
        sctTableSpaceMgr::unlockSpaceNode( sSpaceNode );
    }

    return IDE_FAILURE;
}


/*
    ALTER TABLESPACE TBS1 OFFLINE .... �� ���� REDO ����

    [ �α� ���� ]
    After Image  --------------------------------------------
      UInt                aAState

    [ ALTER TABLESPACE OFFLINE �� REDO ó�� ]
     Offline�� ���� REDO�� TBSNode.Status �� ����
     Commit Pending Operation ���
     (note-1) TBSNode�� loganchor�� flush���� ����
              -> Restart Recovery�Ϸ��� ��� TBS�� loganchor�� flush�ϱ� ����
     (note-2) Commit Pending�� Resource ������ �������� ����
              -> Restart Recovery�Ϸ��� OFFLINE TBS�� ���� Resource������ �Ѵ�
*/
IDE_RC sddUpdate::redo_SCT_UPDATE_DRDB_ALTER_TBS_OFFLINE( idvSQL      * /* aStatistics */,
                                                          void        * aTrans,
                                                          smLSN         /* aCurLSN */,
                                                          scSpaceID     aSpaceID,
                                                          UInt          /*aFileID*/,
                                                          UInt          aValueSize,
                                                          SChar       * aValuePtr,
                                                          idBool        /*aIsRestart*/ )
{
    UInt                sTBSState;
    sddTableSpaceNode * sTBSNode;
    sctPendingOp      * sPendingOp;

    sTBSNode = (sddTableSpaceNode*)sctTableSpaceMgr::findSpaceNodeWithoutException( aSpaceID );

    if ( sTBSNode != NULL )
    {
        IDE_TEST( getAlterTBSOnOffImage( aValueSize,
                                         aValuePtr,
                                         & sTBSState ) != IDE_SUCCESS );

        if ( smLayerCallback::isBeginTrans( aTrans ) == ID_TRUE )
        {
            // Commit Pending���
            // Transaction Commit�ÿ� ������ Pending Operation���
            IDE_TEST( sctTableSpaceMgr::addPendingOperation(
                        aTrans,
                        sTBSNode->mHeader.mID,
                        ID_TRUE, /* Pending ���� ���� ���� : Commit �� */
                        SCT_POP_ALTER_TBS_OFFLINE,
                        & sPendingOp ) != IDE_SUCCESS );

            // Commit�� sctTableSpaceMgr::executePendingOperation����
            // (note-2) Commit Pending�� Resource ������ �������� �ʴ´�.
            sPendingOp->mPendingOpFunc =
                         smLayerCallback::alterTBSOfflineCommitPending;
            sPendingOp->mNewTBSState   = sTBSState;

            sTBSNode->mHeader.mState |= SMI_TBS_SWITCHING_TO_OFFLINE;
        }
        else
        {
            // ActiveTx�� �ƴ� ��� Pending ������� �ʴ´�.
        }
    }
    else
    {
        // �̹� Drop�� Tablespace�� ���
        // nothing to do ...
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/*
    ALTER TABLESPACE TBS1 OFFLINE .... �� ���� UNDO ����

    [ �α� ���� ]
    Before Image  --------------------------------------------
      UInt                aBState

    [ ALTER TABLESPACE OFFLINE �� UNDO ó�� ]
      (u-010) (020)�� ���� UNDO�� TBSNode.Status := Before Image(ONLINE)
      (note-1) TBSNode�� loganchor�� flush���� ����
                => Restart Recovery ���Ŀ� ó����.
*/
IDE_RC sddUpdate::undo_SCT_UPDATE_DRDB_ALTER_TBS_OFFLINE( idvSQL      * /* aStatistics */,
                                                          void        * /*aTrans*/,
                                                          smLSN         /* aCurLSN */,
                                                          scSpaceID     aSpaceID,
                                                          UInt          /*aFileID*/,
                                                          UInt          aValueSize,
                                                          SChar       * aValuePtr,
                                                          idBool        /*aIsRestart*/)
{
    UInt                 sTBSState;
    sddTableSpaceNode  * sTBSNode;

    sTBSNode = (sddTableSpaceNode*)sctTableSpaceMgr::findSpaceNodeWithoutException( aSpaceID );

    if ( sTBSNode != NULL )
    {
        IDE_TEST( getAlterTBSOnOffImage( aValueSize,
                                         aValuePtr,
                                         & sTBSState ) != IDE_SUCCESS );

        // ����̰ų� Restart Recovery�� ���
        // Switch_to_Offline -> Before State�� �����Ѵ�.
        sTBSNode->mHeader.mState = sTBSState;
    }
    else
    {
        // �̹� Drop�� Tablespace�� ���
        // nothing to do ...
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/*
    ALTER TABLESPACE TBS1 ONLINE .... �� ���� REDO ����

    [ �α� ���� ]
    After Image  --------------------------------------------
      UInt                aAState

    [ ALTER TABLESPACE ONLINE �� REDO ó�� ]
    (r-010) TBSNode.Status := After Image(SW)
      (note-1) TBSNode�� loganchor�� flush���� ����
               -> Restart Recovery�Ϸ��� ��� TBS�� loganchor�� flush�ϱ� ����
*/
IDE_RC sddUpdate::redo_SCT_UPDATE_DRDB_ALTER_TBS_ONLINE( idvSQL       * /* aStatistics */,
                                                         void         * aTrans,
                                                         smLSN          /* aCurLSN */,
                                                         scSpaceID      aSpaceID,
                                                         UInt           /*aFileID*/,
                                                         UInt           aValueSize,
                                                         SChar        * aValuePtr,
                                                         idBool         /*aIsRestart*/ )
{
    UInt                 sTBSState;
    sddTableSpaceNode  * sTBSNode;
    sctPendingOp       * sPendingOp;

    sTBSNode = (sddTableSpaceNode*)sctTableSpaceMgr::findSpaceNodeWithoutException( aSpaceID );

    if ( sTBSNode != NULL )
    {
        IDE_TEST( getAlterTBSOnOffImage( aValueSize,
                                         aValuePtr,
                                         & sTBSState ) != IDE_SUCCESS );

        if ( smLayerCallback::isBeginTrans( aTrans ) == ID_TRUE )
        {
            // Commit Pending���
            // Transaction Commit�ÿ� ������ Pending Operation���
            IDE_TEST( sctTableSpaceMgr::addPendingOperation(
                        aTrans,
                        sTBSNode->mHeader.mID,
                        ID_TRUE, /* Pending ���� ���� ���� : Commit �� */
                        SCT_POP_ALTER_TBS_ONLINE,
                        & sPendingOp ) != IDE_SUCCESS );

            // Commit�� sctTableSpaceMgr::executePendingOperation
            // (note-2) Commit Pending�� Resource ������ �������� �ʴ´�.
            sPendingOp->mPendingOpFunc =
                        smLayerCallback::alterTBSOnlineCommitPending;
            sPendingOp->mNewTBSState   = sTBSState;

            sTBSNode->mHeader.mState |= SMI_TBS_SWITCHING_TO_ONLINE;
        }
        else
        {
            // ActiveTx�� �ƴ� ��� Pending ������� �ʴ´�.
        }
    }
    else
    {
        // �̹� Drop�� Tablespace�� ���
        // nothing to do ...
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/*
    ALTER TABLESPACE TBS1 ONLINE .... �� ���� UNDO ����

    [ �α� ���� ]
    Before Image  --------------------------------------------
      UInt                aBState

    [ ALTER TABLESPACE ONLINE �� UNDO ó�� ]
      (u-050)  TBSNode.Status := Before Image(OFFLINE)
      (note-1) TBSNode�� loganchor�� flush���� ����
               -> ALTER TBS ONLINEE�� Commit Pending�� ����
                  COMMIT���Ŀ��� ����� TBS���°� log anchor�� flush�Ǳ� ����
*/
IDE_RC sddUpdate::undo_SCT_UPDATE_DRDB_ALTER_TBS_ONLINE( idvSQL       * /* aStatistics */,
                                                         void         * /*aTrans*/,
                                                         smLSN          /* aCurLSN */,
                                                         scSpaceID      aSpaceID,
                                                         UInt           /*aFileID*/,
                                                         UInt           aValueSize,
                                                         SChar        * aValuePtr,
                                                         idBool         /*aIsRestart*/ )
{
    UInt                sTBSState;
    sddTableSpaceNode * sTBSNode;

    sTBSNode = (sddTableSpaceNode*)sctTableSpaceMgr::findSpaceNodeWithoutException( aSpaceID );

    if ( sTBSNode != NULL )
    {
        IDE_TEST( getAlterTBSOnOffImage( aValueSize,
                                         aValuePtr,
                                         & sTBSState ) != IDE_SUCCESS );

        sTBSNode->mHeader.mState = sTBSState;
    }
    else
    {
        // �̹� Drop�� Tablespace�� ���
        // nothing to do ...
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*
    ALTER TABLESPACE TBS1 ONLINE/OFFLINE .... �� ���� Log Image�� �м��Ѵ�.

    [IN]  aValueSize     - Log Image �� ũ��
    [IN]  aValuePtr      - Log Image
    [OUT] aState         - Tablespace�� ����
 */
IDE_RC sddUpdate::getAlterTBSOnOffImage( UInt       aValueSize,
                                         SChar    * aValuePtr,
                                         UInt     * aState )
{
    IDE_DASSERT( aValuePtr != NULL );
    IDE_DASSERT( aValueSize == (UInt)( ID_SIZEOF(*aState)));
    IDE_DASSERT( aState   != NULL );
    
    ACP_UNUSED( aValueSize );

    idlOS::memcpy(aState, aValuePtr, ID_SIZEOF(*aState));
    aValuePtr += ID_SIZEOF(*aState);

    return IDE_SUCCESS;
}

/*
    DRDB_ALTER_OFFLINE_DBF �� ���� REDO ����

    [ �α� ���� ]
    After Image  --------------------------------------------
      UInt                aAState

    [ OFFLINE DBF�� REDO ó�� ]
     Offline�� ���� REDO�� DBFNode.Status �� ���� Commit Pending Operation ���

     (note-1) DBFNode�� loganchor�� flush���� ����
              -> Restart Recovery�Ϸ��� ��� DBF�� loganchor�� flush�ϱ� ����
     (note-2) Commit Pending�� Resource ������ �������� ����
              -> DBF ���������� ó���� ���� ����. ( Pending �Լ� ���ʿ� )
*/
IDE_RC sddUpdate::redo_SCT_UPDATE_DRDB_ALTER_DBF_OFFLINE( idvSQL      * /* aStatistics */,
                                                          void        * aTrans,
                                                          smLSN         /* aCurLSN */,
                                                          scSpaceID     aSpaceID,
                                                          UInt          aFileID,
                                                          UInt          aValueSize,
                                                          SChar       * aValuePtr,
                                                          idBool        /*aIsRestart*/ )
{
    UInt                 sDBFState;
    sddTableSpaceNode  * sSpaceNode;
    sddDataFileNode    * sFileNode;
    sctPendingOp       * sPendingOp;

    IDE_TEST( getTBSDBF( aSpaceID, aFileID, &sSpaceNode, &sFileNode )
              != IDE_SUCCESS );

    if (( sSpaceNode != NULL ) &&
        ( sFileNode  != NULL ))
    {
        IDE_TEST( getAlterDBFOnOffImage( aValueSize,
                                         aValuePtr,
                                         & sDBFState ) != IDE_SUCCESS );

        if ( smLayerCallback::isBeginTrans( aTrans ) == ID_TRUE )
        {
            // Commit Pending���
            // Transaction Commit�ÿ� ������ Pending Operation���
            IDE_TEST( sddDataFile::addPendingOperation(
                          aTrans,
                          sFileNode,
                          ID_TRUE, /* Pending ���� ���� ���� : Commit �� */
                          SCT_POP_ALTER_DBF_OFFLINE,
                          & sPendingOp ) != IDE_SUCCESS );

            // Commit�� sctTableSpaceMgr::executePendingOperation����
            // (note-2) Commit Pending�� Resource ������ �������� �ʴ´�.
            sPendingOp->mNewDBFState = sDBFState;
            // pending �� ó���� �Լ��� ����.
            sPendingOp->mPendingOpFunc = NULL;
            
            // loganchor flush
            IDE_ASSERT( smLayerCallback::updateDBFNodeAndFlush( sFileNode )
                        == IDE_SUCCESS );
        }
        else
        {
            // ActiveTx�� �ƴ� ��� Pending ������� �ʴ´�.
        }
    }
    else
    {
        // �̹� Drop�� DBF�� ���
        // nothing to do ...
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/*
    DRDB_ALTER_OFFLINE_DBF �� ���� UNDO ����

    [ �α� ���� ]
    Before Image  --------------------------------------------
      UInt                aBState

    [ ALTER TABLESPACE OFFLINE �� UNDO ó�� ]
      (u-010) (020)�� ���� UNDO�� DBFNode.Status := Before Image
      (note-1) DBFNode�� loganchor�� flush���� ����
               commit ���� ���� offline ������ loganchor�� offline ���°�
               �������� ���� ������ undo�ÿ��� loganchor�� ���� ���¸�
               flush�� �ʿ����.
*/
IDE_RC sddUpdate::undo_SCT_UPDATE_DRDB_ALTER_DBF_OFFLINE(
                        idvSQL    * /* aStatistics */,
                        void      * /*aTrans*/,
                        smLSN       /* aCurLSN */,
                        scSpaceID   aSpaceID,
                        UInt        aFileID,
                        UInt        aValueSize,
                        SChar     * aValuePtr,
                        idBool      /*aIsRestart*/)
{
    UInt                 sDBFState;
    sddTableSpaceNode  * sSpaceNode;
    sddDataFileNode    * sFileNode;

    IDE_TEST( getTBSDBF( aSpaceID, aFileID, &sSpaceNode, &sFileNode )
              != IDE_SUCCESS );

    if (( sSpaceNode != NULL ) &&
        ( sFileNode  != NULL ))
    {
        IDE_TEST( getAlterDBFOnOffImage( aValueSize,
                                         aValuePtr,
                                         & sDBFState ) != IDE_SUCCESS );

        // (u-010)
        // ����̰ų� Restart Recovery�� ��� Before State�� �����Ѵ�.
        sFileNode->mState = sDBFState;
    }
    else
    {
        // �̹� Drop�� DBF�� ���
        // nothing to do ...
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/*
    DRDB_ALTER_ONLINE_DBF �� ���� REDO ����

    [ �α� ���� ]
    After Image  --------------------------------------------
      UInt                aAState

    [ ONLINE DBF�� REDO ó�� ]
    Online �� ���� REDO�� DBFNode.Status�� ���� Commit Pending Operation ���

    (note-1) DBFNode�� loganchor�� flush���� ����
             -> Restart Recovery�Ϸ��� ��� DBF�� loganchor�� flush�ϱ� ����
     (note-2) Commit Pending�� Resource ������ �������� ����
              -> DBF ���������� ó���� ���� ����. ( Pending �Լ� ���ʿ� )

*/
IDE_RC sddUpdate::redo_SCT_UPDATE_DRDB_ALTER_DBF_ONLINE(
                        idvSQL    * /* aStatistics */,
                        void      * aTrans,
                        smLSN       /* aCurLSN */,
                        scSpaceID   aSpaceID,
                        UInt        aFileID,
                        UInt        aValueSize,
                        SChar     * aValuePtr,
                        idBool      /*aIsRestart*/ )
{
    UInt                 sDBFState;
    sddTableSpaceNode  * sSpaceNode;
    sddDataFileNode    * sFileNode;
    sctPendingOp       * sPendingOp;

    IDE_TEST( getTBSDBF( aSpaceID, aFileID, &sSpaceNode, &sFileNode )
              != IDE_SUCCESS );

    if (( sSpaceNode != NULL ) &&
        ( sFileNode  != NULL ))
    {
        IDE_TEST( getAlterTBSOnOffImage( aValueSize,
                                         aValuePtr,
                                         & sDBFState ) != IDE_SUCCESS );

        if ( smLayerCallback::isBeginTrans( aTrans ) == ID_TRUE )
        {
            // Commit Pending���
            // Transaction Commit�ÿ� ������ Pending Operation���
            IDE_TEST( sddDataFile::addPendingOperation(
                          aTrans,
                          sFileNode,
                          ID_TRUE, /* Pending ���� ���� ���� : Commit �� */
                          SCT_POP_ALTER_DBF_ONLINE,
                          & sPendingOp ) != IDE_SUCCESS );

            // Commit�� sctTableSpaceMgr::executePendingOperation
            // (note-2) Commit Pending�� Resource ������ �������� �ʴ´�.
            sPendingOp->mNewDBFState   = sDBFState;
            // pending �� ó���� �Լ��� ����.
            sPendingOp->mPendingOpFunc = NULL;
        }
        else
        {
            // ActiveTx�� �ƴ� ��� Pending ������� �ʴ´�.
        }
    }
    else
    {
        // �̹� Drop�� DBF�� ���
        // nothing to do ...
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/*
    ALTER TABLESPACE TBS1 ONLINE .... �� ���� UNDO ����

    [ �α� ���� ]
    Before Image  --------------------------------------------
      UInt                aBState

    [ ALTER TABLESPACE ONLINE �� UNDO ó�� ]
      (u-050)  TBSNode.Status := Before Image(OFFLINE)
      (note-1) TBSNode�� loganchor�� flush���� ����
               -> ALTER TBS ONLINEE�� Commit Pending�� ����
                  COMMIT���Ŀ��� ����� TBS���°� log anchor�� flush�Ǳ� ����
*/
IDE_RC sddUpdate::undo_SCT_UPDATE_DRDB_ALTER_DBF_ONLINE(
                        idvSQL        * /* aStatistics */,
                        void          * /*aTrans*/,
                        smLSN           /* aCurLSN */,
                        scSpaceID       aSpaceID,
                        UInt            aFileID,
                        UInt            aValueSize,
                        SChar         * aValuePtr,
                        idBool          /*aIsRestart*/ )
{
    UInt                sDBFState;
    sddTableSpaceNode * sSpaceNode;
    sddDataFileNode   * sFileNode;

    IDE_TEST( getTBSDBF( aSpaceID, aFileID, &sSpaceNode, &sFileNode )
              != IDE_SUCCESS );

    if (( sSpaceNode != NULL ) &&
        ( sFileNode  != NULL ))
    {
        IDE_TEST( getAlterDBFOnOffImage( aValueSize,
                                         aValuePtr,
                                         & sDBFState ) != IDE_SUCCESS );

        // (u-010)
        // ����̰ų� Restart Recovery�� ��� Before State�� �����Ѵ�.
        sFileNode->mState = sDBFState;
    }
    else
    {
        // �̹� Drop�� DBF�� ���
        // nothing to do ...
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*
    DRDB_ALTER_ONLINE_DBF/OFFLINE_DBF�� ���� Log Image�� �м��Ѵ�.

    [IN]  aValueSize     - Log Image �� ũ��
    [IN]  aValuePtr      - Log Image
    [OUT] aState         - DBF�� ����
*/
IDE_RC sddUpdate::getAlterDBFOnOffImage( UInt       aValueSize,
                                         SChar    * aValuePtr,
                                         UInt     * aState )
{
    IDE_DASSERT( aValuePtr != NULL );
    IDE_DASSERT( aValueSize == (UInt)( ID_SIZEOF(*aState)));
    IDE_DASSERT( aState   != NULL );

    ACP_UNUSED( aValueSize );

    idlOS::memcpy(aState, aValuePtr, ID_SIZEOF(*aState));
    aValuePtr += ID_SIZEOF(*aState);

    return IDE_SUCCESS;
}
