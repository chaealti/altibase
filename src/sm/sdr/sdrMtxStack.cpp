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
 * $Id: sdrMtxStack.cpp 82075 2018-01-17 06:39:52Z jina.kim $
 *
 * Description :
 *
 *
 **********************************************************************/

#include <ide.h>
#include <sdrMtxStack.h>
#include <sdb.h>
#include <smErrorCode.h>

void sdrMtxStack::logMtxStack( sdrMtxStackInfo *aStack )
{
    UInt i;
    
    ideLog::log(SM_TRC_LOG_LEVEL_DEBUG, SM_TRC_MTXSTACK_SIZE_OVERFLOW1);

    for( i = 0; i < aStack->mCurrSize; i++ )
    {
        switch( aStack->mArray[i].mLatchMode )
        {
            case SDR_MTX_PAGE_X:
              ideLog::log(IDE_SM_0,
                          SM_TRC_MTXSTACK_SIZE_OVERFLOW2,
                          i,
                          ((sdbBCB*)aStack->mArray[i].mObject)->mSpaceID,
                          ((sdbBCB*)aStack->mArray[i].mObject)->mPageID);
              break;
            case SDR_MTX_PAGE_S:
              ideLog::log(IDE_SM_0,
                          SM_TRC_MTXSTACK_SIZE_OVERFLOW3,
                          i,
                          ((sdbBCB*)aStack->mArray[i].mObject)->mSpaceID,
                          ((sdbBCB*)aStack->mArray[i].mObject)->mPageID);
              break;
            case SDR_MTX_PAGE_NO:
              ideLog::log(IDE_SM_0,
                          SM_TRC_MTXSTACK_SIZE_OVERFLOW4,
                          i,
                          ((sdbBCB*)aStack->mArray[i].mObject)->mSpaceID,
                          ((sdbBCB*)aStack->mArray[i].mObject)->mPageID);
              break;
            case SDR_MTX_LATCH_X:
              ideLog::log(IDE_SM_0,
                          SM_TRC_MTXSTACK_SIZE_OVERFLOW5,
                          i);
              break;
            case SDR_MTX_LATCH_S:
              ideLog::log(IDE_SM_0,
                          SM_TRC_MTXSTACK_SIZE_OVERFLOW6,
                          i);
              break;
            case SDR_MTX_MUTEX_X:
              ideLog::log(IDE_SM_0,
                          SM_TRC_MTXSTACK_SIZE_OVERFLOW7,
                          i);
              break;
            default:
              IDE_DASSERT( 0 );
              break;
        }
    }
}

/***********************************************************************
 * Description : ������ item�� stack���� ��´�
 *
 * ���ÿ��� �������� ����. ������´�.
 *
 * - 1st code design
 *
 *   aStack->mArray[���� ũ��]�� ����
 **********************************************************************/
sdrMtxLatchItem* sdrMtxStack::getTop(sdrMtxStackInfo*  aStack )
{

    IDE_DASSERT( aStack != NULL );
    return &(aStack->mArray[aStack->mCurrSize-1]);
    
}

/***********************************************************************
 * Description : ���� stack�� size�� ��´�.
 **********************************************************************/
UInt sdrMtxStack::getTotalSize(sdrMtxStackInfo*  aStack)
{

    IDE_DASSERT( aStack != NULL );
    return aStack->mTotalSize;
}

/***********************************************************************
 * Description : ���ÿ� �ش� �������� ã�´�.
 *
 * �� �Լ��� �����ȴ�.
 *
 * - 1st. code design
 *
 *   + while( ��� ������ �����ۿ� ���ؼ� )
 *     if( �������� ������  ) �������� ����
 *   + null�� ����
 **********************************************************************/
sdrMtxLatchItem* sdrMtxStack::find( 
    sdrMtxStackInfo*          aStack,
    sdrMtxLatchItem*          aItem,
    sdrMtxItemSourceType      aType,
    sdrMtxLatchItemApplyInfo *aVector )
{

    UInt sLoop;
    UInt sLatchMode;
    sdrMtxLatchItem *sCurrItem;
    UInt sStackSize;
    

    IDE_DASSERT( aStack != NULL );
    IDE_DASSERT( aItem != NULL );
    IDE_DASSERT( validate(aStack) == ID_TRUE );

    sStackSize= getCurrSize(aStack);
    
    for( sLoop = 0; sLoop != sStackSize; ++sLoop )
    {
        // ���� stack�� item
        sCurrItem = &(aStack->mArray[sLoop]);
        
        // stack�� �ִ� item�� latch mode
        sLatchMode = sCurrItem->mLatchMode;

        // ���� �������� Ÿ�԰� ���� Ÿ���� ����. ��
        // ���� ������ ������Ʈ�̸� �˻��ϱ� ���Ѵ�.
        if( aVector[sLatchMode].mSourceType == aType )
        {
            if( aVector[sLatchMode].mCompareFunc(
                                         aItem->mObject,
                                         aStack->mArray[sLoop].mObject )
                == ID_TRUE )
            {
                return sCurrItem;
            }   
        }
    }

    return NULL;
    
}

/***********************************************************************
 * Description : 
 * stack�� dump
 *
 **********************************************************************/
void sdrMtxStack::dump(
    sdrMtxStackInfo          *aStack,
    sdrMtxLatchItemApplyInfo *aVector,
    sdrMtxDumpMode            aDumpMode )
{

    UInt sLoop;
    UInt sLatchMode;
    UInt sStackSize;

    IDE_DASSERT(aStack != NULL);
    IDE_DASSERT(aVector != NULL);
    
    sStackSize = getCurrSize(aStack);

    ideLog::log( IDE_SERVER_0,
                 "----------- Mtx Stack Info Begin ----------\n"
                 "Stack Total Capacity\t: %u\n"
                 "Stack Curr Size\t: %u",
                 getTotalSize(aStack),
                 sStackSize );

    if( aDumpMode == SDR_MTX_DUMP_DETAILED )
    {    
        for( sLoop = 0; sLoop != sStackSize; ++sLoop )
        {
            sLatchMode = aStack->mArray[sLoop].mLatchMode;
            
            ideLog::log( IDE_SERVER_0,
                         "Latch Mode(Stack Num)\t: %s(%u)",
                         (aVector[sLatchMode]).mLatchModeName, sLoop);
            
            (aVector[sLatchMode]).mDumpFunc( aStack->mArray[sLoop].mObject );
        }
    }

    ideLog::log( IDE_SERVER_0, "----------- Mtx Stack Info End ----------" );
}

/***********************************************************************
 *
 * Description : 
 *
 * validate
 *
 * Implementation :
 * 
 **********************************************************************/

#ifdef DEBUG
idBool sdrMtxStack::validate(  sdrMtxStackInfo   *aStack )
{
    UInt sLoop;
    sdrMtxLatchItem *aItem;
    UInt sStackSize;
    
    IDE_DASSERT( aStack != NULL );
    
    sStackSize = getCurrSize(aStack);
    for( sLoop = 0; sLoop != sStackSize; ++sLoop )
    {
        aItem = &(aStack->mArray[sLoop]);
        IDE_DASSERT( aItem != NULL );
        IDE_DASSERT( aItem->mObject != NULL );


        IDE_DASSERT( (aItem->mLatchMode == SDR_MTX_PAGE_X) ||
                     (aItem->mLatchMode == SDR_MTX_PAGE_S) ||
                     (aItem->mLatchMode == SDR_MTX_PAGE_NO) ||
                     (aItem->mLatchMode == SDR_MTX_LATCH_X) ||
                     (aItem->mLatchMode == SDR_MTX_LATCH_S) ||
                     (aItem->mLatchMode == SDR_MTX_MUTEX_X) );
    }

    return ID_TRUE;    
    
}
#endif 
