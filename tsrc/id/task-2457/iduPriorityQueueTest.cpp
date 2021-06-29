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
 
#include <iduPriorityQueue.h>

#define ARRAY_SIZE 13071
#define TEST_RETRY_CNT 10


SInt verifyAscArray(SInt *array, SInt size)
{
    SInt i;

    for(i=1; i< size; i++)
    {
        //�տ����� �� ũ�� ����, �ڿ����� �� Ŀ�� �Ѵ�.
        if( array[i-1] > array[i]){
            IDE_ASSERT(0);
            return -1;
        }
    }
    return 1;
}

SInt ascIntCmp(const void *a,const void *b)
{
    return (*(SInt*)a - *(SInt*)b);
}

/*--------------------------------------------------------------------
  PrioriQueueTest1
  integer test
  -------------------------------------------------------------------*/

void PrioriQueueTest1()
{
    iduPriorityQueue    sPQueue;
    UInt            i,k;
    SInt            sValue;
    SInt            sResult[ARRAY_SIZE];
    idBool          sError=ID_FALSE;

    //priority queue�� ���� memcpy�ؼ� �迭�� ���·� �����Ѵ�.
    //�׷��� ������ �������� ũ�⸦ �ݵ�� �������־�� �Ѵ�.
    IDE_TEST( sPQueue.initialize( IDU_MEM_OTHER,
                                  ARRAY_SIZE,
                                  ID_SIZEOF(SInt),
                                  ascIntCmp )
              != IDE_SUCCESS);

    for(k=0; k<TEST_RETRY_CNT; k++)
    {
        for(i=0; i<ARRAY_SIZE; i++)
        {
            sValue = rand()%ARRAY_SIZE;

            //priority queue�� ���� memcpy�ؼ� �迭�� ���·� �����Ѵ�.
            //�׷��Ƿ� ���� �������� ���� �����ϰ� ���� �ʿ䰡 ����.
            sPQueue.enqueue(&sValue, &sError);
            
            IDE_ASSERT( sError == ID_FALSE);
        }
        for(i=0; i<ARRAY_SIZE; i++)
        {
            //priority queue�� ���� memcpy�ؼ� �����Ѵ�.
            sPQueue.dequeue((void*)&sResult[i], &sError);
            IDE_ASSERT( sError == ID_FALSE);
        }
        
        if( verifyAscArray( sResult, ARRAY_SIZE) < 0)
        {
            IDE_ASSERT(0);
        }
    }

    sPQueue.destroy();

    return;

    IDE_EXCEPTION_END;

    IDE_ASSERT(0);
}

/*--------------------------------------------------------------------
  PrioriQueueTest2
  struct test
  -------------------------------------------------------------------*/
typedef struct saTest
{
    SInt mNum;
    SInt mDummy;
}saTest;


SInt saTestAscIntCmp(const void *a, const void *b)
{
    SInt sNumA,sNumB;
    sNumA = ((saTest*)a)->mNum;
    sNumB = ((saTest*)b)->mNum;
    return sNumA - sNumB;
}


void PrioriQueueTest2()
{
    struct saTest    sData;
    SInt             sResult[ARRAY_SIZE];
    SInt             i,k;
    SInt             sInputCnt;
    SInt             sOutputCnt;
    iduPriorityQueue     sPQueue;
    idBool           sError;

    IDE_TEST( sPQueue.initialize( IDU_MEM_OTHER,
                                  ARRAY_SIZE,
                                  ID_SIZEOF(saTest),
                                  saTestAscIntCmp )
              != IDE_SUCCESS);

    for( k=0; k<TEST_RETRY_CNT;k++)
    {
        //sInput�� ������ ARRAY_SIZE���� ���� random���� ����
        sInputCnt = rand()%ARRAY_SIZE;
        //overflow�� �����ϱ� ���� empty�� ����
        sPQueue.empty();
        for(i=0; i<sInputCnt; i++)
        {
            sData.mNum = rand()%ARRAY_SIZE;
            sPQueue.enqueue((void*)&sData, &sError);
            IDE_ASSERT( sError == ID_FALSE);
        }
        //sOutput�� ������ sInputCnt���� ���� random���� ����
        sOutputCnt =rand()%sInputCnt;
        for(i=0; i<sOutputCnt; i++)
        {
            sPQueue.dequeue(&sData, &sError);
            IDE_ASSERT( sError == ID_FALSE);
            sResult[i] = sData.mNum;
        }
        if( verifyAscArray( sResult, sOutputCnt) < 0)
        {
            IDE_ASSERT(0);
        }
    }
    return;
    
    IDE_EXCEPTION_END;

    IDE_ASSERT(0);
}

/*--------------------------------------------------------------------
  PrioriQueueTest3
  pointer test
  -------------------------------------------------------------------*/
//Priority queue�� �����͸� �����ϰ� �ֱ� ������ �Ʒ��� ���� ĳ������ �ؾ� �Ѵ�.
SInt saTestAscIntCmpPoint(const void *a,const void *b)
{
    SInt sNumA,sNumB;
    sNumA = (**(saTest**)a).mNum;
    sNumB = (**(saTest**)b).mNum;
    return sNumA - sNumB;
}


void PrioriQueueTest3()
{
    struct saTest   *sData;
    SInt             sResult[ARRAY_SIZE];
    SInt             i,k;
    SInt             sInputCnt;
    SInt             sOutputCnt;
    iduPriorityQueue sPQueue;
    idBool           sError;

    //saTest�� �����͸� ������
    IDE_TEST( sPQueue.initialize( IDU_MEM_OTHER,
                                  ARRAY_SIZE,
                                  ID_SIZEOF(saTest*),
                                  saTestAscIntCmpPoint )
              != IDE_SUCCESS);

    for( k=0; k<TEST_RETRY_CNT;k++)
    {
        //sInput�� ������ ARRAY_SIZE���� ���� random���� ����
        sInputCnt = rand()%ARRAY_SIZE;
        //overflow�� �����ϱ� ���� empty�� ����
        sPQueue.empty();
        for(i=0; i<sInputCnt; i++)
        {
            IDE_TEST(iduMemMgr::malloc( IDU_MEM_OTHER,
                                        ID_SIZEOF(saTest),
                                        (void**)&sData)
                     != IDE_SUCCESS);
        
            sData->mNum = rand()%ARRAY_SIZE;
            //�����͸� �����ǳ����� ����, �̰�쿣 ������ �ּҰ���
            //�Ѱ� �־�� �Ѵ�.
            sPQueue.enqueue((void*)&sData, &sError);
            IDE_ASSERT(sError == ID_FALSE);
        
        }
        //sOutput�� ������ sInputCnt���� ���� random���� ����
        sOutputCnt =rand()%sInputCnt;
        for(i=0; i<sOutputCnt; i++)
        {
            sPQueue.dequeue((void*)&sData, &sError);
            sResult[i] = sData->mNum;
            //malloc�� �ּҰ��� queue�� ����ְ�, ���� �����Ƿ�
            //�״�� free�� �����ϴ�.
            IDE_TEST(iduMemMgr::free( sData) != IDE_SUCCESS);
        }
        if( verifyAscArray( sResult, sOutputCnt) < 0)
        {
            IDE_ASSERT(0);
        }
    }
    return;
    
    IDE_EXCEPTION_END;

    IDE_ASSERT(0);
}


/*--------------------------------------------------------------------
  PrioriQueueTest4
  char test
  -------------------------------------------------------------------*/
SInt ascCharCmp(const void *a,const void *b)
{
    return (*(SChar*)a - *(SChar*)b);
}

void PrioriQueueTest4()
{
    SInt                sResult[ARRAY_SIZE];
    iduPriorityQueue    sPQueue;
    SInt                i;
    idBool              sError;
    SChar               sData;

    IDE_TEST( sPQueue.initialize( IDU_MEM_OTHER,
                                  ARRAY_SIZE,
                                  ID_SIZEOF(SChar),
                                  ascCharCmp )
              != IDE_SUCCESS);

    for(i=0; i<ARRAY_SIZE; i++)
    {
        //���ĺ��� �ƹ��ų� ����
        sData = rand()%('z' - 'A' ) + 'A';
        sPQueue.enqueue(&sData, &sError);
        IDE_ASSERT(sError == ID_FALSE);
    }
    for(i=0; i<ARRAY_SIZE; i++)
    {
        sPQueue.dequeue(&sData, &sError);
        IDE_ASSERT(sError == ID_FALSE);
        sResult[i] = (SInt)sData;
    }
    
    if( verifyAscArray( sResult, ARRAY_SIZE) < 0)
    {
        IDE_ASSERT(0);
    }

    sPQueue.destroy();
    return;
    
    IDE_EXCEPTION_END;

    IDE_ASSERT(0);

}
    


SInt main()
{
    PrioriQueueTest1();
    PrioriQueueTest2();
    PrioriQueueTest3();
    PrioriQueueTest4();
}

