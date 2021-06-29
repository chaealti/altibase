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
 * $Id$
 ******************************************************************************/

#if !defined(_O_IDU_SHM_KEY_FILE_H_)
#define _O_IDU_SHM_KEY_FILE_H_ 1

#include <iduFile.h>

/* Shared Memory�� ������ XDB Deamon Process�� Startup�ð���
 * ������ ������ �̸� */
#define IDU_SHM_KEY_FILENAME PRODUCT_PREFIX"altibase_shm.info"

/* IDU_SHM_KEY_FILENAME�� ��ϵ� ����Ÿ �����μ�
 * XDB Deamon Process�� Startup�ð��� �ǹ��ϰ� ���� ���� �ι�
 * ����� ������ ������ Valid������ Check�ϱ� ���ؼ� �̴�. */
typedef struct iduStartupInfoOfXDB
{
    /* XDB Deamon Process�� Startup�ð� */
    struct timeval mStartupTime1;
    /* mStartupTime1�� ������ ���� ������. */
    struct timeval mStartupTime2;
    /* ������ ���� SHM_DB_KEY�� ���´�. */
    UInt           mShmDBKey;
} iduStartupInfoOfXDB;

class iduShmKeyFile
{
public:
    static IDE_RC getStartupInfo( struct timeval * aStartUpTime,
                                  UInt           * aPrevShmKey );
    static IDE_RC setStartupInfo( struct timeval * aStartUpTime );
};

#endif /* _O_IDU_SHM_KEY_FILE_H_ */
