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
 * $Id: smmDirtyPageMgr.h 82075 2018-01-17 06:39:52Z jina.kim $
 **********************************************************************/

#ifndef _O_SMM_DIRTY_PAGE_MGR_H_
#define _O_SMM_DIRTY_PAGE_MGR_H_ 1

#include <idl.h>
#include <idu.h>
#include <iduHash.h>
#include <smmDef.h>
#include <smmDirtyPageList.h>

class smmDirtyPageMgr
{
private:
    // �� Dirty Page Mgr��  �� Tablespace�� ���� Dirty Page���� �����Ѵ�.
    scSpaceID mSpaceID ; 
    
    SInt m_listCount; // list ����
    smmDirtyPageList *m_list;
    
    IDE_RC lockDirtyPageList(smmDirtyPageList **a_list);
    
public:
    IDE_RC initialize(scSpaceID aSpaceID, SInt a_listCount);
    IDE_RC destroy();

    // ��� Dirty Page List���� Dirty Page���� �����Ѵ�.
    IDE_RC removeAllDirtyPages();
    
    // Dirty Page ����Ʈ�� ����� Page�� �߰��Ѵ�.
    IDE_RC insDirtyPage(scPageID aPageID ); 
    
    smmDirtyPageList* getDirtyPageList(SInt a_num)
    {
        IDE_ASSERT(a_num < m_listCount);
        return &m_list[a_num];
    }

    SInt  getDirtyPageListCount() { return m_listCount; }

    
   /***********************************************************************
      ���� Tablespace�� ���� Dirty Page������ �����ϴ� �������̽�
      Refactoring�� ���� ���� ������ Class�� ������ �Ѵ�.
    ***********************************************************************/
public :
    // Dirty Page ����Ʈ�� ����� Page�� �߰��Ѵ�.
    static IDE_RC insDirtyPage(scSpaceID aSpaceID, scPageID aPageID );
    static IDE_RC insDirtyPage(scSpaceID aSpaceID, void *   a_new_page);
    
    // Dirty Page�����ڸ� �����Ѵ�.
    static IDE_RC initializeStatic();

    // Dirty Page�����ڸ� �ı��Ѵ�.
    static IDE_RC destroyStatic();

    // Ư�� Tablespace�� ���� Dirty Page�����ڸ� �����Ѵ�.
    static IDE_RC createDPMgr(smmTBSNode * aTBSNode );
    
    // Ư�� Tablespace�� ���� Dirty Page�����ڸ� ã�Ƴ���.
    static IDE_RC findDPMgr( scSpaceID aSpaceID, smmDirtyPageMgr ** aDPMgr );
    
    // Ư�� Tablespace�� Dirty Page�����ڸ� �����Ѵ�.
    static IDE_RC removeDPMgr(smmTBSNode * aTBSNode);
};

#endif
