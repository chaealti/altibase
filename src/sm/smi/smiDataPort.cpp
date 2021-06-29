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
 * $Id: smiDataPort.cpp
 **********************************************************************/
/**************************************************************
 * FILE DESCRIPTION : smiDataPort.cpp                        *
 * -----------------------------------------------------------*
 *
 * Proj-2059 DB Upgrade ���
 * Server �߽������� �����͸� �������� �ִ� ���
 **************************************************************/

#include <idl.h>
#include <ide.h>
#include <smErrorCode.h>

#include <smiDef.h>
#include <smiDataPort.h>
#include <smiMisc.h>
#include <scpDef.h>
#include <scpfModule.h>
#include <scpManager.h>


smiDataPortHeaderColDesc  gSmiDataPortHeaderColDesc[]=
{
    {
        (SChar*)"IS_BIGENDIAN",
        SMI_DATAPORT_HEADER_OFFSETOF( smiDataPortHeader, mIsBigEndian ),
        SMI_DATAPORT_HEADER_SIZEOF( smiDataPortHeader, mIsBigEndian ),
        0, NULL,
        SMI_DATAPORT_HEADER_TYPE_INTEGER
    },
    {
        (SChar*)"COMPILE_BIT",
        SMI_DATAPORT_HEADER_OFFSETOF( smiDataPortHeader, mCompileBit ),
        SMI_DATAPORT_HEADER_SIZEOF( smiDataPortHeader, mCompileBit ),
        0, NULL,
        SMI_DATAPORT_HEADER_TYPE_INTEGER
    },
    {
        (SChar*)"DB_CHARSET",
        SMI_DATAPORT_HEADER_OFFSETOF( smiDataPortHeader, mDBCharSet ),
        IDN_MAX_CHAR_SET_LEN,
        0, NULL,
        SMI_DATAPORT_HEADER_TYPE_CHAR
    },
    {
        (SChar*)"NATIONAL_CHARSET",
        SMI_DATAPORT_HEADER_OFFSETOF( smiDataPortHeader, mNationalCharSet ),
        IDN_MAX_CHAR_SET_LEN,
        0, NULL,
        SMI_DATAPORT_HEADER_TYPE_CHAR
    },
    {
        (SChar*)"COLUMN_COUNT",
        SMI_DATAPORT_HEADER_OFFSETOF( smiDataPortHeader , mColumnCount ),
        SMI_DATAPORT_HEADER_SIZEOF( smiDataPortHeader , mColumnCount ),
        0, NULL,
        SMI_DATAPORT_HEADER_TYPE_INTEGER
    },
    {
        (SChar*)"BASIC_COLUMN_COUNT",
        SMI_DATAPORT_HEADER_OFFSETOF( smiDataPortHeader , mBasicColumnCount ),
        SMI_DATAPORT_HEADER_SIZEOF( smiDataPortHeader , mBasicColumnCount ),
        0, NULL,
        SMI_DATAPORT_HEADER_TYPE_INTEGER
    },
    {
        (SChar*)"LOB_COLUMN_COUNT",
        SMI_DATAPORT_HEADER_OFFSETOF( smiDataPortHeader , mLobColumnCount ),
        SMI_DATAPORT_HEADER_SIZEOF( smiDataPortHeader , mLobColumnCount ),
        0, NULL,
        SMI_DATAPORT_HEADER_TYPE_INTEGER
    },
    {
        (SChar*)"PARTITION_COUNT",
        SMI_DATAPORT_HEADER_OFFSETOF( smiDataPortHeader , mPartitionCount ),
        SMI_DATAPORT_HEADER_SIZEOF( smiDataPortHeader , mPartitionCount ),
        0, NULL,
        SMI_DATAPORT_HEADER_TYPE_INTEGER
    },
    {
        NULL,
        0,
        0,
        0,0,
        0
    }
};

IDE_RC gSmiDataPortHeaderValidation( void * aDesc, 
                                      UInt   aVersion,
                                      void * aHeader )
{
    smiDataPortHeaderDesc   * sDesc;
    smiDataPortHeader       * sHeader;
    SChar                   * sTempBuf;

    sDesc   = (smiDataPortHeaderDesc*)aDesc;
    sHeader = (smiDataPortHeader*)aHeader;

    IDE_TEST_RAISE( sHeader->mIsBigEndian >= 2,
                    ERR_ABORT_DATA_PORT_INTERNAL_ERROR );

    IDE_TEST_RAISE( ( sHeader->mCompileBit  < 32 ) ||
                    ( sHeader->mCompileBit  > 64 ),
                    ERR_ABORT_DATA_PORT_INTERNAL_ERROR );

    IDE_TEST_RAISE( sHeader->mColumnCount > SMI_COLUMN_ID_MAXIMUM,
                    ERR_ABORT_DATA_PORT_INTERNAL_ERROR );

    IDE_TEST_RAISE( sHeader->mBasicColumnCount > SMI_COLUMN_ID_MAXIMUM,
                    ERR_ABORT_DATA_PORT_INTERNAL_ERROR );

    IDE_TEST_RAISE( sHeader->mLobColumnCount > SMI_COLUMN_ID_MAXIMUM,
                    ERR_ABORT_DATA_PORT_INTERNAL_ERROR );

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_ABORT_DATA_PORT_INTERNAL_ERROR );
    {
        // Memory �Ҵ� ��, �ش� �޸𸮿� �� Header�� Dump�صӴϴ�
        // �׸��� Dump�� Header�� TRC�α׿� ����մϴ�.
        if( iduMemMgr::calloc( IDU_MEM_SM_SCP, 
                               1, 
                               IDE_DUMP_DEST_LIMIT, 
                               (void**)&sTempBuf )
            == IDE_SUCCESS )
        {
            (void) smiDataPort::dumpHeader( sDesc,
                                            aVersion,
                                            aHeader,
                                            SMI_DATAPORT_HEADER_FLAG_FULL,
                                            sTempBuf,
                                            IDE_DUMP_DEST_LIMIT );
            ideLog::log( IDE_SERVER_0, "%s", sTempBuf );
            (void)iduMemMgr::free( sTempBuf );
        }

        IDE_SET( ideSetErrorCode( 
                smERR_ABORT_DATA_PORT_INTERNAL_ERROR ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

smiDataPortHeaderDesc gSmiDataPortHeaderDesc[ SMI_DATAPORT_VERSION_COUNT ]=
{
    {
        (SChar*)"VERSION_NONE",
        0,
        NULL,
        NULL
    },
    {
        (SChar*)"COMMON_HEADER",
        ID_SIZEOF( smiDataPortHeader ),
        (smiDataPortHeaderColDesc*)gSmiDataPortHeaderColDesc,
        gSmiDataPortHeaderValidation
    }
};

IDE_RC smiDataPort::findHandle( SChar          * aName,
                                void          ** aHandle )
{
    IDE_TEST( scpManager::findHandle( aName,
                                      aHandle )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}



/***********************************************************************
 *
 * Description :
 *  Export�� ���� �غ� �Ѵ�.
 *
 *  aStatistics         - [IN]     �������
 *  aHanlde             - [IN/OUT] ������ �۾��� ���� Handle 
 *  aHeader             - [IN]     ���� Header
 *  aJobName            - [IN]     ���� �����ϴ� Job�� �̸�
 *  aObjectName         - [IN]     ���  ��ü�� �̸�
 *  aLobColumnCount     - [IN]     LobColumn�� ����
 *  aType               - [IN]     Type
 *  aSplit              - [IN]     Split���� Row ��
 *
 **********************************************************************/

IDE_RC smiDataPort::beginExport( idvSQL              * aStatistics, 
                                 void               ** aHandle,
                                 smiDataPortHeader   * aHeader,
                                 SChar               * aJobName, 
                                 SChar               * aObjectName, 
                                 SChar               * aDirectory, 
                                 UInt                  aType,
                                 SLong                 aSplit )
{
    scpHandle * sHandle; 
    scpModule * sModule;
    SChar     * sTempBuf;
    UInt        sBufState = 0;
    UInt        sState = 0;

    IDE_DASSERT( aHandle               != NULL );
    IDE_DASSERT( aHeader               != NULL );
    IDE_DASSERT( aJobName              != NULL );
    IDE_DASSERT( aObjectName           != NULL );
    IDE_DASSERT( aDirectory            != NULL );

    // -----------------------------------------
    // Select Module
    // -----------------------------------------
    switch( aType )
    {
        case SMI_DATAPORT_TYPE_FILE:
            sModule = &scpfModule;
            break;
        default:
            // File��� �ܿ��� �������� �ʴ´�.
            ideLog::log( IDE_SERVER_0, "invalid type : %u ",
                         aType );
            IDE_ASSERT( 0 );
            break;
    }
    aHeader->mVersion               = SMI_DATAPORT_VERSION_LATEST;

    // -----------------------------------------
    // begin export
    // -----------------------------------------
    IDE_TEST( sModule->mBeginExport( aStatistics,
                                     aHandle,
                                     aHeader,
                                     aObjectName,
                                     aDirectory,
                                     aSplit )  
              != IDE_SUCCESS );
    sState = 1;


    // setting common handle
    sHandle          = (scpHandle*)*aHandle;
    sHandle->mModule = sModule;
    sHandle->mType   = aType;

    idlOS::strncpy( (char*)sHandle->mJobName,
                    (char*)aJobName,
                    SMI_DATAPORT_JOBNAME_SIZE );

    IDE_TEST( scpManager::addNode( sHandle ) != IDE_SUCCESS );
    sState = 2;

    // ------------------------------------
    // Write trace log
    // -----------------------------------
    IDE_TEST( iduMemMgr::calloc( IDU_MEM_SM_SMI, 1,
                                 ID_SIZEOF( SChar ) * IDE_DUMP_DEST_LIMIT,
                                 (void**)&sTempBuf )
              != IDE_SUCCESS );
    sBufState = 1;

    idlOS::snprintf( sTempBuf,
                     IDE_DUMP_DEST_LIMIT,
                     "--------- DataPort Export Begin -------\n" );

    IDE_TEST( dumpHeader( 
            gSmiDataPortHeaderDesc,
            aHeader->mVersion,
            aHeader,
            SMI_DATAPORT_HEADER_FLAG_FULL,
            sTempBuf,
            IDE_DUMP_DEST_LIMIT )
        != IDE_SUCCESS );

    idlVA::appendFormat( sTempBuf,
                         IDE_DUMP_DEST_LIMIT,
                         "Split     : %"ID_UINT64_FMT"\n"
                         "JobName   : %s\n"
                         "ObjectName: %s\n"
                         "Directory : %s\n",
                         aSplit,
                         aJobName,
                         aObjectName,
                         aDirectory );
    ideLog::log( IDE_SM_0, "%s\n", sTempBuf );

    sBufState = 0;
    IDE_TEST( iduMemMgr::free( sTempBuf ) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if( sBufState == 1)
    {
        (void) iduMemMgr::free( sTempBuf );
    }
    switch( sState )
    {
        case 2:
            (void) scpManager::delNode( sHandle );
        case 1:
            (void) sModule->mEndExport( aStatistics,
                                        aHandle );
        default:
            break;
    }

    return IDE_FAILURE;
}

/***********************************************************************
 *
 * Description :
 *  Row�� �ϳ� Write�Ѵ�.
 *
 *  aStatistics         - [IN] �������
 *  aHanlde             - [IN] ������ �۾��� ���� Handle 
 *  aValueList          - [IN] ������ value
 *
 **********************************************************************/

IDE_RC smiDataPort::write( idvSQL      * aStatistics, 
                           void       ** aHandle,
                           smiValue    * aValueList )
{
    scpHandle   * sHandle;

    IDE_DASSERT( aHandle            != NULL );
    IDE_DASSERT( *aHandle           != NULL );
    IDE_DASSERT( aValueList         != NULL );

    sHandle = (scpHandle*)*aHandle;
    IDE_TEST( ((scpModule*)sHandle->mModule)->mWrite(
            aStatistics,
            sHandle,
            aValueList )
        != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 *
 * Description :
 *  Lob�� �� �غ� �Ѵ�.
 *
 *  aStatistics         - [IN] �������
 *  aHanlde             - [IN] ������ �۾��� ���� Handle 
 *  aLobLength          - [IN] ����� LobColumn�� ��ü Length
 *
 **********************************************************************/

IDE_RC smiDataPort::prepareLob( idvSQL      * aStatistics, 
                                void       ** aHandle,
                                UInt          aLobLength )
{
    scpHandle   * sHandle;

    IDE_DASSERT( aHandle           != NULL );
    IDE_DASSERT( *aHandle          != NULL );

    sHandle = (scpHandle*)*aHandle;
    IDE_TEST( ((scpModule*)sHandle->mModule)->mPrepareLob(
            aStatistics,
            sHandle,
            aLobLength )
        != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

/***********************************************************************
 *
 * Description :
 *  Lob�� ����Ѵ�.
 *
 *  aStatistics         - [IN] �������
 *  aHanlde             - [IN] ������ �۾��� ���� Handle 
 *  aLobPieceLength     - [IN] ����� LobPiece�� ����
 *  aLobPieceValue      - [IN] ����� LobPiece
 *
 **********************************************************************/
IDE_RC smiDataPort::writeLob( idvSQL      * aStatistics,
                              void       ** aHandle,
                              UInt          aLobPieceLength,
                              UChar       * aLobPieceValue )
{
    scpHandle   * sHandle;

    IDE_DASSERT( aHandle           != NULL );
    IDE_DASSERT( *aHandle          != NULL );
    IDE_DASSERT( aLobPieceValue    != NULL );

    sHandle = (scpHandle*)*aHandle;
    IDE_TEST( ((scpModule*)sHandle->mModule)->mWriteLob(
            aStatistics,
            sHandle,
            aLobPieceLength,
            aLobPieceValue )
        != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

/***********************************************************************
 *
 * Description :
 *  Lob�� ����� �Ϸ�Ǿ���.
 *
 *  aStatistics         - [IN] �������
 *  aHanlde             - [IN] ������ �۾��� ���� Handle 
 *
 **********************************************************************/
IDE_RC smiDataPort::finishLobWriting( idvSQL      * aStatistics,
                                      void       ** aHandle )
{
    scpHandle   * sHandle;

    IDE_DASSERT( aHandle           != NULL );
    IDE_DASSERT( *aHandle          != NULL );

    sHandle = (scpHandle*)*aHandle;
    IDE_TEST( ((scpModule*)sHandle->mModule)->mFinishLobWriting(
                  aStatistics,
                  sHandle )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

/***********************************************************************
 *
 * Description :
 *  Export�� �����Ѵ�.
 *
 *  aStatistics         - [IN] �������
 *  aHanlde             - [IN] ������ �۾��� ���� Handle 
 *
 **********************************************************************/
IDE_RC smiDataPort::endExport( idvSQL      * aStatistics,
                               void       ** aHandle )
{
    UInt        sState = 2;
    scpHandle * sHandle;

    IDE_DASSERT( aHandle            != NULL );
    IDE_DASSERT( *aHandle           != NULL );

    sHandle = (scpHandle*)*aHandle;

    ideLog::log( IDE_SM_0, "--------------- End Export(%s) ----------\n",
                 sHandle->mJobName);

    sState = 1;
    IDE_TEST( scpManager::delNode( sHandle ) != IDE_SUCCESS );

    sState = 0;
    IDE_TEST( ((scpModule*)sHandle->mModule)->mEndExport(
            aStatistics,
            aHandle )
        != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    switch( sState )
    {
        case 2:
            (void) scpManager::delNode( sHandle );
        case 1:
            (void) ((scpModule*)sHandle->mModule)->mEndExport(
                aStatistics,
                aHandle );
        default:
            break;
    }

    return IDE_FAILURE;

}


/***********************************************************************
 *
 * Description :
 *  Import�� ���� �غ� �Ѵ�.
 *
 *  aStatistics         - [IN]      �������
 *  aHanlde             - [IN/OUT]  ������ �۾��� ���� Handle 
 *  aJobName            - [IN]      ���� �����ϴ� Job�� �̸�
 *  aObjectName         - [IN]      ���  ��ü�� �̸�
 *  aType               - [IN]      aType
 *  aFirstRowSeq        - [IN]      ������ ù��° Row��ȣ
 *  aLastRowSeq         - [IN]      ������ ������ Row��ȣ
 *
 **********************************************************************/
IDE_RC smiDataPort::beginImport( idvSQL               * aStatistics, 
                                 void                ** aHandle,
                                 smiDataPortHeader    * aHeader,
                                 SChar                * aJobName, 
                                 SChar                * aObjectName, 
                                 SChar                * aDirectory, 
                                 UInt                   aType,
                                 SLong                  aFirstRowSeq,
                                 SLong                  aLastRowSeq )
{
    scpHandle * sHandle;
    scpModule * sModule;
    SChar     * sTempBuf;
    UInt        sBufState = 0;
    UInt        sState = 0;

    IDE_DASSERT( aHandle               != NULL );
    IDE_DASSERT( aHeader               != NULL );
    IDE_DASSERT( aJobName              != NULL );
    IDE_DASSERT( aObjectName           != NULL );
    IDE_DASSERT( aDirectory            != NULL );

    // -----------------------------------------
    // Select Module
    // -----------------------------------------
    switch( aType )
    {
        case SMI_DATAPORT_TYPE_FILE:
            sModule = &scpfModule;
            break;
        default:
            // File��� �ܿ��� �������� �ʴ´�.
            ideLog::log( IDE_SERVER_0, "invalid type : %u ",
                         aType );
            IDE_ASSERT( 0 );
            break;
    }

    IDE_TEST( sModule->mBeginImport( aStatistics,
                                     aHandle,
                                     aHeader,
                                     aFirstRowSeq,
                                     aLastRowSeq,
                                     aObjectName,
                                     aDirectory )
              != IDE_SUCCESS );
    sState = 1;

    // setting common handle
    sHandle          = (scpHandle*)*aHandle;
    sHandle->mModule = sModule;
    sHandle->mType   = aType;

    idlOS::strncpy( (char*)sHandle->mJobName,
                    (char*)aJobName,
                    SMI_DATAPORT_JOBNAME_SIZE );

    IDE_TEST( scpManager::addNode( sHandle ) != IDE_SUCCESS );
    sState = 2;

    // ------------------------------------
    // Write trace log
    // -----------------------------------
    IDE_TEST( iduMemMgr::calloc( IDU_MEM_SM_SMI, 1,
                                 ID_SIZEOF( SChar ) * IDE_DUMP_DEST_LIMIT,
                                 (void**)&sTempBuf )
              != IDE_SUCCESS );
    sBufState = 1;

    idlOS::snprintf( sTempBuf,
                     IDE_DUMP_DEST_LIMIT,
                     "--------- DataPort Import Begin -------\n" );
    IDE_TEST( dumpHeader( 
            gSmiDataPortHeaderDesc,
            aHeader->mVersion,
            aHeader,
            SMI_DATAPORT_HEADER_FLAG_FULL,
            sTempBuf,
            IDE_DUMP_DEST_LIMIT )
        != IDE_SUCCESS );
    idlVA::appendFormat( sTempBuf,
                         IDE_DUMP_DEST_LIMIT,
                         "FirstRow  : %"ID_UINT64_FMT"\n"
                         "LastRow   : %"ID_UINT64_FMT"\n"
                         "JobName   : %s\n"
                         "ObjectName: %s\n"
                         "Directory : %s\n",
                         aFirstRowSeq,
                         aLastRowSeq,
                         aJobName,
                         aObjectName,
                         aDirectory );
    ideLog::log( IDE_SM_0, "%s\n", sTempBuf );

    sBufState = 0;
    IDE_TEST( iduMemMgr::free( sTempBuf ) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if( sBufState == 1)
    {
        (void) iduMemMgr::free( sTempBuf );
    }

    switch( sState )
    {
        case 2:
            (void) scpManager::delNode( sHandle );
        case 1:
            (void) sModule->mEndImport( aStatistics,
                                        &sHandle );
        default:
            break;
    }

    return IDE_FAILURE;
}


/***********************************************************************
 *
 * Description :
 *  row���� �д´�.
 *
 *  aStatistics         - [IN]  �������
 *  aHanlde             - [IN]  ������ �۾��� ���� Handle 
 *  aRows               - [IN]  ���� row
 *  aRowCount           - [IN]  ���� Row�� ����
 *
 **********************************************************************/

IDE_RC smiDataPort::read( idvSQL      * aStatistics, 
                          void       ** aHandle,
                          smiRow4DP  ** aRows,
                          UInt        * aRowCount )
{
    scpHandle   * sHandle;

    IDE_DASSERT( aStatistics        != NULL );
    IDE_DASSERT( aHandle            != NULL );
    IDE_DASSERT( *aHandle           != NULL );
    IDE_DASSERT( aRows              != NULL );
    IDE_DASSERT( aRowCount          != NULL );

    sHandle = (scpHandle*)*aHandle;
    IDE_TEST( ((scpModule*)sHandle->mModule)->mRead(
            aStatistics,
            sHandle,
            aRows,
            aRowCount )
        != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 *
 * Description :
 *  �߰��� ���� LobColumn �ϳ��� �� ���̸� �����´�.
 *
 *  aStatistics         - [IN]  �������
 *  aHanlde             - [IN]  ������ �۾��� ���� Handle 
 *  aLength             - [OUT] ���� Lob�� �� ����
 *
 **********************************************************************/

IDE_RC smiDataPort::readLobLength( idvSQL      * aStatistics, 
                                   void       ** aHandle,
                                   UInt        * aLength )
{
    scpHandle   * sHandle;

    IDE_DASSERT( aStatistics        != NULL );
    IDE_DASSERT( aHandle            != NULL );
    IDE_DASSERT( *aHandle           != NULL );
    IDE_DASSERT( aLength            != NULL );

    sHandle = (scpHandle*)*aHandle;
    IDE_TEST( ((scpModule*)sHandle->mModule)->mReadLobLength(
            aStatistics,
            sHandle,
            aLength )
        != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 *
 * Description :
 *  Lob�� �д´�.
 *
 *  aStatistics         - [IN]  �������
 *  aHanlde             - [IN]  ������ �۾��� ���� Handle 
 *  aLobPieceLength     - [OUT] ���� LobPiece�� ����
 *  aLobPieceValue      - [OUT] ���� LobPiece
 *
 **********************************************************************/
IDE_RC smiDataPort::readLob( idvSQL      * aStatistics, 
                             void       ** aHandle,
                             UInt        * aLobPieceLength,
                             UChar      ** aLobPieceValue )
{
    scpHandle   * sHandle;

    IDE_DASSERT( aHandle           != NULL );
    IDE_DASSERT( *aHandle          != NULL );
    IDE_DASSERT( aLobPieceLength   != NULL );
    IDE_DASSERT( aLobPieceValue    != NULL );

    sHandle = (scpHandle*)*aHandle;
    IDE_TEST( ((scpModule*)sHandle->mModule)->mReadLob(
            aStatistics,
            sHandle,
            aLobPieceLength,
            aLobPieceValue )
        != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 *
 * Description :
 *  Lob�� �бⰡ �Ϸ�Ǿ���.
 *
 *  aStatistics         - [IN] �������
 *  aHanlde             - [IN] ������ �۾��� ���� Handle 
 *
 **********************************************************************/
IDE_RC smiDataPort::finishLobReading( idvSQL      * aStatistics,
                                      void       ** aHandle )
{
    scpHandle   * sHandle;

    IDE_DASSERT( aHandle           != NULL );
    IDE_DASSERT( *aHandle          != NULL );

    sHandle = (scpHandle*)*aHandle;
    IDE_TEST( ((scpModule*)sHandle->mModule)->mFinishLobReading(
                  aStatistics,
                  sHandle )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}


/***********************************************************************
 *
 * Description :
 *  import ������ �����մϴ�.
 *
 *  aStatistics         - [IN]  �������
 *  aHanlde             - [IN]  ������ �۾��� ���� Handle 
 *
 **********************************************************************/
IDE_RC smiDataPort::endImport( idvSQL      * aStatistics,
                               void       ** aHandle )
{
    UInt        sState = 2;
    scpHandle * sHandle;

    IDE_DASSERT( aHandle            != NULL );
    IDE_DASSERT( *aHandle           != NULL );

    sHandle = (scpHandle*)*aHandle;
    ideLog::log( IDE_SM_0, "--------------- End Import(%s) ----------\n",
                 sHandle->mJobName);

    sState = 1;
    IDE_TEST( scpManager::delNode( sHandle ) != IDE_SUCCESS );

    sState = 0;
    IDE_TEST( ((scpModule*)sHandle->mModule)->mEndImport( 
            aStatistics,
            aHandle )
        != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    switch( sState )
    {
        case 2:
            (void) scpManager::delNode( sHandle );
        case 1:
            (void) ((scpModule*)sHandle->mModule)->mEndImport( 
                aStatistics,
                aHandle );
        default:
            break;
    }

    return IDE_FAILURE;

}

/****************************************************************
 * Proj-2059 DB Upgrade
 *
 * Structure�� �ִ� Member Variable�� Endian ��� ����
 * �а� �� �� �ֵ���, Format�� ������ DATA_PORT_HEADER�� ���� �Լ�
 ****************************************************************/

/* ���� ����ü�� ũ�⸦ ��ȯ�Ѵ�. */
UInt smiDataPort::getHeaderSize( smiDataPortHeaderDesc  * aDesc,
                                 UInt                     aVersion)
{
    return aDesc[ aVersion ].mSize;
}

/* Encoding�� �������� ũ�⸦ ���Ѵ�. */
UInt smiDataPort::getEncodedHeaderSize( smiDataPortHeaderDesc   * aDesc,
                                        UInt                      aVersion)
{
    smiDataPortHeaderColDesc  * sColDesc;
    UInt                        sTotalSize = 0;
    UInt                        i;

    IDE_DASSERT( aDesc         != NULL );

    // �ش� �������� �߰��� �׸��� ũ�⸦ ����
    for( i=SMI_DATAPORT_VERSION_BEGIN; i<=aVersion; i++ )
    {
        sColDesc = aDesc[i].mColumnDesc;
        while( sColDesc->mName != NULL )
        {
            sTotalSize += sColDesc->mSize;
            sColDesc++;
        }
    }

    // Checksum�� �ڿ� �ٴ´�
    sTotalSize += ID_SIZEOF( UInt );

    return sTotalSize;
}


/* Desc�� Min/Max������ �������� Header�� ���� Validation�� �����Ѵ�. */
IDE_RC smiDataPort::validateHeader( smiDataPortHeaderDesc   * aDesc,
                                    UInt                      aVersion,
                                    void                    * aHeader )
{
    IDE_DASSERT( aDesc         != NULL );
    IDE_DASSERT( aHeader       != NULL );

    IDE_TEST( aDesc[aVersion].mValidateFunc( &aDesc[aVersion],
                                             aVersion, 
                                             aHeader ) );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}


/* Desc�� ���� Header�� write�Ѵ�. */
IDE_RC smiDataPort::writeHeader( smiDataPortHeaderDesc   * aDesc,
                                 UInt                      aVersion,
                                 void                    * aHeader,
                                 UChar                   * aDestBuffer,
                                 UInt                    * aOffset,
                                 UInt                      aDestBufferSize )
{
    smiDataPortHeaderColDesc * sColDesc;
    UInt                       i;
    UInt                       sCalculatedChecksum;
    UInt                       sBeginOffset;

    IDE_DASSERT( aDesc         != NULL );
    IDE_DASSERT( aHeader       != NULL );
    IDE_DASSERT( aDestBuffer   != NULL );
    IDE_DASSERT( aOffset       != NULL );

    sBeginOffset = *aOffset;

    // �ش� �������� �߰��� �׸���� ������
    for( i=SMI_DATAPORT_VERSION_BEGIN; i<=aVersion; i++ )
    {
        sColDesc = aDesc[i].mColumnDesc;

        while( sColDesc->mName != NULL )
        {
            IDE_TEST( aDestBufferSize < ( (*aOffset) + sColDesc->mSize ) );

            switch( sColDesc->mType )
            {
                case SMI_DATAPORT_HEADER_TYPE_CHAR:
                    idlOS::memcpy( aDestBuffer + (*aOffset),
                                   ((UChar *)aHeader) + sColDesc->mOffset,
                                   sColDesc->mSize );
                    break;
                case SMI_DATAPORT_HEADER_TYPE_LONG:
                    IDE_DASSERT( sColDesc->mSize == ID_SIZEOF( ULong ) );

                    SMI_WRITE_ULONG( ((UChar *)aHeader) + sColDesc->mOffset,
                                     aDestBuffer + (*aOffset) );
                    break;
                case SMI_DATAPORT_HEADER_TYPE_INTEGER:
                    IDE_DASSERT( sColDesc->mSize == ID_SIZEOF( UInt ) );

                    SMI_WRITE_UINT( ((UChar *)aHeader) + sColDesc->mOffset,
                                    aDestBuffer + (*aOffset) );
                    break;
                default:
                    break;
            }

            (*aOffset) += sColDesc->mSize;
            sColDesc++;
        }
    }

    // Checksum ���
    sCalculatedChecksum = smuUtility::foldBinary( aDestBuffer + sBeginOffset,
                                                  (*aOffset) - sBeginOffset );

    SMI_WRITE_UINT( &sCalculatedChecksum, aDestBuffer + (*aOffset) );
    (*aOffset) += ID_SIZEOF( UInt );

    IDE_TEST( smiDataPort::validateHeader( aDesc, aVersion, aHeader )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}


/* Desc�� ���� Header�� Read�Ѵ�. */
IDE_RC smiDataPort::readHeader( smiDataPortHeaderDesc   * aDesc,
                                UInt                      aVersion,
                                UChar                   * aSourceBuffer,
                                UInt                    * aOffset,
                                UInt                      aSourceBufferSize,
                                void                    * aDestHeader )
{
    smiDataPortHeaderColDesc  * sColDesc;
    ULong                       sDefaultLong;
    UInt                        sDefaultInt;
    UInt                        i;
    UInt                        sCalculatedChecksum;
    UInt                        sReadChecksum;
    UInt                        sBeginOffset;

    IDE_DASSERT( aDesc         != NULL );
    IDE_DASSERT( aSourceBuffer != NULL );
    IDE_DASSERT( aOffset       != NULL );
    IDE_DASSERT( aDestHeader   != NULL );

    ACP_UNUSED( aSourceBufferSize );
    
    sBeginOffset = *aOffset;

    // �ش� ���������� �׸���� ����
    for( i=SMI_DATAPORT_VERSION_BEGIN; i<=aVersion; i++ )
    {
        sColDesc = aDesc[i].mColumnDesc;

        while( sColDesc->mName != NULL )
        {
            //  ���� ����
            IDE_DASSERT( ((*aOffset) + sColDesc->mSize) <= aSourceBufferSize );

            switch( sColDesc->mType )
            {
                case SMI_DATAPORT_HEADER_TYPE_CHAR:
                    idlOS::memcpy( ((UChar *)aDestHeader)
                                       + sColDesc->mOffset,
                                   aSourceBuffer + (*aOffset),
                                   sColDesc->mSize );
                    break;
                case SMI_DATAPORT_HEADER_TYPE_LONG:
                    IDE_DASSERT( sColDesc->mSize == ID_SIZEOF( ULong ) );

                    SMI_READ_ULONG( aSourceBuffer + (*aOffset),
                                    ((UChar *)aDestHeader) 
                                        + sColDesc->mOffset );
                    break;
                case SMI_DATAPORT_HEADER_TYPE_INTEGER:
                    IDE_DASSERT( sColDesc->mSize == ID_SIZEOF( UInt ) );

                    SMI_READ_UINT( aSourceBuffer + (*aOffset),
                                   ((UChar *)aDestHeader) 
                                        + sColDesc->mOffset );
                    break;
                default:
                    break;
            }

            (*aOffset) += sColDesc->mSize;
            sColDesc++;
        }
    }

    // Checksum Ȯ��
    sCalculatedChecksum = smuUtility::foldBinary( aSourceBuffer + sBeginOffset,
                                                  (*aOffset) - sBeginOffset );

    SMI_READ_UINT( aSourceBuffer + (*aOffset), &sReadChecksum );
    (*aOffset) += ID_SIZEOF( UInt );

    IDE_TEST_RAISE( sCalculatedChecksum != sReadChecksum,
                    ERR_ABORT_CORRUPTED_HEADER );


    // �ֽ� ������ �׸���� Default������ ������
    for( i=aVersion+1; i<=SMI_DATAPORT_VERSION_LATEST; i++ )
    {
        sColDesc = aDesc[i].mColumnDesc;

        if( sColDesc == NULL )
        {
            continue;
        }

        while( sColDesc->mName != NULL )
        {
            // SMI_READ_XXX �Լ��� �ƴ� memcpy�� ����մϴ�.
            // Memory�󿡼� �ٷ� �����ϱ� �����Դϴ�. 
            switch( sColDesc->mType )
            {
                case SMI_DATAPORT_HEADER_TYPE_CHAR:
                    idlOS::memcpy( ((UChar *)aDestHeader)
                                       + sColDesc->mOffset,
                                   sColDesc->mDefaultStr,
                                   sColDesc->mSize );
                    break;
                case SMI_DATAPORT_HEADER_TYPE_LONG:
                    IDE_DASSERT( sColDesc->mSize == ID_SIZEOF( ULong ) );

                    sDefaultLong = sColDesc->mDefaultNum;
                    idlOS::memcpy( ((UChar *)aDestHeader) 
                                       + sColDesc->mOffset,
                                   &sDefaultLong,
                                   ID_SIZEOF( ULong ) );
                    break;
                case SMI_DATAPORT_HEADER_TYPE_INTEGER:
                    IDE_DASSERT( sColDesc->mSize == ID_SIZEOF( UInt ) );

                    sDefaultInt = sColDesc->mDefaultNum;
                    idlOS::memcpy( ((UChar *)aDestHeader) 
                                       + sColDesc->mOffset,
                                   &sDefaultInt,
                                   ID_SIZEOF( UInt ) );
                    break;
                default:
                    break;
            }
            sColDesc++;
        }
    }


    IDE_TEST( smiDataPort::validateHeader( aDesc, aVersion, aDestHeader )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_ABORT_CORRUPTED_HEADER );
    {
        IDE_SET( ideSetErrorCode( smERR_ABORT_CORRUPTED_HEADER ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/* Header�� ���� Dump�Ѵ�. */
IDE_RC smiDataPort::dumpHeader( smiDataPortHeaderDesc   * aDesc,
                                UInt                      aVersion,
                                void                    * aSourceHeader,
                                UInt                      aFlag,
                                SChar                   * aOutBuf ,
                                UInt                      aOutSize )
{
    smiDataPortHeaderColDesc  * sColDesc;
    UInt                        sTempInt;
    ULong                       sTempLong;
    UInt                        i;

    IDE_ERROR( aDesc         != NULL );
    IDE_ERROR( aSourceHeader != NULL );
    IDE_ERROR( aOutBuf       != NULL );
    IDE_ERROR( aOutSize       > 0 );


    // �ش� �������� �׸���� dump
    for( i=SMI_DATAPORT_VERSION_BEGIN; i<=aVersion; i++ )
    {
        sColDesc = aDesc[i].mColumnDesc;

        if( (aFlag & SMI_DATAPORT_HEADER_FLAG_DESCNAME_MASK) ==
            SMI_DATAPORT_HEADER_FLAG_DESCNAME_YES )
        {
            idlVA::appendFormat( aOutBuf,
                                 aOutSize,
                                 "\n[%s]\n",
                                 aDesc[i].mName );
        }

        while( sColDesc->mName != NULL )
        {
            if( (aFlag & SMI_DATAPORT_HEADER_FLAG_COLNAME_MASK) ==
                SMI_DATAPORT_HEADER_FLAG_COLNAME_YES )
            {
                idlVA::appendFormat( aOutBuf,
                                     aOutSize,
                                     "%-24s : ",
                                     sColDesc->mName );
            }

            if( (aFlag & SMI_DATAPORT_HEADER_FLAG_TYPE_MASK) ==
                SMI_DATAPORT_HEADER_FLAG_TYPE_YES )
            {
                switch( sColDesc->mType )
                {
                    case SMI_DATAPORT_HEADER_TYPE_CHAR:
                        idlVA::appendFormat( aOutBuf, aOutSize, "CHAR " );
                        break;
                    case SMI_DATAPORT_HEADER_TYPE_LONG:
                        idlVA::appendFormat( aOutBuf, aOutSize, "LONG " );
                        break;
                    case SMI_DATAPORT_HEADER_TYPE_INTEGER:
                        idlVA::appendFormat( aOutBuf, aOutSize, "INT  " );
                        break;
                }
            }

            switch( sColDesc->mType )
            {
                case SMI_DATAPORT_HEADER_TYPE_CHAR:
                    idlOS::strncat( aOutBuf,
                                    ((SChar *)aSourceHeader) + sColDesc->mOffset,
                                    aOutSize );
                    break;

                case SMI_DATAPORT_HEADER_TYPE_LONG:
                    idlOS::memcpy( &sTempLong,
                                   ((UChar *)aSourceHeader) + sColDesc->mOffset,
                                   sColDesc->mSize );
                    idlVA::appendFormat( 
                        aOutBuf, aOutSize, "%-8"ID_UINT64_FMT, sTempLong );
                    break;

                case SMI_DATAPORT_HEADER_TYPE_INTEGER:
                    idlOS::memcpy( &sTempInt,
                                   ((UChar *)aSourceHeader) + sColDesc->mOffset,
                                   sColDesc->mSize );

                    idlVA::appendFormat( 
                        aOutBuf, aOutSize, "%-8"ID_UINT32_FMT, sTempInt );
                    break;
                default:
                    break;
            }

            if( (aFlag & SMI_DATAPORT_HEADER_FLAG_SIZE_MASK) ==
                SMI_DATAPORT_HEADER_FLAG_SIZE_YES )
            {
                idlVA::appendFormat( 
                    aOutBuf, 
                    aOutSize, 
                    "(Size:%"ID_UINT32_FMT")",
                    sColDesc->mSize );
            }

            idlVA::appendFormat( aOutBuf, aOutSize, "\n" );
            sColDesc++;
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}



IDE_RC smiDataPort::dumpFileHeader( SChar  * aFileName,
                                    SChar  * aDirectory,
                                    idBool   aDetail )
{
    idvSQL                sDummyStat;
    void                * sHandle;
    smiDataPortHeader     sHeader;

    IDE_TEST( scpfModule.mBeginImport( &sDummyStat,
                                       &sHandle,
                                       &sHeader,
                                       0,
                                       SMI_DATAPORT_NULL_ROWSEQ,
                                       aFileName,
                                       aDirectory )
              != IDE_SUCCESS );

    IDE_TEST( scpfDumpHeader( (scpfHandle*)sHandle,
                              aDetail )
              != IDE_SUCCESS );

    IDE_TEST( scpfModule.mEndImport( &sDummyStat,
                                     &sHandle )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC smiDataPort::dumpFileBlocks( SChar    * aFileName,
                                    SChar    * aDirectory,
                                    idBool     aHexa,
                                    SLong      aFirstBlock,
                                    SLong      aLastBlock )
{
    idvSQL                sDummyStat;
    void                * sHandle;
    smiDataPortHeader     sHeader;

    IDE_TEST( scpfModule.mBeginImport( &sDummyStat,
                                       &sHandle,
                                       &sHeader,
                                       0,
                                       SMI_DATAPORT_NULL_ROWSEQ,
                                       aFileName,
                                       aDirectory )
              != IDE_SUCCESS );

    IDE_TEST( scpfDumpBlocks( &sDummyStat,
                              (scpfHandle*)sHandle,
                              aFirstBlock,
                              aLastBlock,
                              aHexa )
              != IDE_SUCCESS );

    IDE_TEST( scpfModule.mEndImport( &sDummyStat,
                                     &sHandle )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

IDE_RC smiDataPort::dumpFileRows( SChar    * aFileName,
                                  SChar    * aDirectory,
                                  SLong      aFirst,
                                  SLong      aLast )
{
    idvSQL                sDummyStat;
    void                * sHandle;
    smiDataPortHeader     sHeader;

    IDE_TEST( scpfModule.mBeginImport( &sDummyStat,
                                       &sHandle,
                                       &sHeader,
                                       aFirst,
                                       aLast,
                                       aFileName,
                                       aDirectory )
              != IDE_SUCCESS );

    IDE_TEST( scpfDumpRows( &sDummyStat,
                            (scpfHandle*)sHandle )
              != IDE_SUCCESS );

    IDE_TEST( scpfModule.mEndImport( &sDummyStat,
                                     &sHandle )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}


