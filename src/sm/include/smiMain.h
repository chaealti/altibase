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
 * $Id: smiMain.h 82075 2018-01-17 06:39:52Z jina.kim $
 **********************************************************************/

#ifndef _O_SMI_MAIN_H_
#define _O_SMI_MAIN_H_ 1

#include <idl.h>
#include <smDef.h>
#include <smiDef.h>

/* SM���� ����� �ݹ��Լ��� */
extern smiGlobalCallBackList gSmiGlobalCallBackList;

/* MEM_MAX_DB_SIZE property���� �ùٸ��� �˻��Ѵ�. */
IDE_RC smiCheckMemMaxDBSize( void );

// ����ڰ� ������ �����ͺ��̽� ũ�⸦ ���� 
// ������ ������ �����ͺ��̽� ũ�⸦ ����Ѵ�.
IDE_RC smiCalculateDBSize( scPageID   aUserDbCreatePageCount,
                           scPageID * aDbCreatePageCount );

// �����ͺ��̽��� ������ �� �ִ� �ִ� Page���� ���
scPageID smiGetMaxDBPageCount( void );

// �����ͺ��̽��� ������ �� �ִ� �ּ� Page���� ���
scPageID smiGetMinDBPageCount( void );

// �ϳ��� �����ͺ��̽� ������ ���ϴ� Page�� ���� �����Ѵ� 
IDE_RC smiGetTBSFilePageCount(scSpaceID aSpaceID, scPageID * aDBFilePageCount);

// �����ͺ��̽��� �����Ѵ�.
IDE_RC smiCreateDB(SChar        * aDBName,
                   scPageID       aCreatePageCount,
                   SChar        * aDBCharSet,
                   SChar        * aNationalCharSet,
                   smiArchiveMode aArchiveMode = SMI_LOG_NOARCHIVE);

idBool smiRuntimeSelectiveLoadingSupport();

/* for A4 Startup Phase */

/*
 *  �� �Լ��� ������ ȣ��� �� ����.
 *  �����ϴ� �������� ȣ��Ǹ�, ������ ���� ����. 
 */

IDE_RC smiCreateDBCoreInit(smiGlobalCallBackList *   /*aCallBack*/);

IDE_RC smiCreateDBMetaInit(smiGlobalCallBackList*    /*aCallBack*/);

IDE_RC smiCreateDBCoreShutdown(smiGlobalCallBackList *   /*aCallBack*/);

IDE_RC smiCreateDBMetaShutdown(smiGlobalCallBackList*    /*aCallBack*/);

IDE_RC smiSmUtilInit(smiGlobalCallBackList*    aCallBack);

IDE_RC smiSmUtilShutdown(smiGlobalCallBackList*    aCallBack);


IDE_RC smiStartup(smiStartupPhase        aPhase,
                  UInt                   aActionFlag,
                  smiGlobalCallBackList* aCallBack = NULL);

IDE_RC smiSetRecStartupEnd();
/*
 * ������ Startup Phase�� �����޴´�. 
 */
smiStartupPhase smiGetStartupPhase();

#endif
 
