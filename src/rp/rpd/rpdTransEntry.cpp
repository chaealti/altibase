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
 
#include <rpDef.h>
#include <rpuProperty.h>
#include <rpdTransTbl.h>

/* TASK-6548 unique key duplication */
IDE_RC rpdTransEntry::rollback( smTID aTID )
{
    if ( mTransForConflictResolution != NULL )
    {
        IDE_TEST_RAISE( mTransForConflictResolution->mSmiTrans.rollback(
                            NULL, SMI_DO_NOT_RELEASE_TRANSACTION )
                        != IDE_SUCCESS, ERR_TX_CONFLICT_RESOLUTION_ABORT );
    }
    else
    {
        /* Nothing to do */
    }
    IDE_TEST_RAISE( mRpdTrans->mSmiTrans.rollback( NULL, SMI_DO_NOT_RELEASE_TRANSACTION )
                    != IDE_SUCCESS, ERR_TX_ABORT );

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_TX_CONFLICT_RESOLUTION_ABORT );
    {
        ideLog::log( IDE_RP_0, "conflict resolution transaction abort error [TID : %"ID_UINT32_FMT"]", aTID );
    }
    IDE_EXCEPTION(ERR_TX_ABORT);
    {
        ideLog::log( IDE_RP_0, "transaction abort error [TID : %"ID_UINT32_FMT"]", aTID );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC rpdTransEntry::commit( smTID aTID )
{
    IDE_ASSERT( mRpdTrans != NULL );

    if ( mTransForConflictResolution != NULL )
    {
        IDE_TEST_RAISE( mTransForConflictResolution->mSmiTrans.commit(
                                        NULL, /* aCommitSCN */
                                        SMI_DO_NOT_RELEASE_TRANSACTION )
                        != IDE_SUCCESS, ERR_TX_CONFLICT_RESOLUTION_COMMIT );
    }
    else
    {
        /* Nothing to do */
    }

    IDE_TEST_RAISE( mRpdTrans->mSmiTrans.commit( NULL, /* aCommitSCN */
                                                 SMI_DO_NOT_RELEASE_TRANSACTION )
                    != IDE_SUCCESS, ERR_TX_COMMIT );

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_TX_CONFLICT_RESOLUTION_COMMIT );
    {
        ideLog::log( IDE_RP_0, "conflict resolution transaction commit error [TID : %"ID_UINT32_FMT"]", aTID );
    }
    IDE_EXCEPTION( ERR_TX_COMMIT );
    {
        ideLog::log( IDE_RP_0, "transaction commit error [TID : %"ID_UINT32_FMT"]", aTID );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC rpdTransEntry::setSavepoint( smTID aTID, rpSavepointType  aType, SChar * aSavepointName )
{
    IDE_ASSERT( mRpdTrans != NULL );

    switch ( aType )
    {
        case RP_SAVEPOINT_PSM:
            if ( mTransForConflictResolution != NULL )
            {
                mTransForConflictResolution->mSmiTrans.reservePsmSvp( ID_FALSE );
            }
            else
            {
                /* Nothing to do */
            }
            /* BUG-21800 [RP] PSM SAVEPIONT�� ��� ���� ó�����ݴϴ�
             * PSM SVP�� ���� �κп��� reserve�� �صξ��ٰ� DML�� �Ͼ�� �κп���
             * ���� write�� �Ǿ� ��� �ǹǷ� �� ������ reserve�� ȣ���Ѵ�.
             */
            mRpdTrans->mSmiTrans.reservePsmSvp( ID_FALSE );
            mSetPSMSavepoint = ID_TRUE;;
            break;
        case RP_SAVEPOINT_IMPLICIT:
        case RP_SAVEPOINT_EXPLICIT:
            if ( mTransForConflictResolution != NULL )
            {
                IDE_TEST_RAISE( mTransForConflictResolution->mSmiTrans.savepoint( aSavepointName )
                                != IDE_SUCCESS, ERR_SET_SVP_CONFLICT_RESOLUTION );
            }
            else
            {
                /* Nothing to do */
            }
            IDE_TEST_RAISE( mRpdTrans->mSmiTrans.savepoint( aSavepointName )
                            != IDE_SUCCESS, ERR_SET_SVP );
            break;
        default:
            break;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_SET_SVP_CONFLICT_RESOLUTION );
    {
        ideLog::log( IDE_RP_0, "conflict resolution transaction set savepoint error [TID : %"ID_UINT32_FMT"]",
                     aTID );
    }
    IDE_EXCEPTION( ERR_SET_SVP );
    {
        ideLog::log( IDE_RP_0, "transaction set savepoint error [TID : %"ID_UINT32_FMT"]", aTID );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC rpdTransEntry::abortSavepoint( rpSavepointType  aType, SChar * aSavepointName )
{
    IDE_DASSERT( mRpdTrans != NULL );

    switch ( aType )
    {
        case RP_SAVEPOINT_PSM:
            mSetPSMSavepoint = ID_FALSE;
            if ( mTransForConflictResolution != NULL )
            {
                if ( mTransForConflictResolution->mSmiTrans.abortToPsmSvp()
                               != IDE_SUCCESS )
                {
                    IDE_ERRLOG( IDE_RP_0 );
                    IDE_TEST_RAISE( mTransForConflictResolution->mSmiTrans.rollback(
                                        RP_CONFLICT_RESOLUTION_BEGIN_SVP_NAME )
                                    != IDE_SUCCESS, ERR_ABORT_SVP );
                }
                else
                {
                    /* Nothing to do */
                }
            }
            else
            {
                /* Nothing to do */
            }
            IDE_TEST_RAISE( mRpdTrans->mSmiTrans.abortToPsmSvp()
                            != IDE_SUCCESS, ERR_ABORT_PSM_SVP );
            break;
        case RP_SAVEPOINT_IMPLICIT:
        case RP_SAVEPOINT_EXPLICIT:
            if ( mTransForConflictResolution != NULL )
            {
                if ( mTransForConflictResolution->mSmiTrans.rollback( aSavepointName )
                     != IDE_SUCCESS )
                {
                     IDE_ERRLOG( IDE_RP_0 );
                    /* savepoint�� ã�� �� ���� ���, ��ü rollback ��Ų��. conflict resolution ����
                       ������ set savepoint �ߴ� �Ϳ� ���ؼ��� mTransForConflictResolution�� SVP������
                       ������� �ʴ´� */
                    IDE_TEST_RAISE( mTransForConflictResolution->mSmiTrans.rollback(
                                        RP_CONFLICT_RESOLUTION_BEGIN_SVP_NAME )
                                    != IDE_SUCCESS, ERR_ABORT_SVP );
                }
                else
                {
                    /* Nothing to do */
                }
            }
            else
            {
                /* Nothing to do */
            }
            IDE_TEST_RAISE(mRpdTrans->mSmiTrans.rollback( aSavepointName )
                           != IDE_SUCCESS, ERR_ABORT_SVP );
            break;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_ABORT_SVP );
    {
        IDE_ERRLOG( IDE_RP_0 );
        IDE_SET( ideSetErrorCode( rpERR_ABORT_RP_ABORT_SAVEPOINT_ERROR_IN_RUN ) );
    }
    IDE_EXCEPTION( ERR_ABORT_PSM_SVP );
    {
        IDE_ERRLOG( IDE_RP_0 );
        IDE_SET( ideSetErrorCode( rpERR_ABORT_RP_ABORT_PSM_SVP ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}
