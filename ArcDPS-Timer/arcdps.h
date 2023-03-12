#pragma once

#include <cstdint>
#include <Windows.h>
#include <string>

#include "imgui/imgui.h"
#include "arcdps-extension/arcdps_structs.h"

extern "C" __declspec(dllexport) void* get_init_addr(char* arcversion, ImGuiContext * imguictx, void* id3dptr, HANDLE arcdll, void* mallocfn, void* freefn, uint32_t d3dversion);
extern "C" __declspec(dllexport) void* get_release_addr();

void log_arc(std::string str);
void log_file(std::string str);

void log(std::string str);
void log_debug(std::string str);

extern arcdps_exports arc_exports;

namespace arcdps {
	bool is_buffdmg(cbtevent* ev);
	bool is_directdmg(cbtevent* ev);
}