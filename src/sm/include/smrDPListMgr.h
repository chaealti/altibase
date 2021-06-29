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
 * $Id: smrDPListMgr.h 19996 2007-01-18 13:00:36Z bskim $
 **********************************************************************/

#ifndef _O_SMR_DPLIST_MGR_
#define _O_SMR_DPLIST_MGR_ 1

#include <iduHash.h>
#include <smm.h>
#include <smrDef.h>

/*
    Dirty Page��� �ɼ�
    - writeDirtyPages4AllTBS �� �ɼ����ڷ� �ѱ��.
 */
typedef enum smrWriteDPOption
{
    SMR_WDP_NONE = 0,
    SMR_WDP_NO_PID_LOGGING = 1, // Media Recovery �Ϸ��Ŀ��� PID�α� ����
    SMR_WDP_FINAL_WRITE    = 2  // Shutdown ������ ���� Flush
} smrWriteDPOption;


// ���� Tablespace�� Dirty Page������ ��ü��(smrDirtyPageList)��
// �����ϴ� Class.

class smrDPListMgr
{
private:
    // Key:scSpaceID => data:smrDirtyPageList
    static iduHash   mMgrHash;
    
public :

    // Tablespace�� Dirty Page�����ڸ� �����ϴ� smrDPListMgr�� �ʱ�ȭ
    static IDE_RC initializeStatic();

    // Tablespace�� Dirty Page�����ڸ� �����ϴ� smrDPListMgr�� �ı�
    static IDE_RC destroyStatic();

    // Ư�� Tablespace�� Dirty Page�����ڿ� Dirty Page�� �߰��Ѵ�.
    static IDE_RC add(scSpaceID aSpaceID,
                      smmPCH*   aPCHPtr,
                      scPageID  aPageID);

    // ��� Tablespace�� Dirty Page���� ���� 
    static IDE_RC getTotalDirtyPageCnt( ULong * aDirtyPageCount);
    
    // Ư�� Tablespace�� Dirty Page���� ����Ѵ�.
    static IDE_RC getDirtyPageCountOfTBS( scSpaceID   aTBSID,
                                          UInt      * aDirtyPageCount );


    // ��� Tablespace�� Dirty Page�� Checkpoint Image�� ��� 
    static IDE_RC writeDirtyPages4AllTBS(
                       sctStateSet                  aStateSet,
                       ULong                      * aTotalCnt,
                       ULong                      * aRemoveCnt,
                       ULong                      * aWaitTime,
                       ULong                      * aSyncTime,
                       smmGetFlushTargetDBNoFunc    aGetFlushTargetDBNoFunc,
                       smrWriteDPOption             aOption );
    
    // ��� Tablespace�� Dirty Page�� SMM=>SMR�� �̵��Ѵ�.
    static IDE_RC moveDirtyPages4AllTBS ( 
                           sctStateSet  aStateSet,
                           ULong *      aNewCnt,
                           ULong *      aDupCnt );

    // ��� Tablespace�� Dirty Page�� Discard ��Ų��. 
    static IDE_RC discardDirtyPages4AllTBS();
    
private :
    // Ư�� Tablespace�� ���� Dirty Page�����ڸ� �����Ѵ�.
    static IDE_RC createDPList(scSpaceID aSpaceID );

    // Ư�� Tablespace�� Dirty Page�����ڸ� �����Ѵ�.
    static IDE_RC removeDPList( scSpaceID aSpaceID );


    // Ư�� Tablespace�� ���� Dirty Page�����ڸ� ã�Ƴ���.
    // ã�� ���� ��� NULL�� ���ϵȴ�.
    static IDE_RC findDPList( scSpaceID           aSpaceID,
                              smrDirtyPageList ** aDPList );

    // smrDirtyPageList::writeDirtyPage�� �����ϴ� Action�Լ�
    static IDE_RC writeDirtyPageAction( idvSQL            * aStatistics,
                                        sctTableSpaceNode * aTBSNode,
                                        void * aActionArg );

    // smrDirtyPageList::writePIDLogs�� �����ϴ� Action�Լ�
    static IDE_RC writePIDLogAction( idvSQL            * aStatistics,
                                     sctTableSpaceNode * aTBSNode,
                                     void              * aActionArg );
    
    // ������ Hash Element�� ���� ȣ��� Visitor�Լ�
    static IDE_RC destoyingVisitor( vULong   aKey,
                                    void   * aData,
                                    void   * aVisitorArg);

    //  Hashtable�� ��ϵ� ��� smrDirtyPageList�� destroy�Ѵ�.
    static IDE_RC destroyAllMgrs();

    // Ư�� Tablespace�� Dirty Page�� SMM=>SMR�� �̵��Ѵ�.
    static IDE_RC moveDirtyPages4TBS( scSpaceID   aSpaceID,
                                      UInt      * aNewCnt,
                                      UInt      * aDupCnt );

    //  moveDirtyPages4AllTBS �� ���� Action�Լ� 
    static IDE_RC moveDPAction( idvSQL*             aStatistics,
                                sctTableSpaceNode * aTBSNode,
                                void * aActionArg );

    // getTotalDirtyPageCnt �� ���� Action�Լ� 
    static IDE_RC countDPAction( idvSQL*             aStatistics,
                                 sctTableSpaceNode * aSpaceNode,
                                 void * aActionArg );

    // Ư�� Tablespace�� SMR�� �ִ� Dirty Page�� �����Ѵ�. 
    static IDE_RC discardDirtyPages4TBS( scSpaceID aSpaceID );

    //  discardDirtyPages4AllTBS �� ���� Action�Լ� 
    static IDE_RC discardDPAction( idvSQL            * aStatistics,
                                   sctTableSpaceNode * aTBSNode,
                                   void * aActionArg );
};

#endif // _O_SMR_DPLIST_MGR_
