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
 *
 * $Id:$
 *
 * �� ������ extent descriptor�� ���� ��� �����̴�.
 *
 **********************************************************************/

#ifndef _O_SDPSF_EXTENT_H_
#define _O_SDPSF_EXTENT_H_ 1

#include <sdr.h>
#include <sdpDef.h>
#include <sdpsfDef.h>

class sdpsfExtent
{
public:
    /* ext desc �ʱ�ȭ */
    static IDE_RC initialize();

    /* dummy function */
    static IDE_RC destroy();

public:
    static inline void init( sdpsfExtDesc*  aExtDesc );

    /* PID�� �����Ѵ�.(tablespace�� extent�� ���϶� ���)*/
    static inline IDE_RC setFirstPID( sdpsfExtDesc*  aExtDesc,
                                      scPageID     aPID,
                                      sdrMtx*      aMtx );

    static inline IDE_RC setFlag( sdpsfExtDesc*  aExtDesc,
                                  UInt           aFlag,
                                  sdrMtx*        aMtx );

    /* ext desc�� first page ��ȯ */
    static inline scPageID getFstPID( sdpsfExtDesc*  aExtDesc );

    /* ext desc�� first page ��ȯ */
    static inline UInt getFlag( sdpsfExtDesc*  aExtDesc );

    static inline scPageID getFstDataPID( sdpsfExtDesc*  aExtDesc );

    static inline idBool isPIDInExt( sdpExtInfo   * aExtInfo,
                                     UInt           aPageCntInExt,
                                     scPageID       aPageID );

    static inline idBool isPIDInExt( scPageID       aFstPIDOfExt,
                                     UInt           aPageCntInExt,
                                     scPageID       aPageID );
};

inline void sdpsfExtent::init( sdpsfExtDesc*  aExtDesc )
{
    aExtDesc->mFstPID = SD_NULL_PID;
    aExtDesc->mFlag   = 0;
}

/***********************************************************************
 * Description : Ext Desc�� First Page ID�� �����Ѵ�.
 **********************************************************************/
inline IDE_RC sdpsfExtent::setFirstPID( sdpsfExtDesc*  aExtDesc,
                                        scPageID       aPID,
                                        sdrMtx*        aMtx )
{
    IDE_DASSERT( aExtDesc != NULL );
    IDE_DASSERT( aMtx != NULL );

    return sdrMiniTrans::writeNBytes(aMtx,
                                     (UChar*)&aExtDesc->mFstPID,
                                     &aPID,
                                     ID_SIZEOF(aPID) );
}

/***********************************************************************
 * Description : Ext Desc�� Flag�� �����Ѵ�.
 **********************************************************************/
inline IDE_RC sdpsfExtent::setFlag( sdpsfExtDesc*  aExtDesc,
                                    UInt           aFlag,
                                    sdrMtx*        aMtx )
{
    IDE_DASSERT( aExtDesc != NULL );
    IDE_DASSERT( aMtx != NULL );

    return sdrMiniTrans::writeNBytes(aMtx,
                                     (UChar*)&aExtDesc->mFlag,
                                     &aFlag,
                                     ID_SIZEOF(UInt) );
}

/***********************************************************************
 * Description : Ext Desc�� Flag���� �����Ѵ�.
 **********************************************************************/
inline UInt sdpsfExtent::getFlag( sdpsfExtDesc*  aExtDesc )
{
    return aExtDesc->mFlag;
}

/***********************************************************************
 * Description : Ext Desc�� First Page ID�� ��ȯ
 **********************************************************************/
inline  scPageID sdpsfExtent::getFstPID( sdpsfExtDesc*  aExtDesc )
{
    IDE_DASSERT( aExtDesc != NULL );

    return aExtDesc->mFstPID;
}

inline scPageID sdpsfExtent::getFstDataPID( sdpsfExtDesc*  aExtDesc )
{
    scPageID sFstDataPID = aExtDesc->mFstPID;

    if( SDP_SF_IS_FST_EXTDIRPAGE_AT_EXT( aExtDesc->mFlag ) )
    {
        /* ù��° �������� Extent Directory Page�� ���Ǿ����ϴ�. */
        sFstDataPID++;
    }

    return sFstDataPID;
}

inline idBool sdpsfExtent::isPIDInExt( sdpExtInfo  * aExtInfo,
                                       UInt          aPageCntInExt,
                                       scPageID      aPageID )
{
    return isPIDInExt( aExtInfo->mFstPID, aPageCntInExt, aPageID );
}

inline idBool sdpsfExtent::isPIDInExt( scPageID       aFstPIDOfExt,
                                       UInt           aPageCntInExt,
                                       scPageID       aPageID )
{
    scPageID sLstPIDInExt = aFstPIDOfExt + aPageCntInExt - 1;

    if( ( aPageID >= aFstPIDOfExt  ) && ( aPageID <= sLstPIDInExt ) )
    {
        return ID_TRUE;
    }
    else
    {
        return ID_FALSE;
    }
}

#endif // _O_SDPSF_EXTENT_H_
