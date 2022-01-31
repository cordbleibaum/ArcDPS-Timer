#include "timer.h"

#include <string>
#include <fstream>
#include <filesystem>
#include <chrono>

#include "json.hpp"
using json = nlohmann::json;


arcdps_exports arc_exports;
char* arc_version;

void* filelog;
void* arclog;

bool showTimer = false;
bool windowBorder = true;

std::string config_file = "timer_config.json";

bool timer_running = true;
bool timer_prepared = true;
std::chrono::system_clock::time_point start_time;
std::chrono::system_clock::time_point current_time;


void log_file(char* str) {
	size_t(*log)(char*) = (size_t(*)(char*))filelog;
	if (log) (*log)(str);
	return;
}

void log_arc(char* str) {
	size_t(*log)(char*) = (size_t(*)(char*))arclog;
	if (log) (*log)(str);
	return;
}

extern "C" __declspec(dllexport) void* get_init_addr(char *arcversion, ImGuiContext *imguictx, void *id3dptr, HANDLE arcdll, void *mallocfn, void *freefn, uint32_t d3dversion) {
	arc_version = arcversion;
	filelog = (void*)GetProcAddress((HMODULE)arcdll, "e3");
	arclog = (void*)GetProcAddress((HMODULE)arcdll, "e8");
	ImGui::SetCurrentContext((ImGuiContext*)imguictx);
	ImGui::SetAllocatorFunctions((void* (*)(size_t, void*))mallocfn, (void (*)(void*, void*))freefn);
	return mod_init;
}

extern "C" __declspec(dllexport) void* get_release_addr() {
	arc_version = 0;
	return mod_release;
}

arcdps_exports* mod_init() {
	json config;
	if (std::filesystem::exists(config_file)) {
		std::ifstream i(config_file);
		i >> config;
	}
	else {
		config["showTimer"] = true;
		config["windowBorder"] = true;
	}
	showTimer = config["showTimer"];
	windowBorder = config["windowBorder"];

	start_time = std::chrono::system_clock::now();
	current_time = std::chrono::system_clock::now();
	timer_running = false;
	timer_prepared = false;

	memset(&arc_exports, 0, sizeof(arcdps_exports));
	arc_exports.sig = 0x1A0;
	arc_exports.imguivers = IMGUI_VERSION_NUM;
	arc_exports.size = sizeof(arcdps_exports);
	arc_exports.out_name = "Timer";
	arc_exports.out_build = "0.1";
	arc_exports.options_end = mod_options;
	arc_exports.options_windows = mod_windows;
	arc_exports.imgui = mod_imgui;
	log_arc((char*)"combatdemo: done mod_init");
	log_file((char*)"combatdemo: done mod init");
	return &arc_exports;
}

uintptr_t mod_release() {
	json config;
	config["showTimer"] = showTimer;
	config["windowBorder"] = windowBorder;
	std::ofstream o(config_file);
	o << std::setw(4) << config << std::endl;

	return 0;
}

uintptr_t mod_options() {
	ImGui::Checkbox("Timer Window Border", &windowBorder);
	return 0;
}

uintptr_t mod_windows(const char* windowname) {
	if (!windowname) {
		ImGui::Checkbox("Timer", &showTimer);
	}
	return 0;
}

uintptr_t mod_imgui(uint32_t not_charsel_or_loading) {
	if (!not_charsel_or_loading) return 0;

	if (timer_running) {
		current_time = std::chrono::system_clock::now();
	}

	if (showTimer) {
		ImGuiWindowFlags flags = ImGuiWindowFlags_AlwaysAutoResize;
		if (!windowBorder) {
			flags |= ImGuiWindowFlags_NoDecoration;
		}

		ImGui::Begin("Timer", &showTimer, flags);

		std::chrono::duration<double> duration_dbl = current_time - start_time;
		double duration = duration_dbl.count();
		int minutes = (int) duration / 60;
		duration -= minutes * 60;
		int seconds = (int) duration;
		duration -= seconds;
		int milliseconds = (int) (duration * 100);
		ImGui::Text("%02d:%02d.%02d", minutes, seconds, milliseconds);

		if (ImGui::Button("Prepare", ImVec2(100, 20))) {
			timer_prepare();
		}

		if (ImGui::Button("Start")) {
			timer_start();
		}
		
		ImGui::SameLine();
		
		if (ImGui::Button("Stop")) {
			timer_stop();
		}

		ImGui::End();
	}



	return 0;
}

void timer_start() {
	timer_running = true;
	timer_prepared = false;
	start_time = std::chrono::system_clock::now();
}

void timer_stop() {
	timer_running = false;
	timer_prepared = false;
}

void timer_prepare() {
	timer_running = false;
	timer_prepared = true;
}
