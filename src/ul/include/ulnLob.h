/**
 *  Copyright (c) 1999~2017, Altibase Corp. and/or its affiliates. All rights reserved.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License, version 3,
 *  as published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _O_ULN_LOB_H_
#define _O_ULN_LOB_H_ 1

#include <ulnPrivate.h>

#define ULN_LOB_PIECE_SIZE  (32000)

typedef struct ulnLobBuffer ulnLobBuffer;

/*
 * ======================================
 * ulnLobBuffer
 * ======================================
 *
 * gUlnLobBufferOp ������ ulnLobBufferOp �� �Լ����� ���ǵǾ� �ִ�.
 */

typedef struct ulnLobBufferFile
{
    acp_char_t   *mFileName;
    acp_uint32_t  mFileOption;
    acp_file_t    mFileHandle;
    acp_uint32_t  mFileSize;

    acp_uint8_t  *mTempBuffer;
} ulnLobBufferFile;

typedef struct ulnLobBufferMemory
{
    acp_uint8_t  *mBuffer;
    acp_uint32_t  mBufferSize;
    acp_uint32_t  mCurrentOffset;    /* �������� �аų� �� offset. ������ ���ۺ��� ����� */
} ulnLobBufferMemory;

typedef ACI_RC ulnLobBufferInitializeFunc(ulnLobBuffer *aLob,
                                          acp_uint8_t  *aBufferPtrOrFileName,
                                          acp_uint32_t  aBufferSizeOrFileOption);

typedef ACI_RC ulnLobBufferPrepareFunc(ulnFnContext *aFnContext, ulnLobBuffer *aBuffer);

typedef ACI_RC ulnLobBufferDataOutFunc(ulnFnContext *aFnContext,
                                       ulnPtContext *aPtContext,
                                       ulnLob       *aLob);

typedef ACI_RC ulnLobBufferDataInFunc(cmtVariable  *aCmVariable,
                                      acp_uint32_t  aOffset,
                                      acp_uint32_t  aReceivedDataSize,
                                      acp_uint8_t  *aReceivedData,
                                      void         *aContext);

typedef ACI_RC ulnLobBufferFinalizeFunc(ulnFnContext *aFnContext, ulnLobBuffer *aBuffer);

typedef acp_uint32_t ulnLobBufferGetSizeFunc(ulnLobBuffer *aBuffer);

typedef struct ulnLobBufferOp
{
    ulnLobBufferInitializeFunc  *mInitialize;
    ulnLobBufferPrepareFunc     *mPrepare;
    ulnLobBufferDataInFunc      *mDataIn;
    ulnLobBufferDataOutFunc     *mDataOut;
    ulnLobBufferFinalizeFunc    *mFinalize;

    ulnLobBufferGetSizeFunc     *mGetSize;
} ulnLobBufferOp;

typedef enum
{
    ULN_LOB_BUFFER_TYPE_FILE = 1,   /* �Ϻη� 1 ���� ������ */
    ULN_LOB_BUFFER_TYPE_CHAR,
    ULN_LOB_BUFFER_TYPE_WCHAR,
    ULN_LOB_BUFFER_TYPE_BINARY,
    ULN_LOB_BUFFER_TYPE_MAX
} ulnLobBufferType;

struct ulnLobBuffer
{
    ulnLobBufferType  mType;
    ulnLobBufferOp   *mOp;
    ulnLob           *mLob;

    union
    {
        ulnLobBufferMemory mMemory;
        ulnLobBufferFile   mFile;
    } mObject;

    /* PROJ-2047 Strengthening LOB - Partial Converting */
    ulnCharSet             mCharSet;
};

/*
 * ======================================
 * ulnLob
 * ======================================
 *
 * gUlnLobOp ������ ulnLobOp �� �Լ����� ���ǵǾ� �ִ�.
 */

typedef enum
{
    ULN_LOB_ST_INITIALIZED = 1,     /* �Ϻη� 1 ���� ������ */
    ULN_LOB_ST_LOCATOR,
    ULN_LOB_ST_OPENED
} ulnLobState;

typedef enum
{
    ULN_LOB_TYPE_BLOB = 1,      /* �Ϻη� 1 ���� ���� */
    ULN_LOB_TYPE_CLOB,
    ULN_LOB_TYPE_MAX
} ulnLobType;

typedef ACI_RC ulnLobOpenFunc(ulnFnContext *aFnContext,
                              ulnPtContext *aPtContext,
                              ulnLob *aLob);

typedef ACI_RC ulnLobCloseFunc(ulnFnContext *aFnContext,
                               ulnPtContext *aPtContext,
                               ulnLob *aLob);

typedef ACI_RC ulnLobAppendFunc(ulnFnContext *aFnContext,
                                ulnPtContext *aPtContext,
                                ulnLob       *aLob,
                                ulnLobBuffer *aLobBuffer);

typedef ACI_RC ulnLobOverWriteFunc(ulnFnContext *aFnContext,
                                   ulnPtContext *aPtContext,
                                   ulnLob       *aLob,
                                   ulnLobBuffer *aLobBuffer);

typedef ACI_RC ulnLobUpdateFunc(ulnFnContext *aFnContext,
                                ulnPtContext *aPtContext,
                                ulnLob       *aLob,
                                ulnLobBuffer *aLobBuffer,
                                acp_uint32_t  aStartOffset,
                                acp_uint32_t  aLengthToUpdate);

typedef ACI_RC ulnLobGetDataFunc(ulnFnContext *aFnContext,
                                 ulnPtContext *aPtContext,
                                 ulnLob       *aLob,
                                 ulnLobBuffer *aLobBuffer,
                                 acp_uint32_t  aStartOffset,
                                 acp_uint32_t  aSizeToGet);

typedef ACI_RC ulnLobSetLocatorFunc(ulnFnContext *aFnContext,
                                    ulnLob       *aLob,
                                    acp_uint64_t  aLocatorID);

typedef ACI_RC ulnLobTrimFunc(ulnFnContext *aFnContext,
                              ulnPtContext *aPtContext,
                              ulnLob       *aLob,
                              acp_uint32_t  aStartOffset);

typedef ulnLobState ulnLobGetStateFunc(ulnLob *aLob);

typedef ACI_RC ulnLobPrepare4WriteFunc(ulnFnContext *aFnContext,
                                       ulnPtContext *aPtContext,
                                       ulnLob       *aLob,
                                       acp_uint32_t  aStartOffset,
                                       acp_uint32_t  aNewSize);

typedef ACI_RC ulnLobWriteFunc(ulnFnContext *aFnContext,
                               ulnPtContext *aPtContext,
                               ulnLob       *aLob,
                               ulnLobBuffer *aLobBuffer);

typedef ACI_RC ulnLobFinishWriteFunc(ulnFnContext *aFnContext,
                                     ulnPtContext *aPtContext,
                                     ulnLob       *aLob);

typedef struct ulnLobOp
{
    ulnLobGetStateFunc    *mGetState;

    ulnLobSetLocatorFunc  *mSetLocator;

    ulnLobOpenFunc        *mOpen;
    ulnLobCloseFunc       *mClose;

    ulnLobOverWriteFunc   *mOverWrite;
    ulnLobAppendFunc      *mAppend;
    ulnLobUpdateFunc      *mUpdate;

    ulnLobGetDataFunc     *mGetData;

    /* PROJ-2047 Strengthening LOB - Added Interfaces */
    ulnLobTrimFunc        *mTrim;

    /* PROJ-2728 Sharding LOB */
    ulnLobPrepare4WriteFunc *mPrepare4Write;
    ulnLobWriteFunc         *mWrite;
    ulnLobFinishWriteFunc   *mFinishWrite;
} ulnLobOp;

/* BUG-21932  The ulnLob structure should consider 4 byte alignment for 32bit-GCC */
#ifdef COMPILE_64BIT
typedef acp_uint64_t ulnLobLocator;
#else
/* 32 bit  GCC �÷������� structure����
   acp_uint64_t��  4byte align����ó���Ǵ� case ���־�
   ���� ���� 4byteũ�� UInt type 2���� constain�ϴ� structure�� �����Ѵ� */
typedef struct ulnLobLocator
{
    acp_uint32_t mHigh;
    acp_uint32_t mLow;
}ulnLobLocator;
#endif


#ifdef COMPILE_64BIT
/* a value��  ULong�̴�. . */
#define ULN_SET_LOB_LOCATOR(aDest,aSrcVal) *((ulnLobLocator*)(aDest)) = ((ulnLobLocator)(aSrcVal));
#define ULN_GET_LOB_LOCATOR_VALUE(aDestVal,aSrc) *(acp_uint64_t*)aDestVal = *((acp_uint64_t*)(aSrc));


#else
/* BUG-21932  The ulnLob structure should consider 4 byte alignment for 32bit-GCC
   ULong value �� ulnLob structure �� copy�Ѵ�.
 */
#define ULN_SET_LOB_LOCATOR(aDest,aSrcVal) { \
        ((ulnLobLocator*)(aDest))->mHigh =  (((aSrcVal) &  ACP_UINT64_LITERAL(0xffffffff00000000)) >> 32)  ; \
        ((ulnLobLocator*)(aDest))->mLow =   (((aSrcVal) &  ACP_UINT64_LITERAL(0x00000000ffffffff)) ); \
}

/* BUG-21932  The ulnLob structure should consider 4 byte alignment for 32bit-GCC
   ulnLob structure���� ULong value��  �����Ѵ�.
 */
#define ULN_GET_LOB_LOCATOR_VALUE(aDestVal,aSrc) \
    *(acp_uint64_t*)aDestVal =   ((((acp_uint64_t)(((ulnLobLocator*)(aSrc))->mHigh)) << 32)  | (acp_uint64_t)(((ulnLobLocator*)(aSrc))->mLow))

#endif
struct ulnLob
{
    acp_list_node_t  mList;
    ulnLobType       mType;
    ulnLobState      mState;
    /* BUG-21932  The ulnLob structure should consider 4 byte alignment for 32bit-GCC
       ULong��� ulnLobLocator �� Abstract type�� ����Ѵ�
     */
    ulnLobLocator    mLocatorID;        /* LOB Locator ID */
    acp_uint32_t     mSize;             /* Lob �� ������. Open �� �����Ǿ,
                                           Append �ϰų� �ϸ� ���ŵ�. */

    acp_uint32_t     mSizeRetrieved;    /* �����κ��� �о�� ������ �� */

    ulnLobBuffer    *mBuffer;

    ulnLobOp        *mOp;

    /* PROJ-2047 Strengthening LOB - LOBCACHE */
    acp_uint8_t     *mData;             /* Cache���� LOB Data�� ����Ʈ */

    /* PROJ-2047 Strengthening LOB - Partial Converting */
    acp_uint32_t     mGetSize;          /* ������ ��û�� ������ �� */

    /* PROJ-2728 Sharding LOB */
    acp_bool_t       mIsNull;
};

/*
 * =====================================
 * �ʱ�ȭ �Լ�
 * =====================================
 */

ACI_RC ulnLobInitialize(ulnLob *aLob, ulnMTypeID aMTYPE);

ACI_RC ulnLobBufferInitialize(ulnLobBuffer *aLobBuffer,
                              ulnDbc       *aDbc,
                              ulnLobType    aLobType,
                              ulnCTypeID    aCTYPE,
                              acp_uint8_t  *aFileNameOrBufferPtr,
                              acp_uint32_t  aFileOptionOrBufferSize);

/*
 * ======================================
 * LOB callback �Լ�
 * ======================================
 */

ACI_RC ulnCallbackLobGetSizeResult(cmiProtocolContext *aProtocolContext,
                                   cmiProtocol        *aProtocol,
                                   void               *aServiceSession,
                                   void               *aUserContext);

ACI_RC ulnCallbackLobGetResult(cmiProtocolContext *aPtContext,
                               cmiProtocol        *aProtocol,
                               void               *aServiceSession,
                               void               *aUserContext);

ACI_RC ulnCallbackDBLobPutBeginResult(cmiProtocolContext *aProtocolContext,
                                      cmiProtocol        *aProtocol,
                                      void               *aServiceSession,
                                      void               *aUserContext);

ACI_RC ulnCallbackDBLobPutEndResult(cmiProtocolContext *aProtocolContext,
                                    cmiProtocol        *aProtocol,
                                    void               *aServiceSession,
                                    void               *aUserContext);

ACI_RC ulnCallbackLobFreeResult(cmiProtocolContext *aProtocolContext,
                                cmiProtocol        *aProtocol,
                                void               *aServiceSession,
                                void               *aUserContext);

ACI_RC ulnCallbackLobFreeAllResult(cmiProtocolContext *aProtocolContext,
                                   cmiProtocol        *aProtocol,
                                   void               *aServiceSession,
                                   void               *aUserContext);

/* PROJ-2047 Strengthening LOB - Added Interfaces */
ACI_RC ulnCallbackLobTrimResult(cmiProtocolContext *aProtocolContext,
                                cmiProtocol        *aProtocol,
                                void               *aServiceSession,
                                void               *aUserContext);


/*
 * ========================================
 * �ܺη� export �Ǵ� �Լ���
 * ========================================
 */

ACI_RC ulnLobGetSize(ulnFnContext *aFnContext,
                     ulnPtContext *aPtContext,
                     acp_uint64_t  aLocatorID,
                     acp_uint32_t *aSize,
                     acp_uint16_t *aIsNull);

ACI_RC ulnLobFreeLocator(ulnFnContext *aFnContext, ulnPtContext *aPtContext, acp_uint64_t aLocator);


#endif /* _O_ULN_LOB_H_ */
