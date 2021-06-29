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
 * $Id: smrDirtyPageList.h 90522 2021-04-09 01:29:20Z emlee $
 **********************************************************************/

#ifndef _O_SMR_DIRTY_PAGE_LIST_
#define _O_SMR_DIRTY_PAGE_LIST_ 1

#include <smm.h>
#include <smrDef.h>

#define MAX_PAGE_INFO ((SM_PAGE_SIZE - SMR_DEF_LOG_SIZE) / ID_SIZEOF(scGRID))

class smmDirtyPageMgr;

class smrDirtyPageList
{
    
public:
    
    IDE_RC initialize( scSpaceID aSpaceID );
    
    IDE_RC destroy();

    inline void  add( smmPCH  * aPCHPtr,
                      scPageID  aPageID );
    
    inline vULong getDirtyPageCnt() { return mDirtyPageCnt; }

    // Dirty Page ID���� �����ϰ� Page ID�� �����ͺ��� �α��Ѵ�.
    IDE_RC writePIDLogs();
    
    // Dirty Page���� Checkpoint Image�� Write�Ѵ�.
    IDE_RC writeDirtyPages(
                smmTBSNode                * aTBSNode,
                smmGetFlushTargetDBNoFunc   aGetFlushTargetDBNoFunc,
                idBool                      aIsFinalWrite,
                UInt                        aTotalCnt,
                UInt                      * aWriteCnt,
                UInt                      * aRemoveCnt,
                ULong                     * aWaitTime,
                ULong                     * aSyncTime );

    //  SMM Dirty Page Mgr�κ��� Dirty Page���� �����´�
    IDE_RC moveDirtyPagesFrom( smmDirtyPageMgr * aSmmDPMgr,
                               UInt            * aNewCnt,
                               UInt            * aDupCnt );

    void  removeAll( idBool aIsForce );
    
    smrDirtyPageList();

    virtual ~smrDirtyPageList();

    inline IDE_RC lock() { return mMutex.lock( NULL ); }
    inline IDE_RC unlock() { return mMutex.unlock(); };
    
private:
    // �ߺ��� PID�� ������ üũ�Ѵ�.
    idBool isAllPageUnique();
    
    inline void remove( smmPCH * aPCHPtr );


    static IDE_RC writePageNormal(smmTBSNode *     aTBSNode,
                                  smmDatabaseFile* aDBFilePtr, 
                                  scPageID         aPID); 

    // Page ID Array�� ��ϵ� Log Buffer�� Log Record�� ����Ѵ�.
    static IDE_RC writePIDLogRec(SChar * aLogBuffer,
                                 UInt    aDirtyPageCount);


    // Page Image�� Checkpoint Image�� ����Ѵ�.
   
    static IDE_RC writePageImage( smmTBSNode * aTBSNode,
                                  SInt         aWhichDB,
                                  scPageID     aPageID );
    
    // �� Dirty Page�����ڴ� �� Tablespace�� ���� Page�鸸 �����Ѵ�.
    scSpaceID   mSpaceID ;
    vULong            mMaxDirtyPageCnt;
    vULong            mDirtyPageCnt;
    smmPCH            mFstPCH;
    scGRID*           mArrPageGRID;

    iduCond           mCV;
    PDL_Time_Value    mTV;
    iduMutex          mMutex;

};

inline void  smrDirtyPageList::add( smmPCH    * aPCHPtr,
                                    scPageID    aPageID)
{
#if defined (DEBUG_SMR_DIRTY_PAGE_LIST_CHECK)
    UInt                 i;
#endif /* DEBUG_SMR_DIRTY_PAGE_LIST_CHECK */
    
    if(aPCHPtr->m_pnxtDirtyPCH == NULL)
    {
        /* �� �������� DirtyPageList�� ���� */
        /* �ߺ��� Dirty Page�� �����ϴ��� üũ�Ѵ�. */
#if defined(DEBUG_SMR_DIRTY_PAGE_LIST_CHECK )
        for( i = 0; i < mDirtyPageCnt; i++)
        {
            IDE_ASSERT((SC_MAKE_SPACE(mArrPageGID[i]) != aPCHPtr->mSpaceID) ||
                       (SC_MAKE_PID(mArrPageGID[i]) != aPageID) );
        }
#endif /* DEBUG_SMR_DIRTY_PAGE_LIST_CHECK */

        if( mDirtyPageCnt >= mMaxDirtyPageCnt )
        {
            scGRID * sTmpDPList;

            mMaxDirtyPageCnt = mMaxDirtyPageCnt * 2;
            IDE_ASSERT( iduMemMgr::malloc(IDU_MEM_SM_SMR,
                                          ID_SIZEOF(scGRID) * mMaxDirtyPageCnt,
                                          (void**)&sTmpDPList)
                        == IDE_SUCCESS );
            idlOS::memcpy(sTmpDPList, mArrPageGRID,
                          ID_SIZEOF(scGRID) * mDirtyPageCnt);
            IDE_ASSERT( iduMemMgr::free((void*)mArrPageGRID)
                        == IDE_SUCCESS );

            mArrPageGRID = sTmpDPList;
        }

        IDE_ASSERT(aPCHPtr->m_pprvDirtyPCH == NULL);

        IDE_ASSERT((aPCHPtr->m_dirtyStat & SMM_PCH_DIRTY_STAT_MASK)
                   == SMM_PCH_DIRTY_STAT_INIT);

        aPCHPtr->m_dirtyStat = SMM_PCH_DIRTY_STAT_FLUSH;

        aPCHPtr->m_pnxtDirtyPCH = &mFstPCH;
        aPCHPtr->m_pprvDirtyPCH = mFstPCH.m_pprvDirtyPCH;
        mFstPCH.m_pprvDirtyPCH->m_pnxtDirtyPCH = aPCHPtr;
        mFstPCH.m_pprvDirtyPCH = aPCHPtr;

        SC_MAKE_GRID(mArrPageGRID[mDirtyPageCnt],
                     aPCHPtr->mSpaceID,
                     aPageID,
                     (scOffset)0);
        
        mDirtyPageCnt++;
    }
    else
    {
        /* �� �������� �̹� DirtyPageList�� �� ���� */
        IDE_ASSERT((aPCHPtr->m_dirtyStat & SMM_PCH_DIRTY_STAT_MASK)
                   != SMM_PCH_DIRTY_STAT_INIT);

        aPCHPtr->m_dirtyStat = SMM_PCH_DIRTY_STAT_FLUSHDUP;
    }
    
    return;
}

inline void smrDirtyPageList::remove( smmPCH * aPCHPtr )
{
    smmPCH * sCurPCHPtr;

    sCurPCHPtr = aPCHPtr;

    IDE_ASSERT((sCurPCHPtr->m_dirtyStat & SMM_PCH_DIRTY_STAT_MASK)
               == SMM_PCH_DIRTY_STAT_REMOVE);

    sCurPCHPtr->m_dirtyStat = SMM_PCH_DIRTY_STAT_INIT;

    sCurPCHPtr->m_pnxtDirtyPCH->m_pprvDirtyPCH
        = sCurPCHPtr->m_pprvDirtyPCH;
    sCurPCHPtr->m_pprvDirtyPCH->m_pnxtDirtyPCH
        = sCurPCHPtr->m_pnxtDirtyPCH;
    sCurPCHPtr->m_pnxtDirtyPCH = NULL;
    sCurPCHPtr->m_pprvDirtyPCH = NULL;

    mDirtyPageCnt--;

    if (mDirtyPageCnt == 0)
    {
        IDE_ASSERT(mFstPCH.m_pnxtDirtyPCH == &mFstPCH);
        IDE_ASSERT(mFstPCH.m_pprvDirtyPCH == &mFstPCH);
    }

    return;
    
}

#endif /* _O_SMR_DIRTY_PAGE_LIST_ */
