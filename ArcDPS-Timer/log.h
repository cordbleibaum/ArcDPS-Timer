#pragma once

#include <chrono>
#include <cstdint>
#include <vector>
#include <tuple>
#include <string>

#include "mumble_link.h"
#include "settings.h"

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

namespace nlohmann {
	template<typename Clock, typename Duration>
	struct adl_serializer<std::chrono::time_point<Clock, Duration>> {
		static void to_json(json& j, const std::chrono::time_point<Clock, Duration>& tp) {
			j = std::format("{:%FT%T}", std::chrono::floor<std::chrono::milliseconds>(tp));
		}
	};
}

class Logger {
public:
	Logger(GW2MumbleLink& mumble_link, const Settings& settings);
	void map_change(uint32_t map_id);
	void start(std::chrono::system_clock::time_point time);
	void stop(std::chrono::system_clock::time_point time);
	void reset(std::chrono::system_clock::time_point time);
	void segment(int segment_num, std::chrono::system_clock::time_point time);
	~Logger();

private:
	GW2MumbleLink& mumble_link;
	const Settings& settings;

	uint32_t map_id = 0;
	bool is_instanced = false;

	std::chrono::system_clock::time_point start_time;
	std::chrono::system_clock::time_point end_time;
	std::map<int, std::chrono::system_clock::time_point> segments;
	std::vector<std::tuple< std::chrono::system_clock::time_point, LogEvent>> events;

	std::string logs_directory = "addons/arcdps/arcdps-timer-logs/";

	void save_log();
	void save_log_thread(std::vector<std::tuple< std::chrono::system_clock::time_point, LogEvent>> events_save, uint32_t current_map_id);
	void add_log();
};
