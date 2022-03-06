#include "timer.h"

#include <string>
#include <chrono>
#include <cmath>
#include <format>
#include <regex>

#include <cpr/cpr.h>

#include "imgui_stdlib.h"
#include "json.hpp"
#include "mumble_link.h"
#include "hash-library/crc32.h"

#include "ntp.h"
#include "settings.h"

using json = nlohmann::json;
using namespace std::chrono_literals;

enum class TimerStatus { stopped, prepared, running };

arcdps_exports arc_exports;

Settings settings;

std::string config_file = "addons/arcdps/timer.json";
constexpr int server_version = 6;
constexpr int settings_version = 7;

TimerStatus status;
std::chrono::system_clock::time_point start_time;
std::chrono::system_clock::time_point current_time;
std::chrono::system_clock::time_point update_time;

HANDLE hMumbleLink;
LinkedMem *pMumbleLink;
float lastPosition[3];
uint32_t lastMapID = 0;

std::string map_code;
std::mutex mapcode_mutex;

std::chrono::system_clock::time_point last_update;

bool outOfDate = false;
bool isInstanced = false;

double clockOffset = 0;

arcdps_exports* mod_init() {
	memset(&arc_exports, 0, sizeof(arcdps_exports));
	arc_exports.sig = 0x1A0;
	arc_exports.imguivers = IMGUI_VERSION_NUM;
	arc_exports.size = sizeof(arcdps_exports);
	arc_exports.out_name = "Timer";
	arc_exports.out_build = "0.5";
	arc_exports.options_end = mod_options;
	arc_exports.options_windows = mod_windows;
	arc_exports.imgui = mod_imgui;
	arc_exports.combat = mod_combat;

	settings = Settings(config_file, settings_version);

	start_time = std::chrono::system_clock::now();
	current_time = std::chrono::system_clock::now();
	update_time = std::chrono::system_clock::now();
	status = TimerStatus::stopped;

	hMumbleLink = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, sizeof(LinkedMem), L"MumbleLink");
	if (hMumbleLink == NULL) {
		log("timer: could not create mumble link file mapping object\n");
		arc_exports.sig = 0;
	}
	else {
		pMumbleLink = (LinkedMem*) MapViewOfFile(hMumbleLink, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(LinkedMem));
		if (pMumbleLink == NULL) {
			log("timer: failed to open mumble link file\n");
			arc_exports.sig = 0;
		}
	}

	auto response = cpr::Get(cpr::Url{ settings.server_url + "version" });
	if (response.status_code != 200) {
		log("timer: failed to connect to timer api, enabling offline mode\n");
		settings.is_offline_mode = true;
	}
	else {
		auto data = json::parse(response.text);
		if (data["major"] != server_version) {
			log("timer: out of date version, going offline mode\n");
			outOfDate = true;
		}
	}

	clockOffset = 0;
	std::thread ntp_thread([&]() {
		NTPClient ntp("pool.ntp.org");
		clockOffset = ntp.request_time_delta();
		log_arc("timer: clock offset: " + std::to_string(clockOffset));
	});
	ntp_thread.detach();

	log_arc("timer: done mod_init");
	return &arc_exports;
}

uintptr_t mod_release() {
	settings.save();

	UnmapViewOfFile(pMumbleLink);
	CloseHandle(hMumbleLink);

	return 0;
}

uintptr_t mod_options() {
	settings.show_options();
	return 0;
}

uintptr_t mod_windows(const char* windowname) {
	if (!windowname) {
		ImGui::Checkbox("Timer", &settings.show_timer);
	}
	return 0;
}

bool checkDelta(float a, float b, float delta) {
	return std::abs(a - b) > delta;
}

uintptr_t mod_imgui(uint32_t not_charsel_or_loading) {
	if (!not_charsel_or_loading) return 0;

	if (lastMapID != ((MumbleContext*)pMumbleLink->context)->mapId) {
		std::scoped_lock<std::mutex> guard(mapcode_mutex);

		lastMapID = ((MumbleContext*)pMumbleLink->context)->mapId;
		isInstanced = ((MumbleContext*)pMumbleLink->context)->mapType == MapType::MAPTYPE_INSTANCE;

		CRC32 crc32;
		map_code = crc32(((MumbleContext*)pMumbleLink->context)->serverAddress, sizeof(sockaddr_in));
		update_time = std::chrono::sys_days{ 1970y / 1 / 1 };

		bool doAutoPrepare = settings.auto_prepare;
		doAutoPrepare &= isInstanced | !settings.disable_outside_instances;

		if (doAutoPrepare) {
			log_debug("timer: preparing on map change");
			timer_prepare();
		}
	}

	if (settings.disable_outside_instances && !isInstanced) {
		return 0;
	}


	if (status == TimerStatus::prepared) {
		if (checkDelta(lastPosition[0], pMumbleLink->fAvatarPosition[0], 0.1f) ||
			checkDelta(lastPosition[1], pMumbleLink->fAvatarPosition[1], 0.1f) ||
			checkDelta(lastPosition[2], pMumbleLink->fAvatarPosition[2], 0.1f)) {
			log_debug("timer: starting on movement");
			timer_start();
		}
	}

	if (status == TimerStatus::running) {
		current_time = std::chrono::system_clock::now();
	}

	if (std::chrono::duration_cast<std::chrono::duration<float>>(std::chrono::system_clock::now() - last_update).count() > settings.sync_interval) {
		last_update = std::chrono::system_clock::now();
		if (!settings.is_offline_mode && !outOfDate) {
			std::thread sync_thread(sync_timer);
			sync_thread.detach();
		}
	}

	if (settings.show_timer) {
		ImGui::Begin("Timer", &settings.show_timer, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoDecoration);

		ImGui::Dummy(ImVec2(0.0f, 3.0f));
		
		auto duration = std::chrono::round<std::chrono::milliseconds>(current_time - start_time);

		std::string time_string = "";
		try {
			time_string = std::format(settings.time_formatter, duration);
		}
		catch(const std::exception& e) {
			time_string = "INVALID";
		}

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

		if (!settings.hide_buttons) {
			if (ImGui::Button("Prepare", ImVec2(190, ImGui::GetFontSize() * 1.5f))) {
				log_debug("timer: preparing manually");
				timer_prepare();
			}

			if (ImGui::Button("Start", ImVec2(60, ImGui::GetFontSize() * 1.5f))) {
				log_debug("timer: starting manually");
				timer_start();
			}

			ImGui::SameLine(0, 5);

			if (ImGui::Button("Stop", ImVec2(60, ImGui::GetFontSize() * 1.5f))) {
				log_debug("timer: stopping manually");
				timer_stop(0);
			}

			ImGui::SameLine(0, 5);

			if (ImGui::Button("Reset", ImVec2(60, ImGui::GetFontSize() * 1.5f))) {
				log_debug("timer: resetting manually");
				timer_reset();
			}
		}
		else {
			ImGui::Dummy(ImVec2(160, 0));
		}

		if (outOfDate) {
			ImGui::TextColored(ImVec4(1, 0, 0, 1), "OFFLINE, ADDON OUT OF DATE");
		}

		ImGui::End();
	}

	return 0;
}

std::string format_time(std::chrono::system_clock::time_point time) {
	return std::format(
		"{:%FT%T}",
		std::chrono::floor<std::chrono::milliseconds>(time + std::chrono::milliseconds((int)(clockOffset * 1000.0)))
	);
}

void request_start() {
	if (!settings.is_offline_mode && !outOfDate) {
		std::thread request_thread([&]() {
			json request;
			request["time"] = format_time(start_time);
			request["update_time"] = format_time(update_time);

			cpr::Post(
				cpr::Url{ settings.server_url + "groups/" + map_code + "/stop" },
				cpr::Body{ request.dump() },
				cpr::Header{ {"Content-Type", "application/json"} }
			);
			});
		request_thread.detach();
	}
}

void timer_start() {
	start_time = std::chrono::system_clock::now();
	update_time = std::chrono::system_clock::now();
	current_time = std::chrono::system_clock::now();
	status = TimerStatus::running;

	request_start();
}

void timer_start(uint64_t time) {
	status = TimerStatus::running;
	std::chrono::milliseconds time_ms(time);
	start_time = std::chrono::time_point<std::chrono::system_clock>(time_ms);
	update_time = std::chrono::system_clock::now();
	current_time = std::chrono::system_clock::now();

	request_start();
}

void timer_stop(int delta) {
	if (status != TimerStatus::stopped) {
		current_time = std::chrono::system_clock::now() - std::chrono::seconds(delta);
		status = TimerStatus::stopped;
		update_time = std::chrono::system_clock::now();

		if (!settings.is_offline_mode && !outOfDate) {
			std::thread request_thread([&]() {
				json request;
				request["time"] = format_time(current_time);
				request["update_time"] = format_time(update_time);

				cpr::Post(
					cpr::Url{ settings.server_url + "groups/" + map_code + "/stop" },
					cpr::Body{ request.dump() },
					cpr::Header{ {"Content-Type", "application/json"} }
				);
				});
			request_thread.detach();
		}
	}
}

void timer_prepare() {
	status = TimerStatus::prepared;
	start_time = std::chrono::system_clock::now();
	current_time = std::chrono::system_clock::now();
	lastPosition[0] = pMumbleLink->fAvatarPosition[0];
	lastPosition[1] = pMumbleLink->fAvatarPosition[1];
	lastPosition[2] = pMumbleLink->fAvatarPosition[2];
	update_time = std::chrono::system_clock::now();

	if (!settings.is_offline_mode && !outOfDate) {
		std::thread request_thread([&]() {
			json request;
			request["update_time"] = format_time(update_time);

			cpr::Post(
				cpr::Url{ settings.server_url + "groups/" + map_code + "/prepare" },
				cpr::Body{ request.dump() },
				cpr::Header{ {"Content-Type", "application/json"} }
			);
		});
		request_thread.detach();
	}
}

uintptr_t mod_combat(cbtevent* ev, ag* src, ag* dst, char* skillname, uint64_t id, uint64_t revision) {
	if (ev && ev->is_activation) {
		std::chrono::duration<double> duration_dbl = std::chrono::system_clock::now() - start_time;
		double duration = duration_dbl.count();

		if (status == TimerStatus::prepared || (status == TimerStatus::running && duration < 3)) {
			log_debug("timer: starting on skill");
			auto ticks_now = timeGetTime();
			auto ticks_diff = ticks_now - ev->time;
			auto now_ms = std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::system_clock::now());
			auto skill_time = now_ms.time_since_epoch().count() - ticks_diff;
			timer_start(skill_time);
		}
	}

	return 0;
}

void timer_reset() {
	status = TimerStatus::stopped;
	start_time = std::chrono::system_clock::now();
	current_time = std::chrono::system_clock::now();
	update_time = std::chrono::system_clock::now();

	if (!settings.is_offline_mode && !outOfDate) {
		std::thread request_thread([&]() {
			json request;
			request["update_time"] = format_time(update_time);

			cpr::Post(
				cpr::Url{ settings.server_url + "groups/" + map_code + "/reset" },
				cpr::Body{ request.dump() },
				cpr::Header{ {"Content-Type", "application/json"} }
			);
		});
		request_thread.detach();
	}
}

std::chrono::system_clock::time_point parse_time(const std::string& source) {
	std::chrono::sys_time<std::chrono::microseconds> timePoint;
	std::istringstream(source) >> std::chrono::parse(std::string("%FT%T"), timePoint);
	return timePoint;
}

void sync_timer() {
	std::string mapcode_copy = "";
	
	{
		std::scoped_lock<std::mutex> guard(mapcode_mutex);
		if (map_code == "") {
			return;
		}
		mapcode_copy = map_code;
	}

	std::regex empty_group("^0+$");
	if (std::regex_match(mapcode_copy, empty_group)) {
		return;
	}

	auto response = cpr::Get(cpr::Url{ settings.server_url + "groups/" + mapcode_copy });

	if (response.status_code != 200) {
		log("timer: failed to sync with server");
		return;
	}

	auto data = json::parse(response.text);

	bool isNewer = true;
	if (data.find("update_time") != data.end()) {
		std::chrono::system_clock::time_point new_update_time = parse_time(data["update_time"]) - std::chrono::milliseconds((int)(clockOffset * 1000.0));
		isNewer = new_update_time > update_time;
	}

	if (isNewer && data.find("status") != data.end()) {
		if (data["status"] == "running") {
			log_debug("timer: starting on server");
			status = TimerStatus::running;
			start_time = parse_time(data["start_time"]) - std::chrono::milliseconds((int)(clockOffset * 1000.0));
		}
		else if (data["status"] == "stopped") {
			log_debug("timer: stopping on server");
			status = TimerStatus::stopped;
			current_time = parse_time(data["stop_time"]) - std::chrono::milliseconds((int)(clockOffset*1000.0));
		}
		else if (data["status"] == "resetted") {
			log_debug("timer: resetting on server");
			status = TimerStatus::stopped;
			start_time = std::chrono::system_clock::now();
			current_time = std::chrono::system_clock::now();
		}
		else if (data["status"] == "prepared") {
			log_debug("timer: preparing on server");
			status = TimerStatus::prepared;
			start_time = std::chrono::system_clock::now();
			current_time = std::chrono::system_clock::now();
			lastPosition[0] = pMumbleLink->fAvatarPosition[0];
			lastPosition[1] = pMumbleLink->fAvatarPosition[1];
			lastPosition[2] = pMumbleLink->fAvatarPosition[2];
		}
	}
}
