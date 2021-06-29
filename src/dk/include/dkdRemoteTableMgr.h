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

#ifndef _O_DKD_REMOTE_TABLE_MGR_H_
#define _O_DKD_REMOTE_TABLE_MGR_H_ 1


#include <dkd.h>
#include <dkdTypeConverter.h>
#include <dkpDef.h>


class dkdRemoteTableMgr
{
private:
    /* ���ݼ����κ��� ��� ���ڵ带 �� �����Դ��� ���� */
    idBool                  mIsEndOfFetch;
    /* fetch row buffer �� ���ڵ���� �� �о���� ���� */
    idBool                  mIsFetchBufReadAll;
    /* fetch row buffer �� �� á���� ���� */
    idBool                  mIsFetchBufFull;
    /* Converted Row's size */
    UInt                    mRowSize;
    /* Fetch row buffer's size */
    UInt                    mFetchRowBufSize;
    /* Converted row �� ����Ǵ� temp buffer's size */
    UInt                    mConvertedRowBufSize;
    /* �ѹ��� ���ݼ������� �������� ���ڵ��� ���� */
    UInt                    mFetchRowCnt;
    /* ���ݼ������� ������ fetch row buffer �� �����ϴ� ���ڵ� ���� */
    UInt                    mInsertRowCnt;
    /* ��ü ���ڵ� ���� */
    ULong                   mRecordCnt;
    /* QP ���� ���ڵ带 �������� �� �Ͻ������� ���ڵ带 ������ ���� */
    SChar                 * mFetchRowBuffer;
    /* Converted �� row �ϳ��� ����Ǵ� temp buffer */
    SChar                 * mConvertedRowBuffer;
    /* QP ���� fetch �� ������ �� ����Ű�� ���ڵ� */
    SChar                 * mInsertPos;    
    /* Type converter */
    struct dkdTypeConverter    * mTypeConverter;
    /* fetch �� row �� �Ͻ������� ���� */
    SChar                 * mCurRow; 

public:
    /* Initialize / Activate / Finalize */
    IDE_RC                  initialize( UInt * aBuffSize );
    IDE_RC                  activate();
    IDE_RC                  makeFetchRowBuffer();
    void                    freeFetchRowBuffer();
    void                    freeConvertedRowBuffer();
    void                    cleanFetchRowBuffer();
    void                    finalize();

    /* Data converter */
    IDE_RC                  createTypeConverter( dkpColumn   *aColMetaArr, 
                                                 UInt         aColCount );
    IDE_RC                  destroyTypeConverter();
    IDE_RC                  getConvertedMeta( mtcColumn **aMeta );
    IDE_RC                  getRecordLength();

    /* Operations */
    void                    fetchRow( void  **aRow );
    IDE_RC                  insertRow( void *aRow );
    IDE_RC                  restart();

    /* Setter / Getter */
    inline void             setEndOfFetch( idBool   aIsEndOfFetch );
    inline idBool           getEndOfFetch();

    inline UInt             getFetchRowSize();
    inline UInt             getFetchRowCnt();

    inline UInt             getInsertRowCnt();

    inline dkdTypeConverter * getTypeConverter();

    inline void             moveFirst();

    inline void             setCurRowNull();

    inline UInt             getConvertedRowBufSize();

    inline SChar *          getConvertedRowBuffer();
    
    inline idBool           isFirst();
};

inline void dkdRemoteTableMgr::setEndOfFetch( idBool   aIsEndOfFetch )
{
    mIsEndOfFetch = aIsEndOfFetch;
}

inline idBool   dkdRemoteTableMgr::getEndOfFetch()
{
    return mIsEndOfFetch;
}

inline UInt dkdRemoteTableMgr::getFetchRowSize()
{
    return mRowSize;
}

inline UInt dkdRemoteTableMgr::getFetchRowCnt()
{
    return mFetchRowCnt;
}

inline UInt dkdRemoteTableMgr::getInsertRowCnt()
{
    return mInsertRowCnt;
}

inline dkdTypeConverter *   dkdRemoteTableMgr::getTypeConverter()
{
    return mTypeConverter;
}

inline void dkdRemoteTableMgr::moveFirst()
{
    mCurRow = mFetchRowBuffer;
}

inline void dkdRemoteTableMgr::setCurRowNull()
{
    mCurRow = NULL;
}

inline UInt dkdRemoteTableMgr::getConvertedRowBufSize()
{
    return mConvertedRowBufSize;
}

inline SChar *  dkdRemoteTableMgr::getConvertedRowBuffer()
{
    return mConvertedRowBuffer;
}

inline idBool dkdRemoteTableMgr::isFirst()
{
    idBool sResult = ID_FALSE;

    if ( mCurRow == mFetchRowBuffer )
    {
        sResult = ID_TRUE;
    }
    else
    {
        sResult = ID_FALSE;
    }

    return sResult;
}

#endif /* _O_DKD_REMOTE_TABLE_MGR_H_ */
