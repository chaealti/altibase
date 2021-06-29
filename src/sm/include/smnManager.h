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
 * $Id: smnManager.h 89963 2021-02-09 05:22:10Z justin.kwon $
 **********************************************************************/

#ifndef _O_SMN_MANAGER_H_
# define _O_SMN_MANAGER_H_ 1

#include <smc.h>
#include <smnDef.h>
#include <smnbDef.h>
#include <smu.h>
#include <smx.h>


class smnManager
{
 public:
    static IDE_RC initialize( void );
    
    static IDE_RC destroy( void );

    static IDE_RC appendIndexType( smnIndexType * aIndexType );

    static IDE_RC updateIndexModule(smnIndexModule *aIndexModule);
    
    static IDE_RC allocFreeNodeList(smnFreeNodeFunc   aFreeNodeFunc,
                                    void**            aFreeNodeList);

    static void destroyFreeNodeList(void*  aFreeNodeList);
    
    static IDE_RC prepareIdxFreePages( );

    static IDE_RC releaseIdxFreePages( );

    static IDE_RC initializeJobInfo(smnpJobItem * aJobItem);

    static IDE_RC destroyJobInfo(smnpJobItem    * aJobItem);

    static IDE_RC removeAllIndexFile();

    static IDE_RC createIndex( idvSQL             * aStatistics,
                               void               * aTrans,
                               smcTableHeader     * aTable,
                               smnIndexHeader     * aIndex,
                               idBool               aIsRestartBuild,
                               idBool               aIsNeedValidation,
                               idBool               aIsPersistent,
                               smuJobThreadMgr    * aThreadMgr,
                               smiSegAttr         * aSegAttr,
                               smiSegStorageAttr  * aSegStoAttr, 
                               smnIndexHeader    ** aRebuildIndexHeader = NULL );
    
    static IDE_RC initIndex(idvSQL             * aStatistics,
                            smcTableHeader     * aTable,
                            void               * aIndex,
                            smiSegAttr         * aSegAttr,
                            smiSegStorageAttr  * aSegStoAttr,
                            void              ** aRebuildIndexHeader,
                            ULong                aSmoNo );
    
    static IDE_RC reInitIndex(idvSQL* aStatistics, void*    aIndex);
    
    static void setInitIndexPtr( void* aIndexHeader );

    /* BUG-17456 Disk Tablespace online���� update �߻��� index ���ѷ��� */
    static void setIndexSmoNo( void* aIndexHeader, ULong aSmoNo );

    // BUG-24403
    static void getMaxSmoNoOfAllIndexes( smcTableHeader * aTable,
                                         ULong          * aMaxSmoNo );

    static void getIndexSmoNo( smnIndexHeader * aIndexHeader,
                               ULong          * aSmoNo );

    static void releasePageLatch( smcTableHeader   * aTable,
                                  scPageID           aPageID );

    static IDE_RC getNextPageForMemTable( smcTableHeader  * aTable,
                                          scPageID        * aPageID,
                                          idBool          * aLocked );

    static IDE_RC getNextPageForVolTable( smcTableHeader  * aTable,
                                          scPageID        * aPageID,
                                          idBool          * aLocked );

    static IDE_RC getNextRowForMemTable( smcTableHeader   * aTable,
                                         scPageID           aPageID,
                                         SChar           ** aFence,
                                         SChar           ** aRow,
                                         idBool             aIsNeedValidation );

    static IDE_RC getNextRowForVolTable( smcTableHeader   * aTable,
                                         scPageID           aPageID,
                                         SChar           ** aFence,
                                         SChar           ** aRow,
                                         idBool             aIsNeedValidation );

    // BUG-25279 Btree For Spatial�� Disk Btree�� �ڷᱸ�� �� �α� �и�
    // Bug ���� �������� Index ���� ������ ���� Build Index�� �ϳ��� �����մϴ�.
    static IDE_RC buildIndex( idvSQL              * aStatistics,
                              void                * aTrans,
                              smcTableHeader      * aTable,
                              smnIndexHeader      * aIndex,
                              idBool                aIsNeedValidation,
                              idBool                aIsPersistent,
                              UInt                  aParallelDegree,
                              UInt                  aBuildFlag,
                              UInt                  aTotalRecCnt );

    static void setIndexCreated( void* aIndexHeader, idBool aIsCreated );

    static idBool getUniqueCheck(const void* aIndexHeader);

    static IDE_RC dropIndex( smcTableHeader * aTable,
                             void           * aIndexHeader );

    static IDE_RC rebuildIndexes( );
    static void   createIndexesParallel( void * aParam );

    static void   buildIndexParallel( void  * aParam );

    static IDE_RC destroyIndexes( void );
    
    static IDE_RC createIndexes( idvSQL            * aStatistics,
                                 void              * aTrans,
                                 smcTableHeader    * aTable,
                                 idBool              aIsRestartBuild,
                                 idBool              aIsNeedValidation,
                                 idBool              aIsPersistent,
                                 smiSegAttr        * aSegAttr,
                                 smiSegStorageAttr * aSegStoAttr,
                                 smuJobThreadMgr   * aThreadMgr );

    static IDE_RC createIndexes( idvSQL            * aStatistics,
                                 void              * aTrans,
                                 smcTableHeader    * aTable,
                                 idBool              aIsRestartBuild,
                                 idBool              aIsNeedValidation,
                                 smiSegAttr        * aSegAttr,
                                 smiSegStorageAttr * aSegStoAttr );

    static IDE_RC dropIndexes( smcTableHeader * aTable );

    static IDE_RC disableAllIndex( idvSQL          * aStatistics,
                                   void            * aTrans,
                                   smcTableHeader  * aTableHeader );

    static IDE_RC enableAllIndex( idvSQL          * aStatistics,
                                  void            * aTrans,
                                  smcTableHeader  * aTableHeader );

    static inline IDE_RC dropIndexRuntimeByAbort( smOID         aTableOID,
                                                  smOID         aIndexOID );

    static IDE_RC indexOperation( idvSQL*, void*, void*, void*, smSCN,
                                  SChar*, SChar*, idBool, smSCN, void*, SChar**, ULong, idBool );
    
    /*============================================================*/

    /* index module���� smp, svp�� ������� ����ϴ� �Լ� */
    static inline IDE_RC checkSCN( smiIterator *aIterator,
                                   const void  *aRow,
                                   idBool      *aCanReusableRollback,
                                   idBool      *aIsVisible,
                                   idBool       aIsFullScan = ID_FALSE );

    static inline idBool checkCanReusableRollback( smiIterator * aIterator,
                                                   const void  * aRow );

    static IDE_RC lockRow( smiIterator * aIterator);
    
    static void updatedRow( smiIterator * aIterator );
    
    /* smp�� ����ϴ� �Լ� */
    static IDE_RC lockMemRow(smiIterator *aIterator);

    static void updatedMemRow(smiIterator *aIterator);

    /* svp�� ����ϴ� �Լ� */
    static IDE_RC lockVolRow( smiIterator * aIterator);

    static void updatedVolRow( smiIterator * aIterator );

    /*============================================================*/

    static IDE_RC prepareRebuildIndexs(smcTableHeader  * aTable,
                                       smnIndexHeader ** aIndexHeaderList);

    static idBool isIndexToVerifyIntegrity( const void * aIndexHeader );

    static IDE_RC verifyIndexIntegrityAtStartUp();

    static SInt  getIndexModuleNo(UInt aTableType, UInt aIndexType);

    static void * getIndexModule(UInt aTableType, UInt aIndexType);

    static UInt * getFlagPtrOfIndexHeader( void   * a_pIndexHeader );

    static smOID  getTableOIDOfIndexHeader( void * a_pIndexHeader );

    static UInt  getIndexIDOfIndexHeader( void * a_pIndexHeader );

    static inline UInt getSizeOfIndexHeader( void );

    static IDE_RC indexInsertWithoutUniqueCheck( void   * a_pTrans,
                                                 void   * aTable,
                                                 void   * a_pIndexHeader,
                                                 SChar  * a_pRow,
                                                 SChar  * a_pNull );
    
    static IDE_RC indexDeleteFunc( void   * aIndexHeader,
                                   SChar  * aRow,
                                   idBool   aIgnoreNotFoundKey,
                                   idBool * aExistKey );
    
    static void   initIndexHeader( void                 * aIndexHeader,
                                   smOID                  aTableSelfOID, 
                                   SChar                * aName,
                                   UInt                   aID,
                                   UInt                   aType,
                                   UInt                   aFlag,
                                   const  smiColumnList * aColumns,
                                   smiSegAttr           * aSegAttr,
                                   smiSegStorageAttr    * aSegStoAttr,
                                   ULong                  aDirectKeyMaxSize );
    
    static IDE_RC initIndexMetaPage ( UChar            * aPagePtr,
                                      UInt               aType,
                                      UInt               aBuildFlag,
                                      sdrMtx           * aMtx );

    static UInt   getColumnCountOfIndexHeader ( void *a_pIndexHeader );
    
    static UInt*  getColumnIDPtrOfIndexHeader ( void *a_pIndexHeader, 
                                                UInt a_nIndex );
    
    static UInt  getFlagOfIndexHeader ( void *a_pIndexHeader );
    
    static void  setFlagOfIndexHeader ( void *a_pIndexHeader, UInt a_Flag );
    
    static idBool getIsConsistentOfIndexHeader ( void   * aIndexHeader );
    
    static void   setIsConsistentOfIndexHeader ( void   * aIndexHeader, 
                                                 idBool   aFlag );
    

    static IDE_RC deleteRowFromIndexForSVC( SChar          * aRow,
                                            smcTableHeader * aHeader,
                                            ULong            aModifyIdxBit );

    static IDE_RC deleteRowFromIndex( SChar          * aRow, 
                                      smcTableHeader * aHeader,
                                      ULong            aModifyIdxBit );
    
    static idBool isNullModuleOfIndexHeader(void *a_pIndexHeader);

    static scSpaceID getIndexSpaceID( void * aIndexHeader );

    static inline scGRID * getIndexSegGRIDPtr( void * aIndexHeader );
    static UInt * getIndexFlagPtr( void * aIndexHeader );
    static UShort getIndexDropFlag( void * aIndexHeader );
    static smiSegStorageAttr * getIndexSegStoAttrPtr( void * aIndexHeader );
    static smiSegAttr * getIndexSegAttrPtr( void * aIndexHeader );
    static void  setIndexSegStoAttrPtr( void * aIndexHeader, smiSegStorageAttr aSegStoAttr );
    static void  setIndexSegAttrPtr( void * aIndexHeader, smiSegAttr aSegAttr );
    

    static SChar*  getNameOfIndexHeader(void *aIndexHeader);
    static void    setNameOfIndexHeader(void *aIndexHeader, SChar *aName);
    static UInt    getIndexNameOffset();

    static idBool checkIdxIntersectCols( void* aIndexHeader,
                                         UInt   aColumnCount,
                                         UInt * aColumns );
    static  UInt  getMaxIndexCnt();
    
    
    static idBool isPrimaryIndex(void* aIndexHeader);

    static inline idBool isIndexEnabled( void* aIndexHeader );

    static void * getSegDescByIdxPtr( void * aIndex );

    /* BUG-31845 [sm-disk-index] Debugging information is needed for 
     * PBT when fail to check visibility using DRDB Index.
     * ������ Dump �ڵ� �߰� */
    static IDE_RC dumpCommonHeader( smnIndexHeader * aHeader,
                                    SChar          * aOutBuf,
                                    UInt             aOutSize );

    static void   logCommonHeader( smnIndexHeader * aHeader );

    static IDE_RC initIndexStatistics( smnIndexHeader   * aIndexHeader,
                                       smnRuntimeHeader * aRunHeader,
                                       UInt               aIndexID );
    static IDE_RC destIndexStatistics( smnIndexHeader   * aIndexHeader,
                                       smnRuntimeHeader * aRunHeader );
public:
    
    // added for A4. free list of memory index nodes(B,R Tree) 
    static smnFreeNodeList  mBaseFreeNodeList; 

    static iduMemPool       mDiskTempPagePool;

    static IDE_RC doKeyReorganization( smnIndexHeader * aIndexHeader );

private:
    static idBool mIsInvalidUniqueness;  // BUG-47736
};

inline idBool smnManager::isIndexEnabled( void* aIndexHeader )
{
    smnIndexHeader *sIndexHeader = (smnIndexHeader*)aIndexHeader;

    return ( (sIndexHeader->mFlag & SMI_INDEX_USE_MASK) == SMI_INDEX_USE_ENABLE ) ?
        ID_TRUE : ID_FALSE;
}

/*******************************************************************************
 * Description: Enable index ���� abort�� �߻����� ��, �̹� ������ index��
 *      runtime header ���� �ı��� �ִ� �Լ��̴�.
 *
 *      Disk index�� index segment�� �� �Լ����� �ı����� �ʾƵ� NTA logical
 *      rollback ���� �ı��ȴ�.
 *
 * Related Issues:
 *      PROJ-2184 RP Sync ���� ���
 * 
 * aStatistics      - [IN] idvSQL
 * aTableOID        - [IN] ��� index�� ���� table�� OID
 * aIndexOID        - [IN] ��� index�� OID
 ******************************************************************************/
inline IDE_RC smnManager::dropIndexRuntimeByAbort( smOID        aTableOID,
                                                   smOID        aIndexOID )
{
    smcTableHeader  * sTableHeader;
    smnIndexHeader  * sIndexHeader;

    IDE_ASSERT( smcTable::getTableHeaderFromOID( aTableOID,
                                                 (void**)&sTableHeader )
                == IDE_SUCCESS );

    IDE_ASSERT( smcTable::getIndexHeaderFromOID( aIndexOID,
                                                 (void**)&sIndexHeader )
                == IDE_SUCCESS );

    IDE_TEST( dropIndex( sTableHeader,
                         sIndexHeader )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/* PROJ-1594 Volatile TBS
 * SCAN ��⿡�� scn�� üũ�� ��, memory, volatile table�� ����
 * �������� ����� �� �ִ� �Լ��̴�.
 *
 * PROJ-1381 Fetch Across Commits
 * Row SCN     : sCreateSCN, sLimitSCN
 * SCN from TX : sRowSCN,    sNxtSCN
 *
 *          Row SCN  | SCN from TX | TransID 
 * case 1   INFINITE | INFINITE    | VALID TID
 * case 2   INFINITE | COMMITED    | VALID TID
 * case 3   COMMITED | COMMITED    | NULL  TID
 *
 * case 2�� TX�� commit ������ ���� commit SCN��
 * Row�� �������� ���� �����̴�.
 */
inline IDE_RC smnManager::checkSCN( smiIterator *aIterator,
                                    const void  *aRow,
                                    idBool      *aCanReusableRollback,
                                    idBool      *aIsVisible,
                                    idBool       aIsFullScan ) /* Default : ID_FALSE */
{
    smSCN     sCreateSCN;
    smSCN     sLimitSCN;
    smSCN     sCreateSCNTmp;
    smSCN     sLimitSCNTmp;
    smSCN     sRowSCN;
    smSCN     sNxtSCN;
    smTID     sRowTID = SM_NULL_TID;
    smTID     sNxtTID = SM_NULL_TID;
#ifdef DEBUG
    smSCN     sRowSCNAfterCheck;
#endif
    smxTrans               * sTrans = (smxTrans*)aIterator->trans;
    volatile smpSlotHeader * sSlotHeader = NULL;

    sSlotHeader = (smpSlotHeader*)aRow;

    if( aCanReusableRollback != NULL )
    {
        *aCanReusableRollback = ID_TRUE;
    }
    *aIsVisible = ID_FALSE;

    if ( aIsFullScan == ID_FALSE )
    {
        SM_SET_SCN( &sCreateSCN, &(sSlotHeader->mCreateSCN) );
        SM_SET_SCN( &sLimitSCN, &(sSlotHeader->mLimitSCN) );
    }
    else
    {
        /* BUG-48353
           FULL SCAN �ΰ�� PAGE LATCH �ۿ��� checkSCN() ȣ���Ѵ�.
           CreateSCN�� LimitSCN�� ������ Record�κ��� �޾ƿ� �������� �����ؾ� �Ѵ�. */
        do
        {
            ID_SERIAL_BEGIN( SM_SET_SCN( &sCreateSCN, &(sSlotHeader->mCreateSCN) ) );
            ID_SERIAL_EXEC(  SM_SET_SCN( &sLimitSCN, &(sSlotHeader->mLimitSCN) ), 1 );
            ID_SERIAL_EXEC(  SM_SET_SCN( &sCreateSCNTmp, &(sSlotHeader->mCreateSCN) ), 2 );
            ID_SERIAL_END(   SM_SET_SCN( &sLimitSCNTmp, &(sSlotHeader->mLimitSCN) ) );

        } while ( !SM_SCN_IS_EQ( &sCreateSCN, &sCreateSCNTmp ) ||
                  !SM_SCN_IS_EQ( &sLimitSCN, &sLimitSCNTmp ) );
    }

    if ( sTrans->mIsGCTx == ID_TRUE )
    {
        if ( SM_SCN_IS_INFINITE( sCreateSCN ) &&
             /* BUG-48244 : ��(TX)�� 2PC�� TX�� Pending �Ϸ�ñ��� ����� �ʿ����. */
             ( SMP_GET_TID( sCreateSCN ) != aIterator->tid ) )
        {
            IDE_TEST( smxTrans::waitPendingTx( sTrans,
                                               sCreateSCN,
                                               aIterator->SCN )
                      != IDE_SUCCESS );
        }

        if ( SM_SCN_IS_INFINITE( sLimitSCN ) &&
             /* BUG-48244 : ��(TX)�� 2PC�� TX�� Pending �Ϸ�ñ��� ����� �ʿ����. */
             ( SMP_GET_TID( sLimitSCN ) != aIterator->tid ) )
        {
            IDE_TEST( smxTrans::waitPendingTx( sTrans,
                                               sLimitSCN,
                                               aIterator->SCN )
                      != IDE_SUCCESS );
        }

        /* BUG-48244 : waitPendingTX()���ο��� COMMIT�� ���Ŀ� RECORD SCN�� �ٽ� �޾ƿ´�.
                       GCTX���� ���� view�� ���°��, RECORD�� �Ⱥ��ϼ��� �ֱ� �����̴�. */ 
        if ( aIsFullScan == ID_FALSE )
        {
            SM_SET_SCN( &sCreateSCN, &(sSlotHeader->mCreateSCN) );
            SM_SET_SCN( &sLimitSCN, &(sSlotHeader->mLimitSCN) );
        }
        else
        {
            /* BUG-48353
               FULL SCAN �ΰ�� PAGE LATCH �ۿ��� checkSCN() ȣ���Ѵ�.
               CreateSCN�� LimitSCN�� ������ Record�κ��� �޾ƿ� �������� �����ؾ� �Ѵ�. */
            do
            {
                ID_SERIAL_BEGIN( SM_SET_SCN( &sCreateSCN, &(sSlotHeader->mCreateSCN) ) );
                ID_SERIAL_EXEC(  SM_SET_SCN( &sLimitSCN, &(sSlotHeader->mLimitSCN) ), 1 );
                ID_SERIAL_EXEC(  SM_SET_SCN( &sCreateSCNTmp, &(sSlotHeader->mCreateSCN) ), 2 );
                ID_SERIAL_END(   SM_SET_SCN( &sLimitSCNTmp, &(sSlotHeader->mLimitSCN) ) );

            } while ( !SM_SCN_IS_EQ( &sCreateSCN, &sCreateSCNTmp ) ||
                      !SM_SCN_IS_EQ( &sLimitSCN, &sLimitSCNTmp ) );
        }
    }

    SMX_GET_SCN_AND_TID( sCreateSCN, sRowSCN, sRowTID );
    SMX_GET_SCN_AND_TID( sLimitSCN, sNxtSCN, sNxtTID );

    while ( 1 )
    {
        /* delete bit�� ������ ��� �ش� ROW�� ���� �ʴ´�. */
        IDE_TEST_CONT( SM_SCN_IS_DELETED( sRowSCN ), skip_check_visibility );

        /* ���� �� �ִ� ��� SCN�� ��� */
        if ( /* row�� SCN�� cursor�� view SCN���� �۰ų� */
             ( SM_SCN_IS_LE( &sRowSCN, &(aIterator->SCN) ) ) ||
             /* ���� Tx���� cursor�� ������ ���� ������ row�̸� */
             ( ( sRowTID == aIterator->tid ) &&
               ( SM_SCN_IS_LT( &sCreateSCN, &(aIterator->infinite) ) ) ) )
        {
            /* row�� lock�� �ɾ��ų� next version�� ������ �� �� �ִ�. */
            if ( SM_SCN_IS_FREE_ROW( sNxtSCN ) ||
                 SM_SCN_IS_LOCK_ROW( sNxtSCN ) )
            {
                /* PROJ-2694 Fetch Across Rollback
                 * holdable cursor open ���� �ڽ��� ������ row�� �������� ���
                 * �ش� Tx�� rollback�� cursor�� ��Ȱ���� �� ����. */
                if ( ( sRowTID == aIterator->tid ) && 
                     ( sTrans->mCursorOpenInfSCN != SM_SCN_INIT ) &&
                     ( SM_SCN_IS_LT( &sCreateSCN, &( sTrans->mCursorOpenInfSCN ) ) ) &&
                     ( aCanReusableRollback != NULL ) )
                {
                    *aCanReusableRollback = ID_FALSE;
                }
                else
                {
                    /* nothing to do */
                }

                *aIsVisible = ID_TRUE;
                break;
            }
            else
            {
                /* nothing to do */
            }

            if ( sNxtTID == aIterator->tid )
            {
                /* Next Version�� ���� ��������� cursor�� �������� ������� */
                if ( SM_SCN_IS_GE( &(sLimitSCN), &(aIterator->infinite) ) )
                {

                    if( ( sTrans->mCursorOpenInfSCN != SM_SCN_INIT ) && 
                        ( SM_SCN_IS_GE( &sLimitSCN, &sTrans->mCursorOpenInfSCN ) ) &&
                        ( aCanReusableRollback != NULL ) )
                    {
                        /* PROJ-2694 Fetch Across Rollback 
                         * holdable cursor open ���� �ڽ��� ������ row�� �������� ���
                         * �ش� Tx�� rollback�� cursor�� ��Ȱ���� �� ����. */
                        *aCanReusableRollback = ID_FALSE;
                    }

                    *aIsVisible = ID_TRUE;
                    break;
                }
                else
                {
                    /* nothing to do */
                }
            }
            else
            {
                /* Next Version�� ���� ������ ���� ���
                 * �Ʒ� 3���� ��쿡 ���� ��� �˻簡 �����ϴ�.
                 * 1. Commit ���� �ʾҰ�, Next Version�� SCN�� infinite�� ���
                 * 2. Commit ������ ���� Commit SCN�� �������� ���� ���
                 * 3. Commit �߰�, Commit SCN�� ������ ���
                 *    (1   : sNxtSCN�� INFINITE SCN�̹Ƿ� �� �� �ִ�. )
                 *    (2,3 : sNxtSCN�� COMMITED SCN�̹Ƿ� ��� �� ���� ) */
                if ( SM_SCN_IS_GT( &(sNxtSCN), &(aIterator->SCN) ) )
                {
                    *aIsVisible = ID_TRUE;
                    break;
                }
                else
                {
                    /* nothing to do */
                }
            }
        }
        else
        {
            /* nothing to do */
        }

        break;
    }

#ifdef DEBUG
    if ( *aIsVisible == ID_TRUE )
    {
        /* Next Row�� Free�ϸ� Next Row�� SCN�� 0x7FFFFFFFFFFFFFFD�� �ȴ�.
         * ���� ���� Row�� �� �� �ִٰ� �Ǵ��Ѵ�.
         * Next Row�� �����Ǹ� ���� Row�� ������ Row�� �� �ִ�.
         * �׷��� �̷� ���� ���׿� ���� ���̴�.
         * �ֳ��ϸ� Check�ϰ� �ִ� Row�� ���� Update�� �����߱� �����̴�. */
        SMX_GET_SCN_AND_TID( ((smpSlotHeader*)sSlotHeader)->mCreateSCN,
                             sRowSCNAfterCheck,
                             sRowTID );
        if ( !SM_SCN_IS_EQ( &sRowSCN, &sRowSCNAfterCheck ) )
        {
            ideLog::log(SM_TRC_LOG_LEVEL_WARNNING,
                        "[##WARNING ##WARNING!!] Invalid row detected\n");

            ideLog::log(SM_TRC_LOG_LEVEL_WARNNING,
                        "TID:[%"ID_UINT32_FMT"] View SCN:[%"ID_UINT64_FMT"] Inf SCN:[%"ID_UINT64_FMT"]\n"
                        "Row SCN:[%"ID_UINT64_FMT"]  RowSCNAfterCheck:[%"ID_UINT64_FMT"]\n",
                        (UInt)( aIterator->tid ),
                        SM_SCN_TO_LONG( aIterator->SCN ),
                        SM_SCN_TO_LONG( aIterator->infinite ),
                        SM_SCN_TO_LONG( sRowSCN ),
                        SM_SCN_TO_LONG( sRowSCNAfterCheck ) );

            smcRecord::logSlotInfo((smpSlotHeader*)sSlotHeader);

            IDE_ASSERT(0);
        }
        else
        {
            /* nothing to do */
        }
    }
    else
    {
        /* nothing to do */
    }
#endif

    /* BUG-40385 *aIsVisible ���� ���� Failure ������ �� �����Ƿ�,
     * ���� IDE_TEST_RAISE -> IDE_TEST_CONT �� ��ȯ���� �ʴ´�. */
    IDE_EXCEPTION_CONT( skip_check_visibility );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

inline UInt smnManager::getSizeOfIndexHeader( void )
{
    return ID_SIZEOF( smnIndexHeader );
}

inline scGRID * smnManager::getIndexSegGRIDPtr( void * aIndexHeader )
{
    return ( &((smnIndexHeader *)aIndexHeader)->mIndexSegDesc );
}

inline idBool smnManager::checkCanReusableRollback( smiIterator * aIterator,
                                                    const void  * aRow )
{
    smSCN     sCreateSCN;
    smTID     sRowTID = SM_NULL_TID;
    smSCN     sRowSCN;
    idBool    sIsReusableRollback = ID_TRUE;
    smxTrans *sTrans = (smxTrans*)aIterator->trans;

    const   smpSlotHeader   * sSlotHeader = NULL;

    IDE_DASSERT( aIterator != NULL );
    IDE_DASSERT( aRow != NULL );

    sSlotHeader = (smpSlotHeader*)aRow;

    SM_SET_SCN( &sCreateSCN, &(sSlotHeader->mCreateSCN) );

    SMX_GET_SCN_AND_TID( sCreateSCN, sRowSCN, sRowTID );
    if( SM_SCN_IS_DELETED( sRowSCN ) == ID_TRUE )
    {
        /* �ش� row�� �̹� �����Ǿ��ٸ� fetch across rollback�� ���谡 ����. */
        sIsReusableRollback = ID_TRUE;
    }
    else
    {
        if( sRowTID != aIterator->tid )
        {
            /* �ٸ� Tx�� ���뿡 ������ ��� fetch across rollback�� ���谡 ����. */
            sIsReusableRollback = ID_TRUE;
        }
        else
        {
            if( ( SM_SCN_IS_LT( &sCreateSCN, &( sTrans->mCursorOpenInfSCN ) ) == ID_TRUE ) && 
                ( sTrans->mCursorOpenInfSCN != SM_SCN_INIT ) )
            {
                /* ������ row�� holdable cursor�� ������ �� ������ row�� ��� fetch across rollback�� �Ұ����ϴ�.     */
                sIsReusableRollback = ID_FALSE;
            }
            else
            {
                /* non holdable cursor�̰ų� holdable cursor�� ���� �� ������ row�� ��� fetch across rollback�� ���谡 ����. */
                sIsReusableRollback = ID_TRUE;
            }
        }
    }

    return sIsReusableRollback;
}


#endif /* _O_SMN_MANAGER_H_ */
