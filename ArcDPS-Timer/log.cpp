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

Logger::Logger(GW2MumbleLink& mumble_link, const Settings& settings)
:	mumble_link(mumble_link),
	settings(settings) {
	if (!std::filesystem::exists(logs_directory)) {
		std::filesystem::create_directory(logs_directory);
	}
}

void Logger::map_change(uint32_t map_id) {
	save_log();
	this->map_id = map_id;
	is_instanced = mumble_link->getMumbleContext()->mapType == MapType::MAPTYPE_INSTANCE;
}

void Logger::start(std::chrono::system_clock::time_point time) {
	start_time = time;
}

void Logger::stop(std::chrono::system_clock::time_point time) {
	end_time = time;
	if (is_instanced) {
		add_log();
	}
}

void Logger::reset(std::chrono::system_clock::time_point time) {
	end_time = time;
	if (is_instanced) {
		add_log();
	}
}

void Logger::segment(int segment_num, std::chrono::system_clock::time_point time) {
	if (is_instanced) {
		segments[segment_num] = time;
	}
}

Logger::~Logger() {
	if (is_instanced) {
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

	std::map<uint32_t, std::string> map_name_overrides = {
		{872, "Mistlock Observatory"},
		{947, "Uncategorized Fractal"},
		{948, "Snowblind Fractal"},
		{949, "Snowblind Fractal"},
		{950, "Urban Battleground Fractal"},
		{951, "Aquatic Ruins Fractal"},
		{952, "Cliffside Fractal"},
		{953, "Underground Facility Fractal"},
		{954, "Volcanic Fractal"},
		{955, "Molten Furnace"},
		{956, "Aetherblade Retreat"},
		{957, "Thaumanova Reactor"},
		{958, "Solid Ocean Fractal"},
		{959, "Molten Furnace"},
		{960, "Aetherblade Retreat"},
		{1164, "Chaos Fractal"},
		{1177, "Nightmare"},
		{1205, "Shattered Observatory"},
		{1267, "Twilight Oasis"},
		{1290, "Deepstone Fractal"},
		{1384, "Sunqua Peak Fractal"}
	};
	
	std::string map_name = "Unknown";
	if (map_name_overrides.find(current_map_id) != map_name_overrides.end()) {
		map_name = map_name_overrides[current_map_id];
	}
	else {
		auto map_response = cpr::Get(
			cpr::Url{ "https://api.guildwars2.com/v2/maps/" + std::to_string(current_map_id) },
			cpr::Timeout{ 8000 }
		);

		if (map_response.status_code == cpr::status::HTTP_OK) {
			json map_response_json = json::parse(map_response.text);
			map_name = map_response_json["name"];
		}
	}

	std::string log_path = logs_directory + map_name + "/";
	if (!std::filesystem::exists(log_path)) {
		std::filesystem::create_directory(log_path);
	}

	json events_json = events_save;
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
