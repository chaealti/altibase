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
 
#ifndef _O_SGM_MANAGER_H_
#define _O_SGM_MANAGER_H_ 1

#include <smDef.h>
#include <sctTableSpaceMgr.h>
#include <smmManager.h>
#include <svmManager.h>

#if 0 //not used
#define SGM_GET_NEXT_SCN_AND_TID( aSpaceID, aHeaderNext, aTID,                \
                                  aNextSCN, aNextTID,    aTmpSlotHeader ) {   \
    if( sctTableSpaceMgr::isMemTableSpace(aSpaceID) == ID_TRUE )              \
    {                                                                         \
        SMP_GET_NEXT_SCN_AND_TID( aSpaceID, aHeaderNext, aTID,                \
                                  aNextSCN, aNextTID,    aTmpSlotHeader );    \
    }                                                                         \
    else                                                                      \
    {                                                                         \
        IDE_DASSERT( sctTableSpaceMgr::isVolatileTableSpace(aSpaceID)         \
                     == ID_TRUE );                                            \
        SVP_GET_NEXT_SCN_AND_TID( aSpaceID, aHeaderNext, aTID,                \
                                  aNextSCN, aNextTID,    aTmpSlotHeader );    \
    }                                                                         \
}
#endif

class sgmManager
{
  public:
    static inline  void * getOIDPtr( scSpaceID     aSpaceID,
                                     smOID         aOID );

    static smVCPieceHeader* getNxtVCPieceHeader( smVCPieceHeader *  aVCPieceHeader,
                                                 const smiColumn *  aColumn,
                                                 UShort          *  aOffsetIdx ); 
   static inline smVCPieceHeader* getVCPieceHeader( const void      *  aRow,
                                                    const smiColumn *  aColumn,
                                                    UShort          *  aOffsetIdx );
    static inline SChar* getVarColumnDirect( SChar           * aRow,
                                             const smiColumn * aColumn,
                                             UInt*             aLength );
    static SChar* getVarColumn( SChar           * aRow,
                                const smiColumn * aColumn,
                                UInt*             aLength );
    static SChar* getVarColumnDisk( SChar           * aRow,
                                    const smiColumn * aColumn,
                                    UInt*             aLength );
    static SChar* getVarColumn( SChar           * aRow,
                                const smiColumn * aColumn,
                                SChar           * aDestBuffer );
    // PROJ-2264
    static SChar* getCompressionVarColumn( SChar           * aRow,
                                           const smiColumn * aColumn,
                                           UInt            * aLength );
};

/***********************************************************************
 * Description : smiGetVarColumn() �� ���� �Լ�, BUG-48513
 *               get Valiable Column ���� �켱
 ***********************************************************************/
SChar* sgmManager::getVarColumnDirect( SChar           * aRow,
                                       const smiColumn * aColumn,
                                       UInt            * aLength )
{
    UShort          * sCurrOffsetPtr;
    smVCPieceHeader * sVCPieceHeader;
    UShort            sOffsetIdx ;

    /*  aColumns�� Ÿ���� Memory type�̸� */
    if ( (aColumn->flag & SMI_COLUMN_STORAGE_MASK)
         != SMI_COLUMN_STORAGE_MEMORY )
    {
        return getVarColumnDisk( aRow, aColumn, aLength );
    }

    if ( ( aColumn->flag & SMI_COLUMN_TYPE_MASK ) == SMI_COLUMN_TYPE_VARIABLE_LARGE )
    {
        return smcRecord::getVarLarge( aRow,
                                       aColumn,
                                       0,
                                       aLength,
                                       (SChar*)(aColumn->value),
                                       ID_TRUE );
    }
    
    if (( (smpSlotHeader*)aRow)->mVarOID != SM_NULL_OID )
    {
        sVCPieceHeader = getVCPieceHeader( aRow, aColumn, &sOffsetIdx );
        if ( sVCPieceHeader != NULL ) 
        {
            IDE_DASSERT( sOffsetIdx != ID_USHORT_MAX );
            IDE_DASSERT( sOffsetIdx < sVCPieceHeader->colCount );

            /* +1 �� ��� ����� �ǳʶٰ� offset array �� Ž���ϱ� �����̴�. */
            sCurrOffsetPtr = ((UShort*)(sVCPieceHeader + 1) + sOffsetIdx);

            IDE_DASSERT( *(sCurrOffsetPtr + 1) >= *sCurrOffsetPtr );

            /* next offset ���� ���� ���̰� �ȴ�. */
            *aLength    = ( *( sCurrOffsetPtr + 1 )) - ( *sCurrOffsetPtr );

            if ( *aLength != 0 )
            {
                return (SChar*)sVCPieceHeader + (*sCurrOffsetPtr);
            }
        }
    }

    *aLength    = 0;
    
    return NULL;
}

/***********************************************************************
 * Description : Variable slot header �� �����´�. 
 ***********************************************************************/
smVCPieceHeader* sgmManager::getVCPieceHeader( const void      *  aRow,
                                               const smiColumn *  aColumn,
                                               UShort          *  aOffsetIdx )
{
    UShort            sOffsetIdx;
    smVCPieceHeader * sVCPieceHeader ;
    smOID             sOID = ((smpSlotHeader*)aRow)->mVarOID;

    IDE_DASSERT( sOID != SM_NULL_OID );

    sVCPieceHeader = (smVCPieceHeader*)getOIDPtr( aColumn->colSpace,
                                                  sOID );
    if ( sVCPieceHeader == NULL )
    {
        /* BUG-47474 smpSlotHeader�� mVarOID�� smpFreeSlotHeader�� next free slot pointer��
         *           ������ ��ġ�� ����ϰ� �����Ƿ� �ε������� lock�� ���� �ʰ� ���ͳγ�带 Ž���ϴ� ���
         *           �̹� ������ slot�� ����ִ� slot���� �Ǵ��ϰ� ������ ��찡 �߻��� �� �ִ�.
         *           �� ��� next free slot pointer�� OID�� �Ǵ��ϴ� ������ �߻��� �� �����Ƿ�
         *           �� ������ �߻��� ��� ������ ������ �ʰ� ������ �ø��� �ε��� ��⿡��
         *           ����� ������ Ȯ���ϰ� Ž���� ������Ѵ�. */
        *aOffsetIdx = ID_USHORT_MAX;

        return NULL;
    }

    sOffsetIdx = aColumn->varOrder;

    if ( sOffsetIdx >= sVCPieceHeader->colCount )
    {
        return getNxtVCPieceHeader( sVCPieceHeader,
                                    aColumn,
                                    aOffsetIdx );
    }

    *aOffsetIdx     = sOffsetIdx;
    return sVCPieceHeader;
}

/***********************************************************************
 * Description : getOIDPtr ����ȭ ����
 ***********************************************************************/
void * sgmManager::getOIDPtr( scSpaceID     aSpaceID,
                              smOID         aOID )
{
    scOffset sOffset  = (scOffset)(aOID & SM_OFFSET_MASK);
    scPageID sPageID  = (scPageID)(aOID >> SM_OFFSET_BIT_SIZE);
    SChar  * sPagePtr = (SChar*)smmManager::getPagePtr( aSpaceID, sPageID );

    if ( sPagePtr != NULL )
    {
        return (void *)( sPagePtr + sOffset  );
    }
    
    return NULL;
}

#endif

