#pragma once

#include "arcdps.h"
#include "settings.h"
#include "mumble_link.h"
#include "grouptracker.h"

#include <chrono>
#include <set>
#include <thread>
#include <mutex>
#include <vector>

#include "json.hpp"
using json = nlohmann::json;

#include <cpr/cpr.h>

enum class TimerStatus { stopped, prepared, running };
enum class ServerStatus { online, offline, outofdate };

struct TimeSegment {
	bool is_set = false;
	bool is_used = false;
	std::chrono::system_clock::time_point start = std::chrono::system_clock::now();
	std::chrono::system_clock::time_point end = std::chrono::system_clock::now();
	std::chrono::system_clock::duration shortest_duration = std::chrono::system_clock::duration::zero();
	std::chrono::system_clock::duration shortest_time = std::chrono::system_clock::duration::zero();
};

class Timer {
public:
	Timer(Settings& settings, GW2MumbleLink& mumble_link, GroupTracker& group_tracker);
	void start(std::chrono::system_clock::time_point time = std::chrono::system_clock::now());
	void stop(std::chrono::system_clock::time_point time = std::chrono::system_clock::now());
	void reset();
	void prepare();
	void segment();
	void clear_segments();

	void mod_combat(cbtevent* ev, ag* src, ag* dst, const char* skillname, uint64_t id, uint64_t revision);
	void mod_imgui();

	double clock_offset = 0;
private:
	Settings& settings;
	GW2MumbleLink& mumble_link;
	GroupTracker& group_tracker;

	TimerStatus status;
	std::chrono::system_clock::time_point start_time;
	std::chrono::system_clock::time_point current_time;
	std::chrono::system_clock::time_point update_time;
	std::chrono::system_clock::time_point log_start_time;
	std::vector<TimeSegment> segments;

	float lastPosition[3];
	uint32_t lastMapID = 0;
	std::set<uintptr_t> log_agents;
	std::chrono::system_clock::time_point last_damage_ticks;
	std::string map_code;
	std::string last_id = "";

	mutable std::mutex mapcode_mutex;
	mutable std::mutex logagents_mutex;
	ServerStatus serverStatus = ServerStatus::online;
	bool isInstanced = false;

	void sync();
	std::string format_time(std::chrono::system_clock::time_point time);
	cpr::Response post_serverapi(std::string method, const json& payload);
	std::string get_id();

	template<class F> void network_thread(F&& f) {
		if (!settings.is_offline_mode && serverStatus == ServerStatus::online) {
			std::thread thread(std::forward<F>(f));
			thread.detach();
		}
	}
};

arcdps_exports* mod_init();
uintptr_t mod_release();
uintptr_t mod_options();
uintptr_t mod_windows(const char* windowname);
uintptr_t mod_imgui(uint32_t not_charsel_or_loading);
uintptr_t mod_combat(cbtevent* ev, ag* src, ag* dst, const char* skillname, uint64_t id, uint64_t revision);
uintptr_t mod_wnd(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
