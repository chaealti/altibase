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
 
/*******************************************************************************
 * $Id: utScanner.cpp 89731 2021-01-11 00:37:00Z chkim $
 ******************************************************************************/
#include <mtcl.h>
#include <uto.h>
#include <utAtb.h>

/* TASK-4212: audit���� ��뷮 ó���� ���� */
// static variables
SChar utScanner::mFieldTerm = ',';
SChar utScanner::mRowTerm   = '\n';
SChar utScanner::mEnclosing = '"';
utProperties *utScanner::mProp;

IDL_EXTERN_C  SChar * str_case_str(const SChar*,const SChar*);

// row ������ �񱳼�����.
compare_t utScanner::compare()
{
    SInt       i, count, c;
    Field     *f, *s;
    compare_t  sCmp = CMP_OK;
    bool       isLob = false;
    idBool     sUseFraction = ID_TRUE;
    dmlQuery  *sLobQuery = NULL;

    if((mConnA->getDbType() == DBA_ORA) || (mConnB->getDbType() == DBA_ORA))
    {
        sUseFraction = ID_FALSE;
    }

    count = mMeta->getPKSize();

    for(i = 1, f = mRowA->getField(i); i<= count ;f = mRowA->getField(++i) )
    {
        s = mRowB->getField(i);
        c = f->compareLogical(s, sUseFraction);
        IDE_TEST_RAISE( c != 0, diff_primary_key );
    }

    for( f = mRowA->getField(i); f ;f = mRowA->getField(++i) )
    {
        s = mRowB->getField(i);

        /* BUG-45909 Improve LOB Processing, Step1
         *   FileMode������ �ǳʶٴ� �� ����
         */
        if((f->getSQLType() == SQL_BLOB) || (f->getSQLType() == SQL_CLOB))
        {
            if ( mIsFileMode == true )
            {
                isLob = true;
            }
            else
            {
                IDE_TEST_RAISE(f->compareLob(s) != true, diff_column);
            }
        }
        else
        {
            // BUG-17167
            if((mConnA->getDbType() == DBA_ATB) && (mConnB->getDbType() == DBA_ATB))
            {
                IDE_TEST_RAISE(f->comparePhysical(s, sUseFraction) != true, diff_column);
            }
            else
            {
                IDE_TEST_RAISE(f->compareLogical(s, sUseFraction) != 0, diff_column);
            }
        }
    }

    /* BUG-45909 Improve LOB Processing */
    /* 
     * BUG-32566
     *
     * mMI ���� NULL üũ�ϵ��� ����
     * Insert Master�� OFF �Ǿ� �ִ� ��� SegFault ����.
     * BUG-40205
     * => Insert Master�� OFF �� ��� lob Ÿ���� �˻簡 ���� �����Ƿ� 
     *    mSI, mSD ���� ����� �� �ֵ��� ����.
     *    lobAtToAt �Լ��� static ���� �ٲٴ� ���� ���� �� ������ ������..
     */
    if ( isLob == true )
    {
        if (mMI != NULL)
        {
            sLobQuery = mMI;
        }
        else if (mSI != NULL)
        {
            sLobQuery = mSI;
        }
        else if (mSD != NULL)
        {
            sLobQuery = mSD;
        }
        else if (mSU != NULL)
        {
            sLobQuery = mSU;
        }
        IDE_TEST_CONT(sLobQuery == NULL, skip_compare_lob);

        mSelectA->lobCompareMode = true;
        sLobQuery->lobAtToAt(mSelectA, mSelectB,
                             (SChar *)mTableNameA, (SChar *)mTableNameB);
        mSelectA->lobCompareMode = false;
        IDE_TEST_RAISE(((mSelectA->getLobDiffCol() != NULL) ||
                        (mSelectB->getLobDiffCol() != NULL)),
                       diff_lob_column);

        /* TASK-4212: audit���� ��뷮 ó���� ���� */
        // lob �� ���ԵǾ��ִ� row�� ���ϸ�, ���ѵ� comit�ع���.
        mConnA->commit();
        mConnB->commit();
    }

    IDE_EXCEPTION_CONT( skip_compare_lob );

    return sCmp;

    IDE_EXCEPTION( diff_column );
    {
        sCmp = CMP_CL;
        diffColumn.name = f->getName();

/// idlOS::printf("CL.diff->%s[%d].%s\n",mTableName, mRows, f->getName());
    }
    IDE_EXCEPTION( diff_primary_key );
    {
        sCmp = (c > 0 )?CMP_PKA:CMP_PKB;
        diffColumn.name = f->getName();

///idlOS::printf("%s.diff->%s[%d].%s\n",( (c > 0 )?"CMP_PKA":"CMP_PKB" )
///      ,mTableName, mRows, f->getName());
    }
    IDE_EXCEPTION( diff_lob_column );
    {
        sCmp = CMP_CL;
        if(mSelectA->getLobDiffCol() != NULL)
        {
            diffColumn.name = mSelectA->getLobDiffCol();
        }
        else if(mSelectB->getLobDiffCol() != NULL)
        {
            diffColumn.name = mSelectB->getLobDiffCol();
        }
        mSelectA->setLobDiffCol(NULL);
        mSelectB->setLobDiffCol(NULL);

        mConnA->commit();
        mConnB->commit();
    }
    IDE_EXCEPTION_END;

    ++mDiffCount;

    return sCmp;
}


IDE_RC utScanner::fetch( bool aFetchA, bool aFetchB)
{

    /* TASK-4212: audit���� ��뷮 ó���� ���� */
    if ( mIsFileMode == false )
    {
        if(mRowA && aFetchA)
        {
            mRowA = mSelectA->fetch(mConnA->getDbType());
        }

        if(mRowB && aFetchB)
        {
            mRowB = mSelectB->fetch(mConnB->getDbType());
        }
    }
    else
    {
        if(mRowA && aFetchA)
        {
            mRowA = utaReadCSVRow( &mMasterFile, mSelectA );
        }

        if(mRowB && aFetchB)
        {
            mRowB = utaReadCSVRow( &mSlaveFile, mSelectB );
        }
    }

    return IDE_SUCCESS;
}

UInt utScanner::progress()
{
    ++_fetch;
    if( (_fetch % mCountToCommit) == 0)
    {
        /* //BUGBUG Altibase can not do SELECT + DELETE
           if( mConnB )
           {
           IDE_TEST( mConnB->commit() != IDE_SUCCESS);
           }
           if( mConnA )
           {
           /IDE_TEST( mConnA->commit() != IDE_SUCCESS);
           }
        */
        idlOS::fprintf(stderr,".");
        idlOS::fflush(stderr);
    }

    IDE_TEST( mSelectA->rows() > _limit );

    return _fetch;

    IDE_EXCEPTION_END;

    if( utProperties::mVerbose)
    {
        idlOS::fprintf(stderr,"LIMIT exceed %d/%d\n",
                       mSelectA->rows(), _limit);
    }

    return 0;
}

void utScanner::reset()
{
    _fetch   = 0;
    fetchA   = true;
    fetchB   = true;

    if(mSD) mSD->reset();
    if(mSI) mSI->reset();
    if(mSU) mSU->reset();

    if(mMI) mMI->reset();

    un_idx   = un_idx && mSD;
    mTimeStamp = getTimeStamp();

    // BUG-17951
    mMOSODiffCount = 0;
    mMXSODiffCount = 0;
    mMOSXDiffCount = 0;
    
    // BUG-47693
    mMOSOSameCount = 0;
}

IDE_RC utScanner::execute()
{
    // delete row
    (void)reset();

    /* TASK-4212: audit���� ��뷮 ó���� ���� */
    if ( mIsFileMode == false )
    {
        /* next execution */
        if( mRowA == NULL )
        {
            IDE_TEST( mSelectA->close() != IDE_SUCCESS );    // BUG-18732
            IDE_TEST( mSelectA->execute() != IDE_SUCCESS );
            mRowA = mSelectA->fetch(mConnA->getDbType());
        }

        if( mRowB == NULL )
        {
            IDE_TEST( mSelectB->close() != IDE_SUCCESS );    // BUG-18732
            IDE_TEST( mSelectB->execute() != IDE_SUCCESS );
            mRowB = mSelectB->fetch(mConnB->getDbType());
        }
    }
    else
    {   // it's on the file mode.
        IDE_TEST_RAISE( (mMasterFile.mFile = idlOS::fopen( mMasterFile.mName, "rb" ))
                == NULL, ERR_MFILE_OPEN );
        IDE_TEST_RAISE( (mSlaveFile.mFile = idlOS::fopen( mSlaveFile.mName, "rb" ))
                == NULL, ERR_SFILE_OPEN );
        // fetch one master row.
        mRowA = utaReadCSVRow( &mMasterFile, mSelectA );
        // fetch one slave row.
        mRowB = utaReadCSVRow( &mSlaveFile, mSelectB );
    }

    // ** 1. pair fetching process ** //
    //If there's column that data type is Lob, it will execute updating after inserting.
    //doM?S?()�ȿ��� bind()�� execute() ����.
    //bind()�ÿ� lob �÷��� ���� NULL�� ����.
    //execute()�ÿ� lob�÷��� �����ϸ� �׿� ���� �۾��� �߰������Ѵ�.
    while( mRowA && mRowB )
   {
        cmp = compare();
        switch( cmp )
        {
            case CMP_OK:
                fetchA  = true;
                fetchB  = true;

                // BUG-47693
                mMOSOSameCount++;

                /* BUG-48134 Print MOSO EQ record to log file optionallay */
                if ( utProperties::mIsLogEqMOSO == true )
                {
                    IDE_TEST( log_equal() != IDE_SUCCESS );
                }
                break;

            case CMP_CL:
                fetchA  = true;
                fetchB  = true;

                IDE_TEST( doMOSO() != IDE_SUCCESS);
                break;

            case CMP_PKB:
                fetchA  = true ;
                fetchB  = false;

                IDE_TEST( doMOSX() != IDE_SUCCESS);
                break;

            case CMP_PKA:
                fetchA  = false;
                fetchB  = true ;

                IDE_TEST( doMXSO() != IDE_SUCCESS);
                break;

            default:;
        }

        IDE_TEST( fetch( fetchA, fetchB ) != IDE_SUCCESS);
        if( progress() == 0 )
        {
            goto LIMIT;
        }
    }

    /* TASK-4212: audit���� ��뷮 ó���� ���� */
    // ** 2. Tail processing for Master ** //
    cmp = CMP_PKB;
    while( mRowA != NULL )
    {
        IDE_TEST( doMOSX()   != IDE_SUCCESS);

        if( progress() == 0)
        {
            goto LIMIT;
        }

        if ( mIsFileMode == false )
        {
            mRowA = mSelectA->fetch(mConnA->getDbType());
        }
        else
        {
            mRowA = utaReadCSVRow( &mMasterFile, mSelectA );
        }
    }

    // ** 3. Tail processing for Slave ** //
    cmp = CMP_PKA;
    while( mRowB != NULL )
    {
        IDE_TEST( doMXSO() != IDE_SUCCESS);

        if( progress() == 0)
        {
            goto LIMIT;
        }

        if ( mIsFileMode == false )
        {
            mRowB = mSelectB->fetch(mConnB->getDbType());
        }
        else
        {
            mRowB = utaReadCSVRow( &mSlaveFile, mSelectB );
        }
    }

  LIMIT: /*  Limit expression end point */

    IDE_TEST( mConnB->commit() != IDE_SUCCESS);
    IDE_TEST( mConnA->commit() != IDE_SUCCESS);

    mTimeStamp = getTimeStamp() - mTimeStamp;
    if( flog != NULL )
    {
        idlOS::fflush(flog);
    }

    /* TASK-4212: audit���� ��뷮 ó���� ���� */
    if ( mIsFileMode == true )
    {
        IDE_TEST_RAISE(idlOS::fclose(mMasterFile.mFile) != 0, ERR_MFILE_CLOSE);
        IDE_TEST_RAISE(idlOS::fclose(mSlaveFile.mFile) != 0, ERR_SFILE_CLOSE);
        idlOS::remove( mMasterFile.mName );
        idlOS::remove( mSlaveFile.mName );
    }

    return IDE_SUCCESS;
    IDE_EXCEPTION( ERR_MFILE_OPEN );
    {
        if( _error)
        {
            uteSetErrorCode( &gErrorMgr, utERR_ABORT_AUDIT_File_Open_4CSVfile,
                             mMasterFile.mFile );
            uteSprintfErrorCode(_error, ERROR_BUFSIZE, &gErrorMgr);
        }
    }
    IDE_EXCEPTION( ERR_SFILE_OPEN );
    {
        if( _error)
        {
            uteSetErrorCode( &gErrorMgr, utERR_ABORT_AUDIT_File_Open_4CSVfile,
                             mSlaveFile.mFile );
            uteSprintfErrorCode(_error, ERROR_BUFSIZE, &gErrorMgr);
        }
    }
    IDE_EXCEPTION( ERR_MFILE_CLOSE );
    {
        if( _error)
        {
            uteSetErrorCode( &gErrorMgr, utERR_ABORT_AUDIT_File_Close_4CSVfile,
                             mMasterFile.mFile );
            uteSprintfErrorCode(_error, ERROR_BUFSIZE, &gErrorMgr);
        }
    }
    IDE_EXCEPTION( ERR_SFILE_CLOSE );
    {
        if( _error)
        {
            uteSetErrorCode( &gErrorMgr, utERR_ABORT_AUDIT_File_Close_4CSVfile,
                             mSlaveFile.mFile );
            uteSprintfErrorCode(_error, ERROR_BUFSIZE, &gErrorMgr);
        }
    }
    IDE_EXCEPTION_END;
    return IDE_FAILURE;
}


/* TASK-4212: audit���� ��뷮 ó���� ���� */
Row *utScanner::utaReadCSVRow( utaFileInfo *aFileInfo, Query *aQuery )
{
/***********************************************************************
 *
 * Description :
 *    file buffer �κ��� ��row�� field����ŭ �ݺ��ؼ� CSV data�� ���� data��
 *    ��ȯ�Ͽ� ���´�.
 *
 ***********************************************************************/
    SInt   i;
    Row   *sRow;

    /* BUG-32569 The string with null character should be processed in Audit */
    UInt  sValueLength;

    utaCSVTOKENTYPE sTmpToken;

    switch( aFileInfo->mOffset )
    {
        case -1:
            // ���� file�� �о���� �ʾҴ�.
        case MAX_FILE_BUF:
            // ������ �о���.
            aFileInfo->mFence = idlOS::fread(aFileInfo->mBuffer, 1, MAX_FILE_BUF,
                                             aFileInfo->mFile);

            if( aFileInfo->mFence != MAX_FILE_BUF )
            {
                IDE_TEST_RAISE( ferror(aFileInfo->mFile), ERR_READ_FILE );
                IDE_TEST_RAISE( aFileInfo->mFence == 0, MEET_EOF );
            }
            aFileInfo->mOffset = 0;
            break;
        default:
            break;
    }

    sRow = aQuery->getRow();
    for( i = 1 ; i <= mFiledsCount ; i++ )
    {
        // �켱 field data�� null�� �ƴ϶�� �ʱ�ȭ������.
        /* BUG-32569 The string with null character should be processed in Audit */
        sValueLength = 0;
        sRow->getField(i)->setIsNull( false );

        if ( (sRow->getRealSqlType(i) != SQL_BLOB) &&
             (sRow->getRealSqlType(i) != SQL_CLOB) )
        {
            sTmpToken = utaGetCSVTokenFromBuff( aFileInfo,
                                                sRow->getField(i)->getValue(),
                                                sRow->getField(i)->getValueSize(),
                                                &sValueLength );
            /* BUG-32569 The string with null character should be processed in Audit */
            sRow->getField(i)->setValueLength(sValueLength);
        }
        else
        {   // LOB type�̶�� ������ T_NULL_VALUE�� ���ϵɰ��̱� ������, value buffer�� size�� �ʿ����.
            sTmpToken = utaGetCSVTokenFromBuff( aFileInfo, NULL, (UInt) 0, NULL );
        }

        switch( sTmpToken )
        {
            case T_EOF:
                IDE_RAISE( MEET_EOF );
                break;
            case T_VALUE:
                break;
            case T_NULL_VALUE:
                // is Null data
                sRow->getField(i)->setIsNull( true );
                break;
            default:
                IDE_RAISE( ERR_CSV_FORMAT );
                break;
        }

        if ( i != mFiledsCount )
        {
            switch( utaGetCSVTokenFromBuff( aFileInfo, NULL, (UInt) 0 , NULL ) )
            {
                case T_FIELD_TERM:
                    break;
                default:
                    IDE_RAISE( ERR_CSV_FORMAT );
                    break;
            }
        }
    }

    switch( utaGetCSVTokenFromBuff( aFileInfo, NULL, (UInt) 0, NULL ) )
    {
        case T_ROW_TERM:
            break;
        default:
            IDE_RAISE( ERR_CSV_FORMAT );
            break;
    }

    // row�� �ϳ� �������� _rows ������ 1�߰� ��Ų��.
    aQuery->utaIncRows();

    return sRow;

    IDE_EXCEPTION( ERR_READ_FILE );
    {
        if( _error)
        {
            uteSetErrorCode( &gErrorMgr, utERR_ABORT_AUDIT_File_Read_4CSVfile,
                             aFileInfo->mName );
            uteSprintfErrorCode(_error, ERROR_BUFSIZE, &gErrorMgr);
            mProp->log("FATAL[ TASK ] Process failure! [SCANER]: %s\n",_error);
        }

        idlOS::exit(1);
    }
    IDE_EXCEPTION( ERR_CSV_FORMAT );
    {
        if( _error)
        {
            uteSetErrorCode( &gErrorMgr, utERR_ABORT_AUDIT_Wrong_CSV_Format,
                             aFileInfo->mName );
            uteSprintfErrorCode(_error, ERROR_BUFSIZE, &gErrorMgr);
            mProp->log("FATAL[ TASK ] Process failure! [SCANER]: %s\n",_error);
        }

        idlOS::exit(1);
    }
    IDE_EXCEPTION( MEET_EOF );
    {
        aFileInfo->mFence  = 0;
        aFileInfo->mOffset = 0;
    }
    IDE_EXCEPTION_END;
    return NULL;
}


IDE_RC utScanner::exec(dmlQuery * aQ)
{
    const SChar * sType;
    
    if(aQ != NULL)
    {
        if(aQ->execute(mIsFileMode) != IDE_SUCCESS)
        {
            if(flog != NULL)
            {
                idlOS::fprintf(flog,"%s->%s\n", aQ->getType(), aQ->error());
            }

            return IDE_FAILURE;
        }

        if(mIsFileMode == true && mMeta->getCLSize(true))
        {
            sType = aQ->getType();

            //sync mode������ ����ǹǷ� mModeüũ�� �ʿ� ���� ��..
            if(sType[1] == 'I' || sType[1] == 'U')
            {
                if(sType[0] == 'M')
                {
                    aQ->lobAtToAt(mSelectB, mSelectA,
                                  (SChar *)mTableNameB, (SChar *)mTableNameA);
                }
                else if(sType[0] == 'S')
                {
                    aQ->lobAtToAt(mSelectA, mSelectB,
                                  (SChar *)mTableNameA, (SChar *)mTableNameB);
                }
                else
                {
                    idlOS::fprintf(stderr, "Invalid target of server : %s\n", sType);
                    return IDE_FAILURE;
                }
            }
        }
    }

    return IDE_SUCCESS;
}

IDE_RC utScanner::doMOSO()
{
    switch(mMode)
    {
        case DIFF:
            mMOSODiffCount++;   // BUG-17951
            /* BUG-48425 Print MOSX and MXSO record to log file optionallay */
            if ( utProperties::mIsLogDfMOSO == true )
            {
                IDE_TEST( log_diff(MOSO)  != IDE_SUCCESS);
            }
            break;

        case SYNC:
            if( mSU )
            {
                if( mRowA != NULL )
                {
                    IDE_TEST( mSU->bind(mRowA) != IDE_SUCCESS);
                }
                IDE_TEST_RAISE( exec(mSU) != IDE_SUCCESS, err_sync_exec );
            }
            else
            {
                if( mRowB != NULL && mSD )
                {
                    IDE_TEST( mSD->bind(mRowB) != IDE_SUCCESS);
                }
                IDE_TEST_RAISE( exec(mSD) != IDE_SUCCESS, err_sync_exec );
            }
            break;

        default:;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION ( err_sync_exec );
    {
        (void) log_record(MOSO);
    }
    IDE_EXCEPTION_END;

    return IDE_SUCCESS;
}

IDE_RC utScanner::doMXSO()
{
    switch(mMode)
    {
        case DIFF:
            mMXSODiffCount++;   // BUG-17951
            /* BUG-48425 Print MOSX and MXSO record to log file optionallay */
            if ( utProperties::mIsLogMXSO == true )
            {
                IDE_TEST( log_diff(MXSO)  != IDE_SUCCESS);
            }
            break;

        case SYNC:
            if( mMI )
            {
                if( mRowB != NULL )
                {
                    IDE_TEST( mMI->bind(mRowB) != IDE_SUCCESS);
                }
                IDE_TEST_RAISE( exec( mMI ) != IDE_SUCCESS, err_sync_exec );
            }
            else
            {
                if( mRowB != NULL && mSD )
                {
                    IDE_TEST( mSD->bind(mRowB) != IDE_SUCCESS);
                }
                IDE_TEST_RAISE( exec( mSD ) != IDE_SUCCESS, err_sync_exec );
            }
            break;

        default:;
    }
    return IDE_SUCCESS;

    IDE_EXCEPTION ( err_sync_exec );
    {
        (void) log_record(MXSO);
    }
    IDE_EXCEPTION_END;

    return IDE_SUCCESS;
}
IDE_RC utScanner::doMOSX()
{
    switch(mMode)
    {
        case DIFF:
            mMOSXDiffCount++;   // BUG-17951
            /* BUG-48425 Print MOSX and MXSO record to log file optionallay */
            if ( utProperties::mIsLogMOSX == true )
            {
                IDE_TEST( log_diff(MOSX)  != IDE_SUCCESS);
            }
            break;

        case SYNC:
            if( mRowA != NULL && mSI )
            {
                IDE_TEST( mSI->bind(mRowA) != IDE_SUCCESS);
            }
            IDE_TEST_RAISE(exec(mSI) != IDE_SUCCESS, err_sync_exec);

            break;

        default: ;
    }
    return IDE_SUCCESS;

    IDE_EXCEPTION ( err_sync_exec );
    {
        (void) log_record(MOSX);
    }
    IDE_EXCEPTION_END;

    return IDE_SUCCESS;
}

/* TASK-4212: audit���� ��뷮 ó���� ���� */
IDE_RC utScanner::prepare( utProperties *aProp )
{
    SChar *     sqlA;
    SChar *     sqlB;

    SInt        i;
    pmode_t     sMode;
    bool       *sDML;
    utaFileMArg sFileArgM;
    utaFileMArg sFileArgS;

    idtThreadRunner sThrId[NUM_FILE_THR];

    mProp = aProp;
    sMode = aProp->mMode;
    sDML  = aProp->mDML;

    mDML = sDML;
    mDiffCount  = 0;

    IDE_TEST( mSelectA->close() != IDE_SUCCESS);
    IDE_TEST( mSelectB->close() != IDE_SUCCESS);

    sqlA = mSelectA->statement(); // get SQL buffer
    sqlB = mSelectB->statement(); // get SQL buffer

    /* BUG-43455 Slave table not found error due to different user id from SCHEMA at altiComp.cfg */
    IDE_TEST( select_sql(sqlA, mSchemaA, mTableNameA)  != IDE_SUCCESS );
    IDE_TEST( select_sql(sqlB, mSchemaB, mTableNameB)  != IDE_SUCCESS );

    IDE_TEST( mSelectA->execute() != IDE_SUCCESS );
    IDE_TEST( mSelectB->execute() != IDE_SUCCESS );

    mFiledsCount =  mSelectA->columns();
    if(  mSelectB->columns() )
    {
        IDE_TEST_RAISE( mFiledsCount != mSelectB->columns() , err_diff_rssize );
    }

    /* TASK-4212: audit���� ��뷮 ó���� ���� */
    // mMaxArrayFetch�� 0���� ũ�� fetch�� CSV file�� ����.
    // oracle DB�� �ϳ��� �����ϸ� file�� ���� �ʰ� �ϳ��ϳ� fetch�� ��.
    if ( (aProp->mMaxArrayFetch  < 1)   ||
         (mSelectA->getDbType() != DBA_ATB) ||
         (mSelectB->getDbType() != DBA_ATB) )
    {
        IDE_TEST( bindColumns() != IDE_SUCCESS);
        mRowA = mSelectA->fetch(mConnA->getDbType());
        mRowB = mSelectB->fetch(mConnB->getDbType());
    }
    else
    {
        // set Filemode true
        mIsFileMode   = true;
        mSelectA->getRow()->setFileMode4Fields( true );
        mSelectB->getRow()->setFileMode4Fields( true );

        /* 1. set array fetch count */
        // altibase DB������ array fetch ��. ����Ŭ�� X.
        if ( aProp->mMaxArrayFetch > 1 )
        {
            mSelectA->setArrayCount( aProp->mMaxArrayFetch );
            mSelectB->setArrayCount( aProp->mMaxArrayFetch );
        }

        /* 2. bind columns */
        IDE_TEST( bindColumns() != IDE_SUCCESS);

        /* 3. set stmt for array fetch */
        IDE_TEST( mSelectA->setStmtAttr4Array() != IDE_SUCCESS);
        IDE_TEST( mSelectB->setStmtAttr4Array() != IDE_SUCCESS);

        /* 4. set file name and create */
        idlOS::snprintf( mMasterFile.mName, MAX_FILE_NAME,
                         "%s_M_%lu.csv", mTableNameA, getTimeStamp() );
        IDE_TEST_RAISE( (mMasterFile.mFile = idlOS::fopen( mMasterFile.mName, "wb" ))
                        == NULL, ERR_FILE_OPEN );

        idlOS::snprintf( mSlaveFile.mName, MAX_FILE_NAME,
                         "%s_S_%lu.csv", mTableNameB, getTimeStamp() );
        IDE_TEST_RAISE( (mSlaveFile.mFile = idlOS::fopen( mSlaveFile.mName, "wb" ))
                        == NULL, ERR_FILE_OPEN );

        sFileArgM.mFilename = mMasterFile.mName;
        sFileArgM.mFile  = mMasterFile.mFile;
        sFileArgM.mQuery = mSelectA;
        sFileArgM.mRow   = mRowA;

        /* 5. fetch and write CSV file */
        IDE_TEST_RAISE(
            sThrId[0].launch(utaFileModeWrite, (void*)&sFileArgM)
            != IDE_SUCCESS, ERR_THREAD);

        sFileArgS.mFilename = mSlaveFile.mName;
        sFileArgS.mFile  = mSlaveFile.mFile;
        sFileArgS.mQuery = mSelectB;
        sFileArgS.mRow   = mRowB;

        IDE_TEST_RAISE(
            sThrId[1].launch(utaFileModeWrite, (void*)&sFileArgS)
            != IDE_SUCCESS, ERR_THREAD);

        for( i = 0; i < NUM_FILE_THR; i++ )
        {
            IDE_TEST_RAISE( sThrId[i].join() != IDE_SUCCESS, ERR_THREAD );
        }

        /* 6. Commit */
        IDE_TEST( mConnA->commit() != IDE_SUCCESS);
        IDE_TEST( mConnB->commit() != IDE_SUCCESS);

        /* 7. close file */
        IDE_TEST_RAISE(idlOS::fclose(mMasterFile.mFile) != 0, ERR_MFILE_CLOSE);
        IDE_TEST_RAISE(idlOS::fclose(mSlaveFile.mFile) != 0, ERR_SFILE_CLOSE);
    }

    /* Set mode of scanner */
    switch( sMode)
    {
        case DIFF :
            IDE_TEST(setModeDIFF(  true  )!=IDE_SUCCESS);

        case SYNC :
            if( sDML[SI])
            {
                IDE_TEST( setSI() != IDE_SUCCESS);
            }
            if(sDML[SU])
            {
                IDE_TEST(setSU() != IDE_SUCCESS);
            }
            if(sDML[SD])
            {
                IDE_TEST( setSD() != IDE_SUCCESS);
            }
            if(sDML[MI])
            {
                IDE_TEST(setMI() != IDE_SUCCESS);
            }

            break;

        case DUMMY:
            IDE_TEST(setModeDIFF( false  )!=IDE_SUCCESS);
            break;
    }

    mMode = sMode;

    return IDE_SUCCESS;

    IDE_EXCEPTION( err_diff_rssize );
    {
        if( _error)
        {
            uteSetErrorCode(&gErrorMgr, utERR_ABORT_AUDIT_Different_Column_Error,
                            mTableNameA,mTableNameB);
            uteSprintfErrorCode(_error, ERROR_BUFSIZE, &gErrorMgr);
        }
    }
    IDE_EXCEPTION( ERR_THREAD );
    {
        if( _error)
        {
            uteSetErrorCode(&gErrorMgr, utERR_ABORT_AUDIT_Managing_Thread_Filemode,
                            mTableNameA,mTableNameB);
            uteSprintfErrorCode(_error, ERROR_BUFSIZE, &gErrorMgr);
        }
    }
    IDE_EXCEPTION( ERR_FILE_OPEN );
    {
        if( _error)
        {
            uteSetErrorCode( &gErrorMgr, utERR_ABORT_AUDIT_File_Open_4CSVfile,
                             (mSlaveFile.mName[0]=='\0')?
                                     mMasterFile.mName:mSlaveFile.mName );
            uteSprintfErrorCode((SChar *)_error, ERROR_BUFSIZE, &gErrorMgr);
        }
    }
    IDE_EXCEPTION( ERR_MFILE_CLOSE );
    {
        if( _error)
        {
            uteSetErrorCode( &gErrorMgr, utERR_ABORT_AUDIT_File_Close_4CSVfile,
                             mMasterFile.mName );
            uteSprintfErrorCode(_error, ERROR_BUFSIZE, &gErrorMgr);
        }
    }
    IDE_EXCEPTION( ERR_SFILE_CLOSE );
    {
        if( _error)
        {
            uteSetErrorCode( &gErrorMgr, utERR_ABORT_AUDIT_File_Close_4CSVfile,
                             mSlaveFile.mName );
            uteSprintfErrorCode(_error, ERROR_BUFSIZE, &gErrorMgr);
        }
    }
    IDE_EXCEPTION_END;
    return IDE_FAILURE;
}


/* TASK-4212: audit���� ��뷮 ó���� ���� */
void *utScanner::utaFileModeWrite( void *aFileArg )
{
/***********************************************************************
 *
 * Description :
 *    array fetch�� �Ѵ��� row����ŭ �ݺ��Ͽ�, ���� row�� field���� csv format
 *    ���� ��ȯ�Ͽ� file�� ����, ���Ͽ� �� ������ statement�� close����.
 *
 ***********************************************************************/
    SInt   sFetchedCount;
    SInt   sArrayNum;
    SInt   i;
    SInt   sFiledsCount;
    SInt   sValueLen;
    SInt   sSqlType;
    SChar *sFilename;
    FILE  *sFile;
    Query *sQuery;
    Row   *sRow;
    Field *sField;
    SChar *sValue;
    SChar  sError[ERROR_BUFSIZE]; // BUG-43607

    sFilename = ((utaFileMArg *)aFileArg) -> mFilename;
    sFile  = ((utaFileMArg *)aFileArg) -> mFile;
    sQuery = ((utaFileMArg *)aFileArg) -> mQuery;
    sRow   = ((utaFileMArg *)aFileArg) -> mRow;

    sValue        = NULL;
    sFiledsCount  = sQuery->columns();

    /* 1. fetch row */
    while ( (sRow = sQuery->fetch( DBA_ATB , true )) != NULL )
    {
        sFetchedCount = (SInt) sRow->mRowsFetched;

        /* 2. write to file */
        for ( sArrayNum = 0 ; sArrayNum < sFetchedCount ; sArrayNum++ )
        {
            for( i = 1; i <= sFiledsCount; i++ )
            {
                switch( sSqlType = sRow->getRealSqlType( i ) )
                {
                    case SQL_BLOB:
                    case SQL_CLOB:
                        // do nothing
                        break;
                    default:
                        sField   = sRow->getField( i );

                        if ( sSqlType == SQL_BYTE )
                        {   // SQL_BYTE�ϰ�� �����ڵ���� ȣȯ�������� +1 ����.
                            // utAtbField::bindColumn() ����.
                            sValue = (SChar*) (sField->getValue() +
                                     sArrayNum * (sField->getValueSize()+1));
                        }
                        else
                        {
                            sValue = (SChar*) (sField->getValue() +
                                     sArrayNum * sField->getValueSize());
                        }

                        sValueLen  = (UInt)(((utAtbField *)sField)->getValueInd())[sArrayNum];

                        if ( sValueLen != SQL_NULL_DATA )
                        {
                            switch( sSqlType )
                            {
                                case SQL_CHAR:
                                case SQL_VARCHAR:
                                case SQL_BIT:
                                case SQL_VARBIT:
                                case SQL_TINYINT:
                                case SQL_SMALLINT:
                                case SQL_INTEGER:
                                case SQL_BIGINT:
                                case SQL_REAL:
                                case SQL_DOUBLE:
                                case SQL_BINARY:
                                case SQL_VARBINARY:
                                case SQL_LONGVARBINARY:
                                case SQL_DATE:
                                case SQL_TIME:
                                case SQL_TIMESTAMP:
                                case SQL_TYPE_TIMESTAMP:
                                case SQL_GUID:
                                case SQL_GEOMETRY :
                                    // need traslation
                                    IDE_TEST_RAISE( utaCSVWrite (sValue, sValueLen, sFile )
                                                    != IDE_SUCCESS, ERR_WRITE_FILE);
                                    break;
                                default:
                                    // no translation
                                    IDE_TEST_RAISE( idlOS::fwrite( sValue, sValueLen, 1, sFile )
                                                    != (UInt)1, ERR_WRITE_FILE );
                                    break;
                            }
                        }
                        break;
                }

                if ( i == sFiledsCount )
                {   // �� row�� ������ column�ϰ��.
                    IDE_TEST_RAISE( idlOS::fwrite( &mRowTerm, 1, 1, sFile ) != (UInt)1,
                                    ERR_WRITE_FILE );
                }
                else
                {
                    IDE_TEST_RAISE( idlOS::fwrite( &mFieldTerm, 1, 1, sFile ) != (UInt)1,
                                    ERR_WRITE_FILE );
                }
            }
        }
    }

    /* 3. close cursor */
    IDE_TEST_RAISE( sQuery->utaCloseCur() != IDE_SUCCESS, ERR_CUR_CLOSE );

    return 0;

    IDE_EXCEPTION(ERR_WRITE_FILE);
    {
        uteSetErrorCode( &gErrorMgr, utERR_ABORT_AUDIT_File_Write_4CSVfile,
                         sFilename );
        uteSprintfErrorCode(sError, ERROR_BUFSIZE, &gErrorMgr);
        mProp->log("FATAL[ TASK ] Process failure! [SCANER]: %s\n", sError);

        idlOS::exit(1);
    }
    IDE_EXCEPTION(ERR_CUR_CLOSE);
    {
        uteSetErrorCode( &gErrorMgr, utERR_ABORT_AUDIT_Cursor_Close );
        uteSprintfErrorCode(sError, ERROR_BUFSIZE, &gErrorMgr);
        mProp->log("FATAL[ TASK ] Process failure! [SCANER]: %s\n", sError);

        idlOS::exit(1);
    }
    IDE_EXCEPTION_END;

    return 0;
}

IDE_RC utScanner::bindColumns()
{
    UShort     i;
    SInt sqlType = SQL_VARCHAR;
    Query *sQuery = mSelectA;

    if ( mSelectA->getDbType() == DBA_ATB )
    {
        sQuery = mSelectA;
    }
    else if ( mSelectB->getDbType() == DBA_ATB )
    {
        sQuery = mSelectB;
    }
    for( i = 1; i <= mFiledsCount; i++ )
    {
        sqlType = sQuery->getSQLType(i);
        IDE_TEST( mSelectA->bindColumn(i, sqlType ) != IDE_SUCCESS);
        IDE_TEST( mSelectB->bindColumn(i, sqlType ) != IDE_SUCCESS);
    }
    return IDE_SUCCESS;
    IDE_EXCEPTION_END;
    return IDE_FAILURE;
}


IDE_RC utScanner::setTable( utTableProp * aTab)
{
    SChar  fullLogName[512];

    metaColumns *sMeta = NULL;

    IDE_TEST( mConnA == NULL );
    IDE_TEST( mConnB == NULL );

    IDE_TEST( aTab == NULL );

    mTableNameA = (SChar*)aTab->master;
    mSchemaA    = mConnA->getSchema();

    if(aTab->slave == NULL )
    {
        aTab->slave = aTab->master;
    }
    if(aTab->schema == NULL)
    {
        aTab->schema = mConnB->getSchema();
    }
    mTableNameB = aTab->slave;
    mSchemaB = aTab->schema;

    if( mMeta )
    {
        mConnA->delMetaColumns(mMeta);
        mMeta = NULL;
    }

    // ** 1. get Master Server MetatDate ** //
    mMeta = mConnA->getMetaColumns((SChar*) mTableNameA);
    IDE_TEST( mMeta == NULL );


    // ** 2. get Slave  Server MetatDate ** //
    sMeta = mConnB->getMetaColumns((SChar*) mTableNameB,aTab->schema);

    /* for DEBUG sMeta->dump(); mMeta->dump(); */

    // ** 3. check up the compatibility ** //
    if( sMeta != NULL )
    {
        if(utProperties::mVerbose)
        {
            idlOS::fprintf(stderr,"PK is %d/%d\n",
                           mMeta->getPKSize(),
                           sMeta->getPKSize() );
        }

        /* BUG-43519 Checking if the table exists */
        IDE_TEST_RAISE( mMeta->getColumnCount() == 0, err_no_master_table );
        IDE_TEST_RAISE( sMeta->getColumnCount() == 0, err_no_slave_table );

        IDE_TEST_RAISE( mMeta->getPKSize() < 1                   , err_pk_no   );
        IDE_TEST_RAISE( mMeta->getPKSize() !=  sMeta->getPKSize(), err_pk_size );
        IDE_TEST_RAISE( mMeta->asc()       !=  sMeta->asc()      , err_pk_order );
        mConnB->delMetaColumns( sMeta );
    }

    // ** 4. exclusion column list setting */
    IDE_TEST( exclude(aTab->exclude) != IDE_SUCCESS );

    mPKCount = mMeta->getPKSize();
    diffColumn.name = mMeta->getPK(1);
    
    /*
    if(isSpecialCharacter((SChar *)mTableNameA)  ||
       isSpecialCharacter((SChar *)mTableNameB)  ||
       isSpecialCharacter((SChar *)aTab->schema) ||
       (idlOS::strcasecmp(mTableNameA, "\"TABLE\"") == 0) ||
       (idlOS::strcasecmp(mTableNameB, "\"TABLE\"") == 0) ||
       (idlOS::strcasecmp(aTab->schema, "\"USER\"") == 0))
    {
        idlOS::sprintf(fullLogName, "%s/TABLE-%d.log", mLogDir,aTab->mTabNo);
    }

    else
    {
        idlOS::sprintf(fullLogName, "%s/%s-%s.%s.log", mLogDir, mTableNameA
                       , aTab->schema, mTableNameB);
    }
    */
    generateLogName(fullLogName, mLogDir, mTableNameA, aTab->schema, mTableNameB);

    flog = idlOS::fopen(fullLogName, "w+");
    IDE_TEST_RAISE( flog == NULL, err_fopen );

    ordASC = mMeta->asc();

    /* set condition by WHERE */
    mQueryCond = aTab->where;

    return IDE_SUCCESS;
    IDE_EXCEPTION( err_fopen );
    {
        if(_error)
        {
            uteSetErrorCode(&gErrorMgr, utERR_ABORT_AUDIT_DIFF_File_Open_Error,
                            (SChar*)PRODUCT_PREFIX"audit", fullLogName);
            uteSprintfErrorCode(_error, ERROR_BUFSIZE, &gErrorMgr);
        }
    }

    IDE_EXCEPTION( err_pk_order  );
    {
        if(_error)
        {
            uteSetErrorCode(&gErrorMgr, utERR_ABORT_AUDIT_PK_Different_Order_Error,
                            mTableNameA,mTableNameB);
            uteSprintfErrorCode(_error, ERROR_BUFSIZE, &gErrorMgr);
        }
    }
    IDE_EXCEPTION( err_no_master_table );
    {
        if(_error)
        {
            uteSetErrorCode(&gErrorMgr, utERR_ABORT_Table_Not_Found_Error,
                            mSchemaA, mTableNameA, "master");
            uteSprintfErrorCode(_error, ERROR_BUFSIZE, &gErrorMgr);
        }
    }
    IDE_EXCEPTION( err_no_slave_table );
    {
        if(_error)
        {
            uteSetErrorCode(&gErrorMgr, utERR_ABORT_Table_Not_Found_Error,
                            mSchemaB, mTableNameB, "slave");
            uteSprintfErrorCode(_error, ERROR_BUFSIZE, &gErrorMgr);
        }
    }
    IDE_EXCEPTION( err_pk_no     );
    {
        if(_error)
        {
            uteSetErrorCode(&gErrorMgr, utERR_ABORT_AUDIT_Master_No_PK_Error,
                            mTableNameA,mTableNameB);
            uteSprintfErrorCode(_error, ERROR_BUFSIZE, &gErrorMgr);
        }
    }
    IDE_EXCEPTION( err_pk_size   );
    {
        if(_error)
        {
            uteSetErrorCode(&gErrorMgr, utERR_ABORT_AUDIT_Different_PK_Columns_Error,
                            mTableNameA,mTableNameB);
            uteSprintfErrorCode(_error, ERROR_BUFSIZE, &gErrorMgr);
        }
    }
    IDE_EXCEPTION_END;
    return IDE_FAILURE;
}

//BUG-24467 : log���ϸ���  ���̺�A-�����̸�.���̺�B.log�� ����
/* BUG-39623 object ���� ������ 128�� �þ�鼭 ���� �̸��� �ʹ� �����
* ���� �̸��� �ִ� ���� ���� => fat system:226bytes, ext system:255bytes
* So, [master_tbl] + [slave_usr] + [slave_tbl] = 210 bytes�� �����ϰ�
* ������ ���̸� 70 bytes�� ������ */
void utScanner::generateLogName(SChar              *aFullLogName,
                                const SChar        *aLogDir,
                                const SChar        *aTableNameA,
                                const SChar        *aSchemaB,
                                const SChar        *aTableNameB)
{
    SChar  sTmp[128];
    UInt sLen1 = 0;
    UInt sLen2 = 0;
    UInt sLen3 = 0;    

    sLen1 = idlOS::strlen(aTableNameA);
    sLen2 = idlOS::strlen(aSchemaB);
    sLen3 = idlOS::strlen(aTableNameB);
    if (sLen1 + sLen2 + sLen3 > 210)
    {
        idlOS::sprintf(aFullLogName, "%s"IDL_FILE_SEPARATORS, aLogDir);
        if (sLen1 > 70)
        {
            idlOS::strncpy(sTmp, aTableNameA, 70);
            sTmp[70] = '\0';
            idlOS::strcat(aFullLogName, sTmp);
        }
        else
        {
            idlOS::strcat(aFullLogName, aTableNameA);
        }
        idlOS::strcat(aFullLogName, "-");
        if (sLen2 > 70)
        {
            idlOS::strncpy(sTmp, aSchemaB, 70);
            sTmp[70] = '\0';
            idlOS::strcat(aFullLogName, sTmp);
        }
        else
        {
            idlOS::strcat(aFullLogName, aSchemaB);
        }
        idlOS::strcat(aFullLogName, ".");
        if (sLen3 > 70)
        {
            idlOS::strncpy(sTmp, aTableNameB, 70);
            sTmp[70] = '\0';
            idlOS::strcat(aFullLogName, sTmp);
        }
        else
        {
            idlOS::strcat(aFullLogName, aTableNameB);
        }
        idlOS::strcat(aFullLogName, ".log");
    }
    else
    {
        idlOS::sprintf(aFullLogName, "%s"IDL_FILE_SEPARATORS"%s-%s.%s.log", aLogDir, aTableNameA,
                       aSchemaB, aTableNameB);
    }
}

IDE_RC utScanner::exclude(SChar *s)
{
    UShort      i;
    SChar col[128 + 1] = {0};
    SChar *index;
    bool sIsLobType = false;
    const mtlModule * sDefaultModule = mtlDefaultModule();

    if(s)
        while(*s)
        {
            while(*s == ' ' || *s == ',' || *s == ';' )         s++;
            // PRJ-1678 : For multi-byte character set strings
            for(i = 0;(i < 128) && *s && (*s != ' ')&&(*s != ',')&&(*s != ';'); s++)
            {
                col[i++] = *s;
            }
            col[i] = '\0';

            // PRJ-1678 : For multi-byte character set string
            for(index = col; *index != 0;)
            {
                // BUG-17517
                if(col[0] != '\"')
                {
                    *index = idlOS::idlOS_toupper(*index);
                }
                // bug-21949: nextChar error check
/*                IDE_TEST(sDefaultModule->nextChar((UChar**)&index,
                            (UChar*)(&col[i])) != IDE_SUCCESS);
*/
                  sDefaultModule->nextCharPtr((UChar**)&index,
                                              (UChar*)(&col[i]));
            }

            if(index != col)
            {
                IDE_TEST_RAISE( mMeta->isPrimaryKey(col) ,err_pk);
                IDE_TEST_RAISE( mMeta->delCL(col, sIsLobType) != IDE_SUCCESS,err_cl);
            }
            else
            {
                break;
            }
        }//while

    return IDE_SUCCESS;
    IDE_EXCEPTION( err_pk );
    {
        if(_error)
        {
            uteSetErrorCode(&gErrorMgr, utERR_ABORT_AUDIT_PK_CanNotBe_EXCLUDE_Error, col);
            uteSprintfErrorCode(_error, ERROR_BUFSIZE, &gErrorMgr);
        }
    }
    IDE_EXCEPTION( err_cl );
    {
        if(_error)
        {
            uteSetErrorCode(&gErrorMgr,
                            utERR_ABORT_AUDIT_NOT_Fount_Column_EXCLUDE_Error, col);
            uteSprintfErrorCode(_error, ERROR_BUFSIZE, &gErrorMgr);
        }
    }
    IDE_EXCEPTION_END;
    return IDE_FAILURE;
}

IDE_RC utScanner::setModeDIFF(bool on)
{
    if(on)
    {
        setReport(true);
    }
    else
    {
    }
    return IDE_SUCCESS;
}

IDE_RC utScanner::initialize(Connection * aConnA,
                             Connection * aConnB,
                             SInt aCountToCommit,
                             SChar* errorBuffer)
{
    mDiffCount =  0;
    mPKCount   =  0;

    mSI = mSD = mSU = mMI =  NULL;


    ordASC   = true;
    un_idx   = true;

    flog  = stdout;
    _error = errorBuffer;
    _limit = ID_UINT_MAX;

    diffColumn.name    = NULL;

    IDE_TEST( setModeDIFF(false) != IDE_SUCCESS);

    IDE_TEST( aConnA == NULL );
    mConnA = aConnA;


    IDE_TEST( aConnB == NULL );
    mConnB = aConnB;

    mSelectA = aConnA->query();
    IDE_TEST( mSelectA == NULL );

    mSelectB = aConnB->query();
    IDE_TEST( mSelectB == NULL );
    
    IDE_TEST(Object::initialize() != IDE_SUCCESS );

    if( aCountToCommit > 0)
    {
        mCountToCommit = aCountToCommit;
        IDE_TEST( mConnA->autocommit(false) != IDE_SUCCESS);
        IDE_TEST( mConnB->autocommit(false) != IDE_SUCCESS);
    }
    else
    {
        mCountToCommit = 100;
        mConnA->autocommit(false);
        mConnB->autocommit(false);
    }

    // BUG-17951
    mMOSODiffCount = 0;
    mMXSODiffCount = 0;
    mMOSXDiffCount = 0;

    /* TASK-4212: audit���� ��뷮 ó���� ���� */
    mProp = NULL;
    mCSVNextToken = T_INIT;
    mIsFileMode   = false;

    mMasterFile.mFile    = NULL;
    mMasterFile.mName[0] = '\0';
    idlOS::memset( mMasterFile.mBuffer, 0x00, MAX_FILE_BUF );
    mMasterFile.mOffset  = -1;
    mMasterFile.mFence   = 0;

    mSlaveFile.mFile    = NULL;
    mSlaveFile.mName[0] = '\0';
    idlOS::memset( mSlaveFile.mBuffer, 0x00, MAX_FILE_BUF );
    mSlaveFile.mOffset  = -1;
    mSlaveFile.mFence   = 0;

    return IDE_SUCCESS;
    IDE_EXCEPTION_END;
    return IDE_FAILURE;
}

IDE_RC utScanner::finalize()
{
    if(mSI != NULL)
    {
        delete mSI;
        mSI = NULL;
    }
    if( mSU != NULL )
    {
        delete mSU;
        mSU = NULL;
    }
    if( mSD != NULL )
    {
        delete mSD;
        mSD = NULL;
    }
    if( mMI != NULL )
    {
        delete mMI;
        mMI = NULL;
    }
    return IDE_SUCCESS;
}

IDE_RC utScanner::select_sql(SChar *sql, const SChar* aSchemaName, const SChar * aTableName )
{
    UShort  i;
    SChar  *s;
    bool sIsLobType = false; // common column type:0, lob column type:1

    IDE_TEST(aTableName == NULL);
    idlOS::strcpy( sql,"SELECT ");

    for( i = 1, s= mMeta->getPK(1); s ; s = mMeta->getPK(++i) )
    {
        if( i > 1 )
        {
            idlOS::strcat(sql,", ");
        }
        idlOS::strcat(sql,s);
    }

    for(i = 1, s= mMeta->getCL(1, sIsLobType); s; s= mMeta->getCL(++i, sIsLobType))
    {
        idlOS::strcat(sql,", ");
        idlOS::strcat(sql,s);
    }

    /* BUG-43455 Slave table not found error due to different user id from SCHEMA at altiComp.cfg */
    idlOS::strcat(sql,  " FROM ");
    idlOS::strcat(sql,aSchemaName);
    idlOS::strcat(sql,".");
    idlOS::strcat(sql,aTableName);
    
    if( mQueryCond )
    {
        idlOS::strcat(sql," WHERE " );
        idlOS::strcat(sql,mQueryCond);
    }

    if( mMeta->getPKSize() > 0 )
    {
        idlOS::strcat(sql,  " ORDER BY ");
        for( i = 1, s= mMeta->getPK(1); s ;  s= mMeta->getPK(++i) )
        {
            if( i > 1 )
            {
                idlOS::strcat(sql,", ");
            }
            idlOS::strcat(sql,s);
        }
    }

    if(utProperties::mVerbose)
    {
        idlOS::printf("\n%s\n",sql);
    }
    
    return IDE_SUCCESS;
    IDE_EXCEPTION_END;
    return IDE_FAILURE;
}


/* thread safe as single I/O operation */
IDE_RC utScanner::printResult(FILE* f)
{
    //  database operation maps for:>>MOSX<<   >>MXSO<<    >>MOSO<< //
    SChar *ps[4][2] = {{(SChar *)"-",(SChar *)"-"},
                       {(SChar *)"-",(SChar *)"-"},
                       {(SChar *)"-",(SChar *)"-"},
                       {(SChar *)"-",(SChar *)"-"}};

    UInt  didS = didSl();
    UInt  didM = didMa();
    UInt failS = failSl();
    UInt failM = failMa();

    IDE_TEST( f == NULL );

    switch(mMode)
    {
        case DIFF:
        case SYNC:
            if(mDML[SI]) { ps[MOSX][1] = (SChar *)"SI"; }
            if(mDML[SU]) { ps[MOSO][1] = (SChar *)"SU"; }
            if(mDML[SD]) { ps[MXSO][1] = (SChar *)"SD"; }
            if(mDML[MI]) { ps[MXSO][0] = (SChar *)"MI"; }
            break;

        default:
            break;
    }

    if(mMode == SYNC)
    {
        idlOS::fprintf(f,
                       "\n[%s->%s]\n"                         // 1, 2
                       "Fetch Rec In Master: %d\n"            // 3
                       "Fetch Rec In Slave : %d\n"            // 4
                       "MOSX = %2s,%2s\nMXSO = %2s,%2s\n"
                       "MOSO = %2s,%2s\nMXSX = %2s,%2s\n"     // 5, 6, 7
                       "\n--------------------------------------------\n"
                       " Operation  Type      MASTER           SLAVE    "
                       "\n--------------------------------------------\n"
                       " INSERT     Try   %10d      %10d \n"  // 8, 9
                       "            Fail  %10d      %10d \n"  // 10, 11
                       "\n"
                       " UPDATE     Try   %10d      %10d \n"  // 12, 13
                       "            Fail  %10d      %10d \n"  // 14, 15
                       "\n"
                       " DELETE     Try   %10d      %10d \n"  // 16, 17
                       "            Fail  %10d      %10d"     // 18, 19
                       "\n--------------------------------------------\n"
                       " UPDATE     Try   %10d      %10d \n"  // 20, 21
                       "            Fail  %10d      %10d \n"  // 22, 23
                       " OOP  TPS: %10.2f\n"                  // 24
                       " SCAN TPS: %10.2f\n"                  // 25
                       "     Time: %10.2f sec\n"              // 26
                       ,mTableNameA,mTableNameB
                       ,mSelectA->rrows()
                       ,mSelectB->rrows()

                       ,ps[0][0],ps[0][1],ps[1][0],ps[1][1]
                       ,ps[2][0],ps[2][1],ps[3][0],ps[3][1]   // 5,6,7

                       ,                 did(mMI),   did(mSI) // 8, 9
                       ,                fail(mMI),  fail(mSI) // 10, 11

                       ,                        0,   did(mSU) // 12, 13
                       ,                        0,  fail(mSU) // 14, 15

                       ,                        0,   did(mSD) // 16, 17
                       ,                        0,  fail(mSD) // 18, 19
                       ,                     didM,      didS  // 20, 21
                       ,                    failM,     failS  // 22, 23
                       ,((didM + didS)*1000000.0/(mTimeStamp))
                       ,((mSelectA->rows() > mSelectB->rows())
                        ? mSelectA->rows() : mSelectB->rows())*1000000.0/mTimeStamp
                       , mTimeStamp/1000000.0);               // 26
    }
    // BUG-17951
    else //if(mMode == DIFF)
    {
        idlOS::fprintf(f,
                       "\n[%s->%s]\n"                         // 1, 2
                       "Fetch Rec In Master: %d\n"            // 3
                       "Fetch Rec In Slave : %d\n"            // 4
                       "MOSX = DF, Count : %10d\n"            // 5
                       "MXSO = DF, Count : %10d\n"            // 6
                       "MOSO = DF, Count : %10d\n"            // 7
                       "MOSO = EQ, Count : %10d\n\n"          // 8
                       " SCAN TPS: %10.2f\n"                  // 9
                       "     Time: %10.2f sec\n"              // 10

                       ,mTableNameA,mTableNameB               // 1, 2
                       ,mSelectA->rrows()                     // 3
                       ,mSelectB->rrows()                     // 4

                       ,mMOSXDiffCount                        // 5
                       ,mMXSODiffCount                        // 6
                       ,mMOSODiffCount                        // 7
                       ,mMOSOSameCount                        // 8

                       ,((mSelectA->rows() > mSelectB->rows())
                        ? mSelectA->rows() : mSelectB->rows())*1000000.0/mTimeStamp //9
                       , mTimeStamp/1000000.0);               // 10
    }
    IDE_TEST( idlOS::fflush(f) < 0);

    return IDE_SUCCESS;
    IDE_EXCEPTION_END;
    return IDE_FAILURE;
}

SChar *utScanner::error()
{
    return _error;
}


utScanner::utScanner():Object()
{
    mConnA     =
        mConnB     = NULL;
    mSelectA   =
        mSelectB   = NULL;
    mRowA      =
        mRowB      = NULL;
    mMeta      = NULL;

    mTableNameA=
        mTableNameB= NULL;

    mQueryCond = NULL;
    _error     = NULL;
    mLogDir    = (SChar*)".";
}

ULong utScanner::getTimeStamp()
{
    PDL_Time_Value sTimeValue  = idlOS::gettimeofday();
    return ( (ULong)sTimeValue.sec() * 1000000 + sTimeValue.usec() );
}

IDE_RC utScanner::setSI()
{
    Query * sQuery = NULL;

    // BUG-17176
    if(mSI != NULL)
    {
        delete mSI;
        mSI = NULL;
    }

    sQuery = mConnB->query();
    IDE_TEST( sQuery == NULL);

    mSI = new dmlQuery();
    IDE_TEST(mSI == NULL);

    IDE_TEST( mSI->initialize(sQuery,'S','I',
              mSchemaB, mTableNameB, mMeta ) != IDE_SUCCESS);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if( mSI != NULL )
    {
        delete mSI;
        mSI = NULL;
    }

    if(sQuery != NULL)
    {
        delete sQuery;
    }
    return IDE_FAILURE;
}

IDE_RC utScanner::setSU()
{
    Query * sQuery = NULL;

    // BUG-17176
    if( mSU != NULL )
    {
        delete mSU;
        mSU = NULL;
    }

    sQuery = mConnB->query();
    IDE_TEST( sQuery == NULL);

    mSU = new dmlQuery();
    IDE_TEST(mSU == NULL);

    IDE_TEST( mSU->initialize(sQuery,'S','U',
              mSchemaB, mTableNameB, mMeta ) != IDE_SUCCESS);


    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if( mSU != NULL )
    {
        delete mSU;
        mSU = NULL;
    }

    if(sQuery != NULL)
    {
        delete sQuery;
    }
    return IDE_FAILURE;
}

IDE_RC utScanner::setSD()
{
    Query * sQuery = NULL;

    // BUG-17176
    if( mSD != NULL )
    {
        delete mSD;
        mSD = NULL;
    }

    sQuery = mConnB->query();
    IDE_TEST( sQuery == NULL);

    mSD = new dmlQuery();
    IDE_TEST(mSD == NULL);

    IDE_TEST( mSD->initialize(sQuery,'S','D',
              mSchemaB, mTableNameB, mMeta ) != IDE_SUCCESS);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if( mSD != NULL )
    {
        delete mSD;
        mSD = NULL;
    }

    if(sQuery != NULL)
    {
        delete sQuery;
    }
    return IDE_FAILURE;
}

IDE_RC utScanner::setMI()
{
    Query * sQuery = NULL;

    // BUG-17176
    if( mMI != NULL )
    {
        delete mMI;
        mMI = NULL;
    }

    sQuery = mConnA->query();
    IDE_TEST( sQuery == NULL);

    mMI = new dmlQuery();
    IDE_TEST(mMI == NULL);

    IDE_TEST( mMI->initialize(sQuery,'M','I',
              mSchemaA, mTableNameA, mMeta )  != IDE_SUCCESS);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if( mMI != NULL )
    {
        delete mMI;
        mMI = NULL;
    }

    if(sQuery != NULL)
    {
        delete sQuery;
    }
    return IDE_FAILURE;
}

/* TASK-4212: audit���� ��뷮 ó���� ���� */
IDE_RC utScanner::utaCSVWrite ( SChar *aValue, UInt aValueLen, FILE *aWriteFp )
{
/***********************************************************************
 *
 * Description :
 *    aValueLen������ aValue string�� �޾� CSV format���·� ��������, ���Ͽ� ���� �Լ�.
 *
 ***********************************************************************/
    UInt  i;
    UInt  j;

    i = j = 0;

    /* " " is forced. */
    while ( i < aValueLen )
    {
        if( *(aValue + i) == mEnclosing )
        {
            if( i == 0 )
            {
                IDE_TEST_RAISE(idlOS::fwrite( &mEnclosing, 1, 1, aWriteFp ) 
                               != (UInt)1, WriteError);
            }
            else
            {
                if ( j == 0 )
                {
                    /* write heading quote, except for the i==0 case. */
                    IDE_TEST_RAISE(idlOS::fwrite( &mEnclosing, 1, 1, aWriteFp )
                                   != (UInt)1, WriteError);
                }
                IDE_TEST_RAISE( idlOS::fwrite( aValue + j, i - j, 1, aWriteFp )
                                != (UInt)1, WriteError);
                IDE_TEST_RAISE( idlOS::fwrite( &mEnclosing, 1, 1, aWriteFp )
                                != (UInt)1, WriteError);
            }
            j = i;
        }
        if( i == aValueLen - 1 )
        {
            if ( j == 0 )
            {
                IDE_TEST_RAISE( idlOS::fwrite( &mEnclosing, 1, 1, aWriteFp )
                                != (UInt)1, WriteError);
                IDE_TEST_RAISE( idlOS::fwrite( aValue, aValueLen, 1, aWriteFp )
                                != (UInt)1, WriteError);
                IDE_TEST_RAISE( idlOS::fwrite( &mEnclosing, 1, 1, aWriteFp )
                                != (UInt)1, WriteError);
            }
            else
            {
                IDE_TEST_RAISE( idlOS::fwrite( aValue + j, i - j + 1, 1, aWriteFp )
                                != (UInt)1, WriteError);
                IDE_TEST_RAISE( idlOS::fwrite( &mEnclosing, 1, 1, aWriteFp )
                                != (UInt)1, WriteError);
            }
        }
        i++;
    }
    return IDE_SUCCESS;

    IDE_EXCEPTION(WriteError);
    IDE_EXCEPTION_END;
    return IDE_FAILURE;
}

/* TASK-4212: audit���� ��뷮 ó���� ���� */
/* parse csv format data, and store the token value */

/* BUG-32569 The string with null character should be processed in Audit */
utaCSVTOKENTYPE utScanner::utaGetCSVTokenFromBuff( utaFileInfo *aFileInfo,
                                                   SChar       *aTokenBuff,
                                                   UInt         aTokenBuffLen,
                                                   UInt        *aTokenValueLength )
{
/***********************************************************************
 *
 * Description :
 *    parse csv format data, and convert to original data.
 *
 ***********************************************************************/

    SInt   sReadLen;
    SInt   sPartialLen;
    UInt   sTokenBuffIndex;
    SChar  sChr;
    UInt   sMaxTokenSize;
    idBool sInQuotes;
    utaCSVSTATE     sState;
    utaCSVTOKENTYPE sTmpToken;

    sState    = stStart;
    /* ��ū�� �ִ� ���� = VARBIT ���ڿ� ǥ���� �ִ� ���� */
    sMaxTokenSize = MAX_TOKEN_VALUE_LEN;
    sInQuotes   = ID_FALSE;
    sReadLen    = 0;
    sPartialLen = 0;
    sTokenBuffIndex = 0;

    // ������ �� ��ū�� �̹� �������־��ٸ�.
    if ( mCSVNextToken != T_INIT )
    {
        sTmpToken     = mCSVNextToken;
        mCSVNextToken = T_INIT;
        return sTmpToken;
    }

    // initialize token buffer
    if ( aTokenBuff != NULL )
    {
        idlOS::memset( aTokenBuff, 0x00, aTokenBuffLen );
    }

    while ( sMaxTokenSize )//*sReadLen != *sBufferIndex )
        //sMaxTokenSize && ( sReadresult = ReadDataFromCBuff(&sChr) > 0 ) )
    {

        /* 1. READ FROM FILE */
        // ���� file �κ��� �о�ð� �������� �ְ�, buffer �� 95%�� ó���ߴٸ� file�� ���� �� �б⸦ �õ��غ���.
        if ( (aFileInfo->mFence == MAX_FILE_BUF) &&
             (aFileInfo->mOffset > FILE_BUF_LIMIT) )
        {
            sPartialLen = aFileInfo->mFence - aFileInfo->mOffset;
            idlOS::memcpy( aFileInfo->mBuffer, aFileInfo->mBuffer + aFileInfo->mOffset, sPartialLen );
            sReadLen = idlOS::fread(aFileInfo->mBuffer + sPartialLen,
                                    1,
                                    MAX_FILE_BUF - sPartialLen,
                                    aFileInfo->mFile);

            if( sReadLen != MAX_FILE_BUF - sPartialLen )
            {
                IDE_TEST_RAISE( ferror(aFileInfo->mFile), ERR_READ_FILE );
            }

            aFileInfo->mFence = sPartialLen + sReadLen;
            aFileInfo->mOffset = 0;
        }
        else
        {
            // file�� ������ �о��ٸ�...
            if ( aFileInfo->mFence == aFileInfo->mOffset )
            {
                return T_EOF;
            }
        }

        /* 2. GET CHAR FROM BUFF */
        sChr = aFileInfo->mBuffer[(aFileInfo->mOffset)++];

        /* 3. CSV format -> original data format */
        switch ( sState )
        {
            /* stStart   : state of starting up reading a field  */
            case stStart :
                if ( sChr != '\n' && isspace(sChr) )
                {
                    break;
                }
                else if ( sChr == mEnclosing )
                {
                    sState = stCollect;
                    sInQuotes = ID_TRUE;
                    break;
                }
                sState = stCollect;
            /* state of in the middle of reading a field  */    
            case stCollect :
                if ( sInQuotes == ID_TRUE )
                {
                    if ( sChr == mEnclosing )
                    {
                        sState = stEndQuote;
                        break;
                    }
                }
                else if ( sChr == mFieldTerm )
                {
                    mCSVNextToken = T_FIELD_TERM;
                    if ( sTokenBuffIndex == 0 )
                    {
                        return T_NULL_VALUE;
                    }
                    else
                    {
                        return T_VALUE;
                    }
                }
                else if ( sChr == mRowTerm )
                {
                    mCSVNextToken = T_ROW_TERM;
                    if ( sTokenBuffIndex == 0 )
                    {
                        return T_NULL_VALUE;
                    }
                    else
                    {
                        return T_VALUE;
                    }
                }
                else if ( sChr == mEnclosing )
                {
                    /* CSV format is wrong, so state must be changed to stError  */
                    sState = stError;
                    break;
                }
                /* collect good(csv format) charcters */
                if ( aTokenBuff != NULL )
                {
                    IDE_TEST_RAISE( aTokenBuffLen <= sTokenBuffIndex,
                                    ERR_CSV_BUFF_OVERFLOW );
                    aTokenBuff[ sTokenBuffIndex++ ] = sChr;
                    sMaxTokenSize--;

                    /* BUG-32569 The string with null character should be processed in Audit */
                    if (aTokenValueLength != NULL)
                    {
                        (*aTokenValueLength)++;
                    }
                }
                break;
            /* at tailing spaces out of quotes */
            case stTailSpace :
            /* at tailing quote */
            case stEndQuote :
                /* In case of reading an escaped quote. */
                if ( sChr == mEnclosing && sState != stTailSpace )
                {
                    if ( aTokenBuff != NULL )
                    {
                        IDE_TEST_RAISE( aTokenBuffLen <= sTokenBuffIndex,
                                        ERR_CSV_BUFF_OVERFLOW );
                        aTokenBuff[ sTokenBuffIndex++ ] = sChr;
                        sMaxTokenSize--;
                        /* BUG-32569 The string with null character should be processed in Audit */
                        if (aTokenValueLength != NULL)
                        {
                            (*aTokenValueLength)++;
                        }
                    }
                    sState = stCollect;
                    break;
                }
                else if ( sChr == mFieldTerm )
                {
                    mCSVNextToken = T_FIELD_TERM;
                    return T_VALUE;
                }
                else if ( sChr == mRowTerm )
                {
                    mCSVNextToken = T_ROW_TERM;
                    return T_VALUE;
                }
                else if ( isspace(sChr) )
                {
                    sState = stTailSpace;
                    break;
                }

                sState = stError;
                break;
            /* state of a wrong csv format is read */
            case stError :
                if ( sChr == mFieldTerm )
                {
                    mCSVNextToken = T_FIELD_TERM;
                    IDE_RAISE( ERR_CSV_FORMAT );
                }
                else if ( sChr == mRowTerm )
                {
                    mCSVNextToken = T_ROW_TERM;
                    IDE_RAISE( ERR_CSV_FORMAT );
                }
                break;
        }
    }

    if ( sMaxTokenSize == 0 )
    {
        IDE_RAISE( ERR_CSV_BUFF_OVERFLOW );
    }

    IDE_RAISE( ERR_CSV_FORMAT );

    IDE_EXCEPTION( ERR_CSV_FORMAT );
    {
    }
    IDE_EXCEPTION( ERR_CSV_BUFF_OVERFLOW );
    {
        if( _error)
        {
            aTokenBuff[ aTokenBuffLen - 1 ] = '\0';
            uteSetErrorCode( &gErrorMgr, utERR_ABORT_AUDIT_CSV_Token_Buffer_Overflow,
                             aTokenBuff );
            uteSprintfErrorCode(_error, ERROR_BUFSIZE, &gErrorMgr);
            mProp->log("FATAL[ TASK ] Process failure! [SCANER]: %s\n",_error);
        }

        idlOS::exit(1);
    }
    IDE_EXCEPTION( ERR_READ_FILE )
    {
        if( _error)
        {
            uteSetErrorCode( &gErrorMgr, utERR_ABORT_AUDIT_File_Read_4CSVfile,
                             aFileInfo->mName );
            uteSprintfErrorCode(_error, ERROR_BUFSIZE, &gErrorMgr);
            mProp->log("FATAL[ TASK ] Process failure! [SCANER]: %s\n",_error);
        }

        idlOS::exit(1);
    }
    IDE_EXCEPTION_END;

    return T_ERR;
}
