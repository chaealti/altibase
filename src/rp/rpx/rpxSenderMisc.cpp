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
 

/***********************************************************************
 * $Id: rpxSenderMisc.cpp 83321 2018-06-21 07:03:04Z yoonhee.kim $
 **********************************************************************/

#include <idl.h>
#include <ide.h>

#include <smi.h>
#include <smErrorCode.h>

#include <qci.h>

#include <rpDef.h>
#include <rpuProperty.h>
#include <rpxSender.h>

//===================================================================
//
// Name:          isYou
//
// Return Value:  idBool
//
// Argument:
//
// Called By:     rpcExecutor.cpp
//
// Description:
//
//===================================================================
idBool rpxSender::isYou(const SChar* aRepName )
{
    return ( idlOS::strcmp( mMeta.mReplication.mRepName, aRepName ) == 0 )
        ? ID_TRUE : ID_FALSE;
}

/*
 *
 */
void rpxSender::getLocalAddress( SChar ** aMyIP,
                                 SInt * aMyPort )
{
    *aMyIP = mMessenger.mLocalIP;
    *aMyPort = mMessenger.mLocalPort;
}

void rpxSender::getRemoteAddress( SChar ** aPeerIP,
                                  SInt   * aPeerPort )
{
    *aPeerIP = mMessenger.mRemoteIP;
    *aPeerPort = mMessenger.mRemotePort;
}

void rpxSender::getRemoteAddressForIB( SChar      ** aPeerIP,
                                       SInt        * aPeerPort,
                                       rpIBLatency * aIBLatency )
{
    *aPeerIP    = mMessenger.mRemoteIP;
    *aPeerPort  = mMessenger.mRemotePort;
    *aIBLatency = mMessenger.mIBLatency;
}
