#pragma once

#include <chrono>
#include <cstdint>
#include <vector>
#include <tuple>
#include <string>

#include "chrono-json.h"

#include "mumble_link.h"
#include "settings.h"
#include "maptracker.h"

enum class LogEvent {
	INVALID,
	START,
	END,
	SEGMENT
};

NLOHMANN_JSON_SERIALIZE_ENUM(LogEvent, {
	{LogEvent::INVALID, nullptr},
	{LogEvent::START, "starting run"},
	{LogEvent::END, "ending run"},
	{LogEvent::SEGMENT, "segment"}
})

class Logger {
public:
	Logger(GW2MumbleLink& mumble_link, const Settings& settings, MapTracker& map_tracker);
	void map_change(uint32_t map_id);
	void start(std::chrono::system_clock::time_point time);
	void stop(std::chrono::system_clock::time_point time);
	void reset(std::chrono::system_clock::time_point time);
	void segment(int segment_num, std::chrono::system_clock::time_point time);
	void mod_release();

private:
	GW2MumbleLink& mumble_link;
	const Settings& settings;
	MapTracker& map_tracker;

	uint32_t map_id = 0;
	bool is_enabled = false;

	std::chrono::system_clock::time_point start_time;
	std::chrono::system_clock::time_point end_time;
	std::map<int, std::chrono::system_clock::time_point> segments;
	std::vector<std::tuple< std::chrono::system_clock::time_point, LogEvent>> events;

	std::string logs_directory = "addons/arcdps/arcdps-timer-logs/";

	void save_log();
	void save_log_thread(std::vector<std::tuple< std::chrono::system_clock::time_point, LogEvent>> events_save, uint32_t current_map_id);
	void add_log();
};
