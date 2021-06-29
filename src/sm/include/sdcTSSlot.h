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
 * $Id: sdcTSSlot.h 82075 2018-01-17 06:39:52Z jina.kim $
 *
 * Description :
 *
 * �� ������ transaction status slot�� ���� ��������Դϴ�.
 *
 * # ����
 *
 *   DRDB�� MVCC, garbage collecting, transaction rollback��
 *   transaction ���¸� �����ϱ� ���� transaction status slot
 *
 *
 * # ����
 *
 *   - tss (transaction status slot)�� ����
 *
 *   tss�� �������̸� ������, ������ ���� ������ �����ȴ�.
 *    ___________________________
 *   |TID |        | Commite SCN |
 *   |____|_status_|_____________|
 *
 *    - TID
 *      : Ʈ����� ID
 *
 *    - Status
 *      : TSS�� ����
 *
 *    - Commit SCN
 *      : transaction commit�������� �Ҵ��ϴ� SCN ���� �����ϸ�,
 *        commit�Ǳ������� ���Ѵ� ���� ǥ��
 *
 *
 * # �����ڷᱸ��
 *
 *  - sdcTSS      ����ü
 *
 **********************************************************************/

#ifndef _O_SDC_TSSLOT_H_
#define _O_SDC_TSSLOT_H_ 1

# include <smDef.h>
# include <sdcDef.h>

class sdcTSSlot
{

public:

    static IDE_RC logAndInit( sdrMtx    * aMtx,
                              sdcTSS    * aSlotPtr,
                              sdSID       aTSSlotSID,
                              sdRID       aLstExtRID,
                              smTID       aTransID,
                              UInt        aTXSegEntryID );

    static IDE_RC setCommitSCN( sdrMtx      * aMtx,
                                sdcTSS      * aSlotPtr,
                                smSCN       * aCommitSCN,
                                idBool        aDoUpdate );

    static IDE_RC setState( sdrMtx       * aMtx,
                            sdcTSS       * aSlotPtr,
                            sdcTSState     aState );

    static inline void getTransInfo( sdcTSS  * aSlotPtr,
                                     smTID   * aTransID,
                                     smSCN   * aCommitSCN );

    static inline void init( sdcTSS   * aSlotPtr,
                             smTID      aTransID );

    static inline void  getCommitSCN( sdcTSS  * aSlotPtr,
                                      smSCN   * aCommitSCN );

    static inline smTID getTransID( sdcTSS   * aSlotPtr );

    static inline sdcTSState getState( sdcTSS  * aSlotPtr );

    static inline void unbind( sdcTSS     * aTSSlotPtr,
                               smSCN      * aCommitSCN,
                               sdcTSState   aState );
};

inline smTID sdcTSSlot::getTransID( sdcTSS   * aSlotPtr )
{
    IDE_ASSERT( aSlotPtr != NULL );

    return aSlotPtr->mTransID;
}

inline void sdcTSSlot::init( sdcTSS   * aSlotPtr,
                             smTID      aTransID )
{
    IDE_ASSERT( aSlotPtr != NULL );

    aSlotPtr->mTransID = aTransID;
    aSlotPtr->mState   = SDC_TSS_STATE_ACTIVE;
    SM_SET_SCN_INFINITE( &(aSlotPtr->mCommitSCN) );
}


/***********************************************************************
 * Description : TSS�� CommitSCN ��ȯ
 **********************************************************************/
inline void sdcTSSlot::getCommitSCN( sdcTSS  * aSlotPtr,
                                     smSCN   * aCommitSCN )
{
    IDE_ASSERT( aSlotPtr != NULL );

    SM_GET_SCN( aCommitSCN, &(aSlotPtr->mCommitSCN) );
}

/***********************************************************************
 * Description : TSS ���� ��ȯ
 **********************************************************************/
inline sdcTSState sdcTSSlot::getState( sdcTSS*  aSlotPtr )
{
    return aSlotPtr->mState;
}



/***********************************************************************
 * Description : Ʈ������� TID�� CommitSCN ��ȯ
 **********************************************************************/
inline void sdcTSSlot::getTransInfo( sdcTSS   * aSlotPtr,
                                     smTID    * aTransID,
                                     smSCN    * aCommitSCN )
{
    IDE_ASSERT( aSlotPtr != NULL );

    *aTransID = getTransID( aSlotPtr );
    getCommitSCN( aSlotPtr, aCommitSCN );
}


inline void sdcTSSlot::unbind( sdcTSS     * aTSSlotPtr,
                               smSCN      * aCommitSCN,
                               sdcTSState   aState )
{
    IDE_ASSERT( aTSSlotPtr != NULL );
    IDE_ASSERT( aCommitSCN != NULL );

    SM_SET_SCN( &aTSSlotPtr->mCommitSCN, aCommitSCN );
    aTSSlotPtr->mState = aState;
}


# endif // _O_SDC_TSSLOT_H_
