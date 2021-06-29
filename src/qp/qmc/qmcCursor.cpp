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
 * Copyright 1999-2000, RTBase Corporation or its subsidiaries.
 * All rights reserved.
 **********************************************************************/

/***********************************************************************
 * $Id: qmcCursor.cpp 88285 2020-08-05 05:49:37Z bethy $
 **********************************************************************/

#include <idl.h>
#include <ide.h>
#include <qmcCursor.h>
#include <qtc.h>

IDE_RC
qmcCursor::init( iduMemory * aMemory )
{
    mMemory = NULL;
    mTop = NULL;
    mCurrent = NULL;

    // BUG-40427 Opened LOB Cursor �ʱ�ȭ
    mOpenedLob = MTD_LOCATOR_NULL;

    //---------------------------------
    // table cursor�� ����� memroy
    //---------------------------------
    IDU_FIT_POINT( "qmcCursor::init::alloc::mMemory",
                    idERR_ABORT_InsufficientMemory );

    IDE_TEST(aMemory->alloc( idlOS::align8((UInt)ID_SIZEOF(qmcMemory)) ,
                             (void**)&mMemory)
             != IDE_SUCCESS);

    mMemory = new (mMemory)qmcMemory();

    /* BUG-38290 */
    mMemory->init( idlOS::align8(ID_SIZEOF(qmcOpenedCursor)) );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmcCursor::addOpenedCursor( iduMemory      * aMemory,
                                   UShort           aTableID,
                                   smiTableCursor * aCursor )
{
    qmcOpenedCursor * sCursor;

    /* BUG-38290 */
    IDU_FIT_POINT( "qmcCursor::addOpenedCursor::alloc::sCursor",
                    idERR_ABORT_InsufficientMemory );

    IDE_TEST(mMemory->alloc(
            aMemory,
            idlOS::align8(ID_SIZEOF(qmcOpenedCursor)),
            (void**)&sCursor)
        != IDE_SUCCESS);

    sCursor->tableID= aTableID;
    sCursor->cursor = aCursor;
    sCursor->next = NULL;

    if ( mTop == NULL )
    {
        mTop = mCurrent = sCursor;
    }
    else
    {
        mCurrent->next = sCursor;
        mCurrent = sCursor;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    // To fix BUG-14911
    // memory alloc �����ϸ� openedCursor�� ������ �� ���� ������
    // cursor�� close�� �� ����.
    (void)aCursor->close();

    return IDE_FAILURE;
}

IDE_RC
qmcCursor::getOpenedCursor( UShort aTableID, smiTableCursor ** aCursor, idBool * aFound )
{
    qmcOpenedCursor *sQmcCursor;

    *aCursor = NULL;
    *aFound = ID_FALSE;

    if ( mTop != NULL )
    {
        sQmcCursor = mTop;

        while ( sQmcCursor != NULL )
        {
            if ( sQmcCursor->tableID == aTableID )
            {
                *aCursor = sQmcCursor->cursor;
                *aFound = ID_TRUE;
                break;
            }
            else
            {
                sQmcCursor = sQmcCursor->next;
            }
        }
    }
    else
    {
        // Nothing to do.
    }

    return IDE_SUCCESS;
}

IDE_RC qmcCursor::addOpenedLobCursor( smLobLocator aLocator )
{
/***********************************************************************
 *
 * Description : BUG-40427
 *
 * Implementation :
 *
 *    qmcCursor�� open�� lob locator 1���� ����Ѵ�.
 *    ���ʷ� open�� lob locator 1���� �ʿ��ϹǷ�, 
 *    �̹� ��ϵ� ��� qmcCursor������ �ƹ� �ϵ� ���� �ʴ´�.
 *
 ***********************************************************************/

    if ( mOpenedLob == MTD_LOCATOR_NULL )
    {
        mOpenedLob = aLocator;
    }
    else
    {
        // Nothing to do.
    }

    return IDE_SUCCESS;
}

IDE_RC
qmcCursor::closeAllCursor( idvSQL * aStatistics )
{
    qmcOpenedCursor    * sCursor            = NULL;
    idBool               sIsCloseCusror     = ID_FALSE;
    UInt                 sClientLocatorInfo = MTC_LOB_LOCATOR_CLIENT_TRUE;
    smLobLocator         sOpenedLob;
 
    /* BUG-40427
     * ���������� ����ϴ� LOB Cursor �ϳ��� closeLobCursor() �� ȣ���ϴ� �����, 
     * SM Module�� �ִ� LOB Cursor List������ Ž�� ����� ������� ���� ����̴�.
     * �׷��� CLIENT_TRUE �� ������ ���� LOB Cursor�� List�� ���󰡸� ��� �ݵ��� �Ѵ�.
     * ��, �̹� open �� LOB Cursor�� �����ϴ� ��쿡 ���ؼ� ȣ���Ѵ�. */

    if ( mOpenedLob != MTD_LOCATOR_NULL )
    {
        // smiLob::closeAllLobCursors()�� �����ϴ� ��� 
        // mOpenedLob�� �ʱ�ȭ���� �����Ƿ�, sOpenedLob���� ���� �ѱ�� �̸� �ʱ�ȭ�Ѵ�.
        sOpenedLob = mOpenedLob;
        mOpenedLob = MTD_LOCATOR_NULL;

        (void) smiLob::closeAllLobCursors( aStatistics, sOpenedLob, sClientLocatorInfo );
    }
    else
    {
        // Nothing to do.
    }

    //---------------------------------
    // table cursor�� close
    //---------------------------------

    for ( sCursor = mTop; sCursor != NULL; sCursor = sCursor->next )
    {
        IDE_TEST( sCursor->cursor->close() != IDE_SUCCESS );
    }

    sIsCloseCusror = ID_TRUE;

    if ( mMemory != NULL )
    {
        // To fix BUG-17591
        // closeAllCursor�� ȣ��Ǵ� ������
        // qmcMemory���� ����ϴ� qmxMemory�� ���� ���� �������� ���ư���
        // �����̹Ƿ�, qmcMemory::clear�� ����Ѵ�.
        mMemory->clear( idlOS::align8((UInt)ID_SIZEOF(qmcOpenedCursor) ) );
    }

    mTop = mCurrent = NULL;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    IDE_PUSH();

    if ( sIsCloseCusror == ID_FALSE )
    {
        if ( sCursor == NULL )
        {
            sCursor = mTop;
        }
        else
        {
            sCursor = sCursor->next;
        }

        for ( ; sCursor != NULL; sCursor = sCursor->next )
        {
            (void) sCursor->cursor->close();
        }

        if ( mMemory != NULL )
        {
            // To fix BUG-17591
            // closeAllCursor�� ȣ��Ǵ� ������
            // qmcMemory���� ����ϴ� qmxMemory�� ���� ���� �������� ���ư���
            // �����̹Ƿ�, qmcMemory::clear�� ����Ѵ�.
            mMemory->clear( idlOS::align8((UInt)ID_SIZEOF(qmcOpenedCursor) ) );
        }
        else
        {
            // Nothing to do.
        }

        mTop = mCurrent = NULL;
    }
    else
    {
        // Nothing to do.
    }

    IDE_POP();

    return IDE_FAILURE;
}
