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
 * $Id: sdpstStackMgr.cpp 27229 2008-07-23 17:37:19Z newdaily $
 *
 * �� ������ Treelist Managed Segment���� ��ġ �̷������� �����ϴ�
 * Stack �������� ���������̴�.
 *
 ***********************************************************************/

# include <sdpstStackMgr.h>

/***********************************************************************
 * Description : Stack�� �ʱ�ȭ�Ѵ�.
 ***********************************************************************/
void sdpstStackMgr::initialize( sdpstStack  * aStack )
{
    aStack->mDepth = SDPST_EMPTY;
    idlOS::memset( &(aStack->mPosition), 
                   0x00, 
                   ID_SIZEOF(sdpstPosItem)*SDPST_BMP_TYPE_MAX);
    return;
}

/***********************************************************************
 * Description : Stack�� �����Ѵ�.
 ***********************************************************************/
void sdpstStackMgr::destroy()
{
    return;
}

/***********************************************************************
 * Description : Virtual Stack Depth ������ Slot ���� (aLHS �� aRHS)
 *               �Ÿ����� ��ȯ�Ѵ�.
 ***********************************************************************/
SShort sdpstStackMgr::getDistInVtDepth( sdpstPosItem       * aLHS,
                                        sdpstPosItem       * aRHS )
{
    IDE_DASSERT( aLHS != NULL );
    IDE_DASSERT( aRHS != NULL );
    return getDist( &(aLHS[ (UInt)SDPST_VIRTBMP ]),
                    &(aRHS[ (UInt)SDPST_VIRTBMP ]) );
}

/***********************************************************************
 * Description : Root Stack Depth ������ Slot ���� (aLHS �� aRHS)
 *               �Ÿ����� ��ȯ�Ѵ�.
 ***********************************************************************/
SShort sdpstStackMgr::getDistInRtDepth( sdpstPosItem       * aLHS,
                                        sdpstPosItem       * aRHS )
{
    IDE_DASSERT( aLHS != NULL );
    IDE_DASSERT( aRHS != NULL );
    return getDist( &(aLHS[ (UInt)SDPST_RTBMP ]),
                    &(aRHS[ (UInt)SDPST_RTBMP ]) );
}

/***********************************************************************
 * Description : Internal Stack Depth ������ Slot ���� (aLHS �� aRHS)
 *               �Ÿ����� ��ȯ�Ѵ�.
 ***********************************************************************/
SShort sdpstStackMgr::getDistInItDepth( sdpstPosItem       * aLHS,
                                        sdpstPosItem       * aRHS )
{
    IDE_DASSERT( aLHS != NULL );
    IDE_DASSERT( aRHS != NULL );
    return getDist( &(aLHS[ (UInt)SDPST_ITBMP ]),
                    &(aRHS[ (UInt)SDPST_ITBMP ]) );
}

/***********************************************************************
 * Description : Leaf Stack Depth ������ Slot ���� (aLHS �� aRHS)
 *               �Ÿ����� ��ȯ�Ѵ�.
 ***********************************************************************/
SShort sdpstStackMgr::getDistInLfDepth( sdpstPosItem       * aLHS,
                                        sdpstPosItem       * aRHS )
{
    IDE_DASSERT( aLHS != NULL );
    IDE_DASSERT( aRHS != NULL );
    return getDist( &(aLHS[ (UInt)SDPST_LFBMP ]),
                    &(aRHS[ (UInt)SDPST_LFBMP ]) );
}

/***********************************************************************
 * Description : Stack Depth�� ������� �� ��ġ���� ���İ��踦 ��ȯ�Ѵ�.
 ***********************************************************************/
SShort sdpstStackMgr::compareStackPos( sdpstStack  * aLHS,
                                       sdpstStack  * aRHS )
{
    SShort            sDist = 0;
    sdpstPosItem    * sLHS;
    sdpstPosItem    * sRHS;
    SInt              sDepth;
    sdpstBMPType   sDepthL;
    sdpstBMPType   sDepthR;

    IDE_DASSERT( aLHS != NULL );
    IDE_DASSERT( aRHS != NULL );

    sDepthL = getDepth( aLHS );
    sDepthR = getDepth( aRHS );

    if ( sDepthL == SDPST_EMPTY )
    {
        if ( sDepthR == SDPST_EMPTY )
        {
            return 0; // �Ѵ� empty �ΰ��
        }
        else
        {
            return 1; // lhs�� empty�� ���
        }
    }
    else
    {
        if ( sDepthR == SDPST_EMPTY )
        {
            return -1; // rhs �� empty�� ���
        }
        else
        {
            // �Ѵ� empty�� �ƴѰ��
        }
    }

    sLHS = getAllPos( aLHS );
    sRHS = getAllPos( aRHS );

    sDepth = (SInt)(sDepthL > sDepthR ? sDepthR : sDepthL);

    for ( ;
          sDepth > (SInt)SDPST_EMPTY;
          sDepth-- )
    {
        sDist = getDist( &(sLHS[ sDepth ]), &(sRHS[ sDepth ]));

        // ��� stack level���� distance�� 0���� �Ѵ�.
        if ( sDist == SDPST_FAR_AWAY_OFF )
        {
            // �Ǵ��� ���� �����Ƿ� �����񱳰� �ʿ��ϴ�.
        }
        else
        {
            if ( sDist == 0)
            {
                // ������������ ���ϱ� ������ �������� ������,
                // �� ���������� ���� ���� ��찡 ������ �ʴ´�.
                // �ֳ��ϸ�, �Ʒ� else���� �� �ɷ����� �����̴�.
                break;
            }
            else
            {
                sDist = ( sDist > 0 ? 1 : -1 );
                break;
            }
        }
    }
    return sDist;
}

void sdpstStackMgr::dump( sdpstStack    * aStack )
{
    IDE_ASSERT( aStack != NULL );

    ideLog::logMem( IDE_SERVER_0,
                    (UChar*)aStack,
                    ID_SIZEOF( sdpstStack ) );

    ideLog::log( IDE_SERVER_0,
                 "-------------------- Stack Begin --------------------\n"
                 "mDepth: %d\n"
                 "mPosition[VT].mNodePID: %u\n"
                 "mPosition[VT].mIndex: %u\n"
                 "mPosition[RT].mNodePID: %u\n"
                 "mPosition[RT].mIndex: %u\n"
                 "mPosition[IT].mNodePID: %u\n"
                 "mPosition[IT].mIndex: %u\n"
                 "mPosition[LF].mNodePID: %u\n"
                 "mPosition[LF].mIndex: %u\n"
                 "--------------------  Stack End  --------------------\n",
                 aStack->mDepth,
                 aStack->mPosition[SDPST_VIRTBMP].mNodePID,
                 aStack->mPosition[SDPST_VIRTBMP].mIndex,
                 aStack->mPosition[SDPST_RTBMP].mNodePID,
                 aStack->mPosition[SDPST_RTBMP].mIndex,
                 aStack->mPosition[SDPST_ITBMP].mNodePID,
                 aStack->mPosition[SDPST_ITBMP].mIndex,
                 aStack->mPosition[SDPST_LFBMP].mNodePID,
                 aStack->mPosition[SDPST_LFBMP].mIndex );
}
