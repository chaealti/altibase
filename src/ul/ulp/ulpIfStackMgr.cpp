/**
 *  Copyright (c) 1999~2017, Altibase Corp. and/or its affiliates. All rights reserved.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License, version 3,
 *  as published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <ulpIfStackMgr.h>


ulpPPifstackMgr::ulpPPifstackMgr()
{
    mIndex = IFSTACKINITINDEX;
}


SInt ulpPPifstackMgr::ulpIfgetIndex()
{
    return mIndex;
}


// #elif, #endif, #else ���� ���� ���� ��� ���� �˻�.
idBool ulpPPifstackMgr::ulpIfCheckGrammar( ulpPPiftype aIftype )
{

    switch( aIftype )
    {
        case PP_ELIF:
            if( (mIfstack[mIndex].mType == PP_ELSE)
                 || (mIndex == IFSTACKINITINDEX) )
            {
                return ID_FALSE;
            }
            break;
        case PP_ENDIF:
        case PP_ELSE:
            if( mIndex == IFSTACKINITINDEX )
            {
                return ID_FALSE;
            }
            break;
        default:
            break;
    }

    return ID_TRUE;
}


// #endif �� ���������, #if, #ifdef Ȥ�� #ifndef ���� pop �Ѵ�.
idBool ulpPPifstackMgr::ulpIfpop4endif()
{
    idBool sBreak;
    idBool sSescDEC;

    sBreak   = ID_FALSE;
    sSescDEC = ID_FALSE;

    while ( mIndex > IFSTACKINITINDEX )
    {
        switch( mIfstack[ mIndex ].mType )
        {
            case PP_IF:
                //idlOS::printf("#pop n:%d, t:%d, s:%d\n", 
                //              mIndex, mIfstack[ mIndex ].mType, mIfstack[ mIndex ].mSkip);
                mIndex--;
                sBreak = ID_TRUE;
                break;
            case PP_IFDEF:
                //idlOS::printf("#pop n:%d, t:%d, s:%d\n", 
                //              mIndex, mIfstack[ mIndex ].mType, mIfstack[ mIndex ].mSkip);
                /* BUG-28162 : SESC_DECLARE ��Ȱ  */
                sSescDEC = mIfstack[ mIndex ].mSescDEC;
                mIndex--;
                sBreak = ID_TRUE;
                break;
            case PP_IFNDEF:
                //idlOS::printf("#pop n:%d, t:%d, s:%d\n", 
                //              mIndex, mIfstack[ mIndex ].mType, mIfstack[ mIndex ].mSkip);
                mIndex--;
                sBreak = ID_TRUE;
                break;
            default:
                //idlOS::printf("#pop n:%d, t:%d, s:%d\n", 
                //              mIndex, mIfstack[ mIndex ].mType, mIfstack[ mIndex ].mSkip);
                mIndex--;
                break;
        }
        if ( sBreak == ID_TRUE )
        {
            break;
        }
    }

    return sSescDEC;
}


IDE_RC ulpPPifstackMgr::ulpIfpush( ulpPPiftype aIftype, ulpPPifresult aVal )
{
    if( mIndex >= MAX_MACRO_IF_STACK_SIZE )
    {
        // error report
        return IDE_FAILURE;
    }
    mIndex++;
    mIfstack[ mIndex ].mType = aIftype;
    /* BUG-28162 : SESC_DECLARE ��Ȱ  */
    if( aVal != PP_IF_SESC_DEC )
    {
        mIfstack[ mIndex ].mVal     = aVal;
        mIfstack[ mIndex ].mSescDEC = ID_FALSE;
    }
    else
    {
        // #ifdef SESC_DECLARE ������ ������ ���̴�.
        mIfstack[ mIndex ].mVal     = PP_IF_TRUE;
        mIfstack[ mIndex ].mSescDEC = ID_TRUE;
    }

    return IDE_SUCCESS;
}


// �ش� macro ���ǹ��� �׳� skip�ص� �Ǵ��� �˻��ϱ����� �������°��� �������ش�.
ulpPPifresult ulpPPifstackMgr::ulpPrevIfStatus()
{
    if( mIndex > IFSTACKINITINDEX )
    {
        return mIfstack[ mIndex ].mVal;
    }
    else
    {
        return PP_IF_TRUE;
    }
}

/* BUG-27961 : preprocessor�� ��ø #ifó���� #endif �����ҽ� ������ ����ϴ� ����  */
// preprocessor�� #endif �� ��ģ�� ���������� �ڵ����
// ���Ϸ� ����� �ؾ��ϴ��� �׳� skip�ؾ��ϴ����� ��������.
idBool ulpPPifstackMgr::ulpIfneedSkip4Endif()
{
    idBool sRes;

    if( mIndex < 0 )
    {
        // �Ʒ��� ��쿡 �ش��.
        // #if ...
        // ...
        // #endif
        // ... ���� ���ʹ� ���Ϸ� ��� �ؾ���. <<< �� ������.
        sRes = ID_FALSE;
    }
    else
    {
        if( (mIndex == 0) && (mIfstack[ mIndex ].mVal == PP_IF_TRUE) )
        {
            // �Ʒ��� ��쿡 �ش��.
            // #if 1
            // ...
            //   #if ...
            //   ...
            //   #endif
            // ... ���� ���ʹ� ���Ϸ� ��� �ؾ���. <<< �� ������.
            // #endif
            sRes = ID_FALSE;
        }
        else
        {
            if( (mIndex > 0) &&
                (mIfstack[ mIndex ].mVal     == PP_IF_TRUE) &&
                (mIfstack[ mIndex - 1 ].mVal == PP_IF_FALSE) )
            {
                // �Ʒ��� ��쿡 �ش��.
                // #if 0
                // ...
                // #else
                // ...
                //   #if ...
                //   ...
                //   #endif
                // ... ���� ���ʹ� ���Ϸ� ��� �ؾ���. <<< �� ������.
                // #endif
                sRes = ID_FALSE;
            }
            else
            {
                if( (mIndex > 0) &&
                    (mIfstack[ mIndex ].mVal     == PP_IF_TRUE) &&
                    (mIfstack[ mIndex - 1 ].mVal == PP_IF_TRUE) )
                {
                    // �Ʒ��� ��쿡 �ش��.
                    //#if 1
                    // #if 1
                    // ...
                    //   #if ...
                    //   ...
                    //   #endif
                    // ... ���� ���ʹ� ���Ϸ� ��� �ؾ���. <<< �� ������.
                    // #endif
                    //#endif
                    sRes = ID_FALSE;
                }
                else
                {
                    // ������ ��쿡�� ���õ�.
                    sRes = ID_TRUE;
                }
            }
        }
    }

    return sRes;
}

