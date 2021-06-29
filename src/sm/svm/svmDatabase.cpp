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
 
#include <idl.h>
#include <idm.h>
#include <idu.h>
#include <ideErrorMgr.h>
#include <smErrorCode.h>
#include <smDef.h>
#include <svmDef.h>
#include <svmDatabase.h>
#include <smu.h>
#include <smr.h>

/* �����ͺ��̽� ������ membase�� �ʱ�ȭ�Ѵ�.
 * aDBName           [IN] �����ͺ��̽� �̸�
 * aDbFilePageCount  [IN] �ϳ��� �����ͺ��̽� ������ ������ Page��
 * aChunkPageCount   [IN] �ϳ��� Expand Chunk�� Page��
 */
IDE_RC svmDatabase::initializeMembase( svmTBSNode * aTBSNode,
                                       SChar *      aDBName,
                                       vULong       aChunkPageCount )
{
    SInt i;

    IDE_ASSERT( aDBName != NULL );
    IDE_ASSERT( idlOS::strlen( aDBName ) < SM_MAX_DB_NAME );
    IDE_ASSERT( aChunkPageCount > 0 );

    idlOS::strncpy( aTBSNode->mMemBase.mDBname,
                    aDBName,
                    SM_MAX_DB_NAME - 1 );
    aTBSNode->mMemBase.mDBname[ SM_MAX_DB_NAME - 1 ] = '\0';

    aTBSNode->mMemBase.mAllocPersPageCount = 0;
    aTBSNode->mMemBase.mExpandChunkPageCnt = aChunkPageCount ;
    aTBSNode->mMemBase.mCurrentExpandChunkCnt = 0;

    // Free Page List�� �ʱ�ȭ �Ѵ�.
    for ( i = 0 ; i< SVM_MAX_FPL_COUNT ; i++ )
    {
        aTBSNode->mMemBase.mFreePageLists[i].mFirstFreePageID = SM_NULL_PID;
        aTBSNode->mMemBase.mFreePageLists[i].mFreePageCount = 0;
    }

    aTBSNode->mMemBase.mFreePageListCount = SVM_FREE_PAGE_LIST_COUNT;

    return IDE_SUCCESS;
}

/*
 * Expand Chunk���õ� ������Ƽ ������ ����� �� ������ üũ�Ѵ�.
 *
 * 1. �ϳ��� Chunk���� ������ �������� ���� Free Page List�� �й��� ��,
 *    �ּ��� �ѹ��� �й谡 �Ǵ��� üũ
 *
 *    �������� : Chunk�� �������������� >= 2 * List�� �й��� Page�� * List ��
 *
 * aChunkDataPageCount [IN] Expand Chunk���� ������������ ��
 *                          ( FLI Page�� ������ Page�� �� )
 */
IDE_RC svmDatabase::checkExpandChunkProps( svmMemBase * aMemBase )
{
    IDE_DASSERT( aMemBase != NULL );

    // ����ȭ�� Free Page List����  createdb������ �ٸ� ���
    IDE_TEST_RAISE( aMemBase->mFreePageListCount !=
                    SVM_FREE_PAGE_LIST_COUNT,
                    different_page_list_count );

    // Expand Chunk���� Page���� createdb������ �ٸ� ���
    IDE_TEST_RAISE( aMemBase->mExpandChunkPageCnt !=
                    smuProperty::getExpandChunkPageCount() ,
                    different_expand_chunk_page_count );

    //  Expand Chunk�� �߰��� ��
    //  ( �����ͺ��̽� Chunk�� ���� �Ҵ�� ��  )
    //  �� ���� Free Page���� �������� ����ȭ�� Free Page List�� �й�ȴ�.
    //
    //  �� ��, ������ Free Page List�� �ּ��� �ϳ��� Free Page��
    //  �й�Ǿ�� �ϵ��� �ý����� ��Ű���İ� ����Ǿ� �ִ�.
    //
    //  ���� Expand Chunk���� Free Page���� ������� �ʾƼ�,
    //  PER_LIST_DIST_PAGE_COUNT ���� ��� Free Page List�� �й��� ����
    //  ���ٸ� ������ �߻���Ų��.
    //
    //  Expand Chunk���� Free List Info Page�� ������
    //  ���������������� Free Page List�� �й�ǹǷ�, �� ������ üũ�ؾ� �Ѵ�.
    //  �׷���, �̷��� ��� ������ �Ϲ� ����ڰ� �����ϱ⿡�� �ʹ� �����ϴ�.
    //
    //  Expand Chunk�� ������ ���� Free Page List�� �ι��� �й��� �� ������ŭ
    //  ����� ũ�⸦ �������� �����Ѵ�.
    //
    //  ���ǽ� : EXPAND_CHUNK_PAGE_COUNT <=
    //           2 * PER_LIST_DIST_PAGE_COUNT * PAGE_LIST_GROUP_COUNT
   IDE_TEST_RAISE( aMemBase->mExpandChunkPageCnt <
                   ( 2 * SVM_PER_LIST_DIST_PAGE_COUNT * aMemBase->mFreePageListCount ),
                   err_too_many_per_list_page_count );

    return IDE_SUCCESS;

    IDE_EXCEPTION(different_expand_chunk_page_count);
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_DIFFERENT_DB_EXPAND_CHUNK_PAGE_COUNT,
                                SVM_FREE_PAGE_LIST_COUNT,
                                aMemBase->mExpandChunkPageCnt ));
    }
    IDE_EXCEPTION(different_page_list_count);
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_DIFFERENT_DB_FREE_PAGE_LIST_COUNT,
                                SVM_FREE_PAGE_LIST_COUNT,
                                aMemBase->mFreePageListCount ));
    }
    IDE_EXCEPTION( err_too_many_per_list_page_count );
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_TOO_MANY_PER_LIST_PAGE_COUNT_ERROR,
                                (ULong) aMemBase->mExpandChunkPageCnt,
                                (ULong) SVM_PER_LIST_DIST_PAGE_COUNT,
                                (ULong) aMemBase->mFreePageListCount ));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

