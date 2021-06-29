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

#ifndef _O_ULN_DESC_REC_H_
#define _O_ULN_DESC_REC_H_ 1

#include <ulnConfig.h>
#include <ulnMeta.h>
#include <ulnBindInfo.h>
#include <ulnPutData.h>

struct ulnDescRec
{
    acp_list_node_t    mList;
    ulnDesc           *mParentDesc;

    acp_uint16_t       mIndex;
    ulnBindInfo        mBindInfo;             /* ������ ������ BIND (COLUMN/PARAM) INFO
                                                 �Ź��� execute ���� BIND INFO �� ���� ������ �Ͱ�
                                                 ���ؼ� ���̰� ���� ������ �ؾ� �Ѵ�.
                                                 Ư�� CHAR, BINARY Ÿ�԰� ���� variable length �����Ϳ���
                                                 �ʿ��ϴ�. SQL_NTS �����̴�. */

    ulnMeta            mMeta;
    ulnParamInOutType  mInOutType;            /* SShort SQL_DESC_PARAMETER_TYPE ipd(rw).
                                                 �Ķ���Ͱ� IN / OUT / INOUT ������ ������. */

    ulnPDContext       mPDContext;            /* SQLPutData() �� ���� ����ü */

    uluArray          *mLobArray;             /* LOB �÷��� ��� LOB �� ���� ������ �����ϱ� ���� �迭.
                                                 IPD ������ ���δ�. LOB fetch �� ���� ĳ���� ulnLob ����ü��
                                                 ����ȴ�. */

    acp_uint32_t       mOutParamBufferSize;   /* output param ������ ������. 
                                                 ulnColumn �� ũ��������� ũ��. �� �������� ũ�� */
    ulnColumn         *mOutParamBuffer;       /* output param �� ��� �������� ���ŵ� param data out ��
                                                 �������� ���� �ӽ÷� ������ �� ulnColumn �� ���� ���� */

    void              *mDataPtr;              /* SQL_DESC_DATA_PTR ard apd(rw).
                                                 Note �׻� SQL_ATTR_PARAM_BIND_OFFSET_PTR �� ���� ���ؼ�
                                                 ����ؾ� �Ѵ�. */

    acp_uint32_t      *mFileOptionsPtr;       /* SQLBindFileToCol() �̳� SQLBindFileToParam() �Լ��� ����
                                                 �Ǿ����� file options �迭�� ����Ű�� ���ؼ� ����.
                                                 ��� �� �Լ� �ܿ��� ������ ���� */

    /*
     * BUGBUG : octet length �� octet length ptr ��Ȯ�� ����!
     */
    ulvSLen           *mOctetLengthPtr;       /* SQL_DESC_OCTET_LENGTH_PTR
                                                 Note �׻� SQL_ATTR_PARAM_BIND_OFFSET_PTR �� ���� ���ؼ�
                                                 ����ؾ� �Ѵ�. */

    ulvSLen           *mIndicatorPtr;         /* SQL_DESC_INDICATOR_PTR ard apd(rw).
                                                 Note �׻� SQL_ATTR_PARAM_BIND_OFFSET_PTR �� ���� ���ؼ�
                                                 ����ؾ� �Ѵ�. */

    /*
     * BUGBUG : �Ʒ��� ������� M$ ODBC �� SQLSetDescField() �Լ��� ���� ���� �ִ� �͵��̴�.
     *          �׷��� �ϵ� ����-_-�ٴ��ؼ� �ϴ� ��� ������ �ΰ� ������� �ʱ�� �����ߴ�.
     *
     * ���� ������ �͵��� �𸣰ڴ�. ��°��.
     */

    acp_sint32_t       mAutoUniqueValue;      /* SQL_DESC_AUTO_UNIQUE_VALUE ird(r) */
    acp_sint32_t       mCaseSensitive;        /* SQL_DESC_CASE_SENSITIVE ird ipd(r) */

    acp_sint32_t       mDisplaySize;          /* SQL_DESC_DISPLAY_SIZE ird(r) */

    acp_sint16_t       mRowVer;               /* SQL_DESC_ROWVER ird ipd(r) */
    acp_sint16_t       mUnnamed;              /* SQL_DESC_UNNAMED ird(r) ipd(rw) */
    acp_sint16_t       mUnsigned;             /* SQL_DESC_UNSIGNED ird ipd(r) */
    acp_sint16_t       mUpdatable;            /* SQL_DESC_UPDATABLE ird(r) */
    acp_sint16_t       mSearchable;           /* SQL_DESC_SEARCHABLE ird(r) */

    acp_char_t        *mTypeName;             /* SQL_DESC_TYPE_NAME ird ipd(r) */

    acp_char_t         mDisplayName   [MAX_DISPLAY_NAME_LEN + 1]; /* BUGBUG (BUG-33625): SQL_DESC_LABEL ird(r) */
    acp_char_t         mColumnName    [MAX_OBJECT_NAME_LEN  + 1]; /* SQL_DESC_NAME ird(r) ipd(rw) */
    acp_char_t         mBaseColumnName[MAX_OBJECT_NAME_LEN  + 1]; /* SQL_DESC_BASE_COLUMN_NAME ird(r) */
    acp_char_t         mTableName     [MAX_OBJECT_NAME_LEN  + 1]; /* SQL_DESC_TABLE_NAME ird(r) */
    acp_char_t         mBaseTableName [MAX_OBJECT_NAME_LEN  + 1]; /* SQL_DESC_BASE_TABLE_NAME ird(r)*/
    acp_char_t         mCatalogName   [MAX_OBJECT_NAME_LEN  + 1]; /* SQL_DESC_CATALOG_NAME ird(r)*/
    acp_char_t         mSchemaName    [MAX_OBJECT_NAME_LEN  + 1]; /* SQL_DESC_SCHEMA_NAME ird(r) */

    acp_char_t        *mLiteralPrefix;        /* SQL_DESC_LITERAL_PREFIX ird(r) */
    acp_char_t        *mLiteralSuffix;        /* SQL_DESC_LITERAL_SUFFIX ird(r) */
    acp_char_t        *mLocalTypeName;        /* SQL_DESC_LOCAL_TYPE_NAME ird ipd(r) */

    /* PROJ-2616 */
    /*******************************************************************
     * ���� ������ ���, ��ġ�ÿ� DB�� ����� ������ ����� ��°��
     * ����ȴ�.
     * Data Row = |Data1 MaxByte|Data2 MaxByte| ... |DataN MaxByte]
     * UL�� ������ �ο� �޾Ƽ� mMaxByteSize�� ����, �÷��� �и��Ͽ�
     * ����ڿ��� �����͸� �����Ѵ�.
     *******************************************************************/
    acp_uint32_t       mMaxByteSize;

    /* PROJ-2739 Client-side Sharding LOB
     *   locator OutBind�� ��� ShardLocator�� �����Ͽ� �ӽ� ����.
     *   ���� ��尡 ������ ��� �����ϱ� ���ؼ�... */
    void              *mShardLocator;
};

/*
 * Descriptor Record
 */

ACI_RC ulnDescRecCreate(ulnDesc *aParentDesc, ulnDescRec **aDescRec, acp_uint16_t aIndex);
ACI_RC ulnDescRecInitialize(ulnDesc *aParentDesc, ulnDescRec *aRecord, acp_uint16_t aIndex);

acp_bool_t ulnDescRecIsDataAtExecParam(ulnDescRec *aAppDescRec, acp_uint32_t aRow);

ACP_INLINE ulnParamInOutType ulnDescRecGetParamInOut(ulnDescRec *aRec)
{
    return aRec->mInOutType;
}

ACP_INLINE ACI_RC ulnDescRecSetParamInOut(ulnDescRec *aRec, ulnParamInOutType aInOutType)
{
    aRec->mInOutType = aInOutType;

    return ACI_SUCCESS;
}

ACP_INLINE void ulnDescRecSetIndicatorPtr(ulnDescRec *aDescRec, ulvSLen *aNewIndicatorPtr)
{
    aDescRec->mIndicatorPtr = aNewIndicatorPtr;
}

ACP_INLINE ulvSLen *ulnDescRecGetIndicatorPtr(ulnDescRec *aDescRec)
{
    return aDescRec->mIndicatorPtr;
}

ACP_INLINE void ulnDescRecSetOctetLengthPtr(ulnDescRec *aDescRec, ulvSLen *aNewOctetLengthPtr)
{
    aDescRec->mOctetLengthPtr = aNewOctetLengthPtr;
}

ACP_INLINE ulvSLen *ulnDescRecGetOctetLengthPtr(ulnDescRec *aDescRec)
{
    return aDescRec->mOctetLengthPtr;
}

ACP_INLINE void ulnDescRecSetDataPtr(ulnDescRec *aDescRec, void *aDataPtr)
{
    aDescRec->mDataPtr = aDataPtr;
}

ACP_INLINE void *ulnDescRecGetDataPtr(ulnDescRec *aDescRec)
{
    return aDescRec->mDataPtr;
}

ACP_INLINE acp_sint32_t ulnDescRecGetDisplaySize(ulnDescRec *aDescRec)
{
    return aDescRec->mDisplaySize;
}

ACP_INLINE void ulnDescRecSetDisplaySize(ulnDescRec *aDescRec, acp_sint32_t aDisplaySize)
{
    aDescRec->mDisplaySize = aDisplaySize;
}

ACP_INLINE acp_sint16_t ulnDescRecGetUnsigned(ulnDescRec *aDescRec)
{
    return aDescRec->mUnsigned;
}

ACP_INLINE void ulnDescRecSetUnsigned(ulnDescRec *aDescRec, acp_sint16_t aUnsignedFlag)
{
    aDescRec->mUnsigned = aUnsignedFlag;
}

ACP_INLINE acp_sint16_t ulnDescRecGetUnnamed(ulnDescRec *aDescRec)
{
    return aDescRec->mUnnamed;
}

ACP_INLINE void ulnDescRecSetUnnamed(ulnDescRec *aDescRec, acp_sint16_t aUnnamedFlag)
{
    aDescRec->mUnnamed = aUnnamedFlag;
}

ACP_INLINE acp_sint16_t ulnDescRecGetSearchable(ulnDescRec *aDescRec)
{
    /*
     * BUGBUG : ó�� �� �Ӽ��� ã�� �Լ��� ȣ�� �� �� ������ ������ Ÿ�Կ� ���� ��Ÿ�� ��ȸ�ؼ�
     *          �����ؾ� �Ѵ�.
     *          �׷���, ������ �ϴ�, ULN_MTYPE �� ���� �������Ѽ� �־�ε��� ����.
     */
    return aDescRec->mSearchable;
}

ACP_INLINE void ulnDescRecSetSearchable(ulnDescRec *aDescRec, acp_sint16_t aSearchable)
{
    aDescRec->mSearchable = aSearchable;
}

ACP_INLINE void ulnDescRecSetLiteralPrefix(ulnDescRec *aDescRec, acp_char_t *aPrefix)
{
    aDescRec->mLiteralPrefix = aPrefix;
}

ACP_INLINE acp_char_t *ulnDescRecGetLiteralPrefix(ulnDescRec *aDescRec)
{
    return aDescRec->mLiteralPrefix;
}

ACP_INLINE void ulnDescRecSetLiteralSuffix(ulnDescRec *aDescRec, acp_char_t *aSuffix)
{
    aDescRec->mLiteralSuffix = aSuffix;
}

ACP_INLINE acp_char_t *ulnDescRecGetLiteralSuffix(ulnDescRec *aDescRec)
{
    return aDescRec->mLiteralSuffix;
}

ACP_INLINE void ulnDescRecSetTypeName(ulnDescRec *aDescRec, acp_char_t *aTypeName)
{
    aDescRec->mTypeName = aTypeName;
}

ACP_INLINE acp_char_t *ulnDescRecGetTypeName(ulnDescRec *aDescRec)
{
    return aDescRec->mTypeName;
}

ACP_INLINE acp_uint16_t ulnDescRecGetIndex(ulnDescRec *aDescRec)
{
    return aDescRec->mIndex;
}


void        ulnDescRecSetName(ulnDescRec *aRecord, acp_char_t *aName);
acp_char_t *ulnDescRecGetName(ulnDescRec *aRecord);

/*
 * ulnDescRec::mLobArray ���� �Լ���
 */
void     ulnDescRecInitLobArray(ulnDescRec *aDescRec);
ACI_RC   ulnDescRecArrangeLobArray(ulnDescRec *aImpDescRec, acp_uint32_t aArrayCount);
ulnLob  *ulnDescRecGetLobElement(ulnDescRec *aDescRec, acp_uint32_t aRowNumber);

/*
 * output parameter �� conversion �� ���� ���ۿ� ���õ� �Լ���
 */

void   ulnDescRecInitOutParamBuffer(ulnDescRec *aDescRec);
void   ulnDescRecFinalizeOutParamBuffer(ulnDescRec *aDescRec);
ACI_RC ulnDescRecPrepareOutParamBuffer(ulnDescRec *aDescRec, acp_uint32_t aNewSize, ulnMTypeID aMTYPE);

/* PROJ-1789 Updatable Scrollable Cursor */

#define ulnDescRecGetDisplayName(aRecordPtr)        ( (aRecordPtr)->mDisplayName )
#define ulnDescRecGetColumnName(aRecordPtr)         ( (aRecordPtr)->mColumnName )
#define ulnDescRecGetBaseColumnName(aRecordPtr)     ( (aRecordPtr)->mBaseColumnName )
#define ulnDescRecGetTableName(aRecordPtr)          ( (aRecordPtr)->mTableName )
#define ulnDescRecGetBaseTableName(aRecordPtr)      ( (aRecordPtr)->mBaseTableName )
#define ulnDescRecGetSchemaName(aRecordPtr)         ( (aRecordPtr)->mSchemaName )
#define ulnDescRecGetCatalogName(aRecordPtr)        ( (aRecordPtr)->mCatalogName )

void ulnDescRecSetDisplayName(ulnDescRec *aRecord, acp_char_t *aDisplayName, acp_size_t aDisplayNameLen);
void ulnDescRecSetColumnName(ulnDescRec *aRecord, acp_char_t *aColumnName, acp_size_t aColumnNameLen);
void ulnDescRecSetBaseColumnName(ulnDescRec *aRecord, acp_char_t *aBaseColumnName, acp_size_t aBaseColumnNameLen);
void ulnDescRecSetTableName(ulnDescRec *aRecord, acp_char_t *aTableName, acp_size_t aTableNameLen);
void ulnDescRecSetBaseTableName(ulnDescRec *aRecord, acp_char_t *aBaseTableName, acp_size_t aBaseTableNameLen);
void ulnDescRecSetSchemaName(ulnDescRec *aRecord, acp_char_t *aSchemaName, acp_size_t aSchemaNameLen);
void ulnDescRecSetCatalogName(ulnDescRec *aRecord, acp_char_t *aCatalogName, acp_size_t aCatalogNameLen);

/* PROJ-2739 Client-side Sharding LOB */
ACP_INLINE void ulnDescRecSetShardLobLocator(ulnDescRec *aDescRec, void *aShardLocator)
{
    aDescRec->mShardLocator = aShardLocator;
}

ACP_INLINE void *ulnDescRecGetShardLobLocator(ulnDescRec *aDescRec)
{
    return aDescRec->mShardLocator;
}

#endif /* _O_ULN_DESC_REC_H_ */
