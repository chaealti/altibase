/**
 *  Copyright (c) 1999~2017, Altibase Corp. and/or its affiliates. All rights reserved.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License, version 3,
 *  as published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _ULP_GENCODE_H_
#define _ULP_GENCODE_H_ 1

#include <idl.h>
#include <ide.h>
#include <ulpProgOption.h>
#include <ulpErrorMgr.h>
#include <ulpSymTable.h>
#include <ulpMacro.h>
#include <ulpTypes.h>

/* WHENEVER ���� ���� �ڷᱸ��*/
typedef struct ulpGenWheneverDetail 
{
    SInt               mScopeDepth;
    ulpGENWHENEVERACT  mAction;
    // Action�� �ʿ��� string ����.
    SChar              mText[MAX_WHENEVER_ACTION_LEN];
} ulpGenWheneverDetail;

/* ���� WHENEVER ���� �������� */
typedef struct ulpWhenever
{
    ulpGENWHENEVERCOND   mCondition;  // ���� ó������� Condition�� ���� ������ ���´�.
    // ���� Condition ������ action ������ ���´�.
    // [0] : SQL_NOTFOUND ���¿� ���� ����.
    // [1] : SQL_ERROR ���¿� ���� ����.
    ulpGenWheneverDetail mContent[2];
} ulpWhenever;

/* code��ȯ�� ���� �Ľ��� ȣ��Ʈ ���� ������ list�� �����Ѵ�. */
typedef struct ulpGenHostVarList
{
    SChar              mRealName[MAX_HOSTVAR_NAME_SIZE * 2];
    SChar              mRealIndName[MAX_HOSTVAR_NAME_SIZE * 2];
    SChar              mRealFileOptName[MAX_HOSTVAR_NAME_SIZE * 2];
    ulpHVarType        mInOutType;
    ulpHostDiagType    mDiagType;
    ulpSymTElement    *mValue;
    ulpSymTElement    *mInd;
} ulpGenHostVarList;

/* ���� ��ȯ �Ϸ��� ���� SQL������ ���� ������ �����Ѵ�. */
typedef struct  ulpGenEmSQLInfo
{
    SChar       mConnName[MAX_HOSTVAR_NAME_SIZE];
    SChar       mCurName[MAX_HOSTVAR_NAME_SIZE];
    SChar       mStmtName[MAX_HOSTVAR_NAME_SIZE];
    ulpStmtType mStmttype;
    SChar       mIters[GEN_EMSQL_INFO_SIZE];
    UInt        mNumofHostvar;
    SChar       mSqlinfo[GEN_EMSQL_INFO_SIZE];
    SChar       mScrollcur[GEN_EMSQL_INFO_SIZE];

    UInt        mCursorScrollable;
    UInt        mCursorSensitivity;
    UInt        mCursorWithHold;

    // ���� �������� cli�� �Ѱ��ֱ����� �պκ��� �߶� ���� pointer.
    SChar      *mQueryStr;
    SChar       mQueryHostValue[MAX_HOSTVAR_NAME_SIZE];

    /* host������ ������ ���� ulpSymbolNode �� list���·� �����Ѵ�. */
    iduList     mHostVar;
    // host���� list�� ����� �����鿡 ���� array,struct,arraystruct type ��������.
    // (isarr, isstruct ���� code ��ȯ�� ������)
    ulpGENhvType       mHostValueType;

    /* PSM ���� ����. */
    idBool mIsPSMExec;
    /* ������ string ������ ������. */
    SChar *mExtraStr;
    /* SQL_ATTR_PARAM_STATUS_PTR ���� ���� */
    SChar  mStatusPtr[MAX_HOSTVAR_NAME_SIZE];
    /* ATOMIC ����.*/
    SChar  mAtomic[GEN_EMSQL_INFO_SIZE];
    /* ErrCode ���� ���� */
    SChar  mErrCodePtr[MAX_HOSTVAR_NAME_SIZE];
    /* Multithread ���� ����. */
    idBool mIsMT;

    /* TASK-7218 Handling Multiple Errors */
    SChar  mConditionNum[GEN_EMSQL_INFO_SIZE];
} ulpGenEmSQLInfo;

/* BUG-35518 Shared pointer should be supported in APRE */
typedef struct ulpSharedPtrInfo
{
    /* The name of shared pointer */
    SChar       mSharedPtrName[MAX_HOSTVAR_NAME_SIZE * 2];
    /* Whether it was first declaration */
    idBool      mIsFirstSharedPtr;
    /* Whether previous declaration was single array */
    idBool      mIsPrevArraySingle;
    /* Previous name of variable */
    SChar       mPrevName[MAX_HOSTVAR_NAME_SIZE * 2];
    /* Previous size of variable */
    SChar       mPrevSize[MAX_HOSTVAR_NAME_SIZE * 2];
    /* Previous type of variable */
    ulpHostType mPrevType;
} ulpSharedPtrInfo;

/* BUG-42357 [mm-apre] The -lines option is added to apre. (INC-31008) */
typedef struct ulpGenCurFileInfo
{
    SInt  mFirstLineNo;
    SInt  mInsertedLineCnt;
    SChar mFileName[MAX_FILE_PATH_LEN + MAX_FILE_NAME_LEN];
} ulpGenCurFileInfo;


/**********************************************************
 * DESCRIPTION :
 *
 * Embedded SQL ������ C���� ��ȯ�����ִ� ����̸�,
 * File �ڵ�� file�� ������ temprary�ϰ� ����Ǵ� ���۸� �����Ѵ�.
 **********************************************************/
class ulpCodeGen
{

private:

    /* ��ȯ�� query string�� ����ȴ�. Initial size�� 32k �̳�,
       ���� size�� �̺��� �� ũ�� 2��� realloc�Ͽ� ���ȴ�.*/
    SChar *mQueryBuf;
    /* 10k ���� size �̸�, 10k�� ������ �ڵ�������
       utpGenWriteFile �Լ��� ȣ���Ͽ� file�� ����. */
    SChar mWriteBuf [ GEN_WRITE_BUF_SIZE ];

    ulpGenEmSQLInfo mEmSQLInfo;        /* ��ȯ �Ϸ��� ���� SQL������ ���� ������ �����Ѵ�. */
    UInt mWriteBufOffset;              /* write buffer�� ��ŭ ä�������� ��Ÿ�� (0-base)*/
    UInt mQueryBufSize;                /* ���� query buffer�� max size���� ����. */
    UInt mQueryBufOffset;              /* query buffer�� ��ŭ ä�������� ��Ÿ�� (0-base)*/

    /* ���屸���� ȣ��Ʈ ������ ?�� ��ȯ�ϴµ� �ʿ��� ȣ��Ʈ���� �ϳ��ϳ��� ���� ?���� ������ �迭�� ���� */
    UInt *mHostVarNumArr;
    UInt mHostVarNumOffset;
    UInt mHostVarNumSize;

    FILE *mOutFilePtr;                 /* output file pointer */
    SChar mOutFileName[ID_MAX_FILE_NAME + 1];

    ulpErrorMgr mErrorMgr;             /* Error ó���� ���� �ڷᱸ�� */

    /* BUGBUG: ��� scope table���� �����ؾ� �Ѵ�. */
    ulpWhenever mWhenever;             /* WHENEVER ���� ���� */

    void ulpGenSnprintf( SChar *aBuf, UInt aSize, const SChar *aStr, SInt aType );

    /* BUG-33025 Predefined types should be able to be set by user in APRE */
    /* Flag to decide whether ulpGenWriteFile is called by doPPparse(first call) */
    UInt mIsPpFlag;


    /* BUG-35518 Shared pointer should be supported in APRE */
    ulpSharedPtrInfo mSharedPtrInfo;

    /* BUG-42357 */
    ulpGenCurFileInfo mCurFileInfoArr[MAX_HEADER_FILE_NUM];
    SInt              mCurFileInfoIdx;

public:


    IDE_RC ulpInit();

    IDE_RC ulpFinalize();

    ulpCodeGen();                      /* initialize */

    SChar *ulpGetQueryBuf();

    void ulpGenNString ( SChar *aStr, UInt aLen);  /* Ư�� string�� �ش� ���̸�ŭ �״�� buffer�� ����. */

    void ulpGenString ( SChar *aStr );  /* Ư�� string�� �״�� buffer�� ����. */

    void ulpGenPutChar ( SChar aCh ); /* Ư�� charater�� buffer�� ����. */

    void ulpGenUnputChar ( void );  /* mWriteBufOffset��  ���ҽ�Ų��. */

    IDE_RC ulpGenQueryString ( SChar *aStr );  /* Ư�� query string�� �״�� mQueryBuf�� ����. */

    /* Ư�� query string�� aLen ��ŭ �״�� mQueryBuf�� ����. */
    //IDE_RC ulpGenQueryNString ( SChar *aStr, UInt aLen );

    void ulpGenComment( SChar *aStr );  /* Ư�� string�� comment ���·� buffer�� ����. */

    /* varchar���� �κ��� C code�� ��ȯ�Ͽ� buffer�� ����. */
    void ulpGenVarchar( ulpSymTElement *aSymNode );

    /* BUG-35518 Shared pointer should be supported in APRE */
    void ulpGenSharedPtr( ulpSymTElement *aSymNode );
    SChar *ulpConvertFromHostType( ulpHostType aHostType );

    /* Embedded SQL ������ ���� ������ mEmSQLInfo �� �����Ѵ�. */
    void ulpGenEmSQL( ulpGENSQLINFO aType, void *aValue );

    /* mEmSQLInfo������ mQueryBuf�� ���� �ڵ带 �����Ͽ� mWriteBuf�� ����.  */
    void ulpGenEmSQLFlush( ulpStmtType aStmtType, idBool aIsPrintQuery );

    void ulpGenHostVar( ulpStmtType aStmtType, ulpGenHostVarList *aHostVar, UInt *aCnt );

    /* mEmSQLInfo->mHostVar�� host���� ������ ���� utpSymbolNode �� �߰��Ѵ�. */
    IDE_RC ulpGenAddHostVarList( SChar          *aRealName,
                                 ulpSymTElement *aNode,
                                 SChar          *aIndName,
                                 ulpSymTElement *aIndNode,
                                 SChar          *aFileOptName,
                                 ulpHVarType     aIOType );

    /* mWriteBuf�� data�� file�� ����. */
    IDE_RC ulpGenWriteFile( );

    IDE_RC ulpGenOpenFile( SChar *aFileName);

    IDE_RC ulpGenCloseFile();

    /* BUG-33025 Predefined types should be able to be set by user in APRE */
    IDE_RC ulpGenRemovePredefine(SChar *aFileName);

    /* ó�� utpInitialize �� ȣ�� �Ǿ��� ��ó�� �ʱ�ȭ �ȴ�. */
    void ulpGenClearAll();

    /* query buffer �� �ʱ�ȭ ���ش�. */
    void ulpGenInitQBuff( void );

    /* mEmSQLInfo.mNumofHostvar �� aNum ��ŭ ���� ��Ų��. */
    void ulpIncHostVarNum( UInt aNum );

    /* mEmSQLInfo�� �ʱ�ȭ ���ش�. */
    void ulpClearEmSQLInfo();

    /* BUG-35518 Shared pointer should be supported in APRE */
    void ulpClearSharedPtrInfo();

    /* ���� ������ cli�� �����Ҽ� �ֵ��� �������ش�. */
    void ulpTransEmQuery ( SChar *aQueryBuf );

    /* WHENEVER ���� �������� ���� �Լ� */
    void ulpGenSetWhenever( SInt aDepth,
                            ulpGENWHENEVERCOND aCond,
                            ulpGENWHENEVERACT aAct,
                            SChar *aText );

    void ulpGenPrintWheneverAct( ulpGenWheneverDetail *aWheneverDetail );

    void ulpGenResetWhenever( SInt aDepth );

    ulpWhenever *ulpGenGetWhenever( void );

    //
    void ulpGenAddHostVarArr( UInt aNum );

    void ulpGenCutQueryTail( SChar *aToken );

    void ulpGenCutQueryTail4PSM( SChar aCh );

    /* BUG-46824 */
    void ulpGenCutStringTail4PSM( SChar *aBuf, SChar aCh );

    void ulpGenRemoveQueryToken( SChar *aToken );

    // write some code at the beginning of the .cpp file.
    void ulpGenInitPrint( void );

    ulpGenEmSQLInfo *ulpGenGetEmSQLInfo( void );

    /* BUG-29479 : double �迭 ���� precompile �߸��Ǵ� ���߻���. */
    /*    ȣ��Ʈ���� �̸��� ���ڷ� �޾� �̸� �ǵڿ� array index�� �����ϴ� ������
     *   [...] �� ��� �ݺ��Ǵ��� count���ִ� �Լ�. */
    SShort ulpGenBraceCnt4HV( SChar *aValueName, SInt aLen );

    /* host value�� �����鸦 bitset���� �������ش�. */
    void ulpGenGetHostValInfo( idBool          aIsField,
                               ulpSymTElement *aHVNode,
                               ulpSymTElement *aINDNode,
                               UInt           *aHVInfo,
                               SShort          aBraceCnt4HV,
                               SShort          aBraceCnt4Ind
                             );

    /* for dbugging */
    void ulpGenDebugPrint( ulpSymTElement *aSymNode );

    /* BUG-35518 Shared pointer should be supported in APRE */
    /* get shared pointer name in ulpCompy.y */
    SChar *ulpGetSharedPtrName();

    /* BUG-42357 The -lines option is added to apre. (INC-31008) */
    void   ulpGenSetCurFileInfo( SInt   aFstLineNo,
                                 SInt   aInsLineCnt,
                                 SChar *aFileNm );
    void   ulpGenResetCurFileInfo();
    void   ulpGenAddSubHeaderFilesLineCnt();
    idBool ulpGenIsHeaderFile();
    SInt   ulpGenMakeLineMacroStr( SChar *aBuffer, UInt aBuffSize = MAX_FILE_PATH_LEN + MAX_FILE_NAME_LEN );
    void   ulpGenPrintLineMacro();
};

#endif
