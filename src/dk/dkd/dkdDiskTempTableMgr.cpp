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
 * $id$
 **********************************************************************/

#include <dkdDiskTempTableMgr.h>


/***********************************************************************
 * Description: Disk temp table manager �� �ʱ�ȭ�Ѵ�.
 *              ���������� dkdDiskTempTable.cpp �� �Լ����� ȣ���Ѵ�.
 *
 **********************************************************************/
IDE_RC  dkdDiskTempTableMgr::initialize( void       *aQcStatement,
                                         mtcColumn  *aColMetaArr,
                                         UInt        aColCnt )
{
    mIsEndOfFetch   = ID_FALSE;
    mIsCursorOpened = ID_FALSE;
    mDiskTempTableH = NULL;

    IDE_TEST( dkdDiskTempTableCreate( aQcStatement, 
                                      aColMetaArr, 
                                      aColCnt, 
                                      &mDiskTempTableH )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/************************************************************************
 * Description : Disk temp table manager �� �����Ѵ�. 
 *               ���������� dkdDiskTempTableDrop �� ȣ���Ͽ� �����Ͽ�
 *               ���� �ִ� disk temp table �� drop ���ش�. 
 *
 ************************************************************************/
IDE_RC  dkdDiskTempTableMgr::finalize()
{
    if ( mDiskTempTableH != NULL )
    {
        IDE_TEST( dkdDiskTempTableDrop( mDiskTempTableH ) != IDE_SUCCESS );
        mDiskTempTableH = NULL;
    }
    else
    {
        /* do nothing */
    }
 
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/************************************************************************
 * Description : Disk temp table �κ��� record �ϳ��� fetch �ؿ´�. 
 *
 *  aRow        - [OUT] fetch �ؿ� record �� ����Ű�� ������
 *
 ************************************************************************/
IDE_RC  dkdDiskTempTableMgr::fetchRow( void **aRow )
{
    if ( mIsCursorOpened != ID_TRUE )
    {
        IDE_TEST( dkdDiskTempTableOpenCursor( mDiskTempTableH )
                  != IDE_SUCCESS );

        mIsCursorOpened = ID_TRUE;
    }
    else
    {
        /* already cursor opened */
    }

    IDE_TEST( dkdDiskTempTableFetchRow( mDiskTempTableH, aRow ) 
              != IDE_SUCCESS );

    if ( aRow == NULL )
    {
        mIsEndOfFetch = ID_TRUE;
    }
    else
    {
        /* success */
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/************************************************************************
 * Description : Record buffer Ȥ�� disk temp table �� record �ϳ��� 
 *               insert �Ѵ�. 
 *
 *  aRecord     - [IN] insert �� record �� ����Ű�� ������
 *
 ************************************************************************/
IDE_RC  dkdDiskTempTableMgr::insertRow( dkdRecord   *aRecord )
{
    IDE_TEST( dkdDiskTempTableInsertRow( mDiskTempTableH, aRecord->mData ) 
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/************************************************************************
 * Description : Record buffer Ȥ�� disk temp table �� cursor �� restart
 *               ��Ų��.
 *
 ************************************************************************/
IDE_RC  dkdDiskTempTableMgr::restart()
{
    if ( mIsCursorOpened != ID_TRUE )
    {
        IDE_TEST( dkdDiskTempTableOpenCursor( mDiskTempTableH ) 
                  != IDE_SUCCESS );

        mIsCursorOpened = ID_TRUE;
    }
    else
    {
        /* already cursor opened */
    }

    IDE_TEST( dkdDiskTempTableRestartCursor( mDiskTempTableH ) 
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC  fetchRowFromDiskTempTable( void *aMgrHandle, void **aRow )
{
    IDE_TEST( ((dkdDiskTempTableMgr *)aMgrHandle)->fetchRow( aRow ) 
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC  insertRowIntoDiskTempTable( void  *aMgrHandle, dkdRecord *aRecord )
{
    IDE_TEST( ((dkdDiskTempTableMgr *)aMgrHandle)->insertRow( aRecord ) 
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC  restartDiskTempTable( void  *aMgrHandle )
{
    IDE_TEST( ((dkdDiskTempTableMgr *)aMgrHandle)->restart() 
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

