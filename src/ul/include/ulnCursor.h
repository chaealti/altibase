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

#ifndef _O_ULN_CURSOR_H_
#define _O_ULN_CURSOR_H_ 1

#include <ulnPrivate.h>

/*
 * Note : Cursor �� "cursor" �� �� �ƹ��͵� �ƴϴ�.
 *        �ٽ� ���ؼ�, �ٺ�������, Ŀ���� "������" �̴�.
 *        �ű� ���ؼ� stmt �� cursor �� ���õ� �Ӽ��� ���� �����̴�.
 */

typedef enum
{
    ULN_CURSOR_STATE_CLOSED,
    ULN_CURSOR_STATE_OPEN
} ulnCursorState;

typedef enum
{
    ULN_CURSOR_DIR_NEXT =  1,
    ULN_CURSOR_DIR_PREV = -1,

    ULN_CURSOR_DIR_MAX
} ulnCursorDir;

struct ulnCursor
{
    ulnStmt        *mParentStmt;
    acp_sint64_t    mPosition;   // To Fix BUG-20482
    ulnCursorDir    mDirection;  /* PROJ-1789 Updatable Scrollable Cursor */
    ulnCursorState  mState;
    ulnCursorState  mServerCursorState; /* Note : ���� Ŀ���� ����
                                           ColumnCount > 0 �� statement �� Execute �� ���� -> ����.
                                           Fetch �ؼ� ���� result set �� ������ �о��� -> ����.
                                           ���� Ŀ�� ���¿� ������� CLOSE CURSOR REQ �� ������
                                           ������ �ȴ�. ���� ���¿��� �����ص� �׳� ��������
                                           �����Ѵ�. �� ������ ������ ���� �ѹ��̶�
                                           I/O transaction �� ���̱� ���Ѱ��� */

    /*
     * STMT �� cursor �� ���õ� attribute ��
     */
    acp_uint32_t    mAttrCursorSensitivity; /* SQL_ATTR_CURSOR_SENSITIVITY */
    acp_uint32_t    mAttrCursorType;        /* SQL_ATTR_CURSOR_TYPE */
    acp_uint32_t    mAttrCursorScrollable;  /* SQL_ATTR_CURSOR_SCROLLABLE */
    acp_uint32_t    mAttrSimulateCursor;    /* SQL_ATTR_SIMULATE_CURSOR */
    acp_uint32_t    mAttrConcurrency;       /* SQL_ATTR_CONCURRENCY */
    acp_uint32_t    mAttrHoldability;       /* SQL_ATTR_CURSOR_HOLD */

    /* PROJ-1789 Updatable Scrollable Cursor */
    acp_uint16_t    mRowsetPos;             /**< rowset�������� position. SetPos�� ���� ����. */
};

void   ulnCursorInitialize(ulnCursor *aCursor, ulnStmt *aParentStmt);

ACI_RC ulnCursorClose(ulnFnContext *aFnContext, ulnCursor *aCursor);
void   ulnCursorOpen(ulnCursor *aCursor);

ACP_INLINE ulnCursorState ulnCursorGetState(ulnCursor *aCursor)
{
    return aCursor->mState;
}

ACP_INLINE void ulnCursorSetState(ulnCursor *aCursor, ulnCursorState aNewState)
{
    aCursor->mState = aNewState;
}

ACP_INLINE void ulnCursorSetServerCursorState(ulnCursor *aCursor, ulnCursorState aState)
{
    aCursor->mServerCursorState = aState;
}

ACP_INLINE ulnCursorState ulnCursorGetServerCursorState(ulnCursor *aCursor)
{
    return aCursor->mServerCursorState;
}

ACP_INLINE void ulnCursorSetType(ulnCursor *aCursor, acp_uint32_t aType)
{
    aCursor->mAttrCursorType = aType;
}

ACP_INLINE acp_uint32_t ulnCursorGetType(ulnCursor *aCursor)
{
    return aCursor->mAttrCursorType;
}

acp_uint32_t ulnCursorGetSize(ulnCursor *aCursor);

ACP_INLINE acp_sint64_t ulnCursorGetPosition(ulnCursor *aCursor)
{
    return aCursor->mPosition;
}

void         ulnCursorSetPosition(ulnCursor *aCursor, acp_sint64_t aPosition);

void         ulnCursorSetConcurrency(ulnCursor *aCursor, acp_uint32_t aConcurrency);
acp_uint32_t ulnCursorGetConcurrency(ulnCursor *aCursor);

ACP_INLINE void ulnCursorSetScrollable(ulnCursor *aCursor, acp_uint32_t aScrollable)
{
    aCursor->mAttrCursorScrollable = aScrollable;
}

ACP_INLINE acp_uint32_t ulnCursorGetScrollable(ulnCursor *aCursor)
{
    return aCursor->mAttrCursorScrollable;
}

void         ulnCursorSetSensitivity(ulnCursor *aCursor, acp_uint32_t aSensitivity);
acp_uint32_t ulnCursorGetSensitivity(ulnCursor *aCursor);

/*
 * ��� row �� ����� ���۷� �����ؾ� �ϴ��� ����ϴ� �Լ�
 */
acp_uint32_t ulnCursorCalcRowCountToCopyToUser(ulnCursor *aCursor);

/*
 * Ŀ���� ��ġ ������ ���õ� �Լ���
 */
void ulnCursorMovePrior(ulnFnContext *aFnContext, ulnCursor *aCursor);
void ulnCursorMoveNext(ulnFnContext *aFnContext, ulnCursor *aCursor);
void ulnCursorMoveRelative(ulnFnContext *aFnContext, ulnCursor *aCursor, acp_sint32_t aOffset);
void ulnCursorMoveAbsolute(ulnFnContext *aFnContext, ulnCursor *aCursor, acp_sint32_t aOffset);
void ulnCursorMoveFirst(ulnCursor *aCursor);
void ulnCursorMoveLast(ulnCursor *aCursor);

#define ULN_CURSOR_POS_BEFORE_START     (-1)
#define ULN_CURSOR_POS_AFTER_END        (-2)

/* PROJ-1381 Fetch Across Commit */

/**
 * Cursor Hold �Ӽ��� ��´�.
 *
 * @param[in] aCursor cursor object
 *
 * @return Holdable�̸� SQL_CURSOR_HOLD_ON, �ƴϸ� SQL_CURSOR_HOLD_OFF
 */
ACP_INLINE acp_uint32_t ulnCursorGetHoldability(ulnCursor *aCursor)
{
    return aCursor->mAttrHoldability;
}

/**
 * Cursor Hold �Ӽ��� �����Ѵ�.
 *
 * @param[in] aCursor      cursor object
 * @param[in] aHoldability holdablility. SQL_CURSOR_HOLD_ON or SQL_CURSOR_HOLD_OFF.
 */
ACP_INLINE void ulnCursorSetHoldability(ulnCursor *aCursor, acp_uint32_t aHoldability)
{
    aCursor->mAttrHoldability = aHoldability;
}


/* PROJ-1789 Updatable Scrollable Cursor */

void         ulnCursorMoveByBookmark(ulnFnContext *aFnContext, ulnCursor *aCursor, acp_sint32_t aOffset);
ulnCursorDir ulnCursorGetDirection(ulnCursor *aCursor);
void         ulnCursorSetDirection(ulnCursor *aCursor, ulnCursorDir aDirection);

/**
 * rowset position�� ��´�.
 *
 * @param[in] aCursor cursor object
 *
 * @return rowset position
 */
ACP_INLINE acp_uint16_t ulnCursorGetRowsetPosition(ulnCursor *aCursor)
{
    return aCursor->mRowsetPos;
}


/**
 * rowset position�� �����Ѵ�.
 *
 * @param[in] aCursor    cursor object
 * @param[in] aRowsetPos rowset position
 */
ACP_INLINE void ulnCursorSetRowsetPosition(ulnCursor *aCursor, acp_uint16_t aRowsetPos)
{
    aCursor->mRowsetPos = aRowsetPos;
}

/* BUG-46100 Session SMN Update */
acp_bool_t ulnCursorHasNoData(ulnCursor *aCursor);

#endif /* _O_ULN_CURSOR_H_ */
