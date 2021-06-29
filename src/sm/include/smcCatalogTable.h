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
 * $Id: smcCatalogTable.h 88191 2020-07-27 03:08:54Z mason.lee $
 **********************************************************************/

#ifndef _O_SMC_CATALOG_TABLE_H_
#define _O_SMC_CATALOG_TABLE_H_ 1


#include <smDef.h>
#include <smcDef.h>

class smcCatalogTable
{
public:

    /* DB �����ÿ� Catalog Table�� Temp Catalog Table ���� */
    static IDE_RC createCatalogTable();

    /* Server shutdown�� Catalog Table�� ���� �޸� ���� ���� */
    static IDE_RC finalizeCatalogTable();

    /* Server startup�� Catalog Table�� ���� �ʱ�ȭ ���� */
    static IDE_RC initialize();

    /* Server shutdown�� Catalog Table�� ���� �޸� ���� ���� */
    static IDE_RC destroy();

    /* Temp Catalog Table Offset�� return */
    static UInt getCatTempTableOffset();

    /* restart recovery�ÿ�
       disk table�� header�� �ʱ�ȭ�ϰ� �ش� table��
       ��� index runtime header��
       rebuild �Ѵ�.*/
    static IDE_RC refineDRDBTables();

   static IDE_RC doAction4EachTBL(idvSQL            * aStatistics,
                                   smcAction4TBL       aAction,
                                   void              * aActionArg );

private:

   // Create DB�� Catalog Table�� �����Ѵ�.
    static IDE_RC createCatalog( void*   aCatTableHeader,
                                 UShort  aOffset );

    // Shutdown�� Catalog Table�� �����۾��� ����
    static IDE_RC finCatalog( void* sCatTableHeader );

    //  Catalog Table���� Used Slot�� ���� Lock Item�� Runtime Item����
    static IDE_RC finAllocedTableSlots( smcTableHeader * aCatTblHdr );
};

#endif /* _O_SMC_CATALOG_TABLE_H_ */
