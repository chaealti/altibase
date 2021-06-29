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
 ** $Id$
 ***********************************************************************/

#include <idl.h>
#include <ideErrorMgr.h>
#include <mtdTypes.h>
#include <mtuProperty.h>

#include <ste.h>
#include <stcDef.h>
#include <stdTypes.h>
#include <stdUtils.h>
#include <stdParsing.h>
#include <stdPrimitive.h>
#include <qcuProperty.h>
#include <stfAnalysis.h>

#include <stdPolyClip.h>

/***********************************************************************
 * Description :
 * Vertex�� ��ϵǾ� �ִ� VertexEntry�� �� Entry�� �����ϴ� ���׸�Ʈ�� 
 * ������ ���� �����ϰ�, VertexEntry�� ���۰� ���� �̾ ��ȯ���·�
 * �����. 
 * 
 *                      Seg2 (Entry2)
 *                      /
 *                     /
 * (Entry1) Seg1 -----*----- Seg3 (Entry3)
 *            aVertex
 * 
 * �� �׸��� ���� ��� Vertex���� VertexEntry�� ������ 
 * 
 *   3->2->1->3 �� ���� ���·� ���ĵȴ�.
 * 
 * Vertex* aVertex : Vertex ������
 **********************************************************************/
void stdPolyClip::sortVertexList( Vertex* aVertex )
{
    VertexEntry* sStart;
    VertexEntry* sEnd;
    VertexEntry* sCurr;
    VertexEntry* sComp;   
    VertexEntry  sTmp;

    if ( aVertex->mList != NULL )
    {
        if ( aVertex->mNext != aVertex )
        {
            sStart = aVertex->mList;
            sCurr = sStart;

            do
            {
                sCurr->mAngle = getSegmentAngle( sCurr->mSeg, aVertex->mCoord );
                sCurr = sCurr->mNext;
            } while ( sCurr != sStart );

            sCurr = sStart;
            sEnd = sStart->mPrev;
            do
            {
                sComp = sStart;
                do
                {
                    if ( sComp->mAngle > sComp->mNext->mAngle )
                    {
                        sTmp.mAngle          = sComp->mAngle;
                        sTmp.mSeg            = sComp->mSeg;
                        sComp->mAngle        = sComp->mNext->mAngle;
                        sComp->mSeg          = sComp->mNext->mSeg;
                        sComp->mNext->mAngle = sTmp.mAngle;
                        sComp->mNext->mSeg   = sTmp.mSeg;
                    }
                    else
                    {
                        // Nothing to do
                    }

                    sComp = sComp->mNext;
                }while ( sComp != sEnd );
        
                sCurr = sCurr->mNext;
            }while ( sCurr != sStart );

            aVertex->mNext = aVertex;
        }
        else
        {
            // Nothing to do
        }
    }
    else
    {
        // Nothing to do.
    }
}

/***********************************************************************
 * Description :
 * ���׸�Ʈ�� x��� �̷�� ���� ���Ѵ�.
 *
 *          aSeg
 *           /
 *          / ) ��
 * mCoord  *-------- x��
 *
 * Segment*   aSeg   : ���׸�Ʈ
 * stdPoint2D mCoord : ������ ��ǥ
 **********************************************************************/
SDouble stdPolyClip::getSegmentAngle( Segment*   aSeg, 
                                      stdPoint2D mCoord )
{
    SDouble sRet;
   
    if ( ( stdUtils::isSamePoints2D4Func( &(aSeg->mStart), &mCoord ) == ID_TRUE ) )
    {
       sRet = idlOS::atan2( aSeg->mEnd.mY - mCoord.mY, aSeg->mEnd.mX - mCoord.mX ); 
    }
    else
    {
       sRet = idlOS::atan2( aSeg->mStart.mY - mCoord.mY, aSeg->mStart.mX - mCoord.mX );
    }

    return sRet;
}

/***********************************************************************
 * Description :
 * �Էµ� ���׸�Ʈ�� Intersection ���� ����� ���ԵǴ��� �Ǵ��Ѵ�.
 * ���� ���Եȴٸ�, ���׸�Ʈ�� �Էµ� ������ ����� ��, 
 * �ݴ� �������� ��������� �����Ͽ� aDir�� �ѱ��. 
 *
 * Segment*   aSeg : ���׸�Ʈ
 * Direction* aDir : ���� ���׸�Ʈ�� ��� �� ��� ���ؾ� �ϴ� ���� 
 * UInt       aPN  : ������� ����
 **********************************************************************/
idBool stdPolyClip::segmentRuleIntersect( Segment*   aSeg, 
                                          Direction* aDir, 
                                          UInt       /* aPN */ )
{
    idBool sRet;

    if ( ( aSeg->mLabel == ST_SEG_LABEL_INSIDE ) || 
         ( aSeg->mLabel == ST_SEG_LABEL_SHARED1) ) 
    {
        *aDir = ST_SEG_DIR_FORWARD;
         sRet = ID_TRUE;
    }
    else
    {
        sRet = ID_FALSE;
    }

    return sRet;
}

/***********************************************************************
 * Description :
 * �Էµ� ���׸�Ʈ�� Union ���� ����� ���ԵǴ��� �Ǵ��Ѵ�.
 * ���� ���Եȴٸ�, ���׸�Ʈ�� �Էµ� ������ ����� ��, 
 * �ݴ� �������� ��������� �����Ͽ� aDir�� �ѱ��. 
 *
 * Segment*   aSeg : ���׸�Ʈ
 * Direction* aDir : ���� ���׸�Ʈ�� ��� �� ��� ���ؾ� �ϴ� ���� 
 * UInt       aPN  : ������� ����
 **********************************************************************/
idBool stdPolyClip::segmentRuleUnion( Segment*   aSeg, 
                                      Direction* aDir, 
                                      UInt       /* aPN */ )
{
    idBool sRet;

    if ( ( aSeg->mLabel == ST_SEG_LABEL_OUTSIDE ) ||
         ( aSeg->mLabel == ST_SEG_LABEL_SHARED1 ) )
    {
        *aDir = ST_SEG_DIR_FORWARD;
         sRet = ID_TRUE;
    }
    else
    {
        sRet = ID_FALSE;
    }
    
    return sRet;
}

/***********************************************************************
 * Description :
 * �Էµ� ���׸�Ʈ�� Difference ���� ����� ���ԵǴ��� �Ǵ��Ѵ�.
 * ���� ���Եȴٸ�, ���׸�Ʈ�� �Էµ� ������ ����� ��, 
 * �ݴ� �������� ��������� �����Ͽ� aDir�� �ѱ��. 
 *
 * A difference B �� ���� ������ �ϴ� ��� �Է¹��� ���׸�Ʈ�� ����
 * �������� ��ȣ�� aPN ���� ������ ���׸�Ʈ�� A�� ���ϴ� ���׸�Ʈ�̰�, 
 * ũ�ų� ������ B�� ���ϴ� ���׸�Ʈ�̴�.
 *
 * Segment*   aSeg : ���׸�Ʈ
 * Direction* aDir : ���� ���׸�Ʈ�� ��� �� ��� ���ؾ� �ϴ� ���� 
 * UInt       aPN  : ������ ��ȣ 
 **********************************************************************/
idBool stdPolyClip::segmentRuleDifference( Segment*   aSeg, 
                                           Direction* aDir, 
                                           UInt       aPN )
{
    idBool sRet = ID_FALSE;

    if ( ( ( aSeg->mLabel == ST_SEG_LABEL_OUTSIDE ) ||
           ( aSeg->mLabel == ST_SEG_LABEL_SHARED2 ) ) && 
           ( aSeg->mParent->mPolygonNum < aPN ) )
    {
        *aDir = ST_SEG_DIR_FORWARD;
         sRet = ID_TRUE;
    }
    else
    {

        if ( ( ( aSeg->mLabel == ST_SEG_LABEL_INSIDE  ) || 
               ( aSeg->mLabel == ST_SEG_LABEL_SHARED2 ) ) && 
               ( aSeg->mParent->mPolygonNum >= aPN ) )
        {
            *aDir = ST_SEG_DIR_BACKWARD;
             sRet = ID_TRUE;
        }
        else
        {
            // Nothing to do
        }
    }

    return sRet;
}

/***********************************************************************
 * Description :
 * �Էµ� ���׸�Ʈ�� SymDifference ���� ����� ���ԵǴ��� �Ǵ��Ѵ�.
 * ���� ���Եȴٸ�, ���׸�Ʈ�� �Էµ� ������ ����� ��, 
 * �ݴ� �������� ��������� �����Ͽ� aDir�� �ѱ��. 
 *
 * Segment*   aSeg : ���׸�Ʈ
 * Direction* aDir : ���� ���׸�Ʈ�� ��� �� ��� ���ؾ� �ϴ� ���� 
 * UInt       aPN  : ������� ����
 **********************************************************************/
idBool stdPolyClip::segmentRuleSymDifference( Segment*   aSeg, 
                                              Direction* aDir, 
                                              UInt       /* aPN */ )
{
    idBool sRet = ID_FALSE;

    if ( aSeg->mLabel == ST_SEG_LABEL_OUTSIDE )
    {
        *aDir = ST_SEG_DIR_FORWARD;
         sRet = ID_TRUE;
    }
    else
    {
        if ( aSeg->mLabel == ST_SEG_LABEL_INSIDE )
        {
            *aDir = ST_SEG_DIR_BACKWARD;
             sRet = ID_TRUE;
        }
        else
        {
            // Nothing to do
        }
    }

    return sRet;
}

/***********************************************************************
 * Description :
 * �Է¹��� ���� Ž���Ͽ� Clip ����� ���ԵǾ�� �ϴ� ���׸�Ʈ�� ã��, 
 * ã�Ƴ� ���׸�Ʈ�� �������� �ϴ� ���� �����ϵ��� Collect�� ȣ���Ѵ�.
 *
 * iduMemory*   aQmxMem        :
 * Segment**    aRingList      : �� ����Ʈ
 * UInt         aRingCount     : �� ����
 * ResRingList* aResRingList   : ��� �� ����Ʈ
 * Segment**    aIndexSegList  : ����� ���� �ε���
 * UInt*        aIndexSegCount : �ε��� ��
 * SegmentRule  aRule          : ����� ���Ե����� �����ϴ� �Լ��� ������
 * UInt         aPN            : ������ ��ȣ
 **********************************************************************/
IDE_RC stdPolyClip::clip( iduMemory*   aQmxMem,    
                          Segment**    aRingList, 
                          UInt         aRingCount,
                          ResRingList* aResRingList,
                          Segment**    aIndexSegList,
                          UInt*        aIndexSegCount,
                          SegmentRule  aRule, 
                          UInt         aPN,
                          UChar*       aCount )
{
    UInt      i;
    Segment*  sCurrSeg;
    Direction sDir = ST_SEG_DIR_FORWARD;
    UInt      sRingCnt = 0;
    
    *aIndexSegCount = 0;

    for ( i = 0 ; i < aRingCount ; i++ )
    {
        sCurrSeg = aRingList[i];
        do
        {
            if ( ( sCurrSeg->mUsed == ST_SEG_USABLE ) && 
                 ( aRule( sCurrSeg, &sDir, aPN ) == ID_TRUE ) )
            {
                if ( aCount == NULL )
                {
                    IDE_TEST ( collect( aQmxMem, 
                                        aResRingList,
                                        sRingCnt,
                                        aIndexSegList, 
                                        aIndexSegCount, 
                                        sCurrSeg, 
                                        aRule, 
                                        &sDir, 
                                        aPN,
                                        aCount ) 
                               != IDE_SUCCESS );
                    sRingCnt++;
                }
                else
                {
                    if ( sCurrSeg->mLabel == ST_SEG_LABEL_OUTSIDE )
                    {
                        IDE_TEST ( collect( aQmxMem, 
                                            aResRingList,
                                            sRingCnt,
                                            aIndexSegList, 
                                            aIndexSegCount, 
                                            sCurrSeg, 
                                            aRule, 
                                            &sDir, 
                                            aPN,
                                            aCount ) 
                                   != IDE_SUCCESS );
                        sRingCnt++;
                    }
                    else
                    {
                        // Nothing to do
                    }
                }
            }
            else
            {
                // Nothing to do
            }
            sCurrSeg = stdUtils::getNextSeg( sCurrSeg );

        }while ( sCurrSeg != aRingList[i] );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description :
 * �Է¹��� ���׸�Ʈ�� �������� ����� ���ԵǾ�� �ϴ� ���׸�Ʈ�� 
 * ã�� ���� �����ϰ� ��� �� ����Ʈ�� �߰��Ѵ�.
 *
 * iduMemory*   aQmxMem
 * ResRingList* aResRingList   : ��� �� ����Ʈ 
 * UInt         aRingNumber    : ������ ���� ��ȣ
 * Segment**    aIndexSegList  : ����� ���� �ε���
 * UInt*        aIndexSegCount : �ε��� ��
 * Segment*     aSeg           : ���� ���׸�Ʈ
 * SegmentRule  aRule          : ���׸�Ʈ �Ǻ� �Լ� ������
 * Direction*   aDir           : ���׸�Ʈ ���� ����
 * UInt         aPN            : ������ ��ȣ
 **********************************************************************/
IDE_RC  stdPolyClip::collect( iduMemory*   aQmxMem,
                              ResRingList* aResRingList,
                              UInt         aRingNumber,
                              Segment**    aIndexSegList,
                              UInt*        aIndexSegCount,
                              Segment*     aSeg, 
                              SegmentRule  aRule, 
                              Direction*   aDir, 
                              UInt         aPN, 
                              UChar*       aCount )
{
    Segment*     sSeg           = aSeg;
    Segment*     sNewSeg;
    Segment*     sMaxSeg        = NULL;
    VertexEntry* sCurrEntry;
    VertexEntry* sStart;
    SInt         sPointsCount   = 0;
    SInt         sVaritation    = ST_NOT_SET;
    SInt         sReverse       = ST_NOT_SET;
    SInt         sPreVar        = ST_NOT_SET;
    SInt         sPreRev        = ST_NOT_SET;
    Chain*       sChain         = NULL;
    Chain*       sParent        = NULL;
    Chain*       sPrevChain     = NULL;
    Chain*       sFirstChain    = NULL;
    idBool       sIsCreateChain = ID_FALSE;
    stdPoint2D   sPt;
    stdPoint2D   sNextPt;
    stdPoint2D   sStartPt;
    stdPoint2D   sMaxPt;
    Ring*        sRing;
    stdMBR       sTotalMbr;
    stdMBR       sSegmentMbr;

    stdUtils::initMBR( &sTotalMbr );

    if ( isSameDirection( *aDir, getDirection( sSeg ) ) == ID_TRUE )
    {
        sStartPt = sSeg->mStart;
    }
    else
    {
        sStartPt = sSeg->mEnd;
    }

    sMaxPt = sStartPt;
    
    do
    {
        IDE_TEST_RAISE( sSeg == NULL, ERR_UNKNOWN );

        sPointsCount++;
        sSeg->mUsed = ST_SEG_USED;

        if ( isSameDirection( *aDir, getDirection( sSeg ) ) == ID_TRUE )
        {
            sPt     = sSeg->mStart;
            sNextPt = sSeg->mEnd;
        }
        else
        {
            sPt     = sSeg->mEnd;
            sNextPt = sSeg->mStart;
        }

        /* BUG-33634
         * ������ġ�� �ִ� ���� ã�ų�, �� �� ���԰��踦 ����� �� �ֵ���
         * Clip ����� ���ؼ��� �ε����� �����Ѵ�. */

        stdUtils::getSegProperty( &sPt,
                                  &sNextPt,
                                  &sVaritation,
                                  &sReverse ) ;

        IDE_TEST ( aQmxMem->alloc( ID_SIZEOF(Segment),
                                   (void**) &sNewSeg )
                   != IDE_SUCCESS );
        
        sNewSeg->mBeginVertex = NULL;
        sNewSeg->mEndVertex   = NULL;

        if ( sReverse == ST_NOT_REVERSE )
        {
            sNewSeg->mStart = sPt;
            sNewSeg->mEnd   = sNextPt;
        }
        else
        {
            sNewSeg->mStart = sNextPt;
            sNewSeg->mEnd   = sPt;
        }

        sNewSeg->mPrev = NULL;
        sNewSeg->mNext = NULL;

        if ( ( sVaritation != sPreVar ) || ( sReverse != sPreRev ) )
        {
            if ( sParent != NULL )
            {
                aIndexSegList[*aIndexSegCount] = sParent->mBegin;
                (*aIndexSegCount)++;
            }
            else
            {
                // Nothing to do
            }

            /* �Ӽ��� �ٸ��� ü���� �����Ѵ�. */
            IDE_TEST( aQmxMem->alloc( ID_SIZEOF(Chain),
                                      (void**) &sChain )
                      != IDE_SUCCESS );

            if ( sIsCreateChain == ID_FALSE )
            {
                sIsCreateChain = ID_TRUE;
                sFirstChain    = sChain;
            }
            else
            {
                // Nothing to do
            }

            stdUtils::initChain( sNewSeg, 
                                 sChain, 
                                 sVaritation, 
                                 sReverse, 
                                 0,             /* ������ ��ȣ, ��� ���� */
                                 aRingNumber, 
                                 ST_PARALLEL,   /* �� ����, ��� ���� */
                                 ST_CHAIN_NOT_OPPOSITE );

            if ( sPrevChain != NULL )
            {
                sChain->mPrev     = sPrevChain;
                sPrevChain->mNext = sChain;
            }
            else
            {
                sChain->mNext = NULL;
                sChain->mPrev = NULL;
            }

            sParent = sChain;
            IDE_TEST_RAISE( sParent == NULL, ERR_UNKNOWN );
            sNewSeg->mParent = sParent;

            sPreVar = sVaritation;
            sPreRev = sReverse;
        }
        else
        {
            sParent = sChain;
            IDE_TEST_RAISE( sParent == NULL, ERR_UNKNOWN );
            sNewSeg->mParent = sParent;

            if ( sReverse == ST_NOT_REVERSE )
            {
                stdUtils::appendLastSeg( sNewSeg, sChain );
            }
            else
            {
                stdUtils::appendFirstSeg( sNewSeg, sChain );
            }
        }
        sPrevChain = sChain;
        // classify end
        
        if ( ( sSeg->mLabel == ST_SEG_LABEL_SHARED1 ) || 
             ( sSeg->mLabel == ST_SEG_LABEL_SHARED2 ) )
        {
            // mark used shared segment
            sStart     = sSeg->mBeginVertex->mList;
            sCurrEntry = sStart;
            do
            {
                if ( ( sCurrEntry->mSeg != sSeg )                             &&
                     ( stdUtils::isSamePoints2D4Func( &(sCurrEntry->mSeg->mStart), 
                                                 &(sSeg->mStart) ) 
                       == ID_TRUE )                                           &&
                     ( stdUtils::isSamePoints2D4Func( &(sCurrEntry->mSeg->mEnd), 
                                                 &(sSeg->mEnd) ) 
                       == ID_TRUE ) )
                {
                    sCurrEntry->mSeg->mUsed = ST_SEG_USED;
                }
                else
                {
                    // Nothing to do
                }
                sCurrEntry = sCurrEntry->mNext;

            }while ( ( sCurrEntry != NULL ) && ( sCurrEntry != sStart ) );
        }
        else
        {
            // Nothing to do
        }

        if ( sMaxSeg == NULL )
        {
            sMaxSeg = sNewSeg;
        }

        if ( sMaxPt.mY < sNextPt.mY )
        {
            sMaxSeg = sNewSeg;
            sMaxPt  = sNextPt;
        }
        else
        {
            if ( sMaxPt.mY == sNextPt.mY )
            {
                if ( sMaxPt.mX <= sNextPt.mX )
                {
                    sMaxSeg = sNewSeg;
                    sMaxPt  = sNextPt;
                }
                else
                {
                    // Nothing to do
                }
            }
            else
            {
                // Nothing to do
            }
        }

        sSegmentMbr.mMinX = sSeg->mStart.mX;
        sSegmentMbr.mMaxX = sSeg->mEnd.mX;
        if( sSeg->mStart.mY < sSeg->mEnd.mY )
        {
            sSegmentMbr.mMinY = sSeg->mStart.mY;
            sSegmentMbr.mMaxY = sSeg->mEnd.mY;
        }
        else
        {
            sSegmentMbr.mMinY = sSeg->mEnd.mY;
            sSegmentMbr.mMaxY = sSeg->mStart.mY;
        }

        stdUtils::mergeOrMBR( &sTotalMbr, &sTotalMbr, &sSegmentMbr );

        if ( stdUtils::isSamePoints2D4Func( &sStartPt, &sNextPt ) == ID_TRUE )
        {
            /* ���� �������� ���� ���׸�Ʈ�� ������ ���ٸ�
             * ���� ���� ������ ���̹Ƿ� �������� �������´�. */
            break;
        }
        else
        {
            if ( isIntersectPoint( sSeg, *aDir ) == ID_TRUE )
            {
                /* ���� ���� �����̶��, ������ �����ؾ� �� 
                 * ���׸�Ʈ�� ã�ƾ� �Ѵ�. */
                sSeg = jump( sSeg, aRule, aDir, aPN, aCount );
            }
            else
            {
                /* ���� ���ù������� ���� ���׸�Ʈ�� �����Ѵ�. */
                if ( *aDir == ST_SEG_DIR_FORWARD )
                {
                    sSeg = stdUtils::getNextSeg( sSeg );
                }
                else
                {
                    sSeg = stdUtils::getPrevSeg( sSeg );
                }
            }
        } // end of if

    }while(1);

    if ( ( sIsCreateChain == ID_TRUE ) && ( sChain != sFirstChain ) )
    {
        aIndexSegList[ *aIndexSegCount ] = sParent->mBegin;
        ( *aIndexSegCount )++;

        sChain->mNext      = sFirstChain;
        sFirstChain->mPrev = sChain;
        
        IDE_TEST( aQmxMem->alloc( ID_SIZEOF(Ring),
                                  (void**) &sRing )
                  != IDE_SUCCESS );

        sRing->mPrev       = NULL;
        sRing->mNext       = NULL;
        sRing->mParent     = NULL;
        sRing->mFirstSeg   = sFirstChain->mBegin;
        sRing->mRingNumber = aRingNumber;
        sRing->mPointCnt   = sPointsCount;
        stdUtils::copyMBR( &(sRing->mMBR), &sTotalMbr );

        /* ���� ���� ��� */
        if ( sMaxSeg->mParent->mReverse == ST_NOT_REVERSE )
        {
            sRing->mOrientation = stdUtils::CCW( *getPrevPoint( sMaxSeg, ID_FALSE ),
                                                 sMaxSeg->mEnd,
                                                 *getNextPoint( sMaxSeg, ID_FALSE ) );
        }
        else
        {
            sRing->mOrientation = stdUtils::CCW( *getPrevPoint( sMaxSeg, ID_TRUE ),
                                                 sMaxSeg->mStart,
                                                 *getNextPoint( sMaxSeg, ID_TRUE ) );
        }

        /* ���� ���⿡ ���� �ݽð� ������ ����Ʈ�� �տ�, 
         * �ð������ ����Ʈ�� �ڿ� �߰��Ѵ�. */
        if ( sRing->mOrientation == ST_COUNTERCLOCKWISE )
        {
            appendFirst( aResRingList, sRing );
        }
        else
        {
            appendLast( aResRingList, sRing );
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_UNKNOWN );
    {
        IDE_SET( ideSetErrorCode( stERR_ABORT_UNKNOWN_POLYGON ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description :
 * ���׸�Ʈ ���� ���⿡ ���� ���׸�Ʈ�� �� ���� �������� ���θ� ��ȯ�Ѵ�.
 *
 * Segment*  aSeg : ���׸�Ʈ 
 * Direction aDir : ���׸�Ʈ ���� ���� 
 **********************************************************************/
idBool  stdPolyClip::isIntersectPoint( Segment*  aSeg, 
                                       Direction aDir )
{
    idBool sRet;

    if ( isSameDirection( aDir, getDirection( aSeg ) ) == ID_TRUE )
    {
        if ( aSeg->mEndVertex != NULL )
        {
            sRet = ID_TRUE;
        }
        else
        {
            sRet = ID_FALSE;
        }
    }
    else
    {
        if ( aSeg->mBeginVertex != NULL )
        {
            sRet = ID_TRUE;
        }
        else
        {
            sRet = ID_FALSE;
        }
    }

    return sRet;
}

/***********************************************************************
 * Description :
 * �Է¹��� ���׸�Ʈ���� �������� �����ؾ� �ϴ� ���׸�Ʈ�� ã�� ��ȯ�Ѵ�.
 *
 *              (1)
 *             /
 *            /
 * aSeg  --->*------ (2)
 *            \
 *             \
 *              (3)
 *
 * aSeg���� �ݽð� �������� ���鼭 ����� ���ԵǾ�� �ϴ� ���׸�Ʈ�� ã�´�.
 * (1), (2), (3)�� ������ Ž���Ѵ�. 
 *
 * Segment*    aSeg  : ���׸�Ʈ
 * SegmentRule aRule : ���׸�Ʈ �Ǻ� �Լ� ������
 * Direction   aDir  : ���׸�Ʈ ���� ����
 * UInt        aPN   : ������ ��ȣ
 **********************************************************************/
Segment* stdPolyClip::jump( Segment*    aSeg, 
                            SegmentRule aRule, 
                            Direction*  aDir, 
                            UInt        aPN,
                            UChar*      aCount )
{
    VertexEntry* sCurrEntry;
    VertexEntry* sStartEntry;
    Segment*     sRet        = NULL;
    
    if ( isSameDirection( *aDir, getDirection( aSeg ) ) == ID_TRUE )
    {
        sStartEntry = aSeg->mEndVertex->mList;
    }
    else
    {
        sStartEntry = aSeg->mBeginVertex->mList;
    }

    sCurrEntry = sStartEntry;
    do
    {
        if ( sCurrEntry->mSeg == aSeg )
        {
            break;
        }
        else
        {
            // Nothing to do
        }

        sCurrEntry = sCurrEntry->mNext;

    }while ( sCurrEntry != sStartEntry );

    sStartEntry = sCurrEntry;

    if ( aCount == NULL )
    {
        do
        {
           sCurrEntry = sCurrEntry->mPrev;
           if ( ( sCurrEntry->mSeg->mUsed == ST_SEG_USABLE )        && 
                ( aRule( sCurrEntry->mSeg, aDir, aPN ) == ID_TRUE ) )
           {
               sRet = sCurrEntry->mSeg;
               break;
           }
           else
           {
               // Nothing to do
           }
        }while ( sCurrEntry != sStartEntry );
    }
    else
    {
        do
        {
           sCurrEntry = sCurrEntry->mNext;
           if ( ( sCurrEntry->mSeg->mUsed == ST_SEG_USABLE )        && 
                ( aRule( sCurrEntry->mSeg, aDir, aPN ) == ID_TRUE ) )
           {
               sRet = sCurrEntry->mSeg;
               break;
           }
           else
           {
               // Nothing to do
           }
        }while ( sCurrEntry != sStartEntry );
    }

    return sRet;
}

/***********************************************************************
 * Description :
 * �� ���׸�Ʈ�� ���� ���δ�. 
 * A (op) B���� A�� ���ϴ� ���׸�Ʈ�� 
 * B�� �ܺο� ������       ST_SEG_LABEL_OUTSIDE,
 * B�� ���ο� ������       ST_SEG_LABEL_INSIDE,
 * B�� ���׸�Ʈ�� ��ġ�ϸ� ST_SEG_LABEL_SHARE1 (������� ��ġ)
 *                         ST_SEG_LABEL_SHARE2 (������ �ٸ�)
 *
 * B�� ���ϴ� ���׸�Ʈ�� �������� ������� ���� ���δ�. 
 *
 *
 * ��) �� �� ����� ������ A�� �� �� ����� ������ B 
 *
 *                         ST_SEG_LABEL_OUTSIDE
 *                                /
 *                          +--------------------------+
 *  ST_SEG_LABEL_OUTSIDE    |  ( ST_SEG_LABEL_INSIDE ) |
 *                       \  |     /                    |
 *                       +--*------+                   |
 *                       |  |      |                   |
 *                       +--*--+   |                   |
 *                          |  |   |                   |
 *                          +--*===*-------------------+
 *                                \ 
 *                           ST_SEG_LABEL_SHARE1 (������ ���� ���)
 *                           ST_SEG_LABEL_SHARE2 (������ �ٸ� ���)
 *
 * Segment** aRingSegList   : �� ����Ʈ 
 * UInt      aRingCount     : �� ����
 * Segment** aIndexSeg      : �ε��� 
 * UInt      aIndexSegTotal : �ε��� ��
 * UInt      aMax1          : A (op) B ���� A�� ���ϴ� ������ ��ȣ �� ���� ū ��
 * UInt      aMax2          : B�� ���ϴ� ������ ��ȣ �� ���� ū ��
 **********************************************************************/
IDE_RC stdPolyClip::labelingSegment( Segment** aRingSegList, 
                                     UInt      aRingCount, 
                                     Segment** aIndexSeg, 
                                     UInt      aIndexSegTotal,
                                     UInt      aMax1, 
                                     UInt      aMax2,
                                     UChar*    aCount )
{
    UInt     i;
    Segment* sCurrSeg;
    idBool   sIsFirst;

    for ( i = 0 ; i < aRingCount ; i++ )
    {
        sIsFirst    = ID_TRUE;
        sCurrSeg    = aRingSegList[i];
        
        do
        {
            sCurrSeg->mUsed  = ST_SEG_USABLE;
            sCurrSeg->mLabel = ST_SEG_LABEL_OUTSIDE;
            if ( ( sCurrSeg->mBeginVertex != NULL ) && ( sCurrSeg->mEndVertex != NULL ) )
            {
                /* BUG-33436
                 * ���׸�Ʈ�� �� ������ ��� �����̶�� Shared ���׸�Ʈ�� ���ɼ��� �ִ�.
                 * Share ���� üũ�� ��, �ƴ� ��쿡�� Inside �Ǵ� Outside���� üũ�ؾ��Ѵ�. */

                determineShare( sCurrSeg );
                if ( ( sCurrSeg->mLabel != ST_SEG_LABEL_SHARED1 ) && 
                     ( sCurrSeg->mLabel != ST_SEG_LABEL_SHARED2 ) )
                {
                    determineInside( aIndexSeg, aIndexSegTotal, sCurrSeg, aMax1, aMax2, aCount);
                }
                else
                {
                    // Nothing to do 
                }

                sortVertexList( sCurrSeg->mBeginVertex );
                sortVertexList( sCurrSeg->mEndVertex );
            }
            else
            {
                // Nothing to do 
            }

            if ( ( sCurrSeg->mBeginVertex == NULL ) && ( sCurrSeg->mEndVertex == NULL ) )
            {
                if ( sIsFirst == ID_TRUE )
                {
                    determineInside( aIndexSeg, aIndexSegTotal, sCurrSeg, aMax1, aMax2, aCount );
                    sIsFirst = ID_FALSE;
                }
                else
                {
                    sCurrSeg->mLabel = ( stdUtils::getPrevSeg( sCurrSeg ) )->mLabel;
                }
            }
            else
            {
                // Nothing to do
            }

            if ( ( sCurrSeg->mBeginVertex != NULL ) && ( sCurrSeg->mEndVertex == NULL ) )
            {
                if ( ( sCurrSeg->mParent->mReverse == ST_NOT_REVERSE ) ||
                     ( sIsFirst == ID_TRUE ) )
                {
                    determineInside( aIndexSeg, aIndexSegTotal, sCurrSeg, aMax1, aMax2, aCount );
                    sIsFirst = ID_FALSE;
                }
                else
                {
                    sCurrSeg->mLabel = ( stdUtils::getPrevSeg( sCurrSeg ) )->mLabel;
                }
                
                sortVertexList( sCurrSeg->mBeginVertex );
            }
            else
            {
                // Nothing to do
            }

            if ( ( sCurrSeg->mEndVertex != NULL ) && ( sCurrSeg->mBeginVertex == NULL ) )
            {
                if ( ( sCurrSeg->mParent->mReverse == ST_REVERSE ) || 
                     ( sIsFirst == ID_TRUE ) )
                {
                    determineInside( aIndexSeg, aIndexSegTotal, sCurrSeg, aMax1, aMax2, aCount );
                    sIsFirst = ID_FALSE;                   
                }
                else
                {
                    sCurrSeg->mLabel = ( stdUtils::getPrevSeg( sCurrSeg ) )->mLabel;
                }

                sortVertexList( sCurrSeg->mEndVertex );
            }
            else
            {
                // Nothing to do
            }

            sCurrSeg = stdUtils::getNextSeg( sCurrSeg );

        }while ( sCurrSeg != aRingSegList[i] );
    }

    return IDE_SUCCESS;
}

/***********************************************************************
 * Description :
 * ��� ���� �������� stdLinearRing2D�� �����Ѵ�.
 * 
 * Segment*         aFirstSeg : ���� ù ��° ���׸�Ʈ
 * UInt             aPointCnt : ���� ����Ʈ ��
 * stdLinearRing2D* aRes      : ���
 * UInt             aFence    : stdLinearRing�� ���� �� �ִ� �ִ� ũ��
 **********************************************************************/
IDE_RC stdPolyClip::chainToRing( Segment*         aFirstSeg,
                                 UInt             aPointCnt,
                                 stdLinearRing2D* aRes, 
                                 UInt             aFence )
{
    stdPoint2D* sPoint;
    Segment*    sCurrSeg;

    IDE_TEST_RAISE( (aPointCnt + 1) * sizeof( stdPoint2D ) > aFence, ERR_SIZE_ERROR );
 
    STD_N_POINTS(aRes) = aPointCnt + 1;
    sPoint             = STD_FIRST_PT2D(aRes);

    sCurrSeg = aFirstSeg;
    do
    {
        if ( sCurrSeg->mParent->mReverse == ST_NOT_REVERSE ) 
        {
            idlOS::memcpy( sPoint, &(sCurrSeg->mStart), STD_PT2D_SIZE );
        }
        else
        {
            idlOS::memcpy( sPoint, &(sCurrSeg->mEnd), STD_PT2D_SIZE );
        }

        sPoint++;
        sCurrSeg = stdUtils::getNextSeg( sCurrSeg );

    }while ( sCurrSeg != aFirstSeg );

    // To fix BUG-33472 CodeSonar warining

    if ( sPoint != STD_FIRST_PT2D(aRes))
    {    
        idlOS::memcpy( sPoint, STD_FIRST_PT2D(aRes), STD_PT2D_SIZE );
    }
    else
    {
        // Nothing to do
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_SIZE_ERROR );
    {
        IDE_SET(ideSetErrorCode(stERR_ABORT_OBJECT_BUFFER_OVERFLOW ));
    } 

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description :
 * Clip ����� �������� Geometry�� �����Ѵ�.
 * 
 * ResRingList*       aResRingList : Clip �� �� ����Ʈ 
 * stdGeometryHeader* aRes         : Geometry
 * UInt               aFence       : Geometry�� �ִ� ũ��
 **********************************************************************/
IDE_RC stdPolyClip::makeGeometryFromRings( ResRingList*       aResRingList, 
                                           stdGeometryHeader* aRes, 
                                           UInt               aFence )
{
    stdMultiPolygon2DType* sMultiPolygon;
    stdPolygon2DType*      sPolygon;
    stdLinearRing2D*       sRing;
    stdMBR                 sTotalMbr;
    UInt                   sPolygonCount = 0;
    Ring*                  sResRing;

    if ( aResRingList->mRingCnt <= 0 )
    {
        stdUtils::makeEmpty(aRes);
    }
    else
    {
        stdUtils::setType( aRes, STD_MULTIPOLYGON_2D_TYPE );

        sMultiPolygon        = (stdMultiPolygon2DType*) aRes;
        sMultiPolygon->mSize = STD_MPOLY2D_SIZE;
        sPolygon             = STD_FIRST_POLY2D(sMultiPolygon);
        sResRing             = aResRingList->mBegin;
        sTotalMbr            = sResRing->mMBR;

        while ( sResRing != NULL )
        {
            stdUtils::setType( (stdGeometryHeader*) sPolygon, STD_POLYGON_2D_TYPE );

            sPolygon->mSize      = STD_POLY2D_SIZE;
            sPolygon->mMbr       = sResRing->mMBR;
            sPolygon->mNumRings  = 1;
            sRing                = STD_FIRST_RN2D(sPolygon);

            stdUtils::mergeOrMBR( &sTotalMbr, &sTotalMbr, &(sResRing->mMBR) );

            IDE_TEST( chainToRing( sResRing->mFirstSeg, 
                                   sResRing->mPointCnt, 
                                   sRing, 
                                   aFence - sMultiPolygon->mSize ) 
                      != IDE_SUCCESS);

            sPolygon->mSize += STD_RN2D_SIZE + (STD_PT2D_SIZE * STD_N_POINTS(sRing));

            sResRing = sResRing->mNext;
            
            while ( sResRing != NULL )
            {
                /* ���θ��� �ִ� ��� �����￡ ���θ����� �߰��Ѵ�. */
                if ( sResRing->mOrientation == ST_CLOCKWISE )
                {
                    sRing = STD_NEXT_RN2D(sRing);

                    IDE_TEST( chainToRing( sResRing->mFirstSeg, 
                                           sResRing->mPointCnt, 
                                           sRing, 
                                           aFence - sMultiPolygon->mSize - sPolygon->mSize ) 
                              != IDE_SUCCESS );

                    sPolygon->mSize += STD_RN2D_SIZE + (STD_PT2D_SIZE * STD_N_POINTS(sRing));
                    sPolygon->mNumRings++;

                    sResRing = sResRing->mNext;
                }
                else
                {
                    break;
                }
            }
            
            sMultiPolygon->mSize += sPolygon->mSize;
            sPolygonCount++;

            sPolygon = STD_NEXT_POLY2D(sPolygon);
        }
        sMultiPolygon->mMbr        = sTotalMbr;
        sMultiPolygon->mNumObjects = sPolygonCount;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

void stdPolyClip::determineInside( Segment** aIndexSeg, 
                                   UInt      aIndexSegTotal, 
                                   Segment*  aSeg, 
                                   UInt      aMax1, 
                                   UInt      aMax2,
                                   UChar*    aCount )
{
    idBool sIsInside;

    if ( aCount == NULL )
    {
        if ( aSeg->mParent->mPolygonNum < aMax1 )
        {
            sIsInside = stdUtils::isRingInSide( aIndexSeg, 
                                                aIndexSegTotal, 
                                                aSeg, 
                                                aMax1, 
                                                aMax1 + aMax2 - 1,
                                                aCount );

            aSeg->mLabel = ( sIsInside == ID_TRUE ) ? 
                             ST_SEG_LABEL_INSIDE :
                             ST_SEG_LABEL_OUTSIDE;
        }
        else
        {
            sIsInside = stdUtils::isRingInSide( aIndexSeg, 
                                                aIndexSegTotal, 
                                                aSeg, 
                                                0, 
                                                aMax1 - 1,
                                                aCount );

            aSeg->mLabel = ( sIsInside == ID_TRUE ) ? 
                             ST_SEG_LABEL_INSIDE : 
                             ST_SEG_LABEL_OUTSIDE;
        }
    }
    else
    {
        sIsInside = stdUtils::isRingInSide( aIndexSeg,
                                            aIndexSegTotal,
                                            aSeg,
                                            0,
                                            aMax2,
                                            aCount );

        aSeg->mLabel = ( sIsInside == ID_TRUE ) ? 
                         ST_SEG_LABEL_INSIDE : 
                         ST_SEG_LABEL_OUTSIDE;
    }

    return ;    
}

void stdPolyClip::determineShare( Segment* aSeg )
{
    VertexEntry* sCurrEntry;
    VertexEntry* sStart      = aSeg->mBeginVertex->mList;
   
    if ( ( aSeg->mLabel != ST_SEG_LABEL_SHARED1 ) && 
         ( aSeg->mLabel != ST_SEG_LABEL_SHARED2 ) )
    {            
        sCurrEntry = sStart;
        do
        {
            if ( ( sCurrEntry->mSeg != aSeg )                             &&
                 ( sCurrEntry->mSeg->mBeginVertex == aSeg->mBeginVertex ) &&
                 ( sCurrEntry->mSeg->mEndVertex == aSeg->mEndVertex ) )

            {
                if ( sCurrEntry->mSeg->mParent->mReverse == aSeg->mParent->mReverse )
                {
                    aSeg->mLabel             = ST_SEG_LABEL_SHARED1;
                    sCurrEntry->mSeg->mLabel = ST_SEG_LABEL_SHARED1;
                }
                else
                {
                    aSeg->mLabel             = ST_SEG_LABEL_SHARED2;
                    sCurrEntry->mSeg->mLabel = ST_SEG_LABEL_SHARED2;
                }
            }
            else
            {
                // Nothing to do 
            }
            sCurrEntry = sCurrEntry->mNext;

        }while ( ( sCurrEntry != NULL ) && ( sCurrEntry != sStart ) );
    }
}

// BUG-33436
/***********************************************************************
 * Description:
 *   �Է¹��� ���׸�Ʈ�� ������ �������� �Ǻ��Ѵ�.
 *
 *   Direction aDir1 : 1�� ����
 *   Direction aDir2 : 2�� ����
 **********************************************************************/
idBool stdPolyClip::isSameDirection( Direction aDir1, 
                                     Direction aDir2 )
{
    idBool sRet;

    if ( aDir1 == aDir2 )
    {
        sRet = ID_TRUE;
    }
    else
    {
        sRet = ID_FALSE;
    }

    return sRet;
}

// BUG-33634
Direction stdPolyClip::getDirection( Segment* aSeg ) 
{
    Direction sRet;

    if( aSeg->mParent->mReverse == ST_NOT_REVERSE )
    {
        sRet = ST_SEG_DIR_FORWARD;
    }
    else
    {
        sRet = ST_SEG_DIR_BACKWARD;
    }

    return sRet;
}

/***********************************************************************
* Description:
*   ���� ������ �ùٸ��� �˻��ϰ�, �ƴѰ�� �ݴ�� �����´�. 
*
*   aRingSegList   : �� ���� ù ���׸�Ʈ ����Ʈ
*   aRingCount     : ���� ����
*   aIndexSeg      : �� ü���� ù ���׸�Ʈ ����Ʈ
*   aIndexSegTotal : ü���� ����
*   aMax1          : A (op) B �� A�� ���ϴ� �������� �� 
*   aMax2          : A (op) B �� B�� ���ϴ� �������� ��
**********************************************************************/
void stdPolyClip::adjustRingOrientation( Segment** aRingSegList, 
                                         UInt      aRingCount,
                                         Segment** aIndexSeg,
                                         UInt      aIndexSegTotal,
                                         UInt      aMax1,
                                         UInt      aMax2 )
{
    UInt     i;
    SInt     sOrientation;
    Segment* sRingSeg;
    Chain*   sChain;
    idBool   sIsInside;

    for ( i = 0 ; i < aRingCount ; i++ )
    {
        sRingSeg = aRingSegList[i];
       
        if ( sRingSeg->mParent->mPolygonNum < aMax1 )
        {
            sIsInside = stdUtils::isRingInSide( aIndexSeg, 
                                                aIndexSegTotal,
                                                sRingSeg, 
                                                0, 
                                                aMax1 - 1,
                                                NULL );

            sOrientation = ( sIsInside == ID_TRUE ) ? 
                             ST_CLOCKWISE : 
                             ST_COUNTERCLOCKWISE;
        }
        else
        {
            sIsInside = stdUtils::isRingInSide( aIndexSeg, 
                                                aIndexSegTotal, 
                                                sRingSeg, 
                                                aMax1, 
                                                aMax1 + aMax2 - 1,
                                                NULL );

            sOrientation = ( sIsInside == ID_TRUE ) ? 
                             ST_CLOCKWISE : 
                             ST_COUNTERCLOCKWISE;
        }

        if ( sRingSeg->mParent->mOrientaion != sOrientation )
        {
            /* ���� ������ �ݴ�� �ٲ۴�. 
             * �̿� ���� ���� �����ϴ� ü���� ���⵵ �����ؾ� �Ѵ�. */ 
            sChain = sRingSeg->mParent;
            do
            {
                sChain->mOrientaion = ( sChain->mOrientaion == ST_CLOCKWISE ) ?
                                        ST_COUNTERCLOCKWISE :
                                        ST_CLOCKWISE ;

                if ( sChain->mReverse == ST_NOT_REVERSE )
                {
                    sChain->mReverse = ST_REVERSE;
                }
                else
                {
                    sChain->mReverse = ST_NOT_REVERSE;
                }

                if ( sChain->mOpposite == ST_CHAIN_NOT_OPPOSITE )
                {
                    sChain->mOpposite = ST_CHAIN_OPPOSITE;
                }
                else
                {
                    sChain->mOpposite = ST_CHAIN_OPPOSITE;
                }

                sChain = sChain->mNext;

            }while ( sChain != sRingSeg->mParent );
        }
        else
        {
            // Nothing to do
        }
    }
}

void stdPolyClip::appendFirst( ResRingList* aResRingList, 
                               Ring*        aRing )
{
    aRing->mNext = aResRingList->mBegin;

    if ( aResRingList->mBegin != NULL )
    {
        aResRingList->mBegin->mPrev = aRing;
        aResRingList->mBegin        = aRing;
    }
    else
    {
        aResRingList->mBegin = aRing;
        aResRingList->mEnd   = aRing;
    }
    aResRingList->mRingCnt++;
}

void stdPolyClip::appendLast( ResRingList* aResRingList, 
                              Ring*        aRing )
{
    aRing->mPrev = aResRingList->mEnd;

    if ( aResRingList->mEnd != NULL )
    {
        aResRingList->mEnd->mNext = aRing;
        aResRingList->mEnd        = aRing;
    }
    else
    {
        aResRingList->mBegin = aRing;
        aResRingList->mEnd   = aRing;
    }
    aResRingList->mRingCnt++;
}

IDE_RC stdPolyClip::removeRingFromList( ResRingList* aResRingList, 
                                        UInt         aRingNum )
{
    Ring* sRing;

    sRing = aResRingList->mBegin;

    while ( sRing != NULL )
    {
        if ( sRing->mRingNumber == aRingNum )
        {
            break;
        }
        else
        {
            sRing = sRing->mNext;
        }
    }

    if ( sRing != NULL )
    {
        if ( sRing->mPrev != NULL )
        {
            sRing->mPrev->mNext = sRing->mNext;
        }
        else
        {
            // Nothing to do 
        }

        if ( sRing->mNext != NULL )
        {
            sRing->mNext->mPrev = sRing->mPrev;
        }
        else
        {
            // Nothing to do 
        }

        if ( aResRingList->mBegin == sRing )
        {
            aResRingList->mBegin = sRing->mNext;
        }
        else
        {
            // Nothing to do 
        }

        if ( aResRingList->mEnd == sRing )
        {
            aResRingList->mEnd = sRing->mPrev;
        }
        else
        {
            // Nothing to do 
        }
    }
    else
    {
        /* aRingNum�� �ش��ϴ� ���� ã�� �� ���� ��� */
        IDE_RAISE( ERR_UNKNOWN );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_UNKNOWN );
    {
        IDE_SET( ideSetErrorCode( stERR_ABORT_UNKNOWN_POLYGON ));
    } 
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
* Description:
* ���׸�Ʈ���� ���� ���� �ּҸ� ��ȯ�Ѵ�. aIsStart�� TRUE�̸� ���׸�Ʈ
* ���� ���� ���� ��(���׸�Ʈ�� �� ��)�� ��ȯ�ϰ�, FALSE�̸� �� ���� 
* ���� �� (���� ���׸�Ʈ�� �� ��)�� ��ȯ�ϰ� �ȴ�.
*
* Segment* aSeg     : ���׸�Ʈ
* idBool   aIsStart : ���׸�Ʈ�� �������ΰ�?
**********************************************************************/
stdPoint2D* stdPolyClip::getNextPoint( Segment* aSeg, 
                                       idBool   aIsStart )
{
    Segment*    sTmp;
    stdPoint2D* sRet;

    if ( aIsStart == ID_TRUE )
    {
        if ( aSeg->mParent->mReverse == ST_NOT_REVERSE ) 
        {
            sRet = &(aSeg->mEnd);
        }
        else
        {
            sTmp = stdUtils::getNextSeg( aSeg );

            if ( aSeg->mParent->mReverse == sTmp->mParent->mReverse ) 
            {
                sRet = &(sTmp->mStart);
            }
            else
            {
                sRet = &(sTmp->mEnd);
            }
        }
    }
    else
    {
        if ( aSeg->mParent->mReverse == ST_NOT_REVERSE )
        {
            sTmp = stdUtils::getNextSeg( aSeg );
            
            if ( aSeg->mParent->mReverse == sTmp->mParent->mReverse ) 
            {
                sRet = &(sTmp->mEnd);
            }
            else
            {
                sRet = &(sTmp->mStart);
            }
        }
        else
        {
            sRet = &(aSeg->mStart);
        }
    }

    return sRet;
}

/***********************************************************************
* Description:
* ���׸�Ʈ���� ���� ���� �ּҸ� ��ȯ�Ѵ�. aIsStart�� TRUE�̸� ���׸�Ʈ
* ���� ���� ���� ��(���� ���׸�Ʈ�� ���� ��)�� ��ȯ�ϰ�, FALSE�̸� 
* ���׸�Ʈ �� ���� ���� �� (���׸�Ʈ�� ���� ��)�� ��ȯ�ϰ� �ȴ�.
*
* Segment* aSeg     : ���׸�Ʈ
* idBool   aIsStart : ���׸�Ʈ�� �������ΰ�?
**********************************************************************/
stdPoint2D* stdPolyClip::getPrevPoint( Segment* aSeg, 
                                       idBool   aIsStart )
{
    Segment*    sTmp;
    stdPoint2D* sRet;

    if ( aIsStart == ID_TRUE )
    {
        if ( aSeg->mParent->mReverse == ST_NOT_REVERSE )
        {
            sTmp = stdUtils::getPrevSeg( aSeg );

            if ( aSeg->mParent->mReverse == sTmp->mParent->mReverse ) 
            {
                sRet = &(sTmp->mStart);
            }
            else
            {
                sRet = &(sTmp->mEnd);
            }
        }
        else
        {
            sRet =  &(aSeg->mEnd);
        }
    }
    else
    {
        if ( aSeg->mParent->mReverse == ST_NOT_REVERSE )
        {
            sRet =  &(aSeg->mStart);
        }
        else
        {
            sTmp = stdUtils::getPrevSeg( aSeg );

            if ( aSeg->mParent->mReverse == sTmp->mParent->mReverse )
            {
                sRet = &(sTmp->mEnd);
            }
            else
            {
                sRet = &(sTmp->mStart);
            }
        }
    }

    return sRet;
}

/***********************************************************************
 * Description :
 *   Clip ��� ���� ������ �����Ѵ�. ���� ���� �ܺθ����� �����ϰ�, 
 *   ���θ��� �ڽ��� ���ԵǾ�� �ϴ� �ܺθ��� �ڿ� ��ġ�ϵ��� ����Ʈ�� 
 *   �����Ѵ�. 
 *   Ring�� �θ�� �ڽ��� ������ �� �ִ� ���� ���� �ܺθ��� �ǹ��Ѵ�.
 *
 *   ex) �ʱ� aResRingList
 *   - �ܺθ�1->�ܺθ�2->�ܺθ�3->���θ�1->���θ�2
 *   
 *   ���� ��
 *   - �ܺθ�1->���θ�1->���θ�2->�ܺθ�2->�ܺθ�3
 *
 *   ResRingList* aResRingList   : Clip ��� ���� ����Ʈ
 *   Segment**    aIndexSeg,     : Clip ��� ���� �ε��� ���׸�Ʈ ����Ʈ
 *   UInt         aIndexSegTotal : �ε��� ���׸�Ʈ�� �� 
 **********************************************************************/
IDE_RC stdPolyClip::sortRings( ResRingList* aResRingList,
                               Segment**    aIndexSeg, 
                               UInt         aIndexSegTotal )
{
    Ring* sCurrRing = aResRingList->mBegin->mNext;
    Ring* sCompRing;
    Ring* sTmp;

    /* �ܺθ� */
    while ( ( sCurrRing != NULL ) )
    {
        if ( sCurrRing->mOrientation != ST_COUNTERCLOCKWISE )
        {
            break;
        }
        else
        {
            // Nothing to do
        }

        sCompRing = aResRingList->mBegin;

        while ( sCompRing != sCurrRing )
        {
            if ( ( sCurrRing->mParent == sCompRing->mParent )    &&
                 ( stdUtils::isMBRIntersects( &(sCurrRing->mMBR), 
                                              &(sCompRing->mMBR) )
                   == ID_TRUE ) )
            {
                /* �θ� ����, �� ���� MBR�� ��ġ�� ��쿡�� �� ���� ���԰��踦
                 * ����Ѵ�. �θ� �ٸ��ų�, MBR�� ��ġ�� �ʴ� ��쿡�� 
                 * �� �� ���̿� ���԰��谡 ���� �� ����. */

                if ( stdUtils::isRingInSide( aIndexSeg,
                                             aIndexSegTotal,
                                             sCurrRing->mFirstSeg,
                                             sCompRing->mRingNumber,
                                             sCompRing->mRingNumber,
                                             NULL )
                     == ID_TRUE ) 
                {
                    sCurrRing->mParent = sCompRing;
                }
                else
                {
                    if ( stdUtils::isRingInSide( aIndexSeg,
                                                 aIndexSegTotal,
                                                 sCompRing->mFirstSeg,
                                                 sCurrRing->mRingNumber,
                                                 sCurrRing->mRingNumber,
                                                 NULL )
                         == ID_TRUE )
                    {
                        sCompRing->mParent = sCurrRing;
                    }
                    else
                    {
                        /* Nothing to do */
                    }
                }
            }
            else
            {
                /* Nothing to do */
            }
            sCompRing = sCompRing->mNext;
        }

        sTmp = sCurrRing->mNext;

        if ( sCurrRing->mParent == NULL )
        {
            /* �θ� ���ٸ� ����Ʈ�� �� ������ ������. */
            if ( sTmp != NULL )
            {
                sTmp->mPrev = sCurrRing->mPrev;
            }
            else
            {
                // Nothing to do
            }

            sCurrRing->mPrev->mNext     = sTmp;

            sCurrRing->mPrev            = NULL;
            sCurrRing->mNext            = aResRingList->mBegin;
            
            aResRingList->mBegin->mPrev = sCurrRing;
            aResRingList->mBegin        = sCurrRing;
        }
        else
        {
            /* �θ� �ִٸ� �θ� �� �ڷ� ������. */
            if ( sCurrRing->mPrev != sCurrRing->mParent )
            {
                if ( sTmp != NULL )
                {
                    sTmp->mPrev = sCurrRing->mPrev;
                }
                else
                {
                    // Nothing to do 
                }

                sCurrRing->mPrev->mNext = sTmp;

                sCurrRing->mPrev        = sCurrRing->mParent;
                sCurrRing->mNext        = sCurrRing->mParent->mNext;

                sCurrRing->mParent->mNext = sCurrRing;
            }
            else
            {
                // Nothing to do
            }
        }
        sCurrRing = sTmp;
    }

    /* ���θ� */
    while ( sCurrRing != NULL )
    {
        sCompRing = aResRingList->mBegin;

        while ( sCompRing != sCurrRing )
        {
            if ( ( sCurrRing->mParent      == sCompRing->mParent  ) && 
                 ( sCompRing->mOrientation == ST_COUNTERCLOCKWISE ) &&
                 ( stdUtils::isMBRIntersects( &(sCurrRing->mMBR), 
                                              &(sCompRing->mMBR) )
                  == ID_TRUE ) )
            {
                if ( stdUtils::isRingInSide( aIndexSeg,
                                             aIndexSegTotal,
                                             sCurrRing->mFirstSeg,
                                             sCompRing->mRingNumber,
                                             sCompRing->mRingNumber,
                                             NULL )
                     == ID_TRUE ) 
                {
                    sCurrRing->mParent = sCompRing;
                }
                else
                {
                    // Nothing to do
                }

            }
            else
            {
                // Nothing to do
            }
            
            sCompRing = sCompRing->mNext;
        }

        sTmp = sCurrRing->mNext;


        /* �θ��� ���� ���θ��� ������ �� ����.*/
        IDE_TEST_RAISE( sCurrRing->mParent == NULL, ERR_UNKNOWN ); 
        
        if ( sCurrRing->mPrev != sCurrRing->mParent )
        {
            if ( sTmp != NULL )
            {
                sTmp->mPrev = sCurrRing->mPrev;
            }
            else
            {
                // Nothing to do
            }

            sCurrRing->mPrev->mNext   = sTmp;

            sCurrRing->mPrev          = sCurrRing->mParent;
            sCurrRing->mNext          = sCurrRing->mParent->mNext;

            sCurrRing->mParent->mNext = sCurrRing;
        }
        else
        {
            // Nothing to do
        }

        sCurrRing = sTmp;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_UNKNOWN )
    {
        IDE_SET( ideSetErrorCode( stERR_ABORT_UNKNOWN_POLYGON ));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description :
 *   Clip ������� ������ġ�� �ִ� �� ã�Ƽ� aResPrimInterSeg�� ����Ѵ�.
 * 
 *   iduMemory*     aQmxMem        
 *   Segment**      aIndexSeg,       : Clip ��� ���� �ε��� ���׸�Ʈ ����Ʈ
 *   UInt           aIndexSegTotal   : �ε��� ���׸�Ʈ�� ��
 *   PrimInterSeg** aResPrimInterSeg : ������ġ�� �߻��� ���� ����Ʈ 
 **********************************************************************/
IDE_RC stdPolyClip::detectSelfTouch( iduMemory*     aQmxMem,
                                     Segment**      aIndexSeg,
                                     UInt           aIndexSegTotal,
                                     PrimInterSeg** aResPrimInterSeg )
{
    UInt             i;
    UInt             sIndexSegTotal    = aIndexSegTotal;
    ULong            sReuseSegCount    = 0;
    SInt             sIntersectStatus;
    Segment**        sIndexSeg         = aIndexSeg;
    Segment*         sCurrSeg          = NULL;
    Segment*         sCmpSeg;    
    Segment**        sReuseSeg;
    Segment**        sTempIndexSeg;    
    iduPriorityQueue sPQueue;
    idBool           sOverflow         = ID_FALSE;
    idBool           sUnderflow        = ID_FALSE;
    Segment*         sCurrNext;
    Segment*         sCurrPrev;
    stdPoint2D       sInterResult[4];
    SInt             sInterCount;
    stdPoint2D*      sStartPt[2];
    stdPoint2D*      sNextPt[2];

    
    IDE_TEST( sPQueue.initialize( aQmxMem,
                                  sIndexSegTotal,
                                  ID_SIZEOF(Segment*),
                                  cmpSegment )
              != IDE_SUCCESS);
    
    IDE_TEST( aQmxMem->alloc( ID_SIZEOF(Segment*) * sIndexSegTotal,
                              (void**)  &sReuseSeg )
              != IDE_SUCCESS);
        
    for ( i = 0; i < sIndexSegTotal; i++ )
    {
        sPQueue.enqueue( sIndexSeg++, &sOverflow);
        IDE_TEST_RAISE( sOverflow == ID_TRUE, ERR_ABORT_ENQUEUE_ERROR );
    }

    while(1)
    {
        sPQueue.dequeue( (void*) &sCurrSeg, &sUnderflow );

        sCurrNext = stdUtils::getNextSeg(sCurrSeg);
        sCurrPrev = stdUtils::getPrevSeg(sCurrSeg);
        
        if ( sUnderflow == ID_TRUE )
        {
            break;
        }
        else
        {
            // Nothing to do 
        }

        sReuseSegCount = 0;
        sTempIndexSeg  = sReuseSeg;  

        while(1)
        {      
            sPQueue.dequeue( (void*) &sCmpSeg, &sUnderflow );

            if ( sUnderflow == ID_TRUE )
            {
                break;
            }
            else
            {
                // Nothing to do
            }


            sReuseSeg[sReuseSegCount++] = sCmpSeg;
            
            if ( sCmpSeg->mStart.mX > sCurrSeg->mEnd.mX )
            {
                /* �� �� ���� ���� ���뿡 �־�� �Ѵ�. */
                break;                
            }
            else
            {
                // Nothing to do
            }
            
            do
            {
                /*
                  ���⼭ intersect�� ���⿡ ���� ������ �־� �ĳ��� �ִ�.                  
                 */
                if ( ( sCurrNext != sCmpSeg ) && ( sCurrPrev != sCmpSeg ) )
                {
                    IDE_TEST( stdUtils::intersectCCW4Func( sCurrSeg->mStart,
                                                           sCurrSeg->mEnd,
                                                           sCmpSeg->mStart,
                                                           sCmpSeg->mEnd,
                                                           &sIntersectStatus,
                                                           &sInterCount,
                                                           sInterResult)
                              != IDE_SUCCESS );

                    switch( sIntersectStatus )
                    {
                        case ST_POINT_TOUCH:
                            /* BUG-33436 
                               ������ġ�� �߻��ϴ� ������ ���� ������ ���׸�Ʈ�� 
                               �߻��� ������ ������ ������ ���׸�Ʈ�� ����Ѵ�.  */
                            if ( sCurrSeg->mParent->mReverse == ST_NOT_REVERSE )
                            {
                                sStartPt[0] = &(sCurrSeg->mStart);
                                sNextPt[0]  = &(sCurrSeg->mEnd);
                            }
                            else
                            {
                                sStartPt[0] = &(sCurrSeg->mEnd);
                                sNextPt[0]  = &(sCurrSeg->mStart);
                            }

                            if ( sCmpSeg->mParent->mReverse == ST_NOT_REVERSE )
                            {
                                sStartPt[1] = &(sCmpSeg->mStart);
                                sNextPt[1]  = &(sCmpSeg->mEnd);
                            }
                            else
                            {
                                sStartPt[1] = &(sCmpSeg->mEnd);
                                sNextPt[1]  = &(sCmpSeg->mStart);
                            }

                            if ( stdUtils::isSamePoints2D( sNextPt[0], sStartPt[1] )
                                 == ID_TRUE )
                            {
                                IDE_TEST( stdUtils::addPrimInterSeg( aQmxMem,
                                                                     aResPrimInterSeg,
                                                                     sCurrSeg,
                                                                     sCmpSeg,
                                                                     sIntersectStatus,
                                                                     sInterCount,
                                                                     sInterResult )
                                          != IDE_SUCCESS );
                            }
                            else
                            {
                                // Nothing to do
                            }

                            if ( stdUtils::isSamePoints2D( sNextPt[1], sStartPt[0] ) 
                                 == ID_TRUE )
                            {
                                IDE_TEST( stdUtils::addPrimInterSeg( aQmxMem,
                                                                     aResPrimInterSeg,
                                                                     sCmpSeg,
                                                                     sCurrSeg,
                                                                     sIntersectStatus,
                                                                     sInterCount,
                                                                     sInterResult )
                                          != IDE_SUCCESS );
                            }
                            else
                            {
                                // Nothing to do
                            }
                            break;

                        case ST_TOUCH:                       
                        case ST_INTERSECT:
                        case ST_SHARE:
                            // Clip ������� INTERSECT, SHARE�� �߻��� �� ����. 
                            IDE_RAISE( ERR_UNKNOWN );
                            break;

                        case ST_NOT_INTERSECT:
                            break;                        
                    }
                }
                else
                {
                    // Nothing to do 
                }

                sCmpSeg = sCmpSeg->mNext;

                if ( sCmpSeg == NULL )
                {
                    break;                    
                }
            
                /* ������ �����Ѵ�. */

            }while( sCmpSeg->mStart.mX <= sCurrSeg->mEnd.mX );            
        }

        /* ������ ���� �Ѵ�. */
    
        
        for ( i =0; i < sReuseSegCount ; i++)
        {
            sPQueue.enqueue( sTempIndexSeg++, &sOverflow);
            IDE_TEST_RAISE( sOverflow == ID_TRUE, ERR_ABORT_ENQUEUE_ERROR );
            /* Overflow �˻� */
        }

        if ( sCurrSeg->mNext != NULL )
        {
            sPQueue.enqueue( &(sCurrSeg->mNext), &sOverflow);
            IDE_TEST_RAISE( sOverflow == ID_TRUE, ERR_ABORT_ENQUEUE_ERROR );
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_UNKNOWN )
    {
        IDE_SET( ideSetErrorCode( stERR_ABORT_UNKNOWN_POLYGON ));
    }
    IDE_EXCEPTION( ERR_ABORT_ENQUEUE_ERROR )
    {
        IDE_SET( ideSetErrorCode( stERR_ABORT_UNEXPECTED_ERROR,
                                  "stdPolyClip::detectSelfTouch",
                                  "enqueue overflow" ));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC stdPolyClip::breakRing( iduMemory*    aQmxMem, 
                               ResRingList*  aResRingList,
                               PrimInterSeg* aResPrimInterSeg )
{
    UInt          i;
    PrimInterSeg* sPrim         = aResPrimInterSeg;
    Segment*      sSeg1;
    Segment*      sSeg2;
    Segment*      sMaxSeg;
    Segment*      sFirstSeg[2];
    Ring*         sRing;
    UInt          sPointCnt;
    stdPoint2D*   sNextPt;
    stdPoint2D*   sMaxPt;
    Chain*        sChain1;
    Chain*        sChain2;
    Chain*        sNewChain1;
    Chain*        sNewChain2;
    stdMBR        sTotalMbr;
    stdMBR        sSegmentMbr;

    while ( sPrim != NULL )
    {
        sSeg1   = stdUtils::getNextSeg( sPrim->mFirst );
        sSeg2   = stdUtils::getPrevSeg( sPrim->mSecond );

        sChain1 = sPrim->mFirst->mParent;
        sChain2 = sPrim->mSecond->mParent;

        if ( ( sSeg1 == stdUtils::getPrevSeg( sSeg2 ) ) ||
             ( sSeg1 == stdUtils::getNextSeg( sSeg2 ) ) ||
             ( sChain1->mPolygonNum != sChain2->mPolygonNum ) )
        {
            /* sSeg2�� sSeg1�� ����,���� ���׸�Ʈ�� �ƴϾ�� �Ѵ�.
             * ���� �׷��ٸ� �̹� ������ �Ͼ �� �̹Ƿ�
             * �̰��� �����ϰ� �������� �Ѿ��. 
             * �� ���׸�Ʈ�� �� ��ȣ�� �ٸ� ��쿡�� �������� �Ѿ��. */

            sPrim = sPrim->mNext;
            continue;
        }
        else
        {
            // Nothing to do
        }

        /* Clip ������� ������ġ�� �ִ� ���� �����Ѵ�. */
        IDE_TEST( removeRingFromList( aResRingList, sChain1->mPolygonNum )
                  != IDE_SUCCESS );

        /* ������ġ�� �ִ� ������ ���� ���׸�Ʈ�� �ٷ� �� ���� ���׸�Ʈ��
         * �θ�(ü��)�� ���� ���, Chain�� �и��ؾ� �Ѵ�. */
        if ( sSeg1->mParent == sChain1 )
        {
            IDE_TEST( aQmxMem->alloc( ID_SIZEOF(Chain), 
                                      (void**) &sNewChain1 )
                      != IDE_SUCCESS );

            if ( sChain1->mReverse == ST_NOT_REVERSE )
            {
                stdUtils::initChain( sSeg1,
                                     sNewChain1, 
                                     sChain1->mVaritation,
                                     sChain1->mReverse,
                                     0,                         /* Ring ��ȣ, ��� ���� */
                                     aResRingList->mRingCnt,
                                     sChain1->mOrientaion,
                                     sChain1->mOpposite );

                sNewChain1->mEnd          = sChain1->mEnd;

                sChain1->mEnd             = sSeg1->mPrev;
                sChain1->mEnd->mNext      = NULL;

                sNewChain1->mBegin->mPrev = NULL;

                while ( sSeg1 != NULL )
                {
                    sSeg1->mParent = sNewChain1;
                    sSeg1          = sSeg1->mNext;
                }
            }
            else
            {
                stdUtils::initChain( sChain1->mBegin, 
                                     sNewChain1,
                                     sChain1->mVaritation,
                                     sChain1->mReverse,
                                     0,
                                     aResRingList->mRingCnt,
                                     sChain1->mOrientaion,
                                     sChain1->mOpposite );

                sNewChain1->mEnd        = sSeg1;

                sChain1->mBegin         = sSeg1->mNext;
                sChain1->mBegin->mPrev  = NULL;

                sNewChain1->mEnd->mNext = NULL;

                while ( sSeg1 != NULL )
                {
                    sSeg1->mParent = sNewChain1;
                    sSeg1          = sSeg1->mPrev;
                }
            }

            sNewChain1->mNext = sChain1->mNext;
        }
        else
        {
            sNewChain1 = sSeg1->mParent;
        }
        sChain1->mNext = NULL;

        /* ������ġ�� �ִ� ������ ���� ���׸�Ʈ�� �ٷ� �� ���� ���׸�Ʈ��
         * �θ�(ü��)�� ���� ���, Chain�� �и��ؾ� �Ѵ�. */
        if ( sSeg2->mParent == sChain2 )
        {
            IDE_TEST( aQmxMem->alloc( ID_SIZEOF(Chain), 
                                      (void**) &sNewChain2 )
                      != IDE_SUCCESS );

            if ( sChain2->mReverse == ST_NOT_REVERSE )
            {
                stdUtils::initChain( sChain2->mBegin,
                                     sNewChain2, 
                                     sChain2->mVaritation,
                                     sChain2->mReverse,
                                     0,
                                     aResRingList->mRingCnt,
                                     sChain2->mOrientaion,
                                     sChain2->mOpposite );
                
                sNewChain2->mEnd        = sSeg2;
                
                sChain2->mBegin         = sSeg2->mNext;
                sChain2->mBegin->mPrev  = NULL;

                sNewChain2->mEnd->mNext = NULL;

                while ( sSeg2 != NULL )
                {
                    sSeg2->mParent = sNewChain2;
                    sSeg2          = sSeg2->mPrev;
                }
            }
            else
            {
                stdUtils::initChain( sSeg2,
                                     sNewChain2,
                                     sChain2->mVaritation,
                                     sChain2->mReverse,
                                     0,
                                     aResRingList->mRingCnt,
                                     sChain2->mOrientaion,
                                     sChain2->mOpposite );

                sNewChain2->mEnd          = sChain2->mEnd;
                
                sChain2->mEnd             = sSeg2->mPrev;
                sChain2->mEnd->mNext      = NULL;

                sNewChain2->mBegin->mPrev = NULL;

                while ( sSeg2 != NULL )
                {
                    sSeg2->mParent = sNewChain2;
                    sSeg2          = sSeg2->mNext;
                }
            }

            sNewChain2->mPrev = sChain2->mPrev;
        }
        else
        {
            sNewChain2 = sSeg2->mParent;
        }
        sChain2->mPrev = NULL;

        /* �� ���� ������ �и��ǵ��� ü���� �����Ѵ�. */
        sChain1->mNext = sChain2;
        sChain2->mPrev = sChain1;

        sNewChain1->mPrev = sNewChain2;
        sNewChain2->mNext = sNewChain1;

        if ( sNewChain1->mNext == sChain2 )
        {
            sNewChain1->mNext = sNewChain2;
        }
        else
        {
            // Nothing to do 
        }

        if ( sNewChain2->mPrev == sChain1 )
        {
            sNewChain2->mPrev = sNewChain1;
        }
        else
        {
            // Nohting to do   
        }

        sNewChain1->mNext->mPrev = sNewChain1;
        sNewChain2->mPrev->mNext = sNewChain2;

        /* �и��� �� ���� MBR�� ������ �ٽ� ����ϰ�, 
         * Clip ����� �ٽ� ��� �Ѵ�. */

        sFirstSeg[0] = sChain1->mBegin;
        sFirstSeg[1] = sNewChain1->mBegin;

        for ( i = 0 ; i < 2 ; i++ )
        {
            sSeg1     = sFirstSeg[i];
            sPointCnt = 0;
            sMaxSeg   = sSeg1;

            stdUtils::initMBR( &sTotalMbr );

            if ( sSeg1->mParent->mReverse == ST_NOT_REVERSE )
            {
                sMaxPt = &sSeg1->mStart;
            }
            else
            {
                sMaxPt = &sSeg1->mEnd;
            }

            do
            {
                if ( sSeg1->mParent->mReverse == ST_NOT_REVERSE )
                {
                    sNextPt = &sSeg1->mEnd;
                }
                else
                {
                    sNextPt = &sSeg1->mStart;
                }

                if ( sMaxPt->mY < sNextPt->mY )
                {
                    sMaxSeg = sSeg1;
                    sMaxPt  = sNextPt;
                }
                else
                {
                    if ( sMaxPt->mY == sNextPt->mY )
                    {
                        if ( sMaxPt->mX <= sNextPt->mX )
                        {
                            sMaxSeg = sSeg1;
                            sMaxPt  = sNextPt;
                        }
                        else
                        {
                            // Nothing to do
                        }
                    }
                    else
                    {
                        // Nothing to do
                    }
                }

                sSegmentMbr.mMinX = sSeg1->mStart.mX;
                sSegmentMbr.mMaxX = sSeg1->mEnd.mX;
                if( sSeg1->mStart.mY < sSeg1->mEnd.mY )
                {
                    sSegmentMbr.mMinY = sSeg1->mStart.mY;
                    sSegmentMbr.mMaxY = sSeg1->mEnd.mY;
                }
                else
                {
                    sSegmentMbr.mMinY = sSeg1->mEnd.mY;
                    sSegmentMbr.mMaxY = sSeg1->mStart.mY;
                }

                stdUtils::mergeOrMBR( &sTotalMbr, &sTotalMbr, &sSegmentMbr );
               
                sPointCnt++;
                sSeg1 = stdUtils::getNextSeg( sSeg1 );

            }while ( sSeg1 != sFirstSeg[i] );

            sChain1 = sFirstSeg[i]->mParent;

            do
            {
                sChain1->mPolygonNum = aResRingList->mRingCnt;
                sChain1              = sChain1->mNext;
            }while ( sChain1 != sFirstSeg[i]->mParent );


            IDE_TEST( aQmxMem->alloc( ID_SIZEOF(Ring),
                                      (void**) &sRing )
                      != IDE_SUCCESS );

            sRing->mPrev       = NULL;
            sRing->mNext       = NULL;
            sRing->mParent     = NULL;
            sRing->mFirstSeg   = sFirstSeg[i];
            sRing->mRingNumber = aResRingList->mRingCnt;
            sRing->mPointCnt   = sPointCnt;
            stdUtils::copyMBR( &(sRing->mMBR), &sTotalMbr );

            if ( sMaxSeg->mParent->mReverse == ST_NOT_REVERSE )
            {
                sRing->mOrientation = stdUtils::CCW( *getPrevPoint( sMaxSeg, ID_FALSE ),
                                                     sMaxSeg->mEnd,
                                                     *getNextPoint ( sMaxSeg, ID_FALSE) );
            }
            else
            {
                sRing->mOrientation = stdUtils::CCW( *getPrevPoint( sMaxSeg, ID_TRUE ),
                                                     sMaxSeg->mStart,
                                                     *getNextPoint( sMaxSeg, ID_TRUE ) );
            }

            if ( sRing->mOrientation == ST_COUNTERCLOCKWISE )
            {
                appendFirst( aResRingList, sRing );
            }
            else
            {
                appendLast( aResRingList, sRing );
            }
        }

        sPrim = sPrim->mNext;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC stdPolyClip::unionPolygons2D( iduMemory*          aQmxMem,
                                     stdGeometryHeader** aGeoms,
                                     SInt                aCnt,
                                     stdGeometryHeader*  aRet,
                                     UInt                aFence )
{
    SInt i;
    idlOS::memcpy( aRet, aGeoms[0], STD_GEOM_SIZE(aGeoms[0]) );
    for ( i = 1 ; i < aCnt ; i++ )
    {
        IDE_TEST( stfAnalysis::getUnionAreaArea2D4Clip( aQmxMem,
                                                        (stdGeometryType*) aRet,
                                                        (stdGeometryType*) aGeoms[i],
                                                        aRet,
                                                        aFence )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;
    
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}
