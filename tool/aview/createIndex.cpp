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
 
//---------------------------------------------------------------------------
#include <stdio.h>
#include <vcl.h>
#pragma hdrstop
#define WIN32
#include <sql.h>
#include <sqlext.h>

#include "createIndex.h"
#include "mainForm1.h"
//---------------------------------------------------------------------------
#pragma package(smart_init)
#pragma resource "*.dfm"
TForm7 *Form7;





//---------------------------------------------------------------------------
__fastcall TForm7::TForm7(TComponent* Owner)
	: TForm(Owner)
{
}
//---------------------------------------------------------------------------
void __fastcall TForm7::Button1Click(TObject *Sender)
{
	int i;
    int row=1;

	for (i=0;i<ColGrid2->Items->Count; i++)
	{
		if ( ColGrid2->Checked[i] == true )
		{
			IndexGrid->Cells[0][row] = ColGrid2->Items->Strings[i];
			IndexGrid->Cells[1][row] = "asc";
            row++;
		}
	}
}
//---------------------------------------------------------------------------
void __fastcall TForm7::Button2Click(TObject *Sender)
{
	IndexGrid->Cells[0][IndexGrid->Row] = "";
    IndexGrid->Cells[1][IndexGrid->Row] = "";
}
//---------------------------------------------------------------------------
void dbErrMsg5(SQLHENV _env, SQLHDBC _dbc, SQLHSTMT _stmt)
{
    SQLCHAR errMsg[4096];
	SQLSMALLINT msgLen;
	SQLINTEGER errNo;
	char gMsg[8192];

	memset(errMsg, 0x00, sizeof(errMsg));
	SQLError(_env, _dbc, _stmt, NULL, &errNo, errMsg, 4095, &msgLen);
	wsprintf(gMsg, "Form7] ErrNo=[%d]:ErrMsg=%s", errNo, errMsg);
	ShowMessage(gMsg);
}
//---------------------------------------------------------------------------
void __fastcall TForm7::Button3Click(TObject *Sender)
{
	SQLHENV env;
	SQLHDBC dbc;
	SQLHSTMT stmt;
	char query[1024], unique[10];
    char USER[41], PASSWD[41];
	int i, colCount=0;

	if (CheckBox2->Checked == false && IndexName->Text.Length() == 0)
	{
		ShowMessage("Input Indexname !!");
		IndexName->SetFocus();
		return;
	}

    // ����ũindex�� ������ ����.
	if (CheckBox1->Checked == true)
	{
		sprintf(unique, "unique");
	}else {
        memset(unique, 0x00, sizeof(unique));
    }

	// SQL�� ����
	sprintf(query, "create %s index %s.%s on %s (",
						unique,
						USERS->Text.c_str(),
						IndexName->Text.c_str(),
						TARGET->Text.c_str());
	colCount = 0;
	for (i = 1; i < IndexGrid->RowCount; i++)
	{
		if (IndexGrid->Cells[0][i].Length() == 0)
		{
			continue;
		}

		strcat(query, IndexGrid->Cells[0][i].c_str());
		strcat(query, " ");
		strcat(query, IndexGrid->Cells[1][i].c_str());
		strcat(query, ",");
		colCount++;
	}
    // �÷��� ���ε��Ҳ� ������ ���� ���ش�.
	if (colCount == 0)
	{
		ShowMessage("Are you kidding me??");
		return;
	}
	
	i = strlen(query);
	query[i-1] = ')';
	query[i] = 0x00;

	// PK�� ���õ� ���¸� ������ �ٽ� �����.
	// �׸��� INDEXNAME�κ��� �ԷµǾ��Ŀ� ���� ������ �ٸ����� �°� ó���Ѵ�.
	if (CheckBox2->Checked == true)
	{
		sprintf(query, "alter table %s add ", TARGET->Text.c_str());
		if (IndexName->Text.Length() > 0)
		{
			strcat (query, "constraint ");
			strcat (query, IndexName->Text.c_str());
			strcat (query, " ");
		}
		strcat (query, "primary key ( ");
        for (i = 1; i < IndexGrid->RowCount; i++)
		{
			if (IndexGrid->Cells[0][i].Length() == 0)
			{
				continue;
			}

			strcat(query, IndexGrid->Cells[0][i].c_str());
			strcat(query, " ");
			strcat(query, IndexGrid->Cells[1][i].c_str());
			strcat(query, ",");
			colCount++;
		}
		i = strlen(query);
		query[i-1] = ')';
	    query[i] = 0x00;
	}

	// ������ ������ ������.
	if (SQLAllocEnv(&env) != SQL_SUCCESS) {
		ShowMessage("SQLAllocEnv Fail");
		return;
	}
    if (SQLAllocConnect(env, &dbc) != SQL_SUCCESS) {
		ShowMessage("SQLAllocConnect Fail");
        SQLFreeEnv(env);
		return;
	}
    
    // ���������� �����ɴϴ�..
	if (!Form5->getDsnInfo(SERVERNAME->Text, "User",     USER))
	{
		ShowMessage("Can't Get User from DSN");
		return;
	}
	if (!Form5->getDsnInfo(SERVERNAME->Text, "Password", PASSWD))
	{
		ShowMessage("Can't Get Password from DSN");
		return;
	}

	// ��¥ �����غ��ϴ�.
	if (SQLConnect(dbc, SERVERNAME->Text.c_str(), SQL_NTS, USER, SQL_NTS, PASSWD, SQL_NTS) != SQL_SUCCESS)
	{
		dbErrMsg5(env, dbc, SQL_NULL_HSTMT);
		SQLFreeConnect(dbc);
		SQLFreeEnv(env);
		return;
	}

	if (SQLAllocStmt(dbc, &stmt) != SQL_SUCCESS) {
		ShowMessage("SQLAllocStmt Fail");
		SQLFreeConnect(dbc);
		SQLFreeEnv(env);
		return;
	}

	Button3->Enabled = false;
	if (SQLExecDirect(stmt, query, SQL_NTS) != SQL_SUCCESS)
	{
		dbErrMsg5(env, dbc, stmt);
		SQLFreeStmt(stmt, SQL_DROP);
        SQLFreeConnect(dbc);
		SQLFreeEnv(env);
        Button3->Enabled = true;
		return;
	}

	SQLFreeStmt(stmt, SQL_DROP);
	SQLFreeConnect(dbc);
	SQLFreeEnv(env);
    Button3->Enabled = true;
	ShowMessage("Create index Success!!");
}
//---------------------------------------------------------------------------
