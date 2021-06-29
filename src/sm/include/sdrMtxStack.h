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
 * $Id: sdrMtxStack.h 82075 2018-01-17 06:39:52Z jina.kim $
 *
 * Description :
 *
 *
 *
 *
 **********************************************************************/



#ifndef _O_SDR_MTX_STACK_H_
#define _O_SDR_MTX_STACK_H_ 1

#include <sdrDef.h>

class sdrMtxStack
{
public:

    inline static IDE_RC initialize( sdrMtxStackInfo * aStack );
    
    inline static IDE_RC destroy ( sdrMtxStackInfo * aStack );
    
    inline static IDE_RC push( sdrMtxStackInfo * aStack, 
                               sdrMtxLatchItem * aItem );

    inline static sdrMtxLatchItem * pop( sdrMtxStackInfo * aStack );

    static sdrMtxLatchItem* getTop( sdrMtxStackInfo *aStack );

    inline static UInt getCurrSize( sdrMtxStackInfo * aStack );

    static UInt getTotalSize(sdrMtxStackInfo*  aStack);

    inline static idBool isEmpty( sdrMtxStackInfo * aStack );

    static sdrMtxLatchItem* find( sdrMtxStackInfo*        aStack,
                                  sdrMtxLatchItem*        aItem,
                                  sdrMtxItemSourceType    aType,
                                  sdrMtxLatchItemApplyInfo *aVector );
    
    static void dump( sdrMtxStackInfo *aStack,
                      sdrMtxLatchItemApplyInfo *aVector,
                      sdrMtxDumpMode   aDumpMode );

    static idBool validate( sdrMtxStackInfo   *aStack );
private:
    static void logMtxStack( sdrMtxStackInfo *aStack );
};

/***********************************************************************
 * Description : ������ �ʱ�ȭ�Ѵ�.
 *
 * - 1st. code design
 *
 *   size��ŭ �޸𸮸� �Ҵ��Ѵ�.
 *   
 **********************************************************************/
inline IDE_RC sdrMtxStack::initialize( sdrMtxStackInfo *  aStack )
{
    IDE_DASSERT( aStack != NULL );

    aStack->mTotalSize = SDR_MAX_MTX_STACK_SIZE;
    aStack->mCurrSize = 0;

    return IDE_SUCCESS;
}

/***********************************************************************
 * Description : �ı���
 *
 * �޸� �ı�
 **********************************************************************/
inline IDE_RC sdrMtxStack::destroy( sdrMtxStackInfo * /*aStack*/ )
{
    return IDE_SUCCESS;
}

/***********************************************************************
 * Description : �ϳ��� item�� stack�� �ִ´�.
 *
 * ������Ʈ�� �޸𸮸� �Ҵ���� ������
 *
 * - 1st. code design
 *
 * if( stack�� �� á�� )
 *     2��� resize
 *
 * array[size++] = item
 * 
 **********************************************************************/
inline IDE_RC sdrMtxStack::push( sdrMtxStackInfo * aStack,
                                 sdrMtxLatchItem * aItem )
{
    SInt sIndex;

    IDE_DASSERT( aStack != NULL );
    IDE_DASSERT( aItem != NULL );
    IDE_DASSERT( validate( aStack ) == ID_TRUE );

    // BUG-13972
    // sNewSize�� ������������ Ŀ���� ���
    // sdrMtxStack�� dump�ϰ� ������ ���� ������ ���� ��Ų��.
    sIndex = aStack->mCurrSize;

    if ( sIndex < SDR_MAX_MTX_STACK_SIZE )
    {
        aStack->mArray[ sIndex ] = *aItem;
    }
    else
    {
        logMtxStack( aStack );
        IDE_ASSERT( 0 );
    }

    aStack->mCurrSize = sIndex + 1;

    return IDE_SUCCESS;
}

/***********************************************************************
 * Description : ���� ���� item�� stack���� �����Ѵ�.
 * 
 * - 1st. code design
 *
 * aStack->mSize�� ����
 *
 **********************************************************************/
inline sdrMtxLatchItem * sdrMtxStack::pop( sdrMtxStackInfo * aStack )
{
    sdrMtxLatchItem* sItem;
    IDE_DASSERT( aStack != NULL );
    IDE_DASSERT( validate( aStack ) == ID_TRUE );

    IDE_ASSERT( aStack->mCurrSize != 0 );

    sItem = getTop( aStack );
    
    --aStack->mCurrSize;

    return sItem;
}

/***********************************************************************
 * Description : ���� stack�� size�� ��´�.
 **********************************************************************/
inline UInt sdrMtxStack::getCurrSize( sdrMtxStackInfo * aStack )
{
    IDE_DASSERT( aStack != NULL );
    return aStack->mCurrSize;
}

/***********************************************************************
 * Description : stack�� ����ִ��� Ȯ���Ѵ�.
 *
 * - 1st. code design
 *   + if( size�� 0 )
 *         return true
 *   + return false
 **********************************************************************/
inline idBool sdrMtxStack::isEmpty( sdrMtxStackInfo * aStack )
{
    return ( getCurrSize( aStack ) == 0 ? ID_TRUE : ID_FALSE );
}

#endif // _O_SDR_MTX_STACK_H_

