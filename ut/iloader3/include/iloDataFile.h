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
 * $Id: iloDataFile.h 80545 2017-07-19 08:05:23Z daramix $
 **********************************************************************/

#ifndef _O_ILO_DATAFILE_H
#define _O_ILO_DATAFILE_H

/* ��ū�� �ִ� ���� = VARBIT ���ڿ� ǥ���� �ִ� ���� */
#define MAX_TOKEN_VALUE_LEN 131070
#define MAX_SEPERATOR_LEN   11
// BUG-21837
/* LOB_PIECE_SIZE : �������� LOB ����Ÿ�� �ɰ��� ������ ���� */
#define ILO_LOB_PIECE_SIZE  (32000)

/**
 * class iloLOB.
 *
 * LOB locator�� ����� ������ �а� ���⸦ ���ϰ� �ϱ� ����
 * wrapper class�̴�.
 */
class iloLOB
{
public:
    enum LOBAccessMode
    {
        LOBAccessMode_RDONLY = 0,
        LOBAccessMode_WRONLY = 1
    };

    iloLOB();

    ~iloLOB();
    
    //PROJ-1714 Parallel iLoader
    // Parallel ����� �����ϱ� ���ؼ� Data Upload �ÿ���, Upload Connection�� �������� �����ǰ�,
    // Data Download�ÿ��� Download File�� ������ �����ȴ�.
    // ����, �� ��ɿ��� ���Ǵ� Open, Close ���� �Լ��� �����Ͽ���.
    // iloDataFile�� Member �Լ� ���� ���� ������ �Լ��� �����Ͽ���.

    IDE_RC OpenForUp( ALTIBASE_ILOADER_HANDLE  aHandle,
                      SQLSMALLINT              aLOBLocCType,
                      SQLUBIGINT               aLOBLoc,
                      SQLSMALLINT              aValCType,
                      LOBAccessMode            aLOBAccessMode, 
                      iloSQLApi               *aISPApi);        //PROJ-1714

    IDE_RC CloseForUp( ALTIBASE_ILOADER_HANDLE aHandle, iloSQLApi *aISPApi );
    
    IDE_RC OpenForDown( ALTIBASE_ILOADER_HANDLE aHandle,
                        SQLSMALLINT             aLOBLocCType, 
                        SQLUBIGINT              aLOBLoc,
                        SQLSMALLINT             aValCType,
                        LOBAccessMode           aLOBAccessMode );

    IDE_RC CloseForDown( ALTIBASE_ILOADER_HANDLE aHandle );

    UInt GetLOBLength() { return mLOBLen; }

    // BUG-31004
    SQLUBIGINT GetLOBLoc() { return mLOBLoc; }

    IDE_RC Fetch( ALTIBASE_ILOADER_HANDLE   aHandle,
                  void                    **aVal,
                  UInt                     *aStrLen);

    iloBool IsFetchError() { return mIsFetchError; }

    IDE_RC GetBuffer(void **aVal, UInt *aBufLen);

    //For Upload
    IDE_RC Append( ALTIBASE_ILOADER_HANDLE  aHandle,
                   UInt                     aStrLen, 
                   iloSQLApi               *aISPApi);

    /* BUG-21064 : CLOB type CSV up/download error */
    // 32000 byte �̻��� CLOB data�� ��� �ʿ��� ������.
    // data ó�� �� �κ��̳�?
    iloBool mIsBeginCLOBAppend;
    // CLOB data�� CSV �����̳�?
    iloBool mIsCSVCLOBAppend;
    // ���� �� ó���� enclosing���ڰ� ����Ǿ�� �ϴ��� ����.
    iloBool mSaveBeginCLOBEnc;

private:
    /* LOB locator */
    SQLUBIGINT    mLOBLoc;

    /* LOB locator's type: SQL_C_BLOB_LOCATOR or SQL_C_CLOB_LOCATOR */
    SQLSMALLINT   mLOBLocCType;

    /* User buffer's type: SQL_C_BINARY or SQL_C_CHAR */
    SQLSMALLINT   mValCType;

    /* LOB length */
    UInt          mLOBLen;

    /* Current position in LOB */
    UInt          mPos;

    /* Whether error is occurred in SQLGetLOB() */
    iloBool        mIsFetchError;

    /* LOB access mode: LOBAccessMode_RDONLY or LOBAccessMode_WRONLY */
    LOBAccessMode mLOBAccessMode;

    // BUG-21837 LOB ���� ����� �����Ѵ�.
    /* User buffer used when user buffer's type is SQL_C_BINARY */
    UChar         mBinBuf[ILO_LOB_PIECE_SIZE];

    /* User buffer used when user buffer's type is SQL_C_CHAR */
    SChar         mChBuf[ILO_LOB_PIECE_SIZE * 2];

    void ConvertBinaryToChar(UInt aBinLen);

    IDE_RC ConvertCharToBinary( ALTIBASE_ILOADER_HANDLE aHandle, UInt aChLen);

    /* BUG-21064 : CLOB type CSV up/download error */
    void iloConvertCharToCSV( ALTIBASE_ILOADER_HANDLE  aHandle,
                              UInt                    *aValueLen );

    IDE_RC iloConvertCSVToChar( ALTIBASE_ILOADER_HANDLE aHandle, UInt *aStrLen );
};

/* TASK-2657 */
/* enum type for iloDataFile::getCSVToken() */
enum eReadState
{
    /*******************************************************
     * stStart   : state of starting up reading a field
     * stCollect : state of in the middle of reading a field
     * stTailSpace : at tailing spaces out of quotes
     * stEndQuote  : at tailing quote 
     * stError     : state of a wrong csv format is read
     * stRowTerm   : ensuring the row term state
     *******************************************************/
    stStart= 0 , stCollect, stTailSpace, stEndQuote, stError, stRowTerm
};

class iloDataFile
{
public:

    iloDataFile( ALTIBASE_ILOADER_HANDLE aHandle );
    
    ~iloDataFile();

    /* BUG-24358 iloader Geometry Data */
    IDE_RC ResizeTokenLen( ALTIBASE_ILOADER_HANDLE aHandle, UInt aLen );
    
    SInt OpenFileForUp( ALTIBASE_ILOADER_HANDLE  aHandle,
                        SChar                   *aDataFileName,
                        SInt                     aDataFileNo, 
                        iloBool                   aIsWr,
                        iloBool                    aLOBColExist );    //Upload ���� �Լ�
    
    FILE* OpenFileForDown( ALTIBASE_ILOADER_HANDLE  aHandle,
                           SChar                   *aDataFileName, 
                           SInt                     aDataFileNo,
                           iloBool                   aIsWr,
                           iloBool                   aLOBColExist );   //Download���� �Լ� 

    SInt CloseFileForUp( ALTIBASE_ILOADER_HANDLE aHandle );             //Upload ���� �Լ�
    
    SInt CloseFileForDown( ALTIBASE_ILOADER_HANDLE aHandle, FILE *fp);     //Download ���� �Լ�

    void SetTerminator(SChar *szFiledTerm, SChar *szRowTerm);

    void SetEnclosing(SInt bSetEnclosing, SChar *szEnclosing);

    
    SInt PrintOneRecord( ALTIBASE_ILOADER_HANDLE  aHandle,
                         SInt                     aRowNo,
                         iloColumns              *pCols,
                         iloTableInfo            *pTableInfo, 
                         FILE                    *aWriteFp,
                         SInt                     aArrayNum );  //PROJ-1714
    /* TASK-2657 */
    // BUG-28069: log, bad���� csv �������� ���
    static SInt csvWrite( ALTIBASE_ILOADER_HANDLE  aHandle,
                          SChar                   *aValue,
                          UInt                     aValueLen,
                          FILE                    *aWriteFp );

    EDataToken GetLOBToken( ALTIBASE_ILOADER_HANDLE  aHandle,
                            ULong                   *aLOBPhyOffs,
                            ULong                   *aLOBPhyLen,
                            ULong                   *aLOBLen );

    void rtrim();

    SInt strtonumCheck(SChar *p);

    void SetLOBOptions(iloBool aUseLOBFile, ULong aLOBFileSize,
                       iloBool aUseSeparateFiles, const SChar *aLOBIndicator);

    IDE_RC LoadOneRecordLOBCols( ALTIBASE_ILOADER_HANDLE  aHandle,
                                 SInt                     aRowNo,
                                 iloTableInfo            *aTableInfo,
                                 SInt                     aArrIdx, 
                                 iloSQLApi               *aISPApi);
    
    //PROJ-1714
    SInt        ReadFromFile(SInt aSize, SChar* aResult);
    SInt        ReadOneRecordFromCBuff( ALTIBASE_ILOADER_HANDLE  aHandle,
                                        iloTableInfo            *aTableInfo,
                                        SInt                     aArrayCount );
    SInt        ReadDataFromCBuff( ALTIBASE_ILOADER_HANDLE  aHandle,
                                   SChar                   *aResult );    //BUG-22434 : Double Buffer���� ���� �д´�.
    /* TASK-2657 */
    EDataToken  GetCSVTokenFromCBuff( ALTIBASE_ILOADER_HANDLE  aHandle, 
                                      SChar                   *aColName);
    
    EDataToken  GetTokenFromCBuff( ALTIBASE_ILOADER_HANDLE aHandle );
    
    SInt        WriteDataToCBuff( ALTIBASE_ILOADER_HANDLE aHandle,
                                  SChar* pBuf,
                                  SInt nSize);

    /* BUG-24583 Lob FilePath & FileName */
    IDE_RC      LOBFileInfoAlloc( ALTIBASE_ILOADER_HANDLE aHandle, UInt aRowCount );
    void        LOBFileInfoFree();

private:
    SChar      mDataFilePath[MAX_FILEPATH_LEN]; // BUG-24902: ����Ÿ ������ ��� ����
    FILE      *m_DataFp;
    UInt       mTokenMaxSize;
    /* ���� ������ ���� ��ȣ */
    SInt       mDataFileNo;
    /* ������ ���� ������ ���� ��ġ. ������ ���� �б� �� ���. */
    ULong      mDataFilePos;        //���� ���ۿ� �ֱ� ���� ���� Byte ��..
    /* mDataFilePos�� �ӽ� ����� ���� */
    ULong      mDataFileRead;       //PROJ-1714 : �����͸� ó���ϱ� ���� �������ۿ��� ���� Byte��.. (For LOB)
    SInt       mDoubleBuffPos;      //BUG-22434 : Double Buffer���� ���� ����Ʈ..
    SInt       mDoubleBuffSize;     //BUG-22434 : Double Buffer�� ���� ������ Size
    ULong      mDataFilePosBk;
    SInt       m_SetEnclosing;
    SChar      m_FieldTerm[MAX_SEPERATOR_LEN];
    SChar      m_RowTerm[MAX_SEPERATOR_LEN];
    SChar      m_Enclosing[MAX_SEPERATOR_LEN];
    SInt       m_nFTLen;
    /* �ʵ� ������ ������ ����.
     * Windows�� \n�� \r\n���� ����Ǵµ� \r�� ������ ���� ������ ����. */
    UInt       mFTPhyLen;
    /* ������ ���� �б� �� �ʵ� ������ ��Ī�� ��������ǥ */
    UChar    **mFTLexStateTransTbl;
    SInt       m_nRTLen;
    /* �� ������ ������ ���� */
    UInt       mRTPhyLen;
    /* ������ ���� �б� �� �� ������ ��Ī�� ��������ǥ */
    UChar    **mRTLexStateTransTbl;
    SInt       m_nEnLen;
    /* �ʵ� encloser ������ ���� */
    UInt       mEnPhyLen;
    /* ������ ���� �б� �� �ʵ� encloser ��Ī�� ��������ǥ */
    UChar    **mEnLexStateTransTbl;

    /* BUG-24358 iloader Geometry Data */    
    SChar *m_TokenValue;
    
    /* TASK-2657 */
    // BUG-27633: mErrorToken�� �÷��� �ִ� ũ��� �Ҵ�
    SChar     *mErrorToken;
    UInt       mErrorTokenMaxSize;
    /* m_TokenValue�� ����� ���ڿ��� ���� */
    UInt       mTokenLen;
    UInt       mErrorTokenLen;
    SInt       m_SetNextToken;
    EDataToken mNextToken;

    /* use_lob_file �ɼ� */
    iloBool     mUseLOBFileOpt;
    /* lob_file_size �ɼ�. ����Ʈ ����. */
    ULong      mLOBFileSizeOpt;
    /* use_separate_files �ɼ� */
    iloBool     mUseSeparateFilesOpt;
    /* lob_indicator �ɼ� */
    SChar      mLOBIndicatorOpt[MAX_SEPERATOR_LEN];
    /* lob_indicator ���ڿ� ����. ������ ���� �ƴ�. */
    UInt       mLOBIndicatorOptLen;

    /* LOB �÷��� ���� ���� */
    iloBool     mLOBColExist;
    /* LOB wrapper ��ü */
    iloLOB    *mLOB;
    /* LOB ���� ������ */
    FILE      *mLOBFP;
    /* LOB ���� ��ȣ.
     * use_lob_file=yes, use_separate_files=no�� �� ���. */
    UInt       mLOBFileNo;
    /* LOB ���� ������ ���� ��ġ.
     * use_lob_file=yes, use_separate_files=no�� �� ���. */
    ULong      mLOBFilePos;
    /* LOB ���ϵ� ������ ���� ��ġ.
     * ���� LOB ���� ��ȣ���� ���� ��ȣ�� ���� LOB ���ϵ��� ũ�Ⱑ ������ ��.
     * use_lob_file=yes, use_separate_files=no�� �� ���. */
    ULong      mAccumLOBFilePos;

    /* ������ �� LOB ���� �̸����� ����� ��ü �κ� */
    SChar      mFileNameBody[MAX_FILEPATH_LEN];
    /* ������ ���� Ȯ���� */
    SChar      mDataFileNameExt[MAX_FILEPATH_LEN];

    FILE      *mOutFileFP; // PROJ-2030, CT_CASE-3020 CHAR outfile ����
    
    void AnalDataFileName(const SChar *aDataFileName, iloBool aIsWr);

    UInt GetStrPhyLen(const SChar *aStr);

    IDE_RC MakeAllLexStateTransTbl( ALTIBASE_ILOADER_HANDLE aHandle );

    void FreeAllLexStateTransTbl();

    IDE_RC MakeLexStateTransTbl( ALTIBASE_ILOADER_HANDLE  aHandle, 
                                 SChar                   *aStr,
                                 UChar                   ***aTbl);

    void FreeLexStateTransTbl(UChar **aTbl);

    IDE_RC InitLOBProc( ALTIBASE_ILOADER_HANDLE aHandle, iloBool aIsWr);

    IDE_RC FinalLOBProc( ALTIBASE_ILOADER_HANDLE aHandle );

    // BUG-24902: ����Ÿ ������ ��ü ��ο��� ��θ� �����صд�.
    void InitDataFilePath(SChar *aDataFileName);

    IDE_RC OpenLOBFile( ALTIBASE_ILOADER_HANDLE  aHandle,
                        iloBool                   aIsWr, 
                        UInt                     aLOBFileNo_or_RowNo,
                        UInt                     aColNo,
                        iloBool                   aErrMsgPrint, 
                        SChar                   *aFilePath = NULL);        //BUG-24583

    IDE_RC CloseLOBFile( ALTIBASE_ILOADER_HANDLE aHandle );

    void ConvNumericNormToExp(iloColumns *aCols, SInt aColIdx, SInt aArrayNum);

    IDE_RC PrintOneLOBCol( ALTIBASE_ILOADER_HANDLE  aHandle,
                           SInt                     aRowNo, 
                           iloColumns              *aCols,
                           SInt                     aColNo, 
                           FILE                    *aWriteFp, 
                           iloTableInfo            *pTableInfo);        //BUG-24583

    IDE_RC PrintOneLOBColToDataFile( ALTIBASE_ILOADER_HANDLE  aHandle,
                                     iloColumns              *aCols, 
                                     SInt                     aColIdx, 
                                     FILE                    *aWriteFp);

    IDE_RC PrintOneLOBColToNonSepLOBFile( ALTIBASE_ILOADER_HANDLE  aHandle,
                                          iloColumns              *aCols, 
                                          SInt                     aColIdx, 
                                          FILE                    *aWriteFp );

    IDE_RC PrintOneLOBColToSepLOBFile( ALTIBASE_ILOADER_HANDLE  aHandle,
                                       SInt                     aRowNo,
                                       iloColumns              *aCols,
                                       SInt                     aColIdx,
                                       FILE                    *aWriteFp,
                                       iloTableInfo            *pTableInfo);        //BUG-24583

    IDE_RC AnalLOBIndicator(ULong *aLOBPhyOffs, ULong *aLOBPhyLen);

    IDE_RC LoadOneLOBCol( ALTIBASE_ILOADER_HANDLE  aHandle,
                          SInt                     aRowNo,
                          iloTableInfo            *aTableInfo,
                          SInt                     aColIdx,
                          SInt                     aArrIdx,
                          iloSQLApi               *aISPApi);

    IDE_RC LoadOneLOBColFromDataFile( ALTIBASE_ILOADER_HANDLE  aHandle,
                                      iloTableInfo            *aTableInfo,
                                      SInt                     aColIdx,
                                      SInt                     aArrIdx, 
                                      iloSQLApi               *aISPApi);

    IDE_RC LoadOneLOBColFromNonSepLOBFile( ALTIBASE_ILOADER_HANDLE  aHandle, 
                                           iloTableInfo            *aTableInfo,
                                           SInt                     aColIdx, 
                                           SInt                     aArrIdx, 
                                           iloSQLApi               *aISPApi);

    IDE_RC LoadOneLOBColFromSepLOBFile( ALTIBASE_ILOADER_HANDLE  aHandle, 
                                        SInt                     aRowNo,
                                        iloTableInfo            *aTableInfo,
                                        SInt                     aColIdx, 
                                        SInt                     aArrIdx, 
                                        iloSQLApi               *aISPApi);

    IDE_RC DataFileSeek( ALTIBASE_ILOADER_HANDLE aHandle,
                         ULong                   aDestPos );

    void BackupDataFilePos() { mDataFilePosBk = mDataFilePos; }

    IDE_RC RestoreDataFilePos( ALTIBASE_ILOADER_HANDLE aHandle )
    { return DataFileSeek( aHandle,  mDataFilePosBk); }

    IDE_RC GetLOBFileSize( ALTIBASE_ILOADER_HANDLE aHandle, ULong *aLOBFileSize);

    IDE_RC LOBFileSeek( ALTIBASE_ILOADER_HANDLE aHandle, ULong aAccumDestPos);

    IDE_RC loadFromOutFile( ALTIBASE_ILOADER_HANDLE aHandle ); // PROJ-2030, CT_CASE-3020 CHAR outfile ����
    
public:
    CCircularBuf mCirBuf;            //PROJ-1714 Data Upload�� ����ϴ� ���� ����
    SChar       *mDoubleBuff;        //BUG-22434 Double Buffer ��ü
    
    /* BUG-24583 Lob FilePath & FileName */
    SChar     **mLOBFile;
    SInt        mLOBFileRowCount;
    SInt        mLOBFileColumnNum;             //LOB Column�� ������ �϶�, �ش� LOB file�� ��� Column�� �ش��ϴ����� ����
    
public:
    void SetEOF( ALTIBASE_ILOADER_HANDLE aHandle, iloBool aVal )
    {
        mCirBuf.SetEOF( aHandle, aVal ); 
    }
    void InitializeCBuff( ALTIBASE_ILOADER_HANDLE aHandle )
    { mCirBuf.Initialize(aHandle); }
    void FinalizeCBuff( ALTIBASE_ILOADER_HANDLE aHandle)
    { mCirBuf.Finalize(aHandle); }
    
};

#endif /* _O_ILO_DATAFILE_H */


