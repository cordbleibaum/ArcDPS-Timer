#pragma once

#include <chrono>
#include <vector>
#include <string>
#include <mutex>
#include <variant>
#include <optional>
#include <shared_mutex>

#include <boost/signals2.hpp>
#include <nlohmann/json.hpp>

#include "uuid-json.h"
#include "chrono-json.h"

#include "api.h"

enum class EventType {
	start,
	stop,
	reset,
	prepare,
	segment,
	segment_clear,
	map_change,
	history_clear
};

NLOHMANN_JSON_SERIALIZE_ENUM(EventType, {
	{EventType::start, "start"},
	{EventType::stop, "stop"},
	{EventType::reset, "reset"},
	{EventType::prepare, "prepare"},
	{EventType::segment, "segment"},
	{EventType::segment_clear, "segment_clear"},
	{EventType::map_change, "map_change"},
	{EventType::history_clear, "history_clear"}
});

enum class EventSource {
	manual,
	combat,
	movement,
	other
};

NLOHMANN_JSON_SERIALIZE_ENUM(EventSource, {
	{EventSource::manual, "manual"},
	{EventSource::combat, "combat"},
	{EventSource::movement, "movement"},
	{EventSource::other, "other"}
});

enum class TimerStatus {
	stopped,
	prepared,
	running
};

struct TimerState {
	TimerStatus status;
	std::chrono::system_clock::time_point start_time;
	std::chrono::system_clock::time_point current_time;
	bool was_prepared;
};

struct TimeSegment {
	bool is_set = false;
	std::string name = "";
	std::chrono::system_clock::time_point start = std::chrono::system_clock::now();
	std::chrono::system_clock::time_point end = std::chrono::system_clock::now();
	std::chrono::system_clock::duration shortest_duration = std::chrono::system_clock::duration::zero();
	std::chrono::system_clock::duration shortest_time = std::chrono::system_clock::duration::zero();
};

struct HistoryEntry {
	HistoryEntry(std::chrono::system_clock::time_point start, std::chrono::system_clock::time_point end, std::string name);

	std::string name = "";
	std::chrono::system_clock::time_point start = std::chrono::system_clock::now();
	std::chrono::system_clock::time_point end = std::chrono::system_clock::now();
};

struct EventEntry {
	EventEntry(std::chrono::system_clock::time_point time, EventType type, EventSource source);
	EventEntry(std::chrono::system_clock::time_point time, EventType type, EventSource source, boost::uuids::uuid uuid);
	EventEntry(std::chrono::system_clock::time_point time, EventType type, EventSource source, boost::uuids::uuid uuid, std::string name);
	EventEntry(std::chrono::system_clock::time_point time, EventType type, EventSource source, std::string name);

	std::chrono::system_clock::time_point time;
	EventType type;
	EventSource source;
	boost::uuids::uuid uuid;
	std::optional<std::string> name;

	bool is_relevant = true;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(EventEntry, time, type, source, uuid)

class EventStore {
public:
	EventStore(API& api, const Settings& settings);
	void dispatch_event(EventEntry entry);
	TimerState get_timer_state();
	std::vector<HistoryEntry> get_history();
	std::vector<TimeSegment> get_segments();
	void save_map_log();
	void mod_release();

	double clock_offset = 0;
private:
	API& api;
	const Settings& settings;

	std::shared_mutex log_mutex;
	std::vector<EventEntry> entries;
	std::vector<HistoryEntry> history;
	std::vector<TimeSegment> segments;

	TimerState state;

	void reevaluate_state();
	void sync(const nlohmann::json& data);
	void add_event(EventEntry entry);
	std::string format_time(std::chrono::system_clock::time_point time);
	void save_log_thread(std::vector<EventEntry> entries);

	std::string logs_directory = "addons/arcdps/arcdps-timer-logs/";
};
