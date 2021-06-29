/***********************************************************************
 * Copyright 1999-2000, RTBase Corporation or its subsidiaries.
 * All rights reserved.
 **********************************************************************/

/***********************************************************************
 * $Id: idpUInt.cpp 55945 2012-10-16 06:05:36Z gurugio $
 **********************************************************************/
#include <idl.h>
#include <ide.h>
#include <idp.h>
#include <idu.h>
#include <idpUInt.h>

static UInt idpUIntGetSize(void *, void *)
{
    return ID_SIZEOF(UInt);
}

static SInt idpUIntCompare(void *aObj, void *aVal1, void *aVal2)
{
    idpUInt *obj = (idpUInt *)aObj;
    return obj->compare(aVal1, aVal2);
}

static IDE_RC idpUIntValidateRange(void* aObj, void *aVal)
{
    idpUInt *obj = (idpUInt *)aObj;
    return obj->validateRange(aVal);
}

static IDE_RC idpUIntConvertFromString(void *aObj,
                                       void *aString,
                                       void **aResult)
{
    idpUInt *obj = (idpUInt *)aObj;
    return obj->convertFromString(aString, aResult);
}

static UInt idpUIntConvertToString(void *aObj,
                                   void *aSrcMem,
                                   void *aDestMem,
                                   UInt aDestSize)
{
    idpUInt *obj = (idpUInt *)aObj;
    return obj->convertToString(aSrcMem, aDestMem, aDestSize);
}

static IDE_RC idpUIntClone(void* aObj, SChar* aSID, void** aCloneObj)
{
    idpUInt* obj = (idpUInt *)aObj;
    return obj->clone(obj, aSID, aCloneObj);
}

static void idpUIntCloneValue(void* aObj, void* aSrc, void** aDst)
{
    idpUInt* obj = (idpUInt *)aObj;
    obj->cloneValue(aSrc, aDst);
}

static idpVirtualFunction gIdpVirtFuncUInt =
{
    idpUIntGetSize,
    idpUIntCompare,
    idpUIntValidateRange,
    idpUIntConvertFromString,
    idpUIntConvertToString,
    idpUIntClone,
    idpUIntCloneValue
};

idpUInt::idpUInt(const SChar *aName,
                 idpAttr     aAttr,
                 UInt        aMin,
                 UInt        aMax,
                 UInt        aDefault)
{
    UInt sValSrc, sValNum;
    mVirtFunc = &gIdpVirtFuncUInt;

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


SInt  idpUInt::compare(void *aVal1, void *aVal2)
{
    UInt sValue1;
    UInt sValue2;

    idlOS::memcpy(&sValue1, aVal1, ID_SIZEOF(UInt)); // preventing SIGBUG
    idlOS::memcpy(&sValue2, aVal2, ID_SIZEOF(UInt)); // preventing SIGBUG

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

IDE_RC idpUInt::validateRange(void *aVal)
{
    UInt sValue;

    IDE_TEST_RAISE((compare(aVal, mMin) < 0) || (compare(aVal, mMax) > 0),
                   ERR_RANGE);

    return IDE_SUCCESS;

    // ���⼭�� ���� ���� �� �����ڵ带 ��� �����Ѵ�.
    // �ֳ��ϸ�, �� �Լ��� insert() �� �ƴ϶�, update()������ ȣ��Ǳ� �����̴�.
    IDE_EXCEPTION(ERR_RANGE);
    {
        idlOS::memcpy(&sValue, aVal, ID_SIZEOF(UInt)); // preventing SIGBUG

        idlOS::snprintf(mErrorBuf,
                        IDP_ERROR_BUF_SIZE,
                        "idp checkRange() Error : Property [%s] %"ID_UINT32_FMT
                        " Overflowed the Value Range."
                        "(%"ID_UINT32_FMT"~%"ID_UINT32_FMT")",
                        getName(),
                        sValue,
                        mInMin,
                        mInMax);
        IDE_SET(ideSetErrorCode(idERR_ABORT_idp_RangeOverflow, getName()));
    }

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

UInt  idpUInt::convertToString(void  *aSrcMem,
                                void  *aDestMem,
                                UInt   aDestSize) /* for conversion to string*/
{
    if (aSrcMem != NULL)
    {
        SChar *sDestMem = (SChar *)aDestMem;
        UInt   sSize;

        sSize = idlOS::snprintf(sDestMem, aDestSize, "%"ID_UINT32_FMT, *((UInt *)aSrcMem));
        return IDL_MIN(sSize, aDestSize - 1);
    }
    return 0;

}

IDE_RC idpUInt::convertFromString(void *aString, void **aResult)
{
    UInt        *sValue = NULL;
    acp_rc_t     sRC;
    acp_sint32_t sSign;
    acp_char_t  *sEnd;
    
    sValue = (UInt *)iduMemMgr::mallocRaw(ID_SIZEOF(UInt), IDU_MEM_FORCE);
    IDE_ASSERT(sValue != NULL);

    sRC = acpCStrToInt32((acp_char_t *)aString,
                         acpCStrLen((acp_char_t *)aString, ID_UINT_MAX),
                         &sSign,
                         sValue,
                         10, /* only decimal */
                         &sEnd);
    IDE_TEST_RAISE(ACP_RC_NOT_SUCCESS(sRC) || sSign == -1, data_validation_error);

    switch (*sEnd)
    {
        /* range: 0 ~ 4G-1 */
        case 'g':
        case 'G':
            IDE_TEST_RAISE(*sValue >= 4, data_validation_error);                
            *sValue = *sValue * 1024 * 1024 * 1024;
            break;
        case 'm':
        case 'M':
            IDE_TEST_RAISE(*sValue >= 4*1024, data_validation_error);
            *sValue = *sValue * 1024 * 1024;
            break;
        case 'k':
        case 'K':
            IDE_TEST_RAISE(*sValue >= 4*1024*1024, data_validation_error);
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

void idpUInt::normalize(UInt aUInt, UChar *aNormalized)
{
#ifdef ENDIAN_IS_BIG_ENDIAN
    aNormalized[0] = ((UChar*)&aUInt)[0];
    aNormalized[1] = ((UChar*)&aUInt)[1];
    aNormalized[2] = ((UChar*)&aUInt)[2];
    aNormalized[3] = ((UChar*)&aUInt)[3];
#else
    aNormalized[0] = ((UChar*)&aUInt)[3];
    aNormalized[1] = ((UChar*)&aUInt)[2];
    aNormalized[2] = ((UChar*)&aUInt)[1];
    aNormalized[3] = ((UChar*)&aUInt)[0];
#endif    
}

void idpUInt::normalize3Bytes(UInt aUInt, UChar *aNormalized3Bytes)
{
#ifdef ENDIAN_IS_BIG_ENDIAN
    aNormalized3Bytes[0] = ((UChar*)&aUInt)[0];
    aNormalized3Bytes[1] = ((UChar*)&aUInt)[1];
    aNormalized3Bytes[2] = ((UChar*)&aUInt)[2];
#else
    aNormalized3Bytes[0] = ((UChar*)&aUInt)[3];
    aNormalized3Bytes[1] = ((UChar*)&aUInt)[2];
    aNormalized3Bytes[2] = ((UChar*)&aUInt)[1];
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
IDE_RC idpUInt::clone(idpUInt* aObj, SChar* aSID, void** aCloneObj)
{
    idpUInt       *sCloneObj = NULL;
    UInt           sValNum;
    idpValueSource sSrc;
    
    *aCloneObj = NULL;
    
    sCloneObj = (idpUInt*)iduMemMgr::mallocRaw(ID_SIZEOF(idpUInt));
    
    IDE_TEST_RAISE(sCloneObj == NULL, memory_alloc_error);
    
    new (sCloneObj) idpUInt(aObj->mName, 
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
                        "idpUInt clone() Error : memory allocation error\n");
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

void idpUInt::cloneValue(void* aSrc, void** aDst)
{
    UInt* sValue;
    
    sValue = (UInt *)iduMemMgr::mallocRaw(ID_SIZEOF(UInt), IDU_MEM_FORCE);
    
    IDE_ASSERT(sValue != NULL);
    
    *sValue = *(UInt*)(aSrc);
    
    IDP_TYPE_ALIGN(UInt, *sValue);
    
    *aDst = sValue;
}
