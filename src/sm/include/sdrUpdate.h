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
 * $Id: sdrUpdate.h 82075 2018-01-17 06:39:52Z jina.kim $
 *
 * Description :
 *
 * �� ������ DRDB�� redo/undo function map�� ���� ��������̴�.
 *
 **********************************************************************/

#ifndef _O_SDR_UPDATE_H_
#define _O_SDR_UPDATE_H_ 1

#include <smDef.h>
#include <sdd.h>
#include <sdrDef.h>

class sdrUpdate
{
public:

    /* �ʱ�ȭ */
    static void initialize();

    /* dummy function  */
    static void destroy();

    /* BUG-25279 Btree for spatial�� Disk Btree�� �ڷᱸ�� �� �α� �и� 
     * Btree for Spatial�� �Ϲ� Disk Btree���� �α� �и��� ����
     * Recovery�ÿ� st layer�� �Ŵ޸� redo/undo/nta undo �Լ��� ȣ���ؾ� �Ѵ�.
     * �̸� ���� �ܺ������� Undo Redo NTA Undo �Լ��� �Ŵ� �� �ֵ��� �Ѵ�. */
    static void appendExternalUndoFunction( UInt                aUndoMapID,
                                            sdrDiskUndoFunction aDiskUndoFunction );

    static void appendExternalRedoFunction( UInt                aRedoMapID,
                                            sdrDiskRedoFunction aDiskRedoFunction );

    static void appendExternalRefNTAUndoFunction( UInt                      aNTARefUndoMapID,
                                                  sdrDiskRefNTAUndoFunction aDiskRefNTAUndoFunction );

    /* DRDB �α��� undo ���� */
    static IDE_RC doUndoFunction( idvSQL      * aStatistics,
                                  smTID         aTransID,
                                  smOID         aOID,
                                  SChar       * aData,
                                  smLSN       * aPrevLSN );
    
    /* DRDB �α��� redo ó�� */
    static IDE_RC doRedoFunction( SChar       * aValue,
                                  UInt          aValueLen,
                                  UChar       * aPageOffset,
                                  sdrRedoInfo * aRedoInfo,
                                  sdrMtx      * aMtx );
    
    /* DRDB NTA�α��� operational undo ó�� */
    static IDE_RC doNTAUndoFunction( idvSQL      * aStatistics,
                                     void        * aTrans,
                                     UInt          aOPType,
                                     scSpaceID     aSpaceID,
                                     smLSN       * aPrevLSN,
                                     ULong       * aArrData,
                                     UInt          aDataCount );
    
    /* DRDB Index NTA�α��� operational undo ó�� */
    static IDE_RC doRefNTAUndoFunction( idvSQL      * aStatistics,
                                        void        * aTrans,
                                        UInt          aOPType,
                                        smLSN       * aPrevLSN,
                                        SChar       * aRefData );

private:
  
      static  IDE_RC redoNA( SChar       * aData,
                             UInt          aLength,
                             UChar       * aPagePtr,
                             sdrRedoInfo * aRedoInfo,
                             sdrMtx      * aMtx );
  
};

extern sdrDiskRedoFunction gSdrDiskRedoFunction[SM_MAX_RECFUNCMAP_SIZE];
extern sdrDiskUndoFunction gSdrDiskUndoFunction[SM_MAX_RECFUNCMAP_SIZE];

#endif // _O_SDR_UPDATE_H_


