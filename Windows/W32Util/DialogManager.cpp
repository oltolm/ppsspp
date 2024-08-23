#include "Common/CommonWindows.h"
#include "Windows/Debugger/Debugger_Disasm.h"
#include "Windows/Debugger/Debugger_MemoryDlg.h"
#include "Windows/Debugger/Debugger_VFPUDlg.h"
#include "Windows/GEDebugger/GEDebugger.h"
#include <memory>
#include <vector>
#include <algorithm>
#include "Windows/W32Util/DialogManager.h"


Dialog::Dialog(LPCSTR res, HINSTANCE _hInstance, HWND _hParent) 
{
	m_hInstance = _hInstance;
	m_hParent = _hParent;
	m_hResource = res;
	m_bValid = true;
	Create();
}

Dialog::~Dialog()
{
	m_bValid = false;
	Destroy();
}

void Dialog::Create()
{
	m_hDlg = CreateDialogParam(m_hInstance, (LPCWSTR)m_hResource, m_hParent, DlgProcStatic, (LPARAM)this);
	SetWindowLongPtr(m_hDlg, GWLP_USERDATA, (LONG_PTR)this);
}

void Dialog::Destroy()
{
	DestroyWindow(m_hDlg);
}

void Dialog::Show(bool _bShow, bool includeToTop)
{
	if (_bShow && includeToTop)
		m_bShowState = SW_SHOWNORMAL;
	else if (_bShow)
		m_bShowState = SW_SHOWNOACTIVATE;
	else
		m_bShowState = SW_HIDE;
	ShowWindow(m_hDlg, m_bShowState);
	if (_bShow && includeToTop)
		BringWindowToTop(m_hDlg);
}

INT_PTR Dialog::DlgProcStatic(HWND hdlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	Dialog *dis = (Dialog*)GetWindowLongPtr(hdlg, GWLP_USERDATA);
	if (dis && dis->m_bValid)
		return dis->DlgProc(message,wParam,lParam);
	else
	{
		return 0;
		/*
		if (message == WM_INITDIALOG)
		{
			SetWindowLongPtr(hdlg, GWLP_USERDATA, (LONG_PTR)lParam);
			return ((Dialog*)lParam)->DlgProc(message,wParam,lParam);
		}
		else
		{
			return 0;
		}*/
	}
}
