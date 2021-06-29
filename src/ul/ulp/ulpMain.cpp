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

/***********************************************************************
 * APRE C/C++ precompiler
 *
 *    APRE �� ������ ũ�� �ļ�, �ڵ������, �׸��� APRE library�� �����ȴ�.
 *
 *    1. �ļ�
 *          - ulpPreprocl/y    ; Macro �Ľ��� �����.
 *          - ulpPreprocIfl/y  ; Macro �������� �Ľ��� �����.
 *          - ulpCompl/y       ; ���� SQL������ C��� �Ľ��� �����.
 *    1.1. �Ľ� ó�� ����
 *          - ulpPreprocl/y ���� Macro �Ľ��� �ϸ鼭 #if�� ���� �������� ������ ulpPreprocIfl/y
 *            �ļ��� ȣ���Ͽ� ������� ���´�.
 *          - ulpPreprocl/y ���� Macro �Ľ��� ��ġ�� macroó���� �Ϸ��� .pp ����(�ӽ�����)�� �����ȴ�.
 *          - �׷��� ulpCompl/y �ļ��� .pp�� input���� ���� SQL������ C��� �Ľ��� �Ѵ�.
 *            (ulpCompl/y �ļ� ������ �κ������� macro �Ľ��� �Ѵ�.)
 *          - ulpCompl/y �Ľ��� �Ϸ�Ǹ� .c/.cpp ������ �����ȸ� .pp ������ ���ŵȴ�.
 *
 *    2. �ڵ������
 *          - ulpGenCode.cpp   ; �ļ����� �Ľ��� �ڵ��������� �Լ����� ȣ���Ͽ�
 *                               �ڵ带 ��ȯ�Ͽ� ������Ͽ� ����.
 *
 *    3. APRE library
 *          - ./lib �� �ִ� ��� �ڵ���� �����ϸ�, apre���� ��ó���� ��ģ ��ȯ���ڵ忡��
 *            ���Ǵ� interface���� �����Ѵ�. ���������� ODBC cli�Լ��� ȣ���Ͽ� SQL���屸������
 *            ����� �����Ѵ�.
 *
 ***********************************************************************/

#include <ulpMain.h>

/* extern for parser functions */
extern int doPPparse  ( SChar *aFilename );   // parser for preprocessor
extern int doCOMPparse( SChar *aFilename );   // parser for precompiler


int main( SInt argc, SChar **argv )
{
    SInt sI;
    iduMemory       sUlpMem;
    iduMemoryStatus sUlpMempos;

    gUlpProgOption.ulpInit();

    /*********************************
     * 1. Command-line option ó��
     *      : utpProgOption ��ü���� ó��.
     *********************************/

    /* �־��� option���� ó���Ѵ�. */
    if ( gUlpProgOption.ulpParsingProgOption( argc, argv ) != IDE_SUCCESS )
    {
        idlOS::exit( 1 );
    }

    /* Copywrite message�� �����ش�. */
    if( gUlpProgOption.mOptSilent == ID_FALSE )
    {
        gUlpProgOption.ulpPrintCopyright();
    }

    /* initialize iduMemory */
    sUlpMem.init(IDU_MEM_OTHER);
    gUlpMem = &sUlpMem;

    for( sI = 0 ; sI < gUlpProgOption.mInFileCnt ; sI++ )
    {

        gUlpCodeGen.ulpInit();
        gUlpMacroT.ulpInit();
        gUlpScopeT.ulpInit();
        gUlpStructT.ulpInit();

        gUlpProgOption.ulpAddPreDefinedMacro();

        /* ���� ó���� ����SQL���� ���α׷� file ���� & �߰�file ���� & ���file ����. */
        gUlpProgOption.ulpSetInOutFiles( gUlpProgOption.mInFileList[ sI ] );

        /*********************************
        * 2. Do Preprocessing
        *********************************/

        /* preprocessing output �߰� file�� ����. */
        gUlpCodeGen.ulpGenOpenFile( gUlpProgOption.mTmpFile );
        gUlpErrDelFile = ERR_DEL_TMP_FILE;

        IDE_ASSERT(gUlpMem->getStatus( &sUlpMempos ) == IDE_SUCCESS);

        /* Preprocessing�� �����Ѵ�.*/
        doPPparse( gUlpProgOption.mInFile );

        IDE_ASSERT(gUlpMem->setStatus( &sUlpMempos ) == IDE_SUCCESS);

        /* ulpCodeGen ��ü�� �ʱ�ȭ �����ش�. */
        gUlpCodeGen.ulpGenClearAll();

        if ( gUlpProgOption.mDebugMacro == ID_TRUE )
        {
            // macro table�� �����ش�.
            gUlpMacroT.ulpMPrint();
        }

        /*********************************
        * 4. Do Precompiling
        *********************************/

        /* BUG-28061 : preprocessing����ġ�� marco table�� �ʱ�ȭ�ϰ�, *
         *             ulpComp ���� �籸���Ѵ�.                       */
        // macro table �ʱ�ȭ.
        gUlpMacroT.ulpFinalize();
        gUlpProgOption.ulpAddPreDefinedMacro();

        /* precompiling output file�� ����. */
        gUlpCodeGen.ulpGenOpenFile( gUlpProgOption.mOutFile );
        gUlpErrDelFile = ERR_DEL_ALL_FILE;

        IDE_ASSERT(gUlpMem->getStatus( &sUlpMempos ) == IDE_SUCCESS);

        /* precompiling�� �����Ѵ�. */
        doCOMPparse( gUlpProgOption.mTmpFile );

        IDE_ASSERT(gUlpMem->setStatus( &sUlpMempos ) == IDE_SUCCESS);

        /* ulpCodeGen ��ü�� �ʱ�ȭ �����ش�. */
        gUlpCodeGen.ulpGenClearAll();

        if ( gUlpProgOption.mDebugPP != ID_TRUE )
        {
            /* �߰� file�� �����Ѵ�. */
            IDE_TEST_RAISE ( idlOS::remove( gUlpProgOption.mTmpFile ) != 0,
                             ERR_FILE_TMP_REMOVE );
        }

        /* BUG-33025 Predefined types should be able to be set by user in APRE */
        gUlpCodeGen.ulpGenRemovePredefine(gUlpProgOption.mOutFile);
        gUlpCodeGen.ulpGenCloseFile();

        if ( gUlpProgOption.mDebugSymbol == ID_TRUE )
        {
            // struct table �� symbol table �� �����ش�.
            gUlpStructT.ulpPrintStructT();
            gUlpScopeT.ulpPrintAllSymT();
        }

        gUlpCodeGen.ulpFinalize();
        gUlpMacroT.ulpFinalize();
        gUlpScopeT.ulpFinalize();
        gUlpStructT.ulpFinalize();

        gUlpErrDelFile = ERR_DEL_FILE_NONE;
    }

    gUlpProgOption.ulpFreeNCharVarNameList();

    /* iduMemory free */
    gUlpMem->destroy();

    // bug-27661 : sun ��񿡼� apre main�Լ� exit �Ҷ� SEGV
    idlOS::exit(0);

    IDE_EXCEPTION ( ERR_FILE_TMP_REMOVE );
    {
        ulpErrorMgr mErrorMgr;
        ulpSetErrorCode( &mErrorMgr,
                         ulpERR_ABORT_FILE_DELETE_ERROR,
                         gUlpProgOption.mTmpFile, errno );
        ulpPrintfErrorCode( stderr,
                            &mErrorMgr);
        ulpFinalizeError();
    }
    IDE_EXCEPTION_END;

    // bug-27661 : sun ��񿡼� apre main�Լ� exit �Ҷ� SEGV
    idlOS::exit(-1);
}

void ulpFinalizeError()
{
    SChar *sFileName;

    if ( gUlpProgOption.mDebugPP != ID_TRUE )
    {
        switch ( gUlpErrDelFile )
        {
            case ERR_DEL_FILE_NONE:
                // do nothing 
                break;
            case ERR_DEL_TMP_FILE :
                sFileName = gUlpProgOption.mTmpFile;
                IDE_TEST_RAISE ( idlOS::remove( sFileName ) != 0,
                                 ERR_FILE_TMP_REMOVE );
                break;
            case ERR_DEL_ALL_FILE :
                sFileName = gUlpProgOption.mTmpFile;
                IDE_TEST_RAISE ( idlOS::remove( sFileName ) != 0,
                                 ERR_FILE_TMP_REMOVE );
                sFileName = gUlpProgOption.mOutFile;
                IDE_TEST_RAISE ( idlOS::remove( sFileName ) != 0,
                                 ERR_FILE_TMP_REMOVE );
                break;
            default:
                IDE_ASSERT(0);
                break;
        }
    }

    idlOS::exit(1);

    IDE_EXCEPTION ( ERR_FILE_TMP_REMOVE );
    {
        ulpErrorMgr mErrorMgr;
        ulpSetErrorCode( &mErrorMgr,
                         ulpERR_ABORT_FILE_DELETE_ERROR,
                         sFileName, errno );
        ulpPrintfErrorCode( stderr,
                            &mErrorMgr);
    }
    IDE_EXCEPTION_END;

    idlOS::exit(1);
}
