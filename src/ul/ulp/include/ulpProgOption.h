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

#ifndef _UTP_PROGOPTION_H_
#define _UTP_PROGOPTION_H_ 1

#include <idl.h>
#include <ide.h>
#include <iduVersion.h>
#include <iduList.h>
#include <ulpErrorMgr.h>
#include <ulpMacro.h>


typedef struct ulpHeaderInfo
{
    SChar mFileName[MAX_INCLUDE_PATH_LEN + MAX_FILE_NAME_LEN];
    ulpHEADERSTATE  mState;
    idBool          mIsCInclude;
} ulpHeaderInfo;

/**********************************************************
 * DESCRIPTION :
 *
 * Precompiler �� command-line option ������ ���� �ִ� class
 * & include header file�� ���� ������ ������.
 **********************************************************/
class ulpProgOption
{
    private:
        /* file extension string ���� */
        SChar mFileExtName[MAX_FILE_EXT_LEN];
        /* Error ó���� ���� �ڷᱸ�� */
        ulpErrorMgr mErrorMgr;

    public:
        /*** Member functions ***/
        ulpProgOption();

        void ulpInit();

        /* command-line option�鿡 ���� �Ľ� */
        IDE_RC ulpParsingProgOption( SInt aArgc, SChar **aArgv );
        /* Preprocessing ���� �ӽ� file�� ���� �̸��� ������ */
        void ulpSetTmpFile();
        /* ������ ���� file�� ���� �̸��� ������ */
        void ulpSetOutFile();
        /* input/tmp/output file�� ���� �̸��� ������ */
        void ulpSetInOutFiles( SChar *aInFile );

        /* mIncludePathList�� path�� �ش� header ������ �����ϴ��� Ȯ����.*/
        IDE_RC ulpLookupHeader( SChar *aFileName, idBool aIsCInc );

        IDE_RC ulpPushIncList( SChar *aFileName, idBool aIsCInc );

        void   ulpPopIncList( void );

        SChar *ulpGetIncList( void );

        idBool ulpIsHeaderCInclude( void );

        IDE_RC ulpPrintCopyright();

        void ulpPrintVersion();

        void ulpPrintKeyword();

        void ulpPrintHelpMsg();
        /* mSysIncludePathList�� standard header file path�� ������. */
        void ulpSetSysIncludePath();

        /* NCHAR ���� �Լ� */
        void ulpFreeNCharVarNameList();

        void ulpAddPreDefinedMacro();

        /* BUG-28026 : add keywords */
        /* Keywords sorting function */
        void ulpQuickSort4Keyword( SChar **aKeywords, SInt aLeft, SInt aRight );

        /*** Member variables ***/

        /* �Է¹��� ����SQL���� ������ ����Ʈ */
        SChar mInFileList[MAX_INPUT_FILE_NUM][MAX_FILE_PATH_LEN + MAX_FILE_NAME_LEN];
        /* ����ó�� ���� ����SQL���� ���α׷� ���� */
        SChar mInFile[MAX_FILE_PATH_LEN + MAX_FILE_NAME_LEN];
        /* �߰� ���� �����̸� */
        SChar mTmpFile[MAX_FILE_PATH_LEN + MAX_FILE_NAME_LEN];
        /* ���� ���� �����̸� */
        SChar mOutFile[MAX_FILE_PATH_LEN + MAX_FILE_NAME_LEN];
        SChar mOutPath[MAX_FILE_PATH_LEN];
        /* name list of predefined names */
        SChar mDefineList[MAX_DEFINE_NUM][MAX_DEFINE_NAME_LEN];
        /* current working path */
        SChar mCurrentPath[MAX_FILE_PATH_LEN];
        /* start working path */
        SChar mStartPath[MAX_FILE_PATH_LEN];

        /** Variables for Header file **/
        /* include option path ����Ʈ */
        SChar mIncludePathList[MAX_HEADER_FILE_NUM][MAX_INCLUDE_PATH_LEN];
        /* preprocessor���� ó�� �Ǵ� include file ����Ʈ */
        ulpHeaderInfo mIncludeFileList[MAX_HEADER_FILE_NUM];
        /* include option path�� ���� */
        SInt  mIncludePathCnt;
        /* ó������ include file index */
        SInt  mIncludeFileIndex;

        /* the include path list for standard header files */
        SChar mSysIncludePathList[MAX_HEADER_FILE_NUM][MAX_INCLUDE_PATH_LEN];
        /* include path�� ���� for standard header files */
        SInt  mSysIncludePathCnt;
        /******************************/

        SChar mSpillValue[MAX_FILE_NAME_LEN];
        /* �Է¹��� ����SQL���� ���α׷� ������ ���� */
        SInt  mInFileCnt;
        /* ��õ� define ���� */
        SInt  mDefineCnt;

        /* -mt option for multi-threaded app. */
        idBool mOptMt;
        /* -o option for speciping output file path*/
        idBool mOptOutPath;
        /* -t */
        idBool mOptFileExt;
        /* -include */
        idBool mOptInclude;
        /* -align */
        idBool mOptAlign;
        /* -n */
        idBool mOptNotNullPad;
        /* -spill */
        idBool mOptSpill;
        /* -atc */
        idBool mOptAtc;
        /* -silent */
        idBool mOptSilent;
        /* -unsafe_null */
        idBool mOptUnsafeNull;
        /* -parse */
        idBool mOptParse;
        ulpPARSEOPT mOptParseInfo;
        /* -define */
        idBool mOptDefine;
        /* -debug macro */
        idBool mDebugMacro;
        /* -debug symbol */
        idBool mDebugSymbol;
        /* -pp */
        idBool mDebugPP;

        /* Version ���� */
        const SChar *mVersion;
        /* ���� Ȯ���� ���� */
        const SChar *mExtEmSQLFile;

        /* NCHAR */
        idBool  mNcharVar;
        iduList mNcharVarNameList;
        idBool  mNCharUTF16;

        /* BUG-42357 [mm-apre] The -lines option is added to apre. (INC-31008) */
        idBool  mOptLineMacro;
};

#endif
