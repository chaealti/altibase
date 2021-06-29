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

#include <uln.h>
#include <ulnPrivate.h>
#include <ulnSetStmtAttr.h>

static acp_bool_t ulnSetStmtAttrCheck2(acp_sint32_t sAttribute)
{
    if (sAttribute == SQL_ATTR_CONCURRENCY ||
        sAttribute == SQL_ATTR_CURSOR_TYPE ||
        sAttribute == SQL_ATTR_SIMULATE_CURSOR ||
        sAttribute == SQL_ATTR_USE_BOOKMARKS ||
        sAttribute == SQL_ATTR_CURSOR_SCROLLABLE ||
        sAttribute == SQL_ATTR_CURSOR_SENSITIVITY)
    {
        /* [2] */
        return ACP_TRUE;
    }
    else
    {
        /* [1] */
        return ACP_FALSE;
    }
}

/*
 * ULN_SFID_50
 * SQLSetStmtAttr(), STMT, S2-S3, S4, S5-S7
 *      -- [1]
 *      (HY011) [2]
 *  where
 *      [1] The Attribute argument was not SQL_ATTR_CONCURRENCY, SQL_ATTR_CURSOR_TYPE,
 *          SQL_ATTR_SIMULATE_CURSOR, SQL_ATTR_USE_BOOKMARKS, SQL_ATTR_CURSOR_SCROLLABLE,
 *          or SQL_ATTR_CURSOR_SENSITIVITY.
 *
 *      [2] The Attribute argument was SQL_ATTR_CONCURRENCY, SQL_ATTR_CURSOR_TYPE,
 *          SQL_ATTR_SIMULATE_CURSOR, SQL_ATTR_USE_BOOKMARKS, SQL_ATTR_CURSOR_SCROLLABLE,
 *          or SQL_ATTR_CURSOR_SENSITIVITY.
 */
ACI_RC ulnSFID_50(ulnFnContext *aContext)
{
    acp_sint32_t sAttribute;

    if (aContext->mWhere == ULN_STATE_ENTRY_POINT)
    {
        sAttribute = *(acp_sint32_t *)(aContext->mArgs);

        if (ulnSetStmtAttrCheck2(sAttribute) == ACP_TRUE)
        {
            /* [2] */
            switch (aContext->mUlErrorCode)
            {
                    /*
                     * 24000
                     */
                case ulERR_ABORT_INVALID_CURSOR_STATE:
                    ACI_RAISE(LABEL_INVALID_CURSOR_STATE);
                    break;

                    /*
                     * HY011
                     */
                case ulERR_ABORT_ATTRIBUTE_CANNOT_BE_SET_NOW:
                    ACI_RAISE(LABEL_ABORT_SET);
                    break;

                default:
                    ACI_RAISE(LABEL_MEM_MAN_ERR);
                    break;
            }
        }
    }

    return ACI_SUCCESS;

    ACI_EXCEPTION(LABEL_INVALID_CURSOR_STATE)
    {
        ulnError(aContext, ulERR_ABORT_INVALID_CURSOR_STATE);
    }
    ACI_EXCEPTION(LABEL_ABORT_SET)
    {
        ulnError(aContext, ulERR_ABORT_ATTRIBUTE_CANNOT_BE_SET_NOW);
    }
    ACI_EXCEPTION(LABEL_MEM_MAN_ERR)
    {
        ulnError(aContext, ulERR_FATAL_MEMORY_MANAGEMENT_ERROR, "SFID_50");
    }
    ACI_EXCEPTION_END;
    return ACI_FAILURE;
}

/*
 * ULN_SFID_51
 * SQLSetStmtAttr(), STMT, S8-S10, S11-S12
 *      (HY010) [np] or [1]
 *      (HY011) [p] and [2]
 *  where
 *      [1] The Attribute argument was not SQL_ATTR_CONCURRENCY, SQL_ATTR_CURSOR_TYPE,
 *          SQL_ATTR_SIMULATE_CURSOR, SQL_ATTR_USE_BOOKMARKS, SQL_ATTR_CURSOR_SCROLLABLE,
 *          or SQL_ATTR_CURSOR_SENSITIVITY.
 *
 *      [2] The Attribute argument was SQL_ATTR_CONCURRENCY, SQL_ATTR_CURSOR_TYPE,
 *          SQL_ATTR_SIMULATE_CURSOR, SQL_ATTR_USE_BOOKMARKS, SQL_ATTR_CURSOR_SCROLLABLE,
 *          or SQL_ATTR_CURSOR_SENSITIVITY.
 */
ACI_RC ulnSFID_51(ulnFnContext *aContext)
{
    acp_sint32_t sAttribute;

    if (aContext->mWhere == ULN_STATE_ENTRY_POINT)
    {
        sAttribute = *(acp_sint32_t *)(aContext->mArgs);

        if (ulnStmtIsPrepared(aContext->mHandle.mStmt) == ACP_TRUE /* [p] */ &&
            ulnSetStmtAttrCheck2(sAttribute) == ACP_TRUE /* [2] */)
        {
            /* [p] and [2] : HY011 */
            ACI_RAISE(LABEL_ABORT_SET);
        }
        else
        {
            /* [np] or [1] : HY010 */
            ACI_RAISE(LABEL_ABORT_FUNC_SEQ_ERR);
        }
    }

    return ACI_SUCCESS;

    ACI_EXCEPTION(LABEL_ABORT_SET)
    {
        ulnError(aContext, ulERR_ABORT_ATTRIBUTE_CANNOT_BE_SET_NOW);
    }
    ACI_EXCEPTION(LABEL_ABORT_FUNC_SEQ_ERR)
    {
        ulnError(aContext, ulERR_ABORT_FUNCTION_SEQUENCE_ERR);
    }
    ACI_EXCEPTION_END;
    return ACI_FAILURE;
}

ACI_RC ulnStmtAttrCheckUnsupportedAttr(ulnFnContext *aContext, acp_sint32_t aAttr)
{
    switch (aAttr)
    {
            /*
             * SQLGetInfo() �� SQL_DYNAMIC_CURSOR_ATTRIBUTES2 ����
             * SQL_ATTR_MAX_ROWS �� ���õ� �Ӽ��� �� �� �ִ�. �׷��� ��Ƽ���̽�������
             * ����� �������� �ʴ´�.
             */
        case SQL_ATTR_MAX_ROWS:

            /*
             * Note:
             * SQL_ATTR_MAX_LENGTH : ������ ���� ������, �� �����ϴ�. ������ ����.
             */
        case SQL_ATTR_MAX_LENGTH:

        case SQL_ATTR_NOSCAN:
        case SQL_ATTR_METADATA_ID:
        case SQL_ATTR_KEYSET_SIZE:
        case SQL_ATTR_ENABLE_AUTO_IPD:
        case SQL_ATTR_ASYNC_ENABLE:

            /*
             * Note:
             * SQL_ATTR_SIMULATE_CURSOR : ������ positioned update/insert �� �������� ������,
             * ������ �����ϴ� ����� �߰��� ������ �Ʒ��� �ΰ��� üũ��ƾ�� �ݵ�� �־�� �Ѵ�
             * ACI_TEST(ulnSetStmtAttrCheck24000(aContext) != ACI_SUCCESS);
             * ACI_TEST(ulnSetStmtAttrCheckHY011(aContext) != ACI_SUCCESS);
             */
        case SQL_ATTR_SIMULATE_CURSOR:

            /*
             * HYC00
             */
            ACI_RAISE(LABEL_NOT_IMPLEMENTED);
            break;

        default:
        break;
    }

    return ACI_SUCCESS;

    ACI_EXCEPTION(LABEL_NOT_IMPLEMENTED)
    {
        ulnError(aContext, ulERR_ABORT_OPTIONAL_FEATURE_NOT_IMPLEMENTED);
    }
    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

ACI_RC ulnSetStmtAttrCheckHY017(ulnFnContext *aContext, void *aValuePtr)
{
    //BUG-28623 [CodeSonar]Null Pointer Dereference
    ACI_TEST_RAISE( aValuePtr == NULL, LABEL_INVALID_POINTER );

    /*
     * HY017 : Invalid use of an automatically allocated descriptor handle
     * (DM) The Attribute argument was SQL_ATTR_IMP_ROW_DESC or SQL_ATTR_IMP_PARAM_DESC.
     * (DM) The Attribute argument was SQL_ATTR_APP_ROW_DESC or SQL_ATTR_APP_PARAM_DESC,
     * and the value in ValuePtr was an implicitly allocated descriptor handle other
     * than the handle originally allocated for the ARD or APD.
     */
    ACI_TEST_RAISE( (ULN_OBJ_GET_DESC_TYPE(aValuePtr) == ULN_DESC_TYPE_IRD) ||
                    (ULN_OBJ_GET_DESC_TYPE(aValuePtr) == ULN_DESC_TYPE_IPD), LABEL_INVALID_DESCRIPTOR);

    return ACI_SUCCESS;

    ACI_EXCEPTION(LABEL_INVALID_POINTER);
    {
        ulnError(aContext, ulERR_ABORT_INVALID_USE_OF_NULL_POINTER);
    }
    ACI_EXCEPTION(LABEL_INVALID_DESCRIPTOR);
    {
        ulnError(aContext, ulERR_ABORT_INVALID_USE_OF_IMP_DESC);
    }
    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

ACI_RC ulnSetStmtAttrCheck24000(ulnFnContext *aContext)
{
    /*
     * 24000 Invalid cursor state
     * The Attribute was SQL_ATTR_CONCURRENCY, SQL_ATTR_CURSOR_TYPE, SQL_ATTR_SIMULATE_CURSOR,
     * or SQL_ATTR_USE_BOOKMARKS, and the cursor was open.
     */
    ACI_TEST_RAISE(ulnStmtIsCursorOpen(aContext->mHandle.mStmt) == ACP_TRUE, LABEL_INVALID_CURSOR_STATE);

    return ACI_SUCCESS;

    ACI_EXCEPTION(LABEL_INVALID_CURSOR_STATE)
    {
        ulnError(aContext, ulERR_ABORT_INVALID_CURSOR_STATE);
    }
    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

ACI_RC ulnSetStmtAttrCheckHY011(ulnFnContext *aContext)
{
    /*
     * HY011 Attribute cannot be set now
     * The Attribute was SQL_ATTR_CONCURRENCY, SQL_ATTR_CURSOR_TYPE, SQL_ATTR_SIMULATE_CURSOR,
     * or SQL_ ATTR_USE_BOOKMARKS, and the statement was prepared.
     */
    ACI_TEST_RAISE(aContext->mHandle.mStmt->mIsPrepared == ACP_TRUE, LABEL_ABORT_SET);
    return ACI_SUCCESS;

    ACI_EXCEPTION(LABEL_ABORT_SET)
    {
        ulnError(aContext, ulERR_ABORT_ATTRIBUTE_CANNOT_BE_SET_NOW);
    }
    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

ACI_RC ulnSetStmtAttrDoConcurrency(ulnFnContext *aFnContext, acp_uint32_t aValue)
{
    ulnStmt *sStmt = aFnContext->mHandle.mStmt;

    switch (aValue)
    {
        case SQL_CONCUR_READ_ONLY:
            ulnStmtSetAttrConcurrency(sStmt, SQL_CONCUR_READ_ONLY);

            if (ulnStmtGetAttrCursorSensitivity(sStmt) != SQL_INSENSITIVE)
            {
                ulnStmtSetAttrCursorSensitivity(sStmt, SQL_INSENSITIVE);

                ulnError(aFnContext, ulERR_IGNORE_OPTION_VALUE_CHANGED,
                         "SQL_ATTR_CURSOR_SENSITIVITY automatically changed"
                         " to SQL_INSENSITIVE as a result of setting"
                         " SQL_ATTR_CONCURRENCY to SQL_CONCUR_READ_ONLY");
            }
            break;

        case SQL_CONCUR_LOCK:
        case SQL_CONCUR_VALUES:
            ulnError(aFnContext, ulERR_IGNORE_OPTION_VALUE_CHANGED,
                     "SQL_ATTR_CONCURRENCY changed to SQL_CONCUR_ROWVER");

            /* continued .. */

        case SQL_CONCUR_ROWVER:
            ulnStmtSetAttrConcurrency(sStmt, SQL_CONCUR_ROWVER);

            if (ulnStmtGetAttrCursorSensitivity(sStmt) != SQL_SENSITIVE)
            {
                ulnStmtSetAttrCursorSensitivity(sStmt, SQL_SENSITIVE);

                ulnError(aFnContext, ulERR_IGNORE_OPTION_VALUE_CHANGED,
                         "SQL_ATTR_CURSOR_SENSITIVITY automatically changed"
                         " to SQL_SENSITIVE as a result of setting"
                         " SQL_ATTR_CONCURRENCY to SQL_CONCUR_ROWVER");
            }
            break;

        default:
            /*
             * HY024 : Invalid Attribute Value
             */
            ACI_RAISE(LABEL_INVALID_ATTR_VALUE);
            break;
    }

    return ACI_SUCCESS;

    ACI_EXCEPTION(LABEL_INVALID_ATTR_VALUE)
    {
        ulnError(aFnContext, ulERR_ABORT_INVALID_ATTRIBUTE_VALUE);
    }

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

ACI_RC ulnSetStmtAttrDoScrollable(ulnFnContext *aFnContext, acp_uint32_t aScrollable)
{
    ulnStmt *sStmt = aFnContext->mHandle.mStmt;

    switch (aScrollable)
    {
        case SQL_NONSCROLLABLE:
            ulnStmtSetAttrCursorScrollable(sStmt, SQL_NONSCROLLABLE);

            if (ulnStmtGetAttrCursorType(sStmt) != SQL_CURSOR_FORWARD_ONLY)
            {
                ulnStmtSetAttrCursorType(sStmt, SQL_CURSOR_FORWARD_ONLY);

                ulnError(aFnContext, ulERR_IGNORE_OPTION_VALUE_CHANGED,
                         "SQL_ATTR_CURSOR_TYPE automatically changed"
                         " to SQL_CURSOR_FORWARD_ONLY as a result of setting"
                         " SQL_ATTR_SCROLLABLE to SQL_NONSCROLLABLE");
            }
            break;

        case SQL_SCROLLABLE:
            ulnStmtSetAttrCursorScrollable(sStmt, SQL_SCROLLABLE);

            if (ulnStmtGetAttrCursorType(sStmt) == SQL_CURSOR_FORWARD_ONLY)
            {
                if (ulnStmtGetAttrConcurrency(sStmt) == SQL_CONCUR_READ_ONLY)
                {
                    ulnStmtSetAttrCursorType(sStmt, SQL_CURSOR_STATIC);

                    ulnError(aFnContext, ulERR_IGNORE_OPTION_VALUE_CHANGED,
                             "SQL_ATTR_CURSOR_TYPE automatically changed"
                             " to SQL_CURSOR_STATIC as a result of setting"
                             " SQL_ATTR_SCROLLABLE to SQL_SCROLLABLE");
                }
                else /* if SQL_CONCUR_ROWVER */
                {
                    ulnStmtSetAttrCursorType(sStmt, SQL_CURSOR_KEYSET_DRIVEN);

                    ulnError(aFnContext, ulERR_IGNORE_OPTION_VALUE_CHANGED,
                             "SQL_ATTR_CURSOR_TYPE automatically changed"
                             " to SQL_CURSOR_KEYSET_DRIVEN"
                             " as a result of setting"
                             " SQL_ATTR_SCROLLABLE to SQL_SCROLLABLE");
                }
            }
            break;

        default:
            /*
             * HY024 : Invalid Attribute Value
             */
            ACI_RAISE(LABEL_INVALID_ATTR_VALUE);
    }

    return ACI_SUCCESS;

    ACI_EXCEPTION(LABEL_INVALID_ATTR_VALUE)
    {
        ulnError(aFnContext, ulERR_ABORT_INVALID_ATTRIBUTE_VALUE);
    }
    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

ACI_RC ulnSetStmtAttrDoSensitivity(ulnFnContext *aFnContext, acp_uint32_t aValue)
{
    ulnStmt *sStmt = aFnContext->mHandle.mStmt;

    switch (aValue)
    {
        case SQL_INSENSITIVE:
            ulnStmtSetAttrCursorSensitivity(sStmt, SQL_INSENSITIVE);

            /* Insensitive cursors are never updatable  */
            if (ulnStmtGetAttrConcurrency(sStmt) != SQL_CONCUR_READ_ONLY)
            {
                ulnStmtSetAttrConcurrency(sStmt, SQL_CONCUR_READ_ONLY);

                ulnError(aFnContext, ulERR_IGNORE_OPTION_VALUE_CHANGED,
                         "SQL_ATTR_CONCURRENCY automatically changed"
                         " to SQL_CONCUR_READ_ONLY as a result of setting"
                         " SQL_ATTR_CURSOR_SENSITIVITY to SQL_INSENSITIVE");
            }

            if (ulnStmtGetAttrCursorScrollable(sStmt) == SQL_SCROLLABLE)
            {
                /* Keyset-driven + Insensitive is not supported */
                if (ulnStmtGetAttrCursorType(sStmt) != SQL_CURSOR_STATIC)
                {
                    ulnStmtSetAttrCursorType(sStmt, SQL_CURSOR_STATIC);

                    ulnError(aFnContext, ulERR_IGNORE_OPTION_VALUE_CHANGED,
                             "SQL_ATTR_CURSOR_TYPE automatically changed"
                             " to SQL_CURSOR_STATIC as a result of setting"
                             " SQL_ATTR_CURSOR_SENSITIVITY to SQL_INSENSITIVE");
                }
            }
            else /* if SQL_NONSCROLLABLE */
            {
                if (ulnStmtGetAttrCursorType(sStmt) != SQL_CURSOR_FORWARD_ONLY)
                {
                    ulnStmtSetAttrCursorType(sStmt, SQL_CURSOR_FORWARD_ONLY);

                    ulnError(aFnContext, ulERR_IGNORE_OPTION_VALUE_CHANGED,
                             "SQL_ATTR_CURSOR_TYPE automatically changed"
                             " to SQL_CURSOR_FORWARD_ONLY"
                             " as a result of setting"
                             " SQL_ATTR_CURSOR_SENSITIVITY to SQL_INSENSITIVE");
                }
            }

            break;

        case SQL_SENSITIVE:
            ulnStmtSetAttrCursorSensitivity(sStmt, SQL_SENSITIVE);

            if (ulnStmtGetAttrConcurrency(sStmt) != SQL_CONCUR_ROWVER)
            {
                ulnStmtSetAttrConcurrency(sStmt, SQL_CONCUR_ROWVER);

                ulnError(aFnContext, ulERR_IGNORE_OPTION_VALUE_CHANGED,
                         "SQL_ATTR_CONCURRENCY automatically changed"
                         " to SQL_CONCUR_ROWVER as a result of setting"
                         " SQL_ATTR_CURSOR_SENSITIVITY to SQL_SENSITIVE");
            }

            /* SQL_SENSITIVE�� keyset-driven������ �����Ѵ�. */
            if (ulnStmtGetAttrCursorType(sStmt) != SQL_CURSOR_KEYSET_DRIVEN)
            {
                ulnStmtSetAttrCursorType(sStmt, SQL_CURSOR_KEYSET_DRIVEN);

                ulnError(aFnContext, ulERR_IGNORE_OPTION_VALUE_CHANGED,
                         "SQL_ATTR_CURSOR_TYPE automatically changed"
                         " to SQL_CURSOR_KEYSET_DRIVEN as a result of setting"
                         " SQL_ATTR_CURSOR_SENSITIVITY to SQL_SENSITIVE");
            }
            break;

        case SQL_UNSPECIFIED:
            /*
             * SQL_UNSPECIFIED�� �������� �ʴ´�.
             * 01S02 : Option Value Changed
             */
            ACI_RAISE(LABEL_OPT_VALUE_CHANGED_NOT_SUPPORTED);
            break;

        default:
            /*
             * HY024 : Invalid Attribute Value
             */
            ACI_RAISE(LABEL_INVALID_ATTR_VALUE);
            break;
    }

    return ACI_SUCCESS;

    ACI_EXCEPTION(LABEL_OPT_VALUE_CHANGED_NOT_SUPPORTED)
    {
        ulnError(aFnContext, ulERR_IGNORE_OPTION_VALUE_CHANGED,
                 "SQL_UNSPECIFIED is not supported");
    }
    ACI_EXCEPTION(LABEL_INVALID_ATTR_VALUE)
    {
        ulnError(aFnContext, ulERR_ABORT_INVALID_ATTRIBUTE_VALUE);
    }
    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

ACI_RC ulnSetStmtAttrDoCursorType(ulnFnContext *aFnContext, acp_uint32_t aValue)
{
    ulnStmt      *sStmt = aFnContext->mHandle.mStmt;

    switch (aValue)
    {
        case SQL_CURSOR_FORWARD_ONLY:
            ulnStmtSetAttrCursorType(sStmt, SQL_CURSOR_FORWARD_ONLY);

            if (ulnStmtGetAttrCursorScrollable(sStmt) != SQL_NONSCROLLABLE)
            {
                ulnStmtSetAttrCursorScrollable(sStmt, SQL_NONSCROLLABLE);

                ulnError(aFnContext, ulERR_IGNORE_OPTION_VALUE_CHANGED,
                         "SQL_ATTR_CURSOR_SCROLLABLE automatically changed"
                         " to SQL_NONSCROLLABLE as a result of setting"
                         " SQL_ATTR_CURSOR_TYPE to SQL_CURSOR_FORWARD_ONLY");
            }
            break;

        case SQL_CURSOR_STATIC:
            ulnStmtSetAttrCursorType(sStmt, SQL_CURSOR_STATIC);

            if (ulnStmtGetAttrCursorScrollable(sStmt) != SQL_SCROLLABLE)
            {
                ulnStmtSetAttrCursorScrollable(sStmt, SQL_SCROLLABLE);

                ulnError(aFnContext, ulERR_IGNORE_OPTION_VALUE_CHANGED,
                         "SQL_ATTR_CURSOR_SCROLLABLE automatically changed"
                         " to SQL_SCROLLABLE as a result of setting"
                         " SQL_ATTR_CURSOR_TYPE to SQL_CURSOR_STATIC");
            }

            /* Notes. ������ Concurrency�� ���� Sensitivity�� �ٲ�� �Ѵ�.
             * ������, Static + Sensitive�� �������� �����Ƿ� �׻� Insensitive�� �ٲ۴�. */
            if (ulnStmtGetAttrCursorSensitivity(sStmt) != SQL_INSENSITIVE)
            {
                ulnStmtSetAttrCursorSensitivity(sStmt, SQL_INSENSITIVE);

                ulnError(aFnContext, ulERR_IGNORE_OPTION_VALUE_CHANGED,
                         "SQL_ATTR_CURSOR_SENSITIVITY automatically changed"
                         " to SQL_INSENSITIVE as a result of setting"
                         " SQL_ATTR_CURSOR_TYPE to SQL_CURSOR_STATIC");
            }
            break;

        case SQL_CURSOR_DYNAMIC:
            ulnError(aFnContext, ulERR_IGNORE_OPTION_VALUE_CHANGED,
                     "SQL_ATTR_CURSOR_TYPE changed to SQL_CURSOR_KEYSET_DRIVEN");

            /* continued .. */

        case SQL_CURSOR_KEYSET_DRIVEN:
            ulnStmtSetAttrCursorType(sStmt, SQL_CURSOR_KEYSET_DRIVEN);

            if (ulnStmtGetAttrCursorScrollable(sStmt) != SQL_SCROLLABLE)
            {
                ulnStmtSetAttrCursorScrollable(sStmt, SQL_SCROLLABLE);

                ulnError(aFnContext, ulERR_IGNORE_OPTION_VALUE_CHANGED,
                         "SQL_ATTR_CURSOR_SCROLLABLE automatically changed"
                         " to SQL_SCROLLABLE as a result of setting"
                         " SQL_ATTR_CURSOR_TYPE to SQL_CURSOR_KEYSET_DRIVEN");
            }

            /* Notes. ������ Concurrency�� ���� Sensitivity�� �ٲ�� �Ѵ�.
             * ������, Keyset-Driven + Insensitive�� �������� �����Ƿ� �׻� Sensitive�� �ٲ۴�. */
            if (ulnStmtGetAttrCursorSensitivity(sStmt) != SQL_SENSITIVE)
            {
                ulnStmtSetAttrCursorSensitivity(sStmt, SQL_SENSITIVE);

                ulnError(aFnContext, ulERR_IGNORE_OPTION_VALUE_CHANGED,
                         "SQL_ATTR_CURSOR_SENSITIVITY automatically changed"
                         " to SQL_SENSITIVE as a result of setting"
                         " SQL_ATTR_CURSOR_TYPE to SQL_CURSOR_KEYSET_DRIVEN");
            }
            break;

        default:
            /*
             * HY024 : Invalid Attribute Value
             */
            ACI_RAISE(LABEL_INVALID_ATTR_VALUE);
            break;
    }

    return ACI_SUCCESS;

    ACI_EXCEPTION(LABEL_INVALID_ATTR_VALUE)
    {
        ulnError(aFnContext, ulERR_ABORT_INVALID_ATTRIBUTE_VALUE);
    }
    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

/*
 * ulnSetStmtAttr.
 *
 * SQLSetStmtAttr �� ��Ȯ�ϰ� 1:1 �� ��ġ�Ǵ� �Լ�.
 *
 * ����: aValuePtr �� ��Ȳ�� ���� 32(64)��Ʈ ������ /��/���ε� �ؼ��� �� �ְ�,
 *       /������/�ε� �ؼ��� �� �ִ�.
 *       ���� �� ���� NULL ������ üũ�ϴ� ���� �ǹ̰� ����. -_-;;;
 *
 * Note : When the Attribute parameter has one of the following values,
 *        a 64-bit value is returned in *ValuePtr:
 *
 *          SQL_ATTR_APP_PARAM_DESC  --------+
 *          SQL_ATTR_APP_ROW_DESC            |
 *          SQL_ATTR_IMP_PARAM_DESC          |
 *          SQL_ATTR_IMP_ROW_DESC            +--> ������ Ÿ��. �Ű澲�� �ʾƵ� ��.
 *          SQL_ATTR_PARAM_BIND_OFFSET_PTR   |
 *          SQL_ATTR_ROW_BIND_OFFSET_PTR     |
 *          SQL_ATTR_ROWS_FETCHED_PTR -------+
 *
 *          SQL_ATTR_MAX_LENGTH  : �������� ����
 *          SQL_ATTR_KEYSET_SIZE : �������� ����
 *          SQL_ATTR_MAX_ROWS    : �������� ����
 *
 *          SQL_ATTR_ROW_ARRAY_SIZE
 *          SQL_ATTR_ROW_NUMBER
 *
 * ���� �Ӽ��� �� �ϳ��� ������ ������ aValuePtr ���� 64 ��Ʈ ���� �Ѿ�´�.
 *
 * ���� : 32 ��Ʈ�� ������ �� ������ 32��Ʈ ���̴�.
 */

SQLRETURN ulnSetStmtAttr(ulnStmt      *aStmt,
                         acp_sint32_t  aAttribute,
                         void         *aValuePtr,
                         acp_sint32_t  aStringLength)
{
    ULN_FLAG(sNeedExit);

    ulnFnContext  sFnContext;
    acp_uint32_t  sValue;

    ACP_UNUSED(aStringLength);

    ULN_INIT_FUNCTION_CONTEXT(sFnContext, ULN_FID_SETSTMTATTR, aStmt, ULN_OBJ_TYPE_STMT);

    sValue = (acp_uint32_t)(((acp_ulong_t)aValuePtr) & ACP_UINT32_MAX);

    ACI_TEST(ulnEnter(&sFnContext, (void *)(&aAttribute)) != ACI_SUCCESS);
    ULN_FLAG_UP(sNeedExit);

    ACI_TEST(ulnStmtAttrCheckUnsupportedAttr(&sFnContext, aAttribute) != ACI_SUCCESS);

    switch (aAttribute)
    {
        case SQL_ATTR_APP_PARAM_DESC:
            ACI_TEST(ulnSetStmtAttrCheckHY017(&sFnContext, aValuePtr) != ACI_SUCCESS);

            ACI_TEST_RAISE( aStmt->mParentDbc != (ulnDbc *)(((ulnDesc *)aValuePtr)->mParentObject),
                            LABEL_INVALID_ATTR_VALUE );

            ulnStmtSetApd(aStmt, (ulnDesc *)aValuePtr);
            break;

        case SQL_ATTR_APP_ROW_DESC:
            ACI_TEST(ulnSetStmtAttrCheckHY017(&sFnContext, aValuePtr) != ACI_SUCCESS);

            ACI_TEST_RAISE( aStmt->mParentDbc != (ulnDbc *)(((ulnDesc *)aValuePtr)->mParentObject),
                            LABEL_INVALID_ATTR_VALUE );

            ulnStmtSetArd(aStmt, (ulnDesc *)aValuePtr);
            break;

        case SQL_ATTR_IMP_PARAM_DESC:
        case SQL_ATTR_IMP_ROW_DESC:
            ACI_RAISE(LABEL_INVALID_USE);
            break;

        /*
         * Note: �Ʒ��� �� �Ӽ� (SQL_ATTR_CONCURRENCY, SQL_ATTR_CURSOR_SCROLLABLE,
         *       SQL_ATTR_CURSOR_SENSITIVITY, SQL_ATTR_CURSOR_TYPE) �� �ϳ��� �մ��
         *       �ٸ� �͵� consistency ������ ���ؼ� �Բ� �ٲ�� ��� �Ѵ�.
         *       �ʿ��ϴٸ� �ٸ� �Ӽ��� �մ� �� �ִ�. consistency ������ ���ؼ�
         *        -- refer to M$DN ODBC Cursor Characteristics and Cursor Type section
         */
        case SQL_ATTR_CONCURRENCY:
            ACI_TEST(ulnSetStmtAttrCheck24000(&sFnContext) != ACI_SUCCESS);
            ACI_TEST(ulnSetStmtAttrCheckHY011(&sFnContext) != ACI_SUCCESS);
            ACI_TEST(ulnSetStmtAttrDoConcurrency(&sFnContext, sValue) != ACI_SUCCESS);
            break;

        case SQL_ATTR_CURSOR_SCROLLABLE:
            ACI_TEST(ulnSetStmtAttrDoScrollable(&sFnContext, sValue) != ACI_SUCCESS);
            break;

        case SQL_ATTR_CURSOR_SENSITIVITY:
            ACI_TEST(ulnSetStmtAttrDoSensitivity(&sFnContext, sValue) != ACI_SUCCESS);
            break;

        case SQL_ATTR_CURSOR_TYPE:
            ACI_TEST(ulnSetStmtAttrCheck24000(&sFnContext) != ACI_SUCCESS);
            ACI_TEST(ulnSetStmtAttrCheckHY011(&sFnContext) != ACI_SUCCESS);
            ACI_TEST(ulnSetStmtAttrDoCursorType(&sFnContext, sValue) != ACI_SUCCESS);
            break;

        case SQL_ATTR_PARAM_BIND_OFFSET_PTR:
            ulnStmtSetAttrParamBindOffsetPtr(aStmt, (ulvULen *)aValuePtr);
            /*
             * BUGBUG: �Ʒ��� descriptor attribute �� ���� �����ؾ� �Ѵٰ� �ϸ鼭��,
             *         Ÿ���� ���� SQLINTEGER �� SQL/U/INTEGER �� ���� �ٸ��� -_-;;;
             *         ��δ� acp_uint32_t �� ��������.
             */
            break;

        case SQL_ATTR_PARAM_BIND_TYPE:
            ulnStmtSetAttrParamBindType(aStmt, sValue);
            break;

        case SQL_ATTR_PARAM_OPERATION_PTR:
            ulnStmtSetAttrParamOperationPtr(aStmt, (acp_uint16_t *)aValuePtr);
            break;

        case SQL_ATTR_PARAM_STATUS_PTR:
            ulnStmtSetAttrParamStatusPtr(aStmt, (acp_uint16_t *)aValuePtr);
            break;

        case SQL_ATTR_PARAMS_PROCESSED_PTR:
            ulnStmtSetAttrParamsProcessedPtr(aStmt, (ulvULen *)aValuePtr);
            break;

        case SQL_ATTR_PARAMSET_SIZE:
            //BUG-21253 �ѹ��� array insert�ϴ� ���� �����ؾ� �մϴ�.
            ACI_TEST_RAISE( sValue > ACP_UINT16_MAX,
                            LABEL_EXCEEDING_ARRAY_SIZE_LIMIT );
            ulnStmtSetAttrParamsetSize(aStmt, sValue);
            break;

        case SQL_ATTR_QUERY_TIMEOUT:
            /* BUGBUG: DataSource �� max/min timeout ���� ���;� �Ѵ�.
             *         ������ ����� 01S02 Option Val Changed �� �����Ѵ�. */
            ulnStmtSetAttrQueryTimeout(aStmt, sValue);
            break;

        case SQL_ATTR_RETRIEVE_DATA:
            ACI_TEST_RAISE( sValue != SQL_RD_ON && sValue != SQL_RD_OFF,
                            LABEL_INVALID_ATTR_VALUE );

            /* �� ���� RD_OFF �� �����ϸ�, �����͸� ��ġ���� �ʰ�, Ŀ���� �����̶��
             * ���̴�. �׷���, �׷��� �ؼ� ��ũ���� ������, ��ũ�� �� �� �� ����.
             * optional feature not supported �� ���� ��� �Ѵ�.
             * BUGBUG : �ƴ�, option value changed �ΰ�? */
            if (sValue == SQL_RD_OFF)
            {
                ulnError(&sFnContext, ulERR_IGNORE_OPTION_VALUE_CHANGED,
                         "SQL_ATTR_RETRIEVE_DATA changed to SQL_RD_ON");

                sValue = SQL_RD_ON;
            }

            ulnStmtSetAttrRetrieveData(aStmt, sValue);
            break;

        /* ExtendedFetch ���� �� �Ӽ��� ����ϹǷ� ����־���. */
        // fix BUG-20372
        // BUG-17715 ����
        case SQL_ROWSET_SIZE:
            if (aStmt->mPrevRowSetSize == 0)
            {
                aStmt->mPrevRowSetSize = ulnStmtGetRowSetSize(aStmt);
            }
            ulnStmtSetRowSetSize(aStmt, sValue);
            break;

        // fix BUG-20372
        // BUG-17715 ����
        case SQL_ATTR_ROW_ARRAY_SIZE:
            // bug-35198: ������ fetch rowset size�� �ѹ��� ����ؾ� ��
            if (aStmt->mPrevRowSetSize == 0)
            {
                aStmt->mPrevRowSetSize = ulnStmtGetAttrRowArraySize(aStmt);
            }
            ulnStmtSetAttrRowArraySize(aStmt, sValue);
            break;

        case SQL_ATTR_ROW_BIND_OFFSET_PTR:
            ulnStmtSetAttrRowBindOffsetPtr(aStmt, (ulvULen *)aValuePtr);
            /* BUGBUG: �Ʒ��� descriptor attribute �� ���� �����ؾ� �Ѵٰ� �ϸ鼭��,
             *         Ÿ���� ���� SQLINTEGER �� SQL/U/INTEGER �� ���� �ٸ��� -_-;;;
             *         ��δ� acp_uint32_t �� ��������. */
            break;

        case SQL_ATTR_ROW_BIND_TYPE:
            ulnStmtSetAttrRowBindType(aStmt, sValue);
            break;

        case SQL_ATTR_ROW_OPERATION_PTR:
            ulnStmtSetAttrRowOperationPtr(aStmt, (acp_uint16_t *)aValuePtr);
            break;

        case SQL_ATTR_ROW_STATUS_PTR:
            ulnStmtSetAttrRowStatusPtr(aStmt, (acp_uint16_t *)aValuePtr);
            break;

        case SQL_ATTR_ROWS_FETCHED_PTR:
            ulnStmtSetAttrRowsFetchedPtr(aStmt, (ulvULen *)aValuePtr);
            break;

        case SQL_ATTR_PREFETCH_ROWS:
            ulnStmtSetAttrPrefetchRows(aStmt, sValue);
            ulnStmtSetAttrPrefetchMemory(aStmt, 0);
            ulnStmtSetAttrPrefetchBlocks(aStmt, 0);
            break;

        case SQL_ATTR_PREFETCH_BLOCKS:
            ulnStmtSetAttrPrefetchRows(aStmt, 0);
            ulnStmtSetAttrPrefetchBlocks(aStmt, sValue);
            ulnStmtSetAttrPrefetchMemory(aStmt, 0);
            break;

        case SQL_ATTR_PREFETCH_MEMORY:
            ulnStmtSetAttrPrefetchRows(aStmt, 0);
            ulnStmtSetAttrPrefetchBlocks(aStmt, 0);
            ulnStmtSetAttrPrefetchMemory(aStmt, sValue);
            break;

        case SQL_ATTR_INPUT_NTS:
            if (sValue == SQL_FALSE)
            {
                ulnStmtSetAttrInputNTS(aStmt, ACP_FALSE);
            }
            else
            {
                ulnStmtSetAttrInputNTS(aStmt, ACP_TRUE);
            }
            break;

        // PROJ-1518
        case ALTIBASE_STMT_ATTR_ATOMIC_ARRAY:
            ACI_TEST_RAISE((sValue != SQL_TRUE) && (sValue != SQL_FALSE),
                           LABEL_INVALID_ATTR_VALUE);

            ulnStmtSetAtomicArray(aStmt, sValue);
            break;

        /* PROJ-1381 Fetch Across Commits */
        case SQL_ATTR_CURSOR_HOLD:
            ACI_TEST_RAISE((sValue != SQL_CURSOR_HOLD_ON) && (sValue != SQL_CURSOR_HOLD_OFF),
                           LABEL_INVALID_ATTR_VALUE);

            ulnStmtSetAttrCursorHold(aStmt, sValue);
            break;

        /* PROJ-1789 Updatable Scrollable Cursor */

        case SQL_ATTR_FETCH_BOOKMARK_PTR:
            ulnStmtSetAttrFetchBookmarkPtr(aStmt, (acp_uint8_t *)aValuePtr);
            break;

        case SQL_ATTR_USE_BOOKMARKS:
            ACI_TEST(ulnSetStmtAttrCheck24000(&sFnContext) != ACI_SUCCESS);
            ACI_TEST(ulnSetStmtAttrCheckHY011(&sFnContext) != ACI_SUCCESS);

            switch (sValue)
            {
                case SQL_UB_VARIABLE:
                case SQL_UB_FIXED: /* is same as SQL_UB_ON */
                case SQL_UB_OFF:
                    ulnStmtSetAttrUseBookMarks(aStmt, sValue);
                    break;

                default:
                    ACI_RAISE(LABEL_INVALID_ATTR_VALUE);
                    break;
            }
            break;

            /* PROJ-1891 Deferred Prepare */
        case SQL_ATTR_DEFERRED_PREPARE:
            ulnStmtSetAttrDeferredPrepare(aStmt, (acp_char_t)sValue);
            break;

        /* PROJ-2625 Semi-async Prefetch, Prefetch Auto-tuning */
        case ALTIBASE_PREFETCH_ASYNC:
            if ((sValue == ALTIBASE_PREFETCH_ASYNC_OFF) ||
                (sValue == ALTIBASE_PREFETCH_ASYNC_PREFERRED))
            {
                ACI_TEST(ulnStmtSetAttrPrefetchAsync(&sFnContext, sValue)
                        != ACI_SUCCESS);
            }
            else
            {
                /* HY024 : Invalid attribute value */
                ACI_RAISE(LABEL_INVALID_ATTR_VALUE);
            }
            break;

        case ALTIBASE_PREFETCH_AUTO_TUNING:
            if ((sValue == ALTIBASE_PREFETCH_AUTO_TUNING_OFF) ||
                (sValue == ALTIBASE_PREFETCH_AUTO_TUNING_ON))
            {
                ulnStmtSetAttrPrefetchAutoTuning(&sFnContext,
                        (sValue == ALTIBASE_PREFETCH_AUTO_TUNING_ON) ? ACP_TRUE : ACP_FALSE);
            }
            else
            {
                /* HY024 : Invalid attribute value */
                ACI_RAISE(LABEL_INVALID_ATTR_VALUE);
            }
            break;

        case ALTIBASE_PREFETCH_AUTO_TUNING_INIT:
            ulnStmtSetAttrPrefetchAutoTuningInit(aStmt, sValue);
            break;

        case ALTIBASE_PREFETCH_AUTO_TUNING_MIN:
            ulnStmtSetAttrPrefetchAutoTuningMin(&sFnContext, sValue);
            break;

        case ALTIBASE_PREFETCH_AUTO_TUNING_MAX:
            ulnStmtSetAttrPrefetchAutoTuningMax(&sFnContext, sValue);
            break;

        /* BUG-44858 */
        case ALTIBASE_PREPARE_WITH_DESCRIBEPARAM:
            if ((sValue == ALTIBASE_PREPARE_WITH_DESCRIBEPARAM_OFF) ||
                (sValue == ALTIBASE_PREPARE_WITH_DESCRIBEPARAM_ON))
            {
                ulnStmtSetAttrPrepareWithDescribeParam(aStmt, (acp_bool_t)sValue);
            }
            else
            {
                ACI_RAISE(LABEL_INVALID_ATTR_VALUE);
            }
            break;

        /* BUG-44957 */
        case SQL_ATTR_PARAMS_ROW_COUNTS_PTR:
            ulnStmtSetAttrParamsRowCountsPtr(aStmt, (acp_sint32_t *)aValuePtr);
            break;

        case SQL_ATTR_PARAMS_SET_ROW_COUNTS:
            if ((sValue == SQL_ROW_COUNTS_ON) ||
                (sValue == SQL_ROW_COUNTS_OFF))
            {
                ulnStmtSetAttrParamsSetRowCounts(aStmt, sValue);
            }
            else
            {
                /* HY024 : Invalid attribute value */
                ACI_RAISE(LABEL_INVALID_ATTR_VALUE);
            }
            break;

        case SQL_ATTR_ROW_NUMBER:   /* ������ �� ���� �Ӽ��̴� */
        default:
            ACI_RAISE(LABEL_INVALID_ATTR);
            break;
    }

    ULN_FLAG_DOWN(sNeedExit);
    ACI_TEST(ulnExit(&sFnContext) != ACI_SUCCESS);

    ULN_TRACE_LOG(&sFnContext, ULN_TRACELOG_MID, NULL, 0,
            "%-18s| [%2"ACI_INT32_FMT" val: 0x%08x]",
            "ulnSetStmtAttr", aAttribute, sValue);

    return ULN_FNCONTEXT_GET_RC(&sFnContext);

    ACI_EXCEPTION(LABEL_INVALID_ATTR)
    {
        /*
         * HY092: Invalid attribute/option identifier. The attribute is read-only.
         */
        ulnError(&sFnContext, ulERR_ABORT_INVALID_ATTR_OPTION, aAttribute);
    }
    ACI_EXCEPTION(LABEL_EXCEEDING_ARRAY_SIZE_LIMIT)
    {
        ulnError(&sFnContext, ulERR_ABORT_INVALID_ARRAY_SIZE);
    }
    ACI_EXCEPTION(LABEL_INVALID_ATTR_VALUE)
    {
        ulnError(&sFnContext, ulERR_ABORT_INVALID_ATTRIBUTE_VALUE);
    }
    ACI_EXCEPTION(LABEL_INVALID_USE)
    {
        ulnError(&sFnContext, ulERR_ABORT_INVALID_USE_OF_IMP_DESC);
    }
    ACI_EXCEPTION_END;

    ULN_IS_FLAG_UP(sNeedExit)
    {
        ulnExit(&sFnContext);
    }

    ULN_TRACE_LOG(&sFnContext, ULN_TRACELOG_LOW, NULL, 0,
            "%-18s| [%2"ACI_INT32_FMT" val: 0x%08x] fail",
            "ulnSetStmtAttr", aAttribute, sValue);

    return ULN_FNCONTEXT_GET_RC(&sFnContext);
}
