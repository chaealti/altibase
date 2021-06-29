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
* $Id: smmTBSAlterDiscard.cpp 19201 2006-11-30 00:54:40Z kmkim $
**********************************************************************/

#include <idl.h>
#include <idm.h>
#include <idu.h>
#include <ideErrorMgr.h>
#include <smDef.h>
#include <smErrorCode.h>
#include <smu.h>
#include <smm.h>
#include <smmReq.h>
#include <sctTableSpaceMgr.h>
#include <smmTBSAlterDiscard.h>
#include <smmTBSMultiPhase.h>

/*
  ������ (�ƹ��͵� ����)
*/
smmTBSAlterDiscard::smmTBSAlterDiscard()
{

}


/*
    Tablespace�� DISCARDED���·� �ٲٰ�, Loganchor�� Flush�Ѵ�.

    Discard�� ���� :
       - �� �̻� ����� �� ���� Tablespace
       - ���� Drop���� ����

    ��뿹 :
       - Disk�� ������ ������ ���̻� ���Ұ��� ��
         �ش� Tablespace�� discard�ϰ� ������ Tablespace���̶�
         ��ϰ� ������, CONTROL�ܰ迡�� Tablespace�� DISCARD�Ѵ�.

     ���ü����� :
       - CONTROL�ܰ迡���� ȣ��Ǳ� ������, sctTableSpaceMgr��
         Mutex�� ���� �ʿ䰡 ����.
 */
IDE_RC smmTBSAlterDiscard::alterTBSdiscard( smmTBSNode * aTBSNode )
{
    IDE_DASSERT( aTBSNode != NULL );

    IDE_TEST_RAISE ( SMI_TBS_IS_DISCARDED( aTBSNode->mHeader.mState),
                     error_already_discarded );

    // Discard Tablespace�� �ش�Ǵ� �ٴܰ� �ʱ�ȭ �ܰ���
    // MEDIA�ܰ�� �����Ѵ�.
    //
    // => ONLINE Tablespace�� Discard�ϴ� ��� PAGE �ܰ踦 �����ؾ���
    IDE_TEST( smmTBSMultiPhase::finiToMediaPhase( aTBSNode->mHeader.mState,
                                                  aTBSNode )
              != IDE_SUCCESS );
    
    aTBSNode->mHeader.mState = SMI_TBS_DISCARDED;

    IDE_TEST( smLayerCallback::updateTBSNodeAndFlush( & aTBSNode->mHeader )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION( error_already_discarded );
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_TBS_ALREADY_DISCARDED));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

