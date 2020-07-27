/**
 *  Copyright (c) 1999~2017, Altibase Corp. and/or its affiliates. All rights reserved.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU Affero General Public License, version 3,
 *  as published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU Affero General Public License for more details.
 *
 *  You should have received a copy of the GNU Affero General Public License
 *  along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <mmm.h>
#include <mmmActList.h>


/* =======================================================
 *  Action Descriptor
 * =====================================================*/
static mmmPhaseAction *gShutdownActions[] =
{
    &gMmmActBeginShutdown,

    &gMmmActShutdownPrepare,
    &gMmmActShutdownSNMP,
    &gMmmActShutdownService,
    &gMmmActShutdownQueryProfile,
    &gMmmActShutdownREPL,
    &gMmmActShutdownFixedTable,
    &gMmmActShutdownSecurity,
    &gMmmActShutdownDK,
    &gMmmActShutdownQP,
    &gMmmActShutdownSM,
    &gMmmActShutdownMT,
    &gMmmActShutdownCM,
    &gMmmActShutdownSD,

//     &gMmmActShutdownGPKI,
    &gMmmActShutdownTimer,

    &gMmmActShutdownProperty,
    &gMmmActEndShutdown,
    NULL /* should exist!! */
};

/* =======================================================
 *  Shutdown Phase Descriptor
 * =====================================================*/
mmmPhaseDesc gMmmGoToShutdown =
{
    MMM_STARTUP_SHUTDOWN,
    (SChar *)"Shutdown Altibase",
    gShutdownActions
};
