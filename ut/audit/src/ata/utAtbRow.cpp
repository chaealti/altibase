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
 * $Id: utAtbRow.cpp 82705 2018-04-05 01:31:36Z bethy $
 ******************************************************************************/

#include <utAtb.h>

utAtbRow::utAtbRow(utAtbQuery * aQuery, SInt &aErrNo) : Row(aErrNo),_stmt(aQuery->_stmt)
{
    mQuery  = aQuery;
    mCount  = 0;
    mField  = NULL;
}

IDE_RC utAtbRow::initialize()
{
    UShort i;
    utAtbField *sField = NULL;
    mCount = mQuery->_cols;

    if( mCount == 0 )
        return IDE_SUCCESS;

    IDE_TEST( Row::initialize() != IDE_SUCCESS );

    for(i = 0; i < mCount; i++)
    {
        //*** 1. allocate utAtbField  ***//
        sField = new utAtbField();
        IDE_TEST(sField == NULL);
        //*** 3. init memory for data    ***//
        IDE_TEST_RAISE(sField->initialize( i+1, this ) != IDE_SUCCESS, init_error);
        if(mField[0])
        {
            mField[0]->add(sField);
        }
        mField[i]  = sField;
    }

    return IDE_SUCCESS;

    // BUG-25229 [CodeSonar] Audit�� �޸� Leak
    IDE_EXCEPTION(init_error);
    {
        delete sField;
    }

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*
 * BUG-45909 Improve LOB Processing
 */
IDE_RC utAtbRow::reset()
{
    int    i;

    for( i = 0; i < mCount; i++ )
    {
        mField[i]->finiLob();
    }
    return Row::reset();
}

/* TASK-4212: audit���� ��뷮 ó���� ���� */
IDE_RC utAtbRow::setStmtAttr4Array(void)
{
/***********************************************************************
 *
 * Description :
 *    CSV FILE_MODE �� array fetch �� �ϱ����� array size setting.
 *
 ***********************************************************************/
    SQLRETURN sSqlRes;

    if( mArrayCount > 1 )
    {
        if( mRowStatusArray != NULL )
        {
            idlOS::free( mRowStatusArray );
            mRowStatusArray = NULL;
        }

        // BUG-33945 Codesonar warning - 189587.1380136
        IDE_ASSERT( mArrayCount * ID_SIZEOF(SQLUSMALLINT) < ID_vULONG_MAX );

        mRowStatusArray = (SQLUSMALLINT *)idlOS::malloc(mArrayCount *
                ID_SIZEOF(SQLUSMALLINT));

        sSqlRes = SQLSetStmtAttr ( _stmt,
                                   SQL_ATTR_ROW_BIND_TYPE,
                                   SQL_BIND_BY_COLUMN,
                                   0 );
        IDE_TEST(sSqlRes != SQL_SUCCESS);

        sSqlRes = SQLSetStmtAttr( _stmt,
                                  SQL_ATTR_ROW_ARRAY_SIZE,
                                  (SQLPOINTER)(vULong) mArrayCount,
                                  0);
        IDE_TEST(sSqlRes != SQL_SUCCESS);

        sSqlRes = SQLSetStmtAttr( _stmt,
                                  SQL_ATTR_ROWS_FETCHED_PTR,
                                  &mRowsFetched,
                                  0);
        IDE_TEST(sSqlRes != SQL_SUCCESS);

        sSqlRes = SQLSetStmtAttr( _stmt,
                                  SQL_ATTR_ROW_STATUS_PTR,
                                  mRowStatusArray,
                                  0);
        IDE_TEST(sSqlRes != SQL_SUCCESS);

    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}
