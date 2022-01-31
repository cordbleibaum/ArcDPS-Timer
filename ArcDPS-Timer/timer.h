#pragma once

#include "arcdps.h"

arcdps_exports* mod_init();
uintptr_t mod_release();
uintptr_t mod_options();
uintptr_t mod_windows(const char* windowname);
uintptr_t mod_imgui(uint32_t not_charsel_or_loading);

void timer_start();
void timer_stop();
void timer_prepare();
