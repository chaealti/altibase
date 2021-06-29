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
 * $Id: smcTableSpace.h 19201 2006-11-30 00:54:40Z kmkim $
 **********************************************************************/

#ifndef _O_SMC_TABLE_SPACE_H_
#define _O_SMC_TABLE_SPACE_H_ 1


#include <idu.h>
#include <iduMemPool.h>
#include <idp.h>
#include <smDef.h>
#include <smcDef.h>



/*
   Table Space�ȿ� �ִ� ������ Table�� ���� ������ Action�Լ�

   aSlotFlag     - Table Header�� ����� Catalog Slot�� Drop Flag
   aSlotSCN      - Table Header�� ����� Catalog Slot�� SCN
   aTableHeader  - Table�� Header
   aActionArg    - Action �Լ� Argument

*/
typedef IDE_RC (*smcAction4Table)( idvSQL*          aStatistics,
                                   ULong            aSlotFlag,
                                   smSCN            aSlotSCN,
                                   smcTableHeader * aTableHeader,
                                   void           * aActionArg );
/*
   Tablespace���� �ڵ��� catalog table�� ������ �ʿ��� ������ ����


   [ Offline Tablespace�� ���� Table�� ���� �����ϴ� ���� ]
     - smcTableHeader
       - mLock
         - Offline/Online Tablespace��� Table Lock�� ȹ����
           Tablespace�� ��밡��(Online)���� üũ�ؾ� �ϱ� ����
       - mRuntimeInfo
         - QP���� �����ϴ� �ڷ�
         - Table�� qcmTableInfo�� Stored Procedure�� Plan Tree
         - Tablespace�� Page Memory�� ����Ű�� �����Ƿ� �״�� �д�.

       - ���� �ʵ�� NULL�� ������ ( �����Ǿ�� �ȵ� )
         - mDropIndexLst, mDropIndex
           - Drop�Ǿ����� dropIndexPnding�� ���� ������� ���� Index�� ����
             Catalog Table�� smnIndexHeader�� �����ϰ� �ִ� ����

         - mFixed.mMRDB.mRuntimeEntry
           - Tablespace�� Page Memory�� ���� ����Ű�� �ڷᱸ��
           - �ش� �޸�/��ü�� ��� Destroy�ǰ� NULL�� ���õȴ�.
           - Offline Tablespace�� ���� Table�� ���
             �� Field�� �����ؼ��� �ȵ�

   [ Online Tablespace�� ���� Table�� ���� �����ϴ� ���� ]
     - smcTableHeader.mLock
       - mLock
       - mFixed.mMRDB.mRuntimeEntry
       - mRuntimeInfo
       - mDropIndexLst
       - mDropIndex


   [ Alter Tablespace Offline�� Destroy�� �ʵ�� ]
     - mDropIndexLst, mDropIndex
       - Tablespace X���� ���� ���¶�� �� �� �ʵ�� NULL, 0 �̾�� ��
         - Drop Index�� ������ Tx�� Commit/Abort�ÿ�
           smxTrans::addListToAger����
           smcTable::dropIndexList�� ȣ���Ͽ� dropIndexPending �����ϰ�
           �� �� �ʵ带 �ʱ�ȭ �ϱ� ����

     - mFixed.mMRDB.mRuntimeEntry
       - Alloced Page List, Free Page List�� ���� Tablespace��
         Page Memory�� �����ϴ� �ڷᱸ���̴�.
       - Offline���� ���ϴ� Tablespace�� Page Memory��
         ��� Free�� ���̹Ƿ� �� �ڷᱸ���� �����Ͱ� �ƹ� �ǹ̰� ����.

   [ Alter Tablespace Online�� �ʱ�ȭ�� �ʵ�� ]
     - mFixed.mMRDB.mRuntimeEntry
       - Tablespace�� Disk Checkpoint Image -> Memory�� Restore �� ��
         Tablespace���� Page Memory������ ����������.
       - Table�� Page Memory�� �������� Refine�۾��� �����Ͽ�
         mRuntimeEntry�� ���� �����Ѵ�.

 */
class smcTableSpace
{
public :
    // ������ (�ƹ��͵� ����)
    smcTableSpace();


    // Tablespace���� ������ Table�� ���� Ư�� �۾��� �����Ѵ�.
    static IDE_RC run4TablesInTBS( idvSQL*           aStatistics,
                                   scSpaceID         aTBSID,
                                   smcAction4Table   aActionFunc,
                                   void            * aActionArg);


    // Online->Offline���� ���ϴ� Tablespace�� ���� Table�鿡 ���� ó��
    static IDE_RC alterTBSOffline4Tables(idvSQL*      aStatistics,
                                         scSpaceID    aTBSID );


    // Offline->Online���� ���ϴ� Tablespace�� ���� Table�鿡 ���� ó��
    static IDE_RC alterTBSOnline4Tables(idvSQL     * aStatistics,
                                        void       * aTrans,
                                        scSpaceID    aTBSID );


private:
    // Catalog Table���� Ư�� Tablespace�� ����� Table�� ����
    // Action�Լ��� �����Ѵ�.
    static IDE_RC run4TablesInTBS( idvSQL*           aStatistics,
                                   smcTableHeader  * aCatTableHeader,
                                   scSpaceID         aTBSID,
                                   smcAction4Table   aActionFunc,
                                   void            * aActionArg);


    // Online->Offline���� ���ϴ� Tablespace�� ���� Table�� ���� Action�Լ�
    static IDE_RC alterTBSOfflineAction( idvSQL*          aStatistics,
                                         ULong            aSlotFlag,
                                         smSCN            aSlotSCN,
                                         smcTableHeader * aTableHeader,
                                         void           * aActionArg );


    // Offline->Online ���� ���ϴ� Tablespace�� ���� Table�� ���� Action�Լ�
    static IDE_RC alterTBSOnlineAction( idvSQL*          aStatistics,
                                        ULong            aSlotFlag,
                                        smSCN            aSlotSCN,
                                        smcTableHeader * aTableHeader,
                                        void           * aActionArg );


    // Offline->Online ���� ���ϴ� Disk Tablespace�� ����
    // Table�� ���� Action�Լ�
    static IDE_RC alterDiskTBSOnlineAction( idvSQL*          aStatistics,
                                            ULong            aSlotFlag,
                                            smSCN            aSlotSCN,
                                            smcTableHeader * aTableHeader,
                                            void           * aActionArg );


    // Online->Offline ���� ���ϴ� Disk Tablespace�� ����
    // Table�� ���� Action�Լ�
    static IDE_RC alterDiskTBSOfflineAction( ULong            aSlotFlag,
                                             smSCN            aSlotSCN,
                                             smcTableHeader * aTableHeader,
                                             void           * aActionArg );


    // Offline->Online ���� ���ϴ� Memory Tablespace�� ����
    // Table�� ���� Action�Լ�
    static IDE_RC alterMemTBSOnlineAction( ULong            aSlotFlag,
                                           smSCN            aSlotSCN,
                                           smcTableHeader * aTableHeader,
                                           void           * aActionArg );


    // Online->Offline ���� ���ϴ� Memory Tablespace�� ����
    // Table�� ���� Action�Լ�
    static IDE_RC alterMemTBSOfflineAction( smcTableHeader * aTableHeader );
};

#endif /* _O_SMC_TABLE_SPACE_H_ */
