// progressdlg.cpp : implementation of the CProgressDlg class
//
/////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "resource.h"
#include "versionno.h"
#include "progressdlg.h"
#include "settings.h"
#include "ComBSTR2.h"
#include "MainDlg.h"
#include <wininet.h>
#include <io.h>
#include <fcntl.h>
#include <sys/stat.h>

#ifdef _APPUPDLIB
#include "\WeOnlyDo\wodAppUpdate\Code\Component\resource.h"
#endif

extern CSettings _Settings;
extern CMainDlg _MainDlg;
void ShowNiceByteCount(int value, char *buff);

CProgressDlg::CProgressDlg()
{
	m_SkinBuffer = NULL;
	m_Value = 0;
	m_Total = 0;
//	m_Brush = CreateSolidBrush(RGB(0,128,255));
//	m_Back = GetSysColorBrush(COLOR_3DFACE);
	m_Canceled = FALSE;
}

CProgressDlg::~CProgressDlg()
{
//	DeleteObject(m_Brush);
}

BOOL CProgressDlg::PreTranslateMessage(MSG* pMsg)
{
	_MainDlg.CheckIfAntiInactivityMessage(pMsg->message);
	return CWindow::IsDialogMessage(pMsg);
}

BOOL CProgressDlg::OnIdle()
{
	return FALSE;
}

void CProgressDlg::DoEvents(void)
{
	MSG m;
	while (PeekMessage(&m, m_hWnd, 0, 0, PM_REMOVE))
	{
		TranslateMessage(&m);
		DispatchMessage(&m);
	}
}

LRESULT CProgressDlg::OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	HICON hi = LoadIcon(_Module.GetModuleInstance(), MAKEINTRESOURCE(IDR_PROGRESSWINICON));
	SetIcon(hi);

	SetWindowText(_Settings.Translate("Progress"));
	SetDlgItemText(IDC_DOWNLOADINGFILE, _Settings.Translate("Downloading"));
	SetDlgItemText(IDCANCEL, _Settings.Translate("&Cancel"));
	return TRUE;
}

LRESULT CProgressDlg::OnCloseCmd(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	m_Canceled = TRUE;
//	EndDialog(wID);
	return 0;
}

BOOL CProgressDlg::InitDownloadSkin(Buffer *data)
{
	m_SkinBuffer = data;
	DWORD id = 0;
	CreateThread(NULL, 0, DownloadSkinThreadProc, this, 0, &id);

	return TRUE;
}

extern char m_SkinThreadBuffer[1024];
DWORD WINAPI CProgressDlg::DownloadSkinThreadProc(LPVOID lpParam)
{
	CProgressDlg *me = (CProgressDlg *)lpParam;

	// get filename
	char *a = NULL;
	unsigned int len = 0;
	

	HWND ownerhwnd = (HWND)me->m_SkinBuffer->GetInt();

	if (::IsWindow(ownerhwnd))
	{
		a = me->m_SkinBuffer->GetString(&len);
		if (a)
		{
		
			char *c = NULL;
			c = me->m_SkinBuffer->GetString(&len);
			if (c)
			{

				me->Create(ownerhwnd);
				me->CenterWindow();
				me->ShowWindow(SW_SHOW);

				char fnamebuff[1024];
				sprintf(fnamebuff, "%s.smf", a);
				::ShowWindow(me->GetDlgItem(IDC_DOWNLOADINGFILE2), SW_HIDE);
				me->SetDlgItemText(IDC_DOWNLOADINGFILE2, fnamebuff);
				me->m_Value = 0;
				me->m_Total = 10000;

				//CComBSTR f1 = "http://wippien.com/skins/";
				CComBSTR f1 = "/Skins/";
				f1 += a;
				CComBSTR f2 = f1;
				f1 += ".smf";
				f2 += ".png";

				CComBSTR2 fa1 = f1;
				CComBSTR2 fa2 = f2;

				Buffer file1, file2;
				::SendMessage(me->GetDlgItem(IDC_PROGRESS1), PBM_SETPOS, 0,0);
				me->DownloadFile(FALSE, fa1.ToString(), &file1, TRUE);
				if (!me->m_Canceled && file1.Len())
				{
					sprintf(fnamebuff, "%s.png", a);
					::ShowWindow(me->GetDlgItem(IDC_DOWNLOADINGFILE2), SW_HIDE);
					me->SetDlgItemText(IDC_DOWNLOADINGFILE, fnamebuff);
//					res = ::SendMessage(me->GetDlgItem(IDC_PROGCTRL1), PBM_SETPOS, 0,0);
					me->m_Total = 10000;
					me->m_Value = 0;
//					me->Invalidate();

					
					::SendMessage(me->GetDlgItem(IDC_PROGRESS1), PBM_SETPOS, 0,0);
					me->DownloadFile(FALSE, fa2.ToString(), &file2, TRUE);
					if (!me->m_Canceled && file2.Len())
					{
						// save file1...
						CComBSTR d1 = c;
						d1 += "skin\\";
						d1 += a;
						d1 += ".smf";
						CComBSTR2 d2 = d1;
						int handle = _open(d2.ToString(), O_BINARY | O_WRONLY | O_CREAT | O_TRUNC, S_IREAD | S_IWRITE);
						if (handle != (-1))
						{
							_write(handle, file1.Ptr(), file1.Len());
							_close(handle);
						
							// save file2...
							CComBSTR e1 = c;
							e1 += "skin\\";
							e1 += a;
							e1 += ".png";
							CComBSTR2 e2 = e1;
							handle = _open(e2.ToString(), O_BINARY | O_WRONLY | O_CREAT | O_TRUNC, S_IREAD | S_IWRITE);
							if (handle != (-1))
							{
								_write(handle, file2.Ptr(), file2.Len());
								_close(handle);

								// set skin!
								if (::IsWindow(ownerhwnd))
								{
									// refresh it
									memset(m_SkinThreadBuffer, 0, sizeof(m_SkinThreadBuffer));
									memcpy(m_SkinThreadBuffer, a, strlen(a));
									::PostMessage(ownerhwnd, WM_REFRESH, 0,0);
								}

							}
						}
					}
					me->DestroyWindow();
				}
				free(c);
			}
			free(a);
		}
	}
	delete me;
	return 0;
}

BOOL CProgressDlg::DownloadFile(BOOL issecure, char *URL, Buffer *data, BOOL updateprogress)
{
	HINTERNET Initialize,Connection,File;
	DWORD dwBytes;
	char buff[1024];

	/*initialize the wininet library*/
	Initialize = InternetOpen("Wippien",INTERNET_OPEN_TYPE_DIRECT,NULL,NULL,0);
	/*connect to the server*/
	if (updateprogress)
	{
		::SetWindowText(GetDlgItem(IDC_DOWNLOADINGFILE), _Settings.Translate("Connecting"));
		DoEvents();
	}
//	issecure = FALSE;
//	Connection = InternetConnect(Initialize,"linux.weonlydo.com",issecure?INTERNET_DEFAULT_HTTPS_PORT:INTERNET_DEFAULT_HTTP_PORT,NULL,NULL,INTERNET_SERVICE_HTTP,0,0);
	Connection = InternetConnect(Initialize,"www.wippien.com",issecure?INTERNET_DEFAULT_HTTPS_PORT:INTERNET_DEFAULT_HTTP_PORT,NULL,NULL,INTERNET_SERVICE_HTTP,0,0);
	/*open up an HTTP request*/
	if (updateprogress)
	{
		::SetWindowText(GetDlgItem(IDC_DOWNLOADINGFILE), _Settings.Translate("Requesting"));
		DoEvents();
	}
	File = HttpOpenRequest(Connection,NULL,URL,NULL,NULL,NULL,INTERNET_FLAG_PRAGMA_NOCACHE | INTERNET_FLAG_NO_CACHE_WRITE | (issecure?INTERNET_FLAG_SECURE:0),0);
	
	int total = 0;
	char bf1[128], bf2[128];
	if(HttpSendRequest(File,NULL,0,NULL,0))
	{
		if (updateprogress)
		{
			::SetWindowText(GetDlgItem(IDC_DOWNLOADINGFILE), _Settings.Translate("Downloading"));
			// setup name
			int k = strlen(URL);
			while (k>1 && URL[k-1]!='/')
				k--;
			int j = strlen(URL);
			if (j>3)
			{
				if (!strcmp(URL+j-3, ".gz"))
					URL[j-3]=0;
			}
			char *a1 = strchr(&URL[k], '?');
			if (a1)
			{
				*a1 = 0;
				if (!strncmp(&URL[k], "update", 6))
					a1 = _Settings.Translate("Configuration file");
			}
			else
				a1 = &URL[k];
			::SetWindowText(GetDlgItem(IDC_DOWNLOADINGFILE2), a1);
			::ShowWindow(GetDlgItem(IDC_DOWNLOADINGFILE2), SW_SHOW);
			DoEvents();
		}

		ShowNiceByteCount(m_Total, bf2);
		while(!m_Canceled && InternetReadFile(File,buff,1024,&dwBytes))
		{
			if (dwBytes>0)
			{
				data->Append(buff, dwBytes);
				if (updateprogress)
				{
					total += dwBytes;
					if (m_Total)
					{
						m_Value = ((total*100)/m_Total);
						ShowNiceByteCount(total, bf1);
						sprintf(buff, "%s / %s", bf1, bf2);
						::SetWindowText(GetDlgItem(IDC_PROGRESSNUMBER), buff);
					}
					else
					{
						m_Value = total;
						ShowNiceByteCount(total, bf1);
						sprintf(buff, "%s", bf1);
						::SetWindowText(GetDlgItem(IDC_PROGRESSNUMBER), buff);
					}
					::SendMessage(GetDlgItem(IDC_PROGRESS1), PBM_SETPOS, m_Value,0);
					DoEvents();
				}
			}
			else
				break;
		}
	}
	/*close file , terminate server connection and
	deinitialize the wininet library*/
	InternetCloseHandle(File);
	InternetCloseHandle(Connection);
	InternetCloseHandle(Initialize);

	if (m_Canceled)
		data->Clear();
	return 0;
	
}
