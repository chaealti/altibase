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

#ifndef _O_DKD_DATA_MGR_H_
#define _O_DKD_DATA_MGR_H_ 1


#include <dkd.h>
#include <dkdRecordBufferMgr.h>
#include <dkdDiskTempTableMgr.h>
#include <dkdDataBufferMgr.h>
#include <dkdTypeConverter.h>
#include <dkpDef.h>

typedef enum
{
    DKD_DATA_MGR_TYPE_DISK,
    DKD_DATA_MGR_TYPE_MEMORY
} dkdDataMgrType;

typedef IDE_RC  (* dkdFetchRowFunc)( void *aMgrHandle, void **aRow );
typedef IDE_RC  (* dkdInsertRowFunc)( void *aMgrHandle, dkdRecord *aRecord );
typedef IDE_RC  (* dkdRestartFunc)( void *aMgrHandle );


class dkdDataMgr
{
private:
    /* ���� iteration �ؾ� �� data �� record buffer �� �����ϴ��� disk temp
       table �� �����ϴ��� ���� */
    idBool                mIsRecordBuffer;
    /* ������ data block �� �� �о��� readRecordDataFromProtocol �� ���
       DK_RC_EOF �� ��� TRUE �� ���� */
    idBool                mIsEndOfFetch;
    /* ��ü record �� ����, insertRow �� ������ ������ 1�� ���� */
    UInt                  mRecordCnt;
    /* Converted record �� ���� */
    UInt                  mRecordLen;
    /* Converted Row's size */
    UInt                  mRowSize;
    /* Disk temp table �� ���� ���� �Ҵ���� record catridge */
    dkdRecord           * mRecord;
    /* Record buffer �� ����ϴ� ��� ���� record buffer �� �ִ�ũ�� */
    UInt                  mRecordBufferSize;
    /* AltiLinker ���μ����κ��� packet �� �ϳ� ���� ������ �����ϴ� 
       data block �� count. ���� ���ġ Ȯ������ ���� �� �ִ�. */
    UInt                  mDataBlockRecvCount;
    /* Data manager �� ���� data block �� count. */
    UInt                  mDataBlockReadCount;
    /* ���� data block ���� ����Ű�� �ִ� ���� */
    SChar               * mDataBlockPos;

    struct dkdTypeConverter    * mTypeConverter;

    iduMemAllocator     * mAllocator;
    dkdRecordBufferMgr  * mRecordBufferMgr;
    dkdDiskTempTableMgr * mDiskTempTableMgr;

    void                * mMgrHandle;

    void                * mCurRow; /* fetch �� row �� �Ͻ������� ���� */

    dkdFetchRowFunc       mFetchRowFunc;
    dkdInsertRowFunc      mInsertRowFunc;
    dkdRestartFunc        mRestartFunc;

    dkdDataMgrType        mDataMgrType;

    UInt                  mFetchRowCount;
    
    void                * mNullRow;

    idBool                mFlagRestartOnce;
    
public:
    IDE_RC      initialize( UInt  *aBuffSize );
     /* PROJ-2417 */
    IDE_RC      initializeRecordBuffer();
    IDE_RC      activate( void  *aQcStatement );
    /* BUG-37487 */
    void        finalize();
    IDE_RC      getRecordLength();
    IDE_RC      getRecordBufferSize();

    /* Operations */
    IDE_RC      moveNextRow( idBool *aEndFlag );
    IDE_RC      fetchRow( void  **aRow );
    IDE_RC      insertRow( void  *aRow, void *aQcStatement );
    IDE_RC      restart();
    IDE_RC      restartOnce( void );

    /* Record buffer manager */
    IDE_RC      createRecordBufferMgr( UInt  aAllocableBlockCnt );
    /* BUG-37487 */
    void        destroyRecordBufferMgr();

    /* Disk temp table manager */
    IDE_RC      createDiskTempTableMgr( void  *aQcStatement );
    /* BUG-37487 */
    void        destroyDiskTempTableMgr();

    /* Data converter */
    IDE_RC      createTypeConverter( dkpColumn   *aColMetaArr, 
                                     UInt         aColCount );
    IDE_RC      destroyTypeConverter();

    IDE_RC      getConvertedMeta( mtcColumn **aMeta );

    /* Switch from record buffer manager to disk temp table manager */
    void        switchToDiskTempTable(); /* BUG-37487 */
    IDE_RC      moveRecordToDiskTempTable();

    /* inline functions */
    inline dkdTypeConverter *   getTypeConverter();
    inline void *   getColValue( UInt   aOffset );

    inline void     setEndOfFetch( idBool   aIsEndOfFetch );
    inline idBool   getEndOfFetch();
    inline idBool   isRemainedRecordBuffer();

    IDE_RC getNullRow( void ** aRow, scGRID * aRid );
    idBool isNeedFetch( void );
    
    inline idBool   isFirst();
};


inline dkdTypeConverter *   dkdDataMgr::getTypeConverter()
{
    return mTypeConverter;
}

inline void *   dkdDataMgr::getColValue( UInt   aOffset )
{
    return (void *)((SChar *)mCurRow + aOffset);
}

inline void dkdDataMgr::setEndOfFetch( idBool   aIsEndOfFetch )
{
    mIsEndOfFetch = aIsEndOfFetch;
}

inline idBool   dkdDataMgr::getEndOfFetch()
{
    return mIsEndOfFetch;
}

inline idBool   dkdDataMgr::isRemainedRecordBuffer()
{
    return ( mRecordBufferSize - ( mRecordCnt * mRecordLen ) >= mRecordLen ) 
        ? ID_TRUE 
        : ID_FALSE;
}

inline idBool   dkdDataMgr::isFirst()
{
    idBool  sResult = ID_FALSE;

    if ( mFetchRowCount == 0 )
    {
        sResult = ID_TRUE;
    }
    else
    {
        sResult = ID_FALSE;
    }

    return sResult;
}

#endif /* _O_DKD_DATA_MGR_H_ */

