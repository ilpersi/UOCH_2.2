/******************************************************************************\
* 
* 
*  Copyright (C) 2004 Daniel 'Necr0Potenc3' Cavalcanti
* 
* 
*  This program is free software; you can redistribute it and/or modify
*  it under the terms of the GNU General Public License as published by
*  the Free Software Foundation; either version 2 of the License, or
*  (at your option) any later version.
* 
*  This program is distributed in the hope that it will be useful,
*  but WITHOUT ANY WARRANTY; without even the implied warranty of
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*  GNU General Public License for more details.
* 
* 	Dec 28th, 2004 -- UOCH was released 2 years ago, coded in VB. This is the
*   improved version, written in C
*   Jan 16th, 2005 -- added loginkeys searching to Developer's Dump
*   Jan 21th, 2005 -- Changes to CrackTools, added \" \" to all string refs (%s)
*   all vars that hold returns from CrackTools are now initialized as -1
*   changed the "ioh" declaration to avoid warnings/errors
*   wrote the function WriteFileSequence, its cleaner this way
* 
\******************************************************************************/

#include <windows.h>
#include <stdio.h>
#include "resource.h"
#include "Main.h"
#include "CrackTools.h"
#include "Crc.h"


HINSTANCE CurInstance = NULL;
HWND CurWnd = NULL;

unsigned char *ClientBuf = NULL;
unsigned int ClientSize = 0;
unsigned int ImageBase = 0;
unsigned int EntryPoint = 0;
unsigned int TimeDateStamp = 0;


int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, INT nCmdShow)
{
	CurInstance = hInstance;

	DialogBox(CurInstance, MAKEINTRESOURCE(DIALOG_MAIN), 0, (DLGPROC)MainDlgProc);

	return 1;
}

BOOL CALLBACK MainDlgProc(HWND hDlg, UINT wMsg, WPARAM wParam, LPARAM lParam)
{
	CurWnd = hDlg;

	switch(wMsg)
	{
		case WM_INITDIALOG:
		{
			char ClientPath[4096];
			HKEY Key = NULL;
			BOOL RegOK = TRUE;
			DWORD Size = sizeof(ClientPath);

			/* set the UOCH icon :) */
			SendMessage(CurWnd, WM_SETICON, ICON_SMALL, (LPARAM)LoadIcon(CurInstance, MAKEINTRESOURCE(NECRO_ICON)));

			/* you only deserve a client withouth the necro logo if you can patch this :P */
			SendMessage(GetDlgItem(CurWnd, CHECK_LOGO), BM_SETCHECK, BST_CHECKED, 0);

			/* see if we can open uo's registry key */
			if(RegOpenKeyEx(HKEY_LOCAL_MACHINE, "Software\\Origin Worlds Online\\Ultima Online\\1.0", 0, KEY_READ, &Key) != ERROR_SUCCESS)
				if(RegOpenKeyEx(HKEY_LOCAL_MACHINE, "Software\\Origin Worlds Online\\Ultima Online Third Dawn\\1.0", 0, KEY_READ, &Key) != ERROR_SUCCESS)
					RegOK = FALSE;

			/* if we can, get the client path and set it to the text box */
			if(RegOK)
			{
				RegQueryValueEx(Key, "ExePath", NULL, NULL/*type*/, (LPBYTE)ClientPath, &Size);
				RegCloseKey(Key);

				SetDlgItemText(CurWnd, EDIT_PATH, ClientPath);
			}

			return TRUE;
		}break;
		case WM_COMMAND:
		{
			switch(LOWORD(wParam))
			{
				case BUTTON_PATH:
				{
					OPENFILENAME ofn;
					char ClientPath[4096];

					/* ask for the client's path */
					memset(ClientPath, 0, sizeof(ClientPath));
					memset(&ofn, 0, sizeof(ofn));
					ofn.lStructSize = sizeof(OPENFILENAME);
					ofn.hwndOwner = CurWnd;
					ofn.lpstrFile = ClientPath;
					ofn.nMaxFile = sizeof(ClientPath);
					ofn.lpstrFilter = "Exe\0*.Exe\0All\0*.*\0";
					ofn.nFilterIndex = 1;
					ofn.lpstrTitle = "Choose the UO client to be patched";
					ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;
					GetOpenFileName(&ofn);

					/* set it */
					SetDlgItemText(CurWnd, EDIT_PATH, ClientPath);

					return TRUE;
				}break;
				case BUTTON_PATCH:
				{
					IMAGE_DOS_HEADER *idh = NULL;
					IMAGE_FILE_HEADER *ifh = NULL;
					IMAGE_OPTIONAL_HEADER *ioh = NULL;

					HANDLE ClientHandle = NULL;
					DWORD BytesRead = 0;
					char ClientPath[4096], NewClientPath[4096];
					char *Tmp = NULL;
					int i = 0;

					DWORD TickCount = 0;

					memset(NewClientPath, 0, sizeof(NewClientPath));
					GetDlgItemText(CurWnd, EDIT_PATH, ClientPath, sizeof(ClientPath));

					/* copy the client to a buffer */
					ClientHandle = CreateFile(ClientPath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_RANDOM_ACCESS, 0);
					if(ClientHandle == INVALID_HANDLE_VALUE)
					{
						MBOut("Yet another uneducated soul uses me...", "Could not open the file \"%s\". Check if the path is right and the file is not being used", ClientPath);
						EndDialog(CurWnd, 0);
						return TRUE;
					}

					ClientSize = GetFileSize(ClientHandle, NULL);
					ClientBuf = (unsigned char *)malloc(ClientSize);
					if(ClientBuf == NULL)
					{
						MBOut("Shice", "Couldn't allocate enough memory (%d bytes) for the client's image (handle: %X).. weird.", ClientSize, ClientHandle);
						CloseHandle(ClientHandle);
						PostQuitMessage(0);
						return TRUE;
					}

					ReadFile(ClientHandle, ClientBuf, ClientSize, &BytesRead, NULL);
					CloseHandle(ClientHandle);

					/*
					* find the imagebase (virtual address of the module in memory when loaded)
					* and the entrypoint
					*/
					idh = (IMAGE_DOS_HEADER *)ClientBuf;
					ifh = (IMAGE_FILE_HEADER *)(ClientBuf + idh->e_lfanew + sizeof(IMAGE_NT_SIGNATURE));
					ioh = (IMAGE_OPTIONAL_HEADER *)((ClientBuf + idh->e_lfanew + sizeof(IMAGE_NT_SIGNATURE)) + sizeof(IMAGE_FILE_HEADER));

					if((*(DWORD*)(ClientBuf + idh->e_lfanew)) != IMAGE_NT_SIGNATURE)
					{
						MBOut("Ti Durak", "The client is not a valid PE");
						PostQuitMessage(0);
						return TRUE;
					}

					TimeDateStamp = ifh->TimeDateStamp;
					ImageBase = ioh->ImageBase;
					EntryPoint = ioh->AddressOfEntryPoint;

					TickCount = GetTickCount();

					TextBoxCat(CurWnd, TEXT_DUMP, "Patching...\r\n");
					Sleep(25);

					/* developer mode just prints debug info about the client */
					if(SendMessage(GetDlgItem(CurWnd, CHECK_DEV), BM_GETCHECK, 0, 0) == BST_CHECKED)
					{
						DeveloperDump();
						free(ClientBuf);
						ClientBuf = NULL;
						TextBoxCat(CurWnd, TEXT_DUMP, "Time elapsed during analysis: %dms\r\n\r\n", GetTickCount() - TickCount);
						return TRUE;
					}

					/* now check which patches need to be done and execute them */
					if(SendMessage(GetDlgItem(CurWnd, CHECK_EVERMAN), BM_GETCHECK, 0, 0) == BST_CHECKED)
						SetEvermanProtection();

					if(SendMessage(GetDlgItem(CurWnd, CHECK_MULTI), BM_GETCHECK, 0, 0) == BST_CHECKED)
						RemoveMultiProtection();

					if(SendMessage(GetDlgItem(CurWnd, CHECK_NIGHTHACK), BM_GETCHECK, 0, 0) == BST_CHECKED)
						RemoveNightCheck();

					if(SendMessage(GetDlgItem(CurWnd, CHECK_NAMES), BM_GETCHECK, 0, 0) == BST_CHECKED)
						RemoveNameProtection();

					if(SendMessage(GetDlgItem(CurWnd, CHECK_CRYPT), BM_GETCHECK, 0, 0) == BST_CHECKED)
						RemoveEncryption();

					if(SendMessage(GetDlgItem(CurWnd, CHECK_MACRO), BM_GETCHECK, 0, 0) == BST_CHECKED)
						RemoveMacroDelay();

					if(SendMessage(GetDlgItem(CurWnd, CHECK_STAMINA), BM_GETCHECK, 0, 0) == BST_CHECKED)
						RemoveStaminaCheck();

					if(SendMessage(GetDlgItem(CurWnd, CHECK_LOGO), BM_GETCHECK, 0, 0) == BST_CHECKED)
						AddNecroLogo();

					/* create a new client */
					/* remove the exe name from the path */
					if((Tmp = strrchr(ClientPath, '\\')) != NULL)
						*Tmp = '\0';

					sprintf(NewClientPath, "%s\\UOCH_n0p3_Client", ClientPath);
					Tmp = WriteFileSequence(NewClientPath, ClientBuf, ClientSize);

					if(Tmp == NULL)
						TextBoxCat(CurWnd, TEXT_DUMP, "Could not write: \"%s\" GetLastError: %d\r\n", NewClientPath, GetLastError);
					else
					{
						TextBoxCat(CurWnd, TEXT_DUMP, "New UOCH client: \"%s\"\r\n", Tmp);
						TextBoxCat(CurWnd, TEXT_DUMP, "Time elapsed during patching: %dms\r\n\r\n", GetTickCount() - TickCount);
					}

					free(ClientBuf);
					ClientBuf = NULL;

					return TRUE;
				}break;
			}
		}break;
		case WM_DROPFILES:
		{
			char Path[4096];

			/* a text box will only pass the WM_DROPFILES to the owner if it's set as multiline and no accept files flag */
			DragQueryFile((HDROP)wParam, 0, Path, sizeof(Path));
			DragFinish((HDROP)wParam);

			SetDlgItemText(CurWnd, EDIT_PATH, Path);
			return TRUE;
		}break;
		case WM_NOTIFY:
			{
				MBOut("", "noitify");
			}break;
		case WM_CLOSE:
		{
			EndDialog(CurWnd, 0);
			return TRUE;
		}break;
	}

	return FALSE;
}


void TextBoxCat(HWND Dlg, int DlgItem, const char *Text, ...)
{
	char Final[4096];
	va_list List;

	char *NewText = NULL;
	unsigned int NewTextSize = 0;

	va_start(List, Text);
	vsprintf((char*)Final, Text, List);
	va_end(List);

	/* get the dlg text, make sure we have enough space for it */
	NewTextSize += 100;
	NewText = (char*)realloc(NewText, NewTextSize);
	while(GetDlgItemText(Dlg, DlgItem, NewText, NewTextSize) == (int)NewTextSize - 1)
	{
		NewTextSize += 100;
		NewText = (char*)realloc(NewText, NewTextSize);
	}

	NewTextSize += (int)(strlen(Final) + 1);
	NewText = (char*)realloc(NewText, NewTextSize);
	strcat(NewText, Final);
	SetDlgItemText(Dlg, DlgItem, NewText);

	free(NewText);
	return;
}

void MBOut(char *title, char *msg, ...)
{
	char final[4096];
	va_list list;
	va_start(list, msg);
	vsprintf((char*)final, msg, list);
	va_end(list);

	MessageBox(NULL, final, title, 0);
	
	return;
}

/* Path should not end with <.filetype>. returns the name of the new file or NULL */
char* WriteFileSequence(char *Path, char *Buf, unsigned int BufSize)
{
	HANDLE FHandle = INVALID_HANDLE_VALUE;
	DWORD BytesRead = 0;
	static char NewPath[4096];
	int i = 0;

	/* try to find an unexisting name for the new exe */
	for(i = 0; i < 0xFFFF; i++)
	{
		sprintf(NewPath, "%s%02d.exe", Path, i);

		FHandle = CreateFile(NewPath, GENERIC_WRITE, FILE_SHARE_WRITE, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_RANDOM_ACCESS, 0);
		if(FHandle != INVALID_HANDLE_VALUE)
			break;
		else if(FHandle == INVALID_HANDLE_VALUE && GetLastError() != ERROR_FILE_EXISTS)
			return NULL;
	}

	WriteFile(FHandle, Buf, BufSize, &BytesRead, NULL);
	CloseHandle(FHandle);

	return NewPath;
}

void DeveloperDump(void)
{
	/* to find the client's version */
	int FirstID = 0, MiddleID = 0, ThirdID = 0;
	unsigned int StringIDPos = 0;
	unsigned int VersionPos = -1, VersionpushPos = -1;
	char VersionSig[10] = "%d.%d.%d%s";
	char ClientVersion[1024];
	/* 7.x clients read their version from the VS_FIXEDFILEINFO (signature 0xFEEF04BD) */
	unsigned char FixedinfoSig[4] = { 0xBD, 0x04, 0xEF, 0xFE };
	unsigned int FixedinfoPos = -1, FileVersionMS = 0, FileVersionLS = 0;
	BOOL FileVersionFound = FALSE;

	/* for the encryption function */
	unsigned char CryptSig[8] = { 0x81, 0xF9, 0x00, 0x00, 0x01, 0x00, 0x0F, 0x8F };
	/* 7.x clients: CMP ECX,10000 - JG SHORT */
	unsigned char CryptSig2[7] = { 0x81, 0xF9, 0x00, 0x00, 0x01, 0x00, 0x7F };
    unsigned int CryptPos = -1, CryptfuncPos = -1;

	/* for the login keys */
	unsigned char KeysSig[14] = { 0x81, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0x4D, 0x89, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0x89 };
	unsigned int KeysPos = -1, LKey1 = 0, LKey2 = 0;
	/* 7.x clients derive the keys from the version, these are the XOR constants of the formula */
	unsigned char KeyformulaSig1[4] = { 0xFD, 0xA5, 0x13, 0x2C };
	unsigned char KeyformulaSig2[4] = { 0x7F, 0x52, 0x1D, 0xA3 };
	unsigned int KeyformulaPos = -1, Major = 0, Minor = 0, Build = 0;

	/* for the packet handler */
	unsigned char PackethandlerSig[12] = { 0x8A, 0x06, 0x83, 0xC0, 0xCC, 0x3D, 0xCC, 0x00, 0x00, 0x00, 0x0F, 0x87 };
	/* 7.x clients: MOVZX EAX,BYTE PTR [ESI] instead of MOV AL,[ESI] */
	unsigned char PackethandlerSig2[13] = { 0x0F, 0xB6, 0xCC, 0x83, 0xC0, 0xCC, 0x3D, 0xCC, 0x00, 0x00, 0x00, 0x0F, 0x87 };
	unsigned int PackethandlerPos = -1, PackethandlerfuncPos = -1, PackethandlerbadfuncPos = -1;

	/* for the packet len dump */
	/* old clients use PACKET ID, LEN as the packet structure */
	unsigned char OldlenSig[8 * 2] = { 0x00, 0x00, 0x00, 0x00, 0x68, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00 };
	/* newer clients use PACKET ID, SOME ADDRESS, LEN as the packet structure */
	unsigned char NewlenSig[12 * 2] = { 0x00, 0x00, 0x00, 0x00, 0xCC, 0xCC, 0xCC, 0xCC, 0x68, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0xCC, 0xCC, 0xCC, 0xCC, 0x05, 0x00, 0x00, 0x00 };
	/*
	* 7.x clients use the same structure, but the entry for 0x00 is labelled 0xF8
	* (len 0x6A, the new create character packet), so match from the 0x01 entry on
	*/
	unsigned char NewlenSig2[12 * 2] = { 0x01, 0x00, 0x00, 0x00, 0xCC, 0xCC, 0xCC, 0xCC, 0x05, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0xCC, 0xCC, 0xCC, 0xCC, 0x07, 0x00, 0x00, 0x00 };
	unsigned int PackettablePos = -1;
	int Skip = 8; /* skip <Skip> bytes when reading the table */
	int i = 0, LastTablePkt = 0, PktLen = 0, PktID = 0, LastPktID = -1;
	BOOL TableFinished = FALSE;

	TextBoxCat(CurWnd, TEXT_DUMP, "\r\n==========================================\r\n");
	TextBoxCat(CurWnd, TEXT_DUMP, "Dev dump: Client executable CRC32 %08X\r\n", CalculateCRC32(ClientBuf, ClientSize));
	TextBoxCat(CurWnd, TEXT_DUMP, "Dev dump: Client executable size %X\r\n", ClientSize);
	TextBoxCat(CurWnd, TEXT_DUMP, "Dev dump: Client time date stamp %X\r\n", TimeDateStamp);
	TextBoxCat(CurWnd, TEXT_DUMP, "Dev dump: Client image base %X\r\n", ImageBase);
	TextBoxCat(CurWnd, TEXT_DUMP, "Dev dump: Client entry point @%X\r\n", EntryPoint);

	/*
	* find the client version
	* all clients follow the format a.b.cs
	* s is a string, not a char
	*/
	strcpy(ClientVersion, "Unknown");

	/* the VERSIONINFO resource, if there's one: VS_FIXEDFILEINFO is signature, struct version, MS, LS */
	FixedinfoPos = FleXSearch(FixedinfoSig, ClientBuf, sizeof(FixedinfoSig), ClientSize, 0, 0x100, 1);
	if(FixedinfoPos != -1 && FixedinfoPos + 16 <= ClientSize && *(DWORD*)(ClientBuf + FixedinfoPos + 4) == 0x00010000)
	{
		memcpy(&FileVersionMS, ClientBuf + FixedinfoPos + 8, sizeof(DWORD));
		memcpy(&FileVersionLS, ClientBuf + FixedinfoPos + 12, sizeof(DWORD));
		FileVersionFound = TRUE;
	}

	VersionPos = FleXSearch("1.25.", ClientBuf, 5, ClientSize, 0, 0x100, 1);
	if(VersionPos == -1)
		VersionPos = FleXSearch("1.26.", ClientBuf, 5, ClientSize, 0, 0x100, 1);
	if(VersionPos == -1)
		VersionPos = FleXSearch("2.0.", ClientBuf, 4, ClientSize, 0, 0x100, 1);

	if(VersionPos != -1)
		strcpy(ClientVersion, (const char*)(ClientBuf + VersionPos));
	else
	{
		VersionpushPos = GetTextRef(ClientBuf, ClientSize, ImageBase, VersionSig, TRUE, 1);

		if((int)VersionpushPos < 0 && FileVersionFound)
		{
			/* 7.x clients: no "%d.%d.%d%s", the version is in the VERSIONINFO resource */
			sprintf(ClientVersion, "%d.%d.%d.%d", HIWORD(FileVersionMS), LOWORD(FileVersionMS), HIWORD(FileVersionLS), LOWORD(FileVersionLS));
		}
		else if((int)VersionpushPos < 0)
			TextBoxCat(CurWnd, TEXT_DUMP, "Dev dump: Could not a reference to \"%s\" return was %d \r\n", VersionSig, VersionpushPos);
		else
		{
			/* copy the a.b.c */
			memcpy(&FirstID, (void*)(ClientBuf + VersionpushPos - 1), 1);
			memcpy(&MiddleID, (void*)(ClientBuf + VersionpushPos - 3), 1);
			memcpy(&ThirdID, (void*)(ClientBuf + VersionpushPos - 5), 1);
			/* copy the string's offset	*/
			memcpy(&StringIDPos, (void*)(ClientBuf + VersionpushPos - 10), 4);
			StringIDPos -= ImageBase; /* remove the base as from being "in memory" */

			sprintf(ClientVersion, "%d.%d.%d%s", FirstID, MiddleID, ThirdID, ClientBuf + StringIDPos);
		}
	}

	TextBoxCat(CurWnd, TEXT_DUMP, "Dev dump: Client version is \"%s\"\r\n", ClientVersion);


	/* get the login encrypt info */
	CryptPos = FleXSearch(CryptSig, ClientBuf, sizeof(CryptSig), ClientSize, 0, 0x100, 1);
	if(CryptPos == -1)
		CryptPos = FleXSearch(CryptSig2, ClientBuf, sizeof(CryptSig2), ClientSize, 0, 0x100, 1);

	if(CryptPos == -1)
		TextBoxCat(CurWnd, TEXT_DUMP, "Dev dump: Could not find the crypt signature\r\n");
	else
	{
		CryptfuncPos = GetFunctionByRef(ClientBuf, ClientSize, CryptPos);
		TextBoxCat(CurWnd, TEXT_DUMP, "Dev dump: Login encryption function @%X\r\n", CryptfuncPos);
		TextBoxCat(CurWnd, TEXT_DUMP, "Dev dump: Login encryption signature @%X\r\n", CryptPos);
	}

	/*
	* get the login keys
	* client 4.0.4b
	* 00418FB3  |. 35 BD85F32E    |XOR EAX,2EF385BD
	* 00418FB8  |. 81F1 7F126DA2  |XOR ECX,A26D127F
	* 00418FBE  |. 4D             |DEC EBP
	* 00418FBF  |. 8983 F8000A00  |MOV DWORD PTR DS:[EBX+A00F8],EAX
	* 00418FC5  |. 898B F4000A00  |MOV DWORD PTR DS:[EBX+A00F4],ECX
	*
	* the above is found after the crypt sig
	* find the 2nd login key cause the first one has a different opcode in different client versions
	* the 4 bytes before are lkey1, the 4 bytes after (KeyPos + 1) are lkey2
	*/
	if(CryptPos != -1)
		KeysPos = FleXSearch(KeysSig, ClientBuf, sizeof(KeysSig), ClientSize, CryptPos, 0xCC, 1);

	/*
	* client 7.0.118.46
	* 7.x clients have no hard coded keys, they compute them from the version
	* in the VERSIONINFO resource. key1 is calculated like this (key2 is similar):
	* 0069F850  8B0D 7C698D00   MOV ECX,DWORD PTR DS:[8D697C]   ; build
	* 0069F856  A1 78698D00     MOV EAX,DWORD PTR DS:[8D6978]   ; minor
	* 0069F85B  8B15 74698D00   MOV EDX,DWORD PTR DS:[8D6974]   ; major
	* ...
	* 0069F890  81F2 FDA5132C   XOR EDX,2C13A5FD
	* 0069F896  8915 84698D00   MOV DWORD PTR DS:[8D6984],EDX   ; key1
	*
	* if the XOR constants are there, do the same math here
	*/
	if((KeysPos == -1 || CryptPos == -1) && FileVersionFound)
	{
		KeyformulaPos = FleXSearch(KeyformulaSig1, ClientBuf, sizeof(KeyformulaSig1), ClientSize, 0, 0x100, 1);
		if(KeyformulaPos != -1 && FleXSearch(KeyformulaSig2, ClientBuf, sizeof(KeyformulaSig2), ClientSize, 0, 0x100, 1) == -1)
			KeyformulaPos = -1;
	}

	if((KeysPos == -1 || CryptPos == -1) && KeyformulaPos != -1)
	{
		Major = HIWORD(FileVersionMS);
		Minor = LOWORD(FileVersionMS);
		Build = HIWORD(FileVersionLS);

		LKey1 = (Major << 23) | (Minor << 14) | (Build << 4);
		LKey1 ^= (Build * Build) << 9;
		LKey1 ^= Minor * Minor;
		LKey1 ^= (Minor * 11) << 24;
		LKey1 ^= (Build * 7) << 19;
		LKey1 ^= 0x2C13A5FD;

		LKey2 = (Major << 22) | (Build << 13) | (Minor << 3);
		LKey2 ^= (Build * Build * 3) << 10;
		LKey2 ^= Minor * Minor;
		LKey2 ^= (Minor * 13) << 23;
		LKey2 ^= (Build * 7) << 18;
		LKey2 ^= 0xA31D527F;

		TextBoxCat(CurWnd, TEXT_DUMP, "Dev dump: Login keys are derived from the version (formula @%X)\r\n", KeyformulaPos);
		TextBoxCat(CurWnd, TEXT_DUMP, "Dev dump: Login key1: %X\r\n", LKey1);
		TextBoxCat(CurWnd, TEXT_DUMP, "Dev dump: Login key2: %X\r\n", LKey2);
	}
	else if(KeysPos == -1 || CryptPos == -1)
		TextBoxCat(CurWnd, TEXT_DUMP, "Dev dump: Could not find the loginkeys signature\r\n");
	else
	{
		memcpy(&LKey1, ClientBuf + (KeysPos - 4), sizeof(unsigned int));
		memcpy(&LKey2, ClientBuf + (KeysPos + 2), sizeof(unsigned int));

		TextBoxCat(CurWnd, TEXT_DUMP, "Dev dump: Login key signature @%X\r\n", KeysPos);
		TextBoxCat(CurWnd, TEXT_DUMP, "Dev dump: Login key1: %X\r\n", LKey1);
		TextBoxCat(CurWnd, TEXT_DUMP, "Dev dump: Login key2: %X\r\n", LKey2);
	}

	/* get the packet handler info */
	PackethandlerPos = FleXSearch(PackethandlerSig, ClientBuf, sizeof(PackethandlerSig), ClientSize, 0, 0xCC, 1);
	if(PackethandlerPos == -1)
		PackethandlerPos = FleXSearch(PackethandlerSig2, ClientBuf, sizeof(PackethandlerSig2), ClientSize, 0, 0xCC, 1);

	if(PackethandlerPos == -1)
		TextBoxCat(CurWnd, TEXT_DUMP, "Dev dump: Could not find the packet handler signature\r\n");
	else
	{
		PackethandlerfuncPos = GetFunctionByRef(ClientBuf, ClientSize, PackethandlerPos);
		PackethandlerbadfuncPos = GetFunctionUnsafe(ClientBuf, ClientSize, PackethandlerPos, 4);
		TextBoxCat(CurWnd, TEXT_DUMP, "Dev dump: Packet handler function @%X or @%X\r\n", PackethandlerfuncPos, PackethandlerbadfuncPos);
		TextBoxCat(CurWnd, TEXT_DUMP, "Dev dump: Packet handler signature @%X\r\n", PackethandlerPos);
	}

	/* get the packet table and dump it (this is a real... suka :P) */
	PackettablePos = FleXSearch(OldlenSig, ClientBuf, sizeof(OldlenSig), ClientSize, 0, 0x100, 1);
	if(PackettablePos == -1)
	{
		PackettablePos = FleXSearch(NewlenSig, ClientBuf, sizeof(NewlenSig), ClientSize, 0, 0xCC, 1);
		Skip = 12;
	}
	if(PackettablePos == -1)
	{
		/* the match starts at the 0x01 entry, go back to the 0x00 entry */
		PackettablePos = FleXSearch(NewlenSig2, ClientBuf, sizeof(NewlenSig2), ClientSize, 0, 0xCC, 1);
		if(PackettablePos != -1)
			PackettablePos -= Skip;
	}

	if(PackettablePos == -1)
		TextBoxCat(CurWnd, TEXT_DUMP, "Dev dump: Could not find the packet tables\r\n");
	else
	{
		TextBoxCat(CurWnd, TEXT_DUMP, "Dev dump: Packet table found @%X\r\n", PackettablePos);

		/* 7.x clients: the client still looks up 0x00 by index, the label is just a name */
		memcpy(&PktID, ClientBuf + PackettablePos, sizeof(int));
		if(PktID != 0)
			TextBoxCat(CurWnd, TEXT_DUMP, "Dev dump: Table entry 0x00 is labelled as packet 0x%02X\r\n", PktID);

		TextBoxCat(CurWnd, TEXT_DUMP, "static int PktLenTable[] = \r\n{");

		for(i = 0; i < 0x100; i++)
		{
			PktID = i; PktLen = 0;

			/* copy the packet id and len from the image */
			if(!TableFinished)
			{
				memcpy(&PktID, ClientBuf + PackettablePos + (i * Skip), sizeof(int));
				memcpy(&PktLen, ClientBuf + PackettablePos + (i * Skip) + (Skip - 4), sizeof(int));
			}

			/* if the pktid we just read is not in sequence, the table is finished (entry 0x00 may have another label) */
			if(PktID != i && i != 0 && !TableFinished)
			{
				TableFinished = TRUE;
				LastTablePkt = i - 1;
				PktID = i; PktLen = 0;
			}

			/* 0x00, 0x10, 0x20, 0x30, 0x40... 0xf0 */
			if(i == 0x00 || i % 0x10 == 0)
				TextBoxCat(CurWnd, TEXT_DUMP, "\r\n\t/* 0x%02x */ ", i);

			/* dump the packet's len */
			TextBoxCat(CurWnd, TEXT_DUMP, "0x%04X", PktLen);
			/* avoid writting the comma at the last packet */
			if(i != 0xff)
				TextBoxCat(CurWnd, TEXT_DUMP, ", ", PktLen);
		}

		TextBoxCat(CurWnd, TEXT_DUMP, "\r\n};\r\n", i);
		TextBoxCat(CurWnd, TEXT_DUMP, "Dev dump: Last packet in client's table %X\r\n", LastTablePkt);
	}

	TextBoxCat(CurWnd, TEXT_DUMP, "==========================================\r\n");

	return;
}

void SetEvermanProtection(void)
{
	/*
	* client 4.0.4b
	* 0044DE18  |. 68 5CEF5800    PUSH client40.0058EF5C ;  ASCII "Permanently delete %s?"
	* 
	* search for PUSH <offset of "Permanently delete %s?">
	* and write a INT3-RETN to the beginning of the function
	* if the start of the function wasnt found, write a INT3 and 50 NOPs
	*/

	unsigned char BreaknRet[2] = { 0xCC, 0xC3 };
	unsigned int DeletePos = -1, FunctionPos = -1;

	DeletePos = GetTextRef(ClientBuf, ClientSize, ImageBase, "Permanently delete %s?", TRUE, 1);
	if((int)DeletePos < 0)
	{
		TextBoxCat(CurWnd, TEXT_DUMP, "Everman's way: Could not find \"Permanently delete %%s\" return was %d\r\n", DeletePos);
		return;
	}

	/* find the start of the function that contains the push */
    FunctionPos = GetFunctionByRef(ClientBuf, ClientSize, DeletePos);

	/* if we couldn't find it, write 1 INT3 and 50 NOPs*/
	if((int)FunctionPos == -1)
	{
		memset(ClientBuf + DeletePos, 0xCC, 1);
		memset(ClientBuf + DeletePos + 1, 0x90, 50);
		TextBoxCat(CurWnd, TEXT_DUMP, "Everman's way: Patched using old method %X\r\n", GetTickCount());
		return;
	}

	/* this will still cause a crash, but more effectively ;) */
    memcpy(ClientBuf + FunctionPos, BreaknRet, sizeof(BreaknRet));
	TextBoxCat(CurWnd, TEXT_DUMP, "Everman's way: Patched with bpx and ret @%X\r\n", FunctionPos);

	return;
}

void RemoveMultiProtection(void)
{
	/*
	* client 4.0.4b
	* 004D463D  |. 53             PUSH EBX                                 ; /Title => NULL
	* 004D463E  |. 50             PUSH EAX                                 ; |Class => "Ultima Online"
	* 004D463F  |. FF15 48024F00  CALL NEAR DWORD PTR DS:[<&USER32.FindWin>; \FindWindowA
	* 004D4645  |. 85C0           TEST EAX,EAX
	* 004D4647  |. 74 15          JE SHORT Client.004D465E
	* 004D4649  |. 6A 40          PUSH 40                                  ; /Style = MB_OK|MB_ICONASTERISK|MB_APPLMODAL
	* 004D464B  |. 53             PUSH EBX                                 ; |Title => NULL
	* 004D464C  |. 68 FC215400    PUSH Client.005421FC                     ; |Text = "Another copy of UO is already running!"
	* 004D4651  |. 53             PUSH EBX                                 ; |hOwner => NULL
	* 004D4652  |. FF15 44024F00  CALL NEAR DWORD PTR DS:[<&USER32.Message>; \MessageBoxA
	* 
	* find a reference to "Another copy of UO is already running!"
	* and patch the first TEST EAX,EAX-JE XXX above it to CMP EAX,EAX (85 -> 3B)
    * 
	* and:
	* 004D48B5  |. 3D B7000000    CMP EAX,0B7
	* 004D48BA  |. 75 1B          JNZ SHORT Client.004D48D7
	* 004D48BC  |. 8B0D 60CA5400  MOV ECX,DWORD PTR DS:[54CA60]
	* 004D48C2  |. 51             PUSH ECX                                 ; /hObject => NULL
	* 004D48C3  |. FFD5           CALL NEAR EBP                            ; \CloseHandle
	* 004D48C5  |. 53             PUSH EBX                                 ; /Style
	* 004D48C6  |. 68 A8205400    PUSH Client.005420A8                     ; |Title = "ERROR"
	* 004D48CB  |. 68 3CCC5400    PUSH Client.0054CC3C                     ; |Text = "Another instance of UO is already running."
	* 004D48D0  |. 53             PUSH EBX                                 ; |hOwner
	* 004D48D1  |. FFD6           CALL NEAR ESI                            ; \MessageBoxA
	* 004D48D3  |. 6A 01          PUSH 1                                   ; /ExitCode = 1
	* 004D48D5  |. FFD7           CALL NEAR EDI                            ; \ExitProcess
	* 
	* find a reference to "Another instance of UO is already running."
	* and patch the first JNZ XXX above it to JMP (75 -> EB)
	*/

	unsigned char CmpeaxSig[3] = { 0x85, 0xC0, 0x74 };
	unsigned int AnothercopyPos = -1, CmpeaxPos = -1;
	unsigned char CmpJNZ[6] = { 0x3D, 0xB7, 0x00 ,0x00, 0x00, 0x75 };
	unsigned int AnotherinstPos = -1, CmpJNZPos = -1;

	/* find the CmpEaxSig in a range of 16 bytes before the PUSH */
	AnothercopyPos = GetTextRef(ClientBuf, ClientSize, ImageBase, "Another copy of UO is already running!", TRUE, 1);
	if((int)AnothercopyPos < 0)
	{
		/* 7.x clients don't have this message, they ask instead */
		if(RemoveMultiPrompt())
			return;

		TextBoxCat(CurWnd, TEXT_DUMP, "Multi: Could not find \"Another copy of UO is already running!\" return was %d\r\n", AnothercopyPos);
		return;
	}

    CmpeaxPos = FleXSearch(CmpeaxSig, ClientBuf, sizeof(CmpeaxSig), ClientSize, AnothercopyPos - 0x10, 0xCC, 1);
	if(CmpeaxPos == -1)
	{
		TextBoxCat(CurWnd, TEXT_DUMP, "Multi: Could not find a TEST EAX,EAX-JE %X\r\n", AnothercopyPos);
		return;
	}

	/* find the CMP EAX,B7-JNZ in a range of 32 bytes before the PUSH */
	AnotherinstPos = GetTextRef(ClientBuf, ClientSize, ImageBase, "Another instance of UO is already running.", 1, 1);
	if((int)AnotherinstPos < 0)
	{
		TextBoxCat(CurWnd, TEXT_DUMP, "Multi: Could not find \"Another instance of UO is already running.\" return was %d\r\n", AnotherinstPos);
		return;
	}

    CmpJNZPos = FleXSearch(CmpJNZ, ClientBuf, sizeof(CmpJNZ), ClientSize, AnotherinstPos - 0x20, 0xCC, 1);
	if(CmpJNZPos == -1)
	{
		TextBoxCat(CurWnd, TEXT_DUMP, "Multi: Could not find a CMP EAX,37-JNZ %X\r\n", AnotherinstPos);
		return;
	}

	/* change the TEST (0x80) to CMP (0x3B) */
    ClientBuf[CmpeaxPos] = 0x3B;
	TextBoxCat(CurWnd, TEXT_DUMP, "Multi: Patched with cmp @%X\r\n", CmpeaxPos);

	/* change the JNZ (0x75) to JMP (0xEB) */
	ClientBuf[CmpJNZPos + 5] = 0xEB;
	TextBoxCat(CurWnd, TEXT_DUMP, "Multi: Patched with jmp @%X\r\n", CmpJNZPos + 5);

	return;
}

/* returns FALSE if the client doesn't have the 7.x question */
BOOL RemoveMultiPrompt(void)
{
	/*
	* client 7.0.118.46
	* 7.x clients let you run more than one client, but they ask first. the question is asked from 3 places
	*
	* WinMain, if a window of the class "Ultima Online" exists:
	* 0060AB75  |. FF15 24246A00  CALL NEAR DWORD PTR DS:[<&USER32.FindWindowA>]
	* 0060AB7B  |. 85C0           TEST EAX,EAX
	* 0060AB7D  |. 8B1D 98246A00  MOV EBX,DWORD PTR DS:[<&USER32.MessageBoxA>]
	* 0060AB83  |. 74 1C          JE SHORT client.0060ABA1
	* 0060AB85  |. 6A 04          PUSH 4                                   ; /Style = MB_YESNO
	* 0060AB87  |. 68 008C6C00    PUSH client.006C8C00                     ; |Title = "Multiple Instances Running"
	* 0060AB8C  |. 68 508B6C00    PUSH client.006C8B50                     ; |Text = "Running two instances of Ultima Online at once is an UNSUPPORTED feature..."
	*
	* when CreateFileMappingA(..., "UoClientApp") fails:
	* 0060AE64  |. 3BF5           CMP ESI,EBP
	* 0060AE66  |. 75 1C          JNZ SHORT client.0060AE84
	* 0060AE68  |. 6A 04          PUSH 4
	* 0060AE6A  |. 68 008C6C00    PUSH client.006C8C00
	* 0060AE6F  |. 68 508B6C00    PUSH client.006C8B50
	*
	* and a function called when the mapping already exists (ERROR_ALREADY_EXISTS):
	* 006070CF     CC             INT3
	* 006070D0  /$ 6A 04          PUSH 4
	* 006070D2  |. 68 008C6C00    PUSH client.006C8C00
	* 006070D7  |. 68 508B6C00    PUSH client.006C8B50
	* ...
	* 006070F1  \. C3             RETN
	*
	* for each PUSH of the text: if a JE/JNZ right before the PUSH 4 jumps over the question,
	* patch it to JMP (74/75 -> EB). if the PUSH 4 starts a function, make it return (6A -> C3)
	*/

	const char *PromptText = "Running two instances of Ultima Online at once is an UNSUPPORTED feature.";
	unsigned int PromptPos = -1, AskPos = -1;
	int Which = 0, Patched = 0;

	PromptPos = GetTextRef(ClientBuf, ClientSize, ImageBase, PromptText, FALSE, 1);
	if((int)PromptPos == -2)
		return FALSE;

	for(Which = 1; (int)(PromptPos = GetTextRef(ClientBuf, ClientSize, ImageBase, PromptText, FALSE, Which)) >= 0; Which++)
	{
		/* PUSH 4 - PUSH <title> - PUSH <text> */
		AskPos = PromptPos - 7;
		if(PromptPos < 9 || ClientBuf[AskPos] != 0x6A || ClientBuf[AskPos + 1] != 0x04 || ClientBuf[AskPos + 2] != 0x68)
		{
			TextBoxCat(CurWnd, TEXT_DUMP, "Multi: The question @%X is not preceded by a PUSH 4\r\n", PromptPos);
			continue;
		}

		/* a short JE/JNZ that lands after the PUSH <text> */
		if((ClientBuf[AskPos - 2] == 0x74 || ClientBuf[AskPos - 2] == 0x75) &&
			ClientBuf[AskPos - 1] < 0x80 && AskPos + ClientBuf[AskPos - 1] >= PromptPos + 5)
		{
			ClientBuf[AskPos - 2] = 0xEB;
			TextBoxCat(CurWnd, TEXT_DUMP, "Multi: Patched with jmp @%X\r\n", AskPos - 2);
			Patched++;
		}
		else if(ClientBuf[AskPos - 1] == 0xCC || ClientBuf[AskPos - 1] == 0x90)
		{
			ClientBuf[AskPos] = 0xC3;
			TextBoxCat(CurWnd, TEXT_DUMP, "Multi: Patched with ret @%X\r\n", AskPos);
			Patched++;
		}
		else
			TextBoxCat(CurWnd, TEXT_DUMP, "Multi: Could not find a JE/JNZ before the question @%X\r\n", PromptPos);
	}

	if(Patched == 0)
		TextBoxCat(CurWnd, TEXT_DUMP, "Multi: Could not patch the \"Running two instances\" question\r\n");

	return TRUE;
}

void RemoveNightCheck(void)
{
	/*
	* clients >= 3.0.4n
	* client 4.0.4b
	* 0041CA97     25 FF000000    AND EAX,0FF
	* 0041CA9C   . 83C4 0C        ADD ESP,0C
	* 0041CA9F   . 3BC8           CMP ECX,EAX
	* 0041CAA1   . 74 2F          JE SHORT Client40.0041CAD2
	* 
	* clients <= 3.0.3a
	* client 2.0.0g
	* 0048FEC1     25 FF000000    AND EAX,0FF
	* 0048FEC6   . 3BC8           CMP ECX,EAX
	* 0048FEC8   . 74 2C          JE SHORT Client.0048FEF6
	* 
	* The client gets the light value from the 0x4f packet (Packet[1] & 0xff)
	* and compares it to the current value (ECX) and changes only if
	* the two values are different (JE)
	* change the AND EAX,FF to AND EAX,0 (25 FF -> 25 00)
	* this will set the value to 0
	*/

	/*
	* client 7.0.118.46
	* 0046BBCF  |. 8A4C24 0F      MOV CL,BYTE PTR SS:[ESP+F]
	* 0046BBD3  |. 0FB6C1         MOVZX EAX,CL
	* 0046BBD6  |. 83C4 0C        ADD ESP,0C
	* 0046BBD9  |. 3905 F46B7900  CMP DWORD PTR DS:[796BF4],EAX
	* 0046BBDF  |. 74 1E          JE SHORT client.0046BBFF
	* 0046BBE1  |. 80F9 1F        CMP CL,1F
	* 0046BBE4  |. 73 19          JNB SHORT client.0046BBFF
	*
	* same thing, but there's no AND to patch
	* change the MOVZX EAX,CL to XOR EAX,EAX - NOP (0F B6 C1 -> 33 C0 90)
	*/

	unsigned char LightSig[11] = { 0x25, 0xFF, 0x00, 0x00, 0x00, 0x83, 0xC4, 0x0C, 0x3B, 0xC8, 0x74 };
	unsigned char LightSig2[8] = { 0x25, 0xFF, 0x00, 0x00, 0x00, 0x3B, 0xC8, 0x74 };
	unsigned char LightSig3[18] = { 0x0F, 0xB6, 0xC1, 0x83, 0xC4, 0x0C, 0x39, 0x05, 0xCC, 0xCC, 0xCC, 0xCC, 0x74, 0xCC, 0x80, 0xF9, 0x1F, 0x73 };
	unsigned char XorEax[3] = { 0x33, 0xC0, 0x90 };
	unsigned int LightPos = -1;

	/* search for both sigs */
	LightPos = FleXSearch(LightSig, ClientBuf, sizeof(LightSig), ClientSize, 0, 0x100, 1);
	if(LightPos == -1)
		LightPos = FleXSearch(LightSig2, ClientBuf, sizeof(LightSig2), ClientSize, 0, 0x100, 1);

	if(LightPos == -1)
	{
		/* 7.x clients */
		LightPos = FleXSearch(LightSig3, ClientBuf, sizeof(LightSig3), ClientSize, 0, 0xCC, 1);
		if(LightPos != -1)
		{
			memcpy(ClientBuf + LightPos, XorEax, sizeof(XorEax));
			TextBoxCat(CurWnd, TEXT_DUMP, "Night hack: Patched with xor @%08X\r\n", LightPos);
			return;
		}

		TextBoxCat(CurWnd, TEXT_DUMP, "Night hack: Could not find the light signature\r\n");
		return;
	}

	ClientBuf[LightPos + 1] = 0x00;
	TextBoxCat(CurWnd, TEXT_DUMP, "Night hack: Patched @%08X\r\n", LightPos);

	return;
}

void RemoveNameProtection(void)
{
	/*
	* client 4.0.4b
	* 0041F362   . 83F9 02        CMP ECX,2
	* 0041F365   . 73 26          JNB SHORT Client.0041F38D
	* 0041F367   . 50             PUSH EAX
	* 0041F368   . 50             PUSH EAX
	* 0041F369   . 6A 01          PUSH 1
	* 0041F36B   . 53             PUSH EBX
	* 0041F36C   . 68 D0865000    PUSH Client.005086D0                     ;  ASCII "Your character name is too short."
	* 0041F371   . 8943 6C        MOV DWORD PTR DS:[EBX+6C],EAX
	* 0041F374   . E8 57830300    CALL Client.004576D0
	* 0041F379   . 83C4 14        ADD ESP,14
	* 0041F37C   . 5F             POP EDI
	* 0041F37D   . 5B             POP EBX
	* 0041F37E   . 8B4C24 04      MOV ECX,DWORD PTR SS:[ESP+4]
	* 0041F382   . 64:890D 000000>MOV DWORD PTR FS:[0],ECX
	* 0041F389   . 83C4 10        ADD ESP,10
	* 0041F38C   . C3             RETN
	* 0041F38D   > 52             PUSH EDX
	* 0041F38E   . E8 7DF50500    CALL Client.0047E910
	* 0041F393   . 83C4 04        ADD ESP,4
	* 0041F396   . 85C0           TEST EAX,EAX
	* 0041F398   . 74 2C          JE SHORT Client.0041F3C6
	* 0041F39A   . 6A 00          PUSH 0
	* 0041F39C   . 6A 00          PUSH 0
	* 0041F39E   . 6A 01          PUSH 1
	* 0041F3A0   . 53             PUSH EBX
	* 0041F3A1   . 68 BC865000    PUSH Client.005086BC                     ;  ASCII "Unacceptable name."
	* 
	* There are 2 cool name protections in the client. only the second,
	* located just bellow "Your character name is too short." is used
	* 
	* We could search for the strings, but this method is easier
	* To remove the short names protection:
	* 83 f9 02 73 XX 50 50
	* To remove the cool names protection:
	* 85 c0 74 bellow short names protection
	*/
	/*
	* client 7.0.118.46
	* 005519DE  |> 83FF 02        CMP EDI,2
	* 005519E1  |. 7D 2C          JGE SHORT client.00551A0F
	* 005519E3  |. 53             PUSH EBX
	* 005519E4  |. 53             PUSH EBX
	* 005519E5  |. 53             PUSH EBX
	* 005519E6  |. 6A 01          PUSH 1
	* 005519E8  |. 56             PUSH ESI
	* 005519E9  |. 68 98EE6B00    PUSH client.006BEE98                     ;  ASCII "Your character name is too short."
	* ...
	* 00551A1C  |. E8 AF7E0500    CALL client.005A98D0
	* 00551A21  |. 83C4 04        ADD ESP,4
	* 00551A24  |. 85C0           TEST EAX,EAX
	* 00551A26  |. 74 2C          JE SHORT client.00551A54
	* 00551A28  |. 53             PUSH EBX
	* ...
	* 00551A2E  |. 68 AC966B00    PUSH client.006B96AC                     ;  ASCII "Unacceptable name."
	*
	* the 7.x clients use a JGE and other registers, so find the CMP <reg>,2 - JGE
	* in a range of 32 bytes before the "too short" PUSH and patch the JGE (0x7D) to JMP (0xEB)
	* the cool names protection is the same
	*/
	unsigned char ShortSig[7] = { 0x83, 0xF9, 0x02, 0x73, 0xCC, 0x50, 0x50 };
	unsigned char ShortSig2[4] = { 0x83, 0xCC, 0x02, 0x7D };
	unsigned char CoolSig[3] = { 0x85, 0xC0, 0x74 };
	unsigned int ShortPos = -1, CoolPos = -1, ShortrefPos = -1;

    ShortPos = FleXSearch(ShortSig, ClientBuf, sizeof(ShortSig), ClientSize, 0, 0xCC, 1);

	if(ShortPos == -1)
	{
		/* 7.x clients */
		ShortrefPos = GetTextRef(ClientBuf, ClientSize, ImageBase, "Your character name is too short.", TRUE, 1);
		if((int)ShortrefPos >= 0x20)
		{
			ShortPos = FleXSearch(ShortSig2, ClientBuf, sizeof(ShortSig2), ClientSize, ShortrefPos - 0x20, 0xCC, 1);
			if(ShortPos >= ShortrefPos)
				ShortPos = -1;
		}
	}

	CoolPos = FleXSearch(CoolSig, ClientBuf, sizeof(CoolSig), ClientSize, ShortPos, 0x100, 1);

	if(ShortPos == -1 || CoolPos == -1)
	{
		TextBoxCat(CurWnd, TEXT_DUMP, "Special names: Could not find the special names signatures %X %X\r\n", ShortPos, CoolPos);
		return;
	}

	/* change the JNB (0x73) or JGE (0x7D) to JMP (0xEB) */
	ClientBuf[ShortPos + 3] = 0xEB;
	TextBoxCat(CurWnd, TEXT_DUMP, "Special names: Patched with jmp @%X\r\n", ShortPos);

	/* change the TEST (0x80) to CMP (0x3B) */
	ClientBuf[CoolPos] = 0x3B;
	TextBoxCat(CurWnd, TEXT_DUMP, "Special names: Patched with cmp @%X\r\n", CoolPos);

	return;
}

void RemoveEncryption(void)
{
	/* for login encryption */
	unsigned char CryptSig[8] = { 0x81, 0xF9, 0x00, 0x00, 0x01, 0x00, 0x0F, 0x8F };
	unsigned char JNZSig[2] = { 0x0F, 0x85 };
	unsigned char JNESig[2] = { 0x0F, 0x84 };
	unsigned int CryptPos = -1, JNZPos = -1, JEPos = -1;
	/* 7.x clients: CMP ECX,10000 - JG SHORT XX - CMP DWORD PTR DS:[XXX],0 - JNZ */
	unsigned char CryptSig2[16] = { 0x81, 0xF9, 0x00, 0x00, 0x01, 0x00, 0x7F, 0xCC, 0x83, 0x3D, 0xCC, 0xCC, 0xCC, 0xCC, 0x00, 0x75 };
	unsigned int Crypt2Pos = -1;


	/* for game encryption */
	unsigned char BFGamecryptSig[5] = { 0x2C, 0x52, 0x00, 0x00, 0x76 }; /* CMP XXX, 522c - JBE */
	unsigned char CmpSig[4] = { 0x3B, 0xC3, 0x0F, 0x84 }; /* CMP EAX,EBX - JE */
	unsigned int BFGamecryptPos = -1, CmpPos = -1;
	/* RTD: make sure the JE 0x10 and JE 0x9X000000 stays like that, if not... I have to find a new way */
	unsigned char TFGamecryptSig[14] = { 0x8B, 0x8B, 0xCC, 0xCC, 0xCC, 0xCC, 0x81, 0xF9, 0x00, 0x01, 0x00, 0x00, 0x74, 0x10 }; /* MOV EBX, XXX - CMP ECX, 0x100 - JE 0x10 */
	/* 7.x clients: MOV EAX,[EBX+XXX] - SUB DWORD PTR SS:[ESP+XXX],1 - CMP EAX,0x100 - JE */
	unsigned char TFGamecryptSig2[20] = { 0x8B, 0x83, 0xCC, 0xCC, 0xCC, 0xCC, 0x83, 0xAC, 0x24, 0xCC, 0xCC, 0xCC, 0xCC, 0x01, 0x3D, 0x00, 0x01, 0x00, 0x00, 0x74 };
	unsigned char JELong[7] = { 0x0F, 0x84, 0xCC, 0x00, 0x00, 0x00, 0x55 }; /* JE XX000000 -  */
	unsigned int TFGamecryptPos = -1, GJEPos = -1;

	/* for game decryption */
	unsigned char DecryptSig[8] = { 0x4A, 0x83, 0xCA, 0xF0, 0x42, 0x8A, 0x94, 0x32 };
	unsigned char DectestSig[10] = { 0x85, 0xCC, 0x74, 0xCC, 0x33, 0xCC, 0x85, 0xCC, 0x7E, 0xCC };
	/* 7.x clients: TEST AL,AL instead of TEST EAX,EAX */
	unsigned char DectestSig2[10] = { 0x84, 0xCC, 0x74, 0xCC, 0x33, 0xCC, 0x85, 0xCC, 0x7E, 0xCC };
	unsigned int DecryptPos = -1, DectestPos = -1;

	/* LOGINENCRYPTION START */

	/*
	* magic x90 encryption signature: 81 f9 00 00 01 00 0f 8f
	* patching : find the first 0f 84 bellow and change it to 0f 85
	* or first 0f 85 and change it to 0f 84
	*
	* client 7.0.118.46
	* 00466D3E  |. 8D0C38         LEA ECX,DWORD PTR DS:[EAX+EDI]
	* 00466D41  |. 81F9 00000100  CMP ECX,10000
	* 00466D47  |. 7F 40          JG SHORT client.00466D89
	* 00466D49  |. 833D F083A000 >CMP DWORD PTR DS:[A083F0],0
	* 00466D50  |. 75 1B          JNZ SHORT client.00466D6D            ;  copy to the send buffer as is
	* 00466D52  |. 83BE B4000000 >CMP DWORD PTR DS:[ESI+B4],0          ;  game server? (twofish)
	* 00466D59  |. 75 12          JNZ SHORT client.00466D6D
	* 00466D5B  |. 8B5424 0C      MOV EDX,DWORD PTR SS:[ESP+C]
	* 00466D5F  |. 57             PUSH EDI
	* 00466D60  |. 52             PUSH EDX
	* 00466D61  |. 8BCE           MOV ECX,ESI
	* 00466D63  |. E8 48FFFFFF    CALL client.00466CB0                 ;  login encryption
	*
	* the JG is short now. patch the first JNZ (0x75) to JMP (0xEB)
	*/
	CryptPos = FleXSearch(CryptSig, ClientBuf, sizeof(CryptSig), ClientSize, 0, 0x100, 1);

	/* search for the first JNZ or JNE from crypt_addr */
	JNZPos = FleXSearch(JNZSig, ClientBuf, sizeof(JNZSig), ClientSize, CryptPos, 0x100, 1);
	JEPos = FleXSearch(JNESig, ClientBuf, sizeof(JNESig), ClientSize, CryptPos, 0x100, 1);

	if(CryptPos == -1)
		Crypt2Pos = FleXSearch(CryptSig2, ClientBuf, sizeof(CryptSig2), ClientSize, 0, 0xCC, 1);

	if(CryptPos == -1 && Crypt2Pos != -1)
	{
		ClientBuf[Crypt2Pos + sizeof(CryptSig2) - 1] = 0xEB;
		TextBoxCat(CurWnd, TEXT_DUMP, "LCrypt: Crypt sig @%X\r\n", Crypt2Pos);
		TextBoxCat(CurWnd, TEXT_DUMP, "LCrypt: Patched with jmp @%X\r\n", Crypt2Pos + sizeof(CryptSig2) - 1);
	}
	/* if this is an OSI UO client, shouldnt happen AT ALL */
	else if(CryptPos == -1)
		TextBoxCat(CurWnd, TEXT_DUMP, "LCrypt: could not find the sig... weird\r\n");
	else
	{
		TextBoxCat(CurWnd, TEXT_DUMP, "LCrypt: Crypt sig @%X\r\n", CryptPos);

		if(!JEPos || JEPos > JNZPos)
		{
			ClientBuf[JNZPos + 1] = 0x84;
			TextBoxCat(CurWnd, TEXT_DUMP, "LCrypt: Patched with je @%X\r\n", JNZPos);
		}
		else if(!JNZPos || JNZPos > JEPos)
		{
			ClientBuf[JEPos + 1] = 0x85;
			TextBoxCat(CurWnd, TEXT_DUMP, "LCrypt: Patched with jnz @%X\r\n", JEPos);
		}
	}

	/* LOGINENCRYPTION END */

	/* GAMEENCRYPTION START */

	/*
	* BLOWFISH
	* this is simply "lost" inside the "send" function, just above it
	* it looks like:
	* if(GameMode != LOGIN_SOCKET) ;game socket
	* {
	*     BlowfishEncrypt() ;starts with a (while > 0x522c)
	*     ;a whole bunch of other stuff
	*     TwofishEncrypt() ;if present
	*     send()
	* }
	* else ;login socket
	*     send() ;yay, a send that bypasses all the encryption crap
	*
	* find the beginning of the loop while(Obj->stream_pos + len > CRYPT_GAMETABLE_TRIGGER)
	* CRYPT_GAMETABLE_TRIGGER is 0x522c
	* find the first CMP EAX,EBX-JE above and patch it to CMP EAX,EAX
	*/
	BFGamecryptPos = FleXSearch(BFGamecryptSig, ClientBuf, sizeof(BFGamecryptSig), ClientSize, 0, 0x100, 1);
	CmpPos = FleXSearch(CmpSig, ClientBuf, sizeof(CmpSig), ClientSize, BFGamecryptPos - 0x20, 0x100, 1);

	if(BFGamecryptPos == -1 || CmpPos == -1)
		TextBoxCat(CurWnd, TEXT_DUMP, "GCrypt: Could not find blowfish encryption %X %X\r\n", BFGamecryptPos, CmpPos);
	else
	{
		ClientBuf[CmpPos + 1] = 0xC0;
		TextBoxCat(CurWnd, TEXT_DUMP, "GCrypt: Found blowfish @%X\r\n", BFGamecryptPos);
		TextBoxCat(CurWnd, TEXT_DUMP, "GCrypt: Patched with cmp @%X\r\n", CmpPos);
	}

	
	/* 
	* TWOFISH
	* patch the encryption function to skip encryption
	* the function is always called before send
	*
	* find the beginning of the loop and the first JE above it
	* patch the JE (0x84) to JNE (0x85)
	*
	* client 7.0.118.46
	* 0048CB64  |. 83BC24 1001000>CMP DWORD PTR SS:[ESP+110],0          ;  len == 0?
	* 0048CB6C  |. 8B9424 0C01000>MOV EDX,DWORD PTR SS:[ESP+10C]
	* 0048CB73  |. 53             PUSH EBX
	* 0048CB74  |. 8BD9           MOV EBX,ECX
	* 0048CB76  |. 895424 04      MOV DWORD PTR SS:[ESP+4],EDX
	* 0048CB7A  |. 0F84 9A000000  JE client.0048CC1A
	* 0048CB80  |. 55             PUSH EBP
	* ...
	* 0048CB90  |> 8B83 4C120000  /MOV EAX,DWORD PTR DS:[EBX+124C]
	* 0048CB96  |. 83AC24 2001000>|SUB DWORD PTR SS:[ESP+120],1
	* 0048CB9E  |. 3D 00010000    |CMP EAX,100
	* 0048CBA3  |. 74 12          |JE SHORT client.0048CBB7
	*
	* the loop uses EAX and has a SUB in the middle, the JE above it is the same
	*/

	TFGamecryptPos = FleXSearch(TFGamecryptSig, ClientBuf, sizeof(TFGamecryptSig), ClientSize, 0, 0xCC, 1);
	GJEPos = FleXSearch(JELong, ClientBuf, sizeof(JEPos), ClientSize, TFGamecryptPos - 0x20, 0xCC, 1);

	if(TFGamecryptPos == -1)
	{
		/* 7.x clients, the JE must be within the 32 bytes above the loop */
		TFGamecryptPos = FleXSearch(TFGamecryptSig2, ClientBuf, sizeof(TFGamecryptSig2), ClientSize, 0, 0xCC, 1);
		GJEPos = FleXSearch(JELong, ClientBuf, sizeof(JELong), ClientSize, TFGamecryptPos - 0x20, 0xCC, 1);
		if(GJEPos >= TFGamecryptPos)
			GJEPos = -1;
	}

    if(TFGamecryptPos == -1 || GJEPos == -1)
		TextBoxCat(CurWnd, TEXT_DUMP, "GCrypt: Could not find twofish encryption %X %X\r\n", TFGamecryptPos, GJEPos);
	else
	{
		ClientBuf[GJEPos + 1] = 0x85;
		TextBoxCat(CurWnd, TEXT_DUMP, "GCrypt: Found twofish @%X\r\n", TFGamecryptPos);
		TextBoxCat(CurWnd, TEXT_DUMP, "GCrypt: Patched with jnz @%X\r\n", GJEPos);
	}
	

	/* GAMEENCRYPTION END */

	/* GAMEDECRYPTION START (now for the easy part) */

	/*
	* search for 4A 83 CA F0 42 8A 94 32
	* and above it, 85 xx 74 xx 33 xx 85 xx 7E xx
	* the first TEST (85 xx) must be cracked to CMP EAX, EAX (3B C0)
	* if I want to do it like LB does in UORice, I'd crack
	* the first CMP xx JMP xx (85 xx 74 xx) to CMP EAX, EAX (3B C0)
	* which is bellow the one I crack
	*/

	/*
	* client 7.0.118.46
	* 00468416  |. E8 55EAFFFF    CALL client.00466E70                 ;  is the decryption on?
	* 0046841B  |. 84C0           TEST AL,AL
	* 0046841D  |. 74 49          JE SHORT client.00468468
	* 0046841F  |. 33C0           XOR EAX,EAX
	* 00468421  |. 85FF           TEST EDI,EDI
	* 00468423  |. 7E 43          JLE SHORT client.00468468
	* ...
	* 0046844E  |. 4A             DEC EDX
	* 0046844F  |. 83CA F0        OR EDX,FFFFFFF0
	* 00468452  |. 42             INC EDX
	* 00468453  |> 8A9432 44130A0>MOV DL,BYTE PTR DS:[EDX+ESI+A1344]
	*
	* the TEST is 8 bit now, crack it to CMP AL,AL (84 -> 3A)
	* the TEST must be above the sig, otherwise it's something else
	*/

	/* find 4A 83 CA F0 42 8A 94 32 */
	/* find the TEST above it (not the one right above that is) */
	DecryptPos = FleXSearch(DecryptSig, ClientBuf, sizeof(DecryptSig), ClientSize, 0, 0x100, 1);
	DectestPos = FleXSearch(DectestSig, ClientBuf, sizeof(DectestSig), ClientSize, DecryptPos - 0x100, 0xCC, 1);
	if(DectestPos >= DecryptPos)
		DectestPos = FleXSearch(DectestSig2, ClientBuf, sizeof(DectestSig2), ClientSize, DecryptPos - 0x100, 0xCC, 1);
	if(DectestPos >= DecryptPos)
		DectestPos = -1;

	if(DecryptPos == -1 || DectestPos == -1)
		TextBoxCat(CurWnd, TEXT_DUMP, "GDecrypt: Could not find MD5 decryption %X %X \r\n", DecryptPos, DectestPos);
	else
	{
		/* TEST r/m32,r32 -> CMP r32,r/m32 or TEST r/m8,r8 -> CMP r8,r/m8 */
		if(ClientBuf[DectestPos] == 0x84)
			ClientBuf[DectestPos] = 0x3A;
		else
			ClientBuf[DectestPos] = 0x3B;
		TextBoxCat(CurWnd, TEXT_DUMP, "GCrypt: Found MD5 @%X\r\n", DecryptPos);
		TextBoxCat(CurWnd, TEXT_DUMP, "GDecrypt: Sig @%X patched with cmp @%X\r\n", DecryptPos, DectestPos);
	}

	/* GAMEDECRYPTION END */

	return;
}

void RemoveMacroDelay(void)
{
	/*
	* 4.0.4b
	* 004DF3BD  |. 3BC3           CMP EAX,EBX
	* 004DF3BF  |. 57             PUSH EDI
	* 004DF3C0  |. 74 18          JE SHORT client40.004DF3DA
	* 004DF3C2  |. 68 6C635900    PUSH client40.0059636C                   ;  ASCII "You must wait to perform another action."
	*
	* find the reference to "You must sait to perform another action."
	* and patch the JE (0x74) to JMP (0xEB)
	*/

	unsigned int WaitRef = -1;

	WaitRef = GetTextRef(ClientBuf, ClientSize, ImageBase, "You must wait to perform another action.", TRUE, 1);

	if((int)WaitRef < 0)
	{
		TextBoxCat(CurWnd, TEXT_DUMP, "Macro delay: Could not find \"You must wait to perform another action.\" return was %d\r\n", WaitRef);
		return;
	}

	if(ClientBuf[WaitRef - 2] != 0x74)
	{
		TextBoxCat(CurWnd, TEXT_DUMP, "Macro delay: The opcode (%c) before the ref @%X is not a JE!\r\n", ClientBuf[WaitRef - 2], WaitRef);
		return;
	}

	ClientBuf[WaitRef - 2] = 0xEB;
	TextBoxCat(CurWnd, TEXT_DUMP, "Macro delay: Patched with jmp @%X\r\n", WaitRef - 2);
	return;
}

void RemoveStaminaCheck(void)
{
	/*
	* client 4.0.4b
	* 004EA7A6  |. 85C0           |TEST EAX,EAX
	* 004EA7A8  |. 74 09          |JE SHORT client40.004EA7B3
	* 004EA7AA  |. 7E 36          |JLE SHORT client40.004EA7E2
	* 004EA7AC  |. 83F8 02        |CMP EAX,2
	* 004EA7AF  |. 7F 31          |JG SHORT client40.004EA7E2
	* 004EA7B1  |. EB 5D          |JMP SHORT client40.004EA810
	* 004EA7B3  |> 8B91 FC010000  |MOV EDX,DWORD PTR DS:[ECX+1FC]
	* 004EA7B9  |. 8B81 00020000  |MOV EAX,DWORD PTR DS:[ECX+200]
	* 004EA7BF  |. 3BD0           |CMP EDX,EAX
	* 004EA7C1  |. 74 4D          |JE SHORT client40.004EA810
	*
	* if I remember it right, at the end EDX has the current stamina, EAX the full
	* it checks if they are equal and if they are it allows you to walk
	* through other players
	* patch the CMP EDX,EAX (3B D0) to CMP EAX,EAX (3B C0)
	* hooligan mode :P
	* thanks to Bloodbob (bloob) for this one
	*
	* client 7.0.118.46
	* 005A74EE  |. 85C9           |TEST ECX,ECX
	* 005A74F0  |. 74 09          |JE SHORT client.005A74FB
	* 005A74F2  |. 7E 34          |JLE SHORT client.005A7528
	* 005A74F4  |. 83F9 02        |CMP ECX,2
	* 005A74F7  |. 7F 2F          |JG SHORT client.005A7528
	* 005A74F9  |. EB 63          |JMP SHORT client.005A755E
	* 005A74FB  |> 8B82 78020000  |MOV EAX,DWORD PTR DS:[EDX+278]
	* 005A7501  |. 3B82 7C020000  |CMP EAX,DWORD PTR DS:[EDX+27C]
	* 005A7507  |. 74 55          |JE SHORT client.005A755E
	*
	* same check with ECX, but the CMP reads the full stamina from memory
	* so it can't become CMP EAX,EAX. patch the JE (0x74) to JMP (0xEB) instead
	*/

	unsigned char StaminacheckSig[11] = { 0x74, 0xCC, 0x7E, 0xCC, 0x83, 0xF8, 0x02, 0x7F, 0xCC, 0xEB, 0xCC };
	unsigned char CmpSig[3] = { 0x3B, 0xCC, 0x74 };
	/* 7.x clients: the check above with any register, MOV reg,[reg+XXX] - CMP reg,[reg+XXX] - JE */
	unsigned char StaminacheckSig2[24] = { 0x74, 0xCC, 0x7E, 0xCC, 0x83, 0xCC, 0x02, 0x7F, 0xCC, 0xEB, 0xCC, 0x8B, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0x3B, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0x74 };
	unsigned int StaminacheckPos = -1, CmpPos = -1;

	StaminacheckPos = FleXSearch(StaminacheckSig, ClientBuf, sizeof(StaminacheckSig), ClientSize, 0, 0xCC, 1);

	if(StaminacheckPos == -1)
	{
		StaminacheckPos = FleXSearch(StaminacheckSig2, ClientBuf, sizeof(StaminacheckSig2), ClientSize, 0, 0xCC, 1);
		if(StaminacheckPos != -1)
		{
			ClientBuf[StaminacheckPos + sizeof(StaminacheckSig2) - 1] = 0xEB;
			TextBoxCat(CurWnd, TEXT_DUMP, "Hooligan: patched with jmp @%X\r\n", StaminacheckPos + sizeof(StaminacheckSig2) - 1);
			return;
		}
	}

	CmpPos = FleXSearch(CmpSig, ClientBuf, sizeof(CmpSig), ClientSize, StaminacheckPos, 0xCC, 1);

    if(StaminacheckPos == -1 || CmpPos == -1)
	{
		TextBoxCat(CurWnd, TEXT_DUMP, "Hooligan: Could not find the stamina check %X %X\r\n", StaminacheckPos, CmpPos);
		return;
	}

	ClientBuf[CmpPos + 1] = 0xC0;
	TextBoxCat(CurWnd, TEXT_DUMP, "Hooligan: patched with cmp @%X\r\n", CmpPos);


	return;
}

void AddNecroLogo(void)
{
	/* find "UO Version %s" and write "Necr0Potenc3" on top of it ;) */

	unsigned int VersionPos = -1;

	VersionPos = FleXSearch("UO Version %s", ClientBuf, (DWORD)strlen("UO Version %s"), ClientSize, 0, 0xCC, 1); 

	if(VersionPos == -1)
		TextBoxCat(CurWnd, TEXT_DUMP, "Logo: Couldn't find \"UO Version %%s\"... the logo can't be added :(\r\n");
	else
		strcpy(ClientBuf + VersionPos, "Necr0Potenc3");

	return;
}
