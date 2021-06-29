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
 * $Id: smiDataPort.h 
 * 
 * Proj-2059 DB Upgrade ���
 * Server �߽������� �����͸� �������� �ִ� ���

 **********************************************************************/

#ifndef __O_SMI_DATAPORT_H__
#define __O_SMI_DATAPORT_H__ 1_

/* - Version Up�� ���� Guide.
 *
 *  ���⼭ ���ϴ� Version UP�� ������ ������ ����Ǵ� ��� �Դϴ�.
 * ���� ������ ������ ����� �ʿ䰡 ���� ���, ����� ������ ó��
 * ����� �޶����� ���� ��� �����ϴ�.
 *
 *  ���� ������ �߰��� ���ο� Attribute�� �߰����� ����մϴ�. ����
 * �׸��� ������ ���ġ �ʽ��ϴ�. ����ȣȯ���� �����Դϴ�.
 *
 *  ���� ����� ���� ������ DBMS�� ���� ������ ������ ���� �� �ֵ���
 * ����Ǿ� �ֽ��ϴ�. �ݴ�� ���� �ʽ��ϴ�.
 *
 *
 *
 *  Version���� ���� ������ ������ �����ϴ�.
 *
 *  1. �ֽ� Version Number �ø�
 *    smiDef.h�� ���ǵ� SMI_DATAPORT_VERSION_LATEST�� ��½�ŵ�ϴ�.
 * 
 * ex)   
 * #define SMI_DATAPORT_VERSION_2        (2)
 * 
 * #define SMI_DATAPORT_VERSION_BEGIN    (1)
 * #define SMI_DATAPORT_VERSION_LATEST   (SMI_DATAPORT_VERSION_2)
 * 
 * 
 * 
 * 
 * 
 *  2. ���� ����� ������
 *     ���� ������� ������ �����ϴ�.
 *
 *     ������� - gSmiDataPortHeaderDesc : smiDataPort.cpp 
 *     ������� - gSCPfFileHeaderDesc : scpfModule.cpp        
 *     ���̺���� - gQsfTableHeaderDesc : qsfDataPort.cpp 
 *     Į����� - gQsfColumnHeaderDesc : qsfDataPort.cpp 
 *     ��Ƽ����� - gQsfPartitionHeaderDesc : qsfDataPort.cpp 
 *
 *     ����� - gSCPfBlockHeaderDesc : scpfModule.cpp
 *
 *
 *
 *  1) �ش� ����� �� �׸��� �߰�
 *       HeaderDesc�� �� ������ �߰��ϰ�, ColumnDesc�� �߰��Ͽ� ����
 *     �մϴ�.
 *         
 *     ex)   
 *     smiDataPortHeaderColDesc  gSmiDataPortHeaderColDescV2[]=                  
 *     {                                                                           
 *         {                                                                       
 *             (SChar*)"VERSION_UP_TEST",                                          
 *             SMI_DATAPORT_HEADER_OFFSETOF( smiDataPortHeader , mTestValue ),   
 *             SMI_DATAPORT_HEADER_SIZEOF( smiDataPortHeader , mTestValue ),     
 *             1, NULL,   // Default                                               
 *             SMI_DATAPORT_HEADER_TYPE_INTEGER                                   
 *         },                                                                      
 *         {                                                                       
 *             NULL,                                                               
 *             0,                                                                  
 *             0,                                                                  
 *             0,0,                                                                
 *             0                                                                   
 *         }                                                                       
 *     };                                                                          
 *         
 *    smiDataPortHeaderDesc gSmiDataPortHeaderDesc[ SMI_DATAPORT_VERSION_COUNT ]=
 *    ...
 *        {                                                                 
 *            (SChar*)"COMMON_HEADER_V2",                                   
 *            ID_SIZEOF( smiDataPortHeader ),                              
 *            (smiDataPortHeaderColDesc*)gSmiDataPortHeaderColDescV2,     
 *            gSmiDataPortHeaderValidation                                 
 *        }                                                                 
 *    };
 *
 *
 *  2) Default�� �� Module ����
 *    �߰��� �׸��� ���� �������� ����ġ �ʴ� ���Դϴ�. ���� ���� ������ ����
 *  Default���� �����մϴ�. 
 *
 *    ���� Default�������ε� ���� ������ �б� ����ϴٸ� ���������, �ٸ� ó����
 * �ʿ��� ��� Default�� Ư���� ������ ���� �� Module���� ó������� �մϴ�.
 *
 *
 *
 *  3) �� �׸� �߰��� ���� ����� ���
 *     Header Desc�� Null�� �����ϸ� ����մϴ�.
 *    
 *  {
 *      (SChar*)"BLOCK_HEADER_V2",
 *      ID_SIZEOF( scpfBlockInfo),
 *    <<NULL,>>
 *      gSCPfBlockHeaderValidation
 *  }
 *
 */


#include <smiDef.h>
#include <smuList.h>

class smiDataPort
{
public:
    /************************************************************
     * Common
     ************************************************************/
    static IDE_RC findHandle( SChar          * aName,
                              void          ** aHandle );

    /************************************************************
     * Export
     ************************************************************/
    //export�� �����Ѵ�.
    static IDE_RC beginExport( idvSQL               * aStatistics, 
                               void                ** aHandle,
                               smiDataPortHeader    * aHeader,
                               SChar                * aJobName, 
                               SChar                * aObjectName, 
                               SChar                * aDirectory, 
                               UInt                   aType,
                               SLong                  aSplit );

    //Row�� �ϳ� Write�Ѵ�.
    static IDE_RC write( idvSQL          * aStatistics, 
                         void           ** aHandle,
                         smiValue        * aValueList );

    // Lob�� �� �غ� �Ѵ�.
    static IDE_RC prepareLob( idvSQL          * aStatistics, 
                              void           ** aHandle,
                              UInt              aLobLength );

    // Lob�� ����Ѵ�.
    static IDE_RC writeLob( idvSQL          * aStatistics,
                            void           ** aHandle,
                            UInt              aLobPieceLength,
                            UChar           * aLobPieceValue );

    // Lob�� ����� �Ϸ�Ǿ���.
    static IDE_RC finishLobWriting( idvSQL          * aStatistics,
                                    void           ** aHandle );

    // Export�� �����Ѵ�.
    static IDE_RC endExport( idvSQL          * aStatistics,
                             void           ** aHandle );



    /************************************************************
     * Import
     ************************************************************/
    //import�� �����Ѵ�. ����� �д´�.
    static IDE_RC beginImport( idvSQL               * aStatistics, 
                               void                ** aHandle,
                               smiDataPortHeader    * aHeader,
                               SChar                * aJobName, 
                               SChar                * aObjectName, 
                               SChar                * aDirectory, 
                               UInt                   aType,
                               SLong                  aFirstRowSeq,
                               SLong                  aLastRowSeq );

    //row���� �д´�.
    static IDE_RC read( idvSQL          * aStatistics, 
                        void           ** aHandle,
                        smiRow4DP      ** aRows,
                        UInt            * aRowCount );

    //Lob�� �� ���̸� ��ȯ�Ѵ�.
    static IDE_RC readLobLength( idvSQL          * aStatistics, 
                                 void           ** aHandle,
                                 UInt            * aLength );

    //Lob�� �д´�.
    static IDE_RC readLob( idvSQL          * aStatistics, 
                           void           ** aHandle,
                           UInt            * aLobPieceLength,
                           UChar          ** aLobPieceValue );

    // Lob �бⰡ  �Ϸ�Ǿ���.
    static IDE_RC finishLobReading( idvSQL          * aStatistics,
                                    void           ** aHandle );

    //import�� �����Ѵ�.
    static IDE_RC endImport( idvSQL          * aStatistics,
                             void           ** aHandle );


    /****************************************************************
     * HeaderDescriptor
     *
     * Structure�� �ִ� Member Variable�� Endian ��� ����
     * �а� �� �� �ֵ���, Format�� ������ Header ��Ͽ� �Լ�
     ****************************************************************/
    /* ���� Header�� ũ�⸦ ��ȯ�Ѵ�. */
    static UInt getHeaderSize( smiDataPortHeaderDesc   * aDesc,
                               UInt                      aVersion);

    /* Encoding�� Header�� ũ�⸦ ���Ѵ�. */
    static UInt getEncodedHeaderSize( smiDataPortHeaderDesc   * aDesc,
                                      UInt                      aVersion);


    /* Desc�� Min/Max������ �������� Validation�� �����Ѵ�. */
    static IDE_RC validateHeader( smiDataPortHeaderDesc   * aDesc,
                                  UInt                      aVersion,
                                  void                    * aData );

    /* Desc�� ���� write�Ѵ�. */
    static IDE_RC writeHeader( smiDataPortHeaderDesc   * aDesc,
                               UInt                      aVersion,
                               void                    * aData,
                               UChar                   * aDestBuffer,
                               UInt                    * aOffset,
                               UInt                      aDestBufferSize );

    /* Desc�� ���� Read�Ѵ�. */
    static IDE_RC readHeader( smiDataPortHeaderDesc   * aDesc,
                              UInt                      aVersion,
                              UChar                   * aSourceBuffer,
                              UInt                    * aOffset,
                              UInt                      aSourceBufferSize,
                              void                    * aDestData );

    /* Header�� Dump�Ѵ�. */
    static IDE_RC dumpHeader( smiDataPortHeaderDesc   * aDesc,
                              UInt                      aVersion,
                              void                    * aSourceData,
                              UInt                      aFlag,
                              SChar                   * aOutBuf ,
                              UInt                      aOutSize );

    /**************************************************************
     * DataPort File�� ���/��/Row�� ����Ѵ�.
     **************************************************************/
    static IDE_RC dumpFileHeader( SChar * aFileName,
                                  SChar * aDirectory,
                                  idBool  aDetail );

    static IDE_RC dumpFileBlocks( SChar    * aFileName,
                                  SChar    * aDirectory,
                                  idBool     aHexa,
                                  SLong      aFirstBlock,
                                  SLong      aLastBlock );

    static IDE_RC dumpFileRows( SChar    * aFileName,
                                SChar    * aDirectory,
                                SLong      aFirst,
                                SLong      aLast );


};

#endif /* __O_SMI_DATAPORT_H__ */

