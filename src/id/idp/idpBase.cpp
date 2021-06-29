/***********************************************************************
 * Copyright 1999-2000, RTBase Corporation or its subsidiaries.
 * All rights reserved.
 **********************************************************************/

/***********************************************************************
 * $Id: idpBase.cpp 87064 2020-03-30 04:21:12Z jake.jang $
 *
 * Description:
 *
 * �� Ŭ������ ������Ƽ �Ӽ��� �پ��� ����Ÿ Ÿ�Կ� ����
 * ���̽� Ŭ�����̴�.
 * �� Ŭ���������� pure virtual �Լ��� ��� �غ��� ���� ������,
 * ���� ���ο� ����Ÿ Ÿ���� ������Ƽ�� �߰��� ��쿡��
 * �� Ÿ�Կ� �´� ��� Ŭ������ ���� ����ϸ� �ȴ�.
 *
 * idp.cpp�� �Լ��� �����ϴ� ���� ����ϰ�,
 * �� Ŭ������ method�� ���� �ּ��� �����Ѵ�.
 *
 **********************************************************************/
#include <idl.h>
#include <ide.h>
#include <idp.h>
#include <idu.h>

SChar *idpBase::mErrorBuf;

idpBase::idpBase()
{
    mVirtFunc             = NULL;
    mUpdateBefore = defaultChangeCallback;
    mUpdateAfter  = defaultChangeCallback;

    IDE_ASSERT( idlOS::thread_mutex_init(&mMutex) == 0 );
}

idpBase::~idpBase()
{
    IDE_ASSERT( idlOS::thread_mutex_destroy(&mMutex) == 0 );
}



IDE_RC idpBase::defaultChangeCallback(idvSQL*, SChar *, void *, void *, void *)
{
    return IDE_SUCCESS;
}

void idpBase::initializeStatic(SChar *aErrorBuf)
{
    mErrorBuf = aErrorBuf;
}

void   idpBase::setupBeforeUpdateCallback(idpChangeCallback mCallback)
{
    IDE_DASSERT( mUpdateBefore == defaultChangeCallback );
    mUpdateBefore = mCallback;
}

void   idpBase::setupAfterUpdateCallback(idpChangeCallback mCallback)
{
    IDE_DASSERT( mUpdateAfter == defaultChangeCallback );
    mUpdateAfter = mCallback;
}

IDE_RC idpBase::checkRange(void * aValue)
{
    if ( (mAttr & IDP_ATTR_CK_MASK) == IDP_ATTR_CK_CHECK )
    {
        IDE_TEST( validateRange(aValue) != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/******************************************************************************************
*
* Description :
*  Memovery Value�� ���� �����Ͽ� �����Ѵ�.
*
* void           *aValue,      - [IN]  �����Ϸ����ϴ� ���� ������ (Ÿ�Ժ� Raw Format)
*******************************************************************************************/ 
IDE_RC idpBase::insertMemoryRawValue(void *aValue) /* called by build() */
{
    void *sValue = NULL;
    
    // Multiple Flag Check
    IDE_TEST_RAISE( ((mAttr & IDP_ATTR_ML_MASK) == IDP_ATTR_ML_JUSTONE) &&
                    mMemVal.mCount == 1, only_one_error);
    
    // Store Count Check
    IDE_TEST_RAISE(mMemVal.mCount >= IDP_MAX_VALUE_COUNT,
                   no_more_insert);
    
    cloneValue(this, aValue, &sValue);
    
    /* Value Range Validation */
    IDE_TEST(checkRange(sValue) != IDE_SUCCESS);

    mMemVal.mVal[mMemVal.mCount++] = sValue;
    
    return IDE_SUCCESS;
    IDE_EXCEPTION(only_one_error);
    {
        idlOS::snprintf(mErrorBuf, 
                        IDP_ERROR_BUF_SIZE, 
                        "idp insert() Error : "
                        "Property [%s] Can't Store Multiple Values.",
                        getName());
    }
    IDE_EXCEPTION(no_more_insert);
    {
        idlOS::snprintf(mErrorBuf, 
                        IDP_ERROR_BUF_SIZE, 
                        "idp insert() Error : "
                        "Property [%s] Can't Store more than %"ID_UINT32_FMT
                        " Values.",
                        getName(), 
                        (UInt)IDP_MAX_VALUE_COUNT);
    }
    
    IDE_EXCEPTION_END;
    
    if (sValue != NULL)
    {
        iduMemMgr::freeRaw(sValue);
    }
    
    return IDE_FAILURE;
}

/******************************************************************************************
*
* Description :
*  aSrc�� ���� value source ��ġ�� ��Ʈ�� ������ aValue�� �ڽ��� Ÿ������ ��ȯ�ϰ�, 
*  ���� �����Ͽ� �����Ѵ�.
*
*  SChar         *aValue,   - [IN] �����Ϸ����ϴ� ���� ������ (String Format)
*  idpValueSource aSrc      - [IN] ���� ������ Source ��ġ
*                                  (default/env/pfile/spfile by asterisk, spfile by sid)
*******************************************************************************************/ 
IDE_RC idpBase::insertBySrc(SChar *aValue, idpValueSource aSrc) 
{
    void *sValue = NULL;
    UInt sValueIdx;

    // Multiple Flag Check
    IDE_TEST_RAISE(((mAttr & IDP_ATTR_ML_MASK) == IDP_ATTR_ML_JUSTONE) &&
                   mSrcValArr[aSrc].mCount == 1, only_one_error);
    
    // Store Count Check
    IDE_TEST_RAISE(mSrcValArr[aSrc].mCount >= IDP_MAX_VALUE_COUNT,
                   no_more_insert);
    
    switch(aSrc)
    {
        case IDP_VALUE_FROM_PFILE:
            IDE_TEST_RAISE((mAttr & IDP_ATTR_SL_PFILE) != IDP_ATTR_SL_PFILE, 
                            err_cannot_set_from_pfile);
            break;
            
        case IDP_VALUE_FROM_SPFILE_BY_ASTERISK:
            IDE_TEST_RAISE((mAttr & IDP_ATTR_SL_SPFILE) != IDP_ATTR_SL_SPFILE, 
                            err_cannot_set_from_spfile);
            break;
            
        case IDP_VALUE_FROM_SPFILE_BY_SID:
            IDE_TEST_RAISE((mAttr & IDP_ATTR_SL_SPFILE) != IDP_ATTR_SL_SPFILE, 
                            err_cannot_set_from_spfile);
            break;        
        
        case IDP_VALUE_FROM_ENV:
            IDE_TEST_RAISE((mAttr & IDP_ATTR_SL_ENV) != IDP_ATTR_SL_ENV, 
                            err_cannot_set_from_env);
            break;
        
        default:
            //IDP_VALUE_FROM_DEFAULT NO CHECK 
            break;
    }

    IDE_TEST(convertFromString(aValue, &sValue) != IDE_SUCCESS);

    /* Value Range Validation */
    IDE_TEST(checkRange(sValue) != IDE_SUCCESS);
    
    sValueIdx = mSrcValArr[aSrc].mCount++;
    mSrcValArr[aSrc].mVal[sValueIdx] = sValue;

    return IDE_SUCCESS;
    
    IDE_EXCEPTION(only_one_error);
    {
        idlOS::snprintf(mErrorBuf, 
                        IDP_ERROR_BUF_SIZE, 
                        "idp insertBySrc() Error : "
                        "Property [%s] Can't Store Multiple Values.",
                        getName());
    }
    IDE_EXCEPTION(no_more_insert);
    {
        idlOS::snprintf(mErrorBuf, 
                        IDP_ERROR_BUF_SIZE, 
                        "idp insertBySrc() Error : "
                        "Property [%s] Can't Store more than %"ID_UINT32_FMT
                        " Values.",
                        getName(), 
                        (UInt)IDP_MAX_VALUE_COUNT);
    }
    IDE_EXCEPTION(err_cannot_set_from_pfile);
    {
        idlOS::snprintf(mErrorBuf, 
                        IDP_ERROR_BUF_SIZE, 
                        "idp insertBySrc() Error : "
                        "Property [%s] Can't set from PFILE.", 
                        getName());
    }
    IDE_EXCEPTION(err_cannot_set_from_spfile);
    {
        idlOS::snprintf(mErrorBuf, 
                        IDP_ERROR_BUF_SIZE, 
                        "idp insertBySrc() Error : "
                        "Property [%s] Can't set from SPFILE.", 
                        getName());
    }
    IDE_EXCEPTION(err_cannot_set_from_env);
    {
        idlOS::snprintf(mErrorBuf, 
                        IDP_ERROR_BUF_SIZE, 
                        "idp insertBySrc() Error : "
                        "Property [%s] Can't set from ENV.", 
                        getName());
    }
    
    IDE_EXCEPTION_END;
    
    if (sValue != NULL)
    {
        iduMemMgr::freeRaw(sValue);
    }
    
    return IDE_FAILURE;
}

/******************************************************************************************
*
* Description :
*  aSrc�� ���� value source ��ġ�� Raw ������ aValue 
*  ���� �����Ͽ� �����Ѵ�.
*
*  SChar         *aValue,   - [IN] �����Ϸ����ϴ� ���� ������ (Ÿ�Ժ� Raw Format)
*  idpValueSource aSrc      - [IN] ���� ������ Source ��ġ
*                                  (default/env/pfile/spfile by asterisk, spfile by sid)
*******************************************************************************************/ 
IDE_RC idpBase::insertRawBySrc(void *aValue, idpValueSource aSrc) 
{
    void *sValue = NULL;    
    UInt sValueIdx;
    
    // Multiple Flag Check
    IDE_TEST_RAISE(((mAttr & IDP_ATTR_ML_MASK) == IDP_ATTR_ML_JUSTONE) &&
                   mSrcValArr[aSrc].mCount == 1, only_one_error);
    
    // Store Count Check
    IDE_TEST_RAISE(mSrcValArr[aSrc].mCount >= IDP_MAX_VALUE_COUNT,
                   no_more_insert);
    
    switch(aSrc)
    {
        case IDP_VALUE_FROM_PFILE:
            IDE_TEST_RAISE((mAttr & IDP_ATTR_SL_PFILE) != IDP_ATTR_SL_PFILE, 
                            err_cannot_set_from_pfile);
            break;
            
        case IDP_VALUE_FROM_SPFILE_BY_ASTERISK:
            IDE_TEST_RAISE((mAttr & IDP_ATTR_SL_SPFILE) != IDP_ATTR_SL_SPFILE, 
                            err_cannot_set_from_spfile);
            break;
            
        case IDP_VALUE_FROM_SPFILE_BY_SID:
            IDE_TEST_RAISE((mAttr & IDP_ATTR_SL_SPFILE) != IDP_ATTR_SL_SPFILE, 
                            err_cannot_set_from_spfile);
            break;        
        
        case IDP_VALUE_FROM_ENV:
            IDE_TEST_RAISE((mAttr & IDP_ATTR_SL_ENV) != IDP_ATTR_SL_ENV, 
                            err_cannot_set_from_env);
            break;
        
        default:
            //IDP_VALUE_FROM_DEFAULT NO CHECK 
            break;
    }

    cloneValue(this, aValue, &sValue);
    
    /* Value Range Validation */
    IDE_TEST(checkRange(aValue) != IDE_SUCCESS);
    
    sValueIdx = mSrcValArr[aSrc].mCount++;
    mSrcValArr[aSrc].mVal[sValueIdx] = aValue;
    
    return IDE_SUCCESS;
    
    IDE_EXCEPTION(only_one_error);
    {
        idlOS::snprintf(mErrorBuf, 
                        IDP_ERROR_BUF_SIZE, 
                        "idp insert() Error : "
                        "Property [%s] Can't Store Multiple Values.",
                        getName());
    }
    IDE_EXCEPTION(no_more_insert);
    {
        idlOS::snprintf(mErrorBuf, 
                        IDP_ERROR_BUF_SIZE, 
                        "idp insert() Error : "
                        "Property [%s] Can't Store more than %"ID_UINT32_FMT" Values.",
                        getName(), 
                        (UInt)IDP_MAX_VALUE_COUNT);
    }
    IDE_EXCEPTION(err_cannot_set_from_pfile);
    {
        idlOS::snprintf(mErrorBuf, 
                        IDP_ERROR_BUF_SIZE, 
                        "idp insertBySrc() Error : "
                        "Property [%s] Can't set from PFILE.", 
                        getName());
    }
    IDE_EXCEPTION(err_cannot_set_from_spfile);
    {
        idlOS::snprintf(mErrorBuf, 
                        IDP_ERROR_BUF_SIZE, 
                        "idp insertBySrc() Error : "
                        "Property [%s] Can't set from SPFILE.", 
                        getName());
    }
    IDE_EXCEPTION(err_cannot_set_from_env);
    {
        idlOS::snprintf(mErrorBuf, 
                        IDP_ERROR_BUF_SIZE, 
                        "idp insertBySrc() Error : "
                        "Property [%s] Can't set from ENV.", 
                        getName());
    }    
    IDE_EXCEPTION_END;
        
    if (sValue != NULL)
    {
        iduMemMgr::freeRaw(sValue);
    }
    
    return IDE_FAILURE;
}

IDE_RC idpBase::read(void *aOut, UInt aNum)
{
    idBool sLocked = ID_FALSE;


    IDE_ASSERT( idlOS::thread_mutex_lock(&mMutex) == 0);
    sLocked = ID_TRUE;


    IDE_TEST_RAISE(mMemVal.mCount == 0, not_initialized);

    // ī��Ʈ�� Ʋ�� ���.
    IDE_TEST_RAISE(aNum >= mMemVal.mCount, no_exist_error);

    idlOS::memcpy(aOut, mMemVal.mVal[aNum], getSize(mMemVal.mVal[aNum]));

    sLocked = ID_FALSE;
    IDE_ASSERT( idlOS::thread_mutex_unlock(&mMutex) == 0);

    return IDE_SUCCESS;
    IDE_EXCEPTION(no_exist_error);
    {
        IDE_SET(ideSetErrorCode(idERR_ABORT_idp_NoSuchEntry, getName(), aNum));
    }
    IDE_EXCEPTION(not_initialized);
    {
        idlOS::snprintf(mErrorBuf, IDP_ERROR_BUF_SIZE, "idp read() Error : Property not loaded\n");
    }
    IDE_EXCEPTION_END;

    if ( sLocked == ID_TRUE )
    {
        IDE_ASSERT( idlOS::thread_mutex_unlock(&mMutex) == 0);
    }

    return IDE_FAILURE;
}

IDE_RC idpBase::readBySrc(void *aOut, idpValueSource aSrc, UInt aNum)
{
    idBool sLocked = ID_FALSE;


    IDE_ASSERT( idlOS::thread_mutex_lock(&mMutex) == 0);
    sLocked = ID_TRUE;

    IDE_TEST_RAISE(mSrcValArr[aSrc].mCount == 0, not_initialized);

    // ī��Ʈ�� Ʋ�� ���.
    IDE_TEST_RAISE(aNum >= mSrcValArr[aSrc].mCount , no_exist_error);

    idlOS::memcpy(aOut, mSrcValArr[aSrc].mVal[aNum], getSize(mSrcValArr[aSrc].mVal[aNum]));

    sLocked = ID_FALSE;
    IDE_ASSERT( idlOS::thread_mutex_unlock(&mMutex) == 0);

    return IDE_SUCCESS;
    IDE_EXCEPTION(no_exist_error);
    {
        IDE_SET(ideSetErrorCode(idERR_ABORT_idp_NoSuchEntry, getName(), aNum));
    }
    IDE_EXCEPTION(not_initialized);
    {
        idlOS::snprintf(mErrorBuf, IDP_ERROR_BUF_SIZE, "idp readBySrc() Error : Property not loaded\n");
    }
    IDE_EXCEPTION_END;

    if ( sLocked == ID_TRUE )
    {
        IDE_ASSERT( idlOS::thread_mutex_unlock(&mMutex) == 0);
    }

    return IDE_FAILURE;
}

IDE_RC idpBase::readPtr(void **aOut, UInt aNum)
{
    idBool sLocked = ID_FALSE;

    IDE_ASSERT( idlOS::thread_mutex_lock(&mMutex) == 0);
    sLocked = ID_TRUE;

    IDE_TEST_RAISE(mMemVal.mCount == 0, not_initialized);

    // ī��Ʈ�� Ʋ�� ���.
    IDE_TEST_RAISE(aNum >= mMemVal.mCount, no_exist_error);

    // ���� ������ ��� �˻�
    //IDE_TEST_RAISE( (mAttr & IDP_ATTR_RD_MASK) == IDP_ATTR_RD_WRITABLE,
    //                cant_read_error);

    // String�� ��츸 ������.
    IDE_TEST_RAISE( (mAttr & IDP_ATTR_TP_MASK) != IDP_ATTR_TP_String,
                    cant_read_error);

    *aOut= mMemVal.mVal[aNum];

    sLocked = ID_FALSE;
    IDE_ASSERT( idlOS::thread_mutex_unlock(&mMutex) == 0);

    return IDE_SUCCESS;

    IDE_EXCEPTION(cant_read_error);
    {
        IDE_SET(ideSetErrorCode(idERR_ABORT_idp_NotReadOnly, getName()));
    }
    IDE_EXCEPTION(no_exist_error);
    {
        IDE_SET(ideSetErrorCode(idERR_ABORT_idp_NoSuchEntry, getName(), aNum));
    }
    IDE_EXCEPTION(not_initialized);
    {
        idlOS::snprintf(mErrorBuf, IDP_ERROR_BUF_SIZE, "idp read() Error : Property not loaded\n");
    }

    IDE_EXCEPTION_END;

    if ( sLocked == ID_TRUE )
    {
        IDE_ASSERT( idlOS::thread_mutex_unlock(&mMutex) == 0);
    }

    return IDE_FAILURE;
}

/******************************************************************************************
*
* Description :
* Ư�� source�� ��ġ�� ����� ���� �����͸� ��´�.
* �� ���� ���� ������ �����̸�, ȣ���ڰ� ������ ������ Ÿ������ �����ؼ�
* ����ؾ� �Ѵ�.
* ��� ������Ƽ��, String Ÿ���� ��쿡�� ��ȿ�ϴ�.
*
* UInt           aNum,      - [IN]  ������Ƽ�� ����� n ��° ���� �ǹ��ϴ� �ε���  
* idpValueSource aSrc,      - [IN]  � ���� ����� ���ؼ� ������ �������� ��Ÿ���� Value Source                                   
* void         **aOut,      - [OUT] ��� ���� ������
*******************************************************************************************/ 
IDE_RC idpBase::readPtrBySrc (UInt aNum, idpValueSource aSrc, void **aOut)
{
    idBool sLocked = ID_FALSE;

    IDE_ASSERT( idlOS::thread_mutex_lock(&mMutex) == 0);
    sLocked = ID_TRUE;

    IDE_TEST_RAISE(mSrcValArr[aSrc].mCount == 0, not_initialized);

    // ī��Ʈ�� Ʋ�� ���.
    IDE_TEST_RAISE(aNum >= mSrcValArr[aSrc].mCount , no_exist_error);

    // ���� ������ ��� �˻�
    //IDE_TEST_RAISE( (mAttr & IDP_ATTR_RD_MASK) == IDP_ATTR_RD_WRITABLE,
    //                cant_read_error);

    // String�� ��츸 ������.
    IDE_TEST_RAISE( (mAttr & IDP_ATTR_TP_MASK) != IDP_ATTR_TP_String,
                    cant_read_error);

    *aOut = (void*) mSrcValArr[aSrc].mVal[aNum];

    sLocked = ID_FALSE;
    IDE_ASSERT( idlOS::thread_mutex_unlock(&mMutex) == 0);

    return IDE_SUCCESS;

    IDE_EXCEPTION(cant_read_error);
    {
        IDE_SET(ideSetErrorCode(idERR_ABORT_idp_NotReadOnly, getName()));
    }
    IDE_EXCEPTION(no_exist_error);
    {
        IDE_SET(ideSetErrorCode(idERR_ABORT_idp_NoSuchEntry, getName(), aNum));
    }
    IDE_EXCEPTION(not_initialized);
    {
        idlOS::snprintf(mErrorBuf, IDP_ERROR_BUF_SIZE, "idp read() Error : Property not loaded\n");
    }

    IDE_EXCEPTION_END;

    if ( sLocked == ID_TRUE )
    {
        IDE_ASSERT( idlOS::thread_mutex_unlock(&mMutex) == 0);
    }

    return IDE_FAILURE;
}

/******************************************************************************************
*
* Description :
*  idp������ ����ϱ� ���� �Լ��̸�, ���������� lock�� ����� ������, 
*  �ٸ� ��⿡���� �� �Լ��� ���ؼ� ���� �������� �ȵȴ�. 
*  aNum ��° ���� ��ȯ�Ѵ�. 
*
*  UInt       aNum   - [IN] �� ��° ������ ��Ÿ���� index
*  void     **aOut   - [OUT] ��ȯ�Ǵ� ��
*******************************************************************************************/ 
IDE_RC idpBase::readPtr4Internal(UInt aNum, void **aOut)
{
    IDE_TEST_RAISE(mMemVal.mCount == 0, not_initialized);
    
    // ī��Ʈ�� Ʋ�� ���.
    IDE_TEST_RAISE(aNum >= mMemVal.mCount, err_no_exist);
   
    *aOut= mMemVal.mVal[aNum];
    
    return IDE_SUCCESS;
    
    IDE_EXCEPTION(err_no_exist);
    {
        idlOS::snprintf(mErrorBuf, IDP_ERROR_BUF_SIZE, 
                        "The entry [<%d>] of the property [<%s>] does not exist.\n", 
                        aNum, getName());
    }
    IDE_EXCEPTION(not_initialized);
    {
        idlOS::snprintf(mErrorBuf, 
                        IDP_ERROR_BUF_SIZE, 
                        "idp read() Error : Property not loaded\n");
    }
    
    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

// BUG-43533 OPTIMIZER_FEATURE_ENABLE
IDE_RC idpBase::update4Startup(idvSQL *aStatistics, SChar *aIn, UInt aNum, void *aArg)
{
    // BUG-43533
    // Property file, ȯ�溯�� � ������� ���� property�� �����Ѵ�.
    if ( (mSrcValArr[IDP_VALUE_FROM_PFILE].mCount == 0) &&
         (mSrcValArr[IDP_VALUE_FROM_ENV].mCount == 0) &&
         (mSrcValArr[IDP_VALUE_FROM_SPFILE_BY_ASTERISK].mCount == 0) &&
         (mSrcValArr[IDP_VALUE_FROM_SPFILE_BY_SID].mCount == 0) )
    {
        IDE_TEST( update( aStatistics, aIn, aNum, aArg )
                  != IDE_SUCCESS );
    }
    else
    {
        // Nothing to do.
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC idpBase::update(idvSQL *aStatistics, SChar *aIn, UInt aNum, void *aArg)
{
    void  *sValue       = NULL;
    void  *sOldValue    = NULL;
    UInt   sUpdateValue = 0;
    idBool sLocked      = ID_FALSE;

    IDE_ASSERT( idlOS::thread_mutex_lock(&mMutex) == 0);
    sLocked = ID_TRUE;

    // ī��Ʈ �˻�
    IDE_TEST_RAISE(aNum >= mMemVal.mCount, no_exist_error);

    // ���� �Ұ����� ��� �˻�
    IDE_TEST_RAISE( (mAttr & IDP_ATTR_RD_MASK) == IDP_ATTR_RD_READONLY,
                    cant_modify_error);

    if ( ( mAttr & IDP_ATTR_SK_MASK ) != IDP_ATTR_SK_MULTI_BYTE )
    {
        IDE_TEST(convertFromString(aIn, &sValue) != IDE_SUCCESS);
    }
    else
    {
        sValue = iduMemMgr::mallocRaw(idlOS::strlen((SChar *)aIn) + 1, IDU_MEM_FORCE);
        IDE_ASSERT(sValue != NULL);

        idlOS::strncpy((SChar *)sValue, (SChar *)aIn, idlOS::strlen((SChar *)aIn) + 1);
    }

    sOldValue = mMemVal.mVal[aNum];

    /* Value Range Validation */
    IDE_TEST(checkRange(sValue) != IDE_SUCCESS);

    IDE_TEST( mUpdateBefore(aStatistics, getName(), sOldValue, sValue, aArg)
              != IDE_SUCCESS);

    /* ���� �޸� ���� & Set */
    mMemVal.mVal[aNum] = sValue;
    /* ������Ƽ ���� �ٲپ��ٰ� ǥ���ϰ� �����߻��� ���������� �����Ѵ�. */
    sUpdateValue = 1;

    IDE_TEST( mUpdateAfter(aStatistics, getName(), sOldValue, sValue, aArg)
              != IDE_SUCCESS);

    iduMemMgr::freeRaw(sOldValue);

    sLocked = ID_FALSE;
    IDE_ASSERT( idlOS::thread_mutex_unlock(&mMutex) == 0);

    return IDE_SUCCESS;

    IDE_EXCEPTION(cant_modify_error);
    {
        IDE_SET(ideSetErrorCode(idERR_ABORT_idp_ReadOnlyEntry, getName()));
    }
    IDE_EXCEPTION(no_exist_error);
    {
        IDE_SET(ideSetErrorCode(idERR_ABORT_idp_NameNotFound, getName()));
    }
    IDE_EXCEPTION_END;

    /* BUG-17763: idpBase::update()���� FMR�� ���߽�Ű�� �ֽ��ϴ�.
     *
     * ������ �������� �����߻��� Free���Ѽ� ������ �Ǿ����ϴ�.
     * Free��Ű�� �ʰ�
     * �����߻��� ������Ƽ ���� ���� ������ �����ؾ� �մϴ�. */
    if( sUpdateValue ==  1 )
    {
        mMemVal.mVal[aNum] = sOldValue;
    }

    if (sValue != NULL)
    {
        iduMemMgr::freeRaw(sValue);
    }

    if ( sLocked == ID_TRUE )
    {
        IDE_ASSERT( idlOS::thread_mutex_unlock(&mMutex) == 0);
    }

    return IDE_FAILURE;
}

IDE_RC idpBase::updateForce(SChar *aIn, UInt aNum, void *aArg)
{
    void  *sValue       = NULL;
    void  *sOldValue    = NULL;
    UInt   sUpdateValue = 0;
    idBool sLocked      = ID_FALSE;

    IDE_ASSERT( idlOS::thread_mutex_lock(&mMutex) == 0);
    sLocked = ID_TRUE;

    // ī��Ʈ �˻�
    IDE_TEST_RAISE(aNum >= mMemVal.mCount, no_exist_error);

    /* ���� �Ұ����� ��츦 �˻����� �ʴ´�.
    IDE_TEST_RAISE( (mAttr & IDP_ATTR_RD_MASK) == IDP_ATTR_RD_READONLY,
                    cant_modify_error);
                    */

    if ( ( mAttr & IDP_ATTR_SK_MASK ) != IDP_ATTR_SK_MULTI_BYTE )
    {
    IDE_TEST(convertFromString(aIn, &sValue) != IDE_SUCCESS);
    }
    else
    {
        sValue = iduMemMgr::mallocRaw(idlOS::strlen((SChar *)aIn) + 1, IDU_MEM_FORCE);
        IDE_ASSERT(sValue != NULL);

        idlOS::strncpy((SChar *)sValue, (SChar *)aIn, idlOS::strlen((SChar *)aIn) + 1);
    }

    sOldValue = mMemVal.mVal[aNum];

    /* Value Range Validation */
    IDE_TEST(checkRange(sValue) != IDE_SUCCESS);

    IDE_TEST( mUpdateBefore(NULL, getName(), sOldValue, sValue, aArg)
              != IDE_SUCCESS);

    /* ���� �޸� ���� & Set */
    mMemVal.mVal[aNum] = sValue;
    /* ������Ƽ ���� �ٲپ��ٰ� ǥ���ϰ� �����߻��� ���������� �����Ѵ�. */
    sUpdateValue = 1;

    IDE_TEST( mUpdateAfter(NULL, getName(), sOldValue, sValue, aArg)
              != IDE_SUCCESS);

    iduMemMgr::freeRaw(sOldValue);

    sLocked = ID_FALSE;
    IDE_ASSERT( idlOS::thread_mutex_unlock(&mMutex) == 0);

    return IDE_SUCCESS;

    /* ���� �Ұ����� ��츦 �˻����� �ʴ´�.
    IDE_EXCEPTION(cant_modify_error);
    {
        IDE_SET(ideSetErrorCode(idERR_ABORT_idp_ReadOnlyEntry, getName()));
    }
    */
    IDE_EXCEPTION(no_exist_error);
    {
        IDE_SET(ideSetErrorCode(idERR_ABORT_idp_NameNotFound, getName()));
    }
    IDE_EXCEPTION_END;

    /* BUG-17763: idpBase::update()���� FMR�� ���߽�Ű�� �ֽ��ϴ�.
     *
     * ������ �������� �����߻��� Free���Ѽ� ������ �Ǿ����ϴ�.
     * Free��Ű�� �ʰ�
     * �����߻��� ������Ƽ ���� ���� ������ �����ؾ� �մϴ�. */
    if( sUpdateValue ==  1 )
    {
        mMemVal.mVal[aNum] = sOldValue;
    }

    if (sValue != NULL)
    {
        iduMemMgr::freeRaw(sValue);
    }

    if ( sLocked == ID_TRUE )
    {
        IDE_ASSERT( idlOS::thread_mutex_unlock(&mMutex) == 0);
    }

    return IDE_FAILURE;
}

/**************************************************************************
 * Description:
 *     1.aIn�� �ش��ϴ� ���� Property�� Min, Max���̿� �ִ��� �����Ѵ�.
 *     2.aIsSystem�� �ش��ϴ� Property�� �Ӽ� üũ
 *           SYSTEM, SESSION PROPERTY �����ϰ� üũ�ϱ� ������ �� �Ӽ���
 *           �ٸ� ��� �����Ͽ� ó��
 *           ex> DATE_FORMAT, TIME_ZONE
 *     3.SHARD USER, LIBRARY, COORDINATION SESSION Property attribute����
 * aIn       - [IN] Input Value
 * aIsSystem - [IN] System property 
 *************************************************************************/
IDE_RC idpBase::validate( SChar * aIn,
                          idBool  aIsSystem )
{
    void  * sValue    = NULL;
    idBool  sLocked   = ID_FALSE;

    IDE_ASSERT( idlOS::thread_mutex_lock(&mMutex) == 0);
    sLocked = ID_TRUE;

    if ( aIsSystem == ID_TRUE )
    {
        // ���� �Ұ����� ��� �˻�
        IDE_TEST_RAISE( (mAttr & IDP_ATTR_RD_MASK) == IDP_ATTR_RD_READONLY,
                        cant_modify_error);
    }
    else
    {
        // nothing to do
    }

    IDE_TEST(convertFromString(aIn, &sValue) != IDE_SUCCESS);

    /* Value Range Validation */
    IDE_TEST(checkRange(sValue) != IDE_SUCCESS);

    // BUG-20486
    iduMemMgr::freeRaw(sValue);
    sValue = NULL;

    sLocked = ID_FALSE;
    IDE_ASSERT( idlOS::thread_mutex_unlock(&mMutex) == 0);

    return IDE_SUCCESS;

    IDE_EXCEPTION(cant_modify_error);
    {
        IDE_SET(ideSetErrorCode(idERR_ABORT_idp_ReadOnlyEntry, getName()));
    }
    IDE_EXCEPTION_END;

    // BUG-20486
    if(sValue != NULL)
    {
        iduMemMgr::freeRaw(sValue);
        sValue = NULL;
    }

    if ( sLocked == ID_TRUE )
    {
        IDE_PUSH();
        IDE_ASSERT( idlOS::thread_mutex_unlock(&mMutex) == 0);
        IDE_POP();
    }

    return IDE_FAILURE;
}

// PROJ-2727
IDE_RC idpBase::getPropertyAttribute( UInt * aOut )
{
    IDE_ASSERT( idlOS::thread_mutex_lock(&mMutex) == 0);
        
    if (( mAttr & IDP_ATTR_SH_MASK ) == IDP_ATTR_SH_ALL )
    {
        *aOut = IDP_ATTR_SHARD_ALL;
    }
    else if (( mAttr & IDP_ATTR_SH_MASK ) == IDP_ATTR_SH_USER )
    {
        *aOut = IDP_ATTR_SHARD_USER;
    }
    else if (( mAttr & IDP_ATTR_SH_MASK ) == IDP_ATTR_SH_COORD )
    {
        *aOut = IDP_ATTR_SHARD_COORD;
    }
    else
    {
        *aOut = IDP_ATTR_SHARD_LIB;
    }
    
    IDE_ASSERT( idlOS::thread_mutex_unlock(&mMutex) == 0);

    return IDE_SUCCESS;
}

IDE_RC idpBase::verifyInsertedValues()
{
    UInt i;
    UInt sMultiple;

    // [1] Check Multiple Value Consistency, ������ ���� ��Ȯ�� n�������ϴ� �� Ȯ��
    sMultiple = mAttr & IDP_ATTR_ML_MASK;

    if ( (sMultiple != IDP_ATTR_ML_JUSTONE) &&
         (sMultiple != IDP_ATTR_ML_MULTIPLE)
       ) 
    {
        /* ���� ������ ���� �˻� ��� */

        UInt sCurCount;

        sCurCount = (UInt)IDP_ATTR_ML_COUNT(mAttr); /* ������ ���� */

        IDE_TEST_RAISE(sCurCount != mMemVal.mCount, multiple_count_error);

    }

    // [3] Check Range of Insered Value

    for (i = 0; i < mMemVal.mCount; i++)
    {
        IDE_TEST(checkRange(mMemVal.mVal[i]) != IDE_SUCCESS);
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION(multiple_count_error);
    {
        idlOS::snprintf(mErrorBuf, IDP_ERROR_BUF_SIZE, "idp verifyInsertedValues() Error : "
                        "Property [%s] must have %"ID_UINT32_FMT" Value Entries."
                        " But, %"ID_UINT32_FMT" Entries Exist",
                        getName(), (UInt)IDP_ATTR_ML_COUNT(mAttr), mMemVal.mCount);
    }

    IDE_EXCEPTION_END;
    return IDE_FAILURE;
}

/*
 * ��ũ���Ͱ� ��ϵ� ���� ȣ���.
 * �� �Լ����� ������ ���� Conf�� ���� �ԷµǱ� ��������
 * ���� �۾��� �̷������ ��.
 */

void idpBase::registCallback()
{
    /*
      Environment Check & Read
      [ALTIBASE_]PROPNAME
    */

    void  *sValue;
    SChar *sEnvName;
    SChar *sEnvValue;
    UInt   sLen;

    sLen = idlOS::strlen(IDP_PROPERTY_PREFIX) + idlOS::strlen(getName()) + 2;

    sEnvName = (SChar *)iduMemMgr::mallocRaw(sLen);

    IDE_ASSERT(sEnvName != NULL);

    idlOS::memset(sEnvName, 0, sLen);

    idlOS::snprintf(sEnvName, sLen, "%s%s", IDP_PROPERTY_PREFIX, getName());

    sEnvValue = idlOS::getenv( (const SChar *)sEnvName);

    // Re-Validation of return-Value
    if (sEnvValue != NULL)
    {
        if (idlOS::strlen(sEnvValue) == 0)
        {
            sEnvValue = NULL;
        }
    }

    // If Exist, Read It.
    if (sEnvValue != NULL)
    {
        sValue = NULL;

        if (convertFromString(sEnvValue, &sValue) == IDE_SUCCESS)
        {
            mSrcValArr[IDP_VALUE_FROM_ENV].mVal[0] = sValue;
            mSrcValArr[IDP_VALUE_FROM_ENV].mCount++;
        }
        else
        {
            /* ------------------------------------------------
             *  ȯ�溯�� ������Ƽ�� �� ��Ʈ����
             *  Data Type�� ���� �ʾ� ������ ��쿡��
             *  Default Value�� �״�� ����.
             * ----------------------------------------------*/
            ideLog::log(IDE_SERVER_0, ID_TRC_PROPERTY_TYPE_INVALID, sEnvName, sEnvValue);
        }
    }

    iduMemMgr::freeRaw(sEnvName);

}

UInt idpBase::convertToChar(void        *aBaseObj,
                            void        *aMember,
                            UChar       *aBuf,
                            UInt         aBufSize)
{
    idpBase *sBase = (idpBase *)(((idpBaseInfo *)aBaseObj)->mBase);

    return sBase->convertToString(aMember, aBuf, aBufSize);
}

