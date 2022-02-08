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
#include "mumble_link.h"
#include "hash-library/crc32.h"
#include "ntp.h"

using json = nlohmann::json;

enum class TimerStatus { stopped, prepared, running };

arcdps_exports arc_exports;
void* arclog;
void* filelog;

bool showTimer = false;
bool windowBorder = true;

std::string config_file = "addons/arcdps/timer.json";
constexpr int version_major = 5;

TimerStatus status;
std::chrono::system_clock::time_point start_time;
std::chrono::system_clock::time_point current_time;

HANDLE hMumbleLink;
LinkedMem *pMumbleLink;
float lastPosition[3];
uint32_t lastMapID = 0;

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
bool autoPrepareOnlyInstancedContent;
bool autoPrepareOnGroupChange;
bool hideOutsideInstances;

bool isInstanced = false;

double clockOffset = 0;

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

extern "C" __declspec(dllexport) void* get_init_addr(char *arcversion, ImGuiContext *imguictx, void *id3dptr, HANDLE arcdll, void *mallocfn, void *freefn, uint32_t d3dversion) {
	arclog = (void*)GetProcAddress((HMODULE)arcdll, "e8");
	filelog = (void*)GetProcAddress((HMODULE)arcdll, "e3");
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
	arc_exports.out_build = "0.3";
	arc_exports.options_end = mod_options;
	arc_exports.options_windows = mod_windows;
	arc_exports.imgui = mod_imgui;
	arc_exports.combat = mod_combat;

	json config;
	config["filler"] = "empty";
	if (std::filesystem::exists(config_file)) {
		std::ifstream input(config_file);
		input >> config;
		if (config["version"] < version_major) {
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
	autoPrepareOnlyInstancedContent = config.value("autoPrepareOnlyInstancedContent", true);
	autoPrepareOnGroupChange = config.value("autoPrepareOnGroupChange", false);
	hideOutsideInstances = config.value("hideOutsideInstances", true);

	start_time = std::chrono::system_clock::now();
	current_time = std::chrono::system_clock::now();
	status = TimerStatus::stopped;

	hMumbleLink = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, sizeof(LinkedMem), L"MumbleLink");
	if (hMumbleLink == NULL) {
		log("Could not create mumble link file mapping object\n");
		arc_exports.sig = 0;
	}
	else {
		pMumbleLink = (LinkedMem*) MapViewOfFile(hMumbleLink, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(LinkedMem));
		if (pMumbleLink == NULL) {
			log("Failed to open mumble link file\n");
			arc_exports.sig = 0;
		}
	}

	auto response = cpr::Get(cpr::Url{ server + "version" });
	auto data = json::parse(response.text);
	if (data["major"] != version_major) {
		log("Out of date version, going offline mode\n");
		outOfDate = true;
	}

	NTPClient ntp("pool.ntp.org");
	clockOffset = ntp.request_time_delta();
	log_arc("Clock offset: " + std::to_string(clockOffset));

	log_arc("timer: done mod_init");
	return &arc_exports;
}

uintptr_t mod_release() {
	json config;
	config["showTimer"] = showTimer;
	config["windowBorder"] = windowBorder;
	config["server"] = server;
	config["version"] = version_major;
	config["sync_interval"] = sync_interval;
	config["groupWidePrepare"] = groupWidePrepare;
	config["autoPrepare"] = autoPrepare;
	config["offline"] = offline;
	config["autoPrepareOnlyInstancedContent"] = autoPrepareOnlyInstancedContent;
	config["autoPrepareOnGroupChange"] = autoPrepareOnGroupChange;
	config["hideOutsideInstances"] = hideOutsideInstances;
	std::ofstream o(config_file);
	o << std::setw(4) << config << std::endl;

	UnmapViewOfFile(pMumbleLink);
	CloseHandle(hMumbleLink);

	return 0;
}

uintptr_t mod_options() {
	ImGui::InputText("Server", &server);
	ImGui::InputInt("Sync Interval", &sync_interval);
	ImGui::Checkbox("Offline Mode", &offline);
	ImGui::Separator();
	ImGui::Checkbox("Window Border", &windowBorder);
	ImGui::Checkbox("Hide outside Instanced Content", &hideOutsideInstances);
	ImGui::Separator();
	ImGui::Checkbox("Auto Prepare", &autoPrepare);
	ImGui::Checkbox("Groupwide Prepare", &groupWidePrepare);
	ImGui::Checkbox("Auto Prepare only in Instanced Content", &autoPrepareOnlyInstancedContent);
	ImGui::Checkbox("Auto Prepare on Group Change", &autoPrepareOnGroupChange);
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
			log_arc("timer: starting on movement");
			timer_start(0);
		}
	}
		
	if (lastMapID != ((MumbleContext*)pMumbleLink->context)->mapId) {
		lastMapID = ((MumbleContext*)pMumbleLink->context)->mapId;

		if (hideOutsideInstances || autoPrepareOnlyInstancedContent) {
			auto mapRequest = cpr::Get(cpr::Url{ "https://api.guildwars2.com/v2/maps/" + std::to_string(lastMapID) });
			auto mapData = json::parse(mapRequest.text);
			isInstanced = mapData["type"] == "Instance";
		}
		else {
			isInstanced = false;
		}

		bool doAutoPrepare = autoPrepare;
		if (autoPrepareOnlyInstancedContent) {
			doAutoPrepare &= isInstanced;
		}

		if (doAutoPrepare && status == TimerStatus::stopped) {
			log_arc("timer: preparing on map change");
			timer_prepare();
		}
	}

	if (std::chrono::duration_cast<std::chrono::duration<float>>(std::chrono::system_clock::now() - last_update).count() > sync_interval) {
		last_update = std::chrono::system_clock::now();
		if (!offline && !outOfDate) {
			std::thread sync_thread(sync_timer);
			sync_thread.detach();
		}
	}

	if (hideOutsideInstances && !isInstanced) {
		return 0;
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

		ImGui::SetCursorPosX(ImGui::GetStyle().WindowPadding.x + 3);
		switch (status) {
		case TimerStatus::stopped:
			ImGui::TextColored(ImVec4(1, 0.2f, 0.2f, 1), "S");
			break;
		case TimerStatus::prepared:
			ImGui::TextColored(ImVec4(1, 1, 0.2f, 1), "P");
			break;
		case TimerStatus::running:
			ImGui::TextColored(ImVec4(0.2f, 1, 0.2f, 1), "R");
			break;
		}

		auto windowWidth = ImGui::GetWindowSize().x;
		auto textWidth = ImGui::CalcTextSize(time_string.c_str()).x;
		ImGui::SameLine(0, 0);
		ImGui::SetCursorPosX((windowWidth - textWidth) * 0.5f);
		ImGui::Text(time_string.c_str());

		ImGui::Dummy(ImVec2(0.0f, 3.0f));

		if (ImGui::Button("Prepare", ImVec2(190, ImGui::GetFontSize()*1.5f))) {
			log_arc("timer: preparing manually");
			timer_prepare();
		}

		if (ImGui::Button("Start", ImVec2(60, ImGui::GetFontSize() * 1.5f))) {
			log_arc("timer: starting manually");
			timer_start(0);
		}
		
		ImGui::SameLine(0, 5);
		
		if (ImGui::Button("Stop", ImVec2(60, ImGui::GetFontSize() * 1.5f))) {
			log_arc("timer: stopping manually");
			timer_stop();
		}

		ImGui::SameLine(0, 5);

		if (ImGui::Button("Reset", ImVec2(60, ImGui::GetFontSize() * 1.5f))) {
			log_arc("timer: resetting manually");
			timer_reset();
		}

		if (outOfDate) {
			ImGui::TextColored(ImVec4(1, 0, 0, 1), "OFFLINE, ADDON OUT OF DATE");
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
			request["time"] = std::format(
				"{:%FT%T}", 
				std::chrono::floor<std::chrono::milliseconds>(start_time + std::chrono::milliseconds((int)(clockOffset * 1000.0)))
			);
			
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
			request["time"] = std::format(
				"{:%FT%T}", 
				std::chrono::floor<std::chrono::milliseconds>(current_time + std::chrono::milliseconds((int)(clockOffset * 1000.0)))
			);

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

	if (!offline && !outOfDate) {
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
	if (autoPrepare && autoPrepareOnGroupChange && group_code != group_code_new) {
		log_arc("timer: preparing on group change");
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
				log_arc("timer: starting on skill");
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
			log_arc("timer: starting on server");
			status = TimerStatus::running;
			start_time = parse_time(data["start_time"]) - std::chrono::milliseconds((int)(clockOffset * 1000.0));;
		}
		else if (data["status"] == "stopped") {
			log_arc("timer: stopping on server");
			if (status != TimerStatus::prepared || groupWidePrepare) {
				status = TimerStatus::stopped;
				current_time = parse_time(data["stop_time"]) - std::chrono::milliseconds((int)(clockOffset*1000.0));
			}
		}
		else if (data["status"] == "resetted") {
			log_arc("timer: resetting on server");
			if (status != TimerStatus::prepared || groupWidePrepare) {
				status = TimerStatus::stopped;
			}
			start_time = std::chrono::system_clock::now();
			current_time = std::chrono::system_clock::now();
		}
		else if (data["status"] == "prepared") {
			if (status != TimerStatus::running) {
				log_arc("timer: preparing on server");
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
