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
 * $Id$
 **********************************************************************/
/**************************************************************
 * FILE DESCRIPTION : smxLCL.cpp                              *
 * -----------------------------------------------------------*
  �� ������ �� transaction����  �޸� �Ǵ� Disk LobCursor
  list�� �����ϰ�, oldestSCN���� ��ȯ�ϴ� ����̴�.

  LobCursor ���� /������ ,���ü� ���� ���������� ����Ǹ�,
  disk , memory garbage collector���� �д�   oldest SCN����
  LobCursor�� ����/������ �߻��ϴ���  blocking���� ����ǵ���
���� �Ͽ���.

  �׸��� ���� Lob Cursor List(LCL)��  circular double linked list
  �� �����ȴ�.

  baseNode in LCL(Lob Cursor List)
                                    next
   |   ---------------------------------
   V   V                               |
   -------  next   -------         ----------
  |      | ---->  |       | ---->  |        |
  |mSCN  |  prev  |  40   |        |  80    |
  |(40)  | <---   |       | <---   |        |
  -------         ---------        ---------
                                       ^
    |       prev                        |
     -----------------------------------

                 ---------------------->
                   insert�������(SCN�� ������ ����)

 smxLSL�� object�ȿ�  smLobCusro�� base����� mSCN����
 oldestSCN���� �ǵ��� list�� �Ʒ��� ���� �����Ѵ�.

  -  circular dobule linked list��
    node insert�� head node�� �Ǵ� ��쿡�� basenode��
     mSCN�� ���Ž�Ų��.

  -  circular dobule linked list�� node�� ������,
    head node�� �����ϴ� ��쿡�� head node��
    next  node�� mSCN���� base node�� mSCN�� ���Ž�Ų��.

 oldestSCN�� blocking���� �б� ���Ͽ� perterson's algorithm
 �� �����Ͽ���.
**********************************************************************/

#include <smErrorCode.h>
#include <smm.h>
#include <smxTransMgr.h>

/*********************************************************************
  Name:  smxLCL::getOldestSCN
  Argument: smSCN*
  Description:
  baseNode�� mSCN�� assgin�Ͽ� �ش�.
  �� baseNode�� mSCN�� oldestSCN�� ���� �ϵ��� circular double linked
  list ���Խ� head node�� �ǰų� , list������ head node�� ������
  ��쿡 base node�� mSCN�� �����Ѵ�.

  baseNode in SMXLCL
                                    next
   |   ---------------------------------
   V   V                               |
   -------  next   -------         ----------
  |      | ---->  |       | ---->  |        |
  |mSCN  |  prev  |  40   |        |  80    |
  |(40)  | <---   |       | <---   |        |
  -------         ---------        ---------
                                       ^
    |       prev                        |
     -----------------------------------

  list  insert/delet�� serial�ϰ� �̷������ oldestSCN��
 �дºκ��� insert/delete�ÿ� blocking���� �����ϱ� ���Ͽ�
  peterson algorithm��  �Ʒ��� ���� �����Ͽ���.

  mSyncA                      mSyncB
    -----------------------------> reader(smxLCL::getOldestSCN)
    <----------------------------- writer(smxLCL::insert,remove)

  added for A4.
 **********************************************************************/
void smxLCL::getOldestSCN(smSCN* aSCN)
{

    UInt sSyncVa=0;
    UInt sSyncVb=0;

    IDE_DASSERT( aSCN != NULL );
    while(1)
    {
        ID_SERIAL_BEGIN(sSyncVa = mSyncVa);

        // BUG-27735 : Peterson Algorithm
        ID_SERIAL_EXEC( SM_GET_SCN(aSCN, &(mBaseNode.mLobViewEnv.mSCN)), 1 );

        ID_SERIAL_END(sSyncVb = mSyncVb);

        if(sSyncVa == sSyncVb)
        {
            break;
        }
        else
        {
            idlOS::thr_yield();
        }
    }//while
}

/*********************************************************************
  Name:  smxLCL::insert
  Description:
  circular dobule linked list insert�ÿ�
  node��   �Ʒ� �׸��� ���� �ǵڿ� append�ǵ��� �Ѵ�.

  insert�� head node�� �Ǵ� ��쿡�� base node��
  mSCN�� ���Ž�Ų��.

                            next

     |--------------------------------
     V                               |
   -------  next   -------         ----------
  |      | ---->  |       | ---->  |        |
  |mSCN  |  prev  |  40   |        |  80    |
  |(40)  | <---   |       | <---   |        |
  -------         ---------        ---------
  base node
                                        ^
    |       prev                        |
     -----------------------------------

                    -------------------->
                    insert �������(SCN�� ������ ����)

 **********************************************************************/
void  smxLCL::insert(smLobCursor* aNode)
{

    IDE_DASSERT( aNode != NULL );

    aNode->mPrev        =  mBaseNode.mPrev;
    aNode->mNext        =  &mBaseNode;
    mBaseNode.mPrev->mNext  =  aNode;
    mBaseNode.mPrev = aNode;

    mCursorCnt++;

    // head node�� ��쿡 oldest scn���� �����Ѵ�.
    if(aNode->mPrev == &mBaseNode)
    {
        ID_SERIAL_BEGIN(mSyncVb++);

        // BUG-27735 : Peterson Algorithm
        ID_SERIAL_EXEC( SM_SET_SCN(&(mBaseNode.mLobViewEnv.mSCN),
                                   &(aNode->mLobViewEnv.mSCN)), 1 );

        ID_SERIAL_END(mSyncVa++);
    }
}

/*********************************************************************
  Name:  smxLCL::remove
  Description:
  circular dobule linked list�� node�� �����ϰ�,
  head node�� �����ϴ� ��쿡�� head node��
  next node�� mSCN���� base node�� mSCN�� ���Ž�Ų��.
 **********************************************************************/
void   smxLCL::remove(smLobCursor* aNode)
{

    IDE_DASSERT( aNode != NULL );

    // head node�� ��찡 ������ ����̱⶧����
    // oldestSCN���� �����ؾ� �ȴ�.
    if(aNode->mPrev == &mBaseNode)
    {
        ID_SERIAL_BEGIN(mSyncVb++);
        if(mBaseNode.mPrev == aNode)
        {
            //LoCursor��  �ϳ��� ���ԵǴ� ���
            //oldestSCN�� ���Ѵ�� �����Ѵ�.
            // BUG-27735 : Peterson Algorithm
            ID_SERIAL_EXEC(
                SM_SET_SCN_INFINITE(&(mBaseNode.mLobViewEnv.mSCN)),
                1 );
        }//if
        else
        {
            // next node�� SCN���� oldest scn���� �����Ѵ�.
            // BUG-27735 : Peterson Algorithm
            ID_SERIAL_EXEC( SM_SET_SCN(&(mBaseNode.mLobViewEnv.mSCN),
                                       &(aNode->mNext->mLobViewEnv.mSCN)),
                            1 );
        }//else
        ID_SERIAL_END(mSyncVa++);
    }//if aNode->mPrev

    aNode->mPrev->mNext = aNode->mNext;
    aNode->mNext->mPrev = aNode->mPrev;
    aNode->mNext = NULL;
    aNode->mPrev = NULL;

    mCursorCnt--;

    return;
}
