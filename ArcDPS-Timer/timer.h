#pragma once

#include "arcdps.h"
#include "settings.h"
#include "mumble_link.h"
#include "grouptracker.h"
#include "lang.h"

#include <chrono>
#include <set>
#include <thread>
#include <mutex>
#include <array>
#include <vector>

#include "json.hpp"
using json = nlohmann::json;

#include <cpr/cpr.h>

enum class TimerStatus { stopped, prepared, running };
enum class ServerStatus { online, offline, outofdate };

struct TimeSegment {
	bool is_set = false;
	std::chrono::system_clock::time_point start = std::chrono::system_clock::now();
	std::chrono::system_clock::time_point end = std::chrono::system_clock::now();
	std::chrono::system_clock::duration shortest_duration = std::chrono::system_clock::duration::zero();
	std::chrono::system_clock::duration shortest_time = std::chrono::system_clock::duration::zero();
};

class Timer {
public:
	Timer(Settings& settings, GW2MumbleLink& mumble_link, GroupTracker& group_tracker, Translation& translation);
	void start(std::chrono::system_clock::time_point time = std::chrono::system_clock::now());
	void stop(std::chrono::system_clock::time_point time = std::chrono::system_clock::now());
	void reset();
	void prepare();
	void segment();
	void clear_segments();

	void mod_combat(cbtevent* ev, ag* src, ag* dst, const char* skillname, uint64_t id);
	void mod_imgui();

	double clock_offset = 0;
private:
	Settings& settings;
	GW2MumbleLink& mumble_link;
	GroupTracker& group_tracker;
	Translation& translation;

	TimerStatus status;
	std::chrono::system_clock::time_point start_time;
	std::chrono::system_clock::time_point current_time;
	std::chrono::system_clock::time_point update_time;
	std::chrono::system_clock::time_point log_start_time;
	std::vector<TimeSegment> segments;

	std::array<float, 3> last_position;
	uint32_t lastMapID = 0;
	std::set<uintptr_t> log_agents;
	std::chrono::system_clock::time_point last_damage_ticks;
	std::string map_code;
	std::string last_id = "";

	std::mutex mapcode_mutex;
	std::mutex logagents_mutex;
	ServerStatus serverStatus = ServerStatus::online;
	bool isInstanced = false;

	void sync();
	void check_serverstatus();
	std::string format_time(std::chrono::system_clock::time_point time);
	void post_serverapi(std::string method, json payload);
	std::string get_id();
};
