#include "log.h"

#include <cpr/cpr.h>
#include <nlohmann/json.hpp>
#include <thread>
#include <filesystem>
#include <fstream>
#include <ranges>
#include <map>

#include "arcdps.h"
#include "util.h"

using json = nlohmann::json;

Logger::Logger(GW2MumbleLink& mumble_link, const Settings& settings, MapTracker& map_tracker)
:	mumble_link(mumble_link),
	settings(settings),
	map_tracker(map_tracker) {
	if (!std::filesystem::exists(logs_directory)) {
		std::filesystem::create_directory(logs_directory);
	}
}

void Logger::map_change(uint32_t map_id) {
	save_log();
	this->map_id = map_id;
	is_enabled = settings.is_enabled();
}

void Logger::start(std::chrono::system_clock::time_point time) {
	start_time = time;
}

void Logger::stop(std::chrono::system_clock::time_point time) {
	end_time = time;
	if (is_enabled) {
		add_log();
	}
}

void Logger::reset(std::chrono::system_clock::time_point time) {
	end_time = time;
	if (is_enabled) {
		add_log();
	}
}

void Logger::segment(int segment_num, std::chrono::system_clock::time_point time) {
	if (is_enabled) {
		segments[segment_num] = time;
	}
}

void Logger::mod_release() {
	if (is_enabled) {
		std::vector<std::tuple< std::chrono::system_clock::time_point, LogEvent>> current_events(events);
		save_log_thread(current_events, map_id);
	}
}

void Logger::save_log() {
	std::vector<std::tuple< std::chrono::system_clock::time_point, LogEvent>> current_events(events);
	uint32_t current_map_id = map_id;
	defer([&, current_events, current_map_id]() {
		save_log_thread(current_events, current_map_id);
	});
	events.clear();
}

void Logger::save_log_thread(std::vector<std::tuple< std::chrono::system_clock::time_point, LogEvent>> events_save, uint32_t current_map_id) {
	if (events_save.size() < 1 || !settings.save_logs) {
		return;
	}

	log_debug("timer: saving log");
	
	std::string map_name = map_tracker.get_map_name(current_map_id);

	const std::string log_path = logs_directory + map_name + "/";
	if (!std::filesystem::exists(log_path)) {
		std::filesystem::create_directory(log_path);
	}

	const json events_json = events_save;
	std::string name = std::format("{:%FT%H-%M-%S}", std::chrono::floor<std::chrono::milliseconds>(std::chrono::system_clock::now()));
	std::ofstream o(log_path + name + ".json");
	o << std::setw(4) << events_json << std::endl;
}

void Logger::add_log() {
	events.emplace_back(start_time, LogEvent::START);

	if (segments.size() > 0) {
		int max_segment = 0;
		for (int segment : std::views::keys(segments)) {
			if (segment > max_segment) {
				max_segment = segment;
			}
		}

		for (int i = 0; i <= max_segment; ++i) {
			events.emplace_back(segments[i], LogEvent::SEGMENT);
		}
	}
	segments.clear();

	events.emplace_back(end_time, LogEvent::END);
}
