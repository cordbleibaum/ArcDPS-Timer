#include "timer.h"

#include <string>
#include <fstream>
#include <filesystem>
#include <chrono>
#include <cmath>
#include <format>
#include <set>
#include <regex>

#include <cpr/cpr.h>

#include "imgui_stdlib.h"

#include "json.hpp"
using json = nlohmann::json;

#include "mumble_link.h"

#include "hash-library/crc32.h"

enum class TimerStatus { stopped, prepared, running };

arcdps_exports arc_exports;
void* arclog;

bool showTimer = false;
bool windowBorder = true;

std::string config_file = "addons/arcdps/timer.json";
constexpr int version = 5;

TimerStatus status;
std::chrono::system_clock::time_point start_time;
std::chrono::system_clock::time_point current_time;

HANDLE hMumbleLink;
LinkedMem *pMumbleLink;
float lastPosition[3];

std::string server;
std::string selfAccountName;
std::string group_code;
std::set<std::string> group_players;
std::chrono::system_clock::time_point last_update;
int sync_interval;

std::mutex groupcode_mutex;

bool groupWidePrepare;
bool autoPrepare;
bool offline;
bool outOfDate = false;

void log_arc(std::string str) {
	size_t(*log)(char*) = (size_t(*)(char*))arclog;
	if (log) (*log)(str.data());
	return;
}

extern "C" __declspec(dllexport) void* get_init_addr(char *arcversion, ImGuiContext *imguictx, void *id3dptr, HANDLE arcdll, void *mallocfn, void *freefn, uint32_t d3dversion) {
	arclog = (void*)GetProcAddress((HMODULE)arcdll, "e8");
	ImGui::SetCurrentContext((ImGuiContext*)imguictx);
	ImGui::SetAllocatorFunctions((void* (*)(size_t, void*))mallocfn, (void (*)(void*, void*))freefn);
	return mod_init;
}

extern "C" __declspec(dllexport) void* get_release_addr() {
	return mod_release;
}

arcdps_exports* mod_init() {
	memset(&arc_exports, 0, sizeof(arcdps_exports));
	arc_exports.sig = 0x1A0;
	arc_exports.imguivers = IMGUI_VERSION_NUM;
	arc_exports.size = sizeof(arcdps_exports);
	arc_exports.out_name = "Timer";
	arc_exports.out_build = "0.2";
	arc_exports.options_end = mod_options;
	arc_exports.options_windows = mod_windows;
	arc_exports.imgui = mod_imgui;
	arc_exports.combat = mod_combat;

	json config;
	config["filler"] = "empty";
	if (std::filesystem::exists(config_file)) {
		std::ifstream input(config_file);
		input >> config;
		if (config["version"] < version) {
			config.clear();
		}
	}
	showTimer = config.value("showTimer", true);
	windowBorder = config.value("windowBorder", false);
	server = config.value("server", "http://164.92.229.177:5001/");
	sync_interval = config.value("sync_interval", 1);
	groupWidePrepare = config.value("groupWidePrepare", true);
	autoPrepare = config.value("autoPrepare", true);
	offline = config.value("offline", false);

	start_time = std::chrono::system_clock::now();
	current_time = std::chrono::system_clock::now();
	status = TimerStatus::stopped;

	hMumbleLink = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, sizeof(LinkedMem), L"MumbleLink");
	if (hMumbleLink == NULL) {
		log_arc("Could not create mumble link file mapping object\n");
		arc_exports.sig = 0;
	}
	else {
		pMumbleLink = (LinkedMem*) MapViewOfFile(hMumbleLink, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(LinkedMem));
		if (pMumbleLink == NULL) {
			log_arc("Failed to open mumble link file\n");
			arc_exports.sig = 0;
		}
	}

	auto response = cpr::Get(cpr::Url{ server + "version" });
	auto data = json::parse(response.text);
	if (data["major"] != version) {
		log_arc("Out of date version, going offline mode\n");
		outOfDate = true;
	}

	log_arc("timer: done mod_init");
	return &arc_exports;
}

uintptr_t mod_release() {
	json config;
	config["showTimer"] = showTimer;
	config["windowBorder"] = windowBorder;
	config["server"] = server;
	config["version"] = version;
	config["sync_interval"] = sync_interval;
	config["groupWidePrepare"] = groupWidePrepare;
	config["autoPrepare"] = autoPrepare;
	config["offline"] = offline;
	std::ofstream o(config_file);
	o << std::setw(4) << config << std::endl;

	UnmapViewOfFile(pMumbleLink);
	CloseHandle(hMumbleLink);

	return 0;
}

uintptr_t mod_options() {
	ImGui::Checkbox("Window Border", &windowBorder);
	ImGui::InputText("Server", &server);
	ImGui::InputInt("Sync Interval", &sync_interval);
	ImGui::Checkbox("Offline Mode", &offline);
	ImGui::Checkbox("Auto Prepare", &autoPrepare);
	ImGui::Checkbox("Groupwide Prepare", &groupWidePrepare);
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

template<typename ... Args>
std::string string_format(const std::string& format, Args ... args)
{
	int size_s = std::snprintf(nullptr, 0, format.c_str(), args ...) + 1;
	if (size_s <= 0) { throw std::runtime_error("Error during formatting."); }
	auto size = static_cast<size_t>(size_s);
	auto buf = std::make_unique<char[]>(size);
	std::snprintf(buf.get(), size, format.c_str(), args ...);
	return std::string(buf.get(), buf.get() + size - 1);
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
			timer_start(0);
			log_arc("timer: starting on movement");
		}
	}

	if (std::chrono::duration_cast<std::chrono::duration<float>>(std::chrono::system_clock::now() - last_update).count() > sync_interval) {
		last_update = std::chrono::system_clock::now();
		if (!offline && !outOfDate) {
			std::thread sync_thread(sync_timer);
			sync_thread.detach();
		}
	}

	if (showTimer) {
		ImGuiWindowFlags flags = ImGuiWindowFlags_AlwaysAutoResize;
		if (!windowBorder) {
			flags |= ImGuiWindowFlags_NoDecoration;
		}

		ImGui::Begin("Timer", &showTimer, flags);

		ImGui::Dummy(ImVec2(0.0f, 3.0f));

		std::chrono::duration<double> duration_dbl = current_time - start_time;
		double duration = duration_dbl.count();
		int minutes = (int) duration / 60;
		duration -= minutes * 60;
		int seconds = (int) duration;
		duration -= seconds;
		int milliseconds = (int) (duration * 100);
		std::string time_string = string_format("%02d:%02d.%02d", minutes, seconds, milliseconds);

		auto windowWidth = ImGui::GetWindowSize().x;
		auto textWidth = ImGui::CalcTextSize(time_string.c_str()).x;

		ImGui::SetCursorPosX((windowWidth - textWidth) * 0.5f);
		ImGui::Text(time_string.c_str());

		ImGui::Dummy(ImVec2(0.0f, 3.0f));

		if (ImGui::Button("Prepare", ImVec2(190, ImGui::GetFontSize()*1.5f))) {
			timer_prepare();
		}

		if (ImGui::Button("Start", ImVec2(60, ImGui::GetFontSize() * 1.5f))) {
			timer_start(0);
		}
		
		ImGui::SameLine(0, 5);
		
		if (ImGui::Button("Stop", ImVec2(60, ImGui::GetFontSize() * 1.5f))) {
			timer_stop();
		}

		ImGui::SameLine(0, 5);

		if (ImGui::Button("Reset", ImVec2(60, ImGui::GetFontSize() * 1.5f))) {
			timer_reset();
		}

		ImGui::End();
	}

	return 0;
}

void timer_start(int delta) {
	status = TimerStatus::running;
	start_time = std::chrono::system_clock::now() - std::chrono::seconds(delta);

	if (!offline && !outOfDate) {
		std::thread request_thread([&]() {
			json request;
			request["time"] = std::format("{:%FT%T}", std::chrono::floor<std::chrono::milliseconds>(start_time));

			cpr::Post(
				cpr::Url{ server + "groups/" + group_code + "/start" },
				cpr::Body{ request.dump() },
				cpr::Header{ {"Content-Type", "application/json"} }
			);
		});
		request_thread.detach();
	}
}

void timer_stop() {
	status = TimerStatus::stopped;

	if (!offline && !outOfDate) {
		std::thread request_thread([&]() {
			json request;
			request["time"] = std::format("{:%FT%T}", std::chrono::floor<std::chrono::milliseconds>(current_time));

			cpr::Post(
				cpr::Url{ server + "groups/" + group_code + "/stop" },
				cpr::Body{ request.dump() },
				cpr::Header{ {"Content-Type", "application/json"} }
			);
		});
		request_thread.detach();
	}
}

void timer_prepare() {
	status = TimerStatus::prepared;
	start_time = std::chrono::system_clock::now();
	current_time = std::chrono::system_clock::now();
	lastPosition[0] = pMumbleLink->fAvatarPosition[0];
	lastPosition[1] = pMumbleLink->fAvatarPosition[1];
	lastPosition[2] = pMumbleLink->fAvatarPosition[2];

	if (!offline) {
		std::thread request_thread([&]() {
			cpr::Get(cpr::Url{ server + "groups/" + group_code + "/prepare" });
		});
		request_thread.detach();
	}
}

void calculate_groupcode() {
	std::string playersConcat = "";

	for (auto it = group_players.begin(); it != group_players.end(); ++it) {
		playersConcat = playersConcat + (*it);
	}

	CRC32 crc32;
	std::string group_code_new = crc32(playersConcat);
	if (autoPrepare && group_code != group_code_new) {
		timer_prepare();
	}
	group_code = group_code_new;
}

uintptr_t mod_combat(cbtevent* ev, ag* src, ag* dst, char* skillname, uint64_t id, uint64_t revision) {
	if (!ev) {
		if (!src->elite) {
			if (src->name != nullptr && src->name[0] != '\0' && dst->name != nullptr && dst->name[0] != '\0') {
				std::string username(dst->name);
				if (username.at(0) == ':') {
					username.erase(0, 1);
				}

				if (src->prof) {
					std::scoped_lock<std::mutex> guard(groupcode_mutex);
					if (selfAccountName.empty() && dst->self) {
						selfAccountName = username;
					}

					group_players.insert(username);
					calculate_groupcode();
				}
				else {
					if (username != selfAccountName) {
						std::scoped_lock<std::mutex> guard(groupcode_mutex);
						group_players.erase(username);
						calculate_groupcode();
					}
				}
			}
		}
	}
	else {
		if (ev->is_activation) {
			std::chrono::duration<double> duration_dbl = std::chrono::system_clock::now() - start_time;
			double duration = duration_dbl.count();

			if (status == TimerStatus::prepared || (status == TimerStatus::running && duration < 3)) {
				timer_start(3);
			}
		}
	}

	return 0;
}

void timer_reset() {
	status = TimerStatus::stopped;
	start_time = std::chrono::system_clock::now();
	current_time = std::chrono::system_clock::now();

	if (!offline && !outOfDate) {
		std::thread request_thread([&]() {
			cpr::Get(cpr::Url{ server + "groups/" + group_code + "/reset" });
		});
		request_thread.detach();
	}
}

std::chrono::system_clock::time_point parse_time(const std::string& source)
{
	std::chrono::sys_time<std::chrono::microseconds> timePoint;
	std::istringstream(source) >> std::chrono::parse(std::string("%FT%T"), timePoint);
	return timePoint;
}

void sync_timer() {
	std::string groupcode_copy = "";
	
	{
		std::scoped_lock<std::mutex> guard(groupcode_mutex);
		if (group_code == "") {
			calculate_groupcode();
		}
		groupcode_copy = group_code;
	}

	std::regex empty_group("^0+$");
	if (std::regex_match(groupcode_copy, empty_group)) {
		return;
	}

	auto response = cpr::Get(cpr::Url{ server + "groups/" + groupcode_copy });


	if (response.status_code != 200) {
		log_arc("Failed to sync with server");
		return;
	}

	auto data = json::parse(response.text);
	if (data.find("status") != data.end()) {
		if (data["status"] == "running") {
			status = TimerStatus::running;
			start_time = parse_time(data["start_time"]);
		}
		else if (data["status"] == "stopped") {
			if (status != TimerStatus::prepared) {
				status = TimerStatus::stopped;
			}
			current_time = parse_time(data["stop_time"]);
		}
		else if (data["status"] == "resetted") {
			if (status != TimerStatus::prepared) {
				status = TimerStatus::stopped;
			}
			start_time = std::chrono::system_clock::now();
			current_time = std::chrono::system_clock::now();
		}
		else if (data["status"] == "prepared") {
			if (status != TimerStatus::running) {
				status = TimerStatus::prepared;
				start_time = std::chrono::system_clock::now();
				current_time = std::chrono::system_clock::now();
				lastPosition[0] = pMumbleLink->fAvatarPosition[0];
				lastPosition[1] = pMumbleLink->fAvatarPosition[1];
				lastPosition[2] = pMumbleLink->fAvatarPosition[2];
			}
		}
	}
}
