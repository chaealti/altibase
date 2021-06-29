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
* $Id: smmPLoadMgr.cpp 82075 2018-01-17 06:39:52Z jina.kim $
**********************************************************************/

#include <idl.h>
#include <idm.h>
#include <idu.h>
#include <ideErrorMgr.h>
#include <smm.h>

IDE_RC smmPLoadMgr::initializePloadMgr(smmTBSNode *     aTBSNode,
                                       SInt             aThreadCount )
{
    SInt i;

    mTBSNode         = aTBSNode;
    mThreadCount     = aThreadCount;
    mCurrentDB       = mTBSNode->mTBSAttr.mMemAttr.mCurrentDB;
    mCurrFileNumber  = 0;
    mStartReadPageID = 0;

    mMaxFileNumber = smmManager::getRestoreDBFileCount( aTBSNode );

    /* TC/FIT/Limit/sm/smm/smmPLoadMgr_initializePloadMgr_calloc.sql */
    IDU_FIT_POINT_RAISE( "smmPLoadMgr::initializePloadMgr::calloc",
                          insufficient_memory );

    // client �޸� �Ҵ� 
    IDE_TEST_RAISE(iduMemMgr::calloc(IDU_MEM_SM_SMM,
                               aThreadCount,
                               ID_SIZEOF(smmPLoadChild *),
                               (void**)&mChildArray) != IDE_SUCCESS,
                   insufficient_memory );
    
    for (i = 0; i < aThreadCount; i++)
    {
        /* TC/FIT/Limit/sm/smm/smmPLoadMgr_initializePloadMgr_malloc.sql */
        IDU_FIT_POINT_RAISE( "smmPLoadMgr::initializePloadMgr::malloc",
                              insufficient_memory );

        IDE_TEST_RAISE(iduMemMgr::malloc(IDU_MEM_SM_SMM,
                                   ID_SIZEOF(smmPLoadChild),
                                   (void**)&(mChildArray[i])) != IDE_SUCCESS,
                       insufficient_memory );
        
        mChildArray[i] = new (mChildArray[i]) smmPLoadChild();
    }
    
    // �Ŵ��� �ʱ�ȭ 
    IDE_TEST(smtPJMgr::initialize(aThreadCount,
                                  (smtPJChild **)mChildArray,
                                  &mSuccess)
                     != IDE_SUCCESS);
    
    return IDE_SUCCESS;

    IDE_EXCEPTION( insufficient_memory );
    {
        IDE_SET(ideSetErrorCode(idERR_ABORT_InsufficientMemory));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
    
}

IDE_RC smmPLoadMgr::destroy()
{
    
    SInt i;

    IDE_ASSERT(smtPJMgr::destroy() == IDE_SUCCESS);
    
    for (i = 0; i < mThreadCount; i++)
    {
        IDE_TEST(mChildArray[i]->destroy() != IDE_SUCCESS);

        IDE_TEST(iduMemMgr::free(mChildArray[i]) != IDE_SUCCESS);
    }

    IDE_TEST(iduMemMgr::free(mChildArray) != IDE_SUCCESS);

    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
    
}

IDE_RC smmPLoadMgr::assignJob(SInt    aReqChild,
                              idBool *aJobAssigned)
{
    
    smmDatabaseFile *s_DbFile;
    ULong            s_nFileSize = 0;

    ULong            sWrittenPageCount;
    scPageID         sPageCountPerFile;
    
    
    if ( mCurrFileNumber < mMaxFileNumber)
    {
        // �� ���Ͽ� ����� �� �ִ� Page�� �� 
        sPageCountPerFile = smmManager::getPageCountPerFile( mTBSNode,
                                                             mCurrFileNumber );
        
        // DB ������ Disk�� �����Ѵٸ�?
        if ( smmDatabaseFile::isDBFileOnDisk( mTBSNode,
                                              mCurrentDB,
                                              mCurrFileNumber ) == ID_TRUE )
        {
            IDE_TEST( smmManager::openAndGetDBFile( mTBSNode,
                                                    mCurrentDB,
                                                    mCurrFileNumber,
                                                    &s_DbFile )
                      != IDE_SUCCESS );
            
        
            // ���� ���Ͽ� ��ϵ� Page�� ���� ����Ѵ�.
            IDE_TEST( s_DbFile->getFileSize(&s_nFileSize) 
                      != IDE_SUCCESS );

            if ( s_nFileSize > SM_DBFILE_METAHDR_PAGE_SIZE )
            {
                sWrittenPageCount = (s_nFileSize-
                                SM_DBFILE_METAHDR_PAGE_SIZE)
                                / SM_PAGE_SIZE;
            
                mChildArray[aReqChild]->setFileToBeLoad(
                    mTBSNode,
                    mCurrFileNumber, 
                    mStartReadPageID,
                    mStartReadPageID + sPageCountPerFile - 1,
                    sWrittenPageCount );
                *aJobAssigned = ID_TRUE;
            }
            else
            {
                // To FIX BUG-18630
                // 8K���� ���� DB������ ������ ��� Restart Recovery������
                //
                // 8K���� ���� ũ���� DB���Ͽ���
                // ������ �������� ��ϵ��� ���� ���̹Ƿ�
                // DB���� Restore�� SKIP�Ѵ�.
                *aJobAssigned = ID_FALSE;
            }
        }
        else // DB������ �������� �ʴ� ���
        {
            /*
             * To Fix BUG-13966  checkpoint�� DB���� ��ó �������� ����
             *                   ��Ȳ���� server ���� ��, startup�� �ȵ� 
             *
             * Checkpoint Thread �� Chunk�� Ȯ���ϴ� Insert Transaction
             * ������ �۾��� ������ ���� ����Ǹ�,
             * Checkpoint���� DB������ ��ó ������ ���� �� �ִ�.
             *
             * �ڼ��� ������ smmManager::loadSerial2�� �ּ��� ����
             */

            // do nothing !
            // Job�� Assign���� �ʾ����Ƿ� Flag����!
            *aJobAssigned = ID_FALSE;
        }

        // PROJ-1490
        // DB���Ͼ��� Free Page�� Disk�� ���������� �ʰ�
        // �޸𸮷� �ö����� �ʴ´�.
        // �׷��Ƿ�, DB������ ũ��� DB���Ͽ� ����Ǿ�� �� Page���ʹ�
        // �ƹ��� ���谡 ����.
        //
        // �� DB������ ����ؾ��� Page�� ���� ����Ͽ�
        // �� DB������ �ε� ���� Page ID�� ����Ѵ�.
        mStartReadPageID += sPageCountPerFile ;
        
        mCurrFileNumber++;
    }
    else
    {
        *aJobAssigned = ID_FALSE;
    }
    
    smmManager::setStartupPID(mTBSNode, mStartReadPageID);

    return IDE_SUCCESS;
    
    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
    
}
