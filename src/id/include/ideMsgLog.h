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
 

/***********************************************************************
 * $Id: ideMsgLog.h 81764 2017-11-29 09:42:26Z minku.kang $
 **********************************************************************/

/***********************************************************************
 * NAME
 *  ideMsgLog.h
 *
 * DESCRIPTION
 *  This file defines Error Log.
 *   
 * **********************************************************************/

#ifndef _O_IDEMSGLOG_H_
#define _O_IDEMSGLOG_H_ 1

#include <idl.h>
#include <ideLogEntry.h>

class ideMsgLog
{
public:
    IDE_RC initialize(const ideLogModule,
                      const SChar*,
                      const size_t,
                      const UInt,
                      const idBool = ID_TRUE);
    IDE_RC destroy(void);

    IDE_RC open(idBool = ID_FALSE);
    IDE_RC close(void);
    IDE_RC flush(void);

    IDE_RC logBody(const SChar*, const size_t);
    IDE_RC logBody(const ideLogEntry&);

    PDL_HANDLE getFD();

    inline IDE_RC initialize(const SChar* aPath, const size_t aSize, const UInt aCnt)
    {
        return initialize(IDE_SERVER, aPath, aSize, aCnt);
    }

    static IDE_RC setPermission(const UInt);

private:
    idBool          checkExist(void);
    IDE_RC          rotate(void);
    inline void     unrotate(void) { mRotating = 0; }

    /*
     * ���� ����� ��� ����.  ���� �� �޸𸮸� ����Ʈ aMmapIndex
     * ��ġ�� ���� �� ���� ��� �� ǥ��.  ǥ���� �� �α� �޼����� ��
     * ���Ͽ� ��.
     */
    IDE_RC createFileAndHeader(void);
    IDE_RC closeAndRename(void);
    idBool checkHeader(void);

    idBool isSameFile( PDL_HANDLE    aFD,
                       SChar       * aPath );
    IDE_RC writeWarningMessage( void );

    PDL_HANDLE          mFD;                /* File handle */
    SChar               mPath[ID_MAX_FILE_NAME];    /* File name */
    size_t              mSize;              /* File size */
    UInt                mMaxNumber;         /* Loop file number */
    UInt                mCurNumber;         /* Replace�� ȭ�Ϲ�ȣ */
    ideLogModule        mSelf;              /* What log this object writes */
    SInt                mRotating;          /* A lock */
    idBool              mEnabled;           /* Enabled */
    idBool              mDebug;

    static UInt         mPermission;        /* Permission */
};

#endif /* _O_IDEMSGLOG_H_ */
