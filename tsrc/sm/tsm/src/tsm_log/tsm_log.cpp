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
 * $Id: tsm_log.cpp 82075 2018-01-17 06:39:52Z jina.kim $
 **********************************************************************/

#include <idl.h>
#include <tsm.h>
#include <smi.h>
#include <sml.h>

/* Write Log Thread Function */
void * tsmWriteLog( void *data );
void * tsmInsertIntoTable( void */*data*/ );

#define TSM_MAX_THREAD_COUNT 1024

/* Test Thread�� ���ü� ��� ���ؼ� */
static PDL_sema_t gThreadSema;

/* ���ÿ� �α� Write�� Thread�� ���� */
UInt gTotalThreadCount  = 0;

/* ��� Threa�� write�� ������ �� log�� write�� Ƚ�� */
UInt gTotalLoopCount    = 0;

/* ������ Thread�� write�� �α��� ���� */
UInt gLoopCountOfThread = 0;

/* ������ Transaction�� Write�� log�� ũ�� */
UInt gLogSize = 0;

static SChar gTableName1[100] = "Table1";

int main(SInt argc, SChar **argv)
{
    UInt             i;
    PDL_hthread_t    sArrThreadHandle[TSM_MAX_THREAD_COUNT];
    PDL_thread_t     sArrThreadID[TSM_MAX_THREAD_COUNT];
    PDL_Time_Value   sStartTime;
    PDL_Time_Value   sEndTime;
    PDL_Time_Value   sRunningTime;
    double           sSec;
    double           sTPS;

    if(argc != 4)
    {
        printf("tsm_log thr_count total_loop_count log_size\n");
        exit(-1);
    }

    gTotalThreadCount = atoi(argv[1]);
    IDE_ASSERT(gTotalThreadCount != 0 &&
               gTotalThreadCount <= TSM_MAX_THREAD_COUNT);

    gTotalLoopCount   = atoi(argv[2]);
    IDE_ASSERT(gTotalLoopCount != 0);

    gLogSize          = atoi(argv[3]);
    IDE_ASSERT(gLogSize != 0);

    gLoopCountOfThread = gTotalLoopCount / gTotalThreadCount;
    IDE_ASSERT(gLoopCountOfThread != 0);

    printf("##Start Test -----------------------------\n"
           " 1. Thread Count: %d\n"
           " 2. Total  Loop Count: %d\n"
           " 3. Log Size: %d\n",
           gTotalThreadCount,
           gTotalLoopCount,
           gLogSize);

    IDE_ASSERT(tsmInit() == IDE_SUCCESS);

    IDE_ASSERT(smiStartup(SMI_STARTUP_PRE_PROCESS,
                          SMI_STARTUP_NOACTION,
                          &gTsmGlobalCallBackList)
               == IDE_SUCCESS);

    IDE_ASSERT(smiStartup(SMI_STARTUP_PROCESS,
                          SMI_STARTUP_NOACTION,
                          &gTsmGlobalCallBackList)
               == IDE_SUCCESS);

    IDE_ASSERT(smiStartup(SMI_STARTUP_CONTROL,
                          SMI_STARTUP_NOACTION,
                          &gTsmGlobalCallBackList)
               == IDE_SUCCESS);
    IDE_ASSERT(smiStartup(SMI_STARTUP_META,
                          SMI_STARTUP_NOACTION,
                          &gTsmGlobalCallBackList)
               == IDE_SUCCESS);

    idlOS::sleep(10);
    IDE_ASSERT( PDL_OS::sema_init(&gThreadSema, gTotalThreadCount)
                == 0);

    /* Test���� */
    for( i = 0 ; i < gTotalThreadCount; i++)
    {
        IDE_ASSERT( idlOS::thr_create( tsmWriteLog,
                                       NULL,
                                       THR_JOINABLE | THR_BOUND,
                                       &sArrThreadID[i],
                                       &sArrThreadHandle[i],
                                       PDL_DEFAULT_THREAD_PRIORITY,
                                       NULL,
                                       1024*1024)
                    == IDE_SUCCESS );
    }

    sStartTime = idlOS::gettimeofday();

    for( i = 0; i < gTotalThreadCount; i++)
    {
        IDE_ASSERT( PDL_OS::sema_post(&gThreadSema) == 0 );
    }

    for( i = 0; i < gTotalThreadCount; i++ )
    {
        IDE_TEST(idlOS::thr_join( sArrThreadHandle[i], NULL ) != 0);
    }

    sEndTime = idlOS::gettimeofday();

    sRunningTime = sEndTime - sStartTime;
    sSec = sRunningTime.sec() + (double)sRunningTime.usec() / 1000000;

    sTPS = (double)gTotalLoopCount / sSec;

    printf("## Result -----------------------------\n"
           " 1. Running Time(sec):%f\n"
           " 2. TPS:%f\n",
           sSec,
           sTPS);

    /* Test�� */
    IDE_ASSERT( PDL_OS::sema_destroy(&gThreadSema) == 0);

    IDE_TEST(smiStartup(SMI_STARTUP_SHUTDOWN,
                        SMI_STARTUP_NOACTION,
                        &gTsmGlobalCallBackList)
             != IDE_SUCCESS);
    IDE_TEST(tsmFinal() != IDE_SUCCESS);

    return 0;

    IDE_EXCEPTION_END;

    return -1;
}

void * tsmWriteLog( void */*data*/ )
{
    smiTrans      sTrans;
    SChar        *sBuffer;
    smrLogHead   *sLogHeadPtr;
    UInt          i;
    smiStatement *spRootStmt;
    UInt          sLoopCount;

    sBuffer = NULL;
    sBuffer = (SChar*)idlOS::malloc(gLogSize);
    IDE_ASSERT(sBuffer != NULL);

    idlOS::memset(sBuffer, 0, gLogSize);

    IDE_ASSERT(sTrans.initialize() == IDE_SUCCESS);

    sLoopCount = gLoopCountOfThread / 2;

    sLogHeadPtr = (smrLogHead*)sBuffer;
    sLogHeadPtr->mSize = gLogSize;

    IDE_ASSERT( PDL_OS::sema_wait(&gThreadSema) == 0 );

    for(i = 0; i < sLoopCount; i++)
    {
        IDE_ASSERT(sTrans.begin(&spRootStmt, NULL) == IDE_SUCCESS);

        IDE_ASSERT(smrLogMgr::writeLog(NULL,
                                       sTrans.getTrans(),
                                       sBuffer,
                                       NULL,
                                       NULL,
                                       NULL,
                                       NULL)
                   == IDE_SUCCESS);

        IDE_ASSERT(smrLogMgr::writeLog(NULL,
                                       sTrans.getTrans(),
                                       sBuffer,
                                       NULL,
                                       NULL,
                                       NULL,
                                       NULL)
                   == IDE_SUCCESS);

        IDE_ASSERT(sTrans.commit(NULL) == IDE_SUCCESS);
    }

    IDE_ASSERT(sTrans.destroy() == IDE_SUCCESS);

    return NULL;
}

void * tsmInsertIntoTable( void */*data*/ )
{
    smiTrans      sTrans;
    SChar        *sBuffer;
    smrLogHead   *sLogHeadPtr;
    UInt          i;
    smiStatement *spRootStmt;
    UInt          sLoopCount;
    SChar         sBuffer1[32];
    SChar         sBuffer2[24];

    sBuffer = NULL;
    sBuffer = (SChar*)idlOS::malloc(gLogSize);
    IDE_ASSERT(sBuffer != NULL);

    idlOS::memset(sBuffer, 0, gLogSize);

    IDE_ASSERT(sTrans.initialize() == IDE_SUCCESS);

    sLoopCount = gLoopCountOfThread / 2;

    sLogHeadPtr = (smrLogHead*)sBuffer;
    sLogHeadPtr->mSize = gLogSize;

    IDE_ASSERT( PDL_OS::sema_wait(&gThreadSema) == 0 );

    for(i = 0; i < sLoopCount; i++)
    {
        IDE_ASSERT(sTrans.begin(&spRootStmt, NULL) == IDE_SUCCESS);

        idlOS::memset(sBuffer1, 0, 32);
        idlOS::memset(sBuffer2, 0, 24);
        idlOS::sprintf(sBuffer1, "2nd - %d", i);
        idlOS::sprintf(sBuffer2, "3rd - %d", i);

        IDE_ASSERT(tsmInsert(spRootStmt,
                             1,
                             gTableName1,
                             TSM_TABLE1,
                             i,
                             sBuffer1,
                             sBuffer2) == IDE_SUCCESS );

        IDE_ASSERT(sTrans.commit(NULL) == IDE_SUCCESS);
    }

    IDE_ASSERT(sTrans.destroy() == IDE_SUCCESS);

    return NULL;
}
