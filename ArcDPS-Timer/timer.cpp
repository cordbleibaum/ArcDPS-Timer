#include "timer.h"

#include <string>
#include <fstream>
#include <filesystem>
#include <chrono>
#include <cmath>

#include <cpr/cpr.h>


#include "imgui_stdlib.h"

#include "json.hpp"
using json = nlohmann::json;

#include "mumble_link.h"

enum class TimerStatus { stopped, prepared, running };

arcdps_exports arc_exports;
char* arc_version;

void* filelog;
void* arclog;

bool showTimer = false;
bool windowBorder = true;

std::string config_file = "addons/arcdps/timer.json";
constexpr int version = 2;

TimerStatus status;
std::chrono::system_clock::time_point start_time;
std::chrono::system_clock::time_point current_time;
double delta = 0;

HANDLE hMumbleLink;
LinkedMem *pMumbleLink;
float lastPosition[3];

std::string server;
std::string group;
std::chrono::system_clock::time_point last_update;
constexpr int sync_interval = 3;

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

std::string gen_random_string(const int len) {
	static const char alphanum[] =
		"0123456789"
		"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
		"abcdefghijklmnopqrstuvwxyz";
	std::string tmp_s;
	tmp_s.reserve(len);

	for (int i = 0; i < len; ++i) {
		tmp_s += alphanum[rand() % (sizeof(alphanum) - 1)];
	}

	return tmp_s;
}

void generate_config(json* config) {
	(*config)["version"] = version;
	(*config)["showTimer"] = true;
	(*config)["windowBorder"] = false;
	(*config)["server"] = "http:/127.0.0.1:5000/";
	(*config)["group"] = gen_random_string(10);
}


arcdps_exports* mod_init() {
	json config;
	if (std::filesystem::exists(config_file)) {
		std::ifstream i(config_file);
		i >> config;
		if (config["version"] < version) {
			generate_config(&config);
		}
	}
	else {
		generate_config(&config);
	}
	showTimer = config["showTimer"];
	windowBorder = config["windowBorder"];
	server = config["server"];
	group = config["group"];

	start_time = std::chrono::system_clock::now();
	current_time = std::chrono::system_clock::now();
	status = TimerStatus::stopped;

	hMumbleLink = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, sizeof(LinkedMem), L"MumbleLink");
	if (hMumbleLink == NULL)
	{
		log_arc((char*)"Could not create mumble link file mapping object\n");
	}
	else {
		pMumbleLink = (LinkedMem*)MapViewOfFile(hMumbleLink, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(LinkedMem));
		if (pMumbleLink == NULL) {
			log_arc((char*)"Failed to open mumble link file\n");
		}
	}

	memset(&arc_exports, 0, sizeof(arcdps_exports));
	arc_exports.sig = 0x1A0;
	arc_exports.imguivers = IMGUI_VERSION_NUM;
	arc_exports.size = sizeof(arcdps_exports);
	arc_exports.out_name = "Timer";
	arc_exports.out_build = "0.1";
	arc_exports.options_end = mod_options;
	arc_exports.options_windows = mod_windows;
	arc_exports.imgui = mod_imgui;
	arc_exports.combat = mod_combat;
	log_arc((char*)"timer: done mod_init");
	return &arc_exports;
}

uintptr_t mod_release() {
	json config;
	config["showTimer"] = showTimer;
	config["windowBorder"] = windowBorder;
	config["server"] = server;
	config["group"] = group;
	config["version"] = version;
	std::ofstream o(config_file);
	o << std::setw(4) << config << std::endl;

	UnmapViewOfFile(pMumbleLink);
	CloseHandle(hMumbleLink);

	return 0;
}

uintptr_t mod_options() {
	ImGui::Checkbox("Window Border", &windowBorder);
	ImGui::InputText("Server", &server);
	ImGui::InputText("Group", &group);
	return 0;
}

uintptr_t mod_windows(const char* windowname) {
	if (!windowname) {
		ImGui::Checkbox("Timer", &showTimer);
	}
	return 0;
}

bool checkDelta(float a, float b, float delta) {
	return std::abs(a - b) > delta;
}

uintptr_t mod_imgui(uint32_t not_charsel_or_loading) {
	if (!not_charsel_or_loading) return 0;

	if (status == TimerStatus::running) {
		current_time = std::chrono::system_clock::now();
	}
	else if (status == TimerStatus::prepared) {
		if (checkDelta(lastPosition[0], pMumbleLink->fAvatarPosition[0], 1) ||
			checkDelta(lastPosition[1], pMumbleLink->fAvatarPosition[1], 1) ||
			checkDelta(lastPosition[2], pMumbleLink->fAvatarPosition[2], 1)) {
			timer_start();
			log_arc((char*)"timer: starting on movement");
		}
	}

	if (std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now() - last_update).count() > sync_interval) {
		last_update = std::chrono::system_clock::now();
		sync_timer();
		std::thread sync_thread(sync_timer);
		sync_thread.detach();
	}

	if (showTimer) {
		ImGuiWindowFlags flags = ImGuiWindowFlags_AlwaysAutoResize;
		if (!windowBorder) {
			flags |= ImGuiWindowFlags_NoDecoration;
		}

		ImGui::Begin("Timer", &showTimer, flags);

		ImGui::Dummy(ImVec2(0.0f, 3.0f));

		std::chrono::duration<double> duration_dbl = current_time - start_time;
		double duration = duration_dbl.count() + delta;
		int minutes = (int) duration / 60;
		duration -= minutes * 60;
		int seconds = (int) duration;
		duration -= seconds;
		int milliseconds = (int) (duration * 100);

		auto windowWidth = ImGui::GetWindowSize().x;
		auto textWidth = ImGui::CalcTextSize("00:00.00").x;
		ImGui::SetCursorPosX((windowWidth - textWidth) * 0.5f);
		ImGui::Text("%02d:%02d.%02d", minutes, seconds, milliseconds);

		ImGui::Dummy(ImVec2(0.0f, 3.0f));

		if (ImGui::Button("Prepare", ImVec2(190, 20))) {
			timer_prepare();
		}

		if (ImGui::Button("Start", ImVec2(60, 20))) {
			timer_start();
		}
		
		ImGui::SameLine(0, 5);
		
		if (ImGui::Button("Stop", ImVec2(60, 20))) {
			timer_stop();
		}

		ImGui::SameLine(0, 5);

		if (ImGui::Button("Reset", ImVec2(60, 20))) {
			timer_reset();
		}

		ImGui::End();
	}

	return 0;
}

void timer_start() {
	status = TimerStatus::running;
	start_time = std::chrono::system_clock::now();
	delta = 0;
}

void timer_stop() {
	status = TimerStatus::stopped;
}

void timer_prepare() {
	status = TimerStatus::prepared;
	lastPosition[0] = pMumbleLink->fAvatarPosition[0];
	lastPosition[1] = pMumbleLink->fAvatarPosition[1];
	lastPosition[2] = pMumbleLink->fAvatarPosition[2];
}

uintptr_t mod_combat(cbtevent* ev, ag* src, ag* dst, char* skillname, uint64_t id, uint64_t revision) {
	if (!ev) return 0;

	if (ev->is_activation) {
		std::chrono::duration<double> duration_dbl = std::chrono::system_clock::now() - start_time;
		double duration = duration_dbl.count() + delta;

		if (status == TimerStatus::prepared) {
			timer_start();
			delta = 3;
			log_arc((char*)"timer: starting on skill");
		}
		else if (status == TimerStatus::running && duration < 3) {
			delta = 3;
			start_time = std::chrono::system_clock::now();
			log_arc((char*)"timer: retiming on skill");
		}
	}

	return 0;
}

void timer_reset() {
	status = TimerStatus::stopped;
	start_time = std::chrono::system_clock::now();
	current_time = std::chrono::system_clock::now();
}

void sync_timer() {
	cpr::Response r = cpr::Get(cpr::Url{server+"groups/"+group});

	if (r.status_code != 200) {
		log_arc((char*)"Failed to sync with server");
		return;
	}
}
