#pragma once

#include <windows.h>

class Dialog
{
public:
	Dialog(LPCSTR res, HINSTANCE _hInstance, HWND _hParent);
	virtual ~Dialog();

	virtual void Show(bool _bShow, bool includeToTop = true);
	virtual void Update() {}

	HWND GetDlgHandle() {
		return m_hDlg;
	}
protected:
	virtual void Create();
	void Destroy();

	HINSTANCE m_hInstance;
	HWND m_hParent;
	HWND m_hDlg;
	LPCSTR m_hResource;
	bool m_bValid;
	UINT m_bShowState = SW_HIDE;

	virtual BOOL DlgProc(UINT message, WPARAM wParam, LPARAM lParam) = 0;
	static INT_PTR CALLBACK DlgProcStatic(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
};
