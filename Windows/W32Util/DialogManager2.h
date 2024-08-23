#pragma once

#include "Core/MIPS/MIPSDebugInterface.h"
#include "DialogManager.h"
#include "Windows/Debugger/Debugger_Disasm.h"
#include "Windows/Debugger/Debugger_MemoryDlg.h"
#include "Windows/Debugger/Debugger_VFPUDlg.h"
#include "Windows/GEDebugger/GEDebugger.h"
#include <memory>

class DialogManager
{
public:
	static void AddDlg(std::shared_ptr<Dialog> dialog);
	static std::shared_ptr<StepCountDlg> NewStepCountDlg(HINSTANCE _hInstance, HWND _hParent);
	static std::shared_ptr<CDisasm> NewCDisasm(HINSTANCE _hInstance, HWND _hParent, MIPSDebugInterface *cpu);
	static std::shared_ptr<CGEDebugger> NewCGEDebugger(HINSTANCE _hInstance, HWND _hParent);
	static std::shared_ptr<CMemoryDlg> NewCMemoryDlg(HINSTANCE _hInstance, HWND _hParent, MIPSDebugInterface *_cpu);
	static std::shared_ptr<CVFPUDlg> NewCVFPUDlg(HINSTANCE _hInstance, HWND _hParent, MIPSDebugInterface *cpu_);
	static bool IsDialogMessage(LPMSG message);
	static void EnableAll(BOOL enable);
	static void DestroyAll();
	static void UpdateAll();
};
