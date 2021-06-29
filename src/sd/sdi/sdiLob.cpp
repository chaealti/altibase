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
 * $Id: sdiLob.cpp 88285 2020-08-05 05:49:37Z bethy $
 **********************************************************************/

#include <sdDef.h>
#include <sdi.h>
#include <sdiLob.h>
#include <sdl.h>

IDE_RC sdiLob::open()
{
    return IDE_SUCCESS;
}

IDE_RC sdiLob::close( idvSQL        * aStatistics,
                      void          * aTrans,
                      smLobViewEnv  * aLobViewEnv )
{
    sdiConnectInfo   *sConnectInfo = NULL;
    sdlRemoteStmt    *sRemoteStmt  = NULL;
    smShardLobCursor *sShardLobCursor = aLobViewEnv->mShardLobCursor;

    (void) enter( aStatistics,
                  aTrans,
                  sShardLobCursor,
                  &sConnectInfo,
                  &sRemoteStmt );

    IDE_TEST( sdl::freeLob(
                  sConnectInfo,
                  sRemoteStmt,
                  sShardLobCursor->mRemoteLobLocator,
                  &(sConnectInfo->mLinkFailure) )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**********************************************************************
 * lobCursor�� ����Ű�� �ִ� �޸� LOB ����Ÿ�� �о�´�.
 *
 * aTrans      [IN]  �۾��ϴ� Ʈ����� ��ü
 * aLobViewEnv [IN]  �۾��Ϸ��� LobViewEnv ��ü
 * aOffset     [IN]  �о������ Lob ����Ÿ�� ��ġ
 * aMount      [IN]  �о������ piece�� ũ��
 * aPiece      [OUT] ��ȯ�Ϸ��� Lob ����Ÿ piece ������
 **********************************************************************/
IDE_RC sdiLob::read( idvSQL        * aStatistics,
                     void          * aTrans,
                     smLobViewEnv  * aLobViewEnv,
                     UInt            aOffset,
                     UInt            aMount,
                     UChar         * aPiece,
                     UInt          * aReadLength )
{
    sdiConnectInfo   *sConnectInfo = NULL;
    sdlRemoteStmt    *sRemoteStmt  = NULL;
    smShardLobCursor *sShardLobCursor;
    SShort            sTargetCType;

    sShardLobCursor = aLobViewEnv->mShardLobCursor;

    (void) enter( aStatistics,
                  aTrans,
                  sShardLobCursor,
                  &sConnectInfo,
                  &sRemoteStmt );

    if ( sShardLobCursor->mLobLocatorType == MTD_CLOB_LOCATOR_ID )
    {
        sTargetCType = MTD_CHAR_ID;
    }
    else
    {
        sTargetCType = (SShort) MTD_BINARY_ID;
    }
    IDE_TEST( sdl::getLob(
                  sConnectInfo,
                  sRemoteStmt,
                  sShardLobCursor->mLobLocatorType,
                  sShardLobCursor->mRemoteLobLocator,
                  aOffset,
                  aMount,
                  sTargetCType,
                  (void *) aPiece,
                  aMount,
                  aReadLength,
                  &(sConnectInfo->mLinkFailure) )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**********************************************************************
 * ���� �Ҵ�� ������ lob ����Ÿ�� ����Ѵ�.
 *
 * aTrans      [IN] �۾��ϴ� Ʈ����� ��ü
 * aLobViewEnv [IN] �۾��Ϸ��� LobViewEnv ��ü
 * aLobLocator [IN] �۾��Ϸ��� Lob Locator
 * aOffset     [IN] �۾��Ϸ��� Lob ����Ÿ�� ��ġ
 * aPieceLen   [IN] ���� �ԷµǴ� piece�� ũ��
 * aPiece      [IN] ���� �ԷµǴ� lob ����Ÿ piece
 **********************************************************************/
IDE_RC sdiLob::write( idvSQL       * aStatistics,
                      void         * aTrans,
                      smLobViewEnv * aLobViewEnv,
                      smLobLocator   aLobLocator,
                      UInt           aOffset,
                      UInt           aPieceLen,
                      UChar        * aPiece,
                      idBool        /* aIsFromAPI */,
                      UInt          /* aContType */ )
{
    SD_UNUSED( aLobLocator );
    SD_UNUSED( aOffset );

    sdiConnectInfo   *sConnectInfo = NULL;
    sdlRemoteStmt    *sRemoteStmt  = NULL;
    smShardLobCursor *sShardLobCursor;
    SShort            sTargetCType;

    sShardLobCursor = aLobViewEnv->mShardLobCursor;

    (void) enter( aStatistics,
                  aTrans,
                  sShardLobCursor,
                  &sConnectInfo,
                  &sRemoteStmt );

    if ( sShardLobCursor->mLobLocatorType == MTD_CLOB_LOCATOR_ID )
    {
        sTargetCType = MTD_CHAR_ID;
    }
    else
    {
        sTargetCType = (SShort) MTD_BINARY_ID;
    }
    IDE_TEST( sdl::lobWrite(
                  sConnectInfo,
                  sRemoteStmt,
                  sShardLobCursor->mLobLocatorType,
                  sShardLobCursor->mRemoteLobLocator,
                  sTargetCType,
                  (void *) aPiece,
                  aPieceLen,
                  &(sConnectInfo->mLinkFailure) )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*
 * �� �ڵ�� �� �� �ʿ��� ������� �ǴܵǾ� �����Ǿ����ϴ�.
 * ������� �׽�Ʈ�� �ʿ��մϴ�.
 */
IDE_RC sdiLob::erase( idvSQL       * aStatistics,
                      void         * aTrans,
                      smLobViewEnv * aLobViewEnv,
                      smLobLocator   aLobLocator,
                      UInt           aOffset,
                      UInt           aPieceLen )
{
    SD_UNUSED( aStatistics );
    SD_UNUSED( aTrans );
    SD_UNUSED( aLobViewEnv );
    SD_UNUSED( aLobLocator );
    SD_UNUSED( aOffset );
    SD_UNUSED( aPieceLen );

    return IDE_SUCCESS;
}

/**********************************************************************
 * lob write�ϱ� ���� new version�� ���� ���� Offset�� �����Ѵ�.
 *
 * aTrans      [IN] �۾��ϴ� Ʈ����� ��ü
 * aLobViewEnv [IN] �۾��Ϸ��� LobViewEnv ��ü
 * aLobLocator [IN] �۾��Ϸ��� Lob Locator
 * aOffset     [IN] �۾��Ϸ��� Lob ����Ÿ�� ��ġ
 * aNewSize    [IN] ���� �ԷµǴ� ����Ÿ�� ũ��
 **********************************************************************/
IDE_RC sdiLob::prepare4Write( idvSQL*       aStatistics,
                              void*         aTrans,
                              smLobViewEnv* aLobViewEnv,
                              smLobLocator  aLobLocator,
                              UInt          aOffset,
                              UInt          aNewSize )
{
    SD_UNUSED( aLobLocator );

    sdiConnectInfo   *sConnectInfo = NULL;
    sdlRemoteStmt    *sRemoteStmt  = NULL;
    smShardLobCursor *sShardLobCursor;

    sShardLobCursor = aLobViewEnv->mShardLobCursor;

    (void) enter( aStatistics,
                  aTrans,
                  sShardLobCursor,
                  &sConnectInfo,
                  &sRemoteStmt );

    IDE_TEST( sdl::lobPrepare4Write(
                  sConnectInfo,
                  sRemoteStmt,
                  sShardLobCursor->mLobLocatorType,
                  sShardLobCursor->mRemoteLobLocator,
                  aOffset,
                  aNewSize,
                  &(sConnectInfo->mLinkFailure) )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : Write�� ����Ǿ���. Replication Log�� �����.
 *
 *    aStatistics - [IN] ��� ����
 *    aTrans      - [IN] Transaction
 *    aLobViewEnv - [IN] �ڽ��� ���� �� LOB������ ����
 *    aLobLocator - [IN] Lob Locator
 **********************************************************************/
IDE_RC sdiLob::finishWrite( idvSQL       * aStatistics,
                            void         * aTrans,
                            smLobViewEnv * aLobViewEnv,
                            smLobLocator   aLobLocator )
{
    SD_UNUSED( aLobLocator );

    sdiConnectInfo   *sConnectInfo = NULL;
    sdlRemoteStmt    *sRemoteStmt  = NULL;
    smShardLobCursor *sShardLobCursor;

    sShardLobCursor = aLobViewEnv->mShardLobCursor;

    (void) enter( aStatistics,
                  aTrans,
                  sShardLobCursor,
                  &sConnectInfo,
                  &sRemoteStmt );

    IDE_TEST( sdl::lobFinishWrite(
                  sConnectInfo,
                  sRemoteStmt,
                  sShardLobCursor->mLobLocatorType,
                  sShardLobCursor->mRemoteLobLocator,
                  &(sConnectInfo->mLinkFailure) )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**********************************************************************
 * lobCursor�� ����Ű�� �ִ� �޸� LOB�� ���̸� return�Ѵ�.
 *
 * aTrans      [IN] �۾��ϴ� Ʈ����� ��ü
 * aLobViewEnv [IN] �۾��Ϸ��� LobViewEnv ��ü
 * aLobLen     [OUT] LOB ����Ÿ ����
 * aLobMode    [OUT] LOB ���� ��� ( In/Out )
 **********************************************************************/
IDE_RC sdiLob::getLobInfo( idvSQL*        aStatistics,
                           void*          aTrans,
                           smLobViewEnv*  aLobViewEnv,
                           UInt*          aLobLen,
                           UInt*          aLobMode,
                           idBool*        aIsNullLob )
{
    sdiConnectInfo   *sConnectInfo = NULL;
    sdlRemoteStmt    *sRemoteStmt  = NULL;
    smShardLobCursor *sShardLobCursor;

    sShardLobCursor = aLobViewEnv->mShardLobCursor;

    (void) enter( aStatistics,
                  aTrans,
                  sShardLobCursor,
                  &sConnectInfo,
                  &sRemoteStmt );

    if (sShardLobCursor->mRemoteLobLocator == MTD_LOCATOR_NULL )
    {
        *aLobLen = 0;
        *aIsNullLob = ID_TRUE;
    }
    else
    {
        *aIsNullLob = ID_FALSE;

        IDE_TEST( sdl::getLobLength(
                      sConnectInfo,
                      sRemoteStmt,
                      sShardLobCursor->mLobLocatorType,
                      sShardLobCursor->mRemoteLobLocator,
                      aLobLen,
                      aIsNullLob,
                      &(sConnectInfo->mLinkFailure) )
                  != IDE_SUCCESS );
    }

    if ( aLobMode != NULL )
    {
        *aLobMode = 0;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC sdiLob::trim( idvSQL       * aStatistics,
                     void         * aTrans,
                     smLobViewEnv * aLobViewEnv,
                     smLobLocator   aLobLocator,
                     UInt           aOffset )
{
    SD_UNUSED( aLobLocator );

    sdiConnectInfo   *sConnectInfo = NULL;
    sdlRemoteStmt    *sRemoteStmt  = NULL;
    smShardLobCursor *sShardLobCursor;

    sShardLobCursor = aLobViewEnv->mShardLobCursor;

    (void) enter( aStatistics,
                  aTrans,
                  sShardLobCursor,
                  &sConnectInfo,
                  &sRemoteStmt );

    IDE_TEST( sdl::trimLob(
                  sConnectInfo,
                  sRemoteStmt,
                  sShardLobCursor->mLobLocatorType,
                  sShardLobCursor->mRemoteLobLocator,
                  aOffset,
                  &(sConnectInfo->mLinkFailure) )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC sdiLob::writeLog4CursorOpen( idvSQL       * aStatistics,
                                    void         * aTrans,
                                    smLobLocator   aLobLocator,
                                    smLobViewEnv * aLobViewEnv )
{
    SD_UNUSED( aStatistics );
    SD_UNUSED( aTrans );
    SD_UNUSED( aLobLocator );
    SD_UNUSED( aLobViewEnv );

    return IDE_SUCCESS;
}

IDE_RC sdiLob::put( sdiConnectInfo   *aConnectInfo,
                    sdlRemoteStmt    *aRemoteStmt,
                    UInt              aLocatorType,
                    smLobLocator      aLobLocator,
                    void             *aValue,
                    SLong             aLength )
{
    SShort    sSourceCType;

    if ( aLocatorType == MTD_CLOB_LOCATOR_ID )
    {
        sSourceCType = MTD_CHAR_ID;
    }
    else
    {
        sSourceCType = (SShort) MTD_BINARY_ID;
    }

    if ( aLength == 0 )
    {
        IDE_TEST( sdl::putEmptyLob(
                      aConnectInfo,
                      aRemoteStmt,
                      aLocatorType,
                      aLobLocator,
                      sSourceCType,
                      aValue,
                      &(aConnectInfo->mLinkFailure) )
                  != IDE_SUCCESS );
    }
    else
    {
        IDE_TEST( sdl::putLob(
                      aConnectInfo,
                      aRemoteStmt,
                      aLocatorType,
                      aLobLocator,
                      sSourceCType,
                      aValue,
                      aLength,
                      &(aConnectInfo->mLinkFailure) )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}
