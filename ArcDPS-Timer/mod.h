#pragma once

#include "arcdps.h"

arcdps_exports* mod_init();
uintptr_t mod_release();
uintptr_t mod_options();
uintptr_t mod_windows(const char* windowname);
uintptr_t mod_imgui(uint32_t not_charsel_or_loading);
uintptr_t mod_combat(cbtevent* ev, ag* src, ag* dst, const char* skillname, uint64_t id, uint64_t revision);
uintptr_t mod_wnd(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
