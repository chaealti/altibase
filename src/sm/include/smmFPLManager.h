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
* $Id: smmFPLManager.h 82075 2018-01-17 06:39:52Z jina.kim $
**********************************************************************/

#ifndef _O_SMM_FPL_MANAGER_H_
#define _O_SMM_FPL_MANAGER_H_ 1

/*
  PROJ-1490 ����������Ʈ ����ȭ�� �޸𸮹ݳ�

  �������� ����ȭ�� �����ͺ��̽��� Free Page List ���� �����Ѵ�.
  FPL�� Free Page List�� �����̴�.
*/


// �������� ����ȭ�� Free Page�鿡 0���� ���������� 1�� �����ϸ� �ű� ��ȣ
typedef UInt smmFPLNo ;
#define SMM_NULL_FPL_NO ( ID_UINT_MAX )


/*
 * �������� �������� FreeListInfo Page���� Next Free Page�� ���� ������ ����Ʈ
 */
typedef struct smmPageList
{
    scPageID mHeadPID;      // ù��° Page
    scPageID mTailPID;      // ������ Page
    vULong   mPageCount;    // �� Page �� 
} smmPageList ;

// smmPageList�� �ʱ�ȭ�Ѵ�.
#define SMM_PAGELIST_INIT( list )     \
(list)->mHeadPID = SM_NULL_PID ;      \
(list)->mTailPID = SM_NULL_PID ;      \
(list)->mPageCount = 0 ;           

typedef enum smmFPLValidityOp
{
    SMM_FPL_VAL_OP_NORMAL = 1,
    SMM_FPL_VAL_OP_NO_COUNT_CHECK
} smmFPLValidityOp;

class smmFPLManager
{
private:

    // Free Page List�� �����Ѵ�.
    // Free Page List ���� ���� �α뵵 �Բ� �ǽ��Ѵ�.
    static IDE_RC logAndSetFreePageList( smmTBSNode * aTBSNode,
                                         void     *   aTrans,
                                         smmFPLNo     aFPLNo,
                                         scPageID     aFirstFreePageID,
                                         vULong       aFreePageCount );
    
    // Free Page List�� Free Page���� append�Ѵ�.
    static IDE_RC expandFreePageList( smmTBSNode * aTBSNode,
                                      void     *   aTrans,
                                      smmFPLNo     aFPLNo ,
                                      vULong       aRequiredFreePageCnt );

    //////////////////////////////////////////////////////////////////////////
    // Expand ChunkȮ�� ���� �Լ� 
    //////////////////////////////////////////////////////////////////////////
    // smmFPLManager::getTotalPageCount4AllTBS�� ���� Action�Լ�
    static IDE_RC aggregateTotalPageCountAction( idvSQL            * aStatistics, 
                                                 sctTableSpaceNode * aTBSNode,
                                                 void              * aActionArg  );

    // Tablespace�� NEXT ũ�⸸ŭ ChunkȮ���� �õ��Ѵ�.
    static IDE_RC tryToExpandNextSize(smmTBSNode * aTBSNode,
                                      void *       aTrans);

    // �ٸ� Ʈ����ǿ� ���ؼ��̴�,
    // aTrans���ؼ� �̴� ������� Tablespace�� NEXT ũ�⸸ŭ��
    // Chunk�� �������� �����Ѵ�.
    static IDE_RC expandOrWait(smmTBSNode * aTBSNode,
                               void *       aTrans);

    // ��� Tablespace�� ���� �Ҵ�� SIZE < MEM_MAX_DB_SIZE����
    // �˻��ϱ� ���� Tablespace�� Ȯ��� ��� Mutex
    static iduMutex mGlobalPageCountCheckMutex;
public :

    // Static Initializer
    static IDE_RC initializeStatic();

    // Static Destroyer
    static IDE_RC destroyStatic();
    
    // Free Page List �����ڸ� �ʱ�ȭ�Ѵ�.
    static IDE_RC initialize(smmTBSNode * aTBSNode);


    // Free Page List �����ڸ� �ı��Ѵ�.
    static IDE_RC destroy(smmTBSNode * aTBSNode);
    

    static IDE_RC lockGlobalPageCountCheckMutex();
    static IDE_RC unlockGlobalPageCountCheckMutex();
    
    
    
    // Free Page�� ���� ���� ���� �����ͺ��̽� Free Page List�� ã�Ƴ���.
    static IDE_RC getLargestFreePageList( smmTBSNode * aTBSNode,
                                          smmFPLNo *   aFPLNo );

    // �ϳ��� Free Page���� �����ͺ��̽� �������� Page List�� ���� �й��Ѵ�.
    static IDE_RC distributeFreePages( 
                        smmTBSNode *  aTBSNode,
                        scPageID      aChunkFirstFreePID,
                        scPageID      aChunkLastFreePID,
                        idBool        aSetNextFreePage, // PRJ-1548
                        UInt          aArrFreePageListCount,
                        smmPageList * aArrFreePageLists );

    // Chunk �Ҵ� Mutex�� Latch�� �Ǵ�.
    static IDE_RC lockAllocChunkMutex(smmTBSNode * aTBSNode);
    // Chunk �Ҵ� Mutex�κ��� Latch�� Ǭ��.
    static IDE_RC unlockAllocChunkMutex(smmTBSNode * aTBSNode);


    // ��� Tablepsace�� ���� OS�κ��� �Ҵ��� Page�� ���� ������ ��ȯ�Ѵ�
    static IDE_RC getTotalPageCount4AllTBS( scPageID * aTotalPageCount );
        
    // Free Page List �� ��ġ�� �Ǵ�.
    static IDE_RC lockFreePageList( smmTBSNode * aTBSNode,
                                    smmFPLNo     aFPLNo );
    
    // Free Page List �κ��� ��ġ�� Ǭ��.
    static IDE_RC unlockFreePageList( smmTBSNode * aTBSNode,
                                      smmFPLNo     aFPLNo );

    // ��� Free Page List�� latch�� ��´�.
    static IDE_RC lockAllFPLs(smmTBSNode * aTBSNode);
    
    // ��� Free Page List���� latch�� Ǭ��.
    static IDE_RC unlockAllFPLs(smmTBSNode * aTBSNode);
    
    /* BUG-31881 [sm-mem-resource] When executing alter table in MRDB and 
     * using space by other transaction,
     * The server can not do restart recovery. 
     * ���ݺ��� �� Transaction�� ��ȯ�ϴ� Page���� �����صд�. */
    static IDE_RC beginPageReservation( smmTBSNode * aTBSNode,
                                        void       * aTrans );

    /* Page������ �����. */
    static IDE_RC endPageReservation( smmTBSNode * aTBSNode,
                                      void       * aTrans );

    /* mArrFPLMutex�� ���� ���·� ȣ��Ǿ�� �� */
    /* �����ص� �������� ã�´�. */
    static IDE_RC findPageReservationSlot( 
        smmPageReservation  * aPageReservation,
        void                * aTrans,
        UInt                * aSlotNo );

    /* mArrFPLMutex�� ���� ���·� ȣ��Ǿ�� �� */
    /* �ڽ� �� �ٸ� Transaction���� �����ص� Page�� ������ ������ */
    static IDE_RC getUnusablePageCount( 
        smmPageReservation  * aPageReservation,
        void                * aTrans,
        UInt                * aUnusablePageCount );

    /* Ȥ�� �� Transaction�� ���õ� ���� �������� ������, ��� �����Ѵ�. */
    static IDE_RC finalizePageReservation( void      * aTrans,
                                           scSpaceID   aSpaceID );

    /* pageReservation ���� ������ dump �Ѵ�. */
    static IDE_RC dumpPageReservationByBuffer
        ( smmPageReservation * aPageReservation,
          SChar              * aOutBuf,
          UInt                 aOutSize );

    /* pageReservation ���� ������ SM TrcLog�� ����Ѵ�. */
    static void dumpPageReservation( smmPageReservation * aPageReservation );

    // Ư�� Free Page List�� Latch�� ȹ���� �� Ư�� ���� �̻���
    // Free Page�� ������ �����Ѵ�.
    static IDE_RC lockListAndPreparePages( smmTBSNode * aTBSNode,
                                           void       * aTrans,
                                           smmFPLNo     aFPLNo,
                                           vULong       aPageCount );

    
    // Free Page List ���� �ϳ��� Free Page�� �����.
    // aHeadPAge���� aTailPage����
    // Free List Info Page�� Next Free Page ID�� ����� ä�� �����Ѵ�.
    static IDE_RC removeFreePagesFromList( smmTBSNode * aTBSNode,
                                           void *       aTrans,
                                           smmFPLNo     aFPLNo,
                                           vULong       aPageCount,
                                           scPageID  *  aHeadPID,
                                           scPageID  *  aTailPID );

    // Free Page List �� �ϳ��� Free Page List�� ���δ�.
    // aHeadPage���� aTailPage����
    // Free List Info Page�� Next Free Page ID�� ����Ǿ� �־�� �Ѵ�..
    static IDE_RC appendFreePagesToList  ( 
                            smmTBSNode * aTBSNode,
                            void *       aTrans,
                            smmFPLNo     aFPLNo,
                            vULong       aPageCount,
                            scPageID     aHeadPID,
                            scPageID     aTailPID,
                            idBool       aSetFreeListOfMembase,
                            idBool       aSetNextFreePageOfFPL  );


    // Free Page ���� ������ Free Page List�� �� �տ� append�Ѵ�. 
    static IDE_RC appendPageLists2FPLs( 
                            smmTBSNode *  aTBSNode,
                            smmPageList * aArrFreePageList,
                            idBool        aSetFreeListOfMembase,
                            idBool        aSetNextFreePageOfFPL  );

    /* BUG-35443 Add Property for Excepting SYS_TBS_MEM_DIC size from
     * MEM_MAX_DB_SIZE
     */
    static IDE_RC getDicTBSPageCount( scPageID   * aTotalPageCount );

    /* BUG-35443 Add Property for Excepting SYS_TBS_MEM_DIC size from
     * MEM_MAX_DB_SIZE
     */
    static IDE_RC getTotalPageCountExceptDicTBS( 
                                      scPageID   * aTotalPageCount );

    // Free Page List�� ù��° Page ID�� Page���� validity�� üũ�Ѵ�.
    static idBool isValidFPL(scSpaceID        aSpaceID,
                             scPageID         aFirstPID,
                             vULong           aPageCount);

    // Free Page List�� ù��° Page ID�� Page���� validity�� üũ�Ѵ�.
    static idBool isValidFPL( scSpaceID           aSpaceID,
                              smmDBFreePageList * aFPL );

    // ��� Free Page List�� Valid���� üũ�Ѵ�
    static idBool isAllFPLsValid(smmTBSNode * aTBSNode);

    // ��� Free Page List�� ������ ��´�.
    static void dumpAllFPLs(smmTBSNode * aTBSNode);
};

// Chunk �Ҵ� Mutex�� Latch�� �Ǵ�.
inline IDE_RC smmFPLManager::lockAllocChunkMutex(smmTBSNode * aTBSNode)
{
    return aTBSNode->mAllocChunkMutex.lock( NULL );
}


// Chunk �Ҵ� Mutex�κ��� Latch�� Ǭ��.
inline IDE_RC smmFPLManager::unlockAllocChunkMutex(smmTBSNode * aTBSNode)
{
    return aTBSNode->mAllocChunkMutex.unlock();
}





#endif /* _O_SMM_FPL_MANAGER_H_ */
