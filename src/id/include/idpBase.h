/***********************************************************************
 * Copyright 1999-2000, RTBase Corporation or its subsidiaries.
 * All rights reserved.
 **********************************************************************/

/***********************************************************************
 * $Id: idpBase.h 87064 2020-03-30 04:21:12Z jake.jang $
 * Description:
 * ������ idpBase.cpp ����.
 * 
 **********************************************************************/

#ifndef _O_IDP_TYPE_H_
# define _O_IDP_TYPE_H_ 1

# include <idl.h>

/*
 * for user or internal only
 * LC_EXTERNAL : �����ϴ� property. X$PROPERTY �� V$PROPERTY ���� Ȯ�� ����.
 * LC_INTERNAL : ����ڿ��Դ� �������� �ʴ� property. X$PROPERTY ������ Ȯ�� ����.
 */
#define    IDP_ATTR_LC_MASK        0x00000001
#define    IDP_ATTR_LC_EXTERNAL    0x00000000 /* default : internal */
#define    IDP_ATTR_LC_INTERNAL    0x00000001

/*
 * read/write attributes
 * RD_WRITABLE : alter system ���� ���� ������ property
 * RD_READONLY : alter system ���� ���� �Ұ����� property
 */
#define    IDP_ATTR_RD_MASK        0x00000002
#define    IDP_ATTR_RD_WRITABLE    0x00000000 /* default : writable */
#define    IDP_ATTR_RD_READONLY    0x00000002

// PROJ-2727
// USER Connection ����� Coordination Connection ���� �Ǵ� property��
// �����Ѵ�.
// ALTER SESSION / SYSTEM ���� �����ϴ� Property�� user, coordination conn ����
// shardcli, odbccli�� ���� �����ϴ� ��� user, coordination, library conn ����
#define    IDP_ATTR_SH_MASK  0x000000F0
#define    IDP_ATTR_SH_ALL   0x00000000 // default : User, Coordination Conn
#define    IDP_ATTR_SH_USER  0x00000010 // User Conn
#define    IDP_ATTR_SH_COORD 0x00000020 // Coordination Conn  
#define    IDP_ATTR_SH_LIB   0x00000030 // Library Conn

/*
 * Set Location flag
 * ���� RAC ������Ʈ�� �� ��, DB file �� DB instance �� 1:1 �� �������� ���� ��츦
 * �����Ͽ� SL_PFILE �� SL_SPFILE �� ������.
 * �׷��� RAC ������Ʈ�� drop �Ǹ鼭 �ǹ� ������.
 */
#define    IDP_ATTR_SL_MASK        0x07000000
#define    IDP_ATTR_SL_ALL         (IDP_ATTR_SL_PFILE |  \
                                    IDP_ATTR_SL_SPFILE | \
                                    IDP_ATTR_SL_ENV)        /*�⺻ ������, ENV/PFILE/SPFILE ��ο��� ��������*/
#define    IDP_ATTR_SL_PFILE       0x01000000               /* PFILE�� ���� ������  */
#define    IDP_ATTR_SL_SPFILE      0x02000000               /* SPFILE�� ���� ������ */
#define    IDP_ATTR_SL_ENV         0x04000000               /* ȯ�溯���� ���������� */

/* Must Shared flag */
#define    IDP_ATTR_MS_MASK        0x08000000 
#define    IDP_ATTR_MS_ANY         0x00000000 /* �⺻ ������, Cluster���� �������� �ʾƵ� ������� ������Ƽ�� ��Ÿ�� */
#define    IDP_ATTR_MS_SHARE       0x08000000 /* Cluster���� �������� ������ �ȵǴ� ������Ƽ�� ��Ÿ�� */

/* Identical/Unique flag */
#define    IDP_ATTR_IU_MASK        0x0000000C /* Identical/Unique ����ũ */ 
#define    IDP_ATTR_IU_ANY         0x00000000 /* �⺻ ������, Cluster���� ��� ���� ������ ������� ������Ƽ�� ��Ÿ�� */
#define    IDP_ATTR_IU_UNIQUE      0x00000004 /* Cluster������ �����ؾ��Ѵٴ� ���� ��Ÿ�� */
#define    IDP_ATTR_IU_IDENTICAL   0x00000008 /* Cluster���� ��� ����� ���� �����ؾ��Ѵٴ� ���� ��Ÿ�� */

/* multiple attri */
#define    IDP_ATTR_ML_MASK        0x00000F00
#define    IDP_ATTR_ML_JUSTONE     0x00000000 /* default : only one set */
#define    IDP_ATTR_ML_MULTIPLE    0x00000100 /* ������ ������ �� �� ���� */
#define    IDP_ATTR_ML_EXACT_2     0x00000200 // 2 ���� ����  
#define    IDP_ATTR_ML_EXACT_3     0x00000300 // 3 ���� ����  
#define    IDP_ATTR_ML_EXACT_4     0x00000400 // 4 ���� ����  
#define    IDP_ATTR_ML_EXACT_5     0x00000500 // 5 ���� ����  
#define    IDP_ATTR_ML_EXACT_6     0x00000600 // 6 ���� ����  
#define    IDP_ATTR_ML_EXACT_7     0x00000700 // 7 ���� ����  
#define    IDP_ATTR_ML_EXACT_8     0x00000800 // 8 ���� ����  
#define    IDP_ATTR_ML_EXACT_9     0x00000900 // 9 ���� ����  
#define    IDP_ATTR_ML_EXACT_10    0x00000a00 // 10���� ����  
#define    IDP_ATTR_ML_EXACT_11    0x00000b00 // 11���� ����  
#define    IDP_ATTR_ML_EXACT_12    0x00000c00 // 12���� ����  
#define    IDP_ATTR_ML_EXACT_13    0x00000d00 // 13���� ����  
#define    IDP_ATTR_ML_EXACT_14    0x00000e00 // 14���� ����  
#define    IDP_ATTR_ML_EXACT_15    0x00000f00 // 15���� ����  

#define    IDP_ATTR_ML_COUNT(a)   ((a & IDP_ATTR_ML_MASK) >> 8)

/* range check attri */
#define    IDP_ATTR_CK_MASK        0x0000F000
#define    IDP_ATTR_CK_CHECK       0x00000000 /* default : check range */
#define    IDP_ATTR_CK_NOCHECK     0x00001000

/* data should be specified by user. no default!!!*/
#define    IDP_ATTR_DF_MASK         0x000F0000
#define    IDP_ATTR_DF_USE_DEFAULT  0x00000000 /* default : use default value */
#define    IDP_ATTR_DF_DROP_DEFAULT 0x00010000 /* don't use default value, without value, it's error */

/* String Kind Mask */
#define    IDP_ATTR_SK_MASK           0x00F00000
#define    IDP_ATTR_SK_PATH           0x00000000 /* Directory or File Path*/
#define    IDP_ATTR_SK_ALNUM          0x00100000 /* alphanumeric(A~Z, a~z, 0~9)�� ���� ���� ���ڵ�ε� �̸�*/
#define    IDP_ATTR_SK_ASCII          0x00200000 /* ASCII ���� ���� ���ڵ�ε� �̸�*/
#define    IDP_ATTR_SK_MULTI_BYTE     0x00300000 /* PROJ-2208 Multi Byte Type */

/* type of prop. */
#define    IDP_ATTR_TP_MASK        0xF0000000 /* not default allowed */
#define    IDP_ATTR_TP_UInt        0x10000000
#define    IDP_ATTR_TP_SInt        0x20000000
#define    IDP_ATTR_TP_ULong       0x30000000
#define    IDP_ATTR_TP_SLong       0x40000000
#define    IDP_ATTR_TP_String      0x50000000
#define    IDP_ATTR_TP_Directory   0x60000000
#define    IDP_ATTR_TP_Special     0x70000000

/* for align value
 * ������Ƽ�� ���� ���� align�� ���߾� �ִ� ��ũ���̸�, 
 * ���� 8����Ʈ�� ����Ͽ� ������ �� �ִ�.
 * �� ��ũ�θ� ����ϱ� ���ؼ��� IDP_ATTR_AL_SET_VALUE�� �̿��Ͽ�
 * ���� 8����Ʈ�� align �ϰ����ϴ� ���� ���� ������ �� �ִ�. 
 * ���� Ÿ���� ������Ƽ��(ULong,SLong,UInt,SInt) ���ؼ��� �����ȴ�.
 */
#define    IDP_ATTR_AL_MASK             ID_ULONG(0xFFFFFFFF00000000)
#define    IDP_ATTR_AL_SET_VALUE(val)   ((idpAttr)(val) << ID_ULONG(32))
#define    IDP_ATTR_AL_GET_VALUE(val)   ((val & IDP_ATTR_AL_MASK) >> 32)
#define    IDP_IS_ALIGN(val)            ( IDP_ATTR_AL_GET_VALUE(mAttr) > 1)
#define    IDP_ALIGN(val, al)           (((  (val) + (al) - 1 ) / (al)) * (al) )
#define    IDP_TYPE_ALIGN(__type__, __value__)\
    if ( IDP_IS_ALIGN(mAttr) )\
    {\
        __type__     sAlign;\
        sAlign  = (__type__)IDP_ATTR_AL_GET_VALUE(mAttr);\
        __value__ = IDP_ALIGN(__value__, sAlign);\
    }

#define    IDP_MAX_VALUE_LEN            (256)
#define    IDP_MAX_VALUE_COUNT          (128)
#define    IDP_MAX_SID_LEN              (IDP_MAX_VALUE_LEN) /*256*/
#define    IDP_FIXED_TBL_VALUE_COUNT    (8)


struct idvSQL;

typedef    IDE_RC (*idpChangeCallback)( idvSQL * aStatistics,
                                        SChar  * aName,
                                        void   * aOldValue,
                                        void   * aNewValue,
                                        void   * aArg );

typedef ULong idpAttr;

typedef enum idpValueSource
{
    IDP_VALUE_FROM_DEFAULT              = 0,
    IDP_VALUE_FROM_PFILE                = 1,
    IDP_VALUE_FROM_ENV                  = 2,
    IDP_VALUE_FROM_SPFILE_BY_ASTERISK   = 3,
    IDP_VALUE_FROM_SPFILE_BY_SID        = 4,
    IDP_MAX_VALUE_SOURCE_COUNT          = 5
} idpValueSource;

typedef struct 
{
    UInt mCount;
    void* mVal[IDP_MAX_VALUE_COUNT];
} idpValArray;

/*
 * Fixed Table�� ����� ���� ����.
 */
typedef struct idpBaseInfo
{
    SChar*       mSID;    
    SChar*       mName;
    UInt         mMemValCount;     
    idpAttr      mAttr;
    void *       mMin;
    void *       mMax;
    void*        mMemVal[IDP_FIXED_TBL_VALUE_COUNT];
    /*�Ʒ��� �� Source �� ����� ���� ��Ÿ���� �ʵ���*/
    UInt         mDefaultCount;
    void*        mDefaultVal[IDP_FIXED_TBL_VALUE_COUNT];
    UInt         mEnvCount;
    void*        mEnvVal[IDP_FIXED_TBL_VALUE_COUNT];    
    UInt         mPFileCount;    
    void*        mPFileVal[IDP_FIXED_TBL_VALUE_COUNT];
    UInt         mSPFileByAsteriskCount;
    void*        mSPFileByAsteriskVal[IDP_FIXED_TBL_VALUE_COUNT];
    UInt         mSPFileBySIDCount;
    void*        mSPFileBySIDVal[IDP_FIXED_TBL_VALUE_COUNT];

    void *       mBase; // convert ������ ���� �ڽ��� Base�� ���� �����Ͱ� �ʿ���
}idpBaseInfo;

/*
 * ������ idpBase�� virtual function���� �����Ǿ��ִ� �Լ�����
 * �Լ������͹������ �����Ͽ���
 * Ŭ���̾�Ʈ ���̺귯���� C++ dependency�� ���ֱ� ������: BUG-11362
 */
typedef struct idpVirtualFunction
{
    UInt   (*mGetSize)(void *, void *aSrc);
    SInt   (*mCompare)(void *, void *aVal1, void *aVal2);
    IDE_RC (*mValidateRange)(void* aObj, void *aVal);
    IDE_RC (*mConvertFromString)(void *, void *aString, void **aResult);
    UInt   (*mConvertToString)(void *, void *aSrcMem, void *aDestMem, UInt aDestSize);
    IDE_RC (*mClone)(void* aObj, SChar* aSID, void** aCloneObj);
    void   (*mCloneValue)(void* aObj, void* aSrc, void** aDst);
} idpVirtualFunction;

/* ------------------------------------------------
 *  idpArgument Implementation
 * ----------------------------------------------*/
/* ------------------------------------------------
 *  Argument Passing for Property : BUG-12719
 *  How to Added.. 
 *
 *  1. add  ArgumentID  to  idpBase.h => enum idpArgumentID
 *  2. add  Argument Object ot mmuProperty.h => struct mmuPropertyArgument
 *  3. add  switch/case in mmuProperty.cpp => callbackForGetingArgument()
 *
 *  Example property : check this => mmuProperty::callbackAdminMode()
 * ----------------------------------------------*/

typedef enum idpArgumentID
{
    IDP_ARG_USERID = 0,
    IDP_ARG_TRANSID,

    IDP_ARG_MAX
}idpArgumentID;

typedef struct idpArgument
{
    void *(*getArgValue)(idvSQL *aStatistics,
                         idpArgument  *aArg,
                         idpArgumentID aID);
}idpArgument;

class idpBase
{
protected:
    idpVirtualFunction *mVirtFunc;

// protected���� fixed table�� ������ ���� public�� ������.
// protected�� �����ϰ�, ������ �� �ִ� ����� �ֳ�?
public: 
    static SChar       *mErrorBuf;    // pointing to idp::mErroBuf()

    // To Fix BUG-18324 alter system set LOGICAL_AGER_COUNT���� �ϴµ���
    //                  client ������ �ȵ�
    //
    // Property�� �б�/������ ���ü� ��� ���� Mutex
    PDL_thread_mutex_t  mMutex;
    
    idpChangeCallback mUpdateBefore; // ������Ƽ�� ����� �� ���� ���� ȣ���.
    idpChangeCallback mUpdateAfter;  // ������Ƽ�� ����� ���Ŀ� ȣ���.
    
    IDE_RC checkRange(void * aValue);
    
    static IDE_RC defaultChangeCallback(
        idvSQL *, SChar *aName, void *, void *, void *);
    
    /*�ϳ��� ������Ƽ�� ���� data ����
     *�Ʒ� �����鿡 ���� ������ fixed table�� ��ȯ�Ѵ�. 
     */
    SChar         mSID[IDP_MAX_SID_LEN];
    SChar        *mName;
    idpAttr       mAttr;
    void         *mMin;
    void         *mMax;
    idpValArray   mMemVal; //local Instance ��߿� ���Ǵ� Value List
    idpValArray   mSrcValArr[IDP_MAX_VALUE_SOURCE_COUNT];//Source�� ���� �о���� ������Ƽ ��

public:

    idpBase();
    virtual ~idpBase();

    static void initializeStatic(SChar *aErrorBuf);
    static void destroyStatic()         {}

    SChar*  getName() { return mName; }
    SChar*  getSID()  { return mSID; }
    idpAttr getAttr() { return mAttr; }
    UInt    getValCnt() { return mMemVal.mCount; }

    void    setSID(SChar* aSID)
    {
        idlOS::strncpy(mSID, aSID, IDP_MAX_SID_LEN);
    }

    IDE_RC insertMemoryRawValue(void *aValue);
    IDE_RC insertBySrc(SChar *aValue, idpValueSource aSrc);
    IDE_RC insertRawBySrc(void *aValue, idpValueSource aSrc);    
    
    IDE_RC read    (void  *aOut, UInt aNum);
    IDE_RC readBySrc (void *aOut, idpValueSource aSrc, UInt aNum);
    IDE_RC readPtr (void **aOut, UInt aNum); // ReadOnly &&  String Ÿ���� ��쿡��
    IDE_RC readPtrBySrc (UInt aNum, idpValueSource aSrc, void **aOut);

    IDE_RC readPtr4Internal(UInt aNum, void **aOut);
    
    // BUG-43533 OPTIMIZER_FEATURE_ENABLE
    IDE_RC update4Startup(idvSQL *aStatistics, SChar *aIn,  UInt aNum, void *aArg);

    IDE_RC update(idvSQL *aStatistics, SChar *aIn,  UInt aNum, void *aArg);
    IDE_RC updateForce(SChar *aIn,  UInt aNum, void *aArg);

    void   setupBeforeUpdateCallback(idpChangeCallback mCallback);
    void   setupAfterUpdateCallback(idpChangeCallback mCallback);
    IDE_RC verifyInsertedValues();
    IDE_RC validate(SChar *aIn, idBool aIsSystem );
    // PROJ-2727
    IDE_RC getPropertyAttribute( UInt * aOut );
    
    void   registCallback();
    void   outAttribute(FILE *aFP, idpAttr aAttr);
    idBool allowDefault()
    { 
        return (((mAttr & IDP_ATTR_DF_MASK) == IDP_ATTR_DF_USE_DEFAULT) 
                ? ID_TRUE : ID_FALSE);  
    }
    idBool isMustShare()
    {
        return (((mAttr & IDP_ATTR_MS_MASK) == IDP_ATTR_MS_SHARE) 
                ? ID_TRUE : ID_FALSE);
    }
    idBool existSPFileValBySID()
    {
        return ((mSrcValArr[IDP_VALUE_FROM_SPFILE_BY_SID].mCount != 0)
                ? ID_TRUE : ID_FALSE);
    }
    idBool existSPFileValByAsterisk()
    {
        return ((mSrcValArr[IDP_VALUE_FROM_SPFILE_BY_ASTERISK].mCount != 0)
                ? ID_TRUE : ID_FALSE);
    }
    IDE_RC clone(SChar* aSID, idpBase** aCloneObj)
    {
        return mVirtFunc->mClone(this, aSID, (void**)aCloneObj);
    }
    void cloneValue(void* aObj, void* aSrc, void** aDst)
    {
        mVirtFunc->mCloneValue(aObj, aSrc, aDst);
    }    
    UInt   getSize(void *aSrc)               /* for copy value */
    {
        return mVirtFunc->mGetSize(this, aSrc);
    }
    SInt   compare(void *aVal1, void *aVal2)
    {
        return mVirtFunc->mCompare(this, aVal1, aVal2);
    }
    IDE_RC validateRange(void *aVal)   /* for checkRange */
    {
        return mVirtFunc->mValidateRange(this, aVal);
    }
    IDE_RC  convertFromString(void *aString, void **aResult)  /* for insert */
    {
        return mVirtFunc->mConvertFromString(this, aString, aResult);
    }
    UInt   convertToString(void *aSrcMem,
                           void *aDestMem,
                           UInt  aDestSize)  /* for conversion to string */
    {
        return mVirtFunc->mConvertToString(this, aSrcMem, aDestMem, aDestSize);
    }
    
    UInt   getMemValueCount() 
    {
        return mMemVal.mCount;
    }
    
    
    static UInt convertToChar(void        *aBaseObj,
                                void        *aMember,
                                UChar       *aBuf,
                                UInt         aBufSize);
};


#endif /* _O_IDP_H_ */
