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
 * $Id: iSQLExecuteCommand.h 86502 2020-01-07 05:24:08Z bethy $
 **********************************************************************/

#ifndef _O_ISQLEXECUTECOMMAND_H_
#define _O_ISQLEXECUTECOMMAND_H_ 1

#include <uttTime.h>
#include <utISPApi.h>
#include <iSQL.h>
#include <iSQLSpool.h>

class iSQLExecuteCommand
{
public:
    iSQLExecuteCommand( SInt a_bufSize, iSQLSpool * aSpool, utISPApi * aISPApi );
    ~iSQLExecuteCommand();

    IDE_RC ConnectDB();
    void   DisconnectDB(IDE_RC aISQLRC = IDE_SUCCESS);

    void   EndTran(SInt aAutocommit);

    /* [ select * from tab ������ ����]
     * ���̺� ����Ʈ�� �����ְų� TAB���̺��� ROW�� �����ش�. */
    IDE_RC DisplayTableListOrSelect(SChar *aQueryStr);
    IDE_RC DisplayTableListOrPrepare(SChar *aQueryStr,
                                     SInt aQueryBufLen);

    IDE_RC DisplayFixedTableList(const SChar * a_PrefixName,
                                 const SChar * a_TableType);
    IDE_RC DisplaySequenceList();
    IDE_RC DisplayAttributeList( SChar * a_UserName,
                                 SChar * a_TableName );
    IDE_RC DisplayAttributeList4FTnPV( SChar * a_UserName,
                                       SChar * a_TableName );

    IDE_RC BindParam();
    /* PROJ-1584 DML Return Clause */
    void   returnBindParam();

    IDE_RC ExecuteDDLStmt( SChar           * a_DDLStmt,
                           iSQLCommandKind   a_CommandKind );

    /* BUG-37002 isql cannot parse package as a assigned variable */
    IDE_RC ExecutePSMStmt( SChar * a_PSMStmt,
                           SChar * a_UserName,
                           SChar * a_PkgName,
                           SChar * a_ProcName,
                           idBool  a_IsFunc );
   
    IDE_RC ExecuteOtherCommandStmt( SChar * a_OtherCommandStmt );
    IDE_RC ExecuteSysdbaCommandStmt( SChar * a_CommandStr,
                                     SChar * a_SysdbaCommandStmt );

    IDE_RC ExecuteConnectStmt(SChar *aQueryStr,
                              SChar *aCmdUser, SChar *aCmdPasswd,
                              SChar *aCmdNlsUse, idBool aCmdIsSysDBA);
    IDE_RC ExecuteDisconnectStmt(SChar *aQueryStr,
                                 idBool aDisplayMode);
    IDE_RC ExecuteAutoCommitStmt( SChar * a_CommandStr,
                                  idBool  a_IsAutoCommitOn );
    IDE_RC ExecuteEndTranStmt( SChar * a_CommandStr,
                               idBool  a_IsCommit );

    void   ExecuteSpoolStmt( SChar        * a_FileName,
                             iSQLPathType   a_PathType );
    void   ExecuteSpoolOffStmt();

    void   ExecuteShellStmt( SChar * a_ShellStmt );
    void   ExecuteEditStmt( SChar        * a_InFileName,
                            iSQLPathType   a_PathType,
                            SChar        * a_OutFileName );
    IDE_RC PrintHelpString( iSQLCommandKind   eHelpArguKind );

    void   ShowElapsedTime();

    void   ShowHostVar();
    void   ShowHostVar( SChar * a_HostVarName );

    void   PrintHeader( SInt * ColSize,
                        SInt * pg,
                        SInt * space );
    IDE_RC ExecuteSelectOrDMLStmt(SChar *aQueryStr,
                                  iSQLCommandKind aCmdKind);
    IDE_RC PrepareSelectOrDMLStmt(SChar *aQueryStr,
                                  iSQLCommandKind aCmdKind);
    IDE_RC FetchSelectStmt(idBool aPrepare, SInt *aRowCnt);
    IDE_RC PrintFoot(SInt aRowCnt, iSQLCommandKind aCmdKind, idBool aPrepare);
    void   PrintCount(SInt aRowCnt, iSQLCommandKind aCmdKind);
    IDE_RC PrintPlan(idBool aPrepare);
    void   PrintTime();

    IDE_RC ShowColumns( SChar * a_UserName,
                        SChar * a_TableName );
    IDE_RC ShowColumns4FTnPV( SChar * a_UserName,
                              SChar * a_TableName );
    IDE_RC ShowIndexInfo( SChar * a_UserName,
                          SChar * a_TableName,
                          SInt  * aIndexCount);
    IDE_RC ShowPrimaryKeys( SChar * a_UserName,
                            SChar * a_TableName );
    IDE_RC ShowForeignKeys( SChar * a_UserName,
                            SChar * a_TableName );

    /* PROJ-1107 Check Constraint ���� */
    IDE_RC ShowCheckConstraints( SChar * aUserName,
                                 SChar * aTableName );

    /* BUG-43516 DESC with Partition-information */
    IDE_RC ShowPartitions( SChar * aUserName,
                           SChar * aTableName );
    IDE_RC ShowPartitionKeyColumns( SInt   aUserId,
                                    SInt   aTableId );
    IDE_RC ShowPartitionValues( SInt   aUserId,
                                SInt   aTableId,
                                SInt   aPartitionMethod );
    IDE_RC ShowPartitionTbs( SInt   aUserId,
                             SInt   aTableId );

    // for admin
    IDE_RC Startup(SInt aMode, iSQLForkRunType aRunWServer);
    IDE_RC Shutdown(SInt aMode);

    /* BUG-43529 Need to reconnect to a normal service session */
    IDE_RC Reconnect();

#if 0
    IDE_RC Status(SChar * a_CommandStr, SInt aStatID, SChar *aArg);
    IDE_RC Terminate(SChar * a_CommandStr,  SChar *aNumber);
#endif

    /* BUG-32568 Support Cancel */
    void   ResetFetchCancel() { m_IsFetchCanceled = ID_FALSE; };
    void   FetchCancel()      {
        m_IsFetchCanceled = ID_TRUE;
    };
    idBool IsFetchCanceled()  { return m_IsFetchCanceled;     };

    /* BUG-39620 DESC, select * from tab, select * from seq�� ����ϴ�
     * ��ü�� DISPLAY SIZE ����, SET FULLNAME ON/OFF �� ȣ��ȴ�. */
    void   SetObjectDispLen( UInt aObjectDispLen )
                              { mObjectDispLen = aObjectDispLen; };

    /* BUG-41163 SET SQLP[ROMPT] */
    IDE_RC GetCurrentDate( SChar *aCurrentDate );

    /* BUG-34447 SET NUMF[ORMAT] */
    IDE_RC SetNlsCurrency();

    /* BUG-44613 Set PrefetchRows */
    IDE_RC SetPrefetchRows(SInt aPrefetchRows);

    /* BUG-44613 Set AsyncPrefetch On|Auto|Off */
    IDE_RC SetAsyncPrefetch(AsyncPrefetchType aType);

    /* TASK-7218 Handling Multi-Error */
    IDE_RC PrintMultiError();

    utISPApi   * m_ISPApi;

private:
    uttTime      m_uttTime;
    iSQLSpool  * m_Spool;
    SDouble      m_ElapsedTime;
    idBool       m_IsFetchCanceled;
    UInt         mObjectDispLen;

    /* ���� ����ڰ� ������ ������ ���� ������������ �˾Ƴ���. */
    idBool IsSysUser();

    /* ���̺� ����Ʈ�� �����ش�. */
    IDE_RC DisplayTableList();

    /* BUG-39620 �Լ� ������ cpp ���� */
    void   printObjectForDesc( const SChar  * aName,
                               const idBool   aIsFixedLen,
                               const SChar  * aWhiteSpace );
    void   printSchemaObject( const idBool   aIsSysUser,
                              const SChar  * aSchemaName,
                              const SChar  * aObjectName,
                              const SChar  * aTableType );
    void   getObjectNameForDesc( const SChar  * aName,
                                 SChar        * aDispName,
                                 const UInt     aDispLen );

    IDE_RC printRow(idBool    aPrepare,
                    SInt      aColCnt,
                    SInt     *aColCntForEachLine);
};

#endif // _O_ISQLEXECUTECOMMAND_H_

