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
 
#ifndef _O_SVM_FPL_MANAGER_H_
#define _O_SVM_FPL_MANAGER_H_ 1

#include <svm.h>

/*
  PROJ-1490 ����������Ʈ ����ȭ�� �޸𸮹ݳ�

  �������� ����ȭ�� �����ͺ��̽��� Free Page List ���� �����Ѵ�.
  FPL�� Free Page List�� �����̴�.
*/


// �������� ����ȭ�� Free Page�鿡 0���� ���������� 1�� �����ϸ� �ű� ��ȣ
typedef UInt svmFPLNo ;
#define SVM_NULL_FPL_NO ( ID_UINT_MAX )


/*
 * �������� �������� FreeListInfo Page���� Next Free Page�� ���� ������ ����Ʈ
 */
typedef struct svmPageList
{
    scPageID mHeadPID;      // ù��° Page
    scPageID mTailPID;      // ������ Page
    vULong   mPageCount;    // �� Page �� 
} svmPageList ;

// svmPageList�� �ʱ�ȭ�Ѵ�.
#define SVM_PAGELIST_INIT( list )     \
(list)->mHeadPID = SM_NULL_PID ;      \
(list)->mTailPID = SM_NULL_PID ;      \
(list)->mPageCount = 0 ;           

typedef enum svmFPLValidityOp
{
    SVM_FPL_VAL_OP_NORMAL = 1,
    SVM_FPL_VAL_OP_NO_COUNT_CHECK
} svmFPLValidityOp;



class svmFPLManager
{
private:

    // Free Page List�� �����Ѵ�.
    static IDE_RC setFreePageList( svmTBSNode * aTBSNode,
                                   svmFPLNo     aFPLNo,
                                   scPageID     aFirstFreePageID,
                                  vULong       aFreePageCount );
    
    // Free Page List�� Free Page���� append�Ѵ�.
    static IDE_RC expandFreePageList( svmTBSNode * aTBSNode,
                                      void       * aTrans,
                                      svmFPLNo     aFPLNo ,
                                      vULong       aRequiredFreePageCnt );

    //////////////////////////////////////////////////////////////////////////
    // Expand ChunkȮ�� ���� �Լ� 
    //////////////////////////////////////////////////////////////////////////
    // svmFPLManager::getTotalPageCount4AllTBS�� ���� Action�Լ�
    static IDE_RC aggregateTotalPageCountAction( idvSQL*  aStatistics,
                                                 sctTableSpaceNode * aTBSNode,
                                                 void * aActionArg  );
    

    // Tablespace�� NEXT ũ�⸸ŭ ChunkȮ���� �õ��Ѵ�.
    static IDE_RC tryToExpandNextSize(svmTBSNode * aTBSNode);

    // �ٸ� Ʈ����ǿ� ���ؼ��̴�,
    // aTrans���ؼ� �̴� ������� Tablespace�� NEXT ũ�⸸ŭ��
    // Chunk�� �������� �����Ѵ�.
    static IDE_RC expandOrWait(svmTBSNode * aTBSNode);

    // ��� Tablespace�� ���� �Ҵ�� SIZE < VOLATILE_MAX_DB_SIZE����
    // �˻��ϱ� ���� Tablespace�� Ȯ��� ��� Mutex
    static iduMutex mGlobalAllocChunkMutex;
    
public :

    // Static Initializer
    static IDE_RC initializeStatic();

    // Static Destroyer
    static IDE_RC destroyStatic();
    
    // Free Page List �����ڸ� �ʱ�ȭ�Ѵ�.
    static IDE_RC initialize(svmTBSNode * aTBSNode);


    // Free Page List �����ڸ� �ı��Ѵ�.
    static IDE_RC destroy(svmTBSNode * aTBSNode);
    
    //  Create Volatile Tablespace�� DML�� ���� ChunkȮ�� �߻��� Mutex���
    static IDE_RC lockGlobalAllocChunkMutex();
    
    // Create Volatile Tablespace�� DML�� ���� ChunkȮ�� �߻��� MutexǮ�� 
    static IDE_RC unlockGlobalAllocChunkMutex();

    // ��� Tablepsace�� ���� OS�κ��� �Ҵ��� Page�� ���� ������ ��ȯ�Ѵ�.
    static IDE_RC getTotalPageCount4AllTBS( scPageID * aTotalPageCount );
    
    // Free Page�� ���� ���� ���� �����ͺ��̽� Free Page List�� ã�Ƴ���.
    static IDE_RC getShortestFreePageList( svmTBSNode * aTBSNode,
                                           svmFPLNo *   aFPLNo );

    // Free Page�� ���� ���� ���� �����ͺ��̽� Free Page List�� ã�Ƴ���.
    static IDE_RC getLargestFreePageList( svmTBSNode * aTBSNode,
                                          svmFPLNo *   aFPLNo );

    // �ϳ��� Free Page���� �����ͺ��̽� �������� Page List�� ���� �й��Ѵ�.
    static IDE_RC distributeFreePages( svmTBSNode * aTBSNode,
                                       scPageID      aChunkFirstFreePID,
                                       scPageID      aChunkLastFreePID,
                                       idBool        aSetNextFreePage,
                                       svmPageList * aArrFreePageLists );

    // Chunk �Ҵ� Mutex�� Latch�� �Ǵ�.
    static IDE_RC lockAllocChunkMutex(svmTBSNode * aTBSNode);
    // Chunk �Ҵ� Mutex�κ��� Latch�� Ǭ��.
    static IDE_RC unlockAllocChunkMutex(svmTBSNode * aTBSNode);
    
    // Free Page List �� ��ġ�� �Ǵ�.
    static IDE_RC lockFreePageList( svmTBSNode * aTBSNode,
                                    svmFPLNo     aFPLNo );
    
    // Free Page List �κ��� ��ġ�� Ǭ��.
    static IDE_RC unlockFreePageList( svmTBSNode * aTBSNode,
                                      svmFPLNo     aFPLNo );

    // ��� Free Page List�� latch�� ��´�.
    static IDE_RC lockAllFPLs(svmTBSNode * aTBSNode);
    
    // ��� Free Page List���� latch�� Ǭ��.
    static IDE_RC unlockAllFPLs(svmTBSNode * aTBSNode);
    
    /* BUG-31881 [sm-mem-resource] When executing alter table in MRDB and 
     * using space by other transaction,
     * The server can not do restart recovery. 
     * ���ݺ��� �� Transaction�� ��ȯ�ϴ� Page���� �����صд�. */
    static IDE_RC beginPageReservation( svmTBSNode * aTBSNode,
                                        void       * aTrans );

    /* Page������ �����. */
    static IDE_RC endPageReservation( svmTBSNode * aTBSNode,
                                      void       * aTrans );

    /* mArrFPLMutex�� ���� ���·� ȣ��Ǿ�� �� */
    /* �����ص� �������� ã�´�. */
    static IDE_RC findPageReservationSlot( 
        svmPageReservation  * aPageReservation,
        void                * aTrans,
        UInt                * aSlotNo );

    /* mArrFPLMutex�� ���� ���·� ȣ��Ǿ�� �� */
    /* �ڽ� �� �ٸ� Transaction���� �����ص� Page�� ������ ������ */
    static IDE_RC getUnusablePageCount( 
        svmPageReservation  * aPageReservation,
        void                * aTrans,
        UInt                * aUnusablePageCount );

    /* Ȥ�� �� Transaction�� ���õ� ���� �������� ������, ��� �����Ѵ�. */
    static IDE_RC finalizePageReservation( void      * aTrans,
                                           scSpaceID   aSpaceID );

    /* pageReservation ���� ������ dump �Ѵ�. */
    static IDE_RC dumpPageReservationByBuffer
        ( svmPageReservation * aPageReservation,
          SChar              * aOutBuf,
          UInt                 aOutSize );

    /* pageReservation ���� ������ SM TrcLog�� ����Ѵ�. */
    static void dumpPageReservation( svmPageReservation * aPageReservation );

    // Ư�� Free Page List�� Latch�� ȹ���� �� Ư�� ���� �̻���
    // Free Page�� ������ �����Ѵ�.
    static IDE_RC lockListAndPreparePages( svmTBSNode * aTBSNode,
                                           void       * aTrans,
                                           svmFPLNo     aFPLNo,
                                           vULong       aPageCount );

    
    // Free Page List ���� �ϳ��� Free Page�� �����.
    // aHeadPAge���� aTailPage����
    // Free List Info Page�� Next Free Page ID�� ����� ä�� �����Ѵ�.
    static IDE_RC removeFreePagesFromList( svmTBSNode * aTBSNode,
                                           void *       aTrans,
                                           svmFPLNo     aFPLNo,
                                           vULong       aPageCount,
                                           scPageID  *  aHeadPID,
                                           scPageID  *  aTailPID );

    // Free Page List �� �ϳ��� Free Page List�� ���δ�.
    // aHeadPage���� aTailPage����
    // Free List Info Page�� Next Free Page ID�� ����Ǿ� �־�� �Ѵ�..
    static IDE_RC appendFreePagesToList( svmTBSNode * aTBSNode,
                                         void *       aTrans,
                                         svmFPLNo     aFPLNo,
                                         vULong       aPageCount,
                                         scPageID     aHeadPID,
                                         scPageID     aTailPID );
    

    // Free Page ���� ������ Free Page List�� �� �տ� append�Ѵ�. 
    static IDE_RC appendPageLists2FPLs( svmTBSNode *  aTBSNode,
                                        svmPageList * aArrFreePageList );
    
    // ��� Free Page List�� �ִ� Free Page ���� �����Ѵ�.
    static IDE_RC getTotalFreePageCount( svmTBSNode * aTBSNode,
                                         vULong *     aTotalFreePageCnt );

    // Free Page List�� ù��° Page ID�� Page���� validity�� üũ�Ѵ�.
    static idBool isValidFPL(scSpaceID        aSpaceID,
                             scPageID         aFirstPID,
                             vULong           aPageCount);

    // Free Page List�� ù��° Page ID�� Page���� validity�� üũ�Ѵ�.
    static idBool isValidFPL( scSpaceID           aSpaceID,
                              svmDBFreePageList * aFPL );

    // ��� Free Page List�� Valid���� üũ�Ѵ�
    static idBool isAllFPLsValid(svmTBSNode * aTBSNode);

    // ��� Free Page List�� ������ ��´�.
    static void dumpAllFPLs(svmTBSNode * aTBSNode);
};

// Chunk �Ҵ� Mutex�� Latch�� �Ǵ�.
inline IDE_RC svmFPLManager::lockAllocChunkMutex(svmTBSNode * aTBSNode)
{
    return aTBSNode->mAllocChunkMutex.lock(NULL);
}


// Chunk �Ҵ� Mutex�κ��� Latch�� Ǭ��.
inline IDE_RC svmFPLManager::unlockAllocChunkMutex(svmTBSNode * aTBSNode)
{
    return aTBSNode->mAllocChunkMutex.unlock();
}





#endif /* _O_SVM_FPL_MANAGER_H_ */
