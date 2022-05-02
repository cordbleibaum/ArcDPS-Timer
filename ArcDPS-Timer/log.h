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
	MAP_CHANGE,
	START,
	STOP,
	RESET,
	PREPARE,
	SEGMENT
};

NLOHMANN_JSON_SERIALIZE_ENUM(LogEvent, {
	{LogEvent::INVALID, nullptr},
	{LogEvent::MAP_CHANGE, "entering map"},
	{LogEvent::START, "starting"},
	{LogEvent::STOP, "stopping"},
	{LogEvent::PREPARE, "preparing"},
	{LogEvent::SEGMENT, "segment"},
	{LogEvent::RESET, "resetting"}
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
	Logger(GW2MumbleLink& mumble_link, Settings& settings);
	void map_change(uint32_t map_id);
	void start(std::chrono::system_clock::time_point time);
	void stop(std::chrono::system_clock::time_point time);
	void reset(std::chrono::system_clock::time_point time);
	void prepare(std::chrono::system_clock::time_point time);
	void segment(std::chrono::system_clock::time_point time);
	~Logger();

private:
	GW2MumbleLink& mumble_link;
	Settings& settings;

	uint32_t map_id = 0;
	bool is_instanced = false;
	std::vector<std::tuple< std::chrono::system_clock::time_point, LogEvent>> events;

	std::string logs_directory = "addons/arcdps/arcdps-timer-logs/";

	void save_log();
	void save_log_thread(std::vector<std::tuple< std::chrono::system_clock::time_point, LogEvent>> events_save);
};
