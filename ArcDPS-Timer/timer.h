#pragma once

#include "arcdps.h"
#include "settings.h"
#include "mumble_link.h"

#include <chrono>
#include <set>
#include <thread>
#include <mutex>
#include <vector>

#include "json.hpp"
using json = nlohmann::json;

enum class TimerStatus { stopped, prepared, running };

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
	Timer(Settings& settings, GW2MumbleLink& mumble_link);
	void start();
	void start(uint64_t time);
	void stop();
	void stop(uint64_t time);
	void reset();
	void prepare();
	void sync();
	void segment();
	void clear_segments();

	void mod_combat(cbtevent* ev, ag* src, ag* dst, const char* skillname, uint64_t id, uint64_t revision);
	void mod_imgui();

	double clock_offset = 0;
private:
	Settings& settings;
	GW2MumbleLink& mumble_link;

	TimerStatus status;
	std::chrono::system_clock::time_point start_time;
	std::chrono::system_clock::time_point current_time;
	std::chrono::system_clock::time_point update_time;
	std::chrono::system_clock::time_point log_start_time;
	std::vector<TimeSegment> segments;
	double clockOffset = 0;

	float lastPosition[3];
	uint32_t lastMapID = 0;
	std::set<uintptr_t> log_agents;
	unsigned long long last_damage_ticks;
	std::string map_code;
	std::string selfAccountName;
	std::set<std::string> group_players;
	std::string group_code;

	mutable std::mutex mapcode_mutex;
	mutable std::mutex logagents_mutex;
	mutable std::mutex groupcode_mutex;
	bool outOfDate = false;
	bool isInstanced = false;

	void request_stop();
	void request_start();
	void sync_thread();
	void calculate_groupcode();
	std::string format_time(std::chrono::system_clock::time_point time);
	void post_serverapi(std::string method, const json& payload);
	std::string get_id();

	template<class F> void network_thread(F&& f) {
		if (!settings.is_offline_mode && !outOfDate) {
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
