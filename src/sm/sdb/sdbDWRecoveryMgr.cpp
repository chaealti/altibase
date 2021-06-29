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
 * $$Id:$
 **********************************************************************/

#include <smErrorCode.h>
#include <smuProperty.h>
#include <sdbReq.h>
#include <sdbDWRecoveryMgr.h>
#include <sddDiskMgr.h>

/****************************************************************************
 * Abstraction :
 *  ��Ŀ�����ÿ� �� �Լ� �����Ŵ.
 *  �� Double write File(DWFile)���ϵ��� �����ϴ� ���丮�� ����
 *  �� ���ϵ��� ���, �� ���Ͽ� ���� recoverDWFile�� �����ϴ� ��Ȱ�� �Ѵ�.
 ****************************************************************************/
IDE_RC sdbDWRecoveryMgr::recoverCorruptedPages()
{
    UInt            i;
    SInt            sRc;
    DIR            *sDir;
    struct dirent  *sDirEnt;
    struct dirent  *sResDirEnt;
    SInt            sState = 0;
    sddDWFile       sDWFile;
    idBool          sRightDWFile;
    SChar           sFullFileName[SM_MAX_FILE_NAME + 1];
    UInt            sDwFileNamePrefixLen;
    idBool          sFound = ID_FALSE;

    /* sdbDWRecoveryMgr_recoverCorruptedPages_malloc_DirEnt.tc */
    IDU_FIT_POINT("sdbDWRecoveryMgr::recoverCorruptedPages::malloc::DirEnt");
    IDE_TEST(iduMemMgr::malloc(IDU_MEM_SM_SDB,
                               ID_SIZEOF(struct dirent) + SM_MAX_FILE_NAME,
                               (void**)&sDirEnt,
                               IDU_MEM_FORCE)
             != IDE_SUCCESS);
    sState = 1;

    sDwFileNamePrefixLen = idlOS::strlen( SDD_DWFILE_NAME_PREFIX );

    for (i = 0; i < smuProperty::getDWDirCount(); i++)
    {
        sDir = idf::opendir(smuProperty::getDWDir(i));
        if (sDir == NULL)
        {
            ideLog::log(IDE_SERVER_0,
                        "Invalidate double write directory: %s",
                        smuProperty::getDWDir(i));
            continue;
        }

        while (1)
        {
	        errno = 0;
            sRc = idf::readdir_r(sDir, sDirEnt, &sResDirEnt);
            if ( (sRc != 0) && (errno != 0) )
            {
                ideLog::log(IDE_SERVER_0,
                            "Invalidate double write directory: %s",
                            smuProperty::getDWDir(i));
                break;
            }
            if (sResDirEnt == NULL)
            {
                break;
            }

            /* BUG-24946 Server Start�� DWFile load�������� �߻��� �����ڵ带
             *           �����ϰ�, Clear ���־����.
             * ������Ƽ�� ���ǵ� DWDir ���� ��� �����̳� ���丮�� ���ؼ� load
             * �غ��� �ʰ�, prefix�� ��ġ�ϴ� �Ϳ� ���ؼ��� load�غ���. */

            if ( idlOS::strncmp( sResDirEnt->d_name,
                                 SDD_DWFILE_NAME_PREFIX,
                                 sDwFileNamePrefixLen ) != 0 )
            {
                continue;
            }

            // full file name �����
            sFullFileName[0] = '\0';
            idlOS::sprintf(sFullFileName,
                           "%s%c%s",
                           smuProperty::getDWDir(i),
                           IDL_FILE_SEPARATOR,
                           sResDirEnt->d_name );

            if ( sDWFile.load(sFullFileName, &sRightDWFile) != IDE_SUCCESS )
            {
                /* ���Ͽ� ���� permission ������ open ������ �߻��Ͽ���
                 * �����ϰڴٶ�� ���� �����ڵ带 ���ؼ� ���� ������ �����ڵ带 �����Ѵ�.
                 * �׷��� ������ ���� Exception CallStack�� ���� ����ϰԵ� */
                IDE_CLEAR();
            }

            if (sRightDWFile == ID_FALSE)
            {
                (void)sDWFile.destroy();
                continue;
            }

            sFound = ID_TRUE;

            IDE_TEST(recoverDWFile(&sDWFile) != IDE_SUCCESS);

            /* BUG-27776 the server startup can be fail since the dw file is 
             * removed after DW recovery. 
             * DWFile�� ����� ��� Reset�մϴ�.*/
            IDE_TEST( sDWFile.reset() != IDE_SUCCESS );
            (void)sDWFile.destroy();
        }
        idf::closedir(sDir);
    }

    IDE_ASSERT( smuProperty::getSMStartupTest() != 27776 );

    sState = 0;
    IDE_TEST(iduMemMgr::free(sDirEnt) != IDE_SUCCESS);
    
    /*
     * BUG-25957 [SD] ������������ restart�� DOUBLE_WRITE_DIRECTORY,
     *           DOUBLE_WRITE_DIRECTORY_COUNTproperty���� �����ϸ� corrupt�� 
     *           page�鿡���� ������ �ҿ����ϰ� �̷����� ����.
     */
    if( sFound  == ID_FALSE )
    {
        IDE_RAISE( error_dw_file_not_found )
    }
    return IDE_SUCCESS;

    IDE_EXCEPTION( error_dw_file_not_found )
    {
        IDE_SET( ideSetErrorCode( smERR_ABORT_DW_FILE_NOT_FOUND ));
    }
    IDE_EXCEPTION_END;

    if (sState > 0)
    {
        IDE_ASSERT(iduMemMgr::free(sDirEnt) == IDE_SUCCESS);
    }

    return IDE_FAILURE;
}

/****************************************************************************
 * Abstraction :
 *  DWFile�� ���� �ش� space ID�� page ID �����ͼ� 
 *  ���� DB���� �ش� �ϴ� �������� ����� �Ǿ� �ִ��� Ȯ���Ѵ�. 
 *  ���� ���� �ִٸ�, DWFile�� �ش� �κ��� ���� DB�� Write�Ѵ�.
 ****************************************************************************/
IDE_RC sdbDWRecoveryMgr::recoverDWFile(sddDWFile *aDWFile)
{
    SChar      *sAllocPtr;
    UChar      *sDWBuffer;
    UChar      *sDataBuffer;
    SInt        sState = 0;
    idBool      sIsPageValid;
    scPageID    sPageID;
    scSpaceID   sSpaceID;
    UInt        sFileID;
    UInt        i;

    /* TC/FIT/Limit/sm/sdbDWRecoveryMgr_recoverDWFile_malloc.sql */
    IDU_FIT_POINT_RAISE( "sdbDWRecoveryMgr::recoverDWFile::malloc",
                          insufficient_memory );

    IDE_TEST_RAISE(iduMemMgr::malloc(IDU_MEM_SM_SDB,
                                     aDWFile->getPageSize() * 3,
                                     (void**)&sAllocPtr) != IDE_SUCCESS,
                   insufficient_memory );
    sState = 1;

    sDWBuffer = (UChar*)idlOS::align(sAllocPtr, aDWFile->getPageSize());
    sDataBuffer = sDWBuffer + aDWFile->getPageSize();

    for (i = 0; i < aDWFile->getPageCount(); i++)
    {
        IDE_TEST(aDWFile->read(NULL, (SChar*)sDWBuffer, i)
                 != IDE_SUCCESS);

        sPageID = smLayerCallback::getPageID( sDWBuffer );
        sSpaceID = ((sdbFrameHdr*)(sDWBuffer))->mSpaceID;

        // disk manager�� �ʿ�
        // ���� ������ ���� dw buffer frame�� ���
        // �����Ѵ�. �ʱ�ȭ�� ���� �״���� ����.
        if((sSpaceID == SC_NULL_SPACEID) && (sPageID == SC_NULL_PID))
        {
            continue;
        }

        // Fix BUG-17158 �� ���� ����
        // offline Ȥ�� Discard Disk TBS�� ���ؼ� Double Write Buffer�κ���
        // ������ �������� �ʴ´�.
        // 1. offline ���� ����� flush�� �Ϸ�� ���Ŀ� Offline ���·� ����Ǳ�
        // ������ flush �ߴ� Page���� ��� �����ϰ� ����Ÿ���Ͽ� sync����
        // �����Ѵ�.
        // 2. discard TBS�� Control �ܰ迡�� ������ �Ǳ� ������ �� ���Ŀ���
        // �����ϸ� �ȴ�.
        sIsPageValid = sddDiskMgr::isValidPageID( NULL,
                                                  sSpaceID,
                                                  sPageID );

        if (sIsPageValid != ID_TRUE)
        {
            continue;
        }

        /* BUG-19477 [SM-DISK] IOB������ Temp Tablespace�� Meta�������� ���� ���
         *           ������ Media Recovery �� ������ ����
         *
         * Temp Tablespace�� Meta ������ ���� DWA(Double Write Area)�� ����� �ȴ�.
         * ������ Temp Tablespace�� Media Recovery����� �ƴϱ⶧���� Media Recovery��
         * Temp�� ������� �Ǿ� �ʱ�ũ��� ���������. �׷��� Media Recovery�� �ʱ�ũ��
         * ���� �̹����� DWA�� ����� �Ǿ������� Media Recovery�� �������� �ʴ� ������
         * �� ���ؼ� Read�� ��û�Ͽ� ������ ���� �� �ֽ��ϴ�.
         *
         **/
        if ( smLayerCallback::isTempTableSpace( sSpaceID ) == ID_TRUE )
        {
            continue;
        }

        IDE_TEST(sddDiskMgr::read(NULL,
                                  sSpaceID,
                                  sPageID,
                                  sDataBuffer,
                                  &sFileID)
                 != IDE_SUCCESS);

        if ( smLayerCallback::isPageCorrupted( sDataBuffer ) == ID_TRUE )
        {
            if ( smLayerCallback::isPageCorrupted( sDWBuffer ) == ID_TRUE)
            {
                IDE_RAISE(page_corruption_error);
            }

            // offline TBS�� double write buffer�� �����ϴ� ��찡 �ִٸ�
            // �� ������ offline �����߿� ���� ���� �ƴ϶�
            // offline�� �Ϸ�� ���Ŀ� �ܺο��ο� ���� �������̶� �ϰڴ�.
            // double write buffer�� buffer manager�� flush�ϴ� ���߿�
            // crash�� �߻��Ͽ� ������ ���� �����Ϸ��� ������ ��� Media
            // ������ ���� ������ �����ϴ� ���� �ƴϴ�.
            IDE_TEST(sddDiskMgr::write(NULL,
                                       sSpaceID,
                                       sPageID,
                                       sDWBuffer)
                     != IDE_SUCCESS);

            ideLog::log(SM_TRC_LOG_LEVEL_BUFFER,
                        SM_TRC_BUFFER_CORRUPTED_PAGE,
                        sSpaceID,
                        sPageID);
        }
    }

    sState = 0;
    IDE_TEST(iduMemMgr::free(sAllocPtr) != IDE_SUCCESS);

    IDE_TEST(sddDiskMgr::syncAllTBS(NULL,
                                    SDD_SYNC_NORMAL)
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION(page_corruption_error);
    {
        IDE_SET(ideSetErrorCode(smERR_FATAL_PageCorrupted,
                                sSpaceID,
                                sPageID));
    }
    IDE_EXCEPTION( insufficient_memory );
    {
        IDE_SET(ideSetErrorCode(idERR_ABORT_InsufficientMemory));
    }
    IDE_EXCEPTION_END;

    if (sState > 0)
    {
        IDE_ASSERT(iduMemMgr::free(sAllocPtr) == IDE_SUCCESS);
    }

    return IDE_FAILURE;
}

