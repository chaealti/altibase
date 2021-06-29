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
 
/*******************************************************************************
 * $Id:$
 ******************************************************************************/

#include <act.h>
#include <acp.h>

acp_sint32_t main(void)
{
    ACT_TEST_BEGIN();

#if defined(ALTI_CFG_OS_WINDOWS)
    ACT_CHECK(ACP_RC_IS_SUCCESS(acpOSGetVersionInfo()));
#endif

    ACT_TEST_END();

    return 0;
}
