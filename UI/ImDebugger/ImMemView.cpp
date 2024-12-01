#include <cctype>
#include <cmath>
#include <iomanip>
#include <sstream>

#include "ext/imgui/imgui.h"
#include "ext/imgui/imgui_internal.h"
#include "ext/imgui/imgui_impl_thin3d.h"

#include "ext/xxhash.h"
#include "Common/StringUtils.h"
#include "Core/Config.h"
#include "Core/MemMap.h"
#include "Core/Reporting.h"
#include "Core/RetroAchievements.h"
#include "Common/System/Display.h"

#include "UI/ImDebugger/ImDebugger.h"
#include "UI/ImDebugger/ImMemView.h"
// #include "DumpMemoryWindow.h"

ImMemView::ImMemView() {
	const float fontScale = 1.0f / g_display.dpi_scale_real_y;
	charWidth_ = g_Config.iFontWidth * fontScale;
	rowHeight_ = g_Config.iFontHeight * fontScale;
	offsetPositionY_ = offsetLine * rowHeight_;

	windowStart_ = curAddress_;
	selectRangeStart_ = curAddress_;
	selectRangeEnd_ = curAddress_ + 1;
	lastSelectReset_ = curAddress_;

	addressStartX_ = charWidth_;
	hexStartX_ = addressStartX_ + 9 * charWidth_;
	asciiStartX_ = hexStartX_ + (rowSize_ * 3 + 1) * charWidth_;
}

ImMemView::~ImMemView() {}

/*
LRESULT CALLBACK ImMemView::wndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
		if (GET_WHEEL_DELTA_WPARAM(wParam) > 0) {
			ccp->ScrollWindow(-3, GotoModeFromModifiers(false));
		} else if (GET_WHEEL_DELTA_WPARAM(wParam) < 0) {
			ccp->ScrollWindow(3, GotoModeFromModifiers(false));
		}
	return DefWindowProc(hwnd, msg, wParam, lParam);
}
*/

void ImMemView::Draw(ImDrawList *drawList) {
	auto memLock = Memory::Lock();
	if (!debugger_->isAlive()) {
		return;
	}

	ImGui_PushFixedFont();

	rowHeight_ = ImGui::GetTextLineHeightWithSpacing();
	charWidth_ = ImGui::CalcTextSize("W", nullptr, false, -1.0f).x;

	const ImVec2 canvas_p0 = ImGui::GetCursorScreenPos();      // ImDrawList API uses screen coordinates!
	const ImVec2 canvas_sz = ImGui::GetContentRegionAvail();   // Resize canvas to what's available
	const ImVec2 canvas_p1 = ImVec2(canvas_p0.x + canvas_sz.x, canvas_p0.y + canvas_sz.y);

	// This will catch our interactions
	bool pressed = ImGui::InvisibleButton("canvas", canvas_sz, ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);
	const bool is_hovered = ImGui::IsItemHovered(); // Hovered
	const bool is_active = ImGui::IsItemActive();   // Held

	if (pressed) {
		// INFO_LOG(Log::System, "Pressed");
	}
	ImGui::SetItemKeyOwner(ImGuiKey_MouseWheelY);

	visibleRows_ = canvas_sz.y / rowHeight_;

	if (displayOffsetScale_) {
		// visibleRows_ is calculated based on the size of the control, but X rows have already been used for the offsets and are no longer usable
		visibleRows_ -= offsetSpace;
	}

	Bounds bounds;
	bounds.x = canvas_p0.x;
	bounds.y = canvas_p0.y;
	bounds.w = canvas_p1.x - canvas_p0.x;
	bounds.h = canvas_p1.y - canvas_p0.y;

	drawList->PushClipRect(canvas_p0, canvas_p1, true);
	drawList->AddRectFilled(canvas_p0, canvas_p1, IM_COL32(25, 25, 25, 255));

	if (displayOffsetScale_)
		drawOffsetScale(drawList);

	std::vector<MemBlockInfo> memRangeInfo = FindMemInfoByFlag(highlightFlags_, windowStart_, (visibleRows_ + 1) * rowSize_);

	_assert_msg_(((windowStart_ | rowSize_) & 3) == 0, "readMemory() can't handle unaligned reads");

	// draw one extra row that may be partially visible
	for (int i = 0; i < visibleRows_ + 1; i++) {
		int rowY = rowHeight_ * i;
		// Skip the first X rows to make space for the offsets.
		if (displayOffsetScale_)
			rowY += rowHeight_ * offsetSpace;

		char temp[32];
		uint32_t address = windowStart_ + i * rowSize_;
		snprintf(temp, sizeof(temp), "%08X", address);

		drawList->AddText(ImVec2(canvas_p0.x + addressStartX_, canvas_p0.y + rowY), IM_COL32(0, 0, 0x60, 0xFF), temp);

		union {
			uint32_t words[4];
			uint8_t bytes[16];
		} memory;
		int valid = debugger_ != nullptr && debugger_->isAlive() ? Memory::ValidSize(address, 16) / 4 : 0;
		for (int i = 0; i < valid; ++i) {
			memory.words[i] = debugger_->readMemory(address + i * 4);
		}

		for (int j = 0; j < rowSize_; j++) {
			const uint32_t byteAddress = (address + j) & ~0xC0000000;
			std::string tag;
			bool tagContinues = false;
			for (auto info : memRangeInfo) {
				if (info.start <= byteAddress && info.start + info.size > byteAddress) {
					tag = info.tag;
					tagContinues = byteAddress + 1 < info.start + info.size;
				}
			}

			int hexX = hexStartX_ + j * 3 * charWidth_;
			int hexLen = 2;
			int asciiX = asciiStartX_ + j * (charWidth_ + 2);

			char c;
			if (valid) {
				snprintf(temp, sizeof(temp), "%02X ", memory.bytes[j]);
				c = (char)memory.bytes[j];
				if (memory.bytes[j] < 32 || memory.bytes[j] >= 128)
					c = '.';
			} else {
				truncate_cpy(temp, "??");
				c = '.';
			}

			ImColor standardBG = 0xFF000000;
			ImColor continueBGCol = 0xFF000000;
			ImColor hexBGCol = 0xFF000000;
			ImColor hexTextCol = 0xFFFFFFFF;
			ImColor asciiBGCol = 0xFF000000;
			ImColor asciiTextCol = 0xFFFFFFFF;
			int underline = -1;

			if (address + j >= selectRangeStart_ && address + j < selectRangeEnd_ && !searching_) {
				if (asciiSelected_) {
					hexBGCol = ImColor(0xFFC0C0C0);
					hexTextCol = 0x000000;
					asciiBGCol = hasFocus_ ? 0xFF9933 : 0xC0C0C0;
					asciiTextCol = hasFocus_ ? 0xFFFFFF : 0x000000;
				} else {
					hexBGCol = hasFocus_ ? 0xFF9933 : 0xC0C0C0;
					hexTextCol = hasFocus_ ? 0xFFFFFF : 0x000000;
					asciiBGCol = 0xC0C0C0;
					asciiTextCol = 0x000000;
					if (address + j == curAddress_)
						underline = selectedNibble_;
				}
				if (!tag.empty() && tagContinues) {
					continueBGCol = pickTagColor(tag);
				}
			} else if (!tag.empty()) {
				hexBGCol = pickTagColor(tag);
				continueBGCol = hexBGCol;
				asciiBGCol = pickTagColor(tag);
				hexLen = tagContinues ? 3 : 2;
			}

			ImColor fg = hexTextCol;
			ImColor bg = hexBGCol;
			if (underline >= 0) {
				// SelectObject(hdc, underline == 0 ? (HGDIOBJ)underlineFont : (HGDIOBJ)font);
				drawList->AddText(ImVec2(canvas_p0.x + hexX, canvas_p0.y + rowY), fg, &temp[0], &temp[1]);
				// SelectObject(hdc, underline == 0 ? (HGDIOBJ)font : (HGDIOBJ)underlineFont);
				drawList->AddText(ImVec2(canvas_p0.x + hexX + charWidth_, canvas_p0.y + rowY), fg, &temp[1]);

				// If the tag keeps going, draw the BG too.
				if (continueBGCol != standardBG) {
					// setTextColors(0x000000, continueBGCol);
					// TextOutA(hdc, hexX + charWidth_ * 2, rowY, &temp[2], 1);
				}
			} else {
				if (continueBGCol != hexBGCol) {
					drawList->AddText(ImVec2(canvas_p0.x + hexX, canvas_p0.y + rowY), fg, &temp[0], &temp[2]);
				} else {
					drawList->AddText(ImVec2(canvas_p0.x + hexX, canvas_p0.y + rowY), fg, &temp[0], &temp[2]);
				}
			}

			// setTextColors(asciiTextCol, asciiBGCol);
			// TextOutA(hdc, asciiX, rowY, &c, 1);
			drawList->AddText(ImVec2(canvas_p0.x + asciiX, canvas_p0.y + rowY), fg, &c, &c + 1);
		}
	}

	ImGui_PopFont();
	drawList->PopClipRect();
}

// We don't have a scroll bar - though maybe we should build one.
/*
void ImMemView::onVScroll(WPARAM wParam, LPARAM lParam) {
	switch (wParam & 0xFFFF) {
	case SB_LINEDOWN:
		ScrollWindow(1, GotoModeFromModifiers(false));
		break;
	case SB_LINEUP:
		ScrollWindow(-1, GotoModeFromModifiers(false));
		break;
	case SB_PAGEDOWN:
		ScrollWindow(visibleRows_, GotoModeFromModifiers(false));
		break;
	case SB_PAGEUP:
		ScrollWindow(-visibleRows_, GotoModeFromModifiers(false));
		break;
	default:
		return;
	}
}
*/

void ImMemView::ProcessKeyboardShortcuts(bool focused) {
	if (!focused) {
		return;
	}

	ImGuiIO& io = ImGui::GetIO();

	if (io.KeyMods & ImGuiMod_Ctrl) {
		if (ImGui::IsKeyPressed(ImGuiKey_G)) {
			/*
			u32 addr;
			if (executeExpressionWindow(wnd, debugger_, addr) == false)
				return;
			gotoAddr(addr);
			return;
			*/
		}
		if (ImGui::IsKeyPressed(ImGuiKey_F)) {
			search(false);
		}
		if (ImGui::IsKeyPressed(ImGuiKey_C)) {
			search(true);
		}
	} else {
		if (ImGui::IsKeyPressed(ImGuiKey_DownArrow)) {
			ScrollCursor(rowSize_, GotoModeFromModifiers(false));
		}
		if (ImGui::IsKeyPressed(ImGuiKey_UpArrow)) {
			ScrollCursor(-rowSize_, GotoModeFromModifiers(false));
		}
		if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow)) {
			ScrollCursor(-1, GotoModeFromModifiers(false));
		}
		if (ImGui::IsKeyPressed(ImGuiKey_RightArrow)) {
			ScrollCursor(1, GotoModeFromModifiers(false));
		}
		if (ImGui::IsKeyPressed(ImGuiKey_PageDown)) {
			ScrollWindow(visibleRows_, GotoModeFromModifiers(false));
		}
		if (ImGui::IsKeyPressed(ImGuiKey_PageUp)) {
			ScrollWindow(-visibleRows_, GotoModeFromModifiers(false));
		}
	}
}

void ImMemView::onChar(int c) {
	auto memLock = Memory::Lock();
	if (!PSP_IsInited())
		return;

	ImGuiIO& io = ImGui::GetIO();

	if (io.KeyMods & ImGuiMod_Ctrl) {
		return;
	}

	if (!Memory::IsValidAddress(curAddress_)) {
		ScrollCursor(1, GotoMode::RESET);
		return;
	}

	bool active = Core_IsActive();
	if (active)
		Core_Break("memory.access", curAddress_);

	if (asciiSelected_) {
		Memory::WriteUnchecked_U8((u8)c, curAddress_);
		ScrollCursor(1, GotoMode::RESET);
	} else {
		c = tolower(c);
		int inputValue = -1;

		if (c >= '0' && c <= '9') inputValue = c - '0';
		if (c >= 'a' && c <= 'f') inputValue = c - 'a' + 10;

		if (inputValue >= 0) {
			int shiftAmount = (1 - selectedNibble_) * 4;

			u8 oldValue = Memory::ReadUnchecked_U8(curAddress_);
			oldValue &= ~(0xF << shiftAmount);
			u8 newValue = oldValue | (inputValue << shiftAmount);
			Memory::WriteUnchecked_U8(newValue, curAddress_);
			ScrollCursor(1, GotoMode::RESET);
		}
	}

	Reporting::NotifyDebugger();
	if (active)
		Core_Resume();
}

ImMemView::GotoMode ImMemView::GotoModeFromModifiers(bool isRightClick) {
	GotoMode mode = GotoMode::RESET;
	if (isRightClick) {
		mode = GotoMode::RESET_IF_OUTSIDE;
	} else if (KeyDownAsync(VK_SHIFT)) {
		if (KeyDownAsync(VK_CONTROL))
			mode = GotoMode::EXTEND;
		else
			mode = GotoMode::FROM_CUR;
	}
	return mode;
}

void ImMemView::onMouseDown(WPARAM wParam, LPARAM lParam, int button) {
	if (Achievements::HardcoreModeActive())
		return;

	int x = LOWORD(lParam);
	int y = HIWORD(lParam);

	GotoPoint(x, y, GotoModeFromModifiers(button == 2));
}

void ImMemView::onMouseUp(WPARAM wParam, LPARAM lParam, int button) {
	if (button == 2) {
		int32_t selectedSize = selectRangeEnd_ - selectRangeStart_;
		bool enable16 = !asciiSelected_ && (selectedSize == 1 || (selectedSize & 1) == 0);
		bool enable32 = !asciiSelected_ && (selectedSize == 1 || (selectedSize & 3) == 0);

		HMENU menu = GetContextMenu(ContextMenuID::MEMVIEW);
		EnableMenuItem(menu, ID_MEMVIEW_COPYVALUE_16, enable16 ? MF_ENABLED : MF_GRAYED);
		EnableMenuItem(menu, ID_MEMVIEW_COPYVALUE_32, enable32 ? MF_ENABLED : MF_GRAYED);
		EnableMenuItem(menu, ID_MEMVIEW_COPYFLOAT_32, enable32 ? MF_ENABLED : MF_GRAYED);

		if (ImGui::MenuItem("Dump memory")) {
			DumpMemoryWindow dump(wnd, debugger_);
			dump.exec();
			break;
		}

		if (ImGui::MenuItem("Copy value (8-bit)")) {
			auto memLock = Memory::Lock();
			size_t tempSize = 3 * selectedSize + 1;
			char *temp = new char[tempSize];
			memset(temp, 0, tempSize);

			// it's admittedly not really useful like this
			if (asciiSelected_) {
				for (uint32_t p = selectRangeStart_; p != selectRangeEnd_; ++p) {
					uint8_t c = Memory::IsValidAddress(p) ? Memory::ReadUnchecked_U8(p) : '.';
					if (c < 32 || c >= 128)
						c = '.';
					temp[p - selectRangeStart_] = c;
				}
			} else {
				char *pos = temp;
				for (uint32_t p = selectRangeStart_; p != selectRangeEnd_; ++p) {
					uint8_t c = Memory::IsValidAddress(p) ? Memory::ReadUnchecked_U8(p) : 0xFF;
					pos += snprintf(pos, tempSize - (pos - temp + 1), "%02X ", c);
				}
				// Clear the last space.
				if (pos > temp)
					*(pos - 1) = '\0';
			}
			// W32Util::CopyTextToClipboard(wnd, temp);
			delete[] temp;
		}

		if (ImGui::MenuItem("Copy value (16-bit)")) {
			auto memLock = Memory::Lock();
			size_t tempSize = 5 * ((selectedSize + 1) / 2) + 1;
			char *temp = new char[tempSize];
			memset(temp, 0, tempSize);

			char *pos = temp;
			for (uint32_t p = selectRangeStart_; p < selectRangeEnd_; p += 2) {
				uint16_t c = Memory::IsValidRange(p, 2) ? Memory::ReadUnchecked_U16(p) : 0xFFFF;
				pos += snprintf(pos, tempSize - (pos - temp + 1), "%04X ", c);
			}
			// Clear the last space.
			if (pos > temp)
				*(pos - 1) = '\0';

			W32Util::CopyTextToClipboard(wnd, temp);
			delete[] temp;
		}

		if (ImGui::MenuItem("Copy value (32-bit)")) {
			auto memLock = Memory::Lock();
			size_t tempSize = 9 * ((selectedSize + 3) / 4) + 1;
			char *temp = new char[tempSize];
			memset(temp, 0, tempSize);

			char *pos = temp;
			for (uint32_t p = selectRangeStart_; p < selectRangeEnd_; p += 4) {
				uint32_t c = Memory::IsValidRange(p, 4) ? Memory::ReadUnchecked_U32(p) : 0xFFFFFFFF;
				pos += snprintf(pos, tempSize - (pos - temp + 1), "%08X ", c);
			}
			// Clear the last space.
			if (pos > temp)
				*(pos - 1) = '\0';

			W32Util::CopyTextToClipboard(wnd, temp);
			delete[] temp;
		}

		if (ImGui::MenuItem("Copy value (float32)") {
			auto memLock = Memory::Lock();
			std::ostringstream stream;
			stream << (Memory::IsValidAddress(curAddress_) ? Memory::Read_Float(curAddress_) : NAN);
			auto temp_string = stream.str();
			W32Util::CopyTextToClipboard(wnd, temp_string.c_str());
		}
		/*
		case ID_MEMVIEW_EXTENTBEGIN:
		{
			std::vector<MemBlockInfo> memRangeInfo = FindMemInfoByFlag(highlightFlags_, curAddress_, 1);
			uint32_t addr = curAddress_;
			for (MemBlockInfo info : memRangeInfo) {
				addr = info.start;
			}
			gotoAddr(addr);
			break;
		}

		case ID_MEMVIEW_EXTENTEND:
		{
			std::vector<MemBlockInfo> memRangeInfo = FindMemInfoByFlag(highlightFlags_, curAddress_, 1);
			uint32_t addr = curAddress_;
			for (MemBlockInfo info : memRangeInfo) {
				addr = info.start + info.size - 1;
			}
			gotoAddr(addr);
			break;
		}
		*/
		if (ImGui::MenuItem("Copy address")) {
			char temp[24];
			snprintf(temp, sizeof(temp), "0x%08X", curAddress_);
			W32Util::CopyTextToClipboard(wnd, temp);
		}

		if (ImGui::MenuItem("Goto in disasm")) {
			if (disasmWindow) {
				disasmWindow->Goto(curAddress_);
				disasmWindow->Show(true);
			}
		}
	}

	/*
	int x = LOWORD(lParam);
	int y = HIWORD(lParam);
	ReleaseCapture();
	*/
	GotoPoint(x, y, GotoModeFromModifiers(button == 2));
}

void ImMemView::onMouseMove(WPARAM wParam, LPARAM lParam, int button) {
	if (Achievements::HardcoreModeActive())
		return;

	int x = LOWORD(lParam);
	int y = HIWORD(lParam);

	if (button & 1) {
		GotoPoint(x, y, GotoModeFromModifiers(button == 2));
	}
}

void ImMemView::updateStatusBarText() {
	std::vector<MemBlockInfo> memRangeInfo = FindMemInfoByFlag(highlightFlags_, curAddress_, 1);

	char text[512];
	snprintf(text, sizeof(text), "%08X", curAddress_);
	// There should only be one.
	for (MemBlockInfo info : memRangeInfo) {
		snprintf(text, sizeof(text), "%08X - %s %08X-%08X (at PC %08X / %lld ticks)", curAddress_, info.tag.c_str(), info.start, info.start + info.size, info.pc, info.ticks);
	}

	SendMessage(GetParent(wnd), WM_DEB_SETSTATUSBARTEXT, 0, (LPARAM)text);
}

void ImMemView::UpdateSelectRange(uint32_t target, GotoMode mode) {
	if (mode == GotoMode::FROM_CUR && lastSelectReset_ == 0) {
		lastSelectReset_ = curAddress_;
	}

	switch (mode) {
	case GotoMode::RESET:
		selectRangeStart_ = target;
		selectRangeEnd_ = target + 1;
		lastSelectReset_ = target;
		break;

	case GotoMode::RESET_IF_OUTSIDE:
		if (target < selectRangeStart_ || target >= selectRangeEnd_) {
			selectRangeStart_ = target;
			selectRangeEnd_ = target + 1;
			lastSelectReset_ = target;
		}
		break;

	case GotoMode::FROM_CUR:
		selectRangeStart_ = lastSelectReset_ > target ? target : lastSelectReset_;
		selectRangeEnd_ = selectRangeStart_ == lastSelectReset_ ? target + 1 : lastSelectReset_ + 1;
		break;

	case GotoMode::EXTEND:
		if (target < selectRangeStart_)
			selectRangeStart_ = target;
		if (target > selectRangeEnd_)
			selectRangeEnd_ = target;
		break;
	}
	curAddress_ = target;
}

void ImMemView::GotoPoint(int x, int y, GotoMode mode) {
	int line = y / rowHeight_;
	int lineAddress = windowStart_ + line * rowSize_;

	if (displayOffsetScale_) {
		// ignore clicks on the offset space
		if (line < offsetSpace) {
			updateStatusBarText();
			redraw();
			return;
		}
		// since each row has been written X rows down from where the window expected it to be written the target of the clicks must be adjusted
		lineAddress -= rowSize_ * offsetSpace;
	}

	uint32_t target = curAddress_;
	uint32_t targetNibble = selectedNibble_;
	bool targetAscii = asciiSelected_;
	if (x >= asciiStartX_) {
		int col = (x - asciiStartX_) / (charWidth_ + 2);
		if (col >= rowSize_)
			return;

		targetAscii = true;
		target = lineAddress + col;
		targetNibble = 0;
	} else if (x >= hexStartX_) {
		int col = (x - hexStartX_) / charWidth_;
		if ((col / 3) >= rowSize_)
			return;

		switch (col % 3) {
		case 0: targetNibble = 0; break;
		case 1: targetNibble = 1; break;
		case 2: return;		// don't change position when clicking on the space
		}

		targetAscii = false;
		target = lineAddress + col / 3;
	}

	if (target != curAddress_ || targetNibble != selectedNibble_ || targetAscii != asciiSelected_) {
		selectedNibble_ = targetNibble;
		asciiSelected_ = targetAscii;
		UpdateSelectRange(target, mode);

		updateStatusBarText();
		redraw();
	}
}

void ImMemView::gotoAddr(unsigned int addr) {
	int lines = rect_.bottom / rowHeight_;
	u32 windowEnd = windowStart_ + lines * rowSize_;

	curAddress_ = addr;
	lastSelectReset_ = curAddress_;
	selectRangeStart_ = curAddress_;
	selectRangeEnd_ = curAddress_ + 1;
	selectedNibble_ = 0;

	if (curAddress_ < windowStart_ || curAddress_ >= windowEnd) {
		windowStart_ = curAddress_ & ~15;
	}

	updateStatusBarText();
	redraw();
}

void ImMemView::ScrollWindow(int lines, GotoMode mode) {
	windowStart_ += lines * rowSize_;

	UpdateSelectRange(curAddress_ + lines * rowSize_, mode);

	updateStatusBarText();
	redraw();
}

void ImMemView::ScrollCursor(int bytes, GotoMode mode) {
	if (!asciiSelected_ && bytes == 1) {
		if (selectedNibble_ == 0) {
			selectedNibble_ = 1;
			bytes = 0;
		} else {
			selectedNibble_ = 0;
		}
	} else if (!asciiSelected_ && bytes == -1) {
		if (selectedNibble_ == 0) {
			selectedNibble_ = 1;
		} else {
			selectedNibble_ = 0;
			bytes = 0;
		}
	}

	UpdateSelectRange(curAddress_ + bytes, mode);

	u32 windowEnd = windowStart_ + visibleRows_ * rowSize_;
	if (curAddress_ < windowStart_) {
		windowStart_ = curAddress_ & ~15;
	} else if (curAddress_ >= windowEnd) {
		windowStart_ = (curAddress_ - (visibleRows_ - 1) * rowSize_) & ~15;
	}

	updateStatusBarText();
	redraw();
}

bool ImMemView::ParseSearchString(const std::string &query, bool asHex, std::vector<uint8_t> &data) {
	data.clear();
	if (!asHex) {
		for (size_t i = 0; i < query.length(); i++) {
			data.push_back(query[i]);
		}
		return true;
	}

	for (size_t index = 0; index < query.size(); ) {
		if (isspace(query[index])) {
			index++;
			continue;
		}

		u8 value = 0;
		for (int i = 0; i < 2 && index < query.size(); i++) {
			char c = tolower(query[index++]);
			if (c >= 'a' && c <= 'f') {
				value |= (c - 'a' + 10) << (1 - i) * 4;
			} else  if (c >= '0' && c <= '9') {
				value |= (c - '0') << (1 - i) * 4;
			} else {
				return false;
			}
		}

		data.push_back(value);
	}

	return true;
}

std::vector<u32> ImMemView::searchString(const std::string &searchQuery) {
	std::vector<u32> searchResAddrs;

	auto memLock = Memory::Lock();
	if (!PSP_IsInited())
		return searchResAddrs;

	std::vector<u8> searchData;
	if (!ParseSearchString(searchQuery, false, searchData))
		return searchResAddrs;

	if (searchData.empty())
		return searchResAddrs;

	std::vector<std::pair<u32, u32>> memoryAreas;
	memoryAreas.emplace_back(PSP_GetScratchpadMemoryBase(), PSP_GetScratchpadMemoryEnd());
	// Ignore the video memory mirrors.
	memoryAreas.emplace_back(PSP_GetVidMemBase(), 0x04200000);
	memoryAreas.emplace_back(PSP_GetKernelMemoryBase(), PSP_GetUserMemoryEnd());

	for (const auto &area : memoryAreas) {
		const u32 segmentStart = area.first;
		const u32 segmentEnd = area.second - (u32)searchData.size();

		for (u32 pos = segmentStart; pos < segmentEnd; pos++) {
			if ((pos % 256) == 0 && ImGui::IsKeyDown(ImGuiKey_Escape)) {
				return searchResAddrs;
			}

			const u8 *ptr = Memory::GetPointerUnchecked(pos);
			if (memcmp(ptr, searchData.data(), searchData.size()) == 0) {
				searchResAddrs.push_back(pos);
			}
		}
	}

	return searchResAddrs;
};

void ImMemView::search(bool continueSearch) {
	auto memLock = Memory::Lock();
	if (!PSP_IsInited())
		return;

	u32 searchAddress = 0;
	u32 segmentStart = 0;
	u32 segmentEnd = 0;
	if (continueSearch == false || searchQuery_.empty()) {
		if (InputBox_GetString(GetModuleHandle(NULL), wnd, L"Search for", searchQuery_, searchQuery_) == false) {
			SetFocus(wnd);
			return;
		}
		SetFocus(wnd);
		searchAddress = curAddress_ + 1;
	} else {
		searchAddress = matchAddress_ + 1;
	}

	std::vector<u8> searchData;
	if (!ParseSearchString(searchQuery_, !asciiSelected_, searchData)) {
		statusMessage_ = "Invalid search text.";
		return;
	}

	std::vector<std::pair<u32, u32>> memoryAreas;
	// Ignore the video memory mirrors.
	memoryAreas.emplace_back(PSP_GetVidMemBase(), 0x04200000);
	memoryAreas.emplace_back(PSP_GetKernelMemoryBase(), PSP_GetUserMemoryEnd());
	memoryAreas.emplace_back(PSP_GetScratchpadMemoryBase(), PSP_GetScratchpadMemoryEnd());

	searching_ = true;
	redraw();	// so the cursor is disabled

	for (size_t i = 0; i < memoryAreas.size(); i++) {
		segmentStart = memoryAreas[i].first;
		segmentEnd = memoryAreas[i].second;

		// better safe than sorry, I guess
		if (!Memory::IsValidAddress(segmentStart))
			continue;
		const u8 *dataPointer = Memory::GetPointerUnchecked(segmentStart);

		if (searchAddress < segmentStart)
			searchAddress = segmentStart;
		if (searchAddress >= segmentEnd)
			continue;

		int index = searchAddress - segmentStart;
		int endIndex = segmentEnd - segmentStart - (int)searchData.size();

		while (index < endIndex) {
			// cancel search
			if ((index % 256) == 0 && ImGui::IsKeyDown(ImGuiKey_Escape)) {
				searching_ = false;
				return;
			}
			if (memcmp(&dataPointer[index], searchData.data(), searchData.size()) == 0) {
				matchAddress_ = index + segmentStart;
				searching_ = false;
				gotoAddr(matchAddress_);
				return;
			}
			index++;
		}
	}

	statusMessage_ = "Not found";
	searching_ = false;
	redraw();
}

void ImMemView::drawOffsetScale(ImDrawList *drawList) {
	int currentX = addressStartX_;

	drawList->AddText(ImVec2(currentX, offsetPositionY_), 0xFF600000, "Offset");
	// SetTextColor(hdc, 0x600000);
	// TextOutA(hdc, currentX, offsetPositionY_, "Offset", 6);

	// the start offset, the size of the hex addresses and one space
	currentX = addressStartX_ + ((8 + 1) * charWidth_);

	char temp[64];
	for (int i = 0; i < 16; i++) {
		snprintf(temp, sizeof(temp), "%02X", i);
		drawList->AddText(ImVec2(currentX, offsetPositionY_), 0xFF600000, temp);
		currentX += 3 * charWidth_; // hex and space
	}
}

void ImMemView::toggleOffsetScale(CommonToggles toggle) {
	if (toggle == On)
		displayOffsetScale_ = true;
	else if (toggle == Off)
		displayOffsetScale_ = false;

	updateStatusBarText();
	redraw();
}

void ImMemView::setHighlightType(MemBlockFlags flags) {
	if (highlightFlags_ != flags) {
		highlightFlags_ = flags;
		updateStatusBarText();
		redraw();
	}
}

uint32_t ImMemView::pickTagColor(const std::string &tag) {
	int colors[6] = { 0xe0FFFF, 0xFFE0E0, 0xE8E8FF, 0xFFE0FF, 0xE0FFE0, 0xFFFFE0 };
	int which = XXH3_64bits(tag.c_str(), tag.length()) % ARRAY_SIZE(colors);
	return colors[which];
}
