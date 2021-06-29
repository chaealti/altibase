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

#include <vcl.h>
#pragma hdrstop

#include "dsnManager.h"
#include "mainForm1.h"
//---------------------------------------------------------------------------
#pragma package(smart_init)
#pragma resource "*.dfm"
TForm1 *Form1;
//---------------------------------------------------------------------------
__fastcall TForm1::TForm1(TComponent* Owner)
	: TForm(Owner)
{
}
//---------------------------------------------------------------------------
// DSN LIST�� �������� �Լ�
void __fastcall TForm1::GetDsnList()
{
    HKEY hKey, hKey2;
	DWORD value_type, length;
	DWORD key_num;
	DWORD subkey_length;
	TCHAR  ByVal1[1024], sBuf[1024], subkey_name[1024], sBuf2[1024];
	FILETIME file_time;
	AnsiString x;

	// MainRootKey�� ����.
	wsprintf(sBuf , "Software\\ODBC\\ODBC.INI");
	if (RegOpenKeyEx(HKEY_LOCAL_MACHINE,
				  sBuf,
				  0,
				  KEY_ALL_ACCESS,
				  &hKey) != 0)
	{
        ShowMessage("RegOpenKeyEx-1 Fail!!");
		return;
	}

	key_num = 0;
    DSNLIST->Clear();
	
	// Enum�� ������ �������� ODBC.INI�� ������.
	while (1)
	{
		subkey_length = 1024;
		memset(subkey_name , 0x00, sizeof(subkey_name));
		// �� �Լ��� ���ϸ� DSNLIST�� ���´�.
		if (RegEnumKeyEx( hKey,
						  key_num,
						  subkey_name,
						  &subkey_length,
						  0,
						  0 ,
						  0 ,
						  &file_time) != 0)
		{
			//ShowMessage("RegEnumKeyEx-1 Fail!!");
			break;
		}

		// DSN���� ������ �ٽ� Key�� ����.
		wsprintf(sBuf, "Software\\ODBC\\ODBC.INI\\%s", subkey_name);
		if (RegOpenKeyEx(HKEY_LOCAL_MACHINE,
						 sBuf,
						 0,
						 KEY_ALL_ACCESS,
						 &hKey2) != 0)
		{
			//ShowMessage("RegOpenKeyEx-2 Fail");
			break;
		}

		// ���� Key���� Dirver�� Altibase������ Ȯ���Ѵ�.
		length = 1024;
		value_type = NULL;
		memset(ByVal1 , 0x00, sizeof(ByVal1));
		wsprintf(sBuf2, "Driver");
		if (RegQueryValueEx(hKey2,
							sBuf2,
							0,
							&value_type,
							ByVal1,
							&length) == 0)
		{
			// AltibaseDLL�� ���³��̳�?
		   AnsiString x = ByVal1;
		   int c;

		   // a4_CM451.dll �̴�.
		   c = x.Pos("a4_");
		   if (c != 0)
		   {
              // ListBox�� ����Ѵ�.
			  DSNLIST->Items->Add(subkey_name) ;
		   }
		}

		// ���ʿ� �����͸� �ݴ´�.
		RegCloseKey(hKey2);
		key_num++;

	}

	// ���� Key�ݴ´�.
	RegCloseKey(hKey);
    
}
//---------------------------------------------------------------------------
// Form�� ������ List�� �����´�.
void __fastcall TForm1::FormShow(TObject *Sender)
{
	GetDsnList();
}
//---------------------------------------------------------------------------
// DSNLIST ���� DSN �� Ŭ���Ҷ� ó���Ѵ�. ������ ������� �ϴϱ�!!
void __fastcall TForm1::DSNLISTClick(TObject *Sender)
{
	HKEY hKey2;
	DWORD value_type, length;
	TCHAR  ByVal1[1024], sBuf[1024], sBuf2[1024];
	AnsiString x;


	// DSN alias name ����
	DSN->Text = DSNLIST->Items->Strings[DSNLIST->ItemIndex] ;

	// DSN Name�� Ű�� �Ͽ� ��ü �ɼǰ��� ������ ã�´�.
	wsprintf(sBuf, "Software\\ODBC\\ODBC.INI\\%s", DSN->Text.c_str() );
	if (RegOpenKeyEx(HKEY_LOCAL_MACHINE,
					 sBuf,
					 0,
					 KEY_ALL_ACCESS,
					 &hKey2) != 0)
	{
		ShowMessage("RegOpenKeyEx-3 Fail");
		return;
	}

	// Database
	length = 1024;
	value_type = NULL;
	memset(ByVal1 , 0x00, sizeof(ByVal1));
	wsprintf(sBuf2, "DataBase");
	if (RegQueryValueEx(hKey2,
						sBuf2,
						0,
						&value_type,
						ByVal1,
						&length) == 0)
	{
        DataBase->Text = ByVal1;
	}

	// Driver
	length = 1024;
	value_type = NULL;
	memset(ByVal1 , 0x00, sizeof(ByVal1));
	wsprintf(sBuf2, "Driver");
	if (RegQueryValueEx(hKey2,
						sBuf2,
						0,
						&value_type,
						ByVal1,
						&length) == 0)
	{
        DLL->Text = ByVal1;
	}

	// NLS_USE
	length = 1024;
	value_type = NULL;
	memset(ByVal1 , 0x00, sizeof(ByVal1));
	wsprintf(sBuf2, "NLS_USE");
	if (RegQueryValueEx(hKey2,
						sBuf2,
						0,
						&value_type,
						ByVal1,
						&length) == 0)
	{
		NLS->Text = ByVal1;
	}

	// PASSWORD
	length = 1024;
	value_type = NULL;
	memset(ByVal1 , 0x00, sizeof(ByVal1));
	wsprintf(sBuf2, "Password");
	if (RegQueryValueEx(hKey2,
						sBuf2,
						0,
						&value_type,
						ByVal1,
						&length) == 0)
	{
        PASSWD->Text = ByVal1;
	}

	// PORT
	length = 1024;
	value_type = NULL;
	memset(ByVal1 , 0x00, sizeof(ByVal1));
	wsprintf(sBuf2, "Port");
	if (RegQueryValueEx(hKey2,
						sBuf2,
						0,
						&value_type,
						ByVal1,
						&length) == 0)
	{
		PORT->Text = ByVal1;
	}

	// Server
	length = 1024;
	value_type = NULL;
	memset(ByVal1 , 0x00, sizeof(ByVal1));
	wsprintf(sBuf2, "Server");
	if (RegQueryValueEx(hKey2,
						sBuf2,
						0,
						&value_type,
						ByVal1,
						&length) == 0)
	{
		SERVER->Text = ByVal1;
	}

	// User
	length = 1024;
	value_type = NULL;
	memset(ByVal1 , 0x00, sizeof(ByVal1));
	wsprintf(sBuf2, "User");
	if (RegQueryValueEx(hKey2,
						sBuf2,
						0,
						&value_type,
						ByVal1,
						&length) == 0)
	{
		USER->Text = ByVal1;
	}

	RegCloseKey(hKey2);	
}
//---------------------------------------------------------------------------
// DSN ������ �߰� / ���� �Ҷ� ó�� �Ѵ�.
void __fastcall TForm1::Button1Click(TObject *Sender)
{
   HKEY sRootKey;
   HKEY sKey;
   DWORD sDisp;
   TCHAR sBuf[MAX_PATH];
   bool already_exist = false;

   if (DSN->Text.Length() == 0)
   {
	   ShowMessage("First, Input DSN NAME");
	   return;
   }
   if (SERVER->Text.Length() == 0)
   {
	   ShowMessage("First, Input Server IP");
	   return;
   }
   if (PORT->Text.Length() == 0)
   {
	   ShowMessage("First, Input Port Number");
	   return;
   }
   if (USER->Text.Length() == 0)
   {
	   ShowMessage("First, Input User Name");
	   return;
   }
   if (PASSWD->Text.Length() == 0)
   {
	   ShowMessage("First, Input Password");
	   return;
   }
   if (DataBase->Text.Length() == 0)
   {
	   ShowMessage("First, Input Database Name");
	   return;
   }
   if (DLL->Text.Length() == 0)
   {
	   ShowMessage("First, Input Dll location (FullPath)");
	   return;
   }
   sRootKey = HKEY_LOCAL_MACHINE;

   // ���õ� DSN���� KEY�� ����.
   wsprintf( sBuf, "Software\\ODBC\\ODBC.INI\\%s", DSN->Text.c_str());
   if( 0 != RegCreateKeyEx(sRootKey,
                           sBuf,
                           0,
                           NULL,
                           REG_OPTION_NON_VOLATILE,
                           KEY_WRITE,
                           NULL,
                           &sKey,
                           &sDisp))
   {
	  ShowMessage("RegCreateKeyEx Fail!!");
	  return;
   }

   // Server
   if( 0 != RegSetValueEx(sKey, "Server", 0, REG_SZ, SERVER->Text.c_str(),
                          (DWORD) SERVER->Text.Length()*sizeof(TCHAR)))
   {
	  RegCloseKey(sKey);
	  ShowMessage("Save Fail: RegOpenKeyEx:Server");
	  return;
   }

   // Port
   if( 0 != RegSetValueEx(sKey, "Port", 0, REG_SZ, PORT->Text.c_str(),
                          (DWORD) PORT->Text.Length()*sizeof(TCHAR)))
   {
	  RegCloseKey(sKey);
	  ShowMessage("Save Fail: RegOpenKeyEx:Port");
	  return;
   }

   // User
   if( 0 != RegSetValueEx(sKey, "User", 0, REG_SZ, USER->Text.c_str(),
						  (DWORD) USER->Text.Length()*sizeof(TCHAR)))
   {
	  RegCloseKey(sKey);
	  ShowMessage("Save Fail: RegOpenKeyEx:User");
	  return;
   }

   // Password
   if( 0 != RegSetValueEx(sKey, "Password", 0, REG_SZ, PASSWD->Text.c_str(),
                          (DWORD) PASSWD->Text.Length()*sizeof(TCHAR)))
   {
	  RegCloseKey(sKey);
	  ShowMessage("Save Fail: RegOpenKeyEx:Password");
	  return;
   }

   // DataBase
   if( 0 != RegSetValueEx(sKey, "DataBase", 0, REG_SZ, DataBase->Text.c_str(),
                          (DWORD) DataBase->Text.Length()*sizeof(TCHAR)))
   {
	  RegCloseKey(sKey);
	  ShowMessage("Save Fail: RegOpenKeyEx:Database");
	  return;
   }

   // NLS_USE
   if( 0 != RegSetValueEx(sKey, "NLS_USE", 0, REG_SZ, NLS->Text.c_str(),
						  (DWORD) NLS->Text.Length()*sizeof(TCHAR)))
   {
	  RegCloseKey(sKey);
	  ShowMessage("Save Fail: RegOpenKeyEx:NLS_USE");
	  return;
   }

   // Driver
   memset( sBuf, 0x00, MAX_PATH);
   wsprintf( sBuf, "%s", DLL->Text.c_str());
   if( 0 != RegSetValueEx(sKey, "Driver", 0, REG_SZ, (LPBYTE) sBuf,
                          (DWORD) (lstrlen(sBuf)+1)*sizeof(TCHAR)))
   {
	  RegCloseKey(sKey);
	  ShowMessage("Save Fail: RegOpenKeyEx:DLL");
	  return;
   }

   RegCloseKey(sKey);

   // DATASOURCE���� ����ؾ� �Ѵ�.
   memset( sBuf, 0x00, MAX_PATH);
   wsprintf( sBuf, "Software\\ODBC\\ODBC.INI\\ODBC Data Sources");
   if( 0 != RegOpenKeyEx( sRootKey,
                          sBuf,
                          0,
                          KEY_WRITE,
                          &sKey ))
   {
	  ShowMessage("Save Fail: RegOpenKeyEx:ODBC LAST");
	  return;
   }

   // DLL�� ����Ѵ�.
   memset( sBuf, 0x00, MAX_PATH);
   wsprintf( sBuf, "Altibase_ODBC_cm451");
   if( 0 != RegSetValueEx(sKey, DSN->Text.c_str(), 0, REG_SZ, (LPBYTE) sBuf,
                          (DWORD) (lstrlen(sBuf)+1)*sizeof(TCHAR)))
   {
	  RegCloseKey(sKey);
	  ShowMessage("Save Fail: RegSetValueEx:ALIAS");
	  return;
   }

   RegCloseKey(sKey);

   // ó�� ��� �����ֱ�
   ShowMessage("Save Success!!");

   // DSN LIST ����
   GetDsnList();

   // mainForm�� Ʈ�������� ��������� �Ѵ�.
   // �� ������..
   // �˴� ���鼭 ��ϵ� ������ üũ�Ѵ�. ���� ����� ���󼭸�..
   TTreeNode *tNode = Form5->DBNODE->Items->GetFirstNode();
   already_exist = false;
   while (tNode)
   {
       //ShowMessage(tNode->Text + " " + DSN->Text);
	   if (tNode->Text == DSN->Text)
	   {

		   already_exist = true;
		   break;
	   }
	   tNode = tNode->GetNext();
   }
   // ���³��̶�� �ǴܵǸ� root�� ����Ѵ�.
   if (!already_exist)
   {
	   tNode = Form5->DBNODE->Items->GetFirstNode();
	   Form5->DBNODE->Items->Add(tNode, DSN->Text);
   }

   return;
}
//---------------------------------------------------------------------------
// DSN�� �����Ҷ� ó���Ѵ�.
void __fastcall TForm1::Button2Click(TObject *Sender)
{
   HKEY sRootKey;
   HKEY sKey;
   TCHAR sBuf[MAX_PATH];

   if (DSN->Text.Length() == 0)
   {
	   ShowMessage("First, Select DSN to delete");
       return;
   }
   sRootKey = HKEY_LOCAL_MACHINE;

   // ODBC.INI���� ����
   wsprintf( sBuf, "Software\\ODBC\\ODBC.INI\\%s", DSN->Text.c_str());
   if( 0 != RegDeleteKey(sRootKey, sBuf))
   {
	  ShowMessage("RegDeleteKey Fail");
	  return;
   }


   // DATASOURCE ���� ����
   memset( sBuf, 0x00, MAX_PATH);
   wsprintf( sBuf, "Software\\ODBC\\ODBC.INI\\ODBC Data Sources");
   if( 0 != RegOpenKeyEx( sRootKey,
                          sBuf,
                          0,
                          KEY_WRITE,
                          &sKey ))
   {
	  ShowMessage("RegOpenKeyEx Fail");
	  return ;
   }

   if( 0 != RegDeleteValue(sKey, DSN->Text.c_str()))
   {
	  ShowMessage("RegDeleteValue");
	  return ;
   }

   RegCloseKey(sKey);
   ShowMessage("Delete Success!!");
   
   // Form5�� ���� DBNODE treeNode���� ã�Ƽ� �����Ѵ�.
   TTreeNode *tNode = Form5->DBNODE->Items->GetFirstNode();
   while (tNode != NULL)
   {

	   if (tNode->Text == DSN->Text)
	   {
           //ShowMessage("delete");
		   tNode->Delete();
		   break;
	   }
	   tNode = tNode->GetNext();
   }

   // DSN LIST�� �����ϰ� ����ȭ���� Clear�Ѵ�.
   GetDsnList();
   DSN->Text      = "";
   SERVER->Text   = "";
   PORT->Text     = "";
   USER->Text     = "";
   PASSWD->Text   = "";
   DataBase->Text = "";
   DLL->Text      = "";
   NLS->Text      = "";

   return ;
}
//---------------------------------------------------------------------------

