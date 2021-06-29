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

/*****************************************************************************
 * Server ������ ����ϴ� SQLó���� ����Ѵ�.
 * ��� �Լ��� callback function�� �ȴ�.
 ****************************************************************************/

#ifndef _O_MMT_INTERNAL_SQL_H_
#define _O_MMT_INTERNAL_SQL_H_ 1

#include <qci.h>

class mmtInternalSql
{
public:
    /*
     * Internal SQL Callbacks
     *
     * static IDE_RC callbackFunction( void * aUserContext );
     *
     * User Context Parameter�� ����ȯ �Ͽ� ����Ѵ�.
     */

    static qciInternalSQLCallback mCallback;


    // mmcStatement�� �ϳ� alloc
    static IDE_RC allocStmt( void * aUserContext );

    // sql prepare
    static IDE_RC prepare( void * aUserContext );

    // bind parameter info
    static IDE_RC paramInfoSet( void * aUserContext );

/*     BUG-19669 */
/*     client���� qci::setBindColumnInfo()�� ������� �����Ƿ� */
/*     PSM�� ������� �ʵ��� �����Ѵ�. */
/*     // bind column info */
/*     static IDE_RC columnInfoSet( void * aUserContext ); */

    // bindParameter data
    static IDE_RC bindParamData( void * aUserContext );

    // execute
    static IDE_RC execute( void * aUserContext );

    // fetch
    static IDE_RC fetch( void * aUserContext );

    // mmcStatement�� free
    static IDE_RC freeStmt( void * aUserContext );

    // bind parameter count�� üũ(prepare����)
    static IDE_RC checkBindParamCount( void * aUserContext );

    // bind column count�� üũ(fetch����)
    static IDE_RC checkBindColumnCount( void * aUserContext );

    // PROJ-2197 PSM Renewal
    static IDE_RC getQciStmt( void * aUserContext );

    // BUG-36203 PSM Optimize
    static IDE_RC endFetch( void * aUserContext );
};

#endif
