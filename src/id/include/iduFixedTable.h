/***********************************************************************
 * Copyright 1999-2001, ALTIBase Corporation or its subsidiaries.
 * All rights reserved.
 **********************************************************************/

/***********************************************************************
 * $Id: iduFixedTable.h 74637 2016-03-07 06:01:43Z donovan.seo $
 **********************************************************************/
#ifndef _O_IDU_FIXED_TABLE_H_
# define _O_IDU_FIXED_TABLE_H_ 1

#include <iduFixedTableDef.h>
#include <iduMemory.h>

class iduFixedTable
{
    static iduFixedTableDesc  mTableHead; // Dummy
    static iduFixedTableDesc *mTableTail;
    static UInt               mTableCount;
    static UInt               mColumnCount;
    static iduFixedTableAllocRecordBuffer mAllocCallback;
    static iduFixedTableBuildRecord       mBuildCallback;
    static iduFixedTableCheckKeyRange     mCheckKeyRangeCallback;

public:
    // BUG-36588
    static void   initialize( void );
    static void   finalize( void );

    static void   registFixedTable(iduFixedTableDesc *);
    static void   setCallback(iduFixedTableAllocRecordBuffer aAlloc,
                              iduFixedTableBuildRecord       aBuild,
                              iduFixedTableCheckKeyRange     aCheckKeyRange);
    static iduFixedTableDesc* getTableDescHeader()
    {
        return mTableHead.mNext;
    }
    static IDE_RC allocRecordBuffer(void *aHeader,
                                    UInt aRecordCount,
                                    void **aBuffer)
    {
        return mAllocCallback(aHeader, aRecordCount, aBuffer);
    }

    static IDE_RC buildRecord(void                *aHeader,
                              iduFixedTableMemory *aMemory,
                              void      *aObj)
    {
        return mBuildCallback(aHeader, aMemory,  aObj);
    }

    static idBool checkKeyRange( iduFixedTableMemory   * aMemory,
                                 iduFixedTableColDesc  * aColDesc,
                                 void                 ** aObj )
    {
        return mCheckKeyRangeCallback( aMemory, aColDesc, aObj );
    }

    // �� �Լ��� X$TAB�� �����ϱ� ���� ������ ���� build �Լ�
    static IDE_RC buildRecordForSelfTable( idvSQL      *aStatistics,
                                           void        *aHeader,
                                           void        *aDumpObj,
                                           iduFixedTableMemory *aMemory );

    // �� �Լ��� X$COL�� �����ϱ� ���� ������ ���� build �Լ�
    static IDE_RC buildRecordForSelfColumn( idvSQL      *aStatistics,
                                            void        *aHeader,
                                            void        *aDumpObj,
                                            iduFixedTableMemory *aMemory );
};

// �ڵ����� Fixed Table�� �����ϰ�, ����ϵ��� �ϴ� Ŭ����

/*
 * ===================== WARNING!!!!!!!!!!!!! ====================
 * ObjName�� iduFixedTableDesc�� �����ʹ�ſ� ��ü�� �Ѱܾ� �Ѵ�.
 * �ֳ��ϸ�, ����ȭ�� ������ �ټ��� ���ǵ� iduFixedTableDesc��
 * ���� ��ü�� �̸��� ���� �ٸ� ���·� �����ϱ� ����
 * �������μ����� ## ����� �̿��ϵ��� �Ͽ��� �����̴�.
 * �ڵ����� ������ Ÿ������ �Ѿ���� �Ͽ��� ������
 * ��ü�� �̸��� �Ѱܾ� �Ѵ�.
 * ===================== WARNING!!!!!!!!!!!!! ====================

 EXAMPLE)

 iduFixedTableDesc gSampleTable =
 {
 (SChar *)"SampleFixedTable",
 buildRecordCallback,
 gSampleTableColDesc,
 IDU_STARTUP_INIT,
 0, NULL
 };

 IDU_FIXED_TABLE_DEFINE(gSampleTable);   <== OK

 IDU_FIXED_TABLE_DEFINE(&gSampleTable);  <== Wrong!!!

*/

#define IDU_FIXED_TABLE_LOGGING_POINTER()       \
    {                                           \
        mRollbackBeginRecord  = mBeginRecord;   \
        mRollbackBeforeRecord = mBeforeRecord;  \
        mRollbackCurrRecord   = mCurrRecord;    \
    }

#define IDU_FIXED_TABLE_ROLLBACK_POINTER()      \
    {                                           \
        mBeginRecord  = mRollbackBeginRecord;   \
        mBeforeRecord = mRollbackBeforeRecord;  \
        mCurrRecord   = mRollbackCurrRecord;    \
    }

class iduFixedTableMemory
{
    iduMemory mMemory;
    iduMemory *mMemoryPtr;
    // Current Usage Pointer
    UChar    *mBeginRecord;
    UChar    *mBeforeRecord; // or final record..it depends on operation.
    UChar    *mCurrRecord;    // or final record..it depends on operation.

    // For Rollback in filter-false case.
    UChar    *mRollbackBeginRecord;
    UChar    *mRollbackBeforeRecord;
    UChar    *mRollbackCurrRecord;

    void             *mContext;
    iduMemoryStatus   mMemStatus;

    idBool            mUseExternalMemory;
public:
    iduFixedTableMemory() {}
    ~iduFixedTableMemory() {}

    IDE_RC initialize( iduMemory * aMemory );
    IDE_RC restartInit();
    IDE_RC allocateRecord(UInt aSize, void **aMem);
    IDE_RC initRecord(void **aMem);
    void   freeRecord();
    void   resetRecord();

    IDE_RC destroy();

    UChar *getBeginRecord()            { return mBeginRecord;  }
    UChar *getCurrRecord()             { return mCurrRecord;   }
    UChar *getBeforeRecord()           { return mBeforeRecord; }
    void  *getContext()                { return mContext;      }
    void   setContext(void * aContext) { mContext = aContext;  }
    idBool useExternalMemory()         { return mUseExternalMemory;  }
};


class iduFixedTableRegistBroker
{
public:
    iduFixedTableRegistBroker(iduFixedTableDesc *aDesc);
};

#define IDU_FIXED_TABLE_DEFINE_EXTERN(ObjName)                  \
    extern iduFixedTableDesc ObjName;

#define IDU_FIXED_TABLE_DEFINE(ObjName)                         \
    extern iduFixedTableDesc ObjName;                           \
    static iduFixedTableRegistBroker ObjName##ObjName(&ObjName)

// PROJ-1726
// ���� ��⿡�� <���>im::initSystemTable ����
// �������� fixed table �� ����ϱ� ���� ���.

#define IDU_FIXED_TABLE_DEFINE_RUNTIME(ObjName)                 \
    iduFixedTable::registFixedTable(&ObjName);

#endif /* _O_IDU_FIXED_TABLE_H_ */
