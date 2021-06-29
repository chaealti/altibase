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

#ifndef CDBC_H
#define CDBC_H 1



#if defined(DEBUG)
    /* ACE_DASSERT()�� ���� ���ؼ� DEBUG ����� �� ACP_CFG_DEBUG�� ������ �����. */
    #if !defined(ACP_CFG_DEBUG)
        #define ACP_CFG_DEBUG   1
    #endif
#endif

#include <acp.h>
#include <aciErrorMgr.h>
#include <sqlcli.h>

/*
 * alticdbc.h���� ���� �̸��� ���°͵� undef. warning ���Ÿ� ����.
 * �� ��ü�� sqlcli.h�� ���ǵ� �Ͱ� ����.
 */
#undef ALTIBASE_DATE_FORMAT
#undef ALTIBASE_NLS_USE
#undef ALTIBASE_APP_INFO
#undef ALTIBASE_NLS_NCHAR_LITERAL_REPLACE
#undef ALTIBASE_STMT_ATTR_ATOMIC_ARRAY
#undef ALTIBASE_CLIENT_IP
#undef ALTIBASE_FO_BEGIN
#undef ALTIBASE_FO_END
#undef ALTIBASE_FO_ABORT
#undef ALTIBASE_FO_GO
#undef ALTIBASE_FO_QUIT

#include <alticdbc.h>
#include <cdbcDef.h>
#include <cdbcBufferMng.h>
#include <cdbcLog.h>
#include <cdbcInfo.h>
#include <cdbcVersion.h>
#include <cdbcHandle.h>
#include <cdbcError.h>
#include <cdbcException.h>
#include <cdbcMeta.h>
#include <cdbcArray.h>
#include <cdbcFailover.h>
#include <cdbcBind.h>
#include <cdbcFetch.h>



/**
 * aIdx�� aMin ~ (aMax - 1) ���̿� �ִ��� Ȯ���Ѵ�.
 */
#define INDEX_IS_VALID(aIdx,aMin,aMax)      ( ! INDEX_NOT_VALID(aIdx, aMin, aMax) )
#define INDEX_NOT_VALID(aIdx,aMin,aMax)     ( ((aIdx) < (aMin)) || ((aMax) <= (aIdx)) )



#endif /* CDBC_H */

