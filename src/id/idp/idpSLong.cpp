/***********************************************************************
 * Copyright 1999-2000, RTBase Corporation or its subsidiaries.
 * All rights reserved.
 **********************************************************************/

/***********************************************************************
 * $Id: idpSLong.cpp 55981 2012-10-18 06:22:55Z gurugio $
 **********************************************************************/
#include <idl.h>
#include <ide.h>
#include <idp.h>
#include <idu.h>
#include <idpSLong.h>

static UInt idpSLongGetSize(void *, void *)
{
    return ID_SIZEOF(SLong);
}

static SInt idpSLongCompare(void *aObj, void *aVal1, void *aVal2)
{
    idpSLong *obj = (idpSLong *)aObj;
    return obj->compare(aVal1, aVal2);
}

static IDE_RC idpSLongValidateRange(void* aObj, void *aVal)
{
    idpSLong *obj = (idpSLong *)aObj;
    return obj->validateRange(aVal);
}

static IDE_RC idpSLongConvertFromString(void *aObj, void *aString, void **aResult)
{
    idpSLong *obj = (idpSLong *)aObj;
    return obj->convertFromString(aString, aResult);
}

static UInt idpSLongConvertToString(void *aObj,
                                   void *aSrcMem,
                                   void *aDestMem,
                                   UInt aDestSize)
{
    idpSLong *obj = (idpSLong *)aObj;
    return obj->convertToString(aSrcMem, aDestMem, aDestSize);
}

static IDE_RC idpSLongClone(void* aObj, SChar* aSID, void** aCloneObj)
{
    idpSLong* obj = (idpSLong *)aObj;
    return obj->clone(obj, aSID, aCloneObj);
}

static void idpSLongCloneValue(void* aObj, void* aSrc, void** aDst)
{
    idpSLong* obj = (idpSLong *)aObj;
    obj->cloneValue(aSrc, aDst);
}

static idpVirtualFunction gIdpVirtFuncSLong =
{
    idpSLongGetSize,
    idpSLongCompare,
    idpSLongValidateRange,
    idpSLongConvertFromString,
    idpSLongConvertToString,
    idpSLongClone,
    idpSLongCloneValue
};

idpSLong::idpSLong(const SChar *aName,
                   idpAttr      aAttr,
                   SLong        aMin,
                   SLong        aMax,
                   SLong        aDefault)
{
    UInt sValSrc, sValNum;
    
    mVirtFunc = &gIdpVirtFuncSLong;

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


SInt  idpSLong::compare(void *aVal1, void *aVal2)
{
    SLong sValue1;
    SLong sValue2;

    idlOS::memcpy(&sValue1, aVal1, ID_SIZEOF(SLong)); // preventing SIGBUG
    idlOS::memcpy(&sValue2, aVal2, ID_SIZEOF(SLong)); // preventing SIGBUG

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

IDE_RC idpSLong::validateRange(void *aVal)
{
    SLong sValue;

    IDE_TEST_RAISE((compare(aVal, mMin) < 0) || (compare(aVal, mMax) > 0),
                   ERR_RANGE);

    return IDE_SUCCESS;

    // ���⼭�� ���� ���� �� �����ڵ带 ��� �����Ѵ�.
    // �ֳ��ϸ�, �� �Լ��� insert() �� �ƴ϶�, update()������ ȣ��Ǳ� �����̴�.
    IDE_EXCEPTION(ERR_RANGE);
    {
        idlOS::memcpy(&sValue, aVal, ID_SIZEOF(SLong)); // preventing SIGBUG

        idlOS::snprintf(mErrorBuf,
                        IDP_ERROR_BUF_SIZE,
                        "idp checkRange() Error : Property [%s] %"ID_INT64_FMT
                        " Overflowed the Value Range."
                        "(%"ID_INT64_FMT"~%"ID_INT64_FMT")",
                        getName(),
                        sValue,
                        mInMin,
                        mInMax);
        IDE_SET(ideSetErrorCode(idERR_ABORT_idp_RangeOverflow, getName()));
    }

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

UInt  idpSLong::convertToString(void  *aSrcMem,
                                void  *aDestMem,
                                UInt   aDestSize) /* for conversion to string*/
{
    if (aSrcMem != NULL)
    {
        SChar *sDestMem = (SChar *)aDestMem;
        UInt   sSize;

        sSize = idlOS::snprintf(sDestMem, aDestSize, "%"ID_INT64_FMT, *((SLong *)aSrcMem));
        return IDL_MIN(sSize, aDestSize - 1);
    }
    return 0;
}

IDE_RC idpSLong::convertFromString(void *aString, void **aResult) // When Startup
{
    SLong       *sValue = NULL;
    ULong        sTmpValue;
    acp_rc_t     sRC;
    acp_sint32_t sSign;
    acp_char_t  *sEnd;

    sValue = (SLong *)iduMemMgr::mallocRaw(ID_SIZEOF(SLong), IDU_MEM_FORCE);
    IDE_ASSERT(sValue != NULL);

    sRC = acpCStrToInt64((acp_char_t *)aString,
                         acpCStrLen((acp_char_t *)aString, ID_UINT_MAX),
                         &sSign,
                         (acp_uint64_t *)&sTmpValue,
                         10, /* only decimal */
                         &sEnd);
    IDE_TEST_RAISE(ACP_RC_NOT_SUCCESS(sRC), data_validation_error);

    switch (*sEnd)
    {
        /* absolute value range: 0 ~ 2^64-1 */
        case 'g':
        case 'G':
            IDE_TEST_RAISE(sTmpValue >= 2*1024*1024, data_validation_error);
            sTmpValue = sTmpValue * 1024 * 1024 * 1024;
            break;
        case 'm':
        case 'M':
            IDE_TEST_RAISE(sTmpValue >= (ULong)2*1024*1024*1024, data_validation_error);
            sTmpValue = sTmpValue * 1024 * 1024;
            break;
        case 'k':
        case 'K':
            IDE_TEST_RAISE(sTmpValue >= (ULong)2*1024*1024*1024*1024, data_validation_error);
            sTmpValue = sTmpValue * 1024;
            break;
        case '\n':
        case 0:
            IDE_TEST_RAISE(sSign == 1 && sTmpValue > ID_SLONG_MAX, data_validation_error);
            IDE_TEST_RAISE(sSign == -1 && sTmpValue > (ULong)ID_SLONG_MIN, data_validation_error);
            break;
        default:
            goto data_validation_error;
    }

    *sValue = (SLong)sTmpValue * sSign;
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

/**************************************************************************
 * Description :
 *    aObj ��ü�� ������ Ÿ���� ��ü�� �����Ͽ� idpBase*�� ��ȯ�Ѵ�. 
 *    ������, aSID�� ���� ���� SID�� �����ϸ�, "*"�� ������ ����
 *    �����ϰ� �� ���� ���� null ������ �ʱ�ȭ �ȴ�.  
 * aObj      - [IN] ������ ���� Source ��ü
 * aSID      - [IN] ������ ��ü�� �ο��� ���ο� SID
 * aCloneObj - [OUT] ������ �� ��ȯ�Ǵ� ��ü
 **************************************************************************/
IDE_RC idpSLong::clone(idpSLong* aObj, SChar* aSID, void** aCloneObj)
{
    idpSLong      *sCloneObj = NULL;
    UInt           sValNum;
    idpValueSource sSrc;
    *aCloneObj = NULL;
    
    sCloneObj = (idpSLong*)iduMemMgr::mallocRaw(ID_SIZEOF(idpSLong));
    
    IDE_TEST_RAISE(sCloneObj == NULL, memory_alloc_error);
    
    new (sCloneObj) idpSLong(aObj->mName, 
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
                        "idpSLong clone() Error : memory allocation error\n");
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

void idpSLong::cloneValue(void* aSrc, void** aDst)
{
    SLong* sValue;
    
    sValue = (SLong *)iduMemMgr::mallocRaw(ID_SIZEOF(SLong), IDU_MEM_FORCE);
    
    IDE_ASSERT(sValue != NULL);
    
    *sValue = *(SLong*)aSrc;
    
    IDP_TYPE_ALIGN(SLong, *sValue);
    
    *aDst = sValue;
}

