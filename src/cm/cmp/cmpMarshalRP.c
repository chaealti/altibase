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

#include <cmAllClient.h>


ACI_RC cmpReadRPVersion(cmbBlock *aBlock, cmpProtocol *aProtocol, cmpMarshalState *aMarshalState)
{
    cmpArgRPVersion *sArg = CMI_PROTOCOL_GET_ARG(*aProtocol, RP, Version);

    CMP_MARSHAL_BEGIN(1);
    CMP_MARSHAL_READ_ULONG(sArg->mVersion);

    CMP_MARSHAL_STAGE(0);

    return ACI_SUCCESS;
    ACI_EXCEPTION_END;
    return ACI_FAILURE;
}

ACI_RC cmpWriteRPVersion(cmbBlock *aBlock, cmpProtocol *aProtocol, cmpMarshalState *aMarshalState)
{
    cmpArgRPVersion *sArg = CMI_PROTOCOL_GET_ARG(*aProtocol, RP, Version);

    CMP_MARSHAL_BEGIN(1);
    CMP_MARSHAL_WRITE_ULONG(sArg->mVersion);

    CMP_MARSHAL_STAGE(0);

    return ACI_SUCCESS;
    ACI_EXCEPTION_END;
    return ACI_FAILURE;
}

ACI_RC cmpReadRPMetaRepl(cmbBlock *aBlock, cmpProtocol *aProtocol, cmpMarshalState *aMarshalState)
{
    cmpArgRPMetaRepl *sArg = CMI_PROTOCOL_GET_ARG(*aProtocol, RP, MetaRepl);

    acp_uint64_t sDeprecated = -1;

    CMP_MARSHAL_BEGIN(21);
    CMP_MARSHAL_READ_VARIABLE(sArg->mRepName);

    CMP_MARSHAL_STAGE(20);
    CMP_MARSHAL_READ_SINT(sArg->mItemCnt);

    CMP_MARSHAL_STAGE(19);
    CMP_MARSHAL_READ_SINT(sArg->mRole);

    CMP_MARSHAL_STAGE(18);
    CMP_MARSHAL_READ_SINT(sArg->mConflictResolution);

    CMP_MARSHAL_STAGE(17);
    CMP_MARSHAL_READ_UINT(sArg->mTransTblSize);

    CMP_MARSHAL_STAGE(16);
    CMP_MARSHAL_READ_UINT(sArg->mFlags);

    CMP_MARSHAL_STAGE(15);
    CMP_MARSHAL_READ_UINT(sArg->mOptions);

    CMP_MARSHAL_STAGE(14);
    CMP_MARSHAL_READ_ULONG(sArg->mRPRecoverySN);

    /* PROJ-1915 */
    CMP_MARSHAL_STAGE(13);
    CMP_MARSHAL_READ_VARIABLE(sArg->mOSInfo);

    CMP_MARSHAL_STAGE(12);
    CMP_MARSHAL_READ_UINT(sArg->mCompileBit);

    CMP_MARSHAL_STAGE(11);
    CMP_MARSHAL_READ_UINT(sArg->mSmVersionID);

    CMP_MARSHAL_STAGE(10);
    CMP_MARSHAL_READ_UINT(sArg->mLFGCount);

    CMP_MARSHAL_STAGE(9);
    CMP_MARSHAL_READ_ULONG(sArg->mLogFileSize);

    CMP_MARSHAL_STAGE(8);
    CMP_MARSHAL_READ_ULONG( sDeprecated );

    CMP_MARSHAL_STAGE(7);
    CMP_MARSHAL_READ_VARIABLE(sArg->mDBCharSet);

    CMP_MARSHAL_STAGE(6);
    CMP_MARSHAL_READ_VARIABLE(sArg->mNationalCharSet);

    CMP_MARSHAL_STAGE(5);
    CMP_MARSHAL_READ_VARIABLE(sArg->mServerID);

    CMP_MARSHAL_STAGE(4);
    CMP_MARSHAL_READ_UINT(sArg->mReplMode);

    CMP_MARSHAL_STAGE(3);
    CMP_MARSHAL_READ_UINT(sArg->mParallelID);

    CMP_MARSHAL_STAGE(2);
    CMP_MARSHAL_READ_SINT(sArg->mIsStarted);

    CMP_MARSHAL_STAGE(1);
    CMP_MARSHAL_READ_VARIABLE(sArg->mRemoteFaultDetectTime);

    CMP_MARSHAL_STAGE(0);

    return ACI_SUCCESS;
    ACI_EXCEPTION_END;
    return ACI_FAILURE;
}

ACI_RC cmpWriteRPMetaRepl(cmbBlock *aBlock, cmpProtocol *aProtocol, cmpMarshalState *aMarshalState)
{
    cmpArgRPMetaRepl *sArg = CMI_PROTOCOL_GET_ARG(*aProtocol, RP, MetaRepl);

    acp_uint64_t sDeprecated = -1;

    CMP_MARSHAL_BEGIN(21);
    CMP_MARSHAL_WRITE_VARIABLE(sArg->mRepName);

    CMP_MARSHAL_STAGE(20);
    CMP_MARSHAL_WRITE_SINT(sArg->mItemCnt);

    CMP_MARSHAL_STAGE(19);
    CMP_MARSHAL_WRITE_SINT(sArg->mRole);

    CMP_MARSHAL_STAGE(18);
    CMP_MARSHAL_WRITE_SINT(sArg->mConflictResolution);

    CMP_MARSHAL_STAGE(17);
    CMP_MARSHAL_WRITE_UINT(sArg->mTransTblSize);

    CMP_MARSHAL_STAGE(16);
    CMP_MARSHAL_WRITE_UINT(sArg->mFlags);

    CMP_MARSHAL_STAGE(15);
    CMP_MARSHAL_WRITE_UINT(sArg->mOptions);

    CMP_MARSHAL_STAGE(14);
    CMP_MARSHAL_WRITE_ULONG(sArg->mRPRecoverySN);

    /* PROJ-1915 */
    CMP_MARSHAL_STAGE(13);
    CMP_MARSHAL_WRITE_VARIABLE(sArg->mOSInfo);

    CMP_MARSHAL_STAGE(12);
    CMP_MARSHAL_WRITE_UINT(sArg->mCompileBit);

    CMP_MARSHAL_STAGE(11);
    CMP_MARSHAL_WRITE_UINT(sArg->mSmVersionID);

    CMP_MARSHAL_STAGE(10);
    CMP_MARSHAL_WRITE_UINT(sArg->mLFGCount);

    CMP_MARSHAL_STAGE(9);
    CMP_MARSHAL_WRITE_ULONG(sArg->mLogFileSize);

    CMP_MARSHAL_STAGE(8);
    CMP_MARSHAL_WRITE_ULONG( sDeprecated );

    CMP_MARSHAL_STAGE(7);
    CMP_MARSHAL_WRITE_VARIABLE(sArg->mDBCharSet);

    CMP_MARSHAL_STAGE(6);
    CMP_MARSHAL_WRITE_VARIABLE(sArg->mNationalCharSet);

    CMP_MARSHAL_STAGE(5);
    CMP_MARSHAL_WRITE_VARIABLE(sArg->mServerID);

    CMP_MARSHAL_STAGE(4);
    CMP_MARSHAL_WRITE_UINT(sArg->mReplMode);

    CMP_MARSHAL_STAGE(3);
    CMP_MARSHAL_WRITE_UINT(sArg->mParallelID);

    CMP_MARSHAL_STAGE(2);
    CMP_MARSHAL_WRITE_SINT(sArg->mIsStarted);

    CMP_MARSHAL_STAGE(1);
    CMP_MARSHAL_WRITE_VARIABLE(sArg->mRemoteFaultDetectTime);

    CMP_MARSHAL_STAGE(0);

    return ACI_SUCCESS;
    ACI_EXCEPTION_END;
    return ACI_FAILURE;
}

ACI_RC cmpReadRPMetaReplTbl(cmbBlock *aBlock, cmpProtocol *aProtocol, cmpMarshalState *aMarshalState)
{
    cmpArgRPMetaReplTbl *sArg = CMI_PROTOCOL_GET_ARG(*aProtocol, RP, MetaReplTbl);

    CMP_MARSHAL_BEGIN(18);
    CMP_MARSHAL_READ_VARIABLE(sArg->mRepName);

    CMP_MARSHAL_STAGE(17);
    CMP_MARSHAL_READ_VARIABLE(sArg->mLocalUserName);

    CMP_MARSHAL_STAGE(16);
    CMP_MARSHAL_READ_VARIABLE(sArg->mLocalTableName);

    CMP_MARSHAL_STAGE(15);
    CMP_MARSHAL_READ_VARIABLE(sArg->mLocalPartName);

    CMP_MARSHAL_STAGE(14);
    CMP_MARSHAL_READ_VARIABLE(sArg->mRemoteUserName);

    CMP_MARSHAL_STAGE(13);
    CMP_MARSHAL_READ_VARIABLE(sArg->mRemoteTableName);

    CMP_MARSHAL_STAGE(12);
    CMP_MARSHAL_READ_VARIABLE(sArg->mRemotePartName);

    CMP_MARSHAL_STAGE(11);
    CMP_MARSHAL_READ_VARIABLE(sArg->mPartCondMinValues);

    CMP_MARSHAL_STAGE(10);
    CMP_MARSHAL_READ_VARIABLE(sArg->mPartCondMaxValues);

    CMP_MARSHAL_STAGE(9);
    CMP_MARSHAL_READ_UINT(sArg->mPartitionMethod);

    CMP_MARSHAL_STAGE(8);
    CMP_MARSHAL_READ_UINT(sArg->mPartitionOrder);

    CMP_MARSHAL_STAGE(7);
    CMP_MARSHAL_READ_ULONG(sArg->mTableOID);

    CMP_MARSHAL_STAGE(6);
    CMP_MARSHAL_READ_UINT(sArg->mPKIndexID);

    CMP_MARSHAL_STAGE(5);
    CMP_MARSHAL_READ_UINT(sArg->mPKColCnt);

    CMP_MARSHAL_STAGE(4);
    CMP_MARSHAL_READ_SINT(sArg->mColumnCnt);

    CMP_MARSHAL_STAGE(3);
    CMP_MARSHAL_READ_SINT(sArg->mIndexCnt);

    /* PROJ-1915 */
    CMP_MARSHAL_STAGE(2);
    CMP_MARSHAL_READ_ULONG(sArg->mInvalidMaxSN);

    CMP_MARSHAL_STAGE(1);
    CMP_MARSHAL_READ_VARIABLE(sArg->mConditionStr);
    
    CMP_MARSHAL_STAGE(0);

    return ACI_SUCCESS;
    ACI_EXCEPTION_END;
    return ACI_FAILURE;
}

ACI_RC cmpWriteRPMetaReplTbl(cmbBlock *aBlock, cmpProtocol *aProtocol, cmpMarshalState *aMarshalState)
{
    cmpArgRPMetaReplTbl *sArg = CMI_PROTOCOL_GET_ARG(*aProtocol, RP, MetaReplTbl);

    CMP_MARSHAL_BEGIN(18);
    CMP_MARSHAL_WRITE_VARIABLE(sArg->mRepName);

    CMP_MARSHAL_STAGE(17);
    CMP_MARSHAL_WRITE_VARIABLE(sArg->mLocalUserName);

    CMP_MARSHAL_STAGE(16);
    CMP_MARSHAL_WRITE_VARIABLE(sArg->mLocalTableName);

    CMP_MARSHAL_STAGE(15);
    CMP_MARSHAL_WRITE_VARIABLE(sArg->mLocalPartName);

    CMP_MARSHAL_STAGE(14);
    CMP_MARSHAL_WRITE_VARIABLE(sArg->mRemoteUserName);

    CMP_MARSHAL_STAGE(13);
    CMP_MARSHAL_WRITE_VARIABLE(sArg->mRemoteTableName);

    CMP_MARSHAL_STAGE(12);
    CMP_MARSHAL_WRITE_VARIABLE(sArg->mRemotePartName);

    CMP_MARSHAL_STAGE(11);
    CMP_MARSHAL_WRITE_VARIABLE(sArg->mPartCondMinValues);

    CMP_MARSHAL_STAGE(10);
    CMP_MARSHAL_WRITE_VARIABLE(sArg->mPartCondMaxValues);

    CMP_MARSHAL_STAGE(9);
    CMP_MARSHAL_WRITE_UINT(sArg->mPartitionMethod);

    CMP_MARSHAL_STAGE(8);
    CMP_MARSHAL_WRITE_UINT(sArg->mPartitionOrder);

    CMP_MARSHAL_STAGE(7);
    CMP_MARSHAL_WRITE_ULONG(sArg->mTableOID);

    CMP_MARSHAL_STAGE(6);
    CMP_MARSHAL_WRITE_UINT(sArg->mPKIndexID);

    CMP_MARSHAL_STAGE(5);
    CMP_MARSHAL_WRITE_UINT(sArg->mPKColCnt);

    CMP_MARSHAL_STAGE(4);
    CMP_MARSHAL_WRITE_SINT(sArg->mColumnCnt);

    CMP_MARSHAL_STAGE(3);
    CMP_MARSHAL_WRITE_SINT(sArg->mIndexCnt);

    /* PROJ-1915 */
    CMP_MARSHAL_STAGE(2);
    CMP_MARSHAL_WRITE_ULONG(sArg->mInvalidMaxSN);

    CMP_MARSHAL_STAGE(1);
    CMP_MARSHAL_WRITE_VARIABLE(sArg->mConditionStr);
    
    CMP_MARSHAL_STAGE(0);

    return ACI_SUCCESS;
    ACI_EXCEPTION_END;
    return ACI_FAILURE;
}

ACI_RC cmpReadRPMetaReplCol(cmbBlock *aBlock, cmpProtocol *aProtocol, cmpMarshalState *aMarshalState)
{
    cmpArgRPMetaReplCol *sArg = CMI_PROTOCOL_GET_ARG(*aProtocol, RP, MetaReplCol);

    CMP_MARSHAL_BEGIN(15);
    CMP_MARSHAL_READ_VARIABLE(sArg->mColumnName);

    CMP_MARSHAL_STAGE(14);
    CMP_MARSHAL_READ_UINT(sArg->mColumnID);

    CMP_MARSHAL_STAGE(13);
    CMP_MARSHAL_READ_UINT(sArg->mColumnFlag);

    CMP_MARSHAL_STAGE(12);
    CMP_MARSHAL_READ_UINT(sArg->mColumnOffset);

    CMP_MARSHAL_STAGE(11);
    CMP_MARSHAL_READ_UINT(sArg->mColumnSize);

    CMP_MARSHAL_STAGE(10);
    CMP_MARSHAL_READ_UINT(sArg->mDataTypeID);

    CMP_MARSHAL_STAGE(9);
    CMP_MARSHAL_READ_UINT(sArg->mLangID);

    CMP_MARSHAL_STAGE(8);
    CMP_MARSHAL_READ_UINT(sArg->mFlags);

    CMP_MARSHAL_STAGE(7);
    CMP_MARSHAL_READ_SINT(sArg->mPrecision);

    CMP_MARSHAL_STAGE(6);
    CMP_MARSHAL_READ_SINT(sArg->mScale);

    CMP_MARSHAL_STAGE(5);
    CMP_MARSHAL_READ_SINT(sArg->mEncPrecision);

    CMP_MARSHAL_STAGE(4);
    CMP_MARSHAL_READ_VARIABLE(sArg->mPolicyName);

    CMP_MARSHAL_STAGE(3);
    CMP_MARSHAL_READ_VARIABLE(sArg->mPolicyCode);

    CMP_MARSHAL_STAGE(2);
    CMP_MARSHAL_READ_VARIABLE(sArg->mECCPolicyName);

    CMP_MARSHAL_STAGE(1);
    CMP_MARSHAL_READ_VARIABLE(sArg->mECCPolicyCode);

    CMP_MARSHAL_STAGE(0);

    return ACI_SUCCESS;
    ACI_EXCEPTION_END;
    return ACI_FAILURE;
}

ACI_RC cmpWriteRPMetaReplCol(cmbBlock *aBlock, cmpProtocol *aProtocol, cmpMarshalState *aMarshalState)
{
    cmpArgRPMetaReplCol *sArg = CMI_PROTOCOL_GET_ARG(*aProtocol, RP, MetaReplCol);

    CMP_MARSHAL_BEGIN(15);
    CMP_MARSHAL_WRITE_VARIABLE(sArg->mColumnName);

    CMP_MARSHAL_STAGE(14);
    CMP_MARSHAL_WRITE_UINT(sArg->mColumnID);

    CMP_MARSHAL_STAGE(13);
    CMP_MARSHAL_WRITE_UINT(sArg->mColumnFlag);

    CMP_MARSHAL_STAGE(12);
    CMP_MARSHAL_WRITE_UINT(sArg->mColumnOffset);

    CMP_MARSHAL_STAGE(11);
    CMP_MARSHAL_WRITE_UINT(sArg->mColumnSize);

    CMP_MARSHAL_STAGE(10);
    CMP_MARSHAL_WRITE_UINT(sArg->mDataTypeID);

    CMP_MARSHAL_STAGE(9);
    CMP_MARSHAL_WRITE_UINT(sArg->mLangID);

    CMP_MARSHAL_STAGE(8);
    CMP_MARSHAL_WRITE_UINT(sArg->mFlags);

    CMP_MARSHAL_STAGE(7);
    CMP_MARSHAL_WRITE_SINT(sArg->mPrecision);

    CMP_MARSHAL_STAGE(6);
    CMP_MARSHAL_WRITE_SINT(sArg->mScale);

    CMP_MARSHAL_STAGE(5);
    CMP_MARSHAL_WRITE_SINT(sArg->mEncPrecision);

    CMP_MARSHAL_STAGE(4);
    CMP_MARSHAL_WRITE_VARIABLE(sArg->mPolicyName);

    CMP_MARSHAL_STAGE(3);
    CMP_MARSHAL_WRITE_VARIABLE(sArg->mPolicyCode);

    CMP_MARSHAL_STAGE(2);
    CMP_MARSHAL_WRITE_VARIABLE(sArg->mECCPolicyName);

    CMP_MARSHAL_STAGE(1);
    CMP_MARSHAL_WRITE_VARIABLE(sArg->mECCPolicyCode);

    CMP_MARSHAL_STAGE(0);

    return ACI_SUCCESS;
    ACI_EXCEPTION_END;
    return ACI_FAILURE;
}

ACI_RC cmpReadRPMetaReplIdx(cmbBlock *aBlock, cmpProtocol *aProtocol, cmpMarshalState *aMarshalState)
{
    cmpArgRPMetaReplIdx *sArg = CMI_PROTOCOL_GET_ARG(*aProtocol, RP, MetaReplIdx);

    CMP_MARSHAL_BEGIN(6);
    CMP_MARSHAL_READ_VARIABLE(sArg->mIndexName);

    CMP_MARSHAL_STAGE(5);
    CMP_MARSHAL_READ_UINT(sArg->mIndexID);

    CMP_MARSHAL_STAGE(4);
    CMP_MARSHAL_READ_UINT(sArg->mIndexTypeID);

    CMP_MARSHAL_STAGE(3);
    CMP_MARSHAL_READ_UINT(sArg->mKeyColumnCnt);

    CMP_MARSHAL_STAGE(2);
    CMP_MARSHAL_READ_UINT(sArg->mIsUnique);

    CMP_MARSHAL_STAGE(1);
    CMP_MARSHAL_READ_UINT(sArg->mIsRange);

    CMP_MARSHAL_STAGE(0);

    return ACI_SUCCESS;
    ACI_EXCEPTION_END;
    return ACI_FAILURE;
}

ACI_RC cmpWriteRPMetaReplIdx(cmbBlock *aBlock, cmpProtocol *aProtocol, cmpMarshalState *aMarshalState)
{
    cmpArgRPMetaReplIdx *sArg = CMI_PROTOCOL_GET_ARG(*aProtocol, RP, MetaReplIdx);

    CMP_MARSHAL_BEGIN(6);
    CMP_MARSHAL_WRITE_VARIABLE(sArg->mIndexName);

    CMP_MARSHAL_STAGE(5);
    CMP_MARSHAL_WRITE_UINT(sArg->mIndexID);

    CMP_MARSHAL_STAGE(4);
    CMP_MARSHAL_WRITE_UINT(sArg->mIndexTypeID);

    CMP_MARSHAL_STAGE(3);
    CMP_MARSHAL_WRITE_UINT(sArg->mKeyColumnCnt);

    CMP_MARSHAL_STAGE(2);
    CMP_MARSHAL_WRITE_UINT(sArg->mIsUnique);

    CMP_MARSHAL_STAGE(1);
    CMP_MARSHAL_WRITE_UINT(sArg->mIsRange);

    CMP_MARSHAL_STAGE(0);

    return ACI_SUCCESS;
    ACI_EXCEPTION_END;
    return ACI_FAILURE;
}

ACI_RC cmpReadRPMetaReplIdxCol(cmbBlock *aBlock, cmpProtocol *aProtocol, cmpMarshalState *aMarshalState)
{
    cmpArgRPMetaReplIdxCol *sArg = CMI_PROTOCOL_GET_ARG(*aProtocol, RP, MetaReplIdxCol);

    CMP_MARSHAL_BEGIN(2);
    CMP_MARSHAL_READ_UINT(sArg->mColumnID);

    CMP_MARSHAL_STAGE(1);
    CMP_MARSHAL_READ_UINT(sArg->mKeyColumnFlag);

    CMP_MARSHAL_STAGE(0);

    return ACI_SUCCESS;
    ACI_EXCEPTION_END;
    return ACI_FAILURE;
}

ACI_RC cmpWriteRPMetaReplIdxCol(cmbBlock *aBlock, cmpProtocol *aProtocol, cmpMarshalState *aMarshalState)
{
    cmpArgRPMetaReplIdxCol *sArg = CMI_PROTOCOL_GET_ARG(*aProtocol, RP, MetaReplIdxCol);

    CMP_MARSHAL_BEGIN(2);
    CMP_MARSHAL_WRITE_UINT(sArg->mColumnID);

    CMP_MARSHAL_STAGE(1);
    CMP_MARSHAL_WRITE_UINT(sArg->mKeyColumnFlag);

    CMP_MARSHAL_STAGE(0);

    return ACI_SUCCESS;
    ACI_EXCEPTION_END;
    return ACI_FAILURE;
}

ACI_RC cmpReadRPHandshakeAck(cmbBlock *aBlock, cmpProtocol *aProtocol, cmpMarshalState *aMarshalState)
{
    cmpArgRPHandshakeAck *sArg = CMI_PROTOCOL_GET_ARG(*aProtocol, RP, HandshakeAck);

    CMP_MARSHAL_BEGIN(4);
    CMP_MARSHAL_READ_UINT(sArg->mResult);

    CMP_MARSHAL_STAGE(3);
    CMP_MARSHAL_READ_ULONG(sArg->mXSN);

    CMP_MARSHAL_STAGE(2);
    CMP_MARSHAL_READ_SINT(sArg->mFailbackStatus);

    CMP_MARSHAL_STAGE(1);
    CMP_MARSHAL_READ_VARIABLE(sArg->mMsg);

    CMP_MARSHAL_STAGE(0);

    return ACI_SUCCESS;
    ACI_EXCEPTION_END;
    return ACI_FAILURE;
}

ACI_RC cmpWriteRPHandshakeAck(cmbBlock *aBlock, cmpProtocol *aProtocol, cmpMarshalState *aMarshalState)
{
    cmpArgRPHandshakeAck *sArg = CMI_PROTOCOL_GET_ARG(*aProtocol, RP, HandshakeAck);

    CMP_MARSHAL_BEGIN(4);
    CMP_MARSHAL_WRITE_UINT(sArg->mResult);

    CMP_MARSHAL_STAGE(3);
    CMP_MARSHAL_WRITE_ULONG(sArg->mXSN);

    CMP_MARSHAL_STAGE(2);
    CMP_MARSHAL_WRITE_SINT(sArg->mFailbackStatus);

    CMP_MARSHAL_STAGE(1);
    CMP_MARSHAL_WRITE_VARIABLE(sArg->mMsg);

    CMP_MARSHAL_STAGE(0);

    return ACI_SUCCESS;
    ACI_EXCEPTION_END;
    return ACI_FAILURE;
}

ACI_RC cmpReadRPTrBegin(cmbBlock *aBlock, cmpProtocol *aProtocol, cmpMarshalState *aMarshalState)
{
    cmpArgRPTrBegin *sArg = CMI_PROTOCOL_GET_ARG(*aProtocol, RP, TrBegin);

    CMP_MARSHAL_BEGIN(4);
    CMP_MARSHAL_READ_UINT(sArg->mXLogType);

    CMP_MARSHAL_STAGE(3);
    CMP_MARSHAL_READ_UINT(sArg->mTransID);

    CMP_MARSHAL_STAGE(2);
    CMP_MARSHAL_READ_ULONG(sArg->mSN);

    CMP_MARSHAL_STAGE(1);
    CMP_MARSHAL_READ_ULONG(sArg->mSyncSN);

    CMP_MARSHAL_STAGE(0);

    return ACI_SUCCESS;
    ACI_EXCEPTION_END;
    return ACI_FAILURE;
}

ACI_RC cmpWriteRPTrBegin(cmbBlock *aBlock, cmpProtocol *aProtocol, cmpMarshalState *aMarshalState)
{
    cmpArgRPTrBegin *sArg = CMI_PROTOCOL_GET_ARG(*aProtocol, RP, TrBegin);

    CMP_MARSHAL_BEGIN(4);
    CMP_MARSHAL_WRITE_UINT(sArg->mXLogType);

    CMP_MARSHAL_STAGE(3);
    CMP_MARSHAL_WRITE_UINT(sArg->mTransID);

    CMP_MARSHAL_STAGE(2);
    CMP_MARSHAL_WRITE_ULONG(sArg->mSN);

    CMP_MARSHAL_STAGE(1);
    CMP_MARSHAL_WRITE_ULONG(sArg->mSyncSN);

    CMP_MARSHAL_STAGE(0);

    return ACI_SUCCESS;
    ACI_EXCEPTION_END;
    return ACI_FAILURE;
}

ACI_RC cmpReadRPTrCommit(cmbBlock *aBlock, cmpProtocol *aProtocol, cmpMarshalState *aMarshalState)
{
    cmpArgRPTrCommit *sArg = CMI_PROTOCOL_GET_ARG(*aProtocol, RP, TrCommit);

    CMP_MARSHAL_BEGIN(4);
    CMP_MARSHAL_READ_UINT(sArg->mXLogType);

    CMP_MARSHAL_STAGE(3);
    CMP_MARSHAL_READ_UINT(sArg->mTransID);

    CMP_MARSHAL_STAGE(2);
    CMP_MARSHAL_READ_ULONG(sArg->mSN);

    CMP_MARSHAL_STAGE(1);
    CMP_MARSHAL_READ_ULONG(sArg->mSyncSN);

    CMP_MARSHAL_STAGE(0);

    return ACI_SUCCESS;
    ACI_EXCEPTION_END;
    return ACI_FAILURE;
}

ACI_RC cmpWriteRPTrCommit(cmbBlock *aBlock, cmpProtocol *aProtocol, cmpMarshalState *aMarshalState)
{
    cmpArgRPTrCommit *sArg = CMI_PROTOCOL_GET_ARG(*aProtocol, RP, TrCommit);

    CMP_MARSHAL_BEGIN(4);
    CMP_MARSHAL_WRITE_UINT(sArg->mXLogType);

    CMP_MARSHAL_STAGE(3);
    CMP_MARSHAL_WRITE_UINT(sArg->mTransID);

    CMP_MARSHAL_STAGE(2);
    CMP_MARSHAL_WRITE_ULONG(sArg->mSN);

    CMP_MARSHAL_STAGE(1);
    CMP_MARSHAL_WRITE_ULONG(sArg->mSyncSN);

    CMP_MARSHAL_STAGE(0);

    return ACI_SUCCESS;
    ACI_EXCEPTION_END;
    return ACI_FAILURE;
}

ACI_RC cmpReadRPTrAbort(cmbBlock *aBlock, cmpProtocol *aProtocol, cmpMarshalState *aMarshalState)
{
    cmpArgRPTrAbort *sArg = CMI_PROTOCOL_GET_ARG(*aProtocol, RP, TrAbort);

    CMP_MARSHAL_BEGIN(4);
    CMP_MARSHAL_READ_UINT(sArg->mXLogType);

    CMP_MARSHAL_STAGE(3);
    CMP_MARSHAL_READ_UINT(sArg->mTransID);

    CMP_MARSHAL_STAGE(2);
    CMP_MARSHAL_READ_ULONG(sArg->mSN);

    CMP_MARSHAL_STAGE(1);
    CMP_MARSHAL_READ_ULONG(sArg->mSyncSN);

    CMP_MARSHAL_STAGE(0);

    return ACI_SUCCESS;
    ACI_EXCEPTION_END;
    return ACI_FAILURE;
}

ACI_RC cmpWriteRPTrAbort(cmbBlock *aBlock, cmpProtocol *aProtocol, cmpMarshalState *aMarshalState)
{
    cmpArgRPTrAbort *sArg = CMI_PROTOCOL_GET_ARG(*aProtocol, RP, TrAbort);

    CMP_MARSHAL_BEGIN(4);
    CMP_MARSHAL_WRITE_UINT(sArg->mXLogType);

    CMP_MARSHAL_STAGE(3);
    CMP_MARSHAL_WRITE_UINT(sArg->mTransID);

    CMP_MARSHAL_STAGE(2);
    CMP_MARSHAL_WRITE_ULONG(sArg->mSN);

    CMP_MARSHAL_STAGE(1);
    CMP_MARSHAL_WRITE_ULONG(sArg->mSyncSN);

    CMP_MARSHAL_STAGE(0);

    return ACI_SUCCESS;
    ACI_EXCEPTION_END;
    return ACI_FAILURE;
}

ACI_RC cmpReadRPSPSet(cmbBlock *aBlock, cmpProtocol *aProtocol, cmpMarshalState *aMarshalState)
{
    cmpArgRPSPSet *sArg = CMI_PROTOCOL_GET_ARG(*aProtocol, RP, SPSet);

    CMP_MARSHAL_BEGIN(6);
    CMP_MARSHAL_READ_UINT(sArg->mXLogType);

    CMP_MARSHAL_STAGE(5);
    CMP_MARSHAL_READ_UINT(sArg->mTransID);

    CMP_MARSHAL_STAGE(4);
    CMP_MARSHAL_READ_ULONG(sArg->mSN);

    CMP_MARSHAL_STAGE(3);
    CMP_MARSHAL_READ_ULONG(sArg->mSyncSN);

    CMP_MARSHAL_STAGE(2);
    CMP_MARSHAL_READ_UINT(sArg->mSPNameLen);

    CMP_MARSHAL_STAGE(1);
    CMP_MARSHAL_READ_VARIABLE(sArg->mSPName);

    CMP_MARSHAL_STAGE(0);

    return ACI_SUCCESS;
    ACI_EXCEPTION_END;
    return ACI_FAILURE;
}

ACI_RC cmpWriteRPSPSet(cmbBlock *aBlock, cmpProtocol *aProtocol, cmpMarshalState *aMarshalState)
{
    cmpArgRPSPSet *sArg = CMI_PROTOCOL_GET_ARG(*aProtocol, RP, SPSet);

    CMP_MARSHAL_BEGIN(6);
    CMP_MARSHAL_WRITE_UINT(sArg->mXLogType);

    CMP_MARSHAL_STAGE(5);
    CMP_MARSHAL_WRITE_UINT(sArg->mTransID);

    CMP_MARSHAL_STAGE(4);
    CMP_MARSHAL_WRITE_ULONG(sArg->mSN);

    CMP_MARSHAL_STAGE(3);
    CMP_MARSHAL_WRITE_ULONG(sArg->mSyncSN);

    CMP_MARSHAL_STAGE(2);
    CMP_MARSHAL_WRITE_UINT(sArg->mSPNameLen);

    CMP_MARSHAL_STAGE(1);
    CMP_MARSHAL_WRITE_VARIABLE(sArg->mSPName);

    CMP_MARSHAL_STAGE(0);

    return ACI_SUCCESS;
    ACI_EXCEPTION_END;
    return ACI_FAILURE;
}

ACI_RC cmpReadRPSPAbort(cmbBlock *aBlock, cmpProtocol *aProtocol, cmpMarshalState *aMarshalState)
{
    cmpArgRPSPAbort *sArg = CMI_PROTOCOL_GET_ARG(*aProtocol, RP, SPAbort);

    CMP_MARSHAL_BEGIN(6);
    CMP_MARSHAL_READ_UINT(sArg->mXLogType);

    CMP_MARSHAL_STAGE(5);
    CMP_MARSHAL_READ_UINT(sArg->mTransID);

    CMP_MARSHAL_STAGE(4);
    CMP_MARSHAL_READ_ULONG(sArg->mSN);

    CMP_MARSHAL_STAGE(3);
    CMP_MARSHAL_READ_ULONG(sArg->mSyncSN);

    CMP_MARSHAL_STAGE(2);
    CMP_MARSHAL_READ_UINT(sArg->mSPNameLen);

    CMP_MARSHAL_STAGE(1);
    CMP_MARSHAL_READ_VARIABLE(sArg->mSPName);

    CMP_MARSHAL_STAGE(0);

    return ACI_SUCCESS;
    ACI_EXCEPTION_END;
    return ACI_FAILURE;
}

ACI_RC cmpWriteRPSPAbort(cmbBlock *aBlock, cmpProtocol *aProtocol, cmpMarshalState *aMarshalState)
{
    cmpArgRPSPAbort *sArg = CMI_PROTOCOL_GET_ARG(*aProtocol, RP, SPAbort);

    CMP_MARSHAL_BEGIN(6);
    CMP_MARSHAL_WRITE_UINT(sArg->mXLogType);

    CMP_MARSHAL_STAGE(5);
    CMP_MARSHAL_WRITE_UINT(sArg->mTransID);

    CMP_MARSHAL_STAGE(4);
    CMP_MARSHAL_WRITE_ULONG(sArg->mSN);

    CMP_MARSHAL_STAGE(3);
    CMP_MARSHAL_WRITE_ULONG(sArg->mSyncSN);

    CMP_MARSHAL_STAGE(2);
    CMP_MARSHAL_WRITE_UINT(sArg->mSPNameLen);

    CMP_MARSHAL_STAGE(1);
    CMP_MARSHAL_WRITE_VARIABLE(sArg->mSPName);

    CMP_MARSHAL_STAGE(0);

    return ACI_SUCCESS;
    ACI_EXCEPTION_END;
    return ACI_FAILURE;
}

ACI_RC cmpReadRPStmtBegin(cmbBlock *aBlock, cmpProtocol *aProtocol, cmpMarshalState *aMarshalState)
{
    cmpArgRPStmtBegin *sArg = CMI_PROTOCOL_GET_ARG(*aProtocol, RP, StmtBegin);

    CMP_MARSHAL_BEGIN(5);
    CMP_MARSHAL_READ_UINT(sArg->mXLogType);

    CMP_MARSHAL_STAGE(4);
    CMP_MARSHAL_READ_UINT(sArg->mTransID);

    CMP_MARSHAL_STAGE(3);
    CMP_MARSHAL_READ_ULONG(sArg->mSN);

    CMP_MARSHAL_STAGE(2);
    CMP_MARSHAL_READ_ULONG(sArg->mSyncSN);

    CMP_MARSHAL_STAGE(1);
    CMP_MARSHAL_READ_UINT(sArg->mStmtID);

    CMP_MARSHAL_STAGE(0);

    return ACI_SUCCESS;
    ACI_EXCEPTION_END;
    return ACI_FAILURE;
}

ACI_RC cmpWriteRPStmtBegin(cmbBlock *aBlock, cmpProtocol *aProtocol, cmpMarshalState *aMarshalState)
{
    cmpArgRPStmtBegin *sArg = CMI_PROTOCOL_GET_ARG(*aProtocol, RP, StmtBegin);

    CMP_MARSHAL_BEGIN(5);
    CMP_MARSHAL_WRITE_UINT(sArg->mXLogType);

    CMP_MARSHAL_STAGE(4);
    CMP_MARSHAL_WRITE_UINT(sArg->mTransID);

    CMP_MARSHAL_STAGE(3);
    CMP_MARSHAL_WRITE_ULONG(sArg->mSN);

    CMP_MARSHAL_STAGE(2);
    CMP_MARSHAL_WRITE_ULONG(sArg->mSyncSN);

    CMP_MARSHAL_STAGE(1);
    CMP_MARSHAL_WRITE_UINT(sArg->mStmtID);

    CMP_MARSHAL_STAGE(0);

    return ACI_SUCCESS;
    ACI_EXCEPTION_END;
    return ACI_FAILURE;
}

ACI_RC cmpReadRPStmtEnd(cmbBlock *aBlock, cmpProtocol *aProtocol, cmpMarshalState *aMarshalState)
{
    cmpArgRPStmtEnd *sArg = CMI_PROTOCOL_GET_ARG(*aProtocol, RP, StmtEnd);

    CMP_MARSHAL_BEGIN(5);
    CMP_MARSHAL_READ_UINT(sArg->mXLogType);

    CMP_MARSHAL_STAGE(4);
    CMP_MARSHAL_READ_UINT(sArg->mTransID);

    CMP_MARSHAL_STAGE(3);
    CMP_MARSHAL_READ_ULONG(sArg->mSN);

    CMP_MARSHAL_STAGE(2);
    CMP_MARSHAL_READ_ULONG(sArg->mSyncSN);

    CMP_MARSHAL_STAGE(1);
    CMP_MARSHAL_READ_UINT(sArg->mStmtID);

    CMP_MARSHAL_STAGE(0);

    return ACI_SUCCESS;
    ACI_EXCEPTION_END;
    return ACI_FAILURE;
}

ACI_RC cmpWriteRPStmtEnd(cmbBlock *aBlock, cmpProtocol *aProtocol, cmpMarshalState *aMarshalState)
{
    cmpArgRPStmtEnd *sArg = CMI_PROTOCOL_GET_ARG(*aProtocol, RP, StmtEnd);

    CMP_MARSHAL_BEGIN(5);
    CMP_MARSHAL_WRITE_UINT(sArg->mXLogType);

    CMP_MARSHAL_STAGE(4);
    CMP_MARSHAL_WRITE_UINT(sArg->mTransID);

    CMP_MARSHAL_STAGE(3);
    CMP_MARSHAL_WRITE_ULONG(sArg->mSN);

    CMP_MARSHAL_STAGE(2);
    CMP_MARSHAL_WRITE_ULONG(sArg->mSyncSN);

    CMP_MARSHAL_STAGE(1);
    CMP_MARSHAL_WRITE_UINT(sArg->mStmtID);

    CMP_MARSHAL_STAGE(0);

    return ACI_SUCCESS;
    ACI_EXCEPTION_END;
    return ACI_FAILURE;
}

ACI_RC cmpReadRPCursorOpen(cmbBlock *aBlock, cmpProtocol *aProtocol, cmpMarshalState *aMarshalState)
{
    cmpArgRPCursorOpen *sArg = CMI_PROTOCOL_GET_ARG(*aProtocol, RP, CursorOpen);

    CMP_MARSHAL_BEGIN(5);
    CMP_MARSHAL_READ_UINT(sArg->mXLogType);

    CMP_MARSHAL_STAGE(4);
    CMP_MARSHAL_READ_UINT(sArg->mTransID);

    CMP_MARSHAL_STAGE(3);
    CMP_MARSHAL_READ_ULONG(sArg->mSN);

    CMP_MARSHAL_STAGE(2);
    CMP_MARSHAL_READ_ULONG(sArg->mSyncSN);

    CMP_MARSHAL_STAGE(1);
    CMP_MARSHAL_READ_UINT(sArg->mCursorID);

    CMP_MARSHAL_STAGE(0);

    return ACI_SUCCESS;
    ACI_EXCEPTION_END;
    return ACI_FAILURE;
}

ACI_RC cmpWriteRPCursorOpen(cmbBlock *aBlock, cmpProtocol *aProtocol, cmpMarshalState *aMarshalState)
{
    cmpArgRPCursorOpen *sArg = CMI_PROTOCOL_GET_ARG(*aProtocol, RP, CursorOpen);

    CMP_MARSHAL_BEGIN(5);
    CMP_MARSHAL_WRITE_UINT(sArg->mXLogType);

    CMP_MARSHAL_STAGE(4);
    CMP_MARSHAL_WRITE_UINT(sArg->mTransID);

    CMP_MARSHAL_STAGE(3);
    CMP_MARSHAL_WRITE_ULONG(sArg->mSN);

    CMP_MARSHAL_STAGE(2);
    CMP_MARSHAL_WRITE_ULONG(sArg->mSyncSN);

    CMP_MARSHAL_STAGE(1);
    CMP_MARSHAL_WRITE_UINT(sArg->mCursorID);

    CMP_MARSHAL_STAGE(0);

    return ACI_SUCCESS;
    ACI_EXCEPTION_END;
    return ACI_FAILURE;
}

ACI_RC cmpReadRPCursorClose(cmbBlock *aBlock, cmpProtocol *aProtocol, cmpMarshalState *aMarshalState)
{
    cmpArgRPCursorClose *sArg = CMI_PROTOCOL_GET_ARG(*aProtocol, RP, CursorClose);

    CMP_MARSHAL_BEGIN(5);
    CMP_MARSHAL_READ_UINT(sArg->mXLogType);

    CMP_MARSHAL_STAGE(4);
    CMP_MARSHAL_READ_UINT(sArg->mTransID);

    CMP_MARSHAL_STAGE(3);
    CMP_MARSHAL_READ_ULONG(sArg->mSN);

    CMP_MARSHAL_STAGE(2);
    CMP_MARSHAL_READ_ULONG(sArg->mSyncSN);

    CMP_MARSHAL_STAGE(1);
    CMP_MARSHAL_READ_UINT(sArg->mCursorID);

    CMP_MARSHAL_STAGE(0);

    return ACI_SUCCESS;
    ACI_EXCEPTION_END;
    return ACI_FAILURE;
}

ACI_RC cmpWriteRPCursorClose(cmbBlock *aBlock, cmpProtocol *aProtocol, cmpMarshalState *aMarshalState)
{
    cmpArgRPCursorClose *sArg = CMI_PROTOCOL_GET_ARG(*aProtocol, RP, CursorClose);

    CMP_MARSHAL_BEGIN(5);
    CMP_MARSHAL_WRITE_UINT(sArg->mXLogType);

    CMP_MARSHAL_STAGE(4);
    CMP_MARSHAL_WRITE_UINT(sArg->mTransID);

    CMP_MARSHAL_STAGE(3);
    CMP_MARSHAL_WRITE_ULONG(sArg->mSN);

    CMP_MARSHAL_STAGE(2);
    CMP_MARSHAL_WRITE_ULONG(sArg->mSyncSN);

    CMP_MARSHAL_STAGE(1);
    CMP_MARSHAL_WRITE_UINT(sArg->mCursorID);

    CMP_MARSHAL_STAGE(0);

    return ACI_SUCCESS;
    ACI_EXCEPTION_END;
    return ACI_FAILURE;
}

ACI_RC cmpReadRPInsert(cmbBlock *aBlock, cmpProtocol *aProtocol, cmpMarshalState *aMarshalState)
{
    cmpArgRPInsert *sArg = CMI_PROTOCOL_GET_ARG(*aProtocol, RP, Insert);

    CMP_MARSHAL_BEGIN(7);
    CMP_MARSHAL_READ_UINT(sArg->mXLogType);

    CMP_MARSHAL_STAGE(6);
    CMP_MARSHAL_READ_UINT(sArg->mTransID);

    CMP_MARSHAL_STAGE(5);
    CMP_MARSHAL_READ_ULONG(sArg->mSN);

    CMP_MARSHAL_STAGE(4);
    CMP_MARSHAL_READ_ULONG(sArg->mSyncSN);

    CMP_MARSHAL_STAGE(3);
    CMP_MARSHAL_READ_UINT(sArg->mImplSPDepth);

    CMP_MARSHAL_STAGE(2);
    CMP_MARSHAL_READ_ULONG(sArg->mTableOID);

    CMP_MARSHAL_STAGE(1);
    CMP_MARSHAL_READ_UINT(sArg->mColCnt);

    CMP_MARSHAL_STAGE(0);

    return ACI_SUCCESS;
    ACI_EXCEPTION_END;
    return ACI_FAILURE;
}

ACI_RC cmpWriteRPInsert(cmbBlock *aBlock, cmpProtocol *aProtocol, cmpMarshalState *aMarshalState)
{
    cmpArgRPInsert *sArg = CMI_PROTOCOL_GET_ARG(*aProtocol, RP, Insert);

    CMP_MARSHAL_BEGIN(7);
    CMP_MARSHAL_WRITE_UINT(sArg->mXLogType);

    CMP_MARSHAL_STAGE(6);
    CMP_MARSHAL_WRITE_UINT(sArg->mTransID);

    CMP_MARSHAL_STAGE(5);
    CMP_MARSHAL_WRITE_ULONG(sArg->mSN);

    CMP_MARSHAL_STAGE(4);
    CMP_MARSHAL_WRITE_ULONG(sArg->mSyncSN);

    CMP_MARSHAL_STAGE(3);
    CMP_MARSHAL_WRITE_UINT(sArg->mImplSPDepth);

    CMP_MARSHAL_STAGE(2);
    CMP_MARSHAL_WRITE_ULONG(sArg->mTableOID);

    CMP_MARSHAL_STAGE(1);
    CMP_MARSHAL_WRITE_UINT(sArg->mColCnt);

    CMP_MARSHAL_STAGE(0);

    return ACI_SUCCESS;
    ACI_EXCEPTION_END;
    return ACI_FAILURE;
}

ACI_RC cmpReadRPUpdate(cmbBlock *aBlock, cmpProtocol *aProtocol, cmpMarshalState *aMarshalState)
{
    cmpArgRPUpdate *sArg = CMI_PROTOCOL_GET_ARG(*aProtocol, RP, Update);

    CMP_MARSHAL_BEGIN(8);
    CMP_MARSHAL_READ_UINT(sArg->mXLogType);

    CMP_MARSHAL_STAGE(7);
    CMP_MARSHAL_READ_UINT(sArg->mTransID);

    CMP_MARSHAL_STAGE(6);
    CMP_MARSHAL_READ_ULONG(sArg->mSN);

    CMP_MARSHAL_STAGE(5);
    CMP_MARSHAL_READ_ULONG(sArg->mSyncSN);

    CMP_MARSHAL_STAGE(4);
    CMP_MARSHAL_READ_UINT(sArg->mImplSPDepth);

    CMP_MARSHAL_STAGE(3);
    CMP_MARSHAL_READ_ULONG(sArg->mTableOID);

    CMP_MARSHAL_STAGE(2);
    CMP_MARSHAL_READ_UINT(sArg->mPKColCnt);

    CMP_MARSHAL_STAGE(1);
    CMP_MARSHAL_READ_UINT(sArg->mUpdateColCnt);

    CMP_MARSHAL_STAGE(0);

    return ACI_SUCCESS;
    ACI_EXCEPTION_END;
    return ACI_FAILURE;
}

ACI_RC cmpWriteRPUpdate(cmbBlock *aBlock, cmpProtocol *aProtocol, cmpMarshalState *aMarshalState)
{
    cmpArgRPUpdate *sArg = CMI_PROTOCOL_GET_ARG(*aProtocol, RP, Update);

    CMP_MARSHAL_BEGIN(8);
    CMP_MARSHAL_WRITE_UINT(sArg->mXLogType);

    CMP_MARSHAL_STAGE(7);
    CMP_MARSHAL_WRITE_UINT(sArg->mTransID);

    CMP_MARSHAL_STAGE(6);
    CMP_MARSHAL_WRITE_ULONG(sArg->mSN);

    CMP_MARSHAL_STAGE(5);
    CMP_MARSHAL_WRITE_ULONG(sArg->mSyncSN);

    CMP_MARSHAL_STAGE(4);
    CMP_MARSHAL_WRITE_UINT(sArg->mImplSPDepth);

    CMP_MARSHAL_STAGE(3);
    CMP_MARSHAL_WRITE_ULONG(sArg->mTableOID);

    CMP_MARSHAL_STAGE(2);
    CMP_MARSHAL_WRITE_UINT(sArg->mPKColCnt);

    CMP_MARSHAL_STAGE(1);
    CMP_MARSHAL_WRITE_UINT(sArg->mUpdateColCnt);

    CMP_MARSHAL_STAGE(0);

    return ACI_SUCCESS;
    ACI_EXCEPTION_END;
    return ACI_FAILURE;
}

ACI_RC cmpReadRPDelete(cmbBlock *aBlock, cmpProtocol *aProtocol, cmpMarshalState *aMarshalState)
{
    cmpArgRPDelete *sArg = CMI_PROTOCOL_GET_ARG(*aProtocol, RP, Delete);

    CMP_MARSHAL_BEGIN(7);
    CMP_MARSHAL_READ_UINT(sArg->mXLogType);

    CMP_MARSHAL_STAGE(6);
    CMP_MARSHAL_READ_UINT(sArg->mTransID);

    CMP_MARSHAL_STAGE(5);
    CMP_MARSHAL_READ_ULONG(sArg->mSN);

    CMP_MARSHAL_STAGE(4);
    CMP_MARSHAL_READ_ULONG(sArg->mSyncSN);

    CMP_MARSHAL_STAGE(3);
    CMP_MARSHAL_READ_UINT(sArg->mImplSPDepth);

    CMP_MARSHAL_STAGE(2);
    CMP_MARSHAL_READ_ULONG(sArg->mTableOID);

    CMP_MARSHAL_STAGE(1);
    CMP_MARSHAL_READ_UINT(sArg->mPKColCnt);

    CMP_MARSHAL_STAGE(0);

    return ACI_SUCCESS;
    ACI_EXCEPTION_END;
    return ACI_FAILURE;
}

ACI_RC cmpWriteRPDelete(cmbBlock *aBlock, cmpProtocol *aProtocol, cmpMarshalState *aMarshalState)
{
    cmpArgRPDelete *sArg = CMI_PROTOCOL_GET_ARG(*aProtocol, RP, Delete);

    CMP_MARSHAL_BEGIN(7);
    CMP_MARSHAL_WRITE_UINT(sArg->mXLogType);

    CMP_MARSHAL_STAGE(6);
    CMP_MARSHAL_WRITE_UINT(sArg->mTransID);

    CMP_MARSHAL_STAGE(5);
    CMP_MARSHAL_WRITE_ULONG(sArg->mSN);

    CMP_MARSHAL_STAGE(4);
    CMP_MARSHAL_WRITE_ULONG(sArg->mSyncSN);

    CMP_MARSHAL_STAGE(3);
    CMP_MARSHAL_WRITE_UINT(sArg->mImplSPDepth);

    CMP_MARSHAL_STAGE(2);
    CMP_MARSHAL_WRITE_ULONG(sArg->mTableOID);

    CMP_MARSHAL_STAGE(1);
    CMP_MARSHAL_WRITE_UINT(sArg->mPKColCnt);

    CMP_MARSHAL_STAGE(0);

    return ACI_SUCCESS;
    ACI_EXCEPTION_END;
    return ACI_FAILURE;
}

ACI_RC cmpReadRPUIntID(cmbBlock *aBlock, cmpProtocol *aProtocol, cmpMarshalState *aMarshalState)
{
    cmpArgRPUIntID *sArg = CMI_PROTOCOL_GET_ARG(*aProtocol, RP, UIntID);

    CMP_MARSHAL_BEGIN(1);
    CMP_MARSHAL_READ_UINT(sArg->mUIntID);

    CMP_MARSHAL_STAGE(0);

    return ACI_SUCCESS;
    ACI_EXCEPTION_END;
    return ACI_FAILURE;
}

ACI_RC cmpWriteRPUIntID(cmbBlock *aBlock, cmpProtocol *aProtocol, cmpMarshalState *aMarshalState)
{
    cmpArgRPUIntID *sArg = CMI_PROTOCOL_GET_ARG(*aProtocol, RP, UIntID);

    CMP_MARSHAL_BEGIN(1);
    CMP_MARSHAL_WRITE_UINT(sArg->mUIntID);

    CMP_MARSHAL_STAGE(0);

    return ACI_SUCCESS;
    ACI_EXCEPTION_END;
    return ACI_FAILURE;
}

ACI_RC cmpReadRPValue(cmbBlock *aBlock, cmpProtocol *aProtocol, cmpMarshalState *aMarshalState)
{
    cmpArgRPValue *sArg = CMI_PROTOCOL_GET_ARG(*aProtocol, RP, Value);

    CMP_MARSHAL_BEGIN(1);
    CMP_MARSHAL_READ_VARIABLE(sArg->mValue);

    CMP_MARSHAL_STAGE(0);

    return ACI_SUCCESS;
    ACI_EXCEPTION_END;
    return ACI_FAILURE;
}

ACI_RC cmpWriteRPValue(cmbBlock *aBlock, cmpProtocol *aProtocol, cmpMarshalState *aMarshalState)
{
    cmpArgRPValue *sArg = CMI_PROTOCOL_GET_ARG(*aProtocol, RP, Value);

    CMP_MARSHAL_BEGIN(1);
    CMP_MARSHAL_WRITE_VARIABLE(sArg->mValue);

    CMP_MARSHAL_STAGE(0);

    return ACI_SUCCESS;
    ACI_EXCEPTION_END;
    return ACI_FAILURE;
}

ACI_RC cmpReadRPStop(cmbBlock *aBlock, cmpProtocol *aProtocol, cmpMarshalState *aMarshalState)
{
    cmpArgRPStop *sArg = CMI_PROTOCOL_GET_ARG(*aProtocol, RP, Stop);

    CMP_MARSHAL_BEGIN(5);
    CMP_MARSHAL_READ_UINT(sArg->mXLogType);

    CMP_MARSHAL_STAGE(4);
    CMP_MARSHAL_READ_UINT(sArg->mTransID);

    CMP_MARSHAL_STAGE(3);
    CMP_MARSHAL_READ_ULONG(sArg->mSN);

    CMP_MARSHAL_STAGE(2);
    CMP_MARSHAL_READ_ULONG(sArg->mSyncSN);

    CMP_MARSHAL_STAGE(1);
    CMP_MARSHAL_READ_ULONG(sArg->mRestartSN);

    CMP_MARSHAL_STAGE(0);

    return ACI_SUCCESS;
    ACI_EXCEPTION_END;
    return ACI_FAILURE;
}

ACI_RC cmpWriteRPStop(cmbBlock *aBlock, cmpProtocol *aProtocol, cmpMarshalState *aMarshalState)
{
    cmpArgRPStop *sArg = CMI_PROTOCOL_GET_ARG(*aProtocol, RP, Stop);

    CMP_MARSHAL_BEGIN(5);
    CMP_MARSHAL_WRITE_UINT(sArg->mXLogType);

    CMP_MARSHAL_STAGE(4);
    CMP_MARSHAL_WRITE_UINT(sArg->mTransID);

    CMP_MARSHAL_STAGE(3);
    CMP_MARSHAL_WRITE_ULONG(sArg->mSN);

    CMP_MARSHAL_STAGE(2);
    CMP_MARSHAL_WRITE_ULONG(sArg->mSyncSN);

    CMP_MARSHAL_STAGE(1);
    CMP_MARSHAL_WRITE_ULONG(sArg->mRestartSN);

    CMP_MARSHAL_STAGE(0);

    return ACI_SUCCESS;
    ACI_EXCEPTION_END;
    return ACI_FAILURE;
}

ACI_RC cmpReadRPKeepAlive(cmbBlock *aBlock, cmpProtocol *aProtocol, cmpMarshalState *aMarshalState)
{
    cmpArgRPKeepAlive *sArg = CMI_PROTOCOL_GET_ARG(*aProtocol, RP, KeepAlive);

    CMP_MARSHAL_BEGIN(5);
    CMP_MARSHAL_READ_UINT(sArg->mXLogType);

    CMP_MARSHAL_STAGE(4);
    CMP_MARSHAL_READ_UINT(sArg->mTransID);

    CMP_MARSHAL_STAGE(3);
    CMP_MARSHAL_READ_ULONG(sArg->mSN);

    CMP_MARSHAL_STAGE(2);
    CMP_MARSHAL_READ_ULONG(sArg->mSyncSN);

    CMP_MARSHAL_STAGE(1);
    CMP_MARSHAL_READ_ULONG(sArg->mRestartSN);

    CMP_MARSHAL_STAGE(0);

    return ACI_SUCCESS;
    ACI_EXCEPTION_END;
    return ACI_FAILURE;
}

ACI_RC cmpWriteRPKeepAlive(cmbBlock *aBlock, cmpProtocol *aProtocol, cmpMarshalState *aMarshalState)
{
    cmpArgRPKeepAlive *sArg = CMI_PROTOCOL_GET_ARG(*aProtocol, RP, KeepAlive);

    CMP_MARSHAL_BEGIN(5);
    CMP_MARSHAL_WRITE_UINT(sArg->mXLogType);

    CMP_MARSHAL_STAGE(4);
    CMP_MARSHAL_WRITE_UINT(sArg->mTransID);

    CMP_MARSHAL_STAGE(3);
    CMP_MARSHAL_WRITE_ULONG(sArg->mSN);

    CMP_MARSHAL_STAGE(2);
    CMP_MARSHAL_WRITE_ULONG(sArg->mSyncSN);

    CMP_MARSHAL_STAGE(1);
    CMP_MARSHAL_WRITE_ULONG(sArg->mRestartSN);

    CMP_MARSHAL_STAGE(0);

    return ACI_SUCCESS;
    ACI_EXCEPTION_END;
    return ACI_FAILURE;
}

ACI_RC cmpReadRPFlush(cmbBlock *aBlock, cmpProtocol *aProtocol, cmpMarshalState *aMarshalState)
{
    cmpArgRPFlush *sArg = CMI_PROTOCOL_GET_ARG(*aProtocol, RP, Flush);

    CMP_MARSHAL_BEGIN(5);
    CMP_MARSHAL_READ_UINT(sArg->mXLogType);

    CMP_MARSHAL_STAGE(4);
    CMP_MARSHAL_READ_UINT(sArg->mTransID);

    CMP_MARSHAL_STAGE(3);
    CMP_MARSHAL_READ_ULONG(sArg->mSN);

    CMP_MARSHAL_STAGE(2);
    CMP_MARSHAL_READ_ULONG(sArg->mSyncSN);

    CMP_MARSHAL_STAGE(1);
    CMP_MARSHAL_READ_UINT(sArg->mOption);

    CMP_MARSHAL_STAGE(0);

    return ACI_SUCCESS;
    ACI_EXCEPTION_END;
    return ACI_FAILURE;
}

ACI_RC cmpWriteRPFlush(cmbBlock *aBlock, cmpProtocol *aProtocol, cmpMarshalState *aMarshalState)
{
    cmpArgRPFlush *sArg = CMI_PROTOCOL_GET_ARG(*aProtocol, RP, Flush);

    CMP_MARSHAL_BEGIN(5);
    CMP_MARSHAL_WRITE_UINT(sArg->mXLogType);

    CMP_MARSHAL_STAGE(4);
    CMP_MARSHAL_WRITE_UINT(sArg->mTransID);

    CMP_MARSHAL_STAGE(3);
    CMP_MARSHAL_WRITE_ULONG(sArg->mSN);

    CMP_MARSHAL_STAGE(2);
    CMP_MARSHAL_WRITE_ULONG(sArg->mSyncSN);

    CMP_MARSHAL_STAGE(1);
    CMP_MARSHAL_WRITE_UINT(sArg->mOption);

    CMP_MARSHAL_STAGE(0);

    return ACI_SUCCESS;
    ACI_EXCEPTION_END;
    return ACI_FAILURE;
}

ACI_RC cmpReadRPFlushAck(cmbBlock *aBlock, cmpProtocol *aProtocol, cmpMarshalState *aMarshalState)
{
    cmpArgRPFlushAck *sArg = CMI_PROTOCOL_GET_ARG(*aProtocol, RP, FlushAck);

    CMP_MARSHAL_BEGIN(4);
    CMP_MARSHAL_READ_UINT(sArg->mXLogType);

    CMP_MARSHAL_STAGE(3);
    CMP_MARSHAL_READ_UINT(sArg->mTransID);

    CMP_MARSHAL_STAGE(2);
    CMP_MARSHAL_READ_ULONG(sArg->mSN);

    CMP_MARSHAL_STAGE(1);
    CMP_MARSHAL_READ_ULONG(sArg->mSyncSN);

    CMP_MARSHAL_STAGE(0);

    return ACI_SUCCESS;
    ACI_EXCEPTION_END;
    return ACI_FAILURE;
}

ACI_RC cmpWriteRPFlushAck(cmbBlock *aBlock, cmpProtocol *aProtocol, cmpMarshalState *aMarshalState)
{
    cmpArgRPFlushAck *sArg = CMI_PROTOCOL_GET_ARG(*aProtocol, RP, FlushAck);

    CMP_MARSHAL_BEGIN(4);
    CMP_MARSHAL_WRITE_UINT(sArg->mXLogType);

    CMP_MARSHAL_STAGE(3);
    CMP_MARSHAL_WRITE_UINT(sArg->mTransID);

    CMP_MARSHAL_STAGE(2);
    CMP_MARSHAL_WRITE_ULONG(sArg->mSN);

    CMP_MARSHAL_STAGE(1);
    CMP_MARSHAL_WRITE_ULONG(sArg->mSyncSN);

    CMP_MARSHAL_STAGE(0);

    return ACI_SUCCESS;
    ACI_EXCEPTION_END;
    return ACI_FAILURE;
}

ACI_RC cmpReadRPAck(cmbBlock *aBlock, cmpProtocol *aProtocol, cmpMarshalState *aMarshalState)
{
    cmpArgRPAck *sArg = CMI_PROTOCOL_GET_ARG(*aProtocol, RP, Ack);

    CMP_MARSHAL_BEGIN(8);
    CMP_MARSHAL_READ_UINT(sArg->mXLogType);

    CMP_MARSHAL_STAGE(7);
    CMP_MARSHAL_READ_UINT(sArg->mAbortTxCount);

    CMP_MARSHAL_STAGE(6);
    CMP_MARSHAL_READ_UINT(sArg->mClearTxCount);

    CMP_MARSHAL_STAGE(5);
    CMP_MARSHAL_READ_ULONG(sArg->mRestartSN);

    CMP_MARSHAL_STAGE(4);
    CMP_MARSHAL_READ_ULONG(sArg->mLastCommitSN);

    CMP_MARSHAL_STAGE(3);
    CMP_MARSHAL_READ_ULONG(sArg->mLastArrivedSN);

    CMP_MARSHAL_STAGE(2);
    CMP_MARSHAL_READ_ULONG(sArg->mLastProcessedSN);

    CMP_MARSHAL_STAGE(1);
    CMP_MARSHAL_READ_ULONG(sArg->mFlushSN);

    CMP_MARSHAL_STAGE(0);

    return ACI_SUCCESS;
    ACI_EXCEPTION_END;
    return ACI_FAILURE;
}

ACI_RC cmpWriteRPAck(cmbBlock *aBlock, cmpProtocol *aProtocol, cmpMarshalState *aMarshalState)
{
    cmpArgRPAck *sArg = CMI_PROTOCOL_GET_ARG(*aProtocol, RP, Ack);

    CMP_MARSHAL_BEGIN(8);
    CMP_MARSHAL_WRITE_UINT(sArg->mXLogType);

    CMP_MARSHAL_STAGE(7);
    CMP_MARSHAL_WRITE_UINT(sArg->mAbortTxCount);

    CMP_MARSHAL_STAGE(6);
    CMP_MARSHAL_WRITE_UINT(sArg->mClearTxCount);

    CMP_MARSHAL_STAGE(5);
    CMP_MARSHAL_WRITE_ULONG(sArg->mRestartSN);

    CMP_MARSHAL_STAGE(4);
    CMP_MARSHAL_WRITE_ULONG(sArg->mLastCommitSN);

    CMP_MARSHAL_STAGE(3);
    CMP_MARSHAL_WRITE_ULONG(sArg->mLastArrivedSN);

    CMP_MARSHAL_STAGE(2);
    CMP_MARSHAL_WRITE_ULONG(sArg->mLastProcessedSN);

    CMP_MARSHAL_STAGE(1);
    CMP_MARSHAL_WRITE_ULONG(sArg->mFlushSN);

    CMP_MARSHAL_STAGE(0);

    return ACI_SUCCESS;
    ACI_EXCEPTION_END;
    return ACI_FAILURE;
}

ACI_RC cmpReadRPLobCursorOpen(cmbBlock *aBlock, cmpProtocol *aProtocol, cmpMarshalState *aMarshalState)
{
    cmpArgRPLobCursorOpen *sArg = CMI_PROTOCOL_GET_ARG(*aProtocol, RP, LobCursorOpen);

    CMP_MARSHAL_BEGIN(8);
    CMP_MARSHAL_READ_UINT(sArg->mXLogType);

    CMP_MARSHAL_STAGE(7);
    CMP_MARSHAL_READ_UINT(sArg->mTransID);

    CMP_MARSHAL_STAGE(6);
    CMP_MARSHAL_READ_ULONG(sArg->mSN);

    CMP_MARSHAL_STAGE(5);
    CMP_MARSHAL_READ_ULONG(sArg->mSyncSN);

    CMP_MARSHAL_STAGE(4);
    CMP_MARSHAL_READ_ULONG(sArg->mTableOID);

    CMP_MARSHAL_STAGE(3);
    CMP_MARSHAL_READ_ULONG(sArg->mLobLocator);

    CMP_MARSHAL_STAGE(2);
    CMP_MARSHAL_READ_UINT(sArg->mColumnID);

    CMP_MARSHAL_STAGE(1);
    CMP_MARSHAL_READ_UINT(sArg->mPKColCnt);

    CMP_MARSHAL_STAGE(0);

    return ACI_SUCCESS;
    ACI_EXCEPTION_END;
    return ACI_FAILURE;
}

ACI_RC cmpWriteRPLobCursorOpen(cmbBlock *aBlock, cmpProtocol *aProtocol, cmpMarshalState *aMarshalState)
{
    cmpArgRPLobCursorOpen *sArg = CMI_PROTOCOL_GET_ARG(*aProtocol, RP, LobCursorOpen);

    CMP_MARSHAL_BEGIN(8);
    CMP_MARSHAL_WRITE_UINT(sArg->mXLogType);

    CMP_MARSHAL_STAGE(7);
    CMP_MARSHAL_WRITE_UINT(sArg->mTransID);

    CMP_MARSHAL_STAGE(6);
    CMP_MARSHAL_WRITE_ULONG(sArg->mSN);

    CMP_MARSHAL_STAGE(5);
    CMP_MARSHAL_WRITE_ULONG(sArg->mSyncSN);

    CMP_MARSHAL_STAGE(4);
    CMP_MARSHAL_WRITE_ULONG(sArg->mTableOID);

    CMP_MARSHAL_STAGE(3);
    CMP_MARSHAL_WRITE_ULONG(sArg->mLobLocator);

    CMP_MARSHAL_STAGE(2);
    CMP_MARSHAL_WRITE_UINT(sArg->mColumnID);

    CMP_MARSHAL_STAGE(1);
    CMP_MARSHAL_WRITE_UINT(sArg->mPKColCnt);

    CMP_MARSHAL_STAGE(0);

    return ACI_SUCCESS;
    ACI_EXCEPTION_END;
    return ACI_FAILURE;
}

ACI_RC cmpReadRPLobCursorClose(cmbBlock *aBlock, cmpProtocol *aProtocol, cmpMarshalState *aMarshalState)
{
    cmpArgRPLobCursorClose *sArg = CMI_PROTOCOL_GET_ARG(*aProtocol, RP, LobCursorClose);

    CMP_MARSHAL_BEGIN(5);
    CMP_MARSHAL_READ_UINT(sArg->mXLogType);

    CMP_MARSHAL_STAGE(4);
    CMP_MARSHAL_READ_UINT(sArg->mTransID);

    CMP_MARSHAL_STAGE(3);
    CMP_MARSHAL_READ_ULONG(sArg->mSN);

    CMP_MARSHAL_STAGE(2);
    CMP_MARSHAL_READ_ULONG(sArg->mSyncSN);

    CMP_MARSHAL_STAGE(1);
    CMP_MARSHAL_READ_ULONG(sArg->mLobLocator);

    CMP_MARSHAL_STAGE(0);

    return ACI_SUCCESS;
    ACI_EXCEPTION_END;
    return ACI_FAILURE;
}

ACI_RC cmpWriteRPLobCursorClose(cmbBlock *aBlock, cmpProtocol *aProtocol, cmpMarshalState *aMarshalState)
{
    cmpArgRPLobCursorClose *sArg = CMI_PROTOCOL_GET_ARG(*aProtocol, RP, LobCursorClose);

    CMP_MARSHAL_BEGIN(5);
    CMP_MARSHAL_WRITE_UINT(sArg->mXLogType);

    CMP_MARSHAL_STAGE(4);
    CMP_MARSHAL_WRITE_UINT(sArg->mTransID);

    CMP_MARSHAL_STAGE(3);
    CMP_MARSHAL_WRITE_ULONG(sArg->mSN);

    CMP_MARSHAL_STAGE(2);
    CMP_MARSHAL_WRITE_ULONG(sArg->mSyncSN);

    CMP_MARSHAL_STAGE(1);
    CMP_MARSHAL_WRITE_ULONG(sArg->mLobLocator);

    CMP_MARSHAL_STAGE(0);

    return ACI_SUCCESS;
    ACI_EXCEPTION_END;
    return ACI_FAILURE;
}

ACI_RC cmpReadRPLobPrepare4Write(cmbBlock *aBlock, cmpProtocol *aProtocol, cmpMarshalState *aMarshalState)
{
    cmpArgRPLobPrepare4Write *sArg = CMI_PROTOCOL_GET_ARG(*aProtocol, RP, LobPrepare4Write);

    CMP_MARSHAL_BEGIN(8);
    CMP_MARSHAL_READ_UINT(sArg->mXLogType);

    CMP_MARSHAL_STAGE(7);
    CMP_MARSHAL_READ_UINT(sArg->mTransID);

    CMP_MARSHAL_STAGE(6);
    CMP_MARSHAL_READ_ULONG(sArg->mSN);

    CMP_MARSHAL_STAGE(5);
    CMP_MARSHAL_READ_ULONG(sArg->mSyncSN);

    CMP_MARSHAL_STAGE(4);
    CMP_MARSHAL_READ_ULONG(sArg->mLobLocator);

    CMP_MARSHAL_STAGE(3);
    CMP_MARSHAL_READ_UINT(sArg->mOffset);

    CMP_MARSHAL_STAGE(2);
    CMP_MARSHAL_READ_UINT(sArg->mOldSize);

    CMP_MARSHAL_STAGE(1);
    CMP_MARSHAL_READ_UINT(sArg->mNewSize);

    CMP_MARSHAL_STAGE(0);

    return ACI_SUCCESS;
    ACI_EXCEPTION_END;
    return ACI_FAILURE;
}

ACI_RC cmpWriteRPLobPrepare4Write(cmbBlock *aBlock, cmpProtocol *aProtocol, cmpMarshalState *aMarshalState)
{
    cmpArgRPLobPrepare4Write *sArg = CMI_PROTOCOL_GET_ARG(*aProtocol, RP, LobPrepare4Write);

    CMP_MARSHAL_BEGIN(8);
    CMP_MARSHAL_WRITE_UINT(sArg->mXLogType);

    CMP_MARSHAL_STAGE(7);
    CMP_MARSHAL_WRITE_UINT(sArg->mTransID);

    CMP_MARSHAL_STAGE(6);
    CMP_MARSHAL_WRITE_ULONG(sArg->mSN);

    CMP_MARSHAL_STAGE(5);
    CMP_MARSHAL_WRITE_ULONG(sArg->mSyncSN);

    CMP_MARSHAL_STAGE(4);
    CMP_MARSHAL_WRITE_ULONG(sArg->mLobLocator);

    CMP_MARSHAL_STAGE(3);
    CMP_MARSHAL_WRITE_UINT(sArg->mOffset);

    CMP_MARSHAL_STAGE(2);
    CMP_MARSHAL_WRITE_UINT(sArg->mOldSize);

    CMP_MARSHAL_STAGE(1);
    CMP_MARSHAL_WRITE_UINT(sArg->mNewSize);

    CMP_MARSHAL_STAGE(0);

    return ACI_SUCCESS;
    ACI_EXCEPTION_END;
    return ACI_FAILURE;
}

ACI_RC cmpReadRPLobTrim(cmbBlock *aBlock, cmpProtocol *aProtocol, cmpMarshalState *aMarshalState)
{
    cmpArgRPLobTrim *sArg = CMI_PROTOCOL_GET_ARG(*aProtocol, RP, LobTrim);

    CMP_MARSHAL_BEGIN(6);
    CMP_MARSHAL_READ_UINT(sArg->mXLogType);

    CMP_MARSHAL_STAGE(5);
    CMP_MARSHAL_READ_UINT(sArg->mTransID);

    CMP_MARSHAL_STAGE(4);
    CMP_MARSHAL_READ_ULONG(sArg->mSN);

    CMP_MARSHAL_STAGE(3);
    CMP_MARSHAL_READ_ULONG(sArg->mSyncSN);

    CMP_MARSHAL_STAGE(2);
    CMP_MARSHAL_READ_ULONG(sArg->mLobLocator);

    CMP_MARSHAL_STAGE(1);
    CMP_MARSHAL_READ_UINT(sArg->mOffset);

    CMP_MARSHAL_STAGE(0);

    return ACI_SUCCESS;
    ACI_EXCEPTION_END;
    return ACI_FAILURE;
}

ACI_RC cmpWriteRPLobTrim(cmbBlock *aBlock, cmpProtocol *aProtocol, cmpMarshalState *aMarshalState)
{
    cmpArgRPLobTrim *sArg = CMI_PROTOCOL_GET_ARG(*aProtocol, RP, LobTrim);

    CMP_MARSHAL_BEGIN(6);
    CMP_MARSHAL_WRITE_UINT(sArg->mXLogType);

    CMP_MARSHAL_STAGE(5);
    CMP_MARSHAL_WRITE_UINT(sArg->mTransID);

    CMP_MARSHAL_STAGE(4);
    CMP_MARSHAL_WRITE_ULONG(sArg->mSN);

    CMP_MARSHAL_STAGE(3);
    CMP_MARSHAL_WRITE_ULONG(sArg->mSyncSN);

    CMP_MARSHAL_STAGE(2);
    CMP_MARSHAL_WRITE_ULONG(sArg->mLobLocator);

    CMP_MARSHAL_STAGE(1);
    CMP_MARSHAL_WRITE_UINT(sArg->mOffset);

    CMP_MARSHAL_STAGE(0);

    return ACI_SUCCESS;
    ACI_EXCEPTION_END;
    return ACI_FAILURE;
}

ACI_RC cmpReadRPLobPartialWrite(cmbBlock *aBlock, cmpProtocol *aProtocol, cmpMarshalState *aMarshalState)
{
    cmpArgRPLobPartialWrite *sArg = CMI_PROTOCOL_GET_ARG(*aProtocol, RP, LobPartialWrite);

    CMP_MARSHAL_BEGIN(8);
    CMP_MARSHAL_READ_UINT(sArg->mXLogType);

    CMP_MARSHAL_STAGE(7);
    CMP_MARSHAL_READ_UINT(sArg->mTransID);

    CMP_MARSHAL_STAGE(6);
    CMP_MARSHAL_READ_ULONG(sArg->mSN);

    CMP_MARSHAL_STAGE(5);
    CMP_MARSHAL_READ_ULONG(sArg->mSyncSN);

    CMP_MARSHAL_STAGE(4);
    CMP_MARSHAL_READ_ULONG(sArg->mLobLocator);

    CMP_MARSHAL_STAGE(3);
    CMP_MARSHAL_READ_UINT(sArg->mOffset);

    CMP_MARSHAL_STAGE(2);
    CMP_MARSHAL_READ_UINT(sArg->mPieceLen);

    CMP_MARSHAL_STAGE(1);
    CMP_MARSHAL_READ_VARIABLE(sArg->mPieceValue);

    CMP_MARSHAL_STAGE(0);

    return ACI_SUCCESS;
    ACI_EXCEPTION_END;
    return ACI_FAILURE;
}

ACI_RC cmpWriteRPLobPartialWrite(cmbBlock *aBlock, cmpProtocol *aProtocol, cmpMarshalState *aMarshalState)
{
    cmpArgRPLobPartialWrite *sArg = CMI_PROTOCOL_GET_ARG(*aProtocol, RP, LobPartialWrite);

    CMP_MARSHAL_BEGIN(8);
    CMP_MARSHAL_WRITE_UINT(sArg->mXLogType);

    CMP_MARSHAL_STAGE(7);
    CMP_MARSHAL_WRITE_UINT(sArg->mTransID);

    CMP_MARSHAL_STAGE(6);
    CMP_MARSHAL_WRITE_ULONG(sArg->mSN);

    CMP_MARSHAL_STAGE(5);
    CMP_MARSHAL_WRITE_ULONG(sArg->mSyncSN);

    CMP_MARSHAL_STAGE(4);
    CMP_MARSHAL_WRITE_ULONG(sArg->mLobLocator);

    CMP_MARSHAL_STAGE(3);
    CMP_MARSHAL_WRITE_UINT(sArg->mOffset);

    CMP_MARSHAL_STAGE(2);
    CMP_MARSHAL_WRITE_UINT(sArg->mPieceLen);

    CMP_MARSHAL_STAGE(1);
    CMP_MARSHAL_WRITE_VARIABLE(sArg->mPieceValue);

    CMP_MARSHAL_STAGE(0);

    return ACI_SUCCESS;
    ACI_EXCEPTION_END;
    return ACI_FAILURE;
}

ACI_RC cmpReadRPLobFinish2Write(cmbBlock *aBlock, cmpProtocol *aProtocol, cmpMarshalState *aMarshalState)
{
    cmpArgRPLobFinish2Write *sArg = CMI_PROTOCOL_GET_ARG(*aProtocol, RP, LobFinish2Write);

    CMP_MARSHAL_BEGIN(5);
    CMP_MARSHAL_READ_UINT(sArg->mXLogType);

    CMP_MARSHAL_STAGE(4);
    CMP_MARSHAL_READ_UINT(sArg->mTransID);

    CMP_MARSHAL_STAGE(3);
    CMP_MARSHAL_READ_ULONG(sArg->mSN);

    CMP_MARSHAL_STAGE(2);
    CMP_MARSHAL_READ_ULONG(sArg->mSyncSN);

    CMP_MARSHAL_STAGE(1);
    CMP_MARSHAL_READ_ULONG(sArg->mLobLocator);

    CMP_MARSHAL_STAGE(0);

    return ACI_SUCCESS;
    ACI_EXCEPTION_END;
    return ACI_FAILURE;
}

ACI_RC cmpWriteRPLobFinish2Write(cmbBlock *aBlock, cmpProtocol *aProtocol, cmpMarshalState *aMarshalState)
{
    cmpArgRPLobFinish2Write *sArg = CMI_PROTOCOL_GET_ARG(*aProtocol, RP, LobFinish2Write);

    CMP_MARSHAL_BEGIN(5);
    CMP_MARSHAL_WRITE_UINT(sArg->mXLogType);

    CMP_MARSHAL_STAGE(4);
    CMP_MARSHAL_WRITE_UINT(sArg->mTransID);

    CMP_MARSHAL_STAGE(3);
    CMP_MARSHAL_WRITE_ULONG(sArg->mSN);

    CMP_MARSHAL_STAGE(2);
    CMP_MARSHAL_WRITE_ULONG(sArg->mSyncSN);

    CMP_MARSHAL_STAGE(1);
    CMP_MARSHAL_WRITE_ULONG(sArg->mLobLocator);

    CMP_MARSHAL_STAGE(0);

    return ACI_SUCCESS;
    ACI_EXCEPTION_END;
    return ACI_FAILURE;
}

ACI_RC cmpReadRPTxAck(cmbBlock *aBlock, cmpProtocol *aProtocol, cmpMarshalState *aMarshalState)
{
    cmpArgRPTxAck *sArg = CMI_PROTOCOL_GET_ARG(*aProtocol, RP, TxAck);

    CMP_MARSHAL_BEGIN(2);
    CMP_MARSHAL_READ_UINT(sArg->mTID);

    CMP_MARSHAL_STAGE(1);
    CMP_MARSHAL_READ_ULONG(sArg->mSN);

    CMP_MARSHAL_STAGE(0);

    return ACI_SUCCESS;
    ACI_EXCEPTION_END;
    return ACI_FAILURE;
}

ACI_RC cmpWriteRPTxAck(cmbBlock *aBlock, cmpProtocol *aProtocol, cmpMarshalState *aMarshalState)
{
    cmpArgRPTxAck *sArg = CMI_PROTOCOL_GET_ARG(*aProtocol, RP, TxAck);

    CMP_MARSHAL_BEGIN(2);
    CMP_MARSHAL_WRITE_UINT(sArg->mTID);

    CMP_MARSHAL_STAGE(1);
    CMP_MARSHAL_WRITE_ULONG(sArg->mSN);

    CMP_MARSHAL_STAGE(0);

    return ACI_SUCCESS;
    ACI_EXCEPTION_END;
    return ACI_FAILURE;
}

ACI_RC cmpReadRPRequestAck(cmbBlock *aBlock, cmpProtocol *aProtocol, cmpMarshalState *aMarshalState)
{
    cmpArgRPRequestAck *sArg = CMI_PROTOCOL_GET_ARG(*aProtocol, RP, RequestAck);

    CMP_MARSHAL_BEGIN(1);
    CMP_MARSHAL_READ_UINT(sArg->mResult);

    CMP_MARSHAL_STAGE(0);

    return ACI_SUCCESS;
    ACI_EXCEPTION_END;
    return ACI_FAILURE;
}

ACI_RC cmpWriteRPRequestAck(cmbBlock *aBlock, cmpProtocol *aProtocol, cmpMarshalState *aMarshalState)
{
    cmpArgRPRequestAck *sArg = CMI_PROTOCOL_GET_ARG(*aProtocol, RP, RequestAck);

    CMP_MARSHAL_BEGIN(1);
    CMP_MARSHAL_WRITE_UINT(sArg->mResult);

    CMP_MARSHAL_STAGE(0);

    return ACI_SUCCESS;
    ACI_EXCEPTION_END;
    return ACI_FAILURE;
}

ACI_RC cmpReadRPHandshake(cmbBlock *aBlock, cmpProtocol *aProtocol, cmpMarshalState *aMarshalState)
{
    cmpArgRPHandshake *sArg = CMI_PROTOCOL_GET_ARG(*aProtocol, RP, Handshake);

    CMP_MARSHAL_BEGIN(4);
    CMP_MARSHAL_READ_UINT(sArg->mXLogType);

    CMP_MARSHAL_STAGE(3);
    CMP_MARSHAL_READ_UINT(sArg->mTransID);

    CMP_MARSHAL_STAGE(2);
    CMP_MARSHAL_READ_ULONG(sArg->mSN);

    CMP_MARSHAL_STAGE(1);
    CMP_MARSHAL_READ_ULONG(sArg->mSyncSN);

    CMP_MARSHAL_STAGE(0);

    return ACI_SUCCESS;
    ACI_EXCEPTION_END;
    return ACI_FAILURE;
}

ACI_RC cmpWriteRPHandshake(cmbBlock *aBlock, cmpProtocol *aProtocol, cmpMarshalState *aMarshalState)
{
    cmpArgRPHandshake *sArg = CMI_PROTOCOL_GET_ARG(*aProtocol, RP, Handshake);

    CMP_MARSHAL_BEGIN(4);
    CMP_MARSHAL_WRITE_UINT(sArg->mXLogType);

    CMP_MARSHAL_STAGE(3);
    CMP_MARSHAL_WRITE_UINT(sArg->mTransID);

    CMP_MARSHAL_STAGE(2);
    CMP_MARSHAL_WRITE_ULONG(sArg->mSN);

    CMP_MARSHAL_STAGE(1);
    CMP_MARSHAL_WRITE_ULONG(sArg->mSyncSN);

    CMP_MARSHAL_STAGE(0);

    return ACI_SUCCESS;
    ACI_EXCEPTION_END;
    return ACI_FAILURE;
}

ACI_RC cmpReadRPSyncPKBegin(cmbBlock *aBlock, cmpProtocol *aProtocol, cmpMarshalState *aMarshalState)
{
    cmpArgRPSyncPKBegin *sArg = CMI_PROTOCOL_GET_ARG(*aProtocol, RP, SyncPKBegin);

    CMP_MARSHAL_BEGIN(4);
    CMP_MARSHAL_READ_UINT(sArg->mXLogType);

    CMP_MARSHAL_STAGE(3);
    CMP_MARSHAL_READ_UINT(sArg->mTransID);

    CMP_MARSHAL_STAGE(2);
    CMP_MARSHAL_READ_ULONG(sArg->mSN);

    CMP_MARSHAL_STAGE(1);
    CMP_MARSHAL_READ_ULONG(sArg->mSyncSN);

    CMP_MARSHAL_STAGE(0);

    return ACI_SUCCESS;
    ACI_EXCEPTION_END;
    return ACI_FAILURE;
}

ACI_RC cmpWriteRPSyncPKBegin(cmbBlock *aBlock, cmpProtocol *aProtocol, cmpMarshalState *aMarshalState)
{
    cmpArgRPSyncPKBegin *sArg = CMI_PROTOCOL_GET_ARG(*aProtocol, RP, SyncPKBegin);

    CMP_MARSHAL_BEGIN(4);
    CMP_MARSHAL_WRITE_UINT(sArg->mXLogType);

    CMP_MARSHAL_STAGE(3);
    CMP_MARSHAL_WRITE_UINT(sArg->mTransID);

    CMP_MARSHAL_STAGE(2);
    CMP_MARSHAL_WRITE_ULONG(sArg->mSN);

    CMP_MARSHAL_STAGE(1);
    CMP_MARSHAL_WRITE_ULONG(sArg->mSyncSN);

    CMP_MARSHAL_STAGE(0);

    return ACI_SUCCESS;
    ACI_EXCEPTION_END;
    return ACI_FAILURE;
}

ACI_RC cmpReadRPSyncPK(cmbBlock *aBlock, cmpProtocol *aProtocol, cmpMarshalState *aMarshalState)
{
    cmpArgRPSyncPK *sArg = CMI_PROTOCOL_GET_ARG(*aProtocol, RP, SyncPK);

    CMP_MARSHAL_BEGIN(6);
    CMP_MARSHAL_READ_UINT(sArg->mXLogType);

    CMP_MARSHAL_STAGE(5);
    CMP_MARSHAL_READ_UINT(sArg->mTransID);

    CMP_MARSHAL_STAGE(4);
    CMP_MARSHAL_READ_ULONG(sArg->mSN);

    CMP_MARSHAL_STAGE(3);
    CMP_MARSHAL_READ_ULONG(sArg->mSyncSN);

    CMP_MARSHAL_STAGE(2);
    CMP_MARSHAL_READ_ULONG(sArg->mTableOID);

    CMP_MARSHAL_STAGE(1);
    CMP_MARSHAL_READ_UINT(sArg->mPKColCnt);

    CMP_MARSHAL_STAGE(0);

    return ACI_SUCCESS;
    ACI_EXCEPTION_END;
    return ACI_FAILURE;
}

ACI_RC cmpWriteRPSyncPK(cmbBlock *aBlock, cmpProtocol *aProtocol, cmpMarshalState *aMarshalState)
{
    cmpArgRPSyncPK *sArg = CMI_PROTOCOL_GET_ARG(*aProtocol, RP, SyncPK);

    CMP_MARSHAL_BEGIN(6);
    CMP_MARSHAL_WRITE_UINT(sArg->mXLogType);

    CMP_MARSHAL_STAGE(5);
    CMP_MARSHAL_WRITE_UINT(sArg->mTransID);

    CMP_MARSHAL_STAGE(4);
    CMP_MARSHAL_WRITE_ULONG(sArg->mSN);

    CMP_MARSHAL_STAGE(3);
    CMP_MARSHAL_WRITE_ULONG(sArg->mSyncSN);

    CMP_MARSHAL_STAGE(2);
    CMP_MARSHAL_WRITE_ULONG(sArg->mTableOID);

    CMP_MARSHAL_STAGE(1);
    CMP_MARSHAL_WRITE_UINT(sArg->mPKColCnt);

    CMP_MARSHAL_STAGE(0);

    return ACI_SUCCESS;
    ACI_EXCEPTION_END;
    return ACI_FAILURE;
}

ACI_RC cmpReadRPSyncPKEnd(cmbBlock *aBlock, cmpProtocol *aProtocol, cmpMarshalState *aMarshalState)
{
    cmpArgRPSyncPKEnd *sArg = CMI_PROTOCOL_GET_ARG(*aProtocol, RP, SyncPKEnd);

    CMP_MARSHAL_BEGIN(4);
    CMP_MARSHAL_READ_UINT(sArg->mXLogType);

    CMP_MARSHAL_STAGE(3);
    CMP_MARSHAL_READ_UINT(sArg->mTransID);

    CMP_MARSHAL_STAGE(2);
    CMP_MARSHAL_READ_ULONG(sArg->mSN);

    CMP_MARSHAL_STAGE(1);
    CMP_MARSHAL_READ_ULONG(sArg->mSyncSN);

    CMP_MARSHAL_STAGE(0);

    return ACI_SUCCESS;
    ACI_EXCEPTION_END;
    return ACI_FAILURE;
}

ACI_RC cmpWriteRPSyncPKEnd(cmbBlock *aBlock, cmpProtocol *aProtocol, cmpMarshalState *aMarshalState)
{
    cmpArgRPSyncPKEnd *sArg = CMI_PROTOCOL_GET_ARG(*aProtocol, RP, SyncPKEnd);

    CMP_MARSHAL_BEGIN(4);
    CMP_MARSHAL_WRITE_UINT(sArg->mXLogType);

    CMP_MARSHAL_STAGE(3);
    CMP_MARSHAL_WRITE_UINT(sArg->mTransID);

    CMP_MARSHAL_STAGE(2);
    CMP_MARSHAL_WRITE_ULONG(sArg->mSN);

    CMP_MARSHAL_STAGE(1);
    CMP_MARSHAL_WRITE_ULONG(sArg->mSyncSN);

    CMP_MARSHAL_STAGE(0);

    return ACI_SUCCESS;
    ACI_EXCEPTION_END;
    return ACI_FAILURE;
}

ACI_RC cmpReadRPFailbackEnd(cmbBlock *aBlock, cmpProtocol *aProtocol, cmpMarshalState *aMarshalState)
{
    cmpArgRPFailbackEnd *sArg = CMI_PROTOCOL_GET_ARG(*aProtocol, RP, FailbackEnd);

    CMP_MARSHAL_BEGIN(4);
    CMP_MARSHAL_READ_UINT(sArg->mXLogType);

    CMP_MARSHAL_STAGE(3);
    CMP_MARSHAL_READ_UINT(sArg->mTransID);

    CMP_MARSHAL_STAGE(2);
    CMP_MARSHAL_READ_ULONG(sArg->mSN);

    CMP_MARSHAL_STAGE(1);
    CMP_MARSHAL_READ_ULONG(sArg->mSyncSN);

    CMP_MARSHAL_STAGE(0);

    return ACI_SUCCESS;
    ACI_EXCEPTION_END;
    return ACI_FAILURE;
}

ACI_RC cmpWriteRPFailbackEnd(cmbBlock *aBlock, cmpProtocol *aProtocol, cmpMarshalState *aMarshalState)
{
    cmpArgRPFailbackEnd *sArg = CMI_PROTOCOL_GET_ARG(*aProtocol, RP, FailbackEnd);

    CMP_MARSHAL_BEGIN(4);
    CMP_MARSHAL_WRITE_UINT(sArg->mXLogType);

    CMP_MARSHAL_STAGE(3);
    CMP_MARSHAL_WRITE_UINT(sArg->mTransID);

    CMP_MARSHAL_STAGE(2);
    CMP_MARSHAL_WRITE_ULONG(sArg->mSN);

    CMP_MARSHAL_STAGE(1);
    CMP_MARSHAL_WRITE_ULONG(sArg->mSyncSN);

    CMP_MARSHAL_STAGE(0);

    return ACI_SUCCESS;
    ACI_EXCEPTION_END;
    return ACI_FAILURE;
}

ACI_RC cmpReadRPSyncTableNumber( cmbBlock * aBlock, cmpProtocol * aProtocol, cmpMarshalState * aMarshalState )
{
    cmpArgRPSyncTableNumber * sArg = CMI_PROTOCOL_GET_ARG( *aProtocol, RP, SyncTableNumber );

    CMP_MARSHAL_BEGIN( 1 );
    CMP_MARSHAL_READ_UINT( sArg->mSyncTableNumber );

    CMP_MARSHAL_STAGE( 0 );

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

ACI_RC cmpWriteRPSyncTableNumber( cmbBlock * aBlock, cmpProtocol * aProtocol, cmpMarshalState * aMarshalState )
{
    cmpArgRPSyncTableNumber * sArg = CMI_PROTOCOL_GET_ARG( *aProtocol, RP, SyncTableNumber );

    CMP_MARSHAL_BEGIN( 1 );
    CMP_MARSHAL_WRITE_UINT( sArg->mSyncTableNumber );

    CMP_MARSHAL_STAGE( 0 );

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

ACI_RC cmpReadRPSyncStart(cmbBlock *aBlock, cmpProtocol *aProtocol, cmpMarshalState *aMarshalState)
{
    cmpArgRPSyncStart *sArg = CMI_PROTOCOL_GET_ARG( *aProtocol, RP, SyncStart );

    CMP_MARSHAL_BEGIN( 1 );
    CMP_MARSHAL_READ_UINT( sArg->mXLogType );

    CMP_MARSHAL_STAGE( 0 );

    return ACI_SUCCESS;
        ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

ACI_RC cmpWriteRPSyncStart(cmbBlock *aBlock, cmpProtocol *aProtocol, cmpMarshalState *aMarshalState)
{
    cmpArgRPSyncStart *sArg = CMI_PROTOCOL_GET_ARG( *aProtocol, RP, SyncStart );

    CMP_MARSHAL_BEGIN( 1 );
    CMP_MARSHAL_WRITE_UINT( sArg->mXLogType );

    CMP_MARSHAL_STAGE( 0 );

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

ACI_RC cmpWriteRPSyncRebuildIndex(cmbBlock *aBlock, cmpProtocol *aProtocol, cmpMarshalState *aMarshalState)
{
    cmpArgRPSyncRebuildIndex *sArg = CMI_PROTOCOL_GET_ARG( *aProtocol, RP, SyncRebuildIndex );

    CMP_MARSHAL_BEGIN( 1 );
    CMP_MARSHAL_WRITE_UINT( sArg->mXLogType );

    CMP_MARSHAL_STAGE( 0 );

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

ACI_RC cmpReadRPSyncRebuildIndex(cmbBlock *aBlock, cmpProtocol *aProtocol, cmpMarshalState *aMarshalState)
{
    cmpArgRPSyncRebuildIndex *sArg = CMI_PROTOCOL_GET_ARG( *aProtocol, RP, SyncRebuildIndex );

    CMP_MARSHAL_BEGIN( 1 );
    CMP_MARSHAL_READ_UINT( sArg->mXLogType );

    CMP_MARSHAL_STAGE( 0 );

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

/********************************************************************
 * Description  :
 *
 * ********************************************************************/
ACI_RC cmpWriteRPDummy( cmbBlock         *aBlock,
                        cmpProtocol      *aProtocol,
                        cmpMarshalState  *aMarshalState )
{
    ACP_UNUSED( aBlock );
    ACP_UNUSED( aProtocol );
    ACP_UNUSED( aMarshalState );

    return ACI_FAILURE;
}

/********************************************************************
 * Description  :
 *
 * ********************************************************************/
ACI_RC cmpReadRPDummy( cmbBlock         *aBlock,
                       cmpProtocol      *aProtocol,
                       cmpMarshalState  *aMarshalState )
{
    ACP_UNUSED( aBlock );
    ACP_UNUSED( aProtocol );
    ACP_UNUSED( aMarshalState );

    return ACI_FAILURE;
}

cmpMarshalFunction gCmpReadFunctionRPClient[CMP_OP_RP_MAX] =
{
    cmpReadRPVersion,
    cmpReadRPMetaRepl,
    cmpReadRPMetaReplTbl,
    cmpReadRPMetaReplCol,
    cmpReadRPMetaReplIdx,
    cmpReadRPMetaReplIdxCol,
    cmpReadRPHandshakeAck,
    cmpReadRPTrBegin,
    cmpReadRPTrCommit,
    cmpReadRPTrAbort,
    cmpReadRPSPSet,
    cmpReadRPSPAbort,
    cmpReadRPStmtBegin,
    cmpReadRPStmtEnd,
    cmpReadRPCursorOpen,
    cmpReadRPCursorClose,
    cmpReadRPInsert,
    cmpReadRPUpdate,
    cmpReadRPDelete,
    cmpReadRPUIntID,
    cmpReadRPValue,
    cmpReadRPStop,
    cmpReadRPKeepAlive,
    cmpReadRPFlush,
    cmpReadRPFlushAck,
    cmpReadRPAck,
    cmpReadRPLobCursorOpen,
    cmpReadRPLobCursorClose,
    cmpReadRPLobPrepare4Write,
    cmpReadRPLobPartialWrite,
    cmpReadRPLobFinish2Write,
    cmpReadRPTxAck,
    cmpReadRPRequestAck,
    cmpReadRPHandshake,
    cmpReadRPSyncPKBegin,
    cmpReadRPSyncPK,
    cmpReadRPSyncPKEnd,
    cmpReadRPFailbackEnd,
    cmpReadRPSyncTableNumber,
    cmpReadRPSyncStart,
    cmpReadRPSyncRebuildIndex,
    cmpReadRPLobTrim,
    cmpReadRPDummy,
    cmpReadRPDummy,/* BUG-38759 */
    cmpReadRPDummy,
    cmpReadRPDummy,
    cmpReadRPDummy, /* DDLSyncInfo */
    cmpReadRPDummy,
    cmpReadRPDummy,
    cmpReadRPDummy, /* DDLSyncCancel */
    cmpReadRPDummy, /* BUG-46252 Partition Merge / Split / Replace DDL asynchronization support */
    cmpReadRPDummy,
    cmpReadRPDummy,
    cmpReadRPDummy,
    cmpReadRPDummy,  //BUG-46120
    cmpReadRPDummy
};

cmpMarshalFunction gCmpWriteFunctionRPClient[CMP_OP_RP_MAX] =
{
    cmpWriteRPVersion,
    cmpWriteRPMetaRepl,
    cmpWriteRPMetaReplTbl,
    cmpWriteRPMetaReplCol,
    cmpWriteRPMetaReplIdx,
    cmpWriteRPMetaReplIdxCol,
    cmpWriteRPHandshakeAck,
    cmpWriteRPTrBegin,
    cmpWriteRPTrCommit,
    cmpWriteRPTrAbort,
    cmpWriteRPSPSet,
    cmpWriteRPSPAbort,
    cmpWriteRPStmtBegin,
    cmpWriteRPStmtEnd,
    cmpWriteRPCursorOpen,
    cmpWriteRPCursorClose,
    cmpWriteRPInsert,
    cmpWriteRPUpdate,
    cmpWriteRPDelete,
    cmpWriteRPUIntID,
    cmpWriteRPValue,
    cmpWriteRPStop,
    cmpWriteRPKeepAlive,
    cmpWriteRPFlush,
    cmpWriteRPFlushAck,
    cmpWriteRPAck,
    cmpWriteRPLobCursorOpen,
    cmpWriteRPLobCursorClose,
    cmpWriteRPLobPrepare4Write,
    cmpWriteRPLobPartialWrite,
    cmpWriteRPLobFinish2Write,
    cmpWriteRPTxAck,
    cmpWriteRPRequestAck,
    cmpWriteRPHandshake,
    cmpWriteRPSyncPKBegin,
    cmpWriteRPSyncPK,
    cmpWriteRPSyncPKEnd,
    cmpWriteRPFailbackEnd,
    cmpWriteRPSyncTableNumber,
    cmpWriteRPSyncStart,
    cmpWriteRPSyncRebuildIndex,
    cmpWriteRPLobTrim,
    cmpWriteRPDummy,
    cmpWriteRPDummy,/* BUG-38759 */
    cmpWriteRPDummy,
    cmpWriteRPDummy,
    cmpWriteRPDummy, /* DDLSyncInfo */
    cmpWriteRPDummy,
    cmpWriteRPDummy,
    cmpWriteRPDummy, /* DDLSyncCancel */
    cmpWriteRPDummy, /* BUG-46252 Partition Merge / Split / Replace DDL asynchronization support */    
    cmpWriteRPDummy,
    cmpWriteRPDummy,
    cmpWriteRPDummy,
    cmpWriteRPDummy,  //BUG-46120
    cmpWriteRPDummy
};
