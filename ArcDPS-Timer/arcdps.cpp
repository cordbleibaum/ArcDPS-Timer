#include "arcdps.h"
#include "timer.h"

void* arclog;
void* filelog;

void log_arc(std::string str) {
	size_t(*log)(char*) = (size_t(*)(char*))arclog;
	if (log) (*log)(str.data());
	return;
}

void log_file(std::string str) {
	size_t(*log)(char*) = (size_t(*)(char*))filelog;
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
	arclog = (void*)GetProcAddress((HMODULE)arcdll, "e8");
	filelog = (void*)GetProcAddress((HMODULE)arcdll, "e3");
	ImGui::SetCurrentContext((ImGuiContext*)imguictx);
	ImGui::SetAllocatorFunctions((void* (*)(size_t, void*))mallocfn, (void (*)(void*, void*))freefn);
	return mod_init;
}

extern "C" __declspec(dllexport) void* get_release_addr() {
	return mod_release;
}
