#pragma once

#include "arcdps.h"

arcdps_exports* mod_init();
uintptr_t mod_release();
uintptr_t mod_options();
uintptr_t mod_windows(const char* windowname);
uintptr_t mod_imgui(uint32_t not_charsel_or_loading);
uintptr_t mod_combat(cbtevent* ev, ag* src, ag* dst, char* skillname, uint64_t id, uint64_t revision);
uintptr_t mod_wnd(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

void timer_start();
void timer_start(uint64_t time);
void timer_stop();
void timer_stop(uint64_t time);
void timer_prepare();
void timer_reset();

void sync_timer();
