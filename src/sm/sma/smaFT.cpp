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
 * $Id: smaFT.cpp 37316 2009-12-13 14:47:02Z bskim $
 **********************************************************************/

#include <smErrorCode.h>
#include <smx.h>
#include <smcLob.h>
#include <sma.h>
#include <svcRecord.h>
#include <smi.h>
#include <sgmManager.h>
#include <smaFT.h>

IDE_RC  smaFT::buildRecordForDelThr(idvSQL              * /*aStatistics*/,
                                    void                * aHeader,
                                    void                * /* aDumpObj */,
                                    iduFixedTableMemory * aMemory)
{
    smaGCStatus sDelThrStatus;
    SChar       sGCName[SMA_GC_NAME_LEN];
    UInt        i;

    idlOS::memset( &sDelThrStatus, 0x00, ID_SIZEOF(smaGCStatus) );

    //1. alloc record buffer.

    // 2. assign.....
    idlOS::strcpy(sGCName,"MEM_DELTHR");
    sDelThrStatus.mGCName = sGCName;

    smmDatabase::getViewSCN( &sDelThrStatus.mSystemViewSCN );

    SMX_GET_MIN_MEM_VIEW( &sDelThrStatus.mMiMemSCNInTxs,
                          &sDelThrStatus.mOldestTx );

    // ATL�� �ּ� memory view�� ���Ѵ�,
    // �� active transaction�� ���� ����̴�.
    // �̷����� system view SCN�� setting�Ͽ�,
    // ���� GC alert�� �߻����� �ʵ����Ѵ�.
    if( SM_SCN_IS_INFINITE( sDelThrStatus.mMiMemSCNInTxs ) )
    {
        SM_GET_SCN(&(sDelThrStatus.mMiMemSCNInTxs),
                   &(sDelThrStatus.mSystemViewSCN));

        // ������ �ʴ� Ʈ����� ���̵�(0)�� �����Ѵ�.
        sDelThrStatus.mOldestTx = 0;
    }//if

    /* BUG-47367 OID�� ���ο� MinSCN�� ��� List�� Ž���Ͽ��� �˼� �ִ�.
     * �񱳸� ���� �⺻������ SystemViewSCN�� �����Ѵ�. */
    // Empty OIDList
    sDelThrStatus.mIsEmpty = ID_TRUE;
    // SCN GAP�� 0�� �ϱ� ���Ͽ� ���� �ý���ViewSCN.
    SM_GET_SCN( &(sDelThrStatus.mSCNOfTail),
                &(sDelThrStatus.mSystemViewSCN) );

    for( i = 0; i < smaLogicalAger::mListCnt ; i++ )
    {
        if ( smaLogicalAger::mTailDeleteThread[i] != NULL )
        {
            sDelThrStatus.mIsEmpty = ID_FALSE;
            if ( SM_SCN_IS_LE( &(smaLogicalAger::mTailDeleteThread[i]->mSCN),
                               &(sDelThrStatus.mSCNOfTail) ) )
            {
                SM_SET_SCN( &(sDelThrStatus.mSCNOfTail), 
                            &(smaLogicalAger::mTailDeleteThread[i]->mSCN) );
            }
        }
    }

    sDelThrStatus.mAddOIDCnt= smaLogicalAger::mHandledCount;
    sDelThrStatus.mGcOIDCnt = smaDeleteThread::mHandledCnt;

    ID_SERIAL_BEGIN( sDelThrStatus.mAgingProcessedOIDCnt = 
                     smaDeleteThread::mAgingProcessedOIDCnt );
    ID_SERIAL_END( sDelThrStatus.mAgingRequestOIDCnt =
                   smaLogicalAger::mAgingRequestOIDCnt );
    sDelThrStatus.mSleepCountOnAgingCondition = 
                               smaDeleteThread::mSleepCountOnAgingCondition;
    sDelThrStatus.mThreadCount = smaDeleteThread::mThreadCnt;

    // 3. build record for fixed table.

    IDE_TEST(iduFixedTable::buildRecord(aHeader,
                                        aMemory,
                                        (void *) &sDelThrStatus)
                 != IDE_SUCCESS);


    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

static iduFixedTableColDesc  gMemDelThrTableColDesc[]=
{

    {
        (SChar*)"GC_NAME",
        offsetof(smaGCStatus, mGCName),
        SMA_GC_NAME_LEN,
        IDU_FT_TYPE_VARCHAR|IDU_FT_TYPE_POINTER ,
        NULL,
        0, 0,NULL // for internal use
    },

    // ���� systemViewSCN.
    // smSCN  mSystemViewSCN;
    {
        (SChar*)"CurrSystemViewSCN",
        offsetof(smaGCStatus,mSystemViewSCN),
        29,
        IDU_FT_TYPE_VARCHAR,
        smiFixedTable::convertSCN,
        0, 0,NULL // for internal use
    },

    // smSCN  mMiMemSCNInTxs;
    {
        (SChar*)"minMemSCNInTxs",
        offsetof(smaGCStatus,mMiMemSCNInTxs),
        29,
        IDU_FT_TYPE_VARCHAR,
        smiFixedTable::convertSCN,
        0, 0,NULL // for internal use
    },

    // BUG-24821 V$TRANSACTION�� LOB���� MinSCN�� ��µǾ�� �մϴ�.
    // smTID  mOldestTx;
    {
        (SChar*)"oldestTx",
        offsetof(smaGCStatus,mOldestTx),
        IDU_FT_SIZEOF(smaGCStatus,mOldestTx),
        IDU_FT_TYPE_UBIGINT,  // BUG-47379 unsigned int -> big int
        NULL,
        0, 0,NULL // for internal use
    },
    // LogicalAger, DeleteThred�� Tail�� commitSCN or
    // old key���� ������ SCN.
    // smSCN  mSCNOfTail;
    {
        (SChar*)"SCNOfTail",
        offsetof(smaGCStatus,mSCNOfTail),
        29,
        IDU_FT_TYPE_VARCHAR,
        smiFixedTable::convertSCN,
        0, 0,NULL // for internal use
    },

    // OIDList�� ��� �ִ��� boolean
    // idBool mIsEmpty;
    {
        (SChar*)"IS_EMPTY_OIDLIST",
        offsetof(smaGCStatus,mIsEmpty),
        IDU_FT_SIZEOF(smaGCStatus,mIsEmpty),
        IDU_FT_TYPE_UBIGINT,
        NULL,
        0, 0,NULL // for internal use
    },

    // add�� OID ����.
    // ULong  mAddOIDCnt;
    {
        (SChar*)"ADD_OID_CNT",
        offsetof(smaGCStatus,mAddOIDCnt),
        IDU_FT_SIZEOF(smaGCStatus,mAddOIDCnt),
        IDU_FT_TYPE_UBIGINT,
        NULL,
        0, 0,NULL // for internal use
    },

    // GCó������ OID����.
    // ULong  mGcOIDCnt;
    {
        (SChar*)"GC_OID_CNT",
        offsetof(smaGCStatus,mGcOIDCnt),
        IDU_FT_SIZEOF(smaGCStatus,mGcOIDCnt),
        IDU_FT_TYPE_UBIGINT,
        NULL,
        0, 0,NULL // for internal use
    },

    // Aging�� �����ؾ��� OID ����.
    // ULong  mAgingRequestOIDCnt;
    // BUG-17371 [MMDB] Aging�� �и���� System�� ������ �� Aging�� �и��� ������
    // �� ��ȭ��.
    // getMinSCN ������, MinSCN������ �۾����� ���� Ƚ��
    {
        (SChar*)"AGING_REQUEST_OID_CNT",
        offsetof( smaGCStatus, mAgingRequestOIDCnt ),
        IDU_FT_SIZEOF( smaGCStatus, mAgingRequestOIDCnt ),
        IDU_FT_TYPE_UBIGINT,
        NULL,
        0, 0,NULL // for internal use
    },

    // GC�� Aging�� ��ģ OID ����.
    // ULong  mAgingProcessedOIDCnt;
    {
        (SChar*)"AGING_PROCESSED_OID_CNT",
        offsetof( smaGCStatus, mAgingProcessedOIDCnt ),
        IDU_FT_SIZEOF( smaGCStatus, mAgingProcessedOIDCnt ),
        IDU_FT_TYPE_UBIGINT,
        NULL,
        0, 0,NULL // for internal use
    },

    {
        (SChar*)"SLEEP_COUNT_ON_AGING_CONDITION",
        offsetof(smaGCStatus,mSleepCountOnAgingCondition),
        IDU_FT_SIZEOF(smaGCStatus,mSleepCountOnAgingCondition),
        IDU_FT_TYPE_UBIGINT,
        NULL,
        0, 0,NULL // for internal use
    },

    //  ���� �������� Delete Thread�� ����
    {
        (SChar*)"THREAD_COUNT",
        offsetof(smaGCStatus,mThreadCount),
        IDU_FT_SIZEOF(smaGCStatus,mThreadCount),
        IDU_FT_TYPE_UINTEGER,
        NULL,
        0, 0,NULL // for internal use
    },
    {
        NULL,
        0,
        0,
        IDU_FT_TYPE_CHAR,
        NULL,
        0, 0,NULL // for internal use
    }
};

//X$MEMGC_DELTHR
iduFixedTableDesc gMemDeleteThrTableDesc =
{
    (SChar *) "X$MEMGC_DELTHR",
    smaFT::buildRecordForDelThr,
    gMemDelThrTableColDesc,
    IDU_STARTUP_META,
    0,
    0,
    IDU_FT_DESC_TRANS_NOT_USE,
    NULL
};


IDE_RC  smaFT::buildRecordForLogicalAger(idvSQL      * /*aStatistics*/,
                                         void        *aHeader,
                                         void        * /* aDumpObj */,
                                         iduFixedTableMemory *aMemory)
{
    smaGCStatus  sLogicalAgerStatus;
    SChar        sGCName[SMA_GC_NAME_LEN];
    UInt         i;

    idlOS::memset(&sLogicalAgerStatus,0x00, ID_SIZEOF(smaGCStatus));

    // 1. alloc record buffer.

    // 2. assign.....
    idlOS::strcpy(sGCName,"MEM_LOGICAL_AGER");
    sLogicalAgerStatus.mGCName = sGCName;

    smmDatabase::getViewSCN(&(sLogicalAgerStatus.mSystemViewSCN));

    SMX_GET_MIN_MEM_VIEW( &(sLogicalAgerStatus.mMiMemSCNInTxs),
                          &(sLogicalAgerStatus.mOldestTx) );

    // ATL�� �ּ� memory view�� ���Ѵ�,
    // �� active transaction�� ���� ����̴�.
    // �̷����� system view SCN�� setting�Ͽ�,
    // ���� GC alert�� �߻����� �ʵ����Ѵ�.
    if( SM_SCN_IS_INFINITE(sLogicalAgerStatus.mMiMemSCNInTxs) )
    {
        SM_GET_SCN(&(sLogicalAgerStatus.mMiMemSCNInTxs),
                   &(sLogicalAgerStatus.mSystemViewSCN));

        // ������ �ʴ� Ʈ����� ���̵�(0)�� �����Ѵ�.
        sLogicalAgerStatus.mOldestTx = 0;
    }//if

    /* BUG-47367 OID�� ���ο� MinSCN�� ��� List�� Ž���Ͽ��� �˼� �ִ�.
     * �񱳸� ���� �⺻������ SystemViewSCN�� �����Ѵ�. */
    // Empty OIDList
    sLogicalAgerStatus.mIsEmpty = ID_TRUE;
    // SCN GAP�� 0�� �ϱ� ���Ͽ� ���� �ý���ViewSCN.
    SM_GET_SCN(&(sLogicalAgerStatus.mSCNOfTail),
               &(sLogicalAgerStatus.mSystemViewSCN));

    for( i = 0; i < smaLogicalAger::mListCnt ; i++ )
    {
        if ( smaLogicalAger::mTailLogicalAger[i] != NULL )
        {
            sLogicalAgerStatus.mIsEmpty = ID_FALSE;
            if ( SM_SCN_IS_LE( &(smaLogicalAger::mTailLogicalAger[i]->mSCN),
                               &(sLogicalAgerStatus.mSCNOfTail) ) )
            {
                SM_SET_SCN( &(sLogicalAgerStatus.mSCNOfTail), 
                            &(smaLogicalAger::mTailLogicalAger[i]->mSCN) );
            }
        }
    }

    ID_SERIAL_BEGIN( sLogicalAgerStatus.mGcOIDCnt  = 
                     smaLogicalAger::mHandledCount );
    ID_SERIAL_END( sLogicalAgerStatus.mAddOIDCnt = 
                   smaLogicalAger::mAddCount );
    sLogicalAgerStatus.mSleepCountOnAgingCondition = smaLogicalAger::mSleepCountOnAgingCondition;
    sLogicalAgerStatus.mThreadCount          = smaLogicalAger::mRunningThreadCount;

    sLogicalAgerStatus.mAgingRequestOIDCnt   = smaLogicalAger::mAgingRequestOIDCnt;
    sLogicalAgerStatus.mAgingProcessedOIDCnt = smaLogicalAger::mAgingProcessedOIDCnt;

    // 3. build record for fixed table.

    IDE_TEST(iduFixedTable::buildRecord(aHeader,
                                        aMemory,
                                        (void *) &sLogicalAgerStatus)
                 != IDE_SUCCESS);


    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


static iduFixedTableColDesc  gMemLogicalAgerTableColDesc[]=
{

    {
        (SChar*)"GC_NAME",
        offsetof(smaGCStatus, mGCName),
        SMA_GC_NAME_LEN,
        IDU_FT_TYPE_VARCHAR|IDU_FT_TYPE_POINTER ,
        NULL,
        0, 0,NULL // for internal use
    },

    // ���� systemViewSCN.
    // smSCN  mSystemViewSCN;
    {
        (SChar*)"CurrSystemViewSCN",
        offsetof(smaGCStatus,mSystemViewSCN),
        29,
        IDU_FT_TYPE_VARCHAR,
        smiFixedTable::convertSCN,
        0, 0,NULL // for internal use
    },

    // smSCN  mMiMemSCNInTxs;
    {
        (SChar*)"minMemSCNInTxs",
        offsetof(smaGCStatus,mMiMemSCNInTxs),
        29,
        IDU_FT_TYPE_VARCHAR,
        smiFixedTable::convertSCN,
        0, 0,NULL // for internal use
    },

    // BUG-24821 V$TRANSACTION�� LOB���� MinSCN�� ��µǾ�� �մϴ�.
    // smTID  mOldestTx;
    {
        (SChar*)"oldestTx",
        offsetof(smaGCStatus,mOldestTx),
        IDU_FT_SIZEOF(smaGCStatus,mOldestTx),
        IDU_FT_TYPE_UBIGINT,  // BUG-47379 unsigned int -> big int
        NULL,
        0, 0,NULL // for internal use
    },

    // LogicalAger, DeleteThread�� Tail�� commitSCN or
    // old key���� ������ SCN.
    // smSCN  mSCNOfTail;
    {
        (SChar*)"SCNOfTail",
        offsetof(smaGCStatus,mSCNOfTail),
        29,
        IDU_FT_TYPE_VARCHAR,
        smiFixedTable::convertSCN,
        0, 0,NULL // for internal use
    },

    // OIDList�� ��� �ִ��� boolean
    // idBool mIsEmpty;
    {
        (SChar*)"IS_EMPTY_OIDLIST",
        offsetof(smaGCStatus,mIsEmpty),
        IDU_FT_SIZEOF(smaGCStatus,mIsEmpty),
        IDU_FT_TYPE_UBIGINT,
        NULL,
        0, 0,NULL // for internal use
    },

    // add�� OID List ����.
    // ULong  mAddOIDCnt;
    {
        (SChar*)"ADD_OID_CNT",
        offsetof(smaGCStatus,mAddOIDCnt),
        IDU_FT_SIZEOF(smaGCStatus,mAddOIDCnt),
        IDU_FT_TYPE_UBIGINT,
        NULL,
        0, 0,NULL // for internal use
    },

    // GCó������ OID List ����.
    // ULong  mGcOIDCnt;
    {
        (SChar*)"GC_OID_CNT",
        offsetof(smaGCStatus,mGcOIDCnt),
        IDU_FT_SIZEOF(smaGCStatus,mGcOIDCnt),
        IDU_FT_TYPE_UBIGINT,
        NULL,
        0, 0,NULL // for internal use
    },


    // Aging�� �����ؾ��� OID ����.
    // ULong  mAgingRequestOIDCnt;
    {
        (SChar*)"AGING_REQUEST_OID_CNT",
        offsetof( smaGCStatus, mAgingRequestOIDCnt ),
        IDU_FT_SIZEOF( smaGCStatus, mAgingRequestOIDCnt ),
        IDU_FT_TYPE_UBIGINT,
        NULL,
        0, 0,NULL // for internal use
    },

    // GC�� Aging�� ��ģ OID ����.
    // ULong  mAgingProcessedOIDCnt;
    {
        (SChar*)"AGING_PROCESSED_OID_CNT",
        offsetof( smaGCStatus, mAgingProcessedOIDCnt ),
        IDU_FT_SIZEOF( smaGCStatus, mAgingProcessedOIDCnt ),
        IDU_FT_TYPE_UBIGINT,
        NULL,
        0, 0,NULL // for internal use
    },

    // BUG-17371 [MMDB] Aging�� �и���� System�� ������ �� Aging�� �и��� ������
    // �� ��ȭ��.
    // getMinSCN ������, MinSCN������ �۾����� ���� Ƚ��
    {
        (SChar*)"SLEEP_COUNT_ON_AGING_CONDITION",
        offsetof(smaGCStatus,mSleepCountOnAgingCondition),
        IDU_FT_SIZEOF(smaGCStatus,mSleepCountOnAgingCondition),
        IDU_FT_TYPE_UBIGINT,
        NULL,
        0, 0,NULL // for internal use
    },

    // BUG-17371 [MMDB] Aging�� �и���� System�� ������ ��
    //                  Aging�� �и��� ������ �� ��ȭ��.
    //
    // => Logical Ager�� ������ ���ķ� �����Ͽ� �����ذ�
    //     ���� �������� Logical Ager Thread�� ����
    {
        (SChar*)"THREAD_COUNT",
        offsetof(smaGCStatus,mThreadCount),
        IDU_FT_SIZEOF(smaGCStatus,mThreadCount),
        IDU_FT_TYPE_UINTEGER,
        NULL,
        0, 0,NULL // for internal use
    },

    {
        NULL,
        0,
        0,
        IDU_FT_TYPE_CHAR,
        NULL,
        0, 0,NULL // for internal use
    }
};

//X$MEMGC_L_AGER
iduFixedTableDesc gMemLogicalAgerTableDesc =
{
    (SChar *) "X$MEMGC_L_AGER",
    smaFT::buildRecordForLogicalAger,
    gMemLogicalAgerTableColDesc,
    IDU_STARTUP_META,
    0,
    0,
    IDU_FT_DESC_TRANS_NOT_USE,
    NULL
};

