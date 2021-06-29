/***********************************************************************
 * Copyright 1999-2000, RTBase Corporation or its subsidiaries.
 * All rights reserved.
 **********************************************************************/

/***********************************************************************
 * $Id: idpULong.cpp 55981 2012-10-18 06:22:55Z gurugio $
 **********************************************************************/
#include <idl.h>
#include <ide.h>
#include <idp.h>
#include <idu.h>
#include <idpULong.h>

static UInt idpULongGetSize(void *, void *)
{
    return ID_SIZEOF(ULong);
}

static SInt idpULongCompare(void *aObj, void *aVal1, void *aVal2)
{
    idpULong *obj = (idpULong *)aObj;
    return obj->compare(aVal1, aVal2);
}

static IDE_RC idpULongValidateRange(void* aObj, void *aVal)
{
    idpULong *obj = (idpULong *)aObj;
    return obj->validateRange(aVal);
}

static IDE_RC idpULongConvertFromString(void *aObj, void *aString, void **aResult)
{
    idpULong *obj = (idpULong *)aObj;
    return obj->convertFromString(aString, aResult);
}

static UInt idpULongConvertToString(void *aObj,
                                   void *aSrcMem,
                                   void *aDestMem,
                                   UInt aDestSize)
{
    idpULong *obj = (idpULong *)aObj;
    return obj->convertToString(aSrcMem, aDestMem, aDestSize);
}

static IDE_RC idpULongClone(void* aObj, SChar* aSID, void** aCloneObj)
{
    idpULong* obj = (idpULong *)aObj;
    return obj->clone(obj, aSID, aCloneObj);
}

static void idpULongCloneValue(void* aObj, void* aSrc, void** aDst)
{
    idpULong* obj = (idpULong*) aObj;
    obj->cloneValue(aSrc, aDst);
}

static idpVirtualFunction gIdpVirtFuncULong =
{
    idpULongGetSize,
    idpULongCompare,
    idpULongValidateRange,
    idpULongConvertFromString,
    idpULongConvertToString,
    idpULongClone,
    idpULongCloneValue
};

idpULong::idpULong(const SChar *aName,
                   idpAttr      aAttr,
                   ULong        aMin,
                   ULong        aMax,
                   ULong        aDefault)
{
    UInt sValSrc, sValNum;
    
    mVirtFunc = &gIdpVirtFuncULong;
    
    mName          = (SChar *)aName;
    mAttr          = aAttr;
    mMemVal.mCount = 0;
    
    idlOS::memset(mMemVal.mVal, 0, ID_SIZEOF(mMemVal.mVal));

    for( sValSrc = 0; sValSrc < IDP_MAX_VALUE_SOURCE_COUNT; sValSrc++)
    {
        mSrcValArr[sValSrc].mCount = 0;
        for( sValNum = 0; sValNum < IDP_MAX_VALUE_COUNT; sValNum++)
        {
            mSrcValArr[sValSrc].mVal[sValNum] = NULL;
        }
    }

    // Store Value
    mInMin        = aMin;
    mInMax        = aMax;
    mInDefault    = aDefault;

    // Set Internal Value Expression using ptr.
    mMin          = &mInMin;
    mMax          = &mInMax;

    //default�κ��� �� ���� Source Value�� �ִ´�. 
    mSrcValArr[IDP_VALUE_FROM_DEFAULT].mVal[0] =&mInDefault;
    mSrcValArr[IDP_VALUE_FROM_DEFAULT].mCount++;
}


SInt  idpULong::compare(void *aVal1, void *aVal2)
{
    ULong sValue1;
    ULong sValue2;

    idlOS::memcpy(&sValue1, aVal1, ID_SIZEOF(ULong)); // preventing SIGBUG
    idlOS::memcpy(&sValue2, aVal2, ID_SIZEOF(ULong)); // preventing SIGBUG

    if (sValue1 > sValue2)
    {
        return 1;
    }
    if (sValue1 < sValue2)
    {
        return -1;
    }
    return 0;
}

IDE_RC idpULong::validateRange(void *aVal)
{
    ULong sValue;

    IDE_TEST_RAISE((compare(aVal, mMin) < 0) || (compare(aVal, mMax) > 0),
                   ERR_RANGE);

    return IDE_SUCCESS;

    // ���⼭�� ���� ���� �� �����ڵ带 ��� �����Ѵ�.
    // �ֳ��ϸ�, �� �Լ��� insert() �� �ƴ϶�, update()������ ȣ��Ǳ� �����̴�.
    IDE_EXCEPTION(ERR_RANGE);
    {
        idlOS::memcpy(&sValue, aVal, ID_SIZEOF(ULong)); // preventing SIGBUG

        idlOS::snprintf(mErrorBuf,
                        IDP_ERROR_BUF_SIZE,
                        "idp checkRange() Error : Property [%s] %"ID_UINT64_FMT
                        " Overflowed the Value Range."
                        "(%"ID_UINT64_FMT"~%"ID_UINT64_FMT")",
                        getName(),
                        sValue,
                        mInMin,
                        mInMax);
        IDE_SET(ideSetErrorCode(idERR_ABORT_idp_RangeOverflow, getName()));
    }

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

UInt  idpULong::convertToString(void  *aSrcMem,
                                void  *aDestMem,
                                UInt   aDestSize) /* for conversion to string*/
{
    if (aSrcMem != NULL)
    {
        SChar *sDestMem = (SChar *)aDestMem;
        UInt   sSize;

        sSize = idlOS::snprintf(sDestMem, aDestSize, "%"ID_UINT64_FMT, *((ULong *)aSrcMem));
        return IDL_MIN(sSize, aDestSize - 1);
    }
    return 0;
}

IDE_RC idpULong::convertFromString(void *aString, void **aResult) // When Startup
{
    ULong       *sValue = NULL;
    acp_rc_t     sRC;
    acp_sint32_t sSign;
    acp_char_t  *sEnd;

    sValue = (ULong *)iduMemMgr::mallocRaw(ID_SIZEOF(ULong), IDU_MEM_FORCE);
    IDE_ASSERT(sValue != NULL);

    sRC = acpCStrToInt64((acp_char_t *)aString,
                         acpCStrLen((acp_char_t *)aString, ID_UINT_MAX),
                         &sSign,
                         (acp_uint64_t *)sValue,
                         10, /* only decimal */
                         &sEnd);
    IDE_TEST_RAISE(ACP_RC_NOT_SUCCESS(sRC) || sSign == -1, data_validation_error);

    switch (*sEnd)
    {
        /* range: 0 ~ 2^64-1 */
        case 'g':
        case 'G':
            IDE_TEST_RAISE(*sValue >= 4*1024*1024, data_validation_error);                
            *sValue = *sValue * 1024 * 1024 * 1024;
            break;
        case 'm':
        case 'M':
            IDE_TEST_RAISE(*sValue >= (ULong)4*1024*1024*1024, data_validation_error);                
            *sValue = *sValue * 1024 * 1024;
            break;
        case 'k':
        case 'K':
            IDE_TEST_RAISE(*sValue >= (ULong)4*1024*1024*1024*1024, data_validation_error);                
            *sValue = *sValue * 1024;
            break;
        case '\n':
        case 0:
            // no postfix and no wrong character
            break;
        default:
            goto data_validation_error;
    }

    *aResult = sValue;

    return IDE_SUCCESS;
    IDE_EXCEPTION(data_validation_error);
    {
        /* BUG-17208:
         * altibase.properties���� ������ �� �ڿ� �����ݷ��� �پ����� ���
         * �ƹ��� ���� �޽��� ���� iSQL�� ������� �ʴ� ���װ� �־���.
         * ������ ������ iSQL�� mErrorBuf�� ����ִ� ���� �޽����� ����ϴµ�
         * mErrorBuf�� ���� �޽����� �������� ���� ���̾���.
         * �Ʒ��� ���� mErrorBuf�� ���� �޽����� �����ϴ� �ڵ带 �߰��Ͽ�
         * ���׸� �����Ѵ�. */
        idlOS::snprintf(mErrorBuf,
                        IDP_ERROR_BUF_SIZE,
                        "idp convertFromString() Error : "
                        "The property [%s] value [%s] is not convertable.",
                        getName(),
                        aString);
        IDE_SET(ideSetErrorCode(idERR_ABORT_idp_Value_Convert_Error, getName() ,aString));
    }
    IDE_EXCEPTION_END;
    
    if(sValue != NULL)
    {
        iduMemMgr::freeRaw(sValue);
    }
    
    return IDE_FAILURE;
}

void idpULong::normalize(ULong aULong, UChar *aNormalized)
{
#ifdef ENDIAN_IS_BIG_ENDIAN
    aNormalized[0] = ((UChar*)&aULong)[0];
    aNormalized[1] = ((UChar*)&aULong)[1];
    aNormalized[2] = ((UChar*)&aULong)[2];
    aNormalized[3] = ((UChar*)&aULong)[3];
    aNormalized[4] = ((UChar*)&aULong)[4];
    aNormalized[5] = ((UChar*)&aULong)[5];
    aNormalized[6] = ((UChar*)&aULong)[6];
    aNormalized[7] = ((UChar*)&aULong)[7];
#else
    aNormalized[0] = ((UChar*)&aULong)[7];
    aNormalized[1] = ((UChar*)&aULong)[6];
    aNormalized[2] = ((UChar*)&aULong)[5];
    aNormalized[3] = ((UChar*)&aULong)[4];
    aNormalized[4] = ((UChar*)&aULong)[3];
    aNormalized[5] = ((UChar*)&aULong)[2];
    aNormalized[6] = ((UChar*)&aULong)[1];
    aNormalized[7] = ((UChar*)&aULong)[0];
#endif    
}

void idpULong::normalize7Bytes(ULong aULong, UChar *aNormalized7Bytes)
{
#ifdef ENDIAN_IS_BIG_ENDIAN
    aNormalized7Bytes[0] = ((UChar*)&aULong)[0];
    aNormalized7Bytes[1] = ((UChar*)&aULong)[1];
    aNormalized7Bytes[2] = ((UChar*)&aULong)[2];
    aNormalized7Bytes[3] = ((UChar*)&aULong)[3];
    aNormalized7Bytes[4] = ((UChar*)&aULong)[4];
    aNormalized7Bytes[5] = ((UChar*)&aULong)[5];
    aNormalized7Bytes[6] = ((UChar*)&aULong)[6];
#else
    aNormalized7Bytes[0] = ((UChar*)&aULong)[7];
    aNormalized7Bytes[1] = ((UChar*)&aULong)[6];
    aNormalized7Bytes[2] = ((UChar*)&aULong)[5];
    aNormalized7Bytes[3] = ((UChar*)&aULong)[4];
    aNormalized7Bytes[4] = ((UChar*)&aULong)[3];
    aNormalized7Bytes[5] = ((UChar*)&aULong)[2];
    aNormalized7Bytes[6] = ((UChar*)&aULong)[1];
#endif    
}

/**************************************************************************
 * Description :
 *    aObj ��ü�� ������ Ÿ���� ��ü�� �����Ͽ� idpBase*�� ��ȯ�Ѵ�. 
 *    ������, aSID�� ���� ���� SID�� �����ϸ�, "*"�� ������ ����
 *    �����ϰ� �� ���� ���� null ������ �ʱ�ȭ �ȴ�.  
 * aObj      - [IN] ������ ���� Source ��ü
 * aSID      - [IN] ������ ��ü�� �ο��� ���ο� SID
 * aCloneObj - [OUT] ������ �� ��ȯ�Ǵ� ��ü
 **************************************************************************/
IDE_RC idpULong::clone(idpULong* aObj, SChar* aSID, void** aCloneObj)
{
    idpULong      *sCloneObj = NULL;
    UInt           sValNum;
    idpValueSource sSrc;
    
    *aCloneObj = NULL;
    
    sCloneObj = (idpULong*)iduMemMgr::mallocRaw(ID_SIZEOF(idpULong));
    
    IDE_TEST_RAISE(sCloneObj == NULL, memory_alloc_error);
    
    new (sCloneObj) idpULong(aObj->mName, 
                             aObj->mAttr,
                             aObj->mInMin,
                             aObj->mInMax,
                             aObj->mInDefault);
    
    sCloneObj->setSID(aSID);
    /*"*"�� ������ ���븸 �����Ѵ�.*/
    sSrc = IDP_VALUE_FROM_SPFILE_BY_ASTERISK;
    
    for(sValNum = 0; sValNum < aObj->mSrcValArr[sSrc].mCount; sValNum++)
    {
        IDE_TEST(sCloneObj->insertRawBySrc(aObj->mSrcValArr[sSrc].mVal[sValNum], 
                                           IDP_VALUE_FROM_SPFILE_BY_ASTERISK) != IDE_SUCCESS);
    }
    
    *aCloneObj = (idpBase*)sCloneObj;
    
    return IDE_SUCCESS;
    
    IDE_EXCEPTION(memory_alloc_error)
    {
        idlOS::snprintf(mErrorBuf, 
                        IDP_ERROR_BUF_SIZE, 
                        "idpULong clone() Error : memory allocation error\n");
    }
    IDE_EXCEPTION_END;
    
    if(sCloneObj != NULL)
    {
        iduMemMgr::freeRaw(sCloneObj);
        sCloneObj = NULL;
    }
    *aCloneObj = NULL;
    
    return IDE_FAILURE;    
}

void idpULong::cloneValue(void* aSrc, void** aDst)
{
    ULong* sValue;
    
    sValue = (ULong *)iduMemMgr::mallocRaw(ID_SIZEOF(ULong), IDU_MEM_FORCE);
    
    IDE_ASSERT(sValue != NULL);
    
    *sValue = *(ULong*)aSrc;
    
    IDP_TYPE_ALIGN(ULong, *sValue);
    
    *aDst = sValue;
}
