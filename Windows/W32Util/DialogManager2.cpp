#include "DialogManager2.h"
#include "Core/MIPS/MIPSDebugInterface.h"

std::vector <std::shared_ptr<Dialog>> dialogs;


std::shared_ptr<CDisasm> DialogManager::NewCDisasm(HINSTANCE _hInstance, HWND _hParent, MIPSDebugInterface *cpu)
{
    auto dialog = std::make_shared<CDisasm>(_hInstance, _hParent, cpu);
	dialogs.push_back(dialog);
	return dialog;
}

std::shared_ptr<StepCountDlg> DialogManager::NewStepCountDlg(HINSTANCE _hInstance, HWND _hParent)
{
	auto dialog = std::make_shared<StepCountDlg>(_hInstance, _hParent);
	dialogs.push_back(dialog);
	return dialog;
}

std::shared_ptr<CGEDebugger> DialogManager::NewCGEDebugger(HINSTANCE _hInstance, HWND _hParent)
{
	auto dialog = std::make_shared<CGEDebugger>(_hInstance, _hParent);
	dialogs.push_back(dialog);
	return dialog;
}

std::shared_ptr<CMemoryDlg> DialogManager::NewCMemoryDlg(HINSTANCE _hInstance, HWND _hParent, MIPSDebugInterface *_cpu)
{
	auto dialog = std::make_shared<CMemoryDlg>(_hInstance, _hParent, _cpu);
	dialogs.push_back(dialog);
	return dialog;
}

std::shared_ptr<CVFPUDlg> DialogManager::NewCVFPUDlg(HINSTANCE _hInstance, HWND _hParent, MIPSDebugInterface *cpu_)
{
	auto dialog = std::make_shared<CVFPUDlg>(_hInstance, _hParent, cpu_);
	dialogs.push_back(dialog);
	return dialog;
}

bool DialogManager::IsDialogMessage(LPMSG message)
{
	for (auto& dialog : dialogs) {
		if (::IsDialogMessage(dialog->GetDlgHandle(), message))
			return true;
	}
	return false;
}


void DialogManager::EnableAll(BOOL enable)
{
	for (auto& dialog : dialogs)
		EnableWindow(dialog->GetDlgHandle(),enable); 
}

void DialogManager::UpdateAll()
{
	for (auto& dialog : dialogs)
		dialog->Update();
}

void DialogManager::DestroyAll()
{
	dialogs.clear();
}
