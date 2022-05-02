#include "log.h"

#include <cpr/cpr.h>
#include <nlohmann/json.hpp>
#include <thread>
#include <filesystem>
#include <fstream>

#include "arcdps.h"
#include "util.h"

using json = nlohmann::json;

Logger::Logger(GW2MumbleLink& mumble_link, Settings& settings)
:	mumble_link(mumble_link),
	settings(settings) {
	if (!std::filesystem::exists(logs_directory)) {
		std::filesystem::create_directory(logs_directory);
	}
}

void Logger::map_change(uint32_t map_id) {
	this->map_id = map_id;
	is_instanced = mumble_link->getMumbleContext()->mapType == MapType::MAPTYPE_INSTANCE;
	if (is_instanced) {
		log_debug("timer: log instanced");
	}
	else {
		log_debug("timer: log not instanced");
	}
	events.clear();
}

void Logger::start(std::chrono::system_clock::time_point time) {
	if (is_instanced) {
		events.push_back(std::make_tuple(time, LogEvent::START));
	}
}

void Logger::stop(std::chrono::system_clock::time_point time) {
	if (is_instanced) {
		log_debug("timer: log stop event");
		events.push_back(std::make_tuple(time, LogEvent::STOP));
		save_log();
	}
}

void Logger::reset(std::chrono::system_clock::time_point time) {
	if (is_instanced) {
		events.push_back(std::make_tuple(time, LogEvent::RESET));
		save_log();
	}
}

void Logger::prepare(std::chrono::system_clock::time_point time) {
	if (is_instanced) {
		events.push_back(std::make_tuple(time, LogEvent::PREPARE));
	}
}

void Logger::segment(std::chrono::system_clock::time_point time) {
	if (is_instanced) {
		events.push_back(std::make_tuple(time, LogEvent::SEGMENT));
	}
}

Logger::~Logger() {
	if (is_instanced) {
		save_log_thread(events);
	}
}

void Logger::save_log() {
	std::vector<std::tuple< std::chrono::system_clock::time_point, LogEvent>> current_events(events);
	defer([&, current_events]() {
		save_log_thread(current_events);
	});
}

void Logger::save_log_thread(std::vector<std::tuple< std::chrono::system_clock::time_point, LogEvent>> events_save) {
	if (events.size() < 1) {
		return;
	}

	log_debug("timer: saving log");

	auto map_response = cpr::Get(
		cpr::Url{ "https://api.guildwars2.com/v2/maps/" + std::to_string(map_id) },
		cpr::Timeout{ 5000 }
	);

	std::string map_name = "Unknown";
	if (map_response.status_code == cpr::status::HTTP_OK) {
		json map_response_json = json::parse(map_response.text);
		map_name = map_response_json["name"];
	}

	std::string log_path = logs_directory + map_name + "/";
	if (!std::filesystem::exists(log_path)) {
		std::filesystem::create_directory(log_path);
	}

	json events_json = events_save;
	std::string name = std::format("{:%FT%H-%M-%S}", std::chrono::floor<std::chrono::milliseconds>(std::chrono::system_clock::now()));
	std::ofstream o(log_path + name + ".json");
	log_debug(log_path + name + ".json");
	o << std::setw(4) << events_json << std::endl;
}
