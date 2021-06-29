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
 * $Id: qcc.h 88963 2020-10-19 03:33:18Z jake.jang $
 **********************************************************************/
#ifndef _O_QCC_H_
#define  _O_QCC_H_  1

#include <qc.h>

// qc common
class qcc
{
public:
    // dummy parsing
    static IDE_RC parse( qcStatement * aStatement );
    static IDE_RC parseError( qcStatement * aStatement );

    // dummy validation
    static IDE_RC validate( qcStatement * aStatement );

    // dummy optimization
    static IDE_RC optimize( qcStatement * aStatement );

    // dummy exection
    static IDE_RC execute( qcStatement * aStatement );
    // execution error function
    static IDE_RC executeError(qcStatement * aStatement);

};

#endif // _O_QCC_H_
