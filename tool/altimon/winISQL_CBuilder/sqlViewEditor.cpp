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

#include "sqlViewEditor.h"
#include "sqlRun.h"


// ��� �ʿ��� Ű���� �����Ѵ�.
#define SPACE_KEY  32
#define ENTER_KEY  13
#define PG_DOWN    34
#define PG_UP      33
#define LEFT_KEY   37
#define RIGHT_KEY  39
#define UP_KEY     38
#define DOWN_KEY   40
#define HOME_KEY   36
#define END_KEY    35
#define CTRL_KEY   17
#define BACKSPACE  8
#define DEL_KEY    46


// �����޽��� �ڽ��� ��Ʈ�� ũ�������� ���� Message����
#define SC_DRAG_RESIZEL  0xf001  // left resize 
#define SC_DRAG_RESIZER  0xf002  // right resize 
#define SC_DRAG_RESIZEU  0xf003  // upper resize 
#define SC_DRAG_RESIZEUL 0xf004  // upper-left resize 
#define SC_DRAG_RESIZEUR 0xf005  // upper-right resize 
#define SC_DRAG_RESIZED  0xf006  // down resize 
#define SC_DRAG_RESIZEDL 0xf007  // down-left resize 
#define SC_DRAG_RESIZEDR 0xf008  // down-right resize 
#define SC_DRAG_MOVE     0xf012  // move 

//---------------------------------------------------------------------------
#pragma package(smart_init)
#pragma resource "*.dfm"
TForm6 *Form6;

int X = 1, Y = 1;


// ����� ����� ���� �ܾ� ���. ���� ������ ����� �𸣰ڴ�.
int WORD_COUNT = 59;
char *WORD_CHK[] = {"CREATE",     "TABLE",      "CHAR",        "INTEGER",     "NUMERIC",
					"BLOB",       "CLOB",       "DOUBLE",      "VARCHAR",     "VARCHAR2",
					"DATE",       "PRIMARY",    "INDEX",       "ALTER",       "DROP",
					"AUTOEXTEND", "KEY",        ";",           "TABLESPACE",  "ADD",
					"DATAFILE",   "EXTENT",     "NEXT",        "INIT",        "MAX",
					"SIZE",       "SELECT",     "FROM",        "WHERE",       "INSERT",
					"DELETE",     "UPDATE",     "SET",         "INTO",        "VALUES",
					"SET",        "OUTER",      "JOIN",        "LEFT" ,       "LIMIT",
					"ORDER BY",   "GROUP BY",   "PROCEDURE",   "VIEW",        "DROP",
					"TRIGGER",    "REPLACE",    "RENAME",      "BEGIN",       "END",
					"CONSTRAINT", "SMALLINT",   "DEFAULT",     "ASC",         "DESC",
					"FUNCTION",   "RETURN",     "REFERENCING", "FOR",        
					""   
				   };

//---------------------------------------------------------------------------
__fastcall TForm6::TForm6(TComponent* Owner)
	: TForm(Owner)
{
}
//---------------------------------------------------------------------------
// ���ν���, Ʈ����, �Լ������� üũ�Ѵ�.
int findProcedure(AnsiString SQL)
{
	int pos1, pos2, pos3, pos4;

	// ��Ģ : create���� �����鼭 �Ʒ� 3���ܾ��߿� �ϳ��� ��������..
	pos1 = SQL.Trim().UpperCase().AnsiPos("CREATE");
	pos2 = SQL.Trim().UpperCase().AnsiPos("PROCEDURE");
    pos3 = SQL.Trim().UpperCase().AnsiPos("FUNCTION");
    pos4 = SQL.Trim().UpperCase().AnsiPos("TRIGGER");

	if (pos1 && ( pos2 || pos3 || pos4 ) )
	{
		return 1;
	}

    return 0;

}
//---------------------------------------------------------------------------
// ODBC.INI���� DSN list�� �����´�.
void __fastcall TForm6::GetDsnList()
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
// ���� ������ �ϴ�,
// ODBC������ ��������, ���ο� TabSheet �� RichEdit�� �߰��Ѵ�.
void __fastcall TForm6::FormShow(TObject *Sender)
{

	// ���۽� DSN�������� �����´�.
	GetDsnList();
	DSNLIST->ItemIndex = 0;

	// RichEdit�� ����ũ�Ⱑ �ſ� �۾� 10M�� �����Ų��.
    New1Click(this);

}
//---------------------------------------------------------------------------
// �����ư�� ������ ���� ó��
void __fastcall TForm6::Button1Click(TObject *Sender)
{
	AnsiString tmp;
	int RowCount;
	int i, j, exec_count=0, stops=0;
	int chkProc;

	// ���� ������ � richEdit������ �˾Ƴ��߸� �Ѵ�.
	// Active�� page�� �Ѽ����� �ϳ��ϱ� �װ� �������� �˾Ƴ���.
	TRichEdit *SenderControl = dynamic_cast<TRichEdit *>(PageControl1->ActivePage->FindChildControl("Rich_" + PageControl1->ActivePage->Name)) ;


	// ���õ� ���������� ������ ����ó��
	if (DSNLIST->Text.Length() == 0)
	{
		ShowMessage("Select ODBC DSN to Execute !!");
		return;    
	}

	// ��� SQL���� ������ RichEdit2���� �����ؼ� �̸� �̿��Ѵ�.
	RichEdit2->Clear();
	if (SenderControl->SelText.Length() > 0)
	{
		RichEdit2->Text = SenderControl->SelText;
	}else{
        RichEdit2->Text = SenderControl->Text;
	}

	// ���� �����ݷи��� ������ ������ �Ѵ�.
	tmp = "";
	RowCount = RichEdit2->Lines->Count;
	for (i = 0; i < RowCount; i++)
	{
        // �ּ�ó���� ���� ���ڿ��� ����=0 �̸� �����Ѵ�.
		if (RichEdit2->Lines->Strings[i].Trim().SubString(1, 2) == "--" || RichEdit2->Lines->Strings[i].Trim().Length() == 0 ||
			RichEdit2->Lines->Strings[i].Trim().SubString(1, 1) == "\n")
		{
			continue;
		}

		// SQL���� ������.
		tmp = tmp + RichEdit2->Lines->Strings[i] + " ";

		// �����ݷ��̸� ��������Ѵ�.
		if (tmp.AnsiPos(";") != 0)
		{
			// ���� ���ν����� �Ǵ��� �Ǹ�  �޿� �о���δ� "/" ���ö�����.
			chkProc = findProcedure(tmp);
			if (chkProc == 1)
			{
				for (j = i+1; j < RowCount; j++) {
					tmp = tmp + RichEdit2->Lines->Strings[j] + " ";
					if (tmp.AnsiPos("/") != 0) {
						break ;
					}
				}
                i = j ;
                tmp.Delete(tmp.AnsiPos("/"), 1);
			}


			// ���ο� ���� ����� Executeâ���� �����Ѵ�.
			// ���Ŀ��� �ݵ�� ������ ���� ������� �ؾ� �Ѵ�.
			TForm1 *f1 = new TForm1(Application);
            f1->RichEdit1->Clear();
			for (j=stops; j <=i; j++)
			{
				if (RichEdit2->Lines->Strings[j].Trim().Length() == 0 ||
					RichEdit2->Lines->Strings[j].Trim().SubString(1, 1) == "\n")
				{
					continue;
				}
				f1->RichEdit1->Lines->Add(RichEdit2->Lines->Strings[j]);
			}

            // ���ο� ���� DSN������ �Ѱ��ش�. ���߿� ��������� ������ ����.
			f1->DSN->Caption = DSNLIST->Text;
			if (!f1->dbConnect(f1->DSN->Caption))
				return;
     
			// �ϴ� select�� ���� �������
			f1->ExecNonSelect("alter session set select_header_display = 1", 0);
			if (tmp.TrimLeft().UpperCase().SubString(1, 6) == "SELECT") {
				if (f1->ExecSelect(tmp))
					f1->Show();
				else
                    f1->Close();
			}else {
				f1->ExecNonSelect(tmp, 1);
				f1->Close();
			}

			stops = i+1;
            tmp = "";
			exec_count++;
		}

	}

	// �����ѰǼ��� ������ �����ݷ��� ���� ���ɼ��� 100%������..
	if (exec_count == 0)
	{
		ShowMessage("Statement need a charater ';' ");
		return;
	}
}
//---------------------------------------------------------------------------
void __fastcall TForm6::Memo1DblClick(TObject *Sender)
{
    Memo1->Clear();	
}
//---------------------------------------------------------------------------
// �޸�ڽ����� ���콺 �����ӿ� ���� ������ ����.
void __fastcall TForm6::Memo1MouseDown(TObject *Sender, TMouseButton Button,
      TShiftState Shift, int X, int Y)
{
	TControl *SenderControl = dynamic_cast<TControl *>(Sender);
    int SysCommWparam; 

	// ���� ���콺������ ��ġ�� ����
	if(X < 4 && Y < 4)
        SysCommWparam = SC_DRAG_RESIZEUL; 
    else if(X > SenderControl->Width-4 && Y > SenderControl->Height-4) 
        SysCommWparam = SC_DRAG_RESIZEDR; 
    else if(X < 4 && Y > SenderControl->Height-4) 
        SysCommWparam = SC_DRAG_RESIZEDL; 
    else if(X > SenderControl->Width-4 && Y < 4) 
        SysCommWparam = SC_DRAG_RESIZEUR; 
    else if(X < 4) 
        SysCommWparam = SC_DRAG_RESIZEL; 
    else if(X > SenderControl->Width-4) 
        SysCommWparam = SC_DRAG_RESIZER; 
    else if(Y < 4) 
        SysCommWparam = SC_DRAG_RESIZEU; 
    else if(Y > SenderControl->Height-4) 
        SysCommWparam = SC_DRAG_RESIZED; 


	// �޽���ó��
	ReleaseCapture();
	SendMessage(Memo1->Handle, WM_SYSCOMMAND, SysCommWparam, 0);
	
	
}
//---------------------------------------------------------------------------
// �޸� �ڽ����� ���콺 �����ӿ� ���� ������ ���� ����.
void __fastcall TForm6::Memo1MouseMove(TObject *Sender, TShiftState Shift,
      int X, int Y)
{

	TControl *SenderControl = dynamic_cast<TControl *>(Sender);

	if ( (X < 4 && Y < 4) || (X > SenderControl->Width-4 && Y > SenderControl->Height-4))
        SenderControl->Cursor = crSizeNWSE; 
	else if((X < 4 && Y > SenderControl->Height-4) || (X > SenderControl->Width-4 && Y < 4))
        SenderControl->Cursor = crSizeNESW; 
	else if(X < 4 || X > SenderControl->Width-4)
		SenderControl->Cursor = crSizeWE;
    else if(Y < 4 || Y > SenderControl->Height-4) 
        SenderControl->Cursor = crSizeNS; 
    else 
		SenderControl->Cursor = crDefault;
	
	
}
//---------------------------------------------------------------------------
// ���̶������� ���ؼ� RichEdit���� ���ڰ� ������ ó���Ѵ�.
void __fastcall TForm6::MyKeyDown(TObject *Sender, WORD &Key, TShiftState Shift)
{
	TRichEdit *SenderControl = dynamic_cast<TRichEdit *>(Sender);
    int k, len;
	int x, y;
	AnsiString tmp, tmp2;


	// ������ Ŀ����ġ�� ������ ���´�.
	TPoint oldPos   = SenderControl->CaretPos;

	// Default������ �������̴�.
	TColor oldColor = clBlack;

	// ������ ��ġ���� x, y�� �Ǵ��Ѵ�.
	x = SenderControl->CaretPos.x;
	y = SenderControl->CaretPos.y;


    // Ư��Ű�̸� �����Ѵ�.
	if (Key == DEL_KEY || Key == BACKSPACE || Key == CTRL_KEY
		|| Shift.Contains(ssCtrl)
		|| SenderControl->SelText.Length() > 0)
	{
		return;
	}


	// ��ϵ� �ܾ��Ʈ����
	for (k = 0; k < WORD_COUNT ; k++)
	{
		// ��ϵ� �ܾ��� ����
		len = strlen(WORD_CHK[k]);

		// ���� ©�� ��ġ�� 0���� ������.
		if ( (x-len+1) < 0)
		{
			 continue;
		}

		// �ܾ� ĸ��
		tmp  = SenderControl->Lines->Strings[y].SubString( (x-len+1), len).UpperCase().Trim();
		if (Key != SPACE_KEY && Key != ENTER_KEY)
		{
			tmp = tmp + Key;
		}

		// ���� ������ �� �ѱ���.
		tmp2 = SenderControl->Lines->Strings[y].SubString( (x-len), 1).UpperCase();

		// ���̰� 0���� ū��.. ���ʳ��� Space�� �ƴ϶��
		if ( (x-len) > 0 && tmp2 != " ")
		{
			continue;
		}
		
		// �´� ��ϵ� �ܾ��� ���̶�Ʈ��ó���Ѵ�.
		if (memcmp( tmp.c_str(), WORD_CHK[k], strlen(WORD_CHK[k]) ) == 0)
		{
			// ��ġ����.
			SenderControl->CaretPos = TPoint((x-len), y);
			SenderControl->SelLength = strlen(WORD_CHK[k]);
			// �Ӽ�����.
			SenderControl->SelAttributes->Color = clRed;
			if (tmp == ";")
			{
				SenderControl->SelAttributes->Style.Contains(fsBold);
			}
			break;
		}
	}

    // ������� �������� ã�ư���.
	SenderControl->CaretPos = oldPos;
	SenderControl->SelAttributes->Color = oldColor;

}

//---------------------------------------------------------------------------
void __fastcall TForm6::Exit1Click(TObject *Sender)
{
    Form6->Close();	
}
//---------------------------------------------------------------------------
// DSN �ٽ� �б⸦ ������ 
void __fastcall TForm6::DSNreload1Click(TObject *Sender)
{
	DSNLIST->Clear();
	GetDsnList();
    DSNLIST->ItemIndex = 0;
}
//---------------------------------------------------------------------------
// ���Ͽ��� �о���϶�.
void __fastcall TForm6::OpenFromClick(TObject *Sender)
{
	int i, j, k, start  ;
	AnsiString tmp, tmp2;
	char check21[1024];


    if (!OpenDialog1->Execute())
	{
		return;
	}


	// ���Ͽ��� �о����� ���ο� TabSheet, RichEdit�� ������.
	New1Click(this);
	TRichEdit *SenderControl = dynamic_cast<TRichEdit *>(PageControl1->ActivePage->FindChildControl("Rich_" + PageControl1->ActivePage->Name)) ;

	// ���̶������� ���ؼ��� ������â���� �м����Ŀ�
    // �̸� ������ Copy�Ѵ�.
	RichEdit2->Clear();
	RichEdit2->Lines->LoadFromFile(OpenDialog1->FileName);

	PageControl1->ActivePage->Caption = OpenDialog1->FileName;
	
	// ���̶�����
	for (k=0; k < RichEdit2->Lines->Count; k++)
	{
		tmp = RichEdit2->Lines->Strings[k];
		start = 1;
		for (j = 1; j <= tmp.Length(); j++)
		{

			if (tmp.SubString(j , 1) == " " || tmp.SubString(j, 1) == "\n" || tmp.SubString(j, 1) == ";")
			{
				tmp2 = tmp.SubString(start, j - start).Trim().UpperCase();
				if (tmp2.Length() == 0)
				{
					start = j + 1;
					continue;
				}

				for (i = 0; i < WORD_COUNT; i++)
				{
					// �´� ��ϵ� �ܾ��� ���̶�Ʈ ó���Ѵ�.
					if (memcmp( tmp2.c_str() , WORD_CHK[i], tmp2.Length() ) == 0)
					{
						// ��ġ����.
						RichEdit2->CaretPos = TPoint((start-1), k);
						RichEdit2->SelLength = strlen(WORD_CHK[i]);
						// �Ӽ�����.
						RichEdit2->SelAttributes->Color = clRed;
						break;
					}
				}
				start = j + 1;
				continue;
			}
		}
	}


	// ������� �������� ã�ư���.
	RichEdit2->CaretPos = TPoint(0, 0);
	RichEdit2->SelAttributes->Color = clBlack;


	//RichEdit2->PlainText = true;
	RichEdit2->Lines->SaveToFile(OpenDialog1->FileName);
	SenderControl->Lines->LoadFromFile(OpenDialog1->FileName);

}
//---------------------------------------------------------------------------
// ���������ϱ�
void __fastcall TForm6::SaveFile1Click(TObject *Sender)
{
	TRichEdit *SenderControl = dynamic_cast<TRichEdit *>(PageControl1->ActivePage->FindChildControl("Rich_" + PageControl1->ActivePage->Name)) ;

	// untitle�� �ƴϸ� ���� ���ϸ� ������ �ϰ�..
	if (PageControl1->ActivePage->Caption != "untitle")
	{
		SenderControl->PlainText = true;
		SenderControl->Lines->SaveToFile(PageControl1->ActivePage->Caption);
		SenderControl->PlainText = false;
		return;
	}

	// ���ο�Ÿ� ���ο� ���ϸ�..
	if (!SaveDialog1->Execute())
	{
		return;
	}

    // ���̶����� ���������� �ݵ�� PlainText�Ӽ��� �����ؾ� �Ѵ�.
	PageControl1->ActivePage->Caption = SaveDialog1->FileName;
	SenderControl->PlainText = true;
	SenderControl->Lines->SaveToFile(SaveDialog1->FileName);
	SenderControl->PlainText = false;
}
//---------------------------------------------------------------------------
void __fastcall TForm6::ClearMessage1Click(TObject *Sender)
{
    Memo1->Clear();	
}
//---------------------------------------------------------------------------

void __fastcall TForm6::SaveMessage1Click(TObject *Sender)
{
	if (!SaveDialog1->Execute()) {
		return;
	}

	Memo1->Lines->SaveToFile(SaveDialog1->FileName);
}
//---------------------------------------------------------------------------
// �ٽ��ε�.. �޸� ������ ������ �����̴�. 
void __fastcall TForm6::New1Click(TObject *Sender)
{
	static int row = 1;
	char Names123[100];
	
	RichEdit2->Clear();

	// RichEdit�� ������.
	TRichEdit *rch = new TRichEdit(this);
	rch->Align = alClient;
	rch->BevelInner = bvLowered;
	rch->BevelKind  = bkTile;
	rch->BevelOuter = bvLowered;
	rch->ScrollBars = ssBoth;
	rch->HideScrollBars = false;
	rch->Ctl3D = false;
    rch->PopupMenu = PopupMenu2;
	rch->OnKeyDown = MyKeyDown;
    rch->Text = "";
	
	// ���ο� TabSheet�� ������.
	TTabSheet *tab = new TTabSheet(this);
	tab->Parent = Form6;
	tab->PageControl = PageControl1 ;
	tab->InsertControl(rch);
	tab->Caption = "untitle";

	// FindComponent�� ���� �ϱ� ���ؼ� �̸��� �ϰ����ְ� �����.
	sprintf(Names123, "tab_%d", row);
	tab->Name = Names123;

    sprintf(Names123, "Rich_tab_%d", row);
	rch->Name = Names123;
	rch->Clear();

	// �θ���� �������ش�.
	rch->Parent = tab;

	// ��Ŀ�� ó��
	PageControl1->ActivePage = tab;
	rch->SetFocus();

    // RichEdit�� reading�������� 10M�� �ø���.
	SendMessage(rch->Handle, EM_EXLIMITTEXT, 0, (1024*1024*10));

	row++;
	//TabSheet1->Caption = "untitle";
	//RichEdit1->SetFocus();
}
//---------------------------------------------------------------------------

void __fastcall TForm6::execute1Click(TObject *Sender)
{
    Button1Click(this);	
}
//---------------------------------------------------------------------------

void __fastcall TForm6::Newfile1Click(TObject *Sender)
{
	New1Click(this);
}
//---------------------------------------------------------------------------

void __fastcall TForm6::Save1Click(TObject *Sender)
{
    OpenFromClick(this);	
}
//---------------------------------------------------------------------------

void __fastcall TForm6::Save2Click(TObject *Sender)
{
	SaveFile1Click(this);	
}
//---------------------------------------------------------------------------
// �����ϱ�
void __fastcall TForm6::Copy1Click(TObject *Sender)
{
	TRichEdit *SenderControl = dynamic_cast<TRichEdit *>(PageControl1->ActivePage->FindChildControl("Rich_" + PageControl1->ActivePage->Name)) ;
	SenderControl->CopyToClipboard();
}
//---------------------------------------------------------------------------
// �����ֱ�
void __fastcall TForm6::Paste1Click(TObject *Sender)
{
	TRichEdit *SenderControl = dynamic_cast<TRichEdit *>(PageControl1->ActivePage->FindChildControl("Rich_" + PageControl1->ActivePage->Name)) ;
	SenderControl->PasteFromClipboard();
}
//---------------------------------------------------------------------------
// ���� instance �ݱ�.
void __fastcall TForm6::CloseFile1Click(TObject *Sender)
{

    // ������ ������ 0���̸� �Ʒ� �ڵ�� ������ ���״� ���´�.
	if (PageControl1->PageCount == 0)
	{
        return;    
	}

    // ���� ��Ŀ�̵� �� �����Ѵ�.
	TRichEdit *SenderControl = dynamic_cast<TRichEdit *>(PageControl1->ActivePage->FindChildControl("Rich_" + PageControl1->ActivePage->Name)) ;

	SenderControl->Free() ;
    PageControl1->ActivePage->Free();

}
//---------------------------------------------------------------------------
void __fastcall TForm6::CloseFile2Click(TObject *Sender)
{
	CloseFile1Click(this);
}
//---------------------------------------------------------------------------

