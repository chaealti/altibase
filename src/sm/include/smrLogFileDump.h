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
 * $Id: smrLogFileDump.h 22903 2007-08-08 01:53:38Z kmkim $
 **********************************************************************/

#ifndef _O_SMR_LOG_FILE_DUMP_H_
#define _O_SMR_LOG_FILE_DUMP_H_ 1

#include <ida.h>
#include <idu.h>
#include <smDef.h>

#define SMR_XID_DATA_MAX_LEN (256)

/*
    �ϳ��� �α����Ͼ��� �α׷��ڵ���� ����Ѵ�.
 */
class smrLogFileDump
{
public :
    /* Static ���� �� �Լ��� 
     *********************************************************************/
    static IDE_RC initializeStatic();
    static IDE_RC destroyStatic();
    
    /* TASK-4007 [SM] PBT�� ���� ��� �߰�
     * ������ ����Ǿ� �ִ� LogType, Operation Type ��
     * �α׿� ���õ� String ������ �����´�. */
    static SChar *getLogType( smrLogType aIdx );
    static SChar *getOPType( UInt aIdx );
    static SChar *getLOBOPType( UInt aIdx );
    static SChar *getDiskOPType( UInt aIdx );
    static SChar *getDiskLogType( UInt aIdx );
    static SChar *getUpdateType( smrUpdateType aIdx );
    static SChar *getTBSUptType( UInt aIdx );

    static IDE_RC dumpPrepareReqBranchTx( SChar * aBranchTxStr, UInt aSize );

    /* Instance �Լ���
     *********************************************************************/
    IDE_RC initFile();
    IDE_RC destroyFile();
    IDE_RC allocBuffer();
    IDE_RC freeBuffer();

    IDE_RC openFile( SChar * aStrLogFile );
    IDE_RC closeFile();
    IDE_RC dumpLog( idBool * aEOF );

    ULong        getFileSize()     { return mFileSize;}
    UInt         getFileNo()       { return mFileNo;}
    smrLogHead * getLogHead()      { return &mLogHead;}
    SChar      * getLogPtr()       { return mLogPtr;}
    UInt         getOffset()       { return mOffset;}
    UInt         getNextOffset()   { return mNextOffset;}
    idBool       getIsCompressed() { return mIsCompressed;}
    
private:
    /* Dump�ϴµ� �ʿ��� member ���� */
    iduFile                 mLogFile;
    SChar                 * mFileBuffer;
    ULong                   mFileSize;
    iduReusedMemoryHandle   mDecompBuffer;

    UInt                    mFileNo;
    smrLogHead              mLogHead;
    SChar                 * mLogPtr;
    UInt                    mOffset;
    UInt                    mNextOffset;
    idBool                  mIsCompressed;

    /* �α׸޸��� Ư�� Offset���� �α� ���ڵ带 �о�´�.
     * ����� �α��� ���, �α� ���������� �����Ѵ�. */
    static IDE_RC readLog( iduMemoryHandle    * aDecompBufferHandle,
                           SChar              * aFileBuffer,
                           UInt                 aFileNo,
                           UInt                 aLogOffset,
                           smrLogHead         * aRawLogHead,
                           SChar             ** aRawLogPtr,
                           UInt               * aLogSizeAtDisk,
                           idBool             * aIsCompressed);


    // Log File Header�κ��� File Begin�α׸� ����
    static IDE_RC getFileNo( SChar * aFileBeginLog,
                             UInt  * aFileNo );
    
    // BUG-28581 dumplf���� Log ���� ����� ���� sizeof ������ �߸��Ǿ� �ֽ��ϴ�.
    // smrLogType�� UChar�̱� ������ UChar MAX��ŭ Array�� �����ؾ� �մϴ�.
    static SChar mStrLogType[ ID_UCHAR_MAX ][100];
    static SChar mStrOPType[SMR_OP_MAX+1][100];
    static SChar mStrLOBOPType[SMR_LOB_OP_MAX+1][100];
    static SChar mStrDiskOPType[SDR_OP_MAX+1][100];
    static SChar mStrDiskLogType[SDR_LOG_TYPE_MAX+1][128];
    static SChar mStrUpdateType[SM_MAX_RECFUNCMAP_SIZE][100];
    static SChar mStrTBSUptType[SCT_UPDATE_MAXMAX_TYPE+1][64];
};

inline SChar *smrLogFileDump::getLogType( smrLogType aIdx )
{
    //smrLogType�� UChar�� ������ 256�� ������, mStrLogType�� 1Byte��
    //���� �Ǿ� �ֱ⿡, Overflow ������ ����.

    return mStrLogType[ aIdx ];
}

inline SChar *smrLogFileDump::getOPType( UInt aIdx )
{
    if( SMR_OP_MAX < aIdx )
    {
        aIdx = SMR_OP_MAX;
    }
    else
    {
        /* nothing to do ... */
    }

    return mStrOPType[ aIdx ];
}

inline SChar *smrLogFileDump::getLOBOPType( UInt aIdx )
{
    if( SMR_LOB_OP_MAX < aIdx )
    {
        aIdx = SMR_LOB_OP_MAX ;
    }
    else
    {
        /* nothing to do ... */
    }

    return mStrLOBOPType[ aIdx ];
}

inline SChar *smrLogFileDump::getDiskOPType( UInt aIdx )
{
    if( SDR_OP_MAX < aIdx )
    {
        aIdx = SDR_OP_MAX;
    }
    else
    {
        /* nothing to do ... */
    }

    return mStrDiskOPType[ aIdx ];
}

inline SChar *smrLogFileDump::getDiskLogType( UInt aIdx )
{
    if( SDR_LOG_TYPE_MAX < aIdx )
    {
        aIdx = SDR_LOG_TYPE_MAX;
    }
    else
    {
        /* nothing to do ... */
    }

    return mStrDiskLogType[ aIdx ];
}

inline SChar *smrLogFileDump::getUpdateType( smrUpdateType aIdx )
{
    if( SM_MAX_RECFUNCMAP_SIZE <= aIdx )
    {
        aIdx = SM_MAX_RECFUNCMAP_SIZE - 1;
    }
    else
    {
        /* nothing to do ... */
    }

    return mStrUpdateType[ aIdx ];
}

inline SChar *smrLogFileDump::getTBSUptType( UInt aIdx )
{
    if( SCT_UPDATE_MAXMAX_TYPE < aIdx )
    {
        aIdx = SCT_UPDATE_MAXMAX_TYPE;
    }
    return mStrTBSUptType[ aIdx ];
}

#endif /* _O_SMR_LOG_FILE_DUMP_H_ */
