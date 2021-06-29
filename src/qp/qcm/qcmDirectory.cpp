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
 * $Id: qcmDirectory.cpp 82075 2018-01-17 06:39:52Z jina.kim $
 *
 * Description :
 *
 *     [PROJ-1371] Directories
 *
 *     Directories�� ���� ��Ÿ ����
 *
 **********************************************************************/

#include <idl.h>
#include <smiDef.h>
#include <qdParseTree.h>
#include <qcmDirectory.h>
#include <qcg.h>

const void * gQcmDirectories;
const void * gQcmDirectoriesIndex[QCM_MAX_META_INDICES];

IDE_RC qcmDirectory::getDirectory(
    qcStatement      * aStatement,
    SChar            * aDirectoryName,
    SInt               aDirectoryNameLen,
    qcmDirectoryInfo * aDirectoryInfo,
    idBool           * aExist )
{
/***********************************************************************
 *
 * Description :
 *      meta���� directory�� ����
 *
 * Implementation :
 *      1. directory name���� meta range����
 *      2. qcmDirectoryInfo����ü�� ��� ����
 *
 ***********************************************************************/

    vSLong             sRowCount;
    smiRange           sRange;
    qtcMetaRangeColumn sMetaRange;

    qcNameCharBuffer   sDirNameBuffer;
    mtdCharType      * sDirName = (mtdCharType *)&sDirNameBuffer;
    mtcColumn        * sFirstMtcColumn;

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aDirectoryName != NULL );
    IDE_DASSERT( aDirectoryInfo != NULL );

    qtc::setVarcharValue( sDirName,
                          NULL,
                          aDirectoryName,
                          aDirectoryNameLen );

    IDE_TEST( smiGetTableColumns( gQcmDirectories,
                                  QCM_DIRECTORIES_DIRECTORY_NAME,
                                  (const smiColumn**)&sFirstMtcColumn )
              != IDE_SUCCESS );

    qcm::makeMetaRangeSingleColumn(
        &sMetaRange,
        sFirstMtcColumn,
        (const void*)sDirName,
        &sRange );

    IDE_TEST(
        qcm::selectRow(
            QC_SMI_STMT(aStatement),
            gQcmDirectories,
            smiGetDefaultFilter(),
            &sRange,
            gQcmDirectoriesIndex[QCM_DIRECTORIES_DIRECTORYNAME_IDX_ORDER],
            (qcmSetStructMemberFunc)qcmSetDirectory,
            (void*)aDirectoryInfo,
            0,
            1,
            &sRowCount )
        != IDE_SUCCESS );

    if( sRowCount == 0 )
    {
        *aExist = ID_FALSE;
    }
    else if( sRowCount == 1 )
    {
        *aExist = ID_TRUE;
    }
    else
    {
        IDE_RAISE(ERR_META_CRASH);
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_META_CRASH);
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QCM_META_CRASH));
    }

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qcmDirectory::addMetaInfo(
    qcStatement * aStatement,
    UInt          aUserID,
    SChar       * aDirectoryName,
    SChar       * aDirectoryPath,
    idBool        aReplace )
{
/***********************************************************************
 *
 * Description :
 *      meta�� directory�� ����(or update)
 *
 * Implementation :
 *      1. ���� replace��� update(���� ������ ���� �ּ�ȭ)
 *         path�� update(owner�� update���� ����)
 *      2. replace�� �ƴ϶�� insert
 *         directory oid�� sequence�� ���
 *
 ***********************************************************************/

    SChar     * sSqlStr;
    vSLong      sRowCnt;
    SLong       sDirectoryID;

    IDE_TEST( STRUCT_ALLOC_WITH_SIZE( aStatement->qmxMem,
                                      SChar,
                                      QD_MAX_SQL_LENGTH,
                                      &sSqlStr )
              != IDE_SUCCESS);

    if( aReplace == ID_TRUE )
    {
        idlOS::snprintf( sSqlStr, QD_MAX_SQL_LENGTH,
                         "UPDATE SYS_DIRECTORIES_ SET "
                         "DIRECTORY_PATH = '%s', "
                         "LAST_DDL_TIME = SYSDATE "
                         "WHERE DIRECTORY_NAME = '%s'",
                         aDirectoryPath,
                         aDirectoryName );
    }
    else
    {
        IDE_TEST( qcm::getNextDirectoryID( aStatement,
                                           &sDirectoryID )
                  != IDE_SUCCESS );

        idlOS::snprintf( sSqlStr, QD_MAX_SQL_LENGTH,
                         "INSERT INTO SYS_DIRECTORIES_ "
                         "VALUES ( "
                         "%"ID_INT64_FMT","
                         "%"ID_INT32_FMT", "
                         "'%s', "
                         "'%s', "
                         "SYSDATE, SYSDATE )",
                         sDirectoryID,
                         aUserID,
                         aDirectoryName,
                         aDirectoryPath );
    }

    IDE_TEST(qcg::runDMLforDDL( QC_SMI_STMT( aStatement ),
                                sSqlStr,
                                & sRowCnt ) != IDE_SUCCESS);

    IDE_TEST_RAISE( sRowCnt != 1, ERR_META_CRASH );

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_META_CRASH);
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QCM_META_CRASH));
    }

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qcmDirectory::delMetaInfoByDirectoryName(
    qcStatement * aStatement,
    SChar       * aDirectoryName )
{
/***********************************************************************
 *
 * Description :
 *      meta���� directory�� ����(drop direcoory�� ���)
 *
 * Implementation :
 *      1. sys_directories_���� directory name�� ������ object����
 *
 ***********************************************************************/

    SChar     * sSqlStr;
    vSLong      sRowCnt;

    IDE_TEST( STRUCT_ALLOC_WITH_SIZE( aStatement->qmxMem,
                                      SChar,
                                      QD_MAX_SQL_LENGTH,
                                      &sSqlStr )
              != IDE_SUCCESS);


    idlOS::snprintf((SChar*)sSqlStr, QD_MAX_SQL_LENGTH,
                    "DELETE FROM SYS_DIRECTORIES_ WHERE "
                    "DIRECTORY_NAME = '%s'", aDirectoryName);

    IDE_TEST(qcg::runDMLforDDL( QC_SMI_STMT( aStatement ),
                                (SChar*)sSqlStr, &sRowCnt) != IDE_SUCCESS);

    IDE_TEST_RAISE( sRowCnt != 1, ERR_META_CRASH );

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_META_CRASH);
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QCM_META_CRASH));
    }

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qcmSetDirectory(
    idvSQL           * /* aStatistics */,
    const void       * aRow,
    qcmDirectoryInfo * aDirectoryInfo )
{
    mtcColumn   * sDirPathColInfo;
    mtdCharType * sDirPathStr;
    SLong         sSLongID;
    mtcColumn   * sDirectoryIDMtcColumn;
    mtcColumn   * sUserIDMtcColumn;
    mtcColumn   * sDirectoryNameMtcColumn;

    IDE_TEST( smiGetTableColumns( gQcmDirectories,
                                  QCM_DIRECTORIES_DIRECTORY_ID,
                                  (const smiColumn**)&sDirectoryIDMtcColumn )
              != IDE_SUCCESS );

    qcm::getBigintFieldValue(
        aRow,
        sDirectoryIDMtcColumn,
        &sSLongID );
    // BUGBUG 32bit machine���� ���� �� SLong(64bit)������ uVLong(32bit)������
    // ��ȯ�ϹǷ� ������ �ս� ���ɼ� ����
    aDirectoryInfo->directoryID = (vULong)sSLongID;

    IDE_TEST( smiGetTableColumns( gQcmDirectories,
                                  QCM_DIRECTORIES_USER_ID,
                                  (const smiColumn**)&sUserIDMtcColumn )
              != IDE_SUCCESS );

    qcm::getIntegerFieldValue(
        aRow,
        sUserIDMtcColumn,
        &aDirectoryInfo->userID );

    IDE_TEST( smiGetTableColumns( gQcmDirectories,
                                  QCM_DIRECTORIES_DIRECTORY_NAME,
                                  (const smiColumn**)&sDirectoryNameMtcColumn )
              != IDE_SUCCESS );

    qcm::getCharFieldValue(
        aRow,
        sDirectoryNameMtcColumn,
        aDirectoryInfo->directoryName );

    IDE_TEST( smiGetTableColumns( gQcmDirectories,
                                  QCM_DIRECTORIES_DIRECTORY_PATH,
                                  (const smiColumn**)&sDirPathColInfo )
              != IDE_SUCCESS );

    IDE_TEST(mtd::moduleById(
                 &(sDirPathColInfo->module),
                 sDirPathColInfo->type.dataTypeId)
             != IDE_SUCCESS);

    IDE_TEST(mtl::moduleById( &sDirPathColInfo->language,
                              sDirPathColInfo->type.languageId )
             != IDE_SUCCESS);

    sDirPathStr = (mtdCharType*)
        mtc::value( sDirPathColInfo, aRow, MTD_OFFSET_USE );

    if( sDirPathStr->length > 0 )
    {
        idlOS::memcpy( aDirectoryInfo->directoryPath,
                       sDirPathStr->value,
                       sDirPathStr->length );
        aDirectoryInfo->directoryPath[sDirPathStr->length] = '\0';
    }
    else
    {
        aDirectoryInfo->directoryPath[0] = '\0';
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}
