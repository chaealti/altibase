/***********************************************************************
 * Copyright 1999-2000, RTBase Corporation or its subsidiaries.
 * All rights reserved.
 **********************************************************************/

/***********************************************************************
 * $Id: idpSInt.cpp 55945 2012-10-16 06:05:36Z gurugio $
 **********************************************************************/
#include <idl.h>
#include <ide.h>
#include <idp.h>
#include <idu.h>
#include <idpSInt.h>

static UInt idpSIntGetSize(void *, void *)
{
    return ID_SIZEOF(SInt);
}

static SInt idpSIntCompare(void *aObj, void *aVal1, void *aVal2)
{
    idpSInt *obj = (idpSInt *)aObj;
    return obj->compare(aVal1, aVal2);
}

static IDE_RC idpSIntValidateRange(void* aObj, void *aVal)
{
    idpSInt *obj = (idpSInt *)aObj;
    return obj->validateRange(aVal);
}

static IDE_RC idpSIntConvertFromString(void *aObj,
                                       void *aString,
                                       void **aResult)
{
    idpSInt *obj = (idpSInt *)aObj;
    return obj->convertFromString(aString, aResult);
}

static UInt idpSIntConvertToString(void *aObj,
                                   void *aSrcMem,
                                   void *aDestMem,
                                   UInt aDestSize)
{
    idpSInt *obj = (idpSInt *)aObj;
    return obj->convertToString(aSrcMem, aDestMem, aDestSize);
}

static IDE_RC idpSIntClone(void* aObj, SChar* aSID, void** aCloneObj)
{
    idpSInt* obj = (idpSInt *)aObj;
    return obj->clone(obj, aSID, aCloneObj);
}

static void idpSIntCloneValue(void* aObj, void* aSrc, void** aDst)
{
    idpSInt* obj = (idpSInt *)aObj;
    obj->cloneValue(aSrc, aDst);
}

static idpVirtualFunction gIdpVirtFuncSInt =
{
    idpSIntGetSize,
    idpSIntCompare,
    idpSIntValidateRange,
    idpSIntConvertFromString,
    idpSIntConvertToString,
    idpSIntClone,
    idpSIntCloneValue
};

idpSInt::idpSInt(const SChar *aName,
                 idpAttr     aAttr,
                 SInt        aMin,
                 SInt        aMax,
                 SInt        aDefault)
{
    UInt sValSrc, sValNum;
    
    mVirtFunc = &gIdpVirtFuncSInt;

    mName        = (SChar *)aName;
    mAttr        = aAttr;
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


SInt  idpSInt::compare(void *aVal1, void *aVal2)
{
    SInt sValue1;
    SInt sValue2;

    idlOS::memcpy(&sValue1, aVal1, ID_SIZEOF(SInt)); // preventing SIGBUG
    idlOS::memcpy(&sValue2, aVal2, ID_SIZEOF(SInt)); // preventing SIGBUG

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

IDE_RC idpSInt::validateRange(void *aVal)
{
    SInt sValue;

    IDE_TEST_RAISE((compare(aVal, mMin) < 0) || (compare(aVal, mMax) > 0),
                   ERR_RANGE);

    return IDE_SUCCESS;

    // ���⼭�� ���� ���� �� �����ڵ带 ��� �����Ѵ�.
    // �ֳ��ϸ�, �� �Լ��� insert() �� �ƴ϶�, update()������ ȣ��Ǳ� �����̴�.
    IDE_EXCEPTION(ERR_RANGE);
    {
        idlOS::memcpy(&sValue, aVal, ID_SIZEOF(SInt)); // preventing SIGBUG

        idlOS::snprintf(mErrorBuf,
                        IDP_ERROR_BUF_SIZE,
                        "idp checkRange() Error : Property [%s] %"ID_INT32_FMT
                        " Overflowed the Value Range."
                        "(%"ID_INT32_FMT"~%"ID_INT32_FMT")",
                        getName(),
                        sValue,
                        mInMin,
                        mInMax);
        IDE_SET(ideSetErrorCode(idERR_ABORT_idp_RangeOverflow, getName()));
    }

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC idpSInt::convertFromString(void *aString, void **aResult) // When Startup
{
    SInt        *sValue = NULL;
    UInt         sTmpValue;
    acp_rc_t     sRC;
    acp_sint32_t sSign;
    acp_char_t  *sEnd;

    sValue = (SInt *)iduMemMgr::mallocRaw(ID_SIZEOF(SInt), IDU_MEM_FORCE);
    IDE_ASSERT(sValue != NULL);

    sRC = acpCStrToInt32((acp_char_t *)aString,
                         acpCStrLen((acp_char_t *)aString, ID_UINT_MAX),
                         &sSign,
                         &sTmpValue,
                         10, /* only decimal */
                         &sEnd);
    IDE_TEST_RAISE(ACP_RC_NOT_SUCCESS(sRC), data_validation_error);

    switch (*sEnd)
    {
        /* range: -2G ~ 2G-1 */
        case 'g':
        case 'G':
            IDE_TEST_RAISE(sTmpValue >= 2, data_validation_error);
            sTmpValue = sTmpValue * 1024 * 1024 * 1024;
            break;
        case 'm':
        case 'M':
            IDE_TEST_RAISE(sTmpValue >= 2*1024, data_validation_error);
            sTmpValue = sTmpValue * 1024 * 1024;
            break;
        case 'k':
        case 'K':
            IDE_TEST_RAISE(sTmpValue >= 2*1024*1024, data_validation_error);
            sTmpValue = sTmpValue * 1024;
            break;
        case '\n':
        case 0:
            IDE_TEST_RAISE(sSign == 1 && sTmpValue > (UInt)ID_SINT_MAX, data_validation_error);
            IDE_TEST_RAISE(sSign == -1 && sTmpValue > (UInt)ID_SINT_MIN, data_validation_error);
            break;
        default:
            goto data_validation_error;
    }
    
    *sValue = (SInt)sTmpValue * sSign;
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

UInt  idpSInt::convertToString(void  *aSrcMem,
                               void  *aDestMem,
                               UInt   aDestSize) /* for conversion to string*/
{
    if (aSrcMem != NULL)
    {
        SChar *sDestMem = (SChar *)aDestMem;
        UInt   sSize;

        sSize = idlOS::snprintf(sDestMem, aDestSize, "%"ID_INT32_FMT, *((SInt *)aSrcMem));
        return IDL_MIN(sSize, aDestSize - 1);
    }

    return 0;
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
IDE_RC idpSInt::clone(idpSInt* aObj, SChar* aSID, void** aCloneObj)
{
    idpSInt       *sCloneObj = NULL;
    UInt           sValNum;
    idpValueSource sSrc;
    
    *aCloneObj = NULL;
    
    sCloneObj = (idpSInt*)iduMemMgr::mallocRaw(ID_SIZEOF(idpSInt));
    
    IDE_TEST_RAISE(sCloneObj == NULL, memory_alloc_error);
    
    new (sCloneObj) idpSInt(aObj->mName, 
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
                        "idpSInt clone() Error : memory allocation error\n");
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

void idpSInt::cloneValue(void* aSrc, void** aDst)
{
    SInt* sValue;

    sValue = (SInt *)iduMemMgr::mallocRaw(ID_SIZEOF(SInt), IDU_MEM_FORCE);
    
    IDE_ASSERT(sValue != NULL);
    
    *sValue = *(SInt*)aSrc;
    
    IDP_TYPE_ALIGN(SInt, *sValue);
                   
    *aDst = sValue;
}
