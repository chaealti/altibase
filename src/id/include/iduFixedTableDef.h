/***********************************************************************
 * Copyright 1999-2001, ALTIBase Corporation or its subsidiaries.
 * All rights reserved.
 **********************************************************************/

/***********************************************************************
 * $Id: iduFixedTableDef.h 82166 2018-02-01 07:26:29Z ahra.cho $
 **********************************************************************/

#ifndef _O_IDU_FIXED_TABLE_DEF_H_
# define _O_IDU_FIXED_TABLE_DEF_H_ 1

#include <idl.h>


/* ------------------------------------------------
 *  A4�� ���� Startup Flags
 * ----------------------------------------------*/
typedef enum
{
    IDU_STARTUP_INIT         = 0,
    IDU_STARTUP_PRE_PROCESS  = 1,  /* for DB Creation    */
    IDU_STARTUP_PROCESS      = 2,  /* for DB Creation    */
    IDU_STARTUP_CONTROL      = 3,  /* for Recovery       */
    IDU_STARTUP_META         = 4,  /* for upgrade meta   */
    IDU_STARTUP_SERVICE      = 5,  /* for normal service  */
    IDU_STARTUP_SHUTDOWN     = 6,  /* for normal shutdown */
    IDU_STARTUP_DOWNGRADE    = 7,  /* for downgrade meta   */

    IDU_STARTUP_MAX
} iduStartupPhase;

/* ----------------------------------------------------------------------------
 *  Fixed Table���� �����ϴ� ����Ÿ Ÿ�� �� Macros
 * --------------------------------------------------------------------------*/

typedef UShort iduFixedTableDataType;

#define IDU_FT_TYPE_MASK       (0x00FF)
#define IDU_FT_TYPE_CHAR       (0x0000)
#define IDU_FT_TYPE_BIGINT     (0x0001)
#define IDU_FT_TYPE_SMALLINT   (0x0002)
#define IDU_FT_TYPE_INTEGER    (0x0003)
#define IDU_FT_TYPE_DOUBLE     (0x0004)
#define IDU_FT_TYPE_UBIGINT    (0x0005)
#define IDU_FT_TYPE_USMALLINT  (0x0006)
#define IDU_FT_TYPE_UINTEGER   (0x0007)
#define IDU_FT_TYPE_VARCHAR    (0x0008)
#define IDU_FT_TYPE_POINTER    (0x1000)
/* BUG-43006 Fixed Table Indexing Filter */
#define IDU_FT_COLUMN_INDEX    (0x2000)

#define IDU_FT_SIZEOF_BIGINT      ID_SIZEOF(SLong)
#define IDU_FT_SIZEOF_SMALLINT    ID_SIZEOF(SShort)
#define IDU_FT_SIZEOF_INTEGER     ID_SIZEOF(SInt)
#define IDU_FT_SIZEOF_UBIGINT     ID_SIZEOF(SLong)
#define IDU_FT_SIZEOF_USMALLINT   ID_SIZEOF(SShort)
#define IDU_FT_SIZEOF_UINTEGER    ID_SIZEOF(SInt)
#define IDU_FT_SIZEOF_DOUBLE      ID_SIZEOF(SDouble)

#define IDU_FT_GET_TYPE(a)     ( a & IDU_FT_TYPE_MASK)
#define IDU_FT_IS_POINTER(a)   ((a & IDU_FT_TYPE_POINTER) != 0)
#define IDU_FT_MAX_PATH_LEN       (1024)
/* BUG-43006 Fixed Table Indexing Filter */
#define IDU_FT_IS_COLUMN_INDEX(a) ((a & IDU_FT_COLUMN_INDEX) != 0)

#define IDU_FT_OFFSETOF(s, m) \
    (UInt)((SChar*)&(((s *) NULL)->m) - ((SChar*)NULL))

#define IDU_FT_SIZEOF(s, m)   ID_SIZEOF( ((s *)NULL)->m )
    
#define IDU_FT_SIZEOF_CHAR(s, m)  IDU_FT_SIZEOF(s, m)

/* ----------------------------------------------------------------------------
 *  Fixed Table���� �����ϴ� Struct
 * --------------------------------------------------------------------------*/
class iduFixedTableMemory;
struct idvSQL;

typedef IDE_RC (*iduFixedTableBuildFunc)( idvSQL              *aStatistics,
                                          void                *aHeader,
                                          void                *aDumpObj, // PROJ-1618
                                          iduFixedTableMemory *aMemory);

// smnf�� callback �� ȣ���Ͽ�, �̸� valid record ���� �˻��Ѵ�.
typedef IDE_RC (*iduFixedTableFilterCallback)(idBool *aResult,
                                              void   *aIterator,
                                              void   *aRecord);
/*
 *  DataType�� �� �� ���� ���, Char Ÿ������ �����ϱ� ���� Callback
 *
 *      aObj:       ��ü�� ������
 *      aObjOffset: ��ü ����� Offset ��ġ (���� �� ��ġ�� ��ȯ��)
 *      aBuf:       �÷��� ������� ��ġ
 *      aBufSize:   ��� �÷��� �޸� ����
 *      aDataType : ��� ����Ÿ Ÿ�� : varchar�� ��� 0���� ������,
 *                                     Char�� ��� ' '�� ä��.
 *      ���ϰ� : ����� ��Ʈ���� ũ�� : Char�� ��� �ش� Ÿ�Ա����̰�,
 *                                      Varchar�� ��� ��ȯ�� ����Ÿ ������.
 *
 */
typedef UInt   (*iduFixedTableConvertCallback)(void        *aBaseObj,
                                               void        *aMember,
                                               UChar       *aBuf,
                                               UInt         aBufSize);

typedef IDE_RC (*iduFixedTableAllocRecordBuffer)(void *aHeader,
                                                 UInt aRecordCount,
                                                 void **aBuffer);

typedef IDE_RC (*iduFixedTableBuildRecord)(void                *aHeader,
                                           iduFixedTableMemory *aMemory,
                                           void                *aObj);

typedef struct iduFixedTableColDesc
{
    SChar                       *mName;
    //PR-14300  Fixed Table���� 64K�� �Ѵ� ��ü�� �ʵ尪��
    // ������ ������ ����ϴ� ���� �ذ� ; mOffset�� UInt�� ����
    UInt                         mOffset; // Object������ ��ġ
    UInt                         mLength; // Object���� �����ϴ� ũ��
    iduFixedTableDataType        mDataType;
    iduFixedTableConvertCallback mConvCallback;

    /*
     * �Ʒ��� ���� Row�� ǥ���� ���� ũ��� offset�� ǥ���Ѵ�.
     * A3�� ���, Desc�κ���  SQL�� ǥ���Ͽ� Meta Table�� �����
     * �ϱ� ������ �ڵ������� ������ ����������,
     * A4�� ���� Schema�� ���� ����� �ؾ� �ϱ� ������
     * �Ʒ��� ���� �ʱ�ȭ �������� �����Ǿ�� �Ѵ�.
     */
    UInt                    mColOffset; // Row �������� ��ġ
    UInt                    mColSize;   // Row �������� Column ũ��
    SChar                  *mTableName; // Table Name

} iduFixedTableColDesc;

typedef idBool (*iduFixedTableCheckKeyRange)( iduFixedTableMemory  * aMemory,
                                              iduFixedTableColDesc * aColDesc,
                                              void                ** aObj );
typedef enum
{
    IDU_FT_DESC_TRANS_NOT_USE = 0,
    IDU_FT_DESC_TRANS_USE
} iduFtDescTransUse;

typedef struct iduFixedTableDesc
{
    SChar                   *mTableName;   // Fixed Table �̸�
    iduFixedTableBuildFunc   mBuildFunc;   // Record ���� callback
    iduFixedTableColDesc    *mColDesc;     // column ����
    iduStartupPhase          mEnablePhase; // �ٴܰ� �������� ��� ����?
    UInt                     mSlotSize;    // Row�� ũ�� : init �� ����
    UShort                   mColCount;    // �� �÷��� ���� : init �� ����
    iduFtDescTransUse        mUseTrans;    // Transaction ��� ����
    iduFixedTableDesc       *mNext;
} iduFixedTableDesc;


#endif /* _O_IDU_FIXED_TABLE_DEF_H_ */
