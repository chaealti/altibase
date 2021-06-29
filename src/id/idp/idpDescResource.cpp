/***********************************************************************
 * Copyright 1999-2000, RTBase Corporation or its subsidiaries.
 * All rights reserved.
 **********************************************************************/

/***********************************************************************
 * $Id: idpDescResource.cpp 90787 2021-05-07 00:50:48Z ahra.cho $
 *
 * Description:
 *
 * A3���� A4�� ���鼭, ������ ��.
 * 1. ���� ���� : LOCK_ESCALATION_MEMORY_SIZE (M���� �׳� byte�� )
 *                �׳� ũ��� ���.  1024 * 1024 ��ŭ ���ϱ� ����.
 *
 **********************************************************************/

#include <idl.h>
#include <ide.h>
#include <idu.h>

#include <idpUInt.h>
#include <idpSInt.h>
#include <idpULong.h>
#include <idpSLong.h>
#include <idpString.h>

#include <idp.h>
#include <idvTime.h>
#include <idvAuditDef.h>

// To Fix PR-12035
// Page Size = 8K
// �ش� ���� smDef.h �� SD_PAGE_SIZE�� ������ ���̾�� �Ѵ�.
#if defined(SMALL_FOOTPRINT)
#define IDP_SD_PAGE_SIZE  ( 4 * 1024 )
#else
#define IDP_SD_PAGE_SIZE  ( 8 * 1024 )
#endif
#define IDP_DRDB_DATAFILE_MAX_SIZE  ID_ULONG( 32 * 1024 * 1024 * 1024 ) // 32G

// Direct I/O �ִ� Pageũ�� = 8K
// smDef.h�� SM_MAX_DIO_PAGE_SIZE �� ������ ���̾�� �Ѵ�.
#define IDP_MAX_DIO_PAGE_SIZE ( 8 * 1024 )

// Memory table�� page ũ��
// SM_PAGE_SIZE�� ������ ���̾�� �Ѵ�.
#if defined(SMALL_FOOTPRINT)
#define IDP_SM_PAGE_SIZE  ( 4 * 1024 )
#else
#define IDP_SM_PAGE_SIZE  ( 32 * 1024 )
#endif

// LFG �ִ밪 = 1
// smDef.h�� SM_LFG_COUNT�� ������ ���̾�� �Ѵ�.
#if defined(SMALL_FOOTPRINT)
#define IDP_MAX_LFG_COUNT 1
#else
/* Task 6153 */
#define IDP_MAX_LFG_COUNT 1
#endif

/* Task 6153 */
// PAGE_LIST �ִ밪 = 32
// smDef.h�� SM_MAX_PAGELIST_COUNT�� ������ ���̾�� �Ѵ�.
#if defined(SMALL_FOOTPRINT)
#define IDP_MAX_PAGE_LIST_COUNT 1
#else
#define IDP_MAX_PAGE_LIST_COUNT 32
#endif



// Direct I/O �ִ� Pageũ�� = 8K
// smDef.h�� SM_MAX_DIO_PAGE_SIZE �� ������ ���̾�� �Ѵ�.
#define IDP_MAX_DIO_PAGE_SIZE ( 8 * 1024 )

// EXPAND_CHUNK_PAGE_COUNT ������Ƽ�� �⺻��
// �� ���� �������� SHM_PAGE_COUNT_PER_KEY ������Ƽ��
// �⺻���� �Բ� ����ȴ�.
#if defined(SMALL_FOOTPRINT)
#define IDP_DEFAULT_EXPAND_CHUNK_PAGE_COUNT (32)
#else
#define IDP_DEFAULT_EXPAND_CHUNK_PAGE_COUNT (128)
#endif

// �ִ� Ʈ����� ����
#define IDP_MAX_TRANSACTION_COUNT (16384) // 2^14

// �ּ� Ʈ����� ����
// BUG-28565 Prepared Tx�� Undo ���� free trans list rebuild �� ������ ����
// �ּ� transaction table size�� 16���� ����(���� 0)
#define IDP_MIN_TRANSACTION_COUNT (16)

// LOB In Mode Max Size (BUG-30101)
// smDef.h�� SM_LOB_MAX_IN_ROW_SIZE�� ������ ���̾�� �Ѵ�.
#define IDP_MAX_LOB_IN_ROW_SIZE (4000)

/* PROJ-2109 : Remove the bottleneck of alloc/free stmts. */
// This macro returns the number of processor within the value of MIN/MAX.
#define IDP_LIMITED_CORE_COUNT( MIN, MAX ) ( (((idlVA::getProcessorCount() < MIN)? MIN : idlVA::getProcessorCount()) < MAX) ? \
            ((idlVA::getProcessorCount() < MIN)? MIN : idlVA::getProcessorCount()) : MAX )

/* ------------------------------------------------
 *  Defined Default Mutex/Latch Type
 *  (Posix, Native)
 *
 *    IDU_MUTEX_KIND_POSIX = 0,
 *    IDU_MUTEX_KIND_NATIVE,
 *
 *    IDU_LATCH_TYPE_POSIX = 0,
 *    IDU_LATCH_TYPE_NATIVE,
 *
 * ----------------------------------------------*/

/*
  for MM

  alter system set IDU_SOURCE_INFO = n ; // callback
  alter system set AUTO_COMMIT = n ;
  alter system set __SHOW_ERROR_STACK = n ;
 */
static void *idpGetObjMem(UInt aSize)
{
    void    *sMem;
    sMem = (void *)iduMemMgr::mallocRaw(aSize);

    IDE_ASSERT(sMem != NULL);
    return sMem;
}

#define IDP_DEF(type, name, attr, min, max, default) \
{\
    idpBase *sType;\
    void    *sMem;\
    sMem = idpGetObjMem(sizeof(idp##type));\
    sType = new (sMem) idp##type(name, attr | IDP_ATTR_TP_##type, (min), (max), (default));     \
    IDE_TEST(idp::regist(sType) != IDE_SUCCESS);\
}

/*
 * Properties about database link are added to here.
 */ 
static IDE_RC registDatabaseLinkProperties( void )
{

    IDP_DEF(UInt, "DK_MSGLOG_FLAG",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, ID_UINT_MAX, 0);

    IDP_DEF(String, "DK_MSGLOG_FILE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_SK_PATH     |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, IDP_MAX_PROP_STRING_LEN, (SChar *)PRODUCT_PREFIX"altibase_dk.log");

    IDP_DEF(UInt, "DK_MSGLOG_SIZE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, ID_UINT_MAX, (10 * 1024 * 1024));

    IDP_DEF(UInt, "DK_MSGLOG_COUNT",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, ID_UINT_MAX, 10);

    IDP_DEF(UInt, "DBLINK_ENABLE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 0);

    IDP_DEF(UInt, "DBLINK_DATA_BUFFER_BLOCK_COUNT",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, ID_UINT_MAX, 128);

    IDP_DEF(UInt, "DBLINK_DATA_BUFFER_BLOCK_SIZE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, ID_UINT_MAX, 2);

    IDP_DEF(UInt, "DBLINK_DATA_BUFFER_ALLOC_RATIO",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 100, 50);

    IDP_DEF(UInt, "DBLINK_GLOBAL_TRANSACTION_LEVEL",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 2, 1);

    IDP_DEF(UInt, "GLOBAL_TRANSACTION_LEVEL",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 3, 1);

    IDP_DEF(UInt, "DBLINK_REMOTE_STATEMENT_AUTOCOMMIT",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 0);

    IDP_DEF(UInt, "DBLINK_ALTILINKER_CONNECT_TIMEOUT",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, ID_UINT_MAX, 100);

    IDP_DEF(UInt, "DBLINK_REMOTE_TABLE_BUFFER_SIZE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            1, ID_UINT_MAX, 50);

    IDP_DEF(UInt, "DBLINK_RECOVERY_MAX_LOGFILE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, ID_UINT_MAX, 0);

     return IDE_SUCCESS;
     
     IDE_EXCEPTION_END;
     
     return IDE_FAILURE;    
}

/* ------------------------------------------------------------------
 *   �Ʒ��� �Լ��� ����� ������Ƽ�� ����Ÿ �� �� ������ ����ϸ� ��.
 *   IDP_DEF(Ÿ��, �̸�, �Ӽ�, �ּ�,�ִ�,�⺻��) ������.
 *   ���� �����Ǵ� ����Ÿ Ÿ��
 *   UInt, SInt, ULong, SLong, String
 *   �Ӽ� : �ܺ�/���� , �б�����/����, ���ϰ�/�ټ���,
 *          ������ �˻����/�˻�ź� , ����Ÿ Ÿ��
 *
 *
 *   !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!���!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!11

 *   idpString()�� ������ �� NULL�� �ѱ��� ���ÿ�. ��� ""�� �ѱ�ÿ�.

 *   !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!���!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!11

 * ----------------------------------------------------------------*/


IDE_RC registProperties()
{
    UInt sLogFileAlignSize ;
    UInt sDefaultMutexSpinCnt = 0;      //BUG-46911

    /* !!!!!!!!!! REGISTRATION AREA BEGIN !!!!!!!!!! */

    // ==================================================================
    // ID Properties
    // ==================================================================
    IDP_DEF(UInt, "PORT_NO",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            1024,   /* min */
            65535,  /* max */
            20300); /* default */

    IDP_DEF(UInt, "IPC_CHANNEL_COUNT",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 65535, 0);

#if defined(ALTI_CFG_OS_LINUX)
    IDP_DEF(UInt, "IPCDA_CHANNEL_COUNT",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 65535, 0);
#else
    IDP_DEF(UInt, "IPCDA_CHANNEL_COUNT",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 0, 0);
#endif

    /*PROJ-2616*/
    IDP_DEF(UInt, "IPCDA_SERVER_SLEEP_TIME",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1000, 400);

    /*PROJ-2616*/
    IDP_DEF(UInt, "IPCDA_MESSAGEQ_MODE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 0);

    /*PROJ-2616*/
    IDP_DEF(UInt, "IPCDA_SERVER_MESSAGEQ_TIMEOUT",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            1, 65535, 100);

    /*PROJ-2616*/
    IDP_DEF(UInt, "IPCDA_DATABLOCK_SIZE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            32, 102400, 20480);

    IDP_DEF(UInt, "SOURCE_INFO",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 3, 0);

    IDP_DEF(String, "UNIXDOMAIN_FILEPATH",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_SK_PATH     |
            IDP_ATTR_LC_EXTERNAL | /* BUG-43142 */
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, IDP_MAX_PROP_STRING_LEN, (SChar *)"?"IDL_FILE_SEPARATORS"trc"IDL_FILE_SEPARATORS"cm-unix");

    /* BUG-35332 The socket files can be moved */
    IDP_DEF(String, "IPC_FILEPATH",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_SK_PATH     |
            IDP_ATTR_LC_EXTERNAL | /* BUG-43142 */
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, IDP_MAX_PROP_STRING_LEN, (SChar *)"?"IDL_FILE_SEPARATORS"trc"IDL_FILE_SEPARATORS"cm-ipc");

    IDP_DEF(String, "IPCDA_FILEPATH",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_SK_PATH     |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, IDP_MAX_PROP_STRING_LEN, (SChar *)"?"IDL_FILE_SEPARATORS"trc"IDL_FILE_SEPARATORS"cm-ipcda");

    IDP_DEF(UInt, "NET_CONN_IP_STACK",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 2, 0);

    /*
     * PROJ-2473 SNMP ����
     *
     * LINUX�� SNMP�� ��������.
     */
#if defined(ALTI_CFG_OS_LINUX)
    IDP_DEF(UInt, "SNMP_ENABLE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 0);
#else
    IDP_DEF(UInt, "SNMP_ENABLE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 0, 0);
#endif

    IDP_DEF(UInt, "SNMP_PORT_NO",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            1024, 65535, 20400);

    IDP_DEF(UInt, "SNMP_TRAP_PORT_NO",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            1024, 65535, 20500);

    /* milliseconds */
    IDP_DEF(UInt, "SNMP_RECV_TIMEOUT",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            1, ID_UINT_MAX, 1000);

    /* milliseconds */
    IDP_DEF(UInt, "SNMP_SEND_TIMEOUT",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            1, ID_UINT_MAX, 100);

    IDP_DEF(UInt, "SNMP_MSGLOG_FLAG",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, ID_UINT_MAX, 3);

    IDP_DEF(String, "SNMP_MSGLOG_FILE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_SK_PATH     |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, IDP_MAX_PROP_STRING_LEN, (SChar *)PRODUCT_PREFIX"altibase_snmp.log");

    IDP_DEF(UInt, "SNMP_MSGLOG_SIZE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, ID_UINT_MAX, (10 * 1024 * 1024));

    IDP_DEF(UInt, "SNMP_MSGLOG_COUNT",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, ID_UINT_MAX, 10);

    /* 
     * SNMP ALARM Setting - SNMP SET���θ� ������ �� �ִ�. �׷��� READONLY�̴�.
     *
     * 0: no send alarm,
     * 1: send alarm
     */
    IDP_DEF(UInt, "SNMP_ALARM_QUERY_TIMEOUT",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 1);

    IDP_DEF(UInt, "SNMP_ALARM_UTRANS_TIMEOUT",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 1);

    IDP_DEF(UInt, "SNMP_ALARM_FETCH_TIMEOUT",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 1);

    /* 
     * 0: no send alarm,
     * x: xȸ �������� ���а� �Ͼ ��� 
     */
    IDP_DEF(UInt, "SNMP_ALARM_SESSION_FAILURE_COUNT",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, UINT_MAX, 3);

    IDP_DEF(UInt, "SHM_POLICY",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 0);

#ifdef COMPILE_64BIT
    IDP_DEF(ULong, "SHM_MAX_SIZE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            64 * 1024 * 1024 /* 64 MB */, ID_ULONG_MAX, ID_ULONG(8 * 1024 * 1024 * 1024) /* 8G */);
#else
    IDP_DEF(ULong, "SHM_MAX_SIZE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            64 * 1024 * 1024 /* 64 MB */,
            ID_ULONG(4 * 1024 * 1024 * 1024) /* 4G */,
            ID_ULONG(4 * 1024 * 1024 * 1024) /* 4G */);
#endif

    IDP_DEF(UInt, "SHM_LOGGING",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0,  /* min */
            1,  /* max */
            1); /* default */

    IDP_DEF(UInt, "__USE_DUMP_CALLSTACKS",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 0);    //min,max,default

    IDP_DEF(UInt, "SHM_LOCK",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0,  /* min */
            1,  /* max */
            0); /* default */

    IDP_DEF(UInt, "SHM_LATCH_SPIN_LOCK_COUNT",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0,  /* min */
            ID_UINT_MAX,  /* max */
            1 );

    IDP_DEF(UInt, "SHM_LATCH_YIELD_COUNT",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            1,  /* min */
            ID_UINT_MAX,  /* max */
            1 );

    IDP_DEF(UInt, "SHM_LATCH_MAX_YIELD_COUNT",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            1,  /* min */
            ID_UINT_MAX,  /* max */
            1000 );

    IDP_DEF(UInt, "SHM_LATCH_SLEEP_DURATION",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0,  /* min = 0usec */
            1000000,  /* max=1000000usec=1sec */
            0 );

    IDP_DEF(UInt, "USER_PROCESS_CPU_AFFINITY",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0,  /* min */
            1,  /* max */
            0); /* default */

    //----------------------------------
    // NLS Properties
    //----------------------------------

    // fix BUG-13838
    // hangul collation ( ksc5601, ms949 )
    IDP_DEF(UInt, "NLS_COMP",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 0);

    // PROJ-1579 NCHAR
    IDP_DEF(UInt, "NLS_NCHAR_CONV_EXCP",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 0);

    // PROJ-1579 NCHAR
    IDP_DEF(UInt, "NLS_NCHAR_LITERAL_REPLACE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 0);

#if defined(ALTIBASE_PRODUCT_HDB)
    // fix BUG-21266, BUG-29501
    IDP_DEF(UInt, "DEFAULT_THREAD_STACK_SIZE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            1048576,     /* min */
            1048576 * 128,  /* max */
            10 * 1048576);  /* default 10M */
#else
    // fix BUG-21266, BUG-29501
    IDP_DEF(UInt, "DEFAULT_THREAD_STACK_SIZE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            65536,     /* min */
            10485760,  /* max */
            10485760);  /* default */
#endif

    // fix BUG-21547
    IDP_DEF(UInt, "USE_MEMORY_POOL",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0,   /* min */
            1,  /* max */
            1); /* default */

    //----------------------------------
    // For QP
    //----------------------------------

    IDP_DEF(UInt, "TRCLOG_DML_SENTENCE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_USER|
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 0);

    IDP_DEF(UInt, "TRCLOG_DETAIL_PREDICATE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_USER|
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 0);

    IDP_DEF(UInt, "TRCLOG_DETAIL_MTRNODE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_USER|
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 0);

    IDP_DEF(UInt, "TRCLOG_EXPLAIN_GRAPH",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_USER|
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 0);

    // PROJ-2179
    IDP_DEF(UInt, "TRCLOG_RESULT_DESC",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_USER|
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 0);

    // BUG-38192
    // PROJ-2402 ���� �̸� ����
    IDP_DEF(UInt, "TRCLOG_DISPLAY_CHILDREN",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 1);

    // PROJ-1358, BUG-12544
    // Min : 8, Max : 65536, Default : 128
    IDP_DEF(UInt, "QUERY_STACK_SIZE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            8, 65536, 1024);

    // PROJ-1446 Host variable�� ������ ���� ����ȭ
    IDP_DEF(UInt, "HOST_OPTIMIZE_ENABLE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 0);

    // BUG-13068 session�� open�� �� �ִ� filehandle���� ����
    // Min : 0, Max : 128, Default : 16
    IDP_DEF(UInt, "PSM_FILE_OPEN_LIMIT",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 128, 16);

    // BUG-40854
    // Min : 0, Max : 128, Default : 16
    IDP_DEF(UInt, "CONNECT_TYPE_OPEN_LIMIT",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 128, 16);

    /* BUG-41307 User Lock ���� */
    // Min : 128, Max : 10000, Default : 128
    IDP_DEF(UInt, "USER_LOCK_POOL_INIT_SIZE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            128, 100000, 128);

    /* BUG-41307 User Lock ���� */
    // Min : 0, Max : (2^32)-1, Default : 10
    IDP_DEF(UInt, "USER_LOCK_REQUEST_TIMEOUT",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, ID_UINT_MAX, 10);

    /* BUG-41307 User Lock ���� */
    // Min : 10, Max : 999999, Default : 10000
    IDP_DEF(UInt, "USER_LOCK_REQUEST_CHECK_INTERVAL",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            10, 999999, 10000);

    /* BUG-41307 User Lock ���� */
    // Min : 0, Max : 10000, Default : 10
    IDP_DEF(UInt, "USER_LOCK_REQUEST_LIMIT",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 10000, 10);
    
    // BUG-35713
    // sql�� ���� invoke�Ǵ� function���� �߻��ϴ� no_data_found ��������
    IDP_DEF(UInt, "PSM_IGNORE_NO_DATA_FOUND_ERROR",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 0);

    // PROJ-1557 varchar 32k ����
    // Min : 0, Max : 4000, Default : 32
    IDP_DEF(UInt, "MEMORY_VARIABLE_COLUMN_IN_ROW_SIZE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 4000, 32);

    // PROJ-1362 LOB
    // Min : 0, Max : 4000, Default : 64
    IDP_DEF(UInt, "MEMORY_LOB_COLUMN_IN_ROW_SIZE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, IDP_MAX_LOB_IN_ROW_SIZE, 64);

    // PROJ-1862 Disk In Mode LOB In Row Size
    IDP_DEF(UInt, "DISK_LOB_COLUMN_IN_ROW_SIZE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, IDP_MAX_LOB_IN_ROW_SIZE, 3900);

    /* PROJ-1530 PSM, Trigger���� LOB ����Ÿ Ÿ�� ���� */
    IDP_DEF(UInt, "LOB_OBJECT_BUFFER_SIZE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            32000, 104857600, 32000);

    /* PROJ-1530 PSM, Trigger���� LOB ����Ÿ Ÿ�� ���� */
    IDP_DEF(UInt, "__INTERMEDIATE_TUPLE_LOB_OBJECT_LIMIT",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            33554432, 1073741824, 33554432);

    // BUG-18851 disable transitive predicate generation
    IDP_DEF(UInt, "__OPTIMIZER_TRANSITIVITY_DISABLE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 0 );

    /* BUG-42134 Created transitivity predicate of join predicate must be reinforced. */
    IDP_DEF(UInt, "__OPTIMIZER_TRANSITIVITY_OLD_RULE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 0 );

    // PROJ-1473
    IDP_DEF(UInt, "__OPTIMIZER_PUSH_PROJECTION",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 1 );

    // BUG-23780 TEMP_TBS_MEMORY ��Ʈ ���뿩�θ� property�� ����
    IDP_DEF(UInt, "__OPTIMIZER_DEFAULT_TEMP_TBS_TYPE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 2, 0 );

    // BUG-38132 group by�� temp table �� �޸𸮷� �����ϴ� ������Ƽ
    IDP_DEF(UInt, "__OPTIMIZER_FIXED_GROUP_MEMORY_TEMP",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 0 );

    // BUG-38339 Outer Join Elimination
    IDP_DEF(UInt, "__OPTIMIZER_OUTERJOIN_ELIMINATION",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 1 );

    // PROJ-1413 Simple View Merging
    IDP_DEF(UInt, "__OPTIMIZER_SIMPLE_VIEW_MERGING_DISABLE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 0 );

    // BUG-34234
    // target���� ���� ȣ��Ʈ ������ varchar type���� ���� �����Ѵ�.
    IDP_DEF(UInt, "COERCE_HOST_VAR_IN_SELECT_LIST_TO_VARCHAR",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 32000, 0 );

    // BUG-19089 FK�� �ִ� ���¿��� CREATE REPLICATION ������ �����ϵ��� �Ѵ�.
    // Min : 0, Max : 1, Default : 0
    IDP_DEF(UInt, "CHECK_FK_IN_CREATE_REPLICATION_DISABLE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 0 );

    // BUG-29209
    // natc test�� ���Ͽ� Plan Display����
    // Ư�� ������ �������� �ʰ� �ϴ� ������Ƽ
    IDP_DEF(UInt, "__DISPLAY_PLAN_FOR_NATC",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_USER|
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 0);

    // BUG-34342
    // ������� ��弱��
    // precision mode : 0
    // C type�� numeric type�� ����� numeric type���� ����Ѵ�.
    // performance mode : 1
    // C type�� numeric type�� ����� C type���� ����Ѵ�.
    IDP_DEF(UInt, "ARITHMETIC_OPERATION_MODE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 3, 1 ); /* BUG-46195 */

    /* PROJ-1090 Function-based Index */
    IDP_DEF(UInt, "QUERY_REWRITE_ENABLE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 0);

    /* BUG-32101
     * Disk table���� index scan�� cost�� property�� ���� ������ �� �ִ�.
     */
    IDP_DEF(SInt, "OPTIMIZER_DISK_INDEX_COST_ADJ",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            1, 10000, 100);

    // BUG-43736
    IDP_DEF(SInt, "OPTIMIZER_MEMORY_INDEX_COST_ADJ",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            1, 10000, 100);

    /*
     * BUG-34441: Hash Join Cost�� ������ �� �ִ� ������Ƽ
     */
    IDP_DEF(UInt, "OPTIMIZER_HASH_JOIN_COST_ADJ",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            1, 10000, 100);

    /* BUG-34295
     * ANSI style join �� inner join �� join ordering �������
     * ó���Ͽ� ���� ���� plan �� �����Ѵ�. */
    IDP_DEF(SInt, "OPTIMIZER_ANSI_JOIN_ORDERING",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 0);

    // BUG-38402
    // ansi ��Ÿ�� inner ���θ� ������쿡�� �Ϲ� Ÿ������ ��ȯ�Ѵ�.
    IDP_DEF(SInt, "__OPTIMIZER_ANSI_INNER_JOIN_CONVERT",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 1);

    // BUG-40022
    // 1: NL, 2: HASH, 4: SORT, 8: MERGE
    // HASH+MERGE : 10
    IDP_DEF(UInt, "__OPTIMIZER_JOIN_DISABLE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 14, 8);

    // BUG-40492,BUG-45447
    // iduMemPool�� Slot ������ �ּҰ��� �����Ѵ�.
    IDP_DEF( UInt, "__MEMPOOL_MINIMUM_SLOT_COUNT",
             IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
             IDP_ATTR_IU_ANY |
             IDP_ATTR_MS_ANY |
             IDP_ATTR_LC_INTERNAL |
             IDP_ATTR_RD_READONLY |
             IDP_ATTR_ML_JUSTONE  |
             IDP_ATTR_CK_CHECK,
             2, 5000, 5 );

    /* BUG-34350
     * stepAfterOptimize �ܰ迡�� tuple �� ������ �ʴ� �޸𸮸�
     * �����ϰ� ���Ҵ��ϴ� ������ �����Ͽ� prepare �ð��� ���δ�. */
    IDP_DEF(SInt, "OPTIMIZER_REFINE_PREPARE_MEMORY",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 1);

    /*   
     * BUG-34235: in subquery keyrange tip�� ������ ��� �׻� ����
     *
     * 0: AUTO
     * 1: InSubqueryKeyRange
     * 2: TransformNJ
     */
    IDP_DEF(UInt, "OPTIMIZER_SUBQUERY_OPTIMIZE_METHOD",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 2, 1);

    /*
     * PROJ-2242 : eliminate common subexpression �� ������ ��� �׻� ����
     *     
     * 0 : CSE OFF mode
     * 1 : CSE ON mode
     * 2 : PATIAL CSE ON mode ( BUG-48348 )
     *     - where���� CNF�� ����� ��� CSE�� �������� ����
     */
    IDP_DEF(UInt, "__OPTIMIZER_ELIMINATE_COMMON_SUBEXPRESSION",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 2, 1);

    /*
     * PROJ-2242 : constant filter subsumption �� ������ ��� �׻� ����
     *
     * 0 : FALSE
     * 1 : TRUE
     */
    IDP_DEF(UInt, "__OPTIMIZER_CONSTANT_FILTER_SUBSUMPTION",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 1);

    // PROJ-2242
    // ������ �Է¹޾Ƽ� �̸� ���ǵ� ������Ƽ�� �ϰ� ����
    IDP_DEF(String, "OPTIMIZER_FEATURE_ENABLE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, IDP_MAX_PROP_STRING_LEN, (SChar *)IDU_ALTIBASE_VERSION_STRING );  // max feature version

    // BUG-38434
    IDP_DEF(UInt, "__OPTIMIZER_DNF_DISABLE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 0 );

    // BUG-38416 count(column) to count(*)
    IDP_DEF(UInt, "OPTIMIZER_COUNT_COLUMN_TO_COUNT_ASTAR",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 1);

    // BUG-36438 LIST transformation
    IDP_DEF(UInt, "__OPTIMIZER_LIST_TRANSFORMATION",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 1);

#if defined(ALTIBASE_PRODUCT_XDB)
    // BUG-41795
    IDP_DEF(UInt, "__DA_DDL_ENABLE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 0);
#endif

    // fix BUG-42752    
    IDP_DEF(UInt, "__OPTIMIZER_ESTIMATE_KEY_FILTER_SELECTIVITY",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 1);

    // BUG-43059 Target subquery unnest/removal disable
    IDP_DEF(UInt, "__OPTIMIZER_TARGET_SUBQUERY_UNNEST_DISABLE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 0);

    IDP_DEF(UInt, "__OPTIMIZER_TARGET_SUBQUERY_REMOVAL_DISABLE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 0);

    IDP_DEF(UInt, "OPTIMIZER_DELAYED_EXECUTION",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 0);

    //---------------------------------------------
    // PR-13395 TPC-H ??? ?? ??
    //---------------------------------------------
    // TPC-H SCALE FACTOR? ?? ?? ?? ?? ??
    // 0 : ???? ??.
    IDP_DEF(UInt, "__QP_FAKE_STAT_TPCH_SCALE_FACTOR",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 65536, 0);

    // ?? BUFFER SIZE
    // 0 : ???? ??.
    IDP_DEF(UInt, "__QP_FAKE_STAT_BUFFER_SIZE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, ID_UINT_MAX, 0);

    //---------------------------------------------
    // PROJ-2002 Column Security
    //---------------------------------------------

    IDP_DEF(String, "SECURITY_MODULE_NAME",
            IDP_ATTR_SL_ALL       |
            IDP_ATTR_IU_IDENTICAL |
            IDP_ATTR_MS_ANY       |
            IDP_ATTR_SK_ASCII     |
            IDP_ATTR_LC_EXTERNAL  |
            IDP_ATTR_RD_WRITABLE  |
            IDP_ATTR_ML_JUSTONE   |
            IDP_ATTR_CK_CHECK,
            0, IDP_MAX_PROP_STRING_LEN, (SChar *)"");

    IDP_DEF(String, "SECURITY_MODULE_LIBRARY",
            IDP_ATTR_SL_ALL       |
            IDP_ATTR_IU_IDENTICAL |
            IDP_ATTR_MS_ANY       |
            IDP_ATTR_SK_PATH      |
            IDP_ATTR_LC_EXTERNAL  |
            IDP_ATTR_RD_WRITABLE  |
            IDP_ATTR_ML_JUSTONE   |
            IDP_ATTR_CK_CHECK,
            0, IDP_MAX_PROP_STRING_LEN, (SChar *)"");

    IDP_DEF(String, "SECURITY_ECC_POLICY_NAME",
            IDP_ATTR_SL_ALL       |
            IDP_ATTR_IU_IDENTICAL |
            IDP_ATTR_MS_ANY       |
            IDP_ATTR_SK_ASCII     |
            IDP_ATTR_LC_EXTERNAL  |
            IDP_ATTR_RD_WRITABLE  |
            IDP_ATTR_ML_JUSTONE   |
            IDP_ATTR_CK_CHECK,
            0, IDP_MAX_PROP_STRING_LEN, (SChar *)"");

    IDP_DEF(UInt, "__KEY_PRESERVED_TABLE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 1);
    

    //----------------------------------
    // For SM
    //----------------------------------
    IDP_DEF(UInt, "TRCLOG_SET_LOCK_TIME",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 0);

    // fix BUG-33589
    IDP_DEF(UInt, "PLAN_REBUILD_ENABLE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 0);

    IDP_DEF(UInt, "DB_LOGGING_ENABLE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 1);

    /* PROJ-2361 */
    IDP_DEF(UInt, "__OPTIMIZER_AVG_TRANSFORM_ENABLE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 1);

    /*
     *-----------------------------------------------------
     * PROJ-1071 Parallel Query
     *-----------------------------------------------------
     */

    IDP_DEF(UInt, "PARALLEL_QUERY_THREAD_MAX",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1024, IDL_MIN(idlVA::getProcessorCount(), 1024));

    /* queue ��� �ð�: ������ micro-sec */
    IDP_DEF(UInt, "PARALLEL_QUERY_QUEUE_SLEEP_MAX",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            200, 1000000, 500000);

    /* PRLQ queue size */
    IDP_DEF(UInt, "PARALLEL_QUERY_QUEUE_SIZE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            4, 1048576, 1024);

    /* PROJ-2462 Reuslt Cache */
    IDP_DEF(UInt, "RESULT_CACHE_ENABLE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 0 );

    /* PROJ-2462 Reuslt Cache */
    IDP_DEF(UInt, "TOP_RESULT_CACHE_MODE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 3, 0 );

    /* PROJ-2462 Reuslt Cache */
    IDP_DEF(ULong, "RESULT_CACHE_MEMORY_MAXIMUM",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            (4096),            // min : 4K
            ID_ULONG_MAX,      // max
            (1024*1024*10));   // default : 10M

    /* PROJ-2462 Reuslt Cache */
    IDP_DEF(UInt, "TRCLOG_DETAIL_RESULTCACHE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 0 );

    /*
     *  Force changes parallel degree when table created.
     *  HIDDEN PROPERTY
     */
    IDP_DEF(UInt, "__FORCE_PARALLEL_DEGREE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            1, 65535, 1);

    /* PROJ-2452 Caching for DETERMINISTIC Function */

    // Cache object max count for caching
    IDP_DEF(UInt, "__QUERY_EXECUTION_CACHE_MAX_COUNT",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 65536, 65536 );
 
    // Cache memory max size for caching
    IDP_DEF(UInt, "__QUERY_EXECUTION_CACHE_MAX_SIZE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 10 * 1024 * 1024, 65536 );

    // Hash bucket count for caching
    IDP_DEF(UInt, "__QUERY_EXECUTION_CACHE_BUCKET_COUNT",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 512, 61 );

    /* 
     * HIDDEN PROPERTY (only natc test)
     *  1 : Force cache of basic function
     *  2 : Force DETERMINISTIC option
     */
    IDP_DEF(UInt, "__FORCE_FUNCTION_CACHE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 2, 0);

    /* PROJ-2448 Subquery caching */
    // Force subquery cache disable
    IDP_DEF(UInt, "__FORCE_SUBQUERY_CACHE_DISABLE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 0);

    /* ORDER BY Elimination 
     * value| �ǹ�
     *   0  | OBYE off
     *   1  | BUG-41183 target���� count(*)�� �� �ζ��κ信 ORDER BY ����
     *   2  | BUG-48941 where ���� �������� ������Ŷ�� �ζ��κ信 ORDER BY ����
     *   3  | 1+2
     */
    IDP_DEF(UInt, "__OPTIMIZER_ORDER_BY_ELIMINATION_ENABLE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 3, 1);

    // BUG-41249 DISTINCT Elimination
    IDP_DEF(UInt, "__OPTIMIZER_DISTINCT_ELIMINATION_ENABLE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 1);

    // PROJ-2582 recursive with
    IDP_DEF(UInt, "RECURSION_LEVEL_MAXIMUM",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            1,             // min
            ID_UINT_MAX,   // max
            1000 );        // default

    // PROJ-2750 LEFT OUTER SKIP RIGHT
    IDP_DEF(UInt, "__LEFT_OUTER_SKIP_RIGHT_ENABLE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 1);

    //----------------------------------
    // For ST
    //----------------------------------
    
    // PROJ-1583 large geometry
    // Min : 32000 Max : 100M, Default : 32000
    IDP_DEF(UInt, "ST_OBJECT_BUFFER_SIZE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            32000, 104857600, 32000);

    // BUG-22924 allow "ring has cross lines"
    // min : 0, max : 2, default : 0
    // 0 : disallow invalid object
    // 1 : allow invalid object level 1
    // 2 : allow invalid object levle 2
    IDP_DEF(UInt, "ST_ALLOW_INVALID_OBJECT",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 2, 0);

    // PROJ-2166 Spatial Validation
    // 0 : use GPC(General Polygon Clipper) Library
    // 1 : use Altibase Polygon Clipper Library
    IDP_DEF(UInt, "ST_USE_CLIPPER_LIBRARY",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 0);

    // BUG-33904 new clipping library tolerance level in st-function
    // 0 : 1e-4
    // 1 : 1e-5
    // 2 : 1e-6
    // 3 : 1e-7 (default)
    // 4 : 1e-8
    // 5 : 1e-9
    // 6 : 1e-10
    // 7 : 1e-11
    // 8 : 1e-12
    IDP_DEF(UInt, "ST_CLIP_TOLERANCE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 8, 3);
    
    // BUG-33576
    // spatial validation for insert geometry data
    // 0 : disable
    // 1 : enable
    IDP_DEF(UInt, "ST_GEOMETRY_VALIDATION_ENABLE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 1);
    
    //--------------------------------------------------------
    // Trace Log End
    //--------------------------------------------------------
    /* bug-37320 max of max_listen increased */
    IDP_DEF(UInt, "MAX_LISTEN",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 16384, 128);

    IDP_DEF(ULong, "QP_MEMORY_CHUNK_SIZE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            1024, ID_ULONG_MAX, 64 * 1024);

    IDP_DEF(SLong, "ALLOCATION_RETRY_TIME",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, ID_UINT_MAX, IDU_MEM_DEFAULT_RETRY_TIME);

    IDP_DEF(UInt, "MEMMGR_LOG_LEVEL",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 2, 0);

    IDP_DEF(ULong, "MEMMGR_LOG_LOWERSIZE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, ID_ULONG_MAX, 0);

    IDP_DEF(ULong, "MEMMGR_LOG_UPPERSIZE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, ID_ULONG_MAX, 0);

    IDP_DEF( UInt, "DA_FETCH_BUFFER_SIZE",
             IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
             IDP_ATTR_IU_ANY |
             IDP_ATTR_MS_ANY |
             IDP_ATTR_LC_EXTERNAL |
             IDP_ATTR_RD_READONLY |
             IDP_ATTR_ML_JUSTONE  |
             IDP_ATTR_CK_CHECK,
             ( 256 * 1024 ), ID_UINT_MAX, ( 256 * 1024 ) );

    //===================================================================
    // To Fix PR-13963
    // Min: 0 (��뷮 �� ��� ������ �α����� ����)
    // Max: ID_UNIT_MAX
    // default : 0
    //===================================================================
    IDP_DEF(UInt, "INSPECTION_LARGE_HEAP_THRESHOLD",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, ID_UINT_MAX, 0);

    // ==================================================================
    // SM Properties
    // ==================================================================

    // RECPOINT �����׽�Ʈ Ȱ��ȭ ������Ƽ PRJ-1552
    // ��Ȱ��ȭ : 0, Ȱ��ȭ : 1
    IDP_DEF(UInt, "ENABLE_RECOVERY_TEST",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 0);

    IDP_DEF(UInt, "ART_DECREASE_VAL",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, ID_UINT_MAX, 0);

    // ����Ÿ���̽� ȭ�� �̸�
    IDP_DEF(String, "DB_NAME",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_SK_ASCII     |
            IDP_ATTR_LC_EXTERNAL  |
            IDP_ATTR_RD_READONLY  |
            IDP_ATTR_ML_JUSTONE   |
            IDP_ATTR_DF_DROP_DEFAULT | /* �ݵ�� �Է��ؾ� ��. */
            IDP_ATTR_CK_CHECK,
            0, IDP_MAX_PROP_DBNAME_LEN, (SChar *)"");

    // ����Ÿ ������ �����ϴ� ���
    // �ݵ�� 3�� �����Ǿ�� �ϰ� �ݵ�� �Է� �Ǿ�� ��.
    // 2���� �޸� DB�� ���� ��ο� 1���� ��ũ DB�� ���� ��θ�
    // �����ؾ� �Ѵ�.
    IDP_DEF(String, "MEM_DB_DIR",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_SK_PATH     |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_MULTIPLE |
            IDP_ATTR_DF_DROP_DEFAULT | /* �ݵ�� �Է��ؾ� ��. */
            IDP_ATTR_CK_CHECK,
            0, IDP_MAX_PROP_STRING_LEN, (SChar *)"");

    // DISK�� ����Ÿ ������ �����ϴ� ��� :
    // ������ ���丮�� �������� ���� ��� Default�� ���⿡ �����.
    IDP_DEF(String, "DEFAULT_DISK_DB_DIR",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_SK_PATH     |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, IDP_MAX_PROP_STRING_LEN, (SChar *)"?"IDL_FILE_SEPARATORS"dbs");

    // ����Ÿ���̽��� ���� �޸� ������ ����� ��� 0����,
    // �����޸� ������ ����� ��� shared memory key ���� �����ؾ� �Ѵ�.
    IDP_DEF(UInt, "SHM_DB_KEY",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, ID_UINT_MAX, 0);

    // PROJ-1548 Memory Tablespace
    // ����ڰ� �����޸� Chunk�� ũ�⸦ ������ �� �ֵ��� �Ѵ�
    /*
      ������ : ����� �����޸� Chunk�� ũ�Ⱑ DBȮ���� �⺻ ������
                EXPAND_CHUNK_PAGE_COUNT�� �Ǿ� �ִ�.
                �̴� ���ʿ��ϰ� ���� �����޸� Chunk��
                ����� �� ���ɼ��� �ִ�.

      �ذ�å : �����޸� Chunk 1���� ũ�⸦ ����ڰ� ������ �� �ֵ���
                ������ SHM_PAGE_COUNT_PER_KEY ������Ƽ�� ����.
    */
    IDP_DEF(UInt, "SHM_PAGE_COUNT_PER_KEY",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            320  /* 10 Mbyte */,
            ID_UINT_MAX,
            3200 /* 100 Mbyte */ );

    // FreePageList�� ������ Page�� �ּ� ����
    IDP_DEF(UInt, "MIN_PAGES_ON_TABLE_FREE_LIST",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            1, ID_UINT_MAX, 1);

    // DB���� Table�� �Ҵ�޾ƿ� Page ����
    // ���� 0�̸�
    // FreePageList�� ������ Page�� �ּ� ����(MIN_PAGES_ON_TABLE_FREE_LIST)��
    // DB���� �����´�.
    IDP_DEF(UInt, "TABLE_ALLOC_PAGE_COUNT",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, ID_UINT_MAX, 0);

    /*
     * BUG-25327 : [MDB] Free Page Size Class ������ Propertyȭ �ؾ� �մϴ�.
     */
    IDP_DEF(UInt, "MEM_SIZE_CLASS_COUNT",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            1, 4, 4);

    IDP_DEF(UInt, "DB_LOCK_ENABLE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 1);

    IDP_DEF(UInt, "MAX_CID_COUNT",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            1000, ID_UINT_MAX, 10000);

    // FreePageList�� ������ Page�� �ּ� ����
    IDP_DEF(UInt, "TX_PRIVATE_BUCKET_COUNT",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            2, 64, 2);

    // �α�ȭ���� ���
    IDP_DEF(String, "LOG_DIR",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_SK_PATH     |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_MULTIPLE |
            IDP_ATTR_DF_DROP_DEFAULT |
            IDP_ATTR_CK_CHECK,
            0, IDP_MAX_PROP_STRING_LEN, (SChar *)"");

    // �α� ��Ŀ ȭ���� ����Ǵ� ���
    IDP_DEF(String, "LOGANCHOR_DIR",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_SK_PATH     |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_EXACT_3 |
            IDP_ATTR_DF_DROP_DEFAULT |
            IDP_ATTR_CK_CHECK,
            0, IDP_MAX_PROP_STRING_LEN, (SChar *)"");

    // archive �α� ���� ���丮
    IDP_DEF(String, "ARCHIVE_DIR",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_SK_PATH     |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_MULTIPLE |
            IDP_ATTR_DF_DROP_DEFAULT |
            IDP_ATTR_CK_CHECK,
            0, IDP_MAX_PROP_STRING_LEN, (SChar *)"");

    // archive �α� ��ũ�� full�߻��� ��� ó��
    // ��ó�������
    IDP_DEF(UInt, "ARCHIVE_FULL_ACTION",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 0);

    // startup�� archive thread �ڵ� start
    // ����.
    IDP_DEF(UInt, "ARCHIVE_THREAD_AUTOSTART",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 1);

    // lock �� ȹ����� �ʾ��� ���,
    // ������ usec ��ŭ sleep �ϰ� �ٽ� retry�Ѵ�.
    // ���μӼ�
    IDP_DEF(ULong, "LOCK_TIME_OUT",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, ID_ULONG_MAX, 50);
#if 0
    IDP_DEF(UInt, "LOCK_SELECT",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 1);
#endif
    IDP_DEF(UInt, "TABLE_LOCK_ENABLE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 1);

    /* BUG-42928 No Partition Lock */
    IDP_DEF(UInt, "TABLE_LOCK_MODE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 0);

    IDP_DEF(UInt, "TABLESPACE_LOCK_ENABLE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 1);

    // PROJ-1784 DML without retry
    IDP_DEF(UInt, "__DML_WITHOUT_RETRY_ENABLE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 0);

    // �α������� ũ��
    // PR-14475 Group Commit
    // Direct I/O�ÿ� �α������� Disk�� ������ �⺻ ������
    // Direct I/O Page ũ���̴�.
    // ����ڰ� ������Ƽ �����Ͽ� Direct I/O Pageũ�⸦
    // �������� ���氡���ϹǷ�  Direct I/O Page�� �ִ� ũ����
    // 8K ����Ȥ�� mmap�� Align������ �Ǵ� Virtual Page�� ũ����
    // idlOS::getpagesize()���� ū������ �α����� ũ�Ⱑ Align�ǵ��� �Ѵ�
    sLogFileAlignSize= (UInt)( (idlOS::getpagesize() > IDP_MAX_DIO_PAGE_SIZE) ?
                               idlOS::getpagesize():
                               IDP_MAX_DIO_PAGE_SIZE ) ;

    IDP_DEF(ULong, "LOG_FILE_SIZE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_AL_SET_VALUE( sLogFileAlignSize ) |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            64 * 1024, ID_ULONG_MAX, 10 * 1024 * 1024);

    /* BUG-42930 LogFile Prepare Thread�� ���� ���� Server Kill �� �Ͼ ���
     * LogFile Size�� 0�� ������ ������ �ֽ��ϴ�. Server Kill Test�� �����
     * Size 0�� �α������� �˾Ƽ� ����� Server�� ���� �ִ� Property�� �����մϴ�. */
    IDP_DEF(UInt, "__ZERO_SIZE_LOG_FILE_AUTO_DELETE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 0);


    // üũ ����Ʈ �ֱ� (�ʴ���)
    IDP_DEF(UInt, "CHECKPOINT_INTERVAL_IN_SEC",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            3, IDV_MAX_TIME_INTERVAL_SEC, 6000);

    // üũ ����Ʈ �ֱ�( �α� ȭ���� �����Ǵ� ȸ��)
    // ������ ȸ����ŭ �α�ȭ���� ��ü�Ǹ� üũ����Ʈ�� �����Ѵ�.
    IDP_DEF(UInt, "CHECKPOINT_INTERVAL_IN_LOG",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            1, ID_UINT_MAX, 100);

    /* BUG-36764, BUG-40137
     * checkpoint�� �߻������� checkpoint-flush job�� ������ ���.
     * 0: flusher
     * 1: checkpoint thread */
    IDP_DEF(UInt, "__CHECKPOINT_FLUSH_JOB_RESPONSIBILITY",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 0);

    // PROJ-1566: Direct Path Buffer Flush Interval
    IDP_DEF(UInt, "__DIRECT_BUFFER_FLUSH_THREAD_SYNC_INTERVAL",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            1, IDV_MAX_TIME_INTERVAL_USEC, 100000);

    // �������� �þ �� �ִ� DB�� ũ�⸦ ���
#ifdef COMPILE_64BIT
    IDP_DEF(ULong, "MEM_MAX_DB_SIZE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            2*IDP_MAX_PAGE_LIST_COUNT*IDP_SM_PAGE_SIZE, // min : �ּ� EXPAND_CHUNK_PAGE_COUNT*32K���� Ŀ�� �Ѵ�.
            ID_ULONG_MAX,                           // max
            ID_ULONG(2 * 1024 * 1024 * 1024));      // default, 2G

    // BUG-17216
    IDP_DEF(ULong, "VOLATILE_MAX_DB_SIZE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            2*IDP_MAX_PAGE_LIST_COUNT*IDP_SM_PAGE_SIZE, // min : �ּ� EXPAND_CHUNK_PAGE_COUNT*32K���� Ŀ�� �Ѵ�.
            ID_ULONG_MAX,                         // max
            (ULong)ID_UINT_MAX  + 1);             // default

#else
    IDP_DEF(ULong, "MEM_MAX_DB_SIZE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            2*IDP_MAX_PAGE_LIST_COUNT*IDP_SM_PAGE_SIZE,  // min : �ּ� EXPAND_CHUNK_PAGE_COUNT*32K���� Ŀ�� �Ѵ�.
            (ULong)ID_UINT_MAX + 1,                // max
            ID_ULONG(2 * 1024 * 1024 * 1024));     // default, 2G

    // BUG-17216
    IDP_DEF(ULong, "VOLATILE_MAX_DB_SIZE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            2*IDP_MAX_PAGE_LIST_COUNT*IDP_SM_PAGE_SIZE,  // min : �ּ� EXPAND_CHUNK_PAGE_COUNT*32K���� Ŀ�� �Ѵ�.
            (ULong)ID_UINT_MAX + 1,                // max
            (ULong)ID_UINT_MAX + 1);               // default

#endif

    /* TASK-6327 New property for disk size limit */
    IDP_DEF(ULong, "DISK_MAX_DB_SIZE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            2*IDP_MAX_LFG_COUNT*IDP_SM_PAGE_SIZE, // min : �ּ� EXPAND_CHUNK_PAGE_COUNT*32K���� Ŀ�� �Ѵ�.
            ID_ULONG_MAX,                         // max
            ID_ULONG_MAX);                        // default
    /* TASK-6327 New property for license update */
    IDP_DEF(SInt, "__UPDATE_LICENSE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 0);

    // Memory Tablespace�� �⺻ DB File (Checkpoint Image) ũ��
    IDP_DEF(ULong, "DEFAULT_MEM_DB_FILE_SIZE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            IDP_DEFAULT_EXPAND_CHUNK_PAGE_COUNT * IDP_SM_PAGE_SIZE, // min : chunk size
            ID_ULONG_MAX,                        // max
            IDP_DEFAULT_EXPAND_CHUNK_PAGE_COUNT * IDP_SM_PAGE_SIZE * 256);//default:1G

    // �ӽ� ����Ÿ �������� �ѹ��� �Ҵ��ϴ� ����
    IDP_DEF(UInt, "TEMP_PAGE_CHUNK_COUNT",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            1, ID_UINT_MAX, 128);

    // dirty page ����Ʈ ������ ���� pre allocated pool ����
    // ���� �Ӽ�
    IDP_DEF(UInt, "DIRTY_PAGE_POOL",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            1, ID_UINT_MAX, 1024);

    /*BUG-32386  [sm_recovery] If the ager remove the MMDB slot and the
     *checkpoint thread flush the page containing the slot at same time, the
     *server can misunderstand that the freed slot is the allocated slot.
     *Ager�� MMDB Slot�� �����Ҷ� ���ÿ� Checkpoint Thread�� �ش� page��
     *Flush�� ���, ������ Free�� Slot�� �Ҵ�� Slot�̶�� �߸��ľ��� ��
     *�ֽ��ϴ�.
     *Checkpoint Flush ������ ���� ������ �ϱ� ���� Hidden Property ����.
     *DirtyPageFlush(DPFlush) �� Ư�� page�� ��� Ư�� Offset������ ���,
     *Wait �� �ٽ� ����Ͽ� TornPage�� �ǵ������� �����ϵ��� ��.
     *__MEM_DPFLUSH_WAIT_TIME    = ���ð�(��). 0�� ���, �� ��� ���� ����.
     *__MEM_DPFLUSH_WAIT_SPACEID,PAGEID = TablespaceID �� PageID. Page�� ����.
     *__MEM_DPFLUSH_WAIT_OFFSET  = ������ Page�� �� Offset���� ����� ��
     *                             SLEEP��ŭ ��� �� ���� offset�� ������*/
    IDP_DEF(UInt, "__MEM_DPFLUSH_WAIT_TIME",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, ID_UINT_MAX, 0 );

    IDP_DEF(UInt, "__MEM_DPFLUSH_WAIT_SPACEID",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, ID_UINT_MAX, 0 );

    IDP_DEF(UInt, "__MEM_DPFLUSH_WAIT_PAGEID",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, ID_UINT_MAX, 0 );

    IDP_DEF(UInt, "__MEM_DPFLUSH_WAIT_OFFSET",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, ID_UINT_MAX, 0 );

    // ���� �� ����� index rebuild �������� �����Ǵ�
    // index build thread�� ����
    IDP_DEF(UInt, "PARALLEL_LOAD_FACTOR",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            //fix BUG-19787
            1, 512, IDL_MIN(idlVA::getProcessorCount() * 2, 512));

    // sdnn iterator mempool�� �Ҵ�� memlist ����
    IDP_DEF(UInt, "ITERATOR_MEMORY_PARALLEL_FACTOR",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            1,
            ID_UINT_MAX,
            2);

    // ���ÿ� ������ �� �ִ� �ִ� Ʈ����� ����
    // BUG-28565 Prepared Tx�� Undo�������� free trans list rebuild ���� �߻�
    // �ּ� transaction table size�� 16���� ����(���� 0)
    IDP_DEF(UInt, "TRANSACTION_TABLE_SIZE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            IDP_MIN_TRANSACTION_COUNT, IDP_MAX_TRANSACTION_COUNT, 1024);

    /* BUG-35019 TRANSACTION_TABLE_SIZE�� �����߾����� trc �α׿� ����� */
    /* BUG-47655 0 == UInt Max , 0 �߰���*/
    IDP_DEF(UInt, "__TRANSACTION_TABLE_FULL_TRCLOG_CYCLE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, ID_UINT_MAX, 100000 );

    // �α� ���� ���ҽ� Ǯ�� ������ �ּ����� ���ҽ� ����
    IDP_DEF(UInt, "MIN_COMPRESSION_RESOURCE_COUNT",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            1, IDP_MAX_TRANSACTION_COUNT, 16);

    // �α� ���� ���ҽ� Ǯ���� ���ҽ��� �� �� �̻�
    // ������ ���� ��� Garbage Collection����?
    //
    // �⺻�� : �ѽð����� ������ ������ ������ �ݷ��� �ǽ�.
    // �ִ밪 : ULong������ ǥ���� �� �ִ� Micro���� �ִ밪��
    //           �ʴ����� ȯ���Ѱ�
    IDP_DEF(ULong, "COMPRESSION_RESOURCE_GC_SECOND",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            1, ID_ULONG_MAX/1000000, 3600);


    // SM���� ���Ǵ� SCN�� disk write �ֱ⸦ ����
    // �� ���� ���� ��� disk I/O�� ���� �߻��ϸ�,
    // �� ���� Ŭ ��� �׸�ŭ I/O�� ���� �߻��ϴ� ���
    // ��밡���� SCN�� �뷮�� �پ���.
    // ���� �Ӽ�
#if defined(ALTIBASE_PRODUCT_HDB)
    IDP_DEF(ULong, "SCN_SYNC_INTERVAL",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            40, 8000000, 80000);
#else
    IDP_DEF(ULong, "SCN_SYNC_INTERVAL",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            // PROJ-2446 bugbug
            10, 2000000, 20000);
#endif

    // isolation level�� �����Ѵ�.
    // 0 : read committed
    // 1 : repetable read
    // 2 : serialzable
    // ���μӼ�
    IDP_DEF(UInt, "ISOLATION_LEVEL",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 2, 0);

    // BUG-15396
    // Commit Write Wait Mode�� �����Ѵ�.
    // 0 ( commit write no wait )
    //   : commit ��, log�� disk�� ����Ҷ����� ��ٸ��� ����
    // 1 ( commit write wait )
    //   : commit ��, log�� disk�� ����Ҷ����� ��ٸ�
    IDP_DEF(UInt, "COMMIT_WRITE_WAIT_MODE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 0);

    // SHM_DB_KEY ���� ������ ���¿��� ��Ƽ���̽� ���۽� �����Ǵ�
    // ���� �޸� ûũ�� �ִ� ũ�� ����
    IDP_DEF(ULong, "STARTUP_SHM_CHUNK_SIZE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            1024, ID_ULONG_MAX, ID_ULONG(1 * 1024 * 1024 * 1024));

    // SHM_DB_KEY ���� ������ ���¿��� ��Ƽ���̽� ���۽� �����Ǵ�
    // ���� �޸� ûũ�� �ִ� ũ�� ����
    IDP_DEF(UInt, "SHM_STARTUP_SIZE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            ID_ULONG(32 * 1024 * 1024),       // 32M
            ID_ULONG(2 * 1024 * 1024 * 1024), // 2G
            ID_ULONG(512 * 1024 * 1024));     // 512M


    // ���� �޸� ûũ�� ũ��
    IDP_DEF(UInt, "SHM_CHUNK_SIZE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            ID_ULONG(32 * 1024 * 1024),       // 32M
            ID_ULONG(2 * 1024 * 1024 * 1024), // 2G
            ID_ULONG(512 * 1024 * 1024));     // 512M

    // SHM_DB_KEY ���� ������ ���¿��� ��Ƽ���̽� ���۽� �����Ǵ�
    // ���� �޸� ûũ�� Align Size
    IDP_DEF(UInt, "SHM_CHUNK_ALIGN_SIZE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            1024, ID_ULONG(1 * 1024 * 1024 * 1024), ID_ULONG(1 * 1024 * 1024));

    // �̸� �����ϴ� �α�ȭ���� ����
    IDP_DEF(UInt, "PREPARE_LOG_FILE_COUNT",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, ID_UINT_MAX, 5);
 
    /* BUG-48409 �α� ������ �̸� ���� �� �� temp������ Ȱ������ ���� 
     * 0 : temp ������ �������� �ʰ� logfile�� �ٷ� �����.
     * 1 : temp ������ �����ؼ� ������ logfile�� �ϼ� �Ǹ�
     *     rename ���� logfile123 ���� �����ؼ� ��� �Ѵ�. (default)
     *     prepare ���߿� ������ ���� �� ��� �߻��ϴ�
     *     �̿ϼ� logfile�� �����ϱ� ���ؼ� �߰��� */
    IDP_DEF(UInt, "__USE_TEMP_FOR_PREPARE_LOGFILE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 1);

    // BUG-15396 : log buffer type
    // mmap(= 0) �Ǵ� memory(= 1)
    // => LFG���� �����Ͽ��� �Ѵ�.
    IDP_DEF(UInt, "LOG_BUFFER_TYPE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_MULTIPLE |
            IDP_ATTR_CK_CHECK,
            0, 1, 0);

    // �޸� DB�α��� INSERT/UPDATE/DELETE LOG�� ���ؼ�
    // �α׸� ����ϱ� ���� compress�ϱ� �����ϴ� �α� ũ���� �Ӱ�ġ
    //
    // �� ���� 0�̸� �α� �������� ������� �ʴ´�.
    //
    // �α� ���ڵ��� ũ�Ⱑ �� �̻��̸� �����Ͽ� ����Ѵ�.
    IDP_DEF(UInt, "MIN_LOG_RECORD_SIZE_FOR_COMPRESS",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, ID_UINT_MAX, 512 );

    // TASK-2398 Log Compress
    IDP_DEF(ULong, "DISK_REDO_LOG_DECOMPRESS_BUFFER_SIZE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            1, ID_UINT_MAX, 128 * 1024 * 1024 );


    // �α�ȭ���� sync �ֱ�(��)
    // ���� �Ӽ�
    IDP_DEF(UInt, "SYNC_INTERVAL_SEC_",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, IDV_MAX_TIME_INTERVAL_SEC, 3);

    // �α�ȭ���� sync �ֱ�( milliseconds )
    // ���μӼ�
    IDP_DEF(UInt, "SYNC_INTERVAL_MSEC_",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 999999, 200);

    /* BUG-35392 */
    // Uncompleted LSN�� �����ϴ� Thread�� Interval�� ����.
    IDP_DEF(UInt, "UNCOMPLETED_LSN_CHECK_THREAD_INTERVAL",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, UINT_MAX, 1000 );

    /* BUG-35392 */
    // �α�ȭ���� sync �ּ� �ֱ�( milliseconds )
    // ���μӼ�
    IDP_DEF(UInt, "LFTHREAD_SYNC_WAIT_MIN",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 999999, 1);

    /* BUG-35392 */
    // �α�ȭ���� sync �ִ� �ֱ�( milliseconds )
    // ���μӼ�
    IDP_DEF(UInt, "LFTHREAD_SYNC_WAIT_MAX",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, ID_UINT_MAX, (1000));

    /* BUG-35392 */
    // �α�ȭ���� sync �ּ� �ֱ�( milliseconds )
    // ���μӼ�
    IDP_DEF(UInt, "LFG_MANAGER_SYNC_WAIT_MIN",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 999999, 1);

    /* BUG-35392 */
    // �α�ȭ���� sync �ִ� �ֱ�( milliseconds )
    // ���μӼ�
    IDP_DEF(UInt, "LFG_MANAGER_SYNC_WAIT_MAX",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 999999, 1000);

    // �α�ȭ�� ������ ȭ���� ���� ��� 
    // 0 : write
    // 1 : fallocate 
    IDP_DEF(UInt, "LOG_CREATE_METHOD",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 0);

    // �α�ȭ�� ������ ȭ���� �ʱ�ȭ  ���
    // ���μӼ�
    // 0 : �� �� �ѹ���Ʈ�� �ʱ�ȭ  
    // 1 : 0 ���� �α� ��ü �ʱ�ȭ
    // 2 : random������ �α� ��ü �ʱ�ȭ
    IDP_DEF(UInt, "SYNC_CREATE_",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 2, 1);

    // BUG-23637 �ּ� ��ũ ViewSCN�� Ʈ����Ƿ������� Statement ������ ���ؾ���.
    // SysMinViewSCN�� �����ϴ� �ֱ�(mili-sec.)
    /* BUG-32944 [sm_transaction] REBUILD_MIN_VIEWSCN_INTERVAL_ - invalid
     * flag */
    IDP_DEF(UInt, "REBUILD_MIN_VIEWSCN_INTERVAL_",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, IDV_MAX_TIME_INTERVAL_MSEC, 100 );

    // �ϳ��� OID LIST ����ü���� ������ �� �ִ� OID�� ����
    // ���� �Ӽ�
    IDP_DEF(UInt, "OID_COUNT_IN_LIST_",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, ID_UINT_MAX, 8);

    // �ϳ��� TOUCH LIST ����ü���� ������ �� �ִ�
    // ���ŵ� �������� ����
    // ���� �Ӽ�
    IDP_DEF(UInt, "TRANSACTION_TOUCH_PAGE_COUNT_BY_NODE_",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, ID_UINT_MAX, 14 ); /* TASK-6950 : WRITABLE�� �ٲٸ� �ȵ�. */

    // ����ũ�� ��� Ʈ����Ǵ� ĳ���� ������ ����
    // ���� �Ӽ�
    IDP_DEF(UInt, "TRANSACTION_TOUCH_PAGE_CACHE_RATIO_",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 100, 10 );

    // To Fix BUG-17371 [MMDB] Aging�� �и���� System�� ������ ��
    //                         Aging�� �и��� ������ �� ��ȭ��.
    //
    // => Logical Thread �������� ���ķ� �����Ͽ� �����ذ�
    //
    // Logical Ager Thread�� �ּҰ����� �ִ밹���� ������Ƽ�� ����
    IDP_DEF(UInt, "MAX_LOGICAL_AGER_COUNT",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            1, 99, 99);

    IDP_DEF(UInt, "MIN_LOGICAL_AGER_COUNT",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            1, 99, 1);

#if defined(WRS_VXWORKS)
    IDP_DEF(UInt, "LOGICAL_AGER_COUNT_",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            1, 99, 1);
#else
    IDP_DEF(UInt, "LOGICAL_AGER_COUNT_",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            1, 99, 3);
#endif

#if defined(WRS_VXWORKS)
    IDP_DEF(UInt, "DELETE_AGER_COUNT_",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            1, 99, 1);
#else
    IDP_DEF(UInt, "DELETE_AGER_COUNT_",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            1, 99, 3);
#endif

    /* Ager List�� ������ �����ϴ� Property 
     * default: 7
     * min    : 1
     * max    : 99
     * �ش� Property�� �Ҽ������� �ؾ� List �й谡 ������ �̷������. */
    IDP_DEF(UInt, "__AGER_LIST_COUNT",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            1, 99, 7);

    /* Delete Thread�� Parallel ������ ON/OFF�ϴ� ������Ƽ
     * Delete Thread�� ������ DELETE_AGER_COUNT_ ������Ƽ�� ������.
     * 0 : PARALLEL �������� ����.
     *     __AGER_LIST_COUNT �� 1�� ��� 0���� ������ Parallel ���� ����.
     *     __AGER_LIST_COUNT �� 1���� Ŭ ��� List ���� Parallel�� ���� ����.
     * 1 : PARALLEL ������. 
     */
    IDP_DEF( UInt, "__PARALLEL_DELETE_THREAD",
             IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
             IDP_ATTR_IU_ANY |
             IDP_ATTR_MS_ANY |
             IDP_ATTR_LC_INTERNAL |
             IDP_ATTR_RD_READONLY |
             IDP_ATTR_ML_JUSTONE  |
             IDP_ATTR_CK_CHECK,
             0, 1, 1);

    // 0: serial
    // 1: parallel
    // ���μӼ�
    IDP_DEF(UInt, "RESTORE_METHOD_",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 1);

    IDP_DEF(UInt, "RESTORE_THREAD_COUNT",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            //fix BUG-19787
            1, 512, IDL_MIN(idlVA::getProcessorCount() * 2, 512));

    IDP_DEF(UInt, "RESTORE_AIO_COUNT",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, ID_UINT_MAX, 0);

    /* �����ͺ��̽� ���� �ε��,
       �ѹ��� ���Ͽ��� �޸𸮷� �о���� ������ �� */
    IDP_DEF(UInt, "RESTORE_BUFFER_PAGE_COUNT",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            1, ID_UINT_MAX, 1024);

    IDP_DEF(UInt, "CHECKPOINT_AIO_COUNT",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, ID_UINT_MAX, 0);

    // To Fix BUG-9366
    IDP_DEF(UInt, "CHECKPOINT_BULK_WRITE_PAGE_COUNT",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, ID_UINT_MAX, 0);

    // To Fix BUG-9366
    IDP_DEF(UInt, "CHECKPOINT_BULK_WRITE_SLEEP_SEC",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, IDV_MAX_TIME_INTERVAL_SEC, 0);

    // To Fix BUG-9366
    IDP_DEF(UInt, "CHECKPOINT_BULK_WRITE_SLEEP_USEC",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, IDV_MAX_TIME_INTERVAL_USEC, 0);

    /* checkpoint�� flush dirty pages ��������
       ����Ÿ���̽� ���� sync�ϱ� ���� write�ؾ��ϴ�
       ������ ���� ���� �⺻�� 100MB(3200 pages)
       ���� 0�� ��쿡�� page write ���� ���� �ʰ�
       ��� page�� ��� write�ϰ� �������� �ѹ��� sync�Ѵ�.*/
    IDP_DEF(UInt, "CHECKPOINT_BULK_SYNC_PAGE_COUNT",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, ID_UINT_MAX, 3200 );


    IDP_DEF(UInt, "CHECK_STARTUP_VERSION",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 1);

    IDP_DEF(UInt, "CHECK_STARTUP_BITMODE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 1);

    IDP_DEF(UInt, "CHECK_STARTUP_ENDIAN",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 1);

    IDP_DEF(UInt, "CHECK_STARTUP_LOGSIZE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 1);

    // ���� ������ ����ȭ�� ���ؼ� ��� ȣ��Ʈ�� ���� ��
    // �� �ð� �̻� ������ ���� ��� ������ �� �̻� �õ����� �ʰ�
    // �����Ѵ�. (��)
    // ���μӼ�
    IDP_DEF(UInt, "REPLICATION_LOCK_TIMEOUT",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_WRITABLE |  // BUG-18142
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 3600, 5 /*sec*/);

    /* BUG-27709 receiver�� ���� �ݿ���, Ʈ����� alloc��
       �� �ð� �̻��� ����ϸ�, ���н�Ű�� �ش� receiver�� �����Ѵ�. */
    IDP_DEF(UInt, "__REPLICATION_TX_VICTIM_TIMEOUT",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 3600, 10 /*sec*/);

    IDP_DEF(UInt, "LOGFILE_PRECREATE_INTERVAL",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, IDV_MAX_TIME_INTERVAL_SEC, 10);

    IDP_DEF(UInt, "FILE_INIT_BUFFER_SIZE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, ID_UINT_MAX, 10 * 1024 * 1024);
#if 0
    IDP_DEF(UInt, "LOG_BUFFER_LIST_COUNT",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, ID_UINT_MAX, 1);

    IDP_DEF(UInt, "LOG_BUFFER_ITEM_COUNT",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, ID_UINT_MAX, 5);
#endif
    // 0 : buffered IO
    // 1 : direct IO
    // ����� �Ӽ�
    IDP_DEF(UInt, "DATABASE_IO_TYPE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 0);

    // Log ��Ͻ� IOŸ��
    // 0 : buffered IO
    // 1 : direct IO
    IDP_DEF(UInt, "LOG_IO_TYPE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 1);

    /* BUG-15961: DirectIO�� ���� �ʴ� System Property�� �ʿ���*/
    // 0 : disable
    // 1 : enable
    IDP_DEF(UInt, "DIRECT_IO_ENABLED",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 1);

    /* BUG-18646: Direct Path Insert�� ���ӵ� Page�� ���� Insert ����� IO��
       Page������ �ƴ� �������� �������� ��� �ѹ��� IO�� �����Ͽ��� �մϴ�. */

    // SD_PAGE_SIZE�� 8K�϶� ��������
    // Default:   1M
    // Min    :   8K
    // Max    : 100M
    IDP_DEF(UInt, "BULKIO_PAGE_COUNT_FOR_DIRECT_PATH_INSERT",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            128, 12800, 128);

    /* Direct Path Buffer Page������ ���� */
    // Default: 1024
    // Min    : 1024
    // Max    : 2^32 - 1
    IDP_DEF(UInt, "DIRECT_PATH_BUFFER_PAGE_COUNT",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            1024, ID_UINT_MAX, 1024);

// AIX�� ��� Direct IO Page�� 512 byte��
// - iduFile.h �� �׷��� ���ǵǾ� �־���
#if defined(IBM_AIX)
    // Direct I/O�� file offset�� data size�� Align�� Pageũ��
    IDP_DEF(UInt, "DIRECT_IO_PAGE_SIZE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            512, IDP_MAX_DIO_PAGE_SIZE, 512);
#else // �� ���� ���� �⺻���� 4096����
    IDP_DEF(UInt, "DIRECT_IO_PAGE_SIZE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            512, IDP_MAX_DIO_PAGE_SIZE, 4096);
#endif

    IDP_DEF(UInt, "DELETE_AGER_COMMIT_INTERVAL",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, ID_UINT_MAX, 100000 );

    // 0 : CPU count
    // other : ������ ����
    // ����� �Ӽ�
    IDP_DEF(UInt, "INDEX_BUILD_THREAD_COUNT",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            //fix BUG-19787
            1, 512, IDL_MIN(idlVA::getPhysicalCoreCount(), 512));

    // selective index building method (minimum record count)
    // min : 0, max : ID_UINT_MAX, default : 0
    IDP_DEF(UInt, "__DISK_INDEX_BOTTOM_UP_BUILD_THRESHOLD",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, ID_UINT_MAX, 0);

    // min : 2, max : ID_UINT_MAX, default : 128
    IDP_DEF(UInt, "DISK_INDEX_BUILD_MERGE_PAGE_COUNT",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            2, ID_UINT_MAX, 128);

    IDP_DEF(UInt, "INDEX_BUILD_MIN_RECORD_COUNT",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, ID_UINT_MAX, 10000);

    /* PROJ-1628
       2���� Node�� Key redistribution�� ������ ���� Ȯ���Ǿ�� ��
       Free ���� ����(%). Key redistribution �Ŀ� �� ��忡��
       �� ������Ƽ�� ������ ũ���� Free ������ Ȯ������ ������
       split���� �����Ѵ�.(Hidden)
     */
    IDP_DEF(UInt, "__DISK_INDEX_KEY_REDISTRIBUTION_LOW_LIMIT",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            1, 40, 1);

    /* BUG-43046 
       ��ũ �ε����� ������ sdnbBTree::traverse���� ���� ������ �� ���
       �ִ� traverse�ϴ� length (length�� node �ϳ� ������ �� 1�� �����Ѵ�.)
       -1�̸� traverse length�� check���� �ʴ´�.
     */
    IDP_DEF(SLong, "__DISK_INDEX_MAX_TRAVERSE_LENGTH",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            -1, ID_SLONG_MAX, 1000000);

    /* PROJ-1628
       Unbalanced split�� ������ ��, ���� �����Ǵ� Node�� ������ �ϴ�
       Free ������ ����(%). ���� ��� 90���� �����ϸ�, A : B�� Key ������
       90:10�� �ȴٴ� ���� �ǹ��Ѵ�.
     */
    IDP_DEF(UInt, "DISK_INDEX_UNBALANCED_SPLIT_RATE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            50, 99, 90);

    /* PROJ-1591
       Disk RTree�� Split�� �����ϴ� ����� �����Ѵ�.
       0: �⺻���� R-Tree �˰����� Split ������� Split�� �����Ѵ�.
       1: R*-Tree ������� Split�� �����Ѵ�.
     */
    IDP_DEF(UInt, "__DISK_INDEX_RTREE_SPLIT_MODE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 1);

    /* PROJ-1591:
       R*-Tree ����� Split ����� ��쿡�� ���Ǵ� ���̴�.
       Split�� ���� ���� �����ϱ� ���ؼ� �򰡵� �ִ� Ű ������ ������
       �����Ѵ�.
     */
    IDP_DEF(UInt, "__DISK_INDEX_RTREE_SPLIT_RATE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            20, 45, 40);

    /* PROJ-1591:
     */
    IDP_DEF(UInt, "__DISK_INDEX_RTREE_MAX_KEY_COUNT",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            3, 500, 500);

    // refine parallel factor
    // ����� �Ӽ�
    IDP_DEF(UInt, "REFINE_PAGE_COUNT",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, ID_UINT_MAX, 50);

    // 0 : disable
    // 1 : enable
    IDP_DEF(UInt, "TABLE_COMPACT_AT_SHUTDOWN",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 1);

    /* BUG-33008 [sm-disk-collection] [DRDB] The migration row logic
     * generates invalid undo record.
     * ���׿� ���� ���� ó�� ���� ���� */
    IDP_DEF(UInt, "__AFTER_CARE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, ID_UINT_MAX, 0 );

    // 0 : disable
    // 1 : enable
    // ����� �Ӽ�
    IDP_DEF(UInt, "CHECKPOINT_ENABLED",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 1);

    // sender�� ��ũ �� ����� �α׸��� �������� �� �� Ȱ��ȭ ��Ų��.
    IDP_DEF(UInt, "REPLICATION_SYNC_LOG",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 0);

    IDP_DEF(UInt, "TABLE_BACKUP_FILE_BUFFER_SIZE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1024 * 1024, 1024);

    IDP_DEF(UInt, "DEFAULT_LPCH_ALLOC_COUNT",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, ID_UINT_MAX, 1);

    IDP_DEF(UInt, "AGER_WAIT_MINIMUM",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, ID_UINT_MAX, (200000));

    IDP_DEF(UInt, "AGER_WAIT_MAXIMUM",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, ID_UINT_MAX, (1000000));

    /* PROJ-2268 Reuse Catalog Table Slot
     * 0 : Catalog Table Slot�� �������� �ʴ´�.
     * 1 : Catalog Table Slot�� �����Ѵ�. (default) */
    IDP_DEF( UInt, "__CATALOG_SLOT_REUSABLE",
             IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
             IDP_ATTR_IU_ANY |
             IDP_ATTR_MS_ANY |
             IDP_ATTR_LC_INTERNAL |
             IDP_ATTR_RD_READONLY |
             IDP_ATTR_ML_JUSTONE  |
             IDP_ATTR_CK_CHECK,
             0, 1, 1 );

    /* TASK-6006: Index ���� Parallel Index Rebuilding
     * �� ���� 1�� ��쿡�� Index ���� Parallel Index Rebuilding�� �����ϰ�
     * �� ���� 0�� ��� Table ���� Parallel Index Rebuilding�� �����Ѵ�. */
    IDP_DEF(SInt, "REBUILD_INDEX_PARALLEL_MODE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 1);

    /* BUG-40509 Change Memory Index Node Split Rate
     * Memory Index���� Unbalanced split�� ������ ��, 
     * ���� �����Ǵ� Node�� ������ �ϴ� Free ������ ����(%). 
     * ���� ��� 90���� �����ϸ�, A : B�� Key ������ 90:10�� �ȴٴ� ���� �ǹ��Ѵ�. */
    IDP_DEF(UInt, "MEMORY_INDEX_UNBALANCED_SPLIT_RATE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            50, 99, 90); 

// A3 -> A4�� �ٲ� ��

    // ���� ���� : LOCK_ESCALATION_MEMORY_SIZE (M���� �׳� byte�� )
    //             �׳� ũ��� ���.  1024 * 1024 ��ŭ ���ϱ� ����.
    // BUG-18863 : LOCK_ESCALATION_MEMORY_SIZE�� Readonly�� �Ǿ��ֽ��ϴ�.
    //             ���� Default���� 100M���� �ٲپ�� �մϴ�.
    IDP_DEF(UInt, "LOCK_ESCALATION_MEMORY_SIZE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1000 * 1024 * 1024, 100 * 1024 * 1024 );

    /* BUG-19080: Old Version�� ���� �����̻� ����� Transaction�� Abort�ϴ� �����
     *            �ʿ��մϴ�.
     *
     *       # �⺻��: 10M
     *       # �ּҰ�: 0 < 0�̸� �� Property�� �������� �ʽ��ϴ�. >
     *       # �ִ밪: ID_ULONG_MAX
     *       # ��  ��: Session Property�Դϴ�.
     **/
    IDP_DEF( ULong, "TRX_UPDATE_MAX_LOGSIZE",
             IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
             IDP_ATTR_IU_ANY |
             IDP_ATTR_MS_ANY |
             IDP_ATTR_LC_EXTERNAL |
             IDP_ATTR_RD_WRITABLE |
             IDP_ATTR_ML_JUSTONE  |
             IDP_ATTR_CK_CHECK,
             0, ID_ULONG_MAX, (10 * 1024 * 1024) );

    /* PROJ-2162 RestartRiskReduction
     * Startup�� MMDB Index Build�� ���ÿ� ��� Index��
     * Build�� ���ΰ�.*/
    /* BUG-32996 [sm-mem-index] The default value of
     * INDEX_REBUILD_PARALLEL_FACTOR_AT_STARTUP need to adjust */
    /* BUG-37977 �⺻ ���� �ھ� ���� ���� �ٸ��� ����
     * �ھ���� 4���� Ŭ ��쿡�� 4�� �����Ѵ�.
     * �� ������ ���� smuWorkerThreadMgr�� �����Ǹ� ������ �����̴�. */
    IDP_DEF(UInt, "INDEX_REBUILD_PARALLEL_FACTOR_AT_STARTUP",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            //fix BUG-19787, BUG-37977, BUG-40870
            1, 512, IDP_LIMITED_CORE_COUNT( 1, 4 ) );

    /* - BUG-14053
       System �޸� ������ Server Start�� Meta Table�� Index����
       Create�ϰ� Start�ϴ� ������Ƽ �߰�.
       INDEX_REBUILD_AT_STARTUP = 1: Start�� ��� Index Rebuild
       INDEX_REBUILD_AT_STARTUP = 0: Start�� Meta Table�� Index��
       Rebuild
    */
    IDP_DEF(UInt, "INDEX_REBUILD_AT_STARTUP",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 1);

    /* BUG-47540: ��� ���� �ڵ� ���� ��ɿ��� ���� �ϴ� �յ� ���� �ٸ�
     * ��忡 �������� �� ���ü��� ������� �ʰ� ����Ǳ� ������ ����
     * �̹� ����� �Ǵ� �������� row�� �ǵ帱 �� �ִ�. �� ������Ƽ�� �Ѹ�
     * ��� ���� �ڵ� ���� �� �̿� ���� ��� �ش� ����Ÿ�� ���� ������
     * Skip �Ѵ�.
     */
    IDP_DEF(UInt, "__SKIP_IDX_STAT_NODE_BOUND",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 1);

// smi
    /* TASK-4990 changing the method of collecting index statistics
     * 0 (Manual)       - ������� �籸��ÿ��� ���ŵ�
     * 1 (Accumulation) - Row�� ����ɶ����� ������Ŵ(5.5.1 ���� ����)*/
    /* BUG-33203 [sm_transaction] change the default value of 
     * __DBMS_STAT_METHOD, MUTEX_TYPE, and LATCH_TYPE properties */
    IDP_DEF(UInt, "__DBMS_STAT_METHOD",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 0);

    /* 
     * BUG-35163 - [sm_index] [SM] add some exception properties for __DBMS_STAT_METHOD
     * �Ʒ� 3 ������Ƽ�� __DBMS_STAT_METHOD ������ ���ܸ� �α� �����̴�.
     *
     * ���� �Ʒ� ������Ƽ�� �����Ǿ� ������ __DBMS_STAT_METHOD ������ �����ϰ�
     * �ش� ������Ƽ�� ������ ������ �ȴ�.
     * ���� ��� __DBMS_STAT_METHOD=0 �϶�,
     * __DBMS_STAT_METHOD_FOR_VRDB=1 �� �����ϸ� Volatile TBS�� ���ؼ���
     * ��� ������ MANUAL�� �ƴ� AUTO�� ó���ϰ� �ȴ�.
     */
    IDP_DEF(UInt, "__DBMS_STAT_METHOD_FOR_MRDB",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 0);

    IDP_DEF(UInt, "__DBMS_STAT_METHOD_FOR_VRDB",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 0);

    IDP_DEF(UInt, "__DBMS_STAT_METHOD_FOR_DRDB",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 0);

    /* TASK-4990 changing the method of collecting index statistics
     * ������� ȹ��� Sampling �ڵ� ���� ���� ������ ����
     * �⺻�� 131072��. ���� Page 131072 ������ Page�� 100% �� ��������,
     * �� �̻��� ���� �����ͼ� Sampling �Ѵ�.
     * �⺻�� 131072, PAGE SIZE :8K �������� Samplingũ��� ������ ����.
     * 1GB    = 100% 
     * 2GB    = 70.71%  
     * 10GB   = 31.62%
     * 100GB  = 10%
     *
     * BUG-42832 ������Ƽ�� �⺻���� 4096���� �����մϴ�.
     */
    IDP_DEF(UInt, "__DBMS_STAT_SAMPLING_BASE_PAGE_COUNT",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            1, ID_UINT_MAX, 4096 );


    /* TASK-4990 changing the method of collecting index statistics
     * ������� ȹ��� Parallel �⺻�� */
    IDP_DEF(UInt, "__DBMS_STAT_PARALLEL_DEGREE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            1, ID_UINT_MAX, 1 );

    /* TASK-4990 changing the method of collecting index statistics
     * Column������� �� ���� ������ ��������� �������� ����
     * 0 (No) - 1 (Yes) */
    IDP_DEF(UInt, "__DBMS_STAT_GATHER_INTERNALSTAT",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 1);

    /* TASK-4990 changing the method of collecting index statistics
     * Row ������ ���� ������� �����ÿ� N%�� �Ѿ��, ��������� ������Ѵ�.
     * �� Property�� N%�� �ǹ��Ѵ�. */
    IDP_DEF(UInt, "__DBMS_STAT_AUTO_PERCENTAGE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 100,  10 );

    /* TASK-4990 changing the method of collecting index statistics
     * ���� �ð�(SEC)��ŭ �帥 �� ������� �ڵ� ���� ���θ� �Ǵ��Ѵ�.
     * 0�̸� �������� �ʴ´�. */
    IDP_DEF(UInt, "__DBMS_STAT_AUTO_INTERVAL",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, IDV_MAX_TIME_INTERVAL_SEC, 0);

    // BUG-20789
    // SM �ҽ����� contention�� �߻��� �� �ִ� �κп�
    // ����ȭ�� �� ��� CPU�ϳ��� �� Ŭ���̾�Ʈ�� ������ ���� ��Ÿ���� ���
    // SM_SCALABILITY�� CPU���� ���ϱ� �̰��̴�.
    IDP_DEF(UInt, "SCALABILITY_PER_CPU",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            1, 128, 2);

    /* BUG-22235: CPU������ �ٷ��϶� Memory�� ���� ����ϴ� ������ �ֽ��ϴ�.
     *
     * ����ȭ ������ �� ������ �Ѿ�� �� ������ �����Ѵ�.
     * */
#if defined(ALTIBASE_PRODUCT_HDB)
    IDP_DEF(UInt, "MAX_SCALABILITY",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            1, ID_UINT_MAX, 16);
#else
    IDP_DEF(UInt, "MAX_SCALABILITY",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            1, ID_UINT_MAX, 32);
#endif

// for sdnhTempHash
/* --------------------------------------------------------------------
 * temp hash index���� ����� �����ִ� �ִ� hash table  �����̴�.
 * ----------------------------------------------------------------- */
    IDP_DEF(UInt, "MAX_TEMP_HASHTABLE_COUNT",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            1, 128, 128);
#if 0
// sda
    // BUFFER POOL�� ������ ������ ����� TSS�� ������ %��
    // TSS������ BUFFER�� ��%�� ���������ν� undo �������� ���� Ȯ���� ���´�.
    // 0 �� ��쿡�� TSS�� ������ �������� ��밡���ϴ�.
    IDP_DEF(UInt, "TSS_CNT_PCT_TO_BUFFER_POOL",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 100,(0));
#endif
//smx
    // transaction table�� full��Ȳ���� transaction�� alloc������ ��쿡
    // ��� �ϴ�  �ð� micro sec.
    IDP_DEF(UInt, "TRANS_ALLOC_WAIT_TIME",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            1, (60000000) /* 60�� */ , (80));

    // PROJ-1566 : Private Buffer Page
    IDP_DEF(UInt, "PRIVATE_BUFFER_PAGE_COUNT",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, ID_UINT_MAX, 0);

    // BUG-47525 GROUP COMMIT COUNT
    // Group Commit�� �����Ҷ� �ִ� ����� Tx�� Groupȭ �ϴ��� �����ϴ� Property
    // �ִ� ���̱� ������ ������ 1���� Group ȭ �Ǵ� ��쵵 �߻��Ѵ�.
    // 0���� �����Ǿ� ������ Group Commit�� Off �Ѵ�.
    // Min : 0, Max : 50, Default : 10
    IDP_DEF(UInt, "__GROUP_COMMIT_COUNT",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 50, 10);

    // BUG-47525 GROUP COMMIT LIST COUNT
    // Group Commit�� ���� Tx�� ������ List�� ����
    // Write ������ ������ ������ Write�� �� ����
    // Tx�� Groupȭ �� List�� ������ ���� �Ѵ�. 
    // Min : 2, Max : 50, Default : 8
    IDP_DEF(UInt, "__GROUP_COMMIT_LIST_COUNT",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            2, 50, 8);

    // PROJ-1704 Disk MVCC ������
    // Ʈ����� ���׸�Ʈ ��Ʈ�� ���� ����
    // Ʈ����� ���׸�Ʈ �ִ� ������ ù��°
    // ����Ÿ������ File Space Header�� ����� �� �ִ�
    // SegHdr PID�� �����̴�. SegHdr PID �� 4 ����Ʈ�̰�
    // TSSEG 512�� UDSEG 512������ �����ϰ� �����Ҽ� ����.
    IDP_DEF(UInt, "TRANSACTION_SEGMENT_COUNT",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            1, 512, (256));

    // Ʈ����� ���׸�Ʈ ���̺��� ��� ONLINE�� ��Ȳ����
    // Ʈ����� ���׸�Ʈ ��Ʈ���� �Ҵ� ���� ���� ��쿡
    // ��� �ϴ� �ð� micro sec.
    IDP_DEF(UInt, "TRANSACTION_SEGMENT_ALLOC_WAIT_TIME_",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            1, ID_UINT_MAX , (80));

    // BUG-47365 �α� ���� ���ҽ��� �� ��� ����
    // 0 : �������� �ʰ� Ʈ����� ����� ��ȯ
    // 1 : Ʈ����� ����� ��ȯ���� �ʰ� ����
    IDP_DEF(UInt, "LOG_COMP_RESOURCE_REUSE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 1);

    // BUG-47365 �α� ���� ���ҽ� �����
    // �ݳ����� �ʰ� ������ ���� Size
    IDP_DEF(UInt, "COMP_RES_TUNE_SIZE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            512, ID_UINT_MAX, (32 * 1024));

#if 0
    IDP_DEF(UInt, "TRANS_KEEP_TABLE_INFO_COUNT",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            1, 1024, 5);
#endif

#if defined(ALTIBASE_PRODUCT_XDB)
    // BUG-40960 : [sm-transation] [HDB DA] make transaction suspend sleep time tunable
    // transaction�� suspend�� ��, �� ���� sleep�� �ð�
    IDP_DEF(UInt, "TRANS_SUSPEND_SLEEP_USEC",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            1, (1000000) /* 1�� */ , (20));
#endif

// sdd
    // altibase ������ �ִ� ���� �ִ� datafile�� ����
    // �� ������ �׻� system�� �ִ� datafile�� ��������
    // �۾ƾ� �Ѵ�. �׷��� ������ system�� ��� open ȭ����
    // altibase�� ���� �����Ͽ� �ٸ� process�� ȭ���� open
    // �� �� ���� ��Ȳ�� �߻��� �� �𸥴�.
    IDP_DEF(UInt, "MAX_OPEN_DATAFILE_COUNT",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            1, ID_UINT_MAX, 200);

    // �ִ� �� �� �ִ� ȭ���� �̹� ���� �־�
    // ȭ���� �� �� ���� ��� ȭ���� ���� ���� ����ϴ� �ð�
    // �� �ð��� ������ ���̻� ��ٸ��� �ʰ� ������ �����Ѵ�.
    // �ʷ� ����
    IDP_DEF(UInt, "OPEN_DATAFILE_WAIT_INTERVAL",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, IDV_MAX_TIME_INTERVAL_SEC, 10);

    // ����Ÿ ���� backup�� ����Ǳ���� ��� �ð�(��) �Ǵ�
    // ����Ÿ ���� backup�� ����ǰ�, page image log��
    // ����Ÿ ���Ͽ� �����ϴ� ���� ����ϴ� �ð�(��)

    IDP_DEF(UInt, "BACKUP_DATAFILE_END_WAIT_INTERVAL",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, IDV_MAX_TIME_INTERVAL_SEC, 8);

    // ������ ������ ������ �� ������ �ʱ�ȭ�ϴ� ����
    // �� ���� ������ �����̴�.
    // min�� 1�� �� 8192����Ʈ�̰�
    // max�� 1024 ������ �� 8M �̸�
    // �⺻���� 1024 ������ �� 8M�̴�.
    IDP_DEF(ULong, "DATAFILE_WRITE_UNIT_SIZE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            1, 1024, 1024);

    /* To Fix TASK-2688 DRDB�� Datafile�� �������� FD( File Descriptor )
     * ��� �����ؾ� ��
     *
     * �ϳ��� DRDB�� Data File�� Open�� �� �ִ� FD�� �ִ� ����
     * */
    IDP_DEF(UInt, "DRDB_FD_MAX_COUNT_PER_DATAFILE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            1, 1024, 8);

#if defined(ALTIBASE_PRODUCT_HDB)
// For sdb
/* --------------------------------------------------------------------
 * buffer pool�� ũ��. ������ �����̴�.
 * ���� ũ��� ������ ���� * �� ������ ũ�Ⱑ �ȴ�.
 * ----------------------------------------------------------------- */
    IDP_DEF(UInt, "DOUBLE_WRITE_DIRECTORY_COUNT",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            1, 16, 2);

    IDP_DEF(String, "DOUBLE_WRITE_DIRECTORY",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_SK_PATH     |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_MULTIPLE |
            IDP_ATTR_DF_DROP_DEFAULT |
            IDP_ATTR_CK_CHECK,
            0, IDP_MAX_PROP_STRING_LEN, (SChar *)"");
#endif

// For sdb

#if defined(ALTIBASE_PRODUCT_HDB)
#if 0
/* --------------------------------------------------------------------
 * DEBUG ��忡�� validate �Լ� ���� ����
 * �� �Լ��� ����ɶ� ������ ���ϵ� �� �ִ�.
 * 0�϶� ����, 1�϶� �������
 * ----------------------------------------------------------------- */
    IDP_DEF(UInt, "VALIDATE_BUFFER",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 1 );
#endif
    /* PROJ-2669 begin */
/* --------------------------------------------------------------------
 * HOT_FLUSH_LIST_PCT
 *  0    : DO NOT DELAYED FLUSH
 *         IGNORE DELAYED_FLUSH_PROTECTION_TIME_MSEC
 *  1~100: DO DELAYED FLUSH, Use Delayed Flush list 
 * ----------------------------------------------------------------- */
    IDP_DEF( UInt, "DELAYED_FLUSH_LIST_PCT",
             IDP_ATTR_SL_ALL |
             IDP_ATTR_SH_ALL |
             IDP_ATTR_IU_ANY |
             IDP_ATTR_MS_ANY |
             IDP_ATTR_LC_EXTERNAL |
             IDP_ATTR_RD_WRITABLE |
             IDP_ATTR_ML_JUSTONE |
             IDP_ATTR_CK_CHECK ,
             0, 100, 30 );

    IDP_DEF( UInt, "DELAYED_FLUSH_PROTECTION_TIME_MSEC",
             IDP_ATTR_SL_ALL |
             IDP_ATTR_SH_ALL |
             IDP_ATTR_IU_ANY |
             IDP_ATTR_MS_ANY |
             IDP_ATTR_LC_EXTERNAL |
             IDP_ATTR_RD_WRITABLE |
             IDP_ATTR_ML_JUSTONE |
             IDP_ATTR_CK_CHECK ,
             0, 100000, 100 );
    /* PROJ-2669 end */

    // PROJ-1568 begin
    IDP_DEF( UInt, "HOT_TOUCH_CNT",
             IDP_ATTR_SL_ALL |
             IDP_ATTR_SH_ALL |
             IDP_ATTR_IU_ANY |
             IDP_ATTR_MS_ANY |
             IDP_ATTR_CK_CHECK |
             IDP_ATTR_LC_EXTERNAL |
             IDP_ATTR_RD_WRITABLE |
             IDP_ATTR_ML_JUSTONE,
             1, ID_UINT_MAX, 2 );

    IDP_DEF(UInt, "BUFFER_VICTIM_SEARCH_INTERVAL",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, IDV_MAX_TIME_INTERVAL_MSEC, 3000); // millisecond ������

    IDP_DEF(UInt, "BUFFER_VICTIM_SEARCH_PCT",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 100, 40);

    IDP_DEF( UInt, "HOT_LIST_PCT",
             IDP_ATTR_SL_ALL |
             IDP_ATTR_SH_ALL |
             IDP_ATTR_IU_ANY |
             IDP_ATTR_MS_ANY |
             IDP_ATTR_LC_EXTERNAL |
             IDP_ATTR_RD_WRITABLE |
             IDP_ATTR_ML_JUSTONE |
             IDP_ATTR_CK_CHECK ,
             0, 100, 0 );

    IDP_DEF( UInt, "BUFFER_HASH_BUCKET_DENSITY",
             IDP_ATTR_SL_ALL |
             IDP_ATTR_SH_ALL |
             IDP_ATTR_IU_ANY |
             IDP_ATTR_MS_ANY |
             IDP_ATTR_CK_CHECK |
             IDP_ATTR_LC_EXTERNAL |
             IDP_ATTR_RD_READONLY |
             IDP_ATTR_ML_JUSTONE,
             1, 100, 1 );

    IDP_DEF( UInt, "BUFFER_HASH_CHAIN_LATCH_DENSITY",
             IDP_ATTR_SL_ALL |
             IDP_ATTR_SH_ALL |
             IDP_ATTR_IU_ANY |
             IDP_ATTR_MS_ANY |
             IDP_ATTR_CK_CHECK |
             IDP_ATTR_LC_EXTERNAL |
             IDP_ATTR_RD_READONLY |
             IDP_ATTR_ML_JUSTONE,
             1, 100, 1 );

    /* BUG-32750 [sm-disk-resource] The hash bucket numbers of BCB(Buffer
     * Control Block) are concentrated in low number. 
     * ���� ( FID + SPACEID + FPID ) % HASHSIZE �� �� DataFile�� ũ�Ⱑ ����
     * ��� �������� HashValue�� ������ ������ �ִ�. �̿� ���� ������ ����
     * �����Ѵ�
     * ( ( SPACEID * PERMUTE1 +  FID ) * PERMUTE2 + PID ) % HASHSIZE
     * PERMUTE1�� Tablespace�� �ٸ��� ���� ������� ������ �Ǵ°�,
     * PERMUTE2�� Datafile FID�� �ٸ��� ���� ������� ������ �Ǵ°�,
     * �Դϴ�.
     * PERMUTE1�� Tablespace�� Datafile ��� �������� ���� ���� ���� �����ϸ�
     * PERMUTE2�� Datafile�� Page ��� �������� ���� ���� ���� �����մϴ�. */
    IDP_DEF( UInt, "__BUFFER_HASH_PERMUTE1",
             IDP_ATTR_SL_ALL |
             IDP_ATTR_SH_ALL |
             IDP_ATTR_IU_ANY |
             IDP_ATTR_MS_ANY |
             IDP_ATTR_CK_CHECK |
             IDP_ATTR_LC_INTERNAL |
             IDP_ATTR_RD_READONLY |
             IDP_ATTR_ML_JUSTONE,
             1, ID_UINT_MAX, 8 );
    IDP_DEF( UInt, "__BUFFER_HASH_PERMUTE2",
             IDP_ATTR_SL_ALL |
             IDP_ATTR_SH_ALL |
             IDP_ATTR_IU_ANY |
             IDP_ATTR_MS_ANY |
             IDP_ATTR_CK_CHECK |
             IDP_ATTR_LC_INTERNAL |
             IDP_ATTR_RD_READONLY |
             IDP_ATTR_ML_JUSTONE,
             1, ID_UINT_MAX, 65536 );

    /* BUG-21964: BUFFER_FLUSHER_CNT�Ӽ��� Internal�Դϴ�. External��
     *            �ٲ�� �մϴ�.
     *
     *  BUFFER_FLUSH_LIST_CNT
     *  BUFFER_PREPARE_LIST_CNT
     *  BUFFER_CHECKPOINT_LIST_CNT
     *  BUFFER_LRU_LIST_CNT
     *  BUFFER_FLUSHER_CNT �� External�� ����.
     *  */
    IDP_DEF( UInt, "BUFFER_LRU_LIST_CNT",
             IDP_ATTR_SL_ALL |
             IDP_ATTR_SH_ALL |
             IDP_ATTR_IU_ANY |
             IDP_ATTR_MS_ANY |
             IDP_ATTR_CK_CHECK |
             IDP_ATTR_LC_EXTERNAL |
             IDP_ATTR_RD_READONLY |
             IDP_ATTR_ML_JUSTONE,
             1, 64, 7 );

    /* BUG-22070: Buffer Manager���� �л�ó�� �۾���
     * ����� �̷������ �ʽ��ϴ�.
     * XXX
     * ���� ���ҽ� ������ �ϴ� ����Ʈ ������ 1�� ����
     * �Ͽ� ������ �ذ��մϴ�. */

    IDP_DEF( UInt, "BUFFER_FLUSH_LIST_CNT",
             IDP_ATTR_SL_ALL |
             IDP_ATTR_SH_ALL |
             IDP_ATTR_IU_ANY |
             IDP_ATTR_MS_ANY |
             IDP_ATTR_CK_CHECK |
             IDP_ATTR_LC_EXTERNAL |
             IDP_ATTR_RD_READONLY |
             IDP_ATTR_ML_JUSTONE,
             1, 64, 1 );

    IDP_DEF( UInt, "BUFFER_PREPARE_LIST_CNT",
             IDP_ATTR_SL_ALL |
             IDP_ATTR_SH_ALL |
             IDP_ATTR_IU_ANY |
             IDP_ATTR_MS_ANY |
             IDP_ATTR_CK_CHECK |
             IDP_ATTR_LC_EXTERNAL |
             IDP_ATTR_RD_READONLY |
             IDP_ATTR_ML_JUSTONE,
             1, 64, 7 );

    IDP_DEF( UInt, "BUFFER_CHECKPOINT_LIST_CNT",
             IDP_ATTR_SL_ALL |
             IDP_ATTR_SH_ALL |
             IDP_ATTR_IU_ANY |
             IDP_ATTR_MS_ANY |
             IDP_ATTR_CK_CHECK |
             IDP_ATTR_LC_EXTERNAL |
             IDP_ATTR_RD_READONLY |
             IDP_ATTR_ML_JUSTONE,
             1, 64, 4 );

    IDP_DEF( UInt, "BUFFER_FLUSHER_CNT",
             IDP_ATTR_SL_ALL |
             IDP_ATTR_SH_ALL |
             IDP_ATTR_IU_ANY |
             IDP_ATTR_MS_ANY |
             IDP_ATTR_CK_CHECK |
             IDP_ATTR_LC_EXTERNAL |
             IDP_ATTR_RD_READONLY |
             IDP_ATTR_ML_JUSTONE,
             1, 16, 2 );
#endif

#if defined(ALTIBASE_PRODUCT_HDB)
    IDP_DEF( UInt, "BUFFER_IO_BUFFER_SIZE",
             IDP_ATTR_SL_ALL |
             IDP_ATTR_SH_ALL |
             IDP_ATTR_IU_ANY |
             IDP_ATTR_MS_ANY |
             IDP_ATTR_CK_CHECK |
             IDP_ATTR_LC_INTERNAL |
             IDP_ATTR_RD_READONLY |
             IDP_ATTR_ML_JUSTONE,
             1, 256, 64 );

    /* BUG-36092 [sm] If ALTIBASE_BUFFER_AREA_SIZE= "8K", server can't clean
     * BUFFER_AREA_SIZE �� �ʹ� ������ victim�� �������� ���� server start�� ���� ���մϴ�. */
    IDP_DEF( ULong, "BUFFER_AREA_SIZE",
             IDP_ATTR_SL_ALL |
             IDP_ATTR_SH_ALL |
             IDP_ATTR_IU_ANY |
             IDP_ATTR_MS_ANY |
             IDP_ATTR_CK_CHECK |
             IDP_ATTR_LC_EXTERNAL |
             IDP_ATTR_RD_WRITABLE |
             IDP_ATTR_ML_JUSTONE,
             8*1024*10, ID_ULONG_MAX, 128*1024*1024 );

    IDP_DEF( ULong, "BUFFER_AREA_CHUNK_SIZE",
             IDP_ATTR_SL_ALL |
             IDP_ATTR_SH_ALL |
             IDP_ATTR_IU_ANY |
             IDP_ATTR_MS_ANY |
             IDP_ATTR_CK_CHECK |
             IDP_ATTR_LC_EXTERNAL |
             IDP_ATTR_RD_READONLY |
             IDP_ATTR_ML_JUSTONE,
             8*1024, ID_ULONG_MAX, 32*1024*1024 );

    // BUG-48042: BUFFER AREA Parallel ���� ������Ƽ
    // default ���� ���μ��� �� / 4   
    IDP_DEF( UInt, "__BUFFAREA_CREATE_PARALLEL_DEGREE",
             IDP_ATTR_SL_ALL |
             IDP_ATTR_SH_ALL |
             IDP_ATTR_IU_ANY |
             IDP_ATTR_MS_ANY |
             IDP_ATTR_CK_CHECK |
             IDP_ATTR_LC_INTERNAL |
             IDP_ATTR_RD_READONLY |
             IDP_ATTR_ML_JUSTONE,
             1, 512, 
             IDL_MAX((idlVA::getProcessorCount()/4), 1));   

#if 0
    IDP_DEF( UInt, "BUFFER_PINNING_COUNT",
             IDP_ATTR_SL_ALL |
             IDP_ATTR_SH_ALL |
             IDP_ATTR_IU_ANY |
             IDP_ATTR_MS_ANY |
             IDP_ATTR_CK_CHECK |
             IDP_ATTR_LC_INTERNAL |
             IDP_ATTR_RD_READONLY |
             IDP_ATTR_ML_JUSTONE,
             0, 256, 5 );

    IDP_DEF( UInt, "BUFFER_PINNING_HISTORY_COUNT",
             IDP_ATTR_SL_ALL |
             IDP_ATTR_SH_ALL |
             IDP_ATTR_IU_ANY |
             IDP_ATTR_MS_ANY |
             IDP_ATTR_CK_CHECK |
             IDP_ATTR_LC_INTERNAL |
             IDP_ATTR_RD_READONLY |
             IDP_ATTR_ML_JUSTONE,
             0, 256, 5 );
#endif
    IDP_DEF( UInt, "DEFAULT_FLUSHER_WAIT_SEC",
             IDP_ATTR_SL_ALL |
             IDP_ATTR_SH_ALL |
             IDP_ATTR_IU_ANY |
             IDP_ATTR_MS_ANY |
             IDP_ATTR_LC_EXTERNAL |
             IDP_ATTR_RD_WRITABLE |
             IDP_ATTR_ML_JUSTONE |
             IDP_ATTR_CK_CHECK ,
             1, IDV_MAX_TIME_INTERVAL_SEC, 1 );

    IDP_DEF( UInt, "MAX_FLUSHER_WAIT_SEC",
             IDP_ATTR_SL_ALL |
             IDP_ATTR_SH_ALL |
             IDP_ATTR_IU_ANY |
             IDP_ATTR_MS_ANY |
             IDP_ATTR_LC_EXTERNAL |
             IDP_ATTR_RD_WRITABLE |
             IDP_ATTR_ML_JUSTONE |
             IDP_ATTR_CK_CHECK ,
             1, ID_UINT_MAX, 10 );
#endif

    IDP_DEF( ULong, "CHECKPOINT_FLUSH_COUNT",
             IDP_ATTR_SL_ALL |
             IDP_ATTR_SH_ALL |
             IDP_ATTR_IU_ANY |
             IDP_ATTR_MS_ANY |
             IDP_ATTR_LC_EXTERNAL |
             IDP_ATTR_RD_WRITABLE |
             IDP_ATTR_ML_JUSTONE |
             IDP_ATTR_CK_CHECK ,
             1, ID_ULONG_MAX, 64 );

    // BUG-22857 restart recovery�� ���� LogFile��
    // �ݴ�� ���ϸ� ���ܵ� LogFile��
    IDP_DEF( UInt, "FAST_START_LOGFILE_TARGET",
             IDP_ATTR_SL_ALL |
             IDP_ATTR_SH_ALL |
             IDP_ATTR_IU_ANY |
             IDP_ATTR_MS_ANY |
             IDP_ATTR_LC_EXTERNAL |
             IDP_ATTR_RD_WRITABLE |
             IDP_ATTR_ML_JUSTONE |
             IDP_ATTR_CK_CHECK ,
             0, ID_UINT_MAX, 100 );

    // BUG-22857 restart recovery�� ���� redo page��
    // �ݴ�� ���ϸ� ���ܵ� dirty page��
    IDP_DEF( ULong, "FAST_START_IO_TARGET",
             IDP_ATTR_SL_ALL |
             IDP_ATTR_SH_ALL |
             IDP_ATTR_IU_ANY |
             IDP_ATTR_MS_ANY |
             IDP_ATTR_LC_EXTERNAL |
             IDP_ATTR_RD_WRITABLE |
             IDP_ATTR_ML_JUSTONE |
             IDP_ATTR_CK_CHECK ,
             0, ID_ULONG_MAX, 10000 );

    IDP_DEF( UInt, "LOW_PREPARE_PCT",
             IDP_ATTR_SL_ALL |
             IDP_ATTR_SH_ALL |
             IDP_ATTR_IU_ANY |
             IDP_ATTR_MS_ANY |
             IDP_ATTR_LC_EXTERNAL |
             IDP_ATTR_RD_WRITABLE |
             IDP_ATTR_ML_JUSTONE |
             IDP_ATTR_CK_CHECK ,
             0, 100, 1 );

    IDP_DEF( UInt, "HIGH_FLUSH_PCT",
             IDP_ATTR_SL_ALL |
             IDP_ATTR_SH_ALL |
             IDP_ATTR_IU_ANY |
             IDP_ATTR_MS_ANY |
             IDP_ATTR_LC_EXTERNAL |
             IDP_ATTR_RD_WRITABLE |
             IDP_ATTR_ML_JUSTONE |
             IDP_ATTR_CK_CHECK ,
             0, 100, 5 );

    IDP_DEF( UInt, "LOW_FLUSH_PCT",
             IDP_ATTR_SL_ALL |
             IDP_ATTR_SH_ALL |
             IDP_ATTR_IU_ANY |
             IDP_ATTR_MS_ANY |
             IDP_ATTR_LC_EXTERNAL |
             IDP_ATTR_RD_WRITABLE |
             IDP_ATTR_ML_JUSTONE |
             IDP_ATTR_CK_CHECK ,
             0, 100, 1 );

    IDP_DEF( UInt, "TOUCH_TIME_INTERVAL",
             IDP_ATTR_SL_ALL |
             IDP_ATTR_SH_ALL |
             IDP_ATTR_IU_ANY |
             IDP_ATTR_MS_ANY |
             IDP_ATTR_LC_EXTERNAL |
             IDP_ATTR_RD_WRITABLE |
             IDP_ATTR_ML_JUSTONE |
             IDP_ATTR_CK_CHECK ,
             0, 100, 3 );

    IDP_DEF( UInt, "CHECKPOINT_FLUSH_MAX_WAIT_SEC",
             IDP_ATTR_SL_ALL |
             IDP_ATTR_SH_ALL |
             IDP_ATTR_IU_ANY |
             IDP_ATTR_MS_ANY |
             IDP_ATTR_LC_EXTERNAL |
             IDP_ATTR_RD_WRITABLE |
             IDP_ATTR_ML_JUSTONE |
             IDP_ATTR_CK_CHECK ,
             0, ID_UINT_MAX, 10 );

    IDP_DEF( UInt, "CHECKPOINT_FLUSH_MAX_GAP",
             IDP_ATTR_SL_ALL |
             IDP_ATTR_SH_ALL |
             IDP_ATTR_IU_ANY |
             IDP_ATTR_MS_ANY |
             IDP_ATTR_LC_EXTERNAL |
             IDP_ATTR_RD_WRITABLE |
             IDP_ATTR_ML_JUSTONE |
             IDP_ATTR_CK_CHECK ,
             0, ID_UINT_MAX, 10 );

    IDP_DEF( UInt, "BLOCK_ALL_TX_TIME_OUT",
             IDP_ATTR_SL_ALL |
             IDP_ATTR_SH_ALL |
             IDP_ATTR_IU_ANY |
             IDP_ATTR_MS_ANY |
             IDP_ATTR_CK_CHECK |
             IDP_ATTR_LC_EXTERNAL |
             IDP_ATTR_RD_WRITABLE |
             IDP_ATTR_ML_JUSTONE,
             0, UINT_MAX, 3);

    IDP_DEF( UInt, "DB_FILE_MULTIPAGE_READ_COUNT",
             IDP_ATTR_SL_ALL |
             IDP_ATTR_SH_ALL |
             IDP_ATTR_IU_ANY |
             IDP_ATTR_MS_ANY |
             IDP_ATTR_CK_CHECK |
             IDP_ATTR_LC_EXTERNAL |
             IDP_ATTR_RD_WRITABLE |
             IDP_ATTR_ML_JUSTONE,
             1, 128, 8);

    IDP_DEF( UInt, "SMALL_TABLE_THRESHOLD",
             IDP_ATTR_SL_ALL |
             IDP_ATTR_SH_ALL |
             IDP_ATTR_IU_ANY |
             IDP_ATTR_MS_ANY |
             IDP_ATTR_CK_CHECK |
             IDP_ATTR_LC_EXTERNAL |
             IDP_ATTR_RD_WRITABLE |
             IDP_ATTR_ML_JUSTONE,
             1, UINT_MAX, 128);

    //proj-1568 end

    // PROJ-2068 Direct-Path INSERT ���� ����
    IDP_DEF( SLong, "__DPATH_BUFF_PAGE_ALLOC_RETRY_USEC",
             IDP_ATTR_SL_ALL |
             IDP_ATTR_SH_ALL |
             IDP_ATTR_IU_ANY |
             IDP_ATTR_MS_ANY |
             IDP_ATTR_CK_CHECK |
             IDP_ATTR_LC_INTERNAL |
             IDP_ATTR_RD_WRITABLE |
             IDP_ATTR_ML_JUSTONE,
             0, IDV_MAX_TIME_INTERVAL_USEC, 100000);

    IDP_DEF( UInt, "__DPATH_INSERT_ENABLE",
             IDP_ATTR_SL_ALL |
             IDP_ATTR_SH_ALL |
             IDP_ATTR_IU_ANY |
             IDP_ATTR_MS_ANY |
             IDP_ATTR_CK_CHECK |
             IDP_ATTR_LC_INTERNAL |
             IDP_ATTR_RD_WRITABLE |
             IDP_ATTR_ML_JUSTONE,
             0, 1, 0);

// sdp
#if 0 
    // TTS �Ҵ�� �ٸ� Ʈ������� �Ϸ�Ǳ⸦ ��ٸ��� �ð� (MSec.)
    IDP_DEF(ULong, "TRANS_WAIT_TIME_FOR_TTS",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            10000000, ID_ULONG_MAX, 10000000);
#endif

    /* BUG-47223 : Ʈ������� �Ϸ�Ǳ⸦ ��ٸ��� �ð� (uSec.) */
    IDP_DEF( ULong, "TRANS_WAIT_TIME",
             IDP_ATTR_SL_ALL |
             IDP_ATTR_SH_ALL |
             IDP_ATTR_IU_ANY |
             IDP_ATTR_MS_ANY |
             IDP_ATTR_LC_INTERNAL |
             IDP_ATTR_RD_WRITABLE |
             IDP_ATTR_ML_JUSTONE  |
             IDP_ATTR_CK_CHECK,
             0, ID_ULONG_MAX, ID_ULONG_MAX );

/* --------------------------------------------------------------------
 * page�� üũ���� ����
 * 0 : ������ ������ LSN�� page�� valid ���� Ȯ���ϱ� ���� ����Ѵ�.
 * 1 : ������ ���� ���� üũ�� ���� valid Ȯ���� ���� ����Ѵ�.
 * ----------------------------------------------------------------- */
    IDP_DEF(UInt, "CHECKSUM_METHOD",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE | 
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 0 );

/* --------------------------------------------------------------------
 * FLM( Free List Management )�� TableSpace�� Test�� ���ؼ� �߰���.
 * �ϳ��� ExteDirPage���� ���� ExtDesc�� ������ ����.
 * ----------------------------------------------------------------- */

    IDP_DEF(UInt, "__FMS_EXTDESC_CNT_IN_EXTDIRPAGE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, ID_UINT_MAX, 0 );

/* --------------------------------------------------------------------
 * PCTFREE reserves space in the data block for updates to existing rows.
 * It represents a percentage of the block size. Before reaching PCTFREE,
 * the free space is used both for insertion of new rows
 * and by the growth of the data block header.
 * ----------------------------------------------------------------- */
    IDP_DEF(UInt, "PCTFREE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 99, 10 );

/* --------------------------------------------------------------------
 * PCTUSED determines when a block is available for inserting new rows
 * after it has become full(reached PCTFREE).
 * Until the space used is less then PCTUSED,
 * the block is not considered for insertion.
----------------------------------------------------------------- */
    IDP_DEF(UInt, "PCTUSED",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 99, 40 );

    IDP_DEF(UInt, "TABLE_INITRANS",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 120, 2 );

    IDP_DEF(UInt, "TABLE_MAXTRANS",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 120, 120 );

    IDP_DEF(UInt, "INDEX_INITRANS",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 50, 8 );

    IDP_DEF(UInt, "INDEX_MAXTRANS",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 50, 50 ); /* BUG-48064 : CTS MAX ������ 30 -> 50���� ������Ŵ */

    /*
     * PROJ-1671 Bitmap Tablespace And Segment Space Management
     *
     *   �⺻ ���׸�Ʈ �ʱ� Extent ���� ����
     */
    IDP_DEF( UInt, "DEFAULT_SEGMENT_STORAGE_INITEXTENTS",
             IDP_ATTR_SL_ALL |
             IDP_ATTR_SH_ALL |
             IDP_ATTR_IU_ANY |
             IDP_ATTR_MS_ANY |
             IDP_ATTR_LC_EXTERNAL |
             IDP_ATTR_RD_READONLY |
             IDP_ATTR_ML_JUSTONE  |
             IDP_ATTR_CK_CHECK,
             1, ID_UINT_MAX, 1);

    /*
     *   �⺻ ���׸�Ʈ Ȯ�� Extent ���� ����
     */
    IDP_DEF(UInt, "DEFAULT_SEGMENT_STORAGE_NEXTEXTENTS",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            1, ID_UINT_MAX, 1 );

    /*
     *   �⺻ ���׸�Ʈ �ּ� Extent ���� ����
     */
    IDP_DEF(UInt, "DEFAULT_SEGMENT_STORAGE_MINEXTENTS",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            1, ID_UINT_MAX, 1);

    /*
     *   �⺻ ���׸�Ʈ �ִ� Extent ���� ����
     */
    IDP_DEF(UInt, "DEFAULT_SEGMENT_STORAGE_MAXEXTENTS",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            1, ID_UINT_MAX, ID_UINT_MAX);

    IDP_DEF(UInt, "__DISK_TMS_IGNORE_HINT_PID",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 0);

    IDP_DEF(SInt, "__DISK_TMS_MANUAL_SLOT_NO_IN_ITBMP",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            -1, 1023, -1);

    IDP_DEF(SInt, "__DISK_TMS_MANUAL_SLOT_NO_IN_LFBMP",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            -1, 1023, -1);

    /*
     *   TMS�� It-BMP���� ������ �ִ� Lf-BMP �ĺ���
     */
    IDP_DEF(UInt, "__DISK_TMS_CANDIDATE_LFBMP_CNT",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            1, 1024, 10);

    /*
     *   TMS�� Lf-BMP���� ������ �ִ� Page �ĺ���
     */
    IDP_DEF(UInt, "__DISK_TMS_CANDIDATE_PAGE_CNT",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            1, 1024, 5);


    /*
     *   TMS���� �ϳ��� Rt-BMP�� ������ �� �ִ� �ִ� Slot ��
     */
    IDP_DEF(UInt, "__DISK_TMS_MAX_SLOT_CNT_PER_RTBMP",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            1, 65536, 960 );    /* BUG-37695 */

    /*
     *   TMS���� �ϳ��� It-BMP�� ������ �� �ִ� �ִ� Slot ��
     */
    IDP_DEF(UInt, "__DISK_TMS_MAX_SLOT_CNT_PER_ITBMP",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            1, 65536, 1000);

    /*
     *   TMS���� �ϳ��� ExtDir�� ������ �� �ִ� �ִ� Slot ��
     */
    IDP_DEF(UInt, "__DISK_TMS_MAX_SLOT_CNT_PER_EXTDIR",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            1, 65536, 480 );    /* BUG-37695 */

    /* BUG-28935 hint insertable page id array�� ũ�⸦
     * ����ڰ� ���� �� �� �ֵ��� property�� �߰�*/
    IDP_DEF(UInt, "__DISK_TMS_HINT_PAGE_ARRAY_SIZE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            4, 4096, 1024);

    /* BUG-28935 hint insertable page id array�� alloc�ϴ� �ñ� ����
     *  0 - segment cache������ ���� alloc�Ѵ�.
     *  1 - segment cache�����ÿ��� �Ҵ����� �ʰ�,
     *      �� �� ó�� insertable page�� ã�� �� alloc�Ѵ�. (default)
     * */
    IDP_DEF(UInt, "__DISK_TMS_DELAYED_ALLOC_HINT_PAGE_ARRAY",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 1);
    /*
     * PROJ-1704 Disk MVCC Renewal
     *
     * TSS ���׸�Ʈ�� ExtDir �������� ExtDesc ���� ����
     * 4 * 256K = 1M
     */
    IDP_DEF(UInt, "TSSEG_EXTDESC_COUNT_PER_EXTDIR",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            1, 128, 4 );

    /*
     * Undo ���׸�Ʈ�� ExtDir �������� ExtDesc ���� ����
     * 8 * 256K = 2M
     */
    IDP_DEF(UInt, "UDSEG_EXTDESC_COUNT_PER_EXTDIR",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            1, 128, 8 );

    /*
     * TSSEG ���׸�Ʈ�� ũ�Ⱑ ���� ������Ƽ���� ���� ���
     * Shrink ���� ���� �⺻�� 6M
     */
    IDP_DEF(UInt, "TSSEG_SIZE_SHRINK_THRESHOLD",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, ID_UINT_MAX, (6*1024*1024) );

    /*
     * UDSEG ���׸�Ʈ�� ũ�Ⱑ ���� ������Ƽ���� ���� ���
     * Shrink ���� ���� 6M
     */
    IDP_DEF(UInt, "UDSEG_SIZE_SHRINK_THRESHOLD",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, ID_UINT_MAX, (6*1024*1024) );

    /*
     * UDSEG ���׸�Ʈ�� Steal Operation�� Retry Count
     */
    IDP_DEF(UInt, "RETRY_STEAL_COUNT_",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 512, 512 );

/* --------------------------------------------------------------------
 * DW buffer�� ����� ���ΰ� �����Ѵ�.
 * ----------------------------------------------------------------- */
    IDP_DEF(UInt, "USE_DW_BUFFER",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 0 );

    /* BUG-27776 [sm-disk-recovery] the server startup can be fail since the
     * dw file is removed after DW recovery. 
     * Server Startup�߿��� ARTTest�� ���� �ʽ��ϴ�. ���� Startup ����
     * ���� �׽�Ʈ�� �����ϵ���, Hidden Property�� �߰��մϴ�. */
    IDP_DEF(UInt, "__SM_STARTUP_TEST",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, ID_UINT_MAX, 0 );

/* --------------------------------------------------------------------
 * create DB�� ���� property
 * ----------------------------------------------------------------- */

    /*
     * PROJ-1671 Bitmap-based Tablespace And Segment Space Management
     *
     * �⺻ ���׸�Ʈ �������� ��� ����
     * 0 : MANUAL ( FMS )
     * 1 : AUTO   ( TMS ) ( �⺻�� )
     */
    IDP_DEF(UInt, "DEFAULT_SEGMENT_MANAGEMENT_TYPE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 1);

/* --------------------------------------------------------------------
 * �ϳ��� extent group ���� ������ extent�� ������ ���� property
 * ----------------------------------------------------------------- */

    /*
     * PROJ-1671 Bitmap-based Tablespace And Segment Space Management
     *
     * �� property�� �׽�Ʈ�� ���Ͽ��� ����Ѵ�.
     */
    IDP_DEF(UInt, "DEFAULT_EXTENT_CNT_FOR_EXTENT_GROUP",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, ID_UINT_MAX , 0); //default �� 0�̸� ������ �ִ밹���� ����

/* --------------------------------------------------------------------
 * for system data tablespace
 * system�� ���� tablespace(data, temp, undo)�� ���ʿ�
 * �ϳ��� datafile�� �����Ǹ�, max size�� �Ѿ�� datafile�� �߰��ȴ�.
 * �Ʒ��� tablespace ���� property �� ũ��� ���õ� ���� ���ʿ� �����Ǵ�
 * datafile�� default �Ӽ��� �����Ѵ�.
 * ----------------------------------------------------------------- */

    // �ι�° extent�� ũ��
    // To Fix PR-12035
    // �ּҰ� ����
    // >= 2 Pages

    IDP_DEF(ULong, "SYS_DATA_TBS_EXTENT_SIZE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            (5 * IDP_SD_PAGE_SIZE), ID_ULONG_MAX, (512 * 1024));

    // �ι�° datafile�� �ʱ� ũ��
    // To Fix PR-12035
    IDP_DEF(ULong, "SYS_DATA_FILE_INIT_SIZE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            (8 * IDP_SD_PAGE_SIZE),
            IDP_DRDB_DATAFILE_MAX_SIZE,
            (100 * 1024 * 1024));

    // �ι�° data file�� �ִ� ũ��
    // To Fix PR-12035
    // �ּҰ� ����
    // >= SYS_DATA_FILE_INIT_SIZE
    // To Fix BUG-14662. set default to maximum of SInt
    IDP_DEF(ULong, "SYS_DATA_FILE_MAX_SIZE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            (8 * IDP_SD_PAGE_SIZE),
            IDP_DRDB_DATAFILE_MAX_SIZE,
            ID_ULONG(ID_UINT_MAX / 2));

    IDP_DEF(ULong, "SYS_DATA_FILE_NEXT_SIZE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            ( 8 * IDP_SD_PAGE_SIZE ),
            IDP_DRDB_DATAFILE_MAX_SIZE,
            ( 1 * 1024 * 1024 ));

/* --------------------------------------------------------------------
 * for system undo tablespace
 * ----------------------------------------------------------------- */

    // To Fix PR-12035
    // �ּҰ� ����
    // >= 2 Pages
    IDP_DEF(ULong, "SYS_UNDO_TBS_EXTENT_SIZE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            (5 * IDP_SD_PAGE_SIZE), ID_ULONG_MAX, (256 * 1024));

    // To Fix PR-12035
    IDP_DEF(ULong, "SYS_UNDO_FILE_INIT_SIZE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            (32 * IDP_SD_PAGE_SIZE),
            IDP_DRDB_DATAFILE_MAX_SIZE,
            (100 * 1024 * 1024));

    // To Fix PR-12035
    // �ּҰ� ����
    // >= SYS_UNDO_FILE_INIT_SIZE
    // To Fix BUG-14662. set default to maximum of SInt
    IDP_DEF(ULong, "SYS_UNDO_FILE_MAX_SIZE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            (32 * IDP_SD_PAGE_SIZE),
            IDP_DRDB_DATAFILE_MAX_SIZE,
            ID_ULONG(ID_UINT_MAX / 2));

    IDP_DEF(ULong, "SYS_UNDO_FILE_NEXT_SIZE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            (8 * IDP_SD_PAGE_SIZE),
            IDP_DRDB_DATAFILE_MAX_SIZE,
            (1 * 1024 * 1024));

 /* --------------------------------------------------------------------
 * for system temp tablespace
 * ----------------------------------------------------------------- */

    // To Fix PR-12035
    // �ּҰ� ����
    // >= 2 Pages
    IDP_DEF(ULong, "SYS_TEMP_TBS_EXTENT_SIZE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            (5 * IDP_SD_PAGE_SIZE), ID_ULONG_MAX, (512 * 1024));

    // To Fix PR-12035
    IDP_DEF(ULong, "SYS_TEMP_FILE_INIT_SIZE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            (8 * IDP_SD_PAGE_SIZE),
            IDP_DRDB_DATAFILE_MAX_SIZE,
            (100 * 1024 * 1024));

    // To Fix PR-12035
    // �ּҰ� ����
    // >= SYS_TEMP_FILE_INIT_SIZE
    // To Fix BUG-14662. set default to maximum of SInt
    IDP_DEF(ULong, "SYS_TEMP_FILE_MAX_SIZE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            (8 * IDP_SD_PAGE_SIZE),
            IDP_DRDB_DATAFILE_MAX_SIZE,
            ID_ULONG(ID_UINT_MAX / 2));

    IDP_DEF(ULong, "SYS_TEMP_FILE_NEXT_SIZE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            (8 * IDP_SD_PAGE_SIZE),
            IDP_DRDB_DATAFILE_MAX_SIZE,
            (1 * 1024 * 1024));

 /* --------------------------------------------------------------------
 * PROJ-1490 ����������Ʈ ����ȭ �� �޸� �ݳ�
 * ----------------------------------------------------------------- */

    // �����ͺ��̽� Ȯ���� ������ Expand Chunk�� ������ Page�� ��.
    IDP_DEF(UInt, "EXPAND_CHUNK_PAGE_COUNT",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            2*IDP_MAX_PAGE_LIST_COUNT, /* min */
            /* (����ȭ�� DB FREE LIST�� �ּ� 1�� �̻� �����־�� �ϱ� ����) */
            ID_UINT_MAX,         /* max */
            IDP_DEFAULT_EXPAND_CHUNK_PAGE_COUNT );          /* 128 Pages */

    // ������ ���� Page List���� ���� ��� List�� ����ȭ �� �� �����Ѵ�.
    //
    // �����ͺ��̽� Free Page List
    // ���̺��� Allocated Page List
    // ���̺��� Free Page List
    //
    IDP_DEF(UInt, "PAGE_LIST_GROUP_COUNT",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
           1, IDP_MAX_PAGE_LIST_COUNT, 1);


    // Expand ChunkȮ��ÿ� Free Page���� �������� ���ļ�
    // ����ȭ�� Free Page List�� �й�ȴ�.
    //
    // �� ��, �ѹ��� ��� Page�� Free Page List�� �й������� �����Ѵ�.
    //
    // default(0) : �ּҷ� �й�ǰ� �ڵ� ���
    IDP_DEF(UInt, "PER_LIST_DIST_PAGE_COUNT",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, ID_UINT_MAX, 0);

    // Free Page List�� List ������ ������ �ϴ� �ּ����� Page��
    // Free Page List ���ҽÿ� �ּ��� �� ��ŭ�� Page��
    // List�� ���� ���� �� �ִ� ��쿡�� Free Page List�� �����Ѵ�.
    IDP_DEF(UInt, "MIN_PAGES_ON_DB_FREE_LIST",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            1, ID_UINT_MAX, 16);


/* --------------------------------------------------------------------
 * user�� create�ϴ� data tablespace�� ���� �Ӽ�
 * ----------------------------------------------------------------- */

    // To Fix PR-12035
    // �ּҰ� ����
    // >= 2 Pages
    IDP_DEF(ULong, "USER_DATA_TBS_EXTENT_SIZE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            (5 * IDP_SD_PAGE_SIZE), ID_ULONG_MAX, (512 * 1024));

    // To Fix PR-12035
    IDP_DEF(ULong, "USER_DATA_FILE_INIT_SIZE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            (8 * IDP_SD_PAGE_SIZE),
            IDP_DRDB_DATAFILE_MAX_SIZE,
            (100 * 1024 * 1024));

    // To Fix PR-12035
    // �ּҰ� ����
    // >= USER_DATA_FILE_INIT_SIZE
    // To Fix BUG-14662. set default to maximum of SInt
    IDP_DEF(ULong, "USER_DATA_FILE_MAX_SIZE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            (8 * IDP_SD_PAGE_SIZE),
            IDP_DRDB_DATAFILE_MAX_SIZE,
            ID_ULONG(ID_UINT_MAX / 2));

    IDP_DEF(ULong, "USER_DATA_FILE_NEXT_SIZE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            (8 * IDP_SD_PAGE_SIZE),
            IDP_DRDB_DATAFILE_MAX_SIZE,
            (1 * 1024 * 1024));

/* --------------------------------------------------------------------
 * user�� create�ϴ� temp tablespace�� ���� �Ӽ�
 * ----------------------------------------------------------------- */

    // To Fix PR-12035
    // �ּҰ� ����
    // >= 2 Pages
    IDP_DEF(ULong, "USER_TEMP_TBS_EXTENT_SIZE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            (5 * IDP_SD_PAGE_SIZE), ID_ULONG_MAX, (512 * 1024));

    // To Fix PR-12035
    IDP_DEF(ULong, "USER_TEMP_FILE_INIT_SIZE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            (8 * IDP_SD_PAGE_SIZE),
            IDP_DRDB_DATAFILE_MAX_SIZE,
            (100 * 1024 * 1024));

    // To Fix PR-12035
    // �ּҰ� ����
    // >= USER_TEMP_FILE_INIT_SIZE
    // To Fix BUG-14662. set default to maximum of SInt
    IDP_DEF(ULong, "USER_TEMP_FILE_MAX_SIZE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            (8 * IDP_SD_PAGE_SIZE),
            IDP_DRDB_DATAFILE_MAX_SIZE,
            ID_ULONG(ID_UINT_MAX / 2));

    IDP_DEF(ULong, "USER_TEMP_FILE_NEXT_SIZE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            (8 * IDP_SD_PAGE_SIZE),
            IDP_DRDB_DATAFILE_MAX_SIZE,
            (1 * 1024 * 1024));

 // sdc
/* --------------------------------------------------------------------
 * PROJ-1595
 * Disk Index ����� In-Memory Sorting ����
 * ----------------------------------------------------------------- */

    IDP_DEF(ULong, "SORT_AREA_SIZE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            (512*1024), ID_ULONG_MAX, (1 * 1024 * 1024));

    /**************************************************************
     * PROJ-2201 Innovation in sorting and hashing(temp) 
     **************************************************************/
    IDP_DEF(ULong, "HASH_AREA_SIZE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            (3 * 1024 * 1024), ID_ULONG_MAX, (4 * 1024 * 1024));
    
    /* WA(WorkArea)�� �ʱ�ȭ �� �� ũ�� */
    IDP_DEF(ULong, "INIT_TOTAL_WA_SIZE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, ID_ULONG_MAX, ID_ULONG_MAX);

    /* WA(WorkArea)�� �ִ� �� ũ�� */
    IDP_DEF(ULong, "TOTAL_WA_SIZE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, ID_ULONG_MAX, (128 * 1024 * 1024));

    IDP_DEF(UInt,  "__TEMP_CHECK_UNIQUE_FOR_UPDATE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 1);

    // 8���� Extent, 4MB������ �Ҵ�.
    IDP_DEF(UInt, "__TEMP_MIN_INIT_WAEXTENT_COUNT",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, ID_UINT_MAX, 8 );

    // create temp table ������, ���� TOTAL_WA_EXTENT �� �����ϸ�
    // ��� �ʰ��ؼ� �ʱ�ȭ �Ҵ� �Ͽ� ���� �� ������ ���� ��Ÿ����.
    // ���� 0�̸�, �ʱ�ȭ ���� �ʰ� ����Ѵ�.
    IDP_DEF(UInt, "__TEMP_OVER_INIT_WAEXTENT_COUNT",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, ID_UINT_MAX, 4 );

    IDP_DEF(UInt, "__TEMP_ALLOC_WAEXTENT_COUNT",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            1, 1024, 4 );

    IDP_DEF(UInt, "__TEMP_INIT_WASEGMENT_COUNT",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, ID_UINT_MAX, 8 );

    /* TempTable�� Sort��,  QuickSort�� �κкκ��� ���� ��, �̸� MergeSort��
     * ��ġ�� ������ �����Ѵ�. �̶� QuickSort�� ������ �κ��� ũ�⸦ �Ʒ�
     * Property�� �����Ѵ�. */
    IDP_DEF(UInt,  "TEMP_SORT_PARTITION_SIZE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, ID_UINT_MAX,  4096);

    /* TempTable�� Sort��, Sort������ Sort��� ���� Group���� ����������.
     * ���� Sort������ ũ�⸦ �����Ѵ�. (�̿����� ���� Group�� ��������
     * �����Ѵ�. */
    IDP_DEF(UInt,  "TEMP_SORT_GROUP_RATIO",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            5, 90, 80 );

    /* Temp�� Hash��, Hash Slot�� Sub Hash�� ������.
     * �̶� Sub Hash �� �ش��ϴ� �����̴�.
     * ��ü HASH_AREA_SIZE�� ���� ������ ����������
     * ���� ũ��� 512KB������ �����ȴ�. */
    IDP_DEF(UInt,  "__TEMP_SUBHASH_GROUP_RATIO",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            1, 30, 4);

    /* Temp�� Hash��, Hash Slot�� Sub Hash�� ������.
     * �̶� Sub Hash �� �ش��ϴ� �����̴�.
     * ��ü HASH_AREA_SIZE�� ���� ������ ����������
     * ���� ũ��� 1MB������ �����ȴ�. */
    IDP_DEF(UInt,  "TEMP_HASH_GROUP_RATIO",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            1, 90, 6 );

    /* WA �� ������ ���, ��� ��õ� ������ �����Ѵ� */
    IDP_DEF(UInt,  "TEMP_ALLOC_TRY_COUNT",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            1, ID_UINT_MAX,  10000 );

    /* �������� ������ �����Ͽ� Row�� ����� �� ���� ���, �ش� Row�� �Ʒ�
     * ũ�⺸�� ũ�� �ɰ��� ����ϰ� ������ �������� ������� ����ü ���� Row
     * �� �����Ѵ�. */
    IDP_DEF(UInt,  "TEMP_ROW_SPLIT_THRESHOLD",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            1, 8192, 1024);

    /* TempTable ���� �� �ٸ� �۾��� ��ٷ��� �Ҷ�, ���� �ð��̴�. */
    IDP_DEF(UInt, "TEMP_SLEEP_INTERVAL",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, ID_UINT_MAX, 1000 );

    /* Temp�����, RangeScan�� ���� Index���� Key �ϳ��� �ִ� ũ���̴�. Key��
     * �̺��� ũ��, ������ �κ��� ExtraRow�� ������ �����Ѵ�. */
    IDP_DEF(UInt, "TEMP_MAX_KEY_SIZE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            64, 4096, 512 );

    /* Temp���� �� �� Temp������� ���࿩�θ� �����ϱ� ���� StatsWatchArray
     * �� ũ���̴�. �̰��� ������ ��Ȱ���� ������, ���� Temp��谡 ����
     * �������. */
    IDP_DEF(UInt, "TEMP_STATS_WATCH_ARRAY_SIZE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            10, ID_UINT_MAX, 1000 );

    /* StatsWatchArray�� �۾��� ����ϱ� ���� ���� �ð��̴�. �� �ð����� ����
     * �ɸ��� Temp������ StatsWatchArray�� ��ϵȴ�. */
    IDP_DEF(UInt, "TEMP_STATS_WATCH_TIME",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, ID_UINT_MAX, 10 );

    /* �����߻����� Dump�� ������ DIRECTORY. dumptd�� �м� �����ϴ�. */
    IDP_DEF(String, "TEMPDUMP_DIRECTORY",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_SK_PATH     |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, IDP_MAX_PROP_STRING_LEN, (SChar *)"?"IDL_FILE_SEPARATORS"trc");

    /* BUG-45403 �����߻����� Dump �� ���� �Ҽ� �ִ�. */
    IDP_DEF(UInt , "__TEMPDUMP_ENABLE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 0 );

    /* ����ó�� Test�� ���Ͽ�, ������ Nȸ �����ϸ� Abort��Ų��. */
    IDP_DEF(UInt, "__SM_TEMP_OPER_ABORT",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, ID_UINT_MAX, 0 );

    IDP_DEF(UInt, "__WCB_CLEAN_MEMSET",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 0 );

    IDP_DEF( UInt, "TEMP_HASH_BUCKET_DENSITY",
             IDP_ATTR_SL_ALL |
             IDP_ATTR_SH_ALL |
             IDP_ATTR_IU_ANY |
             IDP_ATTR_MS_ANY |
             IDP_ATTR_LC_EXTERNAL |
             IDP_ATTR_RD_READONLY |
             IDP_ATTR_ML_JUSTONE |
             IDP_ATTR_CK_CHECK,
             1, 100, 1 );

    IDP_DEF(UInt, "__TEMP_HASH_FETCH_SUBHASH_MAX_RATIO",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            10, 99, 90 );

/* --------------------------------------------------------------------
 * PROJ-1629
 * Memory Index ����� In-Memory Sorting ����
 * Min = 1204, Default = 32768
 * ----------------------------------------------------------------- */

    IDP_DEF(ULong, "MEMORY_INDEX_BUILD_RUN_SIZE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            (1024), ID_ULONG_MAX, ( 128 * 1024));

/* --------------------------------------------------------------------
 * PROJ-1629
 * Memory Index ����� Key Value�� ����ϱ� ���� Threshold
 * Min = 0, Default = 64
 * ----------------------------------------------------------------- */

    IDP_DEF(ULong, "MEMORY_INDEX_BUILD_VALUE_LENGTH_THRESHOLD",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, ID_ULONG_MAX, ( 64 ));

/* --------------------------------------------------------------------
 * PROJ-2162 RestartRiskReduction Begin
 * ----------------------------------------------------------------- */

/* --------------------------------------------------------------------
 * ���� ���н�, �� Property�� �÷��� ������ ������ų �� �ֽ��ϴ�.
 *
 * RECOVERY_NORMAL(0)    - Default
 * RECOVERY_EMERGENCY(1) - Check and Ignore inconsistent object.
 * RECOVERY_SKIP(2)      - Skip recovery.
 * ----------------------------------------------------------------- */
    IDP_DEF(UInt, "__EMERGENCY_STARTUP_POLICY",
            IDP_ATTR_SL_PFILE |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 2, 0);

/* --------------------------------------------------------------------
 * ���� ���з� DB�� ���������� ���, DB�� ��Ȳ�� ��ȭ�Ǵ� ���� ���� ����
 * ������ ��ü�� ���� ������ �����ϴ�. �� Property�� �׷��� ������ ���
 * �ϰ� �ϰ�, ��Ȳ ��ȭ�� ���� ���� �ټ� �������� ������ ������ �ϰ�
 * �մϴ�.
 * 0 - Default
 * 1 - Inconsistent Page���� �����ϸ� ������ �����ϸ� Ž��
 * 2 - ������ �����ϸ� Ž��
 * ----------------------------------------------------------------- */
    IDP_DEF(UInt, "__CRASH_TOLERANCE",
            IDP_ATTR_SL_PFILE |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 2, 0);

/* --------------------------------------------------------------------
 * �Ʒ� �� Property�� �ϳ��� �׷�����, �ش� LSN�� Log�� �����մϴ�.
 * ----------------------------------------------------------------- */
    IDP_DEF(UInt, "__SM_IGNORE_LFGID_IN_STARTUP",
            IDP_ATTR_SL_PFILE |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_MULTIPLE |
            IDP_ATTR_CK_CHECK,
            0, ID_UINT_MAX, 0);

    IDP_DEF(UInt, "__SM_IGNORE_FILENO_IN_STARTUP",
            IDP_ATTR_SL_PFILE |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_MULTIPLE |
            IDP_ATTR_CK_CHECK,
            0, ID_UINT_MAX, 0);

    IDP_DEF(UInt, "__SM_IGNORE_OFFSET_IN_STARTUP",
            IDP_ATTR_SL_PFILE |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_MULTIPLE |
            IDP_ATTR_CK_CHECK,
            0, ID_UINT_MAX, 0);

/* --------------------------------------------------------------------
 * SpaceID * 2^32 + PageID�� ���� �����ϸ�, �ش� �������� �����մϴ�.
 * ----------------------------------------------------------------- */
    IDP_DEF(ULong, "__SM_IGNORE_PAGE_IN_STARTUP",
            IDP_ATTR_SL_PFILE |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_MULTIPLE |
            IDP_ATTR_CK_CHECK,
            0, ID_ULONG_MAX, 0);

/* --------------------------------------------------------------------
 * TableOID�� �Է��ϸ�, �ش� Table�� ������ �����մϴ�.
 * ----------------------------------------------------------------- */
    IDP_DEF(ULong, "__SM_IGNORE_TABLE_IN_STARTUP",
            IDP_ATTR_SL_PFILE |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_MULTIPLE |
            IDP_ATTR_CK_CHECK,
            0, ID_ULONG_MAX, 0);

/* --------------------------------------------------------------------
 * IndexID�� �Է��ϸ�, �ش� Index�� ������ �����մϴ�.
 * ----------------------------------------------------------------- */
    IDP_DEF(UInt, "__SM_IGNORE_INDEX_IN_STARTUP",
            IDP_ATTR_SL_PFILE |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_MULTIPLE |
            IDP_ATTR_CK_CHECK,
            0, ID_UINT_MAX, 0);

/* --------------------------------------------------------------------
 * RedoLogic�� ServiceLog���� ��ġ�� �񱳸� ���� Bug�� ã���ϴ�.
 * ----------------------------------------------------------------- */
    IDP_DEF(UInt, "__SM_ENABLE_STARTUP_BUG_DETECTOR",
            IDP_ATTR_SL_PFILE |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 0);

/* --------------------------------------------------------------------
 * Minitransaction Rollback�� ���� �׽�Ʈ�� �Ѵ�.
 * ������ �� ��ŭ MtxCommit�� �̷���� �� Rollback�� �õ��Ѵ�.
 * Debug��忡���� �����Ѵ�.
 * ----------------------------------------------------------------- */
    IDP_DEF(UInt, "__SM_MTX_ROLLBACK_TEST",
            IDP_ATTR_SL_PFILE |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, ID_UINT_MAX, 0);

/* --------------------------------------------------------------------
 * PROJ-2162 RestartRiskReduction End
 * ----------------------------------------------------------------- */

/* --------------------------------------------------------------------
 * BUG-38515 ���� ���۽� SCN�� üũ�Ͽ� SCN�� INFINITE �� �� ��� ������ �����.  
 * �� ������Ƽ�� �� ��Ȳ���� ������ ������ �ʰ� ������ ����ϱ� ���� ������ �Ѵ�. 
 * �� ���� 0�� ��� SCN�� INFINITE�� ��� ������ �����Ѵ�.
 * �� ���� 1�� ��� SCN�� INFINITE�� ��쿡�� ������ ������ �ʰ�
 * ���� ������ ����� �� �״�� �����Ѵ�.
 * BUG-41600 ���� 2 �� ��츦 �߰��Ѵ�. SCN�� INFINITE�� ��� ���̺��� �����Ѵ�.
 * ----------------------------------------------------------------- */
    IDP_DEF(UInt, "__SM_SKIP_CHECKSCN_IN_STARTUP",
            IDP_ATTR_SL_PFILE |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 2, 0);

/* --------------------------------------------------------------------
 * BUG-38515 __SM_SKIP_CHECKSCN_IN_STARTUP ���� ������Ƽ�� ���� �м��� ���� ����
 * Legacy Tx�� �����Ͽ� addLegacyTrans�� removeLegacyTrans �Լ����� legacy Tx�� 
 * �߰�/���� �� ������ trc �α׸� �����. 
 * ��, legacy Tx�� ���� ��� ���ɿ� ������ �� �� �����Ƿ� 
 * ���� ������Ƽ�� trc �α׸� ������ ���θ� ������ �� �ֵ��� �Ѵ�.
 * �� ���� 0�� ��� add/remove LegacyTrans�� ���õ� trc �α׸� ������ �ʴ´�.
 * �� ���� 1�� ��� add/remove LegacyTrans�� ���õ� trc �α׸� �����.
 * ----------------------------------------------------------------- */
    IDP_DEF(UInt, "__TRCLOG_LEGACY_TX_INFO",
            IDP_ATTR_SL_PFILE |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 0);

/* --------------------------------------------------------------------
 * When Recovery Fails, Turn On This Property and Skip REDO & UNDO
 * CAUTION : DATABASE WILL NOT BE CONSISTENT!!
 * Min = 0, Default = 0
 * BUG-32632 User Memory Tablesapce���� Max size�� �����ϴ� ���� Property�߰�
 *           User Memory Tablespace�� Max Size�� �����ϰ� ������ Ȯ��
 * ----------------------------------------------------------------- */
    IDP_DEF(UInt, "__EMERGENCY_IGNORE_MEM_TBS_MAXSIZE",
            IDP_ATTR_SL_PFILE |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 0);

    // For Calculation

    // ==================================================================
    // MM Multiplexing Properties
    // ==================================================================

    IDP_DEF(UInt, "MULTIPLEXING_THREAD_COUNT",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            1, 1024, IDL_MIN(idlVA::getProcessorCount(), 1024));

    IDP_DEF(UInt, "MULTIPLEXING_MAX_THREAD_COUNT",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            1, 1024, 1024);

#if defined(WRS_VXWORKS) || defined(VC_WINCE)
    IDP_DEF(UInt, "MULTIPLEXING_POLL_TIMEOUT",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            100000, 300000, 100000);
#else
    IDP_DEF(UInt, "MULTIPLEXING_POLL_TIMEOUT",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            1000, 1000000, 10000);
#endif

    IDP_DEF(UInt, "MULTIPLEXING_CHECK_INTERVAL",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            100000, 10000000, 200000);

    /* TASK-4324  Applying lessons learned from CPBS-CAESE to altibase
       service thread�� �ʱ� lifespan ��.
       service thread�� �������ڸ��� �����ϴ°��� �����ϱ� ����
       �� property�� �ξ���.
     */
    IDP_DEF(UInt, "SERVICE_THREAD_INITIAL_LIFESPAN",
            IDP_ATTR_SL_ALL      |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            30,ID_UINT_MAX , 6000);
    /* TASK-4324  Applying lessons learned from CPBS-CAESE to altibase
       idle �� thread�� ��� live�ϱ� ���� �ּ� assigned �� task����.
     */
    IDP_DEF(UInt, "MIN_TASK_COUNT_FOR_THREAD_LIVE",
            IDP_ATTR_SL_ALL      |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            1,1024 , 1);
    /* TASK-4324  Applying lessons learned from CPBS-CAESE to altibase
       a service thread�� busy degree�� �����Ҷ� ���Ǹ�,
       a service thread�� busy�϶� penalty�� ���ȴ�.
     */
    IDP_DEF(UInt, "BUSY_SERVICE_THREAD_PENALTY",
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            1,ID_UINT_MAX ,128);
    /* TASK-4324  Applying lessons learned from CPBS-CAESE to altibase
       max a idle thread ���� min a idle thread���� migration�Ҷ�
       task�� �����ϴ� idle thread�� task  ��ȭ����
       �� property���� Ŀ���� task migration�� ����Ѵ�.
     */
    IDP_DEF(UInt, "MIN_MIGRATION_TASK_RATE",
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            10,100000 ,50);

    /* TASK-4324  Applying lessons learned from CPBS-CAESE to altibase
       a idle thread�� �Ҵ�� ��� average task������
       a service thread�� ��� Task��������  ũ��,
       �� ������ NEW_SERVICE_CREATE_RATE �̻� ��������,
       �� ������ NEW_SERVICE_CREATE_RATE_GAP �̻� ����������
       �߰������� service thread��  �����ϵ��� �����Ѵ�.
     */
    IDP_DEF(UInt, "NEW_SERVICE_CREATE_RATE",
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            100,100000 , 200);

    IDP_DEF(UInt, "NEW_SERVICE_CREATE_RATE_GAP",
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            1,100000 ,2);

    IDP_DEF(UInt, "SERVICE_THREAD_EXIT_RATE",
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            100,100000 , 150);

    //fix BUG-23776, XA ROLLBACK�� XID�� ACTIVE�϶� ���ð���
    //QueryTime Out�� �ƴ϶�,Property�� �����ؾ� ��.
    IDP_DEF(UInt, "XA_ROLLBACK_TIMEOUT",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            1, UINT_MAX, 120);


    // ==================================================================
    // MM Queue Hash Table Sizes
    // ==================================================================

    IDP_DEF(UInt, "QUEUE_GLOBAL_HASHTABLE_SIZE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            50, ID_UINT_MAX, 50);

    IDP_DEF(UInt, "QUEUE_SESSION_HASHTABLE_SIZE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            10, ID_UINT_MAX, 10);

    //fix BUG-30949 A waiting time for enqueue event in transformed dedicated thread should not be infinite.
    IDP_DEF(ULong, "QUEUE_MAX_ENQ_WAIT_TIME",
                IDP_ATTR_SL_ALL |
             IDP_ATTR_SH_ALL |
                IDP_ATTR_IU_ANY |
                IDP_ATTR_MS_ANY |
                IDP_ATTR_LC_INTERNAL |
                IDP_ATTR_RD_WRITABLE |
                IDP_ATTR_ML_JUSTONE  |
                IDP_ATTR_CK_CHECK,
                10,ID_ULONG_MAX ,3000000);
    // ==================================================================
    // MM  PROJ-1436 SQL-Plan Cache
    // ==================================================================
    // default 64M
    IDP_DEF(ULong, "SQL_PLAN_CACHE_SIZE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, ID_ULONG_MAX,67108864);

    IDP_DEF(UInt, "SQL_PLAN_CACHE_BUCKET_CNT",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            5, 4096,127);

    IDP_DEF(UInt, "SQL_PLAN_CACHE_INIT_PCB_CNT",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0,1024,32);

    IDP_DEF(UInt, "SQL_PLAN_CACHE_INIT_PARENT_PCO_CNT",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0,1024,32);

    IDP_DEF(UInt, "SQL_PLAN_CACHE_INIT_CHILD_PCO_CNT",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0,1024,32);

   // fix BUG-29384,The upper bound of SQL_PLAN_CACHE_HOT_REGION_LRU_RATIO should be raised up to 100%
    IDP_DEF(UInt, "SQL_PLAN_CACHE_HOT_REGION_LRU_RATIO",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            10, 100,50);

    IDP_DEF(UInt, "SQL_PLAN_CACHE_PREPARED_EXECUTION_CONTEXT_CNT",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1024, 1);
    //fix BUG-31150, It needs to add  the property for frequency of  hot region LRU  list.
    IDP_DEF(UInt, "SQL_PLAN_CACHE_HOT_REGION_FREQUENCY",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            2,ID_UINT_MAX,3);

    /* BUG-35521 Add TryLatch in PlanCache. */
    /* BUG-35630 Change max value to UINT_MAX */
    IDP_DEF(UInt, "SQL_PLAN_CACHE_PARENT_PCO_XLATCH_TRY_CNT",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            1, ID_UINT_MAX, 100);

    /* BUG-36205 Plan Cache On/Off property for PSM */
    IDP_DEF(UInt, "SQL_PLAN_CACHE_USE_IN_PSM",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 1);

    /* PROJ-2109 : Remove the bottleneck of alloc/free stmts. */
    /* This property indicates the initially allocated ratio of number of StatementPageTables
       to MAX_CLIENT in mmcStatementManager. */
    // default = 10%
    IDP_DEF(UInt, "STMTPAGETABLE_PREALLOC_RATIO",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 100, 10);

    /* PROJ-2109 : Remove the bottleneck of alloc/free stmts. */
    /* maxinum count of free-list elements of the session mutexpool */
    IDP_DEF(UInt, "SESSION_MUTEXPOOL_FREE_LIST_MAXCNT",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            1, 1024, 64);

    /* PROJ-2109 : Remove the bottleneck of alloc/free stmts. */
    /* the number of initially initialized mutex in the free-list of the session mutexpool */
    IDP_DEF(UInt, "SESSION_MUTEXPOOL_FREE_LIST_INITCNT",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1024, 2);

    /* These properties decide the last parameter of iduMemPool::initilize */
    IDP_DEF(UInt, "MMT_SESSION_LIST_MEMPOOL_SIZE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            1, 512,
            IDP_LIMITED_CORE_COUNT(16,128));

    IDP_DEF(UInt, "MMC_MUTEXPOOL_MEMPOOL_SIZE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            1, 512,
            IDP_LIMITED_CORE_COUNT(8,64));

    IDP_DEF(UInt, "MMC_STMTPAGETABLE_MEMPOOL_SIZE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            1, 512,
            IDP_LIMITED_CORE_COUNT(4,12));

    /* PROJ-1438 Job Scheduler */
    IDP_DEF(UInt, "JOB_THREAD_COUNT",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 128, 0 );

    /* PROJ-1438 Job Scheduler */
    IDP_DEF(UInt, "JOB_THREAD_QUEUE_SIZE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            4, 1024, 64 );

    /* PROJ-1438 Job Scheduler */
    IDP_DEF(UInt, "JOB_SCHEDULER_ENABLE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 0 );

    // ==================================================================
    // QP Properties
    // ==================================================================

    IDP_DEF(UInt, "MAX_CLIENT",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            /* PROJ-2109 : Remove the bottleneck of alloc/free stmts. */
            // BUG-33092 should change of MAX_CLIENT property's min and max value
            1, 65535, 1000);

    IDP_DEF(UInt, "CM_DISCONN_DETECT_TIME",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            1, ID_UINT_MAX, 3);

    IDP_DEF(SInt, "DDL_LOCK_TIMEOUT",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            -1, 65535, 0);

    IDP_DEF(UInt, "AUTO_COMMIT",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 1);

    /* BUG-45295 non-autocommit session���� tx�� �̸� �������� �ʴ´�. */
    IDP_DEF(UInt, "TRANSACTION_START_MODE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 0);

    // BUG-19465 : CM_Buffer�� pending list�� ����
    IDP_DEF(UInt, "CM_BUFFER_MAX_PENDING_LIST",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            1, 512, 512 );

    // ==================================================================
    // RP Properties
    // ==================================================================

    // BUG-14325
    // PK UPDATE in Replicated table.
    IDP_DEF(UInt, "REPLICATION_UPDATE_PK",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 0);

    IDP_DEF(UInt, "REPLICATION_PORT_NO",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 65535, 0);

    IDP_DEF(UInt, "REPLICATION_IB_PORT_NO",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 65535, 0);

    IDP_DEF(UInt, "REPLICATION_IB_LATENCY",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 0);

    IDP_DEF(SInt, "REPLICATION_MAX_LOGFILE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 65535, 0);

    IDP_DEF(UInt, "REPLICATION_UPDATE_REPLACE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 0);

    IDP_DEF(UInt, "REPLICATION_INSERT_REPLACE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 0);

    IDP_DEF(UInt, "REPLICATION_CONNECT_TIMEOUT",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            1, ID_UINT_MAX, 10);

    IDP_DEF(UInt, "REPLICATION_RECEIVE_TIMEOUT",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            1, ID_UINT_MAX, 7200);

    IDP_DEF(UInt, "REPLICATION_SENDER_SLEEP_TIMEOUT",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, IDV_MAX_TIME_INTERVAL_SEC, 60);

    IDP_DEF(UInt, "REPLICATION_RECEIVER_XLOG_QUEUE_SIZE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, ID_UINT_MAX, 100);

    IDP_DEF(UInt, "REPLICATION_ACK_XLOG_COUNT",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, ID_UINT_MAX, 100);

    IDP_DEF(UInt, "REPLICATION_PROPAGATION",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 0, 0);

    IDP_DEF(UInt, "REPLICATION_HBT_DETECT_TIME",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, IDV_MAX_TIME_INTERVAL_SEC, 6);

    IDP_DEF(UInt, "REPLICATION_HBT_DETECT_HIGHWATER_MARK",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, ID_UINT_MAX, 5);

    IDP_DEF(ULong, "REPLICATION_SYNC_TUPLE_COUNT",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, ID_ULONG_MAX, 500000 );

    IDP_DEF(UInt, "REPLICATION_SYNC_LOCK_TIMEOUT",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, ID_UINT_MAX, 30);

    IDP_DEF(UInt, "REPLICATION_TIMESTAMP_RESOLUTION",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 1);

    //fix BUG-9894
    IDP_DEF(UInt, "REPLICATION_CONNECT_RECEIVE_TIMEOUT",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, ID_UINT_MAX, 60);
    //fix BUG-9343
    IDP_DEF(UInt, "REPLICATION_SENDER_AUTO_START",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 1);

    IDP_DEF( UInt, "REPLICATION_SENDER_START_AFTER_GIVING_UP",
             IDP_ATTR_SL_ALL |
             IDP_ATTR_SH_ALL |
             IDP_ATTR_IU_ANY |
             IDP_ATTR_MS_ANY |
             IDP_ATTR_LC_EXTERNAL |
             IDP_ATTR_RD_WRITABLE |
             IDP_ATTR_ML_JUSTONE  |
             IDP_ATTR_CK_CHECK,
             0, 1, 1 );

    //TASK-2359
    IDP_DEF(UInt, "REPLICATION_PERFORMANCE_TEST",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 2, 0); // 0: ENABLE ALL, 1: RECEIVER, 2:NETWORK

    IDP_DEF(UInt, "REPLICATION_PREFETCH_LOGFILE_COUNT",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1024, 3);

    IDP_DEF(UInt, "REPLICATION_SENDER_SLEEP_TIME",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, ID_UINT_MAX, 10000);

    IDP_DEF(UInt, "REPLICATION_KEEP_ALIVE_CNT",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, ID_UINT_MAX, 600);

    // deprecated
    IDP_DEF(UInt, "REPLICATION_SERVICE_WAIT_MAX_LIMIT",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, ID_UINT_MAX, 50000);

    /*PROJ-1670 replication Log Buffer*/
    IDP_DEF(UInt, "REPLICATION_LOG_BUFFER_SIZE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 4095, 0);
    /*PROJ-1608 Recovery From Replication*/
    IDP_DEF(UInt, "REPLICATION_RECOVERY_MAX_TIME",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, ID_UINT_MAX, ID_UINT_MAX);
    IDP_DEF(UInt, "REPLICATION_RECOVERY_MAX_LOGFILE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, ID_UINT_MAX, 0);

    IDP_DEF(UInt, "__REPLICATION_RECOVERY_REQUEST_TIMEOUT",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 10, 5);

    // PROJ-1442 Replication Online �� DDL ���
    IDP_DEF(UInt, "REPLICATION_DDL_ENABLE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 0);

    IDP_DEF( UInt, "REPLICATION_DDL_ENABLE_LEVEL",
             IDP_ATTR_SL_ALL |
             IDP_ATTR_SH_ALL |
             IDP_ATTR_IU_ANY |
             IDP_ATTR_MS_ANY |
             IDP_ATTR_LC_EXTERNAL |
             IDP_ATTR_RD_WRITABLE |
             IDP_ATTR_ML_JUSTONE  |
             IDP_ATTR_CK_CHECK,
             0, 2, 0 );

    // PROJ-1705 RP
    IDP_DEF(UInt, "REPLICATION_POOL_ELEMENT_SIZE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            128, 65536, 256);    // DEFAULT : 128 Byte, MAX : 64K

    // PROJ-1705 RP
    IDP_DEF(UInt, "REPLICATION_POOL_ELEMENT_COUNT",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            1, 1024, 10);       // DEFAULT : 10, MAX : 1024

    IDP_DEF(UInt, "REPLICATION_EAGER_PARALLEL_FACTOR",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            1, IDP_REPLICATION_MAX_EAGER_PARALLEL_FACTOR, 
            IDL_MIN(IDL_MAX((idlVA::getProcessorCount()/2),1),
                    IDP_REPLICATION_MAX_EAGER_PARALLEL_FACTOR));  // DEFAULT : CPU ����, MAX : 512

    IDP_DEF(UInt, "REPLICATION_COMMIT_WRITE_WAIT_MODE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 0);

    IDP_DEF(UInt, "REPLICATION_SERVER_FAILBACK_MAX_TIME",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, ID_UINT_MAX, ID_UINT_MAX);

    IDP_DEF(UInt, "__REPLICATION_FAILBACK_PK_QUEUE_TIMEOUT",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            1, ID_UINT_MAX, 30);

    IDP_DEF(UInt, "REPLICATION_FAILBACK_INCREMENTAL_SYNC",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 1);

    IDP_DEF(UInt, "REPLICATION_MAX_LISTEN",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            1, 512, 32);

    IDP_DEF(UInt, "REPLICATION_TRANSACTION_POOL_SIZE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, ID_UINT_MAX, 2 );

    IDP_DEF(UInt, "REPLICATION_STRICT_EAGER_MODE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 1 ); /*1 is strict eager mode, 0 is loose eager mode*/

    IDP_DEF(SInt, "REPLICATION_EAGER_MAX_YIELD_COUNT",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, ID_SINT_MAX, 20000 );

    /* BUG-36555 */
    IDP_DEF(UInt, "REPLICATION_BEFORE_IMAGE_LOG_ENABLE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 0 ); /* 0: disable, 1: enable */

    IDP_DEF(UInt, "REPLICATION_EAGER_RECEIVER_MAX_ERROR_COUNT",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, ID_UINT_MAX, 5 );

    IDP_DEF(SInt, "REPLICATION_SENDER_COMPRESS_XLOG",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 0 );

    IDP_DEF(SInt, "REPLICATION_SENDER_COMPRESS_XLOG_LEVEL",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            1, 1000, 1 );

    /* BUG-37482 */
    IDP_DEF(UInt, "REPLICATION_MAX_COUNT",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 10240, 32);

    /* PROJ-2261 */
    IDP_DEF( UInt, "REPLICATION_THREAD_CPU_AFFINITY",
             IDP_ATTR_SL_ALL |
             IDP_ATTR_SH_ALL |
             IDP_ATTR_IU_ANY |
             IDP_ATTR_MS_ANY |
             IDP_ATTR_LC_EXTERNAL |
             IDP_ATTR_RD_WRITABLE |
             IDP_ATTR_ML_JUSTONE  |
             IDP_ATTR_CK_CHECK,
             0, 3, 3 );

    /* PROJ-2336 */
    IDP_DEF(SInt, "REPLICATION_ALLOW_DUPLICATE_HOSTS",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 0 );

    /* BUG-38102 */
    IDP_DEF(SInt, "REPLICATION_SENDER_ENCRYPT_XLOG",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 0 );

    /* BUG-38716 */
    IDP_DEF( UInt, "REPLICATION_SENDER_SEND_TIMEOUT",
             IDP_ATTR_SL_ALL |
             IDP_ATTR_SH_ALL |
             IDP_ATTR_IU_ANY |
             IDP_ATTR_MS_ANY |
             IDP_ATTR_LC_EXTERNAL |
             IDP_ATTR_RD_WRITABLE |
             IDP_ATTR_ML_JUSTONE  |
             IDP_ATTR_CK_CHECK,
             0, ID_UINT_MAX, 7200 );

    IDP_DEF( ULong, "REPLICATION_GAPLESS_MAX_WAIT_TIME",
             IDP_ATTR_SL_ALL |
             IDP_ATTR_SH_ALL |
             IDP_ATTR_IU_ANY |
             IDP_ATTR_MS_ANY |
             IDP_ATTR_LC_EXTERNAL |
             IDP_ATTR_RD_WRITABLE |
             IDP_ATTR_ML_JUSTONE  |
             IDP_ATTR_CK_CHECK,
             0, ID_ULONG_MAX, 2000 );

    IDP_DEF( ULong, "REPLICATION_GAPLESS_ALLOW_TIME",
             IDP_ATTR_SL_ALL |
             IDP_ATTR_SH_ALL |
             IDP_ATTR_IU_ANY |
             IDP_ATTR_MS_ANY |
             IDP_ATTR_LC_EXTERNAL |
             IDP_ATTR_RD_WRITABLE |
             IDP_ATTR_ML_JUSTONE  |
             IDP_ATTR_CK_CHECK,
             0, ID_ULONG_MAX, 10000000 );

    IDP_DEF( UInt, "REPLICATION_RECEIVER_APPLIER_QUEUE_SIZE",
             IDP_ATTR_SL_ALL |
             IDP_ATTR_SH_ALL |
             IDP_ATTR_IU_ANY |
             IDP_ATTR_MS_ANY |
             IDP_ATTR_LC_EXTERNAL |
             IDP_ATTR_RD_WRITABLE |
             IDP_ATTR_ML_JUSTONE  |
             IDP_ATTR_CK_CHECK,
             2, ID_UINT_MAX, 20 );

    IDP_DEF( SInt, "REPLICATION_RECEIVER_APPLIER_ASSIGN_MODE",
             IDP_ATTR_SL_ALL |
             IDP_ATTR_SH_ALL |
             IDP_ATTR_IU_ANY |
             IDP_ATTR_MS_ANY |
             IDP_ATTR_LC_EXTERNAL |
             IDP_ATTR_RD_WRITABLE |
             IDP_ATTR_ML_JUSTONE  |
             IDP_ATTR_CK_CHECK,
             0, 1, 0 );

    IDP_DEF( UInt, "REPLICATION_FORCE_RECEIVER_PARALLEL_APPLY_COUNT",
             IDP_ATTR_SL_ALL |
             IDP_ATTR_SH_ALL |
             IDP_ATTR_IU_ANY |
             IDP_ATTR_MS_ANY |
             IDP_ATTR_LC_INTERNAL |
             IDP_ATTR_RD_WRITABLE |
             IDP_ATTR_ML_JUSTONE  |
             IDP_ATTR_CK_CHECK,
             0, ID_UINT_MAX, 0 );

    IDP_DEF( UInt, "REPLICATION_GROUPING_TRANSACTION_MAX_COUNT",
             IDP_ATTR_SL_ALL |
             IDP_ATTR_SH_ALL |
             IDP_ATTR_IU_ANY |
             IDP_ATTR_MS_ANY |
             IDP_ATTR_LC_EXTERNAL |
             IDP_ATTR_RD_WRITABLE |
             IDP_ATTR_ML_JUSTONE  |
             IDP_ATTR_CK_CHECK,
             1, 1000, 5 );

    IDP_DEF( UInt, "REPLICATION_GROUPING_AHEAD_READ_NEXT_LOG_FILE",
             IDP_ATTR_SL_ALL |
             IDP_ATTR_SH_ALL |
             IDP_ATTR_IU_ANY |
             IDP_ATTR_MS_ANY |
             IDP_ATTR_LC_EXTERNAL |
             IDP_ATTR_RD_WRITABLE |
             IDP_ATTR_ML_JUSTONE  |
             IDP_ATTR_CK_CHECK,
             1, ID_UINT_MAX, 2 );

    /* BUG-41246 */
    IDP_DEF( UInt, "REPLICATION_RECONNECT_MAX_COUNT",
             IDP_ATTR_SL_ALL |
             IDP_ATTR_SH_ALL |
             IDP_ATTR_IU_ANY |
             IDP_ATTR_MS_ANY |
             IDP_ATTR_LC_INTERNAL |
             IDP_ATTR_RD_WRITABLE |
             IDP_ATTR_ML_JUSTONE  |
             IDP_ATTR_CK_CHECK,
             0, ID_UINT_MAX, 0 );

    IDP_DEF( UInt, "REPLICATION_SYNC_APPLY_METHOD",
             IDP_ATTR_SL_ALL |
             IDP_ATTR_SH_ALL |
             IDP_ATTR_IU_ANY |
             IDP_ATTR_MS_ANY |
             IDP_ATTR_LC_EXTERNAL |
             IDP_ATTR_RD_WRITABLE |
             IDP_ATTR_ML_JUSTONE  |
             IDP_ATTR_CK_CHECK,
             0, 1, 0 );

    IDP_DEF( UInt, "REPLICATION_EAGER_KEEP_LOGFILE_COUNT",
             IDP_ATTR_SL_ALL |
             IDP_ATTR_SH_ALL |
             IDP_ATTR_IU_ANY |
             IDP_ATTR_MS_ANY |
             IDP_ATTR_LC_INTERNAL |
             IDP_ATTR_RD_WRITABLE |
             IDP_ATTR_ML_JUSTONE  |
             IDP_ATTR_CK_CHECK,
             0, 10, 3 );
   
    IDP_DEF( UInt, "REPLICATION_FORCE_SQL_APPLY_ENABLE",
             IDP_ATTR_SL_ALL |
             IDP_ATTR_SH_ALL |
             IDP_ATTR_IU_ANY |
             IDP_ATTR_MS_ANY |
             IDP_ATTR_LC_INTERNAL |
             IDP_ATTR_RD_WRITABLE |
             IDP_ATTR_ML_JUSTONE  |
             IDP_ATTR_CK_CHECK,
             0, 1, 0 );

    IDP_DEF( UInt, "REPLICATION_SQL_APPLY_ENABLE",
             IDP_ATTR_SL_ALL |
             IDP_ATTR_SH_ALL |
             IDP_ATTR_IU_ANY |
             IDP_ATTR_MS_ANY |
             IDP_ATTR_LC_EXTERNAL |
             IDP_ATTR_RD_WRITABLE |
             IDP_ATTR_ML_JUSTONE  |
             IDP_ATTR_CK_CHECK,
             0, 1, 0 );

    IDP_DEF( UInt, "__REPLICATION_SET_RESTARTSN",
             IDP_ATTR_SL_ALL |
             IDP_ATTR_SH_ALL |
             IDP_ATTR_IU_ANY |
             IDP_ATTR_MS_ANY |
             IDP_ATTR_LC_INTERNAL |
             IDP_ATTR_RD_WRITABLE |
             IDP_ATTR_ML_JUSTONE  |
             IDP_ATTR_CK_CHECK,
             0, 1, 0 );

    IDP_DEF( UInt, "REPLICATION_SENDER_RETRY_COUNT",
             IDP_ATTR_SL_ALL |
             IDP_ATTR_SH_ALL |
             IDP_ATTR_IU_ANY |
             IDP_ATTR_MS_ANY |
             IDP_ATTR_LC_INTERNAL |
             IDP_ATTR_RD_WRITABLE |
             IDP_ATTR_ML_JUSTONE  |
             IDP_ATTR_CK_CHECK,
             0, ID_UINT_MAX, 5 );

    IDP_DEF( UInt, "REPLICATION_ALLOW_QUEUE",
             IDP_ATTR_SL_ALL |
             IDP_ATTR_SH_ALL |
             IDP_ATTR_IU_ANY |
             IDP_ATTR_MS_ANY |
             IDP_ATTR_LC_INTERNAL |
             IDP_ATTR_RD_WRITABLE |
             IDP_ATTR_ML_JUSTONE  |
             IDP_ATTR_CK_CHECK,
             0, 1, 0 );

    IDP_DEF( UInt, "REPLICATION_RECEIVER_APPLIER_YIELD_COUNT",
             IDP_ATTR_SL_ALL |
             IDP_ATTR_SH_ALL |
             IDP_ATTR_IU_ANY |
             IDP_ATTR_MS_ANY |
             IDP_ATTR_LC_EXTERNAL |
             IDP_ATTR_RD_WRITABLE |
             IDP_ATTR_ML_JUSTONE  |
             IDP_ATTR_CK_CHECK,
             0, UINT_MAX, 20000 );

    // PROJ-1723
    IDP_DEF(UInt, "DDL_SUPPLEMENTAL_LOG_ENABLE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 0);

    /* PROJ-2677 DDL ���� ���*/
    IDP_DEF( UInt, "REPLICATION_DDL_SYNC",
             IDP_ATTR_SL_ALL |
             IDP_ATTR_SH_ALL |
             IDP_ATTR_IU_ANY |
             IDP_ATTR_MS_ANY |
             IDP_ATTR_LC_EXTERNAL |
             IDP_ATTR_RD_WRITABLE |
             IDP_ATTR_ML_JUSTONE  |
             IDP_ATTR_CK_CHECK,
             0, 1, 0 );

    IDP_DEF( UInt, "REPLICATION_DDL_SYNC_TIMEOUT",
             IDP_ATTR_SL_ALL |
             IDP_ATTR_SH_ALL |
             IDP_ATTR_IU_ANY |
             IDP_ATTR_MS_ANY |
             IDP_ATTR_LC_EXTERNAL |
             IDP_ATTR_RD_WRITABLE |
             IDP_ATTR_ML_JUSTONE  |
             IDP_ATTR_CK_CHECK,
             0, ID_UINT_MAX, 7200 );

    IDP_DEF(ULong, "REPLICATION_GAP_UNIT",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            1, ID_ULONG_MAX, 1024 * 1024 );

    IDP_DEF(UInt, "REPLICATION_CHECK_SRID_IN_GEOMETRY_ENABLE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 1 );

    IDP_DEF(UInt, "XA_HEURISTIC_COMPLETE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 2, 0);

    IDP_DEF(UInt, "XA_INDOUBT_TX_TIMEOUT",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, ID_UINT_MAX, 60);

    IDP_DEF(UInt, "OPTIMIZER_MODE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 0     /* 0: cost, 1: rule*/);

    IDP_DEF(UInt, "PROTOCOL_DUMP", // 0 : inactive, 1 : active
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 0);

    /* PROJ-2725 Consistent Replication For Shard Cluster System */
    /* XLOGFILE�� ����� ��ġ */
    IDP_DEF(String, "XLOGFILE_DIR",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_SK_PATH     |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_MULTIPLE |
            IDP_ATTR_DF_DROP_DEFAULT |
            IDP_ATTR_CK_CHECK,
            0, IDP_MAX_PROP_STRING_LEN, (SChar *)"");

    /* �̸� ������ XLOGFILE�� �� */
    IDP_DEF(UInt, "XLOGFILE_PREPARE_COUNT",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            1, ID_UINT_MAX, 5);

    IDP_DEF(ULong, "XLOGFILE_SIZE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_AL_SET_VALUE( sLogFileAlignSize ) |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            64 * 1024, ID_ULONG_MAX, 10 * 1024 * 1024);

    IDP_DEF(UInt, "XLOGFILE_REMOVE_INTERVAL",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            3, IDV_MAX_TIME_INTERVAL_SEC, 600);

    IDP_DEF(UInt, "XLOGFILE_REMOVE_INTERVAL_BY_FILE_CREATE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            1, ID_UINT_MAX, 10);

    // BUG-44742
    // NORMALFORM_MAXIMUM�� �⺻���� 128���� 2048�� �����մϴ�.
    IDP_DEF(UInt, "NORMALFORM_MAXIMUM",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, ID_UINT_MAX, 2048);

    IDP_DEF(UInt, "EXEC_DDL_DISABLE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 0     /* 0: DDL can be executed.*/ );

    IDP_DEF(UInt, "SELECT_HEADER_DISPLAY",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 1     );

    IDP_DEF(UInt, "LOGIN_TIMEOUT",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, ID_UINT_MAX, 0);

    IDP_DEF(UInt, "IDLE_TIMEOUT",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, ID_UINT_MAX, 0     /* default 0 : forever running */);

    IDP_DEF(UInt, "QUERY_TIMEOUT",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, ID_UINT_MAX, 600);

    /* BUG-32885 Timeout for DDL must be distinct to query_timeout or utrans_timeout */
    IDP_DEF(UInt, "DDL_TIMEOUT",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, ID_UINT_MAX, 0);

    IDP_DEF(UInt, "FETCH_TIMEOUT",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, ID_UINT_MAX, 60);

    IDP_DEF(UInt, "UTRANS_TIMEOUT",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, ID_UINT_MAX, 3600);
    
    // PROJ-1665 : session property�μ� parallel_dml_mode
    IDP_DEF(UInt, "PARALLEL_DML_MODE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, ID_UINT_MAX, 0);

    IDP_DEF(UInt, "UPDATE_IN_PLACE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 0     /* default 0 : forever running */);

    IDP_DEF(UInt, "MEMORY_COMPACT_TIME",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, ID_UINT_MAX, 60    /* default :check for every 1 min */);

    IDP_DEF(UInt, "ADMIN_MODE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_USER|
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 0);

    /* PROJ-2563 */
    IDP_DEF( UInt, "__REPLICATION_USE_V6_PROTOCOL",
             IDP_ATTR_SL_ALL |
             IDP_ATTR_SH_ALL |
             IDP_ATTR_IU_ANY |
             IDP_ATTR_MS_ANY |
             IDP_ATTR_LC_INTERNAL |
             IDP_ATTR_RD_READONLY |
             IDP_ATTR_ML_JUSTONE  |
             IDP_ATTR_CK_CHECK,
             0, 1, 0 );

    IDP_DEF(UInt, "_STORED_PROC_MODE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, ID_UINT_MAX, 0);

    IDP_DEF(UInt, "__SHOW_ERROR_STACK",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_USER|
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 0 );

    IDP_DEF(UInt, "TIMER_RUNNING_LEVEL",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            1, 3, IDV_TIMER_DEFAULT_LEVEL );

    IDP_DEF(UInt, "TIMER_THREAD_RESOLUTION",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            50, 10000000, 1000 );

    /*
     * TASK-2356 [��ǰ�����м�] DRDB�� DML ���� �ľ�
     * Altibase Wait Interface ������� ���� ���� ����
     */
    IDP_DEF(UInt, "TIMED_STATISTICS",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 0 );

    /* BUG-38946 display name */
    IDP_DEF(UInt, "COMPATIBLE_DISPLAY_NAME",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 0 );

    /* ------------------------------------------------------------------
     *   SERVER
     * --------------------------------------------------------------*/
    // MSG LOG Property

    IDP_DEF(UInt, "ALL_MSGLOG_FLUSH",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 1);


    IDP_DEF(String, "SERVER_MSGLOG_FILE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_SK_PATH     |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, IDP_MAX_PROP_STRING_LEN, IDE_BOOT_LOG_FILE);

    IDP_DEF(String, "SERVER_MSGLOG_DIR",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_SK_PATH     |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, IDP_MAX_PROP_STRING_LEN, (SChar *)"?"IDL_FILE_SEPARATORS"trc");

    IDP_DEF(UInt, "SERVER_MSGLOG_SIZE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, ID_UINT_MAX, (10 * 1024 * 1024));

    IDP_DEF(UInt, "SERVER_MSGLOG_COUNT",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, ID_UINT_MAX, 10);

    IDP_DEF(UInt, "SERVER_MSGLOG_FLAG",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, ID_UINT_MAX, 7);

    /* ------------------------------------------------------------------
     *   QP
     * --------------------------------------------------------------*/
    IDP_DEF(String, "QP_MSGLOG_FILE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_SK_PATH     |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, IDP_MAX_PROP_STRING_LEN, (SChar *)PRODUCT_PREFIX"altibase_qp.log");

    IDP_DEF(UInt, "QP_MSGLOG_SIZE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, ID_UINT_MAX, (10 * 1024 * 1024));

    IDP_DEF(UInt, "QP_MSGLOG_COUNT",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, ID_UINT_MAX, 10);

    // BUG-24354  qp_msglog_flag=2, PREPARE_STMT_MEMORY_MAXIMUM = 200M �� Property �� Default �� ����
    IDP_DEF(UInt, "QP_MSGLOG_FLAG",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, ID_UINT_MAX, 2);

    /* ------------------------------------------------------------------
     *  SD  BUG-46138 
     * --------------------------------------------------------------*/
    IDP_DEF(String, "SD_MSGLOG_FILE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_SK_PATH     |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, IDP_MAX_PROP_STRING_LEN, (SChar *)PRODUCT_PREFIX"altibase_sd.log");

    IDP_DEF(UInt, "SD_MSGLOG_SIZE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, ID_UINT_MAX, (10 * 1024 * 1024));

    IDP_DEF(UInt, "SD_MSGLOG_COUNT",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, ID_UINT_MAX, 10);

    IDP_DEF(UInt, "SD_MSGLOG_FLAG",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, ID_UINT_MAX, 65537);

    /* ------------------------------------------------------------------
     *   SM
     * --------------------------------------------------------------*/
    IDP_DEF(String, "SM_MSGLOG_FILE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_SK_PATH     |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, IDP_MAX_PROP_STRING_LEN, (SChar *)PRODUCT_PREFIX"altibase_sm.log");

    IDP_DEF(UInt, "SM_MSGLOG_SIZE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, ID_UINT_MAX, (10 * 1024 * 1024));

    IDP_DEF(UInt, "SM_MSGLOG_COUNT",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, ID_UINT_MAX, 10);

    IDP_DEF(UInt, "SM_MSGLOG_FLAG",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, ID_UINT_MAX, 2147483647);

    IDP_DEF(UInt, "SM_XLATCH_USE_SIGNAL",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 0);

    /* ------------------------------------------------
     *  MUTEX/LATCH Choolse
     * ----------------------------------------------*/

    /* BUG-33203 [sm_transaction] change the default value of 
     * __DBMS_STAT_METHOD, MUTEX_TYPE, and LATCH_TYPE properties */
    IDP_DEF(UInt, "LATCH_TYPE", // 0 : posix, 1 : native
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 1);

    IDP_DEF(UInt, "LATCH_MINSLEEP", // in microseconds
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1000, 50);

    IDP_DEF(UInt, "LATCH_MAXSLEEP", // in microseconds
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 99999, 1000);

    IDP_DEF(UInt, "MUTEX_SLEEPTYPE", // 0 : sleep, 1 : thryield
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            IDU_MUTEX_SLEEP,
            IDU_MUTEX_YIELD,
            IDU_MUTEX_SLEEP);

    IDP_DEF(UInt, "MUTEX_TYPE", // 0 : posix, 1 : native
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 1);

    //BUG-17371
    // 0 = check disable, 1= check enable
    IDP_DEF(UInt, "CHECK_MUTEX_DURATION_TIME_ENABLE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 0);

/* BUG-46911
 * - Linux, 24 �ھ�� Ŭ ��� ���� ī��Ʈ ���� ũ�� 
 * select ���� ���� �߻�. �׽�Ʈ ����� ���� 10,000���� ����  
 */  
#if defined(ALTI_CFG_OS_LINUX)
    sDefaultMutexSpinCnt = ( (idlVA::getProcessorCount() > 24) ?
                             10000 : (idlVA::getProcessorCount() - 1)*10000 );
#else
    sDefaultMutexSpinCnt = (idlVA::getProcessorCount() - 1)*10000;
#endif

    IDP_DEF(UInt, "LATCH_SPIN_COUNT", // 0 : posix, 1 : native
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            1, ID_UINT_MAX, 
            (idlVA::getProcessorCount() > 1) ? sDefaultMutexSpinCnt : 1);

    IDP_DEF(UInt, "MUTEX_SPIN_COUNT", // 0 : posix, 1 : native
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            1, ID_UINT_MAX, 
            (idlVA::getProcessorCount() > 1) ? sDefaultMutexSpinCnt : 1);

    IDP_DEF(UInt, "NATIVE_MUTEX_SPIN_COUNT", // for IDU_MUTEX_KIND_NATIVE2
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            1,
            ID_UINT_MAX,
            (idlVA::getProcessorCount() > 1) ? sDefaultMutexSpinCnt : 1);

    // BUG-28856 Logging �������� �� Native3�߰�
    // ���� spin count�� ���� native mutex���� Logging���� ���
    // BUGBUG Default���� ���� �� �׽�Ʈ�� �ʿ���
    // for IDU_MUTEX_KIND_NATIVE_FOR_LOGGING
    IDP_DEF(UInt, "NATIVE_MUTEX_SPIN_COUNT_FOR_LOGGING",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            1,
            ID_UINT_MAX,
            (idlVA::getProcessorCount() > 1) ?
            (idlVA::getProcessorCount() - 1)*100000 : 1);

    /* BUG-35392 */
    // BUG-28856 logging ��������
    // Logging �� ����ϴ� log alloc Mutex�� ������ ����
    IDP_DEF(UInt, "LOG_ALLOC_MUTEX_TYPE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 3, 3);

    /* BUG-35392 */
    // BUG-28856 logging ��������
    // Logging �� ����ϴ� log alloc Mutex�� ������ ����
    IDP_DEF(UInt, "FAST_UNLOCK_LOG_ALLOC_MUTEX",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 0);

    IDP_DEF(UInt, "__LOG_READ_METHOD_TYPE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 0);

    IDP_DEF(UInt, "__LOG_COMPRESSION_ACCELERATION",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            1, 1000, 1);

    IDP_DEF(String, "DEFAULT_DATE_FORMAT",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_SK_ASCII    |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, IDP_MAX_PROP_STRING_LEN, (SChar *)"DD-MON-RRRR");

    IDP_DEF ( UInt, "__MUTEX_POOL_MAX_SIZE",
              IDP_ATTR_SL_ALL |
              IDP_ATTR_SH_ALL |
              IDP_ATTR_IU_ANY |
              IDP_ATTR_MS_ANY |
              IDP_ATTR_LC_INTERNAL |
              IDP_ATTR_RD_WRITABLE |
              IDP_ATTR_ML_JUSTONE  |
              IDP_ATTR_CK_CHECK,
              0, ID_UINT_MAX, (256*1024) );

#if defined(ALTIBASE_PRODUCT_XDB)
    /* TASK-4690
     * �޸� �ε��� INode�� �ִ� slot ����.
     * �� ���� *2 �� ���� LNode�� �ִ� slot ������ �ȴ�. */
    IDP_DEF(UInt, "__MEM_BTREE_MAX_SLOT_COUNT",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            2, 1024, 128);
#endif

#if defined(ALTIBASE_PRODUCT_HDB)
    /* PROJ-2433
     * MEMORY BTREE INDEX NODE SIZE */
    IDP_DEF(UInt, "__MEM_BTREE_NODE_SIZE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            1024,
            IDP_SM_PAGE_SIZE,
            4096);

    /* PROJ-2433
     * MEMORY BTREE DIRECT KEY INDEX ����, default max key size */
    IDP_DEF(UInt, "__MEM_BTREE_DEFAULT_MAX_KEY_SIZE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            8,
            IDP_SM_PAGE_SIZE / 3,
            8);

    /* PROJ-2433
     *  Force makes all memory btree index to direct key index when column created. (if possible) */
    IDP_DEF(SInt, "__FORCE_INDEX_DIRECTKEY",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0,
            1,
            0);
#endif

    /* BUG-41121
     * Force to make Persistent Index, When new memory index is created.
     *
     * BUG-41541 Disable Memory Persistent Index and Change Hidden Property
     * 0�� ��� : persistent index �̻��(�⺻)
     * 1�� ��� : persistent�� ���� �� index�� persistent�� ���
     * 2�� ��� : ��� index�� persistent�� ��� */
    
    IDP_DEF(SInt, "__FORCE_INDEX_PERSISTENCE_MODE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0,
            2,
            0);
#if 0
    /* TASK-4690
     * �ε��� cardinality ��� ����ȭ */
    IDP_DEF(UInt, "__INDEX_STAT_PARALLEL_FACTOR",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            1, 512, IDL_MIN(idlVA::getProcessorCount() * 2, 512));
#endif
    /* TASK-4690
     * 0�̸� iduOIDMemory �� ����ϰ�,
     * 1�̸� iduMemPool �� ����Ѵ�. */
    IDP_DEF(UInt, "__TX_OIDLIST_MEMPOOL_TYPE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 1);

    /* ------------------------------------------------------------------
     *   RP
     * --------------------------------------------------------------*/
    IDP_DEF(String, "RP_MSGLOG_FILE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_SK_PATH     |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, IDP_MAX_PROP_STRING_LEN, (SChar *)PRODUCT_PREFIX"altibase_rp.log");

    IDP_DEF(String, "RP_CONFLICT_MSGLOG_FILE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_SK_PATH     |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, IDP_MAX_PROP_STRING_LEN, (SChar *)PRODUCT_PREFIX"altibase_rp_conflict.log");

    IDP_DEF(UInt, "RP_CONFLICT_MSGLOG_ENABLE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 1);

    IDP_DEF(String, "RP_CONFLICT_MSGLOG_DIR",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_SK_PATH     |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, IDP_MAX_PROP_STRING_LEN, (SChar *)"?"IDL_FILE_SEPARATORS"trc");

    // Update Transaction�� �� �� �̻��϶� Group Commit�� ���۽�ų ������
    IDP_DEF(UInt, "LFG_GROUP_COMMIT_UPDATE_TX_COUNT",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, ID_UINT_MAX, 80);

    // �⺻������ 1ms���� �ѹ��� Disk I/O�� ����
#if defined(WRS_VXWORKS) || defined(VC_WINCE)
    IDP_DEF(UInt, "LFG_GROUP_COMMIT_INTERVAL_USEC",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, ID_UINT_MAX, 100000);
#else
    IDP_DEF(UInt, "LFG_GROUP_COMMIT_INTERVAL_USEC",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, ID_UINT_MAX, 1000);
#endif

    // �⺻������ 100us���� �ѹ��� ����� �αװ� �̹� Sync�Ǿ����� üũ
#if defined(WRS_VXWORKS) || defined(VC_WINCE)
    IDP_DEF(UInt, "LFG_GROUP_COMMIT_RETRY_USEC",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, IDV_MAX_TIME_INTERVAL_USEC, 100000);
#else
    IDP_DEF(UInt, "LFG_GROUP_COMMIT_RETRY_USEC",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, IDV_MAX_TIME_INTERVAL_USEC, 100);
#endif

    // Update Transaction�� �� �� �̻��϶� Group Commit�� ���۽�ų ������
    IDP_DEF(UInt, "GROUP_COMMIT_UPDATE_TX_COUNT",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, ID_UINT_MAX, 80);

    // �⺻������ 1ms���� �ѹ��� Disk I/O�� ����
#if defined(WRS_VXWORKS) || defined(VC_WINCE)
    IDP_DEF(UInt, "GROUP_COMMIT_INTERVAL_USEC",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, ID_UINT_MAX, 100000);
#else
    IDP_DEF(UInt, "GROUP_COMMIT_INTERVAL_USEC",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, ID_UINT_MAX, 1000);
#endif

    // �⺻������ 100us���� �ѹ��� ����� �αװ� �̹� Sync�Ǿ����� üũ
#if defined(WRS_VXWORKS) || defined(VC_WINCE)
    IDP_DEF(UInt, "GROUP_COMMIT_RETRY_USEC",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, IDV_MAX_TIME_INTERVAL_USEC, 100000);
#else
    IDP_DEF(UInt, "GROUP_COMMIT_RETRY_USEC",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, IDV_MAX_TIME_INTERVAL_USEC, 100);
#endif

    IDP_DEF(UInt, "RP_MSGLOG_SIZE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, ID_UINT_MAX, (10 * 1024 * 1024));

    IDP_DEF(UInt, "RP_MSGLOG_COUNT",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, ID_UINT_MAX, 10);

    IDP_DEF(UInt, "RP_MSGLOG_FLAG",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, ID_UINT_MAX, 0);

    IDP_DEF(UInt, "RP_CONFLICT_MSGLOG_SIZE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, ID_UINT_MAX, (10 * 1024 * 1024));
    
    IDP_DEF(UInt, "RP_CONFLICT_MSGLOG_COUNT",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, ID_UINT_MAX, 10);
    
    IDP_DEF(UInt, "RP_CONFLICT_MSGLOG_FLAG",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, ID_UINT_MAX, 6);
                
    IDP_DEF( UInt, "REPLICATION_META_ITEM_COUNT_DIFF_ENABLE",
             IDP_ATTR_SL_ALL |
             IDP_ATTR_SH_ALL |
             IDP_ATTR_IU_ANY |
             IDP_ATTR_MS_ANY |
             IDP_ATTR_LC_EXTERNAL |
             IDP_ATTR_RD_WRITABLE |
             IDP_ATTR_ML_JUSTONE  |
             IDP_ATTR_CK_CHECK,
             0, 1, 0 );

    // ==================================================================
    // iduMemory
    // ==================================================================

    // BUG-24354  qp_msglog_flag=2, PREPARE_STMT_MEMORY_MAXIMUM = 200M �� Property �� Default �� ����
    IDP_DEF(ULong, "PREPARE_STMT_MEMORY_MAXIMUM",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            (1024*1024),      // min : 1M
            ID_ULONG_MAX,     // max
            (200*1024*1024)); // default : 200M

    IDP_DEF(ULong, "EXECUTE_STMT_MEMORY_MAXIMUM",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            (1024*1024),       // min : 1M
            ID_ULONG_MAX,      // max
            ID_ULONG(2 * 1024 * 1024 * 1024));      // default, 2G

    // ==================================================================
    // Preallocate Memory
    // ==================================================================
    IDP_DEF(ULong, "PREALLOC_MEMORY",
            IDP_ATTR_SL_ALL | IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, ID_ULONG_MAX, 0);

    /* !!!!!!!!!! REGISTRATION AREA END !!!!!!!!!! */

    // ==================================================================
    // PR-14783 SYSTEM THREAD CONTROL
    // 0 : OFF, 1 : ON
    // ==================================================================

    IDP_DEF(UInt, "MEM_DELETE_THREAD",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 1);

    IDP_DEF(UInt, "MEM_GC_THREAD",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 1);

    IDP_DEF(UInt, "BUFFER_FLUSH_THREAD",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 1);

    IDP_DEF(UInt, "ARCHIVE_THREAD",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 1);

    IDP_DEF(UInt, "CHECKPOINT_THREAD",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 1);

    IDP_DEF(UInt, "LOG_FLUSH_THREAD",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 1);

    IDP_DEF(UInt, "LOG_PREPARE_THREAD",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 1);

    IDP_DEF(UInt, "LOG_PREREAD_THREAD",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 1);

#ifdef ALTIBASE_ENABLE_SMARTSSD
    IDP_DEF(UInt, "SMART_SSD_LOG_RUN_GC_ENABLE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 0);

    IDP_DEF(String, "SMART_SSD_LOG_DEVICE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, IDP_MAX_PROP_STRING_LEN, (SChar *)"");

    IDP_DEF(UInt, "SMART_SSD_GC_TIME_MILLISEC",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            1, 268435455, 10);
#endif /* ALTIBASE_ENABLE_SMARTSSD */

    // To verify CASE-6829
    IDP_DEF(UInt, "__SM_CHECKSUM_DISABLE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 0);

    // To verify CASE-6829
    IDP_DEF(UInt, "__SM_AGER_DISABLE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 0);

    // To verify CASE-6829
    IDP_DEF(UInt, "__SM_CHECK_DISK_INDEX_INTEGRITY",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 3, 0);

    // To BUG-27122
    IDP_DEF(UInt, "__SM_VERIFY_DISK_INDEX_COUNT",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 16, 0);

    // Index Name String MaxSize
    IDP_DEF(String, "__SM_VERIFY_DISK_INDEX_NAME",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_MULTIPLE |
            IDP_ATTR_CK_CHECK,
            0, 47, (SChar *)"");

    /* ------------------------------------------------
     *  PROJ-2059: DB Upgrade Function
     * ----------------------------------------------*/

    // DataPort File�� ������ DIRECTORY
    IDP_DEF(String, "DATAPORT_FILE_DIRECTORY",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_SK_PATH     |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, IDP_MAX_PROP_STRING_LEN, (SChar *)"?"IDL_FILE_SEPARATORS"dbs");

    // DataPort File�� Block Size
    IDP_DEF(UInt, "__DATAPORT_FILE_BLOCK_SIZE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            8192, 1024*1024*1024, 2*1024*1024 );

    // DataPort��� ��� �� Direct IO ��� ����
    IDP_DEF(UInt, "__DATAPORT_DIRECT_IO_ENABLE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0,1,0 );

    //Export�� Column�� Chain(Block�� ��ħ)���θ� �Ǵ��ϴ� ���ذ�
    IDP_DEF(UInt, "__EXPORT_COLUMN_CHAINING_THRESHOLD",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            128, 8192, 128 );

    //Import�� Commit����
    IDP_DEF(UInt, "DATAPORT_IMPORT_COMMIT_UNIT",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            1,ID_UINT_MAX,10 );

    //Import�� Statement����
    IDP_DEF(UInt, "DATAPORT_IMPORT_STATEMENT_UNIT",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            1,ID_UINT_MAX,50000 );


    //Import�� Direct-path Insert ���ۿ���
    IDP_DEF(UInt, "__IMPORT_DIRECT_PATH_ENABLE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0,1,1 );

    //Import�� Validation ���� ����
    IDP_DEF(UInt, "__IMPORT_VALIDATION_ENABLE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0,1,1 );

    // Import�� SourceTable�� Partition������ �����Ҷ� Filtering�� ����
    // ���ִ� ��� ���� ����
    IDP_DEF(UInt, "__IMPORT_PARTITION_MATCH_ENABLE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0,1,1 );

    /* ------------------------------------------------
     *  PROJ-2469: Optimize View Materialization
     * ----------------------------------------------*/
    IDP_DEF(UInt, "__OPTIMIZER_VIEW_TARGET_ENABLE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0,1,1 );

    /* ------------------------------------------------
     *  PROJ-1598: Query Profile
     * ----------------------------------------------*/
    IDP_DEF(UInt, "QUERY_PROF_FLAG",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, ID_UINT_MAX, 0);

    IDP_DEF(UInt, "QUERY_PROF_BUF_SIZE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            32 * 1024, ID_UINT_MAX, 1024 * 1024);

    IDP_DEF(UInt, "QUERY_PROF_BUF_FLUSH_SIZE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            512, ID_UINT_MAX, 32 * 1024);

    IDP_DEF(UInt, "QUERY_PROF_BUF_FULL_SKIP",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 1); // default skip!

    IDP_DEF(UInt, "QUERY_PROF_FILE_SIZE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, ID_UINT_MAX , 100 * 1024 * 1024); // default unlimited(0) : or each file limit up to 4G

    /* BUG-36806 */
    IDP_DEF(String, "QUERY_PROF_LOG_DIR",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_SK_PATH     |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE |
            IDP_ATTR_CK_CHECK,
            0, IDP_MAX_PROP_STRING_LEN, (SChar *)"?"IDL_FILE_SEPARATORS"trc");

    IDP_DEF(String, "ACCESS_LIST",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_SK_ASCII    |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_MULTIPLE |
            IDP_ATTR_CK_CHECK,
            0, IDP_MAX_PROP_STRING_LEN, (SChar *)"");

    /* ---------------------------------------------------------------
     * PROJ-2223 Altibase Auditing 
     * -------------------------------------------------------------- */
    /* BUG-36807 */
    IDP_DEF(String, "AUDIT_LOG_DIR",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_SK_PATH     |    
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE|
            IDP_ATTR_CK_CHECK,
            0, IDP_MAX_PROP_STRING_LEN, (SChar *)"?"IDL_FILE_SEPARATORS"trc");

    IDP_DEF(UInt, "AUDIT_BUFFER_SIZE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            32 * 1024, ID_UINT_MAX, 1024 * 1024);

    IDP_DEF(UInt, "AUDIT_BUFFER_FLUSH_SIZE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            512, ID_UINT_MAX, 32 * 1024);

    IDP_DEF(UInt, "AUDIT_BUFFER_FULL_SKIP",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 1); // default skip!

    IDP_DEF(UInt, "AUDIT_FILE_SIZE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, ID_UINT_MAX , 100 * 1024 * 1024); 

    /* BUG-39760 Enable AltiAudit to write log into syslog */
    /*
       0: use private log file. as it was before ...
       1: use syslog 
       2: use syslog & facility:local0   
       3: use syslog & facility:local1   
       4: use syslog & facility:local2   
       5: use syslog & facility:local3   
       6: use syslog & facility:local4   
       7: use syslog & facility:local5   
       8: use syslog & facility:local6   
       9: use syslog & facility:local7   
    */
    IDP_DEF(UInt, "AUDIT_OUTPUT_METHOD",
            IDP_ATTR_SL_ALL      |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, (IDV_AUDIT_OUTPUT_MAX - 1), 0);
 
    IDP_DEF(String, "AUDIT_TAG_NAME_IN_SYSLOG",
            IDP_ATTR_SL_ALL      |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_SK_ASCII    |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, IDP_MAX_PROP_STRING_LEN, (SChar *)"AUDIT");

    //BUG-21122 :
    IDP_DEF(UInt, "AUTO_REMOTE_EXEC",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 0); // 0: DISABLE 1:ENABLE

    // BUG-20129
    IDP_DEF(ULong, "__HEAP_MEM_MAX_SIZE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0,                       // min
            ID_ULONG_MAX,            // max
            ID_ULONG_MAX);           // default

    // To fix BUG-21134
    // min : 0, max : 3, default : 0
    // 0 : no logging
    // 1 : select logging
    // 2 : select + dml logging
    // 3:  all query logging
    IDP_DEF(UInt, "__QUERY_LOGGING_LEVEL",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 3, 0);

    /* BUG-35155 Partial CNF
     * Normalize predicate partialy. (Normal form + NNF filter)
     * It may decrease output records in SCAN node
     *  and decrease intermediate tuple count. (vs NNF) */
    IDP_DEF(SInt, "OPTIMIZER_PARTIAL_NORMALIZE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 0);

    // fix BUG-36522
    IDP_DEF(UInt, "__PSM_SHOW_ERROR_STACK",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 2, 0);

    // fix BUG-36793
    IDP_DEF(UInt, "__BIND_PARAM_DEFAULT_PRECISION",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            1, 32000, 32000);

    /* PROJ-2264 Dictionary table
     *  Force makes all columns to compression column when column created. (if possible)
     *  HIDDEN PROPERTY */
    IDP_DEF(SInt, "__FORCE_COMPRESSION_COLUMN",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 0);

    /*
     * BUG-21487     Mutex Leak List����� propertyȭ �ؾ��մϴ�.
     */

    IDP_DEF(UInt, "SHOW_MUTEX_LEAK_LIST",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 1); //default�� 1�μ� ����ϴ°���.

    /* PROJ-1864  Partial Write Problem�� ���� ���� �ذ�.
     * Recovery���� Corrupt page ó�� ��å
     * 
     * BUG-45598: ��� Corrupt page ó�� ��å�� �����Ѵ�. 
     *
     * BUG-46182: default ���� 3���� 2�� ����  
     *
     * 0 - corrupt page�� �߰��ϸ� �����Ѵ�.
     *     Group Hdr page�� corrupt �� ��쿡�� ���� ���� �Ѵ�.
     * 1 - corrupt page�� �߰��ϸ� ������ �����Ѵ�.
     * 2 - corrupt page�� �߰��ϸ� ImgLog�� ���� ��� OverWrite �Ѵ�.
     *     �� Group Hdr page�� corrupt �� ��츦 �����ϰ�
     *     Corrupt Page�� Overwrite���� ���ص� ���� ���� ���� �ʴ´�.
     * 3 - corrupt page�� �߰��ϸ� ImgLog�� OverWrite �õ�
     *     �������� ���� Corrupt Page�� �����Ѵٸ� ���� ����.
     * 
     * * ���: 0, 2 ABORT / 1, 3 FATAL 
     */ 
    IDP_DEF(UInt, "CORRUPT_PAGE_ERR_POLICY",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 3, 2); 

    // bug-19279 remote sysdba enable + sys can kill session
    // default: 1 (on)
    IDP_DEF(UInt, "REMOTE_SYSDBA_ENABLE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 1 );

    // BUG-24993 ��Ʈ��ũ ���� �޽��� log ����
    // default: 1 (on)
    IDP_DEF(UInt, "NETWORK_ERROR_LOG",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 1 );

    /* ------------------------------------------------------------------
     *   Shard Related
     * --------------------------------------------------------------*/

    IDP_DEF(UInt, "SHARD_ENABLE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_USER|
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 0 );

    IDP_DEF(UInt, "__SHARD_TEST_ENABLE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_USER|
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 0 );

    /* BUG-48247 */
    IDP_DEF(UInt, "__SHARD_ALLOW_AUTO_COMMIT",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 1);

    IDP_DEF(UInt, "__SHARD_LOCAL_FORCE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 0 );

    // BUG-47108
    IDP_DEF(UInt, "SHARD_AGGREGATION_TRANSFORM_ENABLE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 1 );

    IDP_DEF(UInt, "SHARD_INTERNAL_CONN_ATTR_RETRY_COUNT",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_USER|
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            IDP_SHARD_INTERNAL_CONN_ATTR_RETRY_COUNT_MIN,
            IDP_SHARD_INTERNAL_CONN_ATTR_RETRY_COUNT_MAX,
            IDP_SHARD_INTERNAL_CONN_ATTR_RETRY_COUNT_DEFAULT);

    IDP_DEF(UInt, "SHARD_INTERNAL_CONN_ATTR_RETRY_DELAY",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_USER|
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            IDP_SHARD_INTERNAL_CONN_ATTR_RETRY_DELAY_MIN,
            IDP_SHARD_INTERNAL_CONN_ATTR_RETRY_DELAY_MAX,
            IDP_SHARD_INTERNAL_CONN_ATTR_RETRY_DELAY_DEFAULT);

    /* BUG-45967 Rebuild Data �Ϸ� ��� */
    IDP_DEF(SInt, "SHARD_REBUILD_DATA_STEP",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_USER|
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            -1, 1000, 0);

    IDP_DEF(UInt, "SHARD_INTERNAL_CONN_ATTR_CONNECTION_TIMEOUT",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_USER|
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            IDP_SHARD_INTERNAL_CONN_ATTR_CONNECTION_TIMEOUT_MIN,
            IDP_SHARD_INTERNAL_CONN_ATTR_CONNECTION_TIMEOUT_MAX,
            IDP_SHARD_INTERNAL_CONN_ATTR_CONNECTION_TIMEOUT_DEFAULT);

    IDP_DEF(UInt, "SHARD_INTERNAL_CONN_ATTR_LOGIN_TIMEOUT",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_USER|
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            IDP_SHARD_INTERNAL_CONN_ATTR_LOGIN_TIMEOUT_MIN,
            IDP_SHARD_INTERNAL_CONN_ATTR_LOGIN_TIMEOUT_MAX,
            IDP_SHARD_INTERNAL_CONN_ATTR_LOGIN_TIMEOUT_DEFAULT);

    /* BUG-45899 */
    IDP_DEF(UInt, "TRCLOG_DETAIL_SHARD",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_USER|
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 0 );

    /* PROJ-2701 Sharding online data rebuild */
    IDP_DEF(UInt, "SHARD_REBUILD_PLAN_DETAIL_FORCE_ENABLE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_USER|
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 0 );

    IDP_DEF(UInt, "SHARD_REBUILD_LOCK_TABLE_WITH_DML_ENABLE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_USER|
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 0 );

    IDP_DEF(ULong, "SHARD_META_PROPAGATION_TIMEOUT",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_USER|
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0,         /* min = 0usec */
            60000000,  /* max = 60000000usec = 60sec */
            3000000 ); /* default 3000000usec = 3sec */

    IDP_DEF(UInt, "SHARD_TRANSFORM_STRING_LENGTH_MAX",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_USER|
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            32,    /* min = 32 bytes */
            65535, /* max */
            512 ); /* default 512 bytes */

    // BUG-47817
    IDP_DEF(UInt, "SHARD_ADMIN_MODE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_USER|
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 1);

    IDP_DEF(UInt, "ZOOKEEPER_LOCK_WAIT_TIMEOUT",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            10,     /* min = 10sec */
            36000,  /* max = 36000sec = 10hour */
            3600 ); /* default 3600sec = 1hour */

    IDP_DEF(UInt, "SHARD_DDL_LOCK_TIMEOUT",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 65535, 3);

    IDP_DEF(UInt,  "SHARD_DDL_LOCK_TRY_COUNT",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            1, ID_UINT_MAX,  200 ); /* shard_ddl_lock_timeout default 3 sec * 200 => 600 sec, 10 min */

    /* �پ��� Node Crash ��Ȳ������ Failback TC �ۼ��� ����
     * FailoverForWatcher�� ������ ON/OFF �ϴ� ������Ƽ 
     * 0 �̸� FailoverForWatcher�� �����ϰ� 1�̸� �������� �ʵ��� �Ѵ�.
     * MIN     : 0 
     * MAX     : 1
     * Default : 0 */
    IDP_DEF(UInt,  "__DISABLE_FAILOVER_FOR_WATCHER",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 0 );
    
    /* ------------------------------------------------------------------
     *   Cluster Related
     * --------------------------------------------------------------*/

#ifdef ALTIBASE_PRODUCT_HDB
# define ALTIBASE_SID "altibase"
#else /* ALTIBASE_PRODUCT_HDB */
# define ALTIBASE_SID "xdbaltibase"
#endif /* ALTIBASE_PRODUCT_HDB */

    IDP_DEF(String, "SID",
            IDP_ATTR_SL_PFILE    | IDP_ATTR_SL_ENV |
            IDP_ATTR_IU_UNIQUE   |
            IDP_ATTR_MS_ANY      |
            IDP_ATTR_SK_ALNUM    |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            1, 12, ALTIBASE_SID);

#undef ALTIBASE_SID

    IDP_DEF(String, "SPFILE",
            IDP_ATTR_SL_PFILE       | IDP_ATTR_SL_ENV |
            IDP_ATTR_IU_IDENTICAL   |
            IDP_ATTR_MS_ANY         |
            IDP_ATTR_SK_PATH        |
            IDP_ATTR_LC_INTERNAL    |       /*���� External�� �����ؾ��� XXX*/
            IDP_ATTR_RD_READONLY    |
            IDP_ATTR_ML_JUSTONE     |
            IDP_ATTR_CK_CHECK,
            0, IDP_MAX_PROP_STRING_LEN, (SChar *)"");

    IDP_DEF(UInt, "CLUSTER_DATABASE",
            IDP_ATTR_SL_ALL         |
            IDP_ATTR_IU_IDENTICAL   |       
            IDP_ATTR_MS_ANY         |
            IDP_ATTR_LC_INTERNAL    |       /*���� External�� �����ؾ��� XXX*/
            IDP_ATTR_RD_READONLY    |
            IDP_ATTR_ML_JUSTONE     |
            IDP_ATTR_CK_CHECK,
            0, 1, 0 );

    IDP_DEF(UInt, "INSTANCE_NUMBER",
            IDP_ATTR_SL_SPFILE      |
            IDP_ATTR_IU_UNIQUE      |
            IDP_ATTR_MS_SHARE       |
            IDP_ATTR_LC_INTERNAL    |       /*���� External�� �����ؾ��� XXX*/
            IDP_ATTR_RD_READONLY    |
            IDP_ATTR_ML_JUSTONE     |
            IDP_ATTR_CK_CHECK,
            1, 100, 1 );

    IDP_DEF(String, "CLUSTER_INTERCONNECTS",
            IDP_ATTR_SL_SPFILE   |
            IDP_ATTR_IU_UNIQUE   |
            IDP_ATTR_MS_SHARE    |
            IDP_ATTR_LC_INTERNAL |          /*���� External�� �����ؾ��� XXX*/
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_MULTIPLE |
            IDP_ATTR_CK_CHECK,
            0, IDP_MAX_PROP_STRING_LEN, (SChar *)"");

    /* ------------------------------------------------------------------
     *   XA: bug-24840 divide xa log
     * --------------------------------------------------------------*/
    IDP_DEF(String, "XA_MSGLOG_FILE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_SK_PATH     |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, IDP_MAX_PROP_STRING_LEN, (SChar *)PRODUCT_PREFIX"altibase_xa.log");

    IDP_DEF(UInt, "XA_MSGLOG_SIZE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, ID_UINT_MAX, (10 * 1024 * 1024));

    IDP_DEF(UInt, "XA_MSGLOG_COUNT",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, ID_UINT_MAX, 10);

    // default: 0 = IDE_XA_1(normal print) + IDE_XA_2(xid print)
    IDP_DEF(UInt, "XA_MSGLOG_FLAG",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, ID_UINT_MAX, 0);

    /* ------------------------------------------------------------------
     *   MM: BUG-28866
     * --------------------------------------------------------------*/

    /* BUG-45369 */
    IDP_DEF(UInt, "MM_MSGLOG_FLAG",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, ID_UINT_MAX, 0);

    IDP_DEF(String, "MM_MSGLOG_FILE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_SK_PATH     |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, IDP_MAX_PROP_STRING_LEN, (SChar *)PRODUCT_PREFIX"altibase_mm.log");

    IDP_DEF(UInt, "MM_MSGLOG_SIZE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, ID_UINT_MAX, (10 * 1024 * 1024));

    IDP_DEF(UInt, "MM_MSGLOG_COUNT",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, ID_UINT_MAX, 10);

    IDP_DEF(UInt, "MM_SESSION_LOGGING",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 0);

/* BUG-45274 */
    IDP_DEF(UInt, "LB_MSGLOG_FLAG",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, ID_UINT_MAX, 1);

    IDP_DEF(String, "LB_MSGLOG_FILE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_SK_PATH     |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, IDP_MAX_PROP_STRING_LEN, (SChar *)PRODUCT_PREFIX"altibase_lb.log");

    IDP_DEF(UInt, "LB_MSGLOG_SIZE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, ID_UINT_MAX, (10 * 1024 * 1024));

    IDP_DEF(UInt, "LB_MSGLOG_COUNT",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, ID_UINT_MAX, 10);

    /* ------------------------------------------------------------------
     *   CM: BUG-41909 Add dump CM block when a packet error occurs
     * --------------------------------------------------------------*/

    IDP_DEF(UInt, "CM_MSGLOG_FLAG",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, ID_UINT_MAX, 1);

    IDP_DEF(String, "CM_MSGLOG_FILE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_SK_PATH     |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, IDP_MAX_PROP_STRING_LEN, (SChar *)PRODUCT_PREFIX"altibase_cm.log");

    IDP_DEF(UInt, "CM_MSGLOG_SIZE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, ID_UINT_MAX, (10 * 1024 * 1024));

    IDP_DEF(UInt, "CM_MSGLOG_COUNT",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, ID_UINT_MAX, 10);

    /* ------------------------------------------------------------------
     *   DUMP: PRJ-2118
     * --------------------------------------------------------------*/

    IDP_DEF(String, "DUMP_MSGLOG_FILE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_SK_PATH     |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, IDP_MAX_PROP_STRING_LEN, (SChar *)PRODUCT_PREFIX"altibase_dump.log");

    IDP_DEF(UInt, "DUMP_MSGLOG_SIZE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, ID_UINT_MAX, (10 * 1024 * 1024));

    IDP_DEF(UInt, "DUMP_MSGLOG_COUNT",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, ID_UINT_MAX, 10);

    /* ------------------------------------------------------------------
     *   ERROR: PRJ-2118
     * --------------------------------------------------------------*/

    IDP_DEF(String, "ERROR_MSGLOG_FILE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_SK_PATH     |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, IDP_MAX_PROP_STRING_LEN, (SChar *)PRODUCT_PREFIX"altibase_error.log");

    IDP_DEF(UInt, "ERROR_MSGLOG_SIZE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, ID_UINT_MAX, (10 * 1024 * 1024));

    IDP_DEF(UInt, "ERROR_MSGLOG_COUNT",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, ID_UINT_MAX, 10);

    // BUG-29506 TBT�� TBK�� ��ȯ�� ����� offset�� CTS�� �ݿ����� �ʽ��ϴ�.
    // �����ϱ� ���� CTS �Ҵ� ���θ� ���Ƿ� �����ϱ� ���� PROPERTY�� �߰�
    // 0 : CTS�Ҵ� �����ϸ� �Ҵ�(default), 1 : CTS�Ҵ� �����ϴ��� �Ҵ����� ����
    IDP_DEF(UInt, "__DISABLE_TRANSACTION_BOUND_IN_CTS",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 0);

    // BUG-29839 ����� undo page���� ���� CTS�� ������ �� �� ����.
    // �����ϱ� ���� transaction�� Ư�� segment entry�� binding�ϴ� ��� �߰�
    // 512 : (maximum transaction segment count) automatic
    //   1 : Ư�� entry id�� segment�� transaction binding
    IDP_DEF(UInt, "__MANUAL_BINDING_TRANSACTION_SEGMENT_BY_ENTRY_ID",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 512, 512 );

    //fix BUG-30566
    //shutdown immediate�� ��ٸ� TIMEOUT
    IDP_DEF(UInt, "SHUTDOWN_IMMEDIATE_TIMEOUT",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, ID_UINT_MAX, 60);

    // fix BUG-30731
    // V$STATEMENT, V$SQLTEXT, V$PLANTEXT ��ȸ��
    // �ѹ��� �˻��� Statement ��
    IDP_DEF(UInt, "STATEMENT_LIST_PARTIAL_SCAN_COUNT",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, ID_UINT_MAX, 0);

    /* BUG-31144 
     * Limit max number of statements per a session 
     */
    IDP_DEF(UInt, "MAX_STATEMENTS_PER_SESSION",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            /* PROJ-2109 : Remove the bottleneck of alloc/free stmts. */
            1, 65536, 1024);

    /*
     * BUG-31040 set �������� ���� �ʷ��Ǵ�
     *           qmvQuerySet::validate() �Լ� recursion depth �� �����մϴ�.
     */
    IDP_DEF(SInt, "__MAX_SET_OP_RECURSION_DEPTH",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            128, 16384, 1024);

    /*
     * PROJ-2118 Bug Reporting
     *
     * release mode������ �۵�, release ���� IDE_ERROR��
     *   0. ���ܷ� ó�� �� ������         ( Release default )
     *   1. Assert�� ó�� �� �������� ���� ( Debug default )
     */
#if defined(DEBUG)
    IDP_DEF( UInt, "__ERROR_VALIDATION_LEVEL",
             IDP_ATTR_SL_ALL |
             IDP_ATTR_SH_ALL |
             IDP_ATTR_IU_ANY |
             IDP_ATTR_MS_ANY |
             IDP_ATTR_LC_INTERNAL |
             IDP_ATTR_RD_WRITABLE |
             IDP_ATTR_ML_JUSTONE  |
             IDP_ATTR_CK_CHECK,
             0, 1, 1 );
#else
    IDP_DEF( UInt, "__ERROR_VALIDATION_LEVEL",
             IDP_ATTR_SL_ALL |
             IDP_ATTR_SH_ALL |
             IDP_ATTR_IU_ANY |
             IDP_ATTR_MS_ANY |
             IDP_ATTR_LC_INTERNAL |
             IDP_ATTR_RD_WRITABLE |
             IDP_ATTR_ML_JUSTONE  |
             IDP_ATTR_CK_CHECK,
             0, 1, 0 );
#endif

    /*
     * PROJ-1718 Subquery Unnesting
     *
     * 0: Disable
     * 1: Enable
     */
    IDP_DEF(UInt, "OPTIMIZER_UNNEST_SUBQUERY",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 1);

    IDP_DEF(UInt, "OPTIMIZER_UNNEST_COMPLEX_SUBQUERY",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 1);

    IDP_DEF(UInt, "OPTIMIZER_UNNEST_AGGREGATION_SUBQUERY",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 1);

    /*
        PROJ-2492 Dynamic sample selection
        0 : disable
        1 ; 32 page
        2 : 64 page
        3 : 128 page
        4 : 256 page
        5 : 512 page
        6 : 1024 page
        7 : 4096 page
        8 : 16384 page
        9 : 65536 page
        10 : All page
    */
    IDP_DEF(UInt, "OPTIMIZER_AUTO_STATS",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 10, 0);

    // BUG-43039 inner join push down
    IDP_DEF(UInt, "__OPTIMIZER_INNER_JOIN_PUSH_DOWN",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 1);

    // BUG-43068 Indexable order by ����
    IDP_DEF(UInt, "__OPTIMIZER_ORDER_PUSH_DOWN",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 0);

    // PROJ-2551 simple query ����ȭ
    IDP_DEF(UInt, "EXECUTOR_FAST_SIMPLE_QUERY",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 2, 2);

    /*
     * BUG-32177  The server might hang when disk is full during checkpoint.
     *
     * checkpoint������ ��ũ���� �������� �α������� ������ ���Ͽ�
     * hang�� �ɸ��� ������ "��ȭ"�ϱ� ���Ѱ���.(internal only!)
     */
    IDP_DEF(ULong, "__RESERVED_DISK_SIZE_FOR_LOGFILE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0,(1024*1024*1024) /*1G*/, 0);
  
    /*
     * PROJ-2118 Bug Reporting
     *
     * Error Trace ������� ���� ���� �϶��� �ذ��ϱ� ���� Property
     *   0. Error Trace �� ������� �ʰ� ���� ó���� �Ѵ�.
     *   1. Error Trace �� ����ϰ� ���� ó�� �Ѵ�. ( default )
     */
    IDP_DEF( UInt, "__WRITE_ERROR_TRACE",
             IDP_ATTR_SL_ALL |
             IDP_ATTR_SH_ALL |
             IDP_ATTR_IU_ANY |
             IDP_ATTR_MS_ANY |
             IDP_ATTR_LC_INTERNAL |
             IDP_ATTR_RD_WRITABLE |
             IDP_ATTR_ML_JUSTONE  |
             IDP_ATTR_CK_CHECK,
             0, 1, 1 );
    
    /*
     * PROJ-2118 Bug Reporting
     *
     * Pstack ������� ���� ���� �϶��� �ذ��ϱ� ���� Property
     *   0. Pstack �� ������� �ʰ� ���� ó���� �Ѵ�. ( default )
     *   1. Pstack �� ����ϰ� ���� ó�� �Ѵ�.
     */
    IDP_DEF( UInt, "__WRITE_PSTACK",
             IDP_ATTR_SL_ALL |
             IDP_ATTR_SH_ALL |
             IDP_ATTR_IU_ANY |
             IDP_ATTR_MS_ANY |
             IDP_ATTR_LC_INTERNAL |
             IDP_ATTR_RD_WRITABLE |
             IDP_ATTR_ML_JUSTONE  |
             IDP_ATTR_CK_CHECK,
             0, 1, 0 );

    /*
     * PROJ-2118 Bug Reporting
     *
     * sigaltstack()�� ���� ���� ���� ������ ���� �޸𸮰� �ҿ� �ȴ�.
     *   0. sigaltstack()�� ������� �ʴ´�.
     *   1. sigaltstack()�� ����Ѵ�. (default)
     */
    IDP_DEF( UInt, "__USE_SIGALTSTACK",
             IDP_ATTR_SL_ALL |
             IDP_ATTR_SH_ALL |
             IDP_ATTR_IU_ANY |
             IDP_ATTR_MS_ANY |
             IDP_ATTR_LC_INTERNAL |
             IDP_ATTR_RD_WRITABLE |
             IDP_ATTR_ML_JUSTONE  |
             IDP_ATTR_CK_CHECK,
             0, 1, 1 );
    
    /*
     * PROJ-2118 Bug Reporting
     *
     * A parameter of dump_collector.sh
     */
    IDP_DEF( UInt, "__LOGFILE_COLLECT_COUNT_IN_DUMP",
             IDP_ATTR_SL_ALL |
             IDP_ATTR_SH_ALL |
             IDP_ATTR_IU_ANY |
             IDP_ATTR_MS_ANY |
             IDP_ATTR_LC_INTERNAL |
             IDP_ATTR_RD_WRITABLE |
             IDP_ATTR_ML_JUSTONE  |
             IDP_ATTR_CK_CHECK,
             0, ID_UINT_MAX, 10 );

    /*
     * PROJ-2118 Bug Reporting
     *
     * Log File, Log Anchor, Trace File���� �����Ͽ�
     * �������Ϸ� ���� ������ ����,
     *
     *   0. Dump Info�� �������� �ʴ´�. ( debug mode default )
     *   1. Dump Info�� �����Ѵ�.        ( release mode default )
     */
#if defined(DEBUG)
    IDP_DEF( UInt, "COLLECT_DUMP_INFO",
             IDP_ATTR_SL_ALL |
             IDP_ATTR_SH_ALL |
             IDP_ATTR_IU_ANY |
             IDP_ATTR_MS_ANY |
             IDP_ATTR_LC_EXTERNAL |
             IDP_ATTR_RD_WRITABLE |
             IDP_ATTR_ML_JUSTONE  |
             IDP_ATTR_CK_CHECK,
             0, 1, 0 );
#else /* release */
    IDP_DEF( UInt, "COLLECT_DUMP_INFO",
             IDP_ATTR_SL_ALL |
             IDP_ATTR_SH_ALL |
             IDP_ATTR_IU_ANY |
             IDP_ATTR_MS_ANY |
             IDP_ATTR_LC_EXTERNAL |
             IDP_ATTR_RD_WRITABLE |
             IDP_ATTR_ML_JUSTONE  |
             IDP_ATTR_CK_CHECK,
             0, 1, 1 );
#endif

    /*
     * PROJ-2118 Bug Reporting
     *
     * Windows Mini Dump ���� ���� ���� ������Ƽ
     *   0. Mini Dump �̻��� ( default )
     *   1. Mini Dump ����
     */
    IDP_DEF( UInt, "__WRITE_WINDOWS_MINIDUMP",
             IDP_ATTR_SL_ALL |
             IDP_ATTR_SH_ALL |
             IDP_ATTR_IU_ANY |
             IDP_ATTR_MS_ANY |
             IDP_ATTR_LC_INTERNAL |
             IDP_ATTR_RD_WRITABLE |
             IDP_ATTR_ML_JUSTONE  |
             IDP_ATTR_CK_CHECK,
             0, 1, 1 );

    /* BUG-36203 PSM Optimize */
    IDP_DEF(UInt, "PSM_TEMPLATE_CACHE_COUNT",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 64, 0);

    /*
     * TASK-4949
     * BUG-32751
     * Memory allocator selection
     */
    IDP_DEF(UInt, "MEMORY_ALLOCATOR_TYPE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 0);
    IDP_DEF(UInt, "MEMORY_ALLOCATOR_USE_PRIVATE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 0);
    IDP_DEF(ULong, "MEMORY_ALLOCATOR_POOLSIZE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            256*1024, 1024*1024*1024, 16*1024*1024);
    IDP_DEF(ULong, "MEMORY_ALLOCATOR_POOLSIZE_PRIVATE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            256*1024, 1024*1024, 256*1024);
    IDP_DEF(SInt, "MEMORY_ALLOCATOR_DEFAULT_SPINLOCK_COUNT",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            -1, ID_SINT_MAX, 1024);
    IDP_DEF(UInt, "MEMORY_ALLOCATOR_AUTO_SHRINK",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 1);
    /*
     * BUG-37722. Limit count of TLSF instances.
     * 0 - Default, follow system setting.
     */
    IDP_DEF(UInt, "MEMORY_ALLOCATOR_MAX_INSTANCES",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 512, 0);
    /*
     * Project 2408 Maximum size of global allocator.
     * 0 - Default, unlimited.
     */
    IDP_DEF(ULong, "MEMORY_ALLOCATOR_POOLSIZE_GLOBAL",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, ID_vULONG_MAX, 0);
    /*
     * Project 2379
     * Limit the number of resources
     */
    IDP_DEF(UInt, "MAX_THREAD_COUNT",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 65536, 0);

    IDP_DEF( UInt, "THREAD_REUSE_ENABLE",
             IDP_ATTR_SL_ALL |
             IDP_ATTR_SH_ALL |
             IDP_ATTR_IU_ANY |
             IDP_ATTR_MS_ANY |
             IDP_ATTR_LC_EXTERNAL |
             IDP_ATTR_RD_READONLY |
             IDP_ATTR_ML_JUSTONE  |
             IDP_ATTR_CK_CHECK,
             0, 1, 1);

/* --------------------------------------------------------------------
 * PROJ-2133 Incremental Backup
 * PROJ-2488 Incremental Backup in XDB
 * incremental backup�� backup���ϵ��� ������ ������ �Ⱓ�� ���Ѵ�.
 * ���� ��(day)�����̴�. 0���� ���� �����Ǹ� level0 ����� �����Ǹ� �ٷ�
 * ������ ����� ��� backup���� obsolete�� backupinfo�� �ȴ�.
 * ----------------------------------------------------------------- */
    IDP_DEF(UInt, "INCREMENTAL_BACKUP_INFO_RETENTION_PERIOD",
            IDP_ATTR_SL_PFILE |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, ID_UINT_MAX, 0);

/* --------------------------------------------------------------------
 * PROJ-2133 Incremental Backup
 * PROJ-2488 Incremental Backup in XDB
 * �׽�Ʈ �������� incremental backup�� backup ���ϵ��� ������ ������ �Ⱓ�� ���Ѵ�.
 * ���� ��(second)�����̴�. 0���� ���� �����Ǹ� level0 ����� �����Ǹ� �ٷ�
 * ������ ����� ��� backup���� obsolete�� backupinfo�� �ȴ�.
 * ----------------------------------------------------------------- */
    IDP_DEF(UInt, "__INCREMENTAL_BACKUP_INFO_RETENTION_PERIOD_FOR_TEST",
            IDP_ATTR_SL_PFILE |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, ID_UINT_MAX, 0);

/* --------------------------------------------------------------------
 * PROJ-2133 Incremental Backup
 * PROJ-2488 Incremental Backup in XDB
 * Incremental chunk change traking�� �����Ҷ� ���
 * �������� ��� �������� �����Ѵ�. ������ page �����̴�.
 * ----------------------------------------------------------------- */
    IDP_DEF(UInt, "INCREMENTAL_BACKUP_CHUNK_SIZE",
            IDP_ATTR_SL_PFILE |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            1, ID_UINT_MAX, 4);

/* --------------------------------------------------------------------
 * PROJ-2133 Incremental Backup
 * PROJ-2488 Incremental Backup in XDB
 * Incremental chunk change traking ����� ����� ������������ ����
 * �� bitamp block�� bitmap ������ ũ�⸦ ���Ѵ�.
 * �������� changeTracking������ Ȯ���� ���� �߻��Ѵ�.
 * ----------------------------------------------------------------- */
    IDP_DEF(UInt, "__INCREMENTAL_BACKUP_BMP_BLOCK_BITMAP_SIZE",
            IDP_ATTR_SL_PFILE |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            1, 488, 488);

/* --------------------------------------------------------------------
 * PROJ-2133 Incremental Backup
 * PROJ-2488 Incremental Backup in XDB
 * Incremental chunk change traking body�� ���� extent�� ������ �����Ѵ�.
 * �������� changeTracking������ Ȯ���� ���� �߻��Ѵ�.
 * ----------------------------------------------------------------- */
    IDP_DEF(UInt, "__INCREMENTAL_BACKUP_CTBODY_EXTENT_CNT",
            IDP_ATTR_SL_PFILE |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            32, 320, 320);

/* --------------------------------------------------------------------
 * PROJ-2133 Incremental Backup
 * PROJ-2488 Incremental Backup in XDB
 * ALTER DATABASE CHANGE BACKUP DIRECTORY ��directory_path��; ������
 * �����θ� �Է¹ް� �Ǿ��ִ�. testcase �ۼ��� ���� ����θ�
 * �Է°����ϰ� �Ѵ�.
 *
 * 0: ������ �Է°���
 * 1: ����� �Է°��� (smuProperty::getDefaultDiskDBDir() �ؿ� ��λ���)
 * ----------------------------------------------------------------- */
    IDP_DEF(UInt, "__INCREMENTAL_BACKUP_PATH_MAKE_ABS_PATH",
            IDP_ATTR_SL_PFILE |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 0);
			
    /* PROJ-2207 Password policy support */
    IDP_DEF(String, "__SYSDATE_FOR_NATC",
            IDP_ATTR_SL_ALL       |
            IDP_ATTR_IU_ANY       |
            IDP_ATTR_MS_ANY       |
            IDP_ATTR_SK_ASCII     |
            IDP_ATTR_LC_INTERNAL  |
            IDP_ATTR_RD_WRITABLE  |
            IDP_ATTR_ML_JUSTONE   |
            IDP_ATTR_CK_CHECK,
            0, IDP_MAX_PROP_STRING_LEN, (SChar *)"");

    // FAILED_LOGIN_ATTEMPTS
    IDP_DEF(UInt, "FAILED_LOGIN_ATTEMPTS",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1000, 0 );
    
    /* 
     * PROJ-2047 Strengthening LOB - LOBCACHE
     */
    IDP_DEF(UInt, "LOB_CACHE_THRESHOLD",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 524288, 8192);  /* BUG-46411 */

    // PASSWORD_LOCK_TIME
    IDP_DEF(UInt, "PASSWORD_LOCK_TIME",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 3650, 0 );

    // PASSWORD_LIFE_TIME
    IDP_DEF(UInt, "PASSWORD_LIFE_TIME",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 3650, 0 );

    // PASSWORD_GRACE_TIME
    IDP_DEF(UInt, "PASSWORD_GRACE_TIME",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 3650, 0 );

    // PASSWORD_REUSE_MAX
    IDP_DEF(UInt, "PASSWORD_REUSE_MAX",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1000, 0 );

    // PASSWORD_REUSE_TIME
    IDP_DEF(UInt, "PASSWORD_REUSE_TIME",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 3650, 0 );

    // PASSWORD_VERIFY_FUNCTION
    IDP_DEF(String, "PASSWORD_VERIFY_FUNCTION",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 40, (SChar *)"");

     IDP_DEF(UInt, "__SOCK_WRITE_TIMEOUT",
             IDP_ATTR_SL_ALL |
             IDP_ATTR_SH_ALL |
             IDP_ATTR_IU_ANY |
             IDP_ATTR_MS_ANY |
             IDP_ATTR_LC_INTERNAL |
             IDP_ATTR_RD_WRITABLE |
             IDP_ATTR_ML_JUSTONE  |
             IDP_ATTR_CK_CHECK,
             0, ID_UINT_MAX, 1800);

    /* PROJ-2102 Fast Secondary Buffer */
    // 0 : disable
    // 1 : enable
    IDP_DEF(UInt, "SECONDARY_BUFFER_ENABLE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 0);
 
    // Min : 1, Max : 16, Default : 2
    IDP_DEF( UInt, "SECONDARY_BUFFER_FLUSHER_CNT",
             IDP_ATTR_SL_ALL |
             IDP_ATTR_SH_ALL |
             IDP_ATTR_IU_ANY |
             IDP_ATTR_MS_ANY |
             IDP_ATTR_LC_EXTERNAL |
             IDP_ATTR_RD_READONLY |
             IDP_ATTR_ML_JUSTONE  |
             IDP_ATTR_CK_CHECK,
             1, 16, 2 );

    // Min : 0, Max : 16, Default : 0
    IDP_DEF( UInt, "__MAX_SECONDARY_CHECKPOINT_FLUSHER_CNT",
             IDP_ATTR_SL_ALL |
             IDP_ATTR_SH_ALL |
             IDP_ATTR_IU_ANY |
             IDP_ATTR_MS_ANY |
             IDP_ATTR_LC_INTERNAL |
             IDP_ATTR_RD_READONLY |
             IDP_ATTR_ML_JUSTONE  |
             IDP_ATTR_CK_CHECK,
             0, 16, 0 );

    // Min : 0, Max : 2, Default : 2
    // 0 : ALL  1 : Dirty  2 : Clean 
    IDP_DEF( UInt, "SECONDARY_BUFFER_TYPE",
             IDP_ATTR_SL_ALL |
             IDP_ATTR_SH_ALL |
             IDP_ATTR_IU_ANY |
             IDP_ATTR_MS_ANY |
             IDP_ATTR_LC_EXTERNAL |
             IDP_ATTR_RD_READONLY |
             IDP_ATTR_ML_JUSTONE  |
             IDP_ATTR_CK_CHECK,
             0, 2, 2 );

    // Min : 0, Max : 32G, Default : 0
    IDP_DEF( ULong, "SECONDARY_BUFFER_SIZE",
             IDP_ATTR_SL_ALL |
             IDP_ATTR_SH_ALL |
             IDP_ATTR_IU_ANY |
             IDP_ATTR_MS_ANY |
             IDP_ATTR_LC_EXTERNAL |
             IDP_ATTR_RD_READONLY |
             IDP_ATTR_ML_JUSTONE  |
             IDP_ATTR_CK_CHECK,
             0,
             IDP_DRDB_DATAFILE_MAX_SIZE,
             0);
  
     //  
     IDP_DEF( String, "SECONDARY_BUFFER_FILE_DIRECTORY",
              IDP_ATTR_SL_ALL |
              IDP_ATTR_SH_ALL |
              IDP_ATTR_IU_ANY |
              IDP_ATTR_MS_ANY |
              IDP_ATTR_SK_PATH     |
              IDP_ATTR_LC_EXTERNAL |
              IDP_ATTR_RD_READONLY |
              IDP_ATTR_ML_JUSTONE  |
              IDP_ATTR_CK_CHECK,
              0, IDP_MAX_PROP_STRING_LEN, (SChar *)"");

    /* PROJ-2208 */
    IDP_DEF(String, "NLS_TERRITORY",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_SK_ASCII    |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, IDP_MAX_PROP_STRING_LEN, (SChar *)"KOREA");

    /* PROJ-2208 */
    IDP_DEF(String, "NLS_ISO_CURRENCY",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_SK_ASCII    |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, IDP_MAX_PROP_STRING_LEN, (SChar *)"");

    /* PROJ-2208 */
    IDP_DEF(String, "NLS_CURRENCY",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_SK_MULTI_BYTE |
            IDP_ATTR_LC_EXTERNAL   |
            IDP_ATTR_RD_WRITABLE   |
            IDP_ATTR_ML_JUSTONE    |
            IDP_ATTR_CK_CHECK,
            0, IDP_MAX_PROP_STRING_LEN, (SChar *)"");

    /* PROJ-2208 */
    IDP_DEF(String, "NLS_NUMERIC_CHARACTERS",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_SK_ASCII    |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, IDP_MAX_PROP_STRING_LEN, (SChar *)"");

    /* PROJ-2209 DBTIMEZONE */
    IDP_DEF(String, "TIME_ZONE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 40, (SChar *)"OS_TZ");

    /* PROJ-2232 archivelog ����ȭ ���� ���丮 */
    IDP_DEF(String, "ARCHIVE_MULTIPLEX_DIR",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_SK_PATH     |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_MULTIPLE |
            IDP_ATTR_DF_USE_DEFAULT |
            IDP_ATTR_CK_CHECK,
            0, IDP_MAX_PROP_STRING_LEN, (SChar *)"");

    /* PROJ-2232 archivelog ����ȭ ��*/
    IDP_DEF(UInt, "ARCHIVE_MULTIPLEX_COUNT",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 10 , 0);

    /* PROJ-2232 log ����ȭ ���� ���丮 */
    IDP_DEF(String, "LOG_MULTIPLEX_DIR",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_SK_PATH     |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_MULTIPLE |
            IDP_ATTR_DF_USE_DEFAULT |
            IDP_ATTR_CK_CHECK,
            0, IDP_MAX_PROP_STRING_LEN, (SChar *)"");

    /* PROJ-2232 log ����ȭ ��*/
    IDP_DEF(UInt, "LOG_MULTIPLEX_COUNT",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 10 , 0);

    /*
     * PROJ_2232 log multiplex
     * log����ȭ append Thread�� sleep������ �����Ѵ�.
     * ���� ������ ���� sleep�ϰ� ���� ũ�� �幰�� sleep�Ѵ�.
     * ID_UINT_MAX���̸� sleep���� �ʴ´�.
     */
    IDP_DEF(UInt, "__LOG_MULTIPLEX_THREAD_SPIN_COUNT",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 
            ID_UINT_MAX,
            (idlVA::getProcessorCount() > 1) ?
            (idlVA::getProcessorCount() - 1)*100000 : 1);

    /* ==============================================================
     * PROJ-2108 Dedicated thread mode which uses less CPU
     * ============================================================== */

     IDP_DEF(UInt, "THREAD_CPU_AFFINITY",
             IDP_ATTR_SL_ALL |
             IDP_ATTR_SH_ALL |
             IDP_ATTR_IU_ANY |
             IDP_ATTR_MS_ANY |
             IDP_ATTR_LC_EXTERNAL |
             IDP_ATTR_RD_READONLY |
             IDP_ATTR_ML_JUSTONE  |
             IDP_ATTR_CK_CHECK,
             0, 1, 0);

     IDP_DEF(UInt, "DEDICATED_THREAD_MODE",
             IDP_ATTR_SL_ALL |
             IDP_ATTR_SH_ALL |
             IDP_ATTR_IU_ANY |
             IDP_ATTR_MS_ANY |
             IDP_ATTR_LC_EXTERNAL |
             IDP_ATTR_RD_READONLY |
             IDP_ATTR_ML_JUSTONE  |
             IDP_ATTR_CK_CHECK,
             0, 1, 0);

     IDP_DEF(UInt, "DEDICATED_THREAD_INIT_COUNT",
             IDP_ATTR_SL_ALL |
             IDP_ATTR_SH_ALL |
             IDP_ATTR_IU_ANY |
             IDP_ATTR_MS_ANY |
             IDP_ATTR_LC_EXTERNAL |
             IDP_ATTR_RD_READONLY |
             IDP_ATTR_ML_JUSTONE  |
             IDP_ATTR_CK_CHECK,
             1, 65535, 4);

     IDP_DEF(UInt, "DEDICATED_THREAD_MAX_COUNT",
             IDP_ATTR_SL_ALL |
             IDP_ATTR_SH_ALL |
             IDP_ATTR_IU_ANY |
             IDP_ATTR_MS_ANY |
             IDP_ATTR_LC_EXTERNAL |
             IDP_ATTR_RD_READONLY |
             IDP_ATTR_ML_JUSTONE  |
             IDP_ATTR_CK_CHECK,
             1, 65535, 1000);
     
     IDP_DEF(UInt, "DEDICATED_THREAD_CHECK_INTERVAL",
             IDP_ATTR_SL_ALL |
             IDP_ATTR_SH_ALL |
             IDP_ATTR_IU_ANY |
             IDP_ATTR_MS_ANY |
             IDP_ATTR_LC_EXTERNAL |
             IDP_ATTR_RD_READONLY |
             IDP_ATTR_ML_JUSTONE  |
             IDP_ATTR_CK_CHECK,
             0, ID_UINT_MAX, 3600);

     /* 
      * BUG-35179 Add property for parallel logical ager 
      * �����ϰ��ִ� logical ager�� ���� ������ index���� �۾��� �ϴ� service
      * thread���� ������ ���ϵȴ�. ���� ���ķ� �����ϴ� logical ager�� ����
      * �����ؾ��Ѵ�.
      * LOGICAL_AGER_COUNT_ ������Ƽ�� �̿��� ��������.
      *
      * main trunk�� parallel logical ager ������ ���� �⺻���� 1�� �����Ѵ�.
      * 
      */
#if defined(ALTIBASE_PRODUCT_HDB)
     IDP_DEF(UInt, "__PARALLEL_LOGICAL_AGER",
             IDP_ATTR_SL_ALL |
             IDP_ATTR_SH_ALL |
             IDP_ATTR_IU_ANY |
             IDP_ATTR_MS_ANY |
             IDP_ATTR_LC_INTERNAL |
             IDP_ATTR_RD_READONLY |
             IDP_ATTR_ML_JUSTONE  |
             IDP_ATTR_CK_CHECK,
             0, 1, 1);
#else
     IDP_DEF(UInt, "__PARALLEL_LOGICAL_AGER",
             IDP_ATTR_SL_ALL |
             IDP_ATTR_SH_ALL |
             IDP_ATTR_IU_ANY |
             IDP_ATTR_MS_ANY |
             IDP_ATTR_LC_INTERNAL |
             IDP_ATTR_RD_READONLY |
             IDP_ATTR_ML_JUSTONE  |
             IDP_ATTR_CK_CHECK,
             0, 1, 0);
#endif
     
     /* PROJ-1753 */
     IDP_DEF(UInt, "__LIKE_OP_USE_OLD_MODULE",
             IDP_ATTR_SL_ALL |
             IDP_ATTR_SH_ALL |
             IDP_ATTR_IU_ANY |
             IDP_ATTR_MS_ANY |
             IDP_ATTR_LC_INTERNAL |
             IDP_ATTR_RD_READONLY |
             IDP_ATTR_ML_JUSTONE  |
             IDP_ATTR_CK_CHECK,
             0, 1, 0 );

     // bug-35371
     IDP_DEF(UInt, "XA_HASH_SIZE",
             IDP_ATTR_SL_ALL |
             IDP_ATTR_SH_ALL |
             IDP_ATTR_IU_ANY |
             IDP_ATTR_MS_ANY |
             IDP_ATTR_LC_INTERNAL |
             IDP_ATTR_RD_READONLY |
             IDP_ATTR_ML_JUSTONE  |
             IDP_ATTR_CK_CHECK,
             10, 4096, 21);

     // bug-35381
     IDP_DEF(UInt, "XID_MEMPOOL_ELEMENT_COUNT",
             IDP_ATTR_SL_ALL |
             IDP_ATTR_SH_ALL |
             IDP_ATTR_IU_ANY |
             IDP_ATTR_MS_ANY |
             IDP_ATTR_LC_INTERNAL |
             IDP_ATTR_RD_READONLY |
             IDP_ATTR_ML_JUSTONE  |
             IDP_ATTR_CK_CHECK,
             4, 4096, 4);

     // bug-35382
     IDP_DEF(UInt, "XID_MUTEX_POOL_SIZE",
             IDP_ATTR_SL_ALL |
             IDP_ATTR_SH_ALL |
             IDP_ATTR_IU_ANY |
             IDP_ATTR_MS_ANY |
             IDP_ATTR_LC_INTERNAL |
             IDP_ATTR_RD_READONLY |
             IDP_ATTR_ML_JUSTONE  |
             IDP_ATTR_CK_CHECK,
             1, 1024, 10);

    // PROJ-1685
    IDP_DEF(UInt, "EXTPROC_AGENT_CONNECT_TIMEOUT",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            5, ID_UINT_MAX, 60);

    IDP_DEF(UInt, "EXTPROC_AGENT_IDLE_TIMEOUT",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            5, ID_UINT_MAX, 300);

    IDP_DEF(UInt, "EXTPROC_AGENT_CALL_RETRY_COUNT",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            1, 10, 1);

    // BUG-44652 Socket file path of EXTPROC AGENT could be set by property.
    IDP_DEF(String, "EXTPROC_AGENT_SOCKET_FILEPATH",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_SK_PATH     |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, IDP_MAX_PROP_STRING_LEN, (SChar *)"?"IDL_FILE_SEPARATORS"trc"IDL_FILE_SEPARATORS);

     IDE_TEST( registDatabaseLinkProperties() != IDE_SUCCESS );

    /* BUG-36662 Add property for archive thread to kill server when doesn't
     * exist source logfile
     * �α����� arching�� archive thread�� abort�Ǹ� archiving��
     * �α������� �����ϴ��� �˻��ϰ� �������� �ʴٸ� ���������� ��Ų��.
     */
     IDP_DEF( UInt, "__CHECK_LOGFILE_WHEN_ARCH_THR_ABORT",
              IDP_ATTR_SL_ALL |
              IDP_ATTR_SH_ALL |
              IDP_ATTR_IU_ANY |
              IDP_ATTR_MS_ANY |
              IDP_ATTR_LC_INTERNAL |
              IDP_ATTR_RD_READONLY |
              IDP_ATTR_ML_JUSTONE  |
              IDP_ATTR_CK_CHECK,
              0, 1, 0 );

    /* 
     * BUG-35443 Add Property for Excepting SYS_TBS_MEM_DIC size from
     * MEM_MAX_DB_SIZE
     * SYS_TBS_MEM_DIC size�� MEM_MAX_DB_SIZE�ʹ� ������ �и��Ѵ�.
     * ���� SYS_TBS_MEM_DIC size�� �ִ� MEM_MAX_DB_SIZE��ŭ ������ �� �ְ��Ѵ�.
     */
    IDP_DEF(UInt, "__SEPARATE_DIC_TBS_SIZE_ENABLE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 0);

    // BUG-36203
    IDP_DEF(UInt, "__QUERY_HASH_STRING_LENGTH_MAX",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            1, ID_UINT_MAX, ID_UINT_MAX);
    
    // BUG-37247
    IDP_DEF(UInt, "SYS_CONNECT_BY_PATH_PRECISION",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            1, 32000, 4000);
    
    // BUG-37247
    IDP_DEF(UInt, "GROUP_CONCAT_PRECISION",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            1, 32000, 4000);

    // BUG-38842
    IDP_DEF(UInt, "CLOB_TO_VARCHAR_PRECISION",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, ID_UINT_MAX, ID_UINT_MAX);

    // BUG-38952
    IDP_DEF(UInt, "TYPE_NULL",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 0);

    // BUG-41194
    IDP_DEF(UInt, "IEEE754_DOUBLE_TO_NUMERIC_FAST_CONVERSION",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 1);

    // BUG-41555 DBMS PIPE
    // rw-rw-rw(0666) : 0, rw-r-r(0644) : 1 Default : 0 (0666)
    IDP_DEF(UInt, "MSG_QUEUE_PERMISSION",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 0);

    // BUG-37252
    IDP_DEF(UInt, "EXECUTION_PLAN_MEMORY_CHECK",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 0);

    // BUG-37302
    IDP_DEF(UInt, "SQL_ERROR_INFO_SIZE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            1024, 1024*1024*10, 10240);

    // PROJ-2362 memory temp ���� ȿ���� ����
    IDP_DEF(UInt, "REDUCE_TEMP_MEMORY_ENABLE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 0);

    /* BUG-38254  alter table xxx ���� hang�� �ɸ��� �ֽ��ϴ� */
    IDP_DEF(UInt, "__TABLE_BACKUP_TIMEOUT",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, IDV_MAX_TIME_INTERVAL_SEC, 600 /*sec*/);

    /* BUG-38621  log ��Ͻ� ����η� ���� (Disaster Recovery �׽�Ʈ �뵵) */
    IDP_DEF(UInt, "__RELATIVE_PATH_IN_LOG",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 0);

    // BUG-38101
    IDP_DEF(UInt, "CASE_SENSITIVE_PASSWORD",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 0);

    /* 
     * BUG-38951 Support to choice a type of CM dispatcher on run-time
     *
     * 0 : Reserved (for the future)
     * 1 : Select
     * 2 : Poll     (If supported by OS)
     * 3 : Epoll    (If supported by OS) - BUG-45240
     */
    #define IDP_CM_DISPATCHER_SOCK_POLL_TYPE_MIN       (1)
#if defined(PDL_HAS_EPOLL)
    #define IDP_CM_DISPATCHER_SOCK_POLL_TYPE_MAX       (3)
    #define IDP_CM_DISPATCHER_SOCK_POLL_TYPE_DEFAULT   (3)
#elif defined(PDL_HAS_POLL)
    #define IDP_CM_DISPATCHER_SOCK_POLL_TYPE_MAX       (2)
    #define IDP_CM_DISPATCHER_SOCK_POLL_TYPE_DEFAULT   (1)
#else
    #define IDP_CM_DISPATCHER_SOCK_POLL_TYPE_MAX       (1)
    #define IDP_CM_DISPATCHER_SOCK_POLL_TYPE_DEFAULT   (1)
#endif
    IDP_DEF(UInt, "CM_DISPATCHER_SOCK_POLL_TYPE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            IDP_CM_DISPATCHER_SOCK_POLL_TYPE_MIN,
            IDP_CM_DISPATCHER_SOCK_POLL_TYPE_MAX,
            IDP_CM_DISPATCHER_SOCK_POLL_TYPE_DEFAULT);

    // fix BUG-39754
    IDP_DEF(UInt, "__FOREIGN_KEY_LOCK_ROW",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 2, 1 );

    // BUG-39679 Ericsson POC bug
    IDP_DEF(UInt, "__ENABLE_ROW_TEMPLATE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 1);

    // BUG-40042 oracle outer join property
    IDP_DEF(UInt, "OUTER_JOIN_OPERATOR_TRANSFORM_ENABLE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 1);

    /* PROJ-2451 Concurrent Execute Package */
    IDP_DEF(UInt, "CONCURRENT_EXEC_DEGREE_MAX",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1024, IDL_MIN(idlVA::getProcessorCount(), 1024));

    /* PROJ-2451 Concurrent Execute Package */
    IDP_DEF(UInt, "CONCURRENT_EXEC_DEGREE_DEFAULT",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            2, 1024, 4);

    /* PROJ-2451 Concurrent Execute Package */
    IDP_DEF(UInt, "CONCURRENT_EXEC_WAIT_INTERVAL",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            10, 1000000, 100);

    /* BUG-40138 
     * Flusher������ checkpoint flush�� replacement flush��
     * checkpoint thread������ checkpoint flush ���� �켱 ������ ���ƾ� �Ѵ�.
     * ����, flusher������ checkpoint flush ���� ���� �ٸ� flush �۾��� ����
     * ��û/������ �ִ��� Ȯ���ϵ��� �Ѵ�.
     * ��) ������Ƽ�� ���� 10�̸�, flusher�� ���� checkpoint flush�� ����
           10���� �������� flush�ɶ����� ���� üũ�� �Ѵ�. */
    IDP_DEF(UInt, "__FLUSHER_BUSY_CONDITION_CHECK_INTERVAL",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            1, ID_UINT_MAX, 64);

    /* BUG-41168 SSL extension */
    IDP_DEF(UInt, "TCP_ENABLE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 1);

    /* PROJ-2474 SSL/TLS */
    IDP_DEF(UInt, "SSL_ENABLE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 0);

    IDP_DEF(UInt, "SSL_CLIENT_AUTHENTICATION",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 0);

    IDP_DEF(UInt, "__SSL_VERIFY_PEER_CERTIFICATE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 0);

    IDP_DEF(UInt, "SSL_PORT_NO",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            1024,   /* min */
            65535,  /* max */
            20443); /* default */

    IDP_DEF(UInt, "SSL_MAX_LISTEN",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 16384, 128);

    IDP_DEF(String, "SSL_CIPHER_LIST",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_SK_PATH     |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE|
            IDP_ATTR_CK_CHECK,
            0, IDP_MAX_PROP_STRING_LEN, (SChar *)"");

    IDP_DEF(String, "SSL_CA",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_SK_PATH     |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE|
            IDP_ATTR_CK_CHECK,
            0, IDP_MAX_PROP_STRING_LEN, (SChar *)"");

    IDP_DEF(String, "SSL_CAPATH",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_SK_PATH     |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE|
            IDP_ATTR_CK_CHECK,
            0, IDP_MAX_PROP_STRING_LEN, (SChar *)"");

    IDP_DEF(String, "SSL_CERT",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_SK_PATH     |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE|
            IDP_ATTR_CK_CHECK,
            0, IDP_MAX_PROP_STRING_LEN, (SChar *)"");

    IDP_DEF(String, "SSL_KEY",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_SK_PATH     |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE|
            IDP_ATTR_CK_CHECK,
            0, IDP_MAX_PROP_STRING_LEN, (SChar *)"");

    IDP_DEF(UInt, "DCI_RETRY_WAIT_TIME",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            1, (60000000) /* sec */ , (1000));

    /* PROJ-2441 flashback  size byte */
    IDP_DEF(ULong, "RECYCLEBIN_MEM_MAX_SIZE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, ID_ULONG_MAX, ID_ULONG( 4 * 1024 * 1024 * 1024 ) );
    
    IDP_DEF(ULong, "RECYCLEBIN_DISK_MAX_SIZE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, ID_ULONG_MAX, ID_ULONG_MAX);
    
    IDP_DEF(UInt, "RECYCLEBIN_ENABLE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 0);
    
    IDP_DEF(UInt, "__RECYCLEBIN_FOR_NATC",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 9, 0);

    /* BUG-40790 */
    IDP_DEF(UInt, "__LOB_CURSOR_HASH_BUCKET_COUNT",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            1, 65535, 5);

    /* BUG-42639 Monitoring query */
    IDP_DEF(UInt, "OPTIMIZER_PERFORMANCE_VIEW",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 1);

    /* BUG-48160 fixed table���� table lock �ɸ� table�� skip�ϰ� ��� 
     * 0 : lock wait (default)
     * 1 : skip lock table
     * 2 : small info for lock table (not support, next job)*/
    IDP_DEF(UInt, "__SKIP_LOCKED_TABLE_AT_FIXED_TABLE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 0);

    /************************************************/
    /* PROJ-2553 Cache-aware Memory Hash Temp Table */
    /************************************************/

    // 0 : Array-Partitioned Memory Hash Temp Table�� ���
    // 1 : Bucket-based Memory Hash Temp Table�� ���
    IDP_DEF(UInt, "HASH_JOIN_MEM_TEMP_PARTITIONING_DISABLE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 0);

    // 0 : ���� ���Ե� Record ������ ���� Bucket ���� (=Partition ����) ���
    // 1 : Bucket ������ ������� �ʰ�, ������ ���� �״�� ���
    //     - /*+HASH BUCKET COUNT*/ ��Ʈ�� ������ Bucket ����
    //     - (��Ʈ�� ���� ���) QP Optimizer�� ������ Bucket ���� 
    IDP_DEF(UInt, "HASH_JOIN_MEM_TEMP_AUTO_BUCKET_COUNT_DISABLE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 0);

    // Partition ������ TLB�� �� ���� �� �� ���� ���, �� ���� ���� Fanout �ؾ� �Ѵ�.
    // TLB Entry�� Ŭ ����, �� ���� ���� Fanout �ϴ� ��찡 �پ���.
    // ���� ȯ���� TLB Entry ������ �Է��ϸ�, ������ ������� Fanout �ϰ� �ȴ�.
    IDP_DEF(UInt, "__HASH_JOIN_MEM_TEMP_TLB_ENTRY_COUNT",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            1, 1024, 64);

    // 0 : Single/Double Fanout�� TLB_ENTRY_COUNT�� ���� �����Ѵ�.
    // 1 : Single Fanout�� �����Ѵ�.
    // 2 : Double Fanout�� �����Ѵ�.
    IDP_DEF(UInt, "__FORCE_HASH_JOIN_MEM_TEMP_FANOUT_MODE",
            IDP_ATTR_SL_ALL | IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 2, 0);

    /* PROJ-2554 */
    IDP_DEF( UInt, "ALLOC_SLOT_IN_CURRENT_PAGE",
             IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
             IDP_ATTR_IU_ANY |
             IDP_ATTR_MS_ANY |
             IDP_ATTR_LC_EXTERNAL |
             IDP_ATTR_RD_WRITABLE |
             IDP_ATTR_ML_JUSTONE  |
             IDP_ATTR_CK_CHECK,
             0, 1, 0 );

    // 0 return err
    // 1 skip free slot (default)
    // 2 force free slot
    IDP_DEF(UInt, "__REFINE_INVALID_SLOT",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 2, 1);

    // TASK-6445 use old sort (for Memory Sort Temp Table)
    // 0 : ���� ���� �˰��� (Timsort)
    // 1 : ���� ���� �˰��� (Quicksort)
    IDP_DEF(UInt, "__USE_OLD_SORT",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 0);

    // BUG-43258
    IDP_DEF(UInt, "__OPTIMIZER_INDEX_CLUSTERING_FACTOR_ADJ",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            1, 10000, 100);

    // BUG-41248 DBMS_SQL package
    IDP_DEF(UInt, "PSM_CURSOR_OPEN_LIMIT",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            1, 1024, 32);

    /*
     * Project 2595
     * Miscellanous trace log
     */
    IDP_DEF(String, "MISC_MSGLOG_FILE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_SK_PATH     |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, IDP_MAX_PROP_STRING_LEN, (SChar *)PRODUCT_PREFIX"altibase_misc.log");

    IDP_DEF(UInt, "MISC_MSGLOG_SIZE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, ID_UINT_MAX, (10 * 1024 * 1024));

    IDP_DEF(UInt, "MISC_MSGLOG_COUNT",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, ID_UINT_MAX, 10);

    IDP_DEF(UInt, "MISC_MSGLOG_FLAG",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, ID_UINT_MAX, 2);

    IDP_DEF(UInt, "TRC_ACCESS_PERMISSION",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 777, 644); /*BUG-45405*/

/* PROJ-2581 */
#if defined(ALTIBASE_FIT_CHECK)

    IDP_DEF(String, "FIT_MSGLOG_FILE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_SK_PATH     |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, IDP_MAX_PROP_STRING_LEN, (SChar *)PRODUCT_PREFIX"altibase_fit.log");

    IDP_DEF(UInt, "FIT_MSGLOG_SIZE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, ID_UINT_MAX, (10 * 1024 * 1024));

    IDP_DEF(UInt, "FIT_MSGLOG_COUNT",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, ID_UINT_MAX, 10);

#endif

#if defined(DEBUG)
    /*
     * BUG-42770
     * 1 - Enable CORE Dump on debug mode
     * 0 - Disable CORE Dump on debug mode
     * This property is only valid in debug mode
     */
    IDP_DEF(UInt, "__CORE_DUMP_ON_SIGNAL",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 1);
#else
    IDP_DEF(UInt, "__CORE_DUMP_ON_SIGNAL",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 0);
#endif

    /* PROJ-2465 Tablespace Alteration for Table */
    IDP_DEF( UInt, "DDL_MEM_USAGE_THRESHOLD",
             IDP_ATTR_SL_ALL |
             IDP_ATTR_SH_ALL |
             IDP_ATTR_IU_ANY |
             IDP_ATTR_MS_ANY |
             IDP_ATTR_LC_INTERNAL |
             IDP_ATTR_RD_WRITABLE |
             IDP_ATTR_ML_JUSTONE  |
             IDP_ATTR_CK_CHECK,
             50, 100, 100 );

    /* PROJ-2465 Tablespace Alteration for Table */
    IDP_DEF( UInt, "DDL_TBS_USAGE_THRESHOLD",
             IDP_ATTR_SL_ALL |
             IDP_ATTR_SH_ALL |
             IDP_ATTR_IU_ANY |
             IDP_ATTR_MS_ANY |
             IDP_ATTR_LC_INTERNAL |
             IDP_ATTR_RD_WRITABLE |
             IDP_ATTR_ML_JUSTONE  |
             IDP_ATTR_CK_CHECK,
             50, 100, 100);

    /* PROJ-2465 Tablespace Alteration for Table */
    IDP_DEF( UInt, "ANALYZE_USAGE_MIN_ROWCOUNT",
             IDP_ATTR_SL_ALL |
             IDP_ATTR_SH_ALL |
             IDP_ATTR_IU_ANY |
             IDP_ATTR_MS_ANY |
             IDP_ATTR_LC_INTERNAL |
             IDP_ATTR_RD_WRITABLE |
             IDP_ATTR_ML_JUSTONE  |
             IDP_ATTR_CK_CHECK,
             1000, ID_UINT_MAX, 1000 );
    
    /* PROJ-2613: Key Redistribution in MRDB Index
     * �� ���� 1�� ��쿡�� MRDB Index���� Index�� ������ ���� Ű ��й� ����� ����Ѵ�.
     * �� ���� 0�� ��쿡�� MRDB Index���� Index�� ������ ���� ���� Ű ��й� ����� ������� �ʴ´�. */
    IDP_DEF(SInt, "MEM_INDEX_KEY_REDISTRIBUTION",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 1);

    /* PROJ-2613: Key Redistribution in MRDB Index
     * Ű ��й谡 ����Ǳ� ���� �̿� ����� �ּ� ����� ����
     * ��)�� ������Ƽ�� 30�� ��� �̿� ��尡 30%�̻��� �� ������ ���� ��쿡�� Ű ��й踦 �����Ѵ�. */

    IDP_DEF(SInt, "MEM_INDEX_KEY_REDISTRIBUTION_STANDARD_RATE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            10, 90, 50);

    /* PROJ-2586 PSM Parameters and return without precision */
    /*
     * PSM�� parameter�� return�� precision�� PROJ-2586 ���� ������� ��� ����
     * 0 : PROJ-2586 ���� ���
     * 1 : PROJ-2586�� ����� ���( default )
     * 2 : �׽�Ʈ��, PROJ-2586 + precision�� ����Ͽ��� �����ϰ� default precision �����Ͽ� ����
     */
    IDP_DEF(UInt, "PSM_PARAM_AND_RETURN_WITHOUT_PRECISION_ENABLE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 2, 1);

    /*
     * PSM���� char type parameter�� return�� �ִ� precision�� �����Ѵ�.
     * BUG-46573 PSM default precision�� �����մϴ�.
     * default : 32000
     * maximum : 65534 
     * minimum : 1
     */
    IDP_DEF(UInt, "PSM_CHAR_DEFAULT_PRECISION",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            1, 65534, 32000);

    /*
     * PSM���� varchar type parameter�� return�� �ִ� precision�� �����Ѵ�.
     * BUG-46573 PSM default precision�� �����մϴ�.
     * default : 32000
     * maximum : 65534
     * minimum : 1
     */
    IDP_DEF(UInt, "PSM_VARCHAR_DEFAULT_PRECISION",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            1, 65534, 32000);

    /*
     * PSM���� nchar(UTF16) type parameter�� return�� �ִ� precision�� �����Ѵ�.
     * BUG-46573 PSM default precision�� �����մϴ�.
     * default : 16000
     *           = ( 32766[ manual�� ǥ��� nchar �ִ� precision ] *
     *               32767 [ char type�� parameter �� return�� default precision ] ) /
     *             65534 [ manual�� ǥ��� char �ִ� precision ]
     * maximum : 32766
     * minimum : 1
     */
    IDP_DEF(UInt, "PSM_NCHAR_UTF16_DEFAULT_PRECISION",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            1, 32766, 16000);

    /*
     * PSM���� nvarchar(UTF16) type parameter�� return�� �ִ� precision�� �����Ѵ�.
     * BUG-46573 PSM default precision�� �����մϴ�.
     * default : 16000
     *           = ( 32766[ manual�� ǥ��� nchar �ִ� precision ] *
     *               32767 [ char type�� parameter �� return�� default precision ] ) /
     *             65534 [ manual�� ǥ��� char �ִ� precision ]
     * maximum : 32766
     * minimum : 1
     */
    IDP_DEF(UInt, "PSM_NVARCHAR_UTF16_DEFAULT_PRECISION",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            1, 32766, 16000);

    /*
     * PSM���� nchar(UTF8) type parameter�� return�� �ִ� precision�� �����Ѵ�.
     * BUG-46573 PSM default precision�� �����մϴ�.
     * default : 10666
     *           = ( 21843[ manual�� ǥ��� nchar �ִ� precision ] *
     *               32767 [ char type�� parameter �� return�� default precision ] ) /
     *             65534 [ manual�� ǥ��� char �ִ� precision ]
     * maximum : 21843 
     * minimum : 1
     */
    IDP_DEF(UInt, "PSM_NCHAR_UTF8_DEFAULT_PRECISION",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            1, 21843, 10666);

    /*
     * PSM���� nvarchar(UTF8) type parameter�� return�� �ִ� precision�� �����Ѵ�.
     * BUG-46573 PSM default precision�� �����մϴ�.
     * default : 10666
     *           = ( 21843[ manual�� ǥ��� nchar �ִ� precision ] *
     *               32767 [ char type�� parameter �� return�� default precision ] ) /
     *             65534 [ manual�� ǥ��� char �ִ� precision ]
     * maximum : 21843 
     * minimum : 1
     */
    IDP_DEF(UInt, "PSM_NVARCHAR_UTF8_DEFAULT_PRECISION",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            1, 21843, 10666);

    // BUG-42322
    IDP_DEF(ULong, "__PSM_FORMAT_CALL_STACK_OID",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 9999, 0);
    
    /* PROJ-2617 */
    IDP_DEF(UInt, "__FAULT_TOLERANCE_ENABLE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 1);

    IDP_DEF(UInt, "__FAULT_TOLERANCE_TRACE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 0);

    /* BUG-34112 */
    IDP_DEF(UInt, "__FORCE_AUTONOMOUS_TRANSACTION_PRAGMA",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 0);

    IDP_DEF(UInt, "__AUTONOMOUS_TRANSACTION_PRAGMA_DISABLE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 0 );

    IDP_DEF(UInt, "__PSM_STATEMENT_LIST_COUNT",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 4096, 128);

    IDP_DEF(UInt, "__PSM_STATEMENT_POOL_COUNT",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            32, 4096, 128);

    // BUG-43443 temp table�� ���ؼ� work area size�� estimate�ϴ� ����� off
    IDP_DEF(UInt, "__DISK_TEMP_SIZE_ESTIMATE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 1);

    // BUG-43421
    IDP_DEF(UInt, "__OPTIMIZER_SEMI_JOIN_TRANSITIVITY_DISABLE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 1);

    /* BUG-43495 */
    IDP_DEF(UInt, "__OPTIMIZER_LIKE_INDEX_SCAN_WITH_OUTER_COLUMN_DISABLE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 0);

    // lock node cache enable
    // 0 : disable
    // 1 : list method with count
    // 2 : bitmap method with fixed count - 64
    IDP_DEF(UInt, "LOCK_MGR_CACHE_NODE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 2, 1);
    /* BUG-43408 LockNodeCache Count  */
    IDP_DEF(UInt, "LOCK_NODE_CACHE_COUNT",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1024, 2);
    /* PROJ-2734 */
    IDP_DEF(UInt, "DISTRIBUTION_DEADLOCK_ENABLE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 1);
    IDP_DEF(ULong, "DISTRIBUTION_DEADLOCK_RISK_LOW_WAIT_TIME", /* microseconds */
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, ID_ULONG_MAX, 1000000);
    IDP_DEF(ULong, "DISTRIBUTION_DEADLOCK_RISK_MID_WAIT_TIME", /* microseconds */
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, ID_ULONG_MAX, 1000000);
    IDP_DEF(ULong, "DISTRIBUTION_DEADLOCK_RISK_HIGH_WAIT_TIME", /* microseconds */
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, ID_ULONG_MAX, 0);
    /* PROJ-2733 �л� Ʈ����� ���ռ� */
    IDP_DEF(ULong, "VERSIONING_MIN_TIME", /* milliseconds */
            IDP_ATTR_SL_ALL  |
            IDP_ATTR_SH_USER | 
            IDP_ATTR_IU_ANY  |
            IDP_ATTR_MS_ANY  |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 100000, 0);

    /* 0     : ���Ѵ��
       INDOUBT_FETCH_TIMEOUT �� �����Ϸ��� ?
          => GLOBAL_TRANSACTION_LEVEL�� 3�� �ƴ� ������ �����Ѵ�. */
    IDP_DEF(UInt, "INDOUBT_FETCH_TIMEOUT", /* seconds */
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL | 
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            IDP_SHARD_PROPERTY_INDOUBT_FETCH_TIMEOUT_MIN, 
            IDP_SHARD_PROPERTY_INDOUBT_FETCH_TIMEOUT_MAX, 
            IDP_SHARD_PROPERTY_INDOUBT_FETCH_TIMEOUT_DEFAULT );

    /* INDOUBT_FETCH_TIMEOUT �� �߻��� ������ ���� 
        0 : skip  
        1 : ���ܹ߻�                    */
    IDP_DEF(UInt, "INDOUBT_FETCH_METHOD", 
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL | 
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            IDP_SHARD_PROPERTY_INDOUBT_FETCH_METHOD_MIN, 
            IDP_SHARD_PROPERTY_INDOUBT_FETCH_METHOD_MAX, 
            IDP_SHARD_PROPERTY_INDOUBT_FETCH_METHOD_DEFAULT );

    /* BUG-43737
     * 1 : SYS_TBS_MEM_DATA
     * 2 : SYS_TBS_DISK_DATA */
    IDP_DEF(UInt, "__FORCE_TABLESPACE_DEFAULT",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            1, 2, 1);

    /* PROJ-2626 Snapshot Export */
    IDP_DEF(UInt, "SNAPSHOT_MEM_THRESHOLD",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 100, 80 );

    /* PROJ-2626 Snapshot Export */
    IDP_DEF(UInt, "SNAPSHOT_DISK_UNDO_THRESHOLD",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 100, 80 );

    /* PROJ-2624 ACCESS_LIST */
    IDP_DEF(String, "ACCESS_LIST_FILE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_SK_PATH     |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, IDP_MAX_PROP_STRING_LEN, (SChar *)"");

    /* TASK-6744 */
    IDP_DEF(UInt, "__PLAN_RANDOM_SEED",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, ID_UINT_MAX, 0);

    /* BUG-44499 */
    IDP_DEF(UInt, "__OPTIMIZER_BUCKET_COUNT_MIN",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            1024, 10240000, 1024);

    /* BUG-48161 */
    IDP_DEF(UInt, "__OPTIMIZER_BUCKET_COUNT_MAX",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            1024, 102400000, 102400000);

    /* BUG-43463 scanlist �� lock�� ����� ����,
     * Memory FullScan �ÿ� moveNext,Prev���� lock�� ���� �ʴ´�.
     * link, unlink�� �������(Lock�� ����)*/
    IDP_DEF(UInt, "__SCANLIST_MOVE_NONBLOCK",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 1);

    /* PROJ-2641 Hierarchy Query Index */
    IDP_DEF(UInt, "__OPTIMIZER_HIERARCHY_TRANSFORMATION",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 0 );

    // BUG-44692
    IDP_DEF(UInt, "BUG_44692",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 1 );

    /* BUG-44794 �ε��� ����� �ε��� ��� ������ �������� �ʴ� ���� ������Ƽ �߰� */
    IDP_DEF(UInt, "__GATHER_INDEX_STAT_ON_DDL",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 0 /* 651: 1(AS-IS), trunk: 0 */ );

    // BUG-44795
    IDP_DEF(UInt, "__OPTIMIZER_DBMS_STAT_POLICY",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 0 );

    /* BUG-44850 Index NL , Inverse index NL ���� ����ȭ ����� ����� �����ϸ� primary key�� �켱������ ����.
         0 : primary key �켱������ ���� (BUG-44850) +
             Inverse index NL ������ �� index cost�� ���� �ε��� ���� (BUG-45169)
         1 : ���� �÷� ��İ� ����
         2 : primary key �켱������ ���� (BUG-44850) +
             Inverse index NL ������ �� index cost�� ���� �ε��� ���� (BUG-45169) +
             index NL ���� �϶� index cost�� ���� �ε��� ���� + ( BUG-48327 )
             Anti ���� �϶� index cost�� ���� �ε��� ����
    */
    IDP_DEF(UInt, "__OPTIMIZER_INDEX_NL_JOIN_ACCESS_METHOD_POLICY",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 2, 2 );

    /* BUG-45172 
         0 : �������� ���� ( ������ ���� )
         1 : ������ */
    IDP_DEF(UInt, "__OPTIMIZER_SEMI_JOIN_REMOVE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 1 );

    // BUG-46154
    IDP_DEF(UInt, "__PRINT_OUT_ENABLE",
            IDP_ATTR_SL_ALL | 
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 1);
    
    /* PROJ-2681 */
#if defined(ALTI_CFG_OS_LINUX)
    IDP_DEF(UInt, "IB_ENABLE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 0);
#else /* ALTI_CFG_OS_LINUX */
    IDP_DEF(UInt, "IB_ENABLE",
            IDP_ATTR_SL_ALL | 
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 0, 0);
#endif /* ALTI_CFG_OS_LINUX */

    IDP_DEF(UInt, "IB_PORT_NO",
            IDP_ATTR_SL_ALL | 
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            1024,   /* min */
            65535,  /* max */
            20300); /* default */

    IDP_DEF(UInt, "IB_MAX_LISTEN",
            IDP_ATTR_SL_ALL | 
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1024, 128);

    IDP_DEF(UInt, "IB_LISTENER_DISABLE",
            IDP_ATTR_SL_ALL | 
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 0);

    IDP_DEF(UInt, "IB_LATENCY",
            IDP_ATTR_SL_ALL | 
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 0);

    IDP_DEF(UInt, "IB_CONCHKSPIN",
            IDP_ATTR_SL_ALL | 
IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 2147483, 0);

    /* BUG-46267 */
    IDP_DEF( UInt, "NUMBER_CONVERSION_MODE",
             IDP_ATTR_SL_ALL | 
             IDP_ATTR_SH_ALL |
             IDP_ATTR_IU_ANY |
             IDP_ATTR_MS_ANY |
             IDP_ATTR_LC_INTERNAL |
             IDP_ATTR_RD_READONLY |
             IDP_ATTR_ML_JUSTONE  |
             IDP_ATTR_CK_CHECK,
             0, 1, 1 );

    /* Subquery Unnest�� ���� �������� ȣȯ ������Ƽ
        0   : ���� �߰� �Ǵ� ����� SU�� ���� ����� ��� ����ȴ�.
              OPTIMIZER_UNNEST_SUBQUERY = 0 �� �� UNNEST hint �켱(BUG-46544)
               + �θ��Ͽ� left outer join�� �ְ� 
                 ���������� ���̺��� 1���� ���� ��� no unnest (BUG-48336)
        1   : BUG-46544 ���� ���� ���ϵ���
              UNNEST hint���� "OPTIMIZER_UNNEST_SUBQUERY" Property �켱 ����
        2   : BUG-48336 ���� ���� ���� unnest
        
       ex) 3 = 1+2 ( BUG-46544, BUG-48336 ���� )
    */
    IDP_DEF( UInt, "__OPTIMIZER_UNNEST_COMPATIBILITY",
             IDP_ATTR_SL_ALL | 
             IDP_ATTR_SH_ALL |
             IDP_ATTR_IU_ANY |
             IDP_ATTR_MS_ANY |
             IDP_ATTR_LC_INTERNAL |
             IDP_ATTR_RD_WRITABLE |
             IDP_ATTR_ML_JUSTONE  |
             IDP_ATTR_CK_CHECK,
             0, 3, 2 );

    /* PROJ-2632 */
    IDP_DEF( UInt, "SERIAL_EXECUTE_MODE",
             IDP_ATTR_SL_ALL | 
             IDP_ATTR_SH_ALL |
             IDP_ATTR_IU_ANY |
             IDP_ATTR_MS_ANY |
             IDP_ATTR_LC_EXTERNAL |
             IDP_ATTR_RD_WRITABLE |
             IDP_ATTR_ML_JUSTONE  |
             IDP_ATTR_CK_CHECK,
             0, 1, 1 );

    /* PROJ-2632 */
    IDP_DEF( UInt, "TRCLOG_DETAIL_INFORMATION",
             IDP_ATTR_SL_ALL |
             IDP_ATTR_SH_ALL |
             IDP_ATTR_IU_ANY |
             IDP_ATTR_MS_ANY |
             IDP_ATTR_LC_INTERNAL |
             IDP_ATTR_RD_WRITABLE |
             IDP_ATTR_ML_JUSTONE  |
             IDP_ATTR_CK_CHECK,
             0, 1, 0 );

    IDP_DEF( ULong, "MATHEMATICS_TEMP_MEMORY_MAXIMUM",
             IDP_ATTR_SL_ALL |
             IDP_ATTR_SH_ALL |
             IDP_ATTR_IU_ANY |
             IDP_ATTR_MS_ANY |
             IDP_ATTR_LC_EXTERNAL |
             IDP_ATTR_RD_WRITABLE |
             IDP_ATTR_ML_JUSTONE  |
             IDP_ATTR_CK_CHECK,
             0, ID_ULONG_MAX, 0 );

    /* BUG-46932 */
    IDP_DEF( UInt, "__OPTIMIZER_INVERSE_JOIN_ENABLE",
             IDP_ATTR_SL_ALL |
             IDP_ATTR_SH_ALL |
             IDP_ATTR_IU_ANY |
             IDP_ATTR_MS_ANY |
             IDP_ATTR_LC_INTERNAL |
             IDP_ATTR_RD_WRITABLE |
             IDP_ATTR_ML_JUSTONE  |
             IDP_ATTR_CK_CHECK,
             0, 1, 1 );

    /* BUG-47648  disk partition���� ���Ǵ� prepared memory ��뷮 ���� */
    IDP_DEF( UInt, "__REDUCE_PARTITION_PREPARE_MEMORY",
             IDP_ATTR_SL_ALL |
             IDP_ATTR_SH_ALL |
             IDP_ATTR_IU_ANY |
             IDP_ATTR_MS_ANY |
             IDP_ATTR_LC_INTERNAL |
             IDP_ATTR_RD_WRITABLE |
             IDP_ATTR_ML_JUSTONE  |
             IDP_ATTR_CK_CHECK,
             0, 3, 1 );

    IDP_DEF(UInt, "SHARED_TRANS_HASH_BUCKET_COUNT",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_USER|
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            IDP_SHARD_PROPERTY_SHARE_TRANS_HASH_BUCKET_COUNT_MIN,
            IDP_SHARD_PROPERTY_SHARE_TRANS_HASH_BUCKET_COUNT_MAX,
            IDP_SHARD_PROPERTY_SHARE_TRANS_HASH_BUCKET_COUNT_DEFAULT);

    /* TASK-7219 */
    IDP_DEF( UInt, "SHARD_TRANSFORM_MODE",
             IDP_ATTR_SL_ALL |
             IDP_ATTR_SH_ALL |
             IDP_ATTR_IU_ANY |
             IDP_ATTR_MS_ANY |
             IDP_ATTR_LC_EXTERNAL |
             IDP_ATTR_RD_WRITABLE |
             IDP_ATTR_ML_JUSTONE  |
             IDP_ATTR_CK_CHECK,
             0, 15, 15 );

    /* BUG-47986
     * Enable : 1
     * Disable : 0
     */
    IDP_DEF( UInt, "__OPTIMIZER_OR_VALUE_INDEX",
             IDP_ATTR_SL_ALL |
             IDP_ATTR_SH_ALL |
             IDP_ATTR_IU_ANY |
             IDP_ATTR_MS_ANY |
             IDP_ATTR_LC_INTERNAL |
             IDP_ATTR_RD_WRITABLE |
             IDP_ATTR_ML_JUSTONE  |
             IDP_ATTR_CK_CHECK,
             0, 1, 1 );

    IDP_DEF(UInt, "SHARD_STATEMENT_RETRY",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL|
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_EXTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            IDP_SHARD_PROPERTY_SHARD_STATEMENT_RETRY_MIN,
            IDP_SHARD_PROPERTY_SHARD_STATEMENT_RETRY_MAX,
            IDP_SHARD_PROPERTY_SHARD_STATEMENT_RETRY_DEFAULT);
    
    IDP_DEF(UInt, "__SHARD_ZOOKEEPER_TEST",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 1 );

    /* BUG-48132
     * grouping �÷����� SORT �Ǵ� HASH �޼ҵ� �����ϴ� ������Ƽ
     *   GROUP_HASH  : 1
     *   GROUP_SORT  : 2
     *   default     : 0  GROUP_HASH + GROUP_SORT ��� ���,
     *                    ������ ����(optimizer�� ���� ����) */
    IDP_DEF( UInt, "__OPTIMIZER_PLAN_HASH_OR_SORT_METHOD",
             IDP_ATTR_SL_ALL |
             IDP_ATTR_SH_ALL |
             IDP_ATTR_IU_ANY |
             IDP_ATTR_MS_ANY |
             IDP_ATTR_LC_INTERNAL |
             IDP_ATTR_RD_WRITABLE |
             IDP_ATTR_ML_JUSTONE  |
             IDP_ATTR_CK_CHECK,
             0, 2, 0);

    /* BUG-48135 NL Join Penalty ���� �����Ҽ� �ִ� property �߰� */
    IDP_DEF( UInt, "__OPTIMIZER_INDEX_NL_JOIN_PENALTY",
             IDP_ATTR_SL_ALL |
             IDP_ATTR_SH_ALL |
             IDP_ATTR_IU_ANY |
             IDP_ATTR_MS_ANY |
             IDP_ATTR_LC_INTERNAL |
             IDP_ATTR_RD_WRITABLE |
             IDP_ATTR_ML_JUSTONE  |
             IDP_ATTR_CK_CHECK,
             0, 20, 6);

    // BUG-48120 
    // scan cost ������ �� ��� ���
    // 0 : ���� ���� ���� ����
    // 1 : BUG-48120 ���� ( default )
    //      ALL EQUAL INDEX�� ��� cost ��꿡 predicate NDV�� ���
    //      selection graph�� output record count�� ����� ���� index NDV �� ���
    //      ACCESS METHOD�� ������ ��
    //      cost�� ������ �ε����� �ִ� ��� ALL EQUAL INDEX�� �����ϵ��� �Ѵ�.
    IDP_DEF( UInt, "__OPTIMIZER_INDEX_COST_MODE",
             IDP_ATTR_SL_ALL |
             IDP_ATTR_SH_ALL |
             IDP_ATTR_IU_ANY |
             IDP_ATTR_MS_ANY |
             IDP_ATTR_LC_INTERNAL |
             IDP_ATTR_RD_WRITABLE |
             IDP_ATTR_ML_JUSTONE  |
             IDP_ATTR_CK_CHECK,
             0, 1, 1);

    /* BUG-48594 */
    IDP_DEF(UInt, "__SQL_PLAN_CACHE_VALID_MODE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_READONLY |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 0 );

    /* BUG-48776 */
    IDP_DEF(UInt, "__SUBQUERY_MODE",
            IDP_ATTR_SL_ALL |
            IDP_ATTR_SH_ALL |
            IDP_ATTR_IU_ANY |
            IDP_ATTR_MS_ANY |
            IDP_ATTR_LC_INTERNAL |
            IDP_ATTR_RD_WRITABLE |
            IDP_ATTR_ML_JUSTONE  |
            IDP_ATTR_CK_CHECK,
            0, 1, 1 );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

