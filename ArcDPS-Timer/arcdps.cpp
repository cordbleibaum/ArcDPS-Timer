#include "arcdps.h"
#include "mod.h"

arcdps_exports arc_exports;

arc_export_func_u64 ARC_EXPORT_E6;
arc_export_func_u64 ARC_EXPORT_E7;
e3_func_ptr ARC_LOG_FILE;
e3_func_ptr ARC_LOG;

void log_arc(std::string str) {
	size_t(*log)(char*) = (size_t(*)(char*))ARC_LOG;
	if (log) (*log)(str.data());
	return;
}

void log_file(std::string str) {
	size_t(*log)(char*) = (size_t(*)(char*))ARC_LOG_FILE;
	if (log) (*log)(str.data());
	return;
}

void log(std::string str) {
	log_arc(str);
	log_file(str);
}

void log_debug(std::string str) {
#ifndef NDEBUG
	log(str);
#endif // !NDEBUG
}

extern "C" __declspec(dllexport) void* get_init_addr(char* arcversion, ImGuiContext * imguictx, void* id3dptr, HANDLE arcdll, void* mallocfn, void* freefn, uint32_t d3dversion) {
	ARC_LOG = (e3_func_ptr)GetProcAddress((HMODULE)arcdll, "e8");
	ARC_LOG_FILE = (e3_func_ptr)GetProcAddress((HMODULE)arcdll, "e3");
	ARC_EXPORT_E6 = (arc_export_func_u64)GetProcAddress((HMODULE)arcdll, "e6");
	ARC_EXPORT_E7 = (arc_export_func_u64)GetProcAddress((HMODULE)arcdll, "e7");
	ImGui::SetCurrentContext((ImGuiContext*)imguictx);
	ImGui::SetAllocatorFunctions((void* (*)(size_t, void*))mallocfn, (void (*)(void*, void*))freefn);
	return mod_init;
}

extern "C" __declspec(dllexport) void* get_release_addr() {
	return mod_release;
}
